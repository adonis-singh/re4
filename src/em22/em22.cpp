// em22 module (D:/Bio4/Prog/em22.cpp): the dog. It hunts the player along the route (Em22RouteCk,
// em22_R1_Run / Turn / Threat / SideStep), jumps at him (em22_R1_JumpAtk, plem22_JumpAtkHit), escapes
// when hurt (em22_R1_Escape) and grows parasites out of its back (em22SetParasite, em22_R1_ParaAtk).

#include "atari.h"
#include "ctrl.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "dmg.h"
#include "em22.h"
#include "em10.h"
#include "emdoor.h"
#include "obj16.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "item.h"
#include "pl_wep.h"
#include "dbmodule.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"

// The module's 0x34-byte COMMON block: uninitialised template statics of the original object,
// merged into .bss by the REL link.
asm(".comm common_em22,52,4");



typedef void (*Em22Func)(cEm22*);

static void em22_R0_Init(cEm22* em);
static void em22_R0_Move(cEm22* em);
static void em22_R1_br_dummy(cEm22* em);
static void em22_R1_Goto(cEm22* em);
static void em22_R1_R11B_A(cEm22* em);
static void em22_R1_R11B_B(cEm22* em);
static void em22_R1_R11B_C(cEm22* em);
static void em22_R1_InCage(cEm22* em);
static void em22_R1_JumpWait(cEm22* em);
static void em22_R1_Wait(cEm22* em);
static void em22_R1_RunAbout(cEm22* em);
static void em22_R1_Run(cEm22* em);
static void em22_R1_Turn(cEm22* em);
static void em22_R1_Escape(cEm22* em);
static void em22_R1_Threat(cEm22* em);
static void em22_R1_SideStep(cEm22* em);
static void em22_R1_br_JumpAtk(cEm22* em);
static void em22_R1_JumpAtk(cEm22* em);
static void em22_R1_JumpAtkHit(cEm22* em);
static void plem22_JumpAtkHit(cPlayer* pl);
static void em22_R1_br_ParaAtk(cEm22* em);
static void em22_R1_ParaAtk(cEm22* em);
static void em22_R1_ParaAtkHit(cEm22* em);
static void plem22_ParaAtkHit(cPlayer* pl);
static void em22_R1_Wakeup(cEm22* em);
static void em22_R1_Parasite(cEm22* em);
static void em22_R1_Jump(cEm22* em);
static void em22_R0_Damage(cEm22* em);
static void em22_R1_Dm_Small(cEm22* em);
static void em22_R1_Dm_Blow(cEm22* em);
static void em22_R0_Die(cEm22* em);
static void em22_R1_Die_Lost(cEm22* em);




// Module entry (SN loader): registers Em22Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    EmInitFunc = Em22Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm22 class in the manager's work.
void Em22Init(cEm* em)
{
    new (em) cEm22();
}

// Per-frame damage check (cEm22::move): an explosion / fire damage volume kills the dog outright
// (work flag 0x400, Dm_Blow). A weapon hit takes GetWepDmVal off hp with the blood effect and yelp;
// a kill goes to Dm_Blow (R2 1); otherwise handguns / knife / TMP flinch (Dm_Small, three times in
// four), heavy weapons and explosives blow it away, a near shotgun hit mostly blows it away.
void em22DmCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    int near;

    if (em->hp > 0 && EmDeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            EmSetDie(em);
            EmReserveDropItem(em);
            em->hp = 0;
            w->flags |= 0x400;
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    near = 0;
    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    LifeDownSet2(em, em22SetDmVal(em), 0, 0);
    em22BloodSet(em);
    SndCall(8, 0xF, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        em->r_no_0 = 2;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        return;
    }
    if (w->flags & 8) {
        em->r_no_0 = 2;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        return;
    }
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (w->flags & 0x22) {
            return;
        }
        if (Rnd() & 3) {
            EmRoutineSet(em, 2, 0, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2C:
    case 0x2D:
    default:
        EmRoutineSet(em, 2, 1, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 1, 0, 0);
            } else {
                EmRoutineSet(em, 2, 0, 0, 0);
            }
        } else {
            if (w->flags & 0x22) {
                return;
            }
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    }
}

Em22Func Em22_R0_move_tbl[5] = {
    em22_R0_Init,
    em22_R0_Move,
    em22_R0_Damage,
    em22_R0_Die,
    (Em22Func) Em_R0_Scenario,
};

// Pairs of {branch check, routine} per routine-1 state (em22_R0_Move calls both).
static Em22Func Em22_R1_move_tbl[40] = {
    em22_R1_br_dummy, em22_R1_Goto,
    em22_R1_br_dummy, em22_R1_R11B_A,
    em22_R1_br_dummy, em22_R1_R11B_B,
    em22_R1_br_dummy, em22_R1_R11B_C,
    em22_R1_br_dummy, em22_R1_InCage,
    em22_R1_br_dummy, em22_R1_JumpWait,
    em22_R1_br_dummy, em22_R1_Wait,
    em22_R1_br_dummy, em22_R1_RunAbout,
    em22_R1_br_dummy, em22_R1_Run,
    em22_R1_br_dummy, em22_R1_Turn,
    em22_R1_br_dummy, em22_R1_Escape,
    em22_R1_br_dummy, em22_R1_Threat,
    em22_R1_br_dummy, em22_R1_SideStep,
    em22_R1_br_JumpAtk, em22_R1_JumpAtk,
    em22_R1_br_dummy, em22_R1_JumpAtkHit,
    em22_R1_br_ParaAtk, em22_R1_ParaAtk,
    em22_R1_br_dummy, em22_R1_ParaAtkHit,
    em22_R1_br_dummy, em22_R1_Wakeup,
    em22_R1_br_dummy, em22_R1_Parasite,
    em22_R1_br_dummy, em22_R1_Jump,
};

static Em22Func Em22_R2_move_tbl[2] = {
    em22_R1_Dm_Small,
    em22_R1_Dm_Blow,
};

static Em22Func Em22_R3_move_tbl[1] = {
    em22_R1_Die_Lost,
};

// Parts index remap of the flipped motions (cModel::motFlip): the legs swapped.
static u16 em22_flip[50] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x07, 0x0D, 0x0E, 0x0F, 0x10, 0x09, 0x0A, 0x0B,
    0x0C, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31,
};
// The parasite's remap (em22SetParasite): the identity.
static u16 em22_para_flip[80] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};
// The original link 8-aligns the end of .data (the ngcld BSS tag follows): the 4 pad bytes after the table.
asm(".section .data\n\t.balign 8\n\t.text");

