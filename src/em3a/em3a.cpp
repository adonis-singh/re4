// em3a module (D:/Bio4/Prog/em3a.cpp): the helicopter. Types 0/1 patrol the room's EMI route
// points (em3a_R1_Patrol), find the player (em3aFindPLCk), chase and shoot him with the gun
// (em3aGunHitCk) or a missile (em3aRocketFire); type 2 is the hovering variant that hides,
// appears and circles the player (the B_ routines) until its nearCnt runs out.
//
// Em3aInit is the module's EmInitFunc. Routines: r_no_0 0 init, 1 move with r_no_1 for types
// 0/1 (the gunship; type 1 also carries a missile on parts 9): 0 patrol the EMI 0x13 points of
// its Character, 1 attack (gun bursts / the rocket), 2 chase the player along the route network,
// 3 fixed fly-in from em->set (a direction), 4 attack without height control, 5 die (falls, kills
// with a blast); for type 2 (the hiding boss variant): 6 hidden wait (r_no_3 1 = wait for the
// room's flag bit0), 7 hide, 8 appear, 9 wait, 0xA move around the player, 0xB / 0xC die
// variants, 0xD the self-destruct blast. Em3aWork (em3a.h): flags bit0 player found, bit1 gun
// attack running, bit2 damage smoke set, bit3 hidden; atkWait holds the attacks off (setAtkWait
// from the rooms, 90 frames while the player is down); em->id 0x39 enemies (em39) in front block
// the fire (em3aBossCk).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3a.h"
#include "em10.h"
#include "emhit.h"
#include "embarrel.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "quake.h"
#include "pad.h"
#include "pl_wep.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "em.h"
#include <dolphin/os.h>
#include "em_mod.h"



typedef void (*Em3aFunc)(cEm3a*);

static void em3a_R0_Init(cEm3a* em);
static void em3a_R0_Move(cEm3a* em);
static void em3a_R1_Patrol(cEm3a* em);
static void em3a_R1_Atk(cEm3a* em);
static void em3a_R1_Chase(cEm3a* em);
static void em3a_R1_Fix(cEm3a* em);
static void em3a_R1_FixAtk(cEm3a* em);
static void em3a_R1_Die(cEm3a* em);
static void em3a_R1_B_HideWait(cEm3a* em);
static void em3a_R1_B_Hide(cEm3a* em);
static void em3a_R1_B_Appear(cEm3a* em);
static void em3a_R1_B_Wait(cEm3a* em);
static void em3a_R1_B_Move(cEm3a* em);
static void em3a_R1_B_Die(cEm3a* em);
static void em3a_R1_B_AppearDie(cEm3a* em);
static void em3a_R1_B_Bomb(cEm3a* em);




static inline void EmiSet(EmiEntry*& p, EmiEntry* v) { p = v; }

// Hover: keep the height between fl + 1800 and fl + 2000, apply and damp the speed, vibrate.
static inline void em3aHoverMove(cEm3a* em, Em3aWork* w, f32 fl)
{
    if (em->pos.y < fl + 1800.0f) {
        f32 y = w->spd.y + 10.0f;
        f32 max = 20.0f;

        w->spd.y = y;
        if (y > max) {
            w->spd.y = max;
        }
    }
    if (em->pos.y > fl + 2000.0f) {
        f32 y = w->spd.y - 10.0f;
        f32 min = -20.0f;

        w->spd.y = y;
        if (y < min) {
            w->spd.y = min;
        }
    }
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    PSVECScale(&w->spd, &w->spd, 0.9f);
    em3aVibMove(em);
}

// The same without the height control (R1_FixAtk).
static inline void em3aFixMove(cEm3a* em, Em3aWork* w)
{
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    PSVECScale(&w->spd, &w->spd, 0.9f);
    em3aVibMove(em);
}

// Turn towards the player by at most `lim`.
static inline void em3aTurnToPL(cEm3a* em, f32 lim)
{
    em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, lim);
    em->ang.y = LIMIT_ANGLE(em->ang.y);
}

// Attack found: drop the search effects, start the alert effect and sound.
static inline void em3aFoundSet(cEm3a* em, Em3aWork* w, int est, int parts)
{
    EffectEspDelete(1, w->espKind, em, 0);
    EffectEspgenDelete(1, w->espKind, em);
    EffectEfmDelete(1, w->espKind, em);
    EstSet(em, -1, 0, 0, EFF_EM3A, est, 1, w->espKind, em, 0);
    SndCall(8, 0xA, &em->getPartsPtr(parts)->world, em->id, 0, em);
}

// REL entry: registers the enemy constructor.
extern "C" void _prolog()
{
    OSReport("em3a prolog Ok\n");
    EmInitFunc = Em3aInit;
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// cUnit::setNoSuspend override: the helicopter and its hung missile keep moving through pauses
// (be_flag bit11).
void cEm3a::setNoSuspend(int on)
{
    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
}

// EmInitFunc: placement-constructs the helicopter in the cEm work.
void Em3aInit(cEm* em)
{
    new (em) cEm3a();
}

// Damage of the frame (cEm::dmHit): type 2 while hidden dies at once from anything but a distant
// grenade / rocket blast (-> 0xC), a hit on its weak spot (hit part on parts 0xB) kills it (->
// 0xB); otherwise hp -= 5x the weapon's damage value, a spark / blood effect, the smoke trail
// once (flags bit2, types 0/1); hp <= 0 -> die (type 2: 0xD blast, else 5).
void em3aDmCk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    YARARE_INFO* hit;
    int one;

    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    one = 1;
    em->dmg.m_Timer = one;
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    hit = em->dmg.m_pDamageYarare;
    if (em->type == 2) {
        EmDmBloodSet2(em, 2, 1, 0, 0, 0);
        if (w->flags & 8) {
            switch (em->dmg.m_Wep) {
            case 0x13:
            case 0x29:
            case 0x2D:
                if (hit->len > 1000000.0f) {
                    return;
                }
                break;
            }
            em->hp = 0;
            EmRoutineSet(em, 1, 0xC, 0, 0);
            return;
        }
        if (hit->parts_no == 0xB) {
            em->hp = 0;
            EmRoutineSet(em, one, 0xB, 0, 0);
            return;
        }
    }
    LifeDownSet(em, em3aSetDmVal(em), 0);
    EmDmBloodSet2(em, 2, 1, 0, 0, 0);
    if (em->type != 2) {
        if (!(w->flags & 4)) {
            w->flags |= 4;
            EstSet(em, -1, 0, 0, EFF_EM3A, 2, 1, w->espKind, em, 0);
        }
    }
    if (em->hp <= 0) {
        if (em->type == 2) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        } else {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
    }
}

