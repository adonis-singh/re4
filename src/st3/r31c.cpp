#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "em32.h"
#include "em39.h"
#include "emhit.h"
#include "emswitch.h"
#include "emdoor.h"
#include "emrack.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "cockpit.h"
#include "act_btn.h"
#include "mes.h"
#include "sscrn.h"
#include "item.h"
#include "item_model.h"
#include "read.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "motion.h"
#include "math_sub.h"
#include "rnd.h"
#include "scheduler.h"
#include "room_data.h"
#include "eprintf.h"

// local copy: a header definition changes other units' allocation (its 0.0f pool labels, see model.h)
static inline void SetAngY(cModel* m, f32 y) { Vec v; v.x = 0.0f; v.y = y; v.z = 0.0f; m->setAng(&v); }

// Room 3-1c (D:/Bio4/Prog/r31c.cpp): the ruins with the three crest doors, Krauser's two
// battles, the timer doors and the tower that explodes.

// A collision post the player can shoot down (0x50 bytes; eight of them at work+0x30).
class cR31CPost {
public:
    u16 mode;        // 0x00  0 dmg_ck, 1 die (index into the member table)
    u16 step;        // 0x02
    cObj* obj;       // 0x04
    cEmHit* hit;     // 0x08
    YARARE_INFO box;   // 0x0C  the second yarare box (YarareAdd)
    cSat* eat;       // 0x40
    int type;        // 0x44  1 (posts 0..4), 0 (5, 6), 2 (7)
    int dmgType;     // 0x48  4 heavy weapon, 3 the rest
    u16 no;          // 0x4C
    s16 hp;          // 0x4E

    void init(u32 no);
    void move();
    void atari_set();
    void dmg_ck();
    void die();
};

// One of the nine sliding doors (0x38 bytes): a scroll object moved along one axis by `dist`,
// with its collision pieces and the pG->Key_flg bit it clears / sets.
class cR31CDoor {
public:
    u8 mode;         // 0x00  0 wait, 1 open, 2 close (index into the member table)
    u8 step;         // 0x01  routine step
    u16 objId;       // 0x02
    cObj* obj;       // 0x04
    Vec pos;         // 0x08  closed position
    f32 dist;        // 0x14  travel (signed, along the door's axis)
    f32 spd;         // 0x18  closing speed
    u32 hSnd;        // 0x1C  running RoomSeCall handle
    cSat* sat;       // 0x20
    cSat* eat;       // 0x24
    int init_;       // 0x28  1 once init found the object
    int status;      // 0x2C  0 closed, 1 open, 2 moving (getStatus)
    int timer;       // 0x30  close: shake frames, then the settle wait
    int unlockNo;    // 0x34  pG->Key_flg bit set when opened (0: none)

    void init(u32 id);
    void move();
    void wait();
    void open();
    void close();
    void setOpen();
    void setClose();
    int getStatus();
    void setOpened();
    void setClosed();
};

// The count down of the timer door (Cckpt.countDown with the room's own state).
class cR31CCountDown {
public:
    int running;     // 0x00
    int state;       // 0x04  1 running, 2 paused, 0 stopped

    void countStart();
    void countEnd();
    void setPause(int on);
    void setDisp(int on);
    int isTimeOut();
};

struct R31cWork {
    cObj* crest[3];         // 0x000  the three crest objects on the door
    Vec crestOfs[3];        // 0x00C  their offsets from the door object
    cR31CPost post[8];      // 0x030
    cEmWrap krauser;        // 0x2B0  Krauser of the knife fight (list 0x19)
    cEmWrap krauser2;       // 0x2BC  Krauser of the second battle (list 0x13)
    cEmDoor* door8;             // 0x2C8  etc door 8 (the battle arena door)
    cEm* rack;              // 0x2CC  etc rack 0x10
    ScePrim* talkTask;      // 0x2D0  the running r31c_TalktoKrauser task
    cR31CDoor door[9];      // 0x2D4  ids 0x78 0x79 0x7C 0x7B 0x7D 0x7E 0x7F 0x80 0x6D
    cR31CCountDown countDown;  // 0x4CC
    cEm* sw[2];             // 0x4D4  etc switches 0x11 / 0x12
    cEmWrap seeker[15];     // 0x4DC  the Novistadors (lists 0x12 0x1D 0x23 0x21 0x15 0x16 0x22 0x25 0x26 0x18 0x20 0x1B 0x1E ..)
    cSat* towerSat;         // 0x590
    cSat* towerEat;         // 0x594
    u32 hSnd;               // 0x598  running RoomSeCall / SndStrReq handle of the events
};


static int r31c_mesNo;          // .bss 0x1C  the s01 event's action button message (Rnd)
static R31cWork* r31c_work;   // .bss 0x20





// The same through a caller's Vec (R31cInit reuses its `pos`).
static inline void SetPosAngYV(cModel* m, Vec* v, f32 x, f32 y, f32 z, f32 ry)
{
    v->x = x;
    v->y = y;
    v->z = z;
    m->setPos(v);
    v->x = 0.0f;
    v->y = ry;
    v->z = 0.0f;
    m->setAng(v);
}

// Position and yaw in one go (every angle the room sets is (0, y, 0)).
static inline void SetPosAngY(cModel* m, f32 x, f32 y, f32 z, f32 ry)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
    v.x = 0.0f;
    v.y = ry;
    v.z = 0.0f;
    m->setAng(&v);
}

// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");

static void r31c_CrestUseCheck();
void r31c_SetCrest(u32 no);
static void r31c_DoorCheck();
static void r31c_CrestDoorOpen();
static void r31c_CrestDoorOpenEndProc();
static void r31c_SeekerSet(u32 no);
static void r31c_SeekerFirstSet();
static void r31c_SeekerAppearCut(int no);
static void r31c_SeekerAppearCutEndProc(int no);
static void r31c_CheckTalkToKrauser(int no);
static void r31c_TalktoKrauser(int no);
void r31c_TalktoKrauserEndProc(int no);
static void r31c_TalkToKrauserActBtnSet();
static void r31c_KrauserDieCheck(cEm39* em);
static void r31c_KrauserDieCheckEndProc(cEm39* em);
static void r31c_GetSnakeCrest();
static void r31c_GetSnakeCrestEndProc();
static void r31c_Krauser1stBattle();
static void r31c_TimerDoorCountDown();
void r31c_TimerDoorCountDownEndProc();
static void r31c_TimerDoorCancelCheck();
void r31c_TimerDoorCancel();
static void r31c_Krauser2ndBattle();
static void r31c_Krauser2ndBattleEndProc();
static void r31c_SwitchPushCheck();
static void r31c_SwitchPushCheckEndProc();
static void r31c_LeverCheck();
static void r31c_LeverOperate(int no);
static void r31c_LeverOperateEndProc(int no);
static void r31c_CountDownEnd();
static void r31c_CountDownThread();
static void r31c_TowerEntranceClose();
static void r31c_TowerEntranceCloseEndProc();
static void r31c_TowerExplode();
static void r31c_TowerExplodeEndProc();
static void r31c_TowerCoverClose();
static void r31c_TowerCoverCloseEndProc();
static void r31c_BombCutSet(cEm39* em);
static void r31c_BombCutSetEndProc();
void r31c_TowerExplodeModelSet(int no, int on);
static void r31c_SetChapterEnd();
static void r31c_SetContinuePoint(int no);
static void r31cEventS00();
static void r31cEventS01();
void r31cEventS01EndProc();
static void r31cEventS02();
static void r31cEventS02EndProc();
static void Evt_R31CS00_Func(Event* e);
static void r31c_EventS01Act();
static void Evt_R31CS01_Func(Event* e);
static void Evt_R31CS02_Func(Event* e);
static void r31c_KrauserCorpseMes();

