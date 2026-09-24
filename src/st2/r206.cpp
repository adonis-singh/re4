#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
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
#include "emBarred.h"
#include "emhit.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "math_sub.h"
#include "read.h"
#include "stage.h"
#include "sscrn.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "fade.h"
#include "mes.h"
#include "item.h"
#include "rnd.h"
#include "TexRender.h"
#include "db_log.h"
#include "pl_mod.h"

// Room 2-06 (D:/Bio4/Prog/r206.cpp): the sniper sequence where Ashley runs through the hall while
// the player covers her, Luis' body, the key item pickup and the shelf.

struct R206Work {
    cEmWrap em[14];        // 0x000  [0..2] first wave (0x60..0x62), [4..7] the four chasers (0x64..0x67, [7] = the
                           //        leader), [8..13] the three extra pairs (0x5A..0x5F)
    cObj* head;            // 0x0A8  the leader's head object
    u8 esp;                // 0x0AC  effect kind of the head fire
    u8 pad_AD[3];
    u32 snd;               // 0x0B0  SndCall handle of the taunt
    int timer;             // 0x0B4  frames until the next taunt
    int frame;             // 0x0B8  frames since Ashley's motion started
    int maxFrame;          // 0x0BC  its length
    u32 cnt3;              // 0x0C0  funcAshley3 wait counter
    u32 cnt;               // 0x0C4  taunts since the last long one
    u32 gotoNo;            // 0x0C8  leader waypoint
    TexRenderMng* tex;     // 0x0CC
    u8 texTbl[0x454 - 0xD0];  // 0x0D0
};


static R206Work* r206_work;

void r206_die_event();
static void r206_gouryuu_event();
static void item_chk();
void r206_openTerm();
SceAtWork* GetKeyItemAtari();
static void r206_auto_door_ck();
static void Evt_R206S00_Func(Event* e);
static void Evt_R206S10_Func(Event* e);
static void Evt_R206S20_Func(Event* e);
static void funcAshley(cEm* p);
static void funcAshley2(cEm* p);
static void funcAshley3(cEm* p);
int chkAliveGanadeNum();
static void r206_checkEmDead();
int fire_die_ck();
static void r206_snipe();
static void chkReaderMove();
static void r206_checkDoor();
static void r206_checkDoorToR20c();
static void r206_asl_call();
static void r206_checkDoor2();
static void r206_snipe_end();
void luis_set();
void r206_openShelf_main(int no, int opened);
static void r206_openShelf(int no);
static void r206_openedShelf(int no);
static void destroy_key_atari();

static Vec r206_ashleyPos = {5529.0f, 0.0f, -19612.0f};
Vec r206_readerPos[5] = {
    {9498.0f, 0.0f, -11899.0f},
    {-7546.0f, 0.0f, -6617.0f},
    {10381.0f, 0.0f, -2735.0f},
    {-8495.0f, 0.0f, -17240.0f},
    {8995.0f, 0.0f, -17216.0f},
};
Vec r206_chaserPos[2] = {
    {7570.0f, 0.0f, -12074.0f},
    {8693.0f, 0.0f, -9459.0f},
};
static Vec r206_hitPos0 = {0.0f, 1266.0f, 961.0f};
static Vec r206_hitPos1 = {0.0f, 937.0f, 940.0f};
static Vec r206_hitPos2 = {0.0f, 608.0f, 913.0f};
static Vec r206_hitRot = {0.0f, 0.0f, 0.0f};
static f32 r206_cubeW = 370.0f;
static f32 r206_cubeH = 120.0f;
static f32 r206_cubeD = 170.0f;
static f32 r206_cubeY = -40.0f;
static f32 r206_cubeZ = -30.0f;
static f32 r206_subCubeW = 130.0f;
static f32 r206_subCubeH = 1400.0f;
static f32 r206_subCubeD = 70.0f;
static f32 r206_subCubeY = 0.0f;
static f32 r206_subCubeZ = 0.0f;
static f32 r206_subCube2W = 60.0f;
static f32 r206_subCube2H = 1600.0f;
static f32 r206_subCube2D = 70.0f;
static f32 r206_subCube2Y = 0.0f;
static f32 r206_subCube2Z = 0.0f;
static Vec r206_ashleyGoal = {5250.0f, 0.0f, -19342.0f};

