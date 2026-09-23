// Grenade player routines (the pl_grenade object of the hand grenade modules wep19/30/41/42/45,
// byte-identical in all five; real file name unknown): routine 2 of the player while a throwable is
// equipped: ready (draw + aim, stance by the up/down keys), set (idle / turn), fire (throw), down
// and next target (routine 5). Modelled on game/pl_knife.cpp / wep/pl_handgun.cpp.
//
// Entry: PlGrenadeMove is the module's WeaponMoveFunc (pl_R1_Weapon, r_no_1 == 6). r_no_2 is the
// weapon state (0 ready, 1 set, 2 fire = throw, 3 down, 5 next target; 4 reload is an error),
// r_no_3 the step. The ammo is the item count (ItemMgr.bulletNum); the throw creates a cSubWep
// object (game/objSubWep.cpp) by weapon number: 0x13 hand grenade, 0x16 incendiary, 0x17 flash,
// 0x19/0x1F/0x20 the eggs. The grenade in the hand is the weapon's second object pObj2, the one on
// the belt m_pWep. Weapon archive slots: 0xF draw, 0x10 holster, 0x11/0x14/0x17 aim idle
// down/level/up (mot3 pitch on m3r), 0x12/0x15/0x18 throw, 0x13/0x16/0x19 throw of the last one.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "pad.h"
#include "snd.h"
#include "item.h"
#include "math_sub.h"

// game/objSubWep.cpp: the thrown grenade / egg objects (init only, the module never touches the rest)
class cSubWep : public cObj {
public:
    int init(Vec* rot, f32 power);
};

static void wep19_r2_ready(cPlayer* pl);
static void wep19_r3_ready00(cPlayer* pl);
static void wep19_r3_ready10(cPlayer* pl);
static void wep19_r3_ready20(cPlayer* pl);
static void wep19_r3_ready30(cPlayer* pl);
static void wep19_r2_set(cPlayer* pl);
static void wep19_r3_set00(cPlayer* pl);
static void wep19_r3_set10(cPlayer* pl);
static void wep19_r3_set20(cPlayer* pl);
static void wep19_r3_set30(cPlayer* pl);
static void wep19_r3_set40(cPlayer* pl);
static void wep19_r2_fire(cPlayer* pl);
static void wep19_r3_fire00(cPlayer* pl);
static void wep19_r3_fire10(cPlayer* pl);
static void wepDown(cPlayer* pl);
static void wep19_r2_next(cPlayer* pl);
void readyWeapon(cPlayer* pl);
void itemThrow(cPlayer* pl);

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos;
static Vec tgt;

// WeaponMoveFunc of the grenade modules: lock-on stick control, then the r_no_2 state; a reload
// request (r_no_2 == 4, from the shared keyReload path) is logged and turned back into ready.
void PlGrenadeMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r2_ready,
        wep19_r2_set,
        wep19_r2_fire,
        wepDown,
        0,
        wep19_r2_next,
    };

    pl->Wep->lockMove();
    if (pl->r_no_2 == 4) {
        pLog->err(0, 0, "ERROR:grenade cant reload action!!!!!");
        pl->r_no_2 = 0;
    }
    func_tbl[pl->r_no_2](pl);
}

