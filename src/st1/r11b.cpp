#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj1c.h"
#include "em.h"
#include "em_set.h"
#include "player.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "flr_at.h"
#include "TexRender.h"
#include "rnd.h"

// Room 1-1B (D:/Bio4/Prog/r11b.cpp): the lake; the boat, the floating islands, the lake water
// rendered to texture, the Ganado ambush on the shore and the s00 event (Del Lago).

struct R11bWork {
    cEm* em[9];           // 0x00  the shore Ganado (7 in the event, 9 after EmSetChange)
    cEm* boat;            // 0x24  the boat (list entry 0x3C)
    TexRenderMng* tex[2]; // 0x28  water render targets
    u8 texTbl[2][0x80];   // 0x30  their blend tables (TexRenderModSet)
};


static R11bWork* r11b_work;

// Pointer store through a reference: the pG load that follows stays below it.
// Scale set through references: the pG load of the following setMotion stays below the stores.
static inline void r11b_setScale(cObj* obj, f32 s)
{
    obj->scale.x = s;
    obj->scale.y = s;
    obj->scale.z = s;
}

// Hit effects of attribute type 2 (water)
static const AtEffInfo r11b_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

// The original TexRenderModRes reads the parts number from r4 although its prototype has one

static void R11b_bgm_ck();
static void r11b_ThunderFlagOn();
static void r11b_ThunderFlagOff();
static void r11b_ThunderMove();
extern "C" void EmSetChange();
static void r11b_EmEvent_exit();
static void r11b_EmEvent();
static void R11b_Event();
static void r11b_str_check();
extern "C" void Evt_R11BS00_Func(Event* e);
static void r11b_bort_pos_chk();

