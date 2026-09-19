// em2c module (D:/Bio4/Prog/em2c.cpp): the insect boss (cModel::type 0) and its tail (type 1).
// It shares the em2d routine set (floor walking, wall / ceiling climbing, jump and tail attacks,
// the player-catch callbacks) and adds the hide / ambush states, the freeze reaction and the
// two-motion blends (em2cBlendMotSet).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em2c.h"
#include "emhit.h"
#include "emdoor.h"
#include "obj01.h"
#include "obj.h"
#include "pl_wep.h"
#include "pendulum.h"
#include "main.h"
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
#include "dbmodule.h"
#include "db_log.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em2c,52,4");
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "item.h"

extern "C" void OSReport(const char* fmt, ...);
int GetWepDmVal(cEm* em, u32 wep_no, int near);   // em10.h (not included: it pulls emwep.h's global plemBackjump)
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
// COMPILER-DIFF #4: the original passes the int work field to the u16 parameter without the
// truncation ours emits (`lwz` instead of `lhz`); int-view declarations of the two blend setters.
void em2cBlendMotSetI(cEm2c* em, void* m0, void* m1, void* m2, int a, int b, int c, int d) asm("em2cBlendMotSet__FP5cEm2cPvN21iiiUs");
void em2cBlendMotSet2I(cEm2c* em, void* m0, void* m1, int a, int b, int d) asm("em2cBlendMotSet2__FP5cEm2cPvT1iiUs");

static void em2c_R0_Init(cEm2c* em);
static void em2c_R0_Move(cEm2c* em);
static void em2c_R1_br_Dummy(cEm2c* em);
static void em2c_R1_Wait(cEm2c* em);
static void em2c_R1_Walk(cEm2c* em);
static void em2c_R1_Dash(cEm2c* em);
static void em2c_R1_Turn180(cEm2c* em);
static void em2c_R1_Ambush(cEm2c* em);
static void em2c_R1_SideStep(cEm2c* em);
static void em2c_R1_BackJump(cEm2c* em);
static void em2c_R1_Threat(cEm2c* em);
static void em2c_R1_AtkSign(cEm2c* em);
static void em2c_R1_Atk(cEm2c* em);
static void em2c_R1_JumpAtk(cEm2c* em);
static void em2c_R1_BackKnuckle(cEm2c* em);
static void em2c_R1_TailAtk(cEm2c* em);
static void plem2c_CriticalHit(cPlayer* pl);
static void em2c_R1_SwayBack(cEm2c* em);
static void em2c_R1_WakeupWait(cEm2c* em);
static void em2c_R1_Wakeup(cEm2c* em);
static void em2c_R1_DownJump(cEm2c* em);
static void em2c_R1_ToCeiling(cEm2c* em);
static void em2c_R1_ToHide(cEm2c* em);
static void em2c_R1_HideWait(cEm2c* em);
static void em2c_R1_HideAtk(cEm2c* em);
static void em2c_R1_HideFall(cEm2c* em);
static void em2c_R1_JumpDown(cEm2c* em);
static void em2c_R1_WallOver(cEm2c* em);
static void em2c_R1_F_Wait(cEm2c* em);
static void em2c_R1_F_Walk(cEm2c* em);
static void em2c_R1_F_Atk(cEm2c* em);
static void em2c_R1_F_Clear(cEm2c* em);
static void em2c_R1_W_Wait(cEm2c* em);
static void em2c_R1_W_Walk(cEm2c* em);
static void em2c_R1_W_Atk(cEm2c* em);
static void em2c_R1_W_Turn180(cEm2c* em);
static void em2c_R1_W_Fall(cEm2c* em);
static void em2c_R1_C_Wait(cEm2c* em);
static void em2c_R1_C_Fall(cEm2c* em);
static void em2c_R1_Reset_Wait(cEm2c* em);
static void em2c_R1_T_Wait(cEm2c* em);
static void em2c_R1_T_Hide(cEm2c* em);
static void em2c_R0_Damage(cEm2c* em);
static void em2c_R1_Dm_Normal(cEm2c* em);
static void em2c_R1_Dm_Down(cEm2c* em);
static void em2c_R1_Dm_Jump(cEm2c* em);
static void em2c_R1_Dm_Wall(cEm2c* em);
static void em2c_R1_Dm_Guard(cEm2c* em);
static void em2c_R1_Dm_Freeze(cEm2c* em);
static void em2c_R1_Dm_C_Freeze(cEm2c* em);
static void em2cKickAction(cEm2c* em);
static void em2c_R1_Dm_F_Normal(cEm2c* em);
static void em2c_R1_Dm_F_Blow(cEm2c* em);
static void em2c_R0_Die(cEm2c* em);
static void em2c_R1_Die_Lost(cEm2c* em);
static void em2c_R1_Die_Normal(cEm2c* em);
static void em2c_R1_Die_Freeze(cEm2c* em);
static void em2c_R1_Die_Down(cEm2c* em);
static void em2c_R1_Die_Wall(cEm2c* em);
static void plemDmSide(cPlayer* pl);
static void plemDmTail(cPlayer* pl);
static void em2cSitAction(cEm2c* em);
static void plem2cSit(cPlayer* pl);
static void em2cEscapeAction(cEm2c* em);
static void plem2cEscape(cPlayer* pl);
static void em2cBackjumpAction(cEm2c* em);
static void em2cBackjumpAction2(cEm2c* em);
static void plemBackjump2(cPlayer* pl);

// The two player callbacks that emwep.h also declares carry C linkage here (the module's own
// local copies keep the unmangled names in the REL symbol table).
extern "C" {
static void plemKick(cPlayer* pl);
static void plemBackjump(cPlayer* pl);
}

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// Struct-member view of the player pointer (cam_ctrl.cpp PlayerPtr).
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
static inline int em2cDeadCk(cEm* em)
{
    return em->dmg.m_Flag || em->dmg.m_Timer;
}

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->m_flag |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->m_flag &= mask; }

// Attack wait by difficulty: pG is reloaded after every store (reference stores).
static inline void em2cSetAtkWait(Em2cWork* w, int a, int b, int c, int d, int e)
{
    IntSet(w->atkWait, a);
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
static inline void em2cGravityMove(cEm2c* em, Em2cWork* w)
{
    f32 fl;

    PSVECAdd(&em->pos, &w->spd, &em->pos);
    w->spd.y -= 20.0f;
    fl = SatMgr.getFloor(&em->pos_old, 600.0f, 100000.0f, 0, 0);
    if (em->pos.y < fl) {
        em->pos.y = fl;
        w->spd.y = 0.0f;
    }
}

// Turn towards `target` by at most `step` per frame.
static inline void em2cTurnTo(cEm2c* em, Vec* target, f32 step)
{
    em->ang.y += Muku(&em->pos, target, em->ang.y, step);
    em->ang.y = LIMIT_ANGLE(em->ang.y);
}

Em2cFunc Em2c_R0_move_tbl[4] = {
    em2c_R0_Init,
    em2c_R0_Move,
    em2c_R0_Damage,
    em2c_R0_Die,
};

// Routine 1 table: {branch check, routine} per xFD (a flat table: `[xFD * 2 + 1]`).
static Em2cFunc Em2c_R1_move_tbl[76] = {
    em2c_R1_br_Dummy, em2c_R1_Wait,         // 0x00
    em2c_R1_br_Dummy, em2c_R1_Walk,         // 0x01
    em2c_R1_br_Dummy, em2c_R1_Dash,         // 0x02
    em2c_R1_br_Dummy, em2c_R1_Turn180,      // 0x03
    em2c_R1_br_Dummy, em2c_R1_Ambush,       // 0x04
    em2c_R1_br_Dummy, em2c_R1_SideStep,     // 0x05
    em2c_R1_br_Dummy, em2c_R1_BackJump,     // 0x06
    em2c_R1_br_Dummy, em2c_R1_Threat,       // 0x07
    em2c_R1_br_Dummy, em2c_R1_AtkSign,      // 0x08
    em2c_R1_br_Dummy, em2c_R1_Atk,          // 0x09
    em2c_R1_br_Dummy, em2c_R1_JumpAtk,      // 0x0A
    em2c_R1_br_Dummy, em2c_R1_BackKnuckle,  // 0x0B
    em2c_R1_br_Dummy, em2c_R1_TailAtk,      // 0x0C
    em2c_R1_br_Dummy, em2c_R1_SwayBack,     // 0x0D
    em2c_R1_br_Dummy, em2c_R1_WakeupWait,   // 0x0E
    em2c_R1_br_Dummy, em2c_R1_Wakeup,       // 0x0F
    em2c_R1_br_Dummy, em2c_R1_DownJump,     // 0x10
    em2c_R1_br_Dummy, em2c_R1_ToCeiling,    // 0x11
    em2c_R1_br_Dummy, em2c_R1_ToHide,       // 0x12
    em2c_R1_br_Dummy, em2c_R1_HideWait,     // 0x13
    em2c_R1_br_Dummy, em2c_R1_HideAtk,      // 0x14
    em2c_R1_br_Dummy, em2c_R1_HideFall,     // 0x15
    em2c_R1_br_Dummy, em2c_R1_JumpDown,     // 0x16
    em2c_R1_br_Dummy, em2c_R1_F_Wait,       // 0x17
    em2c_R1_br_Dummy, em2c_R1_F_Walk,       // 0x18
    em2c_R1_br_Dummy, em2c_R1_F_Atk,        // 0x19
    em2c_R1_br_Dummy, em2c_R1_F_Clear,      // 0x1A
    em2c_R1_br_Dummy, em2c_R1_WallOver,     // 0x1B
    em2c_R1_br_Dummy, em2c_R1_W_Wait,       // 0x1C
    em2c_R1_br_Dummy, em2c_R1_W_Walk,       // 0x1D
    em2c_R1_br_Dummy, em2c_R1_W_Atk,        // 0x1E
    em2c_R1_br_Dummy, em2c_R1_W_Turn180,    // 0x1F
    em2c_R1_br_Dummy, em2c_R1_W_Fall,       // 0x20
    em2c_R1_br_Dummy, em2c_R1_C_Wait,       // 0x21
    em2c_R1_br_Dummy, em2c_R1_C_Fall,       // 0x22
    em2c_R1_br_Dummy, em2c_R1_Reset_Wait,   // 0x23
    em2c_R1_br_Dummy, em2c_R1_T_Wait,       // 0x24
    em2c_R1_br_Dummy, em2c_R1_T_Hide,       // 0x25
};

static Em2cFunc Em2c_R1_dm_tbl[9] = {
    em2c_R1_Dm_Normal,
    em2c_R1_Dm_Down,
    em2c_R1_Dm_Jump,
    em2c_R1_Dm_Wall,
    em2c_R1_Dm_Guard,
    em2c_R1_Dm_Freeze,
    em2c_R1_Dm_C_Freeze,
    em2c_R1_Dm_F_Normal,
    em2c_R1_Dm_F_Blow,
};

static Em2cFunc Em2c_R1_die_tbl[5] = {
    em2c_R1_Die_Lost,
    em2c_R1_Die_Normal,
    em2c_R1_Die_Freeze,
    em2c_R1_Die_Down,
    em2c_R1_Die_Wall,
};

// Motion parts flip table (cModel::motFlip): the mirrored parts index per parts.
static u16 em2c_xflip_tbl[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x0A, 0x0B, 0x0C, 0x0D, 0x06, 0x07, 0x08, 0x09, 0x0E, 0x13,
    0x14, 0x15, 0x16, 0x0F, 0x10, 0x11, 0x12, 0x18, 0x17, 0x1A, 0x19, 0x1C, 0x1B, 0x1D, 0x1E, 0x20,
    0x1F, 0x23, 0x24, 0x21, 0x22, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x34, 0x35, 0x36, 0x31, 0x32, 0x33, 0x38, 0x37, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4F, 0x50, 0x4D,
    0x4E, 0x52, 0x51, 0x54, 0x53, 0x55, 0x57, 0x56, 0x58, 0x5A, 0x59, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Attack parameters per attack number (em2cAtkCk).
static EmAtkInfo em2c_atk_info[8] = {
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 300, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
};

// Module entry (SN loader): registers Em2cInit as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em2c prolog Ok\n");
    EmInitFunc = Em2cInit;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm2c class in the manager's work.
void Em2cInit(cEm* em)
{
    new (em) cEm2c();
}

// Damage reaction after a hit while alive (em2cDmCk): by the state flags, then by the weapon.
void em2cDmCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Camera* cam = &pG->Cam;
    YARARE_INFO* part;
    cModel* p;
    int near;
    int guard;
    int dmg;
    f32 dmAng;
    f32 dist;
    f32 py;

    if (em->hp > 0) {
        if (!(w->flags & 0x100840) && !em2cDeadCk(em)) {
            int two = 2;      // the routine 2 of the first two arms in a callee-saved register

            if (pG->Room_flg[2] & 0x80000000) {
            em2cSetFreeze(em);
            if (w->flags & 0x200000) {
                EmRoutineSet(em, two, 6, 0, 0);
                return;
            }
            if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
                EmRoutineSet(em, two, 3, 0, 0);
                return;
            }
            if (w->flags & 0x1010) {
                EmRoutineSet(em, 2, 2, 0, 0);
                return;
            }
            if (w->flags & 0x400) {
                EmRoutineSet(em, 2, 1, 0, 0);
                return;
            }
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
            }
        }
        if (em->hp > 0 && !em2cDeadCk(em)) {
            switch (DmgMgr.hitCheck(&em->pos, 0)) {
            case 1:
            case 4:
            case 5:
            case 7:
                if (w->dmGuard == 0) {
                    w->guardCnt = 0;
                    w->dmGuard = 120;
                    LifeDownSet2(em, 500, 0, 0);
                    if (em->hp <= 0) {
                        EmSetDie(em);
                        EmReserveDropItem(em);
                        if (w->flags & 0x20) {
                            EmRoutineSet(em, 3, 4, 0, 0);
                        } else if (w->flags & 0x1010) {
                            EmRoutineSet(em, 2, 2, 0, 0);
                        } else if (w->flags & 0x800) {
                            EmRoutineSet(em, 3, 2, 0, 0);
                        } else if (w->flags & 0x400) {
                            EmRoutineSet(em, 3, 3, 0, 0);
                        } else {
                            EmRoutineSet(em, 3, 1, 0, 0);
                        }
                        return;
                    }
                    if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
                        if (w->dmgTotal <= 999) {
                            return;
                        }
                        w->dmgTotal = 0;
                        EmRoutineSet(em, 2, 3, 0, 0);
                        return;
                    }
                    if (w->flags & 0x1010) {
                        EmRoutineSet(em, 2, 2, 0, 0);
                        return;
                    }
                    if (w->flags & 0x400) {
                        EmRoutineSet(em, 2, 1, 0, 0);
                        return;
                    }
                    if (w->flags & 8) {
                        return;
                    }
                    if (w->flags & 0x800) {
                        EmRoutineSet(em, 2, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 2, 0, 0, 0);
                    }
                    return;
                }
                break;
            }
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
    w->x56C = 0;
    near = 0;
    part = em->dmg.m_pDamageYarare;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    dmAng = fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f));
    switch (em->dmg.m_Wep) {
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xF:
    case 0x15:
    case 0x21:
    case 0x28:
    case 0x2C:
        if (!(w->flags & 0x800) && w->routeAngAbs < 0.52359879f && (w->flags & 0x40000) && Rnd() % 100 > 49) {
            em->dmg.m_Timer = 0xA;
            py = part->pos.y;  // loaded once (both arms read it)
            // one if/else (two RS blocks, fresh zeros): the four-arm nest cross-jumps the stores first and
            // leaves the li blocks unmerged
            if (em->pos.y > 1700.0f ? py > 1.0f : py > 0.0f) {
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 5, 0, 0);
            }
            return;
        }
        break;
    }
    guard = 0;
    if (w->flags & 0x100000) {
        guard = 1;
    }
    dmg = em2cSetDmVal(em);
    LifeDownSet2(em, dmg, 0, guard);
    w->dmgTotal += dmg;
    if (w->guardCnt) {
        w->guardCnt -= dmg / 6;
        if (w->guardCnt < 0) {
            w->guardCnt = 0;
        }
    }
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
    case 0x15:
    case 0x26:
    case 0x2B:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 0x10:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 0x24, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 0x25, 0, 0, 0);
        }
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 0x23, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (dist < 16000000.0f) {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
                }
            } else {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
                }
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
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
    case 0x14:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (dist < 16000000.0f) {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
            }
        }
        break;
    }
    if (w->flags & 0x800) {
        SndCall(8, 9, &em->pos, em->id, 0, em);
    } else {
        SndCall(8, 6, &em->pos, em->id, 0, em);
    }
    if ((w->flags & 0x400000) && em->hp <= 0) {
        em->hp = 1;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        if (w->flags & 0x20) {
            EmRoutineSet(em, 3, 4, 0, 0);
            return;
        }
        if (w->flags & 0x1010) {
            EmRoutineSet(em, 2, 2, 0, 0);
            return;
        }
        asm volatile("");  // COMPILER-DIFF: #16 candidate: jump2's "if (c) { x = a; goto l; } x = b" hoist of the arm's `li r0,2` above the `beq` (x = r0 = the switch load below) fires in ours, not in the original; the codeless asm makes the next block's first insn a non-SET
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
            if (w->flags & 0x800) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 3, 1, 0, 0);
            }
            return;
        case 7:
        case 8:
        case 0x21:
            if (w->flags & 0x800) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 3, 1, 0, 0);
            }
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
            if (w->flags & 0x800) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 3, 1, 0, 0);
            }
            return;
        }
    }
    if (w->flags & 0x8000) {
        return;
    }
    if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        EmRoutineSet(em, 2, 3, 0, 0);
        return;
    }
    if (w->flags & 0x1010) {
        EmRoutineSet(em, 2, 2, 0, 0);
        return;
    }
    if (w->flags & 0x400) {
        if ((w->flags & 0x800) && w->guardCnt == 0) {
            return;
        }
        switch (em->dmg.m_Wep) {
        default:
            if (Rnd() & 3) {
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x2D:
            EmRoutineSet(em, 2, 1, 0, 0);
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
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        if (w->flags & 0x800) {
            EmRoutineSet(em, 2, 7, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (near == 0) {
            return;
        }
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        if (w->flags & 0x800) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 7, 0, 0);
            } else {
                EmRoutineSet(em, 2, 8, 0, 0);
            }
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 5:
    case 6:
    case 0xF:
    case 0x2C:
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        if (w->flags & 0x800) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 7, 0, 0);
            } else {
                EmRoutineSet(em, 2, 8, 0, 0);
            }
        } else if (dmAng < 1.57079637f) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        if (w->flags & 0x800) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 7, 0, 0);
            } else {
                EmRoutineSet(em, 2, 8, 0, 0);
            }
        } else if (dmAng < 1.57079637f) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 0x17:
    case 0x2A:
        if (w->flags & 0x800) {
            return;
        }
        if (dmAng < 1.57079637f) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 0x14:
        if (w->flags & 0x800) {
            EmRoutineSet(em, 2, 8, 0, 0);
        } else {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 0xE:
        return;
    }
}

