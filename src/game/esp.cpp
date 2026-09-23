// game/esp.cpp: the effect sprite (cEsp) pool. A cEsp is one live effect particle/sprite; the pool
// (g_pEspSys->pEspBuf, nEsp slots of 0x150 bytes) is allocated per room from the cons value and
// handed out by cEsp::operator new / PullEsp, returned by PushEsp. EspMove (game loop, after
// EspgenMove) runs every active effect's move(); EspTrans (trans.cpp) sorts them into the ordering
// tables by Tool_flg / Parts_no / Core_flg. Per-id create and trans functions live in
// EspCreateTbl / EspTransTbl, filled by the esp??.cpp units through EspFuncTblSet.

#include "atari.h"
#include "light.h"
#include "global.h"
#include "main_mem.h"
#include "eprintf.h"
#include "math_sub.h"
#include "esp.h"
#include "espgen.h"

// number of live effects per owner id (debug display)
u16 esp_num_list[EFF_MAX];
EspTransFunc EspTransTbl[0xFF];
EspCreateFunc EspCreateTbl[0xFF];

extern "C" {
void EspDummyTrans(cEsp* esp);
void EspFuncTblInit();
int ESP_IsActive(cEsp* esp);
int EspMove();
f32 EspGetCameraPan();
f32 EspGetCameraPan2();
int EspTrans();
int EspDispInfo();
int EspArrayAlloc(u32 n);
int EspArrayFree();
int EspArrayPush(u32 n);
int EspArrayPop();
}

// Default EspTransTbl entry: an effect whose id has no registered trans function is reported and
// released.
void EspDummyTrans(cEsp* pEsp)
{
    pLog->err(0, 0, "ESP_TRANS : ESP_ID[%x] invalid.", pEsp->m_Id);
    PushEsp(pEsp);
}

// Resets both per-id tables (0xFF ids) to "unregistered"; called once from the effect system init
// (eff_sys.cpp) before the esp units register themselves.
void EspFuncTblInit()
{
    int i;

    for (i = 0; i < 0xFF; i++) {
        EspTransTbl[i] = EspDummyTrans;
        EspCreateTbl[i] = NULL;
    }
}

// Registers the factory and draw function for effect id `id` (each esp??.cpp calls this at init).
void EspFuncTblSet(int id, EspCreateFunc create, EspTransFunc trans)
{
    EspCreateTbl[id] = create;
    EspTransTbl[id] = trans;
}

