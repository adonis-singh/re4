// em3b module (D:/Bio4/Prog/em3b.cpp): the truck (type 0) that runs into the barricade and the mine
// carts (type 1 running down the track, type 2 stopped). The driver is another enemy (work pDriver);
// the vehicles run over the player, the partner and any Ganado in front (em3bRunDownCk*).
//
// Em3bInit is the module's EmInitFunc. The vehicle sits at the origin and is placed by its
// motion (the room's matrix). Routines: r_no_0 0 init, 1 move, 4 scenario; r_no_1 for the truck:
// 0 wait (flag bit0 from the room starts it), 1 run (450 frames; the driver dying or the truck's
// hp at 1 makes it veer at frame 250 / 320), 2 run into the barricade / off the road (r_no_3 =
// direction); for the carts: 3 wait, 4 run down the track, 5 hit (explodes 80 frames after being
// stopped), 6 lost, 7 stopped-cart explosion. em->flag bit1 = the vehicle is finished (the room
// reads it); hp 1 / dmgWait is the "engine on fire" state (ckFire). Em3bWork (em3b.h): timer /
// seTimer, sndId / sndId2 SE handles, espKind, pDriver, hit.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3b.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "snd.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"



typedef void (*Em3bFunc)(cEm3b*);

static void em3b_R0_Init(cEm3b* em);
static void em3b_R0_Move(cEm3b* em);
static void em3b_R1_Truck_Wait(cEm3b* em);
static void em3b_R1_Truck_Run(cEm3b* em);
static void em3b_R1_Truck_RunInto(cEm3b* em);
static void em3b_R1_Cart_Wait(cEm3b* em);
static void em3b_R1_Cart_Run(cEm3b* em);
static void em3b_R1_Cart_Damage(cEm3b* em);
static void em3b_R1_StopCart_Damage(cEm3b* em);
static void em3b_R1_Cart_Lost(cEm3b* em);
static void subem3bRunDown();




// REL entry: registers the enemy constructor.
extern "C" void _prolog()
{
    OSReport("em3b prolog Ok\n");
    EmInitFunc = Em3bInit;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// EmInitFunc: placement-constructs the vehicle in the cEm work.
void Em3bInit(cEm* em)
{
    new (em) cEm3b();
}

// Truck damage of the frame: consumes cEm::dmHit (grenades / flash / mines ignored); hp loss by
// weapon class (200 small arms, 500 shotgun close / 200 far, 1000 magnums / launchers), the
// metal-hit spark and SE; the last hp point (hp <= 1) starts the burning engine: effect 0x23,
// SEs and a 150-frame dmgWait.
void em3bDmCkTruck(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int wep;
    int dmg;
    Vec* pos;
    cModel* p;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A || wep == 0xE) {
        return;
    }
    p = em->getPartsPtr(0);
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
        dmg = 200;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 500;
        if (em->l_pl > 16000000.0f) {
            dmg = 200;
        }
        break;
    case 5:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x29:
    case 0x2C:
    default:   // the default-grouped values are real tree nodes (root 0x10-0x11, casetree.py)
        dmg = 1000;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    LifeDownSet2(em, dmg, 0, 1);
    // the parts pointer crosses LifeDownSet2 (callee-saved copy right after getPartsPtr) and the
    // worldPos address is formed here; sched1 hoists the addi above the call
    pos = &p->world;
    EmDmBloodSet2(em, 1, 0x21, 0, 0, 0);
    SndCall(6, 4, pos, 0, 0, em);
    if (em->hp <= 1 && w->dmgWait == 0) {
        w->dmgWait = 150;
        EstSet(em, -1, 0, 0, EFF_ROOM, 0x23, 0, ESP_CORE_KIND_NONE, em, 0);
        SndCall(6, 6, pos, 0, 0, em);
        w->sndId = SndCall(6, 0xC, pos, 0, 0, em);
    }
}