// Room init (the ruins, Krauser's arena): the nine sliding doors (0x78 the crest door, 0x79 open,
// 0x7B..0x80, 0x6D) and the eight shootable posts; the crest door state (the crests already set, the
// crest-use watcher), the timer door and the tower per the room flags; the Novistador groups per
// area, Krauser's talks / battles, the levers, the count-down, the s00/s01/s02 callbacks and their
// areas; the continue points.
void R31cInit()
{
    Vec zero = {0, 0, 0};
    Vec pos;
    void* bin;
    void* tpl;
    u32 i;

#line 69 "D:/Bio4/Prog/r31c.cpp"
    r31c_work = (R31cWork*) MEM_CALLOC(sizeof(R31cWork), 1, 0xd);
    r31c_work->door[0].init(0x78);
    r31c_work->door[1].init(0x79);
    r31c_work->door[1].setOpened();
    r31c_work->door[2].init(0x7C);
    r31c_work->door[3].init(0x7B);
    r31c_work->door[3].setOpened();
    r31c_work->door[4].init(0x7D);
    r31c_work->door[5].init(0x7E);
    r31c_work->door[6].init(0x7F);
    r31c_work->door[7].init(0x80);
    r31c_work->door[8].init(0x6D);
    r31c_work->door[8].setOpened();
    if (getRoomEtcRack(0x10, &r31c_work->rack, 1)) {
        ((cEmRack*) r31c_work->rack)->setRange(0.0f, 0.0f, 9000.0f, 5000.0f);
    }
    if (RsfCheck(G_ROOM_ID, 0x17) == 0) {
        SmdGetObjPtr(0x6A)->pos.y = 2000.0f;
        r31c_work->rack->pos.y = 2000.0f;
    } else {
        r31c_work->rack->pos.x = 31157.0f;
        r31c_work->rack->pos.y = 5200.0f;
        r31c_work->rack->pos.z = -8384.0f;
    }
    r31c_work->towerSat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 7);
    SceAtDataSet_exec(0x19, 0x12, 0, (TaskFunc) r31c_TowerEntranceClose, 0, 1);
    if (ScfFlagChk(pG, SCF_R31C_TOWER_EXPLODE)) {
        r31c_TowerExplodeModelSet(0, 0);
        r31c_TowerExplodeModelSet(1, 1);
        SceAtSetEnable(0x87, 0);
        SceAtSetEnable(0x8F, 0);
        r31c_work->towerEat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 5);
    } else {
        r31c_TowerExplodeModelSet(0, 1);
        r31c_TowerExplodeModelSet(1, 0);
        r31c_work->towerEat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 4);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x1B, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    }
    if (getRoomEtcSwitch(0x11, &r31c_work->sw[0], 1)) {
        ((cEmSwitch*) r31c_work->sw[0])->setLongCk();
    }
    if (getRoomEtcSwitch(0x12, &r31c_work->sw[1], 1)) {
        ((cEmSwitch*) r31c_work->sw[1])->setLongCk();
    }
    SceExec(0x12, (TaskFunc) r31c_LeverCheck, 0, 0, 2, 0);
    EvtMgr.SetFunc("evt_r31cs00_func", (void*) Evt_R31CS00_Func);
    EvtMgr.SetFunc("evt_r31cs01_func", (void*) Evt_R31CS01_Func);
    EvtMgr.SetFunc("evt_r31cs02_func", (void*) Evt_R31CS02_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(7, 0x12, 0, (TaskFunc) r31cEventS00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r31cs00.evd", (u8) GetEmIdFromList(0x19), 0, 0, 0);
        SceExec(0x12, (TaskFunc) r31c_TimerDoorCancelCheck, 0, 0, 2, 0);
    } else if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(0x14, 0x12, 0, (TaskFunc) r31cEventS02, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r31cs02.evd", (u8) GetEmIdFromList(0x19), 0, 0, 0);
        SndRoomStrStart(1, 0, 1);
    } else if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(0x81, 0x12, 0, (TaskFunc) r31cEventS01, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r31cs01.evd", (u8) GetEmIdFromList(0x19), 0, 0, 0);
        SndRoomStrStart(1, 0, 1);
    }
    Vec rot = {0.0f, 0.0f, 0.0f};
    if (ItemGetBinTplAddr(0x85, &bin, &tpl) == 1) {
        pos.x = -14604.0f;
        pos.y = 1249.0f;
        pos.z = -20437.0f;
        r31c_work->crest[0] = SetObjSmd(bin, tpl, &pos, &rot, 0x10, 1);
        PSVECSubtract(&r31c_work->crest[0]->pos, &SmdGetObjPtr(0x78)->pos, &r31c_work->crestOfs[0]);
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            r31c_work->crest[0]->be_flag &= ~2;
        }
    }
    if (ItemGetBinTplAddr(0x86, &bin, &tpl) == 1) {
        pos.x = -14240.0f;
        pos.y = 1106.0f;
        pos.z = -20437.0f;
        r31c_work->crest[1] = SetObjSmd(bin, tpl, &pos, &rot, 0x10, 1);
        PSVECSubtract(&r31c_work->crest[1]->pos, &SmdGetObjPtr(0x78)->pos, &r31c_work->crestOfs[1]);
        if (RsfCheck(G_ROOM_ID, 4) == 0) {
            r31c_work->crest[1]->be_flag &= ~2;
        }
    }
    if (ItemGetBinTplAddr(0x87, &bin, &tpl) == 1) {
        pos.x = -13954.0f;
        pos.y = 1460.0f;
        pos.z = -20437.0f;
        r31c_work->crest[2] = SetObjSmd(bin, tpl, &pos, &rot, 0x10, 1);
        PSVECSubtract(&r31c_work->crest[2]->pos, &SmdGetObjPtr(0x78)->pos, &r31c_work->crestOfs[2]);
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            r31c_work->crest[2]->be_flag &= ~2;
        }
    }
    if (KyfFlagChk(pG, KYF_R31C_TO_R320_DOOR) == 0) {
        SceExec(0x12, (TaskFunc) r31c_CrestUseCheck, 0, 0, 2, 0);
        SceAtSetEnable(0, 0);
        SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) r31c_DoorCheck, 0, 1);
    } else {
        r31c_work->door[0].setOpened();
    }
    EmReadSearch(0x3A, 0, 0);
    SceAtDataSet_exec(0xB, 0x12, 0, (TaskFunc) r31c_SeekerSet, (void*) 0xB, 1);
    SceAtDataSet_exec(0xC, 0x12, 0, (TaskFunc) r31c_SeekerSet, (void*) 0xC, 1);
    SceAtDataSet_exec(0x15, 0x12, 0, (TaskFunc) r31c_SeekerSet, (void*) 0x15, 1);
    SceAtDataSet_exec(0x16, 0x12, 0, (TaskFunc) r31c_SeekerSet, (void*) 0x16, 1);
    SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r31c_SeekerSet, (void*) 6, 1);
    for (i = 0; i < 8; i++) {
        r31c_work->post[i].init(i);
    }
    SmdSetTrans(0xB5, 0);
    if (RsfCheck(G_ROOM_ID, 0x11) == 0) {
        cEm39* krauser;

        r31c_work->krauser.setEm(0x19, 7, 0, 1, 0);
        krauser = (cEm39*) r31c_work->krauser.getPtr();
        if (RsfCheck(G_ROOM_ID, 0xF)) {
            krauser->set1stDoorClear();
        }
        if (RsfCheck(G_ROOM_ID, 0x10)) {
            krauser->set2ndDoorClear();
        }
    } else {
        r31c_work->door[8].setClosed();
        if (RsfCheck(G_ROOM_ID, 0x18)) {
            r31c_work->door[7].setOpened();
            SmdSetTrans(0xB5, 1);
            SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 9);
            EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 6);
        }
    }
    getRoomEtcDoor(8, &r31c_work->door8, 1);
    if (r31c_work->door8 && RsfCheck(G_ROOM_ID, 0xF) == 0) {
        r31c_work->door8->setCloseLock();
        SceAtDataSet_exec(0x11, 0x12, 0, (TaskFunc) r31c_Krauser1stBattle, 0, 1);
        EstSet(r31c_work->door8, -1, 0, 0, EFF_ROOM, 0x17, 1, ESP_CORE_KIND_ROOM01, 0, 0);
        KyfFlagOff(pG, KYF_ST1_23);
    } else {
        EstSet(r31c_work->door8, -1, 0, 0, EFF_ROOM, 0x18, 1, ESP_CORE_KIND_NONE, 0, 0);
        KyfFlagOn(pG, KYF_ST1_23);
    }
    SceExec(0x12, (TaskFunc) r31c_SeekerFirstSet, 0, 0, 2, 0);
    if (RsfCheck(G_ROOM_ID, 0x14) == 0) {
        SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) r31c_SetChapterEnd, 0, 1);
    }
    SceAtDataSet_exec(0x1B, 0x12, 0, (TaskFunc) r31c_CheckTalkToKrauser, (void*) 1, 1);
    if (DebugTrg(1) == 1) {
        SceAtDataSet_exec(0x81, 0x12, 0, (TaskFunc) r31cEventS01, 0, 1);
        SetPosAngYV(pPL, &pos, -5971.0f, 12000.0f, -14945.0f, 2.86f);
        SceKill((void (*)(int)) r31c_TimerDoorCancelCheck);
    }
}

// Per frame: step the eight posts and the nine doors.
void R31cMain()
{
    u32 i;

    for (i = 0; i < 8; i++) {
        r31c_work->post[i].move();
    }
    for (i = 0; i < 9; i++) {
        r31c_work->door[i].move();
    }
}

// The three crests: each one placed on the door once the player has it.
static void r31c_CrestUseCheck()
{
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 3) == 0 && ItemMgr.check(0x85) == 1) {
            r31c_SetCrest(0);
        }
        if (RsfCheck(G_ROOM_ID, 4) == 0 && ItemMgr.check(0x86) == 1) {
            r31c_SetCrest(1);
        }
        if (RsfCheck(G_ROOM_ID, 5) == 0 && ItemMgr.check(0x87) == 1) {
            r31c_SetCrest(2);
        }
        if (RsfCheck(G_ROOM_ID, 3) && RsfCheck(G_ROOM_ID, 4) && RsfCheck(G_ROOM_ID, 5)) {
            SceExec(0x12, (TaskFunc) r31c_CrestDoorOpen, 0, 0, 2, 0);
            SceExit();
        }
        SceSleep(1);
    }
}

// A crest set into the door (cut 0x17).
void r31c_SetCrest(u32 no)
{
    SceEventStart(1);
    switch (no) {
    case 0:
        RsfSet(G_ROOM_ID, 3);
        break;
    case 1:
        RsfSet(G_ROOM_ID, 4);
        break;
    case 2:
        RsfSet(G_ROOM_ID, 5);
        break;
    }
    CamCtrl.CutCall(0x17);
    SceSleep(15);
    r31c_work->crest[no]->be_flag |= 2;
    RoomSeCall(9, 0, 0, 0, 0);
    SceSleep(30);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The crest door examined: the up-cut shows the crests set so far, then the item screen.
static void r31c_DoorCheck()
{
    u8 n = 0;

    if (RsfCheck(G_ROOM_ID, 3)) {
        n++;
    }
    if (RsfCheck(G_ROOM_ID, 4)) {
        n++;
    }
    if (RsfCheck(G_ROOM_ID, 5)) {
        n++;
    }
    if (n) {
        SceUpCut(1, 0x17, -1, 4);
    } else {
        SceUpCut(0, 0x17, -1, 4);
    }
    if (ItemMgr.num(0x85) || ItemMgr.num(0x86) || ItemMgr.num(0x87)) {
        SubScreenOpen(0x80, 1);
    } else {
        CamCtrl.Comeback(0);
    }
}

// All three crests set: Key_flg[0] 0x200, Scenario_flg[2] 0x1000, area 0 on / 0x10 off, camera cut
// 0x18 while the crest door (door[0]) slides open with its effect; player-cancellable.
static void r31c_CrestDoorOpen()
{
    void* model = NULL;

    KyfFlagOn(pG, KYF_R31C_TO_R320_DOOR);
    ScfFlagOn(pG, SCF_R31C_OPEN_DOOR);
    SceAtSetEnable(0, 1);
    SceAtSetEnable(0x10, 0);
    SceEventStart(1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x19, 1, ESP_CORE_KIND_ROOM00, 0, model);
    r31c_work->door[0].setOpen();
    CamCtrl.CutCall(0x18);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_CrestDoorOpenEndProc, 0, 0, 1);
    while (r31c_work->door[0].getStatus() != 1) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_CrestDoorOpenEndProc();
}

