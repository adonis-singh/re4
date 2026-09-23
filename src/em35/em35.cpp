// em35 module (D:/Bio4/Prog/em35.cpp): the boss of the beam hall. cModel::type 0 is the whole enemy that
// walks the floor (em35_R1_Walk / Atk / BearHug / Hook / Catch ...), type 1 the upper body that climbs the
// room's beam graph after the divide (em35_R1_U_*, em35Beam*Ck) and type 2 the divided legs.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em35.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "obj00.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "item.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pendulum.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "eprintf.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em10.h"
#include "em_mod.h"

asm(".comm common_em35,52,4");


typedef void (*Em35Func)(cEm35*);

static void em35_R0_Init(cEm35* em);
static void em35_R0_Move(cEm35* em);
static void em35_R1_br_dummy(cEm35* em);
static void em35_R1_Divide(cEm35* em);
static void em35_R1_U_Divide(cEm35* em);
static void em35_R1_Wait(cEm35* em);
static void em35_R1_Walk(cEm35* em);
static void em35_R1_BigStep(cEm35* em);
static void em35_R1_Turn(cEm35* em);
static void em35_R1_Atk(cEm35* em);
static void em35_R1_br_AtkDouble(cEm35* em);
static void em35_R1_AtkDouble(cEm35* em);
static void em35_R1_BearHug(cEm35* em);
static void plem35_BearHug(cPlayer* pl);
static void em35AtkEscapeAction(cEm35* em);
static void plem35Sit(cPlayer* pl);
static void em35_R1_Atk2F(cEm35* em);
static void plem35DmFall2F(cPlayer* pl);
static void em35_R1_LongAtk(cEm35* em);
static void em35_R1_Hook(cEm35* em);
static void plem35DmHook(cPlayer* pl);
static void em35_R1_br_Critical(cEm35* em);
static void em35_R1_Critical(cEm35* em);
static void em35_R1_CriticalHit(cEm35* em);
static void plem35_CriticalHit(cPlayer* pl);
static void em35DashEscapeAction(cEm35* em);
static void plem35DashEscape(cPlayer* pl);
static void plem35DmStamp(cPlayer* pl);
static void em35_R1_br_Catch(cEm35* em);
static void em35_R1_Catch(cEm35* em);
static void em35_R1_CatchHit(cEm35* em);
static void plem35_CatchHit(cPlayer* pl);
static void em35_R1_U_Wait(cEm35* em);
static void em35_R1_U_Jump(cEm35* em);
static void em35_R1_U_JumpUp(cEm35* em);
static void em35_R1_U_JumpDown(cEm35* em);
static void em35_R1_U_DoubleJump(cEm35* em);
static void em35_R1_U_BackJump(cEm35* em);
static void em35_R1_U_Turn180(cEm35* em);
static void em35_R1_U_Step(cEm35* em);
static void em35_R1_U_BigStep(cEm35* em);
static void em35_R1_U_OverStep(cEm35* em);
static void em35_R1_U_StepUp(cEm35* em);
static void em35_R1_U_StepDown(cEm35* em);
static void em35_R1_U_HandAtk(cEm35* em);
static void em35_R1_U_Atk(cEm35* em);
static void em35_R1_U_Upper(cEm35* em);
static void em35_R1_U_AtkSpear(cEm35* em);
static void em35_R1_U_Crawl(cEm35* em);
static void em35_R1_U_CrawlTurn(cEm35* em);
static void em35_R1_U_JumpToBeam(cEm35* em);
static void em35_R0_Damage(cEm35* em);
static void em35_R1_Dm_Small(cEm35* em);
static void em35_R1_Dm_Spinal(cEm35* em);
static void em35_R1_Dm_Big(cEm35* em);
static void em35_R1_Dm_Frame(cEm35* em);
static void em35_R1_Dm_U_Fall(cEm35* em);
static void em35_R1_Dm_U_Crawl(cEm35* em);
static void em35_R0_Die(cEm35* em);
static void em35_R1_Die_Normal(cEm35* em);
static void em35_R1_Die_Pose(cEm35* em);

#define VIB_TBL ((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore))





extern "C" void _prolog()
{
    OSReport("em35 prolog Ok\n");
    EmInitFunc = Em35Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

// Placement-constructs the enemy over the manager's cEm slot (EmInitFunc for id 0x35); em35_R0_Init
// does the per-type setup on the first move.
void Em35Init(cEm* em)
{
    new (em) cEm35();
}

// Damage reaction of the whole body (type 0), from move(). The area damage manager (kinds 1 / 4 /
// 5 / 7: the room's traps) once per 120 frames puts it into the frame-fall reaction (routine 2/3).
// A weapon hit in dmHit applies em35SetDmVal (LifeDownSet2, no floor), blood and hit sound sized by
// weapon class (the mine 0x27 sometimes leaves a spark at the hit point; shotgun / heavy hits are
// bigger with the camera within 4 m). Death enters routine 3/0. Otherwise: damage on the weak
// points (em35WeakDmCk) accumulates in weakDmg and past 400 triggers the spinal reaction (2/1);
// handgun-class damage past 400 in dmgCnt the small flinch (2/0, not while attacking, flags 0x80);
// close shotgun hits a 1-in-3 flinch (small or 1-in-8 big); magnum-class (5 / 6) always flinch; any
// other weapon the spinal reaction. flags 8 (a reaction already running) blocks the light cases.
void em35DmCk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Camera* cam = &pG->Camera;
    int near;
    int dmg;
    cParts* p;
    f32 d;

    if (em->hp > 0 && EmDeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            if (w->dieTimer == 0) {
                w->dieTimer = 120;
                if (w->flags & 8) {
                    return;
                }
                em->r_no_0 = 2;
                em->r_no_1 = 3;
                em->r_no_2 = 0;
                em->r_no_3 = 0;
                w->weakDmg = 0;
                w->dmgCnt = 0;
                return;
            }
            break;
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
    dmg = em35SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    p = em->getPartsPtr(0);
    d = (cam->param.pos.x - p->world.x) * (cam->param.pos.x - p->world.x) +
        (cam->param.pos.y - p->world.y) * (cam->param.pos.y - p->world.y) +
        (cam->param.pos.z - p->world.z) * (cam->param.pos.z - p->world.z);
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 0x10:
        EmDmBloodSet2(em, 0x2C, 0x17, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27: {
        Vec pos;
        Vec dir;

        EmDmBloodSet2(em, 0x2C, 0xA, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        if ((Rnd() & 3) == 0) {
            if (EmGetDmPos(em, &pos, &dir)) {
                EstSet(0, -1, &pos, 0, EFF_EM35, 0xB, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        break;
    }
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (d < 16000000.0f) {
                EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
            }
            SndCall(8, 6, &em->pos, em->id, 0, em);
        } else {
            EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
            SndCall(8, 4, &em->pos, em->id, 0, em);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (d < 16000000.0f) {
            EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
        }
        SndCall(8, 6, &em->pos, em->id, 0, em);
        break;
    case 0x17:
    case 0x2A:
        if (w->flags & 8) {
            return;
        }
        em->r_no_3 = 0;
        em->r_no_0 = 2;
        em->r_no_2 = 0;
        em->r_no_1 = 2;
        w->weakDmg = 0;
        w->dmgCnt = 0;
        return;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmRoutineSet(em, 3, 0, 0, 0);
        return;
    }
    w->dmgCnt += dmg;
    if (em35WeakDmCk(em)) {
        w->weakDmg += dmg;
        if (w->weakDmg > 400) {
            w->weakDmg = 0;
            w->dmgCnt = 0;
            em->r_no_0 = 2;
            em->r_no_1 = 1;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            return;
        }
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
    case 0xE:
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
        if (w->flags & 8) {
            return;
        }
        if (w->dmgCnt > 400) {
            if (w->flags & 0x80) {
                return;
            }
            w->dmgCnt = 0;
            em->r_no_0 = 2;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (w->flags & 8) {
            return;
        }
        if (near) {
            if ((u8) (Rnd() % 3) == 0) {
                if (!(w->flags & 0x80)) {
                    if (Rnd() & 7) {
                        EmRoutineSet(em, 2, 0, 0, 0);
                    } else {
                        EmRoutineSet(em, 2, 2, 0, 0);
                    }
                    w->dmgCnt = 0;
                    return;
                }
            }
        }
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 5:
    case 6:
    case 0xF:
    case 0x2C:
        if (Rnd() & 3) {
            EmRoutineSet(em, 2, 0, 0, 0);
        } else {
            EmRoutineSet(em, 2, 2, 0, 0);
        }
        w->weakDmg = 0;
        w->dmgCnt = 0;
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        em->r_no_0 = 2;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        break;
    }
    w->weakDmg = 0;
    w->dmgCnt = 0;
}

// Damage reaction of the upper body (type 1), from move(). Applies em35SetDmVal with the same
// blood / sound selection as em35DmCk (near = within 4 m). At 0 HP it dies through the fall reaction
// (2/4). While in the air or crawling (flags 0x20) only close shotgun / rifle hits (1 in 3) or heavy
// weapons knock it into the crawl reaction (2/5). On a beam, handgun-class damage past 400 in dmgCnt
// (or a 1-in-4 roll on close shotgun / rifle hits) and any magnum-class hit knock it off (2/4).
void em35DmCkUpper(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Camera* cam = &pG->Camera;
    int near;
    int dmg;
    cParts* p;
    f32 d;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    near = 0;
    if (em->dmg.m_pDamageYarare->len < 16000000.0f) {
        near = 1;
    }
    dmg = em35SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    w->dmgCnt += dmg;
    p = em->getPartsPtr(0);
    d = (cam->param.pos.x - p->world.x) * (cam->param.pos.x - p->world.x) +
        (cam->param.pos.y - p->world.y) * (cam->param.pos.y - p->world.y) +
        (cam->param.pos.z - p->world.z) * (cam->param.pos.z - p->world.z);
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (d < 16000000.0f) {
                EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
            }
            SndCall(8, 4, &em->pos, em->id, 0, em);
        } else {
            EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
            SndCall(8, 4, &em->pos, em->id, 0, em);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (d < 16000000.0f) {
            EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
        }
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 0x17:
    case 0x2A:
        break;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCnt(em);
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
    if (w->flags & 0x20) {
        switch (em->dmg.m_Wep) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 0xB:
        case 0xC:
        case 0xE:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2B:
        case 0x2C:
            return;
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                return;
            }
            if ((u8) (Rnd() % 3) != 0) {
                return;
            }
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
        case 9:
        case 0xA:
        case 0x28:
            if ((u8) (Rnd() % 3) != 0) {
                return;
            }
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
        case 0xD:
        case 0x12:
        case 0x13:
        case 0x29:
        case 0x2D:
        default:
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
        }
    }
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 4, 0, 0);
            return;
        }
        if (near == 0) {
            return;
        }
        if ((Rnd() & 3) == 0) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 4, 0, 0);
            return;
        }
        if ((Rnd() & 3) == 0) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
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
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
}

Em35Func Em35_R0_move_tbl[4] = {
    em35_R0_Init,
    em35_R0_Move,
    em35_R0_Damage,
    em35_R0_Die,
};

// Pairs of {branch check, routine} per routine-1 state (em35_R0_Move calls both).
static Em35Func Em35_R1_move_tbl[70] = {
    em35_R1_br_dummy, em35_R1_Wait,
    em35_R1_br_dummy, em35_R1_Walk,
    em35_R1_br_dummy, em35_R1_BigStep,
    em35_R1_br_dummy, em35_R1_Turn,
    em35_R1_br_dummy, em35_R1_Atk,
    em35_R1_br_dummy, em35_R1_LongAtk,
    em35_R1_br_AtkDouble, em35_R1_AtkDouble,
    em35_R1_br_dummy, em35_R1_BearHug,
    em35_R1_br_dummy, em35_R1_Hook,
    em35_R1_br_Critical, em35_R1_Critical,
    em35_R1_br_dummy, em35_R1_CriticalHit,
    em35_R1_br_dummy, em35_R1_Atk2F,
    em35_R1_br_Catch, em35_R1_Catch,
    em35_R1_br_dummy, em35_R1_CatchHit,
    em35_R1_br_dummy, em35_R1_Divide,
    em35_R1_br_dummy, em35_R1_U_Wait,
    em35_R1_br_dummy, em35_R1_U_Jump,
    em35_R1_br_dummy, em35_R1_U_JumpUp,
    em35_R1_br_dummy, em35_R1_U_JumpDown,
    em35_R1_br_dummy, em35_R1_U_DoubleJump,
    em35_R1_br_dummy, em35_R1_U_BackJump,
    em35_R1_br_dummy, em35_R1_U_Turn180,
    em35_R1_br_dummy, em35_R1_U_Step,
    em35_R1_br_dummy, em35_R1_U_BigStep,
    em35_R1_br_dummy, em35_R1_U_OverStep,
    em35_R1_br_dummy, em35_R1_U_StepUp,
    em35_R1_br_dummy, em35_R1_U_StepDown,
    em35_R1_br_dummy, em35_R1_U_HandAtk,
    em35_R1_br_dummy, em35_R1_U_Atk,
    em35_R1_br_dummy, em35_R1_U_Upper,
    em35_R1_br_dummy, em35_R1_U_AtkSpear,
    em35_R1_br_dummy, em35_R1_U_Divide,
    em35_R1_br_dummy, em35_R1_U_Crawl,
    em35_R1_br_dummy, em35_R1_U_CrawlTurn,
    em35_R1_br_dummy, em35_R1_U_JumpToBeam,
};

static Em35Func Em35_R2_move_tbl[6] = {
    em35_R1_Dm_Small,
    em35_R1_Dm_Spinal,
    em35_R1_Dm_Big,
    em35_R1_Dm_Frame,
    em35_R1_Dm_U_Fall,
    em35_R1_Dm_U_Crawl,
};

static Em35Func Em35_R3_move_tbl[2] = {
    em35_R1_Die_Normal,
    em35_R1_Die_Pose,
};

// The beam graph of the room: 17 beams with their links and end points.
static Em35Beam em35_beam_tbl[17] = {
    { 0, 0xFF, 0x0D, { 0xFF, 0xFF }, 0xFF, 0x01, { 0xFF, 0xFF, 0xFF, 0x09 }, { 39071.0f, -4000.0f, -53163.0f }, { 32050.0f, -4000.0f, -53163.0f } },
    { 0, 0xFF, 0x0E, { 0xFF, 0x04 }, 0x00, 0x02, { 0xFF, 0x0B, 0x09, 0xFF }, { 39071.0f, -4000.0f, -58159.0f }, { 32050.0f, -4000.0f, -58159.0f } },
    { 0, 0xFF, 0x0F, { 0x07, 0x05 }, 0x01, 0x03, { 0x0B, 0x0C, 0xFF, 0x0A }, { 39071.0f, -4000.0f, -63158.0f }, { 32050.0f, -4000.0f, -63158.0f } },
    { 0, 0xFF, 0xFF, { 0x08, 0x06 }, 0x02, 0xFF, { 0x0C, 0xFF, 0x0A, 0xFF }, { 39071.0f, -4000.0f, -68158.0f }, { 32050.0f, -4000.0f, -68158.0f } },
    { 0, 0xFF, 0xFF, { 0x01, 0xFF }, 0xFF, 0x05, { 0x09, 0xFF, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -58159.0f }, { 29528.0f, -4000.0f, -58159.0f } },
    { 0, 0xFF, 0xFF, { 0xFF, 0x02 }, 0x04, 0x06, { 0xFF, 0x0A, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -63158.0f }, { 29528.0f, -4000.0f, -63158.0f } },
    { 0, 0xFF, 0xFF, { 0x03, 0xFF }, 0x05, 0xFF, { 0x0A, 0xFF, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -68158.0f }, { 29528.0f, -4000.0f, -68158.0f } },
    { 0, 0xFF, 0xFF, { 0xFF, 0x02 }, 0xFF, 0x08, { 0xFF, 0xFF, 0x0B, 0x0C }, { 41589.0f, -4000.0f, -63158.0f }, { 39071.0f, -4000.0f, -63158.0f } },
    { 0, 0xFF, 0xFF, { 0xFF, 0x03 }, 0x07, 0xFF, { 0xFF, 0xFF, 0x0C, 0xFF }, { 41489.0f, -4000.0f, -68158.0f }, { 39071.0f, -4000.0f, -68158.0f } },
    { 1, 0xFF, 0x10, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0x00, 0x01, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -53163.0f }, { 32050.0f, -4000.0f, -58159.0f } },
    { 1, 0xFF, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0x02, 0x03, 0x05, 0x06 }, { 32050.0f, -4000.0f, -63158.0f }, { 32050.0f, -4000.0f, -68158.0f } },
    { 1, 0xFF, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0xFF, 0xFF, 0x01, 0x02 }, { 39071.0f, -4000.0f, -58159.0f }, { 39071.0f, -4000.0f, -63158.0f } },
    { 1, 0xFF, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0xFF, 0xFF, 0x02, 0x03 }, { 39071.0f, -4000.0f, -63158.0f }, { 39071.0f, -4000.0f, -68158.0f } },
    { 0, 0x00, 0xFF, { 0xFF, 0xFF }, 0xFF, 0x0E, { 0xFF, 0xFF, 0xFF, 0x10 }, { 39071.0f, -8000.0f, -53163.0f }, { 32050.0f, -8000.0f, -53163.0f } },
    { 0, 0x01, 0xFF, { 0xFF, 0xFF }, 0x0D, 0x0F, { 0xFF, 0xFF, 0x10, 0xFF }, { 39071.0f, -8000.0f, -58159.0f }, { 32050.0f, -8000.0f, -58159.0f } },
    { 0, 0x02, 0xFF, { 0xFF, 0xFF }, 0x0E, 0xFF, { 0xFF, 0xFF, 0xFF, 0xFF }, { 39071.0f, -8000.0f, -63158.0f }, { 32050.0f, -8000.0f, -63158.0f } },
    { 1, 0x09, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0x0D, 0x0E, 0xFF, 0xFF }, { 32050.0f, -8000.0f, -53163.0f }, { 32050.0f, -8000.0f, -58159.0f } },
};

