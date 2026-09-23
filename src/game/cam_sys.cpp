// game/cam_sys.cpp: Camera orientation maths shared by every camera routine. A Camera holds pos /
// at / roll / fovy (param) and the derived orientation matrix `mat` (columns Right, up, Look =
// pos - at). CameraSetOrientation* rebuild the matrix from the parameters; the Rot / Dolly /
// Distance helpers move pos or at and rebuild.

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "db_log.h"
#include "math_sub.h"

// Rebuilds mat from pos / at keeping the current `up`: Look = pos - at, Right = up x Look, up
// re-orthogonalised.
void CameraSetOrientationUp(Camera* pCam)
{
    PSVECSubtract(&pCam->param.pos, &pCam->param.at, &pCam->Look);
#line 33 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&pCam->Look, &pCam->Look);
    PSVECCrossProduct(&pCam->Up, &pCam->Look, &pCam->Right);
#line 37 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&pCam->Right, &pCam->Right);
    PSVECCrossProduct(&pCam->Look, &pCam->Right, &pCam->Up);
    MTXSetColumns(pCam->mat, pCam->Right, pCam->Up, pCam->Look, pCam->param.pos);
}

// Rebuilds mat from pos / at with world up, then rolls right / up about the look axis by
// param.roll; stores up / Look / Right. A vertical look direction keeps the old right vector.
void CameraSetOrientationRoll(Camera* pCam)
{
    Vec right;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec dir;
    Mtx m;

    PSVECSubtract(&pCam->param.pos, &pCam->param.at, &dir);
    if (dir.x != 0.0f || dir.z != 0.0f) {
        PSVECCrossProduct(&up, &dir, &right);
    } else {
        getColumn(pCam->mat, 0, &right);
        if (PSVECMag(&right) == 0.0f) {
            right.x = 1.0f;
            right.y = 0.0f;
            right.z = 0.0f;
        }
    }
    PSVECCrossProduct(&dir, &right, &up);
    PSMTXRotAxisRad(m, &dir, pCam->param.roll);
    PSMTXMultVec(m, &right, &right);
    PSMTXMultVec(m, &up, &up);
    if (right.x != 0.0f || right.y != 0.0f || right.z != 0.0f) {
#line 91 "D:/Bio4/Prog/cam_sys.cpp"
        VECNormalize(&right, &right);
    }
    if (up.x != 0.0f || up.y != 0.0f || up.z != 0.0f) {
#line 92 "D:/Bio4/Prog/cam_sys.cpp"
        VECNormalize(&up, &up);
    }
    if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) {
#line 93 "D:/Bio4/Prog/cam_sys.cpp"
        VECNormalize(&dir, &dir);
    }
    MTXSetColumns(pCam->mat, right, up, dir, pCam->param.pos);
    pCam->Up = up;
    pCam->Look = dir;
    pCam->Right = right;
}

// Rebuilds mat from pos / at with world up and no roll.
void CameraSetOrientationZeroRoll(Camera* pCam)
{
    Vec right;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec dir;

    PSVECSubtract(&pCam->param.pos, &pCam->param.at, &dir);
    if (dir.x != 0.0f || dir.z != 0.0f) {
        PSVECCrossProduct(&up, &dir, &right);
    } else {
        getColumn(pCam->mat, 0, &right);
        if (PSVECMag(&right) == 0.0f) {
            right.x = 1.0f;
            right.y = 0.0f;
            right.z = 0.0f;
        }
    }
    PSVECCrossProduct(&dir, &right, &up);
#line 149 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&right, &right);
#line 150 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&up, &up);
#line 151 "D:/Bio4/Prog/cam_sys.cpp"
    VECNormalize(&dir, &dir);
    MTXSetColumns(pCam->mat, right, up, dir, pCam->param.pos);
    pCam->Up = up;
    pCam->Look = dir;
    pCam->Right = right;
}

