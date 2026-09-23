// em23 module (D:/Bio4/Prog/em23.cpp): the crow. Sits on the ground (or on the corpse the room
// hands it) until the player comes near, takes off, circles at flyHeight above the player, turns,
// lands again on the corpse and pecks at it; a hit knocks it out of the air (Dm_Air) or kills it.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em23.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include <dolphin/os.h>
#include "em_mod.h"



typedef void (*Em23Func)(cEm23*);

static void em23_R0_Init(cEm23* em);
static void em23_R0_Move(cEm23* em);
static void em23_R1_R20ALanding(cEm23* em);
static void em23_R1_Wait(cEm23* em);
static void em23_R1_Takeoff(cEm23* em);
static void em23_R1_TakeoffDash(cEm23* em);
static void em23_R1_Turn(cEm23* em);
static void em23_R1_Landing(cEm23* em);
static void em23_R1_ToCorpse(cEm23* em);
static void em23_R1_Eat(cEm23* em);
static void em23_R0_Damage(cEm23* em);
static void em23_R1_Dm_Air(cEm23* em);
static void em23_R0_Die(cEm23* em);
static void em23_R1_Die_Normal(cEm23* em);



// Module entry (SN loader): registers Em23Init as the DOL's enemy constructor (EmInitFunc).
extern "C" void _prolog()
{
    OSReport("em23 prolog Ok\n");
    EmInitFunc = Em23Init;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the cEm23 class in the manager's work.
void Em23Init(cEm* em)
{
    new (em) cEm23();
}

// Per-frame damage check (cEm23::move): an explosion / fire volume kills the crow (flag bit2, Dm_Air);
// a weapon hit does 900..1100 for the hand weapons / knife / TMP and a far shotgun hit, 9999 (a kill)
// for everything else and a near shotgun hit, the mine (0xE) nothing; blood, caw, then Dm_Air (R2 0).
void em23DmCk(cEm23* em)
{
    Em23Work* w = EM23_WK(em);
    int near;
    int dmg;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            em->hp = 0;
            EmSetDie(em);
            EmReserveDropItem(em);
            w->flags |= 4;
            EmRoutineSet(em, 2, 0, 0, 0);
            return;
        }
    }
    if (em->dmg.m_Flag) {
        em->dmg.m_Flag = 0;
        near = 0;
        if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
            near = 1;
        }
        switch (em->dmg.m_Wep) {
        case 0:
        case 2:
        case 0xB:
        case 0x10:
            dmg = Rnd() % 200 + 900;
            break;
        case 7:
        case 8:
        case 0x21:
            if (near) {
                dmg = 9999;
            } else {
                dmg = Rnd() % 200 + 900;
            }
            break;
        case 0x1B:
            dmg = 9999;
            break;
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xC:
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2C:
        case 0x2D:
        default:
            dmg = 9999;
            break;
        case 0xE:
            dmg = 0;
            break;
        }
        LifeDownSet(em, dmg, 0);
        EmDmBloodSet2(em, 0x1B, 0, 0, 0, 0);
        EmDmBloodSet2(em, 0x1B, 1, 0, 0, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        EmRoutineSet(em, 2, 0, 0, 0);
    }
}

Em23Func Em23_R0_move_tbl[4] = {
    em23_R0_Init,
    em23_R0_Move,
    em23_R0_Damage,
    em23_R0_Die,
};

static Em23Func Em23_R1_move_tbl[8] = {
    em23_R1_R20ALanding,
    em23_R1_Wait,
    em23_R1_Takeoff,
    em23_R1_TakeoffDash,
    em23_R1_Turn,
    em23_R1_Landing,
    em23_R1_ToCorpse,
    em23_R1_Eat,
};

static Em23Func Em23_R2_move_tbl[1] = {
    em23_R1_Dm_Air,
};

