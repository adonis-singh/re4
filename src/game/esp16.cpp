// game/esp16.cpp: effect id 0x16, a rope / chain of Num (Work8[0] + 2) points simulated with
// distance constraints (segment length Vec0.x, damping Vec0.y %, constraint feedback Vec0.z %,
// gravity Vec1, jitter Vec2). Point 0 follows the effect; with Work8[1] the last point is
// pinned to model parts Work8[1] - 1 and the chain is relaxed from that end too. Positions and
// speeds live in two esp3f buffers; the trans draws a camera-facing textured strip through them.

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp16Work {
    u32 Num;        // 0x00 number of chain points (gen->Work8[0] + 2)
    cEsp3f* pPosBuf;    // 0x04 point positions
    cEsp3f* pSpdBuf;    // 0x08 point speeds
    Vec grav;       // 0x0C acceleration added every frame (gen->Vec1)
    Vec rand_plus;        // 0x18 random jitter amplitude (gen->Vec2)
    f32 nen;    // 0x24 how much of the constraint correction feeds back into the speed (gen->Vec0.z / 100)
    f32 del;       // 0x28 speed damping (gen->Vec0.y / 100)
    f32 max_len;        // 0x2C segment length (gen->Vec0.x)
    cParts* pParts;  // 0x30 model part the far end is attached to
};

