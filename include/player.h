#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "em.h"
#include "global.h"
// pl_body.h is left to its users: a unit that includes it parses setKnife and takes two extra pool labels.
#include "math_sub.h"
#include "pl_wep.h"
#include "pl_cloth.h"
#include "cFlag.h"

// 0x98-byte work at cEm::p2A4 (player.cpp init1 mem_alloc); only the byte cam_ctrl reads is named.
class cPlayer;

// Neck control (game/pl_class.cpp), 0x1C bytes at cEm::pNeck: turns the head towards the nearest
// enemy with the neck motions blended into the player's motion (cEm::m_SubMot).
class cPlNeck {
public:
    cPlayer* pl;         // 0x00
    cEm* m_pLastTarget;         // 0x04  enemy looked at
    int m_lockCtr;           // 0x08  frames left looking (0x7FFFFFFF: until the target changes)
    u16 m_Flag;           // 0x0C  bit0: m_MotL is the set motion
    u8 m_Mode;             // 0x0E  0 off, 1 on, 2 -> 1 next frame (PlSetNeck)
    u8 pad_F;
    f32 m_NeckY;             // 0x10  current neck angle
    void* m_MotR;          // 0x14  first neck motion, set at init (PS2 cPlNeck::init(motR, motL, frame); was `motL`)
    void* m_MotL;          // 0x18  second neck motion, switched to past the centre (PS2 m_MotL; was `motR`)

    cPlNeck(cPlayer* pl);
    void init(void* motR, void* motL, int frame);   // range-checked pointers (motSet), frame passed on (PS2 parameter order: motR first)
    void move();
    void motSet(void* data, int frame);
    cEm* getTarget();
    void setMode(int mode);   // stores byte 0xE (pl_sub PlSetNeck)
    void clear() { m_MotR = 0; }
};

// Waist control (game/pl_class.cpp), 0xC bytes at cEm::pWaist.
class cPlWaist {
public:
    Vec m_Ang;                   // 0x00  waist twist angles; only .y (the current angle) is used (PS2 m_Ang)

    cPlWaist();
    // cur = cur * (1 - rate) + target * rate; returns the delta applied
    f32 set(f32 dir, f32 rate);
    void reset() { m_Ang.y = 0.0f; }
    operator f32() { return m_Ang.y; }

    static const f32 ROT_LIMIT;   // pl_class.cpp (.sdata2), unused there
};

// Three-way motion blend (game/pl_class.cpp), 0xE8 bytes; `mot3` in player.cpp: the model's own
// motion (mot0) blended with mot1 (rate < 0) or mot2 (rate > 0) through MotionWork::blend.
class cMot3 {
public:
    cModel* m_pEm;       // 0x00
    f32 m_Rate;            // 0x04  last move() rate, clamped to -1..1
    void* mot0;          // 0x08
    void* mot1;          // 0x0C
    void* mot2;          // 0x10
    int m_Mode;             // 0x14  set() 7th argument: 1 = the blend work gets flags2 bit31  set() 7th argument: 1 = the blend work gets flags2 bit31 (PS2 MODE m_Mode)
    MotionWorkSub work;  // 0x18  the blended motion (em.h)

    cMot3();
    // set(model, motion0, motion1, motion2, MotionSetCore seq, u8 mode, int, u16, u16); the PS2 set(pEm, mot0, mot1, mot2, seq, hokan, mode, attr, frame) orders / types the tail differently
    void set(cModel* m, void* m0, void* m1, void* m2, void* seq, u8 b, int c, u16 d, u16 e);
    void set0(void* m, u8 a, int b);
    void move(f32 rate);
};

extern cMot3 mot3;      // game/player.cpp
// A value that follows a target: m_Val0 is the current value, m_Val1 the target and m_Delay the share
// of the current value kept by each move() (0 = the current value follows the target at once).
template <class T> class cDelay {
    T m_Val0;
    T m_Val1;
    T m_Delay;

public:
    cDelay() { m_Val0 = m_Val1 = m_Delay = 0.0f; }
    void setDelay(T delay) { m_Delay = delay; }
    void reset(T v) { m_Val1 = v; m_Val0 = v; }
    void limit(T lo, T hi)
    {
        if (m_Val1 < lo) m_Val1 = lo;
        else if (m_Val1 > hi) m_Val1 = hi;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
    }
    cDelay& operator=(T v)
    {
        m_Val1 = v;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
        return *this;
    }
    cDelay& operator+=(T v)
    {
        m_Val1 += v;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
        return *this;
    }
    cDelay& operator-=(T v)
    {
        m_Val1 -= v;
        if (m_Delay == 0.0f) m_Val0 = m_Val1;
        return *this;
    }
    void move() { m_Val0 = m_Val0 * m_Delay + m_Val1 * (1.0f - m_Delay); }
    operator T() { return m_Val0; }
};
typedef cDelay<f32> cDelayF;

