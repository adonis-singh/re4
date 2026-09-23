// game/obj18: object id 0x18, the event body / cloth model (D:/Bio4/Prog/obj18.cpp). The event
// player creates one per SetOm packet: a character model that follows a parts of its parent
// (OyaSetObj18, slerp catch-up) and runs the cloth simulation of its `type` (1 Leon hair/jacket/
// holster, 2 Ashley, 3 Ada with the ribbon child, 4 Luis, 7/8/9/0xA/0x13..0x16 the enemy cloth sets,
// 0xB the El Gigante rope child); `cmf` holds the event's model control flags (Obj18Cmf*).
#include "atari.h"
#include "event.h"
#include "obj.h"
#include "obj18.h"
#include "global.h"
#include "math_sub.h"
#include "pl_cloth.h"
#include "motion.h"
#include <string.h>
#include "em_cloth.h"

extern "C" {
void obj18SetOya(cObj18* obj);
}

// Unused work-size error message.
// Never called: the original keeps the message of this unused inline in .rodata.
static inline void obj18FreeSizeErr(int size)
{
    pLog->err(0, 0, "SetObj18 freeSize failed : %d", size);
}

PlCloth Obj18Cloth1;
PlCloth Obj18Cloth2;
static PlCloth Obj18Cloth3;
PlCloth Obj18Cloth4;
static PlCloth Obj18Cloth5;
PlCloth Obj18Cloth6;
static PlCloth Evt_leonHair;
PlCloth Evt_leonJacket;
PlCloth Evt_leonHolster;
PlCloth Evt_girlHair;
static PlCloth Evt_girlSkirt;
PlCloth Evt_girlSweater;
PlCloth Evt_adaRibbon;
PlCloth Evt_adaDress;
static PlCloth Evt_adaHair;
static PlCloth Evt_luisHair;

