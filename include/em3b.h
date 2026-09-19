#ifndef EM3B_H
#define EM3B_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the em3b enemy (em3b module, D:/Bio4/Prog/em3b.cpp: the truck (type 0) and the mine carts
// (type 1 running, 2 stopped)), overlaid on cEm from 0x3E0.
struct Em3bWork {
    u32 flags;            // 0x000 (0x3E0)  the low 5 bits are cleared every frame
    int timer;            // 0x004 (0x3E4)
    u8 pad_8[4];
    YARARE_INFO hit;        // 0x00C (0x3EC)  second hit cube of the truck
    u8 pad_40[0x258 - 0x40];
    int seTimer;          // 0x258 (0x638)  frames until the next horn SE (R1_Truck_Run)
    int dmgWait;          // 0x25C (0x63C)  frames the vehicle stays invulnerable / burning (ckFire)
    u8 espKind;           // 0x260 (0x640)  EspPullCoreKind at creation
    u8 pad_261[3];
    u32 sndId2;           // 0x264 (0x644)  SndCall handle of the cart SE
    u32 sndId;            // 0x268 (0x648)  SndCall handle of the truck SE
    cEm* pDriver;         // 0x26C (0x64C)  the driver enemy (set by the room)
};

#define EM3B_WK(em) ((Em3bWork*) (((cEm3b*) (em))->free))

class cEm3b : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM3B_WK)
    virtual void move();
    virtual int ckFire();
};

void Em3bInit(cEm* em);
void em3bDmCkTruck(cEm3b* em);
void em3bDmCkCart(cEm3b* em);
void em3bDmCkStopCart(cEm3b* em);
void em3bRunDownCkTruck(cEm3b* em);
void em3bRunDownCkCart(cEm3b* em);
void em3bSlopeMove(cEm3b* em);

#endif
