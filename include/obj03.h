#ifndef OBJ03_H
#define OBJ03_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Path object: every parts is placed along a path, spaced 40 units apart.
class cObj03 : public cObj {
public:
    u8 r0;                // 0x328
    u8 r1;                // 0x329
    u8 r2;                // 0x32A
    u8 r3;                // 0x32B
    f32 pathLen;          // 0x32C  path length
    f32 pathPos;          // 0x330  current position on the path
    f32 speed;            // 0x334
    u32 flag;             // 0x338  bit0: debug draw
    cModel* pPathParent;  // 0x33C  the path follower (PathGetMatEm pMod)
    void* pPath;          // 0x340

    cObj03();
    virtual void move();
    int init();
};

#endif