Em3aFunc Em3a_R0_move_tbl[4] = {
    em3a_R0_Init,
    em3a_R0_Move,
    NULL,
    NULL,
};

static Em3aFunc Em3a_R1_move_tbl[14] = {
    em3a_R1_Patrol,
    em3a_R1_Atk,
    em3a_R1_Chase,
    em3a_R1_Fix,
    em3a_R1_FixAtk,
    em3a_R1_Die,
    em3a_R1_B_HideWait,
    em3a_R1_B_Hide,
    em3a_R1_B_Appear,
    em3a_R1_B_Wait,
    em3a_R1_B_Move,
    em3a_R1_B_Die,
    em3a_R1_B_AppearDie,
    em3a_R1_B_Bomb,
};

// Parts index remap of the type 2 model's flipped motions (cModel::motFlip).
static u16 em3a_flip_tbl[120] = {
    0x00, 0x03, 0x04, 0x01, 0x02, 0x07, 0x08, 0x05, 0x06, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Gun hit damage handed to EmAtkSetDamagePL (em3aGunHitCk): range, type, damage, ...
static EmAtkInfo em3a_atk_info = { 100.0f, PL_DM_AUTO, 600, 0, 0xA, 0 };

// Per-frame update (emMove): damage, the attack hold-off countdown (90 frames while the player is
// down), the r_no_0 routine (0xFF after a failed init destroys the work), the rotors, parts
// matrices, the gun aim, and while alive the enemy collision plus the flight collision (types
// 0/1: kept 500 above the floor, SatMgr.checkAir) or the ground check (type 2); the engine SE.
void cEm3a::move()
{
    Em3aWork* w = EM3A_WK(this);

    if (r_no_0) {
        em3aDmCk(this);
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    if (EmDeadCk(pPL)) {
        w->atkWait = 90;
    }
    Em3a_R0_move_tbl[r_no_0](this);
    if (r_no_0 == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em3aFanMove(this);
    partsWorldCalc();
    em3aGunMove(this);
    if (hp > 0) {
        EmAtCheck(this);
        atari.move();
        switch (type) {
        case 0:
        case 1:
        default: {
            f32 fl = SatMgr.getFloor(&pos, 0, 600.0f, 100000.0f, 0) + 500.0f;

            if (pos.y < fl) {
                pos.y = fl;
            }
            SatMgr.checkAir(this, 0x383810);
            break;
        }
        case 2:
            SatMgr.check(this, 0);
            break;
        }
    }
    em3aEngineSe(this);
}

// r_no_0 == 0: creation: the model (types 0/1 archive 5/6, type 2 0xB/0xC with its flip table),
// a 10 m light area, the collision cylinder, hit boxes (gunship: body + rotor hub / tail / gun /
// nose cubes; type 2: body + the weak spot on parts 0xB), type 1's missile on parts 9 (the pod
// model hidden), lock-on parts 2, effects (archive 4 as group 2), the hover wobble, the patrol
// route, the rotor tilt / idle effect, EM_STATUS_ACTIVE; the start state from em->set (gunship:
// 0 patrol, 1..5 fixed fly-in directions; type 2: 5 hidden, 6 waiting, 7 hidden until the room's flag).
static void em3a_R0_Init(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->type) {
    case 0:
    case 1:
    default:
        if (em->modelInit(ARC(5), ARC(6)) == 0) {
            pLog->err(0, 0, "em3a() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    case 2:
        if (em->modelInit(ARC(0xB), ARC(0xC)) == 0) {
            pLog->err(0, 0, "em3a() ModelInit failed.");
            em->r_no_0 = 0xFF;
            return;
        }
        break;
    }
    if (em->type == 2) {
        em->Motion.flip = em3a_flip_tbl;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 400.0f, 350.0f, 350.0f, 300.0f, 1, 0x2000, 10);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    switch (em->type) {
    case 0:
    case 1:
    default:
        YarareInit(em, 0.0f, 0.0f, -250.0f, 200.0f, 800.0f, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
        YarareAddCube(em, &w->hit[0], 0.0f, -50.0f, 0.0f, 250.0f, 100.0f, 250.0f, 5, YAT_FLAG_ON);
        YarareAddCube(em, &w->hit[1], 0.0f, -50.0f, 0.0f, 250.0f, 100.0f, 250.0f, 7, YAT_FLAG_ON);
        YarareAddCube(em, &w->hit[2], 0.0f, -170.0f, 180.0f, 50.0f, 140.0f, 250.0f, 0xA, YAT_FLAG_ON);
        YarareAddCube(em, &w->hit[3], 0.0f, -50.0f, 150.0f, 100.0f, 70.0f, 250.0f, 4, YAT_FLAG_ON);
        break;
    case 2:
        YarareInit(em, 0.0f, 50.0f, 0.0f, 250.0f, 0.0f, 1, YAT_FLAG_ON);
        YarareAddCube(em, &w->hit[0], 0.0f, -50.0f, 0.0f, 150.0f, 200.0f, 150.0f, 0xB, YAT_FLAG_ON);
        break;
    }
    if (em->type == 1) {
        Vec pos;
        Vec rot;
        cParts* p;

        pos.x = 0.0f;
        pos.y = -101.0f;
        pos.z = -143.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pMissile = SetHeliMissile(ARC(9), ARC(0xA), &pos, &rot, 1);
        if (w->pMissile) {
            w->pMissile->setParent(em, 9, 0);
        }
        p = em->getPartsPtr(0xA);
        p->scale.x = 0.0f;
        p->scale.y = 0.0f;
        p->scale.z = 0.0f;
    }
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), EFF_EM3A, 0);
    w->espKind = EspPullCoreKind();
    w->flags = 0;
    w->vibAng.x = fRand1_1() * PI;
    w->vibAng.y = fRand1_1() * PI;
    w->vibAng.z = fRand1_1() * PI;
    w->vibSpd.x = fRand0_1() * 0.06981317f + 0.08726646f;
    w->vibSpd.y = fRand0_1() * 0.02617994f + 0.06981317f;
    w->vibSpd.z = fRand0_1() * 0.02617994f + 0.12217305f;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->atkWait = 0;
    w->seTimer = 29;
    em3aPatrolInit(em);
    switch (em->type) {
    case 0:
    case 1:
    default:
        em->getPartsPtr(4)->ang.z = 0.61086524f;
        em->getPartsPtr(6)->ang.z = -0.61086524f;
        EstSet(em, -1, 0, 0, EFF_EM3A, 0, 1, w->espKind, em, 0);
        break;
    case 2:
        EstSet(em, -1, 0, 0, EFF_EM3A, 0xA, 1, w->espKind, em, 0);
        break;
    }
    em->setStatus(EM_STATUS_ACTIVE);
    switch (em->type) {
    case 0:
    case 1:
    default:
        switch (em->set) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0, 0, 0);
            break;
        case 1:
            EmRoutineSet(em, 1, 3, 0, 0);
            break;
        case 2:
            EmRoutineSet(em, 1, 3, 0, 1);
            break;
        case 3:
            EmRoutineSet(em, 1, 3, 0, 2);
            break;
        case 4:
            EmRoutineSet(em, 1, 3, 0, 3);
            break;
        case 5:
            EmRoutineSet(em, 1, 3, 0, 4);
            break;
        }
        break;
    case 2:
        switch (em->set) {
        case 5:
        default:
            EmRoutineSet(em, 1, 6, 0, 0);
            MotionSetCore(em, MOTION(em), ARC(0x12), 0, 0, 1, 0);
            break;
        case 6:
            EmRoutineSet(em, 1, 9, 0, 0);
            MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0, 1, 0);
            break;
        case 7:
            // Plain byte stores: the QImode zero keeps MotionSetCore's `li r9, 0` (em30_R0_Init).
            em->r_no_0 = 1;
            em->r_no_1 = 6;
            em->r_no_2 = 0;
            em->r_no_3 = 1;
            MotionSetCore(em, MOTION(em), ARC(0x12), 0, 0, 1, 0);
            break;
        }
        MotionMove(em, 0);
        break;
    }
    em3a_R0_Move(em);
}

