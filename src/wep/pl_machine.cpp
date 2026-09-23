// Machine gun player routines (wep11/wep12/wep27/wep29/wep39 modules, first routine object; real
// file name unknown): routine 2 of the player while a machine gun is equipped: ready, set (idle /
// turn), fire (burst with recoil), down, reload. Modelled on game/pl_knife.cpp.
//
// Entry: PlMachineMove is the module's WeaponMoveFunc (pl_R1_Weapon, r_no_1 == 6). r_no_2 is the
// weapon state (0 ready, 1 set, 2 fire, 3 down, 4 reload), r_no_3 the step, mirrored into the
// weapon object's r_no_0 / r_no_1. Full auto: the fire state loops step 1 -> 0 every 3 frames
// while the fire key is held and rounds remain. Weapon archive slots: 0x1A draw, 0x1B/0x1F/0x21
// aim idle down/level/up (mot3 pitch blend on m3r), 0x1C/0x20/0x22 fire, 0x1D holster,
// 0x1E/0x23/0x24 reload by weapon_lv_reload. weapon_no 0xB is the TMP (cocking SE on the draw).
// lockCtr (pl_wep.h) is defined here: the lock-on frame counter PlWepLockCtrl uses.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "joy.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "snd.h"
#include "rnd.h"
#include "math_sub.h"



u8 lockCtr = 0;

static void foo22(cPlayer* pl);
static void wep11_r2_ready(cPlayer* pl);
static void wep11_r3_ready00(cPlayer* pl);
static void wep11_r3_ready10(cPlayer* pl);
static void wep11_r3_ready20(cPlayer* pl);
static void wep11_r2_set(cPlayer* pl);
static void wep11_r3_set00(cPlayer* pl);
static void wep11_r3_set10(cPlayer* pl);
static void wep11_r3_set40(cPlayer* pl);
static void wep11_r2_fire(cPlayer* pl);
static void wep11_r3_fire00(cPlayer* pl);
static void wep11_r3_fire10(cPlayer* pl);
static void wep11_r2_down(cPlayer* pl);
static void wep11_r2_reload(cPlayer* pl);

// WeaponMoveFunc of the machine gun modules: dispatches on r_no_2 and runs the lock-on stick control.
void PlMachineMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r2_ready,
        wep11_r2_set,
        wep11_r2_fire,
        wep11_r2_down,
        wep11_r2_reload,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
}

// Empty ready steps 3..5 (the table keeps the size of the other weapons' ready tables).
static void foo22(cPlayer* pl)
{
}

