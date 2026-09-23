// em39 module (D:/Bio4/Prog/em39.cpp): the knife-fight / second-battle boss enemy. Types 0/1 are
// the ruins stalker of the first battle: he hides (Hide), appears at EMI points with the machine
// gun, grenades, flash grenades or the bow (AppearMG / AppearGR / AppearBow / Flash / Atk_MG /
// ThrowGR), runs the walls and jumps (JumpUp* / JumpDown / FanceJump / SuperDash) and fights with
// the knife (AtkKnife / KnifeCatch / Knife4Atk), pausing for the scripted talks (Talk1st / Talk2nd).
// Type 2 is the final battle: the opening knife exchange's outcome (Success / Failure, then the
// cliff grab), then the mutated arm (em39ArmControl, the T_ routines: T_Atk / T_LongAtk / T_JumpAtk
// / T_Kick / T_LowKick / T_CliffAtk) with the Dm_T_* reactions. Both share the floor routines (Wait /
// Walk / Run / Goto / Turn180 / Threat / Escape / Backjump / Step / Slant) and the player callbacks
// (plem39_*).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em_mod.h"
#include "objWep.h"
// emwep.h declares the DOL's plemBackjump (game/emwep.cpp); this unit has a local routine of the
// same name, so the header's declaration is renamed out of the way.
#define plemBackjump plemBackjump_emwep
#include "em39.h"
#include "em10.h"
#undef plemBackjump
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "emdoor.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "act_btn.h"
#include "sscrn.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pl_npc.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "dbmodule.h"
#include "quake.h"
#include "item.h"
#include "sce_at.h"
#include "game.h"
#include "em.h"
#include <dolphin/os.h>
#include "wep_mod.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em39,52,4");


static void em39_R0_Init(cEm39* em);
static void em39_R0_Move(cEm39* em);
static void em39_R0_Damage(cEm39* em);
static void em39_R0_Die(cEm39* em);
static void em39_R1_br_Dummy(cEm39* em);
static void em39_R1_Talk1st(cEm39* em);
static void em39_R1_Talk2nd(cEm39* em);
static void em39_R1_Success(cEm39* em);
static void plem39_Success(cPlayer* pl);
static void em39_R1_Failure(cEm39* em);
static void plem39_Failure(cPlayer* pl);
static void em39_R1_Wait(cEm39* em);
static void em39_R1_Sit(cEm39* em);
static void em39_R1_SitDown(cEm39* em);
static void em39_R1_WallWait(cEm39* em);
static void em39_R1_Walk(cEm39* em);
static void em39_R1_Run(cEm39* em);
static void em39_R1_Goto(cEm39* em);
static void em39_R1_Turn180(cEm39* em);
static void em39_R1_Threat(cEm39* em);
static void em39_R1_Escape(cEm39* em);
static void em39_R1_Backjump(cEm39* em);
static void em39_R1_Step(cEm39* em);
static void em39_R1_Slant(cEm39* em);
static void em39_R1_Slant2(cEm39* em);
static void em39_R1_SuperDash(cEm39* em);
static void em39_R1_JumpDown(cEm39* em);
static void em39_R1_JumpUp(cEm39* em);
static void em39_R1_JumpUp2(cEm39* em);
static void em39_R1_JumpUp3(cEm39* em);
static void em39_R1_FanceJump(cEm39* em);
static void em39_R1_AtkKnife(cEm39* em);
static void em39_R1_AtkDoor(cEm39* em);
static void em39_R1_br_KnifeCatch(cEm39* em);
static void em39_R1_KnifeCatch(cEm39* em);
static void em39_R1_KnifeHit(cEm39* em);
static void plem39_KnifeHit(cPlayer* pl);
static void em39_R1_Knife4Atk(cEm39* em);
static void plem39_Knife4Atk(cPlayer* pl);
static void em39_R1_Atk_MG(cEm39* em);
static void em39_R1_Reload(cEm39* em);
static void em39_R1_AppearMG(cEm39* em);
static void em39_R1_AppearMG2(cEm39* em);
static void em39_R1_AppearGR(cEm39* em);
static void em39_R1_AppearGR2(cEm39* em);
static void em39_R1_ThrowGR(cEm39* em);
static void em39_R1_AppearBow(cEm39* em);
static void em39_R1_Flash(cEm39* em);
static void em39_R1_Hide(cEm39* em);
static void em39_R1_br_T_Atk(cEm39* em);
static void em39_R1_T_Atk(cEm39* em);
static void em39_R1_T_BackKnuckle(cEm39* em);
static void em39_R1_br_T_LongAtk(cEm39* em);
static void em39_R1_T_LongAtk(cEm39* em);
static void em39_R1_T_JumpAtk(cEm39* em);
static void plem39_Stamp(cPlayer* pl);
static void em39SitAction(cEm39* em);
static void plem39Sit(cPlayer* pl);
static void em39BackjumpAction(cEm39* em);
static void plemBackjump(cPlayer* pl);
static void em39_R1_br_T_Kick(cEm39* em);
static void em39_R1_T_Kick(cEm39* em);
static void em39_R1_br_T_LowKick(cEm39* em);
static void em39_R1_T_LowKick(cEm39* em);
static void em39_R1_T_LowKickHit(cEm39* em);
static void plem39_LowKickHit(cPlayer* pl);
static void em39_R1_T_CliffAtk(cEm39* em);
static void plem39_CliffAtk(cPlayer* pl);
static void em39_R1_Dm_Normal(cEm39* em);
static void em39_R1_Dm_Head(cEm39* em);
static void em39_R1_Dm_Blow(cEm39* em);
static void em39_R1_Dm_T_Head(cEm39* em);
static void em39_R1_Dm_T_Down(cEm39* em);
static void em39_R1_Dm_T_DownHead(cEm39* em);
static void em39_R1_Die_Normal(cEm39* em);
static void em39_R1_Die_Flash(cEm39* em);
static void em39ActOn(cEm39* em);
static void plemDmSide(cPlayer* pl);


