// em21 module (D:/Bio4/Prog/em21.cpp): the village dog. It sleeps, wanders and barks until the
// player comes close, then runs away (em21_R1_Escape, taking the other dogs along); the room 100 dog
// waits in a bear trap (em21_R1_R100TrapWait) until the player frees it (plemTrapCancel and its
// camera) and escapes; later it fights El Gigante at its side (em21_R1_VsElgigante).

#include "atari.h"
#include "dmg.h"
#include "em21.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "embarrel.h"
#include "at_mod.h"
#include "atari_init.h"
#include "act_btn.h"
#include "motion.h"
#include "route_ck.h"
#include "cam_ctrl.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include "em_mod.h"

// The module's 0x30-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em21,48,4");



typedef void (*Em21Func)(cEm21*);

static void em21_R0_Init(cEm21* em);
static void em21_R0_Move(cEm21* em);
static void em21_R1_Wait(cEm21* em);
static void em21_R1_Wander(cEm21* em);
static void em21_R1_Turn(cEm21* em);
static void em21_R1_Escape(cEm21* em);
static void em21_R1_Bark(cEm21* em);
static void em21_R1_R100TrapWait(cEm21* em);
static void em21_R1_R100TrapCancel(cEm21* em);
static void em21_R1_R100Escape(cEm21* em);
static void em21_R1_VsElgigante(cEm21* em);
static void em21TrapCancelAction(cEm21* em);
static void plemTrapCancel(cPlayer* pl);




// Module entry (SN loader): registers Em21Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    EmInitFunc = Em21Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm21 class in the manager's work.
void Em21Init(cEm* em)
{
    new (em) cEm21();
}

