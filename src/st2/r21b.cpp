#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "objTrolley.h"
#include "em.h"
#include "emhit.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"

// Room 2-1B (D:/Bio4/Prog/r21b.cpp): the mine cart ride; the Ganados jump down onto the cart at
// the points the tables below list (enemy list entry, work slot, cart motion frame).

struct R21bEmSet {
    int slot;    // work slot (-1 ends the table)
    int no;      // enemy list entry
    u32 frame;   // cart motion frame the enemy jumps at (HalfWay: 1 once the enemy was set)
};

struct R21bEm {
    cEmWrap em;
    int set;     // 0xC  1 once the enemy jumped down
};

struct R21bWork {
    cObjTrolley* trolley;   // 0x000
    R21bEm em[32];          // 0x004
    cEmHit* hit[2];         // 0x204  the two switches
    u32 se;                 // 0x20C  RoomSeCall handle of the half-way switch
    cSat* sat;              // 0x210
};

// One-member struct: every store through the work reloads the pointer.
struct R21bWorkPtr {
    R21bWork* p;
};

static R21bWorkPtr r21b_work;

// COMPILER-DIFF 4: the table's `int` entry is passed to the s16 parameter untruncated.
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

static R21bEmSet r21b_emTbl0[] = {{0, 0, 0xAE}, {1, 1, 0xA6}, {0x1C, 0x3D, 0x6D}, {-1, -1, 0}};
static R21bEmSet r21b_emTbl1[] = {{2, 2, 0x16E}, {3, 3, 0x164}, {0x1D, 0x3E, 0x12C}, {-1, -1, 0}};
static R21bEmSet r21b_emTbl2[] = {{4, 4, 0x439}, {5, 5, 0x43B}, {6, 6, 0x436}, {0x1E, 0x3F, 0x3F3}, {0x1F, 0x40, 0x41D}, {-1, -1, 0}};
static R21bEmSet r21b_emTbl3[] = {{7, 7, 0x787}, {8, 8, 0x7A3}, {9, 9, 0x787}, {0xA, 0xA, 0x7A3}, {-1, -1, 0}};
static R21bEmSet r21b_emTbl4[] = {{0xB, 0xB, 0x272}, {0xC, 0xC, 0x294}, {0xD, 0xD, 0x28C}, {-1, -1, 0}};
static R21bEmSet r21b_emTbl5[] = {{0xE, 0xE, 0x440}, {0xF, 0xF, 0x440}, {0x10, 0x10, 0x448}, {0x11, 0x11, 0x44C}, {-1, -1, 0}};
static R21bEmSet r21b_emTbl6[] = {{0x12, 0x20, 0x80A}, {0x13, 0x21, 0x801}, {0x14, 0x22, 0x813}, {0x15, 0x23, 0x81C}, {-1, -1, 0}};
static R21bEmSet* r21b_emTbl[] = {r21b_emTbl0, r21b_emTbl1, r21b_emTbl2, r21b_emTbl3, r21b_emTbl4, r21b_emTbl5, r21b_emTbl6};

static void r21b_GanadoJumpDownCheck(int no);
static void r21b_HalfWayGanadoSet();
static void r21b_SwitchMove(int no);
static void r21b_GetDragonBall();
static void r21b_GetDragonBallEndProc();
static void r21b_DoorOpen();
static void r21b_StrPlay();
static void r21b_HalfWaySwitchMove();
static void r21b_HalfWaySwitchMoveEndProc();

// Room init (the mine cart ride): the trolley object with its nine motions, the Ganado tables, the two
// switch hit boxes (start / half-way), the collision piece riding along; the dragon-ball item (0x80)
// event and the door once it was taken (Room_flg bit 0); the ride stream watcher.
void R21bInit()
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    R21bWork*& wp = r21b_work.p;   // the store's `lis` sits before the mem_calloc call
    Vec rot = {0.0f, 0.0f, 0.0f};