// The camera's roll angle (radians): the camera's right vector expressed in the zero-roll frame.
f32 CameraGetRoll(Camera* pCam)
{
    Vec v = {1.0f, 0.0f, 0.0f};
    Camera tmp;
    Mtx inv;

    tmp = *pCam;
    CameraSetOrientationZeroRoll(&tmp);
    PSMTXInverse(tmp.mat, inv);
    PSMTXMultVecSR(inv, &v, &v);
    PSMTXMultVecSR(pCam->mat, &v, &v);
    return atan2f(v.y, v.x);
}

// Rotates the camera (pos, at, up) by `rad` about the axis through `pos`, then rebuilds the
// matrix and recomputes param.roll.
void CameraRotAxisPosRad(Camera* cam, Vec* axis, Vec* pos, f32 rad)
{
    Mtx m;

    MtxRotAxisPosRad(m, axis, pos, rad);
    PSMTXMultVec(m, &cam->param.at, &cam->param.at);
    PSMTXMultVec(m, &cam->param.pos, &cam->param.pos);
    PSMTXMultVecSR(m, &cam->Up, &cam->Up);
    CameraSetOrientationUp(cam);
    cam->param.roll = CameraGetRoll(cam);
}

// Rotates the target around the camera position about the camera's own X / Y / Z axis (look
// around).
void CameraTargetRot(Camera* pCam, char axis, f32 rad)
{
    Vec v;

    switch (axis) {
    case 'X':
    case 'x':
        getColumn(pCam->mat, 0, &v);
        break;
    case 'Y':
    case 'y':
        getColumn(pCam->mat, 1, &v);
        break;
    case 'Z':
    case 'z':
        getColumn(pCam->mat, 2, &v);
        break;
    }
    CameraRotAxisPosRad(pCam, &v, &pCam->param.pos, rad);
}

// Rotates the camera position around the target about the camera's own X / Y / Z axis (orbit).
void CameraCamposRot(Camera* pCam, char axis, f32 rad)
{
    Vec v;

    switch (axis) {
    case 'X':
    case 'x':
        getColumn(pCam->mat, 0, &v);
        break;
    case 'Y':
    case 'y':
        getColumn(pCam->mat, 1, &v);
        break;
    case 'Z':
    case 'z':
        getColumn(pCam->mat, 2, &v);
        break;
    }
    CameraRotAxisPosRad(pCam, &v, &pCam->param.at, rad);
}

// Translates pos and at by `speed`.
void CameraDolly(Camera* pCam, Vec* speed)
{
    PSVECAdd(&pCam->param.pos, speed, &pCam->param.pos);
    PSVECAdd(&pCam->param.at, speed, &pCam->param.at);
    CameraSetOrientationUp(pCam);
}

// Moves the target to `dist` in front of the camera along the look axis.
void CameraTargetDistance(Camera* pCam, f32 distance)
{
    Vec v;

    getColumn(pCam->mat, 2, &v);
    PSVECScale(&v, &v, distance);
    PSVECSubtract(&pCam->param.pos, &v, &pCam->param.at);
    pCam->Distance = distance;
    CameraSetOrientationUp(pCam);
}

// Moves the camera to `dist` behind the target along the look axis.
void CameraCamposDistance(Camera* pCam, f32 distance)
{
    Vec v;

    getColumn(pCam->mat, 2, &v);
    PSVECScale(&v, &v, distance);
    PSVECAdd(&pCam->param.at, &v, &pCam->param.pos);
    pCam->Distance = distance;
    CameraSetOrientationUp(pCam);
}

// Sets all four parameters and rebuilds the orientation with roll.
void CameraSetWithRoll(Camera* pCam, Vec* campos, Vec* target, f32 roll, f32 fovy)
{
    pCam->param.pos = *campos;
    pCam->param.at = *target;
    pCam->param.roll = roll;
    pCam->param.fovy = fovy;
    CameraSetOrientationRoll(pCam);
}

// Dead-stripped by the original linker (STRIP_UNUSED); only its DF 0.0 pool remains in .rodata.
static int cam_sys_unused(f64 x)
{
    return x != 0.0;
}
