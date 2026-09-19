#ifndef EM2F_H
#define EM2F_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cCtrl;
class cObj16;

// Work of the em2f enemy (em2f module, D:/Bio4/Prog/em2f.cpp): the lake monster. Overlaid on cEm
// from 0x3E0.
struct Em2fWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: route to the player found, bit1: partner present, bit2: targets the partner, bit3: damage / die routine, bit4: surfaced, bit5: mouth open (flip motions), bit6: damaged (DmCk), bit7: rising, bit8: fast (double water effect), bit9: BGM stopped, bit10: tentacles out
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)
    YARARE_INFO hit[7];     // 0x00C (0x3EC)  extra hit boxes (em2f_R0_Init)
    u8 pad_178[0x41C - 0x178];
    f32 x41C;             // 0x41C (0x7FC)  Swim: 5000
    int x420;             // 0x420 (0x800)  Swim: 10
    int x424;             // 0x424 (0x804)
    u8 pad_428[0x4F8 - 0x428];
    f32 routeAng;         // 0x4F8 (0x8D8)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x4FC (0x8DC)
    f32 subAng;           // 0x500 (0x8E0)  the same for the partner
    f32 subAngAbs;        // 0x504 (0x8E4)
    f32 targetAng;        // 0x508 (0x8E8)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x50C (0x8EC)
    f32 targetDist;       // 0x510 (0x8F0)
    Vec routePos;         // 0x514 (0x8F4)  RouteCkToPos result towards the player
    Vec subRoutePos;      // 0x520 (0x900)  towards the partner
    Vec targetPos;        // 0x52C (0x90C)  chosen target position
    cEm* pTarget;         // 0x538 (0x918)  pPL or pSUB
    Vec nextPos;          // 0x53C (0x91C)  next route point (em2fSetNextRoute / em2fChangeRoute)
    cEm* pBoat;           // 0x548 (0x928)  the boat enemy the death routine releases
    Vec camPos;           // 0x54C (0x92C)  em2fCriCamMove: camera position / target the cut blends to
    Vec camAt;            // 0x558 (0x938)
    cCtrl* pCtrl12;       // 0x564 (0x944)  GetCtrlCtrl12()
    cObj16* pTentacle[6]; // 0x568 (0x948)  tentacle objects (em2fTentacleMove)
    f32 x580;             // 0x580 (0x960)  pos.y at creation
    f32 x584;             // 0x584 (0x964)
    u8 pad_588[4];
    f32 waterY;           // 0x58C (0x96C)  water surface height (pos.y at creation)
    f32 dive;             // 0x590 (0x970)  Swim: height left to dive / surface
    int effTimer1;        // 0x594 (0x974)  em2fWaterEffSet: underwater effect interval
    int effTimer2;        // 0x598 (0x978)  surfaced effect interval
    int effTimer3;        // 0x59C (0x97C)  surfaced effect interval (fast variant)
    u8 pad_5A0[4];
    int waitTimer;        // 0x5A4 (0x984)  frames before the next attack decision (move decrements it)
    int routeIdx;         // 0x5A8 (0x988)  current EMI route point (-1 = none)
    int atkCnt;           // 0x5AC (0x98C)  attacks left before the hide mode
    int seTimer1;         // 0x5B0 (0x990)  move: swim SE intervals
    int seTimer2;         // 0x5B4 (0x994)
    int seTimer3;         // 0x5B8 (0x998)
    u32 sndId1;           // 0x5BC (0x99C)  swim SE handles (SndStop)
    u32 sndId2;           // 0x5C0 (0x9A0)
    int seTimer4;         // 0x5C4 (0x9A4)  em2fTentacleMove SE interval
    u8 espKind;           // 0x5C8 (0x9A8)  EspPullCoreKind at creation (the effect owner)
    u8 x5C9;              // 0x5C9 (0x9A9)
    u8 x5CA;              // 0x5CA (0x9AA)
    u8 x5CB;              // 0x5CB (0x9AB)
    u8 routeType;         // 0x5CC (0x9AC)  EMI route point sub type of the current point
    u8 nextRouteType;     // 0x5CD (0x9AD)  the same of the next point
    u8 hideRush;          // 0x5CE (0x9AE)  HideMode: damaged while hiding (rush out)
    u8 risingOk;          // 0x5CF (0x9AF)  RisingDragon / Packman may start (em2fRisingDragonCk)
    u8 rndFlag;           // 0x5D0 (0x9B0)  Rnd() & 1 at creation, flipped by em2fRisingDragonCk
};

#define EM2F_WK(em) ((Em2fWork*) (((cEm2f*) (em))->free))

class cEm2f : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2F_WK)
    virtual void move();
};

void Em2fInit(cEm* em);
void em2fDmCk(cEm2f* em);
void em2fCriCamMove(cEm2f* em);
void em2fRouteCk(cEm2f* em);
void em2fWaterEffSet(cEm2f* em);
int em2fSetNextRoute(cEm2f* em);
void em2fSetPosBetweenBoat(cEm2f* em);
void em2fSetPosRisingD(cEm2f* em);
void em2fSetPosPackman(cEm2f* em);
void em2fSetPosHideMode(cEm2f* em);
void em2fSetPosDie(cEm2f* em);
int em2fRisingDragonCk(cEm2f* em);
void em2fChangeRoute(cEm2f* em);
void em2fIslandCrashCk(cEm2f* em);
void em2fTentacleMove(cEm2f* em);

#endif