// Rope / chain: a string of points held together by distance constraints, drawn as a textured
// strip facing the camera. The head follows the effect position, the tail can be attached to
// a model part.
class cEsp16 : public cEsp {
public:
    Esp16Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

// EspCreateTbl[0x16] factory.
cEsp* Esp16_Create()
{
    return new cEsp16;
}

// Base update / animation, then one relaxation pass from the head: each point moves by its speed,
// is pulled back to max_len from its predecessor (with the along-segment speed cancelled), gets
// the random jitter, the nen feedback split between neighbours, gravity and damping; then the
// head is set to the effect position and, when pinned, the tail to the parts and a second pass
// runs from the tail toward the head.
void cEsp16::move()
{
    Esp16Work* w = &m_Free;
    Vec pos0;
    Vec d;
    Vec nrm;
    Vec* p;
    Vec* s;
    u32 i;

    if (!CommonMove()) {
        return;
    }
    if (!AnmMove()) {
        PushEsp(this);
        return;
    }
    m_Radius = 100000000.0f;
    if (parent == pEffParentWorld) {
        pos0 = m_Pos;
    } else {
        PSMTXMultVec(parent->mat, &m_Pos, &pos0);
    }
    for (i = 1; i < w->Num; i++) {
        p = Esp3f_GetVecPtr(w->pPosBuf, i);
        s = Esp3f_GetVecPtr(w->pSpdBuf, i);
        PSVECAdd(p, s, Esp3f_GetVecPtr(w->pPosBuf, i));
        PSVECSubtract(Esp3f_GetVecPtr(w->pPosBuf, i - 1), Esp3f_GetVecPtr(w->pPosBuf, i), &d);
        if (PSVECMag(&d) > w->max_len) {
            static f32 nen_mul = 1.0f;

#line 63 "D:/Bio4/Prog/esp16.cpp"
            VECNormalize(&d, &nrm);
            PSVECScale(&nrm, &d, -w->max_len);
            PSVECAdd(Esp3f_GetVecPtr(w->pPosBuf, i - 1), &d, Esp3f_GetVecPtr(w->pPosBuf, i));
            PSVECScale(&nrm, &d, -PSVECDotProduct(&nrm, Esp3f_GetVecPtr(w->pSpdBuf, i)) * nen_mul);
            PSVECAdd(Esp3f_GetVecPtr(w->pSpdBuf, i), &d, Esp3f_GetVecPtr(w->pSpdBuf, i));
        }
        {
            Vec jit;

            jit.x = w->rand_plus.x * fRand1_1();
            jit.y = w->rand_plus.y * fRand1_1();
            jit.z = w->rand_plus.z * fRand1_1();
            PSVECAdd(Esp3f_GetVecPtr(w->pPosBuf, i), &jit, Esp3f_GetVecPtr(w->pPosBuf, i));
        }
        PSVECScale(&d, &d, w->nen);
        PSVECAdd(Esp3f_GetVecPtr(w->pSpdBuf, i), &d, Esp3f_GetVecPtr(w->pSpdBuf, i));
        PSVECSubtract(Esp3f_GetVecPtr(w->pSpdBuf, i - 1), &d, Esp3f_GetVecPtr(w->pSpdBuf, i - 1));
        PSVECAdd(Esp3f_GetVecPtr(w->pSpdBuf, i), &w->grav, Esp3f_GetVecPtr(w->pSpdBuf, i));
        PSVECScale(Esp3f_GetVecPtr(w->pSpdBuf, i), Esp3f_GetVecPtr(w->pSpdBuf, i), w->del);
    }
    *Esp3f_GetVecPtr(w->pPosBuf, 0) = pos0;
    if (w->pParts != NULL) {
        Vec v = { 0.0f, 0.01f, 0.0f };

        PSMTXMultVec(w->pParts->mat, &v, &v);
        *Esp3f_GetVecPtr(w->pPosBuf, w->Num - 1) = v;
        for (i = w->Num - 2; i > 1; i--) {
            static f32 nen_mul = 1.0f;

            p = Esp3f_GetVecPtr(w->pPosBuf, i);
            s = Esp3f_GetVecPtr(w->pSpdBuf, i);
            PSVECAdd(p, s, Esp3f_GetVecPtr(w->pPosBuf, i));
            PSVECSubtract(Esp3f_GetVecPtr(w->pPosBuf, i + 1), Esp3f_GetVecPtr(w->pPosBuf, i), &d);
            if (PSVECMag(&d) > w->max_len) {
#line 109 "D:/Bio4/Prog/esp16.cpp"
                VECNormalize(&d, &nrm);
                PSVECScale(&nrm, &d, -w->max_len);
                PSVECAdd(Esp3f_GetVecPtr(w->pPosBuf, i + 1), &d, Esp3f_GetVecPtr(w->pPosBuf, i));
                PSVECScale(&nrm, &d, -PSVECDotProduct(&nrm, Esp3f_GetVecPtr(w->pSpdBuf, i)) * nen_mul);
                PSVECAdd(Esp3f_GetVecPtr(w->pSpdBuf, i), &d, Esp3f_GetVecPtr(w->pSpdBuf, i));
            }
            {
                Vec jit;

                jit.x = w->rand_plus.x * fRand1_1();
                jit.y = w->rand_plus.y * fRand1_1();
                jit.z = w->rand_plus.z * fRand1_1();
                PSVECAdd(Esp3f_GetVecPtr(w->pPosBuf, i), &jit, Esp3f_GetVecPtr(w->pPosBuf, i));
            }
            PSVECScale(&d, &d, w->nen);
            PSVECAdd(Esp3f_GetVecPtr(w->pSpdBuf, i), &d, Esp3f_GetVecPtr(w->pSpdBuf, i));
            PSVECSubtract(Esp3f_GetVecPtr(w->pSpdBuf, i + 1), &d, Esp3f_GetVecPtr(w->pSpdBuf, i + 1));
            PSVECAdd(Esp3f_GetVecPtr(w->pSpdBuf, i), &w->grav, Esp3f_GetVecPtr(w->pSpdBuf, i));
            PSVECScale(Esp3f_GetVecPtr(w->pSpdBuf, i), Esp3f_GetVecPtr(w->pSpdBuf, i), w->del);
        }
    }
}

// EspTransTbl[0x16]: draws min(Life_time + 1, Num) points as a triangle strip of width
// interpolated Size_base_x -> Size_base_y, widened perpendicular to segment and view; Tool_flg
// bit1 flips s, bit2 runs t backwards.
extern "C" void Esp16_Trans(cEsp16* esp)
{
    Esp16Work* w = &esp->m_Free;
    Mtx inv;
    Vec cam;
    Vec q0;
    Vec q1;
    Vec dir;
    Vec toCam;
    Vec nrm;
    Vec cross;
    EspAnmData* anm;
    Vec* p0;
    Vec* p1;
    u32 n;
    u32 i;
    f32 t;
    f32 tw;
    f32 s0;
    f32 s1;
    f32 rate;
    f32 half;
    register f32 z asm("fr12"); // COMPILER-DIFF: #13

    if (esp->m_Life_time < w->Num) {
        n = esp->m_Life_time + 1;
    } else {
        n = w->Num;
    }
    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    CameraCurrentProjection();
    EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
    esp->ChannelSet();
    GXSetBlendMode(esp->m_Blend_mode, esp->m_Src_factor, esp->m_Dst_factor, esp->m_Logic_op);
    esp->CommonStateSet();
    // The original sets t through an intermediate the copy never absorbed (`lfs f12, 0.0; fmr
    // f29, f12`) and its block ends right after the zero load: `lbz partsNo`, the 1.0 high, the
    // copy and `lfs 1.0` are scheduled as a second block (`lis; lfs f12 | lbz; lis; fmr; lfs`).
    // The dead test below is that block boundary (compare/branch gone at flow/jump2; sched1 and
    // sched2 both run with the blocks split); with the boundary combine cannot merge the load
    // into the copy either, so no keep-alive is needed, and the zero dying at the copy ranks
    // `fmr` above `lfs 1.0`. The pinned f12 is the register the original's zero took. The test's
    // three loads (three short local qtys: r0, r9, r11 before the zero high's qty) put the high
    // in r10 like the target; a one-load compare leaves it r9.
    z = 0.0f;                   // COMPILER-DIFF: #13
    // Dead in the original too: only its 0x43300000 constant survives, shared through the cse
    // path by both `(f32) w->nPt` conversions below (`lis r31, 0x4330` right after
    // CameraCurrentProjection, `stw r31` in both arms). A signed conversion: the arms reload
    // their unsigned magic double separately. Which expression it was is unknown. It must stay
    // in the first block so its constant is hoisted to the top.
    rate = (f32)(int)n;
    if (esp->m_Life_time + w->Num + esp->m_Tex_id == 99) { // COMPILER-DIFF: candidate (sched block split)
        t = z;
    }
    t = z;
    tw = 1.0f;
    if ((s8)esp->m_Parts_no >= -8 && (s8)esp->m_Parts_no <= -3) {
        pLog->err(0, 0, "ESP_16 : Parent is screen.");
        return;
    }
    PSMTXIdentity(esp->m_Mat);
    RotMatrix(esp->m_Mat, &esp->m_Ang);
    PSMTXConcat(pG->Camera.v_mat, esp->m_Mat, esp->m_Mat);
    PSMTXInverse(esp->m_Mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    p1 = NULL;
    cam = pG->Camera.param.pos;
    if (esp->m_Tool_flg & 4) {
        t = tw;
        tw = -1.0f / (f32)w->Num;
    } else {
        tw = tw / (f32)w->Num;
    }
    if (esp->m_Tool_flg & 2) {
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    GXBegin(0x98, 0, n * 2);
    p0 = Esp3f_GetVecPtr(w->pPosBuf, 0);
    for (i = 0; i < n; i++) {
        if (i != n - 1) {
            p1 = Esp3f_GetVecPtr(w->pPosBuf, i + 1);
            PSVECSubtract(p1, p0, &dir);
        }
        PSVECSubtract(p0, &cam, &toCam);
        PSVECCrossProduct(&dir, &toCam, &cross);
        if (cross.x == 0.0f && cross.y == 0.0f && cross.z == 0.0f) {
        } else {
#line 306 "D:/Bio4/Prog/esp16.cpp"
            VECNormalize(&cross, &nrm);
        }
        rate = (f32)i / (f32)(n - 1);
        half = (rate * esp->m_Size_base_y + (1.0f - rate) * esp->m_Size_base_x) * esp->m_Size_mul;
        PSVECScale(&nrm, &q0, half);
        PSVECScale(&nrm, &q1, -half);
        PSVECAdd(&q0, p0, &q0);
        PSVECAdd(&q1, p0, &q1);
        p0 = p1;
        GXPosition3f32(q0.x, q0.y, q0.z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s0, t);
        t += tw;
        GXPosition3f32(q1.x, q1.y, q1.z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s1, t);
    }
}

// Releases the position and speed esp3f buffers.
void cEsp16::Destruct()
{
    Esp16Work* w = &m_Free;
    cEsp* b;

    b = (cEsp*)w->pPosBuf;
    if (b != NULL && (b->m_Be_flg & 1)) {
        PushEsp(b);
    }
    b = (cEsp*)w->pSpdBuf;
    if (b != NULL && (b->m_Be_flg & 1)) {
        PushEsp(b);
    }
}

// Reads the point count, optional tail parts, physics parameters, allocates both buffers (fails
// when the pool is short) and starts every point at the effect's world position with zero speed.
int cEsp16::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp16Work* w = &m_Free;
    Vec p;
    Vec z;
    int i;

    w->Num = (u8)(pSeq->Work8[0] + 2);
    if (parent != pEffParentWorld && (m_Release_time == 0xFF || m_Release_time <= m_Life_time)) {
        s8 no = pSeq->Work8[1];

        if (no != 0) {
            if ((u32)(no - 1) >= m_pMod->nParts) {
                pLog->err(0, 0, "ESP16 : Wk1 PartsNo > %d ", m_pMod->nParts);
                return 0;
            }
            w->pParts = m_pMod->getPartsPtr(no - 1);
        }
    }
    w->grav = *(Vec*)&pSeq->Vec1.x;
    w->max_len = pSeq->Vec0.x;
    w->del = pSeq->Vec0.y * 0.01f;
    w->nen = pSeq->Vec0.z * 0.01f;
    w->rand_plus = *(Vec*)&pSeq->Vec2.x;
    if (!Esp3f_Alloc(sizeof(Vec), w->Num, &w->pPosBuf, &info) || !Esp3f_Alloc(sizeof(Vec), w->Num, &w->pSpdBuf, &info)) {
        pLog->warn(0, 0, "ESP_16 : Buf alloc failed.");
        return 0;
    }
    m_Radius = 100000000.0f;
    m_Flg |= 2;
    z.z = 0.0f;
    z.y = 0.0f;
    z.x = 0.0f;
    if (parent == pEffParentWorld) {
        p = m_Pos;
    } else {
        PSMTXMultVec(parent->mat, &m_Pos, &p);
    }
    for (i = w->Num - 1; i >= 0; i--) {
        *Esp3f_GetVecPtr(w->pPosBuf, i) = p;
        *Esp3f_GetVecPtr(w->pSpdBuf, i) = z;
    }
    return 1;
}
