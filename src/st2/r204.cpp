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
#include "main.h"
#include "pad.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj00.h"
#include "em.h"
#include "emswitch.h"
#include "emBarred.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "math_sub.h"
#include "act_btn.h"
#include "area.h"
#include "read.h"
#include "stage.h"
#include "sscrn.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "eprintf.h"
#include "TexRender.h"
#include "st_mgr_event.h"
#include "db_log.h"

// Room 2-04 (D:/Bio4/Prog/r204.cpp): the two chandeliers the player swings on, the mob that chases
// the player out of the room, the dropping shutter and the texture-rendered object.

struct R204Work {
    cObj* chand[2];        // 0x000  chandelier scroll objects
    cEm* sw;               // 0x008
    u8 pad_C[4];
    cEm* barred[2];        // 0x010
    cEmWrap em[11];        // 0x018  the mob (enemy list 0x4A..0x54)
    u8 pad_9C[0x438 - 0x9C];
    TexRenderMng* tex;     // 0x438
    u32 cnt;               // 0x43C  frames since the chase started
    cObj* head[11];        // 0x440  the head objects riding the enemies
    u8 pad_46C[0x5A0 - 0x46C];
    u8 esp[11];            // 0x5A0  effect kinds of the head fires
    u8 pad_5AB[0x5F8 - 0x5AB];
    int cnt2;              // 0x5F8  frames the chase music waits
    u32 str;               // 0x5FC  SndStrReq handle
};

static u8 r204_texTbl[0x20];

static R204Work* r204_work;


static void door_rsf_off();
static void setTexRender();
static void r204_openBox(int id);
static void r204_openedBox(int id);
static void r204_openTana();
static void r204_openedTana();
void r204_BoxMove(cObj* obj, int opened);
void r204_BoxMove2(cObj* obj, int opened);
void r204_TanaMove(int opened);
static void r204_first_cut_exit();
static void r204_first_cut();
static void r204_nige_check();
static void door5_close();
static void r204_EventChandelier1();
static void r204_EventChandelier2();
static void r204_EventExec();
static void r204_openTerm();
void Evt_R204S00_Func(Event* e);
static void r204_checkEmDead();
static void door_move();

static const Vec r204_chandPos0 = {5437.0f, 6027.0f, -11079.0f};
static const Vec r204_chandPos1 = {-5114.0f, 6027.0f, -20975.0f};
static const Vec r204_chandRot0 = {0.0f, -1.5707964f, 0.0f};
static const Vec r204_chandRot1 = {0.0f, 1.5707964f, 0.0f};
static const Vec r204_chandOfs = {0.0f, 5826.0f, 5610.0f};

