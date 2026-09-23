#include "obj1c.h"
// em2f module (D:/Bio4/Prog/em2f.cpp): the lake monster. It swims along the room's EMI route points
// (em2fSetNextRoute / em2fChangeRoute), dives and surfaces (em2fWaterEffSet), rams the boat from
// below (em2f_R1_RisingDragon), swallows the player (em2f_R1_Packman), hides until the harpoons hit
// (em2f_R1_HideMode), drags the player under when it dies (em2f_R1_Critical with its camera) and
// breaks the floating islands it runs into (em2fIslandCrashCk); its tentacles are cObj16 objects.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em2f.h"
#include "obj16.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "embarrel.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "game.h"
#include "dbmodule.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em2f,52,4");


typedef void (*Em2fFunc)(cEm2f*);

static void em2f_R0_Init(cEm2f* em);
static void em2f_R0_Move(cEm2f* em);
static void em2f_R1_Wait(cEm2f* em);
static void em2f_R1_Walk(cEm2f* em);
static void em2f_R1_SwimWait(cEm2f* em);
static void em2f_R1_Swim(cEm2f* em);
static void em2f_R1_SwimTurn90(cEm2f* em);
static void em2f_R1_SwimTurn180(cEm2f* em);
static void em2f_R1_SwimTurn180Atk(cEm2f* em);
static void em2f_R1_RisingDragon(cEm2f* em);
static void em2f_R1_Packman(cEm2f* em);
static void em2f_R1_HideMode(cEm2f* em);
static void em2f_R1_Critical(cEm2f* em);
static void em2f_R0_Damage(cEm2f* em);
static void em2f_R1_Dm_Normal(cEm2f* em);
static void em2f_R0_Die(cEm2f* em);
static void em2f_R1_Die_Normal(cEm2f* em);





// The effects the monster owns are removed before it is moved somewhere else.
static inline void em2fEffectDelete(cEm2f* em, Em2fWork* w)
{
    EffectEspDelete(0, w->espKind, em, 0);
    EffectEspgenDelete(0, w->espKind, em);
    EffectEfmDelete(0, w->espKind, em);
}

// Module entry (SN loader): registers Em2fInit as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em2f prolog Ok\n");
    EmInitFunc = Em2fInit;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm2f class in the manager's work.
void Em2fInit(cEm* em)
{
    new (em) cEm2f();
}

// Per-frame damage check (cEm2f::move): the monster only takes real damage from the harpoons: a
// hit sets flag bit6 (damaged) and costs 1 hp for guns / knife, 100 for the harpoon kind 0x15, 1 / 3
// (far) for shotguns, 1000 for explosives / mine / grenades; at hp 0 (of 1000) it is marked dead
// (EmSetDie, inactive) and the routines run the death.
void em2fDmCk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int dmg;

    w->flags &= ~0x40;
    if (em->dmg.m_Flag == 0) {
        return;
    }
    w->flags |= 0x40;
    em->dmg.m_Flag = 0;
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = 1;
        break;
    case 0x15:
        dmg = 100;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 1;
        if (em->l_pl > 36000000.0f) {
            dmg = 3;
        }
        break;
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2A:
    case 0x2C:
    default:
        dmg = 1000;
        break;
    }
    LifeDownSet2(em, dmg, 0, 0);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCnt(em);
        em->clearStatus(EM_STATUS_ACTIVE);
    }
}

Em2fFunc Em2f_R0_move_tbl[4] = {
    em2f_R0_Init,
    em2f_R0_Move,
    em2f_R0_Damage,
    em2f_R0_Die,
};

static Em2fFunc Em2f_R1_move_tbl[11] = {
    em2f_R1_Wait,
    em2f_R1_Walk,
    em2f_R1_SwimWait,
    em2f_R1_Swim,
    em2f_R1_SwimTurn90,
    em2f_R1_SwimTurn180,
    em2f_R1_SwimTurn180Atk,
    em2f_R1_RisingDragon,
    em2f_R1_Packman,
    em2f_R1_HideMode,
    em2f_R1_Critical,
};

static Em2fFunc Em2f_R2_move_tbl[1] = {
    em2f_R1_Dm_Normal,
};

static Em2fFunc Em2f_R3_move_tbl[1] = {
    em2f_R1_Die_Normal,
};

