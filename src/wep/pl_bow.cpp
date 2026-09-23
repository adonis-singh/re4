// Bow player routines (wep28 module, first object; real file name unknown): routine 2 of the
// player while Krauser's bow is equipped: ready (draw the arrow), set (idle / turn), fire (shoot),
// down. Modelled on game/pl_knife.cpp.
//
// Entry: PlBowMove is the wep28 module's WeaponMoveFunc (pl_R1_Weapon, r_no_1 == 6). r_no_2 is
// the weapon state (0 ready, 1 set, 2 fire, 3 down; no reload: the arrow count is the ammo),
// r_no_3 the step; wep.mode / wep.step of the bow object follow. The arrow shown on the bow
// (cObjBow::setDispAllow) and the arrow held in the right hand (pObj2 display type 1 plus
// setRightHand(1)) are swapped as the draw / shoot motions play. Weapon archive slots: 0x1F draw,
// 0x20 holster, 0x21/0x24/0x27 aim idle down/level/up (mot3 pitch on m3r), 0x22/0x25/0x28 shoot.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "wep_mod.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "snd.h"
#include "math_sub.h"


#define BOW(pl) ((cObjBow*) (pl)->Wep->m_pWep)


static void wep28_r2_ready(cPlayer* pl);
static void wep28_r3_ready00(cPlayer* pl);
static void wep28_r3_ready10(cPlayer* pl);
static void wep28_r3_ready20(cPlayer* pl);
static void wep28_r2_set(cPlayer* pl);
static void wep28_r3_set00(cPlayer* pl);
static void wep28_r3_set10(cPlayer* pl);
static void wep28_r3_set20(cPlayer* pl);
static void wep28_r3_set30(cPlayer* pl);
static void wep28_r3_set40(cPlayer* pl);
static void wep28_r2_fire(cPlayer* pl);
static void wep28_r3_fire00(cPlayer* pl);
static void wep28_r3_fire10(cPlayer* pl);
static void wepDown(cPlayer* pl);

// WeaponMoveFunc of the bow module: dispatches on r_no_2 (no lockMove: the bow has no stick lock).
void PlBowMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r2_ready,
        wep28_r2_set,
        wep28_r2_fire,
        wepDown,
        0,
    };

    func_tbl[pl->r_no_2](pl);
}

// r_no_2 == 0: the ready (draw) state. Aim key released -> crouch 0x11, or the down state with
// r_no_3 = 0xD (wepDown blends the holster from that many frames in); else the shoulder camera
// aims at the forward scenery hit.
static void wep28_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r3_ready00,
        wep28_r3_ready10,
        wep28_r3_ready20,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0) {
        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            // 0xD and 6 share one register (`li r9,0xd; stb ff; li r9,6; stb fd`): one int local
            // assigned twice, not two constants.
            int no = 0xD;

            pl->r_no_3 = no;
            pl->r_no_0 = 0;
            pl->r_no_2 = 3;
            no = 6;
            pl->r_no_1 = no;
        }
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->world, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

// ready step 0: enter the aim: pitch from the camera pitch (doubled looking up) into Wep->pitch
// and m3r, aim yaw m_Fwork0 = 0, neck reset, camera direction saved in m_CamAdjY, bow object mode
// 1, draw motion 0x1F (blend 4 frames from a crouch, 5 otherwise).
static void wep28_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;
    cObjWep* obj;
    int hokan;

    pl->m_Work1 = 0;
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
    obj = pl->Wep->m_pWep;
    obj->mode = 1;
    obj->step = 0;
    hokan = 4;
    if (!(pl->stat & 0x40)) {
        hokan = 5;
    }
    mot = WEP_ARC_PTR(0x1F);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, hokan, 0);
    mot3.move(m3r);
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays: ang.y turns to the camera direction over 4 frames; at frame 4
// the arrow appears in the hand (pObj2 + right hand 1), at frame 11 it is nocked (hand arrow off,
// bow arrow on). Motion end -> set state.
static void wep28_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame < 4.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->Motion.Seq_frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    if (MotionCheckCrossFrame(&pl->Motion, 4.0f)) {
        pl->Wep->m_pWepHand->setDisp(1, 1);
        pl->setRightHand(1);
    } else if (MotionCheckCrossFrame(&pl->Motion, 11.0f)) {
        pl->Wep->m_pWepHand->setDisp(1, 0);
        BOW(pl)->setDispAllow(1);
    }
    if (pl->motionMove()) {
        EmRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 2: finish a motion set by the lock-on turn, then -> set state.
static void wep28_r3_ready20(cPlayer* pl)
{
    if (MotionMove(pl, 0)) {
        EmRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 1: the set (aiming) state with the laser sight and the lock-on control. Aim released
// -> wepDown (or crouch 0x11); fire trigger / held with arrows left -> fire.
static void wep28_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r3_set00,
        wep28_r3_set10,
        wep28_r3_set20,
        wep28_r3_set30,
        wep28_r3_set40,
    };

    func_tbl[pl->r_no_3](pl);
    pl->setLaserSight(1, 0);
    PlWepLockCtrl(pl);
    if (joyKamae() == 0) {
        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            wepDown(pl);
        }
    } else if (joyFireTrg()) {
        if (pl->Wep->m_pWep->bulletNum()) {
            EmRoutineSet(pl, 0, 6, 2, 0);
        }
    } else if (joyFireOn() && pl->Wep->m_pWep->bulletNum()) {
        EmRoutineSet(pl, 0, 6, 2, 0);
    }
}

