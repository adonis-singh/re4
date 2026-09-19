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

// Bulldozer (obj 0x3E): parts 2 carries the player, the partner and the enemies standing on it
// through the break / move / lift routines of its motion table; the partner drives it
// (Sub_bull_*) while the player shoots the pursuers (objBullHitCk).
class cObjBull : public cObj {
public:
    virtual void move();
    virtual ~cObjBull() {}

    void setMotion(void** tbl);
    int ckBullRide(Vec* pos, u8* partsNo, Vec* out);
    int ckBullRideAdjust(Vec* pos, Vec* out);
    int ckGoal();
    void setRide();
    int ckBreak1st();
    int ckBreak2nd();
    int ckBreak3rd();
    int ckBreak4th();
    int ckLift();
    int ckTruckGo();
    int ckLiftWait();
    void setSubBullDrive();
    void setSubBullFinger();
    void setSubBullLookBack();
    int getMoveFrameToLift();
    int getMoveFrameRtn();
    void setAdjustMode(u8 mode, void (*func)(cObj*));
    void setBreakTruck();
};

extern "C" {
int MotionMove(cModel* m, int a);
void LifeDownSet(cEm* em, int dmg, int rnd);
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
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

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

    obj = ObjMgr.create(0x3E);
    if (obj == 0) {
        return 0;
    }
    w = &obj->bull;
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
    AtariInit(&obj->sub2B4.atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->sub2B4.atari.throughOn();
    w->pSat = 0;
    w->pSat2 = 0;
    w->pEat = 0;
    w->Ride_pl = 0;
    p = w->mot;
    for (i = 0; i < 12; i++) {
        *p++ = 0;
    }
    w->break1st = 1;
    w->break2nd = 3;
    w->break4th = 3;
    w->break3rd = 1;
    w->type = type;
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
void objBull_R0_Set(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    w->cnt = 0;
    w->timer = 0;
    if (w->mot[0]) {
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
    } else {
        obj->matUpdate();
    }
    obj->partsWorldCalc();
    objBullSatSet(obj, 0);
}

// Rno0 == 1: first barrier: sound, the partner starts operating (Sub_bull_operation), the bump
// motion mot[0] repeated break1st times (Be_flg 2 when broken), riders carried along, enemy hit
// check in front of the blade.
void objBull_R0_Break1st(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        SndCall(6, 6, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 7, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        w->Act_ck = 0;
        w->timer = 0;
        obj->r_no_2++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->r_no_2++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        if (w->break1st == 0 || --w->break1st == 0) {
            obj->bull.Be_flg |= 2;
        }
        obj->r_no_2++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break1st == 0) {
                obj->r_no_0 = 2;
                obj->r_no_1 = 0;
                obj->r_no_2 = 0;
                obj->r_no_3 = 0;
            } else {
                obj->r_no_2 = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 2: drives to the second barrier (mot[1]), carrying the riders and hitting enemies.
void objBull_R0_To2nd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[1], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        obj->r_no_2++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->r_no_0 = 3;
            obj->r_no_1 = 0;
            obj->r_no_2 = 0;
            obj->r_no_3 = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 3: second barrier (mot[2], break2nd hits, Be_flg 4).
void objBull_R0_Break2nd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        w->Act_ck = 0;
        obj->r_no_2++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[2], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->r_no_2++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[2], 0, 0, 0x8001, 0);
        if (w->break2nd == 0 || --w->break2nd == 0) {
            obj->bull.Be_flg |= 4;
        }
        obj->r_no_2++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break2nd == 0) {
                obj->r_no_0 = 4;
                obj->r_no_1 = 0;
                obj->r_no_2 = 0;
                obj->r_no_3 = 0;
            } else {
                obj->r_no_2 = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 4: drives onto the lift (mot[3]).
static void objBull_R0_ToLift(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        SndCall(6, 8, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0, 0x8001, 0);
        obj->r_no_2++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->r_no_0 = 5;
            obj->r_no_1 = 0;
            obj->r_no_2 = 0;
            obj->r_no_3 = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 5: waits on the lift holding the last frame of mot[3] until the room sets Room_flg[0]
// 0x08000000 (the lift is called).
void objBull_R0_LiftWait(cObjBull* obj)
{
    BullWork* w = &obj->bull;
    int zero;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        obj->r_no_2++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0, 0x8001, (u16) ((*(u16*) w->mot[3] & 0x3FFF) - 1));
        MotionMove(obj, 0);
        zero = 0;
        if (pG->Room_flg[0] & 0x08000000) {
            obj->r_no_0 = 6;
            obj->r_no_1 = zero;
            obj->r_no_2 = zero;
            obj->r_no_3 = zero;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 0);
    w->timer++;
}

// Rno0 == 6: the lift ride (mot[4], Be_flg 0x20 while lifting, 0x100 at the top), then holds until
// Room_flg[0] 0x00400000.
void objBull_R0_Lift(cObjBull* obj)
{
    BullWork* w = &obj->bull;
    int zero;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[4], 0, 0, 0x8001, 0);
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        obj->bull.Be_flg |= 0x20;
        obj->r_no_2++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            w->Be_flg |= 0x100;
            obj->r_no_2++;
        }
        break;
    case 2:
        obj->r_no_2++;
    case 3:
        MotionMove(obj, 0);
        zero = 0;
        if (pG->Room_flg[0] & 0x00400000) {
            obj->r_no_0 = 7;
            obj->r_no_1 = zero;
            obj->r_no_2 = zero;
            obj->r_no_3 = zero;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 0);
    w->timer++;
}

// Rno0 == 7: drives off the lift to the third barrier (mot[5]).
void objBull_R0_To3rd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[5], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        obj->bull.Be_flg &= ~0x20;
        obj->r_no_2++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->r_no_0 = 8;
            obj->r_no_1 = 0;
            obj->r_no_2 = 0;
            obj->r_no_3 = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 8: third barrier (mot[6], break3rd hits, Be_flg 8).
void objBull_R0_Break3rd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        w->Act_ck = 0;
        obj->r_no_2++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[6], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->r_no_2++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[6], 0, 0, 0x8001, 0);
        if (w->break3rd == 0 || --w->break3rd == 0) {
            obj->bull.Be_flg |= 8;
        }
        obj->r_no_2++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break3rd == 0) {
                obj->r_no_0 = 9;
                obj->r_no_1 = 0;
                obj->r_no_2 = 0;
                obj->r_no_3 = 0;
            } else {
                obj->r_no_2 = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 9: drives to the fourth barrier (mot[7]).
void objBull_R0_To4th(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[7], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        obj->r_no_2++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->r_no_0 = 0xA;
            obj->r_no_1 = 0;
            obj->r_no_2 = 0;
            obj->r_no_3 = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 10: fourth barrier (mot[8], break4th hits, Be_flg 0x10).
void objBull_R0_Break4th(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        w->Act_ck = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        obj->r_no_2++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[8], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->r_no_2++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[8], 0, 0, 0x8001, 0);
        if (w->break4th == 0 || --w->break4th == 0) {
            obj->bull.Be_flg |= 0x10;
        }
        obj->r_no_2++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break4th == 0) {
                obj->r_no_0 = 0xB;
                obj->r_no_1 = 0;
                obj->r_no_2 = 0;
                obj->r_no_3 = 0;
            } else {
                obj->r_no_2 = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Rno0 == 11: the truck collision finale: the approach mot[9] (Be_flg 0x40 = truck coming), on
// setBreakTruck the crash mot[10] with the partner's reaction, Be_flg 1 at its end (goal), then
// the wreck loop mot[11] (Be_flg 0x80).
void objBull_R0_Collision(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->r_no_2) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[9], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].world, 0, 0, obj);
        w->frame = (*(u16*) w->mot[9] & 0x3FFF) - 30;
        obj->bull.Be_flg |= 0x40;
        w->Act_ck = 0;
        w->Truck_down = 0;
        obj->r_no_2++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            if (w->Truck_down) {
                obj->r_no_2 = 2;
            } else {
                obj->r_no_2 = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[10], 0, 0, 0x8201, 0);
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->r_no_2++;
    case 3:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            w->Be_flg |= 1;
        }
        break;
    case 4:
        MotionSetCore(obj, &obj->pMotion, w->mot[11], 0, 0, 0x8001, 0);
        obj->bull.Be_flg |= 0x80;
        obj->r_no_2++;
    case 5:
        w->cnt++;
        MotionMove(obj, 0);
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->Ride_pl) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

