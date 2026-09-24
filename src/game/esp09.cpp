// game/esp09.cpp: effect id 0x09, a position trail of maxPoints (4 - Work8[0], 2..6) points kept
// in a ring buffer, fading along its length. Without a texture (Tex_id 0xFF) it is a GX line
// strip of width Size_base_x * Size_mul * 0.03 (thinner with distance); with a texture, a strip of
// quads facing the camera. flg (Work8[1]) bit0 keeps the points in screen space (2D trail,
// blanked by a Z-buffer test after the render), bit1 records a point only every other frame.

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "main_sub.h"
#include "view.h"


struct Esp09Work {
    s8 maxPoints;         // 0x00 number of trail points (2..6)
    u8 flg;     // 0x01 bit0: screen space, bit1: record a point every other frame (gen->Work8[1])
    u8 pad_2[10];
    u8 Hide_flg;    // 0x0C set by the Z-buffer test (screen space trail)
    u8 nPos;       // 0x0D ring buffer index of the newest point
    s16 Width;    // 0x0E line width
    Vec Pos[6];   // 0x10 position history (screen space: z = distance to the camera)
};

// Position trail drawn as a line strip (or, with a texture, as a strip of quads).
class cEsp09 : public cEsp {
public:
    Esp09Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void Esp09_ClearPrevPos(cEsp09* esp);
void EspChannelSet09(cEsp09* esp);
void Esp09_Trans_Setup(cEsp09* esp);
void Esp09_2DTrans(cEsp09* esp, u8 r, u8 g, u8 b, u8 a);
void Esp09_3DTrans(cEsp09* esp, u8 r, u8 g, u8 b, u8 a);
void Esp09_PolyTrans(cEsp09* esp, u8 r, u8 g, u8 b, u8 a);
void Esp09_StripDrawPoly(cEsp09* esp, int no, Vec* v, u8 r, u8 g, u8 b, u8* a);
f32 GetVecLen(Vec* a, Vec* b);
void Esp09_HideCheck(cEsp* esp);
}

// EspCreateTbl[0x09] factory.
cEsp* Esp09_Create()
{
    return new cEsp09;
}

// Fill the whole history with the current position.
void Esp09_ClearPrevPos(cEsp09* pEsp)
{
    Esp09Work* w = &pEsp->m_Free;
    CAMERA* cam = &pG->Camera;
    Vec* p = &w->Pos[0];
    Vec tmp;
    f32 len;
    int i;

    if (pEsp->m_Id != 9 || w->maxPoints <= 1 || w->maxPoints > 6) {
        pLog->err(0, 0, "Esp09:ERROR![ID:%d / nPnt:%d]", pEsp->m_Id, w->maxPoints);
        return;
    }
    for (i = 0; i < w->maxPoints; i++) {
        if (pEsp->parent == pEffParentWorld) {
            *p = pEsp->m_Pos;
        } else {
            PSMTXMultVec(pEsp->parent->mat, &pEsp->m_Pos, p);
        }
        len = GetVecLen(p, &cam->param.pos);
        if (w->flg & 1) {
            tmp = *p;
            GetScreenPos(&tmp, p);
            p->z = len;
        }
        p++;
    }
}

// Base update, then Rno0 0 (first frame) fills the history with the current position, Rno0 1
// advances the ring index (every frame or every other with flg bit1; m_Type != 0 uses only 3
// slots) and stores the current world (or screen, flg bit0) position. Recomputes the line width
// for untextured trails and queues Esp09_HideCheck after the render.
void cEsp09::move()
{
    Esp09Work* w = &m_Free;
    CAMERA* cam = &pG->Camera;
    Vec* p;
    Vec tmp;
    f32 len = 0.0f;

    if (!CommonMove()) {
        return;
    }
    switch (m_Rno0) {
    case 0:
        w->Hide_flg = 1;
        Esp09_ClearPrevPos(this);
        m_Rno0++;
        break;
    case 1:
        if (w->flg & 2) {
            m_Rno1++;
            if (m_Rno1 & 1) {
                w->nPos++;
            }
        } else {
            w->nPos++;
        }
        if (m_Type != 0) {
            if (w->nPos > 2) {
                w->nPos = 0;
            }
        } else {
            if (w->nPos > w->maxPoints - 1) {
                w->nPos = 0;
            }
        }
        p = &w->Pos[w->nPos];
        if (m_Parts_no > ESP_PARTS_SCREEN) {
            *p = m_Pos;
        } else {
            PSMTXMultVec(parent->mat, &m_Pos, p);
        }
        len = GetVecLen(p, &cam->param.pos);
        if (w->flg & 1) {
            tmp = *p;
            GetScreenPos(&tmp, p);
            p->z = len;
        }
        break;
    }
    if (m_Tex_id == 0xFF) {
        w->Width = (u16)(m_Size_base_x * m_Size_mul * (0.3f * 0.1f));
        len /= 5000.0f;
        if (len > 1.0f) {
            w->Width = (f32)w->Width / len;
        }
    }
    if (m_Be_flg & 1) {
        EspAddOtAfterRender(this, Esp09_HideCheck);
    }
}

