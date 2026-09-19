#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "esp.h"
#include "est.h"
#include "fade.h"
#include "sscrn.h"
#include "room_data.h"

// Room 3-16 (D:/Bio4/Prog/r316.cpp): the s00 event (Ashley taken away), the enemy waves after
// it, the heat effect in front of the furnace and the item that falls off the shelf.

struct R316Work {
    u8 dummy;
};

// The item object's per-object work (obj19 leaves it to the room): the fall speed at cObj+0x32C.
struct R316ItemView {
    u8 pad_0[0x32C];
    f32 fallSpd;
};

static R316Work* r316_work;

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup, r105 idiom).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");
// COMPILER-DIFF: #4 -- the original masks the u8 result of GetEmIdFromList before passing it on;
// ours treats the return as promoted. An int view of the callee plus the (u8) cast gives the clrlwi.
int GetEmIdFromListI(u32 no) asm("GetEmIdFromList");
// COMPILER-DIFF: 3 -- the varargs view of memset gives the `crclr; bl memset` of the `Vec = {0,0,0}`
// libcall for an explicit call (r213).
extern "C" void* r316_memset(void*, ...) asm("memset");
// COMPILER-DIFF: 4 -- `int` table entries reach setEm's s16 parameter unextended (`lwz r3`); ours
// narrows the load through the real prototype.
cEm* setEmI(int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__FsSciii");
// Wait for fade `no` to finish: the index stays a separate `addi` on the array base.
static inline void FadeWait(int no)
{
    while (Fade[no].flags & 1) {
        SceSleep(1);
    }
}

void r316_openShelf_main(int no, int mode);
static void r316_openedShelf(int no);
static void r316_openShelf(int no);
static void r316_checkFallItem();
static void r316_checkHeatEffect();
static void r316_exitDoorTo317();
static void r316_checkEmReset();
static void R316EventS00();
static void R316EventSXX();
extern "C" void Evt_R316S00_Func(Event* e);

// Room init: System_flg 0x400 off, Debug_flg[1] 0x00040000; JumpPoint 1 skips the event (Room_flg bits
// 0/1); Ashley no longer following. Until the event (bit 0) it runs at once (pre-loaded with the enemy of
// ESL 0); else, until the room was left through door 1 (bit 2), the wave refills run and door 1's exit
// hook clears the enemies. Door 0xD close-locked, the heat and falling-item watchers, the furnace
// objects, one shelf item event.
void R316Init()
{
    cEm* door;
    cEm* win;

    SysFlagOff(pG, SYS_SCREEN_STOP);
    DbgFlagOn(pG, DBG_CAST_ERR_NO_DISP);
#line 51 "D:/Bio4/Prog/r316.cpp"
    r316_work = (R316Work*) MEM_CALLOC(sizeof(R316Work), 1, 0xd);
    if (pG->JumpPoint == 1) {
        RsfSet(G_ROOM_ID, 0);
        RsfSet(G_ROOM_ID, 1);
    }
    StaFlagOff(pG, STA_SUB_ASHLEY);
    EvtMgr.SetFunc("evt_r316s00_func", (void*) Evt_R316S00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadAram("event/evd/r316s00.evd", (u8) GetEmIdFromListI(0), 0, 1, 0);
        SceExec(0x12, (TaskFunc) R316EventS00, 0, 2, 2, 0);
    } else if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceExec(0x12, (TaskFunc) r316_checkEmReset, 0, 0, 2, 0);
        SceAtSetDoorFunc(1, (TaskFunc) r316_exitDoorTo317, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) R316EventSXX, 0, 1);
        if (getRoomEtcDoor(0xD, &door, 1)) {
            cEmDoorSetCloseLock(door);
        }
    }
    SceExec(0x12, (TaskFunc) r316_checkHeatEffect, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r316_checkFallItem, 0, 0, 2, 0);
    if (getRoomEtcWindow(0x10, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x11, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x12, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x18, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x19, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x1A, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    SmdSetTrans(0x3D, 1);
    SmdSetTrans(0x3E, 0);
    SceSetItemEvent(7, 0x84, 3, 2, r316_openShelf, (void (*)()) r316_openedShelf, 0, 0);
}

// Per-frame room main: nothing.
void R316Main()
{
}

// Shelf 0 (object 0x3F, locker type 2) opens (mode 1: snap).
void r316_openShelf_main(int no, int mode)
{
    if (no == 0) {
        OpenBoxMain(2, mode, 0x1C, 0x3F, 0xFFFFFFFF, -1);
    }
}

// Item-event "already opened": the shelf posed open.
static void r316_openedShelf(int no)
{
    r316_openShelf_main(no, 1);
}

// Item-event opener: animate the shelf open.
static void r316_openShelf(int no)
{
    r316_openShelf_main(no, 0);
}

// The item on the shelf stays put until etc flag 6 (the shelf tipped over).
static void r316_checkFallItem()
{
    cModel* m;

    SceAtSetEnable(0x83, 1);
    m = SceAtItemModelPtr(0x83);
    if (m) {
        Vec pos = m->pos;

        while ((*GetEtcFlgPtr(6, G_ROOM_ID) & 1) == 0) {
            ((R316ItemView*) m)->fallSpd = 0.0f;
            m->setPos(&pos);
            SceSleep(1);
        }
    }
}

// Heat haze in front of the furnace while the player stands in area 6.
static void r316_checkHeatEffect()
{
    int kind = EspPullCoreKind();
    int on = 0;
    void* zero = 0;

    for (;;) {
        if (on == 0) {
            if (SceAtHitCheck(6) == 1) {
                EstSet(0, -1, 0, 0, 1, 0, 1, (u8) kind, (u32) zero, zero);
                on = 1;
                SceSleep(30);
            }
        } else {
            if (SceAtHitCheck(6) == 0) {
                EffectEspDelete(0, (u8) kind, 0, 0);
                EffectEspgenDelete(0, (u8) kind, 0);
                EffectEfmDelete(0, (u8) kind, 0);
                on = 0;
                EstSet(0, -1, 0, 0, 1, 1, 1, 0, 0, 0);
                SceSleep(30);
            }
        }
        SceSleep(1);
    }
}

// Door 1's exit hook (toward r317): Room_flg bit 2, every Ganado removed, ESL entries 3/8/9 marked alive again.
static void r316_exitDoorTo317()
{
    RsfSet(G_ROOM_ID, 2);
    SceDestroyEm(0x10, 0x20);
    EmListSetAlive(3, 1);
    EmListSetAlive(8, 1);
    EmListSetAlive(9, 1);
}

// Refills the Ganado waves from the side the player is on.
static void r316_checkEmReset()
{
    int a = 1;

    SceSleep(30);
    int b = 1;
    int tbl[4] = {0, 1, 2, 5};
    int tbl2[2] = {4, 6};
    u32 i = 0;
    int c = 0;

    while (1) {
        if (SceAtHitCheck(3) == 1) {
            a = 0;
            b = 1;
        }
        if (SceAtHitCheck(4) == 1) {
            a = 1;
            b = 0;
        }
        if (SceAtHitCheck(5) == 1) {
            a = 1;
            b = 1;
        }
        if ((u32) SceCountEmAlive(0x10, 0x20) <= 3) {
            if (a == 1 && i <= 1) {
                setEmI(tbl2[i], -1, 1, 1, 1);
                i++;
            }
            if (b == 1 && c == 0) {
                setEm(7, -1, 1, 1, 1);
                c = 1;
            }
            if (i > 1 && c != 0) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Once (Room_flg bit 0): System_flg 0x400, event r316s00 (Ashley is taken away by Saddler's men),
// chapter 5-2 ends (SceSetChapterEnd(CHAPTER_5_2)), the wave refills start, fade-in and the inventory opens.
static void R316EventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadExec("event/evd/r316s00.evd", (u8) GetEmIdFromListI(0), 0);
        SceSetChapterEnd(0xF, -1);
        SceExec(0x12, (TaskFunc) r316_checkEmReset, 0, 0, 2, 0);
        FadeSetW(1, 0, 0, 0);
        SubScreenOpen(2, 0);
    }
}

// Leaving through door 2: fade out and jump to r30a.
static void R316EventSXX()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        SceEventStart(0);
        pPL->setNoSuspend(1);
        FadeSetW(2, 60, 0, 0);
        FadeWait(2);
        SceEventEnd(0);
        ScfFlagOn(pG, SCF_R316_TO_R30A_CUTBACK_EVENT);
        {
            Vec pos;
            Vec rot;

            r316_memset(&pos, 0, sizeof(Vec));
            r316_memset(&rot, 0, sizeof(Vec));
            SceAtExecRoomJump(0x30A, &pos, &rot, 0);
        }
        SysFlagOn(pG, SYS_SCREEN_STOP);
    }
    // COMPILER-DIFF: candidate (gcse table size). Five dead insns (folded by cse2, no code) grow the
    // PRE hash table from 45 to 47/49 buckets, which numbers the hoisted `&pos` (fp+0x10, hash
    // 13305) before `&rot` (fp+0x20, 13321) and gives them the target's r29/r28.
    {
        int z = 0;
        do { } while (0);
        if (z > 128) {
            z = z * 77 + 1;
        }
    }
}

// Event r316s00 callback: swaps the door objects 0x3D/0x3E (closed / open) on cuts 0 and 5/0xB, sets the
// event models' flags per cut; the end restores the room.
void Evt_R316S00_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        break;
    case 1: {
        void* mod;

        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x3D, 1);
                SmdSetTrans(0x3E, 0);
            }
            break;
        case 5:
        case 0xB:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x3D, 0);
                SmdSetTrans(0x3E, 1);
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
        case 1:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 0);
            }
            break;
        default:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 1);
            }
            break;
        }
        switch (e->NowCut) {
        case 0xB:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 0);
            }
            break;
        case 0xA:
        case 0xC:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 1);
            }
            break;
        case 0x12:
            if (e->NowFrame == 0) {
                SmdSetTrans(6, 0);
            }
            break;
        case 0x11:
        case 0x13:
            if (e->NowFrame == 0) {
                SmdSetTrans(6, 1);
            }
            break;
        }
        break;
    }
    case 2:
        SmdSetTrans(0x3D, 1);
        SmdSetTrans(0x3E, 0);
        SmdSetTrans(1, 1);
        SmdSetTrans(6, 1);
        break;
    }
}
