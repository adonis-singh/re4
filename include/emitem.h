#ifndef EMITEM_H
#define EMITEM_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the item enemy (game/emitem.cpp), overlaid on cEm from 0x3E0.
struct EmItemWork {
    u32 Be_flg;            // 0x000 (0x3E0)
    u8 pad_4[8];
    int Status;           // 0x00C (0x3EC)  1 = landed (drop), 2 = broken, 3 = damaged (ckStatus)
    cModel* pParent;      // 0x010 (0x3F0)  model the item follows (setParent)
    int partsNo;          // 0x014 (0x3F4)
    int noNormalize;      // 0x018 (0x3F8)
    u8 pad_1C[0x224 - 0x1C];
    Vec spd;              // 0x224 (0x604)  drop speed
    Vec size;             // 0x230 (0x610)  yarare box size
    Vec rotAng;           // 0x23C (0x61C)  medal swing: current angles
    Vec rotSpd;           // 0x248 (0x628)  swing speeds
    Vec rotAmp;           // 0x254 (0x634)  swing amplitudes
    u8 rotType;           // 0x260 (0x640)  setRotType: 1 = swing, 2 = follow the model rotation
    u8 Eff_id;               // 0x261 (0x641)  setEff: effect number of the break (0xFF = none)
    u8 Etc_no;             // 0x262 (0x642)  etc flag index (type 1: taken flag)
};

#define EMITEM_WK(em) ((EmItemWork*) (((cEmItem*) (em))->free))

// Item enemy: a pick-up model that follows a parent's parts (setParent), swings like a medal
// (setRotType) and breaks or drops when damaged.
class cEmItem : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMITEM_WK)
    virtual void move();

    void setEff(u8 eff);
    int ckStatus();
    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setRotType(u8 type);
};

extern "C" {
cEmItem* SetEmItem(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int etcNo);
void emItemDmCk(cEmItem* em);
void emItem_R0_Init(cEmItem* em);
void emItem_R0_Move(cEmItem* em);
void emItem_R1_Set(cEmItem* em);
void emItem_R1_MedalSet(cEmItem* em);
void emItem_R1_Parent(cEmItem* em);
void emItem_R1_Drop(cEmItem* em);
void emItem_R1_Break(cEmItem* em);
void emItemYarareInit(cEmItem* em);
void emItemRotMove(cEmItem* em);
}

#endif
