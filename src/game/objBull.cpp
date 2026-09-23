// game/objBull: object id 0x3E, the bulldozer of the island chase (D:/Bio4/Prog/objBull.cpp). Its
// R0 routines run the scripted route (four barrier breaks, the lift, the truck collision) through
// the room's 12 motions; parts 2 carries the player, the partner (who drives, Sub_bull_*) and the
// enemies standing on it (objBullMoveAdjust*), the blade crushes enemies in front (objBullHitCk),
// and the ck* queries let the room script follow the progress (BullWork::Be_flg bits).
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "objBull.h"
#include "em.h"
#include "emwep.h"
#include "global.h"
#include "math_sub.h"
#include "cam_ctrl.h"
#include "snd.h"
#include "rnd.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "motion.h"
#include "em_sub.h"

extern "C" {
void objBull_R0_Set(cObjBull* obj);
void objBull_R0_Break1st(cObjBull* obj);
void objBull_R0_To2nd(cObjBull* obj);
void objBull_R0_Break2nd(cObjBull* obj);
static void objBull_R0_ToLift(cObjBull* obj);
void objBull_R0_LiftWait(cObjBull* obj);
void objBull_R0_Lift(cObjBull* obj);
void objBull_R0_To3rd(cObjBull* obj);
void objBull_R0_Break3rd(cObjBull* obj);
void objBull_R0_To4th(cObjBull* obj);
void objBull_R0_Break4th(cObjBull* obj);
void objBull_R0_Collision(cObjBull* obj);
void objBullSatClear(cObjBull* obj);
void objBullSatSet(cObjBull* obj, int moving);
void objBullPushMtx(cObjBull* obj);
static int objBullGetBullNo(cObjBull* obj, Vec* pos);
int objBullGetBullNo2(cObjBull* obj, Vec* pos);
void objBullGetAdjust(cObjBull* obj);
void objBullSetAdjust(cObjBull* obj, cEm* em);
static void objBullMoveAdjustPL(cObjBull* obj);
void objBullMoveAdjustEM(cObjBull* obj);
void objBullHitCk(cObjBull* obj);
void Sub_bull_drive(cEm* em);
void Sub_bull_operation(cEm* em);
void Sub_bull_lookback(cEm* em);
void Sub_bull_look(cEm* em);
void Sub_dm_bull(cEm* em);
int SubCkNearEm();
}

void (*ObjBull_R0_move_tbl[12])(cObjBull*) = {
    objBull_R0_Set,      objBull_R0_Break1st, objBull_R0_To2nd, objBull_R0_Break2nd,
    objBull_R0_ToLift,   objBull_R0_LiftWait, objBull_R0_Lift,  objBull_R0_To3rd,
    objBull_R0_Break3rd, objBull_R0_To4th,    objBull_R0_Break4th, objBull_R0_Collision,
};

static Mtx Bull_MatOld;     // parts matrix of the previous frame (objBullPushMtx)
Vec Bull_vec;               // movement of this frame (objBullGetAdjust)
static u8 Bull_parts = 2;   // the parts the riders stand on
static f32 Bull_dir;        // turn of this frame

