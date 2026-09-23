// game/obj09: object id 9, the rigid-body effect model Efm09 (D:/Bio4/Prog/obj09.cpp): crates,
// barrels and debris spawned by effect records (esp_efm.cpp EfmSetObj09) as boxes with mass and
// moments of inertia. Each frame forces (gravity, corner spring/damper contacts with the scenario,
// body-body contacts, player push, water/sand drag) are accumulated with AddForce and integrated
// (Calc: velocity, RK2 angular velocity, orientation re-orthonormalised).
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "player.h"

extern "C" {
void AddForce(cObj* obj, Vec* point, Vec* force);
void dwdt(Vec* w, Vec* t, Vec* moment, Vec* out);
void Calc(cObj* obj, f32 dt);
f32 lu(f32 a[][3], int* ip);
f32 LinerEquation3(f32 a[][3], f32* b, f32* x);
}

// Rigid body effect model (Efm09): a box with mass and moments of inertia, integrated with a
// second order Runge-Kutta step (CalcVel / Calc), colliding with the scenario at its eight
// corners (calcPointHit), with the other rigid bodies (Obj09HitCheck), the water, the sand and
// the player.
class cObj09 : public cObjUnion {
public:
    virtual void move();
};

f32 grav = 400.0f;
f32 sprg = 100.0f;
f32 spd_reg = 0.99f;
f32 rot_reg = 0.99f;
f32 dmp_ratio = -4.0f;
f32 hit_v_dmp_rate = 0.99f;
f32 hit_w_dmp_rate = 0.99f;
f32 MUE = 10000.0f;
static f32 max_in_dist = 50.0f;
f32 obj_max_dist = 30.0f;
f32 obj_max_ratio = 0.1f;
f32 obj_hosei_ratio = 0.0f;
f32 obj_move_pow = 5000.0f;

// The eight corners of the unit box.
Vec pos_tbl[8] = {
    { 0.5f, 0.5f, 0.5f },
    { 0.5f, 0.5f, -0.5f },
    { -0.5f, 0.5f, 0.5f },
    { -0.5f, 0.5f, -0.5f },
    { 0.5f, -0.5f, 0.5f },
    { 0.5f, -0.5f, -0.5f },
    { -0.5f, -0.5f, 0.5f },
    { -0.5f, -0.5f, -0.5f },
};

static cObj* pObj_ck;

// Apply `force` at world point `point`: the force and the torque about the centre accumulate.
void AddForce(cObj* pObj, Vec* pos, Vec* f)
{
    Efm09Work* w = &((cObj09*) pObj)->efm09;
    Vec t;
    Vec r;

    PSVECSubtract(pos, &w->basePos, &r);
    PSVECCrossProduct(&r, f, &t);
    PSVECAdd(&w->force, f, &w->force);
    PSVECAdd(&w->torque, &t, &w->torque);
}

// Euler's equations: angular acceleration from the angular velocity `w`, the torque `t` and the
// principal moments of inertia.
void dwdt(Vec* w, Vec* tq, Vec* I, Vec* pRet)
{
    pRet->x = (tq->x + (I->y - I->z) * w->y * w->z) / I->x;
    pRet->y = (tq->y + (I->z - I->x) * w->z * w->x) / I->y;
    pRet->z = (tq->z + (I->x - I->y) * w->x * w->y) / I->z;
}

