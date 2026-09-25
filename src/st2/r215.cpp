#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "act_btn.h"
#include "shadow.h"
#include "fade.h"

// Room 2-15 (D:/Bio4/Prog/r215.cpp): an event-only room: on the first visit R215_Event plays r215s00
// (a QTE; success clears Room_flg[0] bit 31), then r215s01 (passed: Leon placed at the exit) or the
// r215s02 death event. The Evt_R215Sxx_Func callbacks set the event models' light masks per cut.

struct R215Work {
    u8 dummy;
};

static R215Work* r215_work;

extern "C" void R215_Event();
static void r215_succeedAction();
extern "C" void Evt_R215S00_Func(Event* e);
extern "C" void Evt_R215S01_Func(Event* e);
extern "C" void Evt_R215S02_Func(Event* e);


// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");


// Light kind mask / display flag of an event model.
#define R215_EVT_MOD_LIGHT(name, kind)                  \
    if (e->GetMod(&mod, name, 0, 0) == 1) {             \
        ((cModel*) mod)->LightInfo.EnableMask = kind;          \
    }
#define R215_EVT_MOD_FLAG(name)                         \
    if (e->GetMod(&mod, name, 0, 0) == 1) {             \
        ((cModel*) mod)->be_flag |= 0x80;               \
    }

// Room init: JumpPoint 1 marks the event seen (Room_flg bit 0); registers the three callbacks and, on
// the first visit, pre-loads r215s00 to MRAM and starts the event task.
void R215Init()
{
#line 44 "D:/Bio4/Prog/r215.cpp"
    r215_work = (R215Work*) MEM_CALLOC(sizeof(R215Work), 1, 0xd);
    if (pG->JumpPoint == 1) {
        RsfSet(G_ROOM_ID, 0);
    }
    EvtMgr.SetFunc("evt_r215s00_func", (void*) Evt_R215S00_Func);
    EvtMgr.SetFunc("evt_r215s01_func", (void*) Evt_R215S01_Func);
    EvtMgr.SetFunc("evt_r215s02_func", (void*) Evt_R215S02_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadMram("event/evd/r215s00.evd", 0, 0, 0, 0);
        SceExec(0x12, (TaskFunc) R215_Event, 0, 0, SCE_PRIO_DEF_2, 0);
    }
}

// Per-frame room main: nothing.
void R215Main()
{
}

// Once (Room_flg bit 0): plays r215s00 (its QTE clears Room_flg[0] bit 31 on success); passed -> r215s01
// and Leon is placed at the exit facing +X; failed -> the r215s02 death event and the task sleeps forever.
extern "C" void R215_Event()
{
    SceEventStart(0);
    SceSleep(1);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        pG->Room_flg[0] |= 0x80000000;
        EvtMgr.EvtReadExec("event/evd/r215s00.evd", 0, EvtReadFlagNone);
        if (!(pG->Room_flg[0] & 0x80000000)) {
            EvtMgr.EvtReadExec("event/evd/r215s01.evd", 0, EvtReadFlagNone);
            {
                Vec pos = {39050.0f, 3000.0f, 200.0f};
                Vec ang;
                Vec* pa = &ang;
                f32 ry = 1.5707964f;
                cPlayer* p = pPL;

                p->setPos(&pos);
                ang.x = 0.0f;
                pa->y = ry;
                ang.z = 0.0f;
                p->setAng(pa);
            }
        } else {
            EvtMgr.EvtReadExec("event/evd/r215s02.evd", 0, EvtReadFlagDiedemo);
            for (;;) {
                SceSleep(1);
            }
        }
        SceEventEnd(0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SceAtExecute(2);
    }
}

// QTE success callback: clears Room_flg[0] bit 31 (R215_Event reads it after s00).
static void r215_succeedAction()
{
    pG->Room_flg[0] &= ~0x80000000;
}