// EspTransTbl[0x09]: common GX setup, then the textured quad strip, the 2D line strip or the 3D
// line strip depending on Tex_id and flg bit0.
extern "C" void Esp09_Trans(cEsp09* esp)
{
    Esp09Work* w = &esp->m_Free;
    u8 r = (u8)esp->m_Col_r;
    u8 g = (u8)esp->m_Col_g;
    u8 b = (u8)esp->m_Col_b;
    u8 a = (u8)esp->m_Col_a;

    Esp09_Trans_Setup(esp);
    GXSetLineWidth(w->Width, 0);
    if (esp->m_Tex_id != 0xFF) {
        Esp09_PolyTrans(esp, r, g, b, a);
    } else if (w->flg & 1) {
        Esp09_2DTrans(esp, r, g, b, a);
    } else {
        Esp09_3DTrans(esp, r, g, b, a);
    }
    GXSetLineWidth(6, 0);
}

// Colour channel / TEV setup for the trail: vertex colour only; Tool_flg 0x80 adds the raster
// colour with a x4 scale (bright additive trail).
void EspChannelSet09(cEsp09* esp)
{
    GXSetTevOp(0, 0);
    if (esp->m_Tool_flg & 0x80) {
        GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
        GXSetTevColorOp(0, 0, 0, 2, 1, 0);
    }
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
}