// End of the crest door opening (also its cancel path): the door snapped open, effect dropped, camera back, SceEventEnd.
static void r31c_CrestDoorOpenEndProc()
{
    if (pG->Room_flg[0] & 0x80000000) {
        r31c_work->door[0].setOpened();
        EffectDelete(1, ESP_CORE_KIND_ROOM00);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// A Novistador group set by its area (the area number is the task argument).
static void r31c_SeekerSet(u32 no)
{
    cEmWrap em;

    SceAtSetEnable(no, 0);
    switch (no) {
    case 0xB:
        if (r31c_work->seeker[0].setEm(0x12, 7, 0, 1, 0)) {
            r31c_work->seeker[0].setFlag(1);
        }
        if (r31c_work->seeker[1].setEm(0x1D, 7, 0, 1, 0)) {
            r31c_work->seeker[1].setFlag(1);
        }
        SceExec(0x12, (TaskFunc) r31c_SeekerAppearCut, 1, 0, 2, 0);
        break;
    case 0xC:
        if (r31c_work->seeker[2].setEm(0x23, 7, 0, 1, 0)) {
            r31c_work->seeker[2].setFlag(1);
        }
        if (r31c_work->seeker[3].setEm(0x21, 7, 0, 1, 0)) {
            r31c_work->seeker[3].setFlag(1);
        }
        if (r31c_work->seeker[4].setEm(0x15, 7, 0, 1, 0)) {
            r31c_work->seeker[4].setFlag(1);
        }
        SceAtSetEnable(0x15, 0);
        break;
    case 0x15:
        if (r31c_work->seeker[7].setEm(0x25, 7, 0, 1, 0)) {
            r31c_work->seeker[7].setFlag(1);
        }
        if (r31c_work->seeker[8].setEm(0x26, 7, 0, 1, 0)) {
            r31c_work->seeker[8].setFlag(1);
        }
        SceAtSetEnable(0xB, 0);
        break;
    case 0x16:
        if (r31c_work->seeker[9].setEm(0x18, 7, 0, 1, 0)) {
            r31c_work->seeker[9].setFlag(1);
        }
        if (r31c_work->seeker[10].setEm(0x20, 7, 0, 1, 0)) {
            r31c_work->seeker[10].setFlag(1);
        }
        break;
    case 6:
        r31c_work->seeker[0].setEm(0x12, 7, 0, 1, 0);
        r31c_work->seeker[1].setEm(0x1D, 7, 0, 1, 0);
        r31c_work->seeker[9].setEm(0x18, 7, 0, 1, 0);
        break;    // An empty arm (the compare tree tests 0x15 first and sends 0xD..0x14 to the exit).
    case 0xE:
        break;
    }
}

// The first two Novistadors: set once the player stays in area 0xE for 300 frames.
static void r31c_SeekerFirstSet()
{
    u32 cnt = 0;

    if (RsfCheck(G_ROOM_ID, 0x15)) {
        SceExit();
    }
    do {
        if (SceAtHitCheck(0xE)) {
            SceDebugDisp("T[%d]", cnt);
            cnt++;
        } else {
            cnt = 0;
        }
        if (cnt > 300) {
            break;
        }
        SceSleep(1);
    } while (1);
    r31c_work->seeker[11].setEm(0x1B, 7, 0, 1, 1);
    r31c_work->seeker[12].setEm(0x1E, 7, 0, 1, 1);
    SceExec(0x12, (TaskFunc) r31c_SeekerAppearCut, 0, 0, 2, 0);
}

// The Novistador appearance cuts (0: the first pair, 1: the area 0xB pair).
static void r31c_SeekerAppearCut(int no)
{
    if (RsfCheck(G_ROOM_ID, 0x15)) {
        SceExit();
    }
    RsfSet(G_ROOM_ID, 0x15);
    SceEventStart(1);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_SeekerAppearCutEndProc, no, 0, 1);
    if (no == 0) {
        r31c_work->seeker[11].setNoSuspend(1);
        r31c_work->seeker[11].setFlag(1);
        CamCtrl.CutCall(0x1C);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        r31c_work->seeker[12].setNoSuspend(1);
        r31c_work->seeker[12].setFlag(1);
        CamCtrl.CutCall(0x1D);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    } else {
        r31c_work->seeker[0].setNoSuspend(1);
        r31c_work->seeker[0].setFlag(1);
        r31c_work->seeker[1].setNoSuspend(1);
        r31c_work->seeker[1].setFlag(1);
        CamCtrl.CutCall(0x1E);
        RoomSeCall(0x20, 0, 0, 0, 0);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_SeekerAppearCutEndProc(no);
}

// End of a Novistador appearance cut: the pair (slots 11/12 for cut 0, 0/1 for cut 1) released to hunt, camera back.
static void r31c_SeekerAppearCutEndProc(int no)
{
    int a = -1;
    int b = -1;

    if (no == 0) {
        a = 11;
        b = 12;
    } else if (no == 1) {
        a = 0;
        b = 1;
    }
    // The cancel check has nothing to undo; its pG read survives as the `lis pG@ha` of the flag
    // clear below, hoisted above the calls.
    if (pG->Room_flg[0] & 0x80000000) {
    }
    r31c_work->seeker[a].setFlag(1);
    r31c_work->seeker[a].setNoSuspend(0);
    r31c_work->seeker[b].setFlag(1);
    r31c_work->seeker[b].setNoSuspend(0);
    CamCtrl.Comeback(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventEnd(0);
}

// Waits for Krauser to accept the talk (the first or second one), then runs it.
static void r31c_CheckTalkToKrauser(int no)
{
    int (cEm39::*ck)();
    int flagNo;

    if (no == 0) {
        ck = &cEm39::ckTalk1st;
        flagNo = 0x19;
    } else {
        ck = &cEm39::ckTalk2nd;
        flagNo = 0x1A;
    }
    if (RsfCheck(G_ROOM_ID, flagNo)) {
        SceExit();
    }
    if (r31c_work->krauser.isActive()) {
        cEm39* em = (cEm39*) r31c_work->krauser.getPtr();

        while (r31c_work->krauser.isActive()) {
            if ((em->*ck)() == 1) {
                r31c_work->talkTask = SceExec(0x12, (TaskFunc) r31c_TalktoKrauser, no, 0, 2, 0);
                return;
            }
            SceSleep(1);
        }
    }
}

// The talk with Krauser (0: after the first door, 1: after the second).
static void r31c_TalktoKrauser(int no)
{
    cEm39* em = (cEm39*) r31c_work->krauser.getPtr();

    SceSleep(5);
    SceEventStart(0);
    pPL->setNoSuspend(1);
    r31c_work->krauser.setNoSuspend(1);
    if (no == 0) {
        pG->Room_flg[0] |= 0x40;
        em->setPos(29231.0f, 8609.0f, -10844.0f);
        SetPosAngY(pPL, 35137.0f, 5200.0f, -12965.0f, -1.25f);
        em->setTalk1st();
    } else {
        pG->Room_flg[0] |= 0x20;
        SetPosAngY(em, -719.0f, 2054.0f, -5022.0f, -1.94f);
        SetPosAngY(pPL, -5290.0f, -3200.0f, -7903.0f, 1.18f);
        em->setTalk2nd();
    }
    r31c_work->hSnd = RoomSeCall(no == 0 ? 0x16 : 0x18, 0, 0, 0, 0);
    CamCtrl.CutCall(no == 0 ? 0xA : 0xD);
    pG->Room_flg[0] &= ~0x08000000;
    while (SndEndCheck(r31c_work->hSnd) == 0 || CamCtrl.IsMotionEnd() == 0) {
        if ((pG->Room_flg[0] & 0x08000000) == 0) {
            ActBtn.set(ACT_ANSWER, 5, (void*) r31c_TalkToKrauserActBtnSet, 0, ACTCTR_ENFORCE_EXEC | ACTCTR_NO_SUSPEND, DISP_A_NORMAL, ACT_FUNC_SCE, 0);
            SpfFlagOff(pG, SPF_ACTBTN);
        }
        SceSleep(1);
    }
    if ((pG->Room_flg[0] & 0x08000000) == 0) {
        r31c_TalktoKrauserEndProc(no);
        SceExit();
    }
    r31c_work->hSnd = RoomSeCall(no == 0 ? 0x1B : 0x1C, 0, 0, 0, 0);
    CamCtrl.CutCall(no == 0 ? 0xB : 0xE);
    while (SndEndCheck(r31c_work->hSnd) == 0) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r31c_work->hSnd = RoomSeCall(no == 0 ? 0x17 : 0x1A, 0, 0, 0, 0);
    CamCtrl.CutCall(no == 0 ? 0xC : 0xF);
    while (SndEndCheck(r31c_work->hSnd) == 0) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_TalktoKrauserEndProc(no);
}

// End of a Krauser talk (also its cancel path): SE stopped, the player and Krauser may suspend, camera
// back, SceEventEnd, Krauser's talk motion cancelled (talk 1 or 2) into the fight state.
void r31c_TalktoKrauserEndProc(int no)
{
    cEm39* em = (cEm39*) r31c_work->krauser.getPtr();

    if (pG->Room_flg[0] & 0x80000000) {
        SndStop(r31c_work->hSnd, 0);
    }
    pPL->setNoSuspend(0);
    r31c_work->krauser.setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (no == 0) {
        em->setTalk1stCancel();
    } else {
        em->setTalk2ndCancel();
        if (r31c_work->seeker[5].setEm(0x16, 7, 0, 1, 0)) {
            r31c_work->seeker[5].setFlag(1);
        }
        if (r31c_work->seeker[6].setEm(0x22, 7, 0, 1, 0)) {
            r31c_work->seeker[6].setFlag(1);
        }
    }
}

// Action button of the talk: Room_flg[0] 0x08000000 (the player answered).
static void r31c_TalkToKrauserActBtnSet()
{
    pG->Room_flg[0] |= 0x08000000;
}

// Krauser's death in the second battle: the death cut, then the bomb count down.
static void r31c_KrauserDieCheck(cEm39* em)
{
    while (em->hp > 0) {
        SceSleep(1);
    }
    r31c_work->countDown.setPause(1);
    r31c_work->countDown.setDisp(0);
    SceAtSetEnable(0x1C, 0);
    SndRoomStrStop(1);
    SceEventStart(0);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    em->setNoSuspend(1);
    CamCtrl.clearAttachCamera();
    em->setDie();
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_KrauserDieCheckEndProc, (int) em, 0, 1);
    while ((MotionGetState(r31c_work->krauser2.getPtr()) & 4) == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_KrauserDieCheckEndProc(em);
}

// End of Krauser's death cut (also its cancel path): his death motion cancelled, Leon placed facing
// 1.88 rad, camera back, Krauser may suspend, Status_flg[2] 0x02000000 off, the bomb count-down and
// the corpse's crest item set up.
static void r31c_KrauserDieCheckEndProc(cEm39* em)
{
    Vec at[4];
    int zero = 0;
    int flag;

    if (pG->Room_flg[0] & 0x80000000) {
        em->setDieCancel();
    }
    flag = 2;
    SetPosAngY(pPL, 5907.0f, 12000.0f, -14740.0f, 1.88f);
    CamCtrl.Comeback(0);
    em->setNoSuspend(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r31c_GetSnakeCrest, 0, 0, 2, 0);
    SceAtDataSet_exec(0x18, 0x12, 0, (TaskFunc) r31c_CountDownEnd, 0, 1);
    at[0].x = 750.0f;
    at[0].y = 0.0f;
    at[0].z = -750.0f;
    at[1].x = 750.0f;
    at[1].y = 0.0f;
    at[1].z = 750.0f;
    at[2].x = -750.0f;
    at[2].y = 0.0f;
    at[2].z = 750.0f;
    at[3].x = -750.0f;
    at[3].y = 0.0f;
    at[3].z = -750.0f;
    SceAtCreateExecAt(r31c_work->krauser2.getPtr(), at, 1, 8, 1, 1000.0f, 1, 0.0f, 0.0f, 1, 0x12,
                      (TaskFunc) r31c_KrauserCorpseMes, zero, flag);
    r31c_work->countDown.setPause(0);
    r31c_work->countDown.setDisp(1);
}

// The snake crest taken from the corpse: the timer door opens.
static void r31c_GetSnakeCrest()
{
    while (SceAtItemFlgCk(0x82) == 0) {
        SceSleep(1);
    }
    while (SceAtCheckSaveItemId(0x86) == 1) {
        SceSleep(1);
    }
    StaFlagOn(pG, STA_TIMER_NO_PAUSE);
    SceEventStart(1);
    CamCtrl.CutCall(0x15);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_GetSnakeCrestEndProc, 0, 0, 1);
    if (pG->Room_flg[0] & 0x04000000) {
        r31c_work->door[3].setOpen();
        while (r31c_work->door[3].getStatus() != 1) {
            SceSleep(1);
        }
        r31c_work->door[2].setClose();
        while (r31c_work->door[2].getStatus() != 0) {
            SceSleep(1);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    CamCtrl.CutCall(0x1B);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x15, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    r31c_work->door[7].setOpen();
    while (r31c_work->door[7].getStatus() != 1) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_GetSnakeCrestEndProc();
}

// End of the snake crest pickup (also its cancel path): the timer door closed and doors 3/7 opened,
// effect dropped, camera back, Room_flg bit 0x18, Status_flg[2] 0x00020000 off.
static void r31c_GetSnakeCrestEndProc()
{
    Vec zero = {0, 0, 0};

    if (pG->Room_flg[0] & 0x80000000) {
        r31c_work->door[2].setClosed();
        r31c_work->door[3].setOpened();
        r31c_work->door[7].setOpened();
        EffectDelete(1, ESP_CORE_KIND_ROOM00);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    RsfSet(G_ROOM_ID, 0x18);
    StaFlagOff(pG, STA_TIMER_NO_PAUSE);
    SmdSetTrans(0xB5, 1);
    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 9);
    EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 6);
}

// The first battle: the door closes behind the player and the timer starts.
static void r31c_Krauser1stBattle()
{
    SceUpCut(2, 0x19, 0x11, 0);
    if ((pG->Room_flg[0] & 0x40000000) == 0) {
        pG->Room_flg[0] |= 0x40000000;
        SceExec(0x12, (TaskFunc) r31c_TimerDoorCountDown, 0, 0, 2, 0);
    }
}

// The first battle's timer: 2700 frames while Krauser is out; if he hides the door opens early; at
// zero the door cut plays.
static void r31c_TimerDoorCountDown()
{
    int t = 2700;
    cEm39* em = (cEm39*) r31c_work->krauser.getPtr();

    for (;;) {
        SceDebugDisp("T[%d]", t);
        if (t == 0) {
            break;
        }
        if (em->ckHide() == 1) {
            SceSleep(90);
            break;
        }
        t--;
        SceSleep(1);
    }
    while (ActBtn.m_active_flag == 1) {
        SceSleep(1);
    }
    while (PlGetStatus() & 0xC0080) {
        SceSleep(1);
    }
    SceEventStart(1);
    CamCtrl.CutCall(0x11);
    SceSleep(10);
    EffectDelete(1, ESP_CORE_KIND_ROOM01);
    EstSet(r31c_work->door8, -1, 0, 0, EFF_ROOM, 0x18, 1, ESP_CORE_KIND_NONE, 0, 0);
    KyfFlagOn(pG, KYF_ST1_23);
    RoomSeCall(0x12, 0, 0, 0, 0);
    SceMesSet(3, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_TimerDoorCountDownEndProc();
}

// End of the timer door cut: door 8 normal again, camera back, area 0x11 off, Krauser told the first
// door is clear, SceEventEnd, area 0x17 = continue point, Room_flg[0] 0x20000000.
void r31c_TimerDoorCountDownEndProc()
{
    cEm39* em;

    r31c_work->door8->setNormal();
    CamCtrl.Comeback(0);
    SceAtSetEnable(0x11, 0);
    em = (cEm39*) r31c_work->krauser.getPtr();
    if (em) {
        em->set1stDoorClear();
    }
    SceEventEnd(0);
    SceAtDataSet_exec(0x17, 0x12, 0, (TaskFunc) r31c_SetContinuePoint, 0, 1);
    pG->Room_flg[0] |= 0x20000000;
}

// Krauser hides before the timer runs out: the door unlocks without the cut.
static void r31c_TimerDoorCancelCheck()
{
    cEm39* em = (cEm39*) r31c_work->krauser.getPtr();

    if (em == 0) {
        return;
    }
    while (em->ckHide() == 1) {
        SceSleep(1);
    }
    while (em->ckHide() != 1) {
        if (pG->Room_flg[0] & 0x40000000) {
            return;
        }
        SceSleep(1);
    }
    r31c_TimerDoorCancel();
}

// Krauser hid before the timer ran out: the timer effect swapped, door 8 normal, area 0x11 off,
// Krauser told the first door is clear, the continue point armed, Room_flg[0] 0x20000000.
void r31c_TimerDoorCancel()
{
    cEm39* em;

    EffectDelete(1, ESP_CORE_KIND_ROOM01);
    EstSet(r31c_work->door8, -1, 0, 0, EFF_ROOM, 0x18, 1, ESP_CORE_KIND_NONE, 0, 0);
    r31c_work->door8->setNormal();
    SceAtSetEnable(0x11, 0);
    em = (cEm39*) r31c_work->krauser.getPtr();
    if (em) {
        em->set1stDoorClear();
    }
    SceAtDataSet_exec(0x17, 0x12, 0, (TaskFunc) r31c_SetContinuePoint, 0, 1);
    pG->Room_flg[0] |= 0x20000000;
}

// The second battle: the wall slides and the rack rises out of the floor.
static void r31c_Krauser2ndBattle()
{
    cEm39* em = (cEm39*) r31c_work->krauser.getPtr();
    cObj* wall = SmdGetObjPtr(0x69);
    cObj* floor = SmdGetObjPtr(0x6A);

    wall->be_flag |= 0x20;
    floor->be_flag |= 0x20;
    SceSleep(30);
    SceExec(0x12, (TaskFunc) r31c_CheckTalkToKrauser, 0, 0, 2, 0);
    while (em->ckHide() != 1) {
        SceSleep(1);
    }
    SceSleep(60);
    SceEventStart(1);
    if (r31c_work->rack) {
        CamCtrl.CutCall(0x21);
        pG->Room_flg[0] &= ~0x80000000;
        SceSetEventCancel(1, (TaskFunc) r31c_Krauser2ndBattleEndProc, 0, 0, 1);
        r31c_work->hSnd = RoomSeCall(0x1F, &wall->pos, 0, 0, wall);
        while (wall->pos.x > 34780.0f) {
            wall->pos.x -= 50.0f;
            SceSleep(1);
        }
        wall->pos.x = 34780.0f;
        r31c_work->hSnd = RoomSeCall(0xD, &r31c_work->rack->pos, 0, 0, r31c_work->rack);
        while (r31c_work->rack->pos.y < 5200.0f) {
            r31c_work->rack->pos.y += 50.0f;
            floor->pos.y += 50.0f;
            SceSleep(1);
        }
        r31c_work->hSnd = RoomSeCall(0xE, &r31c_work->rack->pos, 0, 0, r31c_work->rack);
        r31c_work->rack->pos.y = 5200.0f;
        floor->pos.y = 5200.0f;
        SceSetEventCancel(0, 0, 0, -1, 1);
    }
    r31c_Krauser2ndBattleEndProc();
}

// End of the second battle's opening (also its cancel path): SE stopped, the wall 0x69 shown, the
// rack and floor 0x6A raised to y 5200, camera back, SceEventEnd; the player moved off the rack if on it.
static void r31c_Krauser2ndBattleEndProc()
{
    if (pG->Room_flg[0] & 0x80000000) {
        SndStop(r31c_work->hSnd, 0);
    }
    SmdGetObjPtr(0x69)->be_flag &= ~2;
    SmdGetObjPtr(0x6A)->pos.y = 5200.0f;
    if (r31c_work->rack) {
        r31c_work->rack->pos.y = 5200.0f;
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (SceAtHitCheck(0x1E)) {
        SetPosAngY(pPL, 36176.0f, 5200.0f, -4098.0f, 3.1f);
    }
    RsfSet(G_ROOM_ID, 0x17);
    SceExec(0x12, (TaskFunc) r31c_SwitchPushCheck, 0, 0, 2, 0);
}

// The rack pushed onto the switch: the two lever doors open.
static void r31c_SwitchPushCheck()
{
    if (r31c_work->rack) {
        void* model;

        while (SceAtCheckHitModel(0x12, r31c_work->rack) == 0) {
            SceSleep(1);
        }
        model = NULL;
        RoomSeCall(0x13, &r31c_work->rack->pos, 0, 0, 0);
        SceEventStart(1);
        pG->Room_flg[0] &= ~0x80000000;
        SceSetEventCancel(1, (TaskFunc) r31c_SwitchPushCheckEndProc, 0, 0, 1);
        CamCtrl.CutCall(0x12);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 1, ESP_CORE_KIND_ROOM00, 0, model);
        r31c_work->door[4].setOpen();
        while (r31c_work->door[4].getStatus() != 1) {
            SceSleep(1);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(0x1A);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x14, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        r31c_work->door[5].setOpen();
        while (r31c_work->door[5].getStatus() != 1) {
            SceSleep(1);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r31c_SwitchPushCheckEndProc();
    }
}

// End of the switch-push cut (also its cancel path): doors 4/5 snapped open, effect dropped, camera back.
static void r31c_SwitchPushCheckEndProc()
{
    if (pG->Room_flg[0] & 0x80000000) {
        r31c_work->door[4].setOpened();
        r31c_work->door[5].setOpened();
        EffectDelete(1, ESP_CORE_KIND_ROOM00);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The two levers: each one usable once its door is open.
static void r31c_LeverCheck()
{
    ((cEmSwitch*) r31c_work->sw[0])->setClosed();
    r31c_work->sw[0]->setNoSuspend(1);
    ((cEmSwitch*) r31c_work->sw[1])->setClosed();
    r31c_work->sw[1]->setNoSuspend(1);
    if (RsfCheck(G_ROOM_ID, 0x10)) {
        r31c_work->door[4].setOpened();
        r31c_work->door[5].setOpened();
    }
    for (;;) {
        if (RsfCheck(G_ROOM_ID, 0x12) == 0) {
            if (r31c_work->door[4].getStatus() == 1) {
                ((cEmSwitch*) r31c_work->sw[0])->setActButton(1);
            } else {
                ((cEmSwitch*) r31c_work->sw[0])->setActButton(0);
            }
            if (((cEmSwitch*) r31c_work->sw[0])->ckSwitch() == 1) {
                RsfSet(G_ROOM_ID, 0x12);
                ((cEmSwitch*) r31c_work->sw[0])->setActButton(0);
                SceExec(0x12, (TaskFunc) r31c_LeverOperate, 0, 0, 2, 0);
            }
        } else {
            ((cEmSwitch*) r31c_work->sw[0])->setOpened();
            ((cEmSwitch*) r31c_work->sw[0])->setActButton(0);
            r31c_work->door[4].setOpened();
        }
        if (RsfCheck(G_ROOM_ID, 0x13) == 0) {
            if (r31c_work->door[5].getStatus() == 1) {
                ((cEmSwitch*) r31c_work->sw[1])->setActButton(1);
            } else {
                ((cEmSwitch*) r31c_work->sw[1])->setActButton(0);
            }
            if (((cEmSwitch*) r31c_work->sw[1])->ckSwitch() == 1) {
                RsfSet(G_ROOM_ID, 0x13);
                ((cEmSwitch*) r31c_work->sw[1])->setActButton(0);
                SceExec(0x12, (TaskFunc) r31c_LeverOperate, 1, 0, 2, 0);
            }
        } else {
            ((cEmSwitch*) r31c_work->sw[1])->setOpened();
            ((cEmSwitch*) r31c_work->sw[1])->setActButton(0);
            r31c_work->door[5].setOpened();
            if (ScfFlagChk(pG, SCF_R31C_TOWER_EXPLODE) == 0) {
                r31c_work->door[6].setOpened();
            }
        }
        if (RsfCheck(G_ROOM_ID, 0x12) == 0 || RsfCheck(G_ROOM_ID, 0x13) == 0) {
            SceSleep(1);
        } else {
            break;
        }
    }
    ((cEmSwitch*) r31c_work->sw[0])->setOpened();
    if (ScfFlagChk(pG, SCF_R31C_TOWER_EXPLODE)) {
        ((cEmSwitch*) r31c_work->sw[1])->setClosed();
        r31c_work->door[6].setClosed();
    }
    ((cEmSwitch*) r31c_work->sw[0])->setActButton(0);
    ((cEmSwitch*) r31c_work->sw[1])->setActButton(0);
}

// Lever `no`: camera cut 0x13 / 0x14 while door 1 / door 6 slides open with its effect; player-cancellable.
static void r31c_LeverOperate(int no)
{
    void* model = NULL;

    SceEventStart(1);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_LeverOperateEndProc, no, 0, 1);
    if (no == 0) {
        CamCtrl.CutCall(0x13);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 1, ESP_CORE_KIND_NONE, 0, model);
        r31c_work->door[1].setOpen();
        while (r31c_work->door[1].getStatus() != 1) {
            SceSleep(1);
        }
    } else {
        CamCtrl.CutCall(0x14);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x12, 1, ESP_CORE_KIND_NONE, 0, model);
        r31c_work->door[6].setOpen();
        while (r31c_work->door[6].getStatus() != 1) {
            SceSleep(1);
        }
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_LeverOperateEndProc(no);
}

// End of lever `no` (also its cancel path): its door snapped open, effect dropped, camera back, SceEventEnd.
static void r31c_LeverOperateEndProc(int no)
{
    if (pG->Room_flg[0] & 0x80000000) {
        if (no == 0) {
            r31c_work->door[1].setOpened();
        } else {
            r31c_work->door[6].setOpened();
        }
    }
    CamCtrl.Comeback(0);
    if (no == 0) {
        cEm39* em = (cEm39*) r31c_work->krauser.getPtr();

        if (em) {
            em->set2ndDoorClear();
        }
        r31c_SetContinuePoint(1);
    }
    SceEventEnd(0);
}

// The player leaves the tower area before the bomb: the count down display goes.
static void r31c_CountDownEnd()
{
    while (PlGetStatus() != 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 0x1B);
    if (pSys->eff_country == 0) {
        r31c_work->countDown.setDisp(0);
    }
}

// The bomb count-down task: starts the cockpit count-down and waits for time-out, then the tower
// explodes and Status_flg[2] 0x00020000 clears.
static void r31c_CountDownThread()
{
    SceCTask()->task->flag &= ~2;
    r31c_work->countDown.countStart();
    do {
        if (r31c_work->countDown.isTimeOut() == 1) {
            break;
        }
        SceSleep(1);
    } while (1);
    // The post-loop `lis r31c_work@ha` is not shared with the loop's hoisted one: the dead loop's
    // LOOP_END note stops cse1 from skipping the `SceSleep` block into the exit (as in r204).
    do { } while (0);
    r31c_work->countDown.countEnd();
    SceExec(0x12, (TaskFunc) r31c_TowerExplode, 0, 0, 2, 0);
    StaFlagOff(pG, STA_TIMER_NO_PAUSE);
}

// The tower entrance door closes behind the player.
static void r31c_TowerEntranceClose()
{
    void* model = NULL;

    SceEventStart(0);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_TowerEntranceCloseEndProc, 0, 0, 1);
    CamCtrl.CutCall(0x20);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x16, 1, ESP_CORE_KIND_ROOM00, 0, model);
    r31c_work->door[8].setClose();
    while (r31c_work->door[8].getStatus() != 0) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_TowerEntranceCloseEndProc();
}

// End of the tower entrance closing (also its cancel path): the door snapped shut, camera back, SceEventEnd.
static void r31c_TowerEntranceCloseEndProc()
{
    if (pG->Room_flg[0] & 0x80000000) {
        r31c_work->door[8].setClosed();
        EffectDelete(1, ESP_CORE_KIND_ROOM00);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtDataSet_exec(0x1A, 0x12, 0, (TaskFunc) r31c_SetContinuePoint, (void*) 2, 1);
    r31c_work->door[6].setClosed();
    ((cEmSwitch*) r31c_work->sw[1])->setClosed();
}

// Time out: the explosion cut (camera, effects, quake), the tower swapped for its ruin model.
static void r31c_TowerExplode()
{
    Vec zero = {0, 0, 0};

    if (r31c_work->krauser2.isAlive()) {
        r31c_work->krauser2.destroy();
    }
    ScfFlagOn(pG, SCF_R31C_TOWER_EXPLODE);
    SceEventStart(0);
    CamCtrl.CutCall(0x22);
    EffectDelete(1, ESP_CORE_KIND_ROOM03);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    r31c_TowerExplodeModelSet(0, 0);
    r31c_TowerExplodeModelSet(1, 1);
    EatMgr.destroy(r31c_work->towerEat);
    r31c_work->towerEat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 5);
    SceAtSetEnable(0x87, 0);
    SceAtSetEnable(0x8F, 0);
    r31c_work->hSnd = SndStrReq(1, 0xEE, 0x80000003, 0, 0, 0.0f);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_TowerExplodeEndProc, 0, 0, 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_TowerExplodeEndProc();
}

// End of the explosion cut (also its cancel path): the ruin shown, effects dropped, camera back, the
// player placed, SceEventEnd, the chapter end.
static void r31c_TowerExplodeEndProc()
{
    int die = 0;

    if (pG->Room_flg[0] & 0x80000000) {
        EffectDelete(1, ESP_CORE_KIND_ROOM00);
        SndStop(r31c_work->hSnd, 0);
    }
    if (pSys->eff_country == 0) {
        if (RsfCheck(G_ROOM_ID, 0x1B)) {
            CamCtrl.Comeback(0);
        } else {
            die = 1;
        }
    } else {
        die = 1;
    }
    if (die) {
        pPL->be_flag &= ~2;
        DiedemoExec(0, 0);
    } else {
        SceEventEnd(0);
    }
}

// The tower cover: the crest door closes again over the bomb.
static void r31c_TowerCoverClose()
{
    StaFlagOn(pG, STA_TIMER_NO_PAUSE);
    SceEventStart(1);
    CamCtrl.CutCall(0x15);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_TowerCoverCloseEndProc, 0, 0, 1);
    r31c_work->door[2].setOpen();
    while (r31c_work->door[2].getStatus() != 1) {
        SceSleep(1);
    }
    r31c_work->door[3].setClose();
    EstSet(0, -1, 0, 0, EFF_ROOM, 0x13, 1, ESP_CORE_KIND_NONE, 0, 0);
    while (r31c_work->door[3].getStatus() != 0) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_TowerCoverCloseEndProc();
}

// End of the tower cover closing (also its cancel path): the crest door snapped shut, camera back, SceEventEnd.
static void r31c_TowerCoverCloseEndProc()
{
    if (pG->Room_flg[0] & 0x80000000) {
        r31c_work->door[2].setOpened();
        r31c_work->door[3].setClosed();
    }
    pG->Room_flg[0] |= 0x04000000;
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_TIMER_NO_PAUSE);
}

// Krauser plants the bomb (cut 0x1F with its ticking).
static void r31c_BombCutSet(cEm39* em)
{
    Vec pos = {1356.0f, 7882.0f, -9857.0f};
    int i = -3;

    if (em == 0) {
        return;
    }
    while (em->ckBombCutEnable() != 1) {
        SceSleep(1);
    }
    SceEventStart(1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xF, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    pG->Room_flg[0] &= ~0x80000000;
    SceSetEventCancel(1, (TaskFunc) r31c_BombCutSetEndProc, 0, 0, 1);
    CamCtrl.CutCall(0x1F);
    while (CamCtrl.IsMotionEnd() == 0) {
        if (i % 11 == 0) {
            RoomSeCall(0x1E, &pos, 0, 0, 0);
        }
        i++;
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r31c_BombCutSetEndProc();
}

// End of the bomb-planting cut: camera back, SceEventEnd.
static void r31c_BombCutSetEndProc()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r31c_CountDownThread, 0, 0, 2, 0);
}

// The tower models: 0 = the standing tower, 1 = the ruin.
void r31c_TowerExplodeModelSet(int no, int on)
{
    if (no == 0) {
        SmdSetTrans(0xB4, on);
        SmdSetTrans(0x96, on);
        SmdSetTrans(0x97, on);
        SmdSetTrans(0x99, on);
        SmdSetTrans(0x9A, on);
        SmdSetTrans(0x8C, on);
        SmdSetTrans(0x91, on);
        SmdSetTrans(0x8D, on);
        SmdSetTrans(0x7C, on);
        SmdSetTrans(0x7B, on);
        SmdSetTrans(0x80, on);
        SmdSetTrans(0x6D, on);
        SmdSetTrans(0xA0, on);
    } else {
        SmdSetTrans(0xC8, on);
    }
}

// Once (Room_flg bit 0x14): chapter 5-3 ends (SceSetChapterEnd(0x10)) after the tower.
static void r31c_SetChapterEnd()
{
    if (RsfCheck(G_ROOM_ID, 0x14) == 0) {
        FadeColorPair col;

        RsfSet(G_ROOM_ID, 0x14);
        SceEventStart(0);
        pPL->setNoSuspend(1);
        *(u32*) &col.start = 0;
        *(u32*) &col.end = 0xFF;
        FadeSet(1, &col.start, &col.end, 30, 0, 0);
        FadeWait(1);
        SceSetChapterEnd(0x10, 0);
        SceSleep(1);
    }
}

// The continue points (the room flag per point).
static int r31c_contFlag[3] = {0xF, 0x10, 0x11};

// `&r31c_contFlag` is allocated before `pG@ha` (r26/r25): the flag is read before the room id.
static void r31c_SetContinuePoint(int no)
{
    int flag = r31c_contFlag[no];

    if (RsfCheck(G_ROOM_ID, flag) == 0) {
        RsfSet(G_ROOM_ID, r31c_contFlag[no]);
        GameSave.save(pSaveData, -1);
    }
}

// The s00 event: Krauser's first appearance.
static void r31cEventS00()
{
    int i;

    RsfSet(G_ROOM_ID, 0);
    EvtMgr.EvtReadExec("event/evd/r31cs00.evd", (u8) GetEmIdFromList(0x19), EvtReadFlagNone);
    SceAtDataSet_exec(0x14, 0x12, 0, (TaskFunc) r31cEventS02, 0, 1);
    EvtMgr.EvtReadAram("event/evd/r31cs02.evd", (u8) GetEmIdFromList(0x19), 0, 0, 0);
    SetPosAngY(pPL, 3805.0f, 0.0f, 10095.0f, -2.49f);
    SndBgmTblSet(0x31C, 1);
    GamePointBossReset();
    for (i = 0; i < 8; i++) {
        r31c_work->post[i].atari_set();
    }
    SndRoomStrStart(1, 0, 1);
}

// The s01 event: the knife fight.
static void r31cEventS01()
{
    cModel* item = SceAtItemModelPtr(0x81);

    if (RsfCheck(G_ROOM_ID, 1)) {
        SceExit();
    }
    RsfSet(G_ROOM_ID, 1);
    SndRoomStrStop(1);
    SceEventStart(1);
    if (item) {
        item->setNoSuspend(1);
        item->LightInfo.EnableMask &= ~0x20;
        item->LightInfo.EnableMask |= 0x10;
    }
    CamCtrl.CutCall(0x16);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (item) {
        item->setNoSuspend(0);
        item->LightInfo.EnableMask |= 0x20;
        item->LightInfo.EnableMask &= ~0x10;
    }
    SceAtDataReset(0x81);
    SceAtExecute(0x81);
    while (SceAtItemFlgCk(0x81) == 0) {
        SceSleep(1);
    }
    SceEventEnd(0);
    pG->Room_flg[0] &= ~0x80000000;
    EvtMgr.EvtReadExec("event/evd/r31cs01.evd", (u8) GetEmIdFromList(0x19), EvtReadFlagNone);
    SndBgmTblSet(0x31C, 0);
    SndRoomStrStart(1, 0, 1);
    r31cEventS01EndProc();
}

// End of the knife-fight event: the follow-up enemy list entry's set byte by the fight result
// (Room_flg[0] 0x10000000 = the button was hit), area 0x1C = the tower cover, the room state.
void r31cEventS01EndProc()
{
    EmListData* l;
    int i;

    l = &pG->Em_list[0x13];
    l->set = (pG->Room_flg[0] & 0x10000000) ? 2 : 3;
    r31c_work->krauser2.setEm(0x13, -1, 1, 1, 1);
    r31c_work->krauser.destroy();
    CamCtrl.Comeback(0);
    GamePointBossReset();
    SceExec(0x12, (TaskFunc) r31c_KrauserDieCheck, (int) r31c_work->krauser2.getPtr(), 0, 2, 0);
    SceExec(0x12, (TaskFunc) r31c_BombCutSet, (int) r31c_work->krauser2.getPtr(), 0, 2, 0);
    for (i = 0; i < 15; i++) {
        if (r31c_work->seeker[i].isAlive()) {
            r31c_work->seeker[i].destroy();
        }
    }
    SceAtDataSet_exec(0x1C, 0x12, 0, (TaskFunc) r31c_TowerCoverClose, 0, 1);
}

// The s02 event: Krauser takes the crest; the second battle begins.
static void r31cEventS02()
{
    if (RsfCheck(G_ROOM_ID, 2)) {
        SceExit();
    }
    RsfSet(G_ROOM_ID, 2);
    pG->Room_flg[0] &= ~0x80000000;
    SndRoomStrStop(1);
    EvtMgr.EvtReadExec("event/evd/r31cs02.evd", (u8) GetEmIdFromList(0x19), EvtReadFlagNone);
    SceEventStart(1);
    if (pG->Room_flg[0] & 0x80000000) {
        ItemMgr.get(0x85, 1);
        ItfFlagOn(pG, ITF_R31C_CREST_A);
        SceAtSetEnable(0x80, 0);
        r31cEventS02EndProc();
    } else {
        SceEventEnd(0);
        SceAtDataReset(0x80);
        SceAtExecute(0x80);
        while (SceAtItemFlgCk(0x80) == 0) {
            SceSleep(1);
        }
        SceEventStart(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x1A, 1, ESP_CORE_KIND_ROOM00, 0, 0);
        CamCtrl.CutCall(0x13);
        r31c_work->door[1].setClose();
        pG->Room_flg[0] &= ~0x80000000;
        SceSetEventCancel(1, (TaskFunc) r31cEventS02EndProc, 0, 0, 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r31cEventS02EndProc();
    }
}

// End of the s02 event (also its cancel path): door 1 snapped shut; area 0x81 re-arms the s01 event.
static void r31cEventS02EndProc()
{
    if (pG->Room_flg[0] & 0x80000000) {
        EffectDelete(1, ESP_CORE_KIND_ROOM00);
        r31c_work->door[1].setClosed();
    }
    CamCtrl.Comeback(0);
    SetAngY(pPL, -1.66f);
    SceEventEnd(0);
    ((cEm39*) r31c_work->krauser.getPtr())->set2ndBattle();
    SceExec(0x12, (TaskFunc) r31c_Krauser2ndBattle, 0, 0, 2, 0);
    SceAtDataSet_exec(0x81, 0x12, 0, (TaskFunc) r31cEventS01, 0, 1);
    EvtMgr.EvtReadAram("event/evd/r31cs01.evd", 0, 0, 0, 0);
    SndRoomStrStart(1, 0, 0);
}

// Event r31cs00 callback: the two crest item models kept updating during the event and released after;
// the end sets Room_flg[0] bit 31.
static void Evt_R31CS00_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        SceAtItemModelPtr(0x80)->setNoSuspend(1);
        SceAtItemModelPtr(0x81)->setNoSuspend(1);
        break;
    case 1:
        break;
    case 2:
        SceAtItemModelPtr(0x80)->setNoSuspend(0);
        SceAtItemModelPtr(0x81)->setNoSuspend(0);
        break;
    case 3:
        pG->Room_flg[0] |= 0x80000000;
        break;
    }
}

// Action button of the knife fight hit: Room_flg[0] 0x10000000.
static void r31c_EventS01Act()
{
    pG->Room_flg[0] |= 0x10000000;
}

// 1 while the knife fight's action button is offered (cut 0xB frames 0x66..0x78).
static int r31c_evtS01Flag = 0;

// Event r31cs01 callback (the knife fight): cut 0xB frames 0x66..0x78 offer the action button
// (r31c_evtS01Flag); per-cut model flags; the end records the outcome.
static void Evt_R31CS01_Func(Event* e)
{
    void* mod;
    int mode = e->FuncType;
    int cut;

    // `mode` keeps the switch value live into case 1 (the `1` stores reuse it). `cut` is set before
    // the FlgOnStatus call so local-alloc gives it r30 (never r31, the frame pointer), which is why
    // the global pass then puts `mode` in r30 and `e` in r31.

    switch (mode) {
    case 0:
        pG->Room_flg[0] &= ~0x10000000;
        r31c_mesNo = (Rnd() & 1) ? 3 : 4;
        cut = 0xB;
        EvtFlgOnStatus(e, 3);
        e->EvtCancelCut = cut;
        break;
    case 1:
        switch (e->NowCut) {
        case 0:
            if (e->GetMod(&mod, "em3900c", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
            if (e->GetMod(&mod, "em3900d", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
            if (e->GetMod(&mod, "em3900e", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
            if (e->GetMod(&mod, "em3900f", 0, 0) == 1) {
                ((cModel*) mod)->ot_type = 1;
            }
            if (e->GetMod(&mod, "evmb800", 0, 0) == 1) {
                ((cModel*) mod)->be_flag |= 0x10;
            }
            break;
        case 4:
            SmdGetObjPtr(0x97)->be_flag &= ~2;
            break;
        case 7:
            if (e->NowFrame > 0x3B) {
                if (e->GetMod(&mod, "em3900d", 0, 0) == 1) {
                    int f = e->NowFrame - 60;
                    int mf = e->MaxFrame - 60;

                    ((cModel*) mod)->invisible_factor = 1.0f - (f32) f / (f32) mf;
                }
                e->GetMod(&mod, "em3900e", 0, 0);
            }
            break;
        case 0xB:
            if (e->NowFrame == 0x66) {
                r31c_evtS01Flag = 1;
            }
            if (e->NowFrame == 0x78) {
                r31c_evtS01Flag = 0;
            }
            break;
        }
        break;
    case 2:
        SmdGetObjPtr(0x97)->be_flag |= 2;
        break;
    case 3:
        if (EvtStatusCk(e, 0x4000) == 0) {
            EvtMgr.EvtSndStrPlay(evtKey(&EvtMgr), 1, 0x8B, 1, 0.0f);
        }
        break;
    }
    if (r31c_evtS01Flag == 1) {
        if (pG->Room_flg[0] & 0x10000000) {
            r31c_evtS01Flag = 0;
            e->CancelSet();
        } else {
            ActBtn.set(ACT_GUARD, 5, (void*) r31c_EventS01Act, 0, ACTCTR_ENFORCE_EXEC | ACTCTR_NO_SUSPEND | ACTCTR_EXACT_KEY, r31c_mesNo, ACT_FUNC_SCE, 0);
            SpfFlagOff(pG, SPF_ACTBTN);
        }
    }
}

// Event r31cs02 callback (Krauser takes the crest): per-cut flags; the end sets Room_flg[0] bit 31.
static void Evt_R31CS02_Func(Event* e)
{
    // The empty arms keep their own compare-tree nodes: two of them return, one breaks.
    switch (e->FuncType) {
    case 0:
        return;
    case 1:
        break;
    case 2:
        return;
    case 3:
        pG->Room_flg[0] |= 0x80000000;
        break;
    }
}

// The message shown over Krauser's corpse.
static void r31c_KrauserCorpseMes()
{
    CamCtrl.StartLookDownEm(r31c_work->krauser2.getPtr());
    SceEventStart(1);
    r31c_work->krauser2.setNoSuspend(1);
    SceMesSet(4, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    SceEventEnd(0);
    CamCtrl.EndLookDownEm();
}

// ---- cR31CPost ----

// The posts' positions / rotations, the room flag each one sets when it falls.
static Vec r31c_postPos[8] = {
    {-9139.0f, 2000.0f, -3502.0f}, {7684.0f, 2000.0f, -548.0f},   {-700.0f, 2000.0f, -3502.0f},
    {7870.0f, 2000.0f, -3502.0f},  {-4890.0f, 0.0f, 9037.0f},     {-676.0f, 2000.0f, -521.0f},
    {-9031.0f, 2000.0f, -521.0f},  {-1245.0f, 0.0f, 7546.0f},
};
static Vec r31c_postRot[8] = {
    {0.0f, 0.0f, 0.0f}, {0.0f, 1.5707964f, 0.0f}, {0.0f, -1.5707964f, 0.0f}, {0.0f, 3.1415927f, 0.0f},
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},       {0.0f, 0.0f, 0.0f},        {0.0f, 0.0f, 0.0f},
};
static int r31c_postFlag[8] = {7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE};

// Post `no`: its object at r31c_postPos/Rot, hp, the room flag it sets when it falls, collision pieces (atari_set).
void cR31CPost::init(u32 no)
{
    Vec zero = {0, 0, 0};

    this->no = no;
    switch (no) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        type = 1;
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21), &r31c_postPos[no],
                        &r31c_postRot[no], 0x10, 1);
        obj->be_flag |= 0x1000;
        if (RsfCheck(G_ROOM_ID, 0)) {
            atari_set();
        }
        eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r31c_postPos[no], &r31c_postRot[no], 2);
        hp = 1000;
        break;
    case 5:
    case 6:
        type = 0;
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), &r31c_postPos[no],
                        &r31c_postRot[no], 0x10, 1);
        obj->be_flag |= 0x1000;
        if (RsfCheck(G_ROOM_ID, 0)) {
            atari_set();
        }
        eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r31c_postPos[no], &r31c_postRot[no], 1);
        hp = 1000;
        break;
    default:
        type = 2;
        obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x24), ROOM_ARC_PTR(pG->pRoom, 0x25), &r31c_postPos[no],
                        &r31c_postRot[no], 0x10, 1);
        obj->be_flag |= 0x1000;
        if (RsfCheck(G_ROOM_ID, 0)) {
            atari_set();
        }
        eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 3);
        hp = 500;
        break;
    }
    mode = 0;
}

