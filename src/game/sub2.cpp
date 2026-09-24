// game/sub2: small vector / angle helpers used everywhere: yaw between points (GetXZAngle*), the
// clamped turning steps (Muku / Muku2 / Muku3), front cones (Front_check), distances, rotations,
// screen projection (GetScreenPos / Get3DPosFrom2D), line-sphere tests and the parabola / stop
// distance helpers of the throwing and movement code. (D:/Bio4/Prog/sub2.cpp)
#include "types.h"
#include "global.h"
#include "atari.h"
#include "model.h"
#include "player.h"
#include "camera.h"
#include "math_sub.h"
#include "gx.h"
#include "db_log.h"

#line 30 "D:/Bio4/Prog/sub2.cpp"

// 1 when the XZ point `p` lies inside the convex quad (4 corners in order).
int HitCheckPoint4(Vec* pos, Vec* xz)
{
    f32 px = pos->x - xz[0].x;
    f32 pz = pos->z - xz[0].z;
    f32 ax = xz[1].x - xz[0].x;
    f32 az = xz[1].z - xz[0].z;
    f32 bx = xz[3].x - xz[0].x;
    f32 bz = xz[3].z - xz[0].z;
    f32 cx;
    f32 cz;

    if (ax * pz > az * px || bx * pz < bz * px) {
        return 0;
    }
    cx = xz[2].x - xz[0].x;
    cz = xz[2].z - xz[0].z;
    px -= cx;
    pz -= cz;
    ax -= cx;
    az -= cz;
    bx -= cx;
    bz -= cz;
    if (ax * pz < az * px || bx * pz > bz * px) {
        return 0;
    }
    return 1;
}

// Yaw (radians, -PI..PI) from `from` to `to` on the XZ plane (0 = +Z).
f32 GetXZAngle(Vec* v0, Vec* v1)
{
    return LIMIT_ANGLE(atan2f(v1->x - v0->x, v1->z - v0->z));
}

// Angle from `from` to `to` in the XY plane.
f32 GetXYAngle(Vec* v0, Vec* v1)
{
    f32 dx = v1->x - v0->x;
    f32 dy = v1->y - v0->y;
    return LIMIT_ANGLE(atan2f(dy, dx));
}

// Yaw to `to` relative to a facing `ang` (0 = straight ahead, wrapped to -PI..PI).
f32 GetXZAngleLocal(Vec* v0, Vec* v1, f32 v0_dir)
{
    return LIMIT_ANGLE(GetXZAngle(v0, v1) - v0_dir);
}

// Squared distance between two points.
f32 GetDistance(Vec* v0, Vec* v1)
{
    f32 x = v1->x - v0->x;
    f32 y = v1->y - v0->y;
    f32 z = v1->z - v0->z;
    return x * x + y * y + z * z;
}

// Squared distance between two points.
f32 GetDistance(Vec& v0, Vec& v1)
{
    f32 x = v1.x - v0.x;
    f32 y = v1.y - v0.y;
    f32 z = v1.z - v0.z;
    return x * x + y * y + z * z;
}

// Squared XZ distance.
f32 GetDistanceXZ(Vec* v0, Vec* v1)
{
    f32 x = v1->x - v0->x;
    f32 z = v1->z - v0->z;
    return x * x + z * z;
}

// Turn amount toward `target` from facing `ang`: the signed yaw difference clamped to +-limit
// (the usual `ang.y += Muku(...)` turning step).
f32 Muku(Vec* v0, Vec* v1, f32 dir, f32 dy)
{
    f32 d = LIMIT_ANGLE(GetXZAngle(v0, v1) - dir);
    f32 ret;

    if (d > 0.0f) {
        if (d < dy) {
            dy = d;
        }
    } else {
        if (d > -dy) {
            dy = -d;
        }
    }
    ret = dy;
    if (!(d >= 0.0f)) {
        ret = -ret;
    }
    return ret;
}

