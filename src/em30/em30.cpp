// em30 module (D:/Bio4/Prog/em30.cpp): a large stationary enemy with two cloth chains and three
// head objects (obj16) carrying four parasites each; it turns towards the player or the partner
// (em30RouteCk chooses the target) and dies after one damage routine.
//
// Em30Init is the module's EmInitFunc. Routines: r_no_0 0 init, 1 move (r_no_1 0 wait, 1 turn
// towards the target), 2 damage (r_no_1 0), 3 die (r_no_1 0). em30DmCk converts the registered
// hit (cEm::dmHit / dmWep) into hp loss per weapon class; the head objects and their parasites are
// cObj16 objects hung on the model (em30SetParasite), created only when the list flag is negative.
// Em30Work (em30.h): flags bit0 route to the player valid, bit1 partner present, bit2 target is
// the partner, bit3 in damage / die, bit4 the head follows the player; the neck yaw neckAng is
// applied to parts 3.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em30.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "obj16.h"
#include "esp.h"
#include "motion.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "player.h"
#include "pl_npc.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"


typedef void (*Em30Func)(cEm30*);

static void em30_R0_Init(cEm30* em);
static void em30_R0_Move(cEm30* em);
static void em30_R1_Wait(cEm30* em);
static void em30_R1_Walk(cEm30* em);
static void em30_R0_Damage(cEm30* em);
static void em30_R1_Dm_Normal(cEm30* em);
static void em30_R0_Die(cEm30* em);
static void em30_R1_Die_Normal(cEm30* em);


// REL entry: registers the enemy constructor.
extern "C" void _prolog()
{
    OSReport("em30 prolog Ok\n");
    EmInitFunc = Em30Init;
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
void Em30Init(cEm* em)
{
    new (em) cEm30();
}

// Damage of the frame: consumes cEm::dmHit and sets dmType (1, 0x11 for the knife); hp loss by
// weapon class (handguns / rifles / MGs 10-11, shotguns 10 or 50-51 when farther than 4 m,
// magnums / launchers / grenades 50), blood effect; hp <= 0 -> die routine (3). The return value
// is meaningless (a folded `dmWep == 0x21` test).
int em30DmCk(cEm30* em)
{
    int dmg;

    if (em->dmg.m_Flag) {
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
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x2B:
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
        case 0x2C:
        case 0x2D:
        default:
            dmg = 50;
            break;
        }
        LifeDownSet2(em, dmg, 0, 1);
        EmDmBloodSet(em);
        if (em->hp <= 0) {
            EmSetDie(em);
            em->setRno(3, 0, 0, 0);
        } else {
            // `lbz r3, dmWep; cmpwi r3, 0x21` with no branch (r3 = the return value): the branch
            // around an empty taken arm is deleted by jump2 after reload. Written as `== 0x21`, cse
            // folds the returned value to `li r3, 0x21` on the taken path (record_jump_equiv); the
            // xor form hides the equivalence from cse and combine folds the compare back.
            if ((em->dmg.m_Wep ^ 0x21) == 0) {
                return em->dmg.m_Wep;
            }
        }
    }
}

Em30Func Em30_R0_move_tbl[4] = {
    em30_R0_Init,
    em30_R0_Move,
    em30_R0_Damage,
    em30_R0_Die,
};

static Em30Func Em30_R1_move_tbl[2] = {
    em30_R1_Wait,
    em30_R1_Walk,
};

static Em30Func Em30_R2_move_tbl[1] = {
    em30_R1_Dm_Normal,
};

static Em30Func Em30_R3_move_tbl[1] = {
    em30_R1_Die_Normal,
};

// Per-frame update (emMove): damage, the target choice (em30RouteCk), the r_no_0 routine (0xFF
// after a failed init destroys the work), the neck, parts matrices, enemy / scenery collision and
// the two cloth chains.
void cEm30::move()
{
    Em30Work* w = EM30_WK(this);

    if (r_no_0) {
        em30DmCk(this);
    }
    w->flags &= ~0x1F;
    em30RouteCk(this);
    Em30_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em30NeckMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    Em30ClothMove1(this, &w->cloth1);
    Em30ClothMove2(this, &w->cloth2);
}

