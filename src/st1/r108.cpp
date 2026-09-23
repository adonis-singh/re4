#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_wrap.h"
#include "emhit.h"
#include "read.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "math_sub.h"

// Room 1-08 (D:/Bio4/Prog/r108.cpp, in st1_1 and st1_3): the church; the symbol puzzle on the
// balcony, the bell, the doors and the battle streams.

struct R108Work {
    u8 bgmLow;     // 0x00  BGM lowered while the player stands in area 5
    u8 pad_1[3];
    u32 strId;     // 0x04  SndStrReq handle of the show-view stream
};

// One puzzle symbol: event flag, area number, wanted state, effect kind (EspPullCoreKind at init)
struct R108Symbol {
    u8 flagNo;
    u8 no;
    u8 on;
    u8 eff;
};

static R108Work* r108_work;
static u8 r108_symIdx;
static u8 r108_mesNo;
static cObj* r108_dial;
static cObj* r108_coverL;
static cObj* r108_coverR;

static R108Symbol r108_symbol[8] = {
    {0, 9, 1, 0}, {1, 0xA, 0, 0}, {2, 0xB, 1, 0}, {3, 0xC, 1, 0}, {4, 0xD, 0, 0}, {5, 0xE, 0, 0}, {6, 0xF, 0, 0}, {0, 0, 0, 0},
};

// Scroll objects fetched into the room's pointers (references: the address is evaluated before the
// call and the flag stores go through scalar references, which reload the other pointer after them)
static inline void r108_setObj(cObj*& o, u32 id)
{
    o = SmdGetObjPtr(id);
    o->be_flag |= 0x20;
}
// Fetch two scroll objects and mark both script-moved (be_flag 0x20), all pointer loads before the stores.
static inline void r108_setObj2(cObj*& a, cObj*& b, u32 idA, u32 idB)
{
    a = SmdGetObjPtr(idA);
    b = SmdGetObjPtr(idB);
    a->be_flag |= 0x20;
    b->be_flag |= 0x20;
}

static void r108_execShowView_end();
static void r108_execShowView();
static void r108_operator();
extern "C" void r108_checkEmReset();
static void r108_initChurchBell();
static void r108_checkDoor();
static void r108_checkBgm();
static void r108_getItem();
extern "C" void r108_initPuzzle(int dial, int coverL, int coverR, int mesNo);
extern "C" void r108_switchSymbol(int n);
extern "C" void r108_openCover();
static void r108_execPuzzle();
static void r108_str_check();

// Room init: clears System_flg 0x800, battle-stream and BGM tasks, the symbol
// puzzle on dials 0x31/0x32/0x33 with message 2, area 4 = front door check, the bell hit target, enemy
// 0x17 pre-read; area 6 = the dial terminal until Room_flg bit 0, area 0x12 = the show view once
// (Room_flg bit 1); action colour on area 4.
void R108Init()
{
    SysFlagOff(pG, SYS_SCISSOR_ON);
#line 44 "D:/Bio4/Prog/r108.cpp"
    r108_work = (R108Work*) MEM_CALLOC(sizeof(R108Work), 1, 0xd);

    SceExec(0x12, (TaskFunc) r108_str_check, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r108_checkBgm, 0, 0, SCE_PRIO_DEF_2, 0);
    r108_initPuzzle(0x31, 0x32, 0x33, 2);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r108_checkDoor, 0, 1);
    SceExec(0x12, (TaskFunc) r108_initChurchBell, 0, 0, SCE_PRIO_DEF_2, 0);
    EmReadSearch(0x17, 0, 0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r108_operator, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(0x12, SCE_LEVEL10, 0, (TaskFunc) r108_execShowView, 0, 1);
    }
    SceAtSetActColor(4, 1);
}

// Per-frame room main: nothing.
void R108Main()
{
}

// End of the show view: camera back, stream faded out over 200 frames, SceEventEnd.
static void r108_execShowView_end()
{
    CamCtrl.Comeback(0);
    SndStrReq(r108_work->strId, 4, 200, 0);
    SceEventEnd(0);
}

