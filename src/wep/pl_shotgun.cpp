// Shotgun player routines (wep07/wep08/wep33 modules, first routine object; real file name unknown):
// routine 2 of the player while a shotgun is equipped: ready, set (idle / turn), fire (pellet spread
// hit checks), down, reload (shell by shell). Modelled on game/pl_knife.cpp; pl0a/wep07.cpp is
// Krauser's reduced build of the same file.
//
// Entry: PlShotgunMove is the module's WeaponMoveFunc (pl_R1_Weapon, r_no_1 == 6). r_no_2 is the
// weapon state (0 ready, 1 set, 2 fire, 4 reload), r_no_3 the step, mirrored into the weapon
// object's wep.mode / wep.step. Weapon archive slots (pG->pWep): 0x18 draw, 0x1A/0x20/0x22 aim
// idle down/level/up (mot3 pitch blend on m3r), 0x1E/0x21/0x23 fire, 0x1F holster, 0x2A/0x2C/0x2E
// reload by weapon_lv_reload. weapon_no 7 is the shotgun, 8 the Striker (faster draw, shorter
// recoil, 19 pellets instead of 13), 0x21 the wep33 pump shotgun.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"
#include "math_sub.h"
#include "esp.h"



static void wep07_r2_ready(cPlayer* pl);
static void wep07_r3_ready00(cPlayer* pl);
static void wep07_r3_ready10(cPlayer* pl);
static void wep07_r3_ready20(cPlayer* pl);
static void wep07_r2_set(cPlayer* pl);
static void wep07_r3_set00(cPlayer* pl);
static void wep07_r3_set10(cPlayer* pl);
static void wep07_r3_set20(cPlayer* pl);
static void wep07_r3_set30(cPlayer* pl);
static void wep07_r3_set40(cPlayer* pl);
static void wep07_r2_fire(cPlayer* pl);
static void wep07_r3_fire00(cPlayer* pl);
static void wep07_r3_fire10(cPlayer* pl);
void wepDown(cPlayer* pl);
static void wep07_r2_reload(cPlayer* pl);

// WeaponMoveFunc of the shotgun modules: dispatches on r_no_2, runs the lock-on stick control and
// counts the shot timer m_ShotTimer down.
void PlShotgunMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r2_ready,
        wep07_r2_set,
        wep07_r2_fire,
        0,
        wep07_r2_reload,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
    if (pl->Wep->m_ShotTimer) {
        pl->Wep->m_ShotTimer--;
    }
}

// r_no_2 == 0: the ready (draw) state. After the step: aim key released -> footwork (r_no_1 0, or
// 0x11 crouch with stat bit6) at normal motion speed; reload key with shells left -> reload
// (m_Flag bit0, m_Work0 = 1); else the shoulder camera aims at the locked enemy or at the scenario
// hit of the player's forward line.
static void wep07_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_ready00,
        wep07_r3_ready10,
        wep07_r3_ready20,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0) {
        pl->Motion.Seq_speed = 1.0f;
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
    } else if (pl->keyReload() && pl->Wep->m_pWep->reloadable()) {
        pl->Motion.Seq_speed = 1.0f;
        pl->Wep->m_Flag |= 1;
        EmRoutineSet(pl, 0, 6, 4, 0);
        pl->m_Work0 = 1;
    } else if (pl->m_pEm) {
        CamCtrlShoulderSetAim(&pl->m_pEm->pos);
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->world, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

// ready step 0: enter the aim. The start pitch comes from the camera pitch (doubled when looking
// up) into Wep->pitch and the mot3 rate m3r[0..1] (scaled to -1..1 over PI/2); resets the aim yaw
// m_Fwork0, the neck and the lock-on, stores the camera direction in m_CamAdjY, starts the draw
// motion 0x18 (blend 4 frames from a crouch, 5 otherwise; the Striker draws at 1.4x speed).
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
    pl->Wep->lockInit();
    hokan = 4;
    if (!(pl->stat & 0x40)) {
        hokan = 5;
    }
    mot = WEP_ARC_PTR(0x18);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, hokan, 0);
    if (pG->weapon_no == 8) {
        pl->Motion.Seq_speed = 1.4f;
    }
    mot3.move(m3r);
    pl->r_no_3 = 1;
}

