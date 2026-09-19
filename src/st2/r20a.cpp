#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emdoor.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "motion.h"
#include "math_sub.h"
#include "snd.h"
#include "esp.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-0A (D:/Bio4/Prog/r20a.cpp): the locked door with Ashley's shoulder-carry event, the
// treasure boxes and a texture-rendered object.

struct R20aWork {
    cEm* door;           // 0x00
    u8 pad_4[4];
    TexRenderMng* tex;   // 0x08
};

// The work pointer is a struct member: every store through the work reloads it.
struct R20aWorkPtr {
    R20aWork* p;
};

static u8 r20a_texTbl[0x20];
static R20aWorkPtr r20a_work;

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup, r105 idiom).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");

static void r20a_CarryOnShoulder();
static void r20a_CarryOnShoulderEndProc();
static void r20a_AshleyPosCheck();
static void r20a_DoorLockMessage();
void setTexRender();
static void r20a_TreasureBoxOpen(int id);
static void r20a_TreasureBoxOpened(int id);
static void r20a_DoorLock();

// Room init: until Room_flg bit 0 door 0x11 is close-locked and area 2 gives the locked message (the
// shoulder-carry prompt follows when Ashley is with Leon); else areas 2/5 off. The padlocked second door
// task, the refracting render-textured object, five treasure-box item events; area 0xD (with action
// colour) only once Scenario_flg[1] 0x10000000, else area 0 is used.
void R20aInit()
{
#line 42 "D:/Bio4/Prog/r20a.cpp"
    r20a_work.p = (R20aWork*) MEM_CALLOC(sizeof(R20aWork), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        if (getRoomEtcDoor(0x11, &r20a_work.p->door, 1) == 0) {
            r20a_work.p->door = NULL;
        } else {
            cEmDoorSetCloseLock(r20a_work.p->door);
            SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r20a_DoorLockMessage, 0, 1);
        }
    } else {
        SceAtSetEnable(2, 0);
        SceAtSetEnable(5, 0);
    }
    SceExec(0x12, (TaskFunc) r20a_DoorLock, 0, 0, SCE_PRIO_DEF_2, 0);
    setTexRender();
    SceSetItemEvent(7, 0x88, 2, 9, r20a_TreasureBoxOpen, (void (*)()) r20a_TreasureBoxOpened, 0x21, 0);
    SceSetItemEvent(8, 0x90, 3, 0xA, r20a_TreasureBoxOpen, (void (*)()) r20a_TreasureBoxOpened, 0x23, 0);
    SceSetItemEvent(0xC, 0x8A, 4, 0xB, r20a_TreasureBoxOpen, (void (*)()) r20a_TreasureBoxOpened, 0x26, 0);
    SceSetItemEvent(0xB, 0x80, 5, 0xC, r20a_TreasureBoxOpen, (void (*)()) r20a_TreasureBoxOpened, 0x27, 0);
    SceSetItemEvent(0xE, 0x91, 6, 0xD, r20a_TreasureBoxOpen, (void (*)()) r20a_TreasureBoxOpened, 0x2A, 0);
    if (ScfFlagChk(pG, SCF_R206_ASHLEY_RESCUE) == 0) {
        SceAtSetEnable(0xD, 0);
    } else {
        SceAtSetEnable(0, 0);
        SceAtSetEnable(0xD, 1);
        SceAtSetActColor(0xD, 1);
    }
}

// Per-frame room main: nothing.
void R20aMain()
{
}

// Leon lifts Ashley onto his shoulder to unlock the door (both are placed relative to the door).
static void r20a_CarryOnShoulder()
{
    RsfSet(G_ROOM_ID, 0);
    SceAtSetEnable(5, 0);
    SceSleep(1);
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) r20a_CarryOnShoulderEndProc, 0, -1, 1);
    SubCharCtrl(SCC_AUX_MOT, 0);
    SndStrReq(1, 0x27, 0x80000003, 0, 0, 0.0f);
    pPL->setNoSuspend(1);
    pSUB->setNoSuspend(1);
    Vec ofsPl = {-31900.0f, 1977.0f, -58620.0f};
    Vec dPl = {0.0f, 0.0f, -662.5f};
    Vec dSub = {100.49f, 0.0f, -474.38f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    Vec pos;
    Vec pos2;
    Mtx m;
    Vec ang;
    Vec* pa = &ang;
    const f32 ry = PI;

    rot.y = -PI;
    low_RotMatrix(m, &rot);
    TransMatrix(m, &ofsPl);
    PSMTXMultVec(m, &dPl, &pos);
    {
        cPlayer* pl = pPL;
        f32 y = ry;

        pl->setPos(&pos);
        ang.x = 0.0f;
        pa->y = y;
        ang.z = 0.0f;
        pl->setAng(pa);
    }
    low_RotMatrix(m, &pPL->ang);
    TransMatrix(m, &pos);
    PSMTXMultVec(m, &dSub, &pos2);
    {
        cSubChar* sub = pSUB;
        Vec* rot2 = &pPL->ang;

        sub->setPos(&pos2);
        sub->setAng(rot2);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 10, 0, 1, 0);
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 10, 0, 1, 0);
    while (MotionGetState(pPL) != 4) {
        SceSleep(1);
    }
    SceSleep(30);
    RoomSeCall(2, 0, 0, 0, 0);
    SceSleep(30);
    CamCtrl.Comeback(0);
    SceSetEventCancel(0, 0, 0, -1, 1);
    r20a_CarryOnShoulderEndProc();
}

