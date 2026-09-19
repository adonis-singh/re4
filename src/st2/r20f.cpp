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
#include "em.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "stage.h"
#include "read.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "math_sub.h"

// Room 2-0F (D:/Bio4/Prog/r20f.cpp): the lever switch that raises the platform, the patrol /
// guard Ganados and the two reset waves.

struct R20fWork {
    u32 x0;                 // 0x000
    cEmPatrol patrol[3];    // 0x004
    cEmGuard guard[3];      // 0x358
    cEmWrap em[14];         // 0x700
    int pad_7A8[9];         // 0x7A8
    cSat* sat[3];           // 0x7CC
    cSat* eat[3];           // 0x7D8
};

// The work pointer is a struct member: every store through the work reloads it.
struct R20fWorkPtr {
    R20fWork* p;
};

static R20fWorkPtr r20f_work;

// Stores into the work the original keeps below the following `pG` load (scalar-reference stores).
static inline void PSet(cSat*& d, cSat* v) { d = v; }

static Vec r20f_guardPos0 = {54000.0f, 3500.0f, 24000.0f};
static Vec r20f_guardPos1 = {54500.0f, 3500.0f, 26000.0f};
static Vec r20f_guardPos2 = {51700.0f, 3500.0f, 25800.0f};
static Vec r20f_patrolTbl0[4] = {
    {58000.0f, 0.0f, 13000.0f},
    {58000.0f, 0.0f, 16300.0f},
    {66000.0f, 0.0f, 16300.0f},
    {66000.0f, 0.0f, 13000.0f},
};
static Vec r20f_patrolTbl1[2] = {
    {74000.0f, 3600.0f, 21300.0f},
    {74000.0f, 2000.0f, 14000.0f},
};
static Vec r20f_patrolTbl2[4] = {
    {65700.0f, 0.0f, 22700.0f},
    {71700.0f, 0.0f, 22700.0f},
    {65700.0f, 0.0f, 22700.0f},
    {59000.0f, 0.0f, 22700.0f},
};

void R20fSwitchMove(cObj* obj, int dir);
static void R20fSwitchMain();
static void R20fSwitchEnd(int evt);
static void SceBgmCheck();
static void R20fEmSetMain();
void R20fEmResetA0();
static int R20fCkEmGuard(cEmWrap* em);
void R20fEmResetB0();
void R20fEmResetB1();
static void R20fEmWanderingSet();