// Per-frame damage check (cEm21::move): the dog never dies; an explosion / fire damage volume (kind
// 1/4/5/7) or any weapon hit only makes it run away (Escape 3, dmType 0x3C = no more damage), the
// trapped room 100 dog (R1 5) gets free instead (its trap goes to routine 1/4, R100Escape 7 with
// r_no_3 1). The set 2 dog (El Gigante's companion) ignores hits.
void em21DmCk(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Vec hitPos;
    u8 mode;

    if (EmDeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hitPos)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            mode = em->set;
            {
                register int c asm("r9"); // COMPILER-DIFF: #13
                c = 0x1E;
                em->dmg.m_Timer = c;
            }
            if (mode != 2) {
                if ((em->r_no_0 == 1 && em->r_no_1 == 5) && w->pTrap) {
                    w->pTrap->r_no_0 = 1;
                    w->pTrap->r_no_1 = 4;
                    w->pTrap->r_no_2 = 0;
                    w->pTrap->r_no_3 = 0;
                    em->r_no_0 = 1;
                    em->dmg.m_Timer = 0x3C;
                    em->r_no_1 = 7;
                    em->r_no_2 = 0;
                    em->r_no_3 = 1;
                } else {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
            }
            return;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    if (em->set == 2) {
        return;
    }
    if ((em->r_no_0 == 1 && em->r_no_1 == 5) && w->pTrap) {
        w->pTrap->r_no_0 = 1;
        w->pTrap->r_no_1 = 4;
        w->pTrap->r_no_2 = 0;
        w->pTrap->r_no_3 = 0;
        em->r_no_0 = 1;
        em->dmg.m_Timer = 0x3C;
        em->r_no_1 = 7;
        em->r_no_2 = 0;
        em->r_no_3 = 1;
    } else {
        em->dmg.m_Timer = 0x3C;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

Em21Func Em21_R0_move_tbl[5] = {
    em21_R0_Init,
    em21_R0_Move,
    em21_R0_Move,
    em21_R0_Move,
    (Em21Func) Em_R0_Scenario,
};

static Em21Func Em21_R1_move_tbl[9] = {
    em21_R1_Wait,
    em21_R1_Wander,
    em21_R1_Turn,
    em21_R1_Escape,
    em21_R1_Bark,
    em21_R1_R100TrapWait,
    em21_R1_R100TrapCancel,
    em21_R1_R100Escape,
    em21_R1_VsElgigante,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em21_flip_tbl[50] = {
    0, 1, 2, 3, 4, 5, 6, 8, 7, 0xD, 0xE, 0xF, 0x10, 9, 0xA, 0xB, 0xC, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14,
    0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
};

// The camera plemTrapCancel installs (the trap release cut): explicitly zero-initialised so it
// stays in .data.
static Camera em21_trap_cam = { 0 };
// COMPILER-DIFF: candidate #12 (cse related-value): `cam = &em21_trap_cam` after the `&em21_trap_cam.param.pos/at`
// pointers are known is a fresh `lis/addi` pair in the original; our cse rewrites it as `at - 0xB0`.
// An asm-labelled alias declaration gives cse a distinct SYMBOL_REF and keeps the fresh pair.
extern Camera em21_trap_cam_v asm("em21_trap_cam");
// .data is padded to 8 bytes before the linker's BSS tag word.
asm(".section .data\n\t.balign 8\n\t.text");

// Per-frame update: clears the neck flag, damage check, route check (Em21RouteCk), the R0 table
// (Init / Move / Damage and Die share R0_Move / Scenario), then the neck, collision and scenario check;
// a dead player makes the dog forget its target.
void cEm21::move()
{
    Em21Work* w = EM21_WK(this);
    f32 d;

    em21DmCk(this);
    w->flags &= ~2;
    if (w->stuckTimer) {
        w->stuckTimer--;
    }
    if (w->escTimer) {
        w->escTimer--;
    }
    if ((s16) pG->pl_life <= 0) {
        w->escTimer = 1;
    }
    Motion.Mot_flag &= ~0x40000000;
    Em21RouteCk(this);
    Em21_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em21NeckMove(this);
    partsWorldCalc();
    d = VEC_DISTXZ(&pos, &pos_old);
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (VEC_DISTXZ(&pos, &pos_old) < d * 0.5f) {
        w->stuckTimer = 3;
    }
}

// R0 == 0: creation. Builds the model of type 0 / 1, no lock-on / no Ashley help status, hp 1000,
// collision and hit boxes (em21YarareInit), and the start routine by set: 0 Wait, 1 the room 100 bear
// trap dog (R100TrapWait 5), 2 El Gigante's companion (VsElgigante 8).
static void em21_R0_Init(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    cAtariInfo* at;
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em21() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em21() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    em->setStatus(EM_STATUS_LOCKOFF);
    zero = 0;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    at = &em->atari;
    em->hp = 1000;
    em->Motion.flip = em21_flip_tbl;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    at->init(0.0f, -500.0f, 0.0f, 450.0f, 400.0f, 400.0f, 1000.0f, 3, 0x2000, 10);
    em21YarareInit(em);
    w->tilt = 0.0f;
    w->plDist = 100000000.0f;
    w->stuckTimer = zero;
    w->neckX = 0.0f;
    w->neckY = 0.0f;
    w->pTrap = (cEm*) zero;
    w->sndId = zero;
    switch (em->set) {
    default:
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 1:
        at->setPriority(PRI_LV1);
        EmRoutineSet(em, 1, 5, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 1, 8, 0, 0);
        break;
    }
    MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, 5, 0);
    MotionMove(em, 0);
    em21_R0_Move(em);
}

// R0 == 1..3: runs the R1 routine (Em21_R1_move_tbl).
static void em21_R0_Move(cEm21* em)
{
    Em21_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 Wait: lies asleep (motion 0xB) then stretches (0xC) and either wanders (1) or keeps
// sleeping; wakes into Escape (3) when the player comes close (em21WakeCk).
static void em21_R1_Wait(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xB), 0, 30, 5, 0);
        w->timer = Rnd() % 3;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xC), ARC(0x17), 30, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (Rnd() & 1) {
                em21SetWanderPos(em);
                EmRoutineSet(em, 1, 1, 0, 0);
            } else {
                em->r_no_2 = 0;
            }
        }
        break;
    }
    if (em21WakeCk(em)) {
        w->escTimer = 2;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

// R1 == 1 Wander: trots (motion 7) towards the wander point (em21SetWanderPos, work flag bit2),
// arriving after a few steps back to Wait; the player nearby -> Escape (3).
static void em21_R1_Wander(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), ARC(0xD), 30, 5, 0);
        w->timer = Rnd() % 5;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 128.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em21DirMatrix(em, 0.0f);
        MotionMove(em, 0);
        if ((em->pos.x - w->wanderPos.x) * (em->pos.x - w->wanderPos.x) + (em->pos.z - w->wanderPos.z) * (em->pos.z - w->wanderPos.z)
            < 640000.0f) {
            EmRoutineSet(em, 1, 0, 2, 0);
        }
        break;
    }
    if (em21WakeCk(em)) {
        w->escTimer = 2;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

// R1 == 2 Turn: turns on the spot towards the target point, then Escape (3) or Bark (4).
static void em21_R1_Turn(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    w->flags |= 2;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), ARC(0xD), 30, 5, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 64.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em21DirMatrix(em, 0.0f);
        MotionMove(em, 0);
        break;
    }
    if (em->l_pl < 16000000.0f) {
        EmRoutineSet(em, 1, 3, 0, 0);
        w->escTimer = 2;
    } else if (w->routeAngAbs < PI / 8.0f) {
        EmRoutineSet(em, 1, 4, 0, 0);
    }
}

