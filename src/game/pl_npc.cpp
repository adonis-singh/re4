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
#include <string.h>
#include "shape.h"
#include "obj13.h"

extern "C" {
void pl_fall_ok0();
void pl_fall_ok(cPlayer* pl);
void catchOn();
int getFallPos(cSubChar* pl, Vec* pos, Vec* rot);
void waterProc(cSubChar* pl);
}

// Motion data `no` of the partner's motion archive.
#define SUB_MOT(pl, no) PL_ARC_PTR((pl)->subArc, no)
#define SUB_MOTBASE(pl) (&(pl)->m_MotBase)
// MotionSetCore with the sequence table as the 4th argument (declared int in motion.h).
#define MOT_SET(m, w, data, seq, a, b, c) MotionSetCore(m, w, data, seq, a, b, c)

const Vec cSubChar::atckPos = { -100.0f, 0.0f, -500.0f };
const Vec cSubChar::atckPos2 = { 300.0f, 0.0f, -500.0f };

static cDelayF eyeDir;

// 1 while the partner has at least half her life: picks the healthy motion set (0x12..) over
// the hurt one (0x6E..).
int cSubChar::mot_ck()
{
    return hp >= (s16) pG->ashley_life_max / 2;
}

// Partner enemy construction: flags clear, its motion-base helper, no light / damage function,
// neck straight, eye state.
cSubChar::cSubChar() : flg(), status()
{
    pEm = this;
    m_pFunc = 0;
    m_pLiF = 0;
    neckInit();
    eyeDir.reset(0.0f);
    eyeDir.setDelay(0.4f);
}

// Frees her back light and clears the global pSUB.
cSubChar::~cSubChar()
{
    if (m_pLiF && m_pLiF->isAlive()) {
        LightMgr.destroy(m_pLiF);
    }
    // Stored through the struct view: keeps the base destructor's be_flag load below the store.
    pSUB = 0;
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
    atari.init(0.0f, -200.0f, 0.0f, 300.0f, 200.0f, 400.0f, 900.0f, 1, 0x1000, 10);
    if (m_pLiF == 0) {
        m_pLiF = LightMgr.createBack(0, 2, 0, 0);
        m_pLiF->setParent(this);
    }
    {
        cSubChar* s = pEm;

        s->lockParts = 4;
        s->lockOfs.x = 0.0f;
        s->lockOfs.y = 0.0f;
        s->lockOfs.z = 0.0f;
    }
    pEm->setStatus(EM_STATUS_LOCKOFF);
    // statement order brute-forced (store schedule + shared zero registers)
    m_FallWaitTimer = 0;
    m_PlActTime = 0;
    be_flag |= 0x02200000;
    flg.on(F_PL_CTRL);
    flg.off(F_SLEEP).off(F_STAY).off(F_MOVE_TO);
    pAuxDm = 0;
    pAux = 0;
    atari.on();
    YarareInit(this, 0.0f, -30.0f, 0.0f, 140.0f, 100.0f, 2, YAT_FLAG_ON);
    YarareAdd(this, &m_Yarare[0], 0.0f, 0.0f, 0.0f, 150.0f, 130.0f, 3, YAT_FLAG_ON);
    YarareAdd(this, &m_Yarare[1], -20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x13, YAT_FLAG_ON);
    YarareAdd(this, &m_Yarare[2], 20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x17, YAT_FLAG_ON);
    if (mot_ck()) {
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x12), 0, 0, 5, 0);
    } else {
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6E), 0, 0, 5, 0);
    }
    motionMove();
    posBustR = getPartsPtr(0x1D)->pos;
    posBustL = getPartsPtr(0x1E)->pos;
    posScarf = getPartsPtr(0x1A)->pos;
#line 231 "D:/Bio4/Prog/pl_npc.cpp"
    Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 13);
}