extern "C" void _prolog()
{
    OSReport("em39 prolog Ok\n");
    EmInitFunc = Em39Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

// Placement-constructs the boss over the manager's cEm slot (EmInitFunc for id 0x39); em39_R0_Init
// sets it up on the first move.
void Em39Init(cEm* em)
{
    new (em) cEm39();
}

// Destroys the held weapon enemies (knife, machine gun, bow, thrown knife) and the cap object that
// are still alive when the boss is removed.
cEm39::~cEm39()
{
    Em39Work* w = EM39_WK(this);

    if (w->pKnife && w->pKnife->isAlive()) {
        EmMgr.destroy(w->pKnife);
    }
    if (w->pMachineGun && w->pMachineGun->isAlive()) {
        EmMgr.destroy(w->pMachineGun);
    }
    if (w->pBow && w->pBow->isAlive()) {
        EmMgr.destroy(w->pBow);
    }
    if (w->pArrow && w->pArrow->isAlive()) {
        EmMgr.destroy(w->pArrow);
    }
    if (w->pCap && w->pCap->isAlive()) {
        ObjMgr.destroy(w->pCap);
    }
}

// Keeps the boss and its held objects (knife, machine gun, bow, thrown knife, cap) running while
// the scene is suspended (be_flag 0x800), or lets them suspend again.
void cEm39::setNoSuspend(int on)
{
    Em39Work* w = EM39_WK(this);

    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    if (w->pKnife) {
        w->pKnife->setNoSuspend(on);
    }
    if (w->pMachineGun) {
        w->pMachineGun->setNoSuspend(on);
    }
    if (w->pBow) {
        w->pBow->setNoSuspend(on);
    }
    if (w->pArrow) {
        w->pArrow->setNoSuspend(on);
    }
    if (w->pCap) {
        w->pCap->setNoSuspend(on);
    }
}

// Per-frame damage reaction, from move(). Area damage (DmgMgr kinds 1 / 4 / 5 / 7) once per 120
// frames: 200 (knife fight, floored at 1 HP) or 100 (second battle), death (routine 3/1, except in
// room 0x31C where the script handles it) or the flinch (knife fight 2/0; second battle 2/4, or 2/1
// while the arm is up, Be_flg 0x1000). A weapon hit in dmHit: a knife parry (em39GuardCk) only
// clinks; otherwise em39SetDmVal, blood and hit sound, death as above. Reactions are held off by
// Be_flg 0x100 (invulnerable) / 8 (one running); in the second battle a head hit (part 5) gives
// the head flinch (2/3, or 2/5 when downed, Be_flg 0x10000) and 200 accumulated damage the down
// (2/4, not while the arm is up); in the knife fight a head hit gives 2/1, heavy weapons flinch at
// once, handgun-class / shotgun past 200 accumulated, and other weapons (grenades) blow him away (2/2).
void em39DmCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    YARARE_INFO* hit;
    int dmg;

    if (em->hp > 0 && !EmDeadCk(em)) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            if (w->Fire_timer == 0) {
                if (w->Be_flg & 0x100) {
                    return;
                }
                w->Fire_timer = 120;
                if (em->type != 2) {
                    LifeDownSet2(em, 200, 0, 1);
                    w->Appear_damage += 200;
                    w->Total_damage += 200;
                    w->Flash_damage += 200;
                } else {
                    LifeDownSet2(em, 100, 0, 0);
                    w->Appear_damage += 100;
                    w->Total_damage += 100;
                    w->Flash_damage += 100;
                }
                if (em->hp <= 0) {
                    EmSetDie(em);
                    EmSetDieCnt(em);
                    if (pG->room_id == 0x31C) {
                        return;
                    }
                    EmRoutineSet(em, 3, 1, 0, 0);
                    return;
                }
                if (w->Be_flg & 8) {
                    return;
                }
                if (em->type == 2) {
                    if (w->Be_flg & 0x1000) {
                        w->Total_damage = 0;
                        EmRoutineSet(em, 2, 1, 0, 0);
                    } else {
                        w->Total_damage = 0;
                        EmRoutineSet(em, 2, 4, 0, 0);
                    }
                } else {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
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
    hit = em->dmg.m_pDamageYarare;
    if (em39GuardCk(em)) {
        EmDmBloodSet2(em, 0x2F, 0x2D, 0, 0, 0);
        SndCall(8, 0x4C, &pPL->pos, em->id, 0, pPL);
        return;
    }
    dmg = em39SetDmVal(em);
    if (em->type != 2) {
        LifeDownSet2(em, dmg, 0, 1);
    } else {
        LifeDownSet2(em, dmg, 0, 0);
    }
    w->Appear_damage += dmg;
    em39BloodSet(em);
    SndCall(8, 0xD, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCnt(em);
        if (pG->room_id == 0x31C) {
            return;
        }
        EmRoutineSet(em, 3, 1, 0, 0);
        return;
    }
    if (w->Be_flg & 0x100) {
        return;
    }
    w->Total_damage += dmg;
    w->Flash_damage += dmg;
    if (em->type == 2) {
        if (hit->parts_no == 5) {
            w->Total_damage = 0;
            if (w->Be_flg & 0x10000) {
                EmRoutineSet(em, 2, 5, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
            return;
        }
        if (w->Be_flg & 0x1000) {
            return;
        }
        if (w->Total_damage <= 200) {
            return;
        }
        w->Total_damage = 0;
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
    if (w->Be_flg & 8) {
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
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (hit->parts_no == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else if (w->Total_damage > 200) {
            w->Total_damage = 0;
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 9:
    case 0xA:
    case 0x28:
        if (hit->parts_no == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 0x10:
    case 0x1A:
        if (hit->parts_no == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (hit->parts_no == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else if (w->Total_damage > 200) {
            w->Total_damage = 0;
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 0xF:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
        if (hit->parts_no == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 0xE:
        break;
    case 0xD:
    case 0x12:
    default:
        EmRoutineSet(em, 2, 2, 0, 0);
        break;
    case 0x17:
    case 0x2A:
        EmRoutineSet(em, 2, 1, 0, 0);
        break;
    }
}

Em39Func Em39_R0_move_tbl[4] = {
    em39_R0_Init,
    em39_R0_Move,
    em39_R0_Damage,
    em39_R0_Die,
};

// Routine 1 handlers: the branch check and the move of each sub-routine (cModel::xFD), as one
// flat table (`tbl[xFD * 2]` / `tbl[xFD * 2 + 1]`: `slwi 3; ori 4`).
static Em39Func Em39_R1_move_tbl[94] = {
    em39_R1_br_Dummy, em39_R1_Talk1st,  // 0x00
    em39_R1_br_Dummy, em39_R1_Talk2nd,  // 0x01
    em39_R1_br_Dummy, em39_R1_Success,  // 0x02
    em39_R1_br_Dummy, em39_R1_Failure,  // 0x03
    em39_R1_br_Dummy, em39_R1_Wait,  // 0x04
    em39_R1_br_Dummy, em39_R1_Sit,  // 0x05
    em39_R1_br_Dummy, em39_R1_SitDown,  // 0x06
    em39_R1_br_Dummy, em39_R1_WallWait,  // 0x07
    em39_R1_br_Dummy, em39_R1_Walk,  // 0x08
    em39_R1_br_Dummy, em39_R1_Run,  // 0x09
    em39_R1_br_Dummy, em39_R1_Goto,  // 0x0A
    em39_R1_br_Dummy, em39_R1_Turn180,  // 0x0B
    em39_R1_br_Dummy, em39_R1_Threat,  // 0x0C
    em39_R1_br_Dummy, em39_R1_Escape,  // 0x0D
    em39_R1_br_Dummy, em39_R1_Backjump,  // 0x0E
    em39_R1_br_Dummy, em39_R1_Step,  // 0x0F
    em39_R1_br_Dummy, em39_R1_Slant,  // 0x10
    em39_R1_br_Dummy, em39_R1_Slant2,  // 0x11
    em39_R1_br_Dummy, em39_R1_SuperDash,  // 0x12
    em39_R1_br_Dummy, em39_R1_JumpDown,  // 0x13
    em39_R1_br_Dummy, em39_R1_JumpUp,  // 0x14
    em39_R1_br_Dummy, em39_R1_JumpUp2,  // 0x15
    em39_R1_br_Dummy, em39_R1_JumpUp3,  // 0x16
    em39_R1_br_Dummy, em39_R1_FanceJump,  // 0x17
    em39_R1_br_Dummy, em39_R1_AtkKnife,  // 0x18
    em39_R1_br_Dummy, em39_R1_AtkDoor,  // 0x19
    em39_R1_br_KnifeCatch, em39_R1_KnifeCatch,  // 0x1A
    em39_R1_br_Dummy, em39_R1_KnifeHit,  // 0x1B
    em39_R1_br_Dummy, em39_R1_Knife4Atk,  // 0x1C
    em39_R1_br_Dummy, em39_R1_Atk_MG,  // 0x1D
    em39_R1_br_Dummy, em39_R1_Reload,  // 0x1E
    em39_R1_br_Dummy, em39_R1_AppearMG,  // 0x1F
    em39_R1_br_Dummy, em39_R1_AppearMG2,  // 0x20
    em39_R1_br_Dummy, em39_R1_AppearGR,  // 0x21
    em39_R1_br_Dummy, em39_R1_AppearGR2,  // 0x22
    em39_R1_br_Dummy, em39_R1_ThrowGR,  // 0x23
    em39_R1_br_Dummy, em39_R1_AppearBow,  // 0x24
    em39_R1_br_Dummy, em39_R1_Flash,  // 0x25
    em39_R1_br_Dummy, em39_R1_Hide,  // 0x26
    em39_R1_br_T_Atk, em39_R1_T_Atk,  // 0x27
    em39_R1_br_Dummy, em39_R1_T_BackKnuckle,  // 0x28
    em39_R1_br_T_LongAtk, em39_R1_T_LongAtk,  // 0x29
    em39_R1_br_Dummy, em39_R1_T_JumpAtk,  // 0x2A
    em39_R1_br_T_Kick, em39_R1_T_Kick,  // 0x2B
    em39_R1_br_T_LowKick, em39_R1_T_LowKick,  // 0x2C
    em39_R1_br_Dummy, em39_R1_T_LowKickHit,  // 0x2D
    em39_R1_br_Dummy, em39_R1_T_CliffAtk,  // 0x2E
};

static Em39Func Em39_R1_dmg_tbl[6] = {
    em39_R1_Dm_Normal,
    em39_R1_Dm_Head,
    em39_R1_Dm_Blow,
    em39_R1_Dm_T_Head,
    em39_R1_Dm_T_Down,
    em39_R1_Dm_T_DownHead,
};

static Em39Func Em39_R1_die_tbl[2] = {
    em39_R1_Die_Normal,
    em39_R1_Die_Flash,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em39_flip_tbl[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x21, 0x20, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
    0x3F, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Machine gun damage handed to EmAtkSetDamagePL (em39GunHitCk).
static EmAtkInfo em39_gun_atk_info = { 300.0f, PL_DM_AUTO, 800, 0, 0xA, 0 };

// Melee attack table (em39AtkCk / em39AtkCk2 index it by attack number).
static EmAtkInfo em39_atk_tbl[10] = {
    { 300.0f, PL_DM_AUTO, 1500, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 800, 0, 0xA, 0 },
    { 6000.0f, PL_DM_AUTO, 1000, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 1000, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 1000, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 1500, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 1500, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 500, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 500, 0, 0xA, 0 },
    { 200.0f, PL_DM_AUTO, 500, 0, 0xA, 0 },
};

// Per-frame update from the enemy manager. Order: damage, the tower proximity check, clear the
// per-frame flags, tick the attack / dash / escape / hide / trap timers (the back attack wait only
// while Be_flg 0x800), route, the routine table (r_no_0 0xFF = model load failed: destroy), the
// mutated arm, neck and waist aim, parts, attack / collision / stage collision (skipped during the
// jump motions, seFlags 0x40, which also pass pushes), HoseiCnt, the invisibility fade (Be_flg 0x400
// = hidden), the shadow fade while airborne / on steep floors / when the camera is below, the
// marker, voice and speech, the "not shooting" counter, footsteps, the player's voice reactions,
// the em-list HP mirror and the shadow colour.
void cEm39::move()
{
    Em39Work* w = EM39_WK(this);
    f32 dist;

    if (r_no_0) {
        em39DmCk(this);
    }
    em39PLNearTowerCk(this);
    w->Be_flg &= 0xFE6AEA20;
    if (w->Atk_wait) {
        w->Atk_wait--;
    }
    if (w->LongAtk_wait) {
        w->LongAtk_wait--;
    }
    if (w->SuperDashWait) {
        w->SuperDashWait--;
    }
    if (w->Escape_wait) {
        w->Escape_wait--;
    }
    if (w->Hide_timer) {
        w->Hide_timer--;
    }
    if (w->Fire_timer) {
        w->Fire_timer--;
    }
    if (w->Dash_wait) {
        w->Dash_wait--;
    }
    if ((w->Be_flg & 0x800) && w->Back_atk_wait) {
        w->Back_atk_wait--;
    }
    clearStatus(EM_STATUS_IK_OFF);
    em39RouteCk(this);
    Em39_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (r_no_0) {
        em39ArmControl(this);
    }
    em39NeckMove(this);
    em39WaistMove(this);
    partsWorldCalc();
    dist = VEC_DISTXZ(&pos_old, &pos);
    if (Motion.Seq_old.Free & 0x40) {
        atari.m_flag |= 0x10;
    } else {
        atari.m_flag &= ~0x10;
    }
    EmAtCheck(this);
    atari.move();
    if (!(Motion.Seq_old.Free & 0x40)) {
        SatMgr.check(this, 0);
    }
    if (VEC_DISTXZ(&pos, &pos_old) < dist * 0.5f) {
        w->HoseiCnt++;
    } else {
        w->HoseiCnt = 0;
    }
    if (w->Be_flg & 0x400) {
        invisible_factor = 0.0f;
    } else {
        invisible_factor += 0.1f;
        if (invisible_factor > 1.0f) {
            invisible_factor = 1.0f;
        }
    }
    if (Motion.Seq_old.Free & 0x40) {
        if (Shd_color <= 0xF6) {
            Shd_color += 8;
        } else {
            Shd_color = 0xFF;
        }
    } else {
        if (Shd_color > 8) {
            Shd_color -= 8;
        } else {
            Shd_color = 0;
        }
    }
    em39MarkerMove(this);
    em39VoiceMove(this);
    em39SpeechMove(this);
    if (be_flag & 2) {
        if (StaFlagChk(pG, STA_PL_FIRE)) {
            w->No_fire_timer = 0;
        } else {
            w->No_fire_timer++;
        }
    } else {
        w->No_fire_timer = 0;
    }
    em39FootEff(this);
    em39PLVoiceCk(this);
    if (pG->room_id != 0x31C) {
        (&pG->Em_list[emset_no])->hp = hp;
    }
    {
        int hide = 0;
        Camera* cam = &pG->Camera;
        Vec* nrm = pFloor_norm;
        if (nrm == 0 || nrm->y < 0.8f) {
            hide = 1;
        }
        if (cam->param.pos.y < pos.y) {
            hide = 1;
        }
        if ((w->Be_flg & 0x01000000) || hide) {
            if (Shd_color <= 0xF6) {
                Shd_color += 8;
            } else {
                Shd_color = 0xFF;
            }
        } else {
            if (Shd_color > 8) {
                Shd_color -= 8;
            } else {
                Shd_color = hide;
            }
        }
    }
}

// Routine 0: one-time setup. Types 0 / 1 load the first-battle body (archive 0x15) with its extra
// models, the beret as a hanging obj12 on the head, the knife model, the hand models (em39HandSet),
// and the knife / machine gun / bow weapon enemies parented to the hands (gun and bow hidden);
// type 2 loads the final-battle body (0x1A) with its extras and knife model. Then foot shadows,
// the flip table, a huge light box, a 1 m collision, the body hit box plus the extras, effect data
// and the work (long attack wait 300, back attack / super dash waits 450, cap hp 10). Start by
// `set`: types 0 / 1 hidden (Hide, 1/0x26; set 1 waits, set 4 sits), type 2 Wait (sets 0 / 1) or
// the knife exchange outcome scenes (set 2 Success, set 3 Failure).
static void em39_R0_Init(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    cModelInfo* info;
    Vec pos;
    Vec rot;
    int one;
    u8 zero;   // only byte fields take it: a promoted QI variable stores directly (`stb r30`)
    // COMPILER-DIFF: #13 -- the original never allocates its REG_EQUIV constants: the shared literal
    // zero is reload's callee-saved r20, the 0x1D/300/10/-1/450 init constants its spill registers
    // r0/r8/r11/r9/r10 and the subArc load of the routine MotionSetCore its r11, and the post-call
    // init block is issued in pure source order (no store carries a register death). Pinned here,
    // except r0, which the allocator picks by itself.
    register int z0 asm("r20");
    int r0c;
    register int r8c asm("r8");
    register int r9c asm("r9");
    register int r10c asm("r10");
    register int r11c asm("r11");
    register PlArc* arc11 asm("r11");

    switch (em->type) {
    case 1:
    default:
        if (em->modelInit(ARC(0x15), ARC(0x17)) == 0) {
            pLog->err(0, 0, "em39() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(5), ARC(0x10));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(6), ARC(0x18));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(7), ARC(0x12));
        if (info) {
            em->addModel(info);
        }
        {
            Vec pos2;
            Vec rot2;

            pos2.x = 0.0f;
            pos2.y = 146.0f;
            pos2.z = 22.6f;
            rot2.x = 0.0f;
            rot2.y = 0.0f;
            rot2.z = 0.0f;
            w->pCap = SetObj12(ARC(0x16), ARC(0x19), &pos2, &rot2);
            if (w->pCap) {
                w->pCap->setParent(em, 4, 1);
            }
        }
        w->pModKnife = ModInfoMgr.create(ARC(9), ARC(0x10));
        if (w->pModKnife) {
            em->addModel(w->pModKnife);
        }
        w->Hand_type = 0xFF;
        em39HandSet(em, 1);
        break;
    case 2:
        if (em->modelInit(ARC(0x1A), ARC(0x1D)) == 0) {
            pLog->err(0, 0, "em39() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(5), ARC(0x10));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(6), ARC(0x1E));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(0x1B), ARC(0x1F));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(0x1C), ARC(0x20));
        if (info) {
            em->addModel(info);
        }
        w->pModKnife = ModInfoMgr.create(ARC(9), ARC(0x10));
        if (w->pModKnife) {
            em->addModel(w->pModKnife);
        }
        w->Hand_type = 0xFF;
        em39HandSet(em, 1);
        break;
    }
    em->be_flag |= 0x01000000;
    w->pKnife = 0;
    if (em->type != 2) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pKnife = SetWeapon(ARC(0x26), ARC(0x25), &pos, &rot, 0);
        if (w->pKnife) {
            w->pKnife->setParent(em, 0xA, 0);
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
    }
    w->pMachineGun = 0;
    if (em->type != 2) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pMachineGun = SetWeapon(ARC(0x2C), ARC(0x2B), &pos, &rot, 0);
        if (w->pMachineGun) {
            w->pMachineGun->setParent(em, 0xA, 0);
            w->pMachineGun->setTransMode(0);
        }
    }
    w->pBow = 0;
    if (em->type != 2) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pBow = SetWeapon(ARC(0x32), ARC(0x31), &pos, &rot, 0);
        if (w->pBow) {
            w->pBow->setParent(em, 0x10, 0);
            w->pBow->setTransMode(0);
        }
    }
    z0 = 0;
    w->pArrow = (cEmWep*) z0;
    em->pFsdTbl = &Em39_fs_tbl;
    em->Motion.flip = em39_flip_tbl;
#line 1268 "D:/Bio4/Prog/em39.cpp"
    em->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xD);
    // GNU constructor expressions: emitted at the statement like strings, and shared through the
    // constant hash (plem39_CliffAtk reuses this zero vector).
    em->LightInfo.init2(0, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 10000.0f, 10000.0f, 10000.0f }), 2);
    em->atari.init(0.0f, 1000.0f, 0.0f, 500.0f, 400.0f, 400.0f, 1000.0f, 1, 0x2000, 10);
    em->litArea.on(1);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 130.0f, 100.0f, 5, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, -30.0f, 0.0f, 200.0f, 300.0f, 2, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, YAT_FLAG_ON);
    one = 1;
    YarareAdd(em, &w->hit[2], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[3], -300.0f, 0.0f, 0.0f, 100.0f, 400.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 400.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[5], -20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x13, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[6], 20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x17, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    if (em->type == 2) {
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 50.0f, 60.0f, 0x62, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 50.0f, 60.0f, 0x63, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[11], 200.0f, -50.0f, 0.0f, 450.0f, 70.0f, 100.0f, 0x64, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 50.0f, 60.0f, 0x65, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[13], -120.0f, -30.0f, -50.0f, 250.0f, 60.0f, 100.0f, 0x66, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[14], -120.0f, -30.0f, 0.0f, 250.0f, 60.0f, 50.0f, 0x7B, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[15], -120.0f, -30.0f, 0.0f, 300.0f, 60.0f, 50.0f, 0x7C, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[16], -120.0f, -30.0f, 0.0f, 300.0f, 60.0f, 50.0f, 0x7D, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[17], -120.0f, -30.0f, 0.0f, 400.0f, 60.0f, 50.0f, 0x7E, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
        YarareAddCube(em, &w->hit[18], -120.0f, -30.0f, 0.0f, 250.0f, 60.0f, 50.0f, 0x7F, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    }
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), EFF_EM39, 0);
    r0c = 0x1D;
    r8c = 300;
    r11c = 10;
    r9c = -1;
    r10c = 450;
    w->Neck_dir_y = 0.0f;
    w->pDoor = 0;
    w->Be_flg = z0;
    w->Neck_dir_x = 0.0f;
    w->Atk_wait = z0;
    w->Route_type = z0;
    w->pGotoPoint = 0;
    w->Arm_rno = z0;
    w->Hide_timer = z0;
    w->Appear_damage = z0;
    w->Fire_timer = z0;
    w->Flash_damage = z0;
    zero = 0;
    asm("" : "+r"(zero) : "r"(z0), "f"(0.0f)); // COMPILER-DIFF: #13
    w->Locate = zero;
    w->Arm_se_wait = r0c;
    w->LongAtk_wait = r8c;
    w->Cap_hp = r11c;
    w->Old_no = r9c;
    w->Back_atk_wait = r10c;
    w->SuperDashWait = r10c;
    w->EffKindId = EspPullCoreKind();
    w->EffKindIdArrow = EspPullCoreKind();
    if (em->type != 2) {
        int rtn;

        em->setStatus(EM_STATUS_ACTIVE);
        rtn = em->set;
        // The routine bytes are stored in every arm (jump2 cross-jumps the identical tails): in their own
        // blocks the QI stores precede the subArc load, and the dying `xFF` store leads the `xFE` one.
        switch (rtn) {
        case 0:
        default:
            rtn = 0x26;
            em->r_no_0 = one;
            em->r_no_1 = rtn;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        case 1:
            rtn = 4;
            em->r_no_0 = one;
            em->r_no_1 = rtn;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        case 4:
            em->r_no_0 = one;
            em->r_no_1 = rtn;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        }
        arc11 = em->subArc;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(arc11, 0x73), PL_ARC_PTR(arc11, 0x74), 0, 1, 0);
    } else {
        int no;

        em->setStatus(EM_STATUS_ACTIVE);
        no = em->set;
        switch (no) {
        case 0:
        default:
            em->r_no_0 = one;
            em->r_no_1 = 4;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        case 1:
            em->r_no_0 = one;
            em->r_no_1 = 4;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        case 2:
            em->r_no_0 = one;
            em->r_no_1 = no;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        case 3:
            em->r_no_0 = one;
            em->r_no_1 = no;
            em->r_no_2 = zero;
            em->r_no_3 = zero;
            break;
        }
        MotionSetCore(em, MOTION(em), ARC(0xF5), 0, 0, 1, 0);
    }
    MotionMove(em, 0);
    em39_R0_Move(em);
}

// Types 0 / 1: swaps the hand models for pose `type` (0 / 1 / 3 the open hands, 2 the gripping
// hands), keeping the current one when unchanged (Hand_type).
void em39HandSet(cEm39* em, int type)
{
    Em39Work* w = EM39_WK(em);
    cModelInfo* info;
    void* bin;

    if (w->Hand_type == type) {
        return;
    }
    w->Hand_type = type;
    switch ((u32) type) {
    case 0:
    default:
        bin = ARC(0xA);
        break;
    case 1:
        bin = ARC(0xB);
        break;
    case 2:
        bin = ARC(0xC);
        break;
    case 3:
        bin = ARC(0xD);
        break;
    }
    info = ModInfoMgr.create(bin, ARC(0x14));
    if (info) {
        if (w->pHandR) {
            em->swapModelInfo(w->pHandR->model_addr, info);
        } else {
            em->addModel(info);
        }
        w->pHandR = info;
        info->be_flag |= 8;
    }
    if (em->type == 2) {
        return;
    }
    switch ((u32) type) {
    case 0:
    default:
        bin = ARC(0xE);
        break;
    case 1:
        bin = ARC(0xE);
        break;
    case 2:
        bin = ARC(0xF);
        break;
    case 3:
        bin = ARC(0xE);
        break;
    }
    info = ModInfoMgr.create(bin, ARC(0x14));
    if (info) {
        if (w->pHandL) {
            em->swapModelInfo(w->pHandL->model_addr, info);
        } else {
            em->addModel(info);
        }
        w->pHandL = info;
        info->be_flag |= 8;
    }
}

// Swaps the body for the death model (archive 0x21 / 0x22).
void em39DieModelSet(cEm39* em)
{
    cModelInfo* info = ModInfoMgr.create(ARC(0x21), ARC(0x22));

    if (info) {
        em->swapModelInfo(em->pModelInfo->model_addr, info);
    }
}

// Routine 1: runs the branch check and then the behaviour of the current r_no_1 state.
static void em39_R0_Move(cEm39* em)
{
    Em39_R1_move_tbl[em->r_no_1 * 2](em);
    Em39_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the states that have none.
static void em39_R1_br_Dummy(cEm39* em)
{
}

// Routine 1/0 (setTalk1st, first battle): stands at the first talk spot in the idle pose, weapon
// away, collision off, invulnerable (dmType 2, Be_flg 0x30) while the cutscene dialogue runs.
static void em39_R1_Talk1st(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmg.m_Timer = 2;
    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        em->pos.x = 29231.0f;
        em->pos.y = 8658.0f;
        em->pos.z = -10844.0f;
        em->ang.y = 1.8654078f;
        MotionSetCore(em, MOTION(em), ARC(0xAB), 0, 0, 5, 0);
        em->be_flag |= 2;
        AtariOff(&em->atari, 0xFCFF);
        em39WepSet(em, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// Routine 1/1 (setTalk2nd): the same idle at the second talk spot.
static void em39_R1_Talk2nd(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmg.m_Timer = 2;
    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        em->pos.x = -719.0f;
        em->pos.y = 2000.0f;
        em->pos.z = -4022.0f;
        em->ang.y = -1.9547688f;
        MotionSetCore(em, MOTION(em), ARC(0xAB), 0, 0, 5, 0);
        em->be_flag |= 2;
        AtariOff(&em->atari, 0xFCFF);
        em39WepSet(em, 0);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        break;
    }
}

// Routine 1/2 (type 2, set 2): the knife exchange the player won: placed on the arena ledge, the
// parried motion with its effect (the arm pose 6 until motion event bit 0), the player playing his
// side (plem39_Success); then Be_flg 0x00800000 (ckBombCutEnable: the fight is on) and Wait.
static void em39_R1_Success(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = 220.09f;
        em->pos.y = 12000.0f;
        em->pos.z = -9402.42f;
        em->ang.y = -0.9032079f;
        MotionSetCore(em, MOTION(em), ARC(0xF1), ARC(0xF2), 0, 1, 0);
        SetPlDamage(em, plem39_Success);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x43, 0, w->EffKindId, em, 0);
        w->Arm_rno = 6;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            w->Be_flg |= 0x00800000;
            AtariOn(&em->atari, 0x300);
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (em->Motion.Seq_old.Free & 1) {
            AtariOn(&em->atari, 0x300);
            w->Arm_rno = 0;
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player damage callback of the won knife exchange: placed opposite the boss, the parry motion
// with the weapon hidden; the damage ends with it.
static void plem39_Success(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        AtariOff(&pl->atari, 0xFCFF);
        pl->pos.x = -1751.14f;
        pl->pos.y = 12000.0f;
        pl->pos.z = -7649.44f;
        pl->ang.y = 3.1415927f;
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x124), 0, 0, 1, 0);
        pl->Wep->setTrans(0, 0);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0)) {
            pl->Wep->setTrans(1, 0);
            AtariOn(&pl->atari, 0x300);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Routine 1/3 (set 3): the knife exchange the player lost: the slashing motion with its effect
// while the player takes the cut (plem39_Failure); then the cliff attack (stat -> T_CliffAtk 1/0x2E)
// when a cliff spot is at hand (em39GetCliffPos), else Wait.
static void em39_R1_Failure(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = 94.95f;
        em->pos.y = 12000.0f;
        em->pos.z = -9417.92f;
        em->ang.y = -0.8901179f;
        MotionSetCore(em, MOTION(em), ARC(0xF3), ARC(0xF4), 0, 1, 0);
        SetPlDamage(em, plem39_Failure);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x44, 0, w->EffKindId, em, 0);
        w->Arm_rno = 6;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em39GetCliffPos(em)) {
                EmRoutineSetW(em, 1, 0x2E, 0, 0);
            } else {
                AtariOn(&em->atari, 0x300);
                EmRoutineSet(em, 1, 4, 0, 0);
            }
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player damage callback of the lost knife exchange: placed opposite the boss, the slashed
// motion with its blood effect and pain face, weapon hidden; held (the cliff attack takes over).
static void plem39_Failure(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        AtariOff(&pl->atari, 0xFCFF);
        pl->pos.x = -1736.93f;
        pl->pos.y = 12000.0f;
        pl->pos.z = -7639.26f;
        pl->ang.y = 2.8536134f;
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x125), 0, 0, 1, 0);
        EstSet(pl, -1, 0, 0, EFF_EM39, 0x45, 0, ESP_CORE_KIND_NONE, pl, 0);
        PlSetFace(1);
        pl->Wep->setTrans(0, 0);
        pl->r_no_2++;
    case 1:
        MotionMove(pl, 0);
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Routine 1/4: idle (Be_flg 0x30: routine running, aims head). Type 2 first plays its arrival
// pose turning to the target. Then the idle loop; with a live player (not in set 1): a wall jump
// (em39JumpUpCk3), an about-face past 135 deg, the walk (Game_level up to 9) or the run; a dead
// player gets walked to. Being aimed at for more than 10 frames (em39LockCk): type 2 dodges
// (em39SlantCk2), types 0 / 1 dodge, step (1/0xF within 5 m) or escape (1/0xD). Be_flg 0x20000
// (leave the area) sends him into hiding at phase 5, else em39GotoCk may pick a point to go to.
static void em39_R1_Wait(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    if (em->type != 2 && em->r_no_2 == 0) {
        em->r_no_2 = 2;
    }
    switch (em->r_no_2) {
    case 0:
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        MotionSetCore(em, MOTION(em), ARC(0xF5), ARC(0xF6), 3, 1, 0xA);
        w->Arm_rno = 8;
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (em->type != 2) {
            MotionSetCore(em, MOTION(em), ARC(0x73), ARC(0x74), 0x1E, 5, 0);
        } else {
            w->Arm_rno = 0;
            MotionSetCore(em, MOTION(em), ARC(0xD6), 0, 0x1E, 5, 0);
        }
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        w->Arm_rno = 8;
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        em->r_no_2++;
    case 3:
        MotionMove(em, 0);
        if ((s16) pG->pl_life > 0 && em->hp > 0 && em->set != 1) {
            if (EmDeadCk(em)) {
                if (em39JumpUpCk3(em)) {
                    return;
                }
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                if (em->l_pl > 25000000.0f) {
                    w->Atk_wait = 0;
                }
                if (w->Atk_wait) {
                    break;
                }
                if (em39JumpUpCk3(em)) {
                    return;
                }
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (pG->Game_level <= 9) {
                    EmRoutineSet(em, 1, 8, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 9, 0, 0);
                }
            }
        }
        break;
    }
    if (em39LockCk(em)) {
        w->Lock_timer++;
        if (w->Lock_timer > 10) {
            if (em->type == 2) {
                if (em39SlantCk2(em)) {
                    return;
                }
                w->Lock_timer = 0;
            } else {
                if (w->Escape_wait == 0 || em39HeadLockCk(em)) {
                    if (em39SlantCk(em)) {
                        return;
                    }
                    if (em->l_pl < 25000000.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0xD, 0, 0);
                    }
                    return;
                }
            }
        }
    } else {
        w->Lock_timer = 0;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    } else {
        em39GotoCk(em);
    }
}

// Routine 1/5 (types 0 / 1): crouched at a perch point (pGotoPoint), collision off, walking the
// last 30 cm to it if far (r_no_3 skips the settle), turning to face the player once there. If the
// player closes within 5 m, is about to leave (em39ExitCk) or Be_flg 0x2000 is set, he relocates
// (em39AreaMoveCk) or hides for 200 frames; otherwise once Atk_wait is out he draws the bow (1/0x24),
// the machine gun (1/0x1F) or a grenade (1/0x21). Be_flg 0x20000 sends him into hiding at phase 5.
static void em39_R1_Sit(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        w->TmpU32 = 0;
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 0, 5, 0);
        } else {
            int far;

            em39SitChg(em);
            far = 0;
            if (w->pGotoPoint && (em->pos.x - w->pGotoPoint->pos.x) * (em->pos.x - w->pGotoPoint->pos.x) + (em->pos.z - w->pGotoPoint->pos.z) * (em->pos.z - w->pGotoPoint->pos.z) > 90000.0f) {
                far = 1;
            }
            if (far) {
                MotionSetCore(em, MOTION(em), ARC(0x62), ARC(0x63), 3, 5, 0);
                w->TmpU32 = 1;
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 0x1E, 5, 0);
            }
        }
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        AtariOff(&em->atari, 0xFCFF);
        if (w->pGotoPoint) {
            if ((em->pos.x - w->pGotoPoint->pos.x) * (em->pos.x - w->pGotoPoint->pos.x) + (em->pos.z - w->pGotoPoint->pos.z) * (em->pos.z - w->pGotoPoint->pos.z) < 22500.0f) {
                if (w->TmpU32) {
                    MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 0x1E, 5, 0);
                    w->TmpU32 = 0;
                }
                em->pos = w->pGotoPoint->pos;
                em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.09817477f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            } else {
                em->ang.y += Muku(&em->pos, &w->pGotoPoint->pos, em->ang.y, 0.7853982f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            }
        }
        if ((s16) pG->pl_life > 0) {
            if (em39ExitCk(em) || (w->Be_flg & 0x2000) || em->l_pl < 25000000.0f) {
                if (em39AreaMoveCk(em) == 0) {
                    w->Hide_timer = 200;
                    EmRoutineSet(em, 1, 0x26, 0, 0);
                }
            } else {
                if (EmDeadCk(pPL)) {
                    w->Atk_wait = 30;
                }
                if (w->Atk_wait == 0) {
                    u8 r;

                    AtariOn(&em->atari, 0x300);
                    r = Rnd() % 3;
                    switch (r) {
                    case 0:
                    default:
                        EmRoutineSet(em, 1, 0x24, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0x1F, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0x21, 0, 0);
                        break;
                    }
                }
            }
        }
        break;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Routine 1/6 (types 0 / 1): drops back into the crouch after a ranged attack (the bow variant
// when the bow is in hand), collision off; then, after more than 400 total damage, relocates or
// hides, else crouches again (Sit) with a 90..135 frame attack wait.
static void em39_R1_SitDown(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type != 3) {
            MotionSetCore(em, MOTION(em), ARC(0xD1), ARC(0xD2), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x8E), ARC(0x8F), 3, 1, 0);
        }
        AtariOff(&em->atari, 0xFCFF);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if ((s16) w->Appear_damage > 400) {
                if (em39AreaMoveCk(em) == 0) {
                    w->Hide_timer = 200;
                    EmRoutineSet(em, 1, 0x26, 0, 0);
                }
            } else {
                w->Atk_wait = (u8) (Rnd() % 45) + 90;
                EmRoutineSet(em, 1, 5, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/7 (types 0 / 1): standing at a wall point (pGotoPoint, eased onto it and turned to
// face out of it) for 60 frames; then, with the player within 5 m or in front of him, he charges
// (Run, or Walk while Dash_wait runs) with the long attack held 300 frames; otherwise once the
// attack wait is out he draws the gun from the wall (1/0x20; always when the player is 2 m below or
// above) or a grenade (1/0x22). Be_flg 0x20000 sends him into hiding.
static void em39_R1_WallWait(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    f32 dy;

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x96), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x96), 0, 0xA, 5, 0);
        }
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        w->Timer = 60;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if ((s16) pG->pl_life > 0) {
            if (w->pGotoPoint) {
                f32 ang;

                PosToPos(&em->pos, &w->pGotoPoint->pos, &em->pos, 0.1f);
                ang = LIMIT_ANGLE(w->pGotoPoint->rotY + PI);
                em->ang.y += Muku2(em->ang.y, ang, 0.39269908f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            }
            if (w->Timer) {
                w->Timer--;
            } else {
                dy = em->pos.y - pPL->pos.y;
                dy = fabsf(dy);
                if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) < 2.3561945f || em->l_pl < 25000000.0f) {
                    w->LongAtk_wait = 300;
                    w->Atk_wait = 0;
                    if (w->Dash_wait == 0) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                } else if (w->Atk_wait == 0) {
                    u8 r = Rnd() % 10;

                    if (r > 4 || dy > 2000.0f) {
                        EmRoutineSet(em, 1, 0x20, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0x22, 0, 0);
                    }
                }
            }
        }
        break;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Walk / Run / Goto share the "stop when the player is dead or the enemy died" branch and the
// lock-on escape (em39LockCk) and the jump / door checks.
// Routine 1/8: the walk at the target (the type's / weapon's motion; the knife lowered, a thrown
// knife dropped). Route_type tracks which side of the player he is on. Stops in Wait when either
// dies; runs the melee selection (em39AtkRtnCk); an about-face past 135 deg; beyond 4.5 m a dodge
// or the run. Then the lock-on reaction as in Wait, the jump down / up, fence, door and goto checks,
// and the leave-area hide.
static void em39_R1_Walk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        if (em->type == 2) {
            w->Arm_rno = 8;
            MotionSetCore(em, MOTION(em), ARC(0xD4), ARC(0xD5), 0xA, 5, 0);
        } else if (w->Wep_type == 1) {
            MotionSetCore(em, MOTION(em), ARC(0x9F), ARC(0xA0), 0xA, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3C), ARC(0x3D), 0xA, 5, 0);
        }
        if (Muku(&pPL->pos, &em->pos, pPL->ang.y, PI) > 0.0f) {
            w->Route_type = 0;
        } else {
            w->Route_type = 1;
        }
        w->Timer = 0;
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        if (w->pArrow) {
            w->pArrow->setFall(0, 0, 20.0f);
            w->pArrow = 0;
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.09817477f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        {
            f32 ang = Muku(&pPL->pos, &em->pos, pPL->ang.y, PI);

            if (fabsf(ang) > 0.2617994f) {
                if (ang > 0.0f) {
                    w->Route_type = 0;
                } else {
                    w->Route_type = 1;
                }
            }
        }
        if ((s16) pG->pl_life <= 0 || em->hp <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
            break;
        }
        if (em39AtkRtnCk(em)) {
            break;
        }
        if (w->Go_rot > 2.3561945f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if (em->l_pl > 20250000.0f && w->Dash_wait == 0) {
            if (em39SlantCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 9, 0, 0);
        }
        break;
    }
    if (em39LockCk(em)) {
        w->Lock_timer++;
        if (w->Lock_timer > 10) {
            if (em->type == 2) {
                if (em39SlantCk2(em)) {
                    return;
                }
                w->Lock_timer = 0;
            } else {
                if (w->Escape_wait == 0 || em39HeadLockCk(em)) {
                    if (em39SlantCk(em)) {
                        return;
                    }
                    if (em->l_pl < 25000000.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0xD, 0, 0);
                    }
                    return;
                }
            }
        }
    } else {
        w->Lock_timer = 0;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    if (em39DoorOpenCk(em)) {
        return;
    }
    if (em39GotoCk(em)) {
        return;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Routine 1/9: the run at the target (one of two runs, or type 2's; yaw PI/12 per frame). Stops in
// Wait when either dies; the melee selection; an about-face past 135 deg; type 2 slows to a walk
// near the player on the easy ranks. Types 0 / 1 react to a 15-frame lock-on with a wall jump, a
// dodge, a step or the escape; then the jump / fence / door / goto checks and the leave-area hide.
static void em39_R1_Run(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        if (em->type == 2) {
            w->Arm_rno = 8;
            MotionSetCore(em, MOTION(em), ARC(0xD7), ARC(0xD8), 5, 5, 0);
        } else {
            u8 r = Rnd() % 10;

            if (r > 4) {
                MotionSetCore(em, MOTION(em), ARC(0x3E), ARC(0x3F), 5, 5, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x40), ARC(0x41), 5, 5, 0);
            }
        }
        if (Muku(&pPL->pos, &em->pos, pPL->ang.y, PI) > 0.0f) {
            w->Route_type = 0;
        } else {
            w->Route_type = 1;
        }
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        if (w->pArrow) {
            w->pArrow->setFall(0, 0, 20.0f);
            w->pArrow = 0;
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.2617994f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        {
            f32 ang = Muku(&pPL->pos, &em->pos, pPL->ang.y, PI);

            if (fabsf(ang) > 0.2617994f) {
                if (ang > 0.0f) {
                    w->Route_type = 0;
                } else {
                    w->Route_type = 1;
                }
            }
        }
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (em->hp <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (em39AtkRtnCk(em)) {
            break;
        } else if (w->Go_rot > 2.3561945f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if (em->l_pl < 9000000.0f && em->type == 2 && pG->Game_level <= 1) {
            EmRoutineSet(em, 1, 8, 0, 0);
        } else if (em->l_pl < 6250000.0f && em->type == 2 && pG->Game_level <= 3) {
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
    if (em39LockCk(em) && em->type != 2) {
        w->Lock_timer++;
        if (w->Lock_timer > 15) {
            if (w->Escape_wait == 0 || em39HeadLockCk(em)) {
                if (em39JumpUpCk3(em)) {
                    return;
                }
                if (em39SlantCk(em)) {
                    return;
                }
                if (em->l_pl < 25000000.0f) {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                return;
            }
        }
    } else {
        w->Lock_timer = 0;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    if (em39DoorOpenCk(em)) {
        return;
    }
    if (em39GotoCk(em)) {
        return;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Routine 1/0xA (em39GotoCk): runs to Goto_pos (em39RouteCk routes there instead of to the player)
// and idles once within 1 m (Goto_mode cleared); an about-face past 135 deg or a dead player ends it
// early. The jump / fence / door checks and the leave-area hide run every frame.
static void em39_R1_Goto(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        w->Arm_rno = 8;
        MotionSetCore(em, MOTION(em), ARC(0x40), ARC(0x41), 0xA, 5, 0);
        if (Muku(&pPL->pos, &em->pos, pPL->ang.y, PI) > 0.0f) {
            w->Route_type = 0;
        } else {
            w->Route_type = 1;
        }
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        if (w->pArrow) {
            w->pArrow->setFall(0, 0, 20.0f);
            w->pArrow = 0;
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        {
            f32 ang = Muku(&pPL->pos, &em->pos, pPL->ang.y, PI);

            if (fabsf(ang) > 0.2617994f) {
                if (ang > 0.0f) {
                    w->Route_type = 0;
                } else {
                    w->Route_type = 1;
                }
            }
        }
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (w->Go_rot > 2.3561945f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if ((em->pos.x - w->Goto_pos.x) * (em->pos.x - w->Goto_pos.x) + (em->pos.y - w->Goto_pos.y) * (em->pos.y - w->Goto_pos.y) + (em->pos.z - w->Goto_pos.z) * (em->pos.z - w->Goto_pos.z) < 1000000.0f) {
            {
                u8 zero = 0;
                w->Goto_mode = zero;
                EmRoutineSet(em, 1, 4, zero, zero);
            }
        }
        break;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    if (em39DoorOpenCk(em)) {
        return;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Routine 1/0xB: the about-face (TmpF the yaw to reach, steered to the target while motion event
// bit 3 is set); on event bit 2 with the attack wait out it may attack, dodge, or break into the run
// / walk; otherwise Walk when it ends. Jump / fence / goto checks every frame.
static void em39_R1_Turn180(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x48), ARC(0x49), 5, 1, 0);
        w->TmpF = em->ang.y + PI;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            f32 d = Muku(&em->pos, &w->Go_pos, w->TmpF, 0.09817477f);

            w->TmpF += d;
            w->TmpF = LIMIT_ANGLE(w->TmpF);
            em->ang.y += d;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 8, 0, 0);
        } else if (w->Atk_wait == 0 && (em->Motion.Seq_old.Free & 4)) {
            if (em39AtkRtnCk(em)) {
                break;
            }
            if (em39SlantCk(em)) {
                return;
            }
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    em39GotoCk(em);
}

// Routine 1/0xC: the taunt facing the player. With r_no_3 set it may lead (60 %) into a gun burst
// (1/0x1D beyond 5 m) or a grenade throw (1/0x23), else Walk. Types 0 / 1 break it off when aimed
// at: a wall jump (30 %), a step within 5 m or the escape.
static void em39_R1_Threat(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->Be_flg |= 0x10;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x6C), 0, 5, 1, 0);
        w->Arm_rno = 0;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        end = MotionMove(em, 0);
        if (end) {
            if (em->r_no_3 && (u8) (Rnd() % 10) > 3) {
                if ((u8) (Rnd() % 10) > 4 && em->l_pl > 25000000.0f) {
                    EmRoutineSet(em, 1, 0x1D, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 0x23, 0, 1);
                }
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em39LockCk(em) && em->type != 2 && em->r_no_3 <= 3) {
            if ((u8) (Rnd() % 10) > 6 && em39JumpUpCk3(em)) {
                break;
            }
            if (em->l_pl < 25000000.0f) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0xD, 0, 0);
            }
        }
        break;
    }
    em39GotoCk(em);
}

// Wall probe from the chest towards (x, 0, z) in model space (Escape / Step).
#define EM39_WALL_CK(em, a, b, bx, bz)                                                             \
    (a).x = 0.0f;                                                                                  \
    (a).y = 1500.0f;                                                                               \
    (a).z = 0.0f;                                                                                  \
    (b).x = bx;                                                                                    \
    (b).y = 1500.0f;                                                                               \
    (b).z = bz;                                                                                    \
    PSMTXMultVec((em)->mat, &(a), &(a));                                                           \
    PSMTXMultVec((em)->mat, &(b), &(b));

// Routine 1/0xD: the evasive roll when aimed at: probes 2 m right / left / back for walls and rolls
// in a free direction (0 right, 1 left, 2 back, 3 the long back flip only beyond 4 m), each with its
// dust effect, turning to face the player for 30 frames; invulnerable (dmType 0x1E, Be_flg 0x100
// until motion event bit 2). Sets the next escape 150..300 frames out. Still aimed at (types 0 / 1,
// up to three rolls) it repeats or wall-jumps; on event bit 0 half the time it moves on early
// (turn, wait, attack, dodge, run / walk); else Turn or Wait when the motion ends.
static void em39_R1_Escape(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    int end;

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0: {
        int wall = 0;
        u8 dir;

        EM39_WALL_CK(em, a, b, 2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall = 1;
        }
        EM39_WALL_CK(em, a, b, -2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall |= 2;
        }
        EM39_WALL_CK(em, a, b, 0.0f, -2000.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall |= 4;
        }
        dir = Rnd() & 3;
        switch ((u32) wall) {
        case 0:
            break;
        case 1:
            dir = 0;
            break;
        case 2:
            dir = 1;
            break;
        case 3:
            dir = 2;
            break;
        default:
            dir = 3;
            break;
        }
        if (dir == 3 && em->l_pl < 16000000.0f) {
            dir = Rnd() % 3;
        }
        switch ((u32) dir) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x44), ARC(0x45), 3, 1, 5);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xD, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x46), ARC(0x47), 3, 1, 5);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xE, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x42), ARC(0x43), 3, 1, 3);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x14, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x4C), ARC(0x4D), 3, 1, 8);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xF, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        }
        em->dmg.m_Timer = 0x1E;
        w->Timer = 30;
        em->r_no_3++;
        w->Escape_wait = (u8) (Rnd() % 150) + 150;
        em->r_no_2++;
    }
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.39269908f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        end = MotionMove(em, 0);
        if (end) {
            if (w->Go_rot > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 4, 0, 0);
            }
            break;
        }
        if (em->Motion.Seq_old.Free & 4) {
            if (em39LockCk(em) && em->type != 2 && em->r_no_3 <= 3) {
                if ((u8) (Rnd() % 10) > 6 && em39JumpUpCk3(em)) {
                    break;
                }
                em->r_no_2 = end;
                break;
            }
        } else {
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (w->Atk_wait) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    if ((u8) (Rnd() % 10) > 4 && em39AtkRtnCk(em)) {
                        break;
                    }
                    if (em39SlantCk(em)) {
                        break;
                    }
                    if (w->Dash_wait == 0) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                }
            }
        }
        break;
    }
}

// Routine 1/0xE: the back flip away from the player (r_no_3: 0 a plain one, 1 / 2 the chained
// second and third flips, 3 / 4 the retreat variant that leads to the reload (r_no_3 3) or, on type
// 2, a fresh attack), facing him for 10 frames, invulnerable until motion event bit 2; type 2 sets
// Dash_wait by difficulty. On event bit 2: a wall jump, the chained flips, or (types 0 / 1 aimed at)
// a step / escape; on event bit 0 half the time a gun burst at 6..10 m, a dodge or the run / walk.
// Otherwise Turn or Wait at the end.
static void em39_R1_Backjump(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int jump;

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3 == 3 || em->r_no_3 == 4) {
            MotionSetCore(em, MOTION(em), ARC(0x75), ARC(0x76), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x29, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x42), ARC(0x43), 3, 1, 3);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x14, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        w->Arm_rno = 0;
        em->dmg.m_Timer = 0x1E;
        w->Timer = 10;
        if (em->type == 2) {
            w->Dash_wait = 60;
            if (pG->Game_level <= 1) {
                w->Dash_wait = 150;
            }
            if (pG->Game_level <= 3) {
                w->Dash_wait = 120;
            }
            if (pG->Game_level > 6) {
                w->Dash_wait = 30;
            }
            if (pG->Game_level > 9) {
                w->Dash_wait = 0;
            }
        }
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (em->r_no_3 == 3) {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            } else if (w->Go_rot > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 4, 0, 0);
            }
            break;
        }
        if (em->Motion.Seq_old.Free & 4) {
            jump = em39JumpUpCk3(em);
            if (jump) {
                break;
            }
            if (em->r_no_3 == 1) {
                if (em->type == 2) {
                    if (em39AtkRtnCk(em)) {
                        break;
                    }
                    if (em->l_pl > 36000000.0f) {
                        EmRoutineSet(em, 1, 4, 0, 0);
                        break;
                    }
                }
                EmRoutineSet(em, 1, 0xE, 0, 4);
                break;
            }
            if (em->r_no_3 == 2) {
                EmRoutineSet(em, 1, 0xE, 0, 3);
                break;
            }
            if (em39LockCk(em) && em->type != 2 && em->r_no_3 == 0) {
                Rnd();
                if (em->l_pl < 25000000.0f) {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                break;
            }
        } else {
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if (em->r_no_3 == 4 && em->type == 2) {
                if (em39AtkRtnCk(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 4, 0, 0);
                break;
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            if ((u8) (Rnd() % 10) > 4 && em->r_no_3 == 0) {
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (w->Atk_wait) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    if ((u8) (Rnd() % 10) > 4 && (w->Be_flg & 1)) {
                        f32 dy = fabsf(em->pos.y - pPL->pos.y);

                        if (em->l_pl < 100000000.0f && em->l_pl > 36000000.0f && w->Pl_rot < 0.5235988f && dy < 2000.0f) {
                            EmRoutineSet(em, 1, 0x1D, 0, 0);
                            break;
                        }
                    }
                    if (em39SlantCk(em)) {
                        break;
                    }
                    if (w->Dash_wait == 0) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                }
            }
        }
        break;
    }
}

// Routine 1/0xF: the quick side step (right / left, away from a wall within 2 m; the forward dash
// step at 2.5..3.5 m or when r_no_3 is set), with dust, facing the player for 20 frames, dmType 0x14
// and invulnerable until motion event bit 2. On event bit 0 the melee selection, then half the time
// a wall jump / turn / dodge / run; otherwise Turn, a dodge or the run / walk at the end.
static void em39_R1_Step(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0: {
        int dir = Rnd() & 1;

        EM39_WALL_CK(em, a, b, 2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            dir = 0;
        }
        EM39_WALL_CK(em, a, b, -2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            dir = 1;
        }
        if (em->l_pl > 6250000.0f && em->l_pl < 12250000.0f) {
            dir = 2;
        }
        if (em->r_no_3) {
            dir = 2;
        }
        switch ((u32) dir) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x6F), ARC(0x70), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xB, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x6D), ARC(0x6E), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xA, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x71), ARC(0x72), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xC, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        }
        em->dmg.m_Timer = 0x14;
        w->Timer = 20;
        em->r_no_3++;
        em->r_no_2++;
    }
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.39269908f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Go_rot > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
                break;
            }
            if (em39SlantCk(em)) {
                break;
            }
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if (!(em->Motion.Seq_old.Free & 4)) {
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if (em39AtkRtnCk(em)) {
                break;
            }
            if ((u8) (Rnd() % 10) > 4) {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                    break;
                }
                if (em39SlantCk(em)) {
                    break;
                }
                if (w->Dash_wait == 0) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
            }
        }
        break;
    }
}

// Routine 1/0x10 (em39SlantCk): the diagonal dodge run to the side (r_no_3 = right), dmType 5,
// facing the player for 10 frames, the long attack held 30 frames. On motion event bit 2, facing him
// within 45 deg: another dodge, a dash step within 3.5 m, or the run / walk; on event bit 0 half the
// time a turn or the melee selection; else Turn or the run / walk at the end.
static void em39_R1_Slant(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x64), ARC(0x65), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xB, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x66), ARC(0x67), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0xA, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        em->dmg.m_Timer = 5;
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        w->LongAtk_wait = 30;
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.39269908f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Go_rot > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if ((em->Motion.Seq_old.Free & 4) && w->Go_rot < 0.7853982f) {
            if (em39SlantCk(em)) {
                break;
            }
            if (em->l_pl < 12250000.0f) {
                EmRoutineSet(em, 1, 0xF, 0, 1);
            } else if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else {
                    em39AtkRtnCk(em);
                }
            }
        }
        break;
    }
}

