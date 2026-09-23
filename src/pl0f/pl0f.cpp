// pl0f module (D:/Bio4/Prog/pl0f.cpp): the lake boat of the Del Lago fight. A cEm placed by the room
// script; two point masses (bow / stern) on the water carry the hull (pl0fBoatControl), the player
// rides and steers it (PlBoatMove / plboat_R2_*, setTiller), the boss drags it by the anchor rope
// (pl0fBoatChaseBoss) and the player throws the harpoons (plboat_R2_SpearSet / SpearThrow).
//
// The boat is an enemy work (Pl0fInit is the module's EmInitFunc; em->type / em->set from the
// room's enemy list pick the start: type 0 the lake boat (set 1 = the player already aboard),
// 1..5 the ferry entrances / exits of rooms 10D / 10E). Its routines: r_no_0 0 init, 1 move with
// r_no_1: 0 wait at the shore, 1 ride (steered), 2 ride start, 3 crash guard, 4 dropped by the
// boss (player thrown in the water), 5 the player climbs back in, 6 dragged by the boss, 7 boss
// crash guard, 8..13 the room 10D / 10E entrances / exits. The player runs routine 1 == 0xF
// (pl_R1_Boat -> BoatMoveFunc = PlBoatMove) with r_no_2 = the plboat_R2_* state: 0 board, 1 get
// off, 2 sit / steer (setTiller), 3/4 harpoon aim / throw (the lake fight), 5 fall in the water,
// 6 swim back (button mashing), 7 crash guard, 8 climb in, 9 die (drowned / eaten), 10/11 the
// hiding variant of the harpoon aim / throw, 12 the boss death, 13..18 the room entrances / exits;
// the partner (Ashley on the ferry) runs the subBoat* damage-routine handlers. The cameras are the
// module's own (pl0f_camera handed to CamCtrl as the extra camera). Hull physics: two point masses
// bow / stern (Pl0fNode) kept at their rest distance by four relaxation passes, the heading from
// their line; the tiller adds speed to the stern node.

#include "atari.h"
#include "light.h"
#include "pl0f.h"
#include "em2f.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_body.h"
#include "pl_wep.h"
#include "pl_cloth.h"
#include "global.h"
#include "main.h"
#include "joy.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "esp.h"
#include "est.h"
#include "obj00.h"
#include "snd.h"
#include "pad.h"
#include "game.h"
#include "sscrn.h"
#include "dbmodule.h"
#include "rnd.h"
#include "math_sub.h"
#include "db_log.h"
#include "em_sub.h"
#include "act_btn.h"
#include <dolphin/os.h>
#include "pl_mod.h"


// The module's 0x30-byte COMMON block: uninitialised template statics of the original object,
// appended to .bss by snmakerel.
asm(".comm common_pl0f,48,4");

#line 1 "D:/Bio4/Prog/pl0f.cpp"

typedef void (*Pl0fFunc)(cPl0f*);
typedef void (*PlBoatFunc)(cPlayer*);

static void pl0f_R0_Init(cPl0f* em);
static void pl0f_R0_Move(cPl0f* em);
static void pl0f_R1_Wait(cPl0f* em);
static void pl0f_R1_RideMove(cPl0f* em);
static void pl0f_R1_RideStart(cPl0f* em);
static void pl0f_R1_BossMove(cPl0f* em);
static void pl0f_R1_Guard(cPl0f* em);
static void pl0f_R1_Drop(cPl0f* em);
static void pl0f_R1_WaterRide(cPl0f* em);
static void pl0f_R1_BossGuard(cPl0f* em);
static void pl0f_R1_R10dIn(cPl0f* em);
static void pl0f_R1_R10dOut(cPl0f* em);
static void pl0f_R1_R10eIn(cPl0f* em);
static void pl0f_R1_R10eOut(cPl0f* em);
static void pl0f_R1_R10eIn2(cPl0f* em);
static void pl0f_R1_R10eOut2(cPl0f* em);
static void pl0fActRide(cPl0f* em);
static void pl0fActRideR10d(cPl0f* em);
static void pl0fActRideR10e(cPl0f* em);
static void pl0fActRideR10e2(cPl0f* em);
static void pl0fActGetOff(cPl0f* em);
static void PlBoatMove(cPlayer* pl);
static void plboat_R2_Ride(cPlayer* pl);
static void plboat_R2_Getoff(cPlayer* pl);
static void plboat_R2_Move(cPlayer* pl);
static void plboat_R2_SpearSet(cPlayer* pl);
static void plboat_R2_SpearThrow(cPlayer* pl);
static void plboat_R2_SpearSet2(cPlayer* pl);
static void plboat_R2_SpearThrow2(cPlayer* pl);
static void plboat_R2_BossDie(cPlayer* pl);
static void plboat_R2_Guard(cPlayer* pl);
static void plboat_R2_FallWater(cPlayer* pl);
static void plboat_R2_Swim(cPlayer* pl);
static void plboat_R2_WaterRide(cPlayer* pl);
static void plboat_R2_Die(cPlayer* pl);
static void plboat_R2_R10dIn(cPlayer* pl);
static void plboat_R2_R10dOut(cPlayer* pl);
static void plboat_R2_R10eIn(cPlayer* pl);
static void plboat_R2_R10eOut(cPlayer* pl);
static void plboat_R2_R10eIn2(cPlayer* pl);
static void plboat_R2_R10eOut2(cPlayer* pl);
static void subBoatRide();
static void subBoatGetoff();
static void subBoatR10dIn();
static void subBoatR10eIn();
static void subBoatR10eIn2();

#define VIB_TBL ((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore))
// The lake boat player `pl` rides (cPlayer::m_pBoat; pl0e reads the same field as its jet ski, PL_JETSKI).
#define PL_BOAT(pl) ((cPl0f*) (pl)->m_pBoat)
#define ROPE(w) ((cObj*) (w)->pRope)


static Pl0fFunc Pl0f_R0_move_tbl[5] = {
    pl0f_R0_Init,
    pl0f_R0_Move,
    0,
    0,
    (Pl0fFunc) Em_R0_Scenario,
};

static Pl0fFunc Pl0f_R1_move_tbl[14] = {
    pl0f_R1_Wait,
    pl0f_R1_RideMove,
    pl0f_R1_RideStart,
    pl0f_R1_Guard,
    pl0f_R1_Drop,
    pl0f_R1_WaterRide,
    pl0f_R1_BossMove,
    pl0f_R1_BossGuard,
    pl0f_R1_R10dIn,
    pl0f_R1_R10dOut,
    pl0f_R1_R10eIn,
    pl0f_R1_R10eOut,
    pl0f_R1_R10eIn2,
    pl0f_R1_R10eOut2,
};

static Camera pl0f_camera = { 0 };
static f32 pl0f_spd_damp = 0.96f;

// REL entry: registers the boat constructor as the enemy init function.
extern "C" void _prolog()
{
    OSReport("Pl0f prolog Ok\n");
    EmInitFunc = Pl0fInit;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// EmInitFunc: placement-constructs the boat in the cEm work.
void Pl0fInit(cEm* em)
{
    new (em) cPl0f();
}

// Per-frame update (emMove): clears the no-crash / no-drop flags, runs the r_no_0 routine, the
// idle ripple effect every 0x1D frames outside rooms 10D / 10E, pins the long rope's end to the
// anchor (or frees it) and shows the rope only while the boss holds it (and is not submerged, boss
// flag bit8); a boss lunge (boss flag bit6) rocks the hull (sway PI/16) and shoves both nodes away
// from it with an impact SE.
void cPl0f::move()
{
    Pl0fWork* w = PL0F_WK(this);

    w->Be_flg &= ~0xC;
    Pl0f_R0_move_tbl[r_no_0](this);
    if (pG->room_id != 0x10D && pG->room_id != 0x10E) {
        if (w->Ripple_wait) {
            w->Ripple_wait--;
        } else {
            w->Ripple_wait = 0x1D;
            EstSet(this, -1, 0, 0, 0xF, 7, 0, 0x35, this, 0);
        }
    }
    if (w->pRope) {
        if (w->pAnchor) {
            Vec v;

            v.x = 0.0f;
            v.y = 1000.0f;
            v.z = 0.0f;
            PSMTXMultVec(w->pAnchor->mat, &v, &v);
            PenClothFixSet(ROPE(w), &w->Cloth, 0x1C, &v);
        } else {
            PenClothFixClear(ROPE(w), &w->Cloth, 0x1C);
        }
        if (!StaFlagChk(pG, STA_PL_SWIM_CAMERA) && !(w->Be_flg & 8) && w->pBoss) {
            ROPE(w)->be_flag |= 2;
            if (w->pBoss && (w->pBoss->flag & 0x100)) {
                ROPE(w)->be_flag &= ~2;
            }
        } else {
            ROPE(w)->be_flag &= ~2;
        }
    }
    if (w->pBoss && (w->pBoss->flag & 0x40)) {
        Vec v;
        Mtx m;
        u32 i;

        w->swayAmp.x = PI / 16;
        w->swayAmp.y = 0.0f;
        w->swayAmp.z = PI / 16;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 100.0f;
        PSMTXRotRad(m, 'y', GetXZAngle(&w->pBoss->pos, &pos));
        PSMTXMultVecSR(m, &v, &v);
        SndCall(8, 0x17, &pos, 0xF, 0, 0);
        for (i = 0; i < 2; i++) {
            w->node[i].spd = v;
        }
    }
}

// Tiller input of the frame from the stick (player state 2): Tiller bit0 forward, bit1 back, bit2
// left, bit3 right; consumed by pl0fBoatSpdControl.
void cPl0f::setTiller()
{
    Pl0fWork* w = PL0F_WK(this);

    if (Key.on & 1) {
        w->Tiller |= 1;
    }
    if (Key.on & 2) {
        w->Tiller |= 2;
    }
    if (Key.on & 8) {
        w->Tiller |= 4;
    }
    if (Key.on & 4) {
        w->Tiller |= 8;
    }
}

// Room script: full ahead this frame.
void cPl0f::setTillerFront()
{
    Pl0fWork* w = PL0F_WK(this);

    w->Tiller |= 1;
}

// Room script: places the boat at `p` facing `ang` (level, both nodes stopped), rebuilds its
// matrices and kills the wake effects (group 0x35) of the old position.
void cPl0f::setPos(Vec* p, f32 ang)
{
    Pl0fWork* w = PL0F_WK(this);
    u32 i;

    for (i = 0; i < 2; i++) {
        Pl0fNode* n = &w->node[i];

        n->spd.x = 0.0f;
        n->spd.y = 0.0f;
        n->spd.z = 0.0f;
    }
    pos = *p;
    pos_old = pos;
    this->ang.y = ang;
    this->ang.x = 0.0f;
    this->ang.z = 0.0f;
    RotMatrix(mat, &this->ang);
    TransMatrix(mat, &pos);
    partsMatCalc();
    partsWorldCalc();
    EffectEspDelete(0, ESP_CORE_KIND_BOAT, this, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_BOAT, this);
    EffectEfmDelete(0, ESP_CORE_KIND_BOAT, this);
}

// r_no_0 == 0: creation. Loads the boat model (archive 5/6) with a 2 m light area, no IK / lock-on,
// atari priority 1, the effects (archive 4 as group 0xF); the nodes at z +2500 (bow) / -1500
// (stern) with their rest distance and a 25 m leash; zeroes the work; a back light when the
// list flag is negative; the idle wave effect outside 10D / 10E; the anchor and the long rope;
// then the start state from type / set (0: wait or ride start; 1: R10d in; 2: R10e in; 3: type
// 2 -> wait; 4: R10e in 2; 5: type 4 -> wait) and runs it this frame.
static void pl0f_R0_Init(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    u32 i;
    u32 j;

    em->modelInit(ARC(0x5), ARC(0x6));
    em->be_flag &= ~0x10;
    em->ot_type = 0;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 4);
    }
    // A local for the 0: an SImode zero, not merged with x12F's QImode zero across the init2 call above.
    int n = 0;
    em->lockParts = n;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_IK_OFF);
    em->atari.m_flag &= 0xFCFF;
    em->atari.setPriority(PRI_LV1);
    em->setStatus(EM_STATUS_LOCKOFF);
    EspDataLoad((u32) ARC(0x4), EFF_PL0F, 0);
    w->node[0].pos.x = 0.0f;
    w->node[0].pos.y = 0.0f;
    w->node[0].pos.z = 2500.0f;
    w->node[1].pos.x = 0.0f;
    w->node[1].pos.y = 0.0f;
    w->node[1].pos.z = -1500.0f;
    for (i = 0; i < 2; i++) {
        Pl0fNode* n = &w->node[i];

        n->dist[i] = 0.0f;
        n->maxLen = 25000.0f;
        n->spd.x = 0.0f;
        n->spd.y = 0.0f;
        n->spd.z = 0.0f;
        for (j = i + 1; j < 2; j++) {
            Pl0fNode* m = &w->node[j];
            f32 len;

            len = VEC_DIST(&n->pos, &m->pos);
            n->dist[j] = len;
            m->dist[i] = len;
        }
    }
    w->Ripple_wait = 0x1D;
    w->Be_flg = 0;
    w->Roll_rot = 0.0f;
    w->Bank_rot = 0.0f;
    w->Roll_sin = 0.0f;
    w->Bank_sin = 0.0f;
    w->Vib_sin = 0.0f;
    w->Boss_chase = 0;
    w->Tiller = 0;
    w->First_camck = 0;
    w->Cursor_type = 0;
    w->swayAmp.x = 0.0f;
    w->swayAmp.y = 0.0f;
    w->swayAmp.z = 0.0f;
    w->swayPhase.x = 0.0f;
    w->swayPhase.y = 0.0f;
    w->Seid_engine = 0;
    w->swayPhase.z = 0.0f;   // last 0.0 store (dying register): issued first
    if (em->flag & 0x80000000) {
        LightMgr.createBack(0, 3, 0, 0)->setParent(em);
    }
    w->EffKindId = EspPullCoreKind();
    if (pG->room_id != 0x10D && pG->room_id != 0x10E) {
        EstSet(em, -1, 0, 0, 0xF, 0xF, 0x800, 0, em, 0);
    }
    pl0fSetAnchor(em);
    pl0fLongRopeSet(em);
    switch (em->type) {
    case 0:
    default:
        switch (em->set) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0, 0, 0);
            break;
        case 1:
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        break;
    case 1:
        EmRoutineSet(em, 1, 8, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 1, 0xA, 0, 0);
        break;
    case 3:
        em->type = 2;
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 4:
        EmRoutineSet(em, 1, 0xC, 0, 0);
        break;
    case 5:
        em->type = 4;
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    }
    pl0f_R0_Move(em);
}

// r_no_0 == 1: spins the propeller (parts 2, 60 degrees per frame) while moving, runs the r_no_1
// state and records the boss position into the 10-entry history ring.
static void pl0f_R0_Move(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    if (w->Boat_spd > 30.0f) {
        cModel* p = em->getPartsPtr(2);

        p->ang.z += 1.0471976f;
        p->ang.z = LIMIT_ANGLE(p->ang.z);
    }
    Pl0f_R1_move_tbl[em->r_no_1](em);
    if (w->pBoss) {
        w->Boss_pos[w->Boss_pos_no] = w->pBoss->pos;
        w->Boss_pos_no++;
        if (w->Boss_pos_no > 9) {
            w->Boss_pos_no = 0;
        }
    }
}

// Engine stop SE: the long one after a minute of running.
static inline void pl0fEngineStop(Pl0fWork* w, Vec* pos)
{
    w->Be_flg &= ~2;
    if (w->Sailing_timer > 60) {
        SndCall(8, 0xB, pos, 0xF, 0, 0);
    } else {
        SndCall(8, 0x10, pos, 0xF, 0, 0);
    }
}

// r_no_1 == 0: moored / waiting: stops a running engine, no boss chase, the hull physics and the
// boarding action button check.
static void pl0f_R1_Wait(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    if (w->Be_flg & 2) {
        w->Be_flg &= ~2;
        if (w->Sailing_timer > 60) {
            SndCall(8, 0xB, &em->pos, 0xF, 0, 0);
        } else {
            SndCall(8, 0x10, &em->pos, 0xF, 0, 0);
        }
        w->Sailing_timer = 0;
    }
    w->Boss_chase = 0;
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    pl0fRideActEvtCk(em);
}

// The boss is close in front: the player guards (routine 0/F/7), the boat 1/7.
#define BOSS_NEAR(boss, em) \
    ((boss) && ((boss)->flag & 0x10) && ((boss)->pos.x - (em)->pos.x) * ((boss)->pos.x - (em)->pos.x) + ((boss)->pos.z - (em)->pos.z) * ((boss)->pos.z - (em)->pos.z) < 9.0e8f)

