// game/esp08.cpp: effect id 0x08, a scrolling tiled texture sprite (water flow, fog sheets, energy
// fields). The quad is covered by Div_x x Div_y (Work8[0..1] / 10 + 1) copies of the texture,
// scrolled by Spd_x / Spd_y (prm 0xCC / 0xD0 x 0.001) per frame and drawn tile by tile so the
// scroll wraps; Tool_flg 0x4000 adds a mask texture (Mask_type Work8[2]), Work8[3] fades the sprite
// out while the player is in a weather-off area. Esp08_TransShimmer is the same tiling drawn
// through the frame-buffer copy as a heat shimmer (used by EspCommonTransShimmer in esp_sub).

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "main_sub.h"
#include "tpl.h"
#include "espgen.h"

// Scrolling-texture sprite (Esp08_Trans) and the heat-shimmer variant (Esp08_TransShimmer).
struct Esp08Work {
    f32 Div_x;     // 0x00 texture repeat along s (>= 1)
    f32 Div_y;     // 0x04 texture repeat along t
    f32 Spd_x;      // 0x08 scroll speed
    f32 Spd_y;      // 0x0C
    f32 Scr_x;      // 0x10 scroll offset (kept in 0..1)
    f32 Scr_y;      // 0x14
    u8 Mask_type;   // 0x18 0/1
    u8 pad_19[3];
    f32 colA0;     // 0x1C initial alpha (esp->colA)
    u8 Room_del_frame; // 0x20 frames the alpha fades in (0: none)
    u8 Room_del_cnt;    // 0x21
};

class cEsp08 : public cEsp {
public:
    Esp08Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
cEsp* Esp08_Create();
void Esp08_Trans(cEsp08* esp);
void Esp08_TransShimmer(cEsp08* esp, int type);
f32 EspGetCameraPan2();   // game/esp.cpp
}

extern f32 ZNEAR;
extern f32 ZFAR;

#define ESP_PARTS_SCREEN(esp) ((s8) (esp)->m_Parts_no >= -8 && (s8) (esp)->m_Parts_no <= -3)

// One tile of the scrolling texture quad (position, normal, texture coordinate).
#define ESP08_QUAD(px, py, px1, py1, ps0, pt0, ps1, pt1)                                          \
    GXBegin(0x80, 0, 4);                                                                          \
    GXPosition3f32(px, py, z);                                                                    \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps0, pt0);                                                                     \
    GXPosition3f32(px1, py, z);                                                                   \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps1, pt0);                                                                     \
    GXPosition3f32(px1, py1, z);                                                                  \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps1, pt1);                                                                     \
    GXPosition3f32(px, py1, z);                                                                   \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps0, pt1);

// Same tile with the mask texture coordinates (maskType 1: the mask is stretched over the
// whole sprite, so every tile gets its own part of it).
#define ESP08_QUAD2(px, py, px1, py1, ps0, pt0, ps1, pt1, pu0, pv0, pu1, pv1)                     \
    GXBegin(0x80, 0, 4);                                                                          \
    GXPosition3f32(px, py, z);                                                                    \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps0, pt0);                                                                     \
    GXTexCoord2f32(pu0, pv0);                                                                     \
    GXPosition3f32(px1, py, z);                                                                   \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps1, pt0);                                                                     \
    GXTexCoord2f32(pu1, pv0);                                                                     \
    GXPosition3f32(px1, py1, z);                                                                  \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps1, pt1);                                                                     \
    GXTexCoord2f32(pu1, pv1);                                                                     \
    GXPosition3f32(px, py1, z);                                                                   \
    GXNormal3s8(0, 1, 0);                                                                         \
    GXTexCoord2f32(ps0, pt1);                                                                     \
    GXTexCoord2f32(pu0, pv1);