// r_no_0 == 1: dispatches the r_no_1 state.
static void em3a_R0_Move(cEm3a* em)
{
    Em3a_R1_move_tbl[em->r_no_1](em);
}

// r_no_1 == 0 (gunship patrol): hovers turning slowly for 90..179 frames, then flies along the
// route network to the current EMI patrol point (10 units/frame once facing it), advancing to the
// next point on arrival; spotting the player (em3aFindPLCk) -> alert effect and attack (1).
static void em3a_R1_Patrol(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    Vec v;
    Vec a;
    Vec b;
    f32 fl;

    fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    switch (em->r_no_2) {
    case 0:
        w->timer = Rnd() % 90 + 90;
        em->r_no_2++;
    case 1:
        em3aHoverMove(em, w, fl);
        em->ang.y += 0.008726646f;
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (w->timer) {
            w->timer--;
        } else if (w->pRoute) {
            em->r_no_2++;
        }
        break;
    case 2:
        em->r_no_2++;
    case 3: {
        EmiEntry* r = w->pRoute;

        if (r) {
            a = em->pos;
            a.y = fl + 500.0f;
            b = r->pos;
            b.y += 500.0f;
            RouteCkPosToPos(&a, &b, &w->routePos);
            w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
            w->routeAngAbs = fabsf(w->routeAng);
            em->ang.y += Muku(&em->pos, &w->routePos, em->ang.y, 0.017453292f);
            if (w->routeAngAbs < 0.08726646f) {
                v.x = 0.0f;
                v.y = 0.0f;
                v.z = 10.0f;
                PSMTXMultVecSR(em->mat, &v, &v);
                PSVECAdd(&w->spd, &v, &w->spd);
            }
        }
        em3aHoverMove(em, w, fl);
        if (em3aPatrolUpdate(em)) {
            em->r_no_2 = 0;
        }
        break;
    }
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    if (em3aFindPLCk(em)) {
        w->flags |= 1;
        em3aFoundSet(em, w, 9, 0);
    }
    if (w->flags & 1) {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
}

// Gun / rocket attack wait by difficulty rank (R1_Atk / R1_FixAtk step 2).
static inline void em3aSetAtkTimer(Em3aWork* w)
{
    w->timer = 46;
    if (pG->Game_level <= 1) {
        w->timer = 76;
    }
    if (pG->Game_level <= 3) {
        w->timer = 61;
    }
    if (pG->Game_level > 6) {
        w->timer = 31;
    }
    if (pG->Game_level > 9) {
        w->timer = 16;
    }
}

// r_no_1 == 1 (gunship attack): steps 0/1 the gun-out motion 7 (flags bit1); 2/3 hover facing
// the player with the warning ticks (SE + effect every 15 frames) for the rank-based wait
// (16..76 frames; reset while the player is more than 30 degrees off, cleared when he is hidden
// or dead); losing sight or 10 m away -> chase (2); then a 90-frame gun burst (4/5: a shot every
// 3 frames, cut short 30 frames after a hit) or type 1's rocket (6/7); the gun-in motion 8 (8/9)
// ends the attack with a 60-frame hold-off.
static void em3a_R1_Atk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    f32 fl;
    f32 ang;

    if (em->r_no_2 == 0 && (w->flags & 2)) {
        em->r_no_2 = 2;
    }
    fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
        em->r_no_2++;
    case 1:
        em3aHoverMove(em, w, fl);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.034906585f);
        }
        if (MotionMove(em, 0)) {
            w->flags |= 2;
            em->r_no_2++;
        }
        break;
    case 2:
        em3aSetAtkTimer(w);
        em->r_no_2++;
    case 3:
        em3aHoverMove(em, w, fl);
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.034906585f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        ang = fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI));
        if (em3aLookPLCk(em) == 0 || em->l_pl > 100000000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (fabsf(em->pos.y - pPL->pos.y) > 5000.0f) {
            break;
        }
        if (ang > 0.5235988f) {
            w->timer = 76;
            break;
        }
        if (w->atkWait) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 15 == 0) {
                SndCall(8, 3, &em->pos, em->id, 0, em);
                EstSet(em, -1, 0, 0, EFF_EM3A, 0x11, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        } else if (em3aBossCk(em) == 0) {
            if (em->type == 1) {
                em->r_no_2 = 6;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 4:
        w->timer = 90;
        em->r_no_2++;
    case 5:
        em3aHoverMove(em, w, fl);
        em3aTurnToPL(em, 0.017453292f);
        if (em3aBossCk(em)) {
            w->timer = 0;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 3 == 0) {
                if (em3aGunHitCk(em)) {
                    if (w->timer > 30) {
                        w->timer = 30;
                    }
                }
            }
        } else {
            w->atkWait = 30;
            em->r_no_2 = 2;
        }
        break;
    case 6:
        w->timer = 30;
        em->r_no_2++;
    case 7:
        em3aHoverMove(em, w, fl);
        em3aTurnToPL(em, 0.017453292f);
        if (w->timer) {
            w->timer--;
        } else {
            em3aRocketFire(em);
            em->r_no_2++;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 1, 0);
        w->flags &= ~2;
        em->r_no_2++;
    case 9:
        em3aHoverMove(em, w, fl);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.017453292f);
        }
        if (MotionMove(em, 0)) {
            w->atkWait = 60;
            em->r_no_2 = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

// r_no_1 == 2 (gunship chase): flies along the route network towards the player (10 units/frame
// once facing the route point); back to attack (1) once he is in sight within 10 m.
static void em3a_R1_Chase(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    Vec a;
    Vec b;
    Vec v;
    f32 fl;

    fl = SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    switch (em->r_no_2) {
    case 0:
        em->r_no_2++;
    case 1:
        a = em->pos;
        a.y = fl + 500.0f;
        b = pPL->pos;
        b.y += 500.0f;
        RouteCkPosToPos(&a, &b, &w->routePos);
        w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
        w->routeAngAbs = fabsf(w->routeAng);
        em->ang.y += Muku(&em->pos, &w->routePos, em->ang.y, 0.034906585f);
        if (w->routeAngAbs < 0.08726646f) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 10.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            PSVECAdd(&w->spd, &v, &w->spd);
        }
        em3aHoverMove(em, w, fl);
        if (em3aLookPLCk(em) && em->l_pl < 100000000.0f) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

// r_no_1 == 3 (fixed entrance, em->set 1..5): a 20-frame fly-in along the direction of r_no_3
// (0 up, 1 down, 2 left, 3 right, 4 forward; 250 / 350 units, damped), then the alert and the
// fixed attack (4) with the player marked found.
static void em3a_R1_Fix(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    Vec v;
    int t;

    SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    // Dead test (t is reassigned before its use): flow1 deletes the arms and jump2 the branch, but
    // the `w->flags` load behind the call gives `w` its sched priority (addi issued among the arg
    // moves) and the branch splits the block for gcse/sched1.
    if (w->flags & 2) {
        t = 1;
    } else {
        t = 0;
    }
    switch (em->r_no_2) {
    case 0:
        switch (em->r_no_3) {
        case 0:
        default:
            w->spd.x = 0.0f;
            w->spd.y = 250.0f;
            w->spd.z = 0.0f;
            break;
        case 1:
            w->spd.x = 0.0f;
            w->spd.y = -250.0f;
            w->spd.z = 0.0f;
            break;
        case 2:
            w->spd.x = -250.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
            break;
        case 3:
            w->spd.x = 250.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
            break;
        case 4:
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 350.0f;
            break;
        }
        t = 20;
        w->timer = t;
        em->r_no_2++;
    case 1:
        PSMTXMultVecSR(em->mat, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECScale(&w->spd, &w->spd, 0.9f);
        em3aVibMove(em);
        if (w->timer) {
            w->timer--;
        } else {
            w->flags |= 1;
            em3aFoundSet(em, w, 9, 0);
            PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

// r_no_1 == 4 (fixed attack): the attack routine without the height control: gun-out, the
// warning wait (reset beyond 15 m or 30 degrees off; losing sight -> chase 2), the 90-frame gun
// burst (aborted when sight is lost) or the rocket, gun-in; 60-frame hold-offs between rounds.
static void em3a_R1_FixAtk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    f32 ang;

    if (em->r_no_2 == 0 && (w->flags & 2)) {
        em->r_no_2 = 2;
    }
    SatMgr.getFloor(&em->pos, 0, 600.0f, 100000.0f, 0);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
        em->r_no_2++;
    case 1:
        em3aFixMove(em, w);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.034906585f);
        }
        if (MotionMove(em, 0)) {
            w->flags |= 2;
            em->r_no_2++;
        }
        break;
    case 2:
        em3aSetAtkTimer(w);
        em->r_no_2++;
    case 3:
        em3aFixMove(em, w);
        em->ang.y += Muku(&em->pos, &pPL->pos, em->ang.y, 0.034906585f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        ang = fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI));
        if (fabsf(em->pos.y - pPL->pos.y) > 5000.0f) {
            break;
        }
        if (w->atkWait) {
            break;
        }
        if (em3aLookPLCk(em) == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (em->l_pl > 225000000.0f || ang > 0.5235988f) {
            w->timer = 90;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 15 == 0) {
                SndCall(8, 3, &em->pos, em->id, 0, em);
                EstSet(em, -1, 0, 0, EFF_EM3A, 0x11, 0, ESP_CORE_KIND_NONE, em, 0);
            }
        } else if (em3aBossCk(em) == 0) {
            if (em->type == 1) {
                em->r_no_2 = 6;
            } else {
                em->r_no_2++;
            }
        }
        break;
    case 4:
        w->timer = 90;
        em->r_no_2++;
    case 5:
        em3aFixMove(em, w);
        em3aTurnToPL(em, 0.034906585f);
        if (em3aBossCk(em)) {
            w->timer = 0;
        }
        if (em3aLookPLCk(em) == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 3 == 0) {
                if (em3aGunHitCk(em)) {
                    if (w->timer > 30) {
                        w->timer = 30;
                    }
                }
            }
        } else {
            w->atkWait = 60;
            em->r_no_2 = 2;
        }
        break;
    case 6:
        w->timer = 30;
        em->r_no_2++;
    case 7:
        em3aFixMove(em, w);
        em3aTurnToPL(em, 0.034906585f);
        if (w->timer) {
            w->timer--;
        } else {
            em3aRocketFire(em);
            w->atkWait = 60;
            em->r_no_2 = 2;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 1, 0);
        w->flags &= ~2;
        em->r_no_2++;
    case 9:
        em3aFixMove(em, w);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.034906585f);
        }
        if (MotionMove(em, 0)) {
            w->atkWait = 60;
            em->r_no_2 = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

// r_no_1 == 5 (gunship death): dead / counted, the explosion effect 6 and SE, the engine SE off
// (type 2 adds a 4 m grenade blast), hidden and collision off; the crash is the effect's.
static void em3a_R1_Die(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->r_no_2 == 0) {
        em->hp = 0;
        EmSetDie(em);
        EmSetDieCnt(em);
        EffectEspDelete(1, w->espKind, em, 0);
        EffectEspgenDelete(1, w->espKind, em);
        EffectEfmDelete(1, w->espKind, em);
        EstSet(em, -1, 0, 0, EFF_EM3A, 6, 0, ESP_CORE_KIND_NONE, em, 0);
        SndStop(w->sndId, 0);
        SndCall(8, 2, &em->pos, em->id, 0, em);
        if (em->type == 2) {
            PlWepHitCheck2(0, &em->pos, &em->pos, 0x13, 3, 4000.0f);
        }
        em->setStatus(EM_STATUS_ACTIVE);
        em->be_flag &= ~2;
        em->atari.off();
        em->r_no_2++;
    }
}

