// em25 module (D:/Bio4/Prog/em25.cpp): the parasite. It is born out of a host enemy's head
// (cEm25::setParent / setBirth, the P_ routines keep it on the parent's parts through em25OnParent,
// em25SetParasite attaches three tentacle objects), attacks the player from there (P_Atk, the
// poison spit of em25SetPoison) or leaves the host (Dm_P_GoOut) and crawls after the player on the
// floor (Wait / Walk / Run / Turn90 / JumpAtk / Bite with the plem25_Bite catch).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em25.h"
#include "em10.h"
#include "emhit.h"
#include "obj01.h"
#include "obj16.h"
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
#include "quake.h"
#include "em.h"
#include "em_mod.h"

asm(".comm common_em25,52,4");



static void em25_R0_Init(cEm25* em);
static void em25_R0_Move(cEm25* em);
static void em25_R1_br_Dummy(cEm25* em);
static void em25_R1_Hide(cEm25* em);
static void em25_R1_Birth(cEm25* em);
static void em25_R1_Wait(cEm25* em);
static void em25_R1_Walk(cEm25* em);
static void em25_R1_Run(cEm25* em);
static void em25_R1_Turn90(cEm25* em);
static void em25_R1_br_JumpAtk(cEm25* em);
static void em25_R1_JumpAtk(cEm25* em);
static void em25_R1_Bite(cEm25* em);
static void plem25_Bite(cPlayer* pl);
static void em25_R1_P_Appear(cEm25* em);
static void em25_R1_P_Wait(cEm25* em);
static void em25_R1_P_Atk(cEm25* em);
static void em25_R1_P_Poison(cEm25* em);
static void em25_R0_Damage(cEm25* em);
static void em25_R1_Dm_P_Normal(cEm25* em);
static void em25_R1_Dm_P_GoOut(cEm25* em);
static void em25_R1_Dm_Small(cEm25* em);
static void em25_R1_Dm_Big(cEm25* em);
static void em25_R1_Dm_Frame(cEm25* em);
static void em25_R0_Die(cEm25* em);
static void em25_R1_Die_P_Normal(cEm25* em);
static void em25_R1_Die_Normal(cEm25* em);
static void em25_R1_Die_Big(cEm25* em);

Em25Func Em25_R0_move_tbl[5] = {
    em25_R0_Init,
    em25_R0_Move,
    em25_R0_Damage,
    em25_R0_Die,
    (Em25Func) Em_R0_Scenario,
};

// Routine 1 table: {branch check, routine} per xFD.
static Em25Func Em25_R1_move_tbl[24] = {
    em25_R1_br_Dummy, em25_R1_Hide,          // 0x00
    em25_R1_br_Dummy, em25_R1_Birth,         // 0x01
    em25_R1_br_Dummy, em25_R1_Wait,          // 0x02
    em25_R1_br_Dummy, em25_R1_Walk,          // 0x03
    em25_R1_br_Dummy, em25_R1_Run,           // 0x04
    em25_R1_br_Dummy, em25_R1_Turn90,        // 0x05
    em25_R1_br_JumpAtk, em25_R1_JumpAtk,     // 0x06
    em25_R1_br_Dummy, em25_R1_Bite,          // 0x07
    em25_R1_br_Dummy, em25_R1_P_Appear,      // 0x08
    em25_R1_br_Dummy, em25_R1_P_Wait,        // 0x09
    em25_R1_br_Dummy, em25_R1_P_Atk,         // 0x0A
    em25_R1_br_Dummy, em25_R1_P_Poison,      // 0x0B
};

static Em25Func Em25_R2_move_tbl[5] = {
    em25_R1_Dm_P_Normal,
    em25_R1_Dm_P_GoOut,
    em25_R1_Dm_Small,
    em25_R1_Dm_Big,
    em25_R1_Dm_Frame,
};

