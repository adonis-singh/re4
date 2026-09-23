#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "scheduler.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emdoor.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "motion.h"
#include "item.h"
#include "snd.h"
#include "sscrn.h"
#include "cam_ctrl.h"
#include "game.h"

// Room 3-0C (D:/Bio4/Prog/r30c.cpp): Ashley locked in the cell, the s00 event that frees her, the
// plane that crashes through the wall (with its cut), the cell door key and the item box.

struct R30cWork {
    cEmDoor* door;          // 0x00  the cell door
    cEmWrap em[2];      // 0x04
    cSubChar* ashley;   // 0x1C  pSUB while she is locked away
    u32 strId;          // 0x20  SndStrReq handle of the plane stream
    ScePrim* shout;     // 0x24  the AshleyShout task
};

// The event model's status word at cModel+0x328 (pl_npc).
struct R30cModView {
    u8 pad_0[0x328];
    u32 flags;
};


static R30cWork* r30c_work;



static void r30c_checkImprisonDoorKeyUse();
static void r30c_checkImprisonDoor();
static void R30cEventS00();
void Evt_R30CS00_Func(Event* e);
static void r30c_EventCut();
static void r30c_EventCutEndProc();
static void r30c_AshleyShout();
static void r31c_AshleyDieCheck();
static void r30c_PlaneMove();
static void r30c_PlaneMoveEndProc(cObj* obj);
void r30c_LinkObjItemAt(int no, cObj* obj);
static void r30c_ItemBoxOpen(int no);
static void r30c_ItemBoxOpened(int no);
static void r30c_StrStart();
static void r30c_StrCheck();

// Room init (Ashley's cell): door 1 takes key item 0x13; until unlocked (Key_flg[0] 0x1000) area 3 =
// the cell door with its key watcher. Until the s00 event (Room_flg bit 0): it is pre-loaded (enemy of
// ESL 0x40), Ashley initialised and locked in the cell (mode 5) with her shout task and, on the first
// visit of the stage, the cell cut on area 5; after it: until the plane crashed (bit 3) area 6 = the
// plane with its stream; the battle stream. One item box; the plane's item area 0x82 only after the crash.
void R30cInit()
{
    // The store's address is computed before the calloc call (a reference bound first): the `li r5`
    // of getRoomEtcDoor then issues before the stw (sched1 slot).
    R30cWork*& wp = r30c_work;
#line 51 "D:/Bio4/Prog/r30c.cpp"
    wp = (R30cWork*) MEM_CALLOC(sizeof(R30cWork), 1, 0xd);
    if (getRoomEtcDoor(1, &r30c_work->door, 1)) {
        r30c_work->door->setKey(0x13);
    }
    if (!KyfFlagChk(pG, KYF_R30C_DOOR)) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r30c_checkImprisonDoor, 0, 1);
        SceExec(0x12, (TaskFunc) r30c_checkImprisonDoorKeyUse, 0, 0, 2, 0);
    }
    EvtMgr.SetFunc("evt_r30cs00_func", (void*) Evt_R30CS00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) R30cEventS00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r30cs00.evd", (u8) GetEmIdFromList(0x40), 0, 0, 0);
        SubCharInit(1, &pPL->pos, pPL->ang.y);
        SubCharCtrl(5, 0);
        if (ItemMgr.num(0x83) != 0 || KyfFlagChk(pG, KYF_R30C_DOOR)) {
            Vec pos = {0.0f, 0.0f, 0.0f};
            Vec ang;
            cSubChar* sub = SUB_CHAR();
            f32 rotY = 0.0f;
            Vec* pa = &ang;

            sub->setPos(&pos);
            ang.x = 0.0f;
            pa->y = rotY;
            ang.z = 0.0f;
            sub->setAng(pa);
            AtariOffV(&pSUB->atari, ~0x100);
            pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 9, 0);
            pSUB->dmg.m_Timer = 0x80;
        } else {
            Vec pos = {5250.0f, 0.0f, -7150.0f};
            Vec ang;
            cSubChar* sub = SUB_CHAR();
            f32 rotY = -1.6f;
            Vec* pa = &ang;

            sub->setPos(&pos);
            ang.x = 0.0f;
            pa->y = rotY;
            ang.z = 0.0f;
            sub->setAng(pa);
            AtariOffV(&pSUB->atari, ~0x100);
            AtariOffV(&pSUB->atari, ~0x200);
            pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 4, 0);
            r30c_work->shout = SceExec(0x12, (TaskFunc) r30c_AshleyShout, 0, 0, 2, 0);
            if (RsfCheck(*(u16*) &pG->stage_no, 1) == 0) {
                SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r30c_EventCut, 0, 1);
            }
        }
        r30c_work->ashley = SUB_CHAR();
        pSUB = 0;
    } else {
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r30c_PlaneMove, 0, 1);
            r30c_work->strId = SndStrReq(1, 0xEF, 0x80000001, 0, 0, 0.0f);
        }
        SceExec(0x12, (TaskFunc) r30c_StrCheck, 0, 0, 2, 0);
    }
    r30c_work->em[0].setEm(0x40, 6, 0, 1, 1);
    r30c_work->em[1].setEm(0x50, 6, 0, 1, 1);
    SceSetItemEvent(7, 0x80, 2, 3, r30c_ItemBoxOpen, r30c_ItemBoxOpened, 0xF, 0);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtSetEnable(0x82, 0);
    } else {
        Vec pos = {0.0f, 0.0f, 0.0f};
        cObj* obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &pos, 0x10, 1);
        void* mot = ROOM_ARC_PTR(pG->pRoom, 0x23);

        obj->motionSet(mot, 0, FcvGetMaxFrame((u16*) mot), 1, 0);
        r30c_LinkObjItemAt(0x82, obj);
    }
}