// Disables the bulldozer's collision (SAT, EAT, moving SAT) for this frame.
void objBullSatClear(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    if (w->pSat) {
        w->pSat->m_Flag &= ~4;
    }
    if (w->pSat2) {
        w->pSat2->m_Flag &= ~4;
    }
    if (w->pEat) {
        w->pEat->m_Flag &= ~4;
    }
}

// Places the collision (room archive entry 5) 500 above the rider parts; `moving` also places the
// second SAT set (kind 8) used while driving.
void objBullSatSet(cObjBull* obj, int moving)
{
    BullWork* w = &obj->bull;
    Vec pos;
    Vec rot;
    Vec v;
    cModel* parts;

    parts = obj->getPartsPtr(Bull_parts);
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
        w->pSat->m_Flag |= 4;
        w->pSat->setCoord(&pos, &rot);
    } else {
        w->pSat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
    }
    if (w->pEat) {
        w->pEat->m_Flag |= 4;
        w->pEat->setCoord(&pos, &rot);
    } else {
        w->pEat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 7);
    }
    if (moving) {
        if (w->pSat2) {
            w->pSat2->m_Flag |= 4;
            w->pSat2->setCoord(&pos, &rot);
        } else {
            w->pSat2 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 8);
        }
    }
}

// Installs the 12 route motions and starts mot[0].
void cObjBull::setMotion(void** tbl)
{
    BullWork* w = &bull;

    w->mot[0] = tbl[0];
    w->mot[1] = tbl[1];
    w->mot[2] = tbl[2];
    w->mot[3] = tbl[3];
    w->mot[4] = tbl[4];
    w->mot[5] = tbl[5];
    w->mot[6] = tbl[6];
    w->mot[7] = tbl[7];
    w->mot[8] = tbl[8];
    w->mot[9] = tbl[9];
    w->mot[10] = tbl[10];
    w->mot[11] = tbl[11];
    MotionSetCore(this, &pMotion, tbl[0], 0, 0, 0x8001, 0);
}