// r_no_1 == 1: the player steers freely: tiller -> speed, hull physics, the player's control flags;
// the boss surfacing close (BOSS_NEAR: flag bit4 within 30 m) -> guard (player state 7, boat 7),
// a crash into the boss / an island -> guard 3.
static void pl0f_R1_RideMove(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->Boss_chase = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    ((cPlayer*) em)->checkCtrl();
    em->partsMatCalc();
    em->partsWorldCalc();
    if (BOSS_NEAR(w->pBoss, em)) {
        EmRoutineSet(pPL, 0, 0xF, 7, 0);
        EmRoutineSet(em, 1, 7, 0, 0);
    } else if (pl0fCrashCk(em)) {
        EmRoutineSet(pPL, 0, 0xF, 7, 0);
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

// r_no_1 == 2 (set 1: the player starts aboard): the tiller hand model, the weapon hidden, the
// player into routine 0xF state 2 (steer) with PlBoatMove installed, the engine started, the
// partner into subBoatRide step 2 (already seated), then -> ride (1).
static void pl0f_R1_RideStart(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    cPlayer* pl = pPL;

    pl->Body->initWepHand((u32) ARC(0x8));
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    pl->m_pBoat = em;
    BoatMoveFunc = PlBoatMove;
    EmRoutineSet(pPL, 0, 0xF, 2, 0);
    w->Be_flg |= 1;
    w->Seid_engine = SndCall(8, 0x11, &em->pos, 0xF, 0, 0);
    if (pSUB) {
        SetSubDamage(em, subBoatRide);
        pSUB->r_no_2 = 2;
    }
    EmRoutineSet(em, 1, 1, 0, 0);
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// r_no_1 == 6: dragged by Del Lago (setBossStart / after the climb-in): the bow is leashed to the
// boss (pl0fBoatChaseBoss), tiller and hull physics still run. The boss dying -> ride (1) with
// the boss forgotten. After a 30-frame grace: the boss surfacing close -> guard 7; a crash while
// fast (> 200) or during the boss's ram (flag bit2) -> the player is thrown in the water (player
// state 5, harpoon lost, boat 4), a slow crash -> guard 7.
static void pl0f_R1_BossMove(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->Boss_chase = 1;
    pl0fBoatChaseBoss(em);
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    if (w->pBoss && (s16) w->pBoss->hp <= 0) {
        em->r_no_0 = 1;   // plain byte stores: the QImode 1 is the `bossMode = 1` pseudo (r30) kept across the calls
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        {
            int zero = 0;   // one SImode zero for both stores, separate from the routine's QImode zero

            w->pBoss = (cEm*) zero;
            w->Boss_chase = zero;
        }
        return;
    }
    switch (em->r_no_2) {
    case 0:
        w->Timer = 30;
        em->r_no_2++;
    case 1: {
        if (BOSS_NEAR(w->pBoss, em)) {
            EmRoutineSet(pPL, 0, 0xF, 7, 0);
            EmRoutineSet(em, 1, 7, 0, 0);
        } else if (w->Timer) {
            w->Timer--;
        } else if (w->pBoss && pl0fCrashCk(em)) {
            if (w->Boat_spd > 200.0f || (w->pBoss->flag & 4)) {
                EmRoutineSet(pPL, 0, 0xF, 5, 0);
                {
                    cPlayer* pl = pPL;   // second pPL load (the byte stores above alias it), kept across setLost

                    if (pl->m_pSpear) {
                        pl->m_pSpear->setLost();
                        pl->m_pSpear = 0;
                    }
                }
                EmRoutineSet(em, 1, 4, 0, 0);
                w->Boss_chase = 1;
            } else {
                EmRoutineSet(pPL, 0, 0xF, 7, 0);
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        }
        break;
    }
    }
}

// r_no_1 == 3: the crash shake (motion 0x1D, impact SE, vibration) during free riding; back to
// ride (1) at its end; the boss close / another crash re-trigger the guards.
static void pl0f_R1_Guard(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x1D), 0, 0, 5, 0);
        SndCall(8, 0x17, &em->pos, 0xF, 0, 0);
        VibSetData(VIB_TBL, 0xB, 1);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    if (BOSS_NEAR(w->pBoss, em)) {
        EmRoutineSet(pPL, 0, 0xF, 7, 0);
        EmRoutineSet(em, 1, 7, 0, 0);
    } else if (pl0fCrashCk(em)) {
        EmRoutineSet(pPL, 0, 0xF, 7, 0);
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

// r_no_1 == 4: the boat capsizes and throws the player out: the drop motion 0x1F with the splash
// effect, 100 damage to the boat itself (its hp is the fight's boat damage), the tiller reset,
// and for 60 frames both nodes are shoved along their current direction (300..450 units by the
// boat's remaining hp); crash / drop checks are off (Be_flg bits 2/3); ends in wait (0).
static void pl0f_R1_Drop(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->Be_flg |= 0xC;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x1F), 0, 0, 5, 0);
        EstSet(em, -1, 0, 0, 0xF, 0xB, 0, 0x35, em, 0);
        LifeDownSet2(em, 100, 0, 1);
        em->getPartsPtr(1)->ang.y = 0.0f;
        w->Timer = 60;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else if (w->Timer) {
            f32 spd;
            u32 i;

            w->Timer--;
            spd = 300.0f;
            if ((s16) em->hp <= 699) {
                spd = 350.0f;
            }
            if ((s16) em->hp <= 399) {
                spd = 400.0f;
            }
            if ((s16) em->hp <= 0) {
                spd = 450.0f;
            }
            for (i = 0; i < 2; i++) {
                Vec d;

                d = w->node[i].spd;
                if (d.x != 0.0f && d.y != 0.0f && d.z != 0.0f) {
                    d.x = 0.0f;
                    d.y = 0.0f;
                    d.z = -1.0f;
                    PSMTXMultVecSR(em->mat, &d, &d);
                }
#line 806
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, spd);
                w->node[i].spd = d;
            }
        }
        break;
    }
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// r_no_1 == 5: the player climbs back in (set by the swim state within 1.8 m): the rocking motion
// 0x22 with a splash effect and SE, crash checks off; then -> dragged by the boss (6).
static void pl0f_R1_WaterRide(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->Be_flg |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x22), 0, 0, 5, 0);
        EstSet(em, -1, 0, 0, 0xF, 0xC, 0, 0x35, em, 0);
        SndCall(8, 0x17, &em->pos, 0xF, 0, 0);
        w->Timer = 60;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// r_no_1 == 7: the crash shake (motion 0x1D) while dragged by the boss (leash and tiller keep
