// Handgun player routines (the pl_handgun object of the handgun weapon modules wep01/02/04/05/06/
// 15/38/43/44, byte-identical in all nine; real file name unknown): routine 2 of the player while a
// handgun is equipped: ready (draw + aim), set (idle / turn), fire, down (wepDown) and reload.
// Modelled on game/pl_knife.cpp / wep/pl_rocket.cpp.
//
// Entry: PlHandgunMove is the module's WeaponMoveFunc, called by pl_R1_Weapon (player.cpp) every
// frame while r_no_1 == 6. r_no_2 selects the weapon state (0 ready, 1 set, 2 fire, 4 reload),
// r_no_3 the step inside it; the routines mirror the state into the weapon object's r_no_0 /
// r_no_1 so cObjWep::move plays the matching weapon animation. Weapon archive slots (pG->pWep):
// 0x22/0x23 draw, 0x24/0x25 holster, 0x26..0x28 aim idle (down/level/up for the mot3 pitch blend),
// 0x29..0x2B fire, 0x2D..0x2F reload by weapon_lv_reload. The aim pitch is m3r[] (player.h):
// [1] target, [0] current, [2] mix; the waist twist is cPlWaist and the yaw of the lock-on turn
// goes through m_Fwork0 / PlWepLockRand.

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
#include "rnd.h"
#include "math_sub.h"

static void wep02_r2_ready(cPlayer* pl);
static void wep02_r3_ready00(cPlayer* pl);
static void wep02_r3_ready10(cPlayer* pl);
static void wep02_r3_ready20(cPlayer* pl);
static void wep02_r3_ready30(cPlayer* pl);
static void wep02_r2_set(cPlayer* pl);
static void wep02_r3_set00(cPlayer* pl);
static void wep02_r3_set10(cPlayer* pl);
static void wep02_r3_set40(cPlayer* pl);
static void wep02_r2_fire(cPlayer* pl);
static void wep02_r3_fire00(cPlayer* pl);
static void wep02_r3_fire10(cPlayer* pl);
void wepDown(cPlayer* pl);
static void wep02_r2_reload(cPlayer* pl);

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos = {0.0f, 0.0f, 0.0f};
static Vec tgt = {0.0f, 0.0f, 0.0f};

#ifndef PL_HANDGUN_NO_MOVE
// WeaponMoveFunc of the handgun modules: dispatches on r_no_2 (0 ready, 1 set, 2 fire, 4 reload;
// 3 = down has no entry, wepDown leaves the routine directly) and runs the lock-on stick control.
void PlHandgunMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r2_ready,
        wep02_r2_set,
        wep02_r2_fire,
        0,
        wep02_r2_reload,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
}
#else
// pl0d (Wesker, src/pl0d/wep02.cpp): the same object without PlHandgunMove; its routine table stays in .data.
static void (*wep02_func_tbl[])(cPlayer*) = {
    wep02_r2_ready,
    wep02_r2_set,
    wep02_r2_fire,
    0,
    wep02_r2_reload,
};
#endif

// r_no_2 == 0: the ready (draw) state. Runs the r_no_3 step, then: aim key released before the
// lock turn (step 3) -> back to footwork (r_no_1 0, or 0x11 crouch with stat bit6) and the
// weapon's enemy collision (atari flag 0x200) is cleared; reload key with rounds available -> reload (r_no_2 4,
// m_Flag bit0, m_Work0 = 1); otherwise the over-shoulder camera is aimed at the scenario hit of
// the player's forward line (10 m ahead, 1 m up).
static void wep02_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r3_ready00,
        wep02_r3_ready10,
        wep02_r3_ready20,
        wep02_r3_ready30,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0 && pl->r_no_3 != 3) {
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
            WEP_ATARI(pl)->clrFlag200();
        }
    } else if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
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

