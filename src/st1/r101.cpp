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
#include "main.h"
#include "game.h"
#include "datactrl.h"
#include "dvd.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "area.h"
#include "scroll.h"
#include "obj.h"
#include "obj00.h"
#include "obj13.h"
#include "em.h"
#include "obj01.h"
#include "emdoor.h"
#include "emrack.h"
#include "emwindow.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "cam_ctrl.h"
#include "id_sys.h"
#include "item.h"
#include "mes.h"
#include "sscrn.h"
#include "fade.h"
#include "snd.h"
#include "math_sub.h"
#include "rnd.h"

// Room 1-01 (D:/Bio4/Prog/r101.cpp): the village; the first fight, the tower and the enemy resets,
// the church bell event (s30), the house event (s20), the binocular view (s00) and the door messages.

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup before the bl);
// an asm-labelled free declaration reproduces the call (same trick as dvd.h ReadCheckInfo).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");

struct R101Work {
    s8 emNum;             // 0x00  enemies alive when the fight started (+5 per reset wave)
    u8 pad_1[7];
    cDataUnit* evt00;     // 0x08  evd/r101s00.evd
    cDataUnit* evt21;     // 0x0C  evd/r101s21.evd
    cDataUnit* evt30;     // 0x10  evd/r101s30.evd
    cObj* obj00;          // 0x14
    u8 pad_18[8];
    cEm* ladder[3];       // 0x20  etc ladders 5 / 6 / 7
    cEmWrap em[10];       // 0x2C  the reset waves (5 per side)
    cEmWrap* pEm[10];     // 0xA4
};

static R101Work* r101_work;

// pPL read as a struct member: the load stays below the preceding Vec template stores (r102).
struct PlPtr {
    cPlayer* p;
};
#define pPLS (((PlPtr*) &pPL)->p)
// Same for pSys (em10.cpp): the load stays below the preceding `ang = pPL->pos` copy stores (Event00).
struct SystemWorkPtr {
    SYSTEM_SAVE_WORK* p;
};
#define pSysS (((SystemWorkPtr*) &pSys)->p)

// Pointer stores through a reference: the work pointer is reloaded after them (see st_room.h).
static inline void PSet(cEmWrap*& d, cEmWrap* v) { d = v; }
static inline void PSet(cDataUnit*& d, cDataUnit* v) { d = v; }
static inline void PSet(cObj*& d, cObj* v) { d = v; }

// Hit effects of attribute type 4
static const AtEffInfo r101_eff_info = {
    0, {0xD2, 0}, {1, 0xF}, {0, 0xB}, {0, 0xC}, {1, 0xE}, {1, 0xE}, {0, 0x36}, {0xD2, 0},
};

static void r101_checkTowerBesieged();
extern "C" void r101_setFlameBottle(Vec* from, Vec* to);
static void r101_checkEmNum();
static void r101_Event30_TitleCall();
static void r101_Event30();
static void r101_execOperator2();
static void r101_execOperator();
static void r101_DoorCk();
static void r101_Event20();
static void r101_checkEmReset2();
static void r101_checkEmReset_end(int side);
static void r101_checkEmReset();
static void r101_DoorDontOpen100();
static void r101_DoorDontOpen103();
static void r101_checkDoor102KeyUse();
static void r101_checkDoor102();
static void r101_DoorDontOpen3();
static void r101_checkFindPlayer(int mode);
static void r101_FindPlayer2();
extern "C" void r101_FindPlayer();
static void r101_setChickenFlag();
static void r101_Event00();
static void r101_callGanadoVoice();
extern "C" void Evt_R101S21_Func(Event* e);
extern "C" void Evt_R101S30_Func(Event* e);

// Marks list entry `no` alive; clears its death bit of the loaded list.
static inline void r101_emListOn(int no)
{
    EM_LIST(no)->be_flag |= 1;
}

// Clear the death bit of list entry `no` in the loaded enemy list's death words (pG+0x501C + list*0x20).
static inline void r101_emDeadClear(int no)
{
    int list = pG->em_list_no;

    if (list >= 0) {
        BitOff(*(u32*) ((list << 5) + (u32) pG + 0x501C), 0x80000000 >> (no & 31));
    }
}

