#include "types.h"
#include "main_mem.h"
#include "st_room.h"
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
#include "obj.h"
#include "em.h"
#include "em_wrap.h"
#include "cam_ctrl.h"
#include "item.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"

// Room 2-11 (D:/Bio4/Prog/r211.cpp): the castle room with the two statues that take the cup items
// (0x6E / 0x6F): each statue can be inspected (areas 4 / 5) and a task waits for its cup to be used;
// once both are placed the grate (smd 0x1B) rises (r211_GrateOpen, Room_flg bit 2) and opens the way.
// Two shelf item events. Room_flg bits 0 / 1 remember the placed cups.

struct R211Work {
    u32 se;   // 0x0  RoomSeCall handle of the grate
};

static R211Work* r211_work;

Vec r211_cup_pos0 = {12229.0f, 1469.0f, -20709.0f};
static Vec r211_cup_pos1 = {6244.0f, 1403.0f, -20920.0f};
Vec r211_cup_rot = {0.0f, 0.0f, 0.0f};

static void r211_DoorMessage();
static void r211_InspectStatue(int no);
static void r211_CheckUseCup(int no);
void r211_GrateOpen();
static void r211_GrateOpenEndProc();
static void r211_ShelfOpen(int no);
static void r211_ShelfOpened(int no);

// Room init: each statue cup already placed (Room_flg bit 0 / 1) is shown as a scroll model with its area
// (4 / 5) off and the Room_flg[0] bit 31 / 30 set; otherwise the statue can be inspected and a task waits
// for the cup item (0x6E / 0x6F) to be used; the grate is open when bit 2; two shelf item events.
void R211Init()
{
#line 42 "D:/Bio4/Prog/r211.cpp"
    r211_work = (R211Work*) MEM_CALLOC(sizeof(R211Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 0)) {
        cObj* obj;

        pG->Room_flg[0] |= 0x80000000;
        SceAtSetEnable(4, 0);
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &r211_cup_pos0, &r211_cup_rot, 0x10, 1);
        if (obj) {
            obj->be_flag |= 0x4000;
            obj->setNoSuspend(1);
        }
    } else {
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r211_InspectStatue, 0, 1);
        SceExec(0x12, (TaskFunc) r211_CheckUseCup, 0, 0, SCE_PRIO_DEF_2, 0);
        if (ItemMgr.num(0x6E)) {
            pG->Room_flg[0] |= 0x80000000;
        }
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        cObj* obj;

        pG->Room_flg[0] |= 0x40000000;
        SceAtSetEnable(5, 0);
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &r211_cup_pos1, &r211_cup_rot, 0x10, 1);
        if (obj) {
            obj->be_flag |= 0x4000;
            obj->setNoSuspend(1);
        }
    } else {
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r211_InspectStatue, (void*) 1, 1);
        SceExec(0x12, (TaskFunc) r211_CheckUseCup, 1, 0, SCE_PRIO_DEF_2, 0);
        if (ItemMgr.num(0x6F)) {
            pG->Room_flg[0] |= 0x40000000;
        }
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        SmdGetObjPtr(0x1B)->be_flag &= ~2;
    } else {
        SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r211_DoorMessage, 0, 1);
    }
    if (FlagChkSign(pG->Room_flg, 0) && FlagChkSign(pG->Room_flg, 1)) {
        setEm(0xAA, -1, 0, 1, 0);
        setEm(0xAB, -1, 0, 1, 0);
        setEm(0xAC, -1, 0, 1, 0);
        setEm(0xAD, -1, 0, 1, 0);
        setEm(0xAE, -1, 0, 1, 0);
        setEm(0xAF, -1, 0, 1, 0);
    }
    SceSetItemEvent(7, 0x80, 3, 6, r211_ShelfOpen, (void (*)()) r211_ShelfOpened, 0, 0);
    SceSetItemEvent(8, 0x84, 4, 5, r211_ShelfOpen, (void (*)()) r211_ShelfOpened, 1, 0);
}

// Per-frame room main: nothing.
void R211Main()
{
}

// The grate door while closed: up-cut 3 with message 2.
static void r211_DoorMessage()
{
    SceUpCut(3, -1, 2, 0);
}