// Room init: the shelf item event; before the sniper sequence is done (Room_flg bit 0) Ashley is
// initialised at the far end facing -PI and the snipe / gate tasks run, else Luis' body is laid out.
// Returning after Ashley's section (from r20d, bit 4 once) plays the reunion event; the item take-over
// (bit 9) when the reunion happened; the three event callbacks; door areas 7/6 with their messages, the
// key item area removed after the reunion, Ashley's call on area 8 once (bit 6), the debug snipe-end task.
void R206Init()
{
#line 102 "D:/Bio4/Prog/r206.cpp"
    r206_work = (R206Work*) MEM_CALLOC(sizeof(R206Work), 1, 0xd);
    SceSetItemEvent(9, 0x80, 8, 0xA, r206_openShelf, r206_openedShelf, 0, 0);
    SceAtSetEnable(0xB, 0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        Vec pos = {0.0f, 0.0f, 900.0f};

        SubCharInit(1, &pos, -3.14f);
        SceExec(0x12, (TaskFunc) r206_snipe, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r206_auto_door_ck, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        luis_set();
    }
    if (RsfCheck(G_ROOM_ID, 4) && RsfCheck(G_ROOM_ID, 9) == 0) {
        FadeSetW(1, 0, 0, 0);
        SceExec(0x12, (TaskFunc) item_chk, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (pG->room_id_prev == 0x20D && RsfCheck(G_ROOM_ID, 4) == 0) {
        RsfSet(G_ROOM_ID, 4);
        SceExec(0x12, (TaskFunc) r206_gouryuu_event, 0, 0, SCE_PRIO_DEF_2, 0);
        ScfFlagOn(pG, SCF_R206_ASHLEY_RESCUE);
        StaFlagOn(pG, STA_SUB_ASHLEY);
    }
    EvtMgr.SetFunc("evt_r206s00_func", (void*) Evt_R206S00_Func);
    EvtMgr.SetFunc("evt_r206s10_func", (void*) Evt_R206S10_Func);
    EvtMgr.SetFunc("evt_r206s20_func", (void*) Evt_R206S20_Func);
    SmdSetTrans(8, 0);
    SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r206_checkDoor, 0, 1);
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r206_checkDoorToR20c, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 4)) {
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r206_checkDoor2, 0, 1);
        SmdSetTrans(0xC, 0);
        SmdSetTrans(0xD, 0);
        SmdSetTrans(0xE, 0);
        SceExec(0x12, (TaskFunc) destroy_key_atari, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r206_asl_call, 0, 1);
    }
    SceExec(0x12, (TaskFunc) r206_snipe_end, 0, 0, SCE_PRIO_DEF_2, 0);
    TexRenderInit(&r206_work->tex, 0, 2);
}

// Per-frame room main: nothing.
void R206Main()
{
}

// Luis' death: event r206s00 (slot 0x11), r206s10 pre-loaded, chapter 3-3 ends (SceSetChapterEnd(CHAPTER_3_3)).
void r206_die_event()
{
    EvtMgr.EvtReadExec("event/evd/r206s00.evd", 0x11, EvtReadFlagNone);
    EvtMgr.EvtReadAram("event/evd/r206s10.evd", 0, 0, 0, 0);
    SceSetChapterEnd(CHAPTER_3_3, -1);
}

// The reunion ("gouryuu") after Ashley's section: door flags opened, event r206s20, chapter 3-4 ends
// (SceSetChapterEnd(CHAPTER_3_4)), then the item take-over.
static void r206_gouryuu_event()
{
    ScfFlagOn(pG, SCF_7f);
    ScfFlagOn(pG, SCF_80);
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r206s20.evd", 0x11, EvtReadFlagNone);
    SceSetChapterEnd(CHAPTER_3_4, -1);
    FadeSetW(1, 0, 0, 0);
    item_chk();
}

// After the reunion (Room_flg bit 9): Ashley's items are merged into Leon's inventory (ItemMgr.takeOver,
// the banked pesetas added), the inventory opens in Ashley mode, then the typewriter.
static void item_chk()
{
    u32 zero = 0;

    RsfSet(G_ROOM_ID, 9);
    ItemMgr.takeOver();
    pG->peseta = pG->peseta + pG->peseta_bak;
    pG->peseta_bak = zero;
    SubScreenOpen(SS_OPEN_NORMAL, SS_ATTR_ASHLEY);
    SceSleep(1);
    r206_openTerm();
}

// Once (Room_flg bit 7): typewriter terminal 0x10.
void r206_openTerm()
{
    RsfSet(G_ROOM_ID, 7);
    OpeSetOpenTerm(0x10, 0.0f, 0.0f, 0.0f, 0.0f);
}

// The area of the key item (type 3, item 0xA3).
SceAtWork* GetKeyItemAtari()
{
    SceAtWork* p;

    p = sceAtSetOtStart();
    while ((p = sceAtGetOtAddr(p)) != NULL) {
        if (p->type == 3 && p->item.id == 0xA3) {
            return p;
        }
    }
    return NULL;
}

// Task: the two barred gates (etc 0x10/0x11) start closed and open / close with Room_flg[2] bit 31
// (the sniper sequence's gate control).
static void r206_auto_door_ck()
{
    cEm* gate0;
    cEm* gate1;

    if (getRoomEtcBarred(0x10, &gate0, 1) != 1) {
        return;
    }
    if (getRoomEtcBarred(0x11, &gate1, 1) != 1) {
        return;
    }
    ((cEmBarred*) gate0)->setClosed();
    ((cEmBarred*) gate1)->setClosed();
    for (;;) {
        if (pG->Room_flg[2] & 0x80000000) {
            if (((cEmBarred*) gate0)->ckStatus() == 2) {
                ((cEmBarred*) gate0)->setOpen(0);
            }
        } else {
            if (((cEmBarred*) gate0)->ckStatus() == 1) {
                ((cEmBarred*) gate0)->setClose(0);
            }
        }
        if (pG->Room_flg[2] & 0x40000000) {
            if (((cEmBarred*) gate1)->ckStatus() == 2) {
                ((cEmBarred*) gate1)->setOpen(0);
            }
        } else {
            if (((cEmBarred*) gate1)->ckStatus() == 1) {
                ((cEmBarred*) gate1)->setClose(0);
            }
        }
        SceSleep(1);
    }
}

