// em2d module (D:/Bio4/Prog/em2d.cpp): the camouflaged insect enemy. It walks the floor, the walls
// (W_ routines) and the ceiling (C_ routines), the type 4 variant flies (A_ routines); it jumps at
// the player (JumpAtk / JumpKick, the player shakes it off with the button mash), spits poison and
// bites the head off on a critical attack. em2dCamouflageMove blends the model in and out.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em2d.h"
#include "em10.h"
#include "emhit.h"
#include "emdoor.h"
#include "obj01.h"
#include "em_set.h"
#include "em_sub.h"
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
#include "dbmodule.h"
#include "db_log.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "item.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em2d,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");

static void em2d_R0_Init(cEm2d* em);
static void em2d_R0_Move(cEm2d* em);
static void em2d_R1_br_Dummy(cEm2d* em);
static void em2d_R1_R213NestWait(cEm2d* em);
static void em2d_R1_Wait(cEm2d* em);
static void em2d_R1_Walk(cEm2d* em);
static void em2d_R1_Turn180(cEm2d* em);
static void em2d_R1_SideStep(cEm2d* em);
static void em2d_R1_BackJump(cEm2d* em);
static void em2d_R1_Atk(cEm2d* em);
static void em2d_R1_AtkPoison(cEm2d* em);
static void em2d_R1_CriticalAtk(cEm2d* em);
static void plem2d_CriticalHit(cPlayer* pl);
static void em2d_R1_JumpSign(cEm2d* em);
static void em2d_R1_br_JumpAtk(cEm2d* em);
static void em2d_R1_JumpAtk(cEm2d* em);
static void em2d_R1_JumpAtkHit(cEm2d* em);
static void plem2d_JumpAtkHit(cPlayer* pl);
static void em2d_R1_JumpKickHit(cEm2d* em);
static void plem2d_JumpKickHit(cPlayer* pl);
static void em2d_R1_JumpAtkCounter(cEm2d* em);
static void em2dKickAction(cEm2d* em);
static void plem2dKick(cPlayer* pl);
static void em2d_R1_WakeupWait(cEm2d* em);
static void em2d_R1_Wakeup(cEm2d* em);
static void em2d_R1_DownJump(cEm2d* em);
static void em2d_R1_ToCeiling(cEm2d* em);
static void em2d_R1_JumpDown(cEm2d* em);
static void em2d_R1_WallOver(cEm2d* em);
static void em2d_R1_W_Wait(cEm2d* em);
static void em2d_R1_W_Walk(cEm2d* em);
static void em2d_R1_W_WalkTest(cEm2d* em);
static void em2d_R1_W_Atk(cEm2d* em);
static void em2d_R1_W_AtkPoison(cEm2d* em);
static void em2d_R1_W_Turn180(cEm2d* em);
static void em2d_R1_W_Fall(cEm2d* em);
static void em2d_R1_ToAir(cEm2d* em);
static void em2d_R1_ToGround(cEm2d* em);
static void em2d_R1_A_Wait(cEm2d* em);
static void em2d_R1_A_Walk(cEm2d* em);
static void em2d_R1_A_Back(cEm2d* em);
static void em2d_R1_A_Step(cEm2d* em);
static void em2d_R1_A_Up(cEm2d* em);
static void em2d_R1_A_Down(cEm2d* em);
static void em2d_R1_A_Turn180(cEm2d* em);
static void em2d_R1_A_Atk(cEm2d* em);
static void em2d_R1_br_A_Catch(cEm2d* em);
static void em2d_R1_A_Catch(cEm2d* em);
static void em2d_R1_A_CatchKick(cEm2d* em);
static void em2d_R1_A_CatchHit(cEm2d* em);
static void plem2d_A_CatchHit(cPlayer* pl);
static void em2d_R1_C_Wait(cEm2d* em);
static void em2d_R1_C_Fall(cEm2d* em);
static void em2d_R0_Damage(cEm2d* em);
static void em2d_R1_Dm_Normal(cEm2d* em);
static void em2d_R1_Dm_Blow(cEm2d* em);
static void em2d_R1_Dm_Down(cEm2d* em);
static void em2d_R1_Dm_Jump(cEm2d* em);
static void em2d_R1_Dm_Wall(cEm2d* em);
static void em2d_R1_Dm_Air(cEm2d* em);
static void em2d_R1_Dm_Ceiling(cEm2d* em);
static void em2d_R0_Die(cEm2d* em);
static void em2d_R1_Die_Lost(cEm2d* em);
static void em2d_R1_Die_Normal(cEm2d* em);
static void em2d_R1_Die_Down(cEm2d* em);
static void em2d_R1_Die_Wall(cEm2d* em);
static void em2d_R1_Die_Air(cEm2d* em);
static void em2d_R1_Die_Ceiling(cEm2d* em);

Em2dFunc Em2d_R0_move_tbl[4] = {
    em2d_R0_Init,
    em2d_R0_Move,
    em2d_R0_Damage,
    em2d_R0_Die,
};

// Routine 1 table: {branch check, routine} per xFD.
static Em2dFunc Em2d_R1_move_tbl[84] = {
    em2d_R1_br_Dummy, em2d_R1_R213NestWait,   // 0x00
    em2d_R1_br_Dummy, em2d_R1_Wait,           // 0x01
    em2d_R1_br_Dummy, em2d_R1_Walk,           // 0x02
    em2d_R1_br_Dummy, em2d_R1_Turn180,        // 0x03
    em2d_R1_br_Dummy, em2d_R1_SideStep,       // 0x04
    em2d_R1_br_Dummy, em2d_R1_BackJump,       // 0x05
    em2d_R1_br_Dummy, em2d_R1_Atk,            // 0x06
    em2d_R1_br_Dummy, em2d_R1_AtkPoison,      // 0x07
    em2d_R1_br_Dummy, em2d_R1_CriticalAtk,    // 0x08
    em2d_R1_br_Dummy, em2d_R1_JumpSign,       // 0x09
    em2d_R1_br_JumpAtk, em2d_R1_JumpAtk,      // 0x0A
    em2d_R1_br_Dummy, em2d_R1_JumpAtkHit,     // 0x0B
    em2d_R1_br_Dummy, em2d_R1_JumpKickHit,    // 0x0C
    em2d_R1_br_Dummy, em2d_R1_JumpAtkCounter, // 0x0D
    em2d_R1_br_Dummy, em2d_R1_WakeupWait,     // 0x0E
    em2d_R1_br_Dummy, em2d_R1_Wakeup,         // 0x0F
    em2d_R1_br_Dummy, em2d_R1_DownJump,       // 0x10
    em2d_R1_br_Dummy, em2d_R1_ToCeiling,      // 0x11
    em2d_R1_br_Dummy, em2d_R1_JumpDown,       // 0x12
    em2d_R1_br_Dummy, em2d_R1_WallOver,       // 0x13
    em2d_R1_br_Dummy, em2d_R1_W_Wait,         // 0x14
    em2d_R1_br_Dummy, em2d_R1_W_Walk,         // 0x15
    em2d_R1_br_Dummy, em2d_R1_W_Atk,          // 0x16
    em2d_R1_br_Dummy, em2d_R1_W_AtkPoison,    // 0x17
    em2d_R1_br_Dummy, em2d_R1_W_Turn180,      // 0x18
    em2d_R1_br_Dummy, em2d_R1_W_Fall,         // 0x19
    em2d_R1_br_Dummy, em2d_R1_ToAir,          // 0x1A
    em2d_R1_br_Dummy, em2d_R1_ToGround,       // 0x1B
    em2d_R1_br_Dummy, em2d_R1_A_Wait,         // 0x1C
    em2d_R1_br_Dummy, em2d_R1_A_Walk,         // 0x1D
    em2d_R1_br_Dummy, em2d_R1_A_Back,         // 0x1E
    em2d_R1_br_Dummy, em2d_R1_A_Step,         // 0x1F
    em2d_R1_br_Dummy, em2d_R1_A_Up,           // 0x20
    em2d_R1_br_Dummy, em2d_R1_A_Down,         // 0x21
    em2d_R1_br_Dummy, em2d_R1_A_Turn180,      // 0x22
    em2d_R1_br_Dummy, em2d_R1_A_Atk,          // 0x23
    em2d_R1_br_A_Catch, em2d_R1_A_Catch,      // 0x24
    em2d_R1_br_Dummy, em2d_R1_A_CatchKick,    // 0x25
    em2d_R1_br_Dummy, em2d_R1_A_CatchHit,     // 0x26
    em2d_R1_br_Dummy, em2d_R1_C_Wait,         // 0x27
    em2d_R1_br_Dummy, em2d_R1_C_Fall,         // 0x28
    em2d_R1_br_Dummy, em2d_R1_W_WalkTest,     // 0x29
};

static Em2dFunc Em2d_R1_dm_tbl[7] = {
    em2d_R1_Dm_Normal,
    em2d_R1_Dm_Blow,
    em2d_R1_Dm_Down,
    em2d_R1_Dm_Jump,
    em2d_R1_Dm_Wall,
    em2d_R1_Dm_Air,
    em2d_R1_Dm_Ceiling,
};

static Em2dFunc Em2d_R1_die_tbl[6] = {
    em2d_R1_Die_Lost,
    em2d_R1_Die_Normal,
    em2d_R1_Die_Down,
    em2d_R1_Die_Wall,
    em2d_R1_Die_Air,
    em2d_R1_Die_Ceiling,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em2d_xflip_tbl[120] = {
    0,   1,   2,   3,   4,   5,   10,  11,  12,  13,  6,   7,   8,   9,   14,  19,  20,  21,  22,  15,
    16,  17,  18,  24,  23,  26,  25,  28,  27,  29,  30,  32,  31,  35,  36,  33,  34,  43,  44,  45,
    46,  47,  48,  37,  38,  39,  40,  41,  42,  52,  53,  54,  49,  50,  51,  56,  55,  57,  58,  59,
    60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  79,  80,  77,
    78,  82,  81,  84,  83,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
};

// Attack parameters per em2dAtkCk kind: 0 bite, 1 wall bite, 2 critical (head).
static EmAtkInfo em2d_atk_info[3] = {
    { 500.0f, 8, 500, 0, 10, 0 },
    { 500.0f, 8, 500, 0, 10, 0 },
    { 500.0f, 8, 9999, 0, 10, 0 },
};

// Poison projectile (SetObj08) attack parameters.
static EmAtkInfo em2d_poison_atk[1] = {
    { 500.0f, 8, 500, 0, 10, 0 },
};

// TexRender flag written to cModel::x137 every frame (em2dCamouflageMove).
static u8 em2d_tex_flag = 0xF;

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// Struct-member views of the player pointer / pG: a load through them is not hoisted above the
// preceding stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)


// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->r_no_0 = r0;
    em->r_no_1 = r1;
    em->r_no_2 = r2;
    em->r_no_3 = r3;
}

// Scalar reference stores: pG / the player pointer are reloaded after them (st_room.h).
static inline void IntSet(int& d, int v) { d = v; }
static inline void U8Set(u8& d, u8 v) { d = v; }

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em2dDeadCk(cEm* em)
{
    return em->dmg.m_Flag || em->dmg.m_Timer;
}

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->m_flag |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->m_flag &= mask; }

// Attack wait by difficulty: pG is reloaded after every store (reference stores).
// A macro: an inline copies its constant arguments into pseudos at the call point (the parms
// are not TREE_READONLY), and a 0 argument then becomes cse's zero for the following
// EmRoutineSet stores; the target materialises `li 0` at the store and a fresh one for the routine.
#define em2dSetAtkWait(w, a, b, c, d, e)   \
    {                                      \
        IntSet((w)->atkWait, a);           \
        if (pG->Game_level > 1) {               \
            IntSet((w)->atkWait, b);       \
        }                                  \
        if (pG->Game_level > 3) {               \
            IntSet((w)->atkWait, c);       \
        }                                  \
        if (pG->Game_level > 6) {               \
            IntSet((w)->atkWait, d);       \
        }                                  \
        if (pG->Game_level == 10) {             \
            IntSet((w)->atkWait, e);       \
        }                                  \
    }

// The same with the damage counter reset between the first store and the difficulty chain.
static inline void em2dSetAtkWaitR(Em2dWork* w, int a, int b, int c, int d, int e)
{
    IntSet(w->atkWait, a);
    w->dmgTotal = 0;
    if (pG->Game_level > 1) {
        IntSet(w->atkWait, b);
    }
    if (pG->Game_level > 3) {
        IntSet(w->atkWait, c);
    }
    if (pG->Game_level > 6) {
        IntSet(w->atkWait, d);
    }
    if (pG->Game_level == 10) {
        IntSet(w->atkWait, e);
    }
}

// Per-frame movement with gravity: the position follows `spd`, which falls 20 per frame, and the
// enemy lands on the floor under its old position.
// A macro, not an inline: integrate.c drops the RTX_UNCHANGING_P flag of an inlined body's
// constant-pool loads, which then depend on the `spd.y` store and sink below the getFloor
// argument moves (the em2c EM2C_DM_FALL note).
#define em2dGravityMove(em, w)                                                    \
    {                                                                             \
        f32 fl_;                                                                  \
                                                                                  \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                              \
        (w)->spd.y -= 20.0f;                                                      \
        fl_ = SatMgr.getFloor(&(em)->pos_old, 600.0f, 100000.0f, 0, 0);            \
        if ((em)->pos.y < fl_) {                                                  \
            (em)->pos.y = fl_;                                                    \
            (w)->spd.y = 0.0f;                                                    \
        }                                                                         \
    }

// Turn towards `target` by at most `step` per frame.
static inline void em2dTurnTo(cEm2d* em, Vec* target, f32 step)
{
    em->ang.y += Muku(&em->pos, target, em->ang.y, step);
    em->ang.y = LIMIT_ANGLE(em->ang.y);
}

// Module entry (SN loader): registers Em2dInit as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em2d prolog Ok\n");
    EmInitFunc = Em2dInit;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm2d class in the manager's work.
void Em2dInit(cEm* em)
{
    new (em) cEm2d();
}

// Per-frame damage check (cEm2d::move). An explosion / fire volume takes 500 every 120 frames
// (dmGuard) and, dead or alive, sends the insect to the reaction of where it is (on a wall Dm_Wall /
// Die_Wall, ceiling Dm_Ceiling / Die_Ceiling, flying Dm_Air / Die_Air, jumping Dm_Jump, airborne
// Dm_Down / Die_Down, else Dm_Normal / Die_Normal). A weapon hit rings the bell alarm, takes
// em2dSetDmVal off hp with the camouflage-breaking flag 0x200 and the blood / poison-sac effects by
// hit part, then the same by-place dispatch: a kill dies in place, a survivor flinches (heavy
// weapons and near shotgun hits blow it over: Dm_Blow).
void em2dDmCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Camera* cam = &pG->Cam;
    YARARE_INFO* part;
    cModel* p;
    Vec pos;
    Vec dir;
    Mtx inv;
    cEsp* esp;
    cEsp* esp2;
    int near;
    int dmg;
    f32 dist;
    u32 i;

    if (em2dCrashCk(em)) {
        return;
    }
    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->dmGuard == 0) {
                w->dmGuard = 120;
                LifeDownSet2(em, 500, 0, 0);
                if (em->hp <= 0) {
                    EmSetDie(em);
                    EmReserveDropItem(em);
                    if (w->flags & 0x20) {
                        EmRoutineSet(em, 3, 3, 0, 0);
                        return;
                    }
                    if (w->flags & 0x80) {
                        EmRoutineSet(em, 3, 5, 0, 0);
                    } else if (w->flags & 0x800) {
                        EmRoutineSet(em, 3, 4, 0, 0);
                    } else if (w->flags & 0x1010) {
                        EmRoutineSet(em, 2, 3, 0, 0);
                    } else if (w->flags & 0x400) {
                        EmRoutineSet(em, 3, 2, 0, 0);
                    } else {
                        EmRoutineSet(em, 3, 1, 0, 0);
                    }
                    return;
                }
                if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
                    if (w->dmgTotal <= 199) {
                        return;
                    }
                    w->dmgTotal = 0;
                    EmRoutineSet(em, 2, 4, 0, 0);
                    return;
                }
                if (w->flags & 0x80) {
                    EmRoutineSet(em, 2, 6, 0, 0);
                    return;
                }
                if (w->flags & 0x800) {
                    EmRoutineSet(em, 2, 5, 0, 0);
                    return;
                }
                if (w->flags & 0x1010) {
                    EmRoutineSet(em, 2, 3, 0, 0);
                    return;
                }
                if (w->flags & 0x400) {
                    EmRoutineSet(em, 2, 2, 0, 0);
                    return;
                }
                if (w->flags & 8) {
                    return;
                }
                EmRoutineSet(em, 2, 0, 0, 0);
                return;
            }
            break;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    StaFlagOn(pG, STA_SE_BURST);
    pGS->bell_pos = em->pos;
    pGS->bell_stat = 0;
    em->dmg.m_Timer = 1;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    w->x50C = 0;
    w->flags |= 0x200;
    near = 0;
    part = em->dmg.m_pDamageYarare;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    dmg = em2dSetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    w->dmgTotal += dmg;
    p = em->getPartsPtr(0);
    dist = (cam->param.pos.x - p->world.x) * (cam->param.pos.x - p->world.x) +
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
        if (ChkWaterEffectEnable(&em->pos)) {
            EmDmBloodSet2(em, 0x25, 0x18, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x25, 0x28, 0, 0, 0);
        }
        if (EmGetDmPos(em, &pos, &dir)) {
            EspSeqData* seq = EspGetEstAddr(0x25, 0x24, 1);
            if (seq) {
                for (i = 0; i < seq->num; i++) {
                    if (EspEstSetSelect(0x25, 0x24, i, &esp, 0)) {
                        esp->m_Pos = pos;
                        esp->parent = em->getPartsPtr(part->partsNo - 1);
                        PSMTXInverse(((cModel*) esp->parent)->mat, inv);
                        PSMTXMultVec(inv, &esp->m_Pos, &esp->m_Pos);
                        esp->m_Parts_no = part->partsNo - 1;
                    }
                }
            }
        }
        break;
    case 0x10:
        if (ChkWaterEffectEnable(&em->pos)) {
            EmDmBloodSet2(em, 0x25, 0x23, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x25, 0x2B, 0, 0, 0);
        }
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (ChkWaterEffectEnable(&em->pos)) {
            EmDmBloodSet2(em, 0x25, 0x1E, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x25, 0x22, 0, 0, 0);
        }
        if (EmGetDmPos(em, &pos, &dir)) {
            EspSeqData* seq = EspGetEstAddr(0x25, 0x25, 1);
            if (seq) {
                for (i = 0; i < seq->num; i++) {
                    if (EspEstSetSelect(0x25, 0x25, i, &esp2, 0)) {
                        esp2->m_Pos = pos;
                        esp2->parent = em->getPartsPtr(part->partsNo - 1);
                        PSMTXInverse(((cModel*) esp2->parent)->mat, inv);
                        PSMTXMultVec(inv, &esp2->m_Pos, &esp2->m_Pos);
                        esp2->m_Parts_no = part->partsNo - 1;
                    }
                }
            }
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (ChkWaterEffectEnable(&em->pos)) {
            if (near) {
                if (dist < 16000000.0f) {
                    EmDmBloodSet2(em, 0x25, 0x19, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x25, 0x19, 0, 0, 0);
                }
            } else {
                EmDmBloodSet2(em, 0x25, 0x18, 0, 0, 0);
            }
        } else {
            if (near) {
                if (dist < 16000000.0f) {
                    EmDmBloodSet2(em, 0x25, 0x29, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x25, 0x29, 0, 0, 0);
                }
            } else {
                EmDmBloodSet2(em, 0x25, 0x28, 0, 0, 0);
            }
        }
        break;
    case 0x17:
    case 0x2A:
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
        if (ChkWaterEffectEnable(&em->pos)) {
            if (dist < 16000000.0f) {
                EmDmBloodSet2(em, 0x25, 0x19, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x25, 0x19, 0, 0, 0);
            }
        } else {
            if (dist < 16000000.0f) {
                EmDmBloodSet2(em, 0x25, 0x29, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x25, 0x29, 0, 0, 0);
            }
        }
        break;
    }
    if (em->dmg.m_Wep == 0x17 || em->dmg.m_Wep == 0x2A) {
        w->poisonTimer = Rnd() % 450 + 450;
    }
    SndCall(8, 6, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        if (w->flags & 0x20) {
            EmRoutineSet(em, 3, 3, 0, 0);
            return;
        }
        if (w->flags & 0x80) {
            EmRoutineSet(em, 3, 5, 0, 0);
            return;
        }
        if (w->flags & 0x800) {
            EmRoutineSet(em, 3, 4, 0, 0);
            return;
        }
        if (w->flags & 0x1010) {
            EmRoutineSet(em, 2, 3, 0, 0);
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
        case 0xE:
        case 0x10:
        case 0x11:
        case 0x16:
        case 0x17:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2A:
        case 0x2B:
        default:
            if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 2, 0, 0);
                return;
            }
            EmRoutineSet(em, 3, 1, 0, 0);
            return;
        case 7:
        case 8:
        case 0x21:
            if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 2, 0, 0);
                return;
            }
            if (near == 0) {
                EmRoutineSet(em, 3, 1, 0, 0);
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xD:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x18:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
            if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 2, 0, 0);
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        }
    }
    if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
        if (w->dmgTotal <= 199) {
            return;
        }
        w->dmgTotal = 0;
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
    if (w->flags & 0x80) {
        EmRoutineSet(em, 2, 6, 0, 0);
        return;
    }
    if (w->flags & 0x800) {
        EmRoutineSet(em, 2, 5, 0, 0);
        return;
    }
    if (w->flags & 0x1010) {
        EmRoutineSet(em, 2, 3, 0, 0);
        return;
    }
    if (w->flags & 0x400) {
        switch (em->dmg.m_Wep) {
        default:
            if (Rnd() & 3) {
                return;
            }
            EmRoutineSet(em, 2, 2, 0, 0);
            return;
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                return;
            }
            EmRoutineSet(em, 2, 2, 0, 0);
            return;
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x2D:
            EmRoutineSet(em, 2, 2, 0, 0);
            return;
        }
    }
    if (w->flags & 8) {
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
        if (w->dmgTotal <= 199) {
            return;
        }
        w->dmgTotal = 0;
        if (Rnd() & 3) {
            EmRoutineSet(em, 2, 0, 0, 0);
        } else {
            EmRoutineSet(em, 2, 1, 0, 0);
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (near == 0) {
            if (w->dmgTotal <= 199) {
                return;
            }
        }
        w->dmgTotal = 0;
        if (Rnd() & 3) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 0xE:
        EmRoutineSet(em, 2, 0, 0, 0);
        return;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        w->dmgTotal = 0;
        EmRoutineSet(em, 2, 1, 0, 0);
        return;
    case 0x17:
    case 0x2A:
        EmRoutineSet(em, 2, 0, 0, 0);
        return;
    }
}

