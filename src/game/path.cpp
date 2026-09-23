#include "path.h"
#include "esp.h"
#include "model.h"
#include "motion.h"
#include "math_sub.h"
#include "hermite.h"
#include "main_mem.h"
#include "db_log.h"

// 1 when any vertex of the path follows model parts (nWeight != 0), i.e. the path moves with an
// enemy and the *Em variants must be used.
int PathHasWeight(void* pPdat)
{
    Path* p = (Path*)pPdat;
    PathVtx* v = p->vtx;
    int i;
    int w = 0;

    for (i = 0; i < p->num; i++) {
        w += v->nWeight;
        v++;
    }
    if (w != 0) return 1;
    return 0;
}

// Total length: the last vertex's distance from the start.
f32 PathGetLength(void* pPdat)
{
    Path* p = (Path*)pPdat;
    PathVtx* v = p->vtx;
    return v[p->num - 1].dist;
}

// Point at `dist` along a fixed path, linear between vertices; `seg` caches the segment and is
// searched from there in either direction. Returns 0 (out = 0) when dist is outside 0..length.
int PathGetPos(void* pPdat, f32 dist, u16* pPntNo, Vec* pPos)
{
    Path* p = (Path*)pPdat;
    PathVtx* v = p->vtx;
    PathVtx* prev;
    int step;
    f32 fstep;
    f32 len;
    Vec tmp;

    len = PathGetLength(pPdat);
    if (dist >= len || dist < 0.0f) {
        pPos->x = pPos->y = pPos->z = 0.0f;
        return 0;
    }
    step = -1;
    v += *pPntNo;
    if (dist >= v->dist) step = 1;
    fstep = (f32)step;
    do {
        *pPntNo += step;
        v += step;
    } while (fstep * v->dist < fstep * dist);
    *pPntNo -= step;
    prev = v - step;
    dist -= prev->dist;
    dist /= v->dist - prev->dist;
    PSVECSubtract(&v->pos, &prev->pos, &tmp);
    PSVECScale(&tmp, &tmp, dist);
    PSVECAdd(&prev->pos, &tmp, pPos);
    return 1;
}

// PathGetPos for a path attached to `model`: both segment vertices are first moved by their
// weighted parts matrices (PathGetVtxMat).
int PathGetPosEm(void* pPdat, cModel* pMod, f32 dist, u16* pPntNo, Vec* pPos)
{
    Path* p = (Path*)pPdat;
    PathVtx* v = p->vtx;
    PathVtx* prev;
    int step;
    f32 fstep;
    f32 len;
    Vec p0;
    Vec p1;
    Vec tmp;
    Mtx m0;
    Mtx m1;

    len = PathGetLength(pPdat);
    if (dist >= len || dist < 0.0f) {
        pPos->x = pPos->y = pPos->z = 0.0f;
        return 0;
    }
    step = -1;
    v += *pPntNo;
    if (dist >= v->dist) step = 1;
    fstep = (f32)step;
    do {
        *pPntNo += step;
        v += step;
    } while (fstep * v->dist < fstep * dist);
    *pPntNo -= step;
    prev = v - step;
    dist -= prev->dist;
    dist /= v->dist - prev->dist;
    PathGetVtxMat(m0, pMod, prev);
    PathGetVtxMat(m1, pMod, v);
    PSMTXMultVec(m0, &prev->pos, &p0);
    PSMTXMultVec(m1, &v->pos, &p1);
    {
        Vec* pp = &p0;
        PSVECSubtract(&p1, pp, &tmp);
        PSVECScale(&tmp, &tmp, dist);
        PSVECAdd(pp, &tmp, pPos);
    }
    return 1;
}

