// em34 module (D:/Bio4/Prog/em34.cpp): the em34 / em37 / em33 enemies in one module, selected by
// cModel::type (1 = em37, 2..3 = em33, else em34). A large enemy that turns towards its target
// (em34RouteCk: the player or the partner), walks up to it and bites (em34AtkCk).
//
// Em34Init is the module's EmInitFunc. Routines: r_no_0 0 init, 1 move (r_no_1 0 wait, 1 walk /
// turn towards the target, 2 bite), 2 damage, 3 die. Each type has its own model set (archive
// 4..8 em34, 9..0xB em37, 0xC..0xF em33), cloth chains and motion slots (em34: 0x11 idle / 0x12
// walk; em37: 0x14 idle / 0x13 walk / 0x15 bite; em33: 0x17 idle / 0x18 walk). Em34Work (em34.h):
// Be_flg bit0 route to the player valid, bit1 partner present, bit2 target is the partner, bit3
// in damage / die, bit4 the head follows the player; Go_pos / Go_dir / L_go the target of the frame.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em34.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "quake.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"


typedef void (*Em34Func)(cEm34*);

static void em34_R0_Init(cEm34* em);
static void em34_R0_Move(cEm34* em);
static void em34_R1_Wait(cEm34* em);
static void em34_R1_Walk(cEm34* em);
static void em34_R1_Atk(cEm34* em);
static void em34_R0_Damage(cEm34* em);
static void em34_R1_Dm_Normal(cEm34* em);
static void em34_R0_Die(cEm34* em);
static void em34_R1_Die_Normal(cEm34* em);


