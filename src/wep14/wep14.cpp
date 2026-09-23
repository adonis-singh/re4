// wep14 module: the mine thrower (weapon number 0x14, cObjMine object id 0x36; wep14/objMine.cpp).
// The module has its own player routine (the handgun routine of wep/pl_handgun.cpp with the
// mine thrower's aim types: type 0 fires from the hand, the scope type (wep_type bit0) aims
// through the camera trajectory and swaps the right hand model between the ready/reload motions).
//
// Entry: Wep14_init is the WeaponInitFunc, Wep14_move the WeaponMoveFunc (pl_R1_Weapon, r_no_1
// == 6). r_no_2 is the weapon state (0 ready, 1 set, 2 fire, 3 down, 4 reload, 5 next target),
// r_no_3 the step, mirrored into the cObjMine's r_no_0 / r_no_1 (objMine.cpp plays the
// launcher's own motions and launches the dart in mode 2). Weapon archive slots: 0x12 draw /
// turn, 0x13/0x17/0x19 aim idle down/level/up (mot3 pitch on m3r), 0x14/0x18/0x1A fire, 0x15
// holster, 0x16/0x1B reload by weapon_lv_reload, 0x9/0xA the two right-hand models.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "pad.h"
#include "snd.h"
#include "math_sub.h"

// Scalar-reference stores (the following pG load stays below them).

void ObjMine_init(cObj* obj);   // wep14/objMine.cpp

void Wep14_move(cPlayer* pl);
cObjWep* equipWeapon(cPlayer* pl);
void wep14changeRightHand(cPlayer* pl, void* hand);
static void wep14_r2_ready(cPlayer* pl);
static void wep14_r3_ready00(cPlayer* pl);
static void wep14_r3_ready10(cPlayer* pl);
static void wep14_r3_ready20(cPlayer* pl);
static void wep14_r3_ready30(cPlayer* pl);
static void wep14_r2_set(cPlayer* pl);
static void wep14_r3_set00(cPlayer* pl);
static void wep14_r3_set10(cPlayer* pl);
static void wep14_r3_set20(cPlayer* pl);
static void wep14_r3_set30(cPlayer* pl);
static void wep14_r3_set40(cPlayer* pl);
static void wep14_r2_fire(cPlayer* pl);
static void wep14_r3_fire00(cPlayer* pl);
static void wep14_r3_fire10(cPlayer* pl);
static void wep14_r2_down(cPlayer* pl);
static void wep14_r2_reload(cPlayer* pl);
static void wep14_r2_next(cPlayer* pl);

u8 lockCtr = 0;

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos;
static Vec tgt;

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjMine as Wep->m_pWep,
// installs its motions, loads the effects (archive 0x6 as group 0x48), sets the carrying right
// hand (0x9) and left hand 4, and points the debug preview PlWepMot at the aim idles.
void Wep14_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep14_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x6), EFF_WEP14, 1);
        wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
        pl->setLeftHand(4);
        PlWepMot[0] = WEP_ARC_PTR(0x13);
        PlWepMot[1] = WEP_ARC_PTR(0x17);
        PlWepMot[2] = WEP_ARC_PTR(0x19);
    }
}

// WeaponMoveFunc: dispatches on r_no_2 (0..5) and runs the lock-on stick control.
void Wep14_move(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r2_ready,
        wep14_r2_set,
        wep14_r2_fire,
        wep14_r2_down,
        wep14_r2_reload,
        wep14_r2_next,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
}

// r_no_2 == 0: the ready (draw) state. Aim key released before the lock turn (step 3) -> footwork
// (or crouch 0x11) with the launcher object in mode 3 (lower) and the carrying hand 0x9; reload key
// with darts left -> reload (m_Work0 = 1); else the shoulder camera aims at the locked enemy or
// the forward scenery hit.
static void wep14_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r3_ready00,
        wep14_r3_ready10,
        wep14_r3_ready20,
        wep14_r3_ready30,
    };

    func_tbl[pl->r_no_3](pl);
    if (joyKamae() == 0 && pl->r_no_3 != 3) {
        if (pl->stat.check(cPlayer::F_CROUCH)) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            cObjWep* obj;

            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
            obj = WEP_OBJ(pl);
            obj->r_no_0 = 3;
            obj->r_no_1 = 0;
        }
        wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
    }
    if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
        wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
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