// r_no_2 == 0: the ready (draw) state. Aim key released -> footwork (r_no_1 0, or 0x11 crouch
// with stat bit6); reload key with rounds -> reload (m_Flag bit0, m_Work0 = 1); else the
// shoulder camera aims at the forward scenery hit.
static void wep11_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r3_ready00,
        wep11_r3_ready10,
        wep11_r3_ready20,
        foo22,
        foo22,
        foo22,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0) {
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
    } else if (pl->keyReload() && pl->Wep->m_pWep->reloadable()) {
        pl->Wep->m_Flag |= 1;
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 4;
        pl->r_no_3 = 0;
        pl->m_Work0 = 1;
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->world, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

// ready step 0: enter the aim: pitch from the camera pitch (doubled when looking up) into
// Wep->pitch and the mot3 rate m3r, aim yaw m_Fwork0 = 0, camera direction saved in m_CamAdjY,
// neck / lock-on reset, TMP cocking SE 2/9, draw motion 0x1A, lockCtr = 0.
static void wep11_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;

    pl->m_Work1 = 0;
    pl->Wep->m_CenterY = 0.0f;
    pl->m_Fwork0 = 0.0f;
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->Wep->pitch = pitch;
    pitch *= 2.0f / PI;
    m3r.reset(pitch);
    m3r.setDelay(0.0f);
    pl->Neck->init(0, 0, 0);

    if (pG->weapon_no == 0xB) {
        SndCall(2, 9, &pl->getPartsPtr(0xA)->world, 0, 0, 0);
    }
    pl->Wep->lockInit();
    mot = WEP_ARC_PTR(0x1A);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r);
    lockCtr = 0;
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays: ang.y turns to the camera direction over the first 3 frames, draw
// SE at frame 2 (0x29 while m_Work2 == 1, else 0x28); at frame 5 -> set state step 4 (finish the
// motion) with m_Work0 = 0.
static void wep11_r3_ready10(cPlayer* pl)
{
    const f32 endFrame = 5.0f;   // pool order: 5.0 before the 3.0 / 4.0 / 2.0 of the statements above

    if (pl->Motion.Seq_frame <= 3.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->Motion.Seq_frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    if (MotionCheckCrossFrame(&pl->Motion, 2.0f)) {
        int se = 0x28;

        if (pl->m_Work2 == 1) {
            se = 0x29;
        }
        SndCall(1, (u16) se, &pl->getPartsPtr(0)->world, 0, 0, 0);
    }
    if (pl->Motion.Seq_frame >= endFrame) {
        EmRoutineSet(pl, 0, 6, 1, 4);
        pl->m_Work0 = 0;
    }

    MotionMove(pl, 0);
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 2: finish a motion set by the lock-on turn, then SE 5/0 and -> set step 0.
static void wep11_r3_ready20(cPlayer* pl)
{
    if (MotionMove(pl, 0)) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        EmRoutineSet(pl, 0, 6, 1, 0);
        pl->m_Work0 = 0;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 1: the set (aiming) state. Runs the step, the lock-on control and the laser sight,
// then: aim released -> down (r_no_2 3, or crouch 0x11); fire trigger / fire held with rounds ->
// fire (m_Work6 = 0, the burst recoil random restarts); trigger on an empty gun -> reload or the
// empty-click SE 2/0x17; the reload button (Joy trg 0x200, not keyReload) -> reload (m_Work0 = 1).
static void wep11_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r3_set00,
        wep11_r3_set10,
        0,
        0,
        wep11_r3_set40,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
    pl->setLaserSight(1, 0);
    if (joyKamae() == 0) {
        if (pl->stat.check(cPlayer::F_CROUCH)) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 3;

            EmRoutineSet(pl, 0, 6, md, 0);
        }
    } else if (joyFireTrg()) {
        if (pl->Wep->m_pWep->bulletNum()) {
            EmRoutineSet(pl, 0, 6, 2, 0);
            pl->m_Work6 = 0;
            PlWepLockRandInit();
        } else if (pl->Wep->m_pWep->reloadable()) {

            pl->Wep->m_Flag |= 1;
            EmRoutineSet(pl, 0, 6, 4, 0);
        } else {
            SndCall(2, 0x17, &pl->getPartsPtr(4)->world, 0, 0, 0);
            goto reload;
        }
    } else if (joyFireOn() && pl->Wep->m_pWep->bulletNum()) {
        EmRoutineSet(pl, 0, 6, 2, 0);
        pl->m_Work6 = 0;
        PlWepLockRandInit();
    } else {
    reload:
        if ((Joy[0].trg & 0x200) && pl->Wep->m_pWep->reloadable()) {
            pl->Wep->m_Flag |= 1;
            EmRoutineSet(pl, 0, 6, 4, 0);
            pl->m_Work0 = 1;
        }
    }
}

// set step 0: start the three-way aim idle (0x1B down / 0x1F level / 0x21 up on m3r), blended
// over 3 frames unless a burst just ended (m_Work0 != 0: cut); step 1.
static void wep11_r3_set00(cPlayer* pl)
{
    PlArc* arc;
    u8 hokan = 0;

    if (pl->m_Work0 == 0) {
        hokan = 3;
    }
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x1B), PL_ARC_PTR(arc, 0x1F), PL_ARC_PTR(arc, 0x21), 0, hokan, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep11_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// set step 4: finish the draw motion; ends or any fire / aim / action key -> step 0.
static void wep11_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 fires one round, step 1 the 3-frame cycle); the lock-on
// control runs before the step so the burst follows the target.
static void wep11_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r3_fire00,
        wep11_r3_fire10,
        0,
    };

    PlWepLockCtrl(pl);
    func_tbl[pl->r_no_3](pl);
}

// Never called (its two static Vecs are the 0x18 unreferenced .bss bytes before fire00's p0/p1).
static inline void wep11_hitCheck(cPlayer* pl)
{
    static Vec p0;
    static Vec p1;

    PlWepHitCheck2(pl, &p0, &p1, pG->weapon_no, 0, 6000.0f);
}

