#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "datactrl.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emswitch.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "mes.h"
#include "rnd.h"
#include "snd.h"
#include "quake.h"
#include "sscrn.h"
#include "fade.h"
#include "cSceObj.h"

// Room 2-27 (D:/Bio4/Prog/r227.cpp): the entrance event, the cargo lift with its enemy waves and
// falling crates, the gondola and the shelf items.

struct R227Work {
    cDataUnit* evd[3];    // 0x000  r227s00 / s01 / s02 event data
    u8 pad_0C[0x40];
    cEm* sw;              // 0x04C  lever (cEmSwitch)
    cObj* elv;            // 0x050  the lift
    cObj* elv2;           // 0x054  its cage
    f32 elvY0;            // 0x058  start heights
    f32 elv2Y0;           // 0x05C
    cSat* sat;            // 0x060
    cSat* eat;            // 0x064
    cEm* rack[2];         // 0x068  the two crate racks
    cEm* box[3];          // 0x070  crates riding the lift
    f32 boxY0[3];         // 0x07C
    u32 emOnElvCnt;       // 0x088  enemies dropped onto the lift so far
    cEm* emFall[16];      // 0x08C  enemies that die when they fall off the lift
    cSceObj gondola;      // 0x0CC
};

// The work pointer is a struct member: every store through the work reloads it.
struct R227WorkPtr {
    R227Work* p;
};

static R227WorkPtr r227_work;

u32 r227_boxNo[4] = {6, 7, 8, 0};

// Pointer store through a reference: the pG load of the next create stays below it.
static inline void PSetSat(cSat*& d, cSat* v) { d = v; }
// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");
static inline u32* evtKey(EventMgr* m) { return &m->NowExeEvtKey; }
// The room build's cEmRack::setBreak prototype had a Vec* the DOL definition does not read.
void cEmRackSetBreakV(cEmRack* r, Vec* pos) asm("setBreak__7cEmRack");

void r227_openShelf_main(int id, int opened);
static void r227_openShelf(int id);
static void r227_openedShelf(int id);
u32 r227_checkEmNumOnElv();
static void r227_checkBox0Fall();
static void r227_checkBox1Fall();
void r227_initEmFall();
void r227_setEmFall(cEm* em);
static void r227_checkEmFall();
int r227_resetEmOnElv2(cEmWrap* em);
static void r227_setEmOnElv2();
static void r227_setEmOnElv1();
void r227_moveElv(f32 dy);
int r227_checkElvMovePermit();
static void r227_operateElv();
void r227_initCargoElv();
static void r227_setEm3_after();
static void r227_setEm3();
static void r227_setEm2();
void r227_setEm1();
void r227_initGondola();
static void r227_execGondola(int dir);
static void r227_execEvent00();
static void r227_succeedAction();
static void Evt_R227S00_Func(Event* e);
static void Evt_R227S01_Func(Event* e);
static void Evt_R227S02_Func(Event* e);

// Room init: JumpPoint 2 skips the entrance event (Room_flg bit 0); the lever (etc switch 4) is a
// barrel-type auto-open switch; debug trigger 1 replays the event. First visit: a coin toss (Room_flg[0]
// 0x40000000) picks which follow-up event (s01 / s02) the entrance QTE leads to, the three evd files
// are pre-loaded and the event task starts; else the enemies (r227_setEm1) and, unless bit 6, the
// return layout. The gondola, the cargo lift, two shelf item events.
void R227Init()
{
#line 57 "D:/Bio4/Prog/r227.cpp"
    R227Work*& wp = r227_work.p;
    wp = (R227Work*) MEM_CALLOC(sizeof(R227Work), 1, 0xD);
    if (pG->JumpPoint == 2) {
        RsfSet(G_ROOM_ID, 0);
        EmReadSearch(0x14, 0, 0);
    }
    getRoomEtcSwitch(4, &r227_work.p->sw, 1);
    if (r227_work.p->sw) {
        ((cEmSwitch*) r227_work.p->sw)->setAutoOpen();
        ((cEmSwitch*) r227_work.p->sw)->setBarrel();
    }
    if (DebugTrg(1)) {
        RsfClear(G_ROOM_ID, 0);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        if (Rnd() & 0x80) {
            pG->Room_flg[0] |= 0x40000000;
        } else {
            pG->Room_flg[0] &= ~0x40000000;
        }
        SceExec(0x12, (TaskFunc) r227_execEvent00, 0, 0, SCE_PRIO_DEF_2, 0);
        EvtMgr.SetFunc("evt_r227s00_func", (void*) Evt_R227S00_Func);
        EvtMgr.SetFunc("evt_r227s01_func", (void*) Evt_R227S01_Func);
        EvtMgr.SetFunc("evt_r227s02_func", (void*) Evt_R227S02_Func);
        r227_work.p->evd[0] = DC.setData(EvtMgr.NameChange("evd/r227s00.evd"));
        r227_work.p->evd[0]->setCommand(CMND_MRAM_LOAD, 0, 0);
        r227_work.p->evd[1] = DC.setData(EvtMgr.NameChange("evd/r227s01.evd"));
        r227_work.p->evd[1]->setCommand(CMND_ARAM_LOAD, 0, 0);
        r227_work.p->evd[2] = DC.setData(EvtMgr.NameChange("evd/r227s02.evd"));
        r227_work.p->evd[2]->setCommand(CMND_ARAM_LOAD, 0, 0);
    } else {
        r227_setEm1();
        if (RsfCheck(G_ROOM_ID, 6) == 0) {
            SndRoomStrStart(1, 3, 1);
        }
    }
    r227_initGondola();
    r227_initCargoElv();
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    SceSetItemEvent(0x11, 0x86, 4, 9, r227_openShelf, (void (*)()) r227_openedShelf, 0, 0);
    SceSetItemEvent(0x12, 0x85, 5, 0xA, r227_openShelf, (void (*)()) r227_openedShelf, 1, 0);
}

