// game/filter0a: masked blur filter (D:/Bio4/Prog/filter0a.cpp). Quarter-size feedback blur of the
// frame blended back through an ID-system texture mask (filter0a_mask_id, alpha filter0a_mask_alpha):
// the scope / binocular vignette. Runs while use_filter0a is set and the scope flag Status_flg[1]
// 0x04000000 is off.
#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"
#include "global.h"
#include "id_sys.h"
#include "texture.h"

// Blur filter with an optional ID-texture mask (filter0a_mask_id) blended over the result.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;

extern "C" {
void filter0a_mask_tex();
void Filter0aRender();
void Filter0aGetEFB(int div, int div2);
void Filter0aGXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, int div, int mask);
void Filter0aDrawBuffer();
}

u8 use_filter0a = 0;
u8 filter0a_mask_flag = 0;
u8 filter0a_mask_id = 0;
u8 filter0a_mask_alpha = 0;
void* filter0a_buff = 0;

static const f32 level_tbl2[32] = { 0.002f, 0.004f, 0.002f, 0.004f, 0.008f, 0.016f, 0.008f, 0.016f };

// Loads the mask texture (CI8/CI4 with its TLUT, or direct) from the id texture work into map 1.
void filter0a_mask_tex()
{
    GXTexObj tex;
    GXTlutObj tlut;
    Mtx mtx;
    TexWk* wk;
    TEXDescriptor* desc;
    TEXHeader* hdr;
    CLUTHeader* clut;

    if (filter0a_mask_flag == 0) {
        return;
    }
    wk = IdGetTexWk(filter0a_mask_id, 1);
    if (wk == 0) {
        return;
    }
    GXTexObj* pTex = &tex;
    GXTlutObj* pTlut = &tlut;
    desc = TEXGet(wk->pTpl, 0);
    hdr = desc->textureHeader;
    if (hdr->format == 8 || hdr->format == 9) {
        GXInitTexObjCI(pTex, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0, 1);
        clut = desc->CLUTHeader;
        GXInitTlutObj(pTlut, clut->data, clut->format, clut->numEntries);
        GXLoadTlut(pTlut, 1);
    } else {
        GXInitTexObj(pTex, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0);
    }
    GXLoadTexObj(pTex, 1);
    PSMTXIdentity(mtx);
    GXLoadTexMtxImm(mtx, 0x21, 1);
    GXSetTexCoordGen(1, 1, 4, 0x21);
    GXSetNumTevStages(2);
    GXSetNumTexGens(2);
    GXSetTevOrder(1, 1, 1, 4);
    GXSetTevColorIn(1, 15, 15, 15, 0);
    GXSetTevColorOp(1, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(1, 4, 7, 7, 5);
    GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
}

// Boot: off, no mask.
void Filter0aInit()
{
    filter0a_buff = 0;
    use_filter0a = 0;
    filter0a_mask_flag = 0;
    filter0a_mask_id = 0;
}

// Room init: off, no mask.
void Filter0aRoomInit()
{
    filter0a_buff = 0;
    use_filter0a = 0;
    filter0a_mask_flag = 0;
    filter0a_mask_id = 0;
}

// Queues Filter0aRender when enabled and the thermal scope is not active.
void Filter0aTrans()
{
    if (use_filter0a == 0) {
        return;
    }
    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        return;
    }
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter0aRender, 7, 0x400, 0, 0.0f);
    }
}

// Copies the frame at 1/div x 1/div2 into the filter buffer.
void Filter0aGetEFB(int div, int div2)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 6, 1);
    GXCopyTex(filter0a_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

// OT callback: runs the blur passes and restores the fog.
void Filter0aRender()
{
    GXColor col = { 0, 0, 0, 0 };

    filter0a_buff = GetDrawTmpBufAddr(13);
    if (filter0a_buff == 0) {
        pLog->warn(0, 0, "Filter0aTrans() : not enough memory");
        return;
    }
    DCInvalidateRange(filter0a_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    Filter0aDrawBuffer();
    LightMgr.setFog();
}

// Draws the buffer as a screen quad; mask != 0 adds the mask texture stage.
void Filter0aGXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, int div, int mask)
{
    GXTexObj tex;
    Mtx mv;
    GXColor amb;

    GXInitTexObj(&tex, filter0a_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 1, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 1, 1, 0, 2, 1);
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 15, 15, 8);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    if (mask) {
        filter0a_mask_tex();
    }
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
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

// Quarter copy, nFeedLp feedback passes with the level_tbl2 offsets, then the masked blend with
// filter0a_mask_alpha.
void Filter0aDrawBuffer()
{
    Mtx44 proj;
    f32 y = 0.0f;
    f32 x;
    f32 px;
    f32 py;
    f32 z;
    u8 a;
    u32 i;
    int j;
    int n;
    static u32 FocusLevel = 7;
    static f32 x_old = 0.0f;
    static f32 y_old = 0.0f;
    static f32 ppx = 0.0f;
    static f32 ppy = 0.0f;
    static u32 nFeedLp = 2;

    SetNoScissor();
    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    y = 0.0f;
    GXSetZMode(0, 6, 0);
    px = 0.0f;
    z = 65530.0f;
    Filter0aGetEFB(1, 1);
    py = 0.0f;
    a = 0xFF;
    Filter0aGXDraw(px, py, z, 0.0f, 0.0f, (f32) a, 4.0f, 1, 0);
    x = 0.0f;
    for (i = 0; i < nFeedLp; i++) {
        n = 4;
        switch (FocusLevel) {
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

            a = 0x80;
            Filter0aGetEFB(4, 4);
            t = level_tbl2[j];
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
            Filter0aGXDraw(px, py, z, x, y, (f32) a, 4.0f, 4, 0);
        }
    }
    Filter0aGetEFB(4, 4);
    SetScissorState();
    Filter0aGXDraw(px, py, z, x, y, (f32) filter0a_mask_alpha, 1.0f, 4, 1);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
