#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "emwindow.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "cockpit.h"
#include "esp.h"
#include "fade.h"
#include "snd.h"
#include "rnd.h"
#include "est.h"

// Room 1-1f (D:/Bio4/Prog/r11f.cpp): the village chief's barn; the meeting event (s00/s01/s02),
// the boss fight (0xF8 / 0xF9) with its s10 stream and the s11 escape event.

void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp
// Event::FlgOnStatus is called out of line here (the DOL copy), not the header inline.
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");
// The chief (game/em2b.cpp): only the two event virtuals the room calls.
class cEm2b : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2B_WK)
    virtual void v50();
    virtual void v58();
};

struct R11fWork {
    cEmWrap em0;      // 0x00  the boss (list 0xF8, then 0xF9)
    cEmWrap em1;      // 0x0C  list 0xFA
    cEm* dram;        // 0x18  etc dram 9
    cEmWindow* win;         // 0x1C  etc window 0xB
    u32 strId;        // 0x20  SndStrReq handle of the fight stream
};

static int r11f_actNo;
static R11fWork* r11f_work;

// the split object's .data is 8-aligned
asm(".section .data; .balign 8");
static int r11f_actOn = 0;

extern "C" void r11f_DoorReplace();
static void r11f_EventS00();
static void r11f_EventS00_Act();
extern "C" void Evt_R11FS00_Func(Event* e);
extern "C" void Evt_R11FS01_Func(Event* e);
extern "C" void Evt_R11FS02_Func(Event* e);
extern "C" void Evt_R11FS10_Func(Event* e);
extern "C" void Evt_R11FS11_Func(Event* e);
static void r11f_EventS10();
static void r11f_EventS10CancelEndProc();
static void r11f_EventS10EndProc();
static void r11f_Eventxxx();
static void r11f_EventS11();
static void r11f_AshleyRunUp();

// Room init (the chief's barn, Mendez): before the meeting (Room_flg bit 0) pre-load evd r11fs00,
// area 0x18 = the s00 event, the five event callbacks, the intact doors shown, Ashley initialised as the
// following partner (Status_flg[3] 0x04000000), area 8 off; afterwards the broken doors (DoorReplace)
// and the post-fight layout. The dram (etc 9) and window (etc 0xB) handles, a hit piece.
void R11fInit()
{
    Vec pos;
    Vec rot;
    cEmHit* hit;

#line 64 "D:/Bio4/Prog/r11f.cpp"
    r11f_work = (R11fWork*) MEM_CALLOC(sizeof(R11fWork), 1, 0xD);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadAram("event/evd/r11fs00.evd", 0, 0, 0, 0);
        SceAtDataSet_exec(0x18, SCE_LEVEL10, 0, r11f_EventS00, 0, 1);
        EvtMgr.SetFunc("evt_r11fs00_func", (void*) Evt_R11FS00_Func);
        EvtMgr.SetFunc("evt_r11fs01_func", (void*) Evt_R11FS01_Func);
        EvtMgr.SetFunc("evt_r11fs02_func", (void*) Evt_R11FS02_Func);
        EvtMgr.SetFunc("evt_r11fs10_func", (void*) Evt_R11FS10_Func);
        EvtMgr.SetFunc("evt_r11fs11_func", (void*) Evt_R11FS11_Func);
        SmdSetTrans(0x14, 1);
        SmdSetTrans(0x15, 0);
        SmdSetTrans(0x16, 0);
        if (!StaFlagChk(pG, STA_SUB_ASHLEY)) {
            StaFlagOn(pG, STA_SUB_ASHLEY);
            SubCharInit(1, &pPL->pos, pPL->ang.y);
            SubCharCtrl(SCC_CHASE, 0);
        }
        SceAtSetEnable(8, 0);
    } else {
        r11f_DoorReplace();
        SmdSetTrans(0x14, 0);
        SmdSetTrans(0x15, 1);
        SceAtSetEnable(4, 0);
    }
    if (getRoomEtcWindow(0xB, &r11f_work->win, 1)) {
        r11f_work->win->SetEnableDamage(0);
        r11f_work->win->SetEtcFlag(3, 1);
        r11f_work->win->SetEnableFence(0, 0);
        r11f_work->win->be_flag &= ~2;
        r11f_work->win->hp = 0;
    }
    if (getRoomEtcDram(9, &r11f_work->dram, 1) == 0) {
        SceAtSetEnable(7, 0);
    }
    SeAtSetOnOff(0, 0);
    SeAtSetOnOff(1, 0);
    pos.x = 38391.0f;
    pos.y = -5950.0f;
    pos.z = -38581.0f;
    rot.x = 0.0f;
    rot.y = -0.49f;
    rot.z = 0.0f;
    hit = SetEmHit(ROOM_ARC_PTR(pG->pRoom, 0x2B), ROOM_ARC_PTR(pG->pRoom, 0x2C), &pos, &rot, 2);
    if (hit) {
        hit->setBeetle(ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x2E));
    }
}

