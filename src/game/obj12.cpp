// game/obj12: object id 0x12, throwable hanging object (D:/Bio4/Prog/obj12.cpp): an obj00 variant
// (parent follow with slerp catch-up, three-point rope fall) that an enemy can also throw at the
// player (throwMove: flies, hits with the obj12Atk attack record, then falls), with fall types
// (bounce factors), a landing sound, a Lost_wait despawn timer and a burn tint.
#include "atari.h"
#include "obj.h"
#include "obj12.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pad.h"
#include "motion.h"
#include "em_sub.h"

// One point of the falling rope (fallMove).
struct Obj12Node {
    Vec pos;
    Vec old;
    Vec spd;
    f32 len;
    int reflect;
};

// Per-frame: motion, parent follow (destroyed with the parent; catch-up blend on be_flag bit 3),
// throw flight (bit 8), rope fall (bit 2), parts/collision update, Lost_wait countdown to removal.
void cObj12::move()
{
    Obj12Work* w = OBJ12_WK(this);
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec p;
    Quaternion q0;
    Quaternion q1;
    Quaternion q;

    w->Motion_info = 0;
    if (Motion.pMot) {
        w->Motion_info = MotionMove(this, 0);
    }
    if (!(w->be_flag & 0x106)) {
        RotMatrix(l_mat, &ang);
        TransMatrix(l_mat, &pos);
        ScaleMatrix(l_mat, &scale);
        PSMTXCopy(l_mat, mat);
    }
    if (w->pEm_oya) {
        if ((w->pEm_oya->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
        if (w->pEm_oya->pList) {
            PSMTXConcat(w->pEm_oya->getPartsPtr(w->oya_parts)->mat, mat, m);
            if (!(w->be_flag & 0x80)) {
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
#line 102 "D:/Bio4/Prog/obj12.cpp"
                VECNormalize(&v0, &v0);
                if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                    v1.y = 1.0f;
                }
#line 104 "D:/Bio4/Prog/obj12.cpp"
                VECNormalize(&v1, &v1);
                if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                    v2.z = 1.0f;
                }
#line 106 "D:/Bio4/Prog/obj12.cpp"
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
                PSMTXQuat(mat, &q);
                TransMatrix(mat, &p);
                PSMTXCopy(mat, w->hokan_mat);
            } else {
                PSMTXCopy(m, mat);
            }
        }
        if (w->pEm_oya) {
            if (w->pEm_oya->LightInfo.EnableMask & 2) {
                LightInfo.EnableMask &= ~0x10;
                LightInfo.EnableMask |= 2;
            }
        }
    }
    throwMove();
    fallMove();
    if ((be_flag & 0x201) == 1) {
        if (!(w->be_flag & 6)) {
            partsMatCalc();
        }
        partsWorldCalc();
        chainMove();
        if (w->pEm_oya) {
            invisible_factor = w->pEm_oya->invisible_factor;
            invisible_factor2 = w->pEm_oya->invisible_factor2;
            if (w->pEm_oya->be_flag & 2) {
                be_flag |= 2;
            } else {
                be_flag &= ~2;
            }
        }
        if (G_ROOM_ID == 0x30F && (w->be_flag & 4)) {
            ObjMgr.destroy(this);
            return;
        }
        if (w->be_flag & 0x200) {
            if (w->Lost_wait) {
                w->Lost_wait--;
            } else {
                invisible_factor -= 0.1f;
                if (invisible_factor < 0.0f) {
                    invisible_factor = 0.0f;
                    be_flag &= ~2;
                    ObjMgr.destroy(this);
                }
            }
        }
    }
}

// Creates the object (back of the pool) at pos/rot with a 500 light volume and no parent.
cObj12* SetObj12(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj12* obj;
    Obj12Work* w;

    obj = (cObj12*) ObjMgr.createBack(cObjMgr::ID_EM12_WEAPON);
    if (obj) {
        w = OBJ12_WK(obj);
        if (obj->modelInit(bin, tpl) == 0) {
            pLog->err(0, 0, "SetObj12() modelInit() failed.");
            ObjMgr.destroy(obj);
            return 0;
        } else {
            static const Vec p0 = { 0.0f, 0.0f, 0.0f };
            static const Vec p1 = { 500.0f, 500.0f, 500.0f };

            obj->atari.off();
            obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
            obj->pos = *pos;
            obj->pos_old = *pos;
            obj->ang = *rot;
            w->oya_hokan = 1.0f;
            w->oya_hokan_add = 0.0f;
            w->pEm_oya = 0;
            w->oya_parts = 0;
            w->Motion_info = 0;
            w->Lost_wait = 0;
            return obj;
        }
    }
    return 0;
}