// The routine table (.data): dmg_ck / die by `mode`.
static void (cR31CPost::*r31c_postTbl[2])() = {&cR31CPost::dmg_ck, &cR31CPost::die};

// Per-frame step: run the current mode (dmg_ck / die) of r31c_postTbl.
void cR31CPost::move()
{
    (this->*r31c_postTbl[mode])();
}

// The post's collision and attribute pieces (a hit box the player can shoot).
void cR31CPost::atari_set()
{
    Vec zero = {0, 0, 0};

    switch (type) {
    case 1:
        hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &zero, 0, 1);
        YarareInit(hit, 0.0f, 1200.0f, 0.0f, 600.0f, 1300.0f, 1, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        YarareAdd(hit, &box, 0.0f, 0.0f, 0.0f, 900.0f, 500.0f, 1, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        break;
    case 0:
        hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &zero, 0, 1);
        YarareInit(hit, 0.0f, 1200.0f, 0.0f, 600.0f, 2600.0f, 1, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        YarareAdd(hit, &box, 0.0f, 0.0f, 0.0f, 900.0f, 500.0f, 1, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        break;
    case 2:
        hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &zero, 0, 1);
        YarareInitCube(hit, 0.0f, 0.0f, 0.0f, 2500.0f, 2150.0f, 700.0f, 1, YAT_FLAG_ON | YAT_FLAG_NO_MARK);
        break;
    }
    hit->setParent(obj, 0, 0);
}

// Mode 0: apply weapon hits to the post (250 hp per hit, the type remembered); at 0 hp -> die.
void cR31CPost::dmg_ck()
{
    if (hit == 0) {
        return;
    }
    if (hit->ckStatus() == 1) {
        switch (hit->dmg.m_Wep) {
        case 0xD:
        case 0x12:
        case 0x13:
            dmgType = 4;
            hp -= 250;
            break;
        case 0:
        case 0xE:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1F:
        case 0x20:
        case 0x2A:
            break;
        case 7:
        case 8:
        case 0x21:
            EmDmBloodSet2(hit, 1, 5, 0, 0, 0);
            dmgType = 3;
            hp -= 25;
            break;
        default:
            EmDmBloodSet2(hit, 1, 4, 0, 0, 0);
            dmgType = 3;
            hp -= 25;
            break;
        }
    }
    if (hp <= 0) {
        step = 0;
        mode++;
    }
}

// Mode 1: the post topples (rotation over frames, the collision released), its room flag set.
void cR31CPost::die()
{
    if (step == 0) {
        obj->be_flag &= ~2;
        eat->setDisable();
        switch (type) {
        case 1:
            if (dmgType == 4) {
                EstSet(0, -1, &r31c_postPos[no], &r31c_postRot[no], EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, 0, 0);
            } else {
                EstSet(0, -1, &r31c_postPos[no], &r31c_postRot[no], EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            YarareInit(hit, 0.0f, 0.0f, 0.0f, 600.0f, 2500.0f, 1, 0);
            break;
        case 0:
            if (dmgType == 4) {
                EstSet(0, -1, &r31c_postPos[no], &r31c_postRot[no], EFF_ROOM, 3, 1, ESP_CORE_KIND_NONE, 0, 0);
            } else {
                EstSet(0, -1, &r31c_postPos[no], &r31c_postRot[no], EFF_ROOM, 2, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            YarareInit(hit, 0.0f, 0.0f, 0.0f, 600.0f, 3800.0f, 1, 0);
            break;
        default:
            if (dmgType == 4) {
                EstSet(0, -1, &r31c_postPos[no], &r31c_postRot[no], EFF_ROOM, 7, 1, ESP_CORE_KIND_NONE, 0, 0);
            } else {
                EstSet(0, -1, &r31c_postPos[no], &r31c_postRot[no], EFF_ROOM, 6, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            YarareInit(hit, 0.0f, 0.0f, 0.0f, 600.0f, 3800.0f, 1, 0);
            break;
        }
        RoomSeCall(0, &obj->pos, 0, 0, 0);
        RsfSet(G_ROOM_ID, r31c_postFlag[no]);
        step++;
    }
}

// ---- cR31CDoor ----

void cR31CDoor::init(u32 id)
{
    Vec zero = {0, 0, 0};

    init_ = 0;
    obj = SmdGetObjPtr(id);
    if (obj) {
        objId = id;
        obj->be_flag |= 0x20;
        pos = obj->pos;
        switch (objId) {
        case 0x78:
            unlockNo = 0x2D;
            dist = 3333.0f;
            break;
        case 0x79:
            dist = 3000.0f;
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 2);
            unlockNo = 0x2F;
            break;
        case 0x7B:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 4);
            eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 4);
            dist = 3583.0f;
            break;
        case 0x7C:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 3);
            dist = -1100.0f;
            break;
        case 0x7D:
            dist = -1520.0f;
            break;
        case 0x7E:
            dist = -1060.0f;
            break;
        case 0x7F:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 1);
            dist = -2700.0f;
            unlockNo = 0x33;
            break;
        case 0x80:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 5);
            dist = 2900.0f;
            break;
        case 0x6D:
            sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 8);
            dist = 3200.0f;
            break;
        }
        init_ = 1;
    }
}