// Saves the rider parts matrix of the previous frame (Bull_MatOld) before the motion advances.
void objBullPushMtx(cObjBull* obj)
{
    PSMTXCopy(obj->getPartsPtr(Bull_parts)->mat, Bull_MatOld);
}

// 1 when pos lies inside the rider box of the previous-frame parts (x +-2300, y -500..1000, z +-4400).
static int objBullGetBullNo(cObjBull* obj, Vec* pos)
{
    Mtx inv;
    Vec v;

    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, pos, &v);
    if (v.x > -2300.0f && v.x < 2300.0f && v.y > -500.0f && v.y < 1000.0f && v.z > -4400.0f && v.z < 4400.0f) {
        return 1;
    }
    return 0;
}

// 1 when pos lies inside the rider box of the current parts (y 0..1000).
int objBullGetBullNo2(cObjBull* obj, Vec* pos)
{
    Mtx inv;
    Vec v;

    PSMTXInverse(obj->getPartsPtr(Bull_parts)->mat, inv);
    PSMTXMultVec(inv, pos, &v);
    if (v.x > -2300.0f && v.x < 2300.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -4400.0f && v.z < 4400.0f) {
        return 1;
    }
    return 0;
}

// Computes this frame's rider displacement (Bull_vec) and turn (Bull_dir) of the rider parts.
void objBullGetAdjust(cObjBull* obj)
{
    Vec p1;
    Vec p0;
    Vec d;
    cModel* parts;
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
    parts = obj->getPartsPtr(Bull_parts);
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
void objBullSetAdjust(cObjBull* obj, cEm* em)
{
    BullWork* w = &obj->bull;
    Mtx inv;
    Vec v;
    Vec d;
    cModel* parts;

    if (objBullGetBullNo(obj, &em->pos) == 0) {
        return;
    }
    em->be_flag |= 0x20000000;
    if (w->Ride_mode == 0) {
        return;
    }
    parts = obj->getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    PSMTXMultVec(parts->mat, &v, &v);
    PSVECSubtract(&v, &em->pos, &d);
    PSVECAdd(&em->x3A8, &d, &em->x3A8);
    em->ang.y += Bull_dir;
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    em->setPos(&v);
    if (em->id == 0) {
        if (CamCtrl.m_pExtraCamera) {
            PSVECAdd(&((Camera*) CamCtrl.m_pExtraCamera)->param.at, &d, &((Camera*) CamCtrl.m_pExtraCamera)->param.at);
            PSVECAdd(&((Camera*) CamCtrl.m_pExtraCamera)->param.pos, &d, &((Camera*) CamCtrl.m_pExtraCamera)->param.pos);
        }
        pG->quake_ofs = d;
    }
}

// Carries the player (and partner) standing on the rider parts.
static void objBullMoveAdjustPL(cObjBull* obj)
{
    if (obj->bull.adjust_func) {
        obj->bull.adjust_func(obj);
    }
    objBullSetAdjust(obj, pPL);
}

// Carries every enemy standing in the rider box.
void objBullMoveAdjustEM(cObjBull* obj)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1) {
            if (em->id == 0x42) {
                ((cEmWep*) em)->setParentMatCalc(1);
            } else if (em->id == 3) {
                objBullSetAdjust(obj, em);
            } else if (em->id > 0xF) {
                if (em->id <= 0x40) {
                    objBullSetAdjust(obj, em);
                }
            }
        }
    }
}

