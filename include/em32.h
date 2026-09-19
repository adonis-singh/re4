#ifndef EM32_H
#define EM32_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "pl_cloth.h"
#include "camera.h"
#include "model.h"
#include "TexRender.h"

class cCtrl;
class cObj;

// A jump point of the container area (Em32Work::pPoint): the roof / floor position the enemy jumps
// to, `flag` set once the landing effect played, `no` the container it breaks (cEm32::getBreakNo).
struct Em32Point {
    u8 x0;
    u8 x1;
    u8 flag;              // 0x02
    u8 no;                // 0x03
    Vec pos;              // 0x04
};

// Work of the em32 enemy (em32 module, D:/Bio4/Prog/em32.cpp): the caged boss of the container
// area (three forms: `mode` 0 caged / hidden, 1 after the parasite shows, 2 the last form), overlaid
// on cEm from 0x3E0. It shares em2c's floor routines (walk / dash blends, the catch callbacks, the
// step up / down on the containers, the ceiling and tunnel attacks). Field names are the
// work-relative offsets; the comment gives the cEm offset.
struct Em32Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: route found, bit3: damage routine, bit4: yarare mark, bit6: no SatMgr check,
                          //                bit9: player found, bit11: appear / parasite routine, bit12: step up wanted,
                          //                bit14, bit16, bit17, bit18: fading, bit19: fading out, bit20: texture render off
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)  attack: frames the action button prompt stays
    int timer3;           // 0x00C (0x3EC)  attack: frames before the action button shows
    int TmpU32;              // 0x010 (0x3F0)  TunnelAtk: Rnd() & 1 (prompt variant); CatchHit: bite SE handle  scratch (SE handle in CatchHit, Rnd variant in TunnelAtk / Ground) (vendor name as em39)
    u32 sndId2;           // 0x014 (0x3F4)  CatchHit: catch SE handle
    Vec spd;              // 0x018 (0x3F8)  movement left to the step / jump target (a tenth per motion frame)
    f32 turnAng;          // 0x024 (0x404)  AmbushAtk: yaw the swipe turns towards
    YARARE_INFO hit[28];    // 0x028 (0x408)  extra hit boxes (YarareAdd in em32_R0_Init)
    u8 pad_5D8[0x640 - 0x5D8];
    f32 routeAng;         // 0x640 (0xA20)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x644 (0xA24)
    u8 pad_648[8];
    f32 targetAng;        // 0x650 (0xA30)
    f32 targetAngAbs;     // 0x654 (0xA34)
    f32 targetDist;       // 0x658 (0xA38)
    Vec routePos;         // 0x65C (0xA3C)  RouteCkPosToPos result towards the player
    u8 pad_668[0xC];
    Vec targetPos;        // 0x674 (0xA54)
    cPlayer* pTarget;     // 0x680 (0xA60)
    Em32Point* pPoint;    // 0x684 (0xA64)  jump point (em32JumpUpCk / em32GetJumpDownNo)
    u8 breakNo;           // 0x688 (0xA68)  cEm32::getBreakNo (0xFF = none, reset every frame)
    u8 breakNo2;          // 0x689 (0xA69)  cEm32::getBreakNo2
    u8 pad_68A[2];
    Vec plPos;            // 0x68C (0xA6C)  em32GetPlPos: the player's position 18 frames ahead
    f32 plAng;            // 0x698 (0xA78)  Muku towards it
    f32 plAngAbs;         // 0x69C (0xA7C)
    f32 plDist2;          // 0x6A0 (0xA80)  squared XZ distance to it
    Camera cam;           // 0x6A4 (0xA84)  escape camera (em32EscapeCamMove)
    Vec stepPos;          // 0x79C (0xB7C)  step up / down target (em32StepUpCk*, em32GetStepDownPos)
    u8 pad_7A8[0xC];
    Vec stepTarget;       // 0x7B4 (0xB94)  position to face / approach while stepping (flags bit12)
    f32 stepAng;          // 0x7C0 (0xBA0)  yaw to face while stepping (flags bit12)
    int x7C4;             // 0x7C4 (0xBA4)
    f32 blendVal;         // 0x7C8 (0xBA8)  signed blend weight of the two-motion blend (em32BlendMotSet: sign picks the motion)
    int blendCnt;         // 0x7CC (0xBAC)  counts down; its low byte is the MotionSetCore frame argument
    int blendSeq;         // 0x7D0 (0xBB0)  wraps at cModel::frameMax; its low half is the MotionSetCore last argument
    MotionWorkSub blendMot;  // 0x7D4 (0xBB4)  second motion work (cModel::motBlend)
    void* blendM0;        // 0x8A4 (0xC84)  walk blend motions (em32_R1_Walk / Dash: em32BlendMotSet arguments)
    void* blendM1;        // 0x8A8 (0xC88)
    void* blendM2;        // 0x8AC (0xC8C)
    void* blendM3;        // 0x8B0 (0xC90)
    int blendA;           // 0x8B4 (0xC94)
    int blendB;           // 0x8B8 (0xC98)
    int blendC;           // 0x8BC (0xC9C)
    PlCloth cloth;        // 0x8C0 (0xCA0)  tail cloth chain (em32ClothSet)
    MotionWorkSub* pMot;  // 0x920 (0xD00)  mem_alloc'd second motion work (em32_R0_Init)
    cModelInfo* pTexModel;  // 0x924 (0xD04)  the texture-blended model info (ARC(7))
    u8 pad_928[4];
    cObj* pDivide[2];     // 0x92C (0xD0C)  the two halves of the cut player (em32PlDivideSet)
    cObj* pCatchObj;      // 0x934 (0xD14)  the player's weapon model while caught (plem32_P_CatchHit)
    f32 neckAng;          // 0x938 (0xD18)  em32NeckMove: head yaw
    f32 scale;            // 0x93C (0xD1C)  em32ScaleCompress: body scale (the death shrinks it)
    cCtrl* pCtrl12;       // 0x940 (0xD20)  GetCtrlCtrl12()
    TexRenderMng* pTex;   // 0x944 (0xD24)  Ctrl12GetTexRenderEm32
    u8 texBlend[8];       // 0x948 (0xD28)  cModelInfo::setTexBlendTbl table
    u8 pad_950[0x968 - 0x950];
    int wait;             // 0x968 (0xD48)
    int atkWait;          // 0x96C (0xD4C)
    int longAtkWait;      // 0x970 (0xD50)
    int dmgTotal;         // 0x974 (0xD54)  damage since the last reaction
    int x978;             // 0x978 (0xD58)
    int dmGuard;          // 0x97C (0xD5C)  DmgMgr hit guard timer
    u32 stuckCnt;         // 0x980 (0xD60)
    u8 pad_984[2];
    u16 breathTimer;      // 0x986 (0xD66)
    u32 sndId;            // 0x988 (0xD68)  SndCall handle (SndStop)
    u16 voiceTimer;       // 0x98C (0xD6C)
    u8 espKind[3];        // 0x98E (0xD6E)  EspPullCoreKind results
    u8 Atk_ck;              // 0x991 (0xD71)  the attack already hit the player  the attack already hit the player (em32AtkHitSet) (vendor name as em10)
    u8 actionSet;         // 0x992 (0xD72)  player action callback set (em32BackjumpAction)
    u8 mode;              // 0x993 (0xD73)  form: 0, 1 (parasite shown), 2 (last form)
};

