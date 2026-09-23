// em29 module (D:/Bio4/Prog/em29.cpp): the bats. They hang from the ceiling or sit on the ground
// until the player comes close, fly around (em29_R1_Walk / Turn, em29SetSPeed blends the speed and
// keeps the bat between the floor and the ceiling), dash at the player (em29_R1_AtkDash) or rush him
// in a swarm (em29_R1_AtkRush, plem29_BatRush), and are pushed apart from each other and the player by
// em29ObaHitCk. Their sounds go through the room's ctrl11 / ctrl12 controls.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em29.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"



typedef void (*Em29Func)(cEm29*);

static void em29_R0_Init(cEm29* em);
static void em29_R0_Move(cEm29* em);
static void em29_R1_WaitLand(cEm29* em);
static void em29_R1_WaitCeiling(cEm29* em);
static void em29_R1_Walk(cEm29* em);
static void em29_R1_Turn(cEm29* em);
static void em29_R1_AtkDash(cEm29* em);
static void em29_R1_AtkRush(cEm29* em);
static void em29_R0_Damage(cEm29* em);
static void em29_R1_Dm_Air(cEm29* em);
static void em29_R1_Dm_Ceiling(cEm29* em);
static void em29_R1_Dm_Land(cEm29* em);
static void em29_R1_Dm_Recovery(cEm29* em);
static void em29_R0_Die(cEm29* em);
static void em29_R1_Die_Normal(cEm29* em);
static void em29_R1_Die_Reset(cEm29* em);
static void em29_R1_Die_FadeOut(cEm29* em);
static void plem29_BatRush(cPlayer* pl);




// Damage / death routine per the wait state the bat was in (0: flying, 1: on the ceiling, 2: on the ground).
static inline void em29DmRoutineSet(cEm29* em, u32 kind)
{
    switch (kind) {
    case 0:
    default:
        EmRoutineSet(em, 2, 0, 0, 0);
        break;
    case 1:
        EmRoutineSet(em, 2, 1, 0, 0);   // cse stores the kind register for the 1
        break;
    case 2:
        EmRoutineSet(em, 2, 2, 0, 0);
        break;
    }
}

// The same routine set with the arms laid out 1, 2, default: the copy em29DmCk's dead `dmWep == 0x21`
// then-arm uses. Only the layout differs (the tree is the same, followed by `b default`); jump2 deletes
// this copy whole, but with the default body first it merges the arms in an order that leaves the
// then-copy's tree behind (see em29DmCk).
static inline void em29DmRoutineSetLate(cEm29* em, u32 kind)
{
    switch (kind) {
    case 1:
        EmRoutineSet(em, 2, 1, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 2, 2, 0, 0);
        break;
    case 0:
    default:
        EmRoutineSet(em, 2, 0, 0, 0);
        break;
    }
}

// The tail's routine set: `z` is the `zero` pseudo (target `li r29,0` at the dmg join) so the arms are
// register-distinct from the early set's (whose zero is the `em->hp = 0` register) and jump2 does not
// cross-jump the two sets into one.
static inline void em29DmRoutineSetZ(cEm29* em, u32 kind, int z)
{
    switch (kind) {
    case 0:
    default:
        EmRoutineSet(em, 2, z, z, z);
        break;
    case 1:
        EmRoutineSet(em, 2, 1, z, z);
        break;
    case 2:
        EmRoutineSet(em, 2, 2, z, z);
        break;
    }
}

// Module entry (SN loader): registers Em29Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em29 prolog Ok\n");
    EmInitFunc = Em29Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm29 class in the manager's work.
void Em29Init(cEm* em)
{
    new (em) cEm29();
}

