// game/esp45.cpp: effect id 0x45, a lens glow / light bloom. Nothing is drawn as geometry: the
// effect's world (or screen) position is projected and handed to Filter00 as an additive radial
// spread of the effect colour, with power m_Size_plus and spread type Work8[0]. The alpha is
// reduced with camera distance (Vec0.z) and, when Vec1.x is set, by a 24-sample Z-buffer
// visibility test around the screen position run after the frame is rendered.

#include "atari.h"
#include "global.h"
#include "esp.h"
#include "main_sub.h"
#include "filter.h"
#include "cam_ctrl.h"
#include "gx.h"
#include "view.h"


struct Esp45Work {
    Vec wld_pos;       // 0x00 world position
    f32 pos_x;         // 0x0C screen position x
    f32 pos_y;         // 0x10 screen position y
    u8 type;        // 0x14 gen->Work8[0]: Filter00 spread type
    u8 alpha;       // 0x15 colour alpha as a byte
    u8 rate;        // 0x16 gen->Blend_type
    u8 pad_17;
    f32 power;      // 0x18 scaleSpd: spread power
    f32 sz;         // 0x1C view depth
    Vec scrOld;     // 0x20 previous screen position (z: view depth)
    f32 hide_alpha;  // 0x2C alpha from the Z-buffer visibility test
    f32 hide_r;      // 0x30 radius of the visibility test (gen->Vec1.x)
    u8 pad_34[4];
    u16 flg;      // 0x38 bit1: visibility test
    u16 delay_cnt;    // 0x3A
    f32 del_dist;       // 0x3C camera distance where the glow is gone (gen->Vec0.z)
};

// Additive radial blur (Filter00 spread) at the projected position, faded by camera distance and
// a Z-buffer visibility test.
class cEsp45 : public cEsp {
public:
    Esp45Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
cEsp* Esp45_Create();
void Esp45_Trans(cEsp* esp);
static f32 GetDistAlpha(cEsp45* esp);
void Esp45_HideCheck(cEsp* esp);
}

// EspCreateTbl[0x45] factory.
cEsp* Esp45_Create()
{
    return new cEsp45;
}

// Base update with the scale fade disabled; latches alpha and power, records the view depth of
// the world position and queues Esp45_HideCheck in the after-render OT.
void cEsp45::move()
{
    Esp45Work* w = &m_Free;

    m_Size_mul = 1.0e22f;
    if (!CommonMove()) {
        return;
    }
    w->alpha = (u8) m_Col_a;
    w->power = m_Size_plus;
    if (m_Be_flg & 1) {
        Vec v;

        PSMTXMultVec(pG->Camera.v_mat, &w->wld_pos, &v);
        w->sz = v.z;
        EspAddOtAfterRender(this, Esp45_HideCheck);
    }
}

// EspTransTbl[0x45]: screen sprites feed their pixel position straight to Filter00SetAddSpread;
// world sprites compute wld_pos (parent parts matrix), project it, scale the alpha by the
// visibility and distance factors, call the filter and remember the screen position for the
// next visibility test.
void Esp45_Trans(cEsp* pEsp)
{
    cEsp45* esp = (cEsp45*) pEsp;
    Esp45Work* w = &esp->m_Free;

    if (esp->m_Parts_no >= ESP_PARTS_SCR_NO_END && esp->m_Parts_no <= ESP_PARTS_SCR_NO_START) {
        f32 cx = esp->m_Pos.x * 0.001953125f - 0.5f;
        f32 cy = esp->m_Pos.y * 0.001953125f - 0.5f;
        Filter00SetAddSpread(w->type, 1, (u8) esp->m_Col_r, (u8) esp->m_Col_g, (u8) esp->m_Col_b, w->alpha, w->rate, 1,
                             cx, cy, w->power);
    } else {
        Vec view;
        Vec scr;
        f32 cx;
        f32 cy;
        int a;
        f32 sx;
        f32 sy;

        if (esp->parent == pEffParentWorld) {
            w->wld_pos = esp->m_Pos;
        } else {
            if (esp->m_Parts_no >= esp->m_pMod->nParts) {
                pLog->err(0, 0, "ESP45 :PARTS_NO[%d] is invalid(MAX:%d).", esp->m_Parts_no, esp->m_pMod->nParts);
                PushEsp(esp);
                return;
            }
            PSMTXMultVec(esp->m_pMod->getPartsPtr(esp->m_Parts_no)->mat, &esp->m_Pos, &w->wld_pos);
        }
        PSMTXMultVec(pG->Camera.v_mat, &w->wld_pos, &view);
        PSMTX44MultVec(pG->Camera.ProjMat, &view, &scr);
        scr.z = 0.0f;
        cx = scr.x * 0.5f;
        cy = scr.y * -0.5f;
        a = w->alpha;
        if (w->flg & 2) {
            a = (u8) ((f32) a * w->hide_alpha);
        }
        a = (u8) ((f32) a * GetDistAlpha(esp));
        Filter00SetAddSpread(w->type, 1, (u8) esp->m_Col_r, (u8) esp->m_Col_g, (u8) esp->m_Col_b, a, w->rate, 1, cx, cy,
                             w->power);
        PSMTX44MultVec(pG->Camera.ProjMat, &view, &scr);
        sx = (scr.x * 0.5f + 0.5f) * Screen.width;
        sy = (-scr.y * 0.5f + 0.5f) * Screen.height;
        scr.z = 0.0f;
        w->scrOld.x = w->pos_x;
        w->scrOld.y = w->pos_y;
        w->scrOld.z = w->sz;
        scr.x = sx;
        scr.y = sy;
        w->pos_x = sx;
        w->pos_y = sy;
    }
}

