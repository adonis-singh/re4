#include "types.h"
class cObjWep;
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
#include "game.h"
#include "datactrl.h"
#include "read.h"
#include "dvd.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "emdoor.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "rnd.h"
#include "snd.h"
#include "quake.h"
#include "esp.h"
#include "est.h"
#include "math_sub.h"
#include "pl_npc.h"

// game/quake.cpp keeps QuakeKill static; the REL imports it by name.
void QuakeKill(u8 id);

// Room 2-12 (D:/Bio4/Prog/r212.cpp): the four floor switches and the falling roof trap, the
// three shutter doors, the drill Ganados and Ashley's escape.

// One shutter door, driven by a member-function table.
class cR212Door {
public:
    u8 mode;        // 0x00  routine (0 wait, 1 open, 2 close)
    u8 step;        // 0x01
    u8 x2;
    u8 x3;
    cObj* obj;      // 0x04
    Vec pos0;       // 0x08  closed position
    f32 openH;      // 0x14  height when open
    f32 spd;        // 0x18  fall speed
    u32 se;         // 0x1C  RoomSeCall handle
    int valid;      // 0x20
    int flagNo;     // 0x24  door flag index in pG->Scenario_flg
    u32 id;         // 0x28  scroll object id
    int status;     // 0x2C  0 closed, 1 open, 4 moving
    int timer;      // 0x30

    void init(u32 id);
    void move();
    void wait();
    void open();
    void close();
    void setOpen();
    void setClose();
    void setOpened();
    void setClosed();
    int getStatus();
};

struct R212Work {
    cEm* rack[2];        // 0x000
    cSat* sat[4];        // 0x008  switch hit shapes
    f32 y0[4];           // 0x018  switch rest heights
    u16 hitNow;          // 0x028  switches pressed this frame (bits 0-3 by anything, 8-11 rack 0, 12-15 rack 1)
    u16 hitOld;          // 0x02A
    u16 trg;             // 0x02C
    u16 rel;             // 0x02E
    cEmWrap em[2];       // 0x030  the drill Ganados
    cDataUnit* evd;      // 0x048
    cEmHit* hit[4];      // 0x04C  roof trap hit boxes
    int mesNo;           // 0x05C
    cSat* sat2;          // 0x060
    cSat* eat;           // 0x064
    cSat* eat2;          // 0x068
    cSat* eat0;          // 0x06C
    cR212Door door[3];   // 0x070
    u32 se;              // 0x10C
};


static R212Work* r212_work;

static void (cR212Door::*r212_doorTbl[3])() = {
    &cR212Door::wait,
    &cR212Door::open,
    &cR212Door::close,
};

// The four roof-trap flags (event flags 3..6).
#define R212_TRAP_FLAGS_ALL(f) \
    (FlagChkSignW(f, 3) && FlagChkSignW(f, 4) && FlagChkSignW(f, 5) && FlagChkSignW(f, 6))
#define R212_TRAP_FLAGS_NONE(f) \
    (FlagChkSignW(f, 3) == 0 && FlagChkSignW(f, 4) == 0 && FlagChkSignW(f, 5) == 0 && FlagChkSignW(f, 6) == 0)

void r212_TrapInit();
void r212_SetSwitchInfo();
static void r212_Puzzle();
static void r212_PuzzleEndProc();
static void r212_EventTrap();
void Evt_R212S00_Func(Event* e);
static void r212_RoofMove();
static void r212_RoofTrapWatcher();
static void r212_MesRoofDoor();
static void r212_AdhleyToPointWait();
static void r212_AshleyPointTo(cEm* sub);
static void r212_AshleyPointToCheck();
static void r212_DrillAppearCheck();
static void r212_DrillAppearCheckEndProc();
static void r212_DrillEndCheck();
static void r212_DrillMove();
static void r212_AshleyDrillAction(cEm* sub);
static void r212_DoorMessage();
static void r212_DoorLock();
static void r212_TreasureBoxOpen(int id);
static void r212_TreasureBoxOpened(int id);

// Room init (Ashley's section: the switch room): the three shutter doors (0x1B first, 0x16 roof-trap
// room, 0x22 exit; the last two start open), the trap setup, the entrance lock task, two treasure item events.
void R212Init()
{
#line 58 "D:/Bio4/Prog/r212.cpp"
    r212_work = (R212Work*) MEM_CALLOC(sizeof(R212Work), 1, 0xd);
    r212_work->door[0].init(0x1B);
    r212_work->door[1].init(0x16);
    r212_work->door[2].init(0x22);
    r212_work->door[1].setOpened();
    r212_work->door[2].setOpened();
    r212_TrapInit();
    SceExec(0x12, (TaskFunc) r212_DoorLock, 0, 0, SCE_PRIO_DEF_2, 0);
    SceSetItemEvent(0xB, 0x81, 3, 0xA, r212_TreasureBoxOpen, r212_TreasureBoxOpened, 6, 0);
    SceSetItemEvent(0xC, 0x80, 4, 0xB, r212_TreasureBoxOpen, r212_TreasureBoxOpened, 0x3E, 0);
}