// Creates the bulldozer (id 0x3E) at pos/rot: box collision, no rider, 12 motion slots empty, the
// four barrier break counters (1, 3, 1, 3 hits), Move_point = type, ride adjust mode 1, and its
// SAT/EAT collision from room archive entry 5.
cObj* SetBull(void* bin, void* tpl, Vec* pos, Vec* rot, u32 type)
{
    cObj* obj;
    BullWork* w;
    int i;
    void** p;

    obj = ObjMgr.create(cObjMgr::ID_BULL);
    if (obj == 0) {
        return 0;
    }
    w = BULL_WK((cObjBull*) obj);
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->pos_old = obj->pos;
    if (rot) {
        obj->ang = *rot;
    } else {
        obj->ang.x = 0.0f;
        obj->ang.y = 0.0f;
        obj->ang.z = 0.0f;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->atari.off();
    w->pSat = 0;
    w->pSat2 = 0;
    w->pEat = 0;
    w->Ride_pl = 0;
    p = w->Mot_tbl;
    for (i = 0; i < 12; i++) {
        *p++ = 0;
    }
    w->Barrier_hp[0] = 1;
    w->Barrier_hp[1] = 3;
    w->Barrier_hp[3] = 3;
    w->Barrier_hp[2] = 1;
    w->Start_point = type;
    w->Move_point = type;
    w->Ride_mode = 1;
    obj->r_no_0 = 0;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    objBullSatSet((cObjBull*) obj, 0);
    return obj;
}

// Per-frame: releases the moving collision, then the R0 routine (ObjBull_R0_move_tbl).
void cObjBull::move()
{
    objBullSatClear(this);
    ObjBull_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: parked at the start with mot[0] on its first frame; collision placed.
void objBull_R0_Set(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    w->Move_frame = 0;
    w->Move_frame2 = 0;
    if (w->Mot_tbl[0]) {
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[0], 0, 0, 0x8001, 0);
        MotionMove(pObj, 0);
    } else {
        pObj->matUpdate();
    }
    pObj->partsWorldCalc();
    objBullSatSet(pObj, 0);
}

// Rno0 == 1: first barrier: sound, the partner starts operating (Sub_bull_operation), the bump
// motion mot[0] repeated Barrier_hp[0] times (Be_flg 2 when broken), riders carried along, enemy hit
// check in front of the blade.
void objBull_R0_Break1st(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        SndCall(6, 6, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 7, &(pObj->pList)[2].world, 0, 0, pObj);
        w->Act_ck = 0;
        w->Move_frame2 = 0;
        pObj->r_no_2++;
    case 1:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[0], 0, 0, 0x8001, 0);
        MotionMove(pObj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_operation, Sub_dm_bull);
        }
        pObj->r_no_2++;
        break;
    case 2:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[0], 0, 0, 0x8001, 0);
        if (w->Barrier_hp[0] == 0 || --w->Barrier_hp[0] == 0) {
            BULL_WK(pObj)->Be_flg |= 2;
        }
        pObj->r_no_2++;
    case 3:
        if (MotionMove(pObj, 0)) {
            if (w->Barrier_hp[0] == 0) {
                pObj->r_no_0 = 2;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            } else {
                pObj->r_no_2 = 0;
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 2: drives to the second barrier (mot[1]), carrying the riders and hitting enemies.
void objBull_R0_To2nd(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[1], 0, 0, 0x8001, 0);
        SndCall(6, 8, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 9, &(pObj->pList)[2].world, 0, 0, pObj);
        pObj->r_no_2++;
    case 1:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            pObj->r_no_0 = 3;
            pObj->r_no_1 = 0;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 3: second barrier (mot[2], Barrier_hp[1] hits, Be_flg 4).
void objBull_R0_Break2nd(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        SndCall(6, 0xA, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 0xB, &(pObj->pList)[2].world, 0, 0, pObj);
        w->Act_ck = 0;
        pObj->r_no_2++;
    case 1:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[2], 0, 0, 0x8001, 0);
        MotionMove(pObj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_operation, Sub_dm_bull);
        }
        pObj->r_no_2++;
        break;
    case 2:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[2], 0, 0, 0x8001, 0);
        if (w->Barrier_hp[1] == 0 || --w->Barrier_hp[1] == 0) {
            BULL_WK(pObj)->Be_flg |= 4;
        }
        pObj->r_no_2++;
    case 3:
        if (MotionMove(pObj, 0)) {
            if (w->Barrier_hp[1] == 0) {
                pObj->r_no_0 = 4;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            } else {
                pObj->r_no_2 = 0;
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 4: drives onto the lift (mot[3]).
static void objBull_R0_ToLift(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        SndCall(6, 8, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 9, &(pObj->pList)[2].world, 0, 0, pObj);
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[3], 0, 0, 0x8001, 0);
        pObj->r_no_2++;
    case 1:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            pObj->r_no_0 = 5;
            pObj->r_no_1 = 0;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 5: waits on the lift holding the last frame of mot[3] until the room sets Room_flg[0]
// 0x08000000 (the lift is called).
void objBull_R0_LiftWait(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);
    int zero;

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        SndCall(6, 0xA, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 0xB, &(pObj->pList)[2].world, 0, 0, pObj);
        pObj->r_no_2++;
    case 1:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[3], 0, 0, 0x8001, (u16) ((*(u16*) w->Mot_tbl[3] & 0x3FFF) - 1));
        MotionMove(pObj, 0);
        zero = 0;
        if (pG->Room_flg[0] & 0x08000000) {  // RMF_LIFT_START (r30f)
            pObj->r_no_0 = 6;
            pObj->r_no_1 = zero;
            pObj->r_no_2 = zero;
            pObj->r_no_3 = zero;
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 0);
    w->Move_frame2++;
}

