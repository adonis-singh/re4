#ifndef EM3C_H
#define EM3C_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cModelInfo;
class cObj16;

// Work of the em3c enemy (em3c module, D:/Bio4/Prog/em3c.cpp), overlaid on cEm from 0x3E0. The four
// cModel::type variants share the routines: types 1 / 3 use the second motion set (Wep_type), types
// 2 / 3 carry the parasite head that attacks on its own (em3c_R1_CoreAtk).
struct Em3cWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: route to the player found, bit2, bit4: head bomb done, bit6: bomb SE played, bit7: player found,
                          //                bit8: damage routine, bit9: head damage routine, bit10: start / attack wait, bit11: parasite set
    int Timer;            // 0x004 (0x3E4)
    u8 pad_8[4];
    f32 TmpF;          // 0x00C (0x3EC)  em3c_R1_Turn180: target yaw
    int TmpU32;          // 0x010 (0x3F0)  em3c_R1_AtkWait: action button variant (1 / 2, 0 = none)
    u8 pad_14[0xC];
    f32 Pl_dir;         // 0x020 (0x600)  Muku towards the route point
    f32 Pl_rot;      // 0x024 (0x604)
    u8 pad_28[8];
    f32 Go_dir;        // 0x030 (0x610)
    f32 Go_rot;     // 0x034 (0x614)
    f32 L_go;       // 0x038 (0x618)
    f32 L_pl_route;           // 0x03C (0x61C)  RouteCkPosToPosDis to the player
    Vec Pl_pos;         // 0x040 (0x620)  RouteCkToPos result
    u8 pad_4C[0xC];
    Vec Go_pos;        // 0x058 (0x638)
    cEm* pEm;         // 0x064 (0x644)  the player
    u8 pad_68[0xC];
    int timer74;          // 0x074 (0x454)
    int Core_se_wait;       // 0x078 (0x458)  frames until the next parasite voice
    int Run_wait;        // 0x07C (0x45C)  frames the routine 4 choice is suppressed
    Vec Set_pos;         // 0x080 (0x460)  pos at init
    Vec Set_ang;         // 0x08C (0x46C)  rot at init
    YARARE_INFO hit[11];    // 0x098 (0x478)  extra hit boxes (YarareAdd); [9] the parasite, [10] the head object
    u8 pad_2D4[0x3A4 - 0x2D4];
    int HeadOffTimer;        // 0x3A4 (0x784)  frames until the head parts are hidden (em3cPartsBombHead) (PS2 HeadOffTimer; was `bombTimer`)
    cModelInfo* pWeapon; // 0x3A8 (0x788)  em3cModelInit extra models
    cModelInfo* pChainmail; // 0x3AC (0x78C)
    cObj16* pCore;    // 0x3B0 (0x790)  head object (em3cSetParasite)
    cObj16* pTen[4];     // 0x3B4 (0x794)  its four parasites (types 0 / 1)
    u8 pad_3C4[4];
    int Atk_wait;          // 0x3C8 (0x7A8)  frames until the next attack
    int Head_hp;           // 0x3CC (0x7AC)  damage left before the head bursts
    int Head_cnt;        // 0x3D0 (0x7B0)  head hits left before the big damage routine
    u32 HoseiCnt;         // 0x3D4 (0x7B4)  frames the collision halved the movement
    u8 EffKindIdCore;           // 0x3D8 (0x7B8)  EspPullCoreKind at creation
    u8 Atk_ck;            // 0x3D9 (0x7B9)  the attack hit (em3cAtkCk)
    u8 Act_ck;           // 0x3DA (0x7BA)  the player escaped the grab
    u8 Atk_type;             // 0x3DB (0x7BB)  em3c_R1_MoveAtk: attack table entry
    u8 Wep_type;            // 0x3DC (0x7BC)  type 1 / 3: second motion set (PS2 Wep_type; was `female`)
    u8 Armor_type;            // 0x3DD (0x7BD)  type 2 / 3
};

#define EM3C_WK(em) ((Em3cWork*) (((cEm3c*) (em))->free))

class cEm3c : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM3C_WK)
    virtual void move();
};

void Em3cInit(cEm* em);
void em3cDmCk(cEm3c* em);
int em3cAtkCk(cEm3c* em, Vec* pos, int no);
int em3cAtkCk2(cEm3c* em, int no);
void em3cRouteCk(cEm3c* em);
void em3cModelInit(cEm3c* em);
void em3cPartsBombSet(cEm3c* em, int add);
void em3cPartsBombHead(cEm3c* em);
void em3cPartsBombControl(cEm3c* em);
int em3cSetDmVal(cEm3c* em);
void em3cSetParasite(cEm3c* em);
void em3cFootSe(cEm3c* em);
int em3cStayCk(cEm3c* em);
int em3cFindCk(cEm3c* em);
void em3cDoorOpenCk(cEm3c* em);
void em3cAtkSuspend(cEm3c* em, int on);

#endif
