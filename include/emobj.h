#ifndef EMOBJ_H
#define EMOBJ_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "atari.h"

// Work of the generic object enemy (game/emobj.cpp), overlaid on cEm from 0x3E0 (the em
// classes keep their per-type work there and address it through a pointer to 0x3E0).
struct EmObjWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: motion plays, bit1: sat registered, bit2: eat registered
    u8 pad_4[0x214 - 0x4];
    cSat* pSat;           // 0x214 (0x5F4)  scenario collision piece
    cSat* pEat;           // 0x218 (0x5F8)  effect collision piece
    Vec satPos;           // 0x21C (0x5FC)  quad centre (model space)
    Vec eatPos;           // 0x228 (0x608)
    Vec satSize;          // 0x234 (0x614)  x/z half size, y height
    Vec eatSize;          // 0x240 (0x620)
    int satN;             // 0x24C (0x62C)  cSatMgr::create n
    int eatN;             // 0x250 (0x630)
    int satFlag;          // 0x254 (0x634)  cSatMgr::create flag
    int eatFlag;          // 0x258 (0x638)
    u8 eff;               // 0x25C (0x63C)
    u8 etc;               // 0x25D (0x63D)
};

#define EMOBJ_WK(em) ((EmObjWork*) (((cEmObj*) (em))->free))

// Generic object enemy: a cEm with an optional scenario / effect collision quad and the
// yarare (hit box) setup. No key function: the vtable and the implicit destructor are
// emitted by em.cpp (cEmMgr::construct), so no in-class inline members here.
class cEmObj : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMOBJ_WK)
    void EmObjInit();
    void EmObjMove();
    void setSat(Vec* pos, int n, int flag, int cube, f32 sx, f32 sy, f32 sz);   // `cube` is unused (emwindow passes its setYarare cube flag; the mangled name needs the 4th int)
    void setSatMain();
    void clrSat();
    void setEat(Vec* pos, int n, int flag, int cube, f32 sx, f32 sy, f32 sz);
    void setEatMain();
    void clrEat();
    void setYarare(s16 no, Vec* pos, u16 flag, int cube, f32 w, f32 h, f32 rad);
    void setEff(u8 v);
    u8 getEff();
    void setEtc(u8 v);
    u8 getEtc();
};

#endif