// Attaches to parts partsNo of `oya`; noNormalize keeps the parent's scale (be_flag 0x80).
void cObj12::setParent(cModel* oya, int partsNo, int noNormalize)
{
    Obj12Work* w = OBJ12_WK(this);

    w->pEm_oya = oya;
    w->oya_parts = partsNo;
    w->be_flag &= ~8;
    w->be_flag &= ~3;
    if (noNormalize) {
        w->be_flag |= 0x80;
    } else {
        w->be_flag &= ~0x80;
    }
}

// Follows the parent parts matrix (axes normalised unless be_flag 0x80) with the catch-up slerp.
void cObj12::chainMove()
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
}

// Sets the catch-up speed when the blend is at 0 (percent per frame, min 1).
// Never called (dead-stripped by the original linker, STRIP_UNUSED): its constant pool survives
// (double 0.0, the u32 -> f32 magic, 1.0).
static void obj12SetRate(cObj* obj, u32 rate)
{
    Obj12Work* w = OBJ12_WK((cObj12*) obj);

    if (w->oya_hokan == 0.0) {
        return;
    }
    w->oya_hokan_add = (f32) rate;
    if (w->oya_hokan_add < 1.0f) {
        w->oya_hokan_add = 1.0f;
    }
}

// Starts the rope fall (be_flag bit 2, parent dropped): node speeds from spd with random spread
// (or a random upward toss when spd is NULL), fall_type selects the bounce factors.
void cObj12::setFall(Vec* pSpd, u8 type)
{
    Obj12Work* w = OBJ12_WK(this);
    u32 i;
    f32 r;

    w->be_flag |= 4;
    w->pEm_oya = 0;
    for (i = 0; i < 3; i++) {
        if (pSpd) {
            if (i == 0) {
                r = fRand0_1();
                w->spd[i][0] = (s16) ((pSpd->x * 0.5f + pSpd->x * r) * 10.0f);
                r = fRand0_1();
                w->spd[i][1] = (s16) ((pSpd->y * 0.5f + pSpd->y * r) * 10.0f);
                r = fRand0_1();
                w->spd[i][2] = (s16) ((pSpd->z * 0.5f + pSpd->z * r) * 10.0f);
            } else {
                r = fRand0_1();
                w->spd[i][0] = (s16) ((pSpd->x * 0.5f + pSpd->x * r * 2.0f) * 10.0f);
                r = fRand0_1();
                w->spd[i][1] = (s16) ((pSpd->y * 0.5f + pSpd->y * r * 2.0f) * 10.0f);
                r = fRand0_1();
                w->spd[i][2] = (s16) ((pSpd->z * 0.5f + pSpd->z * r * 2.0f) * 10.0f);
            }
        } else {
            w->spd[i][0] = (s16) (fRand1_1() * 100.0f);
            w->spd[i][1] = (s16) (fRand1_1() * 100.0f) + 500;
            w->spd[i][2] = (s16) (fRand1_1() * 100.0f);
        }
    }
    w->be_flag |= 0x200;
    w->fall_se_id = 0xFF;
    w->fall_em_id = 0;
    w->fall_se_ck = 0;
    w->fall_type = type;
    w->Lost_wait = 90;
    w->fall_se_no = 0xFF;
}

// Sets the landing sound (block, number, enemy id; block 0xFF = none).
void cObj12::setFallSe(u8 se_id, u8 se_no, u8 em_id)
{
    Obj12Work* w = OBJ12_WK(this);

    w->fall_se_id = se_id;
    w->fall_se_no = se_no;
    w->fall_em_id = em_id;
    w->fall_se_ck = 0;
}

