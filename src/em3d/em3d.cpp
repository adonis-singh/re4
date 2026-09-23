// em3d module (D:/Bio4/Prog/em3d.cpp): the support helicopter. It patrols a fixed position table
// (em3d_R1_Patrol), flies to the position the room selects (em3d_R1_TargetMove, setTarget) and
// hovers there shooting its chain guns at the room targets or the enemies it finds
// (em3d_R1_Atk, em3dChainGunMove, em3dGetTargetEm) before it fires a rocket; the rooms drive it
// through the extra virtuals of cEm3d.
//
// Em3dInit is the module's EmInitFunc. Routines: r_no_0 0 init, 1 move with r_no_1: 0 patrol
// (hover at Patrol_pos, then circle), 1 fly to Em3d_pos_tbl[Target_area], 2 attack (hover 240
// frames shooting at Em3d_target_tbl[Target_area] / the nearest enemy near it, then a rocket,
// then the next area), 3 warp-in (setTargetPos). Em3dWork (em3d.h): Be_flg bit0 guns may fire
// (aimed at the target), bit1 the room may select a target (ckSelectEnable), bit2 the rocket is
// about to fire (ckMissileFire), bit3 an enemy locked on it (setEmLocked -> the "under fire"
// radio line), bit4 a target was requested, bit5 free fire (the player far from the patrol
// point), bit6 patrolling; Spd is the hover speed, Se_wait the radio message hold, pMissile[]
// the four rockets on parts 0xC..0xF. The helicopter itself cannot be killed (hp stays 1;
// weapon hits only trigger the pilot's radio lines).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3d.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "mes.h"
#include "sce.h"
#include "dbmodule.h"
#include "pad.h"
#include "pl_wep.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"


typedef void (*Em3dFunc)(cEm3d*);

static void em3d_R0_Init(cEm3d* em);
static void em3d_R0_Move(cEm3d* em);
static void em3d_R1_Patrol(cEm3d* em);
static void em3d_R1_TargetMove(cEm3d* em);
static void em3d_R1_Atk(cEm3d* em);
static void em3d_R1_WarpMove(cEm3d* em);




