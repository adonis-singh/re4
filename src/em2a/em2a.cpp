// em2a module (D:/Bio4/Prog/em2a.cpp): the traps. Type 0 is the bear trap that bites the player
// (em2a_R1_Trap1Bite, with the player catch motion and a cut-in camera) or the partner
// (em2a_R1_Trap1BiteSub, freed by the action button), types 1 and 2 are the tripwire bombs that go
// off when the player or an enemy crosses the wire (em2aTrap2HitCk, em2aTrap2Bomb).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em2a.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "game.h"
#include "quake.h"
#include "pad.h"
#include "pl_wep.h"
#include "snd.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em10.h"
#include "em_mod.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em2a,52,4");



typedef void (*Em2aFunc)(cEm2a*);

static void em2a_R0_Init(cEm2a* em);
static void em2a_R0_Move(cEm2a* em);
static void em2a_R1_br_Dummy(cEm2a* em);
static void em2a_R1_br_Trap1Set(cEm2a* em);
static void em2a_R1_Trap1Set(cEm2a* em);
static void em2a_R1_Trap1Bite(cEm2a* em);
static void plem2a_Trap1Bite(cPlayer* pl);
static void em2a_R1_Trap1BiteSub(cEm2a* em);
static void subem2a_Trap1Bite(cSubChar* sub);
static void em2aResuceAshleyAction(cSubChar* sub);
static void plemResuceAshley(cPlayer* pl);
static void em2a_R1_Trap1Break(cEm2a* em);
static void em2a_R1_Trap1Reset(cEm2a* em);
static void em2a_R1_Trap1R100(cEm2a* em);
static void em2a_R1_Trap2Set(cEm2a* em);
static void em2a_R1_Trap2Bomb(cEm2a* em);





// Module entry (SN loader): registers Em2aInit as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em2a prolog Ok\n");
    EmInitFunc = Em2aInit;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm2a class in the manager's work.
void Em2aInit(cEm* em)
{
    new (em) cEm2a();
}

// Damage check of the bear trap (type 0): a weapon hit other than the hand / flash / mine / explosive
// kinds breaks it (hp 0, snap SE, spark effect 0x22/9): while it holds the dog (R1 5) it goes to
// Trap1Reset (4), else Trap1Break (3) with a critical-hit score.
void em2aDmCkTrap1(cEm2a* em)
{
    int wep;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A || wep == 0xE || wep == 0x13 || wep == 0x29
        || wep == 0x2D) {
        return;
    }
    em->hp = 0;
    SndCall(8, 2, &em->pos, em->id, 0, em);
    EmDmBloodSet2(em, 0x22, 9, 0, 0, 0);
    if (em->r_no_0 == 1 && em->r_no_1 == 5) {
        EmRoutineSet(em, 1, 4, 0, 0);
    } else {
        GameAddPoint(LVADD_CRITICALHIT);
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

// Damage check of the tripwire bombs (types 1 / 2): the wire crossed (em2aTrap2HitCk) or any weapon
// hit except the hand / flash / mine kinds sets it off (hp 0, EmSetDie, Trap2Bomb 7); a shot wire
// scores a critical.
void em2aDmCkTrap2(cEm2a* em)
{
    int hit = em2aTrap2HitCk(em);

    if (hit) {
        em->hp = 0;
        // both arms call EmSetDie: the arm's own `mr r3, em` copy is scheduled above the hp store,
        // so the original's cross-jump starts at the `bl` and each arm keeps the copy
        EmSetDie(em);
        EmRoutineSet(em, 1, 7, 0, 0);
    } else {
        int wep;

        if (em->dmg.m_Flag == 0) {
            return;
        }
        wep = em->dmg.m_Wep;
        em->dmg.m_Flag = 0;
        if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A || wep == 0xE) {
            return;
        }
        EmDmBloodSet2(em, 0x22, 9, 0, 0, 0);
        GameAddPoint(LVADD_CRITICALHIT);
        em->hp = 0;
        EmSetDie(em);
        EmRoutineSet(em, 1, 7, 0, 0);
    }
}

