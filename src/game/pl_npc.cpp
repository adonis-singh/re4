// game/pl_npc.cpp: cSubChar, the partner character (Ashley): routine tables, follow / escape /
// hide / action logic, damage, neck and face control. Owns the cSubChar vtable.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "joy.h"
#include "snd.h"
#include "motion.h"
#include "math_sub.h"
#include "act_btn.h"
#include "emwindow.h"
#include "emdoor.h"
#include "sce_at.h"
#include "cam_ctrl.h"
#include "esp.h"
#include "est.h"
#include "rnd.h"
#include "em_sub.h"
#include "emhit.h"
#include "embarrel.h"
#include "at_mod.h"
#include "route_ck.h"
#include "cMotBase.h"
#include "eprintf.h"
#include "dbmodule.h"
#include "main_mem.h"
#include "atari_init.h"

extern "C" {
double atan2(double y, double x);
f32 sinf(f32 x);
void* memset(void* dst, int c, unsigned int n);
void ShapeMove(void* p);
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");   // motion.h declares the one-argument form
int SubLadderClimbCk(cModel* m);
int SubLadderClimbCk2(cModel* m);
void pl_fall_ok0();
void pl_fall_ok(cPlayer* pl);
void catchOn();
int getFallPos(cSubChar* pl, Vec* pos, Vec* rot);
void waterProc(cSubChar* pl);
}

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Motion data `no` of the partner's motion archive.
#define SUB_MOT(pl, no) PL_ARC_PTR((pl)->subArc, no)
// The partner's cMotBase (em.h keeps the bytes under pl_sub's name).
#define SUB_MOTBASE(pl) ((cMotBase*) &(pl)->subFlags58C)
// The partner flags tested as cFlag bits (atari.h): the reads stay HImode and get the original's
// `mr` / `clrlwi` copies when gcse PRE shares them.
#define SUBFLAG(pl) ((cFlag*) &(pl)->subFlags)
#define SUBFLAG2(pl) ((cFlag*) &(pl)->subFlags2)
// Collision flag bits set / cleared through the info's address (`cAtariInfo* at = &atari` locals).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->m_flag |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->m_flag &= mask; }
// Same as a raw (non-struct) store through the info's address: a following global load (pG)
// stays below it (interrupt(), moveDamage).
static inline void AtariOnRaw(cAtariInfo* at, u16 b) { *(u16*) ((u8*) at + 0x1a) |= b; }
// MotionSetCore with the sequence table as the 4th argument (declared int in motion.h).
#define MOT_SET(m, w, data, seq, a, b, c) MotionSetCore(m, w, data, (int) (seq), a, b, c)

// pSUB stored through a struct view: keeps the base destructor's be_flag load below the store.
struct SubCharPtr {
    cSubChar* p;
};

const Vec cSubChar::atckPos = { -100.0f, 0.0f, -500.0f };
const Vec cSubChar::atckPos2 = { 300.0f, 0.0f, -500.0f };

// Eye direction state (moveFace): a class with a constructor, so the file-scope static gets the
// dynamic initialiser the original has.
struct SubEyeDir {
    f32 x;
    f32 y;
    f32 z;
    SubEyeDir()
    {
        z = 0.0f;
        y = 0.0f;
        x = 0.0f;
        if (x > 1000.0f) {   // folded away by cse (x is known to be 0), but its 1000 stays in the pool
            x = 1000.0f;
        }
    }
    // Blend the current direction towards the target. A member function like limit(): the loads
    // through `this` give the original's z-before-1.0 load order.
    void mix()
    {
        x = x * z + y * (1.0f - z);
    }
    // Clamp the target and latch it into the current value while the mix is 0. A member function so
    // the accesses go through `this` (the original's pointer-form clamp block).
    void limit()
    {
        f32 lo = -0.3141592741012573f;   // plain locals: both bounds are loaded before the first test
        f32 hi = 0.3141592741012573f;

        if (y < lo) {
            y = lo;
        } else if (y > hi) {
            y = hi;
        }
        if (z == 0.0f) {
            x = y;
        }
    }
};
static SubEyeDir eyeDir;

// Routine bytes written through an inline taking ints (pl_class PlRoutineSet): the stores come out
// in the original's order.
static inline void SubRoutineSet(cSubChar* pl, int r0, int r1, int r2, int r3)
{
    pl->r_no_0 = r0;
    pl->r_no_1 = r1;
    pl->r_no_2 = r2;
    pl->r_no_3 = r3;
}

// 1 while the partner has at least half her life: picks the healthy motion set (0x12..) over
// the hurt one (0x6E..).
int cSubChar::mot_ck()
{
    return hp >= (s16) pG->ashley_life_max / 2;
}

// Partner enemy construction: flags clear, its motion-base helper, no light / damage function,
// neck straight, eye state.
cSubChar::cSubChar()
{
    subFlags = 0;
    subFlags2 = 0;
    new (SUB_MOTBASE(this)) cMotBase;
    subSelf = this;
    subFunc = 0;
    subLight = 0;
    neckInit();
    eyeDir.y = 0.0f;
    eyeDir.z = 0.4f;
    eyeDir.x = 0.0f;
}

// Frees her back light and clears the global pSUB.
cSubChar::~cSubChar()
{
    if (subLight && subLight->isAlive()) {
        LightMgr.destroy(subLight);
    }
    ((SubCharPtr*) &pSUB)->p = 0;
}

// Set up after creation (EmMgr.createBack): cloth, a 1000-unit light and a back light, the 300 x
// 400 collision cylinder, lock point at the head (parts 4) but not lockable, three damage capsules
// (body, two legs), the idle motion by health, bust rest positions, the 0x98-byte p2A4 work.
void cSubChar::init()
{
    initCloth();
    TevScaleGroup = 1;
    {
        static const Vec lightOfs = { 0.0f, 0.0f, 0.0f };
        static const Vec lightSize = { 1000.0f, 1000.0f, 0.0f };
        LightInfo.init2(0, 1, &lightOfs, &lightSize, 0x40);
    }
    atariInitF(&atari, 0.0f, -200.0f, 0.0f, 300.0f, 200.0f, 400.0f, 900.0f, 1, 0x1000, 10);
    if (subLight == 0) {
        subLight = LightMgr.createBack(0, 2, 0, 0);
        subLight->setParent(this);
    }
    {
        cSubChar* s = subSelf;

        s->lockParts = 4;
        s->lockOfs.x = 0.0f;
        s->lockOfs.y = 0.0f;
        s->lockOfs.z = 0.0f;
    }
    subSelf->setStatus(EM_STATUS_LOCKOFF);
    // statement order brute-forced (store schedule + shared zero registers)
    m_FallWaitTimer = 0;
    m_PlActTime = 0;
    be_flag |= 0x02200000;
    subFlags |= 0x40;
    subFlags &= 0xFFF4;
    subAux1 = 0;
    subAux0 = 0;
    AtariOn(&atari, 0x300);
    YarareInit(this, 0.0f, -30.0f, 0.0f, 140.0f, 100.0f, 2, 1);
    YarareAdd(this, &subHit[0], 0.0f, 0.0f, 0.0f, 150.0f, 130.0f, 3, 1);
    YarareAdd(this, &subHit[1], -20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x13, 1);
    YarareAdd(this, &subHit[2], 20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x17, 1);
    if (mot_ck()) {
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x12), 0, 0, 5, 0);
    } else {
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6E), 0, 0, 5, 0);
    }
    motionMove();
    subBustBase[0] = getPartsPtr(0x1D)->pos;
    subBustBase[1] = getPartsPtr(0x1E)->pos;
    subBustBase[2] = getPartsPtr(0x1A)->pos;
#line 231 "D:/Bio4/Prog/pl_npc.cpp"
    p2A4 = (EmWork2A4*) MEM_ALLOC(0x98, 1, 13);
}

// Per frame: hp mirrors pG->ashley_life, the player's routine bits are cached (plStat), damage
// areas are checked, then r_no_0 (0 core, 1 damage, 2 die, 3 bulldozer, 4 custom subFunc, 5
// event, 6 dijection); afterwards the neck, face, motion base, shadow, collision, sequence SEs,
// cloth, bust, face morph, water effects. Kaiouken (Debug_flg[2] 0x10000) runs the routine again.
void cSubChar::move()
{
    static void (cSubChar::*NpcFuncTbl[])() = {
        &cSubChar::moveCore,
        &cSubChar::moveDamage,
        &cSubChar::moveDie,
        &cSubChar::moveBull,
        0,
        &cSubChar::moveEvent,
        &cSubChar::moveDijection,
    };
    f32 water;

    if (SUBFLAG(this)->check(0)) {
        return;
    }
    hp = pG->ashley_life;
    SUB_MOTBASE(this)->adjust();
    StaFlagOff(pG, STA_TAKEAWAY);
    StaFlagOff(pG, STA_SUB_CATCHED);
    StaFlagOff(pG, STA_SUB_LADDER);
    plStat = PlGetStatus();
    dmgCheck();
    damageCheck();
    if (r_no_0 == 4) {
        subFunc();
    } else {
        (this->*NpcFuncTbl[r_no_0])();
    }
    if (DbgFlagChk(pG, DBG_KAIOUKEN)) {
        int i;

        for (i = 0; i < PlKaiou + 1; i++) {
            if (r_no_0 == 4) {
                subFunc();
            } else {
                (this->*NpcFuncTbl[r_no_0])();
            }
        }
    }
    if (m_PlActTime) {
        m_PlActTime--;
    }
    backCheckMove();
    neckCtrl();
    moveFace();
    SUB_MOTBASE(this)->move();
    shadowCtrl();
    partsWorldCalc();
    EmAtCheck(this);
    SatMgr.check(this, 0);
    atari.move();
    PartsWorldPosCalc(this);
    seqSeCtrl();
    moveCloth();
    moveBust();
    if (subShape) {
        ShapeMove(subShape);
    }
    if (GetWaterHeight(&pos, &water) && water > pos.y) {
        waterProc(this);
    }
    debugMove();
}

// r_no_0 == 0 (normal): analyzes the surroundings, then r_no_1: 0 footwork, 1 move, 2/5 behind the
// aiming player, 4 crouch (kagamu), 6 pants, 7 down, 8 fence, 9 fall, 0xA action, 0xC ladder, 0xE
// back, 0xF aux, 0x10 hide, 0x11 stoop, 0x12 fall wait, 0x13 ladder wait, 0x14 window wait. The
// player dying in a plain state puts her in dijection (routine 6).
void cSubChar::moveCore()
{
    static void (cSubChar::*funcTbl[])() = {
        &cSubChar::moveFootwork,
        &cSubChar::moveMove,
        &cSubChar::moveBehind,
        0,
        &cSubChar::moveKagamu,
        &cSubChar::moveBehind,
        &cSubChar::movePants,
        &cSubChar::moveDown,
        &cSubChar::moveFance,
        &cSubChar::moveFall,
        &cSubChar::moveAction,
        0,
        &cSubChar::moveLadder,
        0,
        &cSubChar::moveBack,
        &cSubChar::moveAux,
        &cSubChar::moveHide,
        &cSubChar::moveStoop,
        &cSubChar::moveFallWait,
        &cSubChar::moveLadderWait,
        &cSubChar::moveWindowWait,
    };

    analyze();
    (this->*funcTbl[r_no_1])();
    if ((s16) pG->pl_life <= 0) {
        switch (r_no_1) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 0xE:
            SubRoutineSet(this, 6, 0, 0, 0);
            dmg.set(0, 0x80);
            break;
        }
    }
}


