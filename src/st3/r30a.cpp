#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "snd.h"
#include "fade.h"
#include "cam_ctrl.h"
#include "route_ck.h"
#include "cSceObj.h"

// Room 3-0A (D:/Bio4/Prog/r30a.cpp): the lift between the two floors (the player and the partner ride it),
// the s00 escape event that jumps to 3-16 and the s10 event.

// The original's .rodata is 8-aligned (a pad word after the header strings and at the end).
asm(".section .rodata; .balign 8");

struct R30aWork {
    cSceObj elv;   // 0x00
};

static R30aWork* r30a_work;

// game/EtcModel.cpp's static (the room build had it global): break-object display on/off.
int setRoomEtcBreakDisp(int no, int on, int flag) asm("setRoomEtcBreakDisp");

static void r30a_setElvCamera(u32 mode);
static void r30a_moveElevator(u32 dir);
void r30a_initElevator();
static void r30a_execEvent10();
static void R30aEventS00();
extern "C" void Evt_R30AS00_Func(Event* e);

// Room init: the s00 (and s98) callback. In the escape phase (Scenario_flg[1] 0x800, consumed here):
// BGM table 3 off and the s00 escape event once (Room_flg bit 0). Otherwise area 3 = the s10 event once
// (bit 1, pre-loaded to MRAM) and the lift.
void R30aInit()
{
#line 54 "D:/Bio4/Prog/r30a.cpp"
    r30a_work = (R30aWork*) MEM_CALLOC(sizeof(R30aWork), 1, 0xd);
    EvtMgr.SetFunc("evt_r30as00_func", (void*) Evt_R30AS00_Func);
    EvtMgr.SetFunc("evt_r30as98_func", (void*) Evt_R30AS00_Func);
    if (ScfFlagChk(pG, SCF_R316_TO_R30A_CUTBACK_EVENT)) {
        SndBgmTblSetDisable(3, 0);
        ScfFlagOff(pG, SCF_R316_TO_R30A_CUTBACK_EVENT);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceExec(0x12, (TaskFunc) R30aEventS00, 0, 0, 2, 0);
            EvtMgr.EvtReadAram("event/evd/r30as00.evd", 0, 0, 0, 0);
        }
    } else {
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r30a_execEvent10, 0, 1);
            EvtMgr.EvtReadMram("event/evd/r30as10.evd", 0, 0, 0, 0);
        }
        r30a_initElevator();
    }
}

// Per-frame room main: nothing.
void R30aMain()
{
}