// Route points used when the room has no EMI data.
static Vec em2f_route_tbl[4] = {
    { -25230.0f, -2000.0f, 98080.0f },
    { 11390.0f, -2000.0f, 113600.0f },
    { 49290.0f, -2000.0f, 18650.0f },
    { 95900.0f, -2000.0f, 18650.0f },
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em2f_flip_tbl[80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xD, 0xE, 0xF, 0xA, 0xB, 0xC, 0x10, 0x14, 0x15, 0x16, 0x11, 0x12, 0x13, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
    0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};

// Where the monster surfaces for the death scene.
static Vec em2f_die_pos = { -46800.0f, -734.0f, 22157.0f };

// The camera em2fCriCamMove installs (the drowning cut): explicitly zero-initialised so it stays
// in .data.
static Camera em2f_cri_cam = { 0 };

// Per-frame update: damage check, waitTimer countdown, clears the per-frame flags, the route check,
// the R0 table (Init / Move / Damage / Die), then collision, scenario check, the island crash test,
// the swim SEs (surfaced motion frames) and the tentacle objects (em2fTentacleMove).
void cEm2f::move()
{
    Em2fWork* w = EM2F_WK(this);

    if (r_no_0) {
        em2fDmCk(this);
    }
    if (w->waitTimer) {
        w->waitTimer--;
    }
    w->flags &= ~0x58F;
    em2fRouteCk(this);
    Em2f_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    em2fIslandCrashCk(this);
    if (Motion.Seq_old.Free & 0x10) {
        if (w->seTimer1) {
            w->seTimer1--;
        } else {
            w->seTimer1 = 90;
            w->sndId1 = SndCall(8, 0xB, &pos, id, 0, this);
        }
        if (w->seTimer2) {
            w->seTimer2--;
        } else {
            w->seTimer2 = 57;
            w->sndId2 = SndCall(8, 0xC, &pos, id, 0, this);
        }
        if (w->flags & 0x400) {
            if (w->seTimer3) {
                w->seTimer3--;
            } else {
                w->seTimer3 = 45;
                SndCall(8, 0x10, &pos, id, 0, this);
            }
        }
    } else {
        SndStop(w->sndId1, 0);
        SndStop(w->sndId2, 0);
    }
    em2fTentacleMove(this);
}

// R0 == 0: creation. Builds the model of type 0 / 1, hp 1000, waterY = the creation height, the hit
// boxes (hit[0..6]), atkCnt 1..3, the room's ctrl12, the first route point (em2fSetNextRoute), and the
// start routine by cEm::set: 0 Wait (a land-bound test enemy), 1 SwimWait (waiting for the boss
// fight flag), 2 Critical (the drowning cut scene, IK off).
static void em2f_R0_Init(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int zero;
    u32 i;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em2b() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(6), ARC(7)) == 0) {
            pLog->err(0, 0, "em2b() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 50000.0f, 50000.0f, 50000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    AtariInit(&em->atari, 0.0f, 0.0f, 0.0f, 700.0f, 3500.0f, 3500.0f, 2000.0f, 1, 2, 0);   // COMPILER-DIFF: #1
    YarareInit(em, 0.0f, -200.0f, 0.0f, 1200.0f, 1000.0f, 9, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], 0.0f, -200.0f, 0.0f, 1200.0f, 1400.0f, 2, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    zero = 0;
    YarareAdd(em, &w->hit[1], 0.0f, -100.0f, 0.0f, 1400.0f, 1400.0f, 4, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[2], 0.0f, -200.0f, 0.0f, 1200.0f, 700.0f, 6, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 700.0f, 1400.0f, 0x19, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 900.0f, 1000.0f, 0x1B, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 700.0f, 600.0f, 0x1C, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[6], 0.0f, 0.0f, -600.0f, 500.0f, 600.0f, 0x1D, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    em->hp = 1000;
    em->Motion.flip = em2f_flip_tbl;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(8), EFF_EM2F, 0);
    w->flags = zero;
    w->x580 = em->pos.y;
    w->x584 = 0.0f;
    w->x5C9 = zero;
    w->x5CB = zero;
    w->effTimer1 = 30;
    w->effTimer2 = 3;
    w->effTimer3 = 3;
    w->atkCnt = Rnd() % 3 + 1;
    // the zero's dying store (pBoat, written last) is issued first, the rest in source order
    w->seTimer1 = zero;
    w->seTimer2 = zero;
    w->seTimer3 = zero;
    w->sndId1 = zero;
    w->sndId2 = zero;
    w->pBoat = (cEm*) zero;
    w->rndFlag = Rnd() & 1;
    for (i = 0; i < 6; i++) {
        w->pTentacle[i] = 0;
    }
    w->pCtrl12 = GetCtrlCtrl12();
    w->espKind = EspPullCoreKind();
    w->waterY = em->pos.y;
    w->routeIdx = -1;
    w->nextRouteType = em2fSetNextRoute(em);
    w->routeType = w->nextRouteType;
    switch (em->set) {
    case 0:
    default:
        em->setStatus(EM_STATUS_ACTIVE);
        // plain byte stores (em30 rule): the int inline's SI zero would take r9 and reload_cse
        // would delete MotionSetCore's `li r9, 0`
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x401, 0);
        MotionMove(em, 0);
        break;
    case 1:
        em->atari.m_flag &= ~0x100;
        em->setStatus(EM_STATUS_ACTIVE);
        em->scale.x = 2.0f;
        em->scale.y = 2.0f;
        em->scale.z = 2.0f;
        EmRoutineSet(em, 1, 2, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x401, 0);
        MotionMove(em, 0);
        break;
    case 2:
        em->atari.m_flag &= ~0x100;
        em->scale.x = 2.0f;
        em->scale.y = 2.0f;
        em->scale.z = 2.0f;
        em->setStatus(EM_STATUS_IK_OFF);
        em->atari.m_flag &= ~0x300;
        em->atari.m_flag |= 8;
        EmRoutineSet(em, 1, 0xA, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x401, 0);
        MotionMove(em, 0);
        break;
    }
    em2f_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em2f_R1_move_tbl) and mirrors the motion's event bits into cEm::flag
// bits 2..4 for the room / boat code.
static void em2f_R0_Move(cEm2f* em)
{
    em->flag &= ~0x1FC;
    Em2f_R1_move_tbl[em->r_no_1](em);
    if (em->Motion.Seq_old.Free & 1) {
        em->flag |= 4;
    }
    if (em->Motion.Seq_old.Free & 0x80) {
        em->flag |= 8;
    }
    if (em->Motion.Seq_old.Free & 8) {
        em->flag |= 0x10;
    }
}

// R1 == 0 Wait: the idle motion (ARC 0xA) on land, waiting for the player.
static void em2f_R1_Wait(cEm2f* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 10, 0x405, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// R1 == 1 Walk: the land test walk (ARC 9) towards the target, back to Wait within 1000 units.
static void em2f_R1_Walk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(9), 0, 10, 0x405, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (em->l_pl < 1000000.0f) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

// R1 == 2 SwimWait: invisible under the lake until the room starts the boss fight (Status_flg[1]
// bit21), then Swim (3).
static void em2f_R1_SwimWait(cEm2f* em)
{
    // single use in another block: update_equiv_regs moves the li next to the stb (short qty, r0)
    int one = 1;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 1, 0);
        MotionMove(em, 0);
        em->be_flag &= ~2;
        em->r_no_2++;
    case 1:
        if (StaFlagChk(pG, STA_PL_BOAT)) {
            em->be_flag |= 2;
            EmRoutineSet(em, one, 3, 0, 0);
        }
        break;
    }
}