Em2aFunc Em2a_R0_move_tbl[5] = {
    em2a_R0_Init,
    em2a_R0_Move,
    0,
    0,
    (Em2aFunc) Em_R0_Scenario,
};

static Em2aFunc Em2a_R1_move_tbl[16] = {
    em2a_R1_br_Trap1Set,
    em2a_R1_Trap1Set,
    em2a_R1_br_Dummy,
    em2a_R1_Trap1Bite,
    em2a_R1_br_Dummy,
    em2a_R1_Trap1BiteSub,
    em2a_R1_br_Dummy,
    em2a_R1_Trap1Break,
    em2a_R1_br_Dummy,
    em2a_R1_Trap1Reset,
    em2a_R1_br_Dummy,
    em2a_R1_Trap1R100,
    em2a_R1_br_Dummy,
    em2a_R1_Trap2Set,
    em2a_R1_br_Dummy,
    em2a_R1_Trap2Bomb,
};

// The camera plemResuceAshley installs (the partner rescue cut): explicitly zero-initialised so it
// stays in .data.
static Camera em2a_rescue_cam = { 0 };
// COMPILER-DIFF: candidate #12 (cse related-value): `cam = &em2a_rescue_cam` after the `&em2a_rescue_cam.param.pos/at`
// pointers are known is a fresh `lis/addi` pair in the original; our cse rewrites it as `at - 0xB0`.
// An asm-labelled alias declaration gives cse a distinct SYMBOL_REF and keeps the fresh pair.
extern Camera em2a_rescue_cam_v asm("em2a_rescue_cam");
// .data is padded to 8 bytes before the linker's BSS tag word.
asm(".section .data\n\t.balign 8\n\t.text");

// Per-frame update: the damage check of the trap kind (em2aDmCkTrap1 / Trap2), clears the per-frame
// flags and runs the R0 table (Init / Move / Damage / Die / Scenario).
void cEm2a::move()
{
    Em2aWork* w = EM2A_WK(this);

    if (r_no_0 != 0) {
        switch (type) {
        case 0:
        default:
            em2aDmCkTrap1(this);
            break;
        case 1:
            em2aDmCkTrap2(this);
            break;
        case 2:
            em2aDmCkTrap2(this);
            break;
        }
    }
    w->flags &= ~0xF;
    Em2a_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
    } else {
        partsWorldCalc();
    }
}

