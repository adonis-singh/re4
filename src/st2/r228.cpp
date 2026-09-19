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
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em38.h"
#include "em_wrap.h"
#include "player.h"
#include "cam_ctrl.h"
#include "cockpit.h"
#include "shadow.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-28 (D:/Bio4/Prog/r228.cpp): Salazar's throne room; the s00/s01/s02 event chain, the
// boss fight and the render-to-texture setup.

struct R228Work {
    TexRenderMng* tex[2];   // 0x000  the two room render targets
    u8 pad_8[0x100 - 0x8];
    int eff;                // 0x100  EspPullCoreKind of the fight effects
    cObj* obj77;            // 0x104
    cObj* obj79;            // 0x108
    cObj* obj76;            // 0x10C
    cObj* obj78;            // 0x110
    ScePrim* se;            // 0x114  the neck-down camera task
    int eff2;               // 0x118  EspPullCoreKind of the event effect
    cSat* sat;              // 0x11C
    cSat* eat;              // 0x120
    TexRenderMng* texEvt;   // 0x124  event render target
    u8 texTbl[0x384];       // 0x128  its blend table
};

// One-member struct: every store through the work reloads the pointer.
struct R228WorkPtr {
    R228Work* p;
};

static u8 r228_texTbl0[0x20];
static u8 r228_texTbl1[0x20];
static R228WorkPtr r228_work;

static void r228_execSalazarNeckDown();
static void r228_checkSalazarBattle();
static void r228_execEvent00();
void r228_initEvent00();
extern "C" void Evt_R228S00_Func(Event* e);
extern "C" void Evt_R228S01_Func(Event* e);
extern "C" void Evt_R228S02_Func(Event* e);
void setTexRender();

// Stores through references: the following pG / pPL load stays below the store.
static inline void FSetP(f32& d, f32 v) { d = v; }
static inline void PSet(cObj*& d, cObj* v) { d = v; }
static inline void PSetSat(cSat*& d, cSat* v) { d = v; }
// Struct view of pPL: the load stays below a preceding store through the work pointer.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// The event skips its fades when the player skipped the event (Event::status bit 30).
static inline int r228_evtSkip(Event* e)
{
    int skip = 1;

    if ((e->StatusFlag & 0x40000000) == 0) {
        skip = 0;
    }
    return skip;
}

// Room init (Salazar's throne room): the two room render targets, the s00/s01/s02 callbacks, the fight
// effect kind, the event chain / fight setup (r228_initEvent00), and an event render target with its effect.
void R228Init()
{
#line 57 "D:/Bio4/Prog/r228.cpp"
    r228_work.p = (R228Work*) MEM_CALLOC(sizeof(R228Work), 1, 0xd);
    setTexRender();
    EvtMgr.SetFunc("evt_r228s00_func", (void*) Evt_R228S00_Func);
    EvtMgr.SetFunc("evt_r228s01_func", (void*) Evt_R228S01_Func);
    EvtMgr.SetFunc("evt_r228s02_func", (void*) Evt_R228S02_Func);
    r228_work.p->eff2 = EspPullCoreKind();
    r228_initEvent00();
    TexRenderInit(&r228_work.p->texEvt, 0xE0, 2);
    EstSet(0, -1, 0, 0, 1, 3, r228_work.p->texEvt->mask | 0x3001, 0, 0, 0);
}

// Per-frame room main: nothing.
void R228Main()
{
}

