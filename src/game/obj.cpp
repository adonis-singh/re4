// game/obj: the object manager (D:/Bio4/Prog/obj.cpp). ObjMgr (cObjMgr) owns the pool of 0x3D8-byte
// cObj works; construct() placement-news the per-id class (obj00..objBull, ids 0..0x3F) so the
// virtual move() dispatches to the unit that implements it, move() runs every alive object once a
// frame (objMove), destroy() frees an object's model resources first.
#include "atari.h"
#include "event.h"
#include "obj.h"
#include "obj00.h"
#include "obj01.h"
#include "obj03.h"
#include "obj04.h"
#include "obj05.h"
#include "obj08.h"
#include "obj09.h"
#include "obj10.h"
#include "obj12.h"
#include "obj13.h"
#include "obj14.h"
#include "obj15.h"
#include "obj16.h"
#include "obj18.h"
#include "obj1b.h"
#include "obj1c.h"
#include "obj1d.h"
#include "obj20.h"
#include "obj26.h"
#include "objBull.h"
#include "objGondola.h"
#include "objMissile.h"
#include "objPillar.h"
#include "objRobo.h"
#include "objTrolley.h"
#include "objYagura.h"
#include "global.h"
#include "db_log.h"
#include "va_ppc.h"
#include "main_mem.h"
#include "pl_wep.h"
#include "at_mod.h"
#include <dolphin/os.h>
#include "shape.h"

// Map object manager (ObjMgr): 0x3D8-byte cObj works, constructed by id (construct), moved once per
// frame (move / objMove). The per-id classes live in the obj* units; only their constructors are
// needed here.

extern "C" {
void objMove(cObj* p);
}

class cObjScr : public cObjUnion {
public:
    cObjScr();
    virtual void move();
};
class cObjBox : public cObjUnion {
public:
    cObjBox();
    virtual void move();
};
class cItemObj : public cObjUnion {
public:
    cItemObj();
    virtual void move();
};
class cObjGrenade : public cObjUnion {
public:
    cObjGrenade();
    virtual void move();
};
class cObjGreFire : public cObjUnion {
public:
    cObjGreFire();
    virtual void move();
};
class cObjGreLight : public cObjUnion {
public:
    cObjGreLight();
    virtual void move();
};
class cObjEgg : public cObjUnion {
public:
    cObjEgg();
    virtual void move();
};
void (*ObjInitFunc[0x40])(cObj*);

// Manager of the 0x3D8-byte cObj works (kind 2 of the unit managers).
cObjMgr::cObjMgr() : cManager<cObj>(OBJ_WORK_SIZE, 2)
{
    setName("cObjMgr");
    Guid = 0;
}

// Manager warnings to the log.
void cObjMgr::log(const char* pStr, ...)
{
    va_list ap;

    va_start(ap, pStr);
    pLog->vwarn(6, 0, pStr, ap);
}