// Full matrix at `dist` along a path on `model`: position from a Hermite curve through the segment
// (tangents from the neighbour vertices), forward from the curve tangent, up from the vertices'
// interpolated nrm. Returns 0 (identity) when dist is outside the path.
int PathGetMatEm(void* pPdat, cModel* pMod, f32 dist, u16* pPntNo, Mtx pMat)
{
    Path* p = (Path*)pPdat;
    PathVtx* vtx;
    PathVtx* v;
    PathVtx* prev;
    PathVtx* v0;
    PathVtx* pv;
    PathVtx* pn;
    PathVtx* pb;
    int step;
    int j = 0;    // dead initialisers: 3 more insn uids, which decides gcse's hash table size
    int k = 0;    // and with it the spill-slot order of the PRE'd &local pseudos
    int idx = 0;
    int ib;
    int in;
    f32 fstep;
    f32 len;
    f32 t;
    f32 d;
    Vec p0;
    Vec p1;
    Vec tmp;
    Mtx m0;
    Mtx m1;
    Vec n0;
    Vec n1;
    Vec side;
    Vec up;
    Vec fwd;
    Vec lpos;
    Vec hpos;
    Vec hvel;
    Vec d0;
    Vec d1;
    HermiteKey key0;
    HermiteKey key1;
    static int dbg_tangent_base = 1;
    static int inter_flag = 1;

    len = PathGetLength(pPdat);
    vtx = p->vtx;
    if (dist >= len || dist < 0.0f) {
        PSMTXIdentity(pMat);
        return 0;
    }
    step = -1;
    v = &vtx[*pPntNo];
    if (dist >= v->dist) step = 1;
    fstep = (f32)step;
    do {
        *pPntNo += step;
        v += step;
    } while (fstep * v->dist < fstep * dist);
    *pPntNo -= step;
    prev = v - step;
    t = (dist - prev->dist) / (v->dist - prev->dist);

    PathGetVtxMat(m0, pMod, prev);
    PathGetVtxMat(m1, pMod, v);
    PSMTXMultVec(m0, &prev->pos, &p0);
    PSMTXMultVec(m1, &v->pos, &p1);
    {
        Vec* pp = &p0;
        PSVECSubtract(&p1, pp, &tmp);
#line 291 "D:/Bio4/Prog/path.cpp"
        VECNormalize(&tmp, &fwd);
        PSVECScale(&tmp, &tmp, t);
        PSVECAdd(pp, &tmp, &lpos);
    }

    v0 = v - step;
    for (k = 0; k < 2; k++) {
        pv = (k == 0) ? v0 : v;
        idx = pv - vtx;
        ib = idx - 1;
        in = idx + 1;
        pb = &vtx[ib];
        pn = &vtx[in];
        for (j = 0; j < 3; j++) {
            if (idx == 0) {
                d = (&pn->pos.x)[j] - (&pv->pos.x)[j];
            } else if (p->num - 1 == idx) {
                d = (&pv->pos.x)[j] - (&pb->pos.x)[j];
            } else {
                d = ((&pn->pos.x)[j] - (&pb->pos.x)[j]) * 0.5f;
            }
            if (k == 0) {
                (&d0.x)[j] = d;
            } else {
                (&d1.x)[j] = d;
            }
        }
    }

    for (j = 0; j < 3; j++) {
        f32* ph = &(&hpos.x)[j];   // first &hpos use before &key0/&key1: PRE inserts its copy first
        key0.t = 0.0f;
        key0.v = (&v0->pos.x)[j];
        key0.out = key0.in = (&d0.x)[j];
        key1.t = 1.0f;
        key1.v = (&v->pos.x)[j];
        key1.out = key1.in = (&d1.x)[j];
        Hermite_1(&key0, &key1, t, ph);
        Hermite_1_dt(&key0, &key1, t, &(&hvel.x)[j]);
    }
    PSMTXMultVec(m0, &hpos, &hpos);
    PSMTXMultVecSR(m0, &hvel, &hvel);
    PSMTXMultVecSR(m0, &(v - step)->nrm, &n0);
    PSMTXMultVecSR(m1, &v->nrm, &n1);
    PSVECSubtract(&n1, &n0, &tmp);
    PSVECScale(&tmp, &tmp, t);
    PSVECAdd(&n0, &tmp, &up);
    PSVECCrossProduct(&up, &fwd, &side);

    if (dbg_tangent_base) {
#line 404 "D:/Bio4/Prog/path.cpp"
        VECNormalize(&hvel, &fwd);
        PSVECCrossProduct(&up, &fwd, &side);
        PSVECCrossProduct(&fwd, &side, &up);
#line 410 "D:/Bio4/Prog/path.cpp"
        VECNormalize(&side, &side);
        VECNormalize(&up, &up);
    } else {
        PSVECCrossProduct(&side, &up, &fwd);
#line 416 "D:/Bio4/Prog/path.cpp"
        VECNormalize(&side, &side);
        VECNormalize(&up, &up);
        VECNormalize(&fwd, &fwd);
    }

    if (inter_flag) {
        pMat[0][0] = side.x;
        pMat[1][0] = side.y;
        pMat[2][0] = side.z;
        pMat[0][1] = up.x;
        pMat[1][1] = up.y;
        pMat[2][1] = up.z;
        pMat[0][2] = fwd.x;
        pMat[1][2] = fwd.y;
        pMat[2][2] = fwd.z;
        pMat[0][3] = hpos.x;
        pMat[1][3] = hpos.y;
        pMat[2][3] = hpos.z;
    } else {
        pMat[0][0] = side.x;
        pMat[1][0] = side.y;
        pMat[2][0] = side.z;
        pMat[0][1] = up.x;
        pMat[1][1] = up.y;
        pMat[2][1] = up.z;
        pMat[0][2] = fwd.x;
        pMat[1][2] = fwd.y;
        pMat[2][2] = fwd.z;
        pMat[0][3] = lpos.x;
        pMat[1][3] = lpos.y;
        pMat[2][3] = lpos.z;
    }
    return 1;
}

