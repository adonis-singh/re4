#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "pl_sub.h"
#include "snd.h"
#include "fade.h"

// Room 3-31 (D:/Bio4/Prog/r331.cpp): the s00 event (into the mine cart) and the s10 event (the count-down
// start) with their room jumps.

struct R331Work {
    int timer;   // 0x0  count-down frames when the s10 event started
};

static R331Work* r331_work;

// st3.cpp's count-down helpers
void st3_setCountDownTimer(int frame);
int st3_getCountDownTimer();
void st3_startCountDown();
void st3_checkCountDown();

static void R331ExecEventS00();
static void R331ExecEventS10();
extern "C" void Evt_R331S00_Func(Event* e);
extern "C" void Evt_R331S10_Func(Event* e);

// 1 while the event is being skipped (EVT status bit 30).
static inline int r331_evtSkip(Event* e)
{
    int skip = 1;

    if ((e->StatusFlag & 0x40000000) == 0) {
        skip = 0;
    }
    return skip;
}

// Room init: the s00 / s10 callbacks; until Room_flg bit 0 area 3 = the s00 event (pre-loaded). With
// Scenario_flg[1] 0x200 (the count-down phase): BGM table 0x331 set 2 and the s10 event task, the
// count-down resumed; else BGM table 3 enabled.
void R331Init()
{
#line 58 "D:/Bio4/Prog/r331.cpp"
    r331_work = (R331Work*) MEM_CALLOC(sizeof(R331Work), 1, 0xd);
    EvtMgr.SetFunc("evt_r331s00_func", (void*) Evt_R331S00_Func);
    EvtMgr.SetFunc("evt_r331s10_func", (void*) Evt_R331S10_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) R331ExecEventS00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r331s00.evd", 0, 0, 0, 0);
    }
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE)) {
        SndBgmTblSetDisable(3, 0);
        SndBgmTblSet(0x331, 2);
        SceExec(0x12, (TaskFunc) R331ExecEventS10, 0, 2, 2, 0);
        st3_startCountDown();
    } else {
        SndBgmTblSetEnable(3, 0);
    }
}

// Per frame: the island count-down check (time over -> the death demo).
void R331Main()
{
    st3_checkCountDown();
}

// Area 3 once (Room_flg bit 0): event r331s00 (Leon and Ashley board the mine cart): Ashley marked as
// separated (Status_flg[3] bit 31, following off), then a room jump to r332 at the cart start.
static void R331ExecEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(3, 0);
        SceEventStart(0);
        EvtMgr.EvtReadExec("event/evd/r331s00.evd", 0, 0);
        SceEventEnd(0);
        StaFlagOn(pG, STA_SAVEDATA_NO_UPDATE);
        SubCharCtrl(2, 0);
        StaFlagOff(pG, STA_SUB_ASHLEY);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        {
            Vec pos = {-42000.0f, 15800.0f, 46900.0f};
            Vec rot = {0.0f, 0.0f, 0.0f};

            SceAtExecRoomJump(0x332, &pos, &rot, 0);
        }
    }
}

// Once (Room_flg bit 1) in the count-down phase: event r331s10, Ashley following again, BGM table
// 0x331 set 1 with both BGMs started, then a room jump to r333.
static void R331ExecEventS10()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SceSleep(1);
        SceEventStart(0);
        EvtMgr.EvtReadExec("event/evd/r331s10.evd", 0, 0);
        SceEventEnd(0);
        StaFlagOn(pG, STA_SUB_ASHLEY);
        SndBgmTblSet(0x331, 1);
        SndRoomBgmStart(0, 0);
        SndRoomBgmStart(1, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        {
            Vec pos = {-646166.0f, -12718.0f, -588290.0f};
            Vec rot = {0.0f, -1.78f, 0.0f};

            SceAtExecRoomJump(0x333, &pos, &rot, 0);
        }
    }
}

// Event r331s00 callback: cut 0 hands scroll object 0x24 (scr0000) to the event; cut 4 fades out unless
// the event is skipped; the end restores the object.
extern "C" void Evt_R331S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if ((obj = SmdGetObjPtr(0x24)) != 0) {
                    e->SetMod("scr0000", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
            }
            break;
        case 4:
            if (e->NowFrame == e->MaxFrame - 40) {
                int skip = r331_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 40, 0, 0);
                }
            }
            break;
        }
        break;
    case 2:
        w = SmdGetWorkPtr(0x24);
        if ((obj = SmdGetObjPtr(0x24)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        SysFlagOn(pG, SYS_SCREEN_STOP);
        break;
    }
}

// Event r331s10 callback: remembers the count-down at the start; cut 0 hands scroll object 0x24 to the
// event with a fade-in, cut 2 fades out; the end restores the object and restarts the count-down with
// the event's length subtracted.
extern "C" void Evt_R331S10_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->funcMode) {
    case 0:
        r331_work->timer = st3_getCountDownTimer();
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                int skip;

                if ((obj = SmdGetObjPtr(0x24)) != 0) {
                    e->SetMod("scr0000", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
                skip = r331_evtSkip(e);
                if (skip == 0) {
                    FadeSetW(0x80000002, 40, 0, 0);
                }
            }
            break;
        case 2:
            if (e->NowFrame == e->MaxFrame - 40) {
                int skip = r331_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 40, 0, 0);
                }
            }
            break;
        }
        break;
    case 2:
        w = SmdGetWorkPtr(0x24);
        if ((obj = SmdGetObjPtr(0x24)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        st3_setCountDownTimer(r331_work->timer - e->MaxTotalFrame);
        st3_startCountDown();
        SysFlagOn(pG, SYS_SCREEN_STOP);
        break;
    }
}
