#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "emBarred.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "snd.h"
#include "est.h"
#include "esp.h"
#include "TexRender.h"
#include "id_sys.h"
#include "cockpit.h"
#include "hermite.h"
#include "cManager.h"
#include "st_mgr_event.h"

// Room 3-30 (D:/Bio4/Prog/r330.cpp): the s00 event (the mine cart ride start) with its two render-to-texture
// cameras and the count-down style HUD (idR330) shown during the event.

struct R330Work {
    TexRenderMng* tex[2];   // 0x000  render targets of the two screens
    u8 texTbl0[0x80];       // 0x008  blend table of tex[0]
    u8 texTbl1[0x80];       // 0x088  blend table of tex[1]
    TexRenderCam cam0;      // 0x108  event render camera of tex[0]
    TexRenderCam cam1;      // 0x40C  event render camera of tex[1]
};

// Event HUD: a percentage pair (unit ids 1..4 / 5..8 of id table 0x2C) driven by the event mode.
class idR330 {
public:
    u32 mode;   // 0x0  init argument: which id table (0..2)
    u32 cnt;    // 0x4  frames since init

    void init(u32 mode);
    void move();
    void quit();
};

static R330Work* r330_work;
idR330 IdR330;

// Scroll speed (frames per texture cycle) of the five background units 9 / 0x10..0x13.
static s16 r330_scrollTbl[5] = {30, 15, 20, 25, 40};



void R330EventS00Main();
void R330EventS00End();
extern "C" void Evt_R330S00_Func(Event* e);
void EvtTexRenderCamTrans(Event* e, int cut);

// Room init: the barred doors 0xA/0xB paired; the s00 callback; until seen (Room_flg bit 0) area 3 =
// the event (pre-loaded with the enemy of ESL 0xA0) and both doors lock-locked; the two screen render
// targets; scroll objects 0x28/0x29 hidden.
void R330Init()
{
#line 48 "D:/Bio4/Prog/r330.cpp"
    r330_work = (R330Work*) MEM_CALLOC(sizeof(R330Work), 1, 0xd);
    {
        cEm* a;
        cEm* b;

        if (getRoomEtcBarred(0xA, &a, 1)) {
            if (getRoomEtcBarred(0xB, &b, 1)) {
                ((cEmBarred*) a)->setDouble((cEmBarred*) b);
            }
        }
    }
    EvtMgr.SetFunc("evt_r330s00_func", (void*) Evt_R330S00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) R330EventS00Main, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r330s00.evd", (u8) GetEmIdFromList(0xA0), 0, 0, 0);
        {
            cEm* a;

            if (getRoomEtcBarred(0xA, &a, 1)) {
                ((cEmBarred*) a)->setLockMode(1);
            }
        }
        {
            cEm* b;

            if (getRoomEtcBarred(0xB, &b, 1)) {
                ((cEmBarred*) b)->setLockMode(1);
            }
        }
    }
    TexRenderInit(&r330_work->tex[0], 0, 1);
    TexRenderInit(&r330_work->tex[1], 0, 1);
    SmdSetTrans(0x28, 0);
    SmdSetTrans(0x29, 0);
}

// Per-frame room main: nothing.
void R330Main()
{
}

// Area 3 once (Room_flg bit 0), only with Ashley able to come along (else message 0x67): the doors
// unlocked, camera cut 1, then event r330s00 with the HUD (idR330); player-cancellable.
void R330EventS00Main()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        if (CheckDoorJumpWithAshley() == 0) {
            cMes.MesSet(0x67, 100, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        } else {
            cEm* a;
            cEm* b;
            int i;

            RsfSet(G_ROOM_ID, 0);
            SceAtSetEnable(3, 0);
            if (getRoomEtcBarred(0xA, &a, 1)) {
                ((cEmBarred*) a)->setLockMode(0);
            }
            if (getRoomEtcBarred(0xB, &b, 1)) {
                ((cEmBarred*) b)->setLockMode(0);
            }
            SceEventStart(0);
            SceSetEventCancel(1, (TaskFunc) R330EventS00End, 0, -1, 1);
            pPL->beginEvent(0);
            pPL->setNoSuspend(1);
            if (pSUB) {
                pSUB->beginEvent(0);
                pSUB->setNoSuspend(1);
            }
            CamCtrl.CutCall(1);
            for (i = 0; i < 15; i++) {
                SceSleep(1);
            }
            if (getRoomEtcBarred(0xA, &a, 1)) {
                a->setNoSuspend(0);
            }
            if (getRoomEtcBarred(0xB, &b, 1)) {
                b->setNoSuspend(0);
            }
            pPL->endEvent(0);
            pPL->setNoSuspend(0);
            if (pSUB) {
                pSUB->endEvent(0);
                pSUB->setNoSuspend(0);
            }
            CamCtrl.Comeback(0);
            SceSetEventCancel(0, 0, 0, -1, 1);
            EvtMgr.EvtReadExec("event/evd/r330s00.evd", (u8) GetEmIdFromList(0xA0), EvtReadFlagNone);
            SceSetEventCancel(0, 0, 0, -1, 1);
            R330EventS00End();
        }
    }
}