// R1 == 3 Swim: follows the EMI route points (nextPos, turning PI/200 per frame, diving / surfacing
// by `dive` towards waterY, mouth open flag bit5 mirroring the motion); at a point takes the next one
// (em2fSetNextRoute) and by its sub type turns (SwimTurn90 4 / 180 5), attacks (SwimTurn180Atk 6) or
// hides (HideMode 9); em2fChangeRoute re-routes when the point lies far behind.
static void em2f_R1_Swim(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;
    f32 d;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->r_no_2) {
    case 0:
        em->atari.m_flag &= ~0x100;
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x1D), 30, flip, 0);
        w->waitTimer = Rnd() % 90 + 90;
        w->x424 = 0;
        w->x420 = 10;
        w->x41C = 0.0f;
        if (w->flags & 0x10) {
            w->dive = w->waterY - 5000.0f - em->pos.y;
        } else {
            w->dive = w->waterY - em->pos.y;
        }
        w->timer = 0;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->nextPos, em->ang.y, PI / 200.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (em->Motion.Seq_old.Free & 0x40) {
            w->flags |= 0x20;
        } else {
            w->flags &= ~0x20;
        }
        {
            f32 t = w->dive * 0.05f;
            em->pos.y += t;
            w->dive -= t;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xE), ARC(0x1F), 30, flip, 0);
        EstSet(em, -1, 0, 0, EFF_EM2F, 0xC, 0, w->espKind, em, 0);
        w->timer = 80;
        em->r_no_2++;
        goto swim;  // the target's case 2 runs the swim step too (tails cross-jumped into case 4's)
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x1E), 30, flip, 0);
        EstSet(em, -1, 0, 0, EFF_EM2F, 4, 0, w->espKind, em, 0);
        w->timer = 80;
        em->r_no_2++;
    case 3:
    case 5:
    swim:
        em->ang.y += Muku(&em->pos, &w->nextPos, em->ang.y, PI / 200.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
    em2fWaterEffSet(em);
    if (em2fRisingDragonCk(em)) {
        return;
    }
    if (w->timer) {
        w->timer--;
    }
    {
        cModel* p = em->getPartsPtr(8);

        d = (p->world.x - w->nextPos.x) * (p->world.x - w->nextPos.x)
            + (p->world.z - w->nextPos.z) * (p->world.z - w->nextPos.z);
        if (d < 100000000.0f) {
            w->routeType = w->nextRouteType;
            w->nextRouteType = em2fSetNextRoute(em);
            d = (em->pos.x - w->nextPos.x) * (em->pos.x - w->nextPos.x) + (em->pos.z - w->nextPos.z) * (em->pos.z - w->nextPos.z);
            if (w->routeType == 2) {
                if (w->timer) {
                    return;
                }
                if (w->atkCnt) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 9, 0, 0);
                }
                return;
            }
        }
        if (w->timer) {
            return;
        }
        {
            f32 ang = fabsf(Muku(&p->world, &w->nextPos, em->ang.y, PI));

            if (ang > 1.0471976f && d < 1600000000.0f) {
                em2fChangeRoute(em);
                return;
            }
            if (ang > 2.3561945f) {
                EmRoutineSet(em, 1, 5, 0, 0);
                return;
            }
            if (ang > 1.0471976f) {
                EmRoutineSet(em, 1, 4, 0, 0);
                return;
            }
        }
        if (w->flags & 0x10) {
            if (w->routeType == 0) {
                w->flags &= ~0x10;
                em->r_no_2 = 4;
            }
        } else {
            if (w->routeType == 1) {
                w->flags |= 0x10;
                em->r_no_2 = 2;
            }
        }
    }
}

// R1 == 4 SwimTurn90: banks 90 deg towards nextPos (mirrored by side), then Swim (3).
static void em2f_R1_SwimTurn90(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->r_no_2) {
    case 0:
        if (Muku(&em->pos, &w->nextPos, em->ang.y, PI) < 0.0f) {
            if (w->flags & 0x20) {
                MotionSetCore(em, MOTION(em), ARC(0x10), ARC(0x21), 30, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0xF), ARC(0x20), 30, flip, 0);
            }
            if (!(w->flags & 0x10)) {
                EstSet(em, -1, 0, 0, EFF_EM2F, 1, 0, w->espKind, em, 0);
            }
        } else {
            if (w->flags & 0x20) {
                MotionSetCore(em, MOTION(em), ARC(0xF), ARC(0x20), 30, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x10), ARC(0x21), 30, flip, 0);
            }
            if (!(w->flags & 0x10)) {
                EstSet(em, -1, 0, 0, EFF_EM2F, 0xD, 0, w->espKind, em, 0);
            }
        }
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 2) {
            em->ang.y += Muku(&em->pos, &w->nextPos, em->ang.y, PI / 200.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em2fWaterEffSet(em);
    em2fRisingDragonCk(em);
}

// R1 == 5 SwimTurn180: turns around diving or surfacing (flag bit4), then Swim (3).
static void em2f_R1_SwimTurn180(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x10) {
            MotionSetCore(em, MOTION(em), ARC(0x1A), ARC(0x2B), 30, flip, 0);
            w->flags &= ~0x10;
            EstSet(em, -1, 0, 0, EFF_EM2F, 0x12, 0, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x2A), 30, flip, 0);
            w->flags |= 0x10;
            EstSet(em, -1, 0, 0, EFF_EM2F, 0x11, 0, w->espKind, em, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em2fWaterEffSet(em);
    em2fRisingDragonCk(em);
}

// R1 == 6 SwimTurn180Atk: the turn that starts an attack: with attacks left (atkCnt) and the fight on
// it goes for the boat (em2fRisingDragonCk -> RisingDragon 7 / Packman 8), else back to Swim (3).
static void em2f_R1_SwimTurn180Atk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->r_no_2) {
    case 0:
        if (StaFlagChk(pG, STA_PL_BOAT) && w->atkCnt) {
            w->atkCnt--;
        }
        if (w->flags & 0x10) {
            MotionSetCore(em, MOTION(em), ARC(0x18), ARC(0x29), 30, flip, 0);
            EstSet(em, -1, 0, 0, EFF_EM2F, 0x10, 0, w->espKind, em, 0);
        } else if (Rnd() & 3) {
            MotionSetCore(em, MOTION(em), ARC(0x16), ARC(0x27), 30, flip, 0);
            EstSet(em, -1, 0, 0, EFF_EM2F, 0xE, 0, w->espKind, em, 0);
        } else if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x24), 30, flip, 0);
            EstSet(em, -1, 0, 0, EFF_EM2F, 9, 0, w->espKind, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x14), ARC(0x25), 30, flip, 0);
            EstSet(em, -1, 0, 0, EFF_EM2F, 8, 0, w->espKind, em, 0);
        }
        w->flags &= ~0x10;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            w->flags &= ~0x10;
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em2fWaterEffSet(em);
    em2fRisingDragonCk(em);
}

