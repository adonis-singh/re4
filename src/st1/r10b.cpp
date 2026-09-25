#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "event.h"
#include "dmg.h"
#include "flag_rsf.h"
#include "st_mgr_event.h"
#include "global.h"
#include "game.h"
#include "datactrl.h"
#include "dvd.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj1c.h"
#include "obj16.h"
#include "em.h"
#include "em_set.h"
#include "esp.h"
#include "est.h"
#include "espgen.h"
#include "player.h"
#include "cockpit.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "snd.h"
#include "rnd.h"

// Room 1-0b (D:/Bio4/Prog/r10b.cpp): the lake; the boss fight from the boat (enemy 0x2f), the
// floating islands, the binocular view of the cliff event and the boss's tentacle heads.

struct R10bWork {
    cEm* boss;             // 0x00  the lake boss (list entry 0xA1)
    cEm* boat;             // 0x04  the boat enemy (list entry 0xA2 / 0xA3)
    cEm* em0;              // 0x08  list entry 0xA0
    int count;             // 0x0C  action button count of the s20 event
    cDataUnit* evt[16];    // 0x10  event data units by readEvent index
    IdBinocular* bino;     // 0x50
    FocusAnimation* focus; // 0x54
    u8 pad_58[4];
    IdBinocular binoObj;   // 0x5C
    FocusAnimation focusObj;  // 0x9C
    cObj* island[4];       // 0xAC
    u8 pad_BC[4];
    cObj* head[6];         // 0xC0  tentacle heads (obj 0x16)
};

static R10bWork* r10b_work;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r10b_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

// The lake boss (enemy 0x2f) by vtable slot.
class cEm2f : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM2F_WK)
    virtual void v50();
    virtual void setCamPos(Vec* pos, f32 ang);   // 0x58
    virtual void setDie();                        // 0x60
};


// Waits for the running event to end.
static inline void r10b_waitEvt()
{
    // `&EvtMgr` inside the loop (no pointer local before it): loop.c hoists the `addi` into the
    // inner preheader from its own `lis` (the target's second EvtMgr high, r26).
    while (EvtMgr.IsAliveEvt(EvtMgr.GetNowExeEvtNamePtr(), 0, 0)) {
        SceSleep(1);
    }
}

// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");

extern "C" int readEvent(int no, int wait, void** out);
extern "C" void freeEvent(int no);
static void R10b_chkEmDie();
static void R10b_chkWater();
static void r10b_GakeEvent();
extern "C" void Evt_R10BS00_Func(Event* e);
extern "C" void em2fTentacleMove(cEm* em, Event* e, int mode);
extern "C" void Evt_R10BS10_Func(Event* e);
extern "C" void Evt_R10BS20_Func(Event* e);
extern "C" void Evt_R10BS21_Func(Event* e);
extern "C" void Evt_R10BS22_Func(Event* e);
extern "C" void Evt_R10BSXX_Func_Pl0f(Event* e);
extern "C" void Evt_R10BSXX_Func_Em2f(Event* e);
static void r10b_setEm();