// R0 == 0: creation. Builds the model of the trap type (0 bear trap; 1 / 2 the tripwire bombs whose
// wire parts 1 / 2 are stretched to hp/1000 * 0.5 of the model, hp = wire length), collision / hit
// boxes (em2aYarareInit), the room's ctrl11 / ctrl12, and the start routine: the bear trap Trap1Set
// (0), set 1 the room 100 dog trap (Trap1R100 5), set 2 an already sprung trap (Trap1Break, inactive);
// the bombs Trap2Set (6).
static void em2a_R0_Init(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);
    cAtariInfo* at;
    int zero;
    f32 scale;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em2a 00() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(6), ARC(7)) == 0) {
            pLog->err(0, 0, "em2a 01() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        // the wire scaling is written out in case 1 AND case 2 (jump2 cross-jumps the copies): the
        // function-scope `scale` then has two sets and local-alloc cannot tie the hp * 0.001 temp to it
        // (temp in f0, scale in f31); a shared `goto wire` block ties the whole chain to f31
        {
            f32 hp = (f32) em->hp;
            cParts* p;

            scale = hp * 0.001f * 0.5f;
            p = em->getPartsPtr(1);
            p->pos.z *= scale;
            p = em->getPartsPtr(2);
            p->pos.z *= scale;
        }
        break;
    case 2:
        if (em->modelInit(ARC(8), ARC(9)) == 0) {
            pLog->err(0, 0, "em2a 02() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        if (em->hp <= 0) {
            em->hp = 1;
        }
        {
            f32 hp = (f32) em->hp;
            cParts* p;

            scale = hp * 0.001f * 0.5f;
            p = em->getPartsPtr(1);
            p->pos.z *= scale;
            p = em->getPartsPtr(2);
            p->pos.z *= scale;
        }
        break;
    }
    em->be_flag &= ~0x10;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };

    switch (em->type) {
    case 0:
    default: {
        static const Vec size = { 750.0f, 750.0f, 750.0f };

        em->LightInfo.init2(0, 3, &ofs, &size, 2);
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        break;
    }
    case 1:
    case 2: {
        static const Vec size = { 8000.0f, 8000.0f, 8000.0f };

        em->LightInfo.init2(0, 3, &ofs, &size, 2);
        em->lockParts = 1;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 0.0f;
        em->lockOfs.z = 0.0f;
        break;
    }
    }
    }
    at = &em->atari;
    at->init(0.0f, 0.0f, 0.0f, 500.0f, 400.0f, 400.0f, 1500.0f, 3, 0x2000, 10);
    zero = 0;
    at->off();
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em2aYarareInit(em);
    w->espKind = EspPullCoreKind();
    EspDataLoad((u32) ARC(0x12), EFF_EM2A, 0);
    w->flags = zero;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    em->setStatus(EM_STATUS_ACTIVE);
    switch (em->type) {
    case 0:
    default:
        switch (em->set) {
        default:
            EmRoutineSet(em, 1, 0, zero, zero);
            break;
        case 1:
            EmRoutineSet(em, 1, 5, zero, zero);
            break;
            // dead loop: its LOOP_END note stops cse from following `beq case2`, so the arm does not
            // know zero == 0 and `z` is a fresh SI zero pseudo set before the clearStatus call (li r30,0)
            do { } while (0);
        case 2: {
            int z = 0;

            em->hp = zero;
            em->clearStatus(EM_STATUS_ACTIVE);
            EmRoutineSet(em, 1, 3, z, 1);
            break;
        }
        }
        break;
    case 1:
        EstSet(em, -1, 0, 0, EFF_EM2A, 3, 0x800, (u8) w->espKind, em, (void*) zero);
        EmRoutineSet(em, 1, 6, zero, zero);
        break;
    case 2:
        EstSet(em, -1, 0, 0, EFF_EM2A, 5, 0x800, (u8) w->espKind, em, (void*) zero);
        EmRoutineSet(em, 1, 6, zero, zero);
        break;
    }
    em2a_R0_Move(em);
}

// R0 == 1: runs the branch check and the move handler of R1 (Em2a_R1_move_tbl pairs).
static void em2a_R0_Move(cEm2a* em)
{
    Em2a_R1_move_tbl[em->r_no_1 * 2](em);
    Em2a_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the routines that have none.
static void em2a_R1_br_Dummy(cEm2a* em)
{
}

// Branch check of the armed bear trap: the player (em2aTrap1BiteCk -> Trap1Bite 1) or the partner
// (em2aTrap1BiteSubCk -> Trap1BiteSub 2) stepping into it.
static void em2a_R1_br_Trap1Set(cEm2a* em)
{
    if (em2aTrap1BiteCk(em) == 0) {
        em2aTrap1BiteSubCk(em);
    }
}

// R1 == 0 Trap1Set: the armed bear trap lying open (the open motion held).
static void em2a_R1_Trap1Set(cEm2a* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// R1 == 1 Trap1Bite: snaps shut on the player's leg (ARC 0xB, snap SE, the player catch
// plem2a_Trap1Bite with the trap camera for 120 frames, the list entry marked sprung), then stays shut
// (hp 0); a dead player leaves the trap at the motion's last frame.
static void em2a_R1_Trap1Bite(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);
    EmListData* l = &pG->Em_list[em->emset_no];

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xB), ARC(0xD), 5, 1, 0);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        SndCall(1, 0x39, &pPL->pos, 0, 0, pPL);
        EmCatchPLSet(em, 0.0f, 0, 34.69f, 0.0f, 250.42f, plem2a_Trap1Bite);
        w->camTimer = 120;
        w->biteTimer = 10;
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        l->set = 2;
        em->r_no_2++;
    case 1:
        if (w->biteTimer) {
            w->biteTimer--;
            if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
                em->hp = 0;
                em->r_no_2++;
            }
        } else {
            if (MotionMove(em, 0)) {
                em->hp = 0;
                em->r_no_2++;
            }
        }
        if (w->camTimer) {
            em2aTrap1CamMove(em);
            w->camTimer--;
        }
        if ((em->Motion.Seq_old.Free & 4) && EmDeadCk(pPL)) {
            u16 frame = (*(u16*) ARC(0xB) & 0x3FFF) - 1;

            MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, 1, frame);
            MotionMove(em, 0);
            em->hp = 0;
            em->r_no_2++;
        } else {
            em->Catch_at_adj = em->pos;
        }
        break;
    }
}