// Per-frame update: damage check, clears the per-frame flags, the water / near-floor tests (flags
// 0x2000 / 0x80000 / 0x200000), route check, the R0 table, then the collision size and scenario check
// by mode (checkAir on walls / ceilings / in the air with the wall attribute mask), the camouflage
// blend (em2dCamouflageMove, blendRatio), the eye glow and the hum SE.
void cEm2d::move()
{
    Em2dWork* w = EM2D_WK(this);
    cAtariInfo* at;
    f32 waterH;
    f32 len;
    u16 atFlags;
    f32 moved;
    int n;

    motFlags2 &= ~0x40000000;
    if (r_no_0 != 0) {
        em2dDmCk(this);
    }
    clearStatus(EM_STATUS_IK_OFF);
    w->flags &= ~0x002F19FE;
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->jumpWait) {
        w->jumpWait--;
    }
    if (w->dmGuard) {
        w->dmGuard--;
    }
    if (w->poisonWait) {
        w->poisonWait--;
    }
    if (w->atkWait == 0 && em2dDeadCk(pPL) && pG->Game_level <= 9) {
        w->atkWait = 10;
    }
    if (w->atkCnt > 450) {
        w->flags |= 0x2000;
    } else {
        w->flags &= ~0x2000;
    }
    w->waterH = pos.y - 99999.0f;
    if (GetWaterHeight(&pos, &waterH) && pos.y < waterH) {
        w->waterH = waterH;
        w->flags |= 0x80000;
    }
    em2dRouteCk(this);
    Em2d_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    w->flags &= ~0x400;
    if (seFlags28B & 0x80) {
        w->flags |= 0x400;
    }
    at = &atari;
    if (w->catchGuard) {
        w->catchGuard--;
        dmg.m_Timer = 2;
        AtariOff(at, 0xFCFF);
        if (w->catchGuard == 0) {
            AtariOn(at, 0x300);
        }
    }
    partsWorldCalc();
    em2dScaleCompress(this);
    if (SatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0) < pos.y - 100.0f) {
        w->flags |= 0x200000;
    }
    len = SQRTF((pos_old.x - pos.x) * (pos_old.x - pos.x) + (pos_old.z - pos.z) * (pos_old.z - pos.z));
    atFlags = atari.m_flag;
    if (w->flags & 0x1000) {
        at->m_flag &= ~0x200;
    }
    EmAtCheck(this);
    at->move();
    if (w->flags & 0x800) {
        at->set(5, 700.0f, 550.0f);
        SatMgr.checkAir(this, 0x980800);
    } else if (w->flags & 0xE0) {
        at->set(5, 210.000015f, 550.0f);
        if (!(w->flags & 0x20)) {
            SatMgr.checkAir(this, 0x980800);
        }
    } else {
        at->set(5, 700.0f, 550.0f);
        SatMgr.check(this, 0);
    }
    atari.m_flag = atFlags;
    moved = SQRTF((pos.x - pos_old.x) * (pos.x - pos_old.x) + (pos.z - pos_old.z) * (pos.z - pos_old.z));
    if (moved < len * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em2dCamouflageMove(this);
    pModelInfo->setBlendRatio(w->blendRatio);
    em2dFootSeMove(this);
    if (hp > 0 && type != 4) {
        n = w->effTimer;
        if (n) {
            w->effTimer--;
        } else {
            w->effTimer = Rnd() % 3 + 5;
            EstSet((int) this, -1, 0, 0, 0x25, 5, 0, 0, (u32) this, (void*) n);
        }
    }
    em2dEyeMove(this);
    em2dHumSeMove(this);
}

// Start routine and work defaults from cEm::set: 0 Wait on the floor, 3 Walk already alerted, 1 a
// flying one (type 4: A_Wait 0x1C, flags 0x840), 2 hanging under the ceiling found above (flag 0x80,
// C_Wait 0x27), 4 on the wall behind it (W_Wait... or A_Wait when no wall), 5 the wall walk test
// (W_WalkTest 0x29); homePos = the start position.
void em2dInitRtnSet(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int zero = 0;

    w->flags = zero;
    w->atkWait = zero;
    w->jumpWait = zero;
    w->poisonWait = zero;
    // x4F8, spd, wallNrm x/y/z: the dying-store rule (1.0 dies at wallNrm.y, 0.0 at wallNrm.z) issues them y, z, x4F8,
    // spd, x like the target -- no keep-alive needed.
    w->Compress_y = 1.0f;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->wallNrm.x = 0.0f;
    w->wallNrm.y = 1.0f;
    w->wallNrm.z = 0.0f;
    w->humTimer = Rnd() % 90 + 90;
    w->effTimer = 5;
    w->x4D0 = zero;
    w->poisonTimer = zero;
    w->dmgTotal = zero;
    w->atkCnt = zero;
    w->x50C = zero;
    w->sndId = zero;
    w->dmGuard = zero;
    w->catchGuard = zero;
    w->x534 = zero;
    w->Reset_enable = zero;
    w->homePos = em->pos;
    if (em->type != 4) {
        EstSet((int) em, -1, 0, 0, 0x25, 4, 0, w->espKind, (u32) em, 0);
        if (em->type != 4) {
            EstSet((int) em, -1, 0, 0, 0x25, 0x10, 0, w->espKind2, (u32) em, 0);
        }
    }
    if (em->type == 4) {
        em->flag |= 0x40000000;
    }
    em->setStatus(EM_STATUS_ACTIVE);
    switch (em->set) {
    case 0:
    default:
        EmRoutineSet(em, 1, 1, 0, 1);
        MotionSetCore(em, &em->Motion, ARC(0x46), 0, 0, 5, 0);
        break;
    case 3:
        w->flags |= 0x200;
        EmRoutineSet(em, 1, 2, 0, 0);
        MotionSetCore(em, &em->Motion, ARC(0x46), 0, 0, 5, 0);
        break;
    case 1:
        w->flags |= 0x840;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        EmRoutineSet(em, 1, 0x1C, 0, 0);
        MotionSetCore(em, &em->Motion, ARC(0x5D), 0, 0, 5, 0);
        break;
    case 2: {
        Vec top;
        Vec bottom;
        Vec hit;
        f32 fy;

        w->flags |= 0x80;
        top = bottom = em->pos;  // chained: bottom stored first, top.x reloaded from bottom.x
        top.y -= 500.0f;
        bottom.y += 2000.0f;
        if (SatMgr.hitCheck(&top, &bottom, &hit, 0, 0, 0x383830)) {
            em->pos.y = hit.y;
        }
        fy = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        w->homePos = em->pos;
        w->homePos.y = fy;  // the floor y overrides the copy (stfs after the copy's stw)
        EmRoutineSet(em, 1, 0x27, 0, 0);
        MotionSetCore(em, &em->Motion, ARC(0x47), 0, 0, 5, 0);
        break;
    }
    case 4: {
        Mtx m;
        Vec a;
        Vec b;
        Vec hit;
        Vec nrm;
        int one = 1;      // routine 1 of both arms in a callee-saved register
        Vec* pos = &em->pos;
        f32 fz;

        em->set = one;
        PSMTXRotRad(m, 'y', em->ang.y);
        TransMatrix(m, pos);
        fz = 0.0f;        // one 0.0 for the line ends and the else arm's spd
        a.x = fz;
        a.y = fz;
        a.z = fz;
        b.x = fz;
        b.y = fz;
        b.z = 10000.0f;
        PSMTXMultVec(m, &a, &a);
        PSMTXMultVec(m, &b, &b);
        if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x383830)) {
            w->wallNrm = nrm;
            *pos = hit;
            EmRoutineSet(em, one, 0, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(0x46), 0, 0, 5, 0);
            em2dSetWallMatrix(em);
            MotionMoveF(em, 0);
        } else {
            w->flags |= 0x840;
            w->spd.x = fz;
            w->spd.y = fz;
            w->spd.z = fz;
            EmRoutineSet(em, one, 0x1C, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(0x5D), 0, 0, 5, 0);
        }
        break;
    }
    case 5:
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = 1.0f;
        w->wallNrm.z = 0.0f;
        EmRoutineSet(em, 1, 0x29, 0, 0);
        MotionSetCore(em, &em->Motion, ARC(0x48), 0, 0, 5, 0);
        break;
    }
}

// R0 == 0: creation. Builds the model of the type (0..4, the flying type 4 with wings), collision and
// the fifteen hit boxes, camouflage blend 0, the room's ctrl11 / ctrl12, the IK-off parts, effect data,
// and the start routine (em2dInitRtnSet).
static void em2d_R0_Init(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    void* bin;
    void* tpl;
    u32 i;
    int one;

    em->ot_type = 0;
    switch (em->type) {
    case 0:
    default:
        bin = ARC(5);
        tpl = ARC(6);
        break;
    case 1:
        bin = ARC(7);
        tpl = ARC(8);
        break;
    case 2:
        bin = ARC(9);
        tpl = ARC(0xA);
        break;
    case 3:
        bin = ARC(0xB);
        tpl = ARC(0xC);
        break;
    case 4:
        bin = ARC(0xD);
        tpl = ARC(0xE);
        break;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "em2d() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    em->pXFlip = em2d_xflip_tbl;
    {
        static const Vec ofs = {0.0f, 0.0f, 0.0f};
        static const Vec size = {2000.0f, 2000.0f, 2000.0f};
        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 700.0f, 550.0f, 550.0f, 1000.0f, 1, 0x2000, 10);
    em->litArea.on(1);
    YarareInit(em, 0.0f, -50.0f, 0.0f, 210.0f, 100.0f, 6, 1);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 260.0f, 50.0f, 2, 1);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 260.0f, 200.0f, 3, 1);
    YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 240.0f, 50.0f, 4, 1);
    YarareAdd(em, &w->hit[3], -400.0f, 0.0f, 0.0f, 160.0f, 400.0f, 8, 3);
    YarareAdd(em, &w->hit[4], -400.0f, 0.0f, 0.0f, 130.0f, 500.0f, 9, 3);
    YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 160.0f, 400.0f, 0xC, 3);
    YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 130.0f, 500.0f, 0xD, 3);
    one = 1;
    YarareAdd(em, &w->hit[7], 0.0f, -500.0f, 0.0f, 170.0f, 500.0f, 0x10, 1);
    YarareAdd(em, &w->hit[8], 0.0f, -600.0f, 0.0f, 140.0f, 600.0f, 0x11, 1);
    YarareAdd(em, &w->hit[9], 0.0f, -500.0f, 0.0f, 170.0f, 500.0f, 0x14, 1);
    YarareAdd(em, &w->hit[10], 0.0f, -600.0f, 0.0f, 140.0f, 600.0f, 0x15, 1);
    YarareAdd(em, &w->hit[11], 0.0f, 0.0f, 0.0f, 210.0f, 50.0f, 0x1E, 1);
    YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 210.0f, 50.0f, 0x1F, 1);
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->lockParts = 0;
    EspDataLoad((u32) ARC(4), 0x25, 0);
    EffEm2d_setTexRender(em);
    w->blendRatio = 0;
    em->pModelInfo->setBlendRatio(0);
    em->Refract_ratio = 0xFF;
    w->espKind = EspPullCoreKind();
    w->espKind2 = EspPullCoreKind();
    w->espKind3 = EspPullCoreKind();
    w->x535 = one;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    if (em->type == 4) {
        u8 tbl[15] = {0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
        for (i = 0; i < 15; i++) {
            ((cParts*) em->getPartsPtr(tbl[i]))->motParts.flags |= 0x20000002;
        }
    }
    w->startPos = em->pos;
    w->startRot = em->ang;
    w->startSet = em->set;
    w->startSet2 = em->set;
    em2dInitRtnSet(em);
    MotionMoveF(em, 0);
    em2d_R0_Move(em);
}

// R0 == 1: runs the branch check and the move handler of R1 (Em2d_R1_move_tbl pairs).
static void em2d_R0_Move(cEm2d* em)
{
    Em2d_R1_move_tbl[em->r_no_1 * 2](em);
    Em2d_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the routines that have none.
static void em2d_R1_br_Dummy(cEm2d* em)
{
}

// R1 == 0x00 R213NestWait: the room 213 nest insects (hp 1, flag 0x20, IK off): idle wriggles with
// random 1..3 loops, die in place (Die_Wall) when killed.
static void em2d_R1_R213NestWait(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec plPos;
    Mtx rot;

    w->flags |= 0x20;
    em->setStatus(EM_STATUS_IK_OFF);
    AtariOff(&em->atari, 0xFCFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x46), 0, 30, 5, 0);
        em->hp = 1;
        em->flag |= 0x20000000;
        w->timer = (Rnd() & 1) + 2;
        em->r_no_2++;
    case 1:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        em2dSetWallMatrix2(em, 0.4f);
        MotionMoveF(em, 0);
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x48), (int) ARC(0x4A), 10, 5, 0);
        w->timer = (Rnd() & 1) + 1;
        em->r_no_2++;
    case 3:
        PSMTXRotRad(rot, 'y', 0.0490873866f);
        PSMTXConcat(em->mat, rot, em->mat);
        em2dSetWallMatrix2(em, 0.4f);
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->r_no_2 = 0;
            }
        }
        break;
    }
    if (em->hp <= 0) {
        EmRoutineSet(em, 3, 3, 0, 0);
    }
}

// R1 == 0x01 Wait: idle on the floor (flag 0x100 = ground); when the player is found (em2dFindCk)
// turns to him (Turn180 3) or walks (2), or side-steps (4) one time in four.
static void em2d_R1_Wait(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int hit;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            MotionSetCore(em, &em->Motion, ARC(0x46), 0, 10, 5, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0xF), (int) ARC(0x10), 10, 5, 0);
        }
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dGravityMove(em, w);
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life > 0) {
            em2dFindCk(em);
            if (w->flags & 0x200) {
                if (!(w->flags & 0x8000) && w->atkWait == 0 && em2dToCeilingCk(em) == 0 && em2dToAirCk(em) == 0 &&
                    (hit = em2dWallWalkCk(em)) == 0) {
                    if (w->targetAngAbs > 2.09439516f) {
                        EmRoutineSet(em, 1, 3, hit, hit);
                    } else {
                        EmRoutineSet(em, 1, 2, hit, hit);
                    }
                }
            }
        }
        break;
    }
    if (w->flags & 0x200) {
        hit = em2dLockCk(em);
        if (hit) {
            w->lockCnt++;
            if (w->lockCnt > 5) {
                if ((Rnd() & 3) == 0) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                    return;
                }
                w->lockCnt = 0;
            }
        } else {
            w->lockCnt = hit;
        }
    }
    em2dFallCk(em);
}

// R1 == 0x02 Walk: approaches the player (walkMode 0) or keeps its distance (1 / 2) along the route;
// leaves for Turn180, SideStep, BackJump (5), the melee Atk (6), poison spit (7), the critical (8) or
// jump sign (9 -> JumpAtk), climbs a wall in the way (W_Walk 0x15 unless an EMI no-wall point
// forbids it), and at low rank may just Wait.
static void em2d_R1_Walk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int hit;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if ((Rnd() & 3) == 0 || em->r_no_3) {
            MotionSetCore(em, &em->Motion, ARC(0x14), (int) ARC(0x15), 5, 5, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x12), (int) ARC(0x13), 10, 5, 0);
        }
        w->walkMode = Rnd() & 1;
        if (w->poisonWait) {
            w->walkMode = 0;
        }
        if ((s16) pG->pl_life <= 699 && Rnd() % 3 == 0) {
            w->walkMode = 2;
        }
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dGravityMove(em, w);
        if (w->flags & 0x8000) {
            em2dTurnTo(em, &w->targetPos, 0.0981747732f);
        } else {
            em2dTurnTo(em, &w->routePos, 0.0981747732f);
        }
        MotionMoveF(em, 0);
        break;
    }
    em2dDoorOpenCk(em);
    if (em2dJumpDownCk(em)) {
        return;
    }
    hit = em2dWallOverCk(em);
    if (hit) {
        return;
    }
    if (w->flags & 0x8000) {
        if ((em->pos.x - w->homePos.x) * (em->pos.x - w->homePos.x) +
                (em->pos.z - w->homePos.z) * (em->pos.z - w->homePos.z) < 4000000.0f) {
            EmRoutineSet(em, 1, 1, hit, hit);
        }
        return;
    }
    hit = em2dLockCk(em);
    if (hit) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            if ((Rnd() & 3) == 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
                return;
            }
            w->lockCnt = 0;
        }
    } else {
        w->lockCnt = hit;
    }
    if (w->atkWait) {
        if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.785398185f) {
            EmRoutineSet(em, 1, 5, 0, 0);
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        return;
    }
    {
        f32 dy = fabsf(em->pos.y - pPL->pos.y);
        if ((w->flags & 1) && dy < 500.0f) {
            if (em->plDist2 < 4000000.0f && w->routeAngAbs < 0.785398185f) {
                if (pG->Game_level <= 1 && (em->r_no_0 != 1 || em->r_no_1 != 1) && Rnd() % 10 > 4) {
                    w->atkWait = 30;
                    EmRoutineSet(em, 1, 1, 0, 0);
                    return;
                }
                if (!(w->flags & 0x200000)) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    return;
                }
            }
            if (em2dScreenInCk(em)) {
                if (pG->Game_level <= 1) {
                    if (w->jumpWait) {
                        goto next;
                    }
                    if (Rnd() % 10 > 4) {
                        w->jumpWait = 150;
                    }
                }
                if (w->jumpWait == 0) {
                    if (em->type == 4 && w->walkMode == 1) {
                        w->walkMode = 0;
                    }
                    switch ((u32) w->walkMode) {
                    case 0:
                    default:
                        if (em->plDist2 > 6250000.0f && em->plDist2 < 12250000.0f && w->routeAngAbs < 0.785398185f &&
                            !(w->flags & 0x200000)) {
                            EmRoutineSet(em, 1, 0xA, 0, 0);
                            return;
                        }
                        break;
                    case 1:
                        if (em->plDist2 > 9000000.0f && em->plDist2 < 16000000.0f && w->routeAngAbs < 0.785398185f &&
                            !(w->flags & 0x200000)) {
                            EmRoutineSet(em, 1, 7, 0, 0);
                            return;
                        }
                        break;
                    case 2:
                        if (em->plDist2 > 9000000.0f && em->plDist2 < 16000000.0f && w->routeAngAbs < 0.785398185f &&
                            !(w->flags & 0x200000)) {
                            EmRoutineSet(em, 1, 9, 0, 0);
                            return;
                        }
                        break;
                    }
                }
            }
        }
    }
next:
    if (!(w->flags & 0x2000) && !(em->flag & 0x40000000) && (pG->Frame_cnt & 3) != (em->emset_no & 3)) {
        Vec a;
        Vec b;

        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830) && (hit = em2dNoWallCk(em)) == 0) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            EmRoutineSet(em, 1, 0x15, hit, hit);
            return;
        }
    }
    if (em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f && w->routeAngAbs > 1.04719758f &&
        w->routeAngAbs < 1.91986215f) {
        Vec d;

        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1.0f;
        PSMTXMultVecSR(em->mat, &d, &d);
        em->ang.y = atan2f(d.x, d.z);
        EmRoutineSet(em, 1, 5, 0, 0);
        return;
    }
    w->atkCnt++;
    if (em2dFallCk(em)) {
        return;
    }
    if (w->stuckCnt % 10 == 9) {
        em2dToAirCk(em);
    }
}

// R1 == 0x03 Turn180: turns towards the target (turnAng eased), then Walk (2) or another turn.
static void em2d_R1_Turn180(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 ang;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if (w->targetAng < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x1A), (int) ARC(0x1B), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x1A), (int) ARC(0x1B), 5, 0x41, 0);
        }
        w->turnAng = em->ang.y + 3.14159274f;
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dGravityMove(em, w);
        if (em->seFlags28B & 0x10) {
            ang = Muku(&em->pos, &w->targetPos, w->turnAng, 0.0981747732f);
            w->turnAng += ang;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->ang.y += ang;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
    em2dFallCk(em);
}

