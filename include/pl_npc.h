#ifndef PL_NPC_H
#define PL_NPC_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cEmWindow;

// Partner character base (game/pl_npc.cpp, `pSUB`): a cEm with the partner virtuals. Vtable order
// from pl_npc's `cSubChar virtual table`: cUnit/cCoord/cModel/cEm virtuals, then the ones below.
// The cSubChar-only fields (sub*) live in cEm (they sit below 0xDE0, see em.h).
class cSubChar : public cEm {
public:
    cSubChar* subSelf;        // 0x3E0  the model the routines animate (itself)
    int subNeckOn;            // 0x3E4  neckSet() called this frame
    f32 subNeckX;             // 0x3E8
    f32 subNeckAng;           // 0x3EC  current neck angle (parts 3)
    f32 subNeckZ;             // 0x3F0
    Vec subNeckPos;           // 0x3F4  position looked at
    u16 subFlags;             // 0x400  bit7 (0x80) manual control, bit6 (0x40) ok to control, bit4 (0x10), bit3 (0x8) move-to, bit0
    u16 subFlags2;            // 0x402  (pl_sub SubCharMoveTo clears 0x60)
    u8 m_BackRno;             // 0x404  (PS2 cSubChar::m_BackRno)
    u8 m_BackRno2;            // 0x405
    u16 m_BackTime;           // 0x406  frame counter
    u8 m_Frame;               // 0x408
    u8 m_Hokan;               // 0x409
    u8 m_Timer;               // 0x40A  timer
    u8 pad_40B;
    f32 m_Blend;              // 0x40C  blend rate of subBackMot (pl0e subBlendMotSet, like the player's m_Blend)
    f32 dir;                  // 0x410  angle to the player (analyze)
    f32 dist;                 // 0x414  distance to the player (analyze)
    Vec distPos;              // 0x418  position to walk to
    f32 fyBak;                // 0x424  (PS2 cSubChar::fyBak)
    Vec subOfs;               // 0x428  offset behind the player (atckPos)
    u32 plStat;               // 0x434  PlGetStatus() of the frame
    u32 satAttr;              // 0x438  scenario attribute of the wall in front (anaSatInfo)
    Vec satCross;             // 0x43C  hit point of the action wall check (actionCheck)
    Vec satNorm;              // 0x448  its normal
    MotionWorkSub subBackMot; // 0x454 .. 0x524  look-back motion blended in (backCheckSet -> blendMot)
    cModelInfo* subHand[2];   // 0x524  hand model infos (pl11 cSubAshley::setHand)
    f32 fWork0;               // 0x52C  fence / window action direction  (PS2 cSubChar::fWork0)
    int subHideMode;          // 0x530  (pl_sub SubCharCtrlHide); pl_npc: general step counter
    int subX534;              // 0x534  (SubCharCtrlHide mode 0 sets 1)
    int sub538;               // 0x538  step counter
    int sub53C;               // 0x53C  the catch action button is set (moveFallWait)
    int sub540;               // 0x540  frames waiting for the player
    Vec subHidePos;           // 0x544  hide position
    u8 m_FallWaitTimer;       // 0x550  frames until the route is re-checked  (PS2 cSubChar::m_FallWaitTimer)
    u8 pad_551[3];
    struct EmiEntry* pAnotherRoute;   // 0x554  EMI route entry (type 0xB) walked to (embarrel.h)  (PS2 EMINFO_WK*)
    Vec m_PlActPos;           // 0x558  ledge position to wait at (catchOn / actionCheck)
    f32 m_PlActAngY;          // 0x564  angle to turn to while waiting to be caught
    int subAux0;              // 0x568  (SetSubAux/SetSubBulldozer arguments)
    int subAux1;              // 0x56C
    f32 subMoveTo[4];         // 0x570  (SubCharMoveTo x, y, z, w)
    u8 m_PlActTime;           // 0x580  timer  (PS2 cSubChar::m_PlActTime)
    u8 m_PlActType;           // 0x581  (PS2 cSubChar::m_PlActType)
    u8 pad_582[2];
    void* subMot0;            // 0x584  registered motions (SubCharRegistMotion, SetSubDamage)
    void* subMot1;            // 0x588
    // 0x58C .. 0x5C4 is the partner's cMotBase (pl_npc.cpp / obj13: `(cMotBase*) &subFlags58C`)
    u8 subFlags58C;           // 0x58C  (SetSubDamage sets 0x40)
    u8 pad_58D[0x5C4 - 0x58D];
    u32 subSndId;             // 0x5C4  SndCall handle of the bulldozer SEs (objBull Sub_bull_*)
    f32 subX5C8;              // 0x5C8  (obj13 SubLadderClimbCk: the partner climbs only while >= 1000)
    YARARE_INFO subHit[3];    // 0x5CC .. 0x668  extra hit boxes (YarareAdd in cSubChar::init)
    u8 pad_668[0x7D4 - 0x668];
    cLight* subLight;         // 0x7D4  back light (cLightMgr::createBack)
    void* subShape;           // 0x7D8  ShapeMove work (NULL = none)
    void (*subFunc)();        // 0x7DC  routine 4 (damage) handler (cSubChar::move)
    Vec subBustBase[3];       // 0x7E0 .. 0x804  rest positions of parts 0x1D, 0x1E, 0x1A (moveBust)
    cSubChar();
    virtual ~cSubChar();
    virtual void beginEvent();
    virtual void endEvent();
    virtual void move();
    virtual void modelSet() = 0;   // slot 9: pure here (`cSubAshley::modelSet` in pl11); pl_sub EndSubDamage calls it for id 4
    virtual void setFace(int no);
    virtual void setHand(int no);
    virtual void initCloth();
    virtual void moveCloth();
    virtual void setEmFunc();           // pl_sub SetSubDamage (Ashley)