// Rno0 == 6: the lift ride (mot[4], Be_flg 0x20 while lifting, 0x100 at the top), then holds until
// Room_flg[0] 0x00400000.
void objBull_R0_Lift(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);
    int zero;

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[4], 0, 0, 0x8001, 0);
        SndCall(6, 0xA, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 0xB, &(pObj->pList)[2].world, 0, 0, pObj);
        BULL_WK(pObj)->Be_flg |= 0x20;
        pObj->r_no_2++;
    case 1:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            w->Be_flg |= 0x100;
            pObj->r_no_2++;
        }
        break;
    case 2:
        pObj->r_no_2++;
    case 3:
        MotionMove(pObj, 0);
        zero = 0;
        if (pG->Room_flg[0] & 0x00400000) {  // RMF_LIFT_END (r30f)
            pObj->r_no_0 = 7;
            pObj->r_no_1 = zero;
            pObj->r_no_2 = zero;
            pObj->r_no_3 = zero;
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 0);
    w->Move_frame2++;
}

// Rno0 == 7: drives off the lift to the third barrier (mot[5]).
void objBull_R0_To3rd(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[5], 0, 0, 0x8001, 0);
        SndCall(6, 8, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 9, &(pObj->pList)[2].world, 0, 0, pObj);
        BULL_WK(pObj)->Be_flg &= ~0x20;
        pObj->r_no_2++;
    case 1:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            pObj->r_no_0 = 8;
            pObj->r_no_1 = 0;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 8: third barrier (mot[6], Barrier_hp[2] hits, Be_flg 8).
void objBull_R0_Break3rd(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        SndCall(6, 0xA, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 0xB, &(pObj->pList)[2].world, 0, 0, pObj);
        w->Act_ck = 0;
        pObj->r_no_2++;
    case 1:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[6], 0, 0, 0x8001, 0);
        MotionMove(pObj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_operation, Sub_dm_bull);
        }
        pObj->r_no_2++;
        break;
    case 2:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[6], 0, 0, 0x8001, 0);
        if (w->Barrier_hp[2] == 0 || --w->Barrier_hp[2] == 0) {
            BULL_WK(pObj)->Be_flg |= 8;
        }
        pObj->r_no_2++;
    case 3:
        if (MotionMove(pObj, 0)) {
            if (w->Barrier_hp[2] == 0) {
                pObj->r_no_0 = 9;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            } else {
                pObj->r_no_2 = 0;
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 9: drives to the fourth barrier (mot[7]).
void objBull_R0_To4th(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[7], 0, 0, 0x8001, 0);
        SndCall(6, 8, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 9, &(pObj->pList)[2].world, 0, 0, pObj);
        pObj->r_no_2++;
    case 1:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            pObj->r_no_0 = 0xA;
            pObj->r_no_1 = 0;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 10: fourth barrier (mot[8], Barrier_hp[3] hits, Be_flg 0x10).
void objBull_R0_Break4th(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        w->Act_ck = 0;
        SndCall(6, 0xA, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 0xB, &(pObj->pList)[2].world, 0, 0, pObj);
        pObj->r_no_2++;
    case 1:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[8], 0, 0, 0x8001, 0);
        MotionMove(pObj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_operation, Sub_dm_bull);
        }
        pObj->r_no_2++;
        break;
    case 2:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[8], 0, 0, 0x8001, 0);
        if (w->Barrier_hp[3] == 0 || --w->Barrier_hp[3] == 0) {
            BULL_WK(pObj)->Be_flg |= 0x10;
        }
        pObj->r_no_2++;
    case 3:
        if (MotionMove(pObj, 0)) {
            if (w->Barrier_hp[3] == 0) {
                pObj->r_no_0 = 0xB;
                pObj->r_no_1 = 0;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            } else {
                pObj->r_no_2 = 0;
            }
        }
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Rno0 == 11: the truck collision finale: the approach mot[9] (Be_flg 0x40 = truck coming), on
// setBreakTruck the crash mot[10] with the partner's reaction, Be_flg 1 at its end (goal), then
// the wreck loop mot[11] (Be_flg 0x80).
void objBull_R0_Collision(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    objBullPushMtx(pObj);
    switch (pObj->r_no_2) {
    case 0:
        w->Move_frame2 = 0;
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[9], 0, 0, 0x8001, 0);
        SndCall(6, 8, &(pObj->pList)[2].world, 0, 0, pObj);
        SndCall(6, 9, &(pObj->pList)[2].world, 0, 0, pObj);
        w->Timer = (*(u16*) w->Mot_tbl[9] & 0x3FFF) - 30;
        BULL_WK(pObj)->Be_flg |= 0x40;
        w->Act_ck = 0;
        w->Truck_down = 0;
        pObj->r_no_2++;
    case 1:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            if (w->Truck_down) {
                pObj->r_no_2 = 2;
            } else {
                pObj->r_no_2 = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[10], 0, 0, 0x8201, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_operation, Sub_dm_bull);
        }
        pObj->r_no_2++;
    case 3:
        w->Move_frame++;
        if (MotionMove(pObj, 0)) {
            w->Be_flg |= 1;
        }
        break;
    case 4:
        MotionSetCore(pObj, &pObj->Motion, w->Mot_tbl[11], 0, 0, 0x8001, 0);
        BULL_WK(pObj)->Be_flg |= 0x80;
        pObj->r_no_2++;
    case 5:
        w->Move_frame++;
        MotionMove(pObj, 0);
        break;
    }
    pObj->partsWorldCalc();
    objBullGetAdjust(pObj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(pObj);
    }
    objBullMoveAdjustEM(pObj);
    objBullSatSet(pObj, 1);
    objBullHitCk(pObj);
    w->Move_frame2++;
}