// The trap room: its attribute piece; until the trap ran (Room_flg bit 0) area 1 = the roof trap event
// (r212s00 pre-loaded) and the roof hit-box watcher; the switch puzzle / racks per bit 1; the drill
// Ganados and the exit per bits 2/3.
void r212_TrapInit()
{
    cObj* o = SmdGetObjPtr(0x1A);

    o->be_flag &= ~2;
    cSat* e0 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &o->pos, &o->ang, 2);
    r212_work->eat0 = e0;
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r212_EventTrap, 0, 1);
        r212_work->evd = DC.setData(EvtMgr.NameChange("evd/r212s00.evd"));
        r212_work->evd->setCommand(CMND_ARAM_LOAD, 0, 0);
        EvtMgr.SetFunc("evt_r212s00_func", (void*) Evt_R212S00_Func);
        SceExec(0x12, (TaskFunc) r212_RoofTrapWatcher, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    SceAtSetEnable(9, 0);
    if (RsfCheck(G_ROOM_ID, 1)) {
        SceAtSetEnable(6, 0);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            r212_work->door[0].setOpened();
        }
    } else {
        SceExec(0x12, (TaskFunc) r212_Puzzle, 0, 0, SCE_PRIO_DEF_2, 0);
        if (!StaFlagChk(pG, STA_SUB_ASHLEY)) {
            StaFlagOn(pG, STA_SUB_ASHLEY);
            SubCharInit(1, &pPL->pos, pPL->ang.y);
            SubCharCtrl(SCC_CHASE, 0);
        }
    }
    if (getRoomEtcRack(5, &r212_work->rack[0], 1)) {
        ((cEmRack*) r212_work->rack[0])->setRange(5100.0f, 3000.0f, 7156.0f, 3000.0f);
    }
    if (getRoomEtcRack(6, &r212_work->rack[1], 1)) {
        ((cEmRack*) r212_work->rack[1])->setRange(5500.0f, 3000.0f, 6756.0f, 3000.0f);
    }
    Vec zero = {0.0f, 0.0f, 0.0f};

    r212_work->sat[0] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 8);
    r212_work->sat[1] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 7);
    r212_work->sat[2] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 6);
    r212_work->sat[3] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 9);
    r212_work->y0[0] = SmdGetObjPtr(0xD)->pos.y;
    r212_work->y0[1] = SmdGetObjPtr(0xF)->pos.y;
    r212_work->y0[2] = SmdGetObjPtr(0x12)->pos.y;
    r212_work->y0[3] = SmdGetObjPtr(0x10)->pos.y;
    r212_work->door[2].setOpened();
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        cObj* drill = SmdGetObjPtr(0x2A);
        cEmGanado* g;

        if (r212_work->em[0].setEm(0xD7, 4, 1, 1, 0)) {
            g = (cEmGanado*) r212_work->em[0].getPtr();
            g->setDrill(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), drill, (void*) 0);
        }
        if (r212_work->em[1].setEm(0xD8, 4, 1, 1, 0)) {
            g = (cEmGanado*) r212_work->em[1].getPtr();
            g->setDrill(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), drill, (void*) 1);
        }
        SceExec(0x12, (TaskFunc) r212_DrillAppearCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        u32 id[3] = {0x2A, 0x2B, 0x2C};
        f32 y[3] = {13363.0f, 15384.0f, 16017.0f};
        u32 i;

        for (i = 0; i < 3; i++) {
            cObj* d = SmdGetObjPtr(id[i]);

            d->pos.z = y[i];
            d->matUpdate();
            d->be_flag &= ~0x20;
        }
        SmdGetObjPtr(0x32)->be_flag &= ~2;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 0, ESP_CORE_KIND_NONE, 0, 0);
    }
    {
        cObj* d = SmdGetObjPtr(0x2C);
        Vec zero2 = {0.0f, 0.0f, 0.0f};

        r212_work->sat2 = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &d->pos, &d->ang, 1);
        r212_work->eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &d->pos, &d->ang, 3);
        r212_work->eat2 = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero2, &zero2, 1);
    }
}

// Per frame: read the floor switches, step the three doors, keep the attribute piece on object 0xC.
void R212Main()
{
    cObj* o = SmdGetObjPtr(0xC);

    r212_SetSwitchInfo();
    r212_work->door[0].move();
    r212_work->door[1].move();
    r212_work->door[2].move();
    r212_work->eat0->setCoord(&o->pos, &o->ang);
}

