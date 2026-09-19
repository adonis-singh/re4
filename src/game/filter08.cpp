// game/filter08: aiming zoom blur (D:/Bio4/Prog/filter08.cpp). While Status_flg[3] 0x08000000 (the
// weapon's scope/zoom view) is set, filter08_ratio eases to 1 and the frame is blurred outwards from
// the weapon marker position (g_cx/g_cy, jittered) by feedback copies; the last pass blends a tinted
// (sr/sg/sb) mono copy back. Eases out when the flag drops.
#include "filter.h"
#include "light.h"
#include "atari.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"
#include "global.h"
#include "camera.h"
#include "rnd.h"
#include "player.h"
#include "pl_wep.h"

// Zoom-blur filter centred on the weapon marker (filter08_ratio fades it in/out); the last pass
// blends a tinted (sr/sg/sb[ptn]) I8 copy back over the frame.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;

extern "C" {
void Filter08GetEFB(int div, int div2, int mip, int mode);
static void Filter08Render();
void setCoord(f32 s, f32 t, f32 su, f32 sv);
void Filter08GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 alpha2, f32 alpha3, f32 s, int div, int mode);
void Filter08DrawBuffer();
}

static void* filter08_buff = 0;
f32 filter08_ratio = 0.0f;
static f32 g_cx = 0.5f;
f32 g_cy = 0.5f;
static f32 g_cx2 = 0.5f;
f32 g_cy2 = 0.5f;

// Boot: same as the room init.
void Filter08Init()
{
    Filter08RoomInit();
}

// Room init: ratio 0, centre at the screen middle.
void Filter08RoomInit()
{
    filter08_buff = 0;
    filter08_ratio = 0.0f;
    g_cx = 0.5f;
    g_cy = 0.5f;
    g_cx2 = 0.5f;
    g_cy2 = 0.5f;
}

// Per-frame: eases filter08_ratio towards 1 (zoom on) or 0 (off; skipped below 0.01), tracks the
// weapon marker screen position with random jitter, and queues Filter08Render.
void Filter08Trans()
{
    static int use_filter8 = 1;
    static f32 filter08_rnd_dist = 0.01f;
    Vec pos;
    cPlayer* pl = pPL;

    if (use_filter8 == 0) {
        return;
    }
    if (!StaFlagChk(pG, STA_SLOW)) {
        filter08_ratio -= filter08_ratio * 0.6f;
        if (filter08_ratio < 0.01f) {
            return;
        }
    } else {
        filter08_ratio += (1.0f - filter08_ratio) * 0.25f;
    }
    if (pl->Wep->getMarkerPos(&pos)) {
        PSMTXMultVec(pG->Cam.v_mat, &pos, &pos);
        PSMTX44MultVec(pG->Cam.ProjMat, &pos, &pos);
        g_cx2 = pos.x * 0.5f * 1.2f + 0.5f;
        g_cy2 = -pos.y * 0.5f * 0.9f + 0.5f;
    } else {
        g_cx2 = 0.5f;
        g_cy2 = 0.5f;
    }
    g_cx2 += fRand1_1() * filter08_rnd_dist;
    g_cy2 += fRand1_1() * filter08_rnd_dist;
    g_cx += (g_cx2 - g_cx) * 0.3f;
    g_cy += (g_cy2 - g_cy) * 0.3f;
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter08Render, 7, 0x400, 0, 0.0f);
    }
}

