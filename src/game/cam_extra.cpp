// game/cam_extra.cpp: the special cCamera implementations the camera controller switches to:
// CameraAttachedToMotion (a camera track inside a model's motion), CameraScope (rifle scope
// with zoom, pitch and reticle ids), CameraBinocular (with its HUD ids), CameraPushObject,
// CameraLookAt (item examine) and CameraLookDownEm, plus the FocusAnimation blur used by the
// scope and binoculars.

#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "atari.h"
#include "db_log.h"
#include "math_sub.h"
#include "model.h"
#include "motion.h"
#include "player.h"
#include "em.h"
#include "id_sys.h"
#include "main.h"
#include "joy.h"
#include "rnd.h"
#include "mes.h"
#include "cockpit.h"
#include <string.h>
#include "filter.h"


static u8 init_focus_frame = 30;
static u8 init_alpha_max = 0xFF;
f32 focus_frame = 5.0f;
u8 alpha_max = 0xDC;

// ---------------------------------------------------------------------------
// CameraAttachedToMotion: follows the AttachCamera channels of a model's motion.
// ---------------------------------------------------------------------------

CameraAttachedToMotion::CameraAttachedToMotion(cModel* m)
{
    m_pModel = m;
}

// Poisons the object (memset 9) so a stale pointer is caught.
CameraAttachedToMotion::~CameraAttachedToMotion()
{
    memset(this, 9, 0x200);
}

// Per-frame: reads the model's attached camera track (pAttachCam: pos / at / roll / fov keys of
// the motion) and, unless the motion is flagged world-space (Mot_attr 0x200), transforms them
// from the model's frame into the world; rebuilds the orientation with roll.
void CameraAttachedToMotion::move()
{
    AttachCamera* ac = MOTION(m_pModel)->pAttachCam;
    Vec hit;
    Vec nrm;
    Vec pos;
    Vec at;
    Vec d;
    Mtx inv;
    Vec to;
    Vec from;

    if (ac == 0) {
        return;
    }
    if (ac->parts[0] != 0xFF) {
        param.pos = ac->camera_data[0];
        PSMTXMultVec(*ac->p_mat, &param.pos, &param.pos);
    }
    if (ac->parts[1] != 0xFF) {
        param.at = ac->camera_data[1];
        PSMTXMultVec(*ac->p_mat, &param.at, &param.at);
    }
    if (ac->parts[2] != 0xFF) {
        param.roll = ac->camera_data[2].y;
    }
    if (ac->parts[3] != 0xFF) {
        param.fovy = ac->camera_data[3].y * 180.0f / PI;
    }
    if (!(MOTION(m_pModel)->Mot_attr & 0x200)) {
        PSMTXInverse(m_pModel->mat, inv);
        PSMTXMultVec(inv, &param.pos, &pos);
        PSMTXMultVec(inv, &param.at, &at);
        // Frame order hit, nrm, pos, at, d, inv, to, from; the model-space test is at.z > 0 && pos.z < 0
        // and the hit check runs from the transformed `at` copy to the `pos` copy.
        if (at.z > 0.0f && pos.z < 0.0f) {
            PSVECSubtract(&at, &pos, &d);
            PSVECScale(&d, &d, -pos.z / d.z);
            PSVECAdd(&pos, &d, &to);
            PSMTXMultVec(m_pModel->mat, &to, &param.at);
        }
        to = param.at;
        from = param.pos;
        if (cameraHitCheck(&hit, &nrm, &to, &from)) {
            param.pos = hit;
        }
    }
    CameraSetOrientationRoll(this);
}

// ---------------------------------------------------------------------------
// FocusAnimation: filter0a blur fade.
// ---------------------------------------------------------------------------

void FocusAnimation::init(int mask_id)
{
    filter0a_mask_alpha = alpha_max;
    if (mask_id < 0) {
        filter0a_mask_flag = use_filter0a = 0;
    } else {
        filter0a_mask_id = mask_id;
        filter0a_mask_flag = use_filter0a = 1;
    }
    m_anim_on = 1;
    m_focus_frame = (f32) init_focus_frame;
    m_alpha_max = init_alpha_max;
    m_counter = (int) m_focus_frame;
}

// Focus blur animation step: dir 1 ramps the counter up to m_focus_frame (blur in while zooming),
// dir 0 ramps it down (blur out), then sets the filter0a mask alpha or the Filter01 level from
// counter / m_focus_frame. m_anim_on: 0 idle, 1 rising, 2 falling.
void FocusAnimation::move(int anim_flag)
{
    static int _filter0a_flag = 0;
    static f32 level_max = 7.0f;

    if (anim_flag != 0) {
        m_focus_frame = focus_frame;
        m_alpha_max = alpha_max;
    }
    switch (m_anim_on) {
    case 0:
        if (anim_flag == 1) {
            m_counter++;
            if (m_counter >= (int) m_focus_frame) {
                m_anim_on = 1;
            }
        } else {
            m_counter = 0;
        }
        break;
    case 1:
        if (anim_flag == 0) {
            m_counter--;
            if (m_counter < 0) {
                m_anim_on = 0;
                m_focus_frame = focus_frame;
                m_alpha_max = alpha_max;
            }
        } else {
            m_counter = (int) m_focus_frame;
        }
        break;
    }
    if (_filter0a_flag) {
        int cnt;
        if ((f32) m_counter > m_focus_frame) {
            cnt = (int) m_focus_frame;
        } else {
            cnt = m_counter;
        }
        filter0a_mask_alpha = (u8) ((f32) (m_alpha_max * cnt) / m_focus_frame);
    } else {
        int cnt;
        if ((f32) m_counter > m_focus_frame) {
            cnt = (int) m_focus_frame;
        } else {
            cnt = m_counter;
        }
        Filter01SetParam(1, 100, 1, level_max * (f32) cnt / m_focus_frame);
        filter0a_mask_alpha = 0;
    }
}

