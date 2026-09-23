#include "atari.h"
#include "at_sub2.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "item.h"
#include "pl_wep.h"
#include "wep_mod.h"
#include "player.h"
#include "cam_ctrl.h"
#include "dbmodule.h"
#include "eprintf.h"
#include "gx.h"
#include "camera.h"
#include "motion.h"
#include "em_sub.h"

// Player weapon object: the model the player holds, its mode dispatch (stay / ready / fire /
// down / reload / drop), the laser sight and the debug collision display.

extern "C" {
void Draw_line3d_local_222(Vec* p0, Vec* p1, Mtx mtx, u32 color, int blend);
void Draw_line3d_222(Vec* p0, Vec* p1, u32 color, int blend);
void drawPoint(Vec& lpos, Vec& lcross);
}

// Common weapon object setup: no collision, a 500-unit light, no motions yet, all three display
// types shown.
cObjWep::cObjWep()
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 500.0f, 0.0f, 0.0f };

    flag.reset();
    atari.throughOn();
    LightInfo.init2(1, 1, &p0, &p1, 1);
    Motion.pMot = 0;
    motReset[1] = 0;
    motReset[0] = 0;
    m_pParent = 0;
    m_StopSeId = 0;
    flag.reset();
    flag.on(F_DISP_0).on(F_DISP_1).on(F_DISP_2);
}

// Per-frame: dispatches mode (0 stay, 1 ready, 2 fire, 3 down, 4 reload, 5 drop) to the
// module's move* virtuals, then moveAll(); hides the model unless all three display types are on
// and the parent is drawn; follows the parent's transparency / ot_type; draws the laser sight if
// flag bit0 was requested this frame (bit1 remembers it for the next).
void cObjWep::move()
{
    switch (r_no_0) {
    default:
        moveStay();
        break;
    case 1:
        moveReady();
        break;
    case 2:
        moveFire();
        break;
    case 3:
        moveDown();
        break;
    case 4:
        moveReload();
        break;
    case 5:
        moveDrop();
        break;
    }
    moveAll();
    if (!flag.check(F_DISP_0) || !flag.check(F_DISP_1) || !flag.check(F_DISP_2) ||
        (m_pParent && (m_pParent->isTrans() == 0 || (DpfFlagChk(pG, DPF_PL))))) {
        be_flag &= ~2;
    } else {
        be_flag |= 2;
    }
    if (m_pParent) {
        invisible_factor = m_pParent->invisible_factor;
        invisible_factor2 = m_pParent->invisible_factor2;
    }
    ot_type = pPL->ot_type;
    if (Motion.pMot) {
        MotionMove(this, 0);
    } else {
        matUpdate();
    }
    if (flag.check(F_ON_LASER_SIGHT)) {
        drawLaserSight(1, 0);
    }
    flag.off(F_ON_LASER_SIGHT_D);
    if (flag.check(F_ON_LASER_SIGHT)) {
        flag.on(F_ON_LASER_SIGHT_D);
    }
    flag.off(F_ON_LASER_SIGHT);
}

// Sets / clears one of the three display types (0 -> flag bit2, 1 -> bit3, 2 -> bit4); the model
// is drawn only when all three are on.
void cObjWep::setDisp(int level, int onoff)
{
    if (onoff == 1) {
        switch (level) {
        case 0:
            flag.on(F_DISP_0);
            break;
        case 1:
            flag.on(F_DISP_1);
            break;
        case 2:
            flag.on(F_DISP_2);
            break;
        }
    } else {
        switch (level) {
        case 0:
            flag.off(F_DISP_0);
            break;
        case 1:
            flag.off(F_DISP_1);
            break;
        case 2:
            flag.off(F_DISP_2);
            break;
        }
    }
}

// Hangs the weapon on parts `partsNo` of `parent` with a local offset / rotation.
void cObjWep::parentSet(cModel* pMod, int parts_no, Vec* pOffset, Vec* pAng)
{
    m_pParent = pMod;
    pParts->pParent = pMod->getPartsPtr(parts_no);
    pParts->pos = *pOffset;
    pParts->ang = *pAng;
}

