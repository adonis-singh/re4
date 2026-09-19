#ifndef EM30_H
#define EM30_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "pl_cloth.h"

class cObj;
class cModelInfo;

// Four parasite heads (obj16) hung on one em30 head object.
struct Em30Parasite {
    cObj* p[5];           // 0x00  parts 0x16, 0x17, 0x18, 0x19 (SetObj16 type 0xF); [4] unused
};

// Work of the em30 enemy (em30 module, D:/Bio4/Prog/em30.cpp), overlaid on cEm from 0x3E0.
struct Em30Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: route to the player found, bit1: partner present, bit2: targets the partner, bit3: damage / die routine, bit4: neck follows the target
    int timer;            // 0x004 (0x3E4)
    u8 pad_8[4];
    YARARE_INFO hit[3];     // 0x00C (0x3EC)  extra hit boxes (YarareAdd)
    u8 pad_A8[0x214 - 0xA8];
    f32 routeAng;         // 0x214 (0x5F4)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x218 (0x5F8)
    f32 subAng;           // 0x21C (0x5FC)  the same for the partner
    f32 subAngAbs;        // 0x220 (0x600)
    f32 targetAng;        // 0x224 (0x604)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x228 (0x608)
    f32 targetDist;       // 0x22C (0x60C)
    Vec routePos;         // 0x230 (0x610)  RouteCkToPos result towards the player
    Vec subRoutePos;      // 0x23C (0x61C)  towards the partner
    Vec targetPos;        // 0x248 (0x628)  chosen target position (em30_R1_Walk turns to it)
    cEm* pTarget;         // 0x254 (0x634)  pPL or pSUB
    f32 neckAng;          // 0x258 (0x638)  smoothed neck yaw (em30NeckMove)
    PlCloth cloth1;       // 0x25C (0x63C)  Em30ClothSet1 / Em30ClothMove1
    PlCloth cloth2;       // 0x2BC (0x69C)  Em30ClothSet2 / Em30ClothMove2
    u8 pad_31C[0x37C - 0x31C];
    cModelInfo* pInfo0;   // 0x37C (0x75C)  extra model infos (be_flag bit3 cleared when flags_3C8 bit31)
    cModelInfo* pInfo1;   // 0x380 (0x760)
    cObj* pHead[3];       // 0x384 (0x764)  head objects (SetObj16 type 0xE, parts 3)
    Em30Parasite para[3]; // 0x390 (0x770)  their parasites
};

#define EM30_WK(em) ((Em30Work*) (((cEm30*) (em))->free))

class cEm30 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM30_WK)
    virtual void move();
};

void Em30Init(cEm* em);
int em30DmCk(cEm30* em);   // returns the weapon id when it was 0x21 and the enemy survived (dead value)
void em30RouteCk(cEm30* em);
void em30NeckMove(cEm30* em);
void em30SetParasite(cEm30* em, int no);

#endif
