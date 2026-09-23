// Knife player routines of the knife weapon modules (wep16 / wep26, first object; real file name
// unknown): the DOL's game/pl_knife.cpp with every knife_r2_*/knife_r3_* routine `static` (the REL's
// .data table fields hold S+A) and the -G 0 build (the DOL's .sdata routine tables land in .data).
// Keep in sync with game/pl_knife.cpp.
//
// Entry: PlKnifeMove is player routine r_no_1 == 0xB (player.cpp's routine table), entered from
// the footwork routine when the knife key (joyLKamae) is held. r_no_2 is the knife state (0 ready,
// 1 set, 2 fire = slash, 3 down), r_no_3 the step. The knife is not a weapon module object: the
// motions come from the player archive (pG->pPlayer: 0x23/0x24 draw, 0x81/0x83/0x85 stance idle
// low/middle/high, 0x82/0x84/0x86 slash, 0x87 put away) or, with the rocket launcher (weapon_no 0x0D,
// weapon_type 2) in hand, from m_MotTbl[0x55..0x5C] (the launcher is gripped back / released around
// the slash). Wep->knifeStance (0 low, 1 middle, 2 high from the stick) picks the blend; the
// equipped gun is hidden by setWepTrans while the knife is out; hitCheck traces the blade.

#include "atari.h"
#include "light.h"
// player.h declares the four knife_r2_* entry points extern (the DOL's are global); the module's are
// static, so they get module-local names here.
#define knife_r2_ready knife_r2_ready_mod
#define knife_r2_set knife_r2_set_mod
#define knife_r2_fire knife_r2_fire_mod
#define knife_r2_down knife_r2_down_mod
#include "player.h"
#include "motion.h"
#undef knife_r2_ready
#undef knife_r2_set
#undef knife_r2_fire
#undef knife_r2_down
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "item.h"
#include "snd.h"
#include "esp.h"
#include "math_sub.h"

static void knife_r2_ready(cPlayer* pl);
static void knife_r2_set(cPlayer* pl);
static void knife_r2_fire(cPlayer* pl);
static void knife_r2_down(cPlayer* pl);
static void knife_r3_ready00(cPlayer* pl);
static void knife_r3_ready10(cPlayer* pl);
static void knife_r3_set00(cPlayer* pl);
static void knife_r3_set10(cPlayer* pl);
static void knife_r3_set20(cPlayer* pl);
static void knife_r3_set30(cPlayer* pl);
static void knife_r3_set40(cPlayer* pl);
void hitCheck(cPlayer* pl, int no, u32 flag);
static void knife_r3_fire00(cPlayer* pl);
static void knife_r3_fire10(cPlayer* pl);
static void knife_r3_down00(cPlayer* pl);
static void knife_r3_down10(cPlayer* pl);

// Routine 0xB (knife): dispatches on r_no_2, runs the lock-on stick control and the character's
// X-button (partner command) check.
void PlKnifeMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r2_ready,
        knife_r2_set,
        knife_r2_fire,
        knife_r2_down,
    };

    func_tbl[pl->r_no_2](pl);
    pl->Wep->lockMove();
    pl->checkXbutton();
}

// r_no_2 == 0: the ready (draw) state. r_no_3 == 100 is the re-entry from a set state exit
// (m_Work0 = 1). The stick picks knifeStance (up 0 low, down 2 high, else 1 middle). Knife key
// released -> the gun is shown again, the face reset, back to footwork (r_no_1 0, or 0x11 crouch
// with stat bit6); else the shoulder camera aims at the locked enemy or the forward hit.
static void knife_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_ready00,
        knife_r3_ready10,
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
    if (joyLKamae() == 0 && pl->r_no_3 != 3) {
        setWepTrans(pl, 1);
        FACE_SET(pl, 0.0f);
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
    }
}

// ready step 0: enter the stance: pitch from the camera pitch (doubled looking up) into Wep->pitch
// and m3r, aim yaw m_Fwork0 = 0, neck reset; the draw motion is the launcher's (m_MotTbl 0x59/0x5A),
// the launcher-aim variant while the aim key is held with rockets (0x55/0x56), or the player
// archive's 0x23/0x24. lockCtr = 0, step 1.
static void knife_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot0;
    void* mot1;

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
    pl->Neck->init(0, 0, 0);
    if (pG->weapon_no == 0xD && pG->weapon_type == 2) {
        mot0 = pl->m_MotTbl[0x59];
        mot1 = pl->m_MotTbl[0x5A];
    } else if (ItemMgr.bulletNum() && (Key.on & 0x10) && pG->weapon_no == 0xD) {
        mot0 = pl->m_MotTbl[0x55];
        mot1 = pl->m_MotTbl[0x56];
    } else {
        mot0 = PL_ARC_PTR(pG->pPlayer, 0x23);
        mot1 = PL_ARC_PTR(pG->pPlayer, 0x24);
    }
    mot3.set(pl, mot0, mot0, mot0, mot1, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    lockCtr = 0;
    pl->r_no_3 = 1;
}

