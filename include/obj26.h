#ifndef OBJ26_H
#define OBJ26_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Fading attachment work (game/obj26.cpp): scales toward `tgtScale`, then shrinks and fades out.
struct Obj26Work {
    u8 pad_0[8];
    cObj* pEm;         // 0x08  followed parts 2 of this object
    u8 pad_C[0xC];
    Vec Scale;         // 0x18
};

// Attachment that follows parts 2 of its parent, scales toward a target size (routine 0) and
// then shrinks/fades away (routine 1). Routine index in xFD, step in xFE.
class cObj26 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj26Work

    virtual void move();
};

#define OBJ26_WK(o) ((Obj26Work*) (o)->free)

#endif