// End of the s00 event (also its cancel path): both barred doors closed, Leon and Ashley out of event
// mode and placed at the far side facing -1.66 rad, camera back, SceEventEnd, Scenario_flg[2] 0x800,
// chapter 5-4 ends (SceSetChapterEnd(0x11)).
void R330EventS00End()
{
    {
        cEm* a;

        if (getRoomEtcBarred(0xA, &a, 1)) {
            a->setNoSuspend(1);
            ((cEmBarred*) a)->setLockMode(0);
            ((cEmBarred*) a)->setClosed();
        }
    }
    {
        cEm* b;

        if (getRoomEtcBarred(0xB, &b, 1)) {
            b->setNoSuspend(1);
            ((cEmBarred*) b)->setLockMode(0);
            ((cEmBarred*) b)->setClosed();
        }
    }
    pPL->endEvent(0);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->endEvent(0);
        pSUB->setNoSuspend(0);
    }
    pPL->setPos(7806.0f, -4649.0f, 7377.0f);
    {
        Vec ang;

        ang.x = 0.0f;
        ang.z = 0.0f;
        ang.y = -1.66f;
        pPL->setAng(&ang);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    ScfFlagOn(pG, SCF_R330_END_OPE);
    SceSetChapterEnd(0x11, -1);
    SeAtSndCall(0);
}

