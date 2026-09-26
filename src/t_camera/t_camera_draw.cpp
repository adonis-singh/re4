#include "types.h"
#include "global.h"
#include "vec.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "view.h"
#include "math_sub.h"
#include "eprintf.h"
#include "light.h"
#include "t_camera.h"

// Camera tool (t_camera REL, t_camera_draw.cpp): the per-frame tool camera update (projection / view
// matrices into pG->Camera), n-gon outline / fill helpers, the TcMenu drawer with the blinking cursor and
// the CamBSpline preview curve.

// Menu cursor blink timer (a struct: its stores alias the pad reads through pTc, which are
// reloaded after `blink = 8` in the target).
static int tcMenuBlink = 4;
static int tcMenuDummy = 0;
// joy.trg / rep through byte pointers: the member forms reschedule tcMenuSelect (60 words)
#define TC_TRG (*(u32*) ((u8*) pTc + 0x120))
#define TC_REP (*(u32*) ((u8*) pTc + 0x128))

// Per frame: rebuilds the tool camera's projection (perspective or ortho by CameraGetProjection)
// and view matrices, copies it into pG->Camera and updates the view.
void tcCameraMove()
{
    TcWork* w = pTc;
    CAMERA* cam = &w->cam;

    switch (CameraGetProjection()) {
    case 1:
        C_MTXPerspective(cam->ProjMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
        break;
    case 2:
        C_MTXOrtho(cam->ProjMat, ORTHO_T, ORTHO_B, ORTHO_L, ORTHO_R, 0.0f, ZFAR);
        break;
    }
    cam->Distance = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->v_mat, &cam->param.pos, &cam->Up, &cam->param.at);
    tcToolCamera2GameCamera();
    View.move();
}

// Outline of an n-gon (closed line loop).
void tcDrawNgon(TcNgon* ngon, u32 color)
{
    Vec a;
    Vec b;
    int num = ngon->num;
    int i;
    int n;

    for (i = 0; i < num; i++) {
        n = (i + 1) % num;
        a.x = ngon->vtx[i].x;
        a.y = ngon->vtx[i].y;
        a.z = ngon->vtx[i].z;
        b.x = ngon->vtx[n].x;
        b.y = ngon->vtx[n].y;
        b.z = ngon->vtx[n].z;
        tcDrawLine3D(&a, &b, color);
    }
}

// Filled n-gon as a triangle fan.
void tcFillNgon(TcNgon* ngon, u32 color)
{
    Vec p[3];
    int i;
    int n = ngon->num - 1;

    p[0] = ngon->vtx[0];
    for (i = 1; i < n; i++) {
        p[1] = ngon->vtx[i];
        p[2] = ngon->vtx[i + 1];
        tcDrawPoly(p, color);
    }
}

// Draws a TcMenu at text cell (x, y) with a blinking cursor; up/down move it (flag 8: trigger
// only, flag 4: no input), B jumps to the last entry with flag 1, flag 2 shows the cursor steady.
// Returns the entry on A (if enabled), else -1.
int tcMenuSelect(int x, int y, int flag, TcMenu* tbl, int num, s8* cursor)
{
    TcMenu* m = tbl;
    TcMenu* sel;
    int i;
    int c;
    int ret;

    if (!(flag & 4)) {
        if (flag & 8) {
            if (TC_TRG & (JOY_UP | JOY_DOWN)) tcMenuBlink = 8;
            if (TC_TRG & JOY_DOWN) (*cursor)++;
            if (TC_TRG & JOY_UP) (*cursor)--;
        } else {
            if (TC_REP & (JOY_UP | JOY_DOWN)) tcMenuBlink = 8;
            if (TC_REP & JOY_DOWN) (*cursor)++;
            if (TC_REP & JOY_UP) (*cursor)--;
        }
    }
    *cursor = *cursor < 0 ? num - 1 : (*cursor > num - 1 ? 0 : *cursor);
    if ((TC_TRG & JOY_B) && (flag & 1)) {
        tcMenuBlink = 8;
        *cursor = num - 1;
    }
    for (i = 0; i < num; i++) {
        int col = 0x14;
        if (m->enable) col = 0;
        eprintf(x, y + i * 14, col, 0, "%s", m->name);
        m++;
    }
    if ((tcMenuBlink & 0x18) || (flag & 2)) {
        eprintf(x - 8, y + *cursor * 14, 0, 0, ">");
    }
    tcMenuBlink++;
    ret = -1;
    c = *cursor;
    sel = &tbl[c];
    if ((TC_TRG & JOY_A) && sel->enable) ret = c;
    return ret;
}

static Vec tcCurveOld;

// Draws the current CamBSpline as 128 segments (position curve red, target curve blue).
void tcDrawParametricCurve()
{
    CameraBSpline* bs = &CamBSpline;
    Vec p;
    int i;
    int j;

    for (i = 0; i < 128; i++) {
        f32 t = (f32) ((bs->num - 1) * i) * (1.0f / 128.0f) + 0.0f;
        de_Boor_Cox(bs->num, NULL, t, bs->k, bs->basis);
        p.x = p.y = p.z = 0.0f;
        for (j = 0; j < bs->num; j++) {
            p.x += bs->basis[j] * bs->px[j];
            p.y += bs->basis[j] * bs->py[j];
            p.z += bs->basis[j] * bs->pz[j];
        }
        if (i > 0) tcDrawLine3D(&tcCurveOld, &p, 0xFF0000FE);
        tcCurveOld = p;
    }
    for (i = 0; i < 128; i++) {
        f32 t = (f32) ((bs->num - 1) * i) * (1.0f / 128.0f) + 0.0f;
        de_Boor_Cox(bs->num, NULL, t, bs->k, bs->basis);
        p.x = p.y = p.z = 0.0f;
        for (j = 0; j < bs->num; j++) {
            p.x += bs->basis[j] * bs->ax[j];
            p.y += bs->basis[j] * bs->ay[j];
            p.z += bs->basis[j] * bs->az[j];
        }
        if (i > 0) tcDrawLine3D(&tcCurveOld, &p, 0x0000FFFE);
        tcCurveOld = p;
    }
}
