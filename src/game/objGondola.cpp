// game/objGondola: object id 0x35, the cable car of the castle (D:/Bio4/Prog/objGondola.cpp). It
// travels along its motion (R0 1 Move without / 2 Down / 3 Up with the player), carrying the
// player, the partner and up to five enemies by the displacement of its floor point (4828 below
// parts 1), places a floor collision quad under them, and can break (R0 4) with a camera change and
// the player's death. setRidePL / setRideEm / setMoveMotion are the room script entry points.
#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "objGondola.h"
#include "em.h"
#include "global.h"
#include "math_sub.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "pad.h"
#include "quake.h"
#include "player.h"
#include "pl_npc.h"
#include "motion.h"
#include "game.h"

// motion.h declares MotionMove with one argument; the object units call it with two. Only the
// two blend fields of MotionWork are touched here.
struct GondolaMotWork {
    u8 pad_0[0x44];
    u32 flags2;           // 0x44
    u8 pad_48[0xC8 - 0x48];
    f32 blendRate;        // 0xC8
};

extern "C" {
void objGondola_R0_Set(cObjGondola* obj);
void objGondola_R0_Move(cObjGondola* obj);
void objGondola_R0_Down(cObjGondola* obj);
void objGondola_R0_Up(cObjGondola* obj);
void objGondola_R0_Break(cObjGondola* obj);
void objGondolaSatClear(cObjGondola* obj);
void objGondolaSatSet(cObjGondola* obj);
void objGondolaRideEmAdjust(cObjGondola* obj, Vec* pVec);
}

static void (*ObjGondola_R0_move_tbl[5])(cObjGondola*) = {
    objGondola_R0_Set, objGondola_R0_Move, objGondola_R0_Down, objGondola_R0_Up, objGondola_R0_Break,
};

// Creates the cable car (id 0x35) at pos/rot with its box collision and the floor quad (SatSet),
// no riders.
cObj* SetGondola(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    GondolaWork* w;
    int i;
    cEm** p;

    obj = ObjMgr.create(cObjMgr::ID_GONDOLA);
    if (obj == 0) {
        return 0;
    }
    w = GONDOLA_WK((cObjGondola*) obj);
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->atari.throughOn();
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
    for (i = 0; i < 5; i++) {
        w->pSat[i] = 0;
        w->pEat[i] = 0;
    }
    p = w->pEm;
    for (i = 0; i < 5; i++) {
        *p++ = 0;
    }
    w->Ride_pl = 0;
    w->Ride_sub = 0;
    w->pMot_info = 0;
    w->Sub_mot1 = 0;
    w->Sub_mot2 = 0;
    obj->r_no_0 = 0;
    obj->r_no_1 = 0;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    objGondolaSatSet((cObjGondola*) obj);
    return obj;
}

// Per-frame: releases the floor collision, action wait timer, R0 routine.
void cObjGondola::move()
{
    GondolaWork* w = GONDOLA_WK(this);

    objGondolaSatClear(this);
    if (w->Act_wait) {
        w->Act_wait--;
    }
    ObjGondola_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: parked: matrices only.
void objGondola_R0_Set(cObjGondola* pObj)
{
    pObj->matUpdate();
    pObj->partsWorldCalc();
}

// Rno0 == 1: moving without the player: advances the motion and carries the riding enemies by the
// floor point's displacement.
void objGondola_R0_Move(cObjGondola* pObj)
{
    Vec b;
    Vec a;
    Vec d;
    cModel* parts;

    GONDOLA_WK(pObj)->Ride_pl = 0;
    parts = pObj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
    parts = pObj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &d);
    objGondolaRideEmAdjust(pObj, &d);
}

// Rno0 == 2: descending with the player (and partner) aboard: motion, riders moved by the floor
// displacement (also fed to the camera quake offset), floor collision re-placed.
void objGondola_R0_Down(cObjGondola* pObj)
{
    GondolaWork* w = GONDOLA_WK(pObj);
    Vec b;
    Vec a;
    Vec d;
    cModel* parts;

    w->Ride_pl = 1;
    parts = pObj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
    parts = pObj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &d);
    PSVECAdd(&pPL->pos, &d, &b);
    pPL->setPos(&b);
    VEC_COPY(pG->quake_ofs, d);
    if (pSUB && w->Ride_sub) {
        PSVECAdd(&pSUB->pos, &d, &b);
        pSUB->setPos(&b);
    }
    objGondolaSatSet(pObj);
}