// Routine 1/0x11 (type 2, em39SlantCk2): the dodging dash of the final form (one of two motions,
// with the effect variant for Ada), dmType 5, facing the player 10 frames. On motion event bit 2,
// beyond 3.5 m on Game_level above 3, half the time another dodge or the run; else Walk (Turn past
// 135 deg at the end).
static void em39_R1_Slant2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        if ((u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0xFB), ARC(0xFC), 3, 1, 0);
            if (pG->pl_type == 2) {
                EstSet(em, -1, 0, 0, EFF_EM39, 0x49, 0, ESP_CORE_KIND_NONE, em, 0);
            } else {
                EstSet(em, -1, 0, 0, EFF_EM39, 0x40, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xFD), ARC(0xFE), 3, 1, 0);
            if (pG->pl_type == 2) {
                EstSet(em, -1, 0, 0, EFF_EM39, 0x4A, 0, ESP_CORE_KIND_NONE, em, 0);
            } else {
                EstSet(em, -1, 0, 0, EFF_EM39, 0x41, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        }
        em->dmg.m_Timer = 5;
        w->Timer = 10;
        w->Arm_rno = 8;
        em->r_no_2++;
    case 1:
        w->LongAtk_wait = 30;
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.39269908f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Go_rot > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if (em->Motion.Seq_old.Free & 4) {
            if (pG->Game_level > 3) {
                if (em->l_pl > 12250000.0f && (u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x11, 0, 0);
                    break;
                }
                if (em->l_pl > 12250000.0f && (u8) (Rnd() % 10) > 4 && w->Dash_wait == 0) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                    break;
                }
            }
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
}

// Routine 1/0x12 (type 2): the closing dash that covers the distance to the player less 7.1 m in
// four frames of motion event bit 0 (TmpF per frame), turning to him while event bit 3 is set;
// sets SuperDashWait by difficulty (360..900). On event bit 2 the melee selection, else Wait step 2.
static void em39_R1_SuperDash(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec spd;
    f32 d;

    w->Be_flg |= 0x30;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD9), ARC(0xDA), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0xC, 0, ESP_CORE_KIND_NONE, em, 0);
        w->TmpU32 = 0;
        d = GetDistance3(&em->pos, &pPL->pos) - 7100.0f;
        w->TmpF = d * 0.25f;
        w->SuperDashWait = 600;
        if (pG->Game_level <= 3) {
            w->SuperDashWait = 900;
        }
        if (pG->Game_level > 6) {
            w->SuperDashWait = 450;
        }
        if (pG->Game_level > 9) {
            w->SuperDashWait = 360;
        }
        w->Arm_rno = 8;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.39269908f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 1) {
            if (w->TmpU32 == 0) {
                w->TmpU32 = 1;
                d = GetDistance3(&em->pos, &pPL->pos) - 7100.0f;
        w->TmpF = d * 0.25f;
            }
            spd.x = 0.0f;
            spd.y = 0.0f;
            spd.z = w->TmpF;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            PSVECAdd(&em->pos, &spd, &em->pos);
        }
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 4, 2, 0);
        } else if (em->Motion.Seq_old.Free & 4) {
            if (em39AtkRtnCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 4, 2, 0);
        }
        break;
    }
}

// Routine 1/0x13 (em39JumpDownCk): drops off a ledge (r_no_3 picks the plain or the long drop),
// facing Target_dir, invulnerable (dmType 2), collision off and shadow hidden; on motion event bit 4
// past the floor height it snaps to the floor, puts a gun / bow away, and plays the landing. Then
// below y -4 m it hides for 200 frames; else a goto point or the run / walk. Be_flg 0x20000 hides.
static void em39_R1_JumpDown(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;
    Vec v;
    f32 fl;

    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3 == 0) {
            MotionSetCore(em, MOTION(em), ARC(0x50), ARC(0x51), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x11, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x52), ARC(0x53), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x12, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        em->be_flag |= 2;
        AtariOff(&em->atari, 0xFCFF);
        w->Be_flg &= ~0x8000;
        if (w->pArrow) {
            w->pArrow->setFall(0, 0, 20.0f);
            w->pArrow = 0;
        }
        em->r_no_2++;
    case 1:
        w->Be_flg |= 0x01000000;
        em->ang.y += Muku2(em->ang.y, w->Target_dir, 0.39269908f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em->dmg.m_Timer = 2;
        end = MotionMove(em, 0);
        if (em->Motion.Seq_old.Free & 0x10) {
            v = em->pos;
            v.y = em->pos_old.y;
            fl = SatMgr.getFloor(&v, 0, 600.0f, 100000.0f, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                AtariOn(&em->atari, 0x300);
                if (w->Wep_type == 3 || w->Wep_type == 4) {
                    em39WepSet(em, 0);
                }
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
                MotionSetCore(em, MOTION(em), ARC(0x54), ARC(0x55), 3, 1, 0);
                EstSet(em, -1, 0, 0, EFF_EM39, 0x13, 0, ESP_CORE_KIND_NONE, em, 0);
                MotionMove(em, 0);
                em->r_no_2 = 2;
                break;
            }
        }
        if (end) {
            em->r_no_2++;
        }
        break;
    case 2:
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (em->pos.y < -4000.0f) {
                w->Hide_timer = 200;
                EmRoutineSet(em, 1, 0x26, 0, 0);
                return;
            }
            if (em39GotoCk(em)) {
                return;
            }
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Routine 1/0x14 (em39JumpUpCk): the climb onto a ledge at Target_pos facing Target_dir (Be_flg 0x100:
// invulnerable): the motion's own rise brings him 3.2 m up and 2.5 m forward, TmpV covers the rest a
// tenth per frame while motion event bit 0 is set; collision off in the air. Then a goto point or
// the run / walk.
static void em39_R1_JumpUp(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec v;

    w->Be_flg |= 0x100;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        PSMTXRotRad(m, 'y', w->Target_dir);
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 3200.0f;
        v.z = 2530.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&w->Target_pos, &v, &w->TmpV);
        MotionSetCore(em, MOTION(em), ARC(0x4E), ARC(0x4F), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x10, 0, ESP_CORE_KIND_NONE, em, 0);
        AtariOff(&em->atari, 0xFCFF);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->Target_dir, 0.39269908f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (em->Motion.Seq_old.Free & 1) {
            PSVECScale(&w->TmpV, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->TmpV, &v, &w->TmpV);
            w->Be_flg |= 0x01000000;
        }
        if (MotionMove(em, 0)) {
            AtariOn(&em->atari, 0x300);
            if (em39GotoCk(em)) {
                break;
            }
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/0x15 (em39JumpUpCk2): the high leap up to Target_pos, invulnerable: a ballistic arc
// (Spd.y starts at 19 steps of the rise-plus-1 m over 190 frames, TmpF the per-frame fall; the
// landing phase, motion event bit 4, falls faster) while event bit 6 is set, with TmpV covering the
// horizontal gap a tenth per frame, facing Target_dir. Then a goto point or the run / walk.
static void em39_R1_JumpUp2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec v;

    w->Be_flg |= 0x100;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0: {
        // COMPILER-DIFF: #2 -- the target's `fmuls f12,f0,f12` ties the product to the constant's
        // register (its sum was not a tieable operand); value-carrying pins reproduce the tie and
        // the FPR names around it.
        register f32 posy asm("fr11");
        f32 dy;
        register f32 k asm("fr12");
        register f32 t asm("fr12");
        f32 k19;
        register f32 py asm("fr13");

        MotionSetCore(em, MOTION(em), ARC(0x6A), ARC(0x6B), 0xA, 1, 0);
        posy = em->pos.y;
        dy = w->Target_pos.y - posy + 1000.0f;
        k = 0.0052631581f;
        t = dy * k;
        w->Spd.x = 0.0f;
        w->Spd.z = 0.0f;
        k19 = 19.0f;
        py = t * k19;
        w->Spd.y = py;
        w->TmpF = t;
        PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &w->Target_pos));
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&w->Target_pos, &v, &w->TmpV);
        w->TmpV.y = 0.0f;
        em->r_no_2++;
    }
    case 1:
        em->ang.y += Muku2(em->ang.y, w->Target_dir, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (em->Motion.Seq_old.Free & 0x40) {
            if (em->Motion.Seq_old.Free & 0x10) {
                PSVECAdd(&em->pos, &w->Spd, &em->pos);
                w->Spd.y -= 35.714287f;
            } else {
                PSVECAdd(&em->pos, &w->Spd, &em->pos);
                w->Spd.y -= w->TmpF;
            }
            PSVECScale(&w->TmpV, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->TmpV, &v, &w->TmpV);
            w->Be_flg |= 0x01000000;
        }
        if (MotionMove(em, 0)) {
            AtariOn(&em->atari, 0x300);
            if (em39GotoCk(em)) {
                break;
            }
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/0x16 (em39JumpUpCk3): the wall jump up to a perch at Target_pos when aimed at: the
// forward leap (r_no_3 0) or the backwards one (r_no_3 1, facing away from the perch), the same arc
// as em39_R1_JumpUp2. On landing Be_flg 0x8000 (perched; 0x00400000 / 0x00040000 count the wall
// jumps), then a goto point, or a gun burst (50 % beyond 5 m) / grenade throw from above.
static void em39_R1_JumpUp3(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec v;
    f32 zf;

    w->Be_flg |= 0x100;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0: {
        if (fabsf(Muku(&em->pos, &w->Target_pos, em->ang.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x6A), ARC(0x6B), 0xA, 1, 0);
            // COMPILER-DIFF: #2 -- both arms: the product is tied to the constant's register
            // (`fmuls f12,f13,f12` / `fmuls f12,f0,f12`) and the 0.0 reuses the 1000.0 register;
            // value-carrying pins per arm (the JumpUp2 recipe). Store order x, y, z, x18: the zero's
            // first use sinks last, the dying stores keep source order.
            register f32 posy asm("fr0");
            f32 dy;
            register f32 k asm("fr12");
            register f32 t asm("fr12");
            f32 k19;
            register f32 py asm("fr0");

            posy = em->pos.y;
            dy = w->Target_pos.y - posy + 1000.0f;
            k = 0.0052631581f;
            t = dy * k;
            zf = 0.0f;
            k19 = 19.0f;
            py = t * k19;
            w->Spd.x = zf;
            w->Spd.y = py;
            w->Spd.z = zf;
            w->TmpF = t;
            em->r_no_3 = 0;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x68), ARC(0x69), 0xA, 1, 0);
            register f32 posy asm("fr13"); // COMPILER-DIFF: #2 (see the other arm)
            f32 dy;
            register f32 k asm("fr12");
            register f32 t asm("fr12");
            f32 k19;
            register f32 py asm("fr13");

            posy = em->pos.y;
            dy = w->Target_pos.y - posy + 1000.0f;
            k = 0.0058479533f;
            t = dy * k;
            zf = 0.0f;
            k19 = 18.0f;
            py = t * k19;
            {
                // COMPILER-DIFF: 13 -- the `li r0,1` precedes the four stores: with the values kept alive
                // past the stores (no store dies) the block is in LUID order, and the `1` is set first.
                int one = 1;
                w->Spd.y = py;
                w->Spd.z = zf;
                w->TmpF = t;
                w->Spd.x = zf;
                asm("" : "=m"(w->Timer) : "f"(py), "f"(zf), "f"(t)); // COMPILER-DIFF: 13
                em->r_no_3 = one;
            }
        }
        PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &w->Target_pos));
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&w->Target_pos, &v, &w->TmpV);
        w->TmpV.y = 0.0f;
        em->r_no_2++;
    }
    case 1:
        if (em->r_no_3) {
            f32 a;

            a = GetXZAngle(&w->Target_pos, &em->pos);
            {
                // COMPILER-DIFF: #1 -- the limit constant reaches the call as a dying pseudo (the original's
                // REG_EQUIV constant): its `lis` then outranks the GetXZAngle result copy in sched1 (path
                // lis; lfs; fmr f3 = 12 vs 11) and precedes the `fmr f2,f1`. The codeless asm is the second
                // reference that keeps combine from folding the pseudo into the argument load.
                f32 k = 0.39269908f;
                asm("" : "=m"(w->Timer) : "f"(k));
                em->ang.y += Muku2(em->ang.y, a, k);
            }
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        } else {
            em->ang.y += Muku(&em->pos, &w->Target_pos, em->ang.y, 0.39269908f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 0x40) {
            if (em->Motion.Seq_old.Free & 0x10) {
                PSVECAdd(&em->pos, &w->Spd, &em->pos);
                w->Spd.y -= 35.714287f;
            } else {
                PSVECAdd(&em->pos, &w->Spd, &em->pos);
                w->Spd.y -= w->TmpF;
            }
            PSVECScale(&w->TmpV, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->TmpV, &v, &w->TmpV);
            w->Be_flg |= 0x01000000;
        }
        if (MotionMove(em, 0)) {
            AtariOn(&em->atari, 0x300);
            w->Be_flg |= 0x8000;
            if (w->Be_flg & 0x00400000) {
                w->Be_flg |= 0x00040000;
            }
            w->Be_flg |= 0x00400000;
            if (em39GotoCk(em)) {
                break;
            }
            if ((u8) (Rnd() % 10) > 4 && em->l_pl > 25000000.0f) {
                EmRoutineSet(em, 1, 0x1D, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x23, 0, 1);
            }
        }
        break;
    }
}

// Routine 1/0x17 (em39FanceJumpCk): vaults a fence facing Target_dir, invulnerable; then a goto
// point or Walk.
static void em39_R1_FanceJump(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x100;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x4A), ARC(0x4B), 3, 1, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->Target_dir, 0.39269908f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            if (em39GotoCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
}

// Routine 1/0x18: the knife slash (Be_flg 0x80: attacking). With the knife already out (Wep_type
// 1) steps 0/1 the plain slash, else 2/3 the draw-and-slash (the knife appears on motion event bit 4)
// homing on the player; the blade (attack 2 at the hand part 0xA) hits on event bit 0, the grunt on
// bit 1. A miss awards the escape point; then a wall jump, the back flip within 3 m (30-frame
// attack wait) or Wait.
static void em39_R1_AtkKnife(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    if (em->r_no_2 == 0 && w->Wep_type != 1) {
        em->r_no_2 = 2;
    }
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA1), ARC(0xA2), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x17, 0, ESP_CORE_KIND_NONE, em, 0);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        w->Timer = 0;
        w->Atk_ck = 0;
        w->Timer2 = 22;
        w->Act_ck = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.2617994f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 1) {
            em39AtkCk(em, 2, 0xA);
        }
        if (em->Motion.Seq_old.Free & 2) {
            em39SetVoice(em, 0x19);
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                w->Atk_wait = 30;
                if (em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA3), ARC(0xA4), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x18, 0, ESP_CORE_KIND_NONE, em, 0);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        em39SetVoice(em, 0x19);
        w->Atk_ck = 0;
        w->Act_ck = 0;
        w->Timer = 10;
        w->Timer2 = 22;
        em->r_no_2++;
    case 3:
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (em->Motion.Seq_old.Free & 0x10) {
            em39WepSet(em, 1);
            if (w->pKnife) {
                MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
            }
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                w->Atk_wait = 30;
                if (em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            em39AtkCk(em, 2, 0xA);
        }
        break;
    }
    em39HandSet(em, 1);
}

// The knife swing at a door (cEmDoor): break or open it at the hit frame.
static inline void em39DoorHit(cEm39* em, Em39Work* w)
{
    if ((em->Motion.Seq_old.Free & 1) && w->pDoor) {
        if (w->pDoor->isAlive()) {
            if (w->pDoor->type == 0) {
                w->pDoor->setBreak(&em->pos);
            } else {
                w->pDoor->setOpen(&em->pos, 0, 0, 0);
            }
        }
        w->pDoor = 0;
    }
}

// Routine 1/0x19 (em39DoorOpenCk): the knife slash at a door in the way (pDoor), the same two
// variants as em39_R1_AtkKnife; the door breaks or opens on the hit frame (em39DoorHit). Then the
// run / walk.
static void em39_R1_AtkDoor(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    if (em->r_no_2 == 0 && w->Wep_type != 1) {
        em->r_no_2 = 2;
    }
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA1), ARC(0xA2), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x17, 0, ESP_CORE_KIND_NONE, em, 0);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        w->Timer = 0;
        w->Atk_ck = 0;
        w->Timer2 = 22;
        w->Act_ck = 0;
        em->r_no_2++;
    case 1:
        em39DoorHit(em, w);
        if (em->Motion.Seq_old.Free & 2) {
            em39SetVoice(em, 0x19);
        }
        if (MotionMove(em, 0)) {
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA3), ARC(0xA4), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x18, 0, ESP_CORE_KIND_NONE, em, 0);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        em39SetVoice(em, 0x19);
        w->Atk_ck = 0;
        w->Act_ck = 0;
        w->Timer = 10;
        w->Timer2 = 22;
        em->r_no_2++;
    case 3:
        if (em->Motion.Seq_old.Free & 0x10) {
            em39WepSet(em, 1);
            if (w->pKnife) {
                MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
            }
        }
        em39DoorHit(em, w);
        if (em->Motion.Seq_old.Free & 2) {
            em39SetVoice(em, 0x19);
        }
        if (MotionMove(em, 0)) {
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    em39HandSet(em, 1);
}

// Branch check of the knife grab: on motion event bit 1 with the player in reach (em39CatchCk) it
// rumbles and switches to KnifeHit (1/0x1B).
static void em39_R1_br_KnifeCatch(cEm39* em)
{
    if (em->hp > 0 && (em->Motion.Seq_old.Free & 2) && em39CatchCk(em)) {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        EmRoutineSetW(em, 1, 0x1B, 0, 0);
    }
}

// Routine 1/0x1A: the grab with the knife: the reach forward (the side variants are dead code:
// `pang` is always 0), TmpF homing on the player for 10 frames, the knife drawn to its grab pose.
// A miss awards the escape point, then a wall jump, the back flip within 3 m or Wait.
static void em39_R1_KnifeCatch(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    f32 pang = 0.0f;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (em->r_no_2) {
    case 0: {
        f32 ang = Muku(&em->pos, &pPL->pos, em->ang.y, PI);

        pang = fabsf(pang);
        if (pang < 0.7853982f) {
            MotionSetCore(em, MOTION(em), ARC(0xA9), ARC(0xAA), 3, 1, 0);
            w->TmpF = em->ang.y;
        } else if (ang < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0xA5), ARC(0xA6), 3, 1, 0);
            w->TmpF = em->ang.y + -1.5707964f;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xA7), ARC(0xA8), 3, 1, 0);
            w->TmpF = em->ang.y + 1.5707964f;
        }
        w->TmpF = LIMIT_ANGLE(w->TmpF);
        asm("" : "=m"(w->Timer) : "f"(pang)); // COMPILER-DIFF: candidate #12 (cse2 canonical register)
        w->Timer = 10;
        w->Atk_ck = 0;
        w->Act_ck = 0;
        em39WepSet(em, 1);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x29), 0, 0, 5, 0);
        }
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        em39SetVoice(em, 0x19);
        em->r_no_2++;
    }
    case 1:
        if (w->Timer) {
            f32 d;

            w->Timer--;
            d = Muku(&em->pos, &pPL->pos, w->TmpF, 0.09817477f);
            w->TmpF += d;
            w->TmpF = LIMIT_ANGLE(w->TmpF);
            em->ang.y += d;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                w->Atk_wait = 30;
                if (em39JumpUpCk3(em)) {
                    break;
                }
                if (em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        }
        break;
    }
    em39HandSet(em, 1);
}

// Routine 1/0x1B: the player is held at knife point (EmCatchPLSet with plem39_KnifeHit). Step 0/1
// the hold: the button mash runs until it reaches the difficulty threshold (5..15), then the action
// prompt (em39ActOn) appears on the swing frame; step 2/3 the stab for 1150 damage (the kill
// variant at 0 HP), then Wait or the back flip; step 4/5 the player broke free: the recoil, then a
// wall jump or Wait.
static void em39_R1_KnifeHit(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xAC), ARC(0xAD), 3, 1, 0);
        if (w->pKnife) {
            EstSet(w->pKnife, -1, 0, 0, EFF_EM39, 0x19, 0, ESP_CORE_KIND_NONE, w->pKnife, 0);
        }
        EmCatchPLSet(em, PI, 2, -25.34f, 0.0f, -239.75f, plem39_KnifeHit);
        PlGachaInit();
        w->Act_ck = 0;
        w->TmpU32 = Rnd() & 1;
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x23);
        } else {
            em39SetVoice(em, 0x24);
        }
        w->Timer = 10;
        if (pG->Game_level <= 1) {
            w->Timer = 5;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 7;
        }
        if (pG->Game_level > 6) {
            w->Timer = 13;
        }
        if (pG->Game_level > 9) {
            w->Timer = 15;
        }
        em->r_no_2++;
    case 1:
        if ((u32) PlGachaGet() < (u32) w->Timer || !(em->Motion.Seq_old.Free & 1)) {
            PlGachaMove();
        } else if (w->Act_ck == 0) {
            if (w->TmpU32) {
                ActBtn.set(ACT_GUARD, 0xB, (void*) em39ActOn, em, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
            } else {
                ActBtn.set(ACT_GUARD, 0xB, (void*) em39ActOn, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
            }
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f)) {
            em->r_no_2++;
        } else if (w->Act_ck) {
            em->r_no_2 = 4;
        }
        break;
    case 2:
        LifeDownSet(pPL, 0x47E, 0);
        if ((s16) pG->pl_life <= 0) {
            pG->pl_life = 0;
            MotionSetCore(em, MOTION(em), ARC(0xAE), ARC(0xAF), 3, 1, 0);
            if (w->pKnife) {
                EstSet(w->pKnife, -1, 0, 0, EFF_EM39, 0x1A, 0, ESP_CORE_KIND_NONE, w->pKnife, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xC2), ARC(0xC3), 3, 1, 0);
        }
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->pKnife) {
                MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        } else if ((s16) pG->pl_life <= 0) {
            if (em->Motion.Seq_frame > 22.7f && em->Motion.Seq_frame < 23.3f) {
                PlSetDamageSe(0xD);
            }
        } else {
            if (em->Motion.Seq_frame > 10.7f && em->Motion.Seq_frame < 11.3f) {
                PlSetDamageSe(0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xB0), ARC(0xB1), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x1C, 0, ESP_CORE_KIND_NONE, em, 0);
        EmCatchPLSet(em, PI, 2, -82.6f, 0.0f, -230.94f, plem39_KnifeHit);
        pPL->r_no_2 = 4;
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        w->Timer = 10;
        em->r_no_2++;
    case 5:
        if (w->Timer) {
            w->Timer--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMove(em, 0);
        }
        if (end) {
            if (em39JumpUpCk3(em)) {
                break;
            }
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player damage callback of the knife hold (r_no_2 mirrors the boss's): 0/1 held following his
// motion (weapon hidden), 2/3 stabbed (the death variant at 0 HP) with rumble, 4/5 the break-free
// counter; ends with the motion or as soon as the boss leaves 1/0x1B.
static void plem39_KnifeHit(cPlayer* pl)
{
    int end;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x113), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(0xA, 480.00003f, 400.0f);
        pl->Wep->setTrans(0, 0);
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0x1B) {
            goto END;
        }
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    case 2:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x114), 0, 5, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM39, 0x1B, 0, ESP_CORE_KIND_NONE, pl, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11A), 0, 5, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM39, 0x2A, 0, ESP_CORE_KIND_NONE, pl, 0);
        }
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        pl->r_no_2++;
    case 3:
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
        END:
            pl->Wep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x115), 0, 5, 1, 0);
        pl->m_Work0 = 10;
        pl->r_no_2++;
    case 5:
        if (pl->m_Work0) {
            pl->m_Work0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMove(pl, 0);
        }
        if (end) {
            pl->Wep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Knife4Atk: the four-step knife combo with the player caught (cases 0/2/4/6 start a swing, the odd
// cases wait for the action button).
#define EM39_K4_WAIT(em, w)                                                                         \
    if (EmCatchMotionMove(em, 0.3f, 0.2f)) {                                                       \
        (em)->r_no_2 = 8;                                                                             \
        break;                                                                                     \
    }                                                                                              \
    if ((w)->Timer == 0) {                                                                            \
        switch ((u32) (w)->TmpU32) {                                                                  \
        case 0:                                                                                    \
        default:                                                                                   \
            ActBtn.set(ACT_GUARD, 0xB, (void*) em39ActOn, em, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);                        \
            break;                                                                                 \
        case 1:                                                                                    \
            ActBtn.set(ACT_GUARD, 0xB, (void*) em39ActOn, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);                        \
            break;                                                                                 \
        }                                                                                          \
    }                                                                                              \
    if ((w)->Timer) {                                                                                 \
        (w)->Timer--;                                                                                 \
        break;                                                                                     \
    }                                                                                              \
    if ((w)->Act_ck == 0) {                                                                          \
        break;                                                                                     \
    }

#define EM39_K4_EFF_DELETE(em, w)                                                                   \
    EffectEspDelete(0, (w)->EffKindId, em, 0);                                               \
    EffectEspgenDelete(0, (w)->EffKindId, em);                                               \
    EffectEfmDelete(0, (w)->EffKindId, em);

// Routine 1/0x1C: the four-swing knife combo with the player locked in (EmCatchPLSet with
// plem39_Knife4Atk). Even steps 0 / 2 / 4 / 6 start a swing (each with its effect, voice and a
// difficulty-scaled window in Timer), the odd steps wait: the player's action prompt (em39ActOn)
// shows once the window passes and a press parries into the next swing (after swings 2 and 3 half
// the time he breaks off instead, step 0xA / 0xB the recoil); a swing that ends unparried lands
// (step 8 / 9: 1150 damage, the kill variant at 0 HP), then the back flip (player alive) or a wall
// jump / Wait. Step 0xC / 0xD after the recoil: the recover motion, then the melee selection or Walk.
static void em39_R1_Knife4Atk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xBA), ARC(0xBB), 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -288.71f, 0.0f, 1982.72f, plem39_Knife4Atk);
        w->Act_ck = 0;
        w->TmpU32 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        w->Timer = 12;
        if (pG->Game_level <= 1) {
            w->Timer = 5;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 8;
        }
        if (pG->Game_level > 6) {
            w->Timer = 13;
        }
        if (pG->Game_level > 9) {
            w->Timer = 15;
        }
        EstSet(0, -1, 0, 0, EFF_EM39, 0x1D, 0, w->EffKindId, em, 0);
        em->r_no_2++;
    case 1:
        EM39_K4_WAIT(em, w);
        em->r_no_2++;
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xBC), ARC(0xBD), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -42.88f, 0.0f, 1328.94f, plem39_Knife4Atk);
        pPL->r_no_2 = 2;
        w->Act_ck = 0;
        w->TmpU32 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        w->Timer = 8;
        if (pG->Game_level <= 1) {
            w->Timer = 3;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 6;
        }
        if (pG->Game_level > 6) {
            w->Timer = 9;
        }
        if (pG->Game_level > 9) {
            w->Timer = 11;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, EFF_EM39, 0x1E, 0, w->EffKindId, em, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x1F, 0, ESP_CORE_KIND_NONE, em, 0);
        em39SetVoice(em, 0x19);
        em->r_no_2++;
    case 3:
        EM39_K4_WAIT(em, w);
        if ((u8) (Rnd() % 10) > 4) {
            em->r_no_2 = 0xA;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xBE), ARC(0xBF), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, 132.05f, 0.0f, 1047.06f, plem39_Knife4Atk);
        pPL->r_no_2 = 4;
        w->Act_ck = 0;
        w->TmpU32 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x28), 0, 0, 1, 0);
        }
        w->Timer = 10;
        if (pG->Game_level <= 1) {
            w->Timer = 5;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 8;
        }
        if (pG->Game_level > 6) {
            w->Timer = 11;
        }
        if (pG->Game_level > 9) {
            w->Timer = 13;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, EFF_EM39, 0x20, 0, w->EffKindId, em, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x21, 0, ESP_CORE_KIND_NONE, em, 0);
        em39SetVoice(em, 0x19);
        em->r_no_2++;
    case 5:
        EM39_K4_WAIT(em, w);
        if ((u8) (Rnd() % 10) > 4) {
            em->r_no_2 = 0xA;
        } else if (pG->Game_level <= 1) {
            em->r_no_2 = 0xA;
        } else {
            em->r_no_2++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xC0), ARC(0xC1), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -3.97f, 0.0f, 1927.6f, plem39_Knife4Atk);
        pPL->r_no_2 = 6;
        w->Act_ck = 0;
        w->TmpU32 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x2A), 0, 0, 1, 0);
        }
        w->Timer = 11;
        if (pG->Game_level <= 1) {
            w->Timer = 6;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 9;
        }
        if (pG->Game_level > 6) {
            w->Timer = 12;
        }
        if (pG->Game_level > 9) {
            w->Timer = 14;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, EFF_EM39, 0x22, 0, w->EffKindId, em, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x23, 0, ESP_CORE_KIND_NONE, em, 0);
        em39SetVoice(em, 0x19);
        em->r_no_2++;
    case 7:
        EM39_K4_WAIT(em, w);
        em->r_no_2 = 0xA;
        break;
    case 8:
        LifeDownSet(pPL, 0x47E, 0);
        if ((s16) pG->pl_life <= 0) {
            EmCatchPLSet(em, 0.0f, 2, 220.72f, 0.0f, 1134.69f, plem39_Knife4Atk);
            pPL->r_no_2 = 8;
            MotionSetCore(em, MOTION(em), ARC(0xB4), ARC(0xB5), 0, 1, 0);
        } else {
            EmCatchPLSet(em, 0.0f, 2, -451.47f, 0.0f, 1038.97f, plem39_Knife4Atk);
            pPL->r_no_2 = 8;
            MotionSetCore(em, MOTION(em), ARC(0xB8), ARC(0xB9), 0, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x27, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        EM39_K4_EFF_DELETE(em, w);
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        w->Timer = 10;
        em->r_no_2++;
    case 9:
        if (w->Timer) {
            w->Timer--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMove(em, 0);
        }
        if (end) {
            if ((s16) pG->pl_life > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 1);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 4, 0, 0);
            }
        } else if ((s16) pG->pl_life <= 0) {
            if (em->Motion.Seq_frame > 11.7f && em->Motion.Seq_frame < 12.3f) {
                PlSetDamageSe(0xD);
            }
        } else {
            if (em->Motion.Seq_frame > 2.7f && em->Motion.Seq_frame < 3.3f) {
                PlSetDamageSe(0);
            }
        }
        break;
    case 0xA:
        MotionSetCore(em, MOTION(em), ARC(0xB6), ARC(0xB7), 3, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -3.97f, 0.0f, 1927.6f, plem39_Knife4Atk);
        pPL->r_no_2 = 0xA;
        if (w->pKnife) {
            MotionSetCore(w->pKnife, MOTION(w->pKnife), ARC(0x27), 0, 0, 5, 0);
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x26, 0, ESP_CORE_KIND_NONE, em, 0);
        w->Timer = 10;
        em->r_no_2++;
    case 0xB:
        if (w->Timer) {
            w->Timer--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMove(em, 0);
        }
        if (end) {
            em->r_no_2++;
        }
        break;
    case 0xC:
        MotionSetCore(em, MOTION(em), ARC(0x77), ARC(0x78), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x2B, 0, ESP_CORE_KIND_NONE, em, 0);
        w->Timer = 10;
        em->r_no_2++;
    case 0xD:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (em39JumpUpCk3(em)) {
                break;
            }
            if (em39AtkRtnCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player side of Knife4Atk: follow the catch motion until the enemy leaves routine 0x1C.
#define PLEM39_K4_WAIT(pl)                                                                          \
    EmCatchMotionMove(pl, 0.3f, 0.2f);                                                             \
    if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0x1C) {                                                   \
        EndPlDamage();                                                                             \
        (pl)->dmg.set(0, 0x1E);                                                                    \
    }                                                                                              \
    break;