// R1 == 7 RisingDragon: dives (em2fSetPosRisingD beside the boat), rushes up under it and rams it
// from below (the boat event flag Status_flg[1] bit22: the player is thrown into the water, his
// routine 0xF/9), the BGM restarts, then em2fChangeRoute and Swim (3).
static void em2f_R1_RisingDragon(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int r;
    // `hide` is set again after the switch: its last mention lies beyond the case-1 ebb, so cse2
    // keeps it (not the timer register, whose last use is the decrement) as the canonical zero of
    // the routine stores. `one` (single use in another block) is moved next to its stb by
    // update_equiv_regs and takes r0.
    u32 hide;
    int one = 1;

    em->flag |= 0x80;
    switch (em->r_no_2) {
    case 0:
        em2fSetPosBetweenBoat(em);
        w->flags &= ~0x20;
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x1D), 0, 5, 0);
        w->timer = 149;
        w->timer2 = 15;
        w->risingOk = 0;
        w->flags |= 0x10;
        em->r_no_2++;
    case 1:
        em->flag |= 0x108;
        MotionMove(em, 0);
        if (w->timer == 0) {
            hide = StaFlagChk(pG, STA_PL_SWIM);
            if (hide == 0) {
                EmRoutineSet(em, one, 3, 0, 0);
                break;
            }
            em->r_no_2++;
            break;
        }
        do { w->timer--; } while (0);  // LOOP_END barrier: the timer2 load stays below the store
        if (w->timer2) {
            w->timer2--;
            if (w->timer2 == 0) {
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
            }
        }
        break;
    case 2:
        em2fSetPosRisingD(em);
        if (w->flags & 0x200) {
            SndRoomStrStart(1, 3, 1);
            w->flags &= ~0x200;
        }
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x1D), 0, 5, 75);
        w->flags |= 0x10;
        w->timer = 170;
        em->r_no_2++;
    case 3:
        em->flag |= 0x108;
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x15), ARC(0x26), 10, 0x401, 0);
        w->flags &= ~0x10;
        EstSet(em, -1, 0, 0, EFF_EM2F, 7, 0, w->espKind, em, 0);
        w->timer = 0;
        w->timer2 = 90;
        em->r_no_2++;
    case 5:
        if (w->timer2) {
            w->timer2--;
            em->flag |= 0x100;
        }
        r = MotionMove(em, 0);
        if (r) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                if (StaFlagChk(pG, STA_PL_SWIM)) {
                    EmRoutineSet(pPL, r, 0xF, 9, r);
                    SndCall(8, 0x1E, &em->pos, em->id, 0, em);
                    SndCall(8, 0x1F, &pPL->pos, em->id, 0, pPL);
                }
                w->timer = 30;
                em->flag |= 0x40;
            }
            if (w->timer) {
                w->timer--;
                AddWaterPower(em->pos, 3.0f);
            }
            if (em->Motion.Seq_frame > 129.7f && em->Motion.Seq_frame < 130.3f) {
                em->flag |= 0x40;
            }
        }
        break;
    }
    hide = StaFlagChk(pG, STA_PL_SWIM);
    if (hide == 0) {
        w->risingOk = 1;
    }
    if (w->risingOk && em2fRisingDragonCk(em)) {
        return;
    }
    em2fWaterEffSet(em);
}

// R1 == 8 Packman: dives beside the boat (em2fSetPosPackman), surfaces with the mouth open and
// swallows the player off the boat (his routine 0xF/9, hidden) when the boat flag is set, else misses;
// then em2fChangeRoute and Swim (3).
static void em2f_R1_Packman(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int r;
    u32 hide;  // see em2f_R1_RisingDragon
    int one = 1;

    switch (em->r_no_2) {
    case 0:
        em2fSetPosBetweenBoat(em);
        w->flags &= ~0x20;
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x1D), 0, 5, 0);
        w->timer = 149;
        w->risingOk = 0;
        w->timer2 = 15;
        w->flags |= 0x10;
        em->r_no_2++;
    case 1:
        em->flag |= 0x108;
        MotionMove(em, 0);
        if (w->timer == 0) {
            hide = StaFlagChk(pG, STA_PL_SWIM);
            if (hide == 0) {
                EmRoutineSet(em, one, 3, 0, 0);
                break;
            }
            em->r_no_2++;
            break;
        }
        do { w->timer--; } while (0);  // LOOP_END barrier: the timer2 load stays below the store
        if (w->timer2) {
            w->timer2--;
            if (w->timer2 == 0) {
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
            }
        }
        break;
    case 2:
        em2fSetPosPackman(em);
        if (w->flags & 0x200) {
            SndRoomStrStart(1, 3, 1);
            w->flags &= ~0x200;
        }
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x1D), 0, 5, 0);
        w->flags |= 0x10;
        w->timer = 180;
        em->r_no_2++;
    case 3:
        em->flag |= 0x108;
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x17), ARC(0x28), 10, 0x401, 0);
        w->flags &= ~0x10;
        EstSet(em, -1, 0, 0, EFF_EM2F, 0xF, 0, w->espKind, em, 0);
        w->timer = 0;
        w->timer2 = 90;
        em->r_no_2++;
    case 5:
        if (w->timer2) {
            w->timer2--;
            em->flag |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 4) {
            SndCall(8, 0x19, &em->pos, em->id, 0, em);
        }
        r = MotionMove(em, 0);
        if (r) {
            if ((s16) pG->pl_life > 0) {
                em2fChangeRoute(em);
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                em->r_no_2++;
            }
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                if (StaFlagChk(pG, STA_PL_SWIM)) {
                    EmRoutineSet(pPL, r, 0xF, 9, r);
                    pPL->be_flag &= ~2;
                    SndCall(8, 0x1E, &em->pos, em->id, 0, em);
                }
                w->timer = 30;
            }
            if (w->timer) {
                w->timer--;
                AddWaterPower(em->pos, 3.0f);
            }
            if (em->Motion.Seq_frame > 59.7f && em->Motion.Seq_frame < 60.3f) {
                em->flag |= 0x40;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xE), ARC(0x1F), 30, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM2F, 0xC, 0, w->espKind, em, 0);
        em->r_no_2++;
    case 7:
        if (MotionMove(em, 0)) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    hide = StaFlagChk(pG, STA_PL_SWIM);
    if (hide == 0) {
        w->risingOk = 1;
    }
    if (w->risingOk && em2fRisingDragonCk(em)) {
        return;
    }
    em2fWaterEffSet(em);
}