// Disables the bulldozer's collision (SAT, EAT, moving SAT) for this frame.
void objBullSatClear(cObjBull* pObj)
{
    BullWork* w = BULL_WK(pObj);

    if (w->pSat) {
        w->pSat->setDisable();
    }
    if (w->pSat2) {
        w->pSat2->setDisable();
    }
    if (w->pEat) {
        w->pEat->setDisable();
    }
}

// Places the collision (room archive entry 5) 500 above the rider parts; `moving` also places the
// second SAT set (kind 8) used while driving.
void objBullSatSet(cObjBull* pObj, int mode)
{
    BullWork* w = BULL_WK(pObj);
    Vec pos;
    Vec rot;
    Vec v;
    cParts* parts;

    parts = pObj->getPartsPtr(Bull_parts);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXMultVecSR(parts->mat, &v, &v);
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos = parts->world;
    pos.y += 500.0f;
    if (w->pSat) {
        w->pSat->setEnable();
        w->pSat->setCoord(&pos, &rot);
    } else {
        w->pSat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
    }
    if (w->pEat) {
        w->pEat->setEnable();
        w->pEat->setCoord(&pos, &rot);
    } else {
        w->pEat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 7);
    }
    if (mode) {
        if (w->pSat2) {
            w->pSat2->setEnable();
            w->pSat2->setCoord(&pos, &rot);
        } else {
            w->pSat2 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 8);
        }
    }
}

// Installs the 12 route motions and starts mot[0].
void cObjBull::setMotion(void** mot_tbl)
{
    BullWork* w = BULL_WK(this);

    w->Mot_tbl[0] = mot_tbl[0];
    w->Mot_tbl[1] = mot_tbl[1];
    w->Mot_tbl[2] = mot_tbl[2];
    w->Mot_tbl[3] = mot_tbl[3];
    w->Mot_tbl[4] = mot_tbl[4];
    w->Mot_tbl[5] = mot_tbl[5];
    w->Mot_tbl[6] = mot_tbl[6];
    w->Mot_tbl[7] = mot_tbl[7];
    w->Mot_tbl[8] = mot_tbl[8];
    w->Mot_tbl[9] = mot_tbl[9];
    w->Mot_tbl[10] = mot_tbl[10];
    w->Mot_tbl[11] = mot_tbl[11];
    MotionSetCore(this, &Motion, mot_tbl[0], 0, 0, 0x8001, 0);
}

// Saves the rider parts matrix of the previous frame (Bull_MatOld) before the motion advances.
void objBullPushMtx(cObjBull* pObj)
{
    PSMTXCopy(pObj->getPartsPtr(Bull_parts)->mat, Bull_MatOld);
}