extern cDelayF m3r;     // game/player.cpp  mot3 blend rate

// Player (game/player.cpp, pl_*.cpp): a cEm with the player virtuals. Its fields are the cEm ones
// (all below 0xDE0, see em.h). Vtable order (pl_class.cpp): cUnit/cCoord/cModel/cEm virtuals, then
// the player ones below.

// In-class bodies below are the ones the original emits after ~cPlayer at the end of pl_class.o
// (in-class inline members of the class whose vtable the unit owns); other units drop their
// linkonce copies (fold_linkonce). Add none that pl_class's target lacks.
class cPlayer : public cEm {
public:
    enum FLAG {
        F_NO_WEP_EFF = 0,
        F_EVENT = 1,
        F_BINOCULAR = 2,
        F_OBJPUSH = 3,
        F_SCOPE = 4,
        F_SP_L_HAND = 5,
        F_CROUCH = 6,
        F_LANDING = 7,
        F_FALLING = 8,
        F_THERMO = 9,
        F_NO_LAUNCHER = 10,
        F_SHADOW = 11,
        F_KNIFE = 12,
    };

    u32 m_Work0;          // 0x3E0  event walk flag / damage timer  (PS2 cPlayer::m_Work0)
    int m_Work1;          // 0x3E4  damage: 1 = turning towards m_Fwork0  (PS2 cPlayer::m_Work1)
    u32 m_Work2;          // 0x3E8  damage (blow): water splash done  (PS2 cPlayer::m_Work2)
    int m_Work3;          // 0x3EC  damage (emrock plemRockEscape): EMI route point run to (-1 = none)  (PS2 cPlayer::m_Work3)
    int m_Work4;          // 0x3F0  emrock escape: frames since the last button press  (PS2 cPlayer::m_Work4)
    int m_Work5;          // 0x3F4  emrock escape: EMI goal sub type (plemRockEscapeCk)  (PS2 cPlayer::m_Work5)
    int m_Work6;          // 0x3F8  emrock escape: goal reached  (PS2 cPlayer::m_Work6)
    int m_Work7;          // 0x3FC  emrock escape: Rnd() & 1 (action button variant)  (PS2 cPlayer::m_Work7)
    f32 m_Fwork0;         // 0x400  event turn limit / damage direction angle (123.0 = none)  (PS2 cPlayer::m_Fwork0)
    Vec m_VecWork0;         // 0x404  event: walk-to position
    Vec m_VecWork1;        // 0x410  position setPos'd while stat bit7 is set (objRobo R0WaitGondola)
    u32 m_Flag;           // 0x41C  bit8 (0x100) event motion done -> reset routine  (PS2 cPlayer::m_Flag)
    cFlag<u32, FLAG> stat;   // 0x420
    void** m_MotTbl;       // 0x424  motion data table ([0] walk, [2] turn, [0x5F..0x6C] set by setMotion)
    void** m_MotTbl2;    // 0x428  registered motion table (pl_sub PlRegistMotion fills [0..11])
    MotionWorkSub m_SubMot;   // 0x42C .. 0x4FC  neck turn motion (pl_class cPlNeck::motSet), blended via blendMot
    u8 m_Frame;              // 0x4FC  (pl_sub PlChangeData/PlMotionReset clear it)
    u8 m_Hokan;              // 0x4FD
    u8 m_BbtnCnt;         // 0x4FE  (PS2 cPlayer::m_BbtnCnt)
    u8 m_CmdTimer;        // 0x4FF  frames until the X button (partner command) is accepted again  (PS2 cPlayer::m_CmdTimer)
    f32 m_Blend;     // 0x500  em2b plBlendMotSet: m_SubMot blend rate source (the strangle button mash 0..255)
    u32 m_SeId;           // 0x504  SndCall handle cPlayer::interrupt stops  (PS2 cPlayer::m_SeId)
    cEm* m_pEm;         // 0x508  locked-on enemy (pl_wep lock, knife aim)  (PS2 cEm* m_pEm)
    cEm* m_pBoat;         // 0x50C  the jet ski the player rides (pl0e cPl0e::setRide / PlBoatMove)
    class cObjSpear* m_pSpear;  // 0x510  the harpoon in hand (pl0f plboatSetSpear / plboatSpearThrow)
    f32 m_BoatPlDir;        // 0x514  pl0f harpoon aim: vertical sight rate (-0.3927 .. 0.3927)
    int m_GachaCtr;         // 0x518  button mash counter (pl_sub PlGacha*)
    u8 m_SplashCtr;       // 0x51C  (PS2 cPlayer::m_SplashCtr; unused on GC)
    u8 m_OCMode;          // 0x51D  (PS2 cPlayer::m_OCMode; unused on GC)
    u8 m_EyeMode;           // 0x51E  (pl_sub PlSetEyeMode)
    u8 m_BinoRno;          // 0x51F  binocular step (cPlayer::moveBinocular 1 -> 2 -> 3 -> 0)
    u8 m_ConDmFlag;       // 0x520  1 once setDamage ran  (PS2 cPlayer::m_ConDmFlag)
    u8 m_ConDm_Dummy;
    u16 m_ConDmTimer;     // 0x522  accumulated setDamage counts; a damage reaction starts past 0xFE  (PS2 cPlayer::m_ConDmTimer)
    Vec m_PosOldWater;    // 0x524
    YARARE_INFO m_Yarare[10];   // 0x530 .. 0x738  the player's hit boxes (YarareAdd)
    int m_pSatMask;       // 0x738  SatMgr.check flag (player.cpp startUp / move)
    void (*pFuncAux)(class cPlayer*);  // 0x73C  routine 1/0xA (pl_R1_Aux) handler
    struct PlRoomEff* m_pEffRoom;  // 0x740  room water effect table (pl_sub PlRegistRoomEff/PlWaterProc)
    void* m_pBoss;        // 0x744  (pl_sub PlRegistBoss)
    void* m_pBossRmf;     // 0x748
    Vec m_FallVec;        // 0x74C  -wallNrm of the ledge to drop from (pl_class fallCheck)
    Vec m_JumpVec;        // 0x758  -normal of the jump-over wall (pl_class jumpCheck)
    f32 m_JumpAdjY;       // 0x764  floor height behind the jump wall minus pos.y
    Vec m_ActCross;       // 0x768  hit point of the action wall check (pl_class actWallCheck)
    Vec m_ActNorm;        // 0x774  its normal
    u32 m_ActAttr;        // 0x780  its scenario attribute (0 = no wall in front)
    struct cPlAlert* Alert;   // 0x784
    class cPlWep* Wep;    // 0x788  weapon control (pl_wep.cpp, 0x44 bytes)
    class cPlNeck* Neck;  // 0x78C  neck control (pl_class.cpp, 0x1C bytes)
    class cPlWaist* Waist;   // 0x790  waist control (pl_class.cpp, 0xC bytes)
    class cPlBody* Body;  // 0x794  body / face / hand model set (pl_body.cpp, 0xF0 bytes)
    struct cMental* Mental;   // 0x798
    struct cPlForm* Form;     // 0x79C
    class cPlPush* Push;  // 0x7A0  push-object control (pl_push.cpp, 0x10 bytes)
    class cMotBase* MotBase;  // 0x7A4  (0x38 bytes)
    u8 pad_7A8[4];
    union {
        Vec bustBase[3];      // 0x7AC  Ashley: rest positions of parts 0x1D, 0x1E, 0x1A (pl_ashley moveBust)
        struct {              // Krauser (pl0a pl_klauser.cpp): the three fading model infos and their state
            cModelInfo* krModel[3];   // 0x7AC  [0]/[1] arm models faded against each other, [2] the tex-render one
            int krX7B8;               // 0x7B8  (ctor: 0)
            int krX7BC;               // 0x7BC
            int krX7C0;               // 0x7C0  (ctor: 0)
            u8 krPad_7C4[0x7D0 - 0x7C4];
        };
    };
    u8 pad_7D0[0x880 - 0x7D0];
    f32 x880;             // 0x880  (Krauser): ctor 1.0
    u8 pad_884[0x890 - 0x884];
    int x890;             // 0x890  (Krauser): cleared by cPlayer::interrupt with pG->flags_5018 bit23
    int krEffWait;        // 0x894  (Krauser): frames until the idle effects (EstSet group 0x3F) respawn; -1 while the arm attack runs (glow off), 0x546 cooldown after it; interrupt turns -1 into 1
    int x898;             // 0x898  (Krauser): tex-render model alpha pulse counter (0..0x1F, transMove)