// Routine 0 / 0: standing still, looking around, turning towards the player.
void cSubChar::moveFootwork()
{
    u32 act;

    switch (r_no_2) {
    case 0:
        if (r_no_3 == 1) {
            void* m;
            void* seq;

            if (mot_ck()) {
                m = SUB_MOT(subSelf, 0x12);
                seq = SUB_MOT(subSelf, 0x4F);
            } else {
                m = SUB_MOT(subSelf, 0x6E);
                seq = 0;
            }
            MOT_SET(subSelf, MOTION(subSelf), m, seq, 0, 4, 0);
            subSelf->motionMove();
        }
        if (mot_ck()) {
            BitOff16(subFlags, 4);
        } else {
            subFlags |= 4;
        }
        subHideMode = 0;
        if (SUBFLAG(this)->check(3)) {
            if (dist > 400.0f) {
                SubRoutineSet(this, 0, 1, 0, 0);
                return;
            }
            r_no_2 = 0x32;
            break;
        }
        r_no_2 = 1;
    case 1: {
        void* m;
        void* seq;
        void* back;

        if (mot_ck()) {
            m = SUB_MOT(subSelf, 0x12);
            seq = SUB_MOT(subSelf, 0x4F);
            back = SUB_MOT(subSelf, 0x18);
        } else {
            m = SUB_MOT(subSelf, 0x6E);
            seq = 0;
            back = 0;
        }
        MOT_SET(subSelf, MOTION(subSelf), m, seq, 0x14, 4, 0);
        backCheckSet(back);
        m_Timer = Rnd();
        if (m_Timer < 60) {
            m_Timer += 60;
        }
        r_no_2 = 2;
    }
    case 2:
        neckSet(&pPL->pos);
        m_Timer--;
        if (m_Timer != 0xFF) {
            break;
        }
        if (checkBackEm()) {
            break;
        }
        if (dir > 2.617994f || dir < -2.617994f) {
            m_Timer = 60;
            r_no_2 = 0x14;
            break;
        }
        if (dir > 0.5235988f || dir < -0.5235988f) {
            m_Timer = 6;
            r_no_2 = 0xA;
            break;
        }
        m_Timer = Rnd();
        if (m_Timer < 100) {
            m_Timer += 100;
        }
        r_no_2 = 0x1E;
        break;
    case 0xA:
        if (dir < -0.19634955f) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x35), SUB_MOT(subSelf, 0x5D), 7, 5, 0);
            subSelf->r_no_2 = 0xB;
        } else if (dir > 0.19634955f) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x36), SUB_MOT(subSelf, 0x5E), 7, 5, 0);
            subSelf->r_no_2 = 0xC;
        } else {
            subSelf->r_no_2 = 1;
        }
        break;
    case 0xB:
        if (dir >= -0.19634955f) {
            subSelf->r_no_2 = 1;
        }
        break;
    case 0xC:
        if (dir < 0.19634955f) {
            subSelf->r_no_2 = 1;
        }
        break;
    case 0x14:
        if (mot_ck()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x1A), SUB_MOT(subSelf, 0x53), 7, 5, 0);
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x72), 0, 7, 5, 0);
        }
        subSelf->r_no_2 = 0x1F;
    case 0x15:
        if (subSelf->frame >= (f32) (subSelf->frameMax - 1)) {
            subSelf->r_no_2 = 1;
        }
        break;
    case 0x1E:
        setFace(2);
        if (!SUBFLAG(this)->check(2)) {
            subFlags |= 4;
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x19), 0, 7, 5, 0);
            r_no_2 = 0x1F;
        } else {
            neckSet(&pPL->pos);
            r_no_3 = Rnd();
            if (r_no_3 < 60) {
                m_Timer += 60;
            }
            r_no_2 = 0x20;
        }
    case 0x1F:
        if (subSelf->frame >= (f32) (subSelf->frameMax - 1) || checkBackEm()) {
            setFace(4);
            subSelf->r_no_2 = 1;
        }
        break;
    case 0x20:
        neckSet(&pPL->pos);
        r_no_3--;
        if (r_no_3 == 0 || checkBackEm()) {
            r_no_3 = 0;
            setFace(4);
            subSelf->r_no_2 = 1;
        }
        break;
    case 0x28:
        if (subSelf->frame >= (f32) (subSelf->frameMax - 1)) {
            subSelf->r_no_2 = 0;
        }
        break;
    case 0x32:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x43), 0, 7, 1, 0);
        r_no_2 = 0x33;
    case 0x33:
        if (subSelf->frame >= (f32) (subSelf->frameMax - 1)) {
            MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x44), 0, 7, 5, 0);
            r_no_2 = 0x34;
        }
        break;
    case 0x34:
        break;
    }
    motionMove();
    if (SUBFLAG(this)->check(1)) {
        if (SUBFLAG2(this)->check(9)) {
            if (!SUBFLAG2(this)->check(1)) {
                subHideMode++;
                if (subHideMode > 150) {
                    subHideMode = 0;
                    subSndId = SndCall(8, 2, &subSelf->pParts->world, id, 0, 0);
                }
            }
        }
    }
    backCheckCtrlFootwork();
    if (SUBFLAG2(this)->check(2)) {
        SubRoutineSet(this, 0, 6, 0, 0);
        return;
    }
    if (SUBFLAG2(this)->check(4)) {
        SubRoutineSet(this, 0, 4, 0, 0);
        return;
    }
    if (plDownCheck()) {
        SubRoutineSet(this, 0, 0x11, 0, 0);
        return;
    }
    if (!SUBFLAG2(this)->check(1) && SUBFLAG2(this)->check(0)) {
        SubRoutineSet(this, 0, 7, 0, 0);
        return;
    }
    if (SUBFLAG(this)->check(1)) {
        return;
    }
    if (plStat & 0x40000) {
        return;
    }
    if (m_FallWaitTimer) {
        m_FallWaitTimer--;
        if (m_FallWaitTimer == 0) {
            checkAnotherRoute();
        }
        return;
    }
    if (plStat & 0x400) {
        return;
    }
    if (SUBFLAG2(this)->check(8)) {
        return;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED) && SUBFLAG2(this)->check(1)) {
        SubRoutineSet(this, 0, 7, 0, 0);
        return;
    }
    if (StaFlagChk(pG, STA_PL_EM_ACTION) && SUBFLAG2(this)->check(1)) {
        if ((u8) (r_no_2 - 0x32) > 9) {
            r_no_2 = 0x32;
        }
        return;
    }
    if ((plStat & 0x20000) && dist < 1000.0f) {
        return;
    }
    if (readyCheck()) {
        SubRoutineSet(this, 0, 1, 0, 0);
        return;
    }
    act = actCheck();
    switch (act) {
    default:
        if (plStat & 0x41F) {
            if (SUBFLAG(subSelf)->check(3)) {
                if (!SUBFLAG2(subSelf)->check(6)) {
                    subSelf->m_Frame = 0;
                    subSelf->m_Hokan = 5;
                    SubRoutineSet(subSelf, 0, 1, 0, 0);
                    break;
                }
            }
            if (dist > 1100.0f || m_PlActTime) {
                subSelf->m_Frame = 0;
                subSelf->m_Hokan = 5;
                SubRoutineSet(subSelf, 0, 1, 0, 0);
            }
        }
        break;
    case 7:
        SubRoutineSet(this, 0, 2, 0, 0);
        break;
    case 8:
        SubRoutineSet(this, 0, 0xA, 0, 0);
        break;
    case 9:
        SubRoutineSet(this, 0, 0xC, 0, 0);
        break;
    case 5:
    case 6:
        break;
    }
}

// Routine 0 / 1: walk or run after the player to subTarget.
void cSubChar::moveMove()
{
    const f32 near = 400.0f;   // pool order: 400 precedes the turn angles

    switch (subSelf->r_no_2) {
    case 0:
        m_Timer = 0;
        if (dir > 2.617994f || dir < -2.617994f) {
            if (!SUBFLAG(this)->check(4) && (ckPlRun() || dist > 3000.0f)) {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x26), SUB_MOT(subSelf, 0x58), 7, 5, 0);
                subSelf->r_no_3 = 3;
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x26), SUB_MOT(subSelf, 0x58), 7, 5, 0);
                subSelf->r_no_3 = 2;
            }
        } else {
            if (!SUBFLAG(this)->check(4) && (ckPlRun() || dist > 3000.0f)) {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x25), SUB_MOT(subSelf, 0x57), 7, 4, 0);
                subSelf->r_no_3 = 1;
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x24), SUB_MOT(subSelf, 0x56), 7, 4, 0);
                subSelf->r_no_3 = 0;
            }
        }
        checkAnotherRoute();
        subBackMot.blendRate = 0.0f;
        m_BackRno = 0;
        m_BackRno2 = 0;
        m_BackTime = 30;
        subSelf->r_no_2 = 1;
    case 1:
        if (subSelf->r_no_3 <= 1) {
            ang.y += Muku(&pos, &distPos, ang.y, 0.20943952f);
        }
        if (!subSelf->motionMove()) {
            break;
        }
        subSelf->r_no_2 = 2;
    case 2:
        switch (r_no_3) {
        case 2:
            r_no_3 = 0;
        case 0:
            if (mot_ck()) {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x13), SUB_MOT(subSelf, 0x50), 7, 4, 0);
                backCheckSet(SUB_MOT(subSelf, 0x15));
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x6F), SUB_MOT(subSelf, 0x76), 7, 4, 0);
                backCheckSet(0);
            }
            break;
        case 3:
            subSelf->r_no_3 = 1;
        case 1:
            if (mot_ck()) {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x16), SUB_MOT(subSelf, 0x52), 7, 4, 0);
                backCheckSet(SUB_MOT(subSelf, 0x17));
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x71), SUB_MOT(subSelf, 0x78), 7, 4, 0);
                backCheckSet(0);
            }
            break;
        }
        subSelf->r_no_2 = 3;
    case 3:
        motionMove();
        switch (subSelf->r_no_3) {
        case 0:
            if (!SUBFLAG(this)->check(4) && (ckPlRun() || dist > 3000.0f)) {
                m_Frame = (u8) (pPL->frame * 100.0f / (f32) (int) pPL->frameMax);
                m_Hokan = 3;
                r_no_2 = 2;
                r_no_3 = 1;
            }
            break;
        case 1:
            if (SUBFLAG(this)->check(4) || (!ckPlRun() && dist < 550.0f)) {
                m_Frame = (u8) (pPL->frame * 100.0f / (f32) (int) pPL->frameMax);
                m_Hokan = 3;
                r_no_2 = 2;
                r_no_3 = 0;
            }
            break;
        }
        if (dist > 400.0f || SUBFLAG(this)->check(3) || m_PlActTime) {
            ang.y += Muku(&pos, &distPos, ang.y, 0.20943952f);
        } else if (plStat & 4) {
            f32 a = LIMIT_ANGLE(pPL->ang.y + 3.1415927f);

            ang.y += Muku2(ang.y, a, 0.10471976f);
        } else {
            ang.y += Muku2(ang.y, pPL->ang.y, 0.10471976f);
        }
        if (SUBFLAG(this)->check(3) || m_PlActTime) {
            if (dist <= 400.0f) {
                r_no_2 = 4;
                motFlags &= ~1;
                subHideMode = 0;
            }
        } else if (dist <= 400.0f) {
            if (pPL->r_no_0 == 0 && (pPL->r_no_1 == 0 || pPL->r_no_1 == 4)) {
                r_no_0 = 0;
                dist = 0.0f;
                r_no_1 = 0;
                r_no_2 = 0;
                r_no_3 = 0;
            }
        }
        break;
    case 4:
        motionMove();
        subHideMode++;
        if (dist < 50.0f || subHideMode > 6) {
            setPos(&distPos);
            if (subMoveTo[3] == 193.0f) {
                subFlags2 |= 0x40;
                r_no_0 = 0;
                r_no_1 = 0;
                r_no_2 = 0;
                r_no_3 = 0;
            } else {
                r_no_2 = 0xA;
            }
        }
        break;
    case 0xA:
        if (Muku2(ang.y, subMoveTo[3], 3.1415927f) < 0.0f) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x35), SUB_MOT(subSelf, 0x5D), 4, 5, 0);
            r_no_2 = 0xB;
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x36), SUB_MOT(subSelf, 0x5E), 4, 5, 0);
            r_no_2 = 0xC;
        }
        motionMove();
        break;
    case 0xB:
        motionMove();
        if (Muku2(ang.y, subMoveTo[3], 3.1415927f) > 0.0f) {
            ang.y = subMoveTo[3];
            subFlags2 |= 0x40;
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 0xC:
        motionMove();
        if (Muku2(ang.y, subMoveTo[3], 3.1415927f) < 0.0f) {
            ang.y = subMoveTo[3];
            subFlags2 |= 0x40;
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
    backCheckCtrlMove();
    if (SUBFLAG(this)->check(3) || SUBFLAG(this)->check(4)) {
        Vec p;
        Vec r;

        MotionGetSpeed(this, MOTION(this), 0, &p, &r);
        MotionAddSpeed(this, MOTION(this), &p, &r);
    } else if (r_no_2 == 4) {
        Vec d;

        PSVECSubtract(&distPos, &pos, &d);
        PSVECScale(&d, &d, 0.4f);
        PSVECAdd(&pos, &d, &pos);
    } else if (r_no_3 <= 1) {
        static f32 distMin = 310.0f;
        f32 spd;

        if (subSelf->r_no_3 == 0) {
            if (dist > 600.0f) {
                spd = 100.0f;
            } else if (dist <= distMin) {
                spd = 0.0f;
            } else {
                spd = (dist - distMin) * 100.0f / (600.0f - distMin);
            }
        } else {
            if (dist > 600.0f) {
                spd = 160.0f;
            } else if (dist <= distMin) {
                spd = 0.0f;
            } else {
                spd = (dist - distMin) * 160.0f / (600.0f - distMin);
            }
        }
        movePos(&distPos, spd);
    }
    if (m_Timer <= 0xF9) {
        m_Timer++;
    }
    if (SUBFLAG2(this)->check(2)) {
        SubRoutineSet(this, 0, 6, 0, 0);
    } else if (SUBFLAG2(this)->check(4)) {
        SubRoutineSet(this, 0, 4, 0, 0);
    } else if (StaFlagChk(pG, STA_PL_EM_ACTION) && SUBFLAG2(this)->check(1)) {
        SubRoutineSet(this, 0, 0, 0x32, 0);
    } else if (!SUBFLAG2(this)->check(1) && SUBFLAG2(this)->check(0)) {
        SubRoutineSet(this, 0, 7, 0, 0);
    } else if (StaFlagChk(pG, STA_PL_CATCHED) && SUBFLAG2(this)->check(1)) {
        SubRoutineSet(this, 0, 7, 0, 0);
    } else if (SUBFLAG(this)->check(1)) {
        SubRoutineSet(this, 0, 0, 0, 0);
    } else if (SUBFLAG2(this)->check(8)) {
        return;
    } else if (m_FallWaitTimer) {
        SubRoutineSet(this, 0, 0, 0, 0);
    } else if ((plStat & 0x400) && !SUBFLAG(this)->check(3)) {
        SubRoutineSet(this, 0, 0, 0, 0);
    } else if (plStat & 0x40000) {
        SubRoutineSet(this, 0, 0, 0, 0);
    } else if ((plStat & 0x20000) && dist < 1000.0f && !SUBFLAG(this)->check(3)) {
        SubRoutineSet(this, 0, 0, 0, 0);
    } else {
        switch ((u32) actCheck()) {
        case 1:
            SubRoutineSet(this, 0, 8, 0, 0);
            break;
        case 2:
            SubRoutineSet(this, 0, 8, 0, 1);
            break;
        case 3:
            subFlags2 |= 0x80;
            SubRoutineSet(this, 0, 0x12, 0, 0);
            break;
        case 4:
            SubRoutineSet(this, 0, 0x14, 0, 0);
            break;
        case 5:
        case 6:
            break;
        case 7:
            SubRoutineSet(this, 0, 2, 0, 0);
            break;
        case 8:
            SubRoutineSet(this, 0, 0xA, 0, 0);
            break;
        case 9:
            SubRoutineSet(this, 0, 0xC, 0, 0);
            break;
        case 0xB:
            SubRoutineSet(this, 0, 0x13, 0, 0);
            break;
        }
    }
}

// The player is aiming (plStat 0x10), within 550 and with a clear line: she should take cover.
int cSubChar::readyOkCheck()
{
    if (!(plStat & 0x10)) {
        return 0;
    }
    if (GetDistance(&pPL->pos, &pos) > 302500.0f) {
        return 0;
    }
    return SatMgr.hitCheck(&pPL->pParts->world, &pParts->world, 0, 0, 0, 0) == 0;
}

// Routine 0 / 2 (and 5): stand behind the aiming player, cover the ears.
void cSubChar::moveBehind()
{
    const Vec* ap;

    switch (PlGetWeaponNo()) {   // default first: its block is laid out first
    default:
        ap = &atckPos;
        break;
    case 0xD:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1F:
    case 0x20:
        ap = &atckPos2;
        break;
    }
    if (r_no_2 <= 0x13 && (plStat & 0x10)) {
        subOfs = *ap;
        // reference stores: the following pPL loads stay below them
        FSet(subOfs.z, subOfs.z - pPL->Wep->getAngle() * 0.31830987f * 300.0f);
        PSMTXMultVec(pPL->mat, &subOfs, &subOfs);
        FSet(pos.x, pos.x * 0.7f + subOfs.x * 0.3f);
        FSet(pos.z, pos.z * 0.7f + subOfs.z * 0.3f);
        ang.y += Muku2(ang.y, pPL->ang.y, 0.31415927f);
    }
    if (!(plStat & 0x10)) {
        subHideMode = 1;
    }
    switch (r_no_2) {
    case 0:
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x1D), 0, 3, 5, 0);
        AtariOff(&atari, 0xFCFF);
        subHideMode = 0;
        r_no_2 = 1;
    case 1:
        if (!(plStat & 0x10)) {
            r_no_2 = 0x14;
        }
        if (motionMove()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x1E), 0, 3, 5, 0);
            r_no_2 = 2;
        }
        break;
    case 2:
        motionMove();
        if (StaFlagChk(pG, STA_CRITICAL)) {
            StaFlagOff(pG, STA_CRITICAL);
            if (!(plStat & 0x2080)) {
                r_no_2 = 0xA;
            }
        }
        if (subHideMode) {
            r_no_2 = 0x14;
        }
        break;
    case 0xA:
        setHand(1);
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x20), 0, 7, 5, 0);
        r_no_2 = 0xB;
    case 0xB:
        if ((plStat & 0x2080) || motionMove()) {
            setHand(0);
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x1E), 0, 3, 5, 0);
            motionMove();
            r_no_2 = 2;
        }
        break;
    case 0x14:
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x1F), 0, 3, 5, 0);
        StaFlagOff(pG, STA_CRITICAL);
        AtariOn(&atari, 0x300);
        inSat();
        atari.set(-10, 300.0f, 200.0f);
        SubRoutineSet(this, 0, 0, 0x28, 0);
        break;
    }
}

