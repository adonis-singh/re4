// game/obj26: object id 0x26, parts-2 attachment (D:/Bio4/Prog/obj26.cpp): a dummy-model object
// that follows parts 2 of its parent, grows to a target scale (R1 0) and then shrinks/fades away
// (R1 1) before destroying itself. Its creator was dead-stripped; the class stays for ObjMgr.
#include "atari.h"
#include "light.h"
#include "ctrl.h"
#include "obj.h"
#include "obj26.h"
#include "global.h"
#include "math_sub.h"
#include "motion.h"

extern "C" {
void obj26_R1_Set(cObj26* obj);
void obj26_R1_Die(cObj26* obj);
void obj26MatCalc(cObj26* obj);
}

static void (*Obj26_R1_move_tbl[2])(cObj26*) = { obj26_R1_Set, obj26_R1_Die };

// (Unused) creates the attachment on `parent` with target scale `scale`, starting at scale 0.
// Never called: the original linker dropped the body but kept its string, statics and pool.
static cObj* SetObj26(cObj* parent, Vec* scale)
{
    cObj* obj;

    obj = ObjMgr.create(cObjMgr::ID_EM2B_PARASITE);
    if (obj == 0) {
        return 0;
    }
    if (obj->modelInit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore),
                       (void*) (pG->pCore->ofs_24 + (u32) pG->pCore)) == 0) {
        pLog->err(0, 0, "SetObj26() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 1000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    OBJ26_WK((cObj26*) obj)->pEm = parent;
    OBJ26_WK((cObj26*) obj)->Scale = *scale;
    obj->scale.x = obj->scale.y = obj->scale.z = 0.0f;
    obj->invisible_factor = 1.0f;
    return obj;
}

// Per-frame: dies with the parent, R1 routine, destroyed once fully faded.
void cObj26::move()
{
    if (OBJ26_WK(this)->pEm) {
        if (!OBJ26_WK(this)->pEm->isAlive()) {
            ObjMgr.destroy(this);
            return;
        }
    }
    Obj26_R1_move_tbl[r_no_1](this);
    if (invisible_factor == 0.0f) {
        ObjMgr.destroy(this);
    }
}

// Rno1 == 0: eases the scale to tgtScale (10% per frame) and plays the motion.
void obj26_R1_Set(cObj26* pObj)
{
    Obj26Work* w = OBJ26_WK(pObj);

    switch (pObj->r_no_2) {
    case 0:
        pObj->r_no_2++;
    case 1:
        pObj->scale.x = pObj->scale.x * 0.9f + w->Scale.x * 0.1f;
        pObj->scale.y = pObj->scale.y * 0.9f + w->Scale.y * 0.1f;
        pObj->scale.z = pObj->scale.z * 0.9f + w->Scale.z * 0.1f;
        if (pObj->Motion.pMot) {
            MotionMove(pObj, 0);
        }
        break;
    }
    obj26MatCalc(pObj);
}

// Rno1 == 1: shrinks by 10% and fades by 10% per frame until invisible.
void obj26_R1_Die(cObj26* pObj)
{
    switch (pObj->r_no_2) {
    case 0:
        pObj->r_no_2++;
    case 1:
        pObj->scale.y = pObj->scale.z = pObj->scale.x = pObj->scale.x * 0.9f;
        pObj->invisible_factor *= 0.9f;
        if (pObj->invisible_factor <= 0.01f) {
            pObj->invisible_factor = 0.0f;
            pObj->r_no_2++;
        } else if (pObj->Motion.pMot) {
            MotionMove(pObj, 0);
        }
        break;
    case 2:
        break;
    }
    obj26MatCalc(pObj);
}

// Places the object under parts 2 of the parent (or free) and updates its parts.
void obj26MatCalc(cObj26* pObj)
{
    if (OBJ26_WK(pObj)->pEm) {
        cParts* parts = OBJ26_WK(pObj)->pEm->getPartsPtr(2);
        RotMatrix(pObj->mat, &pObj->ang);
        TransMatrix(pObj->mat, &pObj->pos);
        ScaleMatrix(pObj->mat, &pObj->scale);
        PSMTXConcat(parts->mat, pObj->mat, pObj->mat);
        pObj->Motion.Mot_flag |= 0x40000000;
    } else {
        RotMatrix(pObj->l_mat, &pObj->ang);
        TransMatrix(pObj->l_mat, &pObj->pos);
        ScaleMatrix(pObj->l_mat, &pObj->scale);
        PSMTXCopy(pObj->l_mat, pObj->mat);
    }
    pObj->partsMatCalc();
    pObj->partsWorldCalc();
}
