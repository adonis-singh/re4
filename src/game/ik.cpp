// game/ik.cpp: two-bone inverse kinematics for the legs of motion-driven models (feet on the
// floor). IKInit reads the chain roots from the motion data; InverseKinematics runs every frame.

#include "atari.h"
#include "global.h"
#include "model.h"
#include "motion.h"
#include "em.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
void ikCalc(cParts* root, cParts* joint, cParts* eff);
static void heel2toe(Mtx m, cParts* p, Vec* pos);
}

#define IK_FLAGS(p) ((p)->motParts.flags)
#define BIND_X(p) (IK_PARTS(p)->bindMat[0][3])
#define BIND_Y(p) (IK_PARTS(p)->bindMat[1][3])
#define BIND_Z(p) (IK_PARTS(p)->bindMat[2][3])

// Called by MotionSetCore for a new motion: clears the IK flags of every parts, then for each
// joint of the motion's joint table flagged as an IK root (kind bits 0x30) marks the chain root
// (flags bit 2), stores the bend axis (kind >> 8), the bone lengths from the bind pose and the
// root->effector direction, and the options: 0x20 = also correct the toe angle (ikAng), 0x80 =
// the chain has an extra joint (0x210). A degenerate bend plane disables the chain.
void IKInit(cModel* pEm, MotionWorkSub* pInfo)
{
    Vec axis;
    Mtx mtx;
    Vec v;
    cParts* p;
    cParts* root;
    cParts* joint;
    cParts* eff;
    int i;

    for (p = pEm->pList; p != 0; p = p->pList) {
        IK_FLAGS(p) &= ~4;
        IK_FLAGS(p) &= ~0x10;
        IK_FLAGS(p) &= ~0x300;
    }
    for (i = 0; i < pInfo->Joint_num; i++) {
        int kind = pInfo->pJoint_kind[i] & 0xFF;
        if (!(kind & 0x30)) {
            continue;
        }
        root = pEm->getPartsPtr(pInfo->pJoint_no[i]);
        if (root == 0) {
            pLog->err(2, 0, "IKInit(): missing Root.");
            return;
        }
        joint = root->pList;
        if (joint == 0) {
            pLog->err(2, 0, "IKInit(): missing Joint.");
            return;
        }
        eff = joint->pList;
        if (eff == 0) {
            pLog->err(2, 0, "IKInit(): missing Effector.");
            return;
        }
        IK_FLAGS(root) |= 4;
        switch ((pInfo->pJoint_kind[i] >> 8) & 0xF) {
        case 4:
            axis.x = 0.0f;
            axis.y = 0.0f;
            axis.z = 1.0f;
            break;
        case 5:
            axis.x = 0.0f;
            axis.y = 0.0f;
            axis.z = -1.0f;
            break;
        case 0:
            axis.x = 0.0f;
            axis.y = 1.0f;
            axis.z = 0.0f;
            break;
        case 1:
            axis.x = 0.0f;
            axis.y = -1.0f;
            axis.z = 0.0f;
            break;
        case 2:
            axis.x = 1.0f;
            axis.y = 0.0f;
            axis.z = 0.0f;
            break;
        case 3:
            axis.x = -1.0f;
            axis.y = 0.0f;
            axis.z = 0.0f;
            break;
        }
        if (kind & 0x20) {
            IK_FLAGS(root) |= 0x100;
            root->pList->pList->pList->motParts.ikAng = 0.0f;
        }
        if (kind & 0x80) {
            u32 f = IK_FLAGS(root) | 0x210;
            IK_FLAGS(root) = f;
            if (f & 0x100) {
                eff = joint->pList->pList;
                if (eff == 0) {
                    pLog->err(2, 0, "IKInit(): missing joint.");
                    return;
                }
            }
        }
        v.x = BIND_X(joint) - BIND_X(root);
        v.y = BIND_Y(joint) - BIND_Y(root);
        v.z = BIND_Z(joint) - BIND_Z(root);
        IK_PARTS(root)->len = PSVECMag(&v);
        if (v.y == 0.0f && v.z == 0.0f) {
            IK_FLAGS(root) |= 0x2000;
        } else if (v.x == 0.0f && v.z == 0.0f) {
            IK_FLAGS(root) |= 0x4000;
        } else if (v.x == 0.0f && v.y == 0.0f) {
            IK_FLAGS(root) |= 0x8000;
        } else {
            IK_FLAGS(root) &= ~0xE000;
        }
        v.x = BIND_X(eff) - BIND_X(joint);
        v.y = BIND_Y(eff) - BIND_Y(joint);
        v.z = BIND_Z(eff) - BIND_Z(joint);
        IK_PARTS(joint)->len = PSVECMag(&v);
        IK_PARTS(root)->dir.x = BIND_X(root) - BIND_X(eff);
        IK_PARTS(root)->dir.y = BIND_Y(root) - BIND_Y(eff);
        IK_PARTS(root)->dir.z = BIND_Z(root) - BIND_Z(eff);
        {
            Vec* d = &IK_PARTS(root)->dir;
            int n;
            if (d->x == 0.0f && d->y == 0.0f && d->z == 0.0f) {
                goto plane_error;
            }
            n = 0;
            if (axis.x == 0.0f && d->x == 0.0f) {
                n++;
            }
            if (axis.y == 0.0f && d->y == 0.0f) {
                n++;
            }
            if (axis.z == 0.0f && d->z == 0.0f) {
                n++;
            }
            if (n > 1) {
            plane_error:
                IK_FLAGS(root) &= ~4;
                pLog->err(2, 0, "IKInit(): IK plane error");
            } else {
                SetOrientationZY(&IK_PARTS(root)->dir, &axis, mtx);
                PSMTXTranspose(mtx, IK_PARTS(root)->mat);
                axis.x = 1.0f;
                axis.y = 0.0f;
                axis.z = 0.0f;
                PSMTXMultVec(mtx, &axis, &IK_PARTS(root)->axis);
            }
        }
    }
}