// set step 0: start the three-way aim idle (0x21 down / 0x24 level / 0x27 up on m3r), step 1.
static void wep28_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pWep;

    mot3.set(pl, PL_ARC_PTR(arc, 0x21), PL_ARC_PTR(arc, 0x24), PL_ARC_PTR(arc, 0x27), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep28_r3_set10(cPlayer* pl)
{
    MotionMove(pl, 0);
}

// set step 2: a turn motion held while Key.on bit2 stays down (foot SEs at frames 10 and 23);
// released -> step 0. Set by PlWepLockCtrl's turn request.
static void wep28_r3_set20(cPlayer* pl)
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
static void wep28_r3_set30(cPlayer* pl)
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

// set step 4: finish the current motion; ends or any fire / aim / action key -> step 0.
static void wep28_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 shoots, step 1 plays the shot and re-nocks an arrow).
static void wep28_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r3_fire00,
        wep28_r3_fire10,
        0,
    };

    func_tbl[pl->r_no_3](pl);
}

// fire step 0: the shot. trigger() spends the arrow and the bow object (mode 2, cObjBow::moveFire)
// launches it as a projectile (setAllow), so there is no hit line here; the shoot motions
// 0x22/0x25/0x28 start, the hand arrow model is removed (right hand 0), m_Work4/m_Work5 = 1,
// PlWepLockRand kicks the aim. Step 1.
static void wep28_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    f32 pitch;
    cObjWep* obj;

    pl->Wep->m_pWep->trigger();
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x22), PL_ARC_PTR(arc, 0x25), PL_ARC_PTR(arc, 0x28), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    MotionMove(pl, 0);
    pl->m_Work5 = 1;
    pl->m_Work4 = 1;
    obj = pl->Wep->m_pWep;
    obj->mode = 2;
    obj->step = 0;
    pl->setRightHand(0);
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    pl->r_no_3 = 1;
}

// fire step 1: the shot motion plays with the lock-on control: at frame 16 the next arrow is
// taken in hand, at frame 35 nocked on the bow. Motion end -> set state; aim released from frame
// 5 -> down state with r_no_3 = 0xD.
static void wep28_r3_fire10(cPlayer* pl)
{
    int endFrame = 5;

    PlWepLockCtrl(pl);
    if (MotionCheckCrossFrame(&pl->Motion, 16.0f)) {
        pl->Wep->m_pWepHand->setDisp(1, 1);
        pl->setRightHand(1);
    } else if (MotionCheckCrossFrame(&pl->Motion, 35.0f)) {
        pl->Wep->m_pWepHand->setDisp(1, 0);
        BOW(pl)->setDispAllow(1);
    }
    if (pl->motionMove()) {
        EmRoutineSet(pl, 0, 6, 1, 0);
    } else if (pl->Motion.Seq_frame >= (f32) endFrame && joyKamae() == 0) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 3;
        pl->r_no_3 = 0xD;
    }
}

// r_no_2 == 3: the down (holster) state, one frame: hand arrow removed, bow object mode 3, the
// holster motion 0x20 (blended in from frame r_no_3) into footwork sub-routine 2 when a motion may
// be set, else the idle with m_Hokan = 0xF.
static void wepDown(cPlayer* pl)
{
    cObjWep* obj;

    pl->setRightHand(0);
    pl->Wep->m_pWepHand->setDisp(1, 0);
    obj = pl->Wep->m_pWep;
    obj->mode = 3;
    obj->step = 0;
    if (dmMotCk()) {
        pl->motionSet(WEP_ARC_PTR(0x20), 3, pl->r_no_3, 1, 0);
        EmRoutineSet(pl, 0, 0, 2, 0);
    } else {
        pl->r_no_3 = 1;
        pl->m_Hokan = 0xF;
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->m_Frame = 0;
    }
    pl->motionMove();
}