// Per-frame update: clears the per-frame work flags, damage check, forgets the target when the player
// is dead (plDeadWait), route check (Em22RouteCk), the R0 table, then the neck / body tilt,
// collision, scenario check (checkAir while jumping, flag bit7), the back parasites' upkeep (growl
// timer, em22ParaSetMotWait), the slaver drool effect, foot effects and SEs.
void cEm22::move()
{
    Em22Work* w = EM22_WK(this);
    f32 len;
    u16 atFlags;

    em22DmCk(this);
    w->flags &= ~0x2AE;
    if (w->plDeadWait) {
        w->plDeadWait--;
    }
    if (EmDeadCk(pPL)) {
        w->plDeadWait = 30;
    }
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
    Em22RouteCk(this);
    Em22_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (hp > 0) {
        em22OpenBack(this);
    }
    em22NeckMove(this);
    partsWorldCalc();
    em22ScaleCompress(this);
    len = VEC_DISTXZ(&pos, &pos_old);
    atFlags = atari.m_flag;
    if (w->flags & 0x80) {
        atari.m_flag |= 0x10;
    } else {
        atari.m_flag &= ~0x10;
    }
    EmAtCheck(this);
    atari.move();
    if (w->flags & 0x80) {
        SatMgr.checkAir(this, 0x180800);
    } else {
        SatMgr.check(this, 0);
    }
    atari.m_flag = atFlags;
    if (VEC_DISTXZ(&pos, &pos_old) < len * 0.5f) {
        w->stuckTimer = 3;
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em22FootSeControl(this);
    if (hp > 0 && w->pPara[0]) {
        if (w->voiceTimer) {
            w->voiceTimer--;
        } else {
            w->voiceTimer = 30;
            SndCall(8, 0x28, &pos, id, 0, this);
        }
    }
    if ((w->flags & 0x10) && hp > 0) {
        if (be_flag & 0x800) {
            EstSet(this, -1, 0, 0, EFF_EM22, 0x10, 1, ESP_CORE_KIND_NONE, this, 0);
        } else {
            EstSet(this, -1, 0, 0, EFF_EM22, 0x10, 0, ESP_CORE_KIND_NONE, this, 0);
        }
    }
    em22FootEff(this);
    em22AtkParaClearCk(this);
}

// R0 == 0: creation. Builds the model (ARC 5/6, remap tables), collision and hit boxes
// (em22YarareInit), the room's ctrl11 / ctrl12 controls, the parasite grow delay paraWait (300..1200
// frames), water flag, and the start routine by cEm::set: 0 Wait (6), 1 R11B_A (room 11B kennel
// dog), 2 R11B_B, 3 R11B_C, 4 InCage, 5 JumpWait.
static void em22_R0_Init(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    void* mot;
    int zero;
    u32 i;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em22() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    zero = 0;
    mot = MOTION(em);
    EspDataLoad((u32) ARC(6), EFF_EM22, 0);
    em->Motion.flip = em22_flip;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->atari.init(0.0f, 0.0f, 0.0f, 450.0f, 400.0f, 400.0f, 500.0f, 3, 0x2000, 10);
    em22YarareInit(em);
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    w->tilt = 0.0f;
    w->stuckTimer = zero;
    w->neckX = 0.0f;
    w->neckY = 0.0f;
    w->sndId[0] = zero;
    w->sndId[1] = zero;
    w->sndId[2] = zero;
    w->slaverTimer = zero;
    w->x2D0 = zero;
    w->plDist = 100000000.0f;
    w->scale = 1.0f;
    w->paraWait = (int) Rnd() % 900 + 300;
    w->voiceTimer = zero;
    for (i = 0; i < 5; i++) {
        w->pPara[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        w->pParaAtk[i] = 0;
    }
    w->espKind = EspPullCoreKind();
    if (em->be_flag & 0x800) {
        EstSet(em, -1, 0, 0, EFF_EM22, 2, 1, w->espKind, em, 0);
    } else {
        EstSet(em, -1, 0, 0, EFF_EM22, 2, 0, w->espKind, em, 0);
    }
    em->setStatus(EM_STATUS_ACTIVE);
    switch (em->set) {
    case 0:
    default:
        EmRoutineSet(em, 1, 6, 0, 0);
        break;
    case 1:
        EmRoutineSet(em, 1, 1, 0, 0);
        break;
    case 3:
        EmRoutineSet(em, 1, 3, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 1, em->set, 0, 0);
        break;
    case 4:
        EmRoutineSet(em, 1, em->set, 0, 0);
        break;
    case 5:
        EmRoutineSet(em, 1, em->set, 0, 0);
        break;
    }
    if (em->flag & 0x40000000) {
        em22SetParasite(em);
    }
    MotionSetCore(em, mot, ARC(7), 0, 0, 5, 0);
    MotionMove(em, 0);
    em22_R0_Move(em);
}

// R0 == 1: runs the branch check and the move handler of R1 (Em22_R1_move_tbl pairs).
static void em22_R0_Move(cEm22* em)
{
    Em22_R1_move_tbl[em->r_no_1 * 2](em);
    Em22_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the routines that have none.
static void em22_R1_br_dummy(cEm22* em)
{
}

// The run motion of the enemy (five variants by list number) with its blend sequence.
static inline void em22SetRunMotion(cEm22* em)
{
    switch ((u8) (em->emset_no % 5)) {
    case 0:
    default:
        MotionSetCore(em, MOTION(em), ARC(9), ARC(0x21), 10, 5, 0);
        break;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(9), ARC(0x22), 10, 5, 0);
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(9), ARC(0x23), 10, 5, 0);
        break;
    case 3:
        MotionSetCore(em, MOTION(em), ARC(9), ARC(0x24), 10, 5, 0);
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(9), ARC(0x25), 10, 5, 0);
        break;
    }
}

// Turn towards the route point (slower while the enemy is stuck) and face the run direction. A macro:
// as an inline taking `w` the LIMIT_ANGLE result gets copied (`fmr f0, f1`) before the store.
#define em22RunTurn(em, w)                                                                        \
    if ((w)->stuckTimer) {                                                                        \
        lim = 0.058904864f;                                                                       \
    } else {                                                                                      \
        lim = 0.078539819f;                                                                       \
    }                                                                                             \
    (em)->ang.y += Muku(&(em)->pos, &(w)->routePos, (em)->ang.y, lim);                            \
    (em)->ang.y = LIMIT_ANGLE((em)->ang.y);                                                       \
    em22DirMatrix(em, Muku(&(em)->pos, &(w)->routePos, (em)->ang.y, PI))

// R1 == 0 Goto: runs to gotoPos (cEm22::setGoto) and drops into Wait (6) when there or when the
// order is cancelled.
static void em22_R1_Goto(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        em22SetRunMotion(em);
        em->r_no_2++;
    case 1:
        em22RunTurn(em, w);
        MotionMove(em, 0);
        if ((em->pos.x - w->gotoPos.x) * (em->pos.x - w->gotoPos.x) + (em->pos.y - w->gotoPos.y) * (em->pos.y - w->gotoPos.y) +
                (em->pos.z - w->gotoPos.z) * (em->pos.z - w->gotoPos.z) <
            1000000.0f) {
            em->r_no_0 = 1;
            em->r_no_1 = 6;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            w->gotoOn = 0;
        }
        break;
    }
    if (em22JumpCk(em) == 0) {
        em22DoorOpenCk(em);
        em22SlaverSet(em, 1);
    }
}

// R1 == 1: room 11B kennel dog A: breaks out of the kennel (motion 0x44), then the idle 0xD until the
// player is near, then Threat (0xB).
static void em22_R1_R11B_A(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x44), ARC(0x46), 0, 5, 0);
        w->timer = 58;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x27), 10, 5, 0);
        w->timer = 252;
        em->r_no_2++;
    case 3:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        }
        break;
    }
    em22SlaverSet(em, 0);
}

// R1 == 2: room 11B kennel dog B: idles (0xD), jumps down from its ledge (0xB / 0xC landing on the
// floor found below), shakes itself (0xE), idles again and goes to Threat (0xB) when the player is near.
static void em22_R1_R11B_B(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        em->atari.m_flag &= ~0x300;
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x27), 10, 5, 0);
        w->timer = 150;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xB), 0, 10, 1, 0);
        em->r_no_2++;
    case 3: {
        Vec v;
        f32 fl;

        em22DirMatrix(em, 0.0f);
        MotionMove(em, 0);
        v = em->pos;
        v.y = em->pos_old.y;
        fl = SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0);
        if (em->pos.y > fl) {
            break;
        }
        em->pos.y = fl;
        em->r_no_2++;
    }
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 3, 5, 0);
        em->atari.m_flag |= 0x300;
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xE), ARC(0x2C), 3, 1, 0);
        if (em->be_flag & 0x800) {
            EstSet(em, -1, 0, 0, EFF_EM22, 6, 1, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM22, 6, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        em->r_no_2++;
    case 7:
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        } else if (em->Motion.Seq_old.Free & 1) {
            em22SetParasite(em);
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x27), 10, 5, 0);
        w->timer = 28;
        em->r_no_2++;
    case 9:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        }
        break;
    }
    em22SlaverSet(em, 0);
}

// R1 == 3: room 11B kennel dog C (a decoy): idles from frame 15 of motion 0xD and, once the room
// releases it, vanishes (hp 0, invisible, inactive).
static void em22_R1_R11B_C(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        em->atari.m_flag &= ~0x300;
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x27), 10, 0x45, 0xF);
        w->timer = 310;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->timer == 0) {
            em->hp = 0;
            em->clearStatus(EM_STATUS_ACTIVE);
            em->invisible_factor = 0.0f;
            em->be_flag &= ~2;
            em->be_flag |= 0x4000;
            em->r_no_2++;
        } else {
            w->timer--;
            em22SlaverSet(em, 0);
        }
        break;
    }
}

// R1 == 4 InCage: idles (0xD) inside the cage until the room's release flag, then Run (8).
static void em22_R1_InCage(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x27), 0, 4, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->flag & 1) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    em22SlaverSet(em, 0);
}