// Type 2 r_no_1 == 6: hidden (flags bit3, no collision): the hidden idle 0x12, turning left /
// right in random spells (r_no_3 == 0), or held until the room's flag bit0 (r_no_3 == 1);
// spotting the player at its own height -> the appear alert and -> appear (8).
static void em3a_R1_B_HideWait(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    w->flags |= 8;
    switch (em->r_no_2) {
    case 0:
        em->atari.off();
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 3, 5, 0);
        w->flags &= ~1;
        w->timer = 30;
        w->turnDir = 0;
        em->r_no_2++;
    case 1:
        if (em->r_no_3) {
            MotionMove(em, 0);
            break;
        }
        if (w->turnDir) {
            // LIMIT_ANGLE written in both arms: the tails are cross-jumped after reload, while
            // a shared statement makes `ry` a global pseudo and swaps the f0/f13 temps.
            if (w->turnDir & 1) {
                em->ang.y += 0.02617994f;
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            } else {
                em->ang.y -= 0.02617994f;
                em->ang.y = LIMIT_ANGLE(em->ang.y);
            }
        }
        if (w->timer) {
            w->timer--;
        } else if (w->turnDir) {
            w->turnDir = 0;
            w->timer = Rnd() % 60 + 60;
        } else {
            w->turnDir = (Rnd() & 1) + 1;
            if (Rnd() % 10 > 4) {
                w->timer = 30;
            } else {
                w->timer = 60;
            }
        }
        MotionMove(em, 0);
        break;
    }
    if (em->r_no_3) {
        if (em->flag & 1) {
            w->flags |= 1;
        }
    } else if (em3aFindPLCk(em)) {
        if (fabsf(em->pos.y - pPL->pos.y) < 250.0f) {
            w->flags |= 1;
        }
    }
    if (w->flags & 1) {
        em3aFoundSet(em, w, 0xB, 0xA);
        EmRoutineSet(em, 1, 8, 0, 0);
    }
}

