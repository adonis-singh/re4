#ifndef OBJ08_H
#define OBJ08_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Thrown / shot object work (game/obj08.cpp): a projectile with gravity, scenario / enemy /
// player hit checks and up to four effect sets.
struct Obj08Work {
    u32 be_flag;            // 0x00  bit0 start motion, bit1 motion running, bit3 rotate, bit4 enemy hit check, bit5 player hit check
    void* pMot;             // 0x04
    u8 pad_8[2];
    u16 motPrm;             // 0x0A
    Vec rot_spd;            // 0x0C
    Vec spd;                // 0x18
    f32 gravity;            // 0x24
    f32 r;                  // 0x28  hit radius (min 1.0)
    cModel* pEm;            // 0x2C  thrower (its id goes to SndCall)
    int timer;              // 0x30  frames left (-1 = forever)
    EmAtkInfo* pAtk;        // 0x34  attack record for EmAtkHitCk
    u32 wep_id;             // 0x38  low 16 bits: GetWepTargetList flag, low byte: damage kind
    u32 eff1;               // 0x3C  ?
    u32 eff2;               // 0x40  scenario hit / timeout
    u32 eff3;               // 0x44  floor hit
    u32 eff4;               // 0x48  enemy / player hit
    u32 est1;               // 0x4C
    u32 est2;               // 0x50
    u32 est3;               // 0x54
    u32 est4;               // 0x58
    u16 blk_no;             // 0x5C  hit SE block number (0xFFFF = none)
    u16 call_no;            // 0x5E
    u8 hit_type;            // 0x60  1: the enemy-hit effect follows the target instead of the hit point
};

// Thrown object (bottle, dynamite, ...): flies under gravity, optionally spinning, and checks
// the scenario, the enemies and the player for hits.
class cObj08 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj08Work

    virtual void move();
};

#define OBJ08_WK(o) ((Obj08Work*) (o)->free)

#endif