// Per-frame room main: nothing.
void R227Main()
{
}

// Shelf `id` (chest 0xBC / 0xA1, parts lid up -X) opens (opened: snap).
void r227_openShelf_main(int id, int opened)
{
    switch (id) {
    case 0:
        OpenBoxMain(OpenBoxPartsUpXM, opened, 0x5B, 0xBC, -1, -1);
        break;
    case 1:
        OpenBoxMain(OpenBoxPartsUpXM, opened, 0x5B, 0xA1, -1, -1);
        break;
    }
}

// Item-event opener: animate shelf `id` open.
static void r227_openShelf(int id)
{
    r227_openShelf_main(id, 0);
}

// Item-event "already opened": pose shelf `id` open.
static void r227_openedShelf(int id)
{
    r227_openShelf_main(id, 1);
}

// Enemies standing in the lift area.
u32 r227_checkEmNumOnElv()
{
    u32 n = 0;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        if (SceAtCheckHitModel(0xE, (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i)) == 1) {
            n++;
        }
    }
    return n;
}

// Task: rack 0 tips over and falls once the player leaves its area.
static void r227_checkBox0Fall()
{
    SceSleep(1);
    while (SceAtCheckHitModel(0xE, r227_work.p->rack[0]) == 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 1);
    while (1) {
        r227_work.p->rack[0]->pParts->ang.x += -0.034906585f;
        if (r227_work.p->rack[0]->pParts->ang.x < -0.5235988f) {
            break;
        }
        SceSleep(1);
    }
    const f32 acc = 20.0f;
    const f32 lim = 10000.0f;
    const f32 rotLim = -1.5707964f;
    f32 baseY = r227_work.p->rack[0]->pos.y;
    f32 spd = 0.0f;
    while (1) {   // `for (;;)` here rotates the SceSleep to the loop top
        if (r227_work.p->rack[0]->pParts->ang.x > rotLim) {
            r227_work.p->rack[0]->pParts->ang.x += -0.034906585f;
        }
        r227_work.p->rack[0]->pos.y -= spd;
        spd += acc;
        if (baseY - r227_work.p->rack[0]->pos.y > lim) {
            break;
        }
        SceSleep(1);
    }
    cEmRackSetBreakV((cEmRack*) r227_work.p->rack[0], &r227_work.p->rack[0]->pos);
}

// Task: rack 1, the same the other way round.
static void r227_checkBox1Fall()
{
    SceSleep(1);
    while (SceAtCheckHitModel(0xE, r227_work.p->rack[1]) == 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 2);
    while (1) {
        r227_work.p->rack[1]->pParts->ang.z += 0.034906585f;
        if (r227_work.p->rack[1]->pParts->ang.z > 0.5235988f) {
            break;
        }
        SceSleep(1);
    }
    const f32 acc = 20.0f;
    const f32 lim = 10000.0f;
    const f32 rotLim = 1.5707964f;
    f32 baseY = r227_work.p->rack[1]->pos.y;
    f32 spd = 0.0f;
    while (1) {   // `for (;;)` here rotates the SceSleep to the loop top
        if (r227_work.p->rack[1]->pParts->ang.z < rotLim) {
            r227_work.p->rack[1]->pParts->ang.z += 0.034906585f;
        }
        r227_work.p->rack[1]->pos.y -= spd;
        spd += acc;
        if (baseY - r227_work.p->rack[1]->pos.y > lim) {
            break;
        }
        SceSleep(1);
    }
    cEmRackSetBreakV((cEmRack*) r227_work.p->rack[1], &r227_work.p->rack[1]->pos);
}

