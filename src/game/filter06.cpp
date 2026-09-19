// game/filter06: dust / snow / rain particle filter (D:/Bio4/Prog/filter06.cpp). Up to 0x800 line
// particles drift with a speed around the camera and are wrapped back into a box around it; drawn
// as lines whose alpha fades with camera distance and eases to the requested colour. Rooms start it
// through effect controller 44 (Filter06SetParam).
#include "filter.h"
#include "atari.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "db_log.h"
#include "trans_ot.h"
#include "global.h"
#include "camera.h"
#include "rnd.h"
#include "math_sub.h"

// Dust / snow particle filter: `num` line particles drift with a speed around the camera and
// are wrapped back into a box (LR half width x34, up half height x38, depth x3C) around it.
//
// cParticle06::move: the alphaBase load/product live in r11 in the original (the fast-cast address
// pseudo of `(u8) a` took r9 first, i.e. the loadaddr sat below the `lbz` in the scheduled RTL);
// ours hoists the loadaddr to the block top, so alphaBase is pinned (see the function). The re-read
// mask (`clrlwi rX, rStore, 24`) is reproduced with a volatile asm on `v`.

class cParticle06 {
public:
    Vec m_Pos;       // 0x00
    Vec m_Spd;       // 0x0C
    u8 pad_18[4];
    u8 m_Base_alpha;  // 0x1C
    u8 m_Alpha;      // 0x1D
    u8 pad_1E[2];

    void init(u32 no);
    void move();
};

struct Filter06Work {
    cParticle06* p;  // 0x00
    u32 num;         // 0x04
    Vec spd;         // 0x08
    u8 col[4];       // 0x14  target colour
    u8 cur[4];       // 0x18  current colour
    f32 colF[3];     // 0x1C
    f32 alphaF;      // 0x28
    f32 rate;        // 0x2C  alpha approach rate
    f32 spread;      // 0x30  initial spread around the camera (x 10)
    f32 rangeLR;     // 0x34  half width of the wrap box
    f32 rangeUp;     // 0x38  half height
    f32 rangeDepth;  // 0x3C  depth
    f32 scale;       // 0x40  line length (speed scale)
    u8 alphaMin;     // 0x44
    u8 pad_45[3];
};

extern "C" {
void Filter06Render();
}

Filter06Work flt06;
static Vec cam_vec_LR;

// Places particle `no` randomly around the camera within `spread` * 10 units (plus a per-particle offset).
void cParticle06::init(u32 no)
{
    FSet(m_Pos.x, pG->Cam.param.pos.x);
    FSet(m_Pos.y, pG->Cam.param.pos.y);
    FSet(m_Pos.z, pG->Cam.param.pos.z);
    m_Pos.x += flt06.spread * 10.0f * fRand1_1() + (f32) no * 500.0f;
    m_Pos.y += flt06.spread * 10.0f * fRand1_1() + (f32) no * 500.0f;
    m_Pos.z += flt06.spread * 10.0f * fRand1_1() + (f32) no * 500.0f;
    m_Base_alpha = 0x80;
    m_Alpha = 0;
}

// Advances the particle by its speed and wraps it back into the camera box (rangeLR left/right,
// rangeUp, rangeDepth in front); alpha = base * 255/(depth/800), scaled by the current colour alpha,
// not below alphaMin.
void cParticle06::move()
{
    Vec d;
    Vec dir;
    Vec up;
    Vec tmp;
    f32 dot;
    f32 a;
    int n;
    int v;

    PSVECAdd(&m_Pos, &m_Spd, &m_Pos);
    PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &d);
    dot = PSVECDotProduct(&d, &cam_vec_LR);
    if (dot >= 0.0f) {
        n = (int) ((dot + flt06.rangeLR) / (flt06.rangeLR + flt06.rangeLR));
    } else {
        n = (int) ((dot - flt06.rangeLR) / (flt06.rangeLR + flt06.rangeLR));
    }
    if (n != 0) {
        PSVECScale(&cam_vec_LR, &dir, -(flt06.rangeLR * (f32) n + flt06.rangeLR * (f32) n));
        PSVECAdd(&m_Pos, &dir, &m_Pos);
    }