// Room init: three collision pieces (archive 5 sets 1..3) and two attribute pieces for the platform
// levels; ESL 0xF stays dead when its list-4 death bit says so; door 0xF gets lock models; the lever
// (objects 0x18/0x19) and platform posed by Room_flg bit 1 (already raised: waves task, else area 5 =
// the lever); the patrol / guard Ganados; the battle-stream task.
void R20fInit()
{
    u32 i;
    cObj* obj0;
    cObj* obj1;

    R20fWork*& wp = r20f_work.p;
#line 45 "D:/Bio4/Prog/r20f.cpp"
    wp = (R20fWork*) MEM_CALLOC(sizeof(R20fWork), 1, 0xd);
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cEm* door;
    for (i = 0; i < 3; i++) {
        r20f_work.p->sat[i] = NULL;
        r20f_work.p->eat[i] = NULL;
    }
    PSet(r20f_work.p->sat[0], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1));
    PSet(r20f_work.p->sat[1], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2));
    PSet(r20f_work.p->sat[2], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3));
    PSet(r20f_work.p->eat[1], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 2));
    PSet(r20f_work.p->eat[2], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 1));
    if (checkEmListNo(G_ROOM_ID) == 4 && (pG->Em_flg[3][6] & 0x10)) {
        EmListSetAlive(0xF, 0);
    }
    getRoomEtcDoor(0xF, &door, 1);
    if (door) {
        ((cEmDoor*) door)->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0);
    }
    obj0 = SmdGetObjPtr(0x18);
    obj1 = SmdGetObjPtr(0x19);
    if (obj0 && obj1) {
        if (RsfCheck(G_ROOM_ID, 1)) {
            R20fSwitchEnd(0);
        } else {
            Vec v;
            Vec* pv = &v;
            {
                f32 x = obj0->pos.x;
                f32 z = obj0->pos.z;

                v.x = x;
                pv->y = 0.0f;
                v.z = z;
                obj0->setPos(pv);
            }
            {
                f32 x = obj1->ang.x;
                f32 y = obj1->ang.y;
                f32 z = obj1->ang.z;

                v.x = x;
                v.y = y;
                v.z = z;
                obj1->setAng(pv);
            }
            if (r20f_work.p->sat[0]) {
                r20f_work.p->sat[0]->m_Flag |= 4;
            }
            if (r20f_work.p->sat[1]) {
                r20f_work.p->sat[1]->m_Flag &= ~4;
            }
            if (r20f_work.p->sat[2]) {
                r20f_work.p->sat[2]->m_Flag &= ~4;
            }
            if (r20f_work.p->eat[1]) {
                r20f_work.p->eat[1]->m_Flag &= ~4;
            }
            if (r20f_work.p->eat[2]) {
                r20f_work.p->eat[2]->m_Flag &= ~4;
            }
            SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) R20fSwitchMain, 0, 1);
        }
        obj0->matUpdate();
        obj1->matUpdate();
    }
    if (checkEmListNo(G_ROOM_ID) == 3 && StaFlagChk(pG, STA_SUB_ASHLEY) == 0) {
        int id = GetEmIdFromList(0xED);

        EmReadSearch((u8) id, 0, 0);
        SceExec(0x12, (TaskFunc) R20fEmSetMain, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    {
        u32 flags = pG->Status_flg[3];

        if (flags & 0x04000000) {
            SmdSetTrans(0x10, 0);
            SceAtSetEnable(8, 0);
        }
    }
    SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per-frame room main: nothing.
void R20fMain()
{
}

// Swings the lever (dir 1: up to 90 degrees, else back to 0).
void R20fSwitchMove(cObj* obj, int dir)
{
    if (obj) {
        int end = 0;

        SndCall(6, 0, &obj->pos, 0, 0, 0);
        CamCtrl.CutCall(7);
        for (;;) {
            if (end == 0) {
                if (dir == 1) {
                    obj->ang.z += 0.06981317f;
                } else {
                    obj->ang.z -= 0.06981317f;
                }
                obj->matUpdate();
                if (dir == 1) {
                    if (obj->ang.z >= 1.5707964f) {
                        obj->ang.z = 1.5707964f;
                        return;
                    }
                } else {
                    if (obj->ang.z <= 0.0f) {
                        obj->ang.z = 0.0f;
                        return;
                    }
                }
            }
            SceSleep(1);
        }
    }
}

// Area 5: the lever is pulled, the platform rises over 70 frames.
static void R20fSwitchMain()
{
    f32 y = 0.0f;
    cObj* obj0 = SmdGetObjPtr(0x18);
    cObj* obj1 = SmdGetObjPtr(0x19);

    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        SceAtSetEnable(5, 0);
        SceEventStart(1);
        SceSetEventCancel(1, (TaskFunc) R20fSwitchEnd, 1, -1, 1);
        if (obj1) {
            R20fSwitchMove(obj1, 1);
        }
        SceSleep(20);
        CamCtrl.CutCall(6);
        EstSet(0, -1, 0, 0, 1, 0, 1, 2, 0, 0);
        SceSleep(70);
        if (obj0) {
            int i;
            Vec pos;
            f32 base = y;

            SndCall(6, 1, &obj0->pos, 0, 0, 0);
            for (i = 0; i < 70; i++) {
                f32 x = obj0->pos.x;
                f32 z = obj0->pos.z;

                pos.x = x;
                pos.z = z;
                y = (f32) i * 3500.0f / 70.0f + base;
                pos.y = y;
                obj0->setPos(&pos);
                SceSleep(1);
            }
            SndCall(6, 2, &obj0->pos, 0, 0, 0);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R20fSwitchEnd(1);
    }
}

// End of the lever event (evt: cancelled): drop the dust effect, snap the platform 0x18 to y 3500 and
// the lever 0x19 to 90 degrees, switch the collision pieces to the raised layout, Room_flg bit 1.
static void R20fSwitchEnd(int evt)
{
    cObj* obj0 = SmdGetObjPtr(0x18);
    cObj* obj1 = SmdGetObjPtr(0x19);

    EffectEspDelete(1, 2, 0, 0);
    EffectEspgenDelete(1, 2, 0);
    EffectEfmDelete(1, 2, 0);
    if (obj0) {
        Vec pos;
        f32 x = obj0->pos.x;
        f32 z = obj0->pos.z;

        pos.x = x;
        pos.y = 3500.0f;
        pos.z = z;
        obj0->setPos(&pos);
    }
    if (obj1) {
        Vec rot;
        f32 x = obj1->ang.x;
        f32 y = obj1->ang.y;

        rot.x = x;
        rot.y = y;
        rot.z = 1.5707964f;
        obj1->setAng(&rot);
    }
    if (r20f_work.p->sat[0]) {
        r20f_work.p->sat[0]->m_Flag &= ~4;
    }
    if (r20f_work.p->sat[1]) {
        r20f_work.p->sat[1]->m_Flag |= 4;
    }
    if (r20f_work.p->sat[2]) {
        r20f_work.p->sat[2]->m_Flag |= 4;
    }
    if (r20f_work.p->eat[1]) {
        r20f_work.p->eat[1]->m_Flag |= 4;
    }
    if (r20f_work.p->eat[2]) {
        r20f_work.p->eat[2]->m_Flag |= 4;
    }
    if (evt) {
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        SceExit();
    }
}

// The battle stream while a Ganado has found the player.
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

// Watches the areas once the platform is up: the reset waves.
static void R20fEmSetMain()
{
    SceSleep(1);
    R20fEmWanderingSet();
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 1)) {
            if (RsfCheck(G_ROOM_ID, 2) == 0 && (pG->Room_flg[2] & 0x40000000)) {
                RsfSet(G_ROOM_ID, 2);
                R20fEmResetA0();
            }
            if (RsfCheck(G_ROOM_ID, 3) == 0 && (pG->Room_flg[2] & 0x80000000)) {
                RsfSet(G_ROOM_ID, 3);
                R20fEmResetB0();
            }
            if (RsfCheck(G_ROOM_ID, 4) == 0 && (pG->Room_flg[2] & 0x80000000)) {
                int dead = r20f_work.p->em[2].isActive() == 0;

                if (r20f_work.p->em[3].isActive() == 0) {
                    dead++;
                }
                if (r20f_work.p->em[4].isActive() == 0) {
                    dead++;
                }
                if (r20f_work.p->em[5].isActive() == 0) {
                    dead++;
                }
                if (r20f_work.p->em[6].isActive() == 0) {
                    dead++;
                }
                if (dead > 2) {
                    RsfSet(G_ROOM_ID, 4);
                    R20fEmResetB1();
                }
            }
        }
        SceSleep(1);
    }
}