// Rno0 == 3: ascending with the player: as Down; at motion frame 4105 (arrival) the player and
// partner are put on the platform and the ride flag cleared.
void objGondola_R0_Up(cObjGondola* pObj)
{
    GondolaWork* w = GONDOLA_WK(pObj);
    Vec b;
    Vec a;
    Vec d;
    cModel* parts;

    w->Ride_pl = 1;
    parts = pObj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
    parts = pObj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &d);
    PSVECAdd(&pPL->pos, &d, &b);
    pPL->setPos(&b);
    VEC_COPY(pG->quake_ofs, d);
    if (pSUB && w->Ride_sub) {
        PSVECAdd(&pSUB->pos, &d, &b);
        pSUB->setPos(&b);
    }
    objGondolaSatSet(pObj);
    if (pObj->Motion.Seq_frame >= 4105.0f && pObj->Motion.Seq_frame <= 4105.0f) {
        b.x = 21722.0f;
        b.y = 10274.0f;
        b.z = -35327.0f;
        pPL->ang.y = -0.49f;
        pPL->setPos(&b);
        if (pSUB && w->Ride_sub) {
            b.x = 22577.0f;
            b.y = 10274.0f;
            b.z = -35864.0f;
            pSUB->ang.y = -0.49f;
            pSUB->setPos(&b);
        }
        StaFlagOff(pG, STA_RIDE_GONDOLA);
        w->Ride_pl = 0;
        pObj->r_no_0 = 1;
        pObj->r_no_1 = 0;
        pObj->r_no_2 = 0;
        pObj->r_no_3 = 0;
    }
}

// Rno0 == 4: the car breaks: kills the player if aboard (death demo after 60 frames), 42 frames
// later blends in the break motion, then keeps carrying the riders; drives an extra camera looking
// at the car from the side.
void objGondola_R0_Break(cObjGondola* pObj)
{
    GondolaWork* w = GONDOLA_WK(pObj);
    Vec b;
    Vec a;
    Vec v;
    cModel* parts;
    f32 len;
    Vec* cp;
    Vec* ca;
    static Camera ObjGondolaCam;

    switch (pObj->r_no_2) {
    case 0:
        w->Spd.x = 0.0f;
        w->Spd.y = 0.0f;
        w->Spd.z = 0.0f;
        if (w->Ride_pl) {
            pG->pl_life = 0;
            DiedemoExec(0x3C, 0);
        }
        w->Timer = 42;
        pObj->r_no_2++;
    case 1:
        parts = pObj->getPartsPtr(0);
        v.x = -2000.0f;
        v.y = 0.0f;
        v.z = -1115.0f;
        PSMTXMultVec(parts->mat, &v, &v);
        ObjGondolaCam.param.pos = v;
        parts = pObj->getPartsPtr(1);
        v.x = 0.0f;
        v.y = -2757.0f;
        v.z = 0.0f;
        PSMTXMultVec(parts->mat, &v, &v);
        ObjGondolaCam.param.at = v;
        cp = &ObjGondolaCam.param.pos;
        ca = &ObjGondolaCam.param.at;
        ObjGondolaCam.Up.x = 0.0f;
        ObjGondolaCam.Up.z = 0.0f;
        ObjGondolaCam.param.fovy = 50.0f;
        ObjGondolaCam.Up.y = 1.0f;
        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        ObjGondolaCam.Distance = SQRTF(len);
        CameraSetOrientationUp(&ObjGondolaCam);
        CamCtrl.m_pExtraCamera = (s32) &ObjGondolaCam;
        if (w->Timer) {
            w->Timer--;
            if (w->Timer == 0) {
                if (w->pMot_info && w->Sub_mot2) {
                    ((GondolaMotWork*) w->pMot_info)->flags2 |= 0x10000000;
                    MotionSetCore(pObj, w->pMot_info, w->Sub_mot2, 0, 0, 0, 0);
                    ((GondolaMotWork*) w->pMot_info)->flags2 &= ~0x10000000;
                    pObj->Motion.blend = w->pMot_info;
                    ((GondolaMotWork*) pObj->Motion.blend)->blendRate = 1.0f;
                    ((GondolaMotWork*) pObj->Motion.blend)->flags2 |= 0x80000000;
                }
            }
        }
        break;
    }
    parts = pObj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(pObj, 0);
    pObj->partsWorldCalc();
    parts = pObj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &v);
    if (w->Ride_pl) {
        PSVECAdd(&pPL->pos, &v, &b);
        pPL->setPos(&b);
        VEC_COPY(pG->quake_ofs, v);
        if (pSUB && w->Ride_sub) {
            PSVECAdd(&pSUB->pos, &v, &b);
            pSUB->setPos(&b);
        }
        objGondolaSatSet(pObj);
    }
}

// Disables the car's collision quads (flag 4 off) for this frame.
void objGondolaSatClear(cObjGondola* pObj)
{
    GondolaWork* w = GONDOLA_WK(pObj);
    int i;

    for (i = 0; i < 5; i++) {
        if (w->pSat[i]) {
            w->pSat[i]->setDisable();
        }
        if (w->pEat[i]) {
            w->pEat[i]->setDisable();
        }
    }
}