// Room init (the village, chapter 1-1): the ten reset-wave handles; first visit (Scenario_flg[0] 0x2000)
// = typewriter + save; s21/s30 callbacks, floor hit effects, door 0xB lock models, rack ranges. Before
// the bell (Room_flg bit 7): until the fight starts (bit 6) the find-player watcher, the Ganado voices
// (area 0x13), the s00 binocular event (area 7), the kill counter, the door messages (areas 0/2); the
// house event s21 on area 8 with the door watcher unless bit 8; the tower siege, chicken and terminal
// tasks. After the bell: the post-fight layout, the stream watcher. Doors: area 1 until Key_flg[0]
// 0x02000000, area 0x19 (door 102 with its key) until 0x20000000; item area 0xA3.
void R101Init()
{
    cEm* door;
    cEm* rack;
    cEm* win;

#line 81 "D:/Bio4/Prog/r101.cpp"
    r101_work = (R101Work*) MEM_CALLOC(sizeof(R101Work), 1, 0xd);

    PSet(r101_work->pEm[0], &r101_work->em[0]);
    PSet(r101_work->pEm[1], &r101_work->em[1]);
    PSet(r101_work->pEm[2], &r101_work->em[2]);
    PSet(r101_work->pEm[3], &r101_work->em[3]);
    PSet(r101_work->pEm[4], &r101_work->em[4]);
    PSet(r101_work->pEm[5], &r101_work->em[5]);
    PSet(r101_work->pEm[6], &r101_work->em[6]);
    PSet(r101_work->pEm[7], &r101_work->em[7]);
    PSet(r101_work->pEm[8], &r101_work->em[8]);
    PSet(r101_work->pEm[9], &r101_work->em[9]);
    if (!ScfFlagChk(pG, SCF_R101_ENTER)) {
        ScfFlagOn(pG, SCF_R101_ENTER);
        SceExec(0x12, (TaskFunc) r101_execOperator2, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    EvtMgr.SetFunc("evt_r101s21_func", (void*) Evt_R101S21_Func);
    EvtMgr.SetFunc("evt_r101s30_func", (void*) Evt_R101S30_Func);
    EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &r101_eff_info);
    if (getRoomEtcDoor(0xB, &door, 1)) {
        ((cEmDoor*) door)->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1E), ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0);
    }
    if (getRoomEtcRack(0xD, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 1000.0f, 0.0f, 2000.0f);
    }
    if (getRoomEtcRack(0xF, &rack, 1)) {
        ((cEmRack*) rack)->setRange(2000.0f, 2000.0f, 0.0f, 2600.0f);
    }
    if (getRoomEtcRack(0x11, &rack, 1)) {
        FSet(rack->pos.x, 6656.0f);
        FSet(rack->pos.y, 902.0f);
        FSet(rack->pos.z, 8925.0f);
        ((cEmRack*) rack)->setRange(2000.0f, 800.0f, 0.0f, 4400.0f);
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        if (getRoomEtcLadder(5, &r101_work->ladder[0], 1)) {
            ((cObjLadder*) r101_work->ladder[0])->setOff();
        }
        if (getRoomEtcLadder(6, &r101_work->ladder[1], 1)) {
            ((cObjLadder*) r101_work->ladder[1])->setOff();
        }
        if (getRoomEtcLadder(7, &r101_work->ladder[2], 1)) {
            ((cObjLadder*) r101_work->ladder[2])->setOff();
        }
    }
    if (pSys->eff_country == 0) {
        SceAtSetEnable(0x20, 0);
    } else {
        Vec pos;
        Vec rot;

        pos.x = -2880.0f;
        pos.y = 50.0f;
        pos.z = 2810.0f;
        rot.x = 0.0f;
        rot.y = -1.5707964f;
        rot.z = 0.0f;
        PSet(r101_work->obj00, SetObj00(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21), &pos, &rot));
        r101_work->obj00->setNoSuspend(1);
        EstSet(0, -1, 0, 0, 1, 0, 0x801, 0, 0, 0);
        pos.y += 1500.0f;
        SndCall(6, 0x58, &pos, 0, 0, 0);
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        if (RsfCheck(G_ROOM_ID, 6) == 0) {
            SceExec(0x12, (TaskFunc) r101_checkFindPlayer, 0, 0, SCE_PRIO_DEF_2, 0);
            SceAtDataSet_exec(0x13, SCE_LEVEL10, 0, (TaskFunc) r101_callGanadoVoice, 0, 1);
            PSet(r101_work->evt00, DC.setData(EvtMgr.NameChange("evd/r101s00.evd")));
            r101_work->evt00->setCommand(CMND_ARAM_LOAD, 0, 0);
            EmReadSearch(0x26, 0, r101_work->evt00->m_size);
            SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r101_Event00, 0, 1);
        } else {
            SceExec(0x12, (TaskFunc) r101_checkEmNum, 0, 0, SCE_PRIO_DEF_2, 0);
            SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r101_DoorDontOpen100, 0, 1);
            SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r101_DoorDontOpen103, 0, 1);
        }
        PSet(r101_work->evt30, DC.setData(EvtMgr.NameChange("evd/r101s30.evd")));
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            PSet(r101_work->evt21, DC.setData(EvtMgr.NameChange("evd/r101s21.evd")));
            r101_work->evt21->setCommand(CMND_ARAM_LOAD, 0, 0);
            if (r101_work->evt21->m_size > r101_work->evt30->m_size) {
                EmReadSearch(0x15, 0, r101_work->evt21->m_size);
            } else {
                EmReadSearch(0x15, 0, r101_work->evt30->m_size);
            }
            if (getRoomEtcWindow(0, &win, 1)) {
                ((cEmWindow*) win)->SetEnableDamage(0);
            }
            if (getRoomEtcWindow(0x13, &win, 1)) {
                ((cEmWindow*) win)->SetEnableDamage(0);
            }
            SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r101_Event20, 0, 2);
            SceExec(0x12, (TaskFunc) r101_DoorCk, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        SceExec(0x12, (TaskFunc) r101_checkTowerBesieged, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceExec(0x12, (TaskFunc) r101_setChickenFlag, 0, 0, SCE_PRIO_DEF_2, 0);
        if (RsfCheck(G_ROOM_ID, 10) == 0) {
            SceExec(0x12, (TaskFunc) r101_execOperator, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        if (ScfFlagChk(pG, SCF_R106_EVENT)) {
            if (RsfCheck(G_ROOM_ID, 9) == 0) {
                RsfSet(G_ROOM_ID, 9);
                r101_emListOn(0x14);
                r101_emListOn(0x15);
                r101_emListOn(0x16);
                r101_emListOn(0x17);
                r101_emListOn(0x18);
                r101_emListOn(0x19);
                r101_emListOn(0x1E);
                r101_emListOn(0x1F);
                r101_emDeadClear(0x14);
                r101_emDeadClear(0x15);
                r101_emDeadClear(0x16);
                r101_emDeadClear(0x17);
                r101_emDeadClear(0x18);
                r101_emDeadClear(0x19);
                r101_emDeadClear(0x1E);
                r101_emDeadClear(0x1F);
                ((EmListData*) &pGS->Em_list[0x48 * 0x20])->be_flag |= 1;
                ((EmListData*) &pGS->Em_list[0x49 * 0x20])->be_flag |= 1;
                ((EmListData*) &pGS->Em_list[0x4A * 0x20])->be_flag |= 1;
            }
            SceExec(0x12, (TaskFunc) r101_checkFindPlayer, 1, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if (!(pG->Key_flg[0] & 0x02000000)) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r101_DoorDontOpen3, 0, 1);
    }
    if (!(pG->Key_flg[0] & 0x20000000)) {
        SceAtDataSet_exec(0x19, SCE_LEVEL10, 0, (TaskFunc) r101_checkDoor102, 0, 1);
        SceExec(0x12, (TaskFunc) r101_checkDoor102KeyUse, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    SceAtSetEnable(0xA3, 1);
    {
        cModel* m = SceAtItemModelPtr(0xA3);

        if (m != 0) {
            m->LightInfo.EnableMask = (m->LightInfo.EnableMask & ~0x20) | 8;
        }
    }
}

// Per-frame room main: nothing.
void R101Main()
{
}

// Ganados throw a flame bottle every 150 frames while the player stays on the tower.
static void r101_checkTowerBesieged()
{
    int cnt = 0;

    while (1) {
        if (SceAtHitCheck(0x18)) {
            cnt++;
            if (cnt > 599 && cnt % 150 == 0) {
                Vec from = {15307.0f, 2800.0f, -1931.0f};
                Vec to = {17394.0f, 10800.0f, -4968.0f};

                r101_setFlameBottle(&from, &to);
            }
        } else {
            cnt = 0;
        }
        if (RsfCheck(G_ROOM_ID, 7)) {
            break;
        }
        SceSleep(1);
    }
}

// A flame bottle (obj 0x01) thrown from `from` to a random point of area 0x15 around `to`.
extern "C" void r101_setFlameBottle(Vec* from, Vec* to)
{
    const f32 spd = 20.0f;
    void* zero = 0;
    Vec dir;
    cObj* obj;

    AreaGetInsidePos(to, &SceAtPtr(0x15)->area);
    CalcParabolaVector(&dir, from, to, 2000.0f);
    Vec zeroVec = {0.0f, 0.0f, 0.0f};
    obj = SetObj01(ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), from, &zeroVec, &dir, spd, 50.0f, 0xD2, 5);
    Obj01SetEst(obj, 1, 0x12, 2, 1, 0x11, 0, 0x14, (int) zero, (int) zero);
    EstSet((int) obj, -1, 0, 0, 1, 0x10, 0, 0, (u32) obj, zero);
}

// Starts the bell event (s30): the fight is over.
static inline void r101_startEvent30()
{
    RsfSet(G_ROOM_ID, 7);
    ScfFlagOff(pG, SCF_R101_IMPRISON);
    ScfFlagOn(pG, SCF_96);
    ScfFlagOn(pG, SCF_9a);
    SceExec(0x12, (TaskFunc) r101_Event30, 0, 0, SCE_PRIO_DEF_2, 0);
    pG->Room_flg[0] |= 0x10000000;
}

// Counts the killed enemies: the two reset waves, then the bell after 15 / 18 kills or a time limit.
static void r101_checkEmNum()
{
    int cnt = 0;
    int alive;

    SceSleep(10);
    r101_work->emNum = SceCountEmAlive(0x10, 0x20);
    for (;;) {
        SceSleep(1);
        if (pG->Room_flg[0] & 0x10000000) {
            continue;
        }
        alive = SceCountEmAlive(0x10, 0x20);
        if (RsfCheck(G_ROOM_ID, 3) == 0 && r101_work->emNum - alive > 4) {
            RsfSet(G_ROOM_ID, 3);
            SceExec(0x12, (TaskFunc) r101_checkEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
            continue;
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0 && r101_work->emNum - alive > 9) {
            RsfSet(G_ROOM_ID, 2);
            SceExec(0x12, (TaskFunc) r101_checkEmReset2, 0, 0, SCE_PRIO_DEF_2, 0);
            continue;
        }
        if (RsfCheck(G_ROOM_ID, 7)) {
            return;
        }
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            if (r101_work->emNum - alive > 14 || cnt > 11700) {
                r101_startEvent30();
                return;
            }
        } else {
            if (r101_work->emNum - alive > 17 || cnt > 13500) {
                r101_startEvent30();
                return;
            }
        }
        if (DebugTrg(0) == 1) {
            r101_startEvent30();
            return;
        }
        if (StaFlagChk(pG, STA_EVENT)) {
            continue;
        }
        cnt++;
        if (RsfCheck(G_ROOM_ID, 8) == 0) {
            SceDebugDisp("T[%d]", 11700 - cnt);
            SceDebugDisp("E[%d]", 15 - (r101_work->emNum - alive));
        } else {
            SceDebugDisp("T[%d]", 13500 - cnt);
            SceDebugDisp("E[%d]", 18 - (r101_work->emNum - alive));
        }
    }
}

// The chapter title over the bell event.
static void r101_Event30_TitleCall()
{
    cDataUnit* tex;
    cDataUnit* id;
    EventMgr* m;

    if (pSys->language == 0) {
        tex = DC.setData("etc/jpn/id101.eff");
    } else {
        tex = DC.setData("etc/eng/id101.eff");
    }
    tex->setCommand(CMND_MRAM_LOAD, 0, 0);
    if (pSys->language == 0) {
        id = DC.setData("etc/jpn/event001.uwf");
    } else {
        id = DC.setData("etc/eng/event001.uwf");
    }
    id->setCommand(CMND_MRAM_LOAD, 0, 0);
    while (tex->isUseOk() != 1 || id->isUseOk() != 1) {
        if (pG->Room_flg[0] & 0x20000000) {
            goto end;
        }
        SceSleep(1);
    }
    // `m` is set INSIDE the loop (loop.c hoists it): with it before the loop the previous poll
    // loop's exit no longer falls straight into this loop's label, the two `addi` form a preheader
    // block and gcse PREs `&evt` into it (`addi r28,r1,8` + `mr r5,r28`, one more callee-saved reg).
    do {
        int frame = 0;
        void* evt;

        m = &EvtMgr;
        if (m->GetEvt(&m->NowExeEvtKey, &evt)) {
            frame = ((Event*) evt)->NowTotalFrame;
        }
        if (frame > 1099) {
            break;
        }
        if (pG->Room_flg[0] & 0x20000000) {
            goto end;
        }
        SceSleep(1);
    } while (1);
    IdSys.dispSw(0x21, 0);
    IdTexDataLoad(tex->m_addr, TEX_OWNER_ID_EVENT);
    IdSys.set(id->m_addr, 0xFF, 0x2C, 0x13, 6, 0);
    while (1) {
        if (pG->Room_flg[0] & 0x20000000) {
            goto end;
        }
        SceSleep(1);
    }
end:
    tex->setCommand(CMND_DEL_DATA, 0, 0);
    id->setCommand(CMND_DEL_DATA, 0, 0);
    IdSys.kill(0xFF, 0x2C);
    IdTexRelease(TEX_OWNER_ID_EVENT);
    IdSys.dispSw(0x21, 1);
}

// The bell event: the Ganados leave, the s30 event plays out of the enemy module block.
static void r101_Event30()
{
    int fail = 0;
    u32 unused[2];   // an 8-byte aggregate slot precedes `win`/`ladder` in the original's frame (0x30)
    ReadModule* m;
    cEm* win;
    cEm* ladder;

    RsfSet(G_ROOM_ID, 7);
    ScfFlagOff(pG, SCF_R101_IMPRISON);
    ScfFlagOn(pG, SCF_96);
    ScfFlagOn(pG, SCF_9a);
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        SceAtSetEnable(8, 0);
        r101_work->evt21->setCommand(CMND_DEL_DATA, 0, 0);
    }
    r101_work->evt30->setCommand(CMND_ARAM_LOAD, 0, 0);
    SceSleep(60);
    while (r101_work->evt30->isLoadOk() == 0) {
        if (r101_work->evt30->m_err != 0) {
            fail = 1;
            break;
        }
        SceSleep(1);
    }
    while (SceCheckEventStart() == 0) {
        SceSleep(1);
    }
    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    SndRoomStrStop(3);
    SceDestroyEm(0x10, 0x20);
    SceSleep(2);
    m = SearchEmModule(0x15);
    if (fail != 1) {
        if (r101_work->evt30->m_size > m->size) {
            // COMPILER-DIFF: frame layout -- codeless use that keeps the 8-byte slot allocated
            // (an unreferenced aggregate gets no slot; the original's use is not in the bytes).
            asm("" : "=m"(unused));
            pLog->err(0, 0, "r101_Event30 exec error");
        } else {
            EspDataRelease(0x10, 0, 1);
            InitModule(m);
            r101_work->evt30->setCommand(CMND_MRAM_LOAD, 0, 1);
            SceExec(0x12, (TaskFunc) r101_Event30_TitleCall, 0, 2, SCE_PRIO_DEF_2, 0);
            EvtMgr.SetEvt(r101_work->evt30->m_addr, 0);
            while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0)) {
                SceSleep(1);
            }
            SysFlagOff(pG, SYS_SCREEN_STOP);
        }
    }
    BitOn(pG->Room_flg[0], 0x20000000);
    r101_work->evt30->setCommand(CMND_DEL_DATA, 0, 0);
    SceEventEnd(0);
    SceAtDataReset(0);
    SceAtDataReset(2);
    if (getRoomEtcWindow(0, &win, 1)) {
        ((cEmWindow*) win)->SetEnableDamage(1);
    }
    if (getRoomEtcWindow(0x13, &win, 1)) {
        ((cEmWindow*) win)->SetEnableDamage(1);
    }
    EmListSetAlive(0x14, 0);
    EmListSetAlive(0x15, 0);
    EmListSetAlive(0x16, 0);
    EmListSetAlive(0x17, 0);
    EmListSetAlive(0x18, 0);
    EmListSetAlive(0x19, 0);
    EmListSetAlive(0x1A, 0);
    EmListSetAlive(0x1B, 0);
    EmListSetAlive(0x1C, 0);
    EmListSetAlive(0x1E, 0);
    EmListSetAlive(0x1F, 0);
    EmListSetAlive(0x22, 0);
    EmListSetAlive(0x23, 0);
    EmListSetAlive(0x24, 0);
    EmListSetAlive(0x28, 0);
    EmListSetAlive(0x29, 0);
    EmListSetAlive(0x2A, 0);
    EmListSetAlive(0x2B, 0);
    EmListSetAlive(0x2C, 0);
    EmListSetAlive(0x2E, 0);
    EmListSetAlive(0x2F, 0);
    EmListSetAlive(0x30, 0);
    EmListSetAlive(0x31, 0);
    EmListSetAlive(0x32, 0);
    EmListSetAlive(0x36, 0);
    EmListSetAlive(0x37, 0);
    EmListSetAlive(0x38, 0);
    EmListSetAlive(0x39, 0);
    EmListSetAlive(0x3A, 0);
    EmListSetAlive(0x3C, 0);
    EmListSetAlive(0x3D, 0);
    EmListSetAlive(0x3E, 0);
    EmListSetAlive(0x3F, 0);
    EmListSetAlive(0x40, 0);
    EmListSetAlive(0x41, 0);
    EmListSetAlive(0x42, 0);
    EmListSetAlive(0x43, 0);
    EmListSetAlive(0x44, 0);
    EmListSetAlive(0x45, 0);
    EmListSetAlive(0x46, 0);
    if (RsfCheck(G_ROOM_ID, 8)) {
        if (getRoomEtcLadder(5, &ladder, 1) && ((cObjLadder*) ladder)->getStatus() == 4) {
            ((cObjLadder*) ladder)->setStand();
        }
        if (getRoomEtcLadder(6, &ladder, 1) && ((cObjLadder*) ladder)->getStatus() == 4) {
            ((cObjLadder*) ladder)->setStand();
        }
        if (getRoomEtcLadder(7, &ladder, 1) && ((cObjLadder*) ladder)->getStatus() == 4) {
            ((cObjLadder*) ladder)->setStand();
        }
    }
    pG->Room_flg[0] &= ~0x10000000;
    r101_execOperator();
}

// First visit: the typewriter terminal, the fade and the save.
static void r101_execOperator2()
{
    OpeSetOpenTerm(0xC, 0.0f, 0.0f, 0.0f, 0.0f);
    if (pG->game_cnt == 0) {
        FadeSetW(1, 0, 0, 0);
        SceAtExecute(0xAC);
    }
    SceSleep(1);
    GameSaveSave(&GameSave, pSaveData, -1);
}

// Once (Room_flg bit 10): the typewriter terminal 2 with the overwrite type.
static void r101_execOperator()
{
    RsfSet(G_ROOM_ID, 10);
    OpeOwTypeSet(1);
    OpeSetOpenTerm(2, 0.0f, 0.0f, 0.0f, 0.0f);
}

// The house door stays open-locked during the fight, then closes and locks.
static void r101_DoorCk()
{
    cEm* door;

    if (getRoomEtcDoor(2, &door, 1)) {
        door->setNoSuspend(0);
        while (1) {
            if (RsfCheck(G_ROOM_ID, 8)) {
                break;
            }
            if (RsfCheck(G_ROOM_ID, 7)) {
                break;
            }
            ((cEmDoor*) door)->setOpenLock(0);
            SceSleep(1);
        }
        SceSleep(1);
        cEmDoorSetCloseLock(door);
        SceSleep(30);
        ((cEmDoor*) door)->setNormal();
    }
}

// The house event (s21): the player is thrown out of the house, the ladders come up.
static void r101_Event20()
{
    Vec pos;
    Vec ang;
    int diff;
    int fail = 0;
    ReadModule* m;
    cEm* rack;
    cEm* r;
    cEm* win;

    if (RsfCheck(G_ROOM_ID, 7)) {
        SceExit();
    }
    RsfSet(G_ROOM_ID, 8);
    diff = r101_work->emNum - SceCountEmAlive(0x10, 0x20);
    SceAtSetEnable(8, 0);
    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    SndRoomStrStop(3);
    if (r101_work->evt21->waitLoadOk() == 0) {
        fail = 1;
    }
    if (getRoomEtcRack(0xF, &rack, 1)) {
        rack->setNoSuspend(0);
        FSet(rack->pos.x, 7201.0f);
        FSet(rack->pos.z, -7280.0f);
        r = rack;
        {
            Vec* pp = &r->pos;
            Vec* pa = &r->ang;
            r->setPos(pp);
            r->setAng(pa);
        }
    }
    SceDestroyEm(0x10, 0x20);
    SceSleep(2);
    m = SearchEmModule(0x15);
    if (fail != 1) {
        if (r101_work->evt21->m_size > m->size) {
            pLog->err(0, 0, "r101_Event20 exec error");
        } else {
            MemorySwap(m->pArc, (u32) r101_work->evt21->m_addr, r101_work->evt21->m_size);
            EvtMgr.SetEvt(m->pArc, 0);
            SceSleep(3);
            for (;;) {
                EventMgr* em = &EvtMgr;
                u32* key = &em->NowExeEvtKey;
                void* evt;

                if (em->IsAliveEvt(key, 0, 0) == 0) {
                    break;
                }
                if (em->GetEvt(key, &evt) && ((Event*) evt)->NowCut == 0xA && ((Event*) evt)->NowFrame == 0x20) {
                    if (getRoomEtcWindow(0, &win, 1)) {
                        ((cEmWindow*) win)->SetBreakModel();
                    }
                }
                SceSleep(1);
            }
            SysFlagOff(pG, SYS_SCREEN_STOP);
            MemorySwap(m->pArc, (u32) r101_work->evt21->m_addr, r101_work->evt21->m_size);
        }
    }
    r101_work->evt21->setCommand(CMND_DEL_DATA, 0, 0);
    setEm(0x3C, -1, 1, 1, 1);
    setEm(0x3D, -1, 1, 1, 1);
    setEm(0x3E, -1, 1, 1, 1);
    setEm(0x3F, -1, 1, 1, 1);
    setEm(0x40, -1, 1, 1, 1);
    setEm(0x41, -1, 1, 1, 1);
    setEm(0x42, -1, 1, 1, 1);
    setEm(0x43, -1, 1, 1, 1);
    setEm(0x44, -1, 1, 1, 1);
    setEm(0x45, -1, 1, 1, 1);
    setEm(0x46, -1, 1, 1, 1);
    r101_work->emNum = diff + SceCountEmAlive(0x10, 0x20);
    if (getRoomEtcWindow(0, &win, 1)) {
        ((cEmWindow*) win)->SetBreakModel();
    }
    if (getRoomEtcWindow(0x13, &win, 1)) {
        ((cEmWindow*) win)->SetEnableDamage(1);
    }
    if (r101_work->ladder[0] != 0) {
        ((cObjLadder*) r101_work->ladder[0])->setOn();
    }
    if (r101_work->ladder[1] != 0) {
        ((cObjLadder*) r101_work->ladder[1])->setOn();
    }
    if (r101_work->ladder[2] != 0) {
        ((cObjLadder*) r101_work->ladder[2])->setOn();
    }
    {
        static const Vec r101_plPos20 = {2666.0f, 0.0f, -8495.0f};
        f32 ry = -1.21f;
        cPlayer* pl;

        pos = r101_plPos20;
        // `&ang` before the first pPL read: gcse's PRE insertions at the end of the SearchEmModule
        // block are emitted in hash-table (first-occurrence) order, and the LAST one shares sched1's
        // cycle with `li r3,0x15`; it must be high(pPL), not `&ang` (sched2 then keeps addi before li).
        Vec* pa = &ang;
        pl = pPLS;
        pl->setPos(&pos);
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(&ang);
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        r101_FindPlayer();
    }
    SndRoomStrStart(1, 3, 1);
    pPL->cCoord::matUpdate();
    CamCtrl.m_QuasiFPS.setPlayerLocation(pPL->mat, pPL->pFloor_norm);
    CamCtrl.roomInit();
    SceEventEnd(0);
}

// The second reset wave, by the side the player is on.
static void r101_checkEmReset2()
{
    RsfSet(G_ROOM_ID, 2);
    if (SceAtHitCheck(0x1A) == 1) {
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            setEm(0x28, -1, 1, 1, 1);
            setEm(0x29, -1, 1, 1, 1);
            setEm(0x2A, -1, 1, 1, 1);
            setEm(0x2B, -1, 1, 1, 1);
            setEm(0x2C, -1, 1, 1, 1);
        } else {
            setEm(0x2E, -1, 1, 1, 1);
            setEm(0x2F, -1, 1, 1, 1);
            setEm(0x30, -1, 1, 1, 1);
            setEm(0x31, -1, 1, 1, 1);
            setEm(0x32, -1, 1, 1, 1);
        }
    } else {
        setEm(0x36, -1, 1, 1, 1);
        setEm(0x37, -1, 1, 1, 1);
        setEm(0x38, -1, 1, 1, 1);
        setEm(0x39, -1, 1, 1, 1);
        setEm(0x3A, -1, 1, 1, 1);
    }
    r101_work->emNum += 5;
}