// Routine 0 / 4: crouch (kagamu) while the player fires over her.
void cSubChar::moveKagamu()
{
    switch (subSelf->r_no_2) {
    case 0:
        if (mot_ck()) {
            subSelf->motionSet(SUB_MOT(subSelf, 0x12), 10, 0, 1, 0);
        } else {
            subSelf->motionSet(SUB_MOT(subSelf, 0x6E), 10, 0, 1, 0);
        }
        subSelf->r_no_3 = 0;
        subSelf->r_no_2 = 1;
    case 1:
        subSelf->motionMove();
        if (++subSelf->r_no_3 > 5) {
            subSelf->r_no_2 = 2;
        }
        break;
    case 2:
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x43), 0, 1, 5, 0);
        subSelf->r_no_2 = 3;
        fyBak = subSelf->pos.y;
        AtariOff(&subSelf->atari, 0xFDFF);
    case 3:
        if (subSelf->frame > 7.7f && subSelf->frame < 8.3f) {
            subFlags2 |= 0x20;
        }
        if (subSelf->motionMove()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x44), 0, 3, 5, 0);
            subSelf->r_no_2 = 4;
        }
        break;
    case 4:
        subSelf->motionMove();
        if (Key.on & 0x810) {
            subSelf->subHideMode = 0;
        } else {
            if (++subSelf->subHideMode > 30) {
                MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x45), 0, 3, 5, 0);
                AtariOn(&subSelf->atari, 0x200);
                subSelf->r_no_2 = 5;
            }
        }
        break;
    case 5:
        if (subSelf->frame > 11.7f && subSelf->frame < 12.3f) {
            BitOff16(subFlags2, 0x20);
        }
        if (subSelf->motionMove()) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 6: the "pants" reaction (the player looks up her skirt).
void cSubChar::movePants()
{
    switch (subSelf->r_no_2) {
    case 0:
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x21), 0, 7, 5, 0);
        subSndId = SndCall(8, 0x16, &subSelf->pParts->world, id, 0, 0);
        subSelf->r_no_2 = 1;
    case 1:
        if (MotionMoveF(subSelf, 0)) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x22), 0, 7, 5, 0);
            subSelf->r_no_2 = 2;
        }
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        break;
    case 2:
        MotionMoveF(subSelf, 0);
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        if (!SUBFLAG2(this)->check(2)) {
            subSelf->r_no_2 = 3;
            subSelf->r_no_3 = 0;
        }
        break;
    case 3:
        MotionMoveF(subSelf, 0);
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        if (SUBFLAG2(this)->check(2)) {
            subSelf->r_no_2 = 2;
        } else {
            if (++subSelf->r_no_3 > 30) {
                MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x23), 0, 7, 5, 0);
                subSelf->r_no_2 = 4;
            }
        }
        break;
    case 4:
        if (MotionMoveF(subSelf, 0)) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 7: crouch down (the player's "wait" command).
void cSubChar::moveDown()
{
    switch (r_no_2) {
    case 0:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x43), 0, 7, 5, 0);
        subFlags2 |= 0x20;
        subSelf->r_no_2 = 1;
    case 1:
        if (motionMove()) {
            subSelf->r_no_2 = 2;
        }
        break;
    case 2:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x44), 0, 7, 1, 0);
        r_no_2 = 3;
    case 3:
        if (SUBFLAG2(this)->check(0) || (StaFlagChk(pG, STA_PL_CATCHED))) {
            r_no_3 = 40;
        } else if (r_no_3) {
            r_no_3--;
        }
        if (r_no_3 == 0) {
            MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x45), 0, 7, 5, 0);
            r_no_2 = 4;
        }
        motionMove();
        break;
    case 4:
        if (motionMove()) {
            BitOff16(subFlags2, 0x20);
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Point 300 behind the scenario wall (of attribute `attr`) 1000 ahead of the partner: position and
// facing angle for the fence / window actions. 1 when found.
int cSubChar::getScrActionPoint(Vec* opos, Vec* orot, u32 attr)
{
    // `h` first: its pool entry precedes -1000 (and the const local's extra RTL gives the
    // original's r22/r23 allocation and store schedule).
    const f32 h = 400.0f;
    Vec a = { 0.0f, h, 0.0f };

    PSVECAdd(&a, &pos, &a);
    Vec b = { 0.0f, h, 1000.0f };   // initialised after the first call (mid-block declaration)
    Vec nrm;
    Vec hit;
    u32 r;
    PSMTXMultVec(mat, &b, &b);
    r = SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0);
    if (!(r & 0x01000000) || !(r & attr)) {
        return 0;
    }
    PSVECScale(&nrm, &b, -1000.0f);
    PSVECAdd(&b, &pos, &b);
    b.y += h;
    r = SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0);
    if (!(r & 0x01000000) || !(r & attr)) {
        return 0;
    }
    PSVECScale(&nrm, &b, 300.0f);
    PSVECAdd(&b, &hit, &b);
    b.y = pos.y;
    *opos = b;
    orot->z = 0.0f;
    orot->x = 0.0f;
    orot->y = atan2(-nrm.x, -nrm.z);
    return 1;
}

// Never called: only its initializer template survives in `.rodata` (the `{0, 1000, 300}` words
// between getScrActionPoint's pool and moveFance's).
static inline void subFanceDummy(Vec* out)
{
    Vec v = { 0.0f, 1000.0f, 300.0f };

    *out = v;
}

// Routine 0 / 8: climb over a fence.
void cSubChar::moveFance()
{
    switch (subSelf->r_no_2) {
    case 0: {
        Vec p;
        Vec r;

        if (subSelf->r_no_3 == 0) {
            if (!getScrActionPoint(&p, &r, 0x20)) {
                SubRoutineSet(this, 0, 0, 0, 0);
                break;
            }
        } else {
            p = pos;
            r.y = fWork0;
            r.x = 0.0f;
            r.z = 0.0f;
        }
        SUB_MOTBASE(this)->set((cMotModel*) this, &p, &r, 10);
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x2C), SUB_MOT(subSelf, 0x67), 7, 5, 0);
        AtariOff(&subSelf->atari, 0xFEFF);
        atari.setPriority(PRI_LV2);
        subSelf->dmg.set(0, 0x80);
        subSelf->cCoord::matUpdate();
        subSelf->r_no_2 = 1;
    }
    case 1:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (subSelf->motionMove()) {
            AtariOn(&subSelf->atari, 0x100);
            atari.setPriority(0);
            subSelf->dmg.clear();
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Floor under the partner (2000 up): 1 when found, pos.y moved onto it.
int cSubChar::landCheck()
{
    Vec a = { 0.0f, 2000.0f, 0.0f };
    Vec hit;
    Vec nrm;

    PSVECAdd(&a, &pos, &a);
    if (SatMgr.hitCheck(&a, &pos, &hit, &nrm, 0, 0)) {
        pos.y = hit.y;
        return 1;
    }
    return 0;
}

// Player damage handlers while the partner drops down a ledge (SetPlDamage).
void pl_fall_ok0()
{
}

// Player custom damage routine (SetPlDamage) of the ledge catch: plays the catch motion from the
// partner's archive (0x3E / 0x68), hides the weapon meanwhile, then endAction / EndPlDamage.
void pl_fall_ok(cPlayer* pl)
{
    PlArc* arc;

    arc = pPL->pEmCatch->subArc;
    pl->subArc = arc;
    switch (pl->r_no_1) {
    case 0:
        MOT_SET(pl, MOTION(pl), PL_ARC_PTR(arc, 0x3E), PL_ARC_PTR(arc, 0x68), 0, 5, 0);
        pl->Wep->setTrans(0, 0);
        pl->r_no_1 = 1;
    case 1:
        if (pl->motionMove()) {
            pl->Wep->setTrans(1, 0);
            pl->endAction(0);
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Routine 0 / 9: the player lets her down a ledge.
void cSubChar::moveFall()
{
    static const Vec v_ok = { -501.4f, 3200.0f, 106.0f };
    void* m;

    switch (r_no_2) {
    case 0:
        getFallPos(this, &pPL->pos, &pPL->ang);
        FSet(pPL->ang.y, pPL->ang.y + 4.712389f);
        FSet(pPL->ang.y, LIMIT_ANGLE(pPL->ang.y));
        pPL->cCoord::matUpdate();
        SetPlDamage(this, (void (*)(cPlayer*)) pl_fall_ok0);
        pPL->dmg.set(0, 0x80);
        if (SUBFLAG2(this)->check(7)) {
            m = SUB_MOT(subSelf, 0x46);
        } else {
            m = SUB_MOT(subSelf, 0x47);
        }
        motionSet(m, 3, 0, 0x201, 0);
        BitOff16(subFlags2, 0x80);
        AtariOff(&atari, 0xFCFF);
        dmg.set(0, 0x80);
        m_PlActTime = 0;
        subFlags |= 0x20;
        r_no_2 = 1;
    case 1:
        motionMove();
        if (subSelf->frame >= 41.0f) {
            r_no_2 = 4;
        }
        break;
    case 4:
        AtariOff(&pPL->atari, 0xFCFF);
        AtariOn(&atari, 0x200);
        AtariOff(&atari, 0xFEFF);
        atari.setPriority(PRI_LV3);
        ang.y = pPL->ang.y - 4.712389f;
        FSet(ang.y, LIMIT_ANGLE(ang.y));
        PSMTXMultVec(pPL->mat, &v_ok, &pos);
        setPos(&pos);
        SetPlDamage(this, pl_fall_ok);
        pPL->dmg.set(0, 0x80);
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x3F), SUB_MOT(subSelf, 0x5F), 7, 5, 0);
        AtariOff(&atari, 0xFEFF);
        r_no_2 = 5;
    case 5:
        if (motionMove()) {
            AtariOn(&atari, 0x300);
            atari.setPriority(0);
            dmg.clear();
            BitOff16(subFlags, 0x20);
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 0xA: one-shot actions selected by subHideMode (jump down, climb, registered motion).
void cSubChar::moveAction()
{
    void* m = 0;
    void* seq;

    switch (r_no_2) {
    case 0:
        switch (subHideMode) {
        default:
            pLog->err(0, 0, "ASHLEY UNKNOWN ACTION %d", subHideMode);
        case 0:
            m = SUB_MOT(subSelf, 0x1B);
            seq = SUB_MOT(subSelf, 0x54);
            break;
        case 1:
            m = SUB_MOT(subSelf, 0x1C);
            seq = SUB_MOT(subSelf, 0x55);
            break;
        case 2:
            m = subSelf->subMot0;
            seq = subSelf->subMot1;
            break;
        case 3:
            m = SUB_MOT(subSelf, 0x47);
            seq = SUB_MOT(subSelf, 0x7C);
            break;
        case 4:
            m = SUB_MOT(subSelf, 0x48);
            seq = SUB_MOT(subSelf, 0x5C);
            break;
        }
        switch (subHideMode) {
        default:
            r_no_2 = 1;
            break;
        case 3:
            r_no_2 = 2;
            break;
        case 4:
            r_no_2 = 4;
            fWork0 = getJumpAdjY();
            break;
        }
        if (subHideMode == 4) {
            subSndId = SndCall(8, 0x12, &subSelf->pParts->world, id, 0, 0);
        }
        MOT_SET(subSelf, MOTION(subSelf), m, seq, 3, 0x101, 0);
        AtariOff(&atari, 0xFCFF);
        dmg.set(0, 0x80);
    case 1:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (motionMove()) {
            AtariOn(&atari, 0x300);
            dmg.clear();
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 2:
        StaFlagOn(pG, STA_SUB_LADDER);
        motionMove();
        if (subSelf->frame >= 40.0f && landCheck()) {
            AtariOn(&atari, 0x300);
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x2D), SUB_MOT(subSelf, 0x5A), 0, 0x101, 0);
            motionMove();
            r_no_2 = 3;
        }
        break;
    case 3:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (motionMove()) {
            AtariOn(&atari, 0x300);
            dmg.clear();
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 4:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (subSelf->frame <= 9.0f) {
            jumpAdjust();
        }
        if (subSelf->frame >= 35.0f && subSelf->frame <= 44.0f) {
            pos.y += fWork0 * 0.1f;
        }
        if (motionMove()) {
            AtariOn(&atari, 0x300);
            dmg.clear();
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 0xC: climb a ladder (subHideMode: 0 up, 1 down; subX534 rungs).
void cSubChar::moveLadder()
{
    void* m;
    void* seq;

    StaFlagOn(pG, STA_SUB_LADDER);
    switch (r_no_2) {
    case 0:
        if (subHideMode) {
            m = SUB_MOT(subSelf, 0x4C);
            seq = SUB_MOT(subSelf, 0x64);
        } else {
            m = SUB_MOT(subSelf, 0x49);
            seq = SUB_MOT(subSelf, 0x61);
        }
        MOT_SET(subSelf, MOTION(subSelf), m, seq, 3, 5, 0);
        r_no_2 = 1;
        subFlags |= 0x20;
    case 1:
        if (motionMove()) {
            r_no_2 = 2;
        }
        break;
    case 2:
        if (subHideMode) {
            m = SUB_MOT(subSelf, 0x4D);
            seq = SUB_MOT(subSelf, 0x65);
        } else {
            m = SUB_MOT(subSelf, 0x4A);
            seq = SUB_MOT(subSelf, 0x62);
        }
        MOT_SET(subSelf, MOTION(subSelf), m, seq, 3, 5, 0);
        r_no_2 = 3;
    case 3:
        if (motionMove()) {
            subX534--;
            if (subX534 <= 0) {
                r_no_2 = 4;
            }
        }
        break;
    case 4:
        if (subHideMode) {
            m = SUB_MOT(subSelf, 0x4E);
            seq = SUB_MOT(subSelf, 0x66);
        } else {
            m = SUB_MOT(subSelf, 0x4B);
            seq = SUB_MOT(subSelf, 0x63);
        }
        MOT_SET(subSelf, MOTION(subSelf), m, seq, 3, 5, 0);
        r_no_2 = 5;
    case 5:
        if (motionMove()) {
            AtariOn(&atari, 0x300);
            dmg.clear();
            BitOff16(subFlags, 0x20);
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Height difference to the floor 3800 behind the action wall (0 when more than 1500 away).
f32 cSubChar::getJumpAdjY()
{
    Vec v;
    f32 h;
    const f32 far = 3800.0f;   // pool order 3800, 1500; `lim` at the top keeps 1500 in f31 across the calls
    const f32 lim = 1500.0f;

    v.x = -satNorm.x;
    v.y = satNorm.y;
    v.z = -satNorm.z;
    PSVECScale(&v, &v, far);
    PSVECAdd(&v, &satCross, &v);
    v.y += lim;
    h = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
    h -= pos.y;
    if (fabsf(h) > lim) {
        h = 0.0f;
    }
    return h;
}

// Slide the partner sideways (35) off the posts while she jumps over a wall: probes 300 to each
// side at height 300, 800 forward.
void cSubChar::jumpAdjust()
{
    Vec a;
    Vec b;

    a.x = 300.0f;
    a.y = 300.0f;
    a.z = 0.0f;
    PSMTXMultVec(mat, &a, &a);
    b.x = 300.0f;
    b.y = 300.0f;
    b.z = 800.0f;
    PSMTXMultVec(mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x80000)) {
        a.x = -35.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        PSMTXMultVecSR(mat, &a, &a);
        PSVECAdd(&pos, &a, &pos);
    }
    a.x = -300.0f;
    a.y = 300.0f;
    a.z = 0.0f;
    PSMTXMultVec(mat, &a, &a);
    b.x = -300.0f;
    b.y = 300.0f;
    b.z = 800.0f;
    PSMTXMultVec(mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x80000)) {
        a.x = 35.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        PSMTXMultVecSR(mat, &a, &a);
        PSVECAdd(&pos, &a, &pos);
    }
}

// Routine 0 / 0xE: turn back towards the player (the "come back" reaction).
void cSubChar::moveBack()
{
    switch (r_no_2) {
    case 0:
        if (mot_ck()) {
            motionSet(SUB_MOT(subSelf, 0x14), 8, 0, 1, 0);
        } else {
            motionSet(SUB_MOT(subSelf, 0x70), 8, 0, 1, 0);
        }
        r_no_3 = 0;
        r_no_2 = 1;
    case 1:
        motionMove();
        if (++r_no_3 > 30) {
            r_no_2 = 2;
        }
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        break;
    case 2:
        if (mot_ck()) {
            motionSet(SUB_MOT(subSelf, 0x12), 8, 0, 1, 0);
        } else {
            motionSet(SUB_MOT(subSelf, 0x6E), 8, 0, 1, 0);
        }
        r_no_2 = 3;
        break;
    case 3:
        motionMove();
        break;
    }
    if (!StaFlagChk(pG, STA_PL_CATCHED)) {
        SubRoutineSet(this, 0, 0, 0, 0);
    }
}

// Routine 0 / 0xF: run the scenario's own handler (SetSubAux).
void cSubChar::moveAux()
{
    ((void (*)(cSubChar*)) subAux0)(this);
}

// Routine 0 / 0x10: run to the hide spot (SubCharCtrlHide) and duck / climb into it.
void cSubChar::moveHide()
{
    static Vec norm;
    static Vec vdz0 = { 1.0f, 0.0f, 0.0f };
    Vec a;
    Vec b;
    Vec hit;
    f32 ang;
    u32 r;

    switch (r_no_2) {
    case 0: {
        void* m;
        void* seq;

        BitOff16(subFlags, 2);
        subHidePos.y += 300.0f;
        ang = Muku(&pos, &subHidePos, this->ang.y, 3.1415927f);
        if (fabsf(ang) > 2.0943952f) {
            if (mot_ck()) {
                m = SUB_MOT(subSelf, 0x1A);
                seq = SUB_MOT(subSelf, 0x53);
            } else {
                m = SUB_MOT(subSelf, 0x72);
                seq = 0;
            }
            r_no_2 = 2;
            r = 4;
        } else if (fabsf(ang) > 1.0471976f) {
            if (mot_ck()) {
                m = SUB_MOT(subSelf, 0x16);
                seq = SUB_MOT(subSelf, 0x52);
            } else {
                m = SUB_MOT(subSelf, 0x71);
                seq = 0;
            }
            r_no_2 = 1;
            r = 4;
        } else {
            if (mot_ck()) {
                m = SUB_MOT(subSelf, 0x16);
                seq = SUB_MOT(subSelf, 0x52);
            } else {
                m = SUB_MOT(subSelf, 0x71);
                seq = 0;
            }
            r_no_2 = 5;
            r = 4;
        }
        MOT_SET(this, MOTION(this), m, seq, 7, (u16) r, 0);
        motionMove();
        r_no_2 = 1;
        break;
    }
    case 1:
        ang = Muku(&pos, &subHidePos, this->ang.y, 0.62831855f);
        subSelf->ang.y += ang;
        if (fabsf(ang) < 0.44879895f) {
            r_no_2 = 5;
        }
        motionMove();
        break;
    case 2:
        if (motionMove()) {
            r_no_2 = 5;
        }
        break;
    case 5:
        if (mot_ck()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x16), SUB_MOT(subSelf, 0x52), 7, 5, 0);
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x71), 0, 7, 5, 0);
        }
        r_no_2 = 6;
        break;
    case 6: {
        f32 d;
        int up;

        a.x = pos.x;
        a.y = subHidePos.y;
        a.z = pos.z;
        b.x = subHidePos.x;
        b.y = subHidePos.y;
        b.z = subHidePos.z;
        r = SatMgr.hitCheck(&a, &b, &hit, &norm, 0, 0);
        d = GetDistance(a, hit);
        if ((r & 8) && d < 250000.0f) {
            subX534 = 0;
            subHidePos = hit;
            switch (subHideMode) {
            case 1:
            default:
                r_no_2 = 0xA;
                break;
            case 2:
                r_no_2 = 0x14;
                break;
            }
        }
        up = subHidePos.y > pos.y + 1000.0f;
        RouteCkToPos(this, &b, &a, up | 2, &subX5C8);
        subSelf->ang.y += Muku(&pos, &a, this->ang.y, 0.20943952f);
        motionMove();
        break;
    }
    case 0xA:
        MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x2C), SUB_MOT(subSelf, 0x67), 7, 5, 0);
        this->ang.y = atan2f(-norm.x, -norm.z);
        cCoord::matUpdate();
        PSVECScale(&norm, &a, 400.0f);
        PSVECAdd(&a, &subHidePos, &a);
        pos.x = a.x;
        pos.z = a.z;
        AtariOn(&atari, 0x200);
        subSelf->atari.setPriority(PRI_LV3);
        AtariOff(&atari, 0xFEFF);
        dmg.set(0, 0x80);
        fWork0 = getAdjustX(8) * 0.1f;
        sub538 = 10;
        r_no_2 = 0xB;
    case 0xB:
        if (fWork0 != 0.0f && sub538) {
            Vec v;

            PSMTXMultVecSR(subSelf->mat, &vdz0, &v);
            PSVECScale(&v, &v, fWork0);
            PSVECSubtract(&pos, &v, &pos);
            sub538--;
        }
        if (motionMove()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x43), 0, 7, 1, 0);
            StaFlagOn(pG, STA_ASHLEY_HIDE);
            sub538 = 6;
            r_no_2 = 0xC;
        }
        break;
    case 0xC:
        if (motionMove()) {
            r_no_3 = 10;
            r_no_2 = 0xD;
        }
        break;
    case 0xD:
        motionMove();
        r_no_3--;
        if (r_no_3 == 0) {
            this->ang.y = LIMIT_ANGLE(this->ang.y + 3.1415927f);
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x44), 0, 7, 5, 0);
            r_no_2 = 0xE;
        }
        break;
    case 0xE:
        motionMove();
        if (subX534) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x45), 0, 7, 5, 0);
            r_no_2 = 0xF;
            subX534 = frameMax - 3;
        }
        break;
    case 0xF:
        if (subX534 > 0) {
            Vec v;

            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 200.0f / (f32) (frameMax - 3);
            PSMTXMultVecSR(mat, &v, &v);
            PSVECAdd(&pos, &v, &pos);
            subX534--;
        }
        if (motionMove()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x2C), SUB_MOT(subSelf, 0x67), 7, 5, 0);
            r_no_2 = 0x10;
        }
        break;
    case 0x10:
        if (motionMove()) {
            u8 z = 0;

            StaFlagOff(pG, STA_ASHLEY_HIDE);
            subSelf->atari.setPriority(0);
            AtariOn(&atari, 0x300);
            dmg.clear();
            SubRoutineSet(this, z, z, z, z);
        }
        break;
    }
}