// 1 when `esp` is in use and should move/draw this frame. Under Status_flg[1] 0x10000000
// (event pause) only effects with Core_flg bit0 count, and an effect attached to a model only
// when that model carries be_flag 0x800 (moves during events, see emMove).
int ESP_IsActive(cEsp* pEsp)
{
    if (!(pEsp->m_Be_flg & 1)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_SUSPEND)) {
        if (!(pEsp->info.Core_flg & 1)) {
            return 0;
        }
        if (pEsp->parent != pEffParentWorld) {
            cModel* m = pEsp->m_pMod;
            if (m != NULL) {
                int off = !(m->be_flag & 0x800);
                if (off) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

// Dead-stripped by the original link (nothing calls it).
int cEsp::IsActive()
{
    return m_Be_flg & 1;
}

// Allocates an effect of id `id`: runs its registered create (operator new picks a free slot),
// marks it live (m_Be_flg bit0), stores the id and bumps ActiveEspNum. Returns 1 on success;
// on a bad id or a full pool *out is the dummy esp (pDmyEsp) and 0 is returned.
int PullEsp(cEsp** ppEsp, int id)
{
    cEspSystem* sys = g_pEspSys;
    EspCreateFunc create = EspCreateTbl[id];
    cEsp* esp;
    int ret = 0;

    if (create == NULL) {
        pLog->err(0, 0, "ESP : EspID[%x] is invalid.", id);
        *ppEsp = sys->pDmyEsp;
        return 0;
    }
    esp = create();
    *ppEsp = esp;
    if (esp != sys->pDmyEsp) {
        esp->m_Be_flg |= 1;
        ret = 1;
        sys->ActiveEspNum++;
        (*ppEsp)->m_Id = id;
    } else {
        pLog->warn(6, 0, "ESP : ESP work full!!");
    }
    return ret;
}

// Pool slot search: scans the pool from the last hit (wrapping) for a free slot; if none, steals the
// first live slot flagged Tool_flg 0x40000 (low-priority, may be recycled) after releasing it.
// The found slot is zeroed. Returns pDmyEsp when nothing is available.
// The target's third loop has `mr r9,r10` (a copy of the sys+0x10000 base) before the loop and in
// its latch, with the pEspBuf load reading r9. That is cse_around_loop (cse.c): it only runs on a
// loop with LOOP_BEG/END notes whose latch jumps straight back to the header, and it rewrites the
// header's `sys+0x10000` into the latch's REG_LOOP_TEST_P copy. So loop3 is a real `for` loop;
// loop.c (find_and_verify_loops) would then move the `PushEsp; goto found` block behind the found:
// block (a guarded block ending in a jump out of the loop) and, with no call left in the loop,
// hoist the pEspBuf load. The `do { PushEsp(esp); goto found; } while (0)` wrapper stops that: the
// backward scan from the `goto` stops at the inner NOTE_INSN_LOOP_BEG instead of the guard jump.
// Loops 1/2/4 as for/while loops get strength-reduced `&esp->flag` givs the target lacks, so they
// stay goto loops.
void* cEsp::operator new(unsigned int size)
{
    static u32 old_hit = 0;
    cEspSystem* sys = g_pEspSys;
    u32 i;
    cEsp* esp;
    cEsp* ret;
    u32 start;
    u32 ofs;

    if (old_hit >= sys->nEsp) {
        old_hit = 0;
    }
    i = old_hit;
    start = i;
    ret = sys->pDmyEsp;
    if (i < sys->nEsp) {
        ofs = i * 0x150;
loop1:
        esp = (cEsp*) (sys->EspArray + ofs);
        if (!(esp->m_Be_flg & 1)) {
            goto found;
        }
        i++;
        ofs += 0x150;
        if (i < sys->nEsp) {
            goto loop1;
        }
    }
    i = 0;
    if (i < start) {
        ofs = 0;
loop2:
        esp = (cEsp*) (sys->EspArray + ofs);
        if (!(esp->m_Be_flg & 1)) {
            goto found;
        }
        i++;
        ofs += 0x150;
        if (i < start) {
            goto loop2;
        }
    }
    if (ret == sys->pDmyEsp) {
        for (i = start; i < sys->nEsp; i++) {
            esp = (cEsp*) (sys->EspArray + i * 0x150);
            if ((esp->m_Be_flg & 1) && (esp->m_Tool_flg & 0x40000)) {
                do { // keeps loop.c from moving this block out of the loop (see above)
                    PushEsp(esp);
                    goto found;
                } while (0);
            }
        }
        i = 0;
        if (i < start) {
            ofs = 0;
loop4:
            esp = (cEsp*) (sys->EspArray + ofs);
            if ((esp->m_Be_flg & 1) && (esp->m_Tool_flg & 0x40000)) {
found:
                memclr_asm(esp, 0x150);
                old_hit = i + 1;
                return esp;
            }
            i++;
            ofs += 0x150;
            if (i < start) {
                goto loop4;
            }
        }
    }
    return ret;
}

u32 tubo_amb = 0;

// Releases a live effect: clears m_Be_flg bits 0-1, decrements ActiveEspNum and runs Destruct().
// Pushing a slot that is not live only warns.
void PushEsp(cEsp* pEsp)
{
    if (pEsp->m_Be_flg & 1) {
        pEsp->m_Be_flg &= ~3;
        g_pEspSys->ActiveEspNum--;
        pEsp->Destruct();
    } else {
        pLog->warn(0, 0, "PushEsp() : No alive work is pushed.");
    }
}

// Per-frame update of all effects (game loop, after EspgenMove). Drops effects whose attached model
// died or was re-used (be_flag / serial mismatch); during the pause (Status_flg[1] bit1) only
// Core_flg 0x8000 effects move. Prints the live count (debug page 0xE also lists per-owner counts).
int EspMove()
{
    cEspSystem* sys = g_pEspSys;
    cEsp* esp;
    u32 cnt;
    u32 i;
    int pause;
    int color;
    int y;

    for (i = 0; i < EFF_MAX; i++) {
        esp_num_list[i] = 0;
    }
    pause = 0;
    if (StaFlagChk(pG, STA_ITEM_GET)) {
        pause = 1;
    }
    cnt = 0;
    for (i = 0; i < sys->nEsp; i++) {
        esp = (cEsp*) (sys->EspArray + i * 0x150);
        if (!ESP_IsActive(esp)) {
            continue;
        }
        if (esp->parent != pEffParentWorld) {
            cModel* m = esp->m_pMod;
            if (m != NULL) {
                if (!m->isAlive() || m->guid != esp->m_Guid_pMod) {
                    PushEsp(esp);
                    continue;
                }
            }
        }
        if (pause) {
            if (!(esp->info.Core_flg & 0x8000)) {
                continue;
            }
        }
        esp->move();
        if (esp->m_Be_flg & 1) {
            cnt++;
            if (pG->debug_mode == 0xE) {
                esp_num_list[esp->info.owner]++;
            }
        }
    }
    color = 0;
    if ((f32) cnt > (f32) sys->nEsp * 0.7f) {
        color = 0x16;
    }
    if ((f32) cnt > (f32) sys->nEsp * 0.9f) {
        color = 2;
    }
    if (DbgFlagChk(pG, DBG_EFF_NUM_DISP)) {
        eprintf(0x1B0, 0xC8, color, 0, "%d/%d", sys->ActiveEspNum, cnt);
    } else {
        eprintf(0x1D8, 0xC8, color, 0xE, "%d", cnt);
    }
    if (!DbgFlagChk(pG, DBG_TEST_MODE) && pG->debug_mode == 0xE) {
        eprintf(0x20, 0x60, 0, 0xE, "TOTAL:%d", cnt);
        // y counts printed rows; `0x70 + y * 0x10` is a strength-reduced giv whose `li 0x70` init
        // is emitted by loop.c after the hoisted `lis`/`addi`s (a plain `y = 0x70; y += 0x10`
        // schedules the li before the call).
        y = 0;
        for (i = 0; i < EFF_MAX; i++) {
            if (esp_num_list[i] != 0) {
                eprintf(0x20, 0x70 + y * 0x10, 0, 0xE, "%s:%d", owner_name_tbl[i], esp_num_list[i]);
                y++;
            }
        }
    }
    return 1;
}

// Camera yaw in degrees as computed by the last EspTrans (used by the sprite transforms).
f32 EspGetCameraPan()
{
    return g_pEspSys->CameraPan;
}

// Camera pitch in degrees as computed by the last EspTrans.
f32 EspGetCameraPan2()
{
    return g_pEspSys->CameraPan2;
}

// Draw registration (trans.cpp, once per frame): computes the camera pan/pitch, then queues every
// active effect's trans function into the ordering tables: screen sprites (Parts_no 0xF8..0xFD) go
// to fixed OT slots, Tool_flg 0x10000 selects a texture-render target by Core_flg, 0x1000/0x400/
// 0x800/0x400000 pick the pre/post-world layers, everything else is Z-sorted by world position
// (with m_Radius when set). Tool_flg 0x100 hides an effect in the first-person (scope /
// binocular) view (Status_flg[0] 0x8000), 0x200 hides it outside that view.
int EspTrans()
{
    cEspSystem* sys = g_pEspSys;
    Vec dir;
    Vec wpos;
    Vec* wp;
    cEsp* esp;
    EspTransFunc trans;
    Camera* cam;
    u32 i;
    u16 prio;
    int ot;
    f32 zlimit;

    if (sys == NULL) {
        return 0;
    }
    LightMgr.setEsp(&sys->EspLightEnv, 8);
    cam = &pG->Camera;
    dir.x = cam->param.at.x - cam->param.pos.x;
    dir.y = cam->param.at.y - cam->param.pos.y;
    dir.z = cam->param.at.z - cam->param.pos.z;
    if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
        dir.y = 1.0f;
    }
#line 584 "D:/Bio4/Prog/esp.cpp"
    VECNormalize(&dir, &dir);
    if (dir.x == 0.0f && dir.z == 0.0f) {
        sys->CameraPan = 0.0f;
    } else {
        sys->CameraPan = atan2f(dir.x, dir.z) * 57.295776f;
    }
    if (dir.y == 0.0f) {
        sys->CameraPan2 = 0.0f;
    } else {
        sys->CameraPan2 = -atan2f(dir.y, SQRTF(dir.x * dir.x + dir.z * dir.z)) * 57.295776f;
    }
    for (i = 0; i < sys->nEsp; i++) {
        esp = (cEsp*) (sys->EspArray + i * 0x150);
        if (!ESP_IsActive(esp)) {
            continue;
        }
        if (esp->m_pMod != NULL && !(esp->m_pMod->be_flag & 2) && esp->m_Release_time == 0xFF) {
            continue;
        }
        trans = EspTransTbl[esp->m_Id];
        if (trans == NULL) {
            continue;
        }
        if (esp->info.Core_flg & 0x400) {
            if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
                continue;
            }
        }
        if (StaFlagChk(pG, STA_LOOK_THROUGH)) {
            if (esp->m_Tool_flg & 0x100) {
                continue;
            }
        } else {
            if (esp->m_Tool_flg & 0x200) {
                continue;
            }
        }
        prio = 0x10;
        if (trans == EspCommonTrans && esp->m_Shimmer_type == 0 && !(esp->m_Tool_flg & 0x6000)) {
            prio = 8;
        }
        if (StaFlagChk(pG, STA_ITEM_GET)) {
            if (!(esp->info.Core_flg & 0x8000)) {
                continue;
            }
            AddOtDirect(0x14, esp, (void (*)()) trans, 0, prio, NULL, 0.0f);
            continue;
        }
        if (esp->m_Tool_flg & 0x10000) {
            StaFlagOn(pG, STA_TEX_RENDER);
            ot = 0;
            if (!(esp->info.Core_flg & 8)) {
                if (esp->info.Core_flg & 0x10) {
                    ot = 1;
                } else if (esp->info.Core_flg & 0x20) {
                    ot = 2;
                } else if (esp->info.Core_flg & 0x40) {
                    ot = 3;
                } else if (esp->info.Core_flg & 0x80) {
                    ot = 4;
                } else if (esp->info.Core_flg & 0x100) {
                    ot = 5;
                } else if (esp->info.Core_flg & 0x200) {
                    ot = 6;
                } else if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
                    pLog->err(6, 0, "ESP : FLG_TEX_RENDER but no set tex_no");
                }
            }
            if ((u8) (esp->m_Parts_no + 8) <= 5) {
                switch (esp->m_Parts_no) {
                case ESP_PARTS_SCREEN:
                    AddOtDirect(ot, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
                    break;
                case ESP_PARTS_SCREEN_AFTER1:
                    AddOtDirect(ot, esp, (void (*)()) trans, 2, prio, NULL, 0.0f);
                    break;
                case ESP_PARTS_SCREEN_AFTER2:
                    AddOtDirect(ot, esp, (void (*)()) trans, 1, prio, NULL, 0.0f);
                    break;
                case ESP_PARTS_SCREEN_PRE1:
                    AddOtDirect(ot, esp, (void (*)()) trans, 5, prio, NULL, 0.0f);
                    break;
                case ESP_PARTS_SCREEN_PRE2:
                    AddOtDirect(ot, esp, (void (*)()) trans, 4, prio, NULL, 0.0f);
                    break;
                case ESP_PARTS_SCREEN_FIRST:
                    AddOtDirect(ot, esp, (void (*)()) trans, 6, prio, NULL, 0.0f);
                    break;
                default:
                    pLog->err(0, 0, "ESP : FLG_TEX_RENDER but no screen");
                    break;
                }
            } else {
                pLog->err(0, 0, "ESP : FLG_TEX_RENDER but no screen");
            }
            continue;
        }
        if (esp->m_Tool_flg & 0x1000) {
            if ((u8) (esp->m_Parts_no + 8) <= 5) {
                AddOtDirect(0x12, esp, (void (*)()) trans, 8, prio, NULL, 0.0f);
            } else {
                AddOtDirect(0x12, esp, (void (*)()) trans, 9, prio, NULL, 0.0f);
            }
            continue;
        }
        if (esp->m_Flg & 8) {
            AddOtDirect(0x12, esp, (void (*)()) trans, 6, prio, NULL, 0.0f);
            continue;
        }
        if (esp->m_Tool_flg & 0x400000) {
            if ((esp->m_Tool_flg & 0xC00) == 0xC00) {
                AddOtDirect(0xB, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
            } else {
                AddOtDirect(0x10, esp, (void (*)()) trans, 4, prio, NULL, 0.0f);
            }
            continue;
        }
        if (esp->m_Tool_flg & 0x400) {
            if (esp->m_Tool_flg & 0x800) {
                AddOtDirect(0x10, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
            } else {
                AddOtDirect(0x10, esp, (void (*)()) trans, 2, prio, NULL, 0.0f);
            }
            continue;
        }
        if (esp->m_Tool_flg & 0x800) {
            AddOtDirect(0x10, esp, (void (*)()) trans, 0, prio, NULL, 0.0f);
            continue;
        }
        if ((u8) (esp->m_Parts_no + 8) <= 5) {
            switch (esp->m_Parts_no) {
            case 0xFD:
                AddOtDirect(0x12, esp, (void (*)()) trans, 3, prio, NULL, 0.0f);
                break;
            case 0xFC:
                AddOtDirect(0x12, esp, (void (*)()) trans, 2, prio, NULL, 0.0f);
                break;
            case 0xFB:
                AddOtDirect(0x12, esp, (void (*)()) trans, 1, prio, NULL, 0.0f);
                break;
            case 0xFA:
                AddOtDirect(0x12, esp, (void (*)()) trans, 5, prio, NULL, 0.0f);
                break;
            case 0xF9:
                AddOtDirect(0x12, esp, (void (*)()) trans, 4, prio, NULL, 0.0f);
                break;
            case 0xF8:
                AddOtDirect(0x12, esp, (void (*)()) trans, 6, prio, NULL, 0.0f);
                break;
            }
            continue;
        }
        if (esp->parent != pEffParentWorld) {
            PSMTXMultVec(esp->parent->mat, &esp->m_Pos, &wpos);
        } else {
            wp = &wpos;
            *wp = esp->m_Pos;
        }
        wp = &wpos;
        zlimit = 0.0f;
        if (esp->m_Radius == zlimit) {
            AddOtWorldPos(esp, (void (*)(void*)) trans, wp, prio, 200.0f);
        } else {
            if ((esp->m_Flg & 2) == 0 && (esp->m_Tool_flg & 1) == 0) {
                zlimit = 200.0f;
            }
            AddOtWorldPosRadius(esp, (void (*)(void*)) trans, wp, esp->m_Radius, prio, zlimit);
        }
    }
    return 1;
}

// Debug page 12 "EP" row: current / peak / total esp slot usage.
int EspDispInfo()
{
    static u32 max = 0;
    cEspSystem* sys = g_pEspSys;
    u8* p = sys->EspArray;
    u32 cnt;
    u32 i;

    if (p == NULL) {
        return 0;
    }
    cnt = 0;
    for (i = 0; i < sys->nEsp; i++) {
        if (((cEsp*) (p + i * 0x150))->m_Be_flg & 1) {
            cnt++;
        }
    }
    eprintf(0x1A0, 0x38, 0, 0xC, "%3d/%3d/%4d", cnt, max, sys->nEsp);
    if (cnt > max) {
        max = cnt;
    }
    return 1;
}

u32 esp_dmy_amb = 0;

// Allocates the esp pool with `n` slots (game.cpp room start, count from the cons table); frees
// the previous pool first. Returns 1 on success.
int EspArrayAlloc(u32 workNum)
{
    cEspSystem* sys = g_pEspSys;
    u32 size;
    u8* p;

    EspArrayFree();
    if (workNum == 0) {
        return 0;
    }
    size = workNum * 0x150;
#line 879 "D:/Bio4/Prog/esp.cpp"
    p = (u8*) MEM_ALLOC(size, 1, 0xD);
    sys->EspArray = p;
    if (p == NULL) {
        return 0;
    }
    sys->nEsp = workNum;
    memclr_asm(p, size);
    return 1;
}

// Frees the esp pool. Returns 0 when there was none.
int EspArrayFree()
{
    cEspSystem* sys = g_pEspSys;

    if (sys->EspArray == NULL) {
        return 0;
    }
    Mem_free(sys->EspArray);
    sys->EspArray = NULL;
    return 1;
}

// Debug tools: swaps in a Debug_alloc'd pool of `n` slots, saving the game pool. 0 if one is
// already pushed.
int EspArrayPush(u32 esp_num)
{
    cEspSystem* sys = g_pEspSys;

    if (sys->pEspBufSave != NULL) {
        return 0;
    }
    sys->pEspBufSave = sys->EspArray;
    sys->EspArray = (u8*) Debug_alloc(esp_num * 0x150, 1);
    sys->nEspBack = sys->nEsp;
    sys->nEsp = esp_num;
    return 1;
}

// Debug tools: restores the pool saved by EspArrayPush. 0 if none was pushed.
int EspArrayPop()
{
    cEspSystem* sys = g_pEspSys;

    if (sys->pEspBufSave == NULL) {
        return 0;
    }
    Debug_free(sys->EspArray);
    sys->EspArray = sys->pEspBufSave;
    sys->pEspBufSave = NULL;
    sys->nEsp = sys->nEspBack;
    return 1;
}

// Releases every live effect in the pool (room change, est.cpp).
void EspArrayClear()
{
    cEspSystem* sys = g_pEspSys;
    cEsp* esp;
    u32 i;

    for (i = 0; i < sys->nEsp; i++) {
        esp = (cEsp*) (sys->EspArray + i * 0x150);
        if (esp->m_Be_flg & 1) {
            PushEsp(esp);
        }
    }
}

// The dummy esp that PullEsp hands out when it fails; callers compare against it.
cEsp* EspGetDmyPtr()
{
    return g_pEspSys->pDmyEsp;
}

// Queues `func(esp)` in the after-render OT (0x16) so an effect can read the depth buffer (the
// HideCheck occlusion tests of esp09/esp0e/esp45). Skipped while the generator loop pre-runs.
void EspAddOtAfterRender(cEsp* esp, void (*func)(cEsp*))
{
    if (EspGenGetMoveLoop() == 0) {
        AddOtDirect(0x16, esp, (void (*)()) func, 0, 0x20, NULL, 0.0f);
    }
}