// Show the altar: camera cut 11 with its stream.

// One-shot event (Room_flg bit 1): stream 0x33 and camera cut 0xB (the show view) until the camera
// motion ends; player-cancellable.
static void r108_execShowView()
{
    // The 0.0 is loaded after the RsfSet store: a pool constant would move above it (pool loads never
    // depend on stores), a `static const` read through a reference stays below (docs/matching.md, cSceObj).
    static const f32 vol = 0.0f;

    RsfSet(G_ROOM_ID, 1);
    r108_work->strId = SndStrReq(0, 0x33, 0x80000003, 0, 0, *(const f32*) &vol);
    SceSetEventCancel(1, (TaskFunc) r108_execShowView_end, 0, -1, 1);
    SceEventStart(1);
    CamCtrl.CutCall(0xB);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r108_execShowView_end();
}

// The dial operator area: opens the sub screen puzzle terminal once.
static void r108_operator()
{
    if (!ScfFlagChk(pG, SCF_R108_OPERATOR)) {
        RsfSet(G_ROOM_ID, 0);
        OpeSetOpenTerm(6, 0.0f, 0.0f, 0.0f, 0.0f);
    }
}

// Ringing the bell: re-create up to three of the outside Ganados when few are left.
// Called from the module's other rooms: when 7 or fewer Ganados (ids 0x10..0x20) are alive, respawn up to
// three of the eleven listed ESL entries as reinforcements.
extern "C" void r108_checkEmReset()
{
    int list[11] = {1, 2, 0x2D, 0x47, 0x4B, 0x4D, 0x33, 0x35, 0x3B, 0x67, 0x6A};
    int* tbl = list;
    u32 n;

    n = SceCountEmAlive(0x10, 0x20);
    if (n <= 7) {
        u32 cnt = 0;
        int* p;

        // COMPILER-DIFF: 3 -- the original forms the end pointer from the array pseudo (`addi r29,r31,40`),
        // ours folds `&list[10]` to the frame; the launder hides the frame address from cse.
        asm("" : "+r"(tbl));

        for (p = tbl; p <= &tbl[10]; p++) {
            if (setEm(*p, -1, 0, 1, 1) != 0) {
                cnt++;
                if (cnt > 2) {
                    break;
                }
            }
        }
    }
}

// The church bell: a hit enemy on the scroll object; shots ring it.
static void r108_initChurchBell()
{
    cObj* bell;
    cEmHit* hit;

    bell = SmdGetObjPtr(0x1C);
    hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &bell->pos, &bell->ang, 1);
    {
        // `const`: the single-use constants are loaded in declaration order (w, x, h, z), not in
        // argument order (the r103 checkCloseCover lever)
        const f32 w = 1000.0f;
        const f32 h = -3000.0f;
        const f32 x = 0.0f;
        const f32 z = 500.0f;
        YarareInitCube(hit, x, x, z, w, h, w, 0, YAT_FLAG_ON);
    }
    for (;;) {
        if (hit->ckStatus() == 1) {
            switch (hit->dmg.m_Wep) {
            case 0xD:
            case 0xF:
            case 0x12:
            case 0x13:
                SndCall(6, 0xD, &bell->pos, 0, 0, 0);
                break;
            default:
                SndCall(6, 0xC, &bell->pos, 0, 0, 0);
                break;
            }
            r108_checkEmReset();
        }
        SceSleep(1);
    }
}

// The front door: locked message, then the sub screen terminal once the church flag is set.
static void r108_checkDoor()
{
    SndCall(6, 7, 0, 0, 0, 0);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (!ScfFlagChk(pG, SCF_R108_OPERATOR)) {
        ScfFlagOn(pG, SCF_R108_OPERATOR);
        OpeSetOpenTerm(7, 22600.0f, 11775.0f, -26200.0f, 1.6f);
    }
}

