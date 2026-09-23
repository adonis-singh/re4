// em32 module (D:/Bio4/Prog/em32.cpp): the caged boss of the container area. It appears in four
// steps out of its cage (Parasite / LastMode / 2ndAppear .. 4thAppear), then walks the containers
// (em2c's blended walk / dash, StepUp / StepDown, the tunnel and ceiling attacks), catches the
// player (CatchHit / P_CatchHit / C_AtkHit with the plem32_* callbacks) and breaks the barred
// door (BreakBarred). Three forms: `mode` 0, 1 (the parasite shows) and 2 (the last form).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em32.h"
#include "emhit.h"
#include "obj00.h"
#include "obj01.h"
#include "obj.h"
#include "emBarred.h"
#include "embarrel.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
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
#include "db_log.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "dbmodule.h"
#include "foot_shadow.h"
#include "main_mem.h"
#include "em.h"
#include <string.h>
#include <dolphin/os.h>
#include "em_mod.h"
#include "obj20.h"

asm(".comm common_em32,52,4");

int GetWepDmVal(cEm* em, u32 wep_no, int near);   // em10.h (not included: it pulls emwep.h's global plemBackjump)
extern FootShadowTbl Em32_fs_tbl;     // game/foot_shadow_tbl.cpp (static there, so not in foot_shadow.h; the REL link resolves the local symbol)

static void em32_R0_Init(cEm32* em);
static void em32_R0_Move(cEm32* em);
static void em32_R1_br_Dummy(cEm32* em);
static void em32_R1_Parasite(cEm32* em);
static void em32_R1_LastMode(cEm32* em);
static void em32_R1_2ndAppear(cEm32* em);
static void em32_R1_3rdAppear(cEm32* em);
static void em32_R1_3rdFall(cEm32* em);
static void em32_R1_4thAppear(cEm32* em);
static void em32_R1_Wait(cEm32* em);
static void em32_R1_Ambush(cEm32* em);
static void em32_R1_Walk(cEm32* em);
static void em32_R1_Dash(cEm32* em);
static void em32_R1_Back(cEm32* em);
static void em32_R1_AtkWalk(cEm32* em);
static void em32_R1_Turn(cEm32* em);
static void em32_R1_Threat(cEm32* em);
static void em32_R1_AmbushAtk(cEm32* em);
static void em32_R1_Atk(cEm32* em);
static void em32_R1_br_Catch(cEm32* em);
static void em32_R1_Catch(cEm32* em);
static void em32_R1_CatchHit(cEm32* em);
static void plem32_CatchHit(cPlayer* pl);
static void em32_R1_LongAtk(cEm32* em);
static void em32SitAction(cEm32* em);
static void em32SitUpAction(cEm32* em);
static void em32BackjumpAction(cEm32* em);
static void em32EscapeAction(cEm32* em);
static void em32_R1_StepUp(cEm32* em);
static void em32_R1_StepWait(cEm32* em);
static void em32_R1_StepDown(cEm32* em);
static void em32_R1_TunnelAtk(cEm32* em);
static void em32_R1_JumpUp(cEm32* em);
static void em32_R1_JumpDown(cEm32* em);
static void em32_R1_C_Wait(cEm32* em);
static void em32_R1_br_C_Atk(cEm32* em);
static void em32_R1_C_Atk(cEm32* em);
static void em32_R1_C_AtkHit(cEm32* em);
static void plem32_C_AtkHit(cPlayer* pl);
static void em32_R1_P_Atk(cEm32* em);
static void em32_R1_br_P_Catch(cEm32* em);
static void em32_R1_P_Catch(cEm32* em);
static void em32_R1_P_CatchHit(cEm32* em);
static void plem32_P_CatchHit(cPlayer* pl);
static void em32_R1_Ground(cEm32* em);
static void em32_R1_BreakBarred(cEm32* em);
static void em32_R0_Damage(cEm32* em);
static void em32_R1_Dm_Normal(cEm32* em);
static void em32_R0_Die(cEm32* em);
static void em32_R1_Die_Normal(cEm32* em);
static void plemDivide(cPlayer* pl);

// The player callbacks that emwep.h also declares carry C linkage here (the module's own local
// copies keep the unmangled names in the REL symbol table).
extern "C" {
static void plemSit(cPlayer* pl);
static void plemBackjump(cPlayer* pl);
static void plemEscape(cPlayer* pl);
}

#define VIB_TBL ((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore))


// COMPILER-DIFF #12 (cse path knowledge): in a `case` arm reached through the switch's once-used label
// our cse still knows `w == em + 0x3E0` and folds the reference stores of the difficulty tables into
// `em`-based addresses; the original keeps them `w`-based there. The launder hides the equivalence.
#define EM32_W_FRESH(w) asm("" : "+r"(w))

static inline void em32Timer2SetW(Em32Work* w, int a, int b, int c, int e)
{
    if (pG->Game_level <= 3) {
        w->timer2 = a;
    }
    if (pG->Game_level <= 1) {
        w->timer2 = b;
    }
    if (pG->Game_level > 6) {
        w->timer2 = c;
    }
    if (pG->Game_level > 9) {
        w->timer2 = e;
    }
}

// The three effect systems attached to an esp kind are deleted together.
#define EM32_EFFECT_DELETE(kind, em)          \
    EffectEspDelete(0, kind, em, 0);  \
    EffectEspgenDelete(0, kind, em);  \
    EffectEfmDelete(0, kind, em)

// Claws (parts 0x12..0x19) of an attack motion against the player.
#define EM32_CLAW_ATK(em, no)      \
    em32AtkCk(em, no, 0x12);       \
    em32AtkCk(em, no, 0x13);       \
    em32AtkCk(em, no, 0x14);       \
    em32AtkCk(em, no, 0x15);       \
    em32AtkCk(em, no, 0x16);       \
    em32AtkCk(em, no, 0x17);       \
    em32AtkCk(em, no, 0x18);       \
    em32AtkCk(em, no, 0x19)

// Tail (parts 0x54..0x5D) of the last form's attacks.
#define EM32_TAIL_ATK(em, no)      \
    em32AtkCk(em, no, 0x54);       \
    em32AtkCk(em, no, 0x55);       \
    em32AtkCk(em, no, 0x56);       \
    em32AtkCk(em, no, 0x57);       \
    em32AtkCk(em, no, 0x58);       \
    em32AtkCk(em, no, 0x59);       \
    em32AtkCk(em, no, 0x5A);       \
    em32AtkCk(em, no, 0x5B);       \
    em32AtkCk(em, no, 0x5C);       \
    em32AtkCk(em, no, 0x5D)

// Turn towards `target` with the blended walk: the yaw step is filtered into blendVal (which also
// picks the left / right blend motion) and applied as a fraction of PI/32.
#define EM32_BLEND_TURN(em, w, target, lim)                                        \
    {                                                                              \
        f32 ang = Muku(&(em)->pos, target, (em)->ang.y, 3.14159274f);              \
        if (ang > lim) {                                                           \
            ang = lim;                                                             \
        }                                                                          \
        if (ang < -lim) {                                                          \
            ang = -lim;                                                            \
        }                                                                          \
        {                                                                          \
            f32 blend = ang * 324.676086f;                                         \
            (w)->blendVal = (w)->blendVal * 0.899999976f + blend * 0.100000001f;   \
        }                                                                          \
        (em)->ang.y += (w)->blendVal * 0.00392156886f * 0.0981747732f;             \
        (em)->ang.y = LIMIT_ANGLE((em)->ang.y);                                    \
    }

// The same turn with the product written back into the angle variable (`ang *= K`: the multiply is
// tied to the dying Muku result).
#define EM32_BLEND_TURN2(em, w, target, lim)                                       \
    {                                                                              \
        f32 ang = Muku(&(em)->pos, target, (em)->ang.y, 3.14159274f);              \
        if (ang > lim) {                                                           \
            ang = lim;                                                             \
        }                                                                          \
        if (ang < -lim) {                                                          \
            ang = -lim;                                                            \
        }                                                                          \
        ang *= 324.676086f;                                                        \
        (w)->blendVal = (w)->blendVal * 0.899999976f + ang * 0.100000001f;         \
        (em)->ang.y += (w)->blendVal * 0.00392156886f * 0.0981747732f;             \
        (em)->ang.y = LIMIT_ANGLE((em)->ang.y);                                    \
    }

Em32Func Em32_R0_move_tbl[4] = {
    em32_R0_Init,
    em32_R0_Move,
    em32_R0_Damage,
    em32_R0_Die,
};

// Routine 1 table: {branch check, routine} per xFD (a flat table: `[xFD * 2 + 1]`).
static Em32Func Em32_R1_move_tbl[66] = {
    em32_R1_br_Dummy, em32_R1_Parasite,     // 0x00
    em32_R1_br_Dummy, em32_R1_LastMode,     // 0x01
    em32_R1_br_Dummy, em32_R1_2ndAppear,    // 0x02
    em32_R1_br_Dummy, em32_R1_3rdAppear,    // 0x03
    em32_R1_br_Dummy, em32_R1_3rdFall,      // 0x04
    em32_R1_br_Dummy, em32_R1_4thAppear,    // 0x05
    em32_R1_br_Dummy, em32_R1_Wait,         // 0x06
    em32_R1_br_Dummy, em32_R1_Ambush,       // 0x07
    em32_R1_br_Dummy, em32_R1_Walk,         // 0x08
    em32_R1_br_Dummy, em32_R1_Dash,         // 0x09
    em32_R1_br_Dummy, em32_R1_Back,         // 0x0A
    em32_R1_br_Dummy, em32_R1_AtkWalk,      // 0x0B
    em32_R1_br_Dummy, em32_R1_Turn,         // 0x0C
    em32_R1_br_Dummy, em32_R1_Threat,       // 0x0D
    em32_R1_br_Dummy, em32_R1_AmbushAtk,    // 0x0E
    em32_R1_br_Dummy, em32_R1_Atk,          // 0x0F
    em32_R1_br_Catch, em32_R1_Catch,        // 0x10
    em32_R1_br_Dummy, em32_R1_CatchHit,     // 0x11
    em32_R1_br_Dummy, em32_R1_LongAtk,      // 0x12
    em32_R1_br_Dummy, em32_R1_StepUp,       // 0x13
    em32_R1_br_Dummy, em32_R1_StepWait,     // 0x14
    em32_R1_br_Dummy, em32_R1_StepDown,     // 0x15
    em32_R1_br_Dummy, em32_R1_TunnelAtk,    // 0x16
    em32_R1_br_Dummy, em32_R1_JumpUp,       // 0x17
    em32_R1_br_Dummy, em32_R1_JumpDown,     // 0x18
    em32_R1_br_Dummy, em32_R1_C_Wait,       // 0x19
    em32_R1_br_C_Atk, em32_R1_C_Atk,        // 0x1A
    em32_R1_br_Dummy, em32_R1_C_AtkHit,     // 0x1B
    em32_R1_br_Dummy, em32_R1_P_Atk,        // 0x1C
    em32_R1_br_P_Catch, em32_R1_P_Catch,    // 0x1D
    em32_R1_br_Dummy, em32_R1_P_CatchHit,   // 0x1E
    em32_R1_br_Dummy, em32_R1_Ground,       // 0x1F
    em32_R1_br_Dummy, em32_R1_BreakBarred,  // 0x20
};

static Em32Func Em32_R2_move_tbl[1] = {
    em32_R1_Dm_Normal,
};

static Em32Func Em32_R3_move_tbl[1] = {
    em32_R1_Die_Normal,
};

// Attack parameters per em32AtkCk kind.
static EmAtkInfo em32_atk_tbl[6] = {
    { 400.0f, PL_DM_AUTO, 0x44C, 0, 10, 0 },
    { 400.0f, PL_DM_AUTO, 0, 4, 10, 0 },
    { 400.0f, PL_DM_AUTO, 0x44C, 0, 10, 0 },
    { 1200.0f, PL_DM_AUTO, 0, 4, 10, 0 },
    { 600.0f, PL_DM_AUTO, 0x44C, 0, 10, 0 },
    { 600.0f, PL_DM_AUTO, 0x44C, 0, 10, 0 },
};

// Tail cloth chain (em32ClothSet): the parts, their upper / lower neighbours and the swing limits.
static u8 em32_cloth_parts[12] = { 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x3D, 0x3E, 0x3F, 0x40 };
static u8 em32_cloth_up[12] = { 0xFF, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0xFF, 0x3D, 0x3E, 0x3F };
static u8 em32_cloth_down[12] = { 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0x3E, 0x3F, 0x40, 0xFF };
static f32 em32_cloth_max[12] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
asm(".section .data\n\t.balign 8\n\t.text");