// Room init (the lake, Del Lago): System_flg 0x800, the water-follow task, water hit effects; area 4 =
// the cliff event once (Room_flg bit 0); event units 1..4 pre-read into the boss module's block; the
// s00/s10/s20/s21/s22 callbacks; the four floating islands and the six tentacle-head objects are
// created at fixed positions; the boat enemy (ESL 0xA2), the boss module and the death watcher.
void R10bInit()
{
    void* zero = 0;
    Vec pos;
    Vec rot;
    cObj* obj;

    SysFlagOn(pG, SYS_SCISSOR_ON);
#line 95 "D:/Bio4/Prog/r10b.cpp"
    r10b_work = (R10bWork*) MEM_CALLOC(sizeof(R10bWork), 1, 0xd);

    SceExec(0x12, (TaskFunc) R10b_chkWater, 0, 2, SCE_PRIO_DEF_2, 0);
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r10b_eff_info);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r10b_GakeEvent, 0, 1);
    }
    readEvent(1, 0, 0);
    readEvent(2, 0, 0);
    readEvent(3, 0, 0);
    readEvent(4, 0, 0);
    EvtMgr.SetFunc("evt_r10bs00_func", (void*) Evt_R10BS00_Func);
    EvtMgr.SetFunc("evt_r10bs10_func", (void*) Evt_R10BS10_Func);
    EvtMgr.SetFunc("evt_r10bs20_func", (void*) Evt_R10BS20_Func);
    EvtMgr.SetFunc("evt_r10bs21_func", (void*) Evt_R10BS21_Func);
    EvtMgr.SetFunc("evt_r10bs22_func", (void*) Evt_R10BS22_Func);
    pos.x = 79072.0f;
    pos.y = -1300.0f;
    pos.z = 45680.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1E), ROOM_ARC_PTR(pG->pRoom, 0x1F), &pos, &rot);
    r10b_work->island[0] = obj;
    if (obj != 0) {
        obj->scale.x = 1.5f;
        obj->scale.y = 1.5f;
        obj->scale.z = 1.5f;
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21),
                                   ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23));
    }
    pos.x = 12708.0f;
    pos.y = -1300.0f;
    pos.z = 33719.0f;
    rot.x = 0.0f;
    rot.y = 3.1415927f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1E), ROOM_ARC_PTR(pG->pRoom, 0x1F), &pos, &rot);
    r10b_work->island[1] = obj;
    if (obj != 0) {
        obj->scale.x = 2.0f;
        obj->scale.y = 2.0f;
        obj->scale.z = 2.0f;
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21),
                                   ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23));
    }
    pos.x = 412.0f;
    pos.y = -1300.0f;
    pos.z = 64963.0f;
    rot.x = 0.0f;
    rot.y = 1.5707964f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1E), ROOM_ARC_PTR(pG->pRoom, 0x1F), &pos, &rot);
    r10b_work->island[2] = obj;
    if (obj != 0) {
        obj->scale.x = 1.7f;
        obj->scale.y = 1.7f;
        obj->scale.z = 1.7f;
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21),
                                   ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23));
    }
    pos.x = 55820.0f;
    pos.y = -1300.0f;
    pos.z = 81636.0f;
    rot.x = 0.0f;
    rot.y = 0.7853982f;
    rot.z = 0.0f;
    obj = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1E), ROOM_ARC_PTR(pG->pRoom, 0x1F), &pos, &rot);
    r10b_work->island[3] = obj;
    if (obj != 0) {
        obj->scale.x = 1.5f;
        obj->scale.y = 1.5f;
        obj->scale.z = 1.5f;
        ((cObj1c*) obj)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21),
                                   ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23));
    }
    EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, zero, zero);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_ROOM01, zero, zero);
    r10b_work->boat = EmSetFromList2(0xA2, 1);
    RoomEfmRegist(SmdGetGroupObjPtr(0x58), 0x60);
    SetSstAddAreaFlag(0x800);
    r10b_setEm();
}

// Per-frame room main: nothing.
void R10bMain()
{
}

// Event files by readEvent index (defined here: the strings follow R10bInit's in .rodata).
static char* r10b_evtName[6] = {
    "evd/r10bs20.evd", "evd/r10bs21.evd", "evd/r10bs22.evd", "evd/r10bs00.evd", "evd/r10bs10.evd", 0,
};

