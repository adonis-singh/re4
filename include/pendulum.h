#ifndef PENDULUM_H
#define PENDULUM_H

#include "types.h"
#include "vec.h"

class cModel;
class cParts;
struct CLOTH_AT_SET;

// Pendulum / cloth chain work (game/pendulum.cpp), 0x60 bytes (same object as pl_cloth.h's
// PlCloth). Field meanings from obj14ClothSet; the rest is zeroed there. Every link is one
// model parts; the u8 tables give the parts index per link and its neighbours (0xFF = none).
struct PenCloth {
    int Num;             // 0x00  number of chain links
    const u8* pCloth;    // 0x04  parts index per link
    const u8* pLeft;       // 0x08  left neighbour per link
    const u8* pRight;       // 0x0C  right neighbour per link
    const u8* pUpLeft;       // 0x10  third neighbour per link (Ada dress)
    const u8* pUpRight;       // 0x14  fourth neighbour per link
    const u8* pParent;       // 0x18  upper neighbour per link (0xFF = none)
    const u8* pChild;     // 0x1C  lower neighbour per link (0xFF = none)
    const f32* pGravity;      // 0x20  gravity per link (NULL: x3C)
    const f32* pRate;      // 0x24  damping rate per link (NULL: x40)
    const f32* pMax;     // 0x28  max swing angle per link
    const f32* pWindSin;      // 0x2C  wind phase per link
    const f32* pWindRate;      // 0x30  wind rate per link
    CLOTH_AT_SET* pAtset;      // 0x34  collision volumes
    int At_num;             // 0x38  number of collision volumes
    f32 Gravity;             // 0x3C  gravity (15.0 for the bell)
    f32 Rate;             // 0x40  damping rate (1.0)
    u32 Bundle_num;             // 0x44  constraint iterations
    f32 WindSin;             // 0x48  wind phase
    f32 Stretchy;             // 0x4C  constraint stiffness (Move2 / Move3)
    f32 Move_rate;             // 0x50  parent speed rate
    cParts** pPtbl;        // 0x54  parts pointer table (NULL: cModel::getPartsPtr)
    cModel* pEm_at;         // 0x58  model the collision volumes hang on (NULL: the chain model)
    u32 Flag;           // 0x5C  (0x100)
};

// Per-link work the pendulum keeps in the parts' cModel from 0x128 on.
struct PenParts {
    Vec dir;             // 0x128  link vector in parts space (0, -len, 0)
    Vec nrm;             // 0x134  unit link direction in parts space
    f32 distL;           // 0x140  half distance to the left neighbour
    f32 distR;           // 0x144  right
    f32 distUL;          // 0x148  third
    f32 distUR;          // 0x14C  fourth
    Vec speed;           // 0x150
    Vec pos;             // 0x15C  world position of the link end
    Vec oldPos;          // 0x168
    f32 len;             // 0x174  link length
    u8 pad_178[0x18C - 0x178];
    Vec fixPos;          // 0x18C  PenClothFixSet position
    u8 hit;              // 0x198  bit0: collided this frame
    u8 flags;            // 0x199  bit0: fixed at fixPos
    u8 pad_19A[0x1C0 - 0x19A];
    u32 x1C0;            // 0x1C0  |= 0x06000000 by PenClothSet
};

// One collision volume in world space (penClothAtMake), 0x84 bytes.
struct PenAt {
    int type;            // 0x00  0 sphere, 1 cylinder
    Mtx mat;             // 0x04  cylinder space -> world
    Mtx inv;             // 0x34  world -> cylinder space
    Vec p0;              // 0x64  sphere centre / cylinder start
    Vec p1;              // 0x70  cylinder end
    f32 r;               // 0x7C  radius
    f32 len;             // 0x80  cylinder length
};

// Collision volume list built per frame in the locked cache (0xE0000000).
struct PenAtWork {
    int num;             // 0x00
    PenAt* pAt;          // 0x04
    PenAt at[1];         // 0x08
};

// Wind of the pendulum system (PenWindSet; light.cpp cPenWind::set)
extern Vec GlobalWind;
extern f32 GlobalWindAdd;

extern "C" {
void PenClothSet(cModel* m, PenCloth* pInfo, f32 min_len);
void PenClothFixSet(cModel* m, PenCloth* pInfo, int no, Vec* pos);
void PenClothFixClear(cModel* m, PenCloth* pInfo, int no);
void PenClothMove(cModel* m, PenCloth* pInfo);
void PenClothMove2(cModel* m, PenCloth* pInfo);
void PenClothMove3(cModel* m, PenCloth* c);
PenAtWork* penClothAtMake(cModel* m, CLOTH_AT_SET* at, int n);
int penClothAtCk(Vec* pos, Vec* up, PenAtWork* wk);
int penClothAtCkBorder(Vec* pos, Vec* up, PenAtWork* wk);
void penClothAtCkParallel(Vec* pos, Vec* up, PenAtWork* wk);
// global wind: direction (radians), strength, x (cPenWind::set in light.cpp)
void PenWindSet(f32 dir, f32 power, f32 x);
}

#endif