// r_no_2 == 0: the ready (draw) state. r_no_3 == 100 is the re-entry marker (m_Work0 = 1). The
// stick picks knifeStance (up 0, down 2, else 1; the throw arc). Aim key released before the lock
// turn (step 3) -> footwork (r_no_1 0, or 0x11 crouch with stat bit6). The shoulder camera
// aims at the locked enemy or the forward scenery hit.
static void wep19_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r3_ready00,
        wep19_r3_ready10,
        wep19_r3_ready20,
        wep19_r3_ready30,
    };

    pl->m_Work0 = 0;
    if (pl->r_no_3 == 100) {
        pl->r_no_3 = 0;
        pl->m_Work0 = 1;
    }
    if (Key.on & 1) {
        if (pl->Wep->m_WepUd != 0) {
            pl->Wep->m_WepUd = 0;
        }
    } else if (Key.on & 2) {
        if (pl->Wep->m_WepUd != 2) {
            pl->Wep->m_WepUd = 2;
        }
    } else {
        if (pl->Wep->m_WepUd != 1) {
            pl->Wep->m_WepUd = 1;
        }
    }
    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0 && pl->r_no_3 != 3) {
        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
        }
    }
    if (pl->m_pEm) {
        CamCtrlShoulderSetAim(&pl->m_pEm->pos);
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->world, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

// ready step 0: enter the aim: camera direction saved in m_CamAdjY, Wep->pitch from the camera
// pitch, aim yaw m_Fwork0 = 0, neck / lock-on reset, draw motion 0xF; the mot3 pitch rate is then
// zeroed (the throw always starts level), draw SE 1/0x28, lockCtr = 0.
static void wep19_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;

    pl->m_Work1 = 0;
    pl->Wep->m_CenterY = 0.0f;
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->Wep->pitch = pitch;
    m3r.setDelay(0.0f);
    pitch *= 2.0f / PI;
    m3r.reset(pitch);
    pl->m_Fwork0 = 0.0f;
    pl->Neck->init(0, 0, 0);
    pl->Wep->lockInit();
    mot = WEP_ARC_PTR(0xF);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r);
    m3r.reset(0.0f);
    m3r.setDelay(0.0f);
    SndCall(1, 0x28, &pl->pParts->world, 0, 0, 0);
    lockCtr = 0;
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays while ang.y turns to the camera direction; at frame 4 -> set state
// step 4 (finish the motion).
static void wep19_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame < 4.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->Motion.Seq_frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    pl->motionMove();
    if (pl->Motion.Seq_frame >= 4.0f) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 4;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 2: like step 1 without the camera turn; frame 4 -> set step 4.
