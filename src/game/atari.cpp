// game/atari.cpp: the scenario collision ("atari") system. cSat is one collision piece: a SAT
// file of triangles (floors, slopes, walls; each with a 24-bit attribute word) partitioned into
// XZ blocks, placed by a matrix. SatMgr holds the room's pieces and the ones objects create,
// EatMgr the effect-collision set (what bullets, thrown objects and effects hit). Queries:
// check / checkAir push a character's body out of the scenery (scrAtCheckSphere, at_sub.cpp
// primitives), hitCheck traces a line for the nearest polygon, getFloor probes the floor,
// adjust sweeps a sphere; disp draws the polygons for the debug pages.

#include "atari.h"
#include "atariInfo.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "main.h"
#include "gx.h"
#include <string.h>
#include <dolphin/base/PPCArch.h>

#line 30 "D:/Bio4/Prog/atari.cpp"

// at_sub attribute filter bypass mode (cSatMgr::seCk of the manager running the check)
int SEck;
struct SEckView {
    int v;
};

// game/game.cpp collision profiling counters (debug page 0x14); uninitialised there, so not in game.h (a header
// extern reorders game.cpp's .bss)
extern u32 g_at2_total;
extern u32 g_at2_cnt[];
extern u32 g_at2_cyc[];
extern u32 g_at2_total_cyc;

// pointer to game memory (0x80000000 .. 0x82FFFFFF)

// polygons already tested during one check (one bit per polygon index)
u8 polyBit[0x400];
// the piece the last hitCheck2 hit (hitCheck transforms the normal with its matrix)
static cSat* pBypassAt;

int atck(Vec* vec0, Vec* vec1, cAtariInfo* info, cModel* m, int flag);
int blkPolySphereCk(cSat* sat, cSatBlock* blk, Vec* pos0, Vec* pos1, f32 r, int flag, Vec* nrm, int mask);
int blkPolySphereCkCore(cSat* sat, cSatBlock* blk, Vec* pos0, Vec* pos1, f32 r, int flag, Vec* nrm, int mask);
int blkPolyLineCk(cSat* sat, cSatBlock* blk, Vec* pos0, Vec* pos1, int flag, int mask, Vec* hit, u32* pn);
int blkPolyLineCkCore(cSat* sat, cSatBlock* blk, Vec* pos0, Vec* pos1, int flag, int mask, Vec* hit, u32* pn);
void polyBitSet(u32 no);
int polyBitCk(u32 no);
cSatFile* createSat(Vec* v, u32 attr, f32 h);
cSatFile* createBoxSat(Vec* v, u32 attr, f32 h);
static cSatFile* createFloorSat(Vec* v, u32 attr, f32 h);
void at_pos_calc(cModel* m, Vec* vec);

// Model-vs-scenario collision for a character (its cAtariInfo, m_flag 0x100 = collision on):
// the rectangle form (m_flag bit1: 12 edge probes, checkRect) or the sphere form for the info
// and every extra info chained on m_pList (scrAtCheckSphere: walls then floor). 1 when the
// model was pushed.
int cSatMgr::check(cModel* pMod, int mask)
{
    cAtariInfo* info = &((cEm*) pMod)->atari;
    int ret = 0;

    if (!(info->m_flag & 0x100)) {
        return 0;
    }
    if (info->m_flag & 2) {
        ret = checkRect(pMod);
    } else {
        while (info->m_pList) {
            info = info->m_pList;
            if (scrAtCheckSphere(pMod, info, mask) != 0.0f) {
                ret = 1;
            }
        }
        if (scrAtCheckSphere(pMod, &((cEm*) pMod)->atari, mask) != 0.0f) {
            ret = 1;
        }
    }
    return ret;
}

// Rectangle collision: 12 horizontal probes from the model centre / its long axis to the box
// corners (m_radius x m_radius2) against the walls at pos.y + 300; each hit pushes the model
// back along the surface normal. 1 when any probe moved it.
int cSatMgr::checkRect(cModel* pMod)
{
    cAtariInfo* info = &((cEm*) pMod)->atari;
    Vec a;
    Vec b;
    int ret;

    a.x = 0.0f;
    a.y = 0.0f;
    a.z = info->m_radius2 * 0.9f;
    b.x = info->m_radius;
    b.y = 0.0f;
    b.z = info->m_radius2 * 0.9f;
    ret = atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = info->m_radius;
    b.y = 0.0f;
    b.z = 0.0f;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = -info->m_radius2 * 0.9f;
    b.x = info->m_radius;
    b.y = 0.0f;
    b.z = -info->m_radius2 * 0.9f;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = info->m_radius2 * 0.9f;
    b.x = -info->m_radius;
    b.y = 0.0f;
    b.z = info->m_radius2 * 0.9f;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = -info->m_radius;
    b.y = 0.0f;
    b.z = 0.0f;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = -info->m_radius2 * 0.9f;
    b.x = -info->m_radius;
    b.y = 0.0f;
    b.z = -info->m_radius2 * 0.9f;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = info->m_radius * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = info->m_radius * 0.9f;
    b.y = 0.0f;
    b.z = info->m_radius2;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 0.0f;
    b.z = info->m_radius2;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = -info->m_radius * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = -info->m_radius * 0.9f;
    b.y = 0.0f;
    b.z = info->m_radius2;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = info->m_radius * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = info->m_radius * 0.9f;
    b.y = 0.0f;
    b.z = -info->m_radius2;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 0.0f;
    b.z = -info->m_radius2;
    ret |= atck(&a, &b, info, pMod, 0);
    a.x = -info->m_radius * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = -info->m_radius * 0.9f;
    b.y = 0.0f;
    b.z = -info->m_radius2;
    ret |= atck(&a, &b, info, pMod, 0);
    return ret;
}

// Airborne variant of check(): sphere collision without the floor snap (scrAtCheckSphereAir) for
// the info and its chained infos.
int cSatMgr::checkAir(cModel* pMod, int mask)
{
    cAtariInfo* info = &((cEm*) pMod)->atari;
    int ret = 0;

    if (info->m_flag & 0x100) {
        while (info->m_pList) {
            info = info->m_pList;
            if (scrAtCheckSphereAir(pMod, info, mask) != 0.0f) {
                ret = 1;
            }
        }
        if (scrAtCheckSphereAir(pMod, &((cEm*) pMod)->atari, mask) != 0.0f) {
            ret = 1;
        }
    }
    return ret;
}

// Segment a-b (model local, offset by the info position) against the scenario; the model is
// pushed back to the hit point. Returns 1 when it moved by a metre or more.
int atck(Vec* vec0, Vec* vec1, cAtariInfo* offset, cModel* pMod, int flag)
{
    Vec v0;
    Vec v1;
    Vec hit;
    Vec nrm;
    Vec old;

    PSVECAdd(vec0, &offset->m_offset, &v0);
    PSVECAdd(vec1, &offset->m_offset, &v1);
    RotVector(&v0, &pMod->ang, &v0);
    RotVector(&v1, &pMod->ang, &v1);
    PSVECAdd(&v0, &pMod->pos, &v0);
    PSVECAdd(&v1, &pMod->pos, &v1);
    v0.y = v1.y = pMod->pos.y + 300.0f;
    if (SatMgr.hitCheck(&v0, &v1, &hit, &nrm, flag, 0)) {
        old = pMod->pos;
        PSVECSubtract(&hit, &v1, &v1);
        PSVECAdd(&pMod->pos, &v1, &pMod->pos);
        PSVECAdd(&pMod->pos, &nrm, &pMod->pos);
        if (fabsf(pMod->pos.x - old.x) >= 1.0f || fabsf(pMod->pos.z - old.z) >= 1.0f) {
            return 1;
        }
    }
    return 0;
}