// Per-frame damage check (cEm29::move): an explosion / fire volume kills the bat (flag bit7, the
// room's EM29_DIE count, death squeak). A weapon hit does 999..1000 (= the whole hp) for most guns and
// the knife, 9999 for explosives / magnum and a near shotgun hit (far: 999), the mine 0xE nothing; a
// dead bat is counted and goes to Dm_Air / Dm_Ceiling / Dm_Land by where it was (R2 0..2), a
// surviving one flinches the same way.
void em29DmCk(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    u32 kind;
    int hit;
    int wep;
    int b1;
    int b3;
    int dmg;
    int zero;

    kind = 0;
    if (w->flags & 0x40) {
        kind = 1;
    }
    if (w->flags & 0x20) {
        kind = 2;
    }
    if ((em->be_flag & 2) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_GRENADE_BLAST:
        case DMG_TYPE_ENV_LIGHT:
        case DMG_TYPE_GRENADE:
            em->hp = 0;
            Ctrl12CntAdd(w->pCtrl12, CTRL12_ID_CNT_EM29_DIE, 1);
            Ctrl11SetSe(w->pCtrl11, em, 1, 0x1C, 0xA);
            w->flags |= 0x80;
            em29DmRoutineSet(em, kind);
            return;
        }
    }
    hit = em->dmg.m_Flag;
    if (hit == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    // b3, b1, store: `hit` dies at b1 (weight 0), so sched1 ranks b1 above b3 and, by source order, above
    // the dmHit store: `rlwinm b1; stb dmHit; rlwinm b3` like the target (the other five orders differ).
    b3 = (hit >> 3) & 1;
    b1 = (hit >> 1) & 1;
    em->dmg.m_Flag = 0;
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = (Rnd() & 1) + 999;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 9999;
        if (em->l_pl > 16000000.0f) {
            dmg = (Rnd() & 1) + 999;
        }
        break;
    // default-grouped nodes shape the tree (tools/research/casetree.py): 0xF gives the left half the weight
    // that keeps [0x10,0x11] the root, [0x2C,0x2D] makes 0x21 the right root; their compares fold
    // into `b default`. The default arm is written before case 0xE (bodies laid out A, X, D, Y).
    case 5:
    case 6:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    zero = 0;
    if (b1) {
        dmg <<= 2;
    }
    if (b3) {
        dmg = 9999;
    }
    LifeDownSet2(em, dmg, 0, 0);
    if (em->hp > 0) {
        EmDmBloodSet2(em, 0x21, 1, 0, 0, 0);
    }
    SndCall(8, 0xE, &em->pos, em->id, 0, em);
    if (em->hp > 0) {
        goto alive;
    }
    // The do-while doubles zero's ref weight so global alloc places it (r29) before `kind` (r28)
    // and `b3` (r26) -- with plain refs kind ranks above zero and the two swap registers.
    do {
        EstSet(0, -1, &em->pos, &em->ang, EFF_EM29, 0, 0, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
        em->be_flag &= ~2;
        Ctrl12CntAdd(w->pCtrl12, CTRL12_ID_CNT_EM29_DIE, 1);
        em29LastCk(em);
        em29DmRoutineSetZ(em, kind, zero);
    } while (0);
    goto tail;
    // Dead loop in front of the `alive` label: its LOOP_END note stops cse from carrying `zero == 0`
    // into the hp > 0 arm, whose routine sets keep fresh `li r0,0`/`li r9,0` like the target.
    do {
    } while (0);
alive:
    // Dead test with identical arms: jump2 cross-jumps the then-copy into the else copy and the
    // surviving `lbz dmWep; cmpwi 0x21` is the target's dead compare. Two layout conditions make the
    // then-copy vanish whole: (1) the else copy's last arm must end in `b END` at jump2 entry -- the
    // `return` jumps over the `tail:` block whose store is dead (deleted in flow1, so `b END; tail: END:`
    // reaches jump2 with the jump intact) -- otherwise the then kind-0 body's last `stb` is matched
    // first against the code falling into END (1-insn fall-through candidate) and the whole-body
    // match is never tried; (2) the then-copy's arms are laid out 1, 2, default (em29DmRoutineSetLate),
    // so its kind-0 remnant `b` is not inverted around the kind-2 arm's remnant and the then-tree is
    // cross-jumped into the else tree. The tail's `dmg = 0` is also the dead store keeping the jump.
    if (em->dmg.m_Wep == 0x21) {
        em29DmRoutineSetLate(em, kind);
    } else {
        em29DmRoutineSet(em, kind);
    }
    return;
tail:
    dmg = 0;
}

Em29Func Em29_R0_move_tbl[4] = {
    em29_R0_Init,
    em29_R0_Move,
    em29_R0_Damage,
    em29_R0_Die,
};

static Em29Func Em29_R1_move_tbl[6] = {
    em29_R1_WaitLand,
    em29_R1_WaitCeiling,
    em29_R1_Walk,
    em29_R1_Turn,
    em29_R1_AtkDash,
    em29_R1_AtkRush,
};

static Em29Func Em29_R2_move_tbl[4] = {
    em29_R1_Dm_Air,
    em29_R1_Dm_Ceiling,
    em29_R1_Dm_Land,
    em29_R1_Dm_Recovery,
};

static Em29Func Em29_R3_move_tbl[3] = {
    em29_R1_Die_Normal,
    em29_R1_Die_Reset,
    em29_R1_Die_FadeOut,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em29_flip_tbl[22] = {
    0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 0,
};

// Bite attack (em29AtkCk): range, type, damage, ...
static EmAtkInfo em29_atk_tbl[1] = {
    { 350.0f, PL_DM_AUTO, 10, 1, 10, 0 },
};

// Per-frame update: damage check, clears the per-frame flags, the route check, the R0 table (Init /
// Move / Damage / Die), the airborne scenario check, the push-apart (em29ObaHitCk) and the "a bat is
// alive" ctrl12 tick (EM29_LIVE) for the room.
void cEm29::move()
{
    Em29Work* w = EM29_WK(this);

    if (r_no_0) {
        em29DmCk(this);
    }
    if (w->escTimer) {
        w->escTimer--;
    }
    if (w->atkTimer) {
        w->atkTimer--;
    }
    w->flags &= ~0x6F;
    em29RouteCk(this);
    Em29_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    em29ObaHitCk(this);
    atari.move();
    SatMgr.checkAir(this, 0);
    if (hp > 0 && (be_flag & 2)) {
        Ctrl12Set(w->pCtrl12, CTRL12_ID_EM29_LIVE, 2);
    }
    if (em29FriendCk(this) == 0) {
        EmSetDie(this);
    }
}

// R0 == 0: creation. Builds the model (ARC 5/6), hp 1000, no Ashley help, collision and hit box,
// initPos / initRot, the room's ctrl11 / ctrl12, and the start routine by cEm::set: 0 WaitLand (on the
// ground), 1 WaitCeiling (hanging).
static void em29_R0_Init(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    int one;
    int zero;

    one = 1;
    em->ot_type = one;
    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em29() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    em->be_flag &= ~0x10;
    em->Motion.flip = em29_flip_tbl;
    em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 300.0f, 300.0f, 300.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->scale.x = 1.5f;
    em->scale.y = 1.5f;
    em->scale.z = 1.5f;
    AtariInit(&em->atari, 0.0f, 0.0f, 0.0f, 250.0f, 100.0f, 100.0f, 100.0f, 1, 0x2800, 10);   // COMPILER-DIFF: #1
    em->atari.m_flag &= ~0x200;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInit(em, 0.0f, -30.0f, 0.0f, 100.0f, 60.0f, 5, YAT_FLAG_ON);
    EspDataLoad((u32) ARC(6), EFF_EM29, 0);
    w->flags = zero;
    w->atkTimer = 180;
    w->escTimer = zero;
    w->x90 = zero;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->tgtSpd.x = 0.0f;
    w->tgtSpd.y = 0.0f;
    w->tgtSpd.z = 0.0f;
    w->initPos = em->pos;
    w->initRot = em->ang;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    switch (em->set) {
    case 0:
    default:
        EmRoutineSet(em, one, zero, zero, zero);
        break;
    case 1:
        EmRoutineSet(em, 1, 1, zero, zero);
        break;
    }
    em->ang.y = fRand1_1() * PI;
    MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
    MotionMove(em, 0);
    em29_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em29_R1_move_tbl: WaitLand, WaitCeiling, Walk, Turn, AtkDash, AtkRush).
static void em29_R0_Move(cEm29* em)
{
    Em29_R1_move_tbl[em->r_no_1](em);
}

// R1 == 0 WaitLand: sits on the ground (flag bit5) with the idle squeaks until the player comes near
// or another bat takes off (em29FriendCk), then flies (Walk 2).
static void em29_R1_WaitLand(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    u8 se;

    w->flags |= 0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 5, Rnd() % 20);
        w->timer = Rnd() % 30;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->l_pl < 25000000.0f) {
            if (w->timer) {
                w->timer--;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 4:
        em->pos.y += 400.0f;
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
        MotionMove(em, 0);
        w->atkTimer = 180;
        EmRoutineSet(em, 1, 2, 0, 0);
        w->spd.x = 0.0f;
        w->spd.y = 80.0f;
        w->spd.z = 100.0f;
        break;
    }
    se = Rnd() % 6 + 21;
    Ctrl11SetSe(w->pCtrl11, em, Rnd() % 30 + 60, se, 9);
}

// R1 == 1 WaitCeiling: hangs from the ceiling (flag bit6) squeaking until disturbed, then drops and
// flies (Walk 2).
static void em29_R1_WaitCeiling(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    u8 se;

    w->flags |= 0x40;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 5, Rnd() % 20);
        w->timer = Rnd() % 30;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->l_pl < 25000000.0f) {
            if (w->timer) {
                w->timer--;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(9), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            w->atkTimer = 180;
            EmRoutineSet(em, 1, 2, 0, 0);
            w->spd.x = 0.0f;
            w->spd.y = -50.0f;
            w->spd.z = 100.0f;
        }
        break;
    }
    se = Rnd() % 6 + 21;
    Ctrl11SetSe(w->pCtrl11, em, Rnd() % 30 + 60, se, 9);
}