static Em25Func Em25_R3_move_tbl[3] = {
    em25_R1_Die_P_Normal,
    em25_R1_Die_Normal,
    em25_R1_Die_Big,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em25_flip_tbl[80] = {
    0,  1,  2,  3,  6,  7,  4,  5,  10, 11, 8,  9,  13, 12, 16, 17, 14, 15, 19, 18,
    22, 23, 20, 21, 25, 24, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
};

// Attack parameters per em25AtkCk kind: 0 bite (floor), 1 bite from the host.
static EmAtkInfo em25_atk_tbl[2] = {
    { 300.0f, PL_DM_AUTO, 500, 4, 10, 0 },
    { 500.0f, PL_DM_AUTO, 800, 0, 10, 0 },
};

// Poison projectile (SetObj08) attack parameters.
static EmAtkInfo em25_poison_atk[1] = {
    { 500.0f, PL_DM_AUTO, 800, 0, 10, 0 },
};
static int em25_atk_pad = 0;





// Module entry (SN loader): registers Em25Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    EmInitFunc = Em25Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm25 class in the manager's work.
void Em25Init(cEm* em)
{
    new (em) cEm25();
}

// Per-frame damage check (cEm25::move): a floor parasite (Mode 0) in an explosion / fire volume burns
// (Dm_Frame, R2 4). A weapon hit takes em25SetDmVal off hp with the blood effect: a dead floor
// parasite goes to Die_Big (R3 2), a dead attached one is left at 1 hp in Dm_P_Normal (the host
// decides its death); a surviving floor one flinches (Dm_Small / Dm_Big, half the time), an attached
// one recoils (Dm_P_Normal).
void em25DmCk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int wep;
    int zero;

    if ((em->be_flag & 2) && EmDeadCk(em) == 0 && w->pEm_oya == 0 && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            if (w->Fire_timer == 0) {
                w->Fire_timer = 120;
                em->setRno(2, 4, 0, 0);
                return;
            }
            break;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    zero = 0;
    em->dmg.m_Flag = zero;
    em->dmg.m_Timer = 1;
    if (wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    // COMPILER-DIFF: candidate #12 (AROUND form; r104 execEvent00 family) -- the original's cse forgets
    // `zero == 0` past the skipped `if` block, so the `Wm_no = 0` below gets its own `li`; ours
    // carries the equivalence through and would store `zero`.
    asm("" : "+r"(zero));
    em25BloodSet(em);
    w->Wm_no++;
    if (w->Wm_no > 3) {
        w->Wm_no = 0;
    }
    LifeDownSet(em, em25SetDmVal(em), 0);
    SndCall(8, 0xB, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        switch (w->Mode) {
        case 0:
        default:
            em->be_flag &= ~0x10000;
            EmSetDieCnt(em);
            em->setRno(3, 2, zero, zero);
            break;
        case 1:
            em->setRno(2, zero, zero, zero);
            em->hp = 1;
            w->Die_ck = 1;
            break;
        }
    } else {
        if (w->Be_flg & 8) {
            return;
        }
        switch (w->Mode) {
        case 0:
        default:
            if (!(Rnd() & 1)) {
                if (Rnd() & 3) {
                    em->setRno(2, 2, 0, 0);
                } else {
                    em->setRno(2, 3, 0, 0);
                }
            }
            break;
        case 1:
            em->setRno(2, 0, 0, 0);
            break;
        }
    }
}

// Per-frame update: the R0 table (Init / Move / Damage / Die / Scenario) after the damage check, the
// route check, Atk_wait countdown; a floor parasite gets collision and scenario check, an attached one
// follows its host's parts (em25OnParent); the tentacle objects, the crawl SE and effects, and the
// hide-all flag (Status_flg[1] bit26) are handled here too.
void cEm25::move()
{
    Em25Work* w = EM25_WK(this);

    if (r_no_0 && w->pEm_oya && !w->pEm_oya->isAlive()) {
        w->pEm_oya = 0;
        setRno(1, 0, 0, 0);
    }
    be_flag &= ~0x4000;
    Motion.Mot_flag &= ~0x40000000;
    em25DmCk(this);
    w->Be_flg &= ~0x2F;
    if (!DbgFlagChk(pG, DBG_EM_NO_DEATH) && w->Alive_timer) {
        w->Alive_timer--;
    }
    if (w->Atk_wait) {
        w->Atk_wait--;
    }
    if (EmDeadCk(pPL)) {
        w->Atk_wait = 120;
    }
    if (w->Fire_timer) {
        w->Fire_timer--;
    }
    em25RouteCk(this);
    Em25_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    em25ScaleCompress(this);
    if (w->Mode == 0 && hp > 0) {
        EmAtCheck(this);
        atari.move();
        SatMgr.check(this, 0);
    }
    if (hp > 0 || w->pEm_oya) {
        if (w->Se_breath_wait) {
            w->Se_breath_wait--;
        } else {
            w->Se_breath_wait = 30;
            if (w->pEm_oya) {
                SndCall(8, 0x1A, &w->pEm_oya->pos, id, 0, this);
            } else {
                SndCall(8, 8, &pos, id, 0, this);
            }
        }
        if (w->pEm_oya) {
            if (w->Eff_wait2) {
                w->Eff_wait2--;
            } else {
                w->Eff_wait2 = 2;
                EstSet(this, -1, 0, 0, EFF_EM25, 0xB, 0, ESP_CORE_KIND_NONE, this, 0);
            }
        }
    }
    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        LightInfo.EnableMask = 0x80;
    } else {
        LightInfo.EnableMask = 2;
    }
}

// R0 == 0: creation. Builds the model (ARC 5/6), no Ashley help, hit boxes (hit[0..2]), effect data,
// and the start routine by cEm::set: 0 Hide (0, waits for setParent / setBirth), 1 an active floor
// parasite (Wait 2).
static void em25_R0_Init(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    f32 fzero;
    int zero;
    u32 i;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em25() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    zero = 0;
    em->Motion.flip = em25_flip_tbl;
    EspDataLoad((u32) ARC(7), EFF_EM25, 0);
    {
        static const Vec ofs = {0.0f, 0.0f, 0.0f};
        static const Vec size = {2000.0f, 2000.0f, 2000.0f};
        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    fzero = 0.0f;
    em->lockParts = zero;
    em->lockOfs.x = fzero;
    em->lockOfs.y = fzero;
    em->lockOfs.z = fzero;
    em->atari.init(fzero, 500.0f, fzero, 350.0f, 350.0f, 350.0f, 1000.0f, 1, 0x2000, 10);
    YarareInit(em, fzero, fzero, fzero, 300.0f, 200.0f, 2, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], fzero, fzero, fzero, 100.0f, 100.0f, 0x1D, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], fzero, fzero, fzero, 100.0f, 100.0f, 0x1E, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], fzero, fzero, fzero, 100.0f, 100.0f, 0x1F, YAT_FLAG_ON);
    w->Be_flg = zero;
    w->Compress_y = 1.0f;
    w->pEm_oya = (cEm*) zero;
    w->oya_parts = zero;
    w->Wm_no = zero;
    w->Die_ck = zero;
    w->Eff_wait1 = zero;
    w->Eff_wait2 = zero;
    w->Atk_wait = zero;
    w->Atk_enable = zero;
    for (i = 0; i < 3; i++) {
        w->pParasite[i] = 0;
    }
    w->EffKindId = EspPullCoreKind();
    switch (em->set) {
    case 0:
    default:
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        w->Mode = 0;
        break;
    case 1:
        em->setStatus(EM_STATUS_ACTIVE);
        w->Alive_timer = 900;
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        EstSet(em, -1, 0, 0, EFF_EM25, 2, 0, w->EffKindId, em, 0);
        em25SetParasite(em);
        break;
    }
    em25_R0_Move(em);
}

// R0 == 1: runs the branch check and the move handler of R1 (Em25_R1_move_tbl pairs).
static void em25_R0_Move(cEm25* em)
{
    Em25_R1_move_tbl[em->r_no_1 * 2](em);
    Em25_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the routines that have none.
static void em25_R1_br_Dummy(cEm25* em)
{
}

// R1 == 0 Hide: parked out of play (hp 0, invisible, no collision, lock-on off) until a host calls
// setParent (P_Appear) or the room calls setBirth (Birth); be_flag 0x4000 keeps it hidden.
static void em25_R1_Hide(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cModelInfo* info;
    cParts* p;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(8), 0, 0, 5, 0);
        MotionMove(em, 0);
        em->hp = 0;
        em->be_flag &= ~2;
        em->be_flag &= ~0x10000;
        w->pEm_oya = 0;
        w->Die_ck = 0;
        em->atari.off();
        em->setStatus(EM_STATUS_LOCKOFF);
        em->be_flag &= ~0x10;
        w->Compress_y = 1.0f;
        for (info = em->pModelInfo; info; info = info->pList) {
            info->color[0] = 0xFF;
            info->color[1] = 0xFF;
            info->color[2] = 0xFF;
        }
        for (p = em->pList; p; p = p->pList) {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
        w->Compress_y = 1.0f;
        em->r_no_2++;
        break;
    case 1:
        em->be_flag |= 0x4000;
        break;
    }
}