    cPlayer();
    virtual ~cPlayer() {}
    virtual void beginEvent(u32 flag);
    virtual void endEvent(u32 flag);
    virtual void move();
    virtual void setNoSuspend(int onoff);
    virtual int checkXbutton() { return 0; }
    virtual void setModel() = 0;
    virtual void setMotion() {}
    virtual void setRightHand(int type) = 0;
    virtual void setLeftHand(u32 type) = 0;
    virtual void setFace(int type) = 0;
    virtual void setHead(int no) {}
    virtual void setHead(void* bin, void* tpl) {}
    virtual void setWound() {}
    virtual void moveMatCalcBefore() {}
    virtual void initCloth() {}
    virtual void moveCloth() {}
    void setEyeMode(u8 mode) { m_EyeMode = mode; }
    // Partner (id 3) dead while the player is in routine 0: routine 6 (die), damage info 0x80.
    // Inline, but defined in pl_class.cpp: player.cpp's move() calls it out of line.
    void subCharLiveCheck();

    // game/player.cpp
    void init0();
    void init1();
    void startUp();
    // game/pl_class.cpp
    // m0/seq0 when dmMotCk(), else m1/seq1.
    void motionSet(void* m0, void* seq0, void* m1, void* seq1, int hokan, int frame);
    // Hides cModel::motionSet, so it is forwarded here (always inlined; its out-of-line copy is dead-stripped).
    void motionSet(void* mot, u8 hokan, u16 frame, u16 stat, void* seq) { cModel::motionSet(mot, hokan, frame, stat, seq); }
    int actionSelect();  // routine 1 selection from the keys / action buttons; returns checkXbutton()
    void dmgCheck();     // DmgMgr areas -> setDamage
    void visibleCtrl();  // alpha fade with pG->flags_500C bit13
    void seqSeCtrl();    // motion sequence sound (seNo) -> SndCall
    void keyConfig();
    void keyConfigTypeA();
    void endEvent0(u32 mode);   // 0: to routine 0/1 idle, 1: flags_41C bit8, 2: routine 0
    void beginAction();
    void endAction(int hokan);
    void setSlow(f32 speed);
    void moveEye();
    void moveEyeNormal();
    void moveEyeMotion();
    void setLaserSight(int draw, int noCalc);
    void moveBinocular();
    void shadowCtrl();
    int keyReload();
    void setFootwork();
    void beginDamage();
    void endDamage();
    int endCamera();
    int isKamae();       // aiming (weapon routine ready/fire states, or the aim key held)
    int actCheck();      // 1 when the player may take an action button (act_btn checkPLStatus)
    void interrupt();
    void checkCtrl();    // Key 0x400/0x100000 -> pG->flags_500C bits
    int subScrCheck();   // 1 when the sub screen may open (sscrn SubScreenCall)
    int checkEvent();    // 1 when the event routine is ready (sscrn OpeSetOpenTerm)
    int getLifeLevel();  // 0 fine, 1 caution, 2 danger (cockpit meter colours)