// Radio message `no` at the bottom of the screen, held for 90 frames.
static inline void em3dMesSet(Em3dWork* w, int no)
{
    SceMesSet(no, 0xB2, 1, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    w->Se_wait = 90;
}

// Pilot voice `no`: stop the running one first.
static inline void em3dVoice(cEm3d* em, Em3dWork* w, u16 no)
{
    SndStop(w->Se_id, 0);
    w->Se_id = SndCall(6, no, &em->pos, 0, 0, em);
}

// Hover: apply and damp the speed, vibrate (a macro for the same reason as EM3D_TURN_TO: the damp
// constant is loaded right before PSVECScale, not held across PSVECAdd).
#define EM3D_HOVER_MOVE(em, w, damp)                                                    \
    PSVECAdd(&(em)->pos, &(w)->Spd, &(em)->pos);                                        \
    PSVECScale(&(w)->Spd, &(w)->Spd, damp);                                             \
    em3dVibMove(em)

// Turn towards `target` by `rate` of the remaining angle, at most `lim` per frame. A macro: an
// inline's constant parameters would stay live across the calls (PI in a callee-saved FPR), the
// original reloads every constant from the pool.
#define EM3D_TURN_TO(em, target, rate, lim, nlim)                                        \
    {                                                                                   \
        f32 a = Muku(&(em)->pos, target, (em)->ang.y, PI) * (rate);                     \
                                                                                        \
        if (a > (lim)) {                                                                \
            a = (lim);                                                                  \
        }                                                                               \
        if (a < (nlim)) {                                                               \
            a = (nlim);                                                                 \
        }                                                                               \
        (em)->ang.y += a;                                                               \
        (em)->ang.y = LIMIT_ANGLE((em)->ang.y);                                         \
    }

// World matrix from the coordinates, then the parts.
static inline void em3dMatCalc(cEm3d* em)
{
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

// REL entry: registers the enemy constructor.
extern "C" void _prolog()
{
    OSReport("em3d prolog Ok\n");
    EmInitFunc = Em3dInit;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// cUnit::setNoSuspend override: the helicopter and its four rockets keep moving through pauses
// (be_flag bit11).
void cEm3d::setNoSuspend(int on)
{
    Em3dWork* w = EM3D_WK(this);
    u32 i;

    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    for (i = 0; i < 4; i++) {
        if (w->pMissile[i]) {
            w->pMissile[i]->setNoSuspend(on);
        }
    }
}

// EmInitFunc: placement-constructs the helicopter in the cEm work.
void Em3dInit(cEm* em)
{
    new (em) cEm3d();
}

// A weapon hit on the helicopter (cEm::dmHit, no damage taken): when no radio line is running the
// pilot complains -- "close" (hit within 3 m of the player or a rocket / grenade: voice 0x6D,
// message 3) or "far" (voice 0x6C, message 2).
void em3dDmCk(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    int near;
    u8 wep;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    if (w->Se_wait) {
        return;
    }
    near = 0;
    if ((em->dmg.m_PosFrom.x - pPL->pos.x) * (em->dmg.m_PosFrom.x - pPL->pos.x) + (em->dmg.m_PosFrom.y - pPL->pos.y) * (em->dmg.m_PosFrom.y - pPL->pos.y)
            + (em->dmg.m_PosFrom.z - pPL->pos.z) * (em->dmg.m_PosFrom.z - pPL->pos.z)
        < 9000000.0f) {
        near = 1;
    }
    wep = em->dmg.m_Wep;
    if (wep == 0xD) {
        near = 1;
    }
    if (wep == 0x12) {
        near = 1;
    }
    if (wep == 0x13) {
        near = 1;
    }
    if (near) {
        em3dVoice(em, w, 0x6D);
        em3dMesSet(w, 3);
    } else {
        em3dVoice(em, w, 0x6C);
        em3dMesSet(w, 2);
    }
}

Em3dFunc Em3d_R0_move_tbl[4] = {
    em3d_R0_Init,
    em3d_R0_Move,
    NULL,
    NULL,
};

static Em3dFunc Em3d_R1_move_tbl[4] = {
    em3d_R1_Patrol,
    em3d_R1_TargetMove,
    em3d_R1_Atk,
    em3d_R1_WarpMove,
};

// Parts the four rockets hang on.
static u8 em3d_missile_parts[4] = { 0xC, 0xD, 0xE, 0xF };

// Flight positions and the targets shot at from them (setTarget's no).
Vec Em3d_pos_tbl[8] = {
    { 64477.0f, 16694.0f, 39279.0f },
    { 71742.0f, 18189.0f, 6156.0f },
    { 76500.0f, 21158.0f, 11073.0f },
    { 78400.0f, 22195.0f, 18985.0f },
    { 30104.0f, 18091.0f, -7604.0f },
    { 38118.0f, 17783.0f, -12089.0f },
    { 39018.0f, 17783.0f, -11119.0f },
    { 38763.0f, 22559.0f, -319.0f },
};

Vec Em3d_target_tbl[8] = {
    { 60169.0f, 12923.0f, 27734.0f },
    { 69984.0f, 11609.0f, 25335.0f },
    { 61087.0f, 12269.0f, 14072.0f },
    { 76253.0f, 15319.0f, 6588.0f },
    { 41774.0f, 10000.0f, -12444.0f },
    { 35073.0f, 10920.0f, 730.0f },
    { 27033.0f, 6732.0f, -13125.0f },
    { 25807.0f, 13198.0f, -13498.0f },
};
// The module's .data is padded to 8 bytes after the last table.
asm(".section .data\n\t.balign 8\n\t.text");

// Per-frame update (emMove): the hit check, the radio hold countdown, the r_no_0 routine (0xFF
// after a failed init destroys the work), the locked target check, the nose pitch, the rotors,
// parts matrices, the chain guns (with hp 0 so its own shots skip it), enemy / scenery collision
// and the "under fire" radio line when an enemy locked on it this frame (Be_flg bit3).
void cEm3d::move()
{
    Em3dWork* w = EM3D_WK(this);

    if (r_no_0) {
        em3dDmCk(this);
    }
    w->Be_flg &= ~0x47;
    if (w->Se_wait) {
        w->Se_wait--;
    }
    Em3d_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em3dTargetEmUpdate(this);
    em3dHeliPitchMove(this);
    em3dRoterMove(this);
    partsWorldCalc();
    hp = 0;
    em3dChainGunMove(this);
    hp = 1;
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    {
        Vec target = Em3d_target_tbl[w->Target_area];

        if (w->Be_flg & 8) {
            em3dVoice(this, w, 0x6C);
            em3dMesSet(w, 2);
        }
    }
    w->Be_flg &= ~0x8;
}

// r_no_0 == 0: creation: the model (archive 5/6), a 10 m light area, the collision cylinder (no
// enemy collision bits), not lockable, Ashley does not ask for help, a big hit box behind the
// nose, lock-on parts 2, effects (archive 4 as group 0x32), a random hover wobble phase / speed,
// search range 12 m, the four rockets hung on parts 0xC..0xF, the first patrol position, the
// rotor effects and SE; then patrol (1/0).
static void em3d_R0_Init(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    cAtariInfo* at;
    u32 i;
    int zero;

    if (em->modelInit(ARC(5), ARC(6)) == 0) {
        pLog->err(0, 0, "em3d() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    at = &em->atari;
    at->init(0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);
    zero = 0;
    at->off();
    em->setStatus(EM_STATUS_LOCKOFF);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInit(em, 0.0f, 750.0f, -3000.0f, 1500.0f, 6000.0f, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS | YAT_FLAG_NO_MARK);
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), EFF_EM3D, 0);
    w->Be_flg = zero;
    w->Fire_wait = zero;
    w->vibAng.x = fRand1_1() * PI;
    w->vibAng.y = fRand1_1() * PI;
    w->vibAng.z = fRand1_1() * PI;
    w->Vib_v.x = fRand0_1() * 0.17453292f + 0.17453292f;
    w->Vib_v.y = fRand0_1() * 0.05235988f + 0.13962634f;
    w->Vib_v.z = fRand0_1() * 0.05235988f + 0.2443461f;
    w->Search_len = 12000.0f;
    w->Spd.x = 0.0f;
    w->Spd.y = 0.0f;
    w->Spd.z = 0.0f;
    w->Target_area = 0;
    w->pTargetEm = 0;
    w->Target_chg = 150;
    for (i = 0; i < 4; i++) {
        Vec pos;
        Vec rot;

        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pMissile[i] = SetHeliMissile(ARC(7), ARC(8), &pos, &rot, 0);
        if (w->pMissile[i]) {
            w->pMissile[i]->setParent(em, em3d_missile_parts[i], 0);
        }
    }
    w->Patrol_pos = Em3d_pos_tbl[0];
    EstSet(em, -1, 0, 0, EFF_EM3D, 0, 1, ESP_CORE_KIND_NONE, em, 0);
    EstSet(em, -1, 0, 0, EFF_EM3D, 3, 1, ESP_CORE_KIND_NONE, em, 0);
    EmRoutineSet(em, 1, 0, 0, 0);
    SndCall(6, 0, &em->pos, 0, 0, em);
    em3d_R0_Move(em);
}

// r_no_0 == 1: dispatches the r_no_1 state.
static void em3d_R0_Move(cEm3d* em)
{
    Em3d_R1_move_tbl[em->r_no_1](em);
}

// r_no_1 == 0: patrol: hovers facing Patrol_pos for 90..179 frames (the room may select a target
// once the 30-frame entry delay from an attack has passed: Be_flg bit1), then circles forward /
// climbing (up to 25 m) for 120..149 frames; in free-fire mode (bit5) the guns fire when the
// player is farther than 5 m from the patrol point. A setTarget request (Target_ck) -> fly to
// the target (1).
static void em3d_R1_Patrol(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target;

    w->Be_flg |= 0x40;
    target = w->Patrol_pos;
    switch (em->r_no_2) {
    case 0:
        if (em->r_no_3) {
            w->Timer = 30;
        } else {
            w->Timer = 0;
        }
        em->r_no_3 = 0;
        w->Be_flg &= ~0x10;
        w->Timer2 = Rnd() % 90 + 90;
        w->pTargetEm = 0;
        em->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            w->Be_flg |= 2;
        }
        EM3D_HOVER_MOVE(em, w, 0.95f);
        EM3D_TURN_TO(em, &target, 0.05f, 0.034906585f, -0.034906585f);
        if (w->Timer2) {
            w->Timer2--;
        } else {
            em->r_no_2++;
        }
        break;
    case 2:
        w->Timer = Rnd() % 30 + 120;
        em->r_no_2++;
    case 3: {
        Vec v;

        w->Be_flg |= 2;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 30.0f;
        if (em->pos.y < 25000.0f) {
            v.y = 15.0f;
        }
        PSMTXMultVecSR(em->mat, &v, &v);
        PSVECAdd(&w->Spd, &v, &w->Spd);
        em->ang.y += 0.012271847f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if (w->Be_flg & 0x20) {
            if ((w->Patrol_pos.x - pPL->pos.x) * (w->Patrol_pos.x - pPL->pos.x)
                    + (w->Patrol_pos.y - pPL->pos.y) * (w->Patrol_pos.y - pPL->pos.y)
                    + (w->Patrol_pos.z - pPL->pos.z) * (w->Patrol_pos.z - pPL->pos.z)
                > 25000000.0f) {
                w->Be_flg |= 1;
            }
        }
        if (w->Timer) {
            w->Timer--;
        } else {
            em->r_no_2 = 0;
        }
        break;
    }
    }
    em3dMatCalc(em);
    if (w->Target_ck) {
        // Plain byte stores: one QI zero serves targetSet and the routine bytes (the int inline's SI zero
        // would be a second `li`).
        w->Target_ck = 0;
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
}

// r_no_1 == 1: fly to the area's flight position: the "on my way" voice, turn to face it, then
// fly forward (30 units/frame, climbing / descending to its height) until within 3 m -> attack (2).
// Debug mode 7 draws the flight line and the target sphere.
static void em3d_R1_TargetMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec v;
    Vec target = Em3d_target_tbl[w->Target_area];
    Vec pos = Em3d_pos_tbl[w->Target_area];

    switch (em->r_no_2) {
    case 0:
        em3dVoice(em, w, 0x69);
        em->r_no_2++;
    case 1:
        EM3D_TURN_TO(em, &pos, 0.1f, 0.034906585f, -0.034906585f);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if (fabsf(Muku(&em->pos, &pos, em->ang.y, PI)) < 0.034906585f) {
            em->r_no_2++;
        }
        break;
    case 2:
        w->Spd.x = 0.0f;
        w->Spd.y = 0.0f;
        w->Spd.z = 0.0f;
        em->r_no_2++;
    case 3:
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 30.0f;
        if (em->pos.y > pos.y + 500.0f) {
            v.y = -15.0f;
        }
        if (em->pos.y < pos.y - 500.0f) {
            v.y = 15.0f;
        }
        PSMTXMultVecSR(em->mat, &v, &v);
        PSVECAdd(&w->Spd, &v, &w->Spd);
        EM3D_TURN_TO(em, &pos, 0.05f, 0.034906585f, -0.034906585f);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) < 9000000.0f) {
            w->pTargetEm = 0;
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em3dMatCalc(em);
    if (pG->debug_mode == 7) {
        Draw_line3d(&em->pos, &pos, -1, 0);
        Draw_sphere(&target, 8000.0f, -1, 1, 1);
    }
}

// r_no_1 == 2: the attack: hovers at the flight height facing the area's target for 240 frames,
// picking enemies near the target to gun (em3dGetTargetEm; `count` = kills claimed); the player
// within 8 m of the target gets the "get clear" line and the timer restarts once; the last 30
// frames arm the rocket warning (Be_flg bit2), then the rocket fires; 45 more frames, then the
// next area (wrapping 0..7), a result line (6 with 3+ targets, else 5) and back to patrol with
// the 30-frame delay. Be_flg bit0 (guns may fire) while facing the target within 30 degrees.
static void em3d_R1_Atk(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target = Em3d_target_tbl[w->Target_area];
    Vec pos = Em3d_pos_tbl[w->Target_area];

    switch (em->r_no_2) {
    case 0:
        w->Timer2 = 0;
        w->Timer = 240;
        w->TmpU32 = 0;
        em->r_no_2++;
    case 1:
        if (em->pos.y > pos.y + 500.0f) {
            w->Spd.y = -15.0f;
        }
        if (em->pos.y < pos.y - 500.0f) {
            w->Spd.y = 15.0f;
        }
        EM3D_HOVER_MOVE(em, w, 0.93f);
        EM3D_TURN_TO(em, &target, 0.05f, 0.034906585f, -0.034906585f);
        if (w->pTargetEm == 0) {
            em3dGetTargetEm(em);
            if (w->pTargetEm) {
                w->Timer2++;
            }
        }
        if (w->Timer == 0) {
            em3dRocketFire(em);
            em->r_no_2++;
            break;
        }
        w->Timer--;
        if (w->Se_wait == 0
            && (pPL->pos.x - target.x) * (pPL->pos.x - target.x) + (pPL->pos.y - target.y) * (pPL->pos.y - target.y)
                    + (pPL->pos.z - target.z) * (pPL->pos.z - target.z)
                < 64000000.0f) {
            w->Se_id = SndCall(6, 0x6E, &em->pos, 0, 0, em);
            em3dMesSet(w, 4);
            if (w->TmpU32 == 0) {
                w->TmpU32 = 1;
                if (w->Timer > 30 && w->Timer < 240) {
                    w->Timer = 240;
                }
            }
        }
        if (w->Timer < 30) {
            w->Be_flg |= 4;
        }
        break;
    case 2:
        w->Timer = 45;
        em->r_no_2++;
    case 3:
        if (em->pos.y > pos.y + 500.0f) {
            w->Spd.y = -30.0f;
        }
        if (em->pos.y < pos.y - 500.0f) {
            w->Spd.y = 30.0f;
        }
        EM3D_HOVER_MOVE(em, w, 0.95f);
        EM3D_TURN_TO(em, &target, 0.1f, 0.20943952f, -0.20943952f);
        if (w->Timer) {
            w->Timer--;
            break;
        }
        w->Target_area++;
        if (w->Target_area > 7) {
            w->Target_area = 0;
        }
        if ((s16) pG->pl_life > 0 && w->Se_wait == 0) {
            if (w->Timer2 > 2) {
                w->Se_id = SndCall(6, 0x72, &em->pos, 0, 0, em);
                em3dMesSet(w, 6);
            } else {
                w->Se_id = SndCall(6, 0x71, &em->pos, 0, 0, em);
                em3dMesSet(w, 5);
            }
        }
        EmRoutineSet(em, 1, 0, 0, 1);
        break;
    }
    em3dMatCalc(em);
    if (fabsf(Muku(&em->pos, &target, em->ang.y, PI)) < 0.5235988f) {
        w->Be_flg |= 1;
    }
}

// r_no_1 == 3 (setTargetPos): flies in from the placed position at 1000 units/frame sideways
// (its local +X), turning to the flight position, for 45 frames -> attack (2).
static void em3d_R1_WarpMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Mtx m;
    Vec target = Em3d_target_tbl[w->Target_area];
    Vec pos = Em3d_pos_tbl[w->Target_area];

    switch (em->r_no_2) {
    case 0:
        PSMTXRotRad(m, 'y', em->ang.y);
        w->Spd.x = 1000.0f;
        w->Spd.y = 0.0f;
        w->Spd.z = 0.0f;
        PSMTXMultVecSR(m, &w->Spd, &w->Spd);
        w->Timer = 45;
        em->r_no_2++;
    case 1:
        EM3D_TURN_TO(em, &pos, 0.1f, 0.034906585f, -0.034906585f);
        Muku(&em->pos, &pos, em->ang.y, PI);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if (w->Timer) {
            w->Timer--;
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em3dMatCalc(em);
    if (pG->debug_mode == 7) {
        Draw_line3d(&em->pos, &pos, -1, 0);
        Draw_sphere(&target, 8000.0f, -1, 1, 1);
    }
}

// Spins the main rotor (parts 0xA, yaw) and the tail rotor (parts 0xB, pitch) 35 degrees per frame.
void em3dRoterMove(cEm3d* em)
{
    cParts* p;

    p = em->getPartsPtr(0xA);
    p->ang.y += 0.61086524f;
    p->ang.y = LIMIT_ANGLE(p->ang.y);
    p = em->getPartsPtr(0xB);
    p->ang.x -= 0.61086524f;
    p->ang.y = LIMIT_ANGLE(p->ang.y);
}

// Aim one gun mount at `aim`: the mount (parts `gun`) pitches, the barrel (parts `gun` + 1) yaws,
// each towards the helicopter's own angle plus the offset to the target, limited to +/- lim. Macros
// (like EM3D_TURN_TO): the constants are loaded where they are used and shared by cse between the
// two identical mounts, an inline's parameters would be live across the whole function. The
// temporaries (p, len, angX, angY, m) are the FUNCTION's variables: one multi-set `angY` pseudo
// conflicts with every shared clamp constant and is allocated after `limY` (f29 below f30);
// macro-local variables give a short block-local angY that takes f30 first.
#define EM3D_GUN_AIM(em, aim, d, mount, gun, rotX, rotY, limX, nlimX, limY, nlimY)     \
    {                                                                                   \
        p = (em)->getPartsPtr(mount);                                                   \
        PSVECSubtract(aim, &p->world, &(d));                                         \
        len = SQRTF((d).x * (d).x + (d).z * (d).z);                                     \
        angX = -atan2f((d).y, len);                                                     \
        angY = atan2f((d).x, (d).z);                                                    \
        p = (em)->getPartsPtr(gun);                                                     \
        m = Muku2(rotX, angX, limX);                                                    \
        p->ang.x += Muku2(p->ang.x, m, 0.024543693f);                                   \
        if (p->ang.x > (limX)) {                                                        \
            p->ang.x = (limX);                                                          \
        }                                                                               \
        if (p->ang.x < (nlimX)) {                                                       \
            p->ang.x = (nlimX);                                                         \
        }                                                                               \
        p->ang.x = LIMIT_ANGLE(p->ang.x);                                               \
        p = (em)->getPartsPtr((gun) + 1);                                               \
        m = Muku2(rotY, angY, limY);                                                    \
        p->ang.y += Muku2(p->ang.y, m, 0.024543693f);                                   \
        if (p->ang.y > (limY)) {                                                        \
            p->ang.y = (limY);                                                          \
        }                                                                               \
        if (p->ang.y < (nlimY)) {                                                       \
            p->ang.y = (nlimY);                                                         \
        }                                                                               \
        p->ang.y = LIMIT_ANGLE(p->ang.y);                                               \
    }

// One chain gun shot from parts `mount`: a random line ahead, the weapon hit check, then the wall
// hit effect (and its sound for the first gun). Uses the function's `p` (a multi-set pseudo):
// local-alloc then cannot tie it to the `&p->mat` addi, and it keeps its own callee-saved r31.
#define EM3D_GUN_SHOT(em, mount, spread, len, se, zero)                                 \
    {                                                                                   \
        p = (em)->getPartsPtr(mount);                                                   \
                                                                                        \
        a.x = 0.0f;                                                                     \
        a.y = 0.0f;                                                                     \
        a.z = 0.0f;                                                                     \
        b.x = fRand1_1() * (spread);                                                    \
        b.y = fRand1_1() * (spread);                                                    \
        b.z = (len);                                                                    \
        PSMTXMultVec(p->mat, &a, &a);                                                   \
        PSMTXMultVec(p->mat, &b, &b);                                                   \
        PlWepHitCheck2(0, &a, &b, 0xA, 3, 6000.0f);                                     \
        if (EatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0)) {                                \
            f32 l = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);                               \
                                                                                        \
            rot.x = -atan2f(nrm.y, l);                                                  \
            rot.y = atan2f(nrm.x, nrm.z);                                               \
            rot.z = 0.0f;                                                               \
            PSVECScale(&nrm, &s, 30.0f);                                                \
            PSVECAdd(&hit, &s, &hit);                                                   \
            EstSet(0, -1, &hit, &rot, EFF_EM3D, 6, 0, ESP_CORE_KIND_NONE, (void*) (zero), (void*) (zero));             \
            if (se) {                                                                   \
                SndCall(6, 0xA, &hit, 0, 0, em);                                        \
            }                                                                           \
        }                                                                               \
    }

// Aims the two chain gun mounts (parts 4/2, 7/5; +-30 / +-20 degrees) at the locked enemy (1 m
// above it) or the area target (the patrol point while patrolling), spins the barrels, aims the
// rocket pod (parts 9/8, +-45 degrees) at the area target; while Be_flg bit0 both guns fire every
// 3 frames: muzzle effects, SE, and a random hit line per gun (weapon 0xA, no player damage
// flags) with a wall-hit effect (and SE for the first gun).
void em3dChainGunMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target;
    Vec aim;
    Vec d;
    Vec a;
    Vec b;
    Vec hit;
    Vec s;
    Vec nrm;
    Vec rot;
    f32 rotX;
    f32 rotY;
    cParts* p;
    f32 len;
    f32 angX;
    f32 angY;
    f32 m;
    int noAim;
    int t;

    if (w->Be_flg & 0x40) {
        target = w->Patrol_pos;
        aim = target;
    } else {
        target = Em3d_target_tbl[w->Target_area];
        if ((w->Be_flg & 1) && w->pTargetEm) {
            aim = w->pTargetEm->pos;
            aim.y += 1000.0f;
        }
    }
    rotX = em->getPartsPtr(0)->ang.x;
    rotY = em->ang.y;
    EM3D_GUN_AIM(em, &aim, d, 4, 2, rotX, rotY, 0.5235988f, -0.5235988f, 0.34906584f, -0.34906584f);
    EM3D_GUN_AIM(em, &aim, d, 7, 5, rotX, rotY, 0.5235988f, -0.5235988f, 0.34906584f, -0.34906584f);
    p = em->getPartsPtr(4);
    p->ang.z += 0.34906584f;
    p->ang.z = LIMIT_ANGLE(p->ang.z);
    p = em->getPartsPtr(7);
    p->ang.z += 0.34906584f;
    p->ang.z = LIMIT_ANGLE(p->ang.z);
    EM3D_GUN_AIM(em, &target, d, 9, 8, rotX, rotY, 0.7853982f, -0.7853982f, 0.7853982f, -0.7853982f);
    noAim = !(w->Be_flg & 1);
    if (noAim) {
        return;
    }
    t = w->Fire_wait;
    if (t) {
        w->Fire_wait--;
        return;
    }
    w->Fire_wait = 2;
    EstSet(em, -1, 0, 0, EFF_EM3D, 1, 1, ESP_CORE_KIND_NONE, em, (void*) t);
    EstSet(em, -1, 0, 0, EFF_EM3D, 2, 1, ESP_CORE_KIND_NONE, em, (void*) t);
    SndCall(6, 1, &em->pos, 0, 0, em);
    EM3D_GUN_SHOT(em, 4, 5000.0f, 100000.0f, 1, t);
    EM3D_GUN_SHOT(em, 7, 2000.0f, 300000.0f, 0, t);
}

