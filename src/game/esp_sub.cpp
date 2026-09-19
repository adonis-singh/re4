// game/esp_sub: the shared part of the effect sprites (D:/Bio4/Prog/esp_sub.cpp): the cEsp base
// class members (CommonMove life/motion/colour update, AnmMove texture animation, ChannelSet
// colour + distance fade, ApplyMatrix), the common sprite draw EspCommonTrans with its heat
// shimmer and frame-buffer ("nega") variants, and EspSeqSet, which turns one EspGenWork record
// of an effect sequence into a live esp (or an effect model through EfmSeqSet). EspEstSetSelect
// spawns one record of an est table directly (laser sight, gatling, ...).
#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "espgen.h"
#include "obj.h"
#include "scroll.h"
#include "rnd.h"
#include "main_sub.h"
#include "db_log.h"
#include "tpl.h"
#include "trans_ot.h"

// game/trans_lit.cpp
extern "C" void commonEspLightSet(cLight** list, int n);

int GetDrawTmpBufType();        // game/TmpBuf.cpp (C++ linkage)
extern GXTexObj g_Get_tex_obj;  // game/trans.cpp

extern "C" {
static void EspCommonTransShimmer(cEsp* esp, int type, u32 blur);
void EspCommonTransNega(cEsp* esp, u32 type);
f32 EspGetCameraPan();   // game/esp.cpp
int EspEstSetSelect(int owner, int id, int no, cEsp** out, int bNoSuspend);
void GetPosXY(Vec* p0, Vec* p1, Vec* p2, Vec* p3, f32 u, f32 v, Vec* out);
void Esp1b_SpTrans(cEsp* esp);
}

extern f32 ZNEAR;
extern f32 ZFAR;

#define DEG2RAD (3.14f / 180.0f)

// The pulled effect is kept in a one-member struct: the original reloads the pointer from its
// stack slot after every store through it (a struct-member slot aliases the member stores).
struct EspPtr {
    cEsp* p;
};

#define ESP_PARTS_SCREEN(esp) ((s8) (esp)->m_Parts_no >= -8 && (s8) (esp)->m_Parts_no <= -3)

// Texture coordinate corners for the sprite orientation (flags bit1: flip s, bit2: flip t;
// screen sprites are drawn upside down). One combined condition and corners built from a `zero`
// variable: each leaf is a jump target where cse knows neither operand of `zero + z`, which
// keeps the adds (nested ifs with literals fold 0 + z). The flip-s leaves add first, copy after.
#define ESP_FLIP_T(esp)                                                                           \
    ((ESP_PARTS_SCREEN(esp) && !((esp)->m_Tool_flg & 4)) || (!ESP_PARTS_SCREEN(esp) && ((esp)->m_Tool_flg & 4)))
#define ESP_TEXCOORD_SET()                                                                        \
    if (esp->m_Tool_flg & 2) {                                                                         \
        if (ESP_FLIP_T(esp)) {                                                                    \
            s0 = zero + z;                                                                        \
            s1 = zero;                                                                            \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        } else {                                                                                  \
            s0 = zero + z;                                                                        \
            t0 = zero;                                                                            \
            s1 = zero;                                                                            \
            t1 = s0;                                                                              \
        }                                                                                         \
    } else {                                                                                      \
        if (ESP_FLIP_T(esp)) {                                                                    \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t1 = s0;                                                                              \
            t0 = s1;                                                                              \
        } else {                                                                                  \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        }                                                                                         \
    }