// Clear the list of enemies watched for falling off the lift.
void r227_initEmFall()
{
    u32 i;

    for (i = 0; i < 16; i++) {
        r227_work.p->emFall[i] = 0;
    }
}

// Add a live enemy to the fall watch list (first free of 16 slots).
void r227_setEmFall(cEm* em)
{
    if (em) {
        cEmWrap w;

        w.setPtr(em, 1);
        if (w.isAlive()) {
            u32 i;

            for (i = 0; i < 16; i++) {
                if (r227_work.p->emFall[i] == 0) {
                    r227_work.p->emFall[i] = em;
                    break;
                }
            }
        }
    }
}

// Task: an enemy that fell well below the player dies.
static void r227_checkEmFall()
{
    while (1) {
        u32 i;

        if (RsfCheck(G_ROOM_ID, 3)) {
            SceExit();
        }
        for (i = 0; i < 16; i++) {
            if (r227_work.p->emFall[i]) {
                cEmWrap w;

                w.setPtr(r227_work.p->emFall[i], 1);
                if (w.getPosY() < pPL->pos.y - 200.0f) {
                    if (w.getHp() > 0) {
                        w.setHp(0);
                    }
                }
            }
        }
        SceSleep(1);
    }
}

// If the enemy may be reset, reset it onto the lift, count it and wait half a second; 1 when it was.
int r227_resetEmOnElv2(cEmWrap* em)
{
    if (em->ckResetEnable() == 0) {
        return 0;
    }
    em->setReset();
    r227_work.p->emOnElvCnt++;
    SceSleep(30);
    return 1;
}

// Task: the waves dropped onto the moving lift.
static void r227_setEmOnElv2()
{
    cEmWrap e0;
    cEmWrap e1;
    cEmWrap e2;
    cEmWrap e3;

    e0.setEm(0x81, -1, 1, 1, 1);
    e1.setEm(0x82, -1, 1, 1, 1);
    e0.setFlag(1);
    e1.setFlag(1);
    r227_setEmFall(e0.getPtr());
    r227_setEmFall(e1.getPtr());
    r227_work.p->emOnElvCnt = 2;
    while (1) {
        if (RsfCheck(G_ROOM_ID, 3)) {
            SceExit();
        }
        if (r227_work.p->emOnElvCnt > 2) {
            break;
        }
        if (r227_checkEmNumOnElv() <= 2) {
            int r = Rnd() & 2;
            int n = r227_resetEmOnElv2(&e0) == 1;

            if (r == n) {
                continue;
            }
            if (r227_resetEmOnElv2(&e1) == 1) {
                n++;
            }
            if (r == n) {
                continue;
            }
        }
        SceSleep(90);
    }
    e2.setEm(0x83, -1, 1, 1, 1);
    e3.setEm(0x84, -1, 1, 1, 1);
    e2.setFlag(1);
    e3.setFlag(1);
    r227_setEmFall(e2.getPtr());
    r227_setEmFall(e3.getPtr());
    r227_work.p->emOnElvCnt += 2;
    while (1) {
        if (RsfCheck(G_ROOM_ID, 3)) {
            SceExit();
        }
        if (r227_work.p->emOnElvCnt > 14) {
            break;
        }
        if (r227_checkEmNumOnElv() <= 2) {
            int r = Rnd() & 2;
            int n = r227_resetEmOnElv2(&e0) == 1;

            if (r == n) {
                continue;
            }
            if (r227_resetEmOnElv2(&e1) == 1) {
                n++;
            }
            if (r == n) {
                continue;
            }
            if (r227_resetEmOnElv2(&e2) == 1) {
                n++;
            }
            if (r == n) {
                continue;
            }
            if (r227_resetEmOnElv2(&e3) == 1) {
                n++;
            }
            if (r == n) {
                continue;
            }
        }
        SceSleep(120);
    }
}

