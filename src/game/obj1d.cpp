// game/obj1d: object id 0x1D, chain link (D:/Bio4/Prog/obj1d.cpp): a model hung between two parts
// of a parent (position and orientation interpolated half-way between them, setParent2) or on one
// parts (setParent), with an optional pendulum cloth (setChain); fades out (LostWait/Lost) when
// released. Used for the El Gigante / trolley chains.
#include "atari.h"
#include "obj.h"
#include "obj1d.h"
#include "global.h"
#include "math_sub.h"
#include "motion.h"

extern "C" {
void obj1d_R1_Set(cObjChain* obj);
void obj1d_R1_LostWait(cObjChain* obj);
void obj1d_R1_Lost(cObjChain* obj);
void obj1d_R1_Parent(cObjChain* obj);
}

static void (*Obj1d_R1_move_tbl[4])(cObjChain*) = { obj1d_R1_Set, obj1d_R1_LostWait, obj1d_R1_Lost,
                                                     obj1d_R1_Parent };

// Creates the chain link (back of the pool) at pos/rot with no parent or cloth.
cObjChain* SetChain(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObjChain* obj;
    ChainWork* w;

    obj = (cObjChain*) ObjMgr.createBack(cObjMgr::ID_CHAIN);
    if (obj == 0) {
        return 0;
    }
    w = CHAIN_WK(obj);
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetChain() modelInit() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 10000.0f, 10000.0f, 100000.0f };

    obj->sub2B4.atari.throughOn();
    obj->LightInfo.init2(0, 1, &p0, &p1, 2);
    obj->pos = *pos;
    obj->pos_old = *pos;
    obj->ang = *rot;
    w->parent = 0;
    w->parts1 = 0;
    w->parts2 = 0;
    w->cloth = 0;
    obj->r_no_0 = 1;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    Obj1d_R1_move_tbl[obj->r_no_1]((cObjChain*) obj);
    return obj;
}