// End of the shoulder-carry event: Leon and Ashley may suspend, Ashley placed behind the door facing
// like Leon and back to follow mode, SceEventEnd, the door becomes a normal door, Scenario_flg[4]
// 0x40000 (unlocked), area 2 off.
static void r20a_CarryOnShoulderEndProc()
{
    Vec pos = {-31142.0f, 1977.0f, -62155.0f};

    pPL->setNoSuspend(0);
    pSUB->setNoSuspend(0);
    {
        cSubChar* sub = pSUB;
        Vec* rot = &pPL->ang;

        sub->setPos(&pos);
        sub->setAng(rot);
    }
    SubCharCtrl(0, 0);
    SceEventEnd(0);
    if (r20a_work.p->door) {
        ((cEmDoor*) r20a_work.p->door)->setNormal();
    }
    ScfFlagOn(pG, SCF_8d);
    SceAtSetEnable(2, 0);
}

// The action button appears once Ashley is within 5000 of Leon.
static void r20a_AshleyPosCheck()
{
    const f32 lim = 25000000.0f;

    if (PSVECSquareDistance(&pPL->pos, &pSUB->pos) < lim) {
        ActBtn.set(0x16, 5, (int) r20a_CarryOnShoulder, 0, 0, 1, 1, 0);
    }
}

// Area 2, the locked door: up-cut message 6/3; with Ashley following (Status_flg[3] 0x04000000) arm the
// distance check on area 5.
static void r20a_DoorLockMessage()
{
    SceUpCut(0, 6, 3, 0);
    if (StaFlagChk(pG, STA_SUB_ASHLEY)) {
        SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r20a_AshleyPosCheck, 0, 1);
    }
}

// The render-textured object 0x1C: a render target blended over it (refraction shader 2).
void setTexRender()
{
    cObj* obj;
    u8* tbl = r20a_texTbl;

    if (GetTexRenderMgr(&r20a_work.p->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r20a_work.p->tex->texId;
        r20a_work.p->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 0, r20a_work.p->tex->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x1C);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
    obj->pModelInfo->color[3] = 0xF0;
    obj->Shader_type = 2;
    obj->Refract_pow = 8;
    obj->Refract_ratio = 0x20;
    obj->invisible_factor = 0.7f;
}

// Item-event opener: chest `id` opens (lid up -X for 0x21, +Z for 0x23/0x26/0x27; 0x2A slides +X 500).
static void r20a_TreasureBoxOpen(int id)
{
    switch ((u32) id) {
    case 0x21:
        OpenBoxMain(OpenBoxPartsUpXM, 0, 0x5B, 0x21, -1, -1);
        break;
    case 0x23:
    case 0x26:
    case 0x27:
        OpenBoxMain(OpenBoxPartsUpZP, 0, 0x5B, id, -1, -1);
        break;
    case 0x2A:
        OpenBoxMain(OpenBoxPosXP500, 0, 0x1B, 0x2A, -1, -1);
        break;
    }
}

// Item-event "already opened": pose chest `id` open (the 0x5B chests still animate: vendor copy).
static void r20a_TreasureBoxOpened(int id)
{
    switch ((u32) id) {
    case 0x21:
        OpenBoxMain(OpenBoxPartsUpXM, 0, 0x5B, 0x21, -1, -1);
        break;
    case 0x23:
    case 0x26:
    case 0x27:
        OpenBoxMain(OpenBoxPartsUpZP, 0, 0x5B, id, -1, -1);
        break;
    case 0x2A:
        OpenBoxMain(OpenBoxPosXP500, 1, 0x1B, 0x2A, -1, -1);
        break;
    }
}

// The second door: locked with the padlock model until the player breaks it.
static void r20a_DoorLock()
{
    cEm* door;

    if (getRoomEtcDoor(0xB, &door, 1)) {
        ((cEmDoor*) door)->setLock(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), 0, 0);
    }
    if (door) {
        while (((cEmDoor*) door)->ckLock()) {
            SceSleep(1);
        }
        ScfFlagOn(pG, SCF_8a);
    }
}