// R1 == 1 Birth: appears on the floor at the setBirth position (full hp, visible, active) with the
// landing motion 0x26, then Wait (2).
static void em25_R1_Birth(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cParts* p;

    switch (em->r_no_2) {
    case 0:
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        em->invisible_factor = 1.0f;
        em->hp = em->hp_max;
        em->be_flag |= 2;
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        TransMatrix(em->mat, &em->scale);
        MotionSetCore(em, &em->Motion, ARC(0x26), ARC(0x2E), 0, 1, 0);
        for (p = em->pList; p; p = p->pList) {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
        em->setStatus(EM_STATUS_ACTIVE);
        em->atari.on();
        em->clearStatus(EM_STATUS_LOCKOFF);
        w->Compress_y = 1.0f;
        w->Alive_timer = 900;
        EstSet(em, -1, 0, 0, EFF_EM25, 2, 0, w->EffKindId, em, 0);
        em25SetParasite(em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R1 == 2 Wait: idle (ARC 0x19) 30..60 frames, then Turn90 (5) when the player is off to the side,
// Run (4) / Walk (3) towards him, or JumpAtk (6) when close; dies by itself when Alive_timer runs out
// (Die_Big).
static void em25_R1_Wait(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x19), 0, 5, 5, 0);
        if (w->Atk_wait <= 29) {
            w->Atk_wait = (u8) (Rnd() % 30) + 30;
        }
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if ((s16) pG->pl_life > 0) {
            if (w->Go_rot > 1.22173047f) {
                em->setRno(1, 5, 0, 0);
            } else if (w->Atk_wait == 0) {
                if (em->l_pl < 4000000.0f && w->Go_rot < 0.523598790f) {
                    em->setRno(1, 6, 0, 0);
                } else if (em->l_pl > 25000000.0f) {
                    em->setRno(1, 4, 0, 0);
                } else {
                    em->setRno(1, 3, 0, 0);
                }
            }
        }
        break;
    }
    if (w->Alive_timer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        em->setRno(3, 2, 0, 0);
    }
}

// R1 == 3 Walk: crawls (ARC 0x1A) towards the route point turning 0.098 rad/frame, back to Wait after
// the timer or when close, Turn90 when the target is far off the side; Alive_timer death.
static void em25_R1_Walk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x1A), ARC(0x1B), 5, 5, 0);
        w->Timer = (u8) (Rnd() % 5) + 5;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.0981747732f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            if (w->Timer == 0) {
                em->setRno(1, 2, 0, 0);
                break;
            }
            w->Timer--;
        }
        if (em->l_pl < 4000000.0f && w->Go_rot < 0.523598790f) {
            em->setRno(1, 2, 0, 0);
        } else if (w->Go_rot > 1.22173047f) {
            em->setRno(1, 5, 0, 0);
        }
        break;
    }
    if (w->Alive_timer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        em->setRno(3, 2, 0, 0);
    }
}

// R1 == 4 Run: the fast crawl (ARC 0x1C), same exits as Walk.
static void em25_R1_Run(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x1C), ARC(0x1D), 5, 5, 0);
        w->Timer = (u8) (Rnd() % 3) + 2;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.0981747732f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            if (w->Timer == 0) {
                em->setRno(1, 2, 0, 0);
                break;
            }
            w->Timer--;
        }
        if (em->l_pl < 4000000.0f && w->Go_rot < 0.523598790f) {
            em->setRno(1, 2, 0, 0);
        } else if (w->Go_rot > 1.22173047f) {
            em->setRno(1, 5, 0, 0);
        }
        break;
    }
    if (w->Alive_timer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        em->setRno(3, 2, 0, 0);
    }
}

// R1 == 5 Turn90: turns 90 deg towards the player (ARC 0x1E, mirrored by side), then Run / Walk when
// he is still off the side or Wait.
static void em25_R1_Turn90(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f) < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x1E), ARC(0x1F), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x1E), ARC(0x1F), 5, 0x41, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f)) > 1.22173047f) {
                em->r_no_2 = 0;
            } else if (em->l_pl > 9000000.0f) {
                if (em->l_pl > 25000000.0f) {
                    em->setRno(1, 4, 0, 0);
                } else {
                    em->setRno(1, 3, 0, 0);
                }
            } else {
                em->setRno(1, 2, 0, 0);
            }
        }
        break;
    }
    if (w->Alive_timer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        em->setRno(3, 2, 0, 0);
    }
}

// Branch check of JumpAtk (6): the catch test em25CatchCk on the leap's hit frame -> Bite (7).
static void em25_R1_br_JumpAtk(cEm25* em)
{
    if ((em->Motion.Seq_old.Free & 1) && em25CatchCk(em)) {
        em->setRno(1, 7, 0, 0);
    }
}

