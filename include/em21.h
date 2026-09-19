#ifndef EM21_H
#define EM21_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the em21 enemy (em21 module, D:/Bio4/Prog/em21.cpp): the dog. Overlaid on cEm from 0x3E0.
struct Em21Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: the player is in sight (Em21RouteCk), bit1: neck follows the target, bit2: wander point set, bit3: heading for the bark point (VsElgigante)
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)  Escape: frames until the next random turn
    f32 routeAng;         // 0x00C (0x3EC)  Muku towards the route point to the player
    f32 routeAngAbs;      // 0x010 (0x3F0)
    u8 pad_14[8];
    f32 targetAng;        // 0x01C (0x3FC)  the same towards the current target
    f32 targetAngAbs;     // 0x020 (0x400)
    f32 targetDist2;      // 0x024 (0x404)
    f32 plDist;           // 0x028 (0x408)  RouteCkPosToPosDis to the player
    Vec routePos;         // 0x02C (0x40C)  RouteCkToPos result towards the player
    u8 pad_38[0xC];
    Vec targetPos;        // 0x044 (0x424)  route point the routines run to
    cEm* pTarget;         // 0x050 (0x430)  pPL, or 0 while heading for a wander / bark point
    cEm* pTrap;           // 0x054 (0x434)  the bear trap the dog is caught in (em21TrapSearch)
    cEm* pGigante;        // 0x058 (0x438)  El Gigante the dog fights (em21SearchElgigante)
    YARARE_INFO hit[5];     // 0x05C (0x43C)  extra hit boxes (em21YarareInit)
    f32 neckX;            // 0x160 (0x540)  em21NeckMove: head pitch
    f32 neckY;            // 0x164 (0x544)  head yaw
    int stuckTimer;       // 0x168 (0x548)  frames the dog barely moved (slow turn)
    int escTimer;         // 0x16C (0x54C)  escaping (Em21RouteCk uses RouteCkEscEm)
    f32 escAng;           // 0x170 (0x550)  Escape: direction to run in
    u32 sndId;            // 0x174 (0x554)  bark / whine SE handle (SndStop)
    f32 tilt;             // 0x178 (0x558)  em21DirMatrix: body roll of the run
    Vec wanderPos;        // 0x17C (0x55C)  em21SetWanderPos
    Vec barkPos;          // 0x188 (0x568)  em21GetBarkPos
};

#define EM21_WK(em) ((Em21Work*) (((cEm21*) (em))->free))

class cEm21 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM21_WK)
    virtual void move();
};

void Em21Init(cEm* em);
void em21DmCk(cEm21* em);
int em21SearchElgigante(cEm21* em);
void plem21TrapCamMove(cModel* m);
void Em21RouteCk(cEm21* em);
void em21DirMatrix(cEm21* em, f32 dir);
void em21NeckMove(cEm21* em);
int em21WakeCk(cEm21* em);
void em21YarareInit(cEm21* em);
void em21EscapeWithYou(cEm21* em);
void em21SetWanderPos(cEm21* em);
int em21TrapSearch(cEm21* em);
int em21GetBarkPos(cEm21* em);

#endif