// Room BGM: tracks 0/1 swap while the player stands in area 5.
static void r108_checkBgm()
{
    SceSleep(1);
    if (pG->room_id_prev == 0x109) {
        r108_work->bgmLow = 1;
        SndRoomBgmStart(0, 1);
        SndRoomBgmStart(1, 0);
    } else {
        r108_work->bgmLow = 0;
        SndRoomBgmStart(0, 0);
        SndRoomBgmStart(1, 1);
    }
    SceSleep(1);
    for (;;) {
        int hit = SceAtHitCheck(5);

        if (hit == 1) {
            if (r108_work->bgmLow == 0) {
                r108_work->bgmLow = hit;
                SndRoomBgmVolReset(1, 2000);
                SndRoomBgmVolSet(0, 1, 2000);
            }
        } else {
            if (r108_work->bgmLow == 1) {
                r108_work->bgmLow = 0;
                SndRoomBgmVolReset(0, 2000);
                SndRoomBgmVolSet(1, 1, 2000);
            }
        }
        SceSleep(1);
    }
}

// The item behind the covers was taken: run item area 0x82 and disable area 0xA.
static void r108_getItem()
{
    SceAtExecute(0x82);
    SceAtSetEnable(0xA, 0);
}

// Puzzle setup: the dial and the two cover halves, the item behind them.
extern "C" void r108_initPuzzle(int dial, int coverL, int coverR, int mesNo)
{
    u32 i;
    cModel* m;

    for (i = 0; i <= 6; i++) {
        r108_symbol[i].eff = EspPullCoreKind();
    }
    r108_setObj(r108_dial, dial);
    r108_setObj2(r108_coverL, r108_coverR, coverL, coverR);
    r108_mesNo = mesNo;
    SceAtSetEnable(0x82, 1);
    if ((m = SceAtItemModelPtr(0x82)) != 0) {
        m->setNoSuspend(1);
    }
    if (!ScfFlagChk(pG, SCF_R108_PUZZLE_CLEAR)) {
        SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r108_execPuzzle, 0, 1);
    } else {
        r108_coverL->pos.x += 220.0f;
        r108_coverR->pos.x -= 220.0f;
        if (!ItfFlagChk(pG, ITF_R108_ITEM)) {
            SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r108_getItem, 0, 1);
            SceAtPtr(0xA)->actBtnKind = 0x28;
        }
    }
}

// Turn the dial `n` symbols on (2 pi / 7 each) and toggle the symbol's flag and effect.
extern "C" void r108_switchSymbol(int n)
{
    f32 ang = (f32) (int) r108_symIdx * 0.8975979f;
    int i;

    for (i = 0; i < n; i++) {
        f32 next;
        f32 rot;

        SndCall(6, 3, 0, 0, 0, 0);
        r108_symIdx++;
        next = (f32) (int) r108_symIdx * 0.8975979f;
        // Dead test (store dead in flow, compare/loads in flow2), placed before the loop's first
        // conditional jump so loop.c still hoists both operands: it adds a loop-weighted ref to the
        // step constant (f29 before the two `fmr` copies) and to the r108_dial high (r31 before `n`).
        if (ang + 0.10471976f == *(f32*) &r108_dial) {
            rot = ang;
        }
    turn:
        rot = -LIMIT_ANGLE(ang);
        r108_dial->pList->ang.y = rot;
        ang += 0.10471976f;
        if (ang >= next) {
            goto done;
        }
        SceSleep(1);
        goto turn;
    done:
        r108_dial->pList->ang.y = -LIMIT_ANGLE(next);
        SceSleep(2);
    }
    FlagXorVar(&pG->Room_flg, (int) r108_symbol[r108_symIdx %= 7].flagNo);
    if (FlagChkVar(&pG->Room_flg, (int) r108_symbol[r108_symIdx].flagNo)) {
        EstSet(0, -1, 0, 0, EFF_ROOM, r108_symbol[r108_symIdx].no, 1, r108_symbol[r108_symIdx].eff, 0, 0);
    } else {
        EffectEspDelete(0, r108_symbol[r108_symIdx].eff, 0, 0);
        EffectEspgenDelete(0, r108_symbol[r108_symIdx].eff, 0);
        EffectEfmDelete(0, r108_symbol[r108_symIdx].eff, 0);
    }
    SceSleep(15);
}