// End of a reset-wave cutscene: camera back, the five new Ganados of `side` may suspend again, the
// kill target grows by 5, SceEventEnd.
static void r101_checkEmReset_end(int side)
{
    if (side == 0) {
        CamCtrl.Comeback(0);
        r101_work->pEm[0]->setNoSuspend(0);
        r101_work->pEm[1]->setNoSuspend(0);
        r101_work->pEm[2]->setNoSuspend(0);
        r101_work->pEm[3]->setNoSuspend(0);
        r101_work->pEm[4]->setNoSuspend(0);
        r101_work->emNum += 5;
    } else {
        CamCtrl.Comeback(0);
        r101_work->pEm[5]->setNoSuspend(0);
        r101_work->pEm[6]->setNoSuspend(0);
        r101_work->pEm[7]->setNoSuspend(0);
        r101_work->pEm[8]->setNoSuspend(0);
        r101_work->pEm[9]->setNoSuspend(0);
        r101_work->emNum += 5;
    }
    SceEventEnd(0);
}

// The first reset wave: five more Ganados come in from the side the player is on, with a camera cut.
static void r101_checkEmReset()
{
    int side;
    cPlayer* pl;

    RsfSet(G_ROOM_ID, 3);
    if (pPL->pos.x > -3300.0f) {
        RsfSet(G_ROOM_ID, 0);
    } else {
        RsfSet(G_ROOM_ID, 1);
    }
    SceSleep(90);
    pl = pPL;
    while (pl->checkEvent() != 1) {
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 7)) {
        SceExit();
    }
    SceEventStart(1);
    if (pPL->pos.x > -3300.0f) {
        SndStrReq(1, 0x3E, 0x80000003, 0, 0, 0.0f);
        side = 0;
        RsfSet(G_ROOM_ID, 0);
        r101_work->pEm[0]->setEm(0x28, -1, 1, 1, 1);
        r101_work->pEm[1]->setEm(0x29, -1, 1, 1, 1);
        r101_work->pEm[2]->setEm(0x2A, -1, 1, 1, 1);
        r101_work->pEm[3]->setEm(0x2B, -1, 1, 1, 1);
        r101_work->pEm[4]->setEm(0x2C, -1, 1, 1, 1);
        r101_work->pEm[0]->setNoSuspend(1);
        r101_work->pEm[1]->setNoSuspend(1);
        r101_work->pEm[2]->setNoSuspend(1);
        r101_work->pEm[3]->setNoSuspend(1);
        r101_work->pEm[4]->setNoSuspend(1);
        CamCtrl.CutCall(5);
    } else {
        SndStrReq(1, 0x3B, 0x80000003, 0, 0, 0.0f);
        side = 1;
        RsfSet(G_ROOM_ID, 1);
        r101_work->pEm[5]->setEm(0x2E, -1, 1, 1, 1);
        r101_work->pEm[6]->setEm(0x2F, -1, 1, 1, 1);
        r101_work->pEm[7]->setEm(0x30, -1, 1, 1, 1);
        r101_work->pEm[8]->setEm(0x31, -1, 1, 1, 1);
        r101_work->pEm[9]->setEm(0x32, -1, 1, 1, 1);
        r101_work->pEm[5]->setNoSuspend(1);
        r101_work->pEm[6]->setNoSuspend(1);
        r101_work->pEm[7]->setNoSuspend(1);
        r101_work->pEm[8]->setNoSuspend(1);
        r101_work->pEm[9]->setNoSuspend(1);
        CamCtrl.CutCall(4);
    }
    SceSetEventCancel(1, (TaskFunc) r101_checkEmReset_end, side, -1, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r101_checkEmReset_end(side);
}

