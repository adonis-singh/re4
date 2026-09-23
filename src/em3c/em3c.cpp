// em3c module (D:/Bio4/Prog/em3c.cpp): a humanoid enemy in four variants (cModel::type 0..3; types
// 1 / 3 use a second motion set, types 2 / 3 carry a parasite head object that bites on its own). It
// walks / runs at the player (em3cRouteCk), grabs (em3c_R1_AtkWait, the player escapes with the action
// button: plemEscape*), kicks (em3c_R1_MoveAtk) and bursts its head on big damage (em3cPartsBombHead:
// the head parts fall apart as five-point cloth pieces, em3cPartsBombControl).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3c.h"
#include "em10.h"
#include "emhit.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "obj16.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"


// The module's 0x34-byte COMMON block: uninitialised template statics of the original object,
// merged into .bss by the REL link.
asm(".comm common_em3c,52,4");


typedef void (*Em3cFunc)(cEm3c*);

static void em3c_R0_Init(cEm3c* em);
static void em3c_R0_Move(cEm3c* em);
static void em3c_R1_StartWait(cEm3c* em);
static void em3c_R1_AtkWait(cEm3c* em);
static void plemSurprised(cPlayer* pl);
static void plemEscapeAction(cEm3c* em);
static void plemEscape(cPlayer* pl);
static void subemSurprised();
static void subemSit();
static void em3c_R1_Wait(cEm3c* em);
static void em3c_R1_Walk(cEm3c* em);
static void em3c_R1_Run(cEm3c* em);
static void em3c_R1_Turn180(cEm3c* em);
static void em3c_R1_MoveAtk(cEm3c* em);
static void em3c_R1_CoreAtk(cEm3c* em);
static void plemDmMStar(cPlayer* pl);
static void em3c_R0_Damage(cEm3c* em);
static void em3c_R1_Dm_Normal(cEm3c* em);
static void em3c_R1_Dm_Big(cEm3c* em);
static void em3c_R1_Dm_Head(cEm3c* em);
static void em3c_R0_Die(cEm3c* em);
static void em3c_R1_Die_Normal(cEm3c* em);