// The camera cuts of the ride (mode 0 down, 1 up).
static void r30a_setElvCamera(u32 mode)
{
    pG->Room_flg[0] &= ~0x80000000;
    if (mode == 0) {
        CamCtrl.CutCall(4);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    } else {
        CamCtrl.CutCall(6);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(7);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    pG->Room_flg[0] |= 0x80000000;
}

// Areas 1 / 2: the ride (dir 0 down, 1 up); the partner rides along when close enough.
static void r30a_moveElevator(u32 dir)
{
    cObj* obj = SmdGetObjPtr(0xA);
    u32 n;

    SceEventStart(0);
    pG->Room_flg[0] &= ~0x80000000;
    if (dir == 0) {
        r30a_work->elv.setReverse(0);
        SceAtSetEnable(5, 1);
        SceAtSetEnable(6, 0);
    } else {
        r30a_work->elv.setReverse(1);
        SceAtSetEnable(5, 0);
        SceAtSetEnable(6, 1);
    }
    if (obj) {
        obj->pModelInfo->flagsDC |= 1;
        if (dir == 0) {
            obj->pModelInfo->uvScrollU = 0.05f;
        } else {
            obj->pModelInfo->uvScrollU = -0.05f;
        }
    }
    SceExec(0x12, (TaskFunc) r30a_setElvCamera, dir, 0, 2, 0);
    {
        cPlayer* pl = pPL;
        cSceObj* elv = &r30a_work->elv;

        if (pl) {
            for (n = 0; n < 4; n++) {
                if (elv->sub[n] == NULL) {
                    elv->sub[n] = pl;
                    break;
                }
            }
        }
    }
    pPL->setNoSuspend(1);
    if (pSUB) {
        if (RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) <= 3000.0f) {
            Vec pos;

            if (dir == 0) {
                cSubChar* sub = pSUB;

                pos.x = 37950.0f;
                pos.y = -3971.0f;
                pos.z = 27640.0f;
                sub->setPos(&pos);
            } else {
                cSubChar* sub = pSUB;

                pos.x = 37950.0f;
                pos.y = 7741.0f;
                pos.z = 27640.0f;
                sub->setPos(&pos);
            }
            {
                cSubChar* pl = pSUB;
                cSceObj* elv = &r30a_work->elv;

                if (pl) {
                    for (n = 0; n < 4; n++) {
                        if (elv->sub[n] == NULL) {
                            elv->sub[n] = pl;
                            break;
                        }
                    }
                }
            }
            pSUB->setNoSuspend(1);
        }
    }
    SndCall(6, 0, 0, 0, 0, 0);
    do {
        if (r30a_work->elv.move() == 0) {
            if (pG->Room_flg[0] & 0x80000000) {
                break;
            }
        }
        SceSleep(1);
    } while (1);
    SndCall(6, 1, 0, 0, 0, 0);
    if (obj) {
        obj->pModelInfo->uvScrollU = 0.0f;
    }
    pPL->setNoSuspend(0);
    {
        cPlayer* pl = pPL;
        cSceObj* elv = &r30a_work->elv;

        if (pl) {
            for (n = 0; n < 4; n++) {
                if (elv->sub[n] == pl) {
                    elv->sub[n] = NULL;
                    break;
                }
            }
        }
    }
    if (pSUB) {
        pSUB->setNoSuspend(0);
        {
            cSubChar* pl = pSUB;
            cSceObj* elv = &r30a_work->elv;

            if (pl) {
                for (n = 0; n < 4; n++) {
                    if (elv->sub[n] == pl) {
                        elv->sub[n] = NULL;
                        break;
                    }
                }
            }
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The lift: areas 1/2 = ride down / up, area 5 off, 6 on; object 9 gets a 210-frame move1 of 11656 up
// with 20 % accel / decel and a shake.
void r30a_initElevator()
{
    SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r30a_moveElevator, 0, 1);
    SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) r30a_moveElevator, (void*) 1, 1);
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 1);
    cObj* obj = SmdGetObjPtr(9);
    Vec d = {0.0f, 11656.0f, 0.0f};
    r30a_work->elv.initMove1_pos(obj, 210, &d, 20.0f, 20.0f);
    r30a_work->elv.setVibration(8, 8, 2.0f, 0.5f, 2.0f);
}

// Area 3: the s10 event.
static void r30a_execEvent10()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadExec("event/evd/r30as10.evd", 0, 0x100);
    }
}

// The s00 event, then the jump to 3-16.
static void R30aEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadExec("event/evd/r30as00.evd", 0, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        Vec pos = {12717.0f, 2000.0f, 7351.0f};
        Vec rot = {0.0f, 1.466f, 0.0f};
        SceAtExecRoomJump(0x316, &pos, &rot, 0);
    }
}

// Events r30as00 / r30as98: the light follows the player model, the fades and the hidden enemy part.
extern "C" void Evt_R30AS00_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        setRoomEtcBreakDisp(0, 0, 1);
        StaFlagOn(pG, STA_CAMERA_SET_ROOM);
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
        case 2:
        case 4:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    cLight* light = LightMgr.getKindLight(1);

                    if (light) {
                        light->setParent((cModel*) mod);
                    }
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod;

                int skip = 1;

                if ((e->StatusFlag & 0x40000000) == 0) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(0x80000002, 60, 0, 0);
                }
                if (e->GetMod(&mod, "em3900", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 7, 0);
                }
            }
            break;
        case 7:
            if (e->NowFrame == 50) {
                int skip = 1;

                if ((e->StatusFlag & 0x40000000) == 0) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(2, 30, 0, 0);
                }
            }
            break;
        }
        break;
    case 2:
        setRoomEtcBreakDisp(0, 1, 1);
        StaFlagOff(pG, STA_CAMERA_SET_ROOM);
        break;
    }
}