// Replaces the two barn doors by the four broken-door objects.
extern "C" void r11f_DoorReplace()
{
    Vec zero = {0, 0, 0};
    cObj* obj;

    SmdSetTrans(1, 0);
    SmdSetTrans(2, 0);
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &zero, &zero, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 3, 0, 1, 0);
    obj->be_flag |= 0x1000;
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &zero, &zero, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 3, 0, 1, 0);
    obj->be_flag |= 0x1000;
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), &zero, &zero, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x27), 3, 0, 1, 0);
    obj->be_flag |= 0x1000;
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x29), &zero, &zero, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, 0, 1, 0);
    obj->be_flag |= 0x1000;
}

// Per frame: item area 7 (on the dram) is disabled once the dram is broken (hp <= 0) or missing.
void R11fMain()
{
    if (r11f_work->dram) {
        if (r11f_work->dram->hp <= 0) {
            SceAtSetEnable(7, 0);
        }
    } else {
        SceAtSetEnable(7, 0);
    }
}

// Area 0x18: the meeting with the chief, then the fight setup.
static void r11f_EventS00()
{
    RsfSet(G_ROOM_ID, 0);
    SceAtSetEnable(0x18, 0);
    SndRoomBgmVolSet(0, 1, 600);
    SndRoomBgmVolSet(1, 1, 600);
    SceEventStart(0);
    SubCharCtrl(SCC_KILL, 0);
    EvtMgr.EvtReadExec("event/evd/r11fs00.evd", 0, EvtReadFlagNone);
    r11f_DoorReplace();
    if (!(pG->Room_flg[0] & 0x80000000)) {
        EvtMgr.EvtReadExec("event/evd/r11fs01.evd", 0, EvtReadFlagDiedemo);
    } else {
        EvtMgr.EvtReadExec("event/evd/r11fs02.evd", 0, EvtReadFlagNone);
        if (r11f_work->em0.setEm(0xF8, -1, 1, 1, 1)) {
            Cckpt.lifeMeterBoss(r11f_work->em0.getPtr());
        }
        GamePointBossReset();
        {
            Vec pos = {37520.0f, -8000.0f, -63991.0f};
            Vec ang;
            f32 ry = -0.25f;
            cPlayer* pl = pPL;
            Vec* pa = &ang;

            pl->setPos(&pos);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
        SceEventEnd(0);
        StaFlagOn(pG, STA_LASERSITE_NOADD);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x801, ESP_CORE_KIND_NONE, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 4, 0x801, ESP_CORE_KIND_ROOM00, 0, 0);
        SceExec(0x12, r11f_EventS10, 0, 0, SCE_PRIO_DEF_2, 0);
        SeAtSetOnOff(0, 1);
        SeAtSetOnOff(1, 1);
    }
}

// Action-button success callback of the s00 event: Room_flg[0] bit 31.
static void r11f_EventS00_Act()
{
    pG->Room_flg[0] |= 0x80000000;
}

