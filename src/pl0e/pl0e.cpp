// pl0e module (D:/Bio4/Prog/pl0e.cpp): the jet ski of the chase. A cEm the room script puts on a rail
// path (setRail / set2ndRail) and the player rides (setRide): pl0ePathMove follows the path with the
// stick steering the lateral offset, the player and partner routines (PlBoatMove / plboat_R2_*,
// subBoat*) ride on it, pl0eCamMove drives the camera, pl0eWaveMove the wave object under it.
//
// The ski is an enemy work (Pl0eInit is the module's EmInitFunc; the room script creates it and
// calls setRail / setRide / set2ndRail). Its routines: r_no_0 0 init, 1 move (r_no_1: 0 wait on
// the water, 1 the boarding cutscene motion, 2 rail run, 3 jump, 4 crash, 5 sink, 6 jump miss;
// 4..6 end the game with pl_life = 0 and DiedemoExec). While ridden the player runs routine 1 ==
// 0xF (pl_R1_Boat -> BoatMoveFunc = PlBoatMove, r_no_2 = the plboat_R2_* state mirroring the
// ski's r_no_1) and the partner's damage routine slot (SetSubDamage) runs the matching subBoat*
// function; both take their motions from the ski's archive (subArc) and are seated by plOnJet /
// subOnJet. Speeds are units per frame along the path: pl0e_spd_max 800 (idle), 1440 boosting
// (up on the stick), 600 braking; the 2nd rail drains `sink` by the speed deficit until the ski
// goes under.

#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "pl0e.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_body.h"
#include "pl_wep.h"
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

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, appended to .bss by snmakerel.
asm(".comm common_pl0e,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);              // game/em.cpp
extern void (*BoatMoveFunc)(cPlayer* pl);        // game/player.cpp (pl_R1_Boat calls it)
extern "C" void Em_R0_Scenario(cEm* em);                    // game/em_sub.cpp
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// COMPILER-DIFF: #1 (argument-move order): pl0ePathMove issues `lwz r4, pRailObj` before `lfs f1, dist`;
// the model-before-dist redeclaration is ABI-identical (GPR / FPR argument registers are numbered independently).
int PathGetPosEmM(void* path, cModel* model, f32 dist, u16* seg, Vec* out) asm("PathGetPosEm");   // MotionMove called with a second argument (pl_npc.cpp)

#line 1 "D:/Bio4/Prog/pl0e.cpp"

typedef void (*Pl0eFunc)(cPl0e*);
typedef void (*PlBoatFunc)(cPlayer*);

static void pl0e_R0_Init(cPl0e* em);
static void pl0e_R0_Move(cPl0e* em);
static void pl0e_R1_Wait(cPl0e* em);
static void pl0e_R1_Ride(cPl0e* em);
static void pl0e_R1_RailMove(cPl0e* em);
static void pl0e_R1_Jump(cPl0e* em);
static void pl0e_R1_Crash(cPl0e* em);
static void pl0e_R1_Sink(cPl0e* em);
static void pl0e_R1_JumpMiss(cPl0e* em);
static void PlBoatMove(cPlayer* pl);
static void plboat_R2_Ride(cPlayer* pl);
static void plboat_R2_Move(cPlayer* pl);
static void plboat_R2_Jump(cPlayer* pl);
static void plboat_R2_Landing(cPlayer* pl);
static void plboat_R2_Crash(cPlayer* pl);
static void plboat_R2_Sink(cPlayer* pl);
static void plboat_R2_JumpMiss(cPlayer* pl);
static void subBoatRide();
static void subBoatRun();
static void subBoatJump();
static void subBoatLanding();
static void subBoatCrash();
static void subBoatSink();
static void subBoatJumpMiss();

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define SUBARC(no) PL_ARC_PTR(sub->subArc, no)
#define PLARC(no) PL_ARC_PTR(pl->subArc, no)
#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))
#define PL_BOAT(pl) ((cPl0e*) (pl)->m_pBoat)

// Store through a reference: a scalar (non-struct) MEM, so a following global load stays below it.
static inline void PSet(void*& d, void* v) { d = v; }
// Read through a reference: a MEM with neither the struct nor the scalar flag stays below preceding member stores.
static inline f32 FRef(f32& v) { return v; }
static inline void U16And(u16& d, u16 m) { d &= m; }
static inline void ISet(int& d, int v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }

struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)
static inline GlobalWork* GRef(GlobalWork*& p) { return p; }

// Struct-member view of pPL: the load stays below a preceding store through a work pointer.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->r_no_0 = r0;
    em->r_no_1 = r1;
    em->r_no_2 = r2;
    em->r_no_3 = r3;
}

// Speed towards a limit by 25 per frame: from above it falls, from below it rises, never crossing it.
#define SPD_ADJUST(spd, lim)                    \
    if ((spd) > (lim)) {                        \
        (spd) -= 25.0f;                         \
        if ((spd) < (lim)) (spd) = (lim);       \
    } else {                                    \
        (spd) += 25.0f;                         \
        if ((spd) > (lim)) (spd) = (lim);       \
    }

f32 pl0e_spd_max = 800.0f;
static f32 pl0e_spd_boost = 1440.0f;
static f32 pl0e_spd_slow = 600.0f;

static Pl0eFunc Pl0e_R0_move_tbl[5] = {
    pl0e_R0_Init,
    pl0e_R0_Move,
    0,
    0,
    (Pl0eFunc) Em_R0_Scenario,
};

static Pl0eFunc Pl0e_R1_move_tbl[7] = {
    pl0e_R1_Wait,
    pl0e_R1_Ride,
    pl0e_R1_RailMove,
    pl0e_R1_Jump,
    pl0e_R1_Crash,
    pl0e_R1_Sink,
    pl0e_R1_JumpMiss,
};

