#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "light.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "cam_ctrl.h"
#include "st_mgr_event.h"
#include "act_btn.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "math_sub.h"
#include "rnd.h"
#include "shadow.h"
#include "room_data.h"
#include "eprintf.h"

// Room 3-17 (D:/Bio4/Prog/r317.cpp): the Krauser knife fight cut scenes (s00..s14 chained with
// action-button reactions), the two-gear elevator (SceElevator2) and the continue point.

struct R317Work {
    int btnCount;   // 0x00  action button presses counted by s13
    int hardMode;   // 0x04  1 above the easy difficulty: the button prompts come earlier
};

// Two-gear elevator (sce_com's SceElevatorData with a second stop, the gear objects and a second cut).
struct SceElevator2Data {
    s32 dir;        // 0x00  3 up / 1 down
    u32 objId;      // 0x04  cage
    u32 objId2;     // 0x08  gear (uv scroll forward)
    u32 objId3;     // 0x0C  gear (uv scroll backward)
    Vec pos;        // 0x10  cage start
    Vec pos2;       // 0x1C  cage stop
    Vec plPos;      // 0x28
    Vec plPos2;     // 0x34
    Vec plRot;      // 0x40
    s32 cut;        // 0x4C
    s32 cut2;       // 0x50
    u16 seStart;    // 0x54
    u16 seMove;     // 0x56
    u16 seStop;     // 0x58
    u16 pad_5A;
};

// The cage object's per-object work: the running sound handle at cObj+0x328.
struct R317ElevatorObjView {
    u8 pad_0[0x328];
    u32 hSnd;
};

// The work pointer is a struct member: every store through the work reloads it (r203).
struct R317WorkPtr {
    R317Work* p;
};

static R317WorkPtr r317_work;

// The original object's .rodata (0x4cc -> 0x4d0) and .data (0xdc -> 0xe0) are 8-aligned: r318's
// sections start 8-aligned in the REL.
asm(".section .rodata\n\t.balign 8\n\t.section .data\n\t.balign 8\n\t.text");
static SceElevator2Data r317_elvUp = {3, 0x2A, 0x19, 0x1A, {0.0f, 0.0f, 0.0f}, {0.0f, 10825.0f, 0.0f}, {6860.0f, -4775.0f, 11950.0f}, {6860.0f, 6016.0f, 11950.0f}, {0.0f, 3.1415927f, 0.0f}, 2, 3, 2, 4, 3, 0};
static SceElevator2Data r317_elvDown = {1, 0x2A, 0x19, 0x1A, {0.0f, 10825.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {6860.0f, 6016.0f, 11950.0f}, {6860.0f, -4775.0f, 11950.0f}, {0.0f, 1.5707964f, 0.0f}, 3, 2, 2, 4, 3, 0};

static f32 r317_elvSpd = 0.0f;
static f32 r317_elvMaxSpd = 100.0f;
static f32 r317_elvMinSpd = 10.0f;
static f32 r317_elvFadeSpdUp = 30.0f;
static f32 r317_elvAccelDown = 1.0f;
static f32 r317_elvStopAddDown = 0.0f;
static f32 r317_elvFadeSpdDown = 40.0f;
static f32 r317_elvAccelUp = 1.5f;
static f32 r317_elvStopAddUp = 2000.0f;

// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");
// The room build's cGameSave::save had a second (unused) parameter: `li r5, -1` before the call.
void GameSaveSave2(cGameSave* g, void* data, int mode) asm("save__9cGameSavePv");

// Position a model from three components (inline owning the Vec).
static inline void SetPosXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
}

// Every s00..s14 handler deletes the same effect on begin.
static inline void EffectDelete2001()
{
    EffectEspDelete(0x2001, 3, 0, 0);
    EffectEspgenDelete(0x2001, 3, 0);
    EffectEfmDelete(0x2001, 3, 0);
}

// The action-button prompt of the knife fight: X or A by the per-event coin flip in flags_174. The
// button number is a compiler temporary that ties to r9; the flags_58 clear reuses its stored value as the
// stack-argument zero (COMPILER-DIFF: 13 shape of R224Main: a two-set pseudo has no REG_EQUIV, so its `li`
// waits for the `stw` and shares r0 with the RMW instead of being born early in r9).
#define KnifeActBtn(bit, action)                                            \
    do {                                                                     \
        int btn = (pG->Room_flg[0] & (bit)) == 0 ? 4 : 3;                      \
        u32 v = pG->Disp_flg & ~0x800;                                       \
                                                                             \
        pG->Disp_flg = v;                                                    \
        v = 0;                                                               \
        ActBtn.set(0x30, 5, (int) action, 0, 0x42, btn, 0, v);               \
    } while (0)

