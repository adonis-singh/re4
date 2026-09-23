// game/objMissile: object id 0x38, the helicopter missile (D:/Bio4/Prog/objMissile.cpp): mounted on
// a parts of the helicopter (R0 1 Parent), ignites (R0 2 FireWait), flies towards its target with
// increasing speed (R0 3 Fire) and explodes on the scenery or an enemy (objMissileBomb), clearing
// enemies with a large player-weapon hit sphere. Type 1 is the shootable variant with a hit box.
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "objMissile.h"
#include "esp.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "pl_wep.h"
#include "motion.h"

extern "C" {
void objMissile_R0_Set(cObjMissile* obj);
void objMissile_R0_Parent(cObjMissile* obj);
void objMissile_R0_FireWait(cObjMissile* obj);
void objMissile_R0_Fire(cObjMissile* obj);
void objMissile_R0_Lost(cObjMissile* obj);
void objMissileBomb(cObjMissile* obj, Vec* pos);
}

void (*ObjMissile_R0_move_tbl[5])(cObjMissile*) = {
    objMissile_R0_Set, objMissile_R0_Parent, objMissile_R0_FireWait, objMissile_R0_Fire, objMissile_R0_Lost,
};

// Creates a missile (id 0x38) of `type` (0 helicopter rocket; 1 a shootable missile with a cEmHit
// hit box that detonates it when shot) at pos/rot.
cObjMissile* SetHeliMissile(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type)
{
    cObjMissile* obj;
    MissileWork* w;

    obj = (cObjMissile*) ObjMgr.create(cObjMgr::ID_HELI_MISSILE);
    if (obj == 0) {
        return 0;
    }
    w = MISSILE_WK(obj);
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 2);
    AtariInit(&obj->atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->atari.off();
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
    obj->type = type;
    w->pHit = 0;
    if (obj->type == 1) {
        w->pHit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &obj->pos, &obj->ang, 1);
        if (w->pHit) {
            w->pHit->hp = 0;
            YarareInitCube(w->pHit, 0.0f, -300.0f, -300.0f, 300.0f, 600.0f, 600.0f, 1, YAT_FLAG_ON | YAT_FLAG_Z_AXIS);
            w->pHit->setParent(obj, 0, 0);
        }
    }
    obj->r_no_0 = 0;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    return obj;
}