// Routine 0 / 0x11: stoop while the player is down.
void cSubChar::moveStoop()
{
    if (!plDownCheck()) {
        subHideMode = 1;
    }
    switch (r_no_2) {
    case 0:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x27), 0, 7, 5, 0);
        subHideMode = 0;
        r_no_2 = 1;
    case 1:
        if (motionMove()) {
            MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x28), 0, 7, 5, 0);
            r_no_2 = 2;
            subFlags2 |= 0x20;
        }
        break;
    case 2:
        if (subHideMode && !SUBFLAG(this)->check(1)) {
            r_no_2 = 3;
        }
        motionMove();
        break;
    case 3:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x29), 0, 7, 5, 0);
        r_no_2 = 4;
        BitOff16(subFlags2, 0x20);
        break;
    case 4:
        if (motionMove()) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 0x12: wait on the ledge for the player to catch her.
void cSubChar::moveFallWait()
{
    const f32 len = 1300.0f;         // pool order: 1300, 0.314 first
    const f32 spd = 0.31415927f;
    Vec v;
    f32 ang;

    switch (r_no_2) {
    case 0:
        sub53C = 0;
        sub540 = 0;
        if (mot_ck()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x13), SUB_MOT(subSelf, 0x50), 7, 5, 0);
            backCheckSet(SUB_MOT(subSelf, 0x15));
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6F), 0, 7, 5, 0);
            backCheckSet(0);
        }
        subX534 = 0;
        if (m_PlActTime) {
            subHideMode = 5;
            r_no_2 = 1;
        } else {
            m_PlActAngY = atan2(satNorm.x, satNorm.z) + 3.1415927f;
            m_PlActAngY = LIMIT_ANGLE(m_PlActAngY);
            subHideMode = 5;
            r_no_2 = 1;
        }
    case 1:
        motionMove();
        ang = Muku2(subSelf->ang.y, m_PlActAngY, 0.31415927f);
        subSelf->ang.y += ang;
        if (fabsf(ang) < 0.15707964f) {
            subHideMode--;
            if (subHideMode <= 0) {
                if (SUBFLAG2(this)->check(7) || (checkSatAttr(500.0f) & 0x100010)) {
                    subHideMode = 0;
                    r_no_2 = 2;
                } else {
                    r_no_2 = 4;
                }
            }
        }
        break;
    case 2:
        if (mot_ck()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x12), SUB_MOT(subSelf, 0x4F), 7, 5, 0);
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6E), 0, 7, 5, 0);
        }
        sub538 = 0;
        r_no_2 = 3;
    case 3:
        motionMove();
        break;
    case 4:
        if (mot_ck()) {
            motionSet(SUB_MOT(subSelf, 0x14), 8, 0, 1, 0);
        } else {
            motionSet(SUB_MOT(subSelf, 0x70), 8, 0, 1, 0);
        }
        r_no_2 = 5;
    case 5:
        if (motionMove()) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1300.0f;
    PSMTXMultVec(mat, &v, &v);
    v.y = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0) + 300.0f;
    if (GetDistance(&v, &pPL->pos) > 25000000.0f) {
        sub540++;
        if (sub540 == 300) {
            subSndId = SndCall(8, 0x14, &subSelf->pParts->world, id, 0, 0);
            sub540 = 0;
        }
    } else if (sub540 <= 0x95) {
        sub540++;
    }
    if (subX534 == 0) {
        if (subHideMode == 0 && (pPL->r_no_0 != 0 || pPL->r_no_1 != 0xE)) {
            subX534 = 1;
        }
    }
    if (subX534) {
        if (GetDistance(&v, &pPL->pos) < 9000000.0f && !SatMgr.hitCheck(&v, &pPL->pParts->world, 0, 0, 0, 0) &&
            !SatMgr.hitCheck(&pPL->pParts->world, &v, 0, 0, 0, 0)) {
            ActBtn.set(0x1C, 6, (int) catchOn, 0, 0, 1, 0, 0);
            sub53C = 1;
        }
    }
    if (!SatMgr.hitCheck(&pParts->world, &pPL->pParts->world, 0, 0, 0, 0)) {
        BitOff16(subFlags2, 0x80);
        SubRoutineSet(this, 0, 0, 0, 0);
    }
}

// Routine 0 / 0x13: wait at the ladder until the player is in sight again.
void cSubChar::moveLadderWait()
{
    switch (r_no_2) {
    case 0:
        if (mot_ck()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x19), 0, 7, 5, 0);
            r_no_2 = 2;
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6E), 0, 7, 5, 0);
            r_no_2 = 1;
        }
        break;
    case 1:
        motionMove();
        break;
    case 2:
        motionMove();
        break;
    }
    if (!SatMgr.hitCheck(&pParts->world, &pPL->pParts->world, 0, 0, 0, 0) &&
        !SatMgr.hitCheck(&pPL->pParts->world, &pParts->world, 0, 0, 0, 0)) {
        SubRoutineSet(this, 0, 0, 0, 0);
    }
}

// Routine 0 / 0x14: wait at the window until the player is in sight again.
void cSubChar::moveWindowWait()
{
    switch (r_no_2) {
    case 0:
        if (mot_ck()) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x19), 0, 7, 5, 0);
            r_no_2 = 2;
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6E), 0, 7, 5, 0);
            r_no_2 = 1;
        }
        break;
    case 1:
        motionMove();
        break;
    case 2:
        motionMove();
        break;
    }
    if (!SatMgr.hitCheck(&pParts->world, &pPL->pParts->world, 0, 0, 0, 0) &&
        !SatMgr.hitCheck(&pPL->pParts->world, &pParts->world, 0, 0, 0, 0)) {
        SubRoutineSet(this, 0, 0, 0, 0);
    }
}

// Scenario attribute of the wall `len` ahead (300 up).
u32 cSubChar::checkSatAttr(f32 len)
{
    Vec a;
    Vec b;

    a = pos;
    b.z = len;
    a.y += 300.0f;
    b.x = 0.0f;
    b.y = 300.0f;
    PSMTXMultVec(mat, &b, &b);
    return SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
}