// Positions the upper body crawls to (em35_R1_U_Crawl / Dm_U_Fall / Dm_U_Crawl).
static Vec em35_crawl_pos[5] = {
    { 35560.0f, -8000.0f, -53163.0f },
    { 36720.0f, -8000.0f, -58159.0f },
    { 36720.0f, -8000.0f, -63158.0f },
    { 30790.0f, -4000.0f, -68158.0f },
    { 40330.0f, -4000.0f, -68158.0f },
};

// Parts index remap of the flipped motions (cModel::motFlip) of the whole body / the upper body.
static u16 em35_flip0[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x1B, 0x20, 0x21, 0x22, 0x23,
    0x1C, 0x1D, 0x1E, 0x1F, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x32, 0x33, 0x30, 0x31, 0x34, 0x37, 0x38, 0x35, 0x36, 0x3B, 0x3C, 0x39, 0x3A, 0x3F, 0x40, 0x3D,
    0x3E, 0x43, 0x44, 0x41, 0x42, 0x47, 0x48, 0x45, 0x46, 0x4C, 0x4D, 0x4E, 0x49, 0x4A, 0x4B, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

static u16 em35_flip1[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x21, 0x22,
    0x23, 0x1E, 0x15, 0x20, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x32, 0x33, 0x30, 0x31, 0x34, 0x37, 0x38, 0x35, 0x36, 0x40, 0x41, 0x3D, 0x3E, 0x3F, 0x39, 0x3A,
    0x3B, 0x3C, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Attacks (em35AtkCk): [0] punch, [1] punch (left), [2] stamp, [3] double punch, [4] hook, [5]/[6] second
// floor punch, [7] critical, [8]/[9] upper body hand, [0xA]/[0xB] upper, [0xC] spear.
static EmAtkInfo em35_atk_tbl[13] = {
    { 400.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 400.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 400.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 400.0f, PL_DM_AUTO, 0, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 0, 0, 0xA, 0 },
    { 300.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 300.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 9999, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 500.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 700.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
};

// Cloth chains (em35ClothSet*): the type 1 tail and the hanging skin of both types.
static u8 em35ClothP[6] = { 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };
static u8 em35ClothUp[6] = { 0xFF, 0x12, 0x13, 0x14, 0x15, 0x16 };
static u8 em35ClothDp[6] = { 0x13, 0x14, 0x15, 0x16, 0x17, 0xFF };
static f32 em35ClothMax[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
static u8 em35ClothP2[2] = { 0x19, 0x1A };
static u8 em35ClothUp2[2] = { 0xFF, 0x19 };
static u8 em35ClothDp2[2] = { 0x1A, 0xFF };
static f32 em35ClothMax2[2] = { 0.3f, 0.4f };
static f32 em35ClothRate2[2] = { 0.8f, 0.8f };
// em35ClothAt2/3 are the only globals among the cloth tables (REL ADDR16 field 0 = global symbol).
CLOTH_AT_SET em35ClothAt2[5] = {
    { 0, 4, 4, 1.0f, 120.0f, { 0.0f, -100.0f, -30.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.3f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 5, 5, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0xB, 0xB, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.7f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};
static u8 em35ClothP3[2] = { 0x0B, 0x0C };
static u8 em35ClothUp3[2] = { 0xFF, 0x0B };
static u8 em35ClothDp3[2] = { 0x0C, 0xFF };
static f32 em35ClothMax3[2] = { 0.3f, 0.4f };
static f32 em35ClothRate3[2] = { 0.8f, 0.8f };
CLOTH_AT_SET em35ClothAt3[5] = {
    { 0, 9, 9, 1.0f, 120.0f, { 0.0f, -100.0f, -30.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 7, 8, 0.3f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0xF, 0xF, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0x15, 0x15, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 7, 8, 0.7f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};

// Per-frame update from the enemy manager. Order: the type's damage check, clear the per-frame
// flags, tick the attack / lock / trap timers, route to the player, the beam the upper body stands on
// (em35GetBeamNo, shown on the debug screen), the routine table (r_no_0 0xFF = model load failed:
// destroy), neck and breathing scale, parts, the three cloth chains, attack / collision / stage
// collision, the whole body's voice every 60 frames or the upper body's drip effect every 14, and
// the weak point objects.
void cEm35::move()
{
    Em35Work* w = EM35_WK(this);

    if (r_no_0) {
        switch (type) {
        case 0:
        default:
            em35DmCk(this);
            break;
        case 1:
            em35DmCkUpper(this);
            break;
        }
    }
    w->flags &= ~0xFF;
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->lockWait) {
        w->lockWait--;
    }
    if (w->dieTimer) {
        w->dieTimer--;
    }
    em35RouteCk(this);
    if (type == 1) {
        int no = em35GetBeamNo(&pos, 0);

        eprintf2(8, 12, 300, 400, 0, 0, "beam = %d", no);
        w->beamNo = no;
        if (no != 0xFF) {
            w->beamType = em35_beam_tbl[no].type;
        }
    }
    Em35_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em35NeckMove(this);
    em35ScaleMove(this);
    partsWorldCalc();
    em35ClothMove(this);
    em35ClothMove2(this);
    em35ClothMove3(this);
    be_flag &= ~0x00200000;
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (hp > 0) {
        if (type == 0) {
            if (w->seTimer) {
                w->seTimer--;
            } else {
                SndCall(8, 8, &pos, id, 0, this);
                w->seTimer = 60;
            }
        }
        if (hp > 0 && type == 1) {
            if (w->effTimer) {
                w->effTimer--;
            } else {
                cParts* p = getPartsPtr(0x15);

                w->effTimer = 14;
                EstSet(0, -1, &p->world, 0, EFF_EM35, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
    }
    em35WeakMove(this);
}

// Routine 0: per-type setup on the first frame. Loads the model (archive 4 with the extra model 6
// for the whole body, 7 the upper body, 8 the legs), the three cloth chains, the flip table, a 5 m
// light box, a 3 m tall collision, the hit boxes (root plus the type's extras), the effect data,
// the work (first attack right away, the double attack / hook held 300 frames), the weak point
// objects and the active / look-at status. Start by `set` and type: set 0 = the whole body walks
// (1/1) with its idle effects, the upper body waits on a beam (1/0xF) with the collision passed
// through; set 1 = the whole body starts in the divide scene (1/0xE), the upper body in its own
// divide (1/0x1F).
static void em35_R0_Init(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em35() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        w->pInfo = ModInfoMgr.create(ARC(6), ARC(5));
        if (w->pInfo) {
            em->addModel(w->pInfo);
        }
        break;
    case 1:
        if (em->modelInit(ARC(7), ARC(5)) == 0) {
            pLog->err(0, 0, "em35() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 2:
        if (em->modelInit(ARC(8), ARC(5)) == 0) {
            pLog->err(0, 0, "em35() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    em35ClothSet(em);
    em35ClothSet2(em);
    em35ClothSet3(em);
    if (em->type == 1) {
        em->Motion.flip = em35_flip1;
    } else {
        em->Motion.flip = em35_flip0;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 5000.0f, 5000.0f, 5000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);
    em->litArea.on(1);
    switch (em->type) {
    case 0:
    default:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 300.0f, 150.0f, 1, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 0xA, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 3, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 4, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 6, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 300.0f, 300.0f, 7, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[6], -300.0f, 0.0f, 0.0f, 150.0f, 400.0f, 0x12, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0x13, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[8], 20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0x18, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0x19, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[10], -20.0f, -300.0f, 0.0f, 220.0f, 300.0f, 0x1D, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[11], -20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0x1E, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[12], 20.0f, -300.0f, 0.0f, 220.0f, 300.0f, 0x21, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[13], 20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0x22, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[14], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x4B, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[15], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x4E, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[16], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2B, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[17], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2C, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[18], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2D, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[19], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2E, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[20], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2F, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[21], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x25, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[22], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x26, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[23], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x27, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[24], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x28, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[25], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x29, YAT_FLAG_ON);
        break;
    case 1:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 300.0f, 150.0f, 1, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x12, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x13, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x14, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x15, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 300.0f, 300.0f, 2, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[6], -300.0f, 0.0f, 0.0f, 150.0f, 400.0f, 8, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 170.0f, 300.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[8], 20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0xE, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[10], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x20, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[11], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x23, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2B, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[13], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2C, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[14], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2D, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[15], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2E, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[16], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2F, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[17], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x25, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[18], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x26, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[19], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x27, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[20], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x28, YAT_FLAG_ON);
        YarareAdd(em, &w->hit[21], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x29, YAT_FLAG_ON);
        break;
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(9), EFF_EM35, 0);
    w->espKind = EspPullCoreKind();
    w->flags = zero;
    w->neckAng = 0.0f;
    w->scaleAng = 0.0f;
    w->atkWait = zero;
    w->weakDmg = zero;
    w->dmgCnt = zero;
    w->sndId = zero;
    w->lockWait = 300;
    w->seTimer = 60;
    em35WeakInit(em);
    em->setStatus(EM_STATUS_ACTIVE);
    em->setStatus(EM_STATUS_LOOK_ME);
    switch (em->set) {
    case 0:
    default:
        switch (em->type) {
        case 0:
        default:
            EstSet(em, -1, 0, 0, EFF_EM35, 3, 1, w->espKind, em, (void*) zero);
            EstSet(em, -1, 0, 0, EFF_EM35, 8, 1, w->espKind, em, (void*) zero);
            EmRoutineSet(em, 1, 1, zero, zero);
            MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 1, 0);
            MotionMove(em, 0);
            break;
        case 1:
            EstSet(em, -1, 0, 0, EFF_EM35, 4, 1, w->espKind, em, (void*) zero);
            EstSet(em, -1, 0, 0, EFF_EM35, 9, 1, w->espKind, em, (void*) zero);
            em->atari.throughOn();
            EmRoutineSet(em, 1, 0xF, zero, zero);
            MotionSetCore(em, MOTION(em), ARC(0x48), 0, 0, 1, 0);
            MotionMove(em, 0);
            break;
        }
        break;
    case 1:
        switch (em->type) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0xE, zero, zero);
            em->clearStatus(EM_STATUS_LOOK_ME);
            MotionSetCore(em, MOTION(em), ARC(0x46), 0, 0, 1, 0);
            MotionMove(em, 0);
            break;
        case 1:
            EstSet(em, -1, 0, 0, EFF_EM35, 4, 1, w->espKind, em, (void*) zero);
            EstSet(em, -1, 0, 0, EFF_EM35, 9, 1, w->espKind, em, (void*) zero);
            em->atari.throughOn();
            EmRoutineSet(em, 1, 0x1F, zero, zero);
            MotionSetCore(em, MOTION(em), ARC(0x88), 0, 0, 1, 0);
            MotionMove(em, 0);
            break;
        }
        break;
    }
    em35_R0_Move(em);
}

// Routine 1: runs the branch check and then the behaviour of the current r_no_1 state.
static void em35_R0_Move(cEm35* em)
{
    Em35_R1_move_tbl[em->r_no_1 * 2](em);
    Em35_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the states that have none.
static void em35_R1_br_dummy(cEm35* em)
{
}

// Routine 1/0xE (whole body, set 1): the divide scene. Placed at the scene spot in room 0x011F,
// it plays the tearing motion as a dead, non-colliding prop (hp 0, active status off) with the
// tearing effect and a squelch every 4 frames on motion event bit 0, then removes its effects and
// holds; the upper body (em35_R1_U_Divide) takes over from here.
static void em35_R1_Divide(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (pG->stage_no == 1 && pG->room_no == 0x1F) {
            Vec v;

            em->ang.y = PI;
            em->pos.x = 36500.0f;
            em->pos.y = -8000.0f;
            em->pos.z = -57970.0f;
            RotMatrix(em->mat, &em->ang);
            TransMatrix(em->mat, &em->pos);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -243.39f;
            PSMTXMultVec(em->mat, &v, &em->pos);
        }
        MotionSetCore(em, MOTION(em), ARC(0x46), ARC(0x47), 0, 1, 0);
        em->hp = 0;
        em->clearStatus(EM_STATUS_ACTIVE);
        em->atari.throughOn();
        w->timer = 0;
        EstSet(em, -1, 0, 0, EFF_EM35, 0x18, 1, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            if (w->timer) {
                w->timer--;
            } else {
                w->timer = 3;
                SndCall(8, 0xC, &em->pos, em->id, 0, em);
            }
        }
        if (MotionMove(em, 0)) {
            EffectEspDelete(1, w->espKind, em, 0);
            EffectEspgenDelete(1, w->espKind, em);
            EffectEfmDelete(1, w->espKind, em);
            em->r_no_2++;
        }
        break;
    }
}

// Routine 1/0x1F (upper body, set 1): the upper body tears itself free at the scene spot (flags
// 0x40 keeps the cloth off): the divide motion, a 90-frame hold, the get-up, then its idle effect and
// the first beam move (em35NextRtnSetUpper).
static void em35_R1_U_Divide(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (pG->stage_no == 1 && pG->room_no == 0x1F) {
            Vec v;

            em->ang.y = PI;
            em->pos.x = 36500.0f;
            em->pos.y = -8000.0f;
            em->pos.z = -57970.0f;
            RotMatrix(em->mat, &em->ang);
            TransMatrix(em->mat, &em->pos);
            v.x = 267.97f;
            v.y = 0.0f;
            v.z = -96.05f;
            PSMTXMultVec(em->mat, &v, &em->pos);
        }
        MotionSetCore(em, MOTION(em), ARC(0x88), 0, 0, 1, 0);
        em->r_no_2++;
    case 1:
        w->flags |= 0x40;
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        w->timer = 90;
        em->r_no_2++;
    case 3:
        w->flags |= 0x40;
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x89), 0, 0, 1, 0);
        em->r_no_2++;
    case 5:
        w->flags |= 0x40;
        if (MotionMove(em, 0)) {
            w->flags &= ~0x40;
            em->be_flag |= 0x00200000;
            EstSet(em, -1, 0, 0, EFF_EM35, 4, 1, w->espKind, em, 0);
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0 (whole body): idle loop (mirrored if the last motion was). With the player dead it
// still turns / walks to reach them; alive, once atkWait is out (cleared at once beyond 5 m) it
// punches a target behind it within 2.5 m (1/4), turns past 60 deg (1/3) or walks (1/1).
static void em35_R1_Wait(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = 5;

        if (em->Motion.Mot_attr & 0x40) {
            flip = 0x45;
        }
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 30, flip, 0);
        em->r_no_2++;
    }
    case 1:
        MotionMove(em, 0);
        if (EmDeadCk(em)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em35BigStepCk(em) == 0) {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if (em->l_pl > 25000000.0f) {
                w->atkWait = 0;
            }
            if (w->atkWait == 0) {
                if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else if (w->targetAngAbs > 1.0471976f) {
                    EmRoutineSet(em, 1, 3, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 1, 0, 0);
                }
            }
        }
        break;
    }
}

// Routine 1/1 (whole body): walks at the target (the run motion beyond 7 m; r_no_3 2 starts the
// loop mid-way), turning PI/64 per frame. A player 2 m above it gets the second-floor attack
// (1/0xB) within 4 m, else a big step. In front at 2..3.5 m with the lock wait out it picks the
// critical (1/9, half the time when the player is under 900 HP) or the hook / long punch / double
// punch. walkType (rolled per walk) 1 / 2 prefer the catch (1/0xC, r_no_3 1 within 1.5 m or when the
// player runs within 3.5 m), 0 the punch within 2.5 m (3.5 m at a running player); a target behind
// within 2.5 m is punched; otherwise em35BigStepCk looks for a step.
static void em35_R1_Walk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = 5;

        if (em->Motion.Mot_attr & 0x40) {
            flip = 0x45;
        }
        if (em->l_pl > 49000000.0f) {
            switch (em->r_no_3) {
            case 0:
            default:
                MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0xE), 10, flip, 0);
                break;
            case 1:
                MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0xE), 10, flip, 0);
                break;
            case 2:
                MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0xE), 10, flip, 0x4C);
                break;
            case 3:
                MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0xE), 10, flip, 0);
                break;
            }
        } else {
            switch (em->r_no_3) {
            case 0:
            default:
                MotionSetCore(em, MOTION(em), ARC(0xB), ARC(0xC), 10, flip, 0);
                break;
            case 1:
                MotionSetCore(em, MOTION(em), ARC(0xB), ARC(0xC), 10, flip, 0);
                break;
            case 2:
                MotionSetCore(em, MOTION(em), ARC(0xB), ARC(0xC), 10, flip, 0x5D);
                break;
            case 3:
                MotionSetCore(em, MOTION(em), ARC(0xB), ARC(0xC), 10, flip, 0);
                break;
            }
        }
        w->walkType = (u8) (Rnd() % 3);
        em->r_no_2++;
    }
    case 1:
        em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, 0.049087387f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (em->pos.y + 2000.0f < pPL->pos.y) {
            if (em->l_pl < 16000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                goto big_step;
            }
        } else if (w->routeAngAbs < 0.5235988f && em->l_pl > 4000000.0f && em->l_pl < 12250000.0f &&
                   w->lockWait == 0) {
            if ((s16) pG->pl_life <= 900 && (Rnd() & 1)) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                switch ((u8) (Rnd() % 3)) {
                case 0:
                    EmRoutineSet(em, 1, 8, 0, 0);
                    break;
                case 1:
                    EmRoutineSet(em, 1, 5, 0, 0);
                    break;
                case 2:
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
            }
        } else if (w->walkType) {
            if (em->l_pl < 2250000.0f && w->routeAngAbs < 0.7853982f) {
                EmRoutineSet(em, 1, 0xC, 0, 1);
            } else if (em35bPlRunCk(em) && em->l_pl < 12250000.0f && w->routeAngAbs < 0.7853982f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else {
                goto turn_ck;
            }
        } else {
            if (w->routeAngAbs < 1.0471976f &&
                (em->l_pl < 6250000.0f || (em35bPlRunCk(em) && em->l_pl < 12250000.0f))) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
            turn_ck:
                if (w->routeAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                big_step:
                    em35BigStepCk(em);
                }
            }
        }
        break;
    }
}