// Room init (the chandelier hall): Debug_flg[1] 0x20000; JumpPoint 1 fakes an entry from r205 Part 1.
// The s00 callback, area 2 = the chapter-end event, the terminal once the event ran (bits 0/7), the
// switch / barred door handles, areas 5/6 = the two chandelier swings. Arriving from r205 upstairs
// (Part 1) starts the mob chase (first cut once, bit 1); once the chase is on (bit 1) and not yet
// escaped (bit 2): the eleven Ganados with their torch heads and flame effects, the death watcher and
// the chase task (nige_check); else the calm layout. Areas, the water render target, box / shelf items.
void R204Init()
{
    R204Work** wp;
    void* zero;
    u32 i;
    u32 no;

    DbgFlagOn(pG, DBG_EMW_ERR_NO_DISP);
    if (pG->JumpPoint == 1) {
        pG->room_id_prev = 0x205;
        pG->Part = 1;
    }
    wp = &r204_work;
#line 98 "D:/Bio4/Prog/r204.cpp"
    *wp = (R204Work*) MEM_CALLOC(sizeof(R204Work), 1, 0xd);
    EvtMgr.SetFunc("evt_r204s00_func", (void*) Evt_R204S00_Func);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r204_EventExec, 0, 1);
    if (RsfCheck(G_ROOM_ID, 0) && RsfCheck(G_ROOM_ID, 7) == 0) {
        SceExec(0x12, (TaskFunc) r204_openTerm, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    getRoomEtcSwitch(8, &r204_work->sw, 1);
    getRoomEtcBarred(0xB, &r204_work->barred[0], 1);
    getRoomEtcBarred(6, &r204_work->barred[1], 1);
    if (r204_work->sw != 0 && r204_work->barred[0] != 0) {
        ((cEmSwitch*) r204_work->sw)->setBarred((cEmBarred*) r204_work->barred[0]);
        ((cEmSwitch*) r204_work->sw)->setBarred2nd((cEmBarred*) r204_work->barred[1]);
        ((cEmSwitch*) r204_work->sw)->setClosed();
        ((cEmBarred*) r204_work->barred[0])->setClosed();
        ((cEmBarred*) r204_work->barred[1])->setClosed();
    }
    {
        Mtx m;
        Vec pos;

        low_RotMatrix(m, (Vec*) &r204_chandRot0);
        PSMTXMultVec(m, (Vec*) &r204_chandOfs, &pos);
        PSVECAdd((Vec*) &r204_chandPos0, &pos, &pos);
        r204_work->chand[0] = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos,
                                          (Vec*) &r204_chandRot0, 0x10, 1);
        r204_work->chand[0]->be_flag |= 0x1000;
        low_RotMatrix(m, (Vec*) &r204_chandRot1);
        PSMTXMultVec(m, (Vec*) &r204_chandOfs, &pos);
        PSVECAdd((Vec*) &r204_chandPos1, &pos, &pos);
        r204_work->chand[1] = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos,
                                          (Vec*) &r204_chandRot1, 0x10, 1);
        r204_work->chand[1]->be_flag |= 0x1000;
    }
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r204_EventChandelier1, 0, 1);
    SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r204_EventChandelier2, 0, 1);
    if ((pG->room_id_prev == 0x205 && pG->Part == 1) || DebugTrg(1)) {
        ScfFlagOn(pG, SCF_R204_ASHLEY_SPLIT);
        readEmList(1);
        if (!RsfCheck(G_ROOM_ID, 1)) {
            SceExec(0x12, (TaskFunc) r204_first_cut, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        RsfSet(G_ROOM_ID, 1);
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        if (pG->room_id_prev == 0x205 && pG->Part == 0) {
            RsfSet(G_ROOM_ID, 2);
            for (no = 0x4A; no < 0x55; no++) {
                pG->Em_list[no].be_flag &= ~1;
            }
        }
        if (!RsfCheck(G_ROOM_ID, 2)) {
            r204_work->str = SndStrReq(1, 0x32, 0x80000003, 0, 0, 0.0f);
            SceExec(0x12, (TaskFunc) r204_checkEmDead, 0, 0, SCE_PRIO_DEF_2, 0);
            for (i = 0; i <= 10; i++) {
                r204_work->em[i].setEm(0x4A + i, 3, 0, 1, 1);
                if (r204_work->em[i].isAlive() == 1) {
                    Vec ofs = {0.0f, 10.0f, 179.0f};
                    Vec rot = {-0.17453292f, 0.0f, 0.0f};

                    if (i == 7) {
                        r204_work->head[i] = SetObj00(ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), &ofs, &rot);
                        r204_work->head[i]->setNoSuspend(1);
                        OyaSetObj00(r204_work->head[i], r204_work->em[7].getPtr(), 2);
                        r204_work->esp[i] = EspPullCoreKind();
                        EstSet(r204_work->head[i], -1, 0, 0, EFF_CORE, 0x2D, 0x801, r204_work->esp[i], 0, 0);
                    } else {
                        r204_work->head[i] = SetObj00(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &ofs, &rot);
                        r204_work->head[i]->setNoSuspend(1);
                        OyaSetObj00(r204_work->head[i], r204_work->em[i].getPtr(), 2);
                        r204_work->esp[i] = EspPullCoreKind();
                        EstSet(r204_work->head[i], -1, 0, 0, EFF_ROOM, 0x1F, 0x801, r204_work->esp[i], 0, 0);
                    }
                }
                if (0x4A + i == 0x51) {
                    r204_work->em[i].motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 0, 0, 5, 0);
                } else {
                    r204_work->em[i].motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 0, 0, 5, 0);
                }
            }
        }
        SmdSetTrans(0x14, 1);
        zero = NULL;
        SmdSetTrans(0x17, 1);
        SmdSetTrans(0x18, 1);
        SceAtSetEnable(7, 1);
        SceAtSetEnable(9, 1);
        SceAtSetEnable(0xA, 1);
        SceAtSetEnable(0xB, 1);
        SceAtSetEnable(0x1A, 1);
        SceAtSetEnable(0x10, 0);
        SmdSetTrans(0x3A, 0);
        SmdSetTrans(0x3B, 0);
        SceAtSetEnable(0x12, 0);
        SceAtSetEnable(0x13, 0);
        SmdGetObjPtr(0x1C)->be_flag |= 0x20;
        SmdGetObjPtr(0x1C)->ang.y = -2.72f;
        SceAtSetEnable(0xD, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, zero, zero);
        SmdSetTrans(0x3C, 0);
        SmdSetTrans(0x3D, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, zero, zero);
    } else {
        SmdSetTrans(0x14, 0);
        SmdSetTrans(0x17, 0);
        SmdSetTrans(0x18, 0);
        SceAtSetEnable(7, 0);
        SceAtSetEnable(9, 0);
        SceAtSetEnable(0xA, 0);
        SceAtSetEnable(0xB, 0);
        SceAtSetEnable(0x1A, 0);
        SmdSetTrans(0x3A, 1);
        SmdSetTrans(0x3B, 1);
        SceAtSetEnable(0x12, 1);
        SceAtSetEnable(0x13, 1);
        zero = NULL;
        EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_NONE, zero, zero);
        SceAtSetEnable(0xD, 1);
        SmdSetTrans(0x3C, 1);
        SmdSetTrans(0x3D, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_NONE, zero, zero);
    }
    if (pG->em_list_no == 3) {
        cEm* em0;
        cEm* em1;

        em0 = setEm(0x59, -1, 1, 1, 1);
        em1 = setEm(0x55, -1, 1, 1, 1);
        if (RsfCheck(G_ROOM_ID, 1)) {
            EmMgr.destroy(em0);
        } else {
            EmMgr.destroy(em1);
        }
    }
    if (ScfFlagChk(pG, SCF_R206_ASHLEY_RESCUE)) {
        SceAtSetEnable(0x11, 0);
    }
    setTexRender();
    if (!RsfCheck(G_ROOM_ID, 0)) {
        EvtMgr.EvtReadAram("event/evd/r204s00.evd", 0, 0, 0, 0);
        EmReadSearch(0x14, 0, 0);
    }
    if (SysFlagChk(pG, SYS_LOAD_GAME) && RsfCheck(G_ROOM_ID, 8)) {
        SceAtSetEnable(0xF, 1);
        SmdGetObjPtr(0x39)->be_flag |= 0x20;
        SmdGetObjPtr(0x39)->pos.y = 0.0f;
    } else {
        SmdGetObjPtr(0x39)->be_flag |= 0x20;
        SmdGetObjPtr(0x39)->pos.y = 5300.0f;
        SceAtSetEnable(0xF, 0);
    }
    SceSetRoomExitFunc(door_rsf_off, 0);
    SceExec(0x12, (TaskFunc) r204_nige_check, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Clears Room_flg bit 8 (the door-5 state) — a small area hook.
static void door_rsf_off()
{
    RsfClear(G_ROOM_ID, 8);
}

// The water render target blended over object 0x18; also links item areas 8 / 0x16 to etc deaths and
// registers the box / shelf item events.
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r204_texTbl;

    if (GetTexRenderMgr(&r204_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r204_work->tex->m_Tex_no;
        r204_work->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r204_work->tex->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x18);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(2);
    SceAtLinkEtcDead(8, 0x2B, 1);
    SceAtLinkEtcDead(0x16, 2, 1);
    SceSetItemEvent(8, 0x88, 3, 5, r204_openBox, r204_openedBox, 0, 0);
    SceSetItemEvent(0x16, 0x81, 4, 6, r204_openBox, r204_openedBox, 1, 0);
    SceSetItemEvent(0x17, 0x87, 5, 7, (void (*)(int)) r204_openTana, (void (*)(int)) r204_openedTana, 0, 0);
}

// Per frame during the chase (Room_flg bit 1): when barred door 1 opens (Room_flg[0] bit 31 once) every
// Ganado is alerted and six run to the door; the escape counter (cnt2, Room_flg[2] 0x40000000) is
// debug-printed and ends the chase (bit 2) when it runs out.
void R204Main()
{
    u32 i;
    u32 no;

    if (RsfCheck(G_ROOM_ID, 1)) {
        if (!(pG->Room_flg[0] & 0x80000000) && ((cEmBarred*) r204_work->barred[1])->ckOpen() == 1) {
            pG->Room_flg[0] |= 0x80000000;
            for (i = 0; i <= 10; i++) {
                r204_work->em[i].setFindPL();
            }
            Vec pos = {-8927.0f, 6000.0f, -20216.0f};
            r204_work->em[0].setGoto(&pos, 0xC);
            r204_work->em[2].setGoto(&pos, 0xC);
            r204_work->em[4].setGoto(&pos, 0xC);
            r204_work->em[6].setGoto(&pos, 0xC);
            r204_work->em[8].setGoto(&pos, 0xC);
            r204_work->em[9].setGoto(&pos, 0xC);
        }
        if (!(pG->Room_flg[0] & 0x40000000) && !RsfCheck(G_ROOM_ID, 2)) {
            if (pG->Room_flg[2] & 0x40000000) {
                SceDebugDisp("CNT[%d/%d]", r204_work->cnt2, 0x10E);
                r204_work->cnt2++;
                if (r204_work->cnt2 == 0x10E) {
                    pG->Room_flg[2] |= 0x80000000;
                }
            } else {
                r204_work->cnt2 = 0;
            }
            if (SceCkFindPL(0) == 1 || (pG->Room_flg[2] & 0x80000000) || (StaFlagChk(pG, STA_PL_FIRE))) {
                pG->Room_flg[0] |= 0x40000000;
                RsfSet(G_ROOM_ID, 2);
                r204_work->cnt = 1;
                SndStrReq(r204_work->str, 4, 200, 0);
                for (no = 0x4A; no < 0x55; no++) {
                    pG->Em_list[no].be_flag &= ~1;
                }
            }
        }
    }
    if (!RsfCheck(G_ROOM_ID, 6)) {
        if (((cEmSwitch*) r204_work->sw)->ckSwitch() == 1 || DebugTrg(0)) {
            RsfSet(G_ROOM_ID, 6);
            SceExec(0x12, (TaskFunc) door_move, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
}

// Item-event opener: box 0 (object 0x36) lid swings, box 1 (object 0x37) tilts open.
static void r204_openBox(int id)
{
    if (id == 0) {
        r204_BoxMove(SmdGetObjPtr(0x36), 0);
    }
    if (id == 1) {
        r204_BoxMove2(SmdGetObjPtr(0x37), 0);
    }
}

// Item-event "already opened": pose box `id` open.
static void r204_openedBox(int id)
{
    if (id == 0) {
        r204_BoxMove(SmdGetObjPtr(0x36), 1);
    }
    if (id == 1) {
        r204_BoxMove2(SmdGetObjPtr(0x37), 1);
    }
}

// Item-event opener: the shelf (tana) doors swing open.
static void r204_openTana()
{
    r204_TanaMove(0);
}

// Item-event "already opened": the shelf posed open.
static void r204_openedTana()
{
    r204_TanaMove(1);
}

// COMPILER-DIFF: the `const f32` locals are declared after the flag write / inside the loop body and the
// final store sits inside the break block; this is what keeps the constants out of callee-saved FPRs.
void r204_BoxMove(cObj* obj, int opened)
{
    if (opened == 0) {
        SndCall(6, 0x5B, &obj->pos, 0, 0, 0);
    }
    obj->be_flag |= 0x20;
    const f32 lim = -1.73f;
    if (opened == 1) {
        obj->ang.z = lim;
    } else {
        if (opened == 0) {
            SndCall(6, 1, 0, 0, 0, 0);
        }
        while (1) {
            const f32 spd = -0.05f;

            obj->ang.z += spd;
            if (obj->ang.z < lim) {
                obj->ang.z = lim;
                break;
            }
            SceSleep(1);
        }
    }
}

// The tilting box: ang.x turns to -1.73 rad in -0.05 steps with the lid SE (opened == 1 snaps).
void r204_BoxMove2(cObj* obj, int opened)
{
    if (opened == 0) {
        SndCall(6, 0x5B, &obj->pos, 0, 0, 0);
    }
    obj->be_flag |= 0x20;
    const f32 lim = -1.73f;
    if (opened == 1) {
        obj->ang.x = lim;
    } else {
        if (opened == 0) {
            SndCall(6, 1, 0, 0, 0, 0);
        }
        while (1) {
            const f32 spd = -0.05f;

            obj->ang.x += spd;
            if (obj->ang.x < lim) {
                obj->ang.x = lim;
                break;
            }
            SceSleep(1);
        }
    }
}

// The shelf doors (objects 0x34/0x35) swing apart to +-2.83 rad in 0.09 steps with SE (opened == 1 snaps).
void r204_TanaMove(int opened)
{
    cObj* a;
    cObj* b;

    a = SmdGetObjPtr(0x34);
    b = SmdGetObjPtr(0x35);
    a->be_flag |= 0x20;
    b->be_flag |= 0x20;
    if (opened == 0) {
        SndCall(6, 1, 0, 0, 0, 0);
    }
    const f32 lim = -2.83f;
    const f32 spd = -0.09f;
    if (opened == 1) {
        a->ang.z = lim;
        b->ang.z = -lim;
    } else {
        while (1) {
            a->ang.y += spd;
            b->ang.y -= spd;
            if (a->ang.y < lim) {
                a->ang.y = lim;
                b->ang.y = -lim;
                break;
            }
            SceSleep(1);
        }
    }
}

// End of the chase's first cut (also its cancel path): camera back, up-cut ended, Stop_flg bits off,
// the Ganados may suspend again.
static void r204_first_cut_exit()
{
    u32 i;

    CamCtrl.Comeback(0);
    SceUpCutEnd();
    SpfFlagOff(pG, SPF_PL);
    SpfFlagOff(pG, SPF_KEY);
    for (i = 0; i <= 10; i++) {
        r204_work->em[i].setNoSuspend(0);
    }
}

// Entering from upstairs: up-cut camera cut 2 shows the mob below (keys locked, Stop_flg 0x10000000); cancellable.
static void r204_first_cut()
{
    u32 i;

    for (i = 0; i <= 10; i++) {
        r204_work->em[i].setNoSuspend(1);
    }
    SceUpCutStart();
    StaFlagOff(pG, STA_SUSPEND);
    SpfFlagOn(pG, SPF_PL);
    KeyStop(0xEFCF0000ULL);
    SceSetEventCancel(1, (TaskFunc) r204_first_cut_exit, 0, -1, 0);
    CamCtrl.CutCall(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r204_first_cut_exit();
}


// The chase task ("nige" = escape): counts frames from the mob's first move; camera cuts 0xF/0x10 as
// the Ganado with the torch (em[7]) charges, scripted run orders to the far points at fixed counts, the
// mob follows the player down; after count 0x12C the door object 0x39 lowers and door5_close runs when
// it drops below 1800 (or after 0x1C1 frames with a Ganado far back); once all are dead the survivors'
// orders end and the exit flags are set; the escape-through-the-door checks the player's position.
static void r204_nige_check()
{
    int started = 0;
    cPlayer* pl;

    while (1) {
        Vec p1 = {0.0f, 0.0f, -58919.0f};
        Vec p2 = {7890.0f, 0.0f, -24661.0f};
        Vec p3 = {-9690.0f, 0.0f, -23661.0f};
        u32 alive;

        alive = SceCountEmAlive(0x10, 0x20);
        if (r204_work->cnt != 0) {
            if (r204_work->cnt == 1) {
                cEm* em = r204_work->em[7].getPtr();

                if (em != 0 && em->dmg.isDamage()) {
                    started = 1;
                    SndStrReq(r204_work->str, 4, 200, 0);
                    r204_work->cnt = 0x23;
                }
            }
            pl = pPL;
            if (started == 0 && pl->checkEvent() == 1 && (alive > 3 || (pG->Room_flg[0] & 0x08000000))) {
                if (r204_work->cnt == 1) {
                    pG->Room_flg[0] |= 0x08000000;
                    SndStrReq(r204_work->str, 4, 200, 0);
                    SceUpCutStart();
                    StaFlagOff(pG, STA_SUSPEND);
                    pPL->dmg.set(0, 0x80);
                    SpfFlagOn(pG, SPF_PL);
                    DpfFlagOn(pG, DPF_PL);
                    PlEndCamera();
                    pl->Wep->m_pWep->setDisp(1, 1);
                    CamCtrl.CutCall(0xF);
                    for (u32 i = 0; i <= 10; i++) {
                        r204_work->em[i].setNoSuspend(1);
                    }
                    r204_work->em[7].setGoto(&pPL->pos, 8);
                }
                if (r204_work->cnt == 0x33) {
                    Vec ang;

                    r204_work->em[7].setGoto(&p1, 8);
                    if (!SpfFlagChk(pG, SPF_PL)) {
                        pG->Room_flg[0] |= 0x08000000;
                        SceUpCutStart();
                        StaFlagOff(pG, STA_SUSPEND);
                        pPL->dmg.set(0, 0x80);
                        SpfFlagOn(pG, SPF_PL);
                        DpfFlagOn(pG, DPF_PL);
                        PlEndCamera();
                    }
                    CamCtrl.CutCall(0x10);
                    for (u32 i = 0; i <= 10; i++) {
                        r204_work->em[i].setNoSuspend(1);
                    }
                    Vec* pa = &ang;
                    if (pPL->pos.x > 0.0f) {
                        ang.x = 0.0f;
                        pa->y = 1.44f;
                        ang.z = 0.0f;
                        r204_work->em[7].setAng(pa);
                    } else {
                        ang.x = 0.0f;
                        pa->y = -1.84f;
                        ang.z = 0.0f;
                        r204_work->em[7].setAng(pa);
                    }
                }
                if (r204_work->cnt == 0x87 && !(pG->Room_flg[0] & 0x20000000)) {
                    r204_work->em[7].setGoto(&p1, 1);
                }
                if (r204_work->cnt == 0x8C && (pG->Room_flg[0] & 0x08000000)) {
                    for (u32 i = 0; i <= 10; i++) {
                        r204_work->em[i].setNoSuspend(0);
                    }
                    CamCtrl.Comeback(0);
                    pPL->dmg.clear();
                    SpfFlagOff(pG, SPF_PL);
                    DpfFlagOff(pG, DPF_PL);
                    SceUpCutEnd();
                }
            }
            if (!(pG->Room_flg[0] & 0x20000000)) {
                // The switch index is `cnt - 30` (19 nodes, root 0x52 = cnt 0x70): the tree compares
                // the biased value and combine folds the root's EQ test back onto cnt.
                switch (r204_work->cnt - 30) {
                case 0x26 - 30:
                    r204_work->em[0].setGoto(&p1, 1);
                    break;
                case 0x31 - 30:
                    r204_work->em[2].setGoto(&p3, 1);
                    break;
                case 0x32 - 30:
                    r204_work->em[5].setGoto(&p3, 1);
                    break;
                case 0x3C - 30:
                    r204_work->em[9].setGoto(&p3, 1);
                    break;
                case 0x46 - 30:
                    r204_work->em[3].setGoto(&p2, 1);
                    break;
                case 0x50 - 30:
                    r204_work->em[10].setGoto(&p2, 1);
                    break;
                case 0x5F - 30:
                    r204_work->em[1].setGoto(&p2, 1);
                    break;
                case 0x64 - 30:
                    r204_work->em[4].setGoto(&p1, 1);
                    break;
                case 0x73 - 30:
                    r204_work->em[6].setGoto(&p1, 1);
                    break;
                case 0x70 - 30:
                    r204_work->em[8].setGoto(&p1, 1);
                    break;
                case 0x6E - 30:
                    r204_work->em[7].setGoto(&p1, 1);
                case 0x82 - 30:
                    r204_work->em[7].setGoto(&p1, 1);
                case 0x96 - 30:
                    r204_work->em[7].setGoto(&p1, 1);
                case 0xF9 - 30:
                    r204_work->em[2].setGoto(&p1, 1);
                    break;
                case 0xFA - 30:
                    r204_work->em[5].setGoto(&p1, 1);
                    break;
                case 0x104 - 30:
                    r204_work->em[9].setGoto(&p1, 1);
                    break;
                case 0x10E - 30:
                    r204_work->em[3].setGoto(&p1, 1);
                    break;
                case 0x118 - 30:
                    r204_work->em[10].setGoto(&p1, 1);
                    break;
                case 0x127 - 30:
                    r204_work->em[1].setGoto(&p1, 1);
                    break;
                }
            }
            if (alive != 0) {
                if (!(pG->Room_flg[0] & 0x20000000)) {
                    int far = 0;

                    for (u32 i = 0; i <= 10; i++) {
                        if (r204_work->em[i].getPosZ() < -28000.0f) {
                            far = 1;
                        }
                    }
                    if (r204_work->cnt > 0x1C1 && far == 1) {
                        SceExec(0x12, (TaskFunc) door5_close, 0, 0, SCE_PRIO_DEF_2, 0);
                    }
                    if (r204_work->cnt > 0x12C) {
                        SmdGetObjPtr(0x39)->pos.y -= 7.0666666f;
                        if (!(pG->Room_flg[0] & 0x20000000)) {
                            if (SmdGetObjPtr(0x39)->pos.y < 1800.0f) {
                                SceExec(0x12, (TaskFunc) door5_close, 0, 0, SCE_PRIO_DEF_2, 0);
                            }
                        }
                    }
                }
            } else {
                if (!(pG->Room_flg[0] & 0x10000000)) {
                    pG->Room_flg[0] |= 0x10000000;
                    pG->Room_flg[0] |= 0x20000000;
                    for (u32 i = 0; i <= 10; i++) {
                        r204_work->em[i].setGoto(&pPL->pos, 0xC);
                    }
                }
            }
            r204_work->cnt++;
        }
        if (alive != 0 && !(pG->Room_flg[0] & 0x20000000)) {
            if (pPL->pos.y < 3000.0f && pPL->pos.z < -25000.0f) {
                SceExec(0x12, (TaskFunc) door5_close, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        SceSleep(1);
    }
}

// The exit portcullis (object 0x39) drops under camera cut 0xC with SE (Room_flg[0] 0x20000000): far
// Ganados stop updating, the item area 0x19 models are pushed clear at fixed frames, the chandeliers
// resume; ends the chase phase.
static void door5_close()
{
    u32 i;
    u32 k;
    int cnt;

    pG->Room_flg[0] |= 0x20000000;
    for (i = 0; i <= 10; i++) {
        if (r204_work->em[i].getPosZ() < -26000.0f) {
            r204_work->em[i].setNoSuspend(1);
        }
    }
    r204_work->chand[0]->setNoSuspend(0);
    r204_work->chand[1]->setNoSuspend(0);
    SceEventStart(1);
    CamCtrl.CutCall(0xC);
    SndCall(6, 0x4D, &SmdGetObjPtr(0x39)->pos, 0, 0, 0);
    cnt = 0;
    while (SmdGetObjPtr(0x39)->pos.y > 0.0f) {
        if (cnt == 0x1E || cnt == 0x3C || cnt == 0x4B) {
            SceAtWork* at = SceAtPtr(0x19);
            u32 j;

            for (j = 0; j <= 10; j++) {
                if (r204_work->em[j].isAlive() == 1 && AreaHitCheck(&at->area, &r204_work->em[j].getPtr()->pos)) {
                    r204_work->em[j].setGoto(&r204_work->em[j].getPtr()->pos, 0xA);
                }
            }
        }
        cnt++;
        SmdGetObjPtr(0x39)->pos.y -= 88.333336f;
        if (SmdGetObjPtr(0x39)->pos.y < 1800.0f) {
            SceAtSetEnable(0xF, 1);
        } else {
            SceAtSetEnable(0xF, 0);
        }
        SceSleep(1);
    }
    SmdGetObjPtr(0x39)->pos.y = 0.0f;
    SndCall(6, 0x4E, &SmdGetObjPtr(0x39)->pos, 0, 0, 0);
    SceSleep(0xF);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(0xF, 1);
    for (i = 0; i <= 10; i++) {
        if (r204_work->em[i].getPosZ() < -33000.0f) {
            r204_work->em[i].destroy();
        }
    }
    for (i = 0; i <= 10; i++) {
        r204_work->em[i].setNoSuspend(0);
    }
    r204_work->chand[0]->setNoSuspend(1);
    r204_work->chand[1]->setNoSuspend(1);
    RsfSet(G_ROOM_ID, 8);
    SceSleep(1);
    for (k = 0; k <= 10; k++) {
        r204_work->em[k].setGoto(&pPL->pos, 0xC);
    }
}

// Swing on a chandelier: `no` picks the chandelier, `pos`/`rot` its placement, the four offsets the
// landing spots and `ofsBase` the swing-start offsets. Shapes (the r117 EventChandelier idioms): the
// first wait is a `do {} while (1)` (a `while (1)` gets rotated by jump.c: `b TOP; SLEEP: ..; TOP:`,
// which loop.c then rejects as "phony" and nothing is hoisted), `frame = 0` right before it (flow nop),
// the dead `do {} while (0)` after it re-materialises `lis pPL@ha` for the second block, FSet + pPL
// reload pPL after each pos store. `if (mf - 5 > 0x41) .. else ..; mf -= 5;` is the target's
// `subi r0,mf,5; mr mf,r0; cmplwi r0,0x41`: gcse deletes the redundant `mf - 5` at the join and
// inserts its reaching copy at the END of the compare block (before the branch); a `mf -= 5` before
// the test is tied by regmove. `BitOn(pl->be_flag, 0x10)` (reference store) keeps the tail's pPL
// reload below the flag store.
// `&crot0` is a two-register `lis r30; addi r25,r30,crot0@l` in the target (the high P in its own
// callee-saved register, `rot` r25 the last callee-saved local of block 0). Pure C: pass `&crot0`
// to low_RotMatrix directly and assign `rot = &crot0` AFTER that call. cse cannot fold the argument
// lo_sum into `rot` (rot is set later; r4 is invalidated by the call), so P has three refs and
// dies at the `addi r4,P` argument (a hard-reg output: local-alloc has nothing to tie P to);
// sched1 hoists the `addi rot,P` above the argument. P (3 refs / span 52) then outranks cpos (500)
// and takes the first callee-saved register r30 that pPL@ha (853) cannot use because the pinned
// `mdl` r30 is live later in the block; `rot` (2 refs: set + setAng use, 192) is allocated last ->
// r25; reload_cse turns the low_RotMatrix `addi r4,r30,@l` into `mr r4,r25`. A `rot` set before
// low_RotMatrix ties P to `rot` (`addi r30,r30`, stmw r17, frame +8).
// COMPILER-DIFF: candidate #17 (`mdl` r30 pin): global-alloc excludes r30 for `mdl` (target r30,
// ours r28), and the pin's r30 in block 0's regs_live_at is what keeps pPL@ha off r30 (see above).
// COMPILER-DIFF: 12 (regmove operand pick, `rp` below): `addi r4,mdl,0xa0` must be computed from
// mdl's register before `mr r3,mdl`. Plain `&mdl->rot` is combined into the `r4` argument move
// (placed after `r3 = mdl`, where mdl dies) and regmove rewrites it as `addi r4,r3`. The codeless
// `asm("" : "+r"(rp) : : "cc")` makes `rp` two-set so combine leaves the `addi` at its own (earlier)
// position and mdl dies at the `mr`; the "cc" clobber (a PARALLEL) keeps the launder from being
// merged into the `r4` move, which would add a second `r4` writer and swap setPos's `mr`/`addi`.
// COMPILER-DIFF: candidate (gcse PRE pseudo numbering) -- the 21 dead `f32 lcN` locals at the top of
// EventChandelier1 (below): the four loop-hoisted highs Key/ActBtn/"%d"/2^31 tie at global priority 68
// (Key 69) and are allocated in PRE pseudo order = gcse bucket order, bucket = (7933 + h(name)) % size
// with h = h*129 + c. "%d" is numbered once (in EventChandelier1, shared by both functions), each
// function's 2^31 double is its own pool label: with our TU's labels (LC51/LC52/LC61) the order is
// right in 1 (207 buckets: ActBtn 20 < 2^31 118 < "%d" 119 -> r19/r18/r17) and wrong in 2 (213
// buckets: "%d" 5 < ActBtn 50 < 2^31 133; target ActBtn r18, "%d" r17, 2^31 r16). Shifting every
// label from EventChandelier1 on by 21 (LC72/LC73/LC82) gives 1: 20 < 170 < 171 and 2: 50 < 51 < 179
// (shifts 21..27 all work; 20 puts "%d" in ActBtn's bucket and wins only by insertion order). The
// dead loads are deleted before gcse and the pool entries never output: .rodata and code unchanged.
#define CHANDELIER(no, cpos0, crot0, dx0, dz0, dx1, dz1, dx2, dz2, postLoop)                                  \
    {                                                                                                              \
        cPlayer* pl = pPL;                                                                                         \
        Mtx m;                                                                                                     \
        Vec cpos;                                                                                                  \
        int frame;                                                                                                 \
        u32 mf;                                                                                                    \
        int far;                                                                                                   \
        f32 nx;                                                                                                    \
        f32 nz;                                                                                                    \
        void* motPl;                                                                                               \
        void* motCh;                                                                                               \
        register cModel* mdl asm("r30"); /* COMPILER-DIFF: candidate #17 */                                        \
        Vec* ang;                                                                                                  \
                                                                                                                   \
        pl->beginEvent(0);                                                                     \
        r204_work->chand[no]->beginEvent(0);                                                 \
        low_RotMatrix(m, (Vec*) &crot0);                                                                           \
        ang = (Vec*) &crot0; /* after the call: see the comment above the macro */                                 \
        PSMTXMultVec(m, (Vec*) &r204_chandOfs, &cpos);                                                             \
        PSVECAdd((Vec*) &cpos0, &cpos, &cpos);                                                                     \
        (pPL->pos.z = cpos.z - (dz0));                                                                          \
        (pPL->pos.x = cpos.x + (dx0));                                                                          \
        mdl = pPL;                                                                                                \
        mdl->setPos(&mdl->pos);                                                                                    \
        mdl->setAng(ang);                                                                                          \
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 3, 0, 1, 0);                                             \
        r204_work->chand[no]->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 3, 0, 1, 0);                          \
        PlSeCall(0x29, &pPL->pos, 0, 0, 0);                                                                        \
        pl->dmg.set(0, 0x80);                                                                                      \
        pl->be_flag &= ~0x10;                                                                                      \
        frame = 0;                                                                                                 \
        do {                                                                                                       \
            if (frame++ == 0x1D) {                                                                                 \
                RoomSeCall(8, &pPL->pos, 0, 0, 0);                                                                 \
            }                                                                                                      \
            if (MotionGetState(pPL) & 4) {                                                                         \
                break;                                                                                             \
            }                                                                                                      \
            SceSleep(1);                                                                                           \
        } while (1);                                                                                               \
        do { } while (0);                                                                                          \
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 3, 0, 5, 0);                                             \
        r204_work->chand[no]->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 3, 0, 5, 0);                          \
        RoomSeCall(0x13, &pPL->pos, 0, 0, 0);                                                                      \
        while (1) {                                                                                                \
            if (MotionGetState(pPL) & 1) {                                                                         \
                RoomSeCall(0x13, &pPL->pos, 0, 0, 0);                                                              \
            }                                                                                                      \
            ActBtn.set(ACT_JUMP_MOVE, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);                                                                 \
            mf = (u32) MotionGetCurrentFrame(MOTION(pPL));                                                         \
            eprintf(0x140, 0x15E, 0, 0, "%d", mf);                                                                 \
            if (mf - 5 > 0x41) {                                                                                   \
                eprintf(0x20, 0x15E, 0, 0, "OK");                                                                  \
            } else {                                                                                               \
                eprintf(0x20, 0x15E, 0, 0, "NO");                                                                  \
            }                                                                                                      \
            mf -= 5;                                                                                               \
            if (Key.trg & 0x00080000) {                                                                            \
                if (mf > 0x41) {                                                                                   \
                    far = 1;                                                                                       \
                    nz = cpos.z + (dz1);                                                                           \
                    nx = cpos.x - (dx1);                                                                           \
                    motPl = ROOM_ARC_PTR(pG->pRoom, 0x27);                                                      \
                    motCh = ROOM_ARC_PTR(pG->pRoom, 0x23);                                                      \
                } else {                                                                                           \
                    far = 0;                                                                                       \
                    nz = cpos.z + (dz2);                                                                           \
                    nx = cpos.x - (dx2);                                                                           \
                    motPl = ROOM_ARC_PTR(pG->pRoom, 0x29);                                                      \
                    motCh = ROOM_ARC_PTR(pG->pRoom, 0x24);                                                      \
                }                                                                                                  \
                break;                                                                                             \
            }                                                                                                      \
            SceSleep(1);                                                                                           \
        }                                                                                                          \
        frame = 0;                                                                                                 \
        (pPL->pos.x = nx);                                                                                      \
        (pPL->pos.z = nz);                                                                                      \
        mdl = pPL;                                                                                                \
        mdl->setPos(&mdl->pos);                                                                                    \
        {                                                                                                          \
            Vec* rp = &mdl->ang;                                                                                   \
            asm("" : "+r"(rp) : : "cc"); /* COMPILER-DIFF: 12 (codeless, see above) */                            \
            mdl->setAng(rp);                                                                                       \
        }                                                                                                          \
        pPL->motionSet(motPl, 3, 0, 1, 0);                                                                         \
        r204_work->chand[no]->motionSet(motCh, 3, 0, 1, 0);                                                      \
        PlSeCall(0x29, &pPL->pos, 0, 0, 0);                                                                        \
        while (!(MotionGetState(pPL) & 4)) {                                                                       \
            frame++;                                                                                               \
            if (far == 1) {                                                                                        \
                if (frame == 0x1F) {                                                                               \
                    FootSeCall(0xD, &pPL->pos, 0, 0);                                                              \
                } else if (frame == 0x21) {                                                                        \
                    FootSeCall(0xE, &pPL->pos, 0, 0);                                                              \
                }                                                                                                  \
            } else {                                                                                               \
                if (frame == 0x26) {                                                                               \
                    FootSeCall(SE_LEON_FALL_BODY, &pPL->pos, 0, 0);                                                                \
                }                                                                                                  \
            }                                                                                                      \
            if (frame == 0x1E) {                                                                                   \
                RoomSeCall(0x14, &r204_work->chand[no]->pos, 0, 0, 0);                                           \
            }                                                                                                      \
            SceSleep(1);                                                                                           \
        }                                                                                                          \
        pl->dmg.clear();                                                                                           \
        (pl->be_flag |= 0x10);                                                                                  \
        pPL->endEvent(0);                                                                      \
        r204_work->chand[no]->endEvent(0);                                                   \
        postLoop                                                                                                   \
    }

// Area 5: swing across on the first chandelier (CHANDELIER macro, chandPos0/Rot0): Leon grabs it, both
// play the swing motions, the camera follows, Leon lands on the far side.
static void r204_EventChandelier1()
{
    /* COMPILER-DIFF: candidate (gcse PRE pseudo numbering): 21 dead pool labels, see CHANDELIER */
    f32 lc0 = 1.5f;
    f32 lc1 = 2.5f;
    f32 lc2 = 3.5f;
    f32 lc3 = 4.5f;
    f32 lc4 = 5.5f;
    f32 lc5 = 6.5f;
    f32 lc6 = 7.5f;
    f32 lc7 = 8.5f;
    f32 lc8 = 9.5f;
    f32 lc9 = 10.5f;
    f32 lc10 = 11.5f;
    f32 lc11 = 12.5f;
    f32 lc12 = 13.5f;
    f32 lc13 = 14.5f;
    f32 lc14 = 15.5f;
    f32 lc15 = 16.5f;
    f32 lc16 = 17.5f;
    f32 lc17 = 18.5f;
    f32 lc18 = 19.5f;
    f32 lc19 = 20.5f;
    f32 lc20 = 21.5f;
    CHANDELIER(0, r204_chandPos0, r204_chandRot0, 5927.0f, 258.0f, 1964.0f, -606.0f, 926.0f, -647.0f, ;)
}

// Area 6: swing on the second chandelier (chandPos1/Rot1); landing alerts all eleven Ganados.
static void r204_EventChandelier2()
{
    CHANDELIER(1, r204_chandPos1, r204_chandRot1, -5927.0f, -258.0f, -1964.0f, 606.0f, -926.0f, 647.0f, {
        u32 i;
        for (i = 0; i <= 10; i++) {
            r204_work->em[i].setFindPL();
        }
    })
}

// Area 2 once (Room_flg bit 0): Scenario_flg[1] 0x40000, event r204s00 (slot 0x14) with the BGM ducked,
// the partner (Ashley) removed from following, then chapter 3-1 ends (SceSetChapterEnd(CHAPTER_3_1))
// and the terminal opens.
static void r204_EventExec()
{
    if (!RsfCheck(G_ROOM_ID, 0)) {
        RsfSet(G_ROOM_ID, 0);
        ScfFlagOn(pG, SCF_R204_ASHLEY_SPLIT);
        SndRoomStrVolSet(1, 200);
        EvtMgr.EvtReadExec("event/evd/r204s00.evd", 0x14, EvtReadFlagFadeOut);
        SndRoomStrVolReset(500);
        if (pSUB != 0) {
            EmMgr.destroy(pSUB);
            StaFlagOff(pG, STA_SUB_ASHLEY);
        }
        SceSetChapterEnd(CHAPTER_3_1, -1);
        r204_openTerm();
    }
}

// Once (Room_flg bit 7): typewriter terminal 0xE.
static void r204_openTerm()
{
    RsfSet(G_ROOM_ID, 7);
    OpeSetOpenTerm(0xE, 0.0f, 0.0f, 0.0f, 0.0f);
}

// Event r204s00 callback: cut 0 hides scroll object 0xC and sets the pl0100 / evm6500 / evm0200 models'
// light mask / draw flags; later cuts hand objects to the event and swap models; the end restores them.
void Evt_R204S00_Func(Event* e)
{
    void* mod;
    void* mod2;
    void* mod3;
    cObj* obj;

    switch (e->FuncType) {
    case 0:
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            SmdSetTrans(0xC, 0);
            obj = SmdGetObjPtr(0xC);
            if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 0x40;
            }
            if (e->GetMod(&mod, "evm6500", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            if (e->GetMod(&mod, "evm0200", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            if (e->GetMod(&mod, "evm0210", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            if (e->GetMod(&mod, "evm0220", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            if (e->GetMod(&mod, "evm0230", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            if (e->GetMod(&mod, "evm0240", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            if (e->GetMod(&mod, "evm0300", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x80;
                ((cModel*) mod)->LightInfo.EnableMask = 0x10;
                ((cModel*) mod)->pModelInfo->color[0] = 0xA5;
                ((cModel*) mod)->pModelInfo->color[1] = 0xA5;
                ((cModel*) mod)->pModelInfo->color[2] = 0xA5;
                ((cModel*) mod)->LightInfo.SelectMask = obj->LightInfo.SelectMask;
            }
        }
        if (e->NowCut <= 2) {
            if (e->NowCut >= 0) {
                if (e->NowFrame == 0) {
                    if (e->GetMod(&mod2, "pl0100", 0, 0) == 1) {
                        ModelInfoSetTrans((cModel*) mod2, 6, 0);
                    }
                }
            } else {
                if (e->NowFrame == 0) {
                    if (e->GetMod(&mod3, "pl0100", 0, 0) == 1) {
                        ModelInfoSetTrans((cModel*) mod3, 6, 1);
                    }
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod3, "pl0100", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod3, 6, 1);
                }
            }
        }
        break;
    case 2:
        SmdSetTrans(0xC, 1);
        break;
    }
}

// Task: when a torch-carrying Ganado stops being active, its torch head object is hidden and its flame effect removed.
static void r204_checkEmDead()
{
    u32 i;

    while (1) {
        for (i = 0; i <= 10; i++) {
            if (r204_work->esp[i] != 0 && r204_work->em[i].isActive() != 1) {
                r204_work->head[i]->be_flag &= ~2;
                EffectEspDelete(0, r204_work->esp[i], 0, 0);
                EffectEspgenDelete(0, r204_work->esp[i], 0);
                EffectEfmDelete(0, r204_work->esp[i], 0);
                r204_work->esp[i] = 0;
            }
        }
        SceSleep(1);
    }
}

// The switch (etc 8) is used: camera cut 0xD, barred door 1 re-closes then opens (setOpen), cut 0xE; the
// player is frozen (Stop_flg bit 31) meanwhile.
static void door_move()
{
    SceEventStart(1);
    pPL->setNoSuspend(1);
    r204_work->sw->setNoSuspend(1);
    SpfFlagOn(pG, SPF_KEY);
    DpfFlagOff(pG, DPF_PL);
    CamCtrl.CutCall(0xD);
    SceSleep(0x28);
    ((cEmBarred*) r204_work->barred[1])->setClosed();
    SceSleep(1);
    ((cEmBarred*) r204_work->barred[1])->setOpen(0);
    CamCtrl.CutCall(0xE);
    SceSleep(0x28);
    CamCtrl.Comeback(0);
    SpfFlagOff(pG, SPF_KEY);
    SceEventEnd(0);
    pPL->setNoSuspend(0);
}
