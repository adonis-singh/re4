// game/obj1b: object id 0x1B, the spear (D:/Bio4/Prog/obj1b.cpp) thrown by the em2f/em3x
// spear-carriers. R1 routines: 0 Set (held, plain motion), 1 LostWait (2 s then fade), 2 Lost
// (destroyed), 3 Parent (stuck in a parts of the victim, falls off after parentTimer), 4 Fall
// (three-point rope fall), 5 Throw (flies along throwSpd, hits the scenario or a character through
// GetWepTargetList2 and sticks). setParent / setFall / setThrow / setLost switch the routines.
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "esp.h"
#include "est.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "em.h"
#include "main_mem.h"
#include "motion.h"
#include "em_sub.h"

// Spear (obj 0x1B): thrown by an enemy (R1_Throw), sticks into the enemy it hits (R1_Parent:
// follows a parts of the target), falls off as a three-point rope (R1_Fall) and fades out (Lost).
class cObjSpear : public cObjUnion {
public:
    virtual void move();
    virtual void beginEvent(u32 mode);
    virtual ~cObjSpear() {}

    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setFall(u8 type, Vec* dir);
    void setThrow(Vec* dir);
    void setLost();
};

// One point of the falling rope (obj1b_R1_Fall).
struct Obj1bNode {
    Vec pos;
    Vec old;
    Vec spd;
    f32 len;
    int reflect;
};

// EspSeqOpt as the spear fills it: flag byte 2, speed vector at 4.
struct SpearEstOpt {
    u8 x0;
    u8 x1;
    u8 flag;
    u8 x3;
    Vec spd;
    u8 pad_10[0xC];
};

extern "C" {
void obj1b_R1_Set(cObjSpear* obj);
void obj1b_R1_LostWait(cObjSpear* obj);
void obj1b_R1_Lost(cObjSpear* obj);
void obj1b_R1_Parent(cObjSpear* obj);
void obj1b_R1_Fall(cObjSpear* obj);
void obj1b_R1_Throw(cObjSpear* obj);
int obj1bHitCk(cObjSpear* obj);
}

void (*Obj1b_R1_move_tbl[6])(cObjSpear*) = { obj1b_R1_Set, obj1b_R1_LostWait, obj1b_R1_Lost, obj1b_R1_Parent, obj1b_R1_Fall, obj1b_R1_Throw };

