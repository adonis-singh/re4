#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "joy.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "player.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "model.h"
#include "act_btn.h"
#include "mes.h"
#include "snd.h"
#include "fade.h"
#include "shadow.h"
#include "esp.h"
#include "rnd.h"
#include "math_sub.h"
#include "eprintf.h"
#include "db_log.h"

// Room 3-0B (D:/Bio4/Prog/r30b.cpp): the crane puzzle. The player drives the crane's magnet over the four
// Ganado (the caught ones ride along and are dropped in the pit), the switch-operated door with its guards,
// the s00 escape event and the shelf item box.

// The crane state machine (r30b_work->cr, cleared with memset at every attempt).
struct R30bCraneWork {
    int step;           // 0x00
    Vec pos;            // 0x04  magnet position
    int cnt;            // 0x10
    int active;         // 0x14
    int catchIdx[11];   // 0x18  enemy indices hanging from the magnet
    int nCatch;         // 0x44
    Vec vel;            // 0x48
};

struct R30bWork {
    cObj* crane;              // 0x0000  the arm
    cObj* magnet;             // 0x0004
    cObj* cable;              // 0x0008
    cEmPatrol patrol[11];     // 0x000C
    cEmRouteRun run[11];      // 0x0C40
    cEmRouteExec exec[11];    // 0x1874
    cEmWrap em[11];           // 0x24A8
    R30bCraneWork cr;         // 0x252C
    int tryLeft;              // 0x2580
    int timer;                // 0x2584
    u32 se;                   // 0x2588  crane motor sound
};

// The work pointer is a struct member: every store through the work reloads it.
struct R30bWorkPtr {
    R30bWork* p;
};
static R30bWorkPtr r30b_work;

// COMPILER-DIFF: 3 -- the varargs view of memset gives the `crclr; bl memset` of the rotation
// clears; the crane work is cleared through the prototyped memset (no crclr).
extern "C" void* r30b_memset(void*, ...) asm("memset");
extern "C" void* memset(void* dst, int c, unsigned int n);

// COMPILER-DIFF: #4 -- the original masks the u8 result of GetEmIdFromList before passing it on;
// ours treats the return as promoted. An int view of the callee plus the (u8) cast gives the clrlwi.
int GetEmIdFromListI(u32 no) asm("GetEmIdFromList");

static f32 r30b_cableOfs = 6000.0f;
static f32 r30b_spd = 100.0f;
static f32 r30b_accel = 5.0f;
static Vec r30b_patrolTbl0[2] = {{-11900.0f, -2000.0f, -6500.0f}, {-7000.0f, -2000.0f, -5000.0f}};
static Vec r30b_patrolTbl1[2] = {{-11500.0f, -2000.0f, -600.0f}, {-7400.0f, -2000.0f, -200.0f}};
static EmControlPoint r30b_sitTbl0[1] = {{{-6870.0f, -2000.0f, -3900.0f}, 0xE}};
static EmControlPoint r30b_sitTbl1[2] = {{{-11590.0f, 1000.0f, -12800.0f}, 8}, {{-8790.0f, -2000.0f, -2610.0f}, 0xE}};
static EmControlPoint r30b_sitTbl3[1] = {{{-6000.0f, -2000.0f, -1200.0f}, 0xE}};
static EmControlPoint r30b_sitTbl2[2] = {{{-11590.0f, 1000.0f, -12800.0f}, 8}, {{-7480.0f, -2000.0f, 1400.0f}, 0xE}};
static EmControlPoint r30b_sitTbl2b[1] = {{{-7480.0f, -2000.0f, 1400.0f}, 0xE}};
static Vec r30b_gotoTbl[3] = {{-5600.0f, -2000.0f, -4900.0f}, {-5780.0f, 1000.0f, -14700.0f}, {-10000.0f, 1000.0f, -14700.0f}};
static Vec r30b_gotoTbl2[1] = {{-5600.0f, -2000.0f, -4900.0f}};
// The module's .data tail is 8-aligned in the original (0x23C -> 0x240; st3.cpp's linker word follows).
asm(".section .data\n\t.balign 8\n\t.text");

extern "C" {
static void r30b_movedShelf(int no);
static void r30b_moveShelf(int no);
void R30bDoorEventEmMove();
static void R30bDoorEvent00Main();
static void R30bDoorEvent00End();
static void R30bDoorSwitchMain();
static void R30bDoorSwitchEnd();
void R30bDoorOpen(int open, int emGoto);
void R30bDoorOpened(int open);
static void R30bEmReset();
int CalcMovePosDistAdd2(Vec* pos, Vec* target, Vec* vel, f32 max, f32 add);
static void R30bCraneEnd();
static void R30bCrane();
static void R30bEventS00();
void Evt_R30BS00_Func(Event* e);
void R30bEmWanderingSet();
static void R30bEmSitDownSet();
static void R30bEmGotoSet();
static void R30bEmGotoSet2();
void SetCatchEm(int no);
int CkCatchEm(int no);
static void SceBgmCheck();
}

// The cable / light positions go through an inline (r315 setPosXYZ): its Vec is the inliner's own frame
// slot and its address is not a PRE candidate, unlike a block-local `Vec p` whose `&p` gets inserted
// at the end of every switch arm (the light setPos is the loop tail's unconditional occurrence).
static inline void r30b_setObjPos(cModel* m, f32 x, f32 y, f32 z)
{
    Vec p;

    p.x = x;
    p.y = y;
    p.z = z;
    m->setPos(&p);
}

