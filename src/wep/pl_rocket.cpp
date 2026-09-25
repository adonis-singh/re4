// Rocket launcher player routines (wep13 module, first object; real file name unknown): routine 2
// of the player while the launcher is equipped: ready (grip + aim), set (idle / turn), fire, down,
// next target (routine 5) and throw away (routine 6). Modelled on game/pl_knife.cpp.
//
// Entry: PlRocketMove is the wep13 module's WeaponMoveFunc (pl_R1_Weapon, r_no_1 == 6). r_no_2
// is the weapon state (0 ready, 1 set = scope view, 2 fire, 3 down, 5 next target, 6 throw the
// empty tube away), r_no_3 the step. The weapon object is the DOL's cObjLauncher (pl_wep.h): its
// grip()/gripBack() move the launcher between the back and the shoulder, launch happens in its
// moveFire (mode 2) along launcher.from/to = the scope camera trajectory stored by the set state.
// weapon_type 2 is the infinite launcher (kept after a shot, back to the scope or down); any other
// type is the single-shot one, thrown away (r_no_2 6, r_no_0 5, stat bit10 = tube gone).
// The knife routine (0xB) shares the launcher grip: down step 3 / ready step 2 use the player
// motion table 0x55..0x58 for the launcher <-> knife transitions. Weapon archive slots: 0x18
// shoulder, 0xF/0x12/0x14 aim idle, 0x11/0x13/0x15 fire, 0x16 throw away, 0x19 unshoulder,
// 0x20 the launcher's own grip motion.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_body.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "pad.h"
#include "snd.h"
#include "math_sub.h"


#define LAUNCHER(pl) ((cObjLauncher*) (pl)->Wep->m_pWep)

u8 lockCtr = 0;

static void wep13_r2_ready(cPlayer* pl);
static void wep13_r3_ready00(cPlayer* pl);
static void wep13_r3_ready10(cPlayer* pl);
static void wep13_r3_ready20(cPlayer* pl);
static void wep13_r3_ready30(cPlayer* pl);
static void wep13_r2_set(cPlayer* pl);
static void wep13_r3_set00(cPlayer* pl);
static void wep13_r3_set10(cPlayer* pl);
static void wep13_r3_set20(cPlayer* pl);
static void wep13_r3_set30(cPlayer* pl);
static void wep13_r3_set40(cPlayer* pl);
static void wep13_r2_fire(cPlayer* pl);
static void wep13_r3_fire00(cPlayer* pl);
static void wep13_r3_fire10(cPlayer* pl);
static void wep13_r2_down(cPlayer* pl);
static void wep13_r3_down00(cPlayer* pl);
static void wep13_r3_down10(cPlayer* pl);
static void wep13_r3_down20(cPlayer* pl);
static void wep13_r3_down30(cPlayer* pl);
static void wep13_r2_throw(cPlayer* pl);
static void wep13_r2_next(cPlayer* pl);

// WeaponMoveFunc of the launcher module: dispatches on r_no_2 (0..3, 5, 6) and runs the lock-on
// stick control.
void PlRocketMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r2_ready,
        wep13_r2_set,
        wep13_r2_fire,
        wep13_r2_down,
        0,
        wep13_r2_next,
        wep13_r2_throw,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
}

// r_no_2 == 0: the ready (shoulder) state. Aim key released before step 3 -> the launcher goes
// back to the back (grip(0), motion reset) and footwork (r_no_1 0, or 0x11 crouch with stat
// bit6). The shoulder camera aims at the locked enemy or the forward scenery hit.
static void wep13_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_ready00,
        wep13_r3_ready10,
        wep13_r3_ready20,
        wep13_r3_ready30,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0 && pl->r_no_3 != 3) {
        LAUNCHER(pl)->grip(0);
        pl->Wep->m_pWep->resetMotion();
        if (pl->stat.check(cPlayer::F_CROUCH)) {
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

// ready step 0: enter the aim: Wep->pitch from the camera pitch, aim yaw m_Fwork0 = 0, camera
// direction saved in m_CamAdjY, neck / lock-on reset, shoulder motion 0x18, mot3 pitch zeroed,
// lockCtr = 0. A launcher thrown away earlier (stat bit10) is shown again and gets its
// motions; the single-shot type is gripped back with its own motion 0x20.
static void wep13_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;

    pl->m_Work1 = 0;
    pl->Wep->m_CenterY = 0.0f;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->Wep->pitch = pitch;
    m3r.setDelay(0.0f);
    pitch *= 2.0f / PI;
    m3r.reset(pitch);
    pl->m_Fwork0 = 0.0f;
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    pl->Neck->init(0, 0, 0);
    pl->Wep->lockInit();
    mot = WEP_ARC_PTR(0x18);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r);
    m3r.reset(0.0f);
    m3r.setDelay(0.0f);
    lockCtr = 0;
    if (pl->stat.check(cPlayer::F_NO_LAUNCHER)) {
        pl->Wep->m_pWep->setDisp(0, 1);
        pl->stat.off(cPlayer::F_NO_LAUNCHER);
        pl->Wep->m_pWep->setMotion(pl);
    }
    if (pG->weapon_type != 2) {
        LAUNCHER(pl)->gripBack();
        pl->Wep->m_pWep->motionSet(WEP_ARC_PTR(0x20), 0, 0, 1, 0);
    }
    pl->r_no_3 = 1;
}