// REL entry: registers the ski constructor as the enemy init function.
extern "C" void _prolog()
{
    OSReport("Pl0e prolog Ok\n");
    EmInitFunc = Pl0eInit;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// EmInitFunc: placement-constructs the ski in the cEm work (cEm::move -> cPl0e::move from then on).
void Pl0eInit(cEm* em)
{
    new (em) cPl0e();
}

// Per-frame update (emMove): clears the frozen-input flag, remembers rot.y for the camera roll,
// runs the r_no_0 routine, counts cnt68 down outside the chase rooms 10D / 10E and moves the wave.
void cPl0e::move()
{
    Pl0eWork* w = PL0E_WK(this);

    w->flags &= ~1;
    w->rotY = ang.y;
    Pl0e_R0_move_tbl[r_no_0](this);
    if (pG->room_id != 0x10D && pG->room_id != 0x10E) {
        if (w->cnt68 == 0) {
            w->cnt68 = 0x1D;
        } else {
            w->cnt68--;
        }
    }
    pl0eWaveMove(this);
}

// Room script: places the ski at `p` facing `ang` (level), recomputes its matrices and kills the
// spray effects (group 0x35) of the old position.
void cPl0e::setPos(Vec* p, f32 ang)
{
    pos = *p;
    pos_old = pos;
    this->ang.y = ang;
    this->ang.x = 0.0f;
    this->ang.z = 0.0f;
    RotMatrix(mat, &this->ang);
    TransMatrix(mat, &pos);
    partsMatCalc();
    partsWorldCalc();
    EffectEspDelete(0, 0x35, (u32) this, 0);
    EffectEspgenDelete(0, 0x35, (int) this);
    EffectEfmDelete(0, 0x35, (int) this);
}

// r_no_0 == 0: creation. Loads the ski model (archive 5/6) with a 2 m light area, no IK, no lock-on
// (EM_STATUS_IK_OFF / LOCKOFF), atari priority 1, the effects (archive 4 as group 0xE), zeroes
// the work, sink = 96000, creates the wave object (SetObj00 from the room archive), remembers the
// effect pull kind, then goes to r_no_0 1 / r_no_1 0 (wait) and runs it this frame.
static void pl0e_R0_Init(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    int zero;

    em->modelInit(ARC(0x5), ARC(0x6));
    em->be_flag &= ~0x10;
    em->ot_type = 0;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 4);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_IK_OFF);
    em->atari.m_flag &= 0xFCFF;
    em->atari.setPriority(PRI_LV1);
    em->setStatus(EM_STATUS_LOCKOFF);
    EspDataLoad((u32) ARC(0x4), 0xE, 0);
    w->flags = zero;
    w->cnt68 = 0x1D;
    w->sink = 96000.0f;
    // pRailObj / spdX are written LAST: they are the last uses of the shared zero (r28) and 0.0 (f31),
    // so sched1 issues them first (dying source) and the target's block order comes out (weight model).
    w->roll = 0.0f;
    w->pitch = 0.0f;
    w->rollPhase = 0.0f;
    w->pitchPhase = 0.0f;
    w->x54 = 0.0f;
    w->x78 = zero;
    w->x71 = zero;
    w->x72 = zero;
    w->swayAmp.x = 0.0f;
    w->swayAmp.y = 0.0f;
    w->swayAmp.z = 0.0f;
    w->swayPhase.x = 0.0f;
    w->swayPhase.y = 0.0f;
    w->swayPhase.z = 0.0f;
    w->xD8 = 0.0f;
    w->camRate = 0.0f;
    w->jumpCnt = zero;
    w->seNo = zero;
    w->pitch104 = zero;
    w->pPath = 0;
    w->ofs.x = 0.0f;
    w->ofs.y = 0.0f;
    w->ofs.z = 0.0f;
    w->pRailObj = 0;
    w->spdX = 0.0f;
    w->floorY0 = em->pos.y;
    w->floorY1 = em->pos.y;
    w->pWave = SetObj00((void*) (pGS->pArc->ofs_20 + (u32) pGS->pArc), (void*) (pGS->pArc->ofs_24 + (u32) pGS->pArc), 0, 0);
    w->espKind = EspPullCoreKind();
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    em->r_no_0 = 1;
    pl0e_R0_Move(em);
}

// r_no_0 == 1: dispatches the r_no_1 state.
static void pl0e_R0_Move(cPl0e* em)
{
    Pl0e_R1_move_tbl[em->r_no_1](em);
}

// r_no_1 == 0: waiting on the water before the ride: floats on the floor / water height, bobs
// (pl0eBoatControl) and checks the player's boarding action.
static void pl0e_R1_Wait(cPl0e* em)
{
    em->pos.y = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    pl0eBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    pl0eRideActEvtCk(em);
}

// r_no_1 == 1 (setRide): the boarding motion 0xF played at the origin (the motion carries the
// world placement) with the launch effect; at its end the ski is put at the chase start
// (y -26663, heading 2.2), the engine SE 8/0xA starts and -> rail run (r_no_1 2).
static void pl0e_R1_Ride(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->ang.x = 0.0f;
        em->ang.y = 0.0f;
        em->ang.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0xF), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0xE, 0xA, 1, w->espKind, (u32) em, 0);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->pos.y = -26663.0f;
            em->ang.y = 2.2f;
            w->seNo = SndCall(8, 0xA, &em->pos, em->id, 0, em);
            EffectEspDelete(1, w->espKind, (u32) em, 0);
            EffectEspgenDelete(1, w->espKind, (int) em);
            EffectEfmDelete(1, w->espKind, (int) em);
            PlRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em->partsWorldCalc();
}

// r_no_1 == 2: the rail run. Step 0 resets the lean blend; step 1 follows the path
// (pl0ePathMove), rides the water (pl0eSlopeControl) and checks, in order, a wall crash (-> 4),
// sinking (-> 5), a fall-off area (-> 6) and a jump ramp (-> 3 with spdY 200); the first three
// kill the player (pl_life = 0). The lean blendRate follows the stick left / right (+-31.875
// per frame up to +-255, decays 0.9) and drives the straight / left / right idle blend 8 / 0xA / 9.
static void pl0e_R1_RailMove(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->hokan = 0;
        w->frame = 0;
        w->frameOld = 0;
        w->blendRate = 0.0f;
        w->spdY = 0.0f;
        em->r_no_2++;
    case 1:
        pl0ePathMove(em, 0);
        pl0eSlopeControl(em);
        if (pl0eCrashCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 4, 0, 0);
        } else if (pl0eSinkCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 5, 0, 0);
        } else if (pl0eJumpMissCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 6, 0, 0);
        } else if (pl0eJumpCk(em)) {
            w->spdY = 200.0f;
            PlRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    if (Key.on & 0xC) {
        if (Key.on & 0x8) {
            w->blendRate += 31.875f;
            if (w->blendRate > 255.0f) {
                w->blendRate = 255.0f;
            }
        }
        if (Key.on & 0x4) {
            w->blendRate -= 31.875f;
            if (w->blendRate < -255.0f) {
                w->blendRate = -255.0f;
            }
        }
    } else {
        w->blendRate *= 0.9f;
    }
    w->frameOld = w->frame;
    pl0eBlendMotSet(em, ARC(0x8), ARC(0xA), ARC(0x9), 0, 0, 0);
    MotionMoveF(em, 0);
    em->partsWorldCalc();
}