// Ends the animation: disables the filter0a mask and clears the state.
void FocusAnimation::quit()
{
    use_filter0a = 0;
    filter0a_mask_flag = 0;
    filter0a_mask_id = 0;
}

// Resets the animation state and parameters to the defaults.
void FocusAnimation::clear()
{
    m_anim_on = 0;
    m_counter = 0;
    m_focus_frame = focus_frame;
    m_alpha_max = alpha_max;
}

// ---------------------------------------------------------------------------
// CameraScope: rifle scope view.
// ---------------------------------------------------------------------------

// The wep_type filter is an if/else-if chain on a local (`t == 0`, `== 1`, `== 2`, each storing
// `t`; jump2 cross-jumps the three `stb`s) -- a switch or `||` on one value range-folds; the 9/10/0x28
// arm is one body (its label has a jump use, so cse reloads pG there) and the `type = 0` arm is written
// last so its `stb` is the cross-jump survivor; `&dir` is written per use (a `Vec* dir` local
// merges the arms' PRE copies `mr r29, ..`).
#define SCOPE_WEP_TYPE()                                                                             \
    {                                                                                                \
        int t = pG->weapon_type;                                                                        \
        if (t == 0) {                                                                                \
            type = t;                                                                                \
        } else if (t == 1) {                                                                         \
            type = t;                                                                                \
        } else if (t == 2) {                                                                         \
            type = t;                                                                                \
        }                                                                                            \
    }

// The rifle scope camera: the eye offset and look direction come from pos / at in the player's
// frame (type picks the scope reticle set by the equipped rifle), fov 45, pitch limited to
// +-70 degrees, zoom 0, and the reticle ids are created.
CameraScope::CameraScope(Vec* pos, Vec* at)
{
    Mtx inv;
    const f32 len = 300000.0f;

    PSMTXInverse(pPL->mat, inv);
    if (pos && at) {
        pos_ofs = *pos;
        PSVECSubtract(at, pos, &m_rad);
    } else {
        cParts* p[2];
        Vec* d;

        p[0] = pPL->getPartsPtr(0x20);
        p[1] = pPL->getPartsPtr(0x21);
        PSVECAdd(&p[0]->world, &p[1]->world, &pos_ofs);
        PSVECScale(&pos_ofs, &pos_ofs, 0.5f);
        cPlayer* pl = pPL;

        d = &m_rad;
        d->x = pl->mat[0][2];
        d->y = pl->mat[1][2];
        d->z = pl->mat[2][2];
    }
    PSMTXMultVec(inv, &pos_ofs, &pos_ofs);
#line 306 "D:/Bio4/Prog/cam_extra.cpp"
    VECNormalize(&m_rad, &m_rad);
    PSVECScale(&m_rad, &m_rad, len);
    PSMTXMultVecSR(inv, &m_rad, &m_rad);
    switch (pG->weapon_no) {
    case 9:
    case 10:
    case 0x28:
        SCOPE_WEP_TYPE();
        break;
    case 13:
    case 14:
    case 0x1D:
        type = 0;
        break;
    }
    f32 zero = 0.0f;
    param.fovy = 45.0f;
    angle_min = -70.0f * 3.1415927f / 180.0f;
    angle_max = 70.0f * 3.1415927f / 180.0f;
    param.roll = zero;
    m_zoom_ratio = zero;
    angle_x = zero;
    m_rnd.x = zero;
    m_rnd.y = zero;
    m_rnd.z = zero;
    m_id.init(&type);
    m_focus.init(0x9A);
}

// Removes the reticle ids and the focus filter, poisons the object.
CameraScope::~CameraScope()
{
    m_id.quit(0);
    m_focus.quit();
    memset(this, 9, 0x200);
}

// Restores zoom (0..1) and pitch (radians) - the weapon keeps them between scope uses.
void CameraScope::setParam(f32 zoom_ratio, f32 x_radian)
{
    m_zoom_ratio = zoom_ratio;
    angle_x = x_radian;
    m_focus.clear();
}

// Reads back zoom and pitch.
void CameraScope::getParam(f32* zoom_ratio, f32* x_radian)
{
    *zoom_ratio = m_zoom_ratio;
    *x_radian = angle_x;
}



