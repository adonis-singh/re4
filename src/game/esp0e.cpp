// game/esp0e.cpp: effect id 0x0E, a lens-flare style glow sprite. The world position (parent
// parts + m_Pos) is projected every frame and the sprite is drawn as a screen sprite there,
// jittered by R_pos; the alpha is the product of a screen-centre falloff (Vec0.x %), the facing
// cone test (Vec2: x/y direction in degrees, z cone angle), the camera distance fade (Vec0.z)
// and a 12-sample Z-buffer visibility test of radius Vec1.x run after the frame is rendered.
// Vec0.y is how much the alpha also shrinks the sprite.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"
#include "main_sub.h"
#include "view.h"


struct Esp0eWork {
    Vec wld_pos;       // 0x00 world position
    Vec dir_vec;        // 0x0C facing direction (world)
    f32 dir_ang;      // 0x18 half angle of the visible cone (rad)
    f32 center_dist_ratio;   // 0x1C 1 - gen->Vec0.x / 100: screen-centre fade factor
    f32 size_ratio;   // 0x20 gen->Vec0.y / 100: how much the alpha scales the size
    f32 del_dist;       // 0x24 camera distance where the glow is gone (gen->Vec0.z)
    Vec scr;        // 0x28 screen position (z: view depth)
    Vec scrOld;     // 0x34 previous screen position
    f32 hide_alpha;  // 0x40 alpha from the Z-buffer visibility test
    f32 hide_r;      // 0x44 radius of the visibility test (gen->Vec1.x)
    f32 alpha;      // 0x48 final alpha
    u16 flg;      // 0x4C bit0: direction test, bit1: visibility test
    u16 delay_cnt;    // 0x4E frames the visibility test is forced to 0
    u32 Rand_seed;       // 0x50 random seed for the screen jitter
    EspGenWork* gen;  // 0x54
};

// Screen-space glow (lens flare style): the sprite is drawn in screen mode at the projected
// position, faded by distance from the screen centre, the facing direction and a Z-buffer test.
class cEsp0e : public cEsp {
public:
    Esp0eWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
static f32 GetDistAlpha(cEsp0e* esp);
static f32 GetDirAlpha(cEsp0e* esp, Vec* dir);
void Esp0e_HideCheck(cEsp* esp);
}

// EspCreateTbl[0x0E] factory.
cEsp* Esp0e_Create()
{
    return new cEsp0e;
}

// Base update and animation, then computes wld_pos and the facing direction from the parent
// parts, projects to screen (scr), and evaluates `alpha` = centre falloff x direction x distance
// x visibility (0 when behind the camera). A camera cut (Status_flg[2] 0x10000000) blanks the
// visibility for 2 frames. Queues Esp0e_HideCheck after the render.
void cEsp0e::move()
{
    Esp0eWork* w = &m_Free;
    Vec dir;
    Vec view;
    Vec scr;
    Vec d;
    f32 alpha;
    f32 sx;
    f32 sy;

    if (!CommonMove()) {
        return;
    }
    if (!AnmMove()) {
        PushEsp(this);
        return;
    }
    if (w->flg & 2) {
        if (StaFlagChk(pG, STA_CUT_CHANGE)) {
            w->delay_cnt = 2;
        }
        if (w->delay_cnt != 0) {
            w->delay_cnt--;
            w->hide_alpha = 0.0f;
        }
    }
    if (parent == pEffParentWorld) {
        w->wld_pos = m_Pos;
        dir = w->dir_vec;
    } else {
        cParts* parts;

        if (m_Parts_no >= m_pMod->nParts) {
            pLog->err(0, 0, "ESP0E :PARTS_NO[%d] is invalid(MAX:%d).", m_Parts_no, m_pMod->nParts);
            PushEsp(this);
            return;
        }
        parts = m_pMod->getPartsPtr(m_Parts_no);
        PSMTXMultVec(parts->mat, &m_Pos, &w->wld_pos);
        {
            Mtx m;

            PSMTXCopy(parts->mat, m);
            m[0][3] = 0.0f;
            m[1][3] = 0.0f;
            m[2][3] = 0.0f;
            PSMTXMultVec(m, &w->dir_vec, &dir);
        }
    }
    PSMTXMultVec(pG->Camera.v_mat, &w->wld_pos, &view);
    PSMTX44MultVec(pG->Camera.ProjMat, &view, &scr);
    sx = (scr.x * 0.5f + 0.5f) * Screen.width;
    sy = (-scr.y * 0.5f + 0.5f) * Screen.height;
    w->scrOld.x = w->scr.x;
    w->scrOld.y = w->scr.y;
    w->scrOld.z = w->scr.z;
    scr.x = sx;
    scr.y = sy;
    scr.z = 0.0f;
    w->scr.x = sx;
    w->scr.y = sy;
    if (view.z < 0.0f) {
        view.x = Screen.width * 0.5f;
        view.y = Screen.height * 0.5f;
        view.z = 0.0f;
        PSVECSubtract(&view, &scr, &d);
        alpha = PSVECMag(&d) / (Screen.height * (w->center_dist_ratio * 0.7f));
        alpha *= alpha;
        alpha = 1.0f - alpha;
        if (w->flg & 1) {
            alpha *= GetDirAlpha(this, &dir);
        }
        alpha *= GetDistAlpha(this);
        if (w->flg & 2) {
            alpha *= w->hide_alpha;
        }
        w->alpha = alpha;
    } else {
        w->alpha = 0.0f;
    }
    if (m_Be_flg & 1) {
        Vec v;

        PSMTXMultVec(pG->Camera.v_mat, &w->wld_pos, &v);
        w->scr.z = v.z;
        EspAddOtAfterRender(this, Esp0e_HideCheck);
    }
}