// The routine table (.data): wait / open / close by `mode`.
static void (cR31CDoor::*r31c_doorTbl[3])() = {&cR31CDoor::wait, &cR31CDoor::open, &cR31CDoor::close};

// Per-frame step: run the current mode (wait / open / close) of r31c_doorTbl.
void cR31CDoor::move()
{
    if (init_) {
        (this->*r31c_doorTbl[mode])();
    }
}

// Mode 0: idle.
void cR31CDoor::wait()
{
}

// Slide open: the door 0x78 (the crest door) moves along x with its crests, 0x7B / 0x7F / 0x7D /
// 0x7E along z / y, the others along y.
void cR31CDoor::open()
{
    int se = -1;

    switch (step) {
    case 0:
        switch (objId) {
        case 0x6D:
        case 0x79:
        case 0x7F:
        case 0x80:
            se = 5;
            break;
        case 0x7B:
            se = 0xD;
            break;
        case 0x78:
            se = 0xF;
            break;
        case 0x7D:
            se = 0x14;
            break;
        case 0x7E:
            se = 0x14;
            break;
        case 0x7C:
            se = 0x21;
            break;
        }
        if (se != -1) {
            hSnd = RoomSeCall((u16) se, &obj->pos, 0, 0, 0);
        }
        step++;
        break;
    case 1:
        switch (objId) {
        case 0x6D:
        case 0x79:
        case 0x80:
            obj->pos.y += 45.0f;
            if (obj->pos.y > pos.y + dist) {
                obj->pos.y = pos.y + dist;
                se = 6;
                sat->setDisable();
                step++;
            }
            break;
        case 0x78: {
            int i;

            obj->pos.x += 60.0f;
            if (obj->pos.x > pos.x + dist) {
                obj->pos.x = pos.x + dist;
                step++;
            }
            for (i = 0; i < 3; i++) {
                if (r31c_work->crest[i]) {
                    PSVECAdd(&obj->pos, &r31c_work->crestOfs[i], &r31c_work->crest[i]->pos);
                }
            }
            break;
        }
        case 0x7B:
            obj->pos.z += 60.0f;
            if (obj->pos.z > pos.z + dist) {
                obj->pos.z = pos.z + dist;
                se = 0xE;
                sat->setDisable();
                eat->setDisable();
                step++;
            }
            break;
        case 0x7F:
            obj->pos.y -= 45.0f;
            if (obj->pos.y < pos.y + dist) {
                obj->pos.y = pos.y + dist;
                if (sat) {
                    sat->setDisable();
                }
                se = 6;
                step++;
            }
            break;
        case 0x7C:
        case 0x7D:
        case 0x7E:
            obj->pos.y -= 30.0f;
            if (obj->pos.y < pos.y + dist) {
                obj->pos.y = pos.y + dist;
                if (sat) {
                    sat->setDisable();
                }
                step++;
            }
            break;
        }
        if (se != -1) {
            RoomSeCall((u16) se, &obj->pos, 0, 0, 0);
        }
        break;
    case 2:
        if (unlockNo) {
            FlagOnVar(&pG->Key_flg, (u32) unlockNo);
        }
        mode = 0;
        status = 1;
        step = 0;
        break;
    }
}

