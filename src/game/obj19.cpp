// game/obj19: object id 0x19, static item pick-up model cItemObj (D:/Bio4/Prog/obj19.cpp): the
// model of an item lying in the room, placed once by setItemObj with a light set and never moved
// (the scenario item area handles the pick-up).
#include "obj.h"
#include "obj19.h"

// A file-scope `static const` would be deferred to the end of the unit (after the cManager
// strings); a class static member is emitted here, in front of the two function-local ones.
const Vec cItemObj::zero = { 0.0f, 0.0f, 0.0f };

// New item model: collision off, small light volume.
cItemObj::cItemObj()
{
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    sub2B4.clrFlags(0xFCFF);
    LightInfo.init2(0, 1, &zero, &p1, 4);
}

// Only rebuilds the matrices.
void cItemObj::move()
{
    matUpdate();
}

// Creates a static item model at pos/rot (kept through suspends, light class 0x20).
cObj* setItemObj(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    static const Vec p1 = { 500.0f, 500.0f, 0.0f };
    cObj* obj;

    obj = ObjMgr.create(cObjMgr::ID_ITEM);
    if (obj == 0) {
        return 0;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        return 0;
    }
    obj->be_flag |= 0x4000;
    obj->pos = *pos;
    obj->ang = *rot;
    obj->setNoSuspend(1);
    obj->LightInfo.init2(1, 1, &cItemObj::zero, &p1, 0x20);
    return obj;
}
