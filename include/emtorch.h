#ifndef EMTORCH_H
#define EMTORCH_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the torch enemy (game/emtorch.cpp), overlaid on cEm from 0x3E0.
struct EmTorchWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: setParent flag (no matrix normalisation)
    int Timer;            // 0x004 (0x3E4)  30 after the first frame
    u8 pad_8[4];
    cModel* pParent;      // 0x00C (0x3EC)  model the torch follows (setParent)
    int partsNo;          // 0x010 (0x3F0)
    u8 pad_14[0x48 - 0x14];
    Vec size;             // 0x048 (0x428)  yarare box size
    Vec spd;              // 0x054 (0x434)  fall speed
    int Lost_wait;              // 0x060 (0x440)  150 when broken
    u8 EffKindId;             // 0x064 (0x444)  effect number of the flame (50)
    u8 Eff_id;               // 0x065 (0x445)  setEff: effect owner id of the flame, 0xFF = none
    u8 Etc_no;             // 0x066 (0x446)  etc flag index (broken / taken flag)
};

#define EMTORCH_WK(em) ((EmTorchWork*) (((cEmTorch*) (em))->free))

// Torch enemy: a candle / brazier / lamp model that follows a parent's parts (setParent), burns
// an effect (setEff) and breaks or falls when shot.
class cEmTorch : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMTORCH_WK)
    virtual void move();

    void setBreak();
    void setDelete();
    void setEff(u8 eff);
    void setParent(cModel* parent, int partsNo, int flag);
};

// C++ linkage (EtcModel.cpp calls it as SetTorch__FPvT0P3VecT2ii)
cEmTorch* SetTorch(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int etcNo);

extern "C" {
void emTorchDmCk(cEmTorch* em);
void emTorchSetBreak(cEmTorch* em, u32 kind);   // 0/1: break effect 2, 2: effect 3
void emTorch_R0_Init(cEmTorch* em);
void emTorch_R0_Move(cEmTorch* em);
void emTorch_R1_Set(cEmTorch* em);
void emTorch_R1_Parent(cEmTorch* em);
void emTorch_R1_Break(cEmTorch* em);
void emTorch_R1_Fall(cEmTorch* em);
void emTorchYarareInit(cEmTorch* em);
}

#endif