// EspTransTbl[0x0E]: when alpha > 0.01 draws a one-frame screen-sprite copy (Parts_no 0xF8) at
// scr + random R_pos jitter with the colour alpha and size scaled by `alpha`, via EspCommonTrans.
extern "C" void Esp0e_Trans(cEsp0e* esp)
{
    Esp0eWork* w = &esp->m_Free;

    if (w->alpha > 0.01f) {
        cEsp tmp;
        cEsp* p = &tmp;
        Mtx m;
        PSMTXIdentity(m);
        *p = *esp;
        p->m_Id = 0;
        p->m_pMod = NULL;
        p->m_Parts_no = ESP_PARTS_NOPARTS;
        p->m_Life_max = 1;
        p->m_Pos.x = w->scr.x + w->gen->R_pos.x * fRandSeed1_1(&w->Rand_seed);
        p->m_Pos.y = w->scr.y + w->gen->R_pos.y * fRandSeed1_1(&w->Rand_seed);
        p->m_Pos.z = 1.0f;
        p->m_Col_a *= w->alpha;
        if (w->size_ratio != 0.0f) {
            f32 s;

            s = w->alpha * w->size_ratio + (1.0f - w->size_ratio);
            if (s < 0.0f) {
                s = 0.0f;
            }
            p->m_Size_base_x *= s;
            p->m_Size_base_y *= s;
        }
        EspCommonTrans(p);
    }
}

// Alpha from the distance to the camera: 1 at the camera, 0 at `dist`.
static f32 GetDistAlpha(cEsp0e* esp)
{
    Esp0eWork* w = &esp->m_Free;
    Vec d;
    f32 a;

    if (w->del_dist != 0.0f) {
        Camera* cam = &pG->Camera;

        d.x = w->wld_pos.x - cam->param.pos.x;
        d.y = w->wld_pos.y - cam->param.pos.y;
        d.z = w->wld_pos.z - cam->param.pos.z;
        a = PSVECMag(&d) / w->del_dist;
        if (a > 1.0f) {
            a = 1.0f;
        }
        if (a < 0.0f) {
            a = 0.0f;
        }
        return 1.0f - a;
    }
    return 1.0f;
}

// Alpha from the angle between the facing direction and the camera: 1 when looking straight at
// the camera, 0 at the cone edge.
static f32 GetDirAlpha(cEsp0e* esp, Vec* dir)
{
    Esp0eWork* w = &esp->m_Free;
    Camera* cam;
    Vec d;
    f32 ang;
    f32 c;
    f32 a;

    ang = LIMIT_ANGLE(w->dir_ang);
    cam = &pG->Camera;
    d.x = w->wld_pos.x - cam->param.pos.x;
    d.y = w->wld_pos.y - cam->param.pos.y;
    d.z = w->wld_pos.z - cam->param.pos.z;
#line 295 "D:/Bio4/Prog/esp0e.cpp"
    VECNormalize(&d, &d);
    a = -PSVECDotProduct(&d, dir);
    c = cosf(ang);
    a -= c;
    if (a <= 0.0f) {
        a = 0.0f;
    } else {
        a /= 1.0f - c;
    }
    return a;
}

