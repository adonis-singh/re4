// wep17 module: the VP70 (weapon number 0x11, cObjVp70 object id 0x34) and the weapon number 3
// variant of the Mauser (wep02's cObjMauser, id 0x27, imported from that module). The module has
// its own player routine: the handgun routine (wep/pl_handgun.cpp) with an aim stance (the
// pWep->knifeStance byte), a wall check / enemy check at the ready start (ready00, ckEmWep), the
// lock target switch (r2_next) and the wall-facing "out" routine (r2_out).
//
// Entry: Wep17_init is the WeaponInitFunc, Wep17_move the WeaponMoveFunc (pl_R1_Weapon, r_no_1
// == 6). r_no_2 is the weapon state (0 ready, 1 set, 2 fire, 4 reload, 5 next target, 6 out),
// r_no_3 the step, mirrored into the weapon object's wep.mode / wep.step. The VP70 (0x11) fires a
// three-round burst while the trigger is held (m_Work4 counts); weapon 3 is the Red9 with this
// module's motions (weapon_type 2 = the stock variant's fire / reload set). Weapon archive slots:
// 0x11 draw, 0x12/0x17/0x19 aim idle down/level/up (mot3 pitch on m3r), 0x14/0x18/0x1A (Red9
// stock) / 0x21..0x23 (Red9) / 0x3D..0x3F (VP70) fire, 0x15 holster, 0x16 / 0x24 / 0x40,0x27,0x2A
// reload.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "esp.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "pad.h"
#include "snd.h"
#include "rnd.h"
#include "math_sub.h"
#include "em.h"

void ObjMauser_init(cObj* obj);   // wep02/objMauser.cpp (module import)
void ObjVp70_init(cObj* obj);     // wep17/objVp70.cpp

void Wep17_move(cPlayer* pl);
int ckEmWep(cPlayer* pl);
void wepDown(cPlayer* pl);
cObjWep* equipWeapon(cPlayer* pl);
static void wep17_r2_ready(cPlayer* pl);
static void wep17_r3_ready00(cPlayer* pl);
static void wep17_r3_ready10(cPlayer* pl);
static void wep17_r3_ready20(cPlayer* pl);
static void wep17_r3_ready30(cPlayer* pl);
static void wep17_r2_set(cPlayer* pl);
static void wep17_r3_set00(cPlayer* pl);
static void wep17_r3_set10(cPlayer* pl);
static void wep17_r3_set20(cPlayer* pl);
static void wep17_r3_set30(cPlayer* pl);
static void wep17_r3_set40(cPlayer* pl);
static void wep17_r2_fire(cPlayer* pl);
static void wep17_r3_fire00(cPlayer* pl);
static void wep17_r3_fire10(cPlayer* pl);
static void wep17_r2_reload(cPlayer* pl);
static void wep17_r2_next(cPlayer* pl);
static void wep17_r2_out(cPlayer* pl);

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos;
static Vec tgt;
// enemy found by ckEmWep (the out routine turns to it)
static cModel* pCkEm;

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the weapon object of weapon_no as
// Wep->m_pWep, installs its motions (for the Red9 the footwork set is then replaced by this
// module's own), loads the muzzle-flash effects (archive 0x4 as group 0x4B) and points the debug
// preview PlWepMot at 0x14/0x18/0x1A.
void Wep17_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep17_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        if (pG->weapon_no == 3) {
            WEP_MOT(pl, 0x00, 0x0A);
            NO_MOT(pl, 0x01);
            WEP_MOT(pl, 0x02, 0x0D);
            WEP_MOT(pl, 0x03, 0x1D);
            WEP_MOT(pl, 0x06, 0x0F);
            WEP_MOT(pl, 0x07, 0x1F);
            WEP_MOT(pl, 0x08, 0x0E);
            WEP_MOT(pl, 0x09, 0x1E);
            WEP_MOT(pl, 0x0B, 0x10);
            WEP_MOT(pl, 0x0C, 0x20);
            WEP_MOT(pl, 0x0D, 0x0B);
            WEP_MOT(pl, 0x0E, 0x1B);
            WEP_MOT(pl, 0x0F, 0x0C);
            WEP_MOT(pl, 0x10, 0x1C);
        }
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP17, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x14);
        PlWepMot[1] = WEP_ARC_PTR(0x18);
        PlWepMot[2] = WEP_ARC_PTR(0x1A);
    }
}