extern "C" void _prolog()
{
    OSReport("em32 prolog Ok\n");
    EmInitFunc = Em32Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

// Placement-constructs the boss over the manager's cEm slot (EmInitFunc for id 0x32); em32_R0_Init
// sets it up on the first move.
void Em32Init(cEm* em)
{
    new (em) cEm32();
}

// Per-frame damage reaction, from move(). First the area damage (DmgMgr: the container falls /
// explosions, kinds 1 / 4 / 5 / 7): 500 damage once per 120 frames (dmGuard), kept at 1 HP outside the
// last form, and the flinch (routine 2 with dmWep 0x16) or death (routine 3) unless an appear
// routine runs (flags 0x800). Then the weapon hit in dmHit: em32SetDmVal through LifeDownSet2
// (again floored at 1 HP before the last form), blood and hit sound, death at 0 HP. Reactions are
// held off by flags 0x800 / 8 / 0x100; in form 1 with the texture render on, HP at or under 3/8 of
// max triggers the last form (routine 1/1). Otherwise a per-weapon flinch value (halved in form 1,
// quartered in form 2) accumulates in dmgTotal, and past 100 (heavy weapons always) the flinch
// routine 2/0 starts.
void em32DmCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;
    int near;
    int dmg;
    int flag;
    int wep;

    if (em->hp > 0 && EmDeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            if (w->dmGuard == 0) {
                w->dmGuard = 120;
                flag = 0;
                if (w->mode != 2) {
                    flag = 1;
                }
                if (w->flags & 0x800) {
                    flag = 1;
                }
                LifeDownSet2(em, 500, 0, flag);
                {
                    int f = w->flags & 0x800;
                    if (f) {
                        return;
                    }
                    if (em->hp > 0) {
                        w->dmgTotal = f;
                        em->dmg.m_Wep = 0x16;
                        em->r_no_0 = 2;
                        em->r_no_1 = f;
                        em->r_no_2 = f;
                        em->r_no_3 = f;
                        return;
                    }
                    EmSetDie(em);
                    EmSetDieCnt(em);
                    em->r_no_0 = 3;
                    em->r_no_1 = f;
                    em->r_no_2 = f;
                    em->r_no_3 = f;
                    return;
                }
            }
            break;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    zero = 0;
    em->dmg.m_Flag = zero;
    near = 0;
    w->flags |= 0x200;
    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Timer = 1;
    if (wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    dmg = em32SetDmVal(em);
    flag = 0;
    if (w->mode != 2) {
        flag = 1;
    }
    LifeDownSet2(em, dmg, 0, flag);
    em32BloodSet(em);
    SndCall(8, 0xA, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCnt(em);
        em->r_no_0 = 3;
        em->r_no_1 = zero;
        em->r_no_2 = zero;
        em->r_no_3 = zero;
        return;
    }
    if (w->flags & 0x800) {
        return;
    }
    if (w->flags & 8) {
        return;
    }
    if (w->flags & 0x100) {
        return;
    }
    if (w->mode == 1 && (w->flags & 0x100000)) {
        if (em->hp <= em->hp_max * 3 / 8) {
            EmRoutineSet(em, 1, 1, 0, 0);
            return;
        }
    }
    switch (em->dmg.m_Wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x11:
    case 0x26:
    case 0x2B:
        dmg = Rnd() % 10 + 10;
        break;
    case 0xB:
    case 0x1B:
    case 0x1D:
    case 0x27:
        dmg = Rnd() % 10 + 5;
        break;
    case 0xC:
        dmg = Rnd() % 20 + 20;
        break;
    case 9:
    case 0xA:
    case 0x10:
    case 0x14:
    case 0x15:
    case 0x28:
        dmg = Rnd() % 25 + 25;
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            dmg = Rnd() % 70 + 50;
        } else {
            dmg = Rnd() % 20 + 10;
        }
        break;
    case 5:
    case 6:
    case 0x2C:
        dmg = Rnd() % 100 + 50;
        break;
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        dmg = 1000;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    if (w->mode == 1) {
        dmg /= 2;
    }
    if (w->mode == 2) {
        dmg /= 4;
    }
    w->dmgTotal += dmg;
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
        if (w->dmgTotal <= 100) {
            return;
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (w->dmgTotal <= 100) {
            return;
        }
        break;
    case 0xE:
        return;
    case 5:
    case 6:
    case 0xD:
    case 0x12:
    case 0x29:
    case 0x2C:
    default:
        break;
    }
    w->dmgTotal = 0;
    EmRoutineSet(em, 2, 0, 0, 0);
}

// Per-frame update from the enemy manager. Order: damage, clear the per-frame flags, tick the wait
// / attack / area-damage timers (at 1 HP before the last form the waits are pinned to 30 so it keeps
// moving), reset the container break numbers, route (em32RouteCk) and the predicted player position
// (em32GetPlPos), the routine table (r_no_0 0xFF = model load failed: destroy), body / neck
// overrides, parts, the death shrink, attack / collision / stage collision (skipped with flags 0x40
// while jumping; stuckCnt counts frames the collision halved the movement), the tail cloth, the
// texture-blended skin, the breathing sound every 60 frames, and the shadow colour fade of the
// invisible form (flags 0x20000: in with 0x10000, out otherwise).
void cEm32::move()
{
    Em32Work* w = EM32_WK(this);
    f32 d;

    if (r_no_0) {
        em32DmCk(this);
    }
    w->flags &= 0xFFF632A0;
    if (w->wait) {
        w->wait--;
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->longAtkWait) {
        w->longAtkWait--;
    }
    if (w->dmGuard) {
        w->dmGuard--;
    }
    if (w->mode != 2 && hp <= 1) {
        w->wait = 30;
        w->longAtkWait = 30;
    }
    w->breakNo = 0xFF;
    w->breakNo2 = 0xFF;
    if (r_no_0) {
        em32RouteCk(this);
    }
    em32GetPlPos(this);
    Em32_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em32BodyMove(this);
    em32NeckMove(this);
    partsWorldCalc();
    em32ScaleCompress(this);
    d = VEC_DISTXZ(&pos_old, &pos);
    EmAtCheck(this);
    atari.move();
    if (!(w->flags & 0x40)) {
        SatMgr.check(this, 0);
    }
    if (VEC_DISTXZ(&pos, &pos_old) < d * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em32ClothMove(this);
    if (w->pTexModel && !(w->flags & 0x40000)) {
        w->pTexModel->setTexBlendTbl(w->texBlend);
        w->pTexModel->setBlendRatio(0xFF);
        w->pTexModel->setBlendType(2);
    }
    if (hp > 0 && (be_flag & 2) && !(w->flags & 0x10000)) {
        if (w->breathTimer) {
            w->breathTimer--;
        } else {
            w->breathTimer = 59;
            SndCall(8, 0x30, &pos, id, 0, this);
        }
    }
    em32BreathSeStopCk(this);
    if (w->flags & 0x20000) {
        be_flag |= 0x10;
        if (w->flags & 0x10000) {
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
    } else {
        be_flag &= ~0x10;
    }
}

// Routine 0: one-time setup. Loads the body (archive 4 / 5) with the extra model 6 and the
// texture-blended skin 7, the effect data, the cut-player halves (em32PlDivideModelInit), the ctrl12
// controller, foot shadows and the tail cloth; marks the four claw tips as non-motion parts, a huge
// light box, a 1.5 m collision at priority 1, three sphere obstacles on parts 0x1C..0x1E, the root
// hit box plus the extra ones, the work (first attack in 150..300 frames, long attack in 600), the
// idle effect, form 0, the second motion work (pMot) for the blended motions, part 0x53 hidden, hp
// 500, and the start routine Wait (1/6) with the idle motion.
static void em32_R0_Init(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cModelInfo* info;
    cParts* p;
    Vec v;
    f32 fzero;
    int zero;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em32() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    info = ModInfoMgr.create(ARC(6), ARC(5));
    if (info) {
        em->addModel(info);
    }
    zero = 0;
    w->pTexModel = (cModelInfo*) zero;
    info = ModInfoMgr.create(ARC(7), ARC(5));
    if (info) {
        em->addModel(info);
        w->pTexModel = info;
    }
    em->be_flag |= 0x1000000;
    EspDataLoad((u32) ARC(8), EFF_EM32, 0);
    em32PlDivideModelInit(em);
    w->pCatchObj = (cObj*) zero;
    w->pCtrl12 = GetCtrlCtrl12();
    em->pFsdTbl = &Em32_fs_tbl;
    em32ClothSet(em);
    ((cParts*) em->getPartsPtr(0x21))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x27))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x2D))->motParts.flags |= 0x1000;
    ((cParts*) em->getPartsPtr(0x33))->motParts.flags |= 0x1000;
#line 836 "D:/Bio4/Prog/em32.cpp"
    em->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xD);
    // Compound literals: the zero template is shared with plem32_P_CatchHit's light init.
    em->LightInfo.init2(0, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 10000.0f, 10000.0f, 10000.0f }), 2);
    em->atari.init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 1500.0f, 1, 0x2000, 10);
    em->atari.setPriority(PRI_LV1);
    em->litArea.on(1);
    fzero = 0.0f;
    v.x = fzero;
    v.y = fzero;
    v.z = fzero;
    SetObaModel((cObj*) em, 0x1C, &v, 700.0f, 900.0f, 0);
    SetObaModel((cObj*) em, 0x1D, &v, 700.0f, 900.0f, 0);
    SetObaModel((cObj*) em, 0x1E, &v, 700.0f, 900.0f, 0);
    YarareInit(em, fzero, fzero, fzero, 300.0f, 200.0f, 2, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[0], fzero, fzero, fzero, 250.0f, 200.0f, 3, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[1], fzero, fzero, fzero, 320.0f, 200.0f, 4, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[2], fzero, fzero, fzero, 200.0f, 100.0f, 6, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[3], fzero, fzero, -200.0f, 350.0f, 200.0f, 0x1B, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[4], fzero, fzero, -200.0f, 350.0f, 200.0f, 0x1C, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[5], fzero, fzero, -200.0f, 500.0f, 200.0f, 0x1D, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[6], fzero, fzero, -200.0f, 500.0f, 200.0f, 0x1E, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[7], fzero, fzero, -200.0f, 400.0f, 200.0f, 0x1F, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[8], fzero, 50.0f, -150.0f, 350.0f, 150.0f, 0x20, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
    YarareAdd(em, &w->hit[9], -450.0f, fzero, fzero, 130.0f, 450.0f, 9, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[10], -500.0f, fzero, fzero, 115.0f, 500.0f, 0xA, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[11], -150.0f, fzero, fzero, 150.0f, 150.0f, 0xC, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[12], fzero, fzero, fzero, 150.0f, 450.0f, 0x12, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[13], fzero, fzero, fzero, 150.0f, 300.0f, 0x13, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[14], fzero, fzero, fzero, 150.0f, 300.0f, 0x14, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[15], fzero, fzero, fzero, 150.0f, 250.0f, 0x15, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[16], fzero, fzero, fzero, 130.0f, 250.0f, 0x16, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    YarareAdd(em, &w->hit[17], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x22, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[18], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x23, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[19], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x28, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[20], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x29, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[21], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x2E, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[22], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x2F, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[23], fzero, -500.0f, fzero, 150.0f, 600.0f, 0x34, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[24], fzero, -800.0f, fzero, 150.0f, 800.0f, 0x35, YAT_FLAG_ON);
    YarareAdd(em, &w->hit[25], fzero, fzero, fzero, 250.0f, 300.0f, 0x56, 0);
    YarareAdd(em, &w->hit[26], fzero, fzero, fzero, 250.0f, 300.0f, 0x57, 0);
    YarareAdd(em, &w->hit[27], fzero, fzero, fzero, 250.0f, 300.0f, 0x58, 0);
    em->lockParts = 2;
    em->lockOfs.x = fzero;
    em->lockOfs.y = fzero;
    em->lockOfs.z = fzero;
    w->flags = zero;
    w->neckAng = fzero;
    w->wait = zero;
    w->atkWait = Rnd() % 150 + 150;
    w->x7C4 = zero;
    w->dmgTotal = zero;
    w->sndId = zero;
    // The 600/0xFF constants are REG_EQUIV pseudos the original never allocates: reload re-materialises
    // them in the spill registers r10/r9 (59 takes r11); ours local-allocs them the other way round.
    {
        register int lw asm("r10"); // COMPILER-DIFF: #13
        register int bn asm("r9"); // COMPILER-DIFF: #13
        lw = 600;
        w->longAtkWait = lw;
        w->voiceTimer = 59;
        bn = 0xFF;
        w->breakNo = bn;
    }
    w->espKind[0] = EspPullCoreKind();
    w->espKind[1] = EspPullCoreKind();
    w->espKind[2] = EspPullCoreKind();
    EstSet(em, -1, 0, 0, EFF_EM32, 4, 0, w->espKind[0], em, (void*) zero);
    w->mode = zero;
#line 1000 "D:/Bio4/Prog/em32.cpp"
    w->pMot = (MotionWorkSub*) MEM_ALLOC(0xD0, 1, 0xD);
    if (w->pMot) {
        memclr_asm(w->pMot, 0xD0);
    }
    p = em->getPartsPtr(0x53);
    p->scale.x = fzero;
    p->scale.y = fzero;
    p->scale.z = fzero;
    // hp = 500 and the routine bytes 6/1 are REG_EQUIV pseudos re-materialised by the original's reload
    // (r0, r11, r0 again): with no dying register the `sth hp` stays in source order ahead of the
    // routine stores. sched2 issues `li r0,1` after `stb r11,0xfd` (both priority 15; ours has one more
    // dependent on the stb) unless the `li` gets a dependent, see the asm below.
    {
        register int hp asm("r0"); // COMPILER-DIFF: #13
        register int six asm("r11"); // COMPILER-DIFF: #13
        register int one asm("r0"); // COMPILER-DIFF: #13
        hp = 500;
        em->hp = hp;
        six = 6;
        one = 1;
        em->r_no_0 = one;
        em->r_no_1 = six;
        em->r_no_2 = zero;
        em->r_no_3 = zero;
        MotionSetCore(em, &em->Motion, ARC(9), 0, 0, 1, 0);
        // sched2's tie between `li r0,1` and `stb r11,0xfd` (both priority 15) is broken by dependent counts
        // (5 vs 6): the code-less read of `one` gives the `li` its sixth dependent so it is issued first like
        // the original's. Placed after the call so the anchor (priority 8, tied to the callee-saved `w`)
        // ranks below the argument `li`s and takes no issue slot of the block.
        asm("" : "+r"(w) : "r"(one)); // COMPILER-DIFF: #13 (codeless anchor)
    }
    MotionMove(em, 0);
    em32_R0_Move(em);
    if (w->pMot) {
        w->pMot->Seq_speed = 1.0f;
        MotionSetCore(em, w->pMot, ARC(0xAD), 0, 0, 4, 0);
    }
    OSReport("em32 free size = 0x%x\n", sizeof(Em32Work));
}

// Routine 1: runs the branch check and then the behaviour of the current r_no_1 state.
static void em32_R0_Move(cEm32* em)
{
    Em32_R1_move_tbl[em->r_no_1 * 2](em);
    Em32_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the states that have none.
static void em32_R1_br_Dummy(cEm32* em)
{
}

// The parasite shows: the invisible form ends, the effects of the first form are replaced.
static void em32_R1_Parasite(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;

    w->flags |= 0x4800;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x43), ARC(0x44), 3, 1, 0);
        w->hit[25].flag |= 1;
        w->hit[26].flag |= 1;
        w->hit[27].flag |= 1;
        em32TexrenderInit(em);
        EstSet(em, -1, 0, 0, EFF_EM32, 5, 0, w->espKind[1], em, (void*) step);
        EM32_EFFECT_DELETE(w->espKind[0], em);
        EstSet(em, -1, 0, 0, EFF_EM32, 6, 0, w->espKind[0], em, (void*) step);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0x17, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            w->wait = 10;
            // A local for the 1: stored directly, it shares a register with EmRoutineSet's own
            // literal arguments below instead of getting its own.
            int n = 1;
            w->mode = n;
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
}

// The last form: the motion event turns the mode to 2.
static void em32_R1_LastMode(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;

    w->flags |= 0x800;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x7B), ARC(0x7C), 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x22, 0, ESP_CORE_KIND_NONE, em, (void*) step);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            w->x978 = 450;
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            w->mode = 2;
        }
        break;
    }
}

// Routine 1/2: the second appearance (scripted, dmType 2: no hit damage). Hidden with its idle
// effect removed until the level script sets flag bit 0, then shown with the appear effects, its
// hit boxes marked, the appear motion, and into Threat (1/0xD).
static void em32_R1_2ndAppear(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;

    w->flags |= 0x800;
    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        em->be_flag &= ~2;
        EM32_EFFECT_DELETE(w->espKind[0], em);
        em->r_no_2++;
    case 1:
        MotionSetCore(em, &em->Motion, ARC(0x53), ARC(0x54), 0, 5, 0);
        MotionMove(em, 0);
        if (!(em->flag & 1)) {
            break;
        }
        em->flag &= ~1;
        em->r_no_2++;
    case 2:
        zero = 0;
        MotionSetCore(em, &em->Motion, ARC(0x53), ARC(0x54), 0, 5, 0);
        em32SetYarareMark(em, 1);
        EstSet(em, -1, 0, 0, EFF_EM32, 4, 1, w->espKind[0], em, (void*) zero);
        EstSet(em, -1, 0, 0, EFF_EM32, 0xC, 1, w->espKind[0], em, (void*) zero);
        em->be_flag |= 2;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        }
        break;
    }
}

// Routine 1/3: the third appearance. Hidden and fully transparent until the script sets flag bit
// 0, then it picks the jump-down point (em32GetJumpDownNo) and drops from the ceiling (JumpDown 1/0x18
// with r_no_3 1).
static void em32_R1_3rdAppear(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;

    w->flags |= 0x800;
    em->dmg.m_Timer = 2;
    if (em->r_no_2 == 0) {
        em->be_flag &= ~2;
        em->invisible_factor = 0.0f;
        {
            u32 t = em->flag ^ 1;
            zero = t & 1;
        }
        if (zero == 0) {
            em->flag &= ~1;
            em32SetYarareMark(em, 1);
            em32GetJumpDownNo(em);
            EmRoutineSet(em, 1, 0x18, zero, 1);
        }
    }
}

// Routine 1/4: the fall of the third appearance played visibly once, after which the enemy hides
// again (transparent, not displayed) and holds.
static void em32_R1_3rdFall(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x800;
    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x79), 0, 0, 1, 0);
        em->be_flag |= 2;
        em->invisible_factor = 1.0f;
        em32SetYarareMark(em, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->be_flag &= ~2;
            em->invisible_factor = 0.0f;
            em->r_no_2++;
        }
        break;
    }
}

// Routine 1/5: the fourth appearance. Hidden until the script sets flag bit 0, then shown with the
// appear effect and the appear motion, which it holds (the script moves it on).
static void em32_R1_4thAppear(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x800;
    em->dmg.m_Timer = 2;
    switch (em->r_no_2) {
    case 0:
        em->r_no_2++;
        em->be_flag &= ~2;
    case 1:
        MotionSetCore(em, &em->Motion, ARC(0x7A), 0, 0, 1, 0);
        MotionMove(em, 0);
        if (!(em->flag & 1)) {
            break;
        }
        em->r_no_2++;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x7A), 0, 0, 1, 0);
        em32SetYarareMark(em, 1);
        EstSet(em, -1, 0, 0, EFF_EM32, 4, 1, w->espKind[0], em, 0);
        em->be_flag |= 2;
        em->r_no_2++;
    case 3:
        MotionMove(em, 0);
        break;
    }
}

// Wait end: the next floor routine by the target angle, the form and chance.
static inline void em32NextWalkSet(cEm32* em, Em32Work* w, int r)
{
    if (w->targetAngAbs > 1.30899692f) {
        EmRoutineSet(em, 1, 0xC, r, r);
    } else if (w->mode == 2) {
        EmRoutineSet(em, 1, 0xB, r, r);
    } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
        EmRoutineSet(em, 1, 8, r, r);
    } else {
        EmRoutineSet(em, 1, 9, r, r);
    }
}

// Routine 1/6: idle (the form's idle loop, flags 0x10 = routine running). The player counts as
// found (flags 0x200) within 8 m, or within 15 m inside 30 deg of the facing; once found and `wait`
// is out, em32NextWalkSet picks Turn / AtkWalk / Walk / Dash. At 1 HP in the area's west half it
// looks for a jump up onto the containers (em32JumpUpCk). Breathes.
static void em32_R1_Wait(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int wait;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(9), 0, 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x3E), 0, 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0xA), 0, 30, 5, 0);
            break;
        }
        w->flags |= 0x200;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (!(w->flags & 0x200)) {
            if (em->l_pl < 64000000.0f) {
                w->flags |= 0x200;
            }
            if (em->l_pl < 225000000.0f && w->routeAngAbs < 0.52359879f) {
                w->flags |= 0x200;
            }
        } else {
            wait = w->wait;
            if (wait == 0) {
                em32NextWalkSet(em, w, wait);
            }
        }
        break;
    }
    if (em->hp <= 1 && em->pos.x <= 25338.0f && em32JumpUpCk(em)) {
        return;
    }
    em32BreathSe(em);
}