// Integrates linear velocity (force/mass) and angular velocity (second-order Runge-Kutta on dwdt,
// capped at 16 rad/s) over dt, converts the angular velocity to world space (w) and clears the
// accumulators.
// Integrate the linear and angular velocities over `dt` and clear the accumulators.
static void CalcVel(cObj* pObj, f32 dt)
{
    Efm09Work* w = &((cObj09*) pObj)->efm09;
    Vec a;
    Vec lt;
    Mtx inv;
    Vec k1;
    Vec k2;
    Vec half;

    PSVECScale(&w->force, &a, dt / w->mass);
    PSVECAdd(&w->spd, &a, &w->spd);
    PSMTXInverse(w->mat, inv);
    PSMTXMultVec(inv, &w->torque, &lt);
    dwdt(&w->rotSpd, &lt, &w->moment, &k1);
    PSVECScale(&k1, &k1, dt);
    PSVECScale(&k1, &half, 0.5f);
    PSVECAdd(&w->rotSpd, &half, &half);
    dwdt(&half, &lt, &w->moment, &k2);
    PSVECScale(&k2, &k2, dt);
    PSVECAdd(&w->rotSpd, &k2, &w->rotSpd);
    if (PSVECMag(&w->rotSpd) > 16.0f) {
#line 196 "D:/Bio4/Prog/obj09.cpp"
        VECNormalize(&w->rotSpd, &w->rotSpd);
        PSVECScale(&w->rotSpd, &w->rotSpd, 16.0f);
    }
    PSMTXMultVec(w->mat, &w->rotSpd, &w->w);
    w->force.x = w->force.y = w->force.z = 0.0f;
    w->torque.x = w->torque.y = w->torque.z = 0.0f;
}

// One time step: velocities, position, then the rotation matrix (R += dt * omega x R), which
// is re-orthonormalised from its z axis.
void Calc(cObj* pObj, f32 dt)
{
    Efm09Work* w = &((cObj09*) pObj)->efm09;
    Vec v;
    Vec av;
    Mtx n;
    Mtx skew;
    Vec tmp;
    Vec tmp2;

    CalcVel(pObj, dt);
    PSVECScale(&w->spd, &v, dt);
    PSVECAdd(&w->basePos, &v, &w->basePos);
    PSVECScale(&w->w, &av, dt);
    skew[0][0] = 0.0f;
    skew[0][1] = -av.z;
    skew[0][2] = av.y;
    skew[0][3] = 0.0f;
    skew[1][0] = av.z;
    skew[1][1] = 0.0f;
    skew[1][2] = -av.x;
    skew[1][3] = 0.0f;
    skew[2][0] = -av.y;
    skew[2][1] = av.x;
    skew[2][2] = 0.0f;
    skew[2][3] = 0.0f;
    PSMTXConcat(skew, w->mat, skew);
    n[0][0] = w->mat[0][0] + skew[0][0];
    n[0][1] = w->mat[0][1] + skew[0][1];
    n[0][2] = w->mat[0][2] + skew[0][2];
    n[0][3] = w->mat[0][3] + skew[0][3];
    n[1][0] = w->mat[1][0] + skew[1][0];
    n[1][1] = w->mat[1][1] + skew[1][1];
    n[1][2] = w->mat[1][2] + skew[1][2];
    n[1][3] = w->mat[1][3] + skew[1][3];
    n[2][0] = w->mat[2][0] + skew[2][0];
    n[2][1] = w->mat[2][1] + skew[2][1];
    n[2][2] = w->mat[2][2] + skew[2][2];
    n[2][3] = w->mat[2][3] + skew[2][3];

    tmp.x = n[2][0];
    tmp.y = n[2][1];
    tmp.z = n[2][2];
#line 274 "D:/Bio4/Prog/obj09.cpp"
    VECNormalize(&tmp, &tmp);
    n[2][0] = tmp.x;
    n[2][1] = tmp.y;
    n[2][2] = tmp.z;
    tmp.x = n[1][0];
    tmp.y = n[1][1];
    tmp.z = n[1][2];
    tmp2.x = n[2][0];
    tmp2.y = n[2][1];
    tmp2.z = n[2][2];
    PSVECCrossProduct(&tmp, &tmp2, &tmp);
#line 286 "D:/Bio4/Prog/obj09.cpp"
    VECNormalize(&tmp, &tmp);
    n[0][0] = tmp.x;
    n[0][1] = tmp.y;
    n[0][2] = tmp.z;
    tmp.x = n[2][0];
    tmp.y = n[2][1];
    tmp.z = n[2][2];
    tmp2.x = n[0][0];
    tmp2.y = n[0][1];
    tmp2.z = n[0][2];
    PSVECCrossProduct(&tmp, &tmp2, &tmp);
    n[1][0] = tmp.x;
    n[1][1] = tmp.y;
    n[1][2] = tmp.z;
    {
        int i_ = 2;
        MtxPtr d_ = w->mat;
        MtxPtr s_ = n;
        int j_;
        f32* sp_;
        f32* dp_;
        do {
            dp_ = *d_;
            sp_ = *s_;
            for (j_ = 0; j_ < 4; j_++) {
                *dp_++ = *sp_++;
            }
            d_++;
            s_++;
        } while (--i_ != -1);
    }
}