    static const f32 SPEED_WALK_TURN;   // pl_class.cpp (.sdata2)
    static const f32 SPEED_RUN_TURN;
    // game/pl_debug.cpp
    void debugInit();
    void debugMove();
    void emSearch();
    // game/pl_wep.cpp
    void weaponRelease();
    void weaponLoad(int wep_id, int wep_type);  // stores pG 0x4FB0/0x4FB1, then ReadWepData
    void weaponInit();
    // game/pl_class.cpp: scenario damage area hit (sce_at sceAtFunc_damage)
    void setDamage(u8 kind, int arg, f32 power, int a, int b);
};

// Leon (game/pl_leon.cpp): the player model set for the main character.
class cPlLeon : public cPlayer {
public:
    cPlLeon();
    virtual ~cPlLeon() {}
    virtual void move();
    virtual int checkXbutton();
    virtual void setModel();
    virtual void setMotion();
    virtual void setRightHand(int type);
    virtual void setLeftHand(u32 type);
    virtual void setFace(int type);
    virtual void setHead(int type);
    virtual void setHead(void* bin, void* tpl);
    virtual void setWound();
    // In-class on purpose: the original emits these after the destructor at the end of the unit
    // (in-class inline members of the class whose vtable the unit owns), not in source order.
    virtual void initCloth()
    {
        if (pG->pl_costume != 2) {
            PlClothSetLeon(this, &leonHair, &leonJacket, &leonHolster);
        }
    }
    virtual void moveCloth()
    {
        if (pG->pl_costume != 2) {
            PlClothMoveLeon(this, &leonHair, &leonJacket, &leonHolster);
        }
    }
};