// R1 == 3 Escape: runs away from the player (RouteCkEscEm target, one of four run motions 9 +
// 0x12..0x15, random swerves every 15..45 frames, body tilt em21DirMatrix), rings the bell alarm
// (Status_flg[1] bit29) and takes the other dogs along (em21EscapeWithYou); slows to a trot (0xA) and
// back to Turn when far enough.
static void em21_R1_Escape(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    f32 t;

    if (em->l_pl < 25000000.0f) {
        w->flags |= 2;
    }
    if (em->r_no_2 == 0) {
        RouteCkEscEm(em, pPL, &w->targetPos);
        t = fabsf(Muku(&em->pos, &w->targetPos, em->ang.y, PI));
        if (t > 2.0943952f) {
            em->r_no_2 = 2;
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0xB, &em->pos, em->id, 0, em);
        if (!StaFlagChk(pG, STA_SE_BURST)) {
            StaFlagOn(pG, STA_SE_BURST);
            pG->SeInfo.pos = em->pos;
            pG->SeInfo.type = 0;
        }
    }
    w->escTimer = 2;
    switch (em->r_no_2) {
    case 0:
        switch (Rnd() & 3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(9), ARC(0x12), 3, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(9), ARC(0x13), 3, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(9), ARC(0x14), 3, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(9), ARC(0x15), 3, 5, 0);
            break;
        }
        em21EscapeWithYou(em);
        w->escAng = GetXZAngle(&pPL->pos, &em->pos);
        w->escAng += fRand1_1() * (PI / 2.0f);
        w->escAng = LIMIT_ANGLE(w->escAng);
        w->timer = Rnd() % 3 + 3;
        w->timer2 = Rnd() % 30 + 30;
        em->r_no_3 = Rnd() & 1;
        em->r_no_2++;
    case 1:
        if (w->timer2) {
            w->timer2--;
        } else {
            w->timer2 = Rnd() % 15 + 15;
            w->escAng += fRand1_1() * (PI / 4.0f);
            w->escAng = LIMIT_ANGLE(w->escAng);
        }
        t = Muku2(em->ang.y, w->escAng, PI / 40.0f);
        em->ang.y += t;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em21DirMatrix(em, t);
        if (w->stuckTimer > 3) {
            if (em->r_no_3) {
                w->escAng += PI / 5.0f;
            } else {
                w->escAng -= PI / 5.0f;
            }
            w->escAng = LIMIT_ANGLE(w->escAng);
        }
        if (MotionMove(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x16), 3, 1, 0);
        em21EscapeWithYou(em);
        em->r_no_2++;
    case 3:
        w->timer = Rnd() % 30 + 30;
        if (em->Motion.Seq_old.Free & 2) {
            if (w->stuckTimer) {
                t = 0.058904864f;
            } else {
                t = PI / 40.0f;
            }
            em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, t);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            em21DirMatrix(em, Muku(&em->pos, &w->targetPos, em->ang.y, PI));
        } else {
            em21DirMatrix(em, 0.0f);
        }
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
}