// Loads event `no` (r10b_evtName) through a data unit; with `wait` the data is swapped into the
// boss module's block and `out` receives its address.
extern "C" int readEvent(int no, int wait, void** out)
{
    if (out != 0) {
        *out = 0;
    }
    if (r10b_work->evt[no] == 0) {
        r10b_work->evt[no] = DC.setData(EvtMgr.NameChange(r10b_evtName[no]));
        if (r10b_work->evt[no] == 0) {
            goto fail;
        }
    }
    if (wait != 0) {
        ReadModule* m;
        u32 max;

        if (r10b_work->evt[no]->waitLoadOk() == 0) {
            r10b_work->evt[no]->setCommand(CMND_CLEAR_DATA, 0, 0);
            pLog->err(0, 0, "readEvent() : out of memory (0x%x)", r10b_work->evt[no]->m_size);
            return 0;
        }
        EspEmDataSwapPush(0x2F);
        m = SearchEmModule(0x2F);
        max = m->size;
        if (r10b_work->evt[no]->m_size > max) {
            // `return 0` (not `goto fail`): at sched2 the block continues past the err call with
            // `li r3,0`, whose output dependence on the pLog load and the block-end jump rank the
            // `mr r7,size` and `lwz r3` above the string `lis`; jump2 then cross-jumps the tail.
            pLog->err(0, 0, "readEvent() : event size too large!![%d]>[%d]", r10b_work->evt[no]->m_size, max);
            return 0;
        }
        MemorySwap(m->pArc, (u32) r10b_work->evt[no]->m_addr, r10b_work->evt[no]->m_size);
        *out = m->pArc;
    } else {
        r10b_work->evt[no]->setCommand(CMND_ARAM_LOAD, 0, 0);
    }
    return 1;
fail:
    return 0;
}

// Release event unit `no`: swap the boss (0x2F) module's archive back over the event data it had been
// loaded into, pop the effect data swap, and clear the unit.
extern "C" void freeEvent(int no)
{
    if (r10b_work->evt[no] != 0) {
        ReadModule* m;

        m = SearchEmModule(0x2F);
        MemorySwap(m->pArc, (u32) r10b_work->evt[no]->m_addr, r10b_work->evt[no]->m_size);
        EspEmDataSwapPop(0x2F);
        r10b_work->evt[no]->setCommand(CMND_CLEAR_DATA, 0, 0);
    }
}

// Waits for the boss's death: the death event, then the chapter end (or the escape event).
static void R10b_chkEmDie()
{
    void* evt;
    u32 key;
    u32 key2;
    cEm* boss;

    SceSleep(1);
    r10b_work->em0 = GetEmPtrFromList(0xA0);
    boss = r10b_work->boss;
    Cckpt.lifeMeterBoss(boss);
    for (;;) {
        if (DebugTrg(1)) {
            boss->hp = 1;
        }
        if (boss != 0 && boss->checkStatus(EM_STATUS_ACTIVE) == 0) {
            SndRoomStrStop(0);
            SndEventStrStop(0);
            ((cEm2f*) GetEmPtrFromList(0xA0))->setDie();
            SceEventStart(0);
            SysFlagOn(pG, SYS_SCREEN_STOP);
            EmMgr.destroy(r10b_work->boss);
            EffectEventDelete();
            DmgMgr.beginEvent(0);
            SceSleep(2);
            if (readEvent(0, 1, &evt)) {
                if (EvtMgr.SetEvt(evt, 0) == 0) {
                    r10b_work->count = 30;
                }
                r10b_waitEvt();
                freeEvent(0);
            }
            if (r10b_work->count > 14) {
                if (readEvent(2, 1, &evt)) {
                    if (EvtMgr.SetEvt(evt, &key) != 0) {
                        ((Event*) key)->StatusFlag |= EvtStfBit(EvtStfFadeOut);
                    }
                    r10b_waitEvt();
                    freeEvent(2);
                }
            } else {
                if (readEvent(1, 1, &evt)) {
                    if (EvtMgr.SetEvt(evt, &key2) != 0) {
                        ((Event*) key2)->StatusFlag |= EvtStfBit(EvtStfEndSleepOrder);
                        ((Event*) key2)->StatusFlag |= EvtStfBit(EvtStfDiedemo);
                    }
                    r10b_waitEvt();
                    return;
                }
            }
            SceEventStart(0);
            SceSetChapterEnd(CHAPTER_1_3, 6);
        }
        SceSleep(1);
    }
}

// Drop the room's effect kind `kind` in all three effect systems (esp / espgen / efm).
static inline void r10b_effDelete(int kind)
{
    EffectEspDelete(0, kind, 0, 0);
    EffectEspgenDelete(0, kind, 0);
    EffectEfmDelete(0, kind, 0);
}