// Places the floor collision quad (2000 x 3000 at 4828 below parts 0, yaw from the car) at the car's
// position, creating it on first use; the four wall quads are compiled out (loop bound 1).
void objGondolaSatSet(cObjGondola* pObj)
{
    GondolaWork* w = GONDOLA_WK(pObj);
    Vec pos;
    Vec rot;
    Vec v;
    Vec poly[4];
    cModel* parts;
    u32 i;
    f32 hw;
    f32 hd;
    f32 cx;
    f32 cz;
    f32 h;
    f32 r;

    parts = pObj->getPartsPtr(0);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXMultVecSR(parts->mat, &v, &v);
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos = parts->world;
    pos.y -= 4828.03f;
    for (i = 0; i < 1; i++) {
        switch (i) {
        case 0:
        default:
            h = 0.0f;
            hw = 1000.0f;
            r = 3000.0f;
            cx = 0.0f;
            hd = 1500.0f;
            cz = 0.0f;
            break;
        case 1:
            hw = 1000.0f;
            r = 1100.0f;
            hd = 100.0f;
            cx = 0.0f;
            h = -100.0f;
            cz = 1500.0f;
            break;
        case 2:
            hw = 1000.0f;
            cz = -1500.0f;
            r = 1100.0f;
            hd = 100.0f;
            cx = 0.0f;
            h = -100.0f;
            break;
        case 3:
            hw = 100.0f;
            r = 1100.0f;
            hd = 1500.0f;
            cx = 1000.0f;
            h = -100.0f;
            cz = 0.0f;
            break;
        case 4:
            hw = 100.0f;
            cx = -1000.0f;
            r = 1100.0f;
            hd = 1500.0f;
            h = -100.0f;
            cz = 0.0f;
            break;
        }
        poly[0].x = -hw + cx;
        poly[0].y = h;
        poly[0].z = -hd + cz;
        poly[1].x = -hw + cx;
        poly[1].y = h;
        poly[1].z = hd + cz;
        poly[2].x = hw + cx;
        poly[2].y = h;
        poly[2].z = hd + cz;
        poly[3].x = hw + cx;
        poly[3].y = h;
        poly[3].z = -hd + cz;
        if (w->pSat[i]) {
            w->pSat[i]->setEnable();
            w->pSat[i]->setCoord(&pos, &rot);
        } else {
            w->pSat[i] = SatMgr.create(&pos, &rot, poly, r, 0, 0x100);
        }
    }
}

// (Unused) angular/box test whether em stands in the boarding area.
// Never called (dead-stripped by the original linker, STRIP_UNUSED): only their constant pools
// survive in .rodata (3000^2, pi, pi/3, 4250, 4343, 1850, 1950, 4828.03, 5000^2 / 4828.03, 5000^2).
static int objGondolaRideAreaCk(cObjGondola* obj, cEm* em)
{
    Vec d;
    f32 ang;

    PSVECSubtract(&em->pos, &obj->pos, &d);
    if (d.x * d.x + d.z * d.z > 9000000.0f) {
        return 0;
    }
    ang = atan2f(d.x, d.z) - obj->ang.y;
    if (ang > 3.1415927f) {
        return 0;
    }
    if (ang < 1.0471976f) {
        return 0;
    }
    if (d.x > 4250.0f) {
        return 0;
    }
    if (d.x < 4343.0f) {
        return 0;
    }
    if (d.z > 1850.0f) {
        return 0;
    }
    if (d.z < 1950.0f) {
        return 0;
    }
    if (d.y > 4828.03f) {
        return 0;
    }
    if (d.x * d.x + d.y * d.y + d.z * d.z > 25000000.0f) {
        return 0;
    }
    return 1;
}

// (Unused) 1 when em is within 5000 units of the car floor.
static int objGondolaRideDistCk(cObjGondola* obj, cEm* em)
{
    Vec d;

    PSVECSubtract(&em->pos, &obj->pos, &d);
    d.y -= 4828.03f;
    if (d.x * d.x + d.y * d.y + d.z * d.z > 25000000.0f) {
        return 0;
    }
    return 1;
}