// Event r11fs00 callback (Mendez confronts Leon): funcMode 0 picks the QTE variant (actNo 3 or 4 at
// random), status 3, cancel cut 0x10, hides objects 1/2; from cut 5 the Mendez / Leon event models
// evm7000 / evm8300 are swapped in (CMF), later cuts run the action prompt and its pass / fail branches.
extern "C" void Evt_R11FS00_Func(Event* e)
{
    void* mod;
    int skip;

    switch (e->FuncType) {
    case 0:
        pG->Room_flg[0] &= ~0x80000000;
        r11f_actNo = (Rnd() & 1) ? 3 : 4;
        {
            int cut = 0x10;

            EvtFlgOnStatus(e, 3);
            e->EvtCancelCut = cut;
        }
        SmdSetTrans(1, 0);
        SmdSetTrans(2, 0);
        break;
    case 1:
        if (e->NowCut > 4) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm7000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "evm8300", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm7000", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "evm8300", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm7000", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm8300", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                    ((cModel*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm0500", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                    ((cModel*) mod)->be_flag |= 0x80;
                }
                if (e->GetMod(&mod, "evm8400", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                    ((cModel*) mod)->be_flag |= 0x80;
                }
            }
            break;
        case 0x11:
            if (e->NowFrame == 20) {
                r11f_actOn = 1;
            }
            break;
        case 3:
            if (e->NowFrame == 160) {
                skip = 1;
                if (!(e->StatusFlag & EvtStfBit(EvtStfToolFrontExec))) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(2, 7, 0, 0);
                }
            }
            break;
        case 4:
            if (e->NowFrame == 0) {
                skip = 1;
                if (!(e->StatusFlag & EvtStfBit(EvtStfToolFrontExec))) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(0x80000002, 25, 0, 0);
                }
            }
            break;
        }
        break;
    case 2:
        SmdSetTrans(1, 1);
        SmdSetTrans(2, 1);
        r11f_actOn = 0;
        break;
    case 3:
        skip = 1;
        if (!(e->StatusFlag & EvtStfBit(EvtStfEvtCancelSet))) {
            skip = 0;
        }
        if (skip == 0) {
            EventMgr* em = &EvtMgr;
            em->EvtSndStrPlay(em->GetNowExeEvtNamePtr(), 1, 0x50, 1, 0.0f);
        }
        break;
    }
    if (r11f_actOn == 1) {
        if (pG->Room_flg[0] & 0x80000000) {
            r11f_actOn = 0;
            e->CancelSet();
        } else {
            DpfFlagOff(pG, DPF_MESSAGE);
            ActBtn.set(ACT_GUARD, 5, (void*) r11f_EventS00_Act, 0, ACTCTR_ENFORCE_EXEC | ACTCTR_NO_SUSPEND | ACTCTR_EXACT_KEY, r11f_actNo, ACT_FUNC_SCE, 0);
            SpfFlagOff(pG, SPF_ACTBTN);
        }
    }
}

// Event r11fs01 callback (the QTE dodge variant): hides the barn wall objects 0xD/0xE/0x12 on cuts
// 2/4/5, shows them otherwise.
extern "C" void Evt_R11FS01_Func(Event* e)
{
    if (e->FuncType == 1) {
        if (e->NowFrame == 0) {
            if (e->NowCut <= 5) {
                if (e->NowCut == 2 || e->NowCut == 4 || e->NowCut == 5) {
                    SmdSetTrans(0xE, 0);
                    SmdSetTrans(0xD, 0);
                    SmdSetTrans(0xD, 0);
                    SmdSetTrans(0x12, 0);
                } else {
                    SmdSetTrans(0xE, 1);
                    SmdSetTrans(0xD, 1);
                    SmdSetTrans(0xD, 1);
                    SmdSetTrans(0x12, 1);
                }
            }
        }
    }
}