// REL entry: hands the module's constructor to the enemy manager (EmInitFunc) so an enemy set
// with id 0x3C is built as a cEm3c.
extern "C" void _prolog()
{
    OSReport("em3c prolog Ok\n");
    EmInitFunc = Em3cInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

// Placement-constructs the enemy over the manager's cEm slot (vtable install; the work is set up
// by em3c_R0_Init on the first move).
void Em3cInit(cEm* em)
{
    new (em) cEm3c();
}

// Per-frame damage reaction, run from move() once the enemy is past init. Consumes the hit the
// weapon code left in dmHit / dmPart / dmWep: picks the hit sound and blood effect by weapon
// class (handgun / shotgun (7, 8: strong only within 6 m) / other) and hit part (kind 0 body, 1 the
// part 3 head, 2 the part 5 parasite), then applies em3cSetDmVal through LifeDownSet. Knife (0x14)
// and 0x16 hits are ignored; the parasite core dies outright to 0x17 / 0x2A (flash). On death it
// plays the core death effect and enters routine 3 (die). Otherwise: a parasite hit drains Head_hp /
// Head_cnt and picks the head-damage routine (2/2); a body hit below half HP without the parasite
// set forces the big damage routine (2/1), and handgun-class / shotgun hits roll for the normal (2/0)
// or big flinch. Nothing happens while the enemy is in start / attack wait (Be_flg 0x400) or already
// in a damage routine (0x100 / 0x200).
void em3cDmCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    YARARE_INFO* part;
    int near;
    int kind;
    int dmg;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    if (em->dmg.m_Wep == 0x14 || em->dmg.m_Wep == 0x16) {
        return;
    }
    part = em->dmg.m_pDamageYarare;
    near = 0;
    if (part->len < 36000000.0f) {
        near = 1;
    }
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    w->Be_flg |= 0x80;
    kind = part->parts_no == 3;
    if (part->parts_no == 5) {
        kind = 2;
    }
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
    case 0x28:
    case 0x2B:
    case 0x2C:
    default:
        switch ((u32) kind) {
        case 0:
        default:
            SndCall(8, 0xE, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
            break;
        case 1:
            SndCall(8, 0x38, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
            break;
        case 2:
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
            break;
        }
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
        switch ((u32) kind) {
        case 0:
        default:
            SndCall(8, 0xE, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
            break;
        case 1:
            SndCall(8, 0x38, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
            break;
        case 2:
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
            break;
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            switch ((u32) kind) {
            case 0:
            default:
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
                break;
            case 1:
                SndCall(8, 0x38, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
                break;
            case 2:
                SndCall(8, 0xF, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
                break;
            }
        } else {
            switch ((u32) kind) {
            case 0:
            default:
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
                break;
            case 1:
                SndCall(8, 0x38, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
                break;
            case 2:
                SndCall(8, 0xF, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
                break;
            }
        }
        break;
    case 0x17:
    case 0x2A:
        break;
    }
    if (w->Be_flg & 0x400) {
        return;
    }
    dmg = em3cSetDmVal(em);
    LifeDownSet(em, dmg, 0);
    if (w->pCore && em->dmg.m_Wep == 0x17) {
        em->hp = 0;
    }
    if (w->pCore && em->dmg.m_Wep == 0x2A) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        if (w->pCore) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
            switch (em->type) {
            case 0:
            case 1:
            default:
                EstSet(em, -1, 0, 0, EFF_EM3C, 0x15, 0, ESP_CORE_KIND_NONE, em, 0);
                break;
            case 2:
            case 3:
                EstSet(0, -1, &w->pCore->getPartsPtr(9)->world, 0, EFF_EM3C, 0x26, 0, ESP_CORE_KIND_NONE, 0, 0);
                break;
            }
        }
        EmSetDie(em);
        EmSetDieCnt(em);
        EmRoutineSet(em, 3, 0, 0, 0);
    } else {
        if (w->Be_flg & 0x200) {
            return;
        }
        if (w->Be_flg & 0x100) {
            return;
        }
        if (part->parts_no == 5) {
            if (w->pCore) {
                SndCall(8, 0x37, &em->pos, em->id, 0, em);
            }
            w->Head_hp -= dmg;
            w->Head_cnt--;
            if (w->Head_hp <= 0) {
                EmRoutineSet(em, 2, 2, 0, 0);
                return;
            }
            if (w->Head_cnt <= 0) {
                if (w->Be_flg & 0x200) {
                    return;
                }
                w->Head_cnt = Rnd() % 3 + 1;
                EmRoutineSet(em, 2, 2, 0, 0);
            }
        } else {
            if (em->hp < em->hp_max / 2 && !(w->Be_flg & 0x800)) {
                EmRoutineSet(em, 2, 1, 0, 0);
                return;
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
            case 0x1B:
            case 0x1D:
            case 0x26:
            case 0x27:
            case 0x28:
            case 0x2B:
                break;
            case 5:
            case 6:
            case 0xD:
            case 0xF:
            case 0x12:
            case 0x13:
            case 0x29:
            case 0x2A:
            case 0x2C:
            case 0x2D:
            default:
                if (Rnd() & 3) {
                    EmRoutineSet(em, 2, 1, 0, 0);
                } else {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
                break;
            case 7:
            case 8:
            case 0x21:
                if (near == 0) {
                    return;
                }
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
                break;
            }
        }
    }
}

Em3cFunc Em3c_R0_move_tbl[4] = {
    em3c_R0_Init,
    em3c_R0_Move,
    em3c_R0_Damage,
    em3c_R0_Die,
};

static Em3cFunc Em3c_R1_move_tbl[8] = {
    em3c_R1_StartWait,
    em3c_R1_AtkWait,
    em3c_R1_Wait,
    em3c_R1_Walk,
    em3c_R1_Run,
    em3c_R1_Turn180,
    em3c_R1_MoveAtk,
    em3c_R1_CoreAtk,
};

static Em3cFunc Em3c_R2_move_tbl[3] = {
    em3c_R1_Dm_Normal,
    em3c_R1_Dm_Big,
    em3c_R1_Dm_Head,
};

static Em3cFunc Em3c_R3_move_tbl[1] = {
    em3c_R1_Die_Normal,
};

// Attacks (em3cAtkCk): [0] grab, [1] kick, [2] the parasite bite.
static EmAtkInfo em3c_atk_tbl[3] = {
    { 400.0f, PL_DM_AUTO, 0x30C, 0, 0xA, 0 },
    { 400.0f, PL_DM_AUTO, 0x17C, 0, 0xA, 0 },
    { 1000.0f, PL_DM_AUTO, 0x30C, 0, 0xA, 0 },
};

// Head parts that fall apart (em3cPartsBombSet) and the frames each waits before it starts.
static u8 em3c_bomb_parts[25] = {
    0x01, 0x03, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x1B, 0x1C, 0x1D, 0x1E,
};
static u16 em3c_bomb_time[25] = {
    3, 2, 6, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 4, 5, 6, 7, 4, 5, 6, 5, 5, 5, 4,
};
// Rest distances between the five points of each piece kind (em3c_R0_Init fills it).
static f32 em3c_bomb_dist[5][5][5] = { 0.0f };
// The five corner points of each piece kind in parts space.
Vec em3c_bomb_pt[5][5] = {
    { { 0.0f, 0.0f, 300.0f }, { 0.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 0.0f }, { -300.0f, 0.0f, 0.0f }, { 0.0f, 300.0f, 0.0f } },
    { { 0.0f, 0.0f, 300.0f }, { 0.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 0.0f }, { -300.0f, 0.0f, 0.0f }, { 0.0f, 600.0f, 0.0f } },
    { { 0.0f, 0.0f, 200.0f }, { 0.0f, 0.0f, -200.0f }, { 200.0f, 0.0f, 0.0f }, { -200.0f, 0.0f, 0.0f }, { 0.0f, -500.0f, 0.0f } },
    { { 0.0f, 0.0f, 200.0f }, { 0.0f, 0.0f, -200.0f }, { 200.0f, 0.0f, 0.0f }, { -200.0f, 0.0f, 0.0f }, { 0.0f, -600.0f, 0.0f } },
    { { 0.0f, 0.0f, 2000.0f }, { 0.0f, 0.0f, -500.0f }, { 200.0f, 0.0f, 0.0f }, { -200.0f, 0.0f, 0.0f }, { 0.0f, 300.0f, 0.0f } },
};
// The original link 8-aligns the end of .data (the ngcld BSS tag follows): the 4 pad bytes after the table.
asm(".section .data\n\t.balign 8\n\t.text");

// Per-frame update from the enemy manager. Order: damage check, clear the per-frame Be_flg bits,
// tick the wait timers (a dead player forces at least a 5-frame attack wait), route to the player,
// the routine table, parts / attack / collision / stage-collision passes (HoseiCnt counts frames
// the collision cut the movement in half, em3cDoorOpenCk keys on it), the falling head pieces, the
// 45-frame hide of head parts 3 / 4 after the burst, footsteps, the parasite's periodic voice and
// the parasite hit box (hit[10]) placed at the core's head part in part-2 space.
void cEm3c::move()
{
    Em3cWork* w = EM3C_WK(this);
    f32 len;

    if (r_no_0) {
        em3cDmCk(this);
    }
    w->Be_flg &= ~0x707;
    if (w->timer74) {
        w->timer74--;
    }
    if (w->Atk_wait) {
        w->Atk_wait--;
    }
    if (w->Run_wait) {
        w->Run_wait--;
    }
    if (EmDeadCk(pPL)) {
        if (w->Atk_wait <= 4) {
            w->Atk_wait = 5;
        }
    }
    em3cRouteCk(this);
    Em3c_R0_move_tbl[r_no_0](this);
    partsWorldCalc();
    len = VEC_DISTXZ(&pos_old, &pos);
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (VEC_DISTXZ(&pos, &pos_old) < len * 0.5f) {
        w->HoseiCnt++;
    } else {
        w->HoseiCnt = 0;
    }
    em3cPartsBombControl(this);
    if (w->HeadOffTimer) {
        w->HeadOffTimer--;
        if (w->HeadOffTimer == 0) {
            cParts* p;

            p = getPartsPtr(3);
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
            p = getPartsPtr(4);
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
    }
    em3cFootSe(this);
    if (w->pCore) {
        if (w->Core_se_wait) {
            w->Core_se_wait--;
        } else if (hp > 0) {
            w->Core_se_wait = 30;
            switch (type) {
            case 0:
            case 1:
            default:
                SndCall(8, 0x3A, &pos, id, 0, this);
                break;
            case 2:
            case 3:
                SndCall(8, 0x30, &pos, id, 0, this);
                break;
            }
        }
        if (w->pCore && w->pCore->pList && w->pCore->isAlive()) {
            Mtx inv;
            Vec v;
            cParts* p;

            PSMTXInverse(getPartsPtr(2)->mat, inv);
            switch (type) {
            case 0:
            case 1:
            default:
                p = w->pCore->getPartsPtr(0x15);
                break;
            case 2:
            case 3:
                p = w->pCore->getPartsPtr(9);
                break;
            }
            PSMTXMultVec(inv, &p->world, &v);
            w->hit[10].offset = v;
            w->hit[10].flag |= 1;
        } else {
            w->hit[10].flag &= ~1;
        }
    } else {
        w->hit[10].flag &= ~1;
    }
}

// Routine 0: one-time setup on the first frame. Loads the models (em3cModelInit), light, a 1.2 m
// wide collision, the body hit box plus 11 extra ones (arms, legs, [9] the parasite, [10] the head
// object; both disabled until em3cSetParasite), the effect data, the work (Head_hp = hp_max / 14 plus
// 1..25, Head_cnt 1..3 head hits before the head-damage flinch), the motion set (`female` for types
// 1 / 3) and the piece rest distances for the head burst. `set` picks the start: 0 active and
// waiting, 1 a lying dummy that rises (StartWait), 2 an ambush pose that grabs the player when they
// come near (AtkWait). Falls through to em3c_R0_Move.
static void em3c_R0_Init(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    int zero = 0;
    u32 i;
    u32 j;
    u32 k;
    u8 no;

    em3cModelInit(em);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->atari.init(0.0f, -900.0f, 0.0f, 550.0f, 450.0f, 450.0f, 1800.0f, 1, 0x2000, 10);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 200.0f, 250.0f, 2, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 150.0f, 100.0f, 5, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[1], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[2], -300.0f, 0.0f, 0.0f, 100.0f, 300.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 300.0f, 0xF, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[5], 0.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x13, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[6], 0.0f, -400.0f, 0.0f, 130.0f, 400.0f, 0x14, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[7], 0.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x17, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[8], 0.0f, -400.0f, 0.0f, 130.0f, 400.0f, 0x18, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[9], 0.0f, 300.0f, 0.0f, 150.0f, 50.0f, 3, 0);
    YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 3, 0);
    EspDataLoad((u32) ARC(4), EFF_EM3C, 0);
    w->Be_flg = zero;
    w->Core_se_wait = 60;
    w->Run_wait = 300;
    w->HeadOffTimer = zero;
    w->timer74 = zero;
    w->Atk_wait = zero;
    w->Set_pos = em->pos;
    w->Set_ang = em->ang;
    w->Head_hp = (s16) (em->hp_max / 14) + Rnd() % 25 + 1;
    w->Head_cnt = Rnd() % 3 + 1;
    if (em->type != 1 && em->type != 3) {
        w->Wep_type = zero;
    } else {
        w->Wep_type = 1;
    }
    switch (em->type) {
    default:
        w->Armor_type = 0;
        break;
    case 2:
    case 3:
        w->Armor_type = 1;
        break;
    }
    w->EffKindIdCore = EspPullCoreKind();
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            for (k = 0; k < 5; k++) {
                if (j == k) {
                    em3c_bomb_dist[i][j][k] = 0.0f;
                } else {
                    em3c_bomb_dist[i][j][k] = GetDistance3(&em3c_bomb_pt[i][j], &em3c_bomb_pt[i][k]);
                }
            }
        }
    }
    no = em->set;
    switch (no) {
    case 0:
    default:
        em->setStatus(EM_STATUS_ACTIVE);
        EmRoutineSet(em, 1, 2, 0, 0);
        break;
    case 1:
        em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
        em->atari.off();
        w->Be_flg |= 0x400;
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 2:
        em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
        em->atari.setPriority(PRI_LV2);
        em->atari.offSca();
        em->be_flag |= 0x10000;
        w->Be_flg |= 0x400;
        EmRoutineSet(em, 1, 1, 0, 0);
        break;
    }
    if (w->Wep_type) {
        MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 0, 1, 0);
    } else {
        MotionSetCore(em, MOTION(em), ARC(0x10), 0, 0, 1, 0);
    }
    MotionMove(em, 0);
    em3c_R0_Move(em);
}

// Routine 1: dispatches the behaviour sub-routine (r_no_1: StartWait, AtkWait, Wait, Walk, Run,
// Turn180, MoveAtk, CoreAtk).
static void em3c_R0_Move(cEm3c* em)
{
    Em3c_R1_move_tbl[em->r_no_1](em);
}

// Routine 1/0 (set 1): the enemy lies as a harmless dummy (hp forced to 1000, collision passed
// through, Be_flg 0x400 blocks damage) until the level script raises flag bit 0; then it plays the
// rising motion at full HP, marks the player found and goes to Turn180 or Walk by the route angle.
static void em3c_R1_StartWait(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->Be_flg |= 0x400;
    switch (em->r_no_2) {
    case 0:
        em->hp = 1000;
        em->r_no_2++;
    case 1:
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x33), ARC(0x34), 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x1A), 0, 1, 0);
        }
        MotionMove(em, 0);
        if (!(em->flag & 1)) {
            break;
        }
        em->clearStatus(EM_STATUS_ASHLEY_NO_HELP);
        em->r_no_2++;
    case 2:
        em->setStatus(EM_STATUS_ACTIVE);
        em->hp = em->hp_max;
        w->Be_flg |= 0x80;
        em->atari.on();
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x33), ARC(0x34), 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x19), ARC(0x1A), 0, 1, 0);
        }
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->Go_rot > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/1 (set 2): the ambush grab. Steps: 0 idle pose, 1 wait until the living player is
// within 3 m, 2 kill the enemy as an enemy (hp 0, EmSetDie: it is a one-shot scripted grab), take
// over the player (plemSurprised) and a partner within 5 m (subemSurprised), suspend the other
// enemies (em3cAtkSuspend), 3 a 30-frame hold after which the action-button variant (TmpU32 1 / 2)
// is rolled, 4 the grab motion with its effect and a 60-frame window, 5 the grab hits through
// em3cAtkCk2 (Atk_ck -> Act_ck and flag bit 1, which releases the partner), a foot dust effect on
// motion event 4, and when the window ends the suspend is lifted and the enemy dies (routine 3,
// r_no_3 1: the "grab" death effect). While the player has not escaped, the action button prompt
// 0x25 is offered and runs plemEscapeAction.
static void em3c_R1_AtkWait(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->Be_flg |= 0x400;
    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x47), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x45), 0, 0, 5, 0);
        }
        w->TmpU32 = 0;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (em->l_pl > 9000000.0f) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        em->r_no_2++;
    case 2:
        em->hp = 0;
        EmSetDie(em);
        em->atari.off();
        SetPlDamage(em, plemSurprised);
        if (pSUB) {
            f32 d = (em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) + (em->pos.y - pSUB->pos.y) * (em->pos.y - pSUB->pos.y)
                    + (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z);

            if (pSUB->hp > 0 && d < 25000000.0f) {
                SetSubDamage(em, subemSurprised);
            }
        }
        em3cAtkSuspend(em, 1);
        w->Timer = 30;
        em->r_no_2++;
    case 3:
        MotionMove(em, 0);
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                w->TmpU32 = (Rnd() & 1) + 1;
            }
            break;
        }
        em->r_no_2++;
        break;
    case 4:
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x2F), ARC(0x30), 0, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM3C, 0x23, 0, ESP_CORE_KIND_NONE, em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x15), ARC(0x16), 0, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM3C, 0x20, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        w->Timer = 60;
        w->Act_ck = 0;
        em->r_no_2++;
    case 5:
        MotionMove(em, 0);
        em3cAtkCk2(em, 2);
        if (em->Motion.Seq_old.Free & 1) {
            w->TmpU32 = 0;
        }
        if (w->Atk_ck) {
            w->Act_ck = 1;
            em->flag |= 2;
        }
        if (em->Motion.Seq_old.Free & 4) {
            Vec v;

            if (w->Wep_type) {
                v.x = 100.0f;
                v.y = 0.0f;
                v.z = 1000.0f;
            } else {
                v.x = 100.0f;
                v.y = 0.0f;
                v.z = 1150.0f;
            }
            PSMTXMultVec(em->mat, &v, &v);
            EstSet(0, -1, &v, &em->ang, EFF_EM3C, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        if (w->Timer) {
            w->Timer--;
        } else {
            em3cAtkSuspend(em, 0);
            EmRoutineSet(em, 3, 0, 0, 1);
        }
        break;
    }
    if (w->Act_ck == 0) {
        switch (w->TmpU32) {
        case 1:
            ActBtn.set(ACT_GUARD, 5, (void*) plemEscapeAction, em, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_L_R, ACT_FUNC_NORMAL, 0);
            break;
        case 2:
            ActBtn.set(ACT_GUARD, 5, (void*) plemEscapeAction, em, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_A_B, ACT_FUNC_NORMAL, 0);
            break;
        }
    }
}