// Per-frame damage check of the tail (type 1): a weapon hit rings the bell alarm, takes the damage
// off the tail's hp (the boss body reads it) with the blood effect by weapon kind; the tail itself
// has no reaction routines.
void em2cTailDmCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Camera* cam = &pG->Cam;
    cModel* p;
    int near;
    int dmg;
    f32 dist;

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
    w->x56C = 0;
    near = 0;
    if (em->dmg.m_pDamageYarare->rad < 36000000.0f) {
        near = 1;
    }
    fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f));
    dmg = em2cSetDmVal(em);
    LifeDownSet2(em, dmg, 0, 1);
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
    case 0x15:
    case 0x26:
    case 0x2B:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 0x10:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (dist < 16000000.0f) {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
                }
            } else {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
                }
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
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
    case 0x14:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (dist < 16000000.0f) {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
            }
        }
        break;
    }
    SndCall(8, 6, &em->pos, em->id, 0, em);
}

// Per-frame update: the damage check of the body (em2cDmCk) or the tail (em2cTailDmCk), clears the
// per-frame flags, timers (Dash_wait, guardCnt), the water test, the route check, the R0 table, then
// the collision size and scenario check by mode (checkAir on walls / ceilings / in the air), the
// frozen-ice texture render, cloth, neck and breath SEs.
void cEm2c::move()
{
    Em2cWork* w = EM2C_WK(this);
    cAtariInfo* at;
    f32 len;
    f32 moved;
    u16 atFlags;
    int n;

    motFlags2 &= ~0x40000000;
    if (r_no_0 != 0) {
        switch (type) {
        case 0:
        default:
            em2cDmCk(this);
            break;
        case 1:
            em2cTailDmCk(this);
            break;
        }
    }
    clearStatus(EM_STATUS_IK_OFF);
    w->flags &= ~0x007F91FE;
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->jumpWait) {
        w->jumpWait--;
    }
    if (w->dmGuard) {
        w->dmGuard--;
    }
    if (w->guardCnt) {
        w->guardCnt--;
    }
    if (w->atkWait == 0 && em2cDeadCk(pPL)) {
        w->atkWait = 10;
    }
    if (w->Dash_wait) {
        w->Dash_wait--;
    }
    if (w->x574) {
        w->x574--;
    }
    if (w->atkCnt > 450) {
        w->flags |= 0x2000;
    } else {
        w->flags &= ~0x2000;
    }
    em2cRouteCk(this);
    Em2c_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    w->flags &= ~0x400;
    if (seFlags28B & 0x80) {
        w->flags |= 0x400;
    }
    if (w->wallNrm.y < 0.699999988f) {  // member calls in the arms: `&atari` is PRE'd into both (`mr r30,r0` copy)
        atari.set(10, 600.0f, 250.0f);
    } else {
        atari.set(10, 600.0f, 500.0f);
    }
    at = &atari;
    em2cNeckMove(this);
    partsWorldCalc();
    em2cScaleCompress(this);
    len = SQRTF((pos_old.x - pos.x) * (pos_old.x - pos.x) + (pos_old.z - pos.z) * (pos_old.z - pos.z));
    atFlags = atari.m_flag;
    if (w->flags & 0x1000) {
        at->m_flag &= ~0x200;
    }
    EmAtCheck(this);
    at->move();
    if (w->flags & 0x60) {
        at->set(5, 180.0f, 500.0f);
        if (!(w->flags & 0x20)) {
            SatMgr.checkAir(this, 0x980800);
        }
    } else {
        at->set(5, 600.0f, 500.0f);
        SatMgr.check(this, 0);
    }
    atari.m_flag = atFlags;
    moved = SQRTF((pos.x - pos_old.x) * (pos.x - pos_old.x) + (pos.z - pos_old.z) * (pos.z - pos_old.z));
    if (moved < len * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em2cClothMove(this);
    em2cFootSeMove(this);
    if ((w->flags & 0x800) && hp > 0) {
        n = w->effTimer;
        if (n) {
            w->effTimer--;
        } else {
            w->effTimer = 8;
            EstSet((int) this, -1, 0, 0, 0x24, 5, 0, 0, (u32) this, (void*) n);
        }
    }
    em2cBreathSeStopCk(this);
}

// Start routine from the type and cEm::set: the body (type 0): set 0 Walk (1), 1 C_Wait (0x21, hangs
// under the ceiling), 2 Reset_Wait (0x23, waits out of sight until the room releases it); the tail
// (type 1): set 0 T_Wait (0x24), 1 T_Hide (0x25).
void em2cInitRtnSet(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero = 0;

    w->flags = zero;
    w->atkWait = zero;
    w->jumpWait = zero;
    w->Compress_y = 1.0f;
    w->wallNrm.y = 1.0f;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->wallNrm.x = 0.0f;
    w->wallNrm.z = 0.0f;
    w->humTimer = Rnd() % 90 + 90;
    w->x6BC = 5;
    w->breathTimer = 0x1D;
    w->Neck_dir_y = 0.0f;
    w->dmgTotal = zero;
    w->atkCnt = zero;
    w->x56C = zero;
    w->dmGuard = zero;
    w->Dash_wait = zero;
    w->effTimer = zero;
    w->x574 = zero;
    w->pTail = (cEm*) zero;
    w->x534 = zero;
    w->homePos = em->pos;
    em->setStatus(EM_STATUS_ACTIVE);
    switch (em->type) {
    case 0:
    default:
        switch (em->set) {
        case 0:
        default:
            EmRoutineSet(em, 1, 1, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(7), 0, 0, 5, 0);
            break;
        case 1:
            AtariOff(&em->atari, 0xFCFF);
            EmRoutineSet(em, 1, 0x21, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(0x49), 0, 0, 5, 0);
            break;
        case 2:
            AtariOff(&em->atari, 0xFCFF);
            EmRoutineSet(em, 1, 0x23, 0, 0);
            MotionSetCore(em, &em->Motion, ARC(0x49), 0, 0, 5, 0);
            break;
        }
        break;
    case 1:
        AtariOff(&em->atari, 0xFCFF);
        switch (em->set) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0x24, 0, 0);
            break;
        case 3:
            EmRoutineSet(em, 1, 0x25, 0, 0);
            break;
        }
        MotionSetCore(em, &em->Motion, ARC(0x87), 0, 0, 5, 0);
        break;
    }
}

// R0 == 0: creation. Builds the body (type 0) or tail (type 1) model, the room's ctrl12 (and the
// texture-render work for the ice, em2cTexrenderInit), collision and hit boxes, the cloth
// (em2cClothSet), effect data, and the start routine (em2cInitRtnSet).
static void em2c_R0_Init(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;

    em->ot_type = 5;
    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(5), ARC(6)) == 0) {
            pLog->err(0, 0, "em2c() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(0x86), ARC(6)) == 0) {
            pLog->err(0, 0, "em2c() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    EspDataLoad((u32) ARC(4), 0x24, 0);
    w->pCtrl12 = GetCtrlCtrl12();
    em2cTexrenderInit(em);
    em->pXFlip = em2c_xflip_tbl;
#line 1623 "D:/Bio4/Prog/em2c.cpp"
    em->p2A4 = MEM_ALLOC(0x98, 1, 13);
    {
        static const Vec ofs = {0.0f, 0.0f, 0.0f};
        static const Vec size = {10000.0f, 10000.0f, 10000.0f};
        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 600.0f, 500.0f, 500.0f, 1000.0f, 1, 0x2000, 10);
    em->litArea.on(1);
    switch (em->type) {
    case 0:
    default:
        YarareInit(em, 0.0f, -50.0f, 0.0f, 190.0f, 100.0f, 6, 1);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 240.0f, 50.0f, 2, 1);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 240.0f, 200.0f, 3, 1);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 220.0f, 50.0f, 4, 1);
        YarareAdd(em, &w->hit[3], -400.0f, 0.0f, 0.0f, 140.0f, 400.0f, 8, 3);
        YarareAdd(em, &w->hit[4], -400.0f, 0.0f, 0.0f, 110.0f, 500.0f, 9, 3);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 140.0f, 400.0f, 0xC, 3);
        YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 110.0f, 500.0f, 0xD, 3);
        YarareAdd(em, &w->hit[7], 0.0f, -500.0f, 0.0f, 150.0f, 500.0f, 0x10, 1);
        YarareAdd(em, &w->hit[8], 0.0f, -600.0f, 0.0f, 120.0f, 600.0f, 0x11, 1);
        YarareAdd(em, &w->hit[9], 0.0f, -500.0f, 0.0f, 150.0f, 500.0f, 0x14, 1);
        YarareAdd(em, &w->hit[10], 0.0f, -600.0f, 0.0f, 120.0f, 600.0f, 0x15, 1);
        YarareAdd(em, &w->hit[11], 0.0f, 0.0f, 0.0f, 190.0f, 50.0f, 0x1E, 1);
        YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 190.0f, 50.0f, 0x1F, 1);
        break;
    case 1:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 1, 1);
        break;
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em2cClothSet(em);
    w->espKind = EspPullCoreKind();
    w->espKind2 = EspPullCoreKind();
    switch (em->type) {
    case 0:
    default:
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, (void*) zero);
        break;
    case 1:
        break;
    }
    w->startPos = em->pos;
    w->startRot = em->ang;
    em2cInitRtnSet(em);
    MotionMoveF(em, 0);
    em2c_R0_Move(em);
    OSReport("em2c free size = 0x%x\n", 0x6C0);
}

// R0 == 1: runs the branch check and the move handler of R1 (Em2c_R1_move_tbl pairs).
static void em2c_R0_Move(cEm2c* em)
{
    Em2c_R1_move_tbl[em->r_no_1 * 2](em);
    Em2c_R1_move_tbl[em->r_no_1 * 2 + 1](em);
}

// Branch check of the routines that have none.
static void em2c_R1_br_Dummy(cEm2c* em)
{
}

// ---------------------------------------------------------------------------------------------
// Routine 1: floor

// R1 == 0x00 Wait: idle on the floor (flags 0x180 ground, 0x40000 breath); when the player is found
// turns (Turn180 3), walks (1) or dashes (2), or hides above him (ToHide 0x12, room 221 when he looks
// away), or side-steps (5).
static void em2c_R1_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x180;
    w->flags |= 0x40000;
    switch (em->r_no_2) {
    case 0:
        if (em->motFlags & 0x40) {
            MotionSetCore(em, &em->Motion, ARC(7), 0, 30, 0x45, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(7), 0, 30, 5, 0);
        }
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        if (w->routeAngAbs > 1.57079637f && pG->Game_level > 3) {
            w->atkWait = 0;
        }
        if (em->plDist2 > 25000000.0f && pG->Game_level > 3) {
            w->atkWait = 0;
        }
        if (em2cDeadCk(em) && pG->Game_level > 1) {
            w->atkWait = 0;
        }
        if (w->atkWait) {
            break;
        }
        r = em2cAmbushCk(em);
        if (r) {
            break;
        }
        if (w->targetAngAbs > 2.09439516f) {
            EmRoutineSet(em, 1, 3, r, r);
        } else if (w->flags & 0x800) {
            EmRoutineSet(em, 1, 0x18, r, r);
        } else if (em->plDist2 > 25000000.0f && Rnd() % 10 > 7 && pG->room_id == 0x221 &&
                   fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) > 1.57079637f) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f && w->Dash_wait == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    if (em2cLockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            if (Rnd() % 3 == 0) {
                if (em2cToCeilingCk(em)) {
                    return;
                }
            }
            EmRoutineSet(em, 1, 5, 0, 0);
            return;
        }
    } else {
        w->lockCnt = 0;
    }
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// R1 == 0x01 Walk: walks towards the player along the route; picks the attacks by distance / angle
// (Atk 9, JumpAtk 0xA, BackKnuckle 0xB, TailAtk 0xC, AtkSign 8, Threat 7 when he is out of ammo),
// SideStep / BackJump (5 / 6) when aimed at, Dash (2) when far, Turn180 (3) when he is behind, ToHide
// (0x12); em2cNextWalkSet picks the follow-up.
static void em2c_R1_Walk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec pos;
    f32 ang;
    int seq;

    w->flags |= 0x100;
    w->flags |= 0x40000;
    switch (em->r_no_2) {
        do { } while (0);  // dead loop before the label: fresh `li 0` for the zero stores
    case 0:
        if (em->motFlags & 0x40) {
            w->blendSeq = 0xB;
        } else {
            w->blendSeq = 0x20;
        }
        w->blendM0 = ARC(8);
        w->blendM1 = ARC(0xE);
        w->blendM2 = ARC(0xD);
        w->blendA = (int) ARC(9);
        w->blendB = 0;
        w->blendD = 5;
        w->blendC = 0;
        w->blendCnt = 5;
        w->blendVal = 0.0f;
        em->r_no_2++;
    case 1:
        ang = Muku(&em->pos, &w->targetPos, em->ang.y, 3.14159274f);
        if (ang > 0.785398185f) {
            ang = 0.785398185f;
        }
        if (ang < -0.785398185f) {
            ang = -0.785398185f;
        }
        {
            f32 blend = ang * 324.676086f; // separate statement: pool order 324.676086, 0.8, 0.2
            w->blendVal = w->blendVal * 0.800000012f + blend * 0.200000003f;
        }
        em->ang.y += w->blendVal * 0.00392156886f * 0.0490873866f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em2cBlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendA, 0, 0, w->blendD);
        MotionMoveF(em, 0);
        break;
    }
    em2cDoorOpenCk(em);
    if (em2cJumpDownCk(em)) {
        return;
    }
    if (em2cWallOverCk(em)) {
        return;
    }
    if (em2cLockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            switch (pG->weapon_no) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 0x10:
            case 0x11:
            case 0x26:
            case 0x2B:
                if (Rnd() % 10 > 8) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                    return;
                }
                break;
            }
            if (Rnd() % 3 == 0) {
                if (em2cToCeilingCk(em)) {
                    return;
                }
            }
            EmRoutineSet(em, 1, 5, 0, 0);
            return;
        }
    } else {
        w->lockCnt = 0;
    }
    if (w->atkWait) {
        if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.785398185f) {
            EmRoutineSet(em, 1, 6, 0, 0);
        } else {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        return;
    }
    if (w->flags & 1) {
        f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
        f32 lim = 12250000.0f;
        if (plAng > 1.57079637f) {
            lim = 1690000.0f;
        }
        if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
            if (pPL->r_no_0 != 0 && pPL->r_no_1 != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                !(w->flags & 0x800)) {
                EmRoutineSet(em, 1, 7, 0, 0);
                return;
            }
            if (plAng > 1.57079637f) {
                GetPlPos(&pos, 0, 20.0f);
                if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) <
                    6250000.0f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                }
                return;
            }
            if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                EmRoutineSet(em, 1, 9, 0, 0);
                return;
            }
            if (Rnd() % 10 > 7 && plAng < 1.22173047f) {
                if (Rnd() % 10 > 8) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
                return;
            }
            switch (Rnd() % 3) {
            default:
                EmRoutineSet(em, 1, 0xB, 0, 0);
                return;
            case 1:
                if (plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    return;
                }
                // fallthrough into case 2: its label has two uses, so the arm gets fresh `li 1`s
            case 2:
                EmRoutineSet(em, 1, 0xA, 0, 0);
                return;
            }
        }
    }
    if (em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f && w->routeAngAbs > 1.04719758f &&
        w->routeAngAbs < 1.91986215f) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 1.0f;
        PSMTXMultVecSR(em->mat, &pos, &pos);
        em->ang.y = atan2f(pos.x, pos.z);
        EmRoutineSet(em, 1, 6, 0, 0);
        return;
    }
    if (em2cAmbushCk(em)) {
        return;
    }
    if (w->plDist > 10000.0f) {
        if (w->Dash_wait == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
            return;
        }
    } else if (w->Dash_wait == 0 && (w->flags & 1) && w->plDist > 5000.0f) {
        EmRoutineSet(em, 1, 2, 0, 0);
        return;
    }
    w->atkCnt++;
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// Wait / Turn180 end: the next floor routine by the target angle, the hide state and chance.
static inline void em2cNextWalkSet(cEm2c* em, Em2cWork* w, int r)
{
    if (w->targetAngAbs > 2.09439516f) {
        EmRoutineSet(em, 1, 3, r, r);
    } else if (w->flags & 0x800) {
        EmRoutineSet(em, 1, 0x18, r, r);
    } else if (em->plDist2 > 25000000.0f && Rnd() % 10 > 7 && pG->room_id == 0x221 &&
               fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) > 1.57079637f) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
    } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f && w->Dash_wait == 0) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
}