// Event r330s00 callback: the pre-event objects hidden / shown, the two screen render passes fed on
// their cuts (EvtTexRenderCamTrans), the idR330 HUD driven by the event mode, per-cut model flags.
extern "C" void Evt_R330S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0x1C, 0);
        SmdSetTrans(0x1D, 0);
        SmdSetTrans(0x1E, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
        pG->Room_flg[0] &= ~0x80000000;
        SmdSetTrans(0x28, 1);
        SmdSetTrans(0x29, 1);
        setRoomEtcDisp(0xA, 0, 1);
        setRoomEtcDisp(0xB, 0, 1);
        {
            cEm* a;

            if (getRoomEtcBarred(0xA, &a, 1)) {
                ((cEmBarred*) a)->setClose(0);
            }
        }
        {
            cEm* b;

            if (getRoomEtcBarred(0xB, &b, 1)) {
                ((cEmBarred*) b)->setClose(0);
            }
        }
        break;
    case 1:
        switch (e->NowCut) {
        case 0x18:
        case 0x1A:
        case 0x1B:
        case 0x1D:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0100a", 0, 0) == 1) {
                    ((cObjUnion*) mod)->o18.be_flag |= 0x40;
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0100a", 0, 0) == 1) {
                    ((cObjUnion*) mod)->o18.be_flag &= ~0x40;
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 9:
        case 0x14:
        case 0x19:
            if (e->NowFrame == 0) {
                u32 no;

                switch (e->NowCut) {
                default:
                case 9:
                    no = 0;
                    break;
                case 0x14:
                    no = 1;
                    break;
                case 0x19:
                    no = 2;
                    break;
                }
                IdR330.init(no);
                pG->Room_flg[0] |= 0x80000000;
            }
            if (pG->Room_flg[0] & 0x80000000) {
                IdR330.move();
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (pG->Room_flg[0] & 0x80000000) {
                    pG->Room_flg[0] &= ~0x80000000;
                    IdR330.quit();
                }
            }
            break;
        }
        {
            void* mod;

        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm9900", 0, 0) == 1) {
                    ((cModel*) mod)->ot_type = 1;
                }
                if (e->GetMod(&mod, "evm9700", 0, 0) == 1) {
                    ((cModel*) mod)->ot_type = 1;
                    ((cModel*) mod)->LightInfo.EnableMask = 2;
                }
                if ((obj = SmdGetObjPtr(0x28)) != 0) {
                    e->SetMod("scr0000", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
                if ((obj = SmdGetObjPtr(0x29)) != 0) {
                    e->SetMod("scr0100", obj, 5, 0, 2, 0);
                    obj->setPos(&pos);
                    obj->setAng(&rot);
                    obj->be_flag |= 0x20;
                    e->EspSetModelPtr(obj);
                }
            }
        }
        switch (e->NowCut) {
        case 6:
        case 8:
        case 0xB:
        case 0xF:
        case 0x17:
            if (e->NowFrame == 0) {
                if ((mod = SmdGetObjPtr(0x22)) != 0) {
                    TexRenderModSet((cModel*) mod, 0, r330_work->texTbl0, r330_work->tex[0], 1, 1, 1, 1, 1.0f);
                }
                if ((mod = SmdGetObjPtr(0x23)) != 0) {
                    TexRenderModSet((cModel*) mod, 0, r330_work->texTbl1, r330_work->tex[1], 1, 1, 1, 1, 1.0f);
                }
            }
            if (e->NowCut == 6) {
                EvtTexRenderCamTrans(e, 6);
            }
            if (e->NowCut == 8) {
                EvtTexRenderCamTrans(e, 8);
            }
            if (e->NowCut == 0xB) {
                EvtTexRenderCamTrans(e, 0xB);
            }
            if (e->NowCut == 0xF) {
                EvtTexRenderCamTrans(e, 0xF);
            }
            if (e->NowCut == 0x17) {
                EvtTexRenderCamTrans(e, 0x17);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if ((mod = SmdGetObjPtr(0x22)) != 0) {
                    TexRenderModRes((cModel*) mod, 0);
                    ModelInfoSetTrans((cModel*) mod, 0, 1);
                }
                if ((mod = SmdGetObjPtr(0x23)) != 0) {
                    TexRenderModRes((cModel*) mod, 0);
                    ModelInfoSetTrans((cModel*) mod, 0, 1);
                }
            }
            break;
        }
        }
        break;
    case 2:
        SmdSetTrans(0x1C, 1);
        SmdSetTrans(0x1D, 1);
        SmdSetTrans(0x1E, 1);
        EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
        EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
        EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
        w = SmdGetWorkPtr(0x28);
        if ((obj = SmdGetObjPtr(0x28)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        w = SmdGetWorkPtr(0x29);
        if ((obj = SmdGetObjPtr(0x29)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        SmdSetTrans(0x28, 0);
        SmdSetTrans(0x29, 0);
        setRoomEtcDisp(0xA, 1, 1);
        setRoomEtcDisp(0xB, 1, 1);
        break;
    }
}

// Render-to-texture pass of the event camera cuts 6 / 8 / 11 / 15 / 23: the three cart models into both
// targets, then the cut's camera path.
void EvtTexRenderCamTrans(Event* e, int cut)
{
    void* mod;
    void* bin;
    int skip = 1;

    if ((e->StatusFlag & EvtStfBit(EvtStfToolFrontExec)) == 0) {
        skip = 0;
    }
    if (skip == 0) {
        if (e->GetMod(&mod, "evm9900", 0, 0) == 1) {
            TexRenderModAddOt(0, (cModel*) mod);
            TexRenderModAddOt(1, (cModel*) mod);
        }
        if (e->GetMod(&mod, "evm9700", 0, 0) == 1) {
            TexRenderModAddOt(0, (cModel*) mod);
            TexRenderModAddOt(1, (cModel*) mod);
        }
        if (e->GetMod(&mod, "evm9800", 0, 0) == 1) {
            TexRenderModAddOt(0, (cModel*) mod);
            TexRenderModAddOt(1, (cModel*) mod);
        }
        switch (cut) {
        case 6:
            if (EvtMgr.GetBin(&bin, "event/r330/s00/cam/etc_s00_006.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r330_work->cam0, (TexRenderEvt*) e, bin);
                TexRenderCamAddOt(1, &r330_work->cam1, (TexRenderEvt*) e, bin);
            }
            break;
        case 8:
            if (EvtMgr.GetBin(&bin, "event/r330/s00/cam/etc_s00_008.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r330_work->cam0, (TexRenderEvt*) e, bin);
                TexRenderCamAddOt(1, &r330_work->cam1, (TexRenderEvt*) e, bin);
            }
            break;
        case 0xB:
            if (EvtMgr.GetBin(&bin, "event/r330/s00/cam/etc_s00_011.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r330_work->cam0, (TexRenderEvt*) e, bin);
                TexRenderCamAddOt(1, &r330_work->cam1, (TexRenderEvt*) e, bin);
            }
            break;
        case 0xF:
            if (EvtMgr.GetBin(&bin, "event/r330/s00/cam/etc_s00_015.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r330_work->cam0, (TexRenderEvt*) e, bin);
                TexRenderCamAddOt(1, &r330_work->cam1, (TexRenderEvt*) e, bin);
            }
            break;
        case 0x17:
            if (EvtMgr.GetBin(&bin, "event/r330/s00/cam/etc_s00_023.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r330_work->cam0, (TexRenderEvt*) e, bin);
                TexRenderCamAddOt(1, &r330_work->cam1, (TexRenderEvt*) e, bin);
            }
            break;
        }
    }
}

// The event HUD: id table 0x2C from room archive 0x22 / 0x20 / 0x21 by `no`, textures from 0x1F.
void idR330::init(u32 no)
{
    mode = no;
    IdTexRelease(4);
    IdSys.roomInit();
    IdTexDataLoad(ROOM_ARC_PTR(pG->pRoom, 0x1F), 7);
    switch (mode) {
    case 0:
        IdSys.set(ROOM_ARC_PTR(pG->pRoom, 0x22), 0xFF, IDC_EVENT, 0xC, 6, 0);
        break;
    case 1:
        IdSys.set(ROOM_ARC_PTR(pG->pRoom, 0x20), 0xFF, IDC_EVENT, 0xC, 6, 0);
        break;
    case 2:
        IdSys.set(ROOM_ARC_PTR(pG->pRoom, 0x21), 0xFF, IDC_EVENT, 0xC, 6, 0);
        break;
    }
    cnt = 0;
}

// Shows `val` (0..999) in the three digit units from `id` (leading zeros hidden) and the "%" unit after
// them. A macro over the caller's `n`/`on`/`u`: the two copies share those pseudos (the shared `n` keeps
// the r8 preference of `cnt + 20` for both digit loops, the shared `on` outranks the unit-index giv).
#define R330_SET_NUMBER(val, id)                                    \
    {                                                               \
        int digit[3];                                               \
        int i;                                                      \
                                                                    \
        n = val;                                                    \
        for (i = 0; i < 3; i++) {                                   \
            digit[i] = n % 10;                                      \
            n /= 10;                                                \
        }                                                           \
        on = 0;                                                     \
        for (i = 2; i >= 0; i--) {                                  \
            u = IdSys.unitPtr((id) + (2 - i), IDC_EVENT);                \
            if (on == 0 && digit[i] == 0 && i != 0) {               \
                u->be_flag &= ~8;                                   \
            } else {                                                \
                on = 1;                                             \
                u->be_flag |= 8;                                    \
                u->tex_flag |= 2;                                   \
                u->texNo = digit[i];                                \
            }                                                       \
        }                                                           \
        u = IdSys.unitPtr((id) + 3, IDC_EVENT);                          \
        u->texNo = 10;                                              \
        u->tex_flag |= 2;                                           \
    }

// Per frame: scroll the five background units (ids 9, 0x10..0x13) at r330_scrollTbl rates and count
// the percentage digit pairs up with `cnt`.
void idR330::move()
{
    IdUnit* u;
    int i;
    int a;
    int b;
    int n;
    int on;

    cnt++;
    for (i = 0; i < 5; i++) {
        u8 id;
        f32 rate;

        switch (i) {
        case 0:
            id = 9;
            break;
        case 1:
            id = 0x10;
            break;
        case 2:
            id = 0x11;
            break;
        case 3:
            id = 0x12;
            break;
        case 4:
        default:
            id = 0x13;
            break;
        }
        u = IdSys.unitPtr(id, IDC_EVENT);
        rate = (f32) (cnt % r330_scrollTbl[i]) / (f32) r330_scrollTbl[i];
        u->v0 = rate;
        u->v1 = rate + 1.0f;
    }
    switch (mode) {
    case 0:
        a = cnt + 20;
        b = (int) ((f32) cnt * 0.8f) + 15;
        break;
    case 1:
        u = IdSys.unitPtr(0x14, IDC_EVENT);
        a = (int) (Hermite_1CurveCalc(u->curve[1], (f32) (s16) u->timer[1]) * 100.0f);
        u = IdSys.unitPtr(0x15, IDC_EVENT);
        b = (int) (Hermite_1CurveCalc(u->curve[1], (f32) (s16) u->timer[1]) * 100.0f);
        break;
    case 2:
        a = 100;
        b = 100;
        break;
    default:
        a = 0;
        b = 0;
        break;
    }
    R330_SET_NUMBER(a, 1);
    R330_SET_NUMBER(b, 5);
}

// Close the HUD: cockpit ids back, display 0x21 off.
void idR330::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
    IdSys.dispSw(IDC_LIFE_METER, 0);
}