// R1 == 4 Bark: faces the player and barks (motions 8 + 0xE..0x11 at random), escaping (3) when he
// comes within range or looks at it.
static void em21_R1_Bark(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    f32 ang;

    w->flags |= 2;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xB), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        em21DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            int zero = 0;

            em->r_no_2 = zero;
            if (w->routeAngAbs > PI / 4.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI));
        if (ang < PI / 4.0f && em->l_pl < 100000000.0f) {
            em->r_no_2++;
        }
        break;
    case 2:
        switch (Rnd() & 3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0xE), 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0xF), 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0x10), 30, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0x11), 30, 5, 0);
            break;
        }
        em->r_no_2++;
    case 3:
        em21DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            int zero = 0;

            em->r_no_2 = zero;
            if (w->routeAngAbs > PI / 4.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI));
                if (ang > 1.0471976f || em->l_pl > 144000000.0f) {
                    em->r_no_2 = zero;
                }
            }
        }
        break;
    }
    if (em->l_pl < 25000000.0f) {
        w->escTimer = 2;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

// R1 == 5: the room 100 dog caught in the bear trap (pTrap, em21TrapSearch): whimpers (motion 0x18,
// SE every 75..105 frames) until the player frees it (the trap's action button -> em21TrapCancelAction)
// or a hit makes it tear loose (R100Escape 7).
static void em21_R1_R100TrapWait(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 0, 5, 0);
        w->pTrap = 0;
        w->timer = Rnd() % 30 + 75;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = Rnd() % 30 + 75;
            SndStop(w->sndId, 0);
            w->sndId = SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
        MotionMove(em, 0);
        em21TrapSearch(em);
        if (w->pTrap && (w->pTrap->r_no_0 == 1 && w->pTrap->r_no_1 == 4)) {
            em->dmg.m_Timer = 0x3C;
            EmRoutineSet(em, 1, 7, 0, 1);
            return;
        }
        break;
    }
    if (!(em->l_pl > 9000000.0f)) {
        ActBtn.set(ACT_RESCUE, 5, (void*) em21TrapCancelAction, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
}

// R1 == 6: freed from the trap by the player: the release motion 0x19 in sync with plemTrapCancel,
// the "dog freed" music cue, then R100Escape (7).
static void em21_R1_R100TrapCancel(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x1C), 3, 5, 0);
        if (w->pTrap) {
            w->pTrap->flag |= 1;
        }
        SndStrReq(1, 0xE, 0x80000003, 0, 0, 0.0f);
        ScfFlagOn(pG, SCF_R100_DOG_RUN);
        em->hp = 0;
        em->atari.m_flag &= ~0x200;
        em->r_no_2++;
    case 1:
        em->dmg.m_Timer = 2;
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
}

// R1 == 7: the freed / torn-loose trap dog runs off to the room exit (motion 9 or 0x1A), jumps the
// fence (0x1E) and fades out (invisible_factor) once out of sight.
static void em21_R1_R100Escape(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Vec target = { -24950.0f, -400.0f, -26740.0f };

    switch (em->r_no_2) {
    case 0:
        EmSetDie(em);
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(9), ARC(0x12), 3, 5, 0);
            SndStop(w->sndId, 0);
            w->sndId = SndCall(8, 0xB, &em->pos, em->id, 0, em);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1A), ARC(0x1D), 3, 5, 0);
        }
        em->atari.m_flag |= 0x100;
        MotionMove(em, 0);
        em->r_no_2++;
    case 1: {
        f32 lim;

        if (w->stuckTimer) {
            lim = 0.058904864f;
        } else {
            lim = PI / 40.0f;
        }
        em->ang.y += Muku(&em->pos, &target, em->ang.y, lim);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em21DirMatrix(em, Muku(&em->pos, &target, em->ang.y, PI));
        MotionMove(em, 0);
        if ((em->pos.x - target.x) * (em->pos.x - target.x) + (em->pos.z - target.z) * (em->pos.z - target.z) < 36000000.0f) {
            em->r_no_2++;
        }
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x1E), 0, 3, 1, 0);
        em->atari.m_flag &= ~0x100;
        MotionMove(em, 0);
        em->r_no_2++;
    case 3:
        if (em->Motion.Seq_frame > 2.7f && em->Motion.Seq_frame < 3.3f) {
            SndCall(8, 7, &em->pos, em->id, 0, em);
        }
        em21DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x1A), ARC(0x1D), 3, 5, 0);
        MotionMove(em, 0);
        em->r_no_2++;
    case 5:
        em21DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 6:
        MotionMove(em, 0);
        em->atari.m_flag &= ~0x300;
        em->invisible_factor -= 0.1f;
        if (em->invisible_factor <= 0.0f) {
            em->be_flag &= ~2;
            em->invisible_factor = 0.0f;
            em->r_no_2++;
        }
        break;
    }
}