// Event r11fs02 callback (the other QTE variant): et1200 shown on cut 0, the fire effects re-set on cut
// 0xB, the knife model wep0200 shown from cut 7.
extern "C" void Evt_R11FS02_Func(Event* e)
{
    void* mod;

    if (e->FuncType == 1) {
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "et1200", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
            }
            break;
        case 0xB:
            if (e->NowFrame == 0) {
                EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
                EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM01, 0);
                EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM01, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 4, 0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
            }
            break;
        case 7:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 8:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        }
    }
}

// Event r11fs10 callback (Mendez transforms): light mask 2 on evm3500 and evm0600 drawn on cut 0.
extern "C" void Evt_R11FS10_Func(Event* e)
{
    void* mod;

    if (e->FuncType == 1) {
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm3500", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 2;
                }
                if (e->GetMod(&mod, "evm0600", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x10;
                }
            }
        }
    }
}

// Event r11fs11 callback (the escape from the burning barn): cut 2 hides the wall objects and swaps the
// door objects 0x14 -> 0x15/0x16 (broken), with a fire effect; other cuts show the walls; the end
// restores.
extern "C" void Evt_R11FS11_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        break;
    case 1:
        if (e->NowFrame == 0) {
            if (e->NowCut == 2) {
                SmdSetTrans(0xE, 0);
                SmdSetTrans(0xD, 0);
                SmdSetTrans(0xD, 0);
                SmdSetTrans(0x12, 0);
                SmdSetTrans(0x14, 0);
                SmdSetTrans(0x15, 1);
                SmdSetTrans(0x16, 1);
            } else {
                SmdSetTrans(0xE, 1);
                SmdSetTrans(0xD, 1);
                SmdSetTrans(0xD, 1);
                SmdSetTrans(0x12, 1);
            }
        }
        if (e->NowCut == 2) {
            if (e->NowFrame == 0) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
        }
        break;
    case 2:
        break;
    case 3:
        SmdSetTrans(0xE, 1);
        SmdSetTrans(0xD, 1);
        SmdSetTrans(0xD, 1);
        SmdSetTrans(0x12, 1);
        SmdSetTrans(0x14, 0);
        SmdSetTrans(0x15, 1);
        SmdSetTrans(0x16, 1);
        break;
    }
}