#define EM32_WK(em) ((Em32Work*) (((cEm32*) (em))->free))
#define EM32_BLEND_MOT(w) ((MotionWork*) &(w)->blendMot)

class cEm32 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM32_WK)
    virtual void move();
    virtual void setNext(int no);
    virtual int getBreakNo();
    virtual int getBreakNo2();
    virtual int ckDie();
};

typedef void (*Em32Func)(cEm32*);

void Em32Init(cEm* em);
void em32DmCk(cEm32* em);
void em32EscapeCamMove(cEm32* em);
void em32RouteCk(cEm32* em);
void em32NeckMove(cEm32* em);
void em32ClothSet(cEm32* em);
void em32ClothMove(cEm32* em);
void em32BlendMotSet(cEm32* em, void* m0, void* m1, void* m2, void* m3, int a, int b, u16 d);
int em32AtkCk(cEm32* em, int no, int parts);
int em32AtkCk2(cEm32* em, int no, Vec* pos, Vec* oldPos);
int em32StepUpCk(cEm32* em);
int em32StepUpCk2(cEm32* em);
int em32StepUpCk3(cEm32* em);
int em32PlInTunnelCk(cEm32* em);
int em32TunnelAtkCk(cEm32* em);
int em32JumpUpCk(cEm32* em);
int em32JumpDownCk(cEm32* em);
void em32GetJumpDownNo(cEm32* em);
int em32CeilingAtkCk(cEm32* em);
void em32PlHeadLost();
void em32PlHeadFall();
void em32TexrenderInit(cEm32* em);
void em32GetPlPos(cEm32* em);
void em32GetStepDownPos(cEm32* em);
int em32SetDmVal(cEm32* em);
int em32AmbushAtkCk(cEm32* em);
void em32BloodSet(cEm32* em);
void em32PlDivideModelInit(cEm32* em);
void em32PlDivideSet(cEm32* em);
void em32PlDivideSet2(cEm32* em);
void em32RackBreakCk(cEm32* em);
void em32SetYarareMark(cEm32* em, int on);
void em32BreakBarred(cEm32* em);
void em32BodyMove(cEm32* em);
void em32BreathSe(cEm32* em);
void em32BreathSeStopCk(cEm32* em);
void em32ScaleCompress(cEm32* em);
void em32GetGroundPos(cEm32* em);
int em32BetweenHitCk(cEm32* em);

#endif