// Watches the lake: the boss appears when the boat reaches the middle (event s10), the rain / spray
// effects follow the boat areas and the camera target effect.
static void R10b_chkWater()
{
    void* zero;

    for (;;) {
        if (!(pG->Room_flg[0] & 0x40000000) && (pG->Room_flg[2] & 0x20000000)) {
            void* evt;
            cEm* em;
            Vec pos;

            pG->Room_flg[0] |= 0x40000000;    // reference RMW: the r10b_work load waits for the store
            EmMgr.destroy(r10b_work->boat);
            EffectEventDelete();
            DmgMgr.beginEvent(0);
            SceSleep(2);
            if (readEvent(4, 1, &evt)) {
                EvtMgr.SetEvt(evt, 0);
                while (EvtMgr.IsAliveEvt(EvtMgr.GetNowExeEvtNamePtr(), 0, 0)) {
                    SceSleep(1);
                }
                freeEvent(4);
            }
            readEvent(0, 0, 0);
            GamePointBossReset();
            r10b_work->boss = EmSetFromList2(0xA1, 1);
            SceExec(0x12, (TaskFunc) R10b_chkEmDie, 0, 0, SCE_PRIO_DEF_2, 0);
            CamCtrl.Comeback(0);
            em = GetEmPtrFromList(0xA0);
            pos.x = 67713.0f;
            pos.y = em->pos.y;
            pos.z = -31975.0f;
            ((cEm2f*) em)->setCamPos(&pos, -1.12f);
            pPL->ot_type = 0;
        }
        zero = 0;
        if (pG->Room_flg[0] & 0x80000000) {
            if (pG->Room_flg[2] & 0x40000000) {
                pG->Room_flg[0] &= ~0x80000000;
                r10b_effDelete(2);
                r10b_effDelete(3);
                SceSleep(1);
                EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_ROOM01, zero, zero);
                EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, zero, zero);
            }
        } else if (pG->Room_flg[2] & 0x80000000) {
            pG->Room_flg[0] |= 0x80000000;
            r10b_effDelete(2);
            r10b_effDelete(3);
            SceSleep(1);
            EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_ROOM01, 0, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        if (StaFlagChk(pG, STA_PL_BOAT)) {
            if (!(pG->Room_flg[0] & 0x20000000)) {
                pG->Room_flg[0] |= 0x20000000;
                r10b_effDelete(3);
            }
        } else if (pG->Room_flg[0] & 0x20000000) {
            pG->Room_flg[0] &= ~0x20000000;
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        }
        if (StaFlagChk(pG, STA_PL_BOAT)) {
            Estgen45SetTargetCamera(1);
        } else {
            Estgen45SetTargetCamera(0);
        }
        SceSleep(1);
    }
}