// R1 == 0x02 Dash: the charge along the route (Dash_wait afterwards), the same attack choice as Walk
// when in reach, SideStep when aimed at, else Wait / BackJump.
static void em2c_R1_Dash(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec pos;
    int over;

    w->flags |= 0x180;
    switch (em->r_no_2) {
        do { } while (0);  // dead loop before the label: fresh `li 0` for the zero stores
    case 0:
        if (em->motFlags & 0x40) {
            w->blendSeq = 8;
        } else {
            w->blendSeq = 0;
        }
        w->blendM0 = ARC(0xA);
        w->blendM1 = ARC(0xC);
        w->blendA = (int) ARC(0xB);
        w->blendB = 0;
        w->blendD = 5;
        w->blendCnt = 5;
        w->blendVal = 0.0f;
        if (em->plDist2 > 25000000.0f) {
            w->blendVal = 255.0f;
        }
        w->timer = 15;
        w->timer8 = 0;
        w->Dash_wait = 150;
        em->r_no_2++;
    case 1:
        em2cTurnTo(em, &w->routePos, 0.0981747732f);
        if (w->timer8 % 6 == 0) {
            EstSet((int) em, -1, 0, 0, 0x24, 0xE, 0, w->espKind2, (u32) em, 0);
        }
        if (em->plDist2 > 25000000.0f && pG->Game_level > 1) {
            w->blendVal += 16.0f;
            if (w->blendVal > 255.0f) {
                w->blendVal = 255.0f;
            }
        } else {
            w->blendVal -= 16.0f;
            if (w->blendVal < 0.0f) {
                w->blendVal = 0.0f;
            }
        }
        em2cBlendMotSet2I(em, w->blendM0, w->blendM1, w->blendA, 0, w->blendD);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            w->timer--;
        }
        w->timer8++;
        break;
    }
    em2cDoorOpenCk(em);
    if (em2cJumpDownCk(em)) {
        return;
    }
    over = em2cWallOverCk(em);
    if (over) {
        return;
    }
    if (em2cLockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            if (Rnd() % 10 > 5) {
                EmRoutineSet(em, 1, 5, over, over);
                return;
            }
            w->lockCnt = over;
        }
    } else {
        w->lockCnt = 0;
    }
    if (w->flags & 1) {
        f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
        f32 lim = 12250000.0f;
        if (plAng > 1.57079637f) {
            lim = 1690000.0f;
        }
        if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
            if (pPL->r_no_0 != 0 && pPL->r_no_1 != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                !(w->flags & 0x800)) {
                EmRoutineSet(em, 1, 7, 0, 0);
                return;
            }
            if (plAng > 1.57079637f) {
                GetPlPos(&pos, 0, 20.0f);
                if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) <
                    6250000.0f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                }
                return;
            }
            if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                EmRoutineSet(em, 1, 9, 0, 0);
                return;
            }
            if (Rnd() % 10 > 7 && plAng < 1.22173047f) {
                EmRoutineSet(em, 1, 8, 0, 0);
                return;
            }
            switch (Rnd() % 3) {
            default:
                EmRoutineSet(em, 1, 0xB, 0, 0);
                return;
            case 1:
                if (plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    return;
                }
                // fallthrough into case 2: its label has two uses, so the arm gets fresh `li 1`s
            case 2:
                EmRoutineSet(em, 1, 0xA, 0, 0);
                return;
            }
        }
    }
    if (em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f && w->routeAngAbs > 1.04719758f &&
        w->routeAngAbs < 1.91986215f) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 1.0f;
        PSMTXMultVecSR(em->mat, &pos, &pos);
        em->ang.y = atan2f(pos.x, pos.z);
        EmRoutineSet(em, 1, 6, 0, 0);
        return;
    }
    w->atkCnt++;
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// R1 == 0x03 Turn180: turns around towards the target, then Dash (2) or Walk (1).
static void em2c_R1_Turn180(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    f32 d;
    int r;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x800) {
            if (em->motFlags & 0x40) {
                MotionSetCore(em, &em->Motion, ARC(0x34), (int) ARC(0x35), 5, 0x41, 0);
            } else {
                MotionSetCore(em, &em->Motion, ARC(0x34), (int) ARC(0x35), 5, 1, 0);
            }
        } else if (em->motFlags & 0x40) {
            MotionSetCore(em, &em->Motion, ARC(0x13), (int) ARC(0x14), 5, 0x41, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x13), (int) ARC(0x14), 5, 1, 0);
        }
        w->turnAng = em->ang.y + 3.14159274f;
        w->timer = 60;
        em->r_no_2++;
    case 1:
        if (em->seFlags28B & 0x10) {
            d = Muku(&em->pos, &w->targetPos, w->turnAng, 0.0981747732f);
            w->turnAng += d;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->ang.y += d;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMoveF(em, 0)) {
            r = em2cAmbushCk(em);
            if (r) {
                break;
            }
            em2cNextWalkSet(em, w, r);
        }
        break;
    }
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// Wait end without the hide options: turn, dash or walk.
static inline void em2cNextWalkSet2(cEm2c* em, Em2cWork* w)
{
    if (w->targetAngAbs > 2.09439516f) {
        EmRoutineSet(em, 1, 3, 0, 0);
    } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
}

// Side-step start: the step motion mirrored towards the free side, with the sound / effect.
static inline void em2cSideStepMotSet(cEm2c* em, Em2cWork* w, int side)
{
    if (side) {
        MotionSetCore(em, &em->Motion, ARC(0xF), (int) ARC(0x10), 5, 0x41, 0);
    } else {
        MotionSetCore(em, &em->Motion, ARC(0xF), (int) ARC(0x10), 5, 1, 0);
    }
    EstSet((int) em, -1, 0, 0, 0x24, 9, 0, w->espKind2, (u32) em, 0);
}

// R1 == 0x04 Ambush: waits at an em2c_ambush_pos (em2cAmbushCk) until the player comes within 10000,
// then springs out (motion 0xF mirrored by side, effect / roar) and walks.
static void em2c_R1_Ambush(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    w->flags |= 0x180;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(7), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if (em->plDist2 < 100000000.0f) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f) < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0xF), (int) ARC(0x10), 5, 0x41, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0xF), (int) ARC(0x10), 5, 1, 0);
        }
        EstSet((int) em, -1, 0, 0, 0x24, 9, 0, w->espKind2, (u32) em, 0);
        SndCall(8, 0x40, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em2cNextWalkSet2(em, w);
        }
        break;
    }
}

// R1 == 0x05 SideStep: dodges to the free side (wall probes), then the attack choice / Dash / Walk.
static void em2c_R1_SideStep(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec a;
    Vec b;
    f32 plAng;
    f32 lim;
    int side;

    w->flags |= 0x180;
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
            side = 1;
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
            side = 0;
        }
        em2cSideStepMotSet(em, w, side);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x40, &em->pos, em->id, 0, em);
        em->r_no_2++;
    case 1:
        if ((w->flags & 1) && w->routeAngAbs < 0.52359879f) {
            em2cTurnTo(em, &w->routePos, 0.0981747732f);
        }
        if (MotionMoveF(em, 0)) {
            em2cNextWalkSet2(em, w);
            break;
        }
        if ((em->seFlags28B & 4) && (w->flags & 1)) {
            plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
            lim = 12250000.0f;
            if (plAng > 1.57079637f) {
                lim = 4000000.0f;
            }
            if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                if (pPL->r_no_0 != 0 && pPL->r_no_1 != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                    !(w->flags & 0x800)) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                    break;
                }
                if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                    break;
                }
                if (Rnd() % 10 > 7 && plAng < 1.22173047f) {
                    if (Rnd() % 10 > 8) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                    break;
                }
                switch (Rnd() % 3) {
                default:
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                    break;
                case 1:
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    break;
                case 2:
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                    break;
                }
            } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x06 BackJump: hops back (flag 0x80 jumping), then ToHide (0x12) in room 221 when the player
// looks away, Threat (7) when he is out of ammo, else Walk.
static void em2c_R1_BackJump(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;
    int r;

    w->flags |= 0x80;
    if (em->r_no_3 == 0) {
        w->flags |= 0x100;
    }
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x27), (int) ARC(0x28), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x2B, 0, 0, (u32) em, (void*) fe);
        w->timer = 10;
        em->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
            em2cTurnTo(em, &w->routePos, 0.196349546f);
        }
        if (MotionMoveF(em, 0)) {
            r = em2cAmbushCk(em);
            if (r) {
                break;
            }
            if (em->plDist2 > 25000000.0f && Rnd() % 10 > 5 && pG->room_id == 0x221 &&
                fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) > 1.57079637f) {
                EmRoutineSet(em, 1, 0x12, r, r);
            } else if (pPL->r_no_0 != 0 && pPL->r_no_1 != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                       !(w->flags & 0x800)) {
                EmRoutineSet(em, 1, 7, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x07 Threat: the roar at the player (flags 0x50000), then an attack when he is in reach,
// Dash / Turn180 / Walk.
static void em2c_R1_Threat(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    f32 plAng;
    f32 lim;
    int fe;

    w->flags |= 0x50000;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x44), 0, 5, 1, 0);
        w->dmgTotal = fe;
        w->timer = 20;
        em->r_no_2++;
    case 1:
        em2cTurnTo(em, &pPL->pos, 0.157079637f);
        if (MotionMoveF(em, 0)) {
            if (w->flags & 1) {
                plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
                lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->frame > 24.7000008f && em->frame < 25.2999992f) {
            SndCall(8, 0x45, &em->pos, em->id, 0, em);
        }
        break;
    }
}

// R1 == 0x08 AtkSign: the wind-up before an attack; on its key frame (rank > 3, half the time)
// attacks directly (Atk / BackKnuckle / TailAtk / JumpAtk), else Dash / Walk.
static void em2c_R1_AtkSign(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    w->flags |= 0x50000;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x25), (int) ARC(0x26), 5, 1, 0);
        w->timer = 20;
        em->r_no_2++;
    case 1:
        em2cTurnTo(em, &pPL->pos, 0.157079637f);
        end = MotionMoveF(em, 0);
        if (end) {
            if (w->flags & 1) {
                f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
                f32 lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if ((em->seFlags28B & 4) && (Rnd() & 1) && pG->Game_level > 3) {
            if (w->flags & 1) {
                f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
                f32 lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, end, end);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x09 Atk: the double claw slash (random side, flags 0x80 | 0x10000): em2cClawAtkCk on the
// hit frames; the player can escape / duck with the action button (em2cEscapeAction / em2cSitAction);
// then BackJump (6), ToHide (0x12) or Wait.
static void em2c_R1_Atk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x15), (int) ARC(0x16), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x15), (int) ARC(0x16), 10, 0x41, 0);
        }
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 0x10, 0, 0, (u32) em, (void*) zero);
        w->atkCnt = zero;
        w->timer = 15;
        w->atkHit = zero;
        em->flag &= ~4;
        em->r_no_2++;
    case 1:
        if (em->r_no_3 == 0 && w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &pPLS->pos, em->ang.y, 0.157079637f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(LVADD_ESCAPEATTACK);
            }
            w->dmgTotal = 0;
            w->atkWait = 5;
            if (w->atkHit) {
                if (em->plDist2 < 4000000.0f) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
                if (pG->room_id == 0x221) {
                    EmRoutineSet(em, 1, 0x12, 0, 0);
                    break;
                }
            }
            EmRoutineSet(em, 1, 0, 0, 0);
        } else if (em->seFlags28B & 1) {
            em2cDoorOpenCk2(em);
            if (em->motFlags & 0x40) {
                em2cAtkCk(em, 0, 0xB);
                em2cAtkCk(em, 0, 0xC);
                em2cAtkCk(em, 0, 0xD);
            } else {
                em2cAtkCk(em, 0, 7);
                em2cAtkCk(em, 0, 8);
                em2cAtkCk(em, 0, 9);
            }
        }
        break;
    }
}

// Attack end: the next routine by whether it hit (side-step / hide close by, else wait).
static inline void em2cAtkEndSet(cEm2c* em, Em2cWork* w)
{
    if (w->atkHit == 0) {
        GameAddPoint(LVADD_ESCAPEATTACK);
    }
    w->dmgTotal = 0;
    w->atkWait = 5;
    if (w->atkHit) {
        if (em->plDist2 < 4000000.0f) {
            EmRoutineSet(em, 1, 6, 0, 0);
            return;
        }
        if (pG->room_id == 0x221) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
            return;
        }
    }
    EmRoutineSet(em, 1, 0, 0, 0);
}

// Attack hit frames: the claw parts of the swinging side.
static inline void em2cClawAtkCk(cEm2c* em, int no)
{
    if (em->motFlags & 0x40) {
        em2cAtkCk(em, no, 0xB);
        em2cAtkCk(em, no, 0xC);
        em2cAtkCk(em, no, 0xD);
    } else {
        em2cAtkCk(em, no, 7);
        em2cAtkCk(em, no, 8);
        em2cAtkCk(em, no, 9);
    }
}

// R1 == 0x0A JumpAtk: the leaping claw attack: em2cClawAtkCk on landing; the duck action button
// (em2cSitAction) lets the player avoid it; then the walk.
static void em2c_R1_JumpAtk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int hit;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, &em->Motion, ARC(0x17), (int) ARC(0x18), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x17), (int) ARC(0x18), 10, 0x41, 0);
        }
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 0x14, 0, w->espKind2, (u32) em, (void*) zero);
        w->timer = 3;
        w->timer8 = 25;
        w->walkMode = 5;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->flag &= ~4;
        em->r_no_2++;
    case 1:
        if (w->walkMode) {
            w->walkMode--;
        } else if (w->timer8) {
            hit = w->atkHit;
            if (hit == 0) {
                w->timer8--;
                if (fabsf(Muku(&pPLS->pos, &em->pos, pPLS->ang.y, 3.14159274f)) < 1.04719758f) {
                    ActBtn.set(0x25, 0xB, (int) em2cEscapeAction, (int) em, 1, 3, 0, hit);
                } else {
                    ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 1, 3, 0, hit);
                }
            }
        }
        if (w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &pPLS->pos, em->ang.y, 0.157079637f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMoveF(em, 0)) {
            em2cAtkEndSet(em, w);
        } else if (em->seFlags28B & 1) {
            em2cClawAtkCk(em, 1);
        }
        break;
    }
}

// R1 == 0x0B BackKnuckle: the spinning back-hand swipe (random side), em2cClawAtkCk on the hit
// frames, then the walk.
static void em2c_R1_BackKnuckle(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int fe;
    int hit;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x19), (int) ARC(0x1A), 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x14, 0, w->espKind2, (u32) em, (void*) fe);
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        w->timer = 15;
        w->timer8 = 25;
        w->walkMode = 5;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->flag &= ~4;
        em->r_no_2++;
    case 1:
        if (w->walkMode) {
            w->walkMode--;
        } else if (w->timer8) {
            hit = w->atkHit;
            if (hit == 0) {
                w->timer8--;
                ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 1, 3, 0, hit);
            }
        }
        if (MotionMoveF(em, 0)) {
            em2cAtkEndSet(em, w);
        } else if (em->seFlags28B & 1) {
            em2cClawAtkCk(em, 2);
        }
        break;
    }
}

// R1 == 0x0C TailAtk: the tail sweep (flags 0x80 | 0x30000): em2cAtkCk kind 5 along the seventeen
// tail parts 0x3C..0x4C (mode = already hit); the escape action button lets the player roll away.
static void em2c_R1_TailAtk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int hit;

    w->flags |= 0x80;
    w->flags |= 0x30000;
    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, &em->Motion, ARC(0x1B), (int) ARC(0x1C), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x1B), (int) ARC(0x1C), 10, 0x41, 0);
        }
        zero = 0;
        w->atkCnt = zero;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 0x12, 0, w->espKind2, (u32) em, (void*) zero);
        w->timer = 15;
        w->timer8 = 25;
        w->walkMode = 5;
        w->atkHit = zero;
        em->flag &= ~4;
        w->mode = zero;
        em->r_no_2++;
    case 1:
        if (w->walkMode) {
            w->walkMode--;
        } else if (w->timer8) {
            hit = w->mode;
            if (hit == 0) {
                w->timer8--;
                ActBtn.set(0x25, 0xB, (int) em2cEscapeAction, (int) em, 1, 3, 0, hit);
            }
        }
        if (MotionMoveF(em, 0)) {
            em2cAtkEndSet(em, w);
            break;
        }
        if (pPL->r_no_0 == 1 && (pPL->dmg.m_Timer & 0x80)) {
            pPL->dmg.m_Timer = 8;
        }
        if (em->seFlags28B & 1) {
            w->atkHit = 0;
            em->flag &= ~4;
            em2cAtkCk(em, 5, 0x3C);
            em2cAtkCk(em, 5, 0x3D);
            em2cAtkCk(em, 5, 0x3E);
            em2cAtkCk(em, 5, 0x3F);
            em2cAtkCk(em, 5, 0x40);
            em2cAtkCk(em, 5, 0x41);
            em2cAtkCk(em, 5, 0x42);
            em2cAtkCk(em, 5, 0x43);
            em2cAtkCk(em, 5, 0x44);
            em2cAtkCk(em, 5, 0x45);
            em2cAtkCk(em, 5, 0x46);
            em2cAtkCk(em, 5, 0x47);
            em2cAtkCk(em, 5, 0x48);
            em2cAtkCk(em, 5, 0x49);
            em2cAtkCk(em, 5, 0x4A);
            em2cAtkCk(em, 5, 0x4B);
            em2cAtkCk(em, 5, 0x4C);
            if (w->atkHit) {
                w->mode = 1;
            }
        }
        break;
    }
}