// Type 2 r_no_1 == 7: hides (lost the player): collision off, the hide motion 0x11 with SE and the
// idle / hide effects, then -> hidden wait (6).
static void em3a_R1_B_Hide(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.off();
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
        SndCall(8, 5, &em->getPartsPtr(0xA)->world, em->id, 0, em);
        EffectEspDelete(1, w->espKind, em, 0);
        EffectEspgenDelete(1, w->espKind, em);
        EffectEfmDelete(1, w->espKind, em);
        EstSet(em, -1, 0, 0, EFF_EM3A, 0xA, 1, w->espKind, em, 0);
        EstSet(em, -1, 0, 0, EFF_EM3A, 0xC, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
}

// Type 2 r_no_1 == 8: appears: collision on, the appear motion 0x13 with SE / effect, then ->
// move (0xA) with the lost counter reset.
static void em3a_R1_B_Appear(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.on();
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 3, 1, 0);
        SndCall(8, 5, &em->getPartsPtr(0xA)->world, em->id, 0, em);
        EstSet(em, -1, 0, 0, EFF_EM3A, 0xD, 0, ESP_CORE_KIND_NONE, em, 0);
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            w->lostCnt = 0;
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
        break;
    }
}

// Type 2 r_no_1 == 9 (em->set 6): visible idle: the idle 0xD then a turn 0x10 (randomly flipped),
// repeating; spotting the player at its height -> the alert and -> move (0xA).
static void em3a_R1_B_Wait(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->r_no_2) {
    case 0:
        em->atari.on();
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 3, 5, 0);
        w->flags &= ~1;
        w->turnDir = 0;
        em->r_no_2++;
    case 1:
        if (MotionMove(em, 0)) {
            em->r_no_2++;
        }
        break;
    case 2:
        if (Rnd() % 10 > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 3, 1, 0);
        }
        w->timer = Rnd() % 3 + 2;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            em->r_no_2 = 0;
        }
        break;
    }
    if (em3aFindPLCk(em)) {
        if (fabsf(em->pos.y - pPL->pos.y) < 250.0f) {
            w->flags |= 1;
        }
    }
    if (w->flags & 1) {
        em3aFoundSet(em, w, 0xB, 0xA);
        EmRoutineSet(em, 1, 0xA, 0, 0);
    }
}