// Copies the frame at 1/div x 1/div2 into the buffer (mode 1: I8 mono copy).
void Filter08GetEFB(int div, int div2, int mip, int mode)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[8] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32, 0 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    if (mode == 1) {
        GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 1, mip);
    } else {
        GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 6, mip);
    }
    GXCopyTex(filter08_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

// OT callback: runs the zoom blur passes without scissor and restores the fog.
static void Filter08Render()
{
    GXColor col = { 0, 0, 0, 0 };

    SetNoScissor();
    filter08_buff = GetDrawTmpBufAddr(11);
    if (filter08_buff == 0) {
        pLog->warn(0, 0, "Filter08Trans() : not enough memory");
        return;
    }
    DCInvalidateRange(filter08_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    Filter08DrawBuffer();
    LightMgr.setFog();
    SetScissorState();
}

// Emits one texture coordinate scaled away from the blur centre by (su, sv).
void setCoord(f32 s, f32 t, f32 su, f32 sv)
{
    GXTexCoord2f32((g_cx - s) * su + s, (g_cy - t) * sv + t);
}

u8 ptn = 1;
static u8 sr[5] = { 0xFF, 0xB9, 0, 0, 0 };
static u8 sg[5] = { 0xEB, 0xE1, 0, 0, 0 };
static u8 sb[5] = { 0xD7, 0xFF, 0, 0, 0 };

// Draws the buffer as a fan of 5 quads around the blur centre with three alpha rings (alpha,
// alpha2, alpha3); mode 1 tints with the sr/sg/sb palette entry.
void Filter08GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 alpha2, f32 alpha3, f32 s, int div, int mode)
{
    GXTexObj tex;
    GXColor mat;
    u8 r;
    u8 g;
    u8 b;
    f32 sw;
    f32 sh;

    if (mode == 1) {
        u8 c;

        c = sr[ptn];
        mat.r = c;
        r = c;
        c = sg[ptn];
        mat.g = c;
        g = c;
        c = sb[ptn];
        mat.b = c;
        b = c;
        mat.a = 0xFF;
        GXInitTexObj(&tex, filter08_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 1, 0, 0, 0);
    } else {
        mat.r = 0xFF;
        r = 0xFF;
        mat.g = 0xFF;
        g = 0xFF;
        mat.b = 0xFF;
        b = 0xFF;
        mat.a = 0xFF;
        GXInitTexObj(&tex, filter08_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 6, 0, 0, 0);
    }
    GXSetChanMatColor(4, mat);
    GXSetChanAmbColor(4, mat);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    sw = (f32) SCR_W / s;
    sh = (f32) SCR_H / s;
    GXBegin(0xA0, 0, 6);
    GXPosition3f32(x + sw * 0.5f, y + sh * 0.5f, z);
    GXColor4u8(r, g, b, (u8) alpha3);
    setCoord(0.5f, 0.5f, u, v);
    GXPosition3f32(x + sw * 0.25f, y + sh * 0.25f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.25f, 0.25f, u, v);
    GXPosition3f32(x + sw * 0.75f, y + sh * 0.25f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.75f, 0.25f, u, v);
    GXPosition3f32(x + sw * 0.75f, y + sh * 0.75f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.75f, 0.75f, u, v);
    GXPosition3f32(x + sw * 0.25f, y + sh * 0.75f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.25f, 0.75f, u, v);
    GXPosition3f32(x + sw * 0.25f, y + sh * 0.25f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.25f, 0.25f, u, v);
    GXBegin(0x98, 0, 10);
    GXPosition3f32(x + sw * 0.25f, y + sh * 0.25f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.25f, 0.25f, u, v);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(r, g, b, (u8) alpha);
    setCoord(0.0f, 0.0f, u, v);
    GXPosition3f32(x + sw * 0.75f, y + sh * 0.25f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.75f, 0.25f, u, v);
    GXPosition3f32(x + sw, y + 0.0f, z);
    GXColor4u8(r, g, b, (u8) alpha);
    setCoord(1.0f, 0.0f, u, v);
    GXPosition3f32(x + sw * 0.75f, y + sh * 0.75f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.75f, 0.75f, u, v);
    GXPosition3f32(x + sw, y + sh, z);
    GXColor4u8(r, g, b, (u8) alpha);
    setCoord(1.0f, 1.0f, u, v);
    GXPosition3f32(x + sw * 0.25f, y + sh * 0.75f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.25f, 0.75f, u, v);
    GXPosition3f32(x + 0.0f, y + sh, z);
    GXColor4u8(r, g, b, (u8) alpha);
    setCoord(0.0f, 1.0f, u, v);
    GXPosition3f32(x + sw * 0.25f, y + sh * 0.25f, z);
    GXColor4u8(r, g, b, (u8) alpha2);
    setCoord(0.25f, 0.25f, u, v);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(r, g, b, (u8) alpha);
    setCoord(0.0f, 0.0f, u, v);
}

// The zoom passes: full copy, nFeedLp08 feedback passes with offsets px/py * ratio, then the mono
// tinted blend with mono_alpha* scaled by filter08_ratio.
void Filter08DrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    f32 y = 0.0f;
    f32 z;
    u32 i;
    static f32 px = 0.03f;
    static f32 py = 0.03f;
    static f32 flip = 1.0f;
    static u8 alpha = 0x40;
    static u8 alpha2 = 0x20;
    static u8 alpha3 = 0;
    static u8 flt08_blend_type = 0;
    static u8 flt08_blend_add = 1;
    static u32 nFeedLp08 = 1;
    static u32 flt08_dither_on_lp = 0;
    static u8 mono_alpha = 0x90;
    static u8 mono_alpha2 = 0x30;
    static u8 mono_alpha3 = 0x18;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 0, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 0, 1, 0, 2, 1);
    GXColor amb = { 0, 0, 0, 0xFF };
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 15, 15, 8);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXSetZMode(0, 7, 1);
    y = 0.0f;
    z = 65530.0f;
    Filter08GetEFB(1, 1, 1, 0);
    GXSetBlendMode(1, 4, 5, 0);
    Filter08GXDraw(y, y, z, y, y, 255.0f, 255.0f, 255.0f, 2.0f, 1, 0);
    GXSetDither(0);
    if (flt08_blend_type == 0) {
        GXSetBlendMode(1, 4, 5, 0);
    } else {
        GXSetBlendMode(1, 4, 1, 0);
    }
    for (i = 0; i < nFeedLp08; i++) {
        if (i >= flt08_dither_on_lp) {
            GXSetDither(1);
        }
        if (flt08_blend_add != 0 && i >= nFeedLp08 - flt08_blend_add) {
            GXSetBlendMode(1, 4, 1, 0);
        }
        Filter08GetEFB(2, 1, 0, 0);
        Filter08GXDraw(y, y, z, px * flip, py * flip, (f32) (int) alpha * filter08_ratio,
                       (f32) (int) alpha2 * filter08_ratio, (f32) (int) alpha3 * filter08_ratio, 2.0f, 1, 0);
    }
    GXSetBlendMode(1, 4, 5, 0);
    Filter08GetEFB(2, 1, 0, 0);
    Filter08GXDraw(y, y, z, px * filter08_ratio, py * filter08_ratio, 255.0f, 255.0f, 255.0f, 1.0f, 1, 0);
    GXSetTevColorIn(0, 15, 10, 8, 15);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    Filter08GetEFB(1, 1, 1, 1);
    GXSetBlendMode(1, 4, 1, 0);
    Filter08GXDraw(y, y, z, px * flip, py * flip, (f32) (int) mono_alpha * filter08_ratio,
                   (f32) (int) mono_alpha2 * filter08_ratio, (f32) (int) mono_alpha3 * filter08_ratio, 1.0f, 1, 1);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
