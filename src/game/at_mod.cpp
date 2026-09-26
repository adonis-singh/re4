// game/at_mod.cpp: character-to-character collision and the hit box ("yarare") setup. EmAtCheck
// pushes a character's cAtariInfo body (cylinder or yaw-aligned box) out of every other
// character's and object's body by push priority; EmHitCheck / ObjHitCheck trace a line against
// those bodies (camera, aiming). The Yarare* functions build the YARARE_INFO chain of hit boxes
// weapons test (em_sub.cpp).

#include "atari.h"
#include "at_mod.h"
#include "at_sub.h"
#include "emhit.h"
#include "obj.h"
#include "player.h"
#include "global.h"
#include "math_sub.h"
#include "motion.h"
#include "db_log.h"
#include "em_sub.h"

extern "C" {
void yarareInit0(YARARE_INFO* y, f32 x, f32 yy, f32 z, f32 w, f32 h, s16 no, u16 flags);
static bool priorityCheck(cModel* pMod, cModel* pMod2);
static u32 sphereRectCk(cAtariInfo* info, Vec& p, f32 rad);
}

// Matrix copy written out as loops. The row counter is a do-while starting at 2 (`i_-- != 0`): the
// original's counter of ComnHitCheck's first copy is live across the getPartsPtr call (callee-saved
// r30), which a `while (i_--)` from 3 cannot give (cse folds the peeled test and re-materialises
// `li 2` after the call). The row pointers step destination first (`d_++; s_++;`): that LUID order
// gives the target's `addi d; addi s` pairs and keeps the second copy's `sp_ = *s_` a separate
// register copy (s_ r0, sp_ r9) -- ComnHitCheck is byte-identical with it.
// ObaLineHitChk (matching): one PSVECMag call squared (`mag * mag`), tc/s clamped with ternaries (s in
// place: its temporary is copied back into f30), den anchored in f0 (anchor gated by `de * ef` so the
// hoisted `mr r3,r27` keeps its slot), and the getPartsPtr `if` written with an explicit `else pm = m`
// so cse's extended block ends at the join and the `&p0` argument after the call stays a fresh
// `addi r4,r1,8` (with `pm = m` hoisted before the `if`, cse skips the arm and folds `&p0` into the
// copy's address pseudo).
// MTX_COPY (vec.h) as a do/while with `i_ = 2` between the two pointer inits: the counter `li` is
// issued between the `d_` and `s_` inits (ComnHitCheck).
#define MTX_COPY_DO(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        int i_ = 2;                      \
        MtxPtr s_ = (src);               \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        do {                             \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        } while (i_--);                  \
    }

// Fills one hit box: offset, width (radius) / height, the parts it follows (1-based, 0 = model),
// flags; not linked.
void yarareInit0(YARARE_INFO* y, f32 x, f32 yy, f32 z, f32 w, f32 h, s16 no, u16 flags)
{
    y->offset.x = x;
    y->offset.y = yy;
    y->offset.z = z;
    y->radius = w;
    y->height = h;
    y->parts_no = no;
    y->flag = flags;
    y->pList = 0;
}

// Sets the character's primary hit box (cEm::hitInfo) as a cylinder of radius w / height h.
void YarareInit(cEm* pEm, f32 x, f32 y, f32 z, f32 r, f32 h, s16 pno, u16 flag)
{
    yarareInit0(&pEm->hitInfo, x, y, z, r, h, pno, flag);
}

// Sets the primary hit box as a box (flags bit3): half sizes w (x) / h (y) / extent (z).
void YarareInitCube(cEm* em, f32 x, f32 y, f32 z, f32 w, f32 h, f32 extent, s16 no, u16 flags)
{
    yarareInit0(&em->hitInfo, x, y, z, w, h, no, flags);
    em->hitInfo.extent = extent;
    em->hitInfo.flag |= 8;
}

// Appends a cylinder hit box to the character's hit box chain (error when already linked).
void YarareAdd(cEm* pEm, YARARE_INFO* pAt, f32 x, f32 y, f32 z, f32 r, f32 h, s16 pno, u16 flag)
{
    YARARE_INFO* p = &pEm->hitInfo;

    yarareInit0(pAt, x, y, z, r, h, pno, flag);
    for (; p->pList != 0; p = p->pList) {
        if (p == pAt) {
            pLog->err(0, 0, "YarareAdd() Add same pointer, EM%02x", pEm->id);
            return;
        }
    }
    if (p == pAt) {
        pLog->err(0, 0, "YarareAdd() Add same pointer, EM%02x", pEm->id);
        return;
    }
    p->pList = pAt;
}

