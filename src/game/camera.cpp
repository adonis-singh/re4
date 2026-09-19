// game/camera.cpp: the game camera front end. pG->Cam is the Camera used for rendering; CameraMove
// (game loop) lets the camera controller (CamCtrl, cam_ctrl.cpp) compute the frame's camera,
// applies the quake offset and the debug camera, rebuilds the projection / view matrices and
// updates the view frustum. Also small helpers: stick direction in camera space, up / look
// vectors, screen point to world ray.

#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "view.h"
#include "db_cam.h"
#include "db_log.h"
#include "joy.h"
#include "quake.h"
#include "main_sub.h"
#include "gx.h"

extern "C" {
void* memset(void* dst, int c, unsigned int n);
f32 sinf(f32);
f32 cosf(f32);
}

extern f32 ORTHO_T;
extern f32 ORTHO_B;
extern f32 ORTHO_L;
extern f32 ORTHO_R;

#define PI 3.1415927f

// Matrix copy written out as loops (same as motion.cpp; the original never calls PSMTXCopy here).
#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        MtxPtr s_ = (src);               \
        int i_ = 3;                      \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        while (i_--) {                   \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

int ProjType = 1;

// Loads pG->Cam's projection into GX: type 1 perspective, type 2 orthographic (ProjType
// remembers it for CameraCurrentProjection).
void CameraSetProjection(int type)
{
    ProjType = type;
    switch (type) {
    case 1:
        GXSetProjection(pG->Cam.ProjMat, 0);
        break;
    case 2:
        GXSetProjection(pG->Cam.ProjMat, 1);
        break;
    }
}

// Re-loads the current projection (effects and debug draws restore it after their own).
void CameraCurrentProjection()
{
    CameraSetProjection(ProjType);
}

// The current projection type (1 perspective, 2 ortho).
int CameraGetProjection()
{
    return ProjType;
}

// Game start: a default camera (1000 up, 2000 back, fov 50), perspective projection, room init.
void CameraGameInit()
{
    Vec at = {0.0f, 0.0f, 0.0f};
    Vec pos = {0.0f, 1000.0f, 2000.0f};

    CameraSetWithRoll(&pG->Cam, &pos, &at, 0.0f, 50.0f);
    CameraSetOrientationRoll(&pG->Cam);
    ProjType = 1;
    CameraRoomInit();
}

// Room start: resets the debug camera move gain.
void CameraRoomInit()
{
    CamDbg.m_move_gain = 1.0f;
}

// Per-frame camera update (game loop): CamCtrl.Check / Move produce the frame's camera, copied
// into pG->Cam when the camera is live (Status_flg[0] 0x100) and not overridden by the debug
// camera (Debug_flg[0] 0x10000000; an extra camera pointer wins), then the quake offset (unless
// Stop_flg 0x10000), the debug camera pad handling, projection (fovy 0 is an error -> 50), dist,
// the look-at matrix, the view frustum and the camera debug text. Stop_flg 0x40000000 freezes
// the controller.
void CameraMove()
{
    Camera* cam = &pG->Cam;

    CamCtrl.Check();
    if (!SpfFlagChk(pG, SPF_CAMERA)) {
        CamCtrl.Move();
        if (StaFlagChk(pG, STA_CAMERA) && !DbgFlagChk(pG, DBG_DBG_CAM)) {
            pG->Cam = CamCtrl.camera;
            if (CamCtrl.m_pExtraCamera != 0) {
                pG->Cam = *(Camera*) CamCtrl.m_pExtraCamera;
            }
        }
        CamCtrl.m_pExtraCamera = 0;
        if (!SpfFlagChk(pG, SPF_EARTHQUAKE)) {
            QuakeMove();
        }
    }
    CamDbg.move(cam, &Joy[1], 0);
    if (cam->param.fovy == 0.0f) {
        pLog->err(0, 0, "CameraMove(): Fovy = 0.0f");
        cam->param.fovy = 50.0f;
    }
    switch (ProjType) {
    case 1:
        C_MTXPerspective(cam->ProjMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
        break;
    case 2:
        C_MTXOrtho(cam->ProjMat, ORTHO_T, ORTHO_B, ORTHO_L, ORTHO_R, 0.0f, ZFAR);
        break;
    }
    cam->dist = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->v_mat, &cam->param.pos, &cam->up, &cam->param.at);
    View.move();
    CameraDebugInformation();
}

// Analog stick as a world-space move direction: rotated by the camera matrix, or by the previous
// camera's matrix while the stick is held through a camera cut (so the run direction does not
// flip on a cut).
void CamStick2World(Camera* cam, JOY* joy, Vec* out)
{
    static Mtx mat_prev;
    static int carry_on_flag = 0;
    Vec v;

    v.x = (f32) joy->stickX;
    v.y = 0.0f;
    v.z = (f32) -joy->stickY;
    if (CamCtrl.IsChangeCamera()) {
        if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) {
            MTX_COPY(CamCtrl.prev_mat, mat_prev);
            carry_on_flag = 1;
        }
    }
    if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
        carry_on_flag = 0;
    }
    if (carry_on_flag) {
        PSMTXMultVecSR(mat_prev, &v, out);
    } else {
        PSMTXMultVecSR(cam->mat, &v, out);
    }
}

// The world-space view frustum of the current camera (View.worldFull).
ViewFrustum* CameraViewFrustumPtr(Camera* cam)
{
    return &View.worldFull;
}

// The camera's up vector.
void CameraGetUpVec(Camera* cam, Vec* up)
{
    *up = cam->up;
}

// The camera's look vector (pos - at, normalised: points backwards).
void CameraGetLookVec(Camera* cam, Vec* look)
{
    *look = cam->Look;
}

// The forward view direction (-Look).
void CameraGetLookVecInverse(Camera* cam, Vec* look)
{
    look->x = -cam->Look.x;
    look->y = -cam->Look.y;
    look->z = -cam->Look.z;
}

// Never called; dead-stripped from the DOL. Its constant pool (0.0f, the int->float magic
// double, -1.0f) is still in .rodata right before CamPos2ScrnVec's.
static f32 ScrnY2Ratio(int y)
{
    f32 r = 0.0f;

    if (y != 0) {
        r = (f32) y + -1.0f;
    }
    return r;
}

// World-space ray direction through screen pixel (sx, sy): the pixel offset from the screen
// centre in 640 x 480 units, z from the vertical fov, rotated by the camera matrix (aiming /
// picking).
void CamPos2ScrnVec(Vec* out, f32 sx, f32 sy)
{
    f32 ang = pG->Cam.param.fovy;
    f32 h = 480.0f;  // first constant of the pool

    out->x = sx - Screen.width * 0.5f;
    out->y = -(sy - Screen.height * 0.5f);
    out->x *= 640.0f / Screen.width;
    ang = ang * 0.5f;
    ang = ang * PI;
    ang = ang / 180.0f;
    out->y *= h / Screen.height;
    FSet(out->z, -(cosf(ang) * 240.0f / sinf(ang)));
    PSMTXMultVecSR(pG->Cam.mat, out, out);
}