// Player damage callback (SetPlDamage) for the ambush grab: puts the player 1.8 m in front of the
// enemy facing it, plays the surprised motion from the enemy's archive (0x64 for Ashley, 0x63
// otherwise) with the collision passed through for 5 frames, and ends the damage when it finishes.
// The player's sub archive is swapped to the enemy's for the duration of the call.
static void plemSurprised(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0: {
        Vec v;

        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1800.0f;
        PSMTXMultVec(pl->pEmCatch->mat, &v, &pl->pos);
        pl->ang.y = GetXZAngle(&pl->pos, &pl->pEmCatch->pos);
        pl->atari.off();
        if (pG->pl_type == 1) {
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x64), 0, 3, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), EM_ARC(pl, 0x63), 0, 3, 1, 0);
        }
        pl->m_Work0 = 5;
        pl->r_no_2++;
    }
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else {
            pl->atari.on();
        }
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Action button handler (ActBtn.set in em3c_R1_AtkWait): the player broke the grab. Clears the
// prompt, marks the escape (Act_ck), switches the player damage to plemEscape (dmType 2: no hit
// damage), sits a partner within 4 m down (subemSit) and awards a critical-hit rank point.
static void plemEscapeAction(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->TmpU32 = 0;
    w->Act_ck = 1;
    pPL->dmg.m_Timer = 2;
    SetPlDamage(em, plemEscape);
    if (pSUB) {
        f32 d = (em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) + (em->pos.y - pSUB->pos.y) * (em->pos.y - pSUB->pos.y)
                + (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z);

        if (pSUB->hp > 0 && d < 16000000.0f) {
            SetSubDamage(em, subemSit);
        }
    }
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player damage callback for the grab escape: plays the break-free motion (archive 0x65 for
// Ashley with her voice, 0x60 otherwise with the struggle sounds) with per-frame voice / foley cues,
// and ends the damage when it finishes. The sub archive is swapped like in plemSurprised.
static void plemEscape(cPlayer* pEm)
{
    pEm->subArc = pEm->pEmCatch->subArc;
    pEm->dmg.m_Timer = 2;
    switch (pEm->r_no_2) {
    case 0:
        if (pG->pl_type == 1) {
            MotionSetCore(pEm, MOTION(pEm), EM_ARC(pEm, 0x65), 0, 3, 1, 0);
            EstSet(pEm, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, pEm, 0);
            SndCall(1, 5, &pEm->pos, pEm->id, 0, pEm);
        } else {
            MotionSetCore(pEm, MOTION(pEm), EM_ARC(pEm, 0x60), 0, 3, 1, 0);
            EstSet(pEm, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, pEm, 0);
            SndCall(1, 0x43, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
            SndCall(1, 0x44, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        }
        pEm->atari.on();
        pEm->r_no_2++;
    case 1:
        if (pG->pl_type == 1) {
            if (pEm->Motion.Seq_frame > 24.7f && pEm->Motion.Seq_frame < 25.3f) {
                SndCall(5, 5, &pEm->pos, 0, 0, pEm);
            }
            if (pEm->Motion.Seq_frame > 60.7f && pEm->Motion.Seq_frame < 61.3f) {
                SndCall(1, 6, &pEm->pos, pEm->id, 0, pEm);
                SndCall(1, 0x12, &pEm->pos, pEm->id, 0, pEm);
            }
        } else {
            if (pEm->Motion.Seq_frame > 10.7f && pEm->Motion.Seq_frame < 11.3f) {
                SndCall(1, 0x4F, &pEm->pos, 0, 0, pEm);
            }
            if (pEm->Motion.Seq_frame > 21.7f && pEm->Motion.Seq_frame < 22.3f) {
                SndCall(5, 0x14, &pEm->pos, 0, 0, pEm);
            }
            if ((pEm->Motion.Seq_frame > 36.7f && pEm->Motion.Seq_frame < 37.3f) || (pEm->Motion.Seq_frame > 49.7f && pEm->Motion.Seq_frame < 50.3f)) {
                SndCall(5, 2, &pEm->pos, 0, 0, pEm);
            }
            if ((pEm->Motion.Seq_frame > 37.7f && pEm->Motion.Seq_frame < 38.3f) || (pEm->Motion.Seq_frame > 50.7f && pEm->Motion.Seq_frame < 51.3f)) {
                SndCall(5, 3, &pEm->pos, 0, 0, pEm);
            }
        }
        if (MotionMove(pEm, 0)) {
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Partner damage callback (SetSubDamage) for the ambush grab: places the partner beside the
// player (0.4 m right, 2.1 m ahead of the enemy), plays her surprised pose with the collision off and
// holds it up to 50 frames or until the grab connects (enemy flag bit 1), then the recoil motion with
// its effect and voice, and ends when it finishes.
static void subemSurprised()
{
    cSubChar* sub = SUB_CHAR();

    sub->subArc = sub->pEmCatch->subArc;
    sub->dmg.m_Timer = 2;
    switch (sub->r_no_2) {
    case 0: {
        Vec v;

        v.x = 400.0f;
        v.y = 0.0f;
        v.z = 2100.0f;
        PSMTXMultVec(sub->pEmCatch->mat, &v, &sub->pos);
        sub->ang.y = GetXZAngle(&sub->pos, &sub->pEmCatch->pos);
        sub->ang.y += -0.17453292f;
        sub->ang.y = LIMIT_ANGLE(sub->ang.y);
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x64), 0, 3, 0x101, 0);
        sub->atari.off();
        sub->m_Work0 = 50;
        sub->r_no_2++;
    }
    case 1:
        MotionMove(sub, 0);
        if (!(sub->pEmCatch->flag & 2) && sub->m_Work0) {
            sub->m_Work0--;
        } else {
            sub->r_no_2++;
        }
        break;
    case 2:
        sub->atari.on();
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x65), 0, 3, 0x101, 0);
        EstSet(sub, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, sub, 0);
        SndCall(8, 4, &sub->pos, sub->id, 0, sub);
        sub->r_no_2++;
    case 3:
        if (sub->Motion.Seq_frame > 24.7f && sub->Motion.Seq_frame < 25.3f) {
            SndCall(5, 5, &sub->pos, 0, 0, sub);
        }
        if (MotionMove(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
    sub->subArc = sub->subArc2;
}

// Partner damage callback after the player escapes the grab: the partner drops to a sit (archive
// 0x43), stays there (0x44) while her r_no_3 is set, then stands up (0x45) and the damage ends.
static void subemSit()
{
    cSubChar* sub = SUB_CHAR();

    sub->dmg.m_Timer = 2;
    switch (sub->r_no_2) {
    case 0:
        sub->atari.on();
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x43), 0, 3, 1, 0);
        SndCall(8, 4, &sub->pos, sub->id, 0, sub);
        sub->r_no_2++;
    case 1:
        if (MotionMove(sub, 0)) {
            sub->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x44), 0, 3, 1, 0);
        sub->r_no_2++;
    case 3:
        if (MotionMove(sub, 0) && sub->r_no_3 == 0) {
            sub->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(sub, MOTION(sub), EM_ARC(sub, 0x45), 0, 3, 1, 0);
        sub->r_no_2++;
    case 5:
        if (MotionMove(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
}

// Routine 1/2: idle. Plays the standing loop and runs em3cFindCk; once the player is found, the
// attack wait is over and em3cStayCk lets it go, it turns (route angle over 120 deg) or walks.
static void em3c_R1_Wait(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 10, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 10, 1, 0);
        }
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        em3cFindCk(em);
        if ((w->Be_flg & 0x80) && em3cStayCk(em) == 0 && w->Atk_wait == 0) {
            if (w->Go_rot > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/3: walk towards the route point (r_no_3 = start phase of the loop in 1/256ths; yaw
// turns at PI/64 per frame). Leaves to Wait when the motion loops and em3cStayCk holds it back, or
// during the attack wait within 2.5 m. Within 2 m and facing the player it attacks: the kick / grab
// (MoveAtk), or on types 2 / 3 a coin flip for the parasite bite (CoreAtk) when the core can bite.
// Otherwise it turns around past 120 deg, and breaks into a run on Game_level above 9, or with a
// 50 % roll when the route to the player is over 7 m (Run_wait suppresses the reroll 150 frames).
// Opens doors on its way (em3cDoorOpenCk).
static void em3c_R1_Walk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type) {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x2B))->maxFrame & 0x3FFF) * (f32) em->r_no_3 / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x2B), ARC(0x2C), 10, 5, fr);
        } else {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x11))->maxFrame & 0x3FFF) * (f32) em->r_no_3 / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x11), ARC(0x12), 10, 5, fr);
        }
        em->r_no_2++;
    case 1:
        if (em->Motion.Hokan_cnt == 0) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, PI / 64.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0) && em3cStayCk(em)) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (w->Atk_wait && em->l_pl < 6250000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        {
            Vec pl;
            f32 d;

            GetPlPos(&pl, 18.0f, 0);
            d = (em->pos.x - pl.x) * (em->pos.x - pl.x) + (em->pos.z - pl.z) * (em->pos.z - pl.z);
            if (fabsf(Muku(&em->pos, &pl, em->ang.y, PI)) < PI / 2.0f && d < 4000000.0f) {
                switch (em->type) {
                case 0:
                case 1:
                default:
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                case 2:
                case 3:
                    if (w->pCore && (Rnd() & 1) && w->pCore->ckAtkEnable()) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 6, 0, 0);
                    }
                    break;
                }
            } else if (w->Go_rot > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else if (pG->Game_level > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->L_pl_route > 7000.0f && w->Run_wait == 0 && pG->Game_level > 1) {
                if ((u8) (Rnd() % 10) > 4) {
                    w->Run_wait = 150;
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        }
        break;
    }
    em3cDoorOpenCk(em);
}