// R1 == 9 HideMode: after atkCnt attacks the monster dives out of sight (BGM stopped, invisible,
// em2fSetPosHideMode far from the player) for 90..180 frames, surfaces and rushes (flags 0x480 /
// 0x580: fast, surfaced, tentacles out) at the boat; a harpoon hit while hiding (hideRush) makes it
// break out early; back to Swim (3) with a new atkCnt.
static void em2f_R1_HideMode(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    if (w->flags & 0x40) {
        w->hideRush = 1;
    }
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x1C), ARC(0x2D), 30, flip, 0);
        w->flags &= ~0x20;
        w->atkCnt = Rnd() % 3 + 1;
        w->timer = 120;
        SndRoomStrStop(5);
        SndEventStrStop(5);
        w->flags |= 0x200;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            em->flag |= 0x28;
        }
        if (MotionMove(em, 0)) {
            em->be_flag &= ~2;
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA), ARC(0x1D), 0, 5, 0);
        w->flags |= 0x10;
        w->timer = Rnd() % 90 + 90;
        em->r_no_2++;
    case 3:
        em->flag |= 0x128;
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        em2fSetPosHideMode(em);
        w->hideRush = 0;
        MotionSetCore(em, MOTION(em), ARC(0x32), ARC(0x34), 30, flip, 0);
        em->be_flag |= 2;
        SndRoomStrStart(1, 3, 1);
        w->flags &= ~0x200;
        EstSet(em, -1, 0, 0, EFF_EM2F, 0x15, 0, w->espKind, em, 0);
        em->r_no_2++;
    case 5:
        em->flag |= 0x12C;
        w->flags |= 0x480;
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(9), ARC(0x33), 30, 5, 0);
        SndCall(8, 0x1A, &em->pos, em->id, 0, em);
        SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        w->flags &= ~0x10;
        em->r_no_2++;
    case 7:
        em->flag |= 0x12C;
        w->flags |= 0x580;
        MotionMove(em, 0);
        if (em->l_pl < 900000000.0f) {
            if (w->hideRush) {
                em->r_no_2 = 0xA;
            } else {
                em->r_no_2 = 8;
            }
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0x17), ARC(0x28), 10, 0x401, 0);
        w->flags &= ~0x10;
        EstSet(em, -1, 0, 0, EFF_EM2F, 0xF, 0, w->espKind, em, 0);
        w->timer2 = 90;
        em->r_no_2++;
    case 9:
        em->flag |= 0x12C;
        if (w->timer2) {
            w->timer2--;
            em->flag |= 0x100;
        }
        w->flags |= 0x480;
        if (em->Motion.Seq_old.Free & 4) {
            SndCall(8, 0x1B, &em->pos, em->id, 0, em);
        }
        if (MotionMove(em, 0)) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    case 0xA:
        MotionSetCore(em, MOTION(em), ARC(0xE), ARC(0x1F), 30, flip, 0);
        EstSet(em, -1, 0, 0, EFF_EM2F, 0x16, 0, w->espKind, em, 0);
        w->timer2 = 90;
        em->r_no_2++;
    case 0xB:
        em->flag |= 0x28;
        if (w->timer2) {
            w->timer2--;
            em->flag |= 0x100;
        }
        if (MotionMove(em, 0)) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    if (em2fRisingDragonCk(em)) {
        return;
    }
    em2fWaterEffSet(em);
}

// R1 == 0xA Critical: the death scene: surfaces at em2f_die_pos when the player is near (quake), then
// the drowning motion (ARC 0x1B) pulling the player under (the fixed camera pair blended by
// em2fCriCamMove, DiedemoExec 2, pl_life 0); ends with the monster dead and hidden.
static void em2f_R1_Critical(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;

    switch (em->r_no_2) {
    case 0:
        em->ang.y = -1.57f;
        PSMTXRotRad(m, 'y', -1.57f);
        v.x = -58.0f;
        v.y = -48095.0f;
        v.z = -35742.0f;
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&em2f_die_pos, &v, &em->pos);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x405, 0);
        MotionMove(em, 0);
        em->be_flag &= ~2;
        w->timer = 5;
        em->r_no_2++;
    case 1:
        if ((em2f_die_pos.x - pPL->pos.x) * (em2f_die_pos.x - pPL->pos.x) + (em2f_die_pos.z - pPL->pos.z) * (em2f_die_pos.z - pPL->pos.z)
            > 6250000.0f) {
            break;
        }
        if (pPL->pos.y < -800.0f) {
            break;
        }
        if (!StaFlagChk(pG, STA_PL_FIRE)) {
            break;
        }
        if (w->timer) {
            QuakeExec(0, 0, 30, 3.0f, 2);
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        em->ang.y = -1.57f;
        PSMTXRotRad(m, 'y', -1.57f);
        v.x = -58.0f;
        v.y = -24500.0f;
        v.z = -20000.0f;
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&em2f_die_pos, &v, &em->pos);
        em->be_flag |= 2;
        KeyStop(0xEFCF0000);
        w->camPos.x = -51287.0f;
        w->camPos.y = 3580.0f;
        w->camPos.z = 23010.0f;
        w->camAt.x = -46241.0f;
        w->camAt.y = 28.0f;
        w->camAt.z = 22609.0f;
        w->timer = 3;
        w->timer2 = 15;
        MotionSetCore(em, MOTION(em), ARC(0x1B), ARC(0x2C), 0, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM2F, 0x13, 0, w->espKind, em, 0);
        em->r_no_2++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            if (w->timer2) {
                w->timer2--;
                if (w->timer2 == 0) {
                    w->camPos.x = -57285.0f;
                    w->camPos.y = 38.0f;
                    w->camPos.z = 22167.0f;
                    w->camAt.x = -45583.0f;
                    w->camAt.y = 221.0f;
                    w->camAt.z = 22613.0f;
                }
            }
            em2fCriCamMove(em);
        }
        if (em->Motion.Seq_old.Free & 4) {
            pPL->be_flag &= ~2;
        }
        if (em->Motion.Seq_old.Free & 1) {
            DiedemoExec(2, 0);
            pG->pl_life = 0;
        }
        if (MotionMove(em, 0)) {
            em->hp = 0;
            em->be_flag &= ~2;
            em->r_no_2++;
        }
        break;
    }
}