// r_no_1 == 3: the jump. Step 0 picks the jump motion (0xB plain; with both shoulder buttons a
// trick: 0xE the first time, 0x14 after flags bit3) and puts the player (routine 0xF state 2,
// m_Work0 = variant) and the partner (subBoatJump, r_no_3 = variant) into their jump, SE 8/8,
// vibration, engine SE stops after 5 frames. Step 1 flies with the rail input frozen (flags bit0)
// until the ski has fallen and is nearly level again; step 2 sets up the landing (player state 3,
// subBoatLanding, splash effect, SE, engine SE restarted); step 3 plays the landing blend
// (0xC / 0x11 / 0x10) with the rail checks and returns to the rail run at its end.
static void pl0e_R1_Jump(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->jumpCnt++;
        if ((Joy[0].on & 0x60) == 0x60) {
            if (w->flags & 8) {
                MotionSetCore(em, &em->Motion, ARC(0x14), 0, 0xA, 1, 0);
                PlRoutineSet(pPL, 0, 0xF, 2, 0);
                U32Set(pPL->m_Work0, 2);
                if (pSUB) {
                    SetSubDamage(em, (void*) subBoatJump);
                    pSUB->r_no_3 = 2;
                }
            } else {
                MotionSetCore(em, &em->Motion, ARC(0xE), 0, 0xA, 1, 0);
                PlRoutineSet(pPL, 0, 0xF, 2, 0);
                U32Set(pPL->m_Work0, 1);
                if (pSUB) {
                    SetSubDamage(em, (void*) subBoatJump);
                    pSUB->r_no_3 = 1;
                }
            }
        } else {
            MotionSetCore(em, &em->Motion, ARC(0xB), 0, 0xA, 1, 0);
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
            U32Set(pPL->m_Work0, 0);
            if (pSUB) {
                SetSubDamage(em, (void*) subBoatJump);
            }
        }
        w->flags |= 8;
        SndCall(8, 8, &em->pos, em->id, 0, em);
        VibSetData(VIB_TBL, 7, 1);
        w->timer = 5;
        w->fall = 0;
        em->r_no_2++;
    case 1:
        w->flags |= 1;
        pl0ePathMove(em, 1);
        pl0eSlopeControl(em);
        if (pl0eJumpCk(em)) {
            w->spdY = 200.0f;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                SndStop(w->seNo, 0);
            }
        }
        if (w->spdY < -15.0f) {
            w->fall = 1;
        }
        if (w->fall && w->spdY > -15.0f) {
            em->r_no_2++;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 2:
        w->hokan = 0;
        w->blendRate = 0.0f;
        w->frame = 0;
        w->frameOld = 0;
        PlRoutineSet(pPLS, 0, 0xF, 3, 0);
        if (pSUB) {
            SetSubDamage(em, (void*) subBoatLanding);
        }
        if (em->be_flag & 2) {
            EstSet((int) em, -1, 0, 0, 0xE, 4, 0, 0, (u32) em, 0);
        }
        SndCall(8, 9, &em->pos, em->id, 0, em);
        VibSetData(VIB_TBL, 7, 1);
        w->seNo = SndCall(8, 0xA, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 3:
        pl0ePathMove(em, 0);
        pl0eSlopeControl(em);
        w->frameOld = w->frame;
        pl0eBlendMotSet(em, ARC(0xC), ARC(0x11), ARC(0x10), 0, 0, 0);
        if (MotionMoveF(em, 0)) {
            PlRoutineSet(em, 1, 2, 0, 0);
        } else if (pl0eCrashCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 4, 0, 0);
        } else if (pl0eSinkCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 5, 0, 0);
        } else if (pl0eJumpMissCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 6, 0, 0);
        } else if (pl0eJumpCk(em)) {
            w->spdY = 200.0f;
            PlRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em->partsWorldCalc();
}

// r_no_1 == 4: the wall crash (game over): motion 0xD with the crash effect (0xB on the 2nd rail,
// else 5), the player into routine 0xF state 4 and the partner into subBoatCrash, engine SE off,
// the crash stream 0x38 and vibration; then the motion plays out.
static void pl0e_R1_Crash(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0xD), 0, 3, 1, 0);
        if (w->flags & 2) {
            EstSet((int) em, -1, 0, 0, 0xE, 0xB, 0, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0xE, 5, 0, 0, (u32) em, 0);
        }
        PlRoutineSet(pPL, 0, 0xF, 4, 0);
        if (pSUB) {
            SetSubDamage(em, (void*) subBoatCrash);
        }
        SndStop(w->seNo, 0);
        SndStrReq(1, 0x38, 0x80000003, 0, 0, 0.0f);
        VibSetData(VIB_TBL, 0xD, 1);
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
    em->partsWorldCalc();
}

// r_no_1 == 5: the ski sinks (game over, `sink` ran out on the 2nd rail): the sinking motion 0x12
// at the origin with effect 0xC, flags bit2, the death demo 0x1E, the player into routine 0xF
// state 5 and the partner into subBoatSink, engine off, stream 0x39; a vibration at frame 18.
static void pl0e_R1_Sink(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->ang.x = 0.0f;
        em->ang.y = 0.0f;
        em->ang.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x12), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0xE, 0xC, 0, 0, (u32) em, 0);
        w->flags |= 4;
        pGS->pl_life = 0;
        DiedemoExec(0x1E, 0);
        w->xD4 = 0x14;
        PlRoutineSet(pPLS, 0, 0xF, 5, 0);
        if (pSUB) {
            SetSubDamage(em, (void*) subBoatSink);
        }
        SndStop(w->seNo, 0);
        SndStrReq(1, 0x39, 0x80000003, 0, 0, 0.0f);
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if (em->frame > 17.7f && em->frame < 18.3f) {
            VibSetData(VIB_TBL, 0xD, 1);
        }
        break;
    }
    em->partsWorldCalc();
}

// r_no_1 == 6: the missed jump (game over, fell into a bit-13 area): the fall motion 0x13 at the
// origin with effect 0xD, the death demo 0x1E, the player into routine 0xF state 6 and the
// partner into subBoatJumpMiss, engine off, stream 0x72; a vibration at frame 33.
static void pl0e_R1_JumpMiss(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->ang.x = 0.0f;
        em->ang.y = 0.0f;
        em->ang.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x13), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0xE, 0xD, 0, 0, (u32) em, 0);
        pG->pl_life = 0;
        DiedemoExec(0x1E, 0);
        w->xD4 = 0x14;
        PlRoutineSet(pPLS, 0, 0xF, 6, 0);
        if (pSUB) {
            SetSubDamage(em, (void*) subBoatJumpMiss);
        }
        SndStop(w->seNo, 0);
        SndStrReq(1, 0x72, 0x80000003, 0, 0, 0.0f);
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if (em->frame > 32.7f && em->frame < 33.3f) {
            VibSetData(VIB_TBL, 0xD, 1);
        }
        break;
    }
    em->partsWorldCalc();
}