// Event r215s00 callback: status 3 with cancel cut 9; shadow camera size zeroed on cut 3; cut 0 assigns
// light masks to the pl0100 / em3700 / evm* event models and be_flag 0x80 to some; cut 0xA and the end
// modes 2/3 handle the QTE window and cleanup.
extern "C" void Evt_R215S00_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 9;
        break;
    case 1:
        switch (e->NowCut) {
        case 3:
            if (e->NowFrame == 0) {
                SetShadowCamMoveSize(0.0f);
            }
            break;
        case 2:
        case 4:
            if (e->NowFrame == 0) {
                ResetShadowCamMoveSize();
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                R215_EVT_MOD_LIGHT("pl0100", 1)
                R215_EVT_MOD_LIGHT("em3700", 2)
                R215_EVT_MOD_LIGHT("evm7400", 4)
                R215_EVT_MOD_LIGHT("evm7410", 4)
                R215_EVT_MOD_LIGHT("evm5500", 8)
                R215_EVT_MOD_LIGHT("evm5510", 8)
                R215_EVT_MOD_LIGHT("evm5700", 8)
                R215_EVT_MOD_LIGHT("evm4900", 8)
                R215_EVT_MOD_LIGHT("evm5100", 0x20)
                R215_EVT_MOD_LIGHT("evm5200", 0x40)
                R215_EVT_MOD_FLAG("evm5500")
                R215_EVT_MOD_FLAG("evm5510")
                R215_EVT_MOD_FLAG("evm5600")
            }
            break;
        case 0xA:
            SpfFlagOff(pG, SPF_ACTBTN);
            DpfFlagOff(pG, DPF_MESSAGE);
            if (pG->Room_flg[0] & 0x80000000) {
                // The button object and the callback are evaluated before the stack argument store.
                cActionButton* ab = &ActBtn;
                void* func = (void*) r215_succeedAction;

                ab->set(0x30, 5, func, 0, 0x42, 4, 0, 0);
            } else {
                e->CancelSet();
            }
            break;
        }
        break;
    case 2:
        ResetShadowCamMoveSize();
        break;
    case 3:
        if (e->FlgCkStatus(EvtStfEvtCancelSet) == 0) {
            EvtMgr.EvtSndStrPlay(EvtMgr.GetNowExeEvtNamePtr(), 1, 0x89, 1, 0.0f);
        }
        break;
    }
}

// Event r215s01 callback: shadow camera size zeroed on cut 0x10; scroll objects 0x2C/0x2E hidden during
// cut 0x11; per-cut light masks / flags on the event models.
extern "C" void Evt_R215S01_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        break;
    case 1:
        if (e->NowCut == 0x10) {
            if (e->NowFrame == 0) {
                SetShadowCamMoveSize(0.0f);
            }
        } else {
            if (e->NowFrame == 0) {
                ResetShadowCamMoveSize();
            }
        }
        if (e->NowCut == 0x11) {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x2C, 0);
                SmdSetTrans(0x2E, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x2C, 1);
                SmdSetTrans(0x2E, 1);
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                R215_EVT_MOD_LIGHT("pl0100", 1)
                R215_EVT_MOD_LIGHT("em3700", 2)
                R215_EVT_MOD_LIGHT("evm7400", 4)
                R215_EVT_MOD_LIGHT("evm7410", 4)
                R215_EVT_MOD_LIGHT("evm2500a", 4)
                R215_EVT_MOD_LIGHT("evm3800", 4)
                R215_EVT_MOD_LIGHT("evm4000", 4)
                R215_EVT_MOD_LIGHT("evm5500", 8)
                R215_EVT_MOD_LIGHT("evm5510", 8)
                R215_EVT_MOD_LIGHT("evm5700", 8)
                R215_EVT_MOD_LIGHT("evm5800", 8)
                R215_EVT_MOD_LIGHT("evm5900", 8)
                R215_EVT_MOD_LIGHT("evm5100", 0x20)
                R215_EVT_MOD_LIGHT("evm5200", 0x40)
                R215_EVT_MOD_FLAG("evm5500")
                R215_EVT_MOD_FLAG("evm5510")
            }
            break;
        case 0x12:
            if (e->NowFrame == 0x5A) {
                if (e->FlgCkStatus(EvtStfToolFrontExec) == 0) {
                    FadeSetW(2, 30, 0, 0);
                }
            }
            break;
        }
        break;
    case 2:
        SmdSetTrans(0x2C, 1);
        SmdSetTrans(0x2E, 1);
        ResetShadowCamMoveSize();
        break;
    }
}

// Event r215s02 callback (the failure event): nothing to do.
extern "C" void Evt_R215S02_Func(Event* e)
{
}