// Eases the drowning cut camera (em2f_cri_cam) towards camPos / camAt and installs it as the extra camera.
void em2fCriCamMove(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Camera* c = &pG->Camera;
    Camera* cam;
    Vec v;

    PosToPos(&c->param.pos, &w->camPos, &em2f_cri_cam.param.pos, 0.1f);
    PosToPos(&c->param.at, &w->camAt, &em2f_cri_cam.param.at, 0.1f);
    v.x = fRand1_1() * 50.0f;
    v.y = fRand1_1() * 50.0f;
    v.z = fRand1_1() * 50.0f;
    PSVECAdd(&em2f_cri_cam.param.pos, &v, &em2f_cri_cam.param.pos);
    PSVECAdd(&em2f_cri_cam.param.at, &v, &em2f_cri_cam.param.at);
    {
        // cam BEFORE the pos/at pointers: its lo_sum finds no register for em2f_cri_cam+N (the call
        // arguments were hard regs) and stays a fresh lis/addi; the pointers then reuse the call
        // arguments' lo_sum table entries instead of being re-based on cam.
        Vec* pos;
        Vec* at;
        f32 dx, dy, dz;
        cam = &em2f_cri_cam;
        pos = &em2f_cri_cam.param.pos;
        at = &em2f_cri_cam.param.at;
        dx = pos->x - at->x;
        dy = pos->y - at->y;
        dz = pos->z - at->z;
        cam->param.fovy = c->param.fovy;
        cam->Up.x = 0.0f;
        cam->Up.y = 1.0f;
        cam->Up.z = 0.0f;
        cam->Distance = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// R0 == 2: damage (flag bit3), runs Em2f_R2_move_tbl (Dm_Normal).
static void em2f_R0_Damage(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    w->flags |= 8;
    Em2f_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Normal: the land flinch motion, then Wait (0) (the lake fight never uses it).
static void em2f_R1_Dm_Normal(cEm2f* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(9), 0, 3, 0x401, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

// R0 == 3: death (flag bit3), runs Em2f_R3_move_tbl (Die_Normal).
static void em2f_R0_Die(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    w->flags |= 8;
    Em2f_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: puts the player into his boat routine (0/0xF) and releases the boat
// (pBoat routine 1/1), the death thrash at em2fSetPosDie, inactive, then fades out (invisible_factor
// -0.02 per frame).
static void em2f_R1_Die_Normal(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    cPlayer* pl;

    switch (em->r_no_2) {
    case 0:
        em2fSetPosDie(em);
        MotionSetCore(em, MOTION(em), ARC(0x2E), 0, 0, 0x401, 0);
        pl = pPL;
        pl->r_no_1 = 0xF;
        pl->r_no_2 = 0xC;
        pl->r_no_0 = 0;
        pl->r_no_3 = 0;
        if (w->pBoat) {
            w->pBoat->r_no_0 = 1;
            w->pBoat->r_no_1 = 1;
            w->pBoat->r_no_2 = 0;
            w->pBoat->r_no_3 = 0;
        }
        w->timer = 112;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->clearStatus(EM_STATUS_ACTIVE);
            em->r_no_2++;
        } else if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                em->clearStatus(EM_STATUS_ACTIVE);
            }
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

// Per frame: the route point / angle to the player (routePos, routeAng, flag bit0 = found) and the
// partner (subRoutePos, flag bit1 present / bit2 targeted), and the chosen target copies.
void em2fRouteCk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

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

// Never called (the original REL link dropped the body and kept the pool: 0, 1/256, 0.9, 0.05, 1.1, 1).
static void em2fScaleSet(cEm2f* em)
{
    f32 r = 0.0f;

    if (em->hp) {
        r = (f32) Rnd() * (1.0f / 256.0f);
    }
    if (r > 0.9f) {
        r = 0.9f;
    }
    em->scale.x = r * 0.05f + 1.1f;
    em->scale.y = 1.0f;
}

// Water splash on the parts `no`.
static inline void em2fWaterPower(cEm2f* em, int no)
{
    cModel* p = em->getPartsPtr(no);

    AddWaterPower(p->world, fRand0_1() * 0.3f + 0.3f);
}

// Water effects on the swim: the wake / splash at the body parts every effTimer2 (surfaced), effTimer3
// (fast, flag bit8) or effTimer1 (underwater) frames.
void em2fWaterEffSet(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    if (w->waterY < em->pos.y + 2000.0f) {
        em2fWaterPower(em, 0);
        em2fWaterPower(em, 4);
        em2fWaterPower(em, 7);
        em2fWaterPower(em, 0x18);
        em2fWaterPower(em, 0x1B);
        if (w->effTimer2) {
            w->effTimer2--;
            if (w->effTimer2 == 0) {
                w->effTimer2 = 9;
                EstSet(em, -1, 0, 0, EFF_EM2F, 3, 0, w->espKind, em, 0);
            }
        }
        if (w->effTimer3) {
            w->effTimer3--;
            if (w->effTimer3 == 0) {
                w->effTimer3 = 3;
                if (w->flags & 0x100) {
                    EstSet(em, -1, 0, 0, EFF_EM2F, 0x14, 0, w->espKind, em, 0);
                } else {
                    EstSet(em, -1, 0, 0, EFF_EM2F, 6, 0, w->espKind, em, 0);
                }
            }
        }
    } else {
        if (w->effTimer1) {
            w->effTimer1--;
            if (w->effTimer1 == 0) {
                w->effTimer1 = 2;
                EstSet(em, -1, 0, 0, EFF_EM2F, 2, 0, w->espKind, em, 0);
            }
        }
    }
    if (pG->debug_mode == 7) {
        Draw_line3d(&w->nextPos, &em->pos, 0xFF808080, 0);
        Draw_sphere(&w->nextPos, 2000.0f, 0xFFFFFFFF, 1, 1);
    }
}

// Advances to the next EMI type 2 route point (routeIdx, wrapping to the first) into nextPos and
// returns its sub type (the action at that point); without EMI data a random em2f_route_tbl point
// and a random type 0..2.
int em2fSetNextRoute(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    EmiData* emi = pG->pEmi;
    u32 i;
    int idx;

    if (emi == 0) {
        i = Rnd() & 3;  // the loop counter: its r3 copy preference puts i in r3
        w->nextPos = em2f_route_tbl[i];
        return Rnd() % 3;
    }
    idx = -1;
    for (i = 0; i < (u32) emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 2) {
            continue;
        }
        if (i > (u32) w->routeIdx) {
            w->routeIdx = i;
            w->nextPos = e->pos;
            return e->sub;
        }
        if ((u32) idx > i) {
            idx = i;
        }
    }
    // the returning then-arm is moved to the end by jump.c, so the idx arm is the fall-through
    if (idx == -1) {
        i = Rnd() & 3;
        w->nextPos = em2f_route_tbl[i];
        return Rnd() % 3;
    }
    {
        EmiEntry* e;

        w->routeIdx = idx;  // reference store: the pG reload waits for it (idx frees r10)
        e = &(pG->pEmi)->entry[idx];
        w->nextPos = e->pos;
        return e->sub;
    }
}

// Puts the monster right under the boat, facing the player.
void em2fSetPosBetweenBoat(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0xF) {
            continue;
        }
        em->ang.x = 0.0f;
        em->ang.y = pPL->ang.y;
        em->ang.z = 0.0f;
        em->be_flag |= 2;
        em2fEffectDelete(em, w);
        PSMTXRotRad(m, 'y', em->ang.y);
        PosToPos(&e->pos, &pPL->pos, &v, 0.5f);
        v.y = w->waterY;
        TransMatrix(m, &v);
        v.x = -5000.0f;
        v.y = -5000.0f;
        v.z = -40000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        break;
    }
}

// Puts the monster beside the boat for the ramming attack.
void em2fSetPosRisingD(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    Vec a;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0xF) {
            continue;
        }
        em2fEffectDelete(em, w);
        PSMTXRotRad(m, 'y', e->ang.y);
        TransMatrix(m, &e->pos);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 500.0f;
        PSMTXMultVec(m, &a, &a);
        em->ang.y = e->ang.y - (PI / 2.0f) + (PI / 32.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        PSMTXRotRad(m, 'y', em->ang.y);
        v = a;
        v.y = w->waterY;
        TransMatrix(m, &v);
        v.x = 0.0f;
        v.y = -5000.0f;
        v.z = -133000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        break;
    }
}

// Puts the monster beside the boat for the swallowing attack.
void em2fSetPosPackman(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    Vec a;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0xF) {
            continue;
        }
        em2fEffectDelete(em, w);
        PSMTXRotRad(m, 'y', e->ang.y);
        TransMatrix(m, &e->pos);
        a.x = 2000.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        PSMTXMultVec(m, &a, &a);
        em->ang.y = e->ang.y - (PI / 2.0f) + (PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        PSMTXRotRad(m, 'y', em->ang.y);
        v = a;
        v.y = w->waterY;
        TransMatrix(m, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -133000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        break;
    }
}

// Puts the monster far away from the player, facing him, for the hiding phase.
void em2fSetPosHideMode(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    f32 ang;

    if (Rnd() & 1) {
        ang = 2.45f;
    } else {
        ang = -0.79f;
    }
    PSMTXRotRad(m, 'y', fRand1_1() * (PI / 64.0f) + ang);
    TransMatrix(m, &pPL->pos);
    v.x = 0.0f;
    v.z = 100000.0f;
    v.y = 0.0f;
    PSMTXMultVec(m, &v, &em->pos);
    em->pos.y = w->waterY - 37894.84f - 4882.0f;
    em->ang.y = GetXZAngle(&em->pos, &pPL->pos);
    em->ang.y += PI;
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    v.x = -10000.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(m, &v, &em->pos);
    em2fEffectDelete(em, w);
}

// Puts the monster at the fixed death position em2f_die_pos for the death routine.
void em2fSetPosDie(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    em->pos.x = 40120.0f;
    em->pos.y = w->waterY;
    em->pos.z = 38050.0f;
    em->ang.x = 0.0f;
    em->ang.y = -0.2951f;
    em->ang.z = 0.0f;
}

// Attack choice when the fight is on (Status_flg[1] bit22 clear): alternates RisingDragon (7) and
// Packman (8) with rndFlag (three in four keep the last kind). 1 when a routine was set.
int em2fRisingDragonCk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int one;

    one = 1;
    if (StaFlagChk(pG, STA_PL_SWIM)) {
        if (w->rndFlag) {
            if (Rnd() & 3) {
                w->rndFlag = 0;
            }
            EmRoutineSet(em, one, 7, 0, 0);
        } else {
            if (Rnd() & 3) {
                w->rndFlag = one;
            }
            EmRoutineSet(em, one, 8, 0, 0);
        }
        return 1;
    }
    return 0;
}