#line 46 "D:/Bio4/Prog/r21b.cpp"
    wp = (R21bWork*) MEM_CALLOC(sizeof(R21bWork), 1, 0xd);
    wp->trolley = (cObjTrolley*) SetTrolley(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);
    if (wp->trolley) {
        void* mot[9];

        mot[0] = ROOM_ARC_PTR(pG->pRoom, 0x21);
        mot[1] = ROOM_ARC_PTR(pG->pRoom, 0x22);
        mot[2] = ROOM_ARC_PTR(pG->pRoom, 0x23);
        mot[3] = ROOM_ARC_PTR(pG->pRoom, 0x24);
        mot[4] = ROOM_ARC_PTR(pG->pRoom, 0x25);
        mot[5] = ROOM_ARC_PTR(pG->pRoom, 0x26);
        mot[6] = ROOM_ARC_PTR(pG->pRoom, 0x27);
        mot[7] = ROOM_ARC_PTR(pG->pRoom, 0x28);
        mot[8] = ROOM_ARC_PTR(pG->pRoom, 0x29);
        wp->trolley->setMotion(mot);
    }
    r21b_work.p->hit[0] = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &pos, 0, 1);
    r21b_work.p->hit[0]->setParent(SmdGetObjPtr(0xC8), 0, 0);
    YarareInit(r21b_work.p->hit[0], 0.0f, 0.0f, 0.0f, 400.0f, 0.0f, 0, 1);
    if (RsfCheck(G_ROOM_ID, 0)) {
        SmdGetObjPtr(0xB9)->be_flag &= ~2;
        SceAtSetEnable(2, 0);
        SmdGetObjPtr(0xAF)->be_flag &= ~2;
        SceAtSetEnable(3, 0);
    } else {
        SceAtDataSet_exec(0x80, SCE_LEVEL10, 0, (TaskFunc) r21b_GetDragonBall, 0, 1);
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r21b_DoorOpen, 0, 1);
        SceExec(0x12, (TaskFunc) r21b_StrPlay, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r21b_HalfWaySwitchMove, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r21b_GanadoJumpDownCheck, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r21b_GanadoJumpDownCheck, 1, 0, SCE_PRIO_DEF_2, 0);
    }
    PlRegistMotion(0, 0, 0, 0, 0, 0, 0, 0, ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B),
                   ROOM_ARC_PTR(pG->pRoom, 0x2C), ROOM_ARC_PTR(pG->pRoom, 0x2D));
    r21b_work.p->sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &pos, 7);
}

// Per frame: the start switch hit by anything but a knife / grenade weapon type (0xD/0xE/0x12/0x13)
// starts the cart; at the half-way stop the second switch likewise restarts it.
void R21bMain()
{
    if (r21b_work.p->hit[0]->ckStatus() == 1) {
        switch (r21b_work.p->hit[0]->dmg.m_Wep) {
        case 0xD:
        case 0xE:
        case 0x12:
        case 0x13:
            break;
        default:
            SceExec(0x12, (TaskFunc) r21b_SwitchMove, 0, 0, SCE_PRIO_DEF_2, 0);
            break;
        }
    }
    if (r21b_work.p->trolley->ckStop() == 1) {
        if (r21b_work.p->hit[1] && r21b_work.p->hit[1]->ckStatus() == 1) {
            switch (r21b_work.p->hit[1]->dmg.m_Wep) {
            case 0xD:
            case 0xE:
            case 0x12:
            case 0x13:
                break;
            default:
                SceExec(0x12, (TaskFunc) r21b_SwitchMove, 1, 0, SCE_PRIO_DEF_2, 0);
                break;
            }
        }
    }
}