// running); back to dragged (6) at its end.
static void pl0f_R1_BossGuard(cPl0f* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x1D), 0, 0, 5, 0);
        SndCall(8, 0x17, &em->pos, 0xF, 0, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
    pl0fBoatChaseBoss(em);
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Entrance of rooms 10D / 10E: the boat drives itself in and the player gets off at the landing.
// Macros, not inlines: an inline's constant arguments (the partner routine address, the four f32 room
// coordinates) are expanded at the call's head, i.e. before the switch / the timer2 test.
#define PL0F_ROOM_IN(plRoutine, m_pFunc, t1, t2) \
{ \
    cPlayer* pl = pPL; \
 \
    switch (em->r_no_2) { \
    case 0: \
        (pl->m_pBoat = em); \
        BoatMoveFunc = PlBoatMove; \
        EmRoutineSet(pPL, 0, 0xF, plRoutine, 0); \
        if (pSUB) { \
            SetSubDamage(em, m_pFunc); \
        } \
        w->Timer = t1; \
        w->Timer2 = t2; \
        w->Timer3 = 0; \
        em->r_no_2++; \
    case 1: \
        w->Timer3++; \
        if (w->Timer3 & 1) { \
            EstSet(em, -1, 0, 0, 1, 0xA, 0, 0x35, em, 0); \
        } \
        if (w->Timer) { \
            w->Timer--; \
            w->Tiller |= 1; \
        } \
        break; \
    } \
    pl0fBoatSpdControl(em); \
    pl0fBoatControl(em); \
    em->partsMatCalc(); \
    em->partsWorldCalc(); \
}

#define PL0F_ROOM_IN_END(ang, px, py, pz) \
{ \
    if (w->Timer2) { \
        w->Timer2--; \
    } else { \
        cPlayer* pl = pPL; \
 \
        pl->m_Fwork0 = ang; \
        pl->m_VecWork0.x = px; \
        pl->m_VecWork0.y = py; \
        pl->m_VecWork0.z = pz; \
        EmRoutineSet(pl, 0, 0xF, 1, 0); \
        EmRoutineSet(em, 1, 0, 0, 0); \
        PL0F_WK(em)->Be_flg &= ~1; \
        SndStop(PL0F_WK(em)->Seid_engine, 0); \
        if (pSUB) { \
            SetSubDamage(em, subBoatGetoff); \
        } \
    } \
}

// r_no_1 == 8 (type 1): the room 10D entrance: the player (state 0xD) and partner ride in, full
// ahead for 45 frames, then after 75 frames the player gets off at the 10D landing (state 1) and
// the boat waits.
static void pl0f_R1_R10dIn(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    PL0F_ROOM_IN(0xD, subBoatR10dIn, 45, 75);
    PL0F_ROOM_IN_END(1.47f, -500.0f, -2280.0f, -16870.0f);
}

// r_no_1 == 9: leaving room 10D: steered by the player's exit state (0xE), no boss chase.
static void pl0f_R1_R10dOut(cPl0f* em)
{
    PL0F_WK(em)->Boss_chase = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// r_no_1 == 10 (type 2): the first room 10E entrance (player state 0xF): 65 frames ahead, off at
// the landing after 95.
static void pl0f_R1_R10eIn(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    PL0F_ROOM_IN(0xF, subBoatR10eIn, 65, 95);
    PL0F_ROOM_IN_END(1.568879f, 38250.0f, -15000.0f, 52360.0f);
}

// r_no_1 == 11: leaving room 10E (player state 0x10).
static void pl0f_R1_R10eOut(cPl0f* em)
{
    PL0F_WK(em)->Boss_chase = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// r_no_1 == 12 (type 4): the second room 10E entrance (player state 0x11), the other landing.
static void pl0f_R1_R10eIn2(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    PL0F_ROOM_IN(0x11, subBoatR10eIn2, 65, 95);
    PL0F_ROOM_IN_END(1.57f, -46610.0f, -15000.0f, 39280.0f);
}

// r_no_1 == 13: leaving room 10E the second time (player state 0x12).
static void pl0f_R1_R10eOut2(cPl0f* em)
{
    PL0F_WK(em)->Boss_chase = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Pulls a node back inside maxLen of its fixPos (x / z only). A macro on the routine's `d`, `n` and
// `len`: the PSVEC* `&d` arguments are one gcse-PRE'd pseudo per block (`mr rX,r23` copies hoisted
// out of the loops) while the member reads stay frame-direct (an inline taking `Vec*` substitutes
// `&d` into every use; called with a pointer local it keeps `4(rP)`/`8(rP)` reads); the squared
// length goes through the routine's `len` (a global pseudo: f12 by global-alloc, not tied to the
// dying `fmuls` result).
#define PL0F_NODE_LIMIT(n, line)                                                                   \
    PSVECSubtract(&(n)->wpos, &(n)->fixPos, &d);                                                   \
    len = d.x * d.x + d.z * d.z;                                                                   \
    if (len > (n)->maxLen * (n)->maxLen) {                                                         \
        if (0.0f == d.x && 0.0f == d.y && 0.0f == d.z) {                                           \
            pLog->err(0, 0, "VECNormalize:[%s/%d]", "D:/Bio4/Prog/pl0f.cpp", line);              \
            d.x = d.y = d.z = 0.0f;                                                                \
        } else {                                                                                   \
            PSVECNormalize(&d, &d);                                                                \
        }                                                                                          \
        d.y = 0.0f;                                                                                \
        PSVECScale(&d, &d, (n)->maxLen);                                                           \
        PSVECAdd(&(n)->fixPos, &d, &(n)->wpos);                                                    \
    }

// The hull physics of the frame: each node moves by its speed (a leashed node is pulled back
// inside maxLen of fixPos), four relaxation passes restore the bow-stern distance, the speeds
// are re-derived and damped (pl0f_spd_damp 0.96), the nodes are pushed out of the scenery
// (pl0fScrAdjust); the boat's heading is the stern -> bow line and its position the bow node
// minus the bow offset. Then the movement direction, the wake effects, the roll / pitch bobbing,
// water ripples under the nodes when moving, and the tiller parts (1) follows the rider's lean
// while the player sits (stat 0x0F02xx).
void pl0fBoatControl(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec d;
    u32 i;
    u32 k;
    u32 pass;
    f32 len;
    Pl0fNode* n;

    // `i` and `n` are the SAME variables in all three node loops: with `i` set in the first loop,
    // gcse does not PRE the second loop's `i + 1` across the k loop (a fresh `u32 j` gets `r = j + 1`
    // hoisted into the k preheader, so `j` is no biv and `&node[j]` a `mulli`); with `n` first
    // mentioned in the first loop it is not `replaceable` in the second, so at the `if (n->fixed)`
    // jump loop.c marks it `cant_derive` (the biv is `maybe_multiple` behind the k loop's back
    // edge) and `&n->wpos` / `&n->fixPos` / `n->dist` stay displacements off the `mr r31,r25` copy
    // instead of becoming their own stepping pointers.
    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    for (i = 0; i < 2; i++) {
        n = &w->node[i];
        PSMTXMultVec(m, &n->pos, &n->wpos);
        n->wposOld = n->wpos;
        PSVECAdd(&n->wpos, &n->spd, &n->wpos);
        if (n->fixed) {
            PL0F_NODE_LIMIT(n, 1205);
        }
    }
    for (pass = 0; pass < 4; pass++) {
        for (i = 0; i < 2; i++) {
            n = &w->node[i];
            if (n->fixed) {
                PL0F_NODE_LIMIT(n, 1226);
            }
            for (k = 0; k < 2; k++) {
                if (i != k) {
                    Pl0fNode* o = &w->node[k];
                    f32 rate;

                    PSVECSubtract(&o->wpos, &n->wpos, &d);
                    len = PSVECMag(&d);   // the routine's `len`: f12 (`fmr f12,f1`), not tied to f1
                    rate = (n->dist[k] - len) * 0.5f;   // 0.5 enters the pool before 1.0
                    PSVECScale(&d, &d, (1.0f / len) * rate);   // 1/len is the left operand of the fmuls
                    PSVECAdd(&o->wpos, &d, &o->wpos);
                    PSVECSubtract(&n->wpos, &d, &n->wpos);
                }
            }
        }
    }
    for (i = 0; i < 2; i++) {
        n = &w->node[i];
        n->fixed = 0;
        PSVECSubtract(&n->wpos, &n->wposOld, &n->spd);
        PSVECScale(&n->spd, &n->spd, pl0f_spd_damp);
    }
    pl0fScrAdjust(em);
    PSVECSubtract(&w->node[0].wpos, &w->node[1].wpos, &d);
    em->ang.x = 0.0f;
    em->ang.y = atan2f(d.x, d.z);
    RotMatrix(em->mat, &em->ang);
    PSVECScale(&w->node[0].pos, &d, -1.0f);
    TransMatrix(em->mat, &w->node[0].wpos);
    PSMTXMultVec(em->mat, &d, &d);
    TransMatrix(em->mat, &d);
    em->pos = d;
    pl0fGetBoatDir(em);
    pl0fWaterEff(em);
    pl0fBoatRoll(em);
    if (w->Boat_spd > 100.0f) {
        AddWaterPower(w->node[0].wpos, fRand1_1() * 0.3f);
        AddWaterPower(w->node[1].wpos, fRand1_1() * 0.3f);
    }
    {
        cPlayer* pl = pPL;
        cModel* p = em->getPartsPtr(1);

        if (pPL->r_no_0 == 0 && pPL->r_no_1 == 0xF && pPL->r_no_2 == 2) {
            p->ang.y = pl->m_Blend * (1.0f / 255.0f) * -0.5235988f;
        } else {
            p->ang.y *= 0.9f;
        }
    }
}

// Movement of the frame: Boat_spd = the XZ distance moved since pos_old; faster than 100 units
// Boat_dir = the heading change towards the movement direction, else it decays by 0.9;
// Boat_rot = |Boat_dir|.
void pl0fGetBoatDir(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec d;

    PSVECSubtract(&em->pos, &em->pos_old, &d);
    w->Boat_spd = SQRTF(d.x * d.x + d.z * d.z);
    if (w->Boat_spd > 100.0f) {
        w->Boat_dir = atan2f(d.x, d.z);
        w->Boat_dir = Muku2(em->ang.y, w->Boat_dir, PI);
    } else {
        w->Boat_dir *= 0.9f;
    }
    w->Boat_rot = fabsf(w->Boat_dir);
}

// Wake / spray effects (group 0x35) on the lake only: a sideways skid spray (0x1D) when moving
// almost backwards, and above 150 units/frame (not while capsized): the turn spray 3 / 4 once per
// turn with SE 8/0xC, the bow wake (0) every 2nd frame, the side wakes (1, 2) when going forward,
// a splash (9) with SE every 20 frames, and while the player hides (Status_flg[1] bit23) the
// stopped-boat ripple (5) after 4 frames, then the restart splash (8).
void pl0fWaterEff(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    static u8 cnt = 0;
    static u8 turn = 0;
    static u8 hideCnt = 0;
    static u8 cnt3 = 0;
    f32 a;

    if (pG->room_id == 0x10D || pG->room_id == 0x10E) {
        return;
    }
    if (w->Boat_rot > 2.443461f && w->Boat_spd > 30.0f && !(pG->Frame_cnt & 3)) {
        EstSet(em, -1, 0, 0, 0xF, 0x1D, 0, 0x35, em, 0);
    }
    if (w->Boat_spd < 150.0f) {
        return;
    }
    if (w->Be_flg & 4) {
        return;
    }
    a = fabsf(w->Boat_dir);
    if (w->Boat_rot > PI / 16 && a < 2.0943952f) {
        if (turn == 0) {
            turn = 1;
            if (w->Boat_dir < 0.0f) {
                EstSet(em, -1, 0, 0, 0xF, 3, 0, 0x35, em, 0);
            } else {
                EstSet(em, -1, 0, 0, 0xF, 4, 0, 0x35, em, 0);
            }
            SndCall(8, 0xC, &em->pos, 0xF, 0, 0);
        }
    } else {
        turn = 0;
    }
    cnt++;
    if (cnt & 1) {
        EstSet(em, -1, 0, 0, 0xF, 0, 0, 0x35, em, 0);
    }
    if (a < 2.0943952f) {
        EstSet(em, -1, 0, 0, 0xF, 1, 0, 0x35, em, 0);
        EstSet(em, -1, 0, 0, 0xF, 2, 0, 0x35, em, 0);
    }
    cnt3++;
    if (cnt3 % 20 == 0) {
        EstSet(em, -1, 0, 0, 0xF, 9, 0, 0x35, em, 0);
        SndCall(8, 0x12, &em->pos, 0xF, 0, 0);
    }
    if (StaFlagChk(pG, STA_PL_SPEAR_SET)) {
        if (hideCnt <= 4) {
            hideCnt++;
            return;
        }
        EstSet(em, -1, 0, 0, 0xF, 5, 0, 0x35, em, 0);
    } else {
        if (hideCnt != 0) {
            EstSet(em, -1, 0, 0, 0xF, 8, 0, 0x35, em, 0);
        }
        hideCnt = 0;
    }
}

// Adds the hull attitude to `mat`: a roll into the turn (Boat_dir * 0.3 scaled by the speed,
// smoothed 0.9/0.1), a bow-up pitch with the speed (up to -0.196 rad) plus a random wobble
// (Bank_sin), and the decaying impact sway (swayAmp * sin(swayPhase), amplitude * 0.96 per frame).
void pl0fBoatRoll(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec sway;
    f32 t;
    f32 a;

    if (w->Boat_rot < PI / 2) {
        t = w->Boat_spd * 0.01f;
        if (t > 1.0f) {
            t = 1.0f;
        }
        a = w->Boat_dir * 0.3f * t;
        w->Roll_rot = w->Roll_rot * 0.9f + a * 0.1f;
    } else {
        w->Roll_rot *= 0.9f;
    }
    w->Roll_sin += fRand0_1() * 0.3926991f + 0.09817477f;
    sinf(w->Roll_sin);
    PSMTXRotRad(m, 'z', w->Roll_rot);
    PSMTXConcat(em->mat, m, em->mat);
    if (w->Boat_rot < PI / 2) {
        a = w->Boat_spd * 0.005f;
        if (a > 1.0f) {
            a = 1.0f;
        }
        a *= -0.19634955f;
        w->Bank_rot = w->Bank_rot * 0.9f + a * 0.1f;
    } else {
        w->Bank_rot *= 0.9f;
    }
    a = w->Bank_rot;
    w->Bank_sin += fRand0_1() * 0.3926991f + 0.09817477f;
    PSMTXRotRad(m, 'x', sinf(w->Bank_sin) * 0.012271847f + a);
    PSMTXConcat(em->mat, m, em->mat);
    sway.x = w->swayAmp.x * sinf(w->swayPhase.x);
    sway.y = 0.0f;
    sway.z = w->swayAmp.z * sinf(w->swayPhase.z);
    PSVECScale(&w->swayAmp, &w->swayAmp, 0.96f);
    w->swayPhase.x += 0.31415927f;
    w->swayPhase.z += 0.34906587f;
    RotMatrix(m, &sway);
    PSMTXConcat(em->mat, m, em->mat);
}

// Adds a world-space speed to node `no` (0 bow, 1 stern).
void pl0fBoatAddSpd(cPl0f* em, u32 no, Vec* spd)
{
    Pl0fWork* w = PL0F_WK(em);

    if (no < 2) {
        PSVECAdd(&w->node[no].spd, spd, &w->node[no].spd);
    }
}

// Tiller -> engine: while the player hides (Status_flg[1] bit23) the engine only stops; else any
// tiller bit starts the engine SE (8/0xA) and counts Sailing_timer, none stops it. Forward adds
// 50 units/frame (20 on the ferry types 1..5) at the stern, back -15; left / right turn the
// thrust by PI/20 while going forward, or spin the boat (PI/8, PI/16 when the boss is hooked)
// with a 25 / 50 push, halved and reversed with back. The Tiller bits are consumed.
void pl0fBoatSpdControl(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec spd;
    Vec p;

    p = em->pos;
    p.y += 500.0f;
    if (StaFlagChk(pG, STA_PL_SPEAR_SET)) {
        if (w->Be_flg & 2) {
            pl0fEngineStop(w, &p);
        }
        w->Sailing_timer = 0;
        return;
    }
    if (w->Tiller & 0xF) {
        if (!(w->Be_flg & 2)) {
            w->Be_flg |= 2;
            SndCall(8, 0xA, &p, 0xF, 0, 0);
            w->Sailing_timer = 0;
        }
        w->Sailing_timer++;
    } else {
        if (w->Be_flg & 2) {
            pl0fEngineStop(w, &p);
        }
        w->Sailing_timer = 0;
    }
    spd.x = 0.0f;
    spd.y = 0.0f;
    spd.z = 0.0f;
    if (w->Tiller & 1) {
        switch (em->type) {   // range node: `cmpwi 5; bgt; cmpwi 1; bge` (an if-range folds to subi/cmplwi); default first
        default:
            spd.x = 0.0f;
            spd.y = 0.0f;
            spd.z = 50.0f;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            spd.x = 0.0f;
            spd.y = 0.0f;
            spd.z = 20.0f;
            break;
        }
    }
    if (w->Tiller & 2) {
        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = -15.0f;
    }
    if (w->Tiller & 0xC) {
        if (w->Tiller & 1) {
            if (w->Tiller & 4) {
                w->Rot_spd = -PI / 20;
            }
            if (w->Tiller & 8) {
                w->Rot_spd = PI / 20;
            }
        } else {
            if (w->pBoss) {
                if (w->Tiller & 4) {
                    w->Rot_spd = -PI / 16;
                }
                if (w->Tiller & 8) {
                    w->Rot_spd = PI / 16;
                }
                spd.x = 0.0f;
                spd.y = 0.0f;
                spd.z = 50.0f;
            } else {
                if (w->Tiller & 4) {
                    w->Rot_spd = -PI / 8;
                }
                if (w->Tiller & 8) {
                    w->Rot_spd = PI / 8;
                }
                spd.x = 0.0f;
                spd.y = 0.0f;
                spd.z = 25.0f;
            }
            if (w->Tiller & 2) {
                spd.z *= -0.5f;
            }
        }
    } else {
        w->Rot_spd = 0.0f;
    }
    PSMTXRotRad(m, 'y', em->ang.y + w->Rot_spd);
    PSMTXMultVecSR(m, &spd, &spd);
    pl0fBoatAddSpd(em, 1, &spd);
    w->Tiller = 0;
}

// The anchor rope: leashes the bow node to a point 2 m behind the boss; while the boss dives (flag
// bit3) the leash is slack (500 m), else it shortens to the current distance when that is between
// 25 m and the leash, otherwise relaxes by 3 % + 750 units per frame.
void pl0fBoatChaseBoss(cPl0f* em)
{
    cEm* boss = PL0F_WK(em)->pBoss;   // no work pointer: pBoss folds into em+0x3F0
    Pl0fNode* n;
    Vec v;
    Vec b;

    if (boss == 0) {
        return;
    }
    n = &PL0F_WK(em)->node[0];   // assigned after the early return: the addi is issued with the first block's stores
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = -2000.0f;
    PSMTXMultVec(boss->mat, &v, &v);
    v.y = n->wpos.y;
    n->fixed = 1;
    n->fixPos = v;
    if (boss->flag & 8) {
        n->maxLen = 500000.0f;
    } else {
        f32 len;

        b.x = 0.0f;
        b.y = 0.0f;
        b.z = -2000.0f;
        PSMTXMultVec(boss->mat, &b, &b);
        len = VEC_DISTXZ(&n->wpos, &b);
        if (len < n->maxLen && len > 25000.0f) {
            n->maxLen = len;
        } else {
            n->maxLen = n->maxLen * 0.97f + 750.0f;
        }
    }
}

// Camera distance from the position / target pair, then the orientation.
#define CAM_SET(cam)                                                                                              \
    {                                                                                                             \
        Vec* cp = &(cam).param.pos;                                                                               \
        Vec* ca = &(cam).param.at;                                                                                \
                                                                                                                  \
        (cam).Distance = VEC_DIST(cp, ca); \
    }                                                                                                             \
    CameraSetOrientationUp(&(cam))

static Vec pl0f_ride_cam_ofs = { -1000.0f, 1500.0f, -5000.0f };

// Ferry ride camera (rooms 10D / 10E): 5 m behind, 1.5 m up and 1 m left of the boat, looking at
// the player, blended in at `rate` (1.0 = snap, 0.3 = follow); fovy 40.
void pl0fRideCamMove(cPl0f* em, f32 rate)
{
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec pos;
    Vec at;

    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &pl0f_ride_cam_ofs, &pos);
    at = pPL->getPartsPtr(0)->world;
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &at, &pl0f_camera.param.at, rate);
    PosToPos(&gcam->param.pos, &pos, &pl0f_camera.param.pos, rate);
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

static Vec pl0f_getoff_cam_ofs = { -1000.0f, 1500.0f, -5000.0f };

// Get-off camera: the same offset as the ride camera, snapped every frame.
void pl0fGetoffCamMove(cPl0f* em)
{
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec pos;
    Vec at;

    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &pl0f_getoff_cam_ofs, &pos);
    at = pPL->getPartsPtr(0)->world;
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &at, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &pos, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

static Vec pl0f_boss_cam_ofs = { -1200.0f, 1400.0f, 0.0f };
static f32 pl0f_boss_cam_y = -500.0f;
static f32 pl0f_boss_cam_up = 1400.0f;
static f32 pl0f_boss_cam_dist = 5000.0f;
static f32 pl0f_boss_cam_y2 = -1000.0f;
static f32 pl0f_boss_cam_up2 = 1600.0f;
static f32 pl0f_boss_cam_dist2 = 1800.0f;
// Struct view of a static f32: the load is MEM_IN_STRUCT_P and stays below the preceding `cat = bpos`
// struct-copy stores (a plain scalar load never aliases a varying struct store and floats above them).
struct Pl0fF32V { f32 f; };
#define PL0F_F32S(x) (((Pl0fF32V*) &(x))->f)
static Vec pl0f_boss_cam_at_ofs = { -400.0f, 0.0f, 0.0f };
static Vec pl0f_boss_cam_pos0 = { -1000.0f, 1500.0f, -5000.0f };
static Vec pl0f_boss_cam_at0 = { 0.0f, 1000.0f, 5000.0f };
static Vec pl0f_boss_cam_pos1 = { -500.0f, 1900.0f, -1500.0f };
static Vec pl0f_boss_cam_at1 = { 0.0f, 1500.0f, 5000.0f };

// The lake fight camera (skipped while the player has stat bit2). With the boss hooked
// (Boss_chase): `hide` (the player ducks) looks from 1.8 m behind the boat (within +-45 degrees
// of its heading, 1.6 m up) at the boss; otherwise it sits 5 m behind the boat on the line away
// from the boss position of 10 frames ago (the history ring), 1.4 m up, looking at the midpoint
// between boat and boss with the pitch clamped to +-15 degrees. Without the boss a fixed
// behind-the-boat camera (pos0 / at0, or pos1 / at1 while hiding). fovy relaxes to 30 / 40.
void pl0fBossCamMove(cPl0f* em, int hide)
{
    Pl0fWork* w = PL0F_WK(em);
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec bpos;
    Vec cpos;
    Vec cat;
    Vec d;
    Vec ang;
    Vec d2;
    f32 len;

    cEm* boss = w->pBoss;   // read before the flags test (the original loads pBoss above the `andi.`)

    if (pPL->stat & 4) {
        return;
    }
    if (boss && w->Boss_chase) {
        if (hide) {
            bpos = boss->pos;
            bpos.y = em->pos.y + 1300.0f;
            cpos = pl0f_boss_cam_at_ofs;
            PSMTXMultVec(em->mat, &cpos, &cpos);
            PSVECSubtract(&bpos, &cpos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            ang.x = -atan2f(d.y, len);
            ang.y = atan2f(d.x, d.z);
            ang.z = 0.0f;
            ang.y = Muku2(em->ang.y, ang.y, PI / 4) + em->ang.y;
            RotMatrix(m, &ang);
            TransMatrix(m, &cpos);
            d.z = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
            d.x = 0.0f;
            d.y = 0.0f;
            PSMTXMultVec(m, &d, &bpos);
            cpos.y += pl0f_boss_cam_up2;
            PSVECSubtract(&cpos, &bpos, &d);
#line 1838
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, pl0f_boss_cam_dist2);
            PSVECAdd(&cpos, &d, &d);
            if (d.y < em->pos.y + pl0f_boss_cam_up2) {
                d.y = em->pos.y + pl0f_boss_cam_up2;
            }
            cpos = d;
            cat = bpos;
            if (cat.y < em->pos.y + PL0F_F32S(pl0f_boss_cam_y2)) {
                cat.y = em->pos.y + PL0F_F32S(pl0f_boss_cam_y2);
            }
            PSVECSubtract(&cat, &cpos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            ang.x = -atan2f(d.y, len);
            ang.y = atan2f(d.x, d.z);
            ang.z = 0.0f;
            if (ang.x > 0.2617994f) {
                ang.x = 0.2617994f;
            }
            if (ang.x < -0.2617994f) {
                ang.x = -0.2617994f;
            }
            RotMatrix(m, &ang);
            TransMatrix(m, &cpos);
            len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = len;
            PSMTXMultVec(m, &d, &cat);
            pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 3.0f;
        } else {
            PSMTXRotRad(m, 'y', em->ang.y);
            TransMatrix(m, &em->pos);
            PSMTXMultVec(m, &pl0f_boss_cam_ofs, &cpos);
            bpos = w->Boss_pos[w->Boss_pos_no];
            bpos.y = em->pos.y + pl0f_boss_cam_y;
            PSVECSubtract(&cpos, &bpos, &d);
#line 1876
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, pl0f_boss_cam_dist);
            PSVECAdd(&cpos, &d, &d);
            if (d.y < em->pos.y + pl0f_boss_cam_up) {
                d.y = em->pos.y + pl0f_boss_cam_up;
            }
            cpos = d;
            PSVECAdd(&em->pos, &bpos, &d);
            PSVECScale(&d, &d, 0.5f);
            cat = d;
            pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
            PSVECSubtract(&cat, &cpos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            ang.x = -atan2f(d.y, len);
            ang.y = atan2f(d.x, d.z);
            ang.z = 0.0f;
            if (ang.x > 0.2617994f) {
                ang.x = 0.2617994f;
            }
            if (ang.x < -0.2617994f) {
                ang.x = -0.2617994f;
            }
            RotMatrix(m, &ang);
            TransMatrix(m, &cpos);
            len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = len;
            PSMTXMultVec(m, &d, &cat);
        }
        PosToPos(&gcam->param.at, &cat, &pl0f_camera.param.at, 0.1f);
        PosToPos(&gcam->param.pos, &cpos, &pl0f_camera.param.pos, 0.5f);
    } else {
        PSMTXRotRad(m, 'y', em->ang.y);
        TransMatrix(m, &em->pos);
        if (hide) {
            PSMTXMultVec(m, &pl0f_boss_cam_pos1, &cpos);
            PSMTXMultVec(m, &pl0f_boss_cam_at1, &cat);
        } else {
            PSMTXMultVec(m, &pl0f_boss_cam_pos0, &cpos);
            PSMTXMultVec(m, &pl0f_boss_cam_at0, &cat);
        }
        pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
        PosToPos(&gcam->param.at, &cat, &pl0f_camera.param.at, 0.3f);
        PosToPos(&gcam->param.pos, &cpos, &pl0f_camera.param.pos, 0.5f);
        PSVECSubtract(&pl0f_camera.param.at, &pl0f_camera.param.pos, &d2);
#line 1929
        VECNormalize(&d2, &d2);
        PSVECScale(&d2, &d2, 1500.0f);
        PSVECAdd(&pl0f_camera.param.pos, &d2, &d2);
        EatMgr.adjust(0, &d2, &pl0f_camera.param.pos, 500.0f, 0x2001, 0);
    }
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

static Vec pl0f_hide_cam_at = { -500.0f, 1850.0f, -1800.0f };
static Vec pl0f_hide_cam_pos = { 0.0f, -200.0f, 10000.0f };

// Hiding-mode camera set-up (plboat_R2_SpearSet2 step 0): from 1.85 m up behind the player's
// shoulder looking 10 m ahead of the boat, snapped (the at / pos offsets are used swapped).
void pl0fHideModeCamSet(cPlayer* pl)
{
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->m_pBoat->ang.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_hide_cam_at, &at);
    PSMTXMultVec(m, &pl0f_hide_cam_pos, &pos);
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

// Hiding-mode camera per frame: the same placement followed at 10 % per frame, handed to CamCtrl.
void pl0fHideModeCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->m_pBoat->ang.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_hide_cam_at, &at);
    PSMTXMultVec(m, &pl0f_hide_cam_pos, &pos);
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 0.1f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 0.1f);
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

static Vec pl0f_die_cam_at = { -300.0f, 1700.0f, -2500.0f };
static Vec pl0f_die_cam_pos = { 0.0f, 1600.0f, 10000.0f };

// Boss-death camera set-up (plboat_R2_BossDie step 0): from behind the player's shoulder (1.7 m
// up, 2.5 m back) looking 10 m ahead of the boat, fovy 40, snapped.
void pl0fBossDieCamSet(cPlayer* pl)
{
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->m_pBoat->ang.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_die_cam_at, &at);
    PSMTXMultVec(m, &pl0f_die_cam_pos, &pos);
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

// Boss-death camera per frame: the same placement, snapped and handed to CamCtrl.
void pl0fBossDieCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Camera;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->m_pBoat->ang.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_die_cam_at, &at);
    PSMTXMultVec(m, &pl0f_die_cam_pos, &pos);
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

// Boarding action button: while the player is on foot (Status_flg[1] bit21 clear), faces the
// boat within 45 degrees, is within 3 m and 10 m in height, offers action 0x23 whose callback is
// the pl0fActRide* of the boat type (lake / 10D / 10E / 10E second).
void pl0fRideActEvtCk(cPl0f* em)
{
    if (StaFlagChk(pG, STA_PL_BOAT)) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI)) > PI / 4) {
        return;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 10000.0f) {
        return;
    }
    if (em->l_pl > 9000000.0f) {
        return;
    }
    switch (em->type) {
    case 0:
    default:
        ActBtn.set(ACT_RIDE, 5, (void*) pl0fActRide, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    case 1:
        ActBtn.set(ACT_RIDE, 5, (void*) pl0fActRideR10d, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    case 2:
    case 3:
        ActBtn.set(ACT_RIDE, 5, (void*) pl0fActRideR10e, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    case 4:
    case 5:
        ActBtn.set(ACT_RIDE, 5, (void*) pl0fActRideR10e2, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        break;
    }
}

static Vec pl0f_getoff_ck[3] = {
    { -48000.0f, -1300.0f, 22350.0f },
    { 124900.0f, -1300.0f, 148110.0f },
    { 0.0f, 0.0f, 0.0f },
};
static Vec pl0f_getoff_land[3] = {
    { -48810.0f, -1300.0f, 24780.0f },
    { 127560.0f, -1300.0f, 149100.0f },
    { 0.0f, 0.0f, 0.0f },
};
static f32 pl0f_getoff_ang[3] = { 1.466677f, 3.089821f, 0.0f };

// Get-off action button (rooms 10B / 11B, while aboard): near one of the two landings (7 m) the
// landing position / heading go to Getoff_pos / Getoff_dir and action 0x24 (pl0fActGetOff) is offered.
void pl0fGetoffActEvtCk(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    u32 i;

    if (!StaFlagChk(pG, STA_PL_BOAT)) {
        return;
    }
    if ((pG->stage_no != 1 || pG->room_no != 0xB) && (pG->stage_no != 1 || pG->room_no != 0x1B)) {
        return;
    }
    for (i = 0; i < 2; i++) {
        if ((pPL->pos.x - pl0f_getoff_ck[i].x) * (pPL->pos.x - pl0f_getoff_ck[i].x) + (pPL->pos.z - pl0f_getoff_ck[i].z) * (pPL->pos.z - pl0f_getoff_ck[i].z) < 4.9e7f) {
            w->Getoff_dir = pl0f_getoff_ang[i];
            w->Getoff_pos = pl0f_getoff_land[i];
            ActBtn.set(ACT_GET_OFF, 5, (void*) pl0fActGetOff, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        }
    }
}

// Action callbacks: the player boards (m_pBoat, PlBoatMove installed, routine 0xF at the boarding
// state of the boat type) and the boat goes to its ride state; the partner into subBoatRide.
// Lake boat: player state 0, boat ride (1).
static void pl0fActRide(cPl0f* em)
{
    pPL->m_pBoat = em;
    BoatMoveFunc = PlBoatMove;
    EmRoutineSet(pPL, 0, 0xF, 0, 0);
    EmRoutineSet(em, 1, 1, 0, 0);
    if (pSUB) {
        SetSubDamage(em, subBoatRide);
    }
}

// Room 10D exit: player state 0xE, boat 9.
static void pl0fActRideR10d(cPl0f* em)
{
    pPL->m_pBoat = em;
    BoatMoveFunc = PlBoatMove;
    EmRoutineSet(pPL, 0, 0xF, 0xE, 0);
    EmRoutineSet(em, 1, 9, 0, 0);
    if (pSUB) {
        SetSubDamage(em, subBoatRide);
    }
}

// Room 10E first exit: player state 0x10, boat 11.
static void pl0fActRideR10e(cPl0f* em)
{
    pPL->m_pBoat = em;
    BoatMoveFunc = PlBoatMove;
    EmRoutineSet(pPL, 0, 0xF, 0x10, 0);
    EmRoutineSet(em, 1, 0xB, 0, 0);
    if (pSUB) {
        SetSubDamage(em, subBoatRide);
    }
}

// Room 10E second exit: player state 0x12, boat 13.
static void pl0fActRideR10e2(cPl0f* em)
{
    pPL->m_pBoat = em;
    BoatMoveFunc = PlBoatMove;
    EmRoutineSet(pPL, 0, 0xF, 0x12, 0);
    EmRoutineSet(em, 1, 0xD, 0, 0);
    if (pSUB) {
        SetSubDamage(em, subBoatRide);
    }
}

// Get-off callback: the player walks to the landing (state 1 with m_VecWork0 / m_Fwork0), the boat
// waits, the engine stops, the partner into subBoatGetoff.
static void pl0fActGetOff(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    cPlayer* pl = pPL;

    pl->m_Fwork0 = w->Getoff_dir;
    pl->m_VecWork0 = w->Getoff_pos;
    EmRoutineSet(pl, 0, 0xF, 1, 0);
    EmRoutineSet(em, 1, 0, 0, 0);
    w->Be_flg &= ~1;
    SndStop(w->Seid_engine, 0);
    if (pSUB) {
        SetSubDamage(em, subBoatGetoff);
    }
}

// The boat hits the boss (em2f) or a floating island (obj1c).
// A node within 800 of a live Del Lago's hit box, or inside an island's radius (scale * 1800,
// which is set crashing): pl0fCrashAdjustSet with `away` (a straight backward shove) while the
// player hides or the boss rams (flag bit2), else a shove off the hit point. Returns 1 on a hit.
int pl0fCrashCk(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec hit;
    u32 i;
    u32 n;

    for (n = 0; n < EmMgr.getArrayNum(); n++) {
        cEm* e = EmMgr.fastAt(n);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x2F && (s16) e->hp > 0) {
            for (i = 0; i < 2; i++) {
                if (EmYarareContactCk(e, &w->node[i].wpos, 800.0f, &hit)) {
                    int away = 0;

                    if (StaFlagChk(pG, STA_PL_SPEAR_SET)) {
                        away = 1;
                    }
                    if (e->flag & 4) {
                        away = 1;
                    }
                    pl0fCrashAdjustSet(em, &hit, away);
                    return 1;
                }
            }
        }
    }
    for (n = 0; n < ObjMgr.getArrayNum(); n++) {
        cObj* o = ObjMgr.fastAt(n);

        if ((o->be_flag & 0x201) == 1 && o->id == 0x1C) {
            f32 r = o->scale.x * 1800.0f;

            for (i = 0; i < 2; i++) {
                Pl0fNode* nd = &w->node[i];

                if ((nd->wpos.x - o->pos.x) * (nd->wpos.x - o->pos.x) + (nd->wpos.z - o->pos.z) * (nd->wpos.z - o->pos.z) < r * r) {
                    int away;

                    ((cObj1c*) o)->setCrash();
                    away = 0;   // after the call: the flag stays in the argument register
                    if (StaFlagChk(pG, STA_PL_SPEAR_SET)) {
                        away = 1;
                    }
                    pl0fCrashAdjustSet(em, &o->pos, away);
                    return 1;
                }
            }
        }
    }
    return 0;
}

// Crash response: both nodes get a 300-unit speed straight backwards (`away`) or away from the
// hit point `p` (horizontal; backwards when the boat sits on the point).
void pl0fCrashAdjustSet(cPl0f* em, Vec* p, int away)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec d;
    u32 i;

    if (away) {
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = -1.0f;
        PSMTXRotRad(m, 'y', em->ang.y);
        PSMTXMultVecSR(m, &d, &d);
        PSVECScale(&d, &d, 300.0f);
    } else {
        PSVECSubtract(&em->pos, p, &d);
        d.y = 0.0f;
        if (d.x == 0.0f && d.z == 0.0f) {
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = -1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
        }
#line 2446
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 300.0f);
    }
    for (i = 0; i < 2; i++) {
        w->node[i].spd = d;
    }
}


// Pushes both nodes out of the scenario walls (SatMgr.adjust, 600-unit radius); the correction of
// the first node that hit is applied to both so the hull keeps its length. Skipped on the ferry
// types 1 / 2 (rails do it).
void pl0fScrAdjust(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec nrm;
    Vec p;
    Vec d;
    u32 i;
    u32 j;

    if (em->type == 1) {
        return;
    }
    if (em->type == 2) {
        return;
    }
    if (em->type == 3) {
        return;
    }
    if (em->type == 4) {
        return;
    }
    if (em->type == 5) {
        return;
    }
    for (i = 0; i < 2; i++) {
        Pl0fNode* n = &w->node[i];

        nrm.x = 0.0f;
        nrm.y = 0.0f;
        nrm.z = 0.0f;
        p = n->wpos;
        SatMgr.adjust(&nrm, &n->wposOld, &p, 300.0f, 0x2081, 0);
        if (!(nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f)) {
            f32 len;

            PSVECSubtract(&p, &n->wpos, &d);
            d.y = 0.0f;
            if (!(d.x == 0.0f && d.z == 0.0f)) {
                len = SQRTF(d.x * d.x + d.z * d.z) * 1.2f;
#line 2499
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, len);
                for (j = 0; j < 2; j++) {
                    Vec* wp = &w->node[j].wpos;
                    Vec* sp = &w->node[j].spd;

                    PSVECAdd(wp, &d, wp);
                    *sp = d;
                }
                return;
            }
        }
        // Dead second set of `n` (deleted by flow, the compare by jump2): with two sets `n` is not a
        // giv, so loop.c keeps the target's per-iteration `add n, w, ofs` (ofs = the reduced giv
        // i * sizeof(Pl0fNode) + 0x168, initialised after the hoisted highs) and the member addresses
        // stay displacements from `n` instead of becoming stepping pointers. The frame operands add
        // no register refs, so the callee-saved order (&nrm, ofs, i, &p, w) is unchanged.
        if (d.x == d.y) {
            n = 0;
        }
    }
}