// Type 2 r_no_1 == 0xA: closes in on the player along the route network: the move motion 0xE/0xF
// (2..4 cycles) turning towards the route point, a turn motion 0x10 when more than 30 degrees
// off, idle 0xD pauses of 60..119 frames; the player 5 m away for 90 frames (or 1 m above /
// below) -> hide (7). Within 3 m of the player (and no em39 near) nearCnt counts with a warning
// tick every 15 frames; past the rank limit (46..121 frames) -> the self-destruct blast (0xD).
static void em3a_R1_B_Move(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    int lim;

    RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0);
    w->routeAng = Muku(&em->pos, &w->routePos, em->ang.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), ARC(0xF), 5, 5, 0);
        w->timer = Rnd() % 3 + 2;
        em->r_no_2++;
    case 1:
        em->ang.y += Muku(&em->pos, &w->routePos, em->ang.y, 0.05235988f);
        em->ang.y = LIMIT_ANGLE(em->ang.y);
        if (MotionMove(em, 0)) {
            if (w->timer == 0) {
                em->r_no_2 = 4;
                break;
            }
            w->timer--;
        }
        if (w->lostCnt > 89) {
            EmRoutineSet(em, 1, 7, 0, 0);
            break;
        }
        if (w->routeAngAbs > 0.5235988f) {
            em->r_no_2 = 2;
        }
        break;
    case 2:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 5, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 5, 1, 0);
        }
        w->timer = Rnd() % 3 + 2;
        em->r_no_2++;
    case 3:
        if (MotionMove(em, 0)) {
            if (w->routeAngAbs < 0.5235988f) {
                em->r_no_2 = 0;
            } else {
                em->r_no_2 = 2;
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0xA, 5, 0);
        w->timer = Rnd() % 60 + 60;
        em->r_no_2++;
    case 5:
        MotionMove(em, 0);
        if (w->timer == 0) {
            em->r_no_2 = 0;
            break;
        }
        w->timer--;
        if (w->routeAngAbs > 1.0471976f) {
            em->r_no_2 = 2;
            break;
        }
        if (w->lostCnt > 89) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
    if ((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y)
            + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) < 9000000.0f
        && em3aBossNearCk(em) == 0) {
        u8 rank;

        w->nearCnt++;
        if (w->nearCnt % 15 == 5) {
            SndCall(8, 3, &em->pos, em->id, 0, em);
            EstSet(em, -1, 0, 0, EFF_EM3A, 0x12, 0, ESP_CORE_KIND_NONE, em, 0);
        }
        rank = pG->Game_level;
        lim = 76;
        if (rank <= 3) {
            lim = 91;
        }
        if (rank <= 1) {
            lim = 121;
        }
        if (rank > 6) {
            lim = 61;
        }
        if (rank > 9) {
            lim = 46;
        }
        if (w->nearCnt > lim) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
            return;
        }
    } else {
        w->nearCnt = 0;
    }
    if (em->l_pl > 25000000.0f) {
        w->lostCnt++;
    } else {
        w->lostCnt = 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 1000.0f) {
        w->lostCnt = 90;
    }
}

// Type 2 r_no_1 == 0xB (the weak spot was hit): the death motion 0x14 with effect / SE, hp 0;
// after 50 frames -> the blast (0xD).
static void em3a_R1_B_Die(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->r_no_2) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM3A, 0xF, 0, ESP_CORE_KIND_NONE, em, 0);
        SndCall(8, 7, &em->getPartsPtr(0xA)->world, em->id, 0, em);
        em->hp = 0;
        w->timer = 50;
        em->r_no_2++;
    case 1:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        }
        break;
    }
}

// Type 2 r_no_1 == 0xC (killed while hidden): the appear-and-die motion 0x15 with effect / SEs,
// hp 0; after 50 frames -> the blast (0xD).
static void em3a_R1_B_AppearDie(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->r_no_2) {
    case 0: {
        Vec* wp;

        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, 1, 0);
        EstSet(em, -1, 0, 0, EFF_EM3A, 0xE, 0, ESP_CORE_KIND_NONE, em, 0);
        wp = &em->getPartsPtr(0xA)->world;
        SndCall(8, 5, wp, em->id, 0, em);
        SndCall(8, 7, wp, em->id, 0, em);
        em->hp = 0;
        w->timer = 50;
        em->r_no_2++;
    }
    case 1:
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        }
        break;
    }
}