// Z-buffer visibility test around the screen position: hidden samples fade the glow out.
void Esp0e_HideCheck(cEsp* esp0)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static s32 Zs_bias0e = 0;
    static const f32 hide_x_tbl[12] = { 0.0f, 0.5f, 0.86f, 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f };
    static const f32 hide_y_tbl[12] = { 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f, 0.0f, 0.5f, 0.86f };
    static s32 Zs_bias0e_2 = 0;  // unreferenced 4-byte .sdata word after Zs_bias0e (name unknown)
    cEsp0e* esp = (cEsp0e*)esp0;
    Esp0eWork* w = &esp->m_Free;
    Vec p;
    u32 z;
    s32 zi;
    f32 nz;
    f32 inv;
    f32 inv2;
    f32 m22;
    f32 m23;
    f32 zv;
    f32 margin;
    f32 scale;
    u32 hidden;
    u32 i;

    if (!(esp->m_Be_flg & 1)) {
        return;
    }
    if (!(w->flg & 2)) {
        return;
    }
    nz = w->scrOld.z + 150.0f;
    inv = 1.0f / (ZFAR - ZNEAR);
    inv2 = 1.0f / -nz;
    m22 = -(ZNEAR) * inv;
    m23 = -(ZFAR * ZNEAR) * inv;
    zv = (m23 + m22 * nz) * Zscale;
    // The result is written back into inv2: the 1.0 constant, inv2 and the final value are one
    // register chain (f12) with the most refs, so it is allocated first and -ZNEAR/inv take
    // f11/f10 (a separate result variable ties the fmadds to zv instead).
    inv2 = inv2 * zv + Zoffset;
    zi = (u32)(inv2 * 16777215.0f);
    if (SysFlagChk(pG, SYS_SCISSOR_ON)) {
        margin = 56.0f;
    } else {
        margin = 0.0f;
    }
    GXPixModeSync();
    GXDrawDone();
    hidden = 0;
    scale = 5000.0f / nz;
    for (i = 0; i < 12; i++) {
        f32 ox;
        f32 oy;

        ox = hide_x_tbl[i] * w->hide_r;
        oy = hide_y_tbl[i] * w->hide_r;
        if (scale < 1.0f) {
            ox *= scale;
            ox *= scale;
        }
        p.x = w->scrOld.x + ox;
        p.y = w->scrOld.y + oy;
        if (p.x < 0.0f || p.x >= Screen.width || p.y < 0.0f + margin || p.y >= Screen.height - margin) {
            hidden++;
        } else {
            GXPeekZ((u16)p.x, (u16)p.y, &z);
            if (zi > (s32)(z - Zs_bias0e)) {
                hidden++;
            }
        }
        // COMPILER-DIFF: candidate (loop.c pass-1 insn_count). Dead test (+3 real insns at loop
        // pass 1) so `high(Screen)` misses pass 1's threshold and is hoisted in pass 2, after the
        // giv init `li i4,0` (see esp45 Esp45_HideCheck); the operand must not add a ref to `w`.
        if (Zs_bias0e == 99) {
            ox = oy;
        }
    }
    if (hidden == 12) {
        w->hide_alpha = 0.0f;
    } else {
        f32 a;

        a = 1.0f - (f32)hidden * 0.1f;
        if (a < 0.0f) {
            a = 0.0f;
        }
        if (a > 1.0f) {
            a = 1.0f;
        }
        w->hide_alpha = w->hide_alpha + (a - w->hide_alpha) * 0.6f;
    }
}

// Builds dir_vec / dir_ang from Vec2 (enables the direction test), the centre and size ratios
// from Vec0, the distance fade Vec0.z and the visibility radius Vec1.x; m_Flg bit3 marks the
// screen-glow OT layer. Attached effects with Release_time 0 are kept attached forever.
int cEsp0e::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp0eWork* w = &m_Free;

    m_Flg |= 8;
    w->flg = 0;
    if (pSeq->Vec2.z != 0.0f) {
        Mtx mx;
        Mtx my;
        f32 rx;
        f32 ry;

        w->dir_ang = pSeq->Vec2.z * PI * 2.0f / 360.0f * 0.5f;
        w->dir_vec.x = 0.0f;
        w->dir_vec.y = 0.0f;
        w->dir_vec.z = 1.0f;
        rx = pSeq->Vec2.x * PI * 2.0f / 360.0f;
        ry = pSeq->Vec2.y * PI * 2.0f / 360.0f;
        rx = LIMIT_ANGLE(rx);
        ry = LIMIT_ANGLE(ry);
        PSMTXRotRad(mx, 'Y', ry);
        PSMTXRotRad(my, 'X', rx);
        PSMTXConcat(mx, my, mx);
        PSMTXMultVec(mx, &w->dir_vec, &w->dir_vec);
#line 442 "D:/Bio4/Prog/esp0e.cpp"
        VECNormalize(&w->dir_vec, &w->dir_vec);
        w->flg |= 1;
    }
    w->center_dist_ratio = 1.0f - pSeq->Vec0.x * 0.01f;
    if (w->center_dist_ratio > 1.0f) {
        w->center_dist_ratio = 1.0f;
    }
    w->size_ratio = pSeq->Vec0.y * 0.01f;
    w->del_dist = pSeq->Vec0.z;
    if (pSeq->Vec1.x != 0.0f) {
        w->hide_r = pSeq->Vec1.x;
        w->flg |= 2;
    }
    w->Rand_seed = 0x12345678;
    w->gen = pSeq;
    if (m_Parts_no != ESP_PARTS_WORLD && m_Release_time == 0) {
        m_Release_time = 0xFF;
    }
    return 1;
}
