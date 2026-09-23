// game/esp19.cpp: effect id 0x19, a 3D line from the effect position to the fixed world point Vec0,
// at most `max_laser_dist` (Vec1.x, default 12000) long, whose far end fades to black in
// proportion to the length used. Used for laser sight style beams and tracers.

#include "atari.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp19Work {
    Vec Vec0;  // 0x00 end point of the line
    f32 max_laser_dist;     // 0x0C maximum length
};

// 3D line effect (laser sight / tracer): draws a line from the effect toward a target point,
// fading the far end.
class cEsp19 : public cEsp {
public:
    Esp19Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

// EspCreateTbl[0x19] factory.
cEsp* Esp19_Create()
{
    return new cEsp19;
}

// Base update only (no texture animation); marks the effect as never Z-culled (huge m_Radius,
// m_Flg bit1) since the line can span the whole view.
void cEsp19::move()
{
    if (CommonMove()) {
        m_Radius = 100000000.0f;
        m_Flg |= 2;
    }
}

// End point from Vec0, maximum length from Vec1.x (0 -> 12000 units).
int cEsp19::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp19Work* w = &m_Free;

    w->Vec0 = pSeq->Vec0;
    if (pSeq->Vec1.x == 0.0f) {
        w->max_laser_dist = 12000.0f;
    } else {
        w->max_laser_dist = pSeq->Vec1.x;
    }
    return 1;
}

// Draws a 2-vertex GX line from p0 toward p1 in view matrix `mtx`, clipped to `len`, with the
// effect's blend mode; the end vertex colour is scaled by 1 - d / len. Alpha byte 0xFE in
// `color` disables the Z test.
static void Draw_line3d_local_222(Vec* p0, Vec* p1, Mtx mat, u32 col, cEsp* pEsp, f32 max_laser_dist)
{
    Vec end;
    Vec dir;
    f32 rate = 1.0f;
    f32 d;
    u8 r, g, b, a;

    GXSetBlendMode(pEsp->m_Blend_mode, pEsp->m_Src_factor, pEsp->m_Dst_factor, pEsp->m_Logic_op);
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
#line 148 "D:/Bio4/Prog/esp19.cpp"
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

// EspTransTbl[0x19]: packs the current colour and draws the line from m_Pos (plus the camera
// quake offset) to Vec0.
extern "C" void Esp19_Trans(cEsp19* esp)
{
    Esp19Work* w = &esp->m_Free;
    Vec p;
    u32 color;

    color = ((u32)esp->m_Col_r << 16) + ((u32)esp->m_Col_g << 8) + (u32)esp->m_Col_b + ((u32)esp->m_Col_a << 24);
    PSVECAdd(&esp->m_Pos, &pG->quake_ofs, &p);
    Draw_line3d_local_222(&p, &w->Vec0, pG->Camera.v_mat, color, esp, w->max_laser_dist);
}
