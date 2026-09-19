// game/filter03: alpha glow filter (D:/Bio4/Prog/filter03.cpp). The frame's alpha channel (written by
// glowing surfaces) is copied to quarter-size textures, blurred by feedback passes and blended over
// the screen in the requested colour. Rooms/enemies request it with Filter03SetParam (priority
// ordered, 0xFF locks it); flag sets Status_flg[1] bit 0x80 while active.
#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"
#include "global.h"

// Glow filter: the frame buffer's alpha is copied to quarter-size textures, blurred by drawing it
// back with pixel offsets, then blended over the screen with the requested color.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

struct Flt03Work {
    int on;      // 0x00
    u32 level;   // 0x04  blur spread (level_tbl3 rows)
    u32 loop;    // 0x08  feedback passes
    u8 r;        // 0x0C
    u8 g;        // 0x0D
    u8 b;        // 0x0E
    u8 pri;      // 0x0F  0xFF = locked
    int flag;    // 0x10  sets pG->flags_5010 bit 0x80 while active
};

extern f32 ZNEAR;
extern f32 ZFAR;

void* filter03_buff = 0;
Flt03Work flt03;

static const f32 level_tbl3[32] = { 0.008f, 0.008f, 0.008f, 0.008f, 0.008f, 0.016f, 0.008f, 0.016f };

extern "C" {
void Filter03Render();
void Filter03GetEFB(int div, int div2);
void Filter03GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, u8 r, u8 g, u8 b, u8 a, f32 s, int div, int fmt);
static void Filter03DrawBuffer();
}

// Boot: forgets the buffer.
void Filter03Init()
{
    filter03_buff = 0;
}

// Room init: glow off, default level 1 / 2 passes / orange colour.
void Filter03RoomInit()
{
    filter03_buff = 0;
    memclr_asm(&flt03, sizeof(Flt03Work));
    flt03.level = 1;
    flt03.loop = 2;
    flt03.r = 0xFF;
    flt03.g = 0x80;
    flt03.b = 0x40;
}

// Requests the glow this frame: passes = level + 2 (min 0), colour r,g,b; a request only replaces a
// pending one of lower priority (pri 0xFF = locked); flag != 0 raises Status_flg[1] 0x80.
void Filter03SetParam(int level, u8 r, u8 g, u8 b, u8 pri, int flag)
{
    if (flt03.on == 1) {
        if (flt03.pri == 0xFF) {
            return;
        }
        if (pri != 0xFF && flt03.pri < pri) {
            return;
        }
    }
    flt03.on = 1;
    flt03.pri = pri;
    if (level < -2) {
        level = -2;
    }
    flt03.loop = level + 2;
    flt03.r = r;
    flt03.g = g;
    flt03.b = b;
    flt03.flag = flag;
    if (flag) {
        StaFlagOn(pG, STA_ALPHA_DRAW2);
    }
}

// Queues Filter03Render when a request is pending (and clears the request).
void Filter03Trans()
{
    static int use_filter3 = 1;

    if (use_filter3 == 0) {
        return;
    }
    if (flt03.on == 0) {
        return;
    }
    flt03.on = 0;
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter03Render, 7, 0x400, 0, 0.0f);
    }
}

// Copies the frame alpha at 1/div x 1/div2 into the filter buffer.
void Filter03GetEFB(int div, int div2)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 0x27, 1);
    GXCopyTex(filter03_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

// OT callback: runs the glow passes without scissor and restores the fog; clears Status_flg[1] 0x80
// unless flag was set.
void Filter03Render()
{
    GXColor col = { 0, 0, 0, 0 };

    SetNoScissor();
    filter03_buff = GetDrawTmpBufAddr(8);
    if (filter03_buff == 0) {
        pLog->warn(0, 0, "Filter03Trans() : not enough memory");
        return;
    }
    DCInvalidateRange(filter03_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    Filter03DrawBuffer();
    LightMgr.setFog();
    SetScissorState();
    if (flt03.flag == 0) {
        StaFlagOff(pG, STA_ALPHA_DRAW2);
    }
}

// Draws the buffer as a screen quad (scale s, offset u,v, colour r,g,b,a).
void Filter03GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, u8 r, u8 g, u8 b, u8 a, f32 s, int div, int fmt)
{
    GXTexObj tex;

    GXInitTexObj(&tex, filter03_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, fmt, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(u + 0.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s, y + 0.0f, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(u + 1.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s, y + (f32) SCR_H / s, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(u + 1.0f, v + 1.0f);
    GXPosition3f32(x + 0.0f, y + (f32) SCR_H / s, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(u + 0.0f, v + 1.0f);
}

// The glow passes: 1/2, 1/4 copies, then `loop` feedback passes with the level_tbl3 offsets, and the
// final additive draw in flt03 colour.
static void Filter03DrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    GXColor amb;
    u8 c;
    f32 zero = 0.0f;
    f32 x;
    f32 y;
    f32 px;
    f32 py;
    f32 z;
    u32 i;
    int j;
    int n;
    static f32 ppx = 0.0f;
    static f32 ppy = 0.0f;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(0);
    GXSetAlphaUpdate(1);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 1, 0, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 0, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 0, 1, 0, 2, 1);
    c = 0xFF;
    amb.r = amb.g = amb.b = amb.a = c;
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 10, 8, 15);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(0, 7, 5, 4, 7);
    GXSetTevAlphaOp(0, 0, 0, 1, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    zero = 0.0f;
    GXSetZMode(0, 6, 0);
    y = zero;
    x = zero;
    px = zero;
    py = zero;
    z = 65530.0f;
    Filter03GetEFB(1, 1);
    Filter03GXDraw(px, py, z, 0.0f, 0.0f, 0xFF, 0xFF, 0xFF, 0xFF, 2.0f, 1, 1);
    Filter03GetEFB(2, 2);
    Filter03GXDraw(px, py, z, 0.0f, 0.0f, 0xFF, 0xFF, 0xFF, 0xFF, 4.0f, 2, 1);
    for (i = 0; i < flt03.loop; i++) {
        n = 4;
        switch (flt03.level) {
        case 0:
            n = 1;
            break;
        case 1:
            n = 2;
            break;
        case 2:
            n = 3;
            break;
        case 3:
            n = 4;
            break;
        case 4:
            n = 5;
            break;
        case 5:
            n = 6;
            break;
        case 6:
            n = 7;
            break;
        case 7:
            n = 8;
            break;
        }
        for (j = 0; j < n; j++) {
            f32 t;

            Filter03GetEFB(4, 4);
            t = level_tbl3[j];
            switch (j) {
            case 0:
            case 4:
                x = zero - t;
                y = zero;
                break;
            case 1:
            case 5:
                x = zero + t;
                y = zero;
                break;
            case 2:
            case 6:
                x = zero;
                y = zero - t;
                break;
            case 3:
            case 7:
                x = zero;
                y = zero + t;
                break;
            }
            x += ppx;
            y += ppy;
            Filter03GXDraw(px, py, z, x, y, 0xFF, 0xFF, 0xFF, 0x80, 4.0f, 4, 1);
        }
    }
    GXSetBlendMode(1, 1, 1, 0);
    Filter03GetEFB(4, 4);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    SetScissorState();
    Filter03GXDraw(px, py, z, x, y, flt03.r, flt03.g, flt03.b, 0xFF, 1.0f, 4, 1);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