// Rigid body against rigid body: every corner of the body being moved (pObj_ck) inside `obj`'s
// box gets a spring / damper force along the closest face normal; both bodies are pushed apart.
static void Obj09HitCheck(cObj* pObj)
{
    cObj* ck;
    Efm09Work* w1;
    Efm09Work* w2;
    Mtx m1;
    Mtx inv;
    Mtx m2;
    Vec p;
    Vec lp;
    Vec n;
    Vec wp;
    Vec nrm;
    Vec tmp;
    Vec vel;
    Vec vn;
    f32 maxDepth;
    f32 depth;
    f32 d;
    f32 mm;
    f32 ms;
    f32 mag1;
    f32 mag2;
    f32 dot;
    int i;

    if (pObj->id != 9) {
        return;
    }
    ck = pObj_ck;
    if (pObj == ck) {
        return;
    }
    PSMTXCopy(((cObj09*) pObj)->efm09.mat, m2);
    TransMatrix(m2, &((cObj09*) pObj)->efm09.basePos);
    w2 = &((cObj09*) pObj)->efm09;
    PSMTXInverse(m2, inv);
    PSMTXCopy(((cObj09*) ck)->efm09.mat, m1);
    TransMatrix(m1, &((cObj09*) ck)->efm09.basePos);
    w1 = &((cObj09*) ck)->efm09;
    maxDepth = 0.0f;
    n.x = n.y = n.z = 0.0f;
    for (i = 0; i < 8; i++) {
        lp.x = pos_tbl[i].x * w1->size.x;
        lp.y = pos_tbl[i].y * w1->size.y;
        lp.z = pos_tbl[i].z * w1->size.z;
        PSMTXMultVec(m1, &lp, &wp);
        PSMTXMultVec(m1, &n, &n);
        PSMTXMultVec(inv, &wp, &p);
        PSMTXMultVec(inv, &n, &n);
        if (p.x > -w2->size.x * 0.5f && p.x < w2->size.x * 0.5f && p.y > -w2->size.y * 0.5f &&
            p.y < w2->size.y * 0.5f && p.z > -w2->size.z * 0.5f && p.z < w2->size.z * 0.5f) {
            depth = 1.0e16f;
            if ((p.x + w2->size.x) * (n.x + w2->size.x) < 0.0f) {
                d = fabsf(p.x + w2->size.x);
                if (d < depth) {
                    depth = d;
                    nrm.x = -1.0f;
                    nrm.y = 0.0f;
                    nrm.z = 0.0f;
                }
            }
            if ((p.x - w2->size.x) * (n.x - w2->size.x) < 0.0f) {
                d = fabsf(p.x - w2->size.x);
                if (d < depth) {
                    depth = d;
                    nrm.x = 1.0f;
                    nrm.y = 0.0f;
                    nrm.z = 0.0f;
                }
            }
            if ((p.y + w2->size.y) * (n.y + w2->size.y) < 0.0f) {
                d = fabsf(p.y + w2->size.y);
                if (d < depth) {
                    depth = d;
                    nrm.x = 0.0f;
                    nrm.y = -1.0f;
                    nrm.z = 0.0f;
                }
            }
            if ((p.y - w2->size.y) * (n.y - w2->size.y) < 0.0f) {
                d = fabsf(p.y - w2->size.y);
                if (d < depth) {
                    depth = d;
                    nrm.x = 0.0f;
                    nrm.y = 1.0f;
                    nrm.z = 0.0f;
                }
            }
            if ((p.z + w2->size.z) * (n.z + w2->size.z) < 0.0f) {
                d = fabsf(p.z + w2->size.z);
                if (d < depth) {
                    depth = d;
                    nrm.x = 0.0f;
                    nrm.y = 0.0f;
                    nrm.z = -1.0f;
                }
            }
            if ((p.z - w2->size.z) * (n.z - w2->size.z) < 0.0f) {
                d = fabsf(p.z - w2->size.z);
                if (d < depth) {
                    depth = d;
                    nrm.x = 0.0f;
                    nrm.y = 0.0f;
                    nrm.z = 1.0f;
                }
            }
            if (depth == 1.0e16f) {
                nrm.x = 0.0f;
                nrm.y = 1.0f;
                nrm.z = 0.0f;
            }
            d = depth;
            if (maxDepth < d) {
                maxDepth = d;
            }
            mag1 = PSVECMag(&w2->size);
            mag2 = PSVECMag(&w1->size);
            if (mag1 < mag2) {
                ms = mag2;
            } else {
                ms = mag1;
            }
            if (d > obj_max_ratio * ms) {
                f32 m;

                PSVECSubtract(&w1->basePos, &w2->basePos, &nrm);
                if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
                    nrm.y = 1.0f;
                }
#line 464 "D:/Bio4/Prog/obj09.cpp"
                VECNormalize(&nrm, &nrm);
                if (w2->mass > w1->mass * 2.0f) {
                    m = w1->mass * 2.0f;
                } else if (w1->mass > w2->mass * 2.0f) {
                    m = w2->mass * 2.0f;
                } else {
                    m = w2->mass + w1->mass;
                }
                PSVECScale(&nrm, &tmp, obj_move_pow * m);
                AddForce(pObj_ck, &w1->basePos, &tmp);
                PSVECScale(&tmp, &tmp, -1.0f);
                AddForce(pObj, &w2->basePos, &tmp);
            }
            if (depth > obj_max_dist) {
                d = obj_max_dist;
            }
            if (w2->mass > w1->mass * 2.0f) {
                mm = w1->mass * 2.0f;
            } else if (w1->mass > w2->mass * 2.0f) {
                mm = w2->mass * 2.0f;
            } else {
                mm = w2->mass + w1->mass;
            }
            PSVECScale(&nrm, &tmp, d * (sprg * mm));
            AddForce(pObj_ck, &wp, &tmp);
            PSVECScale(&tmp, &tmp, -1.0f);
            AddForce(pObj, &wp, &tmp);
            PSVECCrossProduct(&w1->w, &lp, &vel);
            PSVECAdd(&w1->spd, &vel, &vel);
            dot = PSVECDotProduct(&nrm, &vel);
            PSVECScale(&nrm, &vn, dot);
            PSVECScale(&nrm, &tmp, dot * (dmp_ratio * mm));
            AddForce(pObj_ck, &wp, &tmp);
            PSVECScale(&tmp, &tmp, -1.0f);
            AddForce(pObj, &wp, &tmp);
            PSVECScale(&w2->spd, &w2->spd, hit_v_dmp_rate);
            PSVECScale(&w1->spd, &w1->spd, hit_v_dmp_rate);
            PSVECScale(&w2->rotSpd, &w2->rotSpd, hit_w_dmp_rate);
            PSVECScale(&w1->rotSpd, &w1->rotSpd, hit_w_dmp_rate);
        }
    }
    if (maxDepth != 0.0f) {
        PSVECScale(&nrm, &vel, maxDepth * obj_hosei_ratio);
        PSVECAdd(&w1->basePos, &vel, &w1->basePos);
        PSVECScale(&nrm, &vel, maxDepth * -obj_hosei_ratio);
        PSVECAdd(&w2->basePos, &vel, &w2->basePos);
    }
}