// The boss fight: the chief's second form, the stream and the two camera cuts.
static void r11f_EventS10()
{
    while (r11f_work->em0.isActive() == 1) {
        SceSleep(1);
    }
    r11f_work->em0.destroy();
    r11f_work->em0.setEm(0xF9, -1, 1, 1, 1);
    r11f_work->em1.setEm(0xFA, -1, 0, 1, 0);
    SceEventStart(0);
    r11f_work->em0.setNoSuspend(1);
    r11f_work->em1.setNoSuspend(1);
    CamCtrl.AreaOnOff(1, 0, 0);
    CamCtrl.AreaOnOff(2, 0, 0);
    CamCtrl.AreaOnOff(7, 0, 1);
    CamCtrl.AreaOnOff(8, 0, 1);
    CamCtrl.UnsetAreaAttr(7, 0, 4);
    CamCtrl.UnsetAreaAttr(8, 0, 4);
    CamCtrl.SetAreaAttr(7, 0, 3);
    CamCtrl.SetAreaAttr(8, 0, 3);
    CamCtrl.CutCall(5);
    r11f_work->strId = SndStrReq(1, 0x2C, 0x80000003, 0, 0, 0.0f);
    pG->Room_flg[0] &= ~0x20000000;
    SceSetEventCancel(1, r11f_EventS10CancelEndProc, 0, 2, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r11f_EventS10EndProc();
}

// Cancel path of the transform event: tells the boss (cEm2b virtual 0x58) the event was skipped, then the common end.
static void r11f_EventS10CancelEndProc()
{
    ((cEm2b*) r11f_work->em0.getPtr())->v58();
    r11f_EventS10EndProc();
}

// End of the transform event: stop the stream if Room_flg[0] 0x20000000 says it plays, camera back,
// put Leon at the fixed fight position facing -0.25 rad, SceEventEnd, point the life meter at the boss
// and start the fight watcher (r11f_Eventxxx).
static void r11f_EventS10EndProc()
{
    Vec pos = {36459.0f, -8000.0f, -63991.0f};

    // struct-member view: the pG load stays below the three template-copy stores of `pos`
    if (pG->Room_flg[0] & 0x20000000) {
        SndStrReq(r11f_work->strId, 8, 0, 0);
    }
    CamCtrl.Comeback(0);
    r11f_work->em0.setNoSuspend(0);
    r11f_work->em1.setNoSuspend(0);
    {
        Vec ang;
        f32 ry = -0.25f;
        cPlayer* pl = pPL;
        Vec* pa = &ang;

        pl->setPos(&pos);
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(&ang);
    }
    SceEventEnd(0);
    if (r11f_work->em0.isActive()) {
        Cckpt.lifeMeterBoss(r11f_work->em0.getPtr());
    }
    SceExec(0x12, r11f_Eventxxx, 0, 0, SCE_PRIO_DEF_2, 0);
}

// After the fight: the s10 event once the boss is gone.
static void r11f_Eventxxx()
{
    EvtMgr.EvtReadAram("event/evd/r11fs11.evd", 0, 0, 0, 0);
    EvtMgr.EvtReadAram("event/evd/r11fs10.evd", 0, 0, 0, 0);
    while (r11f_work->em0.isActive() == 1) {
        SceSleep(1);
    }
    SceEventStart(0);
    ((cEm2b*) r11f_work->em0.getPtr())->v50();
    SndRoomStrStop(2);
    EvtMgr.EvtReadExec("event/evd/r11fs10.evd", 0, EvtReadFlagNone);
    SceAtDataSet_exec(0x80, SCE_LEVEL10, 0, r11f_EventS11, 0, 1);
    SceEventEnd(0);
}

// Area 0x80: the s11 escape event, then the player and Ashley outside.
static void r11f_EventS11()
{
    Vec pos = {35332.0f, -8000.0f, -60179.0f};

    SceEventStart(0);
    SceAtDataReset(0x80);
    EvtMgr.EvtReadExec("event/evd/r11fs11.evd", 0, EvtReadFlagNone);
    SceEventEnd(0);
    SceAtSetEnable(8, 1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 7, 1, ESP_CORE_KIND_NONE, 0, 0);
    EspGenSetMoveLoop(200);
    SceAtExecute(0x80);
    {
        Vec ang;
        f32 ry = 1.33f;
        cPlayer* pl = pPL;
        Vec* pa = &ang;

        pl->setPos(&pos);
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(&ang);
    }
    {
        pG->Room_flg[0] |= 0x40000000;
        Vec subPos = {44154.0f, -8000.0f, -48080.0f};
        SubCharInit(1, &subPos, 2.987f);
    }
    SubCharCtrl(0, 0);
    CamCtrl.AreaOnOff(7, 0, 0);
    CamCtrl.AreaOnOff(8, 0, 0);
    CamCtrl.AreaOnOff(6, 0, 1);
    CamCtrl.UnsetAreaAttr(6, 0, 4);
    CamCtrl.SetAreaAttr(6, 0, 3);
    r11f_work->win->SetEnableFence(1, 0);
    while (EffGetAreaState(0) == 1) {
        SceSleep(1);
    }
    EffectEspDelete(0x801, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(0x801, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(0x801, ESP_CORE_KIND_ROOM00, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_NONE, 0, 0);
    StaFlagOff(pG, STA_LASERSITE_NOADD);
    SceExec(0x12, r11f_AshleyRunUp, 0, 0, SCE_PRIO_DEF_2, 0);
}

// After the escape event: half a second later Ashley (pSUB) runs back to Leon (chase) with her SE.
static void r11f_AshleyRunUp()
{
    SceSleep(30);
    if (!(SubCharGetStatus() & 0x20000000)) {
        SubCharCtrl(SCC_CHASE, 0);
    }
    RoomSeCall(0, &pSUB->pos, 0, 0, pSUB);
}
