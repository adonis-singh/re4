#ifndef EM_SUB_H
#define EM_SUB_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"
#include "item.h"

// game/em_sub.cpp: the shared enemy helper library (hit boxes, damage, blood effects, weapon
// target lists, catch motions, item drops). EmGetDmPos, EmDmBloodSet2, EmPlBloodSet2,
// EmAtkLineHitCk, EmAtkSetDamagePL, VehicleAdjust and PlSetDamage are declared in emhit.h.

// GetWepTargetList* entry: the enemy and the hit box that was hit.
struct WepTarget {
    cEm* em;
    YARARE_INFO* part;
};

extern u32 No_drop_cnt;
extern u32 No_drop_cnt2;

extern "C" {
void Em_R0_Scenario(cEm* pEm);
void EmDmBloodSet(cEm* pEm);
void EmDmBloodSet3(cEm* pEm, u32 est_id, u32 est_no, u32 mode, u16 esp_core_flg, u32 core_kind);
void EmPlBloodSet(cEm* pEm, Vec* pPos, u32 type, u8 eff_id, u8 est_id);
void EmSubBloodSet(cEm* pEm, Vec* pPos, u32 type, u8 eff_id, u8 est_id);
// Hit box of `em` touched by the capsule of the 8-corner `box`; the best one by squared distance.
YARARE_INFO* emBoxAtCk(cEm* pEm, Vec* pBox, Vec* pPos, int wep_no);
YARARE_INFO* emLineAtCk(cEm* pEm, Vec* pPos, Vec* pPos2, f32 hit_len, int wep_no);
YARARE_INFO* emLineAtCk2(cEm* pEm, Vec* pPos, Vec* pPos2, f32 hit_len, Vec* pCross, int wep_no);
int emLineCapsuleCrossCk(Vec* a, Vec* b, Vec* pTop, Vec* pBtm, Vec* pCross, f32 r);
int emLineCubeCrossCk(Vec* a, Vec* b, Mtx m, Vec* ofs, Vec* hit, f32 sx, f32 sy, f32 sz);
int emLinePolyCrossCk(Vec* pPos, Vec* pPos2, Vec* pRect, Vec* pCross);
YARARE_INFO* emSphereAtCk(cEm* em, Vec* pos, Vec* pos2, f32 r, int flag, f32 r2);
u32 GetWepTargetList(Vec* box, Vec* pos, WepTarget* list, u32 max, int flag);
u32 GetWepTargetList2(Vec* pPos, Vec* pPos2, WepTarget* list, u32 max, Vec* hit, Vec* nrm, u32* attr, int type,
                      int flag);
int GetWepTargetListBomb(Vec* pPos, f32 radius, WepTarget* list, int num, int wep_no, int flag);
int PlBombHitCk(Vec* pPos, f32 radius);
int GetWepTargetPos(Vec* pPos, Vec* pPos2, int mode, int wep_no, cEm** ppEm, u32* pAttr);
YARARE_INFO* EmYarareContactCk(cEm* em, Vec* pos, f32 r, Vec* out);
void EmYarareDisp(cEm* pEm);
void EmScenario(cEm* pEm);
int LifeDownSet(cEm* pEm, int dm_val, int rnd);
int LifeDownSet2(cEm* pEm, int dm_val, int rnd, int flag);
int EmAtkHitCk(EmAtkInfo* info, Vec* pPos, Vec* pPosOld, int noSub);
int EmAtkHitCk2(EmAtkInfo* pAtk, Vec* pPos, Vec* pPosOld);
YARARE_INFO* EmAtkLineHitCkSub(Vec* pPos, Vec* pPos2, Vec* pCross, Vec* pNorm);
void EmAtkSetDamageSub(YARARE_INFO* pAt, EmAtkInfo* pAtk, Vec* pPos, Vec* pPos2);
YARARE_INFO* EmAtkHitSubCk2(EmAtkInfo* pAtk, Vec* pPos, Vec* pPosOld);
void EmCatchPLSet(cEm* em, f32 pl_dir, u32 mode, f32 x, f32 y, f32 z, void (*ft)(cPlayer*));
int EmCatchMotionMove(cEm* pEm, f32 rot_rate, f32 pos_rate);
int EmRackCk(cEm* pEm, Vec* pPos, f32 dir);
int GetBulletPoint();
void GetDropBullet(int* ret_id, int* ret_num);
int GetRecoveryPoint();
void EmSetDropItem(cEm* pEm);
void EmReserveDropItem(cEm* pEm);
void RandomItemSet(cEm* pEm);
int RandomItemCk(int em_id, int* ret_id, int* ret_num, int ctrl_flag);
int CheckInWater(cModel* pEm, int parts_no);
int HandgunCk(int wep_no);
int TrolleyItemSetCk(Vec* pPos, ITEM_ID item_id, int item_num);
int BullItemSetCk(Vec* pPos, ITEM_ID item_id, int item_num);
void adjust_add_set(Vec add);
}

// Position of `em` (the player when NULL) plus `t` of its parts 0 movement this frame (C++ linkage;
// Bio4.sym marks it local but the em3c module calls it).
void GetPlPos(Vec* pPos, f32 frame, cEm* pEm);

#endif