static Em23Func Em23_R3_move_tbl[1] = {
    em23_R1_Die_Normal,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em23_flip_tbl[80] = {
    0, 1, 2, 3, 4, 5, 0xA, 0xB, 0xC, 0xD, 6, 7, 8, 9, 0xE, 0xF, 0x11, 0x10, 0x15, 0x16, 0x17, 0x12, 0x13, 0x14,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
    0x4E, 0x4F,
};

// Per-frame update: damage check, the R0 table (Init / Move / Damage / Die), then the collision and
// airborne scenario check (checkAir), and flag bit1 = the position changed this frame (moveTimer).
void cEm23::move()
{
    Em23Work* w = EM23_WK(this);
    Vec old;

    em23DmCk(this);
    w->flags &= ~1;
    if (w->moveTimer) {
        w->moveTimer--;
    }
    if (w->stateTimer) {
        w->stateTimer--;
    }
    Em23_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    old = pos;
    EmAtCheck(this);
    atari.move();
    SatMgr.checkAir(this, 0x100000);
    PartsWorldPosCalc(this);
    partsFixAdjust();
    PartsWorldPosCalc(this);
    if (old.x != pos.x || old.z != pos.z) {
        w->moveTimer = 10;
        w->flags |= 2;
    } else {
        w->flags &= ~2;
    }
    if ((Motion.Seq_old.Free & 0x80) && pos.y - pPL->pos.y < 3000.0f) {
        SndCall(8, 0xB, &pos, id, 0, this);
    }
}

// R0 == 0: creation. No lock-on / no Ashley help, builds the model (ARC 5/6 + the folded wing), hp,
// collision, hit box, the room's ctrl11 / ctrl12, flyHeight, and the start routine by cEm::set: 0 Wait
// (1), 1 the room 20A corpse-landing crow (R20ALanding 0).
static void em23_R0_Init(cEm23* em)
{
    Em23Work* w = EM23_WK(em);
    f32 scale;
    int zero;

    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->atari.init(0.0f, -100.0f, 0.0f, 350.0f, 150.0f, 150.0f, 200.0f, 1, 0x2000, 10);
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    EspDataLoad((u32) ARC(4), EFF_EM23, 0);
    em->Motion.flip = em23_flip_tbl;
    YarareInit(em, 0.0f, -100.0f, 0.0f, 250.0f, 200.0f, 1, YAT_FLAG_ON);
    if (em->modelInit(ARC(5), ARC(8)) == 0) {
        pLog->err(0, 0, "em23() ModelInit failed.");
        em->r_no_0 = 0xFF;
        return;
    }
    em->be_flag &= ~0x10;
    scale = fRand1_1() * 0.1f + 1.0f;
    em->scale.x = scale;
    em->scale.y = scale;
    em->scale.z = scale;
    w->pWingInfo = 0;
    w->wing = 0xFF;
    em23SetWing(em, 1);
    w->flags = zero;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->moveTimer = zero;
    w->x20 = 0.0f;
    w->pCorpse = 0;
    w->x3C = 100000000.0f;
    w->x24 = 0.0f;
    w->stateTimer = Rnd() % 300 + 150;
    w->flyHeight = (f32) (em->emset_no % 5) * 1000.0f + 8000.0f;
    if (pG->room_id == 0x30A) {
        w->flyHeight = (f32) (em->emset_no % 5) * 2000.0f + 20000.0f;
    }
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    switch (em->set) {
    case 0:
    default:
        EmRoutineSet(em, 1, 1, 0, 0);
        break;
    case 1:
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    }
    MotionSetCore(em, MOTION(em), ARC(9), 0, 0, 5, 0);
    MotionMove(em, 0);
    em23_R0_Move(em);
}

// R0 == 1: runs the R1 routine (Em23_R1_move_tbl).
static void em23_R0_Move(cEm23* em)
{
    Em23_R1_move_tbl[em->r_no_1](em);
}

// Idle motion: one of the four ground animations, two of them with a blend motion.
static inline void em23WaitMotion(cEm23* em)
{
    void* m0;
    void* m1;

    switch (Rnd() & 3) {
    case 0:
    default:
        m0 = ARC(9);
        m1 = 0;
        break;
    case 1:
        m0 = ARC(0xA);
        m1 = 0;
        break;
    case 2:
        m0 = ARC(0xB);
        m1 = ARC(0x1C);
        break;
    case 3:
        m0 = ARC(0xC);
        m1 = ARC(0x1D);
        break;
    }
    MotionSetCore(em, MOTION(em), m0, m1, 5, 5, 0);
}

// Takeoff motion: the blend motion depends on the enemy list slot.
static inline void em23TakeoffMotion(cEm23* em)
{
    void* m1;

    switch (em->emset_no % 5) {
    case 0:
    default:
        m1 = ARC(0x24);
        break;
    case 1:
        m1 = ARC(0x25);
        break;
    case 2:
        m1 = ARC(0x26);
        break;
    case 3:
        m1 = ARC(0x27);
        break;
    case 4:
        m1 = ARC(0x28);
        break;
    }
    MotionSetCore(em, MOTION(em), ARC(0xE), m1, 5, 1, 0);
}

// The player is close enough to react to (farther with the noise flag set).
#define EM23_PL_NEAR(em, near, far) \
    ((em)->l_pl < (near) || (StaFlagChk(pG, STA_PL_FIRE) && (em)->l_pl < (far)))

// Flight speed update shared by the air routines: accelerate forward, climb or dive towards the
// height flyHeight above the player, then move.
// (the clamp constants are pseudos created after the sums: the em3a hover-clamp idiom, otherwise
// the pool load waits behind the store and the compare reuses the sum's register)
static inline void em23FlyMove(cEm23* em, Em23Work* w)
{
    {
        f32 z = w->spd.z + 10.0f;
        f32 max = 150.0f;

        w->spd.z = z;
        if (z > max) {
            w->spd.z = max;
        }
    }
    if (em->pos.y < pPL->pos.y + w->flyHeight) {
        f32 y = w->spd.y + 10.0f;
        f32 max = 50.0f;

        w->spd.y = y;
        if (y > max) {
            w->spd.y = max;
        }
    } else {
        f32 y = w->spd.y - 10.0f;
        f32 min = 0.0f;

        w->spd.y = y;
        if (y < min) {
            w->spd.y = min;
        }
    }
    em23AddSpeedAir(em, em->ang.y);
}

// R1 == 0: room 20A crow sitting on the corpse: idle (hp 1), takes off (ARC 0xE) when the player comes
// close with the wing flap SE, climbs, then Turn (4) once airborne.
// Both motion switches are written out with ONE pair of routine-scope pointers (not the
// em23WaitMotion/em23TakeoffMotion inlines): the shared `m1` pseudo is what makes the takeoff join's
// subArc copy take r10 instead of r11 (global-alloc `regs_someone_prefers`).
static void em23_R1_R20ALanding(cEm23* em)
{
    Em23Work* w = EM23_WK(em);
    void* m0;
    void* m1;

    switch (em->r_no_2) {
    case 0:
        switch (Rnd() & 3) {
        case 0:
        default:
            m0 = ARC(9);
            m1 = 0;
            break;
        case 1:
            m0 = ARC(0xA);
            m1 = 0;
            break;
        case 2:
            m0 = ARC(0xB);
            m1 = ARC(0x1C);
            break;
        case 3:
            m0 = ARC(0xC);
            m1 = ARC(0x1D);
            break;
        }
        MotionSetCore(em, MOTION(em), m0, m1, 5, 5, 0);
        em->hp = 1;
        em23SetWing(em, 1);
        em->r_no_2++;
    case 1:
        em->partsFixMemory(0x14);
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        if (EM23_PL_NEAR(em, 25000000.0f, 100000000.0f)) {
            em->r_no_2++;
        }
        break;
    case 2:
        switch (em->emset_no % 5) {
        case 0:
        default:
            m1 = ARC(0x24);
            break;
        case 1:
            m1 = ARC(0x25);
            break;
        case 2:
            m1 = ARC(0x26);
            break;
        case 3:
            m1 = ARC(0x27);
            break;
        case 4:
            m1 = ARC(0x28);
            break;
        }
        MotionSetCore(em, MOTION(em), ARC(0xE), m1, 5, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->stateTimer = Rnd() % 300 + 150;
        w->targetAng = em->ang.y + fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = 7;
        em->r_no_2++;
    case 3:
        if (em->Motion.Seq_old.Free & 1) {
            em23SetWing(em, 0);
            Ctrl11SetSe(w->pCtrl11, em, 10, 10, 0xB);
        }
        if (MotionMove(em, 0)) {
            em->r_no_2++;
            w->spd.x = 0.0f;
            w->spd.y = 20.0f;
            w->spd.z = 30.0f;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x23), 5, 5, 0);
        em->atari.off();
        w->timer = 20;
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->atari.on();
                EmRoutineSet(em, 1, 4, 0, 0);
            }
        }
        break;
    }
    if (em->r_no_2 > 3) {
        em23FlyMove(em, w);
    }
}