// Routine 1/4: run towards the route point (yaw at PI/48 per frame). At each loop end it drops to a
// walk within 3 m of the player when a route exists, or to Wait when em3cStayCk holds it back; the
// attack wait within 2.5 m also stops it. The attack / turn choice is the same as em3c_R1_Walk.
static void em3c_R1_Run(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type) {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x2D))->maxFrame & 0x3FFF) * (f32) em->r_no_3 / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x2D), ARC(0x2E), 5, 5, fr);
        } else {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x13))->maxFrame & 0x3FFF) * (f32) em->r_no_3 / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x13), ARC(0x14), 5, 5, fr);
        }
        em->r_no_2++;
    case 1:
        if (em->Motion.Hokan_cnt == 0) {
            em->ang.y += Muku(&em->pos, &w->Go_pos, em->ang.y, PI / 48.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (em->l_pl < 9000000.0f && (w->Be_flg & 1)) {
                EmRoutineSet(em, 1, 3, 0, 0);
                break;
            }
            if (em3cStayCk(em)) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        if (w->Atk_wait && em->l_pl < 6250000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        {
            Vec pl;
            f32 d;

            GetPlPos(&pl, 18.0f, 0);
            d = (em->pos.x - pl.x) * (em->pos.x - pl.x) + (em->pos.z - pl.z) * (em->pos.z - pl.z);
            if (fabsf(Muku(&em->pos, &pl, em->ang.y, PI)) < PI / 2.0f && d < 4000000.0f) {
                switch (em->type) {
                case 0:
                case 1:
                default:
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                case 2:
                case 3:
                    if (w->pCore && (Rnd() & 1) && w->pCore->ckAtkEnable()) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 6, 0, 0);
                    }
                    break;
                }
            } else if (w->Go_rot > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            }
        }
        break;
    }
    em3cDoorOpenCk(em);
}

