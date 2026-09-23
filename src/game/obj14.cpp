// game/obj14: object id 0x14, the church bell (D:/Bio4/Prog/obj14.cpp): a two-link pendulum
// (PenCloth on parts 1/2) with a cEmHit body so shots swing it and ring it (pG->SeInfo.pos /
// SeInfo.type 2 for 90 frames: the village Ganados react); setBreak drops it (R1 1).
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "obj14.h"
#include "pendulum.h"
#include "emhit.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include <string.h>


extern "C" {
void obj14_R1_Set(cObjBell* obj);
void obj14_R1_Break(cObjBell* obj);
void obj14MatCalc(cObjBell* obj);
void obj14DmCk(cObjBell* obj);
void obj14ClothSet(cObjBell* obj);
void obj14ClothMove(cObjBell* obj);
}

void (*Obj14_R1_move_tbl[2])(cObjBell*) = { obj14_R1_Set, obj14_R1_Break };
u8 obj14ClothP[] = { 1, 2 };
u8 obj14ClothUp[] = { 0xFF, 1 };
u8 obj14ClothDp[] = { 2, 0xFF };
f32 obj14ClothMax[] = { 0.7853982f, 0.43633232f };

// Creates the bell at pos/rot with its pendulum set-up and a cEmHit hit body attached to parts 1.
cObj* SetObjBell(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    BellWork* w;
    Vec p0;
    Vec p1;

    obj = ObjMgr.create(cObjMgr::ID_BELL);
    if (obj == 0) {
        return 0;
    }
    w = BELL_WK((cObjBell*) obj);
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
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj14() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec l0 = { 0.0f, 0.0f, 0.0f };
    static const Vec l1 = { 1000.0f, 1000.0f, 0.0f };

    obj14ClothSet((cObjBell*) obj);
    obj->sub2B4.atari.throughOn();
    obj->LightInfo.init2(0, 1, &l0, &l1, 0x10);
    w->ringTimer = 0;
    p0.x = 0.0f;
    p0.y = 0.0f;
    p0.z = 0.0f;
    p1.x = 0.0f;
    p1.y = 0.0f;
    p1.z = 0.0f;
    w->pEmHit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore),
                         (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &p0, &p1, 1);
    if (w->pEmHit) {
        w->pEmHit->setParent(obj, 1, 0);
        YarareInit(w->pEmHit, 0.0f, -650.0f, 0.0f, 300.0f, 50.0f, 1, YAT_FLAG_ON);
    }
    obj->r_no_1 = 0;
    obj->r_no_0 = 1;
    obj->r_no_2 = 0;
    obj->r_no_3 = 0;
    return obj;
}

// Per-frame: damage check, R1 routine, pendulum step.
void cObjBell::move()
{
    obj14DmCk(this);
    Obj14_R1_move_tbl[r_no_1](this);
    obj14ClothMove(this);
}

// Rno1 == 0: hanging: matrices, and while ringTimer runs publishes the bell position (floor point
// 250 units in front) as the ringing bell (Status_flg[1] 0x20000000, SeInfo.type 2).
void obj14_R1_Set(cObjBell* pObj)
{
    BellWork* w = BELL_WK(pObj);

    obj14MatCalc(pObj);
    if (w->ringTimer) {
        Vec p;

        w->ringTimer--;
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = 250.0f;
        PSMTXMultVec(pObj->mat, &p, &p);
        p.y = SatMgr.getFloor(&p, 0, 600.0f, 100000.0f, 0);
        StaFlagOn(pG, STA_SE_BURST);
        pG->SeInfo.pos = p;
        pG->SeInfo.type = 2;
    }
}

// Rno1 == 1: broken: hides the bell, kills its hit body and spawns the break effect (est 1/7) once.
void obj14_R1_Break(cObjBell* pObj)
{
    BellWork* w = BELL_WK(pObj);

    if (pObj->r_no_2 == 0) {
        pObj->be_flag &= ~2;
        if (w->pEmHit) {
            w->pEmHit->hp = 0;
        }
        EstSet(0, -1, &pObj->pos, &pObj->ang, EFF_ROOM, 7, 0, ESP_CORE_KIND_NONE, 0, 0);
        pObj->r_no_2++;
    }
    obj14MatCalc(pObj);
}

// Rebuilds the bell matrix and parts (when no motion drives them).
void obj14MatCalc(cObjBell* pObj)
{
    RotMatrix(pObj->l_mat, &pObj->ang);
    TransMatrix(pObj->l_mat, &pObj->pos);
    ScaleMatrix(pObj->l_mat, &pObj->scale);
    PSMTXCopy(pObj->l_mat, pObj->mat);
    if (pObj->Motion.pMot == 0) {
        pObj->partsMatCalc();
    }
    pObj->partsWorldCalc();
}