// 1 when pos lies inside the rider box of the previous-frame parts (x +-2300, y -500..1000, z +-4400).
static int objBullGetBullNo(cObjBull* pObj, Vec* pPos)
{
    Mtx inv;
    Vec v;

    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, pPos, &v);
    if (v.x > -2300.0f && v.x < 2300.0f && v.y > -500.0f && v.y < 1000.0f && v.z > -4400.0f && v.z < 4400.0f) {
        return 1;
    }
    return 0;
}

// 1 when pos lies inside the rider box of the current parts (y 0..1000).
int objBullGetBullNo2(cObjBull* pObj, Vec* pPos)
{
    Mtx inv;
    Vec v;

    PSMTXInverse(pObj->getPartsPtr(Bull_parts)->mat, inv);
    PSMTXMultVec(inv, pPos, &v);
    if (v.x > -2300.0f && v.x < 2300.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -4400.0f && v.z < 4400.0f) {
        return 1;
    }
    return 0;
}

// Computes this frame's rider displacement (Bull_vec) and turn (Bull_dir) of the rider parts.
void objBullGetAdjust(cObjBull* pObj)
{
    Vec p1;
    Vec p0;
    Vec d;
    cParts* parts;
    f32 a0;
    f32 a1;

    p0.x = 0.0f;
    p0.y = 500.0f;
    p0.z = 0.0f;
    PSMTXMultVec(Bull_MatOld, &p0, &p0);
    d.x = 0.0f;
    d.y = 0.0f;
    d.z = 1.0f;
    PSMTXMultVecSR(Bull_MatOld, &d, &d);
    a0 = atan2f(d.x, d.z);
    parts = pObj->getPartsPtr(Bull_parts);
    p1.x = 0.0f;
    p1.y = 500.0f;
    p1.z = 0.0f;
    PSMTXMultVec(parts->mat, &p1, &p1);
    d.x = 0.0f;
    d.y = 0.0f;
    d.z = 1.0f;
    PSMTXMultVecSR(parts->mat, &d, &d);
    a1 = atan2f(d.x, d.z);
    PSVECSubtract(&p1, &p0, &Bull_vec);
    Bull_dir = Muku2(a0, a1, PI);
}

// Moves a rider by the displacement/turn (mode 0: re-projects it through the parts matrices),
// also shifting the extra camera.
void objBullSetAdjust(cObjBull* pObj, cEm* pEm)
{
    BullWork* w = BULL_WK(pObj);
    Mtx inv;
    Vec v;
    Vec d;
    cParts* parts;

    if (objBullGetBullNo(pObj, &pEm->pos) == 0) {
        return;
    }
    pEm->be_flag |= 0x20000000;
    if (w->Ride_mode == 0) {
        return;
    }
    parts = pObj->getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, &pEm->pos, &v);
    PSMTXMultVec(parts->mat, &v, &v);
    PSVECSubtract(&v, &pEm->pos, &d);
    PSVECAdd(&pEm->Catch_at_adj, &d, &pEm->Catch_at_adj);
    pEm->ang.y += Bull_dir;
    pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
    pEm->setPos(&v);
    if (pEm->id == 0) {
        if (CamCtrl.m_pExtraCamera) {
            PSVECAdd(&((Camera*) CamCtrl.m_pExtraCamera)->param.at, &d, &((Camera*) CamCtrl.m_pExtraCamera)->param.at);
            PSVECAdd(&((Camera*) CamCtrl.m_pExtraCamera)->param.pos, &d, &((Camera*) CamCtrl.m_pExtraCamera)->param.pos);
        }
        pG->quake_ofs = d;
    }
}

// Carries the player (and partner) standing on the rider parts.
static void objBullMoveAdjustPL(cObjBull* pObj)
{
    if (BULL_WK(pObj)->adjust_func) {
        BULL_WK(pObj)->adjust_func(pObj);
    }
    objBullSetAdjust(pObj, pPL);
}

// Carries every enemy standing in the rider box.
void objBullMoveAdjustEM(cObjBull* pObj)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if (em->isAlive()) {
            if (em->id == 0x42) {
                ((cEmWep*) em)->setParentMatCalc(1);
            } else if (em->id == 3) {
                objBullSetAdjust(pObj, em);
            } else if (em->id > 0xF) {
                if (em->id <= 0x40) {
                    objBullSetAdjust(pObj, em);
                }
            }
        }
    }
}

