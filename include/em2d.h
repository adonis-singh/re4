#ifndef EM2D_H
#define EM2D_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"
#include "embarrel.h"

class cCtrl;

// Work of the em2d enemy (em2d module, D:/Bio4/Prog/em2d.cpp): the camouflaged insect that walks
// on walls and ceilings (cModel::type 0..4; type 4 is the flying variant), overlaid on cEm from
// 0x3E0. Field names are the work-relative offsets; the comment gives the cEm offset.
struct Em2dWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: route to the player found, bit4: jump attack hit, bit5: kick / nest wait,
                          //                bit7: wall (type 2 start), bit8: ground routine, bit9: on the ground (Wait / Walk), bit10: in the air (motion event),
                          //                bit11: on the wall / flying, bit12: on the ceiling, bit13: attack count over 450, bit15: returning home,
                          //                bit16: attack routine, bit19: in water, bit21: near the floor, bit29: no side attacks
    int timer;            // 0x004 (0x3E4)
    int timer8;           // 0x008 (0x3E8)
    int walkMode;         // 0x00C (0x3EC)  em2d_R1_Walk: 0 approach, 1 / 2 keep the distance (jump attack / poison)
    f32 turnAng;          // 0x010 (0x3F0)  em2d_R1_Turn180 target yaw; the A_ routines: hover amplitude
    Vec hoverSpd;         // 0x014 (0x3F4)  A_ routines: per-frame phase steps of the hover
    YARARE_INFO hit[15];    // 0x020 (0x400)  extra hit boxes (YarareAdd in em2d_R0_Init)
    f32 routeAng;         // 0x32C (0x70C)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x330 (0x710)
    u8 pad_334[8];
    f32 targetAng;        // 0x33C (0x71C)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x340 (0x720)
    f32 targetDist;       // 0x344 (0x724)
    f32 plDist;           // 0x348 (0x728)  RouteCkPosToPosDis (straight line for type 1) to the player
    f32 homeDist;         // 0x34C (0x72C)  the same from the home position
    Vec routePos;         // 0x350 (0x730)  RouteCkToPos result towards the player
    u8 pad_35C[0xC];
    Vec targetPos;        // 0x368 (0x748)  chosen target position
    cPlayer* pTarget;     // 0x374 (0x754)
    f32 jumpAng;          // 0x378 (0x758)  yaw the jump-down / wall-over turns to (Muku2)
    Vec homePos;          // 0x37C (0x75C)  position the enemy returns to (em2dInitRtnSet)
    Vec startPos;         // 0x388 (0x768)  pos at init
    Vec startRot;         // 0x394 (0x774)  rot at init
    u8 startSet;          // 0x3A0 (0x780)  x38D at init
    u8 pad_3A1[3];
    int startSet2;        // 0x3A4 (0x784)  x38D at init
    f32 plDir;            // 0x3A8 (0x788)  em2dGetPlDir: yaw towards the player's head
    f32 plDirAbs;         // 0x3AC (0x78C)
    Camera cam;           // 0x3B0 (0x790)  catch / death camera (em2dCamMove installs it as CamCtrl.x250)
    Vec spd;              // 0x4A8 (0x888)  movement per frame
    u8 pad_4B4[0xC];
    Vec hoverPhase;       // 0x4C0 (0x8A0)  A_ routines: sine phases of the hover
    int humTimer;         // 0x4CC (0x8AC)  em2dHumSeMove
    int x4D0;             // 0x4D0 (0x8B0)
    int poisonTimer;      // 0x4D4 (0x8B4)  frames the poison weapon damage is suppressed
    int dmGuard;          // 0x4D8 (0x8B8)  DmgMgr hit guard timer
    int catchGuard;       // 0x4DC (0x8BC)  frames the collision stays off after a catch
    int atkWait;          // 0x4E0 (0x8C0)  frames until the next attack
    int jumpWait;         // 0x4E4 (0x8C4)  frames until the next jump attack
    int poisonWait;       // 0x4E8 (0x8C8)  frames until the next poison attack
    int atkCnt;           // 0x4EC (0x8CC)  frames spent walking (over 450: bit13)
    u32 stuckCnt;         // 0x4F0 (0x8D0)  frames the enemy moved less than half of the intended distance
    int dmgTotal;         // 0x4F4 (0x8D4)  damage since the last reaction
    f32 Compress_y;             // 0x4F8 (0x8D8)  em2dScaleCompress: scale.y factor, fades to 0.1 in Die_Lost (vendor name as em10/em2b)
    Vec wallNrm;          // 0x4FC (0x8DC)  normal of the wall / ceiling the enemy stands on ((0, 1, 0) on the floor)
    int lockCnt;          // 0x508 (0x8E8)  frames the player has been locked on (em2dLockCk)
    int x50C;             // 0x50C (0x8EC)
    u32 sndId;            // 0x510 (0x8F0)  poison SE handle
    f32 waterH;           // 0x514 (0x8F4)  water height (-99999 below the floor when none)
    cCtrl* pCtrl12;       // 0x518 (0x8F8)  GetCtrlCtrl12()
    cCtrl* pCtrl11;       // 0x51C (0x8FC)  GetCtrlCtrl11()
    Vec wallTarget;       // 0x520 (0x900)  em2d_R1_W_Walk: point the wall walk turns to
    u8 kickSide;          // 0x52C (0x90C)  Rnd() & 1 (action button variant)
    u8 atkHit;            // 0x52D (0x90D)  the attack hit (em2dAtkCk)
    u8 blendRatio;        // 0x52E (0x90E)  cModelInfo::setBlendRatio
    u8 espKind;           // 0x52F (0x90F)  EspPullCoreKind at creation
    u8 espKind2;          // 0x530 (0x910)
    u8 espKind3;          // 0x531 (0x911)
    s8 effTimer;          // 0x532 (0x912)  frames until the next camouflage effect
    u8 pad_533;
    u8 x534;              // 0x534 (0x914)
    u8 x535;              // 0x535 (0x915)
    u8 Reset_enable;              // 0x536 (0x916)  ckReset: 0 blocks the reset; Die_Lost sets 1 (vendor name as em10)
};

