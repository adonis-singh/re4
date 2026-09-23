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
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj18.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "esp.h"
#include "snd.h"
#include "TexRender.h"
#include "db_log.h"

// Room 4-05 (D:/Bio4/Prog/r405.cpp): the Ada mine chapter start (s00 event), its enemy waves and
// the two water render targets.

struct R405Work {
    TexRenderMng* tex[2];   // 0x00
    u32 cnt;                // 0x08  enemy waves set by R405Main
    int timer;              // 0x0C  frames until the next wave check
};


static u8 r405_texTbl0[0x20];
static u8 r405_texTbl1[0x20];
static R405Work* r405_work;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r405_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

void st4_initAdaGame();   // st4.cpp


static void snd_tbl_set();
void setTexRender();
static void R405ExecEventS00();
extern "C" void Evt_R405S00_Func(Event* e);
static void em_set();
extern "C" cEm* R405_EmSetEvent(EmListData* d);
static void em_set3();
static void r405_StrCheck();

// Room exit hook: BGM table 0x405 set 3 for the next room.
static void snd_tbl_set()
{
    SndBgmTblSet(0x405, 3);
}

// Room init (Assignment Ada starts here): the Ada game flag (st4_initAdaGame), game points reset, the
// s00 / s99 callback; areas 6/8 = the first wave until Room_flg bit 1, area 9 = the second until bit 2;
// the s00 event on the first visit (pre-loaded with the enemy of ESL 0); water hit effects; Ada's
// (pl_type 2) or Leon's room motions; the water render targets; the exit hook; coming from r406 the
// gate 0x42 is posed raised.
void R405Init()
{
#line 69 "D:/Bio4/Prog/r405.cpp"
    r405_work = (R405Work*) MEM_CALLOC(sizeof(R405Work), 1, 0xd);
    st4_initAdaGame();
    void* zero = 0;
    GamePointInit(1);
    EvtMgr.SetFunc("evt_r405s00_func", (void*) Evt_R405S00_Func);
    EvtMgr.SetFunc("evt_r405s99_func", (void*) Evt_R405S00_Func);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) em_set, 0, 1);
        SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) em_set, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) em_set3, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceExec(0x12, (TaskFunc) R405ExecEventS00, 0, 0, SCE_PRIO_DEF_2, 0);
        EvtMgr.EvtReadAram("event/evd/r405s00.evd", (u8) GetEmIdFromList(0), 0, 0, 0);
    }
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r405_eff_info);
    if (pG->pl_type == 2) {
        PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23),
                       ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 0,
                       zero, zero, zero, zero);
    } else {
        PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0, zero, zero, zero, zero);
    }
    setTexRender();
    SceSetRoomExitFunc(snd_tbl_set, 0);
    if (pG->room_id_prev == 0x406) {
        SmdGetObjPtr(0x42)->be_flag |= 0x20;
        SmdGetObjPtr(0x42)->pos.y += 3000.0f;
    }
}

// Per frame after the first wave (Room_flg bit 1): up to five refills — every 240 frames, with eight
// or fewer alive, two alerted Ganados from the list pair of the zone the player is in (Room_flg[2]
// 0x20000000 / 0x10000000 / 0x40000000).
void R405Main()
{
    if (RsfCheck(G_ROOM_ID, 1)) {
        if (r405_work->cnt <= 4) {
            if (r405_work->timer <= 0) {
                if ((u32) SceCountEmAlive(0x10, 0x20) <= 8) {
                    if (pG->Room_flg[2] & 0x20000000) {
                        R405_EmSetEvent(&pG->Em_list[0x10]);
                        R405_EmSetEvent(&pG->Em_list[0x11]);
                        r405_work->cnt++;
                        r405_work->timer = 240;
                    } else if (pG->Room_flg[2] & 0x10000000) {
                        R405_EmSetEvent(&pG->Em_list[0x13]);
                        R405_EmSetEvent(&pG->Em_list[0x14]);
                        r405_work->cnt++;
                        r405_work->timer = 240;
                    } else if (pG->Room_flg[2] & 0x40000000) {
                        R405_EmSetEvent(&pG->Em_list[0x25]);
                        R405_EmSetEvent(&pG->Em_list[0x26]);
                        r405_work->cnt++;
                        r405_work->timer = 240;
                    }
                }
            } else {
                r405_work->timer--;
            }
        }
    }
}