// Slide closed: the door falls back to `pos` (accelerating for the y doors), shakes, settles.
void cR31CDoor::close()
{
    int se = -1;

    switch (step) {
    case 0:
        switch (objId) {
        case 0x6D:
        case 0x79:
        case 0x80:
            spd = -40.0f;
            se = 7;
            sat->setEnable();
            break;
        case 0x78:
            spd = 30.0f;
            break;
        case 0x7B:
            se = 0xD;
            sat->setEnable();
            eat->setEnable();
            spd = -60.0f;
            break;
        case 0x7C:
            se = 0x21;
        case 0x7D:
        case 0x7E:
            if (sat) {
                sat->setEnable();
            }
            spd = 30.0f;
            break;
        case 0x7F:
            se = 5;
            sat->setEnable();
            spd = 40.0f;
            break;
        }
        if (se != -1) {
            hSnd = RoomSeCall((u16) se, &obj->pos, 0, 0, 0);
        }
        step++;
        break;
    case 1:
        switch (objId) {
        case 0x6D:
        case 0x79:
        case 0x80:
            obj->pos.y += spd;
            spd -= 20.0f;
            if (obj->pos.y < pos.y) {
                obj->pos.y = pos.y;
                timer = 5;
                se = 8;
                step++;
            }
            break;
        case 0x78: {
            int i;

            obj->pos.x -= spd;
            if (obj->pos.x < pos.x) {
                obj->pos.x = pos.x;
                timer = 5;
                step++;
            }
            for (i = 0; i < 3; i++) {
                if (r31c_work->crest[i]) {
                    PSVECAdd(&obj->pos, &r31c_work->crestOfs[i], &r31c_work->crest[i]->pos);
                }
            }
            break;
        }
        case 0x7B:
            obj->pos.z += spd;
            if (obj->pos.z < pos.z) {
                obj->pos.z = pos.z;
                timer = 5;
                se = 0xE;
                step++;
            }
            break;
        case 0x7F:
            se = 6;
        case 0x7C:
        case 0x7D:
        case 0x7E:
            obj->pos.y += spd;
            if (obj->pos.y > pos.y) {
                obj->pos.y = pos.y;
                timer = 5;
                step++;
            }
            break;
        }
        if (se != -1) {
            hSnd = RoomSeCall((u16) se, &obj->pos, 0, 0, 0);
        }
        break;
    case 2:
        obj->pos.x = fRand1_1() * 20.0f + pos.x;
        obj->pos.y = fRand0_1() * 20.0f + pos.y;
        obj->pos.z = fRand1_1() * 20.0f + pos.z;
        if (timer) {
            timer--;
        } else {
            obj->pos = pos;
            timer = 30;
            step++;
        }
        if (objId == 0x78) {
            int i;

            for (i = 0; i < 3; i++) {
                if (r31c_work->crest[i]) {
                    PSVECAdd(&obj->pos, &r31c_work->crestOfs[i], &r31c_work->crest[i]->pos);
                }
            }
        }
        break;
    case 3:
        if (timer) {
            timer--;
        } else {
            if (unlockNo) {
                FlagOffVar(&pG->Key_flg, (u32) unlockNo);
            }
            status = 0;
            mode = 0;
            step = 0;
        }
        break;
    }
}