static void R317ContinuePointSet();
void R317EventS00();
void R317Elevator2Init();
void SceElevator2Main(SceElevator2Data* d);
void SceElevator2End(SceElevator2Data* d);
static void R317EventS00Action();
static void R317EventS07Action();
static void R317EventS09Action();
static void R317EventS11Action();
static void R317EventS01Action();
static void R317EventS03Action();
void R317SmdAllOn();
extern "C" void Evt_R317S00_Func(Event* e);
extern "C" void Evt_R317S01_Func(Event* e);
extern "C" void Evt_R317S02_Func(Event* e);
extern "C" void Evt_R317S03_Func(Event* e);
extern "C" void Evt_R317S04_Func(Event* e);
extern "C" void Evt_R317S05_Func(Event* e);
extern "C" void Evt_R317S06_Func(Event* e);
extern "C" void Evt_R317S07_Func(Event* e);
extern "C" void Evt_R317S08_Func(Event* e);
extern "C" void Evt_R317S09_Func(Event* e);
extern "C" void Evt_R317S10_Func(Event* e);
extern "C" void Evt_R317S11_Func(Event* e);
extern "C" void Evt_R317S12_Func(Event* e);
extern "C" void Evt_R317S13_Func(Event* e);
extern "C" void Evt_R317S14_Func(Event* e);

// Room init (the Krauser knife fight): hard mode after more than one continue (r_continue_cnt); a larger
// shadow pool; the two-gear elevator (areas 6/7 up / down); the fifteen event callbacks; the fight chain
// on area 8 (or at once after a continue) until Room_flg bit 0; the continue point on area 9 (bit 1).
void R317Init()
{
    R317Work*& wp = r317_work.p;

#line 138 "D:/Bio4/Prog/r317.cpp"
    wp = (R317Work*) MEM_CALLOC(sizeof(R317Work), 1, 0xd);
    // Reference store: the following pG load stays dependent on it (target `stw; lis; lis; lwz`,
    // the PRE'd pG high filling the slot before the load); a struct store lets the load float up.
    IntSet(r317_work.p->hardMode, 0);
    if (pG->r_continue_cnt > 1) {
        r317_work.p->hardMode = 1;
    }
    ShadowMngReAlloc(0x100);
    R317Elevator2Init();
    SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) SceElevator2Main, &r317_elvUp, 1);
    SceAtDataSet_exec(7, 0x12, 0, (TaskFunc) SceElevator2Main, &r317_elvDown, 1);
    EvtMgr.SetFunc("evt_r317s00_func", (void*) Evt_R317S00_Func);
    EvtMgr.SetFunc("evt_r317s01_func", (void*) Evt_R317S01_Func);
    EvtMgr.SetFunc("evt_r317s02_func", (void*) Evt_R317S02_Func);
    EvtMgr.SetFunc("evt_r317s03_func", (void*) Evt_R317S03_Func);
    EvtMgr.SetFunc("evt_r317s04_func", (void*) Evt_R317S04_Func);
    EvtMgr.SetFunc("evt_r317s05_func", (void*) Evt_R317S05_Func);
    EvtMgr.SetFunc("evt_r317s06_func", (void*) Evt_R317S06_Func);
    EvtMgr.SetFunc("evt_r317s07_func", (void*) Evt_R317S07_Func);
    EvtMgr.SetFunc("evt_r317s08_func", (void*) Evt_R317S08_Func);
    EvtMgr.SetFunc("evt_r317s09_func", (void*) Evt_R317S09_Func);
    EvtMgr.SetFunc("evt_r317s10_func", (void*) Evt_R317S10_Func);
    EvtMgr.SetFunc("evt_r317s11_func", (void*) Evt_R317S11_Func);
    EvtMgr.SetFunc("evt_r317s12_func", (void*) Evt_R317S12_Func);
    EvtMgr.SetFunc("evt_r317s13_func", (void*) Evt_R317S13_Func);
    EvtMgr.SetFunc("evt_r317s14_func", (void*) Evt_R317S14_Func);
    EvtMgr.SetFunc("evt_r317s99_func", (void*) Evt_R317S05_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) R317EventS00, 0, 1);
            EvtMgr.EvtReadAram("event/evd/r317s00.evd", 0, 0, 0, 0);
        } else {
            SceExec(0x12, (TaskFunc) R317EventS00, 0, 0, 2, 0);
            EvtMgr.EvtReadAram("event/evd/r317s13.evd", 0, 0, 0, 0);
            EvtMgr.EvtReadAram("event/evd/r317s03.evd", 0, 0, 0, 0);
        }
    }
    EstSet(0, -1, 0, 0, 1, 0, 0x2001, 3, 0, 0);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) R317ContinuePointSet, 0, 1);
    }
}