// Scope zoom clamp as an inline returning the value: one store after the join, the 0.0 register
// doubling as the result (`fmr f13,f0` / `fmr f13,f12` copies).
static inline f32 scopeClamp01(f32 v)
{
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

// Per-frame scope view: C-stick Y zooms (fov 45 down to the scope's limit), the main stick / D-pad
// turns the player (yaw goes into pPL->ang.y) and pitches within the limits with a gain that
// shrinks with zoom, a breathing sway (m_rnd) is added while aiming, the camera is placed at the
// eye offset in the player's frame, the reticle ids update and the focus blur follows zoom
// changes.
void CameraScope::move()
{
    static f32 ZOOM_LIMIT_0 = 9.0f;
    static f32 ZOOM_LIMIT_1 = 3.0f;
    static f32 SCOP_VEL_Y = 0.062831856f;
    static f32 SCOP_VEL_X = 0.062831856f;
    static f32 rnd_gain = 0.001f;   // unreferenced, still in .sdata
    static int rnd_on = 1;          // unreferenced, still in .sdata
    static f32 yure_spd = 0.062831856f;
    static f32 rnd_gain2 = 0.0005f;
    static u8 sct_max = 0x96;       // unreferenced, still in .sdata
    static f32 x_yure_spd;
    static f32 y_yure_spd;
    static f32 xtime;
    static f32 ytime;
    static f32 rdir0;
    static f32 rdir = 0.0f;
    static u8 sct = 0;
    static int pastkey = 0;
    f32 old_zoom = m_zoom_ratio;
    f32 gain;
    f32 limit;
    f32 add;
    f32 ang;
    Mtx m;
    Vec dir;
    Vec ofs;
    Vec yure2;

    param.fovy = 45.0f;
    if (Key.on & 0x10) { // low word bit 4 (the target masks the low half of the u64)
        f32 sy = (f32) Joy[0].substickY;
        if (sy != 0.0f) {
            m_zoom_ratio = old_zoom + sy * 0.001f;
        }
    }
    m_zoom_ratio = scopeClamp01(m_zoom_ratio);
    if (m_zoom_ratio != 0.0f) {
        limit = ZOOM_LIMIT_0;
        switch (type) {
        case 0:
        case 2:
            limit = ZOOM_LIMIT_0;
            break;
        case 1:
            limit = ZOOM_LIMIT_1;
            break;
        }
        param.fovy = m_zoom_ratio * (limit - param.fovy) + param.fovy;
    }
    gain = m_zoom_ratio * -0.9f + 1.0f;
    if (Key.on & 0x10) {
        if (Joy[0].stickX != 0 || (Joy[0].on & 3)) {
            add = gain * (f32) Joy[0].stickX * -0.05f * DEG2RAD;
            if (Joy[0].on & 1) {
                add = gain * SCOP_VEL_Y + add;
            }
            if (Joy[0].on & 2) {
                add = add - gain * SCOP_VEL_Y;
            }
            pPL->ang.y += add;
        }
    }
    if (Key.on & 0x10) {
        if (Joy[0].stickY != 0 || (Joy[0].on & 0xC)) {
            add = gain * (f32) Joy[0].stickY * -0.05f * DEG2RAD;
            if (Joy[0].on & 8) {
                add = add - gain * SCOP_VEL_X;
            }
            if (Joy[0].on & 4) {
                add = gain * SCOP_VEL_X + add;
            }
            if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
                add = -add;
            }
            ang = angle_x;
            if (ang + add < angle_min) {
                add = angle_min - ang;
            } else if (ang + add > angle_max) {
                add = angle_max - ang;
            }
            angle_x += add; // `+=`: the sum is tied to the load register, `ang` copied (`fmr f11,f0`)
        }
    }
    {
        u8 c = sct--; // the old value in a byte local: `clrlwi r0,r9,24` + `addi r9,r9,255`
        if (c == 0) {
        x_yure_spd = yure_spd * (fRand1_1() * 0.5f + 1.0f);
        y_yure_spd = yure_spd * (fRand1_1() * 0.5f + 1.0f);
        rdir0 = fRand0_1() * rnd_gain2;
        sct = 0x96;
    }
    }
    rdir = rdir * 0.95f + rdir0 * 0.05f;
    m_rnd.x = COSF(xtime) * rdir;
    m_rnd.y = COSF(ytime) * (rnd_gain2 - rdir);
    xtime = LIMIT_ANGLE(xtime + x_yure_spd);
    ytime = LIMIT_ANGLE(ytime + y_yure_spd);
    if (pastkey != 1 || Joy[0].stickX || Joy[0].stickY) {
        Vec* a = (Vec*) &angle_x;
        PSVECAdd(a, &m_rnd, a);
        yure2 = *a;
    }
    PSMTXRotRad(m, 'x', yure2.x);
    PSMTXMultVecSR(m, &this->m_rad, &dir);
    PSMTXRotRad(m, 'y', yure2.y);
    PSMTXMultVecSR(m, &dir, &dir);
    PSVECAdd(&pos_ofs, &dir, &ofs);
    {
        cModel* pl = pPL; // held in r30 across the four calls, `&pl->worldMat` in r29
        RotMatrix(pl->l_mat, &pl->ang);
        TransMatrix(pl->l_mat, &pl->pos);
        ScaleMatrix(pl->l_mat, &pl->scale);
        PSMTXCopy(pl->l_mat, pl->mat);
    }
    PSMTXMultVec(pPL->mat, &pos_ofs, &param.pos);
    PSMTXMultVec(pPL->mat, &ofs, &param.at);
    CameraSetOrientationZeroRoll(this);
    m_id.move(&m_zoom_ratio);
    if (old_zoom != m_zoom_ratio) {
        m_focus.move(1);
    } else {
        m_focus.move(0);
    }
}

// ---------------------------------------------------------------------------
// IdScope
// ---------------------------------------------------------------------------