// ready step 1: the shoulder motion plays: ang.y turns to the camera direction over 4 frames, the
// launcher is gripped (grip(1)) at frame 11, at frame 32 -> set state (the scope).
static void wep13_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame < 4.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->Motion.Seq_frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
    pl->motionMove();
    if (MotionCheckCrossFrame(&pl->Motion, 11.0f)) {
        LAUNCHER(pl)->grip(1);
    }
    if (MotionCheckCrossFrame(&pl->Motion, 32.0f)) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }

}

// ready step 2 (entered from the knife routine when the aim key is pressed): the knife-to-launcher
// transition motion m_MotTbl[0x57]/[0x58]; step 3.
static void wep13_r3_ready20(cPlayer* pl)
{
    pl->motionSet(pl->m_MotTbl[0x57], 5, 0, 0, pl->m_MotTbl[0x58]);
    pl->motionMove();
    pl->r_no_3 = 3;
}

// ready step 3: the transition plays: knife face off at frame 5, launcher gripped at frame 14,
// at frame 39 -> set state.
static void wep13_r3_ready30(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->Motion, 5.0f)) {
        pl->Body->setKnife(false);
    }
    if (MotionCheckCrossFrame(&pl->Motion, 14.0f)) {
        LAUNCHER(pl)->grip(1);
    }
    if (MotionCheckCrossFrame(&pl->Motion, 39.0f)) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
    pl->motionMove();
}

// r2_set: the launcher line copy of `to` reads the frame directly (`lwz 0x18(r1)..0x20(r1)`) while
// `from` (frame offset 0) goes through an address register; a plain `obj->hpos = to` after
// `getTrajectory(&from, &to)` makes cse reuse the call's `&to` pseudo for the copy and gcse PRE
// hoists it into a callee-saved register. The copy through an inline taking the address by pointer
// keeps the frame-relative loads.
static inline void VecCopy(Vec* d, const Vec* s)
{
    *d = *s;
}

// r_no_2 == 1: the set state = looking through the launcher's scope (no laser). Aim released ->
// down (r_no_2 3, or crouch 0x11); fire held with a rocket loaded -> the scope trajectory is
// stored in launcher.from/to, the scope camera ends and -> fire.
static void wep13_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_set00,
        wep13_r3_set10,
        wep13_r3_set20,
        wep13_r3_set30,
        wep13_r3_set40,
    };

    func_tbl[pl->r_no_3](pl);
    pl->setLaserSight(0, 0);
    if (joyKamae() == 0) {
        if (pl->stat.check(cPlayer::F_CROUCH)) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 3;

            pl->setRno(0, 6, md, 0);
        }
    } else if (joyFireOn() && pl->Wep->m_pWep->bulletNum()) {
        Vec from;
        Vec to;
        cObjLauncher* obj;

        CameraMove();
        CamCtrl.getTrajectory(&from, &to);
        obj = LAUNCHER(pl);
        obj->lpos = from;
        VecCopy(&obj->hpos, &to);
        pl->endCamera();
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    }
}