// Sideways offset (in tenths of `width`) to the first wall of attribute `attr` 1000 ahead,
// scanning right then left; 0 when none.
f32 cSubChar::getAdjustX(int attr)
{
    static f32 width = 550.0f;
    static f32 height = 300.0f;
    Vec a;
    Vec b;
    Vec a0;
    Vec b0;
    Vec step;
    int i;
    int j;

    a.x = 0.0f;
    a.y = height;
    a.z = 0.0f;
    PSMTXMultVec(subSelf->mat, &a, &a);
    a0 = a;
    b.x = 0.0f;
    b.y = height;
    b.z = 1000.0f;
    PSMTXMultVec(subSelf->mat, &b, &b);
    b0 = b;
    step.x = width / 10.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    PSMTXMultVecSR(subSelf->mat, &step, &step);
    for (i = 1; i <= 10; i++) {
        if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & attr)) {
            return width - width * (f32) i / 10.0f;
        }
        PSVECAdd(&a, &step, &a);
        PSVECAdd(&b, &step, &b);
    }
    a = a0;
    b = b0;
    for (j = 1; j <= 10; j++) {
        if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & attr)) {
            return -width - -width * (f32) j / 10.0f;
        }
        PSVECSubtract(&a, &step, &a);
        PSVECSubtract(&b, &step, &b);
    }
    return 0.0f;
}

// Routine 1: damage reaction (subHideMode selects the motion / effect).
void cSubChar::moveDamage()
{
    if (subAux1) {
        ((void (*)()) subAux1)();
        return;
    }
    void* m = 0;   // declared after the handler test: `li r30, 0` in the switch block

    switch (r_no_1) {
    case 0:
        beginDamage();
        // raw store through the info's address: the pG load below stays after the flag store
        AtariOnRaw(&atari, 0x300);
        if (DbgFlagChk(pG, DBG_NO_DEATH)) {
            switch (subHideMode) {
            case 7:
                subHideMode = 8;
                break;
            case 9:
                subHideMode = 10;
                break;
            }
        }
        // one body per value (no shared labels): the compare tree tests every value; the
        // identical bodies are cross-jumped afterwards
        switch (subHideMode) {
        case 0:
            m = SUB_MOT(subSelf, 0x2E);
            break;
        case 1:
            m = SUB_MOT(subSelf, 0x2F);
            break;
        case 2:
            m = SUB_MOT(subSelf, 0x2E);
            break;
        case 3:
            m = SUB_MOT(subSelf, 0x2F);
            break;
        case 4:
            m = SUB_MOT(subSelf, 0x2E);
            break;
        case 5:
            m = SUB_MOT(subSelf, 0x2F);
            break;
        case 6:
            m = 0;
            break;
        case 7:
            m = SUB_MOT(subSelf, 0x32);
            EstSet((int) subSelf, -1, 0, 0, 4, ChkWaterEffectEnable(&pos) ? 6 : 5, 0, 0, (u32) subSelf, 0);
            break;
        case 8:
            m = SUB_MOT(subSelf, 0x32);
            EstSet((int) subSelf, -1, 0, 0, 4, ChkWaterEffectEnable(&pos) ? 6 : 5, 0, 0, (u32) subSelf, 0);
            break;
        case 9:
            m = SUB_MOT(subSelf, 0x31);
            EstSet((int) subSelf, -1, 0, 0, 4, ChkWaterEffectEnable(&pos) ? 8 : 7, 0, 0, (u32) subSelf, 0);
            break;
        case 10:
            m = SUB_MOT(subSelf, 0x31);
            EstSet((int) subSelf, -1, 0, 0, 4, ChkWaterEffectEnable(&pos) ? 8 : 7, 0, 0, (u32) subSelf, 0);
            break;
        case 11:
            m = SUB_MOT(subSelf, 0x6A);
            AtariOff(&atari, 0xFCFF);
            break;
        }
        MOT_SET(subSelf, MOTION(subSelf), m, 0, 3, 1, 0);
        setFace(1);
        SndCall(8, 9, &subSelf->pParts->world, id, 0, 0);
        subSelf->r_no_1 = 1;
    case 1:
        if (subSelf->frame >= 10.0f && subHideMode == 11 && landCheck()) {
            AtariOn(&atari, 0x300);
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6B), 0, 3, 1, 0);
            EstSet((int) subSelf, -1, 0, 0, 4, ChkWaterEffectEnable(&pos) ? 10 : 9, 0, 0, (u32) subSelf, 0);
            r_no_1 = 10;
            if (DbgFlagChk(pG, DBG_NO_DEATH)) {
                subHideMode = 8;
                r_no_1 = 5;
            }
        }
        if (subSelf->motionMove()) {
            switch (subHideMode) {   // default first (laid out first); separate identical bodies
            default:
                setFace(0);
                SubRoutineSet(this, 0, 0, 0, 0);
                break;
            case 6:
            case 7:
                r_no_1 = 2;
                break;
            case 8:
                r_no_1 = 5;
                break;
            case 9:
                r_no_1 = 2;
                break;
            case 10:
                r_no_1 = 5;
                break;
            case 11:
                r_no_1 = 7;
                break;
            }
        }
        break;
    case 2:
        if (MotionMoveF(subSelf, 0)) {
            if (subHideMode == 7 || subHideMode == 9) {
                hp = 0;
                pG->ashley_life = 0;
            }
        }
        break;
    case 5:
        if (subHideMode == 8) {
            m = SUB_MOT(subSelf, 0x33);
        } else {
            m = SUB_MOT(subSelf, 0x34);
        }
        MOT_SET(subSelf, MOTION(subSelf), m, 0, 3, 5, 0);
        r_no_1 = 6;
    case 6:
        if (motionMove()) {
            endDamage();
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 7:
        motionMove();
        break;
    case 10:
        motionMove();
        break;
    }
}

// Routine 2: death.
void cSubChar::moveDie()
{
    switch (r_no_1) {
    case 0:
        CamCtrl.deleteAttachCamera((AttachCamera*) pPL->p2A4, pPL);
        if (SUBFLAG2(this)->check(5)) {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x30), 0, 3, 1, 0x32);
        } else {
            MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x30), 0, 3, 1, 0);
            EstSet((int) this, -1, 0, 0, 4, ChkWaterEffectEnable(&pos) ? 4 : 3, 0, 0, (u32) this, 0);
        }
        SndCall(8, 0xD, &pParts->world, id, 0, 0);
        atari.m_parts_no = 4;
        r_no_1 = 1;
    case 1:
        if (motionMove()) {
            AtariOff(&atari, 0xFCFF);
            r_no_1 = 2;
        }
        break;
    default:
        motionMove();
        break;
    }
}

// Routine 3: the bulldozer scenario's own handler (SetSubBulldozer).
void cSubChar::moveBull()
{
    ((void (*)(cSubChar*)) subAux0)(this);
}

// Routine 5: scenario event: walk to subHidePos along the route.
void cSubChar::moveEvent()
{
    f32 ang;

    switch (r_no_1) {
    case 0:
        MotionMoveF(this, 0);
        break;
    case 1:
        switch (r_no_2) {
        case 0:
            if (mot_ck()) {
                MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x16), SUB_MOT(subSelf, 0x52), 10, 5, 0);
            } else {
                MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x71), SUB_MOT(subSelf, 0x78), 10, 5, 0);
            }
            r_no_2 = 1;
            motFlags &= ~1;
        case 1: {
            Vec out;

            RouteCkToPos(this, &subHidePos, &out, subHidePos.y > pos.y + 1000.0f, &subX5C8);
            ang = Muku(&pos, &out, this->ang.y, 0.44879895f);
            this->ang.y += ang;
            if (fabsf(ang) < 0.20943952f) {
                if (mot_ck()) {
                    MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x16), SUB_MOT(subSelf, 0x52), 10, 5, 0);
                } else {
                    MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x71), SUB_MOT(subSelf, 0x78), 10, 5, 0);
                }
                r_no_2 = 2;
            }
            break;
        }
        case 2: {
            Vec out;

            RouteCkToPos(this, &subHidePos, &out, subHidePos.y > pos.y + 1000.0f, &subX5C8);
            this->ang.y += Muku(&pos, &out, this->ang.y, 0.31415927f);
            if (GetDistance(pos, subHidePos) < 250000.0f) {
                if (mot_ck()) {
                    MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x12), SUB_MOT(subSelf, 0x4F), 7, 5, 0);
                } else {
                    MOT_SET(subSelf, MOTION(subSelf), SUB_MOT(subSelf, 0x6E), 0, 7, 5, 0);
                }
                SubRoutineSet(this, 5, 0, 0, 0);
            }
            break;
        }
        }
        motionMove();
        break;
    case 2:
        break;
    }
    partsWorldCalc();
    EmAtCheck(this);
    SatMgr.check(this, 0);
    atari.move();
    PartsWorldPosCalc(this);
    seqSeCtrl();
    moveCloth();
}

// Routine 6: crouch and stay down (the player is dead).
void cSubChar::moveDijection()
{
    switch (r_no_1) {
    case 0:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x43), 0, 7, 5, 0);
        r_no_1 = 1;
        break;
    case 1:
        if (motionMove()) {
            r_no_1 = 2;
        }
        break;
    case 2:
        MOT_SET(this, MOTION(this), SUB_MOT(subSelf, 0x44), 0, 7, 5, 0);
        r_no_2 = 3;
        break;
    case 3:
        motionMove();
        break;
    }
}

// Step `spd` towards `target` in the XZ plane.
void cSubChar::movePos(Vec* target, f32 spd)
{
    f32 ang;

    ang = Muku(&pos, target, 0.0f, 3.1415927f);
    Vec v = { 0.0f, 0.0f, 0.0f };
    v.z = spd;
    Vec r = { 0.0f, 0.0f, 0.0f };
    r.y = ang;
    RotVector(&v, &r, &v);
    PSVECAdd(&pos, &v, &pos);
}

// Neck straight, look request off.
void cSubChar::neckInit()
{
    subNeckOn = 0;
    subNeckX = 0.0f;
    subNeckAng = 0.0f;
    subNeckZ = 0.0f;
}

// Turn the neck (parts 3) towards the player while neckSet() keeps asking for it.
void cSubChar::neckCtrl()
{
    const f32 spdBack = 0.10471976f;   // pool order: the two speeds first
    const f32 spdHome = 0.15707964f;
    cModel* p = getPartsPtr(3);
    int on = 1;
    f32 ang;

    BitOn(MOTION_PARTS(p)->flags, 0x40000000);
    if (!(pPL->stat & 2)) {
        on = 0;
    }
    if (subNeckOn == 0 || (blendMot != 0 && blendMot->blendRate != 0.0f) || on) {
        subNeckAng += Muku2(subNeckAng, 0.0f, 0.15707964f);
    } else {
        if (subNeckOn > 0) {
            subNeckOn--;
        }
        ang = this->ang.y + subNeckAng;
        ang = Muku(&pos, &pPL->pos, ang, 0.10471976f);
        subNeckAng += ang;
        if (subNeckAng > 0.78539819f) {
            subNeckAng = 0.78539819f;
        } else if (subNeckAng < -0.78539819f) {
            subNeckAng = -0.78539819f;
        }
    }
    p->efmSpd.x = subNeckAng;
    subNeckOn = 0;
}

// Ask the neck to look at `pos` this frame (neckCtrl turns toward the player while requested).
void cSubChar::neckSet(Vec* pos)
{
    subNeckPos = *pos;
    subNeckOn = 1;
}

// Which action the partner should take this frame (0 none; the routine 0 sub routine selector).
int cSubChar::actCheck()
{
    if (readyOkCheck()) {
        return 7;
    }
    if (fanceCheck()) {
        return 1;
    }
    switch ((u32) windowCheck()) {
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return 4;
    }
    if (SubLadderClimbCk(this)) {
        return 5;
    }
    if (doorCheck()) {
        return 6;
    }
    if (actionCheck()) {
        return 8;
    }
    if (ladder2Check()) {
        return 9;
    }
    if (fallLadderCheck()) {
        return 0xB;
    }
    return 0;
}

// subFlags2 bit4: the aiming player has her in front of him within 10000.
int cSubChar::cautionCheck()
{
    int ret;

    if (SUBFLAG(this)->check(3)) {
        ret = 0;
    } else if (!(plStat & 0x10)) {
        ret = 0;
    } else if (fabsf(GetXZAngleLocal(&pPL->pos, &pos, pPL->ang.y)) > 0.78539819f) {
        ret = 0;
    } else if (GetDistance(pPL->pParts->world, pParts->world) > 100000000.0f) {
        ret = 0;
    } else {
        ret = 1;
    }
    if (ret == 1) {
        subFlags2 |= 0x10;
    } else {
        BitOff16(subFlags2, 0x10);
    }
    return ret;
}

// The player is crouching (plStat 0x8000).
int cSubChar::plDownCheck()
{
    return (plStat & 0x8000) != 0;
}

// A fence (attribute 0x20) 600 ahead with a floor within 300 of the partner's height 1500 behind it.
int cSubChar::fanceCheck()
{
    Vec a = { 0.0f, 400.0f, -300.0f };

    PSMTXMultVec(subSelf->mat, &a, &a);
    Vec b = { 0.0f, 400.0f, 600.0f };
    PSMTXMultVec(subSelf->mat, &b, &b);
    Vec hit;
    Vec nrm;
    if (!(SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0) & 0x20)) {
        return 0;
    }
    a.x = 400.0f;
    a.y = 300.0f;
    a.z = 1000.0f;
    b.x = -400.0f;
    b.y = 300.0f;
    b.z = 1000.0f;
    PSMTXMultVec(subSelf->mat, &a, &a);
    PSMTXMultVec(subSelf->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    PSVECScale(&nrm, &a, -1500.0f);
    PSVECAdd(&a, &hit, &a);
    b.x = a.x;
    b.y = a.y - 10000.0f;
    b.z = a.z;
    SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0);
    if (fabsf(hit.y - pos.y) > 300.0f) {
        return 0;
    }
    return 1;
}

// Window 600 ahead: 1 climb through (sub52C = facing angle), 2 break it first, 3 blocked, 0 none.
int cSubChar::windowCheck()
{
    Vec a = { 0.0f, 400.0f, 0.0f };

    PSVECAdd(&a, &subSelf->pos, &a);
    Vec b = { 0.0f, 400.0f, 600.0f };
    PSMTXMultVec(subSelf->mat, &b, &b);
    Vec dir;
    Vec p;
    u16 status;
    cEmWindow* w;
    f32 ang;
    if (ChkWindow(this, &a, &b, 1, &status, &dir, &p, &w)) {
        if ((w->ChkStatus() & 1) == 0) {
            return 3;
        }
        ang = atan2f(-dir.x, -dir.z);
        if (EmRackCk(this, &pos, ang) == 0) {
            return 3;
        }
        if (w->ChkBreakDir(&subSelf->pos) == 2) {
            subFlags2 |= 0x80;
            SubRoutineSet(this, 0, 0x12, 0, 0);
            return 2;
        }
        fWork0 = atan2(-dir.x, -dir.z);
        return 1;
    }
    return 0;
}