// Running cart damage: a damage area (explosion kinds 1 / 4 / 5 / 7) or any weapon hit except the
// flash grenade (0x17 / 0x2A) stops the cart: the hit spark (a dull one for melee / knife / mines),
// dmgWait 150, the stop effect and -> r_no_1 5 (hit).
void em3bDmCkCart(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);

    if ((em->be_flag & 2) && !EmDeadCk(em) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            // the death body is written here AND in case 0x16 (jump2 cross-jumps the copies into the
            // later one; each copy stores the dmgWait register cse knows to be 0)
            if (w->dmgWait == 0) {
                w->dmgWait = 150;
                EmRoutineSet(em, 1, 5, 0, 0);
                EstSet(em, -1, 0, 0, EFF_OBM34, 1, 0, w->espKind, em, 0);
                w->dmgWait = 150;
                return;
            }
        }
    }
    if (em->dmg.m_Flag) {
        em->dmg.m_Flag = 0;
        em->dmg.m_Timer = 1;
        // dmWep read directly at both uses: the byte store between them forces the reload the
        // target has before the switch (a u8 local keeps one load)
        if (em->dmg.m_Wep == 0x10) {
            em->dmg.m_Timer = 0x11;
        }
        switch (em->dmg.m_Wep) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x1B:
        case 0x1D:
        case 0x21:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2B:
        case 0x2C:
        case 0x2D:
            EmDmBloodSet2(em, 0xCA, 0, 0, 0, 0);
            goto stop_ck;
        case 0x17:
        case 0x2A:
            break;
        case 0:
        case 0xE:
        case 0x10:
        case 0x14:
        case 0x15:
        case 0x18:
        default:   // the default-grouped values are real tree nodes (root 0x16, casetree.py)
            EmDmBloodSet2(em, 0xCA, 4, 0, 0, 0);
            break;
        case 0x16:
        stop_ck:   // laid out after the default arm: the blood arm reaches it through the goto
            if (w->dmgWait == 0) {
                w->dmgWait = 150;
                EmRoutineSet(em, 1, 5, 0, 0);
                EstSet(em, -1, 0, 0, EFF_OBM34, 1, 0, w->espKind, em, 0);
                w->dmgWait = 150;
            }
            break;
        }
    }
}

// Stopped cart damage: the same triggers as the running cart (a damage area or any hit but the
// flash grenade) kill it (hp 0) and -> r_no_1 7 (the stopped-cart explosion).
void em3bDmCkStopCart(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);

    if ((em->be_flag & 2) && !EmDeadCk(em) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            // the death body is written here AND in case 0x16 (jump2 cross-jumps the copies into the
            // later one; each copy stores the dmgWait register cse knows to be 0)
            if (w->dmgWait == 0) {
                em->hp = 0;
                EmRoutineSet(em, 1, 7, 0, 0);
                return;
            }
        }
    }
    if (em->dmg.m_Flag) {
        em->dmg.m_Flag = 0;
        em->dmg.m_Timer = 1;
        // dmWep read directly at both uses: the byte store between them forces the reload the
        // target has before the switch (a u8 local keeps one load)
        if (em->dmg.m_Wep == 0x10) {
            em->dmg.m_Timer = 0x11;
        }
        switch (em->dmg.m_Wep) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x1B:
        case 0x1D:
        case 0x21:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2B:
        case 0x2C:
        case 0x2D:
            EmDmBloodSet2(em, 0xCA, 0, 0, 0, 0);
            goto stop_ck;
        case 0x17:
        case 0x2A:
            break;
        case 0:
        case 0xE:
        case 0x10:
        case 0x14:
        case 0x15:
        case 0x18:
        default:   // the default-grouped values are real tree nodes (root 0x16, casetree.py)
            EmDmBloodSet2(em, 0xCA, 4, 0, 0, 0);
            break;
        case 0x16:
        stop_ck:   // laid out after the default arm: the blood arm reaches it through the goto
            if (w->dmgWait == 0) {
                em->hp = 0;
                EmRoutineSet(em, 1, 7, 0, 0);
            }
            break;
        }
    }
}

