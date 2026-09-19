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
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emwindow.h"
#include "etc_model.h"
#include "player.h"
#include "esp.h"
#include "snd.h"
#include "fade.h"
#include "sscrn.h"
#include "TexRender.h"

// Room 3-04 (D:/Bio4/Prog/r304.cpp): the s00 event with its two render-to-texture cameras (the monitors),
// the Ganado that appear after it and the two item boxes.

struct R304Work {
    TexRenderMng* tex;    // 0x000
    TexRenderMng* tex2;   // 0x004
    u8 texTbl[0x80];      // 0x008
    u8 texTbl2[0x80];     // 0x088
    TexRenderCam cam;     // 0x108
    TexRenderCam cam2;    // 0x40C
    cEmWrap em;           // 0x710
};

static R304Work* r304_work;

// The original reads r4 although its prototype has one parameter (r11b).
void TexRenderModResP(cModel* m, int parts) asm("TexRenderModRes");
// The u8 result is passed on unmasked (COMPILER-DIFF 4).
int GetEmIdFromListI(u32 no) asm("GetEmIdFromList");

static void r304_EnemySet();
void R304EventS00();
extern "C" void Evt_R304S00_Func(Event* e);
void EvtTexRenderCamTrans(Event* e, int cut);
static void r304_DuraluminCaseOpen(int no);
static void r304_DuraluminCaseOpened(int no);
static void r304_LockerOpen(int no);
static void r304_LockerOpened(int no);

// Room init: the s00 callback; until seen (Room_flg bit 0) area 2 = the event (pre-loaded with the enemy
// of ESL 0x28), the room effect 0 and the windows 0x19..0x1F unbreakable; else the attribute sound off and
// effect 1. Area 3 = the two Ganados until bit 1; case / locker item events; the two monitor render targets.
void R304Init()
{
    cEm* win;
    int i;
    void* zero = 0;

#line 55 "D:/Bio4/Prog/r304.cpp"
    r304_work = (R304Work*) MEM_CALLOC(sizeof(R304Work), 1, 0xd);
    EvtMgr.SetFunc("evt_r304s00_func", (void*) Evt_R304S00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) R304EventS00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r304s00.evd", (u8) GetEmIdFromListI(0x28), 0, 0, 0);
        EstSet(0, -1, 0, 0, 1, 0, 0, 0, (u32) zero, zero);
        for (i = 0x19; i <= 0x1F; i++) {
            if (getRoomEtcWindow(i, &win, 1)) {
                ((cEmWindow*) win)->SetEnableDamage(0);
            }
        }
    } else {
        SeAtSetOnOff(0, 0);
        EstSet(0, -1, 0, 0, 1, 1, 0, 0, (u32) zero, zero);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r304_EnemySet, 0, 1);
    }
    if (pG->room_id_prev == 0x305) {
        if (r304_work->em.setEm(0x25, 6, 0, 1, 1) == 1) {
            cEmWrap* em = &r304_work->em;
            Vec v;
            f32 ry;

            v.x = -13750.0f;
            v.y = 0.0f;
            v.z = 4860.0f;
            ry = 1.59f;
            em->setPos(&v);
            v.x = 0.0f;
            v.y = ry;
            v.z = 0.0f;
            em->setAng(&v);
        }
    }
    SceSetItemEvent(4, 0x81, 2, 2, r304_DuraluminCaseOpen, (void (*)()) r304_DuraluminCaseOpened, 0x17, 0);
    SceSetItemEvent(5, 0x82, 3, 1, r304_LockerOpen, (void (*)()) r304_LockerOpened, 0x20, 0);
    TexRenderInit(&r304_work->tex, 0, 1);
    TexRenderInit(&r304_work->tex2, 0, 1);
}

// Per-frame room main: nothing.
void R304Main()
{
}

// Area 3: the two Ganado run at the player.
static void r304_EnemySet()
{
    cEmWrap em;

    SceAtSetEnable(3, 0);
    RsfSet(G_ROOM_ID, 1);
    if (em.setEm(0x4D, -1, 1, 1, 1)) {
        em.setGoto(&pPL->pos, 0xD);
    }
    if (em.setEm(0x4E, -1, 1, 1, 1)) {
        em.setGoto(&pPL->pos, 0xD);
    }
}

// Area 2: the s00 event.
void R304EventS00()
{
    cEm* win;
    int i;

    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(2, 0);
        SeAtSetOnOff(0, 0);
        SndRoomStrStop(1);
        EstSet(0, -1, 0, 0, 1, 1, 0x800, 0, 0, 0);
        EvtMgr.EvtReadExec("event/evd/r304s00.evd", (u8) GetEmIdFromListI(0x28), 0x200);
        FadeSetW(1, 0, 0, 0);
        SubScreenOpen(2, 1);
        SndRoomStrStart(1, 0, 1);
        for (i = 0x19; i <= 0x1F; i++) {
            if (getRoomEtcWindow(i, &win, 1)) {
                ((cEmWindow*) win)->SetEnableDamage(1);
            }
        }
    }
}

