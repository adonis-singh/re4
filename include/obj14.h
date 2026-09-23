#ifndef OBJ14_H
#define OBJ14_H

#include "types.h"
#include "vec.h"
#include "obj.h"
#include "pendulum.h"

// Bell work (PS2 FREE_OBJ14) in cObjBell::free: a hit-receiving enemy plus a pendulum chain for the swing.
struct BellWork {
    u8 pad_0[0xA];
    u16 ringTimer;        // 0x0A  frames the "rung" state is reported to pG (90 after a hit)
    class cEmHit* pEmHit; // 0x0C
    struct PenCloth cloth;  // 0x10 .. 0x70
};

// Bell: a pendulum model with a hit-receiving enemy work; a shot swings it, rings it (reported to
// pG for 90 frames) and setBreak() lets it fall.
class cObjBell : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  BellWork

    virtual void move();

    void setBreak();
    int ckBreakEnable();
    int ckBreak();
};

#define BELL_WK(o) ((BellWork*) (o)->free)

// game/obj14.cpp: creates the bell object (st2 r218).
cObj* SetObjBell(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
