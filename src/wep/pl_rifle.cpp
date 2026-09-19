// Rifle player routines ("D:/Bio4/Prog/pl_rifle.cpp"; wep09/wep10/wep40/wep47 modules, first routine
// object): routine 2 of the player while a rifle is equipped: ready, set (scope camera), fire, down,
// reload, next target. Modelled on game/pl_knife.cpp.
//
// Entry: PlRifleMove is the module's WeaponMoveFunc (pl_R1_Weapon, r_no_1 == 6). r_no_2 is the
// weapon state (0 ready, 1 set = looking through the scope, 2 fire, 3 down, 4 reload, 5 next
// target), r_no_3 the step. The scope is a camera mode (CamCtrl.startScope / endCamera) with the
// thermal light set for the infrared scope; the aim is the camera trajectory, so there is no
// mot3 pitch blend while scoped (m3r is only reset). weapon_no 9 is the bolt-action rifle (a
// bolt cycle, fire steps 2/3, after every shot), 0xA the semi-auto rifle (the weapon object plays
// the recoil). Weapon archive slots: 0x14 draw, 0x15 scope idle, 0x16/0x19/0x1A holster from
// the scope, 0x17/0x1D/0x1E reload by level, 0x1B bolt cycle, 0x1C holster after a reload.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "snd.h"
#include "pad.h"
#include "math_sub.h"

// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");

#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWep, no)

// Store through a scalar reference: the following global load stays below it.
static inline void ISet(int& d, int v) { d = v; }

// Routine bytes through int parameters (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->r_no_0 = r0;
    pl->r_no_1 = r1;
    pl->r_no_2 = r2;
    pl->r_no_3 = r3;
}

// Scope camera on: the thermal light set for the infrared scope (weapon type 2 / weapon 0x1D).
static inline void scopeOn(cPlayer* pl)
{
    BitOn(pl->stat, 0x10);
    if (pG->weapon_type == 2 || pG->weapon_no == 0x1D) {
        StaFlagOn(pG, STA_THERMO_GRAPH);
        pl->stat |= 0x200;
        LightMgr.setThermo();
    }
}

static void wep09_r2_ready(cPlayer* pl);
static void wep09_r3_ready00(cPlayer* pl);
static void wep09_r3_ready10(cPlayer* pl);
static void wep09_r2_set(cPlayer* pl);
static void wep09_r3_set00(cPlayer* pl);
static void wep09_r3_set10(cPlayer* pl);
static void wep09_r3_set20(cPlayer* pl);
static void wep09_r2_fire(cPlayer* pl);
static void wep09_r3_fire00(cPlayer* pl);
static void wep09_r3_fire10(cPlayer* pl);
static void wep09_r3_fire20(cPlayer* pl);
static void wep09_r3_fire30(cPlayer* pl);
static void wepDown(cPlayer* pl);
static void wep09_r2_reload(cPlayer* pl);
static void wep09_r2_next(cPlayer* pl);

// WeaponMoveFunc of the rifle modules: dispatches on r_no_2 (0..5) and runs the lock-on stick control.
void PlRifleMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r2_ready,
        wep09_r2_set,
        wep09_r2_fire,
        wepDown,
        wep09_r2_reload,
        wep09_r2_next,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
}

// r_no_2 == 0: the ready (draw) state. Aim key released -> footwork (r_no_1 0, or 0x11 crouch with
// stat bit6); else the shoulder camera aims at the locked enemy or the forward scenery hit,
// and the reload key with rounds left starts the reload (weapon display type 1 on, m_Work0 = 1).
static void wep09_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r3_ready00,
        wep09_r3_ready10,
    };

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
    } else {
        if (pl->m_pEm) {
            CamCtrlShoulderSetAim(&pl->m_pEm->pos);
        } else {
            Vec aim = {0.0f, 1000.0f, 10000.0f};
            Vec hit;

            PSMTXMultVec(pl->mat, &aim, &aim);
            SatMgr.hitCheck(&pl->getPartsPtr(0)->world, &aim, &hit, 0, 0, 0);
            CamCtrlShoulderSetAim(&hit);
        }
        if (pl->keyReload() && pl->Wep->m_pWep->reloadable()) {
            pl->Wep->m_Flag |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->Wep->m_pWep->setDisp(1, 1);
            pl->m_Work0 = 1;
        }
    }
}

