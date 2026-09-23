#ifndef PL_NPC_H
#define PL_NPC_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "cMotBase.h"
#include "cFlag.h"

class cEmWindow;

// Partner character base (game/pl_npc.cpp, `pSUB`): a cEm with the partner virtuals. Vtable order
// from pl_npc's `cSubChar virtual table`: cUnit/cCoord/cModel/cEm virtuals, then the ones below.
// The cSubChar-only fields live in cEm's work area (they sit below 0xDE0, see em.h).
class cSubChar : public cEm {
public:
    enum FLAG {
        F_SLEEP = 0,
        F_STAY = 1,
        F_RELAX = 2,
        F_MOVE_TO = 3,
        F_DONT_RUN = 4,
        F_SHADOW_OFF = 5,
        F_PL_CTRL = 6,
        F_CALL_ENABLE = 7,
        F_BEHIND = 8,
    };
    enum STATUS {
        S_EM_NEAR = 0,
        S_PL_NEAR = 1,
        S_PANTS = 2,
        S_HITWALL = 3,
        S_WEPCAUTION = 4,
        S_DOWN = 5,
        S_ARRIVED = 6,
        S_FALL_WINDOW = 7,
        S_NEED_STOP = 8,
        S_EM_NEAR2 = 9,
    };

    cSubChar* pEm;            // 0x3E0  the model the routines animate (itself)
    int m_NeckTimer;          // 0x3E4  neckSet() called this frame
    Vec m_NeckVec;            // 0x3E8  neck angles (y: current neck angle, parts 3)
    Vec m_NeckTgt;            // 0x3F4  position looked at
    cFlag<u16, FLAG> flg;     // 0x400
    cFlag<u16, STATUS> status;   // 0x402
    u8 m_BackRno;             // 0x404
    u8 m_BackRno2;            // 0x405
    u16 m_BackTime;           // 0x406  frame counter
    u8 m_Frame;               // 0x408
    u8 m_Hokan;               // 0x409
    u8 m_Timer;               // 0x40A  timer
    u8 m_Dummy33;
    f32 m_Blend;              // 0x40C  blend rate of subMot (pl0e subBlendMotSet, like the player's m_Blend)
    f32 dir;                  // 0x410  angle to the player (analyze)
    f32 dist;                 // 0x414  distance to the player (analyze)
    Vec distPos;              // 0x418  position to walk to
    f32 fyBak;                // 0x424
    Vec target;               // 0x428  offset behind the player (atckPos)
    u32 plStat;               // 0x434  PlGetStatus() of the frame
    u32 satAttr;              // 0x438  scenario attribute of the wall in front (anaSatInfo)
    Vec satCross;             // 0x43C  hit point of the action wall check (actionCheck)
    Vec satNorm;              // 0x448  its normal
    MotionWorkSub subMot;     // 0x454 .. 0x524  look-back motion blended in (backCheckSet -> blendMot)
    cModelInfo* m_pModRHand;  // 0x524  hand model infos (pl11 cSubAshley::setHand)
    cModelInfo* m_pModLHand;  // 0x528
    f32 fWork0;               // 0x52C  fence / window action direction
    int m_Work0;              // 0x530  (pl_sub SubCharCtrlHide); pl_npc: general step counter
    int m_Work1;              // 0x534  (SubCharCtrlHide mode 0 sets 1)
    int m_Work2;              // 0x538  step counter
    int m_Work3;              // 0x53C  the catch action button is set (moveFallWait)
    int m_Work4;              // 0x540  frames waiting for the player
    Vec m_VecWork0;           // 0x544  hide position
    u8 m_FallWaitTimer;       // 0x550  frames until the route is re-checked
    u8 m_Dummy71;
    u8 m_Dummy72;
    u8 m_Dummy73;
    struct EmiEntry* pAnotherRoute;   // 0x554  EMI route entry (type 0xB) walked to (embarrel.h)  (PS2 EMINFO_WK*)
    Vec m_PlActPos;           // 0x558  ledge position to wait at (catchOn / actionCheck)
    f32 m_PlActAngY;          // 0x564  angle to turn to while waiting to be caught
    void (*pAux)(cEm*);       // 0x568  (SetSubAux/SetSubBulldozer arguments)
    void (*pAuxDm)(cEm*);     // 0x56C
    Vec m_TargetPos;          // 0x570  (SubCharMoveTo x, y, z)
    f32 m_TargetDir;          // 0x57C  (SubCharMoveTo w)
    u8 m_PlActTime;           // 0x580  timer
    u8 m_PlActType;           // 0x581
    u8 m_Dummy82;
    u8 m_Dummy83;
    void* m_MotTbl2[2];       // 0x584  registered motions (SubCharRegistMotion, SetSubDamage)
    cMotBase m_MotBase;       // 0x58C .. 0x5C4  (SetSubDamage sets 0x40 in its first byte)
    u32 m_StopSe;             // 0x5C4  SndCall handle of the bulldozer SEs (objBull Sub_bull_*)
    f32 Route_h;              // 0x5C8  (obj13 SubLadderClimbCk: the partner climbs only while >= 1000)
    YARARE_INFO m_Yarare[10]; // 0x5CC .. 0x7D4  hit boxes (the first three added in cSubChar::init)
    cLight* m_pLiF;           // 0x7D4  back light (cLightMgr::createBack)
    cModelInfo* m_pFace;      // 0x7D8  face model info the ShapeMove work runs on (NULL = none)
    void (*m_pFunc)();        // 0x7DC  routine 4 (damage) handler (cSubChar::move)
    Vec posBustR;             // 0x7E0  rest positions of parts 0x1D, 0x1E, 0x1A (moveBust)
    Vec posBustL;             // 0x7EC
    Vec posScarf;             // 0x7F8
    cSubChar();
    virtual ~cSubChar();
    virtual void beginEvent(u32 flag);
    virtual void endEvent(u32 flag);
    virtual void move();
    virtual void modelSet() = 0;   // slot 9: pure here (`cSubAshley::modelSet` in pl11); pl_sub EndSubDamage calls it for id 4
    virtual void setFace(int type);
    virtual void setHand(int no);
    virtual void initCloth();
    virtual void moveCloth();
    virtual void setEmFunc(void (*pFunc)());   // pl_sub SetSubDamage (Ashley)

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
    int getScrActionPoint(Vec* initPos, Vec* initAng, u32 actAttr);
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
    u32 checkSatAttr(f32 length);
    f32 getAdjustX(int n);
    void moveDamage();
    void moveDie();
    void moveBull();
    void moveEvent();
    void moveDijection();
    void movePos(Vec* toPos, f32 spd);
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
    f32 getCliffHeight(f32 dy);
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
    void registPlAction(Vec* pos, f32 y, u8 a);
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

// pSUB is a cEm* (as on PS2); the partner it points at is always a cSubChar.
#define SUB_CHAR() ((cSubChar*) pSUB)

u32 SubCharGetStatus();  // game/pl_npc.cpp: routine bits for the camera / scenario (C++ linkage)

extern "C" {
// game/pl_npc.cpp: partner condition bits for the HUD (cockpit: 1, 2, 8, 0x10, 0x24)
u32 SubCharGetCondition();
int SubCharHideCheck();
}

#endif