static PlBoatFunc plboat_R2_move_tbl[19] = {
    plboat_R2_Ride,
    plboat_R2_Getoff,
    plboat_R2_Move,
    plboat_R2_SpearSet,
    plboat_R2_SpearThrow,
    plboat_R2_FallWater,
    plboat_R2_Swim,
    plboat_R2_Guard,
    plboat_R2_WaterRide,
    plboat_R2_Die,
    plboat_R2_SpearSet2,
    plboat_R2_SpearThrow2,
    plboat_R2_BossDie,
    plboat_R2_R10dIn,
    plboat_R2_R10dOut,
    plboat_R2_R10eIn,
    plboat_R2_R10eOut,
    plboat_R2_R10eIn2,
    plboat_R2_R10eOut2,
};

// The player's boat routine (pl_R1_Boat -> BoatMoveFunc): the boat's motion archive replaces the
// player's for the duration of the routine.
// Every frame: Status_flg[1] bit21 (hands busy / aboard), neck mode 2, the player's atari bits 8/9
// off, damage type 0x1E, then the plboat_R2_* state of r_no_2, then the anchor rope effects.
static void PlBoatMove(cPlayer* pl)
{
    if (pl->m_pBoat == 0) {
        pLog->err(0, 0, "PlBoatMove(): m_pBoat == NULL!");
        return;
    }
    StaFlagOn(pG, STA_PL_BOAT);
    PlSetNeck(2);
    pl->atari.m_flag &= 0xFCFF;
    pl->dmg.m_Timer = 0x1E;
    pl->subArc = pl->m_pBoat->subArc;
    pl->Motion.Mot_flag &= ~0x40000000;
    pl->m_SubMot.Mot_flag &= ~0x40000000;
    plboat_R2_move_tbl[pl->r_no_2](pl);
    pl->Motion.Mot_flag &= ~0x40000000;
    pl->m_SubMot.Mot_flag &= ~0x40000000;
    pl0fSetAnchorEm2f(pl);
    pl->subArc = pl->subArc2;
}

// The engine start SE of the boat, the player's foot on the tiller.
// Engine start SE: a macro on the routine's own `Vec v` (the original shares the boarding vector's frame
// slot; an inline's temp Vec takes a second slot).
#define PLBOAT_ENGINE_START() \
{ \
    Pl0fWork* w = PL0F_WK(boat); \
 \
    w->Be_flg |= 1; \
    v = boat->pos; \
    v.y += 500.0f; \
    w->Seid_engine = SndCall(8, 0x11, &v, 0xF, 0, 0); \
}

// Player state 0 (board from the shore): the step-in motion 0x29 from 1.5 m beside the boat,
// climbing the height difference (m_Fwork0) over 20 frames, the tiller hand model, the weapon
// hidden, the engine started; then -> steer (2).
static void plboat_R2_Ride(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;

    switch (pl->r_no_3) {
    case 0:
        v.x = 1500.0f;
        v.y = 500.0f;
        v.z = 100.0f;
        PSMTXMultVec(boat->mat, &v, &v);
        pl->pos.x = v.x;
        pl->pos.z = v.z;
        pl->m_Work7 = 1;
        pl->m_Fwork0 = v.y - pl->pos.y;
        pl->m_Work0 = 20;
        pl->ang.y = boat->ang.y - PI / 2;
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x29), 0, 0, 5, 0);
        pl->m_BoatPlDir = 0.0f;
        pl->m_Blend = 0.0f;
        pl->Body->initWepHand((u32) EM_ARC(pl, 0x8));
        pl->setRightHand(1);
        pl->Wep->setTrans(0, 0);
        pl->r_no_3++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            f32 dy = pl->m_Fwork0 * 0.1f;

            pl->pos.y += dy;
            pl->m_Fwork0 -= dy;
        }
        if (pl->Motion.Seq_frame > 25.7f && pl->Motion.Seq_frame < 26.3f) {
            SndCall(8, 4, &pl->pos, boat->id, 0, 0);
        }
        if (pl->Motion.Seq_frame > 27.7f && pl->Motion.Seq_frame < 28.3f) {
            SndCall(8, 5, &pl->pos, boat->id, 0, 0);
        }
        if (MotionMove(pl, 0)) {
            PLBOAT_ENGINE_START();
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->m_Work7) {
        pl->m_Work7--;
        pl0fRideCamMove(boat, 1.0f);
    } else {
        pl0fRideCamMove(boat, 0.05f);
    }
}