static void wep19_r3_ready20(cPlayer* pl)
{
    MotionMove(pl, 0);
    if (pl->Motion.Seq_frame >= 4.0f) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 4;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 3: the lock-on turn (PlWepLockCtrl's ready-turn request): turn towards `tgt` (PI/8
// per frame), slide towards `pos`, aim the pitch target at the target's elevation in 0.05
// steps (clamped -1..1); motion end -> set state step 0. `t` is assigned after the Muku call and
// lives across GetDistance3, as in pl_handgun.
static void wep19_r3_ready30(cPlayer* pl)
{
    f32 dist;
    f32 x;
    f64 a;
    Vec* t;

    if (MotionMove(pl, 0)) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
    pl->ang.y += Muku(&pl->pos, &tgt, pl->ang.y, PI / 8.0f);
    pl->pos.x = pl->pos.x * 0.6f + pos.x * 0.4f;
    pl->pos.z = pl->pos.z * 0.6f + pos.z * 0.4f;
    t = &tgt;
    dist = GetDistance3(&pos, t);
    a = atan2(t->y - pos.y, dist);
    x = a / (PI / 4.0f) - m3r;
    if (x > 0.05f) {
        x = 0.05f;
    }
    if (x < -0.05f) {
        x = -0.05f;
    }
    m3r += x;
    m3r.limit(-1.0f, 1.0f);
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 1: the set (aiming) state with the lock-on control (lockCtr counts down; no laser
// sight). Aim released -> down state (r_no_2 3, or crouch 0x11); fire trigger / held with an item
// left -> fire (throw).
static void wep19_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r3_set00,
        wep19_r3_set10,
        wep19_r3_set20,
        wep19_r3_set30,
        wep19_r3_set40,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
    if (lockCtr != 0) {
        lockCtr--;
    }
    if (joyKamae() == 0) {
        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 3;

            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = md;
            pl->r_no_3 = 0;
        }
    } else if ((joyFireTrg() || joyFireOn()) && ItemMgr.bulletNum()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    }
}

// set step 0: start the three-way aim idle (0x11 down / 0x14 level / 0x17 up on m3r), step 1.
static void wep19_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pWep;

    mot3.set(pl, PL_ARC_PTR(arc, 0x11), PL_ARC_PTR(arc, 0x14), PL_ARC_PTR(arc, 0x17), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep19_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// Two never-called routines the original linker dropped: only their constant pools (200, 500, 300,
// 0 | 30.000002, 283.5, 20, 0.1, 1, -0.2, 0.4, 0, -PI/4) and the static Vec survive (STRIP_UNUSED).
static void wep19_r3_set50(cPlayer* pl)
{
    Vec p;

    p.x = 200.0f;
    p.y = 500.0f;
    p.z = 300.0f;
    if (pl->Motion.Seq_frame != 0.0f) {
        PSMTXMultVec(pl->mat, &p, &p);
        pl->setPos(&p);
    }
}

// (never called, see above)
static void wep19_r3_set60(cPlayer* pl)
{
    static Vec rot = {-0.2617994f, 0.0f, 0.0f};
    f32 d;

    d = pl->Motion.Seq_frame * 30.000002f + 283.5f;
    d = d * 20.0f + 0.1f;
    rot.y = (1.0f - d) + -0.2f;
    rot.z = d * 0.4f;
    if (rot.z != 0.0f) {
        rot.x = -PI / 4.0f;
    }
    pl->setAng(&rot);
}

// set step 2: a turn motion held while Key.on bit2 stays down (foot SEs at frames 10 and 23);
// released -> step 0. Set by PlWepLockCtrl's turn request.
static void wep19_r3_set20(cPlayer* pl)
{
    if ((Key.on & 4) == 0) {
        pl->r_no_3 = 0;
    }
    MotionMove(pl, 0);
    if (pl->Motion.Seq_frame > 9.7f && pl->Motion.Seq_frame < 10.3f) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
    }
    if (pl->Motion.Seq_frame > 22.7f && pl->Motion.Seq_frame < 23.3f) {
        SndCall(5, 1, &pl->getPartsPtr(0x18)->world, 0, 0, 0);
    }
}

// set step 3: the same for the other turn direction (Key.on bit3).
static void wep19_r3_set30(cPlayer* pl)
{
    if ((Key.on & 8) == 0) {
        pl->r_no_3 = 0;
    }
    MotionMove(pl, 0);
    if (pl->Motion.Seq_frame > 9.7f && pl->Motion.Seq_frame < 10.3f) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
    }
    if (pl->Motion.Seq_frame > 22.7f && pl->Motion.Seq_frame < 23.3f) {
        SndCall(5, 1, &pl->getPartsPtr(0x18)->world, 0, 0, 0);
    }
}

// set step 4: finish the draw / throw motion, then step 0.
static void wep19_r3_set40(cPlayer* pl)
{
    if (MotionMove(pl, 0)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire (throw) state; the lock-on control keeps running while the aim key is held.
static void wep19_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r3_fire00,
        wep19_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae()) {
        PlWepLockCtrl(pl);
    }
}

// fire step 0: start the throw: 0x12/0x15/0x18 while more than one item is left (m_Work4 = 0:
// the next one is readied afterwards), 0x13/0x16/0x19 for the last one (m_Work4 = 1: the hand
// ends empty); throw SE 1/1, waist twisted to the lock yaw m_Fwork0. Step 1.
static void wep19_r3_fire00(cPlayer* pl)
{
    if (ItemMgr.bulletNum() > 1) {
        PlArc* arc = pG->pWep;

        mot3.set(pl, PL_ARC_PTR(arc, 0x12), PL_ARC_PTR(arc, 0x15), PL_ARC_PTR(arc, 0x18), 0, 3, 0, 4, 0);
        pl->m_Work4 = 0;
    } else {
        PlArc* arc = pG->pWep;

        mot3.set(pl, PL_ARC_PTR(arc, 0x13), PL_ARC_PTR(arc, 0x16), PL_ARC_PTR(arc, 0x19), 0, 3, 0, 4, 0);
        pl->m_Work4 = 1;
    }
    mot3.move(m3r);
    MotionMove(pl, 0);
    SndCall(1, 1, &pl->pParts->world, 0, 0, 0);
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(pl->m_Fwork0, 0.4f);
    pl->r_no_3 = 1;
}

// fire step 1: the throw plays: at frame 5 itemThrow releases the grenade and the hand model is
// hidden (display type 0 when it was the last). With more left: the next grenade is shown at frame
// 25 (SE 1/0) and at frame 30 -> set step 4. With the last one thrown: at frame 15 the right hand
// is reset and the routine leaves to footwork sub-routine 2 (or crouch 0x11).
static void wep19_r3_fire10(cPlayer* pl)
{
    const f32 throwFrame = 5.0f;

    pl->motionMove();
    if (MotionCheckCrossFrame(&pl->Motion, throwFrame)) {
        itemThrow(pl);
        if (ItemMgr.bulletNum()) {
            pl->Wep->m_pWepHand->setDisp(1, 0);
        } else {
            pl->Wep->m_pWepHand->setDisp(0, 0);
        }
    }
    if (pl->m_Work4 == 0) {
        if (pl->Motion.Seq_frame > 24.7f && pl->Motion.Seq_frame < 25.3f) {
            readyWeapon(pl);
            SndCall(1, 0, &pl->pParts->world, 0, 0, 0);
        }
        if (pl->Motion.Seq_frame >= 30.0f) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 4;
        }
    } else {
        if (pl->Motion.Seq_frame >= 15.0f) {
            pl->setRightHand(1);
            if (pl->stat & 0x40) {
                pl->r_no_0 = 0;
                pl->r_no_2 = 0;
                pl->r_no_1 = 0x11;
                pl->r_no_3 = 0;
            } else {
                pl->r_no_0 = 0;
                pl->r_no_1 = 0;
                pl->r_no_2 = 2;
                pl->r_no_3 = 0;
            }
        }
    }
}

