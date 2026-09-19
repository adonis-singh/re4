// game/filter07: thermal vision filter (D:/Bio4/Prog/filter07.cpp). While Status_flg[1] 0x04000000
// (infrared scope) is set, the frame's green channel is copied to a half-size I8 texture, blurred
// with feedback passes and drawn back through the ThermoTlut palette; the scope noise effect
// (est 0/0x1E, kind 0xB) is started with it and deleted when it ends.
#include "filter.h"
#include "light.h"
#include "atari.h"
#include "esp.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"
#include "global.h"
#include "joy.h"

// Thermal vision filter: the frame buffer's green channel is copied to a half-size I8 texture,
// blurred with pixel offsets, and drawn back through the ThermoTlut palette.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;
extern GXTlutObj ThermoTlut;

extern "C" {
void EffectEspDelete(int a, int b, int c, int d);
void EffectEspgenDelete(int a, int b, int c);
void EffectEfmDelete(int a, int b, int c);
void Filter07Render();
void Filter07GetEFB(int div, int div2);
static void Filter07GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, int div, int tlut);
void Filter07DrawBuffer();
}

void* filter07_buff = 0;
static int filter07_noize = 0;
void* filter07_pLit = 0;

static const f32 level_tbl3[32] = { 0.02f, 0.02f, 0.02f, 0.01f, 0.008f, 0.016f, 0.008f, 0.016f };

// Boot: same as the room init.
void Filter07Init()
{
    Filter07RoomInit();
}

// Room init: buffer/noise/light pointers cleared.
void Filter07RoomInit()
{
    filter07_buff = 0;
    filter07_noize = 0;
    filter07_pLit = 0;
}

// Per-frame: when the scope is off, deletes the noise effect (once); when on, starts it (once) and
// queues Filter07Render.
void Filter07Trans()
{
    static int use_filter7 = 1;

    if (use_filter7 == 0) {
        return;
    }
    if (!StaFlagChk(pG, STA_THERMO_GRAPH)) {
        if (filter07_noize != 1) {
            return;
        }
        filter07_noize = 0;
        EffectEspDelete(0, 0xB, 0, 0);
        EffectEspgenDelete(0, 0xB, 0);
        EffectEfmDelete(0, 0xB, 0);
        return;
    }
    if (filter07_noize == 0) {
        filter07_noize = 1;
        EstSet(0, -1, 0, 0, 0, 0x1E, 0, 0xB, 0, 0);
    }
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter07Render, 7, 0x400, 0, 0.0f);
    }
}

// Copies the frame's green channel at 1/div x 1/div2 as I8 into the filter buffer.
void Filter07GetEFB(int div, int div2)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 0x29, 1);
    GXCopyTex(filter07_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

// OT callback: runs the thermal passes without scissor and restores the fog.
void Filter07Render()
{
    GXColor col = { 0, 0, 0, 0 };

    filter07_buff = GetDrawTmpBufAddr(10);
    if (filter07_buff == 0) {
        pLog->warn(0, 0, "Filter07Trans() : not enough memory");
        return;
    }
    SetNoScissor();
    DCInvalidateRange(filter07_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    Filter07DrawBuffer();
    SetScissorState();
    LightMgr.setFog();
}

// Draws the I8 buffer as a screen quad, through the thermal palette when tlut != 0.
static void Filter07GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, int div, int tlut)
{
    {
        GXTexObj tex;

        if (tlut) {
            GXInitTexObjCI(&tex, filter07_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 9, 0, 0, 0, 0);
            GXLoadTlut(&ThermoTlut, 0);
        } else {
            GXInitTexObj(&tex, filter07_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 1, 0, 0, 0);
        }
        GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
        GXLoadTexObj(&tex, 0);
    }
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 0.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 1.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s, y + (f32) SCR_H / s, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 1.0f, v + 1.0f);
    GXPosition3f32(x + 0.0f, y + (f32) SCR_H / s, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 0.0f, v + 1.0f);
}

// The thermal passes: half copy, nFeedLp07 feedback passes with level_tbl3 offsets, then the final
// palette draw (debug: Y held shows the raw I8 image).
void Filter07DrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    f32 y = 0.0f;
    f32 x;
    f32 px;
    f32 py;
    f32 z;
    u8 a;
    u32 tlut;
    u32 i;
    int j;
    int n;
    static u32 FocusLevel07 = 1;
    static f32 x_old = 0.0f;
    static f32 y_old = 0.0f;
    static f32 ppx = 0.0f;
    static f32 ppy = 0.0f;
    static u32 nFeedLp07 = 1;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 0, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 0, 1, 0, 2, 1);
    GXColor amb = { 0, 0, 0, 0xFF };
    GXSetChanAmbColor(4, amb);
    a = 0xFF;
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
    y = 0.0f;
    GXSetZMode(0, 7, 1);
    px = 0.0f;
    z = 65530.0f;
    Filter07GetEFB(1, 1);
    py = 0.0f;
    Filter07GXDraw(px, py, z, 0.0f, 0.0f, (f32) a, 2.0f, 1, 0);
    x = 0.0f;
    for (i = 0; i < nFeedLp07; i++) {
        n = 4;
        switch (FocusLevel07) {
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

            a = 0x60;
            Filter07GetEFB(2, 2);
            t = level_tbl3[j];
            switch (j) {
            case 0:
            case 4:
                x = x_old - t;
                y = y_old;
                break;
            case 1:
            case 5:
                x = x_old + t;
                y = y_old;
                break;
            case 2:
            case 6:
                x = x_old;
                y = y_old - t;
                break;
            case 3:
            case 7:
                x = x_old;
                y = y_old + t;
                break;
            }
            x += ppx;
            y += ppy;
            Filter07GXDraw(px, py, z, x, y, (f32) a, 2.0f, 2, 0);
        }
    }
    Filter07GetEFB(2, 2);
    a = 0xFF;
    tlut = Joy[0].on & 0x400;
    Filter07GXDraw(px, py, z, x, y, (f32) a, 1.0f, 2, tlut == 0);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
