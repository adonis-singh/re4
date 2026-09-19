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
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emdoor.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "mes.h"
#include "item.h"
#include "sscrn.h"

// Room 3-06 (D:/Bio4/Prog/r306.cpp): the double door, the two locked doors (308 / 30B keys), the two
// Ganado that follow the player in from 30B and the item boxes.

struct R306Work {
    cEm* door1;    // 0x00
    cEm* door2;    // 0x04
    cEmWrap em[2]; // 0x08
};

static R306Work* r306_work;

static void r306_DuraluminCaseOpen(int no);
static void r306_DuraluminCaseOpened(int no);
static void r306_TanaOpen(int no);
static void r306_TanaOpened(int no);
static void r306_StrCheck();
static void r306_checkDoor308KeyUse();
static void r306_checkDoor308();
static void r306_checkDoor30bKeyUse();
static void r306_checkDoor30b();

// Puts enemy `no` of the previous room at its list position `l` (the angle always goes to the first one).
// The list pointer and the angle are the caller's variables (one pseudo across both calls), the Vecs the
// inline's own (one frame slot shared by both copies, the addresses never PRE'd).
static inline void r306_setEmPos(int no, EmListData* l, f32& ry)
{
    Vec pos;
    Vec ang;

    pos.x = (f32) l->pos[0] * 10.0f;
    pos.y = (f32) l->pos[1] * 10.0f;
    pos.z = (f32) l->pos[2] * 10.0f;
    ry = (f32) (l->rot[1] * 360 / 32768);
    r306_work->em[no].setPos(&pos);
    ang.x = 0.0f;
    ang.y = ry;
    ang.z = 0.0f;
    r306_work->em[0].setAng(&ang);
}

// Room init: doors 0x14/0x15 paired; area 4 = the locked 308 door until Key_flg[0] 0x2000 (else
// area 6 off) with its key-use watcher; area 5 = the 30B door until 0x100; the room effect by item flag
// 0x20. After Scenario_flg[1] 0x1000 the two Ganados 0x30/0x31 (list 6) are set, placed at their r30B
// list positions when the player came from there, the battle BGM table and stream watcher; else the
// plain stream. Case / shelf item events.
void R306Init()
{
#line 48 "D:/Bio4/Prog/r306.cpp"
    r306_work = (R306Work*) MEM_CALLOC(sizeof(R306Work), 1, 0xd);
    if (getRoomEtcDoor(0x14, &r306_work->door1, 1) && getRoomEtcDoor(0x15, &r306_work->door2, 1)) {
        ((cEmDoor*) r306_work->door1)->setDoor((cEmDoor*) r306_work->door2);
    }
    if (!(pG->Key_flg[0] & 0x2000)) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) r306_checkDoor308, 0, 1);
    } else {
        SceAtSetEnable(6, 0);
    }
    SceExec(0x12, (TaskFunc) r306_checkDoor308KeyUse, 0, 0, 2, 0);
    void* zero = 0;
    if (!(pG->Key_flg[0] & 0x100)) {
        SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r306_checkDoor30b, 0, 1);
        SceExec(0x12, (TaskFunc) r306_checkDoor30bKeyUse, 0, 0, 2, 0);
    }
    if (ItfFlagChk(pG, ITF_R308_THERMO_RIFLE)) {
        EstSet(0, -1, 0, 0, 1, 1, 1, 0, (u32) zero, zero);
    } else {
        EstSet(0, -1, 0, 0, 1, 0, 1, 0, (u32) zero, zero);
    }
    if (ScfFlagChk(pG, SCF_R307_REGENERATER_APPEAR)) {
        r306_work->em[0].setEm(0x30, 6, 0, 1, 1);
        r306_work->em[1].setEm(0x31, 6, 0, 1, 1);
        if (pG->room_id_prev == 0x30B) {
            EmListData* l;
            f32 ry;

            l = EM_LIST(0x2E);
            r306_setEmPos(0, l, ry);
            l = EM_LIST(0x2F);
            r306_setEmPos(1, l, ry);
        }
        SndBgmTblSet(0x306, 1);
        SceExec(0x12, (TaskFunc) r306_StrCheck, 0, 0, 2, 0);
    } else {
        SndRoomStrStart(1, 0, 1);
    }
    SceSetItemEvent(0xB, 0x80, 0, 9, r306_DuraluminCaseOpen, (void (*)()) r306_DuraluminCaseOpened, 0x17, 0);
    SceSetItemEvent(0xC, 0x84, 1, 8, r306_TanaOpen, (void (*)()) r306_TanaOpened, 0x19, 0);
    SceSetItemEvent(0xD, 0x83, 2, 7, r306_TanaOpen, (void (*)()) r306_TanaOpened, 0x1B, 0);
}