Em3bFunc Em3b_R0_move_tbl[5] = {
    em3b_R0_Init,
    em3b_R0_Move,
    0,
    0,
    (Em3bFunc) Em_R0_Scenario,
};

static Em3bFunc Em3b_R1_move_tbl[9] = {
    em3b_R1_Truck_Wait,
    em3b_R1_Truck_Run,
    em3b_R1_Truck_RunInto,
    em3b_R1_Cart_Wait,
    em3b_R1_Cart_Run,
    em3b_R1_Cart_Damage,
    em3b_R1_Cart_Lost,
    em3b_R1_StopCart_Damage,
    0,
};

// Per-frame update (emMove): the type's damage check, dmgWait countdown, the r_no_0 routine (0xFF
// after a failed init destroys the work), the cart's track tilt, parts matrices and the enemy /
// scenery collision.
void cEm3b::move()
{
    Em3bWork* w = EM3B_WK(this);

    if (r_no_0 != 0) {
        switch (type) {
        case 0:
        default:
            em3bDmCkTruck(this);
            break;
        case 1:
            em3bDmCkCart(this);
            break;
        case 2:
            em3bDmCkStopCart(this);
            break;
        }
    }
    w->flags &= ~0x1F;
    if (w->dmgWait) {
        w->dmgWait--;
    }
    Em3b_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em3bSlopeMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
}

// r_no_0 == 0: creation: the truck (archive 4/5) or cart (0xB/0xC) model, a 10 m light area, the
// collision (a 3.4 m box for the truck, 1.5 m for the carts; priority 3), Ashley does not ask for
// help near it, the hit cubes (truck: body + cab), no lock-on point, EM_STATUS_ACTIVE, the effects
// (archive 6 as group 0x30); then the type's wait state (truck 1/0, carts 1/3).
static void em3b_R0_Init(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    cAtariInfo* at;
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em3b() Turck:ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 1:
    case 2:
        if (em->modelInit(ARC(0xB), ARC(0xC)) == 0) {
            pLog->err(0, 0, "em3b() Cart:ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
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
        AtariInit(&em->atari, 0.0f, 0.0f, 0.0f, 900.0f, 3400.0f, 3400.0f, 1500.0f, 1, 2, 0);   // COMPILER-DIFF: #1
        break;
    case 1:
    case 2:
        AtariInit(&em->atari, 0.0f, 500.0f, 0.0f, 600.0f, 1500.0f, 1500.0f, 1500.0f, 0, 2, 0);   // COMPILER-DIFF: #1
        break;
    }
    at = &em->atari;
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    switch (em->type) {
    case 0:
    default:
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
        YarareInitCube(em, 0.0f, -500.0f, 0.0f, 500.0f, 1500.0f, 3305.0f, 1, YAT_FLAG_ON);
        YarareAddCube(em, &w->hit, 0.0f, 1000.0f, -1500.0f, 850.0f, 1500.0f, 1850.0f, 1, YAT_FLAG_ON);
        break;
    case 1:
    case 2:
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 700.0f, 1000.0f, 1300.0f, 1, YAT_FLAG_ON);
        break;
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_ACTIVE);
    EspDataLoad((u32) ARC(6), EFF_EM3B, 0);
    w->espKind = EspPullCoreKind();
    w->flags = zero;
    w->sndId2 = zero;
    w->sndId = zero;
    w->dmgWait = zero;
    switch (em->type) {
    case 0:
    default:
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 1:
    case 2:
        EmRoutineSet(em, 1, 3, zero, zero);
        break;
    }
    em3b_R0_Move(em);
}

// r_no_0 == 1: dispatches the r_no_1 state.
static void em3b_R0_Move(cEm3b* em)
{
    Em3b_R1_move_tbl[em->r_no_1](em);
}