// Event r206s00 callback (Saddler kills Luis): etc model 0x11 hidden; per cut the sample model
// obm5500's parts, the Luis model ev0401's texture palette (normal / bloodied variant) and evmc800's
// flags; the end restores etc 0x11.
static void Evt_R206S00_Func(Event* e)
{
    void* mod;
    void* mod2;
    void* bin;
    void* bin2;

    switch (e->FuncType) {
    case 0:
        setRoomEtcDisp(0x11, 0, 1);
        break;
    case 1:
        switch (e->NowCut) {
        case 0x13:
        case 0x15:
        case 0x17:
        case 0x18:
        case 0x1A:
        case 0x1E:
            if (e->NowFrame == 0) {
                SmdSetTrans(8, 1);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                SmdSetTrans(8, 0);
            }
            break;
        }
        switch (e->NowCut) {
        case 0xF:
        case 0x11:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 0);
            }
            break;
        default:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 1);
            }
            break;
        }
        if (e->NowCut <= 0xC) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "ev0401", 0, 0) == 1) {
                    if (EvtMgr.GetBin(&bin, "event/model/ev0400/ev0401.tpl", 0) == 1) {
                        ((cModelInfo*) mod2)->setTplAddr(bin);
                    }
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "ev0401", 0, 0) == 1) {
                    if (EvtMgr.GetBin(&bin2, "event/model/ev0400/ev0401_blood.tpl", 0) == 1) {
                        ((cModelInfo*) mod2)->setTplAddr(bin2);
                    }
                }
            }
        }
        switch (e->NowCut) {
        case 5:
        case 7:
        case 8:
        case 0xC:
        case 0xE:
        case 0xF:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evmc800", 0, 0) == 1) {
                    ((cModel*) mod)->ot_type = 1;
                    TexRenderModSet((cModel*) mod, 0, r206_work->texTbl, r206_work->tex, 0, 1, 1, 1, 1.0f);
                }
                EffectEspDelete(r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0, 0);
                EffectEspgenDelete(r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0);
                EffectEfmDelete(r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 2, r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evmc800", 0, 0) == 1) {
                    TexRenderModRes((cModel*) mod, 0);
                }
                EffectEspDelete(r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0, 0);
                EffectEspgenDelete(r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0);
                EffectEfmDelete(r206_work->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_ROOM00, 0);
            }
            break;
        }
        break;
    case 2:
        SmdSetTrans(8, 0);
        setRoomEtcDisp(0x11, 1, 1);
        break;
    }
}

// Event r206s10 callback: hides scroll objects 0xC/0xD/0xE and etc model 0x11 for the event, restores them after.
static void Evt_R206S10_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0xC, 0);
        SmdSetTrans(0xD, 0);
        SmdSetTrans(0xE, 0);
        setRoomEtcDisp(0x11, 0, 1);
        break;
    case 1:
        break;
    case 2:
        setRoomEtcDisp(0x11, 1, 1);
        break;
    }
}

// Event r206s20 callback (the reunion): scroll object 0x12 hidden during the event.
static void Evt_R206S20_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            SmdSetTrans(0x12, 0);
        }
        break;
    case 2:
        SmdSetTrans(0x12, 1);
        break;
    }
}

// Ashley's aux routine: slide towards the fixed spot while the three motions play.
static void funcAshley(cEm* p)
{
    Vec d;

    PSVECSubtract(&r206_ashleyPos, &pSUB->pos, &d);
    PSVECScale(&d, &d, 0.15f);
    PSVECAdd(&pSUB->pos, &d, &d);
    pSUB->setPos(&d);
    switch (p->r_no_2) {
    case 0:
        pSUB->atari.off();
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 0xA, 0, 1, 0);
        SndCall(6, 0xD, &pSUB->pos, 0, 0, 0);
        p->r_no_2 = 1;
    case 1:
        if (p->motionMove() != 0) {
            p->r_no_2 = 2;
        }
        break;
    case 2:
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 0xA, 0, 1, 0);
        p->r_no_2 = 3;
    case 3:
        if (p->motionMove() != 0) {
            p->r_no_2 = 4;
        }
        break;
    case 4:
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0xA, 0, 1, 0);
        p->r_no_2 = 5;
    default:
        if (p->motionMove() != 0) {
            p->setRno(0, 0, 0, 0);
            pSUB->atari.on();
            SubCharCtrl(SCC_CHASE, 0);
        }
        break;
    }
    p->ang.y = 2.35f;
}