// Request opening (mode 1) unless open / opening.
void cR31CDoor::setOpen()
{
    if (init_ == 0 || status == 1 || mode == 1) {
        return;
    }
    status = 2;
    mode = 1;
    step = 0;
}

// Request closing (mode 2) unless closed / closing.
void cR31CDoor::setClose()
{
    if (init_ == 0 || status == 0 || mode == 2) {
        return;
    }
    mode = status = 2;
    step = 0;
}

// 0 closed, 1 open, 4 moving; -1 when the door has no object.
int cR31CDoor::getStatus()
{
    if (init_) {
        return status;
    }
    return -1;
}

// Snap the door open (its slide axis at the open offset, collision following, areas set).
void cR31CDoor::setOpened()
{
    if (init_) {
        status = 1;
        if (sat) {
            sat->setDisable();
        }
        if (eat) {
            eat->setDisable();
        }
        mode = 0;
        step = 0;
        switch (objId) {
        case 0x78: {
            int i;

            obj->pos.x = pos.x + dist;
            for (i = 0; i < 3; i++) {
                if (r31c_work->crest[i]) {
                    PSVECAdd(&obj->pos, &r31c_work->crestOfs[i], &r31c_work->crest[i]->pos);
                }
            }
            break;
        }
        case 0x6D:
        case 0x79:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        case 0x80:
            obj->pos.y = pos.y + dist;
            break;
        case 0x7B:
            obj->pos.z = pos.z + dist;
            break;
        }
        SndStop(hSnd, 0);
        if (unlockNo) {
            FlagOnVar(&pG->Key_flg, (u32) unlockNo);
        }
    }
}