void IdScope::init(void* type)
{
    s8 t = *(u8*) type;

    IdTexDataLoad(WEP_ARC_PTR(4), TEX_OWNER_ID_SCOPE);
    switch (t) {
    case 0:
        IdSys.set(WEP_ARC_PTR(5), 0xFF, IDC_SCOPE, 0x13, 6, 0);
        break;
    case 1:
        IdSys.set(WEP_ARC_PTR(6), 0xFF, IDC_SCOPE, 0x13, 6, 0);
        break;
    case 2:
        IdSys.set(WEP_ARC_PTR(7), 0xFF, IDC_SCOPE, 0x13, 6, 0);
        break;
    }
}

// Reticle ids per frame: positions the zoom indicator (unit 0x25 ids 1 / 2) from *zoom.
void IdScope::move(void* arg)
{
    f32* zoom = (f32*) arg;
    static f32 minA = -90.0f;
    static f32 maxA = 180.0f;
    static f32 ampA = 0.08f;
    static int spdA = 45;
    static f32 minB = 90.0f;
    static f32 maxB = -90.0f;
    static f32 ampB = 0.05f;
    static int spdB = 60;
    IdUnit* a;
    IdUnit* b;
    f32 ra;
    f32 rb;

    if (pG->weapon_no != 0xE) {
        return;
    }
    ra = ampA * SINF((f32) (pG->Frame_cnt % spdA) * 6.2831855f / (f32) spdA) + *zoom;
    rb = ampB * COSF((f32) (pG->Frame_cnt % spdB) * 6.2831855f / (f32) spdB) + *zoom;
    a = IdSys.unitPtr(1, IDC_SCOPE);
    b = IdSys.unitPtr(2, IDC_SCOPE);
    a->curve[3] = 0;
    b->curve[3] = 0;
    a->rot0.y = 0.0f;
    a->rot0.x = 0.0f;
    a->rot0.z = (maxA - minA) * ra + minA;
    b->rot0.y = 0.0f;
    b->rot0.x = 0.0f;
    b->rot0.z = (maxB - minB) * rb + minB;
}

// Never called: its body is stripped at link (STRIP_UNUSED) but its pool words (0.5f, 100000.0f)
// follow IdScope::move's in the original `.rodata`.
static int IdScopeZoomDisp(f32* zoom)
{
    return (int) ((*zoom + 0.5f) * 100000.0f);
}

// Saves the reticle id timers (unit 0x25 ids 0 / 0x10) across a scope re-entry.
void IdScope::save(void*)
{
    m_pos_time_sav = (s16) IdSys.unitPtr(0, IDC_SCOPE)->timer[0];
    m_size_time_sav = (s16) IdSys.unitPtr(0x10, IDC_SCOPE)->timer[1];
}

// Restores the saved reticle id timers (ids 0, 0x10..0x13).
void IdScope::load(void*)
{
    IdSys.unitPtr(0, IDC_SCOPE)->timer[0] = m_pos_time_sav;
    IdSys.unitPtr(0x10, IDC_SCOPE)->timer[1] = m_size_time_sav;
    IdSys.unitPtr(0x11, IDC_SCOPE)->timer[1] = m_size_time_sav;
    IdSys.unitPtr(0x12, IDC_SCOPE)->timer[1] = m_size_time_sav;
    IdSys.unitPtr(0x13, IDC_SCOPE)->timer[1] = m_size_time_sav;
}

// Kills the reticle ids (unit 0x25).
void IdScope::quit(void*)
{
    IdTexRelease(TEX_OWNER_ID_SCOPE);
    IdSys.kill(0xFF, IDC_SCOPE);
}

// ---------------------------------------------------------------------------
// CameraBinocular
// ---------------------------------------------------------------------------

// Frame order c 0x8, up 0x18, inv 0x28 (declaration order); the else arm keeps the getPartsPtr
// results in cModel* locals and writes this->up through a `Vec* u`. OPEN (17 words): the else arm's
// gcse copies of `&param.pos`/`&param.at` (`addi r30,r31,164; addi r29,r31,176; mr r26; mr r25`) sit
// before the first getPartsPtr call in the target and after the second one in ours.
CameraBinocular::CameraBinocular(Vec* pos, Vec* at, void* a, void* b)
{
    Vec c;
    Vec up;
    Mtx inv;

    id_a = a;
    id_b = b;
    if (pos && at) {
        m_flag = 0;
        param.pos = *pos;
        param.at = *at;
        this->Up.x = 0.0f;
        this->Up.y = 1.0f;
        this->Up.z = 0.0f;
    } else {
        m_flag = 1;
        cParts* p[2];
        p[0] = pPL->getPartsPtr(0x20); // the load waits for the `mode` store: the two
                                              // param addresses go above the call
        p[1] = pPL->getPartsPtr(0x21);
        PSVECAdd(&p[0]->world, &p[1]->world, &c);
        PSVECScale(&c, &c, 0.5f);
        up.x = pPL->mat[0][2];
        up.y = pPL->mat[1][2];
        up.z = pPL->mat[2][2];
        param.pos = c;
        PSVECAdd(&c, &up, &param.at);
        {
            cPlayer* pl = pPL;
            Vec* u = &this->Up;

            u->x = pl->mat[0][1];
            u->y = pl->mat[1][1];
            u->z = pl->mat[2][1];
        }
    }
    param.fovy = 45.0f;
    CameraSetOrientationUp(this);
    if (m_flag != 0) {
        PSMTXInverse(pPL->mat, inv);
        PSMTXMultVec(inv, &param.pos, &m_campos);
        PSMTXMultVec(inv, &param.at, &m_target);
        PSMTXMultVecSR(inv, &this->Up, &m_up_vec);
    }
    // Store order pinned by the dying-store rule (the last use of each constant is issued first).
    m_zoom_ratio = 0.0f;
    m_rad.x = 0.0f;
    m_rad.y = 0.0f;
    m_rad_low.x = -1.0471976f;
    m_rad_low.y = -1.0471976f;
    m_rad_up.x = 1.0471976f;
    m_rad_up.y = 1.0471976f;
    m_id.init(this, id_a, id_b);
    m_focus.init(-1);
}