// Two-bone solve: given the current root and effector world positions and the stored bone lengths
// la/lb, computes the knee angles by the law of cosines (clamped when the target is out of reach),
// re-orients the root and joint matrices in the bend plane and re-derives their world positions.
// Bend the root/joint pair so that the effector reaches its current world position.
void ikCalc(cParts* root, cParts* joint, cParts* eff)
{
    Vec dir;
    Vec axis;
    Mtx m;
    Mtx r1;
    Mtx r2;
    f32 ang2 = 0.0f;
    f32 ang1 = ang2;
    f32 d;
    f32 la;
    f32 lb;
    f32 c1;
    f32 c2;

    d = GetDistance3(&root->world, &eff->world);
    la = IK_PARTS(root)->len;
    lb = IK_PARTS(joint)->len;
    if (d < la + lb) {
        c1 = (la * la + d * d - lb * lb) / ((la + la) * d);
        c2 = (d * d + lb * lb - la * la) / ((d + d) * lb);
        if (c1 < -1.0f) {
            c1 = -1.0f;
        }
        if (c1 > 1.0f) {
            c1 = 1.0f;
        }
        if (c2 < -1.0f) {
            c2 = -1.0f;
        }
        if (c2 > 1.0f) {
            c2 = 1.0f;
        }
        ang1 = acosf(c1);
        ang2 = acosf(c2);
    }
    PSMTXMultVecSR(root->mat, &IK_PARTS(root)->axis, &axis);
    PSVECSubtract(&eff->world, &root->world, &dir);
    SetOrientationZX(&dir, &axis, m);
    PSMTXConcat(m, IK_PARTS(root)->mat, m);
    PSMTXRotAxisRad(r1, &IK_PARTS(root)->axis, -ang1);
    PSMTXConcat(m, r1, root->mat);
    TransMatrix(root->mat, &root->world);
    PSMTXRotAxisRad(r2, &IK_PARTS(root)->axis, ang2);
    PSMTXConcat(m, r2, joint->mat);
    PSMTXMultVec(root->mat, &joint->pos, &joint->world);
    TransMatrix(joint->mat, &joint->world);
}

