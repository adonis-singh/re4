#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
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
#include "esp.h"
#include "read.h"
#include "datactrl.h"
#include "fade.h"
#include "player.h"
#include "sscrn.h"
#include "snd.h"
#include "cam_ctrl.h"
#include "em_wrap.h"

// Room 1-02 (D:/Bio4/Prog/r102.cpp, in st1_1 and st1_3): the farm; the well cover event, the
// r102s00 event and the battle BGM.

struct R102Work {
    cDataUnit* evd;   // 0x00  the event data file
};

static R102Work* r102_work;

static void r102_execEvent00();
void r102_checkBgm();
static void r102_openCover();

// Room init: rain without water splashes; the well cover event on area 3 unless Room_flg bit 0 says it
// was already opened (then scroll object 0x25's lid is posed open); the r102s00 event on area 5 unless
// Room_flg bit 1 (its evd file is pre-loaded and enemy 0x18 read ahead); then the BGM task.
void R102Init()
{
#line 39 "D:/Bio4/Prog/r102.cpp"
    r102_work = (R102Work*) MEM_CALLOC(sizeof(R102Work), 1, 0xd);

    Espgen42SetNoWater(1);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r102_openCover, 0, 2);
    } else {
        SmdGetObjPtr(0x25)->pList->ang.z = -2.46091f;
        SmdGetObjPtr(0x25)->be_flag |= 0x20;
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r102_execEvent00, 0, 1);
        EmReadSearch(0x18, 0, 0);
        r102_work->evd = DC.setData(EvtMgr.NameChange("evd/r102s00.evd"));
        r102_work->evd->setCommand(CMND_MRAM_LOAD, 0, 0);
    }
    SceExec(0x12, (TaskFunc) r102_checkBgm, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per-frame room main: nothing.
void R102Main()
{
}

// Event r102s00: play the event, then put the player at the well and open the sub screen.
static void r102_execEvent00()
{
    RsfSet(G_ROOM_ID, 1);
    SceEventStart(0);
    if (r102_work->evd->waitUseOk() == 1) {
        EvtMgr.SetEvt(r102_work->evd->m_addr, 0);
        while (EvtMgr.IsAliveEvt(EvtMgr.GetNowExeEvtNamePtr(), 0, 0) != 0) {
            SceSleep(1);
        }
    }
    SceEventEnd(0);
    FadeSetW(1, 0, 0, 0);
    {
        Vec pos = {-10960.7f, -8769.21f, 38593.6f};
        Vec ang;
        f32 ry = -1.67305f;
        cPlayer* pl = pPL;
        Vec* pa = &ang;

        pl->setPos(&pos);
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(&ang);
    }
    r102_work->evd->setCommand(CMND_DEL_DATA, 0, 0);
    SubScreenOpen(SS_OPEN_SHOP, 0);
    setEm(0x4C, -1, 1, 1, 1);
}

// Room BGM: lowered while the player stands in area 4.
void r102_checkBgm()
{
    int on;

    SceSleep(1);
    if (pG->room_id_prev == 0x101 || pG->room_id_prev == 0x111) {
        on = 0;
        SndRoomBgmStart(0, 1);
    } else {
        on = 1;
        SndRoomBgmStart(0, 0);
    }
    SceSleep(1);
    for (;;) {
        if (SceAtHitCheck(4) == 1) {
            if (on == 1) {
                on = 0;
                SndRoomBgmVolSet(0, 1, 600);
            }
        } else {
            if (on == 0) {
                on = 1;
                SndRoomBgmVolReset(0, 600);
            }
        }
        SceSleep(1);
    }
}

// Well cover: camera cut 4 while the cover swings open.
static void r102_openCover()
{
    const f32 step = -0.09424778f;
    int i = 25;
    cObj* obj;

    RsfSet(G_ROOM_ID, 0);
    SceEventStart(0);
    CamCtrl.CutCall(4);
    SndCall(6, 3, 0, 0, 0, 0);
    obj = SmdGetObjPtr(0x25);
    obj->be_flag |= 0x20;
    obj->pList->ang.z += -0.05235988f;
    SceSleep(1);
    obj->pList->ang.z += -0.05235988f;
    SceSleep(1);
    SceSleep(1);
    do {
        obj->pList->ang.z += step;
        SceSleep(1);
    } while (--i != 0);
    obj->pList->ang.z -= -0.02094395f;
    SceSleep(1);
    obj->pList->ang.z += -0.02094395f;
    SceSleep(10);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}
