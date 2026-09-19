#ifndef EM29_H
#define EM29_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cCtrl;

// Work of the em29 enemy (em29 module, D:/Bio4/Prog/em29.cpp): the bats. Overlaid on cEm from 0x3E0.
struct Em29Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: route to the player found, bit3: damage / die routine, bit4: rushing, bit5: waiting on the ground, bit6: waiting on the ceiling, bit7: killed by the room damage; the low bits (0x6F) are cleared every frame
    int timer;            // 0x004 (0x3E4)
    u8 pad_8[4];
    f32 dmRot;            // 0x00C (0x3EC)  yaw speed while falling (Dm_Air / Dm_Ceiling)
    f32 routeAng;         // 0x010 (0x3F0)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x014 (0x3F4)
    u8 pad_18[8];
    f32 targetAng;        // 0x020 (0x400)  the chosen target's angle / distance
    f32 targetAngAbs;     // 0x024 (0x404)
    f32 targetDist;       // 0x028 (0x408)
    Vec routePos;         // 0x02C (0x40C)  RouteCkToPos result towards the player
    u8 pad_38[0xC];
    Vec targetPos;        // 0x044 (0x424)  chosen target position
    cEm* pTarget;         // 0x050 (0x430)  pPL
    Vec spd;              // 0x054 (0x434)  current speed (local, rotated by rot.y in em29SetSPeed)
    Vec tgtSpd;           // 0x060 (0x440)  speed the current one is blended towards
    Vec initPos;          // 0x06C (0x44C)  position / rotation at creation (Die_Reset restores them)
    Vec initRot;          // 0x078 (0x458)
    f32 grav;             // 0x084 (0x464)  fall speed increment per frame
    int atkTimer;         // 0x088 (0x468)  frames the bat keeps away after an attack / take-off
    int escTimer;         // 0x08C (0x46C)  frames RouteCkEscEm drives the target
    int x90;              // 0x090 (0x470)
    cCtrl* pCtrl11;       // 0x094 (0x474)  GetCtrlCtrl11()
    cCtrl* pCtrl12;       // 0x098 (0x478)  GetCtrlCtrl12()
    u8 pad_9C[4];
    u8 atkHit;            // 0x0A0 (0x480)  the attack already hit (em29AtkCk)
};

#define EM29_WK(em) ((Em29Work*) (((cEm29*) (em))->free))

class cEm29 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM29_WK)
    virtual void move();
};

void Em29Init(cEm* em);
void em29DmCk(cEm29* em);
void em29RouteCk(cEm29* em);
void em29SetSPeed(cEm29* em, f32 rate);
void em29ObaHitCk(cEm29* em);
int em29LastCk(cEm29* em);
int em29FriendCk(cEm29* em);
void em29CallSe(cEm29* em, int type);
int em29AtkCk(cEm29* em, int no);

#endif
