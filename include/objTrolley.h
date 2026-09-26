#ifndef OBJTROLLEY_H
#define OBJTROLLEY_H

#include "types.h"
#include "vec.h"
#include "obj.h"

cObj* SetTrolley(void* bin, void* tpl, Vec* pos, Vec* rot);

// Mine trolley work (game/objTrolley.cpp `cObjTrolley`): three cars (parts 0 / 4 / 8) with a
// scenario and an effect collision piece each.
struct TrolleyWork {
    u32 Be_flg;            // 0x00  bit0: start (setStart), bit1: 2nd start, bit2: stopped (ckStop)
    int Timer;            // 0x04
    void* Mot_tbl[9];         // 0x08  setMotion table: 0 run, 1 2nd run, 2/3 break (xFF), 4..8 player escape / die
    class cSat* pSat[3];   // 0x2C  scenario pieces per car (the SetTrolley / SatClear loops run over 5)
    class cSat* pEat[5];  // 0x38  effect pieces per car
    u8 Ride_pl;              // 0x4C  the player rides the trolley
};

// Mine trolley (obj 0x3B): three cars (parts 0 / 4 / 8) running along their motion with a scenario
// collision piece and an effect collision piece per car; the player and the enemies standing on a
// car are carried along, the break routine throws them off.
class cObjTrolley : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  TrolleyWork

    virtual void move();
    virtual ~cObjTrolley() {}
    void setMotion(void** tbl);
    int ckTrolleyRide(Vec* pos, u8* partsNo, Vec* out);
    int ckTrolleyRideAdjust(Vec* pos, Vec* out);
    void setStart();
    void set2ndStart();
    int ckStop();
};

#define TROLLEY_WK(o) ((TrolleyWork*) (o)->free)

#endif