// Per frame: hp mirrors pG->ashley_life, the player's routine bits are cached (plStat), damage
// areas are checked, then r_no_0 (0 core, 1 damage, 2 die, 3 bulldozer, 4 custom m_pFunc, 5
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

    if (flg.check(F_SLEEP)) {
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
        m_pFunc();
    } else {
        (this->*NpcFuncTbl[r_no_0])();
    }
    if (DbgFlagChk(pG, DBG_KAIOUKEN)) {
        int i;

        for (i = 0; i < PlKaiou + 1; i++) {
            if (r_no_0 == 4) {
                m_pFunc();
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
    if (m_pFace) {
        ShapeMove(m_pFace);
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
            EmRoutineSet(this, 6, 0, 0, 0);
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
                m = SUB_MOT(pEm, 0x12);
                seq = SUB_MOT(pEm, 0x4F);
            } else {
                m = SUB_MOT(pEm, 0x6E);
                seq = 0;
            }
            MOT_SET(pEm, MOTION(pEm), m, seq, 0, 4, 0);
            pEm->motionMove();
        }
        if (mot_ck()) {
            flg.off(F_RELAX);
        } else {
            flg.on(F_RELAX);
        }
        m_Work0 = 0;
        if (flg.check(F_MOVE_TO)) {
            if (dist > 400.0f) {
                EmRoutineSet(this, 0, 1, 0, 0);
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
            m = SUB_MOT(pEm, 0x12);
            seq = SUB_MOT(pEm, 0x4F);
            back = SUB_MOT(pEm, 0x18);
        } else {
            m = SUB_MOT(pEm, 0x6E);
            seq = 0;
            back = 0;
        }
        MOT_SET(pEm, MOTION(pEm), m, seq, 0x14, 4, 0);
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
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x35), SUB_MOT(pEm, 0x5D), 7, 5, 0);
            pEm->r_no_2 = 0xB;
        } else if (dir > 0.19634955f) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x36), SUB_MOT(pEm, 0x5E), 7, 5, 0);
            pEm->r_no_2 = 0xC;
        } else {
            pEm->r_no_2 = 1;
        }
        break;
    case 0xB:
        if (dir >= -0.19634955f) {
            pEm->r_no_2 = 1;
        }
        break;
    case 0xC:
        if (dir < 0.19634955f) {
            pEm->r_no_2 = 1;
        }
        break;
    case 0x14:
        if (mot_ck()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x1A), SUB_MOT(pEm, 0x53), 7, 5, 0);
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x72), 0, 7, 5, 0);
        }
        pEm->r_no_2 = 0x1F;
    case 0x15:
        if (pEm->Motion.Seq_frame >= (f32) (pEm->Motion.Seq_frame_num - 1)) {
            pEm->r_no_2 = 1;
        }
        break;
    case 0x1E:
        setFace(2);
        if (!flg.check(F_RELAX)) {
            flg.on(F_RELAX);
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x19), 0, 7, 5, 0);
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
        if (pEm->Motion.Seq_frame >= (f32) (pEm->Motion.Seq_frame_num - 1) || checkBackEm()) {
            setFace(4);
            pEm->r_no_2 = 1;
        }
        break;
    case 0x20:
        neckSet(&pPL->pos);
        r_no_3--;
        if (r_no_3 == 0 || checkBackEm()) {
            r_no_3 = 0;
            setFace(4);
            pEm->r_no_2 = 1;
        }
        break;
    case 0x28:
        if (pEm->Motion.Seq_frame >= (f32) (pEm->Motion.Seq_frame_num - 1)) {
            pEm->r_no_2 = 0;
        }
        break;
    case 0x32:
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x43), 0, 7, 1, 0);
        r_no_2 = 0x33;
    case 0x33:
        if (pEm->Motion.Seq_frame >= (f32) (pEm->Motion.Seq_frame_num - 1)) {
            MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x44), 0, 7, 5, 0);
            r_no_2 = 0x34;
        }
        break;
    case 0x34:
        break;
    }
    motionMove();
    if (flg.check(F_STAY)) {
        if (status.check(S_EM_NEAR2)) {
            if (!status.check(S_PL_NEAR)) {
                m_Work0++;
                if (m_Work0 > 150) {
                    m_Work0 = 0;
                    m_StopSe = SndCall(8, 2, &pEm->pList->world, id, 0, 0);
                }
            }
        }
    }
    backCheckCtrlFootwork();
    if (status.check(S_PANTS)) {
        EmRoutineSet(this, 0, 6, 0, 0);
        return;
    }
    if (status.check(S_WEPCAUTION)) {
        EmRoutineSet(this, 0, 4, 0, 0);
        return;
    }
    if (plDownCheck()) {
        EmRoutineSet(this, 0, 0x11, 0, 0);
        return;
    }
    if (!status.check(S_PL_NEAR) && status.check(S_EM_NEAR)) {
        EmRoutineSet(this, 0, 7, 0, 0);
        return;
    }
    if (flg.check(F_STAY)) {
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
    if (status.check(S_NEED_STOP)) {
        return;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED) && status.check(S_PL_NEAR)) {
        EmRoutineSet(this, 0, 7, 0, 0);
        return;
    }
    if (StaFlagChk(pG, STA_PL_EM_ACTION) && status.check(S_PL_NEAR)) {
        if ((u8) (r_no_2 - 0x32) > 9) {
            r_no_2 = 0x32;
        }
        return;
    }
    if ((plStat & 0x20000) && dist < 1000.0f) {
        return;
    }
    if (readyCheck()) {
        EmRoutineSet(this, 0, 1, 0, 0);
        return;
    }
    act = actCheck();
    switch (act) {
    default:
        if (plStat & 0x41F) {
            if (pEm->flg.check(cSubChar::F_MOVE_TO)) {
                if (!pEm->status.check(cSubChar::S_ARRIVED)) {
                    pEm->m_Frame = 0;
                    pEm->m_Hokan = 5;
                    EmRoutineSet(pEm, 0, 1, 0, 0);
                    break;
                }
            }
            if (dist > 1100.0f || m_PlActTime) {
                pEm->m_Frame = 0;
                pEm->m_Hokan = 5;
                EmRoutineSet(pEm, 0, 1, 0, 0);
            }
        }
        break;
    case 7:
        EmRoutineSet(this, 0, 2, 0, 0);
        break;
    case 8:
        EmRoutineSet(this, 0, 0xA, 0, 0);
        break;
    case 9:
        EmRoutineSet(this, 0, 0xC, 0, 0);
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

    switch (pEm->r_no_2) {
    case 0:
        m_Timer = 0;
        if (dir > 2.617994f || dir < -2.617994f) {
            if (!flg.check(F_DONT_RUN) && (ckPlRun() || dist > 3000.0f)) {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x26), SUB_MOT(pEm, 0x58), 7, 5, 0);
                pEm->r_no_3 = 3;
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x26), SUB_MOT(pEm, 0x58), 7, 5, 0);
                pEm->r_no_3 = 2;
            }
        } else {
            if (!flg.check(F_DONT_RUN) && (ckPlRun() || dist > 3000.0f)) {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x25), SUB_MOT(pEm, 0x57), 7, 4, 0);
                pEm->r_no_3 = 1;
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x24), SUB_MOT(pEm, 0x56), 7, 4, 0);
                pEm->r_no_3 = 0;
            }
        }
        checkAnotherRoute();
        subMot.Brate = 0.0f;
        m_BackRno = 0;
        m_BackRno2 = 0;
        m_BackTime = 30;
        pEm->r_no_2 = 1;
    case 1:
        if (pEm->r_no_3 <= 1) {
            ang.y += Muku(&pos, &distPos, ang.y, 0.20943952f);
        }
        if (!pEm->motionMove()) {
            break;
        }
        pEm->r_no_2 = 2;
    case 2:
        switch (r_no_3) {
        case 2:
            r_no_3 = 0;
        case 0:
            if (mot_ck()) {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x13), SUB_MOT(pEm, 0x50), 7, 4, 0);
                backCheckSet(SUB_MOT(pEm, 0x15));
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x6F), SUB_MOT(pEm, 0x76), 7, 4, 0);
                backCheckSet(0);
            }
            break;
        case 3:
            pEm->r_no_3 = 1;
        case 1:
            if (mot_ck()) {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x16), SUB_MOT(pEm, 0x52), 7, 4, 0);
                backCheckSet(SUB_MOT(pEm, 0x17));
            } else {
                MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x71), SUB_MOT(pEm, 0x78), 7, 4, 0);
                backCheckSet(0);
            }
            break;
        }
        pEm->r_no_2 = 3;
    case 3:
        motionMove();
        switch (pEm->r_no_3) {
        case 0:
            if (!flg.check(F_DONT_RUN) && (ckPlRun() || dist > 3000.0f)) {
                m_Frame = (u8) (pPL->Motion.Seq_frame * 100.0f / (f32) (int) pPL->Motion.Seq_frame_num);
                m_Hokan = 3;
                r_no_2 = 2;
                r_no_3 = 1;
            }
            break;
        case 1:
            if (flg.check(F_DONT_RUN) || (!ckPlRun() && dist < 550.0f)) {
                m_Frame = (u8) (pPL->Motion.Seq_frame * 100.0f / (f32) (int) pPL->Motion.Seq_frame_num);
                m_Hokan = 3;
                r_no_2 = 2;
                r_no_3 = 0;
            }
            break;
        }
        if (dist > 400.0f || flg.check(F_MOVE_TO) || m_PlActTime) {
            ang.y += Muku(&pos, &distPos, ang.y, 0.20943952f);
        } else if (plStat & 4) {
            f32 a = LIMIT_ANGLE(pPL->ang.y + 3.1415927f);

            ang.y += Muku2(ang.y, a, 0.10471976f);
        } else {
            ang.y += Muku2(ang.y, pPL->ang.y, 0.10471976f);
        }
        if (flg.check(F_MOVE_TO) || m_PlActTime) {
            if (dist <= 400.0f) {
                r_no_2 = 4;
                Motion.Mot_attr &= ~1;
                m_Work0 = 0;
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
        m_Work0++;
        if (dist < 50.0f || m_Work0 > 6) {
            setPos(&distPos);
            if (m_TargetDir == 193.0f) {
                status.on(S_ARRIVED);
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
        if (Muku2(ang.y, m_TargetDir, 3.1415927f) < 0.0f) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x35), SUB_MOT(pEm, 0x5D), 4, 5, 0);
            r_no_2 = 0xB;
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x36), SUB_MOT(pEm, 0x5E), 4, 5, 0);
            r_no_2 = 0xC;
        }
        motionMove();
        break;
    case 0xB:
        motionMove();
        if (Muku2(ang.y, m_TargetDir, 3.1415927f) > 0.0f) {
            ang.y = m_TargetDir;
            status.on(S_ARRIVED);
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 0xC:
        motionMove();
        if (Muku2(ang.y, m_TargetDir, 3.1415927f) < 0.0f) {
            ang.y = m_TargetDir;
            status.on(S_ARRIVED);
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
    backCheckCtrlMove();
    if (flg.check(F_MOVE_TO) || flg.check(F_DONT_RUN)) {
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

        if (pEm->r_no_3 == 0) {
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
    if (status.check(S_PANTS)) {
        EmRoutineSet(this, 0, 6, 0, 0);
    } else if (status.check(S_WEPCAUTION)) {
        EmRoutineSet(this, 0, 4, 0, 0);
    } else if (StaFlagChk(pG, STA_PL_EM_ACTION) && status.check(S_PL_NEAR)) {
        EmRoutineSet(this, 0, 0, 0x32, 0);
    } else if (!status.check(S_PL_NEAR) && status.check(S_EM_NEAR)) {
        EmRoutineSet(this, 0, 7, 0, 0);
    } else if (StaFlagChk(pG, STA_PL_CATCHED) && status.check(S_PL_NEAR)) {
        EmRoutineSet(this, 0, 7, 0, 0);
    } else if (flg.check(F_STAY)) {
        EmRoutineSet(this, 0, 0, 0, 0);
    } else if (status.check(S_NEED_STOP)) {
        return;
    } else if (m_FallWaitTimer) {
        EmRoutineSet(this, 0, 0, 0, 0);
    } else if ((plStat & 0x400) && !flg.check(F_MOVE_TO)) {
        EmRoutineSet(this, 0, 0, 0, 0);
    } else if (plStat & 0x40000) {
        EmRoutineSet(this, 0, 0, 0, 0);
    } else if ((plStat & 0x20000) && dist < 1000.0f && !flg.check(F_MOVE_TO)) {
        EmRoutineSet(this, 0, 0, 0, 0);
    } else {
        switch ((u32) actCheck()) {
        case 1:
            EmRoutineSet(this, 0, 8, 0, 0);
            break;
        case 2:
            EmRoutineSet(this, 0, 8, 0, 1);
            break;
        case 3:
            status.on(S_FALL_WINDOW);
            EmRoutineSet(this, 0, 0x12, 0, 0);
            break;
        case 4:
            EmRoutineSet(this, 0, 0x14, 0, 0);
            break;
        case 5:
        case 6:
            break;
        case 7:
            EmRoutineSet(this, 0, 2, 0, 0);
            break;
        case 8:
            EmRoutineSet(this, 0, 0xA, 0, 0);
            break;
        case 9:
            EmRoutineSet(this, 0, 0xC, 0, 0);
            break;
        case 0xB:
            EmRoutineSet(this, 0, 0x13, 0, 0);
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
    return SatMgr.hitCheck(&pPL->pList->world, &pList->world, 0, 0, 0, 0) == 0;
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
        target = *ap;
        // reference stores: the following pPL loads stay below them
        (target.z = target.z - pPL->Wep->getAngle() * 0.31830987f * 300.0f);
        PSMTXMultVec(pPL->mat, &target, &target);
        pos.x = pos.x * 0.7f + target.x * 0.3f;
        pos.z = pos.z * 0.7f + target.z * 0.3f;
        ang.y += Muku2(ang.y, pPL->ang.y, 0.31415927f);
    }
    if (!(plStat & 0x10)) {
        m_Work0 = 1;
    }
    switch (r_no_2) {
    case 0:
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x1D), 0, 3, 5, 0);
        atari.off();
        m_Work0 = 0;
        r_no_2 = 1;
    case 1:
        if (!(plStat & 0x10)) {
            r_no_2 = 0x14;
        }
        if (motionMove()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x1E), 0, 3, 5, 0);
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
        if (m_Work0) {
            r_no_2 = 0x14;
        }
        break;
    case 0xA:
        setHand(1);
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x20), 0, 7, 5, 0);
        r_no_2 = 0xB;
    case 0xB:
        if ((plStat & 0x2080) || motionMove()) {
            setHand(0);
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x1E), 0, 3, 5, 0);
            motionMove();
            r_no_2 = 2;
        }
        break;
    case 0x14:
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x1F), 0, 3, 5, 0);
        StaFlagOff(pG, STA_CRITICAL);
        atari.on();
        inSat();
        atari.set(-10, 300.0f, 200.0f);
        EmRoutineSet(this, 0, 0, 0x28, 0);
        break;
    }
}