// fire step 0: one round. trigger() spends it and fires the weapon object; the fire motions
// 0x1C/0x20/0x22 start (cut, no blend); the bullet line runs from the right hand (parts 10),
// muzzle offset (-265.5, -24, 38.33), 50 m along -X with a +-200 random spread -> PlWepHitCheck2
// (6 m range). Weapon object mode 2, m_Work4/m_Work5 = 1, PlWepLockRand recoils the aim pitch /
// yaw, the laser is redrawn. Then step 1.
static void wep11_r3_fire00(cPlayer* pl)
{
    static Vec p0;
    static Vec p1;
    PlArc* arc;
    cModel* parts;
    f32 pitch;
    cObjWep* obj;

    pl->Wep->m_pWep->trigger();
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x1C), PL_ARC_PTR(arc, 0x20), PL_ARC_PTR(arc, 0x22), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    MotionMove(pl, 0);
    pl->Body->waistMove();
    pl->partsWorldCalc();
    parts = pl->getPartsPtr(0xA);
    p0.x = -265.5f;
    p0.y = -24.0f;
    p0.z = 38.33f;
    PSMTXMultVec(parts->mat, &p0, &p0);
    p1.x = -50000.0f;
    p1.y = fRand1_1() * 200.0f;
    p1.z = fRand1_1() * 200.0f;
    PSMTXMultVecSR(parts->mat, &p1, &p1);
    PSVECAdd(&p0, &p1, &p1);
    PlWepHitCheck2(pl, &p0, &p1, pG->weapon_no, 0, 6000.0f);
    obj = pl->Wep->m_pWep;
    obj->r_no_0 = 2;
    obj->r_no_1 = 0;
    pl->m_Work5 = 1;
    pl->m_Work4 = 1;
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    pl->Wep->m_pWep->drawLaserSight(1, 0);
    pl->r_no_3 = 1;
}

// fire step 1: the recoil plays (laser drawn without recalculation); from frame 3: fire held with
// rounds -> step 0 (next round), held on an empty magazine -> empty-click SE and the set state,
// released -> set state (m_Work0 = 1 after a burst, 0 otherwise: set00's blend); aim released ->
// down state (or crouch 0x11).
static void wep11_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    pl->Wep->m_pWep->drawLaserSight(1, 1);
    if (pl->Motion.Seq_frame >= 3.0f) {
        if (joyKamae()) {
            if (joyFireOn()) {
                if (pl->Wep->m_pWep->bulletNum()) {
                    pl->r_no_3 = 0;
                } else {
                    SndCall(2, 0x17, &pl->getPartsPtr(4)->world, 0, 0, 0);
                    EmRoutineSet(pl, 0, 6, 1, 0);
                }
                pl->m_Work0 = 1;
            } else {
                pl->m_Work0 = 0;
                EmRoutineSet(pl, 0, 6, 1, 0);
            }

        } else if (pl->stat.check(cPlayer::F_CROUCH)) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 3;

            EmRoutineSet(pl, 0, 6, md, 0);
        }
    }
}


// r_no_2 == 3: the down (holster) state, one frame: footwork routine (r_no_1 0) sub-routine 2 with
// the weapon-down motion 0x1D when a motion may be set (dmMotCk), else the idle with m_Hokan = 0xF.
static void wep11_r2_down(cPlayer* pl)
{
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x1D), 0, 3, 5, 0);
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

// r_no_2 == 4: the reload state. Step 0 starts the reload motion of the reload-speed level
// (0x1E/0x23/0x24), knifeStance = 1, weapon object mode 4 (the magazine refills on its motion).
// Step 1: aim released past PlReloadEndTbl's frame -> step 3 (or crouch 0x11); the motion's end
// -> set state step 0. Step 3 finishes the motion and leaves to footwork idle.
static void wep11_r2_reload(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0: {
        void* mot;
        cObjWep* obj;

        switch (pG->weapon_lv_reload) {
        default:
            mot = WEP_ARC_PTR(0x1E);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x23);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x24);
            break;
        }
        MotionSetCore(pl, &pl->Motion, mot, 0, 3, 5, 0);
        MotionMove(pl, 0);
        pl->Wep->m_WepUd = 1;
        pl->r_no_3 = 1;
        obj = pl->Wep->m_pWep;
        obj->r_no_0 = 4;
        obj->r_no_1 = 0;
        break;
    }
    case 1:
        if (MotionCheckCrossFrame(&pl->Motion, PlReloadEndTbl[pG->weapon_no][pG->weapon_lv_reload]) && joyKamae() == 0) {
            if (pl->stat.check(cPlayer::F_CROUCH)) {
                pl->r_no_0 = 0;
                pl->r_no_2 = 0;
                pl->r_no_1 = 0x11;
                pl->r_no_3 = 0;
            } else {
                pl->r_no_2 = 3;
            }
        } else if (MotionMove(pl, 0)) {
            EmRoutineSet(pl, 0, 6, 1, 0);
            pl->m_Work0 = 0;
        }
        break;
    case 3:
        if (MotionMove(pl, 0)) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
        }
        break;

    }
}

// The module's .data section is 8-aligned (the original linker's placement; the tables start at 4).
asm(".section .data\n\t.balign 8\n\t.text");