// Park the vehicle at the origin (the room moves it with its matrix).
static inline void em3bPosReset(cEm3b* em)
{
    em->pos.x = 0.0f;
    em->pos.y = 0.0f;
    em->pos.z = 0.0f;
    em->ang.x = 0.0f;
    em->ang.y = 0.0f;
    em->ang.z = 0.0f;
}

// Truck r_no_1 == 0: parked (the engine-start motion 0xA for one frame, then the idle 7); after
// 10 frames the room's start flag (em->flag bit0) -> run (1).
static void em3b_R1_Truck_Wait(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int st = em->r_no_2;

    switch (st) {
    case 0:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 1, 0);
        MotionMove(em, 0);
        w->timer = 10;
        em->r_no_2++;
        break;
    case 1:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else if (em->flag & 1) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

// Truck r_no_1 == 1: the drive: the start motion 0xA, then the 470-frame drive motion 7 with the
// exhaust effect, running down whoever is in front; at frame 250 / 320 a dead driver or a burning
// truck (hp <= 1) veers off (-> 2 with r_no_3 = 0 / 1); random engine SEs for 450 frames; at frame
// 465 the crash into the barricade (effect 0x24, flag bit1 done, hp 0, no longer active).
static void em3b_R1_Truck_Run(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    cModel* p = em->getPartsPtr(0);
    f32 f;

    switch (em->r_no_2) {
    case 0:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 3, 1, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(7), 0, 3, 1, 0);
        w->timer = 450;
        w->seTimer = 30;
        EstSet(em, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 3: {
        int end = MotionMove(em, 0);

        if (end) {
            em->r_no_2++;
            break;
        }
        em3bRunDownCkTruck(em);
        f = em->Motion.Seq_frame;
        if (f > 249.7f && f < 250.3f) {
            // the first stop stores the (zero) MotionMove result kept in a callee-saved register; the
            // second test's label has two uses (pDriver == 0 and the `&&` false path), so cse does not
            // carry the known zero into it and its literal zero is a fresh `li` — two copies survive
            if (w->pDriver && w->pDriver->hp <= 0) {
                EmRoutineSet(em, 1, 2, end, end);
                break;
            }
            if (em->hp <= 1) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        f = em->Motion.Seq_frame;
        if (f > 319.7f && f < 320.3f) {
            if ((w->pDriver && w->pDriver->hp <= 0) || em->hp <= 1) {
                EmRoutineSet(em, 1, 2, 0, 1);
                break;
            }
        }
        f = em->Motion.Seq_frame;
        if (f > 469.7f && f < 470.3f) {
            SndCall(6, 9, &p->world, 0, 0, em);
        }
        if (w->timer) {
            w->timer--;
            if (w->seTimer) {
                w->seTimer--;
            } else {
                w->seTimer = (u8) (Rnd() % 60) + 30;
                SndCall(6, 7, &p->world, 0, 0, em);
            }
        }
        f = em->Motion.Seq_frame;
        if (f > 464.7f && f < 465.3f) {
            EstSet(em, -1, 0, 0, EFF_ROOM, 0x24, 0, ESP_CORE_KIND_NONE, em, 0);
            em->flag |= 2;
            em->hp = 0;
            em->clearStatus(EM_STATUS_ACTIVE);
            SndStop(w->sndId, 0);
        }
        break;
    }
    case 4:
        break;
    }
}

// Truck r_no_1 == 2: veers off the road (r_no_3 1: motion 9 / effect 0x26, 60 frames of running
// down; 0: motion 8 / effect 0x25, 120 frames), then the collision shrinks to the wreck; crash SEs
// at the motion's key frames set flag bit1 (done) and hp 0; inactive when the motion ends.
static void em3b_R1_Truck_RunInto(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    cModel* p = em->getPartsPtr(0);
    int st = em->r_no_2;
    f32 f;

    switch (st) {
    case 0: {
        int dir = em->r_no_3;

        if (dir) {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_ROOM, 0x26, 0, ESP_CORE_KIND_NONE, em, 0);
            w->timer = 60;
        } else {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 3, 1, 0);
            EstSet(em, -1, 0, 0, EFF_ROOM, 0x25, 0, ESP_CORE_KIND_NONE, em, 0);
            w->timer = 120;
        }
        em->r_no_2++;
    }
    case 1:
        if (MotionMove(em, 0)) {
            em->clearStatus(EM_STATUS_ACTIVE);
            em->r_no_2++;
            break;
        }
        if (w->timer) {
            w->timer--;
            em3bRunDownCkTruck(em);
            if (w->timer == 0) {
                em->atari.m_offset.x = -150.0f;
                em->atari.m_offset.y = 0.0f;
                em->atari.m_offset.z = -200.0f;
                em->atari.m_radius = 1500.0f;
            }
        }
        if (em->r_no_3) {
            f = em->Motion.Seq_frame;
            if (f > 19.7f && f < 20.3f) {
                SndCall(6, 0xA, &p->world, 0, 0, em);
            }
            f = em->Motion.Seq_frame;
            if (f > 42.7f && f < 43.3f) {
                SndCall(6, 8, &p->world, 0, 0, em);
                em->flag |= 2;
                em->hp = 0;
                SndStop(w->sndId, 0);
            }
        } else {
            f = em->Motion.Seq_frame;
            if (f > 12.7f && f < 13.3f) {
                SndCall(6, 0xB, &p->world, 0, 0, em);
            }
            f = em->Motion.Seq_frame;
            if (f > 110.7f && f < 111.3f) {
                SndCall(6, 8, &p->world, 0, 0, em);
                em->flag |= 2;
                em->hp = 0;
                SndStop(w->sndId, 0);
            }
        }
        break;
    case 2:
        break;
    }
}