#line 133 "D:/Bio4/Prog/filter06.cpp"
    VECNormalize(&pG->Cam.up, &up);
    PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &d);
    dot = PSVECDotProduct(&d, &up);
    if (dot >= 0.0f) {
        n = (int) ((dot + flt06.rangeUp) / (flt06.rangeUp + flt06.rangeUp));
    } else {
        n = (int) ((dot - flt06.rangeUp) / (flt06.rangeUp + flt06.rangeUp));
    }
    if (n != 0) {
        PSVECScale(&up, &tmp, -(flt06.rangeUp * (f32) n + flt06.rangeUp * (f32) n));
        PSVECAdd(&m_Pos, &tmp, &m_Pos);
    }
    PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &dir);
#line 154
    VECNormalize(&dir, &dir);
    PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &d);
    dot = PSVECDotProduct(&d, &dir);
    if (dot >= 0.0f) {
        n = (int) (dot / flt06.rangeDepth);
    } else {
        n = ~(int) (-dot / flt06.rangeDepth);
    }
    if (n != 0) {
        PSVECScale(&dir, &up, -(flt06.rangeDepth * (f32) n));
        PSVECAdd(&m_Pos, &up, &m_Pos);
        PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &d);
        dot = PSVECDotProduct(&d, &dir);
    }
    a = 255.0f / (dot / 800.0f);
    if (a > 255.0f) {
        a = 255.0f;
    }
    {
        // COMPILER-DIFF: #17. The alphaBase load/product live in r11 in the original (the fast-cast
        // address pseudo of `(u8) a` took r9 first); ours gives them r9. Pinned, no code emitted.
        register int ab asm("r11");
        ab = m_Base_alpha;
        v = (ab * (u8) a) >> 8;
    }
    // the original re-reads `alpha` masked (`clrlwi rX, rStore, 24`) with the `&flt06` pair issued
    // after the store; ours knows the product fits in 8 bits and folds the mask -- the volatile asm
    // hides the range from combine and keeps the address pair below the product (emobj setYarare)
    asm volatile("" : "+r"(v));
    m_Alpha = v;
    v = ((u32) m_Alpha * flt06.cur[3]) >> 8;
    m_Alpha = v;
    if (m_Alpha < flt06.alphaMin) {
        m_Alpha = flt06.alphaMin;
    }
}

// Boot: same as the room init.
void Filter06Init()
{
    Filter06RoomInit();
}

// Room init: no particles, colour black, box 2500/2500/5000, spread 5000.
void Filter06RoomInit()
{
    memclr_asm(&flt06, sizeof(flt06));
    flt06.col[0] = 0;
    flt06.col[1] = 0;
    flt06.col[2] = 0;
    flt06.col[3] = 0;
    flt06.cur[0] = 0;
    flt06.cur[1] = 0;
    flt06.cur[2] = 0;
    flt06.cur[3] = 0;
    flt06.colF[0] = 0.0f;
    flt06.colF[1] = 0.0f;
    flt06.colF[2] = 0.0f;
    flt06.alphaF = 0.0f;
    flt06.spread = 5000.0f;
    flt06.rangeLR = 2500.0f;
    flt06.rangeUp = 2500.0f;
    flt06.rangeDepth = 5000.0f;
    flt06.p = 0;
}

// Per-frame (when particles exist): computes the camera right vector, moves every particle unless
// Stop_flg 0x08000000, eases the colour alpha towards the target by `rate`, and queues the render.
// Skipped while the player is in a special effect area (Status_flg[1] 0x02000000, e.g. indoors).
void Filter06Trans()
{
    static int use_filter6 = 1;
    u32 i;

    if (use_filter6 == 0) {
        return;
    }
    if (flt06.num == 0) {
        return;
    }
    if (StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
        return;
    }
    PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &cam_vec_LR);
    PSVECCrossProduct(&cam_vec_LR, &pG->Cam.up, &cam_vec_LR);