// LU decomposition with partial pivoting (Okumura), n = 3. Returns the determinant (0: singular).
f32 lu(f32 a[][3], int* ip)
{
    int i, j, k, ii, ik;
    f32 t, u, det;
    f32 weight[3];

    det = 0.0f;
    for (k = 0; k < 3; k++) {
        ip[k] = k;
        u = 0.0f;
        for (j = 0; j < 3; j++) {
            t = fabsf(a[k][j]);
            if (t > u) {
                u = t;
            }
        }
        if (u == 0.0f) {
            goto EXIT;
        }
        weight[k] = 1.0f / u;
    }
    det = 1.0f;
    for (k = 0; k < 3; k++) {
        u = -1.0f;
        for (i = k; i < 3; i++) {
            ii = ip[i];
            t = fabsf(a[ii][k]) * weight[ii];
            if (t > u) {
                u = t;
                j = i;
            }
        }
        ik = ip[j];
        if (j != k) {
            ip[j] = ip[k];
            ip[k] = ik;
            det = -det;
        }
        u = a[ik][k];
        det *= u;
        if (u == 0.0f) {
            goto EXIT;
        }
        for (i = k + 1; i < 3; i++) {
            ii = ip[i];
            t = (a[ii][k] /= u);
            for (j = k + 1; j < 3; j++) {
                a[ii][j] -= t * a[ik][j];
            }
        }
    }
EXIT:
    return det;
}