// Per-frame: dies with the parent, motion, R1 routine, pendulum step.
void cObjChain::move()
{
    ChainWork* w = CHAIN_WK(this);

    if (w->parent) {
        if ((w->parent->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
    }
    if (Motion.pMot) {
        MotionMove(this, 0);
    }
    Obj1d_R1_move_tbl[r_no_1](this);
    if ((be_flag & 0x201) == 1) {
        chainMove();
    }
}

// Rno1 == 0: free: motion and matrices.
void obj1d_R1_Set(cObjChain* pObj)
{
    if (pObj->Motion.pMot) {
        MotionMove(pObj, 0);
    } else {
        RotMatrix(pObj->mat, &pObj->ang);
        TransMatrix(pObj->mat, &pObj->pos);
        ScaleMatrix(pObj->mat, &pObj->scale);
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Rno1 == 1: waits 90 frames then fades out (or vanishes off screen) -> Lost.
void obj1d_R1_LostWait(cObjChain* pObj)
{
    ChainWork* w = CHAIN_WK(pObj);

    switch (pObj->r_no_2) {
    case 0:
        w->timer = 90;
        pObj->r_no_2++;
    case 1:
        if (w->timer == 0) {
            pObj->invisible_factor -= 0.1f;
            if (pObj->invisible_factor <= 0.0f) {
                pObj->invisible_factor = 0.0f;
                pObj->r_no_0 = 1;
                pObj->r_no_1 = 2;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
                break;
            }
        } else {
            w->timer--;
        }
        {
            Vec scr;
            Vec p;

            p = pObj->pos;
            GetScreenPos(&p, &scr);
            if (scr.z > 1.0f) {
                pObj->r_no_0 = 1;
                pObj->r_no_1 = 2;
                pObj->r_no_2 = 0;
                pObj->r_no_3 = 0;
            }
        }
        break;
    }
    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    pObj->partsMatCalc();
    pObj->partsWorldCalc();
}

// Rno1 == 2: hides and destroys the link.
void obj1d_R1_Lost(cObjChain* pObj)
{
    if (pObj->r_no_2 == 0) {
        pObj->r_no_2++;
        pObj->be_flag &= ~2;
        pObj->be_flag &= ~0x20;
        ObjMgr.destroy(pObj);
    }
}

// Rno1 == 3: hung between parts1 and parts2 of the parent: orientation = slerp of the two parts
// matrices (axes normalised unless flags bit 1), position = midpoint of the two offsets.
void obj1d_R1_Parent(cObjChain* pObj)
{
    ChainWork* w = CHAIN_WK(pObj);
    Mtx ma;
    Mtx mb;
    Vec v0;
    Vec v1;
    Vec v2;
    Quaternion qa;
    Quaternion qb;
    Quaternion q;
    Vec pa;
    Vec pb;
    Vec p;
    cModel* parent = w->parent;
    cModel* partsA;
    cModel* partsB;

    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    if (parent && parent->pParts) {
        partsA = parent->getPartsPtr(w->parts1);
        PSMTXConcat(partsA->mat, pObj->mat, ma);
        partsB = parent->getPartsPtr(w->parts2);
        PSMTXConcat(partsB->mat, pObj->mat, mb);
        if (!(w->flags & 2)) {
            v0.x = ma[0][0];
            v0.y = ma[1][0];
            v0.z = ma[2][0];
            v1.x = ma[0][1];
            v1.y = ma[1][1];
            v1.z = ma[2][1];
            v2.x = ma[0][2];
            v2.y = ma[1][2];
            v2.z = ma[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 290 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 292 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 294 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v2, &v2);
            ma[0][0] = v0.x;
            ma[1][0] = v0.y;
            ma[2][0] = v0.z;
            ma[0][1] = v1.x;
            ma[1][1] = v1.y;
            ma[2][1] = v1.z;
            ma[0][2] = v2.x;
            ma[1][2] = v2.y;
            ma[2][2] = v2.z;
            v0.x = mb[0][0];
            v0.y = mb[1][0];
            v0.z = mb[2][0];
            v1.x = mb[0][1];
            v1.y = mb[1][1];
            v1.z = mb[2][1];
            v2.x = mb[0][2];
            v2.y = mb[1][2];
            v2.z = mb[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 315 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 317 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 319 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v2, &v2);
            mb[0][0] = v0.x;
            mb[1][0] = v0.y;
            mb[2][0] = v0.z;
            mb[0][1] = v1.x;
            mb[1][1] = v1.y;
            mb[2][1] = v1.z;
            mb[0][2] = v2.x;
            mb[1][2] = v2.y;
            mb[2][2] = v2.z;
        }
        C_QUATMtx(&qa, ma);
        C_QUATMtx(&qb, mb);
        C_QUATSlerp(&qa, &qb, &q, 0.5f);
        PSMTXQuat(pObj->mat, &q);
        PSMTXMultVec(partsA->mat, &w->ofs1, &pa);
        PSMTXMultVec(partsB->mat, &w->ofs2, &pb);
        PosToPos(&pa, &pb, &p, 0.5f);
        TransMatrix(pObj->mat, &p);
    }
    if (pObj->Motion.pMot) {
        pObj->Motion.Mot_flag |= 0x40000000;
        MotionMove(pObj, 0);
    } else {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Hangs the link on one parts (both ends the same); flag = keep the parts scale.
void cObjChain::setParent(cModel* parent, int parts, Vec* ofs, int flag)
{
    ChainWork* w = CHAIN_WK(this);

    w->parent = parent;
    w->parts1 = parts;
    w->parts2 = parts;
    w->ofs1 = *ofs;
    w->ofs2 = *ofs;
    if (flag) {
        w->flags |= 2;
    } else {
        w->flags &= ~2;
    }
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Hangs the link between two parts with their offsets.
void cObjChain::setParent2(cModel* pEm, int parts1, Vec* pPos1, int parts2, Vec* pPos2, int mode)
{
    ChainWork* w = CHAIN_WK(this);

    w->parent = pEm;
    w->parts1 = parts1;
    w->parts2 = parts2;
    w->ofs1 = *pPos1;
    w->ofs2 = *pPos2;
    if (mode) {
        w->flags |= 2;
    } else {
        w->flags &= ~2;
    }
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Attaches a pendulum cloth to the link.
void cObjChain::setChain(PenCloth* pCloth)
{
    CHAIN_WK(this)->cloth = pCloth;
    if (pCloth) {
        PenClothSet(this, pCloth, 100.0f);
    }
}

// Steps the pendulum cloth; clears the shadow/cull flags 0x00E00000.
void cObjChain::chainMove()
{
    if (CHAIN_WK(this)->cloth) {
        PenClothMove(this, CHAIN_WK(this)->cloth);
        be_flag &= ~0x00E00000;
    }
}