// ready step 0: enter the aim: Wep->pitch from the camera pitch (doubled looking up), aim yaw
// m_Fwork0 = 0, camera direction saved in m_CamAdjY, the aiming right hand 0xA, neck / lock-on
// reset, draw motion 0x12 (blend 4 frames; the normal type adds flag 0x100), m3r zeroed, lockCtr
// = 0, launcher object mode 1 (raise + load).
static void wep14_r3_ready00(cPlayer* pl)
{
    const f32 zero = 0.0f;
    cObjWep* obj;
    f32 pitch;
    void* m;
    int normal;
    int hokan;

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
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    wep14changeRightHand(pl, WEP_ARC_PTR(0xA));
    pl->Neck->init(0, 0, 0);
    pl->Wep->lockInit();
    hokan = 4;
    normal = !(pG->weapon_type & 1);
    if (normal) {
        hokan = 0x104;
    }
    m = WEP_ARC_PTR(0x12);
    mot3.set(pl, m, m, m, 0, 0, 0, hokan, 0);
    mot3.move(m3r);
    pl->motionMove();
    m3r.reset(zero);
    m3r.setDelay(zero);
    lockCtr = 0;
    obj = WEP_OBJ(pl);
    obj->r_no_1 = 0;
    obj->r_no_0 = 1;
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays while ang.y turns to the camera direction over 4 frames; at its
// end the load SE 2/9 and -> set state.
static void wep14_r3_ready10(cPlayer* pl)
{
    if (pl->Motion.Seq_frame < 4.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->Motion.Seq_frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
    if (pl->motionMove()) {
        SndCall(2, 9, &pl->getPartsPtr(0xA)->world, 0, 0, 0);
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
}

// ready step 2: finish a motion set by the lock-on turn, then SE 5/0 and -> set state.
static void wep14_r3_ready20(cPlayer* pl)
{
    if (pl->motionMove()) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        EmRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(0.0f, 0.4f);
}

// ready step 3: the lock-on turn (PlWepLockCtrl's request): turn towards `tgt` (PI/8 per frame),
// slide towards `pos`, aim the pitch target at the target's elevation in 0.05 steps
// (clamped -1..1); motion end -> set state.
static void wep14_r3_ready30(cPlayer* pl)
{
    f32 dist;
    f32 x;
    f64 a;
    Vec* t;

    if (pl->motionMove()) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->world, 0, 0, 0);
        EmRoutineSet(pl, 0, 6, 1, 0);
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

// r_no_2 == 1: the set (aiming) state: laser sight for the normal type only (the scope type looks
// through the camera). Aim released -> down (r_no_2 3, or crouch 0x11); fire trigger / held with
// darts -> fire; trigger on an empty launcher -> reload (m_Flag bit0) or the empty SE 2/3; reload
// key -> reload (m_Work0 = 1).
static void wep14_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r3_set00,
        wep14_r3_set10,
        wep14_r3_set20,
        wep14_r3_set30,
        wep14_r3_set40,
    };
    int fire;
    int normal;

    func_tbl[pl->r_no_3](pl);
    normal = !(pG->weapon_type & 1);
    if (normal) {
        pl->setLaserSight(1, 0);
    } else {
        pl->setLaserSight(0, 0);
    }
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
    } else if ((fire = joyFireTrg())) {
        fire = WEP_OBJ(pl)->bulletNum();
        if (fire) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 2;
            pl->r_no_3 = 0;
        } else if (WEP_OBJ(pl)->reloadable()) {
            pl->Wep->m_Flag |= 1;
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 4;
            pl->r_no_3 = 0;
            pl->m_Work0 = fire;
        } else {
            SndCall(2, 3, &pl->getPartsPtr(4)->world, 0, 0, 0);
            goto reload;
        }
    } else if (joyFireOn() && WEP_OBJ(pl)->bulletNum()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    } else {
    reload:
        if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 4;
            pl->r_no_3 = 0;
            pl->m_Work0 = 1;
        }
    }
}