// R1 == 8: the dog fighting El Gigante at the player's side (pGigante, em21SearchElgigante): waits at
// the bark point (em21GetBarkPos, work flag bit3), barks at the giant, runs at it and lunges (motions
// 0x1F / 0x21 / 0x26 by distance and angle), and runs off (0x25) when the giant dies; 18 steps.
static void em21_R1_VsElgigante(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    cEm* g = w->pGigante;
    f32 ang;
    f32 d;

    w->flags |= 8;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x24), 0, 0, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em21SearchElgigante(em)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x20), 0, 5, 5, 0);
        w->timer = Rnd() % 60 + 60;
        em->r_no_2++;
    case 3:
        w->flags |= 2;
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = Rnd() % 60 + 60;
            w->sndId = SndCall(8, 5, &em->pos, em->id, 0, em);
        }
        if (MotionMove(em, 0) && (Rnd() & 3) == 0) {
            em->r_no_2++;
            break;
        }
        if (g->hp <= 0) {
            em->r_no_2 = 0x10;
            break;
        }
        if ((em->pos.x - g->pos.x) * (em->pos.x - g->pos.x) + (em->pos.z - g->pos.z) * (em->pos.z - g->pos.z) < 25000000.0f
            && (g->flag & 4)) {
            em->r_no_2 = 6;
            break;
        }
        if (EmDeadCk(em)) {
            em->r_no_2 = 6;
        } else {
            ang = fabsf(Muku(&em->pos, &g->pos, em->ang.y, PI));
            if (ang > 0.87266463f) {
                em->r_no_2 = 0xE;
            }
        }
        break;
    case 4:
        switch (Rnd() & 3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0xE), 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0xF), 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0x10), 30, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(8), ARC(0x11), 30, 5, 0);
            break;
        }
        em->r_no_2++;
    case 5:
        w->flags |= 2;
        if (MotionMove(em, 0)) {
            em->r_no_2 = 2;
            break;
        }
        if ((em->pos.x - g->pos.x) * (em->pos.x - g->pos.x) + (em->pos.z - g->pos.z) * (em->pos.z - g->pos.z) < 25000000.0f
            && (g->flag & 4)) {
            em->r_no_2 = 6;
            break;
        }
        if (EmDeadCk(em)) {
            em->r_no_2 = 6;
        } else {
            ang = fabsf(Muku(&em->pos, &g->pos, em->ang.y, PI));
            if (ang > 0.87266463f) {
                em->r_no_2 = 0xE;
            }
        }
        break;
    case 6:
        if (Rnd() & 2) {
            MotionSetCore(em, MOTION(em), ARC(0x1F), ARC(0x22), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1F), ARC(0x22), 3, 1, 0);
        }
        em->r_no_2++;
    case 7:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
            if (em21GetBarkPos(em) == 0) {
                em->r_no_2 = 2;
                break;
            }
            if (fabsf(Muku(&em->pos, &w->barkPos, em->ang.y, PI)) < 2.0943952f) {
                em->r_no_2 = 8;
                break;
            }
            em->r_no_2 = 0xA;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0x1A), ARC(0x1D), 3, 5, 0);
        em->r_no_2++;
    case 9:
        w->flags |= 2;
        // the Muku limit shares the `ang` variable: its f2 argument copy gives the pseudo an f2
        // preference, so `fabs f2, f1` in every arm
        if (w->stuckTimer) {
            ang = 0.058904864f;
        } else {
            ang = PI / 40.0f;
        }
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, ang);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em21DirMatrix(em, Muku(&em->pos, &w->targetPos, em->ang.y, PI));
        MotionMove(em, 0);
        {
            // temp computed BEFORE d: combine then substitutes t (the later LOG_LINK is tried
            // first) and the fmadds keeps d's own register for the dz*dz term
            f32 t = (em->pos.x - w->barkPos.x) * (em->pos.x - w->barkPos.x);
            d = (em->pos.z - w->barkPos.z) * (em->pos.z - w->barkPos.z);
            d += t;
        }
        if (d < 4000000.0f) {
            ang = fabsf(Muku(&em->pos, &g->pos, em->ang.y, PI));
            if (ang < 1.7453293f) {
                em->r_no_2 = 2;
                break;
            }
            em->r_no_2 = 0xC;
            break;
        }
        if (EmDeadCk(em)) {
            em->r_no_2 = 6;
        }
        break;
    case 0xA:
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x16), 3, 1, 0);
        em->r_no_2++;
    case 0xB:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 8;
            break;
        }
        if (EmDeadCk(em)) {
            em->r_no_2 = 6;
        }
        break;
    case 0xC:
        MotionSetCore(em, MOTION(em), ARC(0x21), ARC(0x23), 3, 1, 0);
        em->r_no_2++;
    case 0xD:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 2;
            break;
        }
        if (EmDeadCk(em)) {
            em->r_no_2 = 6;
        }
        break;
    case 0xE:
        MotionSetCore(em, MOTION(em), ARC(0x26), ARC(0x27), 3, 5, 0);
        em->r_no_2++;
    case 0xF:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 64.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em21DirMatrix(em, 0.0f);
        MotionMove(em, 0);
        ang = fabsf(Muku(&em->pos, &g->pos, em->ang.y, PI));
        if (ang < 0.34906585f) {
            em->r_no_2 = 2;
            break;
        }
        if ((em->pos.x - g->pos.x) * (em->pos.x - g->pos.x) + (em->pos.z - g->pos.z) * (em->pos.z - g->pos.z) < 9000000.0f
            && (g->flag & 4)) {
            em->r_no_2 = 6;
            break;
        }
        if (EmDeadCk(em)) {
            em->r_no_2 = 6;
        }
        break;
    case 0x10:
        MotionSetCore(em, MOTION(em), ARC(0x25), 0, 10, 5, 0);
        em->r_no_2++;
    case 0x11:
        w->flags |= 2;
        MotionMove(em, 0);
        break;
    }
}