#define MAT_ACC(dst, src, w)               \
    dst[0][0] += src[0][0] * w;            \
    dst[0][1] += src[0][1] * w;            \
    dst[0][2] += src[0][2] * w;            \
    dst[0][3] += src[0][3] * w;            \
    dst[1][0] += src[1][0] * w;            \
    dst[1][1] += src[1][1] * w;            \
    dst[1][2] += src[1][2] * w;            \
    dst[1][3] += src[1][3] * w;            \
    dst[2][0] += src[2][0] * w;            \
    dst[2][1] += src[2][1] * w;            \
    dst[2][2] += src[2][2] * w;            \
    dst[2][3] += src[2][3] * w

// Skinning matrix of a path vertex: sum of the parts' matrices weighted by weight[] percent
// (the last weight takes the remainder), concatenated with the same blend of their bind matrices.
void PathGetVtxMat(Mtx pMat, cModel* pMod, PathVtx* pPunit)
{
    Mtx m;
    Mtx m2;
    int i;
    f32 w;
    f32 wsum;
    cParts* p;

    memclr_asm(m, sizeof(Mtx));
    memclr_asm(m2, sizeof(Mtx));
    wsum = 0.0f;
    for (i = 0; i < pPunit->nWeight; i++) {
        if (pPunit->partsNo[i] > pMod->nParts) {
            PSMTXIdentity(pMat);
            pLog->err(0, 0, "PathGetVtxMat(): Invalid Parts Number %d.\n", pPunit->partsNo[i]);
            return;
        }
        p = pMod->getPartsPtr(pPunit->partsNo[i]);
        w = (f32)pPunit->weight[i] * 0.01f;
        if (i == pPunit->nWeight - 1) w = 1.0f - wsum;
        wsum += w;
        MAT_ACC(m, p->mat, w);
        MAT_ACC(m2, IK_PARTS(p)->bindMat, w);
    }
    PSMTXConcat(m, m2, pMat);
}

