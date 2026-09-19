// game/em_set.cpp: enemy creation from the room enemy list (ESL). pG->Em_list holds 256 EmListData
// entries (id, type, set, flag, hp, position in 10 unit steps, rotation in 1/16384 turns, stage /
// room); EmSetFromList creates every entry of the current room at room start and event code
// creates single entries with EmSetFromList2. Killed list enemies are recorded per list in the
// pG->Em_flg bit tables so they stay dead when the room is re-entered.

#include "atari.h"
#include "light.h"
#include "em_set.h"
#include "global.h"
#include "db_log.h"

extern cEm* pPL;   // game/em.cpp

cEm* errEm = 0;
static int emSetDummy = 0;

// Death bit of list entry `no` in the current enemy list (0 when no list is loaded).
// Death bit table of the current enemy list (pG->Em_flg[pG->emlist_no]); the original computes
// it with byte arithmetic: the row offset is added to pG before the table offset.
#define EM_DEAD_TBL() ((u32*) (pG->em_list_no * 0x20 + (u32) pG + 0x501C))

// Death bit of list entry `no` in the current list (pG->em_list_no); 0 when no list is loaded.
static inline u32 EmSetDieCk(u32 no)
{
    u32 v;

    if (pG->em_list_no >= 0) {
        u32* tbl = EM_DEAD_TBL();

        v = tbl[no >> 5] & (0x80000000 >> (no & 31));
    } else {
        v = 0;
    }
    return v;
}

// Sets the death bit of list entry `no` in the current list.
static inline void EmSetDieOn(u32 no)
{
    if (pG->em_list_no >= 0) {
        u32* tbl = EM_DEAD_TBL();

        tbl[no >> 5] |= 0x80000000 >> (no & 31);
    }
}

// Counter update through a reference: the store is a plain scalar access, so pG is reloaded after it.
static inline void CntInc(u32& c) { c++; }

// While flags_68 bit21 is set only the enemies 3 and 4 may be created.
#define EM_SET_ID_NG(id) (DbgFlagChk(pG, DBG_NO_ENEMY) && ((id) != 3 && (id) != 4))

// Pulls an enemy work for `id`: the partner (0xF) and the parasite (0x25) are created at the back
// of the pool so they move after the others.
static inline cEm* EmCreate(u8 id)
{
    if (id == 0xF || id == 0x25) {
        return EmMgr.createBack(id);
    }
    return EmMgr.create(id);
}

// Copy the list entry into the fresh enemy work.
static inline void EmSetWork(cEm* em, EmListData* d, u8 no)
{
    f32 kx = 1000.0f;
    f32 kr = 3.1415927f / 16384.0f;
    f32 kp = 10.0f;

    em->type = d->type;
    em->set = d->set;
    em->flag = d->flag;
    em->Character = d->Character;
    em->Guard_r = (f32) d->Guard_r * kx;
    em->hp_max = em->hp = d->hp;
    em->ang.x = (f32) d->rot[0] * kr;
    em->ang.y = (f32) d->rot[1] * kr;
    em->ang.z = (f32) d->rot[2] * kr;
    em->pos.x = (f32) d->pos[0] * kp;
    em->pos.y = (f32) d->pos[1] * kp;
    em->pos.z = (f32) d->pos[2] * kp;
    em->pos_old = em->pos;
    em->emset_no = no;
}

// The same body as a macro for the two straight-line creators (EmSetFromList2, EmSetEvent). A pool
// constant expanded in the caller keeps RTX_UNCHANGING_P on its MEM; integrate.c drops it when it
// copies an inlined body (copy_rtx_and_substitute, `map->integrating`), so the inline's `lfs` loads
// carry true/anti dependences on every store around them and haifa cannot move them (they end up
// as `lis; lfs` pairs at the top of the block in the original). The loop in EmSetFromList keeps the
// inline (the invariants are hoisted differently with the macro: 11 words).
#define EM_SET_WORK(em, d, no)                                                            \
    do {                                                                                  \
        f32 kx = 1000.0f;                                                                 \
        f32 kr = 3.1415927f / 16384.0f;                                                   \
        f32 kp = 10.0f;                                                                   \
        (em)->type = (d)->type;                                                           \
        (em)->set = (d)->set;                                                             \
        (em)->flag = (d)->flag;                                                    \
        (em)->Character = (d)->Character;                                                             \
        (em)->Guard_r = (f32) (d)->Guard_r * kx;                                                 \
        (em)->hp_max = (em)->hp = (d)->hp;                                                 \
        (em)->ang.x = (f32) (d)->rot[0] * kr;                                             \
        (em)->ang.y = (f32) (d)->rot[1] * kr;                                             \
        (em)->ang.z = (f32) (d)->rot[2] * kr;                                             \
        (em)->pos.x = (f32) (d)->pos[0] * kp;                                             \
        (em)->pos.y = (f32) (d)->pos[1] * kp;                                             \
        (em)->pos.z = (f32) (d)->pos[2] * kp;                                             \
        (em)->pos_old = (em)->pos;                                                         \
        (em)->emset_no = (no);                                                             \
    } while (0)