// Ashley's aux routine 2: play motion 0x19 of archive 0x27 (no collision meanwhile), then back to following.
static void funcAshley2(cEm* p)
{
    if (p->r_no_2 == 0) {
        pSUB->atari.off();
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x27), 0x19, 0, 1, 0);
        p->r_no_2 = 1;
    }
    if (p->motionMove() != 0) {
        p->setRno(0, 0, 0, 0);
        pSUB->atari.on();
        SubCharCtrl(SCC_CHASE, 0);
    }
}

// Ashley's aux routine 3: motion 0x19 of archive 0x31, a 30-frame pause, then back to following.
static void funcAshley3(cEm* p)
{
    int step = p->r_no_2;

    switch (step) {
    case 0:
        pSUB->atari.off();
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x31), 0x19, 0, 1, 0);
        r206_work->cnt3 = step;
        p->r_no_2 = 1;
    case 1:
        if (p->motionMove() != 0) {
            p->r_no_2 = 2;
        }
        break;
    case 2:
        r206_work->cnt3++;
        if (r206_work->cnt3 > 0x1E) {
            p->r_no_2 = 3;
        }
        break;
    default:
        p->setRno(0, 0, 0, 0);
        pSUB->atari.on();
        SubCharCtrl(SCC_CHASE, 0);
        break;
    }
}

// Number of live, active Ganados (ids 0x10..0x20) in the enemy manager.
int chkAliveGanadeNum()
{
    int cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if (em->id >= 0x10 && em->id <= 0x20 && em->checkStatus(EM_STATUS_ACTIVE) != 0 && em->isAlive()) {
            cnt++;
        }
    }
    return cnt;
}

// Task: once the leader's item (area 0x82) is taken, hide its head object and drop the head fire effect.
static void r206_checkEmDead()
{
    while (SceAtItemFlgCk(0x82) == 0) {
        SceSleep(1);
    }
    r206_work->head->be_flag &= ~2;
    EffectEspDelete(0, r206_work->esp, 0, 0);
    EffectEspgenDelete(0, r206_work->esp, 0);
    EffectEfmDelete(0, r206_work->esp, 0);
}

// 1 when Ashley stands in a fire / explosion damage area (DmgMgr kinds 1 / 7).
int fire_die_ck()
{
    Vec out;
    int kind;

    kind = DmgMgr.hitCheck(&pSUB->pos, &out);
    if (kind != DMG_TYPE_FIRE && kind != DMG_TYPE_ENV_FIRE) {
        return 0;
    }
    return 1;
}