// Player damage callback of the knife combo (r_no_2 mirrors the boss's): the four held poses
// (0..7) following his motion, 8/9 the stab taken (death variant at 0 HP) with rumble, 0xA/0xB the
// parry that pushes him back; ends when the boss leaves 1/0x1C.
static void plem39_Knife4Atk(cPlayer* pl)
{
    int end;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11B), 0, 5, 1, 0);
        pl->r_no_2++;
    case 1:
        PLEM39_K4_WAIT(pl);
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11C), 0, 0, 1, 0);
        pl->r_no_2++;
    case 3:
        PLEM39_K4_WAIT(pl);
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11D), 0, 0, 1, 0);
        pl->r_no_2++;
    case 5:
        PLEM39_K4_WAIT(pl);
    case 6:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11E), 0, 0, 1, 0);
        pl->r_no_2++;
    case 7:
        PLEM39_K4_WAIT(pl);
    case 8:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x117), 0, 0, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM39, 0x24, 0, ESP_CORE_KIND_NONE, pl, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x119), 0, 0, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM39, 0x28, 0, ESP_CORE_KIND_NONE, pl, 0);
        }
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        pl->m_Work0 = 10;
        pl->r_no_2++;
    case 9:
        if (pl->m_Work0) {
            pl->m_Work0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMove(pl, 0);
        }
        if (end && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    case 0xA:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x118), 0, 0, 1, 0);
        EstSet(pl, -1, 0, 0, EFF_EM39, 0x25, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->m_Work0 = 10;
        pl->r_no_2++;
    case 0xB:
        if (pl->m_Work0) {
            pl->m_Work0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMove(pl, 0);
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Machine gun pitch towards `target` from the muzzle `a` in 1/1024 turns, clamped to a byte.
#define EM39_GUN_PITCH(ang, target, a, b)                                                           \
    PSVECSubtract(&(target), &(a), &(b));                                                          \
    {                                                                                              \
        f32 d = SQRTF((b).x * (b).x + (b).z * (b).z);                                              \
        ang = -atan2f((b).y, d) * 325.94931f;                                                      \
    }                                                                                              \
    if (ang > 255.0f) {                                                                            \
        ang = 255.0f;                                                                              \
    }                                                                                              \
    if (ang < -255.0f) {                                                                           \
        ang = -255.0f;                                                                             \
    }

// Routine 1/0x1D: the machine gun burst (Be_flg 0xC0: attacking / gun up; long attack held 300).
// Aims at a point 1.5 m up the player, 5 deg to the side: steps 0/1 the draw and shoulder, 2/3 the
// aim (Blend eased, the aim blend em39BlendMotSet, 5 frames, a taunt), 4/5 the firing loop: up
// to 50 rounds, each round's hit at Timer2 3 (em39GunHitCk, a cartridge ejected), stopping early
// after 5 rounds on a dead player or when he gets behind; 6/7 the lower. Then the reload (r_no_3) or
// a wall jump / the retreating back flip.
static void em39_R1_Atk_MG(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 t;

    w->Be_flg |= 0xC0;
    w->Be_flg &= ~0x20;
    // pPL: the pPL load waits for the flags store (struct view), which frees the sched1 slots
    // that put `&b` and its PRE copy before the first call.
    PSMTXRotRad(m, 'y', LIMIT_ANGLE(GetXZAngle(&em->pos, &pPL->pos) + 0.08726646f));
    TransMatrix(m, &pPL->pos);
    b.x = 0.0f;
    b.y = 1500.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &b, &target);
    a = em->pos;
    a.y += 1600.0f;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x92), ARC(0x93), 5, 1, 0);
        w->Timer = (((MotionData*) ARC(0x92))->maxFrame & 0x3FFF) - 20;
        em39WepSet(em, 3);
        w->Atk_ck = 0;
        w->Act_ck = 0;
        w->LongAtk_wait = 300;
        em->r_no_2++;
    case 1:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->ang.y += Muku2(em->ang.y, t, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = ang;
        w->Hokan = 10;
        w->Frame = 0;
        MotionSetCore(em, MOTION(em), ARC(0x7C), 0, 0xA, 5, 0);
        w->TmpU32 = 50;
        w->Timer3 = 5;
        em->r_no_2++;
    case 3:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->ang.y += Muku2(em->ang.y, t, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = w->Blend * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7C), ARC(0x7D), ARC(0x7E), 0, 0, 0, 5);
        MotionMove(em, 0);
        if (w->Timer3) {
            w->Timer3--;
            break;
        }
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x32);
        } else {
            em39SetVoice(em, 0x33);
        }
        em->r_no_2++;
        break;
    case 4:
        w->Hokan = 0;
        w->Timer2 = 3;
        w->Frame = 0;
        if (w->pMachineGun) {
            MotionSetCore(w->pMachineGun, MOTION(w->pMachineGun), ARC(0x2F), 0, 0, 1, 0);
        }
        em->r_no_2++;
    case 5:
        em->ang.y += Muku(&em->pos, &target, em->ang.y, 0.012271847f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = w->Blend * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7F), ARC(0x80), ARC(0x81), 0, 0, 0, 5);
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (em->r_no_3) {
                EmRoutineSet(em, 1, 0x1E, 0, 1);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 0xE, 0, 2);
            }
        } else {
            if (w->TmpU32) {
                if (w->Timer2 == 3) {
                    em39WaistMove(em);
                    em->partsWorldCalc();
                    em39GunHitCk(em);
                    em39SetCartridge(em);
                }
                if (w->Timer2) {
                    w->Timer2--;
                    if (w->Timer2 == 0) {
                        w->TmpU32--;
                        em->r_no_2 = 4;
                        break;
                    }
                }
            }
            if (EmDeadCk(pPL) && (u32) w->TmpU32 > 5) {
                w->TmpU32 = 5;
            }
            if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 1.5707964f) {
                w->TmpU32 = 0;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x8E), ARC(0x8F), 0xA, 1, 0);
        em->r_no_2++;
    case 7:
        w->Be_flg &= ~0x40;
        if (MotionMove(em, 0)) {
            if (em->r_no_3) {
                EmRoutineSet(em, 1, 0x1E, 0, 1);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 0xE, 0, 2);
            }
        }
        break;
    }
    em39HandSet(em, 2);
}

// Routine 1/0x1E: the machine gun reload (gun in hand, its magazine motion). Then Wait (player
// dead); with r_no_3 set 60 % of the time another burst (beyond 5 m) or a grenade throw; else a wall
// jump, Turn or Walk.
static void em39_R1_Reload(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x90), ARC(0x91), 5, 1, 0);
        em39WepSet(em, 3);
        if (w->pMachineGun) {
            MotionSetCore(w->pMachineGun, MOTION(w->pMachineGun), ARC(0x30), 0, 0, 1, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (em->r_no_3 && (u8) (Rnd() % 10) > 3) {
                if ((u8) (Rnd() % 10) > 4 && em->l_pl > 25000000.0f) {
                    EmRoutineSet(em, 1, 0x1D, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 0x23, 0, 1);
                }
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
            }
        }
        break;
    }
    em39HandSet(em, 2);
}

// Routine 1/0x1F (from a perch, Sit): rises and fires the machine gun from cover at the player
// predicted 10 frames ahead (1.3 m up, aimed a degree to the side): steps 0/1 the rise with the gun
// (Total_damage primed to 200 so the next hit flinches), 2/3 the 30-frame aim with a taunt, 4/5 the
// firing loop (up to 50 rounds, a hit per round at Timer2 3; cut to 5 rounds on a dead player or
// when the player is leaving), 6/7 the lower (a line when he hit), then back to the crouch (Sit)
// with a 90..135 frame attack wait.
static void em39_R1_AppearMG(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 t;

    w->Be_flg |= 0xC0;
    w->Be_flg &= ~0x20;
    GetPlPos(&target, 10.0f, 0);
    target.y += 1300.0f;
    PSVECSubtract(&target, &em->pos, &b);
    PSMTXRotRad(m, 'y', 0.018325957f);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &b, &target);
    a = em->pos;
    a.y += 1400.0f;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x8C), ARC(0x8D), 0, 1, 0);
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->Timer = 10;
        em39WepSet(em, 3);
        w->Act_ck = 0;
        w->Total_damage = 200;
        em->r_no_2++;
    case 1:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->ang.y += Muku2(em->ang.y, t, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = ang;
        w->Hokan = 10;
        w->Frame = 0;
        MotionSetCore(em, MOTION(em), ARC(0x7C), 0, 0xA, 5, 0);
        w->TmpU32 = 50;
        w->Timer3 = 30;
        em->r_no_2++;
    case 3:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->ang.y += Muku2(em->ang.y, t, 0.19634955f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = w->Blend * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7C), ARC(0x7D), ARC(0x7E), 0, 0, 0, 5);
        MotionMove(em, 0);
        if (w->Timer3) {
            w->Timer3--;
            break;
        }
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x32);
        } else {
            em39SetVoice(em, 0x33);
        }
        em->r_no_2++;
        break;
    case 4:
        w->Hokan = 0;
        w->Timer2 = 3;
        w->Frame = 0;
        if (w->pMachineGun) {
            MotionSetCore(w->pMachineGun, MOTION(w->pMachineGun), ARC(0x2F), 0, 0, 1, 0);
        }
        em->r_no_2++;
    case 5:
        em->ang.y += Muku(&em->pos, &target, em->ang.y, 0.0061359233f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = w->Blend * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7F), ARC(0x80), ARC(0x81), 0, 0, 0, 5);
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em->r_no_2++;
        } else {
            if (w->TmpU32) {
                if (w->Timer2 == 3) {
                    w->TmpU32--;
                    em39WaistMove(em);
                    em->partsWorldCalc();
                    em39GunHitCk(em);
                    em39SetCartridge(em);
                }
                if (w->Timer2) {
                    w->Timer2--;
                    if (w->Timer2 == 0) {
                        em->r_no_2 = 4;
                        break;
                    }
                }
            }
            if (EmDeadCk(pPL) && (u32) w->TmpU32 > 5) {
                w->TmpU32 = 5;
            }
            if (em39ExitCk(em) && (u32) w->TmpU32 > 5) {
                w->TmpU32 = 5;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x8E), ARC(0x8F), 0xA, 1, 0);
        if (w->Atk_ck) {
            em39SetSpeech(em, 0x5A, 0x20);
        }
        em->r_no_2++;
    case 7:
        w->Be_flg &= ~0x40;
        if (MotionMove(em, 0)) {
            w->Atk_wait = (u8) (Rnd() % 45) + 90;
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    em39HandSet(em, 2);
}

// The same pitch with the negated atan2 kept in a function-scope variable (AppearMG2).
#define EM39_GUN_PITCH2(ang, t, target, a, b)                                                       \
    PSVECSubtract(&(target), &(a), &(b));                                                          \
    {                                                                                              \
        f32 d = SQRTF((b).x * (b).x + (b).z * (b).z);                                              \
        ang = -atan2f((b).y, d);                                                                   \
    }                                                                                              \
    t = ang * 325.94931f;                                                                          \
    if (t > 255.0f) {                                                                              \
        t = 255.0f;                                                                                \
    }                                                                                              \
    if (t < -255.0f) {                                                                             \
        t = -255.0f;                                                                               \
    }

// Routine 1/0x20 (from a wall point, WallWait): swings out of cover (the left or right variant by
// the player's side, r_no_3) turning to face him, aims 5 frames with a taunt, fires the machine
// gun loop as in em39_R1_AppearMG, then either walks straight in (within 3 m or 30 %) or swings
// back behind cover and returns to WallWait with a 60..90 frame attack wait.
static void em39_R1_AppearMG2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 t;

    w->Be_flg |= 0xC0;
    w->Be_flg &= ~0x20;
    GetPlPos(&target, 10.0f, 0);
    target.y += 1500.0f;
    PSVECSubtract(&target, &em->pos, &b);
    PSMTXRotRad(m, 'y', 0.018325957f);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &b, &target);
    a = em->pos;
    a.y += 1600.0f;
    switch (em->r_no_2) {
    case 0:
        if (Muku(&em->pos, &pPL->pos, em->ang.y, PI) < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x83), ARC(0x84), 3, 1, 0);
            w->Timer = (((MotionData*) ARC(0x83))->maxFrame & 0x3FFF) - 20;
            em->r_no_3 = 1;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x88), ARC(0x89), 3, 1, 0);
            w->Timer = (((MotionData*) ARC(0x88))->maxFrame & 0x3FFF) - 20;
            em->r_no_3 = 0;
        }
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->Timer = 10;
        em39WepSet(em, 3);
        w->Act_ck = 0;
        w->Total_damage = 200;
        w->TmpF = em->ang.y + PI;
        em->r_no_2++;
    case 1: {
        ang = Muku(&em->pos, &target, w->TmpF, 0.09817477f);
        w->TmpF += ang;
        w->TmpF = LIMIT_ANGLE(w->TmpF);
        em->ang.y += ang;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    }
    case 2:
        EM39_GUN_PITCH2(ang, t, target, a, b);
        w->Blend = t;
        w->Hokan = 10;
        w->Frame = 0;
        MotionSetCore(em, MOTION(em), ARC(0x7C), 0, 0xA, 5, 0);
        w->TmpU32 = 50;
        w->Timer3 = 5;
        em->r_no_2++;
    case 3:
        em->ang.y += Muku(&em->pos, &target, em->ang.y, 0.09817477f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        EM39_GUN_PITCH2(ang, t, target, a, b);
        w->Blend = w->Blend * 0.9f + t * 0.1f;
        em39BlendMotSet(em, ARC(0x7C), ARC(0x7D), ARC(0x7E), 0, 0, 0, 5);
        MotionMove(em, 0);
        if (w->Timer3) {
            w->Timer3--;
            break;
        }
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x32);
        } else {
            em39SetVoice(em, 0x33);
        }
        em->r_no_2++;
        break;
    case 4:
        w->Hokan = 0;
        w->Timer2 = 3;
        w->Frame = 0;
        if (w->pMachineGun) {
            MotionSetCore(w->pMachineGun, MOTION(w->pMachineGun), ARC(0x2F), 0, 0, 1, 0);
        }
        em->r_no_2++;
    case 5:
        EM39_GUN_PITCH2(ang, t, target, a, b);
        w->Blend = w->Blend * 0.9f + t * 0.1f;
        em39BlendMotSet(em, ARC(0x7F), ARC(0x80), ARC(0x81), 0, 0, 0, 5);
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (em->l_pl < 9000000.0f || (u8) (Rnd() % 10) > 6) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                em->r_no_2++;
            }
        } else {
            if (w->TmpU32) {
                if (w->Timer2 == 3) {
                    w->TmpU32--;
                    em39WaistMove(em);
                    em->partsWorldCalc();
                    em39GunHitCk(em);
                    em39SetCartridge(em);
                }
                if (w->Timer2) {
                    w->Timer2--;
                    if (w->Timer2 == 0) {
                        em->r_no_2 = 4;
                        break;
                    }
                }
            }
            if (EmDeadCk(pPL) && (u32) w->TmpU32 > 5) {
                w->TmpU32 = 5;
            }
        }
        break;
    case 6:
        if (em->r_no_3) {
            MotionSetCore(em, MOTION(em), ARC(0x85), ARC(0x86), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x8A), ARC(0x8B), 3, 1, 0);
        }
        em->r_no_2++;
    case 7:
        w->Be_flg &= ~0x40;
        if (MotionMove(em, 0)) {
            w->Atk_wait = (u8) (Rnd() % 30) + 60;
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
    em39HandSet(em, 2);
}

// Throw the grenade in hand towards the player (AppearGR / AppearGR2 hit frame).
#define EM39_GRENADE_THROW(em, w, spd)                                                              \
    (w)->pBomb->setGrenadeThrow(&(spd), 60, ARC(0x10B), ARC(0x10C), ARC(0x10D), ARC(0x111));   \
    (w)->pBomb = 0;                                                                             \
    em39WepSet(em, 0);

// Routine 1/0x21 (from a perch, Sit): rises and throws a grenade (a taunt if he has not fired for
// 450 frames): the grenade weapon is created in the hand on motion event bit 1 and thrown on event
// bit 0 with a speed scaled to the player's distance (EM39_GRENADE_THROW), tracking him while Timer
// runs; then back to the crouch with a 90..135 frame attack wait.
static void em39_R1_AppearGR(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;

    w->Be_flg |= 0x50;
    w->Be_flg &= ~0x20;
    if (w->Act_ck) {
        target = pPL->pos;
        target.y += 1300.0f;
    } else {
        Mtx m;

        GetPlPos(&target, 10.0f, 0);
        target.y += 1300.0f;
        PSVECSubtract(&target, &em->pos, &b);
        PSMTXRotRad(m, 'y', 0.008726646f);
        TransMatrix(m, &em->pos);
        PSMTXMultVec(m, &b, &target);
    }
    a = em->pos;
    a.y += 1600.0f;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x94), ARC(0x95), 3, 1, 0);
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->Act_ck = 0;
        w->Total_damage = 200;
        if (w->No_fire_timer > 450) {
            u8 r = Rnd() % 3;

            switch (r) {
            case 0:
            default:
                em39SetVoice(em, 0x26);
                break;
            case 1:
                em39SetVoice(em, 0x27);
                break;
            case 2:
                em39SetVoice(em, 0x28);
                break;
            }
            w->No_fire_timer = 0;
        }
        w->Timer = 30;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &target, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 2) {
            if (w->pBomb == 0) {
                Vec pos;
                Vec rot;

                pos.x = 0.0f;
                pos.y = 0.0f;
                pos.z = 0.0f;
                rot.x = 0.0f;
                rot.y = 0.0f;
                rot.z = 0.0f;
                w->pBomb = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6B), &pos, &rot, 0);
            }
            if (w->pBomb) {
                w->pBomb->setParent(em, 0xA, 0);
            }
            em39WepSet(em, 2);
        }
        if ((em->Motion.Seq_old.Free & 1) && w->pBomb) {
            Vec spd;
            f32 d = VEC_DISTXZ(&em->pos, &pPL->pos) - 2000.0f;

            if (d < 5000.0f) {
                d = 5000.0f;
            }
            d *= 0.022222223f;
            spd.x = 0.0f;
            spd.y = 250.0f;
            spd.z = d;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            EM39_GRENADE_THROW(em, w, spd);
        }
        if (MotionMove(em, 0)) {
            w->Atk_wait = (u8) (Rnd() % 45) + 90;
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    em39HandSet(em, 0);
}

// Routine 1/0x22 (from a wall point, WallWait): leans out of cover (left or right by the player's
// side) with a grenade already in hand and throws it at the player on motion event bit 0 (speed by
// distance), then back behind cover to WallWait with a 60..90 frame attack wait.
static void em39_R1_AppearGR2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec pos;
    Vec rot;
    Mtx m;
    Vec spd;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    target = pPL->getPartsPtr(4)->world;
    switch (em->r_no_2) {
    case 0:
        if (Muku(&em->pos, &pPL->pos, em->ang.y, PI) < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x97), ARC(0x98), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x99), ARC(0x9A), 3, 1, 0);
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pBomb = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6B), &pos, &rot, 0);
        if (w->pBomb) {
            w->pBomb->setParent(em, 0xA, 0);
        }
        em39WepSet(em, 2);
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->Act_ck = 0;
        w->Total_damage = 200;
        w->TmpF = em->ang.y + PI;
        em->r_no_2++;
    case 1:
        if ((em->Motion.Seq_old.Free & 1) && w->pBomb) {
            f32 d = (VEC_DISTXZ(&em->pos, &pPL->pos) - 1000.0f) * 0.025f;

            if (d < 200.0f) {
                d = 200.0f;
            }
            spd.x = 0.0f;
            spd.y = 180.0f;
            spd.z = d;
            PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
            PSMTXMultVecSR(m, &spd, &spd);
            EM39_GRENADE_THROW(em, w, spd);
        }
        if (MotionMove(em, 0)) {
            w->Atk_wait = (u8) (Rnd() % 30) + 60;
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
    em39HandSet(em, 0);
}

// Routine 1/0x23: the standing grenade throw (long attack held 600). With r_no_3 set and the player
// on the west side of the arena the target is one of three fixed spots along the walkway (by the
// boss's z) instead of the player; the grenade appears on motion event bit 1 and flies on bit 0
// (flat when thrown from 2 m above). Repeats r_no_3 times, ending in the taunt; otherwise a wall
// jump or the retreating back flip with a 30-frame attack wait.
static void em39_R1_ThrowGR(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec hand;

    Vec p1 = { 28961.0f, 5250.0f, -2620.0f };
    Vec p2 = { 31552.0f, 5250.0f, -6063.0f };
    Vec p3 = { 31546.0f, 5250.0f, -11491.0f };
    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    hand = pPL->getPartsPtr(4)->world;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x9B), ARC(0x9C), 3, 1, 0);
        w->LongAtk_wait = 600;
        w->Timer = 15;
        w->Act_ck = 0;
        w->Atk_ck = 0;
        w->TmpU32 = 0;
        if (pPL->pos.x < 30580.0f && em->r_no_3) { // struct view: the load then depends on the word stores too and the block issues in source order
            w->TmpU32 = 2;
            if (em->pos.z > -5360.0f) {
                w->TmpU32 = 1;
            }
            if (em->pos.z < -8792.0f) {
                w->TmpU32 = 3;
            }
        }
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            switch ((u32) w->TmpU32) {
            case 0:
            default:
                em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            case 1:
                em->ang.y += Muku(&em->pos, &p1, em->ang.y, 0.19634955f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
                break;
            case 2:
                em->ang.y += Muku(&em->pos, &p2, em->ang.y, 0.19634955f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
                break;
            case 3:
                em->ang.y += Muku(&em->pos, &p3, em->ang.y, 0.19634955f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
                break;
            }
        }
        if (em->Motion.Seq_old.Free & 2) {
            if (w->pBomb == 0) {
                Vec pos;
                Vec rot;

                pos.x = 0.0f;
                pos.y = 0.0f;
                pos.z = 0.0f;
                rot.x = 0.0f;
                rot.y = 0.0f;
                rot.z = 0.0f;
                w->pBomb = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6B), &pos, &rot, 0);
            }
            if (w->pBomb) {
                w->pBomb->setParent(em, 0xA, 0);
            }
            em39WepSet(em, 2);
        }
        if ((em->Motion.Seq_old.Free & 1) && w->pBomb) {
            Vec spd;
            Mtx m;
            Vec tpos;
            f32 d;

            switch ((u32) w->TmpU32) {
            case 0:
            default:
                tpos = pPL->pos;
                break;
            case 1:
                tpos = p1;
                break;
            case 2:
                tpos = p2;
                break;
            case 3:
                tpos = p3;
                break;
            }
            d = (VEC_DISTXZ(&em->pos, &tpos) - 1000.0f) * 0.025f;
            if (d < 200.0f) {
                d = 200.0f;
            }
            spd.x = 0.0f;
            spd.y = 120.0f;
            spd.z = d;
            if (em->pos.y > tpos.y + 2000.0f) {
                spd.y = 0.0f;
            }
            asm volatile("" : : "f"(d)); // COMPILER-DIFF: #13 (dying stores issued in source order)
            PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &tpos));
            PSMTXMultVecSR(m, &spd, &spd);
            w->pBomb->setGrenadeThrow(&spd, 45, ARC(0x10B), ARC(0x10C), ARC(0x10D), ARC(0x111));
            w->pBomb = 0;
            em39WepSet(em, 0);
        }
        if (MotionMove(em, 0) || (em->Motion.Seq_old.Free & 4)) {
            if (em->r_no_3) {
                em->r_no_3--;
                if (em->r_no_3 == 0) {
                    EmRoutineSet(em, 1, 0xC, 0, 1);
                } else {
                    em->r_no_0 = 1;
                    em->r_no_1 = 0x23;
                    em->r_no_2 = 0;
                }
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                w->Atk_wait = 30;
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        }
        break;
    }
    em39HandSet(em, 0);
}

// Routine 1/0x24 (from a perch, Sit): rises with the bow (Wep_type 4, the bow string set up and
// an arrow nocked, a taunt if he has not fired for 450 frames) and shoots 3..5 arrows at the player
// predicted 10 frames ahead: steps 2/3 the 30-frame draw with the aim blend (Blend), 4/5 the
// shot (em39ArrowFire with a random spread; the last arrow uses the release motion) and re-draw
// after 50 frames; the volley stops when the player dies, is knocked down or leaves. Then the lower
// and back to the crouch.
static void em39_R1_AppearBow(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    f32 ang;

    w->Be_flg |= 0x50;
    w->Be_flg &= ~0x20;
    if (w->Act_ck) {
        target = pPL->pos;
        target.y += 1300.0f;
    } else {
        Mtx m;

        GetPlPos(&target, 10.0f, 0);
        target.y += 1300.0f;
        PSVECSubtract(&target, &em->pos, &b);
        PSMTXRotRad(m, 'y', 0.008726646f);
        TransMatrix(m, &em->pos);
        PSMTXMultVec(m, &b, &target);
    }
    a = em->pos;
    a.y += 1600.0f;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC4), ARC(0xC5), 0xA, 1, 0);
        w->Timer = (((MotionData*) ARC(0xC4))->maxFrame & 0x3FFF) - 15;
        em39WepSet(em, 4);
        em39BowSet(em, 0);
        if (w->pBow) {
            MotionSetCore(w->pBow, MOTION(w->pBow), ARC(0x33), 0, 0, 0, 0);
        }
        em39ArrowSet(em);
        w->TmpU32 = (u8) (Rnd() % 3) + 3;
        w->Timer3 = 30;
        w->Act_ck = 0;
        em39HandSet(em, 3);
        em->be_flag |= 2;
        w->Total_damage = 200;
        if (w->No_fire_timer > 450) {
            u8 r = Rnd() % 3;

            switch (r) {
            case 0:
            default:
                em39SetVoice(em, 0x26);
                break;
            case 1:
                em39SetVoice(em, 0x27);
                break;
            case 2:
                em39SetVoice(em, 0x28);
                break;
            }
            w->No_fire_timer = 0;
        }
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &target, em->ang.y, 0.09817477f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        } else if (em->Motion.Seq_old.Free & 0x10) {
            em39BowSet(em, 1);
        }
        break;
    case 2:
        EM39_GUN_PITCH(ang, target, a, b);
        w->Blend = ang;
        w->Hokan = 10;
        w->Frame = 0;
        w->Atk_ck = 0;
        w->Act_ck = 0;
        em->r_no_2++;
    case 3:
        em->ang.y += Muku(&em->pos, &target, em->ang.y, 0.09817477f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        PSVECSubtract(&target, &a, &b);
        {
            f32 d = SQRTF(b.x * b.x + b.z * b.z);

            ang = -atan2f(b.y, d) * 257.32843f;
        }
        if (ang > 255.0f) {
            ang = 255.0f;
        }
        if (ang < -255.0f) {
            ang = -255.0f;
        }
        w->Blend = w->Blend * 0.8f + ang * 0.2f;
        em39BlendMotSet(em, ARC(0xC6), ARC(0xC7), ARC(0xC8), 0, 0, 0, 5);
        MotionMove(em, 0);
        if (em39ExitCk(em)) {
            em->r_no_2 = 6;
        }
        if (w->Timer3) {
            w->Timer3--;
            break;
        }
        em->r_no_2++;
        break;
    case 4:
        w->Hokan = 0;
        w->Frame = 0;
        if ((u32) w->TmpU32 > 1) {
            w->Mot_M = ARC(0xC9);
            w->Seq_M = ARC(0xCA);
            w->Mot_L = ARC(0xCB);
            w->Mot_R = ARC(0xCC);
            if (w->pBow) {
                MotionSetCore(w->pBow, MOTION(w->pBow), ARC(0x34), 0, 0, 0, 0);
            }
        } else {
            w->Mot_M = ARC(0xCE);
            w->Seq_M = 0;
            w->Mot_L = ARC(0xCF);
            w->Mot_R = ARC(0xD0);
            if (w->pBow) {
                MotionSetCore(w->pBow, MOTION(w->pBow), ARC(0x35), 0, 0, 0, 0);
            }
        }
        SndCall(8, 0x12, &em->pos, em->id, 0, em);
        em39ArrowFire(em, &target, (u8) (Rnd() % 3));
        em39HandSet(em, 0);
        w->Timer = 5;
        em->r_no_2++;
    case 5:
        PSVECSubtract(&target, &a, &b);
        {
            f32 d = SQRTF(b.x * b.x + b.z * b.z);

            ang = -atan2f(b.y, d) * 261.92355f;
        }
        if (ang > 255.0f) {
            ang = 255.0f;
        }
        if (ang < -255.0f) {
            ang = -255.0f;
        }
        w->Blend = w->Blend * 0.8f + ang * 0.2f;
        em39BlendMotSet(em, w->Mot_M, w->Mot_L, w->Mot_R, w->Seq_M, 0, 0, 1);
        if ((s16) pG->pl_life <= 0) {
            w->TmpU32 = 0;
        }
        if (pPL->r_no_0 == 1) {
            w->TmpU32 = 0;
        }
        if (em39ExitCk(em)) {
            w->TmpU32 = 0;
        }
        if (MotionMove(em, 0)) {
            if (w->TmpU32) {
                w->TmpU32--;
                if (w->TmpU32) {
                    w->Timer3 = 50;
                    em->r_no_2 = 2;
                    break;
                }
            }
            em->r_no_2++;
        } else {
            if (em->Motion.Seq_old.Free & 0x10) {
                em39BowSet(em, 1);
            }
            if (em->Motion.Seq_old.Free & 2) {
                em39ArrowSet(em);
                em39HandSet(em, 3);
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xD1), ARC(0xD2), 0xA, 1, 0);
        w->Timer = 30;
        em->r_no_2++;
    case 7:
        if (MotionMove(em, 0)) {
            w->Atk_wait = (u8) (Rnd() % 45) + 90;
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
}

// Routine 1/0x25: throws a flash grenade to cover his exit (invulnerable, Be_flg 0x130): the flash
// weapon is created in the hand and thrown on motion event bit 0 (setFlashThrow, 28 frames); then he
// vanishes into hiding (Hide) for 200 frames.
static void em39_R1_Flash(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec pos;
    Vec rot;
    Vec spd;
    int end;

    w->Be_flg |= 0x130;
    switch (em->r_no_2) {
    case 0: {
        MotionSetCore(em, MOTION(em), ARC(0x9B), ARC(0x9C), 6, 1, 0);
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pFlash = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6F), &pos, &rot, 0);
        if (w->pFlash) {
            w->pFlash->setParent(em, 0xA, 0);
        }
        em39WepSet(em, 2);
        w->Timer = 0;
        em->r_no_2++;
    }
    case 1:
        end = MotionMove(em, 0);
        if (end) {
            w->Hide_timer = 200;
            EmRoutineSet(em, 1, 0x26, 0, 0);
            break;
        }
        if ((em->Motion.Seq_old.Free & 1) && w->pFlash) {
            spd.x = 0.0f;
            spd.y = 100.0f;
            spd.z = 150.0f;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            w->Timer = 28;
            w->pFlash->setFlashThrow(&spd, 28);
            w->pFlash = 0;
        }
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                w->Hide_timer = 200;
                EmRoutineSet(em, 1, 0x26, 0, 0);
            }
        }
        break;
    }
    em39HandSet(em, 0);
}