// Room init (the lake shore / boat dock): System_flg 0x800; JumpPoint 1 (arriving by boat) marks the
// s00 event seen (Room_flg bit 0); Scenario_flg[0] 8 / 2, Scenario_flg[1] 0x01000000, three door flags
// cleared. Water hit effects, thunder task; the boat enemy (ESL 0x3C) placed at the pier the return
// position flag (bit 2) says; the s00 event on the first visit (bit 0), else the shore Ganado list is
// rewritten (EmSetChange); until bit 1 area 3 = the shore ambush and the battle stream; the two water
// render targets on the lake and shore objects; the floating islands.
void R11bInit()
{
    Vec pos;
    Vec rot;
    Vec rot2;
    EmListData* l;
    cObj* obj = 0;   // the zero of the EstSet data arguments and the list entry's x3 (r27)
    int one = 1;     // COMPILER-DIFF: #13 (single use: update_equiv_regs moves the li next to the store)

    SysFlagOn(pG, SYS_SCISSOR_ON);
    if (pG->JumpPoint == 1) {
        RsfSet(G_ROOM_ID, 0);
    }
    R11bWork*& wp = r11b_work;   // the store's `lis` sits before the SceExec call (r30)
    SceExec(0x12, (TaskFunc) r11b_bort_pos_chk, 0, 0, SCE_PRIO_DEF_2, 0);
    ScfFlagOn(pG, SCF_R11B_END_SALAMANDER);
    // COMPILER-DIFF: candidate (sched1 issue-slot filler): the codeless asm depends on the flags
    // store (output dependence) and is issued in the idle cycle between it and the next pG reload,
    // so local-alloc's fake lifetimes of the two pG values no longer touch and both take r9 (the
    // original's `lwz r9; ... lwz r9`); without it the first load gets r11.
    asm("" : "=m"(rot2.x));
    ScfFlagOn(pG, SCF_ST1_MAP_NIGHT);
    ScfFlagOn(pG, SCF_ST1_NIGHT);
    ScfFlagOff(pG, SCF_90);
    ScfFlagOff(pG, SCF_96);
    ScfFlagOff(pG, SCF_9b);
#line 106 "D:/Bio4/Prog/r11b.cpp"
    wp = (R11bWork*) MEM_CALLOC(sizeof(R11bWork), 1, 0xd);
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r11b_eff_info);
    SceExec(0x12, (TaskFunc) r11b_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    EvtMgr.SetFunc("evt_r11bs00_func", (void*) Evt_R11BS00_Func);
    EstSet(pPL, -1, 0, 0, EFF_PL00, 2, 0x800, ESP_CORE_KIND_NONE, 0, obj);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 2, 0x800, ESP_CORE_KIND_NONE, 0, obj);
    StaFlagOn(pG, STA_ROOM_RAIN);
    if (RsfCheck(G_ROOM_ID, 0)) {
        SceExec(0x12, (TaskFunc) R11b_bgm_ck, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    l = &pG->Em_list[0x3C];
    l->set = 0;
    if (pG->room_id_prev == 0x10D && !SysFlagChk(pG, SYS_LOAD_GAME)) {
        static const Vec r11b_boatPos0 = {141127.0f, -1299.0f, -57107.0f};
        static const Vec r11b_boatRot0 = {0.0f, -0.68f, 0.0f};

        // Copy-initialisation (placement new): the template loads are `mem/s/u` like a `Vec x = tbl;`
        // declaration, so sched2 does not chain them behind the frame stores through the twice-set
        // r29/r30 (an assignment `pos = tbl` goes through the synthesized operator= and loses /u).
        new (&pos) Vec(r11b_boatPos0);
        new (&rot) Vec(r11b_boatRot0);
        l->set = one;
        r11b_work->boat = EmSetFromList2(0x3C, 0);
        pG->room_id_prev = 0x11B;
        r11b_work->boat->setPos(&pos);
        r11b_work->boat->setAng(&rot);
    } else {
        r11b_work->boat = EmSetFromList2(0x3C, 0);
        l->set = 1;
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            static const Vec r11b_boatPos1 = {127560.0f, -1300.0f, 149100.0f};
            static const Vec r11b_boatRot1 = {0.0f, 3.0898211f, 0.0f};

            pos = r11b_boatPos1;
            rot2 = r11b_boatRot1;
            r11b_work->boat->setPos(&pos);
            r11b_work->boat->setAng(&rot2);
            if (RsfCheck(G_ROOM_ID, 0) == 0) {
                RsfSet(G_ROOM_ID, 0);
                SceExec(0x12, (TaskFunc) R11b_Event, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    if (pG->room_id_prev == 0x11A) {
        RsfSet(G_ROOM_ID, 1);
        EmSetChange();
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r11b_EmEvent, 0, 1);
        SceExec(0x12, (TaskFunc) r11b_str_check, 0, 2, SCE_PRIO_DEF_2, 0);
    }
    pos.x = 60782.0f;
    pos.y = -1300.0f;
    pos.z = 45761.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 1.5f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22),
                                      ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24));
        }
    }
    pos.x = 12708.0f;
    pos.y = -1300.0f;
    pos.z = 33719.0f;
    rot.x = 0.0f;
    rot.y = 3.1415927f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 2.0f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22),
                                      ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24));
        }
    }
    pos.x = 412.0f;
    pos.y = -1300.0f;
    pos.z = 64963.0f;
    rot.x = 0.0f;
    rot.y = 1.5707964f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 1.7f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22),
                                      ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24));
        }
    }
    pos.x = 53338.0f;
    pos.y = -1300.0f;
    pos.z = 70475.0f;
    rot.x = 0.0f;
    rot.y = 0.7853982f;
    rot.z = 0.0f;
    {
        cObj* o = SetFloatIsland(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);

        if (o) {
            r11b_setScale(o, 1.5f);
            ((cObj1c*) o)->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22),
                                      ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24));
        }
    }
    TexRenderInit(&r11b_work->tex[0], 0xE0, 2);
    TexRenderInit(&r11b_work->tex[1], 0xE0, 2);
    FlrAtSetDefVal(0, 0, 3);
}

// Per-frame room main: nothing.
void R11bMain()
{
}

// Placeholder task (a single SceSleep): the BGM check of this room was compiled out.
static void R11b_bgm_ck()
{
    SceSleep(1);
}

// Lightning on / off: the sky object's colour.
static void r11b_ThunderFlagOn()
{
    SmdGetObjPtr(0x2B)->pModelInfo->color[0] = 0xDA;
    SmdGetObjPtr(0x2B)->pModelInfo->color[1] = 0xF1;
    SmdGetObjPtr(0x2B)->pModelInfo->color[2] = 0xFF;
}