// ready step 0: enter the aim. Resets the lock-on (lockInit, PlSetLockPitch), the neck, the aim
// yaw m_Fwork0, puts the weapon object into mode 1 (ready), lets the weapon collide with enemies
// (atari flag 0x200) for weapon 2 (the Punisher), remembers the camera direction in m_CamAdjY (ready10 turns the player to
// it) and starts the draw motion 0x22/0x23 through mot3.
static void wep02_r3_ready00(cPlayer* pl)
{
    void* mot0;
    void* mot1;
    cObjWep* obj;

    pl->m_Work1 = 0;
    pl->Wep->m_CenterY = 0.0f;
    pl->Wep->lockInit();
    PlSetLockPitch(pl);
    pl->m_Fwork0 = 0.0f;
    pl->Neck->init(0, 0, 0);
    obj = WEP_OBJ(pl);
    obj->r_no_0 = 1;
    obj->r_no_1 = 0;
    if (pG->weapon_no == 2) {
        WEP_ATARI(pl)->setFlag200();
    }
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    mot0 = WEP_ARC_PTR(0x22);
    mot1 = WEP_ARC_PTR(0x23);
    mot3.set(pl, mot0, mot0, mot0, mot1, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->r_no_3 = 1;
}

// ready step 1: the draw motion plays. Frame ~2: draw SE (0x29 while m_Work2 == 1, else 0x28);
// over frames 0..4 the remaining m_CamAdjY is folded into ang.y so the player faces the camera
// direction; at frame 4 -> set state step 4 (r_no_2 1, r_no_3 4: finish the motion). Blends the
// aim pitch m3r and straightens the waist.
static void wep02_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame > 1.7f && pl->Motion.Seq_frame < 2.3f) {
        if (pl->m_Work2 == 1) {
            SndCall(1, 0x29, &pl->getPartsPtr(0)->world, 0, 0, 0);
        } else {
            SndCall(1, 0x28, &pl->getPartsPtr(0)->world, 0, 0, 0);
        }
    }
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

// ready step 2: like step 1 without the SE and the camera turn (a re-draw after a fire / reload:
// set by the shotgun-style callers; unused by the handgun's own transitions). Frame 4 -> set step 4.
static void wep02_r3_ready20(cPlayer* pl)
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

// ready step 3: the lock-on turn (set by PlWepLockCtrl's ready-turn request): the player turns
// towards `tgt` (PI/8 per frame) and slides 40 % per frame towards `pos` while the motion plays;
// the aim pitch target follows atan2(height difference, distance) in 0.05 steps, clamped
// to -1..1. Motion end: SE 5/0 and -> set state step 0.
// The pitch control is the
// shared PlWepAutoTrack tail (game/pl_wep.cpp). Forms that matter: `t` is assigned AFTER the Muku
// call and lives across GetDistance3 (a pseudo that already crosses a call is hoisted by sched1 above
// the earlier call and reload_cse turns its `addi` into the copy `mr r29, r4` of Muku's argument).
static void wep02_r3_ready30(cPlayer* pl)
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

// r_no_2 == 1: the set (aiming) state. Runs the step, the lock-on control and the laser sight,
// then: aim key released -> holster (wepDown, or crouch routine 0x11 with stat bit6); fire
// trigger with rounds -> fire (r_no_2 2), with an empty magazine -> reload (r_no_2 4, m_Flag
// bit0) or the empty-click SE 2/0x17; fire held with rounds -> fire; reload key -> reload
// (m_Work0 = 1 marks a manual reload).
static void wep02_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r3_set00,
        wep02_r3_set10,
        0,
        0,
        wep02_r3_set40,
    };
    int fire;

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
            wepDown(pl);
        }
        return;
    }
    fire = joyFireTrg();
    if (fire) {
        fire = WEP_OBJ(pl)->bulletNum();
        if (fire) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 2;
            pl->r_no_3 = 0;
            return;
        }
        if (WEP_OBJ(pl)->reloadable()) {
            pl->Wep->m_Flag |= 1;
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 4;
            pl->r_no_3 = 0;
            pl->m_Work0 = fire;
            return;
        }
        SndCall(2, 0x17, &pl->getPartsPtr(4)->world, 0, 0, 0);
    } else if (joyFireOn() && WEP_OBJ(pl)->bulletNum()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
        return;
    }
    if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 4;
        pl->r_no_3 = 0;
        pl->m_Work0 = 1;
    }
}

