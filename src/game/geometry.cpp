// game/geometry: primitive intersection tests (D:/Bio4/Prog/geometry.cpp): point vs cone
// (the flashlight / spotlight volume used by the light system) and sphere vs hexahedron (the
// view frustum test of trans_ot). Several inline helpers only survive as their string/constant pools.
#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "math_sub.h"
#include "geometry.h"

#line 100 "D:/Bio4/Prog/geometry.cpp"

// The functions below are never called in this build. GCC 2.95 still emits the string literals
// and the initializer templates of the local aggregates of inline functions at parse time, and the
// original geometry.o carries exactly these bytes in .rodata around the constant pools. The
// grouping and bodies are a guess that reproduces the bytes.
static inline const char* geo_name()
{
    return "";
}

// Unit vector perpendicular to the cone direction and world up.
// Never called: the original linker dropped the body but kept its constant pool (one 0.0f, the
// VECNormalize strings and the {0,1,0} template are shared with collision_point_cone_rev_play).
static void collision_cone_axis(GeoCone* cone, Vec* axis)
{
    Vec up = {0.0f, 1.0f, 0.0f};

    PSVECCrossProduct(&up, &cone->direction, axis);
    VECNormalize(axis, axis);
}
#line 220

// 1 when point p lies inside the cone (apex at cone->pos, axis direction, height, half angle in
// radians) widened by `margin`; also stores the base radius in cone->radius.
int collision_point_cone_rev_play(Vec* pPoint, GeoCone* pConeRev, f32 play)
{
    int ret = 0;
    Vec axis;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec c;
    Mtx m;
    Mtx inv;
    Vec lp;
    f32 r;
    f32 t;

    if (VecAngle(&pConeRev->direction, &up) != 0.0f) {
        PSVECCrossProduct(&up, &pConeRev->direction, &axis);
#line 245
        VECNormalize(&axis, &axis);
        VECNormalize(&pConeRev->direction, &up);
        PSVECCrossProduct(&axis, &up, &c);
#line 248
        VECNormalize(&c, &c);
        MTXSetColumns(m, axis, up, c, pConeRev->pos);
    } else {
        PSMTXTrans(m, pConeRev->pos.x, pConeRev->pos.y, pConeRev->pos.z);
    }
    PSMTXInverse(m, inv);
    PSMTXMultVec(inv, pPoint, &lp);
    if (!(lp.y < 0.0f) && !(lp.y > pConeRev->height)) {
        r = pConeRev->height * sinf(pConeRev->angle);
        pConeRev->radius = r;
        t = lp.y * r / pConeRev->height + play;
        if (lp.x * lp.x + lp.z * lp.z < t * t) {
            ret = 1;
        }
    }
    return ret;
}

// Cone test plus a facing test: the surface normal `face` must point back towards the cone axis
// within `angle` radians.
int collision_point_cone_rev_play_face(Vec* pPoint, GeoCone* pConeRev, f32 play, Vec* pDirection, f32 open_angle)
{
    Vec v;
    int ret = collision_point_cone_rev_play(pPoint, pConeRev, play);

    if (ret) {
        PSVECScale(pDirection, &v, -1.0f);
        if (VecAngle(&pConeRev->direction, &v) < open_angle) {
            ret = 1;
        } else {
            ret = 0;
        }
    }
    return ret;
}

#line 300
static inline int collision_point_check(Vec* p)
{
    Vec lim = {0.01f, 0.5f, 0.0f};
    return p->x < lim.x && p->y < lim.y && p->z < lim.z;
}

// 1 when the sphere touches the convex hexahedron given by six outward normals (three through
// pointA, three through pointB); used for frustum culling.
int collision_sphere_hexahedron(GeoSphere* pSphere, GeoHexahedron* pHexahedron)
{
    int ret = 1;
    u32 i;
    Vec d;

    PSVECSubtract(&pSphere->pos, &pHexahedron->pointA, &d);
    for (i = 0; i <= 5; i++) {
        if (i == 3) {
            PSVECSubtract(&pSphere->pos, &pHexahedron->pointB, &d);
        }
        if (PSVECDotProduct(&d, &pHexahedron->normal[i]) > pSphere->r + 0.01f) {
            ret = 0;
            break;
        }
    }
    return ret;
}

#line 350
static inline int collision_fanpole_check(GeoCone* cone)
{
    Vec a = {1.0f, 0.0f, 0.5f};
    f32 b[4] = {0.0f, 0.5f, -0.5f, 0.0f};
    if (cone->direction.x != a.y || cone->direction.z != b[3]) {
        pLog->err(0, 0, "Fanpole is not vertical to the ground!\n");
        return 0;
    }
    return a.x != b[1];
}

static inline int collision_cylinder_check(GeoCone* cone)
{
    if (cone->direction.x != 0.0f || cone->direction.z != 0.0f) {
        pLog->err(0, 0, "Cylinder is not vertical to the ground!\n");
        return 0;
    }
    return 1;
}

static inline f32 collision_range_check(Vec* p)
{
    f32 rng[5] = {0.0f, 0.5f, 1.0f, -0.5f, 0.0f};
    return rng[0] + rng[1] * p->x + rng[2] * p->y + rng[3] * p->z + rng[4];
}