// Slide the two cover halves apart.
extern "C" void r108_openCover()
{
    const f32 step = 10.0f;
    f32 x0 = r108_coverL->pos.x;
    f32 x1 = r108_coverR->pos.x;
    f32 t = 0.0f;

    SceSleep(15);
    SndCall(6, 4, 0, 0, 0, 0);
    do {
        t += step;
        r108_coverL->pos.x = x0 + t;
        r108_coverR->pos.x = x1 - t;
        if (t >= 220.0f) {
            break;
        }
        SceSleep(1);
    } while (1);
    // COMPILER-DIFF: candidate #12 (loop-exit form). The dead loop's notes end cse1's AROUND path over the
    // poll loop's exit, so the block below re-materialises the cover highs and 220.0 like the original.
    do { } while (0);
    r108_coverL->pos.x = x0 + 220.0f;
    r108_coverR->pos.x = x1 - 220.0f;
    SceSleep(15);
}

// The puzzle: message selections turn the dial until the symbols match the wanted pattern.
static void r108_execPuzzle()
{
    int quit;
    int i;
    int ok;
    u32 j;

    SceEventStart(0);
    LightMgr.endEvent();
    r108_symIdx = 0;
    for (i = 0; i < 7; i++) {
        FlagOffVar(&pG->Room_flg, (int) r108_symbol[i].flagNo);
    }
    CamCtrl.CutCall(5);
    quit = 0;
    SceSleep(1);
    SceMesSet(r108_mesNo, 0x30, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    SceMesWait();
    do {
        SceMesSet(r108_mesNo + 1, 0x230, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        SceMesWait();
        switch (SceMesGetSelection()) {
        case 1:
            r108_switchSymbol(3);
            break;
        case 2:
            r108_switchSymbol(4);
            break;
        case -1:
        case 3:
            quit = 1;
            break;
        }
        ok = 1;
        for (i = 0; i < 7; i++) {
            if ((FlagChkVar(&pG->Room_flg, (int) r108_symbol[i].flagNo) == 0) != (r108_symbol[i].on == 0)) {
                ok = 0;
                break;
            }
        }
        if (ok == 1) {
            r108_openCover();
            ScfFlagOn(pG, SCF_R108_PUZZLE_CLEAR);
            SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r108_getItem, 0, 1);
            SceAtPtr(0xA)->actBtnKind = 0x28;
            break;
        }
        SceSleep(1);
    } while (quit == 0);
    r108_dial->pList->ang.y = 0.0f;
    r108_symIdx = 0;
    for (j = 0; j <= 6; j++) {
        EffectEspDelete(0, r108_symbol[j].eff, 0, 0);
        EffectEspgenDelete(0, r108_symbol[j].eff, 0);
        EffectEfmDelete(0, r108_symbol[j].eff, 0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Battle stream while a Ganado of list entries 0x10..0x20 is alive, sees the player and is near.
static void r108_str_check()
{
    f32 near = 900000000.0f;
    int on = 0;

    for (;;) {
        u32 i;
        int found = 0;

        for (i = 0; i < EmMgr.getArrayNum(); i++) {
            cEm* em = EmMgr.fastAt(i);

            if (em->id >= 0x10 && em->id <= 0x20 && em->checkStatus(EM_STATUS_ACTIVE) != 0 && em->hp > 0 && em->isAlive()
                && ((cEmGanado*) em)->ckFindPL() == 1 && em->l_pl < near) {
                found = 1;
            }
        }
        if (found == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else if (on == 1) {
            SndRoomStrStop(3);
            on = 0;
        }
        SceSleep(1);
        // Dead test (both stores die in flow, the compare/branch in flow2): its insns raise the outer
        // loop's real-insn count above loop.c's hoist threshold in the SECOND loop pass, so the inner
        // loop's `lis EmMgr@ha` (a fresh pseudo made by pass 1 when it hoisted `&EmMgr`) stays in the
        // outer body and cse2 merges it with the pArray load's high (the original's two EmMgr chains).
        if (EmMgr.size == 0) {
            found = 1;
        }
        found = 2;
    }
}
