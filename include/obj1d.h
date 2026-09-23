#ifndef OBJ1D_H
#define OBJ1D_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Chain link work (game/obj1d.cpp): hangs between two parts of a parent model, fades out when
// the parent is lost.
struct ChainWork {
    u32 Be_flg;            // 0x00  bit1: keep the parent parts matrices as they are (no axis normalize)
    int Timer;            // 0x04  frames before the fade-out (LostWait)
    u8 pad_8[4];
    cModel* pEm_oya;       // 0x0C
    int Parts1;           // 0x10
    int Parts2;           // 0x14
    Vec Offset1;             // 0x18  offset in parts1
    Vec Offset2;             // 0x24  offset in parts2
    struct PenCloth* pCloth;  // 0x30
};

// Chain link: a model hung between two parts of a parent (the interpolated orientation and
// position of the two parts), with an optional pendulum cloth. Fades out when the parent is lost.
class cObjChain : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  ChainWork

    virtual void move();
    virtual ~cObjChain() {}
    void setParent(cModel* parent, int parts, Vec* ofs, int flag);
    void setParent2(cModel* parent, int parts1, Vec* ofs1, int parts2, Vec* ofs2, int flag);
    void setChain(PenCloth* cloth);
    void chainMove();
};

#define CHAIN_WK(o) ((ChainWork*) (o)->free)

cObjChain* SetChain(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