// ready step 0: enter the aim: pitch 0, camera direction saved in m_CamAdjY, neck / lock-on reset,
// the draw motion 0x14 started; m_Work1 (pump SE done) and m_Work4 (fire delay) cleared.
static void wep09_r3_ready00(cPlayer* pl)
{
    void* mot;
    cPlWep* w = pl->Wep;

    w->m_CenterY = 0.0f;
    w->pitch = 0.0f;
    pl->Wep->m_CamAdjY = CamCtrl.getCameraDirection();
    pl->Neck->init(0, 0, 0);
    pl->Wep->lockInit();
    mot = WEP_ARC_PTR(0x14);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->r_no_3 = 1;
    pl->m_Work1 = 0;
    pl->m_Work4 = 0;
}

// ready step 1: the draw plays while ang.y is turned to the camera direction over the first 4
// frames; at its end -> set state (the scope) with a 10-frame fire delay (m_Work4).
static void wep09_r3_ready10(cPlayer* pl)
{
    if (pl->frame < 4.0f) {
        f32 d = pl->Wep->m_CamAdjY / (4.0f - pl->frame);

        pl->ang.y += d;
        pl->Wep->m_CamAdjY -= d;
    }
    if (pl->motionMove()) {
        PlRoutineSet(pl, 0, 6, 1, 0);
        pl->m_Work4 = 10;
    }
    pl->Waist->set(0.0f, 0.4f);
}

// r_no_2 == 1: the set state = looking through the scope. No laser sight; the bolt SE 2/9 once at
// frame 2 (m_Work1), the fire delay m_Work4 counts down. Aim key released -> down (r_no_2 3, or
// crouch 0x11) with the scope direction stored in m_VecWork0 (wepDown restores the body pitch from
// it); fire held after the delay with rounds -> fire; trigger on an empty rifle -> reload or the
// empty-click SE 2/3; reload key -> reload (m_Work0 = 1: manual).
static void wep09_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r3_set00,
        wep09_r3_set10,
        wep09_r3_set20,
    };

    func_tbl[pl->r_no_3](pl);
    pl->setLaserSight(0, 0);
    if (pl->m_Work1 == 0 && MotionCheckCrossFrame(&pl->Motion, 2.0f)) {
        SndCall(2, 9, &pl->pParts->world, 0, 0, 0);
        pl->m_Work1 = 1;
    }
    if (pl->m_Work4 != 0) {
        pl->m_Work4--;
    }
    if (joyKamae() == 0) {
        Vec at;

        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            int md = 3;

            PlRoutineSet(pl, 0, 6, md, 0);
        }
        pl->m_Work0 = 0;
        CamCtrl.getTrajectory(&pl->m_VecWork0, &at);
        PSVECSubtract(&at, &pl->m_VecWork0, &pl->m_VecWork0);
    } else if (joyFireOn() && pl->m_Work4 == 0 && pl->Wep->m_pWep->bulletNum()) {
        PlRoutineSet(pl, 0, 6, 2, 0);
    } else if (joyFireTrg() && pl->Wep->m_pWep->bulletNum() == 0) {
        if (pl->Wep->m_pWep->reloadable()) {
            pl->Wep->m_Flag |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->Wep->m_pWep->setDisp(1, 1);
            pl->m_Work0 = 0;
        } else {
            SndCall(2, 3, &pl->getPartsPtr(4)->world, 0, 0, 0);
            goto reload;
        }
    } else {
    reload:
        if (pl->keyReload() && pl->Wep->m_pWep->reloadable()) {
            pl->Wep->m_pWep->setDisp(1, 1);
            pl->Wep->m_Flag |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->m_Work0 = 1;
        }
    }
}