// Salazar looks down at the player (cut 14).
static void r228_execSalazarNeckDown()
{
    cEmWrap em0;
    cEmWrap em1;

    em0.setPtr(0x2C, -1, 1);
    em1.setPtr(0x28, -1, 1);
    SceSleep(45);
    SceEventStart(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    em0.setNoSuspend(1);
    em1.setNoSuspend(1);
    CamCtrl.CutCall(0xE);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    em0.setNoSuspend(0);
    em1.setNoSuspend(0);
    CamCtrl.Comeback(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventEnd(0);
    r228_work.p->se = 0;
}

// The boss fight: waits for the boss to die, then the death camera cuts 9..12.
static void r228_checkSalazarBattle()
{
    int alive;

    GamePointBossReset();
    cEmWrap boss;
    boss.setPtr(0x2C, -1, 1);
    Cckpt.m_LifeMeter.flags = (u32) boss.getPtr();
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    cEmWrap em3;
    em0.setPtr(0x28, -1, 1);
    em1.setPtr(0x29, -1, 1);
    em2.setPtr(0x2A, -1, 1);
    em3.setPtr(0x2B, -1, 1);
    SceSleep(1);
    while ((alive = boss.isActive()) != 0) {
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            cEm* e = em0.getPtr();

            if (e && ((cEm38*) e)->ckDown() == 1) {
                RsfSet(G_ROOM_ID, 2);
                r228_work.p->se = 0;
                r228_work.p->se = SceExec(0x12, (TaskFunc) r228_execSalazarNeckDown, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        SceSleep(1);
    }
    SndRoomStrStop(3);
    if (r228_work.p->se) {
        SceKill(r228_work.p->se);
    }
    r228_work.p->eff = EspPullCoreKind();
    f32 y = SatMgr.getFloor(&pPLS->pos, 600.0f, 100000.0f, 0, 0);
    FSetP(pPL->pos.y, y);
    pPL->setPos(&pPL->pos);
    SceEventStart(0);
    boss.setNoSuspend(1);
    em0.setNoSuspend(1);
    em1.setNoSuspend(1);
    em2.setNoSuspend(1);
    em3.setNoSuspend(1);
    cEm* e0 = em0.getPtr();
    cEm* eb = boss.getPtr();
    EstSet((int) e0, -1, 0, 0, 0x2E, 0x10, 1, (u8) r228_work.p->eff, 0, 0);
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EffectEspDelete(0, (u8) r228_work.p->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r228_work.p->eff, 0);
    EffectEfmDelete(0, (u8) r228_work.p->eff, 0);
    EstSet((int) eb, -1, 0, 0, 0x2E, 0x11, 1, (u8) r228_work.p->eff, 0, 0);
    CamCtrl.CutCall(0xA);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EffectEspDelete(0, (u8) r228_work.p->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r228_work.p->eff, 0);
    EffectEfmDelete(0, (u8) r228_work.p->eff, 0);
    EstSet((int) e0, -1, 0, 0, 0x2E, 0x12, 1, (u8) r228_work.p->eff, 0, 0);
    CamCtrl.CutCall(0xB);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EffectEspDelete(0, (u8) r228_work.p->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r228_work.p->eff, 0);
    EffectEfmDelete(0, (u8) r228_work.p->eff, 0);
    EstSet(0, -1, 0, 0, 0x2E, 0x13, 1, (u8) r228_work.p->eff, 0, 0);
    CamCtrl.CutCall(0xC);
    boss.destroy();
    em0.destroy();
    em1.destroy();
    em2.destroy();
    em3.destroy();
    SceDestroyEm(0x25, -1);
    {
        cObj* o50 = SmdGetObjPtr(0x32);
        cObj* o2 = SmdGetObjPtr(2);

        if (o50 && o2) {
            o50->be_flag &= ~2;
            o2->be_flag &= ~2;
        }
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EffectEspDelete(0, (u8) r228_work.p->eff, 0, 0);
    EffectEspgenDelete(0, (u8) r228_work.p->eff, 0);
    EffectEfmDelete(0, (u8) r228_work.p->eff, 0);
    SceAtSetEnable(2, 1);
    SceEventEnd(0);
    EstSet(0, -1, 0, 0, 1, 4, 0, 0, 0, 0);
    RsfSet(G_ROOM_ID, 1);
    ScfFlagOn(pG, SCF_83);
    SceAtSetEnable(0x8C, 1);
    SatMgr.destroy(r228_work.p->sat);
    EatMgr.destroy(r228_work.p->eat);
}

// Area 7: the three-part event, then the fight starts.
static void r228_execEvent00()
{
    void* zero = 0;

    RsfSet(G_ROOM_ID, 0);
    SceEventStart(0);
    EvtMgr.EvtReadExec("event/evd/r228s00.evd", 0, 0);
    if (!(pG->Room_flg[0] & 0x80000000)) {
        EvtMgr.EvtReadExec("event/evd/r228s01.evd", 0, 0);
        if (!(pG->Room_flg[0] & 0x80000000)) {
            EvtMgr.EvtReadExec("event/evd/r228s02.evd", 0, 0);
        }
    }
    SceEventEnd(0);
    EstSet(0, -1, 0, 0, 1, 2, 0x801, (u8) r228_work.p->eff2, (u32) zero, zero);
    SceAtSetEnable(8, 1);
    if (r228_work.p->obj76) {
        r228_work.p->obj76->be_flag |= 2;
    }
    if (r228_work.p->obj78) {
        r228_work.p->obj78->be_flag |= 2;
    }
    if (r228_work.p->obj77) {
        r228_work.p->obj77->be_flag &= ~2;
    }
    if (r228_work.p->obj79) {
        r228_work.p->obj79->be_flag &= ~2;
    }
    SndRoomStrStart(1, 0, 1);
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    cEmWrap em3;
    cEmWrap em4;
    em0.setEm(0x2C, -1, 1, 1, 1);
    em1.setEm(0x28, -1, 1, 1, 1);
    em2.setEm(0x29, -1, 1, 1, 1);
    em3.setEm(0x2A, -1, 1, 1, 1);
    em4.setEm(0x2B, -1, 1, 1, 1);
    setEm(0x2E, -1, 1, 1, 1);
    setEm(0x2F, -1, 1, 1, 1);
    setEm(0x30, -1, 1, 1, 1);
    setEm(0x31, -1, 1, 1, 1);
    setEm(0x32, -1, 1, 1, 1);
    TexRenderModSet(em1.getPtr(), 1, r228_work.p->texTbl, r228_work.p->texEvt, 0, 1, 1, 1, 1.0f);
    SceExec(0x12, (TaskFunc) r228_checkSalazarBattle, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Fetch the throne-room objects 0x76..0x79. Before the event (Room_flg bit 0): pre-load r228s00 to MRAM,
// area 7 = the event, area 8 off, objects 1/0x76/0x78 shown (the intact throne), item area 0x8C off.
// After it: area 8 on, objects 0x77/0x79 (the transformed set) shown, and unless bit 1 (boss dead) the
// fight watcher and Salazar's look-down.
void r228_initEvent00()
{
    r228_work.p->obj76 = SmdGetObjPtr(0x76);
    r228_work.p->obj78 = SmdGetObjPtr(0x78);
    r228_work.p->obj77 = SmdGetObjPtr(0x77);
    PSet(r228_work.p->obj79, SmdGetObjPtr(0x79));
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadMram("event/evd/r228s00.evd", 0, 0, 0, 0);
        SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r228_execEvent00, 0, 1);
        SceAtSetEnable(8, 0);
        cObj* o = SmdGetObjPtr(1);
        if (o) {
            o->be_flag &= ~2;
        }
        if (r228_work.p->obj76) {
            r228_work.p->obj76->be_flag &= ~2;
        }
        if (r228_work.p->obj78) {
            r228_work.p->obj78->be_flag &= ~2;
        }
        SceAtSetEnable(0x8C, 0);
    } else {
        SceAtSetEnable(8, 1);
        if (r228_work.p->obj77) {
            r228_work.p->obj77->be_flag &= ~2;
        }
        if (r228_work.p->obj79) {
            r228_work.p->obj79->be_flag &= ~2;
        }
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            SceExec(0x12, (TaskFunc) r228_checkSalazarBattle, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    r228_work.p->sat = 0;
    PSetSat(r228_work.p->eat, 0);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtSetEnable(2, 0);
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        PSetSat(r228_work.p->sat, SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1));
        r228_work.p->eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 1);
    } else {
        cObj* o = SmdGetObjPtr(0x32);
        if (o) {
            o->be_flag &= ~2;
        }
        o = SmdGetObjPtr(2);
        if (o) {
            o->be_flag &= ~2;
        }
    }
}

// The effect setup shared by the three events (funcMode 0).
static inline void r228_evtEffectSet()
{
    if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
        EffectDeleteAll();
        SstSet(1, 0xFFFF, 1, 0, 0x2F, 1);
        EspGenSetMoveLoop(200);
        EspGenLoopMove();
    }
}

// Event r228s00 callback (Salazar's speech, part 1): the shared effect setup; cut 0 hides object 1 and
// flags evma400a (shadow camera zeroed), frame 1 pre-loads r228s01 (Room_flg[0] 0x00200000); fades near
// the ends of cuts 2 and later; the end mode hands over to s01.
extern "C" void Evt_R228S00_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        r228_evtEffectSet();
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 0);
                if (e->GetMod(&mod, "evma400a", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x80;
                    SetShadowCamMoveSize(0.0f);
                }
            }
            if (e->NowFrame == 1) {
                EvtMgr.EvtReadAram("event/evd/r228s01.evd", 0, 0, 0, 0);
                pG->Room_flg[0] |= 0x00200000;
            }
            break;
        case 2:
            if (e->NowFrame == e->MaxFrame - 10) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 10, 0, 0);
                }
            }
            break;
        case 3:
            if (e->NowFrame == 0) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    SysFlagOn(pG, SYS_SCREEN_STOP);
                }
            }
            if (e->NowFrame == 0) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 10, 0, 0);
                }
            }
            if (e->NowFrame == 1) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    SysFlagOff(pG, SYS_SCREEN_STOP);
                }
            }
            if (e->NowFrame == e->MaxFrame - 30) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 30, 0, 0);
                }
            }
            if (e->NowFrame == e->MaxFrame - 1) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    SysFlagOn(pG, SYS_SCREEN_STOP);
                }
            }
            break;
        }
        break;
    case 2:
        ResetShadowCamMoveSize();
        SmdSetTrans(1, 1);
        break;
    case 3:
        BitOn(pG->Room_flg[0], 0x80000000);
        if (pG->Room_flg[0] & 0x00200000) {
            EvtMgr.EvtFree("event/evd/r228s01.evd");
        }
        break;
    }
}