// Player catch routine of the bear trap: the leg-caught motion, 300 damage, then EndPlDamage.
static void plem2a_Trap1Bite(cPlayer* pl)
{
    pl->subArc = pPL->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x10), EM_ARC(pl, 0x11), 5, 1, 0);
        PlSetFace(1);
        EstSet(pl, -1, 0, 0, EFF_EM2A, 1, 0, ESP_CORE_KIND_NONE, pl, 0);
        LifeDownSet2(pPL, 300, 0, 1);
        pl->dmg.set(0, 0);
        pl->r_no_2++;
    case 1:
        if (EmCatchMotionMove(pl, 0.3f, 0.2f)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// R1 == 2 Trap1BiteSub: snaps shut on the partner (Ashley): EmCatchSubSet with subem2a_Trap1Bite,
// the trap stays shut (hp 0) until the player frees her with the action button
// (em2aResuceAshleyAction), then Trap1Break.
static void em2a_R1_Trap1BiteSub(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);
    EmListData* l = &pG->Em_list[em->emset_no];
    int r;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 5, 5, 0);
        SubCharSetFace(1);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        EmCatchSubSet(em, pSUB, PI / 2.0f, 0, 409.6f, 0.0f, -12.87f, subem2a_Trap1Bite);
        w->camTimer = 120;
        w->biteTimer = 10;
        em->hp = 0;
        l->set = 2;
        em->r_no_2++;
    case 1:
        if (w->biteTimer) {
            w->biteTimer--;
            r = EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            r = MotionMove(em, 0);
        }
        if (r) {
            em->r_no_2++;
        }
        em->Catch_at_adj = em->pos;
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 5, 5, 0);
        em->r_no_2++;
    case 3:
        MotionMove(em, 0);
        if (EmDeadCk(pSUB)) {
            u16 frame = (*(u16*) ARC(0xB) & 0x3FFF) - 1;

            MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, 1, frame);
            MotionMove(em, 0);
            em->hp = 0;
        } else {
            em->Catch_at_adj = em->pos;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x18), ARC(0x19), 5, 1, 0);
        em->r_no_2++;
    case 5:
        MotionMove(em, 0);
        break;
    }
}