// Player state 1 (get off at a landing): the boat is moored at the landing position (m_VecWork0 /
// m_Fwork0), the step-out motion 0x2A plays with the partner seated, the hands / weapon are
// restored and the routine ends into footwork.
static void plboat_R2_Getoff(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    switch (pl->r_no_3) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x2A), 0, 0, 5, 0);
        pl->pos = pl->m_VecWork0;
        pl->ang.y = pl->m_Fwork0;
        boat->setPos(&pl->pos, pl->m_Fwork0);
        pl->m_Blend = 0.0f;   // the pSUB load stays below the store
        if (pSUB) {
            subOnBoat(pSUB, boat);
            pSUB->partsMatCalc();
            pSUB->partsWorldCalc();
        }
        pl->m_Work0 = 20;
        pl->m_Fwork0 = 100.0f;
        pl->r_no_3++;
    case 1:
        pl0fGetoffCamMove(boat);
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            f32 dy = pl->m_Fwork0 * 0.1f;

            pl->pos.y += dy;
            pl->m_Fwork0 -= dy;
        }
        if (pl->Motion.Seq_frame > 30.7f && pl->Motion.Seq_frame < 31.3f) {
            SndCall(5, 2, &pl->pos, pl->id, 0, 0);
        }
        if (pl->Motion.Seq_frame > 45.7f && pl->Motion.Seq_frame < 46.3f) {
            SndCall(5, 3, &pl->pos, pl->id, 0, 0);
        }
        if (MotionMove(pl, 0)) {
            cPlayer* p;

            EndPlDamage();
            p = pPL;
            p->setRightHand(0);
            p->Wep->setTrans(1, 0);
        }
        break;
    }
}

// Player state 2 (sitting at the tiller): the sit / lean blend 0x9 / 0xB / 0xA with the lean
// (m_Blend) from the stick left / right, the sight tilt relaxing; the stick goes to the boat's
// tiller; aim key -> harpoon aim (3); the boss opening its mouth (flag bit5) -> the hiding aim
// (0xA); without a boss the get-off action is offered. Runs the fight camera.
static void plboat_R2_Move(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->r_no_3) {
    case 0:
        pl->m_Blend = 0.0f;
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        if (pl->m_pSpear) {
            pl->m_pSpear->setLost();
            pl->m_pSpear = 0;
        }
        pl->r_no_3++;
    case 1:
        plboatBlendMotSet(pl, EM_ARC(pl, 0x9), EM_ARC(pl, 0xB), EM_ARC(pl, 0xA), 0, 0, 0);
        if (Key.on & 0xC) {
            if (Key.on & 8) {
                pl->m_Blend += 31.875f;
                if (pl->m_Blend > 255.0f) {
                    pl->m_Blend = 255.0f;
                }
            }
            if (Key.on & 4) {
                pl->m_Blend -= 31.875f;
                if (pl->m_Blend < -255.0f) {
                    pl->m_Blend = -255.0f;
                }
            }
        } else {
            pl->m_Blend *= 0.9f;
        }
        pl->m_BoatPlDir *= 0.9f;
        plOnBoat(pl);
        MotionMove(pl, 0);
        if (Key.on & 0x10) {
            EmRoutineSet(pPL, 0, 0xF, 3, 0);
        }
        break;
    }
    if (pl->m_pBoat) {
        PL_BOAT(pl)->setTiller();
        if (pl->m_pBoat) {
            pl0fBossCamMove(PL_BOAT(pl), 0);
        }
    }
    if (boss) {
        if (boss->flag & 0x20) {
            EmRoutineSet(pPL, 0, 0xF, 0xA, 0);
        }
    } else {
        pl0fGetoffActEvtCk(boat);
    }
}

// Harpoon aim: the stick (or the buttons) lean the player (m_Blend) and tilt the sight
// (m_BoatPlDir); at the sight limits the boat turns.
// Harpoon aim: a macro, not a static inline. integrate.c drops RTX_UNCHANGING_P from the inlined pool loads
// (the 255 / 0.39 clamp constants), so they would depend on the preceding `stfs m_Blend/m_BoatPlDir` and
// sink below it; the original loads them before the add and compares before the store.
#define PLBOAT_AIM_CONTROL() \
{ \
    f32 d; \
 \
    if ((u8) (Key.stickY + 15) > 30) { \
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) { \
            d = (f32) -Key.stickY / 72.0f * 31.875f; \
        } else { \
            d = (f32) Key.stickY / 72.0f * 31.875f; \
        } \
        pl->m_Blend += d; \
        if (pl->m_Blend > 255.0f) { \
            pl->m_Blend = 255.0f; \
        } \
        if (pl->m_Blend < -255.0f) { \
            pl->m_Blend = -255.0f; \
        } \
    } else if (Key.on & 3) { \
        if (Key.on & 1) { \
            if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) { \
                d = -31.875f; \
            } else { \
                d = 31.875f; \
            } \
        } else { \
            if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) { \
                d = 31.875f; \
            } else { \
                d = -31.875f; \
            } \
        } \
        pl->m_Blend += d; \
        if (pl->m_Blend > 255.0f) { \
            pl->m_Blend = 255.0f; \
        } \
        if (pl->m_Blend < -255.0f) { \
            pl->m_Blend = -255.0f; \
        } \
    } \
    if ((u8) (Key.stickX + 15) > 30) { \
        d = (f32) Key.stickX / -72.0f * 0.049087385f; \
        pl->m_BoatPlDir += d; \
        if (pl->m_BoatPlDir > 0.3926991f) { \
            pl->m_BoatPlDir = 0.3926991f; \
            boat->ang.y += d; \
        } \
        if (pl->m_BoatPlDir < -0.3926991f) { \
            pl->m_BoatPlDir = -0.3926991f; \
            boat->ang.y += d; \
        } \
    } else if (Key.on & 0xC) { \
        if (Key.on & 4) { \
            d = -0.049087385f; \
        } else { \
            d = 0.049087385f; \
        } \
        pl->m_BoatPlDir += d; \
        if (pl->m_BoatPlDir > 0.3926991f) { \
            pl->m_BoatPlDir = 0.3926991f; \
            boat->ang.y += d; \
        } \
        if (pl->m_BoatPlDir < -0.3926991f) { \
            pl->m_BoatPlDir = -0.3926991f; \
            boat->ang.y += d; \
        } \
    } \
}

// Player state 3 (harpoon aim): steps 0/1 stand up (motion 0xC, the harpoon appears in the hand at
// frame 7; the aim key released or the boss opening its mouth cancels back to sitting); step 3
// the aim blend 0xE / 0xF / 0xD with the stick leaning (m_Blend) and tilting the sight
// (m_BoatPlDir, the boat turns at the limits), the sight cursor effect; the throw button -> throw
// (4); released / mouth open -> steps 4/5 sit down (0x13, the harpoon vanishes at frame 14).
// The camera stays in the normal view for 15 frames (m_Work3) then hides the player.
static void plboat_R2_SpearSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->r_no_3) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xC), 0, 0xA, 1, 0);
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        pl->m_Blend = 0.0f;
        pl->m_Work3 = 0xF;
        pl->r_no_3++;
    case 1:
        plOnBoat(pl);
        if (pl->Motion.Seq_frame > 6.7f && pl->Motion.Seq_frame < 7.3f) {
            plboatSetSpear(pl);
        }
        if (MotionMove(pl, 0)) {
            pl->r_no_3++;
        } else if (!(Key.on & 0x10) || (boss && (boss->flag & 0x20))) {
            if (pl->m_pSpear) {
                pl->m_pSpear->setLost();
                pl->m_pSpear = 0;
            }
            pl->m_Blend = 0.0f;   // the pPL load stays below the store
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    case 2:
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        plboatSetSpear(pl);
        pl->r_no_3++;
    case 3:
        plboatBlendMotSet(pl, EM_ARC(pl, 0xE), EM_ARC(pl, 0xF), EM_ARC(pl, 0xD), 0, 0, 0);
        PLBOAT_AIM_CONTROL();
        plOnBoat(pl);
        MotionMove(pl, 0);
        plboatSightCurMove(pl);
        if ((boss && (boss->flag & 0x20)) || !(Key.on & 0x10)) {
            pl->r_no_3++;
        } else if (Key.trg & 0x80) {
            EmRoutineSet(pPL, 0, 0xF, 4, 0);
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x13), 0, 0xA, 1, 0);
        pl->m_Work3 = 99999;
        pl->r_no_3++;
    case 5:
        plOnBoat(pl);
        if (pl->Motion.Seq_frame > 13.7f && pl->Motion.Seq_frame < 14.3f) {
            if (pl->m_pSpear) {
                pl->m_pSpear->setLost();
                pl->m_pSpear = 0;
            }
        }
        if (MotionMove(pl, 0)) {
            pl->m_Blend = 0.0f;
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->m_pBoat) {
        if (pl->m_Work3) {
            pl->m_Work3--;
            pl0fBossCamMove(PL_BOAT(pl), 0);
        } else {
            StaFlagOn(pG, STA_PL_SPEAR_SET);
            pl0fBossCamMove(PL_BOAT(pl), 1);
        }
    }
}

// Player state 4 (harpoon throw): the throw blend 0x11 / 0x12 / 0x10; the harpoon flies at frame
// 12 (plboatSpearThrow), the next one is drawn at frame 46; at the end -> aim (3) step 2. The aim
// key released after 20 frames -> sit (2) during frames 31..49, else -> aim step 4 (sit down).
// Status_flg[1] bit23 (hidden view) and the hidden-view camera.
static void plboat_R2_SpearThrow(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        pl->m_Work0 = 0;
        pl->r_no_3++;
    case 1:
        plboatBlendMotSet(pl, EM_ARC(pl, 0x11), EM_ARC(pl, 0x12), EM_ARC(pl, 0x10), 0, 0, 0);
        plOnBoat(pl);
        if (MotionMove(pl, 0)) {
            EmRoutineSet(pPL, 0, 0xF, 3, 2);
        } else {
            if (pl->Motion.Seq_frame > 11.7f && pl->Motion.Seq_frame < 12.3f) {
                plboatSpearThrow(pl);
            }
            if (pl->Motion.Seq_frame > 45.7f && pl->Motion.Seq_frame < 46.3f) {
                plboatSetSpear(pl);
            }
            pl->m_Work0++;
            if ((int) pl->m_Work0 > 20 && !(Key.on & 0x10)) {
                if ((int) pl->m_Work0 >= 31 && (int) pl->m_Work0 <= 49) {
                    pl->m_Blend = 0.0f;
                    EmRoutineSet(pPL, 0, 0xF, 2, 0);
                } else {
                    EmRoutineSet(pPL, 0, 0xF, 3, 4);
                }
            }
        }
        break;
    }
    StaFlagOn(pG, STA_PL_SPEAR_SET);
    if (pl->m_pBoat) {
        pl0fBossCamMove(PL_BOAT(pl), 1);
    }
}

// Player state 0xA (the boss opens its mouth: the "throw into the mouth" chance): the boat is
// teleported to the hiding spot (pl0fHidePosSet), the long stand-up motion 0x27 (harpoon at frame
// 90), then the aim blend with the sight and the throw action prompt (0x17); the throw button ->
// throw (0xB); the mouth closing (flag bit5 clear) -> sit down (0x13) and back to sitting (2).
// Hidden-view camera throughout.
static void plboat_R2_SpearSet2(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->r_no_3) {
    case 0:
        pl0fHidePosSet(pl);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x27), 0, 0xA, 1, 0);
        pl->m_Blend = 0.0f;
        pl->m_Frame = 0;
        pl->m_Hokan = 0xA;
        pl->m_Work3 = 0xF;
        pl0fHideModeCamSet(pl);
        pl->r_no_3++;
    case 1:
        plOnBoat(pl);
        if (pl->Motion.Seq_frame > 89.7f && pl->Motion.Seq_frame < 90.3f) {
            plboatSetSpear(pl);
        }
        if (MotionMove(pl, 0)) {
            pl->r_no_3++;
        }
        break;
    case 2:
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        plboatSetSpear(pl);
        pl->r_no_3++;
    case 3:
        ActBtn.set(ACT_SEARCH_ATTACK, 5, 0, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        plboatBlendMotSet(pl, EM_ARC(pl, 0xE), EM_ARC(pl, 0xF), EM_ARC(pl, 0xD), 0, 0, 0);
        PLBOAT_AIM_CONTROL();
        plOnBoat(pl);
        MotionMove(pl, 0);
        plboatSightCurMove(pl);
        if (boss && !(boss->flag & 0x20)) {
            pl->r_no_3++;
        } else if (Key.trg & 0x80) {
            EmRoutineSet(pPL, 0, 0xF, 0xB, 0);
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x13), 0, 0xA, 1, 0);
        pl->m_Work3 = 99999;
        pl->r_no_3++;
    case 5:
        plOnBoat(pl);
        if (pl->Motion.Seq_frame > 13.7f && pl->Motion.Seq_frame < 14.3f) {
            if (pl->m_pSpear) {
                pl->m_pSpear->setLost();
                pl->m_pSpear = 0;
            }
        }
        if (MotionMove(pl, 0)) {
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->m_pBoat) {
        if (pl->m_Work3) {
            pl->m_Work3--;
            pl0fBossCamMove(PL_BOAT(pl), 0);
        } else {
            StaFlagOn(pG, STA_PL_SPEAR_SET);
            pl0fHideModeCamMove(pl);
        }
    }
}

// Player state 0xB (the throw of the mouth chance): the throw blend, the harpoon flies at frame 12
// and the next is drawn at frame 46; at the end -> the hiding aim (0xA) step 2. Hidden view.
static void plboat_R2_SpearThrow2(cPlayer* pl)
{
    ActBtn.set(ACT_SEARCH_ATTACK, 5, 0, 0, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    switch (pl->r_no_3) {
    case 0:
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        pl->m_Work0 = 0;
        pl->r_no_3++;
    case 1:
        plboatBlendMotSet(pl, EM_ARC(pl, 0x11), EM_ARC(pl, 0x12), EM_ARC(pl, 0x10), 0, 0, 0);
        plOnBoat(pl);
        if (MotionMove(pl, 0)) {
            EmRoutineSet(pPL, 0, 0xF, 0xA, 2);
        } else {
            if (pl->Motion.Seq_frame > 11.7f && pl->Motion.Seq_frame < 12.3f) {
                plboatSpearThrow(pl);
            }
            if (pl->Motion.Seq_frame > 45.7f && pl->Motion.Seq_frame < 46.3f) {
                plboatSetSpear(pl);
            }
        }
        break;
    }
    StaFlagOn(pG, STA_PL_SPEAR_SET);
    pl0fHideModeCamMove(pl);
}

// Player state 0xC (Del Lago dies, set by the boss): the boat is put at the death spot
// (pl0fBossDiePosSet), the player holds the aim pose 0xE with a harpoon for 300 frames, then sits
// down (0x13, the harpoon vanishes at frame 14) and -> sitting (2). Boss-death camera.
static void plboat_R2_BossDie(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        pl0fBossDiePosSet(pl);
        plboatSetSpear(pl);
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xE), 0, 0, 1, 0);
        pl0fBossDieCamSet(pl);
        pl->m_Work0 = 300;
        pl->r_no_3++;
    case 1:
        plOnBoat(pl);
        MotionMove(pl, 0);
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            pl->r_no_3++;
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x13), 0, 0xA, 1, 0);
        pl->m_Work3 = 99999;
        pl->r_no_3++;
    case 3:
        plOnBoat(pl);
        if (pl->Motion.Seq_frame > 13.7f && pl->Motion.Seq_frame < 14.3f) {
            if (pl->m_pSpear) {
                pl->m_pSpear->setLost();
                pl->m_pSpear = 0;
            }
        }
        if (MotionMove(pl, 0)) {
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    pl0fBossDieCamMove(pl);
}

// Player state 7 (crash / boss-close guard): the brace motion 0x1E with the harpoon dropped, then
// -> sitting (2); fight camera.
static void plboat_R2_Guard(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x1E), 0, 0xA, 1, 0);
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        pl->m_Blend = 0.0f;
        if (pl->m_pSpear) {
            pl->m_pSpear->setLost();
            pl->m_pSpear = 0;
        }
        pl->r_no_3++;
    case 1:
        plOnBoat(pl);
        if (MotionMove(pl, 0)) {
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->m_pBoat) {
        pl0fBossCamMove(PL_BOAT(pl), 0);
    }
}