// Appends a box hit box to the chain.
void YarareAddCube(cEm* em, YARARE_INFO* box, f32 x, f32 y, f32 z, f32 w, f32 h, f32 extent, s16 no, u16 flags)
{
    YARARE_INFO* p;

    yarareInit0(box, x, y, z, w, h, no, flags);
    box->extent = extent;
    box->flag |= YAT_FLAG_CUBE;
    p = &em->hitInfo;
    while (p->pList != 0) {
        if (p == box) {
            pLog->err(0, 0, "YarareAdd() Add same pointer, EM%02x", em->id);
        }
        p = p->pList;
    }
    if (p->pList != box) {
        p->pList = box;
    }
}

// Character-vs-character collision for `em` (called from its move): refreshes every collidable
// body's world position (m_flag 0x200, non-zero radius), pushes `em` against every other
// character and then every object (__em_at_core), stores the old positions and recomputes the
// parts world positions. A body without collision only marks m_stat bit0.
void EmAtCheck(cModel* pMod)
{
    cEm* m;
    cObj* o;

    if (!(pMod->atari.m_flag & 0x200) || pMod->atari.m_radius2 == 0.0f) {
        pMod->atari.m_stat |= 1;
        return;
    }
    pMod->atari.getPos(pMod, &pMod->atari.m_Pos);
    for (m = EmMgr.getActiveWork(); m != 0; m = EmMgr.getNext(m)) {
        if ((m->atari.m_flag & 0x200) && m->atari.m_radius2 != 0.0f) {
            m->atari.getPos(m, &m->atari.m_Pos);
        }
    }
    for (m = EmMgr.getActiveWork(); m != 0; m = EmMgr.getNext(m)) {
        if ((m->atari.m_flag & 0x200) && m != pMod && m->atari.m_radius2 != 0.0f) {
            __em_at_core(pMod, m);
        }
    }
    for (m = EmMgr.getActiveWork(); m != 0; m = EmMgr.getNext(m)) {
        if ((m->atari.m_flag & 0x200) && m->atari.m_radius2 != 0.0f) {
            m->atari.m_oldPos = m->atari.m_Pos;
        }
    }
    for (o = ObjMgr.getActiveWork(); o != 0; o = ObjMgr.getNext(o)) {
        if ((o->atari.m_flag & 0x200) && o->atari.m_radius2 != 0.0f) {
            o->atari.getPos(o, &o->atari.m_Pos);
        }
    }
    for (o = ObjMgr.getActiveWork(); o != 0; o = ObjMgr.getNext(o)) {
        if ((o->atari.m_flag & 0x200) && o != pMod && o->atari.m_radius2 != 0.0f) {
            __em_at_core(pMod, o);
        }
    }
    for (o = ObjMgr.getActiveWork(); o != 0; o = ObjMgr.getNext(o)) {
        if ((o->atari.m_flag & 0x200) && o->atari.m_radius2 != 0.0f) {
            o->atari.m_oldPos = o->atari.m_Pos;
        }
    }
    PartsWorldPosCalc(pMod);
    pMod->atari.m_stat &= ~1;
}

// 1 when pMod's push priority (m_flag bits 3-4) is non-zero and not below pMod2's, i.e. pMod is
// not the one to be pushed.
static bool priorityCheck(cModel* pMod, cModel* pMod2)
{
    u8 pa = pMod->atari.m_flag & 0x18;
    u8 pb = pMod2->atari.m_flag & 0x18;

    if (pa != 0 && pa >= pb) {
        return 1;
    }
    return 0;
}

// Pushes pMod out of pMod2 unless priority says otherwise, choosing the box-box, sphere-box or
// sphere-sphere test by the bodies' m_flag bit1.
void __em_at_core(cModel* pMod, cModel* pMod2)
{
    if (priorityCheck(pMod, pMod2) == 1) {
        return;
    }
    if (pMod->atari.m_flag & 2) {
        if (pMod2->atari.m_flag & 2) {
            At_em_rect_rect_ck(pMod, pMod2);
        } else {
            At_em_sphere_rect_ck(pMod2, pMod);
        }
    } else {
        if (pMod2->atari.m_flag & 2) {
            At_em_sphere_rect_ck(pMod, pMod2);
        } else {
            At_em_sphere_sphere_ck(pMod, pMod2);
        }
    }
}