// Reads the four floor switches (player, Ashley, the two racks) and moves them.
void r212_SetSwitchInfo()
{
    u32 id[4] = {0xD, 0xF, 0x12, 0x10};
    u32 i;

    r212_work->hitNow = 0;
    for (i = 0; i < 4; i++) {
        if (SceAtCheckHitModel(i + 2, pPL)) {
            r212_work->hitNow |= 1 << i;
        }
        if (pSUB && SceAtCheckHitModel(i + 2, pSUB)) {
            r212_work->hitNow |= 1 << i;
        }
        if (r212_work->rack[0] && SceAtCheckHitModel(i + 2, r212_work->rack[0])) {
            r212_work->hitNow |= 1 << i;
            r212_work->hitNow |= 1 << (i + 8);
        }
        if (r212_work->rack[1] && SceAtCheckHitModel(i + 2, r212_work->rack[1])) {
            r212_work->hitNow |= 1 << i;
            r212_work->hitNow |= 1 << (i + 12);
        }
    }
    r212_work->trg = (r212_work->hitOld ^ r212_work->hitNow) & r212_work->hitNow;
    r212_work->rel = (r212_work->hitOld ^ r212_work->hitNow) & r212_work->hitOld;
    r212_work->hitOld = r212_work->hitNow;
    for (i = 0; i < 4; i++) {
        cObj* o = SmdGetObjPtr(id[i]);

        if ((r212_work->trg >> i) & 1) {
            o->pos.y = r212_work->y0[i] - 60.0f;
            o->matUpdate();
            RoomSeCall(8, &o->pos, 0, 0, o);
        }
        if ((r212_work->rel >> i) & 1) {
            o->pos.y = r212_work->y0[i];
            o->matUpdate();
            RoomSeCall(9, &o->pos, 0, 0, o);
        }
        if ((r212_work->trg >> (i + 8)) & 1) {
            r212_work->rack[0]->pos.y -= 60.0f;
        }
        if ((r212_work->trg >> (i + 12)) & 1) {
            r212_work->rack[1]->pos.y -= 60.0f;
        }
        if ((r212_work->rel >> (i + 8)) & 1) {
            r212_work->rack[0]->pos.y += 60.0f;
        }
        if ((r212_work->rel >> (i + 12)) & 1) {
            r212_work->rack[1]->pos.y += 60.0f;
        }
    }
}

// Task: waits for all four switches, then opens the first door.
static void r212_Puzzle()
{
    while (RsfCheck(G_ROOM_ID, 1) == 0) {
        if ((r212_work->hitNow & 0xF) == 0xF) {
            RsfSet(G_ROOM_ID, 1);
        }
        if (DebugTrg(0) == 1) {
            RsfSet(G_ROOM_ID, 1);
        }
        SceSleep(1);
    }
    SceEventStart(0);
    CamCtrl.CutCall(3);
    r212_work->rack[0]->be_flag &= ~2;
    r212_work->rack[1]->be_flag &= ~2;
    r212_work->door[0].setOpen();
    SceSetEventCancel(1, (TaskFunc) r212_PuzzleEndProc, 0, 1, 1);
    while (r212_work->door[0].getStatus() != 1) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r212_PuzzleEndProc();
}

// End of the switch puzzle (also its cancel path): the first door snapped open, camera back, the two
// racks hidden, SceEventEnd, autosave.
static void r212_PuzzleEndProc()
{
    if (pG->Room_flg[0] & 0x40000000) {
        r212_work->door[0].setOpened();
    }
    CamCtrl.Comeback(0);
    r212_work->rack[0]->be_flag |= 2;
    r212_work->rack[1]->be_flag |= 2;
    SceEventEnd(0);
    GameSave.save(pSaveData, -1);
}

// Task: the roof trap event (the player steps in behind the first door).
static void r212_EventTrap()
{
    Vec pos = {-508.0f, 0.0f, -2166.0f};
    Vec ang;
    ReadModule* m = SearchEmModule(0x11);

    RsfSet(G_ROOM_ID, 0);
    SceEventStart(0);
    if (r212_work->evd->waitLoadOk() == 1 && m != 0) {
        MemorySwap(m->pArc, (u32) r212_work->evd->m_addr, r212_work->evd->m_size);
        EvtMgr.SetEvt(m->pArc, 0);
        while (EvtMgr.IsAliveEvt(&EvtMgr.NowExeEvtKey, 0, 0)) {
            SceSleep(1);
        }
        MemorySwap(m->pArc, (u32) r212_work->evd->m_addr, r212_work->evd->m_size);
        r212_work->evd->setCommand(CMND_DEL_DATA, 0, 0);
    }
    Vec* pa = &ang;
    f32 ry = -2.68f;
    {
        cPlayer* pl = pPL;
        Vec* pp = &pos;

        pl->setPos(pp);
        ang.x = 0.0f;
        pa->y = ry;
        ang.z = 0.0f;
        pl->setAng(pa);
    }
    {
        cEm* sub = pSUB;
        Vec* pp = &pos;

        if (sub) {
            sub->setPos(pp);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            sub->setAng(pa);
            SubCharCtrl(SCC_CHASE, 0);
        }
    }
    r212_work->door[0].setClosed();
    SceExec(0x12, (TaskFunc) r212_RoofMove, 0, 0, SCE_PRIO_DEF_2, 0);
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r212_DoorMessage, 0, 1);
    SceEventEnd(0);
}

