#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj13.h"
#include "em.h"
#include "emdoor.h"
#include "emwindow.h"
#include "em_set.h"
#include "em_wrap.h"
#include "pl14.h"
#include "etc_model.h"
#include "read.h"
#include "dvd.h"
#include "datactrl.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "motion.h"
#include "math_sub.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "snd.h"
#include "rnd.h"
#include "room_data.h"
#include "flr_at.h"
#include "merchant.h"
#include "TexRender.h"
#include "db_log.h"
#include "eprintf.h"

// Room 1-1C (D:/Bio4/Prog/r11c.cpp): the village square at night; the besieged cabin (s00: Luis
// arrives, s10: the Ganado wave, s20: the escape), the two gate levers with the gear/chain machinery,
// the merchant.

void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp


struct R11cWork {
    ReadModule* mod3;        // 0x00  enemy module 3 (Luis; the s00 event data is swapped into its archive)
    ReadModule* mod4;        // 0x04  enemy module 4 (Ashley; the s10 event data)
    u8 pad_8[0x30 - 0x08];
    cDataUnit* evd0;         // 0x30  evd/r11cs00.evd
    cDataUnit* evd1;         // 0x34  evd/r11cs10.evd
    cEm* ashley;             // 0x38  EmMgr.create(4) for the s10 event
    cEm* em;                 // 0x3C  enemy list entry 0xC8 (Luis outside)
    cObj* fire[3];           // 0x40  the bonfires (setFire)
    TexRenderMng* tex;       // 0x4C  TexRenderInit output (the s20 event's water surface)
    u8 texTbl[0x80];         // 0x50  TexRenderModSet parts table
    cObjLadder* ladder[4];   // 0xD0  etc ladders 6..9
    f32 gateY[2];            // 0xE0  rest pos.y of the two gates (smd 0x33 / 0x34)
    SCE_TASK* closeGate;      // 0xE8  r11c_closeGate task
    SCE_TASK* gear;           // 0xEC  r11c_moveGear task
    SCE_TASK* chain;          // 0xF0  r11c_moveChain task
    int eff;                 // 0xF4  EspPullCoreKind of the room ambience
    int effGear;             // 0xF8  EspPullCoreKind of the gear effect
    int effGate;             // 0xFC  EspPullCoreKind of the gate effect
    u32 seGear;              // 0x100 SndCall handle of the gears
    u32 seGate;              // 0x104 SndCall handle of the gate
};

static R11cWork* r11c_work;
#define W r11c_work

// The room save block: +4 is the flag word (bit 25 event done, bits 21..24 ladders 3..0 down,
// bit 30 a route chosen, bit 29 the right route).
struct R11cSave {
    u32 x0;
    u32 flags;
};

static inline R11cSave* r11c_save() { return (R11cSave*) RoomData.getRoomSavePtr(pG->room_id); }

// Death bit of entry `no` of the loaded enemy list (0 while no list is loaded), em_set.cpp style: the
// row offset is added to pG before the table offset (`lwz 0x5034(pG + list * 0x20)`).
static inline u32 r11c_emDead(u32 no)
{
    u32 v;

    if (pG->em_list_no >= 0) {
        u32* tbl = pG->Em_flg[pG->em_list_no];

        v = tbl[no >> 5] & (0x80000000 >> (no & 31));
    } else {
        v = 0;
    }
    return v;
}

extern "C" void r11c_eventInit();
static void r11c_EventBesiegedStart();
static void r11c_ThunderMove();
extern "C" void r11c_initGate();
extern "C" void r11c_openGate(u32 id);
static void r11c_closeGate(u32 id);
static void r11c_moveGear(int dir);
static void r11c_moveChain(int dir);
static void r11c_moveLever2(int dir);
extern "C" void r11c_moveLever(int dir, int noGear);
static void r11c_selectRoute_end(int sel);
static void r11c_selectRoute();
static void r11c_operator();
extern "C" void setFire();
extern "C" void deleteFire();
extern "C" void Evt_R11CS00_Func(Event* e);
extern "C" void Evt_R11CS10_Func(Event* e);
extern "C" void Evt_R11CS20_Func(Event* e);

