#ifndef EM18_H
#define EM18_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "pl_cloth.h"

class cModelInfo;

// Work of the em18 enemy (em18 module, D:/Bio4/Prog/em18.cpp, the merchant), overlaid on cEm from 0x3E0.
struct Em18Work {
    u32 Be_flg;            // 0x000 (0x3E0)  bit3: damage / die routine, bit4: neck follows the player, bit5: trade started (em18TradeAction)
    u8 pad_4[8];
    YARARE_INFO hit[9];     // 0x00C (0x3EC)  extra hit boxes (YarareAdd)
    u8 pad_1E0[0x258 - 0x1E0];
    f32 neckAng;          // 0x258 (0x638)  smoothed neck yaw (em18NeckMove)
    PlCloth Cloth;        // 0x25C (0x63C)  Em18ClothSet / Em18ClothMove
    cModelInfo* pRHand;   // 0x2BC (0x69C)  em18HandSet (ARC 0x12)
    cModelInfo* pLHand;   // 0x2C0 (0x6A0)  em18HandSet (ARC 0x13)
    cModelInfo* pCloth;   // 0x2C4 (0x6A4)  em18ClothPartsSet
    cModelInfo* pGoods;   // 0x2C8 (0x6A8)  em18GoodsPartsSet
    cModelInfo* pRobe;   // 0x2CC (0x6AC)  extra model (ARC 0xE / 0xF)
    u32 sndId;            // 0x2D0 (0x6B0)  SndCall handle of the current voice
};

#define EM18_WK(em) ((Em18Work*) (((cEm18*) (em))->free))

class cEm18 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM18_WK)
    virtual void move();
};

void Em18Init(cEm* em);
void em18DmCk(cEm18* em);
void em18BloodSet(cEm18* em);
void em18ActEvtSetTrade(cEm18* em);
void em18NeckMove(cEm18* em);
void em18ClothPartsSet(cEm18* em, int on);
void em18GoodsPartsSet(cEm18* em, int on);
void em18HandSet(cEm18* em);

#endif
