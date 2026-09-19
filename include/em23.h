#ifndef EM23_H
#define EM23_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cCtrl;
class cModelInfo;

// Work of the em23 enemy (em23 module, D:/Bio4/Prog/em23.cpp), overlaid on cEm from 0x3E0.
struct Em23Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: cleared every frame, bit1: moved this frame, bit2: dead (fade)
    int timer;            // 0x004 (0x3E4)  frames left in the current routine step
    int turnTimer;        // 0x008 (0x3E8)  R1_Turn: frames until the turn direction flips
    int turnDir;          // 0x00C (0x3EC)  R1_Turn: 1 = turn the other way
    f32 targetAng;        // 0x010 (0x3F0)  takeoff heading; R1_Dm_Air: the height at the last frame
    Vec spd;              // 0x014 (0x3F4)  local flight speed (em23AddSpeedAir rotates it by the heading)
    f32 x20;              // 0x020 (0x400)
    f32 x24;              // 0x024 (0x404)
    u8 pad_28[0x3C - 0x28];
    f32 x3C;              // 0x03C (0x41C)  1e8 at init
    u8 pad_40[0x64 - 0x40];
    cEm* pCorpse;         // 0x064 (0x444)  the corpse the crow lands on (set by the room)
    f32 dmRotSpd;         // 0x068 (0x448)  R1_Dm_Air: spin per frame
    f32 dmAng;            // 0x06C (0x44C)  R1_Dm_Air: heading the fall speed is applied along
    f32 flyHeight;        // 0x070 (0x450)  height above the player the crow flies at
    int moveTimer;        // 0x074 (0x454)  frames since the position last changed (10 when it did)
    int stateTimer;       // 0x078 (0x458)  Rnd() % 300 + 150 at takeoff
    u8 wing;              // 0x07C (0x45C)  em23SetWing: 1 = spread wing model, 0 = folded, 0xFF = none yet
    u8 pad_7D[3];
    cCtrl* pCtrl12;       // 0x080 (0x460)  GetCtrlCtrl12()
    cCtrl* pCtrl11;       // 0x084 (0x464)  GetCtrlCtrl11()
    cModelInfo* pWingInfo;   // 0x088 (0x468)  the wing model info (em23SetWing)
};

#define EM23_WK(em) ((Em23Work*) (((cEm23*) (em))->free))

class cEm23 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM23_WK)
    virtual void move();
};

void Em23Init(cEm* em);
void em23DmCk(cEm23* em);
int em23AddSpeedAir(cEm23* em, f32 ang);
void em23SetWing(cEm23* em, int on);

#endif