// Routine 1/0x26 (types 0 / 1): hidden between appearances (invisible, no collision, unlockable,
// invulnerable; Be_flg 0x430). Entering it drops the knife / arrow, resets the goto and advances
// the battle phase (Locate 1 -> 2, 4 -> 5; Be_flg 0x20000 forces 5, the final phase, which also
// sets Be_flg 0x100000). He reappears (em39AppearCk) once Hide_timer is out and the damage taken
// in the last appearance (Flash_damage) is under the phase's limit (1000 / 500 / 0).
static void em39_R1_Hide(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int lim;

    em->dmg.m_Timer = 2;
    w->Be_flg |= 0x430;
    if (w->Be_flg & 0x20000) {
        w->Locate = 5;
    }
    switch (em->r_no_2) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em39WepSet(em, 0);
        if (w->pArrow) {
            w->pArrow->setLost();
            w->pArrow = 0;
        }
        em->be_flag |= 0x10000000;
        em->setStatus(EM_STATUS_LOCKOFF);
        w->Back_atk_wait = 450;
        w->Flash_damage = 0;
        w->Goto_mode = 0;
        if (w->Locate == 1) {
            w->Locate = 2;
        }
        if (w->Locate == 4) {
            w->Flash_damage = 0;
            w->Locate = 5;
        }
        w->pGotoPoint = 0;
        em->be_flag &= ~2;
        em->r_no_2++;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x73), 0, 0, 0, 0);
        MotionMove(em, 0);
        if (w->Locate == 5) {
            w->Be_flg |= 0x100000;
        }
        switch (w->Locate) {
        case 0:
        case 3:
        default:
            lim = 1000;
            break;
        case 1:
            lim = 500;
            break;
        case 2:
            lim = 500;
            break;
        case 4:
            lim = 500;
            break;
        case 5:
            lim = 0;
            break;
        }
        if (w->Flash_damage < lim && w->Hide_timer == 0 && em39AppearCk(em)) {
            em->be_flag &= ~0x10000000;
            em->clearStatus(EM_STATUS_LOCKOFF);
        }
        break;
    }
}

// Branch check of the mutated-arm swing: on motion event bit 0 the arm (attack 4, em39LeftArmAtkCk)
// hits; a hit next to a cliff spot (em39GetCliffPos) becomes the cliff kill (stat -> T_CliffAtk,
// the player kept at 1 HP for it), otherwise the player bleeds and is knocked down facing the boss.
static void em39_R1_br_T_Atk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (em->hp > 0 && (em->Motion.Seq_old.Free & 1) && w->Atk_ck == 0) {
        em39LeftArmAtkCk(em, 4);
        if (w->Atk_ck) {
            SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
            if (em39GetCliffPos(em)) {
                if ((s16) pG->pl_life <= 0) {
                    pG->pl_life = 1;
                }
                EmRoutineSetW(em, 1, 0x2E, 0, 0);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0x2F, 0x2C);
                pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(PL_DM_AUTO, 0, 0);
            }
        }
    }
}

// The mutated-arm swings share the wait / cancel handling.
#define EM39_T_ATK_TAIL(em, w, action)                                                              \
    if (em->Motion.Seq_old.Free & 2) {                                                                      \
        w->Act_ck = 1;                                                                               \
    }                                                                                              \
    if (w->Timer2) {                                                                                   \
        w->Timer2--;                                                                                   \
    } else if (w->Atk_ck == 0 && w->Act_ck == 0) {                                                     \
        switch ((u32) w->TmpU32) {                                                                    \
        case 0:                                                                                    \
        default:                                                                                   \
            ActBtn.set(ACT_GUARD, 0xB, (void*) action, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);                             \
            break;                                                                                 \
        case 1:                                                                                    \
            ActBtn.set(ACT_GUARD, 0xB, (void*) action, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);                             \
            break;                                                                                 \
        }                                                                                          \
    }

#define EM39_T_ATK_INIT(em, w, est)                                                                 \
    EstSet(em, -1, 0, 0, EFF_EM39, est, 0, w->EffKindId, em, 0);                             \
    w->Arm_rno = 4;                                                                                   \
    w->Timer2 = 6;                                                                                     \
    if (pG->Game_level <= 1) {                                                                          \
        w->Timer2 = 0;                                                                                 \
    }                                                                                              \
    if (pG->Game_level <= 3) {                                                                          \
        w->Timer2 = 3;                                                                                 \
    }                                                                                              \
    if (pG->Game_level > 6) {                                                                           \
        w->Timer2 = 7;                                                                                 \
    }                                                                                              \
    if (pG->Game_level > 9) {                                                                           \
        w->Timer2 = 9;                                                                                 \
    }                                                                                              \
    w->TmpU32 = Rnd() & 1;                                                                            \
    if (pG->Game_level <= 3) {                                                                         \
        w->TmpU32 = 0;                                                                                \
    }                                                                                              \
    w->Act_ck = 0;                                                                                   \
    w->Atk_ck = 0;

#define EM39_T_ATK_END(em, w, end)                                                                  \
    if (end) {                                                                                     \
        int zero = 0;                                                                              \
                                                                                                   \
        w->Arm_rno = zero;                                                                            \
        if ((s16) pG->pl_life <= 0) {                                                              \
            EmRoutineSet(em, 1, 4, zero, zero);                                                    \
        } else {                                                                                   \
            w->Atk_wait = 30;                                                                          \
            EmRoutineSet(em, 1, 0xE, zero, 1);                                                     \
        }                                                                                          \
    } else if (em->Motion.Seq_old.Free & 4) {                                                               \
        if (w->Atk_ck == 0) {                                                                        \
            GameAddPoint(LVADD_ESCAPEATTACK);                                                                     \
        }                                                                                          \
        EmRoutineSet(em, 1, 0xE, end, 1);                                                          \
    }

// Routine 1/0x27 (type 2): the mutated-arm swing (its effect; the hit is in em39_R1_br_T_Atk),
// homing while motion event bit 3 is set. The back-jump dodge prompt (em39BackjumpAction) is offered
// after the wind-up until it connects; a dodge or the swing's end (event bit 2, an escape point on a
// miss) leads into the retreating back flip, a dead player into Wait.
static void em39_R1_T_Atk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xDB), ARC(0xDC), 3, 1, 0);
        EM39_T_ATK_INIT(em, w, 0x30);
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        end = MotionMove(em, 0);
        EM39_T_ATK_END(em, w, end);
        break;
    }
    EM39_T_ATK_TAIL(em, w, em39BackjumpAction);
}

// Routine 1/0x28 (type 2): the backhand with the mutated arm (attack 5 on motion event bit 0),
// with the duck prompt (em39SitAction); exits as em39_R1_T_Atk.
static void em39_R1_T_BackKnuckle(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xDD), ARC(0xDE), 3, 1, 0);
        EM39_T_ATK_INIT(em, w, 0x31);
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 1) {
            em39LeftArmAtkCk(em, 5);
        }
        end = MotionMove(em, 0);
        EM39_T_ATK_END(em, w, end);
        break;
    }
    EM39_T_ATK_TAIL(em, w, em39SitAction);
}

// Branch check of the long arm sweep: attack 6 on motion event bit 0, with the same cliff kill /
// knock-down outcome as em39_R1_br_T_Atk.
static void em39_R1_br_T_LongAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (em->hp > 0 && (em->Motion.Seq_old.Free & 1) && w->Atk_ck == 0) {
        em39LeftArmAtkCk(em, 6);
        if (w->Atk_ck) {
            SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
            if (em39GetCliffPos(em)) {
                if ((s16) pG->pl_life <= 0) {
                    pG->pl_life = 1;
                }
                EmRoutineSetW(em, 1, 0x2E, 0, 0);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0x2F, 0x2C);
                pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(PL_DM_AUTO, 0, 0);
            }
        }
    }
}

// The long swings offer the dodge button only while the enemy faces the player.
#define EM39_T_LONG_TAIL(em, w, action)                                                             \
    if (w->Timer2) {                                                                                   \
        w->Timer2--;                                                                                   \
    } else {                                                                                       \
        f32 a = fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI));                                   \
                                                                                                   \
        if (w->Atk_ck == 0 && w->Act_ck == 0 && a < 1.5707964f) {                                      \
            switch ((u32) w->TmpU32) {                                                                \
            case 0:                                                                                \
            default:                                                                               \
                ActBtn.set(ACT_GUARD, 0xB, (void*) action, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, 0);                         \
                break;                                                                             \
            case 1:                                                                                \
                ActBtn.set(ACT_GUARD, 0xB, (void*) action, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, 0);                         \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    }

// Routine 1/0x29 (type 2): the long lunging arm sweep (the hit is in em39_R1_br_T_LongAtk), the
// long attack held 450, invulnerable for Timer frames; the duck prompt (em39SitAction) is offered
// while he faces the player. Exits as em39_R1_T_Atk.
static void em39_R1_T_LongAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xDF), ARC(0xE0), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x32, 0, w->EffKindId, em, 0);
        w->Arm_rno = 4;
        w->Timer = 30;
        w->LongAtk_wait = 450;
        w->Timer2 = 15;
        w->Atk_ck = 0;
        w->Act_ck = 0;
        if (pG->Game_level <= 1) {
            w->Timer2 = 5;
        }
        if (pG->Game_level <= 3) {
            w->Timer2 = 10;
        }
        if (pG->Game_level > 6) {
            w->Timer2 = 16;
        }
        if (pG->Game_level > 9) {
            w->Timer2 = 18;
        }
        w->TmpU32 = Rnd() & 1;
        if (pG->Game_level <= 3) {
            w->TmpU32 = 0;
        }
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        end = MotionMove(em, 0);
        if (end) {
            int zero = 0;

            w->Arm_rno = zero;
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, zero, zero);
            } else {
                w->Atk_wait = 30;
                EmRoutineSet(em, 1, 0xE, zero, 1);
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            EmRoutineSet(em, 1, 0xE, end, 1);
        } else if (em->Motion.Seq_old.Free & 2) {
            w->Act_ck = 1;
        }
        break;
    }
    EM39_T_LONG_TAIL(em, w, em39SitAction);
}

// Routine 1/0x2A (type 2): the leaping arm slam (effects with the Ada variant, long attack held
// 450; invulnerable until motion event bit 1): attack 7 on event bit 0, the back-jump prompt while
// he faces the player. A miss on event bit 2 awards the escape point and may chain into another
// attack or a walk within 3 m; otherwise the retreating back flip or Wait.
static void em39_R1_T_JumpAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE5), ARC(0xE6), 3, 1, 0);
        if (pG->pl_type == 2) {
            EstSet(em, -1, 0, 0, EFF_EM39, 0x4B, 0, w->EffKindId, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM39, 0x3B, 0, w->EffKindId, em, 0);
        }
        EstSet(em, -1, 0, 0, EFF_EM39, 0x3C, 0, w->EffKindId, em, 0);
        w->LongAtk_wait = 450;
        w->Timer = 15;
        w->Atk_ck = 0;
        w->Act_ck = 0;
        if (pG->Game_level <= 1) {
            w->Timer = 5;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 10;
        }
        if (pG->Game_level > 6) {
            w->Timer = 16;
        }
        if (pG->Game_level > 9) {
            w->Timer = 18;
        }
        w->TmpU32 = Rnd() & 1;
        if (pG->Game_level <= 3) {
            w->TmpU32 = 0;
        }
        w->Timer3 = 1;
        w->Arm_rno = 4;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 1) {
            em39LeftArmAtkCk(em, 7);
        }
        if (em->Motion.Seq_old.Free & 2) {
            w->Timer3 = 0;
        }
        if (w->Timer3) {
            w->Be_flg |= 0x100;
        }
        if (MotionMove(em, 0)) {
            int zero = 0;

            w->Arm_rno = zero;
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, zero, zero);
            } else {
                w->Atk_wait = 30;
                EmRoutineSet(em, 1, 0xE, zero, 1);
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
                if (w->Atk_ck == 0) {
                    int rtn = em39AtkRtnCk(em);

                    if (rtn) {
                        break;
                    }
                    if ((u8) (Rnd() % 10) > 4 && em->l_pl < 9000000.0f) {
                        EmRoutineSet(em, 1, 8, rtn, 1);
                        break;
                    }
                }
            }
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
    if (em->Motion.Seq_old.Free & 2) {
        w->Act_ck = 1;
    }
    EM39_T_LONG_TAIL(em, w, em39BackjumpAction);
}

// Frame window test on the player's motion frame (sound cues of the damage motions).
#define PL_FRAME_IN(pl, lo, hi) ((pl)->Motion.Seq_frame > (lo) && (pl)->Motion.Seq_frame < (hi))

// Player damage callback of the arm kick / stamp hits: the knock-down motion (the death variant
// at 0 HP) with its blood effect and the footstep / get-up sounds; ends with the motion.
static void plem39_Stamp(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x127), 0, 5, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM39, 0x3E, 0, ESP_CORE_KIND_NONE, pl, 0);
            PlSetDamageSe(0xD);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x126), 0, 5, 1, 0);
            EstSet(pl, -1, 0, 0, EFF_EM39, 0x3D, 0, ESP_CORE_KIND_NONE, pl, 0);
            PlSetDamageSe(0);
        }
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        if (MotionMove(pl, 0) && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
            break;
        }
        if (PL_FRAME_IN(pl, 4.7f, 5.3f)) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if ((s16) pG->pl_life > 0) {
            if (PL_FRAME_IN(pl, 87.7f, 88.3f) || PL_FRAME_IN(pl, 116.7f, 117.3f)) {
                SndCall(5, 0, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 106.7f, 107.3f) || PL_FRAME_IN(pl, 130.7f, 131.3f)) {
                SndCall(5, 1, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 59.7f, 60.3f)) {
                SndCall(1, 4, &pl->pos, 0, 0, pl);
                SndCall(1, 0x29, &pl->getPartsPtr(0)->world, 0, 0, pPL);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Action button of the arm swings: the player ducks (plem39Sit; Act_ck marks the dodge) and gets
// a critical-hit rank point.
static void em39SitAction(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    SetPlDamage(em, plem39Sit);
    w->Act_ck = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the duck (Ada has her own motion): 30 invulnerable frames, an escape
// rank point; ends with the motion.
static void plem39Sit(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 0x1E);
    switch (pl->r_no_2) {
    case 0:
        if (pG->pl_type == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x12A), 0, 5, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x110), 0, 5, 1, 0);
        }
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

// Action button of the arm swings / kicks: the player jumps back (plemBackjump; the side roll
// variant, step 2, when the boss is behind him), Act_ck marks the dodge, a critical-hit rank point.
static void em39BackjumpAction(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    f32 ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI));

    SetPlDamage(em, plemBackjump);
    if (ang > 1.5707964f) {
        pPL->r_no_2 = 2;
    }
    w->Act_ck = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// The dodge: back jump (cases 0/1) or side roll (cases 2/3), cancelled by any button after 35 frames.
#define PLEM39_BACKJUMP_INIT(pl, motA, motB)                                                        \
    if (pG->pl_type == 2) {                                                                          \
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, motA), 0, 3, 1, 5);                  \
    } else {                                                                                       \
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, motB), 0, 3, 1, 5);                  \
    }                                                                                              \
    EstSet(pl, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, pl, 0);                                       \
    SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);                                     \
    SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);                                     \
    GameAddPoint(LVADD_ESCAPEATTACK);                                                                             \
    pl->m_Work0 = 35;                                                                                 \
    pl->m_Work1 = 0;                                                                                  \
    pl->r_no_2++;

#define PLEM39_BACKJUMP_KEY(pl)                                                                     \
    if (pl->m_Work0) {                                                                                \
        pl->m_Work0--;                                                                                \
    } else if (Key.on & 0x1F) {                                                                    \
        pl->m_Work1 = 1;                                                                              \
    }

// Player damage callback of the dodge (dmType 0x1E: invulnerable): the back jump (steps 0/1) or
// the side roll (2/3), each with its dust and sounds, cancellable by any button after 35 frames;
// ends with the motion.
static void plemBackjump(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 0x1E;
    switch (pl->r_no_2) {
    case 0:
        PLEM39_BACKJUMP_INIT(pl, 0x12B, 0x10D);
    case 1:
        PLEM39_BACKJUMP_KEY(pl);
        if (PL_FRAME_IN(pl, 10.7f, 11.3f)) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 21.7f, 22.3f)) {
            SndCall(5, 0x14, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 36.7f, 37.3f) || PL_FRAME_IN(pl, 49.7f, 50.3f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 37.7f, 38.3f) || PL_FRAME_IN(pl, 50.7f, 51.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0) || pl->m_Work1) {
            EndPlDamage();
        }
        break;
    case 2:
        PLEM39_BACKJUMP_INIT(pl, 0x12C, 0x111);
    case 3:
        PLEM39_BACKJUMP_KEY(pl);
        if (PL_FRAME_IN(pl, 11.7f, 12.3f)) {
            EstSet(0, -1, &pl->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 21.7f, 22.3f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 34.7f, 35.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0) || pl->m_Work1) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Branch check of the high kick: on motion event bit 1 with the player in the kick zone
// (em39KickHitCk): 500 damage; next to a cliff spot the cliff kill (stat -> T_CliffAtk), else the
// knock-down facing the boss.
static void em39_R1_br_T_Kick(cEm39* em)
{
    if (em->hp > 0 && (em->Motion.Seq_old.Free & 2) && em39KickHitCk(em)) {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
        if (em39GetCliffPos(em)) {
            LifeDownSet2(pPL, 500, 0, 1);
            EmRoutineSetW(em, 1, 0x2E, 0, 0);
        } else {
            LifeDownSet(pPL, 500, 0);
            pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
            PlSetDamage(PL_DM_AUTO, 0, 0);
        }
    }
}

// Kick hit probe: a point of the leg (parts 0x13) matrix, checked against the player 200 up.
#define EM39_KICK_CK(em, mat, a, b, ay)                                                             \
    (a).x = 0.0f;                                                                                  \
    (a).y = ay;                                                                                    \
    (a).z = 0.0f;                                                                                  \
    PSMTXMultVec(mat, &(a), &(a));                                                                 \
    (a).y += 200.0f;                                                                               \
    (b).y = (a).y;                                                                                 \
    em39AtkCk2(em, 8, &(a), &(b));

// Routine 1/0x2B (type 2): the spinning high kick (its effect; the zone hit is in
// em39_R1_br_T_Kick, and attack 8 is swept along the leg part 0x13 on motion event bit 0). On
// event bit 2 a miss awards the escape point and, facing a player within 3 m (50 %), chains into
// another kick or the low kick beyond 2 m, else the melee selection or a walk; otherwise the
// retreating back flip or Wait.
static void em39_R1_T_Kick(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE1), ARC(0xE2), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x2F, 0, w->EffKindId, em, 0);
        w->Arm_rno = 0;
        w->Atk_ck = 0;
        w->Act_ck = 0;
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->Motion.Seq_old.Free & 1) {
            cModel* p;

            b = em->pos;
            p = em->getPartsPtr(0x13);
            EM39_KICK_CK(em, p->mat, a, b, 0.0f);
            EM39_KICK_CK(em, p->mat, a, b, -200.0f);
            EM39_KICK_CK(em, p->mat, a, b, -400.0f);
            EM39_KICK_CK(em, p->mat, a, b, -600.0f);
        }
        if (em->Motion.Seq_old.Free & 0x10) {
            w->Atk_ck = 0;
        }
        if (MotionMove(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                w->Atk_wait = 30;
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (em->r_no_3 == 0 && em->l_pl < 9000000.0f && (u8) (Rnd() % 10) > 4 && w->Pl_rot < 1.0471976f) {
                int hit = w->Atk_ck;

                if (hit != 0) {
                    goto rtn_e;
                }
                if (em->l_pl > 4000000.0f) {
                    if ((u8) (Rnd() % 10) > 4) {
                        EmRoutineSet(em, 1, 0x2B, hit, 1);
                    } else {
                        EmRoutineSet(em, 1, 0x2C, hit, 1);
                    }
                    break;
                }
            }
            if (w->Atk_ck == 0) {
                int rtn = em39AtkRtnCk(em);

                if (rtn) {
                    break;
                }
                if ((u8) (Rnd() % 10) > 4 && em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 8, rtn, 1);
                    break;
                }
            }
        rtn_e:
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
}

// Branch check of the low kick: on motion event bit 1 with the player in the kick zone, 500 damage
// and the trample follow-up (stat -> T_LowKickHit 1/0x2D).
static void em39_R1_br_T_LowKick(cEm39* em)
{
    if (em->hp > 0 && (em->Motion.Seq_old.Free & 2) && em39KickHitCk(em)) {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        LifeDownSet2(pPL, 500, 0, 1);
        SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
        EmRoutineSetW(em, 1, 0x2D, 0, 0);
    }
}

// Routine 1/0x2C (type 2): the sweeping low kick (effect with the Ada variant; the zone hit is in
// em39_R1_br_T_LowKick). On motion event bit 2 a miss awards the escape point and may chain into a
// high or low kick (facing a player at 2..3 m, 50 %), the melee selection or a walk; otherwise the
// retreating back flip or Wait.
static void em39_R1_T_LowKick(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->r_no_2;

    w->Be_flg |= 0x80;
    w->Be_flg &= ~0x20;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE3), ARC(0xE4), 3, 1, 0);
        if (pG->pl_type == 2) {
            EstSet(em, -1, 0, 0, EFF_EM39, 0x48, 0, w->EffKindId, em, (void*) st);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM39, 0x35, 0, w->EffKindId, em, (void*) st);
        }
        w->Arm_rno = 0;
        w->Atk_ck = 0;
        w->Act_ck = 0;
        w->Action_timer = 0x19;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, 0.19634955f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                w->Atk_wait = 30;
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            int f;

            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            f = em->r_no_3;
            if (f == 0 && em->l_pl < 9000000.0f && (u8) (Rnd() % 10) > 4 && w->Pl_rot < 1.0471976f &&
                em->l_pl > 4000000.0f) {
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x2B, f, 1);
                } else {
                    EmRoutineSet(em, 1, 0x2C, f, 1);
                }
                break;
            }
            if (w->Atk_ck == 0) {
                int rtn = em39AtkRtnCk(em);

                if (rtn) {
                    break;
                }
                if ((u8) (Rnd() % 10) > 4 && em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 8, rtn, 1);
                    break;
                }
            }
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
}

// Routine 1/0x2D (type 2): the swept player is pinned and the arm comes down (EmCatchPLSet with
// plem39_LowKickHit; arm pose 0xC, the pin effects). Step 0/1: after Timer frames (by difficulty)
// the action prompt (em39ActOn) appears until the arm falls (motion event bit 2); the fall on event
// bit 0 kills. A press in time goes to step 2/3: the player rolls clear (arm pose 0xE, effects), then
// the retreating back flip.
static void em39_R1_T_LowKickHit(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->r_no_2;
    int end;

    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE9), ARC(0xEA), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -129.7f, 0.0f, 1405.22f, plem39_LowKickHit);
        w->Act_ck = st;
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x23);
        } else {
            em39SetVoice(em, 0x24);
        }
        w->Arm_rno = 0xC;
        w->Timer = 10;
        if (pG->Game_level <= 1) {
            w->Timer = 5;
        }
        if (pG->Game_level <= 3) {
            w->Timer = 7;
        }
        if (pG->Game_level > 6) {
            w->Timer = 12;
        }
        if (pG->Game_level > 9) {
            w->Timer = 15;
        }
        if ((s16) pG->pl_life <= 1) {
            w->Timer = 18;
        }
        w->TmpU32B = 0;
        w->TmpU32 = Rnd() & 1;
        if (pG->Game_level <= 9) {
            w->TmpU32 = 0;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x36, 0, w->EffKindId, em, 0);
        if (pG->pl_type == 2) {
            EstSet(pPL, -1, 0, 0, EFF_EM39, 0x47, 0, w->EffKindId, em, 0);
        } else {
            EstSet(pPL, -1, 0, 0, EFF_EM39, 0x33, 0, w->EffKindId, em, 0);
        }
        em->r_no_2++;
    case 1:
        em->dmg.m_Timer = 2;
        EmCatchMotionMove(em, 1.0f, 1.0f);
        if (em->Motion.Seq_old.Free & 4) {
            w->TmpU32B = 1;
        }
        if (em->Motion.Seq_old.Free & 1) {
            pG->pl_life = 0;
        }
        if (w->Act_ck) {
            em->r_no_2 = 2;
            break;
        }
        if (w->Timer) {
            w->Timer--;
            break;
        }
        if (w->TmpU32B == 0) {
            if (w->TmpU32) {
                ActBtn.set(ACT_GUARD, 0xB, (void*) em39ActOn, em, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
            } else {
                ActBtn.set(ACT_GUARD, 0xB, (void*) em39ActOn, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xEB), ARC(0xEC), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, -84.43f, 0.0f, 1068.35f, plem39_LowKickHit);
        pPL->r_no_2 = st;
        EM39_K4_EFF_DELETE(em, w);
        w->Arm_rno = 0xE;
        if (pG->pl_type == 2) {
            EstSet(em, -1, 0, 0, EFF_EM39, 0x4C, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM39, 0x37, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        EstSet(pPL, -1, 0, 0, EFF_EM39, 0x34, 0, ESP_CORE_KIND_NONE, pPL, 0);
        w->Timer = 10;
        em->r_no_2++;
    case 3:
        if (w->Timer) {
            w->Timer--;
            end = EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            end = MotionMove(em, 0);
        }
        if (end || (em->Motion.Seq_old.Free & 4)) {
            w->Atk_wait = 30;
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player damage callback of the low-kick pin (Ada has her own motions): 0/1 pinned following the
// boss's motion with the impact / crush sounds and rumble (released early if the boss leaves the
// hold), 2/3 the roll clear; ends with the motion.
static void plem39_LowKickHit(cPlayer* pl)
{
    cModel* p = pl->getPartsPtr(4);
    int end;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        if (pG->pl_type == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x12F), 0, 0, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11F), 0, 0, 1, 0);
        }
        PlSetDamageSe(0);
        PlSetFace(1);
        pl->atari.set(0xA, 480.00003f, 400.0f);
        pl->Wep->setTrans(0, 0);
        pl->r_no_2++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pPL->pEmCatch->r_no_0 != 1 && pPL->pEmCatch->r_no_1 != 0x1B) {
            pl->Wep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
            break;
        }
        if (PL_FRAME_IN(pl, 18.7f, 19.3f)) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 43.7f, 44.3f)) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xD, 1);
        }
        if (PL_FRAME_IN(pl, 78.7f, 79.3f)) {
            if (pG->pl_type == 2) {
                SndCall(8, 0x56, &p->world, pl->pEmCatch->id, 0, pPL);
            } else {
                SndCall(8, 0x54, &p->world, pl->pEmCatch->id, 0, pPL);
            }
        }
        if (PL_FRAME_IN(pl, 130.7f, 131.3f)) {
            PlSetDamageSe(0xD);
        }
        if (PL_FRAME_IN(pl, 151.7f, 152.3f)) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    case 2:
        if (pG->pl_type == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x130), 0, 0, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x120), 0, 0, 1, 0);
        }
        pl->m_Work0 = 10;
        SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        pl->r_no_2++;
    case 3:
        if (pl->m_Work0) {
            pl->m_Work0--;
            end = EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            end = MotionMove(pl, 0);
        }
        if (end) {
            pl->Wep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            if (PL_FRAME_IN(pl, 12.7f, 13.3f) || PL_FRAME_IN(pl, 40.7f, 41.3f)) {
                SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 37.7f, 38.3f)) {
                SndCall(1, 0x43, &p->world, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 45.7f, 46.3f)) {
                SndCall(5, 0xD, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 47.7f, 48.3f)) {
                SndCall(5, 0xE, &pl->pos, 0, 0, pl);
            }
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Cliff attack: the enemy stands on the ledge (Target_pos / jumpAng) and drags the player over it.
// The object pointer is a one-member struct so that every store through the object reloads it.
static struct {
    cObj* p;
} em39CliffObj = { 0 };
// Debug laser-marker line colours (em39MarkerMove).
static u32 em39MarkerCol0 = 0x20400000;
static u32 em39MarkerCol1 = 0x20800000;

#define EM39_CLIFF_POS(em, w, mat, a, az)                                                            \
    PSMTXRotRad(mat, 'y', (w)->Target_dir);                                                            \
    TransMatrix(mat, &(w)->Target_pos);                                                                \
    (a).x = 0.0f;                                                                                  \
    (a).y = 0.0f;                                                                                  \
    (a).z = az;                                                                                    \
    PSMTXMultVec(mat, &(a), &(em)->pos);                                                           \
    (em)->ang.y = (w)->Target_dir;

