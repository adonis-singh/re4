#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emwindow.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "cam_ctrl.h"

// Room 4-0B (D:/Bio4/Prog/r40b.cpp): an Assignment Ada room: the item on the pedestal (area 1 shows
// it with camera cut 6, then item area 0x81) and the Ganados behind the bars that appear when the
// player reaches area 2 (Room_flg bit 0).

struct R40bWork {
    u8 dummy;
};

static R40bWork* r40b_work;

static void r40b_getItem();
static void r40b_checkEmSet1();

// Room init: windows 0xA/0xB without fences, object 6 shown; the Ganados behind the bars until Room_flg
// bit 0; the pedestal item (area 0x81, kept updating) with its camera show on area 1 until Item_flg[0] 8.
void R40bInit()
{
    cEm* win;

#line 37 "D:/Bio4/Prog/r40b.cpp"
    r40b_work = (R40bWork*) MEM_CALLOC(sizeof(R40bWork), 1, 0xd);
    if (getRoomEtcWindow(0xA, &win, 1)) {
        ((cEmWindow*) win)->SetEnableFence(0, 0);
    }
    if (getRoomEtcWindow(0xB, &win, 1)) {
        ((cEmWindow*) win)->SetEnableFence(0, 0);
    }
    {
        cObj* obj = SmdGetObjPtr(6);

        if (obj) {
            obj->be_flag &= ~2;
        }
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceExec(0x12, (TaskFunc) r40b_checkEmSet1, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (!ItfFlagChk(pG, ITF_R40B_SAMPLE00)) {
        cModel* m;

        SceAtSetEnable(0x81, 1);
        m = SceAtItemModelPtr(0x81);
        if (m) {
            m->setNoSuspend(1);
        }
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r40b_getItem, 0, 1);
    }
}

// Per-frame room main: nothing.
void R40bMain()
{
}

// Area 1: the camera shows the item (cut 6), then the item area 0x81 runs.
static void r40b_getItem()
{
    cModel* m;

    SceEventStart(0);
    LightMgr.onKind(0x7F);
    m = SceAtItemModelPtr(0x81);
    if (m) {
        m->setNoSuspend(1);
    }
    CamCtrl.CutCall(6);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceAtExecute(0x81);
    SceSleep(1);
    if (SceAtItemFlgCk(0x81) == 1) {
        SceAtSetEnable(1, 0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The Ganado behind the bars appear when the player reaches area 2.
static void r40b_checkEmSet1()
{
    cEm* barred;

    getRoomEtcBarred(0x32, &barred, 1);
    while (SceAtHitCheck(2) != 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 0);
    setEm(0x7A, -1, 1, 1, 1);
    setEm(0x7B, -1, 1, 1, 1);
    setEm(0x7C, -1, 1, 1, 1);
}