// Routine 0 / 4: crouch (kagamu) while the player fires over her.
void cSubChar::moveKagamu()
{
    switch (pEm->r_no_2) {
    case 0:
        if (mot_ck()) {
            pEm->motionSet(SUB_MOT(pEm, 0x12), 10, 0, 1, 0);
        } else {
            pEm->motionSet(SUB_MOT(pEm, 0x6E), 10, 0, 1, 0);
        }
        pEm->r_no_3 = 0;
        pEm->r_no_2 = 1;
    case 1:
        pEm->motionMove();
        if (++pEm->r_no_3 > 5) {
            pEm->r_no_2 = 2;
        }
        break;
    case 2:
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x43), 0, 1, 5, 0);
        pEm->r_no_2 = 3;
        fyBak = pEm->pos.y;
        pEm->atari.offOba();
    case 3:
        if (pEm->Motion.Seq_frame > 7.7f && pEm->Motion.Seq_frame < 8.3f) {
            status.on(S_DOWN);
        }
        if (pEm->motionMove()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x44), 0, 3, 5, 0);
            pEm->r_no_2 = 4;
        }
        break;
    case 4:
        pEm->motionMove();
        if (Key.on & 0x810) {
            pEm->m_Work0 = 0;
        } else {
            if (++pEm->m_Work0 > 30) {
                MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x45), 0, 3, 5, 0);
                pEm->atari.onOba();
                pEm->r_no_2 = 5;
            }
        }
        break;
    case 5:
        if (pEm->Motion.Seq_frame > 11.7f && pEm->Motion.Seq_frame < 12.3f) {
            status.off(S_DOWN);
        }
        if (pEm->motionMove()) {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 6: the "pants" reaction (the player looks up her skirt).
void cSubChar::movePants()
{
    switch (pEm->r_no_2) {
    case 0:
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x21), 0, 7, 5, 0);
        m_StopSe = SndCall(8, 0x16, &pEm->pList->world, id, 0, 0);
        pEm->r_no_2 = 1;
    case 1:
        if (MotionMove(pEm, 0)) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x22), 0, 7, 5, 0);
            pEm->r_no_2 = 2;
        }
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        break;
    case 2:
        MotionMove(pEm, 0);
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        if (!status.check(S_PANTS)) {
            pEm->r_no_2 = 3;
            pEm->r_no_3 = 0;
        }
        break;
    case 3:
        MotionMove(pEm, 0);
        ang.y += Muku(&pos, &pPL->pos, ang.y, 0.31415927f);
        if (status.check(S_PANTS)) {
            pEm->r_no_2 = 2;
        } else {
            if (++pEm->r_no_3 > 30) {
                MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x23), 0, 7, 5, 0);
                pEm->r_no_2 = 4;
            }
        }
        break;
    case 4:
        if (MotionMove(pEm, 0)) {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 7: crouch down (the player's "wait" command).
void cSubChar::moveDown()
{
    switch (r_no_2) {
    case 0:
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x43), 0, 7, 5, 0);
        status.on(S_DOWN);
        pEm->r_no_2 = 1;
    case 1:
        if (motionMove()) {
            pEm->r_no_2 = 2;
        }
        break;
    case 2:
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x44), 0, 7, 1, 0);
        r_no_2 = 3;
    case 3:
        if (status.check(S_EM_NEAR) || (StaFlagChk(pG, STA_PL_CATCHED))) {
            r_no_3 = 40;
        } else if (r_no_3) {
            r_no_3--;
        }
        if (r_no_3 == 0) {
            MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x45), 0, 7, 5, 0);
            r_no_2 = 4;
        }
        motionMove();
        break;
    case 4:
        if (motionMove()) {
            status.off(S_DOWN);
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Point 300 behind the scenario wall (of attribute `attr`) 1000 ahead of the partner: position and
// facing angle for the fence / window actions. 1 when found.
int cSubChar::getScrActionPoint(Vec* initPos, Vec* initAng, u32 actAttr)
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
    if (!(r & 0x01000000) || !(r & actAttr)) {
        return 0;
    }
    PSVECScale(&nrm, &b, -1000.0f);
    PSVECAdd(&b, &pos, &b);
    b.y += h;
    r = SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0);
    if (!(r & 0x01000000) || !(r & actAttr)) {
        return 0;
    }
    PSVECScale(&nrm, &b, 300.0f);
    PSVECAdd(&b, &hit, &b);
    b.y = pos.y;
    *initPos = b;
    initAng->z = 0.0f;
    initAng->x = 0.0f;
    initAng->y = atan2(-nrm.x, -nrm.z);
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
    switch (pEm->r_no_2) {
    case 0: {
        Vec p;
        Vec r;

        if (pEm->r_no_3 == 0) {
            if (!getScrActionPoint(&p, &r, 0x20)) {
                EmRoutineSet(this, 0, 0, 0, 0);
                break;
            }
        } else {
            p = pos;
            r.y = fWork0;
            r.x = 0.0f;
            r.z = 0.0f;
        }
        SUB_MOTBASE(this)->set((cMotModel*) this, &p, &r, 10);
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x2C), SUB_MOT(pEm, 0x67), 7, 5, 0);
        pEm->atari.offSca();
        atari.setPriority(PRI_LV2);
        pEm->dmg.set(0, 0x80);
        pEm->cCoord::matUpdate();
        pEm->r_no_2 = 1;
    }
    case 1:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (pEm->motionMove()) {
            pEm->atari.onSca();
            atari.setPriority(0);
            pEm->dmg.clear();
            EmRoutineSet(this, 0, 0, 0, 0);
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
        pPL->ang.y = pPL->ang.y + 4.712389f;
        pPL->ang.y = LIMIT_ANGLE(pPL->ang.y);
        pPL->cCoord::matUpdate();
        SetPlDamage(this, (void (*)(cPlayer*)) pl_fall_ok0);
        pPL->dmg.set(0, 0x80);
        if (status.check(S_FALL_WINDOW)) {
            m = SUB_MOT(pEm, 0x46);
        } else {
            m = SUB_MOT(pEm, 0x47);
        }
        motionSet(m, 3, 0, 0x201, 0);
        status.off(S_FALL_WINDOW);
        atari.off();
        dmg.set(0, 0x80);
        m_PlActTime = 0;
        flg.on(F_SHADOW_OFF);
        r_no_2 = 1;
    case 1:
        motionMove();
        if (pEm->Motion.Seq_frame >= 41.0f) {
            r_no_2 = 4;
        }
        break;
    case 4:
        pPL->atari.off();
        atari.onOba();
        atari.offSca();
        atari.setPriority(PRI_LV3);
        ang.y = pPL->ang.y - 4.712389f;
        ang.y = LIMIT_ANGLE(ang.y);
        PSMTXMultVec(pPL->mat, &v_ok, &pos);
        setPos(&pos);
        SetPlDamage(this, pl_fall_ok);
        pPL->dmg.set(0, 0x80);
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x3F), SUB_MOT(pEm, 0x5F), 7, 5, 0);
        atari.offSca();
        r_no_2 = 5;
    case 5:
        if (motionMove()) {
            atari.on();
            atari.setPriority(0);
            dmg.clear();
            flg.off(F_SHADOW_OFF);
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 0xA: one-shot actions selected by m_Work0 (jump down, climb, registered motion).
void cSubChar::moveAction()
{
    void* m = 0;
    void* seq;

    switch (r_no_2) {
    case 0:
        switch (m_Work0) {
        default:
            pLog->err(0, 0, "ASHLEY UNKNOWN ACTION %d", m_Work0);
        case 0:
            m = SUB_MOT(pEm, 0x1B);
            seq = SUB_MOT(pEm, 0x54);
            break;
        case 1:
            m = SUB_MOT(pEm, 0x1C);
            seq = SUB_MOT(pEm, 0x55);
            break;
        case 2:
            m = pEm->m_MotTbl2[0];
            seq = pEm->m_MotTbl2[1];
            break;
        case 3:
            m = SUB_MOT(pEm, 0x47);
            seq = SUB_MOT(pEm, 0x7C);
            break;
        case 4:
            m = SUB_MOT(pEm, 0x48);
            seq = SUB_MOT(pEm, 0x5C);
            break;
        }
        switch (m_Work0) {
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
        if (m_Work0 == 4) {
            m_StopSe = SndCall(8, 0x12, &pEm->pList->world, id, 0, 0);
        }
        MOT_SET(pEm, MOTION(pEm), m, seq, 3, 0x101, 0);
        atari.off();
        dmg.set(0, 0x80);
    case 1:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (motionMove()) {
            atari.on();
            dmg.clear();
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 2:
        StaFlagOn(pG, STA_SUB_LADDER);
        motionMove();
        if (pEm->Motion.Seq_frame >= 40.0f && landCheck()) {
            atari.on();
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x2D), SUB_MOT(pEm, 0x5A), 0, 0x101, 0);
            motionMove();
            r_no_2 = 3;
        }
        break;
    case 3:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (motionMove()) {
            atari.on();
            dmg.clear();
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    case 4:
        StaFlagOn(pG, STA_SUB_LADDER);
        if (pEm->Motion.Seq_frame <= 9.0f) {
            jumpAdjust();
        }
        if (pEm->Motion.Seq_frame >= 35.0f && pEm->Motion.Seq_frame <= 44.0f) {
            pos.y += fWork0 * 0.1f;
        }
        if (motionMove()) {
            atari.on();
            dmg.clear();
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 / 0xC: climb a ladder (m_Work0: 0 up, 1 down; m_Work1 rungs).
void cSubChar::moveLadder()
{
    void* m;
    void* seq;

    StaFlagOn(pG, STA_SUB_LADDER);
    switch (r_no_2) {
    case 0:
        if (m_Work0) {
            m = SUB_MOT(pEm, 0x4C);
            seq = SUB_MOT(pEm, 0x64);
        } else {
            m = SUB_MOT(pEm, 0x49);
            seq = SUB_MOT(pEm, 0x61);
        }
        MOT_SET(pEm, MOTION(pEm), m, seq, 3, 5, 0);
        r_no_2 = 1;
        flg.on(F_SHADOW_OFF);
    case 1:
        if (motionMove()) {
            r_no_2 = 2;
        }
        break;
    case 2:
        if (m_Work0) {
            m = SUB_MOT(pEm, 0x4D);
            seq = SUB_MOT(pEm, 0x65);
        } else {
            m = SUB_MOT(pEm, 0x4A);
            seq = SUB_MOT(pEm, 0x62);
        }
        MOT_SET(pEm, MOTION(pEm), m, seq, 3, 5, 0);
        r_no_2 = 3;
    case 3:
        if (motionMove()) {
            m_Work1--;
            if (m_Work1 <= 0) {
                r_no_2 = 4;
            }
        }
        break;
    case 4:
        if (m_Work0) {
            m = SUB_MOT(pEm, 0x4E);
            seq = SUB_MOT(pEm, 0x66);
        } else {
            m = SUB_MOT(pEm, 0x4B);
            seq = SUB_MOT(pEm, 0x63);
        }
        MOT_SET(pEm, MOTION(pEm), m, seq, 3, 5, 0);
        r_no_2 = 5;
    case 5:
        if (motionMove()) {
            atari.on();
            dmg.clear();
            flg.off(F_SHADOW_OFF);
            EmRoutineSet(this, 0, 0, 0, 0);
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
    h = SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0);
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
            motionSet(SUB_MOT(pEm, 0x14), 8, 0, 1, 0);
        } else {
            motionSet(SUB_MOT(pEm, 0x70), 8, 0, 1, 0);
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
            motionSet(SUB_MOT(pEm, 0x12), 8, 0, 1, 0);
        } else {
            motionSet(SUB_MOT(pEm, 0x6E), 8, 0, 1, 0);
        }
        r_no_2 = 3;
        break;
    case 3:
        motionMove();
        break;
    }
    if (!StaFlagChk(pG, STA_PL_CATCHED)) {
        EmRoutineSet(this, 0, 0, 0, 0);
    }
}

// Routine 0 / 0xF: run the scenario's own handler (SetSubAux).
void cSubChar::moveAux()
{
    pAux(this);
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

        flg.off(F_STAY);
        m_VecWork0.y += 300.0f;
        ang = Muku(&pos, &m_VecWork0, this->ang.y, 3.1415927f);
        if (fabsf(ang) > 2.0943952f) {
            if (mot_ck()) {
                m = SUB_MOT(pEm, 0x1A);
                seq = SUB_MOT(pEm, 0x53);
            } else {
                m = SUB_MOT(pEm, 0x72);
                seq = 0;
            }
            r_no_2 = 2;
            r = 4;
        } else if (fabsf(ang) > 1.0471976f) {
            if (mot_ck()) {
                m = SUB_MOT(pEm, 0x16);
                seq = SUB_MOT(pEm, 0x52);
            } else {
                m = SUB_MOT(pEm, 0x71);
                seq = 0;
            }
            r_no_2 = 1;
            r = 4;
        } else {
            if (mot_ck()) {
                m = SUB_MOT(pEm, 0x16);
                seq = SUB_MOT(pEm, 0x52);
            } else {
                m = SUB_MOT(pEm, 0x71);
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
        ang = Muku(&pos, &m_VecWork0, this->ang.y, 0.62831855f);
        pEm->ang.y += ang;
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
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x16), SUB_MOT(pEm, 0x52), 7, 5, 0);
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x71), 0, 7, 5, 0);
        }
        r_no_2 = 6;
        break;
    case 6: {
        f32 d;
        int up;

        a.x = pos.x;
        a.y = m_VecWork0.y;
        a.z = pos.z;
        b.x = m_VecWork0.x;
        b.y = m_VecWork0.y;
        b.z = m_VecWork0.z;
        r = SatMgr.hitCheck(&a, &b, &hit, &norm, 0, 0);
        d = GetDistance(a, hit);
        if ((r & 8) && d < 250000.0f) {
            m_Work1 = 0;
            m_VecWork0 = hit;
            switch (m_Work0) {
            case 1:
            default:
                r_no_2 = 0xA;
                break;
            case 2:
                r_no_2 = 0x14;
                break;
            }
        }
        up = m_VecWork0.y > pos.y + 1000.0f;
        RouteCkToPos(this, &b, &a, up | 2, &Route_h);
        pEm->ang.y += Muku(&pos, &a, this->ang.y, 0.20943952f);
        motionMove();
        break;
    }
    case 0xA:
        MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x2C), SUB_MOT(pEm, 0x67), 7, 5, 0);
        this->ang.y = atan2f(-norm.x, -norm.z);
        cCoord::matUpdate();
        PSVECScale(&norm, &a, 400.0f);
        PSVECAdd(&a, &m_VecWork0, &a);
        pos.x = a.x;
        pos.z = a.z;
        atari.onOba();
        pEm->atari.setPriority(PRI_LV3);
        atari.offSca();
        dmg.set(0, 0x80);
        fWork0 = getAdjustX(8) * 0.1f;
        m_Work2 = 10;
        r_no_2 = 0xB;
    case 0xB:
        if (fWork0 != 0.0f && m_Work2) {
            Vec v;

            PSMTXMultVecSR(pEm->mat, &vdz0, &v);
            PSVECScale(&v, &v, fWork0);
            PSVECSubtract(&pos, &v, &pos);
            m_Work2--;
        }
        if (motionMove()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x43), 0, 7, 1, 0);
            StaFlagOn(pG, STA_ASHLEY_HIDE);
            m_Work2 = 6;
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
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x44), 0, 7, 5, 0);
            r_no_2 = 0xE;
        }
        break;
    case 0xE:
        motionMove();
        if (m_Work1) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x45), 0, 7, 5, 0);
            r_no_2 = 0xF;
            m_Work1 = Motion.Seq_frame_num - 3;
        }
        break;
    case 0xF:
        if (m_Work1 > 0) {
            Vec v;

            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 200.0f / (f32) (Motion.Seq_frame_num - 3);
            PSMTXMultVecSR(mat, &v, &v);
            PSVECAdd(&pos, &v, &pos);
            m_Work1--;
        }
        if (motionMove()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x2C), SUB_MOT(pEm, 0x67), 7, 5, 0);
            r_no_2 = 0x10;
        }
        break;
    case 0x10:
        if (motionMove()) {
            u8 z = 0;

            StaFlagOff(pG, STA_ASHLEY_HIDE);
            pEm->atari.setPriority(0);
            atari.on();
            dmg.clear();
            EmRoutineSet(this, z, z, z, z);
        }
        break;
    }
}