// 1 when pos is in the rider box: *partsNo = the rider parts, *out = the local position.
int cObjBull::ckBullRide(Vec* pos, u8* partsNo, Vec* out)
{
    Mtx inv;
    cModel* parts;

    if (objBullGetBullNo2(this, pos) == 0) {
        return 0;
    }
    parts = getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    if (out) {
        PSMTXMultVec(inv, pos, out);
    }
    if (partsNo) {
        *partsNo = Bull_parts;
    }
    return 1;
}

// 1 when pos is in the rider box; *out = the position re-projected onto the current parts.
int cObjBull::ckBullRideAdjust(Vec* pos, Vec* out)
{
    Mtx inv;
    Vec v;
    cModel* parts;

    if (objBullGetBullNo(this, pos) == 0) {
        return 0;
    }
    parts = getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, pos, &v);
    PSMTXMultVec(parts->mat, &v, out);
    return 1;
}

// While the blade (parts 4) moves, runs five player-weapon hit spheres (radius 1000, kind 0x12)
// along the blade front so the dozer crushes enemies in its way.
void objBullHitCk(cObjBull* obj)
{
    Vec v;
    cModel* parts;

    parts = obj->getPartsPtr(4);
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
    if (bull.Be_flg & 1) {
        return 1;
    }
    return 0;
}

// Puts the player on the rider parts and marks him riding.
void cObjBull::setRide()
{
    BullWork* w = &bull;
    Vec p;
    cModel* parts;
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
    w->timer = zero;
    switch (w->type) {
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
    if (bull.Be_flg & 2) {
        return 1;
    }
    return 0;
}

// 1 when the second barrier is broken (Be_flg 4).
int cObjBull::ckBreak2nd()
{
    if (bull.Be_flg & 4) {
        return 1;
    }
    return 0;
}

// 1 when the third barrier is broken (Be_flg 8).
int cObjBull::ckBreak3rd()
{
    if (bull.Be_flg & 8) {
        return 1;
    }
    return 0;
}

// 1 when the fourth barrier is broken (Be_flg 0x10).
int cObjBull::ckBreak4th()
{
    if (bull.Be_flg & 0x10) {
        return 1;
    }
    return 0;
}

// 1 while on the lift (Be_flg 0x20).
int cObjBull::ckLift()
{
    if (bull.Be_flg & 0x20) {
        return 1;
    }
    return 0;
}

// 1 once the truck approach started (Be_flg 0x40).
int cObjBull::ckTruckGo()
{
    if (bull.Be_flg & 0x40) {
        return 1;
    }
    return 0;
}

// 1 when the lift reached the top (Be_flg 0x100).
int cObjBull::ckLiftWait()
{
    if (bull.Be_flg & 0x100) {
        return 1;
    }
    return 0;
}

// The partner (`em`) sits in the driver's seat (parts 2 of the bulldozer it drives, em->pEmCatch).
static inline void SubBullSeat(cEm* em)
{
    Vec v;
    cModel* parts;

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
void Sub_bull_drive(cEm* em)
{
    StaFlagOn(pG, STA_SUB_BULLDOZER);
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        if (em->r_no_3) {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoom, 50), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoom, 50), 0, 3, 5, 0);
        }
        ((cSubChar*) em)->subHideMode = (u8) ((u32) Rnd() % 100);
        em->r_no_2++;
    case 1:
        SubBullSeat(em);
        MotionMove(em, 0);
        if (((cSubChar*) em)->subHideMode) {
            ((cSubChar*) em)->subHideMode--;
        } else if (SubCkNearEm()) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer((int) Sub_bull_lookback, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner routine: the lever operation motion (room motion 51) at each barrier, then back to driving.
void Sub_bull_operation(cEm* em)
{
    StaFlagOn(pG, STA_SUB_BULLDOZER);
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pGS->pRoom, 51), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner routine: looks back at the pursuers (room motion 66), then back to driving.
void Sub_bull_lookback(cEm* em)
{
    cModel* parts;

    StaFlagOn(pG, STA_SUB_BULLDOZER);
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pGS->pRoom, 66), 0, 3, 1, 0);
        parts = em->getPartsPtr(3);
        if (em->r_no_3) {
            SndStop(((cSubChar*) em)->subSndId, 0);
            ((cSubChar*) em)->subSndId = SndCall(6, 3, &parts->world, 0, 0, em);
        } else {
            SndStop(((cSubChar*) em)->subSndId, 0);
            ((cSubChar*) em)->subSndId = SndCall(6, 0x19, &parts->world, 0, 0, em);
        }
        em->r_no_2++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner routine: points ahead (room motion 67), then back to driving.