// Nose pitch of the fuselage (parts 0): 15 degrees plus up to 20 degrees with the horizontal
// speed, eased 0.9/0.1.
void em3dHeliPitchMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    cParts* p;
    f32 v;
    f32 ang;

    p = em->getPartsPtr(0);
    v = SQRTF(w->Spd.x * w->Spd.x + w->Spd.z * w->Spd.z) * 0.01f;
    if (v > 1.0f) {
        v = 1.0f;
    }
    ang = v * 0.34906584f + 0.2617994f;
    p->ang.x = p->ang.x * 0.9f + ang * 0.1f;
}

// Hover wobble: three sine offsets (20 / 10 / 20 units) at the random speeds set at init.
void em3dVibMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);

    w->vibAng.x += w->Vib_v.x;
    em->pos.x += SINF(w->vibAng.x) * 20.0f;
    w->vibAng.y += w->Vib_v.y;
    em->pos.y += SINF(w->vibAng.y) * 10.0f;
    w->vibAng.z += w->Vib_v.z;
    em->pos.z += SINF(w->vibAng.z) * 20.0f;
}

// Picks an enemy to gun: the first live, visible enemy (ids 0x10..0x40) within Search_len of the
// area target, within 30 degrees of the helicopter -> target line, with a clear line of sight
// (no effect collision of mask 0x404000) -> pTargetEm. Returns 1 when one was found.
int em3dGetTargetEm(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target = Em3d_target_tbl[w->Target_area];
    Vec a;
    Vec b;
    Vec dir;
    Vec d;
    u32 i;

    PSVECSubtract(&target, &em->pos, &dir);
#line 1218 "D:/Bio4/Prog/em3d.cpp"
    VECNormalize(&dir, &dir);
    a = em->pos;
    w->pTargetEm = 0;
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        int dead = !(e->be_flag & 1);

        if (dead) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x40) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if ((target.x - e->pos.x) * (target.x - e->pos.x) + (target.y - e->pos.y) * (target.y - e->pos.y)
                + (target.z - e->pos.z) * (target.z - e->pos.z)
            > w->Search_len * w->Search_len) {
            continue;
        }
        PSVECSubtract(&e->pos, &em->pos, &d);