// Cart r_no_1 == 3: waits at the first frame of the track motion 0xD (the room switches it to run).
static void em3b_R1_Cart_Wait(cEm3b* em)
{
    MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0, 0, 0);
    MotionMove(em, 0);
}

// Cart r_no_1 == 4: runs down the track (motion 0xD) running over whoever is in front; after 80
// frames (also re-entered after a hit) it explodes: a grenade-type blast hit check 1 m up, the
// shot noise flag, the explosion effect, dmgWait 150 -> lost (6).
static void em3b_R1_Cart_Run(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int t;

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 3, 5, 0);
        w->timer = 80;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        t = w->timer;
        if (t) {
            w->timer--;
        } else {
            Vec v;
            int zero;

            em->hp = t;
            v = em->pos;
            v.y += 1000.0f;
            zero = 0;
            PlWepHitCheck2(0, &v, &v, 0x13, 2, 6000.0f);
            StaFlagOn(pG, STA_PL_FIRE);
            EffectEspDelete(0, w->espKind, em, 0);
            EffectEspgenDelete(0, w->espKind, em);
            EffectEfmDelete(0, w->espKind, em);
            EstSet(em, -1, 0, 0, EFF_OBM34, 2, 0, ESP_CORE_KIND_NONE, em, 0);
            w->dmgWait = 150;
            SndStop(w->sndId2, 0);
            SndCall(6, 0xA, &em->pos, 0, 0, em);
            EmRoutineSet(em, 1, 6, zero, zero);
        }
        em3bRunDownCkCart(em);
        break;
    }
}

// Cart r_no_1 == 5: hit while running: the derail motion 0xE with the screech SE, marked dead and
// inactive; at its end back to run (4), which then explodes.
static void em3b_R1_Cart_Damage(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), 0, 3, 1, 0);
        w->sndId2 = SndCall(6, 9, &em->pos, 0, 0, em);
        EmSetDie(em);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
}