// Finds the alive El Gigante (id 0x2B) into pGigante; 1 when found.
int em21SearchElgigante(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->checkStatus(EM_STATUS_ACTIVE) == 0) {
            continue;
        }
        w->pGigante = e;
        return 1;
    }
    return 0;
}

// Action button callback of the trapped dog: starts the player's release routine (plemTrapCancel) and
// R100TrapCancel (6) on the dog.
static void em21TrapCancelAction(cEm21* em)
{
    pPL->dmg.m_Timer = 2;
    em->dmg.m_Timer = 2;
    SetPlDamage(em, plemTrapCancel);
    EmRoutineSet(em, 1, 6, 0, 0);
}

// Player damage routine of the trap release: the kneel-and-open motion (player archive 0x1B) with the
// trap camera, then EndPlDamage.
static void plemTrapCancel(cPlayer* pl)
{
    cEm* em = pl->pEmCatch;
    Vec v;

    pl->subArc = pPL->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        v.x = 431.86002f;
        v.y = 0.0f;
        v.z = -989.88f;
        pl->ang.y = em->ang.y;
        PSMTXMultVec(em->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x1B), 0, 0, 1, 0);
        pl->r_no_2++;
    case 1:
        pl->dmg.m_Timer = 2;
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->subArc = pl->subArc2;
    plem21TrapCamMove(pl);
}