// A ladder to climb down below the target.
int cSubChar::fallLadderCheck()
{
    Vec d;

    if (!SubLadderClimbCk2(this)) {
        return 0;
    }
    PSVECSubtract(&distPos, &pos, &d);
    if (VecElevation(&d) < 0.78539819f) {
        return 0;
    }
    return 1;
}

// A door in front to open (not while she is being told to wait).
int cSubChar::doorCheck()
{
    cEmDoor* d = DoorOpenCk(this);

    if (d == 0) {
        return 0;
    }
    if (SUBFLAG2(this)->check(1)) {
        return 0;
    }
    SubOpenDoorSet(d);
    return 1;
}

// The player is aiming (plStat 0x10) and she is not flagged 3 (event-held).
int cSubChar::readyCheck()
{
    if (SUBFLAG(this)->check(3)) {
        return 0;
    }
    if (!(plStat & 0x10)) {
        return 0;
    }
    return 1;
}

// Action button: the player catches the partner waiting on the ledge.
void catchOn()
{
    cSubChar* sub = pSUB;
    Vec p;
    Vec r;

    pPL->dmg.set(0, 0x80);
    sub->dmg.set(0, 0x80);
    SubRoutineSet(sub, 0, 9, 0, 0);
    if (sub->m_PlActTime) {
        p = sub->m_PlActPos;
        p.y = sub->pos.y;
        if (fabsf(sub->pos.y - sub->m_PlActPos.y) < 300.0f && GetDistance(&p, &sub->pos) < 25000000.0f) {
            f32 y = sub->m_PlActAngY;

            sub->setPos(&sub->m_PlActPos);
            r.y = y;
            r.x = 0.0f;
            r.z = 0.0f;
            sub->setAng(&r);
        }
    }
    sub->m_PlActTime = 0;
}

