// game/filter00: radial/motion blur and contrast filter (D:/Bio4/Prog/filter00.cpp). The frame is
// copied into a persistent half-size buffer and blended back scaled around the centre (blur_type
// 0/1 zoom, 2 tinted), plus an additive "spread" flash (Filter00SetAddSpread: hit/explosion glow
// from a screen point) and a 3-level contrast pass (Filter00SetContrast). Damage / low health /
// explosions set these through Filter00Set*.
#include "filter.h"
#include "gx.h"
#include "global.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"

// Radial blur / contrast filter: copies the frame buffer to a half-size texture and blends it back.
//
// Render: the zero for `col.r = col.g = col.b` is a block-local `int zero = 0` between the
// `filter00_buff` and `flags_500C` tests (its `li` lands before the `andis.`).

#define BLUR_BUFF_SIZE 0x38000
static const int zero = 0;
#define SET_COL(c, R, G, B, A) ((c).r = (R), (c).g = (G), (c).b = (B), (c).a = (A))
#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

static u8 blur_rate = 0x40;
u8 blur_type = 0;
s8 blur_power = 0;
static u8 eff_spread_pri = 0;
int is_eff_spread_on = 0;
static u8 eff_blur_r = 0;
u8 eff_blur_g = 0;
static u8 eff_blur_b = 0;
static u8 eff_blur_rate = 0x80;
static u8 eff_blur_type = 0;
f32 eff_spread_center_x = 0.0f;
f32 eff_spread_center_y = 0.0f;
static f32 eff_spread_pow = 0.0f;
f32 eff_spread_num = 1.0f;
static void* filter00_buff = 0;
u8 g_cont_level = 0;
u8 g_cont_pow = 0;
static u8 g_cont_bias = 0;

void Filter00CommonInit();
void Filter00Render();
void Filter00RenderContrast();

// Resets every blur/spread/contrast parameter to off.
void Filter00CommonInit()
{
    blur_rate = 0;
    blur_type = 0;
    eff_spread_pri = 0;
    is_eff_spread_on = 0;
    eff_spread_center_x = 0.0f;
    eff_spread_center_y = 0.0f;
    eff_spread_pow = 0.0f;
    eff_spread_num = 1.0f;
    g_cont_bias = g_cont_pow = g_cont_level = 0;
}

// Boot: resets and allocates the 0x38000-byte blur buffer (memory group 13).
void Filter00Init()
{
    Filter00CommonInit();
    filter00_buff = 0;
#line 97 "D:/Bio4/Prog/filter00.cpp"
    filter00_buff = MEM_ALLOC(BLUR_BUFF_SIZE, 1, 13);
    if (filter00_buff == 0) {
        pLog->warn(0, 0, "BLUR:not enough memory!!");
        return;
    }
    DCInvalidateRange(filter00_buff, BLUR_BUFF_SIZE);
}

// Room init: parameters off (the buffer stays).
void Filter00RoomInit()
{
    Filter00CommonInit();
}

// Queues Filter00Render in the OT (type 0x12, kind 0x400) when blur is permitted.
void Filter00Trans()
{
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter00Render, 0, 0x400, 0, 0.0f);
    }
}