// Ashley's routine in the bear trap: caught (300 damage) and struggling (3 damage per struggle, cries
// every 30..45 frames, "partner held" bit Status_flg[2] bit29) until freed, then EndSubDamage.
static void subem2a_Trap1Bite(cSubChar* sub_)
{
    cSubChar* sub = SUB_CHAR();

    sub->subArc = sub->pEmCatch->subArc;
    StaFlagOn(pG, STA_SUB_CATCHED);
    switch (sub->r_no_2) {
    case 0:
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x1A), 0, 5, 5, 0);
        EstSet(sub, -1, 0, 0, EFF_EM2A, 7, 0, ESP_CORE_KIND_NONE, sub, 0);
        LifeDownSet2(pSUB, 300, 0, 1);
        sub->dmg.set(0, 2);
        sub->r_no_2++;
    case 1:
        if (EmCatchMotionMove(sub, 0.3f, 0.2f)) {
            sub->r_no_2++;
            break;
        }
        if (sub->Motion.Seq_frame > 15.7f && sub->Motion.Seq_frame < 16.3f) {
            SndCall(8, 9, &sub->pos, sub->id, 0, sub);
        }
        break;
    case 2:
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x1B), 0, 5, 5, 0);
        sub->m_Work0 = 0;
        sub->r_no_2++;
    case 3:
        EmCatchMotionMove(sub, 0.3f, 0.2f);
        LifeDownSet2(pSUB, 3, 0, 1);
        if (sub->l_pl < 9000000.0f && fabsf(sub->pos.y - pPL->pos.y) < 1000.0f) {
            ActBtn.set(ACT_RESCUE, 5, (void*) em2aResuceAshleyAction, sub, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        }
        if (sub->m_Work0) {
            sub->m_Work0--;
        } else {
            sub->m_Work0 = Rnd() % 15 + 30;
            SndCall(8, 0xE, &sub->pos, sub->id, 0, sub);
        }
        break;
    case 4:
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x1C), 0, 5, 1, 0);
        EstSet(sub, -1, 0, 0, EFF_EM2A, 8, 0, ESP_CORE_KIND_NONE, sub, 0);
        sub->r_no_2++;
    case 5:
        sub->dmg.m_Timer = 2;
        if (MotionMove(sub, 0)) {
            EndSubDamage();
        }
        if (sub->Motion.Seq_frame > 107.7f && sub->Motion.Seq_frame < 108.3f) {
            SndCall(8, 0x11, &sub->pos, sub->id, 0, sub);
        }
        if ((sub->Motion.Seq_frame > 23.7f && sub->Motion.Seq_frame < 24.3f) || (sub->Motion.Seq_frame > 58.7f && sub->Motion.Seq_frame < 59.3f)) {
            SndCall(8, 0xE, &sub->pos, sub->id, 0, sub);
        }
        break;
    }
    sub->subArc = sub->subArc2;
}

// Action button callback "free Ashley": starts the player's rescue routine (plemResuceAshley) with
// both damage-held.
static void em2aResuceAshleyAction(cSubChar* sub)
{
    SetPlDamage(sub->pEmCatch, plemResuceAshley);
    sub->dmg.m_Timer = 10;
    pPL->dmg.m_Timer = 10;
    sub->r_no_2 = 4;
    sub->pEmCatch->r_no_2 = 4;
}

// Player routine of the rescue: kneels and opens the trap (rescue camera plem2aTrapCamMove), then
// EndPlDamage.
static void plemResuceAshley(cPlayer* pl)
{
    cEm* em = pl->pEmCatch;
    Mtx m;
    Vec v;

    pl->subArc = em->subArc;
    pPL->dmg.m_Timer = 10;
    switch (pl->r_no_2) {
    case 0:
        pl->ang.y = em->ang.y + PI / 2.0f;
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        PSMTXRotRad(m, 'y', pl->ang.y);
        TransMatrix(m, &em->pos);
        v.x = 175.38f;
        v.y = 0.0f;
        v.z = -685.31f;
        PSMTXMultVec(m, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x1D), 0, 3, 1, 0);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->subArc = pl->subArc2;
    plem2aTrapCamMove(pl);
}

// Installs the rescue cut camera (em2a_rescue_cam) beside the player looking at the trap.
void plem2aTrapCamMove(cModel* m)
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
    PosToPos(&c->param.at, &a, &em2a_rescue_cam.param.at, 1.0f);
    PosToPos(&c->param.pos, &v, &em2a_rescue_cam.param.pos, 1.0f);
    {
        Vec* pos = &em2a_rescue_cam.param.pos;
        Vec* at = &em2a_rescue_cam.param.at;
        f32 dx = pos->x - at->x;
        f32 dy = pos->y - at->y;
        f32 dz = pos->z - at->z;

        cam = &em2a_rescue_cam_v;   // COMPILER-DIFF: candidate #12
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    cam->param.fovy = 55.0f;
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// R1 == 3 Trap1Break: the sprung / shot trap snaps shut empty (ARC 0xC, spark effect when shot) and
// goes inactive; the list entry is marked sprung.
static void em2a_R1_Trap1Break(cEm2a* em)
{
    EmListData* l = &pG->Em_list[em->emset_no];

    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0xC), ARC(0xF), 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xC), ARC(0xE), 0, 1, 0);
            SndCall(8, 0, &em->pos, em->id, 0, em);
            EstSet(em, -1, 0, 0, EFF_EM2A, 2, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        l->set = 2;
        em->clearStatus(EM_STATUS_ACTIVE);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    }
}

