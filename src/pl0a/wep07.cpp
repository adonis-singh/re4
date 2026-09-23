// pl0a module, first object: Krauser's build of the shotgun player routines (wep07: ready / set / fire,
// no PlShotgunMove / set20 / set30 / set40 / reload — the routine table keeps their empty slots).
// Real file name unknown (the weapon modules' shared routine object).
//
// Nothing in the module registers these routines (Krauser's weapons come from their own wep
// modules), so the object is dead code kept by the link: the shotgun-style ready (0) / set (1) /
// fire (2) states of r_no_2 with r_no_3 steps, but driven from the PLAYER archive (pG->pPlayer:
// 0x8B draw, 0x8D/0x8F/0x4A aim idle, 0x8E/0x90/0x4B fire, 0x8C holster) instead of a weapon
// archive, with no ammo, no hit check and no weapon object.

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "cam_ctrl.h"
#include "motion.h"

static void wep07_r2_ready(cPlayer* pl);
static void wep07_r3_ready00(cPlayer* pl);
static void wep07_r3_ready10(cPlayer* pl);
static void wep07_r3_ready20(cPlayer* pl);
static void wep07_r2_set(cPlayer* pl);
static void wep07_r3_set00(cPlayer* pl);
static void wep07_r3_set10(cPlayer* pl);
static void wep07_r2_fire(cPlayer* pl);
static void wep07_r3_fire00(cPlayer* pl);
static void wep07_r3_fire10(cPlayer* pl);
void wepDown(cPlayer* pl);

// Routine 2 table of the full shotgun build (PlShotgunMove indexes it); unreferenced here.
static void (*wep07_func_tbl[5])(cPlayer*) = {
    wep07_r2_ready,
    wep07_r2_set,
    wep07_r2_fire,
    0,
    0,
};

// r_no_2 == 0: the ready (draw) state: aim key released -> footwork idle; else the shoulder
// camera aims at the forward scenery hit.
static void wep07_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_ready00,
        wep07_r3_ready10,
        wep07_r3_ready20,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->world, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

// ready step 0: enter the aim: Wep->pitch from the camera pitch (doubled looking up) into the
// mot3 rate m3r, aim yaw m_Fwork0 = 0, neck reset, camera direction saved in m_CamAdjY, the draw
// motion 0x8B of the player archive (blend 4 frames from a crouch, 5 otherwise).
static void wep07_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;
    int hokan;

    pl->m_Work1 = 0;
    pl->Wep->m_ShotTimer = 0;
    pl->Wep->m_CenterY = 0.0f;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->Wep->pitch = pitch;
    pitch *= 2.0f / PI;
    m3r.reset(pitch);
    m3r.setDelay(0.0f);
    pl->m_Fwork0 = 0.0f;
    pl->Neck->init(0, 0, 0);
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    hokan = 4;
    if (!(pl->stat & 0x40)) {
        hokan = 5;
    }
    mot = PL_ARC(0x8B);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, hokan, 0);
    mot3.move(m3r);
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays with the lock-on control from frame 5; at its end -> set state.
static void wep07_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame >= 5.0f) {
        PlWepLockCtrl(pl);
    }
    if (pl->motionMove()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 2: finish a motion set by the lock-on turn, then -> set state.
static void wep07_r3_ready20(cPlayer* pl)
{
    if (MotionMove(pl, 0)) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 1: the set (aiming) state with the lock-on control: aim released -> wepDown, fire
// held -> fire (no ammo check).
static void wep07_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_set00,
        wep07_r3_set10,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
    if (joyKamae() == 0) {
        wepDown(pl);
    } else if (joyFireOn()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    }
}

// set step 0: start the three-way aim idle (player archive 0x8D / 0x8F / 0x4A on m3r), step 1.
static void wep07_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pPlayer;

    mot3.set(pl, PL_ARC_PTR(arc, 0x8D), PL_ARC_PTR(arc, 0x8F), PL_ARC_PTR(arc, 0x4A), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep07_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// r_no_2 == 2: the fire state (step 0 starts the fire motion, step 1 plays it out).
static void wep07_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_fire00,
        wep07_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
}

// fire step 0: the fire motions (player archive 0x8E / 0x90 / 0x4B) replace the idle and the
// parts are recalculated; no shot, no hit check. Step 1.
static void wep07_r3_fire00(cPlayer* pl)
{
    PlArc* arc = pG->pPlayer;

    mot3.set(pl, PL_ARC_PTR(arc, 0x8E), PL_ARC_PTR(arc, 0x90), PL_ARC_PTR(arc, 0x4B), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    MotionMove(pl, 0);
    pl->Body->waistMove();
    pl->partsWorldCalc();
    pl->r_no_3 = 1;
}

// fire step 1: the fire motion plays; at its end -> set state.
static void wep07_r3_fire10(cPlayer* pl)
{
    if (pl->motionMove()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
}

// Holster: footwork routine (r_no_1 0) sub-routine 2 with the player archive's down motion 0x8C
// when a motion may be set (dmMotCk), else the idle with m_Hokan = 0xF.
void wepDown(cPlayer* pl)
{
    pl->motionMove();
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->Motion, PL_ARC(0x8C), 0, 3, 5, 0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    } else {
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->r_no_3 = 1;
        pl->m_Hokan = 0xF;
        pl->m_Frame = 0;
    }
}
