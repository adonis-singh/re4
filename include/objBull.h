#ifndef OBJBULL_H
#define OBJBULL_H

#include "types.h"
#include "vec.h"
#include "obj.h"

cObj* SetBull(void* bin, void* tpl, Vec* pos, Vec* rot, u32 type);

// Bulldozer work (game/objBull.cpp `cObjBull`): the player / partner ride parts 2 through the
// break / lift / collision routines; one scenario piece (two while moving) and an effect piece.
struct BullWork {
    u32 Be_flg;             // 0x00  bit0 goal, bit1..4 break 1st..4th done, bit5 lift, bit6 truck go, bit7, bit8 lift wait
    int Timer;              // 0x04  Collision: motion frames - 30
    u8 pad_8[4];
    void* Mot_tbl[12];      // 0x0C  setMotion table: 0 break1st/set, 1 to2nd, 2 break2nd, 3 to lift, 4 lift, 5 to3rd, 6 break3rd, 7 to4th, 8 break4th, 9..11 collision
    class cSat* pSat;       // 0x3C  scenario piece (type 1)
    class cSat* pSat2;      // 0x40  scenario piece (type 8) while moving
    class cSat* pEat;       // 0x44  effect piece (type 7)
    int Move_frame;         // 0x48  MotionMove calls of the current routine
    int Move_frame2;        // 0x4C  frames in the current routine (getMoveFrame*)
    u32 Move_point;         // 0x50  SetBull 5th argument
    u32 Start_point;        // 0x54  SetBull 5th argument: setRide start routine (0: break1st, 1: break2nd, 2: lift wait, 3: to3rd, 4: break3rd)
    u8 Barrier_hp[4];       // 0x58  break repeats left: 1st/2nd/3rd/4th
    void (*adjust_func)(cObj*); // 0x5C  setAdjustMode: called before the player is carried along
    u8 Ride_mode;           // 0x60  0: the riders are not carried along
    u8 Ride_pl;             // 0x61  the player rides the bulldozer (setRide)
    u8 Act_ck;              // 0x62
    u8 Truck_down;          // 0x63  setBreakTruck: Collision continues with step 2
};

// Bulldozer (obj 0x3E): parts 2 carries the player, the partner and the enemies standing on it
// through the break / move / lift routines of its motion table; the partner drives it
// (Sub_bull_*) while the player shoots the pursuers (objBullHitCk).
class cObjBull : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  BullWork

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

#define BULL_WK(o) ((BullWork*) (o)->free)

#endif
