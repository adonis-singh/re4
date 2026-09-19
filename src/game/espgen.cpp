// game/espgen: the effect controller ("ESP_CTRL") pool (D:/Bio4/Prog/espgen.cpp). A controller is
// an EspgenWork (0xC8 bytes) driven by id-indexed Move/Trans/SetFreeWork/Destruct tables: ids
// 0x00..0x3F are the generic controllers (00 emitter, 01 lens flare, 02 path emitter, 10 sequence
// player), 0x40..0x45 the application ones (42/45 water surfaces, 43 sand, 44 filter). The pool
// (EspgenArray, nEspgen) is allocated per room; EspgenMove / EspgenTrans run every active
// controller each frame, PullEspgen / PushEspgen allocate and release, EspgenSeqSet creates a
// controller from a Kind-1 record of an effect sequence, EspgenDelete removes by owner.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "main_mem.h"
#include "eprintf.h"
#include "db_log.h"

#define ESPGEN_ID_MAX 0x46
#define ESPGEN_APP_ID 0x40

extern "C" {
static void EspgenDummyMove(EspgenWork* w);
void EspgenFreeSizeCheckApp();
void EspgenFreeSizeCheck();
int EspgenInit();
int EspgenRoomInit();
int EspgenArrayAlloc(int n);
int EspgenArrayFree();
int EspgenArrayPush(int n);
int EspgenArrayPop();
void EspgenArrayClear();
int EspgenMove();
int EspgenTrans();
void EspgenDelete(int a, int b, int c);
void EspgenDeleteEvent();
int EspgenDispInfo();
int EspgenGetCallNo();
void EspgenIncCallNo();
int EspgenApplyFunc(void (*func)(EspgenWork* w));
// game/Espgen42.cpp
void EspWaterInit();
// game/eff_sys.cpp
void EspGenSetMoveLoop(int loop);

// generator entry points (game/espgen0*.cpp, Espgen4*.cpp)
void Espgen01_Move(EspgenWork* w);
void Espgen01_Trans(EspgenWork* w);
int Espgen01_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag);
void Espgen02_Move(EspgenWork* w);
int Espgen02_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag);
}
// game/espgen40.cpp (declared with the record type in the original)
void Espgen40_Move(EspGenWork* gen);

EspgenWork* EspgenArray = NULL;
EspgenWork* pEspgenArrayBack = NULL;
u32 nEspgen = 0;
u32 nEspgenBack = 0;
int g_Call_no = 0;
EspgenWork g_DmyEspgen;

EspgenMoveFunc EspgenMoveTblApp[6] = {
    (EspgenMoveFunc) Espgen40_Move, (EspgenMoveFunc) Espgen40_Move, Espgen42_Move, Espgen43_Move, Espgen44_Move,
    Espgen45_Move,
};
EspgenTransFunc EspgenTransTblApp[6] = {
    NULL, NULL, Espgen42_Trans, Espgen43_Trans, Espgen44_Trans, Espgen45_Trans,
};
EspgenSetFreeWorkAppFunc EspgenSetFreeWorkTblApp[6] = {
    NULL, NULL, Espgen42_SetFreeWork, Espgen43_SetFreeWork, Espgen44_SetFreeWork, Espgen45_SetFreeWork,
};
static EspgenDestructFunc EspgenDestructTblApp[6] = {
    NULL, NULL, Espgen42_Destruct, Espgen43_Destruct, Espgen44_Destruct, Espgen45_Destruct,
};

static EspgenMoveFunc EspgenMoveTbl[ESPGEN_APP_ID] = {
    Espgen00_Move,   Espgen01_Move,   Espgen02_Move,   EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, Espgen10_Move,   EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
    EspgenDummyMove, EspgenDummyMove, EspgenDummyMove, EspgenDummyMove,
};
EspgenTransFunc EspgenTransTbl[ESPGEN_APP_ID] = {
    NULL, Espgen01_Trans,
};
static EspgenSetFreeWorkFunc EspgenSetFreeWorkTbl[ESPGEN_APP_ID] = {
    Espgen00_SetFreeWork, Espgen01_SetFreeWork, Espgen02_SetFreeWork,
};
EspgenDestructFunc EspgenDestructTbl[ESPGEN_APP_ID] = {
    NULL,
};