// Detaches the weapon at its current world position (used before dropping it).
void cObjWep::parentRelease()
{
    pos = pParts->pParent->world;
    ang.x = 0.0f;
    ang.y = 0.0f;
    ang.z = 0.0f;
    pParts->pParent = this;
    pParts->pos.x = 0.0f;
    pParts->pos.y = 0.0f;
    pParts->pos.z = 0.0f;
    pParts->ang.x = 0.0f;
    pParts->ang.y = 0.0f;
    pParts->ang.z = 0.0f;
    m_pParent = 0;
}

// End of the reload motion: back to the idle motion and, unless noReload, moves ammo into the
// magazine (ItemMgr.reload).
void cObjWep::endReload(int motOnly)
{
    resetMotion();
    if (motOnly == 0) {
        ItemMgr.reload();
    }
}

// Idle motion (the empty-magazine variant when there is no ammo), stops the weapon SE, mode 0 / step 0.
void cObjWep::resetMotion()
{
    void* mot;

    if (ItemMgr.bulletNum() == 0 && motReset[1]) {
        mot = motReset[1];
    } else if (motReset[0]) {
        mot = motReset[0];
    } else {
        mot = 0;
    }
    if (mot) {
        motionSet(mot, 0, 0, 1, 0);
        motionMove();
    }
    if (m_StopSeId) {
        SndStop(m_StopSeId, 0);
    }
    Motion.Seq_speed = 1.0f;
    r_no_0 = 0;
    r_no_1 = 0;
}

// One shot: takes a round from the magazine (ItemMgr.trigger).
void cObjWep::trigger()
{
    ItemMgr.trigger();
}

// 1 when the magazine still has a round.
int cObjWep::bulletNum()
{
    if (ItemMgr.bulletNumCurrent()) {
        return 1;
    }
    return 0;
}

// Whether a reload is possible (ammo in the inventory, magazine not full).
int cObjWep::reloadable()
{
    return ItemMgr.reloadable();
}

// Laser sight: from the marker line (getMarkerPos) finds the target (GetWepTargetPos: 1 map, 2
// enemy), sets m_SightEm / m_ShotPos, draws the laser line (thicker in rooms 22C/228; a plain
// line in shooting range mode) and the dot on an enemy; Status_flg[2] bit31 = "don't fire"
// (target with EM_STATUS_DONT_FIRE, or a map hit with an AtEffInfo flag 2 surface within 20000).
// In the debug collision display modes it shows satCheck() instead.
void cObjWep::drawLaserSight(int draw, int noCalc)
{
    static Vec lpos;
    static Vec lcross;
    static int donfire;

    if (FlagChkSign(pG->Debug_flg, DBG_SAT_DISP) || FlagChkSign(pG->Debug_flg, DBG_EAT_DISP)) {
        satCheck();
        return;
    }
    if (noCalc == 0) {
        Vec dir;
        u32 attr;
        int res;
        f32 dist;

        donfire = 0;
        partsWorldCalc();
        getMarkerPos(&lpos, &lcross);
        res = GetWepTargetPos(&lpos, &lcross, 0, pG->weapon_no, &m_SightEm, &attr);
        if (m_SightEm && m_SightEm->checkStatus(EM_STATUS_DONT_FIRE)) {
            donfire = 1;
        }
        dist = (lcross.x - lpos.x) * (lcross.x - lpos.x) + (lcross.y - lpos.y) * (lcross.y - lpos.y) +
               (lcross.z - lpos.z) * (lcross.z - lpos.z);
        switch (res) {
        case 1: {
            f32 dist2;
            AtEffInfo* info;

            dist2 = (lpos.x - lcross.x) * (lpos.x - lcross.x) + (lpos.y - lcross.y) * (lpos.y - lcross.y) +
                    (lpos.z - lcross.z) * (lpos.z - lcross.z);
            info = EatMgr.getEffInfo(EatGetEffectType(attr));
            if (info) {
                int on = 1;
                if ((info->flag & 2) == 0) {
                    on = 0;
                }
                if (on && dist2 < 400000000.0f) {
                    donfire = res;
                }
            }
            break;
        }
        case 2:
            if (draw && dist > 250000.0f) {
                drawPoint(lpos, lcross);
            }
            break;
        }
        if (dist < 250000.0f) {
            return;
        }
        PSVECSubtract(&lcross, &lpos, &dir);
#line 404 "D:/Bio4/Prog/objWep.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, 500.0f);
        PSVECAdd(&lpos, &dir, &lpos);
    }
    if (draw) {
        f32 width;

        if (pG->stage_no == 2 && pG->room_no == 0x2C || pG->stage_no == 0x22 && pG->room_no == 0x28) {
            width = 3.0f;
        } else {
            width = 1.0f;
        }
        if (pG->shooting_mode == 1) {
            Draw_line3d_222(&lpos, &lcross, 0x20400000, 1);
        } else {
            EspDrawLaserLine(lpos, lcross, width);
            if (pG->stage_no == 2 && pG->room_no == 0x2C || pG->stage_no == 2 && pG->room_no == 0x28) {
                EspDrawLaserLine(lpos, lcross, width);
            }
        }
    }
    if (donfire) {
        StaFlagOn(pG, STA_PL_DONT_FIRE);
    }
    m_ShotPos = lcross;
}