// WeaponMoveFunc: dispatches on r_no_2 (0, 1, 2, 4, 5, 6; 3 = down has no entry, wepDown leaves
// directly) and runs the lock-on stick control.
void Wep17_move(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep17_r2_ready,
        wep17_r2_set,
        wep17_r2_fire,
        0,
        wep17_r2_reload,
        wep17_r2_next,
        wep17_r2_out,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
}

// r_no_2 == 0: the ready (draw) state. r_no_3 == 100 is the re-entry from the out routine
// (m_Work0 = 1: skip the wall check). The stick picks knifeStance (up 0, down 2, else 1). Aim key
// released before the lock turn -> footwork (or crouch 0x11) with the weapon's enemy collision
// (atari 0x200) cleared; reload key with rounds -> reload (m_Flag bit0, m_Work0 = 1); else the
// shoulder camera aims at the locked enemy or the forward scenery hit.
static void wep17_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep17_r3_ready00,
        wep17_r3_ready10,
        wep17_r3_ready20,
        wep17_r3_ready30,
    };

    pl->m_Work0 = 0;
    if (pl->r_no_3 == 0x64) {
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
            WEP_ATARI(pl)->clrFlag200();
        }
    }
    if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
        pl->Wep->m_Flag |= 1;
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 4;
        pl->r_no_3 = 0;
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

// Alive enemy in front of the player (within a quarter turn) with a free line of sight: pCkEm.
// Only enemies whose checkThrow() is set count; returns 1 when one was found (the out routine
// then turns towards it).
int ckEmWep(cPlayer* pl)
{
    u32 i;

    pCkEm = 0;
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if (!em->isAlive()) {
            continue;
        }
        if (em->checkThrow() == 0) {
            continue;
        }
        if (Front_check(pl, em, PI / 2.0f) == 0) {
            continue;
        }
        if (SatMgr.hitCheck(&pl->pParts->world, &em->pParts->world, 0, 0, 0, 0)) {
            continue;
        }
        pCkEm = em;
        return 1;
    }
    return 0;
}