// The cliff event (s00): the boat is replaced and the rain effects restart.
static void r10b_GakeEvent()
{
    void* evt;

    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtSetEnable(4, 0);
        RsfSet(G_ROOM_ID, 0);
        SceEventStart(0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        r10b_effDelete(2);
        r10b_effDelete(3);
        SceSleep(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EmMgr.destroy(r10b_work->boat);
        DmgMgr.beginEvent(0);
        EffectEventDelete();
        SceSleep(2);
        if (readEvent(3, 1, &evt)) {
            EvtMgr.SetEvt(evt, 0);
            while (EvtMgr.IsAliveEvt(EvtMgr.GetNowExeEvtNamePtr(), 0, 0)) {
                SceSleep(1);
            }
            freeEvent(3);
        }
        r10b_work->boat = EmSetFromList2(0xA3, 0);    // reference store: the pG load waits for it
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SceSleep(4);
        SceEventEnd(0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        r10b_effDelete(2);
        r10b_effDelete(3);
        SceSleep(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        SysFlagOff(pG, SYS_SCREEN_STOP);
    }
}

// Event r10bs00 callback (the boat trip out, the boss shows itself): funcMode 0 keeps islands 0/2
// updating; cut 0 sets up the boat player and boss stand-ins; cuts 1/3/7 create the binocular view
// (IdBinocular + FocusAnimation) once per Status_flg[0] 0x400 and point it at the cut's target; the
// end releases them.
extern "C" void Evt_R10BS00_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        r10b_work->island[0]->setNoSuspend(1);
        r10b_work->island[2]->setNoSuspend(1);
        break;
    case 1:
        SetSstAddAreaFlag(0);
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                Evt_R10BSXX_Func_Pl0f(e);
                Evt_R10BSXX_Func_Em2f(e);
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x01000000;
                    ((cModel*) mod)->ot_type = 1;
                }
            }
            break;
        case 1:
        case 3:
        case 7:
            if (e->NowFrame == 0 && !StaFlagChk(pG, STA_BINOCULAR)) {
                StaFlagOn(pG, STA_BINOCULAR);
                r10b_work->bino = new (&r10b_work->binoObj) IdBinocular;
                r10b_work->bino->init(&pG->Camera, ROOM_ARC_PTR(pG->pRoom, 0x26), ROOM_ARC_PTR(pG->pRoom, 0x27));
                if (e->NowCut != 1) {
                    r10b_work->bino->cutin(0);
                }
                r10b_work->focus = &r10b_work->focusObj;
                r10b_work->focus->init(-1);
            }
            r10b_work->bino->move(&pG->Camera);
            break;
        default:
            if (e->NowFrame == 0 && (StaFlagChk(pG, STA_BINOCULAR))) {
                StaFlagOff(pG, STA_BINOCULAR);
                r10b_work->bino->quit(&pG->Camera);
                r10b_work->bino->~IdBinocular();
                r10b_work->focus->quit();
            }
            break;
        }
        break;
    case 2:
        if (StaFlagChk(pG, STA_BINOCULAR)) {
            StaFlagOff(pG, STA_BINOCULAR);
            r10b_work->bino->quit(&pG->Camera);
            r10b_work->bino->~IdBinocular();
        }
        SetSstAddAreaFlag(0x800);
        r10b_work->island[0]->setNoSuspend(0);
        r10b_work->island[1]->setNoSuspend(0);
        r10b_work->island[2]->setNoSuspend(0);
        r10b_work->island[3]->setNoSuspend(0);
        break;
    }
}

// The boss's tentacle heads (obj 0x16) on its parts 0x1D..0x22, each with its motion started at a
// different frame; mode 1 releases them.
extern "C" void em2fTentacleMove(cEm* em, Event* e, int mode)
{
    u32 i;

    if (mode == 0) {
        void* bin;
        void* tpl;
        void* fcv;
        u16 step;

        if (DbgFlagChk(pG, DBG_EVENT_TOOL) && (pG->Room_flg[0] & 0x8000)) {
            return;
        }
        pG->Room_flg[0] |= 0x8000;
        if (EvtMgr.GetBin(&bin, "obj/objmodel/obm3100.bin", 0) == 0) {
            return;
        }
        if (EvtMgr.GetBin(&tpl, "obj/objmodel/obm3100.tpl", 0) == 0) {
            return;
        }
        if (EvtMgr.GetBin(&fcv, "obj/objmodel/obm31000.fcv", 0) == 0) {
            return;
        }
        step = (*(u16*) fcv & 0x3FFF) / 6;
        for (i = 0; i < 6; i++) {
            if (r10b_work->head[i] == 0) {
                int parts;
                Vec pos;
                Vec rot;

                switch (i) {
                case 0:
                default:
                    parts = 0x1D;
                    break;
                case 1:
                    parts = 0x1E;
                    break;
                case 2:
                    parts = 0x1F;
                    break;
                case 3:
                    parts = 0x20;
                    break;
                case 4:
                    parts = 0x21;
                    break;
                case 5:
                    parts = 0x22;
                    break;
                }
                pos.x = 0.0f;
                pos.y = 0.0f;
                pos.z = 0.0f;
                rot.x = 1.5707964f;
                rot.y = 0.0f;
                rot.z = fRand1_1() * 3.1415927f;
                r10b_work->head[i] = SetObj16(bin, tpl, em, em, parts, 0xA, &pos, &rot);
                if (r10b_work->head[i] != 0) {
                    MotSetObj16(r10b_work->head[i], fcv, 4, i * step);
                }
            }
        }
    } else {
        for (i = 0; i < 6; i++) {
            if (r10b_work->head[i] != 0) {
                ((cObj16*) r10b_work->head[i])->clearLostWait();
                r10b_work->head[i] = 0;
            }
        }
    }
}