// R1 == 4 Trap1Reset: the shot dog trap springs open again (ARC 0x15) and re-arms (hp 1, Trap1Set).
static void em2a_R1_Trap1Reset(cEm2a* em)
{
    switch (em->r_no_2) {
    case 0:
        (&pG->Em_list[em->emset_no])->set = 0;
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 0, 1, 0);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->hp = 1;
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_3 = 0;
            em->r_no_2 = 1;
        }
        break;
    }
}

// R1 == 5 Trap1R100: the room 100 trap holding the dog (ARC 0x13 closed on the leg) until the
// release flag (cEm::flag bit0: the dog freed / torn loose), then it stays shut and used.
static void em2a_R1_Trap1R100(cEm2a* em)
{
    switch (em->r_no_2) {
    case 0:
        em->r_no_2++;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x14), 0, 1, 0);
        MotionMove(em, 0);
        if (em->flag & 1) {
            em->r_no_2++;
        }
        break;
    case 2:
        (&pG->Em_list[em->emset_no])->set = 0;
        em->hp = 0;
        MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x14), 0, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    }
}

// R1 == 6 Trap2Set: the armed tripwire bomb: only rebuilds the model matrix each frame.
static void em2a_R1_Trap2Set(cEm2a* em)
{
    RotMatrix(em->l_mat, &em->ang);
    TransMatrix(em->l_mat, &em->pos);
    ScaleMatrix(em->l_mat, &em->scale);
    PSMTXCopy(em->l_mat, em->mat);
    em->partsMatCalc();
}

// R1 == 7 Trap2Bomb: triggered: 3 frames later the blast (em2aTrap2Bomb), then the dead trap keeps its matrix.
static void em2a_R1_Trap2Bomb(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);

    switch (em->r_no_2) {
    case 0:
        w->camTimer = 3;
        em->r_no_2++;
    case 1:
        if (w->camTimer) {
            w->camTimer--;
        } else {
            em2aTrap2Bomb(em);
            em->r_no_2++;
        }
        RotMatrix(em->l_mat, &em->ang);
        TransMatrix(em->l_mat, &em->pos);
        ScaleMatrix(em->l_mat, &em->scale);
        PSMTXCopy(em->l_mat, em->mat);
        em->partsMatCalc();
        break;
    }
}