// Rope fall simulation (as obj00FallMove) with the floor from EatMgr + 50, per-type bounce
// damping, the landing sound below -50 y speed, and the resulting orientation/centre.
void cObj12::fallMove()
{
    Obj12Work* w = OBJ12_WK(this);
    Vec ofs[5][3] = {
        { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 1500.0f }, { 0.0f, 0.0f, 0.0f }, { 300.0f, 0.0f, 1300.0f } },
        { { -140.0f, 60.0f, 140.0f }, { -140.0f, 60.0f, -140.0f }, { 200.0f, 60.0f, 0.0f } },
        { { -140.0f, 30.0f, 140.0f }, { -140.0f, 30.0f, -140.0f }, { 200.0f, 30.0f, 0.0f } },
        { { 140.0f, -140.0f, 0.0f }, { -140.0f, -140.0f, 0.0f }, { 0.0f, 200.0f, 0.0f } },
    };
    Obj12Node node[3];
    Vec vx;
    Vec vy;
    Vec vz;
    Vec d;
    u32 i;
    u32 k;
    Obj12Node* p;
    Obj12Node* n;
    f32 mag;
    f32 diff;
    f32 floor;

    if (!(w->be_flag & 4)) {
        return;
    }
    floor = EatMgr.getFloor(&pos, 0, 600.0f, 100000.0f, 0) + 50.0f;
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.x = (f32) w->spd[i][0] * 0.1f;
        p->spd.y = (f32) w->spd[i][1] * 0.1f;
        p->spd.z = (f32) w->spd[i][2] * 0.1f;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        PSMTXMultVec(mat, &ofs[w->fall_type][i], &p->pos);
        p->old = p->pos;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        if (i == 2) {
            n = node;
        } else {
            n = &node[i + 1];
        }
        p->len = GetDistance3(&p->pos, &n->pos);
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.y -= 20.0f;
        PSVECAdd(&p->pos, &p->spd, &p->pos);
        p->reflect = 0;
    }
    for (k = 0; k < 30; k++) {
        for (i = 0; i < 3; i++) {
            p = &node[i];
            if (i == 2) {
                n = node;
            } else {
                n = &node[i + 1];
            }
            PSVECSubtract(&n->pos, &p->pos, &d);
            mag = PSVECMag(&d);
            diff = (p->len - mag) * 0.5f;
            PSVECScale(&d, &d, (1.0f / mag) * diff);
            PSVECAdd(&n->pos, &d, &n->pos);
            PSVECSubtract(&p->pos, &d, &p->pos);
            if (p->pos.y < floor) {
                p->pos.y = floor;
                p->reflect = 1;
            }
            if (n->pos.y < floor) {
                n->pos.y = floor;
                n->reflect = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        // dead in this loop, but its `i == 2` compare makes loop.c compute the `&node[2]`
        // bound in the preheader, where cse2 copies it from the k loop's final giv value
        if (i == 2) {
            n = node;
        } else {
            n = &node[i + 1];
        }
        if (p->reflect) {
            if (w->fall_se_ck == 0 && p->spd.y < -50.0f) {
                w->fall_se_ck = 1;
                if (w->fall_se_id != 0xFF) {
                    SndCall(w->fall_se_id, w->fall_se_no, &pos, w->fall_em_id, 0, 0);
                }
            }
            switch (w->fall_type) {
            default:
                p->spd.x *= fRand0_1() * 0.2f + 0.5f;
                p->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
                p->spd.z *= fRand0_1() * 0.2f + 0.5f;
                break;
            case 2:
            case 3:
                p->spd.x *= fRand0_1() * 0.2f + 0.4f;
                p->spd.y *= -(fRand0_1() * 0.1f + 0.3f);
                p->spd.z *= fRand0_1() * 0.2f + 0.4f;
                break;
            }
            if (p->spd.y <= 20.0f && p->spd.y > 0.0f) {
                p->spd.y = 0.0f;
            }
        } else {
            PSVECSubtract(&p->pos, &p->old, &p->spd);
        }
        PSVECScale(&p->spd, &p->spd, 0.999f);
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        w->spd[i][0] = (s16) (p->spd.x * 10.0f);
        w->spd[i][1] = (s16) (p->spd.y * 10.0f);
        w->spd[i][2] = (s16) (p->spd.z * 10.0f);
    }
    if (w->fall_type != 4) {
        PSVECSubtract(&node[0].pos, &node[1].pos, &vz);
        PSVECSubtract(&node[2].pos, &node[1].pos, &vx);
        PSVECCrossProduct(&vz, &vx, &vy);
        PSVECCrossProduct(&vy, &vz, &vx);
#line 1039 "D:/Bio4/Prog/obj12.cpp"
        VECNormalize(&vx, &vx);
        VECNormalize(&vy, &vy);
        VECNormalize(&vz, &vz);
    } else {
        PSVECSubtract(&node[0].pos, &node[1].pos, &vx);
        PSVECSubtract(&node[2].pos, &node[1].pos, &vy);
        PSVECCrossProduct(&vx, &vy, &vz);
        PSVECCrossProduct(&vz, &vx, &vy);
#line 1048 "D:/Bio4/Prog/obj12.cpp"
        VECNormalize(&vx, &vx);
        VECNormalize(&vy, &vy);
        VECNormalize(&vz, &vz);
    }
    mat[0][0] = vx.x;
    mat[1][0] = vx.y;
    mat[2][0] = vx.z;
    mat[0][1] = vy.x;
    mat[1][1] = vy.y;
    mat[2][1] = vy.z;
    mat[0][2] = vz.x;
    mat[1][2] = vz.y;
    mat[2][2] = vz.z;
    PSVECScale(&ofs[w->fall_type][0], &d, -1.0f);
    TransMatrix(mat, &node[0].pos);
    PSMTXMultVec(mat, &d, &d);
    TransMatrix(mat, &d);
    pos = d;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z +
          node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z +
          node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        pos.x = mat[0][3];
        pos.y = mat[1][3];
        pos.z = mat[2][3];
        Matrix2AxisAngle(mat, &ang);
        w->be_flag &= ~4;
    }
}