// Turn amount from angle `ang` to `target` the short way round, clamped to +-limit.
f32 Muku2(f32 src_dir, f32 dst_dir, f32 add)
{
    f32 d = dst_dir - src_dir;

    if (d < 0.0f) {
        d += PI2;
    }
    if (d < PI) {
        if (add > d) {
            add = d;
        }
    } else {
        d -= PI2;
        if (add > -d) {
            add = d;
        } else {
            add = -add;
        }
    }
    return add;
}

// Turn amount from `ang` toward the direction vector `dir`, clamped to +-limit.
f32 Muku3(f32 src_dir, Vec* v0, f32 dy)
{
    return Muku2(src_dir, (f32) atan2(v0->x, v0->z), dy);
}

// Dead-stripped in the original (STRIP_UNUSED): only the VECNormalize strings and the 0.0f pools
// survive in .rodata, at this position.
static void sub2_dead1(Vec* v)
{
    VECNormalize(v, v);
}

// Dead-stripped: sign test.
static int sub2_dead2(f32 x)
{
    if (x > 0.0f) {
        return 1;
    }
    return 0;
}

// 1 when `b` is within +-ang of `a`'s facing.
int Front_check(cModel* a, cModel* b, f32 ang)
{
    f32 d = GetXZAngleLocal(&a->pos, &b->pos, a->ang.y);
    int ret = 0;
    if (!(d < -ang) && !(d > ang)) {
        ret = 1;
    }
    return ret;
}

// 1 when point `b` is within +-ang of `a`'s facing.
int Front_check(cModel* a, Vec* b, f32 ang)
{
    f32 d = GetXZAngleLocal(&a->pos, b, a->ang.y);
    int ret = 0;
    if (!(d < -ang) && !(d > ang)) {
        ret = 1;
    }
    return ret;
}

// 1 when `b` is within +-ang of the facing `rot` at `a`.
int Front_check(Vec* a, Vec* b, f32 rot, f32 ang)
{
    f32 d = GetXZAngleLocal(a, b, rot);
    int ret = 0;
    if (!(d < -ang) && !(d > ang)) {
        ret = 1;
    }
    return ret;
}

// Moves `m` by `speed` given in its own (rotated) frame.
void AddSpeed(cModel* pEm, const Vec* speed)
{
    Vec v;
    Mtx mtx;

    low_RotMatrix(mtx, &pEm->ang);
    PSMTXMultVec(mtx, speed, &v);
    pEm->pos.x += v.x;
    pEm->pos.y += v.y;
    pEm->pos.z += v.z;
}

// Length of a vector.
f32 RootSumSquare3(Vec* v)
{
    return SQRTF(v->x * v->x + v->y * v->y + v->z * v->z);
}

// Distance between two points.
f32 GetDistance3(Vec* v0, Vec* v1)
{
    Vec d;

    d.x = v1->x - v0->x;
    d.y = v1->y - v0->y;
    d.z = v1->z - v0->z;
    return RootSumSquare3(&d);
}

// Rotates `src` by the Euler angles `rot` (RotMatrix order).
void RotVector(Vec* vec0, Vec* ang, Vec* vec_ans)
{
    Mtx mtx;

    low_RotMatrix(mtx, ang);
    PSMTXMultVec(mtx, vec0, vec_ans);
}

// Transforms the 8 corners of a box by rotation `rot` and translation `pos`.
void BoxWorldCalc(Vec* BoxSrc, Vec* BoxDst, Vec* pos, Vec* ang)
{
    Mtx mtx;
    int i;

    RotMatrix(mtx, ang);
    TransMatrix(mtx, pos);
    for (i = 0; i < 8; i++) {
        PSMTXMultVec(mtx, &BoxSrc[i], &BoxDst[i]);
    }
}