// Routine 0 / 0x11: stoop while the player is down.
void cSubChar::moveStoop()
{
    if (!plDownCheck()) {
        m_Work0 = 1;
    }
    switch (r_no_2) {
    case 0:
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x27), 0, 7, 5, 0);
        m_Work0 = 0;
        r_no_2 = 1;
    case 1:
        if (motionMove()) {
            MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x28), 0, 7, 5, 0);
            r_no_2 = 2;
            status.on(S_DOWN);
        }
        break;
    case 2:
        if (m_Work0 && !flg.check(F_STAY)) {
            r_no_2 = 3;
        }
        motionMove();
        break;
    case 3:
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x29), 0, 7, 5, 0);
        r_no_2 = 4;
        status.off(S_DOWN);
        break;
    case 4:
        if (motionMove()) {
            EmRoutineSet(this, 0, 0, 0, 0);
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
        m_Work3 = 0;
        m_Work4 = 0;
        if (mot_ck()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x13), SUB_MOT(pEm, 0x50), 7, 5, 0);
            backCheckSet(SUB_MOT(pEm, 0x15));
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6F), 0, 7, 5, 0);
            backCheckSet(0);
        }
        m_Work1 = 0;
        if (m_PlActTime) {
            m_Work0 = 5;
            r_no_2 = 1;
        } else {
            m_PlActAngY = atan2(satNorm.x, satNorm.z) + 3.1415927f;
            m_PlActAngY = LIMIT_ANGLE(m_PlActAngY);
            m_Work0 = 5;
            r_no_2 = 1;
        }
    case 1:
        motionMove();
        ang = Muku2(pEm->ang.y, m_PlActAngY, 0.31415927f);
        pEm->ang.y += ang;
        if (fabsf(ang) < 0.15707964f) {
            m_Work0--;
            if (m_Work0 <= 0) {
                if (status.check(S_FALL_WINDOW) || (checkSatAttr(500.0f) & 0x100010)) {
                    m_Work0 = 0;
                    r_no_2 = 2;
                } else {
                    r_no_2 = 4;
                }
            }
        }
        break;
    case 2:
        if (mot_ck()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x12), SUB_MOT(pEm, 0x4F), 7, 5, 0);
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6E), 0, 7, 5, 0);
        }
        m_Work2 = 0;
        r_no_2 = 3;
    case 3:
        motionMove();
        break;
    case 4:
        if (mot_ck()) {
            motionSet(SUB_MOT(pEm, 0x14), 8, 0, 1, 0);
        } else {
            motionSet(SUB_MOT(pEm, 0x70), 8, 0, 1, 0);
        }
        r_no_2 = 5;
    case 5:
        if (motionMove()) {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1300.0f;
    PSMTXMultVec(mat, &v, &v);
    v.y = SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0) + 300.0f;
    if (GetDistance(&v, &pPL->pos) > 25000000.0f) {
        m_Work4++;
        if (m_Work4 == 300) {
            m_StopSe = SndCall(8, 0x14, &pEm->pList->world, id, 0, 0);
            m_Work4 = 0;
        }
    } else if (m_Work4 <= 0x95) {
        m_Work4++;
    }
    if (m_Work1 == 0) {
        if (m_Work0 == 0 && (pPL->r_no_0 != 0 || pPL->r_no_1 != 0xE)) {
            m_Work1 = 1;
        }
    }
    if (m_Work1) {
        if (GetDistance(&v, &pPL->pos) < 9000000.0f && !SatMgr.hitCheck(&v, &pPL->pList->world, 0, 0, 0, 0) &&
            !SatMgr.hitCheck(&pPL->pList->world, &v, 0, 0, 0, 0)) {
            ActBtn.set(ACT_CATCH, 6, (void*) catchOn, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            m_Work3 = 1;
        }
    }
    if (!SatMgr.hitCheck(&pList->world, &pPL->pList->world, 0, 0, 0, 0)) {
        status.off(S_FALL_WINDOW);
        EmRoutineSet(this, 0, 0, 0, 0);
    }
}