// Creates the body (back of the pool) with the light class of its type, a bound-box light volume,
// no parent, and sets up the type's cloth (loading the ribbon / rope child models from the event
// bins). 0 on failure.
cObj* SetObj18(void* bin, void* tpl, Vec* pos, Vec* rot, int type)
{
    cObj* obj;
    Obj18Work* w;
    int lightFlag;
    Vec sz;
    Vec ofs;
    void* cbin;
    void* ctpl;
    cModelInfo* info;
    ModelBound* b;

    obj = ObjMgr.createBack(cObjMgr::ID_EVENT);
    if (obj == 0) {
        return 0;
    }
    w = OBJ18_WK((cObj18*) obj);
    memset(w, 0, sizeof(Obj18Work));
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    obj->sub2B4.atari.throughOn();
    lightFlag = 4;
    if (type == 1) {
        lightFlag = 0x40;
        if (pG->pl_type == 0) {
            lightFlag = 1;
        }
    }
    if (type == 2) {
        lightFlag = 0x40;
        if (pG->pl_type == 1) {
            lightFlag = 1;
        }
    }
    if (type == 3) {
        lightFlag = 0x40;
        if (pG->pl_type == 2) {
            lightFlag = 1;
        }
    }
    if (type == 0x17) {
        lightFlag = 0x40;
        if (pG->pl_type == 2) {
            lightFlag = 1;
        }
    }
    if (type == 4) {
        lightFlag = 0x40;
    }
    if (type == 5) {
        lightFlag = 0x40;
    }
    if (type == 6) {
        lightFlag = 2;
    }
    if (type == 7) {
        lightFlag = 2;
    }
    if (type == 8) {
        lightFlag = 2;
    }
    if (type == 0x12) {
        lightFlag = 2;
    }
    if (type == 0xB) {
        lightFlag = 2;
    }
    if (type == 0x13) {
        lightFlag = 0x20;
    }
    if (type == 0x14) {
        lightFlag = 4;
    }
    if (type == 0x15) {
        lightFlag = 0x20;
    }
    if (type == 0x16) {
        lightFlag = 4;
    }
    if (type == 9) {
        lightFlag = 2;
    }
    if (type == 0x18) {
        lightFlag = 2;
    }
    if (type == 0xA) {
        lightFlag = 2;
    }
    if (type == 0xC) {
        lightFlag = 2;
    }
    if (type == 0xD) {
        lightFlag = 2;
    }
    if (type == 0) {
        lightFlag = 4;
    }
    if (type == 0xE) {
        lightFlag = 4;
    }
    if (type == 0xF) {
        lightFlag = 0x10;
    }
    if (type == 0x10) {
        lightFlag = 1;
    }
    if (type == 0x11) {
        lightFlag = 8;
    }
    w->obj18_type = type;
    info = obj->pModelInfo;
    b = &info->bound;
    sz.x = b->size.x;
    sz.y = b->size.y;
    sz.z = b->size.z;
    PSVECSubtract(&info->bound.center, &obj->pParts->pos, &ofs);
    obj->LightInfo.init2(2, 1, &ofs, &sz, lightFlag);
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->pos_old = obj->pos;
    if (rot) {
        obj->ang = *rot;
    } else {
        obj->ang.x = 0.0f;
        obj->ang.y = 0.0f;
        obj->ang.z = 0.0f;
    }
    w->oya_hokan = 1.0f;
    w->oya_hokan_add = 0.0f;
    w->pEm_oya = 0;
    w->oya_parts = 0;
    w->ObjChainFlagCommon = 0;
    switch (w->obj18_type) {
    case OBJ18_TYPE_LEON:
        if (pG->pl_type == 0) {
            PlClothSetLeon(obj, &Evt_leonHair, &Evt_leonJacket, &Evt_leonHolster);
        }
        break;
    case OBJ18_TYPE_ASHLEY:
        PlClothSetGirl(obj, &Evt_girlHair, &Evt_girlSkirt, &Evt_girlSweater, 1);
        break;
    case OBJ18_TYPE_ADA:
        PlClothSetAda(obj, &Evt_adaRibbon, &Evt_adaDress, &Evt_adaHair, 1);
        if (pG->game_costume == 0) {
            if (EvtMgr.GetBin(&cbin, "em/pl02/pl020f.bin", 0)) {
                if (EvtMgr.GetBin(&ctpl, "em/pl02/pl020a.tpl", 0)) {
                    w->child = (cObj*) AdaRibbonSet(obj, &Evt_adaRibbon, cbin, ctpl);
                    if (w->child) {
                        w->child->setNoSuspend(1);
                        w->child->LightInfo.EnableMask = obj->LightInfo.EnableMask;
                    }
                }
            }
        }
        break;
    case OBJ18_TYPE_LUIS:
        PlClothSetLuis(obj, &Evt_luisHair);
        break;
    case OBJ18_TYPE_TRADER:
        break;
    case OBJ18_TYPE_MAYOR1:
        Em34ClothSet2(obj, &Obj18Cloth2);
        Em34ClothSet1(obj, &Obj18Cloth1);
        break;
    case OBJ18_TYPE_NO2:
        Em37HairSet(obj, &Obj18Cloth1);
        Em37CoatSet(obj, &Obj18Cloth2);
        break;
    case OBJ18_TYPE_SADDLER:
        Em30ClothSet1(obj, &Obj18Cloth1);
        Em30ClothSet2(obj, &Obj18Cloth2);
        break;
    case OBJ18_TYPE_INSECTBOSS0:
        Em33ClothSet(obj, &Obj18Cloth3, 0);
        Em33ClothSet2(obj, &Obj18Cloth4, 0);
        break;
    case OBJ18_TYPE_INSECTBOSS0S:
        Em33ClothSet(obj, &Obj18Cloth3, 1);
        Em33ClothSet2(obj, &Obj18Cloth4, 1);
        break;
    case OBJ18_TYPE_INSECTBOSS1:
        Em33ClothSet(obj, &Obj18Cloth5, 0);
        Em33ClothSet2(obj, &Obj18Cloth6, 0);
        break;
    case OBJ18_TYPE_INSECTBOSS1S:
        Em33ClothSet(obj, &Obj18Cloth5, 1);
        Em33ClothSet2(obj, &Obj18Cloth6, 1);
        break;
    case OBJ18_TYPE_ELGIGANTE:
        if (EvtMgr.GetBin(&cbin, "obj/objmodel/obm0700.bin", 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Mot : dat failed");
            return 0;
        }
        if (EvtMgr.GetBin(&ctpl, "obj/objmodel/obm0700.tpl", 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Mot : dat failed");
            return 0;
        }
        w->child = (cObj*) Em2bShortRopeSet(obj, &Obj18Cloth1, cbin, ctpl);
        if (w->child) {
            w->child->setNoSuspend(1);
        }
        break;
    }
    return obj;
}

// Destroys the body's child object (ribbon / rope) before the body itself is destroyed.
int DelObj18(cObj* pObj)
{
    if (pObj == 0) {
        pLog->err(0, 0, "Evt_SetElgiganteRope : pointer failed");
        return 0;
    }
    if (OBJ18_WK((cObj18*) pObj)->child) {
        ObjMgr.destroy(OBJ18_WK((cObj18*) pObj)->child);
    }
    return 1;
}

// Per-frame: motion, matrix, parent follow (dropped when the parent dies), parts (unless a double
// joint drives them), and the type's cloth move unless be_flag 0x40 (cloth off; Ashley's cloth is
// always off in the armour costume).
void cObj18::move()
{
    Obj18Work* w = OBJ18_WK(this);

    if (w->DebugFlag) {
        pLog->mes(0, 0, "cObj18:move DebugFlag");
    }
    if (Motion.pMot) {
        MotionMove(this, 0);
        partsWorldCalc();
    } else {
        RotMatrix(l_mat, &ang);
        TransMatrix(l_mat, &pos);
        ScaleMatrix(l_mat, &scale);
        PSMTXCopy(l_mat, mat);
    }
    if (w->pEm_oya) {
        if ((w->pEm_oya->be_flag & 0x201) != 1) {
            w->pEm_oya = 0;
        }
    }
    obj18SetOya(this);
    if (Motion.blendTbl == 0) {
        partsMatCalc();
        partsWorldCalc();
    }
    if (pG->game_costume == 1) {
        if (w->obj18_type == OBJ18_TYPE_ASHLEY) {
            w->be_flag &= ~0x40;
        }
    }
    if (!(w->be_flag & 0x40)) {
        switch (w->obj18_type) {
        case OBJ18_TYPE_LEON:
            if (pG->pl_type == 0) {
                PlClothMoveLeon(this, &Evt_leonHair, &Evt_leonJacket, &Evt_leonHolster);
            }
            break;
        case OBJ18_TYPE_ASHLEY:
            PlClothMoveGirl(this, &Evt_girlHair, &Evt_girlSkirt, &Evt_girlSweater);
            break;
        case OBJ18_TYPE_ADA:
            PlClothMoveAda(this, &Evt_adaRibbon, &Evt_adaDress, &Evt_adaHair);
            break;
        case OBJ18_TYPE_LUIS:
            PlClothMoveLuis(this, &Evt_luisHair);
            break;
        case OBJ18_TYPE_TRADER:
            break;
        case OBJ18_TYPE_MAYOR1:
            Em34ClothMove1(this, &Obj18Cloth1);
            Em34ClothMove2(this, &Obj18Cloth2);
            Em34ClothReset(this);
            break;
        case OBJ18_TYPE_NO2:
            Em37HairMove(this, &Obj18Cloth1);
            Em37CoatMove(this, &Obj18Cloth2);
            Em37ClothReset(this);
            break;
        case OBJ18_TYPE_SADDLER:
            Em30ClothMove1(this, &Obj18Cloth1);
            Em30ClothMove2(this, &Obj18Cloth2);
            Em30ClothReset(this);
            break;
        case OBJ18_TYPE_INSECTBOSS0:
            Em33ClothMove(this, &Obj18Cloth3);
            Em33ClothMove2(this, &Obj18Cloth4);
            Em33ClothReset(this);
            break;
        case OBJ18_TYPE_INSECTBOSS0S:
            Em33ClothMove(this, &Obj18Cloth3);
            Em33ClothMove2(this, &Obj18Cloth4);
            Em33ClothReset(this);
            break;
        case OBJ18_TYPE_INSECTBOSS1:
            Em33ClothMove(this, &Obj18Cloth5);
            Em33ClothMove2(this, &Obj18Cloth6);
            Em33ClothReset(this);
            break;
        case OBJ18_TYPE_INSECTBOSS1S:
            Em33ClothMove(this, &Obj18Cloth5);
            Em33ClothMove2(this, &Obj18Cloth6);
            Em33ClothReset(this);
            break;
        case OBJ18_TYPE_ELGIGANTE:
            break;
        }
    }
}

// Attaches an obj18 to parts partsNo of `oya` (no catch-up blend).
void OyaSetObj18(cObj* obj, cModel* oya, int partsNo)
{
    Obj18Work* w;

    if (obj == 0) {
        return;
    }
    if (obj->kindid != 1) {
        return;
    }
    if (obj->id != 0x18) {
        return;
    }
    w = OBJ18_WK((cObj18*) obj);
    w->pEm_oya = oya;
    w->oya_parts = partsNo;
    w->be_flag &= ~8;
    w->be_flag &= ~3;
}

// Parent model of an obj18 (1 when it has one with parts).
int obj18GetOya(cModel** pOya, cObj* pObj)
{
    *pOya = 0;
    if (OBJ18_WK((cObj18*) pObj)->pEm_oya == 0) {
        return 0;
    }
    if (OBJ18_WK((cObj18*) pObj)->pEm_oya->pParts == 0) {
        return 0;
    }
    *pOya = OBJ18_WK((cObj18*) pObj)->pEm_oya;
    return 1;
}

// Parent follow: parent parts matrix * own matrix, axes normalised, with the oya_hokan slerp catch-up
// (be_flag bit 3) from the saved matrix; copies the parent's light class 2.
void obj18SetOya(cObj18* pObj)
{
    Obj18Work* w = OBJ18_WK(pObj);
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec p;
    Quaternion q0;
    Quaternion q1;
    Quaternion q;

    if (w->pEm_oya == 0) {
        return;
    }
    if (w->pEm_oya->pParts == 0) {
        return;
    }
    PSMTXConcat(w->pEm_oya->getPartsPtr(w->oya_parts)->mat, pObj->mat, m);
    v0.x = m[0][0];
    v0.y = m[1][0];
    v0.z = m[2][0];
    v1.x = m[0][1];
    v1.y = m[1][1];
    v1.z = m[2][1];
    v2.x = m[0][2];
    v2.y = m[1][2];
    v2.z = m[2][2];
#line 976 "D:/Bio4/Prog/obj18.cpp"
    VECNormalize(&v0, &v0);
    VECNormalize(&v1, &v1);
    VECNormalize(&v2, &v2);
    m[0][0] = v0.x;
    m[1][0] = v0.y;
    m[2][0] = v0.z;
    m[0][1] = v1.x;
    m[1][1] = v1.y;
    m[2][1] = v1.z;
    m[0][2] = v2.x;
    m[1][2] = v2.y;
    m[2][2] = v2.z;
    if (w->oya_hokan < 1.0f) {
        w->oya_hokan += w->oya_hokan_add;
        if (w->oya_hokan >= 1.0f) {
            w->oya_hokan = 1.0f;
            w->be_flag &= ~8;
        }
    }
    if (w->be_flag & 8) {
        f32 rate = w->oya_hokan;
        f32 inv = 1.0f - rate;

        p.x = m[0][3] * rate + w->hokan_mat[0][3] * inv;
        p.y = m[1][3] * rate + w->hokan_mat[1][3] * inv;
        p.z = m[2][3] * rate + w->hokan_mat[2][3] * inv;
        C_QUATMtx(&q0, m);
        C_QUATMtx(&q1, w->hokan_mat);
        C_QUATSlerp(&q0, &q1, &q, w->oya_hokan);
        PSMTXQuat(pObj->mat, &q);
        TransMatrix(pObj->mat, &p);
        PSMTXCopy(pObj->mat, w->hokan_mat);
    } else {
        PSMTXCopy(m, pObj->mat);
    }
    if (w->pEm_oya) {
        if (w->pEm_oya->LightInfo.EnableMask & 2) {
            pObj->LightInfo.EnableMask &= ~0x10;
            pObj->LightInfo.EnableMask |= 2;
        }
    }
}

// Sets the event control flags (SetOm packet flag word).
void Obj18CmfSet(cObj* pObj, u32 commonFlag)
{
    if (pObj == 0) {
        return;
    }
    if (pObj->kindid != 1) {
        return;
    }
    if (pObj->id != 0x18) {
        return;
    }
    OBJ18_WK((cObj18*) pObj)->CommonFlag = commonFlag;
}

// Event control flags of an obj18 (0 for other objects).
u32 Obj18CmfGet(cObj* pObj)
{
    if (pObj == 0) {
        return 0;
    }
    if (pObj->kindid != 1 || pObj->id != 0x18) {
        return 0;
    }
    return OBJ18_WK((cObj18*) pObj)->CommonFlag;
}

// Sets one event control flag bit.
void Obj18CmfOn(cObj* pMod, u32 flag)
{
    u32 cmf[1];
    u32* p;

    cmf[0] = Obj18CmfGet(pMod);
    p = cmf;
    FlagOn(p, flag);
    Obj18CmfSet(pMod, cmf[0]);
}