// R1 == 1 Wait: pecks around on the ground (em23WaitMotion) until the player comes near (EM23_PL_NEAR),
// then Takeoff (2).
static void em23_R1_Wait(cEm23* em)
{
    switch (em->r_no_2) {
    case 0:
        em23WaitMotion(em);
        em23SetWing(em, 1);
        em->r_no_2++;
    case 1:
        em->partsFixMemory(0x14);
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        if (EM23_PL_NEAR(em, 25000000.0f, 100000000.0f)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

// R1 == 2 Takeoff: the takeoff flap (em23TakeoffMotion) turning to the heading targetAng (away from
// the player), wing spread (em23SetWing), then Turn (4) in the air.
static void em23_R1_Takeoff(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->r_no_2) {
    case 0:
        em23TakeoffMotion(em);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->stateTimer = Rnd() % 300 + 150;
        w->targetAng = em->ang.y + fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = 7;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->targetAng, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (em->Motion.Seq_old.Free & 1) {
            em23SetWing(em, 0);
            Ctrl11SetSe(w->pCtrl11, em, 10, 10, 0xB);
        }
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
            w->spd.x = 0.0f;
            w->spd.y = 20.0f;
            w->spd.z = 30.0f;
        }
        break;
    }
}

// R1 == 3 TakeoffDash: the hurried takeoff (ARC 0xF) used when startled, otherwise like Takeoff.
static void em23_R1_TakeoffDash(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 5, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->stateTimer = Rnd() % 300 + 150;
        w->timer = 7;
        w->targetAng = em->ang.y + fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        em->r_no_2++;
    case 1:
        em->ang.y += Muku2(em->ang.y, w->targetAng, PI / 16.0f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (em->Motion.Seq_old.Free & 1) {
            em23SetWing(em, 0);
            Ctrl11SetSe(w->pCtrl11, em, 10, 10, 0xB);
        }
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
            w->spd.x = 0.0f;
            w->spd.y = 20.0f;
            w->spd.z = 30.0f;
        }
        break;
    }
}