// r_no_0 == 0: creation: the body (archive 4/5) plus five extra models (6, 8 = pInfo0 / pInfo1 the
// hideable ones, 9, 0xA or 0xB by type, 0xC, 0xD), the em10 foot shadows, the two cloth chains,
// a 10 m light area, a 0x2000-attribute collision cylinder, hit boxes (body + hit[0..2]), lock-on
// on parts 2, effects (archive 0xE as group 0x28); with the list flag negative the two extra
// models are hidden and the three heads with their parasites are created. Then wait (1/0).
static void em30_R0_Init(cEm30* em)
{
    Em30Work* w = EM30_WK(em);
    cModelInfo* info;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em30() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    w->pInfo0 = ModInfoMgr.create(ARC(6), ARC(7));
    if (w->pInfo0) {
        em->addModel(w->pInfo0);
    }
    w->pInfo1 = ModInfoMgr.create(ARC(8), ARC(7));
    if (w->pInfo1) {
        em->addModel(w->pInfo1);
    }
    info = ModInfoMgr.create(ARC(9), ARC(5));
    if (info) {
        em->addModel(info);
    }
    if (em->type == 0) {
        info = ModInfoMgr.create(ARC(0xA), ARC(5));
    } else {
        info = ModInfoMgr.create(ARC(0xB), ARC(5));
    }
    if (info) {
        em->addModel(info);
    }
    info = ModInfoMgr.create(ARC(0xC), ARC(5));
    if (info) {
        em->addModel(info);
    }
    info = ModInfoMgr.create(ARC(0xD), ARC(5));
    if (info) {
        em->addModel(info);
    }
    em->pFsdTbl = &Em10_fs_tbl;
    Em30ClothSet1(em, &w->cloth1);
    Em30ClothSet2(em, &w->cloth2);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);
    em->litArea.on(1);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 400.0f, 200.0f, 1, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[2], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x18, YAT_FLAG_ON);
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(0xE), EFF_EM30, 0);
    w->neckAng = 0.0f;
    w->flags = 0;
    if (em->flag & 0x80000000) {
        if (w->pInfo0) {
            w->pInfo0->be_flag &= ~8;
        }
        if (w->pInfo1) {
            w->pInfo1->be_flag &= ~8;
        }
        em30SetParasite(em, 0);
        em30SetParasite(em, 1);
        em30SetParasite(em, 2);
    }
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 1, 0);
    MotionMove(em, 0);
    em30_R0_Move(em);
}

// r_no_0 == 1: dispatches the r_no_1 state.
static void em30_R0_Move(cEm30* em)
{
    Em30_R1_move_tbl[em->r_no_1](em);
}

// r_no_1 == 0: the idle motion 0xF (30-frame blend), head following the player; a registered
// death (dmg upper bits) -> state 1.
static void em30_R1_Wait(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (EmDeadCk(em)) {
            em->setRno(1, 1, 0, 0);
        }
        break;
    }
}

// r_no_1 == 1: the "walk" motion 0x10 while turning towards the target (PI/64 per frame); back
// to wait when the player is within 2 m.
static void em30_R1_Walk(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x10), 0, 10, 5, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 64.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (em->l_pl < 4000000.0f) {
            em->setRno(1, 0, 0, 0);
        }
        break;
    }
}

// r_no_0 == 2: the damage routine (flags bit3), r_no_1 state table.
static void em30_R0_Damage(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 8;
    Em30_R2_move_tbl[em->r_no_1](em);
}

// Damage state 0: the flinch (motion 0xF), then back to move / state 1 with r_no_3 = 10.
static void em30_R1_Dm_Normal(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setRno(1, 1, 0, 10);
        }
        break;
    }
}

// r_no_0 == 3: the die routine (flags bit3), r_no_1 state table.
static void em30_R0_Die(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 8;
    Em30_R3_move_tbl[em->r_no_1](em);
}