// Stopped cart r_no_1 == 7: the explosion: effect 3, dmgWait 150, the break motion 0xE, dead;
// next frame two grenade-type blast hit checks (1 m and 4 m up), the shot noise flag, the
// explosion SE -> lost (6).
static void em3b_R1_StopCart_Damage(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int t;

    switch (em->r_no_2) {
    case 0:
        EstSet(em, -1, 0, 0, EFF_OBM34, 3, 0, w->espKind, em, 0);
        w->dmgWait = 150;
        MotionSetCore(em, MOTION(em), ARC(0xE), 0, 3, 1, 0);
        w->timer = 1;
        EmSetDie(em);
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        t = w->timer;
        if (t) {
            w->timer--;
        } else {
            Vec v;

            v = em->pos;
            v.y += 1000.0f;
            PlWepHitCheck2(0, &v, &v, 0x13, 2, 6000.0f);
            v.y += 3000.0f;
            PlWepHitCheck2(0, &v, &v, 0x13, 2, 6000.0f);
            StaFlagOn(pG, STA_PL_FIRE);
            SndStop(w->sndId2, 0);
            SndCall(6, 0xA, &em->pos, 0, 0, em);
            EmRoutineSet(em, 1, 6, t, t);
        }
        break;
    }
}

// Cart r_no_1 == 6: gone: hidden, collision off, dead, hp 0, inactive.
static void em3b_R1_Cart_Lost(cEm3b* em)
{
    int st = em->r_no_2;

    if (st == 0) {
        em->be_flag &= ~2;
        em->atari.m_flag &= ~0x300;
        EmSetDie(em);
        em->hp = st;
        em->clearStatus(EM_STATUS_ACTIVE);
        em->r_no_2++;
    }
}

// Run over the player, the partner and the Ganados in front of the truck.
// x/z squared distance: the temp computed BEFORE d fuses dx into d's register and keeps dz*dz standalone
// (em21 VsElgigante rule).
static inline f32 em3bDistXZ(cModel* p, Vec* q)
{
    f32 t;
    f32 d;

    t = (p->world.x - q->x) * (p->world.x - q->x);
    d = (p->world.z - q->z) * (p->world.z - q->z);
    d += t;
    return d;
}

// The truck's parts 0 / 1 / 4 within 2.5 m (XZ) of the player kill him (pl_life 0, turned to
// face the truck, damage motion 8, the run-over SE); the same for the partner (ashley_life 0,
// subem3bRunDown); Ganados (ids 0x10..0x20, not the driver) within 4.5 m of parts 0 die (routine 3/4).
void em3bRunDownCkTruck(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int parts[3] = { 0, 1, 4 };
    cModel* p;
    u32 i;

    if ((s16) pG->pl_life > 0) {
        for (i = 0; i < 3; i++) {
            int zero = 0;

            p = em->getPartsPtr(parts[i]);
            if (em3bDistXZ(p, &pPL->pos) < 6250000.0f) {
                pG->pl_life = zero;
                pPL->ang.y += Muku(&pPL->pos, &p->world, pPL->ang.y, PI);
                pPL->ang.y = LIMIT_ANGLE(em->ang.y);
                PlSetDamage(PL_DM_AUTO, 0, 0);
                SndCall(1, 0x4B, &pPL->pos, 0, 0, 0);
                break;
            }
        }
    }
    if (pSUB && (s16) pG->ashley_life > 0) {
        for (i = 0; i < 3; i++) {
            int zero = 0;

            p = em->getPartsPtr(parts[i]);
            if (em3bDistXZ(p, &pSUB->pos) < 6250000.0f) {
                pG->ashley_life = zero;
                // reference store: pSUB and rot.y are re-read for LIMIT_ANGLE (a plain store is forwarded)
                (pSUB->ang.y = pSUB->ang.y + Muku(&pSUB->pos, &p->world, pSUB->ang.y, PI));
                pSUB->ang.y = LIMIT_ANGLE(pSUB->ang.y);
                SetSubDamage(em, subem3bRunDown);
                SndCall(1, 0x4B, &pSUB->pos, 0, 0, 0);
                break;
            }
        }
    }
    p = em->getPartsPtr(0);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        int zero = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e == w->pDriver) {
            continue;
        }
        if (em3bDistXZ(p, &e->pos) < 20250000.0f) {
            e->hp = zero;
            EmRoutineSet(e, 3, 4, zero, zero);
            SndCall(1, 0x4B, &em->pos, 0, 0, 0);
        }
    }
}