// Routine 1/2 (whole body): the long stride that closes distance (the far variant, r_no_3 1, beyond
// 9 m), with the footfall effects on motion events 0 / 1. On event bit 2 (the foot lands) it runs the
// same attack selection as em35_R1_Walk; when the motion ends it punches behind, turns or walks.
static void em35_R1_BigStep(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip = 5;

        if (em->Motion.Mot_attr & 0x40) {
            flip = 0x45;
        }
        if (w->targetDist > 81000000.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x42), ARC(0x43), 10, flip, 0);
            em->r_no_3 = 1;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x44), ARC(0x45), 10, flip, 0);
            em->r_no_3 = 0;
        }
        w->walkType = (u8) (Rnd() % 3);
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if (em->Motion.Seq_old.Free & 4) {
                if (w->routeAngAbs < 0.5235988f && em->l_pl > 4000000.0f && em->l_pl < 12250000.0f &&
                    w->lockWait == 0) {
                    if ((s16) pG->pl_life <= 900 && (Rnd() & 1)) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        switch ((u8) (Rnd() % 3)) {
                        case 0:
                            EmRoutineSet(em, 1, 8, 0, 0);
                            break;
                        case 1:
                            EmRoutineSet(em, 1, 5, 0, 0);
                            break;
                        case 2:
                            EmRoutineSet(em, 1, 6, 0, 0);
                            break;
                        }
                    }
                } else if (w->walkType) {
                    if (em->l_pl < 2250000.0f && w->routeAngAbs < 0.7853982f) {
                        EmRoutineSet(em, 1, 0xC, 0, 1);
                    } else if (em35bPlRunCk(em) && em->l_pl < 12250000.0f && w->routeAngAbs < 0.7853982f) {
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                    } else {
                        goto turn_ck;
                    }
                } else {
                    if (w->routeAngAbs < 1.0471976f &&
                        (em->l_pl < 6250000.0f || (em35bPlRunCk(em) && em->l_pl < 12250000.0f))) {
                        EmRoutineSet(em, 1, 4, 0, 0);
                    } else {
                    turn_ck:
                        if (w->routeAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                            EmRoutineSet(em, 1, 4, 0, 0);
                        } else {
                            goto se;
                        }
                    }
                }
            } else {
            se:
                if (em->Motion.Seq_old.Free & 1) {
                    EstSet(em, -1, 0, 0, EFF_EM35, 0x1C, 0, ESP_CORE_KIND_NONE, em, 0);
                }
                if (em->Motion.Seq_old.Free & 2) {
                    if (em->r_no_3) {
                        EstSet(em, -1, 0, 0, EFF_EM35, 0x1D, 0, ESP_CORE_KIND_NONE, em, 0);
                    } else {
                        EstSet(em, -1, 0, 0, EFF_EM35, 0x1E, 0, ESP_CORE_KIND_NONE, em, 0);
                    }
                }
            }
        }
        break;
    }
}

// Routine 1/3 (whole body): the turn towards the target: the about-face past 130 deg (r_no_3 1),
// else the left / right turn (2 / 3; the mirrored motion set swaps them). The neck follows from
// motion event bit 3 on. Ends in the catch within 1.5 m, the punch behind, another turn, a big step
// or the walk.
static void em35_R1_Turn(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0: {
        int flip = 1;

        if (em->Motion.Mot_attr & 0x40) {
            flip = 0x41;
        }
        if (w->targetAngAbs > 2.268928f) {
            MotionSetCore(em, MOTION(em), ARC(0xF), ARC(0x10), 10, flip, 0);
            em->r_no_3 = 1;
        } else if (w->targetAng < 0.0f) {
            if (flip & 0x40) {
                MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x14), 10, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x11), ARC(0x12), 10, flip, 0);
            }
            em->r_no_3 = 2;
        } else {
            if (flip & 0x40) {
                MotionSetCore(em, MOTION(em), ARC(0x11), ARC(0x12), 10, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x14), 10, flip, 0);
            }
            em->r_no_3 = 3;
        }
        em->r_no_2++;
    }
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            w->flags |= 0x10;
        }
        if (MotionMove(em, 0)) {
            if (em->l_pl < 2250000.0f && w->routeAngAbs < 0.7853982f) {
                EmRoutineSet(em, 1, 0xC, 0, 1);
            } else if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em35BigStepCk(em) == 0) {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/4 (whole body): the punch with the arm on the player's side (r_no_3 0 right / 1 the
// mirrored left; the wide swing with its effect when the target is more than 90 deg off). flags 0x80
// holds off light damage for 60 frames. On motion event bit 2 the duck prompt (em35AtkEscapeAction)
// is offered to a player in front within 5 m; the hit (attack 0 / 1) sweeps the arm and shoulder
// parts on event bit 0. A miss awards the escape point; a hit rests 90 frames in Wait, else punch
// behind / turn / walk.
static void em35_R1_Atk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->routeAng < 0.0f) {
            em->r_no_3 = 0;
            if (w->routeAngAbs < 1.5707964f) {
                MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x1A), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1B), ARC(0x1C), 10, 1, 0);
                EstSet(em, -1, 0, 0, EFF_EM35, 0x13, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        } else {
            em->r_no_3 = 1;
            if (w->routeAngAbs < 1.5707964f) {
                MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x1A), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1B), ARC(0x1C), 10, 0x41, 0);
                EstSet(em, -1, 0, 0, EFF_EM35, 0x13, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        }
        w->timer = 30;
        w->timer2 = 8;
        w->atkTimer = 60;
        w->atkHit = 0;
        w->atkHit2 = 0;
        em->r_no_2++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if ((em->Motion.Seq_old.Free & 4) && w->routeAngAbs < 1.5707964f && em->l_pl < 25000000.0f) {
                ActBtn.set(ACT_STOOP, 0xB, (void*) em35AtkEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
            }
            if (em->Motion.Seq_old.Free & 1) {
                if (em->r_no_3 == 0) {
                    em35AtkCk(em, 0, 0x11);
                    em35AtkCk(em, 0, 0x12);
                    em35AtkCk(em, 0, 0x13);
                    em35AtkCk(em, 0, 0x14);
                    em35AtkCk(em, 0, 0x2C);
                    em35AtkCk(em, 0, 0x2D);
                    em35AtkCk(em, 0, 0x2E);
                    em35AtkCk(em, 0, 0x2F);
                } else {
                    em35AtkCk(em, 1, 0x17);
                    em35AtkCk(em, 1, 0x18);
                    em35AtkCk(em, 1, 0x19);
                    em35AtkCk(em, 1, 0x1A);
                    em35AtkCk(em, 1, 0x26);
                    em35AtkCk(em, 1, 0x27);
                    em35AtkCk(em, 1, 0x28);
                    em35AtkCk(em, 1, 0x29);
                }
            }
        }
        break;
    }
}

// Branch check of the double punch: the right arm (attack 3) on motion event bit 0, the left on
// bit 1; a hit goes straight to the bear hug (1/7).
static void em35_R1_br_AtkDouble(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->Motion.Seq_old.Free & 1) {
        em35AtkCk(em, 3, 0x11);
        em35AtkCk(em, 3, 0x12);
        em35AtkCk(em, 3, 0x13);
        em35AtkCk(em, 3, 0x14);
        em35AtkCk(em, 3, 0x2C);
        em35AtkCk(em, 3, 0x2D);
        em35AtkCk(em, 3, 0x2E);
        em35AtkCk(em, 3, 0x2F);
        if (w->atkHit) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return;
        }
    }
    if (em->Motion.Seq_old.Free & 2) {
        em35AtkCk(em, 3, 0x17);
        em35AtkCk(em, 3, 0x18);
        em35AtkCk(em, 3, 0x19);
        em35AtkCk(em, 3, 0x1A);
        em35AtkCk(em, 3, 0x26);
        em35AtkCk(em, 3, 0x27);
        em35AtkCk(em, 3, 0x28);
        em35AtkCk(em, 3, 0x29);
        if (w->atkHit) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
    }
}

// Routine 1/6 (whole body): the two-handed punch combination (hits in em35_R1_br_AtkDouble), which
// locks the double / hook attacks for 450 frames. Duck prompts are offered on motion events 2 and 4
// (the second one with a different icon once the player has dodged the first, atkHit2 -> walkType).
// Exits like em35_R1_Atk.
static void em35_R1_AtkDouble(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), ARC(0x18), 10, 1, 0);
        w->lockWait = 450;
        w->atkHit = 0;
        w->atkHit2 = 0;
        w->walkType = 0;
        w->atkTimer = 60;
        em->r_no_2++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if (w->atkHit2) {
                w->walkType = 1;
            }
            if ((em->Motion.Seq_old.Free & 4) && w->routeAngAbs < 1.5707964f && em->l_pl < 25000000.0f) {
                ActBtn.set(ACT_STOOP, 0xB, (void*) em35AtkEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
            }
            if ((em->Motion.Seq_old.Free & 0x10) && w->atkHit == 0 && w->routeAngAbs < 1.5707964f &&
                em->l_pl < 25000000.0f) {
                if (w->walkType) {
                    ActBtn.set(ACT_STOOP, 0xB, (void*) em35AtkEscapeAction, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                } else {
                    ActBtn.set(ACT_STOOP, 0xB, (void*) em35AtkEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);
                }
            }
        }
        break;
    }
}

// Routine 1/7 (whole body): the bear hug after a double-punch hit. Snaps the player to the grab
// spot (em35CatchPosSet) and takes him over (plem35_BearHug); during the squeeze (motion event bit 1)
// the button mash runs and the player loses 10 HP per frame: a mash count over 50 at event bit 2
// frees him (the release motion, then punch behind / turn / walk), event bit 0 is the kill.
static void em35_R1_BearHug(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        em35CatchPosSet(em);
        MotionSetCore(em, MOTION(em), ARC(0x36), ARC(0x37), 5, 1, 0);
        SetPlDamage(em, plem35_BearHug);
        PlSetDamageSe(0);
        PlGachaInit();
        em->dmg.set(0, 0);
        EstSet(em, -1, 0, 0, EFF_EM35, 0x1F, 0, ESP_CORE_KIND_NONE, em, 0);
        EstSet(pPL, -1, 0, 0, EFF_EM35, 0x22, 0, ESP_CORE_KIND_NONE, pPL, 0);
        w->timer = 145;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->Motion.Seq_old.Free & 2) {
            PlGachaMove();
            LifeDownSet2(pPL, 10, 0, 1);
            if ((em->Motion.Seq_old.Free & 4) && (u32) PlGachaGet() > 50) {
                em->r_no_2 = 2;
            } else if (em->Motion.Seq_old.Free & 1) {
                pG->pl_life = 0;
                EstSet(em, -1, 0, 0, EFF_EM35, 0x20, 0, ESP_CORE_KIND_NONE, em, 0);
                EstSet(pPL, -1, 0, 0, EFF_EM35, 0x23, 0, ESP_CORE_KIND_NONE, pPL, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x38), ARC(0x39), 0, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
    em->Catch_at_adj = em->pos;
}


// Player damage callback of the bear hug (dmType 10). Step 0/1: placed 3.5 m in front of the enemy
// facing it, the squeezed motion with the crush sounds / rumble at frames 73 and 158, its step
// mirroring the enemy's r_no_2; the damage ends as soon as the enemy leaves routine 1/7. Step 2/3:
// the break-free motion at the enemy's feet with the relief voice.
static void plem35_BearHug(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->subArc = pPL->pEmCatch->subArc;
    pl->dmg.m_Timer = 10;
    switch (pl->r_no_2) {
    case 0: {
        Vec v;
        f32 y;

        pl->atari.m_flag &= ~0x300;
        v.x = -101.81f;
        v.y = 0.0f;
        v.z = 3473.71f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x93), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->dmg.set(0, 0);
        pl->r_no_2++;
    }
    case 1:
        MotionMove(pl, 0);
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 7) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->Motion.Seq_frame > 72.7f && pl->Motion.Seq_frame < 73.3f) {
                pl->m_Work0 = SndCall(8, 0x4B, &pl->pos, pl->pEmCatch->id, 0, pl);
                VibSetData(VIB_TBL, 0xB, 1);
            }
            if (pl->Motion.Seq_frame > 157.7f && pl->Motion.Seq_frame < 158.3f) {
                pl->m_Work0 = SndCall(8, 0x4D, &pl->pos, pl->pEmCatch->id, 0, pl);
                VibSetData(VIB_TBL, 0xB, 1);
            }
        }
        break;
    case 2: {
        Vec v;
        f32 y;

        v.x = 30.01f;
        v.y = 0.0f;
        v.z = 244.65f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x94), 0, 0, 1, 0);
        SndStop(pl->m_Work0, 0);
        SndCall(1, 0x3D, &pPL->pos, pPL->id, 0, pPL);
        pl->r_no_2++;
    }
    case 3:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 7) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Action button of the punches: the player ducks (plem35Sit, atkHit2 marks the dodge) and gets a