// Event r212s00 callback (the roof trap closes in): objects 0x1B/0xC shown for the event, light mask
// 0x40 on pl0100, then hidden again.
void Evt_R212S00_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        SmdGetObjPtr(0x1B)->be_flag &= ~2;
        SmdGetObjPtr(0xC)->be_flag &= ~2;
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            void* mod;

            if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 0x40;
            }
        }
        break;
    case 2:
        SmdGetObjPtr(0x1B)->be_flag |= 2;
        SmdGetObjPtr(0xC)->be_flag |= 2;
        break;
    }
}

// Task: the roof comes down.
static void r212_RoofMove()
{
    Vec pos = {-768.0f, 0.0f, -6180.0f};
    const f32 spd0 = 2.0f;
    const f32 spd1 = 30.0f;
    const f32 spd2 = 100.0f;
    u32 cnt;
    cObj* o0 = SmdGetObjPtr(0xC);
    cObj* o1 = SmdGetObjPtr(0x15);
    cObj* o2 = SmdGetObjPtr(0x1A);
    cObj* o3 = SmdGetObjPtr(0);

    RoomSeCall(0x14, &pos, 0, 0, 0);
    o2->be_flag |= 2;
    o0->be_flag |= 2;
    o1->be_flag |= 2;
    o0->be_flag |= 0x20;
    o2->be_flag |= 0x20;
    o1->be_flag |= 0x20;
    o3->be_flag |= 0x1020;
    QuakeExec(0, 0, 3000, 5.0f, 2);
    SceExec(0x12, (TaskFunc) r212_AdhleyToPointWait, 0, 0, SCE_PRIO_DEF_2, 0);
    cnt = 0;
    do {
        if (cnt % 10 == 0) {
            EstSet(0, -1, 0, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        cnt++;
        o0->pos.y -= spd0;
        o2->pos.y -= spd0;
        o1->pos.y -= spd0;
        o3->pos.y -= spd0;
        if (o0->pos.y <= -2750.0f) {
            RoomSeCall(0x16, &pos, 0, 0, 0);
            SceEventStart(0);
            CamCtrl.CutCall(8);
            while (o0->pos.y >= -3500.0f) {
                o0->pos.y -= spd1;
                o2->pos.y -= spd1;
                o1->pos.y -= spd1;
                o3->pos.y -= spd1;
                SceSleep(1);
            }
            while (o0->pos.y >= -5000.0f) {
                o0->pos.y -= spd2;
                o2->pos.y -= spd2;
                o1->pos.y -= spd2;
                o3->pos.y -= spd2;
                SceSleep(1);
            }
            QuakeExec(0, 0, 10, 10.0f, 2);
            RoomSeCall(0x15, &pos, 0, 0, 0);
            DiedemoExec(0x2D, 0);
        }
        SceSleep(1);
    } while (!(pG->Room_flg[0] & 0x20000000));
    RoomSeCall(0x15, &pos, 0, 0, 0);
    SndRoomStrStop(2);
    SndBgmTblSet(0x212, 1);
    QuakeKill(0);
    SceSleep(2);
    o0->be_flag &= ~0x20;
    o2->be_flag &= ~0x20;
    o1->be_flag &= ~0x20;
    o3->be_flag &= ~0x20;
}

// Task: the four hit boxes on the roof; each one shot stops that part of the trap.
static void r212_RoofTrapWatcher()
{
    Vec d;
    cObj* o = SmdGetObjPtr(0x15);
    Vec ofs[4] = {
        {-2475.0f, 5089.0f, -7240.0f},
        {1459.0f, 5089.0f, -7240.0f},
        {-2475.0f, 5089.0f, -3270.0f},
        {1476.0f, 5089.0f, -3266.0f},
    };
    int prm[4][3] = {{2, 4, 6}, {0, 2, 4}, {3, 5, 7}, {1, 3, 5}};
    u32 i;

    for (i = 0; i < 4; i++) {
        PSVECSubtract(&ofs[i], &o->pos, &d);
        r212_work->hit[i] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &d, 0, 0);
        r212_work->hit[i]->setParent(o, 0, 0);
        YarareInit(r212_work->hit[i], 0.0f, 0.0f, 0.0f, 400.0f, 0.0f, 0, YAT_FLAG_ON);
        EstSet(0, -1, 0, 0, EFF_ROOM, (u8) prm[i][0], 0x800, (u8) prm[i][1], 0, 0);
    }
    for (;;) {
        for (i = 0; i < 4; i++) {
            if (FlagChkVar(&pG->Room_flg, i + 3) == 0 && r212_work->hit[i]->ckStatus() == 1) {
                FlagOnVar(&pG->Room_flg, i + 3);
                EffectEspDelete(0x800, (u8) prm[i][1], 0, 0);
                EffectEspgenDelete(0x800, (u8) prm[i][1], 0);
                EffectEfmDelete(0x800, (u8) prm[i][1], 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, (u8) prm[i][2], 0, ESP_CORE_KIND_NONE, 0, 0);
                RoomSeCall(0x12, 0, 0, 0, 0);
            }
        }
        if (R212_TRAP_FLAGS_ALL(pG->Room_flg[0])) {
            break;
        }
        if (SceAtHitCheck(7) || SceAtHitCheck(8)) {
            r212_work->door[1].setClose();
        }
        SceSleep(1);
    }
    pG->Room_flg[0] |= 0x20000000;
    r212_work->door[1].setOpen();
}

// Area 0xA, the shut roof-room door: message 2; the first time Ashley's pointing task starts.
static void r212_MesRoofDoor()
{
    SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    if (!(pG->Room_flg[0] & 0x01000000)) {
        pG->Room_flg[0] |= 0x01000000;
        SceExec(0x12, (TaskFunc) r212_AshleyPointToCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    }
}

// Task: Ashley points at the door after a while.
static void r212_AdhleyToPointWait()
{
    SceSleep(750);
    if (!(pG->Room_flg[0] & 0x01000000)) {
        pG->Room_flg[0] |= 0x01000000;
        r212_work->mesNo = 5;
        if (R212_TRAP_FLAGS_NONE(pG->Room_flg[0])) {
            SetSubAux(r212_AshleyPointTo, 0);
        }
    }
    SceSleep(300);
    if (!(pG->Room_flg[0] & 0x20000000)) {
        r212_work->mesNo = 4;
        SetSubAux(r212_AshleyPointTo, 0);
    }
}

// SetSubAux routine: Ashley turns to the door, points and speaks.
static void r212_AshleyPointTo(cEm* sub)
{
    cObj* o = SmdGetObjPtr(0x15);

    switch (sub->r_no_2) {
    case 0:
        sub->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 10, 0, 1, 0);
        sub->r_no_2++;
    case 1:
        if (sub->motionMove()) {
            sub->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 10, 0, 1, 0);
            sub->r_no_2++;
            cMes.MesSet(r212_work->mesNo, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 0x01000052, 0, 0, 4);
            RoomSeCall(0x13, &sub->pos, 0, 0, sub);
        }
        break;
    case 2:
        if (sub->motionMove()) {
            int i;

            sub->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x27), 10, 0, 1, 0);
            sub->r_no_2++;
            MessageControl* m = &cMes;
            for (i = 0; i < 16; i++) {
                m->Delete(i);
            }
        }
        break;
    case 3:
        if (sub->motionMove()) {
            sub->r_no_0 = 0;
            sub->r_no_1 = 0;
            sub->r_no_2 = 0;
            sub->r_no_3 = 0;
        }
        break;
    }
    sub->ang.y += Muku(&sub->pos, &o->pos, sub->ang.y, 0.39269908f);
}