// R1 == 4 Turn: flies circles at flyHeight above the player (em23FlyMove), banking with one of the
// six glide blends (r_no_3), flipping the turn direction every turnTimer frames; after stateTimer it
// heads for the corpse (Landing 5) when the room has one.
static void em23_R1_Turn(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->r_no_2) {
    case 0:
        switch (em->r_no_3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x23), 5, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x22), 5, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x21), 5, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x20), 5, 5, 0);
            break;
        case 4:
            MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x1F), 5, 5, 0);
            break;
        case 5:
            MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x1E), 5, 5, 0);
            break;
        case 6:
            if (Rnd() & 1) {
                MotionSetCore(em, MOTION(em), ARC(0x17), 0, 5, 5, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x1E), 5, 5, 0);
            }
            break;
        }
        w->timer = 5;
        w->turnTimer = Rnd() % 120 + 120;
        w->turnDir = Rnd() & 1;
        em->r_no_2++;
    case 1: {
        f32 step;
        f32 y;

        if (w->turnTimer) {
            w->turnTimer--;
            if (w->turnTimer == 0) {
                w->turnDir ^= 1;
                w->turnTimer = Rnd() % 120 + 120;
            }
        }
        if (w->turnDir) {
            step = PI / 256.0f;
        } else {
            step = 0.017453292f;
        }
        if (w->moveTimer) {
            step = PI / 32.0f;
        }
        if (pG->room_id == 0x30A && em->pos.y < pPL->pos.y + w->flyHeight) {
            if (em->emset_no & 1) {
                step = PI / 128.0f;
            } else {
                step = -PI / 128.0f;
            }
            y = em->ang.y + step;
        } else {
            y = em->ang.y + Muku(&em->pos, &pPL->pos, em->ang.y, step);
        }
        em->ang.y = y;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->r_no_3++;
                if (em->r_no_3 > 5) {
                    em->r_no_3 = 5;
                }
                em->r_no_2 = 0;
            }
        }
        break;
    }
    }
    em23FlyMove(em, w);
}