// Turns the n B-spline control points of order k (id_sys path0) into the interpolation
// coefficients `alpha` (solves the de Boor-Cox basis matrix by Gaussian inversion); temporary
// matrices from MEM_ALLOC. Returns 1 on success.
int FuncPathParametrize(void* pPath, void* pB)
{
    FuncPathData* d = (FuncPathData*)pPath;
    FuncPathWork* w = (FuncPathWork*)pB;
    f32* A;
    f32* Ainv;
    f32* x;
    f32* y;
    f32* z;
    f32* tmp_alpha;
    int i;

    w->n = d->n;
    w->k = d->k;
    if (w->k > w->n - 1) w->k = w->n - 1;
    if (w->n <= 1) return 1;

#line 520 "D:/Bio4/Prog/path.cpp"
    A = (f32*)MEM_ALLOC(sizeof(f32) * w->n * w->n, 1, 13);
    if (A == NULL) {
        pLog->err(0, 0, "FuncPathParametrize(): A, Memory allocation error!");
        return 0;
    }
#line 528 "D:/Bio4/Prog/path.cpp"
    Ainv = (f32*)MEM_ALLOC(sizeof(f32) * w->n * w->n, 1, 13);
    if (Ainv == NULL) {
        pLog->err(0, 0, "FuncPathParametrize(): Ainv, Memory allocation error!");
        Mem_free(A);
        return 0;
    }
#line 537 "D:/Bio4/Prog/path.cpp"
    x = (f32*)MEM_ALLOC(sizeof(f32) * w->n, 1, 13);
    if (x == NULL) {
        pLog->err(0, 0, "FuncPathParametrize(): x, Memory allocation error!");
        Mem_free(A);
        Mem_free(Ainv);
        return 0;
    }
#line 547 "D:/Bio4/Prog/path.cpp"
    y = (f32*)MEM_ALLOC(sizeof(f32) * w->n, 1, 13);
    if (y == NULL) {
        pLog->err(0, 0, "FuncPathParametrize(): y, Memory allocation error!");
        Mem_free(A);
        Mem_free(Ainv);
        Mem_free(x);
        return 0;
    }
#line 558 "D:/Bio4/Prog/path.cpp"
    z = (f32*)MEM_ALLOC(sizeof(f32) * w->n, 1, 13);
    if (z == NULL) {
        pLog->err(0, 0, "FuncPathParametrize(): z, Memory allocation error!");
        Mem_free(A);
        Mem_free(Ainv);
        Mem_free(x);
        Mem_free(y);
        return 0;
    }
#line 570 "D:/Bio4/Prog/path.cpp"
    tmp_alpha = (f32*)MEM_ALLOC(sizeof(f32) * w->n, 1, 13);
    if (tmp_alpha == NULL) {
        pLog->err(0, 0, "FuncPathParametrize(): tmp_alpha, Memory allocation error!");
        Mem_free(A);
        Mem_free(Ainv);
        Mem_free(x);
        Mem_free(y);
        Mem_free(z);
        return 0;
    }

    for (i = 0; i < w->n; i++) {
        x[i] = d->pos[i].x;
        y[i] = d->pos[i].y;
        z[i] = d->pos[i].z;
    }
    for (i = 0; i < w->n; i++) {
        if (de_Boor_Cox(w->n, NULL, (f32)i, w->k, &A[w->n * i]) == 0) return 0;
    }
    if (MtxNNInverse(w->n, A, Ainv) == 0.0f) {
        pLog->err(0, 0, "FuncPathParametrize(): Can not solve Inverse Matrix.");
        return 0;
    }
    MtxNNMultVecSR(w->n, w->n, Ainv, x, tmp_alpha);
    for (i = 0; i < w->n; i++) {
        w->alpha[i].x = tmp_alpha[i];
    }
    MtxNNMultVecSR(w->n, w->n, Ainv, y, tmp_alpha);
    for (i = 0; i < w->n; i++) {
        w->alpha[i].y = tmp_alpha[i];
    }
    MtxNNMultVecSR(w->n, w->n, Ainv, z, tmp_alpha);
    for (i = 0; i < w->n; i++) {
        w->alpha[i].z = tmp_alpha[i];
    }
    Mem_free(A);
    Mem_free(Ainv);
    Mem_free(x);
    Mem_free(y);
    Mem_free(z);
    Mem_free(tmp_alpha);
    return 1;
}

// Point on the parametrised B-spline at t in 0..1: sum of the basis values times alpha. Returns 0
// on an allocation / basis failure.
int FuncPathCalc(void* pPath, void* pB, f32 t, Vec* p)
{
    FuncPathWork* w = (FuncPathWork*)pB;
    f32* B;
    int i;

#line 639 "D:/Bio4/Prog/path.cpp"
    B = (f32*)MEM_ALLOC(sizeof(f32) * w->n, 1, 13);
    if (B == NULL) {
        pLog->err(0, 0, "FuncPathCalc(): B, Memory allocation error!");
        return 0;
    }
    if (de_Boor_Cox(w->n, NULL, t, w->k, B) == 0) {
        pLog->err(0, 0, "FuncPathCalc(): failed at de_Boor_Cox()");
        Mem_free(B);
        return 0;
    }
    p->x = p->y = p->z = 0.0f;
    for (i = 0; i < w->n; i++) {
        p->x += B[i] * w->alpha[i].x;
        p->y += B[i] * w->alpha[i].y;
        p->z += B[i] * w->alpha[i].z;
    }
    Mem_free(B);
    return 1;
}

// Empties the control point list (n = 0).
void FuncPathClear(void* pPath)
{
    FuncPathData* d = (FuncPathData*)pPath;
    int i;

    for (i = 0; i < d->n; i++) {
        memclr_asm(&d->pos[i], sizeof(Vec));
    }
    d->n = 0;
}

// The split object's .sdata is 8-aligned.
asm(".section .sdata; .balign 8");