// Room init: thunder task, rain on the player, Status_flg[1] 0x400, the four etc ladders (all down
// before the siege event, else only those the save block's bits 21..24 remember as knocked down); until
// the siege is done (save flags bit 25) area 3 starts it and Luis (ESL 0xC8) waits outside; afterwards
// areas 8/9 are off, the merchant stock (stock_r11c / _after_event) is added, area 0xC is the typewriter
// and the gates follow the chosen route (r11c_initGate). Bonfires, room ambience effect, rack ranges.
void R11cInit()
{
    cEm* rack;
    cEm* em;
    void* arc;

#line 75 "D:/Bio4/Prog/r11c.cpp"
    W = (R11cWork*) MEM_CALLOC(sizeof(R11cWork), 1, 0xd);

    SceExec(0x12, (TaskFunc) r11c_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 0, 0x800, ESP_CORE_KIND_NONE, 0, 0);
    EstSet(pPL, -1, 0, 0, EFF_PL00, 1, 0x800, ESP_CORE_KIND_NONE, 0, 0);
    EstSet(pPL, -1, 0, 0, EFF_CORE, 0x23, 0x800, ESP_CORE_KIND_NONE, 0, 0);
    StaFlagOn(pG, STA_ROOM_RAIN);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    getRoomEtcLadder(6, &W->ladder[0], 1);
    getRoomEtcLadder(7, &W->ladder[1], 1);
    getRoomEtcLadder(8, &W->ladder[2], 1);
    getRoomEtcLadder(9, &W->ladder[3], 1);
    if (!(r11c_save()->flags & 0x02000000)) {
        if (W->ladder[0]) {
            W->ladder[0]->setOff();
            W->ladder[0]->setDowned();
        }
        if (W->ladder[1]) {
            W->ladder[1]->setOff();
            W->ladder[1]->setDowned();
        }
        if (W->ladder[2]) {
            W->ladder[2]->setOff();
            W->ladder[2]->setDowned();
        }
        if (W->ladder[3]) {
            W->ladder[3]->setOff();
            W->ladder[3]->setDowned();
        }
    } else {
        if (!(r11c_save()->flags & 0x01000000) && W->ladder[0]) {
            W->ladder[0]->setDowned();
        }
        if (!(r11c_save()->flags & 0x00800000) && W->ladder[1]) {
            W->ladder[1]->setDowned();
        }
        if (!(r11c_save()->flags & 0x00400000) && W->ladder[2]) {
            W->ladder[2]->setDowned();
        }
        if (!(r11c_save()->flags & 0x00200000) && W->ladder[3]) {
            W->ladder[3]->setDowned();
        }
    }
    if (getRoomEtcRack(0, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 0.0f, 0.0f, 10000.0f);
    }
    if (getRoomEtcRack(1, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 10000.0f, 0.0f, 10000.0f);
    }
    if (getRoomEtcRack(2, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 10000.0f, 0.0f, 10000.0f);
    }
    if (!ScfFlagChk(pG, SCF_R11C_BESIEGED_EVENT)) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r11c_EventBesiegedStart, 0, 1);
        r11c_eventInit();
        if (!r11c_emDead(0xC8)) {
            W->em = EmSetFromList2(0xC8, 0);
            if (W->em != 0) {
                Vec pos = {62402.0f, -0.33f, -58554.0f};
                Vec ang;
                f32 ry = -2.86535f;
                cEm* e = W->em;
                Vec* pa = &ang;

                e->setPos(&pos);
                ang.x = 0.0f;
                pa->y = ry;
                ang.z = 0.0f;
                e->setAng(&ang);
            }
        }
        W->eff = EspPullCoreKind();
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, (u8) W->eff, 0, 0);
    } else {
        if (!r11c_emDead(0xC8)) {
            EmSetFromList2(0xC8, 0);
        }
        SceAtSetEnable(8, 0);
        SceAtSetEnable(9, 0);
        SmdGetObjPtr(0x3F)->be_flag &= ~2;
    }
    {
        cObj* g0 = SmdGetObjPtr(0x33);
        cObj* g1 = SmdGetObjPtr(0x34);

        if (g0 && g1) {
            g0->be_flag |= 0x20;
            g1->be_flag |= 0x20;
            W->gateY[0] = g0->pos.y;
            W->gateY[1] = g1->pos.y;
            W->effGear = EspPullCoreKind();
            W->effGate = EspPullCoreKind();
        }
    }
    r11c_initGate();
    EvtMgr.SetFunc("evt_r11cs00_func", (void*) Evt_R11CS00_Func);
    EvtMgr.SetFunc("evt_r11cs10_func", (void*) Evt_R11CS10_Func);
    EvtMgr.SetFunc("evt_r11cs20_func", (void*) Evt_R11CS20_Func);
    if (!ScfFlagChk(pG, SCF_R11C_OPERATOR)) {
        SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, (TaskFunc) r11c_operator, 0, 1);
    }
    if (EtcGetDasAddr(ETC_TAIMATU01, &arc)) {
        RoomEfmRegist(GetEtcAddr(arc, "et1400.bin"), GetEtcAddr(arc, "et1400.tpl"), 0x6F);
    }
    FlrAtSetDefVal(0, 0, 3);
    if (!ScfFlagChk(pG, SCF_R11C_BESIEGED_EVENT) && RoomData.checkPassed(pG->room_id, 0) == 0) {
        levelDataAdd(merchantData, level_null);
        stockDataAdd(merchantData, stock_r11c);
    }
    merchantChar.setChar(&merchant_info_A, merchantData, g_item_price_tbl, g_item_price_tbl, level_price);
    TexRenderInit(&W->tex, 0, 2);
    CamCtrl.AreaOnOff(1, 0, 0);
}

// The ladders the player knocked down are remembered in the room save block.
void R11cMain()
{
    cObjLadder* ladder;

    if (r11c_save()->flags & 0x02000000) {
        if (getRoomEtcLadder(6, &ladder, 1) && ladder->getStatus() == 0) {
            r11c_save()->flags |= 0x01000000;
        }
        if (getRoomEtcLadder(7, &ladder, 1) && ladder->getStatus() == 0) {
            r11c_save()->flags |= 0x00800000;
        }
        if (getRoomEtcLadder(8, &ladder, 1) && ladder->getStatus() == 0) {
            r11c_save()->flags |= 0x00400000;
        }
        if (getRoomEtcLadder(9, &ladder, 1) && ladder->getStatus() == 0) {
            r11c_save()->flags |= 0x00200000;
        }
    }
}