// Reset wave A0: two Ganados (0xE5/0xE6) spawn with a shout SE at the platform's far side.
void R20fEmResetA0()
{
    Vec pos = {49000.0f, 3500.0f, 26500.0f};

    SndCall(6, 3, &pos, 0, 0, 0);
    r20f_work.p->em[7].setEm(0xE5, -1, 0, 1, 1);
    r20f_work.p->em[8].setEm(0xE6, -1, 0, 1, 1);
}

// Guard alert: the player is below the platform level.
static int R20fCkEmGuard(cEmWrap* em)
{
    return pPL->pos.y < 3000.0f;
}

// Reset wave B0: Ganados 0xE9/0xEA spawn and stand guard at guardPos0/1 facing PI until the player is
// below the platform level (R20fCkEmGuard).
void R20fEmResetB0()
{
    r20f_work.p->em[9].setEm(0xE9, -1, 0, 1, 1);
    r20f_work.p->em[10].setEm(0xEA, -1, 0, 1, 1);
    r20f_work.p->guard[0].SetGuard(0xE9, &r20f_guardPos0, 1, R20fCkEmGuard, PI, 0, 1);
    r20f_work.p->guard[1].SetGuard(0xEA, &r20f_guardPos1, 1, R20fCkEmGuard, PI, 0, 1);
}

// Reset wave B1: Ganado 0xED spawns and guards guardPos2.
void R20fEmResetB1()
{
    r20f_work.p->em[13].setEm(0xED, -1, 0, 1, 1);
    r20f_work.p->guard[2].SetGuard(0xED, &r20f_guardPos2, 1, R20fCkEmGuard, PI, 0, 1);
}

// The initial Ganados: 0xDC / 0xE1 / 0xE2 patrol the three route tables, 0xDF / 0xE0 are just tracked.
static void R20fEmWanderingSet()
{
    r20f_work.p->patrol[0].SetPatrol(0xDC, r20f_patrolTbl0, 4, 0, 0);
    r20f_work.p->patrol[1].SetPatrol(0xE1, r20f_patrolTbl1, 2, 0, 0);
    r20f_work.p->patrol[2].SetPatrol(0xE2, r20f_patrolTbl2, 4, 0, 0);
    r20f_work.p->em[2].setPtr(0xDC, -1, 0);
    r20f_work.p->em[3].setPtr(0xDF, -1, 0);
    r20f_work.p->em[4].setPtr(0xE0, -1, 0);
    r20f_work.p->em[5].setPtr(0xE1, -1, 0);
    r20f_work.p->em[6].setPtr(0xE2, -1, 0);
}

// The module's .data continues 8-aligned (st2.cpp).
asm(".section .data; .balign 8");