// set step 0: switch the camera to the scope (stat bit4, thermal light set for the infrared
// scope) and hold the scope idle motion 0x15; step 1.
static void wep09_r3_set00(cPlayer* pl)
{
    CamCtrl.startScope(0, 0);
    CameraMove();
    scopeOn(pl);
    pl->motionSet(WEP_ARC_PTR(0x15), 5, 0, 1, 0);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the scope idle.
static void wep09_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// set step 2 (entered from the reload's end): finish the reload motion, then step 0.
static void wep09_r3_set20(cPlayer* pl)
{
    if (pl->motionMove()) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state: step 0 shoots, 1 waits the shot interval, 2/3 cycle the bolt
// (bolt-action rifle only).
static void wep09_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r3_fire00,
        wep09_r3_fire10,
        wep09_r3_fire20,
        wep09_r3_fire30,
        0,
    };

    func_tbl[pl->r_no_3](pl);
}

// fire step 0: the shot. trigger() spends the round; the bullet line is the scope camera's
// trajectory (extended to 200 m for the semi-auto) -> PlWepHitCheck2; the bolt-action plays the
// shot SE 2/0 itself, the semi-auto's weapon object goes to mode 2 (recoil). Controller vibration
// from the player archive, weapon display type 1 off (the scope glass), the scope direction is
// kept in m_VecWork0. Then step 1.
static void wep09_r3_fire00(cPlayer* pl)
{
    Vec from;
    Vec to;
    Vec dir;
    void* mot;
    cObjWep* obj;

    pl->Wep->m_pWep->trigger();
    mot = WEP_ARC_PTR(0x15);
    mot3.set(pl, mot, mot, mot, 0, 0, 0, 4, 0);
    pl->motionMove();
    CamCtrl.getTrajectory(&from, &to);
    if (pG->weapon_no == 0xA) {
        PSVECSubtract(&to, &from, &dir);
#line 406 "D:/Bio4/Prog/pl_rifle.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, 200000.0f);
        PSVECAdd(&from, &dir, &to);
    }
    PlWepHitCheck2(pl, &from, &to, pG->weapon_no, 0, 6000.0f);
    if (pG->weapon_no == 9) {
        SndCall(2, 0, &pl->getPartsPtr(4)->world, 0, 0, 0);
    }
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
    ISet(pl->m_Work4, 0);
    if (pG->weapon_no == 0xA) {
        obj = pl->Wep->m_pWep;
        obj->wep.mode = 2;
        obj->wep.step = 0;
    }
    pl->Wep->m_pWep->setDisp(1, 0);
    CamCtrl.getTrajectory(&pl->m_VecWork0, &dir);
    PSVECSubtract(&dir, &pl->m_VecWork0, &pl->m_VecWork0);
    pl->r_no_3 = 1;
}

// fire step 1: m_Work4 counts the frames since the shot; past the weapon's shot frame
// (PlShotFrameTbl by fire-speed level): the bolt-action with rounds left -> step 2 (bolt cycle),
// otherwise back to the scope (set state) with a 10-frame fire delay. The semi-auto may lower the
// rifle (down state) after 10 frames when the aim key is released.
static void wep09_r3_fire10(cPlayer* pl)
{
    cPlWep* w;

    pl->motionMove();
    ISet(pl->m_Work4, pl->m_Work4 + 1);   // reference store: the pG load stays below it
    w = pl->Wep;
    if (pl->m_Work4 > (u8) PlShotFrameTbl[pG->weapon_no][pG->weapon_lv_speed]) {
        if (pG->weapon_no != 9 || w->m_pWep->bulletNum() == 0) {
            PlRoutineSet(pl, 0, 6, 1, 0);
            pl->m_Work4 = 10;
        } else {
            pl->r_no_3 = 2;
        }
    } else if (pG->weapon_no != 9 && pl->m_Work4 > 10 && joyKamae() == 0) {
        PlRoutineSet(pl, 0, 6, 3, 0);
        pl->m_Work0 = 0;
    }
}