// The sprite is covered with rateX x rateY copies of the texture, scrolled by (ofsX, ofsY):
// the tile that wraps in both directions first, then the wrapping column, the wrapping row and
// the full grid, each tile a separate quad (the last column/row is cut at the sprite edge).
// In the mask (ind) arms du/dv are assigned BEFORE u0/v0: where du is the plain reciprocal
// `1.0f / w->Div_x`, the original's u0 multiply reuses du's register (`fmadds f20,f0,f30,f13`),
// i.e. the reciprocal was computed for du first and cse folded u0's copy of it into du.
// First tile: the original's non-mask quad stores the copies (x, y, ss1, st1) and the mask quad
// the originals (x0, y0, s1, t1). Ours links each copy to its original in cse1 (the copy is
// promoted to canonical: its last use is beyond the ebb and later than the original's), gcse
// copy-propagates it back and cse2 canonicalises again, so one register serves both quads;
// the codeless asm after each copy makes it opaque (COMPILER-DIFF: first-tile copy canon).
// The mask arm's du/dv divides sit in a `do { } while (0)`: as plain statements the scheduler's
// non-pipelined divider (fdivs blockage 17) delays the `cu + du` / `cv + dv` adds past the
// second y0 store, so reload inherits the first y0 reload; the original has the adds right
// after GXBegin and reloads y0 twice, as if the divides sat in another block. The loop notes
// give exactly that: NOTE_INSN_LOOP_END ends the cse1 ebb and is a haifa scheduling barrier
// for the insn that follows (COMPILER-DIFF: candidate (first-tile divider blockage)).
// The `=m` keep-alive after the mask quad keeps y and st1 live through it (global-alloc order
// y after the double loop's 0x4330 magic, st1 after y1: f24/f21 as the original) without
// touching ss1/x. See "DOL esp08/esp18 final closer" in docs/research/.
#define ESP08_TILES()                                                                             \
    ds = s1 - s0;                                                                                 \
    dt = t1 - t0;                                                                                 \
    remX = 1.0f - (w->Div_x - (f32) (u32) w->Div_x);                                              \
    remY = 1.0f - (w->Div_y - (f32) (u32) w->Div_y);                                              \
    numX = (u32) w->Div_x + 1;                                                                    \
    numY = (u32) w->Div_y + 1;                                                                    \
    if (remX == 1.0f) {                                                                           \
        remX = 0.0f;                                                                              \
        numX--;                                                                                   \
    }                                                                                             \
    if (remY == 1.0f) {                                                                           \
        remY = 0.0f;                                                                              \
        numY--;                                                                                   \
    }                                                                                             \
    tileH = -sy / w->Div_y;                                                                       \
    tileW = sx / w->Div_x;                                                                        \
    if (w->Scr_x != 0.0f) {                                                                        \
        if (w->Scr_y != 0.0f) {                                                                    \
            x = x0; asm("" : "+f"(x)); /* COMPILER-DIFF: candidate (first-tile copy canon) */  \
            y = y0; asm("" : "+f"(y));                                                            \
            y1 = y + tileH * w->Scr_y;                                                             \
            x1 = x + tileW * w->Scr_x;                                                             \
            st0 = t0 + dt * (1.0f - w->Scr_y);                                                     \
            ss0 = s0 + ds * (1.0f - w->Scr_x);                                                     \
            st1 = t1; asm("" : "+f"(st1));                                                        \
            ss1 = s1; asm("" : "+f"(ss1));                                                        \
            if (!ind) {                                                                           \
                ESP08_QUAD(x, y, x1, y1, ss0, st0, ss1, st1)                                      \
            } else {                                                                              \
                f32 cu = 0.0f;                                                                    \
                f32 cv = 0.0f;                                                                    \
                do { /* COMPILER-DIFF: candidate (first-tile divider blockage): the loop notes */ \
                    du = w->Scr_x / w->Div_x; /* end the cse1 ebb and are a haifa barrier, so   */ \
                    dv = w->Scr_y / w->Div_y; /* the divides are scheduled as in another block  */ \
                } while (0);                                                                      \
                ESP08_QUAD2(x0, y0, x1, y1, ss0, st0, s1, t1, cu, cv, cu + du, cv + dv)           \
                asm("" : "=m"(inv[0][0]) : "f"(y), "f"(st1)); /* COMPILER-DIFF: candidate (keep-alive, global-alloc order) */ \
            }                                                                                     \
        }                                                                                         \
        y = y0 + tileH * w->Scr_y;                                                                 \
        for (i = 0; i < numY; i++) {                                                              \
            if (i == numY - 1) {                                                                  \
                y1 = y0 - sy;                                                                     \
                st1 = t1 - dt * (remY + w->Scr_y);                                                 \
            } else {                                                                              \
                st1 = t1;                                                                         \
                y1 = y + tileH;                                                                   \
            }                                                                                     \
            x1 = x0 + tileW * w->Scr_x;                                                            \
            ss0 = s0 + ds * (1.0f - w->Scr_x);                                                     \
            if (ind) {                                                                            \
                if (i == numY - 1) {                                                              \
                    du = w->Scr_x / w->Div_x;                                                      \
                    dv = (1.0f - w->Scr_y) / w->Div_y;                                             \
                    u0 = 0.0f;                                                                    \
                    v0 = (f32) i * (1.0f / w->Div_y) + w->Scr_y / w->Div_y;                        \
                } else {                                                                          \
                    du = w->Scr_x / w->Div_x;                                                      \
                    dv = 1.0f / w->Div_y;                                                         \
                    u0 = 0.0f;                                                                    \
                    v0 = (f32) i * (1.0f / w->Div_y) + w->Scr_y / w->Div_y;                        \
                }                                                                                 \
            }                                                                                     \
            if (!ind) {                                                                           \
                ESP08_QUAD(x0, y, x1, y1, ss0, t0, s1, st1)                                       \
            } else {                                                                              \
                ESP08_QUAD2(x0, y, x1, y1, ss0, t0, s1, st1, u0, v0, u0 + du, v0 + dv)            \
            }                                                                                     \
            y = y1;                                                                               \
        }                                                                                         \
    }                                                                                             \
    if (w->Scr_y != 0.0f) {                                                                        \
        x = x0 + tileW * w->Scr_x;                                                                 \
        st0 = t0 + dt * (1.0f - w->Scr_y);                                                         \
        y1 = y0 + tileH * w->Scr_y;                                                                \
        for (j = 0; j < numX; j++) {                                                              \
            if (j == numX - 1) {                                                                  \
                x1 = x0 + sx;                                                                     \
                ss1 = s1 - ds * (remX + w->Scr_x);                                                 \
            } else {                                                                              \
                ss1 = s1;                                                                         \
                x1 = x + tileW;                                                                   \
            }                                                                                     \
            if (ind) {                                                                            \
                if (j == numX - 1) {                                                              \
                    du = (1.0f - w->Scr_x) / w->Div_x;                                             \
                    dv = w->Scr_y / w->Div_y;                                                      \
                    u0 = (f32) j * (1.0f / w->Div_x) + w->Scr_x / w->Div_x;                        \
                    v0 = 0.0f;                                                                    \
                } else {                                                                          \
                    du = 1.0f / w->Div_x;                                                         \
                    dv = w->Scr_y / w->Div_y;                                                      \
                    u0 = (f32) j * (1.0f / w->Div_x) + w->Scr_x / w->Div_x;                        \
                    v0 = 0.0f;                                                                    \
                }                                                                                 \
            }                                                                                     \
            if (!ind) {                                                                           \
                ESP08_QUAD(x, y0, x1, y1, s0, st0, ss1, t1)                                       \
            } else {                                                                              \
                ESP08_QUAD2(x, y0, x1, y1, s0, st0, ss1, t1, u0, v0, u0 + du, v0 + dv)            \
            }                                                                                     \
            x = x1;                                                                               \
        }                                                                                         \
    }                                                                                             \
    y = y0 + tileH * w->Scr_y;                                                                     \
    for (i = 0; i < numY; i++) {                                                                  \
        if (i == numY - 1) {                                                                      \
            y1 = y0 - sy;                                                                         \
            st1 = t1 - dt * (remY + w->Scr_y);                                                     \
        } else {                                                                                  \
            st1 = t1;                                                                             \
            y1 = y + tileH;                                                                       \
        }                                                                                         \
        x = x0 + tileW * w->Scr_x;                                                                 \
        for (j = 0; j < numX; j++) {                                                              \
            if (j == numX - 1) {                                                                  \
                x1 = x0 + sx;                                                                     \
                ss1 = s1 - ds * (remX + w->Scr_x);                                                 \
            } else {                                                                              \
                ss1 = s1;                                                                         \
                x1 = x + tileW;                                                                   \
            }                                                                                     \
            if (ind) {                                                                            \
                if (i == numY - 1) {                                                              \
                    if (j == numX - 1) {                                                          \
                        du = (1.0f - w->Scr_x) / w->Div_x;                                         \
                        dv = (1.0f - w->Scr_y) / w->Div_y;                                         \
                        u0 = (f32) j * (1.0f / w->Div_x) + w->Scr_x / w->Div_x;                    \
                        v0 = (f32) i * (1.0f / w->Div_y) + w->Scr_y / w->Div_y;                    \
                    } else {                                                                      \
                        du = 1.0f / w->Div_x;                                                     \
                        dv = (1.0f - w->Scr_y) / w->Div_y;                                         \
                        u0 = (f32) j * (1.0f / w->Div_x) + w->Scr_x / w->Div_x;                    \
                        v0 = (f32) i * (1.0f / w->Div_y) + w->Scr_y / w->Div_y;                    \
                    }                                                                             \
                } else {                                                                          \
                    if (j == numX - 1) {                                                          \
                        /* u0, dv, v0, du: local-alloc ranks the ofsX/rateX quotient above the   \
                           rateX load here (f9/f8) only in this statement order */                \
                        u0 = (f32) j * (1.0f / w->Div_x) + w->Scr_x / w->Div_x;                    \
                        dv = 1.0f / w->Div_y;                                                     \
                        v0 = (f32) i * (1.0f / w->Div_y) + w->Scr_y / w->Div_y;                    \
                        du = (1.0f - w->Scr_x) / w->Div_x;                                         \
                    } else {                                                                      \
                        du = 1.0f / w->Div_x;                                                     \
                        dv = 1.0f / w->Div_y;                                                     \
                        u0 = (f32) j * (1.0f / w->Div_x) + w->Scr_x / w->Div_x;                    \
                        v0 = (f32) i * (1.0f / w->Div_y) + w->Scr_y / w->Div_y;                    \
                    }                                                                             \
                }                                                                                 \
            }                                                                                     \
            if (!ind) {                                                                           \
                ESP08_QUAD(x, y, x1, y1, s0, t0, ss1, st1)                                        \
            } else {                                                                              \
                ESP08_QUAD2(x, y, x1, y1, s0, t0, ss1, st1, u0, v0, u0 + du, v0 + dv)             \
            }                                                                                     \
            x = x1;                                                                               \
        }                                                                                         \
        y = y1;                                                                                   \
    }