// R1 == 2 Walk: flies towards the target point (em29SetSPeed, between 100 and 2500 above the floor)
// for 10..20 loops, then Turn (3) or the attack (AtkDash 4 / AtkRush 5 when the rush lock allows);
// once ten bats have died (EM29_DIE count) the rest fly off and vanish (Die_FadeOut, R3 2).
static void em29_R1_Walk(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    int hit;

    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
        }
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y < SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0) + 500.0f) {
            w->tgtSpd.y = fRand0_1() * 50.0f + 50.0f;
        }
        w->tgtSpd.z = fRand1_1() * 30.0f + 120.0f;
        if (!(Rnd() & 1)) {
            w->escTimer = 15;
        }
        w->timer = (Rnd() & 0xA) + 10;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em29SetSPeed(em, 0.1f);
        MotionMove(em, 0);
        hit = Ctrl12CntCk(w->pCtrl12, CTRL12_ID_CNT_EM29_DIE, 10);
        if (hit) {
            em->hp = 0;
            EmRoutineSet(em, 3, 2, 0, 0);
        } else if (w->timer == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else {
            w->timer--;
            if (w->targetAngAbs > PI / 2.0f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
    em29CallSe(em, 0);
}

// R1 == 3 Turn: banks around towards the target, then Walk (2); the ten-dead check applies here too.
static void em29_R1_Turn(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    void* mot;

    switch (em->r_no_2) {
    case 0:
        if (w->targetAngAbs > 2.0943952f) {
            mot = ARC(0xF);
        } else if (w->targetAng < 0.0f) {
            mot = ARC(0xE);
        } else {
            mot = ARC(0xD);
        }
        MotionSetCore(em, MOTION(em), mot, 0, 0, 1, 0);
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y < SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0) + 500.0f) {
            w->tgtSpd.y = fRand0_1() * 50.0f + 50.0f;
        }
        w->tgtSpd.z = fRand1_1() * 40.0f + 80.0f;
        em->r_no_2++;
    case 1:
        em29SetSPeed(em, 0.5f);
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        if (Ctrl12CntCk(w->pCtrl12, CTRL12_ID_CNT_EM29_DIE, 10)) {
            em->hp = 0;
            EmRoutineSet(em, 3, 2, 0, 0);
        }
        break;
    }
    em29CallSe(em, 0);
}