// Squared XZ distance to the player. The `x374 = 1e16` reset is a caller statement AFTER this call:
// inside the inline its pool load loses RTX_UNCHANGING_P (see above) and wins the sched1 tie against
// the `dz * dz` multiply through the "independent of the last scheduled insn" class (haifa
// rank_for_schedule), which puts the constant one insn too early and costs it f12 (local-alloc's
// fake_birth avoids a register that died in the previous insn).
static inline void EmSetDist(cEm* em)
{
    f32 dz = pPL->pos.z - em->pos.z;
    f32 dx = pPL->pos.x - em->pos.x;

    em->plDist2 = dx * dx + dz * dz;
}

// Work `no` with the range check read through a manager copy (map_obj.h getWork). A plain
// `for (i = 0; i < EmMgr.nArray; i++)` around it gives the original shape: gcse PRE turns the second
// nArray read into a copy of the first (`mr r10, r0`), the bottom test uses that copy and the back
// edge is threaded past the check.
static inline cEm* emSetWork(u32 no)
{
    cEmMgr* m = &EmMgr;
    if (no >= m->nArray) {
        return 0;
    }
    return (cEm*) ((u8*) m->pArray + m->size * no);
}

// 1 when no live enemy already carries list entry `no` (0xFF: always 1).
int checkListId(int no)
{
    u32 i;

    if (no == 0xFF) {
        return 1;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = emSetWork(i);

        if ((em->be_flag & 0x201) == 1 && em->emset_no == (u8) no) {
            return 0;
        }
    }
    return 1;
}

// Room start: creates every list entry that is alive (be_flag bit0), not yet set (bit1), not
// recorded dead, belongs to the current stage / room, has an id and is not blocked by the debug
// filter; copies the entry into the work, marks it set (bit1, toggling bits 2/3), computes the
// player distance and runs the first move.
void EmSetFromList()
{
    u32 i;

    for (i = 0; i < 256; i++) {
        EmListData* d = EM_LIST(i);
        cEm* em;

        if (!(d->be_flag & 1)) {
            continue;
        }
        if (d->be_flag & 2) {
            continue;
        }
        if (EmSetDieCk(i)) {
            continue;
        }
        if (pG->stage_no != d->room >> 8) {
            continue;
        }
        if (pG->room_no != (d->room & 0xFF)) {
            continue;
        }
        if (checkListId(i) == 0) {
            continue;
        }
        // d->id read directly: the range fold of EM_SET_ID_NG keeps the QImode load and the int
        // uses share one PRE'd `clrlwi`; a `u8 id` local is promoted and never masked.
        if (d->id == 0) {
            continue;
        }
        if (EM_SET_ID_NG(d->id)) {
            continue;
        }
        em = EmCreate(d->id);
        if (em == 0) {
            pLog->err(0, 0, "EmSetFromList() Em set failed, Id = %x", d->id);
            continue;
        }
        EmSetWork(em, d, i);
        d->be_flag |= 2;
        if (d->be_flag & 4) {
            d->be_flag |= 8;
            d->be_flag &= ~4;
        } else if (!(d->be_flag & 8)) {
            d->be_flag |= 4;
        }
        EmSetDist(em);
        em->l_sub = 1.0e16f;
        em->move();
    }
}