// Laser dot (esp 0x50) at p1, scaled with the camera distance (bigger in the 22C/228 rooms / when
// Status_flg[3] 0x02000000); red-tinted while Status_flg[1] bit0.
void drawPoint(Vec& lpos, Vec& lcross)
{
    static f32 laset_max_dist = 8000.0f;
    static f32 laset_max_size = 3.0f;
    static f32 laset_base_size = 0.8f;
    Vec d;
    cEsp* esp;
    f32 dist;
    f32 mul;

    if (DbgFlagChk(pG, DBG_NO_LASER_LINE)) {
        return;
    }
    if (EspEstSetSelect(EFF_CORE, 0x50, 0, &esp, 1) != 1) {
        return;
    }
    PSVECSubtract(&pG->Camera.param.pos, &lcross, &d);
    dist = PSVECMag(&d);
    if (StaFlagChk(pG, STA_BIG_MARKER) || pG->stage_no == 2 && pG->room_no == 0x2C ||
        pG->stage_no == 2 && pG->room_no == 0x28) {
        mul = dist * 0.00033333333f + 1.0f;
        if (mul > 6.0f) {
            mul = 6.0f;
        }
    } else {
        mul = dist * (1.0f / laset_max_dist) + laset_base_size;
        if (mul > laset_max_size) {
            mul = laset_max_size;
        }
    }
    esp->m_Pos = lcross;
    esp->m_Size_base_x = esp->m_Size_base_x * mul;
    esp->m_Size_base_y = esp->m_Size_base_y * mul;
    if (StaFlagChk(pG, STA_LASERSITE_NOADD)) {
        esp->m_Blend_mode = 1;
        esp->m_Src_factor = 4;
        esp->m_Dst_factor = 5;
        esp->m_Logic_op = 0;
    }
}