// Solves the LU system for x.
static void solve(f32 a[][3], f32* b, int* ip, f32* x)
{
    int i, j, ii;
    f32 t;

    for (i = 0; i < 3; i++) {
        ii = ip[i];
        t = b[ii];
        for (j = 0; j < i; j++) {
            t -= a[ii][j] * x[j];
        }
        x[i] = t;
    }
    for (i = 2; i >= 0; i--) {
        t = x[i];
        ii = ip[i];
        for (j = i + 1; j < 3; j++) {
            t -= a[ii][j] * x[j];
        }
        x[i] = t / a[ii][i];
    }
}

// Solve a x = b (3x3); returns the determinant of a.
f32 LinerEquation3(f32 a[][3], f32* b, f32* x)
{
    int ip[3];
    f32 det;

    det = lu(a, ip);
    if (det != 0.0f) {
        solve(a, b, ip, x);
    }
    return det;
}

// A corner (`lp` in body space, `wp` in the world) hit the scenario at `hit` with normal `nrm`:
// push the body out, add the spring / damper force and the friction impulse.
static void calcPointHit(cObj* pObj, Efm09Work* pFree, Vec* pos1, Vec* pos2, Vec* pos_wld, Vec* cross, Vec* pNorm)
{
    static f32 frc_ratio = -1.0f;
    Vec d;
    Vec t;
    Vec v;
    Vec vt;
    Vec r;
    Vec rr;
    f32 A[3][3];
    Vec b;
    Vec x;
    f32 dot;
    f32 len;
    f32 frc;
    f32 ix, iy, iz, im;
    f32 mag;
    f32 lim;

    PSVECSubtract(cross, pos_wld, &d);
    dot = PSVECDotProduct(pNorm, &d);
    if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
        d.y = 1.0f;
    }