// R1 == 0x04 SideStep: dodges to the free side (wall probes), then Turn180 / Walk; when the step ends
// against a wall it climbs it (W_Walk 0x15, flag 0x20).
static void em2d_R1_SideStep(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec nrm;
    Vec hit;
    Vec a;
    Vec b;
    int side;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        side = Rnd() & 1;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 0.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830)) {
            side = 0;
        }
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = -2000.0f;
        b.y = 500.0f;
        b.z = 0.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830)) {
            side = 1;
        }
        if (side) {
            MotionSetCore(em, &em->Motion, ARC(0x16), (int) ARC(0x17), 5, 0x41, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x16), (int) ARC(0x17), 5, 1, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
            break;
        }
        if ((em->seFlags28B & 1) && !(em->flag & 0x40000000)) {
            cModel* p = em->getPartsPtr(0);
            RotMatrix(m, &em->ang);
            TransMatrix(m, &p->world);
            // The whole probe is written in both arms (jump2 cross-jumps everything from `addi &b` on; the arms
            // keep their own `lfs 0.0` / `addi &a` / `lfs +-900` and the join block has no label).
            if (em->motFlags & 0x40) {
                a.x = 0.0f;
                a.y = 0.0f;
                a.z = 0.0f;
                b.x = -900.0f;
                b.y = 0.0f;
                b.z = 0.0f;
                PSMTXMultVec(m, &a, &a);
                PSMTXMultVec(m, &b, &b);
                if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x383830) && em2dNoWallCk(em) == 0) {
                    w->wallNrm = nrm;
                    em->pos = hit;
                    em->r_no_2++;
                }
            } else {
                a.x = 0.0f;
                a.y = 0.0f;
                a.z = 0.0f;
                b.x = 900.0f;
                b.y = 0.0f;
                b.z = 0.0f;
                PSMTXMultVec(m, &a, &a);
                PSMTXMultVec(m, &b, &b);
                if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x383830) && em2dNoWallCk(em) == 0) {
                    w->wallNrm = nrm;
                    em->pos = hit;
                    em->r_no_2++;
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 5, 1, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 3:
        w->flags |= 0x20;
        em->setStatus(EM_STATUS_IK_OFF);
        em2dSetWallMatrix(em);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
        }
        break;
    }
}

// R1 == 0x05 BackJump: hops backwards away from the player, then Wait (1).
static void em2d_R1_BackJump(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    if (em->r_no_3 == 0) {
        w->flags |= 0x100;
    }
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x18), (int) ARC(0x19), 5, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dGravityMove(em, w);
        if (MotionMoveF(em, 0)) {
            em2dSetAtkWait(w, 100, 75, 60, 30, 0);
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

// R1 == 0x06 Atk: the melee slash (one of two motions), em2dAtkCk on the hit frames; a miss scores
// an escape and hops back (BackJump) or waits.
static void em2d_R1_Atk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int zero;

    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x22), (int) ARC(0x23), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x22), (int) ARC(0x23), 10, 0x41, 0);
        }
        if (Rnd() & 1) {
            w->dmgTotal = 200;
        } else {
            w->dmgTotal = 0;
        }
        zero = 0;
        w->timer = 15;
        w->atkCnt = zero;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->atkHit = zero;
        em->r_no_2++;
    case 1:
        em2dGravityMove(em, w);
        if (w->timer) {
            w->timer--;
            em2dTurnTo(em, &pPLS->pos, 0.157079637f);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em2dSetAtkWaitR(w, 100, 75, 60, 45, 30);
            if (w->atkHit) {
                EmRoutineSet(em, 1, 5, 0, 1);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 1) {
            if (em->motFlags & 0x40) {
                em2dAtkCk(em, 0, 0xB);
                em2dAtkCk(em, 0, 0xC);
                em2dAtkCk(em, 0, 0xD);
            } else {
                em2dAtkCk(em, 0, 7);
                em2dAtkCk(em, 0, 8);
                em2dAtkCk(em, 0, 9);
            }
        }
        break;
    }
}

// R1 == 0x07 AtkPoison: spits the poison glob (em2dSetPoison on the spit frame, poisonWait 300..450),
// then Wait (1).
static void em2d_R1_AtkPoison(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int fe = em->r_no_2;

    w->flags |= 0x10000;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x20), (int) ARC(0x21), 10, 1, 0);
        w->atkCnt = fe;
        w->timer = 10;
        if (Rnd() & 1) {
            w->dmgTotal = 200;
        } else {
            w->dmgTotal = 0;
        }
        w->poisonWait = Rnd() % 150 + 300;
        EstSet((int) em, -1, 0, 0, 0x25, 2, 0, 0, (u32) em, 0);
        w->sndId = SndCall(8, 0x16, &em->pos, em->id, 0, em);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dGravityMove(em, w);
        if (w->timer) {
            w->timer--;
            em2dTurnTo(em, &pPLS->pos, 0.0981747732f);
        }
        if (MotionMoveF(em, 0)) {
            em2dSetAtkWaitR(w, 100, 75, 60, 45, 30);
            EmRoutineSet(em, 1, 1, 0, 0);
            break;
        }
        if (em->seFlags28B & 1) {
            EstSet((int) em, -1, 0, 0, 0x25, 3, 0, 0, (u32) em, 0);
            em2dSetPoison(em, 0);
        }
        break;
    }
}

// R1 == 0x08 CriticalAtk: the decapitating bite (em2dAtkCk kind 2): a hit at low player hp kills him
// (plem2d_CriticalHit, em2dPlHeadLost); a miss scores an escape, then Wait (1).
static void em2d_R1_CriticalAtk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int zero;

    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x1E), (int) ARC(0x1F), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x1E), (int) ARC(0x1F), 10, 0x41, 0);
        }
        if (Rnd() & 1) {
            w->dmgTotal = 200;
        } else {
            w->dmgTotal = 0;
        }
        zero = 0;
        w->timer = 5;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em2dTurnTo(em, &pPLS->pos, 0.196349546f);  // struct view: the pPL load stays below the timer store
            em2dActEvtSetKick(em, w->kickSide);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            em2dSetAtkWaitR(w, 100, 75, 60, 45, 30);
            EmRoutineSet(em, 1, 1, 0, 0);
            break;
        }
        if ((em->seFlags28B & 1) && w->atkHit == 0) {
            if (em->motFlags & 0x40) {
                em2dAtkCk(em, 2, 0xB);
                em2dAtkCk(em, 2, 0xC);
                em2dAtkCk(em, 2, 0xD);
            } else {
                em2dAtkCk(em, 2, 7);
                em2dAtkCk(em, 2, 8);
                em2dAtkCk(em, 2, 9);
            }
            if (w->atkHit) {
                pG->pl_life = 0;
                em2dPlHeadLost();
            }
        }
        break;
    }
}

// Player damage routine of the critical bite: the head comes off (em2dPlHeadLost), routine held.
static void plem2d_CriticalHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC_PTR(pG->pPlayer, 0x4C), (int) PL_ARC_PTR(pG->pPlayer, 0x4D), 5, 1, 0);
        em2dPlHeadLost();
        pl->r_no_2++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

// R1 == 0x09 JumpSign: the crouch before the leap, then JumpAtk (0xA).
static void em2d_R1_JumpSign(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x34), (int) ARC(0x35), 5, 1, 0);
        w->timer = 20;
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0) || (em->seFlags28B & 4)) {
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
}

// Branch check of JumpAtk (0xA): on the catch frame em2dCatchCk decides the grab: JumpAtkHit (0xB,
// the face grab) or, half the time on a healthy player, JumpKickHit (0xC, knocked down).
static void em2d_R1_br_JumpAtk(cEm2d* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em2dCatchCk(em)) {
        if ((Rnd() & 1) || (s16) pG->pl_life > 300) {
            EmRoutineSetW(em, 1, 0xC, 0, 0);
        } else {
            EmRoutineSetW(em, 1, 0xB, 0, 0);
        }
    }
}

// R1 == 0x0A JumpAtk: the leap at the player (flag 0x10 = jumping, kickSide picked), lands with the
// landing effect; a miss scores an escape, sets jumpWait 150..300 and goes back to Wait (1).
static void em2d_R1_JumpAtk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int fe = em->r_no_2;
    u32 f = w->flags | 0x10000;

    // block 0 tie: `cmpwi fe,0` and `stw flags` are both ready at t=5 with priority 2 and sched1 ranks the
    // store first (its source dies, weight 0 vs the compare's +1); the original issues the compare first.
    // The codeless tied launder adds a latency-1 link before the store, so it is ready one cycle later.
    asm("" : "=r"(f) : "0"(f)); // COMPILER-DIFF: #8 candidate (sched1 compare/store tie)
    w->flags = f;
    switch (fe) {
        do { } while (0);  // dead loop before the label: the arm does not know fe == 0
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x37), 5, 1, 0);
        w->atkHit = fe;
        w->atkCnt = fe;
        w->timer = 30;
        if (Rnd() & 1) {
            w->dmgTotal = 200;
        } else {
            w->dmgTotal = 0;
        }
        w->kickSide = Rnd() & 1;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em2dTurnTo(em, &pPLS->pos, 0.261799395f);
            em2dActEvtSetKick(em, w->kickSide);
        }
        if (em->seFlags28B & 8) {
            w->flags |= 0x10;
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            fe = 0;
            w->dmgTotal = fe;
            IntSet(w->jumpWait, Rnd() % 150 + 150);  // reference store: the pG load stays below it
            w->atkWait = 100;
            if (pG->Game_level > 1) {
                IntSet(w->atkWait, 75);
            }
            if (pG->Game_level > 3) {
                IntSet(w->atkWait, 60);
            }
            if (pG->Game_level > 6) {
                IntSet(w->atkWait, 45);
            }
            if (pG->Game_level == 10) {
                IntSet(w->atkWait, 30);
            }
            EmRoutineSet(em, 1, 1, fe, fe);
        }
        break;
    }
}

// R1 == 0x0B JumpAtkHit: on the player's face (plem2d_JumpAtkHit, cut camera): 500 damage per bite
// while he mashes the button (PlGacha); over 30 he throws it off (JumpAtkCounter 0xD when he kicks it
// away), else it bites again and kills him at 1 hp (head melted by the acid).
static void em2d_R1_JumpAtkHit(cEm2d* em)
{
    Em2dWork* w;
    Vec spd;
    Vec rot;
    int fe;
    int r;

    em->dmg.set(0, 2);
    w = EM2D_WK(em);
    w->flags |= 0x10000;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x3B), (int) ARC(0x3C), 5, 1, 0);
        PlSetDamageSe(0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem2d_JumpAtkHit, -48.1500015f, 0.0f, 921.190002f);
        GameAddPoint(LVADD_PL_DAMAGE);
        PlGachaInit();
        EstSet((int) em, -1, 0, 0, 0x25, 1, 0, 0, (u32) em, (void*) fe);
        SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        w->timer = 10;
        em->r_no_2++;
    case 1:
        em->setStatus(EM_STATUS_IK_OFF);
        PlGachaMove();
        if (em->seFlags28B & 1) {
            LifeDownSet2(pPL, 500, 0, 1);
            PlSetDamageSe(9);
        }
        if (w->timer) {
            w->timer--;
            r = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            r = MotionMoveF(em, 0);
        }
        if (r) {
            if ((u32) PlGachaGet() > 30 && (s16) pG->pl_life > 1) {
                em->r_no_2 = 2;
            } else {
                em->r_no_2 = 6;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x3F), 0, 5, 0, 0);
        w->timer = 127;
        w->timer8 = 10;
        em->r_no_2++;
    case 3:
    case 9:
    step3:
        em->setStatus(EM_STATUS_IK_OFF);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (w->timer8) {
            w->timer8--;
        } else {
            PSVECScale(&spd, &spd, 2.0f);
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 4;
        }
        break;
    case 4:
        em2dSetdLandingEff(em);
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 5, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            IntSet(w->jumpWait, Rnd() % 150 + 150);
            w->atkWait = 100;
            if (pG->Game_level > 1) {
                IntSet(w->atkWait, 75);
            }
            if (pG->Game_level > 3) {
                IntSet(w->atkWait, 60);
            }
            if (pG->Game_level > 6) {
                IntSet(w->atkWait, 45);
            }
            if (pG->Game_level == 10) {
                IntSet(w->atkWait, 30);
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    case 6:
        MotionSetCore(em, &em->Motion, ARC(0x3D), (int) ARC(0x3E), 5, 0, 0);
        w->timer8 = 45;
        EstSet((int) em, -1, 0, 0, 0x25, 6, 0, 0, (u32) em, 0);
        em->r_no_2++;
    case 7:
        em->setStatus(EM_STATUS_IK_OFF);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (w->timer8) {
            w->timer8--;
        } else {
            PSVECScale(&spd, &spd, 2.0f);
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (em->seFlags28B & 1) {
            LifeDownSet2(pPL, 500, 0, 1);
            PlSetDamageSe(9);
        }
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 8;
        } else if (em->seFlags28B & 4) {
            if ((s16) pG->pl_life <= 1) {
                em->r_no_2 = 10;
                pGS->pl_life = 0;
            } else {
                em->r_no_2 = 8;
            }
        }
        break;
    case 8:
        em->r_no_2++;
        goto step3;
    case 10:
        em->r_no_2++;
    case 11:
        em->setStatus(EM_STATUS_IK_OFF);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (w->timer8) {
            w->timer8--;
        } else {
            PSVECScale(&spd, &spd, 2.0f);
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 4;
        }
        break;
    }
    em->x3A8 = em->pos;
}

// Player damage routine of the face grab: grabbed (weapon hidden), the struggle, the throw-off, the
// kick, or the death (em2dPlHeadMelt, die camera); camera by r_no_3 side (em2dCamMove).
static void plem2d_JumpAtkHit(cPlayer* pl)
{
    int fe;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    fe = pl->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x5F), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.000031f, 400.0f);
        pl->m_Work0 = 127;
        EstSet((int) pl, -1, 0, 0, 0x25, 0x13, 0, 0, (u32) pl, (void*) fe);
        pl->r_no_3 = Rnd() & 1;
        pl->m_Work0 = 10;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
            EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            MotionMoveF(pl, 0);
        }
        if (em2dCamMove((cEm2d*)pl->pEmCatch, pl->r_no_3, 0.3f) == 0) {
            if (pl->r_no_3 == 0) {
                pl->r_no_3 = 2;
            }
            if (pl->r_no_3 == 1) {
                pl->r_no_3 = 3;
            }
            if (pl->r_no_3 == 4) {
                pl->r_no_3 = 6;
            }
            if (pl->r_no_3 == 5) {
                pl->r_no_3 = 7;
            }
        }
        if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0xB) {
            EndPlDamage();
            pl->dmg.set(0, 30);
            break;
        }
        if (pl->frame > 64.6999969f && pl->frame < 65.3000031f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    case 2:
    case 4:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x63), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x15, 0, 0, (u32) pl, 0);
        pl->r_no_2++;
    case 3:
    case 5:
        if (em2dCamMove((cEm2d*)pl->pEmCatch, pl->r_no_3, 0.3f) == 0) {
            if (pl->r_no_3 == 0) {
                pl->r_no_3 = 2;
            }
            if (pl->r_no_3 == 1) {
                pl->r_no_3 = 3;
            }
            if (pl->r_no_3 == 4) {
                pl->r_no_3 = 6;
            }
            if (pl->r_no_3 == 5) {
                pl->r_no_3 = 7;
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        if (pl->frame > 14.6999998f && pl->frame < 15.3000002f) {
            SndCall(8, 0x35, &pl->pos, pl->pEmCatch->id, 0, pl);
        }
        break;
    case 6:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x61), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x14, 0, 0, (u32) pl, 0);
        pl->r_no_2++;
    case 7:
        if (em2dCamMove((cEm2d*)pl->pEmCatch, pl->r_no_3, 0.3f) == 0) {
            if (pl->r_no_3 == 0) {
                pl->r_no_3 = 2;
            }
            if (pl->r_no_3 == 1) {
                pl->r_no_3 = 3;
            }
            if (pl->r_no_3 == 4) {
                pl->r_no_3 = 6;
            }
            if (pl->r_no_3 == 5) {
                pl->r_no_3 = 7;
            }
        }
        if (pl->frame > 27.7000008f && pl->frame < 28.2999992f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        MotionMoveF(pl, 0);
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    case 8:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x62), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x16, 0, 0, (u32) pl, 0);
        pl->r_no_2++;
    case 9:
        if (em2dCamMove((cEm2d*)pl->pEmCatch, pl->r_no_3, 0.3f) == 0) {
            if (pl->r_no_3 == 0) {
                pl->r_no_3 = 2;
            }
            if (pl->r_no_3 == 1) {
                pl->r_no_3 = 3;
            }
            if (pl->r_no_3 == 4) {
                pl->r_no_3 = 6;
            }
            if (pl->r_no_3 == 5) {
                pl->r_no_3 = 7;
            }
        }
        if (pl->frame > 14.6999998f && pl->frame < 15.3000002f) {
            SndCall(8, 0x35, &pl->pos, pl->pEmCatch->id, 0, pl);
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 10:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x64), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x17, 0, 0, (u32) pl, 0);
        em2dPlHeadMelt(pl);
        pl->r_no_2++;
    case 11:
        em2dDieCamMove((cEm2d*)pl->pEmCatch);
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// Player camera side per catch step: the xFF variants 0 / 1 / 4 / 5 turn into 2 / 3 / 6 / 7 once
// the camera has arrived.
static inline void em2dCatchCamMove(cEm2d* em, cEm* pl)
{
    if (em2dCamMove(em, pl->r_no_3, 0.3f) == 0) {
        if (pl->r_no_3 == 0) {
            pl->r_no_3 = 2;
        }
        if (pl->r_no_3 == 1) {
            pl->r_no_3 = 3;
        }
        if (pl->r_no_3 == 4) {
            pl->r_no_3 = 6;
        }
        if (pl->r_no_3 == 5) {
            pl->r_no_3 = 7;
        }
    }
}

// R1 == 0x0C JumpKickHit: the leap knocked the player down (300 damage, plem2d_JumpKickHit, camera
// 4 / 5): the insect lands beyond him, jumpWait 150..300, then Wait (1).
static void em2d_R1_JumpKickHit(cEm2d* em)
{
    Em2dWork* w;
    f32 fl;

    em->dmg.set(0, 2);
    w = EM2D_WK(em);
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x65), 0, 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem2d_JumpKickHit, 0.0f, 0.0f, 1500.0f);
        SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        LifeDownSet(pPL, 300, 0);
        em->r_no_3 = (Rnd() & 1) + 4;
        w->timer = 7;
        em->r_no_2++;
    case 1:
        em->setStatus(EM_STATUS_IK_OFF);
        em2dCatchCamMove(em, em);
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 0.3f, 0.2f);
        } else if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl + 500.0f) {
                em->pos.y = fl;
                em->r_no_2 = 4;
            } else {
                em->r_no_2 = 2;
            }
        }
        break;
    case 2:
        w->spd.x = 0.0f;
        w->spd.y = -500.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 3:
        w->flags |= 0x40;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 5, 1, 0);
        MotionMoveF(em, 0);
        fl = SatMgr.getFloor(&em->pos_old, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            em->r_no_2++;
        }
        break;
    case 4:
        em2dSetdLandingEff(em);
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 5, 1, 0);
        em->r_no_2++;
    case 5:
        em2dCatchCamMove(em, em);
        if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            IntSet(w->jumpWait, Rnd() % 150 + 150);  // reference store: the pG load stays below it
            w->atkWait = 100;
            if (pG->Game_level > 1) {
                IntSet(w->atkWait, 75);
            }
            if (pG->Game_level > 3) {
                IntSet(w->atkWait, 60);
            }
            if (pG->Game_level > 6) {
                IntSet(w->atkWait, 45);
            }
            if (pG->Game_level == 10) {
                IntSet(w->atkWait, 30);
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
}

// Player damage routine of the knock-down: the fall motion, then the standard knock-down (PlSetDamage 8).
static void plem2d_JumpKickHit(cPlayer* pl)
{
    int t;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x66), 0, 5, 1, 0);
        PlSetFace(1);
        pl->m_Work0 = 7;
        pl->r_no_2++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        t = pl->m_Work0;
        if (t) {
            pl->m_Work0 = t - 1;
        } else {
            PlSetDamage(8, 0, 0);
            EstSet((int) pl, -1, 0, 0, 0x25, 0x1A, 0, 0, (u32) pl, (void*) t);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// R1 == 0x0D JumpAtkCounter: kicked off the player's face (em2dKickAction, critical scored): flies
// back, 100 damage on landing (dies into Die_Down), else WakeupWait (0xE).
static void em2d_R1_JumpAtkCounter(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    int fe = em->r_no_2;

    em->dmg.m_Timer = 2;
    switch (fe) {
    case 0:
        em->ang.y = GetXZAngle(&em->pos, &pPL->pos);
        PSMTXRotRad(m, 'y', em->ang.y);
        TransMatrix(m, &pPL->pos);
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = -5000.0f;
        PSMTXMultVec(m, &em->pos, &em->pos);
        pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
        MotionSetCore(em, &em->Motion, ARC(0x40), (int) ARC(0x41), 5, 1, 5);
        GameAddPoint(LVADD_CRITICALHIT);
        w->timer = fe;
        w->flags &= ~0x4000;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dCatchCamMove(em, em);
        em2dGravityMove(em, w);
        if (MotionMoveF(em, 0)) {
            LifeDownSet(em, 100, 0);
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else {
                IntSet(w->jumpWait, Rnd() % 150 + 150);  // reference store: the pG load stays below it
                w->atkWait = 100;
                if (pG->Game_level > 1) {
                    IntSet(w->atkWait, 75);
                }
                if (pG->Game_level > 3) {
                    IntSet(w->atkWait, 60);
                }
                if (pG->Game_level > 6) {
                    IntSet(w->atkWait, 30);
                }
                if (pG->Game_level == 10) {
                    IntSet(w->atkWait, 0);
                }
                EmRoutineSet(em, 1, 0xE, 0, 0);
            }
            break;
        }
        if (em->frame > 14.6999998f && em->frame < 15.3000002f) {
            w->timer = 5;
        }
        if (w->timer) {
            w->timer--;
            em2dSetCrash(em, 1500.0f);
        }
        break;
    }
}

// Offers the kick action button (variant `side`) while the insect hangs on the player's face and he
// faces it.
void em2dActEvtSetKick(cEm2d* em, int side)
{
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) > 1.57079637f) {
        return;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 700.0f) {
        return;
    }
    ActBtn.set(7, 0xB, (int) em2dKickAction, (int) em, 1, 1, 0, 0);
}

