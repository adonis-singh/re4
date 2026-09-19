#ifndef EMHIT_H
#define EMHIT_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the hit-only enemy (game/emhit.cpp), overlaid on cEm from 0x3E0.
struct EmHitWork {
    u32 Be_flg;            // 0x000 (0x3E0)
    int Timer;            // 0x004 (0x3E4)  beetle: frames before it fades out
    u8 pad_8[4];
    int Status;           // 0x00C (0x3EC)  1 = damaged this frame (ckStatus / ckDmgWeapon)
    cModel* pParent;      // 0x010 (0x3F0)  model the hit follows (setParent)
    int partsNo;          // 0x014 (0x3F4)
    int noNormalize;      // 0x018 (0x3F8)  setParent 3rd argument: keep the parent's scale
    u8 pad_1C[0x224 - 0x1C];
    Vec spd;              // 0x224 (0x604)  beetle fly-away speed
    Vec size;             // 0x230 (0x610)  yarare box size
    void* mot0;           // 0x23C (0x61C)  beetle motions (setBeetle)
    void* mot1;           // 0x240 (0x620)
    void* mot2;           // 0x244 (0x624)
    u8 x248;              // 0x248 (0x628)  0xFF
};

#define EMHIT_WK(em) ((EmHitWork*) (((cEmHit*) (em))->free))

// Hit-only enemy: a cEm that exists to receive weapon damage for an object (bell, ...) or to
// follow a parent model's parts (setParent); type 3 is the beetle that flies off when shot.
class cEmHit : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMHIT_WK)
    virtual void move();

    int ckStatus();                                 // EmHitWork::status
    int ckDmgWeapon();                              // weapon id of the damage taken this frame, 0 = none
    void setParent(cModel* parent, int partsNo, int noNormalize);   // 0x8010539C
    void setBeetle(void* mot0, void* mot1, void* mot2);
};

extern "C" {
cEmHit* SetEmHit(void* bin, void* tpl, Vec* pos, Vec* rot, int type);
void emHitDmCk(cEmHit* em);
void emHit_R0_Init(cEmHit* em);
void emHit_R0_Move(cEmHit* em);
void emHit_R1_Set(cEmHit* em);
void emHit_R1_Parent(cEmHit* em);
void emHit_R1_Break(cEmHit* em);
void emHit_R1_Beetle(cEmHit* em);
void emHitYarareInit(cEmHit* em);
// Hit box setup (game/at_mod.cpp): the model's own box (em->hitInfo) and extra boxes chained to it.
// The parts number / flags come last: the callers' `li` argument loads are scheduled after the
// float moves (emhit, obj14, obj15 ...).
void YarareInit(cEm* em, f32 x, f32 y, f32 z, f32 w, f32 h, s16 no, u16 flags);
void YarareInitCube(cEm* em, f32 x, f32 y, f32 z, f32 w, f32 h, f32 extent, s16 no, u16 flags);
void YarareAdd(cEm* em, YARARE_INFO* box, f32 x, f32 y, f32 z, f32 w, f32 h, s16 no, u16 flags);
void YarareAddCube(cEm* em, YARARE_INFO* box, f32 x, f32 y, f32 z, f32 w, f32 h, f32 extent, s16 no, u16 flags);
int EmGetDmPos(cEm* em, Vec* pos, Vec* dir);                                     // em_sub.cpp
void EmDmBloodSet2(cEm* em, int est_id, int type, int mode, int esp_core_flg, int core_kind);               // em_sub.cpp
int VehicleAdjust(Vec* pos);                                                     // em_sub.cpp: rides `pos` along the trolley (room 21B)
}

// Attack parameters handed to EmAtkSetDamagePL (obj15 Obj15_atk_info_tbl: {100.0, 8, 600, 0, 10, 0}).
struct EmAtkInfo {
    f32 range;   // 0x00
    int type;    // 0x04
    u16 dmg;     // 0x08
    u16 flag;     // 0x0A  bit2: LifeDownSet2 keep, bit3: pl_life = 0 (PS2 ATK_INFO.flag)
    u16 dm_cnt;     // 0x0C  (PS2 ATK_INFO.dm_cnt)
    u16 x0E;     // 0x0E
};

extern "C" {
void EmPlBloodSet2(cModel* m, Vec* pos, int a, int eff_id, int type);                 // em_sub.cpp
// Line `a`-`b` against the enemies: the hit enemy or NULL; hit point / normal and the scenario attribute out.
cEm* EmAtkLineHitCk(Vec* pPos, Vec* pPos2, Vec* hit, Vec* nrm, u32* attr);              // em_sub.cpp
void EmAtkSetDamagePL(cEm* em, EmAtkInfo* info, Vec* pPos, Vec* pPos2);                 // em_sub.cpp
}

void PlSetDamage(int type, int dmg, int flag);                                   // em_sub.cpp (C++ linkage; obj10 hitCkPl)

#endif