// Sets the enemies of table `no` and drops each one onto the cart at its motion frame.
static void r21b_GanadoJumpDownCheck(int no)
{
    R21bEmSet* p;
    int wait;

    for (p = r21b_emTbl[no]; p->slot != -1; p++) {
        cEmWrapSetEmI(&r21b_work.p->em[p->slot].em, p->no, 5, 1, 1, 0);
    }
    do {
        wait = 0;
        for (p = r21b_emTbl[no]; p->slot != -1; p++) {
            R21bEm* e = &r21b_work.p->em[p->slot];

            if (e->set == 0) {
                if (r21b_work.p->trolley->motFrame == (f32) p->frame) {
                    e->em.setFlag(1);
                    e->set = 1;
                } else {
                    wait = 1;
                }
            }
        }
        SceSleep(1);
    } while (wait);
    if (no >= 1 && no <= 5) {
        if (no == 3) {
            SceExec(0x12, (TaskFunc) r21b_HalfWayGanadoSet, 0, 0, SCE_PRIO_DEF_2, 0);
        } else {
            SceExec(0x12, (TaskFunc) r21b_GanadoJumpDownCheck, no + 1, 0, SCE_PRIO_DEF_2, 0);
        }
    }
}

// The half-way stop: enemies come as long as few are alive, then the ride goes on.
static void r21b_HalfWayGanadoSet()
{
    R21bEmSet tbl[7] = {{0x16, 0x36, 0}, {0x17, 0x37, 0}, {0x18, 0x38, 0}, {0x19, 0x39, 0}, {0x1A, 0x3A, 0}, {0x1B, 0x3B, 0}, {-1, -1, 0}};
    Vec pos = {0.0f, 0.0f, 0.0f};
    u32 cnt = 0;
    R21bEmSet* p;

    while (r21b_work.p->trolley->ckStop() == 0) {
        SceSleep(1);
    }
    r21b_work.p->hit[1] = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &pos, 0, 1);
    r21b_work.p->hit[1]->setParent(SmdGetObjPtr(0xC9), 0, 0);
    YarareInit(r21b_work.p->hit[1], 0.0f, 0.0f, 0.0f, 400.0f, 0.0f, 0, 1);
    while (r21b_work.p->trolley->ckStop() == 1) {
        if (cnt <= 12 && (u32) SceCountEmAlive(0x10, 0x20) <= 5) {
            for (p = tbl; p->slot != -1; p++) {
                if (p->frame == 0) {
                    if (Rnd() & 1) {
                        cEmWrapSetEmI(&r21b_work.p->em[p->slot].em, p->no, 5, 1, 1, 0);
                        cnt++;
                        r21b_work.p->em[p->slot].em.setFlag(1);
                        p->frame = 1;
                        break;
                    }
                } else if (r21b_work.p->em[p->slot].em.ckResetEnable() == 1) {
                    cnt++;
                    r21b_work.p->em[p->slot].em.setReset();
                    r21b_work.p->em[p->slot].em.setFlag(1);
                    break;
                }
            }
        }
        SceSleep(1);
    }
    SceExec(0x12, (TaskFunc) r21b_GanadoJumpDownCheck, 4, 0, SCE_PRIO_DEF_2, 0);
}

// The switch lever (no 0: the first stop, 1: the half-way stop) starts the cart.
static void r21b_SwitchMove(int no)
{
    const f32 step = 0.07853982f;
    cObj* obj;

    if (no == 0) {
        if (pG->Room_flg[0] & 0x40000000) {
            SceExit();
        }
        pG->Room_flg[0] |= 0x40000000;
        obj = SmdGetObjPtr(0xC8);
        if (r21b_work.p->sat) {
            r21b_work.p->sat->m_Flag &= ~4;
        }
    } else {
        if (pG->Room_flg[0] & 0x20000000) {
            SceExit();
        }
        pG->Room_flg[0] |= 0x20000000;
        obj = SmdGetObjPtr(0xC9);
    }
    EstSet(0, -1, &obj->pos, 0, 1, 0x12, 0, 0, 0, 0);
    if (obj) {
        obj->be_flag |= 0x20;
        RoomSeCall(4, &obj->pos, 0, 0, 0);
        while (obj->ang.x <= 1.5707964f) {
            obj->ang.x += step;
            SceSleep(1);
        }
        obj->ang.x = 1.5707964f;
        SceSleep(2);
        obj->be_flag &= ~0x20;
    }
    if (no == 0) {
        if (SceAtHitCheck(6) == 0) {
            SceSleep(1);
        }
        r21b_work.p->trolley->setStart();
    } else {
        r21b_work.p->trolley->set2ndStart();
    }
}