// Default Trans (draw) entry of a sprite effect: dispatches to the shimmer / nega variants
// (m_Shimmer_type, Tool_flg 0x2000), sets the blend mode, and when the previous OT entry was not a
// sprite selects the projection (screen ortho 512x448 for parts 0xF8..0xFD, else the camera) and
// the texture; then builds the sprite matrix (billboard / axis-aligned per Tool_flg, camera pan
// for screen sprites), the colour through ChannelSet and draws the quad (mask texture stage when
// Tool_flg 0x4000). Tool_flg 0x100000 forces the alpha compare, 0x808000 disables alpha update.
// Shared sprite draw: the sprite quad (g_EspCommonDisplayList) with the effect's texture, an
// optional mask texture in TEV stage 1, screen-space or camera-relative placement.
void EspCommonTrans(cEsp* esp)
{
    static int s_proj_type;
    static int s_tex_no;
    static EspAnmData* s_pAnm;
    static int s_ptn_no;
    Mtx inv;
    f32 sx;
    f32 sy;
    f32 ox;
    f32 oy;

    if (esp->m_Shimmer_type != 0) {
        EspCommonTransShimmer(esp, esp->m_Shimmer_pow, esp->m_Shimmer_type);
        return;
    }
    if (esp->m_Tool_flg & 0x2000) {
        EspCommonTransNega(esp, 2);
        return;
    }
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    if (OtGetPrevKind() != 8) {
        if (ESP_PARTS_SCREEN(esp)) {
            Mtx44 proj;
            s_proj_type = 0;
            C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
            GXSetProjection(proj, 1);
        } else {
            s_proj_type = 1;
            CameraCurrentProjection();
        }
        if (!EspGetAnmAddr(esp->m_Tex_id, &s_pAnm)) {
            pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
            s_tex_no = -1;
            return;
        }
        EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
        s_tex_no = esp->m_Tex_id;
        s_ptn_no = esp->m_Ptn_no;
        esp->CommonStateSet();
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxDesc(0xA, 1);
        GXSetVtxDesc(0xD, 1);
        GXSetVtxAttrFmt(0, 9, 1, 1, 0);
        GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
        GXSetVtxAttrFmt(0, 0xD, 1, 1, 0);
    } else {
        if (ESP_PARTS_SCREEN(esp)) {
            if (s_proj_type != 0) {
                Mtx44 proj;
                s_proj_type = 0;
                C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
                GXSetProjection(proj, 1);
            }
        } else {
            if (s_proj_type != 1) {
                s_proj_type = 1;
                CameraCurrentProjection();
            }
        }
        if (s_tex_no != esp->m_Tex_id) {
            if (!EspGetAnmAddr(esp->m_Tex_id, &s_pAnm)) {
                pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
                s_tex_no = -1;
                return;
            }
        }
        if (s_tex_no != esp->m_Tex_id || s_ptn_no != esp->m_Ptn_no) {
            EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
            s_ptn_no = esp->m_Ptn_no;
        }
        s_tex_no = esp->m_Tex_id;
    }
    if (!esp->ChannelSet()) {
        return;
    }
    sx = esp->m_Size_base_x * esp->m_Size_mul;
    sy = esp->m_Size_base_y * esp->m_Size_mul;
    if ((f32) s_pAnm->Cx == 0.0f) {
        ox = -0.5f;
    } else {
        ox = (f32) -s_pAnm->Cx / s_pAnm->Width;
    }
    if ((f32) s_pAnm->Cy == 0.0f) {
        oy = -0.5f;
    } else {
        oy = (f32) s_pAnm->Cy / s_pAnm->Height + -1.0f;
    }
    if (ESP_PARTS_SCREEN(esp)) {
        Mtx m;

        esp->m_Mat[2][2] = 1.0f;
        esp->m_Mat[0][0] = sx;
        esp->m_Mat[0][1] = 0.0f;
        esp->m_Mat[0][2] = 0.0f;
        esp->m_Mat[0][3] = ox * sx;
        esp->m_Mat[1][0] = 0.0f;
        esp->m_Mat[1][1] = sy;
        esp->m_Mat[1][2] = 0.0f;
        esp->m_Mat[1][3] = oy * sy;
        esp->m_Mat[2][0] = 0.0f;
        esp->m_Mat[2][1] = 0.0f;
        esp->m_Mat[2][3] = 0.0f;
        if (esp->m_Tool_flg & 2) {
            esp->m_Mat[0][0] = -sx;
            esp->m_Mat[0][3] = -(ox * sx);
            if (!(esp->m_Tool_flg & 4)) {
                esp->m_Mat[1][1] = -sy;
                esp->m_Mat[1][3] = -(oy * sy);
            }
        } else if (!(esp->m_Tool_flg & 4)) {
            esp->m_Mat[1][1] = -sy;
            esp->m_Mat[1][3] = -(oy * sy);
        }
        low_RotMatrix(m, &esp->m_Ang);
        PSMTXConcat(m, esp->m_Mat, esp->m_Mat);
        esp->m_Mat[0][3] += esp->m_Pos.x;
        esp->m_Mat[1][3] += esp->m_Pos.y;
        esp->m_Mat[2][3] += esp->m_Pos.z;
    } else if (!(esp->m_Tool_flg & 0x80001)) {
        Mtx m;
        Vec p;
        Mtx m2;

        esp->m_Mat[2][2] = 1.0f;
        esp->m_Mat[0][0] = sx;
        esp->m_Mat[0][1] = 0.0f;
        esp->m_Mat[0][2] = 0.0f;
        esp->m_Mat[0][3] = ox * sx;
        esp->m_Mat[1][0] = 0.0f;
        esp->m_Mat[1][1] = sy;
        esp->m_Mat[1][2] = 0.0f;
        esp->m_Mat[1][3] = oy * sy;
        esp->m_Mat[2][0] = 0.0f;
        esp->m_Mat[2][1] = 0.0f;
        esp->m_Mat[2][3] = 0.0f;
        if (esp->m_Tool_flg & 2) {
            esp->m_Mat[0][0] = -sx;
            esp->m_Mat[0][3] = -(ox * sx);
            if (esp->m_Tool_flg & 4) {
                esp->m_Mat[1][1] = -sy;
                esp->m_Mat[1][3] = -(oy * sy);
            }
        } else if (esp->m_Tool_flg & 4) {
            esp->m_Mat[1][1] = -sy;
            esp->m_Mat[1][3] = -(oy * sy);
        }
        PSMTXRotRad(m, 'z', esp->m_Ang.z);
        PSMTXConcat(m, esp->m_Mat, esp->m_Mat);
        PSMTXConcat(pG->Cam.v_mat, esp->parent->mat, m2);
        PSMTXMultVec(m2, &esp->m_Pos, &p);
        esp->m_Mat[0][3] += p.x;
        esp->m_Mat[1][3] += p.y;
        esp->m_Mat[2][3] += p.z;
    } else {
        Mtx m;

        esp->m_Mat[0][0] = sx;
        esp->m_Mat[0][1] = 0.0f;
        esp->m_Mat[2][2] = 1.0f;
        esp->m_Mat[0][2] = 0.0f;
        esp->m_Mat[0][3] = ox * sx;
        esp->m_Mat[1][0] = 0.0f;
        esp->m_Mat[1][1] = sy;
        esp->m_Mat[1][2] = 0.0f;
        esp->m_Mat[1][3] = oy * sy;
        esp->m_Mat[2][0] = 0.0f;
        esp->m_Mat[2][1] = 0.0f;
        esp->m_Mat[2][3] = 0.0f;
        if (esp->m_Tool_flg & 2) {
            esp->m_Mat[0][0] = -sx;
            esp->m_Mat[0][3] = -(ox * sx);
            if (esp->m_Tool_flg & 4) {
                esp->m_Mat[1][1] = -sy;
                esp->m_Mat[1][3] = -(oy * sy);
            }
        } else if (esp->m_Tool_flg & 4) {
            esp->m_Mat[1][1] = -sy;
            esp->m_Mat[1][3] = -(oy * sy);
        }
        low_RotMatrix(m, &esp->m_Ang);
        PSMTXConcat(m, esp->m_Mat, esp->m_Mat);
        if (esp->m_Tool_flg & 0x80000) {
            Mtx m3;
            PSMTXRotRad(m3, 'y', EspGetCameraPan() * (3.1415927f / 180.0f));
            PSMTXConcat(m3, esp->m_Mat, esp->m_Mat);
        }
        esp->m_Mat[0][3] += esp->m_Pos.x;
        esp->m_Mat[1][3] += esp->m_Pos.y;
        esp->m_Mat[2][3] += esp->m_Pos.z;
        PSMTXConcat(esp->parent->mat, esp->m_Mat, esp->m_Mat);
        PSMTXConcat(pG->Cam.v_mat, esp->m_Mat, esp->m_Mat);
    }
    PSMTXInverse(esp->m_Mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    if (esp->m_Tool_flg & 0x4000) {
        int no = esp->m_MaskTex_id;
        EspTexWk* tw = EspGetTexWk(no, 1);
        if (tw->Owner == 0xD2) {
            pLog->err(0, 0, "ESP : Mask_TexId[%x] no data", no);
        } else {
            GXTexObj tex;
            GXTlutObj tlut;
            GXTexObj* pTex = &tex;
            GXTlutObj* pTlut = &tlut;
            TEXDescriptor* td = TEXGet(tw->pTpl, esp->m_MaskPtn_no);
            TEXHeader* th = td->textureHeader;

            if (th->format == 8 || th->format == 9) {
                GXInitTexObjCI(pTex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);
                GXInitTlutObj(pTlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                GXLoadTlut(pTlut, 1);
            } else {
                GXInitTexObj(pTex, th->data, th->width, th->height, th->format, 0, 0, 0);
            }
            GXLoadTexObj(pTex, 1);
            GXLoadTexMtxImm(tw->mtx, 0x21, 1);
            GXSetTexCoordGen(1, 1, 4, 0x21);
            GXSetNumTevStages(2);
            GXSetNumTexGens(2);
            GXSetTevOrder(1, 1, 1, 4);
            GXSetTevColorIn(1, 0xF, 0xF, 0xF, 0);
            GXSetTevColorOp(1, 0, 0, 0, 1, 0);
            GXSetTevAlphaIn(1, 7, 4, 5, 7);
            if (esp->m_Tool_flg & 0x20000) {
                GXSetTevAlphaOp(1, 0, 0, 2, 1, 0);
            } else {
                GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
            }
        }
    }
    {
        // The flag word is read into a local for the first test only: with two plain reads the
        // pre-cse jump threading merges the compares; the original kept one compare in cr7.
        u32 sysFlags = pG->Status_flg[1];
        if ((!(sysFlags & 0x80) && (esp->m_Tool_flg & 0x8000)) || (StaFlagChk(pG, STA_ALPHA_DRAW2) && (esp->m_Tool_flg & 0x800000))) {
            GXSetAlphaUpdate(1);
        }
    }
    if (esp->m_Tool_flg & 0x200000) {
        GXSetZMode(1, 3, 1);
        GXSetDstAlpha(1, 0);
    }
    if (esp->m_Tool_flg & 0x100000) {
        GXSetAlphaCompare(4, 0x80, 1, 4, 0x80);
    }
    if (esp->m_Flg & 0x10) {
        Esp1b_SpTrans(esp);
    } else {
        GXCallDisplayList(g_EspCommonDisplayList, 0x60);
    }
    if (esp->m_Tool_flg & 0x4000) {
        GXSetNumTevStages(1);
        GXSetNumTexGens(1);
    }
    if (esp->m_Tool_flg & 0x200000) {
        GXSetZMode(1, 3, 0);
        GXSetDstAlpha(0, 0);
    }
    if (esp->m_Tool_flg & 0x100000) {
        GXSetAlphaCompare(4, 1, 1, 4, 1);
    }
    if (esp->m_Tool_flg & 0x808000) {
        GXSetAlphaUpdate(0);
    }
}

// Heat shimmer sprite: the frame is copied into a texture and drawn back through an indirect
// texture (blur 1..3 select the warp mode; type scales the distortion).
static void EspCommonTransShimmer(cEsp* esp, int type, u32 blur)
{
    static Mtx Matrix1 = {
        {0.001953125f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f / 448.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    static Mtx Matrix2 = {
        {0.001953125f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0029762f, -0.167f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
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
    f32 ofs;
    f32 scale;
    u8 stages;
    int texGens;
    int copyOk;
    void* buf;

    scale = (f32) type * (1.0f / 32.0f) + 1.0f;
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
    if (ESP_PARTS_SCREEN(esp)) {
        PSMTXIdentity(esp->m_Mat);
        low_RotMatrix(esp->m_Mat, &esp->m_Ang);
        TransMatrix(esp->m_Mat, &esp->m_Pos);
        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
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
    if (oy == zero) {
        oy = anm->Height * 0.5f;
    }
    x0 = ox * sx / anm->Width;
    y0 = oy * sy / anm->Height;
    ESP_TEXCOORD_SET()
    GXTexObj tex;
    f32 indMtx[2][3];
    f32 dot;
    texGens = 0;
    copyOk = 1;
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    if (esp->m_Tool_flg & 0x1000) {
        if (GetDrawTmpBufType() == 2) {
            copyOk = 0;
        }
        buf = GetDrawTmpBufAddr(2);
    } else {
        buf = GetDrawTmpBufAddr(1);
    }
    ofs = 56.0f;
    if (StaFlagChk(pG, STA_TEX_RENDER)) {
        ofs = 0.0f;
    }
    if (copyOk) {
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
    if (ESP_PARTS_SCREEN(esp)) {
        if (StaFlagChk(pG, STA_TEX_RENDER)) {
            PSMTXConcat(Matrix1, esp->m_Mat, tm);
        } else {
            PSMTXConcat(Matrix2, esp->m_Mat, tm);
        }
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen(texGens, 1, 0, 0x1E);
    } else {
        C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
        PSMTXConcat(pm, esp->m_Mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 0);
        GXSetTexCoordGen(texGens, 0, 0, 0x1E);
    }
    texGens++;
    GXSetNumIndStages(1);
    GXSetTexCoordGen(texGens, 1, 4, 0x3C);
    texGens++;
    GXSetIndTexOrder(0, 1, 0);
    GXSetIndTexCoordScale(0, 0, 0);
    if (ESP_PARTS_SCREEN(esp)) {
        dot = 2500.0f;
    } else {
        Vec dir;
        Vec d;
        Vec p;
        Camera* cam;

        if (esp->parent != pEffParentWorld) {
            PSMTXMultVec(esp->parent->mat, &esp->m_Pos, &p);
        } else {
            p = esp->m_Pos;
        }
        cam = &pG->Cam;
        dir.x = cam->param.at.x - cam->param.pos.x;
        dir.y = cam->param.at.y - cam->param.pos.y;
        dir.z = cam->param.at.z - cam->param.pos.z;
#line 865 "D:/Bio4/Prog/esp_sub.cpp"
        VECNormalize(&dir, &dir);
        d.x = p.x - cam->param.pos.x;
        d.y = p.y - cam->param.pos.y;
        d.z = p.z - cam->param.pos.z;
        dot = PSVECDotProduct(&dir, &d);
    }
    if (dot < 1500.0f) {
        dot = 1500.0f;
    }
    if (blur != 3) {
        indMtx[1][1] = indMtx[0][0] = esp->m_Col_a * (1.0f / 255.0f) * 0.04f * 1000.0f / dot * scale;
        indMtx[0][1] = 0.0f;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = 0.0f;
        indMtx[1][2] = 0.0f;
    } else {
        static f32 prm1 = 0.5f;
        static f32 prm2 = 0.0f;
        static f32 prm3 = 0.0f;
        static f32 prm4 = Screen.height * 0.5f / Screen.width;

        indMtx[0][0] = prm1;
        indMtx[0][1] = prm2;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = prm3;
        indMtx[1][1] = prm4;
        indMtx[1][2] = 0.0f;
    }
    GXSetIndTexMtx(1, indMtx, 1);
    {
        u8 signedOfs;
        u8 replace;

        switch (blur) {
        case 1:
            signedOfs = 0;
            replace = 0;
            break;
        case 2:
            signedOfs = 1;
            replace = 0;
            break;
        case 3:
            signedOfs = 0;
            replace = 1;
            break;
        default:
            pLog->err(0, 0, "ESP_SHIMMER : BLUR_TYPE[%x] invalid", blur);
            signedOfs = 0;
            replace = 1;
            break;
        }
        stages = 1;
        GXSetTevIndWarp(0, 0, signedOfs, replace, 1);
    }
    GXSetTevOrder(0, 0, 1, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    if (esp->m_Tool_flg & 0x4000) {
        int no = esp->m_MaskTex_id;
        EspTexWk* tw = EspGetTexWk(no, 1);
        if (tw->Owner == 0xD2) {
            pLog->err(0, 0, "ESP : Mask_TexId[%x] no data", no);
        } else {
            GXTexObj tex2;
            GXTexObj* pTex = &tex2;
            GXTlutObj* pTlut = (GXTlutObj*) indMtx;
            TEXDescriptor* td = TEXGet(tw->pTpl, esp->m_MaskPtn_no);
            TEXHeader* th = td->textureHeader;

            if (th->format == 8 || th->format == 9) {
                GXInitTexObjCI(pTex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);
                GXInitTlutObj(pTlut, td->CLUTHeader->data, td->CLUTHeader->format, td->CLUTHeader->numEntries);
                GXLoadTlut(pTlut, 1);
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
    GXPosition3f32(x0, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x0 + sx, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x0 + sx, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t1);
    GXSetNumTevStages(1);
    GXSetNumTexGens(0);
    GXSetNumIndStages(0);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    LightMgr.setFog();
}

// Frame-buffer sprite: the frame is copied into a texture and drawn on the sprite quad,
// modulated by the sprite's own texture (type: TEV colour scale 0..2).
void EspCommonTransNega(cEsp* esp, u32 type)
{
    static Mtx Matrix = {
        {0.001953125f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0029762f, -0.167f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
    void* buf;
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

    if (!esp->ChannelSet()) {
        return;
    }
    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    CameraCurrentProjection();
    if (ESP_PARTS_SCREEN(esp)) {
        PSMTXIdentity(esp->m_Mat);
        low_RotMatrix(esp->m_Mat, &esp->m_Ang);
        TransMatrix(esp->m_Mat, &esp->m_Pos);
        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
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
    GXTexObj tex;
    PSMTXInverse(esp->m_Mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
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
    if (oy == zero) {
        oy = anm->Height * 0.5f;
    }
    x0 = ox * sx / anm->Width;
    y0 = oy * sy / anm->Height;
    ESP_TEXCOORD_SET()
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    buf = GetDrawTmpBufAddr(3);
    if (buf == NULL) {
        pLog->warn(0, 0, "Esp0d() : not enough memory");
        return;
    }
    f32 ofs = 56.0f;
    GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
    GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
    GXCopyTex(buf, 0);
    GXPixModeSync();
    GXInvalidateTexAll();
    GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
    GXLoadTexObj(&tex, 1);
    if (ESP_PARTS_SCREEN(esp)) {
        Mtx tm;

        PSMTXConcat(Matrix, esp->m_Mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen(0, 1, 0, 0x1E);
    } else {
        Mtx tm;
        Mtx pm;

        C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
        PSMTXConcat(pm, esp->m_Mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 0);
        GXSetTexCoordGen(0, 0, 0, 0x1E);
    }
    GXSetTevOrder(0, 0, 1, 4);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 8);
    switch (type) {
    case 0:
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        break;
    case 1:
        GXSetTevColorOp(0, 0, 0, 1, 1, 0);
        break;
    case 2:
        GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        break;
    }
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetTevOrder(1, 0xFF, 0xFF, 4);
    GXSetTevColorIn(1, 0xA, 0xF, 0, 0xF);
    GXSetTevColorOp(1, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(1, 7, 7, 7, 5);
    GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
    GXSetNumTexGens(2);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetNumTevStages(2);
    GXSetTevOrder(2, 1, 0, 4);
    GXSetTevColorIn(2, 0xF, 0xF, 0xF, 0);
    GXSetTevColorOp(2, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(2, 7, 0, 4, 7);
    GXSetTevAlphaOp(2, 0, 0, 0, 1, 0);
    GXSetNumTevStages(3);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x0, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x0 + sx, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x0 + sx, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t1);
    GXSetNumTevStages(1);
    GXSetNumTexGens(0);
    GXSetNumIndStages(0);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    LightMgr.setFog();
}

// Base move: only reached for an id without its own class; logs the id.
void cEsp::move()
{
    pLog->err(0, 0, "ESP : ESP_ID[%x] move() invalid", m_Id);
}

// Per-frame update shared by every effect: detaches from the parent parts after m_Release_time
// frames (baking the parent matrix into pos/speed), integrates speed (+Speed_plus, *D_speed) once
// m_Pos_start_cnt has passed, scale (m_Size_mul += m_Size_plus, *D_size_plus; dies at <= 0) once
// m_Size_start_cnt has passed, angle, colour (ColorUpdate), and kills the effect (PushEsp) when
// m_Life_time reaches m_Life_max. Returns 0 when the effect died this frame. Updates m_Radius.
int cEsp::CommonMove()
{
    if (parent != pEffParentWorld && m_Release_time != 0xFF && m_Release_time <= m_Life_time) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (m_Pos_start_cnt == 0 || m_Pos_start_cnt <= m_Life_time) {
        PSVECAdd(&m_Pos, &m_Speed, &m_Pos);
        PSVECAdd(&m_Speed, &m_Speed_plus, &m_Speed);
        PSVECScale(&m_Speed, &m_Speed, m_D_speed);
    }
    if (m_Size_start_cnt == 0 || m_Size_start_cnt <= m_Life_time) {
        m_Size_mul += m_Size_plus;
        m_Size_plus *= m_D_size_plus;
        if (m_Size_mul <= 0.0f) {
            PushEsp(this);
            return 0;
        }
    }
    PSVECAdd(&m_Ang, &m_Ang_plus, &m_Ang);
    if (!ColorUpdate()) {
        return 0;
    }
    if (m_Life_max != 0 && m_Life_max <= m_Life_time) {
        PushEsp(this);
        return 0;
    }
    m_Life_time++;
    m_Radius = SQRTF(m_Size_base_x * m_Size_base_x + m_Size_base_y * m_Size_base_y) * m_Size_mul;
    return 1;
}

// Colour envelope: fades in over the first m_Col_max_cnt frames (alpha, and rgb for Blend_type 3),
// holds for m_Col_start_cnt frames, then multiplies rgba by m_Col_d_* every frame (clamped to 255)
// and kills the effect when alpha drops below 4. Returns 0 when it died.
int cEsp::ColorUpdate()
{
    if (m_Col_max_cnt < m_Life_time) {
        if (m_Col_max_cnt + m_Col_start_cnt <= m_Life_time) {
            m_Col_r *= m_Col_d_r;
            m_Col_g *= m_Col_d_g;
            m_Col_b *= m_Col_d_b;
            m_Col_a *= m_Col_d_a;
            if (m_Col_r > 255.0f) {
                m_Col_r = 255.0f;
            }
            if (m_Col_g > 255.0f) {
                m_Col_g = 255.0f;
            }
            if (m_Col_b > 255.0f) {
                m_Col_b = 255.0f;
            }
            if (m_Col_a > 255.0f) {
                m_Col_a = 255.0f;
            }
            if (m_Col_a < 4.0f) {
                PushEsp(this);
                return 0;
            }
        }
    } else if (m_Col_max_cnt != 0) {
        f32 rate = (f32) m_Life_time / (f32) m_Col_max_cnt;
        if (m_Blend_type == 3) {
            m_Col_r = (f32) m_Col_start_r * rate;
            m_Col_g = (f32) m_Col_start_g * rate;
            m_Col_b = (f32) m_Col_start_b * rate;
        }
        m_Col_a = (f32) m_Col_start_a * rate;
    }
    return 1;
}

// Base per-id parameter set-up: nothing to read (returns 1); ids with their own work override it.
int cEsp::SetFreeWork(EspGenWork* gen, u32* seed)
{
    return 1;
}

// Advances the texture animation: m_Anm_cnt accumulates m_Anm_rate (1/32 frame units) against the
// current pattern's frame count and steps m_Ptn_no; at the end Loop 0 returns 0 (animation over,
// callers stop drawing), 1 restarts, 2 holds the last pattern. Same for the mask animation
// (m_MaskTex_id / m_MaskPtn_no) when Tool_flg 0x4000.
int cEsp::AnmMove()
{
    EspAnmData* anm;
    u32 time;

    if (!EspGetAnmAddr(m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", m_Tex_id);
        return 0;
    }
    if (anm->Data_num == 0) {
        time = 1;
    } else {
        time = anm->Frame_cnt[anm->Frames + m_Ptn_no];
    }
    m_Anm_cnt += m_Anm_rate;
    while ((m_Anm_cnt >> 5) > (u16) time) {
        m_Ptn_no++;
        m_Anm_cnt -= time << 5;
        if (m_Ptn_no >= anm->Frames) {
            switch (anm->Loop & 3) {
            case 0:
                return 0;
            case 1:
                m_Ptn_no = 0;
                break;
            case 2:
                m_Ptn_no = anm->Frames - 1;
                break;
            }
        }
    }
    if (m_Tool_flg & 0x4000) {
        if (!EspGetAnmAddr(m_MaskTex_id, &anm)) {
            pLog->err(0, 0, "ESP : MaskTexId[%x] no data", m_Tex_id);
            return 0;
        }
        if (anm->Data_num == 0) {
            time = 1;
        } else {
            time = anm->Frame_cnt[anm->Frames + m_MaskPtn_no];
        }
        m_MaskAnm_cnt += m_Anm_rate;
        while ((m_MaskAnm_cnt >> 5) > (u16) time) {
            m_MaskPtn_no++;
            m_MaskAnm_cnt -= time << 5;
            if (m_MaskPtn_no >= anm->Frames) {
                switch (anm->Loop & 3) {
                case 0:
                    return 0;
                case 1:
                    m_MaskPtn_no = 0;
                    break;
                case 2:
                    m_MaskPtn_no = anm->Frames - 1;
                    break;
                }
            }
        }
    }
    return 1;
}

// Sets the sprite's material colour for the draw: lit sprites (Tool_flg 0x40) get the effect light
// list, Tool_flg 0x80/0x20000 select colour/alpha scaling; m_Flg bit 0 premultiplies rgb by alpha
// (additive sprites); the final colour filter (EffGetFinalCol) is applied unless m_Flg bit 2; the
// sprite fades out between m_Del_far and m_Del_near (x10 units along the camera axis). Returns 1
// when the resulting alpha is non-zero (worth drawing).
#line 1730 "D:/Bio4/Prog/esp_sub.cpp"
int cEsp::ChannelSet()
{
    GXColor col;
    GXColor fin;
    Vec p;
    Vec dir;
    Vec d;
    cEspSystem* sys = g_pEspSys;

    if (m_Tool_flg & 0x40) {
        GXSetTevOp(0, 0);
        GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
        GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        commonEspLightSet(sys->lightList.p, sys->lightList.num);
    } else {
        GXSetTevOp(0, 0);
        if (m_Tool_flg & 0x80) {
            GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        }
        if (m_Tool_flg & 0x20000) {
            GXSetTevAlphaOp(0, 0, 0, 2, 1, 0);
        }
        GXSetNumChans(1);
        GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    }
    if (m_Flg & 1) {
        col.r = (u8) (m_Col_r * m_Col_a * (1.0f / 255.0f));
        col.g = (u8) (m_Col_g * m_Col_a * (1.0f / 255.0f));
        col.b = (u8) (m_Col_b * m_Col_a * (1.0f / 255.0f));
        col.a = 0xFF;
    } else {
        col.r = (u8) m_Col_r;
        col.g = (u8) m_Col_g;
        col.b = (u8) m_Col_b;
        col.a = (u8) m_Col_a;
    }
    if (!(m_Flg & 4) && EffIsSetFinalCol()) {
        EffGetFinalCol(&fin);
        col.r = (u32) col.r * fin.r >> 8;
        col.g = (u32) col.g * fin.g >> 8;
        col.b = (u32) col.b * fin.b >> 8;
        col.a = (u32) col.a * fin.a >> 8;
    }
    if (m_Del_far != 0) {
        Camera* cam;
        f32 dot;

        if (parent != pEffParentWorld) {
            PSMTXMultVec(parent->mat, &m_Pos, &p);
        } else {
            p = m_Pos;
        }
        cam = &pG->Cam;
        dir.x = cam->param.at.x - cam->param.pos.x;
        dir.y = cam->param.at.y - cam->param.pos.y;
        dir.z = cam->param.at.z - cam->param.pos.z;
#line 1798 "D:/Bio4/Prog/esp_sub.cpp"
        VECNormalize(&dir, &dir);
        d.x = p.x - cam->param.pos.x;
        d.y = p.y - cam->param.pos.y;
        d.z = p.z - cam->param.pos.z;
        dot = PSVECDotProduct(&dir, &d);
        if (dot < m_Del_far * 10.0f) {
            f32 rate = 1.0f - (m_Del_far * 10.0f - dot) / ((m_Del_far - m_Del_near) * 10.0f);
            if (m_Flg & 1) {
                col.r = (u8) (col.r * rate);
                col.g = (u8) (col.g * rate);
                col.b = (u8) (col.b * rate);
            } else {
                col.a = (u8) (col.a * rate);
            }
        }
    }
    GXSetChanMatColor(4, col);
    {
        int ret = 0;
        if (col.a != 0) {
            ret = 1;
        }
        return ret;
    }
}

// Transforms position, speed and acceleration by m (world attachment); with Tool_flg bit 0 also
// rotates m_Ang by the matrix.
void cEsp::ApplyMatrix(Mtx m)
{
    Mtx r;

    PSMTXMultVec(m, &m_Pos, &m_Pos);
    PSMTXMultVecSR(m, &m_Speed, &m_Speed);
    PSMTXMultVecSR(m, &m_Speed_plus, &m_Speed_plus);
    if (m_Tool_flg & 1) {
        low_RotMatrix(r, &m_Ang);
        PSMTXConcat(m, r, r);
        Matrix2AxisAngle(r, &m_Ang);
    }
}

// GX state shared by the sprite draws: no culling, alpha compare, Z test without write, one TEV
// stage with texture matrix 0x1E.
void cEsp::CommonStateSet()
{
    GXSetCullMode(0);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetZMode(1, 3, 0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x1E);
}

// Effects are pool objects: the constructor does nothing (PullEsp clears the work).
cEsp::cEsp()
{
}

// Nothing to release in the base class.
cEsp::~cEsp()
{
}

// Hook run by PushEsp for ids that own external resources (cloth, buffers); empty in the base.
void cEsp::Destruct()
{
}

// Spawns record `no` of est table (owner, id) as one esp at the origin (identity matrix, fixed seed),
// bypassing the sequence timing; only Kind 0 records are allowed. bNoSuspend == 1 marks it with
// Core_flg bit 0 (kept through pauses). Returns 1 with *out = the esp, 0 (with the dummy) on error.
int EspEstSetSelect(int owner, int id, int no, cEsp** out, int bNoSuspend)
{
    EspSeqData* head;
    EspGenWork* rec;
    EspInfo info;
    Mtx m;
    u8 type;

    head = EspGetEstAddr(owner, id, 0);
    if (head == 0) {
        pLog->err(0, 0, "EspEstSetSelect:[%x/0x%02x] OWNER Invalid", owner, id);
        return 0;
    }
    if ((u32) no >= head->num) {
        pLog->err(0, 0, "EspEstSetSelect() : invalid no[%d] MAX=%d", no, head->num);
        return 0;
    }
    u32 seed = 0x12345678;
    rec = &head->rec[no];
    type = rec->Kind;
    if (type != 0) {
        if (type == 1) {
            pLog->err(0, 0, "EspEstSetSelect : can't call Espgen.");
            return 0;
        }
        pLog->err(0, 0, "EspEstSetSelect : KIND[%d] is invalid.", type);
        return 0;
    }
    PSMTXIdentity(m);
    memclr_asm(&info, sizeof(info));
    if (bNoSuspend == 1) {
        info.Core_flg |= 1;
    }
    return EspSeqSet(rec, &info, &seed, 0, &m, 0, 0.0f, out, 0, 0) == 1;
}

// Creates one esp from an effect record: Id 0xFC..0xFF are effect models (EfmSeqSet); otherwise
// pulls an esp of that id, copies the record (position/speed/acceleration/angle with the R_*
// random spreads from `seed`, sizes, colours and fade counts, blend table, life, release time,
// shimmer/mask ids, delete distances x10), resolves the parent (Parent_no scroll object unless
// event mode; Parts_no 0xFF = world through *mtx, 0xF8..0xFD screen, 0xFE free, else a parts of
// `model`, Tool_flg 0x20 = rotation only), runs the id's SetFreeWork, then the EspSeqOpt
// overrides/multipliers (speed, size, colour). flg != 0 rotates the speed by `f` radians about y
// (controller angle spread). Returns 1 with *out set; 0 with the dummy esp on failure.
int EspSeqSet(EspGenWork* rec, EspInfo* info, u32* seed, cModel* model, Mtx* mtx, int flg, f32 f, cEsp** out,
              EspSeqOpt* pSct, Vec* pos)
{
    static int bl[6][4] = {
        {1, 4, 5, 0}, {1, 4, 1, 0}, {1, 1, 1, 0}, {1, 2, 1, 0}, {1, 2, 0, 0}, {1, 4, 3, 0},
    };
    Vec v;
    Mtx m;
    Mtx m2;
    EspPtr e;
    f32 rnd;
    int ret;
    cModel* parts;

    if ((u8) (rec->Id + 4) <= 3) {
        EfmSeqSet(rec, (EfmCore*) info, seed, model, *mtx, 0, 0.0f, pos);
        *out = EspGetDmyPtr();
        return 1;
    }
    if (rec->Id == 0x3F) {
        pLog->err(0, 0, "ESP : ESP_ID[%x] is invalid.", rec->Id);
        *out = EspGetDmyPtr();
        return 0;
    }
    if (rec->Id != 3 && rec->Id != 9 && rec->Id != 0xC && !EspChkTexId(rec->Tex_id)) {
        pLog->err(0, 0, "ESP : TEX_ID[%x] not initialized.", rec->Tex_id);
        *out = EspGetDmyPtr();
        return 0;
    }
    if (PullEsp(&e.p, rec->Id) != 0) {
        e.p->info = *info;
        e.p->m_Id = rec->Id;
        e.p->m_Tex_id = rec->Tex_id;
        e.p->m_Type = rec->Type;
        e.p->m_Parts_no = rec->Parts_no;
        if (!(info->Core_flg & 0x1000) && rec->Parent_no != 0) {
            model = SmdGetObjPtr(rec->Parent_no - 1);
            if (model == 0) {
                pLog->err(0, 0, "ESP : PARENT_NO[%d] Invalid.", rec->Parent_no);
                PushEsp(e.p);
                *out = EspGetDmyPtr();
                return 0;
            }
        }
        e.p->m_Tool_flg = rec->Tool_flg;
        if (e.p->m_Tool_flg & 8) {
            if (fRandSeed1_1(seed) > 0.0f) {
                e.p->m_Tool_flg |= 2;
            } else {
                e.p->m_Tool_flg &= ~2;
            }
        }
        if (e.p->m_Tool_flg & 0x10) {
            if (fRandSeed1_1(seed) > 0.0f) {
                e.p->m_Tool_flg |= 4;
            } else {
                e.p->m_Tool_flg &= ~4;
            }
        }
        e.p->m_Pos = rec->Pos;
        e.p->m_Pos.x += rec->R_pos.x * fRandSeed1_1(seed);
        e.p->m_Pos.y += rec->R_pos.y * fRandSeed1_1(seed);
        e.p->m_Pos.z += rec->R_pos.z * fRandSeed1_1(seed);
        e.p->m_Speed = rec->Speed;
        e.p->m_Speed.x += rec->R_speed.x * fRandSeed1_1(seed);
        e.p->m_Speed.y += rec->R_speed.y * fRandSeed1_1(seed);
        e.p->m_Speed.z += rec->R_speed.z * fRandSeed1_1(seed);
        if (flg) {
            v.x = 0.0f;
            v.y = f;
            v.z = 0.0f;
            RotMatrixZXY(m, &v);
            PSMTXMultVecSR(m, &e.p->m_Speed, &e.p->m_Speed);
        }
        e.p->m_D_speed = rec->D_speed;
        e.p->m_Speed_plus = rec->Speed_plus;
        e.p->m_Speed_plus.x += rec->R_speed_plus.x * fRandSeed1_1(seed);
        e.p->m_Speed_plus.y += rec->R_speed_plus.y * fRandSeed1_1(seed);
        e.p->m_Speed_plus.z += rec->R_speed_plus.z * fRandSeed1_1(seed);
        e.p->m_Ang = rec->Ang;
        e.p->m_Ang.x += rec->R_ang.x * fRandSeed1_1(seed);
        e.p->m_Ang.y += rec->R_ang.y * fRandSeed1_1(seed);
        e.p->m_Ang.z += rec->R_ang.z * fRandSeed1_1(seed);
        PSVECScale(&e.p->m_Ang, &e.p->m_Ang, DEG2RAD);
        e.p->m_Ang_plus = rec->Ang_plus;
        e.p->m_Ang_plus.x += rec->R_ang_plus.x * fRandSeed1_1(seed);
        e.p->m_Ang_plus.y += rec->R_ang_plus.y * fRandSeed1_1(seed);
        e.p->m_Ang_plus.z += rec->R_ang_plus.z * fRandSeed1_1(seed);
        PSVECScale(&e.p->m_Ang_plus, &e.p->m_Ang_plus, DEG2RAD);
        e.p->m_Size_base_x = rec->Size_base_x;
        e.p->m_Size_base_y = rec->Size_base_y;
        e.p->m_Size_mul = 1.0f;
        rnd = rec->R_size_base * fRandSeed1_1(seed);
        e.p->m_Size_base_x += rnd;
        e.p->m_Size_base_y += rnd;
        e.p->m_Size_plus = rec->Size_plus;
        e.p->m_D_size_plus = rec->D_size_plus;
        e.p->m_Col_start_r = rec->Col_start_r;
        e.p->m_Col_start_g = rec->Col_start_g;
        e.p->m_Col_start_b = rec->Col_start_b;
        e.p->m_Col_start_a = rec->Col_start_a;
        e.p->m_Col_r = (f32) rec->Col_start_r;
        e.p->m_Col_g = (f32) rec->Col_start_g;
        e.p->m_Col_b = (f32) rec->Col_start_b;
        e.p->m_Col_a = (f32) rec->Col_start_a;
        e.p->m_Col_d_r = rec->Col_d_r;
        e.p->m_Col_d_g = rec->Col_d_g;
        e.p->m_Col_d_b = rec->Col_d_b;
        e.p->m_Col_d_a = rec->Col_d_a;
        if (rec->Blend_type > 5) {
            pLog->err(0, 0, "ESP : BLEND_TYPE[%d] Invalid.", rec->Blend_type);
            PushEsp(e.p);
            *out = EspGetDmyPtr();
            return 0;
        }
        e.p->m_Blend_type = rec->Blend_type;
        e.p->xA4 = bl[rec->Blend_type][0];
        e.p->xA5 = bl[rec->Blend_type][1];
        e.p->xA6 = bl[rec->Blend_type][2];
        e.p->xA7 = bl[rec->Blend_type][3];
        if (rec->Blend_type == 4) {
            e.p->m_Flg |= 1;
        }
        e.p->m_Col_max_cnt = rec->Col_max_cnt;
        e.p->m_Col_start_cnt = rec->Col_start_cnt;
        e.p->m_Pos_start_cnt = rec->Pos_start_cnt;
        e.p->m_Size_start_cnt = rec->Size_start_cnt;
        e.p->m_Life_max = rec->Life_max;
        e.p->m_Life_time = rec->Life_time;
        e.p->m_Ptn_no = rec->Ptn_no;
        e.p->m_Anm_rate = rec->Anm_rate + 0x20;
        e.p->m_Anm_cnt = rec->Anm_cnt;
        e.p->m_Release_time = rec->Release_time;
        e.p->m_Shimmer_type = rec->Shimmer_type;
        e.p->m_Shimmer_pow = rec->Shimmer_pow;
        e.p->m_MaskTex_id = rec->MaskTex_id;
        e.p->m_Del_far = rec->Del_far * 10;
        e.p->m_Del_near = rec->Del_near * 10;
        if (e.p->m_Shimmer_type != 0) {
            e.p->m_Flg |= 4;
        }
        switch (e.p->m_Parts_no) {
        case 0xFF:
            e.p->parent = pEffParentWorld;
            e.p->ApplyMatrix(*mtx);
            break;
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
            e.p->parent = pEffParentWorld;
            e.p->m_Pos.x += (*mtx)[0][3];
            e.p->m_Pos.y += (*mtx)[1][3];
            e.p->m_Pos.z += (*mtx)[2][3];
            break;
        case 0xFE:
            e.p->parent = pEffParentWorld;
            if (rec->Release_time != 0) {
                pLog->warn(0, 0, "ESP:ReleaseTime not 0 but no parent.");
            }
            break;
        default:
            if (model == 0) {
                pLog->err(0, 0, "ESP : PARTS_NO[%d] but Not on parts.", e.p->m_Parts_no);
                PushEsp(e.p);
                *out = EspGetDmyPtr();
                return 0;
            }
            if (e.p->m_Parts_no < model->nParts) {
                if (e.p->m_Tool_flg & 0x20) {
                    parts = model->getPartsPtr(e.p->m_Parts_no);
                    PSMTXIdentity(m2);
                    low_RotMatrix(m2, &model->ang);
                    PSMTXMultVecSR(m2, &rec->Pos, &v);
                    m2[0][3] = parts->mat[0][3] + v.x;
                    m2[1][3] = parts->mat[1][3] + v.y;
                    m2[2][3] = parts->mat[2][3] + v.z;
                    e.p->parent = pEffParentWorld;
                    e.p->m_Pos.x = 0.0f;
                    e.p->m_Pos.y = 0.0f;
                    e.p->m_Pos.z = 0.0f;
                    e.p->ApplyMatrix(m2);
                    e.p->m_Pos.x += rec->R_pos.x * fRandSeed1_1(seed);
                    e.p->m_Pos.y += rec->R_pos.y * fRandSeed1_1(seed);
                    e.p->m_Pos.z += rec->R_pos.z * fRandSeed1_1(seed);
                } else {
                    e.p->m_pMod = model;
                    e.p->m_Guid_pMod = model->serial;
                    e.p->parent = model->getPartsPtr(e.p->m_Parts_no);
                    if (pos) {
                        PSVECAdd(&e.p->m_Pos, pos, &e.p->m_Pos);
                    }
                }
            } else {
                pLog->err(0, 0, "ESP : PARTS_NO[%d] is invalid(MAX:%d).", e.p->m_Parts_no, model->nParts);
                PushEsp(e.p);
                *out = EspGetDmyPtr();
                return 0;
            }
            break;
        }
        ret = e.p->SetFreeWork(rec, seed);
        if (pSct) {
            if (pSct->OverWrite_flg & 1) {
                e.p->m_Speed = pSct->Speed;
                e.p->m_Speed.x += rec->R_speed.x * fRandSeed1_1(seed);
                e.p->m_Speed.y += rec->R_speed.y * fRandSeed1_1(seed);
                e.p->m_Speed.z += rec->R_speed.z * fRandSeed1_1(seed);
            }
            if (pSct->OverWrite_flg & 2) {
                e.p->m_Size_base_x = pSct->Size_base_x;
                e.p->m_Size_base_y = pSct->Size_base_y;
            }
            if (pSct->OverWrite_flg & 4) {
                e.p->m_Col_start_r = pSct->Col_start_r;
                e.p->m_Col_start_g = pSct->Col_start_g;
                e.p->m_Col_start_b = pSct->Col_start_b;
                e.p->m_Col_start_a = pSct->Col_start_a;
                e.p->m_Col_r = (f32) pSct->Col_start_r;
                e.p->m_Col_g = (f32) pSct->Col_start_g;
                e.p->m_Col_b = (f32) pSct->Col_start_b;
                e.p->m_Col_a = (f32) pSct->Col_start_a;
            }
            if (pSct->Mul_flg & 1) {
                e.p->m_Speed.x *= pSct->Speed.x;
                e.p->m_Speed.y *= pSct->Speed.y;
                e.p->m_Speed.z *= pSct->Speed.z;
            }
            if (pSct->Mul_flg & 2) {
                e.p->m_Size_base_x *= pSct->Size_base_x;
                e.p->m_Size_base_y *= pSct->Size_base_y;
            }
            if (pSct->Mul_flg & 4) {
                f32 c;
                c = (f32) e.p->m_Col_start_r * (f32) (int) pSct->Col_start_r * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_r = (u8) c;
                c = (f32) e.p->m_Col_start_g * (f32) (int) pSct->Col_start_g * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_g = (u8) c;
                c = (f32) e.p->m_Col_start_b * (f32) (int) pSct->Col_start_b * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_b = (u8) c;
                c = (f32) e.p->m_Col_start_a * (f32) (int) pSct->Col_start_a * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_a = (u8) c;
                e.p->m_Col_r = (f32) e.p->m_Col_start_r;
                e.p->m_Col_g = (f32) e.p->m_Col_start_g;
                e.p->m_Col_b = (f32) e.p->m_Col_start_b;
                e.p->m_Col_a = (f32) e.p->m_Col_start_a;
            }
            if (pSct->Add_flg & 1) {
                e.p->m_Speed.x += pSct->Speed.x;
                e.p->m_Speed.y += pSct->Speed.y;
                e.p->m_Speed.z += pSct->Speed.z;
            }
            if (pSct->Add_flg & 2) {
                e.p->m_Size_base_x += pSct->Size_base_x;
                e.p->m_Size_base_y += pSct->Size_base_y;
            }
            if (pSct->Add_flg & 4) {
                f32 c;
                c = (f32) e.p->m_Col_start_r + (f32) (int) pSct->Col_start_r * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_r = (u8) c;
                c = (f32) e.p->m_Col_start_g + (f32) (int) pSct->Col_start_g * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_g = (u8) c;
                c = (f32) e.p->m_Col_start_b + (f32) (int) pSct->Col_start_b * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_b = (u8) c;
                c = (f32) e.p->m_Col_start_a + (f32) (int) pSct->Col_start_a * (1.0f / 255.0f);
                if (c > 255.0f) {
                    c = 255.0f;
                }
                if (c < 0.0f) {
                    c = 0.0f;
                }
                e.p->m_Col_start_a = (u8) c;
                e.p->m_Col_r = (f32) e.p->m_Col_start_r;
                e.p->m_Col_g = (f32) e.p->m_Col_start_g;
                e.p->m_Col_b = (f32) e.p->m_Col_start_b;
                e.p->m_Col_a = (f32) e.p->m_Col_start_a;
            }
        }
    } else {
        ret = 0;
    }
    if (ret == 0) {
        *out = EspGetDmyPtr();
        if (e.p->m_Be_flg & 1) {
            PushEsp(e.p);
        }
        return 0;
    }
    *out = e.p;
    return ret;
}

// Bilinear point of the quad p0 p1 p2 p3 at (u, v).
void GetPosXY(Vec* p0, Vec* p1, Vec* p2, Vec* p3, f32 u, f32 v, Vec* out)
{
    static Vec v0;
    static Vec v1;
    static Vec v2;

    PSVECSubtract(p1, p0, &v0);
    PSVECSubtract(p2, p3, &v1);
    PSVECScale(&v0, &v0, u);
    PSVECScale(&v1, &v1, u);
    PSVECAdd(&v0, p0, &v0);
    PSVECAdd(&v1, p3, &v1);
    PSVECSubtract(&v1, &v0, &v2);
    PSVECScale(&v2, &v2, v);
    PSVECAdd(&v2, &v0, out);
}

// esp1b work at cEsp+0xF8: the quad is subdivided div x div times.
struct Esp1bSpWork {
    u32 div;   // 0x00
    Vec p1;    // 0x04 corner offsets from the unit quad (0,1,1) (1,1,1) (1,0,1) (0,0,1)
    Vec p2;    // 0x10
    Vec p3;    // 0x1C
};

// Draws esp1b's deformed sprite: the unit quad with the three corner offsets of Esp1bSpWork,
// subdivided div x div, as textured quads with an up normal.
void Esp1b_SpTrans(cEsp* esp)
{
    static Vec p0;
    static Vec p1;
    static Vec p2;
    static Vec p3;
    static Vec ret;
    Esp1bSpWork* w = (Esp1bSpWork*) &esp->pad_EC[0xC];
    u32 div;
    u32 i;
    u32 j;
    f32 step;
    f32 u;
    f32 u1;
    f32 v;
    f32 v1;
    f32 tu;
    f32 tu1;
    f32 tv;
    f32 tv1;

    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    div = w->div;
    p0.x = 0.0f;
    p0.y = 1.0f;
    p0.z = 1.0f;
    p1.x = w->p1.x + 1.0f;
    p1.y = w->p1.y + 1.0f;
    p1.z = w->p1.z + 1.0f;
    p2.x = w->p2.x + 1.0f;
    p2.y = w->p2.y + 0.0f;
    p2.z = w->p2.z + 1.0f;
    p3.x = w->p3.x + 0.0f;
    p3.y = w->p3.y + 0.0f;
    p3.z = w->p3.z + 1.0f;
    GXBegin(0x80, 0, div * 4 * div);
    step = 1.0f / (f32) div;
    v = 0.0f;
    tv = 0.0f;
    for (i = 0; i < div; i++) {
        v1 = v + step;
        tv1 = tv + step;
        u = 0.0f;
        tu = 0.0f;
        for (j = 0; j < div; j++) {
            u1 = u + step;
            tu1 = tu + step;
            GetPosXY(&p0, &p1, &p2, &p3, u, v, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu, tv);
            GetPosXY(&p0, &p1, &p2, &p3, u1, v, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu1, tv);
            GetPosXY(&p0, &p1, &p2, &p3, u1, v1, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu1, tv1);
            GetPosXY(&p0, &p1, &p2, &p3, u, v1, &ret);
            GXPosition3f32(ret.x, ret.y, ret.z);
            GXNormal3s8(0, 1, 0);
            GXTexCoord2f32(tu, tv1);
            u = u1;
            tu = tu1;
        }
        v += step;
        tv += step;
    }
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 1, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 1, 0);
}

// The split object's .sdata is padded to 8 bytes.
asm(".section .sdata; .balign 8");