// Per-frame room main: nothing.
void R317Main()
{
}

// Area 9 once (Room_flg bit 1): area off and a checkpoint save (GameSaveSave2).
static void R317ContinuePointSet()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        SceAtSetEnable(9, 0);
        GameSaveSave2(&GameSave, pSaveData, -1);
    }
}

// The knife fight: the cut scenes chain through the action-button flags in flags_174 (bit 31
// downwards: one bit per event that ran, one per missed button).
void R317EventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtSetEnable(8, 0);
        SceEventStart(0);
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            EvtMgr.EvtReadAram("event/evd/r317s08.evd", 0, 0, 0, 0);
            EvtMgr.EvtReadExec("event/evd/r317s00.evd", 0, 0);
            if (pG->Room_flg[0] & 0x80000000) {
                EvtMgr.EvtFree("event/evd/r317s07.evd");
                EvtMgr.EvtReadExec("event/evd/r317s08.evd", 0, 2);
                for (;;) {
                    SceSleep(1);
                }
            }
            EvtMgr.EvtFree("event/evd/r317s08.evd");
            EvtMgr.EvtReadAram("event/evd/r317s10.evd", 0, 0, 0, 0);
            EvtMgr.EvtReadExec("event/evd/r317s07.evd", 0, 0);
            if (pG->Room_flg[0] & 0x40000000) {
                EvtMgr.EvtFree("event/evd/r317s09.evd");
                EvtMgr.EvtReadExec("event/evd/r317s10.evd", 0, 2);
                for (;;) {
                    SceSleep(1);
                }
            }
            EvtMgr.EvtFree("event/evd/r317s10.evd");
            EvtMgr.EvtReadAram("event/evd/r317s12.evd", 0, 0, 0, 0);
            EvtMgr.EvtReadExec("event/evd/r317s09.evd", 0, 0);
            if (pG->Room_flg[0] & 0x20000000) {
                EvtMgr.EvtFree("event/evd/r317s11.evd");
                EvtMgr.EvtReadExec("event/evd/r317s12.evd", 0, 2);
                for (;;) {
                    SceSleep(1);
                }
            }
            EvtMgr.EvtFree("event/evd/r317s12.evd");
            EvtMgr.EvtReadAram("event/evd/r317s02.evd", 0, 0, 0, 0);
            EvtMgr.EvtReadExec("event/evd/r317s11.evd", 0, 0);
            if (pG->Room_flg[0] & 0x10000000) {
                EvtMgr.EvtFree("event/evd/r317s01.evd");
                EvtMgr.EvtReadExec("event/evd/r317s02.evd", 0, 2);
                for (;;) {
                    SceSleep(1);
                }
            }
            EvtMgr.EvtFree("event/evd/r317s02.evd");
            EvtMgr.EvtReadAram("event/evd/r317s04.evd", 0, 0, 0, 0);
            EvtMgr.EvtReadExec("event/evd/r317s01.evd", 0, 0);
            if (pG->Room_flg[0] & 0x08000000) {
                EvtMgr.EvtFree("event/evd/r317s03.evd");
                EvtMgr.EvtReadExec("event/evd/r317s04.evd", 0, 2);
                for (;;) {
                    SceSleep(1);
                }
            }
            EvtMgr.EvtFree("event/evd/r317s04.evd");
        }
        EvtMgr.EvtReadAram("event/evd/r317s14.evd", 0, 0, 0, 0);
        EvtMgr.EvtReadExec("event/evd/r317s03.evd", 0, 0);
        RsfSet(G_ROOM_ID, 2);
        GameSaveSave2(&GameSave, pSaveData, -1);
        if (pG->Room_flg[0] & 0x04000000) {
            EvtMgr.EvtFree("event/evd/r317s13.evd");
            EvtMgr.EvtReadExec("event/evd/r317s14.evd", 0, 2);
            for (;;) {
                SceSleep(1);
            }
        }
        EvtMgr.EvtFree("event/evd/r317s14.evd");
        EvtMgr.EvtReadExec("event/evd/r317s13.evd", 0, 0);
        if (pG->Room_flg[0] & 0x02000000) {
            EvtMgr.EvtFree("event/evd/r317s05.evd");
            EvtMgr.EvtReadExec("event/evd/r317s06.evd", 0, 2);
            for (;;) {
                SceSleep(1);
            }
        }
        EvtMgr.EvtReadExec("event/evd/r317s05.evd", 0, 0);
        SceEventEnd(0);
        void* zero = 0;
        SetPosXYZ(pPL, 6970.0f, 3006.0f, -26415.0f);
        {
            Vec v;

            v.x = 0.0f;
            v.y = 1.5f;
            v.z = 0.0f;
            pPL->setAng(&v);
        }
        RsfSet(G_ROOM_ID, 0);
        ScfFlagOn(pG, SCF_R317_LEON_WOUND);
        pPL->setWound();
        ScfFlagOn(pG, SCF_R317_KNIFE_BATTLE);
        OpeSetOpenTerm(0x14, 0.0f, 0.0f, 0.0f, 0.0f);
        EstSet(0, -1, 0, 0, 1, 0, 0x2001, 3, (u32) zero, zero);
    }
}