// R1 == 5 JumpWait: waits (motion 7) until the player is seen / near, then leaps down (0x59, flag
// bit3 = no damage switch, collision through) and goes to RunAbout (7).
static void em22_R1_JumpWait(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em22GotoCk(em)) {
            return;
        }
        if ((w->flags & 1) && em->l_pl < em->Guard_r * em->Guard_r) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return;
        }
        if (em->flag & 1) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x59), ARC(0x5A), 3, 1, 0);
        SndCall(8, 8, &em->pos, em->id, 0, em);
        w->timer = 20;
        em->r_no_2++;
    case 3:
        w->flags |= 8;
        if (w->timer) {
            w->timer--;
            em->dmg.m_Timer = 2;
            em->atari.throughOn();
        } else {
            em->atari.throughOff();
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            if (em22GotoCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 7, 0, 0);
        } else {
            em22DoorOpenCk(em);
        }
        break;
    }
    em22SlaverSet(em, 0);
}

// R1 == 6 Wait: idle (motion 7), turning slowly to the route point; the player seen, near, or the
// room's forced alert (Status_flg[0] bit23) starts RunAbout (7).
static void em22_R1_Wait(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->routeAngAbs > 0.7853982f) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x44), ARC(0x46), 10, 5, 0);
        em->r_no_2++;
    case 3:
        em->ang.y += Muku(&em->pos, &w->routePos, em->ang.y, 0.049087387f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em22DirMatrix(em, 0.0f);
        MotionMove(em, 0);
        if (w->routeAngAbs < 0.2617994f) {
            em->r_no_2 = 0;
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em->flag & 1) {
        em->flag &= ~1;
        EmRoutineSet(em, 1, 7, 0, 0);
        return;
    }
    if ((w->flags & 1) && em->l_pl < em->Guard_r * em->Guard_r) {
        EmRoutineSet(em, 1, 7, 0, 0);
        return;
    }
    {
        // the zero of the last routine set is a block-local pseudo set before the test (its
        // `li` lands at the top of the test block, above the pG load)
        int zero = 0;
        if (StaFlagChk(pG, STA_PL_FIRE) && em->l_pl < 625000000.0f) {
            EmRoutineSet(em, 1, 7, zero, zero);
            return;
        }
    }
    em22SlaverSet(em, 0);
}

// R1 == 7 RunAbout: runs the route towards the player for 3..5 steps (em22SetRunMotion), then
// Threat (0xB) when close, Turn (9) when the target is behind, Run (8) after a fleeing player,
// Escape (0xA) when hurt, or grows a parasite (0x12) when em22SetParasiteCk allows.
static void em22_R1_RunAbout(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        em->r_no_3 = 1;
        em22SetRunMotion(em);
        w->timer = (u8) (Rnd() % 3) + 3;
        em->r_no_2++;
    case 1:
        em22RunTurn(em, w);
        MotionMove(em, 0);
        if ((w->flags & 1) && em->l_pl < 49000000.0f && w->routeAngAbs < 0.5235988f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if (w->stuckCnt > 5) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (em22PlRunCk(em) && w->plDist > 5000.0f) {
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em->l_pl < 6250000.0f) {
        EmRoutineSet(em, 1, 0xA, 0, 0);
    } else if (em22SetParasiteCk(em)) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
    } else if (em22JumpCk(em) == 0) {
        if (w->targetAngAbs > 2.0943952f) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else {
            em22DoorOpenCk(em);
            em22SlaverSet(em, 1);
        }
    }
}

// R1 == 8 Run: the charge at the player: within 4500 units in front with a clear line and on screen
// it jumps (JumpAtk 0xD, half the time) or slows to RunAbout (7); Turn (9) when the route bends away.
static void em22_R1_Run(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;
    Vec a;
    Vec b;
    int hit;

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        em22SetRunMotion(em);
        em->r_no_2++;
    case 1:
        em22RunTurn(em, w);
        MotionMove(em, 0);
        if (em->l_pl < 20250000.0f && w->routeAngAbs < 0.5235988f && em22PlRunCk(em) == 0) {
            if (w->plDeadWait == 0) {
                a = em->pos;
                b = pPL->pos;
                a.y += 500.0f;
                b.y += 500.0f;
                if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0 && em22ScreenInCk(em) && (u8) (Rnd() % 10) > 4 && (w->flags & 1)) {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 7, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        } else if (em->l_pl < 4000000.0f && w->routeAngAbs < 0.5235988f) {
            if (w->plDeadWait == 0) {
                a = em->pos;
                b = pPL->pos;
                a.y += 500.0f;
                b.y += 500.0f;
                hit = SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
                if (hit == 0 && em22ScreenInCk(em) && (u8) (Rnd() % 10) > 4 && (w->flags & 1)) {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 7, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em22SetParasiteCk(em)) {
        em22SetParasite(em);
    }
    if (em22JumpCk(em)) {
        return;
    }
    if (w->targetAngAbs > 2.0943952f) {
        EmRoutineSet(em, 1, 9, 0, 0);
    } else {
        em22DoorOpenCk(em);
        em22SlaverSet(em, 1);
    }
}

// R1 == 9 Turn: turns towards the route point (delta = angle left), then Escape (0xA) when hurt, Run
// (8) after a running player, RunAbout (7) or Threat (0xB).
static void em22_R1_Turn(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 ang;
    f32 d;

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        ang = LIMIT_ANGLE(GetXZAngle(&em->pos, &w->routePos) - em->ang.y);
        d = fabsf(ang);
        if (d < 0.7853982f) {
            MotionSetCore(em, MOTION(em), ARC(0x3E), ARC(0x41), 3, 1, 0);
            w->delta = 0.0f;
        } else if (ang < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x3F), ARC(0x42), 3, 0x41, 0);
            w->delta = -1.5707964f;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3F), ARC(0x42), 3, 1, 0);
            w->delta = 1.5707964f;
        }
        w->delta = ang - w->delta;
        em->r_no_2++;
    case 1:
        d = w->delta * 0.1f;
        em->ang.y += d;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        w->delta -= d;
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            if (em22GotoCk(em) == 0) {
                if (em->l_pl < 6250000.0f) {
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                    return;
                }
                if (em22PlRunCk(em)) {
                    if (w->plDist > 5000.0f) {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                } else {
                    if (w->plDist > 7000.0f) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                    }
                }
            }
        }
        break;
    }
    em22SlaverSet(em, 0);
}

// R1 == 0xA Escape: runs away from the player (RouteCkEscEm) for 30..60 frames, ringing the bell alarm
// (Status_flg[1] bit29), then RunAbout (7) or Turn (9).
static void em22_R1_Escape(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;

    if (em->l_pl < 25000000.0f) {
        w->flags |= 4;
    }
    if (em->r_no_2 == 0) {
        RouteCkEscEm(em, pPL, &w->routePos);
        lim = fabsf(Muku(&em->pos, &w->routePos, em->ang.y, PI));
        if (lim > 2.3561945f) {
            em->r_no_2 = 2;
        }
        if (!StaFlagChk(pG, STA_SE_BURST)) {
            StaFlagOn(pG, STA_SE_BURST);
            pG->SeInfo.pos = em->pos;
            pG->SeInfo.type = 0;
        }
    }
    w->escTimer = 2;
    switch (em->r_no_2) {
    case 0:
        em22SetRunMotion(em);
        w->timer = (u8) (Rnd() % 30) + 30;
        em->r_no_2++;
    case 1:
        em22RunTurn(em, w);
        MotionMove(em, 0);
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x40), ARC(0x43), 3, 1, 0);
        em->r_no_2++;
    case 3:
        w->timer = (u8) (Rnd() % 30) + 30;
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (w->timer) {
        w->timer--;
    }
    if (em->l_pl > 49000000.0f && w->timer == 0) {
        EmRoutineSet(em, 1, 7, 0, 1);
        return;
    }
    if (em22JumpCk(em)) {
        return;
    }
    if (w->targetAngAbs > 2.0943952f) {
        EmRoutineSet(em, 1, 9, 0, 0);
    } else {
        em22DoorOpenCk(em);
        em22SlaverSet(em, 1);
    }
}

