#ifndef OBJ01_H
#define OBJ01_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// game/obj01.cpp: the thrown flame bottle / dynamite (the rooms throw them from Ganado positions).
cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* v, f32 a, f32 b, int c, int d);
void Obj01SetEst(cObj* pObj, u32 eff, u32 est, u32 action, u32 eff2, u32 est2, u32 eff3, u32 est3, u32 eff4, u32 est4);

// Grenade work (game/obj01.cpp): flies under gravity, bounces off the scenario, can be held by a
// model (follows its parts) and explodes / lands in water after `life` frames.
struct Obj01Work {
    u32 be_flag;            // 0x00  bit0 start motion, bit1 motion running, bit2 water / bounce check, bit3 rotate parts 0
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rot_spd;           // 0x0C
    Vec spd;              // 0x18
    f32 gravity;             // 0x24
    f32 r;              // 0x28  bounce radius
    int timer;             // 0x2C  frames until the explosion (0 = now)
    cModel* pEm;           // 0x30  model holding it (follows `holdParts`)
    int parts_no;        // 0x34
    Vec offset;          // 0x38
    Vec ang;          // 0x44
    int eff;           // 0x50  explosion effects (-1 = none: fade out instead)
    int est;          // 0x54
    int eff2;           // 0x58
    int est2;          // 0x5C
    int eff3;           // 0x60  water splash
    int est3;          // 0x64
    int eff4;           // 0x68  underwater explosion
    int est4;          // 0x6C
    u32 eff_action;             // 0x70  0 plain, 1 hand grenade, 2 incendiary, 3 flash, 4 ?
    int release_timer;        // 0x74  frames until it leaves the holder's hand
    u8 Bound_se_ck;            // 0x78  landing SE state
    u8 pad_79[3];
    u32 flag;          // 0x7C  bit3: water splash done
};

// Grenade (hand / incendiary / flash): thrown under gravity, bounces off the scenario, explodes
// or drowns when its fuse runs out; can be held by a model until `holdTimer` expires.
class cObj01 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj01Work

    virtual void move();
    virtual ~cObj01() {}
    void move00();
    void move01();
    void dmgSet(int kind);
};

#define OBJ01_WK(o) ((Obj01Work*) (o)->free)

#endif
