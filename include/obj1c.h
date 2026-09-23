#ifndef OBJ1C_H
#define OBJ1C_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room-script view of the floating island object (game/obj1c.cpp defines the full class with its
// virtuals; the rooms only call the out-of-line setMotion on a SetFloatIsland() result).
class cObj1c : public cObjUnion {
public:
    void setMotion(void* idle, void* crash, void* idleBig, void* crashBig);
};

cObj* SetFloatIsland(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