// Waiting-state placement: applies the ofsF0 offset in the ski's frame to pos, rebuilds l_mat /
// mat from ang / pos / scale, pushes the ski out of the scenery (pl0eScrAdjust) and adds the
// heading / roll / pitch bobbing.
void pl0eBoatControl(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec v;

    PSMTXRotRad(em->mat, 'y', em->ang.y);
    PSMTXMultVec(em->mat, &w->ofsF0, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    TransMatrix(m, &em->pos);
    RotMatrix(em->l_mat, &em->ang);
    TransMatrix(em->l_mat, &em->pos);
    ScaleMatrix(em->l_mat, &em->scale);
    PSMTXCopy(em->l_mat, em->mat);
    pl0eScrAdjust(em);
    pl0eGetBoatDir(em);
    pl0eBoatRoll(em);
}

// Movement direction of the frame: spdXZ = the XZ distance moved since pos_old; when moving
// faster than 100 units dirAng = the heading change towards the movement direction (Muku2),
// else it decays by 0.9; dirAngAbs = |dirAng|.
void pl0eGetBoatDir(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Vec d;

    PSVECSubtract(&em->pos, &em->pos_old, &d);
    w->spdXZ = SQRTF(d.x * d.x + d.z * d.z);
    if (w->spdXZ > 100.0f) {
        w->dirAng = atan2f(d.x, d.z);
        w->dirAng = Muku2(em->ang.y, w->dirAng, PI);
    } else {
        w->dirAng *= 0.9f;
    }
    w->dirAngAbs = fabsf(w->dirAng);
}

// Adds the hull attitude to `mat`: a roll towards the turn direction (dirAng * 0.3 scaled by the
// speed, smoothed 0.9/0.1), a nose-up pitch with the speed (up to -0.196 rad) plus a random
// noise wobble (pitchPhase), and the decaying sway set by impacts (swayAmp * sin(swayPhase),
// amplitude * 0.96 per frame).
void pl0eBoatRoll(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec sway;
    f32 t;
    f32 a;

    if (w->dirAngAbs < PI / 2) {
        t = w->spdXZ * 0.01f;
        if (t > 1.0f) {
            t = 1.0f;
        }
        a = w->dirAng * 0.3f * t;
        w->roll = w->roll * 0.9f + a * 0.1f;
    } else {
        w->roll *= 0.9f;
    }
    w->rollPhase += fRand0_1() * 0.3926991f + 0.09817477f;
    sinf(w->rollPhase);
    PSMTXRotRad(m, 'z', w->roll);
    PSMTXConcat(em->mat, m, em->mat);
    if (w->dirAngAbs < PI / 2) {
        a = w->spdXZ * 0.005f;
        if (a > 1.0f) {
            a = 1.0f;
        }
        a *= -0.19634955f;
        w->pitch = w->pitch * 0.9f + a * 0.1f;
    } else {
        w->pitch *= 0.9f;
    }
    a = w->pitch;
    w->pitchPhase += fRand0_1() * 0.3926991f + 0.09817477f;
    PSMTXRotRad(m, 'x', sinf(w->pitchPhase) * 0.012271847f + a);
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

static Camera pl0e_camera = { 0 };
static Vec pl0e_cam_ofs = { 0.0f, 0.0f, 5000.0f };
static f32 pl0e_cam_up = 1500.0f;
static f32 pl0e_cam_dist = 5000.0f;
static Vec pl0e_cam_pos0 = { -1500.0f, 0.0f, -3000.0f };
static Vec pl0e_cam_pos1 = { -1500.0f, 0.0f, -5000.0f };

// The chase camera (called from the player's boat states while riding; skipped while the player
// has stat bit2): looks at the path point 15 m ahead (or 5 m ahead of the ski), sits 5 m
// behind on the line ski -> target blended with a fixed offset (pl0e_cam_pos0 -> pos1 by camRate,
// which rises when boosting), 1.5 m up; fovy relaxes to 40, the up vector rolls with the heading
// change of the frame (x3); handed to CamCtrl as the extra camera.
void pl0eCamMove(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec at;
    Vec target;
    Vec dir;

    if (pPL->stat & 4) {
        return;
    }
    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &pl0e_cam_ofs, &target);
    pl0ePathGetTarget(em, &target);
    at = em->pos;
    PSVECSubtract(&at, &target, &dir);
#line 1028
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, pl0e_cam_dist);
    PSVECAdd(&em->pos, &dir, &at);
    if (w->spd > pl0e_spd_max * 0.8f + pl0e_spd_boost * 0.2f) {
        w->camRate = w->camRate * 0.9f + 0.1f;
    } else {
        w->camRate = w->camRate * 0.9f;
    }
    PosToPos(&pl0e_cam_pos0, &pl0e_cam_pos1, &dir, w->camRate);
    PSMTXMultVec(em->mat, &dir, &dir);
    at.x = at.x * 0.5f + dir.x * 0.5f;
    at.z = at.z * 0.5f + dir.z * 0.5f;
    at.y = at.y + pl0e_cam_up;
    PosToPos(&gcam->param.at, &target, &pl0e_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0e_camera.param.pos, 1.0f);
    pl0e_camera.param.fovy = pl0e_camera.param.fovy * 0.9f + 4.0f;
    PSMTXRotRad(m, 'z', -Muku2(w->rotY, em->ang.y, PI) * 3.0f);
    dir.y = 1.0f;
    dir.x = 0.0f;
    dir.z = 0.0f;
    PSMTXMultVecSR(m, &dir, &pl0e_camera.up);
    {
        Vec* cp = &pl0e_camera.param.pos;
        Vec* ca = &pl0e_camera.param.at;

        pl0e_camera.dist = SQRTF((cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z));
    }
    CameraSetOrientationUp(&pl0e_camera);
    CamCtrl.m_pExtraCamera = (s32) &pl0e_camera;
}

// Boarding check while waiting: with the hands free (Status_flg[1] bit21 clear) and the player
// facing the ski within 45 degrees the height difference is computed and discarded -- the
// result is unused, the boarding itself is triggered by the room script (setRide).
void pl0eRideActEvtCk(cPl0e* em)
{
    u8 unused[6];   // the original frame has 8 unused bytes (a BLKmode local nothing references)

    if (!StaFlagChk(pG, STA_PL_BOAT)) {
        if (!(fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI)) > PI / 4)) {
            fabsf(em->pos.y - pPL->pos.y);
        }
    }
}

// Room script: the player (and partner) board the ski (needs a rail): the ski goes to the boarding
// state (r_no_1 1), becomes the player's m_pBoat, PlBoatMove is installed as BoatMoveFunc and the
// player is put into routine 0xF state 0 (plboat_R2_Ride), the partner into subBoatRide.
void cPl0e::setRide()
{
    Pl0eWork* w = PL0E_WK(this);
    cPlayer* pl = pPL;

    if (w->pRailObj) {
        r_no_0 = 1;
        r_no_1 = 1;
        r_no_2 = 0;
        r_no_3 = 0;
        // Reference store: the pPL reload of PlRoutineSet then depends on it (cost 2) and is not
        // ready when the BoatMoveFunc store is, so sched1 issues that store first and the
        // PlBoatMove address dies before the reload is born (both r9; the zero takes r10).
        PSet((void*&) pl->m_pBoat, this);
        BoatMoveFunc = PlBoatMove;
        PlRoutineSet(pPL, 0, 0xF, 0, 0);
        if (pSUB) {
            SetSubDamage(this, (void*) subBoatRide);
        }
    }
}

