#ifndef OBJ10_H
#define OBJ10_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Thrown weapon item work (game/obj10.cpp `cWepItem`): the grenade layout (Obj01Work) with the
// landing SE counters split out.
struct WepItemWork {
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
    u32 eff_action;             // 0x70  0 plain, 1 water bomb, 2 explosive
    int release_timer;        // 0x74  frames until it leaves the holder's hand
    u8 Bound_se_ck;            // 0x78  bounce SEs left to play
    u8 se_count;             // 0x79  bounce SEs played
    u8 pad_7A[2];
    u32 flag;          // 0x7C  bit3: water splash done
};

// Thrown weapon item (bottle / explosive): the grenade (obj01) flight model with its own
// landing sounds, a player hit check on the explosion and no flash / underwater variants.
class cWepItem : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  WepItemWork

    virtual void move();
    virtual void beginEvent(u32 mode);
    virtual ~cWepItem() {}
    void move00();
    void move01();
    void dmgSet(int kind);
    void hitCkPl();
};

#define WEPITEM_WK(o) ((WepItemWork*) (o)->free)

#endif