// Texture coordinate corners for the sprite orientation (flags bit1: flip s, bit2: flip t;
// screen sprites are drawn upside down). The two orientation tests are combined in one
// condition and the corners are built from a `zero` variable: the four leaves are then jump
// targets where cse knows neither operand of `zero + z`, which keeps the adds (with nested ifs
// and literals cse folds 0 + z into z). The flip-s leaves add first and copy after: the add
// reads `zero`'s register, the copies come from the copied variable.
#define ESP08_FLIP_T(esp)                                                                         \
    ((ESP_PARTS_SCREEN(esp) && !((esp)->m_Tool_flg & 4)) || (!ESP_PARTS_SCREEN(esp) && ((esp)->m_Tool_flg & 4)))
#define ESP08_TEXCOORD_SET()                                                                      \
    if (esp->m_Tool_flg & 2) {                                                                         \
        if (ESP08_FLIP_T(esp)) {                                                                  \
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
        if (ESP08_FLIP_T(esp)) {                                                                  \
            s0 = zero;                                                                            \
            t0 = s0 + z;                                                                          \
            t1 = s0;                                                                              \
            s1 = t0;                                                                              \
        } else {                                                                                  \
            s0 = zero;                                                                            \
            s1 = s0 + z;                                                                          \
            t0 = s0;                                                                              \
            t1 = s1;                                                                              \
        }                                                                                         \
    }

// Mask texture (flags bit14) in TEV stage 1, texture coordinate `coord` from texgen `coord`
// (maskType 1 stretches it over the sprite through the tile coordinates: ind).
#define ESP08_MASK_SET(coord, mapId, texDecl, tlutDecl)                                           \
    if (esp->m_Tool_flg & 0x4000) {                                                                    \
        EspTexWk* tw;                                                                             \
        if (w->Mask_type == 1) {                                                                   \
            ind = 1;                                                                              \
        }                                                                                         \
        tw = EspGetTexWk(esp->m_MaskTex_id, 0);                                                         \
        if (tw != NULL) {                                                                         \
            texDecl;                                                                              \
            tlutDecl;                                                                             \
            TEXDescriptor* td = TEXGet(tw->pTpl, esp->m_MaskPtn_no);                                   \
            TEXHeader* th = td->textureHeader;                                                    \
                                                                                                  \
            if (th->format == 8 || th->format == 9) {                                             \
                GXInitTexObjCI(pTex, th->data, th->width, th->height, th->format, 0, 0, 0, 1);    \
                GXInitTlutObj(pTlut, td->CLUTHeader->data, td->CLUTHeader->format,                \
                              td->CLUTHeader->numEntries);                                        \
                GXLoadTlut(pTlut, 1);                                                             \
            } else {                                                                              \
                GXInitTexObj(pTex, th->data, th->width, th->height, th->format, 0, 0, 0);         \
            }                                                                                     \
            GXLoadTexObj(pTex, mapId);                                                            \
            GXLoadTexMtxImm(tw->mtx, 0x21, 1);                                                    \
            if (ind) {                                                                            \
                GXSetTexCoordGen2(coord, 1, 5, 0x21, 0, 0x7D);                                    \
            } else {                                                                              \
                GXSetTexCoordGen2(coord, 1, 4, 0x21, 0, 0x7D);                                    \
            }                                                                                     \
            GXSetNumTevStages(2);                                                                 \
            GXSetNumTexGens(coord + 1);                                                           \
            GXSetTevOrder(1, coord, mapId, 4);                                                    \
            GXSetTevColorIn(1, 0xF, 0xF, 0xF, 0);                                                 \
            GXSetTevColorOp(1, 0, 0, 0, 1, 0);                                                    \
            GXSetTevAlphaIn(1, 7, 4, 5, 7);                                                       \
            GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);                                                    \
        }                                                                                         \
    }                                                                                             \
    {                                                                                             \
        u32 sysFlags = pG->Status_flg[1];                                                            \
        if ((!(sysFlags & 0x80) && (esp->m_Tool_flg & 0x8000)) ||                                      \
            (StaFlagChk(pG, STA_ALPHA_DRAW2) && (esp->m_Tool_flg & 0x800000))) {                               \
            GXSetAlphaUpdate(1);                                                                  \
        }                                                                                         \
    }                                                                                             \
    GXClearVtxDesc();                                                                             \
    GXSetVtxDesc(9, 1);                                                                           \
    GXSetVtxDesc(0xA, 1);                                                                         \
    GXSetVtxDesc(0xD, 1);                                                                         \
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);                                                               \
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);                                                             \
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);                                                             \
    if (ind) {                                                                                    \
        GXSetVtxDesc(0xE, 1);                                                                     \
        GXSetVtxAttrFmt(0, 0xE, 1, 4, 0);                                                         \
    }

