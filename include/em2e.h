#ifndef EM2E_H
#define EM2E_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the em2e enemy (em2e module, D:/Bio4/Prog/em2e.cpp), overlaid on cEm from 0x3E0.
struct Em2eWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: foot swing direction, bit1: wall mode (routines 3..5)
    int timer;            // 0x004 (0x3E4)  frames left in the current routine
    int turnDir;          // 0x008 (0x3E8)  R1_Turn / R1_W_Turn: 1 = turn left
    Vec nrm;              // 0x00C (0x3EC)  surface normal the wall mode stands on (0, 1, 0 at init)
    f32 footAng;          // 0x018 (0x3F8)  swing angle of parts 2 / 3 (em2eFootMove)
};

#define EM2E_WK(em) ((Em2eWork*) (((cEm2e*) (em))->free))

class cEm2e : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2E_WK)
    virtual void move();
};

void Em2eInit(cEm* em);
void em2eDmCk(cEm2e* em);
void em2eFootMove(cEm2e* em);
void em2eSetWallMatrix(cEm2e* em);

#endif
