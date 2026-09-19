#ifndef PL0F_H
#define PL0F_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "pendulum.h"

class cObjChain;
class cPl0f;

// pl0f module (D:/Bio4/Prog/pl0f.cpp): the lake boat of the Del Lago fight (em2f). A cEm the room
// script places (setPos), the player rides (pl0fRideActEvtCk -> PlBoatMove / plboat_R2_*) and steers
// with the tiller (setTiller from the player routine); the boss pulls it by the anchor rope
// (pl0fBoatChaseBoss) and the player throws the harpoons (plboat_R2_SpearSet / SpearThrow).

// One end of the boat: a point mass on the water (pl0fBoatControl moves both and keeps them at
// their rest distance, the boat's position and heading come from them).
struct Pl0fNode {
    int fixed;        // 0x00  1: pulled towards fixPos (crash adjust)
    Vec pos;          // 0x04  rest offset in the boat's frame
    Vec wpos;         // 0x10  world position
    Vec wposOld;      // 0x1C  ... of the previous frame
    Vec spd;          // 0x28
    Vec fixPos;       // 0x34  pull target (pl0fCrashAdjustSet)
    f32 maxLen;       // 0x40  max distance from fixPos
    f32 dist[2];      // 0x44  rest distance to the other node
};

// Work of the boat, overlaid on cEm from 0x3E0.
struct Pl0fWork {
    u32 Be_flg;        // 0x000 (0x3E0)  bit0: player on board, bit1: engine SE running, bit2/3: no crash / drop checks
    int Timer;        // 0x004
    int Timer2;       // 0x008
    int Timer3;          // 0x00C  frame counter of the R10d / R10e entrances (wake effect every 2nd frame)
    cEm* pBoss;       // 0x010  Del Lago (testSearchEm2f)
    Vec swayAmp;      // 0x014  roll sway amplitude (x / z used), decays by 0.96 per frame
    Vec swayPhase;    // 0x020
    u8 pad_2C[0x10];
    f32 rotSpd;       // 0x03C  tiller turn per frame
    f32 rollPhase;    // 0x040
    f32 Bank_sin;   // 0x044
    f32 Roll_rot;         // 0x048
    f32 Bank_rot;        // 0x04C
    f32 Vib_sin;          // 0x050
    f32 Boat_spd;        // 0x054  |pos - oldPos| in the XZ plane
    f32 Boat_dir;       // 0x058  heading change towards the movement direction
    f32 Boat_rot;    // 0x05C
    int Sailing_timer;    // 0x060  frames the engine ran (SE 8/0xB after 60, 8/0x10 before)
    int Ripple_wait;        // 0x064  0x1D countdown outside rooms 10D / 10E (wave effect)
    u32 Seid_engine;         // 0x068  engine SE handle
    u8 EffKindId;       // 0x06C  EspPullCoreKind at creation
    u8 First_camck;           // 0x06D  plboat_R2_Swim: first swim after the drop
    u8 Boss_chase;      // 0x06E  1 while the boss pulls the boat (camera / anchor)
    u8 anchorEff;     // 0x06F  anchor rope effect state (pl0fAnchorEffMove)
    Vec hist[10];     // 0x070  boss position history (pl0f_R0_Move)
    u32 histIdx;      // 0x0E8
    u32 Tiller;       // 0x0EC  setTiller bits: 1 forward, 2 back, 4 left, 8 right
    Vec Getoff_pos;    // 0x0F0  pl0fGetoffActEvtCk: landing position
    f32 Getoff_dir;    // 0x0FC
    PenCloth Cloth;   // 0x100  long rope pendulum (pl0fLongRopeSet)
    cObjChain* pRope; // 0x160  long rope chain object
    cObj* pAnchor;    // 0x164  anchor object (pl0fSetAnchor)
    Pl0fNode node[2]; // 0x168  bow / stern
    u8 pad_200[0x548 - 0x200];
    cPl0f* pSelf;     // 0x548  testSearchEm2f
};

#define PL0F_WK(em) ((Pl0fWork*) (((cPl0f*) (em))->free))

// game/obj1b.cpp `cObjSpear` (SetSpear in obj1c.cpp): the thrown harpoon.
class cObjSpear : public cObj {
public:
    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setThrow(Vec* dir);
    void setLost();
};

// game/obj1c.cpp `cObj1c`: the floating islands the boat crashes into.
class cObj1c : public cObj {
public:
    void setCrash();
};

cObj* SetSpear(void* bin, void* tpl, Vec* pos, Vec* rot);   // game/obj1c.cpp

class cPl0f : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (PL0F_WK)
    // constructor / destructor implicit (Pl0fInit: new (em) cPl0f())
    virtual void move();
    virtual void setPos(Vec* pos, f32 ang);
    virtual void setBossStart(Vec* pos, f32 ang);
    virtual void stopEngine();
    void setTiller();
    void setTillerFront();
};

void Pl0fInit(cEm* em);
void pl0fBoatControl(cPl0f* em);
void pl0fGetBoatDir(cPl0f* em);
void pl0fWaterEff(cPl0f* em);
void pl0fBoatRoll(cPl0f* em);
void pl0fBoatAddSpd(cPl0f* em, u32 no, Vec* spd);
void pl0fBoatSpdControl(cPl0f* em);
void pl0fBoatChaseBoss(cPl0f* em);
void pl0fRideCamMove(cPl0f* em, f32 rate);
void pl0fGetoffCamMove(cPl0f* em);
void pl0fBossCamMove(cPl0f* em, int hide);
void pl0fHideModeCamSet(cPlayer* pl);
void pl0fHideModeCamMove(cPlayer* pl);
void pl0fBossDieCamSet(cPlayer* pl);
void pl0fBossDieCamMove(cPlayer* pl);
void pl0fRideActEvtCk(cPl0f* em);
void pl0fGetoffActEvtCk(cPl0f* em);
int pl0fCrashCk(cPl0f* em);
void pl0fCrashAdjustSet(cPl0f* em, Vec* pos, int away);
void pl0fScrAdjust(cPl0f* em);
void pl00SetSwimCam(cPlayer* pl);
void pl00SwimCamMove(cPlayer* pl);
void pl00SetChaseCam(cPlayer* pl);
void pl00ChaseCamMove(cPlayer* pl);
void pl00SetDieCam(cPlayer* pl);
void pl00DieCamMove(cPlayer* pl);
void pl00SetDropCam(cPlayer* pl);
void pl00DropCamMove(cPlayer* pl);
void plboatSetSpear(cPlayer* pl);
void plboatBlendMotSet(cPlayer* pl, void* m0, void* m1, void* m2, int a, int b, int c);
void subBlendMotSet(cSubChar* sub, void* m0, void* m1, void* m2, int a, int b, int c);
void plOnBoat(cPlayer* pl);
int testSearchEm2f(cPl0f* em);
void plboatSightCurGet(cPlayer* pl, Vec* out);
void plboatSightCurMove(cPlayer* pl);
void plboatSpearThrow(cPlayer* pl);
void pl0fLongRopeSet(cPl0f* em);
void pl0fSwimPosSet(cPlayer* pl);
void pl0fHidePosSet(cPlayer* pl);
void pl0fBossDiePosSet(cPlayer* pl);
void pl0fSetAnchor(cPl0f* em);
void pl0fSetAnchorEm2f(cPl0f* em);
void pl0fSetAnchorEm2f(cPlayer* pl);   // the anchor rope effects (overload, PlBoatMove)
void subOnBoat(cSubChar* sub, cPl0f* boat);

#endif