// Creates a spear at pos/rot with all sound/effect ids unset (0xFF), effect kind 0x32.
cObj* SetSpear(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    SpearWork* w;

    obj = ObjMgr.create(cObjMgr::ID_SPEAR);
    if (obj == 0) {
        return 0;
    }
    w = &((cObjSpear*) obj)->spear;
    if (pos) {
        obj->pos = *pos;
    }
    if (rot) {
        obj->ang = *rot;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj1b() modelInit() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 3000.0f, 3000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->LightInfo.init2(0, 1, &p0, &p1, 4);
    w->flags = 0;
    w->parentTimer = 0;
    w->estTimer = 0;
    w->x50 = 0;
    w->type = 0;
    w->seBlk = 0xFF;
    w->seNo = 0xFF;
    w->seId = 0;
    w->sePlayed = 0;
    w->se2Blk = 0xFF;
    w->se2No = 0xFF;
    w->se2Id = 0;
    w->se3Blk = 0xFF;
    w->se3No = 0xFF;
    w->se3Id = 0;
    w->throwSeBlk = 0xFF;
    w->throwSeNo = 0xFF;
    w->throwSeId = 0;
    w->estNo = 0xFF;
    w->estPrm = 0xFF;
    // Store order read off the scheduler: the last RTL use of the 0xFF register (x6D) is the
    // dying store and is issued first among the 0xFF stores, the rest follow in RTL order with
    // x6C last; espId's `li 50` is issued late so its store leads the block. xFD/xFE/xFF ascending
    // gives the target's 255, 253, 254 issue order.
    w->x6E = 0xFF;
    w->x6F = 0xFF;
    w->x6C = 0xFF;
    w->x6D = 0xFF;
    w->espId = 0x32;
    obj->r_no_0 = 1;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    RotMatrix(obj->mat, &obj->ang);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    obj->partsMatCalc();
    obj->partsWorldCalc();
    return obj;
}

// Event start: a loose spear (no parent) is removed.
void cObjSpear::beginEvent(u32 flag)
{
    if (spear.parent == 0) {
        ObjMgr.destroy(this);
    }
}

// Per-frame: dies with its parent, runs the R1 routine, inherits the parent's light class,
// invisibility and draw flag; hidden when stuck in the player during Status_flg[0] 0x400.
void cObjSpear::move()
{
    SpearWork* w = &spear;

    if (w->parent && (w->parent->be_flag & 0x201) != 1) {
        ObjMgr.destroy(this);
        return;
    }
    Obj1b_R1_move_tbl[r_no_1](this);
    if ((be_flag & 0x201) != 1) {
        return;
    }
    if (w->parent == 0) {
        return;
    }
    if (w->parent->LightInfo.EnableMask & 2) {
        LightInfo.EnableMask = (LightInfo.EnableMask & ~0x10) | 2;
    }
    if (w->parent == 0) {
        return;
    }
    invisible_factor = w->parent->invisible_factor;
    invisible_factor2 = w->parent->invisible_factor2;
    if (w->parent->be_flag & 2) {
        be_flag |= 2;
    } else {
        be_flag &= ~2;
    }
    if (w->parent == 0) {
        return;
    }
    if (w->parent->id == 0 && (StaFlagChk(pG, STA_BINOCULAR))) {
        be_flag &= ~2;
    }
}

// Rno1 == 0: motion and matrices (the spear as held by the thrower).
void obj1b_R1_Set(cObjSpear* pObj)
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

// Rno1 == 1: waits 120 frames then fades out (or vanishes at once when off screen) -> Lost.
void obj1b_R1_LostWait(cObjSpear* pObj)
{
    SpearWork* w = &pObj->spear;
    Vec scr;
    Vec p;

    switch (pObj->r_no_2) {
    case 0:
        w->timer = 120;
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
        p = pObj->pos;
        GetScreenPos(&p, &scr);
        if (scr.z > 1.0f) {
            pObj->r_no_0 = 1;
            pObj->r_no_1 = 2;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
        }
        break;
    }
    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    pObj->partsMatCalc();
    pObj->partsWorldCalc();
}

// Rno1 == 2: hides and destroys the spear.
void obj1b_R1_Lost(cObjSpear* pObj)
{
    if (pObj->r_no_2 == 0) {
        pObj->be_flag &= ~2;
        pObj->be_flag &= ~0x20;
        ObjMgr.destroy(pObj);
        pObj->r_no_2++;
    }
}

// Rno1 == 3: stuck in parts partsNo of the parent (matrix under the parts, axes normalised unless
// flags bit 0), falls off when parentTimer expires, blood effects every other frame while estTimer
// runs.
void obj1b_R1_Parent(cObjSpear* pObj)
{
    SpearWork* w = &pObj->spear;
    cModel* parent = w->parent;

    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    ScaleMatrix(pObj->mat, &pObj->scale);
    if (parent && parent->pParts) {
        Mtx m;
        Vec v0;
        Vec v1;
        Vec v2;
        cModel* parts = parent->getPartsPtr(w->partsNo);

        PSMTXConcat(parts->mat, pObj->mat, m);
        if (!(w->flags & 1)) {
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
#line 356 "D:/Bio4/Prog/obj1b.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 358 "D:/Bio4/Prog/obj1b.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 360 "D:/Bio4/Prog/obj1b.cpp"
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
        ScaleMatrix(pObj->mat, &pObj->scale);
    }
    if (pObj->Motion.pMot) {
        pObj->Motion.Mot_flag |= 0x40000000;
        MotionMove(pObj, 0);
    } else {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
    if (w->parentTimer != 0 && --w->parentTimer == 0) {
        pObj->setFall(0, 0);
    } else if (parent && parent->id == 0x2F && w->estTimer) {
        w->estTimer--;
        if ((w->estTimer & 1) == 0) {
            if (StaFlagChk(pG, STA_WATER_CAMERA)) {
                Vec p;

                p.x = 0.0f;
                p.y = 0.0f;
                p.z = 0.0f;
                PSMTXMultVec(pObj->mat, &p, &p);
                EstSet(0, -1, &p, 0, EFF_EM2F, 0xB, 0, ESP_CORE_KIND_NONE, 0, 0);
            } else {
                Vec p;

                p.x = 0.0f;
                p.y = 0.0f;
                p.z = 0.0f;
                PSMTXMultVec(pObj->mat, &p, &p);
                EstSet(0, -1, &p, 0, EFF_EM2F, 0xB, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
    }
}

// Rno1 == 4: three-point rope fall (nodes by `type` offsets, floor from EatMgr + 50, per-type bounce
// damping, landing sound and effect once), then at rest (node speeds < 25) -> LostWait.
void obj1b_R1_Fall(cObjSpear* obj)
{
    SpearWork* w = &obj->spear;
    Vec ofs[4][3] = {
        { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 1500.0f }, { 0.0f, 0.0f, 0.0f }, { 300.0f, 0.0f, 1300.0f } },
        { { -140.0f, 60.0f, 140.0f }, { -140.0f, 60.0f, -140.0f }, { 200.0f, 60.0f, 0.0f } },
        { { -140.0f, 30.0f, 140.0f }, { -140.0f, 30.0f, -140.0f }, { 200.0f, 30.0f, 0.0f } },
    };
    Obj1bNode node[3];
    Vec vx;
    Vec vy;
    Vec vz;
    Vec d;
    u32 i;
    u32 k;
    Obj1bNode* p;
    Obj1bNode* n;
    f32 floor;
    f32 mag;
    f32 diff;

    floor = EatMgr.getFloor(&obj->pos, 0, 600.0f, 100000.0f, 0) + 50.0f;
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.x = w->spd[i].x;
        p->spd.y = w->spd[i].y;
        p->spd.z = w->spd[i].z;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        PSMTXMultVec(obj->mat, &ofs[w->type][i], &p->pos);
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
        // dead here, but its `i == 2` compare makes loop.c compute the `&node[2]` bound in the
        // preheader, where cse2 copies it from the k loop's final giv value (see obj12)
        if (i == 2) {
            n = node;
        } else {
            n = &node[i + 1];
        }
        if (p->reflect) {
            if (w->sePlayed == 0 && p->spd.y < -50.0f) {
                w->sePlayed = 1;
                if (w->seBlk != 0xFF) {
                    SndCall(w->seBlk, w->seNo, &obj->pos, w->seId, 0, 0);
                }
                if (w->estNo != 0xFF && w->estPrm != 0xFF) {
                    EstSet(obj, -1, 0, 0, w->estNo, w->estPrm, 0, ESP_CORE_KIND_NONE, obj, 0);
                }
                EffectEspDelete(0, w->espId, obj, 0);
                EffectEspgenDelete(0, w->espId, obj);
                EffectEfmDelete(0, w->espId, obj);
            }
            switch (w->type) {
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
        w->spd[i].x = p->spd.x;
        w->spd[i].y = p->spd.y;
        w->spd[i].z = p->spd.z;
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &vz);
    PSVECSubtract(&node[2].pos, &node[1].pos, &vx);
    PSVECCrossProduct(&vz, &vx, &vy);
    PSVECCrossProduct(&vy, &vz, &vx);
#line 634 "D:/Bio4/Prog/obj1b.cpp"
    VECNormalize(&vx, &vx);
    VECNormalize(&vy, &vy);
    VECNormalize(&vz, &vz);
    obj->mat[0][0] = vx.x;
    obj->mat[1][0] = vx.y;
    obj->mat[2][0] = vx.z;
    obj->mat[0][1] = vy.x;
    obj->mat[1][1] = vy.y;
    obj->mat[2][1] = vy.z;
    obj->mat[0][2] = vz.x;
    obj->mat[1][2] = vz.y;
    obj->mat[2][2] = vz.z;
    PSVECScale(&ofs[w->type][0], &d, -1.0f);
    TransMatrix(obj->mat, &node[0].pos);
    PSMTXMultVec(obj->mat, &d, &d);
    TransMatrix(obj->mat, &d);
    obj->pos = d;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z +
            node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z +
            node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        obj->pos.x = obj->mat[0][3];
        obj->pos.y = obj->mat[1][3];
        obj->pos.z = obj->mat[2][3];
        Matrix2AxisAngle(obj->mat, &obj->ang);
        obj->r_no_0 = 1;
        obj->r_no_1 = 1;
        obj->r_no_2 = 0;
        obj->r_no_3 = 0;
    }
    obj->partsWorldCalc();
}

// Rno1 == 5: flies along throwSpd (whoosh sound every 4 frames, 60-frame limit -> Lost), oriented
// along the velocity; sticks into the scenario (-> LostWait) or a character (obj1bHitCk).
void obj1b_R1_Throw(cObjSpear* pObj)
{
    SpearWork* w = &pObj->spear;
    Vec d;
    Vec hit;
    Vec p;
    f32 wh;
    f32 len;

    switch (pObj->r_no_2) {
    case 0:
        w->timer = 0;
        w->timer2 = 60;
        pObj->r_no_2++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = 4;
            if (w->throwSeBlk != 0xFF && w->throwSeNo != 0xFF) {
                SndCall(w->throwSeBlk, w->throwSeNo, &pObj->pos, w->throwSeId, 0, 0);
            }
        }
        if (w->timer2 == 0) {
            pObj->r_no_0 = 1;
            pObj->r_no_1 = 2;
            pObj->r_no_2 = 0;
            pObj->r_no_3 = 0;
            return;
        }
        w->timer2--;
        break;
    }
    w->throwSpd.y -= 15.0f;
    PSVECAdd(&pObj->pos, &w->throwSpd, &pObj->pos);
    if (EatMgr.hitCheck(&pObj->pos_old, &pObj->pos, &hit, 0, 0, 0)) {
        pObj->pos = hit;
        SndCall(6, 1, &pObj->pos, 0, 0, 0);
        pObj->r_no_0 = 1;
        pObj->r_no_1 = 1;
        pObj->r_no_2 = 0;
        pObj->r_no_3 = 0;
        return;
    }
    PSVECSubtract(&pObj->pos, &pObj->pos_old, &d);
    len = SQRTF(d.x * d.x + d.z * d.z);
    pObj->ang.x = -atan2f(d.y, len);
    pObj->ang.y = atan2f(d.x, d.z);
    pObj->ang.z = 0.0f;
    RotMatrix(pObj->mat, &pObj->ang);
    TransMatrix(pObj->mat, &pObj->pos);
    pObj->partsWorldCalc();
    if (GetWaterHeight(&pObj->pos, &wh)) {
        if (pObj->pos.y > wh - 3000.0f) {
            if (obj1bHitCk(pObj)) {
                return;
            }
        }
        if (wh < pObj->pos_old.y && wh > pObj->pos.y) {
            p = pObj->pos;
            p.y = wh;
            EstSet(0, -1, &p, 0, EFF_PL0F, 0x13, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(6, 0, &p, 0, 0, 0);
        }
    } else {
        obj1bHitCk(pObj);
    }
}

// Sweep pos_old -> pos against characters (GetWepTargetList2, attribute 0x15): damages the victim
// (power 10) and sticks the spear into the hit parts (local position 50 units back along the
// hit, scale 1.5; a non-pierceable parts sticks at the parts origin), blood effects (special
// speed-following variant for em2f 0x2F), sound; estTimer 600, falls off after 1800 frames.
int obj1bHitCk(cObjSpear* pObj)
{
    SpearWork* w = &pObj->spear;
    Vec hit;
    Vec nrm;
    WepTarget target;
    u32 attr;
    cEm* em;
    YARARE_INFO* part;
    int no;
    f32 len;

    if (GetWepTargetList2(&pObj->pos_old, &pObj->pos, &target, 1, &hit, &nrm, &attr, 0x15, 0)) {
        part = target.part;
        em = target.em;
        em->dmg.set(0, 10, 0x15, &em->pos_old, part->len, part);
        if (part->flag & YAT_FLAG_DMPOS) {
            Mtx inv;
            Vec v;
            cModel* parts;

            no = part->parts_no ? part->parts_no - 1 : 0;
            parts = em->getPartsPtr(no);
            PSMTXInverse(parts->mat, inv);
            PSMTXMultVec(inv, &part->cross, &pObj->pos);
#line 825 "D:/Bio4/Prog/obj1b.cpp"
            VECNormalize(&pObj->pos, &v);
            PSVECScale(&v, &v, -50.0f);
            PSVECAdd(&pObj->pos, &v, &pObj->pos);
            len = SQRTF(pObj->pos.x * pObj->pos.x + pObj->pos.z * pObj->pos.z);
            pObj->ang.x = -atan2f(-pObj->pos.y, len);
            pObj->ang.y = atan2f(-pObj->pos.x, -pObj->pos.z);
            pObj->ang.z = 0.0f;
        } else {
            no = 0;
            pObj->pos.x = 0.0f;
            pObj->pos.y = 0.0f;
            pObj->pos.z = 0.0f;
            pObj->ang.x = 0.0f;
            pObj->ang.y = 0.0f;
            pObj->ang.z = 0.0f;
        }
        pObj->scale.x = 1.5f;
        pObj->scale.y = 1.5f;
        pObj->scale.z = 1.5f;
        pObj->setParent(em, no, 0);
        obj1b_R1_Parent(pObj);
        if (em->id == 0x2F) {
            SpearEstOpt opt;
            Vec d;

            PSVECSubtract(&em->pos, &em->pos_old, &d);
            memclr_asm(&opt, sizeof(SpearEstOpt));
            opt.flag = 1;
            opt.spd = d;
            EstSet(pObj, -1, 0, 0, EFF_EM2F, 0, 0, ESP_CORE_KIND_NONE, pObj, &opt);
            EstSet(pObj, -1, 0, 0, EFF_EM2F, 5, 0, ESP_CORE_KIND_NONE, pObj, 0);
            SndCall(8, 4, &pObj->pos_old, em->id, 0, 0);
            w->estTimer = 600;
        }
        w->parentTimer = 1800;
        return 1;
    }
    return 0;
}

// Sticks / holds the spear on parts partsNo of `parent` (noNormalize keeps the parts scale) -> Parent.
void cObjSpear::setParent(cModel* pEm, int oya_parts, int mode)
{
    SpearWork* w = &spear;

    w->parent = pEm;
    w->partsNo = oya_parts;
    if (mode) {
        w->flags |= 1;
    } else {
        w->flags &= ~1;
    }
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Starts the rope fall of type `type` with node speeds from dir (nodes 1/2 rotated +-90 degrees) or
// a random upward toss.
void cObjSpear::setFall(u8 type, Vec* pSpd)
{
    SpearWork* w = &spear;
    Mtx m;
    Vec v;
    u32 i;
    f32 ang;

    Motion.pMot = 0;
    for (i = 0; i < 3; i++) {
        if (pSpd) {
            switch (i) {
            default:  // shares case 0: the dispatch falls into it (no `b end`), which lets haifa
                      // pull case 2's address insns into the dispatch block and case 1's tail
            case 0:
                w->spd[i].x = pSpd->x;
                w->spd[i].y = pSpd->y;
                w->spd[i].z = pSpd->z;
                break;
            case 1:
                if (pSpd->x == 0.0f && pSpd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(pSpd->x, pSpd->z);
                }
                PSMTXRotRad(m, 'y', ang + 1.5707964f);
                PSMTXMultVec(m, pSpd, &v);
                w->spd[i].x = v.x;
                w->spd[i].y = v.y;
                w->spd[i].z = v.z;
            case 2:
                if (pSpd->x == 0.0f && pSpd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(pSpd->x, pSpd->z);
                }
                PSMTXRotRad(m, 'y', ang - 1.5707964f);
                PSMTXMultVec(m, pSpd, &v);
                w->spd[i].x = v.x;
                w->spd[i].y = v.y;
                w->spd[i].z = v.z;
                break;
            }
        } else {
            w->spd[i].x = fRand1_1() * 10.0f;
            w->spd[i].y = fRand1_1() * 10.0f + 50.0f;
            w->spd[i].z = fRand1_1() * 10.0f;
            break;
        }
    }
    w->type = type;
    w->parent = 0;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &this->ang);
    r_no_0 = 1;
    r_no_1 = 4;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Throws the spear along dir (or its own forward axis * 1000) from its current position -> Throw.
void cObjSpear::setThrow(Vec* pSpd)
{
    SpearWork* w = &spear;
    Vec d;
    f32 len;

    if (pSpd) {
        w->throwSpd.x = pSpd->x;
        w->throwSpd.y = pSpd->y;
        w->throwSpd.z = pSpd->z;
    } else {
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1000.0f;
        PSMTXMultVecSR(mat, &d, &w->throwSpd);
    }
    len = SQRTF(d.x * d.x + d.z * d.z);
    ang.x = -atan2f(d.y, len);
    ang.y = atan2f(w->throwSpd.x, w->throwSpd.z);
    ang.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    pos_old = pos;
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    w->parent = 0;
    r_no_0 = 1;
    r_no_1 = 5;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Removes the spear at once (-> Lost).
void cObjSpear::setLost()
{
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
}