// Routine 1/5: the about-face motion. TmpF starts at yaw + PI and, while motion event bit 3 is
// set, both it and the yaw steer towards the route point at PI/32 per frame. Ends in Wait or Walk.
static void em3c_R1_Turn180(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x35), ARC(0x36), 5, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1B), ARC(0x1C), 5, 1, 0);
        }
        w->TmpF = em->ang.y + PI;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            f32 a = Muku(&em->pos, &w->Go_pos, w->TmpF, PI / 32.0f);

            w->TmpF += a;
            w->TmpF = LIMIT_ANGLE(w->TmpF);
            em->ang.y += a;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (em3cStayCk(em)) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/6: the melee attack. Picks the lunging kick (`far`, Atk_type 1: only against Leon-type
// players, when the player is more than 15 deg off the facing, with a 50 % roll) or the grab swing
// (Atk_type 0, which keeps homing on the route point for Timer frames: 10 / 20 / 30 by Game_level).
// The hit is em3cAtkCk2 along the weapon part; a miss awards the player an escape rank point, and
// a hit or a player within 2.5 m sets Atk_wait (45 / 60 / 75 frames by Game_level) before Wait, else
// it walks on. A foot dust effect plays on motion event 4.
static void em3c_R1_MoveAtk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec pl;
    Vec v;

    switch (em->r_no_2) {
    case 0: {
        f32 ang;

        GetPlPos(&pl, 18.0f, 0);
        ang = fabsf(Muku(&em->pos, &pl, em->ang.y, PI));
        int far = 1;
        if (ang < PI / 12.0f) {
            far = 0;
        }
        if (pG->pl_type == 1) {
            far = 0;
        }
        if ((u8) (Rnd() % 10) > 4) {
            far = 0;
        }
        w->Timer = 20;
        if (pG->Game_level <= 2) {
            w->Timer = 10;
        }
        if (pG->Game_level > 7) {
            w->Timer = 30;
        }
        if (w->Wep_type) {
            if (far) {
                MotionSetCore(em, MOTION(em), ARC(0x39), ARC(0x3A), 5, 5, 0);
                w->Timer = 0;
                w->Atk_type = 1;
                EstSet(em, -1, 0, 0, EFF_EM3C, 0x25, 0, ESP_CORE_KIND_NONE, em, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x37), ARC(0x38), 5, 5, 0);
                w->Atk_type = 0;
                EstSet(em, -1, 0, 0, EFF_EM3C, 0x24, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        } else {
            if (far) {
                MotionSetCore(em, MOTION(em), ARC(0x1F), ARC(0x20), 5, 5, 0);
                w->Timer = 0;
                w->Atk_type = 1;
                EstSet(em, -1, 0, 0, EFF_EM3C, 0x22, 0, ESP_CORE_KIND_NONE, em, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1D), ARC(0x1E), 5, 5, 0);
                w->Atk_type = 0;
                EstSet(em, -1, 0, 0, EFF_EM3C, 0x21, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        }
        w->Atk_ck = 0;
        em->r_no_2++;
    }
    case 1:
        if (w->Timer) {
            w->Timer--;
            em->ang.y += Muku(&em->pos, &w->Pl_pos, em->ang.y, PI / 32.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->Atk_ck != 0 || em->l_pl < 6250000.0f) {
                w->Atk_wait = 45;
                if (pG->Game_level <= 3) {
                    w->Atk_wait = 60;
                }
                if (pG->Game_level <= 1) {
                    w->Atk_wait = 75;
                }
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else {
            em3cAtkCk2(em, w->Atk_type);
            if (em->Motion.Seq_old.Free & 4) {
                if (w->Wep_type) {
                    v.x = 100.0f;
                    v.y = 0.0f;
                    v.z = 1000.0f;
                } else {
                    v.x = 100.0f;
                    v.y = 0.0f;
                    v.z = 1150.0f;
                }
                PSMTXMultVec(em->mat, &v, &v);
                EstSet(0, -1, &v, &em->ang, EFF_EM3C, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/7: the parasite head attacks. Types 0 / 1 (r_no_3 0) stand idle for 120 frames while
// the core runs its attack (setAtk); types 2 / 3 (r_no_3 1) play the lean-in motion once while the
// core does its critical bite (setCritical). The core reports the hit (ckAtkHit -> Atk_ck); the
// exit is like em3c_R1_MoveAtk (attack wait then Wait on a hit, Walk otherwise).
static void em3c_R1_CoreAtk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0:
        switch (em->type) {
        case 0:
        case 1:
        default:
            em->r_no_3 = 0;
            if (w->Wep_type) {
                MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x10), 0, 10, 1, 0);
            }
            break;
        case 2:
        case 3:
            em->r_no_3 = 1;
            if (w->Wep_type) {
                MotionSetCore(em, MOTION(em), ARC(0x43), 0, 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x29), 0, 10, 1, 0);
            }
            break;
        }
        if (w->pCore) {
            switch (em->type) {
            case 0:
            case 1:
            default:
                w->pCore->setAtk(0);
                break;
            case 2:
            case 3:
                w->pCore->setCritical();
                break;
            }
        }
        w->Timer = 120;
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1: {
        int end = 0;
        u32 ret = MotionMove(em, 0);

        if (em->r_no_3 == 0) {
            if (w->Timer) {
                w->Timer--;
            } else {
                end = 1;
            }
        } else if (ret) {
            end = 1;
        }
        if (w->pCore && w->pCore->ckAtkHit()) {
            w->Atk_ck = 1;
        }
        if (end) {
            if (w->Atk_ck) {
                w->Atk_wait = 45;
                if (pG->Game_level <= 3) {
                    w->Atk_wait = 60;
                }
                if (pG->Game_level <= 1) {
                    w->Atk_wait = 75;
                }
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
    }
}

// Player damage callback for the kick (em3cAtkCk, attack 1): plays the knock-down motion from the
// enemy's archive (0x61 / 0x62; r_no_3 set by em3cAtkCk picks the mirrored variant, flag 0x41) with
// the pain face and sound, and hands control back when it finishes. dmg is set to type 2 while it
// plays and to 0xF (knocked down) on exit.
static void plemDmMStar(cPlayer* pEm)
{
    pEm->subArc = pEm->pEmCatch->subArc;
    pEm->dmg.set(0, 2);
    switch (pEm->r_no_2) {
    case 0: {
        int flag = 1;

        if (pEm->r_no_3) {
            flag = 0x41;
        }
        MotionSetCore(pEm, MOTION(pEm), EM_ARC(pEm, 0x61), EM_ARC(pEm, 0x62), 3, flag, 0);
        PlSetFace(1);
        PlSetDamageSe(0);
        pEm->r_no_2++;
    }
    case 1:
        if (MotionMove(pEm, 0)) {
            EndPlDamage();
            pEm->dmg.set(0, 0xF);
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Routine 2: damage reactions (r_no_1: Dm_Normal, Dm_Big, Dm_Head). Be_flg 0x100 tells em3cDmCk
// not to restart a flinch while one plays.
static void em3c_R0_Damage(cEm3c* em)
{
    EM3C_WK(em)->Be_flg |= 0x100;
    Em3c_R2_move_tbl[em->r_no_1](em);
}

// Routine 2/0: the light flinch. Picks the motion by the side the hit came from (front / back, or
// the heavy stagger on a coin flip) and, when it ends, walks on or runs (Game_level above 9, or a 50 %
// roll when the route to the player is over 7 m and Run_wait is out).
static void em3c_R1_Dm_Normal(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0: {
        f32 ang;
        void* m0;
        void* m1;

        ang = fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, PI));
        int dir = 1;
        if (ang < PI / 2.0f) {
            dir = 0;
        }
        if (Rnd() & 1) {
            dir = 2;
        }
        if (w->Wep_type) {
            switch ((u32) dir) {
            case 0:
            default:
                m0 = ARC(0x3B);
                m1 = ARC(0x3C);
                break;
            case 1:
                m0 = ARC(0x3D);
                m1 = ARC(0x3E);
                break;
            case 2:
                m0 = ARC(0x3F);
                m1 = ARC(0x40);
                break;
            }
        } else {
            switch ((u32) dir) {
            case 0:
            default:
                m0 = ARC(0x21);
                m1 = ARC(0x22);
                break;
            case 1:
                m0 = ARC(0x23);
                m1 = ARC(0x24);
                break;
            case 2:
                m0 = ARC(0x25);
                m1 = ARC(0x26);
                break;
            }
        }
        MotionSetCore(em, MOTION(em), m0, m1, 5, 1, 0);
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (pG->Game_level > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->L_pl_route > 7000.0f && w->Run_wait == 0 && pG->Game_level > 1 && (u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

// Routine 2/1: the heavy stagger. Same exit as em3c_R1_Dm_Normal; while it plays, an enemy below
// half HP that has not set its parasite yet bursts its head 60 frames in (em3cPartsBombHead).
static void em3c_R1_Dm_Big(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->r_no_2) {
    case 0: {
        void* m0;
        void* m1;

        if (w->Wep_type) {
            m0 = ARC(0x3F);
            m1 = ARC(0x40);
        } else {
            m0 = ARC(0x25);
            m1 = ARC(0x26);
        }
        MotionSetCore(em, MOTION(em), m0, m1, 5, 1, 0);
        w->Timer = 60;
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            if (pG->Game_level > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->L_pl_route > 7000.0f && w->Run_wait == 0 && pG->Game_level > 1 && (u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else if (em->hp < em->hp_max / 2 && !(w->Be_flg & 0x800) && w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                em3cPartsBombHead(em);
            }
        }
        break;
    }
}

// Routine 2/2: the head-hit reaction (Be_flg 0x200 keeps further parasite hits from restarting it).
// Plays the head flinch with its sound and, on motion event bit 0 with Head_hp used up, bursts the
// head (em3cPartsBombHead). Same exit as em3c_R1_Dm_Normal.
static void em3c_R1_Dm_Head(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->Be_flg |= 0x200;
    switch (em->r_no_2) {
    case 0:
        if (w->Wep_type) {
            MotionSetCore(em, MOTION(em), ARC(0x41), ARC(0x42), 5, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x27), ARC(0x28), 5, 1, 0);
        }
        SndCall(8, 0x11, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (pG->Game_level > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->L_pl_route > 7000.0f && w->Run_wait == 0 && pG->Game_level > 1 && (u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else if ((em->Motion.Seq_old.Free & 1) && w->Head_hp <= 0) {
            em3cPartsBombHead(em);
        }
        break;
    }
}

// Routine 3: death (only em3c_R1_Die_Normal).
static void em3c_R0_Die(cEm3c* em)
{
    Em3c_R3_move_tbl[em->r_no_1](em);
}

// Routine 3/0: the body falls apart. Step 0 plays the collapse effects (r_no_3 1 = the grab death
// variant), the death sound, unhooks the chainmail model, passes the collision through, deletes the
// parasite core and its effects, and lets every body part drop as a five-point piece
// (em3cPartsBombSet). Step 1 waits 60 frames, then marks the item drop and fades the model out over
// 35 frames (invisible_factor). Step 2 flags the enemy for removal.
static void em3c_R1_Die_Normal(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    // COMPILER-DIFF: #13 (int shape): the EstSet stack zero of the then-arm is a function-scope
    // constant with one use in another block, so sched1 sees the store as a leaf (issued after
    // `mr r3`) and update_equiv_regs moves the `li` next to it, where it takes r0 like the
    // original's rematerialised reload.
    int zero = 0;

    switch (em->r_no_2) {
        // Unreachable loop: its NOTE_INSN_LOOP_END survives in front of the `case 0:` label, so
        // cse does not follow `beq case0` (the label must be preceded by a BARRIER) and the arm
        // does not learn xFE == 0 — the EstSet stack zero is a fresh `li r0, 0`, not the switch register.
        do { } while (0);
    case 0:
        w->Timer = 60;
        if (em->r_no_3) {
            EstSet(em, -1, 0, 0, EFF_EM3C, 5, 0, ESP_CORE_KIND_NONE, em, (void*) zero);
        } else {
            EstSet(em, -1, 0, 0, EFF_EM3C, 0, 0, ESP_CORE_KIND_NONE, em, 0);
            EstSet(em, -1, 0, 0, EFF_EM3C, 3, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        SndCall(8, 4, &em->pos, em->id, 0, em);
        if (w->pChainmail) {
            w->pChainmail->be_flag &= ~8;
        }
        em->atari.off();
        if (w->pCore) {
            w->pCore->clearLostWait();
            EffectEspDelete(0, w->EffKindIdCore, w->pCore, 0);
            EffectEspgenDelete(0, w->EffKindIdCore, w->pCore);
            EffectEfmDelete(0, w->EffKindIdCore, w->pCore);
            w->pCore = 0;
        }
        em3cPartsBombSet(em, 0);
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                em->clearStatus(EM_STATUS_ACTIVE);
                em->setStatus(EM_STATUS_ITEMSET);
                EmSetDropItem(em);
                if (em->r_no_3 == 0) {
                    SndCall(8, 0x3E, &em->pos, em->id, 0, em);
                }
            }
        } else {
            em->clearStatus(EM_STATUS_ACTIVE);
            em->invisible_factor -= 0.02857f;
            em->ot_type = 1;
            if (em->invisible_factor < 0.0f) {
                em->invisible_factor = 0.0f;
                em->be_flag &= ~2;
                em->r_no_2++;
            }
        }
        break;
    case 2:
        em->be_flag |= 0x4000;
        break;
    }
}

// Tests one attack sphere from em3c_atk_tbl[no] at world `pos` against the player (hit bit 0) and
// partner (bit 1), swept from the weapon part's (0x1A) previous position. On a hit: Atk_ck, blood on
// the victim, and for the kick (no 1) on a living player the knock-down takeover (plemDmMStar) with
// the player turned to face towards / away from the enemy (r_no_3 picks the mirrored motion);
// rumble and the hit sound. Returns 1 on a hit.
int em3cAtkCk(cEm3c* em, Vec* pos, int no)
{
    Em3cWork* w = EM3C_WK(em);
    EmAtkInfo* atk = &em3c_atk_tbl[no];
    cParts* p = GetPartsAddr(em->pList, 0x1A);
    int hit = EmAtkHitCk(atk, pos, &p->world_old2, no == 2);

    if (hit) {
        w->Atk_ck = 1;
        if (hit & 1) {
            if (no) {
                EmPlBloodSet2(em, &p->world, 1, 0x31, 6);
            } else {
                EmPlBloodSet2(em, &p->world, 1, 0x31, 0xA);
            }
            if (no == 1 && (s16) pG->pl_life > 0) {
                EstSet(pPL, -1, 0, 0, EFF_EM3C, 9, 0, ESP_CORE_KIND_NONE, pPL, 0);
                SetPlDamage(em, plemDmMStar);
                if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, PI)) < PI / 2.0f) {
                    pPL->ang.y = pPL->ang.y + Muku(&pPL->pos, &em->pos, pPL->ang.y, PI);
                    pPL->r_no_3 = 0;
                } else {
                    pPL->ang.y = pPL->ang.y + Muku(&em->pos, &pPL->pos, pPL->ang.y, PI);
                    pPL->r_no_3 = 1;
                }
            }
        }
        if (hit & 2) {
            if (no) {
                EmSubBloodSet(em, &p->world, 1, 0x31, 6);
            } else {
                EmSubBloodSet(em, &p->world, 1, 0x31, 0xA);
            }
        }
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        SndCall(8, 9, &em->pos, em->id, 0, em);
        return 1;
    }
    return 0;
}

// Runs em3cAtkCk at points 0 / 0.5 / 1 m (and 1.5 m for the first motion set) along the weapon
// part's forward axis, only while motion event bit 0 (the swing) is set and nothing has hit yet.
// Returns 1 on the first hit.
int em3cAtkCk2(cEm3c* em, int no)
{
    Em3cWork* w = EM3C_WK(em);
    cParts* p;
    Vec v;

    if (w->Atk_ck) {
        return 0;
    }
    if (!(em->Motion.Seq_old.Free & 1)) {
        return 0;
    }
    p = GetPartsAddr(em->pList, 0x1A);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (em3cAtkCk(em, &v, no)) {
        return 1;
    }
    p = GetPartsAddr(em->pList, 0x1A);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 500.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (em3cAtkCk(em, &v, no)) {
        return 1;
    }
    p = GetPartsAddr(em->pList, 0x1A);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1000.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (em3cAtkCk(em, &v, no)) {
        return 1;
    }
    if (w->Wep_type == 0) {
        p = GetPartsAddr(em->pList, 0x1A);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(p->mat, &v, &v);
        if (em3cAtkCk(em, &v, no)) {
            return 1;
        }
    }
    return 0;
}

// Every 4th frame (staggered by emset_no), routes towards the player. The goal is a point beside
// the player (0 / +-1.5 / +-2 m sideways by emset_no % 5, scaled by the distance up to 6 m, so a
// group spreads out), pulled back 35 cm in front of any wall between it and the player. RouteCkToPos
// gives Pl_pos (Be_flg bit 0 when a route exists); Pl_dir / Go_dir are the yaw to it,
// L_pl_route the route distance to the player. Debug_flg[0] 0x4000 draws the wall probe.
void em3cRouteCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec ofs;
    Vec top;
    Vec hit;
    Vec nrm;
    f32 spd;

    if (em->hp <= 0) {
        return;
    }
    if ((pG->Frame_cnt & 3) != (em->emset_no & 3)) {
        return;
    }
    spd = SQRTF(em->l_pl);
    if (spd > 6000.0f) {
        spd = 6000.0f;
    }
    spd *= 1.0f / 6000.0f;
    switch (em->emset_no % 5) {
    case 0:
    default:
        ofs.x = 0.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 1:
        ofs.x = spd * 1500.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 2:
        ofs.x = spd * -1500.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 3:
        ofs.x = spd * 2000.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 4:
        ofs.x = spd * -2000.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &ofs, &ofs);
    top = pPL->pos;
    top.y += 500.0f;
    if (SatMgr.hitCheck(&top, &ofs, &hit, 0, 0, 0)) {
        PSVECSubtract(&top, &hit, &nrm);
#line 2600 "D:/Bio4/Prog/em3c.cpp"
        VECNormalize(&nrm, &nrm);
        PSVECScale(&nrm, &nrm, 350.0f);
        PSVECAdd(&hit, &nrm, &ofs);
        if (DbgFlagChk(pG, DBG_RTP_DISP)) {
            Draw_line3d(&top, &hit, 0xFFFFFFFF, 0);
            Draw_line3d(&top, &ofs, 0xFF00FF00, 0);
        }
    }
    top = ofs;
    if (RouteCkToPos(em, &top, &w->Pl_pos, 0, 0)) {
        w->Be_flg |= 1;
    }
    w->Pl_dir = Muku(&em->pos, &w->Pl_pos, em->ang.y, PI);
    w->Pl_rot = fabsf(w->Pl_dir);
    if (em->r_no_0 == 0) {
        w->Pl_dir = 0.0f;
        w->Pl_rot = 0.0f;
        em->l_pl = 100000000.0f;
    }
    w->L_pl_route = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    w->Go_pos = w->Pl_pos;
    w->Go_dir = w->Pl_dir;
    w->Go_rot = w->Pl_rot;
    w->L_go = em->l_pl;
    w->pEm = pPL;
    w->Be_flg &= ~4;
}

// Loads the body model for the type (archive 5 / 6 for types 0 / 1, 9 / 0xA for 2 / 3), the
// chainmail overlay (7 or 0xB with texture 8) and the hand weapon (0xC / 0xD for types 0 / 2,
// 0xE / 0xF for 1 / 3) as attached models, at 1.2x scale.
void em3cModelInit(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Mtx m;
    void* bin;
    cModelInfo* info;

    switch (em->type) {
    case 0:
    default:
        em->modelInit(ARC(5), ARC(6));
        break;
    case 1:
        em->modelInit(ARC(5), ARC(6));
        break;
    case 2:
        em->modelInit(ARC(9), ARC(0xA));
        break;
    case 3:
        em->modelInit(ARC(9), ARC(0xA));
        break;
    }
    w->pChainmail = 0;
    switch (em->type) {
    case 0:
    default:
        bin = ARC(7);
        break;
    case 1:
        bin = ARC(7);
        break;
    case 2:
        bin = ARC(0xB);
        break;
    case 3:
        bin = ARC(0xB);
        break;
    }
    info = ModInfoMgr.create(bin, ARC(8));
    w->pChainmail = info;
    if (info) {
        em->addModel(info);
    }
    PSMTXRotRad(m, 'y', em->ang.y);
    TransMatrix(m, &em->pos);
    w->pWeapon = 0;
    switch (em->type) {
    case 0:
    case 2:
    default:
        w->pWeapon = ModInfoMgr.create(ARC(0xC), ARC(0xD));
        if (w->pWeapon) {
            em->addModel(w->pWeapon);
        }
        break;
    case 1:
    case 3:
        w->pWeapon = ModInfoMgr.create(ARC(0xE), ARC(0xF));
        if (w->pWeapon) {
            em->addModel(w->pWeapon);
        }
        break;
    }
    em->scale.x = 1.2f;
    em->scale.y = 1.2f;
    em->scale.z = 1.2f;
}

// Death collapse: turns each of the 25 body parts in em3c_bomb_parts that is not already falling
// into a five-point piece (motParts flag 0x01000000 marks it as one, 0x25000002 also detaches it from
// the motion). The piece kind picks the corner layout (1 the pelvis, 4 part 0x10, 2 / 3 the limb
// pairs, 0 the rest), the points get a random push, and each piece waits `add` + its table delay
// before em3cPartsBombControl starts dropping it.
void em3cPartsBombSet(cEm3c* em, int add)
{
    u32 i;
    u32 j;
    // pointer locals: the table bases stay first in the `add`/`lhzx` (a symbol operand is
    // swapped behind the index at expand time)
    u16* tm = em3c_bomb_time;
    Vec (*pt)[5] = em3c_bomb_pt;

    for (i = 0; i < 25; i++) {
        int no = em3c_bomb_parts[i];
        cParts* p = (cParts*) em->getPartsPtr(no);
        PBOMB_INFO* b;
        Vec* tbl;
        int kind;

        if (p->motParts.flags & 0x01000000) {
            continue;
        }
        p->motParts.flags |= 0x25000002;
        b = &p->Bomb;
        switch (no) {
        default:
            kind = 0;
            break;
        case 1:
            kind = 1;
            break;
        case 0x10:
            kind = 4;
            break;
        case 0x12:
        case 0x16:
            kind = 2;
            break;
        case 0x11:
        case 0x13:
        case 0x14:
        case 0x17:
        case 0x18:
            kind = 3;
            break;
        }
        tbl = pt[kind];
        for (j = 0; j < 5; j++) {
            PSMTXMultVec(p->mat, &tbl[j], &b->Pos[j]);
            b->Speed[j].x = fRand1_1() * 50.0f;
            b->Speed[j].y = fRand1_1() * 30.0f;
            b->Speed[j].z = fRand1_1() * 50.0f;
        }
        {
            u16* tp = tm + i;
            int t = *tp;
            b->Delay = add + t;
        }
    }
}

// The head bursts (Head_hp used up, or the big flinch below half HP): the head part 3 becomes a
// falling piece pushed 10 cm forward, disabled as a hit box (hit[0]), HeadOffTimer hides parts 3 / 4
// after 45 frames, the burst effect and sound play and the parasite takes its place
// (em3cSetParasite). Be_flg 0x10 makes it a one-shot.
void em3cPartsBombHead(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    cParts* p;
    PBOMB_INFO* b;
    Vec v;
    u32 j;

    w->Head_hp = 0;
    w->hit[0].flag &= ~1;
    if (w->Be_flg & 0x10) {
        return;
    }
    p = (cParts*) em->getPartsPtr(3);
    if (p->motParts.flags & 0x01000000) {
        return;
    }
    p->motParts.flags |= 0x25000002;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 10.0f;
    PSMTXMultVecSR(em->mat, &v, &v);
    b = &p->Bomb;
    for (j = 0; j < 5; j++) {
        PSMTXMultVec(p->mat, &em3c_bomb_pt[0][j], &b->Pos[j]);
        b->Speed[j].x = fRand1_1() * 10.0f + v.x;
        b->Speed[j].y = fRand1_1() * 10.0f + v.y;
        b->Speed[j].z = fRand1_1() * 10.0f + v.z;
    }
    b->Delay = 0;
    w->HeadOffTimer = 45;
    w->Be_flg |= 0x10;
    EstSet(em, -1, 0, 0, EFF_EM3C, 4, 0, ESP_CORE_KIND_NONE, em, 0);
    SndCall(8, 7, &em->pos, em->id, 0, em);
    em3cSetParasite(em);
}

// Per-frame simulation of the falling pieces (em3cPartsBombSet / em3cPartsBombHead), while the
// enemy is displayed. For each falling part past its delay: gravity of 10 mm/frame^2 on the five
// points, five relaxation passes that restore the rest distances (em3c_bomb_dist) and clamp the
// points to the enemy's floor height (hitBits), a bounce (x/z damped to 0.8, y reversed at 40..60 %;
// the head part's first floor hit plays the clang) or the free-flight speed from the moved distance,
// and the part stops falling once the total speed is under 1. The part matrix is rebuilt from the
// points (front and side axes) centred between points 0 / 1; debug_mode 8 draws the pieces.
// Children that are not pieces themselves follow their falling parent.
void em3cPartsBombControl(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec old[5];
    Vec cen;
    Vec a;
    Vec b;
    Vec c;
    cParts* p;
    u32 i;
    // declared right after `i`: gcse creates the PRE'd `bomb+C` / `p+C` / `fp+D` address pseudos in
    // hash-bucket order (hash = K + REGNO + C), and their spill slots follow that order -- `bomb`
    // must be pseudo p+2 (or p+3) for the target's 0xb0/0xb4/0xbc slots
    PBOMB_INFO* bomb;
    u32 n;
    u32 j;
    u32 k;

    if (!(em->be_flag & 2)) {
        return;
    }
    for (i = 0; i < 25; i++) {
        int no = em3c_bomb_parts[i];
        p = (cParts*) em->getPartsPtr(no);
        int kind;
        f32 sum;

        if (!(p->motParts.flags & 0x01000000)) {
            continue;
        }
        switch (no) {
        default:
            kind = 0;
            break;
        case 1:
            kind = 1;
            break;
        case 0x10:
            kind = 4;
            break;
        case 0x12:
        case 0x16:
            kind = 2;
            break;
        case 0x11:
        case 0x13:
        case 0x14:
        case 0x17:
        case 0x18:
            kind = 3;
            break;
        }
        bomb = &p->Bomb;
        if (bomb->Delay) {
            bomb->Delay--;
            continue;
        }
        for (k = 0; k < 5; k++) {
            bomb->Speed[k].y -= 10.0f;
            old[k] = bomb->Pos[k];
            PSVECAdd(&bomb->Pos[k], &bomb->Speed[k], &bomb->Pos[k]);
            bomb->Flag = 0;
        }
        for (n = 0; n < 5; n++) {
            for (j = 0; j < 5; j++) {
                for (k = 0; k < 5; k++) {
                    if (j != k) {
                        f32 s;
                        f32 r;

                        // the length is the routine-scope `sum` (the same pseudo accumulates the speeds
                        // below): a multi-block pseudo that crosses calls, so it takes the callee-saved
                        // f31 (`fmr f31,f1`) and the 0.5/1.0 constants fall to f29/f30 like the target
                        PSVECSubtract(&bomb->Pos[k], &bomb->Pos[j], &cen);
                        sum = PSVECMag(&cen);
                        s = (em3c_bomb_dist[kind][j][k] - sum) * 0.5f;
                        r = 1.0f / sum;
                        PSVECScale(&cen, &cen, r * s);
                        PSVECAdd(&bomb->Pos[k], &cen, &bomb->Pos[k]);
                        PSVECSubtract(&bomb->Pos[j], &cen, &bomb->Pos[j]);
                        if (bomb->Pos[j].y < em->pos.y) {
                            bomb->Pos[j].y = em->pos.y;
                            bomb->Flag |= 1 << j;
                        }
                        if (bomb->Pos[k].y < em->pos.y) {
                            bomb->Pos[k].y = em->pos.y;
                            bomb->Flag |= 1 << k;
                        }
                    }
                }
            }
        }
        sum = 0.0f;
        for (j = 0; j < 5; j++) {
            if ((bomb->Flag >> j) & 1) {
                bomb->Speed[j].x *= 0.8f;
                bomb->Speed[j].y *= fRand0_1() * 0.2f + -0.6f;
                bomb->Speed[j].z *= 0.8f;
                if (no == 3 && (w->Be_flg & 0x50) == 0x10) {
                    SndCall(8, 8, &em->pos, em->id, 0, em);
                    w->Be_flg |= 0x40;
                }
            } else {
                PSVECSubtract(&bomb->Pos[j], &old[j], &bomb->Speed[j]);
                PSVECScale(&bomb->Speed[j], &bomb->Speed[j], 0.999f);
            }
            sum += bomb->Speed[j].x * bomb->Speed[j].x + bomb->Speed[j].y * bomb->Speed[j].y + bomb->Speed[j].z * bomb->Speed[j].z;
        }
        if (sum < 1.0f) {
            p->motParts.flags &= ~0x01000000;
        }
        PSVECSubtract(&bomb->Pos[0], &bomb->Pos[1], &c);
        PSVECSubtract(&bomb->Pos[2], &bomb->Pos[3], &a);
        PSVECCrossProduct(&c, &a, &b);
        PSVECCrossProduct(&b, &c, &a);
#line 2959 "D:/Bio4/Prog/em3c.cpp"
        VECNormalize(&a, &a);
        VECNormalize(&b, &b);
        VECNormalize(&c, &c);
        p->mat[0][0] = a.x;
        p->mat[1][0] = a.y;
        p->mat[2][0] = a.z;
        p->mat[0][1] = b.x;
        p->mat[1][1] = b.y;
        p->mat[2][1] = b.z;
        p->mat[0][2] = c.x;
        p->mat[1][2] = c.y;
        p->mat[2][2] = c.z;
        PSVECAdd(&bomb->Pos[0], &bomb->Pos[1], &cen);
        PSVECScale(&cen, &cen, 0.5f);
        TransMatrix(p->mat, &cen);
        ScaleMatrix(p->mat, &p->scale);
        p->world = cen;
        // pG: the pG load stays behind the `p->worldPos = cen` word stores (global.h)
        if (pG->debug_mode == 8) {
            // the routine's j/k again (and j for the speed loop above, p for the parts walk below):
            // one pseudo per name is what puts j in r24, k in r29 and p in r26 like the target --
            // separate counters rank differently in global alloc and permute the callee-saved set
            for (j = 0; j < 5; j++) {
                for (k = 0; k < 5; k++) {
                    if (j != k) {
                        Draw_line3d(&bomb->Pos[j], &bomb->Pos[k], 0xFFFFFFFF, 0);
                    }
                }
            }
        }
    }
    // the walk reuses `p`: the extra refs rank p above bomb in global alloc (r26/r25)
    p = em->pList;
    while (p) {
        p = p->pList;
        if (p == 0) {
            break;
        }
        if (!(p->motParts.flags & 0x01000000) && (((cParts*) p->pParent)->motParts.flags & 0x01000000)) {
            PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);
            p->world.x = p->mat[0][3];
            p->world.y = p->mat[1][3];
            p->world.z = p->mat[2][3];
        }
    }
}

// Damage of the pending hit: the weapon table value (near = within 6 m for the range falloff; 20
// for weapon ids past 0x2D), tripled on the head part 3.
int em3cSetDmVal(cEm3c* em)
{
    YARARE_INFO* part = em->dmg.m_pDamageYarare;
    int near = 0;
    int dmg;

    if (part->len < 36000000.0f) {
        near = 1;
    }
    dmg = 20;
    if (em->dmg.m_Wep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    if (part->parts_no == 3) {
        dmg *= 3;
    }
    return dmg;
}

// Spawns the parasite that replaces the burst head (Be_flg 0x800): an obj16 core attached 20 cm up
// (types 0 / 1: kind 0xB with four tentacle objects on parts 0x16..0x19 phased a quarter loop apart;
// types 2 / 3: kind 0xD), given its motion set and the player bite motion, with the burst voice /
// effect. Enables the parasite hit box hit[9].
void em3cSetParasite(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec pos;
    Vec rot;
    cObj* obj;
    int frame;
    int female;
    void* bin;
    void* tpl;
    void* m0;
    void* m1;
    void* m2;
    void* m3;
    void* m4;
    void* m5;
    void* m6;
    void* m7;
    void* m8;
    void* m9;
    void* m10;

    w->Be_flg |= 0x800;
    frame = (((MotionData*) ARC(0x4A))->maxFrame & 0x3FFF) / 4;
    switch (em->type) {
    case 0:
    case 1:
    default:
        female = 1;
        bin = ARC(0x4B);
        tpl = ARC(0x4C);
        m0 = ARC(0x4D);
        m1 = ARC(0x4E);
        m2 = ARC(0x52);
        m3 = ARC(0x54);
        m4 = ARC(0x55);
        m5 = ARC(0x55);
        m6 = ARC(0x55);
        m7 = ARC(0x4F);
        m8 = ARC(0x53);
        m9 = ARC(0x50);
        m10 = ARC(0x51);
        break;
    case 2:
    case 3:
        female = 0;
        bin = ARC(0x56);
        tpl = ARC(0x57);
        m0 = ARC(0x58);
        m1 = ARC(0x58);
        m2 = ARC(0x59);
        m3 = ARC(0x5A);
        m4 = ARC(0x5E);
        m5 = ARC(0x5C);
        m6 = ARC(0x5F);
        m7 = ARC(0x5B);
        m8 = ARC(0x5B);
        m9 = ARC(0x5D);
        m10 = ARC(0x5D);
        break;
    }
    pos.x = 0.0f;
    pos.y = 200.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (female) {
        w->pCore = (cObj16*) SetObj16(bin, tpl, em, em, 2, 0xB, &pos, &rot);
    } else {
        w->pCore = (cObj16*) SetObj16(bin, tpl, em, em, 2, 0xD, &pos, &rot);
    }
    if (w->pCore) {
        w->pCore->setMotData(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
        w->pCore->setMotData(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
        w->pCore->setPlDmgMot(ARC(0x61), ARC(0x62));
    }
    if (female) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = -0.6632251f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pCore, 0x16, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, 0);
            w->pTen[0] = (cObj16*) obj;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.61086524f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pCore, 0x17, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, frame);
            w->pTen[1] = (cObj16*) obj;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = -0.5235988f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pCore, 0x18, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, frame * 2);
            w->pTen[2] = (cObj16*) obj;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pCore, 0x19, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, frame * 3);
            w->pTen[3] = (cObj16*) obj;
        }
    }
    SndCall(8, 0x36, &em->getPartsPtr(3)->world, em->id, 0, em);
    if (female) {
        EstSet(em, -1, 0, 0, EFF_EM3C, 0x11, 1, ESP_CORE_KIND_NONE, em, 0);
    } else {
        EstSet(em, -1, 0, 0, EFF_EM3C, 0xD, 1, ESP_CORE_KIND_NONE, em, 0);
    }
    if (w->pCore) {
        EstSet(w->pCore, -1, 0, 0, EFF_EM3C, 0x12, 0, w->EffKindIdCore, w->pCore, 0);
    }
    w->hit[9].flag |= 1;
}

// Plays the footstep at the root part on motion sound events 1 / 2 and consumes the event.
void em3cFootSe(cEm3c* em)
{
    if (em->Motion.Seq_old.Se) {
        if (em->Motion.Seq_old.Se == 1 || em->Motion.Seq_old.Se == 2) {
            em->Motion.Seq_old.Se = 0;
            SndCall(8, 0, &em->getPartsPtr(0)->world, em->id, 0, em);
        }
    }
}

// Crowd control: returns 1 when this enemy should hold back because enough other living, active
// em3c enemies are closer to the player along the route (more than 0 on Game_level up to 2, more than
// 1 otherwise) and it is either within 5 m or has a route to the player.
int em3cStayCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    u32 cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (e->isAlive() && e->id == 0x3C && e->hp > 0 && e != em && e->checkStatus(EM_STATUS_ACTIVE)
            && EM3C_WK(e)->L_pl_route < w->L_pl_route) {
            cnt++;
        }
    }
    if (pG->Game_level <= 2) {
        if (cnt == 0) {
            return 0;
        }
    } else {
        if (cnt <= 1) {
            return 0;
        }
    }
    if (em->l_pl > 25000000.0f && !(w->Be_flg & 1)) {
        return 0;
    }
    return 1;
}

// Player detection while idle; sets Be_flg 0x80 and returns 1 when: a route exists and the player
// is within 60 deg of the facing and 10 m; the alarm bell (Status_flg[1] 0x20000000) rang within 25 m
// of the enemy with the player under 25 m of route away; the global alert (Status_flg[0]
// 0x00800000) is up within 25 m of route; or the player is dead.
int em3cFindCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    if (w->Be_flg & 0x80) {
        return 0;
    }
    if (!(w->Be_flg & 1)) {
        return 0;
    }
    if (w->Pl_rot < 1.0471976f && em->l_pl < 100000000.0f) {
        w->Be_flg |= 0x80;
        return 1;
    }
    if (StaFlagChk(pG, STA_SE_BURST)) {
        f32 r;

        switch (pG->SeInfo.type) {
        case 0:
            r = 25000.0f;
            break;
        case 1:
            r = 25000.0f;
            break;
        default:
            r = 25000.0f;
            break;
        }
        // the override after the switch makes the arm stores dead (flow deletes them, the
        // compares stay) and puts the pool load into the join block; `r` keeps 4 sets so
        // `r * r` is not folded
        r = 25000.0f;
        if ((em->pos.x - pG->SeInfo.pos.x) * (em->pos.x - pG->SeInfo.pos.x) + (em->pos.y - pG->SeInfo.pos.y) * (em->pos.y - pG->SeInfo.pos.y)
                + (em->pos.z - pG->SeInfo.pos.z) * (em->pos.z - pG->SeInfo.pos.z)
            < r * r) {
            if ((w->Be_flg & 1) && w->L_pl_route < r) {
                w->Be_flg |= 0x80;
                return 1;
            }
        }
    }
    if (StaFlagChk(pG, STA_PL_FIRE) && w->L_pl_route < 25000.0f) {
        w->Be_flg |= 0x80;
        return 1;
    }
    if (EmDeadCk(em)) {
        w->Be_flg |= 0x80;
        return 1;
    }
    return 0;
}

// When the collision has been blocking the walk for a while (HoseiCnt at 5 mod 10), looks for an
// intact door enemy (id 0x41) within 2.5 m that the enemy faces within 45 deg from either side and
// stands within 80 cm of, inside its width and +-50 cm of its height, and pushes it open.
void em3cDoorOpenCk(cEm3c* em)
{
    Vec v;
    u32 i;
    f32 ang;

    if (EM3C_WK(em)->HoseiCnt % 10 != 5) {
        return;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEmDoor* e = (cEmDoor*) EmMgr.fastAt(i);
        EmDoorWork* dw;

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0x41) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y)
                + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z)
            > 6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->base_dir, em->ang.y, PI));
        if (ang > PI / 4.0f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->base_im, &em->pos, &v);
        if (ang < PI / 2.0f) {
            if (v.z > 0.0f || v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f || v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->Width || v.x < -dw->Width) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        switch (e->ckOpen()) {
        case 0:
        default:
            e->setOpen(&em->pos, 0, 0, 0);
            break;
        case 1:
        case 2:
        case 3:
            break;
        }
    }
}

// Freezes the rest of the scene during the ambush grab (on = 1) and releases it (on = 0):
// Status_flg[1] 0x10000000 / Status_flg[2] 0x02000000 suspend the world, while the player, partner,
// this enemy and any other em3c still in its start / ambush wait keep running.
void em3cAtkSuspend(cEm3c* em, int on)
{
    u32 i;

    if (on) {
        StaFlagOn(pG, STA_SUSPEND);
        pPL->setNoSuspend(1);
        em->setNoSuspend(1);
        if (pSUB) {
            pSUB->setNoSuspend(1);
        }
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    } else {
        StaFlagOff(pG, STA_SUSPEND);
        pPL->setNoSuspend(0);
        em->setNoSuspend(0);
        if (pSUB) {
            pSUB->setNoSuspend(0);
        }
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (e->isAlive() && e->id == 0x3C && e != em && e->r_no_0 == 1 && e->r_no_1 <= 1) {
            if (on) {
                e->setNoSuspend(1);
            } else {
                e->setNoSuspend(0);
            }
        }
    }
}