// Sphere of the collision info against the scenario (walls, then the floor). Returns the
// distance the model was pushed.
f32 cSatMgr::scrAtCheckSphere(cModel* pMod, cAtariInfo* pAt, int mask)
{
    Vec pos;
    Vec oldPos;
    Vec newPos;
    Vec up;
    f32 mag;
    f32 floor;
    cModel* link;
    u32 c;

    pAt->getSpeedVector(pMod, &oldPos, &pos);
    pMod->Wall_norm.x = 0.0f;
    pMod->Wall_norm.y = 0.0f;
    pMod->Wall_norm.z = 0.0f;
    newPos = pos;
    wallAdjust(&pMod->Wall_norm, &oldPos, &newPos, pAt->m_radius, pAt->m_flag, mask);
    PSVECSubtract(&newPos, &pos, &pos);
    mag = PSVECMag(&pos);
    at_pos_calc(pMod, &pos);
    if (!(pAt->m_flag & 4)) {
        floor = getFloor(&pMod->pos, (u32*) &pMod->pFloor_norm, 600.0f, 100000.0f, mask);
        if (fabsf(floor - pMod->pos.y) < 1000.0f) {
            pMod->pos.y = floor;
        } else if (pG->shooting_mode == 0) {
            f32 x = (f32) ((int) pMod->pos.x / 100) * 100.0f;
            f32 y = (f32) ((int) pMod->pos.y / 100) * 100.0f;
            f32 z = (f32) ((int) pMod->pos.z / 100) * 100.0f;
            if (floor == -100000.0f) {
                pLog->warn(6, 1, "FLOOR LOST %.0f %.0f %.0f", x, y, z);
            } else {
                pLog->warn(4, 2, "FLOOR ERR %.0f %.0f %.0f", x, y, z);
            }
            up = pMod->pos;
            up.y += 50000.0f;
            c = pG->Frame_cnt & 0x3F;
            c <<= 2;
            if (pG->debug_mode != 0) {
                Draw_line3d(&pMod->pos, &up, 0xFFFF0000 | (c << 8) | c, 0);
            }
        }
    }
    link = pAt->m_pMod;
    if (link) {
        at_pos_calc(link, &pos);
        if (!(((cEm*) link)->atari.m_flag & 4)) {
            floor = getFloor(&link->pos, (u32*) &link->pFloor_norm, 600.0f, 100000.0f, mask);
            if (fabsf(floor - link->pos.y) < 1000.0f) {
                link->pos.y = floor;
            }
        }
    }
    if (DbgFlagChk(pG, DBG_SCA_VIEW)) {
        Draw_sphere(&newPos, pAt->m_radius, 0xA0A0A0A0, 1, 1);
    }
    return mag;
}

// Same for a model in the air: walls only, the height is kept.
f32 cSatMgr::scrAtCheckSphereAir(cModel* pMod, cAtariInfo* pAt, int mask)
{
    Vec pos;
    Vec oldPos;
    Vec newPos;
    Vec mpos;
    f32 mag = 0.0f;
    f32 y;
    cModel* link;

    if (!(pAt->m_flag & 0x100)) {
        return 0.0f;
    }
    {
        mpos = pMod->pos;
        pAt->getSpeedVector(pMod, &oldPos, &pos);
        pMod->Wall_norm.x = 0.0f;
        pMod->Wall_norm.y = 0.0f;
        pMod->Wall_norm.z = 0.0f;
        newPos = pos;
        wallAdjust(&pMod->Wall_norm, &oldPos, &newPos, pAt->m_radius, ((cEm*) pMod)->atari.m_flag, mask);
        PSVECSubtract(&newPos, &pos, &pos);
        mag = PSVECMag(&pos);
        at_pos_calc(pMod, &pos);
        y = pMod->pos.y;
        PSVECAdd(&pMod->pos, &pos, &pMod->pos);
        pMod->pos.y = y;
        link = pAt->m_pMod;
        if (link) {
            PSVECSubtract(&pMod->pos, &mpos, &mpos);
            PSVECAdd(&link->pos, &mpos, &link->pos);
        }
        if (DbgFlagChk(pG, DBG_SCA_VIEW)) {
            Draw_sphere(&newPos, pAt->m_radius, 0xA0A0A0A0, 1, 1);
        }
    }
    return mag;
}

// Sphere moving from oldPos to pos against the walls (flag bit0: the segment too); pos is
// pushed out, nrm receives the wall normal.
void cSatMgr::wallAdjust(Vec* pNorm, Vec* pos_old, Vec* pos_new, f32 radius, int flag, int mask)
{
    Vec hit;
    Vec tmp;
    Vec n;
    Vec d;
    Vec p0;

    if (flag & 1) {
        if (hitCheck(pos_old, pos_new, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, radius);
            PSVECAdd(&hit, &tmp, pos_new);
            if (pNorm) {
                *pNorm = n;
            }
        }
    }
    PSVECSubtract(pos_new, pos_old, &d);
    p0 = *pos_old;
    PSVECAdd(pos_old, &d, pos_new);
    polySphereCk(&p0, pos_new, radius, flag | 0xA0, pNorm, mask);
    polySphereCk(&p0, pos_new, radius, flag | 0x80, pNorm, mask);
    if (flag & 1) {
        if (hitCheck(pos_old, pos_new, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, radius);
            PSVECAdd(&hit, &tmp, pos_new);
            if (pNorm) {
                *pNorm = n;
            }
        }
    }
}

// Moves a sphere of radius `r` from oldPos to pos through the scenario: with flag bit0 a line
// hit is resolved first (pos pushed r along the normal), then the swept-sphere polygon check
// (polySphereCk) and a final line hit. The last contact normal is returned in *nrm (zero when
// nothing was hit). Used by rolling / thrown objects.
void cSatMgr::adjust(Vec* pNorm, Vec* pos_old, Vec* pos_new, f32 radius, int flag, int mask)
{
    Vec hit;
    Vec tmp;
    Vec n;
    Vec d;
    Vec p0;

    if (flag & 1) {
        if (hitCheck(pos_old, pos_new, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, radius);
            PSVECAdd(&hit, &tmp, pos_new);
            if (pNorm) {
                *pNorm = n;
            }
        }
    }
    PSVECSubtract(pos_new, pos_old, &d);
    p0 = *pos_old;
    PSVECAdd(pos_old, &d, pos_new);
    polySphereCk(&p0, pos_new, radius, flag, pNorm, mask);
    if (flag & 1) {
        if (hitCheck(pos_old, pos_new, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, radius);
            PSVECAdd(&hit, &tmp, pos_new);
            if (pNorm) {
                *pNorm = n;
            }
        }
    }
}

// Floor height under `pos`: casts from pos.y + up to pos.y - down against floor polygons
// (0x40) and returns the hit y with its attribute word in *attr; -100000 when nothing is below
// (0 when Debug_flg[1] 0x10000000 disables scenery).
f32 cSatMgr::getFloor(Vec* pos, u32* ppNorm, f32 above_limit, f32 below_limit, int mask)
{
    Vec top;
    Vec bottom;
    Vec hit;

    if (DbgFlagChk(pG, DBG_FLAT_FLOOR)) {
        return 0.0f;
    }
    top.x = pos->x;
    top.y = pos->y + above_limit;
    top.z = pos->z;
    bottom.x = pos->x;
    bottom.y = pos->y - below_limit;
    bottom.z = pos->z;
    if (hitCheck2(&top, &bottom, &hit, ppNorm, 0x40, mask) == 0) {
        return -100000.0f;
    }
    return hit.y;
}

// cManager log hook: warnings through pLog.
void cSatMgr::log(const char* pStr, ...)
{
    va_list ap;

    va_start(ap, pStr);
    pLog->vwarn(0, 0, pStr, ap);
}

