#ifndef EM2A_H
#define EM2A_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"

class cCtrl;

// Work of the em2a enemy (em2a module, D:/Bio4/Prog/em2a.cpp), overlaid on cEm from 0x3E0.
struct Em2aWork {
    u32 flags;            // 0x000 (0x3E0)  bits 0-3 cleared every frame
    int camTimer;         // 0x004 (0x3E4)  Trap1Bite: frames the trap camera runs; Trap2Bomb: frames until the blast
    int biteTimer;        // 0x008 (0x3E8)  Trap1Bite: frames the catch motion is driven
    u8 pad_C[0x50 - 0xC];
    YARARE_INFO hit[6];     // 0x050 (0x430)  extra hit boxes of the tripwire types (em2aYarareInit)
    u8 pad_188[0x258 - 0x188];
    cCtrl* pCtrl11;       // 0x258 (0x638)  GetCtrlCtrl11()
    cCtrl* pCtrl12;       // 0x25C (0x63C)  GetCtrlCtrl12()
    u8 pad_260[4];
    u32 espKind;          // 0x264 (0x644)  EspPullCoreKind at init (the low byte is the effect owner)
    CAMERA cam;           // 0x268 (0x648)  bear trap bite camera (em2aTrap1CamMove)
};

#define EM2A_WK(em) ((Em2aWork*) (((cEm2a*) (em))->free))

// The traps: type 0 is the bear trap (bites the player or the partner), types 1 and 2 are the
// tripwire bombs.
class cEm2a : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2A_WK)
    virtual void move();
};

void Em2aInit(cEm* em);
void em2aDmCkTrap1(cEm2a* em);
void em2aDmCkTrap2(cEm2a* em);
void plem2aTrapCamMove(cModel* m);
void em2aYarareInit(cEm2a* em);
int em2aTrap2HitCk(cEm2a* em);
int em2aTrap2HitCkPL(cEm2a* em);
int em2aTrap2HitCkEM(cEm2a* em);
void em2aTrap2Bomb(cEm2a* em);
void em2aTrap1CamMove(cEm2a* em);
int em2aTrap1BiteCk(cEm2a* em);
int em2aTrap1BiteSubCk(cEm2a* em);

#endif