// R1 == 5 Landing: glides towards the corpse (pCorpse), descends onto it and lands (ARC 0x10), then
// ToCorpse (6) / Eat (7), or takes off again when disturbed; without a floor it goes back to Turn.
static void em23_R1_Landing(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), ARC(0x23), 5, 5, 0);
        em->r_no_2++;
    case 1:
        if (w->pCorpse) {
            em->ang.y += Muku(&em->pos, &w->pCorpse->pos, em->ang.y, PI / 64.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        MotionMove(em, 0);
        if (w->pCorpse
            && (em->pos.x - w->pCorpse->pos.x) * (em->pos.x - w->pCorpse->pos.x)
                       + (em->pos.z - w->pCorpse->pos.z) * (em->pos.z - w->pCorpse->pos.z)
                   < 9000000.0f) {
            em->r_no_2++;
        } else {
            w->spd.z += 10.0f;
            if (w->spd.z > 150.0f) {
                w->spd.z = 150.0f;
            }
            if (em->pos.y < pPL->pos.y + 7000.0f) {
                w->spd.y += 10.0f;
                if (w->spd.y > 50.0f) {
                    w->spd.y = 50.0f;
                }
            } else {
                w->spd.y -= 10.0f;
                if (w->spd.y < 0.0f) {
                    w->spd.y = 0.0f;
                }
            }
            em23AddSpeedAir(em, em->ang.y);
        }
        break;
    case 2:
        em->r_no_2++;
    case 3:
        if (w->pCorpse) {
            em->ang.y += Muku(&em->pos, &w->pCorpse->pos, em->ang.y, PI / 64.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        MotionMove(em, 0);
        w->spd.z -= 5.0f;
        if (w->spd.z < 100.0f) {
            w->spd.z = 100.0f;
        }
        w->spd.y -= 5.0f;
        if (w->spd.y < -50.0f) {
            w->spd.y = -50.0f;
        }
        if (em23AddSpeedAir(em, em->ang.y)) {
            if (fabsf(em->pos.y - w->pCorpse->pos.y) > 200.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x10), ARC(0x29), 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            if (w->pCorpse) {
                if (fabsf(em->pos.y - w->pCorpse->pos.y) < 200.0f) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 2, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
    if (w->pCorpse) {
        if ((pG->Frame_cnt & 0xF) == (em->emset_no & 0xF)) {
            Vec v;
            f32 fl;

            v = w->pCorpse->pos;
            fl = SatMgr.getFloor(&em->pos, 0, 0.0f, 5000.0f, 0);
            if (fl == -100000.0f) {
                fl = v.y;
            }
            v.y = fl + 500.0f;
            if (SatMgr.hitCheck(&em->pos, &v, 0, 0, 0, 0)) {
                goto takeoff;
            }
        }
        if (w->pCorpse && w->pCorpse->l_pl < 16000000.0f) {
        takeoff:
            EmRoutineSet(em, 1, 4, 0, 0);
            return;
        }
    }
    em23SetWing(em, 0);
}

// R1 == 6 ToCorpse: hops (ARC 0x1A / 0x1B) towards the corpse turning to it, then Eat (7) when there
// or Takeoff (2 / 3) when the player comes near; the hop-landing motions 0x12 / 0x13.
static void em23_R1_ToCorpse(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    if (w->pCorpse && em->r_no_2 == 0) {
        f32 ang = Muku(&em->pos, &w->pCorpse->pos, em->ang.y, PI);
        f32 a = fabsf(ang);

        if (a > 2.0943952f) {
            em->r_no_2 = 2;
        }
        if (a > 1.0471976f) {
            if (ang < 0.0f) {
                em->r_no_2 = 4;
            } else {
                em->r_no_2 = 6;
            }
        }
    }
    switch (em->r_no_2) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0x1A), ARC(0x2B), 5, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1B), ARC(0x2C), 5, 5, 0);
        }
        em->r_no_2++;
    case 1:
        if (w->pCorpse) {
            em->ang.y += Muku(&em->pos, &w->pCorpse->pos, em->ang.y, PI / 64.0f);
            em->ang.y = LIMIT_ANGLE(em->ang.y);
        }
        if (MotionMove(em, 0) && w->pCorpse
            && (em->pos.x - w->pCorpse->pos.x) * (em->pos.x - w->pCorpse->pos.x)
                       + (em->pos.z - w->pCorpse->pos.z) * (em->pos.z - w->pCorpse->pos.z)
                   < 360000.0f) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return;
        }
        if (EM23_PL_NEAR(em, 6250000.0f, 100000000.0f)) {
            if (Rnd() % 3) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
            return;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 5, 1, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 5, 1, 0);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 5, 0x41, 0);
        em->r_no_2++;
    case 7:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
    em23SetWing(em, 1);
}