// Action button callback of the kick: the player's kick routine (plem2dKick), the insect's
// JumpAtkCounter (0xD), critical scored, damage held 30 frames.
static void em2dKickAction(cEm2d* em)
{
    SetPlDamage(em, plem2dKick);
    pPL->dmg.set(0, 30);
    if (pSUB && pSUB->plDist2 < 9000000.0f) {
        cDmgInfo* d = &pSUB->dmg;  // &pSUB->dmg is computed before the dead test

        if (!em2dDeadCk(pSUB)) {
            d->set(0, 30);
        }
    }
    EmRoutineSet(em, 1, 0xD, 0, 0);
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player routine of the kick: the kick motion; at frame 15 the foot sweep (PlWepHitCheck3 kind 0x14,
// 1200 units) hits the insect; critical scored.
static void plem2dKick(cPlayer* pl)
{
    Vec pos;

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    StaFlagOn(pG, STA_PL_EM_ACTION);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x60), 0, 6, 1, 5);
        GameAddPoint(LVADD_CRITICALHIT);
        pl->m_Work0 = 27;
        pl->m_Work1 = 7;
        pl->m_Work2 = 10;
        SndCall(1, 0x35, &pPLS->pos, 0, 0, pPLS);  // struct view: the pPL load stays below the stores
        pl->r_no_2++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 15);
            break;
        }
        if (pl->frame > 14.6999998f && pl->frame < 15.3000002f) {
            SndCall(1, 0x3A, &pPL->pos, 0, 0, pPL);
            SndCall(1, 0x3B, &pPL->pos, 0, 0, pPL);
            pos.x = 0.0f;
            pos.y = 1500.0f;
            pos.z = 300.0f;
            PSMTXMultVec(pPL->mat, &pos, &pos);
            PlWepHitCheck3(&pos, 0x14, 10, 1200.0f);
            pos.x = 0.0f;
            pos.y = 1000.0f;
            pos.z = 300.0f;
            PSMTXMultVec(pPL->mat, &pos, &pos);
            PlWepHitCheck3(&pos, 0x14, 10, 1200.0f);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// R1 == 0x0E WakeupWait: lies on its back 30..60 frames (flag 0x20000), then Wakeup (0xF).
static void em2d_R1_WakeupWait(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int hit;

    w->flags |= 0x20000;
    switch (em->r_no_2) {
    case 0:
        w->x50C = Rnd() % 30 + 30;
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x50C) {
            w->x50C--;
        } else if ((hit = em2dDownJumpCk(em)) == 0) {
            EmRoutineSet(em, 1, 0xF, hit, hit);
        }
        break;
    }
}

// R1 == 0x0F Wakeup: rights itself, then SideStep (4), Turn180 (3) or Walk (2).
static void em2d_R1_Wakeup(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int zero;
    int hit;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->Motion, ARC(0x2C), (int) ARC(0x2D), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x42), (int) ARC(0x43), 5, 1, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            zero = 0;
            w->dmgTotal = zero;
            hit = em2dLockCk(em);
            if (hit) {
                EmRoutineSet(em, 1, 4, zero, zero);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, hit, hit);
            } else {
                EmRoutineSet(em, 1, 2, hit, hit);
            }
        }
        break;
    }
}

// R1 == 0x10 DownJump: leaps up onto a wall (em2dDownJumpCk found one above / ahead): airborne (0x400)
// until the wall probe hits, snaps onto it (flags 0x120) and continues as W_Turn180 (0x18) or W_Walk.
static void em2d_R1_DownJump(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x5B), (int) ARC(0x5C), 5, 1, 0);
        w->timer = 10;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            w->flags |= 0x400;
        } else {
            w->flags |= 0x40;
            em->dmg.m_Timer = 2;
        }
        MotionMoveF(em, 0);
        a = em->pos_old;
        b = em->pos;
        a.y += 1000.0f;
        b.y += 1000.0f;
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830) == 0) {
            break;
        }
        PSMTXRotRad(m, 'x', 3.14159274f);
        em->pos.y = hit.y;
        TransMatrix(em->mat, &em->pos);
        PSMTXConcat(em->mat, m, em->mat);
        em->r_no_2++;
    case 2:
        em2dSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(EM_STATUS_IK_OFF);
        em2dSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = -1.0f;
            w->wallNrm.z = 0.0f;
            if (plPos.z < -500.0f) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            }
            EmRoutineSet(em, 1, 0x15, 0, 0);
        }
        break;
    }
}

// R1 == 0x11 ToCeiling: leaps from the floor up to the ceiling (em2dToCeilingCk), snaps onto it and
// continues as a wall walker (W_Turn180 / W_Walk).
static void em2d_R1_ToCeiling(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x6F), (int) ARC(0x70), 5, 1, 0);
        w->timer = 10;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->flags |= 0x40;
            em->dmg.m_Timer = 2;
        }
        MotionMoveF(em, 0);
        a = em->pos_old;
        b = em->pos;
        a.y += 1000.0f;
        b.y += 1000.0f;
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830) == 0) {
            break;
        }
        PSMTXRotRad(m, 'z', 3.14159274f);
        em->pos.y = hit.y;
        TransMatrix(em->mat, &em->pos);
        PSMTXConcat(em->mat, m, em->mat);
        em->r_no_2++;
    case 2:
        em2dSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(EM_STATUS_IK_OFF);
        em2dSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = -1.0f;
            w->wallNrm.z = 0.0f;
            if (plPos.z < -500.0f) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            }
            EmRoutineSet(em, 1, 0x15, 0, 0);
        }
        break;
    }
}

// R1 == 0x12 JumpDown: jumps off an edge (em2dJumpDownCk, turning to jumpAng, flag 0x40 airborne),
// lands on the floor below (landing effect; dies in the water / a bottomless drop), then Turn180 / Walk.
static void em2d_R1_JumpDown(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec spd;
    Vec rot;
    f32 fl;

    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x38), 5, 0, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->jumpAng, 0.196349546f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (em->seFlags28B & 1) {
            w->flags |= 0x40;
            spd.x *= 0.5f;
            spd.z *= 0.5f;
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x39), 5, 4, 0);
        w->spd.x = 0.0f;
        w->spd.y = -450.0f;
        w->spd.z = 30.0f;
        em->r_no_2++;
    case 3:
        w->flags |= 0x40;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            if (fl <= -99000.0f) {
                em->be_flag |= 0x10000;
                EmRoutineSet(em, 3, 0, 0, 0);
                break;
            }
            em->r_no_2++;
        }
        MotionMoveF(em, 0);
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x3A), 5, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x13 WallOver: jumps over a low wall found by em2dWallOverCk (turning to jumpAng), then
// Turn180 / Walk.
static void em2d_R1_WallOver(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec d;
    int t;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x48), (int) ARC(0x49), 10, 5, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->jumpAng, 0.392699093f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMoveF(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x48), (int) ARC(0x49), 10, 5, 0);
        w->timer = 30;
        em->r_no_2++;
    case 3:
        w->flags |= 0x20;
        em->setStatus(EM_STATUS_IK_OFF);
        em2dSetWallMatrix2(em, 0.4f);
        MotionMoveF(em, 0);
        t = w->timer;
        if (t) {
            w->timer = t - 1;
        } else if (w->wallNrm.y > 0.699999988f) {
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = 1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
            em->ang.y = atan2f(d.x, d.z);
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, t, t);
            } else {
                EmRoutineSet(em, 1, 2, t, t);
            }
        }
        break;
    }
}

// R1 == 0x14 W_Wait: idle on the wall / ceiling (flags 0x120), then W_Walk (0x15) when the player is
// found or W_Turn180 (0x18).
static void em2d_R1_W_Wait(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec plPos;

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x46), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        em2dSetWallMatrix2(em, 0.4f);
        MotionMoveF(em, 0);
        if (!(w->flags & 0x200)) {
            if (em->plDist2 < 49000000.0f) {
                w->flags |= 0x200;
            }
        } else if (w->atkWait == 0) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
        } else if (plPos.z < -8000.0f) {
            EmRoutineSet(em, 1, 0x18, 0, 0);
        }
        break;
    }
}

// R1 == 0x15 W_Walk: walks the wall / ceiling (em2dSetWallMatrix2 follows the surface) towards the
// player (walkMode 0..3 by chance: straight, around, keep away), 8..11 steps then W_Wait; attacks from
// the wall (W_Atk 0x16 within reach, W_AtkPoison 0x17 or W_Fall 0x19 = the drop attack, the
// floor melee 6 when low), W_Turn180 (0x18) when the player is behind, back to the floor (BackJump 5).
static void em2d_R1_W_Walk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Mtx inv;
    Vec plPos;
    Vec d;
    f32 ang;
    int t;
    int r;
    f32 alpha;

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x48), (int) ARC(0x49), 10, 5, 0);
        w->walkMode = Rnd() & 3;
        if (em->r_no_3) {
            w->turnAng = em2dGetPlDir(em, &w->wallTarget);
            w->timer = Rnd() % 60 + 60;
        } else {
            w->timer = 0;
            w->turnAng = w->plDir;
        }
        w->timer8 = (Rnd() & 3) + 8;
        em->r_no_2++;
    case 1:
        PSMTXInverse(em->mat, inv);
        plPos = pPL->pos;
        plPos.y += 1800.0f;
        PSMTXMultVec(inv, &plPos, &plPos);
        if (w->timer) {
            w->timer--;
            ang = Muku2(0.0f, w->turnAng, 0.0981747732f);
            PSMTXRotRad(m, 'y', ang);
            PSMTXConcat(em->mat, m, em->mat);
            w->turnAng -= ang;
        } else {
            ang = Muku2(0.0f, w->plDir, 0.0981747732f);
            PSMTXRotRad(m, 'y', ang);
            PSMTXConcat(em->mat, m, em->mat);
            w->plDir -= ang;
        }
        em2dSetWallMatrix2(em, 0.4f);
        if (MotionMoveF(em, 0)) {
            t = w->timer8;
            if (t) {
                w->timer8 = t - 1;
            } else {
                // COMPILER-DIFF: candidate #12 (fallthrough-arm form) + #8: the target compares the load temp (f0)
                // and then `alpha` (f12, a copy of it, `fmr f12,f0` before the first compare). cse never
                // canonicalises a hard register, so the fr0 pin keeps the first compare on the load; the
                // DFmode read of fr0 in the second arm keeps it live past the copy, so regmove does not move
                // its death onto the copy (docs/matching.md #8) and the `fmr` survives.
                register f32 ny asm("fr0");
                register f64 nyd asm("fr0");
                ny = w->wallNrm.y;
                alpha = ny;
                if (ny > 0.899999976f || ({ asm("" : "=m"(inv[0][0]) : "f"(nyd)); alpha; }) < -0.899999976f) {
                    w->atkWait = Rnd() % 30 + 30;
                    EmRoutineSet(em, 1, 0x14, t, t);
                    break;
                }
            }
        }
        alpha = w->wallNrm.y;
        if ((w->flags & 1) && plPos.x > -300.0f && plPos.x < 300.0f && plPos.y > 0.0f && plPos.y < 2200.0f &&
            plPos.z > 0.0f && plPos.z < 2000.0f) {
            if (alpha > 0.899999976f) {
                t = w->flags & 0x200000;
                if (t == 0) {
                    d.x = 0.0f;
                    d.y = 0.0f;
                    d.z = 1.0f;
                    PSMTXMultVecSR(em->mat, &d, &d);
                    em->ang.y = atan2f(d.x, d.z);
                    EmRoutineSet(em, 1, 6, t, t);
                    break;
                }
            } else if (em->plDist2 < 250000.0f && alpha < -0.899999976f) {
                if (fabsf(em->pos.y - pPL->pos.y) < 4000.0f) {
                    EmRoutineSet(em, 1, 0x17, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0x19, 0, 0);
                }
                break;
            } else if (alpha < -0.899999976f || (alpha > -0.100000001f && alpha < 0.100000001f)) {
                EmRoutineSet(em, 1, 0x16, 0, 0);
                break;
            }
        }
        if (alpha < -0.899999976f && em->plDist2 < 250000.0f) {
            if (fabsf(em->pos.y - pPL->pos.y) < 4000.0f && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x17, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            }
        } else if (w->flags & 0x2000) {
            EmRoutineSet(em, 1, 0x19, 0, 0);
        } else {
            t = w->flags & 0x8000;
            if (t) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
                break;
            }
            if (plPos.z < -8000.0f) {
                EmRoutineSet(em, 1, 0x18, t, t);
            }
            r = em2dNoWallCk(em);
            if (r) {
                EmRoutineSet(em, 1, 0x19, t, t);
            } else {
                int f = em2dWallFallCk(em);
                if (f) {
                    EmRoutineSet(em, 1, 0x19, r, r);
                } else if (w->wallNrm.y > 0.899999976f && em->plDist2 < 1440000.0f &&
                           fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f && w->routeAngAbs > 1.04719758f &&
                           w->routeAngAbs < 1.91986215f) {
                    d.x = 0.0f;
                    d.y = 0.0f;
                    d.z = 1.0f;
                    PSMTXMultVecSR(em->mat, &d, &d);
                    em->ang.y = atan2f(d.x, d.z);
                    EmRoutineSet(em, 1, 5, f, f);
                }
            }
        }
        break;
    }
    w->atkCnt++;
}

// R1 == 0x29 W_WalkTest: debug set 5: walks the wall forever.
static void em2d_R1_W_WalkTest(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x48), (int) ARC(0x49), 10, 5, 0);
        em->r_no_2++;
    case 1:
        em2dSetWallMatrix2(em, 0.4f);
        MotionMoveF(em, 0);
        break;
    }
}

// R1 == 0x16 W_Atk: the slash from the wall (flag 0x20), em2dAtkCk on the hit frames; a miss scores an
// escape; then W_Wait (0x14) or W_Walk.
static void em2d_R1_W_Atk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec plPos;

    w->flags |= 0x20;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        if (plPos.y > 1000.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x1C), (int) ARC(0x1D), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x78), (int) ARC(0x79), 10, 1, 0);
        }
        w->timer = 10;
        w->atkCnt = 0;
        w->atkHit = 0;
        em->r_no_2++;
    case 1:
        em2dSetWallMatrix2(em, 0.4f);
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            if (w->atkHit) {
                em2dSetAtkWait(w, 100, 75, 60, 45, 30);
                EmRoutineSet(em, 1, 0x14, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x15, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 1) {
            if (em->motFlags & 0x40) {
                em2dAtkCk(em, 1, 0xB);
                em2dAtkCk(em, 1, 0xC);
                em2dAtkCk(em, 1, 0xD);
            } else {
                em2dAtkCk(em, 1, 7);
                em2dAtkCk(em, 1, 8);
                em2dAtkCk(em, 1, 9);
            }
        }
        break;
    }
}

// R1 == 0x17 W_AtkPoison: spits the poison glob from the wall (frame 23, poisonWait 300..450), then W_Wait.
static void em2d_R1_W_AtkPoison(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int fe;
    int end;

    w->flags |= 0x20;
    em->setStatus(EM_STATUS_IK_OFF);
    w->flags |= 0x10000;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x46), 0, 10, 5, 0);
        EstSet((int) em, -1, 0, 0, 0x25, 7, 0, 0, (u32) em, (void*) fe);
        w->sndId = SndCall(8, 0x16, &em->pos, em->id, 0, em);
        w->timer = 30;
        em->r_no_2++;
    case 1:
        em2dSetWallMatrix2(em, 0.4f);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x20), (int) ARC(0x21), 10, 1, 0);
        w->timer = 10;
        w->atkCnt = 0;
        if (Rnd() & 1) {
            w->dmgTotal = 200;
        } else {
            w->dmgTotal = 0;
        }
        w->poisonWait = Rnd() % 150 + 300;
        em->r_no_2++;
    case 3:
        em2dSetWallMatrix2(em, 0.4f);
        end = MotionMoveF(em, 0);
        if (end) {
            em2dSetAtkWaitR(w, 100, 75, 60, 45, 30);
            EmRoutineSet(em, 1, 0x14, 0, 0);
        } else if (em->frame > 22.7000008f && em->frame < 23.2999992f) {
            EstSet((int) em, -1, 0, 0, 0x25, 9, 0, 0, (u32) em, (void*) end);
            em2dSetPoison(em, 1);
        }
        break;
    }
}

// R1 == 0x18 W_Turn180: turns around on the wall, then W_Wait (0x14).
static void em2d_R1_W_Turn180(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x1A), (int) ARC(0x1B), 5, 0, 0);
        em->r_no_2++;
    case 1:
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        RotMatrix(m, &rot);
        PSMTXConcat(em->mat, m, em->mat);
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        em2dSetWallMatrix2(em, 0.4f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x14, 0, 0);
        }
        break;
    }
}

// Wall fall step: the position follows `spd` (added twice: before and after the floor check),
// which falls 20 per frame. 1 = landed (or died below the floor), 0 = still falling.
// The wall-fall step (em2d_R1_W_Fall): open-coded like the damage falls (see EM2D_DM_FALL); the
// macro ends in `else` and the caller supplies the fall arm as the following block.
#define EM2D_WALL_FALL(em, w, v_, fl_)                                                            \
    PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                               \
    (w)->spd.y -= 20.0f;                                                                       \
    fl_ = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                                 \
    PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                               \
    if ((em)->pos.y < fl_) {                                                                   \
        (em)->pos.y = fl_;                                                                     \
        if (fl_ <= -99000.0f) {                                                                \
            (em)->be_flag |= 0x10000;                                                          \
            EmRoutineSet(em, 3, 0, 0, 0);                                                      \
        } else {                                                                               \
            v_.x = 0.0f;                                                                       \
            v_.y = 0.0f;                                                                       \
            v_.z = 1.0f;                                                                       \
            PSMTXMultVecSR((em)->mat, &v_, &v_);                                               \
            (em)->ang.y = atan2f(v_.x, v_.z);                                                  \
            em2dSetdLandingEff(em);                                                            \
            MotionSetCore(em, &(em)->Motion, ARC(0x4F), 0, 5, 1, 0);                              \
            MotionMoveF(em, 0);                                                                \
            (em)->r_no_2 = 6;                                                                     \
        }                                                                                      \
    } else

// R1 == 0x19 W_Fall: drops off the wall / ceiling onto the player (flags 0x140, the fall-catch test
// em2dFallCatchCk grabs him half the time or at low hp -> JumpAtkHit / JumpKickHit), lands on the
// floor (em2dSetFallMatrix, landing effect), then Turn180 / Walk.
static void em2d_R1_W_Fall(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec d;
    f32 fl;
    int t;

    w->flags |= 0x140;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x4B), (int) ARC(0x4C), 5, 1, 0);
        if (w->wallNrm.y > 0.899999976f) {
            w->spd.x = 0.0f;
            w->spd.y = 80.0f;
            w->spd.z = 80.0f;
            PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        } else {
            PSVECScale(&w->wallNrm, &w->spd, 150.0f);
        }
        w->timer = 6;
        SndCall(8, 0x23, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        w->flags |= 0x1000;
        t = w->timer;
        if (t) {
            w->timer = t - 1;
            em2dSetWallMatrix2(em, 0.4f);
            MotionMoveF(em, 0);
            if (w->timer != 0) {
                break;
            }
            em->r_no_2++;
            break;
        }
        EM2D_WALL_FALL(em, w, d, fl) {
            em2dSetFallMatrix(em);
            if (MotionMoveF(em, 0)) {
                em->r_no_2++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x50), 0, 5, 5, 0);
        em->r_no_2++;
    case 3:
        w->flags |= 0x1000;
        EM2D_WALL_FALL(em, w, d, fl) {
            em2dSetFallMatrix(em);
            MotionMoveF(em, 0);
            if (em2dFallCatchCk(em)) {
                em->pos.y = pPL->pos.y;
                if ((Rnd() & 1) != 0 || (s16) pG->pl_life <= 299) {
                    EmRoutineSetW(em, 1, 0xB, 0, 0);
                } else {
                    EmRoutineSetW(em, 1, 0xC, 0, 0);
                }
            }
        }
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x4E), 0, 5, 5, 0);
        em->r_no_2++;
    case 5:
        w->flags |= 0x1000;
        EM2D_WALL_FALL(em, w, d, fl) {
            em2dSetFallMatrix(em);
            MotionMoveF(em, 0);
            if (em2dFallCatchCk(em)) {
                em->pos.y = pPL->pos.y;
                if ((Rnd() & 1) != 0 || (s16) pG->pl_life <= 299) {
                    EmRoutineSetW(em, 1, 0xB, 0, 0);
                } else {
                    EmRoutineSetW(em, 1, 0xC, 0, 0);
                }
            }
        }
        break;
    case 6:
        em->r_no_2++;
    case 7:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x1A ToAir: the flying type takes off from the floor (flag 0x10), then A_Walk (0x1D).