// Installs the trap release cut camera (em21_trap_cam) beside the player looking at the dog.
void plem21TrapCamMove(cModel* m)
{
    Camera* c = &pG->Camera;
    Camera* cam;
    Vec v;
    Vec a;

    v.x = 2500.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(m->mat, &v, &v);
    v.y += 2000.0f;
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 1000.0f;
    PSMTXMultVec(m->mat, &a, &a);
    PosToPos(&c->param.at, &a, &em21_trap_cam.param.at, 1.0f);
    PosToPos(&c->param.pos, &v, &em21_trap_cam.param.pos, 1.0f);
    {
        Vec* pos = &em21_trap_cam.param.pos;
        Vec* at = &em21_trap_cam.param.at;
        f32 dx = pos->x - at->x;
        f32 dy = pos->y - at->y;
        f32 dz = pos->z - at->z;

        cam = &em21_trap_cam_v;   // COMPILER-DIFF: candidate #12
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    cam->param.fovy = 55.0f;
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// Per frame: the route point / angle to the player (routePos, routeAng, flag bit0 = reachable), to
// the current target (bark point, wander point or the player: targetPos / targetAng / targetDist2),
// the player distance plDist, and the escape point while escTimer runs.
void Em21RouteCk(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if ((pG->Frame_cnt & 7) != (em->emset_no & 7)) {
        return;
    }
    w->flags &= ~1;
    if (RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (w->flags & 8) {
        RouteCkToPos(em, &w->barkPos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist2 = (em->pos.x - w->barkPos.x) * (em->pos.x - w->barkPos.x)
                         + (em->pos.z - w->barkPos.z) * (em->pos.z - w->barkPos.z);
        w->pTarget = 0;
        return;
    }
    if (w->flags & 4) {
        RouteCkToPos(em, &w->wanderPos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist2 = (em->pos.x - w->wanderPos.x) * (em->pos.x - w->wanderPos.x)
                         + (em->pos.z - w->wanderPos.z) * (em->pos.z - w->wanderPos.z);
        w->pTarget = 0;
    } else {
        w->targetPos = w->routePos;
        w->targetAng = w->routeAng;
        w->targetAngAbs = w->routeAngAbs;
        w->targetDist2 = em->l_pl;
        w->pTarget = pPL;
    }
    w->flags &= ~4;
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    if (w->escTimer) {
        RouteCkEscEm(em, pPL, &w->targetPos);
    }
}

// Builds the model matrix with the run tilt: rolls the body 30 deg into the turn direction `dir`
// (smoothed in tilt), used by the running routines.
void em21DirMatrix(cEm21* em, f32 dir)
{
    Em21Work* w = EM21_WK(em);
    f32 t;

    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->Motion.Mot_flag |= 0x40000000;
    t = 0.0f;
    if (dir > 0.0f) {
        t = -0.5235988f;
    }
    if (dir < 0.0f) {
        t = 0.5235988f;
    }
    dir = fabsf(dir);
    if (dir < 0.049087387f) {
        t = 0.0f;
    }
    w->tilt = w->tilt * 0.8f + t * 0.2f;
    t = fabsf(w->tilt);
    if (t > 0.001f) {
        Mtx m;
        Vec ax;

        ax.x = 0.0f;
        ax.y = 0.0f;
        ax.z = 1.0f;
        PSMTXMultVecSR(em->mat, &ax, &ax);
        PSMTXRotAxisRad(m, &ax, w->tilt);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}

// Neck tracking (work flag bit1): turns the head parts 3..5 towards El Gigante's root or the player's
// head within 60 deg (neckX / neckY eased), else eases back.
void em21NeckMove(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Mtx m;
    Vec d;
    cParts* tp;
    cParts* p;
    f32 f;
    f32 len;

    if (w->pGigante && w->pGigante->hp > 0) {
        tp = w->pGigante->getPartsPtr(0);
    } else {
        tp = pPL->getPartsPtr(4);
    }
    p = (cParts*) em->getPartsPtr(4);
    if (w->flags & 2) {
        // do{}while(0) (COMPILER-DIFF-tagged tie lever): the refs inside count twice, so tp
        // outranks em in global-alloc (tp r29, em r28); the notes sit at block edges (no barrier)
        do {
            f = Muku(&em->pos, &p->world, em->ang.y, 1.0471976f);
            w->neckY += Muku2(w->neckY, f, 0.098174773f);
            PSVECSubtract(&tp->world, &p->world, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
        } while (0);
        f = -atan2f(d.y, len);
        if (f > 0.7853982f) {
            f = 0.7853982f;
        }
        if (f < -0.7853982f) {
            f = -0.7853982f;
        }
        w->neckX += Muku2(w->neckX, f, 0.098174773f);
    } else {
        w->neckX *= 0.9f;
        w->neckY *= 0.9f;
    }
    f = w->neckY * 0.33333334f;
    p = (cParts*) em->getPartsPtr(3);
    p->inv_offset.z = 0.0f;
    p->inv_offset.x = 0.0f;
    p->inv_offset.y = f;
    p->motParts.flags |= 0x40000000;
    p = (cParts*) em->getPartsPtr(4);
    p->inv_offset.z = 0.0f;
    p->inv_offset.x = 0.0f;
    p->inv_offset.y = f;
    p->motParts.flags |= 0x40000000;
    p = (cParts*) em->getPartsPtr(5);
    p->inv_offset.y = f;
    p->motParts.flags |= 0x40000000;
    p->inv_offset.x = 0.0f;
    p->inv_offset.z = 0.0f;
    f = fabsf(w->neckX);
    if (!(f < 0.01f)) {
        f = w->neckX * 0.33333334f;
        p = (cParts*) em->getPartsPtr(3);
        PSMTXRotRad(m, 'x', f);
        PSMTXConcat(p->l_mat, m, p->l_mat);
        p = (cParts*) em->getPartsPtr(4);
        PSMTXRotRad(m, 'x', f);
        PSMTXConcat(p->l_mat, m, p->l_mat);
        p = (cParts*) em->getPartsPtr(5);
        PSMTXRotRad(m, 'x', f);
        PSMTXConcat(p->l_mat, m, p->l_mat);
    }
}

// Should the sleeping / wandering dog notice the player: within 1500 units, within 4000 in front, the
// alert / bell alarm (Status_flg bits, SeInfo.pos within 15000) or the room's forced alert. 1 = yes.
int em21WakeCk(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    if (em->l_pl < 2250000.0f) {
        return 1;
    }
    if (em->l_pl < 16000000.0f && w->routeAngAbs < PI / 4.0f) {
        return 1;
    }
    if (StaFlagChk(pG, STA_PL_SE_FOOT) && em->l_pl < 16000000.0f) {
        return 1;
    }
    if (StaFlagChk(pG, STA_PL_SE_WHISTLE) || (StaFlagChk(pG, STA_PL_FIRE))) {
        if (w->plDist < 15000.0f) {
            return 1;
        }
    }
    if (StaFlagChk(pG, STA_SE_BURST)) {
        f32 r;

        // three identical arms + the override after the switch: the arm sets are dead (the
        // compare skeleton stays) and the block-local `r` is loaded at the use (em3cFindCk idiom)
        switch (pG->SeInfo.type) {
        case 0:
            r = 15000.0f;
            break;
        case 1:
            r = 15000.0f;
            break;
        default:
            r = 15000.0f;
            break;
        }
        r = 15000.0f;
        if ((em->pos.x - pG->SeInfo.pos.x) * (em->pos.x - pG->SeInfo.pos.x) + (em->pos.y - pG->SeInfo.pos.y) * (em->pos.y - pG->SeInfo.pos.y)
                + (em->pos.z - pG->SeInfo.pos.z) * (em->pos.z - pG->SeInfo.pos.z)
            < r * r) {
            return 1;
        }
    }
    return 0;
}

// Hit boxes: the body box and hit[0..4] on the head / legs (YarareInit / YarareAdd).
void em21YarareInit(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    YarareInit(em, 0.0f, -100.0f, -150.0f, 250.0f, 650.0f, 2, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 150.0f, 100.0f, 6, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[1], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xB, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xF, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[3], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[4], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x18, YAT_FLAG_ON);
}

// Sends every other alive dog (id 0x21) that is waiting / turning / barking into Escape (3).
void em21EscapeWithYou(cEm21* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.at(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x21) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->r_no_0 != 1) {
            continue;
        }
        if (e->r_no_1 == 4 || e->r_no_1 == 0 || e->r_no_1 == 2) {
            if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 9000000.0f) {
                continue;
            }
            EmRoutineSet(e, 1, 3, 0, 0);
        }
    }
}

// Picks a wander point 3000..6000 units ahead within +-90 deg (stopped at the first wall), flag bit2.
void em21SetWanderPos(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Mtx m;
    Vec a;
    Vec b;
    Vec hit;
    f32 ang;

    ang = LIMIT_ANGLE(em->ang.y + fRand1_1() * (PI / 2.0f));
    a = em->pos;
    a.y += 500.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = fRand0_1() * 3000.0f + 3000.0f;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0)) {
        b = hit;
    }
    w->wanderPos = b;
    w->flags |= 4;
}

// Finds the unused bear trap object (id 0x2A, type 0, set 1) and snaps it to the dog's hind leg
// (pTrap). 1 when found.
int em21TrapSearch(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    u32 i;

    if (w->pTrap) {
        return 0;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.at(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x2A) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->type != 0) {
            continue;
        }
        if (e->set == 1) {
            Vec v;

            w->pTrap = e;
            e->ang.y = em->ang.y;
            v.x = 256.48f;
            v.y = 0.0f;
            v.z = -304.75f;
            PSMTXMultVec(em->mat, &v, &e->pos);
            return 1;
        }
    }
    return 0;
}

// The EMI type 0xA bark point farthest from the dog into barkPos; 0 when the room has none.
int em21GetBarkPos(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    EmiData* emi = pG->pEmi;
    EmiEntry* e;
    f32 best = 0.0f;
    int idx;
    int i;

    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < emi->n; i++) {
        e = &emi->entry[i];
        if (e->type != 0xA) {
            continue;
        }
        if (idx == -1) {
            idx = i;
            best = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        } else {
            f32 d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);

            if (!(d < best)) {
                best = d;
                idx = i;
            }
        }
    }
    if (idx == -1) {
        return 0;
    }
    e = &(pG->pEmi)->entry[idx];
    w->barkPos = e->pos;
    return 1;
}