// Unit construction: placement-news the per-id class (0 cObj00 ... 0x3F) into the work.
#line 130 "D:/Bio4/Prog/obj.cpp"
int cObjMgr::construct(cObj* pObj, ID id)
{
    switch (id) {
    case ID_NORMAL:
        pObj = new (pObj) cObj00();
        break;
    case ID_MAGAZINE:
        pObj = new (pObj) cObj01();
        break;
    case ID_SCROLL:
        pObj = new (pObj) cObjScr();
        break;
    case ID_03:
        pObj = new (pObj) cObj03();
        break;
    case ID_ESP:
        pObj = new (pObj) cObj04();
        break;
    case ID_KABOOM:
        pObj = new (pObj) cObj05();
        break;
    case ID_BOX:
        pObj = new (pObj) cObjBox();
        break;
    case ID_MISSILE:
        pObj = new (pObj) cObj08();
        break;
    case ID_ESP2:
        pObj = new (pObj) cObj09();
        break;
    case ID_WEP_ITEM:
        pObj = new (pObj) cWepItem();
        break;
    case ID_PL_WEAPON:
        pObj = new (pObj) cObjWep();
        break;
    case ID_EM12_WEAPON:
        pObj = new (pObj) cObj12();
        break;
    case ID_LADDER:
        pObj = new (pObj) cObjLadder();
        break;
    case ID_BELL:
        pObj = new (pObj) cObjBell();
        break;
    case ID_GATLING:
        pObj = new (pObj) cObjGatling();
        break;
    case ID_EM10_PARASITE:
        pObj = new (pObj) cObj16();
        break;
    case ID_EVENT:
        pObj = new (pObj) cObj18();
        break;
    case ID_ITEM:
        pObj = new (pObj) cItemObj();
        break;
    case ID_WEP_GRENADE:
        pObj = new (pObj) cObjGrenade();
        break;
    case ID_SPEAR:
        pObj = new (pObj) cObjSpear();
        break;
    case ID_FLOATISLAND:
        pObj = new (pObj) cObj1c();
        break;
    case ID_CHAIN:
        pObj = new (pObj) cObjChain();
        break;
    case ID_OBAMODEL:
        pObj = new (pObj) cObjObaModel();
        break;
    case ID_WEP_ROCKET:
        pObj = new (pObj) cObjRocket();
        break;
    case ID_WEP_LAUNCHER:
        pObj = new (pObj) cObjLauncher();
        break;
    case ID_EM2B_PARASITE:
        pObj = new (pObj) cObj26();
        break;
    case ID_WEP_GRE_FIRE:
        pObj = new (pObj) cObjGreFire();
        break;
    case ID_WEP_GRE_LIGHT:
        pObj = new (pObj) cObjGreLight();
        break;
    case ID_GONDOLA:
        pObj = new (pObj) cObjGondola();
        break;
    case ID_ROBO:
        pObj = new (pObj) cObjRobo();
        break;
    case ID_HELI_MISSILE:
        pObj = new (pObj) cObjMissile();
        break;
    case ID_YAGURA:
        pObj = new (pObj) cObjYagura();
        break;
    case ID_WEP_EGG:
        pObj = new (pObj) cObjEgg();
        break;
    case ID_TROLLEY:
        pObj = new (pObj) cObjTrolley();
        break;
    case ID_BULL:
        pObj = new (pObj) cObjBull();
        break;
    case ID_PILLAR:
        pObj = new (pObj) cObjPillar();
        break;
    default:
        if (id > 0x3F) {
#line 175 "D:/Bio4/Prog/obj.cpp"
            HALT();
        }
        ObjInitFunc[id](pObj);
        break;
    }
    pObj->guid = Guid;
    Guid++;
    pObj->id = id;
    return 1;
}

// cManager entry point: forwards to the int version.
int cObjMgr::construct(cObj* pObj, u32 id)
{
    return construct(pObj, (ID) id);
}

// Per-frame: die check, then objMove on every alive object.
void cObjMgr::move()
{
    cObj* p;
    cObj* n;
    void (*func)(cObj*);

    dieCheck();
    func = objMove;
    p = pAlive;
    while (p) {
        n = p;
        p = (cObj*) p->pNext;
        func(n);
    }
}

// One object's frame: skips inactive objects (be_flag 0x20 clear) and, during an event
// (Status_flg[1] 0x10000000), objects without the no-suspend flag; runs move(), the shape
// animation, the position history; debug: obstacle / skeleton / bounding box displays.
void objMove(cObj* pObj)
{
    if (!(pObj->be_flag & 0x20)) {
        return;
    }
    if (StaFlagChk(pG, STA_SUSPEND) && !(pObj->be_flag & 0x800)) {
        return;
    }
    pObj->move();
    ShapeMove(pObj->pModelInfo);
    pObj->updateOldPos();
    if (DbgFlagChk(pG, DBG_OBA_VIEW)) {
        DrawOba(pObj);
    }
    if (DbgFlagChk(pG, DBG_OBJ_SKELETON)) {
        pObj->debugSkeletonDisp();
    }
    if (pObj->be_flag & 0x80000000) {
        pObj->drawAllBoundingBox(pObj->pModelInfo);
    }
}

// Destroys an object: releases its model/parts (push) when it was alive, then the manager slot.
void cObjMgr::destroy(cObj* pObj)
{
    if ((pObj->be_flag & 0x201) != 1) {
        return;
    }
    pObj->push();
    cManager<cObj>::destroy(pObj);
}

// New object: active + alive flags, kindid 1.
cObj::cObj()
{
    be_flag |= 0x21;
    kindid = 1;
}

cObjMgr ObjMgr;