static void em2d_R1_ToAir(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x37), 5, 1, 0);
        w->turnAng = 0.0f;
        em->r_no_2++;
    case 1:
        if (em->seFlags28B & 8) {
            w->flags |= 0x10;
        }
        MotionMoveF(em, 0);
        if (em->seFlags28B & 1) {
            EmRoutineSet(em, 1, 0x1D, 0, 10);
        }
        break;
    }
}

// R1 == 0x1B ToGround: the flying type lands (flags 0x840 -> 0x40 falling) on the floor below (dies in
// water), then Turn180 / Walk on the ground.
static void em2d_R1_ToGround(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;

    w->flags |= 0x840;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x39), 5, 4, 0);
        w->spd.x = 0.0f;
        w->spd.z = 0.0f;
        w->spd.y = -100.0f;
        em->r_no_2++;
    case 1:
        w->flags |= 0x40;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            if (fl <= -99000.0f) {
                em->be_flag |= 0x10000;
                EmRoutineSet(em, 3, 0, 0, 0);
                break;
            }
            em->r_no_2++;
        }
        MotionMoveF(em, 0);
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x3A), 5, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// Hovering: the position drifts with `spd` (damped 0.8 per frame) plus a sine wobble whose
// amplitude grows towards 1.
// A macro: the 0.95 / 0.05 literals are loaded above the pos.z store (an inline's pool loads
// lose RTX_UNCHANGING_P and sink below it).
#define em2dHoverMove(em, w)                                                          \
    {                                                                                 \
        PSVECScale(&(w)->spd, &(w)->spd, 0.800000012f);                               \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                  \
        (w)->hoverPhase.x += (w)->hoverSpd.x;                                         \
        (em)->pos.x = SINF((w)->hoverPhase.x) * 50.0f * (w)->turnAng + (em)->pos.x;   \
        (w)->hoverPhase.y += (w)->hoverSpd.y;                                         \
        (em)->pos.y = SINF((w)->hoverPhase.y) * 50.0f * (w)->turnAng + (em)->pos.y;   \
        (w)->hoverPhase.z += (w)->hoverSpd.z;                                         \
        (em)->pos.z = SINF((w)->hoverPhase.z) * 50.0f * (w)->turnAng + (em)->pos.z;   \
        (w)->turnAng = (w)->turnAng * 0.949999988f + 0.0500000007f;                   \
    }

// R1 == 0x1C A_Wait: hovers in place (em2dHoverMove sine sway, flags 0x840 | 0x40000 hum) at least
// 2000 above the floor; after each loop may climb (A_Up 0x20) / descend (A_Down 0x21) / A_Step
// (0x1F), and when the player is found attacks (A_Atk 0x23 from far, A_Catch 0x24 near).
static void em2d_R1_A_Wait(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    int hit;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x5D), 0, 5, 5, 0);
        w->hoverPhase.x = fRand1_1() * 3.14159274f;
        w->hoverPhase.y = fRand1_1() * 3.14159274f;
        w->hoverPhase.z = fRand1_1() * 3.14159274f;
        w->hoverSpd.x = fRand0_1() * 0.17453292f + 0.0872664601f;
        w->hoverSpd.y = fRand0_1() * 0.052359879f + 0.0698131695f;
        w->hoverSpd.z = fRand0_1() * 0.052359879f + 0.122173049f;
        w->turnAng = 0.0f;
        em->r_no_2++;
    case 1:
        if (w->flags & 0x200) {
            em2dTurnTo(em, &pPL->pos, 0.0981747732f);
        }
        em2dHoverMove(em, w);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
        }
        if (MotionMoveF(em, 0) && (Rnd() & 3) == 0) {
            if (w->flags & 0x200) {
                if (em->pos.y < pPL->pos.y && em->plDist2 > 9000000.0f && (hit = Rnd() & 3) == 0 &&
                    Em2dGetCeiling(em) > em->pos.y + 4000.0f) {
                    EmRoutineSet(em, 1, 0x20, hit, hit);
                    break;
                }
                int h2;
                if (em->pos.y > pPL->pos.y + 3000.0f && em->plDist2 > 9000000.0f && (h2 = Rnd() & 3) == 0) {
                    EmRoutineSet(em, 1, 0x21, h2, h2);
                    break;
                }
            }
            EmRoutineSet(em, 1, 0x1F, 0, 0);
            break;
        } else {
            em2dFindCk(em);
            if ((w->flags & 0x200) && w->atkWait == 0 && em2dStayCk(em)) {
                if (pG->Game_level <= 1) {
                    if (w->atkWait) {
                        break;
                    }
                    if (Rnd() % 10 > 4) {
                        w->atkWait = 30;
                    }
                }
                if (w->atkWait == 0) {
                    em2dAirNextRtnSet(em);
                }
            }
        }
        break;
    }
    {
    int lock = em2dLockCk(em);
    if (lock) {
        w->lockCnt++;
        if (w->lockCnt > 15) {
            if ((Rnd() & 1) && em->pos.y > SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 2000.0f &&
                em->pos.y > pPL->pos.y + 1000.0f && (Rnd() & 1) && em->plDist2 < 9000000.0f) {
                EmRoutineSet(em, 1, 0x21, 0, 0);
            } else if ((Rnd() & 1) && em->pos.y < pPL->pos.y + 5000.0f && em->plDist2 > 9000000.0f && (Rnd() & 1) &&
                       Em2dGetCeiling(em) > em->pos.y + 4000.0f) {
                EmRoutineSet(em, 1, 0x20, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
            return;
        }
    } else {
        w->lockCnt = lock;
    }
    }
    if (em->plDist2 < 16000000.0f && w->routeAngAbs < 0.34906584f && em->pos.y - pPL->pos.y < 1000.0f) {
        if ((Rnd() & 1) && em->plDist2 > 12250000.0f) {
            EmRoutineSet(em, 1, 0x23, 0, 0);
        } else {
            EmRoutineSet(em, 1, 0x24, 0, 0);
        }
    }
}

// Flying move: the motion speed scaled by `turnAng` (the flight speed here), rotated by
// `jumpAng`, turned towards the target and kept above the floor. A macro: the caller's `spd` / `rot`
// locals are the call arguments (`&rot` lives in a callee-saved register, `&m` is recomputed).
#define EM2D_AIR_MOVE(em, w)                                                              \
    MotionGetSpeed(em, &(em)->Motion, 0, &spd, &rot);                                        \
    PSVECScale(&spd, &spd, (w)->turnAng);                                                 \
    PSMTXRotRad(m, 'y', (w)->jumpAng);                                                    \
    PSMTXMultVecSR(m, &spd, &spd);                                                        \
    MotionAddSpeed(em, &(em)->Motion, &spd, &rot);                                           \
    em2dTurnTo(em, &(w)->targetPos, 0.0981747732f);                                       \
    fl = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                            \
    if ((em)->pos.y < fl) {                                                               \
        (em)->pos.y = fl;                                                                 \
    }

// R1 == 0x1D A_Walk: flies towards the target (EM2D_AIR_MOVE), then em2dAirNextRtnSet picks the next
// air routine.
static void em2d_R1_A_Walk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;
    f32 fl;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x67), 0, 5, 0, em->r_no_3);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        if (em->plDist2 > 100000000.0f) {
            w->turnAng = fRand0_1() * 1.5f + 1.5f;
        } else {
            w->turnAng = fRand0_1() + 0.5f;
        }
        if (em->plDist2 > 36000000.0f) {
            w->jumpAng = fRand1_1() * 0.52359879f;
        } else {
            w->jumpAng = 0.0f;
        }
        em->r_no_2++;
    case 1:
        EM2D_AIR_MOVE(em, w);
        if (MotionMoveF(em, 0)) {
            em2dAirNextRtnSet(em);
        }
        break;
    }
    em2dDoorOpenCk(em);
}

// R1 == 0x1E A_Back: flies backwards away from the player, then em2dAirNextRtnSet.
static void em2d_R1_A_Back(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;
    f32 fl;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x68), 0, 5, 0, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->turnAng = fRand0_1() + 0.5f;
        w->jumpAng = fRand1_1() * 0.52359879f;
        em->r_no_2++;
    case 1:
        EM2D_AIR_MOVE(em, w);
        if (MotionMoveF(em, 0)) {
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// R1 == 0x1F A_Step: a sideways dodge in the air (random side, no damage while dodging), then
// em2dAirNextRtnSet.
static void em2d_R1_A_Step(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;
    f32 fl;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        if (em2dLockCk(em)) {
            if (Rnd() & 1) {
                MotionSetCore(em, &em->Motion, ARC(0x71), 0, 5, 0, 0);
            } else {
                MotionSetCore(em, &em->Motion, ARC(0x71), 0, 5, 0x40, 0);
            }
            em->dmg.m_Timer = 30;
        } else {
            if (Rnd() & 1) {
                MotionSetCore(em, &em->Motion, ARC(0x69), 0, 5, 0, 0);
            } else {
                MotionSetCore(em, &em->Motion, ARC(0x69), 0, 5, 0x40, 0);
            }
        }
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->turnAng = fRand0_1() + 0.5f;
        w->jumpAng = fRand1_1() * 0.52359879f;
        em->r_no_2++;
    case 1:
        EM2D_AIR_MOVE(em, w);
        if (MotionMoveF(em, 0)) {
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// Flying up / down: the motion speed scaled by `turnAng`, turned towards the target.
#define EM2D_AIR_MOVE_UD(em, w)                                                           \
    MotionGetSpeed(em, &(em)->Motion, 0, &spd, &rot);                                        \
    PSVECScale(&spd, &spd, (w)->turnAng);                                                 \
    MotionAddSpeed(em, &(em)->Motion, &spd, &rot);                                           \
    em2dTurnTo(em, &(w)->targetPos, 0.0981747732f);                                       \
    fl = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                            \
    if ((em)->pos.y < fl) {                                                               \
        (em)->pos.y = fl;                                                                 \
    }

// R1 == 0x20 A_Up: climbs (EM2D_AIR_MOVE_UD) for the timer, then em2dAirNextRtnSet.
static void em2d_R1_A_Up(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec spd;
    Vec rot;
    f32 fl;
    f32 ceil;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x6A), 0, 5, 0, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->turnAng = fRand0_1() * 0.5f + 0.5f;
        em->r_no_2++;
    case 1:
        EM2D_AIR_MOVE_UD(em, w);
        ceil = Em2dGetCeiling(em) - 2500.0f;
        if (em->pos.y > ceil) {
            em->pos.y = ceil;
        }
        if (MotionMoveF(em, 0)) {
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// R1 == 0x21 A_Down: descends towards the player's height, then em2dAirNextRtnSet.
static void em2d_R1_A_Down(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec spd;
    Vec rot;
    f32 fl;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x6B), 0, 5, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->turnAng = fRand0_1() * 0.5f + 0.5f;
        em->r_no_2++;
    case 1:
        EM2D_AIR_MOVE_UD(em, w);
        if (MotionMoveF(em, 0)) {
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// R1 == 0x22 A_Turn180: turns around in the air towards the target (turnAng eased), then em2dAirNextRtnSet.
static void em2d_R1_A_Turn180(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 ang;
    f32 fl;
    int flip;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            flip = 1;
        } else {
            flip = 0x41;
        }
        MotionSetCore(em, &em->Motion, ARC(0x6C), 0, 5, flip, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->turnAng = em->ang.y + 3.14159274f;
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        em->r_no_2++;
    case 1:
        ang = Muku(&em->pos, &w->targetPos, w->turnAng, 0.0981747732f);
        w->turnAng += ang;
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        em->ang.y += ang;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
        }
        if (MotionMoveF(em, 0)) {
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// R1 == 0x23 A_Atk: the poison spit from the air (em2dSetPoison type 1 on the spit frame), then
// em2dAirNextRtnSet.
static void em2d_R1_A_Atk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    int blend;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        blend = (Rnd() & 1) ? 0x41 : 1;
        MotionSetCore(em, &em->Motion, ARC(0x76), (int) ARC(0x77), 5, blend, 0);
        w->timer = 15;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        SndCall(8, 0x23, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em2dTurnTo(em, &pPLS->pos, 0.0981747732f);
        }
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
        }
        if (em->seFlags28B & 1) {
            if (em->motFlags & 0x40) {
                em2dAtkCk(em, 0, 0xB);
                em2dAtkCk(em, 0, 0xC);
                em2dAtkCk(em, 0, 0xD);
            } else {
                em2dAtkCk(em, 0, 7);
                em2dAtkCk(em, 0, 8);
                em2dAtkCk(em, 0, 9);
            }
        }
        if (MotionMoveF(em, 0)) {
            w->atkWait = 30;
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// Branch check of A_Catch (0x24): em2dAirCatchCk on the dive -> A_CatchHit (0x26, face grab) or
// A_CatchKick (0x25, knock-down), half / half.
static void em2d_R1_br_A_Catch(cEm2d* em)
{
    if (em->hp > 0 && em2dAirCatchCk(em)) {
        if (Rnd() & 1) {
            EmRoutineSetW(em, 1, 0x25, 0, 0);
        } else {
            EmRoutineSetW(em, 1, 0x26, 0, 0);
        }
    }
}

// R1 == 0x24 A_Catch: the dive at the player from the air; a miss scores an escape and continues
// with em2dAirNextRtnSet.
static void em2d_R1_A_Catch(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;

    w->flags |= 0x840;
    em->setStatus(EM_STATUS_IK_OFF);
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x67), 0, 5, 1, 0);
        SndCall(8, 0x23, &em->pos, em->id, 0, em);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 1:
        em2dTurnTo(em, &pPL->pos, 0.0981747732f);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(LVADD_ESCAPEATTACK);
            w->atkWait = 30;
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// R1 == 0x25 A_CatchKick: the dive knocked the player down (300 damage, plem2d_JumpKickHit), then back
// into the air.
static void em2d_R1_A_CatchKick(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    w->flags |= 0x840;
    em->setStatus(EM_STATUS_IK_OFF);
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        em->pos.y = pPL->pos.y;
        MotionSetCore(em, &em->Motion, ARC(0x65), 0, 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem2d_JumpKickHit, 0.0f, 0.0f, 1500.0f);
        SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        LifeDownSet(pPL, 300, 0);
        em->r_no_3 = (Rnd() & 1) + 4;
        w->timer = 7;
        em->r_no_2++;
    case 1:
        em->setStatus(EM_STATUS_IK_OFF);
        em2dCatchCamMove(em, em);
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 0.3f, 0.2f);
        } else if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            w->atkWait = 30;
            em2dAirNextRtnSet(em);
        }
        break;
    }
}

// R1 == 0x26 A_CatchHit: the flying type on the player's face (plem2d_A_CatchHit): the same bite /
// button-mash / throw-off as JumpAtkHit, flies off (A_Back 0x1E) afterwards or lands into Wait.
static void em2d_R1_A_CatchHit(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec spd;
    Vec rot;
    int fe;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    em->dmg.set(0, 2);
    w->flags |= 0x10000;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x3B), (int) ARC(0x3C), 5, 1, 0);
        PlSetDamageSe(0);
        em->pos.y = pPL->pos.y;
        EmCatchPLSet(em, 0.0f, 2, (int) plem2d_A_CatchHit, -48.1500015f, 0.0f, 921.190002f);
        pPL->r_no_3 = 1;
        GameAddPoint(LVADD_PL_DAMAGE);
        PlGachaInit();
        EstSet((int) em, -1, 0, 0, 0x25, 1, 0, 0, (u32) em, (void*) fe);
        SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        em->setStatus(EM_STATUS_IK_OFF);
        PlGachaMove();
        if (em->seFlags28B & 1) {
            LifeDownSet2(pPL, 500, 0, 1);
            PlSetDamageSe(9);
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f)) {
            if ((u32) PlGachaGet() > 30 && (s16) pG->pl_life > 1) {
                em->r_no_2 = 2;
            } else {
                em->r_no_2 = 6;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x3F), 0, 5, 0, 0);
        w->timer = 127;
        w->timer8 = 10;
        em->r_no_2++;
    case 3:
    case 9:
    step3:
        em->setStatus(EM_STATUS_IK_OFF);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (w->timer8) {
            w->timer8--;
        } else {
            PSVECScale(&spd, &spd, 2.0f);
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            EmRoutineSet(em, 1, 0x1E, 0, 0);
        }
        break;
    case 4:
        em2dSetdLandingEff(em);
        MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 5, 1, 0);
        em->r_no_2++;
    case 5:
        do {  // loop notes: the then-block's w refs count double (w outranks the &em->pos PRE register: r27/r26)
            if (MotionMoveF(em, 0)) {
                em->atari.setPriority(0);
                w->jumpWait = Rnd() % 150 + 150;
                w->atkWait = 75;
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } while (0);
        break;
    case 6:
        MotionSetCore(em, &em->Motion, ARC(0x3D), (int) ARC(0x3E), 5, 0, 0);
        w->timer8 = 45;
        EstSet((int) em, -1, 0, 0, 0x25, 6, 0, 0, (u32) em, 0);
        em->r_no_2++;
    case 7:
        em->setStatus(EM_STATUS_IK_OFF);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (w->timer8) {
            w->timer8--;
        } else {
            PSVECScale(&spd, &spd, 2.0f);
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (em->seFlags28B & 1) {
            LifeDownSet2(pPL, 500, 0, 1);
            PlSetDamageSe(9);
        }
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 8;
        } else if (em->seFlags28B & 4) {
            if ((s16) pG->pl_life <= 1) {
                em->r_no_2 = 10;
                pGS->pl_life = 0;
            } else {
                em->r_no_2 = 8;
            }
        }
        break;
    case 8:
        em->r_no_2++;
        goto step3;
    case 10:
        em->r_no_2++;
    case 11:
        em->setStatus(EM_STATUS_IK_OFF);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (w->timer8) {
            w->timer8--;
        } else {
            PSVECScale(&spd, &spd, 2.0f);
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            EmRoutineSet(em, 1, 0x1E, 0, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
}

// Player damage routine of the flying face grab: like plem2d_JumpAtkHit (struggle, throw-off, kick,
// death with the melted head).
static void plem2d_A_CatchHit(cPlayer* pl)
{
    int fe;

    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    pl->subArc = pl->pEmCatch->subArc;
    fe = pl->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x5F), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.000031f, 400.0f);
        pl->m_Work0 = 127;
        EstSet((int) pl, -1, 0, 0, 0x25, 0x13, 0, 0, (u32) pl, (void*) fe);
        pl->r_no_3 = Rnd() & 1;
        pl->m_Work0 = 10;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
            EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            MotionMoveF(pl, 0);
        }
        em2dCatchCamMove((cEm2d*)pl->pEmCatch, pl);
        if (pPL->pEmCatch->r_no_0 != 1 || pPL->pEmCatch->r_no_1 != 0x26) {
            EndPlDamage();
            pl->dmg.set(0, 30);
            break;
        }
        if (pl->frame > 64.6999969f && pl->frame < 65.3000031f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    case 2:
    case 4:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x63), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x15, 0, 0, (u32) pl, 0);
        pl->r_no_2++;
    case 3:
    case 5:
        em2dCatchCamMove((cEm2d*)pl->pEmCatch, pl);
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        if (pl->frame > 14.6999998f && pl->frame < 15.3000002f) {
            SndCall(8, 0x35, &pl->pos, pl->pEmCatch->id, 0, pl);
        }
        break;
    case 6:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x61), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x14, 0, 0, (u32) pl, 0);
        pl->r_no_2++;
    case 7:
        em2dCatchCamMove((cEm2d*)pl->pEmCatch, pl);
        if (pl->frame > 27.7000008f && pl->frame < 28.2999992f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        MotionMoveF(pl, 0);
        pl->r_no_2 = pl->pEmCatch->r_no_2;
        break;
    case 8:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x62), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x16, 0, 0, (u32) pl, 0);
        pl->r_no_2++;
    case 9:
        em2dCatchCamMove((cEm2d*)pl->pEmCatch, pl);
        if (pl->frame > 14.6999998f && pl->frame < 15.3000002f) {
            SndCall(8, 0x35, &pl->pos, pl->pEmCatch->id, 0, pl);
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 10:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x64), 0, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x25, 0x17, 0, 0, (u32) pl, 0);
        em2dPlHeadMelt(pl);
        pl->r_no_2++;
    case 11:
        em2dDieCamMove((cEm2d*)pl->pEmCatch);
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// R1 == 0x27 C_Wait: hangs under the ceiling (flags 0x180) until the player comes below / runs past,
// then drops (C_Fall 0x28).
static void em2d_R1_C_Wait(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 range;
    int t;

    w->flags |= 0x180;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x47), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        t = em->flag & 1;
        if (t) {
            w->flags |= 0x200;
            EmRoutineSet(em, 1, 0x28, 0, 0);
            break;
        }
        range = 9000000.0f;
        if (em2dPlRunCk(em)) {
            range = 25000000.0f;
        }
        if (em->plDist2 < range) {
            w->flags |= 0x200;
            EmRoutineSet(em, 1, 0x28, t, t);
        }
        break;
    }
}