// Active generators are moved when alive and not deleted; during an event only the ones flagged
// as event effects.
static inline int EspgenIsActive(EspgenWork* w)
{
    int on;

    if (!(w->flag & 1) || (w->flag & 2)) {
        on = 0;
    } else {
        // the flag load before `on = 1` keeps jump.c from hoisting `on = 0` above the first
        // test (the else arm must not start with a set of `on`)
        u32 f = pG->Status_flg[1];
        on = 1;
        if (f & 0x10000000) {
            on = w->info.Core_flg & 1;
        }
    }
    return on;
}

// Work size checks: the generator works must fit the 0xB4 bytes after the EspgenWork header.
#define ESPGEN_WORK_SIZE (sizeof(EspgenWork) - 0x14)

static inline void Espgen40_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "EspgenInit():Espgen40 Free size over!!(%d)", size);
    }
}

static inline void Espgen41_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "EspgenInit():Espgen41 Free size over!!(%d)", size);
    }
}

static inline void Espgen42_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "EspgenInit():Espgen42 Free size over!!(%d)", size);
    }
}

static inline void Espgen43_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "EspgenInit():Espgen43 Free size over!!(%d)", size);
    }
}

static inline void Espgen44_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "EspgenInit():Espgen44 Free size over!!(%d)", size);
    }
}

static inline void Espgen45_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "EspgenInit():Espgen45 Free size over!!(%d)", size);
    }
}

// Init-time check that the application controller works (40..45) fit the EspgenWork free area.
void EspgenFreeSizeCheckApp()
{
    Espgen40_FreeSizeCheck(0);
    Espgen41_FreeSizeCheck(0);
    Espgen42_FreeSizeCheck(0);
    Espgen43_FreeSizeCheck(0);
    Espgen44_FreeSizeCheck(0);
    Espgen45_FreeSizeCheck(0);
}

// Move entry of the unassigned controller ids: logs the invalid id.
static void EspgenDummyMove(EspgenWork* w)
{
    pLog->err(0, 0, "ESP_CTRL : CTRL_ID[%d] invalid.", w->id);
}

static inline void Espgen00_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "Eg00 size over(%d)", size);
    }
}

static inline void Espgen01_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "Eg01 size over(%d)", size);
    }
}

static inline void Espgen02_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "Eg02 size over(%d)", size);
    }
}

static inline void Espgen10_FreeSizeCheck(u32 size)
{
    if (size > ESPGEN_WORK_SIZE) {
        pLog->err(0, 0, "Eg10 size over(%d)", size);
    }
}

// Init-time check that the generic controller works (00/01/02/10) fit the EspgenWork free area.
void EspgenFreeSizeCheck()
{
    Espgen00_FreeSizeCheck(0);
    Espgen01_FreeSizeCheck(0);
    Espgen02_FreeSizeCheck(0);
    Espgen10_FreeSizeCheck(0);
}

// System init of the controller pool (same as the room init).
int EspgenInit()
{
    return EspgenRoomInit();
}

// Room init: forgets the pool pointer, resets the call counter, re-inits the water (EspWaterInit) and
// runs the work-size checks. The array itself is allocated separately by EspgenArrayAlloc.
int EspgenRoomInit()
{
    EspgenArray = NULL;
    nEspgen = 0;
    g_Call_no = 0;
    EspWaterInit();
    EspgenFreeSizeCheck();
    EspgenFreeSizeCheckApp();
    pEspgenArrayBack = NULL;
    return 1;
}

