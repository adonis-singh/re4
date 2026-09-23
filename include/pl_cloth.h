#ifndef PL_CLOTH_H
#define PL_CLOTH_H

#include "types.h"
#include "vec.h"

class cModel;

// Collision volume a cloth chain avoids (pl_cloth.cpp `*At` tables), 0x24 bytes: attached to a
// model part, sphere (p1 zero) or capsule between p0 and p1.
struct CLOTH_AT_SET {
    u16 Type;          // 0x00
    u8 P1;       // 0x02
    u8 P2;       // 0x03
    f32 Weight;        // 0x04
    f32 R;           // 0x08  radius
    Vec Ofs1;          // 0x0C
    Vec Ofs2;          // 0x18
};

// Cloth / pendulum chain work of one accessory (game/pl_cloth.cpp, game/pendulum.cpp), 0x60 bytes.
// pendulum.h's PenCloth is the same object with the obj units' field names.
struct PlCloth {
    int Num;             // 0x00  number of chain links
    u8* pCloth;          // 0x04  model parts index per link
    u8* pLeft;           // 0x08  left neighbour per link (0xFF = none)
    u8* pRight;          // 0x0C  right neighbour
    u8* pUpLeft;         // 0x10  (Ada dress)
    u32 pUpRight;             // 0x14
    u8* pParent;             // 0x18  upper neighbour per link
    u8* pChild;           // 0x1C  lower neighbour per link
    u32 pGravity;             // 0x20
    f32* pRate;          // 0x24  per-link rate (em_cloth: em18ClothRate, em37HairRate, ...)
    f32* pMax;           // 0x28  max swing per link
    f32* pWindSin;         // 0x2C  wind phase per link
    f32* pWindRate;         // 0x30  wind rate per link
    CLOTH_AT_SET* pAtset;      // 0x34  collision volumes
    int At_num;             // 0x38
    f32 Gravity;             // 0x3C  link length
    f32 Rate;             // 0x40
    int Bundle_num;             // 0x44
    f32 WindSin;             // 0x48
    f32 Stretchy;             // 0x4C
    f32 Move_rate;             // 0x50  gravity / stiffness rate (skirt: 0.9 under water, 0.5 otherwise)
    u32 pPtbl;             // 0x54
    cModel* pModel;      // 0x58  (AdaRibbonSet)
    u32 Flag;           // 0x5C  0x100 / 0x200 / 0x302
};

// The player units pass these in this order; PlClothSet*/Move* use them as (jacket, holster, hair)
// and (skirt, hair, sweater) respectively (the original naming does not match the use).
extern PlCloth leonHair;
extern PlCloth leonJacket;
extern PlCloth leonHolster;
extern PlCloth girlHair;
extern PlCloth girlSkirt;
extern PlCloth girlSweater;

extern PlCloth luisHair;
extern PlCloth adaDress;
extern PlCloth adaHair;
extern PlCloth adaRibbon;

// Chain object (game/obj1d.cpp, obj1d.h).
class cObjChain;
cObjChain* SetChain(void* bin, void* tpl, Vec* pos, Vec* rot);

void PlClothSetLeon(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3);
void PlClothMoveLeon(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3);
void PlClothSetGirl(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3, int mode);
void PlClothMoveGirl(cModel* pl, PlCloth* pCloth1, PlCloth* pCloth2, PlCloth* pCloth3);
void PlClothSetLuis(cModel* pl, PlCloth* pCloth1);
void PlClothMoveLuis(cModel* pl, PlCloth* pCloth1);
void PlClothSetAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair, int evt);
void PlClothMoveAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair);
cObjChain* AdaRibbonSet(cModel* pl, PlCloth* ribbon, void* bin, void* tpl);

// game/pl_cloth.cpp: Ada's hair chain parameters (pl02 builds its costume-2 hair from them).
extern f32 adaHairMax[14];
extern f32 adaHairWindS[14];
extern f32 adaHairWindR[14];
extern CLOTH_AT_SET adaHairAt[6];

#endif