// set step 0: the scope type switches the camera to the scope (stat bit4); start the
// three-way aim idle (0x13/0x17/0x19 on the pitch), step 1.
static void wep14_r3_set00(cPlayer* pl)
{
    PlArc* arc;

    if (pG->weapon_type & 1) {
        CamCtrl.startScope(0, 0);
        CameraMove();
        pl->stat.on(cPlayer::F_SCOPE);
    }
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x13), PL_ARC_PTR(arc, 0x17), PL_ARC_PTR(arc, 0x19), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the aim idle with the lock-on control.
static void wep14_r3_set10(cPlayer* pl)
{
    PlWepLockCtrl(pl);
    pl->motionMove();
}

// set step 2: a turn motion held while Key.on bit2 stays down; released -> step 0.
static void wep14_r3_set20(cPlayer* pl)
{
    if ((Key.on & 4) == 0) {
        pl->r_no_3 = 0;
    }
    MotionMove(pl, 0);
}

// set step 3: the same for the other turn direction (Key.on bit3).
static void wep14_r3_set30(cPlayer* pl)
{
    if ((Key.on & 8) == 0) {
        pl->r_no_3 = 0;
    }
    pl->motionMove();
}

// set step 4: finish the current motion; ends or any fire / aim / action key -> step 0.
static void wep14_r3_set40(cPlayer* pl)
{
    if (MotionMove(pl, 0) || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 launches, step 1 plays the reload-a-dart motion).
static void wep14_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r3_fire00,
        wep14_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
}

// fire step 0: trigger() spends the dart and the launcher object (mode 2, cObjMine::moveFire)
// launches it, so there is no hit line here; the fire motions 0x14/0x18/0x1A start, PlWepLockRand
// kicks the aim, stat bit5 (hand swap) is cleared. Step 1.
static void wep14_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    cObjWep* obj;
    f32 pitch;

    WEP_OBJ(pl)->trigger();
    arc = pG->pWep;
    mot3.set(pl, PL_ARC_PTR(arc, 0x14), PL_ARC_PTR(arc, 0x18), PL_ARC_PTR(arc, 0x1A), 0, 0, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    obj = WEP_OBJ(pl);
    obj->r_no_0 = 2;
    obj->r_no_1 = 0;
    pitch = m3r;
    PlWepLockRand(pl, 2, &pitch, &pl->m_Fwork0);
    m3r = pitch;
    pl->r_no_3 = 1;
    pl->stat.off(cPlayer::F_SP_L_HAND);
}