// R1 == 6 JumpAtk: leaps at the player (ARC 0x26) turning towards him; a miss scores an escape and
// goes back to Wait (2).
static void em25_R1_JumpAtk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x26), ARC(0x2E), 5, 1, 0);
        w->Timer = 10;
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.0981747732f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R1 == 7 Bite: latched onto the player's face (ARC 0x27, plem25_Bite on the player, catch blend
// EmCatchMotionMove): drains 5 hp per frame while he mashes the button; a failed mash bites for 500
// and kills him at 1 hp (head lost SE); thrown off into Wait with Atk_wait 60.
static void em25_R1_Bite(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cAtariInfo* at;
    int fe;
    int end;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x27), 0, 0, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM25, 3, 0, ESP_CORE_KIND_NONE, em, (void*) fe);
        EmCatchPLSet(em, 0.0f, 2, -150.0f, 0.0f, 628.0f, plem25_Bite);
        PlGachaInit();
        SndCall(8, 0x12, &em->pos, em->id, 0, em);
        w->Timer = 50;
        w->Timer2 = 10;
        em->r_no_3 = Rnd() & 3;
        w->TmpU32 = 0;
        em->r_no_2++;
    case 1:
        em->dmg.set(0, 10);
        PlGachaMove();
        if (w->Timer2) {
            w->Timer2--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMove(em, 0);
        }
        if (end) {
            at = &em->atari;
            at->setPriority(0);
            at->on();
            w->Atk_wait = 60;
            em->dmg.m_Timer = 10;
            em->setRno(1, 2, 0, 0);
            break;
        }
        if (em->Motion.Seq_frame > 19.7000008f && em->Motion.Seq_frame < 20.2999992f) {
            w->TmpU32 = SndCall(8, 0x13, &em->pos, em->id, 0, em);
        }
        if (w->Timer) {
            w->Timer--;
            LifeDownSet2(pPL, 5, 0, 1);
            if (w->Timer == 0) {
                if ((u32) PlGachaGet() < 15) {
                    LifeDownSet2(pPL, 500, 0, 1);
                }
                if ((s16) pG->pl_life <= 1) {
                    pG->pl_life = 0;
                    em->r_no_2++;
                } else {
                    SndStop(w->TmpU32, 0);
                    SndCall(8, 0x14, &em->pos, em->id, 0, em);
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x2B), 0, 0, 1, 0);
        SndStop(w->TmpU32, 0);
        PlSetDamageSe(0xD);
        EmCatchPLSet(em, 0.0f, 2, 24.3600006f, 0.0f, 307.75f, plem25_Bite);
        pPL->r_no_2 = fe;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            at = &em->atari;
            at->setPriority(0);
            at->on();
            w->Atk_wait = 60;
            em->dmg.m_Timer = 10;
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage routine of the bite: grabbed at the face (weapon hidden), the struggle, the throw-off
// or the death; ends with EndPlDamage.
static void plem25_Bite(cPlayer* pl)
{
    int end;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pPL->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x2F), 0, 0, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.000031f, 400.0f);
        pl->m_Work0 = 10;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMove(pl, 0);
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x30), 0, 0, 1, 0);
        pl->atari.set(10, 480.000031f, 400.0f);
        pl->r_no_2++;
    case 3:
        MotionMove(pl, 0);
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// R1 == 8 P_Appear: bursts out of the host's neck (setParent): grows and fades in (scale, invisible_factor
// +0.1 per frame) with the emerge motion, then P_Wait (9) with hp 0 (the host takes the damage) and
// Atk_enable set.
static void em25_R1_P_Appear(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->scale.x = 0.0f;
        em->scale.y = 0.0f;
        em->scale.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0xC), ARC(0xD), 0, 1, 0);
        w->Se_breath_wait = 0;
        em->setStatus(EM_STATUS_ACTIVE);
        EstSet(em, -1, 0, 0, EFF_EM25, 2, 0, w->EffKindId, em, (void*) fe);
        w->Atk_enable = 0;
        w->Compress_y = 1.0f;
        em->invisible_factor = 0.0f;
        em->be_flag |= 2;
        em->r_no_2++;
    case 1:
        em->scale.x = em->scale.x * 0.899999976f + 0.100000001f;
        em->scale.y = em->scale.y * 0.899999976f + 0.100000001f;
        em->scale.z = em->scale.z * 0.899999976f + 0.100000001f;
        em->invisible_factor += 0.100000001f;
        if (em->invisible_factor > 1.0f) {
            em->invisible_factor = 1.0f;
        }
        em25OnParent(em);
        if (MotionMove(em, 0)) {
            em->scale.x = 1.0f;
            em->scale.y = 1.0f;
            em->scale.z = 1.0f;
            em->invisible_factor = 1.0f;
            em->hp = 0;
            // A local for the 1: stored directly, it shares a register with setRno's own
            // literal arguments below instead of getting its own.
            int n = 1;
            w->Atk_enable = n;
            em->setRno(1, 9, 0, 0);
        }
        break;
    }
}

// R1 == 9 P_Wait: rides the host's parts (em25OnParent) between attacks: the idle (ARC 8), rearing up
// (9) when the player is within 5000 units or the host's part tilts up, ducking (0xA) when it tilts
// down (Be_flg bit5 = tilted more than 30 deg); the host triggers P_Atk / P_Poison via setAtk / setPoison.
static void em25_R1_P_Wait(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec v;
    cParts* p;
    f32 ang;
    f32 d;

    w->Atk_enable = 1;
    ang = 0.0f;
    if (w->pEm_oya) {
        p = w->pEm_oya->getPartsPtr(w->oya_parts);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1.0f;
        PSMTXMultVecSR(p->mat, &v, &v);
        if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
            v.z = 1.0f;
        }
#line 1275 "D:/Bio4/Prog/em25.cpp"
        VECNormalize(&v, &v);
        ang = asinf(v.y);
    }
    if (ang > 0.523598790f || ang < -0.523598790f) {
        w->Be_flg |= 0x20;
    }
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(8), 0, 30, 5, 0);
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        em25OnParent(em);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
            break;
        }
        if (!(em->r_no_2 == 2 || em->r_no_2 == 3) && ang > 1.04719758f) {
            em->r_no_2 = 2;
            break;
        }
        if (!(em->r_no_2 == 4 || em->r_no_2 == 5) && ang < -1.04719758f) {
            em->r_no_2 = 4;
            break;
        }
        p = em->getPartsPtr(0);
        d = (pPL->pos.x - p->world.x) * (pPL->pos.x - p->world.x) +
            (pPL->pos.y - p->world.y) * (pPL->pos.y - p->world.y) +
            (pPL->pos.z - p->world.z) * (pPL->pos.z - p->world.z);
        if (d < 25000000.0f) {
            em->r_no_2 = 2;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(9), 0, 30, 5, 0);
        em->r_no_2++;
    case 3:
        em25OnParent(em);
        MotionMove(em, 0);
        p = em->getPartsPtr(0);
        d = (pPL->pos.x - p->world.x) * (pPL->pos.x - p->world.x) +
            (pPL->pos.y - p->world.y) * (pPL->pos.y - p->world.y) +
            (pPL->pos.z - p->world.z) * (pPL->pos.z - p->world.z);
        if ((d > 64000000.0f && ang < 0.785398185f) || ang < -0.523598790f) {
            em->r_no_2 = 0;
        }
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0xA), 0, 30, 5, 0);
        em->r_no_2++;
    case 5:
        em25OnParent(em);
        MotionMove(em, 0);
        if (ang > -0.785398185f) {
            em->r_no_2 = 0;
        }
        break;
    }
}