// Player state 5 (thrown into the water by the capsize): the fall motion 0x20, 500 damage,
// vibration, harpoon lost, the drop camera; the player stays on the boat's origin for 23 frames
// (then 150 up), the splash effect / SE after 26 frames, the stream ducked; from frame 65 he turns
// towards the boat; at the end, if alive, -> swim (6). Status_flg[1] bit22 = in the water.
static void plboat_R2_FallWater(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    StaFlagOn(pG, STA_PL_SWIM);
    switch (pl->r_no_3) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x20), 0, 3, 1, 0);
        pl->ang.x = 0.0f;
        pl->ang.z = 0.0f;
        pl->m_Blend = 0.0f;
        LifeDownSet2(pl, 500, 0, 1);
        VibSetData(VIB_TBL, 0xB, 1);
        if (pl->m_pSpear) {
            pl->m_pSpear->setLost();
            pl->m_pSpear = 0;
        }
        pl->m_Work0 = 65;
        pl->m_Work1 = 23;
        pl->m_Fwork0 = pl->ang.y;
        pl->m_Work3 = 0;
        pPL->endCamera();   // struct view: the pPL load stays below the four stores
        pl00SetDropCam(pl);
        EstSet(pl, -1, 0, 0, 0xF, 0x15, 0, 0x35, boat, 0);
        pl->m_Work2 = 26;
        pl->setRightHand(0);
        SndCall(8, 0x16, &pl->pos, 0xF, 0, 0);
        if ((s16) pG->pl_life <= 0) {
            PlSetDamageSe(0xD);
        } else {
            PlSetDamageSe(0);
        }
        pl->r_no_3++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            pl->ang.y += Muku(&pl->pos, &pl->m_pBoat->pos, pl->ang.y, 0.09817477f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        if (pl->m_Work1) {
            Vec v;

            pl->m_Work1--;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            PSMTXMultVec(pl->m_pBoat->mat, &v, &pl->pos);
            if (pl->m_Work1 == 0) {
                pl->pos.y += 150.0f;
            }
        }
        if (pl->m_Work2) {
            pl->m_Work2--;
            if (pl->m_Work2 == 0) {
                EstSet(0, -1, &pl->pos, 0, 0xF, 0x14, 0, 0x35, pl, 0);
                SndCall(8, 2, &pl->pos, 0xF, 0, 0);
                SndStrVolSet(0, 5, 70, 1);
            }
        }
        if (pl->Motion.Seq_frame > 122.7f && pl->Motion.Seq_frame < 123.3f) {
            SndCall(8, 3, &pl->pos, 0xF, 0, 0);
            SndStrVolReset(0, 5, 1);
        }
        if (MotionMove(pl, 0)) {
            if ((s16) pG->pl_life > 0) {
                EmRoutineSet(pPL, 0, 0xF, 6, 0);
            }
        }
        break;
    }
    pl00DropCamMove(pl);
}

// Player state 6 (swim back to the boat, button mashing): the boat is re-placed (pl0fSwimPosSet),
// the swim camera or (random, not the first time) the chase camera with the boss closing in
// (Status_flg[1] bit20) for 90 frames. m_Work0 is the stroke energy (A presses add m_Work4, which
// recharges to 12 / 8 / 4 by Game_level, capped 159; halved when nearly dead) and decays; every 20
// energy the stroke motion (0x15..0x1C) and forward speed (50..170 units) step up; splash effects
// on the motion events, kept at the water surface; within 1.8 m of the boat -> climb in (8) and
// the boat's r_no_1 5. The action prompt 0x11 (mash) is shown.
static void plboat_R2_Swim(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    int first = 0;
    int one;
    Vec v;
    f32 h;

    if (boat) {
        Pl0fWork* w = PL0F_WK(boat);

        if (w->First_camck == 0) {
            w->First_camck = 1;
            first = 1;
        }
    }
    // COMPILER-DIFF: #13 -- single-use constant set in another block: update_equiv_regs moves the
    // `li` next to the x3E4 store (shortest qty -> r9 before the pG pointer) and the store's source
    // crosses the calls, so it gets the TRUE store->call link and is issued first (source order).
    one = 1;
    StaFlagOn(pG, STA_PL_SWIM);
    switch (pl->r_no_3) {
    case 0:
        pl->ang.x = 0.0f;
        pl->ang.z = 0.0f;
        pl0fSwimPosSet(pl);
        EffectEspDelete(0, ESP_CORE_KIND_WATER, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_WATER, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_WATER, pl);
        StaFlagOff(pG, STA_WATER_CAMERA);
        pl->m_Work1 = one;
        pl->m_Work0 = 0;
        pl->m_Work4 = 0;
        pl->m_Work2 = 4;
        if ((Rnd() & 1) || first) {
            pl00SetSwimCam(pl);
            pl->m_Work3 = 0;
        } else {
            pl00SetChaseCam(pl);
            pl->m_Work3 = 90;
            EstSet(0, -1, 0, 0, 0xF, 0xE, 0, 0x34, pl, (void*) first);
            StaFlagOn(pG, STA_WATER_CAMERA);
        }
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x14), EM_ARC(pl, 0x15), 5, 5, 0);
        pl->m_VecWork0.x = 0.0f;
        pl->m_VecWork0.y = 0.0f;
        pl->m_VecWork0.z = 50.0f;
        pl->ang.y += Muku(&pl->pos, &pl->m_pBoat->pos, pl->ang.y, PI);
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        pl->r_no_3++;
    case 1: {
        int n = (int) pl->m_Work0 / 20;
        int lim;

        if (n > 7) {
            n = 7;
        }
        if (n != pl->m_Work1) {
            void* m;
            f32 rate;
            u32 cnt;
            u32 f;

            pl->m_Work1 = n;
            switch (n) {
            case 0:
            default:
                m = EM_ARC(pl, 0x15);
                pl->m_VecWork0.z = 50.0f;
                break;
            case 1:
                m = EM_ARC(pl, 0x16);
                pl->m_VecWork0.z = 62.5f;
                break;
            case 2:
                m = EM_ARC(pl, 0x17);
                pl->m_VecWork0.z = 75.0f;
                break;
            case 3:
                m = EM_ARC(pl, 0x18);
                pl->m_VecWork0.z = 87.5f;
                break;
            case 4:
                m = EM_ARC(pl, 0x19);
                pl->m_VecWork0.z = 100.0f;
                break;
            case 5:
                m = EM_ARC(pl, 0x1A);
                pl->m_VecWork0.z = 120.5f;
                break;
            case 6:
                m = EM_ARC(pl, 0x1B);
                pl->m_VecWork0.z = 140.0f;
                break;
            case 7:
                m = EM_ARC(pl, 0x1C);
                pl->m_VecWork0.z = 170.0f;
                break;
            }
            rate = pl->Motion.Seq_frame / (f32) pl->Motion.Seq_frame_num;
            cnt = *(u16*) m;
            f = (u32) ((f32) cnt * rate) + 1;
            if (f >= cnt) {
                f = 0;
            }
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x14), m, pl->Motion.Hokan_cnt, 5, (u16) f);
        }
        if (pl->Motion.Seq_old.Free & 0x40) {
            EstSet(pl, -1, 0, 0, 0xF, 0x11, 0, 0x35, boat, 0);
            SndCall(8, 0x1A, &pl->pos, 0xF, 0, 0);
        }
        if (pl->Motion.Seq_old.Free & 0x80) {
            EstSet(pl, -1, 0, 0, 0xF, 0x12, 0, 0x35, boat, 0);
            SndCall(8, 0x19, &pl->pos, 0xF, 0, 0);
        }
        if (pl->m_Work2) {
            pl->m_Work2--;
        } else {
            pl->m_Work2 = 3;
            EstSet(pl, -1, 0, 0, 0xF, 0x10, 0, 0x35, boat, 0);
        }
        lim = 8;
        if (pG->Game_level <= 3) {
            lim = 12;
        }
        if (pG->Game_level > 6) {
            lim = 4;
        }
        pl->m_Work4++;
        if (pl->m_Work4 > lim) {
            pl->m_Work4 = lim;
            if (pl->m_Work0) {
                pl->m_Work0--;
            }
        }
        if (Key.trg & 0x80000) {
            if ((s16) pG->pl_life <= 1) {
                pl->m_Work0 += pl->m_Work4 - pl->m_Work4 / 2;
            } else {
                pl->m_Work0 += pl->m_Work4;
            }
            pl->m_Work4 = 0;
            if ((int) pl->m_Work0 > 159) {
                pl->m_Work0 = 159;
            }
        }
        PSMTXMultVecSR(pl->mat, &pl->m_VecWork0, &v);
        PSVECAdd(&pl->pos, &v, &pl->pos);
        MotionMove(pl, 0);
        break;
    }
    }
    if (GetWaterHeight(&pl->pos, &h)) {
        pl->pos.y = h - 100.0f;
    }
    if (pl->m_pBoat->l_pl < 3240000.0f) {
        EmRoutineSet(pPL, 0, 0xF, 8, 0);
        pl->m_pBoat->r_no_0 = 1;
        pl->m_pBoat->r_no_1 = 5;
        pl->m_pBoat->r_no_2 = 0;
        pl->m_pBoat->r_no_3 = 0;
    }
    if (pl->m_Work3) {
        pl->m_Work3--;
        if (pl->m_Work3 == 0) {
            pl00SetSwimCam(pl);
            EffectEspDelete(0, ESP_CORE_KIND_WATER, pl, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_WATER, pl);
            EffectEfmDelete(0, ESP_CORE_KIND_WATER, pl);
            StaFlagOff(pG, STA_WATER_CAMERA);
        }
    }
    if (pl->m_Work3) {
        pl00ChaseCamMove(pl);
    } else {
        pl00SwimCamMove(pl);
    }
    ActBtn.set(ACT_SWIM, 5, 0, 0, ACTCTR_NONE, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
}

static Vec plboat_ride_pos;
static f32 plboat_ride_ang;

// Player state 8 (climb back into the boat): the climb motion 0x21 while sliding to the boat's
// side seat (30 % per frame) and following the boat's movement / turn; at the end the tiller hand
// and hidden weapon are restored and -> sitting (2). Swim camera.
static void plboat_R2_WaterRide(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Mtx m;
    Vec v;

    switch (pl->r_no_3) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x21), 0, 0xF, 5, 0);
        pl->m_BoatPlDir = 0.0f;
        pl->m_Blend = 0.0f;
        PSMTXRotRad(m, 'y', boat->ang.y);
        TransMatrix(m, &boat->pos);
        v.x = 1604.49f;
        v.y = 0.0f;
        v.z = -50.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&v, &pl->pos, &pl->m_VecWork0);
        pl->ang.y = pl->m_pBoat->ang.y - PI / 2;
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        EstSet(pl, -1, 0, 0, 0xF, 0x16, 0, 0x35, boat, 0);
        plboat_ride_pos = boat->pos;
        plboat_ride_ang = boat->ang.y;
        pl->r_no_3++;
    case 1:
        PSVECScale(&pl->m_VecWork0, &v, 0.3f);
        PSVECAdd(&pl->pos, &v, &pl->pos);
        PSVECSubtract(&pl->m_VecWork0, &v, &pl->m_VecWork0);
        PSVECSubtract(&boat->pos, &plboat_ride_pos, &v);
        plboat_ride_pos = boat->pos;
        PSVECAdd(&pl->pos, &v, &pl->pos);
        pl->ang.y += Muku2(plboat_ride_ang, boat->ang.y, PI);
        plboat_ride_ang = boat->ang.y;
        if (MotionMove(pl, 0)) {
            pl->Body->initWepHand((u32) EM_ARC(pl, 0x8));
            pl->setRightHand(1);
            pl->Wep->setTrans(0, 0);
            EmRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    pl00SwimCamMove(pl);
}

// Player state 9 (death in the water, set by the boss): the death motion 0x28; with the boss
// biting (flag bit7, 50 %) the player is eaten: carried in the boss's mouth (parts 8) with the
// top-down death camera, pl_life = 0 after 60 frames; else he drowns under the swim camera with
// pl_life = 0 next frame.
static void plboat_R2_Die(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->r_no_3) {
    case 0: {
        int eaten = 0;

        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x28), 0, 0, 5, 0);
        if (boss && (boss->flag & 0x80) && (Rnd() & 1)) {
            eaten = 1;
        }
        if (eaten) {
            pl00SetDieCam(pl);
            pl->m_Work0 = 1;
            pl->m_Work1 = 60;
            if (boss) {
                Em2fWork* bw = EM2F_WK(boss);

                EffectEspDelete(0, bw->espKind, boss, 0);
                EffectEspgenDelete(0, bw->espKind, boss);
                EffectEfmDelete(0, bw->espKind, boss);
                EstSet(boss, -1, 0, 0, 0x27, 0xA, 0, 0, boss, 0);
            }
        } else {
            pl->m_Work0 = eaten;
            pl->m_Work1 = 1;
        }
        VibSetData(VIB_TBL, 0xD, 1);
        pl->r_no_3++;
    }
    case 1:
        if (boss) {
            pl->pos.x = 0.0f;
            pl->pos.y = -300.0f;
            pl->pos.z = 700.0f;
            pl->ang.x = PI / 2;
            pl->ang.y = 0.0f;
            pl->ang.z = 0.0f;
            RotMatrix(pl->mat, &pl->ang);
            TransMatrix(pl->mat, &pl->pos);
            ScaleMatrix(pl->mat, &pl->scale);
            PSMTXConcat(boss->getPartsPtr(8)->mat, pl->mat, pl->mat);
            pl->Motion.Mot_flag |= 0x40000000;
        }
        MotionMove(pl, 0);
        if (pl->m_Work1) {
            pl->m_Work1--;
            if (pl->m_Work1 == 0) {
                pG->pl_life = 0;
            }
        }
        break;
    }
    if (pl->m_Work0) {
        pl00DieCamMove(pl);
    } else {
        pl00SwimCamMove(pl);
    }
}

// R10d / R10e entrance: the player sits and steers (the boat drives itself, pl0f_R1_R10xIn). A macro,
// not a static inline: integrate.c copies the inlined body's MEMs without RTX_UNCHANGING_P, so an inlined
// pool load (`lfs 0.0`, `lfs 1.0`) gets a true dependence on every preceding store through `pl` and sinks
// below them; the original issues both loads above the stores (stw m_Work7, stb m_Frame, stfs, ... / lis, subi,
// lfs, mr, stw).
#define PLBOAT_ROOM_IN() \
{ \
    switch (pl->r_no_3) { \
    case 0: \
        pl->Body->initWepHand((u32) EM_ARC(pl, 0x8)); \
        pl->setRightHand(1); \
        pl->Wep->setTrans(0, 0); \
        pl->m_Work7 = 1; \
        pl->m_Hokan = 0; \
        pl->m_Frame = 0; \
        pl->m_Blend = 0.0f; \
        pl->r_no_3++; \
    case 1: \
        plboatBlendMotSet(pl, EM_ARC(pl, 0x9), EM_ARC(pl, 0xB), EM_ARC(pl, 0xA), 0, 0, 0); \
        plOnBoat(pl); \
        MotionMove(pl, 0); \
        break; \
    } \
    if (pl->m_Work7) { \
        pl->m_Work7--; \
        pl0fRideCamMove(boat, 1.0f); \
    } else { \
        pl0fRideCamMove(boat, 0.3f); \
    } \
}

// Player states 0xD / 0xF / 0x11 (ferry entrances of rooms 10D / 10E): sits at the tiller with
// the lean blend while the boat drives itself; ride camera (snapped the first frame).
static void plboat_R2_R10dIn(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_IN();
}

// R10d / R10e exit: the player boards the boat at the landing and it leaves the room. A macro, not an
// inline with f32 parameters: the room position literals must enter the constant pool at their use in
// case 2 (an inline's constant arguments are expanded first and head the pool).
#define PLBOAT_ROOM_OUT(px, py, pz, a) \
{ \
    Vec v; \
 \
    switch (pl->r_no_3) { \
    case 0: \
        v.x = 1500.0f; \
        v.y = 500.0f; \
        v.z = 100.0f; \
        PSMTXMultVec(boat->mat, &v, &v); \
        pl->pos.x = v.x; \
        pl->pos.z = v.z; \
        pl->m_Work7 = 1; \
        pl->m_Fwork0 = v.y - pl->pos.y; \
        pl->m_Work0 = 20; \
        pl->ang.y = boat->ang.y - PI / 2; \
        pl->ang.y = LIMIT_ANGLE(pl->ang.y); \
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x29), 0, 0, 5, 0); \
        pl->m_BoatPlDir = 0.0f; \
        pl->m_Blend = 0.0f; \
        pl->Body->initWepHand((u32) EM_ARC(pl, 0x8)); \
        pl->setRightHand(1); \
        pl->Wep->setTrans(0, 0); \
        pl->r_no_3++; \
    case 1: \
        if (pl->m_Work7) { \
            pl->m_Work7--; \
            pl0fRideCamMove(boat, 1.0f); \
        } else { \
            pl0fRideCamMove(boat, 0.05f); \
        } \
        if (pl->m_Work0) { \
            pl->m_Work0--; \
        } else { \
            f32 dy = pl->m_Fwork0 * 0.1f; \
 \
            pl->pos.y += dy; \
            pl->m_Fwork0 -= dy; \
        } \
        if (pl->Motion.Seq_frame > 25.7f && pl->Motion.Seq_frame < 26.3f) { \
            SndCall(8, 4, &pl->pos, boat->id, 0, 0); \
        } \
        if (pl->Motion.Seq_frame > 27.7f && pl->Motion.Seq_frame < 28.3f) { \
            SndCall(8, 5, &pl->pos, boat->id, 0, 0); \
        } \
        if (MotionMove(pl, 0)) { \
            PLBOAT_ENGINE_START(); \
            pl->r_no_3++; \
        } \
        break; \
    case 2: \
        pl->m_Hokan = 0; \
        pl->m_Frame = 0; \
        pl->m_Blend = 0.0f; \
        pl->pos.x = px; \
        pl->pos.y = py; \
        pl->pos.z = pz; \
        pl->ang.y = a; \
        boat->setPos(&pl->pos, a); \
        EffectEspDelete(0, ESP_CORE_KIND_BOAT, boat, 0); \
        EffectEspgenDelete(0, ESP_CORE_KIND_BOAT, boat); \
        EffectEfmDelete(0, ESP_CORE_KIND_BOAT, boat); \
        pl->m_Work0 = 0; \
        pl->r_no_3++; \
    case 3: \
        pl->m_Work0++; \
        if (pl->m_Work0 & 1) { \
            EstSet(boat, -1, 0, 0, 1, 0xA, 0, 0x35, boat, 0); \
        } \
        if ((int) pl->m_Work0 % 20 == 0) { \
            Vec p; \
 \
            p = pl->pos; \
            p.y += 500.0f; \
            SndCall(8, 0x12, &p, 0xF, 0, 0); \
        } \
        boat->setTillerFront(); \
        pl0fRideCamMove(boat, 1.0f); \
        plboatBlendMotSet(pl, EM_ARC(pl, 0x9), EM_ARC(pl, 0xB), EM_ARC(pl, 0xA), 0, 0, 0); \
        plOnBoat(pl); \
        MotionMove(pl, 0); \
        break; \
    } \
}