// 150 frames after the door message, message 3 and Ashley points at the door (unless a trap part is already stopped).
static void r212_AshleyPointToCheck()
{
    SceSleep(150);
    r212_work->mesNo = 3;
    if (R212_TRAP_FLAGS_NONE(pG->Room_flg[0])) {
        SetSubAux(r212_AshleyPointTo, 0);
    }
}

// Task: the drill Ganados break in once Ashley reaches the far wall.
static void r212_DrillAppearCheck()
{
    Vec v;
    int st;

    while (pSUB == 0 || SceAtCheckHitModel(0x13, pSUB) == 0) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 0, 1);
    int zero = 0;
    SceEventStart(0);
    r212_work->em[0].setNoSuspend(1);
    r212_work->em[1].setNoSuspend(1);
    pSUB->setNoSuspend(1);
    {
        cEm* sub = pSUB;

        v.x = -15000.0f;
        v.y = 0.0f;
        v.z = 16160.0f;
        sub->setPos(&v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        sub->setAng(&v);
    }
    SetSubAux(r212_AshleyDrillAction, 0);
    CamCtrl.CutCall(9);
    pG->Room_flg[0] &= ~0x40000000;
    SceSetEventCancel(1, (TaskFunc) r212_DrillAppearCheckEndProc, 0, 1, 1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xD, 0, ESP_CORE_KIND_NONE, (void*) zero, (void*) zero);
    r212_work->door[2].setClose();
    while ((st = r212_work->door[2].getStatus()) != 0) {
        SceSleep(1);
    }
    SceSleep(30);
    r212_work->se = RoomSeCall(0, &SmdGetObjPtr(0x2C)->pos, 0, 0x80000000, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_ROOM00, (void*) st, (void*) st);
    SmdGetObjPtr(0x32)->be_flag &= ~2;
    CamCtrl.CutCall(4);
    SceSleep(10);
    r212_work->se = RoomSeCall(2, 0, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(5);
    r212_work->se = EmSeCall(0x1A, 0, 0x11, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(6);
    r212_work->se = EmSeCall(0x4E, 0, 0x11, 0, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r212_DrillAppearCheckEndProc();
}

// End of the drill cutscene (also its cancel path): the exit door snapped shut, the drill object
// shown with its SE, Ashley placed at the far wall facing 3.09 rad, the drill Ganados alerted, the
// end-check task.
static void r212_DrillAppearCheckEndProc()
{
    Vec v;

    if (pG->Room_flg[0] & 0x40000000) {
        r212_work->door[2].setClosed();
        SmdGetObjPtr(0x32)->be_flag &= ~2;
        RoomSeCall(0, &SmdGetObjPtr(0x2C)->pos, 0, 0x80000000, 0);
        SndStop(r212_work->se, 0);
    }
    {
        cPlayer* pl = pPL;

        v.x = -15140.0f;
        v.y = 0.0f;
        v.z = 19300.0f;
        f32 ry = 3.09f;
        pl->setPos(&v);
        v.x = 0.0f;
        v.y = ry;
        v.z = 0.0f;
        pl->setAng(&v);
    }
    SubCharCtrl(SCC_AUX_MOT, 0);
    {
        cEm* sub = pSUB;

        v.x = -15000.0f;
        v.y = 0.0f;
        v.z = 16160.0f;
        sub->setPos(&v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        sub->setAng(&v);
    }
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 5, 0);
    r212_work->em[0].setNoSuspend(0);
    r212_work->em[1].setNoSuspend(0);
    pSUB->setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r212_work->eat2->m_Flag &= ~4;
    SceExec(0x12, (TaskFunc) r212_DrillMove, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r212_DrillEndCheck, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Task: both drill Ganados dead -> the last door opens.
static void r212_DrillEndCheck()
{
    while (1) {
        if (r212_work->em[0].isActive() == 0 && r212_work->em[1].isActive() == 0) {
            RsfSet(G_ROOM_ID, 2);
            break;
        }
        SceSleep(1);
    }
    r212_work->door[2].setOpen();
    while (r212_work->door[2].getStatus() != 1) {
        SceSleep(1);
    }
    SceSleep(5);
    SubCharCtrl(SCC_CHASE, 0);
    SceAtDataReset(0);
}

// The drill objects' work: the spinning speed.
struct R212DrillWork {
    Vec rotSpd;   // 0x328
};

// Task: the drill rises through the floor.
static void r212_DrillMove()
{
    Vec v;
    u32 i;
    cObj* d0 = SmdGetObjPtr(0x2A);
    cObj* d1 = SmdGetObjPtr(0x2B);
    cObj* d2 = SmdGetObjPtr(0x2C);
    R212DrillWork* w1 = (R212DrillWork*) &((cObjUnion*) d1)->work;
    R212DrillWork* w2 = (R212DrillWork*) &((cObjUnion*) d2)->work;

    d0->be_flag |= 0x20;
    d1->be_flag |= 0x20;
    d2->be_flag |= 0x20;
    QuakeExec(0, 0, 3000, 10.0f, 2);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 0x800, ESP_CORE_KIND_ROOM00, 0, 0);
    while (1) {
        d0->pos.z += 40.0f;
        d1->pos.z += 40.0f;
        d2->pos.z += 40.0f;
        r212_work->sat2->setCoord(&d2->pos, &d2->ang);
        r212_work->eat->setCoord(&d2->pos, &d2->ang);
        if (SceAtHitCheck(0xD)) {
            f32 dist1;
            f32 dist2;

            v = d2->pos;
            v.y = pPL->pos.y;
            dist1 = PSVECSquareDistance(&pPL->pos, &v);
            dist2 = PSVECSquareDistance(&pSUB->pos, &v);
            SceDebugDisp("Dist1[%f]", dist1);
            SceDebugDisp("Dist2[%f]", dist2);
            if (dist1 < 2890000.0f) {
                pG->pl_life = 1;
                pPL->dmg.m_Timer = 0;
                PlWepHitCheck2(0, &pPL->pos, &pPL->pos, 0x12, 3, 3000.0f);
                break;
            } else if (dist2 < 2890000.0f) {
                pG->ashley_life = 1;
                pSUB->dmg.m_Timer = 0;
                PlWepHitCheck2(0, &pSUB->pos, &pSUB->pos, 0x12, 3, 3000.0f);
                break;
            }
        }
        if (d2->pos.z >= 16000.0f) {
            break;
        }
        SceSleep(1);
    }
    QuakeKill(0);
    EffectEspDelete(0x800, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(0x800, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(0x800, ESP_CORE_KIND_ROOM00, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 0, ESP_CORE_KIND_NONE, 0, 0);
    RoomSeCall(1, &d2->pos, 0, 0, d2);
    for (i = 0; i < 60; i++) {
        w1->rotSpd.z -= 0.00225f;
        w2->rotSpd.z += 0.00275f;
        SceSleep(1);
    }
    d0->be_flag &= ~0x20;
    d1->be_flag &= ~0x20;
    d2->be_flag &= ~0x20;
    SndRoomStrStop(2);
}

// SetSubAux routine: Ashley's reaction to the drill.
static void r212_AshleyDrillAction(cEm* sub)
{
    switch (sub->r_no_2) {
    case 0:
        sub->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x28), 10, 0, 1, 0);
        sub->r_no_2++;
    case 1:
        if (sub->motionMove()) {
            sub->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 10, 0, 1, 0);
            EmSeCall(3, &sub->pos, 3, 0, 0, 0);
            sub->r_no_2++;
        }
        break;
    case 2:
        if (sub->motionMove()) {
            sub->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 5, 0);
            sub->r_no_2++;
        }
        break;
    case 3:
        sub->motionMove();
        break;
    }
}

// The locked entrance: common message 0x67.
static void r212_DoorMessage()
{
    SceUpCut(0x67, -1, -1, UP_CUT_ATTR_MES_COMMON);
}

// Task: the entrance door lock.
static void r212_DoorLock()
{
    cEmDoor* door;

    if (getRoomEtcDoor(0, &door, 1)) {
        door->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0);
    }
    if (door) {
        while (door->ckLock()) {
            SceSleep(1);
        }
        ScfFlagOn(pG, SCF_76);
    }
}

// Bind door `id_` (0x1B / 0x16 / 0x22): the object, its rest position, opening height and flag; door
// 0x16 also arms area 0xA with the shut-door message.
void cR212Door::init(u32 id_)
{
    valid = 0;
    id = id_;
    obj = SmdGetObjPtr(id_);
    if (obj) {
        obj->be_flag |= 0x20;
        pos0 = obj->pos;
        switch (id) {
        case 0x1B:
            flagNo = SCF_78;
            openH = 2900.0f;
            break;
        case 0x16:
            openH = 2400.0f;
            SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r212_MesRoofDoor, 0, 1);
            flagNo = SCF_8b;
            break;
        case 0x22:
            flagNo = SCF_77;
            openH = 2400.0f;
            break;
        }
        valid = 1;
    }
}

