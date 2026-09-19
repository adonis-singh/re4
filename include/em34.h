#ifndef EM34_H
#define EM34_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "pl_cloth.h"

class cModelInfo;

// Work of the em34 enemy (em34 module, D:/Bio4/Prog/em34.cpp; the em34 / em37 / em33 variants are
// picked by cModel::type: 1 = em37, 2..3 = em33, else em34), overlaid on cEm from 0x3E0.
struct Em34Work {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: route to the player found, bit1: partner present, bit2: targets the partner, bit3: damage / die routine, bit4: neck follows the target
    int Timer;            // 0x004 (0x3E4)
    u8 pad_8[4];
    YARARE_INFO hit[3];     // 0x00C (0x3EC)  extra hit boxes (YarareAdd)
    u8 pad_A8[0x214 - 0xA8];
    f32 routeAng;         // 0x214 (0x5F4)  Muku towards the route point (player)
    f32 Pl_rot;      // 0x218 (0x5F8)
    f32 Sub_dir;           // 0x21C (0x5FC)  the same for the partner
    f32 Sub_rot;        // 0x220 (0x600)
    f32 Go_dir;        // 0x224 (0x604)  copy of the chosen target's angle / distance
    f32 Go_rot;     // 0x228 (0x608)
    f32 L_go;       // 0x22C (0x60C)
    Vec Pl_pos;         // 0x230 (0x610)  RouteCkToPos result towards the player
    Vec Sub_pos;      // 0x23C (0x61C)  towards the partner
    Vec Go_pos;        // 0x248 (0x628)  chosen target position (R1_Walk / R1_Atk turn to it)
    cEm* pEm;         // 0x254 (0x634)  pPL or pSUB
    f32 Neck_dir_y;          // 0x258 (0x638)  smoothed neck yaw (em34NeckMove)
    PlCloth Cloth1;       // 0x25C (0x63C)  Em34ClothSet1 / Em37HairSet / Em33ClothSet
    PlCloth Cloth2;       // 0x2BC (0x69C)  Em34ClothSet2 / Em37CoatSet / Em33ClothSet2
    cModelInfo* pShoulder;   // 0x31C (0x6FC)  em34 extra models (ARC 5, 6, 7)
    cModelInfo* pHead;   // 0x320 (0x700)
    cModelInfo* pHand;   // 0x324 (0x704)
    u8 Atk_ck;            // 0x328 (0x708)  the attack already hit (em34AtkCk)
};

#define EM34_WK(em) ((Em34Work*) (((cEm34*) (em))->free))

class cEm34 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM34_WK)
    virtual void move();
};

void Em34Init(cEm* em);
void em34DmCk(cEm34* em);
void em34RouteCk(cEm34* em);
void em34NeckMove(cEm34* em);
int em34AtkCk(cEm34* em, int no, int parts);

#endif