// R1 == 7 Eat: pecks at the corpse (ARC 0x18, repeated at random), looks up, and takes off (2) when
// the player comes near.
static void em23_R1_Eat(cEm23* em)
{
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 5, 5, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0) && (Rnd() & 1)) {
            em->r_no_2++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 5, 5, 0);
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (Rnd() & 1) {
                em->r_no_2 = 0;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 4:
        em23WaitMotion(em);
        em->r_no_2++;
    case 5:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
    if (EM23_PL_NEAR(em, 6250000.0f, 100000000.0f)) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        em23SetWing(em, 1);
    }
}

// R0 == 2: damage, runs Em23_R2_move_tbl (Dm_Air).
static void em23_R0_Damage(cEm23* em)
{
    Em23_R2_move_tbl[em->r_no_1](em);
}

// R0 2 / R1 == 0 Dm_Air: shot in the air: tumbles down spinning (ARC 0x16, dmRotSpd / dmAng, no more
// damage) until it hits the floor (em23AddSpeedAir); a dead crow goes to Die_Normal, a living one
// flaps up again (0x15) and takes off (2).
static void em23_R1_Dm_Air(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->dmg.m_Timer = 0x80;
        MotionSetCore(em, MOTION(em), ARC(0x16), ARC(0x2A), 5, 5, 0);
        if (Rnd() & 1) {
            w->dmRotSpd = PI / 20.0f;
        } else {
            w->dmRotSpd = -PI / 20.0f;
        }
        w->dmAng = em->ang.y;
        w->targetAng = em->pos.y;
        em->r_no_2++;
    case 1:
        em->ang.y += w->dmRotSpd;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        w->spd.y -= 10.0f;
        w->spd.z *= 0.95f;
        if (em23AddSpeedAir(em, w->dmAng) && em->pos.y >= w->targetAng - 10.0f) {
            SndCall(8, 9, &em->pos, em->id, 0, em);
            if (em->hp > 0) {
                em->r_no_2++;
            } else {
                EmRoutineSet(em, 3, 0, 0, 0);
            }
        } else {
            w->targetAng = em->pos.y;
            MotionMove(em, 0);
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 5, 5, 0);
        w->spd.y = 0.0f;
        em->dmg.m_Timer = 0;
        em->r_no_2++;
    case 3:
        w->dmRotSpd *= 0.9f;
        em->ang.y += w->dmRotSpd;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        w->spd.z *= 0.85f;
        em23AddSpeedAir(em, w->dmAng);
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em23SetWing(em, 0);
}