// fire step 2 (bolt-action): leave the scope (saveScopeParam so it can be restored), show the
// scope glass again and start the bolt-cycle motion 0x1B; the weapon object plays its own (mode 2).
static void wep09_r3_fire20(cPlayer* pl)
{
    cObjWep* obj;

    m3r[1] = 0.0f;
    m3r[0] = 0.0f;
    CamCtrl.saveScopeParam();
    pl->endCamera();
    pl->Wep->m_pWep->setDisp(1, 1);
    MotionSetCore(pl, &pl->Motion, WEP_ARC_PTR(0x1B), 0, 3, 5, 0);
    pl->motionMove();
    obj = pl->Wep->m_pWep;
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pl->r_no_3 = 3;
}

// fire step 3 (bolt-action): the bolt cycle plays. Aim released from frame 25 -> down state
// (m_Work0 = 1: holster after a reload-type motion; or crouch 0x11). At the motion end: still
// aiming -> back into the scope (loadScopeParam) and the set state, else the down state.
static void wep09_r3_fire30(cPlayer* pl)
{
    if (joyKamae() == 0 && pl->frame >= 25.0f) {
        if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            // store order brute-forced (x3E0 first, xFD last, the 3 through an int local)
            int md = 3;

            pl->m_Work0 = 1;
            pl->r_no_0 = 0;
            pl->r_no_2 = md;
            pl->r_no_3 = 0;
            pl->r_no_1 = 6;
        }
    } else if (pl->motionMove()) {
        if (joyKamae()) {
            CamCtrl.startScope(0, 0);
            CamCtrl.loadScopeParam();
            CameraMove();
            scopeOn(pl);
            PlRoutineSet(pl, 0, 6, 1, 0);
        } else if (pl->stat & 0x40) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else {
            // store order brute-forced (x3E0 first, xFD last, the 3 through an int local)
            int md = 3;

            pl->m_Work0 = 1;
            pl->r_no_0 = 0;
            pl->r_no_2 = md;
            pl->r_no_3 = 0;
            pl->r_no_1 = 6;
        }
    }
}