// Box vs box (both yaw-aligned): when the XZ boxes overlap, the moving one is pushed out along
// the axis it moved on (x or z), the other when pMod has priority. 1 when pMod moved >= 1 unit.
u32 At_em_rect_rect_ck(cModel* pMod, cModel* pMod2)
{
    Vec ra;
    Vec rb;
    Vec posA;
    f32 aw;
    f32 ad;
    f32 bw;
    f32 bd;
    u8 pa;
    u8 pb;
    int ret;

    ret = em_rect2_ck_sub(pMod, pMod2);
    if (ret != 0) {
    posA = pMod->pos;
    if (!(Get_ang_dir(pMod->ang.y) & 1)) {
        aw = pMod->atari.m_radius;
        ad = pMod->atari.m_radius2;
    } else {
        aw = pMod->atari.m_radius2;
        ad = pMod->atari.m_radius;
    }
    RotVector(&pMod->atari.m_offset, &pMod->ang, &ra);
    if (!(Get_ang_dir(pMod2->ang.y) & 1)) {
        bw = pMod2->atari.m_radius;
        bd = pMod2->atari.m_radius2;
    } else {
        bw = pMod2->atari.m_radius2;
        bd = pMod2->atari.m_radius;
    }
    RotVector(&pMod2->atari.m_offset, &pMod2->ang, &rb);
    pa = pMod->atari.m_flag & 0x18;
    pb = pMod2->atari.m_flag & 0x18;
    if (pa > pb || (pa == 0 && pb == 0)) {
        if (pMod->pos.x != pMod->pos_old.x) {
            f32 ax = pMod->pos.x + ra.x;
            f32 bx = pMod2->pos.x + rb.x;
            if (ax < bx) {
                pMod2->pos.x = ax + aw + bw + 1.0f - rb.x;
            } else {
                pMod2->pos.x = ax - aw - bw - 1.0f - rb.x;
            }
        } else if (pMod->pos.z != pMod->pos_old.z) {
            f32 az = pMod->pos.z + ra.z;
            f32 bz = pMod2->pos.z + rb.z;
            if (az > bz) {
                pMod2->pos.z = pMod->pos.z + rb.z + ad + bd + 1.0f - rb.z;
            } else {
                pMod2->pos.z = pMod->pos.z + rb.z - ad - bd - 1.0f - rb.z;
            }
        }
    } else if (pa != pb) {
        if (pMod->pos.x != pMod->pos_old.x) {
            f32 ax = pMod->pos.x + ra.x;
            f32 bx = pMod2->pos.x + rb.x;
            if (ax > bx) {
                pMod->pos.x = bx + bw + aw + 1.0f - ra.x;
            } else {
                pMod->pos.x = bx - bw - aw - 1.0f - ra.x;
            }
        } else if (pMod->pos.z != pMod->pos_old.z) {
            f32 az = pMod->pos.z + ra.z;
            f32 bz = pMod2->pos.z + rb.z;
            if (az > bz) {
                pMod->pos.z = bz + bd + ad + 1.0f - ra.z;
            } else {
                pMod->pos.z = bz - bd - ad - 1.0f - ra.z;
            }
        }
    }
    if (fabsf(pMod->pos.x - posA.x) >= 1.0f || fabsf(pMod->pos.z - posA.z) >= 1.0f) {
        ret = 1;
    } else {
        ret = 0;
    }
    }
    return ret;
}

