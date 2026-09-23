#ifndef OBJSUBWEP_H
#define OBJSUBWEP_H

#include "types.h"
#include "vec.h"
#include "obj.h"

class cSubWep : public cObj {
public:
    u32 effType;          // 0x328  landing effect (AtEffInfo pair by type; 0xD2 = none)
    u8 effId;             // 0x32C  (PS2 EST_ID effId)
    u8 pad_32D[3];
    s32 effFlag;          // 0x330  AtEffInfo::flags of the hit (bit31 set when known, bit0: solid ground)
    u32* pMot;            // 0x334
    u32 mot_attr;         // 0x338
    Vec rot_spd;          // 0x33C
    Vec spd;              // 0x348
    f32 gravity;          // 0x354
    f32 r;                // 0x358  bounce radius
    int timer;            // 0x35C  frames until the explosion (-1: only on impact)
    class cEm* pEmP;      // 0x360
    u32 parts_no;         // 0x364
    Vec offset;           // 0x368
    Vec ang0;             // 0x374
    u32 eff;              // 0x380
    u32 est;              // 0x384
    u32 eff2;             // 0x388
    u32 est2;             // 0x38C
    u32 eff3;             // 0x390
    u32 est3;             // 0x394
    u32 eff4;             // 0x398
    u32 est4;             // 0x39C
    s32 release_timer;    // 0x3A0
    u8 Bound_se_ck;       // 0x3A4  (ctor: 3)
    u8 se_count;          // 0x3A5  floor bounce SEs played
    u8 se_count_w;        // 0x3A6  wall bounce SEs played
    u8 Flag;              // 0x3A7  bit0: explodes on the floor (fire / light), bit1: egg, bit4: hit a wall

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


#endif
