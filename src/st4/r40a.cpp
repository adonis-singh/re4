#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "em.h"
#include "em_wrap.h"
#include "read.h"
#include "player.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"

// Room 4-0A (D:/Bio4/Prog/r40a.cpp): the storage room; the duralumin case, the two cabinets and
// the Ganado that show up once the key items were taken.

struct R40aWork {
    u8 dummy;
};

static R40aWork* r40a_work;

static void r40a_DuraluminCaseOpen(int id);
static void r40a_DuraluminCaseOpened(int id);
static void r40a_TanaOpen(int id);
static void r40a_TanaOpened(int id);
static void em_set();
static void em_set2();
static void first_init();

// Item-event opener: the duralumin case (lid up +X) opens.
static void r40a_DuraluminCaseOpen(int id)
{
    OpenBoxMain(OpenBoxPartsUpXP, 0, 0x18, id, 0xFFFFFFFF, -1);
}

// Item-event "already opened": the case posed open.
static void r40a_DuraluminCaseOpened(int id)
{
    OpenBoxMain(OpenBoxPartsUpXP, 1, -1, id, 0xFFFFFFFF, -1);
}

// Item-event opener: cabinet `id` (object pair 0x19/0x1A or 0x1B/0x1C) swings open.
static void r40a_TanaOpen(int id)
{
    if (id == 0x19) {
        OpenBoxMain(0, 0, 3, 0x19, 0x1A, -1);
    } else {
        OpenBoxMain(0, 0, 3, 0x1B, 0x1C, -1);
    }
}

// Item-event "already opened": cabinet `id` posed open.
static void r40a_TanaOpened(int id)
{
    if (id == 0x19) {
        OpenBoxMain(0, 1, -1, 0x19, 0x1A, -1);
    } else {
        OpenBoxMain(0, 1, -1, 0x1B, 0x1C, -1);
    }
}

// Room init (Assignment Ada, the storage room): Debug_flg[1] 0x00020000, enemy 0x1F pre-read; area 5 =
// the Ganado group and the item-taken watcher; coming from r407 Ganado 0x65 waits alerted, coming from
// r406 the first-visit setup; the case and two cabinet item events.
void R40aInit()
{
#line 74 "D:/Bio4/Prog/r40a.cpp"
    r40a_work = (R40aWork*) MEM_CALLOC(sizeof(R40aWork), 1, 0xd);
    DbgFlagOn(pG, DBG_EMW_ERR_NO_DISP);
    EmReadSearch(0x1F, 0, 0);
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) em_set, 0, 1);
    SceExec(0x12, (TaskFunc) em_set2, 0, 0, SCE_PRIO_DEF_2, 0);
    if (pG->room_id_prev == 0x407) {
        cEmWrap em;

        em.setEm(0x65, -1, 1, 1, 1);
        em.setFlag(1);
    }
    if (pG->room_id_prev == 0x406) {
        SceExec(0x12, (TaskFunc) first_init, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    SceSetItemEvent(8, 0x80, 1, 9, r40a_DuraluminCaseOpen, (void (*)()) r40a_DuraluminCaseOpened, 0x17, 0);
    SceSetItemEvent(9, 0x87, 2, 8, r40a_TanaOpen, (void (*)()) r40a_TanaOpened, 0x19, 0);
    SceSetItemEvent(0xA, 0x83, 3, 7, r40a_TanaOpen, (void (*)()) r40a_TanaOpened, 0x1B, 0);
}

// Per-frame room main: nothing.
void R40aMain()
{
}

// Area 5: the Ganado come once both key items were taken.
static void em_set()
{
    int n = 0;

    if (ItfFlagChk(pG, ITF_R40B_SAMPLE00)) {
        n = 1;
    }
    if (ItfFlagChk(pG, ITF_R40C_SAMPLE00)) {
        n++;
    }
    if (n == 0) {
        return;
    }
    if (n == 1) {
        return;
    }
    setEm(0x6C, -1, 1, 1, 1);
    setEm(0x6D, -1, 1, 1, 1);
    setEm(0x6E, -1, 1, 1, 1);
    setEm(0x6F, -1, 1, 1, 1);
    setEm(0x70, -1, 1, 1, 1);
    setEm(0x71, -1, 1, 1, 1);
}

// The Ganado come once the item of area 0x88 was taken.
static void em_set2()
{
    int cnt = 0;

    do {
        if (SceAtItemFlgCk(0x88)) {
            cnt++;
        }
        SceSleep(1);
    } while (cnt == 0);
    setEm(0x6C, -1, 1, 1, 1);
    setEm(0x6D, -1, 1, 1, 1);
    setEm(0x6E, -1, 1, 1, 1);
    setEm(0x6F, -1, 1, 1, 1);
    setEm(0x70, -1, 1, 1, 1);
    setEm(0x71, -1, 1, 1, 1);
}

// Coming from 4-06: Leon climbs down through the hatch.
// OPEN (4 words): the setPos block's x/z constant temps swap f0/f13. Our sched1 issues the three
// pool loads in LUID order (x, y, addi, z), so z has the shortest live range and local-alloc gives
// it f0 first; the original has x in f0 (allocated first or outside local-alloc). All 6 statement
// orders, f32/const f32 locals, pointer and inline-helper forms keep z first; only an
// `asm("" : "+f"(px))` launder on the x temp (2 deaths -> global alloc -> f0) reproduces the bytes.
static void first_init()
{
    Vec v;
    f32 y;
    int frame;

    SceEventStart(0);
    SceSleep(1);
    y = pPL->pos.y;
    pPL->setNoSuspend(1);
    cModel* m = pPL;
    v.x = 773.0f;
    v.y = 4010.0f;
    v.z = 939.0f;
    m->setPos(&v);
    v.y = 1.59f;
    v.x = 0.0f;
    v.z = 0.0f;
    pPL->setAng(&v);
    EstSet((int) pPL, -1, 0, 0, 1, 4, 1, 0, 0, 0);
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0xF, 0x201, 0);
    frame = (u32) MotionGetMaxFrame(&pPL->Motion);
    SceSleep(30);
    SndCall(5, 0x14, &pPL->pos, 0, 0, 0);
    SceSleep(frame - 30);
    pPL->setNoSuspend(0);
    pPL->pos.y = y;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}
