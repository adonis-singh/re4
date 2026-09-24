// game/camera.cpp: the game camera front end. pG->Camera is the Camera used for rendering; CameraMove
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
#include <string.h>
#include <math.h>
#include "math_sub_decl.h"

int ProjType = 1;

// Loads pG->Camera's projection into GX: type 1 perspective, type 2 orthographic (ProjType
// remembers it for CameraCurrentProjection).
void CameraSetProjection(int projType)
{
    ProjType = projType;
    switch (projType) {
    case 1:
        GXSetProjection(pG->Camera.ProjMat, 0);
        break;
    case 2:
        GXSetProjection(pG->Camera.ProjMat, 1);
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

    CameraSetWithRoll(&pG->Camera, &pos, &at, 0.0f, 50.0f);
    CameraSetOrientationRoll(&pG->Camera);
    ProjType = 1;
    CameraRoomInit();
}

// Room start: resets the debug camera move gain.
void CameraRoomInit()
{
    CamDbg.m_move_gain = 1.0f;
}

// Per-frame camera update (game loop): CamCtrl.Check / Move produce the frame's camera, copied
// into pG->Camera when the camera is live (Status_flg[0] 0x100) and not overridden by the debug
// camera (Debug_flg[0] 0x10000000; an extra camera pointer wins), then the quake offset (unless
// Stop_flg 0x10000), the debug camera pad handling, projection (fovy 0 is an error -> 50), dist,
// the look-at matrix, the view frustum and the camera debug text. Stop_flg 0x40000000 freezes
// the controller.
void CameraMove()
{
    Camera* cam = &pG->Camera;

    CamCtrl.Check();
    if (!SpfFlagChk(pG, SPF_CAMERA)) {
        CamCtrl.Move();
        if (StaFlagChk(pG, STA_CAMERA) && !DbgFlagChk(pG, DBG_DBG_CAM)) {
            pG->Camera = CamCtrl.camera;
            if (CamCtrl.m_pExtraCamera != 0) {
                pG->Camera = *CamCtrl.m_pExtraCamera;
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
    cam->Distance = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->v_mat, &cam->param.pos, &cam->Up, &cam->param.at);
    View.move();
    CameraDebugInformation();
}

// Analog stick as a world-space move direction: rotated by the camera matrix, or by the previous
// camera's matrix while the stick is held through a camera cut (so the run direction does not
// flip on a cut).
void CamStick2World(Camera* pCam, JOY* pJoy, Vec* pVec)
{
    static Mtx mat_prev;
    static int carry_on_flag = 0;
    Vec v;

    v.x = (f32) pJoy->stickX;
    v.y = 0.0f;
    v.z = (f32) -pJoy->stickY;
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
        PSMTXMultVecSR(mat_prev, &v, pVec);
    } else {
        PSMTXMultVecSR(pCam->mat, &v, pVec);
    }
}

// The world-space view frustum of the current camera (View.worldFull).
ViewFrustum* CameraViewFrustumPtr(Camera* pCam)
{
    return &View.worldFull;
}

// The camera's up vector.
void CameraGetUpVec(Camera* pCam, Vec* up)
{
    *up = pCam->Up;
}

// The camera's look vector (pos - at, normalised: points backwards).
void CameraGetLookVec(Camera* pCam, Vec* look)
{
    *look = pCam->Look;
}

// The forward view direction (-Look).
void CameraGetLookVecInverse(Camera* pCam, Vec* look_inv)
{
    look_inv->x = -pCam->Look.x;
    look_inv->y = -pCam->Look.y;
    look_inv->z = -pCam->Look.z;
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
void CamPos2ScrnVec(f32 sX, f32 sY, Vec* vec)
{
    f32 ang = pG->Camera.param.fovy;
    f32 h = 480.0f;  // first constant of the pool

    vec->x = sX - Screen.width * 0.5f;
    vec->y = -(sY - Screen.height * 0.5f);
    vec->x *= 640.0f / Screen.width;
    ang = ang * 0.5f;
    ang = ang * PI;
    ang = ang / 180.0f;
    vec->y *= h / Screen.height;
    vec->z = -(cosf(ang) * 240.0f / sinf(ang));
    PSMTXMultVecSR(pG->Camera.mat, vec, vec);
}