// Item area 0x80 (the dragon ball): once taken, Room_flg bit 0 and camera cut 1 while the wall object
// 0xB9 slides open (-50 x a frame to -203450) with SE; player-cancellable.
static void r21b_GetDragonBall()
{
    cObj* obj = SmdGetObjPtr(0xB9);

    SceAtDataReset(0x80);
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 0);
    SceEventStart(1);
    obj->be_flag |= 0x20;
    SceSetEventCancel(1, (TaskFunc) r21b_GetDragonBallEndProc, 0, 0, 1);
    CamCtrl.CutCall(1);
    RoomSeCall(5, &obj->pos, 0, 0, 0);
    while (obj->pos.x >= -203450.0f) {
        obj->pos.x -= 50.0f;
        SceSleep(1);
    }
    obj->pos.x = -203450.0f;
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r21b_GetDragonBallEndProc();
}

// End of the wall slide (also its cancel path): the wall shown at rest, area 2 off, camera back, SceEventEnd.
static void r21b_GetDragonBallEndProc()
{
    cObj* obj = SmdGetObjPtr(0xB9);

    obj->be_flag &= ~2;
    obj->be_flag &= ~0x20;
    SceAtSetEnable(2, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The gate object 0xAF slides open (-60 x a frame to -203800) with SE; areas 4 then 3 off.
static void r21b_DoorOpen()
{
    cObj* obj = SmdGetObjPtr(0xAF);

    SceAtSetEnable(4, 0);
    obj->be_flag |= 0x20;
    RoomSeCall(6, 0, 0, 0, 0);
    while (obj->pos.x > -203800.0f) {
        obj->pos.x -= 60.0f;
        SceSleep(1);
    }
    obj->pos.x = -203800.0f;
    SceAtSetEnable(3, 0);
}

// The ride stream starts once the cart passes frame 80 and the player reaches area 5.
static void r21b_StrPlay()
{
    while (r21b_work.p->trolley->motFrame != 80.0f) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 0, 1);
    while (SceAtHitCheck(5) == 0) {
        SceSleep(1);
    }
}

// The half-way switch is thrown by a Ganado once the cart reaches frame 1878.
static void r21b_HalfWaySwitchMove()
{
    const f32 step = 0.07853982f;
    cObj* obj = SmdGetObjPtr(0xC9);
    cEmWrap em;

    obj->ang.x = 1.5707964f;
    obj->be_flag |= 0x20;
    while (r21b_work.p->trolley->motFrame != 1878.0f) {
        SceSleep(1);
    }
    SceEventStart(1);
    SndSePause(1, -1);
    if (em.setEm(0x13, 5, 1, 1, 0) == 1) {
        em.setNoSuspend(1);
    }
    CamCtrl.CutCall(2);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r21b_HalfWaySwitchMoveEndProc, 0, 0, 1);
    SceSleep(5);
    r21b_work.p->se = RoomSeCall(4, &obj->pos, 0, 0, 0);
    while (obj->ang.x > 0.0f) {
        obj->ang.x -= step;
        SceSleep(1);
    }
    obj->ang.x = 0.0f;
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r21b_HalfWaySwitchMoveEndProc();
}

// End of the half-way switch cutscene (also its cancel path): SE stopped, the lever 0xC9 levelled,
// camera back, SEs unpaused, SceEventEnd.
static void r21b_HalfWaySwitchMoveEndProc()
{
    cObj* obj = SmdGetObjPtr(0xC9);

    if (pG->Room_flg[0] & 0x80000000) {
        if (r21b_work.p->se) {
            SndStop(r21b_work.p->se, 0);
        }
    }
    obj->ang.x = 0.0f;
    CamCtrl.Comeback(0);
    SndSePause(0, -1);
    SceEventEnd(0);
}