// Routine 1/7: the pause between moves in which the next attack is chosen. After 15 frames: form
// 0 (with a route and no ambush swipe pending) bites (1/0xF, 50 % within 2 m) or catches (1/0x10)
// within 2.5 m and 60 deg, or does the long lunge (1/0x12) at 3..5 m in front; form 1 picks the
// parasite catch (1/0x1D, 50 %) or the parasite attack (1/0x1C) within 3 m; the last form never
// attacks from here. While the player lives it then tries a step up (flags 0x1000, or 50 % / after a
// hit within 4 m), a jump up (form 0, 20 %), or the floor move of em32NextWalkSet.
static void em32_R1_Ambush(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int timer;
    int mode;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(9), 0, 5, 5, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x3E), 0, 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0xA), 0, 30, 5, 0);
            break;
        }
        w->timer = 15;
        w->flags &= ~1;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        timer = w->timer;
        if (timer) {
            w->timer--;
            break;
        }
        mode = w->mode;
        switch (mode) {
        case 0:
        default:
            if (em32AmbushAtkCk(em) == 0 && (w->flags & 1)) {
                f32 a = w->plAngAbs;
                if (a < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, timer, timer);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                } else {
                    f32 ang = w->plAngAbs;
                    if (ang < 0.52359879f && w->plDist2 > 9000000.0f && w->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0x12, 0, 0);
                    }
                }
            }
            break;
        case 1:
            if (w->routeAngAbs < 0.52359879f && em->l_pl < 9000000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, mode, 0x1D, timer, timer);
            } else if (w->routeAngAbs < 0.785398185f && em->l_pl < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            }
            break;
        case 2:
            break;
        }
        if (EmDeadCk(em)) {
            if ((w->flags & 0x1000) && em32StepUpCk2(em)) {
                break;
            }
            if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                break;
            }
            if ((w->Atk_ck || Rnd() % 10 > 4) && em->l_pl < 16000000.0f && em32StepUpCk2(em)) {
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    em32BreathSe(em);
}

// Floor routine tail shared by Walk / Dash: the step up checks, the rack break and the stuck check
// (a macro: the literal stays at its compare).
#define EM32_WALK_END_CK(em, w, lim)                                                 \
    {                                                                                \
        int zero;                                                                    \
                                                                                     \
        if (((w)->flags & 0x1000) && em32StepUpCk2(em)) {                            \
            return;                                                                  \
        }                                                                            \
        if ((em)->hp <= 1 && (em)->pos.x <= lim && em32JumpUpCk(em)) {               \
            return;                                                                  \
        }                                                                            \
        zero = em32StepUpCk3(em);                                                    \
        if (zero) {                                                                  \
            return;                                                                  \
        }                                                                            \
        em32RackBreakCk(em);                                                         \
        if ((w)->stuckCnt > 30 && (w)->mode != 0) {                                  \
            (w)->stuckCnt = zero;                                                    \
            if (Rnd() % 10 > 4) {                                                    \
                EmRoutineSet(em, 1, 0x20, zero, zero);                               \
                return;                                                              \
            }                                                                        \
        }                                                                            \
        em32BreathSe(em);                                                            \
    }

// Routine 1/8: the walk towards the target as a four-motion turn blend of the current form
// (EM32_BLEND_TURN steers up to 45 deg; r_no_3 skips the blend-in). At each loop end it tries a step
// up, or turns past 75 deg. Then per form: form 0 leaves for the parasite reveal (1/0) east of x 7 m
// or on the script's flag bit 31, else the ambush swipe check, the bite / catch within 2.5 m and 60
// deg, the long lunge at 3..5 m once longAtkWait is out, and beyond 6 m with atkWait out a 20 % jump
// up (else a new 150..300 frame wait); form 1 goes to the last form at 3/8 HP (or script flag bit
// 30) and picks the parasite catch / attack within 3 m; the last form switches to AtkWalk within 5 m.
// The trample sphere (attack 4) is tested at the feet parts 0x54.. on motion event bit 0, then the
// shared floor tail (EM32_WALK_END_CK: steps, jump up at 1 HP, rack break, the barred door when stuck).
static void em32_R1_Walk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    PlArc* arc;
    int ret;
    int mode;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            w->blendM0 = ARC(0xB);
            w->blendM3 = ARC(0xC);
            w->blendM1 = ARC(0xD);
            w->blendM2 = ARC(0xE);
            break;
        case 1:
            w->blendM0 = ARC(0x3F);
            w->blendM3 = ARC(0x40);
            w->blendM1 = ARC(0x41);
            w->blendM2 = ARC(0x42);
            break;
        case 2:
            w->blendM0 = ARC(0x3A);
            w->blendM3 = ARC(0x3B);
            w->blendM1 = ARC(0x3C);
            w->blendM2 = ARC(0x3D);
            break;
        }
        w->blendC = 5;
        w->blendCnt = 10;
        if (em->r_no_3) {
            w->blendCnt = 0;
        }
        w->blendSeq = 0;
        w->blendVal = 0.0f;
        em->r_no_2++;
    case 1:
        EM32_BLEND_TURN(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSet(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMove(em, 0)) {
            if (em32StepUpCk(em)) {
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                break;
            }
        }
        mode = w->mode;
        switch (mode) {
        case 0:
        default:
            if (em->pos.x > 7000.0f || (em->flag & 0x80000000)) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            ret = em32AmbushAtkCk(em);
            if (ret) {
                break;
            }
            if (w->flags & 1) {
                if (w->plAngAbs < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, ret, ret);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                    break;
                }
                {
                    int wait = w->longAtkWait;
                    if (wait == 0 && w->plAngAbs < 0.52359879f && w->plDist2 > 9000000.0f &&
                        w->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0x12, wait, wait);
                        break;
                    }
                }
            }
            if (w->atkWait == 0 && em->l_pl > 36000000.0f) {
                if (Rnd() % 10 > 7) {
                    em32JumpUpCk(em);
                } else {
                    w->atkWait = Rnd() % 150 + 150;
                }
            }
            break;
        case 1:
            if (em->hp <= em->hp_max * 3 / 8 && (w->flags & 0x100000)) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            ret = em->flag & 0x40000000;
            if (ret) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            if (w->routeAngAbs < 0.52359879f && em->l_pl < 9000000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x1D, ret, ret);
                break;
            }
            if (w->routeAngAbs < 0.785398185f && em->l_pl < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            }
            break;
        case 2:
            if (w->routeAngAbs < 0.52359879f && em->l_pl < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            }
            break;
        }
        if (em->Motion.Seq_old.Free & 1) {
            em32AtkCk(em, 4, 0x54);
            em32AtkCk(em, 4, 0x55);
            em32AtkCk(em, 4, 0x56);
            em32AtkCk(em, 4, 0x57);
            em32AtkCk(em, 4, 0x58);
            em32AtkCk(em, 4, 0x59);
            em32AtkCk(em, 4, 0x5A);
            em32AtkCk(em, 4, 0x5B);
            em32AtkCk(em, 4, 0x5C);
            em32AtkCk(em, 4, 0x5D);
        }
        break;
    }
    EM32_WALK_END_CK(em, w, 25338.0f);
}

// Routine 1/9: the dash, the same blended turn / attack selection / floor tail as em32_R1_Walk with
// the form's run motions, limited to 3..6 loops after which it rests in Wait for 60 frames.
static void em32_R1_Dash(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int ret;
    int mode;
    int timer;

    w->flags |= 0x10;
    em32SetYarareMark(em, 1);
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            w->blendM0 = ARC(0x21);
            w->blendM3 = ARC(0x22);
            w->blendM1 = ARC(0x23);
            w->blendM2 = ARC(0x24);
            break;
        case 1:
            w->blendM0 = ARC(0x5D);
            w->blendM3 = ARC(0x5E);
            w->blendM1 = ARC(0x5F);
            w->blendM2 = ARC(0x60);
            break;
        case 2:
            w->blendM0 = ARC(0x30);
            w->blendM3 = ARC(0x31);
            w->blendM1 = ARC(0x32);
            w->blendM2 = ARC(0x33);
            break;
        }
        w->blendC = 5;
        w->blendCnt = 10;
        w->blendSeq = 0;
        w->timer = (Rnd() & 3) + 3;
        w->blendVal = 0.0f;
        em->r_no_2++;
    case 1:
        EM32_BLEND_TURN(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSet(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMove(em, 0)) {
            timer = w->timer;
            if (timer == 0) {
                w->wait = 60;
                EmRoutineSet(em, 1, 6, timer, timer);
                break;
            }
            w->timer = timer - 1;
            if (em32StepUpCk(em)) {
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                break;
            }
        }
        mode = w->mode;
        switch (mode) {
        case 0:
        default:
            if (em->pos.x > 7000.0f || (em->flag & 0x80000000)) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            ret = em32AmbushAtkCk(em);
            if (ret) {
                break;
            }
            if (w->flags & 1) {
                f32 a = w->plAngAbs;
                if (a < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, ret, ret);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                    break;
                }
                {
                    int wait = w->longAtkWait;
                    f32 ang = w->plAngAbs;
                    if (wait == 0 && ang < 0.52359879f && w->plDist2 > 9000000.0f &&
                        w->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0x12, wait, wait);
                        break;
                    }
                }
            }
            if (w->atkWait == 0 && em->l_pl > 36000000.0f) {
                if (Rnd() % 10 > 7) {
                    em32JumpUpCk(em);
                } else {
                    w->atkWait = Rnd() % 150 + 150;
                }
            }
            break;
        case 1:
            if (em->hp <= em->hp_max * 3 / 8 && (w->flags & 0x100000)) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            ret = em->flag & 0x40000000;
            if (ret) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            if (w->routeAngAbs < 0.52359879f && em->l_pl < 9000000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x1D, ret, ret);
                break;
            }
            if (w->routeAngAbs < 0.785398185f && em->l_pl < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            }
            break;
        case 2:
            if (w->routeAngAbs < 0.52359879f && em->l_pl < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            }
            break;
        }
        break;
    }
    EM32_WALK_END_CK(em, w, 25338.0f);
}

// Routine 1/0xA: the step back (its own motion in the last form), then the floor move of
// em32NextWalkSet (the last form only switches to AtkWalk when facing the player within 5 m).
static void em32_R1_Back(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x75), ARC(0x76), 3, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x75), ARC(0x76), 3, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x61), ARC(0x62), 3, 1, 0);
            break;
        }
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2 && w->routeAngAbs < 0.52359879f && em->l_pl < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    em32BreathSe(em);
}

// Attack walk of the last form: the claws are out (parts 0x54..0x5D hit the player) and the step
// ends with a threat or a turn.
static void em32_R1_AtkWalk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    int zero;
    int hit;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        w->blendM0 = ARC(0x6D);
        w->blendM3 = ARC(0x6E);
        w->blendM1 = ARC(0x6F);
        w->blendM2 = ARC(0x70);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x25, 0, w->espKind[1], em, (void*) step);
        w->blendC = 1;
        w->blendCnt = 10;
        w->blendVal = 0.0f;
        w->blendSeq = step;
        w->Atk_ck = step;
        em->r_no_2++;
    case 1:
        EM32_BLEND_TURN2(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSet(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMove(em, 0)) {
            if (w->Atk_ck) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else {
                em->r_no_2++;
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            EM32_TAIL_ATK(em, 4);
        }
        break;
    case 2:
        zero = 0;
        w->blendM0 = ARC(0x4F);
        w->blendM3 = ARC(0x50);
        w->blendM1 = ARC(0x51);
        w->blendM2 = ARC(0x52);
        w->blendC = 5;
        EstSet(em, -1, 0, 0, EFF_EM32, 0x26, 0, w->espKind[1], em, (void*) zero);
        w->blendCnt = 10;
        w->blendSeq = zero;
        w->Atk_ck = zero;
        em->r_no_2++;
    case 3:
        EM32_BLEND_TURN2(em, w, &w->targetPos, 0.785398185f);
        em32BlendMotSet(em, w->blendM0, w->blendM1, w->blendM2, w->blendM3, 0, 0, w->blendC);
        if (MotionMove(em, 0) || (em->Motion.Seq_old.Free & 4)) {
            if (w->Atk_ck) {
                goto threat;
            }
            GameAddPoint(LVADD_ESCAPEATTACK);
            hit = w->Atk_ck;
            if (hit) {
            threat:
                w->wait = 60;
                EM32_EFFECT_DELETE(w->espKind[1], em);
                EmRoutineSet(em, 1, 0xD, 0, 0);
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EM32_EFFECT_DELETE(w->espKind[1], em);
                EmRoutineSet(em, 1, 0xC, hit, hit);
                break;
            }
            if (w->stuckCnt > 30) {
                w->stuckCnt = hit;
                if (Rnd() % 10 > 4) {
                    EM32_EFFECT_DELETE(w->espKind[1], em);
                    EmRoutineSet(em, 1, 0x20, hit, hit);
                    break;
                }
            }
            if (Rnd() % 10 > 7) {
                EM32_EFFECT_DELETE(w->espKind[1], em);
                EmRoutineSet(em, 1, 0x1F, 0, 0);
                break;
            }
            if (pG->Game_level <= 9) {
                if (Rnd() % 10 > 7) {
                    EM32_EFFECT_DELETE(w->espKind[1], em);
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                    break;
                }
                if (em->l_pl > 64000000.0f) {
                    EM32_EFFECT_DELETE(w->espKind[1], em);
                    EmRoutineSet(em, 1, 8, 0, 0);
                    break;
                }
            }
            EstSet(em, -1, 0, 0, EFF_EM32, 0x26, 0, w->espKind[1], em, 0);
        }
        if (em->Motion.Seq_old.Free & 1) {
            EM32_TAIL_ATK(em, 4);
        }
        break;
    }
    em32RackBreakCk(em);
}

// Routine 1/0xC: the turn towards the target: the form's left / right quarter turn under 135 deg,
// its about-face beyond. A trample hit during it (Atk_ck) rests 60 frames in Wait; otherwise the
// floor move of em32NextWalkSet. Breaks racks and breathes.
static void em32_R1_Turn(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            if (w->targetAngAbs < 2.35619450f) {
                if (w->targetAng < 0.0f) {
                    MotionSetCore(em, &em->Motion, ARC(0x19), ARC(0x1A), 10, 1, 0);
                } else {
                    MotionSetCore(em, &em->Motion, ARC(0x17), ARC(0x18), 10, 1, 0);
                }
            } else {
                MotionSetCore(em, &em->Motion, ARC(0x1F), ARC(0x20), 10, 1, 0);
            }
            break;
        case 1:
            if (w->targetAngAbs < 2.35619450f) {
                if (w->targetAng < 0.0f) {
                    MotionSetCore(em, &em->Motion, ARC(0x67), ARC(0x68), 10, 1, 0);
                } else {
                    MotionSetCore(em, &em->Motion, ARC(0x65), ARC(0x66), 10, 1, 0);
                }
            } else {
                MotionSetCore(em, &em->Motion, ARC(0x63), ARC(0x64), 10, 1, 0);
            }
            break;
        case 2:
            if (w->targetAngAbs < 2.35619450f) {
                if (w->targetAng < 0.0f) {
                    MotionSetCore(em, &em->Motion, ARC(0x38), ARC(0x39), 10, 1, 0);
                } else {
                    MotionSetCore(em, &em->Motion, ARC(0x36), ARC(0x37), 10, 1, 0);
                }
            } else {
                MotionSetCore(em, &em->Motion, ARC(0x34), ARC(0x35), 10, 1, 0);
            }
            break;
        }
        w->Atk_ck = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->Atk_ck) {
                w->wait = 60;
                EmRoutineSet(em, 1, 6, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2 && w->routeAngAbs < 0.52359879f && em->l_pl < 25000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    em32RackBreakCk(em);
    em32BreathSe(em);
}

// Attack end: the step up / jump up checks (a macro: the literal stays at its compare, an inline's
// f32 parameter would become a pseudo hoisted into a callee-saved FPR).
#define EM32_ATK_END_CK(em, w, near, jc)                                                \
    if (((w)->flags & 0x1000) && em32StepUpCk2(em)) {                                   \
        return;                                                                         \
    }                                                                                   \
    if ((w)->mode == 0 && Rnd() % 10 > jc && em32JumpUpCk(em)) {                         \
        return;                                                                         \
    }                                                                                   \
    if (((w)->Atk_ck || Rnd() % 10 > 4) && (em)->l_pl < near && em32StepUpCk2(em)) {    \
        return;                                                                         \
    }

// Routine 1/0xD: the roar (the form's threat motion and effect; r_no_3 = no blend-in and no stage
// collision, used right after a landing). The player-blocking collision bits are turned on here.
// When it ends: form 0 bites / catches within 2.5 m and 60 deg, form 1 picks the parasite catch /
// attack within 3 m, the last form does the ground attack (1/0x1F) 20 % of the time; otherwise the
// floor move of em32NextWalkSet. A target more than 75 deg off and 3.5 m away cuts the roar short
// with a turn. At 1 HP in the west half it looks for a jump up.
static void em32_R1_Threat(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    int mode;
    int ret;

    w->flags |= 0x10;
    switch (step) {
    case 0: {
        int hokan = 10;

        em->atari.on();
        if (em->r_no_3) {
            w->flags |= 0x40;
            hokan = 0;
        }
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x4D), ARC(0x4E), hokan, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 9, 0, w->espKind[2], em, (void*) step);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x3E), 0, hokan, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x83), ARC(0x84), hokan, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x23, 0, w->espKind[2], em, (void*) step);
            break;
        }
        em->r_no_2++;
    }
    case 1:
        ret = MotionMove(em, 0);
        if (ret) {
            mode = w->mode;
            switch (mode) {
            case 0:
            default:
                if ((w->flags & 1) && w->plAngAbs < 1.04719758f && w->plDist2 < 6250000.0f) {
                    if (Rnd() % 10 > 4 && w->plDist2 < 4000000.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0x10, 0, 0);
                    }
                    return;
                }
                break;
            case 1:
                if (w->routeAngAbs < 0.52359879f && em->l_pl < 9000000.0f && Rnd() % 10 > 4) {
                    EmRoutineSet(em, mode, 0x1D, 0, 0);
                    return;
                }
                if (w->routeAngAbs < 0.785398185f && em->l_pl < 9000000.0f) {
                    EmRoutineSet(em, 1, 0x1C, 0, 0);
                    return;
                }
                break;
            case 2:
                if (Rnd() % 10 > 7) {
                    EmRoutineSet(em, 1, 0x1F, 0, 0);
                    return;
                }
                break;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                if (w->routeAngAbs < 0.52359879f && em->l_pl < 25000000.0f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                }
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        } else if (w->targetAngAbs > 1.30899692f && em->l_pl > 12250000.0f) {
            EmRoutineSet(em, 1, 0xC, ret, ret);
        }
        break;
    }
    if (em->hp <= 1 && em->pos.x <= 25338.0f) {
        em32JumpUpCk(em);
    }
}