// Die state 0: the death motion 0xF; at its end the battle / active / dog status bits and the
// collision are cleared, then after 30 frames the model fades out (invisible_factor -0.02 per
// frame) and is hidden.
static void em30_R1_Die_Normal(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 3, 1, 0);
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
        w->timer = 30;
        em->r_no_2++;
    case 3:
        if (w->timer) {
            w->timer--;
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

// Target choice of the frame (alive only): the route point towards the player (flags bit0 when
// reachable) and its angle become the target; with a partner present (flags bit1) and the player
// unreachable or farther than the partner (l_sub) the partner's route data is the target (flags
// bit2). During init the angles are zeroed and the player distance forced far.
void em30RouteCk(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    if (em->hp <= 0) {
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
    w->flags &= ~4;
    if (w->flags & 2) {
        if (!(w->flags & 1) || em->l_pl > em->l_sub) {
            w->targetPos = w->subRoutePos;
            w->targetAng = w->subAng;
            w->targetAngAbs = w->subAngAbs;
            w->targetDist = em->l_sub;
            w->pTarget = pSUB;
            w->flags |= 4;
        }
    }
}

// Head tracking: while flags bit4 the neck yaw eases (0.9/0.1) towards the player within +-60
// degrees, else back to 0; applied as the additional rotation of parts 3.
void em30NeckMove(cEm30* em)
{
    Em30Work* w = EM30_WK(em);
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
    if (w->flags & 0x10) {
        w->neckAng = w->neckAng * 0.9f + Muku(&em->pos, &pPL->pos, em->ang.y, 1.0471976f) * 0.1f;
    } else {
        w->neckAng = w->neckAng * 0.9f;
    }
    p = em->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->inv_offset.x = 0.0f;
    ((cParts*) p)->inv_offset.y = w->neckAng;
    ((cParts*) p)->inv_offset.z = 0.0f;
}

// Creates head `no` (0..2, spread 120 degrees apart on parts 3) as a cObj16 (model 0x15/0x16)
// with its motion set (0x17..0x1F) and the player-damage motion 0x20, and four parasites (model
// 0x12/0x13, motion 0x14) on the head's parts 0x16..0x19, their motions offset by a quarter
// period each.
void em30SetParasite(cEm30* em, int no)
{
    Em30Work* w = EM30_WK(em);
    Vec pos;
    Vec rot;
    cObj* obj;
    int frame = (((MotionData*) ARC(0x14))->maxFrame & 0x3FFF) / 4;
    void* bin = ARC(0x15);
    void* tpl = ARC(0x16);
    void* m0 = ARC(0x17);
    void* m1 = ARC(0x18);
    void* m2 = ARC(0x1C);
    void* m3 = ARC(0x1E);
    void* m4 = ARC(0x1F);
    void* m7 = ARC(0x19);
    void* m8 = ARC(0x1D);
    void* m9 = ARC(0x1A);
    void* m10 = ARC(0x1B);

    switch ((u32) no) {
    case 0:
    default:
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        break;
    case 1:
        pos.x = 50.0f;
        pos.y = 0.0f;
        pos.z = -50.0f;
        rot.x = 0.0f;
        rot.y = 2.0943952f;
        rot.z = 0.0f;
        break;
    case 2:
        pos.x = -50.0f;
        pos.y = 0.0f;
        pos.z = -50.0f;
        rot.x = 0.0f;
        rot.y = -2.0943952f;
        rot.z = 0.0f;
        break;
    }
    w->pHead[no] = SetObj16(bin, tpl, em, em, 3, 0xE, &pos, &rot);
    if (w->pHead[no]) {
        ((cObj16*) w->pHead[no])->setMotData(m0, m1, m2, m3, m4, m4, m4, m7, m8, m9, m10);
        ((cObj16*) w->pHead[no])->setMotData(m0, m1, m2, m3, m4, m4, m4, m7, m8, m9, m10);
        ((cObj16*) w->pHead[no])->setPlDmgMot(ARC(0x20), 0);
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = -0.6632251f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x16, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, 0);
        w->para[no].p[0] = obj;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.61086524f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x17, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, frame);
        w->para[no].p[1] = obj;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = -0.5235988f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x18, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, frame * 2);
        w->para[no].p[2] = obj;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x19, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, frame * 3);
        w->para[no].p[3] = obj;
    }
}