// Room init (the crane hall): the s00 callback. With Ashley along (Status_flg[3] 0x04000000): area 3 =
// the s00 escape event until Room_flg bit 0, area 2 off; without her: area 4 = the crane puzzle and area
// 0x10 = its end until bit 2. The crane / magnet / cable objects, the four crane Ganados with their
// patrols and routes, or the return layout (R30bEmReset); area 9 = the door opening from the other side
// until bit 3, area 8 = the door switch; the door posed per bit 5; the shelf item event; the stream.
void R30bInit()
{
#line 63 "D:/Bio4/Prog/r30b.cpp"
    r30b_work.p = (R30bWork*) MEM_CALLOC(sizeof(R30bWork), 1, 0xd);
    EvtMgr.SetFunc("evt_r30bs00_func", (void*) Evt_R30BS00_Func);
    if (StaFlagChk(pG, STA_SUB_ASHLEY)) {
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) R30bEventS00, 0, 1);
            EvtMgr.EvtReadAram("event/evd/r30bs00.evd", (u8) GetEmIdFromListI(0x56), 0, 0, 0);
        }
        SceAtSetEnable(2, 0);
        SceAtSetEnable(3, 1);
    } else {
        SceAtSetEnable(2, 1);
        SceAtSetEnable(3, 0);
    }
    if (StaFlagChk(pG, STA_SUB_ASHLEY) == 0 && RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) R30bCrane, 0, 1);
        SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) R30bCraneEnd, 0, 1);
        SceAtSetEnable(6, 0);
    } else {
        SceAtSetEnable(6, 1);
    }
    {
        Vec pos = {-14000.0f, 3000.0f, -5000.0f};
        Vec rot;

        r30b_memset(&rot, 0, sizeof(Vec));
        r30b_work.p->crane = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0x10, 1);
        {
            cObj* crane = r30b_work.p->crane;

            if (crane) {
                MotionSetCore(crane, &crane->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
                crane->be_flag |= 0x1000;
                r30b_work.p->cable = SetObjSmd(ROOM_ARC_PTR(pGS->pRoom, 0x1F), ROOM_ARC_PTR(pGS->pRoom, 0x20), &crane->pos, &crane->ang, 0x10, 1);
                {
                    cObj* cable = r30b_work.p->cable;

                    if (cable) {
                        MotionSetCore(cable, &cable->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
                        // COMPILER-DIFF: candidate (sched1 priority) -- the target issues the `be_flag` store before the
                        // pool word0 load of `sca`; with the plain stores the be_flag store's only dependents are the three
                        // sca stack stores (output dep, prio 5) and the pool load (prio 6 = word0 stack store + load
                        // latency) wins the lsu slot. Two VOLATILE stores conflict unconditionally (alias.c
                        // write_dependence_p: MEM_VOLATILE_P on both), so the be_flag store gains the pos.y store as a
                        // dependent (prio 6, tie broken by register weight in the store's favour). The loads stay
                        // plain (a volatile pos.y load would have to wait for the be_flag store).
                        {
                            volatile cObj* vc = cable;

                            vc->be_flag = cable->be_flag | 0x1010;
                            vc->pos.y = cable->pos.y + r30b_cableOfs;
                        }
                        Vec sca = {0.3f, 0.3f, 0.3f};
                        cable->setSca(&sca);
                    }
                }
                r30b_work.p->magnet = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x20), &crane->pos, &crane->ang, 0x10, 1);
                {
                    cObj* magnet = r30b_work.p->magnet;

                    if (magnet) {
                        magnet->be_flag |= 0x1000;
                    }
                }
                {
                    cObj* light = SmdGetObjPtr(0xD);

                    if (light) {
                        r30b_setObjPos(light, light->pos.x, light->pos.y, crane->pos.z);
                    }
                }
            }
        }
    }
    r30b_work.p->tryLeft = 3;
    r30b_work.p->timer = 0;
    if (StaFlagChk(pGS, STA_SUB_ASHLEY) == 0) {
        r30b_work.p->em[0].setPtr(0x35, -1, 0);
        r30b_work.p->em[1].setPtr(0x36, -1, 0);
        r30b_work.p->em[2].setPtr(0x3F, -1, 0);
        r30b_work.p->em[3].setPtr(0x43, -1, 0);
        R30bEmWanderingSet();
    } else {
        SceExec(0x12, (TaskFunc) R30bEmReset, 0, 0, 2, 0);
        SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, 2, 0);
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            SceAtDataSet_exec(9, 0x12, 0, (TaskFunc) R30bDoorEvent00Main, 0, 1);
        }
    }
    SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) R30bDoorSwitchMain, 0, 1);
    if (RsfCheck(G_ROOM_ID, 5)) {
        R30bDoorOpened(1);
    } else {
        R30bDoorOpened(0);
    }
    SceSetItemEvent(5, 0x80, 1, 4, r30b_moveShelf, (void (*)()) r30b_movedShelf, 0x80, 0);
}

// Per-frame room main: nothing.
void R30bMain()
{
}