// The sniper sequence task: Ashley runs the hall in stages (aux motions funcAshley*) while the
// Ganado waves (first wave, the four chasers with the leader, the extra pairs) come at her; the player
// covers her from the balcony; the hit boxes on the doors, the gates and the taunt SEs are driven here;
// ends with Room_flg bit 0 and Luis' death event.
static void r206_snipe()
{
    cObj* obj0;
    cObj* obj1;
    cObj* obj2;
    cEmHit* hit0 = NULL;
    cEmHit* hit1 = NULL;
    cEmHit* hit2 = NULL;
    cEmHit* subHit0;
    cEmHit* subHit1;
    cEm* gate;
    SceAtWork* at;
    int done;
    int moved;
    int wave;

    obj0 = SmdGetObjPtr(0xC);
    obj1 = SmdGetObjPtr(0xD);
    obj2 = SmdGetObjPtr(0xE);
    ScfFlagOn(pG, SCF_NO_ASHLEY_DIST_CK);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        hit0 = SetEmHit(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &r206_hitPos0, &r206_hitRot, 0);
        obj0->be_flag |= 0x20;
        YarareInitCube(hit0, 0.0f, r206_cubeY, r206_cubeZ, r206_cubeW, r206_cubeH, r206_cubeD, 0, YAT_FLAG_ON);
    } else {
        obj0->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        hit1 = SetEmHit(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &r206_hitPos1, &r206_hitRot, 0);
        obj1->be_flag |= 0x20;
        YarareInitCube(hit1, 0.0f, r206_cubeY, r206_cubeZ, r206_cubeW, r206_cubeH, r206_cubeD, 0, YAT_FLAG_ON);
    } else {
        obj1->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        hit2 = SetEmHit(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &r206_hitPos2, &r206_hitRot, 0);
        obj2->be_flag |= 0x20;
        YarareInitCube(hit2, 0.0f, r206_cubeY, r206_cubeZ, r206_cubeW, r206_cubeH, r206_cubeD, 0, YAT_FLAG_ON);
    } else {
        obj2->be_flag &= ~2;
    }
    subHit0 = SetEmHit(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &pSUB->pos, &r206_hitRot, 1);
    YarareInitCube(subHit0, 0.0f, r206_subCubeY, r206_subCubeZ, r206_subCubeW, r206_subCubeH, r206_subCubeD, 0, YAT_FLAG_ON);
    subHit1 = SetEmHit(ROOM_ARC_PTR(pG->pCore, 8), ROOM_ARC_PTR(pG->pCore, 9), &pSUB->pos, &r206_hitRot, 1);
    YarareInitCube(subHit1, 0.0f, r206_subCube2Y, r206_subCube2Z, r206_subCube2W, r206_subCube2H, r206_subCube2D, 0, YAT_FLAG_ON);
    if (DebugTrg(1) == 0) {
        SceSleep(1);
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            RsfSet(G_ROOM_ID, 5);
            r206_die_event();
        }
        luis_set();
    }
    SubCharCtrl(SCC_AUX_MOT, 0);
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 5, 0);
    pSUB->dmg.m_Timer = 0x80;
    {
        Vec pos = {0.0f, 0.0f, 900.0f};

        pSUB->setPos(&pos);
    }
    r206_work->timer = 0xF0;
    do {
        r206_work->timer--;
        if (r206_work->timer == 0) {
            u32 rsf;
            u32 zero;

            r206_work->timer = (u32) (fRand0_1() * 60.0f) + 0x3C;
            rsf = RsfCheck(*(u16*) &pG->stage_no, 6);
            zero = 0;
            if (rsf) {
                r206_work->cnt++;
            }
            if (r206_work->cnt > 8) {
                r206_work->cnt = zero;
                r206_work->snd = SndCall(6, 5, &pSUB->pos, 0, 0, 0);
            } else {
                r206_work->snd = SndCall(6, 6, &pSUB->pos, 0, 0, 0);
            }
        }
        done = 0;
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            if (hit0->ckStatus() == 1) {
                RsfSet(G_ROOM_ID, 1);
                EstSet(0, -1, &r206_hitPos0, &r206_hitRot, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
                obj0->be_flag &= ~2;
                if (r206_work->snd != 0) {
                    SndStop(r206_work->snd, 0);
                }
                SndCall(6, 4, &obj0->pos, 0, 0, 0);
                SndCall(6, 0xA, &obj0->pos, 0, 0, 0);
                r206_work->timer = 0x5A;
            }
        } else {
            done = 1;
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0) {
            if (hit1->ckStatus() == 1) {
                RsfSet(G_ROOM_ID, 2);
                EstSet(0, -1, &r206_hitPos1, &r206_hitRot, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
                obj1->be_flag &= ~2;
                if (r206_work->snd != 0) {
                    SndStop(r206_work->snd, 0);
                }
                SndCall(6, 4, &obj1->pos, 0, 0, 0);
                if (done == 0) {
                    SndCall(6, 0xA, &obj1->pos, 0, 0, 0);
                }
                r206_work->timer = 0x5A;
            }
        } else {
            done++;
        }
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            if (hit2->ckStatus() == 1) {
                RsfSet(G_ROOM_ID, 3);
                EstSet(0, -1, &r206_hitPos2, &r206_hitRot, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
                obj2->be_flag &= ~2;
                if (r206_work->snd != 0) {
                    SndStop(r206_work->snd, 0);
                }
                SndCall(6, 4, &obj2->pos, 0, 0, 0);
                if (done == 0) {
                    SndCall(6, 0xA, &obj2->pos, 0, 0, 0);
                }
                r206_work->timer = 0x5A;
            }
        } else {
            done++;
        }
        if (RsfCheck(G_ROOM_ID, 1) && RsfCheck(G_ROOM_ID, 2)) {
            if (r206_work->frame == 0) {
                pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 0, 0, 1, 0);
                *(u32*) &r206_work->maxFrame = (u32) MotionGetMaxFrame(&pSUB->Motion);
                YarareInitCube(subHit0, 0.0f, r206_subCubeY, r206_subCubeZ, r206_subCubeW, r206_subCubeH * 0.85f,
                               r206_subCubeD, 0, YAT_FLAG_ON);
                YarareInitCube(subHit1, 0.0f, r206_subCube2Y, r206_subCube2Z, r206_subCube2W, r206_subCube2H * 0.7f,
                               r206_subCube2D, 0, YAT_FLAG_ON);
            } else if (r206_work->frame == r206_work->maxFrame) {
                pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 0, 5, 0);
            }
            r206_work->frame++;
        }
        if (DbgFlagChk(pG, DBG_NO_DEATH) == 0) {
            if (fire_die_ck() == 1 || subHit0->ckStatus() == 1 || subHit1->ckStatus() == 1) {
                *(s16*) &pG->ashley_life = -1;
                if (RsfCheck(G_ROOM_ID, 1) && RsfCheck(G_ROOM_ID, 2)) {
                    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2E), 0, 0, 1, 0);
                } else {
                    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 1, 0);
                }
            }
        }
        if ((s16) pG->ashley_life != -1 && (u32) done > 2) {
            pSUB->setRno(0, 0, 0, 0);
            goto snipe_done;
        }
        SceSleep(1);
    } while (1);