// After the solve, recomputes the heel matrix at pos and its toe child's world position (marks both
// with flag 0x10000000 so the motion code does not overwrite them).
// Put the heel (p) so that its toe (the child) lands on `pos`.
static void heel2toe(Mtx m, cParts* p, Vec* pos)
{
    Mtx inv;
    cParts* toe = p->pList;

    if (toe == 0) {
        pLog->err(2, 0, "heel2toe(): missing joint.");
        return;
    }
    PSMTXConcat(m, p->l_mat, p->mat);
    PSMTXMultVecSR(p->mat, &toe->pos, &p->world);
    PSVECSubtract(pos, &p->world, &p->world);
    TransMatrix(p->mat, &p->world);
    PSMTXInverse(p->pParent->mat, inv);
    PSMTXConcat(inv, p->mat, p->l_mat);
    IK_FLAGS(p) |= 0x10000000;
    TransMatrix(toe->l_mat, &toe->pos);
    PSMTXConcat(toe->pParent->mat, toe->l_mat, toe->mat);
    toe->world.x = toe->mat[0][3];
    toe->world.y = toe->mat[1][3];
    toe->world.z = toe->mat[2][3];
    IK_FLAGS(toe) |= 0x10000000;
}

// Recompute the local matrix of `q` from its world matrix and flag it as posed by the IK.
#define IK_LOCAL(q)                                                  \
    PSMTXInverse((q)->pParent->mat, inv);                            \
    PSMTXConcat(inv, (q)->mat, (q)->l_mat);                       \
    {                                                                \
        u32 f = IK_FLAGS(q);                                         \
        IK_FLAGS(q) = f | 0x10000000;                                \
        if (blend != 0 && (blend->Mot_flag & 0x80000000)) {                 \
            IK_FLAGS(q) = f | 0x90000000;                            \
        }                                                            \
    }

#define IK_FLAG_ONLY(q)                                              \
    {                                                                \
        u32 f = IK_FLAGS(q);                                         \
        IK_FLAGS(q) = f | 0x10000000;                                \
        if (blend != 0 && (blend->Mot_flag & 0x80000000)) {                 \
            IK_FLAGS(q) = f | 0x90000000;                            \
        }                                                            \
    }

#define MAT_COL(dst, mtx, c)      \
    (dst).x = (mtx)[0][c];        \
    (dst).y = (mtx)[1][c];        \
    (dst).z = (mtx)[2][c]

// Twist of the effector `e` (child matrix `rel` relative to its parent) about the parent plane:
// the angle between the effector's side axis and the plane normal, accumulated in the effector's
// MotionParts so that it can wrap past +-180 degrees.
#define IK_TWIST_ANGLE(rel, e)                                       \
    MAT_COL(a, rel, 0);                                              \
    MAT_COL(b, rel, 2);                                              \
    PSVECCrossProduct(&a, &up, &c);                                  \
    ang = VecAngle(&c, &b);                                          \
    PSVECCrossProduct(&c, &b, &d);                                   \
    if (PSVECDotProduct(&d, &a) < 0.0f) {                            \
        ang = -ang;                                                  \
        if (e->motParts.ikAng > 1.5707964f) {                   \
            ang += 6.2831855f;                                       \
        } else {                                                     \
            e->motParts.ikAng = ang;                            \
        }                                                            \
    } else {                                                         \
        if (e->motParts.ikAng < -1.5707964f) {                  \
            ang -= 6.2831855f;                                       \
        } else {                                                     \
            e->motParts.ikAng = ang;                            \
        }                                                            \
    }

// Rotate parts `q` by `ang * rate` about its own column `col`, keeping its translation.
#define IK_TWIST_APPLY(q, ax, col, rate)                             \
    MAT_COL(ax, (q)->mat, col);                                      \
    MAT_COL(t, (q)->mat, 3);                                         \
    PSMTXRotAxisRad(rm, &ax, ang * rate);                            \
    PSMTXConcat(rm, (q)->mat, (q)->mat);                             \
    TransMatrix((q)->mat, &t)