// Ends the binocular ids and the focus filter, poisons the object.
CameraBinocular::~CameraBinocular()
{
    m_id.quit(this);
    m_focus.quit();
    memset(this, 9, 0x200);
}

// Limits the binocular pitch (x) / yaw (y) angles (radians; default +-60 degrees).
void CameraBinocular::setRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up)
{
    m_rad_low.x = x_low;
    m_rad_low.y = y_low;
    m_rad_up.x = x_up;
    m_rad_up.y = y_up;
}

// Per-frame binocular view: C-stick Y zooms (fov 45 down to 3), stick / D-pad turns yaw / pitch
// within the range with a zoom-dependent gain; mode != 0 places pos / at / up from the stored
// player-relative vectors, mode 0 keeps them; the focus blur follows zoom changes.
void CameraBinocular::move()
{
    static f32 zoom_limit = 3.0f;
    static f32 BINO_VEL_Y = 0.062831856f;
    static f32 BINO_VEL_X = 0.062831856f;
    f32 old_zoom = m_zoom_ratio;
    f32 gain;
    f32 add;
    f32 ang;

    if (m_flag != 0) {
        param.pos = m_campos;
        param.at = m_target;
        Up = m_up_vec;
        CameraSetOrientationUp(this);
    }
    param.fovy = 45.0f;
    {
        f32 sy = (f32) Joy[0].substickY;
        if (sy != 0.0f) {
            m_zoom_ratio = sy * 0.001f + m_zoom_ratio;
        }
    }
    {
        // clamped copy kept in a register (`fmr f12`), stored once, reused by the fovy formula
        f32 zoom = (m_zoom_ratio < 0.0f) ? 0.0f : (m_zoom_ratio > 1.0f) ? 1.0f : m_zoom_ratio;
        m_zoom_ratio = zoom;
        if (zoom != 0.0f) {
            param.fovy = zoom * (zoom_limit - param.fovy) + param.fovy;
        }
    }
    gain = m_zoom_ratio * -0.9f + 1.0f;
    if (Joy[0].stickX != 0 || (Joy[0].on & 3)) {
        Vec axis = {0.0f, 1.0f, 0.0f}; // initialised inside this block (the stores sit below the stb)
        add = gain * (f32) Joy[0].stickX * -0.05f * DEG2RAD;
        if (Joy[0].on & 1) {
            add = gain * BINO_VEL_Y + add;
        }
        if (Joy[0].on & 2) {
            add = add - gain * BINO_VEL_Y;
        }
        ang = m_rad.y;
        if (ang + add < m_rad_low.y) { // two arms: the `fsubs` tails are cross-jumped, each with its own limit
            add = m_rad_low.y - ang;
        } else if (ang + add > m_rad_up.y) {
            add = m_rad_up.y - ang;
        }
        CameraRotAxisPosRad(this, &axis, &param.pos, add);
        m_rad.y = m_rad.y + add;
    }
    if (Joy[0].stickY != 0 || (Joy[0].on & 0xC)) {
        add = gain * (f32) Joy[0].stickY * 0.05f * DEG2RAD;
        if (Joy[0].on & 8) {
            add = gain * BINO_VEL_X + add;
        }
        if (Joy[0].on & 4) {
            add = add - gain * BINO_VEL_X;
        }
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            add = -add;
        }
        ang = m_rad.x;
        if (ang + add < m_rad_low.x) {
            add = m_rad_low.x - ang;
        } else if (ang + add > m_rad_up.x) {
            add = m_rad_up.x - ang;
        }
        CameraTargetRot(this, 'x', add);
        m_rad.x = m_rad.x + add;
    }
    if (m_flag != 0) {
        m_campos = param.pos;
        m_target = param.at;
        m_up_vec = Up;
        PSMTXMultVec(pPL->mat, &m_campos, &param.pos);
        PSMTXMultVec(pPL->mat, &m_target, &param.at);
        PSMTXMultVecSR(pPL->mat, &m_up_vec, &Up);
    }
    CameraSetOrientationUp(this); // unconditional: mode 0 jumps to it
    m_id.move(this);
    if (old_zoom != m_zoom_ratio) {
        m_focus.move(1);
    } else {
        m_focus.move(0);
    }
}

// ---------------------------------------------------------------------------
// IdBinocular
// ---------------------------------------------------------------------------