// Type 2 r_no_1 == 0xD: the self-destruct: dead / counted, the explosion effect 0x10 and SE, the
// item drop, a grenade-type blast hit check with a 3 m range at 250 up; hidden, collision off.
static void em3a_R1_B_Bomb(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->r_no_2 == 0) {
        Vec p;

        em->hp = 0;
        EmSetDie(em);
        EmSetDieCnt(em);
        EffectEspDelete(1, w->espKind, em, 0);
        EffectEspgenDelete(1, w->espKind, em);
        EffectEfmDelete(1, w->espKind, em);
        EstSet(em, -1, 0, 0, EFF_EM3A, 0x10, 0, ESP_CORE_KIND_NONE, em, 0);
        SndStop(w->sndId, 0);
        SndCall(8, 2, &em->pos, em->id, 0, em);
        em->clearStatus(EM_STATUS_ACTIVE);
        em->setStatus(EM_STATUS_ITEMSET);
        EmSetDropItem(em);
        p = em->pos;
        p.y += 250.0f;
        PlWepHitCheck2(0, &p, &p, 0x13, 3, 3000.0f);
        em->be_flag &= ~2;
        em->atari.off();
        em->r_no_2++;
    }
}

// Hover wobble: three sine offsets (6 / 3 / 6 units) at the random speeds set at init.
void em3aVibMove(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    w->vibAng.x += w->vibSpd.x;
    em->pos.x += SINF(w->vibAng.x) * 6.0f;
    w->vibAng.y += w->vibSpd.y;
    em->pos.y += SINF(w->vibAng.y) * 3.0f;
    w->vibAng.z += w->vibSpd.z;
    em->pos.z += SINF(w->vibAng.z) * 6.0f;
}

// Gunship gun pitch (parts 9) while the gun is out: eased towards the player's chest (1.3 m up),
// between -15 and +45 degrees.
void em3aGunMove(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->type != 2 && (w->flags & 2)) {
        Vec t;
        Vec d;
        cParts* p;
        f32 len;
        f32 ang;

        t = pPL->pos;
        t.y += 1300.0f;
        em->getPartsPtr(0);
        p = em->getPartsPtr(9);
        PSVECSubtract(&t, &p->world, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        ang = -atan2f(d.y, len);
        if (ang < -0.2617994f) {
            ang = -0.2617994f;
        }
        if (ang > 0.7853982f) {
            ang = 0.7853982f;
        }
        p->ang.x += Muku2(p->ang.x, ang + 1.5707964f, 0.024543693f);
        p->ang.x = LIMIT_ANGLE(p->ang.x);
    }
}

// Damage of the registered hit: 5x the weapon's table value (GetWepDmVal; the close-range flag
// for hits within 6 m), 500 for unknown weapons.
int em3aSetDmVal(cEm3a* em)
{
    int flag;
    int val;

    flag = 0;
    if (em->dmg.m_pDamageYarare->len < 36000000.0f) {
        flag = 1;
    }
    val = 100;
    if (em->dmg.m_Wep <= 0x2D) {
        val = GetWepDmVal(em, em->dmg.m_Wep, flag);
    }
    return val * 5;
}

// Patrol route start: the room's EMI entry of type 0x13 with state == this enemy's Character and
// index 0 (pad_3), or none.
void em3aPatrolInit(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    u32 i;

    w->pRoute = 0;
    if (pG->pEmi == 0) {
        return;
    }
    for (i = 0; i < (pG->pEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pEmi + o);

        if (e->type == 0x13 && e->state != 0 && e->state == em->Character && e->pad_3 == 0) {
            w->pRoute = e;
            return;
        }
    }
}

// Within 500 (XZ) of the current patrol point: advances to the entry with the next index of the
// same route (wrapping to index 0). Returns 1 when the point changed.
int em3aPatrolUpdate(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    EmiData* emi;
    u32 i;

    emi = pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    if (w->pRoute == 0) {
        return 0;
    }
    if ((w->pRoute->pos.x - em->pos.x) * (w->pRoute->pos.x - em->pos.x)
            + (w->pRoute->pos.z - em->pos.z) * (w->pRoute->pos.z - em->pos.z)
        > 250000.0f) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 0x13 && e->state != 0 && e->state == em->Character && e->pad_3 == w->pRoute->pad_3 + 1) {
            w->pRoute = e;
            return 1;
        }
    }
    for (i = 0; i < (pG->pEmi)->n; i++) {
        EmiEntry* e = &(pG->pEmi)->entry[i];

        if (e->type == 0x13 && e->state != 0 && e->state == em->Character && e->pad_3 == 0) {
            w->pRoute = e;
            return 1;
        }
    }
    return 0;
}

// Spins the gunship's rotors (parts 5 and 7) 30 degrees per frame while alive.
void em3aFanMove(cEm3a* em)
{
    if (em->type != 2 && em->hp > 0) {
        cParts* p;

        p = em->getPartsPtr(5);
        p->ang.y += 0.5235988f;
        p->ang.y = LIMIT_ANGLE(p->ang.y);
        p = em->getPartsPtr(7);
        p->ang.y += 0.5235988f;
        p->ang.y = LIMIT_ANGLE(p->ang.y);
    }
}

// Has it noticed the player? Already found, within 2.5 m, or in front within 30 degrees and in
// sight within 15 m (5 m for type 2); the gunship also reacts to being hit, to a shot fired
// (Status_flg[0] bit23) within 15 m and to the noise bell within 10 m.
int em3aFindPLCk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    f32 range;
    f32 ang;

    if (w->flags & 1) {
        return 1;
    }
    if (em->l_pl < 6250000.0f) {
        return 1;
    }
    if (em->type == 2) {
        range = 5000.0f;
    } else {
        range = 15000.0f;
    }
    ang = fabsf(Muku(&em->pos, &pPL->pos, em->ang.y, PI));
    if (ang < 0.5235988f && em->l_pl < range * range) {
        cParts* p;
        Vec a;
        Vec b;

        if (em->type == 2) {
            p = em->getPartsPtr(0xA);
        } else {
            p = em->getPartsPtr(0);
        }
        a = p->world;
        b = pPL->pos;
        b.y += 1000.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            return 1;
        }
    }
    if (em->type == 2) {
        return 0;
    }
    if (EmDeadCk(em)) {
        return 1;
    }
    if (StaFlagChk(pG, STA_PL_FIRE) && em->l_pl < 225000000.0f) {
        return 1;
    }
    if (StaFlagChk(pG, STA_SE_BURST) && pG->SeInfo.type == 2) {
        if ((em->pos.x - pG->SeInfo.pos.x) * (em->pos.x - pG->SeInfo.pos.x)
                + (em->pos.y - pG->SeInfo.pos.y) * (em->pos.y - pG->SeInfo.pos.y)
                + (em->pos.z - pG->SeInfo.pos.z) * (em->pos.z - pG->SeInfo.pos.z)
            < 100000000.0f) {
            return 1;
        }
    }
    return 0;
}