// Pushes the ski out of the scenery: SatMgr.adjust of the pos_old -> pos move with a 600-unit
// radius (attribute mask 0x2081); the horizontal correction is applied to pos. Debug mode 7
// draws the collision sphere.
void pl0eScrAdjust(cPl0e* em)
{
    Vec nrm;
    Vec p;
    Vec d;

    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    p = em->pos;
    if (pG->debug_mode == 7) {
        Draw_sphere(&em->pos, 300.0f, 0xFFFFFFFF, 1, 1);
    }
    SatMgr.adjust(&nrm, &em->pos_old, &p, 600.0f, 0x2081, 0);
    if (!(nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f)) {
        PSVECSubtract(&p, &em->pos, &d);
        d.y = 0.0f;
        if (!(d.x == 0.0f && d.y == 0.0f && d.z == 0.0f)) {
            PSVECAdd(&em->pos, &d, &em->pos);
        }
    }
}

static PlBoatFunc plboat_R2_move_tbl[7] = {
    plboat_R2_Ride,
    plboat_R2_Move,
    plboat_R2_Jump,
    plboat_R2_Landing,
    plboat_R2_Crash,
    plboat_R2_Sink,
    plboat_R2_JumpMiss,
};

// The player's boat routine (pl_R1_Boat -> BoatMoveFunc): the boat's motion archive replaces the
// player's for the duration of the routine.
// Every frame: the sub screen is held (SubScreenWait), Status_flg[1] bit21 (hands busy) set, the
// neck mode 2, the player's atari bits 8/9 off, damage type 0x1E (boat), then the plboat_R2_*
// state of r_no_2; the motion "no root translation" flag (motFlags2 bit30) is cleared around it.
static void PlBoatMove(cPlayer* pl)
{
    if (pl->m_pBoat == 0) {
        pLog->err(0, 0, "PlBoatMove(): m_pBoat == NULL!");
        return;
    }
    SubScreenWait(0xF);
    StaFlagOn(pG, STA_PL_BOAT);
    PlSetNeck(2);
    pl->atari.m_flag &= 0xFCFF;
    pl->dmg.m_Timer = 0x1E;
    pl->subArc = pl->m_pBoat->subArc;
    pl->motFlags2 &= ~0x40000000;
    pl->m_SubMot.flags2 &= ~0x40000000;
    plboat_R2_move_tbl[pl->r_no_2](pl);
    pl->motFlags2 &= ~0x40000000;
    pl->m_SubMot.flags2 &= ~0x40000000;
    pl->subArc = pl->subArc2;
}

// Player boat state 0 (boarding): the boarding motion 0x1D from the ski archive at the origin,
// the ski-riding hands (right: ski archive 0x7, left: player archive 0x15), the weapon hidden;
// at the motion's end -> state 1 (ride).
static void plboat_R2_Ride(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        pl->ang.x = 0.0f;
        pl->ang.y = 0.0f;
        pl->ang.z = 0.0f;
        MotionSetCore(pl, &pl->Motion, PLARC(0x1D), 0, 0, 0x201, 0);
        pl->m_Blend = 0.0f;
        {
            cAtariInfo* at = &pl->atari;
            at->m_flag &= 0xFCFF;
        }
        pl->be_flag |= 0x00200000;
        pl->Body->initWepHand((u32) PLARC(0x7));
        pl->setRightHand(1);
        pl->Body->initWepHand((u32) PL_ARC_PTR(pG->pPlayer, 0x15));
        pl->setLeftHand(1);
        pl->Wep->setTrans(0, 0);
        pl->r_no_3++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 1, 0);
        }
        break;
    }
}

// Player boat state 1 (riding): the straight / left / right lean blend 0x16 / 0x18 / 0x17 driven
// by the ski's blendRate and frame (so rider and ski stay in step), seated by plOnJet; runs the
// chase camera.
static void plboat_R2_Move(cPlayer* pl)
{
    Pl0eWork* w = PL0E_WK(PL_BOAT(pl));

    switch (pl->r_no_3) {
    case 0:
        pl->m_Blend = 0.0f;
        pl->m_Hokan = 0;
        pl->m_Frame = 0;
        pl->r_no_3++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x16), PLARC(0x18), PLARC(0x17), 0, 0, 0);
        pl->m_Blend = w->blendRate;
        pl->m_Frame = (u8) w->frameOld;
        plOnJet(pl);
        MotionMoveF(pl, 0);
        break;
    }
    if (pl->m_pBoat) {
        pl0eCamMove(PL_BOAT(pl));
    }
}

// Player boat state 2 (in the air): the jump motion of the variant in m_Work0 (0 plain 0x19, 1
// trick 0x1C, 2 second trick 0x23), seated by plOnJet, with the chase camera.
static void plboat_R2_Jump(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        switch ((int) pl->m_Work0) {
        case 0:
        default:
            MotionSetCore(pl, &pl->Motion, PLARC(0x19), 0, 0xA, 1, 0);
            break;
        case 1:
            MotionSetCore(pl, &pl->Motion, PLARC(0x1C), 0, 0xA, 1, 0);
            break;
        case 2:
            MotionSetCore(pl, &pl->Motion, PLARC(0x23), 0, 0xA, 1, 0);
            break;
        }
        pl->r_no_3++;
    case 1:
        plOnJet(pl);
        MotionMoveF(pl, 0);
        break;
    }
    if (pl->m_pBoat) {
        pl0eCamMove(PL_BOAT(pl));
    }
}

// Player boat state 3 (landing): the landing lean blend 0x1A / 0x1F / 0x1E (10-frame blend-in)
// in step with the ski; at its end -> state 1.
static void plboat_R2_Landing(cPlayer* pl)
{
    Pl0eWork* w = PL0E_WK(PL_BOAT(pl));

    switch (pl->r_no_3) {
    case 0:
        pl->m_Hokan = 0xA;
        pl->m_Frame = 0;
        pl->m_Blend = 0.0f;
        pl->r_no_3++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x1A), PLARC(0x1F), PLARC(0x1E), 0, 0, 0);
        pl->m_Blend = w->blendRate;
        pl->m_Frame = (u8) w->frameOld;
        plOnJet(pl);
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 1, 0);
        }
        break;
    }
    if (pl->m_pBoat) {
        pl0eCamMove(PL_BOAT(pl));
    }
}