// Hit boxes: the bear trap's plate, or the tripwire's box (length of the wire between parts 0 and 2,
// hit[0..5]) so bullets can cut the wire.
void em2aYarareInit(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);

    switch (em->type) {
    case 0:
    default:
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 500.0f, 200.0f, 500.0f, 0, YAT_FLAG_ON);
        break;
    case 1:
    case 2: {
        f32 h;

        YarareInit(em, 0.0f, -150.0f, -100.0f, 130.0f, 300.0f, 1, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[0], 0.0f, -150.0f, 100.0f, 130.0f, 300.0f, 3, YAT_FLAG_ON);
        h = fabsf(em->getPartsPtr(1)->pos.z) * 0.4f;
        YarareAdd(em, &w->hit[1], 0.0f, -30.0f, 0.0f, 130.0f, h, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
        YarareAdd(em, &w->hit[2], 0.0f, -50.0f, h, 130.0f, h, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
        YarareAdd(em, &w->hit[3], 0.0f, -70.0f, h + h, 130.0f, h, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
        YarareAdd(em, &w->hit[4], 0.0f, -50.0f, h * 3.0f, 130.0f, h, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
        YarareAdd(em, &w->hit[5], 0.0f, -30.0f, h * 4.0f, 130.0f, h, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
        break;
    }
    }
}

// 1 when something crossed the armed tripwire: the player (em2aTrap2HitCkPL) or a Ganado (HitCkEM).
int em2aTrap2HitCk(cEm2a* em)
{
    if (em->hp <= 0) {
        return 0;
    }
    if (em2aTrap2HitCkPL(em)) {
        return 1;
    }
    if (em2aTrap2HitCkEM(em)) {
        return 1;
    }
    return 0;
}

// The player inside the wire box (the wire's length + 100, 800 wide, 500 high, in the trap's frame):
// sets hp 0 and returns 1.
int em2aTrap2HitCkPL(cEm2a* em)
{
    cParts* p0;
    cParts* p2;
    f32 len;
    Mtx inv;
    Vec v;

    p0 = em->getPartsPtr(0);
    p2 = em->getPartsPtr(2);
    len = VEC_DIST(&p0->world, &p2->world)
          + 100.0f;
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (!(v.z < 0.0f) && !(v.z > len) && !(v.x > 400.0f) && !(v.x < -400.0f) && !(v.y > 250.0f) && !(v.y < -2000.0f)) {
        em->hp = 0;
        return 1;
    }
    return 0;
}

// An alive Ganado (ids 0x10..0x20) inside the wire box: 1 when found.
int em2aTrap2HitCkEM(cEm2a* em)
{
    cParts* p0;
    cParts* p2;
    f32 len;
    Mtx inv;
    Vec v;
    u32 i;

    p0 = em->getPartsPtr(0);
    p2 = em->getPartsPtr(2);
    len = VEC_DIST(&p0->world, &p2->world)
          + 100.0f;
    PSMTXInverse(em->mat, inv);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        PSMTXMultVec(inv, &e->pos, &v);
        if (!(v.z < 0.0f) && !(v.z > len) && !(v.x > 400.0f) && !(v.x < -400.0f) && !(v.y > 250.0f) && !(v.y < -2000.0f)) {
            return 1;
        }
    }
    return 0;
}

// The tripwire blast: the trap dies (inactive, invisible), explosion effects at the two posts (the
// water variant when under the surface), explosion SE, and two 3000-unit blast damage lines
// (PlWepHitCheck2 kind 0x13) along the wire hurting the player and the enemies.
void em2aTrap2Bomb(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);
    cParts* p1;
    cParts* p;
    Vec d;
    Vec e;
    f32 wh;
    int water;

    em->hp = 0;
    EmSetDie(em);
    em->be_flag &= ~2;
    em->clearStatus(EM_STATUS_ACTIVE);
    SndCall(1, 0x14, &em->pos, em->id, 0, em);
    EffectEspDelete(0, (u8) w->espKind, em, 0);
    EffectEspgenDelete(0, (u8) w->espKind, em);
    EffectEfmDelete(0, (u8) w->espKind, em);
    water = 0;
    if (GetWaterHeight(&em->pos, &wh)) {
        if (SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0) < wh) {
            water = 1;
        }
    }
    if (water) {
        EstSet(em, -1, 0, 0, EFF_EM2A, 6, 0, ESP_CORE_KIND_NONE, em, 0);
    } else {
        if (em->type == 1) {
            EstSet(em, -1, 0, 0, EFF_EM2A, 0, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        if (em->type == 2) {
            EstSet(em, -1, 0, 0, EFF_EM2A, 4, 0, ESP_CORE_KIND_NONE, em, 0);
        }
    }
    p = em->getPartsPtr(0);
    p1 = em->getPartsPtr(1);
    PSVECSubtract(&p1->world, &p->world, &d);
#line 1394 "D:/Bio4/Prog/em2a.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 2000.0f);
    PSVECAdd(&p->world, &d, &e);
    PlWepHitCheck2(0, &e, &e, 0x13, 2, 3000.0f);
    p = em->getPartsPtr(2);
    PSVECSubtract(&p1->world, &p->world, &d);
#line 1401 "D:/Bio4/Prog/em2a.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 2000.0f);
    PSVECAdd(&p->world, &d, &e);
    PlWepHitCheck2(0, &e, &e, 0x13, 2, 3000.0f);
    {
        Camera* c = &pG->Camera;
        f32 dist;

        p = em->getPartsPtr(1);
        dist = (p->world.x - c->param.pos.x) * (p->world.x - c->param.pos.x)
               + (p->world.y - c->param.pos.y) * (p->world.y - c->param.pos.y)
               + (p->world.z - c->param.pos.z) * (p->world.z - c->param.pos.z);
        if (dist < 400000000.0f) {
            f32 power = 10.0f;

            if (dist > 25000000.0f) {
                power = 8.0f;
            }
            if (dist > 100000000.0f) {
                power = 6.0f;
            }
            if (dist > 225000000.0f) {
                power = 4.0f;
            }
            QuakeExec(0, 0, 5, power, 2);
        }
    }
}

// Bear trap bite camera: eases the work Camera (cam) to a low viewpoint on the trapped leg and installs
// it as the extra camera.
void em2aTrap1CamMove(cEm2a* em)
{
    Em2aWork* w = EM2A_WK(em);
    Camera* c = &pG->Camera;
    Mtx m;
    Vec v;

    PSMTXRotRad(m, 'y', pPL->ang.y);
    TransMatrix(m, &em->pos);
    v.x = -1200.0f;
    v.y = 1300.0f;
    v.z = -1000.0f;
    PSMTXMultVec(m, &v, &v);
    PosToPos(&c->param.pos, &v, &w->cam.param.pos, 0.1f);
    v.x = 0.0f;
    v.y = 500.0f;
    v.z = -300.0f;
    PSMTXMultVec(m, &v, &v);
    PosToPos(&c->param.at, &v, &w->cam.param.at, 0.1f);
    w->cam.Up.x = 0.0f;
    w->cam.Up.y = 1.0f;
    w->cam.Up.z = 0.0f;
    {
        f32 dx = w->cam.param.pos.x - w->cam.param.at.x;
        f32 dy = w->cam.param.pos.y - w->cam.param.at.y;
        f32 dz = w->cam.param.pos.z - w->cam.param.at.z;
        w->cam.Distance = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    w->cam.param.fovy = 55.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.m_pExtraCamera = (s32) &w->cam;
}

// The alive player within 300 units (same height) of the armed trap: snaps it onto his leg
// (Trap1Bite 1). 1 when set.
int em2aTrap1BiteCk(cEm2a* em)
{
    int dead;

    if ((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z)
        > 90000.0f) {
        return 0;
    }
    dead = EmDeadCk(pPL);
    if (dead) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 500.0f) {
        return 0;
    }
    em->pos.y = pPL->pos.y;
    EmRoutineSet(em, 1, 1, 0, dead);
    return 1;
}

// The alive, free partner within 500 units of the armed trap: snaps it onto her leg (Trap1BiteSub 2).
// 1 when set.
int em2aTrap1BiteSubCk(cEm2a* em)
{
    int dead;
    int two = 2;  // COMPILER-DIFF: #13 (single-use constant re-materialised at the store: `li r0,2` next to `stb`)

    if (pSUB == 0) {
        return 0;
    }
    if (StaFlagChk(pG, STA_SUB_CATCHED)) {
        return 0;
    }
    dead = EmDeadCk(pSUB);
    if (dead) {
        return 0;
    }
    if (pSUB->hp <= 0) {
        return 0;
    }
    if ((em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) + (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z)
        > 250000.0f) {
        return 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 500.0f) {
        return 0;
    }
    em->pos.y = pSUB->pos.y;
    EmRoutineSet(em, 1, two, 0, dead);
    return 1;
}