// ready step 0: aim start. Outside the first stage (and not coming back from the out routine) the player
// facing a wall turns away from it (routine 6, r2_out: x3E0 = the free side) and an enemy in front is
// faced instead. Wep->pitch from the camera pitch, aim yaw m_Fwork0 = 0, cocking SE 2/9, weapon
// object mode 1, its enemy collision (atari 0x200) on; the wall check (Status_flg[3] bit27 set,
// stage > 1) probes 1 m ahead at head height and picks the free side (left: m_Work0 0, right: 1)
// for the out routine, or turns to a visible enemy (ckEmWep, side from the stick). Otherwise the
// lock-on resets and the draw motion 0x11 starts. Forms that matter: `md` (an int holding 1) is what wep.mode and the left tail's x3E4
// share (r23); the x3E0 store is written first in both tails: the later use of the same register is
// the one the scheduler issues early (its REG_DEAD lowers the register weight), so the earlier store
// ends up last, before the call.
static void wep17_r3_ready00(cPlayer* pl)
{
    const f32 zero = 0.0f;
    cObjWep* obj;
    f32 pitch;
    void* m;
    int md;

    pl->m_Work1 = 0;
    pl->Wep->m_CenterY = zero;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > zero) {
        pitch += pitch;
    }
    pl->Wep->pitch = pitch;
    m3r.setDelay(zero);
    pitch *= 2.0f / PI;
    m3r.reset(pitch);
    pl->m_Fwork0 = zero;
    pl->Neck->init(0, 0, 0);
    SndCall(2, 9, &pl->getPartsPtr(0xA)->world, 0, 0, 0);
    obj = WEP_OBJ(pl);
    md = 1;
    obj->wep.mode = md;
    obj->wep.step = 0;
    AtariFlagsOr(WEP_ATARI(pl), 0x200);
    if (pG->stage_no > 1 && pl->m_Work0 == 0 && (StaFlagChk(pG, STA_SLOW))) {
        Vec nrm;
        Vec v0;
        Vec v1;

        v0.x = zero;
        v0.y = 1000.0f;
        v0.z = zero;
        PSVECAdd(&v0, &pl->pos, &v0);
        v1.x = zero;
        v1.y = zero;
        v1.z = 1000.0f;
        RotVector(&v1, &pl->ang, &v1);
        PSVECAdd(&v1, &v0, &v1);
        if (SatMgr.hitCheck(&v0, &v1, 0, &nrm, 0, 0)) {
            v1.x = zero;
            v1.y = 500.0f;
            v1.z = zero;
            PSVECAdd(&v1, &pl->pos, &v1);
            v0.x = -1000.0f;
            v0.y = zero;
            v0.z = zero;
            RotVector(&v0, &pl->ang, &v0);
            PSVECAdd(&v0, &v1, &v0);
            if (SatMgr.hitCheck(&v1, &v0, 0, 0, 0, 0) == 0) {
                v1.x = zero;
                v1.y = zero;
                v1.z = 2000.0f;
                RotVector(&v1, &pl->ang, &v1);
                PSVECAdd(&v1, &v0, &v1);
                if (SatMgr.hitCheck(&v0, &v1, 0, 0, 0, 0) == 0) {
                    pl->m_Work0 = 0;
                    pl->m_Work1 = 1;
                    pl->r_no_2 = 6;
                    pl->r_no_3 = 0;
                    v0.x = -nrm.x;
                    v0.y = zero;
                    v0.z = -nrm.z;
                    pl->ang.y += Muku3(pl->ang.y, &v0, PI);
                    return;
                }
            }
            v1.x = 0.0f;
            v1.y = 500.0f;
            v1.z = 0.0f;
            PSVECAdd(&v1, &pl->pos, &v1);
            v0.x = 1000.0f;
            v0.y = 0.0f;
            v0.z = 0.0f;
            RotVector(&v0, &pl->ang, &v0);
            PSVECAdd(&v0, &v1, &v0);
            if (SatMgr.hitCheck(&v1, &v0, 0, 0, 0, 0) == 0) {
                v1.x = 0.0f;
                v1.y = 0.0f;
                v1.z = 2000.0f;
                RotVector(&v1, &pl->ang, &v1);
                PSVECAdd(&v1, &v0, &v1);
                if (SatMgr.hitCheck(&v0, &v1, 0, 0, 0, 0) == 0) {
                    pl->m_Work0 = 1;
                    pl->m_Work1 = 1;
                    pl->r_no_2 = 6;
                    pl->r_no_3 = 0;
                    v0.x = -nrm.x;
                    v0.y = 0.0f;
                    v0.z = -nrm.z;
                    pl->ang.y += Muku3(pl->ang.y, &v0, PI);
                    return;
                }
            }
        }
        if (ckEmWep(pl)) {
            int side = ((Key.on >> 2) ^ 1) & 1;

            pl->m_Work1 = 0;
            pl->r_no_3 = 0;
            pl->r_no_2 = 6;
            pl->m_Work0 = side;
            return;
        }
    }
    pl->Wep->lockInit();
    m = WEP_ARC_PTR(0x11);
    mot3.set(pl, m, m, m, 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays with the draw SE at frame ~2 (0x29 while m_Work2 == 1, else
// 0x28); at frame 4 -> set state step 4 (finish the motion). Blends the pitch and straightens the waist.
static void wep17_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame > 1.7f && pl->Motion.Seq_frame < 2.3f) {
        int se;

        if (pl->m_Work2 == 1) {
            se = 0x29;
        } else {
            se = 0x28;
        }
        SndCall(1, se, &pl->getPartsPtr(0)->world, 0, 0, 0);
    }
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

// ready step 2: like step 1 without the SE; frame 4 -> set step 4.
static void wep17_r3_ready20(cPlayer* pl)
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

// ready step 3: the lock-on turn (PlWepLockCtrl's request): turn towards `tgt` (PI/8 per frame),
// slide towards `pos`, aim the pitch target at the target's elevation in 0.05 steps
// (clamped -1..1); motion end -> set state.
static void wep17_r3_ready30(cPlayer* pl)
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

// r_no_2 == 1: the set (aiming) state with the laser sight and (outside step 4) the lock-on
// control. Aim released -> wepDown (or crouch 0x11, which still falls through to the fire
// checks); fire trigger with rounds -> fire, empty -> reload (m_Flag bit0) or the empty-click SE
// 2/0x17; fire held with rounds -> fire; reload key -> reload (m_Work0 = 1).
static void wep17_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep17_r3_set00,
        wep17_r3_set10,
        wep17_r3_set20,
        wep17_r3_set30,
        wep17_r3_set40,
    };
    int fire;

    func_tbl[pl->r_no_3](pl);
    if (pl->r_no_3 != 4) {
        PlWepLockCtrl(pl);
    }
    pl->setLaserSight(1, 0);
    if (joyKamae() == 0) {
        if ((pl->stat & 0x40) == 0) {
            wepDown(pl);
            return;
        }
        pl->r_no_0 = 0;
        pl->r_no_2 = 0;
        pl->r_no_1 = 0x11;
        pl->r_no_3 = 0;
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
        pl->Wep->m_Flag |= 1;
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 4;
        pl->r_no_3 = 0;
        pl->m_Work0 = 1;
    }
}

