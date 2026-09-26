// game/dbmodule.cpp: debug primitive drawing (tiles, lines, spheres, cylinders, cones,
// boxes, wireframes) and the time display used by the debug tools.

#include "dbmodule.h"
#include "global.h"
#include "gx.h"
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "model.h"
#include "math_sub.h"
#include "db_log.h"
#include "camera.h"
#include "view.h"
#include "main_mem.h"
#include "trans_ot.h"
#include "joy.h"
#include "eprintf.h"
#include <dolphin/os/OSCache.h>
#include <dolphin/gx/GXDispList.h>
#include "trans.h"

struct TileWork {
    s16 x;       // 0x00
    s16 y;       // 0x02
    s16 w;       // 0x04
    s16 h;       // 0x06
    u32 color;   // 0x08
};

void* sphere_buff;
void* circle_buff;
void* cylinder_buff;
void* corn_buff;

int DB_poly_num = 0;
int DB_quads_num = 0;
int DB_tri_num = 0;
int DB_strip_num = 0;

// Boot: allocates the sphere / circle / cylinder / cone display lists from the debug heap and
// builds them.
void init_dbmodule()
{
    sphere_buff = Debug_alloc(0x3040, 1);
    circle_buff = Debug_alloc(0x4C0, 1);
    cylinder_buff = Debug_alloc(0x4C0, 1);
    corn_buff = Debug_alloc(0x3040, 1);
    DCFlushRange(sphere_buff, 0x3040);
    DCFlushRange(circle_buff, 0x4C0);
    DCFlushRange(cylinder_buff, 0x4C0);
    DCFlushRange(corn_buff, 0x3040);
    init_sphere();
    init_circle();
    init_cylinder();
    init_corn();
}

// Writes one 2D vertex position into the GX FIFO.
static inline void Pos2s16(s16 x, s16 y)
{
    GXWGFifo->s16 = x;
    GXWGFifo->s16 = y;
}

// OT callback of Draw_tile: draws the filled screen rectangle (ortho 512 x 448, no blending).
void Render_tile(void* pTile)
{
    TileWork* t = (TileWork*) pTile;
    Mtx44 proj;
    Mtx m;
    u32 color;
    s16 x, y, w, h;

    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 0, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);

    color = t->color;
    y = t->y;
    w = t->w;
    h = t->h;
    x = t->x;
    GXBegin(0x98, 0, 4);
    Pos2s16(x, y);
    GXColor4u8(color >> 24, color >> 16, color >> 8, color);
    Pos2s16(x + w, y);
    GXColor4u8(color >> 24, color >> 16, color >> 8, color);
    Pos2s16(x, y + h);
    GXColor4u8(color >> 24, color >> 16, color >> 8, color);
    Pos2s16(x + w, y + h);
    GXColor4u8(color >> 24, color >> 16, color >> 8, color);
}

// Queues a filled screen-space rectangle (pixels) in OT 13 for this frame.
void Draw_tile(int x, int y, int w, int h, GXColor* col)
{
    TileWork* t = (TileWork*) GetPrimBuff(sizeof(TileWork));
    if (t) {
        t->x = x;
        t->y = y;
        t->w = w;
        t->h = h;
        t->color = *(u32*) col;
        AddOtDirect(13, t, (void (*)()) Render_tile, 0, 0x1000, NULL, 0.0f);
    }
}

// Immediate 2D line between two screen points (ortho projection, ARGB colour).
void Draw_line(Vec* p0, Vec* b, u32 col)
{
    Mtx44 proj;
    Mtx m;
    s16 x0, y0, x1, y1;
    u8 cr, cg, cb, ca;

    GXSetBlendMode(1, 1, 0, 0);
    cg = col >> 16;
    cb = col >> 8;
    ca = col;
    cr = col >> 24;
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);

    x0 = (s16) p0->x;
    y0 = (s16) p0->y;
    x1 = (s16) b->x;
    y1 = (s16) b->y;
    GXBegin(0xB0, 0, 2);
    GXPosition3s16(x0, y0, 1);
    GXColor4u8(cg, cb, ca, cr);
    GXPosition3s16(x1, y1, 1);
    GXColor4u8(cg, cb, ca, cr);
}