// REL entry: registers the enemy constructor.
extern "C" void _prolog()
{
    OSReport("em34 prolog Ok\n");
    EmInitFunc = Em34Init;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// EmInitFunc: placement-constructs the enemy in the cEm work.
void Em34Init(cEm* em)
{
    new (em) cEm34();
}

// Damage of the frame: consumes cEm::dmHit and sets dmType (1, 0x11 for the knife); hp loss by
// weapon class (handguns / rifles / MGs 10-11, shotguns 10 or 50-51 beyond 4 m, magnums /
// launchers / grenades 50), blood effect; hp <= 0 -> die routine (3).
void em34DmCk(cEm34* em)
{
    int dmg;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    switch (em->dmg.m_Wep) {
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
        dmg = (Rnd() & 1) + 10;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 10;
        if (em->l_pl > 16000000.0f) {
            dmg = (Rnd() & 1) + 50;
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    default:
        dmg = 50;
        break;
    }
    LifeDownSet2(em, dmg, 0, 1);
    EmDmBloodSet(em);
    if (em->hp <= 0) {
        EmSetDie(em);
        em->setRno(3, 0, 0, 0);
    }
}

Em34Func Em34_R0_move_tbl[4] = {
    em34_R0_Init,
    em34_R0_Move,
    em34_R0_Damage,
    em34_R0_Die,
};

static Em34Func Em34_R1_move_tbl[3] = {
    em34_R1_Wait,
    em34_R1_Walk,
    em34_R1_Atk,
};

static Em34Func Em34_R2_move_tbl[1] = {
    em34_R1_Dm_Normal,
};

static Em34Func Em34_R3_move_tbl[1] = {
    em34_R1_Die_Normal,
};

// Bite attack (em34AtkCk): range, type, damage, ...
static EmAtkInfo em34_atk_tbl[1] = {
    { 300.0f, PL_DM_AUTO, 9999, 0, 0xA, 0 },
};
static int em34_atk_pad = 0;

// Per-frame update (emMove): damage, target choice, the r_no_0 routine (0xFF after a failed init
// destroys the work), the neck, parts matrices, enemy / scenery collision and the type's two
// cloth chains.
void cEm34::move()
{
    Em34Work* w = EM34_WK(this);

    if (r_no_0) {
        em34DmCk(this);
    }
    w->Be_flg &= ~0x1F;
    em34RouteCk(this);
    Em34_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em34NeckMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    switch (type) {
    case 0:
    default:
        Em34ClothMove2(this, &w->Cloth2);
        Em34ClothMove1(this, &w->Cloth1);
        break;
    case 1:
        Em37HairMove(this, &w->Cloth1);
        Em37CoatMove(this, &w->Cloth2);
        break;
    case 2:
    case 3:
        Em33ClothMove(this, &w->Cloth1);
        Em33ClothMove2(this, &w->Cloth2);
        break;
    }
}

// r_no_0 == 0: creation: the type's model set (em34: body + shoulder / head / hand infos; em37:
// body + one part; em33: body + one part, be_flag bit24), the em10 foot shadows, the type's cloth
// chains, a 10 m light area, the collision cylinder (smaller for em37), hit boxes (body +
// hit[0..2]), lock-on on parts 2, effects (archive 0x10 as group 0x2B), the idle motion; then wait (1/0).
static void em34_R0_Init(cEm34* em)
{
    Em34Work* w = EM34_WK(em);
    cModelInfo* info;
    int one;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(8)) == 0) {
            pLog->err(0, 0, "em34() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(5), ARC(8));
        if (info) {
            em->addModel(info);
            w->pShoulder = info;
        }
        info = ModInfoMgr.create(ARC(6), ARC(8));
        if (info) {
            em->addModel(info);
            w->pHead = info;
        }
        info = ModInfoMgr.create(ARC(7), ARC(8));
        if (info) {
            em->addModel(info);
            w->pHand = info;
        }
        break;
    case 1:
        if (em->modelInit(ARC(9), ARC(0xB)) == 0) {
            pLog->err(0, 0, "em37() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(0xA), ARC(0xB));
        if (info) {
            em->addModel(info);
        }
        break;
    case 2:
        if (em->modelInit(ARC(0xC), ARC(0xE)) == 0) {
            pLog->err(0, 0, "em33() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(0xD), ARC(0xE));
        if (info) {
            em->addModel(info);
        }
        em->be_flag |= 0x01000000;
        break;
    case 3:
        if (em->modelInit(ARC(0xC), ARC(0xF)) == 0) {
            pLog->err(0, 0, "em33() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(0xD), ARC(0xF));
        if (info) {
            em->addModel(info);
        }
        em->be_flag |= 0x01000000;
        break;
    }
    em->pFsdTbl = &Em10_fs_tbl;
    switch (em->type) {
    case 0:
    default:
        Em34ClothSet2(em, &w->Cloth2);
        Em34ClothSet1(em, &w->Cloth1);
        break;
    case 1:
        Em37HairSet(em, &w->Cloth1);
        Em37CoatSet(em, &w->Cloth2);
        break;
    case 2:
    case 3:
        Em33ClothSet(em, &w->Cloth1, 0);
        Em33ClothSet2(em, &w->Cloth2, 0);
        break;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    switch (em->type) {
    case 0:
    default:
        em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);
        break;
    case 1:
        em->atari.init(0.0f, 0.0f, 0.0f, 300.0f, 250.0f, 250.0f, 1000.0f, 1, 0x2000, 10);
        break;
    case 2:
    case 3:
        em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);
        break;
    }
    em->State.SetLightIgnore();
    YarareInit(em, 0.0f, 0.0f, 0.0f, 400.0f, 200.0f, 1, YAT_FLAG_ON);
    one = 1;
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x18, YAT_FLAG_ON);
    em->setTarget(2, 0.0f, 0.0f, 0.0f);
    EspDataLoad((u32) ARC(0x10), EFF_EM34, 0);
    w->Neck_dir_y = 0.0f;
    w->Be_flg = 0;
    em->setRno(one, 0, 0, 0);
    switch (em->type) {
    case 0:
    default:
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 0, 5, 0);
        break;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 0, 5, 0);
        break;
    case 2:
    case 3:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 5, 0);
        break;
    }
    MotionMove(em, 0);
    em34_R0_Move(em);
}

// r_no_0 == 1: dispatches the r_no_1 state.
static void em34_R0_Move(cEm34* em)
{
    Em34_R1_move_tbl[em->r_no_1](em);
}

// r_no_1 == 0: the type's idle motion (30-frame blend), head following the player; a registered
// death (dmg upper bits) -> walk (1).
static void em34_R1_Wait(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x14), 0, 30, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 30, 5, 0);
            break;
        }
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->dmg.isDamage()) {
            em->setRno(1, 1, 0, 0);
        }
        break;
    }
}

// r_no_1 == 1: the type's walk motion while turning towards the target (PI/64 per frame); em37
// below 500 hp bites (2) within 1 m, the others go back to wait within 2 m of the player.
static void em34_R1_Walk(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x12), 0, 10, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x13), 0, 10, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x18), 0, 10, 5, 0);
            break;
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, PI / 64.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (em->type == 1 && em->hp < 500) {
            if (em->l_pl < 1000000.0f) {
                em->setRno(1, 2, 0, 0);
            }
        } else {
            if (em->l_pl < 4000000.0f) {
                em->setRno(1, 0, 0, 0);
            }
        }
        break;
    }
}

// r_no_1 == 2: the bite: em37's attack motion 0x15/0x16 (the other types just play their idle)
// turning towards the target (PI/32 per frame); the motion's SE flag bit0 marks the frames the
// jaw (parts 10) hits (em34AtkCk); back to wait at the end.
static void em34_R1_Atk(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 10, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 10, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x15), ARC(0x16), 10, 1, 0);
            break;
        }
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->setRno(1, 0, 0, 0);
        } else if (em->Motion.Seq_old.Free & 1) {
            em34AtkCk(em, 0, 0xA);
        }
        break;
    }
}

