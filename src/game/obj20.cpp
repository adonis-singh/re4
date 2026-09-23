// game/obj20: object id 0x20, the obstacle model Oba (D:/Bio4/Prog/obj20.cpp): an invisible dummy
// model carrying a collision sphere/cylinder (radius rad, height h) that follows a parts (type 0)
// or the origin (type 1) of a parent object, so enemies collide with moving scenery.
#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "global.h"
#include "math_sub.h"
#include "at_mod.h"


// Obstacle model (Oba): an invisible collision model attached to a parent object (type 0: to
// one of its parts, type 1: to the object itself) or standing alone.
class cObjObaModel : public cObjUnion {
public:
    virtual void move();
};

// Creates the obstacle on `parent` (parts partsNo + ofs for type 0, parent origin + ofs for type 1),
// collision radius rad / height h, priority level 1, not drawn.
extern "C" cObj* SetObaModel(cObj* parent, int partsNo, Vec* ofs, f32 rad, f32 h, u8 type)
{
    cObj* obj;
    ObaModelWork* w;

    obj = ObjMgr.create(cObjMgr::ID_OBAMODEL);
    if (obj == 0) {
        return 0;
    }
    w = &((cObjObaModel*) obj)->obaModel;
    if (obj->modelInit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore),
                       (void*) (pG->pCore->ofs_24 + (u32) pG->pCore)) == 0) {
        pLog->err(0, 0, "SetObaModel() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->type = type;
    obj->LightInfo.init2(0, 1, &p0, &p1, 0x10);
    obj->sub2B4.atari.init(0.0f, 0.0f, 0.0f, rad, rad, rad, h, 0, 0x2000, 10);
    obj->sub2B4.atari.clrFlag100();
    obj->sub2B4.atari.setPriority(PRI_LV1);
    obj->be_flag &= ~2;
    w->parent = parent;
    w->ofs = *ofs;
    w->partsNo = partsNo;
    obj->move();
    return obj;
}

// Per-frame: dies with the parent; position from the parent parts (clamped to the parent height
// for type 0) or origin; copies the parent's collision-through flag 0x200; enemy attack check and
// collision update.
void cObjObaModel::move()
{
    ObaModelWork* w = &obaModel;

    if (w->parent) {
        if ((w->parent->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
        if (type == 0) {
            cModel* parts = w->parent->getPartsPtr(w->partsNo);
            PSMTXMultVec(parts->mat, &w->ofs, &pos);
            if (pos.y < w->parent->pos.y + 2000.0f) {
                pos.y = w->parent->pos.y;
            }
        }
        if (type == 1) {
            PSMTXMultVec(w->parent->mat, &w->ofs, &pos);
        }
        if (w->parent->sub2B4.atari.m_flag & 0x200) {
            sub2B4.atari.m_flag |= 0x200;
        } else {
            sub2B4.atari.m_flag &= ~0x200;
        }
    }
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    matUpdate();
    EmAtCheck(this);
    sub2B4.atari.move();
}