// Projects a world point to screen coordinates with the current camera; returns 1 when it is in
// front of the camera.
int GetScreenPos(Vec* pos, Vec* scr)
{
    f32 proj[7];
    f32 vp[6];
    Vec cam;

    CameraCurrentProjection();
    GXGetProjectionv(proj);
    GXGetViewportv(vp);
    GXProject(pos->x, pos->y, pos->z, pG->Camera.v_mat, proj, vp, &scr->x, &scr->y, &scr->z);
    PSMTXMultVec(pG->Camera.v_mat, pos, &cam);
    return cam.z < -0.0f;
}

// World point under screen position (sx, sy): the camera ray meets height `y` (y == 1e8: the
// scroll collision hit, else the player's height when none), or the far point 20000 away.
void Get3DPosFrom2D(Vec* pPos3d, f32 sx, f32 sy, f32 h)
{
    CAMERA* cam = &pG->Camera;
    Vec far;
    Vec dir;
    Vec hit;

    CamPos2ScrnVec(sx, sy, &dir);
#line 518 "D:/Bio4/Prog/sub2.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 20000.0f);
    PSVECAdd(&cam->param.pos, &dir, &far);
    if (h == 100000000.0f) {
        if (SatMgr.hitCheck(&cam->param.pos, &far, &hit, 0, 0x40, 0)) {
            *pPos3d = hit;
            return;
        }
        h = pPL->pos.y;
    }
    {
        f32 t = (h - cam->param.pos.y) / (far.y - cam->param.pos.y);
        if (t > 0.0f) {
            PSVECScale(&dir, &dir, t);
            PSVECAdd(&cam->param.pos, &dir, pPos3d);
        } else {
            *pPos3d = far;
        }
    }
}

// Dead-stripped in the original (pools: PI/2, -1, 1, 127 and -1, 1, 0.5, 0).
static s8 sub2_dead3(f32 ang)
{
    f32 v = ang / 1.5707964f;
    if (v < -1.0f) {
        v = -1.0f;
    }
    if (v > 1.0f) {
        v = 1.0f;
    }
    return (s8) (v * 127.0f);
}

// Dead-stripped: clamp to -1..1 and halve.
static f32 sub2_dead4(f32 x)
{
    if (x < -1.0f) {
        x = -1.0f;
    }
    if (x > 1.0f) {
        x = 1.0f;
    }
    x *= 0.5f;
    if (x == 0.0f) {
        return 0.0f;
    }
    return x;
}

// Linear interpolation: out = pos1 * (1 - t) + pos2 * t.
void PosToPos(Vec* pos1, Vec* pos2, Vec* pos3, f32 per)
{
    Vec ta;
    Vec tb;

    PSVECScale(pos1, &ta, 1.0f - per);
    PSVECScale(pos2, &tb, per);
    PSVECAdd(&ta, &tb, pos3);
}

// A 2D stick vector (x right, y forward) turned into a world XZ direction relative to the camera
// yaw.
void VecToCamVec(Vec* v1, Vec* v2)
{
    Mtx mtx;
    Vec t;
    f32 ang;

    t.x = 0.0f;
    t.y = 0.0f;
    t.z = 1.0f;
    PSMTXMultVecSR(pG->Camera.mat, &t, &t);
    ang = atan2f(t.x, t.z);
    t.x = v1->x;
    t.y = 0.0f;
    t.z = -v1->y;
    PSMTXRotRad(mtx, 'y', ang);
    PSMTXMultVecSR(mtx, &t, v2);
}