// EspCreateTbl[0x08] factory.
cEsp* Esp08_Create()
{
    return new cEsp08;
}

// Base update / animation, advances the scroll offsets (wrapped to 0..1) and, with Room_del_frame,
// fades the alpha toward 0 over that many frames while Status_flg[1] 0x02000000 (indoor area) is
// set and back when it clears.
void cEsp08::move()
{
    Esp08Work* w = &m_Free;

    if (w->Room_del_frame != 0) {
        m_Col_a = w->colA0;
    }
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
            return;
        }
        w->Scr_x += w->Spd_x;
        w->Scr_y += w->Spd_y;
        while (w->Scr_x > 1.0f) {
            w->Scr_x -= 1.0f;
        }
        while (w->Scr_y > 1.0f) {
            w->Scr_y -= 1.0f;
        }
        while (w->Scr_x < 0.0f) {
            w->Scr_x += 1.0f;
        }
        while (w->Scr_y < 0.0f) {
            w->Scr_y += 1.0f;
        }
        if (w->Room_del_frame != 0) {
            if (StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
                w->Room_del_cnt++;
            } else {
                if (w->Room_del_cnt == 0) {
                    return;
                }
                w->Room_del_cnt--;
            }
            if (w->Room_del_cnt == 0) {
                return;
            }
            if (w->Room_del_cnt >= w->Room_del_frame) {
                w->Room_del_cnt = w->Room_del_frame;
            }
            m_Col_a = w->colA0 * (1.0f - (f32) w->Room_del_cnt / (f32) (int) w->Room_del_frame);
        }
    }
}