// Number of controller ids (0x46).
u32 GetEspgenIdMax()
{
    return ESPGEN_ID_MAX;
}

// Allocates the pool of n controllers (memory group 13, zeroed), freeing any previous one. Returns 0
// when n == 0 or the allocation failed.
int EspgenArrayAlloc(int n)
{
    u32 size;

    EspgenArrayFree();
    if (n == 0) {
        return 0;
    }
    size = n * sizeof(EspgenWork);
#line 403 "D:/Bio4/Prog/espgen.cpp"
    EspgenArray = (EspgenWork*) MEM_ALLOC(size, 1, 0xD);
    if (EspgenArray == NULL) {
        return 0;
    }
    nEspgen = n;
    memclr_asm(EspgenArray, size);
    return 1;
}

// Frees the controller pool; returns 0 when there was none.
int EspgenArrayFree()
{
    if (EspgenArray == NULL) {
        return 0;
    }
    Mem_free(EspgenArray);
    EspgenArray = NULL;
    return 1;
}

// Debug tools: swaps in a temporary pool of n controllers from the debug heap, saving the room pool.
int EspgenArrayPush(int n)
{
    if (pEspgenArrayBack != NULL) {
        return 0;
    }
    pEspgenArrayBack = EspgenArray;
    EspgenArray = (EspgenWork*) Debug_alloc(n * sizeof(EspgenWork), 1);
    nEspgenBack = nEspgen;
    nEspgen = n;
    return 1;
}

// Debug tools: frees the temporary pool and restores the saved room pool.
int EspgenArrayPop()
{
    if (pEspgenArrayBack == NULL) {
        return 0;
    }
    Debug_free(EspgenArray);
    EspgenArray = pEspgenArrayBack;
    pEspgenArrayBack = NULL;
    nEspgen = nEspgenBack;
    return 1;
}

// Takes a free controller searching from the BACK of the pool (moved/drawn last), clears it and
// marks it in use. On failure *out is the dummy controller and 0 is returned.
int PullEspgen(EspgenWork** out)
{
    EspgenWork* base = EspgenArray;
    EspgenWork* w;
    int ret = 0;
    u32 i;

    *out = &g_DmyEspgen;
    for (i = 0, w = &base[nEspgen - 1]; i < nEspgen; i++, w--) {
        if (!(w->flag & 1) || (w->flag & 2)) {
            memclr_asm(w, sizeof(EspgenWork));
            w->flag |= 1;
            *out = w;
            break;
        }
    }
    if (*out != &g_DmyEspgen) {
        ret = 1;
    }
    return ret;
}

// Releases a controller: sets the delete-request bit (flag bit 1; the slot is reused after the next
// EspgenMove) and runs the id's Destruct entry (frees water/sand buffers, resets filters).
void PushEspgen(EspgenWork* w)
{
    if ((w->flag & 1) && !(w->flag & 2)) {
        u32 max;

        w->flag |= 2;
        max = GetEspgenIdMax();
        if (w->id < max) {
            if (w->id < ESPGEN_APP_ID) {
                if (EspgenDestructTbl[w->id] != NULL) {
                    EspgenDestructTbl[w->id](w);
                }
            } else {
                if (EspgenDestructTblApp[w->id - ESPGEN_APP_ID] != NULL) {
                    EspgenDestructTblApp[w->id - ESPGEN_APP_ID](w);
                }
            }
        }
    }
}

// Like PullEspgen but searches from the FRONT of the pool (controllers that must move/draw first).
int PullEspgenFront(EspgenWork** out)
{
    EspgenWork* base = EspgenArray;
    EspgenWork* w;
    int ret = 0;
    u32 i;

    *out = &g_DmyEspgen;
    for (i = 0, w = base; i < nEspgen; i++, w++) {
        if (!(w->flag & 1) || (w->flag & 2)) {
            memclr_asm(w, sizeof(EspgenWork));
            w->flag |= 1;
            *out = w;
            break;
        }
    }
    if (*out != &g_DmyEspgen) {
        ret = 1;
    }
    return ret;
}