#line 683 "D:/Bio4/Prog/obj09.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, dot);
    len = PSVECMag(&d);
    PSVECScale(pNorm, &t, len);
    PSVECAdd(&pFree->basePos, &t, &pFree->basePos);
    if (len > max_in_dist) {
        len = max_in_dist;
    }
    PSVECScale(pNorm, &d, len * (sprg * pFree->mass));
    AddForce(pObj, cross, &d);
    PSVECCrossProduct(&pFree->w, pos2, &t);
    PSVECAdd(&pFree->spd, &t, &t);
    dot = PSVECDotProduct(pNorm, &t);
    PSVECScale(pNorm, &v, dot);
    PSVECScale(pNorm, &d, dot * (dmp_ratio * pFree->mass));
    AddForce(pObj, cross, &d);
    PSVECSubtract(&t, &v, &vt);
    frc = PSVECMag(&v) * (frc_ratio * pFree->mass);
    if (vt.x == 0.0f && vt.y == 0.0f && vt.z == 0.0f) {
        d.x = d.y = d.z = 0.0f;
    } else {
#line 756 "D:/Bio4/Prog/obj09.cpp"
        VECNormalize(&vt, &d);
    }
    PSVECScale(&d, &d, frc);
    PSVECSubtract(cross, &pFree->basePos, &r);
    rr = r;
    ix = 1.0f / pFree->moment.x;
    iy = 1.0f / pFree->moment.y;
    iz = 1.0f / pFree->moment.z;
    im = 1.0f / pFree->mass;
    A[0][0] = iy * rr.z * rr.z + iz * rr.y * rr.y + im;
    A[0][1] = -iz * rr.x * rr.y;
    A[0][2] = -iy * rr.x * rr.z;
    A[1][0] = -iz * rr.x * rr.y;
    A[1][1] = iz * rr.x * rr.x + ix * rr.z * rr.z + im;
    A[1][2] = -ix * rr.y * rr.z;
    A[2][0] = -iy * rr.x * rr.z;
    A[2][1] = -ix * rr.y * rr.z;
    A[2][2] = ix * rr.y * rr.y + iy * rr.x * rr.x + im;
    b.x = -vt.x * 2.0f;
    b.y = -vt.y * 2.0f;
    b.z = -vt.z * 2.0f;
    if (LinerEquation3(A, (f32*) &b, (f32*) &x) != 0.0f) {
        d.x = x.x;
        d.y = x.y;
        d.z = x.z;
        mag = PSVECMag(&d);
        lim = MUE * PSVECMag(&v) * pFree->mass;
        if (mag > lim) {
            if (mag > 0.0f) {
                PSVECScale(&d, &d, SQRTF(lim / mag));
            } else {
                d.x = d.y = d.z = 0.0f;
            }
        }
    } else {
        d.x = d.y = d.z = 0.0f;
    }
    AddForce(pObj, cross, &d);
}