// Creates list entry `no` on demand (room event scripts): same checks as EmSetFromList, the
// death bit only with chkDead. Returns the enemy, or errEm when nothing was created.
cEm* EmSetFromList2(int no, int chkDead)
{
    EmListData* d = EM_LIST(no);
    cEm* em;

    if (EM_SET_ID_NG(d->id)) {
        return errEm;
    }
    if (pG->stage_no != d->room >> 8) {
        return errEm;
    }
    if (pG->room_no != (d->room & 0xFF)) {
        return errEm;
    }
    if (d->be_flag & 2) {
        return errEm;
    }
    if (d->id == 0) {
        return errEm;
    }
    if (chkDead) {
        if (EmSetDieCk(no)) {
            return errEm;
        }
    }
    if (checkListId(no) == 0) {
        return errEm;
    }
    em = EmCreate(d->id);
    if (em == 0) {
        pLog->err(0, 0, "EmSetFromList2() Em set failed, Id = %x", d->id);
        return errEm;
    }
    EM_SET_WORK(em, d, no);
    d->be_flag |= 2;
    if (d->be_flag & 4) {
        d->be_flag |= 8;
        d->be_flag &= ~4;
    } else if (!(d->be_flag & 8)) {
        d->be_flag |= 4;
    }
    EmSetDist(em);
    em->l_sub = 1.0e16f;
    em->move();
    return em;
}

// Event enemy from a list entry outside the room list (never called in the DOL).
cEm* EmSetEvent(EmListData* d)
{
    cEm* em;

    if (EM_SET_ID_NG(d->id)) {
        return errEm;
    }
    em = EmCreate(d->id);
    if (em == 0) {
        pLog->err(0, 0, "EmSetEvent() Em set failed, Id = %x", d->id);
        return errEm;
    }
    EM_SET_WORK(em, d, 0xFF);
    d->be_flag = 7;
    EmSetDist(em);
    em->l_sub = 1.0e16f;
    em->move();
    return em;
}

// The live enemy created from list entry `no`; NULL when none (or no == 0xFF).
cEm* GetEmPtrFromList(int no)
{
    u32 i;

    if (no == 0xFF) {
        return 0;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = emSetWork(i);

        if ((em->be_flag & 0x201) == 1 && em->emset_no == (u8) no) {
            return em;
        }
    }
    return 0;
}

// The list entry an enemy was created from; NULL for enemies not from the list (emset_no 0xFF).
EmListData* GetListPtrFromEm(cEm* em)
{
    if (em->emset_no == 0xFF) {
        return 0;
    }
    return EM_LIST(em->emset_no);
}

// Enemy id of list entry `no` (0xFF for an invalid index).
u8 GetEmIdFromList(u32 no)
{
    EmListData* list;

    if (no >= 0xFF) {
        return 0xFF;
    }
    list = (EmListData*) pG->Em_list;
    return list[no].id;
}

// Sets / clears the alive bit (be_flag bit0) of list entry `no`, only for entries of the current
// stage / room.
void EmListSetAlive(int no, int on)
{
    EmListData* d = EM_LIST(no);

    if (pG->stage_no != d->room >> 8) {
        return;
    }
    if (pG->room_no != (d->room & 0xFF)) {
        return;
    }
    if (on == 1) {
        d->be_flag |= 1;
    } else {
        d->be_flag &= ~1;
    }
}

// Records the death of a list enemy in the current death bit table (so it is not re-created),
// except in stage 4 rooms and under the debug flags that keep enemies respawning.
void EmSetDie(cEm* em)
{
    if (DbgFlagChk(pG, DBG_NO_SCE_EXE)) {
        return;
    }
    if (pG->stage_no == 0 && pG->room_no == 4) {
        return;
    }
    if (DbgFlagChk(pG, DBG_EM_NO_DIE_FLAG)) {
        return;
    }
    if (EmSetDieCk(em->emset_no)) {
        return;
    }
    EmSetDieOn(em->emset_no);
}

// Counts a kill in the chapter and game kill counters (results screen).
void EmSetDieCnt()
{
    CntInc(pG->c_kill_cnt);
    CntInc(pG->g_kill_cnt);
}

// Room change: clears the "set" bit of every list entry so the new room can create its enemies.
void EmSetRoomInit()
{
    int i;

    for (i = 0; i < 256; i++) {
        EmListData* d = EM_LIST(i);

        d->be_flag &= ~2;
    }
}

// Every 30 frames: intended to delete waiting list enemies; the loop body is empty in the
// shipped game.
void EmListWaitDelete()
{
    int i;

    if (pG->Frame_cnt % 30 != 0) {
        return;
    }
    for (i = 0; i < 256; i++) {
    }
}