#define EM2D_WK(em) ((Em2dWork*) (((cEm2d*) (em))->free))

class cEm2d : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2D_WK)
    virtual void move();
    virtual int ckFindPL();          // 1 while alive, in battle (status 5) and the player is found
    virtual int ckReset();           // 1 once the die routine finished (the room resets the enemy)
    virtual void setReset(Vec* pos, Vec* rot);   // back to the start (or `pos` / `rot`) with a fresh routine
};

typedef void (*Em2dFunc)(cEm2d*);
typedef void (*PlEm2dFunc)(cPlayer*);

void Em2dInit(cEm* em);
void em2dDmCk(cEm2d* em);
void em2dInitRtnSet(cEm2d* em);
void em2dActEvtSetKick(cEm2d* em, int side);
void em2dRouteCk(cEm2d* em);
int em2dSetDmVal(cEm2d* em);
int em2dAtkCk(cEm2d* em, int no, int parts);
void em2dScaleCompress(cEm2d* em);
int em2dLockCk(cEm2d* em);
void em2dSetWallMatrix(cEm2d* em);
int em2dSetWallMatrix2(cEm2d* em, f32 rate);
void em2dSetFallMatrix(cEm2d* em);
void em2dCamouflageMove(cEm2d* em);
int em2dCatchCk(cEm2d* em);
int em2dAirCatchCk(cEm2d* em);
int em2dFallCatchCk(cEm2d* em);
int em2dCamMove(cEm2d* em, int mode, f32 rate);
void em2dDieCamMove(cEm2d* em);
int em2dStayCk(cEm2d* em);
void em2dSetCrash(cEm2d* em, f32 r);
int em2dCrashCk(cEm2d* em);
void em2dPlHeadLost();
void em2dPlHeadMelt(cPlayer* pl);
void em2dSetPoison(cEm2d* em, int type);
f32 em2dGetPlDir(cEm2d* em, Vec* pos);
int em2dWallWalkCk(cEm2d* em);
int em2dWallFallCk(cEm2d* em);
int em2dPlRunCk(cEm2d* em);
void em2dDoorOpenCk(cEm2d* em);
void em2dFootSeMove(cEm2d* em);
void em2dSetdLandingEff(cEm2d* em);
void em2dSetDownEff(cEm2d* em);
void em2dSetJumpEff(cEm2d* em);
int em2dFallCk(cEm2d* em);
int em2dDownJumpCk(cEm2d* em);
int em2dToCeilingCk(cEm2d* em);
int em2dToAirCk(cEm2d* em);
int em2dToGround(cEm2d* em);
void em2dEyeMove(cEm2d* em);
int em2dScreenInCk(cEm2d* em);
int em2dJumpDownCk(cEm2d* em);
int em2dWallOverCk(cEm2d* em);
void em2dAirNextRtnSet(cEm2d* em);
int em2dFindCk(cEm2d* em);
int em2dSomebodyFindCk(cEm2d* em);
f32 Em2dGetCeiling(cEm2d* em);
int em2dReturnPosCk(cEm2d* em);
void em2dHumSeMove(cEm2d* em);
int em2dNoWallCk(cEm2d* em);

#endif