// 1 when pos is in the rider box: *partsNo = the rider parts, *out = the local position.
int cObjBull::ckBullRide(Vec* pPos, u8* pParts_no, Vec* pOffset)
{
    Mtx inv;
    cParts* parts;

    if (objBullGetBullNo2(this, pPos) == 0) {
        return 0;
    }
    parts = getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    if (pOffset) {
        PSMTXMultVec(inv, pPos, pOffset);
    }
    if (pParts_no) {
        *pParts_no = Bull_parts;
    }
    return 1;
}

// 1 when pos is in the rider box; *out = the position re-projected onto the current parts.
int cObjBull::ckBullRideAdjust(Vec* pPos, Vec* pPos2)
{
    Mtx inv;
    Vec v;
    cParts* parts;

    if (objBullGetBullNo(this, pPos) == 0) {
        return 0;
    }
    parts = getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, pPos, &v);
    PSMTXMultVec(parts->mat, &v, pPos2);
    return 1;
}

// While the blade (parts 4) moves, runs five player-weapon hit spheres (radius 1000, kind 0x12)
// along the blade front so the dozer crushes enemies in its way.
void objBullHitCk(cObjBull* pObj)
{
    Vec v;
    cParts* parts;

    parts = pObj->getPartsPtr(4);
    if ((parts->world.x - parts->world_old2.x) * (parts->world.x - parts->world_old2.x) +
        (parts->world.y - parts->world_old2.y) * (parts->world.y - parts->world_old2.y) +
        (parts->world.z - parts->world_old2.z) * (parts->world.z - parts->world_old2.z) < 2500.0f) {
        return;
    }
    v.x = 0.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = 1000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = 2000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = -1000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = -2000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
}

// 1 after the truck crash finished (Be_flg 1).
int cObjBull::ckGoal()
{
    if (BULL_WK(this)->Be_flg & 1) {
        return 1;
    }
    return 0;
}

// Puts the player on the rider parts and marks him riding.
void cObjBull::setRide()
{
    BullWork* w = BULL_WK(this);
    Vec p;
    cParts* parts;
    int zero = 0;

    parts = getPartsPtr(Bull_parts);
    p = parts->world;
    p.y += 1000.0f;
    pPL->setPos(&p);
    StaFlagOn(pG, STA_RIDE_GONDOLA);
    w->Ride_pl = 1;
    if (pSUB) {
        p = parts->world;
        p.y += 1000.0f;
        p.z += -500.0f;
        pSUB->setPos(&p);
    }
    w->Move_frame2 = zero;
    switch (w->Start_point) {
    case 0:
    default:
        r_no_0 = 1;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
        break;
    case 1:
        r_no_0 = 4;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
        break;
    case 2:
        r_no_0 = 5;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
        break;
    case 3:
        r_no_0 = 7;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
        break;
    case 4:
        r_no_0 = 9;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 0;
        break;
    }
}

// 1 when the first barrier is broken (Be_flg 2).
int cObjBull::ckBreak1st()
{
    if (BULL_WK(this)->Be_flg & 2) {
        return 1;
    }
    return 0;
}

// 1 when the second barrier is broken (Be_flg 4).
int cObjBull::ckBreak2nd()
{
    if (BULL_WK(this)->Be_flg & 4) {
        return 1;
    }
    return 0;
}

// 1 when the third barrier is broken (Be_flg 8).
int cObjBull::ckBreak3rd()
{
    if (BULL_WK(this)->Be_flg & 8) {
        return 1;
    }
    return 0;
}

// 1 when the fourth barrier is broken (Be_flg 0x10).
int cObjBull::ckBreak4th()
{
    if (BULL_WK(this)->Be_flg & 0x10) {
        return 1;
    }
    return 0;
}

// 1 while on the lift (Be_flg 0x20).
int cObjBull::ckLift()
{
    if (BULL_WK(this)->Be_flg & 0x20) {
        return 1;
    }
    return 0;
}

// 1 once the truck approach started (Be_flg 0x40).
int cObjBull::ckTruckGo()
{
    if (BULL_WK(this)->Be_flg & 0x40) {
        return 1;
    }
    return 0;
}

// 1 when the lift reached the top (Be_flg 0x100).
int cObjBull::ckLiftWait()
{
    if (BULL_WK(this)->Be_flg & 0x100) {
        return 1;
    }
    return 0;
}