// Sets the throw speed (x, y*7.5, z*35 in 1/10 units) and faces the object along it.
// Never called (dead-stripped, STRIP_UNUSED): constant pool only (10, 75, 350, 0.0, pi/2).
static void obj12ThrowSet(cObj* obj, Vec* spd)
{
    Obj12Work* w = OBJ12_WK((cObj12*) obj);
    f32 ang;

    w->spd[0][0] = (s16) (spd->x * 10.0f);
    w->spd[0][1] = (s16) (spd->y * 75.0f);
    w->spd[0][2] = (s16) (spd->z * 350.0f);
    ang = atan2f(spd->x, spd->z);
    if (ang < 0.0f) {
        ang += 1.5707964f;
    }
    obj->ang.y = ang;
}

// Throw flight (be_flag bit 8): gravity 1.5/frame, hits the scenario (-> setFall) or the player
// (EmAtkHitCk with obj12Atk: power 8, damage 400; -> setFall + vibration); orients the object
// along its velocity.
void cObj12::throwMove()
{
    Obj12Work* w = OBJ12_WK(this);
    Vec spd;
    Mtx m;
    Vec up;
    Vec dir;
    f32 ang;

    if (!(w->be_flag & 0x100)) {
        return;
    }
    static EmAtkInfo obj12Atk = { 300.0f, PL_DM_AUTO, 400, 0, 10, 0 };

    w->spd[0][1] -= 15;
    spd.x = (f32) w->spd[0][0];
    spd.y = (f32) w->spd[0][1];
    spd.z = (f32) w->spd[0][2];
    PSVECAdd(&pos, &spd, &pos);
    if (EatMgr.hitCheck(&pos_old, &pos, 0, 0, 0, 0)) {
        w->be_flag &= ~0x100;
        setFall(0, 0);
    } else if (EmAtkHitCk(&obj12Atk, &pos, &pos_old, 1)) {
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
        w->be_flag &= ~0x100;
        setFall(0, 0);
    }
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXRotRad(m, 'y', this->ang.y);
    dir.x = 0.0f;
    dir.y = 0.0f;
    dir.z = 1.0f;
    PSMTXMultVecSR(m, &dir, &dir);
    if (dir.x == 0.0f) {
        dir.y = 0.0f;
    }
#line 1195 "D:/Bio4/Prog/obj12.cpp"
    VECNormalize(&dir, &dir);
    ang = acosf(PSVECDotProduct(&up, &dir));
    if (ang > 0.01f && ang < 3.1315927f) {
        PSVECCrossProduct(&up, &dir, &up);
        PSMTXRotAxisRad(m, &up, 0.62831855f);
        PSMTXConcat(m, mat, mat);
    }
    TransMatrix(mat, &pos);
}

// Tints every model info dark (burnt look).
void cObj12::setBurn()
{
    cModelInfo* info;

    for (info = pModelInfo; info; info = info->pList) {
        info->color[0] = 0x20;
        info->color[1] = 0x20;
        info->color[2] = 0x20;
    }
}