// Item-event opener: the duralumin case (type 7) opens.
static void r306_DuraluminCaseOpen(int no)
{
    OpenBoxMain(7, 0, 0x18, no, -1, -1);
}

// Item-event "already opened": the case posed open.
static void r306_DuraluminCaseOpened(int no)
{
    OpenBoxMain(7, 1, -1, no, -1, -1);
}

// Item-event opener: shelf `no` (object pair 0x19/0x1A or 0x1B/0x1C) swings open.
static void r306_TanaOpen(int no)
{
    if (no == 0x19) {
        OpenBoxMain(0, 0, 3, 0x19, 0x1A, -1);
    } else {
        OpenBoxMain(0, 0, 3, 0x1B, 0x1C, -1);
    }
}

// Item-event "already opened": shelf `no` posed open.
static void r306_TanaOpened(int no)
{
    if (no == 0x19) {
        OpenBoxMain(0, 1, -1, 0x19, 0x1A, -1);
    } else {
        OpenBoxMain(0, 1, -1, 0x1B, 0x1C, -1);
    }
}

// Per-frame room main: nothing.
void R306Main()
{
}

// The stream runs while the player is seen.
static void r306_StrCheck()
{
    int on = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}

// The 308 door: the key message while the player has the key. The message work pointer of the unlock
// branch is a local computed before the loop; the other branch's cMes.getWork() is hoisted as its copy.
static void r306_checkDoor308KeyUse()
{
    MesWork* w = &cMes.mes[0];

    for (;;) {
        while (ItemMgr.check(0x84) != 1) {
            SceSleep(1);
        }
        if (pG->Room_flg[2] & 0x80000000) {
            SndCall(6, 4, 0, 0, 0, 0);
            SceMesSet(4, 0, 1, 0x64, 0x150 - w->lineSpace - w->m_font_h - 1);
            pG->Key_flg[0] |= 0x2000;
            SceAtDataReset(4);
            SceAtSetEnable(6, 0);
        } else {
            SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        }
    }
}

// Area 4: the 308 door is locked.
static void r306_checkDoor308()
{
    SndCall(6, 5, 0, 0, 0, 0);
    SceUpCut(3, 5, -1, 0);
    if (ItemMgr.num(0x84) != 0) {
        SubScreenOpen(0x80, 1);
    }
}

// The 30B door: the key is used up once the player has it.
static void r306_checkDoor30bKeyUse()
{
    while (ItemMgr.check(0x92) != 1) {
        SceSleep(1);
    }
    ItemMgr.dump(0x92);
    SndCall(6, 4, 0, 0, 0, 0);
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    pG->Key_flg[0] |= 0x100;
    SceAtDataReset(5);
}

// Area 5: the 30B door is locked.
static void r306_checkDoor30b()
{
    SndCall(6, 5, 0, 0, 0, 0);
    SceUpCut(0, 6, -1, 0);
    if (ItemMgr.num(0x84) != 0 || ItemMgr.num(0x92) != 0) {
        SubScreenOpen(0x80, 1);
    }
}