// EspTransTbl[0x08]: sprite matrix (screen ortho / camera-facing / rotated), texture (+ optional
// mask on stage 1), then the ESP08_TILES grid: the wrapping corner tile, the wrapping column and
// row, and the full grid of Div_x x Div_y quads with the scroll offset applied to the texture
// coordinates.
void Esp08_Trans(cEsp08* esp)
{
    Esp08Work* w = &esp->m_Free;
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    f32 y0;
    f32 sx;
    f32 sy;
    f32 tileW;
    f32 tileH;
    f32 ds;
    f32 dt;
    f32 remX;
    f32 remY;
    f32 ox;
    f32 oy;
    f32 x0;
    f32 z;
    f32 zero;
    f32 s0;
    f32 s1;
    f32 t0;
    f32 t1;
    f32 x;
    f32 y;
    f32 x1;
    f32 y1;
    f32 ss0;
    f32 st0;
    f32 ss1;
    f32 st1;
    f32 u0;
    f32 v0;
    f32 du;
    f32 dv;
    u32 numX;
    u32 numY;
    u32 i;
    u32 j;
    int ind = 0;

    if (esp->m_Shimmer_type != 0) {
        Esp08_TransShimmer(esp, esp->m_Shimmer_pow);
        return;
    }
    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    CameraCurrentProjection();
    if (ESP_PARTS_SCREEN(esp)) {
        PSMTXIdentity(esp->m_Mat);
        RotMatrix(esp->m_Mat, &esp->m_Ang);
        TransMatrix(esp->m_Mat, &esp->m_Pos);
        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
        GXSetProjection(proj, 1);
    } else if (!(esp->m_Tool_flg & 1)) {
        Vec p;
        Mtx m;

        PSMTXIdentity(esp->m_Mat);
        PSMTXRotRad(esp->m_Mat, 'z', esp->m_Ang.z);
        if (esp->m_Tool_flg & 0x80000) {
            PSMTXRotRad(m, 'x', EspGetCameraPan2() * (3.1415927f / 180.0f));
            PSMTXConcat(m, esp->m_Mat, esp->m_Mat);
        }
        PSMTXConcat(pG->Cam.v_mat, esp->parent->mat, m);
        PSMTXMultVec(m, &esp->m_Pos, &p);
        esp->m_Mat[0][3] = p.x;
        esp->m_Mat[1][3] = p.y;
        esp->m_Mat[2][3] = p.z;
    } else {
        Mtx m;

        PSMTXIdentity(esp->m_Mat);
        RotMatrix(esp->m_Mat, &esp->m_Ang);
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
    esp->ChannelSet();
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
    {
        GXTexObj tex;
        GXTlutObj tlut;
        ESP08_MASK_SET(1, 1, GXTexObj* pTex = &tex, GXTlutObj* pTlut = &tlut)
    }
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
    ESP08_TEXCOORD_SET()
    ESP08_TILES()
    if (esp->m_Tool_flg & 0x4000) {
        GXSetNumTevStages(1);
        GXSetNumTexGens(1);
    }
    if (esp->m_Tool_flg & 0x808000) {
        GXSetAlphaUpdate(0);
    }
}

// Heat-shimmer variant: the frame is copied into a texture and warped through an indirect
// texture (esp->xEC selects the warp mode, type scales the distortion), tiled like Esp08_Trans.
void Esp08_TransShimmer(cEsp08* esp, int type)
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
    Esp08Work* w = &esp->m_Free;
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
    f32 y0;
    f32 sx;
    f32 sy;
    f32 tileW;
    f32 tileH;
    f32 ds;
    f32 dt;
    f32 remX;
    f32 remY;
    f32 ox;
    f32 oy;
    f32 x0;
    f32 z;
    f32 zero;
    f32 s0;
    f32 s1;
    f32 t0;
    f32 t1;
    f32 x;
    f32 y;
    f32 x1;
    f32 y1;
    f32 ss0;
    f32 st0;
    f32 ss1;
    f32 st1;
    f32 u0;
    f32 v0;
    f32 du;
    f32 dv;
    f32 ofs;
    f32 scale;
    f32 dot;
    u32 numX;
    u32 numY;
    u32 i;
    u32 j;
    int ind = 0;
    void* buf;

    scale = (f32) type * (1.0f / 32.0f) + 1.0f;
    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    CameraCurrentProjection();
    if (ESP_PARTS_SCREEN(esp)) {
        PSMTXIdentity(esp->m_Mat);
        RotMatrix(esp->m_Mat, &esp->m_Ang);
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
        RotMatrix(esp->m_Mat, &esp->m_Ang);
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
    esp->ChannelSet();
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
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
    ESP08_TEXCOORD_SET()
    if (ESP_PARTS_SCREEN(esp)) {
        ofs = 56.0f;
    } else if (SysFlagChk(pG, SYS_SCISSOR_ON)) {
        ofs = 56.0f;
    } else {
        ofs = 0.0f;
    }
    if (StaFlagChk(pG, STA_TEX_RENDER)) {
        ofs = 0.0f;
    }
    GXTexObj tex;
    f32 indMtx[2][3];
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    buf = GetDrawTmpBufAddr(1);
    if (buf == NULL) {
        pLog->warn(0, 0, "Esp08() : not enough memory");
        return;
    }
    GXSetTexCopySrc(0, (u32) ofs, (u32) Screen.width, (u32) (Screen.height - ofs));
    GXSetTexCopyDst((u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 1);
    GXCopyTex(buf, 0);
    GXPixModeSync();
    GXInvalidateTexAll();
    GXInitTexObj(&tex, buf, (u32) Screen.width / 2, (u32) ((f32) ((u32) Screen.height / 2) - ofs), 6, 0, 0, 0);
    GXLoadTexObj(&tex, 1);
    Mtx tm;
    Mtx pm;
    if (ESP_PARTS_SCREEN(esp)) {
        if (StaFlagChk(pG, STA_TEX_RENDER)) {
            PSMTXConcat(Matrix1, esp->m_Mat, tm);
        } else {
            PSMTXConcat(Matrix2, esp->m_Mat, tm);
        }
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen(0, 1, 0, 0x1E);
    } else {
        f32 fovy = pG->Cam.param.fovy;
        if (SysFlagChk(pG, SYS_SCISSOR_ON)) {
            C_MTXLightPerspective(pm, fovy, 1.3333334f, 0.5f, -0.6666667f, 0.5f, 0.5f);
        } else {
            C_MTXLightPerspective(pm, fovy, 1.3333334f, 0.5f, -0.5f, 0.5f, 0.5f);
        }
        PSMTXConcat(pm, esp->m_Mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 0);
        GXSetTexCoordGen(0, 0, 0, 0x1E);
    }
    GXSetNumTevStages(1);
    GXSetNumIndStages(1);
    GXSetNumTexGens(2);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetIndTexOrder(0, 1, 0);
    GXSetIndTexCoordScale(0, 0, 0);
    dot = 2500.0f;
    if (esp->m_Shimmer_type != 3) {
        indMtx[1][1] = indMtx[0][0] = esp->m_Col_a * (1.0f / 255.0f) * 0.04f * 1000.0f / dot * scale;
        indMtx[0][1] = 0.0f;
        indMtx[0][2] = 0.0f;
        indMtx[1][0] = 0.0f;
        indMtx[1][2] = 0.0f;
    } else {
        static f32 prm1 = 0.5f;
        static f32 prm2 = 0.0f;
        static f32 prm3 = 0.0f;
        static f32 prm4 = Screen.width * 0.5f / Screen.width;   // sic (esp_sub uses height)

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

        switch (esp->m_Shimmer_type) {
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
            pLog->err(0, 0, "ESP_SHIMMER : BLUR_TYPE[%x] invalid", esp->m_Shimmer_type);
            signedOfs = 0;
            replace = 1;
            break;
        }
        GXSetTevIndWarp(0, 0, signedOfs, replace, 1);
    }
    GXSetTevOrder(0, 0, 1, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    ESP08_MASK_SET(2, 2, GXTexObj* pTex = &tex, GXTlutObj* pTlut = (GXTlutObj*) indMtx)
    ESP08_TILES()
    if (esp->m_Tool_flg & 0x4000) {
        GXSetNumTevStages(1);
        GXSetNumTexGens(1);
    }
    if (esp->m_Tool_flg & 0x808000) {
        GXSetAlphaUpdate(0);
    }
    GXSetNumTevStages(1);
    GXSetNumTexGens(0);
    GXSetNumIndStages(0);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    LightMgr.setFog();
}

// Repeat counts from Work8[0..1] (min 1), scroll speeds from prm 0xCC / 0xD0, indoor fade frames
// Work8[3], mask type Work8[2] (0/1, else fails); remembers the initial alpha.
int cEsp08::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp08Work* w = &m_Free;
    u32 type;

    w->Div_x = (f32) (int) gen->Work8[0] * 0.1f + 1.0f;
    w->Div_y = (f32) (int) gen->Work8[1] * 0.1f + 1.0f;
    if (w->Div_x < 1.0f) {
        w->Div_x = 1.0f;
    }
    if (w->Div_y < 1.0f) {
        w->Div_y = 1.0f;
    }
    w->Spd_x = (f32) (s32) gen->prm.w.xCC * 0.001f;
    w->Spd_y = (f32) (s32) gen->prm.w.xD0 * 0.001f;
    w->Scr_x = 0.0f;
    w->Scr_y = 0.0f;
    w->Room_del_frame = gen->Work8[3];
    w->colA0 = m_Col_a;
    w->Mask_type = gen->Work8[2];
    type = w->Mask_type;
    if (type > 1) {
        pLog->err(0, 0, "ESP08 : MaskType[%x] invalid", type);
        return 0;
    }
    return 1;
}

// The split object's .sdata is 8-aligned and 0x10 bytes (prm1..prm3 + 4 pad).
asm(".section .sdata; .balign 8");