// Per-frame Efm09: gravity, corner contacts with the scenario (SatMgr sweeps of the 8 corners),
// contacts with the other rigid bodies, water (buoyancy/drag + ripples) and sand (drag + sand
// deformation), a push from the player when he walks into it, the Calc step (dt from the frame
// rate), then writes pos/ang from basePos/mat; destroyed when it falls out of the room.
void cObj09::move()
{
    static f32 water_regist = -0.5f;
    static f32 pl_pow_mul = -2500.0f;
    static f32 pl_spd_dist = 1500.0f;
    static f32 pl_spd_mul = -0.015f;
    static f32 pl_spd_mul2 = -0.25f;
    Efm09Work* w = &efm09;
    f32 dt = 1.0f / 30.0f;
    Vec old;
    Vec lp;
    Vec wp;
    Vec hit;
    Vec nrm;
    Vec d;
    Vec v;
    Vec n;
    Vec n2;
    f32 h;
    f32 dist;
    cObj* p;
    cObj* q;
    void (*func)(cObj*);
    u32 cnt;
    int i;

    if (w->basePos.y < -10000.0f) {
        w->basePos.x = 8000.0f;
        w->basePos.y = 10000.0f;
        w->basePos.z = 8000.0f;
    }
    pObj_ck = this;
    func = Obj09HitCheck;
    p = (cObj*) ObjMgr.getActiveWork();
    while (p) {
        q = p;
        p = (cObj*) p->pNext;
        func(q);
    }

    old = w->basePos;
    cnt = 0;
    for (i = 0; i < 8; i++) {
        lp.x = pos_tbl[i].x * w->size.x;
        lp.y = pos_tbl[i].y * w->size.y;
        lp.z = pos_tbl[i].z * w->size.z;
        PSMTXMultVecSR(w->mat, &lp, &lp);
        PSVECAdd(&lp, &w->basePos, &wp);
        if (SatMgr.hitCheck(&old, &wp, &hit, &nrm, 0, 0)) {
            cnt++;
            calcPointHit(this, w, &old, &lp, &wp, &hit, &nrm);
            if (cnt > 4) {
                break;
            }
        }
    }
    w->spd.y -= grav;
    if (GetWaterHeight(&w->basePos, &h) && w->basePos.y < h) {
        w->spd.y += grav * 1.2f;
        AddWaterPower(w->basePos, -w->spd.y * 2.0e-6f);
        AddWaterPower(w->basePos, PSVECMag(&w->spd) * 1.0e-5f);
        w->spd.y *= 0.5f;
        PSVECScale(&w->rotSpd, &w->rotSpd, 0.92f);
        d = w->basePos;
        PSVECScale(&w->spd, &v, water_regist * w->mass);
        d.x += PSVECMag(&w->size);
        AddForce(this, &d, &v);
    }
    d = pPL->pos;
    d.y += 1000.0f;
    PSVECSubtract(&w->basePos, &d, &d);
    dist = PSVECMag(&d);
    if (dist < 500.0f) {
#line 979 "D:/Bio4/Prog/obj09.cpp"
        VECNormalize(&d, &n);
        PSVECScale(&n, &n, (dist - 500.0f) * pl_pow_mul * w->mass);
        AddForce(this, &w->basePos, &n);
    }
    if (dist < pl_spd_dist) {
#line 990 "D:/Bio4/Prog/obj09.cpp"
        VECNormalize(&d, &n2);
        PSVECSubtract(&pPL->pParts->world, &pPL->pParts->world_old2, &v);
        PSVECScale(&v, &v, (dist - pl_spd_dist) * pl_spd_mul * w->mass);
        PSVECScale(&n2, &n2, (dist - pl_spd_dist) * pl_spd_mul2 * w->mass);
        PSVECAdd(&v, &n2, &v);
        AddForce(this, &w->basePos, &v);
    }
    if (GetSandHeight(&w->basePos, &h) && w->basePos.y < h) {
        v = w->basePos;
        v.x += 150.0f;
        AddSandPower(v, -2.0f);
        v = w->basePos;
        v.x -= 150.0f;
        AddSandPower(v, -2.0f);
        v = w->basePos;
        v.z += 150.0f;
        AddSandPower(v, -2.0f);
        v = w->basePos;
        v.z -= 150.0f;
        AddSandPower(v, -2.0f);
        v = w->basePos;
        AddSandPower(v, -2.0f);
    }
    Calc(this, dt);
    PSVECScale(&w->spd, &w->spd, spd_reg);
    PSVECScale(&w->rotSpd, &w->rotSpd, rot_reg);
    if (SatMgr.hitCheck(&w->pos, &w->basePos, 0, 0, 0, 0)) {
        w->basePos = w->pos;
        PSVECScale(&w->spd, &w->spd, 0.6f);
        PSVECScale(&w->rotSpd, &w->rotSpd, 0.6f);
    }
    w->pos = w->basePos;
    PSMTXCopy(w->mat, mat);
    TransMatrix(mat, &w->basePos);
    ScaleMatrix(mat, &scale);
    partsMatCalc();
    partsWorldCalc();
}

asm(".section .sdata; .balign 8");