// fire step 1: the fire motion plays; the left hand model is swapped to 5 (holding the next dart,
// stat bit5) from frame 23 (SE 2/4) to frame 30; motion end -> set state.
static void wep14_r3_fire10(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->Motion, 23.0f)) {
        SndCall(2, 4, &pl->getPartsPtr(4)->world, 0, 0, 0);
        pl->stat.on(cPlayer::F_SP_L_HAND);
        pl->setLeftHand(5);
    }
    if (MotionCheckCrossFrame(&pl->Motion, 30.0f)) {
        pl->stat.off(cPlayer::F_SP_L_HAND);
        pl->setLeftHand(4);
    }
    if (pl->motionMove()) {
        pl->r_no_0 = 0;
        pl->r_no_1 = 6;
        pl->r_no_2 = 1;
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 3: the down (holster) state, one frame: the scope type ends the scope camera; the
// holster motion 0x15 into footwork sub-routine 2 when a motion may be set, else the idle with
// m_Hokan = 0xF; launcher object mode 3, its enemy collision (atari 0x200) cleared, the carrying
// right hand 0x9, the waist twist unwound into ang.y.
static void wep14_r2_down(cPlayer* pl)
{
    cObjWep* obj;

    if (pG->weapon_type & 1) {
        CamCtrl.endScope();
        CameraMove();
        pl->stat.off(cPlayer::F_SCOPE);
    }
    if (dmMotCk()) {
        pl->motionSet(WEP_ARC_PTR(0x15), 7, 0, 1, 0);
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
    {
        int md = 3;

        obj = WEP_OBJ(pl);
        obj->r_no_0 = md;
        obj->r_no_1 = 0;
    }
    AtariFlagsAndV(WEP_ATARI(pl), 0xFDFF);
    wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
    pl->ang.y = pl->ang.y - pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 4: the reload state. Step 0 ends the scope camera (scope type), starts the reload
// motion of the tune level (0x16 / 0x1B), the aiming right hand 0xA and launcher object mode 4
// (it refills at its frame); step 1 waits for the motion's end, restores the scope and -> set state.
static void wep14_r2_reload(cPlayer* pl)
{
    u8 step = pl->r_no_3;
    cObjWep* obj;
    void* mot;

    switch (step) {
    case 0:
        if (pG->weapon_type & 1) {
            pl->endCamera();
        }
        switch (pG->weapon_lv_reload) {
        default:
            mot = WEP_ARC_PTR(0x16);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x1B);
            break;
        }
        MotionSetCore(pl, &pl->Motion, mot, 0, 3, 5, 0);
        wep14changeRightHand(pl, WEP_ARC_PTR(0xA));
        pl->r_no_3 = 1;
        obj = WEP_OBJ(pl);
        obj->r_no_0 = 4;
        obj->r_no_1 = 0;
    case 1:
        if (pl->motionMove()) {
            if (pG->weapon_type & 1) {
                CamCtrl.startScope(0, 0);
                CameraMove();
                pl->stat.on(cPlayer::F_SCOPE);
            }
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
        }
        break;
    }
}

// r_no_2 == 5: the next-target state (Key.trg bit5 in the lock control): the turn motion 0x12
// while turning towards the locked enemy m_pEm (0.314 rad per frame beyond 200 units) for 10
// frames (m_Work0), then -> set state. Another press cycles lockNext() (new target restarts, none
// -> set); aim released -> set state (or crouch 0x11).
static void wep14_r2_next(cPlayer* pl)
{
    u8 step = pl->r_no_3;
    cModel* em = pl->m_pEm;

    switch (step) {
    case 0:
        pl->m_Work0 = 0;
        pl->m_Work1 = 0;
        MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x12), 0, 0xA, 1, 0);
        pl->r_no_3 = 1;
    case 1:
        if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
            pl->ang.y += Muku(&pl->pos, &em->pos, pl->ang.y, 0.31415927f);
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
        pl->m_Fwork0 = 0.0f;
        pl->Waist->set(0.0f, 0.4f);
        pl->Body->waistMove();
        pl->motionMove();
        if ((int) pl->m_Work0++ > 9) {
            EmRoutineSet(pl, 0, 6, 1, 0);
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

            EmRoutineSet(pl, 0, 6, md, 0);
        }
    }
}

// Creates the cObjMine (ObjMgr id 0x36) and inits it on the player; NULL when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_MINE);
    if (obj == 0) {
        pLog->err(0, 0, "Wep14_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

// Right hand model swap: the mine thrower's hand data by motion (ready / reload).
void wep14changeRightHand(cPlayer* pl, void* hand)
{
    pl->setRightHand(0);
    pl->Body->initWepHand((u32) hand);
    pl->setRightHand(1);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep14_init;
    WeaponMoveFunc = Wep14_move;
    ObjInitFunc[0x36] = ObjMine_init;
    OSReport("Wep14 MINE-THROWER prolog Ok\n");
}

// REL exit: nothing to free (the ObjInitFunc slot is left set).
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}

// The module's .data section is 8-aligned (the original linker's placement; the tables start at 4).
asm(".section .data\n\t.balign 8\n\t.text");