// Re-routes after an attack: picks the EMI type 2 point ahead of the monster (within 45 deg of its
// heading, else the nearest) as nextPos / routeIdx; random em2f_route_tbl point without EMI data.
void em2fChangeRoute(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    EmiData* emi = pG->pEmi;
    int best;
    int bestPrev;
    f32 bestAng;
    int i;
    EmiEntry* e;     // function scope: shared by the search loop and the tail (e r30, prev r3)
    EmiEntry* prev;

    if (emi == 0) {
        i = Rnd() & 3;
        w->nextPos = em2f_route_tbl[i];
        return;
    }
    best = -1;
    bestPrev = -1;
    bestAng = PI;
    for (i = 0; i < (pG->pEmi)->n; i++) {
        int j;
        f32 ang;

        e = &(pG->pEmi)->entry[i];

        if (e->type != 2) {
            continue;
        }
        ang = fabsf(Muku(&em->pos, &e->pos, em->ang.y, PI));
        if (ang > bestAng) {
            continue;
        }
        if (i == 0) {
            j = (pG->pEmi)->n - 1;
        } else {
            j = i - 1;
        }
        prev = 0;
        for (; j > 0; j--) {
            prev = &(pG->pEmi)->entry[j];
            if (prev->type == 2) {
                break;
            }
        }
        if (j < 0) {
            continue;
        }
        if (prev == 0) {
            continue;
        }
        if (fabsf(Muku2(GetXZAngle(&prev->pos, &e->pos), em->ang.y, PI)) > PI / 4.0f) {
            continue;
        }
        bestAng = ang;
        best = i;
        bestPrev = j;
    }
    if (best == -1) {
        i = Rnd() & 3;
        w->nextPos = em2f_route_tbl[i];
        return;
    }
    w->routeIdx = best;  // reference store: the pG reload stays below it
    e = &(pG->pEmi)->entry[best];
    prev = &(pG->pEmi)->entry[bestPrev];
    w->nextPos = e->pos;
    w->routeType = prev->sub;
    w->nextRouteType = e->sub;
}