// The ambush swipe: a quarter turn to the side the player is on, the claws hit during the motion.
static void em32_R1_AmbushAtk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    f32 ang;

    w->flags |= 0x10;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, &em->Motion, ARC(0x4B), ARC(0x4C), 10, 1, 0);
            w->turnAng = em->ang.y - 1.57079637f;
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x49), ARC(0x4A), 10, 1, 0);
            w->turnAng = em->ang.y + 1.57079637f;
        }
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        w->Atk_ck = 0;
        w->timer2 = 24;
        w->timer3 = 0;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 8) {
            ang = Muku(&em->pos, &w->targetPos, w->turnAng, 0.0981747732f);
            w->turnAng += ang;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->ang.y += ang;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            EM32_ATK_END_CK(em, w, 16000000.0f, 4);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (em->l_pl < 25000000.0f) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                EM32_CLAW_ATK(em, 0);
            }
            if (w->timer3) {
                w->timer3--;
            } else if (w->timer2 && w->Atk_ck == 0 && em->l_pl < 36000000.0f) {
                w->timer2--;
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32SitAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, w->Atk_ck);
            }
        }
        break;
    }
}

// Routine 1/0xF: the close bite of form 0. The claw hit (EM32_CLAW_ATK attack 0) runs on motion
// event bit 0; a miss awards the escape point. Ends with the step / jump checks (EM32_ATK_END_CK),
// then a turn, a 60-frame roar (within 5 m on Game_level up to 9, or after a hit) or the walk.
static void em32_R1_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x45), ARC(0x46), 15, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0xB, 0, w->espKind[2], em, (void*) step);
        w->Atk_ck = step;
        w->timer2 = 30;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            EM32_ATK_END_CK(em, w, 16000000.0f, 7);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if ((em->l_pl < 25000000.0f && pG->Game_level <= 9) || w->Atk_ck) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            EM32_CLAW_ATK(em, 0);
        }
        break;
    }
}

// Branch check of the catch: while alive, routed to the player and catch-enabled (flags 0x80000),
// the claw hit (attack 1) runs on motion event bit 0; a hit sets stat 0x01110000, which the routine
// table reads as CatchHit (1/0x11).
static void em32_R1_br_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (em->hp > 0 && em->r_no_2 != 0) {
        int lost = !(w->flags & 1);
        if (lost == 0 && (w->flags & 0x80000) && (em->Motion.Seq_old.Free & 1)) {
            f32 ang = GetXZAngle(&pPL->pos, &em->pos);
            fabsf(Muku2(pPL->ang.y, ang, 3.14159274f));
            EM32_CLAW_ATK(em, 1);
            if (w->Atk_ck) {
                EmRoutineSetW(em, 1, 0x11, 0, 0);
            }
        }
    }
}

// Routine 1/0x10: the grab swing of form 0 (the hit lives in em32_R1_br_Catch). When the motion
// ends the player got away: escape point, then the same exits as em32_R1_Atk.
static void em32_R1_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x13), ARC(0x14), 15, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x15, 0, w->espKind[2], em, (void*) step);
        w->Atk_ck = step;
        w->timer2 = 30;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            EM32_ATK_END_CK(em, w, 16000000.0f, 7);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if ((em->l_pl < 25000000.0f && pG->Game_level <= 9) || w->Atk_ck) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

// The player is caught in the claws: shaken (the button mash frees him), then bitten or thrown.
static void em32_R1_CatchHit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    int dead;

    w->flags |= 0x800;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x2B), 0, 0, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 3.14159274f, 1, -244.559998f, 0.0f, -1746.93994f, plem32_CatchHit);
        GameAddPoint(LVADD_PL_DAMAGE);
        PlGachaInit();
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        w->sndId2 = SndCall(8, 0x15, &em->pos, em->id, 0, em);
        w->timer = step;
        w->timer2 = step;
        w->TmpU32 = step;
        em->dmg.set(0, 0);
        em->r_no_2++;
    case 1:
        if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
            em->dmg.m_Timer = 2;
            em->r_no_2 = 4;
            break;
        }
        if (w->timer2) {
            w->timer2--;
        } else {
            w->timer2 = 4;
            EstSet(em, -1, 0, 0, EFF_EM32, 7, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        if (em->Motion.Seq_frame > 9.69999981f && em->Motion.Seq_frame < 10.3000002f) {
            w->TmpU32 = SndCall(8, 0xE, &em->pos, em->id, 0, em);
        }
        if (em->Motion.Seq_frame > 29.7000008f && em->Motion.Seq_frame < 30.2999992f) {
            SndCall(8, 0x12, &pPL->pos, em->id, 0, pPL);
        }
        LifeDownSet2(pPL, 20, 0, 1);
        PlGachaMove();
        if ((s16) pG->pl_life > 1 && (u32) PlGachaGet() > 30) {
            em->r_no_2 = 2;
            break;
        }
        dead = EmDeadCk(em);
        if (dead) {
            em->r_no_2 = 2;
            break;
        }
        if (em32BetweenHitCk(em)) {
            SndStop(w->TmpU32, 0);
            SndStop(w->sndId2, 0);
            em->r_no_0 = 2;
            em->r_no_1 = dead;
            em->r_no_2 = dead;
            em->r_no_3 = dead;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x2C), ARC(0x2D), 0, 1, 0);
        EmCatchPLSet(em, 3.14159274f, 1, 244.399994f, 0.0f, -1558.33997f, plem32_CatchHit);
        pPL->r_no_2 = step;
        w->timer = 15;
        em->r_no_2++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            em->atari.on();
            if (MotionMove(em, 0)) {
                if ((w->flags & 0x1000) && em32StepUpCk2(em)) {
                    break;
                }
                if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                    break;
                }
                if (Rnd() % 10 > 4 && em->l_pl < 16000000.0f && em32StepUpCk2(em)) {
                    break;
                }
                if (w->targetAngAbs > 1.30899692f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                } else {
                    w->wait = 60;
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                break;
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            SndStop(w->TmpU32, 0);
            SndStop(w->sndId2, 0);
        }
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x2E), ARC(0x2F), 0, 1, 0);
        EmCatchPLSet(em, 3.14159274f, 1, 0.0f, 0.0f, -1456.30005f, plem32_CatchHit);
        pPL->r_no_2 = step;
        pG->pl_life = 0;
        em->r_no_2++;
    case 5:
        em->dmg.m_Timer = 2;
        EmCatchMotionMove(em, 1.0f, 1.0f);
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage callback of em32_R1_CatchHit (EmCatchPLSet: the player follows the enemy's
// motion). r_no_2 is driven by the enemy: 0/1 held and shaken while stat 0x0111.... says the grab is
// on, 2/3 dropped (thrown clear, collision back on, damage ends), 4/5 the death bite (rumble at frame 40).
static void plem32_CatchHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x89), 0, 0, 1, 0);
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pPL->pEmCatch->r_no_0 == 1 && pPL->pEmCatch->r_no_1 == 0x11) {
            break;
        }
        goto end;
    case 2:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x8A), 0, 0, 1, 0);
        EstSet(pl, -1, 0, 0, EFF_EM32, 0x29, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->m_Work0 = 15;
        pl->r_no_2++;
    case 3:
        if (pl->m_Work0) {
            pl->m_Work0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else if (MotionMove(pl, 0)) {
            pl->atari.on();
        end:
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x8B), 0, 0, 1, 0);
        pl->r_no_2++;
    case 5:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pl->Motion.Seq_frame > 39.7000008f && pl->Motion.Seq_frame < 40.2999992f) {
            VibSetData(VIB_TBL, 0xB, 1);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// The long reach attack: a lunge at the player (escape prompt while he is in front).
static void em32_R1_LongAtk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    int hit;
    Mtx inv;
    Vec lp;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x47), ARC(0x48), 10, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0xA, 0, w->espKind[2], em, (void*) step);
        EM32_W_FRESH(w);   // COMPILER-DIFF #12
        w->Atk_ck = step;
        w->longAtkWait = 600;
        w->timer2 = 25;
        w->timer3 = 15;
        w->timer = 10;
        if (pG->Game_level <= 3) {
            w->timer2 = 27;
            w->timer3 = 13;
        }
        if (pG->Game_level <= 1) {
            w->timer3 = 10;
            w->timer2 = 30;
        }
        if (pG->Game_level > 6) {
            w->timer2 = 22;
            w->timer3 = 18;
        }
        if (pG->Game_level > 9) {
            w->timer2 = 20;
            w->timer3 = 20;
        }
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.196349546f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            EM32_ATK_END_CK(em, w, 16000000.0f, 7);
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if ((em->l_pl < 25000000.0f && pG->Game_level <= 9) || w->Atk_ck) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else {
            if (em->Motion.Seq_old.Free & 1) {
                EM32_CLAW_ATK(em, 2);
            }
            if (w->timer3) {
                w->timer3--;
            } else if (w->timer2) {
                hit = w->Atk_ck;
                if (hit == 0) {
                    w->timer2--;
                    PSMTXInverse(em->mat, inv);
                    PSMTXMultVec(inv, &pPL->pos, &lp);
                    if (lp.x > -1000.0f && lp.x < 1000.0f && lp.z > -500.0f && lp.z < 7000.0f) {
                        ActBtn.set(ACT_GUARD, 0xB, (void*) em32EscapeAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, hit);
                    }
                }
            }
        }
        break;
    }
}

// Action button of the ambush swipe: the player ducks (plemSit) and gets a critical-hit rank point.
static void em32SitAction(cEm32* em)
{
    SetPlDamage(em, plemSit);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Action button of the ceiling attack: the duck variant that gets up again (plemSit with r_no_3 1).
static void em32SitUpAction(cEm32* em)
{
    SetPlDamage(em, plemSit);
    pPL->r_no_3 = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player ducks under the swipe (xFF: gets up again).
static void plemSit(cPlayer* pl)
{
    f32 ang;

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    switch (pl->r_no_2) {
    case 0:
        ang = fabsf(Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 3.14159274f));
        if (pl->r_no_3) {
            if (ang < 1.57079637f) {
                MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x8F), 0, 3, 1, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x8E), 0, 3, 1, 0);
            }
        } else {
            if (ang < 1.57079637f) {
                MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x90), 0, 3, 1, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x91), 0, 3, 1, 0);
            }
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

// Action button of the tunnel attack: the player jumps back (plemBackjump; actionSet tells the
// attack the prompt was taken) and gets a critical-hit rank point.
static void em32BackjumpAction(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->actionSet = 1;
    SetPlDamage(em, plemBackjump);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player jumps back out of the tunnel attack, facing away from the enemy.
static void plemBackjump(cPlayer* pEm)
{
    int fe;
    f32 ry;
    f32 d;

    pEm->subArc = pEm->pEmCatch->subArc;
    fe = pEm->r_no_2;
    pEm->dmg.m_Timer = 0x1E;
    switch (fe) {
    case 0:
        ry = LIMIT_ANGLE(pEm->pEmCatch->ang.y + 1.57079637f);
        d = fabsf(Muku2(pEm->ang.y, ry, 3.14159274f));
        if (d < 0.785398185f) {
            pEm->ang.y = ry;
        }
        if (d > 2.35619450f) {
            pEm->ang.y = ry + 3.14159274f;
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        }
        MotionSetCore(pEm, &pEm->Motion, EM_ARC(pEm, 0x94), 0, 3, 1, 0);
        EstSet(pEm, -1, 0, 0, EFF_PL00, 0x14, 0, ESP_CORE_KIND_NONE, pEm, (void*) fe);
        SndCall(1, 0x43, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        SndCall(1, 0x44, &pEm->getPartsPtr(4)->world, 0, 0, pEm);
        GameAddPoint(LVADD_ESCAPEATTACK);
        pEm->m_Work0 = 45;
        pEm->m_Work1 = fe;
        pEm->r_no_2++;
    case 1:
        if (pEm->m_Work0) {
            pEm->m_Work0--;
        } else if (Key.on & 0x1F) {
            pEm->m_Work1 = 1;
        }
        if (pEm->Motion.Seq_frame > 10.6999998f && pEm->Motion.Seq_frame < 11.3000002f) {
            SndCall(1, 0x4F, &pEm->pos, 0, 0, pEm);
        }
        if (pEm->Motion.Seq_frame > 21.7000008f && pEm->Motion.Seq_frame < 22.2999992f) {
            SndCall(5, 0x14, &pEm->pos, 0, 0, pEm);
        }
        if ((pEm->Motion.Seq_frame > 36.7000008f && pEm->Motion.Seq_frame < 37.2999992f) ||
            (pEm->Motion.Seq_frame > 49.7000008f && pEm->Motion.Seq_frame < 50.2999992f)) {
            SndCall(5, 2, &pEm->pos, 0, 0, pEm);
        }
        if ((pEm->Motion.Seq_frame > 37.7000008f && pEm->Motion.Seq_frame < 38.2999992f) ||
            (pEm->Motion.Seq_frame > 50.7000008f && pEm->Motion.Seq_frame < 51.2999992f)) {
            SndCall(5, 3, &pEm->pos, 0, 0, pEm);
        }
        if (MotionMove(pEm, 0) || pEm->m_Work1) {
            EndPlDamage();
        }
        break;
    }
    pEm->subArc = pEm->subArc2;
}

// Action button of the long lunge: the player rolls aside (plemEscape) and gets a critical-hit
// rank point.
static void em32EscapeAction(cEm32* em)
{
    SetPlDamage(em, plemEscape);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player escapes the lunge with a side roll (away from the wall when one is near).
static void plemEscape(cPlayer* pl)
{
    Vec a;
    Vec b;
    int side;

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.m_Timer = 0x1E;
    switch (pl->r_no_2) {
    case 0:
        // The side from the angle is overridden by the random pick right after (dead in the
        // original too: only the compare's 0.0 load survives, hoisted before Rnd).
        if (Muku(&pl->pEmCatch->pos, &pl->pos, pl->ang.y, 3.14159274f) > 0.0f) {
            side = 1;
        } else {
            side = 0;
        }
        side = Rnd() & 1;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            side = 1;
        }
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = -2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            side = 0;
        }
        if (side) {
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x92), EM_ARC(pl, 0x93), 3, 1, 0);
        } else {
            MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x92), EM_ARC(pl, 0x93), 3, 0x41, 0);
        }
        GameAddPoint(LVADD_ESCAPEATTACK);
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pl->m_Work0 = 50;
        pl->m_Work1 = 15;
        pl->r_no_2++;
    case 1:
        em32EscapeCamMove((cEm32*)pl->pEmCatch);
        if (pl->Motion.Seq_frame > 11.6999998f && pl->Motion.Seq_frame < 12.3000002f) {
            EstSet(0, -1, &pl->pos, 0, EFF_PL00, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->m_Work1) {
            pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 0.392699093f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        if (MotionMove(pl, 0)) {
            pl->m_Work0 = 0;
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

// Escape camera: a fixed view behind the player, pulled in front of the nearest wall.
void em32EscapeCamMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Camera* cam = &pG->Camera;
    Vec pos;
    Vec at;
    Vec hit;
    Vec d;
    f32 len;

    w->cam.param.fovy = cam->param.fovy;
    pos.x = -376.0f;
    pos.y = 575.0f;
    pos.z = -1831.0f;
    at.x = -244.0f;
    at.y = 809.0f;
    at.z = 52.5999985f;
    PSMTXMultVec(pPL->mat, &pos, &pos);
    PSMTXMultVec(pPL->mat, &at, &at);
    PosToPos(&cam->param.at, &at, &w->cam.param.at, 1.0f);
    PosToPos(&cam->param.pos, &pos, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 3617 "D:/Bio4/Prog/em32.cpp"
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

// Step up onto a container: the run-up towards stepPos, then the tunnel attack / wait / step down.
static void em32_R1_StepUp(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec v;
    Vec* pos;
    int ret;

    w->flags |= 0x940;
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x1B), ARC(0x1C), 10, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x57), ARC(0x58), 10, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x57), ARC(0x58), 10, 1, 0);
            break;
        }
        PSVECSubtract(&w->stepPos, &em->pos, &w->spd);
        w->spd.y = 0.0f;
#line 3669 "D:/Bio4/Prog/em32.cpp"
        VECNormalize(&w->spd, &v);
        PSVECScale(&v, &v, 1000.0f);
        PSVECAdd(&w->spd, &v, &w->spd);
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            if (w->flags & 0x1000) {
                pos = &em->pos;
                em->ang.y = LIMIT_ANGLE(em->ang.y += Muku2(em->ang.y, w->stepAng, 0.392699093f));
            } else {
                em->ang.y = LIMIT_ANGLE(em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.392699093f));
                pos = &em->pos;
            }
            PSVECScale(&w->spd, &v, 0.1f);
            PSVECAdd(pos, &v, pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (MotionMove(em, 0)) {
            w->flags |= 0x400;
            if (em32TunnelAtkCk(em)) {
                break;
            }
            if (em->r_no_3) {
                EmRoutineSet(em, 1, 0x14, 0, 0);
            } else if (em32PlInTunnelCk(em)) {
                EmRoutineSet(em, 1, 0x14, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x15, 0, 0);
            }
        }
        break;
    }
}