void Sub_bull_look(cEm* em)
{
    StaFlagOn(pG, STA_SUB_BULLDOZER);
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pGS->pRoom, 67), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

// Partner damage routine while driving: damage by the hit's type (grabbed/thrown 9999 = knocked
// off, else 500), the hit motion (room motion 52/53), then back to driving.
void Sub_dm_bull(cEm* em)
{
    int dmg;
    int type = 2;

    StaFlagOn(pG, STA_SUB_BULLDOZER);
    em->setStatus(EM_STATUS_IK_OFF);
    em->dmg.m_Timer = type;
    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        dmg = 0;
        switch (em->dmg.m_Wep) {
        default:
            dmg = 9999;
            break;
        case 0xD:
        case 0x12:
        case 0x13:
            if (em->dmg.m_Dist < 9000000.0f) {
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
        LifeDownSet(em, dmg, 0);
        if ((s16) pG->ashley_life <= 0) {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoom, 53), 0, 3, 1, 0);
            SndStop(((cSubChar*) em)->subSndId, 0);
            ((cSubChar*) em)->subSndId = SndCall(8, 0xD, &em->pParts->world, em->id, 0, 0);
        } else {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoom, 52), 0, 3, 1, 0);
            em->dmg.m_Timer = 1;
            SndStop(((cSubChar*) em)->subSndId, 0);
            ((cSubChar*) em)->subSndId = SndCall(8, 9, &em->pParts->world, em->id, 0, 0);
        }
        em->r_no_2++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->ashley_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
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
            SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
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
            SetSubBulldozer((int) Sub_bull_look, (int) Sub_dm_bull);
            pSUB->pEmCatch = (cEm*) this;
        }
    }
}

// Room call: partner looks back.
void cObjBull::setSubBullLookBack()
{
    if (pSUB) {
        if ((s16) pG->ashley_life > 0) {
            SetSubBulldozer((int) Sub_bull_lookback, (int) Sub_dm_bull);
            pSUB->r_no_3 = 1;
            pSUB->pEmCatch = (cEm*) this;
        }
    }
}

// Frames spent in the current routine (timer).
int cObjBull::getMoveFrameToLift()
{
    return bull.timer;
}

// Frames spent in the current routine (timer).
int cObjBull::getMoveFrameRtn()
{
    return bull.timer;
}

// Rider adjust mode (0 re-project, 1 displacement) and the room callback.
void cObjBull::setAdjustMode(u8 mode, void (*func)(cObj*))
{
    BullWork* w = &bull;

    w->Ride_mode = mode;
    w->adjust_func = func;
}

// Room call: the truck hits (Collision routine advances to the crash).
void cObjBull::setBreakTruck()
{
    bull.Truck_down = 1;
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
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20 && em->hp > 0 && (em->be_flag & 2)) {
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