// Event r10bs10 callback (the boss death / harpoon finish): funcMode 0 optionally clears the boat
// effects (debug flag); cut 3 parents the kind-1 light to Leon; the tentacle heads are animated by
// em2fTentacleMove per cut; the end restores the room state.
extern "C" void Evt_R10BS10_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        {
            // A user variable: jump.c's thread_jumps never equivalences a REG_USERVAR_P pseudo, so
            // the re-test survives (the original re-reads the flag after the effect calls).
            u32 f = pG->Debug_flg[0];

            if (f & 0x02000000) {
                pG->Room_flg[0] &= ~0x8000;
            }
        }
        break;
    case 1:
        SetSstAddAreaFlag(0);
        if (e->NowCut == 3 && e->NowFrame == 0) {
            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                cLight* l = LightMgr.getKindLight(1);

                if (l != 0) {
                    l->setParent((cModel*) mod);
                }
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod0;

                Evt_R10BSXX_Func_Pl0f(e);
                Evt_R10BSXX_Func_Em2f(e);
                if (e->GetMod(&mod0, "obm1300", 0, 0) == 1) {
                    ((cModel*) mod0)->LightInfo.EnableMask = 8;
                }
                if (e->GetMod(&mod0, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod0)->be_flag |= 0x100000;
                }
            }
            break;
        case 4:
            if (e->NowFrame == 0) {
                void* em;

                if (e->GetMod(&em, "em2f00", 0, 0) == 1) {
                    em2fTentacleMove((cEm*) em, e, 0);
                }
            }
            if (e->NowFrame == 40) {
                void* em;

                if (e->GetMod(&em, "em2f00", 0, 0) == 1) {
                    em2fTentacleMove((cEm*) em, e, 1);
                }
            }
            break;
        case 3:
        case 5:
            if (e->NowFrame == 0) {
                void* em;

                if (e->GetMod(&em, "em2f00", 0, 0) == 1) {
                    em2fTentacleMove((cEm*) em, e, 1);
                }
            }
            break;
        }
        break;
    case 2:
        SetSstAddAreaFlag(0x800);
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        break;
    case 3:
        SndRoomStrStart(1, 0, 1);
        break;
    }
}

// Event r10bs20 callback (the boss drags the boat: the rope QTE): status 3, cancel cut 9; cut 0 sets the
// stand-ins and hides Leon's parts 2/6; cut 9 frame 100 starts the action-button prompt 0x29 that the
// count in W->count scores; the outcome selects s21 (escaped) or s22 (pulled under).
extern "C" void Evt_R10BS20_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 9;
        break;
    case 1:
        SetSstAddAreaFlag(0);
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                Evt_R10BSXX_Func_Pl0f(e);
                Evt_R10BSXX_Func_Em2f(e);
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        case 9:
            if (e->NowFrame == 100) {
                e->BeginActBtn(0x29);
            }
            break;
        }
        break;
    case 2:
        e->EndActBtn();
        r10b_work->count = e->GetActBtnCount();
        SetSstAddAreaFlag(0x800);
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        break;
    case 3:
        EvtMgr.EvtSndStrPlay(EvtMgr.GetNowExeEvtNamePtr(), 1, 0x1D, 1, 0.0f);
        break;
    }
}