// Area 0: the door back to r100 is barred — knock SE and message 0.
static void r101_DoorDontOpen100()
{
    SndCall(6, 0x26, 0, 0, 0, 0);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// Area 2: the door to r103 is barred — knock SE and message 0.
static void r101_DoorDontOpen103()
{
    SndCall(6, 0x29, 0, 0, 0, 0);
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// The player holds the key: the door area unlocks.
static void r101_checkDoor102KeyUse()
{
    while (ItemMgr.check(0x3B) != 1) {
        SceSleep(1);
    }
    SndCall(6, 0x25, 0, 0, 0, 0);
    SceMesSet(0xB, 1, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    pG->Key_flg[0] |= 0x20000000;
    SceAtDataReset(0x19);
}

// The locked door: the up-cut message, or the key use through the sub screen.
static void r101_checkDoor102()
{
    SceUpCut(1, 0xE, 0x28, UP_CUT_ATTR_CUT_FIX);
    if (ItemMgr.num(0x3B) == 0) {
        CamCtrl.Comeback(0);
    } else {
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
    }
}

// Area 1: the locked door — knock SE and message 2.
static void r101_DoorDontOpen3()
{
    SndCall(6, 0x27, 0, 0, 0, 0);
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
}

// Once a Ganado has found the player: the fight (mode 0) or the battle stream (mode 1).
static void r101_checkFindPlayer(int mode)
{
    while (SceCkFindPL(0) != 1) {
        SceSleep(1);
    }
    switch (mode) {
    case 0:
        if (RsfCheck(G_ROOM_ID, 6) == 0) {
            r101_FindPlayer();
        }
        break;
    case 1:
        SceExec(0x12, (TaskFunc) r101_FindPlayer2, 0, 0, SCE_PRIO_DEF_2, 0);
        break;
    }
}

// Battle stream 3 (mode 1, after the bell) while any Ganado (ids 0x10..0x20) is alive.
static void r101_FindPlayer2()
{
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// The fight starts: the doors lock, the three Ganados of the square, the battle stream.
extern "C" void r101_FindPlayer()
{
    RsfSet(G_ROOM_ID, 6);
    ScfFlagOn(pG, SCF_R101_IMPRISON);
    ScfFlagOff(pG, SCF_96);
    ScfFlagOff(pG, SCF_9a);
    SceAtSetEnable(7, 0);
    r101_work->evt00->setCommand(CMND_DEL_DATA, 0, 0);
    setEm(0x22, -1, 1, 1, 1);
    setEm(0x23, -1, 1, 1, 1);
    setEm(0x24, -1, 1, 1, 1);
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r101_DoorDontOpen100, 0, 1);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r101_DoorDontOpen103, 0, 1);
    SndRoomStrStart(1, 3, 1);
    SceExec(0x12, (TaskFunc) r101_checkEmNum, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r101_setChickenFlag, 0, 0, SCE_PRIO_DEF_2, 0);
}

// The chickens (enemy 0x28) stop laying.
static void r101_setChickenFlag()
{
    u32 i;

    SceSleep(1);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (em->id == 0x28 && em->isAlive()) {
            em->flag &= ~0x80000000;
        }
    }
}

// Suspends / resumes every Ganado of the room around the s00 event.
static inline void r101_setEmSuspend(int on)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (em->id >= 0x10 && em->id <= 0x20 && em->isAlive()) {
            em->setNoSuspend(on);
        }
    }
}