// The cage waits at the floor the player left from.
void R317Elevator2Init()
{
    cObj* obj = SmdGetObjPtr(0x2A);
    Vec pos0 = {0.0f, 0.0f, 0.0f};
    Vec pos1 = {0.0f, 10825.0f, 0.0f};

    // The struct view keeps the pG load below the template-copy stores.
    if (RsfCheck(*(u16*) &pGS->stage_no, 3)) {
        obj->setPos(&pos1);
    } else {
        obj->setPos(&pos0);
    }
}

// sce_com's SceElevator for the gear elevator: the cage accelerates until the fade, the arrival
// starts stopDist2 away at full speed and decelerates; the two gears scroll their texture with
// the speed.
void SceElevator2Main(SceElevator2Data* d)
{
    cPlayer* pl = pPL;
    cObj* obj;
    cObj* gear1;
    cObj* gear2;
    f32 rotMax;
    f32 rotAccel;
    f32 stopDist;
    f32 stopDist2;
    // Initialised at the declaration (a dead set: the pool gets the 0.0 first) and again with the
    // speed reset below.
    f32 rot = 0.0f;
    f32 step;
    f32 move;
    f32 gearSpd;
    f32 gearSpdUp;
    f32 y;
    int faded;
    int done;
    int i;
    int j;
    u32* hSnd;
    FadeWork* fade;

    obj = SmdGetObjPtr(d->objId);
    gear1 = SmdGetObjPtr(d->objId2);
    gear2 = SmdGetObjPtr(d->objId3);
    if (obj == 0 || gear1 == 0 || gear2 == 0) {
        return;
    }
    rotMax = 0.08f;
    rotAccel = 0.001f;
    hSnd = &((R317ElevatorObjView*) obj)->hSnd;
    faded = 0;
    if (d->dir == 3) {
        stopDist = CalcStopDist(r317_elvMaxSpd, r317_elvAccelUp);
        stopDist2 = stopDist + r317_elvStopAddUp;
        RsfSet(G_ROOM_ID, 3);
    } else {
        stopDist = CalcStopDist(r317_elvMaxSpd, r317_elvAccelDown);
        stopDist2 = stopDist + r317_elvStopAddDown;
        RsfClear(G_ROOM_ID, 3);
    }
    *hSnd = 0;
    gear1->pModelInfo->flagsDC |= 1;
    gear2->pModelInfo->flagsDC |= 1;
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) SceElevator2End, (int) d, -1, 1);
    obj->setNoSuspend(1);
    obj->setPos(&d->pos);
    pPL->setNoSuspend(1);
    ((cUnitEventView*) pPL)->beginEvent(0);
    pPL->setPos(&d->plPos);
    pPL->setAng(&d->plRot);
    pPL->be_flag &= ~0x10;
    if (d->cut != -1) {
        CamCtrl.CutCall((s8) d->cut);
    }
    FSet(r317_elvSpd, 0.0f);
    rot = 0.0f;
    if ((s16) d->seStart != -1) {
        *hSnd = SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
    }
    rot = rotAccel + rot;
    for (i = 0; i < 10; i++) {
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
        SetPosXYZ(obj, obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
        SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
        SceSleep(1);
    }
    obj->setPos(&d->pos);
    pPL->setPos(&d->plPos);
    fade = &Fade[2];
    for (;;) {
        if (d->dir == 3) {
            r317_elvSpd += r317_elvAccelDown;
        } else {
            r317_elvSpd += r317_elvAccelUp;
        }
        if (r317_elvSpd > r317_elvMaxSpd) {
            r317_elvSpd = r317_elvMaxSpd;
        }
        if (d->dir == 3) {
            step = r317_elvSpd;
        } else {
            step = -r317_elvSpd;
        }
        SetPosXYZ(obj, obj->pos.x, obj->pos.y + step, obj->pos.z);
        SetPosXYZ(pPL, pPL->pos.x, pPL->pos.y + step, pPL->pos.z);
        if (rot > rotMax) {
            rot = rotMax;
        }
        // The up loop's own gear speed variable: `rot` stays cse-canonical here (`fneg f13, f30`, r225).
        gearSpdUp = rot;
        if (d->dir != 3) {
            gearSpdUp = -rot;
        }
        gear1->pModelInfo->uvScrollV = gearSpdUp;
        gear2->pModelInfo->uvScrollV = -gearSpdUp;
        if (faded == 0) {
            f32 fadeSpd;

            if (d->dir == 3) {
                fadeSpd = r317_elvFadeSpdUp;
            } else {
                fadeSpd = r317_elvFadeSpdDown;
            }
            if (r317_elvSpd >= fadeSpd) {
                FadeSetW(2, 30, 0, 0);
                faded = 1;
            }
        } else if ((fade->flags & 1) == 0) {
            if (*hSnd) {
                SndStop(*hSnd, 0);
            }
            break;
        }
        SceSleep(1);
        // COMPILER-DIFF: candidate (sched1 loop-note barrier). The target issues `rot += rotAccel`
        // after the SceSleep call; sched1 only keeps it there behind a loop note (r225).
        do { } while (0);
        rot += rotAccel;
    }
    // COMPILER-DIFF: candidate #12. Ends cse1's extended block: without it the up loop's pPL high is
    // carried into this block (the break arm falls into it), PRE's copy of the reaching register is then
    // propagated here and the block loses its own `lis pPL@ha` (the target re-materialises it).
    do { } while (0);
    obj->setNoSuspend(1);
    obj->setPos(&d->pos2);
    pPL->setNoSuspend(1);
    ((cUnitEventView*) pPL)->beginEvent(0);
    pPL->setPos(&d->plPos2);
    pPL->setAng(&d->plRot);
    pPL->be_flag &= ~0x10;
    if (d->cut2 != -1) {
        CamCtrl.CutCall((s8) d->cut2);
    }
    FSet(r317_elvSpd, r317_elvMaxSpd);
    rot = rotMax;
    move = stopDist2;
    if (d->dir == 3) {
        move = -move;
    }
    SetPosXYZ(obj, obj->pos.x, obj->pos.y + move, obj->pos.z);
    SetPosXYZ(pPL, pl->pos.x, pPL->pos.y + move, pl->pos.z);
    FadeSetW(0x80000002, 30, 0, 0);
    if ((s16) d->seMove != -1) {
        *hSnd = SndCall(6, d->seMove, &obj->pos, 0, 0, 0);
    }
    for (;;) {
        y = obj->pos.y;
        if (__builtin_fabsf(d->pos2.y - y) < stopDist) {
            if (d->dir == 3) {
                r317_elvSpd -= r317_elvAccelUp;
            } else {
                r317_elvSpd -= r317_elvAccelDown;
            }
            if (r317_elvSpd < r317_elvMinSpd) {
                r317_elvSpd = r317_elvMinSpd;
            }
            rot -= rotAccel;
            if (rot < rotAccel) {
                rot = rotAccel;
            }
        }
        if (d->dir == 3) {
            move = r317_elvSpd;
        } else {
            move = -r317_elvSpd;
        }
        SetPosXYZ(obj, obj->pos.x, obj->pos.y + move, obj->pos.z);
        SetPosXYZ(pPL, pPL->pos.x, pPL->pos.y + move, pPL->pos.z);
        gearSpd = rot;
        if (d->dir != 3) {
            gearSpd = -rot;
        }
        gear1->pModelInfo->uvScrollV = gearSpd;
        gear2->pModelInfo->uvScrollV = -gearSpd;
        done = 0;
        if (d->dir == 3) {
            if (obj->pos.y >= d->pos2.y) {
                done = 1;
            }
        }
        if (d->dir == 1) {
            if (obj->pos.y <= d->pos2.y) {
                done = 1;
            }
        }
        if (done != 0) {
            if (*hSnd) {
                SndStop(*hSnd, 0);
            }
            *hSnd = 0;
            if ((s16) d->seStop != -1) {
                SndCall(6, d->seStop, &obj->pos, 0, 0, 0);
            }
            obj->setPos(&d->pos2);
            pPL->setPos(&d->plPos2);
            pPL->setAng(&d->plRot);
            gear1->pModelInfo->uvScrollV = 0.0f;
            gear2->pModelInfo->uvScrollV = 0.0f;
            break;
        }
        SceSleep(1);
    }
    for (j = 0; j < 10; j++) {
        obj->setPos(&d->pos2);
        pPL->setPos(&d->plPos2);
        SetPosXYZ(obj, obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
        SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
        SceSleep(1);
    }
    obj->setPos(&d->pos2);
    pPL->setPos(&d->plPos2);
    SceSetEventCancel(0, 0, 0, -1, 1);
    SceElevator2End(d);
}

// End of the two-gear elevator ride: fade killed, the cage and the player snapped to the arrival
// positions, the gear UV scroll stopped, the motor sound stopped with the stop SE.
void SceElevator2End(SceElevator2Data* d)
{
    cObj* obj = SmdGetObjPtr(d->objId);
    cObj* gear1 = SmdGetObjPtr(d->objId2);
    cObj* gear2 = SmdGetObjPtr(d->objId3);

    if (obj && gear1 && gear2) {
        FadeKill(2);
        obj->setPos(&d->pos2);
        pPL->setPos(&d->plPos2);
        gear1->pModelInfo->uvScrollV = 0.0f;
        gear2->pModelInfo->uvScrollV = 0.0f;
        if (((R317ElevatorObjView*) obj)->hSnd) {
            SndStop(((R317ElevatorObjView*) obj)->hSnd, 0);
            ((R317ElevatorObjView*) obj)->hSnd = 0;
            if ((s16) d->seStop != -1) {
                SndCall(6, d->seStop, &obj->pos, 0, 0, 0);
            }
        }
        pPL->be_flag |= 0x10;
        SceEventEnd(0);
        SceExit();
    }
}

// Action button reactions: clear the event's "missed" bit.
static void R317EventS00Action()
{
    pG->Room_flg[0] &= ~0x80000000;
}

// Action button of s07 pressed in time: clear its "missed" bit (Room_flg[0] 0x40000000).
static void R317EventS07Action()
{
    pG->Room_flg[0] &= ~0x40000000;
}

// Action button of s09: clear Room_flg[0] 0x20000000.
static void R317EventS09Action()
{
    pG->Room_flg[0] &= ~0x20000000;
}

// Action button of s11: clear Room_flg[0] 0x10000000.
static void R317EventS11Action()
{
    pG->Room_flg[0] &= ~0x10000000;
}

// Action button of s01: clear Room_flg[0] 0x08000000.
static void R317EventS01Action()
{
    pG->Room_flg[0] &= ~0x08000000;
}

// Action button of s03: clear Room_flg[0] 0x04000000.
static void R317EventS03Action()
{
    pG->Room_flg[0] &= ~0x04000000;
}

// Show every scroll object the fight cuts hide (the arena props).
void R317SmdAllOn()
{
    SmdSetTrans(0xA, 1);
    SmdSetTrans(0x26, 1);
    SmdSetTrans(0x27, 1);
    SmdSetTrans(0x28, 1);
    SmdSetTrans(0x29, 1);
    SmdSetTrans(0x2F, 1);
    SmdSetTrans(0x30, 1);
    SmdSetTrans(0x35, 1);
    SmdSetTrans(0x21, 1);
    SmdSetTrans(0x24, 1);
    SmdSetTrans(0x25, 1);
}

// Event r317s00 callback (the fight's first cut): "missed" bit 31 preset, a coin toss (0x00200000) picks
// the button variant, status 3 with cancel cut 3; the action-button window on its cut clears the bit
// through R317EventS00Action; the Leon / Krauser / knife models' parts per cut.
void Evt_R317S00_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        pG->Room_flg[0] |= 0x80000000;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x00200000;
        } else {
            pG->Room_flg[0] &= ~0x00200000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 3;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        if (e->NowCut != 5) {
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
                SmdSetTrans(0x26, 0);
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x28, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s07.evd", 0, 0, 0, 0);
        }
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
        } else {
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 1);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
        }
        if (r317_work.p->hardMode == 0) {
            if (e->NowCut > 4 && e->NowFrame >= 0) {
                on = 1;
            }
        } else {
            if ((e->NowCut == 4 && e->NowFrame > 8) || (e->NowCut > 4 && e->NowFrame >= 0)) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (pG->Room_flg[0] & 0x80000000) {
                KnifeActBtn(0x00200000, R317EventS00Action);
            } else {
                e->CancelSet();
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s01 callback: as s00 with the missed bit 0x08000000, coin toss 0x00020000, cancel cut 6.
void Evt_R317S01_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        pG->Room_flg[0] |= 0x08000000;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x00020000;
        } else {
            pG->Room_flg[0] &= ~0x00020000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 6;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
                SmdSetTrans(0x26, 0);
                SmdSetTrans(0x27, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s03.evd", 0, 0, 0, 0);
        }
        if (r317_work.p->hardMode == 0) {
            if (e->NowCut == 6 && e->NowFrame > 6) {
                on = 1;
            }
        } else {
            if (e->NowCut == 6 && e->NowFrame > 0) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (pG->Room_flg[0] & 0x08000000) {
                KnifeActBtn(0x00020000, R317EventS01Action);
            } else {
                e->CancelSet();
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s02 callback (no button): the Leon model's part 6 hidden, evmd200 drawn with ot_type 1.
void Evt_R317S02_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "evmd200", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
                ((cModel*) mod)->LightInfo.EnableMask = 1;
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        if (e->NowCut == 1) {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x2F, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x2F, 1);
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s03 callback: after the fight was continued (Room_flg bit 2) it starts at cut 5 of stream
// 0x75; missed bit 0x04000000, coin toss 0x00010000; the Krauser (em3900) parts per cut.
void Evt_R317S03_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        if (RsfCheck(G_ROOM_ID, 2)) {
            e->ChangeNoStr = 0x75;
            e->ChangeNowCut = 5;
        }
        pG->Room_flg[0] |= 0x04000000;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x00010000;
        } else {
            pG->Room_flg[0] &= ~0x00010000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 0x14;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        switch (e->NowCut) {
        case 0x13:
        case 0x14:
        case 0x15:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
            }
            break;
        case 1:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
                SmdSetTrans(0x35, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
            }
            break;
        }
        if (e->NowCut == 4) {
            SmdSetTrans(0x25, 0);
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s13.evd", 0, 0, 0, 0);
        }
        {
            void* mod;

            if (e->NowCut > 0x10) {
                if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 0, 0);
                }
            } else {
                if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 0, 1);
                }
            }
        }
        switch (e->NowCut) {
        case 0xE:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x2E, 0);
            }
            break;
        case 0xD:
        case 0xF:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x2E, 1);
            }
            break;
        }
        if (r317_work.p->hardMode == 0) {
            if ((e->NowCut == 0x14 && e->NowFrame > 0x17) || (e->NowCut == 0x15 && e->NowFrame >= 0)) {
                on = 1;
            }
        } else {
            if ((e->NowCut == 0x14 && e->NowFrame > 0x11) || (e->NowCut == 0x15 && e->NowFrame >= 0)) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (pG->Room_flg[0] & 0x04000000) {
                KnifeActBtn(0x00010000, R317EventS03Action);
            } else {
                e->CancelSet();
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s04 callback (no button): Leon's part 6 and Krauser's part 7 hidden on cut 0.
void Evt_R317S04_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s05 callback (no button): the hard-mode Krauser model em3900h's part 7 hidden; per-cut
// model flags and effects.
void Evt_R317S05_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1: {
        void* mod;

        if (e->NowCut == 0 && e->NowFrame == 0) {
            if (e->GetMod(&mod, "em3900h", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        switch (e->NowCut) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 6, 1);
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x35, 0);
            }
            break;
        case 3:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x27, 0);
            }
            break;
        case 1:
        case 2:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
            }
            break;
        case 6:
        case 7:
        case 8:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x2F, 0);
                SmdSetTrans(0x30, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
            }
            break;
        }
        if (e->NowCut == 0xF) {
            SmdSetTrans(0x24, 0);
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            if (e->GetMod(&mod, "evm7800", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
        }
        break;
    }
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s06 callback (no button): Leon's part 6 and em3900h's part 7 hidden on cut 0.
void Evt_R317S06_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900h", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s07 callback: missed bit 0x40000000, coin toss 0x00100000, cancel cut 7; the button window
// clears the bit via R317EventS07Action.
void Evt_R317S07_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        pG->Room_flg[0] |= 0x40000000;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x00100000;
        } else {
            pG->Room_flg[0] &= ~0x00100000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 7;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        switch (e->NowCut) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
            }
            break;
        case 5:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x26, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
            }
            break;
        case 6:
        case 7:
        case 8:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
            }
            break;
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s09.evd", 0, 0, 0, 0);
        }
        if (r317_work.p->hardMode == 0) {
            if ((e->NowCut == 7 && e->NowFrame > 0x4C) || (e->NowCut == 8 && e->NowFrame >= 0)) {
                on = 1;
            }
        } else {
            if ((e->NowCut == 7 && e->NowFrame > 0x46) || (e->NowCut == 8 && e->NowFrame >= 0)) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (pG->Room_flg[0] & 0x40000000) {
                KnifeActBtn(0x00100000, R317EventS07Action);
            } else {
                e->CancelSet();
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s08 callback (no button): Leon's part 6 and Krauser's part 7 hidden, per-cut flags.
void Evt_R317S08_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "evmd400", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s09 callback: missed bit 0x20000000 with its coin toss and cancel cut; button via R317EventS09Action.
void Evt_R317S09_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        pG->Room_flg[0] |= 0x20000000;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x00080000;
        } else {
            pG->Room_flg[0] &= ~0x00080000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 8;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        switch (e->NowCut) {
        case 0:
        case 1:
        case 2:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0x2F, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
            }
            break;
        }
        if (e->NowCut == 3) {
            SmdSetTrans(0x21, 0);
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s11.evd", 0, 0, 0, 0);
        }
        if (r317_work.p->hardMode == 0) {
            if ((e->NowCut == 8 && e->NowFrame > 0xF) || (e->NowCut == 9 && e->NowFrame >= 0)) {
                on = 1;
            }
        } else {
            if ((e->NowCut == 8 && e->NowFrame > 9) || (e->NowCut == 9 && e->NowFrame >= 0)) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (pG->Room_flg[0] & 0x20000000) {
                KnifeActBtn(0x00080000, R317EventS09Action);
            } else {
                e->CancelSet();
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s10 callback (no button): Leon's part 6 hidden, evmd400 (the knife) drawn.
void Evt_R317S10_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "evmd400", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s11 callback: missed bit 0x10000000, coin toss 0x00040000, cancel cut 2; button via R317EventS11Action.
void Evt_R317S11_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        pG->Room_flg[0] |= 0x10000000;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x00040000;
        } else {
            pG->Room_flg[0] &= ~0x00040000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 2;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            R317SmdAllOn();
            SmdSetTrans(0xA, 0);
            SmdSetTrans(0x28, 0);
            SmdSetTrans(0x29, 0);
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s01.evd", 0, 0, 0, 0);
        }
        if (r317_work.p->hardMode == 0) {
            if (e->NowCut == 2 && e->NowFrame > 0x49) {
                on = 1;
            }
        } else {
            if (e->NowCut == 2 && e->NowFrame > 0x43) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (pG->Room_flg[0] & 0x10000000) {
                KnifeActBtn(0x00040000, R317EventS11Action);
            } else {
                e->CancelSet();
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// Event r317s12 callback (the button-mash cut): counts the presses into W->btnCount; Leon's part 6 and
// evmd400 per cut.
void Evt_R317S12_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "evmd400", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}

// The button-mashing duel: count the presses of the prompted button, 17 clears the "missed" bit.
void Evt_R317S13_Func(Event* e)
{
    int on = 0;

    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        pG->Room_flg[0] |= 0x02000000;
        r317_work.p->btnCount = 0;
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x8000;
        } else {
            pG->Room_flg[0] &= ~0x8000;
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 9;
        break;
    case 1: {
        int btn;

        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
            if (e->GetMod(&mod, "em3900h", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
            }
        }
        switch (e->NowCut) {
        case 1:
        case 2:
        case 3:
        case 4:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
                SmdSetTrans(0x2F, 0);
                SmdSetTrans(0x30, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                R317SmdAllOn();
                SmdSetTrans(0xA, 0);
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x28, 0);
                SmdSetTrans(0x29, 0);
            }
            break;
        }
        if (e->NowCut == 0 && e->NowFrame == 1) {
            EvtMgr.EvtReadAram("event/evd/r317s05.evd", 0, 0, 0, 0);
        }
        if (r317_work.p->hardMode == 0) {
            if (e->NowCut == 0xA && e->NowFrame >= 0) {
                on = 1;
            }
        } else {
            if ((e->NowCut == 9 && e->NowFrame > 0xE) || (e->NowCut == 0xA && e->NowFrame >= 0)) {
                on = 1;
            }
        }
        if (on == 1) {
            SpfFlagOff(pG, SPF_ACTBTN);
            if (e->NowCut == 0xA && e->NowFrame > 0x27) {
                btn = 0xD;
                if ((pG->Room_flg[0] & 0x8000) == 0) {
                    btn = 2;
                }
            } else {
                btn = 0xD;
                if (pG->Room_flg[0] & 0x8000) {
                    btn = 2;
                }
            }
            DpfFlagOff(pG, DPF_MESSAGE);
            ActBtn.set(0x30, 5, 0, 0, 0x42, btn, 0, 0);
            if (btn == 2) {
                if (Key.trg & 0x00080000) {
                    r317_work.p->btnCount++;
                }
            } else {
                if (Key.trg & 0x00040000) {
                    r317_work.p->btnCount++;
                }
            }
            if (r317_work.p->btnCount > 0x10) {
                pG->Room_flg[0] &= ~0x02000000;
            }
            eprintf(0x40, 0x10, 0, 0, "HItPoint:[%d]/[%d]", r317_work.p->btnCount, 0x11);
        }
        break;
    }
    case 2:
        R317SmdAllOn();
        SmdSetTrans(0x2E, 1);
        break;
    }
}

// Event r317s14 callback (the fight's last cut): Leon's part 6 / evmd400 per cut; the end restores the arena.
void Evt_R317S14_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        EffectDelete2001();
        e->CancelNoSet();
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "evmd400", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
        }
        break;
    case 2:
        R317SmdAllOn();
        break;
    }
}