// R1 == 4 AtkDash: a single swoop at the player (ARC 0x12): on the bite frame em29AtkCk hurts him and
// opens the swarm rush lock (ctrl12 EM29_RUSH 60 frames), then the pull-up (0x13) and Walk (2).
static void em29_R1_AtkDash(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x12), ARC(0x19), 5, 1, 0);
        w->escTimer = 0;
        w->atkHit = 0;
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = 0.0f;
        w->tgtSpd.z = 0.0f;
        w->timer = 5;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 32.0f);
        }
        em29SetSPeed(em, 0.3f);
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else if ((em->Motion.Seq_old.Free & 1) && w->atkHit == 0 && em29AtkCk(em, 0)) {
            Ctrl12Set(w->pCtrl12, CTRL12_ID_EM29_RUSH, 60);
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 0, 1, 0);
        w->escTimer = 0;
        w->atkHit = 0;
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        w->tgtSpd.z = 130.0f;
        em->r_no_2++;
    case 3:
        em29SetSPeed(em, 0.5f);
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em29CallSe(em, 0);
}

// R1 == 5 AtkRush: the swarm rush (flag bit4) while the EM29_RUSH lock is open: flutters at the
// player's head, each contact takes 20 hp with blood half the time and kills him at 0; ends into
// Walk (2) when the lock closes or the player dies.
static void em29_R1_AtkRush(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 2, 5, 0);
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.z = fRand1_1() * 25.0f + 150.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y > 1800.0f) {
            w->tgtSpd.y = fRand0_1() * -100.0f;
        }
        if (em->pos.y < 800.0f) {
            w->tgtSpd.y = fRand0_1() * 100.0f;
        }
        w->timer = Rnd() % 10 + 10;
        em->r_no_2++;
    case 1: {
        f32 dy;

        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 10.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em29SetSPeed(em, 0.1f);
        MotionMove(em, 0);
        dy = fabsf(em->pos.y - (pPL->pos.y + 1300.0f));
        if (w->targetAngAbs < PI / 8.0f && w->targetDist < 360000.0f && dy < 400.0f && w->escTimer == 0) {
            em->r_no_2++;
        } else if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2 = 0;
        }
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 2, 5, 0);
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.z = 0.0f;
        w->tgtSpd.y = fRand1_1() * 3.0f;
        if (em->pos.y < 1000.0f) {
            w->tgtSpd.y = fRand0_1() * 10.0f;
        }
        w->escTimer = 0;
        w->timer = Rnd() % 20 + 20;
        if ((s16) pG->pl_life > 0) {
            LifeDownSet2(pPL, 20, 0, 0);
            if (!(Rnd() & 1)) {
                cModel* p = GetPartsAddr(em->pParts, 2);

                EmPlBloodSet(em, &p->world, 1, 0xFF, 0xFF);
                VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xA, 1);
            }
            if ((s16) pG->pl_life <= 0) {
                PlSetDamage(PL_DM_UP_FRONT, 0, 0);
            }
        }
        em->r_no_2++;
    case 3:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em29SetSPeed(em, 0.5f);
        MotionMove(em, 0);
        if (w->timer == 0) {
            em->r_no_2 = 0;
            w->escTimer = 15;
        } else {
            w->timer--;
            if (w->targetDist > 640000.0f) {
                em->r_no_2 = 0;
                w->escTimer = 15;
            }
        }
        break;
    }
    if (Ctrl12Ck(w->pCtrl12, CTRL12_ID_EM29_RUSH) == 0 || (s16) pG->pl_life <= 0) {
        w->escTimer = 30;
        w->atkTimer = 180;
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        em29CallSe(em, 1);
    }
}

