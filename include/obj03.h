#ifndef OBJ03_H
#define OBJ03_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Per-object work layouts (game/obj03.cpp ...), all overlaid at cObj+0x328.
struct Obj03Work {
    u8 x0;        // 0x00
    u8 x1;        // 0x01
    u8 x2;        // 0x02
    u8 x3;        // 0x03
    f32 length;   // 0x04 path length
    f32 t;        // 0x08 current position on the path
    f32 speed;    // 0x0C
    u32 flags;    // 0x10 bit0: debug draw
    cModel* data; // 0x14  the path follower (PathGetMatEm pMod)
    void* path;   // 0x18
};

// Path object: every parts is placed along a path, spaced 40 units apart.
class cObj03 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj03Work

    cObj03();
    virtual void move();
    int init();
};

#define OBJ03_WK(o) ((Obj03Work*) (o)->free)

#endif