// set step 0: start the aim idle (0xF/0x12/0x14 on the pitch), hide display type 1 of the
// launcher (the part in front of the eye), scope-on SE 2/9 and the scope camera (stat bit4
// once); step 1.
static void wep13_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pWep;

    mot3.set(pl, PL_ARC_PTR(arc, 0xF), PL_ARC_PTR(arc, 0x12), PL_ARC_PTR(arc, 0x14), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->Wep->m_pWep->setDisp(1, 0);
    SndCall(2, 9, &pl->pList->world, 0, 0, 0);
    if (!(pl->stat.check(cPlayer::F_SCOPE))) {
        CamCtrl.startScope(0, 0);
        pl->stat.on(cPlayer::F_SCOPE);
    }
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep13_r3_set10(cPlayer* pl)
{
    MotionMove(pl, 0);
}

// set step 2: a turn motion held while Key.on bit2 stays down (foot SEs at frames 10 and 23);
// released -> step 0. Set by PlWepLockCtrl's turn request.
static void wep13_r3_set20(cPlayer* pl)
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
static void wep13_r3_set30(cPlayer* pl)
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
static void wep13_r3_set40(cPlayer* pl)
{
    if (MotionMove(pl, 0) || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 launches, step 1 plays the recoil).
static void wep13_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_fire00,
        wep13_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
}

// fire step 0: the launch. trigger() spends the rocket and the launcher object (mode 2,
// cObjLauncher::moveFire) fires it along launcher.from/to; the fire motions 0x11/0x13/0x15 start
// level (m3r zeroed), display type 1 is shown again, controller vibration, PlWepLockRand kicks
// the aim yaw (the pitch result is discarded). Step 1.
static void wep13_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    cObjWep* obj;
    f32 pitch;

    pl->Wep->m_pWep->trigger();
    m3r.reset(0.0f);
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x11), PL_ARC_PTR(arc, 0x13), PL_ARC_PTR(arc, 0x15), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    MotionMove(pl, 0);
    pl->Wep->m_pWep->setDisp(1, 1);
    obj = pl->Wep->m_pWep;
    obj->r_no_0 = 2;
    obj->r_no_1 = 0;
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    m3r.reset(0.0f);
    pl->r_no_3 = 1;
}

// fire step 1: the recoil plays. Single-shot launcher: at frame 39 -> throw-away state (r_no_2 6).
// Infinite launcher: aim released from frame 45 -> down; at the motion end -> back into the scope
// (set state) while aiming, else down.
static void wep13_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    if (pG->weapon_type != 2) {
        if (MotionCheckCrossFrame(&pl->Motion, 39.0f)) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 6;
            pl->r_no_3 = 0;
        }
        return;
    }
    if (joyKamae() == 0 && pl->Motion.Seq_frame >= 45.0f) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 3;
        pl->r_no_3 = 0;
    }
    if (MotionGetState(pl)) {
        if (joyKamae()) {
            CamCtrl.startScope(0, 0);
            CameraMove();
            pl->stat.on(cPlayer::F_SCOPE);
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
        } else {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 3;
            pl->r_no_3 = 0;
        }
    }
}

// r_no_2 == 3: the down state (unshoulder, or switch to the knife). Unwinds the waist twist into
// ang.y, sets Status_flg[0] bit25 and runs the control-flag check.
static void wep13_r2_down(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_down00,
        wep13_r3_down10,
        wep13_r3_down20,
        wep13_r3_down30,
        0,
    };

    func_tbl[pl->r_no_3](pl);
    pl->ang.y = pl->ang.y - pl->Waist->set(0.0f, 0.4f);
    StaFlagOn(pG, STA_SSCRN_ENABLE);
    pl->checkCtrl();
}

// down step 0: end the scope camera (stat bit4 off), show the launcher fully; knife key held
// -> the launcher-to-knife transition m_MotTbl[0x55]/[0x56] and step 3, else the unshoulder
// motion 0x19 and step 1.
static void wep13_r3_down00(cPlayer* pl)
{
    CamCtrl.endScope();
    CameraMove();
    pl->stat.off(cPlayer::F_SCOPE);
    pl->Wep->m_pWep->setDisp(1, 1);
    if (joyLKamae()) {
        mot3.set(pl, pl->m_MotTbl[0x55], pl->m_MotTbl[0x55], pl->m_MotTbl[0x55], pl->m_MotTbl[0x56], 3, 0, 4, 0);
        mot3.move(m3r);
        pl->r_no_3 = 3;
    } else {

        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x19), 0, 7, 5, 0);
        pl->r_no_3 = 1;
    }
    pl->motionMove();
}