// R1 == 0xA P_Atk: the bite from the host's shoulders (em25AtkCk kind 1 on the hit frames, Atk_ck for
// the host's ckAtkHit), then back to P_Wait (9).
static void em25_R1_P_Atk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x13), ARC(0x14), 5, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM25, 0xC, 0, ESP_CORE_KIND_NONE, em, (void*) fe);
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        em25OnParent(em);
        if (MotionMove(em, 0)) {
            em->setRno(1, 9, 0, 0);
        } else if (em->Motion.Seq_old.Free & 1) {
            em25AtkCk(em, 1, 2);
        }
        break;
    }
}

// R1 == 0xB P_Poison: spits the poison projectile (em25SetPoison at frame 18) at the player / partner,
// then P_Wait (9).
static void em25_R1_P_Poison(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x15), ARC(0x16), 5, 1, 0);
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        em25OnParent(em);
        if (MotionMove(em, 0)) {
            em->setRno(1, 9, 0, 0);
        } else if (em->Motion.Seq_frame > 17.7000008f && em->Motion.Seq_frame < 18.2999992f) {
            em25SetPoison(em);
        }
        break;
    }
}

// R0 == 2: damage, runs Em25_R2_move_tbl (Dm_P_Normal, Dm_P_GoOut, Dm_Small, Dm_Big, Dm_Frame).
static void em25_R0_Damage(cEm25* em)
{
    EM25_WK(em)->Be_flg |= 8;
    Em25_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_P_Normal: the attached parasite recoils from a hit (one of two motions), then P_Wait.
static void em25_R1_Dm_P_Normal(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, &em->Motion, ARC(0xE), 0, 5, 5, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0xF), 0, 5, 5, 0);
        }
        SndCall(8, 5, &em->pos, em->id, 0, em);
        w->Timer = 30;
        em->r_no_2++;
    case 1:
        em25OnParent(em);
        if (w->Timer) {
            w->Timer--;
            w->Atk_enable = 0;
        } else {
            w->Atk_enable = 1;
        }
        if (MotionMove(em, 0)) {
            em->setRno(1, 9, 0, 0);
        }
        break;
    }
}

// R0 2 / R1 == 1 Dm_P_GoOut: leaves the dying host (setGoOut): drops to the host's floor position
// (pulled back when a wall is in the way), becomes a floor parasite (Mode 0, full hp, Alive_timer
// 900, tentacles em25SetParasite) with the landing motion (0x24 or 0x26 by r_no_3), then Wait (2).
static void em25_R1_Dm_P_GoOut(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec a;
    Vec b;
    Vec hit;
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->setStatus(EM_STATUS_ACTIVE);
        if (w->pEm_oya) {
            em->ang.y = w->pEm_oya->ang.y;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = w->pEm_oya->pos.y;
        em->pos.z = em->mat[2][3];
        if (w->pEm_oya) {
            a = w->pEm_oya->pos;
            b = em->pos;
            a.y += 100.0f;
            b.y += 100.0f;
            if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0) || ObjHitCheck(&hit, 0, &a, &b, 1)) {
                em->pos = w->pEm_oya->pos;
            }
        }
        em->pos_old = em->pos;
        w->Mode = 0;
        w->pEm_oya = 0;
        if (em->r_no_3) {
            MotionSetCore(em, &em->Motion, ARC(0x24), ARC(0x25), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x26), ARC(0x2E), 5, 1, 0);
        }
        w->Alive_timer = 900;
        em25SetParasite(em);
        em->hp = em->hp_max;
        SndCall(8, 5, &em->pos, em->id, 0, em);
        w->Compress_y = 1.0f;
        em->dmg.m_Timer = 2;
        MotionMove(em, 0);
        em->partsWorldCalc();
        em->r_no_2++;
        break;
    case 1:
        em->atari.on();
        if (MotionMove(em, 0)) {
            em->setRno(fe, 2, 0, 0);
        }
        break;
    }
}

// R0 2 / R1 == 2 Dm_Small: the floor parasite's small flinch, then Wait.
static void em25_R1_Dm_Small(cEm25* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x21), 0, 5, 1, 0);
        SndCall(8, 5, &em->pos, em->id, 0, em);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R0 2 / R1 == 3 Dm_Big: the floor parasite knocked over: the fall, Die_Normal (R3 1) when dead, else
// gets up and back to Wait.
static void em25_R1_Dm_Big(cEm25* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x29), ARC(0x2A), 5, 1, 0);
        SndCall(8, 5, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                em->setRno(3, 1, 0, 0);
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x24), ARC(0x25), 5, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R0 2 / R1 == 4 Dm_Frame: burning (explosion / fire volume): 1000 damage, the burn motion; dies into
// Die_Normal, else recovers to Wait.
static void em25_R1_Dm_Frame(cEm25* em)
{
    cModelInfo* info;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x29), 0, 5, 1, 0);
        LifeDownSet(em, 1000, 0);
        SndCall(8, 5, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                EmSetDieCnt(em);
                em->be_flag &= ~0x10000;
                em->setRno(3, 1, 0, 0);
            } else {
                em->r_no_2++;
            }
        } else if (em->hp <= 0) {
            for (info = em->pModelInfo; info; info = info->pList) {
                if (info->color[0] > 0x20) {
                    info->color[0] -= 0x20;
                }
                info->color[2] = info->color[1] = info->color[0];
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x24), ARC(0x25), 5, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->setRno(1, 2, 0, 0);
        }
        break;
    }
}