// R1 == 0x28 C_Fall: drops from the ceiling (flag 0x40, the fall-catch grab half the time / at low hp),
// lands (dies in water), then Turn180 / Walk.
static void em2d_R1_C_Fall(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec d;
    f32 fl;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x4D), 0, 5, 1, 0);
        PSVECScale(&w->wallNrm, &w->spd, 150.0f);
        w->timer = 6;
        SndCall(8, 0x23, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        w->flags |= 0x1000;
        if (MotionMoveF(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x4E), 0, 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = -200.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 3:
        w->flags |= 0x1000;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            if (fl <= -99000.0f) {
                em->be_flag |= 0x10000;
                EmRoutineSet(em, 3, 0, 0, 0);
                break;
            }
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = 1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
            em->ang.y = atan2f(d.x, d.z);
            em2dSetdLandingEff(em);
            MotionSetCore(em, &em->Motion, ARC(0x4F), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 4;
            break;
        }
        em2dSetFallMatrix(em);
        MotionMoveF(em, 0);
        if (em2dFallCatchCk(em)) {
            em->pos.y = pPL->pos.y;
            if ((Rnd() & 1) || (s16) pG->pl_life <= 299) {
                EmRoutineSetW(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSetW(em, 1, 0xC, 0, 0);
            }
        }
        break;
    case 4:
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// R0 == 2: damage (flag bit3), runs Em2d_R1_dm_tbl[r_no_1].
static void em2d_R0_Damage(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    w->flags |= 8;
    Em2d_R1_dm_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Normal: the floor flinch by hit side (front / back, left / right), then SideStep,
// Turn180 or Walk.
static void em2d_R1_Dm_Normal(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int side;
    int hit;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f)) < 1.57079637f) {
            side = 0;
        } else {
            side = 1;
        }
        if (Rnd() & 1) {
            side = 2;
        }
        switch ((u32) side) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x28), (int) ARC(0x29), 5, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x2A), (int) ARC(0x2B), 5, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x51), 0, 5, 1, 0);
            break;
        }
        SndCall(8, 0xF, &em->pos, em->id, 0, em);  // second `&em->pos`: gcse PRE copies the Muku argument pseudo (`mr r28,r30`)
        if (w->poisonTimer <= 10) {
            w->poisonTimer = 10;
        }
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            hit = em2dLockCk(em);
            if (hit) {
                if (em2dToCeilingCk(em)) {
                    break;
                }
                {
                    int air = em2dToAirCk(em);
                    if (air) {
                        break;
                    }
                    EmRoutineSet(em, 1, 4, air, air);
                }
                break;
            }
            if ((Rnd() & 3) == 0) {
                if (em2dToCeilingCk(em)) {
                    break;
                }
                if (em2dToAirCk(em)) {
                    break;
                }
            }
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, hit, hit);
            } else {
                EmRoutineSet(em, 1, 2, hit, 1);
            }
        }
        break;
    }
}

// R0 2 / R1 == 1 Dm_Blow: knocked over (turned to the hit direction, flag 0x4000 = on its back), then
// Die_Down when dead or WakeupWait (0xE).
static void em2d_R1_Dm_Blow(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f)) < 1.57079637f) {
            em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            MotionSetCore(em, &em->Motion, ARC(0x24), (int) ARC(0x25), 5, 1, 0);
        } else {
            em->ang.y += Muku(&em->dmg.m_PosFrom, &em->pos, em->ang.y, 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            MotionSetCore(em, &em->Motion, ARC(0x26), (int) ARC(0x27), 5, 1, 0);
        }
        if (em->hp <= 0) {
            SndCall(8, 9, &em->pos, em->id, 0, em);
        } else {
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        if ((w->flags & 0x80000) && (pG->room_id == 0x205 || pG->room_id == 0x21D)) {
            EstSet((int) em, -1, 0, 0, 1, 0x16, 0, 0, (u32) em, 0);
        }
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 2 Dm_Down: shot while airborne / down: the fall, then Wakeup (0xF).
static void em2d_R1_Dm_Down(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int hit;

    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->Motion, ARC(0x30), (int) ARC(0x31), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x57), (int) ARC(0x58), 5, 1, 0);
        }
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0) && (hit = em2dDownJumpCk(em)) == 0) {
            EmRoutineSet(em, 1, 0xF, hit, hit);
        }
        break;
    }
}

// Fall step of the damage jumps: `spd` added twice around the floor check, 20 per frame of
// gravity. 1 = landed / died, 0 = still falling.
// The fall step of the damage routines, a macro (not an inline: integrate.c drops the
// RTX_UNCHANGING_P flag of an inlined body's constant-pool loads, which then sink below the int
// argument moves). The landing tail (`xFE = NEXT`) and the fall arm live inside it, so the caller
// has no return-value diamond.
#define EM2D_DM_FALL(em, w, v_, fl_, A, B, DOWN, NEXT, END_INC)                                  \
    {                                                                                          \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        (w)->spd.y -= 20.0f;                                                                   \
        fl_ = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                             \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        if ((em)->pos.y < fl_) {                                                               \
            (em)->pos.y = fl_;                                                                 \
            if (fl_ <= -99000.0f) {                                                            \
                (em)->be_flag |= 0x10000;                                                      \
                EmRoutineSet(em, 3, 0, 0, 0);                                                  \
            } else {                                                                           \
                v_.x = 0.0f;                                                                   \
                v_.y = 0.0f;                                                                   \
                v_.z = 1.0f;                                                                   \
                PSMTXMultVecSR((em)->mat, &v_, &v_);                                           \
                (em)->ang.y = atan2f(v_.x, v_.z);                                              \
                MotionSetCore(em, &(em)->Motion, ARC(A), (int) ARC(B), 5, 1, 0);                  \
                MotionMoveF(em, 0);                                                            \
                if (DOWN) {                                                                    \
                    em2dSetDownEff(em);                                                        \
                } else {                                                                       \
                    em2dSetdLandingEff(em);                                                    \
                }                                                                              \
                (em)->r_no_2 = NEXT;                                                              \
            }                                                                                  \
        } else {                                                                               \
            em2dSetFallMatrix(em);                                                             \
            if (END_INC) {                                                                     \
                if (MotionMoveF(em, 0)) {                                                      \
                    (em)->r_no_2++;                                                               \
                }                                                                              \
            } else {                                                                           \
                MotionMoveF(em, 0);                                                            \
            }                                                                                  \
        }                                                                                      \
    }

// R0 2 / R1 == 3 Dm_Jump: shot out of a jump (EM2D_DM_FALL: falls to the floor, dies in water), lands
// on its back, then WakeupWait or Die_Down.
static void em2d_R1_Dm_Jump(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    Vec d;
    Vec ofs;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        em->ang.y = GetXZAngle(&em->pos, &em->dmg.m_PosFrom);
        ofs.x = 0.0f;
        ofs.y = -1028.31995f;
        ofs.z = 0.0f;
        PSMTXMultVec(em->mat, &ofs, &em->pos);
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = -100.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        MotionSetCore(em, &em->Motion, ARC(0x53), 0, 0, 5, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        EM2D_DM_FALL(em, w, d, fl, 0x55, 0x56, 0, 4, 1);
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x54), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        EM2D_DM_FALL(em, w, d, fl, 0x55, 0x56, 1, 4, 0);
        break;
    case 4:
        em->r_no_2++;
    case 5:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 2, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 4 Dm_Wall: shot off the wall (after 200+ damage): falls to the floor on its back, then
// WakeupWait or Die_Down.
static void em2d_R1_Dm_Wall(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    Vec ofs;
    Mtx m;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        PSVECScale(&w->wallNrm, &ofs, 700.0f);
        PSVECAdd(&em->pos, &ofs, &em->pos);
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'x', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->Motion, ARC(0x54), 0, 0, 5, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        EM2D_DM_FALL(em, w, ofs, fl, 0x55, 0x56, 1, 2, 0);
        break;
    case 2:
        em->r_no_2++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 2, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 5 Dm_Air: the flying type's flinch (turned to the hit direction), then A_Step (0x1F).
static void em2d_R1_Dm_Air(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    int flip;

    w->flags |= 0x840;
    w->flags |= 0x40000;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        flip = 0x41;
        if (Rnd() & 1) {
            flip = 1;
        }
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f)) < 1.57079637f) {
            em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            MotionSetCore(em, &em->Motion, ARC(0x72), 0, 5, flip, 0);
        } else {
            em->ang.y += Muku(&em->dmg.m_PosFrom, &em->pos, em->ang.y, 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            MotionSetCore(em, &em->Motion, ARC(0x73), 0, 5, flip, 0);
        }
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x1F, 0, 0);
        }
        break;
    }
}

// R0 2 / R1 == 6 Dm_Ceiling: shot off the ceiling: drops to the floor (dies in water), lands on its
// back, then WakeupWait or Die_Down.
static void em2d_R1_Dm_Ceiling(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec d;
    cModel* p;
    f32 fl;
    int end;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x52), 0, 0, 5, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        end = MotionMoveF(em, 0);
        em->partsWorldCalc();
        p = em->getPartsPtr(0);
        fl = SatMgr.getFloor(&p->world, 600.0f, 100000.0f, 0, 0);
        if (p->world.y < fl) {
            em->pos.y = fl;
            if (fl <= -99000.0f) {
                em->be_flag |= 0x10000;
                EmRoutineSet(em, 3, 0, 0, 0);
                break;
            }
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = 1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
            em->ang.y = atan2f(d.x, d.z);
            MotionSetCore(em, &em->Motion, ARC(0x55), (int) ARC(0x56), 5, 1, 0);
            MotionMoveF(em, 0);
            em2dSetdLandingEff(em);
            em->r_no_2 = 4;
        } else if (end) {
            em->r_no_2++;
        }
        break;
    case 2:
        w->spd.x = 0.0f;
        w->spd.y = -200.0f;
        w->spd.z = 0.0f;
        em->pos.y = em->getPartsPtr(0)->world.y - 250.0f;
        MotionSetCore(em, &em->Motion, ARC(0x54), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        EM2D_DM_FALL(em, w, d, fl, 0x55, 0x56, 1, 4, 0);
        break;
    case 4:
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 2, 0, 0);
            }
        }
        break;
    }
}

// R0 == 3: death (flag bit3), runs Em2d_R1_die_tbl[r_no_1].
static void em2d_R0_Die(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    w->flags |= 8;
    Em2d_R1_die_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Lost: the corpse: item drop (ITEMSET, inactive), dissolves (em2dScaleCompress +
// invisible_factor), then hidden (be_flag 0x4000) with Reset_enable for the room.
static void em2d_R1_Die_Lost(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int fe = em->r_no_2;

    switch (fe) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->hp = fe;
        EmSetDie(em);
        EmReserveDropItem(em);
        EmSetDieCntE(em);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        SndCall(8, 0xE, &em->pos, em->id, 0, em);
        if (ChkWaterEffectEnable(&em->pos)) {
            EstSet((int) em, -1, 0, 0, 0x25, 0x21, 0, 0, (u32) em, (void*) fe);
        } else {
            EstSet((int) em, -1, 0, 0, 0x25, 0x2A, 0, 0, (u32) em, 0);
        }
        w->timer = 30;
        w->timer8 = 150;
        w->Compress_y = 1.0f;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->Compress_y -= 0.00999999978f;
            if (w->Compress_y < 0.100000001f) {
                w->Compress_y = 0.100000001f;
            }
            em->pos.y -= 3.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->timer8) {
            w->timer8--;
        } else {
            em->invisible_factor -= 0.100000001f;
            if (em->invisible_factor <= 0.0f) {
                em->invisible_factor = 0.0f;
                em->be_flag &= ~2;
                w->Reset_enable = 1;
                em->be_flag |= 0x4000;
                em->r_no_2++;
            }
        }
        break;
    }
}

// R0 3 / R1 == 1 Die_Normal: dies standing on the floor (death motion, floor snap), then Die_Lost.
static void em2d_R1_Die_Normal(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x32), (int) ARC(0x33), 5, 1, 0);
        SndCall(8, 9, &em->pos, em->id, 0, em);
        w->turnAng = -15.0f;
        em->r_no_2++;
    case 1:
        em->pos.y += w->turnAng;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            w->turnAng = 0.0f;
        }
        w->turnAng -= 15.0f;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 2 Die_Down: dies on its back (the twitch), then Die_Lost.
static void em2d_R1_Die_Down(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;

    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->Motion, ARC(0x2E), (int) ARC(0x2F), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x59), (int) ARC(0x5A), 5, 1, 0);
        }
        SndCall(8, 9, &em->pos, em->id, 0, em);
        w->turnAng = -15.0f;
        em->r_no_2++;
    case 1:
        em->pos.y += w->turnAng;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            w->turnAng = 0.0f;
        }
        w->turnAng -= 15.0f;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

// Fall step of the die routines (no death below the floor). 1 = landed, 0 = still falling.
// Same for the die routines (no bottomless-pit check).
#define EM2D_DIE_FALL(em, w, v_, fl_, DOWN, NEXT, END_INC)                                       \
    {                                                                                          \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        (w)->spd.y -= 20.0f;                                                                   \
        fl_ = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                             \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        if ((em)->pos.y < fl_) {                                                               \
            (em)->pos.y = fl_;                                                                 \
            v_.x = 0.0f;                                                                       \
            v_.y = 0.0f;                                                                       \
            v_.z = 1.0f;                                                                       \
            PSMTXMultVecSR((em)->mat, &v_, &v_);                                               \
            (em)->ang.y = atan2f(v_.x, v_.z);                                                  \
            MotionSetCore(em, &(em)->Motion, ARC(0x55), (int) ARC(0x56), 5, 1, 0);               \
            MotionMoveF(em, 0);                                                                \
            if (DOWN) {                                                                        \
                em2dSetDownEff(em);                                                            \
            } else {                                                                           \
                em2dSetdLandingEff(em);                                                        \
            }                                                                                  \
            (em)->r_no_2 = NEXT;                                                                  \
        } else {                                                                               \
            em2dSetFallMatrix(em);                                                             \
            if (END_INC) {                                                                     \
                if (MotionMoveF(em, 0)) {                                                      \
                    (em)->r_no_2++;                                                               \
                }                                                                              \
            } else {                                                                           \
                MotionMoveF(em, 0);                                                            \
            }                                                                                  \
        }                                                                                      \
    }

// R0 3 / R1 == 3 Die_Wall: dies on the wall: drops to the floor (EM2D_DIE_FALL), then Die_Down.
static void em2d_R1_Die_Wall(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    Vec ofs;
    Mtx m;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        PSVECScale(&w->wallNrm, &ofs, 700.0f);
        PSVECAdd(&em->pos, &ofs, &em->pos);
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'x', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->Motion, ARC(0x54), 0, 0, 5, 0);
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        EM2D_DIE_FALL(em, w, ofs, fl, 0, 2, 0);
        break;
    case 2:
        em->r_no_2++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 2, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 4 Die_Air: the flying type dies in the air: tumbles to the floor, then Die_Down.
static void em2d_R1_Die_Air(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    Vec d;

    w->flags |= 0x840;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        em->ang.y = GetXZAngle(&em->pos, &em->dmg.m_PosFrom);
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        w->spd.x = 0.0f;
        w->spd.z = -100.0f;
        w->spd.y = 0.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        MotionSetCore(em, &em->Motion, ARC(0x53), 0, 0, 5, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        EM2D_DIE_FALL(em, w, d, fl, 0, 4, 1);
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x54), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        EM2D_DIE_FALL(em, w, d, fl, 1, 4, 0);
        break;
    case 4:
        em->r_no_2++;
    case 5:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 2, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 5 Die_Ceiling: dies on the ceiling: drops to the floor, then Die_Down.
static void em2d_R1_Die_Ceiling(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 fl;
    Vec d;
    Mtx m;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        w->spd.x = 0.0f;
        w->spd.z = 0.0f;
        w->spd.y = -200.0f;
        em->pos.y -= 1000.0f;
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'y', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->Motion, ARC(0x54), 0, 0, 5, 0);
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        EM2D_DIE_FALL(em, w, d, fl, 0, 2, 0);
        break;
    case 2:
        em->r_no_2++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 2, 0, 0);
        }
        break;
    }
}

// Per frame: the route point / angle to the player (routePos / routeAng; the flying type aims
// straight at him), the direction to his head (plDir), line of sight (flag bit0), plDist / homeDist,
// and the target (the home position when returning, flag 0x8000, else the player).
void em2dRouteCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec plPos;
    Vec a;
    Vec b;

    if (em->hp <= 0) {
        return;
    }
    if (em->r_no_0 != 0) {
        if (em->set == 1) {
            if (pG->Frame_cnt % 10 != em->emset_no % 10) {
                return;
            }
        } else if ((pG->Frame_cnt & 3) != (em->emset_no & 3)) {
            return;
        }
    }
    // Both arms carry the Muku/fabs/xFC-reset tail (jump2 cross-jumps it from `lis` on; the argument addresses
    // stay per arm). The xFC reset must be inside the arms too: sched2 runs before jump2, so the surviving arm's
    // block has to end in the xFC `bne`, not at a join label, for `stfs routeAngAbs` to be issued before the PRE copies.
    if (w->flags & 0x800) {
        GetPlPos(&w->routePos, 0, 10.0f);
        w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, 3.14159274f);
        w->routeAngAbs = fabsf(w->routeAng);
        if (em->r_no_0 == 0) {
            w->routeAng = 0.0f;
            w->routeAngAbs = 0.0f;
            em->plDist2 = 100000000.0f;
        }
    } else {
        RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0);
        w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, 3.14159274f);
        w->routeAngAbs = fabsf(w->routeAng);
        if (em->r_no_0 == 0) {
            w->routeAng = 0.0f;
            w->routeAngAbs = 0.0f;
            em->plDist2 = 100000000.0f;
        }
    }
    plPos = pPL->pos;
    plPos.y += 1800.0f;
    w->plDir = em2dGetPlDir(em, &plPos);
    w->plDirAbs = fabsf(w->plDir);
    w->flags &= ~1;
    if (em->r_no_0 != 0) {
        a = em->getPartsPtr(0)->world;
        b.x = pPLS->pos.x;
        b.y = pPLS->pos.y + 1500.0f;
        b.z = pPLS->pos.z;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            w->flags |= 1;
        }
    }
    if (em->set == 1) {
        w->plDist = SQRTF((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) +
                          (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y) +
                          (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z));
        w->homeDist = SQRTF((w->homePos.x - pPLS->pos.x) * (w->homePos.x - pPLS->pos.x) +
                            (w->homePos.y - pPLS->pos.y) * (w->homePos.y - pPLS->pos.y) +
                            (w->homePos.z - pPLS->pos.z) * (w->homePos.z - pPLS->pos.z));
    } else {
        w->plDist = RouteCkPosToPosDis(&em->pos, &pPL->pos);
        w->homeDist = RouteCkPosToPosDis(&w->homePos, &pPLS->pos);
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;  // before the flags RMW: the pPL load (may alias a w store) precedes `stw flags`
    w->flags &= ~4;
    if (w->flags & 0x8000) {
        if (w->homeDist < em->Guard_r) {
            w->flags &= ~0x8000;
        }
    } else if (w->homeDist > em->Guard_r + 10000.0f) {
        w->flags |= 0x8000;
    }
    em2dReturnPosCk(em);
    if (w->flags & 0x8000) {
        RouteCkToPos(em, &w->homePos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->ang.y, 3.14159274f);
        w->targetAngAbs = fabsf(w->targetAng);
        if (em->r_no_0 == 0) {
            w->targetAng = 0.0f;
            w->targetAngAbs = 0.0f;
        }
    }
    if (DbgFlagChk(pG, DBG_RTP_DISP)) {
        a = em->pos;
        a.y += 250.0f;
        Draw_line3d(&a, &w->targetPos, 0xFFFFFF40, 0);
        Draw_line3d(&a, &w->routePos, 0xFF0000FF, 0);
    }
}

// Damage of the weapon hit: GetWepDmVal (`near` for a muzzle within 6000), x1.25 on the head part 6
// (a rifle head shot kills outright), x18 / x25 on the flying type in the air (one shot downs it).
int em2dSetDmVal(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    YARARE_INFO* part = em->dmg.m_pDamageYarare;
    int near = 0;
    int dmg;

    if (part->rad < 36000000.0f) {
        near = 1;
    }
    dmg = 100;
    if (em->dmg.m_Wep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    if (part->partsNo == 6) {
        dmg = (int) ((f32) dmg * 1.25f);
        switch (em->dmg.m_Wep) {  // range node: `cmpwi 0xa; bgt; cmpwi 9; blt`
        case 9:
        case 0xA:
            dmg = 9999;
            break;
        }
    }
    if (em->type == 4 && (w->flags & 0x800)) {
        if (Rnd() & 1) {
            dmg *= 18;
        } else {
            dmg *= 25;
        }
    }
    return dmg;
}

// Attack hit test at part `parts`: kind `no` 0 / 1 the slashes (blood, a kill takes the player's
// head), 2 the critical bite (kills outright with plem2d_CriticalHit); the partner is hurt too;
// once per attack (atkHit). 1 = hit.
int em2dAtkCk(cEm2d* em, int no, int parts)
{
    Em2dWork* w = EM2D_WK(em);
    EmAtkInfo* atk;
    cModel* p;
    int hit;

    if (w->atkHit) {
        return 0;
    }
    atk = &em2d_atk_info[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(atk, &p->world, &p->world_old, 0);
    if (hit) {
    if (hit & 1) {
        switch ((u32) no) {
        case 0:
            if (em->motFlags & 0x40) {
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x25, 0xE);
            } else {
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x25, 0x1B);
            }
            if ((s16) pG->pl_life <= 0) {
                em2dPlHeadLost();
            }
            break;
        case 1:
            if (em->motFlags & 0x40) {
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x25, 0xD);
            } else {
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x25, 0x1C);
            }
            if ((s16) pG->pl_life <= 0) {
                em2dPlHeadLost();
            }
            break;
        case 2:
            memcpy((u8*) pPL + 0x328, &em->pos, sizeof(Vec));  // byte-pointer destination: pPL is reloaded for EstSet
            EstSet((int) pPL, -1, 0, 0, 0x25, 0x26, 0, 0, (u32) pPL, 0);
            pG->pl_life = 0;
            SetPlDamage(em, plem2d_CriticalHit);
            break;
        }
        w->atkHit = 1;
    }
    if (hit & 2) {
        EmSubBloodSet(em, &p->world, 1, 0xFF, 0xFF);
        w->atkHit = 1;
    }
    QuakeExec(0, 0, 5, 22.0f, 2);
    SndCall(8, 0xD, &em->pos, em->id, 0, em);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    return 1;
    }
    return 0;
}

