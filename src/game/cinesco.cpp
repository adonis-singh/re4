// game/cinesco.cpp: the cinema-scope letterbox bars. Events / rooms leave Status_flg[0]
// 0x1000000 set to request them; the bars fade in / out over 15 frames and Draw_cinesco paints
// the two black strips at the end of the frame.
#include "types.h"
#include "global.h"
#include "vec.h"
#include "gx.h"
#include "view.h"
#include "main_mem.h"
#include "cinesco.h"

CineWork cine_work;

extern "C" {
void cine_polling(CineWork* w);
void cine_on_move(CineWork* w);
void cine_off_move(CineWork* w);
}

// Per-frame: marks the letterbox request bit (Status_flg[0] 0x1000000; the room / event code
// clears it to turn the bars off) and runs the fade state (0 polling, 1 fading in, 2 fading out).
void CinescoMove(void)
{
    static void (*cine_tbl[])(CineWork*) = {
        cine_polling,
        cine_on_move,
        cine_off_move,
    };

    StaFlagOn(pG, STA_CINESCO);
    cine_tbl[cine_work.rno0](&cine_work);
}

// Rno0 == 0: watches the request bit and starts a 15 frame fade in / out when it changes.
void cine_polling(CineWork* w)
{
    int on;

    if (!StaFlagChk(pG, STA_CINESCO)) {
        on = 0;
    } else {
        on = 1;
    }
    if (w->on != on) {
        w->on = on;
        if (on) {
            w->rno0 = 1;
            w->timer0 = 15.0f;
        } else {
            w->rno0 = 2;
            w->timer0 = 15.0f;
        }
    }
}

// Rno0 == 1: alpha ramps 0 -> 255 over 15 frames.
void cine_on_move(CineWork* w)
{
    w->timer0 -= 1.0f;
    w->alpha = (u8) ((15.0f - w->timer0) / 15.0f * 255.0f);
    if (w->timer0 <= 0.0f) {
        w->rno0 = 0;
        w->alpha = 255;
    }
}

// Rno0 == 2: alpha ramps 255 -> 0 over 15 frames.
void cine_off_move(CineWork* w)
{
    w->timer0 -= 1.0f;
    w->alpha = (u8) (w->timer0 / 15.0f * 255.0f);
    if (w->timer0 <= 0.0f) {
        w->rno0 = 0;
        w->alpha = 0;
    }
}

// Draws the two black bars (rows 0..56 and 393..449 of the 512 x 448 screen) with the current
// alpha in an ortho projection; nothing when alpha is 0.
void Draw_cinesco(void)
{
    Mtx44 proj;
    Mtx mv;
    GXColor fog;
    u8 a = cine_work.alpha;

    if (a == 0) {
        return;
    }
    GXSetBlendMode(1, 4, 5, 0);
    GXSetColorUpdate(1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    GXSetCullMode(0);
    GXSetZMode(0, 7, 1);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 1, 0, 2);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXBegin(0x80, 0, 8);
    GXPosition3s16(0, 0, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 0, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 56, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(0, 56, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(0, 393, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 393, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 449, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(0, 449, 10);
    GXColor4u8(0, 0, 0, a);
}

// Clears the letterbox state.
void CinescoInit(void)
{
    memclr_asm(&cine_work, sizeof(CineWork));
}