// Releases every controller (room change).
void EspgenArrayClear()
{
    EspgenWork* w;
    u32 i;

    for (i = 0, w = EspgenArray; i < nEspgen; i++, w++) {
        PushEspgen(w);
    }
}

// Per-frame move of all controllers: clears the slots deleted last frame, then runs the id's Move
// entry for every active one (during an event only those with Core_flg bit 0; while Status_flg[1]
// bit 1 pauses the game only those with Core_flg 0x8000). Prints the active count at (472,216).
int EspgenMove()
{
    EspgenWork* base = EspgenArray;
    EspgenWork* w;
    int pause = 0;
    u32 cnt;
    u32 i;
    u32 max;

    if (StaFlagChk(pG, STA_ITEM_GET)) {
        pause = 1;
    }
    cnt = 0;
    for (i = 0, w = base; i < nEspgen; i++, w++) {
        if (w->flag & 2) {
            w->flag &= ~3;
        } else if (EspgenIsActive(w)) {
            max = GetEspgenIdMax();
            if (w->id < max) {
                if (pause && !(w->info.Core_flg & 0x8000)) {
                    continue;
                }
                if (w->id < ESPGEN_APP_ID) {
                    EspgenMoveTbl[w->id](w);
                } else {
                    EspgenMoveTblApp[w->id - ESPGEN_APP_ID](w);
                }
                if (!(w->flag & 2)) {
                    cnt++;
                }
            } else {
                pLog->err(0, 0, "ESP_CTRL : CTRL_ID[%x] is invalid.", w->id);
            }
        }
    }
    if (DbgFlagChk(pG, DBG_EFF_NUM_DISP)) {
        eprintf(472, 216, 0, 0, "%d", cnt);
    } else {
        eprintf(472, 216, 0, 14, "%d", cnt);
    }
    return 1;
}

// Per-frame draw pass: runs the id's Trans entry (when any) for every active controller.
int EspgenTrans()
{
    EspgenWork* w;
    u32 i;
    u32 max;

    for (i = 0, w = EspgenArray; i < nEspgen; i++, w++) {
        EspgenTransFunc func;

        if (!EspgenIsActive(w)) {
            continue;
        }
        max = GetEspgenIdMax();
        if (w->id < max) {
            if (w->id < ESPGEN_APP_ID) {
                func = EspgenTransTbl[w->id];
            } else {
                func = EspgenTransTblApp[w->id - ESPGEN_APP_ID];
            }
            if (func != NULL) {
                func(w);
            }
        } else {
            pLog->err(0, 0, "ESP_CTRL : CTRL_ID[%x] is invalid.", w->id);
        }
    }
    return 1;
}

// Releases every controller whose owner info matches Core_flg == a, Core_kind == b, Core_pEm == c
// (each test skipped when 0): effects owned by a dying enemy/object.
void EspgenDelete(int a, int b, int c)
{
    EspgenWork* w;
    u32 i;

    for (i = 0, w = EspgenArray; i < nEspgen; i++, w++) {
        if ((w->flag & 1) && !(w->flag & 2)) {
            if (a != 0 && w->info.Core_flg != a) {
                continue;
            }
            if (b != 0 && w->info.Core_kind != b) {
                continue;
            }
            if (c != 0 && w->info.Core_pEm != c) {
                continue;
            }
            PushEspgen(w);
        }
    }
}

// Releases every controller that is neither permanent (Core_flg bit 0) nor event-owned (bit 0x800):
// event end.
void EspgenDeleteEvent()
{
    EspgenWork* w;
    u32 i;

    for (i = 0, w = EspgenArray; i < nEspgen; i++, w++) {
        if ((w->flag & 1) && !(w->flag & 2)) {
            if (!(w->info.Core_flg & 1) && !(w->info.Core_flg & 0x800)) {
                PushEspgen(w);
            }
        }
    }
}