// The water surface: two render targets blended into the water objects.
void setTexRender()
{
    cObj* obj;
    u8* tbl0 = r405_texTbl0;
    u8* tbl1 = r405_texTbl1;

    if (GetTexRenderMgr(&r405_work->tex[0])) {
        tbl0[0] = 1;
        tbl0[1] = 0;
        tbl0[4] = 0xF7;
        tbl0[5] = r405_work->tex[0]->m_Tex_no;
        r405_work->tex[0]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r405_work->tex[0]->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "R300Init() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xC);
    obj->pModelInfo->setTexBlendTbl(tbl0);
    obj->pModelInfo->setBlendRatio(0xFF);
    if (GetTexRenderMgr(&r405_work->tex[1])) {
        tbl1[0] = 1;
        tbl1[1] = 0;
        tbl1[4] = 0xF7;
        tbl1[5] = r405_work->tex[1]->m_Tex_no;
        r405_work->tex[1]->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 4, r405_work->tex[1]->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "R300Init() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0xE);
    obj->pModelInfo->setTexBlendTbl(tbl1);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
}

// Once (Room_flg bit 0): System_flg 0x400, event r405s00 (Ada's arrival), the stream, BGM table 0x405
// set 2 with both BGMs, message 0.
static void R405ExecEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceEventStart(0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SceSleep(1);
        EvtMgr.EvtReadExec("event/evd/r405s00.evd", (u8) GetEmIdFromList(0), EvtReadFlagNone);
        SceEventEnd(0);
        SndRoomStrStart(1, 0, 1);
        SndBgmTblSet(0x405, 2);
        SndRoomBgmStart(0, 0);
        SndRoomBgmStart(1, 0);
        SceSleep(2);
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    }
}

// Event r405s00 callback: the Ada model pl0c00 gets light mask 1 and its chained child object shown on cut 0.
extern "C" void Evt_R405S00_Func(Event* e)
{
    void* mod;

    if (e->FuncType == 1) {
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0c00", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 1;
                }
                if (e->GetMod(&mod, "pl0c00", 0, 0) == 1) {
                    Obj18Work* w = OBJ18_WK((cObj18*) mod);

                    if (w && w->pObjChain) {
                        OBJ18_WK((cObj18*) mod)->ObjChainFlagCommon |= 0x04000000;
                        w->pObjChain->be_flag &= ~2;
                    }
                }
            }
        }
    }
}

// The first wave: three enemies walk in while the camera shows the gate rising.
static void em_set()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        u32 i;

        RsfSet(G_ROOM_ID, 1);
        SndRoomStrStop(3);
        SndBgmTblSet(0x405, 1);
        SndRoomStrStart(1, 0, 1);
        SceExec(0x12, (TaskFunc) r405_StrCheck, 0, 0, SCE_PRIO_DEF_2, 0);
        cEmWrap em0;
        cEmWrap em1;
        cEmWrap em2;
        em0.setEm(0xC, -1, 1, 1, 1);
        em1.setEm(0xD, -1, 1, 1, 1);
        em2.setEm(0xE, -1, 1, 1, 1);
        em0.setNoSuspend(1);
        em1.setNoSuspend(1);
        em2.setNoSuspend(1);
        em0.setGoto(&pPL->pos, 7);
        em1.setGoto(&pPL->pos, 7);
        em2.setGoto(&pPL->pos, 7);
        SceEventStart(0);
        CamCtrl.CutCall(0xE);
        SmdGetObjPtr(0x42)->be_flag |= 0x20;
        SndCall(6, 0xD, &SmdGetObjPtr(0x42)->pos, 0, 0, 0);
        for (i = 0; i < 25; i++) {
            SmdGetObjPtr(0x42)->pos.y += 120.0f;
            SceSleep(1);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        em0.setNoSuspend(0);
        em1.setNoSuspend(0);
        em2.setNoSuspend(0);
    }
}

// EmSetEvent that returns the Ganado already alerted (setFindPL).
extern "C" cEm* R405_EmSetEvent(EmListData* d)
{
    cEm* em = EmSetEvent(d);

    if (em) {
        ((cEmGanado*) em)->setFindPL();
    }
    return em;
}

// The second wave: up to three enemies depending on how many are alive.
static void em_set3()
{
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        RsfSet(G_ROOM_ID, 2);
        u32 n = SceCountEmAlive(0x10, 0x20);
        cEmWrap em0;
        cEmWrap em1;
        cEmWrap em2;

        if (n <= 10) {
            em0.setEm(0x22, -1, 1, 1, 1);
            em0.setGoto(&pPL->pos, 7);
        }
        if (n <= 9) {
            em1.setEm(0x23, -1, 1, 1, 1);
        }
        if (n <= 8) {
            em2.setEm(0x24, -1, 1, 1, 1);
        }
    }
}

// The battle stream: on while an enemy sees the player.
static void r405_StrCheck()
{
    int on = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else if (on == 1) {
            SndRoomStrStop(3);
            on = 0;
        }
        SceSleep(1);
    }
}