// Routine 0 / 0x13: wait at the ladder until the player is in sight again.
void cSubChar::moveLadderWait()
{
    switch (r_no_2) {
    case 0:
        if (mot_ck()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x19), 0, 7, 5, 0);
            r_no_2 = 2;
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6E), 0, 7, 5, 0);
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
    if (!SatMgr.hitCheck(&pList->world, &pPL->pList->world, 0, 0, 0, 0) &&
        !SatMgr.hitCheck(&pPL->pList->world, &pList->world, 0, 0, 0, 0)) {
        EmRoutineSet(this, 0, 0, 0, 0);
    }
}

// Routine 0 / 0x14: wait at the window until the player is in sight again.
void cSubChar::moveWindowWait()
{
    switch (r_no_2) {
    case 0:
        if (mot_ck()) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x19), 0, 7, 5, 0);
            r_no_2 = 2;
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6E), 0, 7, 5, 0);
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
    if (!SatMgr.hitCheck(&pList->world, &pPL->pList->world, 0, 0, 0, 0) &&
        !SatMgr.hitCheck(&pPL->pList->world, &pList->world, 0, 0, 0, 0)) {
        EmRoutineSet(this, 0, 0, 0, 0);
    }
}

// Scenario attribute of the wall `len` ahead (300 up).
u32 cSubChar::checkSatAttr(f32 length)
{
    Vec a;
    Vec b;

    a = pos;
    b.z = length;
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
    PSMTXMultVec(pEm->mat, &a, &a);
    a0 = a;
    b.x = 0.0f;
    b.y = height;
    b.z = 1000.0f;
    PSMTXMultVec(pEm->mat, &b, &b);
    b0 = b;
    step.x = width / 10.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    PSMTXMultVecSR(pEm->mat, &step, &step);
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

// Routine 1: damage reaction (m_Work0 selects the motion / effect).
void cSubChar::moveDamage()
{
    if (pAuxDm) {
        ((void (*)()) pAuxDm)();
        return;
    }
    void* m = 0;   // declared after the handler test: `li r30, 0` in the switch block

    switch (r_no_1) {
    case 0:
        beginDamage();
        // raw store through the info's address: the pG load below stays after the flag store
        atari.on();
        if (DbgFlagChk(pG, DBG_NO_DEATH)) {
            switch (m_Work0) {
            case 7:
                m_Work0 = 8;
                break;
            case 9:
                m_Work0 = 10;
                break;
            }
        }
        // one body per value (no shared labels): the compare tree tests every value; the
        // identical bodies are cross-jumped afterwards
        switch (m_Work0) {
        case 0:
            m = SUB_MOT(pEm, 0x2E);
            break;
        case 1:
            m = SUB_MOT(pEm, 0x2F);
            break;
        case 2:
            m = SUB_MOT(pEm, 0x2E);
            break;
        case 3:
            m = SUB_MOT(pEm, 0x2F);
            break;
        case 4:
            m = SUB_MOT(pEm, 0x2E);
            break;
        case 5:
            m = SUB_MOT(pEm, 0x2F);
            break;
        case 6:
            m = 0;
            break;
        case 7:
            m = SUB_MOT(pEm, 0x32);
            EstSet(pEm, -1, 0, 0, EFF_PL01, ChkWaterEffectEnable(&pos) ? 6 : 5, 0, ESP_CORE_KIND_NONE, pEm, 0);
            break;
        case 8:
            m = SUB_MOT(pEm, 0x32);
            EstSet(pEm, -1, 0, 0, EFF_PL01, ChkWaterEffectEnable(&pos) ? 6 : 5, 0, ESP_CORE_KIND_NONE, pEm, 0);
            break;
        case 9:
            m = SUB_MOT(pEm, 0x31);
            EstSet(pEm, -1, 0, 0, EFF_PL01, ChkWaterEffectEnable(&pos) ? 8 : 7, 0, ESP_CORE_KIND_NONE, pEm, 0);
            break;
        case 10:
            m = SUB_MOT(pEm, 0x31);
            EstSet(pEm, -1, 0, 0, EFF_PL01, ChkWaterEffectEnable(&pos) ? 8 : 7, 0, ESP_CORE_KIND_NONE, pEm, 0);
            break;
        case 11:
            m = SUB_MOT(pEm, 0x6A);
            atari.off();
            break;
        }
        MOT_SET(pEm, MOTION(pEm), m, 0, 3, 1, 0);
        setFace(1);
        SndCall(8, 9, &pEm->pList->world, id, 0, 0);
        pEm->r_no_1 = 1;
    case 1:
        if (pEm->Motion.Seq_frame >= 10.0f && m_Work0 == 11 && landCheck()) {
            atari.on();
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6B), 0, 3, 1, 0);
            EstSet(pEm, -1, 0, 0, EFF_PL01, ChkWaterEffectEnable(&pos) ? 10 : 9, 0, ESP_CORE_KIND_NONE, pEm, 0);
            r_no_1 = 10;
            if (DbgFlagChk(pG, DBG_NO_DEATH)) {
                m_Work0 = 8;
                r_no_1 = 5;
            }
        }
        if (pEm->motionMove()) {
            switch (m_Work0) {   // default first (laid out first); separate identical bodies
            default:
                setFace(0);
                EmRoutineSet(this, 0, 0, 0, 0);
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
        if (MotionMove(pEm, 0)) {
            if (m_Work0 == 7 || m_Work0 == 9) {
                pG->ashley_life = hp = 0;
            }
        }
        break;
    case 5:
        if (m_Work0 == 8) {
            m = SUB_MOT(pEm, 0x33);
        } else {
            m = SUB_MOT(pEm, 0x34);
        }
        MOT_SET(pEm, MOTION(pEm), m, 0, 3, 5, 0);
        r_no_1 = 6;
    case 6:
        if (motionMove()) {
            endDamage();
            EmRoutineSet(this, 0, 0, 0, 0);
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
        CamCtrl.deleteAttachCamera(pPL->Motion.pAttachCam, pPL);
        if (status.check(S_DOWN)) {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x30), 0, 3, 1, 0x32);
        } else {
            MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x30), 0, 3, 1, 0);
            EstSet(this, -1, 0, 0, EFF_PL01, ChkWaterEffectEnable(&pos) ? 4 : 3, 0, ESP_CORE_KIND_NONE, this, 0);
        }
        SndCall(8, 0xD, &pList->world, id, 0, 0);
        atari.m_parts_no = 4;
        r_no_1 = 1;
    case 1:
        if (motionMove()) {
            atari.off();
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
    pAux(this);
}