#line 1240 "D:/Bio4/Prog/em3d.cpp"
        VECNormalize(&d, &d);
        if (acosf(PSVECDotProduct(&dir, &d)) > 0.5235988f) {
            continue;
        }
        b = e->pos;
        b.y += 1000.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) == 0) {
            w->pTargetEm = e;
            return 1;
        }
    }
    return 0;
}

// Drops the gunned enemy once it is dead or out of sight.
void em3dTargetEmUpdate(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    cEm* e = w->pTargetEm;

    if (e) {
        if (e->hp > 0) {
            Vec a = em->pos;
            Vec b = e->pos;

            b.y += 1000.0f;
            if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) == 0) {
                return;
            }
        }
        w->pTargetEm = 0;
    }
}

// Fires a rocket at the area target: the first pod slot; an empty slot gets a fresh missile
// created on its parts first, a loaded one is launched and the slot cleared.
void em3dRocketFire(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target = Em3d_target_tbl[w->Target_area];
    int no;

    // A loop left at its first iteration: the index stays a register (`lwzx`/`stwx` with the zero)
    // and the missile load is not hoisted above the target copy.
    for (no = 0; no < 4; no++) {
        if (w->pMissile[no] == 0) {
            Vec pos;
            Vec rot;
            cObjMissile* m;

            pos.x = 0.0f;
            pos.y = 0.0f;
            pos.z = 0.0f;
            rot.x = 0.0f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            m = SetHeliMissile(ARC(7), ARC(8), &pos, &rot, 0);
            if (m) {
                m->setParent(em, em3d_missile_parts[no], 0);
                m->setFire(&target);
            }
        } else {
            w->pMissile[no]->setFire(&target);
            w->pMissile[no] = 0;
        }
        break;
    }
}