// Player damage routine of the boss's killing blow: the head comes off (em2cPlHeadLost), routine held.
static void plem2c_CriticalHit(cPlayer* pl)
{
    StaFlagOn(pG, STA_PL_CATCHED);
    pl->dmg.set(0, 10);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC_PTR(pG->pPlayer, 0x4C), (int) PL_ARC_PTR(pG->pPlayer, 0x4D), 5, 1, 0);
        em2cPlHeadLost();
        pl->r_no_2++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

// R1 == 0x0D SwayBack: sways back after a hit / blocked attack (one of three motions, flag 0x80000),
// then attacks when the player is in reach or Dash / Walk.
static void em2c_R1_SwayBack(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    switch (em->r_no_2) {
    case 0:
        switch (Rnd() % 3) {
        case 0:
        default:
            MotionSetCore(em, &em->Motion, ARC(0x7D), (int) ARC(0x7E), 4, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->Motion, ARC(0x7F), (int) ARC(0x80), 4, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->Motion, ARC(0x7F), (int) ARC(0x80), 4, 0x41, 0);
            break;
        }
        EstSet((int) em, -1, 0, 0, 0x24, 0xB, 0, w->espKind2, (u32) em, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x40, &em->pos, em->id, 0, em);
        em->dmg.m_Timer = 3;
        em->r_no_2++;
    case 1:
        if (em->seFlags28B & 1) {
            w->flags |= 0x80000;
        } else {
            w->flags |= 0x40000;
        }
        end = MotionMoveF(em, 0);
        if (end) {
            em2cNextWalkSet2(em, w);
            break;
        }
        if ((em->seFlags28B & 4) && (Rnd() & 1)) {
            if (w->flags & 1) {
                f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
                f32 lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, end, end);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x0E WakeupWait: lies on its back 30..60 frames (flag 0x400), then Wakeup (0xF).
static void em2c_R1_WakeupWait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x400;
    switch (em->r_no_2) {
    case 0:
        w->x56C = Rnd() % 30 + 30;
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if ((w->flags & 0x800) && w->guardCnt == 0) {
            w->x56C = w->guardCnt;
        }
        if (w->x56C) {
            w->x56C--;
            break;
        }
        r = em2cDownJumpCk(em);
        if (r) {
            break;
        }
        EmRoutineSet(em, 1, 0xF, r, r);
        break;
    }
}

// R1 == 0x0F Wakeup: rights itself (flags 0x8100); a frozen boss whose ice ran out (guardCnt 0)
// breaks free (F_Clear 0x1A), else Turn180 / SideStep / Walk.
static void em2c_R1_Wakeup(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x8100;
    switch (em->r_no_2) {
    case 0:
        em->r_no_3 = 0;
        if (w->guardCnt <= 0 || !(w->flags & 0x800)) {
            MotionSetCore(em, &em->Motion, ARC(0x77), (int) ARC(0x78), 5, 1, 0);
            em->r_no_3 = 1;
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x42), (int) ARC(0x43), 5, 1, 0);
        }
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (w->flags & 0x800) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            } else {
                r = em2cLockCk(em);
                if (r) {
                    EmRoutineSet(em, 1, 5, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 1, r, r);
                }
            }
            break;
        }
        if (em->r_no_3 && (w->flags & 0x800)) {
            w->flags |= 0x8000;
            if (em->seFlags28B & 1) {
                w->flags &= ~0x800;
                SndCall(8, 0x35, &em->pos, em->id, 0, em);
                SndCall(8, 0x36, &em->pos, em->id, 0, em);
                if (w->pTex) {
                    em->pModelInfo->setTexBlendTbl(w->texBlend);
                    em->pModelInfo->setBlendRatio(0);
                    em->pModelInfo->setBlendType(2);
                }
            }
        }
        if ((em->seFlags28B & 4) && w->guardCnt == 0) {
            if (em->seFlags28B & 0x80) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1A, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x10 DownJump: leaps up onto a wall (em2cDownJumpCk), snaps onto it (flags 0x120) and
// continues as W_Turn180 (0x1F) / W_Walk (0x1D).
static void em2c_R1_DownJump(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
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
        PSMTXRotRad(m, 'x', 3.14159274f);
        em->pos.y = hit.y;
        TransMatrix(em->mat, &em->pos);
        PSMTXConcat(em->mat, m, em->mat);
        em->r_no_2++;
    case 2:
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(EM_STATUS_IK_OFF);
        em2cSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = -1.0f;
            w->wallNrm.z = 0.0f;
            if (plPos.z < -500.0f) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
            EmRoutineSet(em, 1, 0x1D, 0, 0);
        }
        break;
    }
}

// R1 == 0x11 ToCeiling: leaps up to the ceiling (em2cToCeilingCk), snaps onto it and continues as a
// wall walker.
static void em2c_R1_ToCeiling(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;
    int fe;

    w->flags |= 0x100;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x79), (int) ARC(0x7A), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x2C, 0, 0, (u32) em, (void*) fe);
        em->r_no_2++;
    case 1:
        if (em->seFlags28B & 1) {
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
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x64), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(EM_STATUS_IK_OFF);
        em2cSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = -1.0f;
            w->wallNrm.z = 0.0f;
            if (plPos.z < -500.0f) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
            EmRoutineSet(em, 1, 0x1D, 0, 0);
        }
        break;
    }
}

// R1 == 0x12 ToHide: jumps up out of the player's view (flags 0x8100, airborne) and lands on the
// ceiling above him, then HideWait (0x13).
static void em2c_R1_ToHide(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;
    int fe;

    w->flags |= 0x8100;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x79), (int) ARC(0x7A), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x2C, 0, 0, (u32) em, (void*) fe);
        em->r_no_2++;
    case 1:
        if (em->seFlags28B & 1) {
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
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->Motion, ARC(0x64), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        em->invisible_factor -= 0.100000001f;
        em->dmg.m_Timer = 2;
        if (em->invisible_factor <= 0.0f) {
            em->invisible_factor = 0.0f;
            em->be_flag &= ~2;
        }
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(EM_STATUS_IK_OFF);
        em2cSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            EmRoutineSet(em, 1, 0x13, 0, 0);
        }
        break;
    }
}

// R1 == 0x13 HideWait: lurks unseen 4000 above the player, following him (GetPlPos, flags 0x8000 |
// 0x220000 = hidden, no damage) for 60..120 frames (longer near a door, em2cDoorCk), then the tail
// attack from above (HideAtk 0x14, half the time when the tail is alive) or drops (HideFall 0x15).
static void em2c_R1_HideWait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int r;
    int t;

    w->flags |= 0x8000;
    w->flags |= 0x220000;
    em2cGetTail(em);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->r_no_2++;
        w->mode = 3;
        w->x6B8 = fe;
        AtariOff(&em->atari, 0xFCFF);
        em->invisible_factor = 1.0f;
        em->be_flag |= 2;
        w->timer = Rnd() % 60 + 60;
        w->timer8 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        r = em2cDoorCk(em);
        if (r) {
            w->timer = Rnd() % 6 * 30 + 150;
            w->timer8++;
        } else {
            w->timer8 = r;
        }
        t = w->timer;
        if (t == 0) {
            if (w->pTail && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x14, t, t);
                break;
            }
        } else {
            w->timer = t - 1;
            asm volatile("");  // COMPILER-DIFF: #15 candidate: the original keeps the same-base `lwz timer8` below the `stw timer` (r0 for both); ours hoists it (a dead do-while keeps the order but gives the load r9)
            if (w->timer8 <= 90) {
                break;
            }
        }
        EmRoutineSet(em, 1, 0x15, 0, 0);
        break;
    }
}

// R1 == 0x14 HideAtk: the tail strikes down from hiding (mode 1..3 = the strike variant, the tail
// object pTail animated), effects 1 / 6; the player can dodge with the action button
// (em2cBackjumpAction2 / em2cSitAction); then HideFall (0x15).
static void em2c_R1_HideAtk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int t;

    w->flags |= 0x8000;
    w->flags |= 0x120000;
    em2cGetTail(em);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->r_no_2++;
        w->x6B8 = fe;
        w->mode = Rnd() % 3 + 1;
        AtariOff(&em->atari, 0xFCFF);
        w->pTail->flag &= ~4;
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, (void*) fe);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, (void*) fe);
        w->timer = 24;
        w->timer8 = 24;
        em->r_no_3 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (w->pTail->flag & 4) {
            em->r_no_3 = 1;
        }
        t = w->timer;
        if (t) {
            w->timer = t - 1;
        } else if (em->r_no_3) {
            EmRoutineSet(em, 1, 0x15, t, t);
            break;
        } else if (w->mode) {
            w->mode--;
            w->timer = 30;
            w->timer8 = 15;
            if (w->x6B8) {
                GetPlPos(&w->pTail->pos, 0, 0.0f);
            } else {
                GetPlPos(&w->pTail->pos, 0, 10.0f);
            }
            w->pTail->pos.y = pPLS->pos.y + 4000.0f;
            w->pTail->ang.y += pPLS->ang.y + 3.14159274f;
            w->pTail->ang.y = LIMIT_ANGLE(em->ang.y);
            w->pTail->flag |= 1;
            w->pTail->flag &= ~4;
            w->x6B8 = 0;
        } else {
            em->r_no_2++;
            break;
        }
        if (w->timer < w->timer8) {
            if (w->x6B8 == 0) {
                if (em->r_no_3 == 0) {
                    if (w->mode) {
                        ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction2, (int) em, 2, 3, 0, em->r_no_3);
                    } else {
                        ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 2, 4, 0, w->mode);
                    }
                }
            } else {
                w->timer = 0;
            }
        }
        if (w->timer % 3) {
            EstSet(0, -1, &em->pos, 0, 0x24, 0xC, 0, 0, 0, 0);
        }
        break;
    case 2:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y = pPLS->pos.y + 4000.0f;
            em->ang.y += Muku(&em->pos, &pPLS->pos, em->ang.y, 3.14159274f) + 3.14159274f;
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        MotionSetCore(em, &em->Motion, ARC(0x8A), (int) ARC(0x8B), 0, 5, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x21, 0, 0, (u32) em, 0);
        w->timer = 10;
        em->r_no_2++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->x6B8 == 0 && em->r_no_3 == 0) {
                ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 2, 4, 0, em->r_no_3);
            }
        }
        if (em->seFlags28B & 1) {
            em2cAtkCk(em, 4, 0xB);
            em2cAtkCk(em, 4, 0xC);
            em2cAtkCk(em, 4, 0xD);
        }
        break;
    }
}

// R1 == 0x15 HideFall: drops out of hiding onto the floor (flags 0x8000 | 0x500000, falling 0x1000),
// lands with the landing effect and continues with Turn180 / Dash / Walk.
static void em2c_R1_HideFall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 fl;
    int end;

    w->flags |= 0x8000;
    w->flags |= 0x500000;
    switch (em->r_no_2) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em2cGetFallPos(em);
        em->invisible_factor = 0.0f;
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        w->timer = 30;
        em->r_no_2++;
    case 1:
        em->dmg.m_Timer = 2;
        MotionSetCore(em, &em->Motion, ARC(0x89), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        em->dmg.m_Timer = 2;
        if (w->timer) {
            w->timer--;
            break;
        }
        em->r_no_2++;
        break;
    case 2:
        AtariOff(&em->atari, 0xFCFF);
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, 0);
        MotionSetCore(em, &em->Motion, ARC(0x89), 0, 5, 1, 0);
        em->r_no_2++;
    case 3:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        em->invisible_factor += 0.100000001f;
        if (em->invisible_factor > 1.0f) {
            em->invisible_factor = 1.0f;
        }
        em->dmg.m_Timer = 2;
        end = MotionMoveF(em, 0);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            em->invisible_factor = 1.0f;
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 6;
        } else if (end) {
            AtariOn(&em->atari, 0x300);
            em->r_no_2++;
        }
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x63), 0, 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = -200.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 5:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 6;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 6:
        AtariOn(&em->atari, 0x300);
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = 1.0f;
        w->wallNrm.z = 0.0f;
        em->r_no_2++;
    case 7:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x16 JumpDown: jumps off an edge (em2cJumpDownCk), lands below, then Turn180 / Dash-walk.
static void em2c_R1_JumpDown(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec spd;
    Vec rot;
    f32 fl;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x57), (int) ARC(0x59), 5, 0, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->jumpAng, 0.196349546f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        if (em->seFlags28B & 1) {
            w->flags |= 0x40;
            em->dmg.m_Timer = 2;
            spd.x *= 0.5f;
            spd.z *= 0.5f;
        }
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x57), (int) ARC(0x5A), 5, 4, 0);
        w->spd.x = 0.0f;
        w->spd.y = -450.0f;
        w->spd.z = 30.0f;
        em->r_no_2++;
    case 3:
        w->flags |= 0x40;
        em->dmg.m_Timer = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            em->r_no_2++;
        }
        MotionMoveF(em, 0);
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x57), (int) ARC(0x5B), 5, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (w->flags & 0x800) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x1B WallOver: jumps over a low wall (em2cWallOverCk), then Turn180 / W_ or floor walk.
static void em2c_R1_WallOver(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec d;
    int t;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->jumpAng, 0.392699093f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMoveF(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
        w->timer = 30;
        em->r_no_2++;
    case 3:
        w->flags |= 0x20;
        em->setStatus(EM_STATUS_IK_OFF);
        em2cSetWallMatrix2(em, 0.400000006f);
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
            } else if (w->flags & 0x800) {
                EmRoutineSet(em, 1, 0x18, t, t);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 1: frozen (F_*), wall (W_*)

// R1 == 0x17 F_Wait: the frozen boss (flag 0x800, em2cSetFreeze) stands stiff (motion 0x2B) until
// atkWait runs out, then F_Walk (0x18) or Turn180 (3); dies to any fall.
static void em2c_R1_F_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int wait;

    w->flags |= 0x180;
    switch (em->r_no_2) {
    case 0:
        if (em->motFlags & 0x40) {
            MotionSetCore(em, &em->Motion, ARC(0x2B), 0, 30, 0x45, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x2B), 0, 30, 5, 0);
        }
        em->r_no_2++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        wait = w->atkWait;
        if (wait) {
            break;
        }
        if (w->targetAngAbs > 2.09439516f) {
            EmRoutineSet(em, 1, 3, wait, wait);
        } else {
            EmRoutineSet(em, 1, 0x18, wait, wait);
        }
        break;
    }
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// R1 == 0x18 F_Walk: the frozen boss shuffles towards the player (blend motions 0x2C / 0x32 / 0x33
// by turn angle), attacks with F_Atk (0x19) when he is within 2000 in front, and breaks the ice
// (F_Clear 0x1A) when guardCnt reaches 0.
static void em2c_R1_F_Walk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    f32 ang;
    int seq;
    int wait;
    int r;

    w->flags |= 0x100;
    switch (em->r_no_2) {
        do { } while (0);  // dead loop before the label: the arm does not know xFE == 0 (fresh `li 0` for the zero stores)
    case 0:
        if (em->motFlags & 0x40) {
            w->blendSeq = 0xB;
        } else {
            w->blendSeq = 0x22;
        }
        w->blendM0 = ARC(0x2C);
        w->blendM1 = ARC(0x33);
        w->blendM2 = ARC(0x32);
        w->blendA = (int) ARC(0x2D);
        w->blendB = 0;
        w->blendD = 5;
        w->blendC = 0;
        w->blendCnt = 5;
        w->blendVal = 0.0f;
        em->r_no_2++;
    case 1:
        ang = Muku(&em->pos, &w->targetPos, em->ang.y, 3.14159274f);
        if (ang > 1.04719758f) {
            ang = 1.04719758f;
        }
        if (ang < -1.04719758f) {
            ang = -1.04719758f;
        }
        {
            f32 blend = ang * 243.50705f; // separate statement: pool order 243.50705, 0.8, 0.2
            w->blendVal = w->blendVal * 0.800000012f + blend * 0.200000003f;
        }
        em->ang.y += w->blendVal * 0.00392156886f * 0.0490873866f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        em2cBlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendA, 0, 0, w->blendD);
        MotionMoveF(em, 0);
        break;
    }
    em2cDoorOpenCk(em);
    if (em2cJumpDownCk(em)) {
        return;
    }
    r = em2cWallOverCk(em);
    if (r) {
        return;
    }
    wait = w->atkWait;
    if (wait) {
        EmRoutineSet(em, 1, 0, r, r);
        return;
    }
    if ((w->flags & 1) && em->plDist2 < 4000000.0f && w->routeAngAbs < 0.52359879f) {
        EmRoutineSet(em, 1, 0x19, wait, wait);
        return;
    }
    if (em2cFallCk(em)) {
        return;
    }
    {
        int g = w->guardCnt;  // its own variable: reusing `r` would make it cse's canonical zero for the 0x19 stores

        if (g == 0) {
            EmRoutineSet(em, 1, 0x1A, g, g);
            return;
        }
    }
    em2cBreathSe(em);
}