// ready step 1: the draw motion plays; lock-on control from frame 5, draw SE at frame ~2 (0x29
// while m_Work2 == 1, else 0x28), the camera direction m_CamAdjY is folded into ang.y over the
// first 4 frames. At frame 10 (8 for the Striker): pump SE 2/9 and -> set state step 0.
static void wep07_r3_ready10(cPlayer* pl)
{
    f32 endFrame;

    if (pl->Motion.Seq_frame >= 5.0f) {
        PlWepLockCtrl(pl);
    }
    if (pl->Motion.Seq_frame > 1.7f && pl->Motion.Seq_frame < 2.3f) {
        int se = 0x28;

        if (pl->m_Work2 == 1) {
            se = 0x29;
        }
        SndCall(1, (u16) se, &pl->getPartsPtr(0)->world, 0, 0, 0);
    }
    if (pl->Motion.Seq_frame < 4.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->Motion.Seq_frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    pl->motionMove();
    if (pG->weapon_no == 8) {
        endFrame = 8.0f;
    } else {
        endFrame = 10.0f;
    }
    if (pl->Motion.Seq_frame >= endFrame) {
        pl->Motion.Seq_speed = 1.0f;
        SndCall(2, 9, &pl->getPartsPtr(0xA)->world, 0, 0, 0);
        EmRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 2: finish a motion set by a caller (the lock-on turn), then SE 5/0 and -> set step 0.
static void wep07_r3_ready20(cPlayer* pl)
{
    if (MotionMove(pl, 0)) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        EmRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 1: the set (aiming) state. Runs the step, the lock-on control and the laser sight,
// then: aim released -> wepDown (or crouch routine 0x11); fire trigger -> fire with shells, reload
// with an empty gun (m_Work0 = 0: automatic), else the empty-click SE 2/3; fire held with shells
// -> fire; reload key -> reload (m_Work0 = 1: manual).
static void wep07_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_set00,
        wep07_r3_set10,
        wep07_r3_set20,
        wep07_r3_set30,
        wep07_r3_set40,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
    pl->setLaserSight(1, 0);
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
        } else if (pl->Wep->m_pWep->reloadable()) {
            pl->Wep->m_Flag |= 1;
            EmRoutineSet(pl, 0, 6, 4, 0);
            pl->m_Work0 = 0;
        } else {
            SndCall(2, 3, &pl->getPartsPtr(4)->world, 0, 0, 0);
            goto reload;
        }
    } else if (joyFireOn() && pl->Wep->m_pWep->bulletNum()) {
        EmRoutineSet(pl, 0, 6, 2, 0);
    } else {
    reload:
        if (pl->keyReload() && pl->Wep->m_pWep->reloadable()) {
            pl->Wep->m_Flag |= 1;
            EmRoutineSet(pl, 0, 6, 4, 0);
            pl->m_Work0 = 1;
        }
    }
}

// set step 0: start the three-way aim idle (0x1A down / 0x20 level / 0x22 up on m3r), step 1.
static void wep07_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pWep;

    mot3.set(pl, PL_ARC_PTR(arc, 0x1A), PL_ARC_PTR(arc, 0x20), PL_ARC_PTR(arc, 0x22), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep07_r3_set10(cPlayer* pl)
{
    MotionMove(pl, 0);
}

// set step 2: a turn motion held while Key.on bit2 stays down (foot SEs 5/0 at frame 10 and 5/1 at
// frame 23); released -> step 0. Set by PlWepLockCtrl's turn request.
static void wep07_r3_set20(cPlayer* pl)
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
static void wep07_r3_set30(cPlayer* pl)
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

// set step 4: finish the fire recoil motion; ends or any fire / aim / action key -> step 0.
static void wep07_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 shoots, step 1 plays the recoil and pump).
static void wep07_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_fire00,
        wep07_r3_fire10,
        0,
    };

    func_tbl[pl->r_no_3](pl);
}