// R1 == 0xB Threat: growls facing the player (30..45 frames): Run (8) when he runs at it or looks
// away, ParaAtk (0xF) with the parasites out when he is in the lane, grows a parasite (0x12), or
// side-steps (0xC) / RunAbout (7).
static void em22_R1_Threat(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Vec a;
    Vec b;
    Vec c;

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x27), 10, 5, 0);
        w->timer = (u8) (Rnd() % 15) + 30;
        w->lockCnt = 0;
        em->r_no_2++;
    case 1:
        em22DirMatrix(em, 0.0f);
        MotionMove(em, 0);
        if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI)) < 1.0471976f) {
            if (w->timer) {
                w->timer--;
            } else {
                if (w->plDeadWait == 0) {
                    EmRoutineSet(em, 1, 8, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 7, 0, 0);
                }
                break;
            }
        }
        if (w->routeAngAbs > 1.5707964f) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (em22PlRunCk(em)) {
            if (w->plDist > 5000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->l_pl < 16000000.0f && w->routeAngAbs < 0.5235988f && w->plDeadWait == 0) {
            a = em->pos;
            b = pPL->pos;
            a.y += 500.0f;
            b.y += 500.0f;
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            }
        } else if (em22PlRunCk2(em) && em->l_pl < 49000000.0f && w->plDeadWait == 0) {
            a = em->pos;
            c = pPL->pos;
            a.y += 500.0f;
            c.y += 500.0f;
            if (SatMgr.hitCheck(&a, &c, 0, 0, 0, 0) == 0) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            }
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em22SetParasiteCk(em)) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
        return;
    }
    if (em22LockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 15) {
            EmRoutineSet(em, 1, 0xC, 0, 0);
            return;
        }
    } else {
        w->lockCnt = 0;
    }
    em22SlaverSet(em, 0);
}

// R1 == 0xC SideStep: dodges to the free side (wall probes), then Run (8) when the player faces it,
// else RunAbout (7).
static void em22_R1_SideStep(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    em->dmg.m_Timer = 2;
    w->flags |= 4;
    switch (em->r_no_2) {
    case 0: {
        Vec a;
        Vec b;
        int side = Rnd() & 1;

        if (side) {
            a.x = 0.0f;
            a.y = 500.0f;
            a.z = 0.0f;
            b.x = 1000.0f;
            b.y = 500.0f;
            b.z = 0.0f;
            PSMTXMultVec(em->mat, &a, &a);
            PSMTXMultVec(em->mat, &b, &b);
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                side = 0;
            }
        } else {
            a.x = 0.0f;
            a.y = 500.0f;
            a.z = 0.0f;
            b.x = -1000.0f;
            b.y = 500.0f;
            b.z = 0.0f;
            PSMTXMultVec(em->mat, &a, &a);
            PSMTXMultVec(em->mat, &b, &b);
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                side = 1;
            }
        }
        if (side) {
            MotionSetCore(em, MOTION(em), ARC(0xF), ARC(0x2D), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xF), ARC(0x2D), 3, 1, 0);
        }
        em->r_no_2++;
    }
    case 1:
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            if (em22GotoCk(em)) {
                return;
            }
            if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI)) < 1.0471976f && w->plDeadWait == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 7, 0, 1);
            }
        }
        break;
    }
    em22SlaverSet(em, 1);
}

#define VIB_TBL ((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore))


// Branch check of JumpAtk (0xD): on the motion's hit key (Motion.Seq_old.Free bit0) with the player alive, seen,
// inside the 1000 x 1200 box in front and reachable (scenario probes at 500 / 1500 height and the two
// side lanes) -> JumpAtkHit (0xE), both damage-held, controller vibration.
static void em22_R1_br_JumpAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx inv;
    Vec v;
    Vec a;
    Vec b;
    Mtx m;

    if (!(em->Motion.Seq_old.Free & 1)) {
        return;
    }
    if (EmDeadCk(pPL)) {
        return;
    }
    if (!(w->flags & 1)) {
        return;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 500.0f || v.x < -500.0f || v.z > 1200.0f || v.z < 0.0f || v.y > 500.0f || v.y < -500.0f) {
        return;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    EmRoutineSet(em, 1, 0xE, 0, 0);
    pPL->dmg.m_Timer = 30;
    em->dmg.m_Timer = 30;
    VibSetData(VIB_TBL, 7, 1);
}

// R1 == 0xD JumpAtk: the leap at the player (motion with flag bit3 = jumping), turning slightly onto
// the route point; a miss scores an escape and goes to Escape (0xA).
static void em22_R1_JumpAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x16), ARC(0x33), 3, 1, 0);
        SndCall(8, 8, &em->pos, em->id, 0, em);
        w->timer = 10;
        em->r_no_2++;
    case 1:
        w->flags |= 8;
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &w->routePos, em->ang.y, 0.049087387f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            if (em22GotoCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
        break;
    }
    em22DoorOpenCk(em);
    em22SlaverSet(em, 1);
}

// R1 == 0xE JumpAtkHit: the dog on top of the player biting (plem22_JumpAtkHit on the player side,
// cut-in camera em22CamMove): drains 10 hp per frame while he mashes the button (PlGacha), a failed
// mash bites for 200 more and kills him at 1 hp; thrown off into Jump (0x11) / recovery.
static void em22_R1_JumpAtkHit(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -41.59f, 0.0f, 638.16f, plem22_JumpAtkHit);
        PlSetDamageSe(0);
        w->timer = 10;
        w->tilt = 0.0f;
        SndCall(8, 0x15, &em->pos, em->id, 0, em);
        PlGachaInit();
        w->camType = em22GetCamType(em);
        em->r_no_2++;
    case 1:
        em22CamMove(em, w->camType);
        if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 3, 1, 0);
        w->sndId[0] = SndCall(8, 0x12, &em->pos, em->id, 0, em);
        w->sndId[2] = SndCall(8, 0x21, &pPL->pos, em->id, 0, em);
        EstSet(em, -1, 0, 0, EFF_EM22, 0xD, 0, ESP_CORE_KIND_NONE, em, 0);
        w->timer = 0;
        em->r_no_2++;
    case 3:
        em22CamMove(em, w->camType);
        PlGachaMove();
        LifeDownSet2(pPL, 10, 0, 1);
        if (MotionMove(em, 0)) {
            if ((u32) PlGachaGet() < 15) {
                LifeDownSet2(pPL, 200, 0, 0);
            }
            if ((s16) pG->pl_life <= 1) {
                pG->pl_life = 0;
                em->r_no_2 = 6;
            } else {
                em->r_no_2 = 4;
            }
        } else if (w->timer) {
            w->timer--;
        } else {
            w->timer = 5;
            EstSet(em, -1, 0, 0, EFF_EM22, 7, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x34), 3, 1, 0);
        w->timer = 45;
        em->r_no_2++;
    case 5:
        if (w->timer) {
            w->timer--;
            em22CamMove(em, w->camType);
        }
        if (em->Motion.Seq_old.Free & 1) {
            SndStop(w->sndId[0], 0);
            SndStop(w->sndId[1], 0);
            SndCall(8, 0x11, &em->pos, em->id, 0, em);
            SndStop(w->sndId[2], 0);
            SndCall(8, 0x22, &pPL->pos, em->id, 0, em);
        }
        if ((em->Motion.Seq_old.Free & 2) && ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, 0, EFF_EM22, 4, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (MotionMove(em, 0)) {
            em->atari.setPriority(0);
            EmRoutineSet(em, 1, 0x11, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else {
            w->timer = 5;
            EstSet(em, -1, 0, 0, EFF_EM22, 0xA, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x1A), 0, 3, 1, 0);
        SndStop(w->sndId[2], 0);
        SndCall(1, 0xD, &pPL->pos, 0, 0, pPL);
        EstSet(em, -1, 0, 0, EFF_EM22, 0xB, 0, ESP_CORE_KIND_NONE, em, 0);
        DiedemoExec(30, 0);
        em->r_no_2++;
    case 7:
        em22CamMove(em, w->camType);
        MotionMove(em, 0);
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage routine of JumpAtkHit: knocked down with the dog on him (weapon hidden), the
// struggle, the kick-off, gets up (or dies); ends with EndPlDamage.
static void plem22_JumpAtkHit(cPlayer* pl)
{
    f32 y;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pPL->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x35), 0, 0, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->Wep->setTrans(0, 0);
        VibSetData(VIB_TBL, 0xF, 1);
        pl->r_no_2++;
    case 1:
        if (pl->Motion.Seq_frame > 6.7f && pl->Motion.Seq_frame < 7.3f) {
            SndCall(5, 0xC, &pl->pos, 0, 0, pl);
        }
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        pl->r_no_2 = pPL->pEmCatch->r_no_2;
        break;
    case 2: {
        Vec v;

        v.x = -35.86f;
        v.y = 0.0f;
        v.z = 235.52f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x36), 0, 3, 1, 0);
        pl->r_no_2++;
    }
    case 3:
        MotionMove(pl, 0);
        pl->r_no_2 = pPL->pEmCatch->r_no_2;
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x37), 0, 3, 1, 0);
        pl->m_Work0 = 45;
        VibSetClearType(1);
        pl->r_no_2++;
    case 5:
        if (pl->m_Work0) {
            pl->m_Work0--;
            if (pl->m_Work0 == 0) {
                pl->Wep->setTrans(1, 0);
            }
        }
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 6:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x38), 0, 3, 1, 0);
        pl->r_no_2++;
    case 7:
        MotionMove(pl, 0);
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Branch check of ParaAtk (0xF): on the hit key with the player alive inside the 1000 x 4000 lane in
// front -> ParaAtkHit (0x10), damage held.
static void em22_R1_br_ParaAtk(cEm22* em)
{
    Mtx inv;
    Vec v;

    if (!(em->Motion.Seq_old.Free & 1)) {
        return;
    }
    if (EmDeadCk(pPL)) {
        return;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 500.0f || v.x < -500.0f || v.z > 4000.0f || v.z < 0.0f || v.y > 500.0f || v.y < -500.0f) {
        return;
    }
    EmRoutineSet(em, 1, 0x10, 0, 0);
    pPL->dmg.m_Timer = 30;
    em->dmg.m_Timer = 30;
    VibSetData(VIB_TBL, 7, 1);
}