// Per-frame step: run the current mode (wait / open / close) of r212_doorTbl.
void cR212Door::move()
{
    if (valid != 0) {
        (this->*r212_doorTbl[mode])();
    }
}

// Mode 0: idle.
void cR212Door::wait()
{
}

// Mode 1: the door's open SE (per door), rises 30 units a frame to pos0 + openH; on arrival its
// collision areas go off and status 1.
void cR212Door::open()
{
    int se;

    switch (step) {
    case 0:
        if (id == 0x16) {
            SceAtSetEnable(0xA, 0);
            se = 0xE;
        } else if (id == 0x22) {
            se = 0xA;
        } else {
            se = 6;
        }
        this->se = RoomSeCall(se, &obj->pos, 0, 0, obj);
        step++;
    case 1:
        obj->pos.y += 30.0f;
        if (obj->pos.y > pos0.y + openH) {
            obj->pos.y = pos0.y + openH;
            step++;
        }
        break;
    case 2:
        if (id == 0x16) {
            se = 0xF;
            SceAtSetEnable(9, 0);
        } else if (id == 0x22) {
            se = 0xB;
            SceAtSetEnable(0x16, 0);
        } else {
            SceAtSetEnable(0xF, 0);
            se = 7;
            SceAtSetEnable(6, 0);
        }
        this->se = RoomSeCall(se, &obj->pos, 0, 0, obj);
        obj->pos = pos0;
        obj->pos.y += openH;
        status = 1;
        mode = 0;
        step = 0;
        if (flagNo) {
            FlagOnVar(&pG->Scenario_flg, (u32) flagNo);
        }
        break;
    }
}

