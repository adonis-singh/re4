#ifndef OBJSUBWEP_H
#define OBJSUBWEP_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Player sub weapon work (game/objSubWep.cpp `cSubWep`: hand grenade / incendiary / flash / egg).
struct SubWepWork {
    u32 effNo;            // 0x00 (0x328)  landing effect (AtEffInfo pair by type; 0xD2 = none)
    u8 effPrm;            // 0x04 (0x32C)
    u8 pad_5[3];
    s32 attr;             // 0x08 (0x330)  AtEffInfo::flags of the hit (bit31 set when known, bit0: solid ground)
    u8 pad_C[8];
    Vec rotSpd;           // 0x14 (0x33C)
    Vec spd;              // 0x20 (0x348)
    f32 grav;             // 0x2C (0x354)
    f32 rad;              // 0x30 (0x358)  bounce radius
    int life;             // 0x34 (0x35C)  frames until the explosion (-1: only on impact)
    u8 pad_38[0x7C - 0x38];
    u8 x7C;               // 0x7C (0x3A4)  (ctor: 3)
    u8 seCnt0;            // 0x7D (0x3A5)  floor bounce SEs played
    u8 seCnt1;            // 0x7E (0x3A6)  wall bounce SEs played
    u8 flags;             // 0x7F (0x3A7)  bit0: explodes on the floor (fire / light), bit1: egg, bit4: hit a wall
};

class cSubWep : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  SubWepWork

    cSubWep();
    virtual ~cSubWep() {}
    virtual void beginEvent(u32 mode);
    virtual void move();
    virtual void explode() = 0;
    virtual void waterExplode() = 0;
    void moveNormal();
    void moveWater();
    void scrAdjust();
    void dmgSet(int kind);
    void addSpeed();
    void bounce(Vec* nrm);
    int getEffectType();
    int init(Vec* rot, f32 power);
};

#define SUBWEP_WK(o) ((SubWepWork*) (o)->free)

#endif