// R1 == 0x19 F_Atk: the frozen boss's slow slash (mode picks the motion), em2cAtkCk on the hit
// frames, then F_Wait (0x17).
static void em2c_R1_F_Atk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    switch (em->r_no_2) {
    case 0:
        w->mode = Rnd() & 1;
        if (w->mode) {
            MotionSetCore(em, &em->Motion, ARC(0x38), (int) ARC(0x39), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x36), (int) ARC(0x37), 10, 1, 0);
        }
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        w->atkCnt = zero;
        w->timer = 15;
        w->atkHit = zero;
        em->flag &= ~4;
        em->r_no_2++;
    case 1:
        if (em->r_no_3 == 0 && w->timer) {
            w->timer--;
            em->ang.y += Muku(&em->pos, &pPLS->pos, em->ang.y, 0.157079637f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMoveF(em, 0)) {
            w->atkWait = 30;
            w->dmgTotal = 0;
            EmRoutineSet(em, 1, 0x17, 0, 0);
        } else if (em->seFlags28B & 1) {
            em2cDoorOpenCk2(em);
            if (w->mode) {
                em2cAtkCk(em, 6, 7);
                em2cAtkCk(em, 6, 8);
                em2cAtkCk(em, 6, 9);
            } else {
                em2cAtkCk(em, 6, 0xB);
                em2cAtkCk(em, 6, 0xC);
                em2cAtkCk(em, 6, 0xD);
            }
        }
        break;
    }
}

// R1 == 0x1A F_Clear: shatters the ice (flags 0x8080, the ice texture blend cleared), then Turn180 /
// Dash / Walk at full speed.
static void em2c_R1_F_Clear(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    w->flags |= 0x8080;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x30), (int) ARC(0x31), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x26, 0, 0, (u32) em, (void*) fe);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            w->atkWait = 30;
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em->plDist2 > 25000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->seFlags28B & 1) {
            w->flags &= ~0x800;
            if (w->pTex) {
                em->pModelInfo->setTexBlendTbl(w->texBlend);
                em->pModelInfo->setBlendRatio(0);
                em->pModelInfo->setBlendType(2);
            }
        }
        break;
    }
}

// R1 == 0x1C W_Wait: idle on the wall / ceiling (flags 0x120), then W_Walk (0x1D) or W_Turn180 (0x1F).
static void em2c_R1_W_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec plPos;

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x60), 0, 30, 5, 0);
        em->r_no_2++;
    case 1:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        em2cSetWallMatrix2(em, 0.400000006f);
        MotionMoveF(em, 0);
        if (w->atkWait == 0) {
            EmRoutineSet(em, 1, 0x1D, 0, 0);
        } else if (plPos.z < -8000.0f) {
            EmRoutineSet(em, 1, 0x1F, 0, 0);
        }
        break;
    }
    em2cBreathSe(em);
}

// R1 == 0x1D W_Walk: walks the wall / ceiling (em2cSetWallMatrix2) towards the player; W_Atk (0x1E)
// in reach, the drop attack (W_Fall 0x20), W_Turn180 when he is behind, back to the floor attack (9)
// or BackJump (6) when low.
static void em2c_R1_W_Walk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx m;
    Mtx inv;
    Vec plPos;
    Vec d;
    f32 ang;
    f32 ny;
    int t;
    int r;

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
        w->mode = Rnd() & 3;
        if (em->r_no_3) {
            w->turnAng = em2cGetPlDir(em, &w->wallTarget);
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
        em2cSetWallMatrix2(em, 0.400000006f);
        if (MotionMoveF(em, 0)) {
            t = w->timer8;
            if (t) {
                w->timer8 = t - 1;
            } else if (w->wallNrm.y > 0.899999976f || w->wallNrm.y < -0.899999976f) {
                w->atkWait = Rnd() % 30 + 30;
                EmRoutineSet(em, 1, 0x1C, t, t);
                break;
            }
        }
        if (em2cNoWallCk(em) && w->wallNrm.y < 0.699999988f) {
            EmRoutineSet(em, 1, 0x20, 0, 0);
            break;
        }
        ny = w->wallNrm.y;
        if ((w->flags & 1) && plPos.x > -300.0f && plPos.x < 300.0f && plPos.y > 0.0f && plPos.y < 2200.0f &&
            plPos.z > 0.0f && plPos.z < 2000.0f) {
            if (ny > 0.899999976f) {
                d.x = 0.0f;
                d.y = 0.0f;
                d.z = 1.0f;
                PSMTXMultVecSR(em->mat, &d, &d);
                em->ang.y = atan2f(d.x, d.z);
                EmRoutineSet(em, 1, 9, 0, 0);
                break;
            }
            if (ny < -0.899999976f || (ny > -0.100000001f && ny < 0.100000001f)) {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
                break;
            }
        }
        if (ny < -0.899999976f && em->plDist2 < 250000.0f) {
            if (Rnd() % 10 > 2) {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x20, 0, 0);
            }
            break;
        }
        t = w->flags & 0x2000;
        if (t) {
            EmRoutineSet(em, 1, 0x20, 0, 0);
            break;
        }
        if (plPos.z < -8000.0f) {
            EmRoutineSet(em, 1, 0x1F, t, t);
        }
        r = em2cWallFallCk(em);
        if (r) {
            EmRoutineSet(em, 1, 0x20, t, t);
            break;
        }
        if (w->wallNrm.y > 0.899999976f && em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f &&
            w->routeAngAbs > 1.04719758f && w->routeAngAbs < 1.91986215f) {
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = 1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
            em->ang.y = atan2f(d.x, d.z);
            EmRoutineSet(em, 1, 6, r, r);
        }
        break;
    }
    w->atkCnt++;
    em2cBreathSe(em);
}

// R1 == 0x1E W_Atk: the slash from the wall (em2cAtkCk), then W_Fall (0x20), W_Wait or W_Walk.
static void em2c_R1_W_Atk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec d;
    int zero;
    int fe;

    w->flags |= 0x20;
    em->setStatus(EM_STATUS_IK_OFF);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        d.x = 0.0f;
        d.y = 1.0f;
        d.z = 0.0f;
        PSMTXMultVecSR(em->mat, &d, &d);
        if (d.y > -0.600000024f) {
            em->r_no_3 = 1;
            MotionSetCore(em, &em->Motion, ARC(0x81), (int) ARC(0x82), 10, 1, 0);
        } else {
            em->r_no_3 = fe;
            MotionSetCore(em, &em->Motion, ARC(0x49), (int) ARC(0x4A), 10, 1, 0);
        }
        zero = 0;
        w->timer = 10;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->flag &= ~4;
        em->r_no_2++;
    case 1:
        em2cSetWallMatrix2(em, 0.400000006f);
        if (MotionMoveF(em, 0)) {
            if (Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x20, 0, 0);
            } else if (w->atkHit) {
                em2cSetAtkWait(w, 60, 45, 40, 35, 30);
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1D, w->atkHit, w->atkHit);
            }
        } else if (em->seFlags28B & 1) {
            if (em->r_no_3) {
                em2cAtkCk(em, 3, 0xB);
                em2cAtkCk(em, 3, 0xC);
                em2cAtkCk(em, 3, 0xD);
            } else {
                em2cAtkCk(em, 3, 7);
                em2cAtkCk(em, 3, 8);
                em2cAtkCk(em, 3, 9);
            }
        }
        break;
    }
}

// R1 == 0x1F W_Turn180: turns around on the wall, then W_Wait (0x1C).
static void em2c_R1_W_Turn180(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;

    w->flags |= 0x120;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x47), (int) ARC(0x48), 5, 0, 0);
        em->r_no_2++;
    case 1:
        MotionGetSpeed(em, &em->Motion, 0, &spd, &rot);
        RotMatrix(m, &rot);
        PSMTXConcat(em->mat, m, em->mat);
        MotionAddSpeed(em, &em->Motion, &spd, &rot);
        em2cSetWallMatrix2(em, 0.400000006f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x1C, 0, 0);
        }
        break;
    }
    em2cBreathSe(em);
}

// R1 == 0x20 W_Fall: drops off the wall / ceiling (flags 0x140, falling 0x1000) onto the floor
// (em2cSetFallMatrix, landing effect), then Turn180 / Walk.
static void em2c_R1_W_Fall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 fl;

    w->flags |= 0x140;
    em->setStatus(EM_STATUS_IK_OFF);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x61), (int) ARC(0x62), 5, 1, 0);
        if (w->wallNrm.y > 0.899999976f) {
            w->spd.x = 0.0f;
            w->spd.y = 80.0f;
            w->spd.z = 80.0f;
            PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        } else {
            PSVECScale(&w->wallNrm, &w->spd, 150.0f);
        }
        w->timer = 6;
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        em->r_no_2++;
    case 1:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        if (w->timer) {
            w->timer--;
            em2cSetWallMatrix2(em, 0.400000006f);
            MotionMoveF(em, 0);
            if (w->timer == 0) {
                em->r_no_2++;
            }
            break;
        }
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 6;
            break;
        }
        em2cSetFallMatrix(em);
        if (MotionMoveF(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x65), 0, 5, 5, 0);
        em->r_no_2++;
    case 3:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 6;
            break;
        }
        em2cSetFallMatrix(em);
        MotionMoveF(em, 0);
        break;
    case 4:
        MotionSetCore(em, &em->Motion, ARC(0x63), 0, 5, 5, 0);
        em->r_no_2++;
    case 5:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 6;
            break;
        }
        em2cSetFallMatrix(em);
        MotionMoveF(em, 0);
        break;
    case 6:
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = 1.0f;
        w->wallNrm.z = 0.0f;
        em->r_no_2++;
    case 7:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 1: ceiling (C_*), reset, tail (T_*)

// R1 == 0x21 C_Wait: lurks under the ceiling above the player (flags 0x8000 | 0x120000, no damage)
// following him; when he comes close the tail strikes (the player ducks / back-jumps with the action
// button) or it drops (C_Fall 0x22).
static void em2c_R1_C_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int t;

    w->flags |= 0x8000;
    w->flags |= 0x120000;
    em2cGetTail(em);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->r_no_2++;
        w->x6B8 = fe;
        if (em->r_no_3) {
            *(volatile int*) &w->timer = Rnd() % 150 + 100;  // volatile store: the pG load stays below it (a reference store folds the address to em+0x3E4)
            if (pG->Game_level <= 3) {
                *(volatile int*) &w->timer = Rnd() % 150 + 200;
            }
            if (pG->Game_level <= 1) {
                w->timer = Rnd() % 150 + 300;
            }
        } else {
            w->timer = Rnd() % 60 + 60;
        }
        em->r_no_3 = 1;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (em->flag & 2) {
            break;
        }
        if (em2cDoorCk(em)) {
            w->timer = Rnd() % 60 + 60;
        }
        t = w->timer;
        if (t > 44) {
            if (pPL->r_no_0 == 0 && (u32) (pPL->r_no_1 - 1) <= 2 && t) {
                w->timer = t - 1;
            }
            break;
        }
        if (t) {
            w->timer = t - 1;
            break;
        }
        if (Rnd() % 10 > 3 && w->pTail) {
            w->pTail->flag |= 1;
            em->r_no_2 = 6;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x8A), 0, 0, 5, 0);
        w->timer = 70;
        w->x6B8 = 0;
        em->r_no_2++;
    case 3:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y += 4000.0f;
            em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        MotionSetCore(em, &em->Motion, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (w->timer % 3) {
            EstSet(0, -1, &em->pos, 0, 0x24, 0xC, 0, 0, 0, 0);
        }
        if (w->timer > 17) {
            if (em2cDoorCk(em)) {
                em->r_no_3 = 0;
                em->r_no_2 = 0;
                break;
            }
        } else if (w->x6B8 == 0) {
            ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 1, 4, 0, w->x6B8);
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y = pPLS->pos.y + 4000.0f;
            em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        MotionSetCore(em, &em->Motion, ARC(0x8A), (int) ARC(0x8B), 0, 5, 0);
        w->timer = 10;
        w->atkHit = 0;
        em->flag &= ~4;
        pGS->Room_flg[0] |= 0x40000000;  // struct view: the pG load stays below the flags_3C8 store
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 0;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->x6B8 == 0) {
                ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction, (int) em, 1, 4, 0, w->x6B8);
            }
        }
        if (em->seFlags28B & 1) {
            em2cAtkCk(em, 4, 0xB);
            em2cAtkCk(em, 4, 0xC);
            em2cAtkCk(em, 4, 0xD);
        }
        break;
    case 6:
        em->r_no_2++;
        w->x6B8 = 0;
        w->timer = 120;
    case 7:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        {
            int t7 = w->timer;  // its own variable: with case 1's `t` the three `timer = t - 1` copies are cross-jumped into this one

            if (t7) {
                w->timer = t7 - 1;
            } else {
                em->r_no_2 = t7;
            }
        }
        break;
    }
    if (em->flag & 1) {
        EmRoutineSet(em, 1, 0x22, 0, 0);
    } else {
        em2cBreathSe(em);
    }
}

// R1 == 0x22 C_Fall: appears (visible) and drops from the ceiling onto the floor, then Turn180 / Dash
// / Walk.
static void em2c_R1_C_Fall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    w->flags |= 0x8000;
    switch (em->r_no_2) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = -12725.0f;
        em->pos.y = 886.0f;
        em->pos.z = -80973.0f;
        em->ang.y = -0.589999974f;
        w->jumpAng = -0.589999974f;
        em->invisible_factor = 1.0f;
        em->be_flag |= 2;
        MotionSetCore(em, &em->Motion, ARC(0x94), (int) ARC(0x95), 5, 1, 0);
        em->r_no_2++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end) {
            em->flag |= 8;
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        if (em->frame > 69.6999969f && em->frame < 70.3000031f) {
            EstSet((int) em, -1, 0, 0, 0x24, 7, 0, 0, end, (void*) end);
        }
        if (em->seFlags28B & 1) {
            em2cSetdLandingEff(em);
            AtariOn(&em->atari, 0x300);
        }
        break;
    }
}

// R1 == 0x23 Reset_Wait: parked out of sight at y 4330 (hp 0, invisible, flags 0x8000 | 0x100000)
// until the room's release flag (cEm::flag bit0), then full hp, drops in (falling 0x1000) and joins
// the fight (Turn180 / Dash / Walk).
static void em2c_R1_Reset_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 fl;
    int fe;
    int end;

    w->flags |= 0x8000;
    w->flags |= 0x100000;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.y = 4330.0f;
        MotionSetCore(em, &em->Motion, ARC(0x89), 0, 5, 1, 0);
        if (!(em->flag & 1)) {
            MotionMoveF(em, 0);
            em->hp = fe;
            em->be_flag &= ~2;
            break;
        }
        em->be_flag |= 2;
        em->hp = em->hp_max;
        em->r_no_2++;
    case 1:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        end = MotionMoveF(em, 0);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 4;
        } else if (end) {
            AtariOn(&em->atari, 0x300);
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x63), 0, 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = -200.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 3:
        w->flags |= 0x1000;
        em->dmg.m_Timer = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->ang.y = atan2f(v.x, v.z);
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            MotionSetCore(em, &em->Motion, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->r_no_2 = 4;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 4:
        AtariOn(&em->atari, 0x300);
        em2cSetdLandingEff(em);
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// R1 == 0x24 T_Wait: the tail (type 1) hovers 4000 above the player behind his back (motion 0x87)
// waiting for the body's requests: cEm::flag bit1 sends it into T_Hide (0x25, list set 3), bit0 the
// strike (whose dodge is the back-jump action button); em2cFloorTypeCk picks the strike variant.
static void em2c_R1_T_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int zero;
    u32 i;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->r_no_2++;
        w->x6B8 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x87), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (em->flag & 2) {
            EM_LIST(em->emset_no)->set = 3;
            EmRoutineSet(em, 1, 0x25, 0, 0);
            break;
        }
        if (em->flag & 1) {
            em->flag &= ~1;
            em->r_no_2++;
        }
        break;
    case 2:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x87), 0, 0, 5, 0);
        if (Rnd() % 10 > 4 || em2cFloorTypeCk(em) == 0) {
            em->r_no_3 = 0;
        } else {
            em->r_no_3 = 1;
            SndCall(8, 0x49, &em->pos, em->id, 0, em);
        }
        w->timer = 70;
        w->x6B8 = 0;
        em->r_no_2++;
    case 3:
        if (em->r_no_3) {
            MotionSetCore(em, &em->Motion, ARC(0x8C), 0, 0, 5, 0);
            if (w->x6B8 == 0) {
                GetPlPos(&em->pos, 0, 10.0f);
                em->pos.y = pPLS->pos.y - 500.0f;
                em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            }
            if (w->timer % 3) {
                EstSet(0, -1, &em->pos, 0, 0x24, 0x2D, 0, 0, 0, 0);
            }
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x87), 0, 0, 5, 0);
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y += 4000.0f;
            em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            if (w->timer % 3) {
                EstSet(0, -1, &em->pos, 0, 0x24, 0xC, 0, 0, 0, 0);
            }
        }
        MotionMoveF(em, 0);
        if (w->timer > 17) {
            if ((em->flag & 0x40000000) && em2cDoorCk(em)) {
                em->r_no_2 = 0;
                break;
            }
        } else if (w->x6B8 == 0) {
            ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction, (int) em, 1, 3, 0, w->x6B8);
        }
        if (em->r_no_3) {
            int r = em2cFloorTypeCk(em);
            if (r == 0) {
                em->r_no_2 = r;
                break;
            }
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->r_no_2++;
        }
        break;
    case 4:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y = pPLS->pos.y + 4000.0f;
            em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (em->r_no_3) {
            em->pos.y = pPL->pos.y - 500.0f;
            MotionSetCore(em, &em->Motion, ARC(0x8C), (int) ARC(0x8D), 0, 5, 0);
            zero = 0;
            EstSet((int) em, -1, 0, 0, 0x24, 0x1E, 0, 0, (u32) em, (void*) zero);
            EstSet((int) em, -1, 0, 0, 0x24, 0x20, 0, 0, (u32) em, (void*) zero);
            SndCall(8, 0x4A, &em->pos, em->id, 0, em);
            SndCall(8, 0x3A, &em->pos, em->id, 0, em);
        } else {
            em->pos.y = pPL->pos.y + 4000.0f;
            MotionSetCore(em, &em->Motion, ARC(0x87), (int) ARC(0x88), 0, 5, 0);
            EstSet((int) em, -1, 0, 0, 0x24, 0x1B, 0, 0, (u32) em, 0);
            EstSet(0, -1, &em->pos, &em->ang, 0x24, 0x1F, 0, 0, 0, 0);
            SndCall(8, 0x3F, &em->pos, em->id, 0, em);
            SndCall(8, 0x3A, &em->pos, em->id, 0, em);
        }
        w->timer = 10;
        w->atkHit = 0;
        em->flag &= ~4;
        em->flag |= 0x40000000;
        pGS->Room_flg[0] |= 0x40000000;
        em->r_no_2++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 0;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->x6B8 == 0) {
                ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction, (int) em, 1, 3, 0, w->x6B8);
            }
        }
        if (em->seFlags28B & 1) {
            for (i = 1; i <= 0x12; i++) {
                em2cAtkCk(em, 7, i);
            }
        }
        break;
    }
}