// Per-frame leg IK (from MotionMove): for each IK root chain not disabled, finds the floor under the
// foot effector (SatMgr.getFloor; skipped when EM_STATUS_IK_OFF or chain flag 0x200), moves the
// foot target onto the floor when it is within reach, solves the chain (ikCalc), then places the
// heel/toe (heel2toe) and, with option 0x100, blends the toe angle.
void InverseKinematics(cModel* pEm, int arm_flag)
{
    cEm* em = (cEm*) pEm;
    MotionWorkSub* blend = MOTION(pEm)->blend;
    Mtx inv;
    Vec target;
    Vec a;
    cParts* p;
    cParts* joint;
    cParts* eff;
    cParts* q;
    f32 floorY;
    f32 ang;
    int n;

    for (p = pEm->pList; p != 0; p = p->pList) {
        if (!(IK_FLAGS(p) & 4)) {
            continue;
        }
        if (IK_FLAGS(p) & 0x80) {
            continue;
        }
        joint = p->pList;
        eff = joint->pList;
        PSMTXMultVec(pEm->mat, &eff->pos, &target);
        if (!em->checkStatus(EM_STATUS_IK_OFF) && !(IK_FLAGS(p) & 0x200)) {
            if (!(em->atari.m_flag & 0x100)) {
                floorY = target.y - eff->pos.y;
            } else if (IK_FLAGS(p) & 0x800) {
                target.y += 1000.0f;
                floorY = SatMgr.getFloor(&target, 0, 500.0f, 100000.0f, 0);
            } else if (IK_FLAGS(p) & 0x40) {
                floorY = SatMgr.getFloor(&target, 0, 6000.0f, 100000.0f, 0);
            } else {
                floorY = SatMgr.getFloor(&target, 0, 500.0f, 100000.0f, 0);
            }
            if (floorY != -100000.0f) {
                f32 dist;
                f32 reach;
                a.x = target.x;
                a.y = floorY + eff->pos.y;
                a.z = target.z;
                dist = GetDistance3(&p->world, &a);
                reach = IK_PARTS(p)->len + IK_PARTS(p->pList)->len;
                if (target.y < floorY + eff->pos.y || ((IK_FLAGS(p) & 0x400) && dist < reach) || (IK_FLAGS(p) & 0x800)) {
                    if ((IK_FLAGS(p) & 0x800) && floorY < target.y) {
                        a.y = target.y;
                    } else {
                        target.y = floorY + eff->pos.y;
                    }
                }
            }
        }
        if (!(IK_FLAGS(p) & 0x10)) {
            if (IK_FLAGS(p) & 0x100) {
                heel2toe(pEm->mat, eff, &target);
            } else {
                PSMTXConcat(pEm->mat, eff->l_mat, eff->mat);
                TransMatrix(eff->mat, &target);
                MAT_COL(eff->world, eff->mat, 3);
                IK_LOCAL(eff);
            }
        } else {
            eff = p->pList->pList->pList;
            PSMTXConcat(pEm->mat, eff->l_mat, eff->mat);
            MAT_COL(eff->world, eff->mat, 3);
            IK_LOCAL(eff);
        }
        if (!(IK_FLAGS(p) & 0x10)) {
            ikCalc(p, joint, eff);
            IK_LOCAL(p);
            IK_LOCAL(joint);
            if (IK_FLAGS(p) & 0x100) {
                a.x = BIND_X(joint) - BIND_X(eff);
                a.y = BIND_Y(joint) - BIND_Y(eff);
                a.z = BIND_Z(joint) - BIND_Z(eff);
                PSMTXMultVec(eff->pParent->mat, &a, &a);
                TransMatrix(eff->mat, &a);
                MAT_COL(eff->world, eff->mat, 3);
                IK_LOCAL(eff);
                MAT_COL(eff->pos, eff->l_mat, 3);
            } else {
                PSMTXInverse(eff->pParent->mat, inv);
                PSMTXConcat(inv, eff->mat, eff->l_mat);
            }
            if (IK_FLAGS(p) & 0x100) {
                cParts* toe = eff->pList;
                RotMatrix(toe->l_mat, &toe->ang);
                TransMatrix(toe->l_mat, &toe->pos);
                PSMTXConcat(toe->pParent->mat, toe->l_mat, toe->mat);
                MAT_COL(toe->world, toe->mat, 3);
                IK_FLAG_ONLY(toe);
            }
            if (IK_FLAGS(p) & 0x1000) {
                int sel;
                if (IK_FLAGS(p) & 0x2000) {
                    sel = 0;
                } else if (IK_FLAGS(p) & 0x4000) {
                    sel = 1;
                } else if (IK_FLAGS(p) & 0x8000) {
                    sel = 2;
                } else {
                    sel = 1;
                }
                cParts* j = p->pList;
                cParts* e = j->pList;
                switch (sel) {
                case 0: {
                    Vec b;
                    Vec t;
                    Vec d;
                    Vec up = {0.0f, 1.0f, 0.0f};
                    Vec c;
                    Mtx rm;
                    Mtx rel;
                    Mtx jinv;
                    PSMTXInverse(j->mat, jinv);
                    PSMTXConcat(jinv, e->mat, rel);
                    IK_TWIST_ANGLE(rel, e);
                    IK_TWIST_APPLY(j, a, 0, 0.5f);
                    IK_TWIST_APPLY(p, a, 0, 0.25f);
                    break;
                }
                case 1: {
                    Vec b;
                    Vec t;
                    Vec d;
                    Vec up = {0.0f, 1.0f, 0.0f};
                    Vec c;
                    Mtx rm;
                    Vec ax;
                    Mtx rel;
                    Vec a2;
                    Mtx jinv;
                    Mtx rr;
                    PSMTXInverse(j->mat, jinv);
                    PSMTXConcat(jinv, e->mat, rel);
                    Vec rot = {-1.5707964f, 1.5707964f, 0.0f};
                    RotMatrix(rr, &rot);
                    PSMTXConcat(rr, rel, rel);
                    IK_TWIST_ANGLE(rel, e);
                    IK_TWIST_APPLY(j, ax, 1, 0.5f);
                    IK_TWIST_APPLY(p, ax, 1, 0.25f);
                    break;
                }
                }
                q = p;
                for (n = 0; n <= 2; n++) {
                    PSMTXInverse(q->pParent->mat, inv);
                    PSMTXConcat(inv, q->mat, q->l_mat);
                    if (n == 2) {
                        MAT_COL(q->pos, q->l_mat, 3);
                    }
                    IK_FLAG_ONLY(q);
                    q = q->pList;
                }
            }
        } else if (IK_FLAGS(p) & 0x100) {
            cParts* j = p->pList;
            cParts* e = j->pList;
            cParts* toe = e->pList;
            ikCalc(p, j, toe);
            PSMTXConcat(j->mat, e->l_mat, e->mat);
            {
                Vec b;
                Vec t;
                Vec d;
                Vec up = {0.0f, 1.0f, 0.0f};
                Vec c;
                Mtx rm;
                Mtx rel;
                Mtx jinv;
                PSMTXInverse(e->mat, jinv);
                PSMTXConcat(jinv, toe->mat, rel);
                IK_TWIST_ANGLE(rel, toe);
                IK_TWIST_APPLY(e, a, 0, 0.5f);
                IK_TWIST_APPLY(j, a, 0, 0.25f);
                IK_TWIST_APPLY(p, a, 0, 0.125f);
            }
            q = p;
            for (n = 0; n <= 3; n++) {
                PSMTXInverse(q->pParent->mat, inv);
                PSMTXConcat(inv, q->mat, q->l_mat);
                if (n == 3) {
                    MAT_COL(q->pos, q->l_mat, 3);
                }
                IK_FLAG_ONLY(q);
                q = q->pList;
            }
        }
    }
}