// Task: the first two enemies on the lift, then the fall watcher and the waves.
static void r227_setEmOnElv1()
{
    SceSleep(30);
    cEmWrap e0;
    cEmWrap e1;
    cEmWrap e2;
    cEmWrap e3;
    e0.setEm(0x85, -1, 1, 1, 1);
    e1.setEm(0x86, -1, 1, 1, 1);
    SceExec(0x12, (TaskFunc) r227_checkEmFall, 0, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(360);
    SceExec(0x12, (TaskFunc) r227_setEmOnElv2, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Moves the lift group dy above its start height.
void r227_moveElv(f32 dy)
{
    u32 i;

    r227_work.p->elv->pos.y = r227_work.p->elvY0 + dy;
    r227_work.p->elv2->pos.y = r227_work.p->elv2Y0 + dy;
    r227_work.p->sat->setMatrix(r227_work.p->elv->mat);
    r227_work.p->eat->setMatrix(r227_work.p->elv->mat);
    for (i = 0; i < 3; i++) {
        if (r227_work.p->box[i]) {
            r227_work.p->box[i]->pos.y = r227_work.p->boxY0[i] + dy;
        }
    }
}

// 1 while the lift may move: both racks fell and fewer than four crate/enemy points are on it.
int r227_checkElvMovePermit()
{
    u32 n;
    u32 i;

    if (RsfCheck(G_ROOM_ID, 1) == 0 || RsfCheck(G_ROOM_ID, 2) == 0) {
        return 0;
    }
    n = 0;
    for (i = 0; i < 3; i++) {
        if (r227_work.p->box[i] && r227_work.p->box[i]->hp > 0) {
            n++;
        }
    }
    n += r227_checkEmNumOnElv() * 2;
    return n < 4;
}

// The lift lever: asks, then rides up with the enemy waves.
static void r227_operateElv()
{
    f32 dy;
    f32 lim;

    SceAtSetEnable(3, 0);
    // Two sets of one pointer variable (the r40e idiom): `addi r31,r9,cMes@l; addi r31,r31,4`.
    MesWork* w = (MesWork*) &cMes;
    w = (MesWork*) ((u8*) w + 4);
    SceMesSet(0, 0, 1, 0x64, 0x150 - w->lineSpace - w->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case 1:
        SndCall(6, 0xB, 0, 0, 0, 0);
        if (r227_checkElvMovePermit() == 1) {
            break;
        }
        SceMesSet(1, 0, 1, 0x64, 0x150 - w->lineSpace - w->m_font_h - 1);
    case -1:
    case 0:
    case 2:
        SceAtSetEnable(3, 1);
        return;
    }
    GameSaveSave(&GameSave, pSaveData, -1);
    SceAtSetEnable(0xD, 1);
    SceAtSetEnable(0xF, 1);
    SceAtSetEnable(0x14, 1);
    SceExec(0x12, (TaskFunc) r227_setEmOnElv1, 0, 0, SCE_PRIO_DEF_2, 0);
    SndCall(6, 9, &r227_work.p->elv->pos, 0, 0, 0);
    dy = 0.0f;
    // const locals fold into their uses (the step is hoisted by loop.c after `lim`, QuakeExec gets a
    // fresh pool load each time); their declarations fix the pool order 0, 10, 20, 15015.
    const f32 step = 10.0f;
    const f32 power = 20.0f;
    lim = 15015.0f;
    for (;;) {
        dy += step;
        r227_moveElv(dy);
        if (r227_checkElvMovePermit() == 0) {
            SndCall(6, 0xA, &r227_work.p->elv->pos, 0, 0, 0);
            QuakeExec(0, 0, 10, power, 2);
            while (r227_checkElvMovePermit() == 0) {
                SceSleep(1);
            }
            SceSleep(30);
            SndCall(6, 9, &r227_work.p->elv->pos, 0, 0, 0);
            QuakeExec(0, 0, 10, power, 2);
        }
        if (dy > lim) {
            break;
        }
        SceSleep(1);
    }
    r227_moveElv(15015.0f);
    SndCall(6, 0xA, &r227_work.p->elv->pos, 0, 0, 0);
    QuakeExec(0, 0, 10, power, 2);
    RsfSet(G_ROOM_ID, 3);
    SceAtSetEnable(0xD, 0);
    SceAtSetEnable(0x10, 0);
}

// The cargo lift: the platform 0xE3 and cage 0xD6 (script-moved, start heights kept), areas 0x13 and
// 0xA..0xD parented to the platform, its collision / attribute pieces, the crates riding it and the
// two crate racks, then the lever area and the lift state per the saved flags.
void r227_initCargoElv()
{
    r227_work.p->elv = SmdGetObjPtr(0xE3);
    r227_work.p->elv2 = SmdGetObjPtr(0xD6);
    if (r227_work.p->elv && r227_work.p->elv2) {
        u32 i;

        r227_work.p->elv->be_flag |= 0x20;
        r227_work.p->elv2->be_flag |= 0x20;
        SceAtSetParent(0x13, r227_work.p->elv, 0);
        r227_work.p->elvY0 = r227_work.p->elv->pos.y;
        r227_work.p->elv2Y0 = r227_work.p->elv2->pos.y;
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        PSetSat(r227_work.p->sat, SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1));
        r227_work.p->eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 1);
        SceAtSetParent(0xA, r227_work.p->elv, 0);
        SceAtSetParent(0xB, r227_work.p->elv, 0);
        SceAtSetParent(0xC, r227_work.p->elv, 0);
        SceAtSetParent(0xD, r227_work.p->elv, 0);
        SceAtSetEnable(0xA, 1);
        SceAtSetEnable(0xB, 1);
        SceAtSetEnable(0xC, 1);
        SceAtSetEnable(0xD, 0);
        r227_moveElv(0.0f);
        SceAtSetEnable(0xF, 0);
        SceAtSetEnable(0x14, 0);
        SceAtSetParent(0xE, r227_work.p->elv, 0);
        SceAtSetParent(3, r227_work.p->elv, 0);
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r227_operateElv, 0, 1);
        if (getRoomEtcRack(5, &r227_work.p->rack[0], 1)) {
            if (RsfCheck(G_ROOM_ID, 1) == 0) {
                ((cEmRack*) r227_work.p->rack[0])->setRange(0.0f, 0.0f, 3000.0f, 0.0f);
                SceExec(0x12, (TaskFunc) r227_checkBox0Fall, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        if (getRoomEtcRack(9, &r227_work.p->rack[1], 1)) {
            if (RsfCheck(G_ROOM_ID, 2) == 0) {
                ((cEmRack*) r227_work.p->rack[1])->setRange(0.0f, 0.0f, 0.0f, 3000.0f);
                SceExec(0x12, (TaskFunc) r227_checkBox1Fall, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
        for (i = 0; i < 3; i++) {
            if (getRoomEtcBox(r227_boxNo[i], &r227_work.p->box[i], 1)) {
                r227_work.p->boxY0[i] = r227_work.p->box[i]->pos.y;
            }
        }
        r227_initEmFall();
    }
}

// The enemies of the far side when the room is re-entered after their event.
static void r227_setEm3_after()
{
    SceSleep(1);
    cEmWrap e0;
    cEmWrap e1;
    cEmWrap e2;
    cEmWrap e3;
    cEmWrap e4;
    cEmWrap e5;
    cEmWrap e6;
    e0.setPtr(0xF0, -1, 0);
    e1.setPtr(0xF1, -1, 0);
    e2.setPtr(0xF2, -1, 0);
    e3.setPtr(0xF8, -1, 0);
    e4.setPtr(0xF9, -1, 0);
    e5.setPtr(0xF7, -1, 0);
    e6.setPtr(0xF5, -1, 0);
    e0.setGoto(&pPL->pos, 0xB);
    e0.setCharacter(2);
    e1.setGoto(&pPL->pos, 0xB);
    e1.setCharacter(2);
    e2.setGoto(&pPL->pos, 0xB);
    e2.setCharacter(2);
    e3.setGoto(&pPL->pos, 0xB);
    e3.setCharacter(2);
    e4.setGoto(&pPL->pos, 0xB);
    e4.setCharacter(2);
    e5.setGoto(&pPL->pos, 0xB);
    e5.setCharacter(2);
    e6.setGoto(&pPL->pos, 0xB);
    e6.setCharacter(2);
}

// The far side enemies with their camera cut.
static void r227_setEm3()
{
    RsfSet(G_ROOM_ID, 8);
    cEmWrap e0;
    cEmWrap e1;
    cEmWrap e2;
    cEmWrap e3;
    cEmWrap e4;
    cEmWrap e5;
    cEmWrap e6;
    e0.setEm(0xF0, -1, 0, 1, 1);
    e1.setEm(0xF1, -1, 0, 1, 1);
    e2.setEm(0xF2, -1, 0, 1, 1);
    e3.setEm(0xF8, -1, 0, 1, 1);
    e4.setEm(0xF9, -1, 0, 1, 1);
    e5.setEm(0xF7, -1, 0, 1, 1);
    e6.setEm(0xF5, -1, 0, 1, 1);
    e1.setGoto(&pPL->pos, 0xB);
    e1.setCharacter(2);
    e2.setGoto(&pPL->pos, 0xB);
    e2.setCharacter(2);
    SceEventStart(1);
    e0.setNoSuspend(1);
    e1.setNoSuspend(1);
    e2.setNoSuspend(1);
    e3.setNoSuspend(1);
    e4.setNoSuspend(1);
    e5.setNoSuspend(1);
    e6.setNoSuspend(1);
    CamCtrl.CutCall(5);
    while (CamCtrl.IsMotionEnd() == 0) {
        if (Key.trg & 0x20000000ULL) {
            FadeSetW(0x80000000, 10, 0, 0);
            SubScreenWait(20);
            break;
        }
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    e0.setNoSuspend(0);
    e1.setNoSuspend(0);
    e2.setNoSuspend(0);
    e3.setNoSuspend(0);
    e4.setNoSuspend(0);
    e5.setNoSuspend(0);
    e6.setNoSuspend(0);
    SceEventEnd(0);
    SceSleep(450);
    e3.setGoto(&pPL->pos, 0xB);
    e3.setCharacter(2);
    e4.setGoto(&pPL->pos, 0xB);
    e4.setCharacter(2);
    SceSleep(450);
    e5.setGoto(&pPL->pos, 0xB);
    e5.setCharacter(2);
    SceSleep(450);
    e6.setGoto(&pPL->pos, 0xB);
    e6.setCharacter(2);
    SceSleep(450);
}

// The lever-side enemies with their camera cuts.
static void r227_setEm2()
{
    int skip = 0;

    RsfSet(G_ROOM_ID, 7);
    setEm(0xE9, -1, 1, 1, 1);
    setEm(0xEF, -1, 1, 1, 1);
    cEmWrap e0;
    cEmWrap e1;
    cEmWrap e2;
    e0.setEm(0xED, -1, 1, 1, 1);
    e1.setEm(0xE6, -1, 1, 1, 1);
    e2.setEm(0xE7, -1, 1, 1, 1);
    if (e0.getPtr() && r227_work.p->sw) {
        ((cEmGanado*) e0.getPtr())->setSwitch(r227_work.p->sw);
        SceEventStart(1);
        StaFlagOff(pG, STA_SUSPEND);
        e1.setTrans(0);
        e2.setTrans(0);
        CamCtrl.CutCall(6);
        e0.setFlag(1);
        while (CamCtrl.IsMotionEnd() == 0) {
            if (Key.trg & 0x20000000ULL) {
                skip = 1;
                break;
            }
            SceSleep(1);
        }
        if (skip == 0) {
            CamCtrl.CutCall(7);
            while (CamCtrl.IsMotionEnd() == 0) {
                if (Key.trg & 0x20000000ULL) {
                    skip = 1;
                    break;
                }
                SceSleep(1);
            }
        }
        if (skip == 1) {
            FadeSetW(0x80000000, 10, 0, 0);
            SubScreenWait(20);
        }
        CamCtrl.Comeback(0);
        e1.setTrans(1);
        e2.setTrans(1);
        SceEventEnd(0);
    }
}

// The room's enemies after the event: module 0x14 pre-read; area 4 = the lever-side group until Room_flg
// bit 7, area 9 = the far-side group until bit 8 (else the far side is set at once).
void r227_setEm1()
{
    EmReadSearch(0x14, 0, 0);
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r227_setEm2, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r227_setEm3, 0, 1);
    } else {
        SceExec(0x12, (TaskFunc) r227_setEm3_after, 0, 0, SCE_PRIO_DEF_2, 0);
    }
}

// The gondola (object 0xA2): a cSceObj move1 of 6850 up over 140 frames with 20 % accel / decel and a
// shake; areas 5/6 = ride each way; arriving from r228 (or by jump) it starts at the top (reverse).
void r227_initGondola()
{
    cObj* obj;

    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r227_execGondola, 0, 1);
    SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r227_execGondola, (void*) 1, 1);
    obj = SmdGetObjPtr(0xA2);
    Vec d = {0.0f, 6850.0f, 0.0f};
    r227_work.p->gondola.initMove1_pos(obj, 0x8C, &d, 20.0f, 20.0f);
    r227_work.p->gondola.setVibration(10, 10, 2.0f, 0.5f, 2.0f);
    BitOn(obj->be_flag, 0x20);
    if (pG->room_id_prev == 0x228 || (SysFlagChk(pG, SYS_LOAD_GAME))) {
        r227_work.p->gondola.setReverse(1);
    }
}

// The gondola ride (dir 0: away from the entrance).
static void r227_execGondola(int dir)
{
    SceEventStart(0);
    if (dir == 0) {
        r227_work.p->gondola.setReverse(0);
        CamCtrl.CutCall(3);
    } else {
        r227_work.p->gondola.setReverse(1);
        CamCtrl.CutCall(4);
    }
    {
        cSceObj* g = &r227_work.p->gondola;
        cPlayer* pl = pPL;
        u32 n;

        if (pl) {
            for (n = 0; n < 4; n++) {
                if (g->sub[n] == NULL) {
                    g->sub[n] = pl;
                    break;
                }
            }
        }
    }
    pPL->setNoSuspend(1);
    {
        Vec rot;

        f32 ry = -1.57f;

        rot.x = 0.0f;
        rot.z = 0.0f;
        rot.y = ry;
        pPL->setAng(&rot);
    }
    SndCall(6, 0, 0, 0, 0, 0);
    while (r227_work.p->gondola.move() != 0) {
        SceSleep(1);
    }
    SndCall(6, 1, 0, 0, 0, 0);
    SndRoomStrStop(5);
    RsfSet(G_ROOM_ID, 6);
    pPL->setNoSuspend(0);
    {
        cSceObj* g = &r227_work.p->gondola;
        cPlayer* pl = pPL;
        u32 n;

        if (pl) {
            for (n = 0; n < 4; n++) {
                if (g->sub[n] == pl) {
                    g->sub[n] = 0;
                    break;
                }
            }
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Waits for the running event (r101 idiom: the manager pointer is a loop local, hoisted by loop.c
// into a pseudo that the later loops copy).
static inline void r227_waitEvt()
{
    for (;;) {
        EventMgr* em = &EvtMgr;
        u32* key = &em->NowExeEvtKey;

        if (em->IsAliveEvt(key, 0, 0) == 0) {
            break;
        }
        SceSleep(1);
    }
}

// The entrance event (s00, then s01 or s02 by the flag the coin toss set).
static void r227_execEvent00()
{
    RsfSet(G_ROOM_ID, 0);
    ScfFlagOn(pG, SCF_84);
    SceEventStart(0);
    SceSleep(1);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    if (SmdGetObjPtr(0x9A)) {
        SmdGetObjPtr(0x9A)->be_flag &= ~2;
    }
    if (SmdGetObjPtr(0x9B)) {
        SmdGetObjPtr(0x9B)->be_flag &= ~2;
    }
    if (r227_work.p->evd[0]->waitLoadOk() != 0) {
        u32 key0;

        EvtMgr.SetEvt(r227_work.p->evd[0]->m_addr, &key0);
        ((Event*) key0)->StatusFlag |= 0x800;
        r227_waitEvt();
        SysFlagOn(pG, SYS_SCREEN_STOP);
        r227_work.p->evd[0]->setCommand(CMND_DEL_DATA, 0, 0);
        if (pG->Room_flg[0] & 0x80000000) {
            if (r227_work.p->evd[1]->waitLoadOk() != 0) {
                u32 key1;

                r227_work.p->evd[1]->setCommand(CMND_MRAM_LOAD, 0, 1);
                if (EvtMgr.SetEvt(r227_work.p->evd[1]->m_addr, &key1)) {
                    ((Event*) key1)->StatusFlag |= 0x800;
                }
                r227_waitEvt();
            }
        } else {
            if (r227_work.p->evd[2]->waitLoadOk() != 0) {
                u32 key2;

                r227_work.p->evd[2]->setCommand(CMND_MRAM_LOAD, 0, 1);
                if (EvtMgr.SetEvt(r227_work.p->evd[2]->m_addr, &key2)) {
                    ((Event*) key2)->StatusFlag |= 0x100000;
                    ((Event*) key2)->StatusFlag |= 0x200;
                }
                r227_waitEvt();
                SceExit();
            }
        }
    }
    if (SmdGetObjPtr(0x9A)) {
        SmdGetObjPtr(0x9A)->be_flag |= 2;
    }
    if (SmdGetObjPtr(0x9B)) {
        SmdGetObjPtr(0x9B)->be_flag |= 2;
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    r227_work.p->evd[1]->setCommand(CMND_DEL_DATA, 0, 0);
    r227_work.p->evd[2]->setCommand(CMND_DEL_DATA, 0, 0);
    SceEventEnd(0);
    r227_setEm1();
    SndBgmTblSet(0x227, 1);
    GameSaveSave(&GameSave, pSaveData, -1);
}

// QTE success callback of the entrance event: Room_flg[0] bit 31.
static void r227_succeedAction()
{
    pG->Room_flg[0] |= 0x80000000;
}

// Event r227s00 callback (the entrance): status 3, cancel cut 10; cut 0 light masks on evm5100 /
// evmd900; cut 0xB starts the action-button QTE (0x25, variant by the coin toss) whose success sets
// Room_flg[0] bit 31; later cuts set the models' flags.
static void Evt_R227S00_Func(Event* e)
{
    int v;

    switch (e->funcMode) {
    case 0:
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = 10;
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "evm5100", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 0x20;
                }
                if (e->GetMod(&mod, "evmd900", 0, 0) == 1) {
                    ((cModel*) mod)->LightInfo.EnableMask = 0x10;
                }
            }
            SmdSetTrans(0xA, 0);
            break;
        case 0xB:
            SpfFlagOff(pG, SPF_ACTBTN);
            if (!(pG->Room_flg[0] & 0x80000000)) {
                if (e->NowFrame > 15) {
                    DpfFlagOff(pG, DPF_MESSAGE);
                    if (!(pG->Room_flg[0] & 0x40000000)) {
                        ActBtn.set(0x25, 5, (int) r227_succeedAction, 0, 0x42, 4, 0, 0);
                    } else {
                        ActBtn.set(0x25, 5, (int) r227_succeedAction, 0, 0x42, 3, 0, 0);
                    }
                }
            } else {
                e->CancelSet();
            }
            break;
        }
        if (e->NowCut == 0xB) {
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "evm3300", 0, 0) == 1) {
                    void* bin;

                    if (EvtMgr.GetBin(&bin, "event/model/evm3300/evm330a.tpl", 0) == 1) {
                        ((cModel*) mod)->pModelInfo->setTplAddr(bin);
                    }
                }
            }
        } else {
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "evm3300", 0, 0) == 1) {
                    void* bin;

                    if (EvtMgr.GetBin(&bin, "event/model/evm3300/evm3300.tpl", 0) == 1) {
                        ((cModel*) mod)->pModelInfo->setTplAddr(bin);
                    }
                }
            }
        }
        if (e->NowCut == 6 || e->NowCut == 0xB) {
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
        } else {
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 1);
                    ModelInfoSetTrans((cModel*) mod, 6, 1);
                }
            }
        }
        break;
    case 2:
        break;
    case 3:
        v = 1;
        if (!(e->StatusFlag & 0x4000)) {
            v = 0;
        }
        if (v == 0) {
            EvtMgr.EvtSndStrPlay(evtKey(&EvtMgr), 1, 0x88, 1, 0.0f);
        }
        break;
    }
}

