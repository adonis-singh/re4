#ifndef EM28_H
#define EM28_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cCtrl;

// Work of the em28 enemy (em28 module, D:/Bio4/Prog/em28.cpp: the chicken), overlaid on cEm from 0x3E0.
struct Em28Work {
    u32 flags;            // 0x000 (0x3E0)  bit3: damage / die routine ran, bit4: in the air (checkAir), bit5: item dropped, bit6: dying (parts fade); the low 5 bits are cleared every frame
    int timer;            // 0x004 (0x3E4)
    int turnTimer;        // 0x008 (0x3E8)  R1_Dash: frames until the next random turn
    u8 pad_C[0x15C - 0xC];
    f32 targetAng;        // 0x15C (0x53C)  walk / dash / fly direction
    int stuckCnt;         // 0x160 (0x540)  frames the chicken moved less than half its speed
    int escapeWait;       // 0x164 (0x544)  frames until the next escape check (em28EscapeCk)
    Vec spd;              // 0x168 (0x548)  take-off speed (R1_Jump / R1_Die_Air)
    cCtrl* pCtrl12;       // 0x174 (0x554)  GetCtrlCtrl12()
    cCtrl* pCtrl11;       // 0x178 (0x558)  GetCtrlCtrl11()
    int x17C;             // 0x17C (0x55C)
};

#define EM28_WK(em) ((Em28Work*) (((cEm28*) (em))->free))

class cEm28 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM28_WK)
    virtual void move();
};

void Em28Init(cEm* em);
void em28DmCk(cEm28* em);
int em28EscapeCk(cEm28* em);

#endif