// R0 == 2: damage (flag bit3), runs Em29_R2_move_tbl (Dm_Air, Dm_Ceiling, Dm_Land, Dm_Recovery).
static void em29_R0_Damage(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    w->flags |= 8;
    Em29_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Air: shot in flight: tumbles down spinning (dmRot, grav) onto the floor found
// below (mirrored by r_no_3), then Die_Normal when dead or Dm_Recovery (R2 3).
static void em29_R1_Dm_Air(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    Vec a;
    Vec hit;
    int flag;

    switch (em->r_no_2) {
    case 0:
        em->r_no_3 = Rnd() & 1;
        flag = 1;
        if (em->r_no_3) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, flag, 0);
        w->grav = fRand0_1() * 10.0f + 3.0f;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        w->spd.y -= w->grav;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        a.x = em->pos.x;
        a.y = em->pos_old.y;
        a.z = em->pos.z;
        if (SatMgr.hitCheck(&a, &em->pos, &hit, 0, 0, 0)) {
            em->pos.y = hit.y;
            MotionMove(em, 0);
            em->r_no_2 = 4;
        } else if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        flag = 1;
        if (em->r_no_3) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, flag, 0);
        w->dmRot = fRand0_1() * 0.31415927f + 0.31415927f;
        if (Rnd() & 1) {
            w->dmRot = -w->dmRot;
        }
        em->r_no_2++;
    case 3:
        em->ang.y += w->dmRot;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        w->spd.y -= w->grav;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        a.x = em->pos.x;
        a.y = em->pos_old.y;
        a.z = em->pos.z;
        if (SatMgr.hitCheck(&a, &em->pos, &hit, 0, 0, 0)) {
            em->pos.y = hit.y;
            MotionMove(em, 0);
            em->r_no_2 = 4;
        } else {
            MotionMove(em, 0);
        }
        break;
    case 4:
        flag = 1;
        if (em->r_no_3) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, flag, 0);
        if (em->be_flag & 2) {
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 1 Dm_Ceiling: shot off the ceiling: drops spinning to the floor, then Die_Normal or Dm_Recovery.
static void em29_R1_Dm_Ceiling(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    Vec a;
    Vec hit;
    int flag;

    switch (em->r_no_2) {
    case 0:
        em->r_no_3 = Rnd() & 1;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        flag = 1;
        if (em->r_no_3) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, flag, 0);
        w->grav = fRand0_1() * 10.0f + 3.0f;
        w->dmRot = fRand0_1() * 0.31415927f + 0.31415927f;
        if (Rnd() & 1) {
            w->dmRot = -w->dmRot;
        }
        em->r_no_2++;
    case 1:
        em->ang.y += w->dmRot;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        w->spd.y -= w->grav;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        a.x = em->pos.x;
        a.y = em->pos_old.y;
        a.z = em->pos.z;
        if (SatMgr.hitCheck(&a, &em->pos, &hit, 0, 0, 0)) {
            em->pos.y = hit.y;
            MotionMove(em, 0);
            em->r_no_2++;
        } else {
            MotionMove(em, 0);
        }
        break;
    case 2:
        flag = 1;
        if (em->r_no_3) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, flag, 0);
        if (em->be_flag & 2) {
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 2 Dm_Land: shot on the ground: the flinch, then Die_Normal when dead or Dm_Recovery.
static void em29_R1_Dm_Land(cEm29* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 3 Dm_Recovery: the surviving bat gets up and returns to WaitLand (R1 0).
static void em29_R1_Dm_Recovery(cEm29* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x10), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

// R0 == 3: death (flag bit3), runs Em29_R3_move_tbl (Die_Normal, Die_Reset, Die_FadeOut).
static void em29_R0_Die(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    w->flags |= 8;
    Em29_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: the dead bat lies 30..60 frames and fades out; when the room still has
// bats to spend (em29LastCk) it goes to Die_Reset to respawn.
static void em29_R1_Die_Normal(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.m_flag &= ~0x100;
        w->timer = Rnd() % 30 + 30;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            em->invisible_factor -= 0.02f;
            if (em->invisible_factor < 0.0f) {
                em->invisible_factor = 0.0f;
                em->be_flag &= ~2;
                if (em29FriendCk(em) && !(w->flags & 0x80)) {
                    EmRoutineSet(em, 3, 1, 0, 0);
                } else {
                    em->r_no_2++;
                }
            }
        }
        break;
    }
}

// R0 3 / R1 == 1 Die_Reset: respawns the bat at its creation position (initPos / initRot, hp 1000),
// fades it in and sends it flying (Walk 2).
static void em29_R1_Die_Reset(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    cModel* p;

    switch (em->r_no_2) {
    case 0:
        em->pos = w->initPos;
        em->ang = w->initRot;
        em->pos_old = em->pos;
        em->Motion.Mot_flag &= ~0x40000000;
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 1, 0);
        MotionMove(em, 0);
        PartsWorldPosCalc(em);
        for (p = em->pParts; p; p = p->pParts) {
            p->world_old = p->world;
            p->world_old2 = p->world_old;
        }
        em->hp = 1000;
        em->atari.m_flag |= 0x100;
        em->invisible_factor = 0.0f;
        em->be_flag |= 2;
        em->r_no_2++;
    case 1:
        em->invisible_factor += 0.1f;
        if (em->invisible_factor > 1.0f) {
            em->invisible_factor = 1.0f;
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
            w->tgtSpd.x = 0.0f;
            w->tgtSpd.y = 0.0f;
            w->tgtSpd.z = 0.0f;
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 2 Die_FadeOut: the swarm is spent (ten dead): the bat flies off towards its start point
// and fades out for good.
static void em29_R1_Die_FadeOut(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
        w->tgtSpd.z = fRand1_1() * 25.0f + 75.0f;
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y < 1600.0f) {
            w->tgtSpd.y = fRand0_1() * 50.0f + 50.0f;
        }
        w->escTimer = 100;
        w->timer = (Rnd() & 0xA) + 10;
        em->hp = 0;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em29SetSPeed(em, 0.1f);
        MotionMove(em, 0);
        em->invisible_factor -= 0.1f;
        if (em->invisible_factor < 0.0f) {
            em->invisible_factor = 0.0f;
            em->be_flag &= ~2;
            em->r_no_2++;
        }
        break;
    }
}

// Per frame: the route point / angle to the player (routePos, routeAng, flag bit0 = found) and the
// target (targetPos / targetAng / targetDist): the player, the start point for a fading bat, or the
// escape point while escTimer runs.
void em29RouteCk(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (w->flags & 0x20) {
        return;
    }
    if (w->flags & 0x40) {
        return;
    }
    if ((pG->Frame_cnt & 3) != (em->emset_no & 3)) {
        return;
    }
    if (RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->r_no_0 == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->l_pl = 100000000.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->l_pl;
    w->pTarget = pPL;
    if ((em->pos.x - w->initPos.x) * (em->pos.x - w->initPos.x) + (em->pos.z - w->initPos.z) * (em->pos.z - w->initPos.z)
        > em->Guard_r * em->Guard_r) {
        RouteCkToPos(em, &w->initPos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist = (em->pos.x - w->initPos.x) * (em->pos.x - w->initPos.x)
                        + (em->pos.z - w->initPos.z) * (em->pos.z - w->initPos.z);
    }
    if (!(w->flags & 0x10)) {
        if (em->l_pl < 360000.0f) {
            w->escTimer = 45;
        }
    }
    w->flags &= ~0x10;
    if (w->escTimer) {
        RouteCkEscEm(em, w->pTarget, &w->targetPos);
    }
}

// Blends the speed spd towards tgtSpd by `rate`, moves the bat along its heading and keeps it between
// 100 and 2500 above the floor.
void em29SetSPeed(cEm29* em, f32 rate)
{
    Em29Work* w = EM29_WK(em);
    Mtx m;
    Vec v;
    f32 fl;

    w->spd.x = w->spd.x * (1.0f - rate) + w->tgtSpd.x * rate;
    w->spd.y = w->spd.y * (1.0f - rate) + w->tgtSpd.y * rate;
    w->spd.z = w->spd.z * (1.0f - rate) + w->tgtSpd.z * rate;
    PSMTXRotRad(m, 'y', em->ang.y);
    PSMTXMultVecSR(m, &w->spd, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    if (em->pos.y > fl + 2500.0f) {
        em->pos.y = fl + 2500.0f;
    }
    if (em->pos.y < fl + 100.0f) {
        em->pos.y = fl + 100.0f;
    }
}

// Pushes the bat out of other alive enemies and out of the player's collision radius so the swarm
// does not overlap.
void em29ObaHitCk(cEm29* em)
{
    Vec d;
    f32 len;
    f32 r;
    u32 i;

    if (em->hp <= 0) {
        return;
    }
    if (!(em->be_flag & 2)) {
        return;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.at(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x29) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        PSVECSubtract(&em->pos, &e->pos, &d);
        len = d.x * d.x + d.y * d.y + d.z * d.z;
        if (len > 40000.0f) {
            continue;
        }
        if (len <= 0.0f) {
            continue;
        }
        len = SQRTF(len) * 0.9f + 20.0f;
#line 1674 "D:/Bio4/Prog/em29.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&e->pos, &d, &em->pos);
        if (em->pos.y < 0.0f) {
            em->pos.y = 0.0f;
        }
    }
    if (em->pos.y > pPL->pos.y + 1600.0f) {
        return;
    }
    if (em->pos.y < pPL->pos.y - 200.0f) {
        return;
    }
    PSVECSubtract(&em->pos, &pPL->pos, &d);
    d.y = 0.0f;
    len = d.x * d.x + d.z * d.z;
    r = pPL->atari.m_radius2 + 100.0f;
    if (len > r * r) {
        return;
    }
    if (len <= 0.0f) {
        return;
    }
#line 1691 "D:/Bio4/Prog/em29.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, r);
    em->pos.x = pPL->pos.x + d.x;
    em->pos.z = pPL->pos.z + d.z;
}

// 1 when this is the last alive bat (id 0x29) in the room (Die_Normal then respawns it).
int em29LastCk(cEm29* em)
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
        if (e->id != 0x29) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (!(e->be_flag & 2)) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        return 0;
    }
    return 1;
}

// 1 when another bat is already alive / flying (a waiting bat takes off with it); 0 once ten have died.
int em29FriendCk(cEm29* em)
{
    u32 i;

    if (Ctrl12CntCk(EM29_WK(em)->pCtrl12, CTRL12_ID_CNT_EM29_DIE, 10)) {
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
        if (e->id != 0x29) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        if (e->hp > 0) {
            return 1;
        }
    }
    return 0;
}

// Wing flap + squeak SEs through the room's ctrl11 (`type` 0 flying, else the rush variant).
void em29CallSe(cEm29* em, int type)
{
    Em29Work* w = EM29_WK(em);
    u8 se;

    if (type) {
        se = Rnd() % 2 + 4;
        Ctrl11SetSe(w->pCtrl11, em, Rnd() % 3 + 4, se, 8);
        se = Rnd() % 5 + 16;
        Ctrl11SetSe(w->pCtrl11, em, Rnd() % 10 + 20, se, 9);
    } else {
        se = (Rnd() & 7) + 6;
        Ctrl11SetSe(w->pCtrl11, em, (Rnd() & 3) | 4, se, 8);
        se = Rnd() % 6 + 21;
        Ctrl11SetSe(w->pCtrl11, em, Rnd() % 10 + 20, se, 9);
    }
}

// Bite hit test on the swoop frame: em29_atk_tbl[no] against the player (blood, the plem29_BatRush
// damage routine) or the partner, once per attack (atkHit). 1 = hit.
int em29AtkCk(cEm29* em, int no)
{
    Em29Work* w = EM29_WK(em);
    EmAtkInfo* atk = &em29_atk_tbl[no];
    cModel* p = GetPartsAddr(em->pParts, 2);
    int hit = EmAtkHitCk(atk, &p->world, &p->world_old2, 0);

    if (hit) {
        if (hit & 1) {
            EmPlBloodSet(em, &p->world, 1, 0xFF, 0xFF);
            w->atkHit = 1;
            pPL->subArc = em->subArc;
            SetPlDamage(em, plem29_BatRush);
        }
        if (hit & 2) {
            EmSubBloodSet(em, &p->world, 1, 0xFF, 0xFF);
            w->atkHit = 1;
        }
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        return 1;
    }
    return 0;
}

// Player damage routine of the bat bite: the flinch with the damage SE, then EndPlDamage.
static void plem29_BatRush(cPlayer* pl)
{
    pl->dmg.set(0, 10);
    pl->subArc = pPL->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x18), 0, 3, 5, 0);
        PlSetDamageSe(0);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}