// Ashley (game/pl_ashley.cpp): the partner character's model set and bust motion.
class cPlAshley : public cPlayer {
public:
    cPlAshley();
    virtual ~cPlAshley() {}
    virtual void move();
    virtual void setModel();
    virtual void setRightHand(int type);
    virtual void setLeftHand(u32 type);
    virtual void setFace(int type);
    virtual void moveMatCalcBefore();
    virtual void initCloth() { PlClothSetGirl(this, &girlHair, &girlSkirt, &girlSweater, 0); }
    virtual void moveCloth() { PlClothMoveGirl(this, &girlHair, &girlSkirt, &girlSweater); }
    void moveBust();
};

void pl01weaponSet(cPlayer* pEm);  // game/pl_ashley.cpp: fills m_MotTbl from the player archive

// Debug cheat ("maho") command table (game/pl_debug.cpp), 0x16C bytes, `new`ed by cPlayer::debugInit.
struct PlMahoEntry {
    u8 rno;               // 0x00  (PS2 cPlMahoWork::rno)
    u8 timer;               // 0x01  (PS2 cPlMahoWork::timer)
    void (*pFunc)();     // 0x04
    const char* pSpell;    // 0x08  button sequence string  button sequence string (PS2 pSpell)
};

class cPlMaho {
public:
    PlMahoEntry work[30]; // 0x000
    u32 nWork;             // 0x168

    cPlMaho();
    void reset();
    void regist(const char* code, void (*func)());
};

extern cPlMaho* pMaho;   // game/player.cpp
extern u8 PlKaiou;       // game/player.cpp  kaiouken level (0..2)
extern u8 PlDbFlag;      // game/player.cpp  bit1: draw the player position marker
extern void* PlWepMot[3];  // game/player.cpp  weapon motion data

void PlWepMotSet(int type);
void DrawGage(int x, int y, int h, int w, int now, int max, int color);

// game/pl_event.cpp: routine 0 (event) and its sub-routines (index cModel::r_no_1)
void Pl_R0_Event(cPlayer* pEm);
void pl_R1_Event_Normal(cPlayer* pEm);
void pl_R1_Event_ToWalk(cPlayer* pEm);
void pl_R1_Event_Smooth(cPlayer* pEm);

// game/player.cpp
extern Vec PlFancePos;    // point behind the fence / window the player climbs to (pl_class windowCheck)
extern int PlFanceFlag;   // 1 while a fence / window action runs
extern u8 PlMode;         // debug: player mode override (debug.cpp resets it)
extern u8 PlFormMode;
extern void (*Pl_func_tbl[7])(cPlayer*);   // routine 0 dispatch table (pl_sub.cpp re-enters it)
extern void (*BoatMoveFunc)(cPlayer*);     // pl_R1_Boat calls it; the boat module (pl0e / pl0f) registers it
// game/pl_class.cpp
extern int PlKeyReloadType;   // reload key layout (cPlayer::keyReload)
extern int PlReloadDirect;
extern const f32 PlReloadSpeedTbl[45][3];  // per weapon: reload motion speed by level
extern const f32 PlReloadEndTbl[45][3];    // per weapon: reload end frame by level
extern const f32 PlShotFrameTbl[45][5];    // per weapon: shot frame by level

// game/pl_sub.cpp: control helpers
int joyFireOn();
int joyFireTrg();
int joyKamae();
int joyLKamae();

// game/pl_class.cpp
int dmMotCk();

// game/pl_knife.cpp: routine 2 (knife) and its sub-routines (index cModel::xFE / xFF)
void PlKnifeMove(cPlayer* pl);
void knife_r2_ready(cPlayer* pl);
void knife_r2_set(cPlayer* pl);
void knife_r2_fire(cPlayer* pl);
void knife_r2_down(cPlayer* pl);
void setWepTrans(cPlayer* pl, int onoff);

// game/player.cpp: one-time init of the player system (game.cpp GameInit). C linkage.
extern "C" void PlayerInit();

#endif
