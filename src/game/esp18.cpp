// game/esp18.cpp: effect id 0x18, the heat shimmer / radial blur sprite. Its trans copies the
// frame buffer (temp buffer 1, or 2 with Tool_flg 0x1000) at half resolution and redraws the
// sprite quad esp18_lp (8) times with the copy projected onto it, each layer a little more
// scaled (blur_rate = -Vec0.x) and offset on a circle, alpha 1 / (i + 2), with the sprite's own
// texture as an indirect-texture distortion map. Used for heat haze, explosions and the
// underwater / poison screen wobble.

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "main_sub.h"
#include "tpl.h"
#include "espgen.h"

extern f32 ZNEAR;
extern f32 ZFAR;

struct Esp18Work {
    Vec base_pos;    // 0x00 initial position
    f32 blur_rate;   // 0x0C -gen->xD8
};

// Heat shimmer: copies the frame buffer and redraws it through an indirect texture in
// esp18_lp layers.
class cEsp18 : public cEsp {
public:
    Esp18Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
cEsp* Esp18_Create();
void Esp18_Trans(cEsp18* esp);
}
int GetDrawTmpBufType();       // game/TmpBuf.cpp (C++ linkage)
extern GXTexObj g_Get_tex_obj;  // game/trans.cpp

// EspCreateTbl[0x18] factory.
cEsp* Esp18_Create()
{
    return new cEsp18;
}

// Standard sprite update; released when the animation ends.
void cEsp18::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Remembers the spawn position and takes the blur strength from -Vec0.x.
int cEsp18::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp18Work* w = &m_Free;

    w->base_pos = m_Pos;
    w->blur_rate = -gen->Vec0.x;
    return 1;
}

// Heat shimmer: the frame is copied into a texture and drawn back esp18_lp times through an
// indirect texture, each layer scaled and offset a little more than the previous one.
// Texture corner flip (see esp08.cpp: the combined test and the add-before-copy leaves keep
// the `zero + z` adds and the copies from `zero`'s register).
#define ESP18_FLIP_T(esp) \
    ((ESP_PARTS_SCREEN(esp) && !((esp)->m_Tool_flg & 4)) || (!ESP_PARTS_SCREEN(esp) && ((esp)->m_Tool_flg & 4)))
#define ESP_PARTS_SCREEN(esp) ((s8) (esp)->m_Parts_no >= -8 && (s8) (esp)->m_Parts_no <= -3)