// R0 == 3: death, runs Em25_R3_move_tbl (Die_P_Normal, Die_Normal, Die_Big).
static void em25_R0_Die(cEm25* em)
{
    Em25_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_P_Normal: the attached parasite dies with its host (setDie): the death effect and
// shrink (Compress_y), then back to Hide (0) for reuse.
static void em25_R1_Die_P_Normal(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x10), 0, 5, 1, 0);
        w->Die_ck = 1;
        w->Timer = 15;
        em->hp = 0;
        em->clearStatus(EM_STATUS_ACTIVE);
        EffectEspDelete(0, w->EffKindId, em, 0);
        EffectEspgenDelete(0, w->EffKindId, em);
        EffectEfmDelete(0, w->EffKindId, em);
        em->r_no_2++;
    case 1:
        em25OnParent(em);
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else {
            em->scale.x *= 0.899999976f;
            em->scale.y *= 0.899999976f;
            em->scale.z *= 0.899999976f;
        }
        if (em->scale.x < 0.00999999978f) {
            em->scale.x = 0.0f;
            em->scale.y = 0.0f;
            em->scale.z = 0.0f;
            em->be_flag &= ~2;
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 1 Die_Normal: the floor parasite's death (inactive, item drop), dissolves and returns
// to Hide (0).
static void em25_R1_Die_Normal(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x20), 0, 5, 1, 0);
        em25ClearParasite(em);
        EstSet(em, -1, 0, 0, EFF_EM25, 4, 0, ESP_CORE_KIND_NONE, em, (void*) fe);
        EffectEspDelete(0, w->EffKindId, em, 0);
        EffectEspgenDelete(0, w->EffKindId, em);
        EffectEfmDelete(0, w->EffKindId, em);
        em->clearStatus(EM_STATUS_ACTIVE);
        SndCall(8, 0xE, &em->pos, em->id, 0, em);
        em->hp = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->atari.off();
            em->r_no_2++;
            em->setStatus(EM_STATUS_ITEMSET);
            EmSetDropItem(em);
            SndCall(8, 0xD, &em->pos, em->id, 0, em);
        }
        break;
    case 2:
        em->pos.y -= 3.0f;
        MotionMove(em, 0);
        w->Compress_y -= 0.00999999978f;
        if (w->Compress_y < 0.100000001f) {
            w->Compress_y = 0.100000001f;
            em->be_flag &= ~2;
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 2 Die_Big: the floor parasite bursts (the big death effect), item drop, dissolves and
// returns to Hide (0).
static void em25_R1_Die_Big(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x22), ARC(0x23), 5, 1, 0);
        em->clearStatus(EM_STATUS_ACTIVE);
        em25ClearParasite(em);
        EstSet(em, -1, 0, 0, EFF_EM25, 4, 0, ESP_CORE_KIND_NONE, em, (void*) fe);
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        EffectEspDelete(0, w->EffKindId, em, 0);
        EffectEspgenDelete(0, w->EffKindId, em);
        EffectEfmDelete(0, w->EffKindId, em);
        em->hp = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->atari.off();
            em->r_no_2++;
            em->setStatus(EM_STATUS_ITEMSET);
            EmSetDropItem(em);
        }
        break;
    case 2:
        em->pos.y -= 3.0f;
        MotionMove(em, 0);
        w->Compress_y -= 0.00999999978f;
        if (w->Compress_y < 0.100000001f) {
            w->Compress_y = 0.100000001f;
            em->be_flag &= ~2;
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// Keeps the parasite on its parent's parts: turns the head towards the player (or the partner when
// she is much closer) and concatenates the local matrix onto the parent parts matrix.
void em25OnParent(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cEm* parent;
    cParts* p;
    cParts* t;
    Vec tgt;
    f32 d;
    f32 ang;

    parent = w->pEm_oya;
    if (parent) {
        p = parent->getPartsPtr(w->oya_parts);
        if (w->Be_flg & 0x20) {
            em->ang.y *= 0.899999976f;
        } else {
            t = pPL->getPartsPtr(3);
            tgt = t->world;
            if (pSUB) {
                d = VEC_DIST(&parent->pos, &pPL->pos);
                if (d > VEC_DIST(&parent->pos, &pSUB->pos) +
                            3000.0f) {
                    t = pSUB->getPartsPtr(3);
                    tgt = t->world;
                }
            }
            ang = Muku(&parent->pos, &tgt, parent->ang.y, 1.57079637f);
            ang = Muku2(em->ang.y, ang, 0.0981747732f);
            em->ang.y += ang;
        }
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        PSMTXConcat(p->mat, em->mat, em->mat);
        em->Motion.Mot_flag |= 0x40000000;
    }
}

// 1 while the parasite has no host (free for em10's em10SetParasite / em10SearchParasite).
int cEm25::ckParent()
{
    return EM25_WK(this)->pEm_oya == 0;
}

// Attaches the parasite to `parent`'s part `parts` at the local offset / rotation (Mode 1) and starts
// P_Appear (8).
void cEm25::setParent(cEm* parent, int parts, Vec* ppos, Vec* prot)
{
    Em25Work* w = EM25_WK(this);

    w->pEm_oya = parent;
    w->oya_parts = parts;
    if (ppos) {
        pos = *ppos;
    } else {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
    }
    if (prot) {
        ang = *prot;
    } else {
        ang.x = 0.0f;
        ang.y = 0.0f;
        ang.z = 0.0f;
    }
    atari.off();
    // A local for the 0: stored directly, it shares a register with the w->Mode/setRno
    // literals below instead of getting its own.
    int n = 0;
    w->Die_ck = n;
    w->Mode = 1;
    setRno(1, 8, 0, 0);
}

// Host dying: leaves the host (Dm_P_GoOut, r_no_3 = `flag` picks the landing motion).
void cEm25::setGoOut(int flag)
{
    if (flag) {
        setRno(2, 1, 0, 1);
    } else {
        setRno(2, 1, 0, 0);
    }
}

// Host request: back to the attached idle (P_Wait 9).
void cEm25::setWait()
{
    setRno(1, 9, 0, 0);
}

// Host request (em10_R1_ParasiteAtk): the bite from the shoulders (P_Atk 0xA), Atk_ck cleared.
void cEm25::setAtk()
{
    r_no_0 = 1;
    r_no_1 = 0xA;
    EM25_WK(this)->Atk_ck = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// 1 when the current attack hit the player / partner (Atk_ck).
int cEm25::ckAtkHit()
{
    if (EM25_WK(this)->Atk_ck) {
        return 1;
    }
    return 0;
}

// Host request: the poison spit (P_Poison 0xB).
void cEm25::setPoison()
{
    setRno(1, 0xB, 0, 0);
}

// Motion state of the attack: non-zero when the attack motion ended.
int cEm25::ckAtkEnd()
{
    return Motion.Mot_state;
}

// Host request: die with the host (Die_P_Normal, R3 0).
void cEm25::setDie()
{
    setRno(3, 0, 0, 0);
}

// 1 once the parasite has died (dead).
int cEm25::ckDie()
{
    if (EM25_WK(this)->Die_ck) {
        return 1;
    }
    return 0;
}

// Parks the parasite out of play (hp 0, invisible) in Hide (0).
void cEm25::setHide()
{
    Em25Work* w = EM25_WK(this);

    hp = 0;
    be_flag &= ~2;
    w->pEm_oya = 0;
    w->Die_ck = 0;
    setRno(1, 0, 0, 0);
}

// 1 while the parasite is parked in Hide (R0 1 / R1 0).
int cEm25::ckHide()
{
    return (r_no_0 == 1 && r_no_1 == 0);
}

// Room script: a floor parasite is born at `ppos` facing `ang` (Birth 1).
void cEm25::setBirth(Vec* ppos, f32 ang)
{
    setPos(ppos);
    pos_old = *ppos;
    this->ang.y = ang;
    setRno(1, 1, 0, 0);
}

// Bite hit test on the attack frames: the capsule at part `parts` against the player / partner with
// em25_atk_tbl[no] (0 floor bite, 1 from the host), once per attack (Atk_ck); blood on a hit, the
// player's head comes off when it kills him (em25PlHeadLost). 1 = hit.
int em25AtkCk(cEm25* em, int no, int parts)
{
    Em25Work* w = EM25_WK(em);
    EmAtkInfo* atk;
    cParts* p;
    int hit;

    if (w->Atk_ck) {
        return 0;
    }
    atk = &em25_atk_tbl[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(atk, &p->world, &p->world_old, 0);
    if (hit) {
        if (hit & 1) {
            w->Atk_ck = 1;
            if (no == 1) {
                EmPlBloodSet2(em, &p->world, 1, 0x1D, 0x10);
                if ((s16) pG->pl_life <= 0) {
                    em25PlHeadLost();
                }
                SndCall(8, 0x21, &em->pos, em->id, 0, em);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        }
        if (hit & 2) {
            if (pSUB) {
                w->Atk_ck = 1;
                if (no == 1) {
                    EmSubBloodSet(em, &p->world, 1, 0x1D, 0x10);
                    SndCall(8, 0x21, &em->pos, em->id, 0, em);
                }
            }
        }
        return 1;
    }
    return 0;
}

// Catch test of the leap (br_JumpAtk): the player alive, not held, on the leap's hit frame, inside the
// box in front and reachable (scenario / object probes). 1 = caught -> Bite.
int em25CatchCk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Mtx inv;
    Vec pos;
    Vec a;
    Vec b;
    Mtx m;

    if (EmDeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (!(em->Motion.Seq_old.Free & 1)) {
        return 0;
    }
    if (!(w->Be_flg & 1)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED)) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &pos);
    if (pos.x < -300.0f || pos.x > 300.0f) {
        return 0;
    }
    if (pos.y < -250.0f || pos.y > 250.0f) {
        return 0;
    }
    if (pos.z < 0.0f || pos.z > 1000.0f) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
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
        return 0;
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
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    return 1;
}

// Squashes the parts vertically by Compress_y (the die routines shrink the body into the floor).
void em25ScaleCompress(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Mtx m;
    Vec scale;
    cParts* p;

    PSMTXIdentity(m);
    scale.x = 1.0f;
    scale.y = w->Compress_y;
    scale.z = 1.0f;
    ScaleMatrix(m, &scale);
    for (p = em->pList; p; p = p->pList) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->world.x;
        p->mat[1][3] = p->world.y;
        p->mat[2][3] = p->world.z;
    }
}

// Per frame for a floor parasite: the route point / angle to the player (Pl_pos, Pl_dir, Be_flg
// bit0 = route found) and the target copies used by the crawl routines.
void em25RouteCk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (w->Mode != 0) {
        return;
    }
    if (em->r_no_0 != 0) {
        if ((pG->Frame_cnt & 7) != (em->emset_no & 7)) {
            return;
        }
    }
    if (RouteCkToPos(em, &pPL->pos, &w->Pl_pos, 0, 0)) {
        w->Be_flg |= 1;
    }
    w->Pl_dir = Muku(&em->pos, &w->Pl_pos, em->ang.y, 3.14159274f);
    w->Pl_rot = fabsf(w->Pl_dir);
    if (em->r_no_0 == 0) {
        w->Pl_dir = 0.0f;
        w->Pl_rot = 0.0f;
        em->l_pl = 100000000.0f;
    }
    w->Go_pos = w->Pl_pos;
    w->Go_dir = w->Pl_dir;
    w->Go_rot = w->Pl_rot;
    w->L_go = em->l_pl;
    w->pEm = pPL;
    w->Be_flg &= ~4;
}