// Marks polygon `no` as already tested in this query (polyBit, 0x2000 polygons).
void polyBitSet(u32 pno)
{
    polyBit[pno >> 3] |= 1 << (pno & 7);
}

// Non-zero when polygon `no` was already tested in this query.
int polyBitCk(u32 pno)
{
    return polyBit[pno >> 3] & (1 << (pno & 7));
}

// Segment (centre p, half direction dir, |dir| absDir) against the block's XZ box.
int cSatBlock::lineOverlap(Vec* center, Vec* w, Vec* v)
{
    Vec d;
    Vec ad;

    d.x = (center->x - min.x) - m_Size.x * 0.5f;
    d.z = (center->z - min.z) - m_Size.z * 0.5f;
    ad.x = fabsf(d.x);
    ad.z = fabsf(d.z);
    if (ad.x > v->x + m_Size.x * 0.5f) {
        return 0;
    }
    if (ad.z > v->z + m_Size.z * 0.5f) {
        return 0;
    }
    if (fabsf(d.x * w->z - d.z * w->x) > (m_Size.x * v->z + m_Size.z * v->x) * 0.5f) {
        return 0;
    }
    return 1;
}

// XZ segment a-b against segment c-d.
static inline int lineCross(Vec* a, Vec* b, Vec* c, Vec* d)
{
    f32 denom = (b->x - a->x) * (d->z - c->z) - (b->z - a->z) * (d->x - c->x);
    f32 ax;
    f32 az;
    f32 t;
    f32 s;

    if (denom == 0.0f) {
        return 0;
    }
    ax = a->x - c->x;
    az = a->z - c->z;
    t = az * (d->x - c->x) - ax * (d->z - c->z);
    if (t < 0.0f) {
        if (denom >= 0.0f) {
            return 0;
        }
        if (t < denom) {
            return 0;
        }
    } else {
        if (denom < 0.0f) {
            return 0;
        }
        if (t > denom) {
            return 0;
        }
    }
    s = az * (b->x - a->x) - ax * (b->z - a->z);
    if (s < 0.0f) {
        if (denom >= 0.0f) {
            return 0;
        }
        if (s < denom) {
            return 0;
        }
    } else {
        if (denom < 0.0f) {
            return 0;
        }
        if (s > denom) {
            return 0;
        }
    }
    return 1;
}

// Sphere of radius r sweeping from a to b against the block's XZ box (expanded by r).
int cSatBlock::hitCheckSphere(Vec* pos0, Vec* pos1, f32 radius)
{
    Vec c0;
    Vec c1;
    f32 x0 = min.x;
    f32 z0 = min.z;
    f32 cx = (pos0->x + pos1->x) * 0.5f;
    f32 cz = (pos0->z + pos1->z) * 0.5f;
    f32 hx = fabsf(pos0->x - pos1->x) * 0.5f + radius;
    f32 hz = fabsf(pos0->z - pos1->z) * 0.5f + radius;
    f32 x1;
    f32 z1;

    if (x0 + m_Size.x < cx - hx) {
        return 0;
    }
    if (x0 - m_Size.x > cx + hx) {
        return 0;
    }
    if (z0 + m_Size.z < cz - hz) {
        return 0;
    }
    if (z0 - m_Size.z > cz + hz) {
        return 0;
    }
    if (!(pos0->x < x0 - radius || pos0->x > x0 + m_Size.x + radius || pos0->z < z0 - radius || pos0->z > z0 + m_Size.z + radius)) {
        return 1;
    }
    if (!(pos1->x < x0 - radius || pos1->x > x0 + m_Size.x + radius || pos1->z < z0 - radius || pos1->z > z0 + m_Size.z + radius)) {
        return 1;
    }
    x1 = x0 + m_Size.x + radius;
    z1 = z0 + m_Size.z + radius;
    c0.x = x0;
    c0.y = 0.0f;
    c0.z = z0;
    c1.x = x1;
    c1.y = 0.0f;
    c1.z = z0;
    if (lineCross(pos0, pos1, &c0, &c1)) {
        return 1;
    }
    c0.z = c1.z = z1;
    if (lineCross(pos0, pos1, &c0, &c1)) {
        return 1;
    }
    c0.z = z0;
    c1.x = x0;
    if (lineCross(pos0, pos1, &c0, &c1)) {
        return 1;
    }
    c1.x = c0.x = x1;
    if (lineCross(pos0, pos1, &c0, &c1)) {
        return 1;
    }
    return 0;
}

// The room scenario collision manager (SatMgr): a pool of cSat pieces.
cSatMgr::cSatMgr() : cManager<cSat>(sizeof(cSat), 2)
{
    setName("cSatMgr");
}

// The effect collision manager (EatMgr): the pieces bullets, thrown objects and effects test.
cEatMgr::cEatMgr()
{
    setName("cEatMgr");
}

// Clears the 8 surface effect tables (AtEffInfo per attribute type): every effect pair set to
// "none" (0xD2), all types off.
void cEatMgr::initEffInfo()
{
    int i;

    for (i = 0; i < 8; i++) {
        memclr_asm(&effInfo[i], sizeof(AtEffInfo));
        effInfo[i].eff0[0] = 0xD2;
        effInfo[i].eff13[0] = 0xD2;
        effInfo[i].eff16[0] = 0xD2;
        effInfo[i].eff17[0] = 0xD2;
        effInfo[i].effGun[0] = 0xD2;
        effInfo[i].eff5[0] = 0xD2;
        effInfo[i].eff6[0] = 0xD2;
        effInfo[i].eff0D[0] = 0xD2;
        useFlag[i] = 0;
    }
}