void IdBinocular::init(Camera* cam, void* a, void* b)
{
    IdUnit* u;

    IdSys.kill(0xFF, IDC_LIFE_METER);
    IdSys.kill(0xFF, IDC_ACT_BUTTON);
    IdSys.kill(0xFF, IDC_COUNT_DOWN);
    IdSys.kill(0xFF, IDC_CINESCO);
    SpfFlagOn(pG, SPF_ACTBTN);
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdTexDataLoad(a, TEX_OWNER_ID_COCKPIT);
    IdSys.set(b, 0xFF, IDC_BINOCULAR, 0x13, 5, 0);
    m_pos0_L = IdSys.unitPtr(1, IDC_BINOCULAR)->pos0;
    m_pos0_C = IdSys.unitPtr(2, IDC_BINOCULAR)->pos0;
    m_pos0_R = IdSys.unitPtr(3, IDC_BINOCULAR)->pos0;
    if (StaFlagChk(pG, STA_EVENT)) {
        IdSys.unitPtr(0x30, IDC_BINOCULAR)->be_flag &= ~8;
        IdSys.unitPtr(0x1B, IDC_BINOCULAR)->be_flag &= ~8;
    }
    m_fovy_old = cam->param.fovy;
    u = IdSys.unitPtr(0x35, IDC_BINOCULAR);
    m_meter_pos0 = u->pos0;
    m_meter_h0 = u->size_H;
    m_meter_w0 = u->size_W;
}

// The skipped ids are a switch (`||`/`&&` range tests fold to `cmplwi 4`). OPEN (3 words): the three
// `sth` come out in source order in the target although `u` dies at the last one.
void IdBinocular::cutin(void* arg)
{
    int i;

    for (i = 0; i <= 0x40; i++) {
        switch (i) {
        case 0xD:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
            continue;
        }
        IdUnit* u = IdSys.unitPtr(i, IDC_BINOCULAR);
        u->timer[0] = 0x96;
        u->timer[1] = 0x96;
        u->timer[2] = 0x96;
        // Codeless keep-alive on an unstored field: the original's u does not die at the last
        // store, so the three sth stay in source order; the non-volatile form (unlike the earlier
        // volatile input-only asm) is no scheduling barrier, so the unitPtr arg moves keep their order.
        asm("" : "=m"(u->timer[3]) : "r"(u)); // COMPILER-DIFF: #13 (keep-alive)
    }
}

// Binocular HUD per frame: the heading scale (compass digits scrolled by the view yaw), the zoom
// gauge height from the fov, and the distance readout.
void IdBinocular::move(void* p)
{
    Camera* cam = (Camera*) p;
    static f32 ratio = 0.5f;
    static f32 m = 0.5f;
    static f32 n = 1.0f;
    Vec dir;
    u8 digit[4];
    f32 ang;
    f32 lo;
    f32 hi;
    f32 rate;
    f32 y;
    int i;
    int cnt;
    int dist;

    getColumn(cam->mat, 2, &dir);
    ang = (4.712389f - atan2f(-dir.x, -dir.z)) / PI;
    {
        IdUnit* u = IdSys.unitPtr(0, IDC_BINOCULAR);
        u->u0 = ang;
        u->u1 = ang + 1.0f;
        lo = ang - 0.5f;
        hi = ang + 0.5f;
    }
    // `cnt` crosses no call: each mark passes `cnt + 1` (a temp the call crosses) and cse turns the
    // `cnt++` after the stores into a copy of that temp, so sched1 keeps the `li cnt,0/1` below the
    // unitPtr calls (REG_N_CALLS_CROSSED == 0 anchor) and global gives cnt the temp's r30.
    cnt = 0;
    if (lo <= 0.0f && hi >= 0.0f) {
        IdUnit* u = IdSys.unitPtr(1, IDC_BINOCULAR);
        u->be_flag |= 8;
        u->texNo = 3;
        u->tex_flag |= 2;
        u->pos0.x = (0.0f - lo) * (m_pos0_R.x - m_pos0_L.x) + m_pos0_L.x;
        cnt = 1;
    }
    if (lo <= 0.5f && hi >= 0.5f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, IDC_BINOCULAR);
        u->be_flag |= 8;
        u->texNo = 0;
        u->tex_flag |= 2;
        u->pos0.x = (0.5f - lo) * (m_pos0_R.x - m_pos0_L.x) + m_pos0_L.x;
        cnt++;
    }
    if (lo <= 1.0f && hi >= 1.0f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, IDC_BINOCULAR);
        u->be_flag |= 8;
        u->texNo = 1;
        u->tex_flag |= 2;
        u->pos0.x = (1.0f - lo) * (m_pos0_R.x - m_pos0_L.x) + m_pos0_L.x;
        cnt++;
    }
    if (lo <= 1.5f && hi >= 1.5f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, IDC_BINOCULAR);
        u->be_flag |= 8;
        u->texNo = 2;
        u->tex_flag |= 2;
        u->pos0.x = (1.5f - lo) * (m_pos0_R.x - m_pos0_L.x) + m_pos0_L.x;
        cnt++;
    }
    if (lo <= 2.0f && hi >= 2.0f) {
        IdUnit* u = IdSys.unitPtr(cnt + 1, IDC_BINOCULAR);
        u->be_flag |= 8;
        u->texNo = 3;
        u->tex_flag |= 2;
        u->pos0.x = (2.0f - lo) * (m_pos0_R.x - m_pos0_L.x) + m_pos0_L.x;
        cnt++;
    }
    // The loop counter is a separate variable copied from cnt (the copy is a no-op after allocation and
    // delays the entry `cmpwi` one slot behind the hoisted `&digit`); it is reused by the digit loop.
    i = cnt;
    while (i <= 2) {
        i++;
        IdSys.unitPtr(i, IDC_BINOCULAR)->be_flag &= ~8;
    }
    if (!StaFlagChk(pG, STA_EVENT)) {
        IdUnit* u = IdSys.unitPtr(0x36, IDC_BINOCULAR);
        MessageControl* mc;
        Message* ms;
        s16 x = (s16) ((u->pos0.x + 320.0f) * 0.8f);
        s16 y = (s16) ((240.0f - u->pos0.y) * 0.8f);
        cMes.setLayout(1, LAYOUT_ACT_BTN);
        mc = &cMes;
        ms = &mc->m_Msg[1];
        mc->MesSet(1, x, (s16) (y - ms->m_font_h / 2), 0x20081, 1, 0, 4);
        u = IdSys.unitPtr(0x1B, IDC_BINOCULAR);
        rate = u->col[3] / 255.0f;
        ms->m_col = ((u8) ((f32) (ms->m_col >> 24) * rate) << 24) |
                    ((u8) ((f32) ((ms->m_col >> 16) & 0xFF) * rate) << 16) |
                    ((u8) ((f32) ((ms->m_col >> 8) & 0xFF) * rate) << 8) |
                    (u8) ((f32) (ms->m_col & 0xFF) * rate);
    }
    {
        f32 t0[2] = {1.0f, 16.0f};
        f32 t1[2] = {45.0f, 3.0f};
        int d;
        dist = (int) ((t0[1] - t0[0]) * (cam->param.fovy - t1[0]) / (t1[1] - t1[0]) + t0[0]);
        d = dist * 10;
        for (i = 0; i < 4; i++) {
            digit[i] = d % 10;
            d /= 10;
        }
        for (int k = 0; k <= 3; k++) {
            IdUnit* u = IdSys.unitPtr(0x20 + k, IDC_BINOCULAR);
            u->tex_flag |= 2;
            u->texNo = digit[k];
        }
        ratio = 1.0f - ((f32) dist - t0[0]) / (t0[1] - t0[0]);
    }
    {
        IdUnit* u = IdSys.unitPtr(0x35, IDC_BINOCULAR);
        u->v0 = ratio;
        u->v1 = 1.0f;
        u->pos0 = m_meter_pos0;
        u->pos0.y = u->pos0.y - m_meter_h0 * ratio * m;
        u->size_H = m_meter_h0 * (1.0f - ratio) * n;
        y = (m_meter_h0 * 0.5f * 0.5f + u->pos0.y) * 2.0f;
    }
    for (int k = 0; k <= 3; k++) {
        IdUnit* u = IdSys.unitPtr(5 + k, IDC_BINOCULAR);
        if (y < u->pos.y) {
            u->be_flag &= ~8;
        } else {
            u->be_flag |= 8;
        }
    }
    IdSys.unitPtr(0x1C, IDC_BINOCULAR)->be_flag &= ~8;
    IdSys.unitPtr(0x1D, IDC_BINOCULAR)->be_flag &= ~8;
    if (cam->param.fovy > m_fovy_old) {
        IdSys.unitPtr(0x1D, IDC_BINOCULAR)->be_flag |= 8;
    } else if (cam->param.fovy < m_fovy_old) {
        IdSys.unitPtr(0x1C, IDC_BINOCULAR)->be_flag |= 8;
    }
    m_fovy_old = cam->param.fovy;
}