// Routine 1/0x14: idling on top of a container for 90 frames (flags 0x940: on a container, no stage
// collision, player found). Attacks through the tunnel below when a player is there (em32TunnelAtkCk;
// with r_no_3 set only after the wait) or reacts to the player entering it (em32PlInTunnelCk); when
// the wait runs out it steps down (1/0x15).
static void em32_R1_StepWait(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int timer;

    w->flags |= 0x940;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(9), 0, 10, 1, 0);
        w->timer = 90;
        em->r_no_2++;
    case 1:
        w->flags |= 0x400;
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        }
        if ((em->r_no_3 == 0 || w->timer == 0) && em32TunnelAtkCk(em)) {
            break;
        }
        if (em32PlInTunnelCk(em)) {
            break;
        }
        timer = w->timer;
        if (timer == 0) {
            em->r_no_0 = 1;
            em->r_no_1 = 0x15;
            em->r_no_2 = timer;
            em->r_no_3 = timer;
        }
        break;
    }
    em32BreathSe(em);
}

// Routine 1/0x15: climbs down from a container to the spot em32GetStepDownPos picked, with its
// dust effect; on motion event bit 0 it turns (to stepAng when a step-up target is wanted, else to
// the player) and covers 30 % of the remaining distance. Then a wanted step up goes to Ambush (1/7),
// otherwise a new 150..300 frame attack wait and the floor move of em32NextWalkSet.
static void em32_R1_StepDown(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    Vec v;
    Vec* pos;
    int flag;

    w->flags |= 0x940;
    switch (step) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x1D), ARC(0x1E), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x18, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x59), ARC(0x5A), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x19, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x59), ARC(0x5A), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x19, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            break;
        }
        em32GetStepDownPos(em);
        PSVECSubtract(&w->stepPos, &em->pos, &w->spd);
        w->spd.y = 0.0f;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 1) {
            if (w->flags & 0x1000) {
                pos = &em->pos;
                em->ang.y = LIMIT_ANGLE(em->ang.y += Muku2(em->ang.y, w->stepAng, 0.392699093f));
            } else {
                em->ang.y = LIMIT_ANGLE(em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.392699093f));
                pos = &em->pos;
            }
            PSVECScale(&w->spd, &v, 0.3f);
            PSVECAdd(pos, &v, pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (MotionMove(em, 0)) {
            w->flags &= ~0x400;
            flag = w->flags & 0x1000;
            if (flag) {
                w->flags &= ~0x1000;
                EmRoutineSet(em, 1, 7, 0, 0);
            } else {
                w->atkWait = Rnd() % 150 + 150;
                if (w->targetAngAbs > 1.30899692f) {
                    EmRoutineSet(em, 1, 0xC, flag, flag);
                } else if (w->mode == 2) {
                    EmRoutineSet(em, 1, 0xB, flag, flag);
                } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                    EmRoutineSet(em, 1, 8, flag, flag);
                } else {
                    EmRoutineSet(em, 1, 9, flag, flag);
                }
            }
        }
        break;
    }
}

// The tunnel attack from the container roof: a roar (back jump prompt), then the claw sweep.
static void em32_R1_TunnelAtk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero;
    int act;

    w->flags |= 0xC50;
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x4D), 0, 10, 5, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x3E), 0, 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0xA), 0, 30, 5, 0);
            break;
        }
        w->actionSet = 0;
        w->Atk_ck = 0;
        w->timer = 15;
        if (pG->Game_level <= 3) {
            w->timer2 = 18;
        }
        if (pG->Game_level <= 1) {
            w->timer2 = 20;
        }
        if (pG->Game_level > 6) {
            w->timer2 = 12;
        }
        if (pG->Game_level > 9) {
            w->timer2 = 10;
        }
        w->TmpU32 = Rnd() & 1;
        em->r_no_2++;
    case 1:
        act = w->actionSet;
        if (act) {
            w->timer = 0;
        } else {
            switch (w->TmpU32) {
            case 0:
            default:
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32BackjumpAction, em, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, act);
                break;
            case 1:
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32BackjumpAction, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, act);
                break;
            }
            if (w->actionSet) {
                w->timer = 0;
            }
        }
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        zero = 0;
        MotionSetCore(em, &em->Motion, ARC(0x55), ARC(0x56), 10, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x21, 0, ESP_CORE_KIND_NONE, em, (void*) zero);
        w->Atk_ck = zero;
        w->timer = 1;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0x14, 0, 1);
            break;
        }
        if ((em->Motion.Seq_old.Free & 4) && w->Atk_ck == 0 && em->r_no_3 == 0) {
            w->breakNo2 = w->pPoint->no;
            if (em32TunnelAtkCk(em)) {
                em->r_no_3 = 1;
                break;
            }
        }
        if (w->timer && w->Atk_ck == 0 && w->actionSet == 0) {
            switch (w->TmpU32) {
            case 0:
            default:
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32BackjumpAction, em, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
                break;
            case 1:
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32BackjumpAction, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                break;
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            EM32_TAIL_ATK(em, 5);
            w->timer = 0;
        }
        break;
    }
}

// Jump up onto the container roof (pPoint: the jump point).
static void em32_R1_JumpUp(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec v;
    Em32Point* pt;

    w->flags |= 0x940;
    switch (em->r_no_2) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0xF), ARC(0x10), 10, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x73), ARC(0x74), 10, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x73), ARC(0x74), 10, 1, 0);
            break;
        }
        PSVECSubtract(&w->pPoint->pos, &em->pos, &w->spd);
        w->spd.y = 0.0f;
        em->r_no_2++;
    case 1:
        if (em->Motion.Seq_old.Free & 4) {
            PSVECScale(&w->spd, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (em->Motion.Seq_old.Free & 1) {
            pt = w->pPoint;
            if (pt->flag == 0) {
                pt->flag = 1;
                w->breakNo = w->pPoint->no;
                EstSet(0, -1, &w->pPoint->pos, 0, EFF_ROOM, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        if (MotionMove(em, 0)) {
            w->flags |= 0x80;
            EmRoutineSet(em, 1, 0x19, 0, 0);
        }
        break;
    }
}

// Jump down from the roof (pPoint: the landing point, or straight down).
static void em32_R1_JumpDown(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step;
    Vec v;
    Em32Point* pt;
    int ret;

    w->flags |= 0x940;
    em32SetYarareMark(em, 1);
    step = em->r_no_2;
    switch (step) {
    case 0:
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x11), ARC(0x12), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x16, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0xA9), ARC(0xAA), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x17, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0xA9), ARC(0xAA), 10, 1, 0);
            EstSet(em, -1, 0, 0, EFF_EM32, 0x17, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            break;
        }
        EstSet(em, -1, 0, 0, EFF_EM32, 4, 0, w->espKind[0], em, 0);
        if (w->pPoint) {
            PSVECSubtract(&w->pPoint->pos, &em->pos, &w->spd);
        } else {
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
        }
        em->be_flag |= 2;
        w->spd.y = 0.0f;
        em->r_no_2++;
    case 1:
        em->invisible_factor += 0.1f;
        if (em->invisible_factor > 1.0f) {
            em->invisible_factor = 1.0f;
        }
        if (em->Motion.Seq_old.Free & 4) {
            PSVECScale(&w->spd, &v, 0.15f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (em->Motion.Seq_old.Free & 1) {
            pt = w->pPoint;
            if (pt && pt->flag == 0) {
                pt->flag = 1;
                w->breakNo = w->pPoint->no;
                EstSet(0, -1, &w->pPoint->pos, 0, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        if (MotionMove(em, 0)) {
            w->flags &= ~0x80;
            w->atkWait = Rnd() % 150 + 150;
            ret = em->r_no_3;
            if (ret) {
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, ret, ret);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, ret, ret);
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, ret, ret);
            } else {
                EmRoutineSet(em, 1, 9, ret, ret);
            }
        }
        break;
    }
}

// On the container roof: the wait before the ceiling attack (the player hides in the tunnel).
static void em32_R1_C_Wait(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec v;
    int timer;

    em->dmg.m_Timer = 2;
    w->flags |= 0x840;
    em32SetYarareMark(em, 0);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0xF), 0, 10, 1, 0);
        w->timer = 15;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        timer = w->timer;
        if (timer) {
            w->timer--;
            break;
        }
        em->invisible_factor -= 0.1f;
        if (em->invisible_factor < 0.0f) {
            em->invisible_factor = 0.0f;
            em->be_flag &= ~2;
            if (em->hp <= 1) {
                em->hp = timer;
            }
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(9), 0, 10, 1, 0);
        EM32_EFFECT_DELETE(w->espKind[0], em);
        EM32_W_FRESH(w);   // COMPILER-DIFF #12
        (w->timer = Rnd() % 30 + 60);
        if (pG->Game_level <= 3) {
            w->timer = Rnd() % 60 + 90;
        }
        if (pG->Game_level <= 1) {
            w->timer = Rnd() % 60 + 120;
        }
        if (pG->Game_level > 6) {
            w->timer = Rnd() % 60 + 45;
        }
        if (pG->Game_level > 9) {
            w->timer2 = Rnd() % 60 + 30;
        }
        em->r_no_2++;
    case 3:
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(pPL->mat, &v, &em->pos);
        em->pos.y = pPL->pos.y + 6000.0f;
        em->ang.y = pPL->ang.y + 3.14159274f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionMove(em, 0);
        if (em->hp <= 0) {
            break;
        }
        {
            int t = w->timer;
            if (t > 44 && (pPL->r_no_0 != 0 || (u32) (pPL->r_no_1 - 1) > 2)) {
            } else if (t) {
                w->timer = t - 1;
            } else {
                // The countdown at zero skips the reload test (the shared test block is reached by the
                // hold path and the decrement path only).
                goto ceiling;
            }
        }
        if (w->timer) {
            break;
        }
    ceiling:
        if (em32CeilingAtkCk(em)) {
            break;
        }
        em32JumpDownCk(em);
        break;
    }
    em32BreathSe(em);
}

// Branch check of the ceiling drop: while alive and past the setup step, the claw hit (attack 3)
// runs on motion event bit 0; a hit sets stat 0x011B0000, which the table reads as C_AtkHit (1/0x1B).
static void em32_R1_br_C_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (em->hp > 0 && em->r_no_2 != 0) {
        if (em->Motion.Seq_old.Free & 1) {
            EM32_CLAW_ATK(em, 3);
        }
        if (w->Atk_ck) {
            EmRoutineSetW(em, 1, 0x1B, 0, 0);
        }
    }
}

// The ceiling attack: the drop from the roof onto the player (sit prompt while he is in front).
static void em32_R1_C_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step;
    Vec v;
    Em32Point* pt;
    int x10;

    w->flags |= 0x840;
    em32SetYarareMark(em, 0);
    step = em->r_no_2;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x15), ARC(0x16), 3, 1, 0);
        PSVECSubtract(&w->pPoint->pos, &em->pos, &w->spd);
        w->timer2 = 25;
        w->spd.y = 0.0f;
        w->Atk_ck = step;
        w->timer = step;
        em32Timer2SetW(w, 37, 30, 23, 20);
        em->invisible_factor = 0.0f;
        em->be_flag |= 2;
        EstSet(em, -1, 0, 0, EFF_EM32, 0x12, 0, ESP_CORE_KIND_NONE, em, (void*) step);
        w->TmpU32 = Rnd() & 1;
        if (pG->Game_level <= 3) {
            w->TmpU32 = step;
        }
        em->r_no_2++;
    case 1:
        em->invisible_factor += 0.1f;
        if (em->invisible_factor > 1.0f) {
            em->invisible_factor = 1.0f;
        }
        if (em->Motion.Seq_old.Free & 4) {
            PSVECScale(&w->spd, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->spd, &v, &w->spd);
        }
        if (em->Motion.Seq_old.Free & 2) {
            pt = w->pPoint;
            if (pt->flag == 0) {
                pt->flag = 1;
                w->breakNo = w->pPoint->no;
                EstSet(0, -1, &w->pPoint->pos, 0, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            if ((w->Atk_ck || (Rnd() & 1)) && em32JumpDownCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 0x19, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else if (w->timer2 && w->Atk_ck == 0 && w->routeAngAbs < 1.57079637f && em->l_pl < 16000000.0f) {
            x10 = w->TmpU32;
            w->timer2--;
            switch (x10) {
            case 0:
            default:
                ActBtn.set(ACT_STOOP, 0xB, (void*) em32SitUpAction, em, ACTCTR_WEP_SET_IGNORE, DISP_L_R, ACT_FUNC_NORMAL, w->Atk_ck);
                break;
            case 1:
                ActBtn.set(ACT_STOOP, 0xB, (void*) em32SitUpAction, em, ACTCTR_WEP_SET_IGNORE, DISP_A_B, ACT_FUNC_NORMAL, w->Atk_ck);
                break;
            }
        }
        break;
    }
}

// The player is caught from the ceiling: bitten (the head comes off) unless he shakes free.
static void em32_R1_C_AtkHit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step;

    w->flags |= 0x840;
    em->dmg.set(0, 2);
    em32SetYarareMark(em, 0);
    step = em->r_no_2;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x27), ARC(0x28), 0, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 3.14159274f, 1, -83.8300018f, 0.0f, -2411.40991f, plem32_C_AtkHit);
        GameAddPoint(LVADD_PL_DAMAGE);
        PlGachaInit();
        em->atari.off();
        EstSet(em, -1, 0, 0, EFF_EM32, 0xD, 0, ESP_CORE_KIND_NONE, em, (void*) step);
        EstSet(pPL, -1, 0, 0, EFF_EM32, 0x14, 0, w->espKind[1], em, (void*) step);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        w->sndId2 = SndCall(8, 0x15, &em->pos, em->id, 0, em);
        w->timer = 125;
        w->TmpU32 = step;
        w->timer2 = step;
        em->r_no_2++;
    case 1:
        EmCatchMotionMove(em, 1.0f, 1.0f);
        if (w->timer) {
            w->timer--;
            LifeDownSet2(pPL, 20, 0, 1);
            PlGachaMove();
            if ((s16) pG->pl_life > 1 && (u32) PlGachaGet() > 30) {
                em->r_no_2++;
                break;
            }
            if (w->timer2) {
                w->timer2--;
            } else {
                w->timer2 = 4;
                EstSet(em, -1, 0, 0, EFF_EM32, 7, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            em32PlHeadLost();
            pG->pl_life = 0;
            VibSetData(VIB_TBL, 0xB, 1);
        }
        if (em->Motion.Seq_old.Free & 4) {
            em32PlHeadFall();
        }
        if (em->Motion.Seq_frame > 13.6999998f && em->Motion.Seq_frame < 14.3000002f) {
            w->TmpU32 = SndCall(8, 0xE, &em->pos, em->id, 0, em);
        }
        if (em->Motion.Seq_frame > 17.7000008f && em->Motion.Seq_frame < 18.2999992f) {
            SndCall(8, 0x12, &pPL->pos, em->id, 0, pPL);
        }
        if (em->Motion.Seq_frame > 188.699997f && em->Motion.Seq_frame < 189.300003f) {
            em->be_flag &= ~2;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x29), ARC(0x2A), 0, 1, 0);
        EmCatchPLSet(em, 3.14159274f, 1, 58.7799988f, 0.0f, -468.209991f, plem32_C_AtkHit);
        pPL->r_no_2 = step;
        EM32_EFFECT_DELETE(w->espKind[1], em);
        w->timer = 15;
        em->r_no_2++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else if (MotionMove(em, 0)) {
            em->atari.on();
            if ((w->Atk_ck || (Rnd() & 1)) && em32JumpDownCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 0x19, 0, 0);
            break;
        }
        if (em->Motion.Seq_old.Free & 1) {
            SndStop(w->TmpU32, 0);
            SndStop(w->sndId2, 0);
        }
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage callback of em32_R1_C_AtkHit (EmCatchPLSet). r_no_2 is driven by the enemy: 0/1
// held in the claws following the enemy's motion (collision off; the death bite plays out here), 2/3
// shaken free: the drop motion with its effect, 15 frames still attached, then the collision returns
// and the damage ends.
static void plem32_C_AtkHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x87), 0, 0, 0x201, 0);
        pl->atari.off();
        PlSetFace(1);
        pl->r_no_2++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pl->Motion.Seq_frame > 259.700012f && pl->Motion.Seq_frame < 260.299988f) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x88), 0, 0, 0x201, 0);
        EstSet(pl, -1, 0, 0, EFF_EM32, 0x28, 0, ESP_CORE_KIND_NONE, pl, 0);
        pl->m_Work0 = 15;
        pl->r_no_2++;
    case 3:
        if (pl->m_Work0) {
            pl->m_Work0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            pl->atari.on();
            if (MotionMove(pl, 0)) {
                pl->atari.on();
                EndPlDamage();
                pl->dmg.set(0, 30);
            }
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// Tail swing of the second form (parts 0x54..0x5D, the two tips also sweep a line).
static void em32_R1_P_Atk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    cParts* p;
    Vec v;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x71), ARC(0x72), 10, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x1E, 0, ESP_CORE_KIND_NONE, em, (void*) step);
        w->Atk_ck = step;
        w->timer = 15;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.0981747732f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0)) {
            if (w->Atk_ck == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->Atk_ck) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xA, 0, 0);
            } else if (w->routeAngAbs < 0.785398185f && em->l_pl < 9000000.0f) {
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            } else if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    if (em->Motion.Seq_old.Free & 1) {
        EM32_TAIL_ATK(em, 4);
        p = em->getPartsPtr(0x5A);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->world_old);
        p = em->getPartsPtr(0x5D);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->world_old);
    }
}