// R1 == 0x25 T_Hide: the tail hidden above the player (following him closely) until the body needs it
// (HideAtk), then back to the floor routines.
static void em2c_R1_T_Hide(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int zero;
    u32 i;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        em->r_no_2++;
        w->x6B8 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 3.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->ang.y = em->ang.y + (pPLS->ang.y + 3.14159274f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x8E), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (em->flag & 1) {
            em->flag &= ~1;
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x8E), (int) ARC(0x8F), 0, 5, 0);
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 0x1B, 0, 0, (u32) em, (void*) zero);
        EstSet(0, -1, &em->pos, &em->ang, 0x24, 0x1F, 0, 0, zero, (void*) zero);
        SndCall(8, 0x3F, &em->pos, em->id, 0, em);
        SndCall(8, 0x3A, &em->pos, em->id, 0, em);
        w->atkHit = zero;
        w->timer = 10;
        em->flag &= ~4;
        em->flag |= 0x40000000;
        em->r_no_2++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->r_no_2 = 0;
            break;
        }
        if (em->seFlags28B & 1) {
            for (i = 1; i <= 0x12; i++) {
                em2cAtkCk(em, 7, i);
            }
        }
        if (em->flag & 1) {
            em->flag &= ~1;
            em->r_no_2 = 2;
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 2: damage

// Damage start sounds and effects: the breath stops, the hit voice, the hidden effects go.
static inline void em2cDmStartSet(cEm2c* em, Em2cWork* w)
{
    SndStop(w->breathSnd, 0);
    w->breathTimer = 2;
    SndCall(8, 0xF, &em->pos, em->id, 0, em);
    EffectEspDelete(0, w->espKind2, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind2, (int) em);
    EffectEfmDelete(0, w->espKind2, (int) em);
}

// Damage end on the floor: the walk / turn / dash pick (with the ceiling climb chance).
static inline int em2cDmEndSet(cEm2c* em, Em2cWork* w, int zero)
{
    w->dmgTotal = zero;
    if (Rnd() % 3 == 0) {
        if (em2cToCeilingCk(em)) {
            return 1;
        }
    }
    if (w->targetAngAbs > 2.09439516f) {
        EmRoutineSet(em, 1, 3, zero, zero);
    } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
        EmRoutineSet(em, 1, 2, zero, zero);
    } else {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
    return 1;
}

// R0 == 2: damage (flag bit3), runs Em2c_R1_dm_tbl[r_no_1].
static void em2c_R0_Damage(cEm2c* em)
{
    EM2C_WK(em)->flags |= 8;
    Em2c_R1_dm_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Normal: the floor flinch (one of three by hit side), then SideStep (5) or the walk.
static void em2c_R1_Dm_Normal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f)) < 1.57079637f) {
            switch (Rnd() % 3) {
            case 0:
            default:
                MotionSetCore(em, &em->Motion, ARC(0x1D), (int) ARC(0x1E), 5, 1, 0);
                break;
            case 1:
                MotionSetCore(em, &em->Motion, ARC(0x1F), (int) ARC(0x20), 5, 1, 0);
                break;
            case 2:
                MotionSetCore(em, &em->Motion, ARC(0x21), (int) ARC(0x22), 5, 1, 0);
                break;
            }
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x23), (int) ARC(0x24), 5, 1, 0);
        }
        em2cDmStartSet(em, w);
        w->timer = 30;
        em->r_no_2++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end) {
            end = 0;
            em2cDmEndSet(em, w, end);
            break;
        }
        if (em->seFlags28B & 4) {
            if (Rnd() & 1) {
                em2cDmEndSet(em, w, end);
                break;
            }
        }
        if (w->timer) {
            w->timer--;
            break;
        }
        if (em2cLockCk(em)) {
            int r = em2cToCeilingCk(em);  // block-local: `mr. r3,r3` (keeps this RS out of the cross-jumped RS tails)

            if (r == 0) {
                EmRoutineSet(em, 1, 5, r, r);
            }
        }
        break;
    }
}

// R0 2 / R1 == 1 Dm_Down: knocked down (flag 0x8000), then Wakeup (0xF).
static void em2c_R1_Dm_Down(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x8000;
    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->Motion, ARC(0x92), 0, 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x6B), (int) ARC(0x6C), 5, 1, 0);
        }
        em2cDmStartSet(em, w);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            r = em2cDownJumpCk(em);
            if (r == 0) {
                EmRoutineSet(em, 1, 0xF, r, r);
            }
        }
        break;
    }
}

// Damage fall step: the position follows `spd` with gravity; on the floor the landing motion.
// The fall step of the damage/die routines, a macro (not an inline: integrate.c drops the
// RTX_UNCHANGING_P flag of an inlined body's constant-pool loads, which then depend on the
// preceding byte store and sink below the int argument moves). The landing tail (`xFE = next`)
// and the fall arm live inside it, so the caller has no return-value diamond.
#define EM2C_DM_FALL(em, w, v_, DM_TYPE, DOWN, ATARI_ON, NEXT, FALL_MTX, END_INC)                \
    {                                                                                          \
        f32 fl_;                                                                               \
                                                                                               \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        (w)->spd.y -= 20.0f;                                                                   \
        if (DM_TYPE) {                                                                         \
            (em)->dmg.m_Timer = 2;                                                                  \
        }                                                                                      \
        fl_ = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                             \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        if ((em)->pos.y < fl_) {                                                               \
            (em)->pos.y = fl_;                                                                 \
            v_.x = 0.0f; \
            v_.y = 0.0f; \
            v_.z = 1.0f; \
            PSMTXMultVecSR((em)->mat, &v_, &v_);                                               \
            (em)->ang.y = atan2f(v_.x, v_.z);                                                  \
            MotionSetCore(em, &(em)->Motion, ARC(0x69), (int) ARC(0x6A), 5, 1, 0);               \
            MotionMoveF(em, 0);                                                                \
            if (DOWN) {                                                                        \
                em2cSetDownEff(em);                                                            \
            } else {                                                                           \
                em2cSetdLandingEff(em);                                                        \
            }                                                                                  \
            if (ATARI_ON) {                                                                    \
                AtariOn(&(em)->atari, 0x300);                                                  \
            }                                                                                  \
            (em)->r_no_2 = NEXT;                                                                  \
        } else {                                                                               \
            if (FALL_MTX) {                                                                    \
                em2cSetFallMatrix(em);                                                         \
            }                                                                                  \
            if (END_INC) {                                                                     \
                if (MotionMoveF(em, 0)) {                                                      \
                    (em)->r_no_2++;                                                               \
                }                                                                              \
            } else {                                                                           \
                MotionMoveF(em, 0);                                                            \
            }                                                                                  \
        }                                                                                      \
    }

// R0 2 / R1 == 2 Dm_Jump: shot out of a jump: falls to the floor on its back (flag 0x4000), then
// WakeupWait (0xE) or Die_Down (R3 3).
static void em2c_R1_Dm_Jump(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    Vec ofs;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    fe = em->r_no_2;
    switch (fe) {
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
        MotionSetCore(em, &em->Motion, ARC(0x67), 0, 0, 5, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->r_no_2++;
    case 1:
        EM2C_DM_FALL(em, w, v, 1, 0, 0, 4, 1, 1);
        break;
    case 2:
        MotionSetCore(em, &em->Motion, ARC(0x68), 0, 0, 5, 0);
        em->r_no_2++;
    case 3:
        EM2C_DM_FALL(em, w, v, 1, 1, 0, 4, 1, 0);
        break;
    case 4:
        em->r_no_2++;
    case 5:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 3 Dm_Wall: shot off the wall (after 1000+ damage): falls on its back, then WakeupWait
// or Die_Down.
static void em2c_R1_Dm_Wall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec ofs;
    Mtx m;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        PSVECScale(&w->wallNrm, &ofs, 700.0f);
        PSVECAdd(&em->pos, &ofs, &em->pos);
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'x', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->Motion, ARC(0x68), 0, 0, 5, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->r_no_2++;
    case 1:
        EM2C_DM_FALL(em, w, ofs, 1, 1, 0, 2, 1, 0);
        break;
    case 2:
        em->r_no_2++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
            }
        }
        break;
    }
}

// R0 2 / R1 == 4 Dm_Guard: blocks the hit with the claws (motion 0x11, 30 frames, guard effect
// cleared), then dodges when aimed at (SideStep / ToCeiling), attacks when the player is in reach, or
// Turn180 / Dash / Walk.
static void em2c_R1_Dm_Guard(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int lock;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x11), (int) ARC(0x12), 5, 1, 0);
        w->timer = 30;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            lock = em2cLockCk(em);
            if (lock) {
                if (em2cToCeilingCk(em) == 0) {
                    EmRoutineSet(em, 1, 5, 0, 0);
                }
                break;
            }
            if (Rnd() % 3 == 0) {
                if (em2cToCeilingCk(em)) {
                    break;
                }
            }
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, lock, lock);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, lock, lock);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        if ((em->seFlags28B & 4) && (w->flags & 1)) {
            f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f));
            f32 lim = 12250000.0f;
            if (plAng > 1.57079637f) {
                lim = 4000000.0f;
            }
            if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                if (pPL->r_no_0 != 0 && pPL->r_no_1 != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                    !(w->flags & 0x800)) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                    break;
                }
                if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                    break;
                }
                switch (Rnd() % 3) {
                default:
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                    break;
                case 1:
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    break;
                case 2:
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                    break;
                }
                break;
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        if (w->timer) {
            w->timer--;
            break;
        }
        if (em2cLockCk(em)) {
            int r = em2cToCeilingCk(em);  // block-local (`mr. r3,r3`): identical to the case-1 copy, cross-jumped

            if (r == 0) {
                EmRoutineSet(em, 1, 5, r, r);
            }
        }
        break;
    }
}

// R0 2 / R1 == 5 Dm_Freeze: the liquid nitrogen freezes it on the floor (Room_flg[2] bit31 ->
// em2cSetFreeze): the freeze motion, then F_Walk (0x18).
static void em2c_R1_Dm_Freeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;
    int hit;

    w->flags &= ~8;
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x2E), (int) ARC(0x2F), 5, 1, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x33, &em->pos, em->id, 0, em);
        SndCall(8, 0x34, &em->pos, em->id, 0, em);
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        w->x6B8 = fe;
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x18, 0, 0);
            break;
        }
        if (em->plDist2 < 2250000.0f) {
            hit = w->x6B8;
            if (hit == 0 && fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) < 0.785398185f) {
                ActBtn.set(7, 0xB, (int) em2cKickAction, (int) em, 1, 1, 0, hit);
            }
        }
        break;
    }
}

// R0 2 / R1 == 6 Dm_C_Freeze: frozen while hidden above the player: becomes visible and drops on its
// back, then WakeupWait (0xE) or Die_Down.
static void em2c_R1_Dm_C_Freeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        em->pos = pPLS->pos;
        em->pos.y += 6000.0f;
        em->ang.y = pPLS->ang.y + 3.14159274f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        MotionSetCore(em, &em->Motion, ARC(0x68), 0, 0, 5, 0);
        em->invisible_factor = 1.0f;
        em->be_flag |= 2;
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, (void*) fe);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, (void*) fe);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x33, &em->pos, em->id, 0, em);
        SndCall(8, 0x34, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->r_no_2++;
    case 1:
        EM2C_DM_FALL(em, w, v, 0, 1, 1, 2, 0, 0);
        break;
    case 2:
        em->r_no_2++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
            }
        }
        break;
    }
}

// Action button callback of the kick on the frozen boss: the player's kick routine (plemKick), both
// damage-held, critical scored.
static void em2cKickAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage(em, plemKick);
    pPL->dmg.set(0, 30);
    if (pSUB) {
        cDmgInfo* d = &pSUB->dmg;  // &pSUB->dmg is computed before the dead test

        if (!em2cDeadCk(pSUB)) {
            d->set(0, 30);
        }
    }
    w->x6B8 = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player routine of the kick on the frozen boss (r_no_3 picks one of four kick motions), the foot sweep
// (PlWepHitCheck3) shatters it.
static void plemKick(cPlayer* pl)
{
    Vec pos;

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    StaFlagOn(pG, STA_PL_EM_ACTION);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x7C), 0, 6, 1, 0);
        pl->m_Work1 = 10;
        pl->m_Work2 = 33;
        GameAddPoint(LVADD_CRITICALHIT);
        pl->m_Work0 = 17;
        pl->r_no_3 = Rnd() & 3;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work1) {
            pl->m_Work1--;
            pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 0.196349546f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
            if (pl->m_Work1 == 0) {
                SndCall(1, 0x11, &pl->pos, 0, 0, pl);
                SndCall(1, 0x10, &pl->pos, 0, 0, pl);
            }
        }
        if (pl->m_Work0) {
            pl->m_Work0--;
            if (pl->m_Work0 == 0) {
                pos.x = 0.0f;
                pos.y = 1500.0f;
                pos.z = 300.0f;
                PSMTXMultVec(pPL->mat, &pos, &pos);
                if (PlWepHitCheck3(&pos, 0x14, 10, 1200.0f)) {
                    SndCall(1, 0xF, &pl->pos, 0, 0, pl);
                }
                pos.x = 0.0f;
                pos.y = 1000.0f;
                pos.z = 300.0f;
                PSMTXMultVec(pPL->mat, &pos, &pos);
                if (PlWepHitCheck3(&pos, 0x14, 10, 1200.0f)) {
                    SndCall(1, 0xF, &pl->pos, 0, 0, pl);
                }
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 15);
            break;
        }
        if (pl->m_Work2) {
            pl->m_Work2--;
            break;
        }
        if (joyKamae() || (Key.on & 0x10F)) {
            EndPlDamage();
            pl->dmg.set(0, 15);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// R0 2 / R1 == 7 Dm_F_Normal: the frozen boss hit: the stiff flinch; when the ice is spent (guardCnt 0)
// it breaks free (F_Clear 0x1A), else Turn180 / F_Walk.
static void em2c_R1_Dm_F_Normal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int flip;
    int hit;

    w->flags |= 0x100;
    switch (em->r_no_2) {
    case 0:
        flip = 0x41;
        if (em->motFlags & 0x40) {
            flip = 1;
        }
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f)) < 1.57079637f) {
            MotionSetCore(em, &em->Motion, ARC(0x3A), (int) ARC(0x3B), 5, flip, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x3C), (int) ARC(0x3D), 5, flip, 0);
        }
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->timer = 30;
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            if (w->guardCnt == 0) {
                EmRoutineSet(em, 1, 0x1A, 0, 0);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            }
            break;
        }
        if (w->timer) {
            w->timer--;
        } else if (w->guardCnt) {
            w->flags &= ~8;
        }
        if (w->guardCnt == 0) {
            w->dmgTotal = 0;
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            break;
        }
        if (em->plDist2 < 2250000.0f) {
            hit = w->x6B8;
            if (hit == 0 && fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) < 0.785398185f) {
                ActBtn.set(7, 0xB, (int) em2cKickAction, (int) em, 1, 1, 0, hit);
            }
        }
        break;
    }
}