// Room query: 1 while a new target may be selected (patrolling past the entry delay).
int cEm3d::ckSelectEnable()
{
    int ret = 0;

    if (EM3D_WK(this)->Be_flg & 2) {
        ret = 1;
    }
    return ret;
}

// Room script: attack area `no` (0..7) searching enemies within `range` of its target; the
// patrol routine picks the request up next frame.
void cEm3d::setTarget(u32 no, f32 range)
{
    Em3dWork* w = EM3D_WK(this);

    w->Target_ck = 1;
    w->Be_flg |= 0x10;
    w->Target_area = no;
    if (no > 7) {
        w->Target_area = 7;
    }
    w->Search_len = range;
}

// Room script: place the helicopter at `pos` / `rotY` and attack area `no` at once through the
// warp-in state (1/3).
void cEm3d::setTargetPos(u32 no, Vec* pos, f32 rotY, f32 range)
{
    Em3dWork* w = EM3D_WK(this);

    w->Target_ck = 1;
    w->Target_area = no;
    if (no > 7) {
        w->Target_area = 7;
    }
    w->Be_flg |= 0x10;
    w->Search_len = range;
    ang.y = rotY;
    setPos(pos);
    // Plain byte stores: the QI one of targetSet is reused for xFC (an int inline's SI one is a second `li`).
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Room query: 1 during the last 30 frames before the rocket launch.
int cEm3d::ckMissileFire()
{
    int ret = 0;

    if (EM3D_WK(this)->Be_flg & 4) {
        ret = 1;
    }
    return ret;
}

// Enemies call this when they aim at the helicopter: the pilot's "under fire" line this frame.
void cEm3d::setEmLocked()
{
    EM3D_WK(this)->Be_flg |= 8;
}

// Room script: the hover point of the patrol state.
void cEm3d::setPatrolPos(Vec* pos)
{
    if (pos) {
        EM3D_WK(this)->Patrol_pos = *pos;
    }
}

// Room script: free fire while patrolling (the guns fire once the player is away from the patrol point).
void cEm3d::setFreeFire()
{
    EM3D_WK(this)->Be_flg |= 0x20;
}