// Branch check of the second form's catch: like em32_R1_br_Catch, a claw hit (attack 1) sets stat
// 0x011E0000, which the table reads as P_CatchHit (1/0x1E).
static void em32_R1_br_P_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (em->hp > 0 && em->r_no_2 != 0) {
        int lost = !(w->flags & 1);
        if (lost == 0 && (w->flags & 0x80000) && (em->Motion.Seq_old.Free & 1)) {
            f32 ang = GetXZAngle(&pPL->pos, &em->pos);
            fabsf(Muku2(pPL->ang.y, ang, 3.14159274f));
            EM32_CLAW_ATK(em, 1);
            if (w->Atk_ck) {
                EmRoutineSetW(em, 1, 0x1E, 0, 0);
            }
        }
    }
}

// Routine 1/0x1D: the grab swing of the second form (the hit lives in em32_R1_br_P_Catch). A miss
// awards the escape point and exits through the jump / step checks, a turn, a 60-frame roar or the walk.
static void em32_R1_P_Catch(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0xA3), ARC(0xA4), 15, 1, 0);
        w->Atk_ck = step;
        w->timer2 = 30;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                return;
            }
            if ((w->Atk_ck || Rnd() % 10 > 4) && em->l_pl < 16000000.0f && em32StepUpCk2(em)) {
                return;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (em->l_pl < 25000000.0f && pG->Game_level <= 9) {
                w->wait = 60;
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

// The player is caught by the second form: shaken, then cut in two (em32PlDivideSet2).
static void em32_R1_P_CatchHit(cEm32* em)
{
    Em32Work* w;
    int step;

    em->dmg.set(0, 2);
    w = EM32_WK(em);
    step = em->r_no_2;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0xA5), ARC(0xA6), 0, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 3.14159274f, 1, 248.539993f, 0.0f, -3618.96997f, plem32_P_CatchHit);
        GameAddPoint(LVADD_PL_DAMAGE);
        PlGachaInit();
        EstSet(em, -1, 0, 0, EFF_EM32, 0xE, 0, w->espKind[1], em, (void*) step);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        w->sndId2 = SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        w->TmpU32 = step;
        w->timer = step;
        w->timer2 = step;
        em->r_no_2++;
    case 1:
        EmCatchMotionMove(em, 1.0f, 1.0f);
        if (em->Motion.Seq_old.Free & 1) {
            pG->pl_life = 0;
        }
        if ((s16) pG->pl_life > 1) {
            LifeDownSet2(pPL, 20, 0, 1);
            PlGachaMove();
            if ((u32) PlGachaGet() > 30) {
                em->r_no_2++;
                break;
            }
        }
        if (em->Motion.Seq_old.Free & 2) {
            w->TmpU32 = SndCall(8, 0xE, &em->pos, em->id, 0, em);
        }
        if (em32BetweenHitCk(em)) {
            SndStop(w->TmpU32, 0);
            SndStop(w->sndId2, 0);
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0xA7), ARC(0xA8), 0, 1, 0);
        if (w->pDivide[0]) {
            w->pDivide[0]->be_flag &= ~2;
        }
        if (w->pDivide[1]) {
            w->pDivide[1]->be_flag &= ~2;
        }
        EmCatchPLSet(em, 3.14159274f, 1, -127.949997f, 0.0f, -2747.37012f, plem32_P_CatchHit);
        pPL->r_no_2 = step;
        EM32_EFFECT_DELETE(w->espKind[1], em);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x13, 0, ESP_CORE_KIND_NONE, em, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->timer = 15;
        em->r_no_2++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            em->atari.on();
            if (MotionMove(em, 0)) {
                if ((w->flags & 0x1000) && em32StepUpCk2(em)) {
                    break;
                }
                if (w->mode == 0 && Rnd() % 10 > 7 && em32JumpUpCk(em)) {
                    break;
                }
                if (Rnd() % 10 > 4 && em->l_pl < 16000000.0f && em32StepUpCk2(em)) {
                    break;
                }
                if (w->targetAngAbs > 1.30899692f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                } else {
                    w->wait = 60;
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                break;
            }
        }
        if (em->Motion.Seq_old.Free & 1) {
            SndStop(w->TmpU32, 0);
            SndStop(w->sndId2, 0);
        }
        break;
    }
    em->Catch_at_adj = em->pos;
}

// Player damage callback of em32_R1_P_CatchHit (EmCatchPLSet). r_no_2 is driven by the enemy: 0/1
// held following the enemy's motion with the blood effect (region-dependent variant); on the
// uncensored region the player is cut in two at frame 110 (em32PlDivideSet2) and hidden; the damage
// ends when stat stops reading 0x011E....; 2/3 shaken free: the weapon is dropped as a separate
// object hooked to the player's hand for 15 frames (pCatchObj), then destroyed, and the damage ends.
static void plem32_P_CatchHit(cPlayer* pl)
{
    Em32Work* w = EM32_WK(pl->pEmCatch);
    int step;
    cObj* obj;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    step = pl->r_no_2;
    switch (step) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x9F), 0, 0, 1, 0);
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        PlSetFace(1);
        if (pSys->eff_country) {
            EstSet(pl, -1, 0, 0, EFF_EM32, 0x2C, 0, w->espKind[1], pl->pEmCatch, (void*) step);
        } else {
            EstSet(pl, -1, 0, 0, EFF_EM32, 0x2D, 0, w->espKind[1], pl->pEmCatch, (void*) step);
        }
        pl->r_no_2++;
        break;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (pl->Motion.Seq_frame > 109.699997f && pl->Motion.Seq_frame < 110.300003f && pSys->eff_country) {
            em32PlDivideSet2((cEm32*)pl->pEmCatch);
            pl->be_flag &= ~2;
        }
        if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0x1E) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 2:
        pl->be_flag |= 2;
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0xA0), 0, 0, 1, 0);
        pl->Wep->setTrans(0, 0);
        obj = ObjMgr.create(cObjMgr::ID_PL_WEAPON);
        w->pCatchObj = obj;
        if (obj) {
            obj->modelInit(EM_ARC(pl, 0xAC), EM_ARC(pl, 0xAB));
            w->pCatchObj->atari.m_flag &= 0xFCFF;
            w->pCatchObj->pList->pParent = pPL->getPartsPtr(0xA);
            w->pCatchObj->LightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            ((cObjWep*) w->pCatchObj)->m_pParent = pPL;
            w->pCatchObj->getPartsPtr(1)->ang.y = 3.14159274f;
        }
        pl->m_Work0 = 15;
        pl->r_no_2++;
    case 3:
        if (pl->m_Work0) {
            pl->m_Work0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else if (MotionMove(pl, 0)) {
            if (w->pCatchObj) {
                ObjMgr.destroy(w->pCatchObj);
                w->pCatchObj = 0;
            }
            pl->Wep->setTrans(1, 0);
            pl->atari.on();
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->Catch_at_adj = pl->pos;
    pl->subArc = pl->subArc2;
}

// The last form pins the player to the ground (back jump prompt), then stamps and roars.
static void em32_R1_Ground(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    Mtx m;
    Vec v;
    int zero;

    w->flags |= 0x810;
    w->flags |= 0x10000;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x7D), ARC(0x7E), 10, 1, 0);
        w->timer = 30;
        EM32_W_FRESH(w);   // COMPILER-DIFF #12
        (w->timer2 = Rnd() % 90 + 90);
        w->timer3 = 20;
        if (pG->Game_level <= 3) {
            w->timer3 = 25;
        }
        EstSet(em, -1, 0, 0, EFF_EM32, 0x1A, 0, ESP_CORE_KIND_NONE, em, (void*) step);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        } else if (w->timer) {
            w->timer--;
        } else {
            em->atari.off();
        }
        break;
    case 2:
        em->atari.off();
        w->Atk_ck = 0;
        w->actionSet = 0;
        w->TmpU32 = Rnd() & 1;
        em->ang.y = pPL->ang.y;
        GetPlPos(&em->pos, 18.0f, 0);
        PSMTXRotRad(m, 'y', em->ang.y);
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2500.0f;
        PSMTXMultVec(m, &v, &em->pos);
        em->r_no_2++;
    case 3:
        w->flags |= 0x8000;
        if (w->actionSet == 0) {
            em->ang.y = pPL->ang.y;
            GetPlPos(&em->pos, 18.0f, 0);
            PSMTXRotRad(m, 'y', em->ang.y);
            TransMatrix(m, &em->pos);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2500.0f;
            PSMTXMultVec(m, &v, &em->pos);
        }
        MotionSetCore(em, &em->Motion, ARC(0x7F), 0, 0, 1, 0);
        MotionMove(em, 0);
        if (w->timer2) {
            w->timer2--;
        } else {
            em->r_no_2++;
        }
        if (w->timer2 < w->timer3 && w->Atk_ck == 0 && w->actionSet == 0) {
            switch (w->TmpU32) {
            case 0:
            default:
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32BackjumpAction, em, ACTCTR_ENFORCE_EXEC, DISP_L_R, ACT_FUNC_NORMAL, 0);
                break;
            case 1:
                ActBtn.set(ACT_GUARD, 0xB, (void*) em32BackjumpAction, em, ACTCTR_ENFORCE_EXEC, DISP_A_B, ACT_FUNC_NORMAL, 0);
                break;
            }
        }
        break;
    case 4:
        if (w->actionSet == 0) {
            em->ang.y = pPL->ang.y;
            GetPlPos(&em->pos, 18.0f, 0);
            PSMTXRotRad(m, 'y', em->ang.y);
            TransMatrix(m, &em->pos);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2500.0f;
            PSMTXMultVec(m, &v, &em->pos);
        }
        zero = 0;
        MotionSetCore(em, &em->Motion, ARC(0x7F), ARC(0x80), 0, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x1B, 0, ESP_CORE_KIND_NONE, em, (void*) zero);
        w->Atk_ck = zero;
        w->timer = 5;
        em->r_no_2++;
    case 5:
        w->flags |= 0x8000;
        if (MotionMove(em, 0)) {
            if (Rnd() % 10 > 4) {
                w->timer2 = Rnd() % 60 + 20;
                em->r_no_2 = 2;
            } else {
                em->r_no_2++;
            }
        } else if (em->Motion.Seq_old.Free & 1) {
            EM32_TAIL_ATK(em, 4);
        }
        break;
    case 6:
        zero = 0;
        em32GetGroundPos(em);
        MotionSetCore(em, &em->Motion, ARC(0x81), ARC(0x82), 0, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x1C, 0, ESP_CORE_KIND_NONE, em, (void*) zero);
        w->Atk_ck = zero;
        w->timer = 30;
        em->r_no_2++;
    case 7:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else {
            em->atari.on();
        }
        break;
    }
}

// The last form breaks the barred door (the tail sweeps the racks).
static void em32_R1_BreakBarred(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    cParts* p;
    Vec v;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x85), ARC(0x86), 10, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x24, 0, w->espKind[1], em, (void*) step);
        w->Atk_ck = step;
        w->timer = 15;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        break;
    }
    if (em->Motion.Seq_old.Free & 1) {
        EM32_TAIL_ATK(em, 4);
        p = em->getPartsPtr(0x5A);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->world_old);
        p = em->getPartsPtr(0x5D);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        em32AtkCk2(em, 4, &v, &p->world_old);
        em32BreakBarred(em);
    }
}

// Routine 2: the flinch (only em32_R1_Dm_Normal); flags 8 keeps em32DmCk from restarting it.
static void em32_R0_Damage(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 8;
    Em32_R2_move_tbl[em->r_no_1](em);
}

// Routine 2/0: the form's flinch motion with its pain voice and blood effect (none for the flash
// grenade 0x17); the current attack effect is removed. When it ends: a step up within 5 m, else the
// floor move of em32NextWalkSet. On motion event bit 2: a jump up at 1 HP in the west half, or the
// last form's ground attack (1/0x1F) half the time.
static void em32_R1_Dm_Normal(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        switch (w->mode) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x25), ARC(0x26), 3, 1, 0);
            SndCall(8, 0x20, &em->pos, em->id, 0, em);
            if (em->dmg.m_Wep != 0x17) {
                EstSet(em, -1, 0, 0, EFF_EM32, 0x1D, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            }
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x69), ARC(0x6A), 3, 1, 0);
            SndCall(8, 0x20, &em->pos, em->id, 0, em);
            if (em->dmg.m_Wep != 0x17) {
                EstSet(em, -1, 0, 0, EFF_EM32, 0x1F, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            }
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x6B), ARC(0x6C), 3, 1, 0);
            SndCall(8, 0x20, &em->pos, em->id, 0, em);
            if (em->dmg.m_Wep != 0x17) {
                EstSet(em, -1, 0, 0, EFF_EM32, 0x20, 0, ESP_CORE_KIND_NONE, em, (void*) step);
            }
            break;
        }
        EM32_EFFECT_DELETE(w->espKind[2], em);
        EstSet(em, -1, 0, 0, EFF_EM32, 8, 0, ESP_CORE_KIND_NONE, em, 0);
        w->x978 = 450;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            if (em->l_pl < 25000000.0f && em32StepUpCk2(em)) {
                return;
            }
            if (w->targetAngAbs > 1.30899692f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (w->mode == 2) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
                return;
            } else if (Rnd() % 10 > 4 || em->l_pl < 49000000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 9, 0, 0);
            }
        }
        if (em->Motion.Seq_old.Free & 4) {
            if (em->hp <= 1 && em->pos.x <= 25338.0f && em32JumpUpCk(em)) {
                return;
            }
            if (w->mode == 2 && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
        }
        break;
    }
}

// Routine 3: death (only em32_R1_Die_Normal); flags 8 blocks further reactions.
static void em32_R0_Die(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    w->flags |= 8;
    Em32_R3_move_tbl[em->r_no_1](em);
}

// Death: the body is set on the floor of the last area, fades and sinks after the drop item.
static void em32_R1_Die_Normal(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int step = em->r_no_2;
    Vec pos = { 54602.0f, 4315.0f, 10346.0f };

    w->flags |= 0x4000;
    switch (step) {
    case 0:
        em->atari.off();
        em->setPos(&pos);
        em->ang.y = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x77), ARC(0x78), 3, 1, 0);
        EM32_EFFECT_DELETE(w->espKind[2], em);
        EM32_EFFECT_DELETE(w->espKind[0], em);
        EM32_EFFECT_DELETE(w->espKind[1], em);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x27, 1, w->espKind[1], em, (void*) step);
        em->clearStatus(EM_STATUS_ACTIVE);
        w->scale = 1.0f;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->setStatus(EM_STATUS_ITEMSET);
            EmSetDropItem(em);
            w->flags |= 0x2000;
            em->r_no_2++;
        }
        break;
    case 2:
        w->timer = 30;
        w->timer2 = 150;
        w->flags |= 0x20;
        w->scale = 1.0f;
        SndCall(8, 0x38, &em->pos, em->id, 0, em);
        EstSet(em, -1, 0, 0, EFF_EM32, 0x2B, 1, w->espKind[1], em, 0);
        em->r_no_2++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            w->scale -= 0.003f;
            if (w->scale < 0.1f) {
                w->scale = 0.1f;
            }
            em->pos.y -= 6.0f;
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

// The route to the player from the chest (parts 0x1C) at the enemy's height; the two line checks
// above it decide whether the player is visible (bit0) and reachable (bit19).
void em32RouteCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    Vec c;
    Vec d;
    cParts* p;

    if (em->hp <= 0) {
        return;
    }
    p = em->getPartsPtr(0x1C);
    a = p->world;
    a.y = em->pos.y;
    RouteCkPosToPos(&a, &pPL->pos, &w->routePos);
    b = a;
    b.y += 1500.0f;
    c.x = pPL->pos.x;
    c.y = pPL->pos.y + 1500.0f;
    c.z = pPL->pos.z;
    if (EatMgr.hitCheck(&b, &c, 0, 0, 0, 0x4000) == 0) {
        w->flags |= 1;
    }
    b = a;
    b.y += 1500.0f;
    c.x = pPL->pos.x;
    c.y = pPL->pos.y + 1500.0f;
    c.z = pPL->pos.z;
    if (EatMgr.hitCheck(&b, &c, 0, 0, 0, 0) == 0) {
        w->flags |= 0x80000;
    }
    w->routeAng = Muku(&a, &w->routePos, em->ang.y, 3.14159274f);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->r_no_0 == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->l_pl = 10000000000000000.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->l_pl;
    w->pTarget = pPL;
    w->flags &= ~4;
    if (DbgFlagChk(pG, DBG_RTP_DISP)) {
        d = em->pos;
        d.y += 250.0f;
        Draw_line3d(&d, &w->targetPos, 0xFFFFFF40, 0);
    }
}