// Box vs box with rotation: tests pMod's four corners against pMod2's box and pushes it out of
// the deepest one.
int em_rect2_ck_sub(cModel* pMod, cModel* pMod2)
{
    Vec v;
    Vec ra[4];
    Vec rb[4];
    cAtariInfo* ia = &pMod->atari;
    cAtariInfo* ib = &pMod2->atari;
    int ret;

    {
        f32 rx = ia->m_radius;
        f32 rz = ia->m_radius2;
        v.x = rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ia->m_offset, &v);
        RotVector(&v, &pMod->ang, &v);
        PSVECAdd(&v, &pMod->pos, &ra[0]);
        v.x = rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ia->m_offset, &v);
        RotVector(&v, &pMod->ang, &v);
        PSVECAdd(&v, &pMod->pos, &ra[1]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ia->m_offset, &v);
        RotVector(&v, &pMod->ang, &v);
        PSVECAdd(&v, &pMod->pos, &ra[2]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ia->m_offset, &v);
        RotVector(&v, &pMod->ang, &v);
        PSVECAdd(&v, &pMod->pos, &ra[3]);
    }
    {
        f32 rx = ib->m_radius;
        f32 rz = ib->m_radius2;
        v.x = rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ib->m_offset, &v);
        RotVector(&v, &pMod2->ang, &v);
        PSVECAdd(&v, &pMod2->pos, &rb[0]);
        v.x = rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ib->m_offset, &v);
        RotVector(&v, &pMod2->ang, &v);
        PSVECAdd(&v, &pMod2->pos, &rb[1]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ib->m_offset, &v);
        RotVector(&v, &pMod2->ang, &v);
        PSVECAdd(&v, &pMod2->pos, &rb[2]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ib->m_offset, &v);
        RotVector(&v, &pMod2->ang, &v);
        PSVECAdd(&v, &pMod2->pos, &rb[3]);
    }
    if (fabsf(ra[0].y - rb[0].y) > ia->m_height + ib->m_height) {
        ret = 0;
    } else if (At_rect_rect_ck(ra, rb) != 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

// Sphere (character) vs box (object): with overlapping heights pushes the sphere out of the box
// along the nearest face (sphereRectCk in the box's yaw frame); 1 on contact.
u32 At_em_sphere_rect_ck(cModel* pMod, cModel* pMod2)
{
    Vec ps;
    Vec pr;
    Mtx m;
    Mtx inv;
    Vec c;
    Vec step;
    Vec p;
    Vec q;
    cAtariInfo* ir;
    f32 rad;
    int n;
    int i;
    int hit;

    ps = pMod->atari.m_Pos;
    pr = pMod2->atari.m_Pos;
    if (ps.y + pMod->atari.m_height < pr.y - pMod2->atari.m_height) {
        return 0;
    }
    if (ps.y - pMod->atari.m_height > pr.y + pMod2->atari.m_height) {
        return 0;
    }
    ir = &pMod2->atari;
    if (ir->m_parts_no != 0) {
        cParts* pm = pMod2->getPartsPtr(ir->m_parts_no - 1);
        PSMTXRotRad(m, 'y', pm->ang.y);
        PSMTXMultVecSR(m, &ir->m_offset, &c);
        PSVECAdd(&c, &pr, &c);
        TransMatrix(m, &c);
    } else {
        PSMTXRotRad(m, 'y', pMod2->ang.y);
        TransMatrix(m, &pMod2->pos);
        PSMTXMultVec(m, &ir->m_offset, &c);
        m[0][3] = c.x;
        m[1][3] = c.y;
        m[2][3] = c.z;
    }
    PSMTXInverse(m, inv);
    c = pMod->atari.m_Pos;
    rad = pMod->atari.m_radius;
    if (!(pMod->atari.m_stat & 1)) {
        PSMTXMultVec(inv, &pMod->atari.m_Pos, &step);
        PSMTXMultVec(inv, &pMod->atari.m_oldPos, &p);
        if (rad >= 200.0f) {
            n = (int) (GetDistance3(&step, &p) / rad) + 1;
        } else {
            n = 1;
        }
        PSVECSubtract(&step, &p, &step);
        PSVECScale(&step, &step, 1.0f / (f32) n);
    } else {
        PSMTXMultVec(inv, &pMod->atari.m_Pos, &p);
        n = 1;
        step.x = 0.0f;
        step.y = 0.0f;
        step.z = 0.0f;
    }
    hit = 0;
    for (i = 0; i < n; i++) {
        PSVECAdd(&p, &step, &p);
        hit = sphereRectCk(ir, p, rad);
        if (hit != 0) {
            break;
        }
    }
    if (hit != 0) {
        p.y = c.y;
        PSMTXMultVec(m, &p, &q);
        PSVECSubtract(&q, &c, &q);
        q.y = 0.0f;
        PSVECAdd(&pMod->pos, &q, &pMod->pos);
        pMod->atari.getPos(pMod, &pMod->atari.m_Pos);
    }
    return hit;
}

// Sphere of radius `rad` at box-space point `p` against the box `info`: moves `p` to the nearest
// face; 1 when it was inside.
static u32 sphereRectCk(cAtariInfo* pAt2, Vec& ipos, f32 r)
{
    Vec corner;
    Vec d;
    f32 rx = pAt2->m_radius;
    f32 rz = pAt2->m_radius2;
    f32 rr;
    int hit = 0;

    if (ipos.z > -rz && ipos.z < rz) {
        if (ipos.x > 0.0f) {
            if (ipos.x < rx + r) {
                ipos.x = rx + r;
                hit = 1;
            }
        } else {
            if (ipos.x > -(rx + r)) {
                ipos.x = -(rx + r);
                hit = 1;
            }
        }
    }
    if (ipos.x > -rx && ipos.x < rx) {
        if (ipos.z > 0.0f) {
            if (ipos.z < rz + r) {
                ipos.z = rz + r;
                hit = 1;
            }
        } else {
            if (ipos.z > -(rz + r)) {
                ipos.z = -(rz + r);
                hit = 1;
            }
        }
    }
    if (hit == 0) {
        corner.x = rx;
        corner.y = 0.0f;
        corner.z = rz;
        ipos.y = 0.0f;
        rr = r * r;
        if (GetDistance(&corner, &ipos) < rr) {
            PSVECSubtract(&ipos, &corner, &d);
#line 788 "D:/Bio4/Prog/at_mod.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, r);
            hit = 1;
            PSVECAdd(&corner, &d, &ipos);
        } else {
            corner.x = -rx;
            corner.y = 0.0f;
            corner.z = rz;
            if (GetDistance(&corner, &ipos) < rr) {
                PSVECSubtract(&ipos, &corner, &d);
#line 797 "D:/Bio4/Prog/at_mod.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, r);
                hit = 1;
                PSVECAdd(&corner, &d, &ipos);
            } else {
                corner.x = rx;
                corner.y = 0.0f;
                corner.z = -rz;
                if (GetDistance(&corner, &ipos) < rr) {
                    PSVECSubtract(&ipos, &corner, &d);
#line 806 "D:/Bio4/Prog/at_mod.cpp"
                    VECNormalize(&d, &d);
                    PSVECScale(&d, &d, r);
                    hit = 1;
                    PSVECAdd(&corner, &d, &ipos);
                } else {
                    corner.x = -rx;
                    corner.y = 0.0f;
                    corner.z = -rz;
                    if (GetDistance(&corner, &ipos) < rr) {
                        PSVECSubtract(&ipos, &corner, &d);
#line 815 "D:/Bio4/Prog/at_mod.cpp"
                        VECNormalize(&d, &d);
                        PSVECScale(&d, &d, r);
                        hit = 1;
                        PSVECAdd(&corner, &d, &ipos);
                    }
                }
            }
        }
    }
    return hit;
}

// Cylinder vs cylinder: with overlapping heights and XZ distance below the radii sum, pushes
// pMod (and its m_pMod companion, or pMod2's) apart by the overlap; 1 on contact.
u32 At_em_sphere_sphere_ck(cModel* pMod, cModel* pMod2)
{
    Vec pa;
    Vec pb;
    Vec d;
    const f32 rate = 0.3f;
    f32 hh;
    f32 dist;
    f32 rr;
    cModel* m;

    pa = pMod->atari.m_Pos;
    pb = pMod2->atari.m_Pos;
    hh = pMod->atari.m_height + pMod2->atari.m_height;
    if (pa.y < pb.y - hh) {
        return 0;
    }
    if (pa.y > pb.y + hh) {
        return 0;
    }
    PSVECSubtract(&pb, &pa, &d);
    d.y = 0.0f;
    dist = RootSumSquare3(&d);
    rr = pMod->atari.m_radius2 + pMod2->atari.m_radius2;
    if (dist < rr) {
        if (dist < 0.1f) {
            d.x += 0.1f;
        }
#line 869 "D:/Bio4/Prog/at_mod.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, rr - dist);
        PSVECSubtract(&pMod->pos, &d, &pMod->pos);
        pMod->atari.getPos(pMod, &pMod->atari.m_Pos);
        if (pMod->atari.m_pMod != 0) {
            m = pMod->atari.m_pMod;
            PSVECSubtract(&m->pos, &d, &m->pos);
            ((cEm*) m)->atari.getPos(m, &((cEm*) m)->atari.m_Pos);
        }
        if ((pMod2->atari.m_flag & 0x18) == 0 && (u32) pMod2 > (u32) pMod) {
            PSVECScale(&d, &d, rate);
            PSVECAdd(&pMod2->pos, &d, &pMod2->pos);
            pMod2->atari.getPos(pMod2, &pMod2->atari.m_Pos);
            if (pMod2->atari.m_pMod != 0) {
                m = pMod2->atari.m_pMod;
                PSVECAdd(&m->pos, &d, &m->pos);
                ((cEm*) m)->atari.getPos(m, &((cEm*) m)->atari.m_Pos);
            }
        }
        return 1;
    }
    return 0;
}

// Dead-stripped by the original linker: only its constant pool (a double 0.0) survives in .rodata
// between At_em_sphere_sphere_ck's and EmHitCheck's pools. Body unknown.
static int atModIsZero(f32 x)
{
    return x == 0.0;
}

// Line pos0 -> pos1 against every collidable character body (the player only with flag bit2):
// nearest hit and normal; 1 when hit. The camera uses it to keep characters in view.
BOOL EmHitCheck(Vec* ret_cross, Vec* ret_norm, Vec* pos0, Vec* pos1, u32 flag)
{
    Vec h;
    Vec n;
    f32 dist = 100000.0f;
    f32 d;
    int ret = 0;
    cEm* m;

    if (ret_cross != 0) {
        *ret_cross = *pos1;
    }
    for (m = EmMgr.getActiveWork(); m != 0; m = EmMgr.getNext(m)) {
        if (!(m->atari.m_flag & 0x200)) {
            continue;
        }
        if (!(flag & 4) && m == pPL) {
            continue;
        }
        if (ComnHitCheck(&h, &n, m, pos0, pos1, flag) == 0) {
            continue;
        }
        d = GetDistance3(pos0, &h);
        if (d < dist) {
            dist = d;
            if (ret_cross != 0) {
                *ret_cross = h;
            }
            if (ret_norm != 0) {
                *ret_norm = n;
            }
            ret = 1;
        }
    }
    return ret;
}

// Same against every live object (except id 2): nearest hit / normal.
int ObjHitCheck(Vec* ret_cross, Vec* ret_norm, Vec* pos0, Vec* pos1, u32 flag)
{
    Vec h;
    Vec n;
    f32 dist = 10000000000.0f;
    f32 d;
    int ret = 0;
    cObj* o;

    if (ret_cross != 0) {
        *ret_cross = *pos1;
    }
    for (o = ObjMgr.getActiveWork(); o != 0; o = ObjMgr.getNext(o)) {
        if (!o->isAlive()) {
            continue;
        }
        if (o->id == 2) {
            continue;
        }
        if (ComnHitCheck(&h, &n, (cEm*) o, pos0, pos1, flag) == 0) {
            continue;
        }
        d = (pos0->x - h.x) * (pos0->x - h.x) + (pos0->y - h.y) * (pos0->y - h.y) + (pos0->z - h.z) * (pos0->z - h.z);
        if (d < dist) {
            dist = d;
            if (ret_cross != 0) {
                *ret_cross = h;
            }
            if (ret_norm != 0) {
                *ret_norm = n;
            }
            ret = 1;
        }
    }
    return ret;
}

// Line against one body: a box body (flag bit0 required) through emLineCubeCrossCk in its
// parts / model matrix, a cylinder body (flag bit1) through ObaLineHitChk. 1 on a hit.
int ComnHitCheck(Vec* cross, Vec* norm, cModel* pMod, Vec* pos0, Vec* pos1, u32 flag)
{
    Mtx mat;
    int r;

    if (pMod->atari.m_flag & 2) {
        if (!(flag & 1)) {
            return 0;
        }
        if (pMod->atari.m_parts_no > 0) {
            MTX_COPY_DO(pMod->getPartsPtr(pMod->atari.m_parts_no - 1)->mat, mat);
        } else {
            MTX_COPY_DO(pMod->mat, mat);
        }
        r = emLineCubeCrossCk(pos0, pos1, mat, &pMod->atari.m_offset, cross, pMod->atari.m_radius, pMod->atari.m_height,
                              pMod->atari.m_radius2);
        if (r != 0) {
            return 1;
        }
    } else if (flag & 2) {
        r = ObaLineHitChk(pMod, &pMod->atari, *pos0, *pos1, *cross, *norm);
        if (r != 0) {
            return 1;
        }
    }
    if (cross != 0) {
        *cross = *pos1;
    }
    return 0;
}

// Debug draw of the character's body and every chained info (Debug_flg[2] 0x10000000).
void DrawOba(cModel* pMod)
{
    cAtariInfo* info;

    if (pMod->atari.m_flag & 0x200) {
        info = &pMod->atari;
        do {
            info->disp(pMod);
            info = info->m_pList;
        } while (info != 0);
    }
}

// Line a -> b against the cylinder body treated as a sphere of radius m_radius2 at the body's
// position: nearest entry point in `hit`, outward normal in `nrm`; 1 on a hit.
BOOL ObaLineHitChk(cModel* m, cAtariInfo* info, const Vec& a, const Vec& b, Vec& hit, Vec& nrm)
{
    Vec p0;
    Vec p1;
    Vec w0;
    Vec w1;
    Vec d;
    Vec e;
    Vec f;
    Vec g;
    Vec h;
    Vec q;
    Vec r;
    Vec n;
    cCoord* pm;
    f32 rad;
    f32 dd;
    f32 ee;
    f32 df;
    f32 ef;
    f32 de;
    f32 den;
    f32 t;
    f32 s;
    f32 tc;
    f32 rr;
    f32 depth;
    int parts;

    p1 = info->m_offset;
    p0 = info->m_offset;
    p0.y += info->m_height;
    parts = info->m_parts_no;
    // COMPILER-DIFF: 12 (cse AROUND path): the target's `&p0` argument is a fresh `addi r4,r1,8` (the
    // copy's address pseudo dies at the copy). With `pm = m; if (parts) pm = ..;` cse skips the arm and
    // carries the copy pseudo into the join, folding `&p0` into it across the getPartsPtr call. The
    // explicit else arm is followed as a TAKEN branch and falls into the join label, which ends the
    // extended block: `&p0` stays a hard-reg argument set (never PRE'd, no longer cse-folded).
    if (parts != 0) {
        pm = m->getPartsPtr(parts - 1);
    } else {
        pm = m;
    }
    rad = info->m_radius * 0.75f;
    PSMTXMultVec(pm->mat, &p0, &w0);
    PSMTXMultVec(pm->mat, &p1, &w1);
    PSVECSubtract(&w1, &w0, &d);
    PSVECSubtract(&b, &a, &e);
    PSVECSubtract(&a, &w0, &f);
    dd = PSVECSquareMag(&d);
    ee = PSVECSquareMag(&e);
    df = PSVECDotProduct(&d, &f);
    ef = PSVECDotProduct(&e, &f);
    de = PSVECDotProduct(&d, &e);
    den = dd * ee - de * de;
    // COMPILER-DIFF: 13 (FPR naming): den is settled in f0 before the t numerator is formed. The
    // `de * ef` input makes the anchor ready one cycle later in sched2 (the product's fmuls, not den,
    // gates it), so the `mr r3,r27` hoisted argument takes the slot the codeless anchor used to
    // occupy; the extra `"f"(den)` reference keeps den's local-alloc priority above the product's
    // (which gained a third reference) so den stays in f0 and the product in f13.
    asm("" : "+f"(den) : "f"(den), "f"(de * ef));
    t = (ee * df - de * ef) / den;
    s = (de * df - dd * ef) / den;
    tc = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    // s is clamped in place: the ternary's temporary (f13) is copied back into s (f30) after the join.
    s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
    PSVECScale(&w0, &g, 1.0f - tc);
    rr = rad * rad;
    PSVECScale(&w1, &h, tc);
    PSVECAdd(&g, &h, &q);
    PSVECScale(&a, &g, 1.0f - s);
    PSVECScale(&b, &h, s);
    PSVECAdd(&g, &h, &r);
    if (PSVECSquareDistance(&q, &r) <= rr) {
        PSVECSubtract(&q, &r, &n);
        {
            f32 mag = PSVECMag(&n);
            depth = SQRTF(rr - mag * mag);
        }
#line 1434 "D:/Bio4/Prog/at_mod.cpp"
        VECNormalize(&e, &n);
        PSVECScale(&n, &n, -depth);
        PSVECAdd(&r, &n, &hit);
        PSVECSubtract(&hit, &p1, &nrm);
        nrm.y = 0.0f;
#line 1440 "D:/Bio4/Prog/at_mod.cpp"
        VECNormalize(&nrm, &nrm);
        return 1;
    }
    return 0;
}