// Routine 5: scenario event: walk to m_VecWork0 along the route.
void cSubChar::moveEvent()
{
    f32 ang;

    switch (r_no_1) {
    case 0:
        MotionMove(this, 0);
        break;
    case 1:
        switch (r_no_2) {
        case 0:
            if (mot_ck()) {
                MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x16), SUB_MOT(pEm, 0x52), 10, 5, 0);
            } else {
                MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x71), SUB_MOT(pEm, 0x78), 10, 5, 0);
            }
            r_no_2 = 1;
            Motion.Mot_attr &= ~1;
        case 1: {
            Vec out;

            RouteCkToPos(this, &m_VecWork0, &out, m_VecWork0.y > pos.y + 1000.0f, &Route_h);
            ang = Muku(&pos, &out, this->ang.y, 0.44879895f);
            this->ang.y += ang;
            if (fabsf(ang) < 0.20943952f) {
                if (mot_ck()) {
                    MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x16), SUB_MOT(pEm, 0x52), 10, 5, 0);
                } else {
                    MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x71), SUB_MOT(pEm, 0x78), 10, 5, 0);
                }
                r_no_2 = 2;
            }
            break;
        }
        case 2: {
            Vec out;

            RouteCkToPos(this, &m_VecWork0, &out, m_VecWork0.y > pos.y + 1000.0f, &Route_h);
            this->ang.y += Muku(&pos, &out, this->ang.y, 0.31415927f);
            if (GetDistance(pos, m_VecWork0) < 250000.0f) {
                if (mot_ck()) {
                    MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x12), SUB_MOT(pEm, 0x4F), 7, 5, 0);
                } else {
                    MOT_SET(pEm, MOTION(pEm), SUB_MOT(pEm, 0x6E), 0, 7, 5, 0);
                }
                EmRoutineSet(this, 5, 0, 0, 0);
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
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x43), 0, 7, 5, 0);
        r_no_1 = 1;
        break;
    case 1:
        if (motionMove()) {
            r_no_1 = 2;
        }
        break;
    case 2:
        MOT_SET(this, MOTION(this), SUB_MOT(pEm, 0x44), 0, 7, 5, 0);
        r_no_2 = 3;
        break;
    case 3:
        motionMove();
        break;
    }
}

// Step `spd` towards `target` in the XZ plane.
void cSubChar::movePos(Vec* toPos, f32 spd)
{
    f32 ang;

    ang = Muku(&pos, toPos, 0.0f, 3.1415927f);
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
    m_NeckTimer = 0;
    m_NeckVec.x = 0.0f;
    m_NeckVec.y = 0.0f;
    m_NeckVec.z = 0.0f;
}

// Turn the neck (parts 3) towards the player while neckSet() keeps asking for it.
void cSubChar::neckCtrl()
{
    const f32 spdBack = 0.10471976f;   // pool order: the two speeds first
    const f32 spdHome = 0.15707964f;
    cParts* p = getPartsPtr(3);
    int on = 1;
    f32 ang;

    p->motParts.flags |= 0x40000000;
    if (!(pPL->stat.check(cPlayer::F_EVENT))) {
        on = 0;
    }
    if (m_NeckTimer == 0 || (Motion.blend != 0 && Motion.blend->Brate != 0.0f) || on) {
        m_NeckVec.y += Muku2(m_NeckVec.y, 0.0f, 0.15707964f);
    } else {
        if (m_NeckTimer > 0) {
            m_NeckTimer--;
        }
        ang = this->ang.y + m_NeckVec.y;
        ang = Muku(&pos, &pPL->pos, ang, 0.10471976f);
        m_NeckVec.y += ang;
        if (m_NeckVec.y > 0.78539819f) {
            m_NeckVec.y = 0.78539819f;
        } else if (m_NeckVec.y < -0.78539819f) {
            m_NeckVec.y = -0.78539819f;
        }
    }
    p->ik_inv_mat[0][0] = m_NeckVec.y;
    m_NeckTimer = 0;
}

