#ifndef EM26_H
#define EM26_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cCtrl;

// Work of the em26 enemy (em26 module, D:/Bio4/Prog/em26.cpp), overlaid on cEm from 0x3E0.
struct Em26Work {
    u32 flags;            // 0x000 (0x3E0)  bit3: damage / die routine, bit4: mirrored motions (0x41 flag), bit5: died in a damage volume
    u8 pad_4[0xC];
    YARARE_INFO hit[2];     // 0x010 (0x3F0)  extra hit boxes (YarareAdd: parts 5 and 0x18)
    u8 pad_78[0x17C - 0x78];
    int estTimer;         // 0x17C (0x55C)  frames between the idle effects
    int dmgTotal;         // 0x180 (0x560)  damage taken since the last attack (> 500 -> R1_Atk)
    u32 sndId;            // 0x184 (0x564)  SndCall handle of the current voice
    int breathTimer;      // 0x188 (0x568)  frames between the breath SEs (em26BreathSe)
    cCtrl* pCtrl12;       // 0x18C (0x56C)  GetCtrlCtrl12()
    cCtrl* pCtrl11;       // 0x190 (0x570)  GetCtrlCtrl11()
    int x194;             // 0x194 (0x574)
    u8 atkHit;            // 0x198 (0x578)  the attack already hit the player (em26AtkCk)
};

#define EM26_WK(em) ((Em26Work*) (((cEm26*) (em))->free))

class cEm26 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM26_WK)
    virtual void move();
};

void Em26Init(cEm* em);
void em26DmCk(cEm26* em);
void em26BreathSe(cEm26* em);
int em26AtkCk(cEm26* em);

#endif
