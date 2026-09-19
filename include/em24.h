#ifndef EM24_H
#define EM24_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the em24 enemy (em24 module, D:/Bio4/Prog/em24.cpp), overlaid on cEm from 0x3E0.
struct Em24Work {
    u32 Be_flg;            // 0x000 (0x3E0)  bit2: box mode (checkAir instead of the floor), bit4: die fade done, bit5: in water
    int Timer;            // 0x004 (0x3E4)
    int turnTimer;        // 0x008 (0x3E8)  R1_Free: frames until the next random turn
    u8 pad_C[0x20 - 0xC];
    int motEnd;           // 0x020 (0x400)  MotionMove reported the end of the motion
    u8 pad_24[0x30 - 0x24];
    YARARE_INFO hit[5];     // 0x030 (0x410)  extra hit boxes (YarareAdd)
    u8 pad_134[0x238 - 0x134];
    Vec spd;              // 0x238 (0x618)  jump speed (R1_BoxWait)
    u8 pad_244[0x294 - 0x244];
    Vec slopeRot;         // 0x294 (0x674)  smoothed slope rotation (em24SlopeMove)
    f32 Target_dir;        // 0x2A0 (0x680)  wander direction (R1_Free)
    int HoseiCnt;         // 0x2A4 (0x684)  frames the enemy moved less than half its speed
    int Water_eff_wait;      // 0x2A8 (0x688)  water splash effect interval
    u8 Atk_ck;            // 0x2AC (0x68C)  the attack already hit (em24AtkCk)
};

#define EM24_WK(em) ((Em24Work*) (((cEm24*) (em))->free))

class cEm24 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM24_WK)
    virtual void move();
};

void Em24Init(cEm* em);
void em24DmCk(cEm24* em);
int em24AtkCk(cEm24* em, Vec* a, Vec* b, int no);
void em24SlopeMove(cEm24* em);

#endif
