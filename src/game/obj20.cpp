// game/obj20: object id 0x20, the obstacle model Oba (D:/Bio4/Prog/obj20.cpp): an invisible dummy
// model carrying a collision sphere/cylinder (radius rad, height h) that follows a parts (type 0)
// or the origin (type 1) of a parent object, so enemies collide with moving scenery.
#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "obj20.h"
#include "global.h"
#include "math_sub.h"
#include "at_mod.h"


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
    w = OBAMODEL_WK((cObjObaModel*) obj);
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
    obj->atari.init(0.0f, 0.0f, 0.0f, rad, rad, rad, h, 0, 0x2000, 10);
    obj->atari.clrFlag100();
    obj->atari.setPriority(PRI_LV1);
    obj->be_flag &= ~2;
    w->pEm = parent;
    w->Offset = *ofs;
    w->Parts_no = partsNo;
    obj->move();
    return obj;
}

// Per-frame: dies with the parent; position from the parent parts (clamped to the parent height
// for type 0) or origin; copies the parent's collision-through flag 0x200; enemy attack check and
// collision update.
void cObjObaModel::move()
{
    ObaModelWork* w = OBAMODEL_WK(this);

    if (w->pEm) {
        if ((w->pEm->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
        if (type == 0) {
            cParts* parts = w->pEm->getPartsPtr(w->Parts_no);
            PSMTXMultVec(parts->mat, &w->Offset, &pos);
            if (pos.y < w->pEm->pos.y + 2000.0f) {
                pos.y = w->pEm->pos.y;
            }
        }
        if (type == 1) {
            PSMTXMultVec(w->pEm->mat, &w->Offset, &pos);
        }
        if (w->pEm->atari.m_flag & 0x200) {
            atari.m_flag |= 0x200;
        } else {
            atari.m_flag &= ~0x200;
        }
    }
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    matUpdate();
    EmAtCheck(this);
    atari.move();
}