// Player boat state 4 (crash): thrown off to the side the ski is offset to (0x1B right / 0x20
// left), pl_life = 0; the motion plays out.
static void plboat_R2_Crash(cPlayer* pl)
{
    Pl0eWork* w = PL0E_WK(PL_BOAT(pl));

    switch (pl->r_no_3) {
    case 0:
        if (w->ofs.x > 0.0f) {
            MotionSetCore(pl, &pl->Motion, PLARC(0x1B), 0, 3, 1, 0);
        } else {
            MotionSetCore(pl, &pl->Motion, PLARC(0x20), 0, 3, 1, 0);
        }
        plOnJet(pl);
        pG->pl_life = 0;
        pl->r_no_3++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

// Player boat state 5 (sinking): the sinking motion 0x21 at the origin (world placement in the
// motion), pl_life = 0.
static void plboat_R2_Sink(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        pl->ang.x = 0.0f;
        pl->ang.y = 0.0f;
        pl->ang.z = 0.0f;
        MotionSetCore(pl, &pl->Motion, PLARC(0x21), 0, 0, 0x201, 0);
        pl->m_Blend = 0.0f;
        {
            cAtariInfo* at = &pl->atari;
            at->m_flag &= 0xFCFF;
        }
        pGS->pl_life = 0;
        pl->r_no_3++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

// Player boat state 6 (missed jump): the fall motion 0x22 at the origin, pl_life = 0.
static void plboat_R2_JumpMiss(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        pl->ang.x = 0.0f;
        pl->ang.y = 0.0f;
        pl->ang.z = 0.0f;
        MotionSetCore(pl, &pl->Motion, PLARC(0x22), 0, 0, 0x201, 0);
        pl->m_Blend = 0.0f;
        {
            cAtariInfo* at = &pl->atari;
            at->m_flag &= 0xFCFF;
        }
        pGS->pl_life = 0;
        pl->r_no_3++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

// Lean blend of the rider: m0 straight, m1 left / m2 right by the sign of the blend rate.
// The straight motion goes on the player's own work, the lean into m_SubMot as the blend work with
// weight |m_Blend| / 256; m_Hokan is the blend-in counter, m_Frame the frame (wraps at frameMax).
void plboatBlendMotSet(cPlayer* pl, void* m0, void* m1, void* m2, int a, int b, int c)
{
    f32 rate = fabsf(pl->m_Blend);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(pl, &pl->Motion, m0, a, pl->m_Hokan, 4, pl->m_Frame);
    if (pl->m_Blend < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &pl->m_SubMot;
    MotionSetCore(pl, bm, m, f, pl->m_Hokan, 4, pl->m_Frame);
    pl->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (pl->m_Hokan) {
        pl->m_Hokan--;
    }
    pl->m_Frame++;
    if (pl->m_Frame >= pl->frameMax) {
        pl->m_Frame = 0;
    }
}

// The same lean blend for the partner: straight on her own work, the lean (by the sign of
// m_Blend) into subBackMot with weight |m_Blend| / 256; m_Hokan / m_Frame are the blend counter / frame.
void subBlendMotSet(cSubChar* sub, void* m0, void* m1, void* m2, int a, int b, int c)
{
    f32 rate = fabsf(sub->m_Blend);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(sub, &sub->Motion, m0, a, sub->m_Hokan, 4, sub->m_Frame);
    if (sub->m_Blend < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &sub->subBackMot;
    MotionSetCore(sub, bm, m, f, sub->m_Hokan, 4, sub->m_Frame);
    sub->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (sub->m_Hokan) {
        sub->m_Hokan--;
    }
    sub->m_Frame++;
    if (sub->m_Frame >= sub->frameMax) {
        sub->m_Frame = 0;
    }
}

// Seats the player on the ski: position from the ski's matrix, the matrix and rotation copied.
void plOnJet(cPlayer* pl)
{
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(pl->m_pBoat->mat, &v, &pl->pos);
    PSMTXCopy(pl->m_pBoat->mat, pl->mat);
    pl->ang = pl->m_pBoat->ang;
    pl->motFlags2 |= 0x40000000;
    pl->m_SubMot.flags2 |= 0x40000000;
}

// Partner damage-routine handlers (SetSubDamage(boat, fn) installs them; the boat pointer sits in
// the partner's pEmCatch): each swaps in the ski archive, damage type 0x1E, runs its r_no_2 steps
// and restores the partner's own archive.
// Boarding: the motion 0x2C at the origin with two step SEs; at its end -> subBoatRun.
static void subBoatRide()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        sub->pos.x = 0.0f;
        sub->pos.y = 0.0f;
        sub->pos.z = 0.0f;
        sub->ang.x = 0.0f;
        sub->ang.y = 0.0f;
        sub->ang.z = 0.0f;
        MotionSetCore(sub, &sub->Motion, SUBARC(0x2C), 0, 0, 1, 0);
        sub->be_flag |= 0x00200000;
        {
            cAtariInfo* at = &sub->atari;
            at->m_flag &= 0xFCFF;
        }
        sub->r_no_2++;
    case 1:
        if (MotionMoveF(sub, 0)) {
            SetSubDamage(boat, (void*) subBoatRun);
        } else {
            if (sub->frame > 21.7f && sub->frame < 22.3f) {
                SndCall(5, 0x16, &sub->pos, 0, 0, 0);
            }
            if (sub->frame > 22.7f && sub->frame < 23.3f) {
                SndCall(5, 0x17, &sub->pos, 0, 0, 0);
            }
        }
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Riding: seated behind the player (subOnJet) with the lean blend 0x25 / 0x27 / 0x26 driven by
// the ski's blendRate / frame.
static void subBoatRun()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;
    Pl0eWork* w = PL0E_WK(boat);

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        sub->atari.m_flag &= 0xFCFF;
        sub->m_Blend = 0.0f;
        sub->m_Hokan = 0;
        sub->m_Frame = 0;
        sub->r_no_2++;
    case 1:
        sub->m_Blend = w->blendRate;
        sub->m_Frame = (u8) w->frameOld;
        subOnJet(sub, boat);
        subBlendMotSet(sub, SUBARC(0x25), SUBARC(0x27), SUBARC(0x26), 0, 0, 0);
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// In the air: the jump motion of the variant in r_no_3 (0 plain 0x28, 1 trick 0x2B, 2 second
// trick 0x31), seated on the ski.
static void subBoatJump()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        switch (sub->r_no_3) {
        case 0:
        default:
            MotionSetCore(sub, &sub->Motion, SUBARC(0x28), 0, 0xA, 1, 0);
            break;
        case 1:
            MotionSetCore(sub, &sub->Motion, SUBARC(0x2B), 0, 0xA, 1, 0);
            break;
        case 2:
            MotionSetCore(sub, &sub->Motion, SUBARC(0x31), 0, 0xA, 1, 0);
            break;
        }
        sub->atari.m_flag &= 0xFCFF;
        sub->r_no_2++;
    case 1:
        subOnJet(sub, boat);
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Landing: the landing lean blend 0x29 / 0x2E / 0x2D (10-frame blend-in) in step with the ski;
// at its end -> subBoatRun.
static void subBoatLanding()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;
    Pl0eWork* w = PL0E_WK(boat);

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        sub->atari.m_flag &= 0xFCFF;
        sub->m_Hokan = 0xA;
        sub->m_Frame = 0;
        sub->m_Blend = 0.0f;
        sub->r_no_2++;
    case 1:
        sub->m_Blend = w->blendRate;
        sub->m_Frame = (u8) w->frameOld;
        subOnJet(sub, boat);
        subBlendMotSet(sub, SUBARC(0x29), SUBARC(0x2E), SUBARC(0x2D), 0, 0, 0);
        if (MotionMoveF(sub, 0)) {
            SetSubDamage(boat, (void*) subBoatRun);
        }
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Crash: the crash motion 0x2A, seated on the ski.
static void subBoatCrash()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, &sub->Motion, SUBARC(0x2A), 0, 3, 1, 0);
        sub->atari.m_flag &= 0xFCFF;
        sub->r_no_2++;
    case 1:
        subOnJet(sub, boat);
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Sinking: the motion 0x2F at the origin (world placement in the motion).
static void subBoatSink()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        sub->pos.x = 0.0f;
        sub->pos.y = 0.0f;
        sub->pos.z = 0.0f;
        sub->ang.x = 0.0f;
        sub->ang.y = 0.0f;
        sub->ang.z = 0.0f;
        MotionSetCore(sub, &sub->Motion, SUBARC(0x2F), 0, 0, 1, 0);
        sub->r_no_2++;
    case 1:
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Missed jump: the fall motion 0x30 at the origin.
static void subBoatJumpMiss()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = (cPl0e*)sub->pEmCatch;

    sub->subArc = boat->subArc;
    sub->dmg.m_Timer = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->r_no_2) {
    case 0:
        sub->pos.x = 0.0f;
        sub->pos.y = 0.0f;
        sub->pos.z = 0.0f;
        sub->ang.x = 0.0f;
        sub->ang.y = 0.0f;
        sub->ang.z = 0.0f;
        MotionSetCore(sub, &sub->Motion, SUBARC(0x30), 0, 0, 1, 0);
        sub->r_no_2++;
    case 1:
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

// Seats the partner on the ski: position and matrix from the ski's, the ski's angles, and the
// motion's root translation suppressed (motFlags2 bit30).
void subOnJet(cSubChar* sub, cPl0e* boat)
{
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(boat->mat, &v, &sub->pos);
    PSMTXCopy(boat->mat, sub->mat);
    TransMatrix(sub->mat, &sub->pos);
    sub->ang = boat->ang;
    sub->motFlags2 |= 0x40000000;
}

// Room script: attaches the ski to a rail path: creates the rail object (SetObj00 from the room
// archive) the path positions are evaluated on, resets the distance / segment, speed =
// pl0e_spd_max, length from the path, and seeds the path positions with the current pos.
void cPl0e::setRail(void* path)
{
    Pl0eWork* w = PL0E_WK(this);

    w->pRailObj = SetObj00((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), 0, 0);
    if (w->pRailObj) {
        w->seg = 0;
        w->dist = 0.0f;
        w->pPath = path;
        w->spd = FRef(pl0e_spd_max);
        w->length = PathGetLength(path);
        w->pathPos = pos;
        w->pathPosOld = pos;
    }
}

// One frame along the rail (jump != 0: in the air, no steering / throttle). The path point at
// `dist` is the ski's frame origin; the stick's left / right (Key.on bits 2/3) accelerates the
// lateral speed spdX (+80 / -50 per frame, clamped +-400) which moves ofs.x; a wall (attribute
// 0x80800) between the path and the ski pushes it back 500 units and zeroes spdX. dist advances
// by spd: up on the stick boosts towards pl0e_spd_boost (engine pitch up to 500), down brakes to
// pl0e_spd_slow, else back to pl0e_spd_max (25 per frame). The heading turns towards the path
// point 15 m ahead (0.098 rad per frame). Spray effects (group 0xE: 0 wake, 3 boost, 1/2 the
// side splash with SE 8/0xC on a stick tap), on the 2nd rail `sink` drains by the speed deficit,
// a water-noise SE every 20 frames while alive, and the engine SE doppler pitch is updated.
void pl0ePathMove(cPl0e* em, int jump)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec p;
    Vec a;
    Vec b;
    Vec hit;
    Mtx inv;
    Vec d;
    f32 ang;

    if (w->pRailObj == 0) {
        return;
    }
    w->pathPosOld = w->pathPos;
    PathGetPosEmM(w->pPath, w->pRailObj, w->dist, &w->seg, &w->pathPos);
    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &w->pathPos);
    if ((Key.on & 0xC) && jump == 0) {
        if (Key.on & 0x8) {
            w->spdX += 80.0f;
            if (w->spdX > 400.0f) {
                w->spdX = 400.0f;
            }
        } else {
            w->spdX -= 50.0f;
            if (w->spdX < -400.0f) {
                w->spdX = -400.0f;
            }
        }
    } else {
        w->spdX *= 0.87f;
    }
    w->ofs.x += w->spdX;
    PSMTXMultVec(m, &w->ofs, &p);
    em->pos.x = p.x;
    em->pos.z = p.z;
    if ((w->pathPos.x - p.x) * (w->pathPos.x - p.x) + (w->pathPos.z - p.z) * (w->pathPos.z - p.z) > 10000.0f) {
        PSVECSubtract(&em->pos, &w->pathPos, &d);
        d.y = 0.0f;
#line 2209
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 500.0f);
        a = w->pathPos;
        b = em->pos;
        a.y = em->pos.y;
        PSVECAdd(&b, &d, &b);
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x80800)) {
            PSVECSubtract(&a, &b, &d);
#line 2221
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 500.0f);
            em->pos.x = hit.x + d.x;
            em->pos.z = hit.z + d.z;
            PSMTXInverse(m, inv);
            PSMTXMultVec(inv, &em->pos, &w->ofs);
            w->ofs.y = 0.0f;
            w->ofs.z = 0.0f;
            w->spdX = 0.0f;
        }
    }
    w->dist += w->spd;
    if (jump != 0) {
        if (w->jumpCnt <= 1) {
            SPD_ADJUST(w->spd, FRef(pl0e_spd_max));
        }
    } else {
        if (Key.on & 3) {
            if (Key.on & 1) {
                w->spd += 25.0f;
                if (w->spd > FRef(pl0e_spd_boost)) {
                    w->spd = FRef(pl0e_spd_boost);
                }
                w->pitch104 += 4;
                if (w->pitch104 > 500) {
                    w->pitch104 = 500;
                }
            } else {
                SPD_ADJUST(w->spd, FRef(pl0e_spd_slow));
                if (w->pitch104 > 4) {
                    w->pitch104 -= 4;
                } else {
                    w->pitch104 = 0;
                }
            }
        } else {
            SPD_ADJUST(w->spd, FRef(pl0e_spd_max));
            if (w->pitch104 > 4) {
                w->pitch104 -= 4;
            } else {
                w->pitch104 = 0;
            }
        }
    }
    if (w->dist >= w->length) {
        w->dist = w->length;
    }
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 15000.0f;
    PSMTXMultVec(m, &p, &p);
    pl0ePathGetTarget(em, &p);
    ang = GetXZAngle(&w->pathPos, &p);
    em->ang.y += Muku2(em->ang.y, ang, 0.09817477f);
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    if (!(w->flags & 1) && (em->be_flag & 2)) {
        if (w->spd > 100.0f) {
            EstSet((int) em, -1, 0, 0, 0xE, 0, 0, 0, (u32) em, 0);
        }
        if (w->spd > pl0e_spd_max + 100.0f) {
            EstSet((int) em, -1, 0, 0, 0xE, 3, 0, 0, (u32) em, 0);
        }
        if (Key.trg & 8) {
            EstSet((int) em, -1, 0, 0, 0xE, 1, 0, 0, (u32) em, 0);
            SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
        if (Key.trg & 4) {
            EstSet((int) em, -1, 0, 0, 0xE, 2, 0, 0, (u32) em, 0);
            SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
    }
    if (w->flags & 2) {
        w->sink -= pl0e_spd_boost - w->spd;
        if (w->sink <= 0.0f) {
            w->sink = 0.0f;
        }
    }
    if (!(w->flags & 1) && (s16) pG->pl_life > 0) {
        static u8 cnt = 0;

        cnt++;
        if (cnt % 20 == 0) {
            SndCall(8, 0x12, &em->pos, em->id, 0, em);
        }
    }
    SndSetDopPitch(w->seNo, (s16) w->pitch104);
}

// Room script: the second half of the chase (the collapsing tunnel): flags bit1 (sink drain on),
// the engine SE restarted, sink refilled, the path distance skipped 50 m ahead, the lateral offset
// reset, straight into the rail run with the player (routine 0xF state 1) and partner
// (subBoatRun) riding; the wave object gets its effect 9.
void cPl0e::set2ndRail()
{
    Pl0eWork* w = PL0E_WK(this);

    pPL->m_pBoat = this;
    w->flags |= 2;
    SndStop(w->seNo, 0);
    w->seNo = SndCall(8, 0xA, &pos, id, 0, this);
    w->sink = 96000.0f;
    w->dist += 50000.0f;
    w->ofs.x = 0.0f;
    w->ofs.y = 0.0f;
    w->ofs.z = 0.0f;
    w->spdX = 0.0f;
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 1, 0);
    if (pSUB) {
        SetSubDamage(this, (void*) subBoatRun);
    }
    if (w->pWave) {
        EstSet((int) w->pWave, -1, 0, 0, 0xE, 9, 1, 0, (u32) w->pWave, 0);
    }
}