// The shelf item box.
static void r30b_movedShelf(int no)
{
    if (no == 0x80) {
        OpenBoxMain(2, 1, 0x1C, 0x1D, -1, -1);
    }
}

// Item-event opener: the shelf locker (object 0x1D, type 2) opens.
static void r30b_moveShelf(int no)
{
    if (no == 0x80) {
        OpenBoxMain(2, 0, 0x1C, 0x1D, -1, -1);
    }
}

// The door event's Ganado walks to the door.
void R30bDoorEventEmMove()
{
    Vec pos = {6630.0f, 0.0f, 3660.0f};
    int i;

    r30b_work.p->em[9].setEm(0x61, -1, 0, 1, 1);
    {
        cEmWrap* em = &r30b_work.p->em[9];

        em->setNoSuspend(1);
        em->setGoto(&pos, 4);
        while (em->ckGoto() != 0) {
            SceSleep(1);
        }
    }
    SndCall(6, 4, 0, 0, 0, 0);
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
}

// Area 9: the door opens from the other side.
static void R30bDoorEvent00Main()
{
    if (RsfCheck(G_ROOM_ID, 5) == 0 && RsfCheck(G_ROOM_ID, 3) == 0) {
        RsfSet(G_ROOM_ID, 3);
        SceAtSetEnable(9, 0);
        SceEventStart(1);
        SceSetEventCancel(1, (TaskFunc) R30bDoorEvent00End, 0, -1, 1);
        CamCtrl.CutCall(6);
        R30bDoorEventEmMove();
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        R30bDoorOpen(1, 0);
        SceSetEventCancel(0, 0, 0, -1, 1);
        R30bDoorEvent00End();
    }
}