// The binocular view of the village (s00 event first, then the binocular until a Ganado finds the player).
static void r101_Event00()
{
    Vec pos;
    Vec ang;
    Vec at;

    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        RsfSet(G_ROOM_ID, 5);
        SceEventStart(0);
        r101_setEmSuspend(1);
        if (r101_work->evt00->waitLoadOk() == 1) {
            ReadModule* m;

            SysFlagOn(pG, SYS_SCREEN_STOP);
            SceSleep(2);
            m = SearchEmModule(0x26);
            MemorySwap(m->pArc, (u32) r101_work->evt00->m_addr, r101_work->evt00->m_size);
            EvtMgr.SetEvt(m->pArc, 0);
            while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0)) {
                SceSleep(1);
            }
            MemorySwap(m->pArc, (u32) r101_work->evt00->m_addr, r101_work->evt00->m_size);
        }
        r101_work->evt00->setCommand(CMND_DEL_DATA, 0, 0);
        r101_setEmSuspend(0);
        SceEventEnd(0);
    }
    {
        static const Vec r101_plPos00 = {-38027.0f, 141.0f, 11114.0f};
        f32 ry = 1.72f;
        cPlayer* pl;

        pos = r101_plPos00;
        pl = pPLS;
        pl->setPos(&pos);
        Vec* pa = &ang;
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(&ang);
    }
    pPL->dmg.set(0, 0x80);
    ang = pPL->pos;
    if (pSysS->eff_country == 0) {   // struct view: the pSys load stays below the `ang` copy stores
        at.x = 3492.0f;
        at.y = 1100.0f;
        at.z = 3695.0f;
    } else {
        at.x = -3573.0f;
        at.y = 2400.0f;
        at.z = 2113.0f;
    }
    ang.y += 1750.0f;
    SpfFlagOn(pG, SPF_ACTBTN);
    SndCall(1, 2, 0, 0, 0, 0);
    CamCtrl.HoldBinocular(ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), &ang, &at);
    CamCtrl.SetBinocularRange(-0.05992f, 0.2645f, -0.2532f, 0.14943f);
    SpfFlagOn(pG, SPF_PL);
    for (;;) {
        if (Key.trg & 0x40000000) {
            break;
        }
        if (SceCkFindPL(0) == 1) {
            break;
        }
        SceSleep(1);
    }
    pPL->dmg.clear();
    CamCtrl.LowerBinocular();
    SpfFlagOff(pG, SPF_PL);
    SpfFlagOff(pG, SPF_ACTBTN);
    pPL->cCoord::matUpdate();
    CamCtrl.m_QuasiFPS.setPlayerLocation(pPL->mat, pPL->pFloor_norm);
    CamCtrl.roomInit();
}