// Lightning off: the sky object 0x2B back to its dark colour (0x18/0x19/0x1A).
static void r11b_ThunderFlagOff()
{
    SmdGetObjPtr(0x2B)->pModelInfo->color[0] = 0x18;
    SmdGetObjPtr(0x2B)->pModelInfo->color[1] = 0x19;
    SmdGetObjPtr(0x2B)->pModelInfo->color[2] = 0x1A;
}

// Thunder every 90..235 frames.
static void r11b_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    EffSetToolStateCallBack(0, r11b_ThunderFlagOn, r11b_ThunderFlagOff);
    for (;;) {
        if (cnt == 0) {
            EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, 0, 0);
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}

// Moves the shore Ganado list entries to the pier for the return from 1-1A.
#define EM_LIST_S(no) (&pG->Em_list[no])
// Rewrite ESL entries 0x40/0x41/0x3E/0x3F (the shore Ganados) to their post-event positions near the
// pier, un-set and alive, so they spawn there on later visits.
extern "C" void EmSetChange()
{
    EmListData* l;

    l = EM_LIST_S(0x40);
    l->be_flag = 1;
    l->flag |= 0x40000000;
    l->pos[0] = -5972;
    l->pos[1] = 267;
    l->pos[2] = -1348;
    l->set = 0;
    l = EM_LIST_S(0x41);
    l->be_flag = 1;
    l->set = 0;
    l->pos[0] = -5582;
    l->pos[1] = 394;
    l->pos[2] = -1958;
    l = EM_LIST_S(0x3E);
    l->be_flag = 1;
    l->set = 0;
    l->pos[0] = -6060;
    l->pos[1] = 386;
    l->pos[2] = -2616;
    l = EM_LIST_S(0x3F);
    l->be_flag = 1;
    l->set = 0;
    l->pos[0] = -6440;
    l->pos[1] = 375;
    l->pos[2] = -2932;
}

// End of the ambush cutscene: swap the seven event Ganados for the four repositioned list entries,
// drop the effect, camera back, SceEventEnd.
static void r11b_EmEvent_exit()
{
    EmSetChange();
    EmMgr.destroy(r11b_work->em[0]);
    EmMgr.destroy(r11b_work->em[1]);
    EmMgr.destroy(r11b_work->em[2]);
    EmMgr.destroy(r11b_work->em[3]);
    EmMgr.destroy(r11b_work->em[4]);
    EmMgr.destroy(r11b_work->em[5]);
    EmMgr.destroy(r11b_work->em[6]);
    r11b_work->em[0] = EmSetFromList2(0x40, 1);
    r11b_work->em[1] = EmSetFromList2(0x41, 1);
    r11b_work->em[7] = EmSetFromList2(0x3E, 1);
    r11b_work->em[8] = EmSetFromList2(0x3F, 1);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}