// Damage of the weapon hit: twice GetWepDmVal (`near` for a muzzle within 6000 units); the flash
// grenade (0x17 / 0x2A) kills outright (9999).
int em25SetDmVal(cEm25* em)
{
    int near = 0;
    int dmg;

    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    dmg = 100;
    if (em->dmg.m_Wep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    dmg *= 2;
    if (em->dmg.m_Wep == 0x17 || em->dmg.m_Wep == 0x2A) {
        dmg = 9999;
    }
    return dmg;
}

// Creates the three tentacle objects (pPara[], cObj16 type 7) on the floor parasite's body parts once
// (Be_flg bit4).
void em25SetParasite(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec pos;
    Vec rot;
    u16 step;

    if (w->pParasite[0] == 0 && w->pParasite[1] == 0 && w->pParasite[2] == 0 && !(w->Be_flg & 0x10)) {
        step = (*(u16*) ARC(0x33) & 0x3FFF) / 3;
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = -0.185004905f;
        rot.y = -0.0663225129f;
        rot.z = 2.80474400f;
        w->pParasite[0] = (cObj16*) SetObj16(ARC(0x31), ARC(0x32), em, em, 0x21, 7, &pos, &rot);
        if (w->pParasite[0]) {
            MotSetObj16(w->pParasite[0], ARC(0x33), 4, 0);
        }
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 3.36673999f;
        w->pParasite[1] = (cObj16*) SetObj16(ARC(0x31), ARC(0x32), em, em, 0x22, 7, &pos, &rot);
        if (w->pParasite[1]) {
            MotSetObj16(w->pParasite[1], ARC(0x33), 4, step);
        }
        rot.x = 0.668461084f;
        rot.y = 0.0f;
        rot.z = 3.14159274f;
        w->pParasite[2] = (cObj16*) SetObj16(ARC(0x31), ARC(0x32), em, em, 0x23, 7, &pos, &rot);
        if (w->pParasite[2]) {
            MotSetObj16(w->pParasite[2], ARC(0x33), 4, step * 2);
        }
        w->Be_flg |= 0x10;
    }
}

// Removes the tentacle objects (clearLostWait) and clears Be_flg bit4.
void em25ClearParasite(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    u32 i;

    if (w->Be_flg & 0x10) {
        for (i = 0; i < 3; i++) {
            if (w->pParasite[i]) {
                w->pParasite[i]->clearLostWait();
                w->pParasite[i] = 0;
            }
        }
        w->Be_flg &= ~0x10;
    }
}

// Blood effect of the hit by weapon kind (EmDmBloodSet2 0x1D): small for handguns (a quarter of the
// TMP / knife hits none), big for explosives / magnum / rifles and a near shotgun hit.
void em25BloodSet(cEm25* em)
{
    Vec pos;
    Vec dir;
    int near = 0;

    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    switch (em->dmg.m_Wep) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x11:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x1D, 0, 0, 0, 0);
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x1D, 0xD, 0, 0, 0);
        if ((Rnd() & 3) == 0) {
            if (EmGetDmPos(em, &pos, &dir)) {
                EstSet(0, -1, &pos, 0, EFF_EM25, 0xE, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x1D, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1D, 0, 0, 0, 0);
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
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x1D, 1, 0, 0, 0);
        break;
    case 0x10:
    case 0x1A:
        EmDmBloodSet2(em, 0x1D, 0xF, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    case 0x2A:
    default:
        break;
    }
}