// set step 0: start the three-way aim idle (0x12 down / 0x17 level / 0x19 up on m3r), reset
// the burst counter m_Work4, step 1.
static void wep17_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pWep;

    mot3.set(pl, PL_ARC_PTR(arc, 0x12), PL_ARC_PTR(arc, 0x17), PL_ARC_PTR(arc, 0x19), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->m_Work4 = 0;
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle.
static void wep17_r3_set10(cPlayer* pl)
{
    MotionMove(pl, 0);
}

// set step 2: a turn motion held while Key.on bit2 stays down (foot SEs at frames 10 and 23);
// released -> step 0. Set by PlWepLockCtrl's turn request.
static void wep17_r3_set20(cPlayer* pl)
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
static void wep17_r3_set30(cPlayer* pl)
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

// set step 4: finish the draw / fire motion; ends or any fire / aim / action key -> step 0.
static void wep17_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 shoots, step 1 plays the recoil / continues the burst) with
// the lock-on control.
static void wep17_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep17_r3_fire00,
        wep17_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
}

// fire step 0: the shot. trigger() spends a round; the fire motions by weapon (VP70 0x3D..0x3F,
// Red9 stock 0x14/0x18/0x1A, Red9 0x21..0x23) replace the idle, the waist twists to the lock yaw,
// the weapon's enemy collision is cleared, and the bullet line runs from the right hand (parts
// 10) muzzle offset (234.5, -24, 38.33) 50 m along -X with a +-200 spread -> PlWepHitCheck2.
// m_Work4 counts the burst rounds, weapon object mode 2, PlWepLockRand recoils the aim. Step 1.
static void wep17_r3_fire00(cPlayer* pl)
{
    cModel* parts;
    cObjWep* obj;
    void* m0;
    void* m1;
    void* m2;
    Vec p0;
    Vec p1;
    f32 pitch;

    WEP_OBJ(pl)->trigger();
    if (pG->weapon_no == 0x11) {
        m0 = WEP_ARC_PTR(0x3D);
        m1 = WEP_ARC_PTR(0x3E);
        m2 = WEP_ARC_PTR(0x3F);
    } else if (pG->weapon_type == 2) {
        m0 = WEP_ARC_PTR(0x14);
        m1 = WEP_ARC_PTR(0x18);
        m2 = WEP_ARC_PTR(0x1A);
    } else {
        m0 = WEP_ARC_PTR(0x21);
        m1 = WEP_ARC_PTR(0x22);
        m2 = WEP_ARC_PTR(0x23);
    }
    mot3.set(pl, m0, m1, m2, 0, 0, 0, 4, 0);
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
    pl->m_Work4++;
    obj = WEP_OBJ(pl);
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    pl->r_no_3 = 1;
}

