#ifndef EM2C_H
#define EM2C_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "embarrel.h"
#include "pl_cloth.h"
#include "camera.h"
#include "model.h"
#include "TexRender.h"

class cCtrl;

// Work of the em2c enemy (em2c module, D:/Bio4/Prog/em2c.cpp): the insect boss (cModel::type 0)
// and its tail (type 1), overlaid on cEm from 0x3E0. The flags / routine set follows em2d's
// (floor, wall and ceiling routines, the hide / freeze states are new). Field names are the
// work-relative offsets; the comment gives the cEm offset.
struct Em2cWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: route found, bit3: damage / die routine, bit5: on the wall / ceiling, bit6: in the air,
                          //                bit8: ground routine, bit9: player found, bit10: motion event, bit11: frozen (em2cSetFreeze, the F_ routines), bit12: falling,
                          //                bit13: attack count over 450, bit15: returning, bit16: attack, bit18: no side attacks, bit20: no damage (hidden / Reset_Wait),
                          //                bit21: hidden above the player (HideWait / HideAtk / C_Wait), bit22: no death, bit23: not yet in the room
    int timer;            // 0x004 (0x3E4)
    int timer8;           // 0x008 (0x3E8)
    int walkMode;         // 0x00C (0x3EC)  attack: frames before the action button shows
    int mode;             // 0x010 (0x3F0)  TailAtk: the tail already hit; W_Walk: Rnd() & 3; F_Atk: motion select
    f32 turnAng;          // 0x014 (0x3F4)  Turn180: the angle the model turns towards
    u8 pad_18[0xC];
    YARARE_INFO hit[13];    // 0x024 (0x404)  extra hit boxes (YarareAdd in em2c_R0_Init)
    u8 pad_2C8[0x330 - 0x2C8];
    f32 routeAng;         // 0x330 (0x710)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x334 (0x714)
    u8 pad_338[8];
    f32 targetAng;        // 0x340 (0x720)
    f32 targetAngAbs;     // 0x344 (0x724)
    f32 targetDist;       // 0x348 (0x728)
    f32 plDist;           // 0x34C (0x72C)  RouteCkPosToPosDis to the player
    f32 homeDist;         // 0x350 (0x730)
    Vec routePos;         // 0x354 (0x734)  RouteCkToPos result towards the player
    u8 pad_360[0xC];
    Vec targetPos;        // 0x36C (0x74C)
    cPlayer* pTarget;     // 0x378 (0x758)
    f32 jumpAng;          // 0x37C (0x75C)
    Vec homePos;          // 0x380 (0x760)
    Vec startPos;         // 0x38C (0x76C)  pos at init
    Vec startRot;         // 0x398 (0x778)  rot at init
    u8 pad_3A4[8];
    PlCloth cloth;        // 0x3AC (0x78C)  tail cloth chain (em2cClothSet)
    f32 plDir;            // 0x40C (0x7EC)  em2cGetPlDir towards the player's head
    f32 plDirAbs;         // 0x410 (0x7F0)
    CAMERA cam;           // 0x414 (0x7F4)  escape camera (em2cEscapeCamMove)
    Vec spd;              // 0x50C (0x8EC)  movement per frame
    u8 pad_518[0x18];
    int humTimer;         // 0x530 (0x910)
    int x534;             // 0x534 (0x914)
    int dmGuard;          // 0x538 (0x918)  DmgMgr hit guard timer
    int effTimer;         // 0x53C (0x91C)  frames until the next hidden effect
    int atkWait;          // 0x540 (0x920)  frames until the next attack
    int jumpWait;         // 0x544 (0x924)
    u8 pad_548[4];
    int atkCnt;           // 0x54C (0x92C)  frames spent walking (over 450: bit13)
    u32 stuckCnt;         // 0x550 (0x930)
    int dmgTotal;         // 0x554 (0x934)  damage since the last reaction
    f32 Compress_y;             // 0x558 (0x938)  em2cScaleCompress: scale.y factor (vendor name as em10/em2b)
    Vec wallNrm;          // 0x55C (0x93C)  normal of the wall / ceiling the enemy stands on ((0, 1, 0) on the floor)
    int lockCnt;          // 0x568 (0x948)
    int wakeWait;         // 0x56C (0x94C)  em2c_R1_WakeupWait: Rnd % 30 + 30 before em2cDownJumpCk
    int Dash_wait;             // 0x570 (0x950)  150 after Dash / DoorOpenCk; Walk waits for 0 (vendor name as em10)
    int doorWait;         // 0x574 (0x954)  em2cDoorOpenCk: Rnd % 15 + 15 after hitting a door
    Vec wallTarget;       // 0x578 (0x958)  wall walk target (em2cGetPlDir at the wall walk start)
    f32 Neck_dir_y;             // 0x584 (0x964)  em2cNeckMove: smoothed head yaw -> addRot.y (vendor name as em39)
    int guardCnt;         // 0x588 (0x968)  damage guard: frames (counts down, minus dmg / 6 per hit)
    cEm* pTail;           // 0x58C (0x96C)  the tail unit (em2cGetTail)
    cCtrl* pCtrl12;       // 0x590 (0x970)  GetCtrlCtrl12()
    TexRenderMng* pTex;   // 0x594 (0x974)  Ctrl12GetTexRenderEm2c
    u8 texBlend[8];       // 0x598 (0x978)  cModelInfo::setTexBlendTbl table (em2cSetFreeze)
    u8 pad_5A0[0x18];
    f32 blendVal;         // 0x5B8 (0x998)  signed blend weight of the two-motion blend (em2cBlendMotSet: sign picks the motion)
    int blendCnt;         // 0x5BC (0x99C)  counts down; its low byte is the MotionSetCore frame argument
    int blendSeq;         // 0x5C0 (0x9A0)  wraps at Motion.Seq_frame_num; its low half is the MotionSetCore last argument
    MotionWorkSub blendMot;  // 0x5C4 (0x9A4)  second motion work (cModel::Motion.blend)
    void* blendM0;        // 0x694 (0xA74)  walk blend motions (em2c_R1_Walk / Dash: em2cBlendMotSet arguments)
    void* blendM1;        // 0x698 (0xA78)
    void* blendM2;        // 0x69C (0xA7C)
    int blendA;           // 0x6A0 (0xA80)
    int blendB;           // 0x6A4 (0xA84)
    int blendC;           // 0x6A8 (0xA88)
    int blendD;           // 0x6AC (0xA8C)
    u32 breathSnd;        // 0x6B0 (0xA90)  em2cBreathSe SndCall handle
    u16 breathTimer;      // 0x6B4 (0xA94)
    u8 pad_6B6;
    u8 atkHit;            // 0x6B7 (0xA97)  attack already hit this motion
    u8 actDone;           // 0x6B8 (0xA98)  the player action (kick / sit / backjump callback) fired; no ActBtn.set while set
    u8 pad_6B9;
    u8 espKind2;          // 0x6BA (0xA9A)  EspPullCoreKind at creation
    u8 espKind;           // 0x6BB (0xA9B)
    u8 x6BC;              // 0x6BC (0xA9C)
};