// critical-hit rank point.
static void em35AtkEscapeAction(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->atkHit2 = 1;
    SetPlDamage(em, plem35Sit);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the duck: the crouch motion (invulnerable for 30 frames) with an
// escape rank point; ends with the motion.
static void plem35Sit(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x9A), 0, 3, 1, 0);
        pl->dmg.set(0, 30);
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Routine 1/0xB (whole body): the attack on a player standing on the upper floor. Steps: turn to
// the nearest of the four cardinal directions (jumpAng; r_no_3 picks the mirrored set), then a punch
// (attack 5, 50 % when facing the player) or the up-swing (attack 6) with the hit on motion event
// bit 0 (plem35DmFall2F knocks the player off the ledge), then the recovery; ends like em35_R1_Atk.
static void em35_R1_Atk2F(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0: {
        f32 ang;
        f32 abs;

        w->atkHit = 0;
        ang = GetXZAngle(&em->pos, &pPL->pos);
        abs = fabsf(ang);
        if (ang < 0.0f) {
            w->jumpAng = -1.5707964f;
        } else {
            w->jumpAng = 1.5707964f;
        }
        if (abs < 0.7853982f) {
            w->jumpAng = 0.0f;
        }
        if (abs > 2.3561945f) {
            w->jumpAng = PI;
        }
        if (Muku2(w->jumpAng, ang, PI) < 0.0f) {
            em->r_no_3 = 1;
            MotionSetCore(em, MOTION(em), ARC(0x26), ARC(0x27), 10, 0x41, 0);
        } else {
            em->r_no_3 = 0;
            MotionSetCore(em, MOTION(em), ARC(0x26), ARC(0x27), 10, 1, 0);
        }
        em->r_no_2++;
    }
    case 1:
        em->ang.y += Muku2(em->ang.y, w->jumpAng, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (w->routeAngAbs < 0.7853982f && (Rnd() & 1)) {
            if (em->r_no_3) {
                MotionSetCore(em, MOTION(em), ARC(0x15), ARC(0x16), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x15), ARC(0x16), 10, 1, 0);
            }
        } else {
            if (em->r_no_3) {
                MotionSetCore(em, MOTION(em), ARC(0x28), ARC(0x29), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x28), ARC(0x29), 10, 1, 0);
            }
        }
        SndCall(8, 0x66, &em->pos, em->id, 0, em);
        w->timer = 10;
        w->atkHit = 0;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em->r_no_2++;
        } else if (em->Motion.Seq_old.Free & 1) {
            em35AtkCk(em, 5, 0x2C);
            em35AtkCk(em, 5, 0x2D);
            em35AtkCk(em, 5, 0x2E);
            em35AtkCk(em, 5, 0x2F);
            em35AtkCk(em, 6, 0x26);
            em35AtkCk(em, 6, 0x27);
            em35AtkCk(em, 6, 0x28);
            em35AtkCk(em, 6, 0x29);
        }
        break;
    case 4:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x2A), ARC(0x2B), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x2A), ARC(0x2B), 10, 1, 0);
        }
        w->timer = 10;
        w->atkHit = 0;
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// Player damage callback of the upper-floor hit (dmType 10): the knocked-off-the-ledge motion
// with the collision passed through, the impact sounds / rumble at frame 40, and (if alive) the
// return to the standing routine 1/0 step 0xA when it lands.
static void plem35DmFall2F(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 10;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x92), 0, 3, 1, 0);
        pl->atari.throughOn();
        if ((s16) pG->pl_life > 0) {
            PlSetDamageSe(0);
        } else {
            PlSetDamageSe(0xD);
        }
        EstSet(pl, -1, 0, 0, EFF_EM35, 0x16, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            pl->atari.throughOff();
            EmRoutineSet(pPL, 1, 0, 0xA, 0);
        } else if (pl->Motion.Seq_frame > 39.7f && pl->Motion.Seq_frame < 40.3f) {
            SndCall(5, 5, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 0x12, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 7, &pPL->pos, pPL->id, 0, pPL);
            VibSetData(VIB_TBL, 0xB, 1);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Routine 1/5 (whole body): the long lunging punch (attack 2 swept along the arm on motion event
// bit 0), homing for 15 frames, locking the special attacks 450 frames. The dash-aside prompt
// (em35DashEscapeAction) is offered on event bit 2 to a player in front within 8 m; the landing
// effect plays on event bit 5. Exits like em35_R1_Atk.
static void em35_R1_LongAtk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x1D), ARC(0x1E), 10, 1, 0);
        w->timer = 15;
        w->atkHit = 0;
        w->lockWait = 450;
        w->atkTimer = 60;
        em->r_no_2++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, 0.049087387f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if ((em->Motion.Seq_old.Free & 4) && w->routeAngAbs < 1.5707964f && em->l_pl < 64000000.0f) {
            ActBtn.set(ACT_GUARD, 0xB, (void*) em35DashEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
        }
        if (em->Motion.Seq_old.Free & 0x20) {
            EstSet(em, -1, 0, 0, EFF_EM35, 0x12, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            em35AtkCk(em, 2, 0x11);
            em35AtkCk(em, 2, 0x12);
            em35AtkCk(em, 2, 0x13);
            em35AtkCk(em, 2, 0x14);
            em35AtkCk(em, 2, 0x2C);
            em35AtkCk(em, 2, 0x2D);
            em35AtkCk(em, 2, 0x2E);
            em35AtkCk(em, 2, 0x2F);
            em35AtkCk(em, 2, 0x17);
            em35AtkCk(em, 2, 0x18);
            em35AtkCk(em, 2, 0x19);
            em35AtkCk(em, 2, 0x1A);
            em35AtkCk(em, 2, 0x26);
            em35AtkCk(em, 2, 0x27);
            em35AtkCk(em, 2, 0x28);
            em35AtkCk(em, 2, 0x29);
        }
        break;
    }
}

// Routine 1/8 (whole body): the hook (attack 4 on motion event bit 0, plem35DmHook on a hit),
// homing for 15 frames, locking the special attacks 450 frames. A hit follows up with the catch
// (within 1.5 m in front, 50 %) or the punch within 3.5 m; otherwise turn / walk.
static void em35_R1_Hook(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x2C), ARC(0x2D), 10, 1, 0);
        w->timer = 15;
        w->timer2 = 30;
        w->atkHit = 0;
        w->lockWait = 450;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &w->targetPos, em->ang.y, 0.049087387f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->atkHit) {
                if (em->l_pl < 2250000.0f && w->routeAngAbs < 0.7853982f && (Rnd() & 1)) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                } else if (em->l_pl < 12250000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    goto far;
                }
            } else {
            far:
                if (w->targetAngAbs > 1.0471976f) {
                    if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                        EmRoutineSet(em, 1, 4, 0, 0);
                    } else if (w->targetAngAbs > 1.0471976f) {
                        EmRoutineSet(em, 1, 3, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 3, 0, 0);
                    }
                } else {
                    EmRoutineSet(em, 1, 1, 0, 0);
                }
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            em35AtkCk(em, 4, 0x11);
            em35AtkCk(em, 4, 0x12);
            em35AtkCk(em, 4, 0x13);
            em35AtkCk(em, 4, 0x14);
            em35AtkCk(em, 4, 0x2C);
            em35AtkCk(em, 4, 0x2D);
            em35AtkCk(em, 4, 0x2E);
            em35AtkCk(em, 4, 0x2F);
            em35AtkCk(em, 4, 0x17);
            em35AtkCk(em, 4, 0x18);
            em35AtkCk(em, 4, 0x19);
            em35AtkCk(em, 4, 0x1A);
            em35AtkCk(em, 4, 0x26);
            em35AtkCk(em, 4, 0x27);
            em35AtkCk(em, 4, 0x28);
            em35AtkCk(em, 4, 0x29);
        }
        break;
    }
}

// Player damage callback of the hook: the knock-back motion (dmType 30) with the pain face and
// sound; ends with the motion.
static void plem35DmHook(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x91), 0, 3, 1, 0);
        pl->dmg.m_Timer = 30;
        PlSetDamageSe(0);
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Branch check of the critical: a hit registered by em35AtkCk (attack 7) on motion event bit 0
// rumbles and switches to CriticalHit (1/0xA), the kill.
static void em35_R1_br_Critical(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->hp > 0 && (em->Motion.Seq_old.Free & 1)) {
        em35AtkCk(em, 7, 0x2C);
        em35AtkCk(em, 7, 0x2D);
        em35AtkCk(em, 7, 0x2E);
        em35AtkCk(em, 7, 0x2F);
        if (w->atkHit) {
            VibSetData(VIB_TBL, 0xB, 1);
            em->dmg.set(0, 2);
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
    }
}

// Turn towards the player with a slight lead (em35_R1_Critical). A macro, not an inline: the PI
// pool load is issued above the preceding `timer--` store (an inlined body's pool loads lose
// RTX_UNCHANGING_P and sink below it).
// `ang` is the routine's variable (one pseudo for both expansions): a global pseudo takes the copy
// preference f1 (ascending scan) where a block-local one takes f2 (allocation order), which decides
// whether the `fmr f2,f1` argument copy sits before or after the `lis` of Muku2's limit.
#define EM35_CRITICAL_TURN(em, v)                                                                   \
    {                                                                                               \
        v = LIMIT_ANGLE((em)->ang.y + Muku(&(em)->pos, &pPL->pos, (em)->ang.y, PI) + 0.05235988f);   \
        (em)->ang.y += Muku2((em)->ang.y, v, 0.09817477f);                                          \
        (em)->ang.y = LIMIT_ANGLE((em)->ang.y);                                                     \
    }

// Routine 1/9 (whole body): the one-hit-kill charge, chosen against a weakened player. Steps: the
// wind-up tracking the player (with a 3 deg lead) for 40 frames, then the charge tracking for 10 more
// (the hit is in em35_R1_br_Critical, the dash-aside prompt on motion event bit 2). Locks the
// special attacks 450 frames; when it ends the player escaped (rank point), then Wait or Walk.
static void em35_R1_Critical(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    f32 ang;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x22), ARC(0x23), 10, 1, 0);
        w->timer = 40;
        w->atkHit = 0;
        w->lockWait = 450;
        em->r_no_2++;
    case 1:
        w->flags |= 0x80;
        if (w->timer) {
            w->timer--;
            EM35_CRITICAL_TURN(em, ang);
        }
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x24), ARC(0x25), 10, 1, 0);
        w->timer = 10;
        w->atkHit = 0;
        w->lockWait = 450;
        w->atkTimer = 60;
        em->r_no_2++;
    case 3:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (w->timer) {
            w->timer--;
            EM35_CRITICAL_TURN(em, ang);
        }
        if (em->Motion.Seq_old.Free & 4) {
            ActBtn.set(ACT_GUARD, 0xB, (void*) em35DashEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
        }
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/0xA (whole body): the critical connected: the kill motion with the player taken over
// (plem35_CriticalHit) and the death sound; holds when it ends.
static void em35_R1_CriticalHit(cEm35* em)
{
    em->dmg.set(0, 10);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x40), ARC(0x41), 0, 1, 0);
        SetPlDamage(em, plem35_CriticalHit);
        PlSetDamageSe(0xD);
        em->r_no_2++;
    case 1:
        em->dmg.set(0, 2);
        MotionMove(em, 0);
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage callback of the critical kill: placed in front of the enemy facing it (collision
// blocking off), the impaled motion with its blood effect; the player dies 100 frames in.
static void plem35_CriticalHit(cPlayer* pl)
{
    f32 y;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0: {
        Vec v;

        pl->atari.m_flag &= ~0x300;
        v.x = 755.75f;
        v.y = 0.0f;
        v.z = 2621.64f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x90), 0, 0, 1, 0);
        PlSetFace(1);
        pl->m_Work0 = 100;
        EstSet(pl, -1, 0, 0, EFF_EM35, 0x10, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    }
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
            if (pl->m_Work0 == 0) {
                pG->pl_life = 0;
            }
        }
        MotionMove(pl, 0);
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Action button of the lunge / critical charge: the player dashes aside (plem35DashEscape) and
// gets a critical-hit rank point.
static void em35DashEscapeAction(cEm35* em)
{
    SetPlDamage(em, plem35DashEscape);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the dash aside (dmType 10): the roll to the side away from the enemy
// (mirrored by which side it is on), turning to face it for 15 frames, under the event camera
// (em35EscapeCamMove), with the roll effect / sounds at frame 12; ends after 50 frames.
static void plem35DashEscape(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 10;
    switch (pl->r_no_2) {
    case 0:
        if (Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, PI) < 0.0f) {
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x98), EM_ARC(pl, 0x99), 3, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x98), EM_ARC(pl, 0x99), 3, 0x41, 0);
        }
        GameAddPoint(LVADD_ESCAPEATTACK);
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pl->m_Work0 = 50;
        pl->m_Work1 = 15;
        pl->r_no_2++;
    case 1:
        em35EscapeCamMove((cEm35*)pl->pEmCatch);
        if (pl->m_Work1) {
            pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 0.19634955f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        MotionMove(pl, 0);
        if (pl->Motion.Seq_frame > 11.7f && pl->Motion.Seq_frame < 12.3f) {
            EstSet(0, -1, &pl->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Event camera of the dash aside: eases from the current camera to a spot behind the player's
// right shoulder looking past him, pulled in 25 cm short of any wall (EatMgr), and installs itself as
// the extra camera.
void em35EscapeCamMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    GlobalWork* g = pG;
    Vec a;
    Vec b;
    Vec c;

    w->cam.param.fovy = g->Camera.param.fovy;
    a.x = -376.0f;
    a.y = 575.0f;
    a.z = -1831.0f;
    b.x = -244.0f;
    b.y = 809.0f;
    b.z = 52.6f;
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&g->Camera.param.at, &b, &w->cam.param.at, 1.0f);
    PosToPos(&g->Camera.param.pos, &a, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &c, 0, 0x8000, 0)) {
        Vec d;
        f32 len;

        PSVECSubtract(&c, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 3756 "D:/Bio4/Prog/em35.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.Up.x = 0.0f;
    w->cam.Up.y = 1.0f;
    w->cam.Up.z = 0.0f;
    w->cam.Distance = VEC_DIST(&w->cam.param.pos, &w->cam.param.at);
    CameraSetOrientationUp(&w->cam);
    CamCtrl.m_pExtraCamera = (s32) &w->cam;
}

// Player damage callback of the upper body's stamp (dmType 10): the crushed motion with its blood
// effect under the stamp camera; when it ends a living player returns to the standing routine.
static void plem35DmStamp(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 10;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x97), 0, 3, 1, 0);
        PlSetFace(1);
        pl->atari.m_flag &= ~0x200;
        if ((s16) pG->pl_life > 0) {
            PlSetDamageSe(0);
        } else {
            PlSetDamageSe(0xD);
        }
        EstSet(pl, -1, 0, 0, EFF_EM35, 0x11, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    case 1:
        em35StampCamMove((cEm35*)pl->pEmCatch);
        if (MotionMove(pl, 0)) {
            if ((s16) pG->pl_life > 0) {
                EmRoutineSet(pPL, 1, 0, 0xA, 0);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Event camera of the stamp: eases towards a point 3 m up and 3 m behind the player, looking at
// his root part, and installs itself as the extra camera.
void em35StampCamMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    GlobalWork* g = pG;
    Camera* cam = &w->cam;
    Vec a;
    cParts* p;

    w->cam.param.fovy = g->Camera.param.fovy;
    a.x = 0.0f;
    a.y = 3000.0f;
    a.z = -3000.0f;
    PSMTXMultVec(pPL->mat, &a, &a);
    PosToPos(&g->Camera.param.pos, &a, &w->cam.param.pos, 0.1f);
    p = pPL->getPartsPtr(0);
    PosToPos(&g->Camera.param.at, &p->world, &w->cam.param.at, 0.3f);
    w->cam.Up.x = 0.0f;
    w->cam.Up.y = 1.0f;
    w->cam.Up.z = 0.0f;
    w->cam.Distance = VEC_DIST(&w->cam.param.pos, &w->cam.param.at);
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// Branch check of the catch: on motion event bit 1 with the player in the grab zone
// (em35CatchCk) it rumbles and switches to CatchHit (1/0xD; r_no_3 1 for the mirrored motion).
static void em35_R1_br_Catch(cEm35* em)
{
    if (em->hp > 0 && (em->Motion.Seq_old.Free & 2) && em35CatchCk(em)) {
        VibSetData(VIB_TBL, 7, 1);
        if (em->Motion.Mot_attr & 0x40) {
            EmRoutineSetW(em, 1, 0xD, 0, 1);
        } else {
            EmRoutineSetW(em, 1, 0xD, 0, 0);
        }
    }
}

// Routine 1/0xC (whole body): the grab reach (mirrored to the player's side; the hit lives in
// em35_R1_br_Catch), light damage held off 45 frames. A miss awards the escape point, then punch
// behind / turn / walk.
static void em35_R1_Catch(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x1F), ARC(0x20), 10, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1F), ARC(0x20), 10, 0x41, 0);
        }
        w->atkTimer = 45;
        em->r_no_2++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/0xD (whole body): the player is caught. Snapped to the grab spot and taken over
// (plem35_CatchHit), the lift motion runs the button mash; the slam on motion event bit 0 costs 250
// HP. At frame 100 or the motion end: a mash count over 49 on a living player breaks free (step 2:
// the release motion), otherwise step 4: the crushing throw for 500 damage, then a 90-frame Wait.
static void em35_R1_CatchHit(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.clrFlag100();
        em35CatchPosSet(em);
        MotionSetCore(em, MOTION(em), ARC(0x34), ARC(0x35), 5, 1, 0);
        SetPlDamage(em, plem35_CatchHit);
        PlSetDamageSe(0);
        w->timer = 15;
        w->timer2 = 40;
        PlGachaInit();
        em->dmg.set(0, 0);
        em->r_no_2++;
    case 1:
        PlGachaMove();
        if (em->Motion.Seq_old.Free & 1) {
            LifeDownSet2(pPL, 250, 0, 1);
            SndCall(5, 5, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 0x12, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 7, &pPL->pos, pPL->id, 0, pPL);
            VibSetData(VIB_TBL, 0xB, 1);
        }
        if (MotionMove(em, 0)) {
            if ((u32) PlGachaGet() <= 49 || (s16) pG->pl_life <= 1) {
                em->r_no_2 = 4;
            } else {
                em->r_no_2 = 2;
            }
        } else if (em->Motion.Seq_frame > 99.7f && em->Motion.Seq_frame < 100.3f) {
            if ((u32) PlGachaGet() <= 49 || (s16) pG->pl_life <= 1) {
                em->r_no_2 = 4;
            } else {
                em->r_no_2 = 2;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x3E), ARC(0x3F), 0, 1, 0);
        em->atari.setFlag100();
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x3C), ARC(0x3D), 0, 1, 0);
        AtariOnV(&em->atari, 0x100);
        LifeDownSet2(pPL, 500, 0, 0);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            w->atkWait = 90;
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage callback of the catch (dmType 2). Step 0/1: held 1.8 m in front of the enemy in
// the lifted motion, the step mirroring the enemy's r_no_2, ending as soon as the enemy leaves 1/0xD.
// Step 2/3: the break-free drop with the relief voice. Step 4/5: the thrown motion with its blood
// effect and impact sounds / rumble at frames 39 and 78, then (alive) the get-up in step 6.
static void plem35_CatchHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->subArc = pPL->pEmCatch->subArc;
    pl->dmg.m_Timer = 2;
    switch (pl->r_no_2) {
    case 0: {
        cAtariInfo* at;
        Vec v;
        f32 y;

        pl->atari.m_flag &= ~0x300;
        at = &pl->atari;
        at->clrFlag100();
        v.x = -61.37f;
        v.y = 0.0f;
        v.z = 1774.91f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x8C), 0, 5, 1, 0);
        PlSetFace(1);
        at->set(10, 480.00003f, 400.0f);
        EstSet(pl, -1, 0, 0, EFF_EM35, 0xE, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    }
    case 1:
        MotionMove(pl, 0);
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0xD) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 2: {
        Vec v;
        f32 y;

        v.x = -406.3f;
        v.y = 0.0f;
        v.z = 1014.0f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x8F), 0, 0, 1, 0);
        SndCall(1, 0x3D, &pPL->pos, pPL->id, 0, pPL);
        pl->r_no_2++;
    }
    case 3:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0xD) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 4: {
        Vec v;
        f32 y;

        v.x = -406.3f;
        v.y = 0.0f;
        v.z = 1014.0f;
        pl->ang.x = 0.0f;
        y = pl->pEmCatch->ang.y;
        pl->ang.z = 0.0f;
        pl->ang.y = y + PI;
        LIMIT_ANGLE(pl->ang.y);
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x8E), 0, 0, 1, 0);
        EstSet(pl, -1, 0, 0, EFF_EM35, 0xF, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->r_no_2++;
    }
    case 5:
        pl->dmg.set(0, 2);
        if (MotionMove(pl, 0)) {
            if ((s16) pG->pl_life > 0) {
                pl->r_no_2++;
            }
        } else {
            if (pl->Motion.Seq_frame > 38.7f && pl->Motion.Seq_frame < 39.3f) {
                SndCall(8, 0x13, &pPL->pos, pl->pEmCatch->id, 0, pPL);
                SndCall(8, 0x58, &pl->pos, pl->pEmCatch->id, 0, pl);
                VibSetData(VIB_TBL, 0xB, 1);
            }
            if (pl->Motion.Seq_frame > 77.7f && pl->Motion.Seq_frame < 78.3f) {
                SndCall(8, 0x14, &pPL->pos, pl->pEmCatch->id, 0, pPL);
                SndCall(8, 0x59, &pl->pos, pl->pEmCatch->id, 0, pl);
                VibSetData(VIB_TBL, 0xB, 1);
                if ((s16) pG->pl_life <= 0) {
                    PlSetDamageSe(0xD);
                }
            }
        }
        break;
    case 6:
        pl->atari.m_flag |= 0x300;
        MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x8A), 0, 0, 1, 0);
        SndCall(1, 0x29, &pl->getPartsPtr(0)->world, 0, 0, pPL);
        SndCall(1, 4, &pl->pos, 0, 0, pl);
        pl->r_no_2++;
    case 7:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->Motion.Seq_frame > 21.7f && pl->Motion.Seq_frame < 22.3f) {
                SndCall(5, 2, &pl->pos, 0, 0, pl);
            }
            if (pl->Motion.Seq_frame > 19.7f && pl->Motion.Seq_frame < 20.3f) {
                SndCall(5, 3, &pl->pos, 0, 0, pl);
            }
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Routine 1/0xF (upper body): idles on its beam with one of two loops; at each loop end, beyond
// 5 m it may (1 in 4) drop to the beam below (1/0x1A) or climb to the one above (1/0x19), else
// em35NextRtnSetUpper picks the next beam move.
static void em35_R1_U_Wait(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0x48), 0, 10, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x49), 0, 10, 5, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if ((Rnd() & 3) == 0 && em->l_pl > 25000000.0f && em35BeamDownCk(em, w->beamNo)) {
                EmRoutineSet(em, 1, 0x1A, 0, 0);
            } else if ((Rnd() & 3) == 0 && em->l_pl > 25000000.0f && em35BeamUpCk(em, w->beamNo)) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            } else {
                em35NextRtnSetUpper(em);
            }
        }
        break;
    }
}