// OT callback: when blur/spread/contrast is active, blends the previous frame's buffer back over
// the screen (zoom scaled by blur_power/1024 for type 1, tinted for type 2, alpha blur_rate),
// draws the spread flashes expanding from (eff_spread_center_x/y) with eff_spread_pow, then the
// contrast pass, and copies the new frame into the buffer for the next frame (Status_flg[0]
// 0x80000 = buffer valid). Disp_flg 0x100000 disables it.
void Filter00Render()
{
    GXTexObj tex;
    Mtx44 proj;
    Mtx mv;
    GXColor col;
    f32 pow;
    static int blur_scale = 0;
    static int bl[6][4] = {
        { 1, 4, 5, 0 }, { 1, 4, 1, 0 }, { 1, 1, 1, 0 }, { 1, 2, 1, 0 }, { 1, 2, 0, 0 }, { 1, 4, 3, 0 },
    };

    if (DpfFlagChk(pG, DPF_FILTER) || (blur_rate == 0 && is_eff_spread_on == 0 && g_cont_level == 0)) {
        StaFlagOff(pG, STA_BLUR);
        return;
    }
    if (filter00_buff) {
        int zero = 0;

        if (StaFlagChk(pG, STA_BLUR)) {
        col.r = col.g = col.b = zero;
        col.a = blur_rate;
        GXSetTevColor(1, col);
        GXInitTexObj(&tex, filter00_buff, SCR_W / 2, SCR_H / 2, 6, 0, 0, 0);
        GXSetColorUpdate(1);
        GXSetCullMode(0);
        GXSetZMode(0, 7, 0);
        C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -1.0f);
        GXSetProjection(proj, 1);
        PSMTXIdentity(mv);
        GXLoadPosMtxImm(mv, 0);
        GXSetCurrentMtx(0);
        switch (blur_type) {
        case 2:
            GXSetBlendMode(1, 4, 1, 0);
            break;
        case 3:
            GXSetBlendMode(3, 4, 5, 0);
            break;
        case 0:
        case 1:
        default:
            GXSetBlendMode(1, 4, 5, 0);
            break;
        }
        GXSetNumTevStages(1);
        GXSetNumChans(0);
        GXSetNumTexGens(1);
        GXSetTexCoordGen(0, 1, 4, 0x3C);
        GXLoadTexObj(&tex, 0);
        if (blur_type != 2) {
            GXSetTevOrder(0, 0, 0, 0xFF);
            GXSetTevColorIn(0, 15, 15, 15, 8);
            GXSetTevColorOp(0, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(0, 7, 7, 7, 1);
            GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
        } else {
            col.r = col.g = col.b = blur_power;
            col.a = blur_rate;
            GXSetTevColor(1, col);
            GXSetTevOrder(0, 0, 0, 0xFF);
            GXSetTevColorIn(0, 2, 15, 15, 8);
            GXSetTevColorOp(0, 1, 0, blur_scale, 1, 0);
            GXSetTevAlphaIn(0, 7, 7, 7, 1);
            GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
        }
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(13, 1);
        GXSetVtxAttrFmt(0, 9, 0, 2, 0);
        GXSetVtxAttrFmt(0, 13, 1, 4, 0);
        if (blur_rate) {
            GXBegin(0x80, 0, 4);
            switch (blur_type) {
            case 1:
                pow = (f32) blur_power * (1.0f / 1024.0f);
                GXPosition2u16(0, 0);
                GXTexCoord2f32(pow, pow);
                GXPosition2u16(SCR_W, 0);
                GXTexCoord2f32(1.0f - pow, pow);
                GXPosition2u16(SCR_W, SCR_H);
                GXTexCoord2f32(1.0f - pow, 1.0f - pow);
                GXPosition2u16(0, SCR_H);
                GXTexCoord2f32(pow, 1.0f - pow);
                break;
            case 0:
            default:
                GXPosition2u16(0, 0);
                GXTexCoord2f32(0.0f, 0.0f);
                GXPosition2u16(SCR_W, 0);
                GXTexCoord2f32(1.0f, 0.0f);
                GXPosition2u16(SCR_W, SCR_H);
                GXTexCoord2f32(1.0f, 1.0f);
                GXPosition2u16(0, SCR_H);
                GXTexCoord2f32(0.0f, 1.0f);
                break;
            }
        }
        if (is_eff_spread_on) {
            u32 i;

            GXSetTevOrder(0, 0, 0, 0xFF);
            GXSetTevColorIn(0, 15, 8, 2, 15);
            GXSetTevColorOp(0, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(0, 7, 7, 7, 1);
            GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
            for (i = 0; (f32) i < eff_spread_num; i++) {
                f32 cx, cy, cx2, cy2;

                col.r = eff_blur_r;
                col.g = eff_blur_g;
                col.b = eff_blur_b;
                col.a = eff_blur_rate;
                GXSetTevColor(1, col);
                pow = eff_spread_pow;
                GXSetBlendMode(bl[eff_blur_type][0], bl[eff_blur_type][1], bl[eff_blur_type][2], bl[eff_blur_type][3]);
                cx = eff_spread_center_x + 0.5f;
                cy = eff_spread_center_y + 0.5f;
                if (cx < -1.0f) {
                    cx = -1.0f;
                }
                if (cx > 2.0f) {
                    cx = 2.0f;
                }
                if (cy < -1.0f) {
                    cy = -1.0f;
                }
                if (cy > 2.0f) {
                    cy = 2.0f;
                }
                cx2 = 1.0f - cx;
                cy2 = 1.0f - cy;
                GXBegin(0x80, 0, 4);
                GXPosition2u16(0, 0);
                GXTexCoord2f32(pow * cx, pow * cy);
                GXPosition2u16(SCR_W, 0);
                GXTexCoord2f32(1.0f - pow * cx2, pow * cy);
                GXPosition2u16(SCR_W, SCR_H);
                GXTexCoord2f32(1.0f - pow * cx2, 1.0f - pow * cy2);
                GXPosition2u16(0, SCR_H);
                GXTexCoord2f32(pow * cx, 1.0f - pow * cy2);
                is_eff_spread_on = 0;
                eff_spread_pri = 0;
                if (eff_spread_num != 1.0f && (f32) i != eff_spread_num - 1.0f) {
                    GXSetCopyFilter(0, 0, 0, 0);
                    GXSetTexCopySrc(0, 0, SCR_W, SCR_H);
                    GXSetTexCopyDst(SCR_W / 2, SCR_H / 2, 6, 1);
                    GXCopyTex(filter00_buff, 0);
                }
            }
        }
    }
    }
    if (filter00_buff == 0) {
#line 349 "D:/Bio4/Prog/filter00.cpp"
        filter00_buff = MEM_ALLOC(BLUR_BUFF_SIZE, 1, 13);
        if (filter00_buff == 0) {
            pLog->warn(0, 0, "BLUR:not enough memory!!");
            return;
        }
        DCInvalidateRange(filter00_buff, BLUR_BUFF_SIZE);
    }
    GXSetCopyFilter(0, 0, 0, 0);
    GXSetTexCopySrc(0, 0, SCR_W, SCR_H);
    GXSetTexCopyDst(SCR_W / 2, SCR_H / 2, 6, 1);
    GXCopyTex(filter00_buff, 0);
    GXSetCopyFilter(Rmode.aa, Rmode.sample_pattern, 1, Rmode.vfilter);
    Filter00RenderContrast();
    StaFlagOn(pG, STA_BLUR);
}

// Sets the contrast pass: level 0 off / 1..3 strength stages, pow = alpha, bias = colour bias.
void Filter00SetContrast(u8 level, u8 pow, u8 bias)
{
    g_cont_level = level;
    g_cont_pow = pow;
    g_cont_bias = bias;
}

// Sets the blur feedback alpha (0 = blur off).
void Filter00SetAlpha(u8 rate)
{
    blur_rate = rate;
}

// Sets the blur zoom amount (type 1) or tint (type 2).
void Filter00SetPower(s8 power)
{
    blur_power = power;
}

// Selects the blur type 0..2.
void Filter00SetType(u32 type)
{
    if (type > 2) {
        pLog->err(0, 0, "Filter00SetType() : BLUR_TYPE[%d] Invalid.", type);
    } else {
        blur_type = type;
    }
}

// Requests a one-frame additive flash spreading from screen point (cx, cy) (-0.5..0.5) with colour
// r,g,b, alpha rate, `num` passes and growth pow; only when pri is at most the pending priority.
void Filter00SetAddSpread(u32 pri, int on, u8 r, u8 g, u8 b, u8 rate, u8 type, u32 num, f32 cx, f32 cy, f32 pow)
{
    if (eff_spread_pri >= pri) {
        eff_spread_pri = pri;
        is_eff_spread_on = on;
        eff_blur_r = r;
        eff_blur_g = g;
        eff_blur_b = b;
        eff_blur_rate = rate;
        eff_blur_type = type;
        eff_spread_center_x = cx;
        eff_spread_center_y = cy;
        eff_spread_pow = pow;
        eff_spread_num = (f32) num;
    }
}

// Draws the contrast pass (level 1..3 = number of blend passes of the stored frame with bias/pow).
void Filter00RenderContrast()
{
    Mtx44 proj;
    Mtx mv;
    GXTexObj tex;
    GXColor col;

    GXPixModeSync();
    GXInvalidateTexAll();
    GXInitTexObj(&tex, filter00_buff, SCR_W / 2, SCR_H / 2, 6, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    if (g_cont_level == 0) {
        return;
    }
    col.r = col.g = col.b = g_cont_bias;
    col.a = g_cont_pow;
    GXSetTevColor(1, col);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    GXSetZMode(0, 7, 0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -1.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 1, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetTevOrder(0, 0, 0, 0xFF);
    GXSetTevColorIn(0, 15, 2, 12, 8);
    if (g_cont_level == 1) {
        GXSetTevColorOp(0, 1, 0, 0, 1, 0);
    } else if (g_cont_level == 2) {
        GXSetTevColorOp(0, 1, 0, 1, 1, 0);
    } else if (g_cont_level == 3) {
        GXSetTevColorOp(0, 1, 0, 2, 1, 0);
    }
    GXSetTevAlphaIn(0, 7, 7, 7, 1);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 0, 2, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition2u16(0, 0);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition2u16(SCR_W, 0);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition2u16(SCR_W, SCR_H);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition2u16(0, SCR_H);
    GXTexCoord2f32(0.0f, 1.0f);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