// R1 == 0xF ParaAtk: the back parasites lash out at the player (em22SetParasiteAtk tentacles, work
// flag 0x200) while the dog turns to him; a miss scores an escape -> Escape (0xA).
static void em22_R1_ParaAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 0x204;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), ARC(0x52), 3, 1, 0);
        SndCall(8, 0x2E, &em->pos, em->id, 0, em);
        SndCall(8, 0x2C, &em->pos, em->id, 0, em);
        w->timer2 = 30;
        w->timer = 15;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (w->timer2) {
            w->timer2--;
            if (w->timer2 == 0) {
                SndCall(8, 0x2D, &em->pos, em->id, 0, em);
            }
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            EmRoutineSet(em, 1, 0xA, 0, 0);
        } else {
            if (em->Motion.Seq_old.Free & 0x80) {
                em22SetParasiteAtk(em);
                em22SetParasite(em);
            }
            if (em->Motion.Seq_old.Free & 0x40) {
                em22ParaSetMotAtk(em);
            }
        }
        break;
    }
}

// R1 == 0x10 ParaAtkHit: the tentacles connected: the player takes plem22_ParaAtkHit while the dog
// holds its pose, then RunAbout (7).
static void em22_R1_ParaAtkHit(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    em->dmg.m_Timer = 2;
    w->flags |= 0x204;
    switch (em->r_no_2) {
    case 0:
        em22ParaAtkHitPosSet(em);
        MotionSetCore(em, MOTION(em), ARC(0x4F), 0, 3, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -41.59f, 0.0f, 638.16f, plem22_JumpAtkHit);
        SetPlDamage(em, plem22_ParaAtkHit);
        em22ParaSetMotAtkHit(em);
        SndCall(8, 0x2F, &em->pos, em->id, 0, em);
        PlSetDamageSe(0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.098174773f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->atari.setPriority(0);
            w->plDeadWait = 30;
            em22ParaSetMotWait(em);
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
}

// Player damage routine of the parasite lash: turned to face the dog, the hit motion with 10 damage
// per frame; dies with the standard death when hp runs out.
static void plem22_ParaAtkHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pPL->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, PI);
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x4D), 0, 3, 1, 0);
        PlSetFace(1);
        EstSet(pl, -1, 0, 0, EFF_EM22, 0xC, 0, ESP_CORE_KIND_NONE, pl, 0);
        VibSetData(VIB_TBL, 0xE, 1);
        pl->m_Work0 = 45;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
            LifeDownSet2(pPL, 10, 0, 0);
            if (pl->m_Work0 == 0 && (s16) pG->pl_life <= 0) {
                PlSetDamageSe(0xD);
                PlSetDamage(PL_DM_FRONT, 0, 0);
                break;
            }
        }
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// R1 == 0x11 Wakeup: gets up after being blown away / thrown off (flag bit1), then RunAbout (7).
static void em22_R1_Wakeup(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    int flip;

    switch (em->r_no_2) {
    case 0:
        flip = 1;
        if (em->Motion.Mot_attr & 0x40) {
            flip = 0x41;
        }
        if (w->targetAngAbs < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x14), ARC(0x31), 3, flip, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x15), ARC(0x32), 3, flip, 0);
        }
        w->timer = 15;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            w->flags |= 2;
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x12 Parasite: the parasites burst out of the back (em22OpenBack, em22SetParasite, flag bit5),
// the growl, then RunAbout (7).
static void em22_R1_Parasite(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), ARC(0x2C), 3, 1, 0);
        if (em->be_flag & 0x800) {
            EstSet(em, -1, 0, 0, EFF_EM22, 6, 1, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM22, 6, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        em->r_no_2++;
    case 1:
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            em22SetParasite(em);
        }
        break;
    }
}

// R1 == 0x13 Jump: jumps over the low wall found by em22JumpCk (delta = height to climb, flags
// 0x88 airborne), landing into Run (8).
static void em22_R1_Jump(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx m;
    Vec v;
    f32 fl;

    w->flags |= 4;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x3C), ARC(0x3D), 3, 1, 0);
        PSMTXRotRad(m, 'y', em->ang.y);
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 4356.0f;
        PSMTXMultVec(m, &v, &v);
        fl = SatMgr.getFloor(&v, 0, 1000.0f, 100000.0f, 0);
        if (fl == -100000.0f) {
            fl = em->pos.y;
        }
        w->delta = fl - em->pos.y;
        em->r_no_2++;
    case 1:
        fl = w->delta * 0.1f;
        em->pos.y += fl;
        w->delta -= fl;
        w->flags |= 0x88;
        em22DirMatrix(em, 0.0f);
        if (MotionMove(em, 0)) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    em22SlaverSet(em, 1);
}

// R0 == 2: damage, runs Em22_R2_move_tbl (Dm_Small, Dm_Blow).
static void em22_R0_Damage(cEm22* em)
{
    Em22_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Small: the flinch (one of four yelp motions), then SideStep (0xC) / Escape /
// Run / RunAbout; one time in four a second flinch turns into a side step.
static void em22_R1_Dm_Small(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    void* mot;
    int flip;

    switch (em->r_no_2) {
    case 0:
        switch (Rnd() & 3) {
        case 0:
        default:
            mot = ARC(0x10);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x10);
            flip = 0x41;
            break;
        case 2:
            mot = ARC(0x45);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x45);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, 0, 3, flip, 0);
        SndCall(8, 0xC, &em->pos, em->id, 0, em);
        w->lockCnt = 0;
        w->timer = 8;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em22LockCk(em)) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (Rnd() & 1) {
                EmRoutineSet(em, 1, 0xA, 0, 0);
            } else if (w->plDeadWait == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        } else if (w->timer) {
            w->timer--;
            if (w->timer == 0 && (Rnd() & 3) == 0) {
                em->dmg.m_Timer = 5;
                EmRoutineSet(em, 1, 0xC, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 1 Dm_Blow: blown off its feet by the hit direction (r_no_3 picks the four fall
// motions), flies with blowSpd (flags 0x88), takes landing damage (timer2 * 50), then Die_Lost when
// dead or Wakeup (0x11).
static void em22_R1_Dm_Blow(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 ang;
    f32 angAbs;
    f32 fl;
    void* mot;
    void* blend;
    int flip;
    cModelInfo* info;

    w->flags |= 2;
    switch (em->r_no_2) {
    case 0:
        ang = Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI);
        angAbs = fabsf(ang);
        em->r_no_3 = ang < 0.0f ? 3 : 2;
        if (angAbs < 0.7853982f) {
            em->r_no_3 = 0;
        }
        if (angAbs > 2.3561945f) {
            em->r_no_3 = 1;
        }
        switch (em->r_no_3) {
        case 0:
        default:
            mot = ARC(0x12);
            blend = ARC(0x2F);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x11);
            blend = ARC(0x2E);
            flip = 1;
            break;
        case 2:
            mot = ARC(0x13);
            blend = ARC(0x30);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x13);
            blend = ARC(0x30);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, blend, 3, flip, 0);
        switch (em->r_no_3) {
        case 0:
        default:
            w->blowSpd.x = 0.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = -200.0f;
            break;
        case 1:
            w->blowSpd.x = 0.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = 200.0f;
            break;
        case 2:
            w->blowSpd.x = -200.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = 0.0f;
            break;
        case 3:
            w->blowSpd.x = 200.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = 0.0f;
            break;
        }
        PSMTXMultVecSR(em->mat, &w->blowSpd, &w->blowSpd);
        if (em->hp <= 0) {
            SndStop(w->Seid_foot, 0);
            SndCall(8, 0x1E, &em->pos, em->id, 0, em);
        } else {
            SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
        w->timer = 1;
        w->camType = 0;
        w->timer2 = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                em->atari.m_flag &= ~0x300;
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            }
        } else {
            if (em->Motion.Seq_old.Free & 2) {
                if (ChkWaterEffectEnable(&em->pos)) {
                    EstSet(0, -1, &em->pos, 0, EFF_EM22, 4, 0, ESP_CORE_KIND_NONE, 0, 0);
                }
                fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
                if (em->pos.y < fl) {
                    em->pos.y = fl;
                    w->timer = 0;
                } else {
                    em->r_no_2 = 2;
                }
            }
            if (w->timer) {
                w->flags |= 0x88;
            } else {
                w->flags &= ~0x88;
            }
        }
        break;
    case 2:
        switch (em->r_no_3) {
        case 0:
        default:
            mot = ARC(0x53);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x55);
            flip = 1;
            break;
        case 2:
            mot = ARC(0x57);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x57);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, 0, 3, flip, 0);
        em->r_no_2++;
    case 3:
        w->flags |= 0x88;
        w->timer2++;
        PSVECAdd(&em->pos, &w->blowSpd, &em->pos);
        w->blowSpd.y -= 20.0f;
        MotionMove(em, 0);
        fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            w->blowSpd.x = 0.0f;
            w->blowSpd.y = 0.0f;
            w->blowSpd.z = 0.0f;
            w->timer = 0;
            LifeDownSet(em, w->timer2 * 50, 0);
            em->r_no_2++;
        }
        break;
    case 4:
        switch (em->r_no_3) {
        case 0:
        default:
            mot = ARC(0x54);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x56);
            flip = 1;
            break;
        case 2:
            mot = ARC(0x58);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x58);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, 0, 3, flip, 0);
        SndCall(8, 0x10, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                em->atari.m_flag &= ~0x300;
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            }
        }
        break;
    }
    if ((w->flags & 0x400) && em->pModelInfo) {
        for (info = em->pModelInfo; info; info = info->pList) {
            if (info->color[0] > 0x20) {
                info->color[0] -= 0x20;
            }
            info->color[1] = info->color[0];
            info->color[2] = info->color[0];
        }
    }
}