// Routine 1/0x10 (upper body): the forward leap along the beam, or (facing a player within 7 m at
// beam height, 50 %, Game_level above 1) the leaping hand swipe on the player's side (r_no_3 picks
// the mirrored set; attacks 8 / 9 swept along the hand parts on motion event bit 0, the duck prompt
// on event bit 2). A hit continues with em35NextRtnSetUpper2, a miss with em35NextRtnSetUpper.
static void em35_R1_U_Jump(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        f32 dy = fabsf(em->pos.y - pPL->pos.y);

        if (w->routeAngAbs < 0.5235988f && em->l_pl < 49000000.0f && dy < 500.0f && (Rnd() & 1) &&
            pG->Game_level > 1) {
            if (w->routeAng < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x70), ARC(0x71), 3, 0x41, 0);
                em->r_no_3 = 1;
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x70), ARC(0x71), 3, 1, 0);
                em->r_no_3 = 0;
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x4E), ARC(0x4F), 3, 1, 0);
            em->r_no_3 = 0;
        }
        w->atkHit = 0;
        w->atkHit2 = 0;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                if (em->r_no_3 == 0) {
                    em35AtkCk(em, 9, 0xE);
                    em35AtkCk(em, 9, 0xF);
                    em35AtkCk(em, 9, 0x10);
                    em35AtkCk(em, 9, 0x22);
                } else {
                    em35AtkCk(em, 8, 8);
                    em35AtkCk(em, 8, 9);
                    em35AtkCk(em, 8, 0xA);
                    em35AtkCk(em, 8, 0x1F);
                }
            }
            if ((em->Motion.Seq_old.Free & 4) && w->routeAngAbs < 1.5707964f && em->l_pl < 25000000.0f) {
                ActBtn.set(ACT_STOOP, 0xB, (void*) em35AtkEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
            }
        }
        break;
    }
}

// Routine 1/0x11 (upper body): the jump up to the beam above, then em35NextRtnSetUpper.
static void em35_R1_U_JumpUp(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x7C), ARC(0x7D), 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x12 (upper body): the jump down to the beam below, then em35NextRtnSetUpper.
static void em35_R1_U_JumpDown(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x7E), ARC(0x7F), 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x13 (upper body): the double-length leap that skips a beam ahead, then em35NextRtnSetUpper.
static void em35_R1_U_DoubleJump(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x80), ARC(0x81), 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x14 (upper body): the backwards leap along the beam. On landing, with the player at
// beam height within 5.5 m: the hand attack within 1.5 m (1/0x1B), the uppercut within 3 m when
// facing him (1/0x1D), the arm swing (1/0x1C, 50 % within 3 m) or the spear (1/0x1E) within 42 deg. Otherwise
// (r_no_3) another back jump while a beam behind exists, or a coin-flip drop / climb, or U_Wait.
static void em35_R1_U_BackJump(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), ARC(0x51), 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (em->l_pl < 30250000.0f && dy < 500.0f) {
                if (em->l_pl < 2250000.0f) {
                    EmRoutineSet(em, 1, 0x1B, 0, 0);
                } else if (em->l_pl < 9000000.0f && w->routeAngAbs < 0.2617994f) {
                    EmRoutineSet(em, 1, 0x1D, 0, 0);
                } else if (em->l_pl < 9000000.0f && (Rnd() & 1)) {
                    EmRoutineSet(em, 1, 0x1C, 0, 0);
                } else if (w->routeAngAbs < 0.7330383f) {
                    EmRoutineSet(em, 1, 0x1E, 0, 0);
                } else {
                    goto beam;
                }
            } else {
            beam:
                if (em->r_no_3 && em35BeamBackCk(em, w->beamNo)) {
                    em->r_no_2 = 0;
                } else if ((Rnd() & 1) && em35BeamDownCk(em, w->beamNo)) {
                    EmRoutineSet(em, 1, 0x1A, 0, 0);
                } else if ((Rnd() & 1) && em35BeamUpCk(em, w->beamNo)) {
                    EmRoutineSet(em, 1, 0x19, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                }
            }
        }
        break;
    }
}

// Routine 1/0x15 (upper body): the about-face on the beam, then em35NextRtnSetUpper.
static void em35_R1_U_Turn180(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x5A), ARC(0x5B), 10, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x16 (upper body): a short side step (r_no_3 = mirrored), then em35NextRtnSetUpper.
static void em35_R1_U_Step(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x52), ARC(0x53), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x52), ARC(0x53), 10, 1, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x17 (upper body): a long side step (r_no_3 = mirrored), then em35NextRtnSetUpper.
static void em35_R1_U_BigStep(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x54), ARC(0x55), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x54), ARC(0x55), 10, 1, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x18 (upper body): the side hop onto the neighbouring beam (r_no_3 = to the left):
// jumpSpd is the sideways distance to that beam less 1.5 m (em35GetBeamDis), covered a tenth per
// frame during the motion; then em35NextRtnSetUpper.
static void em35_R1_U_OverStep(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        f32 d;

        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x60), ARC(0x61), 10, 0x41, 0);
            d = -(SQRTF(em35GetBeamDis(&em->pos, w->beamNo, 1, em->ang.y)) - 1500.0f);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x60), ARC(0x61), 10, 1, 0);
            d = SQRTF(em35GetBeamDis(&em->pos, w->beamNo, 0, em->ang.y)) - 1500.0f;
        }
        w->jumpSpd.x = d;
        w->jumpSpd.y = 0.0f;
        w->jumpSpd.z = 0.0f;
        PSMTXMultVecSR(em->mat, &w->jumpSpd, &w->jumpSpd);
        em->r_no_2++;
    }
    case 1: {
        Vec v;
        Vec* js = &w->jumpSpd;

        PSVECScale(js, &v, 0.1f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(js, &v, js);
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
    }
}

// Routine 1/0x19 (upper body): climbs to the beam above, then em35NextRtnSetUpper.
static void em35_R1_U_StepUp(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x56), ARC(0x57), 10, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 1/0x1A (upper body): drops to the beam below: forwards (the attacking drop with the hand
// swipe, attack 8 on motion event bit 0 and the duck prompt on bit 2, against a player within 3 m on
// Game_level above 1) or backwards when the player is behind. A hit continues with
// em35NextRtnSetUpper2, else em35NextRtnSetUpper.
static void em35_R1_U_StepDown(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (w->routeAngAbs < 1.5707964f) {
            if (em->l_pl < 9000000.0f && w->routeAngAbs < 1.0471976f && pG->Game_level > 1) {
                MotionSetCore(em, MOTION(em), ARC(0x72), ARC(0x73), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x58), ARC(0x59), 10, 1, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x62), ARC(0x63), 10, 1, 0);
        }
        w->atkHit = 0;
        w->atkHit2 = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                em35AtkCk(em, 8, 8);
                em35AtkCk(em, 8, 9);
                em35AtkCk(em, 8, 0xA);
                em35AtkCk(em, 8, 0x1F);
                em35AtkCk(em, 8, 0x2B);
                em35AtkCk(em, 8, 0x2C);
                em35AtkCk(em, 8, 0x2D);
                em35AtkCk(em, 8, 0x2E);
                em35AtkCk(em, 8, 0x2F);
            }
            if ((em->Motion.Seq_old.Free & 4) && em->l_pl < 9000000.0f) {
                ActBtn.set(ACT_STOOP, 0xB, (void*) em35AtkEscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);
            }
        }
        break;
    }
}

// Routine 1/0x1B (upper body): the close hand swipe on the player's side (r_no_3 = mirrored; the
// backhand set when he is behind), attack 8 / 9 along the hand parts on motion event bit 0. A miss
// awards the escape point; the follow-up is em35NextRtnSetUpper2 after a hit, else em35NextRtnSetUpper.
static void em35_R1_U_HandAtk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        int flip;
        void* m;
        void* s;

        if (w->routeAng < 0.0f) {
            em->r_no_3 = 1;
            flip = 0x41;
        } else {
            em->r_no_3 = 0;
            flip = 1;
        }
        if (w->routeAngAbs < 1.5707964f) {
            m = ARC(0x66);
            s = ARC(0x67);
        } else {
            m = ARC(0x64);
            s = ARC(0x65);
        }
        MotionSetCore(em, MOTION(em), m, s, 3, flip, 0);
        w->atkHit = 0;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            if (em->r_no_3 == 0) {
                em35AtkCk(em, 8, 8);
                em35AtkCk(em, 8, 9);
                em35AtkCk(em, 8, 0xA);
                em35AtkCk(em, 8, 0x1F);
            } else {
                em35AtkCk(em, 9, 0xE);
                em35AtkCk(em, 9, 0xF);
                em35AtkCk(em, 9, 0x10);
                em35AtkCk(em, 9, 0x22);
            }
        }
        break;
    }
}

// Routine 1/0x1C (upper body): the big arm swing (the turned set when the player is behind, r_no_3
// = mirrored) with its effect; attack 8 / 9 swept along the whole arm on motion event bit 0. Exits
// like em35_R1_U_HandAtk.
static void em35_R1_U_Atk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        void* m = ARC(0x68);
        void* s = ARC(0x69);

        if (w->routeAngAbs > 1.5707964f) {
            m = ARC(0x86);
            s = ARC(0x87);
        }
        if (w->routeAng < 0.0f) {
            em->r_no_3 = 1;
        } else {
            em->r_no_3 = 0;
        }
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), m, s, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), m, s, 3, 1, 0);
        }
        EstSet(em, -1, 0, 0, EFF_EM35, 6, 0, ESP_CORE_KIND_NONE, em, 0);
        w->atkHit = 0;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            if (em->r_no_3 == 0) {
                em35AtkCk(em, 8, 8);
                em35AtkCk(em, 8, 9);
                em35AtkCk(em, 8, 0xA);
                em35AtkCk(em, 8, 0x1F);
                em35AtkCk(em, 8, 0x2B);
                em35AtkCk(em, 8, 0x2C);
                em35AtkCk(em, 8, 0x2D);
                em35AtkCk(em, 8, 0x2E);
                em35AtkCk(em, 8, 0x2F);
            } else {
                em35AtkCk(em, 9, 0xE);
                em35AtkCk(em, 9, 0xF);
                em35AtkCk(em, 9, 0x10);
                em35AtkCk(em, 9, 0x22);
                em35AtkCk(em, 9, 0x25);
                em35AtkCk(em, 9, 0x26);
                em35AtkCk(em, 9, 0x27);
                em35AtkCk(em, 9, 0x28);
                em35AtkCk(em, 9, 0x29);
            }
        }
        break;
    }
}

// Routine 1/0x1D (upper body): the uppercut (r_no_3 1 = mirrored) with its effect; attack 0xA at
// the hand parts on motion event bit 0 (only the unmirrored side tests). Exits like em35_R1_U_HandAtk.
static void em35_R1_U_Upper(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0: {
        void* m = ARC(0x6E);
        void* s = ARC(0x6F);

        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), m, s, 3, 1, 0);
            em->r_no_3 = 0;
        } else {
            MotionSetCore(em, MOTION(em), m, s, 3, 0x41, 0);
            em->r_no_3 = 1;
        }
        EstSet(em, -1, 0, 0, EFF_EM35, 6, 0, ESP_CORE_KIND_NONE, em, 0);
        w->atkHit = 0;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            if (em->r_no_3 == 0) {
                em35AtkCk(em, 0xA, 8);
                em35AtkCk(em, 0xA, 9);
                em35AtkCk(em, 0xA, 0xA);
                em35AtkCk(em, 0xA, 0x1F);
                em35AtkCk(em, 0xA, 0x2B);
                em35AtkCk(em, 0xA, 0x2C);
                em35AtkCk(em, 0xA, 0x2D);
                em35AtkCk(em, 0xA, 0x2E);
                em35AtkCk(em, 0xA, 0x2F);
            } else {
                em35AtkCk(em, 0xB, 0xE);
                em35AtkCk(em, 0xB, 0xF);
                em35AtkCk(em, 0xB, 0x10);
                em35AtkCk(em, 0xB, 0x22);
                em35AtkCk(em, 0xB, 0x25);
                em35AtkCk(em, 0xB, 0x26);
                em35AtkCk(em, 0xB, 0x27);
                em35AtkCk(em, 0xB, 0x28);
                em35AtkCk(em, 0xB, 0x29);
            }
        }
        break;
    }
}

// Routine 1/0x1E (upper body): the spearing thrust, aimed by a left / right blend (em35BlendMotSet,
// weight from the yaw to the player up to 45 deg, biased towards the right); attack 0xC at the hand
// parts on motion event bit 0. Exits like em35_R1_U_HandAtk.
static void em35_R1_U_AtkSpear(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0: {
        Vec v;
        f32 ang;

        // blendB before atkHit: the switch register (known 0) then dies at the `stb`, which is the store
        // sched1 issues right after blendA; blendB keeps weight 0 and sinks before v.y.
        w->blendA = 10;
        w->blendB = 0;
        w->atkHit = 0;
        w->blendRate = 0.0f;
        v.x = -87.72f;
        v.y = 0.0f;
        v.z = 678.9f;
        PSMTXMultVec(em->mat, &v, &v);
        ang = Muku(&v, &pPL->pos, em->ang.y, PI);
        if (ang > 0.0f) {
            ang *= 1.3f;
        }
        if (ang > 0.7853982f) {
            ang = 0.7853982f;
        }
        if (ang < -0.7853982f) {
            ang = -0.7853982f;
        }
        ang *= -324.6761f;
        if (ang > 255.0f) {
            ang = 255.0f;
        }
        if (ang < -255.0f) {
            ang = -255.0f;
        }
        w->blendRate = ang;
        em->r_no_2++;
    }
    case 1:
        em35BlendMotSet(em, ARC(0x6A), ARC(0x6D), ARC(0x6C), ARC(0x6B), 0, 0, 1);
        if (MotionMove(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            em35AtkCk(em, 0xC, 8);
            em35AtkCk(em, 0xC, 9);
            em35AtkCk(em, 0xC, 0xA);
            em35AtkCk(em, 0xC, 0x1F);
            em35AtkCk(em, 0xC, 0x2B);
            em35AtkCk(em, 0xC, 0x2C);
            em35AtkCk(em, 0xC, 0x2D);
            em35AtkCk(em, 0xC, 0x2E);
            em35AtkCk(em, 0xC, 0x2F);
        }
        break;
    }
}