// Mode 2: the door's close SE, its area on, drops with growing speed to pos0, then a short shake; status 0.
void cR212Door::close()
{
    int se = 0;

    switch (step) {
    case 0:
        if (id == 0x16) {
            SceAtSetEnable(9, 1);
            se = 0x10;
            spd = -50.0f;
        } else if (id == 0x22) {
            SceAtSetEnable(0x16, 1);
            se = 0xC;
            spd = -50.0f;
        }
        this->se = RoomSeCall(se, &obj->pos, 0, 0, obj);
        step++;
    case 1:
        obj->pos.y += spd;
        spd -= 10.0f;
        if (obj->pos.y < pos0.y) {
            obj->pos.y = pos0.y;
            step++;
        }
        break;
    case 2:
        if (id == 0x16) {
            se = 0x11;
            SceAtSetEnable(0xA, 1);
        } else if (id == 0x22) {
            se = 0xD;
        }
        this->se = RoomSeCall(se, &obj->pos, 0, 0, obj);
        step++;
        timer = 5;
    case 3:
        obj->pos.x = pos0.x + fRand1_1() * 20.0f;
        obj->pos.y = pos0.y + fRand0_1() * 20.0f;
        obj->pos.z = pos0.z + fRand1_1() * 20.0f;
        if (timer != 0) {
            timer--;
        } else {
            obj->pos = pos0;
            timer = 30;
            step++;
        }
        break;
    case 4:
        if (timer != 0) {
            timer--;
        } else {
            status = 0;
            mode = 0;
        }
        if (flagNo) {
            FlagOffVar(&pG->Scenario_flg, (u32) flagNo);
        }
        break;
    }
}

