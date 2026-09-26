#ifndef OBJ20_H
#define OBJ20_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// obj20 (game/obj20.cpp): an invisible collision obstacle attached to a parent object. C linkage.

extern "C" {
// Creates the obstacle on `parent` (parts partsNo + ofs for type 0, parent origin + ofs for type 1),
// collision radius rad / height h, priority level 1, not drawn.
cObj* SetObaModel(cObj* parent, int partsNo, Vec* ofs, f32 rad, f32 h, u8 type);
}

// Obstacle model work (game/obj20.cpp `SetObaModel`).
struct ObaModelWork {
    u8 pad_0[0xC];
    Vec Offset;              // 0x0C  position relative to the parent (parts) matrix
    int Parts_no;          // 0x18  parts of the parent followed by type 0
    cObj* pEm;         // 0x1C
};

// Obstacle model (Oba): an invisible collision model attached to a parent object (type 0: to
// one of its parts, type 1: to the object itself) or standing alone.
class cObjObaModel : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  ObaModelWork

    virtual void move();
};

#define OBAMODEL_WK(o) ((ObaModelWork*) (o)->free)

#endif