// Per-frame room main: nothing.
void R30cMain()
{
}

// The cell door opens once the key is used.
static void r30c_checkImprisonDoorKeyUse()
{
    while (ItemMgr.check(0x83) != 1) {
        SceSleep(1);
    }
    KyfFlagOn(pG, KYF_R30C_DOOR);
    SceAtSetEnable(3, 0);
    SceUpCut(1, -1, 1, 0);
}

// Area 3, the cell door: up-cut 0 without the key; with the key (item 0x83) up-cut 3 and the item screen to use it.
static void r30c_checkImprisonDoor()
{
    if (ItemMgr.num(0x83) == 0) {
        SceUpCut(0, -1, 0, 4);
        CamCtrl.Comeback(0);
    } else {
        SceUpCut(3, -1, 0, 4);
        SubScreenOpen(0x80, 1);
    }
}

// The s00 event: Ashley is taken out of the cell.
static void R30cEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        Vec ang;

        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(2, 0);
        SndRoomStrStop(1);
        EvtMgr.EvtReadExec("event/evd/r30cs00.evd", (u8) GetEmIdFromList(0x40), EvtReadFlagPlCheckEvent);
        {
            // pPL read first (its load precedes the pool loads in the stream): the x store then
            // issues before the dying y store.
            cPlayer* pl = pPL;
            f32 rotY = 1.5707964f;

            ang.x = 0.0f;
            ang.y = rotY;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
        StaFlagOn(pG, STA_SUB_ASHLEY);
        pSUB = r30c_work->ashley;
        AtariOnV(&pSUB->atari, 0x100);
        MotionClear(pSUB, 1);
        pSUB->be_flag |= 0x200000;
        SubCharCtrl(4, 0);
        SceSetChapterEnd(0xE, -1);
        if (r30c_work->em[0].isActive()) {
            r30c_work->em[0].destroy();
            setEm(0x56, 6, 1, 1, 1);
        }
        if (r30c_work->em[1].isActive()) {
            r30c_work->em[1].destroy();
            setEm(0x57, 6, 1, 1, 1);
        }
        SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r30c_PlaneMove, 0, 1);
        r30c_work->strId = SndStrReq(1, 0xEF, 0x80000001, 0, 0, 0.0f);
        SndBgmTblSet(0x30C, 1);
        ScfFlagOn(pG, SCF_R30C_SAVE_ASHLEY);
    }
}

// Event r30cs00 callback: the pl0100 model's status flag 0x40 on for cut 0 and off from cut 1.
void Evt_R30CS00_Func(Event* e)
{
    if (e->FuncType == 1) {
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                    ((R30cModView*) mod)->flags |= 0x40;
                }
            }
            break;
        case 1:
            if (e->NowFrame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                    ((R30cModView*) mod)->flags &= ~0x40;
                }
            }
            break;
        }
    }
}

// The camera cut on Ashley in the cell.
static void r30c_EventCut()
{
    RsfSet(G_ROOM_ID, 1);
    SceEventStart(0);
    r30c_work->ashley->setNoSuspend(1);
    r30c_work->em[0].setNoSuspend(1);
    r30c_work->em[1].setNoSuspend(1);
    r30c_work->shout->task->flag |= 2;
    SceSetEventCancel(1, (TaskFunc) r30c_EventCutEndProc, 0, -1, 1);
    CamCtrl.CutCall(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r30c_EventCutEndProc();
}

// End of the cell cut: camera back, Ashley and the two Ganados may suspend, SceEventEnd, the shout
// task resumes, Scenario_flg[2] 0x00080000.
static void r30c_EventCutEndProc()
{
    CamCtrl.Comeback(0);
    r30c_work->ashley->setNoSuspend(0);
    r30c_work->em[0].setNoSuspend(0);
    r30c_work->em[1].setNoSuspend(0);
    SceEventEnd(0);
    r30c_work->shout->task->flag &= ~2;
    ScfFlagOn(pG, SCF_R30C_ASHLEY_SCREAM);
}

// Ashley's shouting in the cell: the sound effects on the motion frames, and the wave once the
// player comes close.
static void r30c_AshleyShout()
{
    if (r30c_work->ashley == 0) {
        return;
    }
    SceExec(0x12, (TaskFunc) r31c_AshleyDieCheck, 0, 0, 2, 0);
    for (;;) {
        switch ((u32) r30c_work->ashley->Motion.Seq_frame) {
        case 0x46:
            RoomSeCall(2, &r30c_work->ashley->getPartsPtr(4)->world, 0, 0, 0);
            break;
        case 0x9:
        case 0x12:
        case 0x1E:
        case 0x2B:
        case 0x3A:
        case 0x44:
        case 0x4F:
        case 0x5A:
        case 0x64:
        case 0x77:
        case 0x85:
        case 0x92:
            RoomSeCall(5, &r30c_work->ashley->getPartsPtr(0xA)->world, 0, 0, 0);
            break;
        }
        if (SceAtHitCheck(9)) {
            r30c_work->ashley->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 3, 0, 0, 0);
            while (!(MotionGetState(r30c_work->ashley) & 4)) {
                SceSleep(1);
            }
            r30c_work->ashley->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 3, 0, 0, 0);
            break;
        }
        SceSleep(1);
    }
}