// ready step 1: the draw plays; at frame 4 the gun is hidden and the knife face set; at frame 10
// -> set state step 4 (with the launcher: gripBack at frame 10 first).
static void knife_r3_ready10(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->Motion, 4.0f)) {
        setWepTrans(pl, 0);
        FACE_SET(pl, 1.0f);
    }
    if (pG->weapon_no == 0xD && pG->weapon_type == 2) {
        if (MotionCheckCrossFrame(&pl->Motion, 10.0f)) {
            ((cObjLauncher*) pl->Wep->m_pWep)->gripBack();
            pl->r_no_2 = 1;
            pl->r_no_3 = 4;
        }
    } else {
        if (pl->Motion.Seq_frame >= 10.0f) {
            pl->r_no_2 = 1;
            pl->r_no_3 = 4;
        }
    }
    mot3.move(m3r);
    pl->motionMove();
}

// r_no_2 == 1: the set (stance) state. Lock-on control except in step 4. Knife key released ->
// crouch 0x11, or with the rocket launcher aimed the weapon routine (r_no_1 6) ready step 2, or
// the down state (r_no_2 3); fire trigger / held -> fire (slash).
static void knife_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_set00,
        knife_r3_set10,
        knife_r3_set20,
        knife_r3_set30,
        knife_r3_set40,
    };

    func_tbl[pl->r_no_3](pl);
    if (pl->r_no_3 != 4) {
        PlWepLockCtrl(pl);
    }
    if (joyLKamae() == 0) {
        if (pl->stat.check(cPlayer::F_CROUCH)) {
            pl->r_no_0 = 0;
            pl->r_no_2 = 0;
            pl->r_no_1 = 0x11;
            pl->r_no_3 = 0;
        } else if (pG->weapon_no == 0xD && joyKamae()) {
            pl->r_no_0 = 0;
            pl->r_no_3 = 2;
            pl->r_no_2 = 0;
            pl->r_no_1 = 6;
        } else {
            pl->r_no_0 = 0;
            pl->r_no_1 = 0xB;
            pl->r_no_2 = 3;
            pl->r_no_3 = 0;
        }
    } else if (joyFireTrg() || joyFireOn()) {
        pl->r_no_2 = 2;
        pl->r_no_3 = 0;
    }
}

// set step 0: start the three-way stance idle (0x81 low / 0x83 middle / 0x85 high on m3r), step 1.
static void knife_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pPlayer;

    mot3.set(pl, PL_ARC_PTR(arc, 0x81), PL_ARC_PTR(arc, 0x83), PL_ARC_PTR(arc, 0x85), 0, 3, 0, 4, 0);
    mot3.move(m3r);
    pl->motionMove();
    pl->r_no_3 = 1;
}

// set step 1: hold the stance idle.
static void knife_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// set step 2: a turn motion held while Key.on bit2 stays down (foot SEs at frames 10 and 23);
// released -> step 0. Set by PlWepLockCtrl's turn request.
static void knife_r3_set20(cPlayer* pl)
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
static void knife_r3_set30(cPlayer* pl)
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

// set step 4: finish the draw / slash motion; ends or any fire / aim / action key -> step 0.
static void knife_r3_set40(cPlayer* pl)
{
    if (MotionMove(pl, 0) || (Key.on & 0x10F)) {
        pl->r_no_3 = 0;
    }
}

// r_no_2 == 2: the fire state (step 0 starts the slash, step 1 plays it with the blade hit checks).
static void knife_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_fire00,
        knife_r3_fire10,
    };

    func_tbl[pl->r_no_3](pl);
    PlWepLockCtrl(pl);
}