// Room script: stops the engine SE.
void cPl0e::stopEngine()
{
    SndStop(PL0E_WK(this)->seNo, 0);
}

// Path position 15000 ahead of the ski (x / z only), when it is still on the path.
void pl0ePathGetTarget(cPl0e* em, Vec* out)
{
    Pl0eWork* w = PL0E_WK(em);
    Vec p;
    u16 seg[1];
    f32 d;

    if (w->pRailObj) {
        d = w->dist + 15000.0f;
        if (!(d >= w->length)) {
            seg[0] = 0;
            PathGetPosEm(w->pPath, d, w->pRailObj, seg, &p);
            out->x = p.x;
            out->z = p.z;
        }
    }
}

// Gravity plus the water surface under the four corners: returns 0 while the ski sits on the water.
// spdY falls by 15 per frame and moves pos.y; the surface is the mean of the higher of the two
// front and the higher of the two rear corner floors (+-200 x, +-1500 z); below it the ski is put
// on the surface with spdY = 0. Returns 1 while airborne.
int pl0eSlopeControl(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec v;
    f32 fl[4];
    f32 y;

    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    w->spdY -= 15.0f;
    em->pos.y += w->spdY;
    v.x = 200.0f;
    v.y = 0.0f;
    v.z = 1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[0] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    v.x = -200.0f;
    v.y = 0.0f;
    v.z = 1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[1] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    v.x = 200.0f;
    v.y = 0.0f;
    v.z = -1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[2] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    v.x = -200.0f;
    v.y = 0.0f;
    v.z = -1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[3] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    if (fl[0] < fl[1]) {
        fl[0] = fl[1];
    }
    if (fl[2] < fl[3]) {
        fl[2] = fl[3];
    }
    y = (fl[0] + fl[2]) * 0.5f;
    if (em->pos.y < y) {
        w->spdY = 0.0f;
        em->pos.y = y;
        return 0;
    }
    return 1;
}