// Event r10bs21 callback (QTE passed: Leon cuts the rope): boat stand-in setup on cut 0, sea area flag
// 0x800 restored at the end.
extern "C" void Evt_R10BS21_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        break;
    case 1:
        SetSstAddAreaFlag(0);
        if (e->NowCut == 0 && e->NowFrame == 0) {
            Evt_R10BSXX_Func_Pl0f(e);
            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 2, 0);
                ModelInfoSetTrans((cModel*) mod, 6, 0);
            }
        }
        break;
    case 2:
        SetSstAddAreaFlag(0x800);
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        break;
    }
}

// Event r10bs22 callback (QTE failed: Leon is pulled into the lake, game over): hides object 0x59, the
// boat stand-in on cut 0, Leon's parts per cut.
extern "C" void Evt_R10BS22_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
            EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        SmdSetTrans(0x59, 0);
        break;
    case 1:
        SetSstAddAreaFlag(0);
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod;

                Evt_R10BSXX_Func_Pl0f(e);
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        case 1:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 1);
                    ModelInfoSetTrans((cModel*) mod, 6, 1);
                }
            }
            break;
        case 2:
            break;
        }
        break;
    case 2:
        SetSstAddAreaFlag(0x800);
        if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
            r10b_effDelete(2);
            r10b_effDelete(3);
        }
        SmdSetTrans(0x59, 1);
        break;
    }
}

// The player model of the boat events: no shadow, the parts 3 (the harpoon) hidden.
extern "C" void Evt_R10BSXX_Func_Pl0f(Event* e)
{
    void* mod;

    if (e->GetMod(&mod, "pl0f00", 0, 0) == 1) {
        cParts* p;

        ((cModel*) mod)->be_flag |= 0x80;
        ((cModel*) mod)->LightInfo.EnableMask = 4;
        p = ((cModel*) mod)->getPartsPtr(3);
        p->scale.x = 0.0f;
        p->scale.y = 0.0f;
        p->scale.z = 0.0f;
    }
}

// Fetch the boss event model em2f00 (the lookup registers it with the event; the pointer is unused).
extern "C" void Evt_R10BSXX_Func_Em2f(Event* e)
{
    void* mod;

    e->GetMod(&mod, "em2f00", 0, 0);
}

// The five fish (enemy 0x27) of the lake.
static void r10b_setEm()
{
    EmListData d;

    d.rot[0] = 0;
    d.rot[2] = 0;
    d.id = 0x27;
    d.pos[0] = -0xFE4;
    d.pos[1] = -0x82;
    d.pos[2] = 0x81C;
    d.rot[1] = -0xBBB;
    d.type = 0;
    d.set = 0;
    d.flag = 0;
    d.Character = 0;
    d.hp = 1000;
    d.Guard_r = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0x1090;
    d.pos[1] = -0x82;
    d.pos[2] = 0xA14;
    d.rot[1] = -0x19F4;
    d.type = 0;
    d.set = 0;
    d.flag = 0;
    d.Character = 0;
    d.hp = 1000;
    d.Guard_r = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0x10D8;
    d.pos[1] = -0x82;
    d.pos[2] = 0x766;
    d.rot[1] = -0x19F4;
    d.type = 0;
    d.set = 0;
    d.flag = 0;
    d.Character = 0;
    d.hp = 1000;
    d.Guard_r = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0x105E;
    d.pos[1] = -0x82;
    d.pos[2] = 0x8F5;
    d.rot[1] = -0xBBB;
    d.type = 0;
    d.set = 0;
    d.flag = 0;
    d.Character = 0;
    d.hp = 1000;
    d.Guard_r = 0;
    EmSetEvent(&d);

    d.id = 0x27;
    d.pos[0] = -0xF6D;
    d.pos[1] = -0x82;
    d.pos[2] = 0x93A;
    d.rot[1] = -0xBBB;
    d.type = 0;
    d.set = 0;
    d.flag = 0;
    d.Character = 0;
    d.hp = 1000;
    d.Guard_r = 0;
    EmSetEvent(&d);
}

// the split object's .data is 8-aligned
asm(".section .data; .balign 8");