// fire step 0: the shot. trigger() spends the shell and fires the weapon object, the fire motions
// 0x1E/0x21/0x23 start, then 13 pellets (19 for the Striker) are traced from the right hand
// (parts 10) along -X for 50 m with PlWepHitCheck2 (6 m range): pellet 0 is the centre line from
// the muzzle (small random spread; splash SE 2/0xB when the marker is under water), pellets 1..n
// start on a hexagonal ring (150 units per ring) with a widening random spread of 1500 units per
// 3 pellets; PlWepHitCheck2 flag bit0 (every pellet not divisible by 4) skips the scenery impact
// effect and the noise bell, bit2 marks a secondary pellet for the target list. m_Work4/m_Work5 = 1,
// weapon object mode 2, PlWepLockRand recoils the aim. Then step 1.
static void wep07_r3_fire00(cPlayer* pl)
{
    Vec p0;
    Vec p1;
    Vec d;
    f32 wh;
    f32 pitch;
    PlArc* arc;
    f32 rnd;
    int n = 0xD;
    u32 i;
    cObjWep* obj;

    pl->Wep->m_pWep->trigger();
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x1E), PL_ARC_PTR(arc, 0x21), PL_ARC_PTR(arc, 0x23), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    MotionMove(pl, 0);
    pl->Body->waistMove();
    pl->partsWorldCalc();
    rnd = fRand1_1();
    if (pG->weapon_no == 8) {
        n = 0x13;
    }
    for (i = 0; i < n; i++) {
        cModel* parts = pl->getPartsPtr(0xA);
        u32 flag;

        if (i == 0) {
            p0.x = -15.5f;
            p0.y = -24.0f;
            p0.z = 38.33f;
        } else {
            p0.x = 234.5f;
            p0.y = -24.0f;
            p0.z = 38.33f;
        }
        p1.x = -50000.0f;
        p1.y = 0.0f;
        p1.z = 0.0f;
        if (i != 0) {
            f32 r = (f32) ((i - 1) / 6) + 1.0f;
            f32 r2;

            switch ((i - 1) % 6) {
            case 0:
                d.x = 0.0f;
                d.y = 0.5f;
                d.z = 0.866f;
                break;
            case 1:
                d.x = 0.0f;
                d.y = 0.5f;
                d.z = -0.866f;
                break;
            case 2:
                d.x = 0.0f;
                d.y = -0.5f;
                d.z = 0.866f;
                break;
            case 3:
                d.x = 0.0f;
                d.y = -0.5f;
                d.z = -0.866f;
                break;
            case 4:
                d.x = 0.0f;
                d.y = 1.0f;
                d.z = 0.0f;
                break;
            case 5:
                d.x = 0.0f;
                d.y = -1.0f;
                d.z = 0.0f;
                break;
            }
            p0.y += d.y * (r * 150.0f);
            p0.z += d.z * (r * 150.0f);
            rnd += fRand0_1() + PI;
            d.y = sinf(rnd);
            d.z = cosf(rnd);
            r2 = (f32) ((i - 1) / 3) + 1.0f;
            p1.y += d.y * r2 * 1500.0f;
            p1.z += d.z * r2 * 1500.0f;
        } else {
            Vec* mk;

            p1.y += fRand1_1() * 100.0f;
            p1.z += fRand1_1() * 100.0f;
            mk = &pl->Wep->m_pWep->m_ShotPos;
            if (GetWaterHeight(mk, &wh) && mk->y <= wh) {
                SndCall(2, 0xB, mk, 0, 0, 0);
            }
        }
        PSMTXMultVec(parts->mat, &p0, &p0);
        PSMTXMultVecSR(parts->mat, &p1, &p1);
        PSVECAdd(&p0, &p1, &p1);
        flag = 0;
        if (i & 3) {
            flag = 1;
        }
        if (i != 0) {
            flag |= 4;
        }
        PlWepHitCheck2(pl, &p0, &p1, pG->weapon_no, flag, 6000.0f);
    }
    pl->m_Work5 = 1;
    pl->m_Work4 = 1;
    obj = pl->Wep->m_pWep;
    obj->mode = 2;
    obj->step = 0;
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    pl->r_no_3 = 1;
}

// fire step 1: the recoil / pump motion (40 frames, 32 for the Striker); the lock-on control and
// the waist follow again from frame 10. At the end -> set step 4; holding fire on an empty gun
// plays the empty-click SE 2/3.
static void wep07_r3_fire10(cPlayer* pl)
{
    int endFrame;

    // The dead `cmpwi 0x21` after the tree needs a third node below 8 grouped with the default
    // (`case 7:`): the 3-node tree emits `bgt test` around it, jump.c inverts that into `ble default`
    // and jump2 cross-jumps the separate 0x21 body, leaving its compare.
    switch (pG->weapon_no) {
    case 8:
        endFrame = 0x20;
        break;
    case 0x21:
        endFrame = 0x28;
        break;
    case 7:
    default:
        endFrame = 0x28;
        break;
    }
    if (pl->Motion.Seq_frame >= 10.0f && joyKamae()) {
        PlWepLockCtrl(pl);
    }
    pl->motionMove();
    if (pl->Motion.Seq_frame >= (f32) endFrame) {
        if (joyKamae() && joyFireOn() && pl->Wep->m_pWep->bulletNum() == 0) {
            SndCall(2, 3, &pl->getPartsPtr(4)->world, 0, 0, 0);
        }
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 4;
    }
    if (pl->Motion.Seq_frame >= 10.0f) {
        pl->Body->waistMove();
        pl->partsWorldCalc();
    }
}