// Run over the player and the Ganados in front of the cart.
// The cart's parts 1 within 1.5 m (XZ) of a living, unhit player: 500 damage, turned away, damage
// motion 8; Ganados (ids 0x10..0x20) within 1.7 m die (routine 3/4).
void em3bRunDownCkCart(cEm3b* em)
{
    cModel* p;
    u32 i;

    if ((s16) pG->pl_life > 0 && !EmDeadCk(pPL)) {
        for (i = 0; i < 2; i++) {
            p = em->getPartsPtr(1);
            if (em3bDistXZ(p, &pPL->pos) < 2250000.0f) {
                LifeDownSet(pPL, 500, 0);
                pPL->ang.y = em->ang.y + PI;
                pPL->ang.y = LIMIT_ANGLE(em->ang.y);
                PlSetDamage(PL_DM_AUTO, 0, 0);
                break;
            }
        }
    }
    p = em->getPartsPtr(1);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);
        int zero = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if ((p->world.x - e->pos.x) * (p->world.x - e->pos.x) + (p->world.y - e->pos.y) * (p->world.y - e->pos.y)
            + (p->world.z - e->pos.z) * (p->world.z - e->pos.z) < 2890000.0f) {
            e->hp = zero;
            EmRoutineSet(e, 3, 4, zero, zero);   // the ff store is the zero's last use: issued first
        }
    }
}

// Partner damage routine (SetSubDamage callback): the run-over motion from the truck's archive.
static void subem3bRunDown()
{
    cSubChar* sub = SUB_CHAR();
    int st = sub->r_no_2;
    PlArc* arc = sub->pEmCatch->subArc;

    sub->subArc = arc;
    switch (st) {
    case 0:
        MotionSetCore(sub, MOTION(sub), PL_ARC_PTR(arc, 0xF), 0, 3, 1, 0);
        pG->ashley_life = st;
        sub->r_no_2++;
    case 1:
        MotionMove(sub, 0);
        break;
    }
    sub->subArc = sub->subArc2;
}

// Tilt the running cart to the slope of the track.
// Type 1 only, while the motion places it (Motion.Mot_flag bit30 clear): snaps pos.y to the floor when
// within 500 of it and pitches the matrix by the floor slope between 500 ahead and behind (clamped
// +-30 degrees).
void em3bSlopeMove(cEm3b* em)
{
    Mtx m;
    Vec a;
    Vec b;
    f32 fa;
    f32 fb;
    f32 ang;

    if (em->type != 1) {
        return;
    }
    if (em->Motion.Mot_flag & 0x40000000) {
        return;
    }
    fa = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    // two statements into the function-scope fb: the difference and the fabs share fb's register (f1)
    fb = fa - em->pos.y;
    fb = fabsf(fb);
    if (fb > 500.0f) {
        return;
    }
    em->pos.y = fa;
    TransMatrix(em->mat, &em->pos);
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = 1000.0f;
    b.z = -500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    fa = SatMgr.getFloor(&a, 0, 600.0f, 100000.0f, 0);
    fb = SatMgr.getFloor(&b, 0, 600.0f, 100000.0f, 0);
    if (fa == -100000.0f) {
        fa = em->pos.y;
    }
    if (fb == -100000.0f) {
        fb = em->pos.y;
    }
    fa -= fb;
    ang = -atan2f(fa, VEC_DISTXZ(&a, &b));
    if (ang > PI / 6.0f) {
        ang = PI / 6.0f;
    }
    if (ang < -PI / 6.0f) {
        ang = -PI / 6.0f;
    }
    PSMTXRotRad(m, 'x', ang);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
}

// Room query: 1 while the vehicle burns / has just been hit (dmgWait running).
int cEm3b::ckFire()
{
    if (EM3B_WK(this)->dmgWait != 0) {
        return 1;
    }
    return 0;
}