// Immediate 2D filled quad at `pos` of `size` (screen pixels).
void Draw_quad(Vec* pPos, Vec* pSize, u32 col)
{
    Mtx44 proj;
    Mtx m;
    s16 x0, y0, x1, y1;
    u8 cr, cg, cb, ca;

    GXSetBlendMode(1, 1, 0, 0);
    cg = col >> 16;
    cb = col >> 8;
    ca = col;
    cr = col >> 24;
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);

    x0 = (s16) pPos->x;
    y1 = (s16) pPos->y + (s16) pSize->y;
    y0 = (s16) pPos->y;
    x1 = (s16) pPos->x + (s16) pSize->x;
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x0, y1, 1);
    GXColor4u8(cg, cb, ca, cr);
    GXPosition3s16(x0, y0, 1);
    GXColor4u8(cg, cb, ca, cr);
    GXPosition3s16(x1, y0, 1);
    GXColor4u8(cg, cb, ca, cr);
    GXPosition3s16(x1, y1, 1);
    GXColor4u8(cg, cb, ca, cr);
}

// Immediate world-space line (current camera view matrix); blend 1 = additive.
void Draw_line3d(Vec* p0, Vec* b, u32 col, int blend)
{
    Draw_line3d_local(p0, b, pG->Camera.v_mat, col, blend);
}

// Immediate line in the space of matrix `mtx` (view * local): sets the line GX state, draws the
// 2 vertices and restores the state. Alpha 0xFE in the colour skips the Z test.
void Draw_line3d_local(Vec* p0, Vec* b, Mtx mat, u32 col, int blend)
{
    u8 cr, cg, cb;

    if (blend == 0) {
        GXSetBlendMode(1, 1, 0, 0);
    } else {
        GXSetBlendMode(1, 1, 1, 0);
    }
    CameraCurrentProjection();
    GXSetCullMode(0);
    if ((col >> 24) == 0xFE) {
        GXSetZMode(0, 3, 1);
    } else {
        GXSetZMode(1, 3, 1);
    }
    cr = col >> 16;
    cg = col >> 8;
    cb = col;
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
    GXBegin(0xB0, 0, 2);
    GXPosition3f32(p0->x, p0->y, p0->z);
    GXColor4u8(cr, cg, cb, 0xFF);
    GXPosition3f32(b->x, b->y, b->z);
    GXColor4u8(cr, cg, cb, 0xFF);
}

// Sets the GX state for a batch of debug lines (callers then emit GXBegin lines themselves).
void Draw_line3d_init()
{
    GXColor black;

    black.a = 0;
    black.b = 0;
    black.g = 0;
    black.r = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, black);
    GXSetBlendMode(0, 1, 0, 0);
    CameraCurrentProjection();
    GXSetCullMode(0);
    GXSetZMode(0, 3, 1);
    GXSetZMode(0, 3, 1);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(0, 1);
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(pG->Camera.v_mat, 0);
}

// Restores the GX state after a Draw_line3d_init batch.
void Draw_line3d_end()
{
    LightMgr.setFog();
}

// Immediate world-space filled triangle p[0..2]; zupd 0 leaves the Z buffer alone.
void Draw_poly(Vec* p, u32 col, int zmode)
{
    Draw_poly_local(p, pG->Camera.v_mat, col, zmode);
}

// Immediate filled triangle in matrix `mtx` space with alpha blending.
void Draw_poly_local(Vec* p, Mtx mat, u32 col, int zmode)
{
    u8 cr, cg, cb, ca;

    CameraCurrentProjection();
    GXSetCullMode(0);
    if (zmode) {
        GXSetZMode(1, 3, 1);
    } else {
        GXSetZMode(1, 3, 0);
    }
    cg = col >> 16;
    cb = col >> 8;
    ca = col;
    cr = col >> 24;
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 1, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(0, 1);
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(mat, 0);
    GXSetBlendMode(1, 4, 5, 0);
    GXBegin(0x90, 0, 3);
    GXMatrixIndex1u8(0);
    GXPosition3f32(p->x, p->y, p->z);
    GXColor4u8(cg, cb, ca, cr);
    p++;
    GXMatrixIndex1u8(0);
    GXPosition3f32(p->x, p->y, p->z);
    GXColor4u8(cg, cb, ca, cr);
    p++;
    GXMatrixIndex1u8(0);
    GXPosition3f32(p->x, p->y, p->z);
    GXColor4u8(cg, cb, ca, cr);
}