// Muzzle position and aim end for the current weapon_no: muzzle offset table `ofs` on parts 0
// (parts 1 for weapon 0xE) and -50000 along its x axis (+50000 for 0x1C); the bows (9, 10) use
// the camera trajectory.
void cObjWep::getMarkerPos(Vec* lpos, Vec* lcross)
{
    static const Vec ofs[46] = {
        { 500.0f, 0.0f, 0.0f },      { 262.0f, -24.0f, 32.0f },   { 228.0f, -24.0f, 29.0f },
        { 156.0f, -6.0f, 90.0f },    { 231.0f, -24.0f, 30.5f },   { 378.5f, -1.7f, 61.7f },
        { 192.0f, -24.0f, 134.0f },  { -116.0f, -21.0f, 32.0f },  { -129.0f, -23.0f, 62.0f },
        { 228.0f, -24.0f, 29.0f },   { 228.0f, -24.0f, 29.0f },   { 206.0f, -24.0f, 33.4f },
        { 4.0f, -24.0f, 85.0f },     { 265.7f, 130.0f, 73.3f },   { -97.0f, 7.0f, -18.0f },
        { 87.70001f, -23.1f, 72.6f },    { 500.0f, 0.0f, 0.0f },      { 225.6f, -24.0f, 40.1f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { -400.0f, -6.2f, 163.7f },  { 500.0f, 0.0f, 0.0f },
        { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },      { 500.0f, 0.0f, 0.0f },
        { -300.0f, -21.0f, 32.0f },  { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
    };
    Vec v;
    cModel* parts;
    f32 len;

    switch (pG->weapon_no) {
    default:
        parts = getPartsPtr(pG->weapon_no == 0xE);
        PSMTXMultVec(parts->mat, &ofs[pG->weapon_no], lpos);
        if (pG->weapon_no == 0x1C) {
            len = 50000.0f;
        } else {
            len = -50000.0f;
        }
        v.x = len;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVecSR(parts->mat, &v, &v);
        PSVECAdd(&v, lpos, lcross);
        break;
    case 9:
    case 10:
        CamCtrl.getTrajectory(lpos, lcross);
        break;
    }
}

// Weapon interrupted (event / damage / weapon change): display on, a running reload is completed,
// idle motion.
void cObjWep::interrupt()
{
    setDisp(1, 1);
    setDisp(2, 1);
    be_flag |= 2;
    if (r_no_0 == 4) {
        endReload(0);
    }
    atari.clrFlag200();
    resetMotion();
}

// The hand weapon's stance key counts only while Status_flg[3] 0x00800000 allows it.
int cObjHand::keyKamae()
{
    if (StaFlagChk(pG, STA_KLAUSER_TRANSFORM)) {
        return cObjWep::keyKamae();
    } else {
        return 0;
    }
}

#define NOHIT_COL(bit) col = 0; if ((attr & (bit)) == 0) col = 20;

// Debug (Debug_flg[0] 0x08000000: scroll, 0x04000000: effect collision): casts the marker line
// through SatMgr / EatMgr, draws the line / hit normal and prints the attribute bits by name.
void cObjWep::satCheck()
{
    static const char* strAt[2] = { "SCROLL ATARI INFO", "EFFECT ATARI INFO" };
    static const u32 lcol[2] = { 0xFF000088, 0xFF008800 };
    static const char* strEffType[8] = { "EAT_ET_NORMAL", "EAT_ET_BULLET", "EAT_ET_WATER", "EAT_ET_PAD",
                                         "EAT_ET_ROOM0", "EAT_ET_ROOM1", "EAT_ET_ROOM2", "EAT_ET_ROOM3" };
    Vec p0;
    Vec p1;
    Vec hit;
    Vec nrm;
    u32 attr;
    int col;
    u32 t = DbgFlagChk(pG, DBG_SAT_DISP);
    int eat = t == 0;

    getMarkerPos(&p0, &p1);
    if (eat == 0) {
        attr = SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0x8000, 0);
        attr |= SatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    } else {
        attr = EatMgr.hitCheck(&p0, &p1, &hit, &nrm, 0, 0);
    }
    Draw_line3d_222(&p0, &p1, lcol[attr != 0], 1);
    if (attr & 0x01000000) {
        PSVECScale(&nrm, &p1, 1000.0f);
        PSVECAdd(&p1, &hit, &p1);
        Draw_line3d(&hit, &p1, 0xFFFF5533, 0);
        Draw_pos(&hit, 300);
    }
    eprintf(24, 48, 0, 0, strAt[eat]);
    eprintf(24, 64, 0, 0, "ATTR:%08X", attr);
    if (eat == 0) {
        NOHIT_COL(0x00800000) eprintf(24, 0x50, col, 0, "SEE_NOHIT");
        NOHIT_COL(0x8000) eprintf(24, 0x60, col, 0, "SMALL_NOHIT");
        NOHIT_COL(0x80) eprintf(24, 0x70, col, 0, "STEPS");
        NOHIT_COL(0x00400000) eprintf(24, 0x80, col, 0, "PL_NOHIT");
        NOHIT_COL(0x4000) eprintf(24, 0x90, col, 0, "EM_NOHIT");
        NOHIT_COL(0x40) eprintf(24, 0xA0, col, 0, "ROUTE_NOHIT");
        NOHIT_COL(0x00200000) eprintf(24, 0xB0, col, 0, "UP");
        NOHIT_COL(0x2000) eprintf(24, 0xC0, col, 0, "DOWN");
        NOHIT_COL(0x20) eprintf(24, 0xD0, col, 0, "FANCE");
        NOHIT_COL(0x00100000) eprintf(24, 0xE0, col, 0, "FALL");
        NOHIT_COL(0x1000) eprintf(24, 0xF0, col, 0, "UP2");
        NOHIT_COL(0x10) eprintf(24, 0x100, col, 0, "DOWN2");
        NOHIT_COL(0x00080000) eprintf(24, 0x110, col, 0, "JUMPOVER");
        NOHIT_COL(0x800) eprintf(24, 0x120, col, 0, "CLIFF");
        NOHIT_COL(0x8) eprintf(24, 0x130, col, 0, "HIDE");
        NOHIT_COL(0x00040000) eprintf(24, 0x140, col, 0, "FALL FANCE");
        NOHIT_COL(0x400) eprintf(24, 0x150, col, 0, "ONLY CAM HIT");
        NOHIT_COL(0x4) eprintf(24, 0x160, col, 0, "NO EFF SET");
    } else {
        col = 0;
        eprintf(24, 0x50, col, 0, "EFF TYPE: %s", strEffType[EatGetEffectType(attr)]);
        NOHIT_COL(0x00400000) eprintf(24, 0x60, col, 0, "SMALL_NOHIT");
        NOHIT_COL(0x4000) eprintf(24, 0x70, col, 0, "MIDDLE_NOHIT");
        NOHIT_COL(0x40) eprintf(24, 0x80, col, 0, "NO EFF SET");
    }
}

// Draws a line from p0 toward p1 clipped to 8000 units, fading its colour with the length (the
// shooting-range laser); alpha 0xFE in `color` disables the z test.
void Draw_line3d_local_222(Vec* p0, Vec* p1, Mtx mat, u32 col, int blend)
{
    static f32 max_laser_dist = 8000.0f;
    Vec end;
    Vec dir;
    f32 rate = 1.0f;
    f32 d;
    u8 r, g, b, a;

    if (blend == 0) {
        GXSetBlendMode(1, 1, 0, 0);
    } else {
        GXSetBlendMode(1, 1, 1, 0);
    }
    CameraCurrentProjection();
    GXSetCullMode(0);
    if ((col >> 24) == 0xFE) {
        GXSetZMode(0, 3, 1);
    } else {
        GXSetZMode(1, 3, 1);
    }
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(mat, 0);
    GXSetCurrentMtx(0);
    r = (col >> 16) & 0xFF;
    g = (col >> 8) & 0xFF;
    b = col & 0xFF;
    a = 0xFF;

    end = *p1;
    PSVECSubtract(p1, p0, &dir);
    d = PSVECMag(&dir);
    if (d > max_laser_dist) {
#line 800 "D:/Bio4/Prog/objWep.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, max_laser_dist);
        PSVECAdd(p0, &dir, &end);
        d = max_laser_dist;
    }
    rate = 1.0f - d / max_laser_dist;

    GXBegin(0xB0, 0, 2);
    GXPosition3f32(p0->x, p0->y, p0->z);
    GXColor4u8(r, g, b, a);
    GXPosition3f32(end.x, end.y, end.z);
    GXColor4u8((u8)(r * rate), (u8)(g * rate), (u8)(b * rate), (u8)(a * rate));
}

// Draw_line3d_local_222 in the current camera view matrix.
void Draw_line3d_222(Vec* p0, Vec* p1, u32 col, int blend)
{
    Draw_line3d_local_222(p0, p1, pG->Camera.v_mat, col, blend);
}