// r_no_2 == 3: the down (holster) state, one frame. Ends the scope camera, seeds the mot3 pitch
// from the elevation of the stored scope direction m_VecWork0, shows the scope glass, then the
// holster motion when one may be set: the semi-auto's 0x16, the bolt-action's pitched 0x16/0x19/
// 0x1A from the scope (m_Work0 == 0) or 0x1C after a bolt cycle / reload (the weapon object gets
// its stay motion 0x24 and mode 0). Leaves to footwork sub-routine 2 (or the idle with m_Hokan = 0xF).
static void wepDown(cPlayer* pl)
{
    f32 e;

    pl->endCamera();
    e = VecElevation(&pl->m_VecWork0);
    m3r[0] = e;
    m3r[1] = e;
    m3r[2] = 0.0f;
    pl->Wep->m_pWep->setDisp(1, 1);
    if (dmMotCk()) {
        if (pG->weapon_no == 0xA) {
            void* mot = WEP_ARC_PTR(0x16);

            mot3.set(pl, mot, mot, mot, 0, 5, 0, 4, 0);
        } else if (pl->m_Work0 == 0) {
            PlArc* arc = (PlArc*) pG->pWep;

            mot3.set(pl, PL_ARC_PTR(arc, 0x16), PL_ARC_PTR(arc, 0x19), PL_ARC_PTR(arc, 0x1A), 0, 0, 0, 4, 0);
        } else {
            void* mot = WEP_ARC_PTR(0x1C);
            cObjWep* obj;

            mot3.set(pl, mot, mot, mot, 0, 0, 0, 4, 0);
            obj = pl->Wep->m_pWep;
            obj->motionSet(WEP_ARC_PTR(0x24), 0, 0, 1, 0);
            obj->wep.mode = 0;
            obj->wep.step = 0;
        }
        mot3.move(m3r[0]);
        PlRoutineSet(pl, 0, 0, 2, 0);
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

// r_no_2 == 4: the reload state. Step 0 ends the scope camera and starts the reload motion of the
// reload-speed level (0x17/0x1D/0x1E), knifeStance = 1, weapon object mode 4. Step 1: aim released
// past PlReloadEndTbl's frame -> down state (m_Work0 = 1) or crouch 0x11; 5 frames before the end
// the scope comes back on (bolt SE 2/9) and the set state finishes the motion in its step 2.
static void wep09_r2_reload(cPlayer* pl)
{
    switch (pl->r_no_3) {
    case 0: {
        void* mot;
        cObjWep* obj;

        m3r[1] = 0.0f;
        m3r[0] = 0.0f;
        pl->endCamera();
        switch (pG->weapon_lv_reload) {
        default:
            mot = WEP_ARC_PTR(0x17);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x1D);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x1E);
            break;
        }
        MotionSetCore(pl, &pl->Motion, mot, 0, 3, 5, 0);
        pl->motionMove();
        pl->Wep->knifeStance = 1;
        pl->r_no_3 = 1;
        obj = pl->Wep->m_pWep;
        obj->wep.mode = 4;
        obj->wep.step = 0;
        break;
    }
    case 1:
        if (joyKamae() == 0 && pl->Motion.Mot_frame >= PlReloadEndTbl[pG->weapon_no][pG->weapon_lv_reload]) {
            if (pl->stat & 0x40) {
                pl->r_no_0 = 0;
                pl->r_no_2 = 0;
                pl->r_no_1 = 0x11;
                pl->r_no_3 = 0;
            } else {
                int md = 3;

                PlRoutineSet(pl, 0, 6, md, 0);
                pl->m_Work0 = 1;
            }
        }
        if (pl->frame >= (f32) (pl->frameMax - 5)) {
            CamCtrl.startScope(0, 0);
            CameraMove();
            scopeOn(pl);
            SndCall(2, 9, &pl->pParts->world, 0, 0, 0);
            PlRoutineSet(pl, 0, 6, 1, 2);
            pl->m_Work4 = 10;
        }
        MotionMoveI(pl, 0);
        break;
    }
}

// r_no_2 == 5: the next-target state (entered by the lock control on Key.trg bit5): turns the
// player towards the locked enemy m_pEm (0.314 rad per frame, when farther than 200 units) for
// 10 frames (m_Work0), then back to the scope. Another press cycles lockNext(): a new target
// restarts the state, none returns to the set state; aim released -> set state (or crouch 0x11).
static void wep09_r2_next(cPlayer* pl)
{
    cModel* em = pl->m_pEm;

    switch (pl->r_no_3) {
    case 0:
        pl->m_Work0 = 0;
        pl->r_no_3 = 1;
    case 1:
        if (em) {
            if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
                pl->ang.y += Muku(&pl->pos, &em->pos, pl->ang.y, 0.31415927f);
                pl->ang.y = LIMIT_ANGLE(pl->ang.y);
            }
        }
        if ((int) pl->m_Work0++ > 9) {
            PlRoutineSet(pl, 0, 6, 1, 0);
        }
        break;
    }
    if (Key.trg & 0x20) {
        if (pl->Wep->lockNext()) {
            PlRoutineSet(pl, 0, 6, 5, 0);
        } else {
            PlRoutineSet(pl, 0, 6, 1, 0);
        }
    } else {
        if (joyKamae() == 0) {
            if (pl->stat & 0x40) {
                pl->r_no_0 = 0;
                pl->r_no_2 = 0;
                pl->r_no_1 = 0x11;
                pl->r_no_3 = 0;
            } else {
                int md = 1;

                PlRoutineSet(pl, 0, 6, md, 0);
            }
        }
        CameraMove();
        pl->motionMove();
    }
}

// The module's .data section is 8-aligned in the original.
asm(".section .data\n\t.balign 8\n\t.text");