    static const Vec atckPos;    // offset behind the player while he aims (moveBehind)
    static const Vec atckPos2;   // the same for the two-handed weapons

    int mot_ck();                // 1 while the partner's life is at or below half
    void init();
    void moveCore();
    void moveFootwork();
    void moveMove();
    int readyOkCheck();
    void moveBehind();
    void moveKagamu();
    void movePants();
    void moveDown();
    int getScrActionPoint(Vec* opos, Vec* orot, u32 attr);
    void moveFance();
    int landCheck();
    void moveFall();
    void moveAction();
    void moveLadder();
    f32 getJumpAdjY();
    void jumpAdjust();
    void moveBack();
    void moveAux();
    void moveHide();
    void moveStoop();
    void moveFallWait();
    void moveLadderWait();
    void moveWindowWait();
    u32 checkSatAttr(f32 len);
    f32 getAdjustX(int n);
    void moveDamage();
    void moveDie();
    void moveBull();
    void moveEvent();
    void moveDijection();
    void movePos(Vec* target, f32 spd);
    void neckInit();
    void neckCtrl();
    void neckSet(Vec* pos);
    int actCheck();
    int cautionCheck();
    int plDownCheck();
    int fanceCheck();
    int windowCheck();
    int fallLadderCheck();
    int doorCheck();
    int readyCheck();
    int actionCheck();
    int ladder2Check();
    f32 getCliffHeight(f32 ang);
    void pantsCheck();
    int ckPlRun();
    void seqSeCtrl();
    void backCheckSet(void* mot);
    void backCheckMove();
    void backCheckCtrlFootwork();
    void backCheckCtrlMove();
    int checkBackEm();
    void analyze();
    void frontCheck();
    void anaSatInfo();
    void control(int mode);
    int checkAnotherRoute();
    int moveAnotherRoute();
    void damageCheck();
    // scenario damage area hit (sce_at sceAtFunc_damage)
    void setDamage(u8 kind, int arg, f32 power, int a, int b);
    void registPlAction(Vec* pos, f32 ang);
    void moveBust();
    void moveFace();
    void shadowCtrl();
    void dmgCheck();
    void beginDamage();
    void endDamage();
    void interrupt();
    void inSat();
    void debugMove();
    int farCheck();   // never called; dead-stripped in the DOL, only its pool word (1000) survives
};

extern cSubChar* pSUB;   // game/em.cpp

u32 SubCharGetStatus();  // game/pl_npc.cpp: routine bits for the camera / scenario (C++ linkage)

extern "C" {
// game/pl_npc.cpp: partner condition bits for the HUD (cockpit: 1, 2, 8, 0x10, 0x24)
u32 SubCharGetCondition();
int SubCharHideCheck();
}

#endif