// Before the siege: pre-load evd r11cs00 to ARAM and register r11cs10, pre-read enemy modules 0x13 and 3
// (Luis), mark Ashley as following (Status_flg[3] 0x04000000), init the partner at Leon's position in
// chase mode, and keep module 3 for the event-data swap.
extern "C" void r11c_eventInit()
{
    W->evd0 = DC.setData(EvtMgr.NameChange("evd/r11cs00.evd"));
    W->evd0->setCommand(CMND_ARAM_LOAD, 0, 0);
    W->evd1 = DC.setData(EvtMgr.NameChange("evd/r11cs10.evd"));
    EmReadSearch(0x13, 0, W->evd0->m_size);
    EmReadSearch(3, 0, 0x120000);
    StaFlagOn(pG, STA_SUB_ASHLEY);
    SubCharInit(1, &pPL->pos, pPL->ang.y);
    SubCharCtrl(SCC_CHASE, 0);
    W->mod3 = SearchEmModule(3);
}

// Area 3: the cabin siege. The s00 event, the Ganado waves (list 0xC9..0xD1) until 40 are down or
// 3600 frames passed, then the s10 event and the chapter end.
static void r11c_EventBesiegedStart()
{
    cEmDoor* door;
    ReadModule* mod;
    int err;

    ScfFlagOn(pG, SCF_R11C_BESIEGED_EVENT);
    RmfFlagOn(pG, RMF_BESIEGEDING);
    EffectEspDelete(0, (u8) W->eff, 0, 0);
    EffectEspgenDelete(0, (u8) W->eff, 0);
    EffectEfmDelete(0, (u8) W->eff, 0);
    SceEventStart(0);
    if (pSUB) {
        EmMgr.destroy(pSUB);
        StaFlagOff(pG, STA_SUB_ASHLEY);
    }
    if (getRoomEtcDoor(0xA, &door, 1)) {
        door->setNoSuspend(0);
        door->setCloseLock();
    }
    SysFlagOn(pG, SYS_SCREEN_STOP);
    err = W->evd0->waitLoadOk() == 0;
    EmMgr.destroy(W->em);
    SceSleep(2);
    InitModule(W->mod3);
    mod = SearchEmModule(0x13);
    if (err == 1) {
        pLog->err(0, 0, "r11c_Event00 data read error");
        {
            Vec pos = {109264.0f, 4.0f, -50575.0f};
            Vec ang;
            f32 ry = 1.11f;
            cPlayer* pl = pPL;
            Vec* pa = &ang;

            pl->setPos(&pos);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
    } else if (W->evd0->m_size > mod->size) {
        pLog->err(0, 0, "r11c_Event00 data size over");
        {
            Vec pos = {109264.0f, 4.0f, -50575.0f};
            Vec ang;
            f32 ry = 1.11f;
            cPlayer* pl = pPL;
            Vec* pa = &ang;

            pl->setPos(&pos);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
    } else {
        MemorySwap(mod->pArc, (u32) W->evd0->m_addr, W->evd0->m_size);
        EvtMgr.SetEvt(mod->pArc, (u32*) 0);
        SceSleep(2);
        SceSleep(2);
        while (EvtMgr.IsAliveEvt(EvtMgr.GetNowExeEvtNamePtr(), 0, 0) != 0) {
            SceSleep(1);
        }
        MemorySwap(mod->pArc, (u32) W->evd0->m_addr, W->evd0->m_size);
    }
    W->evd0->setCommand(CMND_DEL_DATA, 0, 0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    EmReadSearch(4, 0, 0x120000);
    W->ashley = EmMgr.create(4);
    // Mid-function declarations: the two error arms above own block-local pos/ang pairs, so these
    // reuse their (combined) temp slots and the em[] array lands behind them.
    Vec pos = {107427.0f, 4.0f, -48370.0f};
    Vec ang;
    f32 ry = 1.27f;
    cEm* ashley = W->ashley;
    Vec* pa = &ang;

    ashley->setPos(&pos);
    ang.x = 0.0f;
    pa->y = ry;
    ang.z = 0.0f;
    ashley->setAng(&ang);
    W->ashley->set = 1;
    W->mod4 = SearchEmModule(4);
    {
        cPlayer* pl = pPL;
        const f32 x = 107436.0f;
        const f32 y = 4.0f;
        const f32 z = -47160.0f;
        f32 ry2 = -2.41f;

        pos.x = x;
        pos.y = y;
        pos.z = z;
        pl->setPos(&pos);
        pos.x = 0.0f;
        pos.y = ry2;
        pos.z = 0.0f;
        pl->setAng(&pos);
    }
    cEm* em[20];
    cEm* e;
    cEmWindow* win;
    cObjLadder* ladder;
    Event* ev;
    cEmDoor* door2;
    cEmWindow* win2;
    int n;
    int i;
    int kill;
    int t;
    ReadModule* mod2;
    int err2;

    W->evd1->setCommand(CMND_ARAM_LOAD, 0, 0);
    SceSleep(2);
    SysFlagOff(pG, SYS_SCREEN_STOP);
    SceEventEnd(0);
    CamCtrl.AreaOnOff(1, 0, 1);
    SndRoomStrStart(1, 3, 1);
    EvtMgr.EvtReadAram("event/evd/r11cs20.evd", 0, 0, 0, 0);
    EffectEspDelete(0, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(0, ESP_CORE_KIND_ROOM00, 0);
    setFire();
    if (getRoomEtcWindow(3, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    if (getRoomEtcWindow(4, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    if (getRoomEtcWindow(5, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    if (getRoomEtcWindow(0x12, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    if (getRoomEtcWindow(0x13, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    if (getRoomEtcWindow(0x14, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    if (getRoomEtcWindow(0x15, &win, 1)) {
        win->SetEnableFence(0, 1);
    }
    n = 0;
    e = EmSetFromList2(0xC9, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xCA, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xCB, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xCC, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xCD, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xCE, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xCF, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xD0, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    e = EmSetFromList2(0xD1, 0);
    if (e) {
        em[n] = e;
        n++;
    }
    kill = 0;
    t = 0;
    while (1) {
        for (i = 0; i < n; i++) {
            if (RmfFlagChk(pG, RMF_LUIS_ANGRY)) {
                SceEventStart(0);
                SndRoomStrStop(3);
                SysFlagOn(pG, SYS_SCREEN_STOP);
                SceDestroyEm(0x10, 0x20);
                SceSleep(2);
                InitModule(SearchEmModule(0x13));
                EvtMgr.EvtReadExec("event/evd/r11cs20.evd", 0, EvtReadFlagDiedemo);
                SceEventEnd(0);
                SceExit();
            }
            e = em[i];
            if (((cEmGanado*) e)->ckResetEnable() == 1) {
                kill++;
                if (r11c_save()->flags & 0x02000000) {
                    ((cEmGanado*) e)->chgSet(0xE);
                }
                ((cEmGanado*) e)->setReset();
            }
        }
        t++;
        if (t == 3600) {
            r11c_save()->flags |= 0x02000000;
            W->ashley->set = 2;
            if (getRoomEtcLadder(6, &ladder, 1)) {
                ladder->setOn();
            }
            if (getRoomEtcLadder(7, &ladder, 1)) {
                ladder->setOn();
            }
            if (getRoomEtcLadder(8, &ladder, 1)) {
                ladder->setOn();
            }
            if (getRoomEtcLadder(9, &ladder, 1)) {
                ladder->setOn();
            }
        }
        if (kill > 39 || t > 8999 || DebugTrg(0) == 1) {
            break;
        }
        SceDebugDisp("T[%d]", 9000 - t);
        SceDebugDisp("E[%d]", 40 - kill);
        SceSleep(1);
    }
    EffectEspDelete(0, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(0, ESP_CORE_KIND_ROOM00, 0);
    deleteFire();
    if (W->ladder[0]) {
        W->ladder[0]->be_flag &= ~2;
    }
    if (W->ladder[1]) {
        W->ladder[1]->be_flag &= ~2;
    }
    if (W->ladder[2]) {
        W->ladder[2]->be_flag &= ~2;
    }
    if (W->ladder[3]) {
        W->ladder[3]->be_flag &= ~2;
    }
    SndRoomStrStop(5);
    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    err2 = W->evd1->waitLoadOk() == 0;
    for (i = 0; i < n; i++) {
        e = em[i];
        if (e->isAlive()) {
            EmMgr.destroy(e);
        }
    }
    EmMgr.destroy(W->ashley);
    pSUB = 0;
    SceSleep(2);
    InitModule(W->mod4);
    mod2 = SearchEmModule(0x13);
    if (err2 != 1) {
        if (W->evd1->m_size > mod2->size) {
            pLog->err(0, 0, "r11c_Event10 exec error");
        } else {
            MemorySwap(mod2->pArc, (u32) W->evd1->m_addr, W->evd1->m_size);
            if (EvtMgr.SetEvt(mod2->pArc, (u32*) &ev)) {
                ev->StatusFlag |= EvtStfBit(EvtStfFadeOut);
            }
            while (EvtMgr.IsAliveEvt(EvtMgr.GetNowExeEvtNamePtr(), 0, 0) != 0) {
                SceSleep(1);
            }
            MemorySwap(mod2->pArc, (u32) W->evd1->m_addr, W->evd1->m_size);
        }
    }
    W->evd1->setCommand(CMND_DEL_DATA, 0, 0);
    SysFlagOff(pG, SYS_SCREEN_STOP);
    SceEventEnd(0);
    r11c_initGate();
    StaFlagOn(pG, STA_SUB_ASHLEY);
    SubCharInit(1, &pPL->pos, pPL->ang.y);
    SubCharCtrl(SCC_CHASE, 0);
    if (!r11c_emDead(0xC8)) {
        pG->Em_list[0xC8].be_flag &= ~2;
        EmSetFromList2(0xC8, 0);
    }
    CamCtrl.AreaOnOff(1, 0, 0);
    if (getRoomEtcDoor(0xA, &door2, 1)) {
        door2->setNormal();
    }
    if (getRoomEtcWindow(3, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (getRoomEtcWindow(4, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (getRoomEtcWindow(5, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (getRoomEtcWindow(0x12, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (getRoomEtcWindow(0x13, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (getRoomEtcWindow(0x14, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (getRoomEtcWindow(0x15, &win2, 1)) {
        win2->SetEnableFence(1, 1);
    }
    if (W->ladder[0]) {
        W->ladder[0]->be_flag |= 2;
    }
    if (W->ladder[1]) {
        W->ladder[1]->be_flag |= 2;
    }
    if (W->ladder[2]) {
        W->ladder[2]->be_flag |= 2;
    }
    if (W->ladder[3]) {
        W->ladder[3]->be_flag |= 2;
    }
    {
        // The zero for EstSet's two stack arguments is born at the top of the block (it crosses the
        // calls below, so sched1 may hoist it): a block-local, as in r111.
        void* zero = 0;

        ScfFlagOn(pG, SCF_R11C_BESIEGED_END_EVENT);
        stockDataAdd(merchantData, stock_r11c_after_event);
        merchantChar.setChar(&merchant_info_A, merchantData, g_item_price_tbl, g_item_price_tbl, level_price);
        SceAtSetEnable(8, 0);
        SceAtSetEnable(9, 0);
        SmdGetObjPtr(0x3F)->be_flag &= ~2;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_NONE, zero, zero);
    }
    RmfFlagOff(pG, RMF_BESIEGEDING);
    SceSetChapterEnd(CHAPTER_2_2, -1);
}

// Thunder while the siege is not running.
// The loop body is written in small statements on purpose: the loop is one haifa scheduling region
// only if its LUID span (insns + notes) is <= 100, and the target shows no interblock motion (the
// `cmpwi cnt` stays behind the flag branch, the EstSet arg `li`s stay behind `bne`), so the target's
// loop was over that limit.
static void r11c_ThunderMove()
{
    int cnt = 0;

    SceSleep(1);
    for (;;) {
        u32 f;

        f = pG->Room_flg[0];
        if (!(f & 0x40000000)) {
            if (cnt <= 0) {
                int st;

                st = EffGetAreaState(3);
                if (st == 0) {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, 0, 0);
                } else {
                    EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_NONE, 0, 0);
                }
                SceSndCallThunder();
                {
                    u8 r;
                    int c;

                    r = Rnd();
                    r %= 30;
                    c = r * 5;
                    c += 90;
                    cnt = c;
                }
            }
            cnt--;
        } else {
            u8 r;
            int c;

            r = Rnd();
            r %= 30;
            c = r * 5;
            c += 90;
            cnt = c;
        }
        SceSleep(1);
    }
}

// The two gates (smd 0x33 left, 0x34 right): up before the siege, then by the chosen route.
extern "C" void r11c_initGate()
{
    cObj* g0 = SmdGetObjPtr(0x33);
    cObj* g1 = SmdGetObjPtr(0x34);

    if (g0 && g1) {
        if (!ScfFlagChk(pG, SCF_R11C_BESIEGED_EVENT)) {
            const f32 h = 3600.0f;

            g0->pos.y += h;
            g1->pos.y += h;
        } else {
            SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r11c_selectRoute, 0, 2);
            if (!(r11c_save()->flags & 0x40000000)) {
                g0->pos.y = 0.0f;
                g1->pos.y = 0.0f;
            } else {
                if (r11c_save()->flags & 0x20000000) {
                    g1->pos.y += 3600.0f;
                    SceAtSetEnable(5, 0);
                } else {
                    g0->pos.y += 3600.0f;
                    SceAtSetEnable(6, 0);
                }
                SceAtSetEnable(0xA, 0);
                SceAtSetEnable(0xB, 0);
            }
        }
    }
}

// Task: raise gate `id` (smd 0x33 / 0x34) by 3600 units over 80 frames with a rattle, gate effect and
// sound; sets Room_flg[2] bit 31 (a gate is open).
extern "C" void r11c_openGate(u32 id)
{
    cObj* g = SmdGetObjPtr(id);
    const f32 step = 45.0f;
    f32 dst;
    int i;

    g->be_flag |= 0x20;
    dst = 3600.0f + g->pos.y;
    EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, (u8) W->effGate, 0, 0);
    W->seGate = SndCall(6, 0x57, 0, 0, 0, 0);
    {
        f32 sw[4] = {10.0f, -10.0f, 20.0f, -20.0f};

        for (i = 0; i < 80; i++) {
            g->pos.y += sw[i % 4] + step;
            SceSleep(1);
        }
    }
    g->pos.y -= step;
    SceSleep(1);
    g->pos.y += step;
    SceSleep(1);
    g->pos.y = dst;
    EffectEspDelete(0, (u8) W->effGate, 0, 0);
    EffectEspgenDelete(0, (u8) W->effGate, 0);
    EffectEfmDelete(0, (u8) W->effGate, 0);
    RmfFlagOn(pG, RMF_GATE_OPEN);
    SceSleep(10);
}

// Task: drop gate `id` 3600 units with acceleration 20/frame^2, then clears the task handle W->closeGate.
static void r11c_closeGate(u32 id)
{
    cObj* g = SmdGetObjPtr(id);
    f32 dst = g->pos.y - 3600.0f;
    const f32 acc = 20.0f;
    f32 spd = 0.0f;

    g->be_flag |= 0x20;
    // The exit store on the break path keeps jump1 from folding the peeled exit test over `b END`;
    // jump2's cross-jump merges the two exit jumps (`cmp; b TEST; ...; TEST: bge TOP; stfs dst`).
    for (;;) {
        g->pos.y -= spd;
        spd += acc;
        if (g->pos.y < dst) {
            g->pos.y = dst;
            break;
        }
        SceSleep(1);
    }
    W->closeGate = 0;
}

// The gear train (smd 0x10, 0x11, 0x32) turns while the gate moves (sceat_x17C bit 31).
static void r11c_moveGear(int dir)
{
    cObj* g0;
    cObj* g1;
    cObj* g2;
    const f32 t0 = 72.0f;
    const f32 t2 = 63.0f;
    const f32 t1 = 36.0f;
    const f32 max = 1.0f;
    const f32 acc = 0.04f;
    f32 t;
    f32 d;

    SceSleep(15);
    g0 = SmdGetObjPtr(0x10);
    g1 = SmdGetObjPtr(0x11);
    g2 = SmdGetObjPtr(0x32);
    t = 0.0f;
    g0->be_flag |= 0x20;
    g1->be_flag |= 0x20;
    g2->be_flag |= 0x20;
    W->seGear = SndCall(6, 0x56, 0, 0, 0, 0);
    d = (f32) dir;
    g0->pList->ang.z += d * DEG(0.2f);
    SceSleep(1);
    g0->pList->ang.z += d * DEG(0.4f);
    SceSleep(1);
    d *= DEG(0.6f);
    g0->pList->ang.z += d;
    g1->pList->ang.y += d;
    SceSleep(1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, (u8) W->effGear, 0, 0);
    while (!RmfFlagChk(pG, RMF_GATE_OPEN)) {
        f32 a;

        t += acc;
        if (t > max) {
            t = max;
        }
        a = t * DEG(1.0f) * (f32) dir;
        g0->pList->ang.z += a;
        g1->pList->ang.y += -a * t0 / t1;
        g2->pList->ang.z += -a * t0 / t1 * t2 / t0;
        SceSleep(1);
    }
    EffectEspDelete(0, (u8) W->effGear, 0, 0);
    EffectEspgenDelete(0, (u8) W->effGear, 0);
    EffectEfmDelete(0, (u8) W->effGear, 0);
    SceSleep(10);
    RmfFlagOff(pG, RMF_GATE_OPEN);
    W->gear = 0;
}

// The chain (smd 2 / 3) drops while the gate moves.
static void r11c_moveChain(int dir)
{
    u32 id = 2;
    cObj* c;
    const f32 max = 15.0f;
    const f32 acc = 1.0f;
    f32 spd;

    if (dir == 1) {
        id = 3;
    }
    SceSleep(15);
    c = SmdGetObjPtr(id);
    spd = 0.0f;
    c->be_flag |= 0x20;
    while (!RmfFlagChk(pG, RMF_GATE_OPEN)) {
        spd += acc;
        if (spd > max) {
            spd = max;
        }
        c->pos.y -= spd;
        SceSleep(1);
    }
    W->chain = 0;
}

// The lever (smd 0x35) swings back.
static void r11c_moveLever2(int dir)
{
    cObj* lv;
    int i;
    f32 spd;

    lv = SmdGetObjPtr(0x35);
    spd = (f32) dir * DEG(5.5f);
    for (i = 0; i < 10; i++) {
        lv->pList->ang.z -= spd;
        SceSleep(1);
    }
    spd = (f32) dir * DEG(1.0f);
    for (i = 0; i < 3; i++) {
        lv->pList->ang.z += spd;
        SceSleep(1);
    }
    lv->pList->ang.z = 0.0f;
}

// The lever is pulled; unless `noGear`, the gear and chain tasks start.
extern "C" void r11c_moveLever(int dir, int noGear)
{
    cObj* lv;
    int i;
    f32 d;
    f32 d2;
    f32 spd;

    lv = SmdGetObjPtr(0x35);
    lv->be_flag |= 0x20;
    lv->pList->ang.z = 0.0f;
    d = (f32) dir;
    spd = d * DEG(5.5f);
    SndCall(6, 0xA, 0, 0, 0, 0);
    d2 = d * DEG(1.0f);
    for (i = 0; i < 3; i++) {
        lv->pList->ang.z += d2;
        SceSleep(1);
    }
    for (i = 0; i < 10; i++) {
        lv->pList->ang.z += spd;
        SceSleep(1);
    }
    d2 = (f32) dir * DEG(1.0f);
    for (i = 0; i < 3; i++) {
        lv->pList->ang.z -= d2;
        SceSleep(1);
    }
    if (noGear == 0) {
        W->gear = SceExec(0x12, (TaskFunc) r11c_moveGear, dir, 0, SCE_PRIO_DEF_2, 0);
        W->chain = SceExec(0x12, (TaskFunc) r11c_moveChain, dir, 0, SCE_PRIO_DEF_2, 0);
    }
}

// End (or cancel, flags_174 bit 28) of the route selection: stop the machinery, set the gates and the
// route flags.
static void r11c_selectRoute_end(int sel)
{
    cObj* g0 = SmdGetObjPtr(0x33);
    cObj* g1 = SmdGetObjPtr(0x34);
    cObj* lv = SmdGetObjPtr(0x35);

    if (RmfFlagChk(pG, RMF_ROUTE_EVT_CANCEL)) {
        if (lv) {
            lv->pList->ang.z = 0.0f;
        }
        if (W->closeGate) {
            SceKill(W->closeGate);
        }
        if (W->gear) {
            SceKill(W->gear);
        }
        if (W->chain) {
            SceKill(W->chain);
        }
        if (W->seGate) {
            SndStop(W->seGate, 0);
        }
        if (W->seGear) {
            SndStop(W->seGear, 0);
        }
        EffectEspDelete(0, (u8) W->effGear, 0, 0);
        EffectEspgenDelete(0, (u8) W->effGear, 0);
        EffectEfmDelete(0, (u8) W->effGear, 0);
        EffectEspDelete(0, (u8) W->effGate, 0, 0);
        EffectEspgenDelete(0, (u8) W->effGate, 0);
        EffectEfmDelete(0, (u8) W->effGate, 0);
        RmfFlagOff(pG, RMF_GATE_OPEN);
    }
    W->seGear = 0;
    W->seGate = 0;
    if (sel < 0) {
        if (RmfFlagChk(pG, RMF_ROUTE_EVT_CANCEL)) {
            if (g0) {
                g0->pos.y = W->gateY[0] + 3600.0f;
            }
            if (g1) {
                g1->pos.y = W->gateY[1];
            }
        }
        r11c_save()->flags |= 0x40000000;
        r11c_save()->flags &= ~0x20000000;
        SceAtSetEnable(6, 0);
        SceAtSetEnable(5, 1);
        SceAtSetEnable(0xA, 0);
        SceAtSetEnable(0xB, 0);
    } else {
        if (RmfFlagChk(pG, RMF_ROUTE_EVT_CANCEL)) {
            if (g0) {
                g0->pos.y = W->gateY[0];
            }
            if (g1) {
                g1->pos.y = W->gateY[1] + 3600.0f;
            }
        }
        r11c_save()->flags |= 0x40000000;
        r11c_save()->flags |= 0x20000000;
        SceAtSetEnable(6, 1);
        SceAtSetEnable(5, 0);
        SceAtSetEnable(0xA, 0);
        SceAtSetEnable(0xB, 0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 4: the lever. Message 0 selects the route; the gate opens (and the other closes).
static void r11c_selectRoute()
{
    W->closeGate = 0;
    W->gear = 0;
    W->chain = 0;
    W->seGate = 0;
    W->seGear = 0;
    RmfFlagOff(pG, RMF_GATE_OPEN);
    SceEventStart(0);
    CamCtrl.CutCall(3);
    SceMesSet(0, 0x220, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    switch (SceMesGetSelection()) {
    case 1:
        if ((r11c_save()->flags & 0x40000000) && !(r11c_save()->flags & 0x20000000)) {
            r11c_moveLever(-1, 1);
            SceExec(0x12, (TaskFunc) r11c_moveLever2, -1, 0, SCE_PRIO_DEF_2, 0);
        } else {
            SceSetEventCancel(1, (TaskFunc) r11c_selectRoute_end, -1, 3, 1);
            r11c_moveLever(-1, 0);
            CamCtrl.CutCall(4);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            CamCtrl.CutCall(5);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            CamCtrl.CutCall(6);
            r11c_openGate(0x33);
            if (r11c_save()->flags & 0x40000000) {
                W->closeGate = SceExec(0x12, (TaskFunc) r11c_closeGate, 0x34, 0, SCE_PRIO_DEF_2, 0);
            }
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceExec(0x12, (TaskFunc) r11c_moveLever2, -1, 0, SCE_PRIO_DEF_2, 0);
            SceSetEventCancel(0, 0, 0, -1, 1);
            r11c_selectRoute_end(-1);
            return;
        }
        break;
    case 2:
        if ((r11c_save()->flags & 0x40000000) && (r11c_save()->flags & 0x20000000)) {
            r11c_moveLever(1, 1);
            SceExec(0x12, (TaskFunc) r11c_moveLever2, 1, 0, SCE_PRIO_DEF_2, 0);
        } else {
            SceSetEventCancel(1, (TaskFunc) r11c_selectRoute_end, 1, 3, 1);
            r11c_moveLever(1, 0);
            CamCtrl.CutCall(4);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            CamCtrl.CutCall(5);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            CamCtrl.CutCall(7);
            r11c_openGate(0x34);
            if (r11c_save()->flags & 0x40000000) {
                W->closeGate = SceExec(0x12, (TaskFunc) r11c_closeGate, 0x33, 0, SCE_PRIO_DEF_2, 0);
            }
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceExec(0x12, (TaskFunc) r11c_moveLever2, 1, 0, SCE_PRIO_DEF_2, 0);
            SceSetEventCancel(0, 0, 0, -1, 1);
            r11c_selectRoute_end(1);
            return;
        }
        break;
    case -1:
        break;
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 0xC: the typewriter terminal.
static void r11c_operator()
{
    if (!ScfFlagChk(pG, SCF_R11C_OPERATOR)) {
        ScfFlagOn(pG, SCF_R11C_OPERATOR);
        SceAtSetEnable(0xC, 0);
        OpeSetOpenTerm(0xB, 71600.0f, -10.0f, -55420.0f, 1.6f);
    }
}

// The three bonfires (room archive 0x1F/0x20, motion 0x21).
extern "C" void setFire()
{
    {
        Vec pos = {112339.0f, -522.0f, -61101.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        cObj* o;

        o = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0x10, 1);
        W->fire[0] = o;
        if (o == 0) {
            pLog->err(0, 0, "setFire : set failed");
            return;
        }
        MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 5, 0);
        EstSet(o, -1, 0, 0, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_ROOM00, 0, 0);
    }
    {
        Vec pos = {97770.0f, -598.0f, -49354.0f};
        Vec rot = {0.0f, DEG(-80.0f), 0.0f};
        cObj* o;

        o = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0x10, 1);
        W->fire[1] = o;
        if (o == 0) {
            pLog->err(0, 0, "setFire : set failed");
            return;
        }
        MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 5, 0);
        EstSet(o, -1, 0, 0, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_ROOM00, 0, 0);
    }
    {
        Vec pos = {122765.0f, -516.0f, -51246.0f};
        Vec rot = {0.0f, DEG(-80.0f), 0.0f};
        cObj* o;

        o = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0x10, 1);
        W->fire[2] = o;
        if (o == 0) {
            pLog->err(0, 0, "setFire : set failed");
            return;
        }
        MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 5, 0);
        EstSet(o, -1, 0, 0, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_ROOM00, 0, 0);
    }
}

// Destroy the three bonfire objects (the s20 event replaces them).
extern "C" void deleteFire()
{
    ObjMgr.destroy(W->fire[0]);
    ObjMgr.destroy(W->fire[1]);
    ObjMgr.destroy(W->fire[2]);
}

// Event r11cs00 handler: the cabin etc models, the scroll object 0x3F handed to the event.
extern "C" void Evt_R11CS00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec ang = {0.0f, 0.0f, 0.0f};
    void* mod;
    cObj* o;

    switch (e->FuncType) {
    case 0:
        setRoomEtcDisp(0x16, 0, 1);
        setRoomEtcDisp(0xF, 0, 1);
        setRoomEtcDisp(0xA, 0, 1);
        break;
    case 1:
        switch (e->NowCut) {
        case 0: {
            int frame = e->NowFrame;

            if (frame == 0) {
                EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
                EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM01, 0);
                EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM01, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 0xD, 0x2001, ESP_CORE_KIND_ROOM01, (void*) frame, (void*) frame);
                o = SmdGetObjPtr(0x3F);
                if (o) {
                    e->SetMod("scr0000", o, 5, 0, 2, 0);
                    o->be_flag |= 0x12;
                    o->setPos(&pos);
                    o->setAng(&ang);
                    o->be_flag |= 0x20;
                    e->EspSetModelPtr(o);
                }
                if (e->GetMod(&mod, "et1700", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        }
        case 0x13:
            setRoomEtcDisp(0, 0, 1);
            break;
        case 0x14:
            setRoomEtcDisp(0, 1, 1);
            break;
        }
        break;
    case 2: {
        SmdWork* w;

        SmdSetTrans(0x3F, 1);
        w = SmdGetWorkPtr(0x3F);
        o = SmdGetObjPtr(0x3F);
        if (o && w) {
            o->setPos(&w->pos);
            o->setAng(&w->rot);
        }
        setRoomEtcDisp(0, 1, 1);
        setRoomEtcDisp(0x16, 1, 1);
        setRoomEtcDisp(0xF, 1, 1);
        setRoomEtcDisp(0xA, 1, 1);
        break;
    }
    }
}

// Event r11cs10 handler: the ladders and the door.
extern "C" void Evt_R11CS10_Func(Event* e)
{
    switch (e->FuncType) {
    case 0: {
        cObj* o;

        LadderEventTrans(0);
        setRoomEtcDisp(0xA, 0, 1);
        o = SmdGetObjPtr(0x3F);
        if (o) {
            o->be_flag &= ~2;
        }
        break;
    }
    case 1:
        switch (e->NowCut) {
        case 0:
            break;
        case 8:
            setRoomEtcDisp(0, 0, 1);
            break;
        }
        break;
    case 2:
        setRoomEtcDisp(0xA, 1, 1);
        setRoomEtcDisp(0, 1, 1);
        LadderEventTrans(1);
        break;
    }
}

// Per-cut effect of the s20 event on the render texture.
static inline void r11c_evtEsp(Event* e, u8 no)
{
    if (e->NowFrame == 0) {
        EffectEspDelete(W->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
        EffectEspgenDelete(W->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
        EffectEfmDelete(W->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, no, W->tex->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
    }
}

// Event r11cs20 handler: the door, the render texture on the two players, the water effects.
extern "C" void Evt_R11CS20_Func(Event* e)
{
    // Function scope: the address-taken `door` of a case block is kept until the switch ends, so
    // a second block-local `door` would get its own slot.
    cEmDoor* door;

    switch (e->FuncType) {
    case 0:
        if (getRoomEtcDoor(0xA, &door, 1)) {
            door->setNoSuspend(1);
        }
        setRoomEtcDisp(0xA, 1, 1);
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod, 6, W->texTbl, W->tex, 0, 1, 1, 1, 1.0f);
                }
                if (e->GetMod(&mod, "pl0400", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod, 3, W->texTbl, W->tex, 0, 1, 1, 1, 1.0f);
                }
            }
            r11c_evtEsp(e, 0xE);
            break;
        case 1:
            r11c_evtEsp(e, 0xF);
            break;
        case 2:
            r11c_evtEsp(e, 0x10);
            break;
        case 3:
            r11c_evtEsp(e, 0x11);
            break;
        case 4:
            r11c_evtEsp(e, 0x12);
            break;
        case 5:
            r11c_evtEsp(e, 0x13);
            break;
        case 6:
            r11c_evtEsp(e, 0x14);
            break;
        }
        break;
    case 2:
        if (getRoomEtcDoor(0xA, &door, 1)) {
            door->setNoSuspend(0);
        }
        setRoomEtcDisp(0xA, 1, 1);
        break;
    }
}