// Starts the car's travel motion at `frame` (looping, sequence frame from the table).
void cObjGondola::setMoveMotion(void* mot, int frame)
{
    MotionSetCore(this, &Motion, mot, 0, 0, 0x8005, frame);
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// 1 while the player rides the car.
int cObjGondola::ckRide()
{
    if (GONDOLA_WK(this)->Ride_pl) {
        return 1;
    }
    return 0;
}

// Puts an enemy aboard in the first free of five slots (positions 300 apart along the floor).
void cObjGondola::setRideEm(cEm* em)
{
    GondolaWork* w = GONDOLA_WK(this);
    cModel* parts = getPartsPtr(0);
    Vec v;
    u32 i;

    for (i = 0; i < 5; i++) {
        if (w->pEm[i] == 0) {
            w->pEm[i] = em;
            v.x = 0.0f;
            v.y = -4828.03f;
            v.z = (f32) i * 300.0f + -1000.0f;
            if (i & 1) {
                v.x = -300.0f;
            }
            em->atari.throughOn();
            PSMTXMultVec(parts->mat, &v, &v);
            em->setPos(&v);
            break;
        }
    }
}

// Removes an enemy from the rider slots.
void cObjGondola::setGetOffEm(cEm* em)
{
    GondolaWork* w = GONDOLA_WK(this);
    int i;

    for (i = 0; i < 5; i++) {
        if (w->pEm[i] == em) {
            w->pEm[i] = 0;
        }
    }
}

// Hit feedback: a 5-frame quake and controller vibration.
void cObjGondola::setDamage()
{
    QuakeExec(0, 0, 5, 22.0f, 2);
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
}

// Break feedback: a 10-frame quake and vibration.
void cObjGondola::setBreak()
{
    QuakeExec(0, 0, 10, 30.0f, 2);
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    r_no_0 = 4;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Moves every riding enemy by the floor displacement; an enemy more than 4000 units from the floor
// centre has left the car.
void objGondolaRideEmAdjust(cObjGondola* pObj, Vec* pVec)
{
    GondolaWork* w = GONDOLA_WK(pObj);
    cModel* parts = pObj->getPartsPtr(1);
    Vec c;
    u32 i;

    c.x = 0.0f;
    c.y = -4828.03f;
    c.z = 0.0f;
    PSMTXMultVec(parts->mat, &c, &c);
    for (i = 0; i < 5; i++) {
        if (w->pEm[i]) {
            PSVECAdd(&w->pEm[i]->pos, pVec, &w->pEm[i]->pos);
            w->pEm[i]->setPos(&w->pEm[i]->pos);
            if ((c.x - w->pEm[i]->pos.x) * (c.x - w->pEm[i]->pos.x) +
                (c.y - w->pEm[i]->pos.y) * (c.y - w->pEm[i]->pos.y) +
                (c.z - w->pEm[i]->pos.z) * (c.z - w->pEm[i]->pos.z) > 16000000.0f) {
                w->pEm[i] = 0;
            }
        }
    }
}

// Puts the player (and the partner, 500/800 behind him) on the car floor facing 2.84 rad and sets
// the riding flag Status_flg[0] 0x20.
void cObjGondola::setRidePL()
{
    GondolaWork* w = GONDOLA_WK(this);
    Vec v;

    MotionMove(this, 0);
    partsWorldCalc();
    v = getPartsPtr(0)->world;
    v.y -= 4828.03f;
    pPL->ang.y = 2.84f;
    pPL->setPos(&v);
    w->Ride_sub = 0;
    if (pSUB) {
        Vec v2;

        v2.x = -500.0f;
        v2.y = 0.0f;
        v2.z = -800.0f;
        PSMTXMultVec(pPL->mat, &v2, &v2);
        pSUB->ang.y = 2.84f;
        pSUB->setPos(&v2);
        w->Ride_sub = 1;
    }
    StaFlagOn(pG, STA_RIDE_GONDOLA);
    r_no_0 = 2;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Clears the riding flag.
void cObjGondola::setGetOffPL()
{
    StaFlagOff(pG, STA_RIDE_GONDOLA);
    r_no_0 = 1;
    r_no_1 = 0;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Installs the secondary MotionWork with the shake and break motions blended over the travel motion.
void cObjGondola::setSubMotion(MotionWork* work, void* mot, void* breakMot)
{
    GondolaWork* w = GONDOLA_WK(this);

    w->pMot_info = work;
    w->Sub_mot1 = mot;
    w->Sub_mot2 = breakMot;
}

// Hit shake: blends the shake motion in (rate 1, additive), quake and vibration.
void cObjGondola::setVib()
{
    GondolaWork* w = GONDOLA_WK(this);

    if (w->pMot_info && w->Sub_mot1) {
        ((GondolaMotWork*) w->pMot_info)->flags2 |= 0x10000000;
        MotionSetCore(this, w->pMot_info, w->Sub_mot1, 0, 0, 0, 0);
        ((GondolaMotWork*) w->pMot_info)->flags2 &= ~0x10000000;
        Motion.blend = w->pMot_info;
        ((GondolaMotWork*) Motion.blend)->blendRate = 1.0f;
        ((GondolaMotWork*) Motion.blend)->flags2 |= 0x80000000;
        QuakeExec(0, 0, 10, 30.0f, 2);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    }
}