// Event r228s01 callback (part 2): as s00 for cut 0 (pre-loads r228s02, Room_flg[0] 0x00100000) with
// fade in/out at frames 0/1; the end hands over to s02.
extern "C" void Evt_R228S01_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        r228_evtEffectSet();
        break;
    case 1:
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 0);
                if (e->GetMod(&mod, "evma400a", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x80;
                    SetShadowCamMoveSize(0.0f);
                }
            }
            if (e->NowFrame == 0) {
                EvtMgr.EvtReadAram("event/evd/r228s02.evd", 0, 0, 0, 0);
                pG->Room_flg[0] |= 0x00100000;
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 20, 0, 0);
                }
            }
            if (e->NowFrame == 1) {
                int skip = r228_evtSkip(e);

                if (skip == 0) {
                    SysFlagOff(pG, SYS_SCREEN_STOP);
                }
            }
        }
        break;
    case 2:
        ResetShadowCamMoveSize();
        SmdSetTrans(1, 1);
        break;
    case 3:
        BitOn(pG->Room_flg[0], 0x80000000);
        if (pG->Room_flg[0] & 0x00100000) {
            EvtMgr.EvtFree("event/evd/r228s02.evd");
        }
        break;
    }
}

// Event r228s02 callback (part 3, the throne transforms): object 0x25 hidden during cut 5; cut 0 hands
// scroll object 1 (scr0000) to the event; later cuts swap the throne objects 0x76/0x78 -> 0x77/0x79 and
// set the event models' flags; the end restores the room for the fight.
extern "C" void Evt_R228S02_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec ang = {0.0f, 0.0f, 0.0f};
    void* mod;
    cObj* o;

    switch (e->funcMode) {
    case 0:
        r228_evtEffectSet();
        break;
    case 1:
        if (e->NowCut == 5) {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x25, 0);
            }
        } else {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x25, 1);
            }
        }
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                o = SmdGetObjPtr(1);
                if (o) {
                    e->SetMod("scr0000", o, 5, 0, 2, 0);
                    o->setPos(&pos);
                    o->setAng(&ang);
                    o->be_flag |= 0x20;
                    e->EspSetModelPtr(o);
                }
            }
        }
        switch (e->NowCut) {
        case 0:
        case 1:
        case 2:
        case 3:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                SmdSetTrans(1, 1);
            }
            break;
        }
        switch (e->NowCut) {
        case 5:
        case 7:
            break;
        case 8:
            if (e->NowFrame == 0) {
                if (r228_work.p->obj77) {
                    r228_work.p->obj77->be_flag &= ~2;
                }
                if (r228_work.p->obj76) {
                    r228_work.p->obj76->be_flag |= 2;
                }
            }
            break;
        case 9:
            if (e->NowFrame == 0) {
                if (r228_work.p->obj79) {
                    r228_work.p->obj79->be_flag &= ~2;
                }
                if (r228_work.p->obj78) {
                    r228_work.p->obj78->be_flag |= 2;
                }
            }
            break;
        }
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "em3800", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod, 1, r228_work.p->texTbl, r228_work.p->texEvt, 0, 1, 1, 1, 1.0f);
                }
            }
        }
        break;
    case 2: {
        SmdWork* w = SmdGetWorkPtr(1);

        o = SmdGetObjPtr(1);

        if (o && w) {
            o->setPos(&w->pos);
            o->setAng(&w->rot);
        }
        SmdSetTrans(1, 1);
        break;
    }
    }
}