// End of the door event (also its cancel path): the door snapped open, Ganado em[9] may suspend, camera back, SceEventEnd, task exit.
static void R30bDoorEvent00End()
{
    R30bDoorOpened(1);
    r30b_work.p->em[9].setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Area 8: the door switch.
static void R30bDoorSwitchMain()
{
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceUpCut(6, -1, -1, 4);
    } else {
        SceEventStart(1);
        SceMesCamSndSet4(5, -1, -1, 4);
        if (SceMesGetSelection() != 1) {
            CamCtrl.Comeback(0);
            SceEventEnd(0);
            SceExit();
        } else {
            int i;

            if (RsfCheck(G_ROOM_ID, 6) == 0) {
                r30b_work.p->em[4].setEm(0x37, -1, 0, 1, 1);
                r30b_work.p->em[5].setEm(0x38, -1, 0, 1, 1);
                r30b_work.p->em[6].setEm(0x5F, -1, 0, 1, 1);
                r30b_work.p->em[4].setNoSuspend(1);
                r30b_work.p->em[5].setNoSuspend(1);
                r30b_work.p->em[6].setNoSuspend(1);
            }
            SceSetEventCancel(1, (TaskFunc) R30bDoorSwitchEnd, 0, -1, 1);
            SndCall(6, 4, 0, 0, 0, 0);
            if (RsfCheck(G_ROOM_ID, 6) == 0) {
                R30bDoorOpen(0, 1);
            } else {
                R30bDoorOpen(0, 0);
            }
            if (RsfCheck(G_ROOM_ID, 6) == 0) {
                for (i = 0; i < 30; i++) {
                    SceSleep(1);
                }
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            R30bDoorSwitchEnd();
        }
    }
}

// End of the door switch event (also its cancel path): the door snapped shut; the first time (Room_flg
// bit 6) the three guards (em[4..6]) are alerted and released; camera back, SceEventEnd, task exit.
static void R30bDoorSwitchEnd()
{
    R30bDoorOpened(0);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        RsfSet(G_ROOM_ID, 6);
        r30b_work.p->em[4].setFlag(1);
        r30b_work.p->em[5].setFlag(1);
        r30b_work.p->em[6].setFlag(1);
        r30b_work.p->em[4].setNoSuspend(0);
        r30b_work.p->em[5].setNoSuspend(0);
        r30b_work.p->em[6].setNoSuspend(0);
    }
    RsfSet(G_ROOM_ID, 6);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// The door slides up (open 1) or down; emGoto sends the guards at the player once it starts closing.
void R30bDoorOpen(int open, int emGoto)
{
    cObj* door;

    CamCtrl.CutCall(7);
    door = SmdGetObjPtr(0x15);
    if (door) {
        Vec pos;

        if (open == 1) {
            int i;

            SceAtSetEnable(0xD, 1);
            SndCall(6, 2, &door->pos, 0, 0, 0);
            for (i = 0; i < 20; i++) {
                f32 x = door->pos.x;
                f32 z = door->pos.z;

                pos.x = x;
                pos.y = (f32) i * -2500.0f / 20.0f + 2500.0f;
                pos.z = z;
                door->setPos(&pos);
                SceSleep(1);
            }
            SndCall(6, 3, &door->pos, 0, 0, 0);
        } else {
            int i;

            SceAtSetEnable(0xD, 0);
            SndCall(6, 0, &door->pos, 0, 0, 0);
            for (i = 0; i < 20; i++) {
                if (i == 0 && emGoto == 1) {
                    r30b_work.p->em[4].setNoSuspend(1);
                    r30b_work.p->em[5].setNoSuspend(1);
                    r30b_work.p->em[6].setNoSuspend(1);
                    r30b_work.p->em[4].setFlag(1);
                    r30b_work.p->em[5].setFlag(1);
                    r30b_work.p->em[6].setFlag(1);
                    r30b_work.p->em[4].setGoto(&pPL->pos, 1);
                    r30b_work.p->em[5].setGoto(&pPL->pos, 1);
                    r30b_work.p->em[6].setGoto(&pPL->pos, 1);
                }
                f32 x = door->pos.x;
                f32 z = door->pos.z;

                pos.x = x;
                pos.y = (f32) i * 2500.0f / 20.0f + 0.0f;
                pos.z = z;
                door->setPos(&pos);
                SceSleep(1);
            }
            SndCall(6, 1, &door->pos, 0, 0, 0);
        }
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
}

// Snap the door (object 0x15) open (y 0, areas 0xD/0xF on, Room_flg bit 5) or closed (its rest height,
// the areas off, bit 5 cleared).
void R30bDoorOpened(int open)
{
    cObj* door = SmdGetObjPtr(0x15);

    if (door) {
        Vec pos;

        if (open == 1) {
            f32 x = door->pos.x;
            f32 z = door->pos.z;

            pos.x = x;
            pos.y = 0.0f;
            pos.z = z;
            door->setPos(&pos);
            SceAtSetEnable(0xD, 1);
            SceAtSetEnable(0xF, 1);
            RsfSet(G_ROOM_ID, 5);
        } else {
            f32 x = door->pos.x;
            f32 z = door->pos.z;

            pos.x = x;
            pos.y = 2500.0f;
            pos.z = z;
            door->setPos(&pos);
            SceAtSetEnable(0xD, 0);
            SceAtSetEnable(0xF, 0);
            RsfClear(G_ROOM_ID, 5);
        }
    }
}

// Re-entry after the puzzle: the four crane Ganado are gone, the others depend on the door flags.
static void R30bEmReset()
{
    SceSleep(2);
    r30b_work.p->em[0].setEm(0x35, -1, 0, 1, 1);
    r30b_work.p->em[1].setEm(0x36, -1, 0, 1, 1);
    r30b_work.p->em[2].setEm(0x3F, -1, 0, 1, 1);
    r30b_work.p->em[3].setEm(0x43, -1, 0, 1, 1);
    r30b_work.p->em[0].destroy();
    r30b_work.p->em[1].destroy();
    r30b_work.p->em[2].destroy();
    r30b_work.p->em[3].destroy();
    SceSleep(1);
    if (RsfCheck(G_ROOM_ID, 6)) {
        r30b_work.p->em[4].setEm(0x37, -1, 0, 1, 1);
        r30b_work.p->em[5].setEm(0x38, -1, 0, 1, 1);
        r30b_work.p->em[6].setEm(0x5F, -1, 0, 1, 1);
    }
    r30b_work.p->em[7].setEm(0x5D, -1, 0, 1, 1);
    r30b_work.p->em[8].setEm(0x60, -1, 0, 1, 1);
    r30b_work.p->em[10].setEm(0x63, -1, 0, 1, 1);
    if (RsfCheck(G_ROOM_ID, 3)) {
        r30b_work.p->em[9].setEm(0x61, -1, 0, 1, 1);
    }
}

// Accelerated version of sub2's CalcMovePosDist: `vel` gains `add` towards `target` (up to `max`) or
// brakes by `add` when max is 0; 1 when it arrived (or stopped).
int CalcMovePosDistAdd2(Vec* pos, Vec* target, Vec* vel, f32 max, f32 add)
{
    f32 spd = SQRTF(vel->x * vel->x + vel->y * vel->y + vel->z * vel->z);

    if (max > 0.0f) {
        Vec dir;

        PSVECSubtract(target, pos, &dir);
#line 584 "D:/Bio4/Prog/r30b.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, add);
        PSVECAdd(vel, &dir, vel);
        spd += add;
        if (spd > max) {
            spd = max;
        }
        if (SQRTF(vel->x * vel->x + vel->y * vel->y + vel->z * vel->z) > spd) {
#line 603 "D:/Bio4/Prog/r30b.cpp"
            VECNormalize(vel, vel);
            PSVECScale(vel, vel, spd);
        }
        PSVECAdd(pos, vel, pos);
        return GetDistance(pos, target) <= max * max;
    }
    spd -= add;
    if (spd < add) {
        spd = 0.0f;
        vel->x = spd;
        vel->y = spd;
        vel->z = spd;
        return 1;
    }
#line 639 "D:/Bio4/Prog/r30b.cpp"
    VECNormalize(vel, vel);
    PSVECScale(vel, vel, spd);
    PSVECAdd(pos, vel, pos);
    return 0;
}

// Area 16: leaving the crane area ends the puzzle.
static void R30bCraneEnd()
{
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        RsfSet(G_ROOM_ID, 7);
        SceAtSetEnable(0x10, 0);
        if (pG->Room_flg[0] & 0x80000000) {
            if ((pG->Room_flg[0] & 0x40000000) == 0 && RsfCheck(G_ROOM_ID, 2) == 0) {
                pG->Room_flg[0] |= 0x40000000;
                SceExec(0x12, (TaskFunc) R30bEmGotoSet2, 0, 0, 2, 0);
                RsfSet(G_ROOM_ID, 2);
                SceAtSetEnable(4, 0);
                SceAtSetEnable(6, 1);
            }
        }
    }
}

// Area 4: the crane.
static void R30bCrane()
{
    R30bCraneWork* c = &r30b_work.p->cr;
    cObj* crane;
    cObj* magnet;
    cObj* cable;
    cObj* light;
    int i;

    memset(c, 0, sizeof(R30bCraneWork));
    int idx[4] = {0, 1, 2, 3};
    crane = r30b_work.p->crane;
    magnet = r30b_work.p->magnet;
    cable = r30b_work.p->cable;
    light = SmdGetObjPtr(0xD);
    if (crane && magnet && cable && light) {
        if (SceCkFindPL(0) == 1) {
            SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
            pG->Room_flg[0] |= 0x40000000;
            SceExec(0x12, (TaskFunc) R30bEmGotoSet, 0, 0, 2, 0);
            RsfSet(G_ROOM_ID, 2);
            SceAtSetEnable(4, 0);
            SceAtSetEnable(6, 1);
            return;
        }
        SceEventStart(0);
        SetShadowParallelDirX(0.0000001f);
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        if (SceMesGetSelection() != 1) {
            ReetShadowParallelDirX();
            SceEventEnd(0);
            SceExit();
            return;
        }
        CamCtrl.CutCall(3);
        {
            int n;

            for (n = 0; n < 4; n++) {
                if (r30b_work.p->em[idx[n]].isActive()) {
                    r30b_work.p->em[idx[n]].setNoSuspend(1);
                }
            }
        }
        c->active = 1;
        c->step = 0;
        c->nCatch = 0;
        c->vel.x = 0.0f;
        c->vel.y = 0.0f;
        c->vel.z = 0.0f;
        r30b_work.p->se = 0;
        while (c->active != 0) {
            c->pos.x = crane->pos.x;
            c->pos.y = crane->pos.y;
            c->pos.z = crane->pos.z;
            switch (c->step) {
            case 0:
                ActBtn.set(0x14, 5, 0, 0, 2, 0xF, 0, 0);
                SpfFlagOff(pG, SPF_ACTBTN);
                if (Key.trg & 0x00040000) {
                    if (r30b_work.p->se) {
                        SndStop(r30b_work.p->se, 0);
                        r30b_work.p->se = 0;
                        SndCall(6, 6, 0, 0, 0, 0);
                    }
                    c->active = 0;
                } else if (Key.trg & 0x00080000) {
                    if (r30b_work.p->se) {
                        SndStop(r30b_work.p->se, 0);
                        r30b_work.p->se = 0;
                        SndCall(6, 6, 0, 0, 0, 0);
                    }
                    MotionSetCore(crane, &crane->Motion, ROOM_ARC_PTR(pG->pRoom, 0x22), 0, 0, 1, 0);
                    MotionSetCore(cable, &cable->Motion, ROOM_ARC_PTR(pG->pRoom, 0x22), 0, 0, 1, 0);
                    c->vel.x = 0.0f;
                    c->vel.y = 0.0f;
                    c->vel.z = 0.0f;
                    SndCall(6, 7, 0, 0, 0, 0);
                    c->step++;
                } else {
                    Vec target = c->pos;
                    int moved = 0;

                    if (Key.on & 0x08000000) {
                        target.x += r30b_spd;
                        moved = 1;
                    }
                    if (Key.on & 0x04000000) {
                        target.x -= r30b_spd;
                        moved = 1;
                    }
                    if (Key.on & 0x01000000) {
                        target.z += r30b_spd;
                        moved = 1;
                    }
                    if (Key.on & 0x02000000) {
                        target.z -= r30b_spd;
                        moved = 1;
                    }
                    if (moved == 1) {
                        if (r30b_work.p->se == 0) {
                            r30b_work.p->se = SndCall(6, 5, 0, 0, 0, 0);
                        }
                        CalcMovePosDistAdd2(&c->pos, &target, &c->vel, r30b_spd, r30b_accel);
                    } else {
                        if (r30b_work.p->se) {
                            SndStop(r30b_work.p->se, 0);
                            r30b_work.p->se = 0;
                            SndCall(6, 6, 0, 0, 0, 0);
                        }
                        CalcMovePosDistAdd2(&c->pos, &target, &c->vel, 0.0f, r30b_accel);
                    }
                    if (Joy[0].on & 0x10) {
                        if (Key.on & 0x00400000) {
                            c->pos.y -= r30b_spd;
                        }
                        if (Key.on & 0x00800000) {
                            c->pos.y += r30b_spd;
                        }
                    }
                }
                break;
            case 1:
                if (MotionGetState(crane) != 0) {
                    c->step++;
                }
                break;
            case 2:
                c->pos.y -= r30b_spd;
                if (c->pos.y <= 1300.0f) {
                    c->pos.y = 1300.0f;
                    MotionSetCore(crane, &crane->Motion, ROOM_ARC_PTR(pGS->pRoom, 0x21), 0, 0, 1, 0);
                    MotionSetCore(cable, &cable->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
                    c->step++;
                }
                break;
            case 3:
                if (MotionGetState(crane) != 0) {
                    int n;
                    // A variable bound: `n < 4` is folded to `n <= 3` at the tree level (cmpwi 3 / ble); the
                    // original compares the counter with 4 (cse propagates the constant into the RTL compare).
                    int emNum = 4;

                    for (n = 0; n < emNum; n++) {
                        if (r30b_work.p->em[idx[n]].isActive()) {
                            Vec p;

                            r30b_work.p->em[idx[n]].getPos(&p);
                            if (GetDistanceXZ(&p, &c->pos) <= 250000.0f) {
                                cEm* em = r30b_work.p->em[n].getPtr();

                                if (em) {
                                    ((cEmGanado*) em)->setUFOCatch(ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
                                    SetCatchEm(n);
                                    c->catchIdx[c->nCatch] = n;
                                    c->nCatch++;
                                }
                            }
                        }
                    }
                    if (!(pG->Room_flg[0] & 0x80000000)) {
                        pG->Room_flg[0] |= 0x80000000;
                        SceExec(0x12, (TaskFunc) R30bEmSitDownSet, 0, 0, 2, 0);
                    }
                    c->step++;
                }
                break;
            case 4:
                c->pos.y += r30b_spd;
                if (c->pos.y >= 3000.0f) {
                    c->pos.y = 3000.0f;
                    c->step = 5;
                    c->cnt = 0;
                }
                break;
            case 5:
                c->pos.y = 3000.0f;
                c->pos.y += fRand1_1() * 100.0f;
                c->cnt++;
                if (c->cnt > 10) {
                    if (r30b_work.p->se == 0) {
                        r30b_work.p->se = SndCall(6, 5, 0, 0, 0, 0);
                    }
                    c->pos.y = 3000.0f;
                    c->step++;
                }
                break;
            case 6: {
                Vec target = {-16500.0f, 3000.0f, -1300.0f};

                if (CalcMovePosDist(&c->pos, &target, r30b_spd) == 1) {
                    if ((pG->Room_flg[0] & 0x40000000) == 0 && r30b_work.p->timer > 700) {
                        pG->Room_flg[0] |= 0x40000000;
                        SceExec(0x12, (TaskFunc) R30bEmGotoSet, 0, 0, 2, 0);
                    }
                    if (r30b_work.p->se) {
                        SndStop(r30b_work.p->se, 0);
                        r30b_work.p->se = 0;
                        SndCall(6, 6, 0, 0, 0, 0);
                    }
                    MotionSetCore(crane, &crane->Motion, ROOM_ARC_PTR(pG->pRoom, 0x22), 0, 0, 1, 0);
                    MotionSetCore(cable, &cable->Motion, ROOM_ARC_PTR(pG->pRoom, 0x22), 0, 0, 1, 0);
                    SndCall(6, 8, 0, 0, 0, 0);
                    c->cnt = 0;
                    c->step++;
                }
                break;
            }
            case 7:
                if (c->nCatch > 0) {
                    c->cnt++;
                    if (c->cnt == 10) {
                        c->cnt = 0;
                        r30b_work.p->em[c->catchIdx[c->nCatch - 1]].setFlag(1);
                        c->nCatch--;
                    }
                }
                if (MotionGetState(crane) != 0) {
                    c->nCatch = 0;
                    c->cnt = 0;
                    c->step++;
                }
                break;
            case 8:
                c->cnt++;
                if (c->cnt > 30) {
                    MotionSetCore(crane, &crane->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
                    MotionSetCore(cable, &cable->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
                    c->step++;
                }
                break;
            case 9: {
                Vec target = {-14000.0f, 3000.0f, -5000.0f};

                if (CalcMovePosDist(&c->pos, &target, r30b_spd) == 1) {
                    c->cnt = 0;
                    c->step++;
                }
                break;
            }
            case 10:
                c->pos.x = -14000.0f;
                c->pos.z = -5000.0f;
                c->pos.x += fRand1_1() * 10.0f;
                c->pos.z += fRand1_1() * 10.0f;
                c->cnt++;
                if (c->cnt > 10) {
                    if (MotionGetState(crane) != 0) {
                        // The zero stored to `active` is its own register (r30, set before the tryLeft update
                        // and kept over the CkCatchEm calls), not the hoisted shared zero of the other stores.
                        int off = 0;

                        c->pos.x = -14000.0f;
                        c->pos.z = -5000.0f;
                        r30b_work.p->tryLeft--;
                        if ((pGS->Room_flg[0] & 0x40000000) || (CkCatchEm(0) && CkCatchEm(1) && CkCatchEm(2) && CkCatchEm(3))) {
                            RsfSet(G_ROOM_ID, 2);
                            c->active = off;
                            SceAtSetEnable(4, 0);
                            SceAtSetEnable(6, 1);
                            if (CkCatchEm(0) == 0) {
                                r30b_work.p->patrol[0].EndControl();
                            }
                            if (CkCatchEm(1) == 0) {
                                r30b_work.p->patrol[1].EndControl();
                            }
                            if (CkCatchEm(2) == 0) {
                                r30b_work.p->patrol[2].EndControl();
                            }
                            if (CkCatchEm(3) == 0) {
                                r30b_work.p->patrol[3].EndControl();
                            }
                        } else {
                            c->step = 0;
                        }
                    }
                }
                break;
            }
            if (c->pos.x >= -6200.0f) {
                c->vel.x = 0.0f;
            }
            if (c->pos.x <= -16500.0f) {
                c->vel.x = 0.0f;
            }
            if (c->pos.z >= 1400.0f) {
                c->vel.z = 0.0f;
            }
            if (c->pos.z <= -5400.0f) {
                c->vel.z = 0.0f;
            }
            if (c->pos.x >= -6200.0f) {
                c->pos.x = -6200.0f;
            }
            if (c->pos.x <= -16500.0f) {
                c->pos.x = -16500.0f;
            }
            if (c->pos.z >= 1400.0f) {
                c->pos.z = 1400.0f;
            }
            if (c->pos.z <= -5400.0f) {
                c->pos.z = -5400.0f;
            }
            if (c->pos.y >= 3000.0f) {
                c->pos.y = 3000.0f;
            }
            if (c->pos.y <= 1300.0f) {
                c->pos.y = 1300.0f;
            }
            crane->setPos(&c->pos);
            magnet->setPos(&c->pos);
            cable->setPos(&c->pos);
            r30b_setObjPos(cable, cable->pos.x, cable->pos.y + r30b_cableOfs, cable->pos.z);
            r30b_setObjPos(light, light->pos.x, light->pos.y, c->pos.z);
            if (c->nCatch > 0) {
                for (i = 0; i < c->nCatch; i++) {
                    if (c->nCatch == 1) {
                        Vec p = {0.0f, 0.0f, 0.0f};
                        Vec ofs = {0.0f, -4000.0f, 0.0f};

                        p.x = c->pos.x + ofs.x;
                        p.y = c->pos.y + ofs.y;
                        p.z = c->pos.z + ofs.z;
                        if (p.y < -2000.0f) {
                            p.y = -2000.0f;
                        }
                        r30b_work.p->em[c->catchIdx[i]].setPos(&p);
                    } else {
                        Vec p = {0.0f, 0.0f, 0.0f};
                        Vec ang = {0.0f, 0.0f, 0.0f};
                        Vec ofsTbl[4] = {{0.0f, -4000.0f, 200.0f}, {0.0f, -4000.0f, -200.0f}, {-200.0f, -4000.0f, 0.0f}, {200.0f, -4000.0f, 0.0f}};
                        Vec angTbl[4] = {{0.0f, 0.0f, 0.0f}, {0.0f, 3.1415927f, 0.0f}, {0.0f, 1.5707964f, 0.0f}, {0.0f, -1.5707964f, 0.0f}};

                        p.x = c->pos.x + ofsTbl[i].x;
                        p.y = c->pos.y + ofsTbl[i].y;
                        p.z = c->pos.z + ofsTbl[i].z;
                        ang.x = angTbl[i].x;
                        ang.y = angTbl[i].y;
                        ang.z = angTbl[i].z;
                        if (p.y < -2000.0f) {
                            p.y = -2000.0f;
                        }
                        r30b_work.p->em[c->catchIdx[i]].setPos(&p);
                        r30b_work.p->em[c->catchIdx[i]].setAng(&ang);
                    }
                }
            }
            {
                JOY* joy = &Joy[0];

                if (joy->on & 0x10) {
                    Vec from;
                    Vec a;
                    Vec b;

                    from.x = crane->pos.x;
                    from.y = -10000.0f;
                    from.z = crane->pos.z;
                    a = crane->pos;
                    b = from;
                    EspDrawLaserLine2(&a, &b, 0xFF, 0, 0, 0x80);
                }
            }
            if (pG->Room_flg[0] & 0x80000000) {
                r30b_work.p->timer++;
            }
            eprintf(0x40, 0x20, 0, 0, "TRY:[%2d:%2d] Timer:[%d/%d]", r30b_work.p->tryLeft, 3, r30b_work.p->timer, 700);
            eprintf(0x40, 0x10, 0, 0, "CranePos:[%f, %f, %f]", c->pos.x, c->pos.y, c->pos.z);
            SceSleep(1);
        }
        {
            int n;

            for (n = 0; n < 4; n++) {
                if (r30b_work.p->em[idx[n]].isActive()) {
                    r30b_work.p->em[idx[n]].setNoSuspend(0);
                }
            }
        }
        ReetShadowParallelDirX();
        SceEventEnd(0);
        SceExit();
    }
}

// Area 3: the s00 event, then the jump to 3-10.
static void R30bEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(3, 0);
        SceDestroyEm(0x10, 0x20);
        SceSleep(2);
        EvtMgr.EvtReadExec("event/evd/r30bs00.evd", (u8) GetEmIdFromListI(0x56), 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        Vec pos = {-6200.0f, 0.0f, -26100.0f};
        Vec rot;
        r30b_memset(&rot, 0, sizeof(Vec));
        SceAtExecRoomJump(0x310, &pos, &rot, 0);
    }
}

// Event r30bs00: the fade at cut 5.
void Evt_R30BS00_Func(Event* e)
{
    if (e->funcMode == 1) {
        switch (e->NowCut) {
        case 0:
            break;
        case 5:
            if (e->NowFrame == 25) {
                FadeSetW(2, 40, 0, 0);
            }
            break;
        }
    }
}

// The two patrols of the crane Ganado.
void R30bEmWanderingSet()
{
    r30b_work.p->patrol[0].SetPatrol(0x35, r30b_patrolTbl0, 2, 2, 0);
    r30b_work.p->patrol[2].SetPatrol(0x3F, r30b_patrolTbl1, 2, 2, 0);
}

// The uncaught Ganado sit down at their posts.
static void R30bEmSitDownSet()
{
    int i;

    if (CkCatchEm(1) == 0) {
        r30b_work.p->exec[1].SetRouteExec(0x36, r30b_sitTbl1, 2, 2, 1);
    }
    if (CkCatchEm(1) != 0) {
        if (CkCatchEm(2) == 0) {
            r30b_work.p->patrol[2].EndControl();
        }
        if (CkCatchEm(2) == 0) {
            r30b_work.p->exec[2].SetRouteExec(0x3F, r30b_sitTbl2, 2, 2, 1);
        }
    } else {
        if (CkCatchEm(2) == 0) {
            r30b_work.p->patrol[2].EndControl();
        }
        if (CkCatchEm(2) == 0) {
            r30b_work.p->exec[2].SetRouteExec(0x3F, r30b_sitTbl2b, 1, 2, 1);
        }
    }
    for (i = 0; i < 10; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(0) == 0) {
        r30b_work.p->patrol[0].EndControl();
    }
    if (CkCatchEm(0) == 0) {
        r30b_work.p->exec[0].SetRouteExec(0x35, r30b_sitTbl0, 1, 2, 1);
    }
    for (i = 0; i < 10; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(3) == 0) {
        r30b_work.p->exec[3].SetRouteExec(0x43, r30b_sitTbl3, 1, 2, 1);
    }
}

// The uncaught Ganado run at the player (through the pit route).
static void R30bEmGotoSet()
{
    int i;

    if (CkCatchEm(0) == 0) {
        r30b_work.p->exec[0].EndControl();
    }
    if (CkCatchEm(0) == 0) {
        r30b_work.p->run[0].SetRouteRun(0x35, r30b_gotoTbl, 3, 2, 1);
    }
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(1) == 0) {
        r30b_work.p->exec[1].EndControl();
    }
    if (CkCatchEm(1) == 0) {
        r30b_work.p->run[1].SetRouteRun(0x36, r30b_gotoTbl, 3, 2, 1);
    }
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(3) == 0) {
        r30b_work.p->exec[3].EndControl();
    }
    if (CkCatchEm(3) == 0) {
        r30b_work.p->run[3].SetRouteRun(0x43, r30b_gotoTbl, 3, 2, 1);
    }
    for (i = 0; i < 10; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(2) == 0) {
        r30b_work.p->exec[2].EndControl();
    }
    if (CkCatchEm(2) == 0) {
        r30b_work.p->run[2].SetRouteRun(0x3F, r30b_gotoTbl, 3, 2, 1);
    }
}

// The same straight at the player.
static void R30bEmGotoSet2()
{
    int i;

    if (CkCatchEm(0) == 0) {
        r30b_work.p->exec[0].EndControl();
    }
    if (CkCatchEm(0) == 0) {
        r30b_work.p->run[0].SetRouteRun(0x35, r30b_gotoTbl2, 1, 2, 1);
    }
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(1) == 0) {
        r30b_work.p->exec[1].EndControl();
    }
    if (CkCatchEm(1) == 0) {
        r30b_work.p->run[1].SetRouteRun(0x36, r30b_gotoTbl2, 1, 2, 1);
    }
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(3) == 0) {
        r30b_work.p->exec[3].EndControl();
    }
    if (CkCatchEm(3) == 0) {
        r30b_work.p->run[3].SetRouteRun(0x43, r30b_gotoTbl2, 1, 2, 1);
    }
    for (i = 0; i < 10; i++) {
        SceSleep(1);
    }
    if (CkCatchEm(2) == 0) {
        r30b_work.p->exec[2].EndControl();
    }
    if (CkCatchEm(2) == 0) {
        r30b_work.p->run[2].SetRouteRun(0x3F, r30b_gotoTbl2, 1, 2, 1);
    }
}

// The caught flags of the four crane Ganado (pG->flags_174 bits 2..5).
void SetCatchEm(int no)
{
    if (no == 0) {
        pG->Room_flg[0] |= 0x20000000;
    }
    if (no == 1) {
        pG->Room_flg[0] |= 0x10000000;
    }
    if (no == 2) {
        pG->Room_flg[0] |= 0x08000000;
    }
    if (no == 3) {
        pG->Room_flg[0] |= 0x04000000;
    }
}

// 1 when crane Ganado `no` (0..3) has been caught (Room_flg[0] bits 0x20000000 >> no).
int CkCatchEm(int no)
{
    if (no == 0 && (pG->Room_flg[0] & 0x20000000)) {
        return 1;
    }
    if (no == 1 && (pG->Room_flg[0] & 0x10000000)) {
        return 1;
    }
    if (no == 2 && (pG->Room_flg[0] & 0x08000000)) {
        return 1;
    }
    if (no == 3 && (pG->Room_flg[0] & 0x04000000)) {
        return 1;
    }
    return 0;
}

// The room stream plays while a Ganado has found the player.
static void SceBgmCheck()
{
    int on = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}
