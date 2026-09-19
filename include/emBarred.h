#ifndef EMBARRED_H
#define EMBARRED_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cSat;
class cEmBarred;

// Work of the barred gate enemy (game/emBarred.cpp), overlaid on cEm from 0x3E0.
struct EmBarredWork {
    u32 be_flag;            // 0x000 (0x3E0)  bit0: never closes (setNoClose), bit1: check what stands under it (setUnderCk)
    int Timer;            // 0x004 (0x3E4)  frames the player stayed away (R1_Set), shake frames (R1_Open/Close)
    f32 spd;              // 0x008 (0x3E8)  fall speed while closing
    YARARE_INFO hit[4];     // 0x00C (0x3EC)  extra hit boxes of type 6 (YarareAddCube)
    u8 pad_DC[0x214 - 0xDC];
    Vec pos0;             // 0x214 (0x5F4)  closed position
    f32 Height;            // 0x220 (0x600)  height the gate rises to
    f32 Width;            // 0x224 (0x604)  half width of the under check
    cSat* pEat;            // 0x228 (0x608)  effect collision quad
    cSat* pEatFrame[4];         // 0x22C (0x60C)  type 6: the four bar pieces
    int Status;           // 0x23C (0x61C)  ckStatus: 0 moving, 1 open, 2 closed
    int Open_flag;             // 0x240 (0x620)  ckOpen: 1 while opening / open
    u32 Seid;            // 0x244 (0x624)  SndCall id of the moving sound (SndStop)
    cEmBarred* pBarred;   // 0x248 (0x628)  the other gate of a pair (setDouble): opens / closes with this one
    u8 Lock_mode;          // 0x24C (0x62C)  setLockMode: 1 = the player cannot open it
    u8 Eff_id;               // 0x24D (0x62D)  setEff: effect owner id, 0xFF = none
    u8 Etc_no;            // 0x24E (0x62E)  etc flag number (bit0 broken, bit1 open)
};

#define EMBARRED_WK(em) ((EmBarredWork*) (((cEmBarred*) (em))->free))

// Barred gate enemy: the iron gates / portcullises that open for the player and drop back shut.
class cEmBarred : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMBARRED_WK)
    virtual void move();   // key function: the vtable stays in this unit (cEmMgr::construct stores it)

    void setOpen(int mode);
    void setClose(int mode);
    void setOpened();
    void setClosed();
    void setLockMode(u8 mode);
    int ckStatus();
    int ckOpen();
    void setEff(u8 eff);
    void setBreak(Vec* target);
    void setNoClose();
    void setDouble(cEmBarred* other);
    void setUnderCk();
};

extern "C" {
cEmBarred* SetEmBarred(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo, int type);
void emBarredDmCk(cEmBarred* em);
void emBarred_R1_Set(cEmBarred* em);
void emBarred_R1_Open(cEmBarred* em);
void emBarred_R1_Close(cEmBarred* em);
void emBarred_R1_Break(cEmBarred* em);
void emBarredEatSet(cEmBarred* em);
int emBarredNearCk(cEmBarred* em);
int emBarredUnderCk(cEmBarred* em);
}

#endif