// Routine 1/0x2E (type 2): the cliff-edge grab: the boss stands on the ledge (Target_pos /
// Target_dir) holding the player over the drop (plem39_CliffAtk). Step 0/1: the button-mash
// prompt during motion event bit 2, TmpU32 presses (5..20 by difficulty and player HP) break free
// before event bit 0 drops him (the kill); voice and rumble cues. Step 2/3: the break-free counter
// with its effect; then Be_flg 0x800000 (the fight is on) and the retreating back flip.
static void em39_R1_T_CliffAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->r_no_2;
    Mtx mat;
    Vec a;

    em->dmg.m_Timer = 2;
    switch (st) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        EM39_CLIFF_POS(em, w, mat, a, -1145.15f);
        MotionSetCore(em, MOTION(em), ARC(0xED), ARC(0xEE), 0, 1, 0);
        SetPlDamage(em, plem39_CliffAtk);
        w->Arm_rno = st;
        w->TmpU32 = 10;
        if (pG->Game_level <= 1) {
            w->TmpU32 = 5;
        }
        if (pG->Game_level <= 3) {
            w->TmpU32 = 8;
        }
        if (pG->Game_level > 6) {
            w->TmpU32 = 12;
        }
        if (pG->Game_level > 9) {
            w->TmpU32 = 15;
        }
        if ((s16) pG->pl_life <= 1) {
            w->TmpU32 = 20;
        }
        EM39_K4_EFF_DELETE(em, w);
        w->Timer = st;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Timer = 4;
            EstSet(0, -1, 0, 0, EFF_EM39, 0x38, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (em->Motion.Seq_old.Free & 1) {
            pG->pl_life = 0;
        }
        if (em->Motion.Seq_old.Free & 4) {
            ActBtn.set(ACT_CLIMB, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
            if (Key.trg & 0x80000) {
                if (w->TmpU32 == 0) {
                    em->r_no_2++;
                    break;
                }
                w->TmpU32--;
            }
        }
        if (em->Motion.Seq_old.Free & 0x10) {
            em39SetVoice(em, 0x52);
        }
        if (em->Motion.Seq_frame > 48.7f && em->Motion.Seq_frame < 49.3f) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        }
        if (em->Motion.Seq_frame > 139.7f && em->Motion.Seq_frame < 140.3f) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xB, 1);
        }
        break;
    case 2:
        EM39_CLIFF_POS(em, w, mat, a, -382.78f);
        MotionSetCore(em, MOTION(em), ARC(0xEF), ARC(0xF0), 0, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x39, 0, ESP_CORE_KIND_NONE, em, 0);
        SetPlDamage(em, plem39_CliffAtk);
        pPL->r_no_2 = st;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0) || (em->Motion.Seq_old.Free & 1)) {
            w->Be_flg |= 0x800000;
            AtariOn(&em->atari, 0x300);
            w->Atk_wait = 30;
            EmRoutineSet(em, 1, 0xE, 0, 1);
        } else if (em->Motion.Seq_old.Free & 4) {
            AtariOn(&em->atari, 0x300);
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player damage callback of the cliff grab (r_no_2 driven by the boss): 0/1 held over the drop
// facing the boss with the struggle sounds, 2/3 the break-free: the knife object appears in his
// hand (em39CliffObj) and he stabs the arm, then the weapon returns and the damage ends.
static void plem39_CliffAtk(cPlayer* pl)
{
    cModel* p = pl->getPartsPtr(4);

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 0xA);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        AtariOff(&pl->atari, 0xFCFF);
        pl->pos.x = -15.66f;
        pl->pos.y = 0.0f;
        pl->pos.z = 934.56f;
        PSMTXMultVec(pl->pEmCatch->mat, &pl->pos, &pl->pos);
        pl->ang.y = pl->pEmCatch->ang.y + PI;
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x121), 0, 0, 1, 0);
        PlSetFace(1);
        pl->Wep->setTrans(0, 0);
        pl->r_no_2++;
    case 1:
        if (PL_FRAME_IN(pl, 7.7f, 8.3f)) {
            SndCall(1, 0x34, &p->world, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 14.7f, 15.3f)) {
            SndCall(1, 0x34, &p->world, 0, 0, pl);
            SndCall(1, 7, &p->world, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 142.7f, 143.3f)) {
            SndCall(1, 0x4A, &p->world, 0, 0, pl);
        }
        MotionMove(pl, 0);
        break;
    case 2:
        pl->pos.x = 54.86f;
        pl->pos.y = 0.0f;
        pl->pos.z = 704.51f;
        PSMTXMultVec(pl->pEmCatch->mat, &pl->pos, &pl->pos);
        pl->ang.y = pl->pEmCatch->ang.y + PI;
        pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x122), 0, 0, 1, 0);
        em39CliffObj.p = ObjMgr.create(cObjMgr::ID_PL_WEAPON);
        if (em39CliffObj.p) {
            em39CliffObj.p->modelInit(PL_ARC_PTR(pl->subArc, 0x129), PL_ARC_PTR(pl->subArc, 0x128));
            em39CliffObj.p->atari.m_flag &= 0xFCFF;
            em39CliffObj.p->pParts->pParent = pPL->getPartsPtr(0xA);
            em39CliffObj.p->LightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            ((cObjWep*) em39CliffObj.p)->m_pParent = pPL;
            em39CliffObj.p->setNoSuspend(1);
        }
        pl->Wep->setTrans(0, 0);
        pl->r_no_2++;
    case 3:
        if (PL_FRAME_IN(pl, 18.7f, 19.3f)) {
            SndCall(1, 0x10, &p->world, 0, 0, pl);
        }
        if (MotionMove(pl, 0)) {
            pl->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x123), 0, 0, 1, 0);
        pl->Wep->setTrans(1, 0);
        if (em39CliffObj.p) {
            ObjMgr.destroy(em39CliffObj.p);
            em39CliffObj.p = 0;
        }
        SndCall(1, 7, &p->world, 0, 0, pl);
        pl->r_no_2++;
    case 5:
        if (PL_FRAME_IN(pl, 8.7f, 9.3f)) {
            SndCall(1, 0x4F, &p->world, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 24.7f, 25.3f)) {
            SndCall(5, 0xD, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 30.7f, 31.3f)) {
            SndCall(5, 0xE, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0)) {
            AtariOn(&pl->atari, 0x300);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Routine 2: damage reactions (r_no_1: Dm_Normal, Dm_Head, Dm_Blow for the first battle, Dm_T_Head,
// Dm_T_Down, Dm_T_DownHead for the final form); Be_flg 8 keeps em39DmCk from restarting one.
static void em39_R0_Damage(cEm39* em)
{
    EM39_WK(em)->Be_flg |= 8;
    Em39_R1_dmg_tbl[em->r_no_1](em);
}

// Damage entry: drop the hanging object and the grenade in hand, delete the effects, voice + speech.
#define EM39_DM_DROP(em, w, voice, speech)                                                          \
    if ((w)->pCap && (w)->Cap_hp == 0) {                                                           \
        Mtx m;                                                                                     \
        Vec v;                                                                                     \
                                                                                                   \
        PSMTXRotRad(m, 'y', GetXZAngle(&pPL->pos, &(em)->pos));                                    \
        v.x = 0.0f;                                                                                \
        v.y = 40.0f;                                                                               \
        v.z = -50.0f;                                                                              \
        PSMTXMultVecSR(m, &v, &v);                                                                 \
        ((cObj12*) (w)->pCap)->setFall(&v, 2);                                                   \
        (w)->pCap = 0;                                                                           \
    }                                                                                              \
    if ((w)->pBomb) {                                                                           \
        Vec spd;                                                                                   \
                                                                                                   \
        spd.x = 0.0f;                                                                              \
        spd.y = 10.0f;                                                                             \
        spd.z = 50.0f;                                                                             \
        PSMTXMultVecSR((em)->mat, &spd, &spd);                                                     \
        EM39_GRENADE_THROW(em, w, spd);                                                            \
    }                                                                                              \
    EffectEspDelete(0, (w)->EffKindIdArrow, em, 0);                                              \
    EffectEspgenDelete(0, (w)->EffKindIdArrow, em);                                              \
    EffectEfmDelete(0, (w)->EffKindIdArrow, em);                                                 \
    EM39_K4_EFF_DELETE(em, w);                                                                     \
    em39SetVoice(em, voice);                                                                       \
    switch ((u8) (Rnd() % 3)) {                                                                    \
    case 0:                                                                                        \
    default:                                                                                       \
        em39SetSpeech(em, speech, 0x2E);                                                           \
        break;                                                                                     \
    case 1:                                                                                        \
        em39SetSpeech(em, speech, 0x2F);                                                           \
        break;                                                                                     \
    case 2:                                                                                        \
        em39SetSpeech(em, speech, 0x30);                                                           \
        break;                                                                                     \
    }                                                                                              \
    (w)->No_fire_timer = 0;                                                                                 \
    AtariOn(&(em)->atari, 0x300);

// Damage recovery decision on the return-to-idle frame (Motion.Seq_old.Free bit 2).
#define EM39_DM_RECOVER(em, w)                                                                      \
    {                                                                                              \
        int lim;                                                                                   \
                                                                                                   \
        if ((w)->pGotoPoint && (w)->pGotoPoint->sub == 0) {                                        \
            EmRoutineSet(em, 1, 6, 0, 0);                                                          \
            break;                                                                                 \
        }                                                                                          \
        if (em39GotoCk(em)) {                                                                      \
            break;                                                                                 \
        }                                                                                          \
        switch ((w)->Locate) {                                                                       \
        case 0:                                                                                    \
        case 3:                                                                                    \
        default:                                                                                   \
            lim = 1000;                                                                            \
            break;                                                                                 \
        case 1:                                                                                    \
            lim = 500;                                                                             \
            break;                                                                                 \
        case 2:                                                                                    \
            lim = 500;                                                                             \
            break;                                                                                 \
        case 4:                                                                                    \
            lim = 500;                                                                             \
            break;                                                                                 \
        case 5:                                                                                    \
            lim = 0;                                                                               \
            break;                                                                                 \
        }                                                                                          \
        if ((w)->Flash_damage >= lim) {                                                                    \
            EmRoutineSet(em, 1, 0x25, 0, 0);                                                       \
            break;                                                                                 \
        }                                                                                          \
        if (em39LockCk(em) || (em)->l_pl < 4000000.0f) {                                       \
            if ((u8) (Rnd() % 10) > 6 && em39JumpUpCk3(em)) {                                      \
                break;                                                                             \
            }                                                                                      \
            if ((em)->l_pl < 25000000.0f) {                                                     \
                EmRoutineSet(em, 1, 0xF, 0, 0);                                                    \
            } else {                                                                               \
                EmRoutineSet(em, 1, 0xD, 0, 0);                                                    \
            }                                                                                      \
            break;                                                                                 \
        }                                                                                          \
    }

// Routine 2/0 (first battle): the body flinch (one of two front flinches or the back one by the
// hit side), the beret knocked a step looser (Cap_hp) and dropped at 0, any grenade in hand tossed,
// effects cleared, pain voice and a line (EM39_DM_DROP). Invulnerable until motion event bit 2,
// where EM39_DM_RECOVER decides: back to the crouch at a perch, a goto, a flash-grenade exit when
// the phase's damage limit is reached, or a step / escape when aimed at or within 2 m. On event bit
// 0 half the time an early exit (crouch / goto / turn / back flip / walk); else Walk from frame 0xA.
static void em39_R1_Dm_Normal(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x10;
    w->Total_damage = 0;
    switch (em->r_no_2) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI)) < 1.5707964f) {
            if ((u8) (Rnd() % 10) > 5) {
                MotionSetCore(em, MOTION(em), ARC(0x56), ARC(0x57), 3, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x58), ARC(0x59), 3, 1, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x5C), ARC(0x5D), 3, 1, 0);
        }
        if (w->Cap_hp) {
            w->Cap_hp--;
        }
        EM39_DM_DROP(em, w, 6, 0x1E);
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 8, 0, 0xA);
        }
        if (em->Motion.Seq_old.Free & 4) {
            EM39_DM_RECOVER(em, w);
        } else {
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                int rtn;

                if (w->pGotoPoint && w->pGotoPoint->sub == 0) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
                rtn = em39GotoCk(em);
                if (rtn) {
                    break;
                }
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, rtn, rtn);
                } else if (em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, rtn, 1);
                } else {
                    EmRoutineSet(em, 1, 8, rtn, rtn);
                }
            }
        }
        break;
    }
}

// Routine 2/1 (first battle): the head-shot flinch, with the same drop / recovery handling as
// em39_R1_Dm_Normal.
static void em39_R1_Dm_Head(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->Be_flg |= 0x10;
    w->Total_damage = 0;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x60), ARC(0x61), 3, 1, 0);
        if (w->Cap_hp) {
            w->Cap_hp--;
        }
        EM39_DM_DROP(em, w, 9, 0x3C);
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 8, 0, 0xA);
        }
        if (em->Motion.Seq_old.Free & 4) {
            EM39_DM_RECOVER(em, w);
        } else {
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                int rtn;

                if (w->pGotoPoint && w->pGotoPoint->sub == 0) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
                rtn = em39GotoCk(em);
                if (rtn) {
                    break;
                }
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, rtn, rtn);
                } else if (em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, rtn, 1);
                } else {
                    EmRoutineSet(em, 1, 8, rtn, rtn);
                }
            }
        }
        break;
    }
}

// Routine 2/2 (first battle): blown off his feet by a grenade (forwards or backwards by the hit
// side, Be_flg 0x200 remembers which; the beret always comes off), then the matching get-up, with
// the recovery handling of em39_R1_Dm_Normal.
static void em39_R1_Dm_Blow(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int rtn;

    w->Be_flg |= 0x10;
    w->Total_damage = 0;
    switch (em->r_no_2) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x5A), ARC(0x5B), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x15, 0, ESP_CORE_KIND_NONE, em, 0);
            w->Be_flg |= 0x200;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x5E), ARC(0x5F), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x16, 0, ESP_CORE_KIND_NONE, em, 0);
            w->Be_flg &= ~0x200;
        }
        if (w->Cap_hp) {
            w->Cap_hp = 0;
        }
        EM39_DM_DROP(em, w, 9, 0x5A);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0) || (em->hp > 0 && (em->Motion.Seq_old.Free & 4))) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (w->Be_flg & 0x200) {
            MotionSetCore(em, MOTION(em), ARC(0x77), ARC(0x78), 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM39, 0x2B, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x79), ARC(0x7A), 3, 1, 0);
        }
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->pGotoPoint && w->pGotoPoint->sub == 0) {
                EmRoutineSet(em, 1, 6, 0, 0);
                break;
            }
            rtn = em39GotoCk(em);
            if (rtn) {
                break;
            }
            EmRoutineSet(em, 1, 8, rtn, 0xA);
        }
        if (em->Motion.Seq_old.Free & 4) {
            EM39_DM_RECOVER(em, w);
        } else {
            w->Be_flg |= 0x100;
        }
        if (em->Motion.Seq_old.Free & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                if (w->Go_rot > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else if (w->Dash_wait == 0) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
            }
        }
        break;
    }
}

// Tower-form damage: the recovery choice by difficulty / lock-on, or the next attack.
#define EM39_DM_T_RECOVER(em, w, end)                                                               \
    if (end) {                                                                                     \
        int rtn = em39AtkRtnCk(em);                                                                \
                                                                                                   \
        if (rtn) {                                                                                 \
            break;                                                                                 \
        }                                                                                          \
        if ((w)->Dash_wait == 0) {                                                                      \
            EmRoutineSet(em, 1, 9, rtn, rtn);                                                      \
        } else {                                                                                   \
            EmRoutineSet(em, 1, 8, rtn, rtn);                                                      \
        }                                                                                          \
    } else if ((em)->Motion.Seq_old.Free & 4) {                                                             \
        if (pG->Game_level <= 3) {                                                                      \
            EmRoutineSet(em, 1, 0xE, end, 1);                                                      \
        } else if (em39LockCk(em)) {                                                               \
            if (pG->Game_level <= 9) {                                                                  \
                EmRoutineSet(em, 1, 0xE, end, 1);                                                  \
            } else {                                                                               \
                EmRoutineSet(em, 1, 0xF, end, end);                                                \
            }                                                                                      \
        } else {                                                                                   \
            int rtn = em39AtkRtnCk(em);                                                            \
                                                                                                   \
            if (rtn) {                                                                             \
                break;                                                                             \
            }                                                                                      \
            if ((w)->Dash_wait == 0) {                                                                  \
                EmRoutineSet(em, 1, 9, rtn, rtn);                                                  \
            } else {                                                                               \
                EmRoutineSet(em, 1, 8, rtn, rtn);                                                  \
            }                                                                                      \
        }                                                                                          \
    } else if ((em)->Motion.Seq_old.Free & 1) {                                                             \
        (w)->Arm_rno = 8;                                                                             \
    }

// Routine 2/3 (final form): the head-shot flinch, invulnerable (Be_flg 0x110), effects cleared,
// pain voice. On motion event bit 2 (EM39_DM_T_RECOVER) a back flip on the harder ranks or when
// aimed at (a step otherwise), else the melee selection or the run / walk; the arm pose returns to
// 8 on event bit 0.
static void em39_R1_Dm_T_Head(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st;
    int end;

    w->Be_flg |= 0x110;
    w->Total_damage = 0;
    st = em->r_no_2;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x60), ARC(0x61), 3, 1, 0);
        EM39_K4_EFF_DELETE(em, w);
        em39SetVoice(em, 9);
        w->No_fire_timer = st;
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        EM39_DM_T_RECOVER(em, w, end);
        break;
    }
}

// Routine 2/4 (final form): knocked to a knee after 200 accumulated damage: the arm shields him
// (Be_flg 0x1000 while the motion runs; 0x10000 until motion event bit 4 marks the head exposed for
// Dm_T_DownHead), then the recovery of EM39_DM_T_RECOVER.
static void em39_R1_Dm_T_Down(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st;
    int end;

    w->Be_flg |= 0x10;
    w->Total_damage = 0;
    st = em->r_no_2;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF7), ARC(0xF8), 3, 1, 0);
        w->Arm_rno = st;
        EM39_K4_EFF_DELETE(em, w);
        em39SetVoice(em, 6);
        w->No_fire_timer = st;
        w->Timer = 10;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        if (end == 0) {
            w->Be_flg |= 0x1000;
            if (!(em->Motion.Seq_old.Free & 0x10)) {
                w->Be_flg |= 0x10000;
            }
        }
        EM39_DM_T_RECOVER(em, w, end);
        break;
    }
}

// Routine 2/5 (final form): the head shot while kneeling: the big stagger with its effect and
// voice, invulnerable, then the same recovery choice as em39_R1_Dm_T_Head (run / walk at the end).
static void em39_R1_Dm_T_DownHead(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st;
    int end;

    w->Be_flg |= 0x110;
    w->Total_damage = 0;
    st = em->r_no_2;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF9), ARC(0xFA), 3, 1, 0);
        EM39_K4_EFF_DELETE(em, w);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x42, 0, ESP_CORE_KIND_NONE, em, (void*) st);
        em39SetVoice(em, 9);
        w->Timer = 10;
        w->Arm_rno = st;
        w->No_fire_timer = st;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        if (end) {
            if (w->Dash_wait == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->Motion.Seq_old.Free & 4) {
            if (pG->Game_level <= 3) {
                EmRoutineSet(em, 1, 0xE, end, 1);
            } else if (em39LockCk(em)) {
                if (pG->Game_level <= 9) {
                    EmRoutineSet(em, 1, 0xE, end, 1);
                } else {
                    EmRoutineSet(em, 1, 0xF, end, end);
                }
            } else {
                int rtn = em39AtkRtnCk(em);

                if (rtn) {
                    break;
                }
                if (w->Dash_wait == 0) {
                    EmRoutineSet(em, 1, 9, rtn, rtn);
                } else {
                    EmRoutineSet(em, 1, 8, rtn, rtn);
                }
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            w->Arm_rno = 8;
        }
        break;
    }
}

// Routine 3: death (r_no_1: 0 the scripted Die_Normal, 1 Die_Flash from the damage check).
static void em39_R0_Die(cEm39* em)
{
    EM39_WK(em)->Be_flg |= 8;
    Em39_R1_die_tbl[em->r_no_1](em);
}

// Routine 3/0 (setDie, the final battle): placed at the death spot on the arena, the death motion
// with its effect, the death model swapped in (em39DieModelSet) and the death stream; at the end it
// holds the last frame, drops the item and the stream fades out.
static void em39_R1_Die_Normal(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->r_no_2;

    switch (st) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = 6640.0f;
        em->pos.y = 12050.0f;
        em->pos.z = -14796.0f;
        em->ang.y = 1.57f;
        MotionSetCore(em, MOTION(em), ARC(0xE7), 0, 0, 0x201, 0);
        EM39_K4_EFF_DELETE(em, w);
        EstSet(em, -1, 0, 0, EFF_EM39, 0x3F, 0, w->EffKindId, em, (void*) st);
        w->Arm_rno = 0x10;
        em39DieModelSet(em);
        w->Str_seid = SndStrReq(1, 0xE7, 0x80000003, 0, 0, 0.0f);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2: {
        MotionData* mot = (MotionData*) ARC(0xE7);

        MotionSetCore(em, MOTION(em), mot, 0, 0, 0x100, (u16) ((mot->maxFrame & 0x3FFF) - 1));
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, EFF_EM39, 0x46, 0, ESP_CORE_KIND_NONE, 0, 0);
        w->Arm_rno = 0x12;
        SndStrReq(w->Str_seid, 8, 0, 0);
        em->r_no_2++;
    }
    case 3:
        MotionMove(em, 0);
        break;
    }
}

// Routine 3/1 (the first battle's "death": HP run out): throws a flash grenade (on motion event
// bit 0) and vanishes: item dropped, collision off, unlockable and hidden; invulnerable throughout.
static void em39_R1_Die_Flash(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->r_no_2;
    Vec pos;
    Vec rot;
    Vec spd;
    int end;

    w->Be_flg |= 0x130;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x9B), ARC(0x9C), 6, 1, 0);
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pFlash = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6F), &pos, &rot, 0);
        if (w->pFlash) {
            w->pFlash->setParent(em, 0xA, 0);
        }
        em39WepSet(em, 2);
        w->Timer = st;
        em->r_no_2++;
    case 1:
        end = MotionMove(em, 0);
        if (end == 0) {
            if ((em->Motion.Seq_old.Free & 1) && w->pFlash) {
                spd.x = 0.0f;
                spd.y = 100.0f;
                spd.z = 150.0f;
                PSMTXMultVecSR(em->mat, &spd, &spd);
                w->Timer = 28;
                w->pFlash->setFlashThrow(&spd, 28);
                w->pFlash = (cEmWep*) end;
            }
            if (w->Timer) {
                w->Timer--;
                if (w->Timer) {
                    break;
                }
                em->clearStatus(EM_STATUS_ACTIVE);
                em->setStatus(EM_STATUS_ITEMSET);
                EmSetDropItem(em);
                AtariOff(&em->atari, 0xFCFF);
                em->be_flag |= 0x10000000;
                em->setStatus(EM_STATUS_LOCKOFF);
                em->be_flag &= ~2;
                em->r_no_2++;
                break;
            } else {
                break;
            }
        }
        AtariOff(&em->atari, 0xFCFF);
        em->be_flag |= 0x10000000;
        em->setStatus(EM_STATUS_LOCKOFF);
        em->be_flag &= ~2;
        em->r_no_2++;
        break;
    }
    em39HandSet(em, 0);
}

// ---- HELPERS ----
// Per-frame routing while alive: the route to the player (`up` when he is a floor above; the
// type 0 / 1 distance branches are dead code), Pl_dir / Pl_rot (zero during init), the
// line-of-sight flag (Be_flg bit 0, clear of 0x4000-class obstacles at head height) and the target
// copy; a goto (Goto_mode) replaces the target with the route to Goto_pos. Debug_flg[0] 0x4000
// draws the target line.
void em39RouteCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec plPos;
    Vec a;
    Vec b;
    int up;

    if (em->hp <= 0) {
        return;
    }
    if (em->type == 2) {
        plPos = pPL->pos;
    } else {
        if (SQRTF(em->l_pl) < 5000.0f) {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (dy < 0.0f) {
                up = 0;
            }
            a = pPL->pos;
            plPos = a;
        } else {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (dy < 0.0f) {
                up = 0;
            }
            a = pPL->pos;
            plPos = a;
        }
    }
    // COMPILER-DIFF: candidate #17 (global-alloc priority order) -- the target allocates the four
    // callee-saved pseudos as &em->pos r27 > &a r26 > pPL high r25 > PI high r24; ours ranks &a (4 refs/74)
    // above &em->pos (4/106) and the PI high (3/178) above the pPL high (5/1096). Codeless asms whose
    // "m" operands go through the PRE'd pseudos add refs without code: three `pPL` mentions in three
    // blocks (8 refs: 3*8/1192 > 3/182) and two `&em->pos` member reads after the second Muku (6 refs:
    // 2*6/108 > 2*4/76). A second mention in the same block would be a fresh `lis` (cse keeps a `high`).
    up = 0;
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        up = 1;
        asm("" : "=m"(w->Atk_ck) : "m"(pPL)); // COMPILER-DIFF: candidate #17
    }
    RouteCkToPos(em, &plPos, &w->Pl_pos, up, 0);
    w->Pl_dir = Muku(&em->pos, &w->Pl_pos, em->ang.y, PI);
    w->Pl_rot = fabsf(w->Pl_dir);
    if (em->r_no_0 == 0) {
        w->Pl_dir = 0.0f;
        w->Pl_rot = 0.0f;
        em->l_pl = 100000000.0f;
        asm("" : "=m"(w->Atk_ck) : "m"(pPL)); // COMPILER-DIFF: candidate #17
    }
    a.x = em->pos.x;
    a.y = em->pos.y + 1300.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 1300.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000) == 0) {
        w->Be_flg |= 1;
    }
    w->Go_pos = w->Pl_pos;
    w->Go_dir = w->Pl_dir;
    w->Go_rot = w->Pl_rot;
    w->L_go = em->l_pl;
    w->pTarget = pPL;
    w->Be_flg &= ~4;
    if (w->Goto_mode) {
        up = 0;
        if (w->Goto_pos.y > em->pos.y + 1000.0f) {
            up = 1;
            asm("" : "=m"(w->Atk_ck) : "m"(pPL)); // COMPILER-DIFF: candidate #17
        }
        RouteCkToPos(em, &w->Goto_pos, &w->Go_pos, up, 0);
        w->Go_dir = Muku(&em->pos, &w->Go_pos, em->ang.y, PI);
        w->Go_rot = fabsf(w->Go_dir);
        w->L_go = (em->pos.x - w->Goto_pos.x) * (em->pos.x - w->Goto_pos.x) + (em->pos.z - w->Goto_pos.z) * (em->pos.z - w->Goto_pos.z);
        {
            Vec* pep = &em->pos; // COMPILER-DIFF: candidate #17 -- two refs for the PRE'd &em->pos
            asm("" : "=m"(w->Act_ck) : "m"(pep->y), "m"(pep->z));
        }
    }
    if (DbgFlagChk(pG, DBG_RTP_DISP)) {
        Vec c = em->pos;

        c.y += 250.0f;
        Draw_line3d(&c, &w->Go_pos, 0xFFFFFF40, 0);
    }
}


// Neck: turn the head (parts 3 inv_offset) towards the player while flags bit 4 is set, else relax.
void em39NeckMove(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    cModel* p = em->getPartsPtr(4);
    cModel* plp = pPL->getPartsPtr(4);
    Vec a;
    Vec d;

    a.x = 0.0f;
    a.y = 50.0f;
    a.z = 0.0f;
    PSMTXMultVec(plp->mat, &a, &a);
    if (w->Be_flg & 0x10) {
        f32 ang = Muku(&em->pos, &a, em->ang.y, 1.0471976f);
        f32 len;
        f32 pitch;

        w->Neck_dir_y = w->Neck_dir_y * 0.9f + ang * 0.1f;
        PSVECSubtract(&a, &p->world, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        pitch = -atan2f(d.y, len);
        if (pitch > 0.7853982f) {
            pitch = 0.7853982f;
        }
        if (pitch < -0.7853982f) {
            pitch = -0.7853982f;
        }
        w->Neck_dir_x = w->Neck_dir_x * 0.9f + pitch * 0.1f;
    } else {
        w->Neck_dir_x *= 0.9f;
        w->Neck_dir_y *= 0.9f;
    }
    p = em->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->inv_offset.x = w->Neck_dir_x;
    ((cParts*) p)->inv_offset.y = w->Neck_dir_y;
    ((cParts*) p)->inv_offset.z = 0.0f;
}

// Waist aim hook called from move() and before each gun hit; empty in the shipped game (only the
// local and its constant pool survive).
void em39WaistMove(cEm39* em)
{
    Vec a;
}

// The original keeps the three constant-pool words (0.9, 0.020000001, 1.0) of em39WaistMove's
// dead-stripped body; ours drops unreferenced pool entries (mark_constant_pool).
// COMPILER-DIFF: candidate #10 (unreferenced constant-pool entries kept).
asm(".section .rodata\n\t.long 0x3f666666, 0x3ca3d70b, 0x3f800000\n\t.text");

// Laser marker: from the machine gun muzzle (x8B4 == 3) or the bow (x8B4 == 4) to the target.
void em39MarkerMove(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    cModel* p;
    Vec from;
    Vec to;

    if (!(w->Be_flg & 0x40)) {
        return;
    }
    switch (w->Wep_type) {
    case 3:
        p = em->getPartsPtr(0xA);
        from.x = 228.0f;
        from.y = -24.0f;
        from.z = 29.0f;
        to.x = -50000.0f;
        to.y = 0.0f;
        to.z = 0.0f;
        break;
    case 4:
        if (w->pBow == 0) {
            return;
        }
        p = w->pBow->getPartsPtr(0);
        from.x = 0.0f;
        from.y = 0.0f;
        from.z = 0.0f;
        to.x = 50000.0f;
        to.y = 0.0f;
        to.z = 0.0f;
        break;
    case 0:
    case 1:
    case 2:
    default:
        return;
    }
    PSMTXMultVec(p->mat, &from, &from);
    PSMTXMultVecSR(p->mat, &to, &to);
    PSVECAdd(&from, &to, &to);
    GetWepTargetPos(&from, &to, 1, 0, 0, 0);
    EstSet(0, -1, &to, 0, EFF_CORE, 0x50, 0, ESP_CORE_KIND_NONE, 0, 0);
    if (pG->shooting_mode == 1) {
        Draw_line3d_222(&to, &from, em39MarkerCol0, 1);
    } else {
        Draw_line3d_222(&to, &from, em39MarkerCol1, 1);
    }
}

// Machine gun shot: a line from the muzzle with a random spread; hits the player or leaves a spark.
int em39GunHitCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec from;
    Vec to;
    EmAtkInfo atk;
    Vec hit;
    Vec nrm;
    Vec a;
    Vec rot;
    Vec b;
    cEm* target;
    cModel* p;
    s16 hp;
    f32 len;

    if (w->pMachineGun) {
        EstSet(w->pMachineGun, -1, 0, 0, EFF_EM39, 3, 0, ESP_CORE_KIND_NONE, w->pMachineGun, 0);
    }
    from.x = 228.0f;
    from.y = -24.0f;
    from.z = 29.0f;
    to.x = -50000.0f;
    to.y = fRand1_1() * 1000.0f;
    to.z = fRand1_1() * 1000.0f;
    atk = em39_gun_atk_info;
    p = em->getPartsPtr(0xA);
    PSMTXMultVec(p->mat, &from, &from);
    PSMTXMultVecSR(p->mat, &to, &to);
    PSVECAdd(&from, &to, &to);
    if (pG->debug_mode == 7) {
        Draw_line3d(&from, &to, 0xFFFFFFFF, 0);
    }
    hp = em->hp;
    em->hp = 0;
    PlWepHitCheck2(0, &from, &to, 0x1B, 3, 6000.0f);
    em->hp = hp;
    SndCall(8, 0xC, &em->pos, em->id, 0, em);
    target = EmAtkLineHitCk(&from, &to, &hit, &nrm, 0);
    if (target) {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        SndCall(6, 0x15, &pPL->pos, 0, 0, pPL);
        QuakeExec(0, 0, 5, 22.0f, 2);
        EmPlBloodSet2(em, &from, 1, 0x2F, 0x2C);
        EmAtkSetDamagePL(target, &atk, &from, &to);
        w->Atk_ck = 1;
        return 1;
    }
    len = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
    rot.x = -atan2f(nrm.y, len);
    rot.y = atan2f(nrm.x, nrm.z);
    rot.z = 0.0f;
    PSVECScale(&nrm, &a, 30.0f);
    PSVECAdd(&hit, &a, &hit);
    EstSet(0, -1, &hit, &rot, EFF_EM39, 4, 0, ESP_CORE_KIND_NONE, target, (void*) target);
    PSVECSubtract(&hit, &from, &b);
    EspSetGatling(from, b);
    SndCall(6, 0xA, &hit, 0, 0, 0);
    return 0;
}