// Blade hit check for one slash frame `no` (frame - 6): the blade line runs from the right elbow
// (parts 2) to the knife tip (parts 9, 750 units out; 1200 for Krauser), traced as weapon type
// 0x10 with PlWepHitCheck2 flag bit0 (no scenery effect). Unless flag bit3 (first frame of the
// slash) it first sweeps four intermediate lines between the tip's previous position `ohpos` and
// the new one, then the line itself and two more offset +200/+400 and -100/-200 in Y (the arc).
// flag bit2 marks the frames outside 7..8 as secondary hits.
void hitCheck(cPlayer* pl, int i, u32 flag)
{
    static Vec ohpos;
    Vec p0;
    Vec p1;
    f32 len;
    cParts* parts;

    switch (pG->pl_type) {
    case 0:
    default:
        len = 750.0f;
        break;
    case 4:
        len = 1200.0f;
        break;
    }
    parts = pl->getPartsPtr(2);
    p0.x = 0.0f;
    p0.y = 0.0f;
    p0.z = 0.0f;
    PSMTXMultVec(parts->mat, &p0, &p0);
    parts = pl->getPartsPtr(9);
    p1.x = -len;
    p1.y = 0.0f;
    p1.z = 0.0f;
    PSMTXMultVec(parts->mat, &p1, &p1);
    if (!(flag & 8)) {
        Vec d;

        PSVECSubtract(&p1, &ohpos, &d);
        PSVECScale(&d, &d, 0.2f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
    }
    ohpos = p1;
    flag &= ~8;
    PlWepHitCheck2(pl, &p0, &p1, 0x10, flag | 1, 6000.0f);
    p0.y += 200.0f;
    p1.y += 400.0f;
    PlWepHitCheck2(pl, &p0, &p1, 0x10, flag | 1, 6000.0f);
    p0.y -= 300.0f;
    p1.y -= 600.0f;
    PlWepHitCheck2(pl, &p0, &p1, 0x10, flag | 1, 6000.0f);
}

// fire step 0: start the slash (0x82/0x84/0x86 blended on the pitch), twist the waist to the lock
// yaw m_Fwork0, spawn the blade-swish effect (EstSet type 0x2B) and recalculate the parts; step 1.
static void knife_r3_fire00(cPlayer* pl)
{
    PlArc* arc = pG->pPlayer;

    mot3.set(pl, PL_ARC_PTR(arc, 0x82), PL_ARC_PTR(arc, 0x84), PL_ARC_PTR(arc, 0x86), 0, 3, 0, 4, 0);
    m3r.move();
    mot3.move(m3r);
    MotionMove(pl, 0);
    pl->Waist->set(pl->m_Fwork0, 0.4f);
    EstSet(pl, -1, 0, 0, EFF_CORE, 0x2B, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
    pl->Body->waistMove();
    pl->partsWorldCalc();
    pl->r_no_3 = 1;
}

// fire step 1: the slash plays: swing SE 1/3 at frame 3, hitCheck on frames 6..10 (bit3 on the
// first, bit2 outside 7..8), a water splash effect + SE at frame 6 when the hand (parts 10) is
// near the water (the swamp rooms 010A/011A use effect 0x25). Two frames before the end -> set
// state step 4.
static void knife_r3_fire10(cPlayer* pl)
{
    f32 st = 6.0f;   // unused: only order the constant pool (6.0, 10.0 before 3.0)
    f32 ed = 10.0f;
    f32 wh;

    if (MotionCheckCrossFrame(&pl->Motion, 3.0f)) {
        SndCall(1, 3, &pl->getPartsPtr(4)->world, 0, 0, 0);
    }
    pl->motionMove();
    if (pl->Motion.Seq_frame >= 6.0f && pl->Motion.Seq_frame <= 10.0f) {
        u32 flag = 0;

        if (!(pl->Motion.Seq_frame >= 7.0f && pl->Motion.Seq_frame <= 8.0f)) {
            flag = 4;
        }
        if (pl->Motion.Seq_frame > 5.7f && pl->Motion.Seq_frame < 6.3f) {
            flag |= 8;
        }
        hitCheck(pl, (int) (pl->Motion.Seq_frame - 6.0f), flag);
    }
    if (MotionCheckCrossFrame(&pl->Motion, 6.0f)) {
        Vec* pos = &pl->getPartsPtr(10)->world;

        if (GetWaterHeight(pos, &wh) && pos->y < wh + 100.0f) {
            if (pG->stage_no == 1 && pG->room_no == 0xA || pG->stage_no == 1 && pG->room_no == 0x1A) {
                EstSet(pl, -1, 0, 0, EFF_ROOM, 0x25, 0, ESP_CORE_KIND_NONE, pl, 0);
            } else {
                EstSet(pl, -1, 0, 0, EFF_PL00, 0, 0, ESP_CORE_KIND_NONE, pl, 0);
            }
            SndCall(1, 0x51, &pl->getPartsPtr(10)->world, 0, 0, 0);
        }
    }
    if (pl->Motion.Seq_frame >= (f32) (pl->Motion.Seq_frame_num - 2)) {
        pl->r_no_2 = 1;
        pl->r_no_3 = 4;
    }
}

// r_no_2 == 3: the down state (put the knife away). Unwinds the waist twist into ang.y, sets
// Status_flg[0] bit25 (knife put away this frame) and runs the control-flag check.
static void knife_r2_down(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_down00,
        knife_r3_down10,
    };

    func_tbl[pl->r_no_3](pl);
    pl->ang.y = pl->ang.y - pl->Waist->set(0.0f, 0.4f);
    StaFlagOn(pG, STA_SSCRN_ENABLE);
    pl->checkCtrl();
}

// down step 0: pick the put-away motion: aim key held -> knife-to-gun transition (m_MotTbl
// 0x57/0x58, weapon object mode 1, m_Work0 = 1: end in the weapon routine), else the launcher's
// 0x5B/0x5C or the archive's 0x87 (m_Work0 = 0). No motion available (damage) -> straight back to
// footwork with the gun shown; else the motion starts (flag 0x100 for weapon 0xE) and step 1.
static void knife_r3_down00(cPlayer* pl)
{
    void* mot0;
    void* mot1;

    if (joyKamae()) {
        cObjWep* obj = pl->Wep->m_pWep;

        int on = 1;

        obj->r_no_0 = on;
        obj->r_no_1 = 0;
        mot0 = pl->m_MotTbl[0x57];
        mot1 = pl->m_MotTbl[0x58];
        pl->m_Work0 = on;
    } else {
        if (dmMotCk()) {
            mot0 = pl->m_MotTbl[0x5B];
            mot1 = pl->m_MotTbl[0x5C];
        } else {
            mot0 = PL_ARC_PTR(pG->pPlayer, 0x87);
            mot1 = 0;
        }
        pl->m_Work0 = 0;
    }
    if (mot0 == 0) {
        FACE_SET(pl, 0.0f);
        setWepTrans(pl, 1);
        pl->r_no_0 = 0;
        pl->r_no_1 = 0;
        pl->r_no_2 = 0;
        pl->r_no_3 = 0;
    } else {
        pl->motionSet(mot0, 5, 0, (pG->weapon_no == 0xE && pG->weapon_type == 0) ? 0x100 : 0, mot1);
        pl->motionMove();
        pl->r_no_3 = 1;
    }
}

// back to routine 0 (both exits of down10 share this tail)
#define KNIFE_RESET(pl)   \
    do {                  \
        (pl)->r_no_0 = 0;    \
        (pl)->r_no_1 = 0;    \
        (pl)->r_no_2 = 0;    \
        (pl)->r_no_3 = 0;    \
    } while (0)

// down step 1: the put-away plays: gun shown / face reset at frame 4, the launcher released at
// frame 15. At the end: m_Work0 == 0 -> footwork idle, else the weapon routine's set state with
// the pitch reset. Any fire / aim / action key, or the aim key changing against m_Work0, cancels
// straight to footwork.
static void knife_r3_down10(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->Motion, 4.0f)) {
        FACE_SET(pl, 0.0f);
        setWepTrans(pl, 1);
    }
    if (MotionCheckCrossFrame(&pl->Motion, 15.0f)) {
        if (pG->weapon_no == 0xD && pG->weapon_type == 2) {
            ((cObjLauncher*) pl->Wep->m_pWep)->grip(0);
        }
    }
    if (pl->motionMove()) {
        if (pl->m_Work0 == 0) {
            KNIFE_RESET(pl);
        } else {
            pl->r_no_0 = 0;
            pl->r_no_1 = 6;
            pl->r_no_2 = 1;
            pl->r_no_3 = 0;
            pl->Wep->pitch = 0.0f;
            m3r.reset(0.0f);
        }
    } else if ((Key.on & 0x10F) || (pl->m_Work0 != 0 && joyKamae() == 0) || (pl->m_Work0 == 0 && joyKamae() != 0)) {
        FACE_SET(pl, 0.0f);
        setWepTrans(pl, 1);
        if (pG->weapon_no == 0xD && pG->weapon_type == 2) {
            ((cObjLauncher*) pl->Wep->m_pWep)->grip(0);
        }
        KNIFE_RESET(pl);
    }
}

// Show (on = 1) / hide the equipped gun's display type 1 while the knife is out: the rifles and
// the launcher-type weapons (0x13, 0x16, 0x17, 0x19, 0x1F, 0x20) switch their second object
// pObj2, the rocket launcher (0xD) is never hidden.
void setWepTrans(cPlayer* pl, int onoff)
{
    switch (pG->weapon_no) {
    default:
        pl->Wep->m_pWep->setDisp(1, onoff);
        break;
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1F:
    case 0x20:
        pl->Wep->m_pWepHand->setDisp(1, onoff);
        break;
    case 0xD:
        break;
    }
}

// The module's .data section is 8-aligned in the original (the routine tables end at 0x3C).
asm(".section .data\n\t.balign 8\n\t.text");