// Head (parts 3) turns towards the player while it walks (addRot.y, damped).
void em32NeckMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cParts* p;
    cParts* pp;
    Vec v;

    if (w->mode == 2) {
        w->flags &= ~0x10;
    }
    em->getPartsPtr(4);
    pp = pPL->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(pp->mat, &v, &v);
    if (w->flags & 0x10) {
        w->neckAng = w->neckAng * 0.899999976f + Muku(&em->pos, &pPL->pos, em->ang.y, 1.04719758f) * 0.100000001f;
    } else {
        w->neckAng = w->neckAng * 0.899999976f;
    }
    p = (cParts*) em->getPartsPtr(3);
    p->motParts.flags |= 0x40000000;
    p->inv_offset.x = 0.0f;
    p->inv_offset.y = w->neckAng;
    p->inv_offset.z = 0.0f;
}

// Sets up the last form's tail as a 12-node pendulum cloth chain (three bundles, gravity 40, no
// wind, 100 mm segments) over the em32_cloth_* part tables; em32ClothMove drives it.
void em32ClothSet(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    int zero = 0;

    w->cloth.Num = 12;
    w->cloth.pCloth = em32_cloth_parts;
    w->cloth.pLeft = (u8*) zero;
    w->cloth.pRight = (u8*) zero;
    w->cloth.pUpLeft = (u8*) zero;
    w->cloth.pUpRight = zero;
    w->cloth.pParent = em32_cloth_up;
    w->cloth.pChild = em32_cloth_down;
    w->cloth.pWindSin = (f32*) zero;
    w->cloth.pWindRate = (f32*) zero;
    w->cloth.pGravity = zero;
    w->cloth.pRate = (f32*) zero;
    w->cloth.pMax = em32_cloth_max;
    w->cloth.pAtset = (CLOTH_AT_SET*) zero;
    w->cloth.At_num = zero;
    w->cloth.Gravity = 40.0f;
    w->cloth.Rate = 0.6f;
    w->cloth.Bundle_num = 3;
    w->cloth.WindSin = 0.0f;
    w->cloth.Stretchy = 0.05f;
    w->cloth.Move_rate = 0.0f;
    w->cloth.Flag = zero;
    w->cloth.pPtbl = zero;
    PenClothSet(em, (PenCloth*) &w->cloth, 100.0f);
}

// The last form's tail follows the cloth chain; the two tail tips copy their parents' matrices.
void em32ClothMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cParts* p;
    cParts* p2;

    if (w->mode != 2) {
        return;
    }
    if (w->flags & 0x8000) {
        return;
    }
    PenClothMove2(em, (PenCloth*) &w->cloth);
    p = em->getPartsPtr(0x16);
    p2 = em->getPartsPtr(0x51);
    PSMTXCopy(p->mat, p2->mat);
    p = em->getPartsPtr(0x17);
    p2 = em->getPartsPtr(0x52);
    PSMTXCopy(p->mat, p2->mat);
}

// The two-motion turn blend of the walk / dash / attack walk: m0 (sequence m3) is the straight
// motion, blended with m1 (sequence a) when blendVal is positive or m2 (b) when negative, at weight
// |blendVal| / 256 through the second motion work (Motion.blend). blendCnt is the MotionSetCore
// interpolation count and blendSeq the frame, both kept in step by this call; `d` the motion flags.
void em32BlendMotSet(cEm32* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int d)
{
    Em32Work* w = EM32_WK(em);
    MotionWork* bm;
    f32 val = fabsf(w->blendVal);
    void* m;
    int arg;

    MotionSetCore(em, &em->Motion, m0, (void*) m3, (u8) w->blendCnt, (u16) d, (u16) w->blendSeq);
    if (w->blendVal > 0.0f) {
        m = m1;
        arg = a;
    } else {
        m = m2;
        arg = b;
    }
    bm = EM32_BLEND_MOT(w);
    MotionSetCore(em, bm, m, (void*) arg, (u8) w->blendCnt, (u16) d, (u16) w->blendSeq);
    em->Motion.blend = bm;
    bm->Brate = val * 0.00390625f;
    if (w->blendCnt) {
        w->blendCnt--;
    }
    w->blendSeq++;
    if ((u32) w->blendSeq >= em->Motion.Seq_frame_num) {
        w->blendSeq = 0;
    }
}

// Tests attack `no` swept from part `parts`' previous to its current world position (em32AtkCk2).
int em32AtkCk(cEm32* em, int no, int parts)
{
    cParts* p = em->getPartsPtr(parts);

    return em32AtkCk2(em, no, &p->world, &p->world_old);
}

// The attack box `no` swept from oldPos to pos against the player / partner.
int em32AtkCk2(cEm32* em, int no, Vec* pos, Vec* oldPos)
{
    Em32Work* w = EM32_WK(em);
    int hit;

    if (w->Atk_ck) {
        return 0;
    }
    hit = EmAtkHitCk(&em32_atk_tbl[no], pos, oldPos, 0);
    if (hit) {
        if (hit & 1) {
            w->Atk_ck = 1;
            EmPlBloodSet2(em, pos, 1, 0x2A, 0x2A);
            switch ((u32) no) {
            case 0:
                SndCall(8, 7, &em->pos, em->id, 0, em);
                pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(PL_DM_AUTO, 0, 0);
                break;
            case 4:
                SndCall(8, 0x1B, &em->pos, em->id, 0, em);
                if ((s16) pG->pl_life <= 0) {
                    em32PlDivideSet(em);
                }
                break;
            case 2:
                SndCall(8, 7, &em->pos, em->id, 0, em);
                pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(PL_DM_AUTO, 0, 0);
                break;
            }
        }
        if (hit & 2) {
            EmSubBloodSet(em, pos, 1, 0xFF, 0xFF);
            w->Atk_ck = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        VibSetData(VIB_TBL, 7, 1);
        return 1;
    }
    return 0;
}

// A step-up point (EMI type 0x11, sub 0) within 6 m and 60 degrees of the player direction, with
// the player in sight: the run-up starts (routine 0x13).
int em32StepUpCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 ang;
    int i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 1000.0f;
    b.y += 1000.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        return 0;
    }
    ang = GetXZAngle(&em->pos, &pPL->pos);
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);
        int sub;

        if (e->type != 0x11) {
            continue;
        }
        sub = e->sub;
        if (sub != 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            36000000.0f) {
            continue;
        }
        if (!(fabsf(Muku2(ang, GetXZAngle(&em->pos, &e->pos), 3.14159274f)) > 1.04719758f)) {
            w->stepPos = e->pos;
            EmRoutineSet(em, 1, 0x13, sub, sub);
            return 1;
        }
    }
    return 0;
}

// The nearest step-up point on the enemy's side of the two container rows, in sight: the run-up
// starts (xFF set: the player is on the roof).
int em32StepUpCk2(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 best;
    f32 ang;
    f32 d;
    int found;
    int i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    best = 169000000.0f;
    found = 0;
    ang = em->ang.y;
    if (w->flags & 0x1000) {
        ang = GetXZAngle(&em->pos, &w->stepTarget);
    }
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        if ((e->pos.x < -11550.0f && em->pos.x > -11550.0f) || (e->pos.x > -11550.0f && em->pos.x < -11550.0f)) {
            continue;
        }
        if ((e->pos.x < 6921.0f && em->pos.x > 6921.0f) || (e->pos.x > 6921.0f && em->pos.x < 6921.0f)) {
            continue;
        }
        a = em->pos;
        b = e->pos;
        b.y = a.y = em->pos.y + 3000.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            continue;
        }
        if ((w->flags & 0x1000) && fabsf(Muku2(ang, GetXZAngle(&em->pos, &e->pos), 3.14159274f)) > 1.04719758f) {
            continue;
        }
        best = d;
        found = 1;
        w->stepPos = e->pos;
    }
    if (found == 0) {
        return 0;
    }
    if (w->flags & 0x1000) {
        EmRoutineSet(em, 1, 0x13, 0, 0);
    } else {
        em->r_no_1 = 0x13;
        em->r_no_2 = 0;
        em->r_no_0 = 1;
        em->r_no_3 = 1;
    }
    return 1;
}

// Step up when a container roof lies ahead (scenario attribute bit22 in front).
int em32StepUpCk3(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 ang;
    int i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    if (em32PlInTunnelCk(em) && em32StepUpCk2(em)) {
        return 1;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 1600.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x400000)) {
        return 0;
    }
    ang = GetXZAngle(&em->pos, &pPL->pos);
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);
        int sub;

        if (e->type != 0x11) {
            continue;
        }
        sub = e->sub;
        if (sub != 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            36000000.0f) {
            continue;
        }
        if ((e->pos.x < -11550.0f && em->pos.x > -11550.0f) || (e->pos.x > -11550.0f && em->pos.x < -11550.0f)) {
            continue;
        }
        if ((e->pos.x < 6921.0f && em->pos.x > 6921.0f) || (e->pos.x > 6921.0f && em->pos.x < 6921.0f)) {
            continue;
        }
        if (!(fabsf(Muku2(ang, GetXZAngle(&em->pos, &e->pos), 3.14159274f)) > 1.04719758f)) {
            w->stepPos = e->pos;
            EmRoutineSet(em, 1, 0x13, sub, sub);
            return 1;
        }
    }
    return 0;
}

// The player is inside one of the two container tunnels (and his way ahead is not under a roof).
int em32PlInTunnelCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec p;
    Vec a;
    Vec b;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    GetPlPos(&p, 20.0f, 0);
    a = pPL->pos;
    b = p;
    a.y += 500.0f;
    b.y += 500.0f;
    PosToPos(&a, &b, &b, 0.5f);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x400000)) {
        return 0;
    }
    if (pPL->pos.x > 13125.0f && pPL->pos.x < 21684.0f && pPL->pos.z > 0.0f && pPL->pos.z < 2500.0f) {
        return 1;
    }
    if (pPL->pos.x > 18336.0f && pPL->pos.x < 21566.0f && pPL->pos.z > 0.0f && pPL->pos.z < 2703.0f) {
        return 1;
    }
    return 0;
}

// The roof point (EMI type 0x11, sub 0, state 1) whose tunnel the player is in: the tunnel attack
// starts from 2 m behind it.
int em32TunnelAtkCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Mtx m;
    Vec v;
    int i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    if (em32PlInTunnelCk(em) == 0) {
        return 0;
    }
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);
        int sub;
        int state;

        if (e->type != 0x11) {
            continue;
        }
        sub = e->sub;
        if (sub != 0) {
            continue;
        }
        state = e->state;
        if (state != 1) {
            continue;
        }
        PSMTXRotRad(m, 'y', e->rotY);
        TransMatrix(m, &e->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(m, &v, &v);
        if (!((w->plPos.x - v.x) * (w->plPos.x - v.x) + (w->plPos.z - v.z) * (w->plPos.z - v.z) > 1000000.0f)) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2000.0f;
            PSMTXMultVec(m, &v, &em->pos);
            em->ang.y = e->rotY;
            w->pPoint = (Em32Point*) e;
            EmRoutineSet(em, state, 0x16, sub, sub);
            return 1;
        }
    }
    return 0;
}

// A jump-up point (EMI type 0x12) within 1.5 m: the jump onto the roof starts.
int em32JumpUpCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    EmiData* emi = pG->pEmi;
    int i;

    if (emi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0x12) {
            continue;
        }
        if (!((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 2250000.0f)) {
            w->pPoint = (Em32Point*) e;
            EmRoutineSet(em, 1, 0x17, 0, 0);
            return 1;
        }
    }
    return 0;
}

// A jump-down point (EMI type 0x12, not sub 1) within 50 cm: the jump off the roof starts.
int em32JumpDownCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    EmiData* emi = pG->pEmi;
    int i;

    if (emi == 0) {
        return 0;
    }
    if (w->mode == 2) {
        return 0;
    }
    if (em->hp <= 1) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0x12) {
            continue;
        }
        if (e->sub == 1) {
            continue;
        }
        if (!((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 250000.0f)) {
            w->pPoint = (Em32Point*) e;
            EmRoutineSet(em, 1, 0x18, 0, 0);
            return 1;
        }
    }
    return 0;
}

// The nearest jump point (EMI type 0x12) within 5 m becomes pPoint.
void em32GetJumpDownNo(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    f32 best;
    f32 d;
    int i;

    w->pPoint = 0;
    if (pG->pEmi == 0) {
        return;
    }
    best = 25000000.0f;
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type != 0x12) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        w->pPoint = (Em32Point*) e;
        best = d;
    }
}

// A jump point within 5 m of the player whose 2 m step towards him lands within 50 cm: the ceiling
// attack starts from there.
int em32CeilingAtkCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Mtx m;
    Vec v;
    f32 ang;
    f32 d;
    int i;

    if (pG->pEmi == 0) {
        return 0;
    }
    if (w->mode != 0) {
        return 0;
    }
    if (em->hp <= 1) {
        return 0;
    }
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type != 0x12) {
            continue;
        }
        if (e->sub == 1) {
            continue;
        }
        d = (w->plPos.x - e->pos.x) * (w->plPos.x - e->pos.x) + (w->plPos.z - e->pos.z) * (w->plPos.z - e->pos.z);
        if (d > 25000000.0f) {
            continue;
        }
        ang = GetXZAngle(&e->pos, &w->plPos);
        PSMTXRotRad(m, 'y', ang);
        TransMatrix(m, &e->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(m, &v, &v);
        d = (w->plPos.x - v.x) * (w->plPos.x - v.x) + (w->plPos.z - v.z) * (w->plPos.z - v.z);
        if (!(d > 250000.0f)) {
            w->pPoint = (Em32Point*) e;
            em->pos = e->pos;
            em->ang.y = ang;
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            return 1;
        }
    }
    return 0;
}

// The player's head comes off (regions other than Japan).
void em32PlHeadLost()
{
    if (pSys->eff_country == 0) {
        PlSetDamageSe(0xD);
        return;
    }
    pPL->setHead(0);
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
}

// The head object flies away.
void em32PlHeadFall()
{
    Vec ofs;
    Vec spd;
    Vec rot;
    cObj* obj;

    if (pSys->eff_country == 0) {
        return;
    }
    pPL->getPartsPtr(3);
    spd.x = 0.0f;
    spd.y = 80.0f;
    spd.z = -50.0f;
    rot = pPL->ang;
    rot.z = 1.22173047f;
    rot.y = LIMIT_ANGLE(rot.y);
    ofs.x = 763.440002f;
    ofs.y = 6000.0f;
    ofs.z = 810.409973f;
    PSMTXMultVec(pPL->mat, &ofs, &ofs);
    spd.x = 0.0f;
    spd.y = 80.0f;
    spd.z = 0.0f;
    obj = SetObj01(PL_ARC_PTR(pG->pPlayer, 0xC), PL_ARC_PTR(pG->pPlayer, 7), &ofs, &rot, &spd, 15.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->LightInfo.EnableMask = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, 0, -1);
    }
}

// Room script hook: the enemy is placed at one of the eight scripted positions with the matching
// routine (0/1 the cage, 2/4/5/6 the appearances, 7 the second form in the open, 3 refills the hp).
void cEm32::setNext(int no)
{
    Em32Work* w = EM32_WK(this);
    Vec pos[8] = {
        { -2216.05005f, 0.0f, 1314.91003f },
        { 9393.0f, 6000.0f, 4016.0f },
        { 0.0f, 0.0f, 0.0f },
        { -20388.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 50810.0f, 4315.0f, 9845.0f },
        { -2261.05005f, 0.0f, 1041.91003f },
        { 10189.0f, 0.0f, 4379.0f },
    };
    f32 rot[8] = { 3.14159274f, 0.785398185f, 0.0f, -0.879999995f, 0.0f, 0.879999995f, 3.14159274f, 0.785398185f };

    if ((u32) no > 7) {
        return;
    }
    if (no == 3) {
        if (w->mode != 2) {
            hp = 500;
        }
        return;
    }
    setPos(&pos[no]);
    this->ang.y = rot[no];
    MotionSetCore(this, &Motion, PL_ARC_PTR(subArc, 0x11), 0, 0, 1, 0);
    MotionMove(this, 0);
    partsWorldCalc();
    pos_old = this->pos;
    w->x7C4 = no;
    invisible_factor = 1.0f;
    be_flag |= 2;
    w->pPoint = 0;
    switch ((u32) no) {
    case 0:
    default:
        w->flags &= ~0x1000;
        hp = 500;
        EmRoutineSet(this, 1, 2, 0, 0);
        break;
    case 1:
        w->flags &= ~0x1000;
        hp = 1500;
        EmRoutineSet(this, no, 3, 0, 0);
        break;
    case 2:
        // A local for the 1: stored directly, it shares a register with the hp/AtariOff work below
        // instead of getting its own.
        {
            int n = 1;
            w->mode = n;
        }
        hp = hp_max;
        atari.off();
        EmRoutineSet(this, 1, 4, 0, 0);
        break;
    case 4:
        // A local for the 1: stored directly, it shares a register with the hp/AtariOff work below
        // instead of getting its own.
        {
            int n = 1;
            w->mode = n;
        }
        hp = hp_max;
        atari.off();
        flag &= ~1;
        w->flags |= 0x100000;
        if (w->pTexModel) {
            w->flags |= 0x140000;
            w->pTexModel->resetTexBlendTbl();
        }
        EmRoutineSet(this, 1, 5, 0, 0);
        break;
    case 5:
        // A local for the 1: stored directly, it shares a register with the hp/AtariOff work below
        // instead of getting its own.
        {
            int n = 1;
            w->mode = n;
        }
        hp = hp_max;
        atari.off();
        flag &= ~1;
        w->flags |= 0x100000;
        if (w->pTexModel) {
            w->flags |= 0x140000;
            w->pTexModel->resetTexBlendTbl();
        }
        w->flags |= 0x20000;
        EmRoutineSet(this, 1, 0xD, 0, 1);
        break;
    case 6:
        hp = hp_max;
        atari.off();
        flag &= ~1;
        r_no_1 = 0xD;
        r_no_2 = 0;
        r_no_0 = 1;
        r_no_3 = 1;
        break;
    case 7:
        // A local for the 1: stored directly, it shares a register with the w->wait store below
        // instead of getting its own.
        {
            int n = 1;
            w->mode = n;
        }
        w->wait = 10;
        EM32_EFFECT_DELETE(w->espKind[1], this);
        em32TexrenderInit(this);
        EmRoutineSet(this, 1, 6, 0, 1);
        break;
    }
}