// r_no_0 == 2: the damage routine (Be_flg bit3), r_no_1 state table.
static void em34_R0_Damage(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->Be_flg |= 8;
    Em34_R2_move_tbl[em->r_no_1](em);
}

// Damage state 0: the flinch (the type's idle motion), then back to walk with r_no_3 = 10.
static void em34_R1_Dm_Normal(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 1, 0);
            break;
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 1, 0, 10);
        }
        break;
    }
}

// r_no_0 == 3: the die routine (Be_flg bit3), r_no_1 state table.
static void em34_R0_Die(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->Be_flg |= 8;
    Em34_R3_move_tbl[em->r_no_1](em);
}

// Die state 0: the death motion (the type's idle); at its end the battle / active / dog status
// bits and the collision are cleared, then after 30 frames the model fades out (invisible_factor
// -0.02 per frame) and is hidden.
static void em34_R1_Die_Normal(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    switch (em->r_no_2) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 1, 0);
            break;
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->clearStatus(0);
            em->clearStatus(EM_STATUS_ACTIVE);
            em->clearStatus(EM_STATUS_DOGCK);
            em->clearStatus(EM_STATUS_DOGATK);
            em->atari.m_flag &= ~0x300;
            em->r_no_2++;
        }
        break;
    case 2:
        w->Timer = 30;
        em->r_no_2++;
    case 3:
        if (w->Timer) {
            w->Timer--;
        } else {
            em->invisible_factor -= 0.02f;
            if (em->invisible_factor < 0.0f) {
                em->invisible_factor = 0.0f;
                em->be_flag &= ~2;
            }
        }
        break;
    }
}

// Target choice of the frame (alive only): the route point towards the player (Be_flg bit0 when
// reachable) and its angle become Go_pos / Go_dir / Go_rot / L_go with pEm = the player; with a
// partner present (bit1) and the player unreachable or farther than the partner (l_sub) the
// partner's route data is the target (bit2). During init the angles are zeroed and the player
// distance forced far.
void em34RouteCk(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (RouteCkToPos(em, &pPL->pos, &w->Pl_pos, 0, 0)) {
        w->Be_flg |= 1;
    }
    w->Pl_dir = Muku(&em->pos, &w->Pl_pos, em->ang.y, PI);
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
    if (w->Be_flg & 2) {
        if (!(w->Be_flg & 1) || em->l_pl > em->l_sub) {
            w->Go_pos = w->Sub_pos;
            w->Go_dir = w->Sub_dir;
            w->Go_rot = w->Sub_rot;
            w->L_go = em->l_sub;
            w->pEm = pSUB;
            w->Be_flg |= 4;
        }
    }
}

// Head tracking: while Be_flg bit4 the neck yaw eases (0.9/0.1) towards the player within +-60
// degrees, else back to 0; applied as the additional rotation of parts 3.
void em34NeckMove(cEm34* em)
{
    Em34Work* w = EM34_WK(em);
    cParts* p;
    Vec v;

    p = em->getPartsPtr(4);
    {
        cParts* h = pPL->getPartsPtr(4);

        v.x = 0.0f;
        v.y = 250.0f;
        v.z = 0.0f;
        PSMTXMultVec(h->mat, &v, &v);
    }
    if (w->Be_flg & 0x10) {
        w->Neck_dir_y = w->Neck_dir_y * 0.9f + Muku(&em->pos, &pPL->pos, em->ang.y, 1.0471976f) * 0.1f;
    } else {
        w->Neck_dir_y = w->Neck_dir_y * 0.9f;
    }
    p = em->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->inv_offset.x = 0.0f;
    ((cParts*) p)->inv_offset.y = w->Neck_dir_y;
    ((cParts*) p)->inv_offset.z = 0.0f;
}

// Bite hit check: once per attack (Atk_ck), the sweep of parts `parts` from its last position
// against the player (hit bit0) / partner (bit1) with em34_atk_tbl[no] (300 range, damage type 8):
// blood on the victim, a quake and pad vibration. Returns 1 on a hit.
int em34AtkCk(cEm34* em, int no, int parts)
{
    Em34Work* w = EM34_WK(em);

    if (w->Atk_ck) {
        return 0;
    }
    {
        EmAtkInfo* atk = &em34_atk_tbl[no];
        cParts* p = em->getPartsPtr(parts);
        int hit = EmAtkHitCk(atk, &p->world, &p->world_old, 0);

        if (hit) {
            if (hit & 1) {
                EmPlBloodSet(em, &p->world, 1, 0xFF, 0xFF);
                w->Atk_ck = 1;
            }
            if (hit & 2) {
                EmSubBloodSet(em, &p->world, 1, 0xFF, 0xFF);
                w->Atk_ck = 1;
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            return 1;
        }
    }
    return 0;
}
