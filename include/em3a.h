#ifndef EM3A_H
#define EM3A_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "obj.h"

struct EmiEntry;   // embarrel.h

// Helicopter missile (game/objMissile.cpp): the class is local to that unit; the helicopter
// enemy only calls these two members through SetHeliMissile's result.
class cObjMissile : public cObj {
public:
    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setFire(Vec* target);
};

cObj* SetHeliMissile(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type);

// Work of the em3a enemy (em3a module, D:/Bio4/Prog/em3a.cpp; cModel::type 0/1 = helicopter,
// 2 = the hovering boss variant with the B_ routines), overlaid on cEm from 0x3E0.
struct Em3aWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: player found, bit1: gun attack running, bit2: damage effect set, bit3: hidden (B_HideWait)
    int timer;            // 0x004 (0x3E4)
    u8 pad_8[8];
    int turnDir;          // 0x010 (0x3F0)  B_HideWait: 0 none, 1 left, 2 right
    u8 pad_14[0x20 - 0x14];
    YARARE_INFO hit[4];     // 0x020 (0x400)  extra hit boxes (YarareAddCube)
    u8 pad_F0[0x238 - 0xF0];
    f32 routeAng;         // 0x238 (0x618)  Muku towards the route point
    f32 routeAngAbs;      // 0x23C (0x61C)
    u8 pad_240[0x25C - 0x240];
    Vec routePos;         // 0x25C (0x63C)  RouteCkPosToPos / RouteCkToPos result
    u8 pad_268[4];
    cObj* pMissile;       // 0x26C (0x64C)  missile hung on the helicopter (type 1)
    Vec vibAng;           // 0x270 (0x650)  hover vibration phases (em3aVibMove)
    Vec vibSpd;           // 0x27C (0x65C)  their per-frame increments
    Vec spd;              // 0x288 (0x668)  movement speed
    u32 sndId;            // 0x294 (0x674)  engine sound handle (em3aEngineSe / SndStop)
    int seTimer;          // 0x298 (0x678)  engine sound interval
    int atkWait;          // 0x29C (0x67C)  frames the gun / rocket attack is held off (setAtkWait)
    int lostCnt;          // 0x2A0 (0x680)  frames the player was far away (B_Move)
    int nearCnt;          // 0x2A4 (0x684)  frames the player was near (B_Move)
    struct EmiEntry* pRoute;  // 0x2A8 (0x688)  current patrol point (EMI type 0x13)
    u8 pad_2AC;
    u8 espKind;           // 0x2AD (0x68D)  EspPullCoreKind at creation
};

#define EM3A_WK(em) ((Em3aWork*) (((cEm3a*) (em))->free))

class cEm3a : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM3A_WK)
    virtual void move();
    virtual void setNoSuspend(int on);
    virtual void setAtkWait(int frames);   // defined last in em3a.cpp (precedes the cUnit linkonce copies)
};

void Em3aInit(cEm* em);
void em3aDmCk(cEm3a* em);
void em3aVibMove(cEm3a* em);
void em3aGunMove(cEm3a* em);
int em3aSetDmVal(cEm3a* em);
void em3aPatrolInit(cEm3a* em);
int em3aPatrolUpdate(cEm3a* em);
void em3aFanMove(cEm3a* em);
int em3aFindPLCk(cEm3a* em);
int em3aLookPLCk(cEm3a* em);
int em3aGunHitCk(cEm3a* em);
void em3aRocketFire(cEm3a* em);
int em3aBossCk(cEm3a* em);
int em3aBossNearCk(cEm3a* em);
void em3aEngineSe(cEm3a* em);

#endif