// R0 2 / R1 == 8 Dm_F_Blow: the frozen boss kicked over (em2cKickAction): falls on its back (flag
// 0x4000), then Die_Down when dead or WakeupWait.
static void em2c_R1_Dm_F_Blow(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f)) < 1.57079637f) {
            em->ang.y += Muku(&em->pos, &em->dmg.m_PosFrom, em->ang.y, 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            MotionSetCore(em, &em->Motion, ARC(0x3E), (int) ARC(0x3F), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x24, 0x28, 0, 0, (u32) em, (void*) fe);
        } else {
            em->ang.y += Muku(&em->dmg.m_PosFrom, &em->pos, em->ang.y, 3.14159274f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
            MotionSetCore(em, &em->Motion, ARC(0x40), (int) ARC(0x41), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x24, 0x29, 0, 0, (u32) em, (void*) fe);
        }
        if (em->hp > 0) {
            SndStop(w->breathSnd, 0);
            w->breathTimer = 2;
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        w->flags |= 0x4000;
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 3: die

// The enemy's effects (espKind) go at death.
static inline void em2cDieEffDelete(cEm2c* em, Em2cWork* w)
{
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
}

// R0 == 3: death (flag bit3), runs Em2c_R1_die_tbl[r_no_1].
static void em2c_R0_Die(cEm2c* em)
{
    EM2C_WK(em)->flags |= 8;
    Em2c_R1_die_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Lost: the corpse dissolves (em2cScaleCompress + invisible_factor), item drop, hidden.
static void em2c_R1_Die_Lost(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    fe = em->r_no_2;
    if (fe == 0) {
        AtariOff(&em->atari, 0xFCFF);
        EmSetDie(em);
        EmReserveDropItem(em);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        em2cDieEffDelete(em, w);
        if (em->be_flag & 2) {
            EstSet((int) em, -1, 0, 0, 0x24, 0x31, 0, 0, fe, (void*) fe);
        }
        w->timer = 30;
        w->timer8 = 150;
        w->Compress_y = 1.0f;
        em->r_no_2++;
    }
}

// R0 3 / R1 == 1 Die_Normal: dies standing (death motion), then Die_Lost.
static void em2c_R1_Die_Normal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    fe = em->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->Motion, ARC(0x90), (int) ARC(0x91), 5, 1, 0);
        EmSetDieCntE(em);
        em2cDieEffDelete(em, w);
        EstSet((int) em, -1, 0, 0, 0x24, 0x33, 0, 0, (u32) em, (void*) fe);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 2 Die_Freeze: the frozen boss shatters (ice break effects), then Die_Lost.
static void em2c_R1_Die_Freeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec rot;
    int t;

    switch (em->r_no_2) {
    case 0:
        EmSetDieCntE(em);
        w->timer = 11;
        w->timer8 = 8;
        em->r_no_2++;
    case 1:
        if (w->timer8) {
            t = w->timer8 - 1;
            w->timer8 = t;
            if (t == 0) {
                Camera* cam = &pG->Cam;
                em2cDieEffDelete(em, w);
                rot.x = 0.0f;
                rot.y = GetXZAngle(&em->pos, &cam->param.pos);
                rot.z = 0.0f;
                EstSet((int) em, -1, 0, 0, 0x24, 0xA, 0, 0, t, (void*) t);
                EstSet(0, -1, &em->pos, &rot, 0x24, 0x32, 0, 0, t, (void*) t);
                SndCall(8, 0x41, &em->pos, em->id, 0, em);
            }
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->be_flag &= ~2;
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 3 Die_Down: dies on its back, then Die_Lost.
static void em2c_R1_Die_Down(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    switch (em->r_no_2) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->Motion, ARC(0x93), 0, 5, 1, 0);
        } else {
            MotionSetCore(em, &em->Motion, ARC(0x6D), (int) ARC(0x6E), 5, 1, 0);
        }
        EmSetDieCntE(em);
        em2cDieEffDelete(em, w);
        em->r_no_2++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

// R0 3 / R1 == 4 Die_Wall: dies on the wall: drops to the floor on its back, then Die_Down.
static void em2c_R1_Die_Wall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec ofs;
    Mtx m;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(EM_STATUS_IK_OFF);
    fe = em->r_no_2;
    switch (fe) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        PSVECScale(&w->wallNrm, &ofs, 700.0f);
        PSVECAdd(&em->pos, &ofs, &em->pos);
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'x', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->Motion, ARC(0x68), 0, 0, 5, 0);
        w->flags |= 0x4000;
        em2cDieEffDelete(em, w);
        EmSetDieCntE(em);
        em->r_no_2++;
    case 1:
        EM2C_DM_FALL(em, w, ofs, 0, 0, 0, 2, 1, 0);
        break;
    case 2:
        em->r_no_2++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 3, 0, 0);
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Helpers

// Per frame: the route point / angle to the player (routePos / routeAng), the direction to his head
// (plDir), line of sight (flag bit0), plDist / homeDist and the target copies.
void em2cRouteCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec plPos;
    Vec a;
    Vec b;
    int up;

    if (em->hp <= 0) {
        return;
    }
    up = 0;
    if (fabsf(pPL->pos.y - em->pos.y) > 1000.0f) {
        up = 1;
    }
    RouteCkToPos(em, &pPL->pos, &w->routePos, up, 0);
    w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, 3.14159274f);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->r_no_0 == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    plPos = pPL->pos;
    plPos.y += 1800.0f;
    w->plDir = em2cGetPlDir(em, &plPos);
    w->plDirAbs = fabsf(w->plDir);
    w->flags &= ~1;
    if (em->r_no_0 != 0) {
        a = em->getPartsPtr(0)->world;
        b.x = pPLS->pos.x;  // struct view: the pPL load stays below the `a` copy's stores
        b.y = pPLS->pos.y + 1500.0f;
        b.z = pPLS->pos.z;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            w->flags |= 1;
        }
    }
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    w->homeDist = RouteCkPosToPosDis(&w->homePos, &pPLS->pos);
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
}

// Damage of the weapon hit: GetWepDmVal (`near` for a muzzle within 6000), tripled on the frozen boss.
int em2cSetDmVal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int near = 0;
    int dmg;

    if (em->dmg.m_pDamageYarare->rad < 36000000.0f) {
        near = 1;
    }
    dmg = 100;
    if (em->dmg.m_Wep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmg.m_Wep, near);
    }
    if (w->flags & 0x800) {
        dmg *= 3;
    }
    return dmg;
}

// Attack hit test at part `parts` with em2c_atk_info[no]: 0 / 1 the side slashes (plemDmSide, the
// player spun by the hit), 2 / 3 the claws (head lost when they kill), 4 the critical (plem2c_CriticalHit
// or knock-down), 5 / 6 the tail (plemDmTail); the partner is hurt too; once per attack (atkHit). 1 = hit.
int em2cAtkCk(cEm2c* em, int no, int parts)
{
    Em2cWork* w = EM2C_WK(em);
    EmAtkInfo* atk;
    cModel* p;
    int hit;

    if (w->atkHit) {  // the early return's label keeps `mr r3, em` before getPartsPtr (em26 idiom)
        return 0;
    }
    atk = &em2c_atk_info[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(atk, &p->world, &p->world_old, 0);
    if (hit) {
        if (hit & 1) {
            switch ((u32) no) {
            case 0:
                if (em->motFlags & 0x40) {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x24, 0x18);
                } else {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x24, 0x17);
                }
                if ((s16) pG->pl_life > 0) {
                    SetPlDamage(em, plemDmSide);
                    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) < 1.57079637f) {
                        pPLS->ang.y += Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f);
                        pPLS->r_no_3 = 0;
                    } else {
                        pPLS->ang.y += Muku(&em->pos, &pPL->pos, pPL->ang.y, 3.14159274f);
                        pPLS->r_no_3 = 1;
                    }
                    if (em->motFlags & 0x40) {
                        if (pPL->r_no_3) {
                            pPL->r_no_3 = 0;
                        } else {
                            pPL->r_no_3 = 1;
                        }
                    }
                } else {
                    em2cPlHeadLost();
                }
                break;
            case 3:
                if (em->motFlags & 0x40) {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x24, 0x18);
                } else {
                    pPL->dmg.m_PosFrom = em->pos;
                    EmPlBloodSet2(em, &p->world, 1, 0x24, 0x17);
                }
                if ((s16) pG->pl_life <= 0) {
                    em2cPlHeadLost();
                }
                break;
            case 4:
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x24, 0x22);
                if ((s16) pG->pl_life <= 0) {
                    em2cPlHeadLost();
                }
                break;
            case 1:
                pPL->dmg.m_PosFrom = em->pos;
                EmPlBloodSet2(em, &p->world, 1, 0x24, 0x15);
                if ((s16) pG->pl_life <= 0) {
                    pG->pl_life = 0;
                    SetPlDamage(em, plem2c_CriticalHit);
                } else {
                    pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                    PlSetDamage(8, 0, 0);
                }
                break;
            case 2:
                EmPlBloodSet2(em, &p->world, 1, 0x24, 0x19);
                pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(8, 0, 0);
                break;
            case 7:
                EmPlBloodSet2(em, &p->world, 1, 0x24, 0x1A);
                if ((s16) pG->pl_life <= 0) {
                    pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                    PlSetDamage(8, 0, 0);
                } else {
                    SetPlDamage(em, plemDmTail);
                }
                break;
            case 5:
                EmPlBloodSet2(em, &p->world, 1, 0x24, 0x1C);
                EstSet((int) pPL, -1, 0, 0, 0x24, 0x1D, 0, 0, (u32) pPL, 0);
                if ((s16) pG->pl_life <= 0) {
                    pPL->ang.y = GetXZAngle(&pPL->pos, &em->pos);
                    PlSetDamage(8, 0, 0);
                } else {
                    SetPlDamage(em, plemDmTail);
                }
                break;
            }
            w->atkHit = 1;
            em->flag |= 4;
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
void em2cScaleCompress(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
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

// Is the player aiming a gun at the boss (by weapon kind, with ammo, in front): the weapon's target is
// this enemy or its root lies in the box in front of the weapon hand.
int em2cLockCk(cEm2c* em)
{
    Mtx inv;
    Vec pos;

    if (pPL->r_no_0 != 0) {
        return 0;
    }
    if (pPL->r_no_1 != 6) {
        return 0;
    }
    if (em->plDist2 > 144000000.0f) {
        return 0;
    }
    if (pG->weapon_no == 0x10) {
        return 0;
    }
    if (pG->Game_level <= 1) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    switch (pG->weapon_no) {
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
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, 3.14159274f)) > 0.785398185f) {
        return 0;
    }
    if (pPL->Wep->m_pWep->wep.target && pPL->Wep->m_pWep->wep.target == em) {
        return 1;
    }
    PSMTXInverse(pPL->getPartsPtr(10)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->world, &pos);
    if (pos.x > 0.0f) {
        return 0;
    }
    if (pos.z > 800.0f || pos.z < -800.0f) {
        return 0;
    }
    if (pos.y > 800.0f) {
        return 0;
    }
    if (pos.y < -800.0f) {
        return 0;
    }
    return 1;
}

// Wall walk step: probes the surface around the feet (four probes), blends wallNrm towards the found
// normal by `rate`, snaps to the surface and rebuilds the matrix; 0 when the surface was lost.
int em2cSetWallMatrix2(cEm2c* em, f32 rate)
{
    Em2cWork* w = EM2C_WK(em);
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
    a.y = 700.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -500.0f;
    b.z = 1200.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n0.x = 0.0f;
    n0.y = 0.0f;
    n0.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n0 = nrm;
    }
    a.x = 0.0f;
    a.y = 700.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -500.0f;
    b.z = -1200.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n1.x = 0.0f;
    n1.y = 0.0f;
    n1.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n1 = nrm;
    }
    b.x = -1200.0f;
    a.x = 0.0f;
    a.y = 700.0f;
    a.z = 0.0f;
    b.y = -500.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n2.x = 0.0f;
    n2.y = 0.0f;
    n2.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n2 = nrm;
    }
    a.y = 700.0f;
    b.x = 1200.0f;
    b.y = -500.0f;
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
#line 7932 "D:/Bio4/Prog/em2c.cpp"
    VECNormalize(&sum, &w->wallNrm);
    em->motFlags2 |= 0x40000000;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 7944 "D:/Bio4/Prog/em2c.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &sum);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &sum, ang * rate);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    a.x = 0.0f;
    a.y = 600.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -600.0f;
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

// While falling off a wall: eases wallNrm back to straight up so the boss lands feet first.
void em2cSetFallMatrix(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
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
#line 8022 "D:/Bio4/Prog/em2c.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &axis);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &axis, ang * 0.200000003f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}

// The player's head comes off: in the overseas versions hides the head model and spawns it as a cObj01
// with the blood effect; the Japanese version only plays the blood / SE.
void em2cPlHeadLost()
{
    Vec ofs;
    Vec spd;
    Vec rot;
    cModel* p3;
    cObj* obj;
    int zero;

    if (pSys->eff_country == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x24, 0x2E, 0, 0, (u32) pPL, 0);
        return;
    }
    zero = 0;
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet((int) pPL, -1, 0, 0, 0x24, 0x2F, 0, 0, (u32) pPL, (void*) zero);
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
    EstSet((int) obj, -1, 0, 0, 0x24, 0x30, 0, 0, (u32) obj, (void*) zero);
}

// Yaw from the boss to `pos`.
f32 em2cGetPlDir(cEm2c* em, Vec* pos)
{
    Mtx inv;
    Vec d;

    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, pos, &d);
    return atan2f(d.x, d.z);
}

// 1 when the wall under the wall-walker ends (no surface ahead / below): it must drop (W_Fall).
int em2cWallFallCk(cEm2c* em)
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

// Door in the way while walking: shock it open (or break it) when the enemy is stuck.
void em2cDoorOpenCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 ang;
    u32 i;

    if (w->x574) {
        return;
    }
    if (w->stuckCnt > 3) {
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
            if (e->hp > 300) {
                e->setShock(0, &em->pos, 0);
                e->hp -= 800;
                if (e->hp <= 0) {
                    e->hp = 1;
                }
                w->x574 = Rnd() % 15 + 15;
                w->Dash_wait = 150;
                if (em->r_no_0 == 1 && em->r_no_1 == 2) {
                    em->r_no_0 = 1;
                    em->r_no_1 = 1;
                    em->r_no_2 = 0;
                    em->r_no_3 = 0;
                }
            } else if (w->flags & 0x800) {
                em->r_no_0 = 1;
                em->r_no_1 = 0x19;
                em->r_no_2 = 0;
                em->r_no_3 = 1;
            } else {
                em->r_no_0 = 1;
                em->r_no_1 = 9;
                em->r_no_2 = 0;
                em->r_no_3 = 1;
            }
            break;
        case 1:
        case 2:
        case 3:
            break;
        }
    }
}

// Opens a closed door (cEmDoor) the boss walks into from its side (the plain open, setOpen).
void em2cDoorOpenCk2(cEm2c* em)
{
    Vec v;
    f32 ang;
    u32 i;

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
            e->setOpen(&em->pos, 0, 0, 1);
            break;
        case 1:
        case 2:
        case 3:
            break;
        }
    }
}

static u8 em2c_cloth_parts[20] = { 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42,
                                   0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C };
static u8 em2c_cloth_up[20] = { 0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41,
                                0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B };
static u8 em2c_cloth_down[20] = { 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43,
                                  0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0xFF };
static f32 em2c_cloth_max[20] = { 0.3f, 0.4f, 0.53f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f,
                                  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

// Sets up the 20-link pendulum cloth of the back tendrils (parts 0x39..0x4C).
void em2cClothSet(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero = em->type;

    if (zero != 0) {
        return;
    }
    w->cloth.Num = 20;
    w->cloth.pCloth = em2c_cloth_parts;
    w->cloth.pLeft = (u8*) zero;
    w->cloth.pRight = (u8*) zero;
    w->cloth.pUpLeft = (u8*) zero;
    w->cloth.pUpRight = zero;
    w->cloth.pParent = em2c_cloth_up;
    w->cloth.pChild = em2c_cloth_down;
    w->cloth.pWindSin = (f32*) zero;
    w->cloth.pWindRate = (f32*) zero;
    w->cloth.pGravity = zero;
    w->cloth.pRate = (f32*) zero;
    w->cloth.pMax = em2c_cloth_max;
    w->cloth.pAtset = (CLOTH_AT_SET*) zero;
    w->cloth.At_num = zero;
    w->cloth.Gravity = 40.0f;
    w->cloth.Rate = 0.6f;
    w->cloth.Bundle_num = 3;
    {
        const f32 z = 0.0f;  // pool order: 0.0 before 0.05

        w->cloth.Stretchy = 0.05f;
        w->cloth.pModel = (cModel*) zero;
        w->cloth.WindSin = z;
        w->cloth.Move_rate = z;
    }
    w->cloth.Flag = zero;
    w->cloth.pPtbl = zero;
    PenClothSet(em, (PenCloth*) &w->cloth, 100.0f);
}

// Per frame: the tendril cloth update (PenClothMove) with the hide bits of the parts handled.
void em2cClothMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    cModel* p;
    u32 i;

    if (em->type != 0) {
        return;
    }
    if (w->flags & 0x20000) {
        em->be_flag |= 0x200000;
        return;
    }
    PenClothMove2(em, (PenCloth*) &w->cloth);
    for (i = 0x4B; i <= 0x4C; i++) {
        p = em->getPartsPtr(i);
        PSMTXConcat(p->pParent->mat, p->l_mat, p->mat);
        p->world.x = p->mat[0][3];
        p->world.y = p->mat[1][3];
        p->world.z = p->mat[2][3];
    }
    em->be_flag &= ~0xE00000;
}

// Footstep SEs on the motion's step events (parts / floor material).
void em2cFootSeMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    cModel* p;
    cModel* p0;
    int parts;
    u32 no;

    if (em->seNo == 0) {
        return;
    }
    if (em->type != 0) {
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
        em2cSetdLandingEff(em);
        return;
    case 5:
        em->seNo = 0;
        em2cSetDownEff(em);
        return;
    case 0xE:
        em->seNo = 0;
        em2cSetJumpEff(em);
        return;
    default:
        return;
    }
    p = em->getPartsPtr(parts);
    p0 = em->getPartsPtr(0);
    if (w->wallNrm.y < 0.0f) {
        EstSet(0, -1, &p->world, 0, 0x24, 0xD, 0, 0, 0, 0);
    }
}

// Landing dust / splash at the root.
void em2cSetdLandingEff(cEm2c* em)
{
    em->getPartsPtr(0);
    SndCall(8, 4, &em->pos, em->id, 0, em);
    EstSet((int) em, -1, 0, 0, 0x24, 0x2A, 0, 0, 0, 0);
}

// Dust / splash when the boss hits the floor on its back.
void em2cSetDownEff(cEm2c* em)
{
    cModel* p0 = em->getPartsPtr(0);

    SndCall(8, 5, &p0->world, em->id, 0, em);
}

// Dust / splash at the take-off of a jump.
void em2cSetJumpEff(cEm2c* em)
{
    cModel* p0 = em->getPartsPtr(0);

    SndCall(8, 0xE, &p0->world, em->id, 0, em);
}