// Nearest of the first three crawl positions (XZ plane).
static inline int em35NearCrawlPos(cEm35* em)
{
    f32 best = 999999730000.0f;
    int no = 0;
    int i;

    for (i = 0; i < 3; i++) {
        f32 d = (em->pos.x - em35_crawl_pos[i].x) * (em->pos.x - em35_crawl_pos[i].x) +
                (em->pos.z - em35_crawl_pos[i].z) * (em->pos.z - em35_crawl_pos[i].z);

        if (d < best) {
            best = d;
            no = i;
        }
    }
    return no;
}

// Routine 1/0x20 (upper body, flags 0x30: crawling, cloth off): crawls on the floor to the nearest
// climb spot (em35_crawl_pos 0..2 on the lower floor below y -6 m, 3 / 4 by x on the upper) along the
// route, with a scraping voice every 60..90 frames and the drag effects every 3 / 6 frames; within 1
// m of the spot it jumps back onto the beams (U_JumpToBeam, 1/0x22) carrying the spot in r_no_3.
static void em35_R1_U_Crawl(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x30;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x4A), ARC(0x4B), 10, 5, em->r_no_3);
        w->timer2 = 0;
        w->timer = 15;
        w->atkTimer = (u8) (Rnd() % 30);
        em->r_no_2++;
    case 1: {
        Vec pos;
        Vec out;
        int no;

        if (w->atkTimer) {
            w->atkTimer--;
        } else {
            cParts* p = em->getPartsPtr(0);

            w->atkTimer = (u8) (Rnd() % 30) + 60;
            SndCall(8, 0x40, &p->world, em->id, 0, em);
        }
        if (em->pos.y < -6000.0f) {
            f32 best = 999999730000.0f;
            int i;

            no = 0;
            for (i = 0; i < 3; i++) {
                f32 d = (em->pos.x - em35_crawl_pos[i].x) * (em->pos.x - em35_crawl_pos[i].x) +
                        (em->pos.z - em35_crawl_pos[i].z) * (em->pos.z - em35_crawl_pos[i].z);

                if (d < best) {
                    best = d;
                    no = i;
                }
            }
        } else {
            no = 4;
            if (em->pos.x < 35000.0f) {
                no = 3;
            }
        }
        pos = em35_crawl_pos[no];
        RouteCkToPos(em, &pos, &out, 0, 0);
        fabsf(Muku(&em->pos, &out, em->ang.y, PI));
        em->ang.y += Muku(&em->pos, &out, em->ang.y, 0.2617994f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) < 1000000.0f) {
            em->r_no_0 = 1;
            em->r_no_1 = 0x22;
            em->r_no_2 = 0;
            em->r_no_3 = no;
        }
        break;
    }
    }
    w->timer2++;
    if (w->timer2 % 3 == 0) {
        EstSet(em, -1, 0, 0, EFF_EM35, 0xC, 0, ESP_CORE_KIND_NONE, em, 0);
    }
    if (w->timer2 % 6 == 0) {
        EstSet(em, -1, 0, 0, EFF_EM35, 0xD, 0, ESP_CORE_KIND_NONE, em, 0);
    }
}

// Routine 1/0x21 (upper body): the turn-over while crawling (drag effects as in U_Crawl), then
// back to U_Crawl with its motion started at frame 0x22 (r_no_3).
static void em35_R1_U_CrawlTurn(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x30;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x84), ARC(0x85), 10, 1, em->r_no_3);
        w->timer2 = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_0 = 1;
            em->r_no_1 = 0x20;
            em->r_no_2 = 0;
            em->r_no_3 = 0x22;
        }
        break;
    }
    w->timer2++;
    if (w->timer2 % 3 == 0) {
        EstSet(em, -1, 0, 0, EFF_EM35, 0xC, 0, ESP_CORE_KIND_NONE, em, 0);
    }
    if (w->timer2 % 6 == 0) {
        EstSet(em, -1, 0, 0, EFF_EM35, 0xD, 0, ESP_CORE_KIND_NONE, em, 0);
    }
}

// Routine 1/0x22 (upper body): the leap from the crawl spot r_no_3 back up onto the beams (the
// lower-floor or upper-floor climb motion), turned to face along the room (jumpAng 0 or PI), the
// motion's speed rotated into that heading and jumpSpd closing the gap a twentieth per frame; in
// the air (motion event bit 2 clear) flags 0x20 marks it airborne. Ends with em35NextRtnSetUpper.
static void em35_R1_U_JumpToBeam(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;
    Vec v;

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->pos.y < -6000.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x82), ARC(0x83), 5, 0, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x4C), ARC(0x4D), 5, 0, 0);
        }
        w->jumpAng = 0.0f;
        if (em->ang.y > 1.5707964f || em->ang.y < -1.5707964f) {
            w->jumpAng = PI;
        }
        PSMTXRotRad(m, 'y', w->jumpAng);
        TransMatrix(m, &em35_crawl_pos[em->r_no_3]);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -73.81f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&v, &em->pos, &w->jumpSpd);
        w->dmgCnt = 0;
        em->r_no_2++;
    case 1:
        if (!(em->Motion.Seq_old.Free & 4)) {
            w->flags |= 0x20;
        }
        em->ang.y += Muku2(em->ang.y, w->jumpAng, 0.19634955f);
        PSVECScale(&w->jumpSpd, &v, 0.05f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(&w->jumpSpd, &v, &w->jumpSpd);
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSMTXRotRad(m, 'y', w->jumpAng);
        PSMTXMultVecSR(m, &spd, &spd);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMove(em, 0)) {
            em->ang.y = w->jumpAng;
            PSVECAdd(&em->pos, &w->jumpSpd, &em->pos);
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

// Routine 2: damage reactions (r_no_1: Dm_Small, Dm_Spinal, Dm_Big, Dm_Frame, Dm_U_Fall,
// Dm_U_Crawl); flags 8 keeps the damage checks from restarting one.
static void em35_R0_Damage(cEm35* em)
{
    EM35_WK(em)->flags |= 8;
    Em35_R2_move_tbl[em->r_no_1](em);
}

// Routine 2/0 (whole body): the light flinch, mirrored to whichever foot (parts 0x1E / 0x22) is
// forward, with its voice; the player-blocking collision bit is set. Then punch behind / turn / walk.
static void em35_R1_Dm_Small(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0: {
        Mtx inv;
        Vec a;
        Vec b;
        cParts* p0 = em->getPartsPtr(0x1E);
        cParts* p1 = em->getPartsPtr(0x22);

        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &p0->world, &a);
        PSMTXMultVec(inv, &p1->world, &b);
        if (a.z < b.z) {
            em->r_no_3 = 1;
            MotionSetCore(em, MOTION(em), ARC(0x32), 0, 3, 0x41, 0);
        } else {
            em->r_no_3 = 0;
            MotionSetCore(em, MOTION(em), ARC(0x32), 0, 3, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x29, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x32, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// Routine 2/1 (whole body): the reaction to damage on the exposed spine (weakDmg over 400): the
// arching motion with its scream, a second cry on motion event bit 0. Then punch behind / turn / walk.
static void em35_R1_Dm_Spinal(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x30), ARC(0x31), 3, 1, 0);
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x2F, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x34, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
                break;
            }
            if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
                break;
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        if (em->Motion.Seq_old.Free & 1) {
            SndStop(w->sndId, 0);
            w->sndId = SndCall(8, 0x3A, &em->pos, em->id, 0, em);
            w->sndId = SndCall(8, 0x37, &em->pos, em->id, 0, em);
        }
        break;
    }
}

// Routine 2/2 (whole body): the heavy stagger (mirrored to the forward foot) with its voice. Then
// punch behind / turn / walk.
static void em35_R1_Dm_Big(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0: {
        Mtx inv;
        Vec a;
        Vec b;
        cParts* p0 = em->getPartsPtr(0x1E);
        cParts* p1 = em->getPartsPtr(0x22);

        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &p0->world, &a);
        PSMTXMultVec(inv, &p1->world, &b);
        if (a.z < b.z) {
            em->r_no_3 = 1;
            MotionSetCore(em, MOTION(em), ARC(0x2E), ARC(0x2F), 3, 0x41, 0);
        } else {
            em->r_no_3 = 0;
            MotionSetCore(em, MOTION(em), ARC(0x2E), ARC(0x2F), 3, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x2C, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x33, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// Routine 2/3 (whole body): hit by the room's falling frame (the area damage manager): the heavy
// stagger with 100 damage up front and 10 more per frame for 50 frames; death (routine 3/0) when
// the HP runs out, else punch behind / turn / walk.
static void em35_R1_Dm_Frame(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0: {
        Mtx inv;
        Vec a;
        Vec b;
        cParts* p0 = em->getPartsPtr(0x1E);
        cParts* p1 = em->getPartsPtr(0x22);

        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &p0->world, &a);
        PSMTXMultVec(inv, &p1->world, &b);
        if (a.z < b.z) {
            em->r_no_3 = 1;
            MotionSetCore(em, MOTION(em), ARC(0x2E), ARC(0x2F), 3, 0x41, 0);
        } else {
            em->r_no_3 = 0;
            MotionSetCore(em, MOTION(em), ARC(0x2E), ARC(0x2F), 3, 1, 0);
        }
        LifeDownSet2(em, 100, 0, 0);
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x2C, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x33, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        w->timer = 50;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
                break;
            }
            if (w->targetAngAbs > 2.0943952f && em->l_pl < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
                break;
            }
            if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
                break;
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        if (w->timer) {
            w->timer--;
            LifeDownSet2(em, 10, 0, 0);
            if (w->timer == 0 && em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            }
        }
        break;
    }
}

// Turn towards the nearest crawl position after a fall (em35_R1_Dm_U_Fall / Dm_U_Crawl).
static inline void em35CrawlStart(cEm35* em)
{
    Vec out;
    int no = em35NearCrawlPos(em);

    RouteCkToPos(em, &em35_crawl_pos[no], &out, 0, 0);
    if (fabsf(Muku(&em->pos, &out, em->ang.y, PI)) > 2.0943952f) {
        EmRoutineSet(em, 1, 0x21, 0, 0);
        MotionMove(em, 0);
    } else {
        EmRoutineSet(em, 1, 0x20, 0, 0);
    }
}

// Routine 2/4 (upper body): knocked off its beam. The fall motion is the backwards one when hit
// from behind or when a wall is within 2 m behind, else forwards; on motion event bit 0 it snaps to
// the floor and plays the landing. Then, alive, it turns (U_CrawlTurn) or crawls (U_Crawl) to the
// nearest climb spot; dead, it stays down with the item drop status and the collision opened.
static void em35_R1_Dm_U_Fall(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Vec out;
    f32 ang;

    switch (em->r_no_2) {
    case 0: {
        Vec a;
        Vec b;

        ang = fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI));

        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = -2000.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x100800) || ang > 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x77), ARC(0x78), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x74), ARC(0x75), 3, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x3B, &em->pos, em->id, 0, em);
        em->r_no_2++;
    }
    case 1:
        // The original keeps both copies of this tail (COMPILER-DIFF #6 shape: ours cross-jumps them).
        if (MotionMove(em, 0)) {
            MotionSetCore(em, MOTION(em), ARC(0x76), 0, 3, 1, 0);
            MotionMove(em, 0);
            em->r_no_2++;
        } else if (em->Motion.Seq_old.Free & 1) {
            f32 fl = SatMgr.getFloor(&em->pos, 0, em->pos_old.y - em->pos.y + 2000.0f, 100000.0f, 0);

            if (em->pos.y < fl) {
                em->pos.y = fl;
                MotionSetCore(em, MOTION(em), ARC(0x76), 0, 3, 1, 0);
                MotionMove(em, 0);
                em->r_no_2++;
            }
        }
        break;
    case 2:
        w->sndId = SndCall(8, 0x3E, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 3:
        w->flags |= 0x20;
        if (MotionMove(em, 0)) {
            if (em->hp > 0) {
                int no = em35NearCrawlPos(em);

                RouteCkToPos(em, &em35_crawl_pos[no], &out, 0, 0);
                ang = fabsf(Muku(&em->pos, &out, em->ang.y, PI));
                if (ang > 2.0943952f) {
                    EmRoutineSet(em, 1, 0x21, 0, 0);
                    MotionMove(em, 0);
                } else {
                    EmRoutineSet(em, 1, 0x20, 0, 0);
                }
            } else {
                em->clearStatus(EM_STATUS_ACTIVE);
                em->setStatus(EM_STATUS_ITEMSET);
                em->atari.m_flag &= ~0x300;
                em->r_no_2++;
            }
        }
        break;
    }
}

// Routine 2/5 (upper body): hit while crawling: the writhing motion with its voice; on motion
// event bit 2 it resumes heading for a climb spot (em35CrawlStart), at the end a living one crawls
// on (U_Crawl from frame 0xA), a dead one stays down with the item drop status.
static void em35_R1_Dm_U_Crawl(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x79), ARC(0x7A), 3, 1, 0);
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x29, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0x20, 0, 0xA);
            } else {
                em->clearStatus(EM_STATUS_ACTIVE);
                em->setStatus(EM_STATUS_ITEMSET);
                em->atari.m_flag &= ~0x300;
                em->r_no_2++;
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            em35CrawlStart(em);
        }
        break;
    }
}

// Routine 3: death (r_no_1: 0 Die_Normal, 1 the scripted Die_Pose); flags 8 blocks reactions.
static void em35_R0_Die(cEm35* em)
{
    EM35_WK(em)->flags |= 8;
    Em35_R3_move_tbl[em->r_no_1](em);
}

// Routine 3/0 (whole body): dies where it stands: the idle pose as the death motion, the item
// drop status, its effects removed, the collision opened when the motion ends, then after 30 frames
// a fade-out (invisible_factor, 50 frames) and the model hidden.
static void em35_R1_Die_Normal(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 3, 1, 0);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EffectEspDelete(0, w->espKind, em, 0);
        EffectEspgenDelete(0, w->espKind, em);
        EffectEfmDelete(0, w->espKind, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
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

// Routine 3/1 (setDiePose, from the level script): the enemy is placed at the scene spot as a
// non-colliding corpse in the death pose for the ending cutscene, its effects removed.
static void em35_R1_Die_Pose(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.throughOn();
        em->pos.x = 36817.0f;
        em->pos.y = -7963.56f;
        em->pos.z = -59757.32f;
        em->ang.x = 0.0f;
        em->ang.y = 0.0f;
        em->ang.z = 0.0f;
        MotionSetCore(em, MOTION(em), ARC(0x7B), 0, 0, 1, 0);
        EffectEspDelete(1, w->espKind, em, 0);
        EffectEspgenDelete(1, w->espKind, em);
        EffectEfmDelete(1, w->espKind, em);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        em->r_no_2++;
        break;
    }
}

// Per-frame target selection while alive. The upper body targets the player directly (it moves
// on the beam graph). The whole body targets the player directly too when he is on the upper floor
// (1 m above it), but with the route point swapped for one of three fixed spots under the walkway
// nearest to him so it stands where the second-floor attack reaches; on the same floor it routes
// to the player (flags bit 0 when a route exists; routeAng zero during init).
void em35RouteCk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (em->type == 1) {
        w->routePos = pPL->pos;
        w->flags |= 1;
        w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
        w->routeAngAbs = fabsf(w->routeAng);
        w->targetPos = w->routePos;
        w->targetAng = w->routeAng;
        w->targetAngAbs = w->routeAngAbs;
        w->targetDist = em->l_pl;
        w->pTarget = pPL;
        w->flags &= ~4;
        return;
    }
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        Vec v;

        w->routePos = pPL->pos;
        w->flags |= 1;
        w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
        w->routeAngAbs = fabsf(w->routeAng);
        w->targetPos = w->routePos;
        w->targetAng = w->routeAng;
        w->targetAngAbs = w->routeAngAbs;
        w->targetDist = em->l_pl;
        w->pTarget = pPL;
        if (pPL->pos.x < 33000.0f && pPL->pos.z > -61300.0f) {
            if (em->pos.z < -60000.0f) {
                v.x = 35686.0f;
                v.y = -8100.0f;
                v.z = -58425.0f;
                RouteCkToPos(em, &v, &w->targetPos, 0, 0);
                w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, PI);
                w->targetAngAbs = fabsf(w->targetAng);
            }
        } else if (pPL->pos.x > 38000.0f && pPL->pos.z > -63000.0f) {
            if (em->pos.x > 36500.0f) {
                v.x = 37419.0f;
                v.y = -8100.0f;
                v.z = -61700.0f;
                RouteCkToPos(em, &v, &w->targetPos, 0, 0);
                w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, PI);
                w->targetAngAbs = fabsf(w->targetAng);
            }
        } else {
            if (em->pos.z > -61500.0f) {
                v.x = 36391.0f;
                v.y = -8100.0f;
                v.z = -63645.0f;
                RouteCkToPos(em, &v, &w->targetPos, 0, 0);
                w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, PI);
                w->targetAngAbs = fabsf(w->targetAng);
            }
        }
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
}