// Squashes the parts vertically by Compress_y (Die_Lost sinks the corpse into the floor).
void em2dScaleCompress(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec scale;
    cModel* p;

    if (em->r_no_0 != 3) {
        return;
    }
    if (em->r_no_1 != 0) {
        return;
    }
    PSMTXIdentity(m);
    scale.x = 1.0f;
    scale.y = w->Compress_y;
    scale.z = 1.0f;
    ScaleMatrix(m, &scale);
    for (p = em->pParts; p; p = p->pParts) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->world.x;
        p->mat[1][3] = p->world.y;
        p->mat[2][3] = p->world.z;
    }
}

// Is the player aiming a gun (ammo, not the knife) at this insect within range and in front: the root
// lies inside the box in front of the weapon hand. The air routines dodge when it is.
int em2dLockCk(cEm2d* em)
{
    Mtx inv;
    Vec pos;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 6) {
        return 0;
    }
    if (em->plDist2 > 64000000.0f) {
        return 0;
    }
    if (pG->weapon_no == 0x10) {
        return 0;
    }
    if (pG->Game_level <= 3) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f)) > 0.785398185f) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(10)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->world, &pos);
    if (pos.x > 0.0f) {
        return 0;
    }
    if (pos.z > 500.0f || pos.z < -500.0f) {
        return 0;
    }
    if (pos.y > 500.0f) {
        return 0;
    }
    if (pos.y < -500.0f) {
        return 0;
    }
    return 1;
}

// Snaps the insect onto the surface under it (probe along wallNrm) and rotates the model matrix so
// its up axis matches the surface normal.
void em2dSetWallMatrix(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec nrm;
    Vec hit;
    Vec a;
    Vec b;
    Vec up;
    Vec axis;
    Mtx m;
    f32 ang;

    PSVECScale(&w->wallNrm, &a, 500.0f);
    PSVECScale(&w->wallNrm, &b, -2000.0f);
    PSVECAdd(&em->pos, &a, &a);
    PSVECAdd(&em->pos, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x383830)) {
        em->pos = hit;
        w->wallNrm = nrm;
    }
    em->motFlags2 |= 0x40000000;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 7744 "D:/Bio4/Prog/em2d.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &axis);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &axis, ang * 0.300000012f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}

// Wall walk step: probes the surface around the feet (four probes), blends wallNrm towards the found
// normal by `rate`, snaps to the surface and rebuilds the matrix; 0 when the surface was lost.
int em2dSetWallMatrix2(cEm2d* em, f32 rate)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx m;
    Vec nrm;
    Vec n0;
    Vec n1;
    Vec n2;
    Vec n3;
    Vec hit;
    Vec a;
    Vec b;
    Vec up;
    Vec sum;
    f32 ang;

    PSMTXCopy(em->mat, m);
    TransMatrix(m, &em->pos);
    a.x = 0.0f;
    a.y = 800.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -600.0f;
    b.z = 1400.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n0.x = 0.0f;
    n0.y = 0.0f;
    n0.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n0 = nrm;
    }
    a.x = 0.0f;
    a.y = 800.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -600.0f;
    b.z = -1400.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n1.x = 0.0f;
    n1.y = 0.0f;
    n1.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n1 = nrm;
    }
    b.x = -1400.0f;
    a.x = 0.0f;
    a.y = 800.0f;
    a.z = 0.0f;
    b.y = -600.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n2.x = 0.0f;
    n2.y = 0.0f;
    n2.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n2 = nrm;
    }
    a.y = 800.0f;
    b.x = 1400.0f;
    b.y = -600.0f;
    a.x = 0.0f;
    a.z = 0.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n3.x = 0.0f;
    n3.y = 0.0f;
    n3.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n3 = nrm;
    }
    PSVECAdd(&n0, &n1, &sum);
    PSVECAdd(&n2, &sum, &sum);
    PSVECAdd(&n3, &sum, &sum);
    if (sum.x == 0.0f && sum.y == 0.0f && sum.z == 0.0f) {
        sum.y = 1.0f;
    }
#line 7866 "D:/Bio4/Prog/em2d.cpp"
    VECNormalize(&sum, &w->wallNrm);
    em->motFlags2 |= 0x40000000;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 7878 "D:/Bio4/Prog/em2d.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &sum);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &sum, ang * rate);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    a.x = 0.0f;
    a.y = 700.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -700.0f;
    b.z = 0.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x383830)) {
        em->pos = hit;
        sum.y = sinf(acosf(PSVECDotProduct(&nrm, &w->wallNrm))) * 700.0f;
        sum.x = 0.0f;
        sum.z = 0.0f;
        PSMTXMultVecSR(em->mat, &sum, &sum);
        PSVECAdd(&em->pos, &sum, &em->pos);
        TransMatrix(em->mat, &em->pos);
        return 1;
    }
    return 0;
}

// While falling off a wall: eases wallNrm back to straight up so the insect lands feet first.
void em2dSetFallMatrix(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec up;
    Vec axis;
    Mtx m;
    f32 ang;

    w->wallNrm.x = 0.0f;
    w->wallNrm.y = 1.0f;
    w->wallNrm.z = 0.0f;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    em->motFlags2 |= 0x40000000;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 7956 "D:/Bio4/Prog/em2d.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &axis);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &axis, ang * 0.200000003f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}

// The colour byte select is a 2-byte struct local (HImode pseudo, not promoted): the constant arms
// are SImode `li r0,0xff`/`li r0,0x80`, the `+ 0x18` arms keep the byte register (no `clrlwi`), and
// jump.c never hoists an arm (the HI dest never equals the SI add's dest).
struct Em2dColSel {
    u16 v;
};

// The camouflage: fades the model in (visible while attacking / damaged / near the player, flag
// 0x200) and out (blendRatio towards the colour-select table), with the shimmer effect every
// effTimer frames; the room's "show all" flag (Status_flg[1] bit26) forces it visible.
void em2dCamouflageMove(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    cModelInfo* info;
    cModelInfo* p;
    int on = 0;
    f32 f;

    if (em->type == 4) {
        return;
    }
    if (w->poisonTimer) {
        w->poisonTimer--;
        w->flags &= ~0x100;
    }
    if ((s16) pG->pl_life <= 0) {
        w->flags &= ~0x100;
    }
    if (em->hp <= 0) {
        if (w->blendRatio > 0x18) {
            w->blendRatio -= 0x18;
        } else {
            w->blendRatio = on;
        }
    } else if (w->flags & 0x100) {
        if (w->blendRatio <= 0xF2) {
            w->blendRatio += 0xC;
            on = 1;
        } else {
            w->blendRatio = 0xFF;
        }
    } else {
        if (w->blendRatio > 0x10) {
            w->blendRatio -= 0x10;
        } else {
            w->blendRatio = on;
        }
    }
    if (em->flag & 0x80000000) {
        return;
    }
    if (w->blendRatio == 0xFF) {
        if (em->Refract_ratio > 0x10) {
            em->Refract_ratio -= 0x10;
            on = 1;
        } else {
            em->Refract_ratio = 0;
        }
    } else {
        if (em->Refract_ratio <= 0xDE) {
            em->Refract_ratio += 0x20;
            on = 1;
        } else {
            em->Refract_ratio = 0xFF;
        }
    }
    em->Refract_pow = em2d_tex_flag;
    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        w->x4D0 = 0;
        em->Refract_ratio = 0xFF;
    }
    if (em->Refract_ratio == 0) {
        if (w->humTimer) {
            w->humTimer--;
        } else {
            w->humTimer = Rnd() % 150 + 150;
            w->x4D0 = 20;
        }
    } else {
        w->x4D0 = 0;
    }
    if (w->x4D0) {
        w->x4D0--;
        on = 1;
    }
    if (em->Refract_ratio == 0xFF) {
        em->Shader_type = 0;
    } else {
        em->Shader_type = 1;
    }
    info = em->pModelInfo;
    if (info) {
        if (w->blendRatio == 0xFF) {
            // one routine-scope `f` set in both arms: the fade value is a global pseudo (f12) and the
            // two pool constants are local-allocated first (12.8/3.2 f0, 0.9 f13)
            if (w->x4D0) {
                f = (f32) info->color[3];
                f = f * 0.899999976f + 12.8000002f;
                info->color[3] = (u8) f;
            } else {
                f = (f32) info->color[3];
                f = f * 0.899999976f + 3.20000005f;
                info->color[3] = (u8) f;
            }
        } else {
            if (info->color[3] <= 0xE6) {
                info->color[3] += 0x18;
            } else {
                info->color[3] = 0xFF;
            }
        }
    }
    if (info->color[3] == 0xFF) {
        em->ot_type = 0;
    } else {
        em->ot_type = 5;
    }
    if (on) {
        Em2dColSel sel;
        p = em->pModelInfo;
        if (p == 0) {
            return;
        }
        if (w->blendRatio == 0) {
            if (p->color2[0] <= 0xE6) {
                sel.v = p->color2[0] + 0x18;
            } else {
                sel.v = 0xFF;
            }
            p->color2[0] = sel.v;
        } else {
            // the store sits between this arm's load and the `> 0x98` compare, so combine cannot
            // prove the byte's upper bits zero there (`clrlwi r0,r9,24; cmplwi r0,0x98`)
            if (!(p->color2[0] & 0x80)) {
                if (p->color2[0] > 0x67) {
                    goto color2_80;
                }
                sel.v = p->color2[0] + 0x18;
            color2_store:
                p->color2[0] = sel.v;
            } else if (p->color2[0] > 0x98) {
                p->color2[0] -= 0x18;
            } else {
            color2_80:
                sel.v = 0x80;
                goto color2_store;
            }
        }
    } else {
        p = em->pModelInfo;
        if (p == 0) {
            return;
        }
        if (p->color2[0] > 0x18) {
            p->color2[0] -= 0x18;
        } else {
            p->color2[0] = on;
        }
    }
    p->color2[1] = p->color2[0];
    p->color2[2] = p->color2[0];
    p->color2[3] = p->color2[0];
}

// Catch test of the leap: the player alive, not held, inside the box in front and reachable. 1 = caught.
int em2dCatchCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec pos;

    if (em2dDeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (!(em->seFlags28B & 2)) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED)) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &pos);
    if (pos.y < -500.0f || pos.y > 500.0f) {
        return 0;
    }
    if (pos.z < 0.0f || pos.z > 1500.0f) {
        return 0;
    }
    if (!(pos.x > -600.0f) || !(pos.x < 600.0f)) {
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    return 1;
}

// Catch test of the dive from the air: like em2dCatchCk with the dive box.
int em2dAirCatchCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Mtx inv;
    Vec pos;

    if (em2dDeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED)) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &pos);
    if (pos.y < -1500.0f || pos.y > 500.0f) {
        return 0;
    }
    if (pos.z < 0.0f || pos.z > 1500.0f) {
        return 0;
    }
    if (!(pos.x > -600.0f) || !(pos.x < 600.0f)) {
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    return 1;
}

// Catch test of the drop from the wall / ceiling: the player right below (within 200 below .. 2000
// above), damage held. 1 = caught.
int em2dFallCatchCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    cPlayer* pl = pPL;
    cDmgInfo* dm = &pl->dmg;
    f32 dy;

    if (em2dDeadCk(pl)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (StaFlagChk(pG, STA_PL_CATCHED)) {
        return 0;
    }
    if (em->plDist2 > 2250000.0f) {
        return 0;
    }
    dy = em->pos.y - pl->pos.y;
    if (!(dy > 2000.0f) && !(dy < -200.0f)) {  // every `return 0` shares the final `li r3,0`
        dm->set(0, 2);
        em->dmg.set(0, 2);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        return 1;
    }
    return 0;
}

// Cut-in camera of the face grab: eases the work Camera (cam) to viewpoint `mode` (0..5 around the
// player) by `rate`, pulled in front of walls; installs it as the extra camera. Returns 0 when the
// view is blocked (the caller falls back to the game camera).
int em2dCamMove(cEm2d* em, int mode, f32 rate)
{
    Em2dWork* w = EM2D_WK(em);
    Camera* cam = &pG->Cam;
    Vec pos;
    Vec at;
    Vec hit;
    Vec d;
    int blocked = 0;
    f32 len;

    switch ((u32) mode) {
    case 0:
    default:
        pos.x = -1254.0f;
        pos.y = 487.0f;
        pos.z = -1701.0f;
        at.x = -219.0f;
        at.y = 1384.0f;
        at.z = -22.0f;
        break;
    case 1:
        pos.x = 1444.0f;
        pos.y = 417.0f;
        pos.z = -1828.0f;
        at.x = -186.0f;
        at.y = 1290.0f;
        at.z = -22.0f;
        break;
    case 2:
        pos.x = 1254.0f;
        pos.y = 487.0f;
        pos.z = 1701.0f;
        at.x = 219.0f;
        at.y = 1384.0f;
        at.z = 22.0f;
        break;
    case 3:
        pos.x = -1444.0f;
        pos.y = 417.0f;
        pos.z = 1828.0f;
        at.x = 186.0f;
        at.y = 1290.0f;
        at.z = 22.0f;
        break;
    case 4:
        pos.x = -1254.0f;
        pos.y = 987.0f;
        pos.z = -1701.0f;
        at.x = -219.0f;
        at.y = 1884.0f;
        at.z = -22.0f;
        break;
    case 5:
        pos.x = 1444.0f;
        pos.y = 917.0f;
        pos.z = -1828.0f;
        at.x = -186.0f;
        at.y = 1790.0f;
        at.z = -22.0f;
        break;
    case 6:
        pos.x = 1254.0f;
        pos.y = 987.0f;
        pos.z = 1701.0f;
        at.x = 219.0f;
        at.y = 1884.0f;
        at.z = 22.0f;
        break;
    case 7:
        pos.x = -1444.0f;
        pos.y = 917.0f;
        pos.z = 1828.0f;
        at.x = 186.0f;
        at.y = 1790.0f;
        at.z = 22.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &pos, &pos);
    PSMTXMultVec(pPL->mat, &at, &at);
    PosToPos(&cam->param.at, &at, &w->cam.param.at, rate);
    PosToPos(&cam->param.pos, &pos, &w->cam.param.pos, rate);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0, 0)) {
        PSVECSubtract(&hit, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 8382 "D:/Bio4/Prog/em2d.cpp"
        VECNormalize(&d, &d);
        blocked = 1;
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    w->cam.param.fovy = 50.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.m_pExtraCamera = (s32) &w->cam;
    return blocked ^ 1;
}

// Camera of the player's death by the insect: looks at his head from the fixed offset.
void em2dDieCamMove(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    GlobalWork* g = pG;
    Vec pos;
    Vec at;

    pos.x = -354.600006f;
    pos.y = 452.700012f;
    pos.z = -670.0f;
    PSMTXMultVec(pPL->mat, &pos, &pos);
    at = pPL->getPartsPtr(4)->world;
    PosToPos(&g->Cam.param.at, &at, &w->cam.param.at, 0.300000012f);
    PosToPos(&g->Cam.param.pos, &pos, &w->cam.param.pos, 0.300000012f);
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    w->cam.param.fovy = 50.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.m_pExtraCamera = (s32) &w->cam;
}

// 1 when this insect may approach: fewer than three visible ones are already nearer to the player, or
// it is farther than 8000 units.
int em2dStayCk(cEm2d* em)
{
    u32 cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x2D && e->hp > 0 && e != em && e->checkStatus(EM_STATUS_ACTIVE) &&
            (EM2D_WK(e)->flags & 0x200) && e->plDist2 < em->plDist2) {
            cnt++;
        }
    }
    return cnt > 2 ? em->plDist2 > 64000000.0f : 1;
}

// Registers a kind 3 crash volume of radius `r` at the insect (a falling one knocks the others over).
void em2dSetCrash(cEm2d* em, f32 r)
{
    DmgMgr.set(3, 2, &em->pos, 1500.0f, r);
}

// Hit by a kind 3 crash volume while standing on the floor -> Dm_Blow (R2 1). 1 = crashed.
int em2dCrashCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec out;
    int zero;

    if (em2dDeadCk(em)) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (w->flags & 8) {
        return 0;
    }
    if (w->flags & 0x20) {
        return 0;
    }
    if (w->flags & 0x40) {
        return 0;
    }
    zero = w->flags & 0x80;
    if (zero) {
        return 0;
    }
    if (DmgMgr.hitCheck(&em->pos, &out) != 3) {
        return 0;
    }
    EmRoutineSet(em, 2, 1, zero, zero);
    return 1;
}

// The player's head comes off (the critical bite): in the overseas versions hides the head model and
// spawns it as a cObj01 with the blood effect; the Japanese version only plays the blood / SE.
void em2dPlHeadLost()
{
    Vec ofs;
    Vec spd;
    Vec rot;
    cModel* p3;
    cObj* obj;
    int zero;

    if (pSys->eff_country == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x25, 0x2C, 0, 0, (u32) pPL, 0);
        return;
    }
    zero = 0;
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet((int) pPL, -1, 0, 0, 0x25, 0x2D, 0, 0, (u32) pPL, (void*) zero);
    pPL->setHead(0);
    p3 = pPL->getPartsPtr(3);
    ofs.x = 0.0f;
    ofs.y = 68.0f;
    ofs.z = 28.0f;
    spd.x = 0.0f;
    spd.y = 80.0f;
    spd.z = -50.0f;
    rot = pPL->ang;
    rot.y += 3.14159274f;
    rot.y = LIMIT_ANGLE(rot.y);
    PSMTXMultVec(p3->mat, &ofs, &ofs);
    PSMTXMultVecSR(pPL->mat, &spd, &spd);
    obj = SetObj01(PL_ARC_PTR(pG->pPlayer, 0xC), PL_ARC_PTR(pG->pPlayer, 7), &ofs, &rot, &spd, 15.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->LightInfo.EnableMask = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, (int) zero, -1);
    }
    EstSet((int) obj, -1, 0, 0, 0x25, 0x2E, 0, 0, (u32) obj, (void*) zero);
}

// Swaps the player's head for the acid-melted skull (player archive 0x6D / 0x6E) after the face grab kill.
void em2dPlHeadMelt(cPlayer* pl)
{
    if (pSys->eff_country) {
        pPL->setHead(PL_ARC(0x6D), PL_ARC(0x6E));
    }
}

// Spits the poison projectile (SetObj08 from the mouth part 5) aimed at the player: `type` 0 from the
// ground / wall, 1 from the air (different effects).
void em2dSetPoison(cEm2d* em, int type)
{
    cObj* obj;
    EmAtkInfo* atk;
    cModel* p;
    Vec spd;

    SndStop(EM2D_WK(em)->sndId, 0);
    SndCall(8, 0x17, &em->getPartsPtr(0)->world, em->id, 0, em);
    atk = em2d_poison_atk;  // table address in a callee-saved register across the getPartsPtr call (em25 idiom)
    p = em->getPartsPtr(5);
    obj = SetObj08(em, 0, 0, &p->world, &em->ang, 0x40000000, atk);
    if (type) {
        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = 0.0f;
        SetObj08Spd(obj, &spd, 30, 10.0f, 100.0f);
        SetObj08Est(obj, 0, 0, 0, 0, 0x25, 0xA, 0x25, 0x20, 1);
    } else {
        spd.x = 0.0f;
        spd.y = 100.0f;
        spd.z = 220.0f;
        PSMTXMultVecSR(em->mat, &spd, &spd);
        SetObj08Spd(obj, &spd, 30, 10.0f, 100.0f);
        SetObj08Est(obj, 0, 0, 0x25, 0xB, 0x25, 0xA, 0x25, 0x1D, 1);
    }
    SetObj08Se(obj, 8, 0x18);
}

// Yaw from the insect to `pos` (used for the head direction and the wall-walk target).
f32 em2dGetPlDir(cEm2d* em, Vec* pos)
{
    Mtx inv;
    Vec d;

    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, pos, &d);
    return atan2f(d.x, d.z);
}

// Wall found: the enemy switches to the wall routine (0x15) at `hit` with the normal reset.
#define em2dWallWalkSet(em, w, hit)     \
    {                                   \
        (w)->wallNrm.x = 0.0f;          \
        (w)->wallNrm.y = 1.0f;          \
        (w)->wallNrm.z = 0.0f;          \
        EmRoutineSet(em, 1, 0x15, 0, 1); \
        (w)->wallTarget = hit;          \
    }