// TexRender blend setup of one object (the two room render targets).
#define R228_TEX_OBJ(id, tbl)                \
    obj = SmdGetObjPtr(id);                  \
    obj->pModelInfo->setTexBlendTbl(tbl);         \
    obj->pModelInfo->setBlendRatio(0xFF);

// The two render targets blended over scroll objects 2 and 3 (R228_TEX_OBJ), with their capture effects.
void setTexRender()
{
    cObj* obj;
    u8* tbl0 = r228_texTbl0;
    u8* tbl1 = r228_texTbl1;

    if (GetTexRenderMgr(&r228_work.p->tex[0])) {
        tbl0[0] = 1;
        tbl0[1] = 0;
        tbl0[4] = 0xF7;
        tbl0[5] = r228_work.p->tex[0]->texId;
        r228_work.p->tex[0]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 0, r228_work.p->tex[0]->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    if (GetTexRenderMgr(&r228_work.p->tex[1])) {
        tbl1[0] = 1;
        tbl1[1] = 0;
        tbl1[4] = 0xF7;
        tbl1[5] = r228_work.p->tex[1]->texId;
        r228_work.p->tex[1]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 1, r228_work.p->tex[1]->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    R228_TEX_OBJ(2, tbl0);
    R228_TEX_OBJ(3, tbl1);
}