// The floor under the boss dropped away by more than 250 -> W_Fall (0x20). 1 = set.
int em2cFallCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    if (SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) > em->pos.y - 250.0f) {
        return 0;
    }
    w->wallNrm.x = 0.0f;
    w->wallNrm.y = 1.0f;
    w->wallNrm.z = 0.0f;
    EmRoutineSet(em, 1, 0x20, 0, 0);
    return 1;
}

// Half the time, with a wall rising in front -> DownJump (0x10). 1 = set.
int em2cDownJumpCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec a;
    Vec b;

    if (!(w->flags & 0x4000)) {
        return 0;
    }
    if (Rnd() & 1) {
        return 0;
    }
    if (em2cNoWallCk(em)) {
        return 0;
    }
    return 0;
}

// With a ceiling within reach above -> ToCeiling (0x11). 1 = set.
int em2cToCeilingCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec a;
    Vec b;
    int r;

    if (pG->room_id != 0x221) {
        return 0;
    }
    r = em2cNoWallCk(em);
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
    w->dmgTotal = r;
    EmRoutineSet(em, 1, 0x11, r, r);
    return 1;
}

// An edge ahead within 30 deg of the heading -> JumpDown (0x16). 1 = set.
int em2cJumpDownCk(cEm2c* em)
{
    Vec a;
    Vec b;
    Vec hit;

    return 0;
}

// A low wall ahead within 30 deg of the heading -> WallOver (0x1B). 1 = set.
int em2cWallOverCk(cEm2c* em)
{
    Vec a;
    Vec b;
    Vec hit;

    return 0;
}

// Neck parts (3) turns towards the player while it is found (addRot.y, damped).
void em2cNeckMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    cParts* p;
    cModel* pp;
    Vec v;

    em->getPartsPtr(4);
    pp = pPL->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(pp->mat, &v, &v);
    if (w->flags & 0x80) {
        w->Neck_dir_y = w->Neck_dir_y * 0.899999976f + Muku(&em->pos, &pPL->pos, em->ang.y, 1.04719758f) * 0.100000001f;
    } else {
        w->Neck_dir_y = w->Neck_dir_y * 0.899999976f;
    }
    p = (cParts*) em->getPartsPtr(3);
    p->motParts.flags |= 0x40000000;
    p->addRot.x = 0.0f;
    p->addRot.y = w->Neck_dir_y;
    p->addRot.z = 0.0f;
}

// Player hit by a side attack: knocked to the side the attack came from.
static void plemDmSide(cPlayer* pl)
{
    int flip;

    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 2);
    switch (pl->r_no_2) {
    case 0:
        flip = 1;
        if (pl->r_no_3) {
            flip = 0x41;
        }
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x83), (int) PL_ARC(0x84), 5, flip, 0);
        PlSetFace(1);
        PlSetDamageSe(0);
        pl->r_no_2++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Player hit by the tail: one of three knock-back motions.
static void plemDmTail(cPlayer* pl)
{
    void* m0;
    void* m1;
    int flip;

    pl->subArc = pl->pEmCatch->subArc;
    switch (pl->r_no_2) {
    case 0:
        flip = 1;
        if (Rnd() % 10 > 4) {
            flip = 0x41;
        }
        switch (Rnd() % 3) {
        case 0:
        default:
            m0 = PL_ARC_PTR(pG->pPlayer, 0x48);
            m1 = 0;
            break;
        case 1:
            m0 = PL_ARC_PTR(pG->pPlayer, 0x49);
            m1 = 0;
            break;
        case 2:
            m0 = PL_ARC(0x83);
            m1 = PL_ARC(0x84);
            break;
        }
        MotionSetCore(pl, &pl->Motion, m0, (int) m1, 5, flip, 0);
        pl->dmg.set(0, 5);
        PlSetFace(1);
        PlSetDamageSe(0);
        pl->r_no_2++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Action button callback "duck": the player's duck routine (plem2cSit), critical scored.
static void em2cSitAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage(em, plem2cSit);
    w->x6B8 = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player routine of the duck under the claw / tail (motion 0x73 of the boss archive), escape scored.
static void plem2cSit(cPlayer* pl)
{
    pl->subArc = pl->pEmCatch->subArc;
    pl->dmg.set(0, 30);
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x73), 0, 5, 1, 0);
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->r_no_2++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Action button callback "dodge": the player's roll (plem2cEscape), critical scored.
static void em2cEscapeAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage(em, plem2cEscape);
    w->x6B8 = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player escapes the ambush: a back jump when walls are on both sides, a side roll otherwise.
static void plem2cEscape(cPlayer* pl)
{
    Vec a;
    Vec b;
    int side;
    int wall;
    int fe;

    pl->subArc = pl->pEmCatch->subArc;
    fe = pl->r_no_2;
    pl->dmg.m_Timer = 0x1E;
    switch (fe) {
    case 0:
        side = 0;
        if (Muku(&pl->pEmCatch->pos, &pl->pos, pl->ang.y, 3.14159274f) < 0.0f) {
            side = 1;
        }
        side = Rnd() & 1;
        wall = 0;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall = 1;
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
            wall |= 2;
        }
        if (wall == 3) {
            MotionSetCore(pl, &pl->Motion, PL_ARC(0x85), 0, 5, 1, 5);
            EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
            SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);
            SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);
            pl->m_Work2 = fe;
        } else {
            if (wall & 1) {
                side = 1;
            }
            if (wall & 2) {
                side = 0;
            }
            if (side) {
                MotionSetCore(pl, &pl->Motion, PL_ARC(0x74), (int) PL_ARC(0x75), 5, 1, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, PL_ARC(0x74), (int) PL_ARC(0x75), 5, 0x41, 0);
            }
            SndCall(1, 0x48, &pl->pos, 0, 0, pl);
            SndCall(1, 0x11, &pl->getPartsPtr(4)->world, 0, 0, pl);
            pl->m_Work2 = 1;
        }
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->m_Work0 = 50;
        pl->m_Work1 = 15;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work2) {
            em2cEscapeCamMove((cEm2c*)pl->pEmCatch);
            if (pl->frame > 11.6999998f && pl->frame < 12.3000002f) {
                EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
                SndCall(5, 5, &pl->pos, 0, 0, pl);
            }
        } else {
            if (pl->frame > 10.6999998f && pl->frame < 11.3000002f) {
                SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
            }
            if (pl->frame > 21.7000008f && pl->frame < 22.2999992f) {
                SndCall(5, 0x14, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 36.7000008f && pl->frame < 37.2999992f) ||
                (pl->frame > 49.7000008f && pl->frame < 50.2999992f)) {
                SndCall(5, 2, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 37.7000008f && pl->frame < 38.2999992f) ||
                (pl->frame > 50.7000008f && pl->frame < 51.2999992f)) {
                SndCall(5, 3, &pl->pos, 0, 0, pl);
            }
        }
        if (pl->m_Work1) {
            pl->ang.y += Muku(&pl->pos, &pl->pEmCatch->pos, pl->ang.y, 0.392699093f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        if (MotionMoveF(pl, 0)) {
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
void em2cEscapeCamMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Camera* cam = &pG->Cam;
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
    PSMTXMultVec(pPLS->mat, &pos, &pos);
    PSMTXMultVec(pPLS->mat, &at, &at);
    PosToPos(&cam->param.at, &at, &w->cam.param.at, 1.0f);
    PosToPos(&cam->param.pos, &pos, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 9125 "D:/Bio4/Prog/em2c.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    CameraSetOrientationUp(&w->cam);
    CamCtrl.m_pExtraCamera = (s32) &w->cam;
}

// Action button callback "back-jump" (C_Wait / T_Wait strikes): the player's back jump (plemBackjump).
static void em2cBackjumpAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage(em, plemBackjump);
    w->x6B8 = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player routine of the back jump away from the tail strike, with its step SEs; escape scored.
static void plemBackjump(cPlayer* pl)
{
    int fe;

    pl->subArc = pl->pEmCatch->subArc;
    fe = pl->r_no_2;
    pl->dmg.m_Timer = 0x3C;
    switch (fe) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x85), 0, 5, 1, 5);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);
        pl->m_Work2 = fe;
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->m_Work0 = 35;
        pl->m_Work1 = fe;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else if (Key.on & 0x1F) {
            pl->m_Work1 = 1;
        }
        if (pl->frame > 20.7000008f && pl->frame < 21.2999992f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 33.7000008f && pl->frame < 34.2999992f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->m_Work1) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Action button callback "back-jump" of the HideAtk strike (plemBackjump2).
static void em2cBackjumpAction2(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage(em, plemBackjump2);
    w->x6B8 = 1;
    GameAddPoint(LVADD_CRITICALHIT);
}

// Player routine of the second back-jump variant (HideAtk), escape scored.
static void plemBackjump2(cPlayer* pl)
{
    int fe;

    pl->subArc = pl->pEmCatch->subArc;
    fe = pl->r_no_2;
    switch (fe) {
    case 0:
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x85), 0, 5, 1, 5);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->world, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->world, 0, 0, pl);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
        pl->m_Work2 = fe;
        GameAddPoint(LVADD_ESCAPEATTACK);
        pl->dmg.m_Timer = 0x14;
        pl->m_Work0 = 35;
        pl->m_Work1 = fe;
        pl->r_no_2++;
    case 1:
        if (pl->m_Work0) {
            pl->m_Work0--;
        } else if (Key.on & 0x1F) {
            pl->m_Work1 = 1;
        }
        if (pl->frame > 20.7000008f && pl->frame < 21.2999992f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 33.7000008f && pl->frame < 34.2999992f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->m_Work1) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Two-motion blend: m0 on the main work, m1 or m2 (by the sign of blendVal) on the blend work,
// weighted by |blendVal| / 256.
void em2cBlendMotSet(cEm2c* em, void* m0, void* m1, void* m2, int a, int b, int c, u16 d)
{
    Em2cWork* w = EM2C_WK(em);
    MotionWork* bm;
    int dd;
    int aa;
    asm("" : "=r"(aa) : "0"(a));            // COMPILER-DIFF: #2 (order only: gives a's copy the same chain length as d's so `mr r0,r7` keeps its place)
    asm("" : "=r"(dd) : "0"((int) d));     // COMPILER-DIFF: #2: the original masks the u16 `d` at each MotionSetCore call (`clrlwi r8,r25,16`)
    f32 val = fabsf(w->blendVal);
    void* m;
    int arg;

    MotionSetCore(em, &em->Motion, m0, aa, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    if (w->blendVal > 0.0f) {
        m = m1;
        arg = b;
    } else {
        m = m2;
        arg = c;
    }
    bm = EM2C_BLEND_MOT(w);
    MotionSetCore(em, bm, m, arg, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    em->motBlend = bm;
    bm->Brate = val * 0.00390625f;
    if (w->blendCnt) {
        w->blendCnt--;
    }
    w->blendSeq++;
    if ((u32) w->blendSeq >= em->frameMax) {
        w->blendSeq = 0;
    }
}

// Two-motion blend with one blend motion: m0 on the main work, m1 on the blend work weighted by
// |blendVal| (F_Walk steering).
void em2cBlendMotSet2(cEm2c* em, void* m0, void* m1, int a, int b, u16 d)
{
    Em2cWork* w = EM2C_WK(em);
    MotionWork* bm;
    int dd;
    dd = (int) d;
    f32 val = fabsf(w->blendVal);

    MotionSetCore(em, &em->Motion, m0, a, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    bm = EM2C_BLEND_MOT(w);
    MotionSetCore(em, bm, m1, b, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    em->motBlend = bm;
    bm->Brate = val * 0.00390625f;
    if (w->blendCnt) {
        w->blendCnt--;
    }
    w->blendSeq++;
    if ((u32) w->blendSeq >= em->frameMax) {
        w->blendSeq = 0;
    }
}

// Takes the room's em2c texture-render work (Ctrl12GetTexRenderEm2c) for the ice overlay (pTex);
// logs an error when none.
void em2cTexrenderInit(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8* tbl = w->texBlend;
    TexRenderMng* tex;
    int zero;

    tex = Ctrl12GetTexRenderEm2c(w->pCtrl12);
    w->pTex = tex;
    if (tex == 0) {
        pLog->err(0, 0, "em2cTexrenderInit:: Manager alloc failed!!");
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = w->pTex->texId;
    w->pTex->m_Rep_type = 1;
    w->pTex->m_H_size = w->pTex->m_W_size = 0x40;
    zero = 0;
    EstSet(0, -1, 0, 0, 0x24, 0, w->pTex->mask | 0x801, 0, zero, (void*) zero);
}

// Freezes the boss (liquid nitrogen): flag 0x800, ice guard guardCnt 900, the room's frozen flag
// (Room_flg[0] bit31), the ice texture blended over the model.
void em2cSetFreeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    void* tbl = w->texBlend;

    w->flags |= 0x800;
    w->guardCnt = 900;
    pGS->Room_flg[0] |= 0x80000000;
    if (w->pTex) {
        em->pModelInfo->setTexBlendTbl(tbl);
        em->pModelInfo->setBlendRatio(0xFF);
        em->pModelInfo->setBlendType(2);
    }
}

// 1 when an EMI type 0x10 "no wall climbing" point lies within 1500 units.
int em2cNoWallCk(cEm2c* em)
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

// Finds the alive tail enemy (id 0x2C type 1) into pTail.
void em2cGetTail(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u32 i;

    if (w->pTail) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2C) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->type != 1) {
            continue;
        }
        w->pTail = e;
        return;
    }
}

// 1 when a door / gate enemy (id 0x41 / 0x4E) stands within 2000 units (HideWait lurks longer there).
int em2cDoorCk(cEm2c* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41 && e->id != 0x4E) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            4000000.0f) {
            continue;
        }
        return 1;
    }
    return 0;
}

// Floor areas the enemy must not fall into (position, facing and half width per area).
static Vec em2c_floor_pos[3] = {
    { -21390.0f, 886.0f, -8534.0f },
    { -18407.0f, 886.0f, -31718.0f },
    { -11984.0f, 886.0f, -74395.0f },
};
static f32 em2c_floor_rot[3] = { 1.17999995f, -1.47000003f, 2.45000005f };
static f32 em2c_floor_w[3] = { 5000.0f, 5000.0f, 10000.0f };

// Which of the three room floor areas (em2c_floor_pos / rot / w) the boss stands in: the index, or -1.
int em2cFloorTypeCk(cEm2c* em)
{
    Mtx m;
    Vec v;
    u32 i;

    for (i = 0; i < 3; i++) {
        PSMTXRotRad(m, 'y', em2c_floor_rot[i]);
        TransMatrix(m, &em2c_floor_pos[i]);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &em->pos, &v);
        if (v.x > -em2c_floor_w[i] && v.x < em2c_floor_w[i] && v.z > -1000.0f) {
            return 0;
        }
    }
    return 1;
}

// Ambush points (position and facing); the player must be 5..10 m in front of one.
static Vec em2c_ambush_pos[2] = {
    { -20544.0f, 886.0f, -31480.0f },
    { -19194.0f, 886.0f, -7630.0f },
};
static f32 em2c_ambush_rot[2] = { -3.03999996f, 2.75f };

// When the player looks away, teleports the boss to one of the two ambush points (em2c_ambush_pos)
// and starts Ambush (4). 1 = set.
int em2cAmbushCk(cEm2c* em)
{
    Mtx m;
    Vec v;
    u32 i;

    if (em->plDist2 < 25000000.0f) {
        return 0;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.14159274f)) < 2.09439516f) {
        return 0;
    }
    for (i = 1; i < 2; i++) {
        PSMTXRotRad(m, 'y', em2c_ambush_rot[i]);
        TransMatrix(m, &em2c_ambush_pos[i]);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &pPL->pos, &v);
        if (v.z > 5000.0f && v.z < 10000.0f) {
            em->setPos(&em2c_ambush_pos[i]);
            em->pos_old = em->pos;
            em->ang.y = em2c_ambush_rot[i];
            EmRoutineSet(em, 1, 4, 0, 0);
            return 1;
        }
    }
    return 0;
}

// Ceiling fall position: the EMI type 0x12 point nearest the player (at least 1 m away), 4330 high.
void em2cGetFallPos(cEm2c* em)
{
    EmiData* emi;
    f32 best;
    f32 d;
    u32 i;

    em->pos = pPLS->pos;
    em->pos.y = 4330.0f;
    em->ang.y = pPLS->ang.y + 3.14159274f;
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    emi = (EmiData*) pGS->pEmi;
    if (emi == 0) {
        return;
    }
    best = 10000000000.0f;
    for (i = 0; i < ((EmiData*) pGS->pEmi)->n; i++) {
        EmiEntry* e = &((EmiData*) pGS->pEmi)->entry[i];

        if (e->type != 0x12) {
            continue;
        }
        d = (pPLS->pos.x - e->pos.x) * (pPLS->pos.x - e->pos.x) + (pPLS->pos.z - e->pos.z) * (pPLS->pos.z - e->pos.z);
        if (d < 1000000.0f) {
            continue;
        }
        if (d > best) {
            continue;
        }
        best = d;
        em->pos = e->pos;
        em->pos.y = 4330.0f;
    }
}

// Breath / hiss SEs while walking, throttled by the work timer.
void em2cBreathSe(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    if (w->breathTimer) {
        w->breathTimer--;
        return;
    }
    w->breathTimer = 59;
    w->breathSnd = SndCall(8, 0x46, &em->pos, em->id, 0, em);
}

// Stops the breath sound on the motion sounds that voice the enemy.
void em2cBreathSeStopCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u32 no;

    if (em->seNo == 0) {
        return;
    }
    no = em->seNo - 1;
    switch (no) {
    case 4:
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x24:
    case 0x26:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x39:
    case 0x3A:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        break;
    }
}