// R0 == 3: death, runs Em23_R3_move_tbl (Die_Normal).
static void em23_R0_Die(cEm23* em)
{
    Em23_R3_move_tbl[em->r_no_1](em);
}

// R0 3 / R1 == 0 Die_Normal: the death motion ARC 0x14, drops the item (ITEMSET), then the body fades
// out (flag bit2) and the enemy is left dead.
static void em23_R1_Die_Normal(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 5, 1, 0);
        em->atari.off();
        SndCall(8, 8, &em->pos, em->id, 0, em);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        EmSetDie(em);
        EmReserveDropItem(em);
        em->r_no_2++;
    case 1:
        if (w->flags & 4) {
            cModelInfo* info;

            for (info = em->pModelInfo; info; info = info->pList) {
                if (info->color[0] > 0x20) {
                    info->color[0] -= 0x20;
                }
                info->color[2] = info->color[1] = info->color[0];
            }
        }
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    }
    em23SetWing(em, 0);
}

// Moves the crow by its local speed spd rotated to heading `ang`; when descending stops it on the
// floor found between the old height + 500 and the new position (halving the fall speed). 1 = landed.
int em23AddSpeedAir(cEm23* em, f32 ang)
{
    Em23Work* w = EM23_WK(em);
    Mtx m;
    Vec v;
    Vec a;
    Vec b;
    Vec hit;

    PSMTXRotRad(m, 'y', ang);
    PSMTXMultVecSR(m, &w->spd, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    if (v.y >= 0.0f) {
        return 0;
    }
    a = em->pos;
    b = em->pos;
    a.y = em->pos_old.y + 500.0f;
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0) && hit.y > em->pos.y) {
        em->pos.y = hit.y;
        w->spd.y *= 0.5f;
        return 1;
    }
    if (em->pos.y < -100000.0f) {
        em->pos.y = -100000.0f;
        return 1;
    }
    return 0;
}

// Swaps the wing model (pWingInfo): `on` 1 the spread wings (ARC 7), 0 the folded ones (ARC 6).
void em23SetWing(cEm23* em, int on)
{
    Em23Work* w = EM23_WK(em);
    cModelInfo* info;
    void* bin;
    void* tpl;

    if (w->wing == on) {
        return;
    }
    w->wing = on;
    if (w->pWingInfo) {
        em->deleteModelData(w->pWingInfo->model_addr);
        w->pWingInfo = 0;
    }
    // bin and tpl both assigned in each arm (two-set pseudos, tails cross-jumped into the join with
    // the tpl offset load first); a shared `ARC(8)` after the join gets a gcse PRE copy of subArc
    if (on) {
        bin = ARC(7);
        tpl = ARC(8);
    } else {
        bin = ARC(6);
        tpl = ARC(8);
    }
    info = ModInfoMgr.create(bin, tpl);
    w->pWingInfo = info;
    if (info) {
        em->addModel(info);
    }
}