// Request opening (mode 1) unless open / opening; status 4 = moving.
void cR212Door::setOpen()
{
    if (valid == 0) {
        return;
    }
    if (status == 1) {
        return;
    }
    if (mode == 1) {
        return;
    }
    status = 4;
    mode = 1;
    step = 0;
}

// Request closing (mode 2) unless closed / closing.
void cR212Door::setClose()
{
    if (valid == 0) {
        return;
    }
    if (status == 0) {
        return;
    }
    if (mode == 2) {
        return;
    }
    status = 4;
    mode = 2;
    step = 0;
}

// Snap the door open (its collision areas off, SE stopped).
void cR212Door::setOpened()
{
    if (valid == 0) {
        return;
    }
    status = 1;
    obj->pos.y = pos0.y + openH;
    mode = 0;
    step = 0;
    if (id == 0x16) {
        SceAtSetEnable(9, 0);
        SceAtSetEnable(0xA, 0);
    } else if (id == 0x22) {
        SceAtSetEnable(0x16, 0);
    } else {
        SceAtSetEnable(0xF, 0);
        SceAtSetEnable(6, 0);
    }
    SndStop(se, 0);
    if (flagNo) {
        FlagOnVar(&pG->Scenario_flg, (u32) flagNo);
    }
}

// Snap the door closed (its collision areas on, SE stopped).
void cR212Door::setClosed()
{
    if (valid == 0) {
        return;
    }
    status = 0;
    obj->pos.y = pos0.y;
    mode = 0;
    step = 0;
    if (id == 0x16) {
        SceAtSetEnable(9, 1);
        SceAtSetEnable(0xA, 1);
    } else if (id == 0x22) {
        SceAtSetEnable(0x16, 1);
    } else {
        SceAtSetEnable(0xF, 1);
    }
    SndStop(se, 0);
    if (flagNo) {
        FlagOffVar(&pG->Scenario_flg, (u32) flagNo);
    }
}

// 0 closed, 1 opened, 4 moving; -1 when the door has no object.
int cR212Door::getStatus()
{
    if (valid != 0) {
        return status;
    }
    return -1;
}

// Item-event opener: chest 6 (lid up -X) or drawer 0x3E (slides +Z).
static void r212_TreasureBoxOpen(int id)
{
    switch (id) {
    case 6:
        OpenBoxMain(OpenBoxUpXM, 0, 0x5B, 6, -1, -1);
        break;
    case 0x3E:
        OpenBoxMain(OpenBoxPosZP500, 0, 0x1B, 0x3E, -1, -1);
        break;
    }
}

// Item-event "already opened": pose the chest / drawer open.
static void r212_TreasureBoxOpened(int id)
{
    switch (id) {
    case 6:
        OpenBoxMain(OpenBoxUpXM, 1, 0x5B, 6, -1, -1);
        break;
    case 0x3E:
        OpenBoxMain(OpenBoxPosZP500, 1, 0x1B, 0x3E, -1, -1);
        break;
    }
}

// The next room's .rodata starts 8-aligned (the split object carries the pad).
asm(".section .rodata\n\t.balign 8\n\t.text");
