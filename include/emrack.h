#ifndef EMRACK_H
#define EMRACK_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cSat;

// The free area of cEmRack cast to the rack's work (PS2 FREE_EMRACK), 0x140 bytes from cEmRack+0x3E0.
typedef struct {
    u32 Be_flg;               // 0x000 (0x3E0)
    int Timer;                // 0x004 (0x3E4)  frames the rack shakes (emRack_R1_Shock)
    f32 TmpF;                 // 0x008 (0x3E8)  fall rotation speed (emRack_R1_Down)
    f32 Size_x;               // 0x00C (0x3EC)  yarare box size
    f32 Size_y;               // 0x010 (0x3F0)
    f32 Size_z;               // 0x014 (0x3F4)
    YARARE_INFO YarareTbl[4]; // 0x018 (0x3F8)  extra yarare cubes of type 1
    f32 Rack_hp;              // 0x0E8 (0x4C8)  shotgun hits left before the shock (1.0 for type 1)
    cSat* pSat;               // 0x0EC (0x4CC)
    cSat* pEatUnder;          // 0x0F0 (0x4D0)  runtime collision pieces (emRackSatSet)
    cSat* pEatCenter;         // 0x0F4 (0x4D4)
    cSat* pEatTop;            // 0x0F8 (0x4D8)
    u8 Eff_id;                // 0x0FC (0x4DC)  setEff: effect owner id, 0xFF = none
    u8 Etc_no;                // 0x0FD (0x4DD)  etc flag index (broken flag)
} FREE_EMRACK;

// The push range (matrix, inverse, 4 limits, flags) is not in the free area: it sits at 0xD60 and is
// addressed through `this` (cEmRack::rackMat / rackInvMat / rackRange / rackFlags).

#define EMRACK_WK(em) ((FREE_EMRACK*) ((cEmRack*) (em))->free)

extern "C" {
cEmRack* SetRack(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo);
void emRackDmCk(cEmRack* em);
void emRack_R0_Init(cEmRack* em);
void emRack_R0_Move(cEmRack* em);
void emRack_R1_Set(cEmRack* em);
void emRack_R1_Down(cEmRack* em);
void emRack_R1_Break(cEmRack* em);
void emRack_R1_Shock(cEmRack* em);
void emRackSatSet(cEmRack* em);
void emRackSatClear(cEmRack* em);
void emRackYarareInit(cEmRack* em);
}

#endif