// EspTransTbl[0x18]: sprite matrix (screen ortho / camera-facing / rotated with Tool_flg bit0),
// then per layer: copy the frame (first layer only), build the projective texture matrix with
// the layer's scale and circular offset, bind the sprite texture as the indirect map, and draw
// the quad with 2 TEV stages. Tool_flg 0x20000 doubles the indirect alpha. Restores the GX state.
void Esp18_Trans(cEsp18* esp)
{
    Esp18Work* w = &esp->m_Free;
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
    GXColor col;
    f32 sx;
    f32 sy;
    f32 ox;
    f32 oy;
    f32 x0;
    f32 y0;
    f32 z;
    f32 zero;
    f32 s0;
    f32 s1;
    f32 t0;
    f32 t1;
    f32 ang = (f32)(int)esp->m_Life_time;
    f32 ofs = 0.0f;
    f32 rx;
    f32 ry;
    f32 mul;
    f32 a;
    int copyOk;
    int stages;
    int texGens;
    void* buf;
    u32 tlutName;

    if (!esp->ChannelSet()) {
        return;
    }
    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    GXSetCullMode(0);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetZMode(1, 3, 0);
    CameraCurrentProjection();
    if ((s8) esp->m_Parts_no >= -8 && (s8) esp->m_Parts_no <= -3) {
        PSMTXIdentity(esp->m_Mat);
        low_RotMatrix(esp->m_Mat, &esp->m_Ang);
        TransMatrix(esp->m_Mat, &esp->m_Pos);
        C_MTXOrtho(proj, ofs, 448.0f, ofs, 512.0f, ofs, -100.0f);
        GXSetProjection(proj, 1);
    } else if (!(esp->m_Tool_flg & 1)) {
        Vec p;
        Mtx m;

        PSMTXIdentity(esp->m_Mat);
        PSMTXRotRad(esp->m_Mat, 'z', esp->m_Ang.z);
        PSMTXConcat(pG->Cam.v_mat, esp->parent->mat, m);
        PSMTXMultVec(m, &esp->m_Pos, &p);
        esp->m_Mat[0][3] = p.x;
        esp->m_Mat[1][3] = p.y;
        esp->m_Mat[2][3] = p.z;
    } else {
        Mtx m;

        PSMTXIdentity(esp->m_Mat);
        low_RotMatrix(esp->m_Mat, &esp->m_Ang);
        TransMatrix(esp->m_Mat, &esp->m_Pos);
        PSMTXConcat(pG->Cam.v_mat, esp->parent->mat, m);
        PSMTXConcat(m, esp->m_Mat, esp->m_Mat);
    }
    PSMTXInverse(esp->m_Mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    sx = esp->m_Size_base_x * esp->m_Size_mul;
    sy = esp->m_Size_base_y * esp->m_Size_mul;
    ox = -anm->Cx;
    oy = (f32) anm->Cy;
    z = 1.0f;
    zero = 0.0f;
    if (ox == zero) {
        ox = -anm->Width * 0.5f;
    }
    // `i` declared here (pseudo 237, not 116): gcse numbers its PRE pseudos in hash-bucket order
    // and hash(i + 1) = 13259 + regno(i) must land after hash(fp + 0xc0) = 13481 in the
    // 563-bucket table (regno 223..252), so the by-value fog copy's address gets spill slot
    // 0x234 and `i + 1` gets 0x238.
    u32 i;
    if (oy == zero) {
        oy = anm->Height * 0.5f;
    }
    x0 = ox * sx / anm->Width;
    y0 = oy * sy / anm->Height;
    if (esp->m_Tool_flg & 2) {
        if (ESP18_FLIP_T(esp)) {
            s0 = zero + z;
            s1 = zero;
            t0 = s0;
            t1 = s1;
        } else {
            s0 = zero + z;
            t0 = zero;
            s1 = zero;
            t1 = s0;
        }
    } else {
        if (ESP18_FLIP_T(esp)) {
            s0 = zero;
            s1 = s0 + z;
            t1 = s0;
            t0 = s1;
        } else {
            s0 = zero;
            s1 = s0 + z;
            t0 = s0;
            t1 = s1;
        }
    }
    // `ang = 0.0f` sits above the esp18_div guard test (its pool load is in the block before it).
    // In the loop, texGens/stages are counters: `= 0` at the body top (the fog zero canonicalises
    // to texGens' register), `++` per texgen/tev stage set up; the `= 2` value comes from gcse's
    // constant propagation, so its later uses stay `mr r3,r25` / `addi r25,r25,1`, and the
    // stack `1` of GXInitTexObjCI is not cse'd to `stages` (stages is `stages++`, unknown).
    static u32 esp18_lp = 8;
    ang = 0.0f;
    static f32 esp18_div = 1.0f / (f32) esp18_lp;
    static f32 prm2 = 5.0f;
    static f32 prm3 = 5.0f;
    static f32 esp18_mul_rate = 0.1f;
    static f32 e18mx = 1.0f;
    static f32 e18my = 1.0f;
    for (i = 0; i < esp18_lp; i++) {
        GXTexObj tex;
        GXTlutObj tlut;

        copyOk = 1;
        texGens = 0;
        stages = 0;
        fog.r = fog.g = fog.b = fog.a = 0;
        GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
        // One codeless insn in the loop: the hoisted 1.0/0.5 (REG_EQUIV, live length doubled)
        // then tie at priority 171 (270000/1570 vs /1574) and the older 0.5 is coloured first
        // (f18), 1.0 after it (f17); with 819 loop insns ours had 172 vs 171 the other way.
        asm("" : "=m"(inv[0][0]));  // COMPILER-DIFF: candidate (loop.c insn_count)
        if (esp->m_Tool_flg & 0x1000) {
            if (GetDrawTmpBufType() == 2) {
                copyOk = 0;
            }
            buf = GetDrawTmpBufAddr(2);
        } else {
            buf = GetDrawTmpBufAddr(1);
        }
        // A loop-local `ofs`: the function-level one (0.0f, the C_MTXOrtho zero) is copied into f1
        // there, and that f1 preference would otherwise follow `ofs` into the height conversion's
        // fmr temp (expand_preferences merges along the dying operands: 109 -> 526 -> 528).
        f32 ofs = 56.0f;
        if (StaFlagChk(pG, STA_TEX_RENDER)) {
            ofs = 0.0f;
        }
        if (copyOk && i == 0) {
            if (buf == NULL) {
                pLog->warn(0, 0, "Esp0d() : not enough memory");
                return;
            }
            GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
            GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
            GXCopyTex(buf, 0);
            GXPixModeSync();
            GXInvalidateTexAll();
        }
        GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
        GXLoadTexObj(&tex, 1);
        g_Get_tex_obj = tex;
        Mtx tm;
        Mtx pm;
        rx = prm2 * sinf(ang) * esp->m_Col_a * 0.01f;
        ry = prm3 * cosf(ang) * esp->m_Col_a * 0.01f;
        mul = esp18_mul_rate * 0.5f * esp18_div * (f32) i * w->blur_rate * esp->m_Col_a * (1.0f / 255.0f) + 1.0f;
        Mtx m1 = {
            {1.0f, 0.0f, -0.5f, 0.0f},
            {0.0f, 1.0f, -0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        };
        Mtx m2 = {{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
        m2[0][0] = mul;
        m2[1][1] = mul;
        m2[2][2] = 1.0f;
        Mtx m3 = {
            {1.0f, 0.0f, 0.5f, 0.0f},
            {0.0f, 1.0f, 0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
        };

        if ((s8) esp->m_Parts_no >= -8 && (s8) esp->m_Parts_no <= -3) {
            Mtx m4 = {{0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
            m4[0][0] = 1.0f / 512.0f;
            m4[1][1] = 1.0f / 448.0f;
            m4[2][2] = 1.0f;
            Mtx m5 = {
                {0.001953125f, 0.0f, 0.0f, 0.0f},
                {0.0f, 0.0029762f, -0.167f, 0.0f},
                {0.0f, 0.0f, 1.0f, 0.0f},
            };

            if (StaFlagChk(pG, STA_TEX_RENDER)) {
                PSMTXConcat(m4, esp->m_Mat, tm);
            } else {
                PSMTXConcat(m5, esp->m_Mat, tm);
            }
            if (w->blur_rate != 0.0f) {
                PSMTXConcat(m1, tm, tm);
                PSMTXConcat(m2, tm, tm);
                PSMTXConcat(m3, tm, tm);
            }
            GXLoadTexMtxImm(tm, 0x1E, 1);
            GXSetTexCoordGen(texGens, 1, 0, 0x1E);
            texGens++;
        } else {
            C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.6666667f, rx * (1.0f / 512.0f) * e18mx + 0.5f,
                                  ry / 392.0f * e18my + 0.5f);
            PSMTXConcat(pm, esp->m_Mat, tm);
            GXLoadTexMtxImm(tm, 0x1E, 0);
            GXSetTexCoordGen(texGens, 0, 0, 0x1E);
            texGens++;
        }
        GXSetNumIndStages(1);
        GXSetTexCoordGen(texGens, 1, 4, 0x3C);
        texGens++;
        GXSetIndTexOrder(0, 1, 0);
        GXSetIndTexCoordScale(0, 0, 0);
        a = esp->m_Col_a;
        if (a > 16.0f) {
            a = 255.0f;
        } else {
            a -= 4.0f;
            if (a < 0.0f) {
                a = 0.0f;
            } else {
                a *= 21.25f;
            }
        }
        f32 inv = 1.0f / (f32) (i + 2);
        col.r = (u8) esp->m_Col_r;
        col.g = (u8) esp->m_Col_g;
        col.b = (u8) esp->m_Col_b;
        col.a = (u8) (a * inv);
        GXSetChanMatColor(4, col);
        GXSetTevOrder(0, 0, 1, 4);
        GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(0, 7, 7, 7, 5);
        GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
        stages++;
        {
            int no = esp->m_Tex_id;
            EspTexWk* tw = EspGetTexWk(no, 1);
            if (tw->Owner == 0xD2) {
                pLog->err(0, 0, "ESP : TexId[%x] no data", no);
            } else {
                GXTexObj tex2;
                GXTexObj* pTex = &tex2;
                GXTlutObj* pTlut = &tlut;
                TEXDescriptor* td = TEXGet(tw->pTpl, esp->m_Ptn_no);
                TEXHeader* th = td->textureHeader;

                if (th->format == 8 || th->format == 9) {
                    // A function-level variable heads loop.c's hoisted `1` group (stack arg + the four
                    // GXNormal3s8 bytes): it ties with the hoisted 0x4330 magic at priority 211 and the
                    // older pseudo wins r14 (the magic is then rematerialised `lis r0,0x4330` per use).
                    tlutName = 1;
                    GXInitTexObjCI(pTex, th->data, th->width, th->height, th->format, 0, 0, 0, tlutName);
                    GXInitTlutObj(pTlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                    GXLoadTlut(pTlut, tlutName);
                } else {
                    GXInitTexObj(pTex, th->data, th->width, th->height, th->format, 0, 0, 0);
                }
                GXLoadTexObj(pTex, 2);
                GXLoadTexMtxImm(tw->mtx, 0x21, 1);
                GXSetTexCoordGen(texGens, 1, 4, 0x21);
                GXSetTevOrder(1, texGens, 2, 4);
                GXSetTevColorIn(1, 0xF, 0xF, 0xF, 0);
                GXSetTevColorOp(1, 0, 0, 0, 1, 0);
                GXSetTevAlphaIn(1, 7, 4, 5, 7);
                if (esp->m_Tool_flg & 0x20000) {
                    GXSetTevAlphaOp(1, 0, 0, 2, 1, 0);
                } else {
                    GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
                }
                stages = 2;
                texGens++;
            }
        }
        GXSetNumTevStages(stages);
        GXSetNumTexGens(texGens);
        GXBegin(0x80, 0, 4);
        GXPosition3f32(x0 + rx, y0 + ry, z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s0, t0);
        GXPosition3f32(x0 + sx + rx, y0 + ry, z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s1, t0);
        GXPosition3f32(x0 + sx + rx, y0 - sy + ry, z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s1, t1);
        GXPosition3f32(x0 + rx, y0 - sy + ry, z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s0, t1);
        GXSetNumTevStages(1);
        GXSetNumTexGens(0);
        GXSetNumIndStages(0);
        GXSetTevDirect(0);
        GXSetTevDirect(1);
        LightMgr.setFog();
        ang += 6.2831855f / (f32) esp18_lp;
    }
}