// Snap the door closed (back at `pos`, collision following, areas set).
void cR31CDoor::setClosed()
{
    if (init_) {
        status = 0;
        if (sat) {
            sat->setEnable();
        }
        if (eat) {
            eat->setEnable();
        }
        mode = 0;
        step = 0;
        obj->pos = pos;
        if (objId == 0x78) {
            int i;

            for (i = 0; i < 3; i++) {
                if (r31c_work->crest[i]) {
                    PSVECAdd(&obj->pos, &r31c_work->crestOfs[i], &r31c_work->crest[i]->pos);
                }
            }
        }
        SndStop(hSnd, 0);
        if (unlockNo) {
            FlagOffVar(&pG->Key_flg, (u32) unlockNo);
        }
    }
}

// ---- cR31CCountDown ----

void cR31CCountDown::countStart()
{
    Cckpt.startCountDownTimer(3, 0, 0);
    Cckpt.setWarningTime(0, 0, 0);
    setDisp(1);
    state = 1;
    running = 1;
}

// Stop the cockpit count-down (state bit 0 off, the display slides out); state 0.
void cR31CCountDown::countEnd()
{
    Cckpt.endCountDownTimer();
    setDisp(0);
    state = 0;
    running = 0;
}

// Pause (on = 1: cockpit count-down state bit 3, state 2) or resume (state 1) the count-down.
void cR31CCountDown::setPause(int on)
{
    if (on == 1) {
        Cckpt.pauseCountDownTimer();
        state = 2;
    } else {
        Cckpt.playCountDownTimer();
        state = 1;
    }
}

// Show (frameIn) or hide (frameOut) the count-down display.
void cR31CCountDown::setDisp(int on)
{
    Cckpt.transCountDownTimer(on);
}

// 1 when the count-down runs and its frame count reached 0.
int cR31CCountDown::isTimeOut()
{
    if (running == 1) {
        int over = Cckpt.isZeroCountDownTimer();

        return over;
    }
    return 0;
}

// The count-down state test: the module build had it inline in the header after the class (a
// linkonce copy follows the room's code; the DOL's is game/mercenaries.cpp's).
// local copy: a header definition changes this unit's allocation (declaration order)
inline int CountDown::checkState(u32 state)
{
    return (m_state & state) ? 1 : 0;
}