// Ask the neck to look at `pos` this frame (neckCtrl turns toward the player while requested).
void cSubChar::neckSet(Vec* pos)
{
    m_NeckTgt = *pos;
    m_NeckTimer = 1;
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

// status bit4: the aiming player has her in front of him within 10000.
int cSubChar::cautionCheck()
{
    int ret;

    if (flg.check(F_MOVE_TO)) {
        ret = 0;
    } else if (!(plStat & 0x10)) {
        ret = 0;
    } else if (fabsf(GetXZAngleLocal(&pPL->pos, &pos, pPL->ang.y)) > 0.78539819f) {
        ret = 0;
    } else if (GetDistance(pPL->pList->world, pList->world) > 100000000.0f) {
        ret = 0;
    } else {
        ret = 1;
    }
    if (ret == 1) {
        status.on(S_WEPCAUTION);
    } else {
        status.off(S_WEPCAUTION);
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

    PSMTXMultVec(pEm->mat, &a, &a);
    Vec b = { 0.0f, 400.0f, 600.0f };
    PSMTXMultVec(pEm->mat, &b, &b);
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
    PSMTXMultVec(pEm->mat, &a, &a);
    PSMTXMultVec(pEm->mat, &b, &b);
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

    PSVECAdd(&a, &pEm->pos, &a);
    Vec b = { 0.0f, 400.0f, 600.0f };
    PSMTXMultVec(pEm->mat, &b, &b);
    Vec dir;
    Vec p;
    u16 winStat;
    cEmWindow* w;
    f32 ang;
    if (ChkWindow(this, &a, &b, 1, &winStat, &dir, &p, &w)) {
        if ((w->ChkStatus() & 1) == 0) {
            return 3;
        }
        ang = atan2f(-dir.x, -dir.z);
        if (EmRackCk(this, &pos, ang) == 0) {
            return 3;
        }
        if (w->ChkBreakDir(&pEm->pos) == 2) {
            status.on(S_FALL_WINDOW);
            EmRoutineSet(this, 0, 0x12, 0, 0);
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
    if (status.check(S_PL_NEAR)) {
        return 0;
    }
    SubOpenDoorSet(d);
    return 1;
}

// The player is aiming (plStat 0x10) and she is not flagged 3 (event-held).
int cSubChar::readyCheck()
{
    if (flg.check(F_MOVE_TO)) {
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
    cSubChar* sub = SUB_CHAR();
    Vec p;
    Vec r;

    pPL->dmg.set(0, 0x80);
    sub->dmg.set(0, 0x80);
    EmRoutineSet(sub, 0, 9, 0, 0);
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
        pEm->m_Work0 = 0;
        ret = 1;
    } else if (satAttr & 0x2000) {
        pEm->m_Work0 = 1;
        ret = 1;
    } else if (satAttr & 0x1000) {
        pEm->m_Work0 = 2;
        ret = 1;
    } else if (satAttr & 0x80000) {
        pEm->m_Work0 = 4;
        ret = 1;
    } else if (satAttr & 0x100010) {
        int up = pPL->pos.y < pos.y - 1000.0f;

        if (up) {
            if (getCliffHeight(atan2(-satNorm.x, -satNorm.z)) < 2900.0f) {
                pEm->m_Work0 = 3;
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
        ang.y += Muku3(ang.y, &d, 3.1415927f);
    }
    if (m_PlActTime && dist <= 300.0f && (pPL->r_no_0 != 0 || pPL->r_no_1 != 0xE)) {
        if (getCliffHeight(m_PlActAngY) < 2900.0f) {
            m_PlActTime = 0;
            pos = m_PlActPos;
            ang.y = m_PlActAngY;
            pEm->m_Work0 = 3;
            ret = 1;
        } else {
            EmRoutineSet(this, 0, 0x12, 0, 0);
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
    m_Work0 = up;
    m_Work1 = ((s8) level < 0 ? -(s8) level : (s8) level) - 2;
    atari.off();
    dmg.set(0, 0x80);
    return 1;
}

// Drop from the partner's height to the floor 1000 ahead in direction `ang` (100000 when < 800).
f32 cSubChar::getCliffHeight(f32 dy)
{
    const f32 len = 1000.0f;
    Vec v;
    Vec r;
    f32 h;

    r.y = dy;
    v.x = 0.0f;
    v.y = 300.0f;
    v.z = len;
    r.x = 0.0f;
    r.z = 0.0f;
    RotVector(&v, &r, &v);
    PSVECAdd(&pos, &v, &v);
    h = pos.y - SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0);
    if (h < 800.0f) {
        h = 100000.0f;
    }
    return h;
}

// status bit2: the player's head is close below her and looking up.
void cSubChar::pantsCheck()
{
    Vec d;
    Vec* hp;
    Vec* sp;

    status.off(S_PANTS);
    if (pG->game_costume == 1) {
        return;
    }
    hp = &pPL->getPartsPtr(4)->world;
    sp = &pList->world;
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
    status.on(S_PANTS);
}

// The player is running (plStat bit3).
int cSubChar::ckPlRun()
{
    return (plStat & 8) != 0;
}

// Motion sequence sound (seNo) -> SndCall: footsteps by surface kind (Motion.Seq_old.Free), voices as is.
void cSubChar::seqSeCtrl()
{
    int no;
    int parts;
    u16 blk;   // u16: no mask before the u16 SndCall argument

    if (Motion.Seq_old.Se == 0) {
        return;
    }
    no = Motion.Seq_old.Se - 1;
    parts = 0;
    blk = 5;
    switch (Motion.Seq_old.Free & 3) {
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
    Motion.Seq_old.Se = 0;
}

// Blend the look-back motion `mot` in (NULL: off).
void cSubChar::backCheckSet(void* mot)
{
    if (mot) {
        f32 rate = subMot.Brate;

        MOT_SET(this, &subMot, mot, 0, 3, 4, 0);
        subMot.Brate = rate;
        Motion.blend = &subMot;
        subMot.Mot_flag |= 0x80000000;
    } else {
        Motion.blend = 0;
        subMot.Brate = 0.0f;
    }
}

// Fade the look-back blend in (sub404 == 2) or out (1).
void cSubChar::backCheckMove()
{
    MotionWorkSub* w = pEm->Motion.blend;
    const f32 d = 0.14f;

    if (w == 0) {
        return;
    }
    switch (m_BackRno) {
    case 0:
        break;
    case 1:
        if (w->Brate > 0.0f) {
            w->Brate -= d;
            if (pEm->Motion.blend->Brate < 0.0f) {
                pEm->Motion.blend->Brate = 0.0f;
                m_BackRno = 0;
            }
        }
        break;
    case 2:
        if (Motion.blend->Brate < 1.0f) {
            Motion.blend->Brate += d;
            if (Motion.blend->Brate > 1.0f) {
                Motion.blend->Brate = 1.0f;
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
    int n = EmMgr.getArrayNum();

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
        if (SatMgr.hitCheck(&pList->world, &em->pList->world, 0, 0, 0, 0)) {
            continue;
        }
        return 1;
    }
    return 0;
}

// The damage routine handler (routine 4): pl_sub's SetSubDamage passes it.
void cSubChar::setEmFunc(void (*pFunc)())
{
    m_pFunc = pFunc;
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
        status.on(S_PL_NEAR);
    } else {
        status.off(S_PL_NEAR);
    }
    n = EmMgr.getArrayNum();
    status.off(S_EM_NEAR).off(S_EM_NEAR2);
    if (!flg.check(F_MOVE_TO)) {
        for (i = 0; i < n; i++) {
            cEm* em = EmMgr.fastAt(i);

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
                status.on(S_EM_NEAR2);
                if (subNear2) {
                    Draw_pos(&em->pos, 1000);
                }
            }
            if (GetDistance(pos, em->pos) < 1000000.0f) {
                status.on(S_EM_NEAR);
            }
        }
    }
    if (pAnotherRoute && moveAnotherRoute()) {
        pAnotherRoute = 0;
    }
    if (m_PlActTime && !SatMgr.hitCheck(&pList->world, &pPL->pList->world, 0, 0, 0, 0)) {
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
        r = RouteCkToPos(this, &pAnotherRoute->pos, &distPos, up, &Route_h);
    } else if (flg.check(F_MOVE_TO)) {
        if (fabsf(m_TargetPos.y - pos.y) > 1000.0f) {
            up = 1;
        }
        r = RouteCkToPos(this, &m_TargetPos, &distPos, up, &Route_h);
    } else if (m_PlActTime) {
        if (fabsf(m_PlActPos.y - pos.y) > 1000.0f) {
            up = 1;
        }
        r = RouteCkToPos(this, &m_PlActPos, &distPos, up, &Route_h);
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
            r = RouteCkToPos(this, &pPL->pos, &distPos, up, &Route_h);
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
            fl = SatMgr.getFloor(&distPos, 0, 1000.0f, 1000.0f, 0);
            if (fl != -100000.0f) {
                distPos.y = fl + 100.0f;
            }
            if (SatMgr.hitCheck(&pPL->pList->world, &distPos, 0, 0, 0, 0)) {
                PSVECSubtract(&distPos, &pPL->pList->world, &d);
#line 3781 "D:/Bio4/Prog/pl_npc.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, -400.0f);
                PSVECAdd(&distPos, &d, &distPos);
            }
            if (fabsf(distPos.y - pos.y) > 1000.0f) {
                up = 1;
            }
            r = RouteCkToPos(this, &distPos, &distPos, up, &Route_h);
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
            eprintf(0x18 + i * 8, 0x180, 0, 0, "%d", flg.check((FLAG) i));
        }
        Draw_pos(&distPos, 1000);
    }
}

// status bit8: a damage area 1500 ahead on the way to subTarget.
void cSubChar::frontCheck()
{
    const f32 len = 1500.0f;   // pool order: 1500 before 0
    Vec d;

    status.off(S_NEED_STOP);
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
        status.on(S_NEED_STOP);
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

    PSVECAdd(&a, &pEm->pos, &a);
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
    status.on(S_HITWALL);
}

// Event start: interrupts her routine, collision off (bits 0x300).
void cSubChar::beginEvent(u32 flag)
{
    interrupt();
    atari.off();
}

// Event end: cloth reset (be_flag 0x200000), collision on.
void cSubChar::endEvent(u32 flag)
{
    be_flag |= 0x200000;
    atari.on();
}

// pl_sub SubCharCtrl modes: 0 stop, 1 wait here, 2 follow, 3 warp to the player and follow,
// 4 move to m_TargetPos, 5 re-init, 6 wait (only from routine 0).
void cSubChar::control(int mode)
{
    f32 far = 1000.0f;   // dead initialisers: the original's pool has 1000 and 400 here
    f32 near = 400.0f;

    if (hp <= 0) {
        return;
    }
    flg.off(F_MOVE_TO).off(F_DONT_RUN);
    flg.on(F_PL_CTRL);
    switch (mode) {
    case 0:
        flg.on(F_SLEEP);
        break;
    case 1:
        if (r_no_0 == 5) {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        if (r_no_0 != 0 || r_no_1 != 0x10) {
            flg.off(F_SLEEP);
            flg.on(F_STAY);
            atari.on();
        }
        break;
    case 3:
        if (r_no_0 == 5) {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        if (r_no_0 != 0 || r_no_1 != 0x10) {
            setPos(&pPL->pos);
        }
        r_no_0 = 0;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 1;
        atari.on();
    case 2:
        if (r_no_0 == 5) {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        if (r_no_0 != 0 || r_no_1 != 0x10) {
            flg.off(F_SLEEP).off(F_STAY);
            atari.on();
        }
        break;
    case 4: {
        int md = 1;   // shared by both arms (`li r6, 1` before the test)

        flg.off(F_STAY);
        flg.on(F_MOVE_TO);
        if (m_TargetPos.x != 193.0f) {
            EmRoutineSet(this, 0, md, 0, 0);
            atari.on();
        } else if (m_TargetDir != 193.0f) {
            m_TargetPos = pos;
            EmRoutineSet(this, 0, md, 0, 0);
            atari.on();
        } else {
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
    case 5:
        init();
        EmRoutineSet(this, 0, 0, 0, 0);
        break;
    case 6:
        if (r_no_0 == 0 && flg.check(F_PL_CTRL)) {
            flg.on(F_STAY);
            flg.off(F_PL_CTRL);
            EmRoutineSet(this, 0, 0, 0, 0);
        }
        break;
    }
}

// Ledge in front of the partner: point 800 out from the wall below it and the facing angle.
int getFallPos(cSubChar* pl, Vec* pVec, Vec* pAng)
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
    pAng->x = 0.0f;
    pAng->y = Muku3(0.0f, &d, 3.1415927f);
    pAng->z = 0.0f;
    pVec->x = b.x;
    pVec->y = SatMgr.getFloor(&b, 0, 600.0f, 100000.0f, 0);
    pVec->z = b.z;
    return 1;
}

// Routine bits of the partner for the camera / scenario.
u32 SubCharGetStatus()
{
    cSubChar* sub = SUB_CHAR();
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
            switch (sub->m_Work0) {
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
    if (sub->flg.check(cSubChar::F_STAY) || sub->flg.check(cSubChar::F_SLEEP) || (sub->r_no_0 == 0 && sub->r_no_1 == 0x10)) {
        ret |= 0x40000000;
    } else {
        ret |= 0x20000000;
    }
    if (sub->flg.check(cSubChar::F_MOVE_TO) && sub->status.check(cSubChar::S_ARRIVED)) {
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
    emi = pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    if (emi->n == 0) {
        return 0;
    }
    found = -1;
    for (i = 0; i < pG->pEmi->n; i++) {
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

            for (j = 0; j < pG->pEmi->n; j++) {
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
    for (i = 0; i < (pG->pEmi)->n; i++) {
        e = &(pG->pEmi)->entry[i];
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
    EmiData* emi = pG->pEmi;
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
    for (i = 0; i < pG->pEmi->n; i++) {
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
    if (pAuxDm) {
        EmRoutineSet(this, 1, 0, 0, 0);
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
            EmRoutineSet(this, one, 0, 0, 0);
            m_Work0 = 11;
        } else if ((s16) pG->ashley_life > 0) {
            EmRoutineSet(this, one, 0, 0, 0);
            m_Work0 = 2;
        } else {
            dmg.m_Timer = 0x80;
            EmRoutineSet(this, 2, 0, 0, 0);
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
            EmRoutineSet(this, 1, 0, 0, 0);
            if (Front_check(this, &dmg.m_PosFrom, 1.5707964f)) {
                m_Work0 = 7;
                ang.y += Muku(&pos, &dmg.m_PosFrom, 3.1415927f, 3.1415927f);
            } else {
                m_Work0 = 9;
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
            EmRoutineSet(this, 1, 0, 0, 0);
            m_Work0 = 2;
        } else {
            dmg.m_Timer = 0x80;
            EmRoutineSet(this, 2, 0, 0, 0);
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
    m_Work0 = kind;
}

// The player registers a ledge for her to wait at (pos / facing angle), for 240 frames.
void cSubChar::registPlAction(Vec* pos, f32 y, u8 a)
{
    m_PlActPos = *pos;
    m_PlActAngY = y;
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
        EstSet(pl, -1, 0, 0, EFF_ROOM, 0x21, 0, ESP_CORE_KIND_NONE, pl, 0);
    }
    d = GetDistance(&m_PosOldWater, &pl->pos);
    if (sibukiTimer) {
        sibukiTimer--;
    } else if (d > spd1) {
        EstSet(pl, -1, 0, 0, EFF_ROOM, 0x23, 0, ESP_CORE_KIND_NONE, pl, 0);
        sibukiTimer = 10;
    } else if (d > spd0) {
        EstSet(pl, -1, 0, 0, EFF_ROOM, 0x22, 0, ESP_CORE_KIND_NONE, pl, 0);
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
    cParts* parts;
    cParts* body;

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
    PSVECAdd(&posBustR, &ofs, &parts->pos);
    parts = getPartsPtr(0x1E);
    PSVECAdd(&posBustL, &ofs, &parts->pos);
    parts = getPartsPtr(0x1A);
    PSVECAdd(&posScarf, &ofs, &parts->pos);
    bbx += step;
    parts->matUpdate();
    PSMTXConcat(parts->pParent->mat, parts->mat, parts->mat);
    parts->world.x = parts->mat[0][3];
    parts->world.y = parts->mat[1][3];
    parts->world.z = parts->mat[2][3];
}

// Eyelid (parts 0x1C) blink sequence on `timer` and the eye direction (parts 0x20/0x21) wander
// (eyeDir), as in pl_class moveEyeNormal. The switch is written sorted with
// `default` first and every case spelled out (no shared labels): cross-jumping merges the identical
// bodies into the LAST copy, which is why the original's block order is 0,3,4,1E,58,5A,5D,5E,5F,60,
// 61,62 while its pool is in ascending case order.
void cSubChar::moveFace()
{
    static int timer;
    cParts* p;

    p = getPartsPtr(0x1C);
    switch (timer++) {
    default:
        p->ang.x = 0.0f;
        break;
    case 0: {
        eyeDir = ((f32) (int) (u8) (Rnd() % 200) * 0.01f - 1.0f) * 3.1415927f * 0.1f;
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
        eyeDir = 0.0f;
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
            eyeDir += ((f32) (int) (u8) (Rnd() % 200) * 0.01f - 1.0f) * 0.03141592815518379f;
            eyetime = (u8) (Rnd() % 3) + 2;
        }
    }
    eyeDir.limit(-0.3141592741012573f, 0.3141592741012573f);
    p = getPartsPtr(0x20);
    p->ang.y = eyeDir;
    p = getPartsPtr(0x21);
    p->ang.y = eyeDir;
    eyeDir.move();
}

// Fade the shadow (shdCol) out while she is on a ledge / above the camera / on a slope.
void cSubChar::shadowCtrl()
{
    int fade = 0;

    if (flg.check(F_SHADOW_OFF)) {
        fade = 1;
    }
    if (pG->Camera.param.pos.y < pEm->pos.y) {
        fade = 1;
    }
    if (pEm->pFloor_norm && pEm->pFloor_norm->y < 0.8f) {
        fade = 1;
    }
    if (fade) {
        if (pEm->Shd_color <= 0xEF) {
            pEm->Shd_color += 0x10;
        } else {
            pEm->Shd_color = 0xFF;
        }
    } else {
        if (pEm->Shd_color > 0xF) {
            pEm->Shd_color -= 0x10;
        } else {
            pEm->Shd_color = 0;
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
    case DMG_TYPE_GRENADE_BLAST:
    case DMG_TYPE_GRENADE: {
        LifeDownSet2(this, (s16) pG->ashley_life_max, 0, 0);
        int one = 1;
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            EmRoutineSet(this, one, 0, 0, 0);
            m_Work0 = 11;
        } else if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            EmRoutineSet(this, one, 0, 0, 0);
            m_Work0 = 2;
        } else {
            dmg.m_Timer = 0x80;
            EmRoutineSet(this, 2, 0, 0, 0);
        }
        StaFlagOff(pG, STA_SUB_LADDER);
        int two = 1;
        LifeDownSet2(this, (s16) pG->ashley_life_max, 0, 0);
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            EmRoutineSet(this, two, 0, 0, 0);
            m_Work0 = 11;
        } else if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            EmRoutineSet(this, two, 0, 0, 0);
            m_Work0 = 2;
        } else {
            dmg.m_Timer = 0x80;
            EmRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
    case DMG_TYPE_FIRE:
    case DMG_TYPE_FLAME: {
        LifeDownSet2(this, (s16) pG->ashley_life_max, 0, 0);
        int one = 1;
        if (StaFlagChk(pG, STA_SUB_LADDER)) {
            EmRoutineSet(this, one, 0, 0, 0);
            m_Work0 = 11;
        } else if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            EmRoutineSet(this, one, 0, 0, 0);
            m_Work0 = 2;
        } else {
            dmg.m_Timer = 0x80;
            EmRoutineSet(this, 2, 0, 0, 0);
        }
        break;
    }
    case DMG_TYPE_LAMP:
        LifeDownSet2(this, 300, 0, 0);
        if ((s16) pG->ashley_life > 0) {
            dmg.m_Timer = 0x5A;
            EmRoutineSet(this, 1, 0, 0, 0);
            m_Work0 = 2;
        } else {
            dmg.m_Timer = 0x80;
            EmRoutineSet(this, 2, 0, 0, 0);
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
    flg.off(F_SHADOW_OFF);
    if (m_StopSe) {
        SndStop(m_StopSe, 0);
    }
}

// 1 while the partner (id 3) is in a routine 0 state the scenario may hide her from.
int SubCharHideCheck()
{
    cSubChar* sub = SUB_CHAR();

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

    if (SatMgr.hitCheck(&pPL->pList->world, &pSUB->pList->world, &hit, &nrm, 0, 0)) {
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
    cSubChar* sub = SUB_CHAR();
    u32 ret;

    if (sub == 0) {
        return 0;
    }
    if (sub->flg.check(cSubChar::F_STAY) || (sub->r_no_0 == 0 && sub->r_no_1 == 0x10)) {
        ret = 2;
    } else {
        ret = 1;
    }
    if (StaFlagChk(pG, STA_TAKEAWAY)) {
        return ret | 8;
    }
    if (sub->flg.check(cSubChar::F_MOVE_TO)) {
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