// Holster: footwork routine (r_no_1 0) sub-routine 2 with the weapon-down motion 0x1F when a
// motion may be set (dmMotCk), else straight to the idle (m_Hokan = 0xF). The weapon object's mode is
// left to cObjWep::move (unlike the handgun's wepDown).
void wepDown(cPlayer* pl)
{
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x1F), 0, 3, 5, 0);
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
// (0x2A/0x2C/0x2E), clears m_ShotCancelCtr, knifeStance = 1, weapon object mode 4 (the object
// loads the shells on its motion). Step 1 waits for PlReloadEndTbl's frame: aiming -> step 2; else
// footwork sub-routine 2 with m_Hokan = 9 (or crouch 0x11); a level aim (|m3r| <= 0.1) that runs
// the motion out returns to set step 0. Steps 2/3 blend the aim idle back over 8 frames (m_Work0).
static void wep07_r2_reload(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0: {
        void* mot;
        cObjWep* obj;

        switch (pG->weapon_lv_reload) {
        default:
            mot = WEP_ARC_PTR(0x2A);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x2C);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x2E);
            break;
        }
        MotionSetCore(pl, &pl->Motion, mot, 0, 3, 5, 0);
        pl->motionMove();
        pl->Wep->m_ShotCancelCtr = 0;
        pl->Wep->m_WepUd = 1;
        pl->r_no_3 = 1;
        obj = pl->Wep->m_pWep;
        obj->mode = 4;
        obj->step = 0;
        break;
    }
    case 1:
        pl->motionMove();
        if (m3r < -0.1f || m3r > 0.1f) {
            if (pl->Motion.Seq_frame >= PlReloadEndTbl[pG->weapon_no][pG->weapon_lv_reload]) {
                if (joyKamae()) {
                    pl->r_no_3 = 2;
                } else if (pl->stat & 0x40) {
                    pl->r_no_0 = 0;
                    pl->r_no_2 = 0;
                    pl->r_no_1 = 0x11;
                    pl->r_no_3 = 0;
                } else {
                    pl->m_Hokan = 9;
                    pl->m_Frame = 0;
                    pl->r_no_0 = 0;
                    pl->r_no_1 = 0;
                    pl->r_no_2 = 0;
                    pl->r_no_3 = 2;
                }
            }
        } else {
            if (joyKamae() == 0 && pl->Motion.Seq_frame >= PlReloadEndTbl[pG->weapon_no][pG->weapon_lv_reload]) {
                if (pl->stat & 0x40) {
                    pl->r_no_0 = 0;
                    pl->r_no_2 = 0;
                    pl->r_no_1 = 0x11;
                    pl->r_no_3 = 0;
                } else {
                    pl->m_Hokan = 9;
                    pl->m_Frame = 0;
                    pl->r_no_0 = 0;
                    pl->r_no_1 = 0;
                    pl->r_no_2 = 0;
                    pl->r_no_3 = 2;
                }
            }
            if (pl->Motion.Seq_frame >= (f32) (pl->Motion.Seq_frame_num - 1)) {
                EmRoutineSet(pl, 0, 6, 1, 0);
            }
        }
        break;
    case 2: {
        PlArc* arc = pG->pWep;

        mot3.set(pl, PL_ARC_PTR(arc, 0x1A), PL_ARC_PTR(arc, 0x20), PL_ARC_PTR(arc, 0x22), 0, 9, 0, 4, 0);
        pl->m_Work0 = 0;
        pl->r_no_3 = 3;
    }
    case 3:
        if ((int) ++pl->m_Work0 > 8) {
            EmRoutineSet(pl, 0, 6, 1, 0);
        }
        mot3.move(m3r);
        pl->motionMove();
        break;
    }
}

// The module's .data section is 8-aligned in the original.
asm(".section .data\n\t.balign 8\n\t.text");