// Event r304s00: the three scroll models join the event, the monitors render the event cameras.
extern "C" void Evt_R304S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec ang = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->funcMode) {
    case 0:
        StaFlagOn(pG, STA_CAMERA_SET_ROOM);
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            obj = SmdGetObjPtr(0x10);
            if (obj) {
                e->SetMod("scr0000", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&ang);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            obj = SmdGetObjPtr(0x12);
            if (obj) {
                e->SetMod("scr0100", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&ang);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            obj = SmdGetObjPtr(0x13);
            if (obj) {
                e->SetMod("scr0200", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&ang);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
        }
        if (e->NowCut == 1) {
            if (e->NowFrame == 0) {
                obj = SmdGetObjPtr(0xA);
                if (obj) {
                    TexRenderModSet(obj, 0, r304_work->texTbl, r304_work->tex, 1, 1, 1, 1, 1.0f);
                }
                obj = SmdGetObjPtr(0xB);
                if (obj) {
                    TexRenderModSet(obj, 0, r304_work->texTbl2, r304_work->tex2, 1, 1, 1, 1, 1.0f);
                }
            }
            EvtTexRenderCamTrans(e, 1);
        } else {
            if (e->NowFrame == 0) {
                obj = SmdGetObjPtr(0xA);
                if (obj) {
                    TexRenderModResP(obj, 0);
                    ModelInfoSetTrans(obj, 0, 1);
                }
                obj = SmdGetObjPtr(0xB);
                if (obj) {
                    TexRenderModResP(obj, 0);
                    ModelInfoSetTrans(obj, 0, 1);
                }
            }
        }
        break;
    case 2:
        StaFlagOff(pG, STA_CAMERA_SET_ROOM);
        w = SmdGetWorkPtr(0x10);
        obj = SmdGetObjPtr(0x10);
        if (obj && w) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x11);
        obj = SmdGetObjPtr(0x11);
        if (obj && w) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x11);
        obj = SmdGetObjPtr(0x11);
        if (obj && w) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        break;
    }
}

// Render-to-texture pass of the event cameras (cuts 0 / 1 / 10): the two monitors.
void EvtTexRenderCamTrans(Event* e, int cut)
{
    void* mod;
    void* bin;
    int skip = 1;

    if ((e->StatusFlag & 0x40000000) == 0) {
        skip = 0;
    }
    if (skip == 0) {
        if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
            TexRenderModAddOt(0, (cModel*) mod);
            TexRenderModAddOt(1, (cModel*) mod);
        }
        if (e->GetMod(&mod, "em1e00", 0, 0) == 1) {
            TexRenderModAddOt(0, (cModel*) mod);
            TexRenderModAddOt(1, (cModel*) mod);
        }
        mod = SmdGetObjPtr(5);
        if (mod) {
            TexRenderModAddOt(0, (cModel*) mod);
            TexRenderModAddOt(1, (cModel*) mod);
        }
        switch (cut) {
        case 0:
            if (EvtMgr.GetBin(&bin, "event/r304/s00/cam/etc_s00_000.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r304_work->cam, (TexRenderEvt*) e, bin);
            }
            if (EvtMgr.GetBin(&bin, "event/r304/s00/cam/et2_s00_000.fcv", 0) == 1) {
                TexRenderCamAddOt(1, &r304_work->cam2, (TexRenderEvt*) e, bin);
            }
            break;
        case 1:
            if (EvtMgr.GetBin(&bin, "event/r304/s00/cam/etc_s00_001.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r304_work->cam, (TexRenderEvt*) e, bin);
            }
            if (EvtMgr.GetBin(&bin, "event/r304/s00/cam/et2_s00_001.fcv", 0) == 1) {
                TexRenderCamAddOt(1, &r304_work->cam2, (TexRenderEvt*) e, bin);
            }
            break;
        case 10:
            if (EvtMgr.GetBin(&bin, "event/r304/s00/cam/etc_s00_010.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r304_work->cam, (TexRenderEvt*) e, bin);
            }
            if (EvtMgr.GetBin(&bin, "event/r304/s00/cam/et2_s00_010.fcv", 0) == 1) {
                TexRenderCamAddOt(1, &r304_work->cam2, (TexRenderEvt*) e, bin);
            }
            break;
        }
    }
}

// Item-event opener: the duralumin case (type 7) opens.
static void r304_DuraluminCaseOpen(int no)
{
    OpenBoxMain(7, 0, 0x18, no, -1, -1);
}

// Item-event "already opened": the case posed open.
static void r304_DuraluminCaseOpened(int no)
{
    OpenBoxMain(7, 1, 0x18, no, -1, -1);
}

// Item-event opener: the locker (type 2) opens.
static void r304_LockerOpen(int no)
{
    OpenBoxMain(2, 0, 0x1C, no, -1, -1);
}

// Item-event "already opened": the locker posed open.
static void r304_LockerOpened(int no)
{
    OpenBoxMain(2, 1, 0x1C, no, -1, -1);
}