// fire step 1: the recoil plays; the VP70 fires up to three shots on the held trigger (m_Work4
// counts them: back to step 0 at frame 3 while <= 2 and rounds remain). At frame 11 the burst
// counter resets, the enemy collision comes back and -> set state step 4.
static void wep17_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    if (joyKamae() && joyFireOn() && pG->weapon_no == 0x11 && MotionCheckCrossFrame(&pl->Motion, 3.0f)
        && pl->m_Work4 <= 2 && WEP_OBJ(pl)->bulletNum()) {
        pl->r_no_3 = 0;
    }
    if (MotionCheckCrossFrame(&pl->Motion, 11.0f)) {
        pl->m_Work4 = 0;
        WEP_ATARI(pl)->setFlag200();
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 4;
    }
}

// Holster: footwork routine (r_no_1 0) sub-routine 2 with the weapon-down motion 0x15 when a
// motion may be set (dmMotCk), else the idle with m_Hokan = 0xF; weapon object mode 3, its enemy
// collision cleared, the waist twist unwound into ang.y.
void wepDown(cPlayer* pl)
{
    cObjWep* obj;

    if (dmMotCk()) {
        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x15), 0, 3, 5, 0);
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
    obj->wep.mode = 3;
    obj->wep.step = 0;
    WEP_ATARI(pl)->clrFlag200();
    pl->ang.y = pl->ang.y - pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 4: the reload state. Step 0 starts the reload motion (VP70: 0x40/0x27/0x2A by reload
// tune level; Red9 stock 0x16; Red9 0x24), knifeStance = 1, weapon object mode 4 (it refills the
// magazine at its frame). Step 1: at the motion's end -> set state while aiming, else crouch 0x11
// or wepDown.
static void wep17_r2_reload(cPlayer* pl)
{
    u8 step = pl->r_no_3;
    cObjWep* obj;
    void* mot;

    switch (step) {
    case 0:
        if (pG->weapon_no == 0x11) {
            switch (pG->weapon_lv_reload) {
            default:
                mot = WEP_ARC_PTR(0x40);
                break;
            case 1:
                mot = WEP_ARC_PTR(0x27);
                break;
            case 2:
                mot = WEP_ARC_PTR(0x2A);
                break;
            }
        } else if (pG->weapon_type == 2) {
            mot = WEP_ARC_PTR(0x16);
        } else {
            mot = WEP_ARC_PTR(0x24);
        }
        MotionSetCore(pl, &pl->Motion, mot, 0, 3, 5, 0);
        MotionMove(pl, 0);
        pl->Wep->m_WepUd = 1;
        pl->r_no_3 = 1;
        obj = WEP_OBJ(pl);
        obj->wep.mode = 4;
        obj->wep.step = 0;
        break;
    case 1:
        if (MotionMove(pl, 0)) {
            if (joyKamae()) {
                pl->r_no_0 = 0;
                pl->r_no_1 = 6;
                pl->r_no_2 = 1;
                pl->r_no_3 = 0;
            } else if (pl->stat & 0x40) {
                pl->r_no_0 = 0;
                pl->r_no_2 = 0;
                pl->r_no_1 = 0x11;
                pl->r_no_3 = 0;
            } else {
                wepDown(pl);
            }
        }
        break;
    }
}

// r_no_2 == 5: the next-target state (Key.trg bit5): turn to the lock target m_pEm. Step 0: a
// target within the waist limit is turned to by the waist alone (m_Fwork0, step 2 -> set state
// step 1 when settled); farther round the body turns with a (missing, NULL) turn motion in step
// 1 (0.314 rad per frame beyond 200 units, 10 frames on m_Work0) then -> set state. Another press
// searches the next enemy from the head (SearchLockEm) and restarts, none -> set; aim released
// -> set state (or crouch 0x11).
static void wep17_r2_next(cPlayer* pl)
{
    u8 step = pl->r_no_3;
    cEm* em = pl->m_pEm;
    f32 ang;

    switch (step) {
    case 0:
        MotionMove(pl, 0);
        pl->m_Work0 = 0;
        pl->m_Work1 = 0;
        if (em) {
            ang = Muku(&pl->pos, &em->pos, pl->ang.y, PI);
        } else {
            ang = 0.0f;
        }
        if (ang < cPlWaist::ROT_LIMIT && ang > -cPlWaist::ROT_LIMIT) {
            pl->m_Fwork0 = ang;
            pl->r_no_3 = 2;
        } else {
            if (ang > 3.0f * PI / 4.0f || ang < -3.0f * PI / 4.0f) {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 1, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 1, 0);
            }
            pl->m_Work1 = 1;
            pl->r_no_3 = 1;
        }
        break;
    case 1:
        if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
            pl->ang.y += Muku(&pl->pos, &em->pos, pl->ang.y, 0.31415927f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        pl->Waist->set(0.0f, 0.4f);
        if (pl->m_Work1 == 0) {
            pl->partsFixMemory(0x19);
        }
        MotionMove(pl, 0);
        if ((int) pl->m_Work0 > 9) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
        }
        pl->m_Work0++;
        break;
    case 2:
        MotionMove(pl, 0);
        if (fabsf(pl->Waist->set(pl->m_Fwork0, 0.4f)) < 0.01f) {
            pl->Waist->m_Ang.y = pl->m_Fwork0;
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 1;
        }
        break;
    }
    if (Key.trg & 0x20) {
        Vec p = pl->getPartsPtr(3)->world;

        em = SearchLockEm(&p, pl->m_pEm);
        if (em) {
            pl->m_pEm = em;
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

// r_no_2 == 6: the "out" state: the player with his back to a wall (ready00) turns round / steps out
// before aiming; the motions of the steps are not in the archive (null motion pointers).
// m_Work0 is the side (0 left, 1 right), m_Work1 = 1 when a wall (not an enemy) started it. Step
// 0/1 the turn-out motion, facing pCkEm over its last frames; step 2 the cover aim (stick turns
// 3 degrees per frame, m_Fwork0 keeps the entry angle): fire held -> a shot from the hand with
// the flash 0x4B and the hit line, step 3 (recoil, back to 2); aim released -> step 6 (turn back
// to m_Fwork0, wall case) or step 5 (step back in, camera angle reset); both end in footwork idle
// with the weapon's enemy collision cleared.
static void wep17_r2_out(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0:
        if (pl->m_Work0 == 0) {
            MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
        } else {
            MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
        }
        pl->r_no_3 = 1;
    case 1:
        if ((int) pl->Motion.Seq_frame_num > (int) pl->Motion.Seq_frame_num - 7 && pCkEm) {
            pl->ang.y += Muku(&pl->pos, &pCkEm->pos, pl->ang.y, 0.31415927f);
        }
        if (MotionMove(pl, 0)) {
            if (pl->m_Work0 == 0) {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
            }
            pl->r_no_3 = 2;
            pl->m_Fwork0 = pl->ang.y;
        }
        break;
    case 2:
        if (Key.on & 4) {
            pl->ang.y -= 0.05235988f;
        }
        if (Key.on & 8) {
            pl->ang.y += 0.05235988f;
        }
        MotionMove(pl, 0);
        if (Key.trg & 0x40000000) {
            pl->m_Work1 = 0;
        }
        if (joyKamae() == 0) {
            if (pl->m_Work1 == 1) {
                if (pl->m_Work0 == 0) {
                    MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 5);
                } else {
                    MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 5);
                }
                pl->r_no_3 = 6;
                pl->ang.y = pl->m_Fwork0;
            } else {
                if (pl->m_Work0 == 0) {
                    MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
                } else {
                    MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
                }
                pl->r_no_3 = 5;
            }
        } else if (joyFireOn()) {
            cModel* parts;
            Vec p0;
            Vec p1;
            int zero;

            zero = 0;
            StaFlagOn(pG, STA_PL_FIRE);
            SndCall(2, 0, &pl->getPartsPtr(4)->world, 0, 0, 0);
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
            EstSet(WEP_OBJ(pl), -1, 0, 0, EFF_WEP17, 0, 0, ESP_CORE_KIND_PL_WEP, (void*) zero, 0);
            parts = pl->getPartsPtr(0xA);
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
            if (pl->m_Work0 == 0) {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
            }
            pl->r_no_3 = 3;
        }
        break;
    case 3: {
        int end;

        if (pl->Motion.Seq_frame >= 5.0f) {
            if (Key.on & 4) {
                pl->ang.y -= 0.05235988f;
            }
            if (Key.on & 8) {
                pl->ang.y += 0.05235988f;
            }
        }
        end = MotionMove(pl, 0);
        if (pl->Motion.Seq_frame > (f32) (pl->Motion.Seq_frame_num - 5) && joyFireOn()) {
            end |= 1;
        }
        if (end) {
            if (pl->m_Work0 == 0) {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
            } else {
                MotionSetCore(pl, &pl->Motion, 0, 0, 3, 5, 0);
            }
            pl->r_no_3 = 2;
        }
        break;
    }
    case 5:
        CamCtrl.resetCameraAngle();
        if ((Key.on & 0x10F) || MotionMove(pl, 0)) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
            WEP_ATARI(pl)->clrFlag200();
        }
        break;
    case 6:
        if ((int) pl->Motion.Seq_frame_num > (int) pl->Motion.Seq_frame_num - 7 && pCkEm) {
            pl->ang.y += Muku(&pl->pos, &pCkEm->pos, pl->ang.y, 0.31415927f);
        }
        if (MotionMove(pl, 0)) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
            WEP_ATARI(pl)->clrFlag200();
        }
        break;
    }
}

// Creates the weapon object of the equipped weapon (weapon_no 3 Red9 -> cObjMauser id 0x27,
// 0x11 -> cObjVp70 0x34) and inits it on the player; NULL when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj;
    int id;

    switch (pG->weapon_no) {
    case 3:
    default:
        id = 0x27;
        break;
    case 0x11:
        id = 0x34;
        break;
    }
    obj = (cObjWep*) ObjMgr.createBack(id);
    if (obj == 0) {
        pLog->err(0, 0, "Wep17_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

// REL entry: registers the weapon init / move routines and both object constructor slots (the
// Mauser's constructor is imported from the wep02 module).
extern "C" void _prolog()
{
    WeaponInitFunc = Wep17_init;
    WeaponMoveFunc = Wep17_move;
    ObjInitFunc[0x27] = ObjMauser_init;
    ObjInitFunc[0x34] = ObjVp70_init;
    OSReport("Wep17 VP70 prolog Ok\n");
}

// REL exit: frees the object constructor slots.
extern "C" void _epilog()
{
    ObjInitFunc[0x27] = 0;
    ObjInitFunc[0x34] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