// The ski's own lean blend: m0 straight on the ski's motion work, m1 left / m2 right (by the
// sign of blendRate) into blendMot with weight |blendRate| / 256; hokan is the blend-in counter,
// frame the shared frame the riders copy (wraps at frameMax).
void pl0eBlendMotSet(cPl0e* em, void* m0, void* m1, void* m2, int a, int b, int c)
{
    Pl0eWork* w = PL0E_WK(em);
    f32 rate = fabsf(w->blendRate);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(em, &em->Motion, m0, a, (u8) w->hokan, 4, (u16) w->frame);
    if (w->blendRate < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, f, (u8) w->hokan, 4, (u16) w->frame);
    em->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (w->hokan) {
        w->hokan--;
    }
    w->frame++;
    if (w->frame >= em->frameMax) {
        w->frame = 0;
    }
}

// Jump ramp under the ski (scenario attribute bit 19).
int pl0eJumpCk(cPl0e* em)
{
    Vec a;
    Vec b;

    if ((s16) pG->pl_life > 0) {
        a = em->pos;
        b = em->pos;
        a.y += 1000.0f;
        b.y -= 10000.0f;
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x80000) {
            return 1;
        }
    }
    return 0;
}

// Wall 3500 ahead of the ski (attribute bit 11) at the right, left and centre.
int pl0eCrashCk(cPl0e* em)
{
    Vec a;
    Vec b;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    a.x = 400.0f;
    a.y = 1000.0f;
    a.z = 0.0f;
    b.x = 400.0f;
    b.y = 1000.0f;
    b.z = 3500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x800) {
        return 1;
    }
    a.x = -400.0f;
    a.y = 1000.0f;
    a.z = 0.0f;
    b.x = -400.0f;
    b.y = 1000.0f;
    b.z = 3500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x800) {
        return 1;
    }
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 1000.0f;
    b.z = 3500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x800) {
        return 1;
    }
    return 0;
}

// The ski has run out of buoyancy (`sink` drained to 0 on the 2nd rail) while the player lives.
int pl0eSinkCk(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    return w->sink <= 0.0f;
}

// Fall-off area under the ski (attribute bit 13).
int pl0eJumpMissCk(cPl0e* em)
{
    Vec a;
    Vec b;

    if ((s16) pG->pl_life > 0) {
        a = em->pos;
        b = em->pos;
        a.y += 1000.0f;
        b.y -= 10000.0f;
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x2000) {
            return 1;
        }
    }
    return 0;
}

// The wave object follows the ski, sinking with it (4000 .. 8000 below by the sink value).
void pl0eWaveMove(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Vec v;
    f32 y;

    if (w->pWave) {
        y = w->sink / 96000.0f * 4000.0f + 4000.0f;
        if (y < 4000.0f) {
            y = 4000.0f;
        }
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -y;
        PSMTXMultVec(em->mat, &v, &v);
        w->pWave->pos = v;
        w->pWave->ang = em->ang;
    }
}
