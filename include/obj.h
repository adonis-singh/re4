#ifndef OBJ_H
#define OBJ_H

#include "types.h"
#include "cManager.h"
#include "model.h"
#include "atariInfo.h"
#include "pendulum.h"
#include "main_mem.h"

class cModel;
struct EmAtkInfo;

class cObj;

// Effect owner info at the head of every Efm work (esp_efm.cpp copies the caller's EspInfo,
// esp.h, into it; EfmDeleteSub matches flg / kind / pEm against g_Core_*).
struct EfmCore {
    u16 flg;              // 0x00
    u8 kind;              // 0x02
    u8 x3;                // 0x03
    u32 x4;               // 0x04
    cModel* pEm;          // 0x08
};

// Event object model type (PS2 OBJ18_TYPE): SetObj18 `type` / Obj18Work::type, from the model name prefix
// (event.cpp ExePacket_SetOm OmTbl).
enum OBJ18_TYPE {
    OBJ18_TYPE_OBMXX = 0,
    OBJ18_TYPE_LEON = 1,
    OBJ18_TYPE_ASHLEY = 2,
    OBJ18_TYPE_ADA = 3,
    OBJ18_TYPE_LUIS = 4,
    OBJ18_TYPE_PLXX = 5,
    OBJ18_TYPE_GANADO = 6,
    OBJ18_TYPE_TRADER = 7,
    OBJ18_TYPE_MAYOR1 = 8,
    OBJ18_TYPE_NO2 = 9,
    OBJ18_TYPE_SADDLER = 10,
    OBJ18_TYPE_ELGIGANTE = 11,
    OBJ18_TYPE_EMXX = 12,
    OBJ18_TYPE_EVXX = 13,
    OBJ18_TYPE_ETXX = 14,
    OBJ18_TYPE_SCRXX = 15,
    OBJ18_TYPE_WEPXX = 16,
    OBJ18_TYPE_EFFECT = 17,
    OBJ18_TYPE_MAYOR2 = 18,
    OBJ18_TYPE_INSECTBOSS0 = 19,
    OBJ18_TYPE_INSECTBOSS1 = 20,
    OBJ18_TYPE_INSECTBOSS0S = 21,
    OBJ18_TYPE_INSECTBOSS1S = 22,
    OBJ18_TYPE_ADA_SKIRT = 23,
    OBJ18_TYPE_NO3 = 24
};

// Map object (game/obj.cpp): the cModel (0x320; motion work `Motion`, `atari`, `pFsdTbl`
// are cModel members, see model.h) and the scroll block. Each subclass declares its own fields from
// 0x328, up to OBJ_WORK_SIZE.
class cObj : public cModel {
public:
    u8 pad_320[4];        // 0x320
    s32 blk;              // 0x324  scroll block the object belongs to (-2 free, -1 SetObjSmd)

    cObj();
    virtual ~cObj() {}
};

// One pool block per object: the manager's stride is the largest cObj subclass.
#define OBJ_WORK_SIZE 0x3D8

class cObjMgr : public cManager<cObj> {
public:
    // Construct ids (PS2 cObjMgr::ID): the row of construct() / the class of the work; the GC switch ends at
    // ID_WEP_THERMO (0x3F), ID_GON..ID_PL_BATTERY are PS2 additions.
    enum ID {
        ID_NORMAL = 0,
        ID_MAGAZINE = 1,
        ID_SCROLL = 2,
        ID_03 = 3,
        ID_ESP = 4,
        ID_KABOOM = 5,
        ID_BOX = 6,
        ID_07 = 7,
        ID_MISSILE = 8,
        ID_ESP2 = 9,
        ID_WEP_ITEM = 10,
        ID_PL_WEAPON = 11,
        ID_0C = 12,
        ID_0D = 13,
        ID_0E = 14,
        ID_0F = 15,
        ID_WEP_ALLOW = 16,
        ID_WEP_BOW = 17,
        ID_EM12_WEAPON = 18,
        ID_LADDER = 19,
        ID_BELL = 20,
        ID_GATLING = 21,
        ID_EM10_PARASITE = 22,
        ID_22C_HITMARK = 23,
        ID_EVENT = 24,
        ID_ITEM = 25,
        ID_WEP_GRENADE = 26,
        ID_SPEAR = 27,
        ID_FLOATISLAND = 28,
        ID_CHAIN = 29,
        ID_LUIS_ITEM = 30,
        ID_PILLAR = 31,
        ID_OBAMODEL = 32,
        ID_WEP_RUGER = 33,
        ID_WEP_ROCKET = 34,
        ID_WEP_LAUNCHER = 35,
        ID_WEP_KNIFE = 36,
        ID_WEP_TOMPSON = 37,
        ID_EM2B_PARASITE = 38,
        ID_WEP_MAUSER = 39,
        ID_WEP_SNIPER = 40,
        ID_WEP_GRE_FIRE = 41,
        ID_WEP_GRE_LIGHT = 42,
        ID_WEP_SHOTGUN = 43,
        ID_WEP_MAGNUM = 44,
        ID_WEP_MACHINE = 45,
        ID_WEP_CIVILIAN = 46,
        ID_WEP_STRIKER = 47,
        ID_WEP_HKSNIPER = 48,
        ID_WEP_GOVERNMENT = 49,
        ID_WEP_FN57 = 50,
        ID_WEP_XD9 = 51,
        ID_WEP_VP70 = 52,
        ID_GONDOLA = 53,
        ID_WEP_MINE = 54,
        ID_ROBO = 55,
        ID_HELI_MISSILE = 56,
        ID_YAGURA = 57,
        ID_WEP_EGG = 58,
        ID_TROLLEY = 59,
        ID_WEP_HANDGRE = 60,
        ID_WEP_HAND = 61,
        ID_BULL = 62,
        ID_WEP_THERMO = 63,
        ID_GON = 64,
        ID_EM3F_TENTACLE = 65,
        ID_WEP_LASER = 66,
        ID_PL_BATTERY = 67,
        ID_NUM = 68
    };

    u32 Guid;

    cObjMgr();
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cObj* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cObj* pEm);
    virtual int construct(cObj* pSat, u32 room_no);   // calls the int overload (obj.cpp)
    int construct(cObj* pSat, ID id);                 // placement-new of the per-id class, or ObjInitFunc[id]
    void move();                              // dieCheck, then objMove on every live object
};

extern cObjMgr ObjMgr;

// Work `no` of ObjMgr, 0 when out of range. A free inline: an in-class one would be emitted out of
// line in obj.cpp (the unit owns cObjMgr's vtable), which the DOL does not have.
static inline cObj* ObjMgrWork(u32 no)
{
    if (no >= ObjMgr.nArray) {
        return 0;
    }
    return (cObj*)((u8*)ObjMgr.pArray + ObjMgr.size * no);
}

struct EspGenWork;
extern "C" {
// game/esp_efm.cpp: creates the obj04 / obj05 / obj09 effect model of a sequence record
// (`info` is the caller's EspInfo, esp.h). esp_sub.cpp EspSeqSet is the only caller.
cObj* EfmSeqSet(EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs);
// game/obj04.cpp / game/obj05.cpp: orient the model along `m`
void Efm04RotMatrix(cObj* pObj, Mtx pMat);
void Efm05RotMatrix(cObj* pObj, Mtx pMat);
}

#endif