// Per-frame: destroyed (with its hit box) when the launcher dies; runs the R0 routine.
void cObjMissile::move()
{
    MissileWork* w = MISSILE_WK(this);

    if (w->pEm_oya) {
        if ((w->pEm_oya->be_flag & 0x201) != 1 || ((cEm*) w->pEm_oya)->hp <= 0) {
            if (w->pHit) {
                EmMgr.destroy(w->pHit);
                w->pHit = 0;
            }
            ObjMgr.destroy(this);
            return;
        }
    }
    ObjMissile_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: free: matrices only.
void objMissile_R0_Set(cObjMissile* pObj)
{
    pObj->matUpdate();
}

// Rno0 == 1: mounted on parts oya_parts of the launcher (axes normalised unless scale_mode).
void objMissile_R0_Parent(cObjMissile* pObj)
{
    MissileWork* w = MISSILE_WK(pObj);
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    cModel* parent = w->pEm_oya;

    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    if (parent && parent->pList) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, pObj->mat, m);
        if (w->scale_mode == 0) {
            v0.x = m[0][0];
            v0.y = m[1][0];
            v0.z = m[2][0];
            v1.x = m[0][1];
            v1.y = m[1][1];
            v1.z = m[2][1];
            v2.x = m[0][2];
            v2.y = m[1][2];
            v2.z = m[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 249 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 251 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 253 "D:/Bio4/Prog/objMissile.cpp"
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
        }
        PSMTXCopy(m, pObj->mat);
    }
    if (pObj->Motion.pMot) {
        pObj->Motion.Mot_flag |= 0x40000000;
        MotionMove(pObj, 0);
    } else {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Rno0 == 2 (setFire): 15 frames on the mount with the ignition effect (type 0: est 0x32/4),
// then Fire.
void objMissile_R0_FireWait(cObjMissile* pObj)
{
    MissileWork* w = MISSILE_WK(pObj);
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    cModel* parent = w->pEm_oya;

    switch (pObj->r_no_2) {
    case 0:
        w->Timer = 15;
        switch (pObj->type) {
        case 0:
        default:
            EstSet(pObj, -1, 0, 0, EFF_EM3D, 4, 0, ESP_CORE_KIND_NONE, pObj, 0);
            break;
        case 1:
            break;
        }
        pObj->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else {
            pObj->r_no_0 = 3;
            pObj->r_no_1 = 0;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        }
        break;
    }
    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    if (parent && parent->pList) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, pObj->mat, m);
        if (w->scale_mode == 0) {
            v0.x = m[0][0];
            v0.y = m[1][0];
            v0.z = m[2][0];
            v1.x = m[0][1];
            v1.y = m[1][1];
            v1.z = m[2][1];
            v2.x = m[0][2];
            v2.y = m[1][2];
            v2.z = m[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 341 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 343 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 345 "D:/Bio4/Prog/objMissile.cpp"
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
        }
        PSMTXCopy(m, pObj->mat);
    }
    if (pObj->Motion.pMot) {
        pObj->Motion.Mot_flag |= 0x40000000;
        MotionMove(pObj, 0);
    } else {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Rno0 == 3: launch from the mount aimed at Target (type 0: 300 units/frame with the smoke trail
// and sound, type 1: 150), speed x1.1 per frame, 90-frame limit; explodes (objMissileBomb) on the
// scenario 300 units back along the path, on a character (type 1), or when its hit box is shot.
void objMissile_R0_Fire(cObjMissile* pObj)
{
    MissileWork* w = MISSILE_WK(pObj);

    if (pObj->r_no_2 == 0) {
        Vec d;

        pObj->pos.x = pObj->mat[0][3];
        pObj->pos.y = pObj->mat[1][3];
        pObj->pos.z = pObj->mat[2][3];
        pObj->pos_old = pObj->pos;
        Matrix2AxisAngle(pObj->mat, &pObj->ang);
        if (w->pHit) {
            w->pHit->hp = 1;
        }
        if (w->Target_ok) {
            f32 len;

            PSVECSubtract(&w->Target, &pObj->pos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            pObj->ang.x = -atan2f(d.y, len);
            pObj->ang.y = atan2f(d.x, d.z);
            pObj->ang.z = 0.0f;
            RotMatrix(pObj->mat, &pObj->ang);
            TransMatrix(pObj->mat, &pObj->pos);
        }
        w->Timer = 90;
        w->Timer2 = 3;
        switch (pObj->type) {
        case 0:
        default:
            EstSet(pObj, -1, 0, 0, EFF_EM3D, 5, 0, ESP_CORE_KIND_NONE, pObj, 0);
            SndCall(6, 2, &pObj->pos, 0, 0, pObj);
            w->Spd.x = 0.0f;
            w->Spd.y = 0.0f;
            w->Spd.z = 300.0f;
            break;
        case 1:
            w->Spd.x = 0.0f;
            w->Spd.y = 0.0f;
            w->Spd.z = 150.0f;
            break;
        }
        PSMTXMultVecSR(pObj->mat, &w->Spd, &w->Spd);
        w->pEm_oya = 0;
        pObj->r_no_2++;
    }
    Vec hit;
    Vec nrm;

    PSVECAdd(&pObj->pos, &w->Spd, &pObj->pos);
    PSVECScale(&w->Spd, &w->Spd, 1.1f);
    if (w->Timer2) {
        w->Timer2--;
    } else {
        if (EatMgr.hitCheck(&pObj->pos_old, &pObj->pos, &hit, 0, 0, 0)) {
            PSVECSubtract(&pObj->pos_old, &pObj->pos, &nrm);
            if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
                objMissileBomb(pObj, &hit);
                return;
            }
#line 450 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, 300.0f);
            PSVECAdd(&hit, &nrm, &hit);
            objMissileBomb(pObj, &hit);
            return;
        }
    }
    if (pObj->type == 1) {
        Vec nrm2;

        if (EmAtkLineHitCk(&pObj->pos_old, &pObj->pos, &hit, &nrm2, 0)) {
            objMissileBomb(pObj, &hit);
            return;
        }
        if (w->pHit) {
            if (w->pHit->ckDmgWeapon()) {
                objMissileBomb(pObj, &pObj->pos);
                return;
            }
        }
    }
    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    if (pObj->Motion.pMot) {
        pObj->Motion.Mot_flag |= 0x40000000;
        MotionMove(pObj, 0);
    } else {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
    if (w->Timer) {
        w->Timer--;
    } else {
        pObj->r_no_0 = 4;
        pObj->r_no_1 = 0;
        pObj->r_no_2 = 0;
        pObj->r_no_3 = 0;
    }
}

// Rno0 == 4: removes the missile and its hit box.
void objMissile_R0_Lost(cObjMissile* pObj)
{
    MissileWork* w = MISSILE_WK(pObj);

    pObj->be_flag &= ~2;
    if (w->pHit) {
        EmMgr.destroy(w->pHit);
        w->pHit = 0;
    }
    ObjMgr.destroy(pObj);
}

// Mounts the missile on parts oya_parts of `parent` -> Parent.
void cObjMissile::setParent(cModel* parent, int partsNo, int noNormalize)
{
    MissileWork* w = MISSILE_WK(this);

    w->pEm_oya = parent;
    w->oya_parts = partsNo;
    w->scale_mode = noNormalize;
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Fires the missile at `target` (NULL = straight ahead) -> FireWait.
void cObjMissile::setFire(Vec* pTarget)
{
    MissileWork* w = MISSILE_WK(this);

    w->Target_ok = 0;
    if (pTarget) {
        w->Target = *pTarget;
        w->Target_ok = 1;
    }
    r_no_0 = 2;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Explosion at pos: type 0 est 0x32/7, sound and a player weapon hit sphere (kind 0x12, radius
// 8000) that destroys enemies; type 1 est 2/5 with radius 2000 (kind 0x13). Kills the hit box, in
// r320 raises Room_flg[0] 0x80000000, -> Lost.
void objMissileBomb(cObjMissile* pObj, Vec* pPos)
{
    MissileWork* w = MISSILE_WK(pObj);

    switch (pObj->type) {
    case 0:
    default:
        EstSet(0, -1, pPos, 0, EFF_EM3D, 7, 0, ESP_CORE_KIND_NONE, 0, 0);
        SndCall(6, 3, &pObj->pos, 0, 0, pObj);
        PlWepHitCheck2(0, &pObj->pos_old, &pObj->pos_old, 0x12, 3, 8000.0f);
        break;
    case 1:
        EstSet(0, -1, pPos, 0, EFF_EM3A, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
        PlWepHitCheck2(0, &pObj->pos_old, &pObj->pos_old, 0x13, 3, 2000.0f);
        break;
    }
    if (w->pHit) {
        EmMgr.destroy(w->pHit);
        w->pHit = 0;
    }
    if (G_ROOM_ID == 0x320) {
        pG->Room_flg[0] |= 0x80000000;  // RMF_TARGET_DESTROY (r320)
    }
    pObj->r_no_0 = 4;
    pObj->r_no_1 = 0;
    pObj->r_no_2 = 0;
    pObj->r_no_3 = 0;
}