// set step 0: start the three-way aim idle (0x26 down / 0x27 level / 0x28 up blended by the
// pitch m3r) with a 3-frame blend-in, then step 1.
static void wep02_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pWep;

    mot3.set(pl, PL_ARC_PTR(arc, 0x26), PL_ARC_PTR(arc, 0x27), PL_ARC_PTR(arc, 0x28), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle (mot3 itself is advanced by the caller chain through m3r).
static void wep02_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// set step 4: finish the current motion (the draw / the fire recoil) and go to step 0 when it
// ends or as soon as the fire / aim / action keys (Key.on & 0x10F) are pressed.
static void wep02_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 shoots, step 1 plays the recoil); keeps the lock-on control
// running so the next shot's target is current.
static void wep02_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r3_fire00,
        wep02_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
}

// fire step 0: the shot. trigger() spends a round and fires the weapon object, the fire motions
// 0x29..0x2B replace the idle, the waist is twisted to the lock yaw m_Fwork0, and after a parts
// recalculation the bullet line is taken from the right hand (parts 10): muzzle offset (234.5,
// -24, 38.33) along -X for 50 m with a +-200 random spread -> PlWepHitCheck2 (6 m damage range).
// m_Work4/m_Work5 = 1 (shot fired flags read by the camera / partner), weapon object mode 2, and
// PlWepLockRand kicks the aim pitch / yaw for the recoil. Then step 1.
static void wep02_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    cParts* parts;
    cObjWep* obj;
    Vec p0;
    Vec p1;
    f32 pitch;

    WEP_OBJ(pl)->trigger();
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x29), PL_ARC_PTR(arc, 0x2A), PL_ARC_PTR(arc, 0x2B), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    MotionMove(pl, 0);
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(pl->m_Fwork0, 0.4f);
    WEP_ATARI(pl)->clrFlag200();
    pl->Body->waistMove();
    pl->partsWorldCalc();
    parts = pl->getPartsPtr(10);
    p0.x = 234.5f;
    p0.y = -24.0f;
    p0.z = 38.33f;
    PSMTXMultVec(parts->mat, &p0, &p0);
    p1.x = -50000.0f;
    p1.y = fRand1_1() * 200.0f;
    p1.z = fRand1_1() * 200.0f;
    PSMTXMultVecSR(parts->mat, &p1, &p1);
    PSVECAdd(&p0, &p1, &p1);
    PlWepHitCheck2(pl, &p0, &p1, pG->weapon_no, 0, 6000.0f);
    pl->m_Work5 = 1;
    pl->m_Work4 = 1;
    obj = WEP_OBJ(pl);
    obj->r_no_0 = 2;
    obj->r_no_1 = 0;
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    pl->r_no_3 = 1;
}

// fire step 1: the recoil plays until the weapon's shot frame (PlShotFrameTbl by fire-speed level,
// minus 2) is crossed, then -> set step 4 (the next shot may start); weapon 2 gets its atari flag
// 0x200 back.
static void wep02_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    if (MotionCheckCrossFrame(&pl->Motion, (f32) ((int) (u8) PlShotFrameTbl[pG->weapon_no][pG->weapon_lv_speed] - 2))) {
        if (pG->weapon_no == 2) {
            WEP_ATARI(pl)->setFlag200();
        }
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 4;
    }
}

