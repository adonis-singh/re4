// game/obj00: object id 0, the hanging object (D:/Bio4/Prog/obj00.cpp): lamps, signs and other
// props that hang from a parts of a parent model (OyaSetObj00) with a slerp catch-up, fall as a
// three-point rope simulation when cut (Obj00Work be_flag bit 2) and fade out when flagged
// (bit 5). SetObj00 creates it from a bin/tpl; MotSetObj00 plays a motion on it.
#include "atari.h"
#include "atari_init.h"
#include "obj.h"
#include "obj00.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "motion.h"

// One point of the falling rope (obj00FallMove).
struct Obj00Node {
    Vec pos;
    Vec old;
    Vec spd;
    f32 len;
    int reflect;
};

extern "C" {
void obj00FallMove(cObj00* obj);
void obj00SetOya(cObj00* obj);
}

// Per-frame: plays the motion when set; follows the parent (destroyed with it), runs the fall
// simulation, updates the parts and collision unless flagged, fades out on be_flag 0x20.
void cObj00::move()
{
    Obj00Work* w = OBJ00_WK(this);

    if (Motion.pMot) {
        MotionMove(this, 0);
    } else if (!(w->be_flag & 0x16)) {
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
    }
    obj00SetOya(this);
    obj00FallMove(this);
    if (Motion.pMot == 0) {
        if (!(w->be_flag & 0x16)) {
            partsMatCalc();
        }
    }
    partsWorldCalc();
    sub2B4.atari.move();
    SatMgr.check(this, 0);
    if (w->be_flag & 0x20) {
        invisible_factor -= 0.1f;
        if (invisible_factor < 0.0f) {
            invisible_factor = 0.0f;
            be_flag &= ~2;
        }
    }
}

// Creates an obj00 from the model files at pos/rot (defaults 0), collision pass-through, light
// volume 3000, no parent.
cObj* SetObj00(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    Obj00Work* w;

    obj = ObjMgr.create(cObjMgr::ID_NORMAL);
    if (obj == 0) {
        return 0;
    }
    w = OBJ00_WK((cObj00*) obj);
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 3000.0f, 3000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
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
    w->pEm_oya = 0;
    w->oya_parts = 0;
    w->oya_hokan = 1.0f;
    w->oya_hokan_add = 0.0f;
    return obj;
}

// Starts motion `mot` on the object with Mot_attr prm.
void MotSetObj00(cObj* obj, void* mot, int prm, int a)
{
    Obj00Work* w = OBJ00_WK((cObj00*) obj);

    if (obj == 0) {
        return;
    }
    w->pMot = mot;
    w->mot_attr = prm;
    w->motA = a;
    MotionSetCore(obj, &obj->Motion, mot, (void*) a, 0, (u16) w->mot_attr, 0);
}

// Attaches the object to parts partsNo of `oya` (motion cleared, no catch-up blend).
void OyaSetObj00(cObj* obj, cModel* oya, int partsNo)
{
    Obj00Work* w = OBJ00_WK((cObj00*) obj);

    if (obj == 0) {
        return;
    }
    w->pEm_oya = oya;
    w->oya_parts = partsNo;
    obj->Motion.pMot = 0;
    w->be_flag &= ~8;
}

// Sets the parent catch-up speed (percent per frame, min 1).
// Never called: the original linker dropped the body but kept its constant pool.
static void obj00SetRate(cObj* obj, u32 rate)
{
    f32 r = (f32) rate;

    if (r < 1.0f) {
        r = 1.0f;
    }
    OBJ00_WK((cObj00*) obj)->oya_hokan_add = r / 100.0f;
}