// R0 == 3: death, runs Em22_R3_move_tbl (Die_Lost).
static void em22_R0_Die(cEm22* em)
{
    Em22_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Lost: the corpse: item drop (ITEMSET), the back parasites released, then the
// body dissolves (em22ScaleCompress + invisible_factor) and the enemy is left invisible (be_flag 0x4000).
static void em22_R1_Die_Lost(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u32 i;

    switch (em->r_no_2) {
    case 0:
        em->atari.m_flag &= ~0x300;
        em->atari.m_flag |= 0x10;
        EmSetDie(em);
        EmReserveDropItem(em);
        EmSetDieCnt(em);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        EffectEspDelete(0, w->espKind, em, 0);
        EffectEspgenDelete(0, w->espKind, em);
        EffectEfmDelete(0, w->espKind, em);
        EstSet(em, -1, 0, 0, EFF_EM22, 5, 0, ESP_CORE_KIND_NONE, em, 0);
        for (i = 0; i < 5; i++) {
            if (w->pPara[i]) {
                w->pPara[i]->clearLostWait();
                w->pPara[i] = 0;
            }
        }
        w->timer = 30;
        w->timer2 = 150;
        w->scale = 1.0f;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                SndCall(8, 0x26, &em->pos, em->id, 0, em);
            }
        } else {
            w->scale -= 0.01f;
            if (w->scale < 0.1f) {
                w->scale = 0.1f;
            }
            em->pos.y -= 3.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->timer2) {
            w->timer2--;
        } else {
            em->invisible_factor -= 0.1f;
            if (em->invisible_factor <= 0.0f) {
                em->invisible_factor = 0.0f;
                em->be_flag &= ~2;
                em->be_flag |= 0x4000;
                em->r_no_2++;
            }
        }
        break;
    }
}

// Per frame: the route point / angle to the player (plRoutePos, routeAng; flag bit0 = line of sight
// clear), plDist, and the current target route point (routePos / targetAng / targetDist2: the goto
// position, the escape point while escTimer runs, or the player).
void Em22RouteCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Vec a;
    Vec b;
    Vec dbg;
    f32 dx;
    f32 dz;

    if (em->hp <= 0) {
        return;
    }
    if ((pG->Frame_cnt & 3) != (em->emset_no & 3)) {
        return;
    }
    w->flags &= ~1;
    RouteCkToPos(em, &pPL->pos, &w->plRoutePos, 0, 0);
    w->routeAng = Muku(&em->pos, &w->plRoutePos, em->ang.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    a.x = em->pos.x;
    a.y = em->pos.y + 500.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 500.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        w->flags |= 1;
    }
    w->routePos = w->plRoutePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist2 = em->l_pl;
    w->pTarget = pPL;
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    if (w->gotoOn) {
        RouteCkToPos(em, &w->gotoPos, &w->routePos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        dz = em->pos.z - w->gotoPos.z;
        dx = em->pos.x - w->gotoPos.x;
        w->targetDist2 = dx * dx + dz * dz;
        if (DbgFlagChk(pG, DBG_RTP_DISP)) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->routePos, 0xFFFFFF40, 0);
            Draw_line3d(&dbg, &w->gotoPos, 0xFF0000FF, 0);
        }
    } else if (w->escTimer) {
        RouteCkEscEm(em, pPL, &w->routePos);
        w->targetAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        if (DbgFlagChk(pG, DBG_RTP_DISP)) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->routePos, 0xFFFFFF40, 0);
        }
    } else {
        if (DbgFlagChk(pG, DBG_RTP_DISP)) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->routePos, 0xFFFFFF40, 0);
        }
    }
}