// Head tracking: while a routine runs (flags 0x10) neckAng eases towards the player's yaw (up to
// 60 deg), else back to centre; applied as addRot.y to the upper body's head part 3, or spread as a
// quarter each over the whole body's spine parts 2..5.
void em35NeckMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Vec v;
    cParts* p;

    em->getPartsPtr(4);
    p = (cParts*) pPL->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (w->flags & 0x10) {
        w->neckAng = w->neckAng * 0.95f + Muku(&em->pos, &pPL->pos, em->ang.y, 1.0471976f) * 0.05f;
    } else {
        w->neckAng = w->neckAng * 0.95f;
    }
    if (em->type == 1) {
        p = (cParts*) em->getPartsPtr(3);
        p->motParts.flags |= 0x40000000;
        p->inv_offset.x = 0.0f;
        p->inv_offset.y = w->neckAng;
        p->inv_offset.z = 0.0f;
    } else {
        f32 ang = w->neckAng * 0.25f;

        p = (cParts*) em->getPartsPtr(2);
        p->motParts.flags |= 0x40000000;
        p->inv_offset.x = 0.0f;
        p->inv_offset.y = ang;
        p->inv_offset.z = 0.0f;
        p = (cParts*) em->getPartsPtr(3);
        p->motParts.flags |= 0x40000000;
        p->inv_offset.x = 0.0f;
        p->inv_offset.y = ang;
        p->inv_offset.z = 0.0f;
        p = (cParts*) em->getPartsPtr(4);
        p->motParts.flags |= 0x40000000;
        p->inv_offset.x = 0.0f;
        p->inv_offset.y = ang;
        p->inv_offset.z = 0.0f;
        p = (cParts*) em->getPartsPtr(5);
        p->motParts.flags |= 0x40000000;
        p->inv_offset.x = 0.0f;
        p->inv_offset.y = ang;
        p->inv_offset.z = 0.0f;
    }
}

// Turn the player towards the enemy and knock him down (em35AtkCk).
static inline void em35PlKnock(cEm35* em)
{
    pPL->ang.y = pPL->ang.y + Muku(&pPL->pos, &em->pos, pPL->ang.y, PI);
    pPL->ang.y = LIMIT_ANGLE(pPL->ang.y);
    PlSetDamage(PL_DM_AUTO, 0, 0);
}

// Tests attack `no` of em35_atk_tbl swept from part `parts`' previous to its current position
// against the player (hit bit 0) and partner (bit 1), once per attack (atkHit). A player hit: the
// punches / hand swipes / upper / spear knock him down facing the enemy (em35PlKnock) with the
// blood effect of the side; the stamp (2) and hook (4) take him over (plem35DmStamp / plem35DmHook);
// the double punch (3) only sounds (the bear hug follows); the second-floor punches (5 / 6) knock
// him off the ledge (em35PlFallCk) or down; the critical (7) leaves him at 1 HP and marks both
// dmType 0x80 for the kill. Any hit shakes the camera and rumbles. Returns 1 on a hit.
int em35AtkCk(cEm35* em, u32 no, int parts)
{
    Em35Work* w = EM35_WK(em);
    EmAtkInfo* info;
    cParts* p;
    int hit;

    if (w->atkHit) {
        return 0;
    }
    info = &em35_atk_tbl[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(info, &p->world, &p->world_old, 0);
    if (hit != 0) {
        if (hit & 1) {
            w->atkHit = 1;
            switch (no) {
            default:
                EmPlBloodSet(em, &p->world, 1, 0xFF, 0xFF);
                break;
            case 0:
            case 1:
                if (no == 0) {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x14);
                } else {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x15);
                }
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 2:
                SetPlDamage(em, plem35DmStamp);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 3:
                SndCall(8, 0x47, &em->pos, em->id, 0, em);
                break;
            case 4:
                pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                SetPlDamage(em, plem35DmHook);
                SndCall(8, 0x47, &em->pos, em->id, 0, em);
                break;
            case 5:
            case 6:
                if (no == 5) {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x14);
                } else {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x15);
                }
                if (pPL->pos.y > em->pos.y + 2000.0f) {
                    if (em35PlFallCk(em) == 0) {
                        pPL->ang.y = GetXZAngle(&p->world, &p->world_old);
                        PlSetDamage(PL_DM_AUTO, 0, 0);
                    }
                }
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 7:
                EmPlBloodSet(em, &p->world, 1, 0xFF, 0xFF);
                pG->pl_life = 1;
                em->dmg.m_Timer = 0x80;
                pPL->dmg.m_Timer = 0x80;
                SndCall(8, 0x1E, &em->pos, em->id, 0, em);
                break;
            case 8:
            case 9:
                if (no == 8) {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x14);
                } else {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x15);
                }
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 0xA:
            case 0xB:
                if (no == 0xA) {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x1A);
                } else {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x1B);
                }
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 0xC:
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x2C, 0x19);
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            }
        }
        if (hit & 2) {
            EmSubBloodSet(em, &p->world, 1, 0xFF, 0xFF);
            w->atkHit = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        VibSetData(VIB_TBL, 7, 1);
        return 1;
    }
    return 0;
}

// For a player on the upper floor (2 m above): if a ledge collision (SatMgr flag 0x00100000)
// lies between him and the enemy, moves him 30 cm past its edge facing the drop and takes him over
// with plem35DmFall2F. Returns 1 when he was knocked off.
int em35PlFallCk(cEm35* em)
{
    Vec a;
    Vec b;
    Vec hit;
    Vec nrm;
    Vec d;

    if (pPL->pos.y < em->pos.y + 2000.0f) {
        return 0;
    }
    a = pPL->pos;
    a.y += 500.0f;
    b = em->pos;
    b.y = a.y;
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0) & 0x00100000) {
        PSVECScale(&nrm, &d, 300.0f);
        PSVECAdd(&hit, &d, &d);
        pPL->pos.x = d.x;
        pPL->pos.z = d.z;
        pPL->ang.y = atan2f(-nrm.x, -nrm.z);
        SetPlDamage(em, plem35DmFall2F);
        return 1;
    }
    return 0;
}

// 1 when the pending hit landed on the exposed spine (parts 3..6), the weak point.
int em35WeakDmCk(cEm35* em)
{
    s16 n = em->dmg.m_pDamageYarare->parts_no;

    if (n == 3) {
        return 1;
    }
    if (n == 4) {
        return 1;
    }
    if (n == 5) {
        return 1;
    }
    return n == 6;
}

// Squared distance of the parts to `pos` below the catch range (em35CatchCk).
#define EM35_CATCH_PARTS_CK(parts)                                                                         \
    p = em->getPartsPtr(parts);                                                                             \
    if ((p->world.x - pos.x) * (p->world.x - pos.x) + (p->world.y - pos.y) * (p->world.y - pos.y) + \
            (p->world.z - pos.z) * (p->world.z - pos.z) <                                            \
        160000.0f) {                                                                                       \
        hit = 1;                                                                                           \
    }

// The grab test of em35_R1_br_Catch: on motion event bit 1, with the player alive and not already
// held (Status_flg[1] 0x8000), 1 when any of the reaching arm's four parts (the mirrored set for a
// flipped motion) is within 40 cm of a point 1.6 m above the player's feet; marks both dmg and rumbles.
int em35CatchCk(cEm35* em)
{
    Vec pos;
    cParts* p;
    int hit;

    if (EmDeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (!(em->Motion.Seq_old.Free & 2)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED)) {
        return 0;
    }
    pos = pPL->pos;
    pos.y += 1600.0f;
    hit = 0;
    if (em->Motion.Mot_attr & 0x40) {
        EM35_CATCH_PARTS_CK(0x4D);
        EM35_CATCH_PARTS_CK(0x1A);
        EM35_CATCH_PARTS_CK(0x19);
        EM35_CATCH_PARTS_CK(0x18);
    } else {
        EM35_CATCH_PARTS_CK(0x4A);
        EM35_CATCH_PARTS_CK(0x14);
        EM35_CATCH_PARTS_CK(0x13);
        EM35_CATCH_PARTS_CK(0x12);
    }
    if (hit == 0) {
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData(VIB_TBL, 7, 1);
    return 1;
}

// In room 0x011F, snaps the enemy to the nearest of three grab spots along the hall's centre
// line, facing along the hall (the middle spot keeps whichever way it faced), 4.8 m back from the
// spot so the grab / bear hug motions line up with the walls.
void em35CatchPosSet(cEm35* em)
{
    Vec tbl[3] = {
        { 36500.0f, -7950.0f, -63186.0f },
        { 36500.0f, -7950.0f, -58162.0f },
        { 36500.0f, -7950.0f, -53152.0f },
    };
    u8 kind[3] = { 0, 1, 2 };

    if (pG->stage_no == 1 && pG->room_no == 0x1F) {
        Vec pos;
        f32 best = 10000000000000000.0f;
        int no = 0;
        int i;

        for (i = 0; i < 3; i++) {
            Vec* p = &tbl[i];
            f32 d = (em->pos.x - p->x) * (em->pos.x - p->x) + (em->pos.y - p->y) * (em->pos.y - p->y) +
                    (em->pos.z - p->z) * (em->pos.z - p->z);

            if (!(d > best)) {
                best = d;
                no = i;
            }
        }
        pos = tbl[no];
        switch (kind[no]) {
        case 0:
        default:
            em->ang.y = PI;
            pos.z += 4819.2f;
            break;
        case 1:
            if (fabsf(em->ang.y) < 1.5707964f) {
                em->ang.y = 0.0f;
                pos.z -= 4819.2f;
            } else {
                em->ang.y = PI;
                pos.z += 4819.2f;
            }
            break;
        case 2:
            em->ang.y = 0.0f;
            pos.z -= 4819.2f;
            break;
        }
        em->setPos(&pos);
    }
}

// 1 when the player is running (routine 0/3) straight at the enemy (within 1 m of his line, in
// front of the enemy) on Game_level above 3: the attacks reach farther against him.
int em35bPlRunCk(cEm35* em)
{
    Mtx inv;
    Vec v;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 3) {
        return 0;
    }
    if (pG->Game_level <= 3) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 1.5707964f) {
        return 0;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    if (v.x > 1000.0f) {
        return 0;
    }
    if (v.x < -1000.0f) {
        return 0;
    }
    return 1;
}

// Upper body only: sets up the six-node tail chain (parts 0x12..0x17; three bundles, gravity 15,
// 100 mm segments) as a pendulum cloth.
void em35ClothSet(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 1) {
        w->cloth1.Num = 6;
        w->cloth1.pCloth = em35ClothP;
        w->cloth1.pLeft = 0;
        w->cloth1.pRight = 0;
        w->cloth1.pUpLeft = 0;
        w->cloth1.pUpRight = 0;
        w->cloth1.pParent = em35ClothUp;
        w->cloth1.pChild = em35ClothDp;
        w->cloth1.pMax = em35ClothMax;
        w->cloth1.pWindSin = 0;
        w->cloth1.pWindRate = 0;
        w->cloth1.pGravity = 0;
        w->cloth1.pRate = 0;
        w->cloth1.pAtset = 0;
        w->cloth1.At_num = 0;
        w->cloth1.Gravity = 15.0f;
        w->cloth1.Rate = 0.9f;
        w->cloth1.Bundle_num = 3;
        w->cloth1.WindSin = 0.0f;
        w->cloth1.Stretchy = 0.05f;
        w->cloth1.Move_rate = 0.0f;
        w->cloth1.Flag = 0;
        w->cloth1.pPtbl = 0;
        PenClothSet(em, (PenCloth*) &w->cloth1, 100.0f);
    }
}

// Upper body only, per frame unless dividing (flags 0x40): simulates the tail chain and rebuilds
// the world matrices of the parts 0x35..0x41 hanging off it.
void em35ClothMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 1 && !(w->flags & 0x40)) {
        u32 i;

        PenClothMove2(em, (PenCloth*) &w->cloth1);
        for (i = 0x35; i <= 0x41; i++) {
            cParts* p = (cParts*) em->getPartsPtr(i);

            PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);
            p->world.x = p->mat[0][3];
            p->world.y = p->mat[1][3];
            p->world.z = p->mat[2][3];
        }
    }
}

// Upper body only: sets up the two-node hanging skin (parts 0x19 / 0x1A) as a pendulum cloth with
// the em35ClothAt2 collision spheres.
void em35ClothSet2(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 1) {
        w->cloth2.Num = 2;
        w->cloth2.pCloth = em35ClothP2;
        w->cloth2.pLeft = 0;
        w->cloth2.pRight = 0;
        w->cloth2.pUpLeft = 0;
        w->cloth2.pUpRight = 0;
        w->cloth2.pParent = em35ClothUp2;
        w->cloth2.pChild = em35ClothDp2;
        w->cloth2.pWindSin = 0;
        w->cloth2.pWindRate = 0;
        w->cloth2.pGravity = 0;
        w->cloth2.pMax = em35ClothMax2;
        w->cloth2.pAtset = em35ClothAt2;
        w->cloth2.pRate = em35ClothRate2;
        w->cloth2.At_num = 5;
        w->cloth2.Gravity = 15.0f;
        w->cloth2.Rate = 0.8f;
        w->cloth2.Bundle_num = 4;
        w->cloth2.pModel = em;
        w->cloth2.WindSin = 0.0f;
        w->cloth2.Stretchy = 1.0f;
        w->cloth2.Move_rate = 0.0f;
        w->cloth2.Flag = 0x100;
        w->cloth2.pPtbl = 0;
        PenClothSet(em, (PenCloth*) &w->cloth2, 100.0f);
    }
}

// Upper body only, per frame: simulates its hanging skin.
void em35ClothMove2(cEm35* em)
{
    if (em->type == 1) {
        PenClothMove3(em, (PenCloth*) &EM35_WK(em)->cloth2);
    }
}

// Whole body only: sets up its two-node hanging skin (parts 0xB / 0xC) as a pendulum cloth with
// the em35ClothAt3 collision spheres.
void em35ClothSet3(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 0) {
        w->cloth2.Num = 2;
        w->cloth2.pCloth = em35ClothP3;
        w->cloth2.pLeft = 0;
        w->cloth2.pRight = 0;
        w->cloth2.pUpLeft = 0;
        w->cloth2.pUpRight = 0;
        w->cloth2.pParent = em35ClothUp3;
        w->cloth2.pChild = em35ClothDp3;
        w->cloth2.pWindSin = 0;
        w->cloth2.pWindRate = 0;
        w->cloth2.pGravity = 0;
        w->cloth2.pMax = em35ClothMax3;
        w->cloth2.pRate = em35ClothRate3;
        w->cloth2.At_num = 5;
        w->cloth2.pAtset = em35ClothAt3;
        w->cloth2.Gravity = 15.0f;
        w->cloth2.Rate = 0.8f;
        w->cloth2.Bundle_num = 4;
        w->cloth2.pModel = em;
        w->cloth2.WindSin = 0.0f;
        w->cloth2.Stretchy = 1.0f;
        w->cloth2.Move_rate = 0.0f;
        w->cloth2.Flag = 0x100;
        w->cloth2.pPtbl = 0;
        PenClothSet(em, (PenCloth*) &w->cloth2, 100.0f);
    }
}

// Whole body only, per frame: simulates its hanging skin.
void em35ClothMove3(cEm35* em)
{
    if (em->type == 0) {
        PenClothMove3(em, (PenCloth*) &EM35_WK(em)->cloth2);
    }
}