// The action-button callback and the player damage routine are static: they are emitted here, where
// the original had them (after em39GunHitCk).
static void em39ActOn(cEm39* em)
{
    EM39_WK(em)->Act_ck = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback of the knife slash (EM39_ATK_SIDE): the sideways stagger (mirrored by
// r_no_3, Ada's own motion) with the pain face and sound, 10 frames of stun; ends with the motion.
static void plemDmSide(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0: {
        int flag = 1;

        if (pl->r_no_3) {
            flag = 0x41;
        }
        if (pG->pl_type == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x12D), PL_ARC_PTR(pl->subArc, 0x12E), 3, flag, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x10E), PL_ARC_PTR(pl->subArc, 0x10F), 3, flag, 0);
        }
        PlSetFace(1);
        pl->dmg.m_Timer = 0xA;
        PlSetDamageSe(0);
        pl->r_no_2++;
    }
    case 1:
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Ejected cartridge (obj10) from the machine gun.
void em39SetCartridge(cEm39* em)
{
    cModel* p = em->getPartsPtr(0xA);
    cObj* obj;
    Vec pos;
    Vec rot;
    Vec spd;

    pos.x = -90.74f;
    pos.y = -17.97f;
    pos.z = 100.98f;
    PSMTXMultVec(p->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 60.0f;
    spd.z = 60.0f;
    spd.x += fRand1_1() * 15.0f;
    spd.y += fRand1_1() * 15.0f;
    spd.z += fRand1_1() * 15.0f;
    PSMTXMultVecSR(p->mat, &spd, &spd);
    obj = SetObj10(ARC(0x2D), ARC(0x2E), &pos, &rot, &spd, 10.0f, 50.0f, 30, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 0x63;
    }
}

// The player aims at the enemy: laser on it, or the body origin inside the gun's aiming box.
int em39LockCk(cEm39* em)
{
    Mtx inv;
    Vec p;

    if (pG->Game_level <= 1) {
        return 0;
    }
    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 6) {
        return 0;
    }
    if (pG->weapon_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (pPL->Wep->m_pWep->m_SightEm && pPL->Wep->m_pWep->m_SightEm == em) {
        return 1;
    }
    if (em->l_pl > 144000000.0f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(0xA)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->world, &p);
    if (p.x > 0.0f) {
        return 0;
    }
    if (p.z > 800.0f) {
        return 0;
    }
    if (p.z < -800.0f) {
        return 0;
    }
    if (p.y > 800.0f) {
        return 0;
    }
    if (p.y < -800.0f) {
        return 0;
    }
    return 1;
}

// 1 when the player's loaded gun (not the rocket launcher, Game_level above 1) points at the
// boss's head: a point 15 cm above the head part lies within a 30 cm box in front of the weapon hand.
int em39HeadLockCk(cEm39* em)
{
    Mtx inv;
    Vec p;
    cModel* head;

    if (pG->Game_level <= 1) {
        return 0;
    }
    if (pG->weapon_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(0xA)->mat, inv);
    head = em->getPartsPtr(4);
    p.x = 0.0f;
    p.y = 150.0f;
    p.z = 0.0f;
    PSMTXMultVec(head->mat, &p, &p);
    PSMTXMultVec(inv, &p, &p);
    if (p.x > 0.0f) {
        return 0;
    }
    if (p.z > 150.0f) {
        return 0;
    }
    if (p.z < -150.0f) {
        return 0;
    }
    if (p.y > 150.0f) {
        return 0;
    }
    if (p.y < -150.0f) {
        return 0;
    }
    return 1;
}

// Tests attack `no` swept from part `parts`' previous to its current world position (em39AtkCk2).
int em39AtkCk(cEm39* em, int no, int parts)
{
    cModel* p = em->getPartsPtr(parts);

    return em39AtkCk2(em, no, &p->world, &p->world_old);
}

// Side damage on the player: turn him towards the enemy and pick the left / right motion.
#define EM39_ATK_SIDE(em, v)                                                                        \
    SetPlDamage((em), plemDmSide);                                                           \
    v = Muku(&pPL->pos, &(em)->pos, pPL->ang.y, PI);                                               \
    v = fabsf(v);                                                                                  \
    if (v < 1.5707964f) {                                                                          \
        (pPL->ang.y = pPL->ang.y + Muku(&pPL->pos, &(em)->pos, pPL->ang.y, PI));                \
        pPL->r_no_3 = 0;                                                                              \
    } else {                                                                                       \
        (pPL->ang.y = pPL->ang.y + Muku(&(em)->pos, &pPL->pos, pPL->ang.y, PI));                \
        pPL->r_no_3 = 1;                                                                              \
    }

#define EM39_ATK_KNOCK(em)                                                                          \
    pPL->ang.y = GetXZAngle(&pPL->pos, &(em)->pos);                                                \
    PlSetDamage(PL_DM_AUTO, 0, 0);

// Attack `no` (1-based into em39_atk_tbl) swept from `b` to `a` against the player (hit bit 0) and
// partner (bit 1), once per attack (Atk_ck). A player hit bleeds with the hit sound and, by attack:
// the knife (2) only cuts; the backhand (5) and the high kick (8) stagger him sideways (plemDmSide)
// or knock a dying player down; the leaping slam (7) knocks him down (plem39_Stamp; Ada is just
// knocked back); 9 knocks back; 0xA only bleeds. Camera shake, a taunt when hit from behind in the
// first battle, rumble. Returns 1 on a hit.
int em39AtkCk2(cEm39* em, int no, Vec* a, Vec* b)
{
    Em39Work* w = EM39_WK(em);
    int hit = w->Atk_ck;
    u32 res;
    f32 ang;

    if (hit) {
        return 0;
    }
    res = EmAtkHitCk(&em39_atk_tbl[no - 1], a, b, 0); // the target addresses .data+0x298 = the entry before the table
    if (res) {
        if (res & 1) {
            switch ((u32) no) {
            case 2:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x2C);
                SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
                break;
            case 5:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
                if ((s16) pG->pl_life <= 0) {
                    EM39_ATK_KNOCK(em);
                } else {
                    EM39_ATK_SIDE(em, ang);
                }
                break;
            case 7:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
                if (pG->pl_type == 2) {
                    EM39_ATK_KNOCK(em);
                } else {
                    SetPlDamage(em, plem39_Stamp);
                }
                break;
            case 8:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
                if ((s16) pG->pl_life <= 0) {
                    EM39_ATK_KNOCK(em);
                } else {
                    EM39_ATK_SIDE(em, ang);
                }
                break;
            case 9:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
                EM39_ATK_KNOCK(em);
                break;
            case 0xA:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
                break;
            }
            w->Atk_ck = 1;
        }
        if (res & 2) {
            EmSubBloodSet(em, a, 1, 0xFF, 0xFF);
            w->Atk_ck = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        if (em->type != 2) {
            ang = Muku(&pPL->pos, &em->pos, pPL->ang.y, PI);
            ang = fabsf(ang);
            if (!(ang < 2.3561945f)) {
                if ((u8) (Rnd() % 10) > 4) {
                    em39SetSpeech(em, 0x3C, 0x21);
                } else {
                    em39SetSpeech(em, 0x3C, 0x22);
                }
            }
        }
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        return 1;
    }
    return 0;
}

// Flags bit 17 when the player stands near one of the two tower bases.
void em39PLNearTowerCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a = { -8462.0f, -3150.0f, -7937.0f };
    Vec b = { 7111.0f, -3150.0f, -8987.0f };

    {
        f32 d;
        d = (pPL->pos.x - a.x) * (pPL->pos.x - a.x) + (pPL->pos.y - a.y) * (pPL->pos.y - a.y) + (pPL->pos.z - a.z) * (pPL->pos.z - a.z);
        if (d < 4000000.0f) {
            w->Be_flg |= 0x20000;
        }
        d = (pPL->pos.x - b.x) * (pPL->pos.x - b.x) + (pPL->pos.y - b.y) * (pPL->pos.y - b.y) + (pPL->pos.z - b.z) * (pPL->pos.z - b.z);
        if (d < 4000000.0f) {
            w->Be_flg |= 0x20000;
        }
    }
}

// Jump down: look for a ledge in one of the four directions (front / back / right / left), or the
// fixed drop point of the second area.
#define EM39_JUMPDOWN_PROBE(em, w, a, b, hit, ax, az, bx, bz, res, sgn, rtnFE, rtnFF)                \
    a.x = ax;                                                                                      \
    a.y = 500.0f;                                                                                  \
    a.z = az;                                                                                      \
    b.x = bx;                                                                                      \
    b.y = 500.0f;                                                                                  \
    b.z = bz;                                                                                      \
    PSMTXMultVec((em)->mat, &a, &a);                                                               \
    PSMTXMultVec((em)->mat, &b, &b);                                                               \
    res = SatMgr.hitCheck(&a, &b, 0, &hit, 0, 0) & 0x142810;                                       \
    if (res) {                                                                                     \
        (w)->Target_dir = atan2f(sgn hit.x, sgn hit.z);                                               \
        EmRoutineSet(em, 1, 0x13, rtnFE, rtnFF);                                                   \
        return 1;                                                                                  \
    }

// The drop check of the first battle (not the final form): every 4th frame a floor more than 35 cm
// below under both feet starts the jump down (1/0x13) straight ahead. Otherwise (outside the 2 m
// no-drop spot; without `force` only when stuck, HoseiCnt at 5 mod 10, and heading for the target)
// four wall probes 1 m ahead / behind / to the sides for a ledge-type collision (EM39_JUMPDOWN_PROBE,
// flags 0x142810) start the drop facing the ledge. Returns 1 when started.
int em39JumpDownCk(cEm39* em, int force)
{
    Em39Work* w = EM39_WK(em);
    Vec drop = { 8941.0f, 2050.0f, -9080.0f };
    Vec a;
    Vec b;
    Vec hit;
    u32 res0;
    u32 res1;
    u32 res2;

    // Every `return 0` is written before the last probe's: jump2 keeps the LAST copy as the cross-jump survivor.
    if (em->type == 2) {
        return 0;
    }
    if ((pG->Frame_cnt & 3) == (em->emset_no & 3)) {
        if (SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0) < em->pos.y - 350.0f) {
            a = em->pos;
            a.z += 100.0f;
            a.x += 100.0f;
            if (SatMgr.getFloor(&a, 0, 600.0f, 100000.0f, 0) < em->pos.y - 350.0f) {
                w->Target_dir = em->ang.y;
                EmRoutineSet(em, 1, 0x13, 0, 0);
                return 1;
            }
        }
    }
    if ((em->pos.x - drop.x) * (em->pos.x - drop.x) + (em->pos.z - drop.z) * (em->pos.z - drop.z) < 4000000.0f) {
        return 0;
    }
    if (force == 0) {
        f32 ang;

        if ((u32) w->HoseiCnt % 10 != 5) {
            return 0;
        }
        ang = GetXZAngle(&em->pos, &w->Go_pos);
        if (fabsf(Muku2(em->ang.y, ang, PI)) > 0.2617994f) {
            return 0;
        }
    }
    EM39_JUMPDOWN_PROBE(em, w, a, b, hit, 0.0f, 0.0f, 0.0f, 1000.0f, res0, -, 0, 0);
    EM39_JUMPDOWN_PROBE(em, w, a, b, hit, 0.0f, 0.0f, 0.0f, -1000.0f, res1, +, 0, 1);
    EM39_JUMPDOWN_PROBE(em, w, a, b, hit, 0.0f, 0.0f, 1000.0f, 0.0f, res2, -, 0, 0);
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = -1000.0f;
    b.y = 500.0f;
    b.z = 0.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, &hit, 0, 0) & 0x142810) {
        w->Target_dir = atan2f(-hit.x, -hit.z);
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return 1;
    }
    return 0;
}

// Jump up: a ladder object (type 0x13) or a scenario ladder near the enemy.
int em39JumpUpCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec a;
    Vec pos;
    s8 level;
    f32 ang;
    u32 i;

    if (em39JumpUpCk2(em)) {
        return 1;
    }
    if (w->Go_pos.y - em->pos.y < 1000.0f) {
        return 0;
    }
    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObj* o = ObjMgr.fastAt(i);
        int alive = o->be_flag & 0x201;

        if (alive != 1) {
            continue;
        }
        if (o->id != 0x13) {
            continue;
        }
        if ((em->pos_old.x - o->pos.x) * (em->pos_old.x - o->pos.x) + (em->pos_old.y - o->pos.y) * (em->pos_old.y - o->pos.y) + (em->pos_old.z - o->pos.z) * (em->pos_old.z - o->pos.z) > 4000000.0f) {
            continue;
        }
        PSMTXRotRad(m, 'y', o->ang.y);
        TransMatrix(m, &o->pos);
        a.x = 0.0f;
        a.y = 3200.0f;
        a.z = -2000.0f;
        PSMTXMultVec(m, &a, &a);
        if (!(fabsf(Muku(&em->pos, &a, em->ang.y, PI)) > 1.0471976f)) {
            w->Target_dir = GetXZAngle(&em->pos, &a);
            w->Target_pos = a;
            EmRoutineSet(em, alive, 0x14, 0, 0);
            return 1;
        }
    }
    if (SceAtSearchLadder(em, &pos, &ang, (u8*) &level) == 0) {
        return 0;
    }
    if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.y - pos.y) * (em->pos.y - pos.y) + (em->pos.z - pos.z) * (em->pos.z - pos.z) > 1000000.0f) {
        return 0;
    }
    if (fabsf(Muku2(em->ang.y, ang, PI)) > 1.5707964f) {
        return 0;
    }
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &pos);
    a.x = 0.0f;
    a.y = (f32) level * 1000.0f;
    a.z = 2000.0f;
    PSMTXMultVec(m, &a, &a);
    w->Target_dir = GetXZAngle(&em->pos, &a);
    w->Target_pos = a;
    EmRoutineSet(em, 1, 0x15, 0, 0);
    return 1;
}

#define EM39_EMI (pG->pEmi)

// Squared distance from the enemy to an EMI point.
#define EM39_EMI_DIST2(em, e)                                                                       \
    (((em)->pos.x - (e)->pos.x) * ((em)->pos.x - (e)->pos.x) + ((em)->pos.y - (e)->pos.y) * ((em)->pos.y - (e)->pos.y) + \
     ((em)->pos.z - (e)->pos.z) * ((em)->pos.z - (e)->pos.z))

// Jump up along a pair of EMI type 0x11 points (state 0 at the foot, 1 at the top, same group byte).
int em39JumpUpCk2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    u32 i;
    u32 j;

    if (pG->pEmi == 0) {
        return 0;
    }
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (EM39_EMI_DIST2(em, e) > 1000000.0f) {
            continue;
        }
        for (j = 0; j < EM39_EMI->n; j++) {
            EmiEntry* f = &EM39_EMI->entry[j];
            int sub;
            int state;
            f32 ang;

            if (f->type != 0x11) {
                continue;
            }
            sub = f->sub;
            if (sub != 0) {
                continue;
            }
            state = f->state;
            if (state != 1) {
                continue;
            }
            if (e->pad_3 != f->pad_3) {
                continue;
            }
            if (fabsf(Muku(&em->pos, &f->pos, em->ang.y, PI)) > 0.2617994f) {
                continue;
            }
            ang = GetXZAngle(&em->pos, &w->Go_pos);
            if (fabsf(Muku2(ang, GetXZAngle(&em->pos, &f->pos), PI)) > 0.2617994f) {
                continue;
            }
            w->Target_dir = GetXZAngle(&em->pos, &f->pos);
            w->Target_pos = f->pos;
            EmRoutineSet(em, state, 0x15, sub, sub);
            return 1;
        }
    }
    return 0;
}

// Drop down along a pair of EMI type 0x11 points (sub 1).
int em39JumpUpCk3(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmiData* emi = EM39_EMI;
    u32 i;
    u32 j;

    if (emi == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        // EM39_EMI (not emi) at the body top: the pG->pRoomEmi reload is hoisted out of the outer loop
        // (no conditional jump passed yet, so the trapping load is movable) and cse2 folds it to a copy
        // of emi; the inner loop's duplicated entry test then reads ->n through that copy while its
        // latch reloads pG->pRoomEmi (after LOOP_VTOP, hoisted into the inner preheader).
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (EM39_EMI_DIST2(em, e) > 1000000.0f) {
            continue;
        }
        for (j = 0; j < EM39_EMI->n; j++) {
            EmiEntry* f = &EM39_EMI->entry[j];
            int state;

            if (f->type != 0x11) {
                continue;
            }
            if (f->sub != 1) {
                continue;
            }
            state = f->state;
            if (state != 1) {
                continue;
            }
            if (e->pad_3 != f->pad_3) {
                continue;
            }
            w->Target_dir = GetXZAngle(&em->pos, &f->pos);
            w->Target_pos = f->pos;
            EmRoutineSet(em, state, 0x16, 0, 0);
            return 1;
        }
    }
    return 0;
}

// Blood effect by the weapon that hit (dmWep), the big ones only from far away.
void em39BloodSet(cEm39* em)
{
    int far = 0;

    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        far = 1;
    }
    switch (em->dmg.m_Wep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x2F, 1, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (far) {
            EmDmBloodSet2(em, 0x2F, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2F, 0, 0, 0, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x2F, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x2F, 2, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    default:
        break; // explicit: the two nodes shape the tree (tools/research/casetree.py search)
    }
}

// Two-motion blend: m0 as the main motion, m1 / m2 by the sign of Blend as the blended one.
void em39BlendMotSet(cEm39* em, void* m0, void* m1, void* m2, void* seq0, void* seq1, void* seq2, u16 attr)
{
    Em39Work* w = EM39_WK(em);
    MotionWorkSub* bm;
    void* m;
    void* seq;
    void* ai;
    int dd;
    asm("" : "=r"(ai) : "0"(seq0)); // COMPILER-DIFF: #2 (u16 argument masked at the calls)
    asm("" : "=r"(dd) : "0"((int) attr)); // COMPILER-DIFF: #2
    f32 rate = fabsf(w->Blend);

    MotionSetCore(em, MOTION(em), m0, ai, (u8) w->Hokan, (u16) dd, (u16) w->Frame);
    if (w->Blend < 0.0f) {
        m = m1;
        seq = seq1;
    } else {
        m = m2;
        seq = seq2;
    }
    bm = &w->Sub_mot;
    MotionSetCore(em, bm, m, seq, (u8) w->Hokan, (u16) dd, (u16) w->Frame);
    em->Motion.blend = bm;
    bm->Brate = rate * 0.00390625f;
    if (w->Hokan) {
        w->Hokan--;
    }
    w->Frame++;
    if ((u32) w->Frame >= em->Motion.Seq_frame_num) {
        w->Frame = 0;
    }
}

// Appear at an EMI type 0xE point: sub 0 = drop in, 1 = door (facing the player), 2 = fixed spot.
#define EM39_APPEAR_POS(em, w, e)                                                                   \
    (w)->pGotoPoint = e;                                                                           \
    (em)->dmg.m_Timer = 2;                                                                              \
    AtariOff(&(em)->atari, 0xFCFF);                                                                \
    (em)->setPos(&(e)->pos);                                                                       \
    (em)->pos = (e)->pos;                                                                          \
    (em)->pos_old = (em)->pos;

// The reappearance from hiding: scans the EMI type 0xE appear points (skipping the last one used
// and, on the first pass, any of the same kind / group) for one whose area suits the player's
// position (em39AreaCk): sub 0 = a perch, he crouches there facing the player (Sit); sub 1 = a wall
// point the player is facing within 45 deg (60 % skipped when another exists), he stands behind it
// (WallWait); sub 2 = a fixed spot from which he runs in (Run). Placed there invulnerable and
// collision off with the damage counters reset. Returns 1 when he appeared; nothing while the
// player is below y -1 m or east of x 20 m.
int em39AppearCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec plPos;
    Vec pos;
    int retry;
    u32 i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (pPL->pos.y < -1000.0f) {
        return 0;
    }
    if (pPL->pos.x > 20000.0f) {
        return 0;
    }
    plPos = pPL->pos;
    plPos.y += 1500.0f;
    retry = 1;
    GetPlPos(&pos, 20.0f, 0);
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (w->Old_no == i) {
            continue;
        }
        if (w->Old_no != -1 && retry) {
            u32 ofs = w->Old_no * 0x40 + 8;

            if ((*(u32*) ((u32) EM39_EMI + ofs) & 0x00FF00FF) == (*(u32*) e & 0x00FF00FF)) {
                continue;
            }
        }
        switch (e->sub) {
        case 0: {
            int st = e->state;
            if (st != 0) {
                continue;
            }
            if (em39AreaCk(em, 0, 1, e->pad_3) == 0) {
                continue;
            }
            w->Total_damage = st;
            EM39_APPEAR_POS(em, w, e);
            em->ang.y = GetXZAngle(&em->pos, &pPL->pos);
            w->Atk_wait = st;
            em->r_no_0 = 1;
            em->r_no_1 = 5;
            em->r_no_2 = st;
            em->r_no_3 = 1;
            w->Old_no = i;
            w->Appear_damage = st;
            w->Be_flg |= 0x800;
            w->Be_flg &= ~0x2000;
            return 1;
        }
        case 1: {
            u32 j;
            int found;

            if (e->state != 0) {
                continue;
            }
            if (em39AreaCk(em, 1, 1, e->pad_3) == 0) {
                continue;
            }
            if (fabsf(Muku(&pPL->pos, &e->pos, pPL->ang.y, PI)) > 0.7853982f) {
                continue;
            }
            found = 0;
            for (j = i + 1; j < EM39_EMI->n; j++) {
                u8* f = (u8*) &EM39_EMI->entry[j];

                if (f[0] == 0xE && f[1] == e->sub && f[2] == e->state && f[3] == e->pad_3) {
                    found = 1;
                    break;
                }
            }
            if (found && (u8) (Rnd() % 10) > 3) {
                retry = 0;
                w->Old_no = i;
                continue;
            }
            w->Total_damage = 0;
            EM39_APPEAR_POS(em, w, e);
            em->ang.y = e->rotY + PI;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            w->Atk_wait = 0;
            em->r_no_0 = 1;
            em->r_no_1 = 7;
            em->r_no_2 = 0;
            em->r_no_3 = 1;
            w->Appear_damage = 0;
            w->Old_no = i;
            w->Be_flg |= 0x800;
            w->Be_flg &= ~0x2000;
            return 1;
        }
        case 2: {
            int st = e->state;
            if (st != 0) {
                continue;
            }
            if (em39AreaCk(em, 2, 1, e->pad_3) == 0) {
                continue;
            }
            w->Total_damage = st;
            w->pGotoPoint = (EmiEntry*) st;
            em->dmg.m_Timer = 2;
            AtariOff(&em->atari, 0xFCFF);
            em->setPos(&e->pos);
            em->pos = e->pos;
            em->pos_old = em->pos;
            em->ang.y = e->rotY;
            w->Atk_wait = st;
            EmRoutineSet(em, 1, 9, st, st);
            w->Appear_damage = st;
            w->Old_no = i;
            w->Be_flg |= 0x800;
            w->Be_flg &= ~0x2000;
            return 1;
        }
        default:
            continue;
        }
    }
    return 0;
}

// The player stands on the exit point (EMI type 0xE, state 2) of the enemy's entry group.
int em39ExitCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmiData* emi = EM39_EMI;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    if (w->pGotoPoint == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (e->sub != w->pGotoPoint->sub) {
            continue;
        }
        if (e->state != 2) {
            continue;
        }
        if (e->pad_3 != w->pGotoPoint->pad_3) {
            continue;
        }
        if (!((pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z) > 4000000.0f)) {
            w->Be_flg |= 0x2000;
            return 1;
        }
    }
    return 0;
}

// Move to the group's EMI type 0xE state 3 point, and retire the group's other points.
int em39AreaMoveCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    u32 i;
    // `t`/`ofs` shared by both loops: with two sets each they are non-replaceable user-variable givs whose benefit
    // (3/5 - copy_cost 4 - add_cost 2) is negative, so loop.c leaves the outer `i*64+8` unreduced (`slwi; addi 8` per
    // iteration like the target); the inner loop's copies get a final value and are reduced as before.
    u32 ofs;
    u32 t;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->pGotoPoint == 0) {
        return 0;
    }
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e;

        t = i * 0x40;
        ofs = t + 8;
        e = (EmiEntry*) ((u32) EM39_EMI + ofs);

        if (e->type != 0xE) {
            continue;
        }
        if (e->sub != w->pGotoPoint->sub) {
            continue;
        }
        if (e->state != 3) {
            continue;
        }
        if (e->pad_3 != w->pGotoPoint->pad_3) {
            continue;
        }
        w->Goto_mode = 1;
        w->Goto_pos = e->pos;
        em->r_no_0 = 1;
        em->r_no_1 = 0xA;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        for (i = 0; i < EM39_EMI->n; i++) {
            EmiEntry* f;

            t = i * 0x40;
            ofs = t + 8;
            f = (EmiEntry*) ((u32) EM39_EMI + ofs);

            if (f->type != 0xE) {
                continue;
            }
            if (f->sub != w->pGotoPoint->sub) {
                continue;
            }
            if (f->pad_3 != w->pGotoPoint->pad_3) {
                continue;
            }
            f->type = 0;
        }
        w->pGotoPoint = 0;
        return 1;
    }
    return 0;
}

// Change the sitting point to another free EMI type 0xE point of the same group.
int em39SitChg(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    u32 i;

    if (pG->pEmi == 0) {
        return 0;
    }
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (e == w->pGotoPoint) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (e->state != w->pGotoPoint->sub) {
            continue;
        }
        if (e->pad_3 != w->pGotoPoint->pad_3) {
            continue;
        }
        if ((u8) (Rnd() % 10) > 4) {
            w->pGotoPoint = e;
            return 1;
        }
    }
    return 0;
}

// The player is within 2000 of an EMI type 0xE point with the given sub / state / group.
int em39AreaCk(cEm39* em, int sub, int state, int group)
{
    EmiData* emi = EM39_EMI;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (e->sub != sub) {
            continue;
        }
        if (e->state != state) {
            continue;
        }
        if (e->pad_3 != group) {
            continue;
        }
        if (!((pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.y - e->pos.y) * (pPL->pos.y - e->pos.y) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z) > 4000000.0f)) {
            return 1;
        }
    }
    return 0;
}

// Weapon in hand: 0 none, 1 knife, 2 (grenade), 3 machine gun, 4 bow.
void em39WepSet(cEm39* em, int no)
{
    Em39Work* w = EM39_WK(em);

    w->Wep_type = no;
    if (w->pMachineGun) {
        w->pMachineGun->setTransMode(0);
    }
    if (w->pKnife) {
        w->pKnife->setTransMode(0);
    }
    if (w->pBow) {
        w->pBow->setTransMode(0);
    }
    if (w->pModKnife) {
        w->pModKnife->be_flag |= 8;
    }
    switch ((u32) no) {
    case 1:
        if (w->pKnife) {
            w->pKnife->setTransMode(1);
        }
        if (w->pModKnife) {
            w->pModKnife->be_flag &= ~8;
        }
        break;
    case 3:
        if (w->pMachineGun) {
            w->pMachineGun->setTransMode(1);
        }
        break;
    case 4:
        if (w->pBow) {
            w->pBow->setTransMode(1);
        }
        break;
    case 0:
    case 2:
    default:
        break;
    }
}

// Catch range: the player in the enemy's local box, nothing between them and beside them.
#define EM39_CATCH_BODY(em, w)                                                                      \
    Mtx inv;                                                                                       \
    Vec p;                                                                                         \
    Vec a;                                                                                         \
    Vec b;                                                                                         \
    Mtx m;                                                                                         \
    int dead = 1;                                                                                  \
    int noFlag;                                                                                    \
                                                                                                   \
    if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {                                                   \
        dead = 0;                                                                                  \
    }                                                                                              \
    if (dead) {                                                                                    \
        return 0;                                                                                  \
    }                                                                                              \
    if ((s16) pG->pl_life <= 0) {                                                                  \
        return 0;                                                                                  \
    }                                                                                              \
    if ((em)->hp <= 0) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    if (!((em)->Motion.Seq_old.Free & 2)) {                                                                 \
        return 0;                                                                                  \
    }                                                                                              \
    noFlag = !((w)->Be_flg & 1);                                                                    \
    if (noFlag) {                                                                                  \
        return 0;                                                                                  \
    }                                                                                              \
    if (StaFlagChk(pG, STA_PL_CATCHED)) {                                                                 \
        return 0;                                                                                  \
    }                                                                                              \
    PSMTXInverse((em)->mat, inv);                                                                  \
    PSMTXMultVec(inv, &pPL->pos, &p);                                                              \
    if (p.x < -800.0f) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.x > 800.0f) {                                                                            \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.y < -500.0f) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.y > 500.0f) {                                                                            \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.z < 0.0f) {                                                                              \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.z > 1700.0f) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    a = (em)->pos;                                                                                 \
    b = pPL->pos;                                                                                  \
    a.y += 500.0f;                                                                                 \
    b.y += 500.0f;                                                                                 \
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    PSMTXRotRad(m, 'y', GetXZAngle(&(em)->pos, &pPL->pos));                                        \
    TransMatrix(m, &(em)->pos);                                                                    \
    a.x = 300.0f;                                                                                  \
    a.y = 500.0f;                                                                                  \
    a.z = 0.0f;                                                                                    \
    b.x = 300.0f;                                                                                  \
    b.y = 500.0f;                                                                                  \
    b.z = 500.0f;                                                                                  \
    PSMTXMultVec(m, &a, &a);                                                                       \
    PSMTXMultVec(m, &b, &b);                                                                       \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    a.x = 300.0f;                                                                                  \
    a.y = 500.0f;                                                                                  \
    a.z = 0.0f;                                                                                    \
    b.x = 300.0f;                                                                                  \
    b.y = 500.0f;                                                                                  \
    b.z = 500.0f;                                                                                  \
    PSMTXMultVec(m, &a, &a);                                                                       \
    PSMTXMultVec(m, &b, &b);                                                                       \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    pPL->dmg.set(0, 2);                                                                            \
    (em)->dmg.set(0, 2);                                                                           \
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);                           \
    return 1;