// Is there a climbable wall right in front (three probes): sets the wall walk (W_Walk 0x15) when so. 1 = set.
int em2dWallWalkCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec a;
    Vec b;
    Vec hit;

    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    PSMTXMultVec(em->mat, &a, &a);
    b.y = 500.0f;
    b.x = 0.0f;
    b.z = 2000.0f;
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830)) {
        em2dWallWalkSet(em, w, hit);
        return 1;
    }
    b.x = 2000.0f;
    b.y = 1500.0f;
    b.z = 0.0f;
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830)) {
        em2dWallWalkSet(em, w, hit);
        return 1;
    }
    b.x = -2000.0f;
    b.y = 1500.0f;
    b.z = 1000.0f;
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830)) {
        em2dWallWalkSet(em, w, hit);
        return 1;
    }
    return 0;
}

// 1 when the wall under the wall-walker ends (no surface ahead / below): it must drop (W_Fall).
int em2dWallFallCk(cEm2d* em)
{
    Vec a;
    Vec b;

    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = -1000.0f;
    b.z = 500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830)) {
        return 0;
    }
    a.x = 0.0f;
    a.y = -500.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = -500.0f;
    b.z = -1000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    return SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830) == 0;
}

// 1 while the player is running (routine 0 / 3).
int em2dPlRunCk(cEm2d* em)
{
    if (pPL->r_no_0 != 0) {
        return 0;
    }
    return pPL->r_no_1 == 3;
}

// Opens a closed door (cEmDoor) the insect walks into from its side.
void em2dDoorOpenCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec v;
    f32 ang;
    u32 i;

    if (w->stuckCnt % 10 != 5) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* e = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->base_dir, em->ang.y, 3.14159274f));
        if (ang > 0.785398185f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->base_im, &em->pos, &v);
        if (ang < 1.57079637f) {
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

// Footstep SEs on the motion's step events (parts / floor material), with the wall / ceiling variants.
void em2dFootSeMove(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    cModel* p;
    cModel* p0;
    Vec pos;
    int parts;
    u32 no;

    if (em->seNo == 0) {
        return;
    }
    no = em->seNo - 1;
    parts = 0x12;
    switch (no) {
    case 0:
        break;
    case 1:
        parts = 0x16;
        break;
    case 4:
        em->seNo = 0;
        em2dSetdLandingEff(em);
        return;
    case 5:
        em->seNo = 0;
        em2dSetDownEff(em);
        return;
    case 7:
        em->seNo = 0;
        em2dSetJumpEff(em);
        return;
    default:
        return;
    }
    p = em->getPartsPtr(parts);
    p0 = em->getPartsPtr(0);
    if (w->wallNrm.y < -0.899999976f) {
        EstSet(0, -1, &p->world, 0, 0x25, 8, 0, 0, 0, 0);
    }
    if (w->wallNrm.y > 0.699999988f && ChkWaterEffectEnable(&p->world)) {
        em->seNo = 0;
        if ((w->flags & 0x80000) && (pG->room_id == 0x205 || pG->room_id == 0x21D)) {
            pos = p->world;
            pos.y = w->waterH;
            EstSet(0, -1, &pos, 0, 1, 0x14, 0, 0, 0, 0);
            switch (no) {
            case 0:
                SndCall(8, 0x2B, &p0->world, em->id, 0, em);
                break;
            case 1:
                SndCall(8, 0x2C, &p0->world, em->id, 0, em);
                break;
            }
        } else {
            EstSet(0, -1, &p->world, 0, 0x25, 0, 0, 0, 0, 0);
            switch (no) {
            case 0:
                SndCall(8, 2, &p0->world, em->id, 0, em);
                break;
            case 1:
                SndCall(8, 3, &p0->world, em->id, 0, em);
                break;
            }
        }
    }
}

// Landing dust / splash at the root and the feet (parts 0x12 / 0x16).
void em2dSetdLandingEff(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    cModel* p0 = em->getPartsPtr(0);
    Vec pos;
    cModel* p;

    if ((w->flags & 0x80000) && (pG->room_id == 0x205 || pG->room_id == 0x21D)) {
        SndCall(8, 0x2E, &em->pos, em->id, 0, em);
        p = em->getPartsPtr(0x12);
        pos = p->world;
        pos.y = w->waterH;
        EstSet(0, -1, &pos, 0, 1, 0x15, 0, 0, 0, 0);
        p = em->getPartsPtr(0x16);
        pos = p->world;
        pos.y = w->waterH;
        EstSet(0, -1, &pos, 0, 1, 0x15, 0, 0, 0, 0);
    } else if (ChkWaterEffectEnable(&em->pos)) {
        SndCall(8, 2, &p0->world, 0, 0, em);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        EstSet(0, -1, &em->getPartsPtr(0x12)->world, 0, 0x25, 0, 0, 0, 0, 0);
        EstSet(0, -1, &em->getPartsPtr(0x16)->world, 0, 0x25, 0, 0, 0, 0, 0);
    } else {
        SndCall(8, 4, &em->pos, em->id, 0, em);
    }
}

// Dust / splash when the insect hits the floor on its back.
void em2dSetDownEff(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    cModel* p0 = em->getPartsPtr(0);

    if (ChkWaterEffectEnable(&em->pos)) {
        SndCall(8, 2, &p0->world, 0, 0, em);
        if ((w->flags & 0x80000) && (pG->room_id == 0x205 || pG->room_id == 0x21D)) {
            SndCall(8, 0x2F, &p0->world, em->id, 0, em);
        } else {
            SndCall(8, 5, &p0->world, em->id, 0, em);
        }
        EstSet(0, -1, &em->pos, 0, 0x25, 0xC, 0, 0, 0, 0);
    } else {
        SndCall(8, 5, &p0->world, em->id, 0, em);
    }
}

// Dust / splash at the take-off of a jump.
void em2dSetJumpEff(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    cModel* p0 = em->getPartsPtr(0);

    if (ChkWaterEffectEnable(&em->pos)) {
        SndCall(8, 2, &p0->world, 0, 0, em);
        if ((w->flags & 0x80000) && (pG->room_id == 0x205 || pG->room_id == 0x21D)) {
            SndCall(8, 0x2D, &p0->world, em->id, 0, em);
        } else {
            SndCall(8, 7, &p0->world, em->id, 0, em);
        }
        EstSet(0, -1, &em->pos, 0, 0x25, 0xC, 0, 0, 0, 0);
    } else {
        SndCall(8, 7, &p0->world, em->id, 0, em);
    }
}

// The floor under the insect dropped away by more than 250 -> W_Fall (0x19). 1 = set.
int em2dFallCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    if (SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) > em->pos.y - 250.0f) {
        return 0;
    }
    w->wallNrm.x = 0.0f;
    w->wallNrm.y = 1.0f;
    w->wallNrm.z = 0.0f;
    EmRoutineSet(em, 1, 0x19, 0, 0);
    return 1;
}

// Half the time, when the player stands above and a wall rises in front -> DownJump (0x10). 1 = set.
int em2dDownJumpCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec a;
    Vec b;
    int r;

    if (!(w->flags & 0x4000)) {
        return 0;
    }
    if (em->flag & 0x40000000) {
        return 0;
    }
    r = Rnd() & 1;
    if (r) {
        return 0;
    }
    b = em->pos;
    a = b;
    a.y += 500.0f;
    b.y += 8000.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830) == 0) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x10, r, r);
    return 1;
}

// Half the time, with a ceiling within reach above -> ToCeiling (0x11). 1 = set.
int em2dToCeilingCk(cEm2d* em)
{
    Vec a;
    Vec b;
    int r;

    if (em->flag & 0x40000000) {
        return 0;
    }
    if (Rnd() & 1) {
        return 0;
    }
    r = em2dNoWallCk(em);
    if (r) {
        return 0;
    }
    b = em->pos;
    a = b;
    a.y += 500.0f;
    b.y += 8000.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830) == 0) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x11, r, r);
    return 1;
}

// The flying type takes off (ToAir 0x1A) one time in four when allowed. 1 = set.
int em2dToAirCk(cEm2d* em)
{
    int r = Rnd() & 3;

    if (r) {
        return 0;
    }
    if (em->set != 1) {
        return 0;
    }
    EmRoutineSet(em, em->set, 0x1A, r, r);
    return 1;
}

// The flying type lands (ToGround 0x1B) one time in four when low enough over the floor. 1 = set.
int em2dToGround(cEm2d* em)
{
    int r = Rnd() & 3;
    int set;

    if (r) {
        return 0;
    }
    set = em->set;
    if (set != 1) {
        return 0;
    }
    if (em->plDist2 < 64000000.0f) {
        return 0;
    }
    if (em->pos.y > SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 2000.0f) {
        return 0;
    }
    EmRoutineSet(em, set, 0x1B, r, r);
    return 1;
}

// 1 while alive, active and the player is found (flag 0x200).
int cEm2d::ckFindPL()
{
    if (hp <= 0) {
        return 0;
    }
    if (checkStatus(EM_STATUS_ACTIVE) == 0) {
        return 0;
    }
    if (EM2D_WK(this)->flags & 0x200) {
        return 1;
    }
    return 0;
}

// The eye glow: blinks the eye parts by the x535 state machine while alive.
void em2dEyeMove(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    int st;

    if (em->type == 4) {
        return;
    }
    st = 0;
    if (em->hp > 0) {
        st = 1;
    }
    if (w->flags & 0x10000) {
        st = 2;
    }
    if (em->Refract_ratio == 0) {
        st = 0;
    }
    if (w->flags & 0x20000) {
        st = 0;
    }
    if (st == w->x535) {
        return;
    }
    EffectEspDelete(0, w->espKind2, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind2, (int) em);
    EffectEfmDelete(0, w->espKind2, (int) em);
    switch (w->x535) {
    case 1:
        EstSet((int) em, -1, 0, 0, 0x25, 0x12, 0, 0, (u32) em, 0);
        break;
    case 2:
        EstSet((int) em, -1, 0, 0, 0x25, 0x11, 0, 0, (u32) em, 0);
        break;
    }
    w->x535 = st;
    switch (st) {
    case 1:
        EstSet((int) em, -1, 0, 0, 0x25, 0x10, 0, w->espKind2, (u32) em, 0);
        break;
    case 2:
        EstSet((int) em, -1, 0, 0, 0x25, 0xF, 0, w->espKind2, (u32) em, 0);
        break;
    }
}

// 1 when the insect's root or head is inside the screen.
int em2dScreenInCk(cEm2d* em)
{
    Vec scr;
    Vec pos;

    pos = em->pos;
    if (GetScreenPos(&pos, &scr) && scr.x > 0.0f && scr.x < 512.0f && scr.y > 0.0f && scr.y < 448.0f) {
        return 1;
    }
    pos = em->getPartsPtr(4)->world;
    if (GetScreenPos(&pos, &scr) && scr.x > 0.0f && scr.x < 512.0f && scr.y > 0.0f && scr.y < 448.0f) {
        return 1;
    }
    return 0;
}

// An edge ahead (scenario mask 0x102010) within 30 deg of the heading -> JumpDown (0x12). 1 = set.
int em2dJumpDownCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec nrm;
    Vec a;
    Vec b;
    f32 ang;

    if (w->stuckCnt % 10 != 4) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 1000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if ((SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & 0x102010) == 0) {
        return 0;
    }
    ang = atan2f(-nrm.x, -nrm.z);
    if (fabsf(Muku2(em->ang.y, ang, 3.14159274f)) > 0.52359879f) {
        return 0;
    }
    w->jumpAng = ang;
    EmRoutineSet(em, 1, 0x12, 0, 0);
    return 1;
}

// A low wall ahead within 30 deg of the heading -> WallOver (0x13). 1 = set.
int em2dWallOverCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Vec nrm;
    Vec a;
    Vec b;
    f32 ang;
    int zero;

    if (w->stuckCnt % 20 != 18) {
        return 0;
    }
    zero = em->flag & 0x40000000;
    if (zero) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 1000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830) == 0) {
        return 0;
    }
    ang = atan2f(-nrm.x, -nrm.z);
    if (fabsf(Muku2(em->ang.y, ang, 3.14159274f)) > 0.52359879f) {
        return 0;
    }
    w->jumpAng = ang;
    EmRoutineSet(em, 1, 0x13, zero, zero);
    return 1;
}

// Picks the next air routine after an A_ routine: attack (A_Atk / A_Catch) when the player is found
// and faces it, dodge (A_Step) when he aims at it (em2dLockCk), A_Back when too close, A_Wait, A_Turn180
// when he is behind, A_Down / A_Up by relative height, else A_Walk.
void em2dAirNextRtnSet(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    f32 dy;
    int wait;
    int wait2;
    int r;

    if (w->flags & 0x8000) {
        EmRoutineSet(em, 1, 0x1D, 0, 0);
        return;
    }
    wait = w->atkWait;
    if (wait == 0 && em->plDist2 < 9000000.0f && w->routeAngAbs < 0.785398185f) {
        dy = em->pos.y - pPL->pos.y;
        if (dy < 1000.0f && dy > -200.0f) {
            fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
            if ((Rnd() & 1) && em->plDist2 > 12250000.0f) {
                EmRoutineSet(em, 1, 0x23, wait, wait);
            } else {
                EmRoutineSet(em, 1, 0x24, 0, 0);
            }
            return;
        }
    }
    if (em2dLockCk(em) && Rnd() % 10 > 6) {
        EmRoutineSet(em, 1, 0x1F, 0, 0);
        return;
    }
    wait2 = w->atkWait;
    if (!(wait2 == 0 && em2dStayCk(em) && (w->flags & 0x200))) {
        if (em->plDist2 < 16000000.0f) {
            EmRoutineSet(em, 1, 0x1E, 0, 0);
            return;
        }
        w->atkWait = 30;
        EmRoutineSet(em, 1, 0x1C, 0, 0);
        return;
    }
    {
        if (w->targetAngAbs > 1.57079637f) {
            EmRoutineSet(em, 1, 0x22, wait2, wait2);
            return;
        }
        if (em->pos.y > SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 2000.0f &&
            em->pos.y > pPL->pos.y + 1000.0f && ((Rnd() & 1) || em->plDist2 < 9000000.0f)) {
            EmRoutineSet(em, 1, 0x21, wait2, wait2);
            return;
        }
        if (em->pos.y < pPL->pos.y + 1000.0f && em->plDist2 > 9000000.0f && (r = Rnd() & 3) == 0 &&
            Em2dGetCeiling(em) > em->pos.y + 4000.0f) {
            EmRoutineSet(em, 1, 0x20, r, r);
            return;
        }
        r = em2dToGround(em);
        if (r) {
            return;
        }
        if ((Rnd() & 3) && em->plDist2 > 16000000.0f) {
            EmRoutineSet(em, 1, 0x1D, r, r);
            return;
        }
        {
            int r2 = Rnd() & 3;
            if (r2) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1E, r2, r2);
            }
        }
    }
}

// Sight check: the player found (flag 0x200) when seen and near, on the bell alarm within range, or
// on the room's forced alert within 25000 route units. 1 = found.
int em2dFindCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);

    if (w->flags & 0x200) {
        return 0;
    }
    if (w->routeAngAbs < 1.04719758f && em->plDist2 < 100000000.0f) {
        w->flags |= 0x200;
        return 1;
    }
    if (!(em->plDist2 < 9000000.0f)) {
        if (StaFlagChk(pG, STA_SE_BURST)) {
            f32 r;

            switch (pG->bell_stat) {
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
            {
                // em10FindCk bell idiom: the override makes the arm sets dead (the compares stay) and
                // `r` a block-local pseudo loaded at the use.
                f32 dx = em->pos.x - pGS->bell_pos.x;
                f32 dy = em->pos.y - pGS->bell_pos.y;
                f32 dz = em->pos.z - pGS->bell_pos.z;
                r = 25000.0f;
                if (dx * dx + dy * dy + dz * dz < r * r && (w->flags & 1) && w->plDist < r) {
                    w->flags |= 0x200;
                    return 1;
                }
            }
        }
        if (!StaFlagChk(pG, STA_PL_FIRE) || !(w->plDist < 25000.0f)) {
            if (em2dDeadCk(em) == 0 && em2dSomebodyFindCk(em) == 0) {
                return 0;
            }
        }
    }
    w->flags |= 0x200;
    return 1;
}

// 1 when another active insect that has found the player is within 10000 units (30000 for a flier):
// it joins in.
int em2dSomebodyFindCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        f32 d;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2D) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->checkStatus(EM_STATUS_ACTIVE) == 0) {
            continue;
        }
        if (!(EM2D_WK(e)->flags & 0x200)) {
            continue;
        }
        d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
            (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        if (w->flags & 0x800) {
            if (d > 900000000.0f) {
                continue;
            }
        } else if (d > 100000000.0f) {
            continue;
        }
        return 1;
    }
    return 0;
}

// 1 once the die routine finished (Reset_enable): the room may reset the enemy.
int cEm2d::ckReset()
{
    if (EM2D_WK(this)->Reset_enable == 0) {
        return 0;
    }
    return 1;
}

// Room script: resets the insect to `pos` / `rot` (or its start) alive and visible with full hp,
// already alerted, into the air routine (A_Walk) for the flying type or the ground walk.
void cEm2d::setReset(Vec* pos, Vec* rot)
{
    Em2dWork* w = EM2D_WK(this);
    Vec p;
    cModel* parts;

    invisible_factor = 1.0f;
    atari.m_flag |= 0x300;
    atari.m_flag &= 0xFFEF;
    be_flag |= 2;
    be_flag &= ~0x10000;
    be_flag &= ~0x4000;
    if (pos) {
        p = *pos;
    } else {
        p = w->startPos;
    }
    if (rot) {
        this->ang = *rot;
    } else {
        this->ang = w->startRot;
    }
    setPos(&p);
    pos_old = p;
    set = w->startSet;
    flag = w->startSet2;
    hp = hp_max;
    w->homePos = this->pos;
    em2dInitRtnSet(this);
    w->flags |= 0x200;
    if (pGS->room_id == 0x213) {
        Vec r213Pos = {0.0f, 6000.0f, -52632.0f};
        this->pos = r213Pos;
        this->ang.y = fRand1_1() * 3.14159274f;
        MotionSetCore(this, &Motion, PL_ARC_PTR(subArc, 0x67), 0, 0, 5, 0);
        EmRoutineSet(this, 1, 0x1D, 0, 0);
        w->catchGuard = 30;
    }
    w->catchGuard = 30;
    if (set == 1) {
        EmRoutineSet(this, set, 0x1D, 0, 0);
    }
    MotionMoveF(this, 0);
    em2d_R0_Move(this);
    partsWorldCalc();
    for (parts = pParts; parts; parts = parts->pParts) {
        parts->world_old = parts->world;
        parts->world_old2 = parts->world_old;
    }
}

// Height of the ceiling above the insect (scenario probe up to the limit), or the probe top.
f32 Em2dGetCeiling(cEm2d* em)
{
    Vec a;
    Vec b;
    Vec hit;

    a = em->pos;
    b = em->pos;
    b.y += 20000.0f;
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830)) {
        b.y = hit.y;
    }
    return b.y;
}

// Reaching an EMI type 0xC return point within 3000 units: heads home (flag 0x8000) and forgets the
// player. 1 = set.
int em2dReturnPosCk(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    EmiData* emi = (EmiData*) pG->pEmi;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    if (w->flags & 0x8000) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 0xC &&
            !((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                  (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
              9000000.0f)) {
            w->flags |= 0x8000;
            w->flags &= ~0x200;
            return 1;
        }
    }
    return 0;
}

// The wing hum: keeps the hum effect / SE (0x27 / 0x32) on the flying insect while flag 0x40000 is
// set, at most three closest to the camera at a time.
void em2dHumSeMove(cEm2d* em)
{
    Em2dWork* w = EM2D_WK(em);
    Camera* cam;
    u32 cnt;
    u32 i;
    f32 d;

    if (!(w->flags & 0x40000)) {
        if (w->flags & 0x100000) {
            EffectEspDelete(0, w->espKind3, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind3, (int) em);
            EffectEfmDelete(0, w->espKind3, (int) em);
            w->flags &= ~0x100000;
        }
        return;
    }
    if (!(w->flags & 0x100000)) {
        EstSet((int) em, -1, 0, 0, 0x25, 0x27, 0, w->espKind3, (u32) em, 0);
        w->flags |= 0x100000;
    }
    if (w->x534) {
        w->x534--;
        return;
    }
    cam = &pG->Cam;
    d = (cam->param.pos.x - em->pos.x) * (cam->param.pos.x - em->pos.x) +
        (cam->param.pos.y - em->pos.y) * (cam->param.pos.y - em->pos.y) +
        (cam->param.pos.z - em->pos.z) * (cam->param.pos.z - em->pos.z);
    cnt = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x2D && e->hp > 0 && e != em && e->checkStatus(EM_STATUS_ACTIVE) &&
            (EM2D_WK(e)->flags & 0x40000) &&
            (cam->param.pos.x - e->pos.x) * (cam->param.pos.x - e->pos.x) +
                    (cam->param.pos.y - e->pos.y) * (cam->param.pos.y - e->pos.y) +
                    (cam->param.pos.z - e->pos.z) * (cam->param.pos.z - e->pos.z) <
                d) {
            cnt++;
        }
    }
    if (cnt > 2) {
        return;
    }
    w->x534 = 29;
    SndCall(8, 0x32, &em->pos, em->id, 0, em);
}

// 1 when an EMI type 0x10 "no wall climbing" point lies within 1500 units: the insect stays on the floor.
int em2dNoWallCk(cEm2d* em)
{
    EmiData* emi = (EmiData*) pG->pEmi;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 0x10 &&
            !((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
              2250000.0f)) {
            return 1;
        }
    }
    return 0;
}