// Builds the model matrix with the run tilt: rolls the body into the turn direction `dir` (smoothed
// in tilt), used by the running routines.
void em22DirMatrix(cEm22* em, f32 dir)
{
    Em22Work* w = EM22_WK(em);
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
    // The magnitude reuses `t` (a multi-set global pseudo): the fabs result is not tied to the
    // dying blend result and lands in t's f11.
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

// Neck tracking (work flag bit2): turns the head parts 3..5 towards the player's head within 60 deg
// (neckX / neckY eased), else eases back to the motion.
void em22NeckMove(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx m;
    Vec d;
    cModel* plP;
    cParts* p;
    f32 f;
    f32 len;

    plP = pPL->getPartsPtr(4);
    p = (cParts*) em->getPartsPtr(4);
    if (w->flags & 4) {
        f = Muku(&em->pos, &pPL->pos, em->ang.y, 1.0471976f);
        w->neckY += Muku2(w->neckY, f, 0.098174773f);
        PSVECSubtract(&plP->world, &p->world, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        f = -atan2f(d.y, len);
        if (f > 1.2217305f) {
            f = 1.2217305f;
        }
        if (f < -1.2217305f) {
            f = -1.2217305f;
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
    p->inv_offset.x = 0.0f;
    p->inv_offset.y = f;
    p->motParts.flags |= 0x40000000;
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

// Hit boxes: the body box and hit[0..4] on the head / legs (YarareInit / YarareAdd).
void em22YarareInit(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    YarareInit(em, 0.0f, -100.0f, -150.0f, 250.0f, 650.0f, 2, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 150.0f, 100.0f, 6, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[1], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xB, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xF, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[3], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[4], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x18, YAT_FLAG_ON);
}

// Blood effect of the hit by weapon kind (EmDmBloodSet2): small for handguns / knife, big for
// explosives, magnum and rifles and a near shotgun hit, none for the hand weapons.
void em22BloodSet(cEm22* em)
{
    int near;

    near = 0;
    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    switch (em->dmg.m_Wep) {
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x1A, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1A, 0, 0, 0, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x1A, 0, 0, 0, 0);
        break;
    case 0x27:
        EmDmBloodSet2(em, 0x1A, 0, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x2A:
    default:
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x1A, 1, 0, 0, 0);
        break;
    }
}

// Damage of the weapon hit (GetWepDmVal, `near` for a muzzle within 6000 units), 100 for unknown weapons.
int em22SetDmVal(cEm22* em)
{
    int near;
    int dm;

    near = 0;
    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    dm = 100;
    if (em->dmg.m_Wep <= 0x2D) {
        dm = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    return dm;
}

// Picks the catch-scene camera side: 1 when the default viewpoint behind the player is blocked by a wall.
int em22GetCamType(cEm22* em)
{
    Vec a;
    Vec b;

    a.x = -15.0f;
    a.y = 225.0f;
    a.z = -1604.0f;
    b.x = 115.0f;
    b.y = 349.0f;
    b.z = 136.0f;
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    if (EatMgr.hitCheck(&b, &a, 0, 0, 0, 0)) {
        return 1;
    }
    return 0;
}

// Cut-in camera of the bite scene: eases the work Camera (cam) to the viewpoint `type` (0 / 1) beside
// the player, pulled in front of walls, and installs it as the extra camera.
void em22CamMove(cEm22* em, int type)
{
    Em22Work* w = EM22_WK(em);
    Camera* gcam = &pG->Camera;
    Vec a;
    Vec b;
    Vec hit;
    Vec d;

    switch (type) {
    case 0:
    default:
        a.x = -373.0f;
        a.y = 101.0f;
        a.z = -1260.0f;
        b.x = 196.0f;
        b.y = 358.0f;
        b.z = -45.0f;
        break;
    case 1:
        a.x = 1221.0f;
        a.y = 1748.0f;
        a.z = 1085.0f;
        b.x = -137.0f;
        b.y = 211.0f;
        b.z = -208.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&gcam->param.pos, &a, &w->cam.param.pos, 0.2f);
    PosToPos(&gcam->param.at, &b, &w->cam.param.at, 0.2f);
    if ((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
            (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
            (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z) >
        100.0f) {
        PSVECSubtract(&w->cam.param.pos, &w->cam.param.at, &d);
#line 3428 "D:/Bio4/Prog/em22.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 250.0f);
        PSVECAdd(&w->cam.param.pos, &d, &w->cam.param.pos);
        if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
            w->cam.param.pos = hit;
        }
        PSVECSubtract(&w->cam.param.pos, &d, &w->cam.param.pos);
    }
    w->cam.Up.x = 0.0f;
    w->cam.Up.y = 1.0f;
    w->cam.Up.z = 0.0f;
    w->cam.Distance = VEC_DIST(&w->cam.param.pos, &w->cam.param.at);
    w->cam.param.fovy = 50.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.m_pExtraCamera = (s32) &w->cam;
}

// 1 when the parasites may grow now: not yet out (flag bit4) and paraWait counted down.
int em22SetParasiteCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    if (w->flags & 0x10) {
        return 0;
    }
    if (w->paraWait) {
        w->paraWait--;
        return 0;
    }
    return 1;
}

// Grows the five back parasites: cObj16 objects (ARC 0x39/0x3A) on parts 0x23..0x27 with staggered
// wriggle motions (0x3B) and 1.3 / 1.5 scale; flag bit4 = parasites out.
void em22SetParasite(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u16 step;
    u32 i;
    Vec pos;
    Vec rot;
    Vec sc;

    if (w->flags & 0x10) {
        return;
    }
    step = (*(u16*) ARC(0x3B) & 0x3FFF) / 5;
    for (i = 0; i < 5; i++) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = fRand1_1() * PI;
        rot.z = 0.0f;
        w->pPara[i] = (cObj16*) SetObj16(ARC(0x39), ARC(0x3A), em, em, i + 0x23, 5, &pos, &rot);
        if (w->pPara[i]) {
            MotSetObj16(w->pPara[i], ARC(0x3B), 4, step * i);
            if (i == 0 || i == 4) {
                sc.x = 1.3f;
                sc.y = 1.3f;
                sc.z = 1.3f;
            } else {
                sc.x = 1.5f;
                sc.y = 1.5f;
                sc.z = 1.5f;
            }
            w->pPara[i]->setScale(&sc);
        }
    }
    w->flags |= 0x10;
}

// Creates the three attack tentacles (pParaAtk[]) for ParaAtk, scaled like the back ones.
void em22SetParasiteAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u16 step;
    u32 i;
    Vec pos;
    Vec rot;
    Vec sc;

    step = (*(u16*) ARC(0x49) & 0x3FFF) / 3;
    for (i = 0; i < 3; i++) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pParaAtk[i] = (cObj16*) SetObj16(ARC(0x47), ARC(0x48), em, em, i + 0x23, 6, &pos, &rot);
        if (w->pParaAtk[i]) {
            MotSetObj16(w->pParaAtk[i], ARC(0x49), 4, step * i);
            sc.x = 1.0f;
            sc.y = 1.0f;
            sc.z = 1.0f;
            w->pParaAtk[i]->setScale(&sc);
            w->pParaAtk[i]->Motion.flip = em22_para_flip;
        }
    }
}

// Puts the back parasites into their idle wriggle motion.
void em22ParaSetMotWait(cEm22* em)
{
    u32 i;
    cObj16** pp = EM22_WK(em)->pParaAtk;

    for (i = 0; i < 3; i++) {
        if (pp[i]) {
            if (i != 1) {
                MotSetObj16(pp[i], ARC(0x49), 5, 0);
            } else {
                // Allocation lever (loop notes, no code): the 7th weighted em ref ranks em
                // above i in global-alloc (em r30, i r29).
                do { MotSetObj16(pp[1], ARC(0x49), 0x45, 0); } while (0);
            }
        }
    }
}

// Puts the attack tentacles into the lash motion.
void em22ParaSetMotAtk(cEm22* em)
{
    u32 i;
    cObj16** pp = EM22_WK(em)->pParaAtk;

    for (i = 0; i < 3; i++) {
        if (pp[i]) {
            if (i != 1) {
                MotSetObj16(pp[i], ARC(0x4C), 1, 0);
            } else {
                // Allocation lever (loop notes, no code): the 7th weighted em ref ranks em
                // above i in global-alloc (em r30, i r29).
                do { MotSetObj16(pp[1], ARC(0x4C), 0x41, 0); } while (0);
            }
        }
    }
}

// Puts the attack tentacles into the hit-hold motions (one per tentacle) for ParaAtkHit.
void em22ParaSetMotAtkHit(cEm22* em)
{
    u32 i;
    cObj16** pp = EM22_WK(em)->pParaAtk;

    for (i = 0; i < 3; i++) {
        if (pp[i]) {
            // default first: the arms are laid out default, 1, 2 (the nested if/else form put
            // the i == 2 body before the i == 1 body).
            switch (i) {
            default:
                MotSetObj16(pp[i], ARC(0x4A), 1, 0);
                break;
            case 1:
                MotSetObj16(pp[1], ARC(0x4B), 0x41, 0);
                break;
            case 2:
                MotSetObj16(pp[2], ARC(0x4B), 1, 5);
                break;
            }
        }
    }
}

// Removes the attack tentacles (clearLostWait) after the attack.
void em22AtkParaClearCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u32 i;

    if (w->flags & 0x200) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (w->pParaAtk[i]) {
            w->pParaAtk[i]->clearLostWait();
            w->pParaAtk[i] = 0;
        }
    }
}

// Opens the back plates (parts 0x1F..0x22 rotated) for the parasites to come out.
void em22OpenBack(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    cModel* p;

    if (w->flags & 0x10) {
        p = em->getPartsPtr(0x1F);
        p->ang.z = p->ang.z * 0.9f + 0.08726647f;
        p = em->getPartsPtr(0x20);
        p->ang.z = p->ang.z * 0.9f + -0.08726647f;
        p = em->getPartsPtr(0x21);
        p->ang.z = p->ang.z * 0.9f + 0.08726647f;
        p = em->getPartsPtr(0x22);
        p->ang.z = p->ang.z * 0.9f + -0.08726647f;
    }
}

// Is the player aiming a gun (not the knife, with ammo, aim routine 0/6) at this dog within 8000 units:
// the weapon's target is this enemy or the root lies inside a 1000-unit box in front of the weapon hand.
int em22LockCk(cEm22* em)
{
    Mtx inv;
    Vec v;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 6) {
        return 0;
    }
    if (em->l_pl > 64000000.0f) {
        return 0;
    }
    if (pG->weapon_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 1.0471976f) {
        return 0;
    }
    if (pPL->Wep->m_pWep->m_SightEm && pPL->Wep->m_pWep->m_SightEm == em) {
        return 1;
    }
    PSMTXInverse(pPL->getPartsPtr(10)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->world, &v);
    if (v.x > 0.0f) {
        return 0;
    }
    if (v.z > 500.0f) {
        return 0;
    }
    if (v.z < -500.0f) {
        return 0;
    }
    if (v.y > 500.0f) {
        return 0;
    }
    if (v.y < -500.0f) {
        return 0;
    }
    return 1;
}