// The player's head comes off: the head object flies away (regions other than Japan).
void em25PlHeadLost()
{
    Vec ofs;
    Vec spd;
    cParts* p;
    cObj* obj;
    int zero;

    if (pSys->eff_country == 0) {
        PlSetDamageSe(0xD);
        EstSet(pPL, -1, 0, 0, EFF_EM25, 0xA, 0, ESP_CORE_KIND_NONE, pPL, 0);
        return;
    }
    zero = 0;
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet(pPL, -1, 0, 0, EFF_EM10, 0x45, 0, ESP_CORE_KIND_NONE, pPL, (void*) zero);
    pPL->setHead(0);
    p = pPL->getPartsPtr(3);
    ofs.x = 0.0f;
    ofs.y = 68.0f;
    ofs.z = 28.0f;
    spd.x = 0.0f;
    spd.y = 50.0f;
    spd.z = -25.0f;
    PSMTXMultVec(p->mat, &ofs, &ofs);
    PSMTXMultVecSR(pPL->mat, &spd, &spd);
    obj = SetObj01(PL_ARC_PTR(pG->pPlayer, 0xC), PL_ARC_PTR(pG->pPlayer, 7), &ofs, &pPL->ang, &spd, 10.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->LightInfo.EnableMask = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, (int) zero, -1);
    }
    EstSet(obj, -1, 0, 0, EFF_EM10, 0x46, 0, ESP_CORE_KIND_NONE, obj, (void*) zero);
}

// Spits the poison projectile (SetObj08 from the head part 0) aimed at the player's or the partner's
// head with the poison attack parameters and effects.
void em25SetPoison(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec spd;
    Mtx m;
    Vec tgt;
    Vec rot;
    cEm* parent;
    cParts* p;
    cParts* t;
    cObj* obj;
    EmAtkInfo* atk;
    f32 d;
    f32 ang;

    parent = w->pEm_oya;
    em->partsWorldCalc();
    t = pPL->getPartsPtr(3);
    tgt = t->world;
    if (pSUB) {
        d = VEC_DIST(&parent->pos, &pPL->pos);
        if (d > VEC_DIST(&parent->pos, &pSUB->pos) +
                    3000.0f) {
            t = pSUB->getPartsPtr(3);
            tgt = t->world;
        }
    }
    SndCall(8, 0x1E, &em->getPartsPtr(0)->world, em->id, 0, em);
    atk = em25_poison_atk;
    p = em->getPartsPtr(0);
    obj = SetObj08(em, 0, 0, &p->world, &em->ang, 0x40000000, atk);
    if (parent) {
        ang = LIMIT_ANGLE(parent->ang.y + em->ang.y);
    } else {
        ang = GetXZAngle(&p->world, &tgt);
    }
    PSMTXRotRad(m, 'y', ang);
    spd.x = 0.0f;
    spd.y = 40.0f;
    spd.z = 130.0f;
    PSMTXMultVecSR(m, &spd, &spd);
    SetObj08Spd(obj, &spd, 30, 10.0f, 100.0f);
    SetObj08Est(obj, 0, 0, 0x1D, 8, 0x1D, 7, 0x1D, 9, 1);
    SetObj08Se(obj, 8, 0x1F);
    rot.x = 0.0f;
    rot.y = ang;
    rot.z = 0.0f;
    EstSet(0, -1, &p->world, &rot, EFF_EM25, 6, 0, ESP_CORE_KIND_NONE, 0, 0);
}

// 1 when the attached parasite may attack now (Atk_enable, set by P_Wait after its wait).
int cEm25::ckAtkEnable()
{
    if (EM25_WK(this)->Atk_enable == 0) {
        return 0;
    }
    return 1;
}

// Host request: the host was hit, recoil (Dm_P_Normal).
void cEm25::setDamage()
{
    setRno(2, 0, 0, 0);
}

// For the host: 1 when the partner is the nearer target (the host is more than 3000 units farther
// from the player than from her), so the parasite attack aims at Ashley.
int cEm25::ckLock()
{
    cEm* parent = EM25_WK(this)->pEm_oya;
    f32 d;

    if (parent && pSUB) {
        d = VEC_DIST(&parent->pos, &pPL->pos);
        return d > VEC_DIST(&parent->pos, &pSUB->pos) +
                       3000.0f;
    }
    return 0;
}