// Removes the binocular HUD ids (unit 0x24), releases the pause stop flag and the mask texture.
void IdBinocular::quit(void*)
{
    IdUnit* u = IdSys.unitPtr(0x35, IDC_BINOCULAR);
    int i;

    u->pos0 = m_meter_pos0;
    u->size_H = m_meter_h0;
    u->size_W = m_meter_w0;
    IdSys.kill(0xFF, IDC_BINOCULAR);
    SpfFlagOff(pG, SPF_ACTBTN);
    Cckpt.roomInit();
    if (StaFlagChk(pG, STA_EVENT)) {
        Cckpt.lifeMeterDisp(0);
    }
    {
        MessageControl* mes = &cMes;
        for (i = 0; i <= 0xF; i++) {
            mes->Delete(i);
        }
    }
}

// ---------------------------------------------------------------------------
// CameraPushObject: pushing a heavy object, looks along the push direction.
// ---------------------------------------------------------------------------

CameraPushObject::CameraPushObject()
{
}

// Poisons the object.
CameraPushObject::~CameraPushObject()
{
    memset(this, 9, 0x200);
}

// Camera while the player pushes an object: 2000 behind / 2000 above the player looking at his
// chest, swung 90 degrees to the side when a door / wall / window (ids 0x41 / 0x44 / 0x46) is in
// the way, pulled in front of scenery / characters by cameraHitCheck.
void CameraPushObject::move()
{
    static f32 default_ofs[8] = {0.0f, 2000.0f, -2000.0f, 0.0f, 800.0f, 800.0f, 0.0f, 45.0f};
    Mtx inv;
    Mtx m;
    Mtx rot;
    Vec em_pos;
    Vec near_pos;
    Mtx plmat;
    Vec plpos;
    f32 ofs[8];   // the rotated default_ofs: v[0] at [0], v[1] at [3], roll/fovy at [6]/[7] (one 32-byte slot;
                  // [6]/[7] are stored to the frame and reloaded after the two inv-MultVec calls, not kept in f30/f31)
    cModel* em = 0;
    cModel* e;
    u32 i;

    MTX_COPY(pPL->mat, inv);
    PSMTXInverse(inv, m);
    plmat[0][0] = inv[0][0]; plmat[0][1] = inv[1][0]; plmat[0][2] = inv[2][0];
    plmat[1][0] = inv[0][1]; plmat[1][1] = inv[1][1]; plmat[1][2] = inv[2][1];
    getColumn(inv, 2, (Vec*) plmat[2]);
    plpos.x = inv[0][3]; plpos.y = inv[1][3]; plpos.z = inv[2][3];
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        e = (cModel*) EmMgr.fastAt(i);
        if ((e->id == 0x41 || e->id == 0x44 || e->id == 0x46) && e->isAlive()) {
            PSMTXMultVec(m, &e->pos, &em_pos);
            // negated tests: `blt` / `cror so,eq,gt; bso` (a positive `>=`/`<=` gives cror + bns)
            if (!(em_pos.z < 0.0f) && !(PSVECMag(&em_pos) >= 4000.0f)) {
                if (em == 0) {
                    em = e;
                    near_pos = em_pos;
                } else if (PSVECMag(&em_pos) <= PSVECMag(&near_pos)) {
                    em = e;
                    near_pos = em_pos;
                }
            }
        }
    }
    PSMTXIdentity(rot);
    if (em) {
        Vec look;
        Vec axis = {0.0f, 1.0f, 0.0f};
        if (em->id == 0x41) {
            look.x = ((f32*) em)[0x784 / 4];
            look.y = ((f32*) em)[0x794 / 4];
            look.z = ((f32*) em)[0x7A4 / 4];
        } else {
            look.x = em->mat[0][2];
            look.y = em->mat[1][2];
            look.z = em->mat[2][2];
        }
        // one call; `&look` is a plain call argument pseudo that gcse PREs together with `&hit`
        // (same frame slot) into the head register
        f32 ang = VecAngle(&look, (Vec*) &plmat[2]);
        if (ang > 0.7853982f && ang < 2.3561945f) {
            if (near_pos.x > 0.0f) {
                MtxRotAxisPosRad(rot, &axis, (Vec*) &default_ofs[3], 1.5707964f);
            } else {
                MtxRotAxisPosRad(rot, &axis, (Vec*) &default_ofs[3], -1.5707964f);
            }
        }
    }
    PSMTXMultVec(rot, (Vec*) &default_ofs[0], (Vec*) &ofs[0]);
    PSMTXMultVec(rot, (Vec*) &default_ofs[3], (Vec*) &ofs[3]);
    ofs[6] = default_ofs[6];
    ofs[7] = default_ofs[7];
    PSMTXMultVec(inv, (Vec*) &ofs[0], (Vec*) &ofs[0]);
    PSMTXMultVec(inv, (Vec*) &ofs[3], (Vec*) &ofs[3]);
    param.pos = *(Vec*) &ofs[0];
    param.at = *(Vec*) &ofs[3];
    param.roll = ofs[6];
    param.fovy = ofs[7];
    {
        Vec hit;
        Vec nrm;
        Vec from;
        Vec to;
        from = param.at;
        to = param.pos;
        if (cameraHitCheck(&hit, &nrm, &from, &to)) {
            param.pos = hit;
        }
    }
}

