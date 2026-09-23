#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "em.h"
#include "etc_model.h"
#include "emdoor.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "esp.h"
#include "room_data.h"
#include "snd.h"

// local copy: a header definition changes other units' allocation (its 0.0f pool labels, see model.h)
static inline void SetAngY(cModel* m, f32 y) { Vec v; v.x = 0.0f; v.y = y; v.z = 0.0f; m->setAng(&v); }

// Room 3-15 (D:/Bio4/Prog/r315.cpp): the first-entry cut with Leon and Ashley, the duralumin
// case and the two sliding shelves.

struct R315Work {
    u8 dummy;
};

static R315Work* r315_work;

// The previous room id read as a raw halfword (not a struct member): the load then depends on the
// work pointer store before it, which keeps the pG load below that store.
#define PREV_ROOM_ID (*(u16*) &pG->room_id_prev)




static void plemRide(cPlayer* pl);
static void funcAshley(cEm* p);
static void first_in();
static void r315_DuraluminCaseOpen(int no);
static void r315_DuraluminCaseOpened(int no);
static void r315_TanaOpen(int no);
static void r315_TanaOpened(int no);

// Room init: a fresh entry marks Ashley as following and initialises her at Leon; the first-entry cut
// once (Room_flg bit 0); the duralumin case and two shelf item events; door 6 loses light select bit 4.
void R315Init()
{
    cEmDoor* door;

#line 45 "D:/Bio4/Prog/r315.cpp"
    r315_work = (R315Work*) MEM_CALLOC(sizeof(R315Work), 1, 0xd);
    if (PREV_ROOM_ID == 0xFFF) {
        if (StaFlagChk(pG, STA_SUB_ASHLEY) == 0) {
            StaFlagOn(pG, STA_SUB_ASHLEY);
            SubCharInit(1, &pPL->pos, pPL->ang.y);
            SubCharCtrl(1, 0);
        }
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceExec(0x12, (TaskFunc) first_in, 0, 0, 2, 0);
    }
    SceSetItemEvent(4, 0x83, 1, 4, r315_DuraluminCaseOpen, r315_DuraluminCaseOpened, 0x14, 0);
    SceSetItemEvent(3, 0x82, 3, 3, r315_TanaOpen, r315_TanaOpened, 0x1F, 0);
    SceSetItemEvent(2, 0x84, 2, 2, r315_TanaOpen, r315_TanaOpened, 0x22, 0);
    if (getRoomEtcDoor(6, &door, 1)) {
        door->LightInfo.SelectMask &= ~4;
    }
}

// Per-frame room main: nothing.
void R315Main()
{
}

// Leon's ride motion (SetPlDamage routine).
static void plemRide(cPlayer* pl)
{
    switch (pl->r_no_2) {
    case 0:
        pPL->setNoSuspend(1);
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0x201, 0);
        pl->r_no_2++;
        pl->r_no_3 = 0;
    case 1:
        pl->r_no_3++;
        if (MotionMove(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
}

// Ashley's motion during the cut (SetSubAux routine).
static void funcAshley(cEm* p)
{
    if (p->r_no_2 == 0) {
        cAtariInfo* at = &pSUB->atari;

        at->off();
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
        p->r_no_2 = 1;
    }
    if (p->motionMove()) {
        cAtariInfo* at;

        p->setRno(0, 0, 0, 0);
        at = &pSUB->atari;
        at->on();
        SubCharCtrl(1, 0);
    }
}

// First entry: place Leon and Ashley, play the cut.
static void first_in()
{
    pPL->setPos(8600.0f, 57.0f, -2276.0f);
    SetAngY(pPL, -1.67f);
    SceSleep(1);
    if (pSUB) {
        pSUB->setPos(7073.0f, 57.0f, -3109.0f);
        SetAngY(pSUB, -0.742f);
        SetSubAux(funcAshley, 0);
    }
    SetPlDamage(0, plemRide);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, 0, 0);
    SndStrReq(1, 0xEA, 0x80000003, 0, 0, 0.0f);
    pPL->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    SceEventStart(1);
    CamCtrl.CutCall(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
}

// Item-event opener: the duralumin case (type 7) opens.
static void r315_DuraluminCaseOpen(int no)
{
    OpenBoxMain(7, 0, 0x18, no, 0xFFFFFFFF, -1);
}

// Item-event "already opened": the case posed open.
static void r315_DuraluminCaseOpened(int no)
{
    OpenBoxMain(7, 1, -1, no, 0xFFFFFFFF, -1);
}

// The shelf slides 800 units aside (the 0x1F one the other way).
static void r315_TanaOpen(int no)
{
    f32 step;
    u32 i;

    SmdGetObjPtr(no)->be_flag |= 0x20;
    SndCall(6, 0x1B, 0, 0, 0, 0);
    step = 26.666666f;
    if (no == 0x1F) {
        step *= -1.0f;
    }
    for (i = 0; i < 30; i++) {
        SmdGetObjPtr(no)->pos.x -= step;
        SceSleep(1);
    }
}

// Item-event "already opened": the shelf `no` posed slid aside (800 units, the 0x1F one the other way).
static void r315_TanaOpened(int no)
{
    f32 step;
    u32 i;

    SmdGetObjPtr(no)->be_flag |= 0x20;
    step = 26.666666f;
    if (no == 0x1F) {
        step *= -1.0f;
    }
    for (i = 0; i < 30; i++) {
        SmdGetObjPtr(no)->pos.x += step;
    }
}