// Line of sight to the player (no effect collision between 200 above the helicopter and 1 m above him).
int em3aLookPLCk(cEm3a* em)
{
    Vec a;
    Vec b;

    a = em->pos;
    b = pPL->pos;
    a.y += 200.0f;
    b.y += 1000.0f;
    return EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0;
}

// One gun shot: muzzle effect and SE, a 50 m line from the gun (parts 0xB) with a +-5 m random
// spread, traced as weapon 0xC for the other enemies (with its own hp zeroed) and against the
// player / partner (EmAtkLineHitCk): a miss puts the impact effect / gatling spark / SE on the
// wall; a hit shakes the pad and camera, blood, and 600 damage through em3a_atk_info. Returns 1 on a hit.
int em3aGunHitCk(cEm3a* em)
{
    Vec a;
    Vec b;
    EmAtkInfo atk;
    Vec hit;
    Vec nrm;
    Vec s;
    Vec rot;
    Vec d;
    u32 attr;
    cParts* p;
    cEm* hitEm;
    s16 hp;
    f32 len;
    int ret;

    EstSet(em, -1, 0, 0, EFF_EM3A, 3, 0, ESP_CORE_KIND_NONE, em, 0);
    SndCall(8, 1, &em->pos, em->id, 0, em);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = fRand1_1() * 5000.0f;
    b.y = fRand1_1() * 5000.0f;
    b.z = 50000.0f;
    p = em->getPartsPtr(0xB);
    PSMTXMultVec(p->mat, &a, &a);
    PSMTXMultVec(p->mat, &b, &b);
    hp = em->hp;
    em->hp = 0;
    PlWepHitCheck2(0, &a, &b, 0xC, 3, 6000.0f);
    hitEm = EmAtkLineHitCk(&a, &b, &hit, &nrm, &attr);
    em->hp = hp;
    if (hitEm == 0) {
        len = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
        rot.x = -atan2f(nrm.y, len);
        rot.y = atan2f(nrm.x, nrm.z);
        rot.z = 0.0f;
        PSVECScale(&nrm, &s, 30.0f);
        PSVECAdd(&hit, &s, &hit);
        EstSet(0, -1, &hit, &rot, EFF_EM3A, 4, 0, ESP_CORE_KIND_NONE, hitEm, hitEm);
        PSVECSubtract(&hit, &a, &d);
        EspSetGatling(a, d);
        SndCall(6, 0xA, &hit, 0, 0, 0);
        ret = 0;
    } else {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        SndCall(6, 0x15, &pPL->pos, 0, 0, pPL);
        QuakeExec(0, 0, 5, 22.0f, 2);
        EmPlBloodSet2(em, &em->pos, 1, 2, 7);
        atk = em3a_atk_info;
        EmAtkSetDamagePL(hitEm, &atk, &a, &b);
        ret = 1;
    }
    return ret;
}

// Fires a missile: a new cObjMissile under parts 9 launched at the player (target NULL).
void em3aRocketFire(cEm3a* em)
{
    Vec pos;
    Vec rot;
    cObjMissile* m;

    pos.x = 0.0f;
    pos.y = -101.0f;
    pos.z = -143.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    m = SetHeliMissile(ARC(9), ARC(0xA), &pos, &rot, 1);
    if (m) {
        m->setParent(em, 9, 0);
        m->setFire(0);
    }
}

// A live, visible em39 in the line of fire (within a 4 m wide, 30 m long box ahead, or within
// 45 degrees and 15 m): 1 = do not shoot.
int em3aBossCk(cEm3a* em)
{
    Mtx inv;
    Vec lp;
    u32 i;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (e->isAlive() && e->id == 0x39 && e->hp > 0 && (e->be_flag & 2)) {
            PSMTXMultVec(inv, &e->pos, &lp);
            if (lp.x > -4000.0f && lp.x < 4000.0f && lp.z > 0.0f && lp.z < 30000.0f) {
                return 1;
            }
            if (fabsf(Muku(&em->pos, &e->pos, em->ang.y, PI)) < 0.7853982f && lp.z > 0.0f && lp.z < 15000.0f) {
                return 1;
            }
        }
    }
    return 0;
}

// A live em39 within 4 m of the helicopter.
int em3aBossNearCk(cEm3a* em)
{
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* e = EmMgr.fastAt(i);

        if (e->isAlive() && e->id == 0x39 && e->hp > 0
            && (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y)
                    + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z)
                < 16000000.0f) {
            return 1;
        }
    }
    return 0;
}

// The gunship's engine SE (8/0) every 30 frames while alive; the handle is kept for SndStop.
void em3aEngineSe(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->type != 2 && em->hp > 0) {
        if (w->seTimer) {
            w->seTimer--;
        } else {
            w->seTimer = 29;
            w->sndId = SndCall(8, 0, &em->pos, em->id, 0, em);
        }
    }
}

// Room script: hold the gun / rocket attacks off for `frames`.
void cEm3a::setAtkWait(int frames)
{
    EM3A_WK(this)->atkWait = frames;
}