// ---------------------------------------------------------------------------
// CameraLookAt: falling camera, looks at a random hand of the player.
// ---------------------------------------------------------------------------

CameraLookAt::CameraLookAt(Camera* cam)
{
    Vec hit;
    Vec nrm;
    Vec from;
    Vec to;

    if (Rnd() & 1) {
        m_target_parts = pPL->getPartsPtr(1);
    } else {
        m_target_parts = pPL->getPartsPtr(2);
    }
    param.pos = cam->param.pos;
    param.at = cam->param.at;
    param.roll = cam->param.roll;
    param.fovy = cam->param.fovy;
    from = param.at;
    to = param.pos;
    if (cameraHitCheck(&hit, &nrm, &from, &to)) {
        param.pos = hit;
    }
}

// Poisons the object.
CameraLookAt::~CameraLookAt()
{
    memset(this, 9, 0x200);
}

// Keeps the target on the tracked hand parts and pulls the camera in front of anything blocking
// the view (item examine close-up).
void CameraLookAt::move()
{
    Vec hit;
    Vec nrm;
    Vec from;
    Vec to;

    param.at = m_target_parts->world;
    from = param.at;
    to = param.pos;
    if (cameraHitCheck(&hit, &nrm, &from, &to)) {
        param.pos = hit;
    }
}

// ---------------------------------------------------------------------------
// CameraLookDownEm
// ---------------------------------------------------------------------------

CameraLookDownEm::CameraLookDownEm(void* e, Vec* pos)
{
    m_target_parts = ((cModel*) e)->getPartsPtr(2);
    param.pos = *pos;
    param.at = m_target_parts->world;
    param.roll = 0.0f;
    param.fovy = 45.0f;
}

// Poisons the object.
CameraLookDownEm::~CameraLookDownEm()
{
    memset(this, 9, 0x200);
}

// Fixed camera (set in the constructor at `pos` looking at the enemy's chest); nothing per frame.
void CameraLookDownEm::move()
{
}

// The split object pads .sdata to 8 bytes (cam_qfps follows 8-aligned).
asm(".section .sdata; .balign 8");