// Z / cull / TEV state, ortho projection with identity matrix (2D) or the camera view matrix
// (3D), blend mode and the position + colour (+ texcoord when textured) vertex format.
void Esp09_Trans_Setup(cEsp09* esp)
{
    Esp09Work* w = &esp->m_Free;

    GXSetZMode(1, 3, 0);
    GXSetCullMode(0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    if (w->flg & 1) {
        Mtx44 proj;
        Mtx m;

        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
        GXSetProjection(proj, 1);
        PSMTXIdentity(m);
        GXLoadPosMtxImm(m, 0);
        GXSetCurrentMtx(0);
    } else {
        CameraCurrentProjection();
        GXLoadPosMtxImm(pG->Camera.v_mat, 0);
        GXSetCurrentMtx(0);
    }
    EspChannelSet09(esp);
    GXSetTevOp(0, 4);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetBlendMode(esp->m_Blend_mode, esp->m_Src_factor, esp->m_Dst_factor, esp->m_Logic_op);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    if (esp->m_Tex_id != 0xFF) {
        EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    }
}

// Screen-space line strip through the ring buffer, newest first, alpha stepping down to 0 at
// the tail; skipped while the trail is hidden behind geometry.
void Esp09_2DTrans(cEsp09* pEsp, u8 r, u8 g, u8 b, u8 a)
{
    Esp09Work* w = &pEsp->m_Free;
    Vec p;
    s8 n1 = w->maxPoints - 1;
    u8 step = (u8)(pEsp->m_Col_a / (f32)n1);
    int idx = w->nPos;
    int i;

    if (w->Hide_flg != 0) {
        return;
    }
    GXBegin(0xB0, 0, (u16)w->maxPoints);
    for (i = 0; i < w->maxPoints; i++) {
        p = w->Pos[idx];
        p.z = 0.0f;
        GXPosition3f32(p.x, p.y, p.z);
        GXColor4u8(r, g, b, a);
        a -= step;
        idx--;
        if (idx < 0) {
            idx = n1;
        }
    }
}

// World-space line strip through the ring buffer, newest first, alpha fading toward the tail.
void Esp09_3DTrans(cEsp09* pEsp, u8 r, u8 g, u8 b, u8 a)
{
    Esp09Work* w = &pEsp->m_Free;
    Vec* p;
    s8 n1 = w->maxPoints - 1;
    u8 step = (u8)(pEsp->m_Col_a / (f32)n1);
    int idx = w->nPos;
    int i;

    GXBegin(0xB0, 0, (u16)w->maxPoints);
    for (i = 0; i < w->maxPoints; i++) {
        p = &w->Pos[idx];
        GXPosition3f32(p->x, p->y, p->z);
        GXColor4u8(r, g, b, a);
        a -= step;
        idx--;
        if (idx < 0) {
            idx = n1;
        }
    }
}

// Textured trail: for each consecutive pair of history points builds a quad perpendicular to
// the segment and the camera (or the screen plane in 2D, scaled by 500 / depth), width
// interpolated Size_base_x -> Size_base_y (x 0.1) and shrunk by Size_plus per segment, and
// draws it with Esp09_StripDrawPoly.
void Esp09_PolyTrans(cEsp09* esp, u8 r, u8 g, u8 b, u8 a)
{
    Esp09Work* w = &esp->m_Free;
    Vec d;
    Vec up;
    Vec q[2];
    Vec v[4];
    Vec* p;
    Vec* p0;
    Vec* pp;
    Vec* s;
    Vec* pn;
    int idx = w->nPos;
    s8 n1 = w->maxPoints - 1;
    int i = 0;
    int first = 0;
    f32 spd;
    f32 rate;
    f32 half;

    esp->m_Size_mul = 1.0f;
    spd = esp->m_Size_plus;
    // The next point pn is recomputed from idx: loop.c strength-reduces it as a giv of the biv idx
    // (`addi -12` after idx--, `add r25,r14,r20` after the wrap) and its preheader init folds to
    // the block-0 temporary `s` (kept as cse's head by the dead trailing `p = s`). The Subtract
    // argument reads the giv register (`mr r3,r25`) and `p = pn` is a codeless asm whose input is
    // tied to the output (a plain copy makes p a second giv; loop.c never derives a giv from an
    // ASM_OPERANDS): regmove's matching-constraint fixup emits the one `mr r29,r25`. That copy is one
    // insn more than the asm alone inside pp's live range (pp 14/126 = esp 17/204 = 3333 in
    // global-alloc), so a codeless `"=m"` anchor at the loop top, outside pp's range, lengthens esp
    // (17/205) and keeps the target's order p > pp > esp.
    // idx-- between the two copies puts the giv `addi` before
    // `mr pp,p`. The two codeless asms give p (2 in-loop mentions -> 17 refs) and pp (4 -> 14 refs)
    // the target's global-alloc order p r29 > pp r28 > esp r27 (ours ranked esp first); the pp asm
    // sits after the second PSVECAdd with a memory input written by that call so it takes no issue
    // slot before the `bl`. The wrap-arm asm also keeps `pp = p` reading p (regmove).
    s = &w->Pos[idx];
    p = s;
    for (i = 0; i < n1; i++) {
        asm("" : "=m"(d));  // COMPILER-DIFF: candidate (global-alloc priority): +1 insn in esp's range only
        p0 = p;
        idx--;
        pp = p;
        if (idx < 0) {
            idx = n1;
            asm("" : "=m"(d) : "r"(p), "r"(p));  // COMPILER-DIFF: candidate (global-alloc priority)
        }
        pn = &w->Pos[idx];
        asm("" : "=r"(p) : "0"(pn));  // COMPILER-DIFF: candidate (cse canonical register): tied copy, regmove emits the mr
        rate = (f32)i / (f32)n1;
        half = (rate * esp->m_Size_base_y + (1.0f - rate) * esp->m_Size_base_x) * 0.1f;
        PSVECSubtract(pn, p0, &d);
        if (w->flg & 1) {
            up.x = 0.0f;
            up.y = 0.0f;
            up.z = 1.0f;
            half *= 500.0f / p0->z;
        } else {
            PSVECSubtract(&pG->Camera.param.pos, p0, &up);
        }
        PSVECCrossProduct(&d, &up, &up);
        if (up.x == 0.0f && up.y == 0.0f && up.z == 0.0f) {
            continue;
        }
#line 463 "D:/Bio4/Prog/esp09.cpp"
        VECNormalize(&up, &up);
        half *= esp->m_Size_mul;
        esp->m_Size_mul += spd;
        spd *= esp->m_D_size_plus;
        if (esp->m_Size_mul < 0.0f) {
            i = n1;
        }
        PSVECScale(&up, &q[0], half);
        PSVECScale(&up, &q[1], -half);
        if (first == 0) {
            PSVECAdd(pp, &q[0], &v[0]);
            PSVECAdd(pp, &q[1], &v[1]);
            asm("" : "=m"(d) : "r"(pp), "r"(pp), "r"(pp), "r"(pp), "m"(v[1].x));  // COMPILER-DIFF: candidate (global-alloc priority)
            first = 1;
        } else {
            v[0] = v[3];
            v[1] = v[2];
        }
        PSVECAdd(p, &q[0], &v[3]);
        PSVECAdd(p, &q[1], &v[2]);
        Esp09_StripDrawPoly(esp, i, v, r, g, b, &a);
    }
    p = s;  // dead: keeps s as cse's canonical register (the giv init copies from s, not p)
}

// Emits quad `no` of the textured trail with texture slice no / maxPoints along t, and steps the
// caller's alpha down by one fade step for the far edge.
void Esp09_StripDrawPoly(cEsp09* esp, int no, Vec* v, u8 r, u8 g, u8 b, u8* a)
{
    Esp09Work* w = &esp->m_Free;
    EspAnmData* anm;
    int step = (u8)(esp->m_Col_a / (w->maxPoints - 1));
    f32 s;
    f32 s2;
    f32 sw;
    f32 t0;
    f32 t1;

    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    sw = 1.0f / (f32)w->maxPoints;
    t1 = 1.0f;
    t0 = 0.0f;
    s = sw * no + t0;
    GXBegin(0x80, 0, 4);
    s2 = s + sw;
    GXPosition3f32(v->x, v->y, v->z);
    GXColor4u8(r, g, b, *a);
    GXTexCoord2f32(t0, s);
    v++;
    GXPosition3f32(v->x, v->y, v->z);
    GXColor4u8(r, g, b, *a);
    GXTexCoord2f32(t1, s);
    *a -= step;
    v++;
    GXPosition3f32(v->x, v->y, v->z);
    GXColor4u8(r, g, b, *a);
    GXTexCoord2f32(t1, s2);
    v++;
    GXPosition3f32(v->x, v->y, v->z);
    GXColor4u8(r, g, b, *a);
    GXTexCoord2f32(t0, s2);
}

// Distance between two points.
f32 GetVecLen(Vec* a, Vec* b)
{
    Vec d;

    PSVECSubtract(a, b, &d);
    return PSVECMag(&d);
}

// Z-buffer test at the projected position: a hidden trail is cleared when it reappears.
void Esp09_HideCheck(cEsp* esp0)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static s32 Zs_bias = -5000;
    static s32 Zs_bias_2 = 0;  // unreferenced 4-byte .sdata word after Zs_bias (name unknown)
    cEsp09* esp = (cEsp09*)esp0;
    Esp09Work* w = &esp->m_Free;
    Vec v;
    Vec s;
    Mtx m;
    u32 z;
    s32 zi;
    f32 nz;
    f32 inv;
    f32 m22;
    f32 m23;
    f32 zv;
    u8 old = w->Hide_flg;

    PSMTXConcat(pG->Camera.v_mat, esp->parent->mat, m);
    PSMTXMultVec(m, &esp->m_Pos, &v);
    PSMTX44MultVec(pG->Camera.ProjMat, &v, &s);
    s.x = (s.x * 0.5f + 0.5f) * Screen.width;
    s.y = (-s.y * 0.5f + 0.5f) * Screen.height;
    nz = v.z + 150.0f;
    inv = 1.0f / (ZFAR - ZNEAR);
    m22 = -(ZNEAR) * inv;
    m23 = -(ZFAR * ZNEAR) * inv;
    zv = (1.0f / -nz) * ((m23 + m22 * nz) * Zscale) + Zoffset;
    v.z = nz;
    zi = (u32)(zv * 16777215.0f);
    if (s.x >= 0.0f && s.x <= 639.0f && s.y >= 0.0f && s.y <= 527.0f) {
        GXPixModeSync();
        GXDrawDone();
        GXPeekZ((u16)s.x, (u16)s.y, &z);
        if (zi > (s32)(z - Zs_bias)) {
            w->Hide_flg = 1;
        } else {
            w->Hide_flg = 0;
        }
    } else {
        w->Hide_flg = 1;
    }
    if (w->Hide_flg == 0 && old == 1) {
        Esp09_ClearPrevPos(esp);
    }
}

// Point count 4 - Work8[0] clamped to 2..6, flags from Work8[1].
int cEsp09::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp09Work* w = &m_Free;

    w->maxPoints = 4 - pSeq->Work8[0];
    w->flg = pSeq->Work8[1];
    if (w->maxPoints <= 1) {
        w->maxPoints = 2;
    } else if (w->maxPoints > 6) {
        w->maxPoints = 6;
    }
    return 1;
}