// Area 3 once (Room_flg bit 1): the shore ambush cutscene — seven Ganados (ESL 0x40..0x46) with torches
// appear while stream 0x24 plays and Leon is placed at the shore; player-cancellable.
static void r11b_EmEvent()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        r11b_work->em[0] = EmSetFromList2(0x40, 1);
        r11b_work->em[1] = EmSetFromList2(0x41, 1);
        r11b_work->em[2] = EmSetFromList2(0x42, 1);
        r11b_work->em[3] = EmSetFromList2(0x43, 1);
        r11b_work->em[4] = EmSetFromList2(0x44, 1);
        r11b_work->em[5] = EmSetFromList2(0x45, 1);
        r11b_work->em[6] = EmSetFromList2(0x46, 1);
        r11b_work->em[0]->setNoSuspend(1);
        r11b_work->em[1]->setNoSuspend(1);
        r11b_work->em[2]->setNoSuspend(1);
        r11b_work->em[3]->setNoSuspend(1);
        r11b_work->em[4]->setNoSuspend(1);
        r11b_work->em[5]->setNoSuspend(1);
        r11b_work->em[6]->setNoSuspend(1);
        SceEventStart(0);
        SndStrReq(1, 0x24, 0x80000003, 0, 0, 0.0f);
        pPL->setNoSuspend(1);
        pPL->setPos(-60735.0f, 2008.0f, -8455.0f);
        pPL->setAng(0.0f, 2.64f, 0.0f);
        EstSet(r11b_work->em[0], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(r11b_work->em[1], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(r11b_work->em[2], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(r11b_work->em[3], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(r11b_work->em[4], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(r11b_work->em[5], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        EstSet(r11b_work->em[6], -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        SceSetEventCancel(1, (TaskFunc) r11b_EmEvent_exit, 0, -1, 1);
        CamCtrl.CutCall(3);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(4);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(5);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(6);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r11b_EmEvent_exit();
    }
}

// The s00 event on the first visit (skipped after the game was already saved here).
static void R11b_Event()
{
    int seen;

    SceSleep(1);
    seen = 0;
    SysFlagOn(pG, SYS_SCREEN_STOP);
    if (SysFlagChk(pG, SYS_START_EVT_SKIP)) {
        seen = 1;
    }
    StaFlagOn(pG, STA_CAMERA_SET_ROOM);
    if (!SysFlagChk(pG, SYS_START_EVT_SKIP)) {
        EvtMgr.EvtReadExec("event/evd/r11bs00.evd", 0, EvtReadFlagFadeIn);
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    StaFlagOff(pG, STA_CAMERA_SET_ROOM);
    SndBgmTblSet(0x11B, 1);
    SceExec(0x12, (TaskFunc) R11b_bgm_ck, 0, 0, SCE_PRIO_DEF_2, 0);
    if (seen == 0) {
        OpeSetOpenTerm(8, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (SysFlagChk(pG, SYS_START_EVT_SKIP)) {
        SndRoomBgmStart(0, 30);
    }
}

// Battle stream while shore Ganado (type 0x22) are alive.
static void r11b_str_check()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        for (;;) {
            if (RsfCheck(G_ROOM_ID, 1) == 0) {
                SceSleep(1);
            } else {
                break;
            }
        }
        SndRoomStrStart(1, 0, 1);
        SceSleep(30);
        for (;;) {
            int n = 0;
            u32 i;

            for (i = 0; i < EmMgr.getArrayNum(); i++) {
                cEm* em = EmMgr.fastAt(i);

                if (em->id == 0x22 && em->hp > 0 && em->isAlive()) {
                    n++;
                }
            }
            if (n == 0) {
                break;
            }
            if (StaFlagChk(pG, STA_PL_BOAT)) {
                break;
            }
            SceSleep(1);
        }
        SndRoomStrStop(3);
    }
}


// Water render setup of the event's player stand-in: parts 6 (lake) and 7 / 8 (the two shores).
static inline void r11b_evtTexRenderSet(Event* e, void*& mod, int a, int b)
{
    if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
        TexRenderModSet((cModel*) mod, 6, r11b_work->texTbl[0], r11b_work->tex[0], 0, 0, 1, 1, 1.0f);
        TexRenderModSet((cModel*) mod, 7, r11b_work->texTbl[1], r11b_work->tex[1], 0, 0, 1, 1, 1.0f);
        TexRenderModSet((cModel*) mod, 8, r11b_work->texTbl[1], r11b_work->tex[1], 0, 0, 1, 1, 1.0f);
        ModelInfoRefrectOn((cModel*) mod, 6);
        ModelInfoRefrectOn((cModel*) mod, 7);
        ModelInfoRefrectOn((cModel*) mod, 8);
        ModelInfoSetTrans((cModel*) mod, 7, a);
        ModelInfoSetTrans((cModel*) mod, 8, b);
    }
}

// Drop the event's water effects bound to the two render targets' masks.
static inline void r11b_evtEffDelete()
{
    EffectEspDelete(r11b_work->tex[0]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
    EffectEspgenDelete(r11b_work->tex[0]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
    EffectEfmDelete(r11b_work->tex[0]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
    EffectEspDelete(r11b_work->tex[1]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
    EffectEspgenDelete(r11b_work->tex[1]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
    EffectEfmDelete(r11b_work->tex[1]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
}

// Event r11bs00 callback (two Ganados dump the officer's body in the lake; Del Lago takes them): hides
// object 0x7C; fade-in on cut 0 unless skipped; per-cut splash / ripple effects (skipped when the event
// is being skipped) and the water render setup on the player stand-in; the end restores the shore.
extern "C" void Evt_R11BS00_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0x7C, 0);
        break;
    case 1:
        SetSstAddAreaFlag(2);
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                int skip = EvtSkipCk(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 30, 0, 0);
                }
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x01000000;
                }
            }
            break;
        case 2:
            if (e->NowFrame == 0x84) {
                int skip = EvtSkipCk(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            break;
        case 3:
            if (e->NowFrame == 0x55) {
                int skip = EvtSkipCk(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            break;
        case 4:
            if (e->NowFrame == 0x26) {
                int skip = EvtSkipCk(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            break;
        case 5:
            if (e->NowFrame == 0x5D) {
                int skip = EvtSkipCk(e);

                if (skip == 0) {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            break;
        case 8: {
            int skip = EvtSkipCk(e);

            if (skip == 0) {
                SetNearClipDist(1.0f);
            }
            if (e->NowFrame == 0x68) {
                int skip2 = EvtSkipCk(e);

                if (skip2 == 0) {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_NONE, 0, 0);
                }
            }
            break;
        }
        }
        switch (e->NowCut) {
        case 6:
            if (e->NowFrame == 0) {
                r11b_evtTexRenderSet(e, mod, 1, 0);
                r11b_evtEffDelete();
                EstSet(0, -1, 0, 0, EFF_ROOM, 6, r11b_work->tex[1]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
            }
            break;
        case 7:
            if (e->NowFrame == 0) {
                r11b_evtTexRenderSet(e, mod, 0, 1);
                r11b_evtEffDelete();
                EstSet(0, -1, 0, 0, EFF_ROOM, 7, r11b_work->tex[1]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
            }
            break;
        case 8:
            if (e->NowFrame == 0) {
                r11b_evtTexRenderSet(e, mod, 1, 0);
                r11b_evtEffDelete();
                EstSet(0, -1, 0, 0, EFF_ROOM, 8, r11b_work->tex[1]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 9, r11b_work->tex[0]->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    TexRenderModRes((cModel*) mod, 6);
                    TexRenderModRes((cModel*) mod, 7);
                    TexRenderModRes((cModel*) mod, 8);
                }
                r11b_evtEffDelete();
            }
            break;
        }
        if (pG->game_costume == 1 && e->NowFrame == 0) {
            if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 7, 0);
                ModelInfoSetTrans((cModel*) mod, 8, 0);
            }
        }
        break;
    case 2:
        SetSstAddAreaFlag(0);
        SmdSetTrans(0x7C, 1);
        break;
    }
}

// Which pier the boat is nearer to when it stops decides the return position (room flag 2).
static void r11b_bort_pos_chk()
{
    int riding = 0;

    if (StaFlagChk(pG, STA_PL_BOAT)) {
        riding = 1;
    }
    for (;;) {
        if (riding) {
            if (!StaFlagChk(pG, STA_PL_BOAT)) {
                Vec pos = r11b_work->boat->pos;
                Vec pierA = {-49902.0f, -700.0f, 22743.0f};
                Vec pierB = {126064.0f, -700.0f, 148628.0f};
                f32 dA;
                f32 dB;

                riding = 0;
                dA = (pos.x - pierA.x) * (pos.x - pierA.x) + (pos.z - pierA.z) * (pos.z - pierA.z);
                dB = (pos.x - pierB.x) * (pos.x - pierB.x) + (pos.z - pierB.z) * (pos.z - pierB.z);
                if (dA < dB) {
                    RsfSet(G_ROOM_ID, 2);
                } else {
                    RsfClear(G_ROOM_ID, 2);
                }
            }
        } else if (StaFlagChk(pG, STA_PL_BOAT)) {
            riding = 1;
        }
        SceSleep(1);
    }
}