// Weapon hits on the hit body: spark/blood effect by weapon kind, the bell sound, ringTimer 90,
// and a swing impulse (50/30/100 by weapon) from the hit direction on the two pendulum links.
void obj14DmCk(cObjBell* pObj)
{
    BellWork* w = BELL_WK(pObj);
    Vec dm;
    Vec dm2;
    Vec dir;
    u32 wep;
    f32 rate;
    cModel* parts;

    if (w->pEmHit == 0) {
        return;
    }
    wep = w->pEmHit->ckDmgWeapon();
    if (wep == 0) {
        return;
    }
    switch (wep) {
    default:
        if (pG->room_id != 4) {
            EmDmBloodSet2(w->pEmHit, 1, 5, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (pG->room_id != 4) {
            EmDmBloodSet2(w->pEmHit, 1, 6, 0, 0, 0);
        }
        break;
    }
    SndCall(6, 0xE, &pObj->pos, 0, 0, 0);
    w->ringTimer = 90;
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x26:
    case 0x2B:
        rate = 50.0f;
        break;
    case 0xB:
    case 0xC:
    case 0x19:
    case 0x1B:
    case 0x1D:
    case 0x1F:
    case 0x20:
    case 0x27:
        rate = 30.0f;
        break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x10:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x21:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2C:
    case 0x2D:
    default:
        rate = 100.0f;
        break;
    }
    if (EmGetDmPos(w->pEmHit, &dm, &dm2) == 0) {
        dm = w->pEmHit->dmg.m_PosFrom;
    }
    PSVECSubtract(&pObj->pos, &dm, &dir);
    dir.y = 0.0f;
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir.z = 1.0f;
    }
#line 359 "D:/Bio4/Prog/obj14.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, rate);
    parts = pObj->getPartsPtr(1);
    PSVECAdd(&((PenParts*) &parts->pFloor_norm)->speed, &dir, &((PenParts*) &parts->pFloor_norm)->speed);
    parts = pObj->getPartsPtr(2);
    PSVECScale(&dir, &dir, 0.8f);
    PSVECAdd(&((PenParts*) &parts->pFloor_norm)->speed, &dir, &((PenParts*) &parts->pFloor_norm)->speed);
}

// Switches to the broken routine.
void cObjBell::setBreak()
{
    r_no_0 = 1;
    r_no_2 = 0;
    r_no_1 = 1;
    r_no_3 = 0;
}

// 1 when the room flagged the bell as breakable (stat high half 0x0100).
int cObjBell::ckBreakEnable()
{
    return (r_no_0 == 1 && r_no_1 == 0);
}

// 1 when the bell is broken (stat high half 0x0101).
int cObjBell::ckBreak()
{
    return (r_no_0 == 1 && r_no_1 == 1);
}

// Pendulum set-up: parts 1 -> 2 chain with max swing 45 / 25 degrees, gravity 15.
void obj14ClothSet(cObjBell* pObj)
{
    BellWork* w = BELL_WK(pObj);

    w->cloth.Num = 2;
    w->cloth.pCloth = obj14ClothP;
    w->cloth.pParent = obj14ClothUp;
    w->cloth.pChild = obj14ClothDp;
    w->cloth.pMax = obj14ClothMax;
    w->cloth.Gravity = 15.0f;
    w->cloth.Rate = 1.0f;
    w->cloth.pLeft = 0;
    w->cloth.pRight = 0;
    w->cloth.pUpLeft = 0;
    w->cloth.pUpRight = 0;
    w->cloth.pWindSin = 0;
    w->cloth.pWindRate = 0;
    w->cloth.pGravity = 0;
    w->cloth.pRate = 0;
    w->cloth.pAtset = 0;
    w->cloth.At_num = 0;
    w->cloth.pEm_at = 0;
    w->cloth.Bundle_num = 0;
    w->cloth.WindSin = 0.0f;
    w->cloth.Stretchy = 0.0f;
    w->cloth.Move_rate = 0.0f;
    w->cloth.Flag = 0x100;
    w->cloth.pPtbl = 0;
    PenClothSet(pObj, &w->cloth, 100.0f);
}

// Pendulum step (PenClothMove3).
void obj14ClothMove(cObjBell* pObj)
{
    PenClothMove3(pObj, &BELL_WK(pObj)->cloth);
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
