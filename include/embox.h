#ifndef EMBOX_H
#define EMBOX_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cSat;

// Work of the box enemy (game/embox.cpp), overlaid on cEm from 0x3E0.
struct EmBoxWork {
    u32 Be_flg;            // 0x000 (0x3E0)
    u8 pad_4[8];
    YARARE_INFO hit;        // 0x00C (0x3EC)  second yarare cube (YarareAddCube)
    u8 pad_40[0x214 - 0x40];
    Vec size;             // 0x214 (0x5F4)  yarare box size
    u8 pad_220[4];
    int itemNo;           // 0x224 (0x604)  item dropped when broken (-1 none)
    int Item_num;          // 0x228 (0x608)
    u16 Item_flg;          // 0x22C (0x60C)
    u16 Auto_item_flg;          // 0x22E (0x60E)
    cSat* pSat;           // 0x230 (0x610)  collision pieces disabled when broken
    cSat* pEat;           // 0x234 (0x614)
    void* Break_bin;       // 0x238 (0x618)  break model (setBreakModel, SetEffModel)
    void* Break_tpl;       // 0x23C (0x61C)
    int Lost_wait;            // 0x240 (0x620)  150 when broken
    u8 Eff_id;               // 0x244 (0x624)  setEff: effect owner id, 0xFF = none
    u8 Etc_no;             // 0x245 (0x625)  etc flag index (broken flag)
};

#define EMBOX_WK(em) ((EmBoxWork*) (((cEmBox*) (em))->free))

// Box enemy: breakable boxes, barrels, vases and cabinets (types 0..7) that drop an item.
class cEmBox : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMBOX_WK)
    virtual void move();

    void setEff(u8 eff);
    void setItem(int no, int num, u16 c, u16 d);
    void setBreakModel(void* bin, void* tpl);
};

extern "C" {
cEmBox* SetBox(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo);
void emBoxDmCk(cEmBox* em);
void emBoxSetBreak(cEmBox* em, u32 kind);
void emBox_R0_Init(cEmBox* em);
void emBox_R0_Move(cEmBox* em);
void emBox_R1_Set(cEmBox* em);
void emBox_R1_Break(cEmBox* em);
void emBoxSatClear(cEmBox* em);
void emBoxYarareInit(cEmBox* em);
void emBoxActEvtCk(cEmBox* em);
int checkNearOtherBarrel(cEmBox* em);
void emBoxAction(cEmBox* em);
void emBoxSetItem(cEmBox* em);
}

#endif