// Ashley can be killed in the cell: watch her life.
static void r31c_AshleyDieCheck()
{
    for (;;) {
        u32 stat;

        pSUB = r30c_work->ashley;
        stat = SubCharGetStatus();
        pSUB = 0;
        if (!(stat & 1)) {
            pG->ashley_life = 0;
            EmRoutineSetW(r30c_work->ashley, 2, 0, 0, 0);
        }
        if ((s16) pG->ashley_life > 0) {
            SceSleep(1);
        } else {
            break;
        }
    }
    DiedemoExec(0x1E, 2);
}

// The plane crashes through the wall.
static void r30c_PlaneMove()
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    cObj* obj;

    RsfSet(G_ROOM_ID, 3);
    SceEventStart(0);
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &pos, 0x10, 1);
    obj->setNoSuspend(1);
#line 494 "D:/Bio4/Prog/r30c.cpp"
    (obj->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xd));
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 0x201, 0);
    SndStrReq(r30c_work->strId, 2, 0, 0);
    pG->Room_flg[0] &= 0x7FFFFFFF;
    SceSetEventCancel(1, (TaskFunc) r30c_PlaneMoveEndProc, (int) obj, 0, 1);
    while (!(MotionGetState(obj) & 4)) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r30c_PlaneMoveEndProc(obj);
}

// End of the plane crash (also its cancel path, Room_flg[0] bit 31 = snap the plane to its last motion
// frame): camera back, SceEventEnd, the item area 0x82 inside the plane enabled and linked to it, five
// Ganados (list 6) spawn, area 8 = start the battle stream.
static void r30c_PlaneMoveEndProc(cObj* obj)
{
    if (pG->Room_flg[0] & 0x80000000) {
        void* mot = ROOM_ARC_PTR(pG->pRoom, 0x23);

        obj->motionSet(mot, 0, FcvGetMaxFrame((u16*) mot), 1, 0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(0x82, 1);
    r30c_LinkObjItemAt(0x82, obj);
    setEm(0x53, 6, 1, 1, 1);
    setEm(0x54, 6, 1, 1, 1);
    setEm(0x62, 6, 1, 1, 1);
    setEm(0x67, 6, 1, 1, 1);
    setEm(0x68, 6, 1, 1, 1);
    SceAtDataSet_exec(8, 0x12, 0, (TaskFunc) r30c_StrStart, 0, 1);
}

// The item inside the plane: link the item attribute to the plane object.
void r30c_LinkObjItemAt(int no, cObj* obj)
{
    SceAtWork* at = SceAtPtr(no);

    if (at && obj) {
        at->item.pModel = obj;
        obj->LightInfo.EnableMask = (obj->LightInfo.EnableMask | 0x20) & ~0x10;
    }
}

// Item-event opener: the box (objects 0xF/0x10, type 6) swings open.
static void r30c_ItemBoxOpen(int no)
{
    OpenBoxMain(0, 0, 6, 0xF, 0x10, -1);
}

// Item-event "already opened": the box posed open.
static void r30c_ItemBoxOpened(int no)
{
    OpenBoxMain(0, 1, 6, 0xF, 0x10, -1);
}

// Area 8: start the battle-stream watcher.
static void r30c_StrStart()
{
    SceExec(0x12, (TaskFunc) r30c_StrCheck, 0, 0, 2, 0);
}

// Battle music while the plane enemies are alive.
static void r30c_StrCheck()
{
    SceSleep(1);
    if (SceCountEmAlive(0x1F, -1) != 0) {
        SndRoomStrStart(1, 0, 1);
        while (SceCountEmAlive(0x1F, -1) != 0 || SceCountEmAlive(0x25, -1) != 0) {
            SceSleep(1);
        }
        SndRoomStrStop(3);
    }
}