// The upper body's move selection on the beam graph, run at the end of each of its routines. A
// player a floor above / below: jump or climb / drop towards his level. When the player aims at it
// (em35LockCk) a quarter of the time it dodges up or down. At beam height within 5.5 m: the hand
// attack within 1.5 m, the uppercut within 3 m when facing him, the arm swing (50 %) within 3 m
// facing or with his back turned, the spear within 42 deg. Otherwise: an about-face when he is
// behind, a side step towards him past 45 deg, then along the beam: a forward jump up / down (50 %),
// a double leap (50 %, Game_level above 1), the plain leap, a side step to either side, or a step in place.
void em35NextRtnSetUpper(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    f32 dy = em->pos.y - pPL->pos.y;

    dy = fabsf(dy);
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        if (em35BeamFrontUpCk(em, w->beamNo) && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return;
        }
        if (em->l_pl < 100000000.0f && em35BeamUpCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x19, 0, 0);
            return;
        }
    }
    if (pPL->pos.y < em->pos.y - 1000.0f) {
        if (em35BeamFrontDownCk(em, w->beamNo) && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
            return;
        }
        if (em->l_pl < 25000000.0f && em35BeamDownCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            return;
        }
    }
    if (em35LockCk(em) && (Rnd() & 1) && w->beamNo != 0xFF && (Rnd() & 1)) {
        if (em35BeamUpCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x19, 0, 0);
            return;
        }
        if (em35BeamDownCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            return;
        }
    }
    if (em->l_pl < 30250000.0f && dy < 500.0f) {
        if (em->l_pl < 2250000.0f) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            return;
        }
        if (em->l_pl < 9000000.0f && w->routeAngAbs < 0.2617994f) {
            EmRoutineSet(em, 1, 0x1D, 0, 0);
            return;
        }
        if (em->l_pl < 9000000.0f && w->routeAngAbs < 1.0471976f && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x1C, 0, 0);
            return;
        }
        if (em->l_pl < 9000000.0f && w->routeAngAbs > 2.0943952f && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x1C, 0, 0);
            return;
        }
        if (w->routeAngAbs < 0.7330383f) {
            EmRoutineSet(em, 1, 0x1E, 0, 0);
            return;
        }
    }
    if (w->routeAngAbs > 1.5707964f) {
        EmRoutineSet(em, 1, 0x15, 0, 0);
        return;
    }
    if (w->routeAngAbs > 0.7853982f) {
        if (w->routeAng < 0.0f) {
            if (em35BeamSideStepCk(em, 1)) {
                return;
            }
        } else {
            if (em35BeamSideStepCk(em, 0)) {
                return;
            }
        }
    }
    if (em35BeamFrontUpCk(em, w->beamNo) && (Rnd() & 1)) {
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return;
    }
    if (em35BeamFrontDownCk(em, w->beamNo) && (Rnd() & 1)) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
        return;
    }
    if ((Rnd() & 1) && pG->Game_level > 1 && em35BeamFrontDobuleCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return;
    }
    if (em35BeamFrontCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x10, 0, 0);
        return;
    }
    if (em35BeamSideStepCk(em, 1)) {
        return;
    }
    if (em35BeamSideStepCk(em, 0)) {
        return;
    }
    if (w->routeAng < 0.0f) {
        EmRoutineSet(em, 1, 0x16, 0, 1);
    } else {
        EmRoutineSet(em, 1, 0x16, 0, 0);
    }
}

// The upper body's retreat after an attack landed: a back jump (repeating, r_no_3 1) if a beam
// lies behind, else climb / drop / leap forward / side step away from the player, else U_Wait.
void em35NextRtnSetUpper2(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em35BeamBackCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x14, 0, 1);
        return;
    }
    if (em35BeamUpCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x19, 0, 0);
        return;
    }
    if (em35BeamDownCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x1A, 0, 0);
        return;
    }
    if (em35BeamFrontCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x10, 0, 0);
        return;
    }
    if (w->routeAng > 0.0f) {
        if (em35BeamSideStepCk(em, 1)) {
            return;
        }
    } else {
        if (em35BeamSideStepCk(em, 0)) {
            return;
        }
    }
    EmRoutineSet(em, 1, 0xF, 0, 0);
}

// 1 when the player is aiming (routine 0/6) a loaded gun other than the rocket launcher (0x10) at
// the enemy within 12 m: the enemy in front within 45 deg and its root within a 1 m box in front of
// the player's weapon hand (part 0xA).
int em35LockCk(cEm35* em)
{
    Mtx inv;
    Vec v;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 6) {
        return 0;
    }
    if (em->l_pl > 144000000.0f) {
        return 0;
    }
    if (pG->weapon_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(0xA)->mat, inv);
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

// The beam of orientation `type` (0 along X, 1 along Z) that `pos` stands on: among the beams at
// the same height (within 50 cm) whose span covers the point, the one it is closest to sideways.
// 0xFF when none.
int em35GetBeamNo(Vec* pos, int type)
{
    Mtx m;
    Vec v;
    f32 best = 100000000.0f;
    int no = 0xFF;
    u8 i;

    for (i = 0; i < 17; i++) {
        Em35Beam* b = &em35_beam_tbl[i];

        if (b->type == type && !(fabsf(b->a.y - pos->y) > 500.0f)) {
            f32 ang = GetXZAngle(&b->a, &b->b);
            f32 len = VEC_DISTXZ(&b->a, &b->b);

            PSMTXRotRad(m, 'y', ang);
            TransMatrix(m, &b->a);
            PSMTXInverse(m, m);
            PSMTXMultVec(m, pos, &v);
            if (!(v.z < 0.0f) && !(v.z > len)) {
                f32 ax = fabsf(v.x);

                if (!(ax > best)) {
                    best = ax;
                    no = i;
                }
            }
        }
    }
    return no;
}

// Squared XZ distance from `pos` to the end of beam `no` that lies ahead for yaw `ang` (or behind,
// with `side` set): the beam's a / b end is picked by which way the yaw points along it.
f32 em35GetBeamDis(Vec* pos, int no, int side, f32 ang)
{
    Em35Beam* b;
    int far;
    f32 d;

    if (no == 0xFF) {
        return 0.0f;
    }
    b = &em35_beam_tbl[no];
    far = 0;
    switch (b->type) {
    case 0:
    default:
        if (ang > 1.5707964f) {
            far = 1;
        }
        if (ang < -1.5707964f) {
            far = 1;
        }
        break;
    case 1:
        if (ang > 0.0f) {
            far = 1;
        }
        break;
    }
    if (side) {
        far ^= 1;
    }
    if (far == 0) {
        d = (pos->x - b->a.x) * (pos->x - b->a.x) + (pos->z - b->a.z) * (pos->z - b->a.z);
    } else {
        d = (pos->x - b->b.x) * (pos->x - b->b.x) + (pos->z - b->b.z) * (pos->z - b->b.z);
    }
    return d;
}

// Two wall probes ahead of the enemy: the way onto the next beam is clear.
#define EM35_BEAM_WAY_CK(em, Y, Z)                                  \
    {                                                               \
        Vec a;                                                      \
        Vec b;                                                      \
                                                                    \
        a.x = 300.0f;                                               \
        a.y = Y;                                                    \
        a.z = 0.0f;                                                 \
        b.x = 300.0f;                                               \
        b.y = Y;                                                    \
        b.z = Z;                                                    \
        PSMTXMultVec((em)->mat, &a, &a);                            \
        PSMTXMultVec((em)->mat, &b, &b);                            \
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x100800)) {           \
            return 0;                                               \
        }                                                           \
        a.x = -300.0f;                                              \
        a.y = Y;                                                    \
        a.z = 0.0f;                                                 \
        b.x = -300.0f;                                              \
        b.y = Y;                                                    \
        b.z = Z;                                                    \
        PSMTXMultVec((em)->mat, &a, &a);                            \
        PSMTXMultVec((em)->mat, &b, &b);                            \
        return SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x100800) == 0;     \
    }

// 1 when beam `no` continues ahead of the enemy (its front / back link by the facing) and the 5.5 m
// ahead are clear of walls.
int em35BeamFrontCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    switch (em35BeamDirCk(no, em->ang.y)) {
    case 0:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, 1000.0f, 5500.0f);
}

// 1 when beam `no` continues behind the enemy and the 5.5 m behind are clear of walls.
int em35BeamBackCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    switch (em35BeamDirCk(no, em->ang.y)) {
    case 0:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, 1000.0f, -5500.0f);
}

// 1 when two beams in a row continue ahead (both em35BeamFrontCk clear) and the player is at
// least 10 m away: the double leap is possible.
int em35BeamFrontDobuleCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    if (em->l_pl < 100000000.0f) {
        return 0;
    }
    if (em35BeamFrontCk(em, no) == 0) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    switch (em35BeamDirCk(no, em->ang.y)) {
    case 0:
        next = b->front;
        break;
    case 1:
        next = b->back;
        break;
    default:
        return 0;
    }
    if (em35BeamFrontCk(em, next) == 0) {
        return 0;
    }
    return 1;
}

// 1 when beam `no` has a beam above it and the enemy is on the lower level (y under -6 m).
int em35BeamUpCk(cEm35* em, int no)
{
    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y > -6000.0f) {
        return 0;
    }
    if (em35_beam_tbl[no].up == 0xFF) {
        return 0;
    }
    return 1;
}

// 1 when beam `no` has a beam below it and the enemy is on the upper level (y above -6 m).
int em35BeamDownCk(cEm35* em, int no)
{
    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y < -6000.0f) {
        return 0;
    }
    if (em35_beam_tbl[no].down == 0xFF) {
        return 0;
    }
    return 1;
}

// 1 when the beam above `no` continues ahead of the enemy (from the lower level) and the way up
// there (5 m up, 5.5 m ahead) is clear: the forward jump up is possible.
int em35BeamFrontUpCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y > -6000.0f) {
        return 0;
    }
    if (em35BeamUpCk(em, no) == 0) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    b = &em35_beam_tbl[b->up];
    switch (em35BeamDirCk(no, em->ang.y)) {
    case 0:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, 5000.0f, 5500.0f);
}

// 1 when the beam below `no` continues ahead of the enemy (from the upper level) and the way down
// there (3 m down, 5.5 m ahead) is clear: the forward jump down is possible.
int em35BeamFrontDownCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y < -6000.0f) {
        return 0;
    }
    if (em35BeamDownCk(em, no) == 0) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    b = &em35_beam_tbl[b->down];
    switch (em35BeamDirCk(no, em->ang.y)) {
    case 0:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, -3000.0f, 5500.0f);
}

// Picks a sideways move on the current beam towards `side` (0 right / 1 left, passed on as r_no_3)
// by the distance to that end of the beam: the long step beyond 3.5 m, the short step beyond 1.5 m,
// else the hop onto the neighbouring beam when one exists there. Returns 1 when a routine was set.
int em35BeamSideStepCk(cEm35* em, int side)
{
    Em35Work* w = EM35_WK(em);
    f32 d = em35GetBeamDis(&em->pos, w->beamNo, side, em->ang.y);
    int ret;

    if (d > 12250000.0f) {
        EmRoutineSet(em, 1, 0x17, 0, side);
        ret = 1;
    } else if (d > 2250000.0f) {
        EmRoutineSet(em, 1, 0x16, 0, side);
        ret = 1;
    } else if (em35BeamSideCk(w->beamNo, side, em->ang.y) == 0) {
        ret = 0;
    } else {
        EmRoutineSet(em, 1, 0x18, 0, side);
        ret = 1;
    }
    return ret;
}

// 1 when beam `no` has a neighbour on the enemy's `side` (0 right / 1 left) for yaw `ang`: the
// facing (em35BeamDirCk) picks which of the side / front / back links that is.
int em35BeamSideCk(int no, int side, f32 ang)
{
    Em35Beam* b;
    int dir;

    if (no == 0xFF) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    dir = em35BeamDirCk(no, ang);
    if (side) {
        switch ((u32) dir) {
        case 0:
            dir = 1;
            break;
        case 1:
            dir = 0;
            break;
        case 2:
            dir = 3;
            break;
        case 3:
            dir = 2;
            break;
        }
    }
    switch ((u32) dir) {
    case 0:
        if (b->side[0] != 0xFF) {
            return 1;
        }
        break;
    case 1:
        if (b->side[1] != 0xFF) {
            return 1;
        }
        break;
    case 2:
        if (b->front != 0xFF) {
            return 1;
        }
        break;
    case 3:
        if (b->back != 0xFF) {
            return 1;
        }
        break;
    }
    return 0;
}

// Which way yaw `ang` points along beam `no`: 0 / 1 = +X / -X on an X beam, 2 / 3 = -Z / +Z on a Z
// beam (the link tables are indexed by it).
int em35BeamDirCk(int no, f32 ang)
{
    int dir;

    if (no == 0xFF) {
        return 0;
    }
    dir = 0;
    switch (em35_beam_tbl[no].type) {
    case 0:
    default:
        if (ang > 1.5707964f) {
            dir = 1;
        }
        if (ang < -1.5707964f) {
            dir = 1;
        }
        return dir;
    case 1:
        dir = 2;
        if (ang > 0.0f) {
            dir = 3;
        }
        return dir;
    }
}

// Damage of the pending hit: the weapon table value (near = within 6 m for the range falloff, 20
// for weapon ids past 0x2D), doubled on the whole body's exposed spine.
int em35SetDmVal(cEm35* em)
{
    int near;
    int dmg;

    near = 0;
    if (em->dmg.m_pDamageYarare->len < 16000000.0f) {
        near = 1;
    }
    {
        u32 no = em->dmg.m_Wep;

        dmg = 20;
        if (no <= 0x2D) {
            dmg = GetWepDmVal(em, no, near);
        }
    }
    if (em->type == 0 && em35WeakDmCk(em)) {
        dmg *= 2;
    }
    return dmg;
}

// The two-motion aim blend (the spear): m0 (sequence m3) blended with m1 (sequence a) for a
// negative blendRate or m2 (b) for a positive one, at weight |blendRate| / 256 through the second
// motion work; blendA is the interpolation count and blendB the frame, both kept in step here.
void em35BlendMotSet(cEm35* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int kind)
{
    Em35Work* w = EM35_WK(em);
    f32 rate = fabsf(w->blendRate);
    MotionWorkSub* bm;
    void* m;
    int seq;

    MotionSetCore(em, MOTION(em), m0, m3, (u8) w->blendA, (u16) kind, (u16) w->blendB);
    if (w->blendRate < 0.0f) {
        m = m1;
        seq = a;
    } else {
        m = m2;
        seq = b;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, (void*) seq, (u8) w->blendA, (u16) kind, (u16) w->blendB);
    em->Motion.blend = bm;
    bm->Brate = rate * (1.0f / 256.0f);
    if (w->blendA) {
        w->blendA--;
    }
    w->blendB++;
    if (w->blendB >= em->Motion.Seq_frame_num) {
        w->blendB = 0;
    }
}

// The pulsing organ (part 0x34): its scale breathes +-30 % on a sine that advances 9 deg per frame
// while the enemy lives.
void em35ScaleMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    cParts* p = em->getPartsPtr(0x34);
    f32 s = SINF(w->scaleAng) * 0.3f + 1.0f;

    p->scale.x = s;
    p->scale.y = s;
    p->scale.z = s;
    ScaleMatrix(p->mat, &p->scale);
    if (em->hp > 0) {
        w->scaleAng += 0.15707964f;
        w->scaleAng = LIMIT_ANGLE(w->scaleAng);
    }
}

// For the level script: puts the enemy into its cutscene death pose at the scene spot (routine 3/1).
void cEm35::setDiePose()
{
    atari.throughOn();
    pos.x = 36817.0f;
    pos.y = -7963.56f;
    pos.z = -59757.32f;
    ang.x = 0.0f;
    ang.y = 0.0f;
    ang.z = 0.0f;
    MotionSetCore(this, MOTION(this), PL_ARC_PTR(subArc, 0x7B), 0, 0, 1, 0);
    MotionMove(this, 0);
    EmRoutineSet(this, 3, 1, 0, 0);
}

// For the level script (after the divide cutscene): places the upper body at its start spot facing
// down the hall in the idle pose, cloth on, and starts its beam behaviour at U_Wait (1/0xF).
void cEm35::setUpperStart()
{
    Em35Work* w = EM35_WK(this);

    pos.x = 36500.0f;
    pos.y = -8000.0f;
    pos.z = -58220.0f;
    ang.x = 0.0f;
    ang.y = PI;
    ang.z = 0.0f;
    MotionSetCore(this, MOTION(this), PL_ARC_PTR(subArc, 0x48), 0, 0, 5, 0);
    MotionMove(this, 0);
    w->flags &= ~0x40;
    be_flag |= 0x00200000;
    EmRoutineSet(this, 1, 0xF, 0, 0);
}

// Starts the big step (1/2) when the target is straight ahead (30 deg) beyond 7.5 m, the enemy is
// at or under 80 % HP, Game_level is at most 1 and the 10 m ahead are clear. Returns 1 when started.
int em35BigStepCk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Vec a;
    Vec b;

    if (w->targetAngAbs > 0.5235988f) {
        return 0;
    }
    if (w->targetDist < 56250000.0f) {
        return 0;
    }
    if (em->hp > (s16) (em->hp_max / 10) * 8) {
        return 0;
    }
    if (pG->Game_level > 1) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 10000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    EmRoutineSet(em, 1, 2, 0, 0);
    return 1;
}

// Whole body only: creates the four weak point objects (obj00, 0.7x) hooked to the spine parts 2..5,
// hidden until em35WeakMove shows them.
void em35WeakInit(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 0) {
        Vec pos;
        Vec rot;
        u32 i;

        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        for (i = 0; i < 4; i++) {
            w->pWeak[i] = SetObj00(ARC(0x9B), ARC(0x9C), &pos, &rot);
            if (w->pWeak[i]) {
                w->pWeak[i]->scale.x = 0.7f;
                w->pWeak[i]->scale.y = 0.7f;
                w->pWeak[i]->scale.z = 0.7f;
                w->pWeak[i]->LightInfo.EnableMask = 0x80;
                OyaSetObj00(w->pWeak[i], em, i + 2);
                w->pWeak[i]->atari.throughOn();
            }
        }
    }
}

// Whole body only, per frame: the weak point objects are displayed while Status_flg[1] 0x04000000
// (weak points shown) is set and the enemy lives.
void em35WeakMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    u32 i;

    if (em->type != 0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        if (w->pWeak[i]) {
            if (StaFlagChk(pG, STA_THERMO_GRAPH) && em->hp > 0) {
                w->pWeak[i]->be_flag |= 2;
            } else {
                w->pWeak[i]->be_flag &= ~2;
            }
        }
    }
}