// Alpha from the distance to the camera: 1 at the camera, 0 at `dist`.
static f32 GetDistAlpha(cEsp45* esp)
{
    Esp45Work* w = &esp->m_Free;
    Vec d;
    f32 a;

    if (w->del_dist != 0.0f) {
        CAMERA* cam = &pG->Camera;

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

// Z-buffer visibility test around the screen position: hidden samples fade the glow out.
void Esp45_HideCheck(cEsp* esp0)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static s32 Zs_bias45 = 0;
    static const f32 hide_x_tbl[12] = { 0.0f, 0.5f, 0.86f, 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f };
    static const f32 hide_y_tbl[12] = { 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f, 0.0f, 0.5f, 0.86f };
    static s32 Zs_bias45_2 = 0;  // unreferenced 4-byte .sdata word after Zs_bias45 (name unknown)
    cEsp45* esp = (cEsp45*) esp0;
    Esp45Work* w = &esp->m_Free;
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

    if (!(w->flg & 2)) {
        return;
    }
    nz = w->scrOld.z + 150.0f;
    inv = 1.0f / (ZFAR - ZNEAR);
    inv2 = 1.0f / -nz;
    m22 = -(ZNEAR) * inv;
    m23 = -(ZFAR * ZNEAR) * inv;
    zv = (m23 + m22 * nz) * Zscale;
    zi = (u32) ((inv2 * zv + Zoffset) * 16777215.0f);
    if (SysFlagChk(pG, SYS_SCISSOR_ON)) {
        margin = 56.0f;
    } else {
        margin = 0.0f;
    }
    GXPixModeSync();
    GXDrawDone();
    hidden = 0;
    scale = 5000.0f / nz;
    for (i = 0; i < 24; i++) {
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
            GXPeekZ((u16) p.x, (u16) p.y, &z);
            if (zi > (s32) (z - Zs_bias45)) {
                hidden++;
            }
        }
        // COMPILER-DIFF: candidate (loop.c pass-1 insn_count). Dead test (+3 real insns: the store
        // goes at flow, compare/branch at jump2): the original's loop had >= 60 real insns at loop
        // pass 1, so `high(Screen)` (savings 1, life 1, threshold 71 - 3 per moved movable = 59)
        // was not hoisted until pass 2 and its `lis` lands AFTER pass 1's giv init `li i4,0`.
        if (w->flg == 99) {
            ox = oy;
        }
    }
    if (hidden == 24) {
        w->hide_alpha = 0.0f;
    } else {
        f32 a;

        a = 2.4f - (f32) hidden * 0.1f;
        if (a < 0.0f) {
            a = 0.0f;
        }
        if (a > 1.0f) {
            a = 1.0f;
        }
        w->hide_alpha = w->hide_alpha + (a - w->hide_alpha) * 0.6f;
    }
    if (CamCtrl.IsChangeCamera()) {
        w->delay_cnt = 2;
    }
    if (w->delay_cnt != 0) {
        w->delay_cnt--;
        w->hide_alpha = 0.0f;
    }
}

// Spread type Work8[0], rate Blend_type, distance fade Vec0.z, visibility test radius Vec1.x
// (enables flg bit1).
int cEsp45::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp45Work* w = &m_Free;

    w->type = pSeq->Work8[0];
    w->rate = pSeq->Blend_type;
    w->alpha = (u8) m_Col_a;
    w->power = m_Size_plus;
    w->del_dist = pSeq->Vec0.z;
    if (pSeq->Vec1.x != 0.0f) {
        w->hide_r = pSeq->Vec1.x;
        w->flg |= 2;
    }
    return 1;
}
