#ifndef EMSWITCH_H
#define EMSWITCH_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emBarred.h"

// Work of the lever switch enemy (game/emswitch.cpp), overlaid on cEm from 0x3E0.
struct EmSwitchWork {
    u8 pad_0[0x210];
    int state;            // 0x210 (0x5F0)  1 = open, 2 = closed, 0 = moving (ckSwitch)
    int opened;           // 0x214 (0x5F4)  ckOpen
    u8 pad_218[4];
    class cEmBarred* pBarred;     // 0x21C (0x5FC)  gates the lever drives
    class cEmBarred* pBarred2;    // 0x220 (0x600)
    class cEmSwitch* pSwitch;    // 0x224 (0x604)  switch moved together with this one
    u8 Mode;              // 0x228 (0x608)  1 = open only (no close prompt), 2 = closes again after opening, 3 = auto open
    u8 Damage_ck;          // 0x229 (0x609)  a hit toggles the lever
    u8 Barrel_ck;            // 0x22A (0x60A)  closing drops the r227 barrel
    int Barrel_wait;            // 0x22C (0x60C)  frames between barrel drops
    f32 Ck_dis;           // 0x230 (0x610)  action button distance (1500, setLongCk 2000)
    u8 Etc_no;            // 0x234 (0x614)  etc flag the lever state is saved in
    u8 pad_235[3];
    int actButton;        // 0x238 (0x618)  0 = no action button prompt
};

#define EMSWITCH_WK(em) ((EmSwitchWork*) (((cEmSwitch*) (em))->free))

class cEmSwitch : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMSWITCH_WK)
    virtual void move();

    int ckSwitch();
    int ckOpen();
    void setOpen();
    void setClose();
    void setOpened();
    void setClosed();
    void setBarred(cEmBarred* b);
    void setBarred2nd(cEmBarred* pBarred);
    void setConnectSwitch(cEmSwitch* s);
    void setActButton(int on);
    void setOpenOnly();
    void setAutoOpen();
    void setBarrel();
    void setLongCk();
};

extern "C" {
cEmSwitch* SetEmSwitch(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo);
void emSwitch_R1_Set(cEmSwitch* em);
void emSwitch_R1_Open(cEmSwitch* em);
void emSwitch_R1_Close(cEmSwitch* em);
void emSwitchOperationActEvtCk(cEmSwitch* em);
void emSwitchActOpen(cEmSwitch* em);
void emSwitchActClose(cEmSwitch* em);
}

#endif