// down step 1: the unshoulder motion plays: launcher released to the back (grip(0)) at frame 13,
// SE 2/2 at frame 18; ends (SE 5/2) into footwork idle, or from frame 15 straight to the idle when
// a damage blocks motions. Knife key -> knife routine 0xB, aim key -> ready again, any fire / aim
// / action key -> footwork idle.
static void wep13_r3_down10(cPlayer* pl)
{
    const f32 gripFrame = 13.0f;
    const f32 seFrame = 18.0f;
    int end = pl->motionMove();

    if (dmMotCk() == 0 && pl->Motion.Seq_frame >= 15.0f) {
        pl->r_no_3 = 1;
        pl->m_Hokan = 0xF;
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->m_Frame = 0;
    } else if (end) {
        SndCall(5, 2, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    }
    if (MotionCheckCrossFrame(&pl->Motion, gripFrame)) {
        LAUNCHER(pl)->grip(0);
    }
    if (MotionCheckCrossFrame(&pl->Motion, seFrame)) {
        SndCall(2, 2, &pl->pList->world, 0, 0, 0);
    }
    if (joyLKamae()) {
        LAUNCHER(pl)->grip(0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0xB;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    } else if (joyKamae()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    } else if (Key.on & 0x10F) {
        LAUNCHER(pl)->grip(0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    }
}

// down step 2: start the idle walk motion (m_MotTbl[0]) and finish it in step 1 (an alternative
// entry, unused by the transitions here).
static void wep13_r3_down20(cPlayer* pl)
{
    MotionSetCore(pl, &pl->Motion, pl->m_MotTbl[0], 0, 3, 5, 0);
    pl->r_no_3 = 1;
}

// down step 3: the launcher-to-knife transition plays: at frame 9 the infinite launcher is
// gripped back (kept in hand), the single-shot one released; knife face on at frame 13. Knife key
// released -> footwork idle; the motion's end (SE 5/2) -> knife routine 0xB set state.
static void wep13_r3_down30(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->Motion, 9.0f)) {
        if (pG->weapon_type == 2) {
            LAUNCHER(pl)->gripBack();
        } else {
            LAUNCHER(pl)->grip(0);
        }
    }
    if (MotionCheckCrossFrame(&pl->Motion, 13.0f)) {
        pl->Body->setKnife(true);
    }
    if (joyLKamae() == 0) {
        LAUNCHER(pl)->grip(0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    }
    if (pl->motionMove()) {
        SndCall(5, 2, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0xB;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 6: throw the empty single-shot tube away: motion 0x16 with SE 2/2; at frame 18 the
// launcher object drops (r_no_0 5, cObjLauncher::moveDrop), stat bit10 marks it gone, the
// player's motion table gets the hand motions back (setMotion) and the routine leaves to footwork
// sub-routine 2.
static void wep13_r2_throw(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        pl->motionSet(WEP_ARC_PTR(0x16), 7, 0, 1, 0);
        SndCall(2, 2, &pl->pList->world, 0, 0, 0);
        pl->r_no_3 = 1;
    case 1:
        if (MotionCheckCrossFrame(&pl->Motion, 18.0f)) {
            cObjWep* obj;

            pl->stat.on(cPlayer::F_NO_LAUNCHER);
            obj = pl->Wep->m_pWep;
            obj->r_no_0 = 5;
            obj->r_no_1 = 0;
            pl->Wep->m_pWep->setMotion(pl);
            pl->setRno(0, 0, 2, 0);
        }
        pl->motionMove();
        break;
    }
}

// r_no_2 == 5: the next-target state (Key.trg bit5 in the lock control): turn towards the locked
// enemy m_pEm (PI/10 per frame beyond 200 units) with the waist straightened for 10 frames
// (m_Work0), then back to the set state. Another press cycles lockNext() (new target restarts,
// none -> set); aim released -> set state (or crouch 0x11).
static void wep13_r2_next(cPlayer* pl)
{
    cModel* em = pl->m_pEm;

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
        if ((int) pl->m_Work0++ > 9) {
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
        if (pl->stat.check(cPlayer::F_CROUCH)) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 1;

            pl->setRno(0, 6, md, 0);
        }
    }
}

// The module's .data section is 8-aligned (the original linker's placement; the tables start at 4).
asm(".section .data\n\t.balign 8\n\t.text");