// Wall in front (analyze's sub438 attribute): pick the action (jump down / climb / wait) for it.
int cSubChar::actionCheck()
{
    Vec a;
    Vec b;
    int ret = 0;

    a.x = 0.0f;
    a.y = 300.0f;
    a.z = -300.0f;
    PSMTXMultVec(mat, &a, &a);
    b = pPL->pos;
    b.y += 300.0f;
    if (!SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    if (satAttr & 0x200000) {
        subSelf->subHideMode = 0;
        ret = 1;
    } else if (satAttr & 0x2000) {
        subSelf->subHideMode = 1;
        ret = 1;
    } else if (satAttr & 0x1000) {
        subSelf->subHideMode = 2;
        ret = 1;
    } else if (satAttr & 0x80000) {
        subSelf->subHideMode = 4;
        ret = 1;
    } else if (satAttr & 0x100010) {
        int up = pPL->pos.y < pos.y - 1000.0f;

        if (up) {
            if (getCliffHeight(atan2(-satNorm.x, -satNorm.z)) < 2900.0f) {
                subSelf->subHideMode = 3;
                ret = 1;
            } else if (m_PlActTime == 0) {
                r_no_0 = 0;
                r_no_1 = 0x12;
                r_no_2 = 0;
                r_no_3 = 0;
            }
        }
    }
    if (ret == 1) {
        Vec d;

        d.x = -satNorm.x;
        d.y = 0.0f;
        d.z = -satNorm.z;
        ang.y += Muku3(&d, ang.y, 3.1415927f);
    }
    if (m_PlActTime && dist <= 300.0f && (pPL->r_no_0 != 0 || pPL->r_no_1 != 0xE)) {
        if (getCliffHeight(m_PlActAngY) < 2900.0f) {
            m_PlActTime = 0;
            pos = m_PlActPos;
            ang.y = m_PlActAngY;
            subSelf->subHideMode = 3;
            ret = 1;
        } else {
            SubRoutineSet(this, 0, 0x12, 0, 0);
        }
    }
    return ret;
}

// A ladder at the partner's position while the player climbs one (routine 0x10): set her on it.
int cSubChar::ladder2Check()
{
    Vec p;
    f32 ang;
    u8 level;
    int up;

    if (!SceAtCheckLadder(this, &p, &ang, &level)) {
        return 0;
    }
    if (fabsf(distPos.y - pos.y) < 1000.0f) {
        return 0;
    }
    if (pPL->r_no_0 == 0 && pPL->r_no_1 == 0x10) {
        return 0;
    }
    {
        Vec r;
        f32 a = ang;
        setPos(&p);
        r.y = a;
        r.x = 0.0f;
        r.z = 0.0f;
        setAng(&r);
    }
    up = 1;
    if ((s8) level > 0) {
        up = 0;
    }
    subHideMode = up;
    subX534 = ((s8) level < 0 ? -(s8) level : (s8) level) - 2;
    AtariOff(&atari, 0xFCFF);
    dmg.set(0, 0x80);
    return 1;
}

// Drop from the partner's height to the floor 1000 ahead in direction `ang` (100000 when < 800).
f32 cSubChar::getCliffHeight(f32 ang)
{
    const f32 len = 1000.0f;
    Vec v;
    Vec r;
    f32 h;

    r.y = ang;
    v.x = 0.0f;
    v.y = 300.0f;
    v.z = len;
    r.x = 0.0f;
    r.z = 0.0f;
    RotVector(&v, &r, &v);
    PSVECAdd(&pos, &v, &v);
    h = pos.y - SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
    if (h < 800.0f) {
        h = 100000.0f;
    }
    return h;
}

// subFlags2 bit2: the player's head is close below her and looking up.
void cSubChar::pantsCheck()
{
    Vec d;
    Vec* hp;
    Vec* sp;

    BitOff16(subFlags2, 4);
    if (pG->game_costume == 1) {
        return;
    }
    hp = &pPL->getPartsPtr(4)->world;
    sp = &pParts->world;
    if (GetDistance(hp, sp) > 25000000.0f) {
        return;
    }
    PSVECSubtract(sp, hp, &d);
    if (VecElevation(&d) < 0.78539819f) {
        return;
    }
    if (fabsf(Muku(hp, sp, pPL->ang.y, 6.2831855f)) > 0.78539819f) {
        return;
    }
    if (EatMgr.hitCheck(hp, sp, 0, 0, 0, 0)) {
        return;
    }
    subFlags2 |= 4;
}

// The player is running (plStat bit3).
int cSubChar::ckPlRun()
{
    return (plStat & 8) != 0;
}

// Motion sequence sound (seNo) -> SndCall: footsteps by surface kind (seFlags28B), voices as is.
void cSubChar::seqSeCtrl()
{
    int no;
    int parts;
    u16 blk;   // u16: no mask before the u16 SndCall argument

    if (seNo == 0) {
        return;
    }
    no = seNo - 1;
    parts = 0;
    blk = 5;
    switch (seFlags28B & 3) {
    case 0:
        switch ((u32) no) {   // unsigned: `cmplwi` range tests, case 0 as `< 1`
        case 0:
        case 2:
            parts = 0x14;
            blk = 5;
            no += 7;
            break;
        case 0x16:
            parts = 0x14;
            break;
        case 1:
        case 3:
            parts = 0x18;
            blk = 5;
            no += 7;
            break;
        case 0x17:
            parts = 0x18;
            break;
        case 4:
        case 5:
            no += 7;
            break;
        case 6:
            break;
        default:
            parts = 0;
            blk = 8;
            break;
        }
        break;
    case 1:
        blk = 8;
        break;
    case 2:
        blk = 6;
        break;
    case 3:
        blk = 1;
        break;
    }
    SndCall(blk, no, &getPartsPtr(parts)->world, id, 0, 0);
    seNo = 0;
}

// Blend the look-back motion `mot` in (NULL: off).
void cSubChar::backCheckSet(void* mot)
{
    if (mot) {
        f32 rate = subBackMot.blendRate;

        MOT_SET(this, &subBackMot, mot, 0, 3, 4, 0);
        subBackMot.blendRate = rate;
        blendMot = &subBackMot;
        subBackMot.flags2 |= 0x80000000;
    } else {
        blendMot = 0;
        subBackMot.blendRate = 0.0f;
    }
}

// Fade the look-back blend in (sub404 == 2) or out (1).
void cSubChar::backCheckMove()
{
    MotionWorkSub* w = subSelf->blendMot;
    const f32 d = 0.14f;

    if (w == 0) {
        return;
    }
    switch (m_BackRno) {
    case 0:
        break;
    case 1:
        if (w->blendRate > 0.0f) {
            w->blendRate -= d;
            if (subSelf->blendMot->blendRate < 0.0f) {
                subSelf->blendMot->blendRate = 0.0f;
                m_BackRno = 0;
            }
        }
        break;
    case 2:
        if (blendMot->blendRate < 1.0f) {
            blendMot->blendRate += d;
            if (blendMot->blendRate > 1.0f) {
                blendMot->blendRate = 1.0f;
                m_BackRno = 0;
            }
        }
        break;
    }
}

// Look back at an enemy behind her now and then (standing).
void cSubChar::backCheckCtrlFootwork()
{
    switch (m_BackRno2) {
    case 0:
        if (checkBackEm()) {
            m_BackRno = 2;
            m_BackTime = (u8) (Rnd() >> 2) + 30;
            m_BackRno2 = 1;
        }
        break;
    case 1:
        m_BackTime--;
        if (m_BackTime & 0x8000) {
            if (!checkBackEm()) {
                m_BackRno = 1;
                m_BackRno2 = 0;
            }
            m_BackTime = (u8) (Rnd() >> 2) + 30;
        }
        break;
    }
}

// Look back at an enemy behind her now and then (walking).
void cSubChar::backCheckCtrlMove()
{
    switch (m_BackRno2) {
    case 0:
        m_BackTime--;
        if (m_BackTime & 0x8000) {
            if (checkBackEm()) {
                m_BackRno = 2;
                m_BackTime = (u8) (Rnd() >> 2) + 30;
                m_BackRno2 = 1;
            }
        }
        break;
    case 1:
        m_BackTime--;
        if (m_BackTime & 0x8000) {
            if (!checkBackEm()) {
                m_BackRno = 1;
                m_BackRno2 = 0;
            }
            m_BackTime = (u8) (Rnd() >> 2) + 30;
        }
        break;
    }
}

// An alive, non-battle enemy behind her (within 20000, in the back cone) with a clear line of sight.
int cSubChar::checkBackEm()
{
    const f32 distFar = 400000000.0f;   // pool order: the far cone angle before the near ones
    const f32 distNear = 25000000.0f;
    const f32 angFar = 2.617994f;
    int i;
    int n = EmMgr.nArray;

    for (i = 0; i < n; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        f32 d;
        f32 ang;

        if ((em->be_flag & 0x201) != 1) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (em == pPL) {
            continue;
        }
        if (em == pSUB) {
            continue;
        }
        if (em->checkStatus(EM_STATUS_LOCKOFF)) {
            continue;
        }
        d = GetDistance(&pos, &em->pos);
        if (d > 400000000.0f) {
            continue;
        }
        ang = GetXZAngleLocal(&pos, &em->pos, this->ang.y);
        if (d < 25000000.0f) {
            if (ang < 2.3561945f && ang > -2.3561945f) {
                continue;
            }
        } else {
            if (ang < 2.617994f && ang > -2.617994f) {
                continue;
            }
        }
        if (SatMgr.hitCheck(&pParts->world, &em->pParts->world, 0, 0, 0, 0)) {
            continue;
        }
        return 1;
    }
    return 0;
}

// The damage routine handler (routine 4): pl_sub's SetSubDamage passes it in r4.
void cSubChar::setEmFunc()
{
    register void (*func)() asm("r4");

    subFunc = func;
}

// Per-frame situation: player distance flags, nearby enemies, the route target (subTarget),
// distance / angle to it, and the action checks.
void cSubChar::analyze()
{
    static const Vec chasePosFwd = { 400.0f, 300.0f, 200.0f };
    static const Vec chasePosBck = { 400.0f, 300.0f, -400.0f };
    static int subNear2 = 0;
    static u8 delayMove = 0;
    static u8 npcCheck = 0;
    Vec d;
    int i;
    int n;
    int up;
    int r;
    const f32 near = 4000000.0f;

    anaSatInfo();
    if (GetDistance(pos, pPL->pos) < near) {
        subFlags2 |= 2;
    } else {
        BitOff16(subFlags2, 2);
    }
    n = EmMgr.nArray;
    BitOff16(subFlags2, 0x201);
    if (!SUBFLAG(this)->check(3)) {
        for (i = 0; i < n; i++) {
            cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

            if (!em->isAlive()) {
                continue;
            }
            if (em->hp <= 0) {
                continue;
            }
            if (em->checkStatus(EM_STATUS_ACTIVE)) {
                continue;
            }
            if (em == this) {
                continue;
            }
            if (em == pPL) {
                continue;
            }
            if (em->id > 0x3F) {
                continue;
            }
            if (em->checkStatus(EM_STATUS_ASHLEY_NO_HELP)) {
                continue;
            }
            if (GetDistance(pos, em->pos) < 16000000.0f) {
                BitOn16(subFlags2, 0x200);
                if (subNear2) {
                    Draw_pos(&em->pos, 1000);
                }
            }
            if (GetDistance(pos, em->pos) < 1000000.0f) {
                subFlags2 |= 1;
            }
        }
    }
    if (pAnotherRoute && moveAnotherRoute()) {
        pAnotherRoute = 0;
    }
    if (m_PlActTime && !SatMgr.hitCheck(&pParts->world, &pPL->pParts->world, 0, 0, 0, 0)) {
        m_PlActTime = 0;
    }
    up = 0;
    if (fabsf(distPos.y - pos.y) > 1000.0f) {
        up = 1;
    }
    if (pAnotherRoute) {
        if (fabsf(pAnotherRoute->pos.y - pos.y) > 1000.0f) {
            up = 1;
        }
        r = RouteCkToPos(this, &pAnotherRoute->pos, &distPos, up, &subX5C8);
    } else if (SUBFLAG(this)->check(3)) {
        if (fabsf(subMoveTo[1] - pos.y) > 1000.0f) {
            up = 1;
        }
        r = RouteCkToPos(this, (Vec*) subMoveTo, &distPos, up, &subX5C8);
    } else if (m_PlActTime) {
        if (fabsf(m_PlActPos.y - pos.y) > 1000.0f) {
            up = 1;
        }
        r = RouteCkToPos(this, &m_PlActPos, &distPos, up, &subX5C8);
    } else {
        if (delayMove) {
            delayMove--;
        }
        if (plStat & 0xE) {
            delayMove = 5;
        }
        if (delayMove == 0 && (GetDistance(&pos, &pPL->pos) > 16000000.0f || !(plStat & 0xE))) {
            if (fabsf(pPL->pos.y - pos.y) > 1000.0f) {
                up = 1;
            }
            r = RouteCkToPos(this, &pPL->pos, &distPos, up, &subX5C8);
        } else {
            f32 fl;

            if (r_no_1 != 0 && r_no_1 != 2) {
                if (plStat & 4) {
                    distPos = chasePosFwd;
                } else {
                    distPos = chasePosBck;
                }
                PSMTXMultVec(pPL->mat, &distPos, &distPos);
            } else {
                distPos = pPL->pos;
            }
            fl = SatMgr.getFloor(&distPos, 1000.0f, 1000.0f, 0, 0);
            if (fl != -100000.0f) {
                distPos.y = fl + 100.0f;
            }
            if (SatMgr.hitCheck(&pPL->pParts->world, &distPos, 0, 0, 0, 0)) {
                PSVECSubtract(&distPos, &pPL->pParts->world, &d);
#line 3781 "D:/Bio4/Prog/pl_npc.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, -400.0f);
                PSVECAdd(&distPos, &d, &distPos);
            }
            if (fabsf(distPos.y - pos.y) > 1000.0f) {
                up = 1;
            }
            r = RouteCkToPos(this, &distPos, &distPos, up, &subX5C8);
        }
    }
    if (SatMgr.hitCheck(&pos, &distPos, 0, 0, 0, 0)) {
        dist = GetDistance3(&pos, &distPos);
    } else {
        d.x = distPos.x;
        d.y = pos.y;
        d.z = distPos.z;
        dist = GetDistance3(&pos, &d);
    }
    if (r == 0) {
        dist += 10000.0f;
    }
    dir = GetXZAngleLocal(&pos, &distPos, ang.y);
    dir = LIMIT_ANGLE(dir);
    pantsCheck();
    cautionCheck();
    frontCheck();
    if (npcCheck) {
        for (i = 0; i <= 15; i++) {
            eprintf(0x18 + i * 8, 0x180, 0, 0, "%d", ((cFlag*) &subFlags)->check(i));
        }
        Draw_pos(&distPos, 1000);
    }
}

// subFlags2 bit8: a damage area 1500 ahead on the way to subTarget.
void cSubChar::frontCheck()
{
    const f32 len = 1500.0f;   // pool order: 1500 before 0
    Vec d;

    BitOff16(subFlags2, 0x100);
    PSVECSubtract(&distPos, &pos, &d);
    if (d.x == 0.0f && d.z == 0.0f) {
        return;
    }
    d.y = 0.0f;
#line 3850 "D:/Bio4/Prog/pl_npc.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 1500.0f);
    PSVECAdd(&d, &pos, &d);
    if (DmgMgr.hitCheck(&d, 0)) {
        subFlags2 |= 0x100;
    }
}

// Scenario wall between her and subTarget (400 up): attribute -> sub438, hit / normal, flag bit3.
void cSubChar::anaSatInfo()
{
    Vec a = { 0.0f, 400.0f, 0.0f };
    Vec b;
    u32 r;
    const f32 dist = 360000.0f;   // pool order: 360000, 0.5236, pi
    const f32 lim = 0.5235988f;

    PSVECAdd(&a, &subSelf->pos, &a);
    satAttr = 0;
    if (fabsf(Muku(&pos, &distPos, ang.y, 3.1415927f)) > lim) {
        return;
    }
    b.x = distPos.x;
    b.y = a.y;
    b.z = distPos.z;
    r = SatMgr.hitCheck(&a, &b, &satCross, &satNorm, 0, 0);
    if (!(r & 0x01000000)) {
        return;
    }
    if (GetDistance(a, satCross) > 360000.0f) {
        return;
    }
    satAttr = r;
    subFlags2 |= 8;
}

// Event start: interrupts her routine, collision off (bits 0x300).
void cSubChar::beginEvent()
{
    interrupt();
    AtariOff(&atari, 0xFCFF);
}

// Event end: cloth reset (be_flag 0x200000), collision on.
void cSubChar::endEvent()
{
    be_flag |= 0x200000;
    AtariOn(&atari, 0x300);
}

// pl_sub SubCharCtrl modes: 0 stop, 1 wait here, 2 follow, 3 warp to the player and follow,
// 4 move to subMoveTo, 5 re-init, 6 wait (only from routine 0).
void cSubChar::control(int mode)
{
    f32 far = 1000.0f;   // dead initialisers: the original's pool has 1000 and 400 here
    f32 near = 400.0f;

    if (hp <= 0) {
        return;
    }
    BitOff16(subFlags, 0x18);
    subFlags |= 0x40;
    switch (mode) {
    case 0:
        subFlags |= 1;
        break;
    case 1:
        if (r_no_0 == 5) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        if (r_no_0 != 0 || r_no_1 != 0x10) {
            BitOff16(subFlags, 1);
            subFlags |= 2;
            AtariOn(&atari, 0x300);
        }
        break;
    case 3:
        if (r_no_0 == 5) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        if (r_no_0 != 0 || r_no_1 != 0x10) {
            setPos(&pPL->pos);
        }
        r_no_0 = 0;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 1;
        AtariOn(&atari, 0x300);
    case 2:
        if (r_no_0 == 5) {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        if (r_no_0 != 0 || r_no_1 != 0x10) {
            BitOff16(subFlags, 3);
            AtariOn(&atari, 0x300);
        }
        break;
    case 4: {
        int md = 1;   // shared by both arms (`li r6, 1` before the test)

        BitOff16(subFlags, 2);
        subFlags |= 8;
        if (subMoveTo[0] != 193.0f) {
            SubRoutineSet(this, 0, md, 0, 0);
            AtariOn(&atari, 0x300);
        } else if (subMoveTo[3] != 193.0f) {
            *(Vec*) subMoveTo = pos;
            SubRoutineSet(this, 0, md, 0, 0);
            AtariOn(&atari, 0x300);
        } else {
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
    case 5:
        init();
        SubRoutineSet(this, 0, 0, 0, 0);
        break;
    case 6:
        if (r_no_0 == 0 && SUBFLAG(this)->check(6)) {
            subFlags |= 2;
            BitOff16(subFlags, 0x40);
            SubRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Ledge in front of the partner: point 800 out from the wall below it and the facing angle.
int getFallPos(cSubChar* pl, Vec* opos, Vec* orot)
{
    static const Vec chk = { 1000.0f, 400.0f, 0.0f };
    static const f32 h = 300.0f;
    static const f32 len = 1000.0f;
    static const f32 back = 800.0f;
    Vec a;
    Vec b;
    Vec hit;
    Vec nrm;
    Vec d;

    a.x = 0.0f;
    a.y = h;
    a.z = 0.0f;
    PSVECAdd(&a, &pl->pos, &a);
    b.y = h;
    b.z = len;
    b.x = 0.0f;
    PSMTXMultVec(pl->mat, &b, &b);
    SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0);
    d.x = -nrm.x;
    d.y = 0.0f;
    d.z = -nrm.z;
#line 4080 "D:/Bio4/Prog/pl_npc.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, back);
    PSVECAdd(&hit, &d, &b);
    orot->x = 0.0f;
    orot->y = Muku3(&d, 0.0f, 3.1415927f);
    orot->z = 0.0f;
    opos->x = b.x;
    opos->y = SatMgr.getFloor(&b, 600.0f, 100000.0f, 0, 0);
    opos->z = b.z;
    return 1;
}

// Routine bits of the partner for the camera / scenario.
u32 SubCharGetStatus()
{
    cSubChar* sub = pSUB;
    u32 ret;

    if (sub == 0) {
        return 0;
    }
    if (sub->id != 3) {
        return 0;
    }
    ret = 0;
    switch (sub->r_no_0) {
    case 0:
        switch (sub->r_no_1) {
        case 0:
            ret = 1;
            break;
        case 1:
            ret = 2;
            break;
        case 0xA:
            switch (sub->subHideMode) {
            case 0:
            case 3:
                ret |= 0x100;
                break;
            case 1:
                ret = 0x200;
                break;
            case 4:
                ret = 0x10000;
                break;
            }
            break;
        case 0xF:
            ret = 0x01000000;
            break;
        case 0x10:
            if (sub->r_no_2 <= 0xC) {
                ret = 0x04000000;
            } else if (sub->r_no_2 <= 0xD) {
                ret = 0x08000000;
            } else {
                ret = 0x10000000;
            }
            break;
        }
        break;
    case 1:
    case 2:
    case 3:
        ret = 0x80;
        break;
    case 4:
        ret = 0x02000000;
        break;
    case 5:
        switch (sub->r_no_1) {
        case 0:
            ret = 1;
            break;
        case 1:
            ret = 8;
            break;
        }
        break;
    }
    if (SUBFLAG(sub)->check(1) || SUBFLAG(sub)->check(0) || (sub->r_no_0 == 0 && sub->r_no_1 == 0x10)) {
        ret |= 0x40000000;
    } else {
        ret |= 0x20000000;
    }
    if (SUBFLAG(sub)->check(3) && SUBFLAG2(sub)->check(6)) {
        ret |= 0x00800000;
    }
    return ret;
}

// Find an EMI "another route" (type 0xB) start near the partner (kind 0: she is above the player;
// kind 1: a kind 2 entry is near the player) and its first step; 1 when sub554 was set.
int cSubChar::checkAnotherRoute()
{
    EmiData* emi;
    EmiEntry* e;
    int found;
    int i;
    u8 id;
    int j;
    f32 dist;

    // Stored through a plain pointer (not a member reference) so the scheduler keeps the pG
    // load below it: a member store never conflicts with a fixed scalar load in GCC 2.95.
    e = 0;
    id = 0;
    *(u32*) &pAnotherRoute = 0;
    emi = (EmiData*) pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    if (emi->n == 0) {
        return 0;
    }
    found = -1;
    for (i = 0; i < *(int*) pG->pEmi; i++) {
        u32 o = i * 0x40 + 8;

        e = (EmiEntry*) ((u8*) pG->pEmi + o);
        if (((u8*) pG->pEmi)[o] != 0xB) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (e->pad_3 > 1) {
            continue;
        }
        {
            f32 dx = pos.x - e->pos.x;
            f32 dy = pos.y - e->pos.y;
            f32 dz = pos.z - e->pos.z;

            dist = dx * dx + dy * dy + dz * dz;
            if (dist > 4000000.0f) {
                continue;
            }
        }
        if (e->pad_3 == 1) {
            int ok = 0;

            for (j = 0; j < *(int*) pG->pEmi; j++) {
                u32 o = j * 0x40 + 8;
                EmiEntry* f = (EmiEntry*) ((u8*) pG->pEmi + o);

                if (((u8*) pG->pEmi)[o] != 0xB) {
                    continue;
                }
                if (f->state != 0) {
                    continue;
                }
                if (f->pad_3 != 2) {
                    continue;
                }
                {
                    f32 dx = pPL->pos.x - f->pos.x;
                    f32 dz = pPL->pos.z - f->pos.z;

                    dist = dx * dx + dz * dz;
                    if (dist > 16000000.0f) {
                        continue;
                    }
                }
                if (fabsf(pPL->pos.y - f->pos.y) > 500.0f) {
                    continue;
                }
                ok = 1;
                break;
            }
            if (!ok) {
                continue;
            }
        } else {
            if (pos.y < pPL->pos.y + 500.0f) {
                continue;
            }
        }
        found = i;
        id = e->sub;
        break;
    }
    if (found == -1) {
        return 0;
    }
    found = -1;
    for (i = 0; i < ((EmiData*) pG->pEmi)->n; i++) {
        e = &((EmiData*) pG->pEmi)->entry[i];
        if (e->type != 0xB) {
            continue;
        }
        if (e->state != 1) {
            continue;
        }
        if (e->sub != id) {
            continue;
        }
        found = i;
        break;
    }
    if (found == -1) {
        return 0;
    }
    pAnotherRoute = e;
    return 1;
}

// Step along the "another route": 1 when it ends (near the player past the last step), 0 while
// walking (sub554 advances to the next step once the current one is reached).
int cSubChar::moveAnotherRoute()
{
    EmiData* emi = (EmiData*) pG->pEmi;
    EmiEntry* f = 0;
    int bad;
    int next;
    int i;
    f32 dist;

    bad = emi == 0;
    if (emi->n == 0) {
        bad = 1;
    }
    if (pAnotherRoute == 0) {
        bad = 1;
    }
    if (bad) {
        pAnotherRoute = f;
        return 1;
    }
    if (pAnotherRoute->state > 1) {
        dist = (pos.x - pPL->pos.x) * (pos.x - pPL->pos.x) + (pos.y - pPL->pos.y) * (pos.y - pPL->pos.y) +
               (pos.z - pPL->pos.z) * (pos.z - pPL->pos.z);
        if (dist < 4000000.0f) {
            return 1;
        }
    }
    dist = (pos.x - pAnotherRoute->pos.x) * (pos.x - pAnotherRoute->pos.x) + (pos.y - pAnotherRoute->pos.y) * (pos.y - pAnotherRoute->pos.y) +
           (pos.z - pAnotherRoute->pos.z) * (pos.z - pAnotherRoute->pos.z);
    if (dist > 1000000.0f) {
        return 0;
    }
    next = -1;
    for (i = 0; i < *(int*) pG->pEmi; i++) {
        u32 o = i * 0x40 + 8;

        f = (EmiEntry*) ((u8*) pG->pEmi + o);
        if (((u8*) pG->pEmi)[o] != 0xB) {
            continue;
        }
        if (f->sub != pAnotherRoute->sub) {
            continue;
        }
        if (f->state == pAnotherRoute->state + 1) {
            next = i;
            break;
        }
    }
    if (next == -1) {
        return 1;
    }
    pAnotherRoute = f;
    return 0;
}

// Damage registered on her (dmg info): pick the reaction by weapon.
void cSubChar::damageCheck()
{
    if (dmg.m_Flag == 0) {
        return;
    }
    if (subAux1) {
        SubRoutineSet(this, 1, 0, 0, 0);
        dmg.m_Flag = 0;
        return;
    }
    interrupt();
    switch (dmg.m_Wep) {
    default: {
        dmg.m_Timer = 1;
        LifeDownSet2(this, 9999, 0, 0);
        int one = 1;
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            SubRoutineSet(this, one, 0, 0, 0);
            subHideMode = 11;
        } else if ((s16) pG->ashley_life > 0) {
            SubRoutineSet(this, one, 0, 0, 0);
            subHideMode = 2;
        } else {
            dmg.m_Timer = 0x80;
            SubRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
    case 0xD:
    case 0x12:
    case 0x13:
        dmg.m_Timer = 1;
        if (dmg.m_Dist > 9000000.0f) {
            r_no_1 = 7;
            r_no_0 = 0;
            r_no_2 = 0;
            r_no_3 = 0;
        } else {
            LifeDownSet2(this, 9999, 0, 0);
            SubRoutineSet(this, 1, 0, 0, 0);
            if (Front_check(this, &dmg.m_PosFrom, 1.5707964f)) {
                subHideMode = 7;
                ang.y += Muku(&pos, &dmg.m_PosFrom, 3.1415927f, 3.1415927f);
            } else {
                subHideMode = 9;
                ang.y += Muku(&pos, &dmg.m_PosFrom, 3.1415927f, 3.1415927f);
            }
        }
        break;
    case 0xE:
    case 0x17:
        goto skip;
    case 0x18:
        dmg.m_Timer = 0x3C;
        LifeDownSet2(this, 300, 0, 0);
        if ((s16) pG->ashley_life > 0) {
            SubRoutineSet(this, 1, 0, 0, 0);
            subHideMode = 2;
        } else {
            dmg.m_Timer = 0x80;
            SubRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
    StaFlagOff(pG, STA_SUB_LADDER);
skip:
    dmg.m_Flag = 0;
}

// Scenario damage area hit (sce_at sceAtFunc_damage): `power` is the hit direction (123 = none).
void cSubChar::setDamage(u8 kind, int arg, f32 power, int a, int b)
{
    dmg.set(0, 30);
    beginDamage();
    LifeDownSet2(this, arg, 0, 1);
    if (power != 123.0f) {
        f32 ang = Muku2(this->ang.y, power, 3.1415927f);

        if (ang < 1.5707964f && ang > -1.5707964f) {
            fWork0 = power;
            switch (kind) {
            case 0:
            case 1:
                kind = 1;
                break;
            case 2:
            case 3:
                kind = 3;
                break;
            case 4:
            case 5:
                kind = 5;
                break;
            }
        } else {
            fWork0 = LIMIT_ANGLE(power + 3.1415927f);
            switch (kind) {
            case 0:
            case 1:
                kind = 0;
                break;
            case 2:
            case 3:
                kind = 2;
                break;
            case 4:
            case 5:
                kind = 4;
                break;
            }
        }
    } else {
        fWork0 = 123.0f;
    }
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
    subHideMode = kind;
}

// The player registers a ledge for her to wait at (pos / facing angle), for 240 frames.
void cSubChar::registPlAction(Vec* pos, f32 ang)
{
    m_PlActPos = *pos;
    m_PlActAngY = ang;
    m_PlActTime = 0xF0;
    m_PlActType = 0;
}

// Water ripples / splashes while she wades (rooms 10A / 11A).
void waterProc(cSubChar* pl)
{
    static f32 spd0 = 1000.0f;
    static f32 spd1 = 6000.0f;
    static u8 hamonTimer;
    static u8 sibukiTimer;
    static Vec m_PosOldWater;
    f32 d;

    if (pG->room_id != 0x10A && pG->room_id != 0x11A) {
        return;
    }
    hamonTimer++;
    if (hamonTimer % 13 == 0) {
        EstSet((int) pl, -1, 0, 0, 1, 0x21, 0, 0, (u32) pl, 0);
    }
    d = GetDistance(&m_PosOldWater, &pl->pos);
    if (sibukiTimer) {
        sibukiTimer--;
    } else if (d > spd1) {
        EstSet((int) pl, -1, 0, 0, 1, 0x23, 0, 0, (u32) pl, 0);
        sibukiTimer = 10;
    } else if (d > spd0) {
        EstSet((int) pl, -1, 0, 0, 1, 0x22, 0, 0, (u32) pl, 0);
        sibukiTimer = 16;
    }
    m_PosOldWater = pl->pos;
}

// Bust motion (parts 0x1D, 0x1E, 0x1A) driven by the body speed; softer with flags_5010 bit21.
void cSubChar::moveBust()
{
    static u8 bbx = 0;
    static f32 bul = 0.0f;
    f32 max;
    f32 div;
    u8 step;
    Vec ofs;
    cModel* parts;
    cModel* body;

    if (StaFlagChk(pG, STA_PL_BOAT)) {
        max = 2.0f;
        div = 3.6666667f;
        step = 5;
    } else {
        max = 6.0f;
        div = 11.0f;
        step = 15;
    }
    body = getPartsPtr(0);
    if (GetDistance3(&body->world, &body->world_old2) > 5.0f) {
        bul = max;
    }
    if (Joy[1].on & JOY_X) {
        bul = max;
    } else {
        if (bul > max / div) {
            bul -= max / div;
        } else {
            bul = 0.0f;
        }
    }
    ofs.x = 0.0f;
    ofs.y = sinf((f32) bbx * (PI * 2.0f) * (1.0f / 256.0f)) * bul;
    ofs.z = 0.0f;
    parts = getPartsPtr(0x1D);
    PSVECAdd(&subBustBase[0], &ofs, &parts->pos);
    parts = getPartsPtr(0x1E);
    PSVECAdd(&subBustBase[1], &ofs, &parts->pos);
    parts = getPartsPtr(0x1A);
    PSVECAdd(&subBustBase[2], &ofs, &parts->pos);
    bbx += step;
    parts->matUpdate();
    PSMTXConcat(parts->pParent->mat, parts->mat, parts->mat);
    parts->world.x = parts->mat[0][3];
    parts->world.y = parts->mat[1][3];
    parts->world.z = parts->mat[2][3];
}

// Eyelid (parts 0x1C) blink sequence on `timer` and the eye direction (parts 0x20/0x21) wander:
// eyeDir = { current, target, mix } (pl_class moveEyeNormal). The switch is written sorted with
// `default` first and every case spelled out (no shared labels): cross-jumping merges the identical
// bodies into the LAST copy, which is why the original's block order is 0,3,4,1E,58,5A,5D,5E,5F,60,
// 61,62 while its pool is in ascending case order.
void cSubChar::moveFace()
{
    static int timer;
    cModel* p;

    p = getPartsPtr(0x1C);
    switch (timer++) {
    default:
        p->ang.x = 0.0f;
        break;
    case 0: {
        // computed into a local first: the constant loads precede the eyeDir.z test
        f32 y = ((f32) (int) (u8) (Rnd() % 200) * 0.01f - 1.0f) * 3.1415927f * 0.1f;

        eyeDir.y = y;
        if (eyeDir.z == 0.0f) {
            eyeDir.x = y;
        }
        p->ang.x = 0.0872664600610733f;
        break;
    }
    case 1:
        p->ang.x = 0.1745329201221466f;
        break;
    case 2:
        p->ang.x = 0.3490658402442932f;
        break;
    case 3:
        p->ang.x = 0.3141592741012573f;
        break;
    case 4:
        p->ang.x = 0.24434609711170197f;
        break;
    case 5:
        p->ang.x = 0.1745329201221466f;
        break;
    case 6:
        p->ang.x = 0.0872664600610733f;
        break;
    case 0x1E:
        eyeDir.y = 0.0f;
        if (eyeDir.z == 0.0f) {
            eyeDir.x = 0.0f;
        }
        break;
    case 0x58:
        timer = (Rnd() & 3) ? 0 : 0x5A;
        break;
    case 0x5A:
        p->ang.x = 0.0872664600610733f;
        break;
    case 0x5B:
        p->ang.x = 0.1745329201221466f;
        break;
    case 0x5C:
        p->ang.x = 0.3490658402442932f;
        break;
    case 0x5D:
        p->ang.x = 0.296705961227417f;
        break;
    case 0x5E:
        p->ang.x = 0.33161255717277527f;
        break;
    case 0x5F:
        p->ang.x = 0.3490658402442932f;
        break;
    case 0x60:
        p->ang.x = 0.2617993950843811f;
        break;
    case 0x61:
        p->ang.x = 0.1745329201221466f;
        break;
    case 0x62:
        p->ang.x = 0.0872664600610733f;
        timer = 10;
        break;
    }
    p->matUpdate();
    {
        static int eyetime = 0;

        if (--eyetime < 0) {
            f32 d = ((f32) (int) (u8) (Rnd() % 200) * 0.01f - 1.0f) * 0.03141592815518379f;

            eyeDir.y += d;
            if (eyeDir.z == 0.0f) {
                eyeDir.x = eyeDir.y;
            }
            eyetime = (u8) (Rnd() % 3) + 2;
        }
    }
    eyeDir.limit();
    p = getPartsPtr(0x20);
    p->ang.y = eyeDir.x;
    p = getPartsPtr(0x21);
    p->ang.y = eyeDir.x;
    eyeDir.mix();
}

// Fade the shadow (shdCol) out while she is on a ledge / above the camera / on a slope.
void cSubChar::shadowCtrl()
{
    int fade = 0;

    if (SUBFLAG(this)->check(5)) {
        fade = 1;
    }
    if (pG->Cam.param.pos.y < subSelf->pos.y) {
        fade = 1;
    }
    if (subSelf->pFloor_norm && subSelf->pFloor_norm->y < 0.8f) {
        fade = 1;
    }
    if (fade) {
        if (subSelf->Shd_color <= 0xEF) {
            subSelf->Shd_color += 0x10;
        } else {
            subSelf->Shd_color = 0xFF;
        }
    } else {
        if (subSelf->Shd_color > 0xF) {
            subSelf->Shd_color -= 0x10;
        } else {
            subSelf->Shd_color = 0;
        }
    }
}

// Damage areas (DmgMgr) at her feet -> damage routine.
void cSubChar::dmgCheck()
{
    int hit = 1;

    if (!dmg.m_Flag && !dmg.m_Timer) {
        hit = 0;
    }
    if (hit) {
        return;
    }
    if ((s16) pG->pl_life <= 0) {
        return;
    }
    switch (DmgMgr.hitCheck(&getPartsPtr(0)->world, 0)) {
    case 2:
    case 8: {
        LifeDownSet2(this, (s16) pG->ashley_life_max, 0, 0);
        int one = 1;
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            SubRoutineSet(this, one, 0, 0, 0);
            subHideMode = 11;
        } else if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            SubRoutineSet(this, one, 0, 0, 0);
            subHideMode = 2;
        } else {
            dmg.m_Timer = 0x80;
            SubRoutineSet(this, 2, 0, 0, 0);
        }
        StaFlagOff(pG, STA_SUB_LADDER);
        int two = 1;
        LifeDownSet2(this, (s16) pG->ashley_life_max, 0, 0);
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            SubRoutineSet(this, two, 0, 0, 0);
            subHideMode = 11;
        } else if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            SubRoutineSet(this, two, 0, 0, 0);
            subHideMode = 2;
        } else {
            dmg.m_Timer = 0x80;
            SubRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
    case 1:
    case 4: {
        LifeDownSet2(this, (s16) pG->ashley_life_max, 0, 0);
        int one = 1;
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            SubRoutineSet(this, one, 0, 0, 0);
            subHideMode = 11;
        } else if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            SubRoutineSet(this, one, 0, 0, 0);
            subHideMode = 2;
        } else {
            dmg.m_Timer = 0x80;
            SubRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
    case 5:
        LifeDownSet2(this, 300, 0, 0);
        if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            SubRoutineSet(this, 1, 0, 0, 0);
            subHideMode = 2;
        } else {
            dmg.m_Timer = 0x80;
            SubRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
}

// Damage start: interrupt the current routine.
void cSubChar::beginDamage()
{
    interrupt();
}

// Damage end: neutral face.
void cSubChar::endDamage()
{
    setFace(0);
}

// Reset face / hands / collision / sound when a routine is cut short.
void cSubChar::interrupt()
{
    cAtariInfo* at = &atari;

    setFace(0);
    setHand(0);
    at->setPriority(0);
    // Scalar (non-struct) store through the pointer: keeps the pG load below it (cAtariInfo::flags).
    *(u16*) ((u8*) at + 0x1a) |= 0x300;
    StaFlagOff(pG, STA_CRITICAL);
    BitOff16(subFlags, 0x20);
    if (subSndId) {
        SndStop(subSndId, 0);
    }
}

// 1 while the partner (id 3) is in a routine 0 state the scenario may hide her from.
int SubCharHideCheck()
{
    cSubChar* sub = pSUB;

    if (sub == 0 || sub->id != 3) {
        return 0;
    }
    if (sub->r_no_0 != 0) {
        return 0;
    }
    switch (sub->r_no_1) {
    default:
        return 0;
    case 0:
    case 1:
    case 2:
    case 5:
    case 6:
        return 1;
    }
}

// Pull her back next to the player (195 away) when a wall separates them.
void cSubChar::inSat()
{
    Vec hit;
    Vec nrm;

    if (SatMgr.hitCheck(&pPL->pParts->world, &pSUB->pParts->world, &hit, &nrm, 0, 0)) {
        PSVECScale(&nrm, &nrm, 400.0f);
        PSVECAdd(&nrm, &hit, &nrm);
        nrm.y = pPL->pos.y;
        pSUB->setPos(&nrm);
    }
    Vec p = pos;
    Vec d;
    PSVECSubtract(&pos, &pPL->pos, &d);
#line 4943 "D:/Bio4/Prog/pl_npc.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 195.0f);
    PSVECAdd(&d, &pPL->pos, &d);
    setPos(&d);
    partsWorldCalc();
    EmAtCheck(this);
}

// Partner condition bits for the HUD: 1 following / 2 waiting, 8 (flags_5010 bit16), 0x20 moving
// to a point, 0x10 waiting at a ladder / window.
u32 SubCharGetCondition()
{
    cSubChar* sub = pSUB;
    u32 ret;

    if (sub == 0) {
        return 0;
    }
    if (SUBFLAG(sub)->check(1) || (sub->r_no_0 == 0 && sub->r_no_1 == 0x10)) {
        ret = 2;
    } else {
        ret = 1;
    }
    if (StaFlagChk(pG, STA_TAKEAWAY)) {
        return ret | 8;
    }
    if (SUBFLAG(sub)->check(3)) {
        return ret | 0x20;
    }
    if (sub->r_no_0 != 0) {
        return ret;
    }
    if ((u8) (sub->r_no_1 - 0x13) > 1) {
        return ret;
    }
    return ret | 0x10;
}

// Debug (dbsubflag): draws the distance / action points and prints the routine numbers.
void cSubChar::debugMove()
{
    static int dbsubflag = 0;

    if (dbsubflag == 0) {
        return;
    }
    Draw_pos(&distPos, 1000);
    Draw_pos(&m_PlActPos, 500);
    eprintf(0x18, 0x8C, 0, 0, "PAT:%d", m_PlActTime);
    eprintf(0x18, 0x118, 0, 0, "R:%02d.%02d.%02d.%02d", r_no_0, r_no_1, r_no_2, r_no_3);
}

int lbl_803140D4 = 0;   // unreferenced .sdata word after dbsubflag

// Empty virtuals defined `inline` (out of class): emitted at the end of the unit after the static
// initialisation function, before the global-constructor thunk.
inline void cSubChar::setFace(int no)
{
}

// No hand models on the base partner (the Ashley class overrides).
inline void cSubChar::setHand(int no)
{
}

// No cloth on the base partner.
inline void cSubChar::initCloth()
{
}

// No cloth on the base partner.
inline void cSubChar::moveCloth()
{
}

// Never called: the linker dead-stripped the body and kept its constant pool (the 1000 word after
// the static initialiser's 0.0 at the end of `.rodata`).
inline int cSubChar::farCheck()
{
    return dist > 1000.0f;
}