// Event r227s01 callback (the QTE passed): cut 0 swaps scroll objects 0xA -> 0xF8 and hands 0xF8
// (scr0000) to the event; per-cut model flags; the end restores the objects.
static void Evt_R227S01_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    void* mod;

    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod2;

                SmdSetTrans(0xA, 0);
                SmdSetTrans(0xF8, 1);
                mod = SmdGetObjPtr(0xF8);
                if (mod) {
                    e->SetMod("scr0000", mod, 5, 0, 2, 0);
                    e->EspSetModelPtr((cModel*) mod);
                }
                if (e->GetMod(&mod2, "evm5100", 0, 0) == 1) {
                    ((cModel*) mod2)->LightInfo.EnableMask = 0x20;
                }
            }
            break;
        case 5:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "scr0000", 0, 0) == 1) {
                    e->SetMod("scr0000", mod, 5, 0, 2, 0);
                    ((cModel*) mod)->setPos(&pos);
                    ((cModel*) mod)->setAng(&rot);
                    ((cModel*) mod)->be_flag |= 0x20;
                }
            }
            break;
        }
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                void* mod3;

                if (e->GetMod(&mod3, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod3, 2, 0);
                    ModelInfoSetTrans((cModel*) mod3, 6, 0);
                }
            }
        } else {
            if (e->NowFrame == 0) {
                void* mod3;

                if (e->GetMod(&mod3, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod3, 2, 1);
                    ModelInfoSetTrans((cModel*) mod3, 6, 1);
                }
            }
        }
        break;
    case 2: {
        SmdWork* w;

        SmdSetTrans(0xA, 1);
        SmdSetTrans(0xF8, 0);
        w = SmdGetWorkPtr(0xA);
        mod = SmdGetObjPtr(0xA);
        if (mod && w) {
            ((cModel*) mod)->setPos(&w->pos);
            ((cModel*) mod)->setAng(&w->rot);
        }
        break;
    }
    }
}

// Event r227s02 callback (the QTE failed, Leon dies): Leon's parts 2/6 hidden on cuts 0/1; the end
// (StatusFlag 0x4000 clear) runs the death demo.
static void Evt_R227S02_Func(Event* e)
{
    void* mod;

    if (e->funcMode == 1) {
        // Two identical arms (not `case 0: case 1:`): the original keeps the `== 0` / `== 1` tests
        // and cross-jumps the first body into the second.
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        case 1:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 0);
                    ModelInfoSetTrans((cModel*) mod, 6, 0);
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod, 2, 1);
                    ModelInfoSetTrans((cModel*) mod, 6, 1);
                }
            }
            break;
        }
    }
}