// r_no_2 == 3: the down (holster) state, one frame: footwork sub-routine 2 with the put-away
// motion 0x10 when a motion may be set, else the idle with m_Hokan = 0xF; the waist twist is
// unwound into ang.y.
static void wepDown(cPlayer* pl)
{
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x10), 0, 3, 5, 0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    } else {
        pl->r_no_3 = 1;
        pl->m_Hokan = 0xF;
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->m_Frame = 0;
    }
    pl->motionMove();
    pl->ang.y = pl->ang.y - pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 5: the next-target state (Key.trg bit5 in the lock control): turn towards the locked
// enemy m_pEm (PI/10 per frame beyond 200 units) with the waist straightened for 10 frames
// (m_Work0), then back to the set state. Another press cycles lockNext() (a new target restarts,
// none -> set); aim released -> set state (or crouch 0x11).
static void wep19_r2_next(cPlayer* pl)
{
    cModel* em = pl->m_pEm;
    int n;

    switch (pl->r_no_3) {
    case 0:
        pl->m_Work0 = 0;
        pl->r_no_3 = 1;
        pl->m_Work1 = 0;
    case 1:
        if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
            pl->ang.y += Muku(&pl->pos, &em->pos, pl->ang.y, PI / 10.0f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        pl->m_Fwork0 = 0.0f;
        pl->Waist->set(0.0f, 0.4f);
        pl->Body->waistMove();
        pl->motionMove();
        n = pl->m_Work0++;
        if (n > 9) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
        }
        break;
    }
    if (Key.trg & 0x20) {
        if (pl->Wep->lockNext()) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 5;
            pl->r_no_3 = 0;
        } else {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
        }
    } else if (joyKamae() == 0) {
        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 1;

            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = md;
            pl->r_no_3 = 0;
        }
    }
}

// Shows the grenade in the hand again after a throw (the next one, while any is left); with one
// item left the belt grenade m_pWep is hidden (unless Debug_flg[2] bit22 keeps it).
void readyWeapon(cPlayer* pl)
{
    pl->Wep->m_pWepHand->setDisp(1, 1);
    if (!DbgFlagChk(pG, DBG_INF_BULLET) && ItemMgr.bulletNum() == 1) {
        pl->Wep->m_pWep->setDisp(0, 0);
    }
}

// Throws the equipped item: creates the sub weapon object of the weapon number (ObjMgr id 0x1A
// hand grenade, 0x29 incendiary, 0x2A flash, 0x3A egg; obj->type 0..5 = the weapon) and launches
// it from the player's angles along the aim pitch (m3r); spends one item (ItemMgr.trigger).
void itemThrow(cPlayer* pl)
{
    cObj* obj;
    int id;

    switch (pG->weapon_no) {
    case 0x13:
    case 0x1E:
    case 0x29:
    default:
        id = 0x1A;
        break;
    case 0x16:
        id = 0x29;
        break;
    case 0x17:
    case 0x2A:
        id = 0x2A;
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        id = 0x3A;
        break;
    }
    obj = ObjMgr.createBack(id);
    if (obj == 0) {
        pLog->err(0, 0, "itemThrow() grenade work alloc failed.");
        return;
    }
    switch (pG->weapon_no) {
    case 0x13:
        obj->type = 0;
        break;
    case 0x16:
        obj->type = 1;
        break;
    case 0x17:
        obj->type = 2;
        break;
    case 0x19:
        obj->type = 3;
        break;
    case 0x1F:
        obj->type = 4;
        break;
    case 0x20:
        obj->type = 5;
        break;
    }
    if (((cSubWep*) obj)->init(&pl->ang, m3r) == 0) {
        pLog->err(0, 0, "itemThrow() init failed.");
    }
    ItemMgr.trigger();
}