// For the level script: the container the landing of this frame breaks (0xFF = none; the jump
// routines set it from their jump point on landing, move() resets it every frame).
int cEm32::getBreakNo()
{
    return EM32_WK(this)->breakNo;
}

// For the level script: the container the jump-down of the third appearance lands on (0xFF = none).
int cEm32::getBreakNo2()
{
    return EM32_WK(this)->breakNo2;
}

// The texture render manager of the second form's blend texture.
void em32TexrenderInit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    u8* tbl = w->texBlend;

    if (w->pTexModel == 0) {
        return;
    }
    w->pTex = Ctrl12GetTexRenderEm32(w->pCtrl12);
    if (w->pTex == 0) {
        pLog->err(0, 0, "em32TexrenderInit:: Manager alloc failed!!");
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = w->pTex->m_Tex_no;
    w->pTex->m_Rep_type = 1;
    w->pTex->m_H_size = w->pTex->m_W_size = 0x40;
    EffectEspDelete(w->pTex->m_Core_flg | 0x801, w->espKind[1], em, 0);
    EffectEspgenDelete(w->pTex->m_Core_flg | 0x801, w->espKind[1], em);
    EffectEfmDelete(w->pTex->m_Core_flg | 0x801, w->espKind[1], em);
    EstSet(0, -1, 0, 0, EFF_EM32, 0, w->pTex->m_Core_flg | 0x801, w->espKind[1], em, 0);
}

// The player's position 18 frames ahead (plPos) and the angle / squared distance to it.
void em32GetPlPos(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cParts* p = pPL->getPartsPtr(0);
    Vec d;

    PSVECSubtract(&p->world, &p->world_old2, &d);
    PSVECScale(&d, &d, 18.0f);
    PSVECAdd(&pPL->pos, &d, &w->plPos);
    w->plAng = Muku(&em->pos, &w->plPos, em->ang.y, 3.14159274f);
    w->plAngAbs = fabsf(w->plAng);
    w->routeAng = fabsf(w->plAng);
    w->plDist2 = (em->pos.x - w->plPos.x) * (em->pos.x - w->plPos.x) + (em->pos.z - w->plPos.z) * (em->pos.z - w->plPos.z);
}

// Step-down target: the nearest EMI type 0x11 / sub 1 point (within 4 m of the player and in his
// sight, or within 5 m of stepTarget while flags bit12 is set), else the nearest at all.
void em32GetStepDownPos(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Vec a;
    Vec b;
    f32 best;
    f32 d;
    int found;
    int i;
    EmiEntry* e;

    w->stepPos = pPL->pos;
    if (pG->pEmi == 0) {
        return;
    }
    best = 10000000000000000.0f;
    found = 0;
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        if (w->flags & 0x1000) {
            d = (w->stepTarget.x - e->pos.x) * (w->stepTarget.x - e->pos.x) +
                (w->stepTarget.z - e->pos.z) * (w->stepTarget.z - e->pos.z);
            if (d > best) {
                continue;
            }
        } else {
            d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
            if (d < 12250000.0f) {
                continue;
            }
            if (d > best) {
                continue;
            }
            a = pPL->pos;
            b = e->pos;
            a.y += 500.0f;
            b.y += 500.0f;
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                continue;
            }
        }
        best = d;
        found = 1;
        w->stepPos = e->pos;
    }
    if (found) {
        return;
    }
    best = 10000000000000000.0f;
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        if (w->flags & 0x1000) {
            d = (w->stepTarget.x - e->pos.x) * (w->stepTarget.x - e->pos.x) +
                (w->stepTarget.z - e->pos.z) * (w->stepTarget.z - e->pos.z);
        } else {
            d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
        }
        if (d > best) {
            continue;
        }
        best = d;
        // stepPos through a byte pointer: `&w->stepPos` shrinks the loop by 6 instructions
        memcpy((u8*) w + 0x79C, &e->pos, sizeof(Vec));
    }
}

// Damage of the pending hit: the weapon table value (near = within 4 m for the range falloff), 20
// for weapon ids past 0x2D.
int em32SetDmVal(cEm32* em)
{
    int near = 0;
    int dmg;

    if (em->dmg.m_pDamageYarare->len < 16000000.0f) {
        near = 1;
    }
    dmg = 20;
    if (em->dmg.m_Wep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    return dmg;
}

// The ambush swipe from the cage (not on the easy difficulties): the player is behind the cage bars
// on the side the enemy faces, within 1.5 m of the swipe point, and both lines are clear.
int em32AmbushAtkCk(cEm32* em)
{
    Vec b;
    Vec a;
    Vec c;
    Vec d;
    f32 ang;

    if (pG->Game_level <= 1) {
        return 0;
    }
    ang = Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f);
    a = em->pos;
    a.y += 500.0f;
    b = pPL->pos;
    b.y += 500.0f;
    if (ang > 0.0f) {
        c.x = 0.0f;
        c.y = 500.0f;
        c.z = 3217.64990f;
        d.x = 4000.0f;
        d.y = 500.0f;
        d.z = 3217.64990f;
        PSMTXMultVec(em->mat, &c, &c);
        PSMTXMultVec(em->mat, &d, &d);
        if ((b.x - d.x) * (b.x - d.x) + (b.z - d.z) * (b.z - d.z) > 1000000.0f) {
            return 0;
        }
        if (SatMgr.hitCheck(&a, &c, 0, 0, 0, 0)) {
            return 0;
        }
        if (SatMgr.hitCheck(&c, &b, 0, 0, 0, 0)) {
            return 0;
        }
        EmRoutineSet(em, 1, 0xE, 0, 0);
        return 1;
    }
    c.x = 0.0f;
    c.y = 500.0f;
    c.z = 3217.64990f;
    d.x = -4000.0f;
    d.y = 500.0f;
    d.z = 3217.64990f;
    PSMTXMultVec(em->mat, &c, &c);
    PSMTXMultVec(em->mat, &d, &d);
    if ((b.x - d.x) * (b.x - d.x) + (b.z - d.z) * (b.z - d.z) > 1000000.0f) {
        return 0;
    }
    if (SatMgr.hitCheck(&a, &c, 0, 0, 0, 0)) {
        return 0;
    }
    if (SatMgr.hitCheck(&c, &b, 0, 0, 0, 0)) {
        return 0;
    }
    EmRoutineSet(em, 1, 0xE, 0, 1);
    return 1;
}

// Blood effect of the pending hit at the hit part, sized by weapon class (handgun small, shotgun
// by range, heavy weapons large); knife and weapon 0 give none.
void em32BloodSet(cEm32* em)
{
    int near = 0;

    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    switch (em->dmg.m_Wep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x2A, 2, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x2A, 3, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2A, 1, 0, 0, 0);
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
        EmDmBloodSet2(em, 0x2A, 1, 0, 0, 0);
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
        EmDmBloodSet2(em, 0x2A, 3, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    case 0x16:
    case 0x2A:
    default:
        break;
    }
}

// The two halves of the cut player: the body models by costume (pG->costume), the parts models
// of the player archive on top, hidden until em32PlDivideSet.
#define EM32_DIVIDE_MODELS(w, bin1, bin2)                                                              \
    obj = SetObj00(ARC(bin1), PL_ARC_PTR(pG->pPlayer, 5), &pPL->pos, &pPL->ang);                      \
    (w)->pDivide[0] = obj;                                                                             \
    if (obj) {                                                                                         \
        info = ModInfoMgr.create(ARC(0x96), PL_ARC_PTR(pG->pPlayer, 7));                                \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x97), PL_ARC_PTR(pG->pPlayer, 7));                                \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x98), PL_ARC_PTR(pG->pPlayer, 7));                                \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x99), PL_ARC_PTR(pG->pPlayer, 0x11));                             \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        info = ModInfoMgr.create(ARC(0x9A), PL_ARC_PTR(pG->pPlayer, 0x11));                             \
        if (info) {                                                                                    \
            (w)->pDivide[0]->addModel(info);                                                           \
        }                                                                                              \
        (w)->pDivide[0]->be_flag &= ~2;                                                                \
        (w)->pDivide[0]->LightInfo.EnableMask = 1;                                                            \
    }                                                                                                  \
    obj = SetObj00(ARC(bin2), PL_ARC_PTR(pG->pPlayer, 5), &pPL->pos, &pPL->ang);                      \
    (w)->pDivide[1] = obj;                                                                             \
    if (obj) {                                                                                         \
        obj->be_flag &= ~2;                                                                            \
        (w)->pDivide[1]->LightInfo.EnableMask = 1;                                                            \
    }

// Creates the two hidden obj00 halves of the cut player (pDivide) for the player's costume
// (pl_costume 0 / 1 the default, 2 and 3 the alternates), used by em32PlDivideSet / Set2.
void em32PlDivideModelInit(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    cObj* obj;
    cModelInfo* info;

    switch (pG->pl_costume) {
    case 0:
    case 1:
    default:
        EM32_DIVIDE_MODELS(w, 0x95, 0x9B);
        break;
    case 2:
        EM32_DIVIDE_MODELS(w, 0xAE, 0xAF);
        break;
    case 3:
        EM32_DIVIDE_MODELS(w, 0xB0, 0xB1);
        break;
    }
}

// The player is cut in two by the tail: the halves take his place and fall.
void em32PlDivideSet(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    SetPlDamage(em, plemDivide);
    pG->pl_life = 0;
    pPL->be_flag &= ~2;
    pPL->ang.y = GetXZAngle(&em->pos, &pPL->pos);
    if (w->pDivide[0]) {
        ((cObj00*) w->pDivide[0])->setScrAtari(300.0f);
        w->pDivide[0]->be_flag |= 2;
        w->pDivide[0]->pos = pPL->pos;
        w->pDivide[0]->ang = pPL->ang;
        MotSetObj00(w->pDivide[0], ARC(0x9C), 1, 0);
        EstSet(w->pDivide[0], -1, 0, 0, EFF_EM32, 0x10, 0, ESP_CORE_KIND_NONE, w->pDivide[0], 0);
    }
    if (w->pDivide[1]) {
        ((cObj00*) w->pDivide[1])->setScrAtari(300.0f);
        w->pDivide[1]->be_flag |= 2;
        w->pDivide[1]->pos = pPL->pos;
        w->pDivide[1]->ang = pPL->ang;
        MotSetObj00(w->pDivide[1], ARC(0x9D), 1, 0);
        EstSet(w->pDivide[0], -1, 0, 0, EFF_EM32, 0x11, 0, ESP_CORE_KIND_NONE, w->pDivide[0], 0);
    }
    VibSetData(VIB_TBL, 0xB, 1);
}

// The caught player is cut in two: the halves drop from the claws.
void em32PlDivideSet2(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (w->pDivide[0]) {
        w->pDivide[0]->be_flag |= 2;
        w->pDivide[0]->pos = pPL->pos;
        w->pDivide[0]->ang = pPL->ang;
        MotSetObj00(w->pDivide[0], ARC(0xA1), 1, 0);
    }
    if (w->pDivide[1]) {
        w->pDivide[1]->be_flag |= 2;
        w->pDivide[1]->pos = pPL->pos;
        w->pDivide[1]->ang = pPL->ang;
        MotSetObj00(w->pDivide[1], ARC(0xA2), 1, 0);
        EstSet(w->pDivide[1], -1, 0, 0, EFF_EM32, 0xF, 0, w->espKind[1], em, 0);
    }
    VibSetData(VIB_TBL, 0xB, 1);
}

// Player damage callback of em32PlDivideSet: the (hidden) player plays the cut-in-two death
// motion with the collision off while the two obj00 halves show the death.
static void plemDivide(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, EM_ARC(pl, 0x9E), 0, 3, 1, 0);
        pl->atari.off();
        pl->r_no_2++;
    case 1:
        MotionMove(pl, 0);
        break;
    }
    pl->subArc = pl->subArc2;
}

// The racks (enemy id 0x45) within 3 m of the point 2 m ahead break.
void em32RackBreakCk(cEm32* em)
{
    Vec v;
    u32 i;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 2000.0f;
    PSMTXMultVec(em->mat, &v, &v);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((v.x - e->pos.x) * (v.x - e->pos.x) + (v.y - e->pos.y) * (v.y - e->pos.y) +
                (v.z - e->pos.z) * (v.z - e->pos.z) >
            9000000.0f) {
            continue;
        }
        ((cEmRack*) e)->setBreak(&em->pos);
    }
}

// The hit boxes take (on) / ignore (off) the damage marks; the tail tip boxes always report.
void em32SetYarareMark(cEm32* em, int on)
{
    Em32Work* w = EM32_WK(em);
    int i;

    if (on) {
        w->hit[27].flag |= 1;
        em->hitInfo.flag &= ~0x40;
        for (i = 0; i < 30; i++) {
            w->hit[i].flag &= ~0x40;
        }
    } else {
        w->hit[27].flag |= 1;
        em->hitInfo.flag |= 0x40;
        for (i = 0; i < 30; i++) {
            w->hit[i].flag |= 0x40;
        }
    }
}

// For the level script: 1 once the death motion has finished and the item dropped (flags 0x2000).
int cEm32::ckDie()
{
    if (EM32_WK(this)->flags & 0x2000) {
        return 1;
    }
    return 0;
}

// The barred doors (enemy id 0x4E) in the box ahead break towards the enemy.
void em32BreakBarred(cEm32* em)
{
    Mtx inv;
    Vec lp;
    u32 i;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x4E) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        PSMTXMultVec(inv, &e->pos, &lp);
        if (lp.x > -2000.0f && lp.x < 2000.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > 0.0f && lp.z < 1500.0f) {
            ((cEmBarred*) e)->setBreak(&em->pos);
            e->ang.y = em->ang.y;
            SndCall(6, 0xD, &e->pos, 0, 0, 0);
        }
    }
}

// The second motion work (the body) runs with the main one.
void em32BodyMove(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (w->pMot && !(w->flags & 0x4000)) {
        MotionMoveCore(em, (MotionWork*) w->pMot, 0);
        MotionSequenceCtrl((MotionWork*) w->pMot);
    }
}

// The idle breath voice every 60 frames (voiceTimer; the routines that voice the enemy reset it
// to 2 and SndStop the handle sndId).
void em32BreathSe(cEm32* em)
{
    Em32Work* w = EM32_WK(em);

    if (w->voiceTimer) {
        w->voiceTimer--;
        return;
    }
    w->voiceTimer = 59;
    w->sndId = SndCall(8, 0x33, &em->pos, em->id, 0, em);
}

// Stops the breath voice on the motion sounds that voice the enemy.
void em32BreathSeStopCk(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    u32 no;

    if (em->Motion.Seq_old.Se == 0) {
        return;
    }
    no = em->Motion.Seq_old.Se - 1;
    switch (no) {
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x10:
    case 0x11:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x20:
    case 0x23:
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
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        break;
    }
}

// The dying body shrinks: every parts matrix is scaled by `scale` (x / z) around its world position.
void em32ScaleCompress(cEm32* em)
{
    Em32Work* w = EM32_WK(em);
    Mtx m;
    Vec scale;
    cParts* p;

    if (!(w->flags & 0x20)) {
        return;
    }
    PSMTXIdentity(m);
    scale.x = 1.0f;
    scale.y = w->scale;
    scale.z = 1.0f;
    ScaleMatrix(m, &scale);
    for (p = em->pList; p; p = p->pList) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->world.x;
        p->mat[1][3] = p->world.y;
        p->mat[2][3] = p->world.z;
    }
}

// The nearest ground attack point (EMI type 7) within 1e16: the enemy stands there facing its
// rotY (turned half round when it points away).
void em32GetGroundPos(cEm32* em)
{
    Vec pos;
    f32 best;
    f32 d;
    f32 ry;
    int i;

    if (pG->pEmi == 0) {
        return;
    }
    pos = em->pos;
    best = 10000000000000000.0f;
    ry = em->ang.y;
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type != 7) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (d > best) {
            continue;
        }
        best = d;
        pos = e->pos;
        ry = e->rotY;
    }
    if (fabsf(Muku2(em->ang.y, ry, 3.14159274f)) > 1.57079637f) {
        ry += 3.14159274f;
    }
    em->ang.y = LIMIT_ANGLE(ry);
    em->pos = pos;
}

// Something (the scenario or an object) lies between the enemy and the player at knee height.
int em32BetweenHitCk(cEm32* em)
{
    Vec a;
    Vec b;

    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 1;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 1;
    }
    return 0;
}