// Breaks the floating islands the head or the tail runs into.
void em2fIslandCrashCk(cEm2f* em)
{
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObj1c* o = (cObj1c*) ObjMgr.fastAt(i);
        cModel* p;
        f32 r;
        f32 d;

        if (!o->isAlive()) {
            continue;
        }
        if (o->id != 0x1C) {
            continue;
        }
        if (o->ckCrash()) {
            continue;
        }
        r = o->scale.x * 1800.0f;
        d = (em->pos.x - o->pos.x) * (em->pos.x - o->pos.x) + (em->pos.z - o->pos.z) * (em->pos.z - o->pos.z);
        if (d > 1600000000.0f) {
            continue;
        }
        p = em->getPartsPtr(8);
        r = r * r;
        d = (p->world.x - o->pos.x) * (p->world.x - o->pos.x) + (p->world.y - o->pos.y) * (p->world.y - o->pos.y) +
            (p->world.z - o->pos.z) * (p->world.z - o->pos.z);
        if (d < r) {
            o->setCrashBig(&p->world);
            continue;
        }
        p = em->getPartsPtr(3);
        d = (p->world.x - o->pos.x) * (p->world.x - o->pos.x) + (p->world.y - o->pos.y) * (p->world.y - o->pos.y) +
            (p->world.z - o->pos.z) * (p->world.z - o->pos.z);
        if (d < r) {
            o->setCrashBig(&p->world);
            continue;
        }
        p = em->getPartsPtr(0x19);
        d = (p->world.x - o->pos.x) * (p->world.x - o->pos.x) + (p->world.y - o->pos.y) * (p->world.y - o->pos.y) +
            (p->world.z - o->pos.z) * (p->world.z - o->pos.z);
        if (d < r) {
            o->setCrashBig(&p->world);
        }
    }
}

// While the tentacle motion flag (Motion.Seq_old.Free bit5) and flag bit7 (rising) are set, creates the six
// tentacle objects (cObj16 type 0xA) on parts 0x1D..0x22 with staggered motions and the tentacle SE
// every 45 frames; removes them (clearLostWait) otherwise.
void em2fTentacleMove(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    u16 step = (*(u16*) ARC(0x31) & 0x3FFF) / 6;
    u32 i;

    if ((em->Motion.Seq_old.Free & 0x20) && (w->flags & 0x80)) {
        int frame = 0;

        for (i = 0; i < 6; i++, frame += step) {
            int parts;
            Vec pos;
            Vec rot;

            if (w->pTentacle[i]) {
                continue;
            }
            switch (i) {
            case 0:
            default:
                parts = 0x1D;
                break;
            case 1:
                parts = 0x1E;
                break;
            case 2:
                parts = 0x1F;
                break;
            case 3:
                parts = 0x20;
                break;
            case 4:
                parts = 0x21;
                break;
            case 5:
                parts = 0x22;
                break;
            }
            pos.x = 0.0f;
            pos.y = 0.0f;
            pos.z = 0.0f;
            rot.x = PI / 2.0f;
            rot.y = 0.0f;
            rot.z = fRand1_1() * PI;
            w->pTentacle[i] = (cObj16*) SetObj16(ARC(0x2F), ARC(0x30), em, em, parts, 0xA, &pos, &rot);
            if (w->pTentacle[i]) {
                MotSetObj16(w->pTentacle[i], ARC(0x31), 4, frame);
            }
        }
        if (w->seTimer4) {
            w->seTimer4--;
        } else {
            w->seTimer4 = 45;
            SndCall(8, 0x12, &em->pos, em->id, 0, em);
        }
    } else {
        for (i = 0; i < 6; i++) {
            if (w->pTentacle[i]) {
                w->pTentacle[i]->clearLostWait();
                w->pTentacle[i] = 0;
            }
        }
        w->seTimer4 = 0;
    }
}