// The knife grab test (em39_R1_br_KnifeCatch): a live, free player in a 1.6 m wide box up to 1.7 m
// ahead at the same height, with clear lines of sight and no wall beside the reach; marks both dmg
// and rumbles. Returns 1 on a catch.
int em39CatchCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    EM39_CATCH_BODY(em, w);
}

// The kick zone test of the final form's kicks: the same box as em39CatchCk.
int em39KickHitCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    EM39_CATCH_BODY(em, w);
}

// Arrow on the bow (weapon 0x37): only while no thrown knife is out.
void em39ArrowSet(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec pos;
    Vec rot;

    if (w->pBow == 0) {
        return;
    }
    if (w->pArrow) {
        return;
    }
    w->pBow->getPartsPtr(4);
    pos.x = -850.0f;
    pos.y = 5.0f;
    pos.z = 24.0f;
    rot.x = 0.0f;
    rot.y = -1.5707964f;
    rot.z = 0.0f;
    w->pArrow = SetWeapon(ARC(0x37), ARC(0x38), &pos, &rot, 0);
    if (w->pArrow) {
        w->pArrow->setParent(em, 0xA, 0);
        em39BowSet(em, 0);
        SndCall(8, 0x10, &em->pos, em->id, 0, em);
    }
}

// Arrow shot from the bow at `target` (mode 1 / 2: aimed 1000 to the right / left of it).
#define EM39_ARROW_AIM(pos, tgt, dir, target)                                                       \
    tgt = *(target);                                                                               \
    PSVECSubtract(&(pos), &(tgt), &(dir));                                                         \
    dir.y = 0.0f;                                                                                  \
    if (dir.x == 0.0f && dir.z == 0.0f) {                                                          \
        dir.z = 1.0f;                                                                              \
    }

// Shoots the nocked arrow from the bow's string part at `target` (mode 0 1.5 m past it, 1 / 2 a
// metre to its right / left) at 1.5 m per frame with a small random spread (setShotArrow with
// attack 0, the wall hit sound); the bow string relaxes and the arrow becomes free-flying.
void em39ArrowFire(cEm39* em, Vec* target, int mode)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec pos;
    Vec tgt;
    Vec dir;

    if (w->pBow == 0) {
        return;
    }
    if (w->pArrow == 0) {
        return;
    }
    em39BowSet(em, 0);
    w->pArrow->setTransMode(1);
    {
        cModel* p = w->pBow->getPartsPtr(4);

        pos.x = 500.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        PSMTXMultVec(p->mat, &pos, &pos);
    }
    TransMatrix(w->pArrow->mat, &pos);
    switch ((u32) mode) {
    case 0:
    default:
        EM39_ARROW_AIM(pos, tgt, dir, target);
#line 11941 "D:/Bio4/Prog/em39.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, 1500.0f);
        PSVECAdd(&tgt, &dir, &tgt);
        break;
    case 1:
        EM39_ARROW_AIM(pos, tgt, dir, target);
#line 11951 "D:/Bio4/Prog/em39.cpp"
        VECNormalize(&dir, &dir);
        PSMTXRotRad(m, 'y', 1.5707964f);
        PSMTXMultVecSR(m, &dir, &dir);
        PSVECScale(&dir, &dir, 1000.0f);
        PSVECAdd(&tgt, &dir, &tgt);
        break;
    case 2:
        EM39_ARROW_AIM(pos, tgt, dir, target);
#line 11963 "D:/Bio4/Prog/em39.cpp"
        VECNormalize(&dir, &dir);
        PSMTXRotRad(m, 'y', -1.5707964f);
        PSMTXMultVecSR(m, &dir, &dir);
        PSVECScale(&dir, &dir, 1000.0f);
        PSVECAdd(&tgt, &dir, &tgt);
        break;
    }
    PSVECSubtract(&tgt, &pos, &dir);
    if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
        dir.z = 1.0f;
    }
#line 11973 "D:/Bio4/Prog/em39.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 1500.0f);
    dir.x += fRand1_1() * 10.0f;
    dir.y += fRand1_1() * 10.0f;
    dir.z += fRand1_1() * 10.0f;
    w->pArrow->be_flag |= 0x4000;
    w->pArrow->setShotArrow(&dir, &em39_atk_tbl[0]);
    w->pArrow->setSeHitWall(8, 0x13, em->id);
    w->pArrow = 0;
}

// Bow string parts (4) drawn / hidden, arrow transparency and the aiming effect.
void em39BowSet(cEm39* em, int on)
{
    Em39Work* w = EM39_WK(em);
    cModel* p;

    if (w->pBow == 0) {
        return;
    }
    p = w->pBow->getPartsPtr(4);
    if (on) {
        p->scale.x = 1.0f;
        p->scale.y = 1.0f;
        p->scale.z = 1.0f;
        if (w->pArrow) {
            w->pArrow->setTransMode(0);
        }
        EstSet(w->pBow, -1, 0, 0, EFF_EM39, 9, 0, w->EffKindIdArrow, em, 0);
    } else {
        p->scale.x = 0.0f;
        p->scale.y = 0.0f;
        p->scale.z = 0.0f;
        if (w->pArrow) {
            w->pArrow->setTransMode(1);
        }
        EffectEspDelete(0, w->EffKindIdArrow, em, 0);
        EffectEspgenDelete(0, w->EffKindIdArrow, em);
        EffectEfmDelete(0, w->EffKindIdArrow, em);
    }
}

// A door (em 0x41) the knife swing can open: in front, within its frame, openable.
int em39DoorOpenCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec p;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmDoor* d = (cEmDoor*) EmMgr.fastAt(i);
        EmDoorWork* dw;
        f32 ang;
        u32 st;

        if ((d->be_flag & 0x201) != 1) {
            continue;
        }
        if (d->id != 0x41) {
            continue;
        }
        if (d->hp <= 0) {
            continue;
        }
        if ((em->pos.x - d->pos.x) * (em->pos.x - d->pos.x) + (em->pos.y - d->pos.y) * (em->pos.y - d->pos.y) + (em->pos.z - d->pos.z) * (em->pos.z - d->pos.z) > 6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(d);
        ang = fabsf(Muku2(dw->base_dir, em->ang.y, PI));
        if (ang > 0.7853982f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->base_im, &em->pos, &p);
        if (ang < 1.5707964f) {
            if (p.z > 0.0f) {
                continue;
            }
            if (p.z < -800.0f) {
                continue;
            }
        } else {
            if (p.z < 0.0f) {
                continue;
            }
            if (p.z > 800.0f) {
                continue;
            }
        }
        if (p.x > dw->Width) {
            continue;
        }
        if (p.x < -dw->Width) {
            continue;
        }
        if (p.y > 500.0f) {
            continue;
        }
        if (p.y < -500.0f) {
            continue;
        }
        st = d->ckOpen();
        if (st) {
            if ((u32) st <= 3) {
                continue;
            }
        }
        if (em->type != 2 && d->type == 0) {
            w->pDoor = d;
            em->r_no_0 = 1;
            em->r_no_1 = 0x19;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
            return 1;
        }
        d->setOpen(&em->pos, 0, 0, 0);
        return 0;
    }
    return 0;
}

// Tower form left arm: its own motion work (armMot) by the arm state x8BB / x8BC.
#define EM39_ARM_MOT(w)  ((MotionWork*) &(w)->Arm_mot)

// Final form only, per frame: the mutated left arm runs its own motion work (Arm_mot) as a state
// machine set by the routines through Arm_rno: 0 / 4 / 8 blend from the current pose (Arm_type 0
// relaxed, 1 raised, 2 shielding) into the new one and hold its loop (2 / 6 / 0xA -> the loop
// states); 0xC / 0xE / 0x10 are the one-shot attack / pin / release motions, 0x12 holds the release's
// last frame. The pose changes play the arm's flesh sound.
void em39ArmControl(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int se = 0;
    void* mot;

    if (em->type != 2) {
        return;
    }
    w->Arm_mot.Seq_speed = 1.0f;
    w->Arm_mot.Mot_flag |= 0x10000000;
    switch (w->Arm_rno) {
    case 0:
        switch (w->Arm_type) {
        case 0:
        default:
            mot = ARC(0xFF);
            break;
        case 1:
            mot = ARC(0x103);
            se = 1;
            break;
        case 2:
            mot = ARC(0x105);
            se = 1;
            break;
        }
        MotionSetCore(em, EM39_ARM_MOT(w), mot, 0, 0, 4, 0);
        w->Arm_type = 0;
        w->Arm_rno++;
        goto STEP;
    case 2:
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0xFF), 0, 0, 4, 0);
        w->Arm_rno++;
        goto MOVE;
    case 4:
        switch (w->Arm_type) {
        case 0:
        default:
            mot = ARC(0x102);
            se = 1;
            break;
        case 1:
            mot = ARC(0x100);
            break;
        case 2:
            mot = ARC(0x107);
            se = 1;
            break;
        }
        MotionSetCore(em, EM39_ARM_MOT(w), mot, 0, 0, 4, 0);
        w->Arm_type = 1;
        w->Arm_rno++;
        goto STEP;
    case 6:
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x100), 0, 0, 4, 0);
        w->Arm_rno++;
        goto MOVE;
    case 8:
        switch (w->Arm_type) {
        case 0:
        default:
            mot = ARC(0x104);
            se = 1;
            break;
        case 1:
            mot = ARC(0x106);
            se = 1;
            break;
        case 2:
            mot = ARC(0x101);
            break;
        }
        MotionSetCore(em, EM39_ARM_MOT(w), mot, 0, 0, 4, 0);
        w->Arm_type = 2;
        w->Arm_rno++;
    case 1:
    case 5:
    case 9:
    STEP:
        MotionMoveCore(em, EM39_ARM_MOT(w), 0);
        if (MotionSequenceCtrl(EM39_ARM_MOT(w))) {
            w->Arm_rno++;
        }
        break;
    case 0xA:
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x101), 0, 0, 4, 0);
        w->Arm_rno++;
        goto MOVE;
    case 0xC:
        w->Arm_mot.Seq_speed = 1.0f;
        w->Arm_mot.Mot_flag |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x108), 0, 0, 1, 0);
        w->Arm_type = 1;
        se = 1;
        w->Arm_rno++;
        goto MOVE;
    case 0xE:
        w->Arm_mot.Seq_speed = 1.0f;
        w->Arm_mot.Mot_flag |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x109), 0, 0, 1, 0);
        w->Arm_type = 1;
        se = 1;
        w->Arm_rno++;
        goto MOVE;
    case 0x10:
        w->Arm_mot.Seq_speed = 1.0f;
        w->Arm_mot.Mot_flag |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x10A), 0, 0, 1, 0);
        w->Arm_type = se;
        w->Arm_rno++;
    case 3:
    case 7:
    case 0xB:
    case 0xD:
    case 0xF:
    case 0x11:
    MOVE:
        MotionMoveCore(em, EM39_ARM_MOT(w), 0);
        MotionSequenceCtrl(EM39_ARM_MOT(w));
        break;
    case 0x12: {
        int mf = ((MotionData*) ARC(0x10A))->maxFrame;

        w->Arm_mot.Seq_speed = 1.0f;
        w->Arm_mot.Mot_flag |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x10A), 0, 0, 0, (u16) ((mf & 0x3FFF) - 1));
        w->Arm_type = se;
        w->Arm_rno++;
    }
    case 0x13:
        MotionMoveCore(em, EM39_ARM_MOT(w), 0);
        MotionSequenceCtrl(EM39_ARM_MOT(w));
        break;
    }
    if (se) {
        SndCall(8, 0x53, &em->getPartsPtr(0xE)->world, em->id, 0, em);
    }
}

// Fence jump: a fence (attribute 0x20) right in front; the landing side is the free one.
int em39FanceJumpCk2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    Vec hit;
    Vec nrm;
    Mtx m;
    Vec c;
    Vec d;
    Vec e;
    int side;
    f32 ang;

    if ((u32) w->HoseiCnt % 10 != 6) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 800.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0) & 0x20)) {
        return 0;
    }
    ang = atan2f(-nrm.x, -nrm.z);
    w->Target_dir = ang;
    side = 0;
    w->Target_pos = em->pos;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    d.x = 300.0f;
    d.y = 1200.0f;
    d.z = 0.0f;
    e.x = 300.0f;
    e.y = 1200.0f;
    e.z = 800.0f;
    PSMTXMultVec(m, &d, &d);
    PSMTXMultVec(m, &e, &e);
    if (SatMgr.hitCheck(&d, &e, 0, 0, 0, 0x20)) {
        side = 1;
    }
    d.x = -300.0f;
    d.y = 1200.0f;
    d.z = 0.0f;
    e.x = -300.0f;
    e.y = 1200.0f;
    e.z = 800.0f;
    PSMTXMultVec(m, &d, &d);
    PSMTXMultVec(m, &e, &e);
    if (SatMgr.hitCheck(&d, &e, 0, 0, 0, 0x20)) {
        side |= 2;
    }
    if (side == 3) {
        return 0;
    }
    if (side & 1) {
        c.x = -300.0f;
        c.y = 0.0f;
        c.z = 0.0f;
        PSMTXMultVec(m, &c, &w->Target_pos);
    }
    if (side & 2) {
        c.x = 300.0f;
        c.y = 0.0f;
        c.z = 0.0f;
        PSMTXMultVec(m, &c, &w->Target_pos);
    }
    return 1;
}

// From the walking routines: starts the fence vault (1/0x17) when em39FanceJumpCk2 finds one to
// clear. Returns 1 when started.
int em39FanceJumpCk(cEm39* em)
{
    if (em39FanceJumpCk2(em)) {
        EmRoutineSet(em, 1, 0x17, 0, 0);
        return 1;
    }
    return 0;
}

// Damage value of the hit (100 for the non-weapon ids), doubled at the head parts.
int em39SetDmVal(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    YARARE_INFO* h = em->dmg.m_pDamageYarare;
    int far = 0;
    int val;

    if (h->len < 36000000.0f) {
        far = 1;
    }
    val = 100;
    if (em->dmg.m_Wep <= 0x2D) {
        val = GetWepDmVal(em, em->dmg.m_Wep, far);
    }
    if (h->parts_no == 5) {
        val += val;
        w->Total_damage += 200;
    }
    return val;
}

// Attack return: what the enemy does after an attack ended.
int em39AtkRtnCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    Vec c;
    int dead = 1;
    f32 dy;
    f32 ang;
    int hit;
    int LongAtk_wait;
    int noFlag;

    if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    noFlag = !(w->Be_flg & 1);
    if (noFlag) {
        return 0;
    }
    if (w->Atk_wait) {
        return 0;
    }
    dy = em->pos.y - pPL->pos.y;
    dy = fabsf(dy);
    ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI));
    if (em->type == 2) {
        if (pG->Game_level > 1 && w->SuperDashWait == 0 && (w->Be_flg & 1) && em->l_pl > 36000000.0f && em->l_pl < 100000000.0f &&
            w->Pl_rot < 0.5235988f && ang < 0.5235988f) {
            a = pPL->pos;
            b = em->pos;
            a.y += 500.0f;
            b.y += 500.0f;
            if (!SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                EmRoutineSet(em, 1, 0x12, 0, 0);
                return 1;
            }
        }
        LongAtk_wait = w->LongAtk_wait;
        if (LongAtk_wait == 0 && em->l_pl < 20250000.0f && em->l_pl > 12250000.0f && w->Pl_rot < 1.5707964f) {
            if ((u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 0x29, LongAtk_wait, LongAtk_wait);
            } else {
                EmRoutineSet(em, 1, 0x2A, LongAtk_wait, LongAtk_wait);
            }
            return 1;
        }
        if (em->l_pl < 4840000.0f && w->Pl_rot < 1.5707964f && dy < 1500.0f) {
            u32 r = (u8) (Rnd() % 100);

            if (em->l_pl > 4000000.0f && r > 30) {
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x2B, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0x2C, 0, 0);
                }
            } else {
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x28, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0x27, 0, 0);
                }
            }
            return 1;
        }
        return 0;
    }
    if (em->l_pl < 2890000.0f && w->Pl_rot < 1.5707964f && (w->Be_flg & 1)) {
        a = em->pos;
        c = pPL->pos;
        a.y += 500.0f;
        c.y += 500.0f;
        hit = SatMgr.hitCheck(&a, &c, 0, 0, 0, 0);
        if (hit == 0) {
            if (dy < 250.0f) {
                if (ang < 1.5707964f) {
                    if ((u8) (Rnd() % 10) > 6) {
                        EmRoutineSet(em, 1, 0x1C, hit, hit);
                        return 1;
                    }
                } else {
                    EmRoutineSet(em, 1, 0x1A, hit, hit);
                    return 1;
                }
            }
            if (dy < 1700.0f) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
                return 1;
            }
        }
    }
    LongAtk_wait = w->LongAtk_wait;
    if (LongAtk_wait == 0 && (w->Be_flg & 1) && em->l_pl > 36000000.0f && em->l_pl < 100000000.0f && w->Pl_rot < 0.5235988f &&
        dy < 2000.0f) {
        if ((u8) (Rnd() % 10) > 2 || dy > 1000.0f) {
            EmRoutineSet(em, 1, 0x1D, LongAtk_wait, LongAtk_wait);
        } else {
            EmRoutineSet(em, 1, 0x23, LongAtk_wait, LongAtk_wait);
        }
        return 1;
    }
    return 0;
}

// From the floor routines: when the level script has set a goto target (Goto_mode, setGoto*), runs
// there (1/0xA). Returns 1 when started.
int em39GotoCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (w->Goto_mode == 0) {
        return 0;
    }
    EmRoutineSet(em, 1, 0xA, 0, 0);
    return 1;
}

// For the level script: starts the second half of the first battle: placed at its spot, phase 3
// (Locate), damage counters reset, a 30-frame attack wait, Wait.
void cEm39::set2ndBattle()
{
    register Em39Work* w asm("r29"); // COMPILER-DIFF: #13 -- w kept live past its last store (see the asm below)
    Vec p = { 31259.0f, 5250.0f, -14068.0f };
    u32 zero;

    w = EM39_WK(this);
    AtariOff(&atari, 0xFCFF);
    ang.y = 1.4660766f;
    setPos(&p);
    zero = 0;
    w->Atk_wait = 30;
    w->Locate = 3;
    w->Flash_damage = zero;
    w->pGotoPoint = (EmiEntry*) zero;
    w->Appear_damage = zero;
    w->Total_damage = zero;
    EmRoutineSet(this, 1, 4, 0, 0);
    asm volatile("" : : "r"(zero), "r"(w)); // COMPILER-DIFF: #13 -- neither the zero nor w dies at its last store in the original (pure source order)
}

// For the level script: the first door was opened: phase 1, the appearance damage counter reset.
void cEm39::set1stDoorClear()
{
    Em39Work* w = EM39_WK(this);

    w->Flash_damage = 0;
    w->Locate = 1;
}

// For the level script: the second door was opened: phase 4, the appearance damage counter reset.
void cEm39::set2ndDoorClear()
{
    Em39Work* w = EM39_WK(this);

    w->Flash_damage = 0;
    w->Locate = 4;
}

// Motion-key voice request (seNo - 1) for the voice numbers the enemy owns.
void em39VoiceMove(cEm39* em)
{
    u32 v = em->Motion.Seq_old.Se;

    if (v == 0) {
        return;
    }
    v--;
    switch (v) {
    case 6:
    case 9:
    case 0x19:
    case 0x1E:
    case 0x1F:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x3E:
    case 0x3F:
        em->Motion.Seq_old.Se = 0;
        em39SetVoice(em, (u8) v);
        break;
    }
}

// Stops the current voice and plays voice `no` at the head part 4; any queued line is dropped.
void em39SetVoice(cEm39* em, u16 no)
{
    Em39Work* w = EM39_WK(em);
    cModel* p = em->getPartsPtr(4);

    SndStop(w->Se_id, 0);
    w->Se_id = SndCall(8, no, &p->world, em->id, 0, em);
    w->Speech_wait = 0;
}

// Queues voice `time` to play `no` frames from now (the arguments are used swapped: `no` is the
// delay in Speech_wait, `time` the voice number in Speech_se).
void em39SetSpeech(cEm39* em, int no, int time)
{
    Em39Work* w = EM39_WK(em);

    w->Speech_wait = no;
    w->Speech_se = time;
}

// Per frame: counts the queued line down and plays it when due.
void em39SpeechMove(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (w->Speech_wait) {
        w->Speech_wait--;
        if (w->Speech_wait == 0) {
            em39SetVoice(em, w->Speech_se);
        }
    }
}

// Slant (side-step) towards the player from mid range; the side alternates.
int em39SlantCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    int hit;

    if (em->type == 2) {
        return 0;
    }
    if (em->l_pl > 144000000.0f) {
        return 0;
    }
    if (em->l_pl < 12250000.0f) {
        return 0;
    }
    if (w->Go_rot > 0.5235988f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 0.5235988f) {
        return 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 500.0f) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    hit = SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
    if (hit) {
        return 0;
    }
    w->Slant_type ^= 1;
    em->r_no_0 = 1;
    em->r_no_1 = 0x10;
    em->r_no_2 = hit;
    em->r_no_3 = w->Slant_type;
    return 1;
}

// Tower form: jump aside when the player aims from far enough (by difficulty / remaining hp).
int em39SlantCk2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (em->type != 2) {
        return 0;
    }
    if (em->l_pl < 6250000.0f) {
        return 0;
    }
    if (w->Go_rot > 0.5235988f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI)) > 0.5235988f) {
        return 0;
    }
    if (pG->Game_level <= 1) {
        return 0;
    }
    if (pG->Game_level <= 3 && (u8) (Rnd() % 10) > 4) {
        return 0;
    }
    if (pG->Game_level <= 9 && em->hp > em->hp_max / 2 && (u8) (Rnd() % 10) > 6) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x11, 0, 0);
    return 1;
}

// Guard check (tower form): the hit landed on the shield parts.
int em39GuardCk(cEm39* em)
{
    YARARE_INFO* h;

    if (em->type != 2) {
        return 0;
    }
    h = em->dmg.m_pDamageYarare;
    if (h->parts_no == 0xE) {
        return 1;
    }
    if (h->parts_no == 0xF) {
        return 1;
    }
    if (h->parts_no == 0x62) {
        return 1;
    }
    if (h->parts_no == 0x63) {
        return 1;
    }
    if (h->parts_no == 0x64) {
        return 1;
    }
    if (h->parts_no == 0x65) {
        return 1;
    }
    if (h->parts_no == 0x66) {
        return 1;
    }
    if (h->parts_no == 0x7B) {
        return 1;
    }
    if (h->parts_no == 0x7C) {
        return 1;
    }
    if (h->parts_no == 0x7D) {
        return 1;
    }
    if (h->parts_no == 0x7E) {
        return 1;
    }
    return h->parts_no == 0x7F;
}

// Motion-key foot sounds (seNo - 1) with the footstep dust in the tower room.
void em39FootEff(cEm39* em)
{
    u32 v = em->Motion.Seq_old.Se;
    u16 no;

    if (v == 0) {
        return;
    }
    no = (u8) (v - 1);
    switch (no) {
    case 2:
        if (em->r_no_0 == 1 && em->r_no_1 == 9) {
            EstSet(0, -1, &em->getPartsPtr(0x15)->world, &em->ang, EFF_EM39, 0x2E, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (pG->room_id != 0x31C) {
            no = 0x61;
        }
        break;
    case 3:
        if (em->r_no_0 == 1 && em->r_no_1 == 9) {
            EstSet(0, -1, &em->getPartsPtr(0x19)->world, &em->ang, EFF_EM39, 0x2E, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (pG->room_id != 0x31C) {
            no = 0x62;
        }
        break;
    case 0:
        if (pG->room_id != 0x31C) {
            no = 0x5F;
        }
        break;
    case 1:
        if (pG->room_id != 0x31C) {
            no = 0x60;
        }
        break;
    case 0xE:
        if (pG->room_id != 0x31C) {
            no = 0x63;
        }
        break;
    case 0x39:
        if (pG->room_id != 0x31C) {
            no = 0x64;
        }
        break;
    default:
        return;
    }
    em->Motion.Seq_old.Se = 0;
    SndCall(8, no, &em->getPartsPtr(0)->world, em->id, 0, pPL);
}

// Motion-key player voice (seNo - 1 = 0x43 / 0x44 / 0x54), Ashley's numbers in her chapter.
void em39PLVoiceCk(cEm39* em)
{
    u32 v = em->Motion.Seq_old.Se;
    u16 no;

    if (v == 0) {
        return;
    }
    no = (u8) (v - 1);
    switch (no) {
    case 0x43:
        if (pG->pl_type == 2) {
            no = 0x55;
        }
        break;
    case 0x44:
        if (pG->pl_type == 2) {
            no = 0x57;
        }
        break;
    case 0x54:
        if (pG->pl_type == 2) {
            no = 0x56;
        }
        break;
    default:
        return;
    }
    em->Motion.Seq_old.Se = 0;
    SndCall(8, no, &pPL->getPartsPtr(4)->world, em->id, 0, pPL);
}

// For the level script: 1 while hidden (Be_flg 0x400).
int cEm39::ckHide()
{
    if (EM39_WK(this)->Be_flg & 0x400) {
        return 1;
    }
    return 0;
}

// Tower form left arm attack: the arm parts origins and points along the forearm (parts 0x63).
void em39LeftArmAtkCk(cEm39* em, int no)
{
    Vec a;
    Vec b = em->pos;
    cModel* p;

    p = em->getPartsPtr(0x10);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x5F);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x60);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x61);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x62);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x63);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 100.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 200.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 300.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 400.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 500.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 600.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 700.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 800.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
}

// The closest EMI type 0xD point to the player within 2000: the cliff-attack spot.
int em39GetCliffPos(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmiData* emi = EM39_EMI;
    EmiEntry* best;
    f32 bestDist;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    bestDist = 4000000.0f;
    best = 0;
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];
        f32 d;

        if (e->type != 0xD) {
            continue;
        }
        d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
        if (d > bestDist) {
            continue;
        }
        bestDist = d;
        best = e;
    }
    if (best == 0) {
        return 0;
    }
    w->Target_pos = best->pos;
    w->Target_dir = best->rotY;
    return 1;
}

// For the level script (final battle timer / HP): the scripted death (3/0).
void cEm39::setDie()
{
    EmRoutineSet(this, 3, 0, 0, 0);
}

// For the level script (the death cutscene was skipped): the corpse placed at its spot, straight
// into the held last frame (3/0 step 2).
void cEm39::setDieCancel()
{
    AtariOff(&atari, 0xFCFF);
    pos.x = 9210.38f;
    pos.y = 12050.0f;
    pos.z = -14974.03f;
    ang.y = 1.57f;
    EmRoutineSet(this, 3, 0, 2, 0);
}

// For the level script: 1 when the first talk is due (Be_flg 0x40000 after the second wall jump)
// and has not run yet (0x80000).
int cEm39::ckTalk1st()
{
    Em39Work* w = EM39_WK(this);

    if (w->Be_flg & 0x80000) {
        return 0;
    }
    if (w->Be_flg & 0x40000) {
        return 1;
    }
    return 0;
}

// For the level script: starts the first talk (voice cut, invulnerable, Be_flg 0x80000, Talk1st).
void cEm39::setTalk1st()
{
    Em39Work* w = EM39_WK(this);

    SndStop(w->Se_id, 0);
    dmg.m_Timer = 2;
    w->Be_flg |= 0x80000;
    EmRoutineSet(this, 1, 0, 0, 0);
}

// For the level script: the first talk ended: collision back, then a gun burst (50 % beyond 5 m)
// or a grenade throw.
void cEm39::setTalk1stCancel()
{
    AtariOn(&atari, 0x300);
    if ((u8) (Rnd() % 10) > 4 && l_pl > 25000000.0f) {
        EmRoutineSet(this, 1, 0x1D, 0, 1);
    } else {
        EmRoutineSet(this, 1, 0x23, 0, 1);
    }
}

// For the level script: 1 when the second talk is due (Be_flg 0x100000, hiding in the final
// phase) and has not run yet (0x200000).
int cEm39::ckTalk2nd()
{
    Em39Work* w = EM39_WK(this);

    if (w->Be_flg & 0x200000) {
        return 0;
    }
    if (w->Be_flg & 0x100000) {
        return 1;
    }
    return 0;
}

// For the level script: starts the second talk (voice cut, invulnerable, Be_flg 0x200000, Talk2nd).
void cEm39::setTalk2nd()
{
    Em39Work* w = EM39_WK(this);

    SndStop(w->Se_id, 0);
    dmg.m_Timer = 2;
    w->Be_flg |= 0x200000;
    EmRoutineSet(this, 1, 1, 0, 0);
}

// For the level script: the second talk ended: back into hiding in the final phase (Locate 5).
void cEm39::setTalk2ndCancel()
{
    EM39_WK(this)->Locate = 5;
    EmRoutineSet(this, 1, 0x26, 0, 0);
}

// For the level script: 1 once the final battle's opening (Success / the cliff grab) is over
// (Be_flg 0x800000) and the bomb countdown may run.
int cEm39::ckBombCutEnable()
{
    if (EM39_WK(this)->Be_flg & 0x800000) {
        return 1;
    }
    return 0;
}

// The split object's .data is 8-aligned (4 pad bytes before the ngcld BSS tag).
asm(".section .data\n\t.balign 8\n\t.text");