// Register the effect ids of one type; pairs left at the (0xD2, 1) default are not copied.
void cEatMgr::registEffInfo(int type, AtEffInfo* pEi)
{
    useFlag[type] = 1;
    effInfo[type].flag = pEi->flag;
    if (pEi->eff0[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff0[0] = pEi->eff0[0];
        effInfo[type].eff0[1] = pEi->eff0[1];
    }
    if (pEi->eff13[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff13[0] = pEi->eff13[0];
        effInfo[type].eff13[1] = pEi->eff13[1];
    }
    if (pEi->eff16[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff16[0] = pEi->eff16[0];
        effInfo[type].eff16[1] = pEi->eff16[1];
    }
    if (pEi->eff17[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff17[0] = pEi->eff17[0];
        effInfo[type].eff17[1] = pEi->eff17[1];
    }
    if (pEi->effGun[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].effGun[0] = pEi->effGun[0];
        effInfo[type].effGun[1] = pEi->effGun[1];
    }
    if (pEi->eff5[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff5[0] = pEi->eff5[0];
        effInfo[type].eff5[1] = pEi->eff5[1];
    }
    if (pEi->eff6[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff6[0] = pEi->eff6[0];
        effInfo[type].eff6[1] = pEi->eff6[1];
    }
    if (pEi->eff0D[0] != 0xD2 || pEi->eff0[1] == 1) {
        effInfo[type].eff0D[0] = pEi->eff0D[0];
        effInfo[type].eff0D[1] = pEi->eff0D[1];
    }
}

// The surface effect table of attribute type `type` (bullet hit sparks, footsteps, splashes);
// NULL when the room registered none.
AtEffInfo* cEatMgr::getEffInfo(int type)
{
    if (useFlag[type] != 0) {
        return &effInfo[type];
    }
    return 0;
}

// cManager log hook: warnings through pLog.
void cEatMgr::log(const char* pStr, ...)
{
    va_list ap;

    va_start(ap, pStr);
    pLog->vwarn(0, 0, pStr, ap);
}

// Creates a collision piece from SAT file data (a multi-SAT header selects entry `type`) placed
// at pos / rot; NULL when the pool is full.
cSat* cSatMgr::create(void* data, int flag, Vec* pos, Vec* rot, u8 type)
{
    cSat* sat = cManager<cSat>::create();
    cSatHeader* hdr = (cSatHeader*) data;

    if (!VALID_PTR(sat)) {
        return 0;
    }
    sat->x5C = 0;
    if (hdr->m_Version != 0xFF && (hdr->m_Version & 0x80)) {
        data = hdr->getSat(type);
    }
    sat->init((cSatFile*) data, pos ? pos : (Vec*) &vecZero, rot ? rot : (Vec*) &vecZero);
    return sat;
}

// Creates a collision piece from a quad: a floor slab (flag 0x200), a closed box of height h
// (flag 0x100) or open side walls (else), all with attribute `attr`; the built file is freed
// with the piece (m_Flag bit1).
cSat* cSatMgr::create(Vec* pPos, Vec* pAng, Vec* pVec, f32 height, u32 attr, u32 flag)
{
    cSatFile* f;
    cSat* sat;

    if (flag & 0x200) {
        f = createFloorSat(pVec, attr, height);
    } else if (flag & 0x100) {
        f = createBoxSat(pVec, attr, height);
    } else {
        f = createSat(pVec, attr, height);
    }
    if (f == 0) {
        return 0;
    }
    sat = create(f, flag, pPos, pAng, 0);
    if (sat) {
        sat->m_Flag.on(cSat::FLAG_MEM);
    } else {
        Mem_free(f);
    }
    return sat;
}

// Sphere of radius r moving from oldPos to pos against every active piece; pos is pushed out
// of the polygons, nrm (when given) receives the last hit normal. Returns 1 on a hit.
int cSatMgr::polySphereCk(Vec* pos0, Vec* pos1, f32 radius, int flag, Vec* pNorm, int mask)
{
    int ret;
    u32 idx = 0;
    u32 i;

    if (pG->debug_mode == 0x14) {
        Vec d;
        PSVECSubtract(pos0, pos1, &d);
        idx = (u32) (PSVECMag(&d) / 1000.0f);
        if (idx > 0x14) {
            idx = 0x13;
        }
        g_at2_total++;
        g_at2_cnt[idx]++;
        PPCMtpmc1(0);
        PPCMtpmc2(0);
        PPCMtpmc3(0);
        PPCMtpmc4(0);
        PPCMtmmcr1(0x78000000);
        PPCMtmmcr0(0x42);
    }
    ret = 0;
    for (i = 0; i < nArray; i++) {
        cSat* sat = fastAt(i);
        if (sat->isAlive()) {
            Vec lo;
            Vec lp;
            cSatBlock* blk = sat->block_p;
            memclr_asm(polyBit, (sat->polygon_num + 7) / 8);
            PSMTXMultVec(sat->imat, pos0, &lo);
            PSMTXMultVec(sat->imat, pos1, &lp);
            if (blkPolySphereCk(sat, blk, &lo, &lp, radius, flag, pNorm, mask)) {
                PSMTXMultVec(sat->mat, &lp, pos1);
                if (pNorm) {
                    PSMTXMultVecSR(sat->mat, pNorm, pNorm);
                }
                ret = 1;
            }
        }
    }
    if (pG->debug_mode == 0x14) {
        PPCMtmmcr0(0);
        PPCMtmmcr1(0);
        g_at2_cyc[idx] += PPCMfpmc1() / 1000;
        g_at2_total_cyc += PPCMfpmc1() / 1000;
    }
    return ret;
}

// Swept sphere through the block chain: recurses into child blocks (m_Flag bit0) whose box the
// sweep overlaps, tests the polygons of leaf blocks; pos1 is pushed out of every hit polygon.
// 1 when anything was hit.
int blkPolySphereCk(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, f32 radius, int flag, Vec* pNorm, int mask)
{
    int ret = 0;
    int hit;

    while (pBlock) {
        if (pBlock->hitCheckSphere(pos0, pos1, radius)) {
            if (pBlock->m_Flag & 1) {
                hit = blkPolySphereCk(pAt, (cSatBlock*) pBlock->idx, pos0, pos1, radius, flag, pNorm, mask);
            } else {
                hit = blkPolySphereCkCore(pAt, pBlock, pos0, pos1, radius, flag, pNorm, mask);
            }
            if (hit) {
                ret = 1;
            }
        }
        pBlock = pBlock->m_pList;
    }
    return ret;
}

// Sphere test of one block's polygons: floors + slopes (flag 0x40), walls (flag 0x80) or all;
// every polygon is tested once per query (polyBit); a hit adjusts pos1 and returns the polygon's
// normal; Debug_flg[0] 0x08000000 highlights hit polygons.
int blkPolySphereCkCore(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, f32 radius, int flag, Vec* pNorm, int mask)
{
    int ret = 0;
    int start;
    int end;
    int i;
    u16* idx;

    if (flag & SAT_CK_FLOOR) {
        start = 0;
        end = pBlock->m_nFloor + pBlock->m_nSlope;
    } else if (flag & SAT_CK_WALL) {
        start = pBlock->m_nFloor + pBlock->m_nSlope;
        end = start + pBlock->m_nWall;
    } else {
        start = 0;
        end = pBlock->m_nFloor + pBlock->m_nSlope + pBlock->m_nWall;
    }
    idx = &pBlock->idx[start];
    for (i = start; i < end; i++, idx++) {
        AtPoly* poly = &pAt->poly_p[*idx];
        if (polyBitCk(*idx)) {
            continue;
        }
        polyBitSet(*idx);
        if (At_poly_sphere_ck((AtPolyData*) pAt, poly, pos0, pos1, radius, flag, mask)) {
            ret = 1;
            if (pNorm) {
                *pNorm = pAt->norm_p[pAt->poly_p[*idx].n];
            }
            if (DbgFlagChk(pG, DBG_SAT_DISP)) {
                pAt->disp(*idx, 0x40FF0000, 1);
            }
        }
    }
    return ret;
}

// Line pos0 -> pos1 against every live piece: the nearest hit in *hit, its world normal in
// *nrm; returns the hit polygon's attribute word (0 = no hit). flag selects floors / walls,
// `mask` attribute bits to ignore.
int cSatMgr::hitCheck(Vec* pos0, Vec* pos1, Vec* pCross, Vec* pNorm, int flag, int mask)
{
    u32 pn;
    int ret;

    ret = hitCheck2(pos0, pos1, pCross, &pn, flag, mask);
    if (pNorm && ret) {
        PSMTXMultVecSR(pBypassAt->mat, (Vec*) pn, pNorm);
    }
    return ret;
}

// Segment a-b against every active piece. The nearest hit goes to hit (world) and `attr`
// receives the address of the hit polygon's normal in the piece's space; b is moved onto the
// piece's grid (mat * inv * b). Returns the attribute word of the hit polygon or 0.
int cSatMgr::hitCheck2(Vec* pos0, Vec* pos1, Vec* pCross, u32* ppNorm, int flag, int mask)
{
    Vec cur;
    Vec la;
    Vec lb;
    Vec lcur;
    Vec tmp;
    u32 pn;
    int ret = 0;
    u32 i;

    // stored through a struct view: keeps the `cur = *b` loads below the store like the original
    ((SEckView*) &SEck)->v = type;
    cur = *pos1;
    for (i = 0; i < nArray; i++) {
        cSat* sat = fastAt(i);
        if (sat->isAlive()) {
            cSatBlock* blk = sat->block_p;
            int r;
            memclr_asm(polyBit, (sat->polygon_num >> 3) + 1);
            PSMTXMultVec(sat->imat, pos0, &la);
            PSMTXMultVec(sat->imat, pos1, &lb);
            PSMTXMultVec(sat->imat, &cur, &lcur);
            r = blkPolyLineCk(sat, blk, &la, &lb, flag, mask, &lcur, &pn);
            if (r) {
                PSMTXMultVec(sat->imat, &cur, &tmp);
                if (GetDistance(&la, &lcur) < GetDistance(&la, &tmp)) {
                    PSMTXMultVec(sat->mat, &lb, pos1);
                    ret = r;
                    PSMTXMultVec(sat->mat, &lcur, &cur);
                    pBypassAt = sat;
                }
            }
        }
    }
    if (pCross) {
        *pCross = cur;
    }
    if (ret && ppNorm) {
        *ppNorm = pn;
    }
    return ret;
}

// Line test through the block chain: descends into blocks whose box the segment overlaps and
// keeps the nearest hit (position, normal pointer in *pn). Returns the attribute of that hit.
int blkPolyLineCk(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, int flag, int mask, Vec* pCross, u32* ppNorm)
{
    static int new_line_check = 1;
    Vec mid;
    Vec dir;
    Vec adir;
    int ret = 0;
    int r;

    PSVECAdd(pos0, pos1, &mid);
    PSVECScale(&mid, &mid, 0.5f);
    PSVECSubtract(pos0, &mid, &dir);
    adir.x = fabsf(dir.x);
    adir.y = fabsf(dir.y);
    adir.z = fabsf(dir.z);
    adir.y = 0.0f;
    dir.y = 0.0f;
    mid.y = 0.0f;
    while (pBlock) {
        if (new_line_check == 0) {
            if (pBlock->hitCheckSphere(pos0, pos1, 0.0f)) {
                if (pBlock->m_Flag & 1) {
                    r = blkPolyLineCk(pAt, (cSatBlock*) pBlock->idx, pos0, pos1, flag, mask, pCross, ppNorm);
                    if (r) {
                        ret = r;
                    }
                } else {
                    r = blkPolyLineCkCore(pAt, pBlock, pos0, pos1, flag, mask, pCross, ppNorm);
                    if (r) {
                        ret = r;
                    }
                }
            }
        } else {
            if (pBlock->lineOverlap(&mid, &dir, &adir)) {
                if (pBlock->m_Flag & 1) {
                    r = blkPolyLineCk(pAt, (cSatBlock*) pBlock->idx, pos0, pos1, flag, mask, pCross, ppNorm);
                    if (r) {
                        ret = r;
                    }
                } else {
                    r = blkPolyLineCkCore(pAt, pBlock, pos0, pos1, flag, mask, pCross, ppNorm);
                    if (r) {
                        ret = r;
                    }
                }
            }
        }
        pBlock = pBlock->m_pList;
    }
    return ret;
}

// Line test of one block's polygons (floor / wall subset by flag), each once per query; keeps
// the hit closest to pos0 and its normal.
int blkPolyLineCkCore(cSat* pAt, cSatBlock* pBlock, Vec* pos0, Vec* pos1, int flag, int mask, Vec* pCross, u32* ppNorm)
{
    Vec h;
    int ret = 0;
    int start;
    int end;
    int n;
    u16* idx;

    if (flag & SAT_CK_FLOOR) {
        start = 0;
        end = pBlock->m_nFloor + pBlock->m_nSlope;
    } else if (flag & SAT_CK_WALL) {
        start = pBlock->m_nFloor + pBlock->m_nSlope;
        end = start + pBlock->m_nWall;
    } else {
        start = 0;
        end = pBlock->m_nFloor + pBlock->m_nSlope + pBlock->m_nWall;
    }
    n = end - start;
    idx = &pBlock->idx[start];
    idx--;
    while (n--) {
        u32 no;
        AtPoly* poly;
        u32 bit;
        u32 attr;
        idx++;
        no = *idx;
        poly = &pAt->poly_p[no];
        bit = 1 << (no & 7);
        if (polyBit[no >> 3] & bit) {
            continue;
        }
        polyBit[no >> 3] |= bit;
        attr = At_poly_line_ck((AtPolyData*) pAt, &h, poly, pos0, pos1, flag, mask);
        if (attr) {
            if (GetDistance(pos0, &h) < GetDistance(pos0, pCross)) {
                *pCross = h;
                ret = attr;
                if (ppNorm) {
                    *ppNorm = (u32) &pAt->norm_p[pAt->poly_p[*idx].n];
                }
            }
        }
    }
    return ret;
}

// Releases a piece (freeing a file built by create(poly)); an invalid pointer is an error.
void cSatMgr::destroy(cSat* p)
{
    if (!VALID_PTR(p)) {
        pLog->err(0, 0, "cSatMgr::destroy() PTR ERR 0x%08x", p);
        return;
    }
    if (p->m_Flag.check(cSat::FLAG_MEM)) {
        Mem_free(p->pFile);
    }
    cManager<cSat>::destroy(p);
}

// cManager hook: constructs a fresh cSat in the pool slot.
int cSatMgr::construct(cSat* pSat, u32 id)
{
    // the alive flag before, the active flag after the constructor: keeps the vptr store last
    pSat->be_flag = 1;
    new (pSat) cSat();
    pSat->m_Flag.reset();
    return 1;
}

// Debug draw: flag low nibble selects the polygon group, bits 24-31 an attribute bit to highlight.
void cSatMgr::disp(int mode)
{
    u32 i;
    u32 sel;

    GXSetLineWidth(6, 0);
    sel = (mode >> 8) & 0xFF0000;
    for (i = 0; i < nArray; i++) {
        cSat* sat = fastAt(i);
        int s;
        int e;
        int j;
        if (!VALID_PTR(sat)) {
            continue;
        }
        if (!sat->isAlive()) {
            continue;
        }
        s = 0;
        e = 0;
        switch ((u32) mode & 0xF) {
        case 0:
            s = 0;
            e = sat->polygon_num;
            break;
        case 1:
            s = 0;
            e = sat->floor_num;
            break;
        case 2:
            s = sat->floor_num;
            e = s + sat->slope_num;
            break;
        case 3:
            s = sat->polygon_num - sat->wall_num;
            e = sat->polygon_num;
            break;
        }
        for (j = s; j < e; j++) {
            AtPoly* poly = (AtPoly*) (j * sizeof(AtPoly) + (u32) sat->poly_p);
            u32 attr = (poly->attrHi & 0xFF) << 16;
            attr |= poly->attrLo;
            u32 color;
            int z;
            if (attr != 0) {
                color = attr | 0x40000000;
                if (sel != 0) {
                    color = 0;
                    if (attr & (1 << sel)) {
                        color = 0x80808080;
                    }
                }
                z = 1;
            } else {
                color = 0xA0A0A0A0;
                if (sel == 0) {
                    color = 0xFFFFFFFF;
                }
                z = 0;
            }
            sat->disp(j, color, z);
        }
    }
}

// Binds the piece to SAT file `f` (table pointers via operator=), places it and links the block
// tree; m_Flag bit2 (active) set.
void cSat::init(cSatFile* pSf, Vec* pos, Vec* ang)
{
    if (!VALID_PTR(pSf)) {
        pLog->err(0, 0, "cSat::init() PTR ERR %08X", pSf);
        return;
    }
    if (!pSf->dataCheck()) {
        pLog->err(0, 0, "ATARI DATA ERROR 0x%08x", pSf);
    }
    m_Flag.reset();
    m_Flag.set(FLAG_ENABLE);
    *this = pSf;
    setCoord(pos, ang);
    blockInit(block_p);
}

// Places the piece: mat from rot / pos and its inverse for world -> local queries.
void cSat::setCoord(Vec* pos, Vec* ang)
{
    RotMatrix(mat, ang);
    TransMatrix(mat, pos);
    PSMTXInverse(mat, imat);
}

// Places the piece with a full matrix (and its inverse).
void cSat::setMatrix(Mtx mat0)
{
    memcpy(mat, mat0, sizeof(Mtx));
    PSMTXInverse(mat, imat);
}

cSat& cSat::operator=(cSatFile* f)
{
    Vec* v;

    vertex_num = f->m_nVertex;
    polygon_num = f->m_nPolygon;
    normal_num = f->m_nNormal;
    edge_num = f->m_nEdge;
    floor_num = f->m_nFloor;
    slope_num = f->m_nSlope;
    wall_num = f->m_nWall;
    bb_num = f->m_nBlock;
    v = f->getVertexPtr();
    pFile = f;
    x5C = 0;
    vtx = v;
    norm_p = v + vertex_num;
    edge_p = norm_p + normal_num;
    poly_p = (AtPoly*) (edge_p + edge_num);
    block_p = (cSatBlock*) (poly_p + polygon_num);
    return *this;
}

// Turn the relative block links of the file into pointers (once).
void cSat::blockInit(cSatBlock* pBlock)
{
    if (!VALID_PTR(pBlock)) {
        pLog->err(0, 0, "cSat::blockInit() INVALID PTR 0x%08x", pBlock);
        return;
    }
    if (VALID_PTR(pBlock->m_pList)) {
        return;
    }
    do {
        if (pBlock->m_Flag & 1) {
            blockInit((cSatBlock*) pBlock->idx);
        }
        {
            u32 ofs = (u32) pBlock->m_pList;
            if (ofs != 0) {
                cSatBlock* p = (cSatBlock*) ((u8*) pBlock + ofs);
                if (p != 0 && !VALID_PTR(p)) {
                    pLog->err(0, 0, "cSat::blockInit() INVALID PTR 0x%08x ( %08x )", p, ofs);
                    return;
                }
                pBlock->m_pList = p;
            }
        }
        pBlock = pBlock->m_pList;
    } while (pBlock);
}

// Debug draw of polygon `no`: outline (zupd bits 0-1 == 0, edges flagged in e[0] bits 13-15
// in grey) or filled (== 1), plus the face normal (white when it faces the camera).
void cSat::disp(int poly_num, u32 col, int mode)
{
    Vec p[3];
    Mtx m;
    Vec n;
    Vec w;
    AtPoly* pt = poly_p;
    Vec* vt = vtx;
    u16 i;

    PSMTXConcat(pG->Camera.v_mat, mat, m);
    for (i = 0; i < 3; i++) {
        AtPoly* pl = (AtPoly*) (poly_num * sizeof(AtPoly) + (u32) pt);
        Vec* v = (Vec*) (*(u16*) (i * 2 + (u32) pl) * sizeof(Vec) + (u32) vt);
        p[i].x = v->x;
        p[i].y = v->y;
        p[i].z = v->z;
    }
    switch (mode & 3) {
    case 0: {
        AtPoly* pl = (AtPoly*) (poly_num * sizeof(AtPoly) + (u32) pt);
        Draw_line3d_local(&p[0], &p[1], m, (pl->e[0] & 0x2000) ? 0x80808080 : col, 0);
        Draw_line3d_local(&p[1], &p[2], m, (pl->e[0] & 0x4000) ? 0x80808080 : col, 0);
        Draw_line3d_local(&p[0], &p[2], m, (pl->e[0] & 0x8000) ? 0x80808080 : col, 0);
        break;
    }
    case 1:
        Draw_poly_local(p, m, col, 1);
        break;
    }
    PSVECAdd(&p[0], &p[1], &p[0]);
    col = 0xFF;
    PSVECAdd(&p[0], &p[2], &p[0]);
    PSVECScale(&p[0], &p[0], 1.0f / 3.0f);
    {
        AtPoly* pl = (AtPoly*) (poly_num * sizeof(AtPoly) + (u32) poly_p);
        n.x = norm_p[pl->n].x;
        n.y = norm_p[pl->n].y;
        n.z = norm_p[pl->n].z;
    }
    PSVECScale(&n, &p[1], 100.0f);
    PSVECAdd(&p[0], &p[1], &p[1]);
    PSMTXMultVec(mat, &p[0], &w);
    PSVECSubtract(&w, &pG->Camera.param.pos, &w);
    PSMTXMultVecSR(mat, &n, &n);
    if (PSVECDotProduct(&n, &w) > 0.0f) {
        col = 0xFFFFFFFF;
    }
    Draw_line3d_local(&p[0], &p[1], m, col, 0);
}

// The vertex table right after the file header.
Vec* cSatFile::getVertexPtr()
{
    return (Vec*) (this + 1);
}

// Sanity check: at most 0x1FFF polygons (the polyBit table size).
int cSatFile::dataCheck()
{
    return m_nPolygon <= 0x1FFF;
}

// SAT `no` of a multi-SAT archive (offset table after the header).
cSatFile* cSatHeader::getSat(int no)
{
    u32* tbl = ofs;

    return (cSatFile*) ((u8*) this + *(u32*) (no * 4 + (u32) tbl));
}

// The three builders below with precomputed normals were dead-stripped by the linker; their
// tables, strings and constant pools stayed in .rodata.
static cSatFile* createSat2(cSat* sat, Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[8] = {
        { { 5, 4, 1 }, 0, { 0, 1, 2 } },
        { { 4, 0, 1 }, 0, { 3, 4, 5 } },
        { { 6, 7, 3 }, 1, { 6, 7, 8 } },
        { { 3, 2, 6 }, 1, { 9, 10, 11 } },
        { { 7, 5, 1 }, 2, { 12, 13, 14 } },
        { { 1, 3, 7 }, 2, { 15, 16, 17 } },
        { { 4, 6, 2 }, 3, { 18, 19, 20 } },
        { { 2, 0, 4 }, 3, { 21, 22, 23 } },
    };
    static const Vec norm0[6] = {
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },  { 0.0f, -1.0f, 0.0f },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    u32 i;

    if (!VALID_PTR(sat)) {
        pLog->err(0, 0, "createSat() INVALID PTR %08X", sat);
        return 0;
    }
    f = (cSatFile*) MEM_ALLOC(0x298, 1, 13);
    if (h == 0.0f) {
        return 0;
    }
    vtx = (Vec*) (f + 1);
    nrm = &vtx[8];
    for (i = 0; i < 6; i++) {
        nrm[i].x = norm0[i].x * 1.1f;
        nrm[i].y = norm0[i].y * 0.1f;
        nrm[i].z = norm0[i].z * 2.2f;
    }
    memcpy(&nrm[6], poly0, sizeof(poly0));
    return f;
}

// Dead-stripped variant of createBoxSat with precomputed (scaled) normals; only its tables
// survive in .rodata.
static cSatFile* createBoxSat2(cSat* sat, Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[12] = {
        { { 1, 0, 2 }, 4, { 0, 1, 2 } },
        { { 3, 1, 2 }, 4, { 3, 4, 5 } },
        { { 5, 4, 1 }, 0, { 6, 7, 8 } },
        { { 4, 0, 1 }, 0, { 9, 10, 11 } },
        { { 6, 7, 3 }, 1, { 12, 13, 14 } },
        { { 3, 2, 6 }, 1, { 15, 16, 17 } },
        { { 7, 5, 1 }, 2, { 18, 19, 20 } },
        { { 1, 3, 7 }, 2, { 21, 22, 23 } },
        { { 4, 6, 2 }, 3, { 24, 25, 26 } },
        { { 2, 0, 4 }, 3, { 27, 28, 29 } },
        { { 4, 5, 6 }, 5, { 30, 31, 32 } },
        { { 7, 6, 5 }, 5, { 33, 34, 35 } },
    };
    static const Vec norm0[6] = {
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },  { 0.0f, -1.0f, 0.0f },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    u32 i;

    if (!VALID_PTR(sat)) {
        pLog->err(0, 0, "createSat() INVALID PTR %08X", sat);
        return 0;
    }
    f = (cSatFile*) MEM_ALLOC(0x398, 1, 13);
    if (h == 0.0f) {
        return 0;
    }
    vtx = (Vec*) (f + 1);
    nrm = &vtx[8];
    for (i = 0; i < 6; i++) {
        nrm[i].x = norm0[i].x * 1.1f;
        nrm[i].y = norm0[i].y * 0.1f;
        nrm[i].z = norm0[i].z * 2.2f;
    }
    memcpy(&nrm[6], poly0, sizeof(poly0));
    return f;
}

// Dead-stripped variant of createFloorSat; only its tables survive.
static cSatFile* createFloorSat2(cSat* sat, Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[2] = {
        { { 1, 0, 2 }, 0, { 0, 1, 2 } },
        { { 3, 1, 2 }, 0, { 3, 4, 5 } },
    };
    static const Vec norm0[1] = {
        { 0.0f, 1.0f, 0.0f },
    };
    static int floor_check = 0;
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    u32 i;

    if (!VALID_PTR(sat)) {
        pLog->err(0, 0, "createSat() INVALID PTR %08X", sat);
        return 0;
    }
    f = (cSatFile*) MEM_ALLOC(0xE8, 1, 13);
    if (h == 0.0f || floor_check) {
        return 0;
    }
    vtx = (Vec*) (f + 1);
    nrm = &vtx[4];
    for (i = 0; i < 1; i++) {
        nrm[i].x = norm0[i].x * 1.1f;
        nrm[i].y = norm0[i].y * 0.1f;
        nrm[i].z = norm0[i].z * 2.2f;
    }
    memcpy(&nrm[1], poly0, sizeof(poly0));
    return f;
}

// Wall piece over the 4-corner polygon v (closed side box of height h, no top/bottom).
cSatFile* createSat(Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[8] = {
        { { 5, 2, 1 }, 0, { 0, 1, 2 } },
        { { 2, 5, 6 }, 0, { 3, 4, 5 } },
        { { 3, 4, 0 }, 1, { 6, 7, 8 } },
        { { 3, 7, 4 }, 1, { 9, 10, 11 } },
        { { 0, 4, 1 }, 2, { 12, 13, 14 } },
        { { 1, 4, 5 }, 2, { 15, 16, 17 } },
        { { 2, 7, 3 }, 3, { 18, 19, 20 } },
        { { 2, 6, 7 }, 3, { 21, 22, 23 } },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    Vec* e;
    AtPoly* poly;
    cSatBlock* blk;
    const AtPoly* p;
    u32 i;

#line 2621 "D:/Bio4/Prog/atari.cpp"
    f = (cSatFile*) MEM_ALLOC(0x298, 1, 13);
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "createSat() memory alloc failed.");
        return 0;
    }
    f->m_Version = 0xFF;
    f->m_nVertex = 8;
    f->m_nNormal = 4;
    f->m_nEdge = 24;
    f->m_nPolygon = 8;
    f->m_nFloor = 0;
    f->m_nSlope = 0;
    f->m_nWall = 8;
    f->m_nBlock = 1;
    vtx = (Vec*) (f + 1);
    vtx[0] = v[0];
    vtx[1] = v[1];
    vtx[2] = v[2];
    vtx[3] = v[3];
    vtx[4] = v[0];
    vtx[4].y += h;
    vtx[5] = v[1];
    vtx[5].y += h;
    vtx[6] = v[2];
    vtx[6].y += h;
    vtx[7] = v[3];
    vtx[7].y += h;
    nrm = &vtx[8];
    for (i = 0; i < 4; i++) {
        Vec d0;
        Vec d1;
        Vec* v0;
        p = &poly0[i * 2];
        v0 = &vtx[p->v[0]];
        PSVECSubtract(&vtx[p->v[1]], v0, &d0);
        PSVECSubtract(&vtx[p->v[2]], v0, &d1);
        PSVECCrossProduct(&d0, &d1, &nrm[i]);
#line 2661 "D:/Bio4/Prog/atari.cpp"
        VECNormalize(&nrm[i], &nrm[i]);
    }
    e = &nrm[4];
    for (i = 0; i < 8; i++) {
        Vec* v1 = (Vec*) (poly0[i].v[1] * sizeof(Vec) + (u32) vtx);
        Vec* v0 = (Vec*) (poly0[i].v[0] * sizeof(Vec) + (u32) vtx);
        Vec* v2 = (Vec*) (poly0[i].v[2] * sizeof(Vec) + (u32) vtx);
        e->x = v1->x - v0->x;
        e->y = v1->y - v0->y;
        e->z = v1->z - v0->z;
        e++;
        e->x = v2->x - v1->x;
        e->y = v2->y - v1->y;
        e->z = v2->z - v1->z;
        e++;
        e->x = v0->x - v2->x;
        e->y = v0->y - v2->y;
        e->z = v0->z - v2->z;
        e++;
    }
    poly = (AtPoly*) e;
    memcpy(poly, poly0, sizeof(poly0));
    for (i = 0; i < 8; i++) {
        poly[i].attr = attr;
    }
    blk = (cSatBlock*) (poly + 8);
    blk->min = vtx[0];
    blk->m_Size = vtx[0];
    for (i = 1; i < 4; i++) {
        if (vtx[i].x < blk->min.x) {
            blk->min.x = vtx[i].x - 10.0f;
        }
        if (vtx[i].z < blk->min.z) {
            blk->min.z = vtx[i].z - 10.0f;
        }
        if (vtx[i].x > blk->m_Size.x) {
            blk->m_Size.x = vtx[i].x + 10.0f;
        }
        if (vtx[i].z > blk->m_Size.z) {
            blk->m_Size.z = vtx[i].z + 10.0f;
        }
    }
    blk->m_Size.x -= blk->min.x;
    blk->m_Size.z -= blk->min.z;
    blk->m_nFloor = 0;
    blk->m_nSlope = 0;
    blk->m_nWall = 8;
    blk->m_Flag = 0;
    blk->m_pList = 0;
    for (i = 0; i < 8; i++) {
        blk->idx[i] = i;
    }
    return f;
}

// Closed box piece over the 4-corner polygon v (height h): floor, walls and ceiling groups.
cSatFile* createBoxSat(Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[12] = {
        { { 4, 6, 5 }, 0, { 0, 1, 2 } },
        { { 6, 4, 7 }, 0, { 3, 4, 5 } },
        { { 0, 1, 2 }, 1, { 6, 7, 8 } },
        { { 0, 2, 3 }, 1, { 9, 10, 11 } },
        { { 5, 2, 1 }, 2, { 12, 13, 14 } },
        { { 2, 5, 6 }, 2, { 15, 16, 17 } },
        { { 3, 4, 0 }, 3, { 18, 19, 20 } },
        { { 3, 7, 4 }, 3, { 21, 22, 23 } },
        { { 0, 4, 1 }, 4, { 24, 25, 26 } },
        { { 1, 4, 5 }, 4, { 27, 28, 29 } },
        { { 2, 7, 3 }, 5, { 30, 31, 32 } },
        { { 2, 6, 7 }, 5, { 33, 34, 35 } },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    Vec* e;
    AtPoly* poly;
    cSatBlock* blk;
    const AtPoly* p;
    u32 i;

#line 2757 "D:/Bio4/Prog/atari.cpp"
    f = (cSatFile*) MEM_ALLOC(0x398, 1, 13);
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "createSat() memory alloc failed.");
        return 0;
    }
    f->m_Version = 0xFF;
    f->m_nVertex = 8;
    f->m_nNormal = 6;
    f->m_nEdge = 36;
    f->m_nPolygon = 12;
    f->m_nFloor = 0;
    f->m_nSlope = 4;
    f->m_nWall = 8;
    f->m_nBlock = 1;
    vtx = (Vec*) (f + 1);
    vtx[0] = v[0];
    vtx[1] = v[1];
    vtx[2] = v[2];
    vtx[3] = v[3];
    vtx[4] = v[0];
    vtx[4].y += h;
    vtx[5] = v[1];
    vtx[5].y += h;
    vtx[6] = v[2];
    vtx[6].y += h;
    vtx[7] = v[3];
    vtx[7].y += h;
    nrm = &vtx[8];
    for (i = 0; i < 6; i++) {
        Vec d0;
        Vec d1;
        Vec* v0;
        p = &poly0[i * 2];
        v0 = &vtx[p->v[0]];
        PSVECSubtract(&vtx[p->v[1]], v0, &d0);
        PSVECSubtract(&vtx[p->v[2]], v0, &d1);
        PSVECCrossProduct(&d0, &d1, &nrm[i]);
#line 2797 "D:/Bio4/Prog/atari.cpp"
        VECNormalize(&nrm[i], &nrm[i]);
    }
    e = &nrm[6];
    for (i = 0; i < 12; i++) {
        Vec* v1 = (Vec*) (poly0[i].v[1] * sizeof(Vec) + (u32) vtx);
        Vec* v0 = (Vec*) (poly0[i].v[0] * sizeof(Vec) + (u32) vtx);
        Vec* v2 = (Vec*) (poly0[i].v[2] * sizeof(Vec) + (u32) vtx);
        e->x = v1->x - v0->x;
        e->y = v1->y - v0->y;
        e->z = v1->z - v0->z;
        e++;
        e->x = v2->x - v1->x;
        e->y = v2->y - v1->y;
        e->z = v2->z - v1->z;
        e++;
        e->x = v0->x - v2->x;
        e->y = v0->y - v2->y;
        e->z = v0->z - v2->z;
        e++;
    }
    poly = (AtPoly*) e;
    memcpy(poly, poly0, sizeof(poly0));
    for (i = 0; i < 12; i++) {
        poly[i].attr = attr;
    }
    blk = (cSatBlock*) (poly + 12);
    blk->min = vtx[0];
    blk->m_Size = vtx[0];
    for (i = 1; i < 4; i++) {
        if (vtx[i].x < blk->min.x) {
            blk->min.x = vtx[i].x - 10.0f;
        }
        if (vtx[i].z < blk->min.z) {
            blk->min.z = vtx[i].z - 10.0f;
        }
        if (vtx[i].x > blk->m_Size.x) {
            blk->m_Size.x = vtx[i].x + 10.0f;
        }
        if (vtx[i].z > blk->m_Size.z) {
            blk->m_Size.z = vtx[i].z + 10.0f;
        }
    }
    blk->m_Size.x -= blk->min.x;
    blk->m_Size.z -= blk->min.z;
    blk->m_nFloor = 0;
    blk->m_nSlope = 4;
    blk->m_nWall = 8;
    blk->m_Flag = 0;
    blk->m_pList = 0;
    for (i = 0; i < 12; i++) {
        blk->idx[i] = i;
    }
    return f;
}

// Floor piece: the 4-corner polygon v as two triangles.
static cSatFile* createFloorSat(Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[2] = {
        { { 0, 2, 1 }, 0, { 0, 1, 2 } },
        { { 2, 0, 3 }, 0, { 3, 4, 5 } },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    Vec* e;
    AtPoly* poly;
    cSatBlock* blk;
    const AtPoly* p;
    u32 i;

#line 2873 "D:/Bio4/Prog/atari.cpp"
    f = (cSatFile*) MEM_ALLOC(0xE8, 1, 13);
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "createSat() memory alloc failed.");
        return 0;
    }
    f->m_Version = 0xFF;
    f->m_nVertex = 4;
    f->m_nNormal = 1;
    f->m_nEdge = 6;
    f->m_nPolygon = 2;
    f->m_nFloor = 2;
    f->m_nSlope = 0;
    f->m_nWall = 0;
    f->m_nBlock = 1;
    vtx = (Vec*) (f + 1);
    vtx[0] = v[0];
    vtx[1] = v[1];
    vtx[2] = v[2];
    vtx[3] = v[3];
    nrm = &vtx[4];
    for (i = 0; i < 1; i++) {
        Vec d0;
        Vec d1;
        Vec* v0;
        p = &poly0[i * 2];
        v0 = &vtx[p->v[0]];
        PSVECSubtract(&vtx[p->v[1]], v0, &d0);
        PSVECSubtract(&vtx[p->v[2]], v0, &d1);
        PSVECCrossProduct(&d0, &d1, &nrm[i]);
#line 2910 "D:/Bio4/Prog/atari.cpp"
        VECNormalize(&nrm[i], &nrm[i]);
    }
    e = &nrm[1];
    for (i = 0; i < 2; i++) {
        Vec* v1 = (Vec*) (poly0[i].v[1] * sizeof(Vec) + (u32) vtx);
        Vec* v0 = (Vec*) (poly0[i].v[0] * sizeof(Vec) + (u32) vtx);
        Vec* v2 = (Vec*) (poly0[i].v[2] * sizeof(Vec) + (u32) vtx);
        e->x = v1->x - v0->x;
        e->y = v1->y - v0->y;
        e->z = v1->z - v0->z;
        e++;
        e->x = v2->x - v1->x;
        e->y = v2->y - v1->y;
        e->z = v2->z - v1->z;
        e++;
        e->x = v0->x - v2->x;
        e->y = v0->y - v2->y;
        e->z = v0->z - v2->z;
        e++;
    }
    poly = (AtPoly*) e;
    memcpy(poly, poly0, sizeof(poly0));
    for (i = 0; i < 2; i++) {
        poly[i].attr = attr;
    }
    blk = (cSatBlock*) (poly + 2);
    blk->min = vtx[0];
    blk->m_Size = vtx[0];
    for (i = 1; i < 4; i++) {
        if (vtx[i].x < blk->min.x) {
            blk->min.x = vtx[i].x - 10.0f;
        }
        if (vtx[i].z < blk->min.z) {
            blk->min.z = vtx[i].z - 10.0f;
        }
        if (vtx[i].x > blk->m_Size.x) {
            blk->m_Size.x = vtx[i].x + 10.0f;
        }
        if (vtx[i].z > blk->m_Size.z) {
            blk->m_Size.z = vtx[i].z + 10.0f;
        }
    }
    blk->m_Size.x -= blk->min.x;
    blk->m_Size.z -= blk->min.z;
    blk->m_nFloor = 2;
    blk->m_nSlope = 0;
    blk->m_nWall = 0;
    blk->m_Flag = 0;
    blk->m_pList = 0;
    for (i = 0; i < 2; i++) {
        blk->idx[i] = i;
    }
    return f;
}

// Move the model (and its parts' world matrices) by d after a collision push.
void at_pos_calc(cModel* pMod, Vec* vec)
{
    cModel* c = pMod->pParts;

    if (PSVECMag(vec) != 0.0f) {
        PSVECAdd(&pMod->pos, vec, &pMod->pos);
        while (c) {
            PSVECAdd(&c->world, vec, &c->world);
            c->mat[0][3] += vec->x;
            c->mat[1][3] += vec->y;
            c->mat[2][3] += vec->z;
            c = c->pParts;
        }
        pMod->mat[0][3] = pMod->pos.x;
        pMod->mat[1][3] = pMod->pos.y;
        pMod->mat[2][3] = pMod->pos.z;
        ((cEm*) pMod)->Motion.Pos_world = pMod->pos;
    } else {
        pMod->mat[0][3] = pMod->pos.x;
        pMod->mat[1][3] = pMod->pos.y;
        pMod->mat[2][3] = pMod->pos.z;
    }
}

cSatMgr SatMgr;
cEatMgr EatMgr;