#define EM2C_WK(em) ((Em2cWork*) (((cEm2c*) (em))->free))
#define EM2C_BLEND_MOT(w) ((MotionWork*) &(w)->blendMot)

class cEm2c : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2C_WK)
    virtual void move();
};

typedef void (*Em2cFunc)(cEm2c*);

void Em2cInit(cEm* em);
void em2cDmCk(cEm2c* em);
void em2cTailDmCk(cEm2c* em);
void em2cInitRtnSet(cEm2c* em);
void em2cRouteCk(cEm2c* em);
int em2cSetDmVal(cEm2c* em);
int em2cAtkCk(cEm2c* em, int no, int parts);
void em2cScaleCompress(cEm2c* em);
int em2cLockCk(cEm2c* em);
int em2cSetWallMatrix2(cEm2c* em, f32 rate);
void em2cSetFallMatrix(cEm2c* em);
void em2cPlHeadLost();
f32 em2cGetPlDir(cEm2c* em, Vec* pos);
int em2cWallFallCk(cEm2c* em);
void em2cDoorOpenCk(cEm2c* em);
void em2cDoorOpenCk2(cEm2c* em);
void em2cClothSet(cEm2c* em);
void em2cClothMove(cEm2c* em);
void em2cFootSeMove(cEm2c* em);
void em2cSetdLandingEff(cEm2c* em);
void em2cSetDownEff(cEm2c* em);
void em2cSetJumpEff(cEm2c* em);
int em2cFallCk(cEm2c* em);
int em2cDownJumpCk(cEm2c* em);
int em2cToCeilingCk(cEm2c* em);
int em2cJumpDownCk(cEm2c* em);
int em2cWallOverCk(cEm2c* em);
void em2cNeckMove(cEm2c* em);
void em2cEscapeCamMove(cEm2c* em);
void em2cBlendMotSet(cEm2c* em, void* m0, void* m1, void* m2, int a, int b, int c, int d);
void em2cBlendMotSet2(cEm2c* em, void* m0, void* m1, int a, int b, int d);
void em2cTexrenderInit(cEm2c* em);
void em2cSetFreeze(cEm2c* em);
int em2cNoWallCk(cEm2c* em);
void em2cGetTail(cEm2c* em);
int em2cDoorCk(cEm2c* em);
int em2cFloorTypeCk(cEm2c* em);
int em2cAmbushCk(cEm2c* em);
void em2cGetFallPos(cEm2c* em);
void em2cBreathSe(cEm2c* em);
void em2cBreathSeStopCk(cEm2c* em);

#endif