// The partner (`em`) sits in the driver's seat (parts 2 of the bulldozer it drives, em->pEmCatch).
static inline void SubBullSeat(cEm* em)
{
    Vec v;
    cParts* parts;

    if (em->pEmCatch) {
        parts = em->pEmCatch->getPartsPtr(2);
        v.x = 0.0f;
        v.y = 452.29f;
        v.z = 3173.64f;
        PSMTXMultVec(parts->mat, &v, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1.0f;
        PSMTXMultVecSR(parts->mat, &v, &v);
        em->ang.y = atan2f(v.x, v.z);
    }
}

// Partner routine (SetSubBulldozer): driving idle motion (room motion 50) in the seat; a
// look-back is queued when enemies approach (SubCkNearEm).
void Sub_bull_drive(cEm* pEm)
{
    StaFlagOn(pG, STA_SUB_BULLDOZER);
    pEm->setStatus(EM_STATUS_IK_OFF);
    switch (pEm->r_no_2) {
    case 0:
        pEm->atari.off();
        if (pEm->r_no_3) {
            MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 50), 0, 0, 5, 0);
        } else {
            MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 50), 0, 3, 5, 0);
        }
        ((cSubChar*) pEm)->m_Work0 = (u8) ((u32) Rnd() % 100);
        pEm->r_no_2++;
    case 1:
        SubBullSeat(pEm);
        MotionMove(pEm, 0);
        if (((cSubChar*) pEm)->m_Work0) {
            ((cSubChar*) pEm)->m_Work0--;
        } else if (SubCkNearEm()) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer(Sub_bull_lookback, Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner routine: the lever operation motion (room motion 51) at each barrier, then back to driving.
void Sub_bull_operation(cEm* pEm)
{
    StaFlagOn(pG, STA_SUB_BULLDOZER);
    pEm->setStatus(EM_STATUS_IK_OFF);
    switch (pEm->r_no_2) {
    case 0:
        pEm->atari.off();
        MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 51), 0, 3, 1, 0);
        pEm->r_no_2++;
    case 1:
        SubBullSeat(pEm);
        if (MotionMove(pEm, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer(Sub_bull_drive, Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner routine: looks back at the pursuers (room motion 66), then back to driving.
void Sub_bull_lookback(cEm* pEm)
{
    cParts* parts;

    StaFlagOn(pG, STA_SUB_BULLDOZER);
    pEm->setStatus(EM_STATUS_IK_OFF);
    switch (pEm->r_no_2) {
    case 0:
        pEm->atari.off();
        MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 66), 0, 3, 1, 0);
        parts = pEm->getPartsPtr(3);
        if (pEm->r_no_3) {
            SndStop(((cSubChar*) pEm)->m_StopSe, 0);
            ((cSubChar*) pEm)->m_StopSe = SndCall(6, 3, &parts->world, 0, 0, pEm);
        } else {
            SndStop(((cSubChar*) pEm)->m_StopSe, 0);
            ((cSubChar*) pEm)->m_StopSe = SndCall(6, 0x19, &parts->world, 0, 0, pEm);
        }
        pEm->r_no_2++;
    case 1:
        SubBullSeat(pEm);
        if (MotionMove(pEm, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer(Sub_bull_drive, Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner routine: points ahead (room motion 67), then back to driving.
void Sub_bull_look(cEm* pEm)
{
    StaFlagOn(pG, STA_SUB_BULLDOZER);
    pEm->setStatus(EM_STATUS_IK_OFF);
    switch (pEm->r_no_2) {
    case 0:
        pEm->atari.off();
        MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 67), 0, 3, 1, 0);
        pEm->r_no_2++;
    case 1:
        SubBullSeat(pEm);
        if (MotionMove(pEm, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer(Sub_bull_drive, Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner damage routine while driving: damage by the hit's type (grabbed/thrown 9999 = knocked
// off, else 500), the hit motion (room motion 52/53), then back to driving.
void Sub_dm_bull(cEm* pEm)
{
    int dmg;
    int type = 2;

    StaFlagOn(pG, STA_SUB_BULLDOZER);
    pEm->setStatus(EM_STATUS_IK_OFF);
    pEm->dmg.m_Timer = type;
    switch (pEm->r_no_2) {
    case 0:
        pEm->atari.off();
        dmg = 0;
        switch (pEm->dmg.m_Wep) {
        default:
            dmg = 9999;
            break;
        case 0xD:
        case 0x12:
        case 0x13:
            if (pEm->dmg.m_Dist < 9000000.0f) {
                dmg = 9999;
            } else {
                dmg = 500;
            }
            break;
        case 0x18:
            dmg = 500;
            break;
        case 0xE:
        case 0x17:
        case 0x2A:
            break;
        }
        LifeDownSet(pEm, dmg, 0);
        if ((s16) pG->ashley_life <= 0) {
            MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 53), 0, 3, 1, 0);
            SndStop(((cSubChar*) pEm)->m_StopSe, 0);
            ((cSubChar*) pEm)->m_StopSe = SndCall(8, 0xD, &pEm->pList->world, pEm->id, 0, 0);
        } else {
            MotionSetCore(pEm, &pEm->Motion, ROOM_ARC_PTR(pG->pRoom, 52), 0, 3, 1, 0);
            pEm->dmg.m_Timer = 1;
            SndStop(((cSubChar*) pEm)->m_StopSe, 0);
            ((cSubChar*) pEm)->m_StopSe = SndCall(8, 9, &pEm->pList->world, pEm->id, 0, 0);
        }
        pEm->r_no_2++;
    case 1:
        SubBullSeat(pEm);
        if (MotionMove(pEm, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer(Sub_bull_drive, Sub_dm_bull);
            }
        }
        break;
    }
}

// Room call: partner into the driving routine on this dozer.
void cObjBull::setSubBullDrive()
{
    if (pSUB) {
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_drive, Sub_dm_bull);
            pSUB->r_no_3 = 1;
            pSUB->pEmCatch = (cEm*) this;
        }
    }
}

// Room call: partner points ahead.
void cObjBull::setSubBullFinger()
{
    if (pSUB) {
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_look, Sub_dm_bull);
            pSUB->pEmCatch = (cEm*) this;
        }
    }
}

// Room call: partner looks back.
void cObjBull::setSubBullLookBack()
{
    if (pSUB) {
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer(Sub_bull_lookback, Sub_dm_bull);
            pSUB->r_no_3 = 1;
            pSUB->pEmCatch = (cEm*) this;
        }
    }
}

// Frames spent in the current routine (timer).
int cObjBull::getMoveFrameToLift()
{
    return BULL_WK(this)->Move_frame2;
}

// Frames spent in the current routine (timer).
int cObjBull::getMoveFrameRtn()
{
    return BULL_WK(this)->Move_frame2;
}

// Rider adjust mode (0 re-project, 1 displacement) and the room callback.
void cObjBull::setAdjustMode(u8 mode, void (*func)(cObj*))
{
    BullWork* w = BULL_WK(this);

    w->Ride_mode = mode;
    w->adjust_func = func;
}

// Room call: the truck hits (Collision routine advances to the crash).
void cObjBull::setBreakTruck()
{
    BULL_WK(this)->Truck_down = 1;
}

// An enemy stands in the box in front of the partner's seat (the player's position widens it).
int SubCkNearEm()
{
    Mtx inv;
    Vec v;
    f32 zmin;
    u32 i;

    if (pSUB == 0) {
        return 0;
    }
    PSMTXInverse(pSUB->mat, inv);
    zmin = -1000.0f;
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x < -2000.0f) {
        zmin = -6000.0f;
    }
    if (v.z < -6000.0f) {
        zmin = -6000.0f;
    }
    if (v.z > 0.0f) {
        zmin = -6000.0f;
    }
    if (v.y < -1000.0f) {
        zmin = -6000.0f;
    }
    if (v.y > 2000.0f) {
        zmin = -6000.0f;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if (em->isAlive() && em->id > 0xF && em->id <= 0x20 && em->hp > 0 && (em->be_flag & 2)) {
            PSMTXMultVec(inv, &em->pos, &v);
            if (v.x < -2000.0f) {
                continue;
            }
            if (v.x < -2000.0f) {
                continue;
            }
            if (v.z < zmin) {
                continue;
            }
            if (v.z > 0.0f) {
                continue;
            }
            if (v.y < -1000.0f) {
                continue;
            }
            if (v.y > 2000.0f) {
                continue;
            }
            return 1;
        }
    }
    return 0;
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