// Wire sphere (16 x 16 display list) of radius r at `pos`; zcmp / zupd select the Z test / write.
void Draw_sphere(Vec* pos, f32 r, u32 rgb, int zmode, int zcheck)
{
    Mtx m;
    GXColor c;

    CameraCurrentProjection();
    if (zcheck) {
        if (zmode) {
            GXSetZMode(1, 3, 1);
        } else {
            GXSetZMode(1, 3, 0);
        }
    } else {
        if (zmode) {
            GXSetZMode(0, 3, 1);
        } else {
            GXSetZMode(0, 3, 0);
        }
    }
    *(u32*) &c = rgb;
    GXSetChanMatColor(0, c);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetBlendMode(0, 4, 5, 0);
    PSMTXScale(m, r, r, r);
    TransMatrix(m, pos);
    PSMTXConcat(pG->Camera.v_mat, m, m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXCallDisplayList(sphere_buff, 0x3040);
}

// Wire cylinder of radius r and height h standing on `pos` (world space).
void Draw_cylinder(Vec* pos, f32 r, f32 h, u32 rgba)
{
    Mtx m;
    GXColor c;

    CameraCurrentProjection();
    GXSetZMode(1, 3, 1);
    *(u32*) &c = rgba;
    GXSetChanMatColor(0, c);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXSetBlendMode(0, 4, 5, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    PSMTXScale(m, r, h, r);
    TransMatrix(m, pos);
    PSMTXConcat(pG->Camera.v_mat, m, m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXCallDisplayList(cylinder_buff, 0x4C0);
}

// Wire cylinder placed through matrix `mtx` (only the first display list vertices: a circle).
void Draw_cylinderMtx(Mtx mtx, Vec* pos, f32 r, f32 h, u32 color)
{
    Mtx m;
    Vec p;
    GXColor c;

    CameraCurrentProjection();
    GXSetZMode(1, 3, 1);
    *(u32*) &c = color;
    GXSetChanMatColor(0, c);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXSetBlendMode(0, 4, 5, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    p = *pos;
    PSMTXMultVec(mtx, &p, &p);
    PSMTXScale(m, r, h, r);
    PSMTXConcat(mtx, m, m);
    TransMatrix(m, &p);
    PSMTXConcat(pG->Camera.v_mat, m, m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXCallDisplayList(cylinder_buff, 4);
}

// Wire cone from `pos` along `dir` (its length) with base radius r.
void Draw_corn3(Vec* pos, Vec* norm, f32 cutoff, u32 rgb)
{
    Vec rot;
    Vec v;
    Mtx m;
    f32 len;

    rot.x = 0.0f;
    rot.y = -atan2f(norm->x, norm->z);
    rot.z = 0.0f;
    low_RotMatrix(m, &rot);
    PSMTXMultVec(m, norm, &v);
    rot.x = atan2f(-v.y, v.z);
    rot.y = -rot.y;
    len = PSVECMag(norm);
    Draw_corn(pos, &rot, len, cutoff, rgb);
}

// Wire cone (display list) of length len / radius r at `pos` rotated by `rot`.
void Draw_corn(Vec* pos, Vec* ang, f32 l, f32 r, u32 rgb)
{
    Mtx m;
    Mtx s;
    GXColor c;
    f32 w;

    CameraCurrentProjection();
    GXSetZMode(1, 3, 1);
    *(u32*) &c = rgb;
    GXSetChanMatColor(0, c);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetBlendMode(0, 4, 5, 0);
    low_RotMatrix(m, ang);
    w = l * r / 90.0f;
    PSMTXScale(s, w, w, l);
    PSMTXConcat(m, s, m);
    TransMatrix(m, pos);
    PSMTXConcat(pG->Camera.v_mat, m, m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXCallDisplayList(corn_buff, 0x3040);
}

// Wire view cone: apex `pos`, axis `dir`, length len, half angle `ang` in degrees (16 rim
// lines and the rim circle); used for the eye trigger / enemy sight displays.
void Draw_corn2(Vec* pPos, Vec* pVec, f32 size, f32 r, u32 col)
{
    Vec axis;
    Vec t;
    Vec d;
    Vec s;
    Vec prev;
    Vec first;
    Mtx m;
    Vec v;
    f32 th;
    f32 h;
    u32 i;

    r = r * DEG2RAD;
    PSMTXIdentity(m);
    axis.x = 1.0f;
    axis.y = 0.0f;
    axis.z = 0.0f;
    d = *pVec;
    PSVECCrossProduct(&axis, &d, &s);
    if (s.x != 0.0f || s.y != 0.0f || s.z != 0.0f) {
#line 1023 "D:/Bio4/Prog/dbmodule.cpp"
        VECNormalize(&s, &s);
    }
    PSVECCrossProduct(&d, &s, &t);
    if (t.x != 0.0f || t.y != 0.0f || t.z != 0.0f) {
#line 1025
        VECNormalize(&t, &t);
    }
    m[0][0] = t.x;
    m[0][1] = d.x;
    m[0][2] = s.x;
    m[1][0] = t.y;
    m[1][1] = d.y;
    m[1][2] = s.y;
    m[2][0] = t.z;
    m[2][1] = d.z;
    m[2][2] = s.z;

    for (i = 0; i < 12; i++) {
        th = (f32) i / 12.0f * 2.0f * PI;
        v.x = SINF(th);
        v.z = COSF(th);
        v.y = size;
        if (r > 3.141592f) {
            v.y = -size;
        }
        h = r * 0.5f;
        v.x *= size * tanf(h);
        v.z *= size * tanf(h);
        if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) {
#line 1040
            VECNormalize(&v, &v);
        }
        PSVECScale(&v, &v, size);
        PSMTXMultVec(m, &v, &v);
        PSVECAdd(pPos, &v, &v);
        Draw_line3d(pPos, &v, 0xB0B0B0B0, 0);
        if (i == 0) {
            first = v;
        } else {
            if (i == 11) {
                Draw_line3d(&v, &first, 0xB0B0B0B0, 0);
            }
            Draw_line3d(&v, &prev, 0xB0B0B0B0, 0);
        }
        prev = v;
    }
    PSVECScale(pVec, &axis, size);
    PSVECAdd(pPos, &axis, &axis);
    Draw_line3d(pPos, &axis, 0xB0B000B0, 0);
}

// Box from its 8 corners: flag 0 filled faces (12 triangles), else the 12 edges.
void Draw_box(Vec* pBoxVec, u32 col, int flg)
{
    static u8 ptbl[36] = {
        0, 2, 1,  2, 3, 1,  4, 5, 6,  5, 7, 6,
        2, 6, 3,  6, 7, 3,  0, 1, 4,  1, 5, 4,
        1, 3, 5,  3, 7, 5,  2, 0, 6,  0, 4, 6,
    };
    Vec p[3];
    int i;

    for (i = 0; i < 12; i++) {
        p[0] = pBoxVec[ptbl[i * 3]];
        p[1] = pBoxVec[ptbl[i * 3 + 1]];
        p[2] = pBoxVec[ptbl[i * 3 + 2]];
        Draw_poly(p, col, 0);
    }
    if (flg & 1) {
        Draw_line3d(&pBoxVec[0], &pBoxVec[1], col, 0);
        Draw_line3d(&pBoxVec[1], &pBoxVec[3], col, 0);
        Draw_line3d(&pBoxVec[3], &pBoxVec[2], col, 0);
        Draw_line3d(&pBoxVec[2], &pBoxVec[0], col, 0);
        Draw_line3d(&pBoxVec[4], &pBoxVec[5], col, 0);
        Draw_line3d(&pBoxVec[5], &pBoxVec[7], col, 0);
        Draw_line3d(&pBoxVec[7], &pBoxVec[6], col, 0);
        Draw_line3d(&pBoxVec[6], &pBoxVec[4], col, 0);
        Draw_line3d(&pBoxVec[0], &pBoxVec[4], col, 0);
        Draw_line3d(&pBoxVec[1], &pBoxVec[5], col, 0);
        Draw_line3d(&pBoxVec[2], &pBoxVec[6], col, 0);
        Draw_line3d(&pBoxVec[3], &pBoxVec[7], col, 0);
    }
}

// Position marker in world space: a red x axis line and white / green y / z lines of `size`.
void Draw_pos(Vec* pos, int size)
{
    Draw_local_pos(pos, size, pG->Camera.v_mat);
}

// Position marker in matrix `mtx` space.
void Draw_local_pos(Vec* pos, int size, Mtx mat)
{
    Vec p;
    f32 s;

    {
        Vec a = {0.0f, 0.0f, 0.0f};
        s = (f32) size;
        a.x = pos->x + s;
        a.y = pos->y;
        a.z = pos->z;
        p = a;
        Draw_line3d_local(pos, &p, mat, 0x00FF0000, 0);
        {
            Vec b = {0.0f, 0.0f, 0.0f};
            b.x = pos->x - s;
            b.y = pos->y;
            b.z = pos->z;
            p = b;
            Draw_line3d_local(pos, &p, mat, 0x00FFFFFF, 0);
        }
    }
    {
        Vec a = {0.0f, 0.0f, 0.0f};
        a.x = pos->x;
        a.y = pos->y + s;
        a.z = pos->z;
        p = a;
        Draw_line3d_local(pos, &p, mat, 0x0000FF00, 0);
    }
    {
        Vec a = {0.0f, 0.0f, 0.0f};
        a.x = pos->x;
        a.y = pos->y - s;
        a.z = pos->z;
        p = a;
        Draw_line3d_local(pos, &p, mat, 0x00FFFFFF, 0);
    }
    {
        Vec a = {0.0f, 0.0f, 0.0f};
        a.x = pos->x;
        a.y = pos->y;
        a.z = pos->z + s;
        p = a;
        Draw_line3d_local(pos, &p, mat, 0x000000FF, 0);
    }
    {
        Vec a = {0.0f, 0.0f, 0.0f};
        a.x = pos->x;
        a.y = pos->y;
        a.z = pos->z - s;
        p = a;
        Draw_line3d_local(pos, &p, mat, 0x00FFFFFF, 0);
    }
}

// Ground grid of (2n + 1) lines each way, `step` units apart, on the y = 0 plane.
void Draw_floor(int size, int num, u32 col)
{
    Vec a;
    Vec b;
    int i;

    for (i = -num; i <= num; i++) {
        a.x = (f32) (-size * num);
        a.y = 0.0f;
        a.z = (f32) (i * size);
        b.x = (f32) (size * num);
        b.y = 0.0f;
        b.z = (f32) (i * size);
        Draw_line3d(&a, &b, col, 0);
        a.x = (f32) (i * size);
        a.y = 0.0f;
        a.z = (f32) (-size * num);
        b.x = (f32) (i * size);
        b.y = 0.0f;
        b.z = (f32) (size * num);
        Draw_line3d(&a, &b, col, 0);
    }
}

// Builds the sphere display list (16 latitude x 16 longitude line loops).
void init_sphere()
{
    static const f32 tbl[15] = {
        -0.7f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -2.0f, 0.0f, 1.0f, 20.0f,
    };
    Vec v0;
    Vec v1;
    int i, j;

    GXBeginDisplayList(sphere_buff, 0x3040);
    GXBegin(0xA8, 0, 0x400);
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            f32 th0, th1;
            th0 = (f32) j * (2.0f * PI) / 16.0f;
            v0.x = COSF(th0) * SINF((f32) i * PI / 16.0f);
            v0.y = -COSF((f32) i * PI / 16.0f);
            v0.z = SINF(th0) * SINF((f32) i * PI / 16.0f);
            th1 = (f32) (j + 1) * (2.0f * PI) / 16.0f;
            v1.x = COSF(th1) * SINF((f32) i * PI / 16.0f);
            v1.y = -COSF((f32) i * PI / 16.0f);
            v1.z = SINF(th1) * SINF((f32) i * PI / 16.0f);
            GXPosition3f32(v0.x, v0.y, v0.z);
            GXPosition3f32(v1.x, v1.y, v1.z);
        }
    }
    for (j = 0; j < 16; j++) {
        for (i = 0; i < 16; i++) {
            f32 th0;
            th0 = (f32) j * (2.0f * PI) / 16.0f;
            v0.x = COSF(th0) * SINF((f32) i * PI / 16.0f);
            v0.y = -COSF((f32) i * PI / 16.0f);
            v0.z = SINF(th0) * SINF((f32) i * PI / 16.0f);
            v1.x = COSF(th0) * SINF((f32) (i + 1) * PI / 16.0f);
            v1.y = -COSF((f32) (i + 1) * PI / 16.0f);
            v1.z = SINF(th0) * SINF((f32) (i + 1) * PI / 16.0f);
            GXPosition3f32(v0.x, v0.y, v0.z);
            GXPosition3f32(v1.x, v1.y, v1.z);
        }
    }
    GXEndDisplayList();
}

// Builds the unit circle display list (16 segments).
void init_circle()
{
    Vec v0;
    Vec v1;
    f32 th0;
    f32 th1;
    int i;

    GXBeginDisplayList(circle_buff, 0x4C0);
    GXBegin(0xA8, 0, 0x30);
    for (i = 0; i < 16; i++) {
        th0 = (f32) i * (2.0f * PI) / 16.0f;
        v0.x = COSF(th0);
        v0.z = SINF(th0);
        th1 = (f32) (i + 1) * (2.0f * PI) / 16.0f;
        v1.x = COSF(th1);
        v1.z = SINF(th1);
        GXPosition3f32(v0.x, 0.0f, v0.z);
        GXPosition3f32(v1.x, 0.0f, v1.z);
    }
    GXEndDisplayList();
}

// Builds the unit cylinder display list (two rings and 16 vertical lines).
void init_cylinder()
{
    Vec v0;
    Vec v1;
    f32 th0;
    f32 th1;
    int i;

    GXBeginDisplayList(cylinder_buff, 0x4C0);
    GXBegin(0xA8, 0, 0x60);
    for (i = 0; i < 16; i++) {
        th0 = (f32) i * (2.0f * PI) / 16.0f;
        v0.x = COSF(th0);
        v0.z = SINF(th0);
        th1 = (f32) (i + 1) * (2.0f * PI) / 16.0f;
        v1.x = COSF(th1);
        v1.z = SINF(th1);
        GXPosition3f32(v0.x, 0.0f, v0.z);
        GXPosition3f32(v1.x, 0.0f, v1.z);
        GXPosition3f32(v0.x, 1.0f, v0.z);
        GXPosition3f32(v1.x, 1.0f, v1.z);
        GXPosition3f32(v0.x, 0.0f, v0.z);
        GXPosition3f32(v0.x, 1.0f, v0.z);
    }
    GXEndDisplayList();
}

// Builds the unit cone display list.
void init_corn()
{
    Vec v0;
    Vec v1;
    f32 th0, th1;
    int i, j;

    GXBeginDisplayList(corn_buff, 0x3040);
    GXBegin(0xA8, 0, 0x200);
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            th0 = (f32) j * (2.0f * PI) / 16.0f;
            v0.x = COSF(th0) * ((f32) i / 16.0f);
            v0.y = SINF(th0) * ((f32) i / 16.0f);
            th1 = (f32) (j + 1) * (2.0f * PI) / 16.0f;
            v1.x = COSF(th1) * ((f32) i / 16.0f);
            v1.y = SINF(th1) * ((f32) i / 16.0f);
            GXPosition3f32(v0.x, v0.y, (f32) i / 16.0f);
            GXPosition3f32(v1.x, v1.y, (f32) i / 16.0f);
        }
    }
    GXEndDisplayList();
}

// `psq_l f,0(p),1,qr5` straight from the vertex pointer: inline asm in the original (the compiler
// always converts an s16 through a stack slot).
#define PSQ_L_S16(p) ({ f32 f_; asm volatile("psq_l %0,0(%1),1,5" : "=f"(f_) : "b"(p)); f_; })




// Byte-identical (was 332 words). The original drives the conversions and the FIFO writes through ONE
// function-scope `Vec* pv` (`mr r31, r24` = pv = p before each loop, `mr r31, r23` = pv = &p[2]), a
// `u16* pidx` re-assigned per command, ONE function-scope `s16* v` for every conversion (a multi-set
// pseudo, so the `add r11,r16,r0` is not tied to the dying shifted index), keeps `part` in r14 and
// advances it in place, spills `obj` (0x40(r1)), `md`, `np` and caller-saves cg/ca (0x50/0x54)
// around PSMTXMultVec, writes the loop bounds as literals (`m < 4` folds to `cmplwi 3; ble`), and
// uses `cnt = n - 2` as the strip bound.
// Shapes: the command loop is `do { if (cmd >= part) break; ... } while (1);` (expand_end_loop's
// "condjump near the end" rule ends the loop early and skips the rotation: test at the top, `b top`
// from every case, no duplicated bottom test); `DB_poly_num = DB_poly_num + part->nPoly` is a plain
// global store (the `part->size` load stays below it with the mem-flags compiler); `idx` is an 8-byte ADDRESSOF aggregate, so its
// element stores are written `*pidx++ = ...` (cse1 rewrites `(mem pidx)` to the addressof / frame
// address: frame-direct `sth 56..62(r1)`, while `idx[k] = ...` creates an address temp that cse merges
// with the `pidx = idx` pseudo -> `sth 2(r29)`); the strip's `idx[2] = idx[1]; pidx = &idx[1];` puts the
// idx[2] address temp first in its block (gcse PRE copy from `&idx`, `sth r0,4(r27)`) and lets combine
// fuse `pidx = &idx + 2` with the idx[1] load into `lhzu` after a reload copy `mr r29,r27`; the
// 2-vertex strip emit loop has its own `u32 m2` counter (caller-saved r11; `m` crosses calls).
// `vtx_size` is an unused non-static local (8-byte .rodata template between init_corn's pool and
// this function's pool; a `static const` lands in .sdata2).
void DrawObjWireframe(cObj* pObj, int col)
{
    const u8 vtx_size[8] = {8, 8, 10, 12, 10, 8, 8, 0};
    cModelData* md;
    ModelPart* part;
    u8* cmd;
    s16* vtx;
    s16* v;
    f32 scale;
    Vec p[4];
    u16 idx[4];
    Vec* pv;
    u16* pidx;
    u32 np;
    u32 n, k, m, cnt;
    u8 op;
    u8 cr, cg, cb, ca;

    if (pObj == NULL) {
        return;
    }
    if (!pObj->isAlive()) {
        return;
    }
    Draw_line3d_init();
    md = pObj->pModelInfo->model_addr;
    scale = 1.0f / (f32) (1 << md->shift);
    vtx = (s16*) md->vtxOrig;
    part = md->pParts;
    for (np = 0; np < md->displist_num; np++) {
        DB_poly_num = DB_poly_num + part->nPoly;
        cmd = (u8*) part + 0x20;
        part = (ModelPart*) ((u8*) part + part->size + 0x20);
        do {
            if (cmd >= (u8*) part) {
                break;
            }
            op = *cmd++;
            switch (op) {
            case 0:
                break;
            case 0x80:
                n = *(u16*) cmd;
                cmd += 2;
                DB_quads_num += n;
                cr = 0x20;
                cg = 0x20;
                cb = 0x8F;
                ca = 0xFF;
                for (k = 0; k < n; k += 4) {
                    pv = p;
                    pidx = idx;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    pidx = idx;
                    for (m = 0; m < 4; m++) {
                        v = (s16*) ((u8*) vtx + *pidx * 8);
                        pidx++;
                        pv->x = PSQ_L_S16(v);
                        pv->y = PSQ_L_S16(v + 1);
                        pv->z = PSQ_L_S16(v + 2);
                        pv->x *= scale;
                        pv->y *= scale;
                        pv->z *= scale;
                        PSMTXMultVec(pObj->mat, pv, pv);
                        pv++;
                    }
                    GXBegin(0xB0, 0, 6);
                    pv = p;
                    for (m = 0; m < 4; m++) {
                        GXMatrixIndex1u8(0);
                        GXPosition3f32(pv->x, pv->y, pv->z);
                        GXColor4u8(cr, cg, cb, ca);
                        pv++;
                    }
                    GXMatrixIndex1u8(0);
                    pv = p;
                    GXPosition3f32(pv->x, pv->y, pv->z);
                    GXColor4u8(cr, cg, cb, ca);
                    GXMatrixIndex1u8(0);
                    pv = &p[2];
                    GXPosition3f32(pv->x, pv->y, pv->z);
                    GXColor4u8(cr, cg, cb, ca);
                }
                break;
            case 0x90:
                n = *(u16*) cmd;
                cmd += 2;
                DB_tri_num += n;
                cr = 0x80;
                cg = 0x20;
                cb = 0x20;
                ca = 0xFF;
                for (k = 0; k < n; k += 3) {
                    pv = p;
                    pidx = idx;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    *pidx++ = *(u16*) cmd;
                    cmd += 8;
                    pidx = idx;
                    for (m = 0; m < 3; m++) {
                        v = (s16*) ((u8*) vtx + *pidx * 8);
                        pidx++;
                        pv->x = PSQ_L_S16(v);
                        pv->y = PSQ_L_S16(v + 1);
                        pv->z = PSQ_L_S16(v + 2);
                        pv->x *= scale;
                        pv->y *= scale;
                        pv->z *= scale;
                        PSMTXMultVec(pObj->mat, pv, pv);
                        pv++;
                    }
                    GXBegin(0xB0, 0, 4);
                    pv = p;
                    for (m = 0; m < 3; m++) {
                        GXMatrixIndex1u8(0);
                        GXPosition3f32(pv->x, pv->y, pv->z);
                        GXColor4u8(cr, cg, cb, ca);
                        pv++;
                    }
                    GXMatrixIndex1u8(0);
                    pv = p;
                    GXPosition3f32(pv->x, pv->y, pv->z);
                    GXColor4u8(cr, cg, cb, ca);
                }
                break;
            case 0x98:
                pv = p;
                n = *(u16*) cmd;
                pidx = idx;
                DB_strip_num += n;
                cr = 0x80;
                cg = 0x80;
                cb = 0x80;
                ca = 0xFF;
                cmd += 2;
                *pidx++ = *(u16*) cmd;
                cmd += 8;
                *pidx++ = *(u16*) cmd;
                cmd += 8;
                pidx = idx;
                for (m = 0; m < 2; m++) {
                    v = (s16*) ((u8*) vtx + *pidx * 8);
                    pidx++;
                    pv->x = PSQ_L_S16(v);
                    pv->y = PSQ_L_S16(v + 1);
                    pv->z = PSQ_L_S16(v + 2);
                    pv->x *= scale;
                    pv->y *= scale;
                    pv->z *= scale;
                    PSMTXMultVec(pObj->mat, pv, pv);
                    pv++;
                }
                cnt = n - 2;
                GXBegin(0xB0, 0, 2);
                pv = p;
                for (u32 m2 = 0; m2 < 2; m2++) {
                    GXMatrixIndex1u8(0);
                    GXPosition3f32(pv->x, pv->y, pv->z);
                    GXColor4u8(cr, cg, cb, ca);
                    pv++;
                }
                idx[2] = idx[1];
                pidx = &idx[1];
                p[2] = p[1];
                for (k = 0; k < cnt; k++) {
                    pv = &p[1];
                    *pidx = *(u16*) cmd;
                    cmd += 8;
                    v = (s16*) ((u8*) vtx + *pidx * 8);
                    pv->x = PSQ_L_S16(v);
                    pv->y = PSQ_L_S16(v + 1);
                    pv->z = PSQ_L_S16(v + 2);
                    pv->x *= scale;
                    pv->y *= scale;
                    pv->z *= scale;
                    PSMTXMultVec(pObj->mat, pv, pv);
                    pv = p;
                    GXBegin(0xB0, 0, 3);
                    for (m = 0; m < 3; m++) {
                        GXMatrixIndex1u8(0);
                        GXPosition3f32(pv->x, pv->y, pv->z);
                        GXColor4u8(cr, cg, cb, ca);
                        pv++;
                    }
                    idx[0] = idx[2];
                    idx[2] = idx[1];
                    p[0] = p[2];
                    p[2] = p[1];
                }
                break;
            default:
                Draw_line3d_end();
                return;
            }
        } while (1);
    }
    Draw_line3d_end();
}

// Debug: draws every live scroll object as a wireframe and counts the primitives (DB_*_num).
void DrawRoomWireframe()
{
    cObj* obj;

    DB_poly_num = 0;
    DB_quads_num = 0;
    DB_tri_num = 0;
    DB_strip_num = 0;
    for (obj = ObjMgr.getActiveWork(); obj; obj = ObjMgr.getNext(obj)) {
        if ((obj->be_flag & 2) && obj->id == 2) {
            DrawObjWireframe(obj, -1);
        }
    }
    eprintf(32, 350, 0, 0, "Poly    = %06d", DB_poly_num);
    eprintf(32, 364, 0, 0, "Quads   = %06d (Line Blue)", DB_quads_num);
    eprintf(32, 378, 0, 0, "Triangle= %06d (Line Red)", DB_tri_num);
    eprintf(32, 392, 0, 0, "Strip   = %06d (Line White)", DB_strip_num);
}

// Prints `time` (frames, 30 / s) as hh:mm:ss:ff; flag bits 8 / 4 / 2 / 1 select hours /
// minutes / seconds / frames.
void DispTime(s16 x, int y, int col, int time, int flag)
{
    int frame, sec, min, hour;
    int s, m;
    int first = 1;

    frame = time % 30;
    time -= frame;
    s = time / 30;
    sec = s % 60;
    time = s - sec;
    m = time / 60;
    min = m % 60;
    time = m - min;
    hour = time / 60;
    frame *= 3;

    if (flag & 8) {
        eprintf(x, y, col, 0, "%02d", hour);
        x += 16;
        first = 0;
    }
    if (flag & 4) {
        if (!first) {
            eprintf(x, y, col, 0, ":");
            x += 8;
        }
        eprintf(x, y, col, 0, "%02d", min);
        x += 16;
        first = 0;
    }
    if (flag & 2) {
        if (!first) {
            eprintf(x, y, col, 0, ":");
            x += 8;
        }
        eprintf(x, y, col, 0, "%02d", sec);
        x += 16;
        first = 0;
    }
    if (flag & 1) {
        if (!first) {
            eprintf(x, y, col, 0, ":");
            x += 8;
        }
        eprintf(x, y, col, 0, "%02d", frame);
    }
}

// The pad array for the bugcheck tool (pad 0).
JOY* GetBugCheckController()
{
    return Joy;
}