#line 246
    VECNormalize(&cam_vec_LR, &cam_vec_LR);
    if (!SpfFlagChk(pG, SPF_ESP)) {
        for (i = 0; i < flt06.num; i++) {
            flt06.p[i].move();
        }
        flt06.colF[0] = (f32) flt06.col[0];
        flt06.colF[1] = (f32) flt06.col[1];
        flt06.colF[2] = (f32) flt06.col[2];
        flt06.alphaF += ((f32) (int) flt06.col[3] - flt06.alphaF) * flt06.rate;
        flt06.cur[0] = (u8) flt06.colF[0];
        flt06.cur[1] = (u8) flt06.colF[1];
        flt06.cur[2] = (u8) flt06.colF[2];
        flt06.cur[3] = (u8) flt06.alphaF;
    }
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter06Render, 7, 0x400, 0, 0.0f);
    }
}

// Starts `level` particles (max 0x800; buffer allocated on first use) with speed spd +- spdRand per
// particle, target colour r,g,b,a approached at `rate`, starting alpha, line length scale and
// minimum alpha.
void Filter06SetParam(u32 level, int r, int g, int b, int a, f32 rate, Vec* spd, f32 alpha, Vec* spdRand, f32 scale,
                      int alphaMin)
{
    u32 i;

    flt06.num = level;
    if (level > 0x800) {
        flt06.num = 0x800;
    }
    if (flt06.p == 0) {
        cParticle06* p;

#line 280
        p = (cParticle06*) MEM_ALLOC(0x10000, 1, 13);
        flt06.p = p;
        if (p == 0) {
            flt06.num = 0;
            pLog->err(0, 0, "Filter06: not enough memory.");
            return;
        }
    }
    for (i = 0; i < flt06.num; i++) {
        flt06.p[i].init(i);
        flt06.p[i].m_Spd = *spd;
        flt06.p[i].m_Spd.x += spdRand->x * fRand1_1();
        flt06.p[i].m_Spd.y += spdRand->y * fRand1_1();
        flt06.p[i].m_Spd.z += spdRand->z * fRand1_1();
    }
    flt06.col[0] = r;
    flt06.col[1] = g;
    flt06.col[2] = b;
    flt06.col[3] = a;
    flt06.alphaF = alpha;
    flt06.rate = rate;
    flt06.spd = *spd;
    flt06.scale = scale;
    flt06.alphaMin = alphaMin;
}

// OT callback: draws every particle as a line from its position along its speed * scale in the
// current colour with the particle's alpha.
void Filter06Render()
{
    u32 i;
    cParticle06* p;
    f32 scale;

    GXSetBlendMode(1, 4, 5, 0);
    CameraCurrentProjection();
    GXLoadPosMtxImm(pG->Cam.v_mat, 0);
    GXSetCurrentMtx(0);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevColorIn(0, 0xF, 0xC, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXEnableTexOffsets(0, 0, 1);
    p = flt06.p;
    scale = flt06.scale;
    for (i = 0; i < flt06.num; i++, p++) {
        GXBegin(0xA8, 0, 2);
        GXPosition3f32(p->m_Pos.x, p->m_Pos.y, p->m_Pos.z);
        GXColor4u8(flt06.cur[0] >> 2, flt06.cur[1] >> 2, flt06.cur[2] >> 2, p->m_Alpha >> 2);
        GXPosition3f32(p->m_Spd.x * scale + p->m_Pos.x, p->m_Spd.y * scale + p->m_Pos.y, p->m_Spd.z * scale + p->m_Pos.z);
        GXColor4u8(flt06.cur[0], flt06.cur[1], flt06.cur[2], p->m_Alpha);
    }
    GXEnableTexOffsets(0, 0, 0);
}
asm(".section .sdata; .balign 32");