snipe_done:
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r206s10.evd", 0x11, EvtReadFlagNone);
    FadeSetW(0x80000002, 0x1E, 0, 0);
    {
        Vec pos = {0.0f, 0.0f, -1740.0f};

        pSUB->setPos(&pos);
    }
    SceAtSetEnable(8, 0);
    RsfSet(G_ROOM_ID, 6);
    pSUB->dmg.clear();
    SubCharMoveTo(0.0f, 0.0f, -2000.0f, 193.0f, 1);
    r206_work->em[0].setEm(0x60, -1, 1, 1, 1);
    r206_work->em[1].setEm(0x61, -1, 1, 1, 1);
    r206_work->em[2].setEm(0x62, -1, 1, 1, 1);
    r206_work->em[0].setBeFlag(0x10000, 1);
    r206_work->em[1].setBeFlag(0x10000, 1);
    r206_work->em[2].setBeFlag(0x10000, 1);
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r206_checkDoorToR20c, 0, 1);
    SceSleep(1);
    while (chkAliveGanadeNum() != 0) {
        SceSleep(1);
    }
    if ((pG->Room_flg[0] & 0x80000000) == 0) {
        pG->Room_flg[0] |= 0x80000000;
    }
    SetSubAux(funcAshley2, 0);
    SceSleep(1);
    while (SubCharGetStatus() & 0x01000000) {
        SceSleep(1);
    }
    SubCharMoveTo(4316.0f, 0.0f, -6652.0f, 193.0f, 1);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SubCharMoveTo(0.0f, 0.0f, -2300.0f, 193.0f, 0);
    SubCharMoveTo(r206_ashleyGoal.x, r206_ashleyGoal.y, r206_ashleyGoal.z, 193.0f, 0);
    SceSleep(1);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SetSubAux(funcAshley, 0);
    pSUB->setNoSuspend(1);
    SceEventStart(1);
    SpfFlagOff(pG, SPF_EM);
    CamCtrl.CutCall(3);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(0x32);
    SndCall(6, 0xB, 0, 0, 0, 0);
    SceMesSet(2, 0xA2, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    SceSleep(0xA);
    SceSleep(0x37);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pSUB->setNoSuspend(0);
    r206_work->gotoNo = 0;
    r206_work->em[4].setEm(0x64, -1, 1, 1, 1);
    r206_work->em[5].setEm(0x65, -1, 1, 1, 1);
    r206_work->em[6].setEm(0x66, -1, 1, 1, 1);
    r206_work->em[7].setEm(0x67, -1, 1, 1, 1);
    r206_work->em[4].setBeFlag(0x10000, 1);
    r206_work->em[5].setBeFlag(0x10000, 1);
    r206_work->em[6].setBeFlag(0x10000, 1);
    if (r206_work->em[7].isAlive() == 1) {
        Vec ofs = {0.0f, -2.0f, 180.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        r206_work->head = SetObj00(ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x30), &ofs, &rot);
        r206_work->head->setNoSuspend(1);
        OyaSetObj00(r206_work->head, r206_work->em[7].getPtr(), 2);
        r206_work->esp = EspPullCoreKind();
        SceExec(0x12, (TaskFunc) r206_checkEmDead, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    r206_work->em[4].setNoSuspend(1);
    r206_work->em[5].setNoSuspend(1);
    r206_work->em[7].setNoSuspend(1);
    if (getRoomEtcBarred(0x11, &gate, 1) == 1) {
        gate->setNoSuspend(1);
        ((cEmBarred*) gate)->setOpen(0);
    }
    SceEventStart(1);
    CamCtrl.CutCall(4);
    r206_work->em[7].setGoto(&r206_readerPos[r206_work->gotoNo], 1);
    while (r206_work->em[7].ckGoto() != 0) {
        SceSleep(1);
    }
    r206_work->em[7].setGoto(&r206_readerPos[r206_work->gotoNo], 8);
    r206_work->em[4].setGoto(&r206_readerPos[r206_work->gotoNo], 1);
    r206_work->em[5].setGoto(&r206_chaserPos[0], 1);
    r206_work->em[6].setGoto(&r206_chaserPos[0], 1);
    r206_work->em[5].setFindPL();
    r206_work->em[6].setFindPL();
    SceSleep(0x37);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r206_work->em[4].setNoSuspend(0);
    r206_work->em[5].setNoSuspend(0);
    r206_work->em[6].setNoSuspend(0);
    r206_work->em[7].setNoSuspend(0);
    SceExec(0x12, (TaskFunc) chkReaderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    SubCharMoveTo(4316.0f, 0.0f, -6652.0f, 193.0f, 0);
    moved = 0;
    wave = 1;
    for (;;) {
        if (moved == 0 && (SubCharGetStatus() & 0x00800000) == 0) {
            moved = 1;
            SubCharMoveTo(0.0f, 0.0f, -2300.0f, 193.0f, 0);
        }
        if (r206_work->em[7].isActive() == 0) {
            goto wave_done;
        }
        if ((u32) SceCountEmAlive(0x10, 0x20) <= 2) {
            r206_work->em[7].setGoto(&pSUB->pos, 8);
            if (wave == 0) {
                r206_work->em[8].setEm(0x5A, -1, 1, 1, 1);
                r206_work->em[9].setEm(0x5B, -1, 1, 1, 1);
                r206_work->em[8].setFindPL();
                r206_work->em[9].setFindPL();
                r206_work->em[8].setBeFlag(0x10000, 1);
                r206_work->em[9].setBeFlag(0x10000, 1);
            } else if (wave == 1) {
                r206_work->em[10].setEm(0x5C, -1, 1, 1, 1);
                r206_work->em[11].setEm(0x5D, -1, 1, 1, 1);
                r206_work->em[10].setFindPL();
                r206_work->em[11].setFindPL();
                r206_work->em[10].setBeFlag(0x10000, 1);
                r206_work->em[11].setBeFlag(0x10000, 1);
            } else if (wave == 2) {
                r206_work->em[12].setEm(0x5E, -1, 1, 1, 1);
                r206_work->em[13].setEm(0x5F, -1, 1, 1, 1);
                r206_work->em[12].setFindPL();
                r206_work->em[13].setFindPL();
                r206_work->em[12].setBeFlag(0x10000, 1);
                r206_work->em[13].setBeFlag(0x10000, 1);
                goto wave_done;
            }
            wave++;
        }
        SceSleep(1);
    }
wave_done:
    for (;;) {
        if (moved == 0 && (SubCharGetStatus() & 0x00800000) == 0) {
            moved = 1;
            SubCharMoveTo(0.0f, 0.0f, -2300.0f, 193.0f, 0);
        }
        if (SceCountEmAlive(0x10, 0x20) == 0) {
            break;
        }
        SceSleep(1);
    }
    at = GetKeyItemAtari();
    if (at != NULL) {
        SubCharMoveTo(at->item.pos.x, at->item.pos.y, at->item.pos.z, 193.0f, 0);
    } else {
        pLog->err(0, 0, "KEY ATARI NOT FOUND!!");
    }
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SetSubAux(funcAshley3, 0);
    SceSleep(0xA);
    if (at != NULL) {
        SceAtSetEnable(at->no, 0);
    }
    SceSleep(0xF);
    SndCall(6, 0xC, 0, 0, 0, 0);
    SceMesSet(4, 0xA2, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    while (SubCharGetStatus() & 0x01000000) {
        SceSleep(1);
    }
    SubCharMoveTo(5316.0f, 0.0f, -18975.0f, 2.27f, 0);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    pSUB->setNoSuspend(1);
    SceEventStart(1);
    {
        Vec pos;
        cModel* m = pSUB;

        pos.x = 5669.0f;
        pos.y = 0.0f;
        pos.z = -19307.0f;
        m->setPos(&pos);
    }
    CamCtrl.CutCall(3);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(1);
    SndCall(6, 3, 0, 0, 0, 0);
    SceMesSet(5, 2, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    if (pSUB != NULL) {
        EmMgr.destroy(pSUB);
        StaFlagOff(pG, STA_SUB_ASHLEY);
    }
    ScfFlagOn(pG, SCF_R206_ASHLEY_GAME);
    PlSelect(1);
    SceAtExecute(0);
    RsfSet(G_ROOM_ID, 0);
    SceEventEnd(0);
}

// The leader walks the waypoints; the chasers follow Ashley once few enemies remain.
static void chkReaderMove()
{
    int timer = 0;
    u32 cnt = 0;
    int near = 0;
    cEm* em;

    while (r206_work->em[7].isActive() != 0) {
        cnt++;
        em = r206_work->em[7].getPtr();
        if (em != NULL) {
            Vec d;
            f32 dist;
            int n;

            PSVECSubtract(&em->pos, &r206_readerPos[r206_work->gotoNo], &d);
            dist = PSVECMag(&d);
            if (dist < 2500.0f) {
                if (dist < 1000.0f && near == 0) {
                    near = 1;
                    r206_work->em[7].setGoto(&pSUB->pos, 8);
                }
                if ((r206_work->gotoNo == 0 && cnt == 0x1A4)
                    || (cnt > 0x257 && em->isAlive() && em->checkStatus(EM_STATUS_ACTIVE) == 1
                        && em->dmg.isDamage())) {
                    timer = 0x5A;
                }
                if (timer != 0) {
                    timer--;
                }
                if (timer == 1) {
                    r206_work->gotoNo++;
                    if (r206_work->gotoNo > 4) {
                        r206_work->gotoNo = 0;
                    }
                    near = 0;
                    r206_work->em[7].setGoto(&r206_readerPos[r206_work->gotoNo], 1);
                    r206_work->em[4].setGoto(&r206_readerPos[r206_work->gotoNo], 1);
                    r206_work->em[6].setGoto(&r206_readerPos[r206_work->gotoNo], 1);
                }
            } else {
                r206_work->em[7].setGoto(&r206_readerPos[r206_work->gotoNo], 1);
            }
            n = SceCountEmAlive(0x10, 0x20);
            if (n == 0) {
                break;
            }
            if (n == 1) {
                r206_work->em[7].setGoto(&pSUB->pos, 1);
                if (em->hp > 0x2BC) {
                    em->hp = 0x2BC;
                }
                break;
            }
        }
        SceSleep(1);
    }
    for (;;) {
        int n = SceCountEmAlive(0x10, 0x20);

        if (n == 1) {
            if (r206_work->em[4].isActive() != 0) {
                r206_work->em[4].getPtr()->flag |= 0x40;
            }
            if (r206_work->em[6].isActive() != 0) {
                r206_work->em[6].getPtr()->flag |= 0x40;
            }
            r206_work->em[4].setGoto(&pSUB->pos, 1);
            r206_work->em[6].setGoto(&pSUB->pos, 1);
            SceSleep(0x168);
        }
        if (n == 2) {
            if (r206_work->em[4].isActive() != 0 && r206_work->em[6].isActive() != 0) {
                if (r206_work->em[4].isActive() != 0) {
                    r206_work->em[4].getPtr()->flag |= 0x40;
                }
                r206_work->em[4].setGoto(&pSUB->pos, 1);
                SceSleep(0x168);
            }
        }
        SceSleep(1);
    }
}

// Area 7: reset the door area (the door is usable).
static void r206_checkDoor()
{
    SceAtDataReset(7);
}

// Area 7 before the reunion: message 0x67 (Ashley is not with Leon; cannot leave).
static void r206_checkDoorToR20c()
{
    cMes.MesSet(0x67, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
}

// Area 8 once: Ashley calls out (SE 6/5) under camera cut 9.
static void r206_asl_call()
{
    r206_work->timer = 0x5A;
    SndStop(r206_work->snd, 0);
    if (pSUB != NULL) {
        pSUB->setNoSuspend(1);
    }
    SceEventStart(1);
    r206_work->snd = SndCall(6, 5, &pSUB->pos, 0, 0, 0);
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (pSUB != NULL) {
        pSUB->setNoSuspend(0);
    }
    RsfSet(G_ROOM_ID, 6);
}

// Area 6 after the reunion: message 6.
static void r206_checkDoor2()
{
    SceMesSet(6, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
}

// Debug task: on debug trigger 0 switch control to Ashley (PlSelect(1)) and run area 0.
static void r206_snipe_end()
{
    for (;;) {
        if (DebugTrg(0) == 0) {
            SceSleep(1);
        } else {
            break;
        }
    }
    PlSelect(1);
    SceAtExecute(0);
}

// Luis' body and the blood.
void luis_set()
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    f32 zero;
    int lit = 4;

    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x32), ROOM_ARC_PTR(pG->pRoom, 0x33), &pos, &rot, 0x10, 1);
    obj->addModel(ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x34), ROOM_ARC_PTR(pG->pRoom, 0x35)));
    obj->addModel(ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x36), ROOM_ARC_PTR(pG->pRoom, 0x37)));
    obj->addModel(ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x38), ROOM_ARC_PTR(pG->pRoom, 0x39)));
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3D), 0xA, 0, 1, 0);
    obj->be_flag |= 0x1000;
    zero = 0.0f;
    obj->Motion.Seq_speed = zero;
    obj->setNoSuspend(1);
    obj->LightInfo.EnableMask = lit;
    obj->be_flag |= 0x10;
    ShapeSet(obj->pModelInfo->pList->pList, 0, ROOM_ARC_PTR(pG->pRoom, 0x3C), 2);
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x3A), ROOM_ARC_PTR(pG->pRoom, 0x3B), &pos, &rot, 0x10, 1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3C), 0xA, 0, 1, 0);
    obj->be_flag |= 0x1000;
    obj->Motion.Seq_speed = zero;
    obj->setNoSuspend(1);
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x3E), 0xA, 0, 1, 0);
    obj->LightInfo.EnableMask = lit;
    obj->be_flag |= 0x10;
    EstSet(obj, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_NONE, 0, 0);
    SceAtSetEnable(0xB, 1);
}