// Area 4 / 5 (statue `no`): the look message (up-cut 8 / 7); with the matching cup item held the item
// screen opens so it can be used.
static void r211_InspectStatue(int no)
{
    switch (no) {
    case 0:
        SceUpCut(0, 8, -1, 0);
        if (ItemMgr.num(0x6E)) {
            SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        }
        break;
    case 1:
        SceUpCut(0, 7, -1, 0);
        if (ItemMgr.num(0x6F)) {
            SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        }
        break;
    }
}

// Task per statue: waits until cup item 0x6E / 0x6F is used, places the cup model (Room_flg bit 0 / 1,
// message), and when both cups are placed opens the grate after half a second.
static void r211_CheckUseCup(int no)
{
    u16 item = no != 0 ? 0x6F : 0x6E;
    cObj* obj;

    while (ItemMgr.check(item) == 0) {
        SceSleep(1);
    }
    switch (no) {
    case 0:
        SceAtSetEnable(4, 0);
        RsfSet(G_ROOM_ID, 0);
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &r211_cup_pos0, &r211_cup_rot, 0x10, 1);
        obj->be_flag |= 0x4000;
        obj->setNoSuspend(1);
        SceUpCut(1, 0xA, 7, 0);
        break;
    case 1:
        SceAtSetEnable(5, 0);
        RsfSet(G_ROOM_ID, 1);
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &r211_cup_pos1, &r211_cup_rot, 0x10, 1);
        obj->be_flag |= 0x4000;
        obj->setNoSuspend(1);
        SceUpCut(2, 9, 7, 0);
        break;
    }
    if (RsfCheck(G_ROOM_ID, 0) && RsfCheck(G_ROOM_ID, 1)) {
        SceSleep(30);
        r211_GrateOpen();
    }
}

// The grate (smd 0x1B) rises 30 units a frame to y = 4600 under camera cut 4 with its SE and dust
// effect; player-cancellable via r211_GrateOpenEndProc.
void r211_GrateOpen()
{
    cObj* obj = SmdGetObjPtr(0x1B);

    SceEventStart(0);
    CamCtrl.CutCall(4);
    r211_work->se = RoomSeCall(0, &obj->pos, 0, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0, 1, 2, 0, 0);
    pG->Room_flg[0] &= ~0x00200000;
    SceSetEventCancel(1, (TaskFunc) r211_GrateOpenEndProc, 0, 0xA, 1);
    while (obj->pos.y < 4600.0f) {
        obj->pos.y += 30.0f;
        obj->matUpdate();
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    RoomSeCall(1, &obj->pos, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r211_GrateOpenEndProc();
}

// End of the grate opening: the grate object stays shown, SE / effect cleanup when cancelled early,
// camera back, Room_flg bit 2 and Scenario_flg[3] 0x10 (the way is open), collision area 0 re-armed.
static void r211_GrateOpenEndProc()
{
    cObj* obj = SmdGetObjPtr(0x1B);

    BitOff(obj->be_flag, 2);
    if (pG->Room_flg[0] & 0x00200000) {
        RoomSeCall(1, &obj->pos, 0, 0, 0);
        EffectEspDelete(1, 2, 0, 0);
        EffectEspgenDelete(1, 2, 0);
        EffectEfmDelete(1, 2, 0);
    }
    CamCtrl.Comeback(0);
    RsfSet(G_ROOM_ID, 2);
    ScfFlagOn(pG, SCF_7b);
    SceAtDataReset(0);
    SceEventEnd(0);
}

// Item-event opener: shelf `no` (OpenBoxMain type 0x1A) swings open.
static void r211_ShelfOpen(int no)
{
    switch (no) {
    case 0:
        OpenBoxMain(0, 0, 0x1A, 0x7A, 0x79, -1);
        break;
    case 1:
        OpenBoxMain(0, 0, 0x1A, 0x7B, 0x7C, -1);
        break;
    }
}

// Item-event "already opened": pose shelf `no` open.
static void r211_ShelfOpened(int no)
{
    switch (no) {
    case 0:
        OpenBoxMain(0, 1, 0x1A, 0x7A, 0x79, -1);
        break;
    case 1:
        OpenBoxMain(0, 1, 0x1A, 0x7B, 0x7C, -1);
        break;
    }
}

// The next unit's .data starts 8-aligned.
asm(".section .data; .balign 8");