// Player states 0xE / 0x10 / 0x12 (ferry exits): steps 0/1 board from the landing (motion 0x29,
// step SEs, the engine starts at the end), step 2 teleports boat and player to the room's exit
// start position / heading, step 3 drives full ahead (setTillerFront) with the wake effect and a
// water SE every 20 frames while sitting (the room script changes the room).
static void plboat_R2_R10dOut(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_OUT(30.0f, -2490.0f, -14520.0f, -0.05043f);
}

// Room 10E first entrance (see plboat_R2_R10dIn).
static void plboat_R2_R10eIn(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_IN();
}

// Room 10E first exit (see plboat_R2_R10dOut).
static void plboat_R2_R10eOut(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_OUT(36030.0f, -15000.0f, 54920.0f, -1.570221f);
}

// Room 10E second entrance (see plboat_R2_R10dIn).
static void plboat_R2_R10eIn2(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_IN();
}

// Room 10E second exit (see plboat_R2_R10dOut).
static void plboat_R2_R10eOut2(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_OUT(-42720.0f, -15000.0f, 42580.0f, 1.57f);
}

static Vec pl00_swim_cam_pos = { -3500.0f, 2000.0f, -2000.0f };
static Vec pl00_swim_cam_at = { 5000.0f, -500.0f, 500.0f };
static Vec pl00_chase_cam_ofs = { 0.0f, -2000.0f, -40000.0f };
static Camera pl00_drop_camera = { 0 };

// Swim camera set-up: a fixed camera in the boat's frame (3.5 m beside, 2 m up, 2 m behind)
// looking 5 m ahead of the boat; fovy 40.
void pl00SetSwimCam(cPlayer* pl)
{
    Mtx m;

    PSMTXRotRad(m, 'y', pl->m_pBoat->ang.y);
    TransMatrix(m, &pl->m_pBoat->pos);
    PSMTXMultVec(m, &pl00_swim_cam_pos, &pl0f_camera.param.pos);
    PSMTXMultVec(m, &pl00_swim_cam_at, &pl0f_camera.param.at);
    pl0f_camera.param.fovy = 40.0f;
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

// Swim camera per frame (Status_flg[1] bit19 = boat camera active): the set-up's fixed camera;
// once the player is dead it stays where it is and looks at the boss's head (parts 7). fovy
// relaxes to 40; handed to CamCtrl.
void pl00SwimCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Camera;
    Vec pos;
    Vec at;

    StaFlagOn(pG, STA_PL_SWIM_CAMERA);
    if ((s16) pG->pl_life <= 0) {
        cEm* boss;

        if (pl->m_pBoat) {
            Pl0fWork* w = PL0F_WK(pl->m_pBoat);   // two insns at jump1: the `boss = 0` arm is not hoisted
            boss = w->pBoss;
        } else {
            boss = 0;
        }
        if (boss) {
            at = boss->getPartsPtr(7)->world;
        } else {
            at = gcam->param.at;
        }
        pos = gcam->param.pos;
        PosToPos(&gcam->param.at, &at, &pl0f_camera.param.at, 1.0f);
        PosToPos(&gcam->param.pos, &pos, &pl0f_camera.param.pos, 1.0f);
    }
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

// Chase camera set-up (the boss closing in on the swimmer): 40 m behind and 2 m below the player,
// looking at him; fovy 40.
void pl00SetChaseCam(cPlayer* pl)
{
    Mtx m;

    pl0f_camera.param.at = pl->pos;
    PSMTXRotRad(m, 'y', pl->ang.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl00_chase_cam_ofs, &pl0f_camera.param.pos);
    pl0f_camera.param.fovy = 40.0f;
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

// Chase camera per frame: closes 260 units per frame on the swimmer (horizontally), looking at
// him; fovy relaxes to 40; handed to CamCtrl.
void pl00ChaseCamMove(cPlayer* pl)
{
    Vec d;

    StaFlagOn(pG, STA_PL_SWIM_CAMERA);
    pl0f_camera.param.at = pl->pos;
    PSVECSubtract(&pl0f_camera.param.at, &pl0f_camera.param.pos, &d);
    d.y = 0.0f;
#line 4642
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 260.0f);
    PSVECAdd(&pl0f_camera.param.pos, &d, &pl0f_camera.param.pos);
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

// Eaten-death camera set-up: straight above the player (23 m) looking down, the up vector along
// his heading; fovy 40.
void pl00SetDieCam(cPlayer* pl)
{
    Mtx m;
    Vec v;

    pl0f_camera.param.pos = pPL->pos;
    pl0f_camera.param.at = pPL->pos;
    pl0f_camera.param.pos.y = 23000.0f;
    pl0f_camera.param.fovy = 40.0f;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXRotRad(m, 'y', pl->ang.y);
    PSMTXMultVecSR(m, &v, &pl0f_camera.Up);
    CAM_SET(pl0f_camera);
}

// Eaten-death camera per frame: 14 m above the player's root parts looking down; handed to CamCtrl.
void pl00DieCamMove(cPlayer* pl)
{
    Mtx m;
    Vec v;
    cModel* p;

    StaFlagOn(pG, STA_PL_SWIM_CAMERA);   // reference store: the pPL load stays below it
    p = pPL->getPartsPtr(0);
    pl0f_camera.param.pos = p->world;
    pl0f_camera.param.at = p->world;
    pl0f_camera.param.pos.y = 14000.0f;
    pl0f_camera.param.fovy = 40.0f;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXRotRad(m, 'y', pl->ang.y);
    PSMTXMultVecSR(m, &v, &pl0f_camera.Up);
    CAM_SET(pl0f_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0f_camera;
}

// Fall-in-the-water camera set-up: one of two random spots beside / behind the player (9 m back
// or 8 m to the side), looking at him; fovy 40.
void pl00SetDropCam(cPlayer* pl)
{
    Vec v;   // frame offset 0: recomputed per call; `m` behind it gets the callee-saved address pseudo
    Mtx m;

    PSMTXRotRad(m, 'y', pl->ang.y);
    TransMatrix(m, &pl->pos);
    if (Rnd() & 1) {
        v.x = 1500.0f;
        v.y = 0.0f;
        v.z = -9000.0f;
    } else {
        v.x = 8000.0f;
        v.y = 0.0f;
        v.z = -4000.0f;
    }
    PSMTXMultVec(m, &v, &pl0f_camera.param.pos);
    pl0f_camera.param.at = pl->pos;
    pl0f_camera.param.fovy = 40.0f;
    pl0f_camera.Up.x = 0.0f;
    pl0f_camera.Up.y = 1.0f;
    pl0f_camera.Up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

// Fall camera per frame (pl00_drop_camera): the set-up position held at the player's height + 1 m,
// looking at him; when the camera point dips under the water surface the underwater effect
// (0x34 group 0xA) and Status_flg[1] bit20 (underwater view) come on (m_Work3 steps 0 -> 1 -> 2),
// and go off again once it comes back up.
void pl00DropCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Camera;
    Vec at;
    Vec pos;
    f32 h;

    at = pl0f_camera.param.pos;
    at.y = pl->pos.y + 1000.0f;
    pos = pl->pos;
    pl00_drop_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &pos, &pl00_drop_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl00_drop_camera.param.pos, 1.0f);
    pl00_drop_camera.Up.x = 0.0f;
    pl00_drop_camera.Up.y = 1.0f;
    pl00_drop_camera.Up.z = 0.0f;
    CAM_SET(pl00_drop_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl00_drop_camera;
    StaFlagOff(pG, STA_WATER_CAMERA);
    if (GetWaterHeight(&at, &h)) {
        switch (pl->m_Work3) {
        case 0:
            if (h > at.y) {
                EstSet(0, -1, 0, 0, 0xF, 0xA, 0, 0x34, pl, 0);
                StaFlagOn(pG, STA_WATER_CAMERA);
                pl->m_Work4 = 10;
                pl->m_Work3++;
            }
            break;
        case 1:
            if (pl->m_Work4) {
                pl->m_Work4--;
            } else {
                StaFlagOn(pG, STA_WATER_CAMERA);
                if (h <= at.y - 300.0f) {
                    pl->m_Work3++;
                }
            }
            break;
        case 2:
            StaFlagOn(pG, STA_WATER_CAMERA);
            if (h <= at.y) {
                EffectEspDelete(0, ESP_CORE_KIND_WATER, pl, 0);
                EffectEspgenDelete(0, ESP_CORE_KIND_WATER, pl);
                EffectEfmDelete(0, ESP_CORE_KIND_WATER, pl);
                StaFlagOff(pG, STA_WATER_CAMERA);
                pl->m_Work3++;
            }
            break;
        }
    }
}

// Puts a harpoon in the player's right hand (parts 10) when he holds none: SetSpear (obj1c) with
// the boat archive's model, the draw SE 8/0.
void plboatSetSpear(cPlayer* pl)
{
    Vec pos;
    Vec rot;

    if (pl->m_pSpear == 0) {
        SndCall(8, 0, &pl->pos, 0xF, 0, 0);
        pos.x = -70.0f;
        pos.y = -30.0f;
        pos.z = -500.0f;
        rot.x = 0.0f;
        rot.y = PI;
        rot.z = 0.0f;
        pl->m_pSpear = (cObjSpear*) SetSpear(EM_ARC(pl, 0x7), EM_ARC(pl, 0x6), &pos, &rot);
        if (pl->m_pSpear) {
            pl->m_pSpear->setParent(pl, 0xA, 0);
        }
    }
}

// Lean blend of the rider: m0 straight, m1 left / m2 right by the sign of the blend rate.
// The straight motion on the player's own work, the lean into m_SubMot as the blend work with
// weight |m_Blend| / 256; m_Hokan is the blend-in counter, m_Frame the frame (wraps at Motion.Seq_frame_num).
void plboatBlendMotSet(cPlayer* pl, void* m0, void* m1, void* m2, int a, int b, int c)
{
    f32 rate = fabsf(pl->m_Blend);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(pl, &pl->Motion, m0, (void*) a, pl->m_Hokan, 4, pl->m_Frame);
    if (pl->m_Blend < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &pl->m_SubMot;
    MotionSetCore(pl, bm, m, (void*) f, pl->m_Hokan, 4, pl->m_Frame);
    pl->Motion.blend = bm;
    bm->Brate = rate * (1.0f / 256.0f);
    if (pl->m_Hokan) {
        pl->m_Hokan--;
    }
    pl->m_Frame++;
    if (pl->m_Frame >= pl->Motion.Seq_frame_num) {
        pl->m_Frame = 0;
    }
}

// The same lean blend for the partner (subMot as the blend work, m_Hokan / m_Frame counters).
void subBlendMotSet(cSubChar* sub, void* m0, void* m1, void* m2, int a, int b, int c)
{
    f32 rate = fabsf(sub->m_Blend);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(sub, &sub->Motion, m0, (void*) a, sub->m_Hokan, 4, sub->m_Frame);
    if (sub->m_Blend < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &sub->subMot;
    MotionSetCore(sub, bm, m, (void*) f, sub->m_Hokan, 4, sub->m_Frame);
    sub->Motion.blend = bm;
    bm->Brate = rate * (1.0f / 256.0f);
    if (sub->m_Hokan) {
        sub->m_Hokan--;
    }
    sub->m_Frame++;
    if (sub->m_Frame >= sub->Motion.Seq_frame_num) {
        sub->m_Frame = 0;
    }
}

// Seats the player on the boat: position from the boat's matrix, the matrix and rotation copied,
// the waist follows the sight.
// Seats the player on the boat: position / matrix / angles from the boat, the motion's root
// translation suppressed (Motion.Mot_flag bit30), and the waist twisted by the harpoon sight rate.
void plOnBoat(cPlayer* pl)
{
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(pl->m_pBoat->mat, &v, &pl->pos);
    PSMTXCopy(pl->m_pBoat->mat, pl->mat);
    pl->ang = pl->m_pBoat->ang;
    pl->Motion.Mot_flag |= 0x40000000;
    pl->m_SubMot.Mot_flag |= 0x40000000;
    pl->Waist->set(pl->m_BoatPlDir, 0.4f);
}

// Finds the living Del Lago (em id 0x2F, x38D == 1) and hooks the boat to it.
// pBoss / pSelf are set, the boat goes to the dragged state (r_no_1 6), the position history is
// filled with the boss position and the bow leash is at least 25 m. Returns 1 when found.
int testSearchEm2f(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    u32 n;

    w->pBoss = 0;
    for (n = 0; n < EmMgr.getArrayNum(); n++) {
        cEm* e = EmMgr.fastAt(n);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x2F && (s16) e->hp > 0 && e->set == 1) {
            Vec v;
            f32 len;
            int i;

            Pl0fNode* n = &w->node[0];   // node pointer kept callee-saved across the calls; `w` itself dies before them

            w->pBoss = e;
            em->r_no_0 = 1;   // plain byte stores: the 6 stays in the loop, the zero is hoisted (an int inline hoists both)
            em->r_no_1 = 6;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            {
                // A second work pointer: cse turns it into a copy of `w` that loop.c hoists (`mr r12, r6`
                // before the loop) and the pSelf store goes through the copy.
                Pl0fWork* w2 = PL0F_WK(em);

                w2->pSelf = em;
            }
            for (i = 0; i < 10; i++) {
                w->Boss_pos[i] = e->pos;
            }
            w->Boss_pos_no = 0;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2000.0f;
            PSMTXMultVec(w->pBoss->mat, &v, &v);
            len = VEC_DISTXZ(&n->wpos, &v);
            if (len > 25000.0f) {
                n->maxLen = len;
            } else {
                n->maxLen = 25000.0f;
            }
            return 1;
        }
    }
    return 0;
}

// Screen position of the harpoon sight from the lean / tilt rates.
void plboatSightCurGet(cPlayer* pl, Vec* out)
{
    out->x = 256.0f - pl->m_BoatPlDir * 488.92398f;
    out->y = 180.0f - pl->m_Blend * 0.390625f;
    out->z = 0.0f;
}

// Draws the harpoon sight cursor (screen effect 0xF/6) at the sight position.
void plboatSightCurMove(cPlayer* pl)
{
    Vec p;

    plboatSightCurGet(pl, &p);
    EstSet(0, -1, &p, 0, 0xF, 6, 0, 0x35, pl, 0);
}

// Throws the held harpoon: the target is 25 m along the camera ray through the sight cursor, the
// harpoon flies from the hand towards it at 2000 units/frame (+50 up) with the throw SE; the
// player's m_pSpear is released (the cObjSpear flies on its own).
void plboatSpearThrow(cPlayer* pl)
{
    cObjSpear* spear = pl->m_pSpear;
    Camera* gcam = &pG->Camera;
    Vec cur;
    Vec dir;
    Vec target;
    Vec hand;
    cModel* p;

    if (spear == 0) {
        return;
    }
    plboatSightCurGet(pl, &cur);
    CamPos2ScrnVec(cur.x, cur.y, &dir);
#line 5166
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 25000.0f);
    PSVECAdd(&gcam->param.pos, &dir, &target);
    hand.x = -70.0f;
    hand.y = -30.0f;
    hand.z = -500.0f;
    p = pl->getPartsPtr(0xA);
    PSMTXMultVec(p->mat, &hand, &hand);
    PSVECSubtract(&target, &hand, &dir);
#line 5177
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 2000.0f);
    dir.y += 50.0f;
    SndCall(8, 1, &p->world, 0xF, 0, 0);
    spear->setThrow(&dir);
    pl->m_pSpear = 0;
}

static u8 pl0f_rope_parts[30] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};
static u8 pl0f_rope_up[30] = {
    0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
};
static u8 pl0f_rope_down[30] = {
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 0xFF,
};

// The anchor rope: a 30-link chain object hung on the boat's parts 5.
// Creates the anchor rope: a 30-link chain object (SetChain, archive 0x23/0x24) simulated as a
// pendulum cloth (parts pl0f_rope_parts, gravity 5, damping 0.9, stretch 0.1) that collides with
// the boat; its end is pinned to the anchor every frame in cPl0f::move.
void pl0fLongRopeSet(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec pos;
    Vec rot;

    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    w->pRope = SetChain(ARC(0x23), ARC(0x24), &pos, &rot);
    if (w->pRope) {
        w->Cloth.Num = 30;
        w->Cloth.pCloth = pl0f_rope_parts;
        w->Cloth.pEm_at = em;
        w->Cloth.WindSin = 0.0f;
        w->Cloth.Move_rate = 0.0f;
        w->Cloth.pParent = pl0f_rope_up;
        w->Cloth.pChild = pl0f_rope_down;
        w->Cloth.Gravity = 5.0f;
        w->Cloth.Rate = 0.9f;
        w->Cloth.Bundle_num = 100;
        w->Cloth.Stretchy = 0.1f;
        w->Cloth.Flag = 0x108;
        w->Cloth.pLeft = 0;
        w->Cloth.pRight = 0;
        w->Cloth.pUpLeft = 0;
        w->Cloth.pUpRight = 0;
        w->Cloth.pMax = 0;
        w->Cloth.pWindSin = 0;
        w->Cloth.pWindRate = 0;
        w->Cloth.pAtset = 0;
        w->Cloth.pGravity = 0;
        w->Cloth.pRate = 0;
        w->Cloth.At_num = 0;
        w->Cloth.pPtbl = 0;   // the last zero store (dying zero register) is issued first
        w->pRope->setChain(&w->Cloth);
        pos.x = 0.0f;
        pos.y = 600.0f;
        pos.z = 2550.0f;
        w->pRope->setParent(em, 0, &pos, 0);
    }
}

// Where the player surfaces after the drop: a random spot around the boat.
// Start of the swim: the boat is teleported to the swim spot of the lake (37460, 63360) facing
// one of two headings, and the player is dropped 15 m (17 / 13 m when the boat and the player are
// both hurt, random) behind it facing the boat.
void pl0fSwimPosSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;
    f32 ang;
    f32 dist;

    v.x = 37460.0f;
    v.y = boat->pos.y;
    v.z = 63360.0f;
    if (Rnd() & 1) {
        ang = -2.0f;
    } else {
        ang = 1.0f;
    }
    ang = fRand1_1() * 0.049087385f + ang;
    boat->setPos(&v, ang);
    dist = 15000.0f;
    if ((s16) boat->hp <= 899) {
        if ((s16) pG->pl_life <= 799) {
            if (Rnd() & 1) {
                dist = 17000.0f;
            } else {
                dist = 19000.0f;
            }
        }
        if ((s16) pG->pl_life <= 399) {
            if (Rnd() & 1) {
                dist = 24000.0f;
            } else {
                dist = 26000.0f;
            }
        }
        if ((s16) pG->pl_life <= 1) {
            if (Rnd() & 1) {
                dist = 26000.0f;
            } else {
                dist = 28000.0f;
            }
        }
    }
    v.x = dist;
    v.y = pl->pos.y;
    v.z = 0.0f;
    PSMTXMultVec(boat->mat, &v, &pl->pos);
    pl->pos_old = pl->pos;
    pl->ang.y = GetXZAngle(&pl->pos, &boat->pos);
    RotMatrix(pl->mat, &pl->ang);
    TransMatrix(pl->mat, &pl->pos);
    RotMatrix(pl->l_mat, &pl->ang);
    TransMatrix(pl->l_mat, &pl->pos);
    ScaleMatrix(pl->l_mat, &pl->scale);
    PSMTXCopy(pl->l_mat, pl->mat);
    EffectEspDelete(0, ESP_CORE_KIND_BOAT, boat, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_BOAT, boat);
    EffectEfmDelete(0, ESP_CORE_KIND_BOAT, boat);
}