// Holster: leaves the weapon routine for footwork (r_no_1 0) sub-routine 2 (the weapon-down
// motion 0x24/0x25, blended over 3 frames from a fresh draw or 5 frames from mid-aim) when a
// motion may be set (dmMotCk), else straight to the idle with m_Hokan = 0xF. Weapon object mode 3
// (down), atari flag 0x200 cleared, the waist twist is unwound into ang.y. Also called by the
// reload routine and the set state; exported for the module's own object.
void wepDown(cPlayer* pl)
{
    cObjWep* obj;

    if (dmMotCk()) {
        int hokan;
        int frame;

        if (pl->r_no_3 == 0) {
            hokan = 3;
            frame = 0;
        } else {
            hokan = 5;
            frame = 3;
        }
        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x24), WEP_ARC_PTR(0x25), hokan, 5, frame);
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
    obj = WEP_OBJ(pl);
    obj->r_no_0 = 3;
    obj->r_no_1 = 0;
    WEP_ATARI(pl)->clrFlag200();
    pl->ang.y = pl->ang.y - pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 4: the reload state. Step 0 starts the reload motion of the reload-speed level
// (0x2D/0x2E/0x2F) and weapon object mode 4 (the object refills the magazine when its motion
// ends). Step 1 waits for PlReloadEndTbl's frame: aiming -> step 2, else holster (wepDown; crouch
// with stat bit6); with a level aim (|m3r| <= 0.1) the motion may also run to its last
// frame and return to set step 0. Steps 2/3 blend the aim idle back in over 8 frames (m_Work0
// counts) and return to set step 0.
static void wep02_r2_reload(cPlayer* pl)
{
    u8 step = pl->r_no_3;
    cObjWep* obj;
    void* mot;

    switch (step) {
    case 0:
        switch (pG->weapon_lv_reload) {
        default:
            mot = WEP_ARC_PTR(0x2D);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x2E);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x2F);
            break;
        }
        MotionSetCore(pl, &pl->Motion, mot, 0, 3, 1, 0);
        pl->motionMove();
        pl->r_no_3 = 1;
        obj = WEP_OBJ(pl);
        obj->r_no_0 = 4;
        obj->r_no_1 = 0;
        break;
    case 1:
        if (m3r < -0.1f || m3r > 0.1f) {
            if (pl->Motion.Mot_frame >= PlReloadEndTbl[pG->weapon_no][pG->weapon_lv_reload]) {
                if (joyKamae()) {
                    pl->r_no_3 = 2;
                } else if (pl->stat.check(cPlayer::F_CROUCH)) {
                    pl->r_no_0 = 0;
                    pl->r_no_2 = 0;
                    pl->r_no_1 = 0x11;
                    pl->r_no_3 = 0;
                } else {
                    wepDown(pl);
                    pl->r_no_3 = step;
                }
            }
        } else {
            if (joyKamae() == 0 && pl->Motion.Mot_frame >= PlReloadEndTbl[pG->weapon_no][pG->weapon_lv_reload]) {
                if (pl->stat.check(cPlayer::F_CROUCH)) {
                    pl->r_no_0 = 0;
                    pl->r_no_2 = 0;
                    pl->r_no_1 = 0x11;
                    pl->r_no_3 = 0;
                } else {
                    wepDown(pl);
                    pl->r_no_3 = step;
                }
            } else if (pl->Motion.Seq_frame >= (f32) (pl->Motion.Seq_frame_num - 1)) {
                pl->r_no_0 = 0;
                pl->r_no_1 = 6;
                pl->r_no_2 = 1;
                pl->r_no_3 = 0;
            }
        }
        pl->motionMove();
        break;
    case 2: {
        PlArc* arc = pG->pWep;

        mot3.set(pl, PL_ARC_PTR(arc, 0x26), PL_ARC_PTR(arc, 0x27), PL_ARC_PTR(arc, 0x28), 0, 9, 0, 4, 0);
        pl->m_Work0 = 0;
        pl->r_no_3 = 3;
    }
    case 3:
        pl->m_Work0++;
        if ((int) pl->m_Work0 > 8) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
        }
        mot3.move(m3r);
        pl->motionMove();
        break;
    }
}

// The object's .data is 8-aligned in the original link (wep01: .data starts 4 bytes after the end of
// .rodata); the size is already a multiple of 8, so this only raises the section alignment.
asm(".section .data\n\t.balign 8\n\t.text");
