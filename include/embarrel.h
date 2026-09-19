#ifndef EMBARREL_H
#define EMBARREL_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cSat;

// One entry of the room "EMI" data (pG->pRoomEmi): 0x40 bytes, entries start at +8.
struct EmiEntry {
    u8 type;              // 0x00  6 = rolling barrel / rock route point, 7 = rock escape goal, 8 = rock start trigger, 3 = rock event trigger
    u8 sub;               // 0x01  sub type (emrock: event flag select / escape goal side)
    u8 state;             // 0x02  emrock emRockAtkScrCk: 3 once the trigger fired
    u8 pad_3;
    Vec pos;              // 0x04
    f32 rotY;             // 0x10  facing (em10 hide / goto points: Muku towards the player, em->rot.y on arrival)
    u8 pad_14[0x40 - 0x14];
};

struct EmiData {
    int n;                // 0x00  number of entries
    u8 pad_4[4];
    EmiEntry entry[1];    // 0x08
};

// Work of the barrel enemy (game/embarrel.cpp), overlaid on cEm from 0x3E0.
struct EmBarrelWork {
    u32 Be_flg;            // 0x000 (0x3E0)
    int Timer;            // 0x004 (0x3E4)  frames until the broken barrel is destroyed
    u8 pad_8[0x40 - 8];
    cSat* sat;            // 0x040 (0x420)  runtime collision piece (emBarrelEatSet)
    u8 pad_44[0x50 - 0x44];
    int routeIdx;         // 0x050 (0x430)  current EMI route point (type 6) of the rolling barrel
    EmiEntry* pRoute;     // 0x054 (0x434)
    Vec Roll_spd;              // 0x058 (0x438)  rolling speed
    f32 floorOfs;         // 0x064 (0x444)  barrel radius above the floor (700)
    u8 pad_68[4];
    u8 rollSe;            // 0x06C (0x44C)  rolling sound / burning effect on
    u8 EffKindId;           // 0x06D (0x44D)  EspPullCoreKind at creation (R227 barrel)
    u8 pad_6E[2];
    u32 Seid;            // 0x070 (0x450)  rolling sound handle (SndStop)
    int Se_wait;          // 0x074 (0x454)  frames until the rolling sound is retriggered
    int Bomb_wait;        // 0x078 (0x458)  frames until the explosion damage check
    Vec Bomb_pos;          // 0x07C (0x45C)
    f32 Bomb_r;        // 0x088 (0x468)
    u8 Eff_id;               // 0x08C (0x46C)  setEff: effect owner id, 0xFF = none
    u8 Etc_no;             // 0x08D (0x46D)  etc flag index (broken flag)
};

#define EMBARREL_WK(em) ((EmBarrelWork*) (((cEmBarrel*) (em))->free))

// Barrel enemy: explosive barrels (types 0/2) and the rolling burning barrel of room 227 (type 1).
class cEmBarrel : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMBARREL_WK)
    virtual void move();

    void setEff(u8 eff);
};

extern "C" {
cEmBarrel* SetBarrel(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo);
cEmBarrel* SetR227Barrel(Vec* pos, Vec* rot);
void emBarrelDmCk(cEmBarrel* em);
void emBarrelDmCk2(cEmBarrel* em);
void emBarrelSetBreak(cEmBarrel* em, int kind);
void emBarrel_R0_Init(cEmBarrel* em);
void emBarrel_R0_Move(cEmBarrel* em);
void emBarrel_R1_Set(cEmBarrel* em);
void emBarrel_R1_Break(cEmBarrel* em);
void emBarrel_R1_R227Roll(cEmBarrel* em);
int emBarrelSetRollRoute(cEmBarrel* em);
int emBarrelSetRollSpd(cEmBarrel* em);
void emBarrelSetBomb(cEmBarrel* em);
void emBarrelSetBomb2(cEmBarrel* em);
void emBarrelEatSet(cEmBarrel* em);
int emBarrelRollHitCk(cEmBarrel* em);
void emBarrelRunDownCk(cEmBarrel* em);
}

#endif