// During Die_Lost squashes every part vertically by `scale` so the dissolving corpse sinks into the floor.
void em22ScaleCompress(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx m;
    Vec s;
    cParts* p;

    if (em->r_no_0 != 3) {
        return;
    }
    if (em->r_no_1 == 0) {
        PSMTXIdentity(m);
        s.x = 1.0f;
        s.y = w->scale;
        s.z = 1.0f;
        ScaleMatrix(m, &s);
        for (p = em->pList; p; p = p->pList) {
            PSMTXConcat(m, p->mat, p->mat);
            p->mat[0][3] = p->world.x;
            p->mat[1][3] = p->world.y;
            p->mat[2][3] = p->world.z;
        }
    }
}

// Footstep / paw SEs through the room's ctrl11 control by the motion's step events and the floor material.
void em22FootSeControl(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u8 no;

    no = em->Motion.Seq_old.Se;
    if (no == 0) {
        return;
    }
    no--;
    switch (no) {
    case 0:
    case 1:
        Ctrl11SetSe(w->pCtrl11, em, 3, no, 0xC);
        em->Motion.Seq_old.Se = 0;
        break;
    case 2:
        Ctrl11SetSe(w->pCtrl11, em, 3, no, 0xD);
        em->Motion.Seq_old.Se = 0;
        break;
    case 4:
    case 5:
    case 8:
    case 9:
        // Allocation lever (loop notes, no code): at depth 2 the two `w` refs count 3x (w 9 refs /
        // 30 insns beats em 11 / 47), so w takes r31 and em r30.
        do { do { w->Seid_foot = Ctrl11SetSe(w->pCtrl11, em, 10, no, 0xE); } while (0); } while (0);
        em->Motion.Seq_old.Se = 0;
        break;
    }
}

// A low jumpable wall (scenario flag 0x80020) in front of the chest part -> Jump (0x13). 1 when set.
int em22JumpCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Vec nrm;
    Mtx m;
    Vec a;
    Vec b;
    cModel* p;

    if (w->stuckCnt <= 1) {
        return 0;
    }
    if (w->targetAngAbs > 0.5235988f) {
        return 0;
    }
    p = em->getPartsPtr(2);
    PSMTXCopy(em->mat, m);
    TransMatrix(m, &p->world);
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 600.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & 0x80020) {
        em->ang.y = atan2f(-nrm.x, -nrm.z);
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return 1;
    }
    return 0;
}

// 1 while the player is running (routine 0 / 3).
int em22PlRunCk(cEm22* em)
{
    if (pPL->r_no_0 != 0) {
        return 0;
    }
    return pPL->r_no_1 == 3;
}

// 1 when the player runs towards this dog (within 45 deg in front, inside the approach lane).
int em22PlRunCk2(cEm22* em)
{
    Mtx inv;
    Vec v;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 3) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    if (v.x > 1500.0f) {
        return 0;
    }
    if (v.x < -1500.0f) {
        return 0;
    }
    return 1;
}

// Splash at a foot (parts `no`) when the motion key says it touched the ground.
static inline void em22FootSplash(cEm22* em, int no)
{
    cModel* p = em->getPartsPtr(no);

    if (em->be_flag & 0x800) {
        EstSet(0, -1, &p->world, 0, EFF_EM22, 3, 1, ESP_CORE_KIND_NONE, em, 0);
    } else {
        EstSet(0, -1, &p->world, 0, EFF_EM22, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
    }
}

// Water splashes at the paws (parts 0xC / 0x10 / 0x15 / 0x19) on the motion's step events when the
// dog stands in water.
void em22FootEff(cEm22* em)
{
    if (!(em->Motion.Seq_old.Free & 0x3C)) {
        return;
    }
    if (ChkWaterEffectEnable(&em->pos) == 0) {
        return;
    }
    if (em->Motion.Seq_old.Free & 4) {
        em22FootSplash(em, 0xC);
    }
    if (em->Motion.Seq_old.Free & 8) {
        em22FootSplash(em, 0x10);
    }
    if (em->Motion.Seq_old.Free & 0x10) {
        em22FootSplash(em, 0x15);
    }
    if (em->Motion.Seq_old.Free & 0x20) {
        em22FootSplash(em, 0x19);
    }
}

// Places the dog for the tentacle hit: a fixed distance in front of the player on the floor.
void em22ParaAtkHitPosSet(cEm22* em)
{
    Vec d;
    Vec a;
    Vec b;

    if (em->l_pl > 6250000.0f) {
        return;
    }
    PSVECSubtract(&em->pos, &pPL->pos, &d);
    d.y = 0.0f;
#line 4013 "D:/Bio4/Prog/em22.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 2500.0f);
    a = pPL->pos;
    a.y = em->pos.y + 500.0f;
    b = a;
    PSVECAdd(&b, &d, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        b.y = SatMgr.getFloor(&b, 0, 600.0f, 100000.0f, 0);
        em->pos = b;
    }
}

// Drool effect from the mouth every 8 (idle) / 14 (running) frames, the water variant when in water.
void em22SlaverSet(cEm22* em, int run)
{
    Em22Work* w = EM22_WK(em);

    if (w->slaverTimer) {
        w->slaverTimer--;
        return;
    }
    switch (run) {
    case 0:
    default:
        w->slaverTimer = 8;
        if (em->be_flag & 0x800) {
            EstSet(em, -1, 0, 0, EFF_EM22, 8, 1, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM22, 8, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        break;
    case 1:
        w->slaverTimer = 14;
        if (em->be_flag & 0x800) {
            EstSet(em, -1, 0, 0, EFF_EM22, 9, 1, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM22, 9, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        break;
    }
}

// When a goto order is pending (gotoOn) switches to Goto (0) and returns 1.
int em22GotoCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    if (w->gotoOn == 0) {
        return 0;
    }
    EmRoutineSet(em, 1, 0, 0, 0);
    return 1;
}

// Room script: sends the dog to `pos` (snapped 50 above the floor), `on` = order pending.
void cEm22::setGoto(Vec* pos, int on)
{
    Em22Work* w = EM22_WK(this);
    f32 fl;

    w->gotoOn = on;
    w->gotoPos = *pos;
    fl = SatMgr.getFloor(&w->gotoPos, 0, 600.0f, 100000.0f, 0);
    if (fl != -100000.0f) {
        w->gotoPos.y = fl + 50.0f;
    }
}

// 1 when the dog's root or head is inside the screen (the jump attack needs it).
int em22ScreenInCk(cEm22* em)
{
    Vec s;
    Vec p;

    p = em->pos;
    if (GetScreenPos(&p, &s) && s.x > 0.0f && s.x < 512.0f && s.y > 0.0f && s.y < 448.0f) {
        return 1;
    }
    p = em->getPartsPtr(4)->world;
    if (GetScreenPos(&p, &s) && s.x > 0.0f && s.x < 512.0f && s.y > 0.0f && s.y < 448.0f) {
        return 1;
    }
    return 0;
}


// Opens a closed door (cEmDoor) the dog runs into from its side (within reach and angle).
void em22DoorOpenCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u32 i;
    Vec v;
    f32 ang;

    if (w->stuckCnt % 10 != 5) {
        return;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmDoor* door = (cEmDoor*) EmMgr.fastAt(i);
        EmDoorWork* dw;

        if ((door->be_flag & 0x201) != 1) {
            continue;
        }
        if (door->id != 0x41) {
            continue;
        }
        if (door->hp <= 0) {
            continue;
        }
        if ((em->pos.x - door->pos.x) * (em->pos.x - door->pos.x) + (em->pos.y - door->pos.y) * (em->pos.y - door->pos.y) +
                (em->pos.z - door->pos.z) * (em->pos.z - door->pos.z) >
            6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(door);
        ang = fabsf(Muku2(dw->base_dir, em->ang.y, PI));
        if (ang > 0.7853982f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->base_im, &em->pos, &v);
        if (ang < 1.5707964f) {
            if (v.z > 0.0f) {
                continue;
            }
            if (v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f) {
                continue;
            }
            if (v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->Width) {
            continue;
        }
        if (v.x < -dw->Width) {
            continue;
        }
        if (v.y > 500.0f) {
            continue;
        }
        if (v.y < -500.0f) {
            continue;
        }
        {
            u32 st = door->ckOpen();

            if (st) {
                if (st <= 3) {
                    continue;
                }
            }
        }
        door->setOpen(&em->pos, 0, 0, 0);
    }
}