// Start of the mouth chance: the boat is teleported to the hiding spot (40000, 40000) facing one
// of two headings, the player onto it, the wake effects killed.
void pl0fHidePosSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;
    f32 ang;

    v.x = 40000.0f;
    v.y = boat->pos.y;
    v.z = 40000.0f;
    if (Rnd() & 1) {
        ang = -2.45f;
    } else {
        ang = 0.59f;
    }
    ang = fRand1_1() * 0.049087385f + ang;   // the inner call into the local first: the vtable lookup follows it
    boat->setPos(&v, ang);
    pl->pos = boat->pos;
    pl->ang.y = boat->ang.y;
    EffectEspDelete(0, ESP_CORE_KIND_BOAT, boat, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_BOAT, boat);
    EffectEfmDelete(0, ESP_CORE_KIND_BOAT, boat);
}

// Boss death: the boat is teleported to the death-scene spot (24250, 92250) facing 2.85 rad, the
// player onto it, the wake effects killed.
void pl0fBossDiePosSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;

    v.x = 24250.0f;
    v.y = boat->pos.y;
    v.z = 92250.0f;
    pl->ang.y = 2.8464928f;
    pl->ang.y = LIMIT_ANGLE(pl->ang.y);
    boat->setPos(&v, pl->ang.y);   // the just-stored member is forwarded: f1 passes straight through
    pl->pos = boat->pos;
    EffectEspDelete(0, ESP_CORE_KIND_BOAT, boat, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_BOAT, boat);
    EffectEfmDelete(0, ESP_CORE_KIND_BOAT, boat);
}

// The anchor object hung on the boat (room 10B only).
void pl0fSetAnchor(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec pos;
    Vec rot;

    if (w->pAnchor == 0 && pG->room_id == 0x10B) {
        pos.x = 0.0f;
        pos.y = 700.0f;
        pos.z = 2300.0f;
        rot.x = 2.3f;
        rot.y = 3.14f;
        rot.z = 0.0f;
        w->pAnchor = SetObj00(ARC(0x25), ARC(0x26), &pos, &rot);
        OyaSetObj00(w->pAnchor, em, 0);
    }
}

// Moves the anchor onto the boss (parts 0x1A) once it is hooked.
void pl0fSetAnchorEm2f(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    if (w->pBoss && w->pAnchor) {
        w->pAnchor->pos.x = 0.0f;
        w->pAnchor->pos.y = 1000.0f;
        w->pAnchor->pos.z = 0.0f;
        w->pAnchor->ang.x = 0.0f;
        w->pAnchor->ang.y = 0.0f;
        w->pAnchor->ang.z = 0.0f;
        OyaSetObj00(w->pAnchor, w->pBoss, 0x1A);
    }
}

// Rope effects while the boss pulls: the rope strain effect left (0x1F) or right (0x1E) of the boat.
// Called every frame from PlBoatMove: without a boss, with its mouth closed (flag bit5 clear) or
// outside its ram (flag bit2 clear) the effects (group 0x36 on the player) are removed and
// anchorEff reset; otherwise the side is picked from the angle to the boss.
void pl0fSetAnchorEm2f(cPlayer* pl)
{
    Pl0fWork* w;
    cEm* boss;
    f32 ang;

    if (pl->m_pBoat == 0) {
        return;
    }
    w = PL0F_WK(pl->m_pBoat);
    boss = w->pBoss;
    if (boss == 0) {
        EffectEspDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        w->Cursor_type = 0;
        return;
    }
    // Two separate ifs: each body is the fall-through of its own test, so cse stores the known-zero `andi.`
    // result (r28) and jump2 merges the two identical bodies; an `||` body starts at a label and gets `li r0,0`.
    if (!(boss->flag & 0x20)) {
        EffectEspDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        w->Cursor_type = 0;
        return;
    }
    if (!(boss->flag & 4)) {
        EffectEspDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        w->Cursor_type = 0;
        return;
    }
    ang = Muku(&pl->pos, &boss->pos, pl->ang.y, PI);
    if (fabsf(ang) < PI / 8) {
        EffectEspDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        EffectEfmDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
        w->Cursor_type = 0;
        return;
    }
    if (ang < 0.0f) {
        if (w->Cursor_type != 1) {
            EffectEspDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
            EffectEfmDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
            w->Cursor_type = 1;
            EstSet(0, -1, 0, 0, 0xF, 0x1F, 0, 0x36, pl, 0);
        }
    } else {
        if (w->Cursor_type != 2) {
            EffectEspDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
            EffectEfmDelete(0, ESP_CORE_KIND_PL0F_CURSOR, pl);
            w->Cursor_type = 2;
            EstSet(0, -1, 0, 0, 0xF, 0x1E, 0, 0x36, pl, 0);
        }
    }
}

// The partner (Ashley is not in the boat; the routines exist for the R10d / R10e entrances). A macro: an
// inline's f32 arguments are expanded at the head of the block (both pool `lis` hoisted above the
// m_Work0 test) and its inlined pool loads lose RTX_UNCHANGING_P (integrate.c), so they would depend on
// the preceding stores through `sub`.
#define SUB_BOAT_SIT(lo, hi) \
{ \
    if (sub->m_Work0) { \
        if (w->Boat_spd < lo) { \
            sub->m_Work0 = 0; \
            MotionSetCore(sub, &sub->Motion, EM_ARC(sub, 0x2C), 0, 5, 5, 0); \
        } \
    } else if (w->Boat_spd > hi) { \
        sub->m_Work0 = 1; \
        sub->pEm->m_Hokan = 5; \
        sub->m_Frame = 0; \
        sub->m_Blend = 0.0f; \
    } \
    if (sub->m_Work0) { \
        sub->m_Blend = sub->m_Blend * 0.9f + pPL->m_Blend * 0.1f; \
        subBlendMotSet(sub, EM_ARC(sub, 0x2E), EM_ARC(sub, 0x2F), EM_ARC(sub, 0x30), 0, 0, 0); \
    } \
    subOnBoat(sub, boat); \
    MotionMove(sub, 0); \
}

// Partner damage-routine handlers (SetSubDamage(boat, fn); the boat pointer sits in the partner's
// pEmCatch): each swaps in the boat archive, damage type 0x1E, runs its r_no_2 steps and restores
// the partner's own archive.
// Boarding (Ashley on the ferry): the step-in motion 0x2B from beside the boat climbing the height
// difference over 20 frames, then step 2 the seated lean blend in step with the boat's rider.
static void subBoatRide()
{
    cSubChar* sub = pSUB;
    cPl0f* boat = (cPl0f*)sub->pEmCatch;
    Pl0fWork* w = PL0F_WK(boat);
    Vec v;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->Motion.Mot_flag &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->Motion, EM_ARC(sub, 0x2B), 0, 5, 5, 0);
        v.x = 984.32f;
        v.y = 500.0f;
        v.z = 731.88f;
        PSMTXMultVec(boat->mat, &v, &v);
        sub->pos.x = v.x;
        sub->pos.z = v.z;
        sub->fWork0 = v.y - sub->pos.y;
        sub->m_Work0 = 20;
        sub->ang.y = boat->ang.y - PI / 2;
        sub->ang.y = LIMIT_ANGLE(sub->ang.y);
        sub->atari.m_flag &= 0xFCFF;
        sub->r_no_2++;
    case 1:
        if (sub->m_Work0) {
            sub->m_Work0--;
        } else {
            f32 dy = sub->fWork0 * 0.1f;

            sub->pos.y += dy;
            sub->fWork0 -= dy;
        }
        if (MotionMove(sub, 0)) {
            sub->r_no_2++;
        } else {
            if (sub->Motion.Seq_frame > 21.7f && sub->Motion.Seq_frame < 22.3f) {
                SndCall(8, 6, &sub->pos, boat->id, 0, 0);
            }
            if (sub->Motion.Seq_frame > 22.7f && sub->Motion.Seq_frame < 23.3f) {
                SndCall(8, 6, &sub->pos, boat->id, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(sub, &sub->Motion, EM_ARC(sub, 0x2C), 0, 5, 5, 0);
        sub->atari.m_flag &= 0xFCFF;
        sub->m_Work0 = 0;
        sub->r_no_2++;
    case 3:
        SUB_BOAT_SIT(50.0f, 30.0f);
        break;
    }
    sub->Motion.Mot_flag &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Getting off at the landing: the step-out motion 0x2D beside the moored boat, then the routine
// ends (she returns to following the player).
static void subBoatGetoff()
{
    cSubChar* sub = pSUB;
    cPl0f* boat = (cPl0f*)sub->pEmCatch;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->Motion.Mot_flag &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->Motion, EM_ARC(sub, 0x2D), 0, 5, 5, 0);
        sub->m_Work0 = 20;
        sub->fWork0 = 100.0f;
        sub->r_no_2++;
    case 1:
        if (sub->m_Work0) {
            sub->m_Work0--;
        } else {
            f32 dy = sub->fWork0 * 0.1f;

            sub->pos.y += dy;
            sub->fWork0 -= dy;
        }
        if (MotionMove(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
    sub->Motion.Mot_flag &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

#define SUB_BOAT_ROOM_IN() \
{ \
    cSubChar* sub = pSUB; \
    cPl0f* boat = (cPl0f*)sub->pEmCatch; \
    Pl0fWork* w = PL0F_WK(boat); \
 \
    sub->subArc = boat->subArc; \
    sub->dmg.m_Timer = 0x1E; \
    sub->Motion.Mot_flag &= ~0x40000000; \
    switch (sub->r_no_2) { \
    case 0: \
        MotionSetCore(sub, &sub->Motion, EM_ARC(sub, 0x2C), 0, 0, 5, 0); \
        sub->atari.m_flag &= 0xFCFF; \
        sub->m_Work0 = 0; \
        sub->r_no_2++; \
    case 1: \
        SUB_BOAT_SIT(50.0f, 30.0f); \
        break; \
    } \
    sub->Motion.Mot_flag &= ~0x40000000; \
    sub->subArc = sub->subArc2; \
}

// Ferry entrances (rooms 10D / 10E): seated on the boat with the lean blend in step with the rider.
static void subBoatR10dIn()
{
    SUB_BOAT_ROOM_IN();
}

// Room 10E first entrance (same as subBoatR10dIn).
static void subBoatR10eIn()
{
    SUB_BOAT_ROOM_IN();
}

// Room 10E second entrance (same as subBoatR10dIn).
static void subBoatR10eIn2()
{
    SUB_BOAT_ROOM_IN();
}

// Seats the partner in the bow (1.2 m ahead of the boat origin) facing backwards, the motion's
// root translation suppressed.
void subOnBoat(cSubChar* sub, cPl0f* boat)
{
    Mtx m;
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1217.69f;
    PSMTXMultVec(boat->mat, &v, &sub->pos);
    PSMTXCopy(boat->mat, sub->mat);
    PSMTXRotRad(m, 'y', PI);
    PSMTXConcat(sub->mat, m, sub->mat);
    TransMatrix(sub->mat, &sub->pos);
    sub->ang = boat->ang;
    sub->ang.y += PI;
    sub->ang.y = LIMIT_ANGLE(sub->ang.y);
    sub->Motion.Mot_flag |= 0x40000000;
}

// Room script (the Del Lago fight starts): hooks the boat to the boss (testSearchEm2f), moves the
// anchor onto it, places boat and player at `p` / `ang`, and puts the player into the sitting
// state (routine 0xF state 2) with the tiller hand and hidden weapon, the boat into dragged (6).
void cPl0f::setBossStart(Vec* p, f32 ang)
{
    if (testSearchEm2f(this)) {
        cPlayer* pl;

        pl0fSetAnchorEm2f(this);
        setPos(p, ang);
        pPL->ang.y = ang;   // scalar-reference store: pPL is reloaded for the pos copy
        pPL->pos = pos;
        EffectEspDelete(0, ESP_CORE_KIND_BOAT, this, 0);
        EffectEspgenDelete(0, ESP_CORE_KIND_BOAT, this);
        EffectEfmDelete(0, ESP_CORE_KIND_BOAT, this);
        EmRoutineSet(pPL, 0, 0xF, 2, 0);
        EmRoutineSet(this, 1, 6, 0, 0);
        pl = pPL;
        pl->Body->initWepHand((u32) PL_ARC_PTR(subArc, 8));
        pl->setRightHand(1);
        pl->Wep->setTrans(0, 0);
    }
}

// Room script: stops the engine SE.
void cPl0f::stopEngine()
{
    SndStop(PL0F_WK(this)->Seid_engine, 0);
}