// The shelf (object 9) falls open (OpenBoxFall type 0x15) and is pinned at its fallen pose.
void r206_openShelf_main(int no, int opened)
{
    cObj* obj;

    OpenBoxMain(OpenBoxFall, opened, 0x15, 9, -1, -1);
    obj = SmdGetObjPtr(9);
    if (obj != NULL) {
        Vec* rot = &obj->ang;

        obj->pos.x = -401.0f;
        obj->pos.y = 8028.0f;
        obj->pos.z = -4054.0f;
        obj->ang.x = 1.5592f;
        obj->ang.y = 1.42932f;
        obj->ang.z = 0.0f;
        obj->pList->ang.x = 0.0f;
        obj->pList->ang.y = 0.0f;
        obj->pList->ang.z = 0.0f;
        obj->setPos(&obj->pos);
        obj->setAng(rot);
    }
}

// Item-event opener: the shelf falls.
static void r206_openShelf(int no)
{
    r206_openShelf_main(no, 0);
}

// Item-event "already opened": the shelf posed fallen.
static void r206_openedShelf(int no)
{
    r206_openShelf_main(no, 1);
}

// After the reunion: disable the key item's area (it was taken in Ashley's section).
static void destroy_key_atari()
{
    SceAtWork* at;

    SceSleep(1);
    at = GetKeyItemAtari();
    if (at != NULL) {
        SceAtSetEnable(at->no, 0);
    } else {
        pLog->err(0, 0, "KEY ATARI NOT FOUND!!");
    }
}