// Debug display at (416,70): active / peak / pool size of the controllers.
int EspgenDispInfo()
{
    static int max = 0;
    EspgenWork* w = EspgenArray;
    int cnt;
    u32 i;

    if (w == NULL) {
        return 0;
    }
    cnt = 0;
    for (i = 0; i < nEspgen; i++, w++) {
        if ((w->flag & 1) && !(w->flag & 2)) {
            cnt++;
        }
    }
    if (cnt > max) {
        max = cnt;
    }
    eprintf(416, 70, 0, 12, "%3d/%3d/%4d", cnt, max, nEspgen);
    return 1;
}

// Current effect call number (g_Call_no, stamped into EspInfo::Call_no by est.cpp).
int EspgenGetCallNo()
{
    return g_Call_no;
}

// Advances the effect call number (one per EstSet call).
void EspgenIncCallNo()
{
    g_Call_no++;
}

// Calls func on every active controller (debug tools / bulk parameter updates).
int EspgenApplyFunc(void (*func)(EspgenWork* w))
{
    EspgenWork* w;
    u32 i;

    for (i = 0, w = EspgenArray; i < nEspgen; i++, w++) {
        if (EspgenIsActive(w)) {
            func(w);
        }
    }
    return 1;
}

// Runs the id's SetFreeWork entry (generic table with `flag`, application table without) to fill a
// freshly pulled controller from its record; returns its result (1 when the id has no entry).
int EspgenSetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                      Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag)
{
    int ret = 1;
    u32 max = GetEspgenIdMax();

    if (w->id < max) {
        if (w->id < ESPGEN_APP_ID) {
            if (EspgenSetFreeWorkTbl[w->id] != NULL) {
                ret = EspgenSetFreeWorkTbl[w->id](w, rec, head, model, parts, mtx, pos, rot, pSct, flag);
            }
        } else {
            if (EspgenSetFreeWorkTblApp[w->id - ESPGEN_APP_ID] != NULL) {
                ret = EspgenSetFreeWorkTblApp[w->id - ESPGEN_APP_ID](w, rec, head, model, parts, mtx, pos, rot, pSct);
            }
        }
    }
    return ret;
}

// Creates the controller described by record `no` (Kind 1) of a sequence: Espgen_id selects the
// type (0xFF is the special "set generator loop count" record, EspGenSetMoveLoop), the owner info is
// copied from `info`, then SetFreeWork fills it. Returns 0 (controller released) on a bad id, a full
// pool or a SetFreeWork failure.
int EspgenSeqSet(EspSeqData* head, int no, EspInfo* info, cModel* model, u16 parts, Mtx* mtx, Vec* pos, Vec* rot,
                 EspSeqOpt* pSct, int flag)
{
    EspGenWork* rec = &head->rec[no];
    EspgenWork* w;
    u32 max = GetEspgenIdMax();

    if (rec->Espgen_id >= max && rec->Espgen_id != 0xFF) {
        pLog->err(0, 0, "ESP_CTRL : CTRL_ID[%x] is invalid.", rec->Id);
        return 0;
    }
    if (rec->Espgen_id == 0xFF) {
        EspGenSetMoveLoop(rec->Espgen_work16[0]);
        return 1;
    }
    if (PullEspEspgen(&w, info->Core_flg, info->Core_kind, info->b.x7, info->Core_pEm, info->owner, 0)) {
        w->id = rec->Espgen_id;
        w->Type = rec->Espgen_type;
        if (!EspgenSetFreeWork(w, rec, head, model, parts, mtx, pos, rot, pSct, flag)) {
            PushEspgen(w);
            return 0;
        }
    } else {
        pLog->warn(6, 0, "ESP_CTRL : ESPGEN Pull failed!!");
        return 0;
    }
    return 1;
}

// the split object pads .rodata to 8 bytes
asm(".section .rodata; .balign 8");