// Does the segment a-b enter the sphere (c, r)? Returns 1 with the entry point in `out` (a itself
// when it starts inside).
int LineSphereCrossCk(Vec* a, Vec* b, Vec* c, f32 r, Vec* pCross)
{
    Vec ab;
    Vec ac;
    Vec p;
    Vec q;
    Vec d1;
    Vec d2;
    f32 r2 = r * r;
    f32 dist2;
    f32 len2;
    f32 t;
    f32 h;

    dist2 = (a->x - c->x) * (a->x - c->x) + (a->y - c->y) * (a->y - c->y) + (a->z - c->z) * (a->z - c->z);
    if (dist2 < r2) {
        if (pCross) {
            *pCross = *a;
        }
        return 1;
    }
    len2 = (a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z);
    if (len2 <= 0.1f) {
        return 0;
    }
    PSVECSubtract(b, a, &ab);
    PSVECSubtract(c, a, &ac);
    t = PSVECDotProduct(&ab, &ac) / len2;
    PSVECScale(&ab, &p, t);
    PSVECAdd(&p, a, &p);
    t = (p.x - c->x) * (p.x - c->x) + (p.y - c->y) * (p.y - c->y) + (p.z - c->z) * (p.z - c->z);
    if (t >= r2) {
        return 0;
    }
    h = SQRTF(r2 - t);
    PSVECSubtract(a, &p, &q);
    PSVECScale(&q, &q, h / PSVECMag(&q));
    PSVECAdd(&q, &p, &q);
    dist2 = (a->x - q.x) * (a->x - q.x) + (a->y - q.y) * (a->y - q.y) + (a->z - q.z) * (a->z - q.z);
    if (dist2 > len2) {
        return 0;
    }
    PSVECSubtract(a, &q, &d1);
    PSVECSubtract(a, b, &d2);
    if (PSVECDotProduct(&d1, &d2) < 0.0f) {
        return 0;
    }
    PSVECSubtract(b, &q, &d1);
    PSVECSubtract(b, a, &d2);
    if (PSVECDotProduct(&d1, &d2) < 0.0f) {
        return 0;
    }
    if (pCross) {
        *pCross = q;
    }
    return 1;
}

// 1 when two spheres overlap.
int SphereHitCk(Vec* pPos1, Vec* pPos2, f32 radius1, f32 radius2)
{
    Vec d;
    f32 r;

    PSVECSubtract(pPos1, pPos2, &d);
    r = radius1 + radius2;
    return d.x * d.x + d.y * d.y + d.z * d.z < r * r;
}

// Launch velocity (units / frame, gravity 20) that carries a body from `from` to `to` peaking `h`
// above the higher end; h <= 0 gives the straight difference.
void CalcParabolaVector(Vec* spd, Vec* src, Vec* dst, f32 height)
{
    f32 g = 20.0f;
    f32 t;

    if (height <= 0.0f) {
        PSVECSubtract(dst, src, spd);
        return;
    }
    if (src->y > dst->y) {
        height += src->y;
    } else {
        height += dst->y;
    }
    t = SQRTF((height - src->y) * 2.0f / 20.0f);
    t += SQRTF((height - dst->y) * 2.0f / 20.0f);
    PSVECSubtract(dst, src, spd);
    PSVECScale(spd, spd, 1.0f / t);
    spd->y = SQRTF((height - src->y) * 40.0f);
}

// Distance covered while `speed` decelerates by `decel` per frame to 0.
f32 CalcStopDist(f32 v0, f32 a)
{
    f32 d = 0.0f;

    do {
        d += v0;
        v0 -= a;
    } while (!(v0 <= 0.0f));
    return d;
}

// Moves `pos` `dist` units toward `target`; returns 1 (and snaps) when it arrives.
int CalcMovePosDist(Vec* pPos, Vec* pTar, f32 dist)
{
    Vec dir;

    if (GetDistance(pPos, pTar) <= dist * dist) {
        *pPos = *pTar;
        return 1;
    }
    PSVECSubtract(pTar, pPos, &dir);
#line 908 "D:/Bio4/Prog/sub2.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, dist);
    PSVECAdd(pPos, &dir, pPos);
    return 0;
}

// Dead-stripped in the original (a double 0.0 pool).
static int sub2_dead5(f64 x)
{
    if (x != 0.0) {
        return 1;
    }
    return 0;
}