// Random Ganado voices from the square until the fight starts.
static void r101_callGanadoVoice()
{
    u8 tbl[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    Vec pos = {-5356.0f, 1000.0f, 3250.0f};
    int last = -1;

    while (1) {
        int v;

        if (RsfCheck(G_ROOM_ID, 6)) {
            break;
        }
        do {
            u32 r = Rnd();
            u8 i = r % 10;

            v = tbl[i];
        } while (last == v);
        last = v;
        SndCall(6, last, &pos, 0, 0, 0);
        SceSleep(Rnd() * 45 / 255 + 75);
    }
}

// Event r101s21 callback: fetch the etc model et0800 on the first frame (registers it with the event).
extern "C" void Evt_R101S21_Func(Event* e)
{
    void* mod;

    if (e->funcMode == 1 && e->NowCut == 0 && e->NowFrame == 0) {
        e->GetMod(&mod, "et0800", 0, 0);
    }
}

// Event r101s30 callback (the church bell rings, the Ganados leave): hides the ladders during the event,
// hands scroll object 0x39 (scr0000) to the event on cut 0 and puts it back at the end.
extern "C" void Evt_R101S30_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;
    SmdWork* w;

    switch (e->funcMode) {
    case 0:
        LadderEventTrans(0);
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            if ((obj = SmdGetObjPtr(0x39)) != 0) {
                e->SetMod("scr0000", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
        }
        break;
    case 2:
        w = SmdGetWorkPtr(0x39);
        if ((obj = SmdGetObjPtr(0x39)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        LadderEventTrans(1);
        break;
    }
}