// Fall simulation (be_flag bit 2): three rope nodes 300 units around the object fall under gravity
// (20/frame), keep their mutual distances (30 relaxation passes), bounce on y = 30 (playing the
// fall sound once) and give the object its new orientation and centre. Node speeds persist in
// Obj00Work::fallSpd (1/10 units).
void obj00FallMove(cObj00* obj)
{
    Obj00Work* w = OBJ00_WK(obj);
    Vec ofs[3] = { { 0.0f, 0.0f, 300.0f }, { 0.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 0.0f } };
    Obj00Node node[3];
    Vec vx;
    Vec vy;
    Vec vz;
    Vec d;
    u32 i;
    u32 k;
    Obj00Node* p;
    Obj00Node* n;
    f32 mag;
    f32 diff;

    if (!(w->be_flag & 4)) {
        return;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.x = (f32) w->fallSpd[i][0] * 0.1f;
        p->spd.y = (f32) w->fallSpd[i][1] * 0.1f;
        p->spd.z = (f32) w->fallSpd[i][2] * 0.1f;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        PSMTXMultVec(obj->mat, &ofs[i], &p->pos);
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
            if (p->pos.y < 30.0f) {
                p->pos.y = 30.0f;
                p->reflect = 1;
            }
            if (n->pos.y < 30.0f) {
                n->pos.y = 30.0f;
                n->reflect = 1;
            }
        }
    }
    {
    // COMPILER-DIFF: candidate #17 (global.c pass 0 regs_used_so_far): a codeless call-crossing
    // pseudo (3 refs, ranked between the hit-loop `end` and the hoisted `sePlayed = 1` constant)
    // occupies r24 across the hit loop so the constant takes r23 like the original; the four dead
    // `i` sets keep the gcse bucket count (spill-slot order of the PRE'd w+32/34/36, fp+136).
    int junk;
    asm("" : "=r"(junk) : "m"(node[0].reflect));
    i = 5; i = 6; i = 7; i = 8;
    for (i = 0; i < 3; i++) {
        p = &node[i];
        if (p->reflect) {
            p->spd.x *= 0.8f;
            p->spd.y *= -0.8f;
            p->spd.z *= 0.8f;
            if (w->fall_se_ck == 0) {
                w->fall_se_ck = 1;
                SndCall(w->fall_se_id, w->fall_se_no, &obj->pos, w->fall_em_id, 0, 0);
            }
        } else {
            PSVECSubtract(&p->pos, &p->old, &p->spd);
        }
        PSVECScale(&p->spd, &p->spd, 0.999f);
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        w->fallSpd[i][0] = (s16) (p->spd.x * 10.0f);
        w->fallSpd[i][1] = (s16) (p->spd.y * 10.0f);
        w->fallSpd[i][2] = (s16) (p->spd.z * 10.0f);
        asm("" : "=m"(pG) : "r"(junk));
    }
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &vz);
    PSVECSubtract(&node[2].pos, &node[1].pos, &vx);
    PSVECCrossProduct(&vz, &vx, &vy);
    PSVECCrossProduct(&vy, &vz, &vx);
#line 539 "D:/Bio4/Prog/obj00.cpp"
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
    PSVECAdd(&node[0].pos, &node[1].pos, &d);
    PSVECScale(&d, &d, 0.5f);
    TransMatrix(obj->mat, &d);
    obj->pos = d;
}

// Parent follow: takes the parent parts' matrix (axes normalised), and while be_flag bit 3 (catch-up)
// is set blends position/rotation from hokan_mat towards it by oya_hokan (advancing by rateSpd);
// copies the parent's light class 2.
void obj00SetOya(cObj00* pObj)
{
    Obj00Work* w = OBJ00_WK(pObj);
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
    PSMTXCopy(w->pEm_oya->getPartsPtr(w->oya_parts)->mat, m);
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
#line 597 "D:/Bio4/Prog/obj00.cpp"
    VECNormalize(&v0, &v0);
    if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
        v1.y = 1.0f;
    }
#line 599 "D:/Bio4/Prog/obj00.cpp"
    VECNormalize(&v1, &v1);
    if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
        v2.z = 1.0f;
    }
#line 601 "D:/Bio4/Prog/obj00.cpp"
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
    PSMTXConcat(m, pObj->mat, m);
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

// Gives the object a scenario collision sphere of radius r.
void cObj00::setScrAtari(f32 radius)
{
    sub2B4.atari.init(0.0f, 0.0f, 0.0f, radius, radius, radius * 0.8f, radius, 1, 0x2000, 10);
    sub2B4.atari.scrOn();
}
