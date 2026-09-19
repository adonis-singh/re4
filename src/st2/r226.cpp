#include "types.h"
class cObjWep;
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
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "objRobo.h"
#include "em.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "st_mgr_event.h"
#include "act_btn.h"
#include "cockpit.h"
#include "joy.h"
#include "pad.h"
#include "quake.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "math_sub.h"
#include "motion.h"
#include "eprintf.h"
#include "db_log.h"

// Room 2-26 (D:/Bio4/Prog/r226.cpp): the giant statue (cObjRobo) chase - the passage switches, the
// statue's walk through the passage and over the bridge, the button-mash escape and the deaths.

struct R226Work {
    u32 x0;                 // 0x000
    cObjRobo* robo;         // 0x004
    cSat* sat[4];           // 0x008
    cSat* eat[4];           // 0x018  effect pieces (passage switch sides, bridge)
    u32 x28;                // 0x028
    cEmWrap em[22];         // 0x02C
    int hitPoint;           // 0x134  button-mash gauge
    int spdOld;             // 0x138
    int spdNew;             // 0x13C
    int sub;                // 0x140
    int moveTimer;          // 0x144
    Vec camPos;             // 0x148  bridge camera offsets
    Vec camAt;              // 0x154
    f32 dieY;               // 0x160
    u32 str;                // 0x164  SndStrReq handle
    int btnCnt;             // 0x168
    int timer;              // 0x16C
};

// The work pointer is a struct member: every store through the work reloads it.
struct R226WorkPtr {
    R226Work* p;
};

// sce_com.cpp SceElevatorData
struct SceElevatorData {
    s32 dir;
    u32 objId;
    Vec pos;
    Vec plPos;
    Vec plRot;
    s32 cut;
    u16 pad_30;
    u16 seStart;
    u16 pad_34;
    u16 seStop;
    Vec jumpPos;
    Vec jumpRot;
    u16 room;
};

extern "C" void SceElevator(SceElevatorData* d);
// COMPILER-DIFF: 4 -- `int` table entries reach cEmWrap::setEm's s16 parameter unextended
// (`lwzx r4`); ours narrows the load to `lha` through the real prototype.
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");
// MotionMove is called with a second argument by the player routines (the DOL definition ignores it).
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

static R226WorkPtr r226_work;
static Camera r226_cam;

static SceElevatorData r226_elvArrive = {0, 0, {0.0f, 0.0f, 0.0f}, {-3300.0f, 5000.0f, 22200.0f}, {0.0f, 3.14f, 0.0f}, -1, 0, 0xE, 0, 0xF, {80130.0f, 1500.0f, -22530.0f}, {0.0f, -1.6f, 0.0f}, 0x225};
static SceElevatorData r226_elvLeave = {1, 0, {0.0f, 0.0f, 0.0f}, {-3300.0f, 5000.0f, 22200.0f}, {0.0f, 3.14f, 0.0f}, 0xE, 0, 0xD, 0, 0xF, {80130.0f, 1500.0f, -22530.0f}, {0.0f, -1.6f, 0.0f}, 0x225};

int R226EmNo[13] = {0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBF, 0xC0};
int R226EmIdx[14] = {3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x3C};

static int r226_buttonAdj = -2;
static Vec r226_dbgCamPos = {-94427.0f, 6073.0f, -13104.0f};
static Vec r226_dbgCamAt = {-98293.0f, 1889.0f, -16325.0f};
static f32 r226_dbgCamFovy = 50.0f;
static int r226_pushFrame = 40;
static Vec r226_camPosInit = {100.0f, 100.0f, 2000.0f};
static Vec r226_camAtInit = {0.0f, 2000.0f, 0.0f};
static Vec r226_camSpdPos = {30.0f, 30.0f, 40.0f};
static Vec r226_camSpdAt = {20.0f, 0.0f, 0.0f};
static f32 r226_fovyBridge = 85.0f;
static Vec r226_camOfsPos = {100.0f, 100.0f, 2000.0f};
static Vec r226_camOfsAt = {0.0f, 2000.0f, 0.0f};
static f32 r226_fovyPassage = 85.0f;
static f32 r226_fovyDie = 27.0f;
static f32 r226_pillarSpd = 160.0f;

static inline void FSetP(f32& d, f32 v) { d = v; }
static inline void PSetSat(cSat*& d, cSat* v) { d = v; }
static inline void PSetRobo(cObjRobo*& d, cObjRobo* v) { d = v; }
// Collision flag bits cleared through the info's address with the following pG / pPL load kept
// below the store (wep_mod.h AtariFlagsAndV).
static inline void AtariFlagsAndV(cAtariInfo* at, u16 mask) { *(volatile u16*) &at->m_flag &= mask; }

// Position a model from three components (inline owning the Vec).
static inline void setPosXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
}

// Rotate a model from three components (inline owning the Vec).
static inline void setAngXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setAng(&v);
}

// Euclidean distance between two points.
static inline f32 vecDist(Vec* a, Vec* b)
{
    return SQRTF((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
}

static void R226EmSetMain();
static void R226EventRoboWatchMain();
static void R226EventRoboWatchCancel();
void R226EventRoboWatchEnd();
static void R226EventRoboStartMain();
static void R226EventRoboStartMainSub(int id);
static void R226EventRoboStartEnd();
static void R226EventPassageSwitchMain(int side);
static void R226EventPassageSwitchEnd(int side);
static void R226EventRoboWalkPassageStart();
static void R226EventRoboWalkPassageGoal();
static void R226EventTowerLookMain();
static void R226EventTowerLookEnd();
static void R226EventRoboWalkDoorDie();
static void R226EventRoboWalkBridgeStart();
static void R226ContinuePointSet();
int ButtonCount(int* hitPoint, int* spdOld, int* spdNew, int* sub, int div, int lim, int dec, int num, void* data, void** mot);
int R226CalcActiveEmWarp();
static void SceBgmCheck();
static void playerRunMovePassage(cPlayer* pl);
static void playerRunMoveBridge(cPlayer* pl);
void playerRunDieSet(cEm* em, int which);
static void playerRunDiePassage(cPlayer* pl);
static void playerRunDieBridge(cPlayer* pl);
void playerRunCamInitBridge();
void playerRunCamMovePassage(cPlayer* pl, f32 t);
void playerRunCamMoveBridge(cPlayer* pl, f32 t);
void playerRunCamDiePassage(cPlayer* pl);
// COMPILER-DIFF: 1 -- the original's prologue copies `fmr f31,f1` before `mr r28,r6` (FP parameter copy
// before the trailing int one); ours orders the copies by parameter order, so the definition declares
// `dist` before `idx` (same argument registers) under the original mangled name as a C symbol.
extern "C" void playerPillarDownCk__FP8cObjRoboiUlif(cObjRobo* robo, int smdNo, u32 flagNo, f32 dist, int idx);
#define playerPillarDownCk(robo, smdNo, flagNo, idx, dist) playerPillarDownCk__FP8cObjRoboiUlif(robo, smdNo, flagNo, dist, idx)
static void playerPillarDownTask(int smdNo);

// Sets the room's 13 statue-chase enemies from the list (the ones not active yet) and wakes them.
static inline void r226_setEmAll(int noSuspend)
{
    u32 i;

    for (i = 0; i < 13; i++) {
        if (r226_work.p->em[R226EmIdx[i]].isActive() == 0) {
            cEmWrapSetEmI(&r226_work.p->em[R226EmIdx[i]], R226EmNo[i], -1, 0, 1, 1);
        }
        r226_work.p->em[R226EmIdx[i]].setNoSuspend(noSuspend);
    }
}

// Room init (the giant Salazar statue chase): JumpPoint 1 clears every room flag then presets the
// switches / statue-awake flags. Area 0 = the elevator out (SceElevator leave data); arriving by the
// elevator plays its arrival. Statue state per flags: awake (bit 9) -> the cObjRobo is created and, until
// the passage walk is done (bit 13), area 5 = the walk start, 0x22 = the tower look, 0x24 = continue
// point, the enemies; else the two passage switch areas 0x12/0x13 (bits 7/8) and area 0x17 = the statue
// waking; area 0x18 = the statue watching once (bit 10); the chase BGM task.
void R226Init()
{
    cEm* door;
    u32 i;
    cObj* o;

    R226Work*& wp = r226_work.p;
#line 99 "D:/Bio4/Prog/r226.cpp"
    wp = (R226Work*) MEM_CALLOC(sizeof(R226Work), 1, 0xd);
    PSetRobo(wp->robo, NULL);
    if (pG->JumpPoint == 1) {
        int n;

        for (n = 0; n < 32; n++) {
            RsfClear(G_ROOM_ID, n);
        }
        RsfSet(G_ROOM_ID, 9);
        RsfSet(G_ROOM_ID, 7);
        RsfSet(G_ROOM_ID, 8);
    }
    {
        int id = GetEmIdFromList(0xB9);

        EmReadSearch((u8) id, 0, 0);
    }
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) SceElevator, &r226_elvLeave, 1);
    SceAtSetActColor(0, 1);
    if (!(SysFlagChk(pG, SYS_CONTINUE) && RsfCheck(G_ROOM_ID, 17))) {
        if (!SysFlagChk(pG, SYS_LOAD_GAME)) {
            if (pG->room_id_prev == 0x225) {
                SceExec(0x12, (TaskFunc) SceElevator, (int) &r226_elvArrive, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    getRoomEtcDoor(0xA, &door, 1);
    if (door) {
        ((cEmDoor*) door)->setLock(ROOM_ARC_PTR(pG->pRoom, 0x5A), ROOM_ARC_PTR(pG->pRoom, 0x5B), 1, 0);
    }
    Vec zeroPos = {0.0f, 0.0f, 0.0f};
    Vec zeroRot = {0.0f, 0.0f, 0.0f};
    {
        for (i = 0; i < 4; i++) {
            r226_work.p->sat[i] = NULL;
            r226_work.p->eat[i] = NULL;
        }
        PSetSat(r226_work.p->sat[0], SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zeroPos, &zeroRot, 1));
        PSetSat(r226_work.p->eat[0], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zeroPos, &zeroRot, 1));
        PSetSat(r226_work.p->eat[1], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zeroPos, &zeroRot, 4));
        PSetSat(r226_work.p->eat[2], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zeroPos, &zeroRot, 5));
        PSetSat(r226_work.p->eat[3], EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zeroPos, &zeroRot, 3));
        SmdSetTrans(0x47, 0);
        if (RsfCheck(G_ROOM_ID, 13) == 0) {
            SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) R226EventRoboWalkPassageStart, 0, 1);
            SceAtDataSet_exec(0x22, SCE_LEVEL10, 0, (TaskFunc) R226EventTowerLookMain, 0, 1);
            SceAtDataSet_exec(0x24, SCE_LEVEL10, 0, (TaskFunc) R226ContinuePointSet, 0, 1);
            SceAtSetEnable(0x11, 0);
            SceAtSetEnable(0x16, 0);
            Vec pos = {1180.0f, 1000.0f, -16560.0f};
            Vec rot = {0.0f, -1.5707964f, 0.0f};
            r226_work.p->robo = (cObjRobo*) SetObjRobo(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot);
            if (r226_work.p->robo == NULL) {
                pLog->err(0, 0, "R226Init : obm5000 set failed");
                return;
            }
            if (r226_work.p->eat[3]) {
                r226_work.p->eat[3]->m_Flag |= 4;
            }
        } else {
            SmdSetTrans(0x35, 0);
            if (r226_work.p->sat[0]) {
                r226_work.p->sat[0]->m_Flag &= ~4;
            }
            if (r226_work.p->eat[0]) {
                r226_work.p->eat[0]->m_Flag &= ~4;
            }
            if (r226_work.p->eat[1]) {
                r226_work.p->eat[1]->m_Flag |= 4;
            }
            if (r226_work.p->eat[2]) {
                r226_work.p->eat[2]->m_Flag |= 4;
            }
            if (r226_work.p->eat[3]) {
                r226_work.p->eat[3]->m_Flag &= ~4;
            }
            SmdSetTrans(0x1B, 0);
            SmdSetTrans(0x1C, 0);
            SmdSetTrans(0x43, 0);
            SmdSetTrans(0x44, 0);
            SmdSetTrans(0x4F, 0);
            SmdSetTrans(0x50, 0);
            SmdSetTrans(0x51, 0);
            SmdSetTrans(0x52, 0);
            EstSet(0, -1, 0, 0, 1, 0x10, 1, 0, 0, 0);
            EstSet(0, -1, 0, 0, 1, 0xE, 1, 0, 0, 0);
            if (r226_work.p->eat[3]) {
                r226_work.p->eat[3]->m_Flag &= ~4;
            }
        }
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtDataSet_exec(0x12, SCE_LEVEL10, 0, (TaskFunc) R226EventPassageSwitchMain, 0, 1);
        o = SmdGetObjPtr(0x3D);
        if (o) {
            setPosXYZ(o, o->pos.x, -1000.0f, o->pos.z);
        }
        SceAtSetEnable(0x14, 1);
        if (r226_work.p->eat[1]) {
            r226_work.p->eat[1]->m_Flag &= ~4;
        }
    } else {
        o = SmdGetObjPtr(0x3B);
        if (o) {
            setAngXYZ(o, o->ang.x, o->ang.y, 1.5707964f);
        }
        SceAtSetEnable(0x14, 0);
        if (r226_work.p->eat[1]) {
            r226_work.p->eat[1]->m_Flag |= 4;
        }
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        SceAtDataSet_exec(0x13, SCE_LEVEL10, 0, (TaskFunc) R226EventPassageSwitchMain, (void*) 1, 1);
        o = SmdGetObjPtr(0x3E);
        if (o) {
            setPosXYZ(o, o->pos.x, -1000.0f, o->pos.z);
        }
        SceAtSetEnable(0x15, 1);
        if (r226_work.p->eat[2]) {
            r226_work.p->eat[2]->m_Flag &= ~4;
        }
    } else {
        o = SmdGetObjPtr(0x3C);
        if (o) {
            setAngXYZ(o, o->ang.x, o->ang.y, -1.5707964f);
        }
        SceAtSetEnable(0x15, 0);
        if (r226_work.p->eat[2]) {
            r226_work.p->eat[2]->m_Flag |= 4;
        }
        o = SmdGetObjPtr(0x4C);
        if (o) {
            setPosXYZ(o, -1935.0f, o->pos.y, o->pos.z);
        }
        SceAtSetEnable(0x23, 0);
    }
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtDataSet_exec(0x17, SCE_LEVEL10, 0, (TaskFunc) R226EventRoboStartMain, 0, 1);
        o = SmdGetObjPtr(0x3D);
        if (o) {
            setPosXYZ(o, o->pos.x, 1000.0f, o->pos.z);
        }
        o = SmdGetObjPtr(0x3E);
        if (o) {
            setPosXYZ(o, o->pos.x, 1000.0f, o->pos.z);
        }
        o = SmdGetObjPtr(0x3B);
        if (o) {
            setAngXYZ(o, o->ang.x, o->ang.y, 1.5707964f);
        }
        o = SmdGetObjPtr(0x3C);
        if (o) {
            setAngXYZ(o, o->ang.x, o->ang.y, -1.5707964f);
        }
    } else {
        if (RsfCheck(G_ROOM_ID, 13) == 0) {
            SceExec(0x12, (TaskFunc) R226EmSetMain, 0, 0, SCE_PRIO_DEF_2, 0);
            SndRoomStrStart(1, 0, 1);
            pG->Room_flg[1] |= 0x10000000;
        }
    }
    if (RsfCheck(G_ROOM_ID, 10) == 0) {
        SceAtDataSet_exec(0x18, SCE_LEVEL10, 0, (TaskFunc) R226EventRoboWatchMain, 0, 1);
    }
    SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    r226_work.p->moveTimer = 0;
    playerRunCamInitBridge();
    U32Set(r226_work.p->str, 0);
    ScfFlagOn(pG, SCF_R226_IN);
}

// Per frame: after the door opened (Room_flg[1] 0x08000000) and before the bridge (bit 14) a 600-frame
// MoveTimer kills the dawdling player (R226EventRoboWalkDoorDie); debug pad 2 buttons replay the
// switch events and the statue walk.
void R226Main()
{
    if ((pG->Room_flg[1] & 0x08000000) && RsfCheck(G_ROOM_ID, 14) == 0) {
        r226_work.p->moveTimer++;
        eprintf(0x40, 0x10, 0, 0, "MoveTimer:[%d]", r226_work.p->moveTimer / 30);
        if (r226_work.p->moveTimer > 599) {
            r226_work.p->moveTimer = 0;
            SceExec(0x12, (TaskFunc) R226EventRoboWalkDoorDie, 0, 4, SCE_PRIO_DEF_2, 0);
        }
    }
    if (Joy[2].trg & 0x100) {
        SceExec(0x12, (TaskFunc) R226EventPassageSwitchMain, 0, 0, SCE_PRIO_DEF_2, 0);
        RsfClear(G_ROOM_ID, 7);
    }
    if (Joy[2].trg & 0x200) {
        SceExec(0x12, (TaskFunc) R226EventPassageSwitchMain, 1, 0, SCE_PRIO_DEF_2, 0);
        RsfClear(G_ROOM_ID, 8);
    }
    if (Joy[2].trg & 0x800) {
        SceExec(0x12, (TaskFunc) R226EventRoboStartMain, 0, 0, SCE_PRIO_DEF_2, 0);
        RsfClear(G_ROOM_ID, 9);
    }
    if (FlagChkSign(pG->Room_flg, 32) && !FlagChkSign(pG->Room_flg, 34)) {
        pG->Room_flg[1] |= 0x20000000;
        playerRunDieSet(0, 0);
    } else if (FlagChkSign(pG->Room_flg, 33) && !FlagChkSign(pG->Room_flg, 34)) {
        pG->Room_flg[1] |= 0x20000000;
        playerRunDieSet(0, 1);
    }
}

// One frame in: set the 13 chase enemies from the list.
static void R226EmSetMain()
{
    SceSleep(1);
    r226_setEmAll(0);
}

// Task: the statue turns to watch the player (event cut 12).
static void R226EventRoboWatchMain()
{
    cObjRobo* robo = r226_work.p->robo;

    if (pPL->stat & 0x100) {
        return;
    }
    if (RsfCheck(G_ROOM_ID, 10)) {
        return;
    }
    RsfSet(G_ROOM_ID, 10);
    SceAtSetEnable(0x18, 0);
    SceEventStart(1);
    cObjRoboSetBeginEvent(robo, 0);
    r226_work.p->str = SndStrReq(0, 0x16, 0x80000003, 0, 0, 0.0f);
    SceSetEventCancel(1, (TaskFunc) R226EventRoboWatchCancel, 0, -1, 1);
    CamCtrl.CutCall(0xC);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R226EventRoboWatchEnd();
}

// Cancel path of the statue-watch cut: stop its stream, then the common end.
static void R226EventRoboWatchCancel()
{
    if (r226_work.p->str) {
        SndStrReq(r226_work.p->str, 8, 0, 0);
        r226_work.p->str = 0;
    }
    R226EventRoboWatchEnd();
}

// End of the statue-watch cut: the statue leaves event mode, camera back, SceEventEnd, task exit.
void R226EventRoboWatchEnd()
{
    cObjRoboSetEndEvent(r226_work.p->robo, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Task: the statue comes alive (event cuts 9, 8, 10).
static void R226EventRoboStartMain()
{
    cObjRobo* robo = r226_work.p->robo;
    cObj* o;
    int i;

    if (RsfCheck(G_ROOM_ID, 9)) {
        return;
    }
    RsfSet(G_ROOM_ID, 9);
    SceAtSetEnable(0x17, 0);
    o = SmdGetObjPtr(0x3D);
    if (o) {
        setPosXYZ(o, o->pos.x, 1000.0f, o->pos.z);
    }
    o = SmdGetObjPtr(0x3E);
    if (o) {
        setPosXYZ(o, o->pos.x, 1000.0f, o->pos.z);
    }
    SceEventStart(0);
    cObjRoboSetBeginEvent(robo, 0);
    SceSetEventCancel(1, (TaskFunc) R226EventRoboStartEnd, 0, -1, 1);
    CamCtrl.CutCall(9);
    if (r226_work.p->em[9].isActive() == 0) {
        r226_work.p->em[9].setEm(0xB9, -1, 1, 1, 1);
    }
    Vec v = {-3300.0f, 5000.0f, -31650.0f};
    r226_work.p->em[9].setGoto(&v, 1);
    r226_work.p->em[9].setNoSuspend(1);
    for (i = 0; i < 90; i++) {
        if (r226_work.p->em[9].ckGoto() != 1) {
            break;
        }
        SceSleep(1);
    }
    SndRoomStrStart(1, 0, 1);
    pG->Room_flg[1] |= 0x10000000;
    o = SmdGetObjPtr(0x3C);
    if (o) {
        SndCall(6, 0, &o->pos, 0, 0, 0);
        do {
            setAngXYZ(o, o->ang.x, o->ang.y, o->ang.z + 0.06981317f);
            if (o->ang.z >= 0.0f) {
                break;
            }
            SceSleep(1);
        } while (1);
        // COMPILER-DIFF: #12 -- the LOOP_END note ends cse1's AROUND path so the 0.0 below is
        // reloaded from the pool instead of reusing the loop compare's register.
        do { } while (0);
        setAngXYZ(o, o->ang.x, o->ang.y, 0.0f);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(8);
    EstSet(0, -1, 0, 0, 1, 0x1F, 1, 2, 0, 0);
    r226_work.p->str = SndStrPlayBlock(1, 0x2B, 0.0f);
    SceExec(0x12, (TaskFunc) R226EventRoboStartMainSub, 0x3D, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(30);
    SceExec(0x12, (TaskFunc) R226EventRoboStartMainSub, 0x3E, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(60);
    SceSleep(10);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r226_setEmAll(1);
    MotionSetCore(robo, &robo->Motion, ROOM_ARC_PTR(pG->pRoom, 0x43), 0, 0, 1, 0);
    CamCtrl.CutCall(0xA);
    while (MotionGetState(robo) == 0) {
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R226EventRoboStartEnd();
}

// Task: one passage switch object sinks into the floor over 60 frames.
static void R226EventRoboStartMainSub(int id)
{
    cObj* o = SmdGetObjPtr(id);
    int i;

    if (o) {
        for (i = 0; i < 60; i++) {
            setPosXYZ(o, o->pos.x, (f32) (-i * 2000 / 60 + 1000), o->pos.z);
            SceSleep(1);
        }
    }
}

// End of the statue-awakening cutscene (also its cancel path): stream stopped, the chase BGM started
// once (Room_flg[1] 0x10000000), the switch objects sunk, the statue put in its walking state and the
// passage areas armed.
static void R226EventRoboStartEnd()
{
    cObjRobo* robo = r226_work.p->robo;
    RoboWork* rw = &robo->robo;
    cObj* o;

    if (r226_work.p->str) {
        SndStrReq(r226_work.p->str, 8, 0, 0);
        r226_work.p->str = 0;
    }
    if (!(pG->Room_flg[1] & 0x10000000)) {
        SndRoomStrStart(1, 0, 1);
        pG->Room_flg[1] |= 0x10000000;
    }
    o = SmdGetObjPtr(0x3D);
    if (o) {
        setPosXYZ(o, o->pos.x, -1000.0f, o->pos.z);
    }
    o = SmdGetObjPtr(0x3E);
    if (o) {
        setPosXYZ(o, o->pos.x, -1000.0f, o->pos.z);
    }
    o = SmdGetObjPtr(0x3B);
    if (o) {
        setAngXYZ(o, o->ang.x, o->ang.y, -0.0f);
    }
    o = SmdGetObjPtr(0x3C);
    if (o) {
        setAngXYZ(o, o->ang.x, o->ang.y, 0.0f);
    }
    r226_setEmAll(0);
    SceExec(0x12, (TaskFunc) R226EmSetMain, 0, 0, SCE_PRIO_DEF_2, 0);
    rw->r_no_0 = 1;
    rw->step = 0;
    cObjRoboSetEndEvent(robo, 0);
    SceEventEnd(0);
    SceExit();
}

// Task: one of the two passage switches (side 0 / 1): the lever turns, the gate sinks, and on
// side 1 the debug camera shows the far gate slide open.
static void R226EventPassageSwitchMain(int side)
{
    int flagNo;
    int atNo;
    int objAng;
    int objPos;
    int cut;
    s8 cut2;
    u8 estNo;
    int cutX;
    int estX;
    cObj* o;

    if (side == 0) {
        flagNo = 7;
        atNo = 0x12;
        objAng = 0x3B;
        objPos = 0x3D;
        cut = 7;
        cut2 = 8;
        estNo = 0x1E;
    } else {
        flagNo = 8;
        atNo = 0x13;
        objAng = 0x3C;
        objPos = 0x3E;
        cut = 6;
        cut2 = 8;
        estNo = 29;
    }
    if (RsfCheck(G_ROOM_ID, flagNo)) {
        return;
    }
    RsfSet(G_ROOM_ID, flagNo);
    // COMPILER-DIFF: 2 -- the original sign-/zero-extends the narrow locals here (`extsb`, `clrlwi 24`)
    // although both arms set them to constants; the int copies make the conversions real.
    {
        int c2 = cut2;
        int e = estNo;

        cutX = (s8) c2;
        estX = (u8) e;
    }
    SceAtSetEnable(atNo, 0);
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) R226EventPassageSwitchEnd, side, -1, 1);
    CamCtrl.CutCall(cut);
    o = SmdGetObjPtr(objAng);
    if (o) {
        SndCall(6, 0, &o->pos, 0, 0, 0);
        while (1) {
            if (side == 0) {
                setAngXYZ(o, o->ang.x, o->ang.y, o->ang.z + 0.06981317f);
                if (o->ang.z >= 1.5707964f) {
                    setAngXYZ(o, o->ang.x, o->ang.y, 1.5707964f);
                    break;
                }
            } else {
                setAngXYZ(o, o->ang.x, o->ang.y, o->ang.z - 0.06981317f);
                if (o->ang.z <= -1.5707964f) {
                    setAngXYZ(o, o->ang.x, o->ang.y, -1.5707964f);
                    break;
                }
            }
            SceSleep(1);
        }
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(cutX);
    EstSet(0, -1, 0, 0, 1, estX, 1, 2, 0, 0);
    o = SmdGetObjPtr(objPos);
    if (o) {
        for (int i = 0; i < 60; i++) {
            if (i == 0x1C) {
                SndCall(6, 6, &o->pos, 0, 0, 0);
            }
            setPosXYZ(o, o->pos.x, (f32) (i * 2000 / 60 - 1000), o->pos.z);
            SceSleep(1);
        }
    }
    SceSleep(10);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (side == 1) {
        Vec pos = {240.0f, 6731.0f, -33417.0f};
        Vec at = {-3013.0f, 6860.0f, -26920.0f};
        f32 fovy = 50.0f;

        o = SmdGetObjPtr(0x4C);
        if (o) {
            SndCall(6, 0xC, &o->pos, 0, 0, 0);
            for (int i = 0; i < 60; i++) {
                SceCamMove(&pos, &at, fovy);
                setPosXYZ(o, -4225.0f + (f32) i * 2290.0f / 60.0f, o->pos.y, o->pos.z);
                SceSleep(1);
            }
        }
        for (int i = 0; i < 10; i++) {
            SceCamMove(&pos, &at, fovy);
            SceSleep(1);
        }
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R226EventPassageSwitchEnd(side);
}

// End of passage switch `side` (also its cancel path): the switch object sunk / turned, its area off,
// the far gate's effect piece and collision swapped, the room flag (7 / 8) set.
static void R226EventPassageSwitchEnd(int side)
{
    int atNo;
    int objAng;
    int objPos;
    int eatNo;
    cObj* o;

    if (side == 0) {
        atNo = 0x14;
        objAng = 0x3B;
        objPos = 0x3D;
        eatNo = 1;
    } else {
        atNo = 0x15;
        objAng = 0x3C;
        objPos = 0x3E;
        eatNo = 2;
    }
    o = SmdGetObjPtr(objPos);
    if (o) {
        setPosXYZ(o, o->pos.x, 1000.0f, o->pos.z);
    }
    o = SmdGetObjPtr(objAng);
    if (o) {
        if (side == 0) {
            setAngXYZ(o, o->ang.x, o->ang.y, 1.5707964f);
        } else {
            setAngXYZ(o, o->ang.x, o->ang.y, -1.5707964f);
        }
    }
    SceAtSetEnable(atNo, 0);
    if (r226_work.p->eat[eatNo]) {
        r226_work.p->eat[eatNo]->m_Flag |= 4;
    }
    if (side == 1) {
        o = SmdGetObjPtr(0x4C);
        if (o) {
            setPosXYZ(o, -1935.0f, o->pos.y, o->pos.z);
            SceAtSetEnable(0x23, 0);
        }
    }
    if (side == 0) {
        if (RsfCheck(G_ROOM_ID, 15) == 0) {
            if (R226CalcActiveEmWarp() <= 4) {
                RsfSet(G_ROOM_ID, 15);
                r226_work.p->em[16].setEm(0xC3, -1, 0, 1, 1);
                r226_work.p->em[17].setEm(0xC4, -1, 0, 1, 1);
                r226_work.p->em[18].setEm(0xC5, -1, 0, 1, 1);
                r226_work.p->em[16].setFlag(1);
                r226_work.p->em[17].setFlag(1);
                r226_work.p->em[18].setFlag(1);
            }
        }
    } else {
        if (RsfCheck(G_ROOM_ID, 11) == 0) {
            RsfSet(G_ROOM_ID, 11);
            r226_work.p->em[0].setEm(0xAE, -1, 0, 1, 1);
            r226_work.p->em[1].setEm(0xAF, -1, 0, 1, 1);
            r226_work.p->em[2].setEm(0xB0, -1, 0, 1, 1);
            r226_work.p->em[0].setFlag(1);
            r226_work.p->em[1].setFlag(1);
            r226_work.p->em[2].setFlag(1);
        }
        if (RsfCheck(G_ROOM_ID, 16) == 0) {
            if (R226CalcActiveEmWarp() <= 4) {
                RsfSet(G_ROOM_ID, 16);
                r226_work.p->em[19].setEm(0xAA, -1, 0, 1, 1);
                r226_work.p->em[20].setEm(0xAB, -1, 0, 1, 1);
                r226_work.p->em[21].setEm(0xAC, -1, 0, 1, 1);
                r226_work.p->em[19].setFlag(1);
                r226_work.p->em[20].setFlag(1);
                r226_work.p->em[21].setFlag(1);
            }
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Task: both switches thrown - the statue starts walking through the passage.
static void R226EventRoboWalkPassageStart()
{
    cObjRobo* robo = r226_work.p->robo;
    RoboWork* rw = &robo->robo;
    int i;

    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        return;
    }
    if (RsfCheck(G_ROOM_ID, 8) == 0) {
        return;
    }
    if (RsfCheck(G_ROOM_ID, 2)) {
        return;
    }
    RsfSet(G_ROOM_ID, 2);
    SceAtSetEnable(5, 0);
    SceEventStart(0);
    SceDestroyEm(0x10, 0x20);
    SndRoomStrStop(3);
    SndBgmTblSet(0x226, 1);
    SndRoomStrStart(1, 0, 1);
    SceSleep(2);
    SmdSetTrans(0x35, 0);
    EstSet(0, -1, 0, 0, 1, 2, 1, 2, 0, 0);
    EstSet((int) robo, -1, 0, 0, 1, 0xA, 1, 2, 0, 0);
    if (r226_work.p->sat[0]) {
        r226_work.p->sat[0]->m_Flag &= ~4;
    }
    if (r226_work.p->eat[0]) {
        r226_work.p->eat[0]->m_Flag &= ~4;
    }
    cObjRoboSetBeginEvent(robo, 0);
    i = 0;
    MotionSetCore(robo, &robo->Motion, ROOM_ARC_PTR(pG->pRoom, 0x39), 0, 0, 1, 0);
    while (MotionGetState(robo) == 0) {
        if (i++ == 0x2C) {
            SndCall(6, 9, &robo->pos, 0, 0, 0);
        }
        SceSleep(1);
    }
    SetPlDamage((cEm*) robo, playerRunMovePassage);
    cObjRoboSetEndEvent(robo, 0);
    rw->r_no_0 = 2;
    rw->step = 0;
    SceEventEnd(0);
    SceExit();
}

// Task: the statue reached the end of the passage: the player jumps, the statue waits at the door.
static void R226EventRoboWalkPassageGoal()
{
    cObjRobo* robo = r226_work.p->robo;
    RoboWork* rw = &robo->robo;

    if (RsfCheck(G_ROOM_ID, 3)) {
        return;
    }
    RsfSet(G_ROOM_ID, 3);
    SceEventStart(0);
    ((cUnitEventView*) pPL)->beginEvent(0);
    pPL->setNoSuspend(1);
    cObjRoboSetBeginEvent(robo, 0);
    setPosXYZ(pPL, -57500.0f, 1000.0f, -16400.0f);
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x38), 0, 3, 1, 0);
    while (MotionGetState(pPL) == 0) {
        if (pPL->frame > 11.7f && pPL->frame < 12.3f) {
            EstSet(0, -1, &pPL->pos, 0, 3, 0x13, 0, 0, 0, 0);
        }
        SceSleep(1);
    }
    setPosXYZ(robo, -30000.0f, 1000.0f, -15089.0f);
    cObjRoboSetEndEvent(robo, 0);
    rw->r_no_0 = 2;
    rw->step = 0;
    SceAtSetEnable(0x11, 1);
    SceEventEnd(0);
    SceExit();
}

// Task: the player looks up at the tower once the door is open (event cut 15).
static void R226EventTowerLookMain()
{
    cEm* door;

    if (RsfCheck(G_ROOM_ID, 14)) {
        return;
    }
    RsfSet(G_ROOM_ID, 14);
    SceAtSetEnable(0x22, 0);
    getRoomEtcDoor(0xA, &door, 1);
    while (!(door->flag & 0x10000000)) {
        SceSleep(1);
    }
    SceEventStart(0);
    SceSetEventCancel(1, (TaskFunc) R226EventTowerLookEnd, 0, -1, 1);
    SndRoomStrStop(3);
    SndBgmTblSet(0x226, 2);
    SndRoomStrStart(1, 0, 1);
    CamCtrl.CutCall(0xF);
    EstSet(0, -1, 0, 0, 1, 0x3F, 1, 2, 0, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R226EventTowerLookEnd();
}

// End of the tower look: start the statue's bridge walk, SceEventEnd, task exit.
static void R226EventTowerLookEnd()
{
    SceExec(0x12, (TaskFunc) R226EventRoboWalkBridgeStart, 0, 0, SCE_PRIO_DEF_2, 0);
    SceEventEnd(0);
    SceExit();
}

// Task: the statue breaks through the door and crushes the player (the MoveTimer death).
static void R226EventRoboWalkDoorDie()
{
    cObjRobo* robo = r226_work.p->robo;
    Vec pos;

    SceEventStart(0);
    cObjRoboSetBeginEvent(robo, 0);
    ((cUnitEventView*) pPL)->beginEvent(0);
    pPL->setNoSuspend(1);
    pos.x = -50000.0f;
    pos.y = 1200.0f;
    pos.z = -16499.0f;
    robo->setPos(&pos);
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x6D), 0, 0, 0x201, 0);
    MotionSetCore(robo, &robo->Motion, ROOM_ARC_PTR(pG->pRoom, 0x6E), 0, 0, 1, 0);
    EstSet((int) robo, -1, 0, 0, 1, 0x22, 1, 0, 0, 0);
    setAngXYZ(robo, 0.0f, -1.5707964f, 0.0f);
    pos.x = robo->pos.x - 9062.5f;
    pos.y = robo->pos.y + 0.0f;
    pos.z = robo->pos.z + 1042.95f;
    pPL->setPos(&pos);
    setAngXYZ(pPL, 0.0f, -1.5707964f, 0.0f);
    pG->pl_life = 0;
    while (1) {
        SceSleep(1);
    }
}

// Task: the statue starts across the bridge after the player.
static void R226EventRoboWalkBridgeStart()
{
    cObjRobo* robo = r226_work.p->robo;
    RoboWork* rw = &robo->robo;
    int i;

    if (RsfCheck(G_ROOM_ID, 5)) {
        return;
    }
    RsfSet(G_ROOM_ID, 5);
    SceAtSetEnable(0xF, 0);
    SceEventStart(0);
    cObjRoboSetBeginEvent(robo, 0);
    setPosXYZ(robo, -53020.0f, 1200.0f, -16731.0f);
    SmdSetTrans(0x1B, 0);
    SmdSetTrans(0x1C, 0);
    MotionSetCore(robo, &robo->Motion, ROOM_ARC_PTR(pG->pRoom, 0x5D), (int) ROOM_ARC_PTR(pG->pRoom, 0x68), 0, 1, 0);
    EffectEspDelete(1, 4, 0, 0);
    EffectEspgenDelete(1, 4, 0);
    EffectEfmDelete(1, 4, 0);
    EstSet(0, -1, 0, 0, 1, 0xB, 1, 2, 0, 0);
    i = 0;
    while (MotionGetState(robo) == 0) {
        if (i++ == 9) {
            SndCall(6, 9, &robo->pos, 0, 0, 0);
        }
        robo->WalkSequence(robo, 1);
        SceSleep(1);
    }
    SetPlDamage((cEm*) robo, playerRunMoveBridge);
    cObjRoboSetEndEvent(robo, 0);
    rw->r_no_0 = 4;
    rw->step = 0;
    {
        int smd[6] = {0x43, 0x44, 0x4F, 0x50, 0x51, 0x52};
        int n = 6;

        for (i = 0; i < n; i++) {
            SmdSetTrans(smd[i], 0);
        }
    }
    EstSet(0, -1, 0, 0, 1, 0x10, 1, 0, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x11, 0x2001, 3, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x12, 0x2001, 4, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x13, 0x2001, 5, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x14, 0x2001, 6, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x15, 0x2001, 7, 0, 0);
    EstSet(0, -1, 0, 0, 1, 0x16, 0x2001, 8, 0, 0);
    SceEventEnd(0);
    SceExit();
}

// Dead-stripped debug helper: only its constant pool ({10000, 0, -500}, 10, 0) survives after
// R226EventRoboWalkBridgeStart's pool; the debug camera statics above are its data.
static void r226_dbgCam()
{
    Vec ofs = {10000.0f, 0.0f, -500.0f};
    Vec pos;

    pos.x = r226_dbgCamPos.x + ofs.x;
    pos.y = r226_dbgCamPos.y + ofs.y;
    pos.z = r226_dbgCamPos.z + ofs.z;
    r226_dbgCamFovy += 10.0f;
    if (r226_dbgCamFovy > 0.0f) {
        SceCamMove(&pos, &r226_dbgCamAt, r226_dbgCamFovy);
    }
}

// Area 0x24: with both switches thrown (bits 7/8), once (bit 17) disable the area and autosave.
static void R226ContinuePointSet()
{
    if (RsfCheck(G_ROOM_ID, 7) && RsfCheck(G_ROOM_ID, 8) && RsfCheck(G_ROOM_ID, 17) == 0) {
        RsfSet(G_ROOM_ID, 17);
        SceAtSetEnable(0x24, 0);
        GameSaveSave(&GameSave, pSaveData, -1);
    }
}

// Button-mash gauge: `sub` counts frames, every `lim` frames the gauge drops by `dec`; the run speed
// is gauge / div (capped at num - 1) and selects the motion; the A button adds the frames back.
int ButtonCount(int* hitPoint, int* spdOld, int* spdNew, int* sub, int div, int lim, int dec, int num, void* data, void** mot)
{
    cPlayer* pl = pPL;
    int ret = 0;
    int limit = lim;

    if (pG->Game_level <= 2) {
        limit += 4;
    }
    if (pG->Game_level > 7) {
        limit = lim + r226_buttonAdj;
    }
    (*sub)++;
    if (*sub > limit) {
        *sub = limit;
        *hitPoint -= dec;
        if (*hitPoint < 0) {
            *hitPoint = 0;
        }
    }
    *spdNew = *hitPoint / div;
    if (*spdNew > num - 1) {
        *spdNew = num - 1;
    }
    if (*spdOld != *spdNew) {
        void* m;
        u32 max;
        f32 rate;
        u32 frame;

        *spdOld = *spdNew;
        rate = pl->frame / (f32) pl->frameMax;
        m = mot[*spdNew];
        max = *(u16*) m;
        frame = (u32) ((f32) max * rate);
        frame++;
        if (frame >= max) {
            frame = 0;
        }
        MotionSetCore(pl, &pl->Motion, data, (int) m, pl->motHokanCnt, 5, (u16) frame);
        ret = 1;
    }
    if (Key.trg & 0x80000) {
        *hitPoint += *sub;
        *sub = 0;
        if (*hitPoint > num * div - 1) {
            *hitPoint = num * div - 1;
        }
    }
    return ret;
}

// Number of the 22 chase enemies currently active.
int R226CalcActiveEmWarp()
{
    int n = 0;
    int i;

    for (i = 0; i < 22; i++) {
        if (r226_work.p->em[i].isActive() == 1) {
            n++;
        }
    }
    return n;
}

// Task: the chase BGM follows the player being seen, until the statue walk starts.
static void SceBgmCheck()
{
    int on = 0;

    while (1) {
        if (RsfCheck(G_ROOM_ID, 2)) {
            break;
        }
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}

// SetPlDamage routine: the player runs through the passage ahead of the statue.
static void playerRunMovePassage(cPlayer* pl)
{
    cObjRobo* robo = (cObjRobo*) pl->pEmCatch;
    RoboWork* rw = &robo->robo;
    void* data = ROOM_ARC_PTR(pG->pRoom, 0x2C);
    void* mot[8] = {ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x30),
                    ROOM_ARC_PTR(pG->pRoom, 0x31), ROOM_ARC_PTR(pG->pRoom, 0x32), ROOM_ARC_PTR(pG->pRoom, 0x33), ROOM_ARC_PTR(pG->pRoom, 0x34)};

    switch (pl->r_no_2) {
    case 0:
        pl->m_Work0 = 0x55;
        Cckpt.lifeMeterDisp(0);
        FSetP(pPL->pos.x, -8540.0f);
        FSetP(pPL->pos.y, 1000.0f);
        FSetP(pPL->pos.z, -16430.0f);
        pl->ang.y = -1.5707964f;
        r226_work.p->hitPoint = 0;
        r226_work.p->spdOld = 0;
        r226_work.p->spdNew = 0;
        r226_work.p->sub = 0;
        pl->r_no_2 = 1;
    case 1:
        MotionSetCore(pl, &pl->Motion, data, (int) mot[r226_work.p->spdNew], 10, 5, 0);
        pl->r_no_2 = 2;
        pl->m_Fwork0 = 1.0f;
    case 2:
        eprintf(0x40, 0x10, 0, 0, "HItPoint:[%d] SpdOld;[%d] SpdNew:[%d] Sub:[%d] ", r226_work.p->hitPoint, r226_work.p->spdOld, r226_work.p->spdNew, r226_work.p->sub);
        playerRunCamMovePassage(pl, 1.0f);
        playerPillarDownCk(robo, 8, 5, 1, -3000.0f);
        playerPillarDownCk(robo, 0xD, 0xA, 2, -3000.0f);
        playerPillarDownCk(robo, 0xA, 7, 0, -3000.0f);
        playerPillarDownCk(robo, 0xE, 0xB, 0, -4500.0f);
        if (!(pG->Room_flg[0] & 0x20000000)) {
            ButtonCount(&r226_work.p->hitPoint, &r226_work.p->spdOld, &r226_work.p->spdNew, &r226_work.p->sub, 10, 5, 3, 5, ROOM_ARC_PTR(pG->pRoom, 0x2C), mot);
            ActBtn.set(0x18, 5, 0, 0, 2, 2, 0, 0);
        } else {
            int hit = 0;

            switch ((u32) rw->pillar) {
            case 0:
            default:
                ActBtn.set(0x25, 5, 0, 0, 0x42, 3, 1, 0);
                if (((Key.trg & 0x400000) && (Key.on & 0x800000)) || ((Key.on & 0x400000) && (Key.trg & 0x800000))) {
                    hit = 1;
                }
                break;
            case 1:
                ActBtn.set(0x25, 5, 0, 0, 0x42, 9, 1, 0);
                if (Key.trg & 0x400000) {
                    hit = 1;
                }
                break;
            case 2:
                ActBtn.set(0x25, 5, 0, 0, 0x42, 0xA, 1, 0);
                if (Key.trg & 0x800000) {
                    hit = 1;
                }
                break;
            }
            if (hit) {
                BitOff(pG->Room_flg[0], 0x20000000);
                BitOn(pG->Room_flg[0], 0x10000000);
                pl->r_no_2 = 3;
                break;
            }
        }
        MotionMoveF(pl, 0);
        break;
    case 3:
        playerRunCamMovePassage(pl, 1.0f);
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x37), 0, 10, 1, 0);
        SndCall(1, 0x43, &pl->pos, 0, 0, 0);
        pl->r_no_2 = 4;
    case 4:
        playerRunCamMovePassage(pl, 1.0f);
        pl->dmg.m_Timer = 0x78;
        if (MotionMoveF(pl, 0)) {
            BitOff(pG->Room_flg[0], 0x10000000);
            if (pG->Room_flg[0] & 0x08000000) {
                pl->r_no_2 = 5;
                SceExec(0x12, (TaskFunc) R226EventRoboWalkPassageGoal, (int) robo, 0, SCE_PRIO_DEF_2, 0);
                EndPlDamage();
            } else {
                pl->r_no_2 = 1;
            }
        }
        break;
    }
}

// SetPlDamage routine: the player runs over the bridge, the pillars fall, the final jump.
// Two event flag tests in one condition: the pair only reproduces through a call, which orders the
// base and the pG load the way the original does.  Every other flag site here goes through the
// FlagChk / FlagOn macros in global.h.
static inline u32* eventFlags()
{
    return &pG->Room_flg[0];
}
static inline u32 evtFlag(u32 no)
{
    return eventFlags()[no >> 5] & (0x80000000 >> (no & 31));
}

static void playerRunMoveBridge(cPlayer* pl)
{
    void* data = ROOM_ARC_PTR(pG->pRoom, 0x2C);
    void* mot[8] = {ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x30),
                    ROOM_ARC_PTR(pG->pRoom, 0x31), ROOM_ARC_PTR(pG->pRoom, 0x32), ROOM_ARC_PTR(pG->pRoom, 0x33), ROOM_ARC_PTR(pG->pRoom, 0x34)};
    u32 smd0[6] = {0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
    u32 smd1[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    u32 i;

    switch (pl->r_no_2) {
    case 0:
        pl->m_Work0 = 0x55;
        Cckpt.lifeMeterDisp(0);
        AtariFlagsAndV(&pPL->atari, 0xFEFF);
        FSetP(pPL->pos.x, -70500.0f);
        FSetP(pPL->pos.y, 1000.0f);
        FSetP(pPL->pos.z, -16430.0f);
        pl->ang.y = -1.5707964f;
        pPL->matUpdate();
        r226_work.p->hitPoint = 0;
        r226_work.p->spdOld = 0;
        r226_work.p->spdNew = 0;
        r226_work.p->sub = 0;
        pl->r_no_2 = 1;
    case 1:
        MotionSetCore(pl, &pl->Motion, data, (int) mot[r226_work.p->spdNew], 10, 5, 0);
        pl->r_no_2 = 2;
        pl->m_Fwork0 = 1.0f;
    case 2:
        eprintf(0x40, 0x10, 0, 0, "HItPoint:[%d] SpdOld;[%d] SpdNew:[%d] Sub:[%d] ", r226_work.p->hitPoint, r226_work.p->spdOld, r226_work.p->spdNew, r226_work.p->sub);
        playerRunCamMoveBridge(pl, 1.0f);
        if (pG->Room_flg[0] & 1) {
            pG->Room_flg[1] |= 0x40000000;
        } else if (pG->Room_flg[0] & 2) {
            ActBtn.set(0xC, 5, 0, 0, 0x42, 3, 1, 0);
            if (((Key.trg & 0x400000) && (Key.on & 0x800000)) || ((Key.on & 0x400000) && (Key.trg & 0x800000))) {
                SndCall(1, 0x43, &pl->pos, 0, 0, 0);
                BitOff(pG->Room_flg[0], 0x20000000);
                BitOn(pG->Room_flg[0], 0x10000000);
                pl->r_no_2 = 3;
                break;
            }
        } else {
            ButtonCount(&r226_work.p->hitPoint, &r226_work.p->spdOld, &r226_work.p->spdNew, &r226_work.p->sub, 10, 5, 3, 5, ROOM_ARC_PTR(pG->pRoom, 0x2C), mot);
            ActBtn.set(0x18, 5, 0, 0, 2, 2, 0, 0);
        }
        for (i = 0; i < 6; i++) {
            if (evtFlag(smd0[i]) && evtFlag(smd1[i])) {
                FlagOn(&pG->Room_flg, 33);
            }
        }
        MotionMoveF(pl, 0);
        break;
    case 3:
        FSetP(pPL->pos.x, -92879.0f);
        FSetP(pPL->pos.y, 1000.0f);
        FSetP(pPL->pos.z, -16405.8f);
        FSetP(pPL->ang.x, 0.0f);
        FSetP(pPL->ang.y, -1.5707964f);
        FSetP(pPL->ang.z, 0.0f);
        BitOff(pPL->be_flag, 0x10);
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x69), 0, 3, 0x201, 0);
        MotionMoveF(pl, 0);
        pl->r_no_2 = 4;
    case 4:
        pl->dmg.m_Timer = 0x78;
        if (pl->frame >= (f32) r226_pushFrame) {
            if (Key.trg & 0x80000) {
                r226_work.p->btnCnt++;
            }
            ActBtn.set(0x19, 5, 0, 0, 2, 2, 0, 0);
            SceDebugDisp("Button:[%d/%d]", r226_work.p->btnCnt, 10);
        }
        if (pl->frame > 9.7f && pl->frame < 10.3f) {
            SndCall(1, 0x10, &pPL->pos, 0, 0, 0);
        }
        if (pl->frame > 23.7f && pl->frame < 24.3f) {
            SndCall(1, 0x34, &pPL->pos, 0, 0, 0);
        }
        if (pl->frame > 29.7f && pl->frame < 30.3f) {
            SndCall(1, 0x4F, &pPL->pos, 0, 0, 0);
        }
        if (MotionMoveF(pl, 0)) {
            IntSet(r226_work.p->btnCnt, 0);
            IntSet(r226_work.p->timer, 0);
            MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x6A), 0, 10, 0x204, 0);
            MotionMoveF(pl, 0);
            pl->r_no_2 = 5;
        }
        break;
    case 5:
        pl->dmg.m_Timer = 0x78;
        if (Key.trg & 0x80000) {
            r226_work.p->btnCnt++;
        }
        ActBtn.set(0x19, 5, 0, 0, 2, 2, 0, 0);
        MotionMoveF(pl, 0);
        r226_work.p->timer++;
        SceDebugDisp("Button:[%d/%d]", r226_work.p->btnCnt, 10);
        SceDebugDisp("Timer: [%d/%d]", r226_work.p->timer, 90);
        if (r226_work.p->timer > 90) {
            if (r226_work.p->btnCnt > 10) {
                MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x6B), 0, 3, 0x201, 0);
                MotionMoveF(pl, 0);
                r226_work.p->str = SndStrPlayBlock(1, 0x2F, 0.0f);
                pl->r_no_2 = 6;
            } else {
                U16Set(pG->pl_life, 0);
                AtariFlagsAndV(&pl->atari, 0xFCFF);
                MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x6C), 0, 3, 0x201, 0);
                SndCall(1, 0x4A, &pPL->pos, 0, 0, 0);
                MotionMoveF(pl, 0);
                pl->r_no_2 = 7;
            }
        }
        break;
    case 6:
        pl->dmg.m_Timer = 0x82;
        if (MotionMoveF(pl, 0)) {
            BitOff(pG->Room_flg[0], 0x10000000);
            RsfSet(G_ROOM_ID, 13);
            pPL->be_flag |= 0x10;
            pl->r_no_2 = 8;
            SceAtSetEnable(0x16, 1);
            if (r226_work.p->eat[3]) {
                r226_work.p->eat[3]->m_Flag &= ~4;
            }
            pPL->atari.setFlag100();
            SceEventStart(0);
            SceEventEnd(0);
            EndPlDamage();
        }
        break;
    case 7:
        MotionMoveF(pl, 0);
        break;
    }
}

// The statue caught the player: rumble + quake, then the crush death routine for the passage (which 0)
// or the bridge (which 1) via SetPlDamage.
void playerRunDieSet(cEm* em, int which)
{
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    if (which == 0) {
        SetPlDamage(em, playerRunDiePassage);
    }
    if (which == 1) {
        SetPlDamage(em, playerRunDieBridge);
    }
}

// SetPlDamage routine: crushed in the passage.
static void playerRunDiePassage(cPlayer* pl)
{
    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x36), 0, 3, 1, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xA);
        pl->r_no_2++;
    case 1:
        if (pl->frame > 22.7f && pl->frame < 23.3f) {
            PlSetDamageSe(0xD);
        }
        MotionMoveF(pl, 0);
        break;
    }
    playerRunCamDiePassage(pl);
}

// SetPlDamage routine: crushed on the bridge (the body falls with the pillar).
static void playerRunDieBridge(cPlayer* pl)
{
    f32 start = 100.0f;
    f32 step = 10.0f;

    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x36), 0, 3, 1, 0);
        CamCtrl.MotionSet(ROOM_ARC_PTR(pG->pRoom, 0x60), 0, 0.0f);
        pG->pl_life = 0;
        pl->atari.throughOn();
        PlSetDamageSe(0xA);
        r226_work.p->dieY = start;
        pl->r_no_2++;
    case 1:
        r226_work.p->dieY += step;
        setPosXYZ(pl, pl->pos.x, pl->pos.y - r226_work.p->dieY, pl->pos.z);
        if (pl->frame > 22.7f && pl->frame < 23.3f) {
            PlSetDamageSe(0xD);
        }
        MotionMoveF(pl, 0);
        break;
    }
}

// Reset the bridge chase camera offsets to their initial values.
void playerRunCamInitBridge()
{
    r226_work.p->camPos = r226_camPosInit;
    r226_work.p->camAt = r226_camAtInit;
}

// The chase camera: the player's frame offsets blended towards the current camera.
void playerRunCamMovePassage(cPlayer* pl, f32 t)
{
    Camera* cam = &r226_cam;
    GlobalWork* g = pG;
    Vec pos;
    Vec at;

    cam->param.fovy = r226_fovyPassage;
    PSMTXMultVec(pl->mat, &r226_camOfsPos, &pos);
    PSMTXMultVec(pl->mat, &r226_camOfsAt, &at);
    PosToPos(&g->Cam.param.at, &at, &r226_cam.param.at, t);
    PosToPos(&g->Cam.param.pos, &pos, &r226_cam.param.pos, t);
    cam->up.x = 0.0f;
    cam->up.y = 1.0f;
    cam->up.z = 0.0f;
    cam->dist = vecDist(&r226_cam.param.pos, &r226_cam.param.at);
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// The bridge chase camera: offsets chased towards r226_camOfsPos/At at r226_camSpd*, FOV r226_fovyBridge.
void playerRunCamMoveBridge(cPlayer* pl, f32 t)
{
    Camera* cam = &r226_cam;
    GlobalWork* g = pG;
    Vec pos;
    Vec at;

    cam->param.fovy = r226_fovyBridge;
    if (g->Room_flg[0] & 0x2000) {
        r226_work.p->camPos.x += r226_camSpdPos.x;
        r226_work.p->camPos.y += r226_camSpdPos.y;
        r226_work.p->camPos.z += r226_camSpdPos.z;
        r226_work.p->camAt.x += r226_camSpdAt.x;
        r226_work.p->camAt.y += r226_camSpdAt.y;
        r226_work.p->camAt.z += r226_camSpdAt.z;
    }
    PSMTXMultVec(pl->mat, &r226_work.p->camPos, &pos);
    PSMTXMultVec(pl->mat, &r226_work.p->camAt, &at);
    PosToPos(&g->Cam.param.at, &at, &r226_cam.param.at, t);
    PosToPos(&g->Cam.param.pos, &pos, &r226_cam.param.pos, t);
    cam->up.x = 0.0f;
    cam->up.y = 1.0f;
    cam->up.z = 0.0f;
    cam->dist = vecDist(&r226_cam.param.pos, &r226_cam.param.at);
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// The death camera in the passage: a fixed view (FOV r226_fovyDie) looking at the crushed player.
void playerRunCamDiePassage(cPlayer* pl)
{
    Camera* cam = &r226_cam;
    GlobalWork* g = pG;
    cModel* parts;

    cam->param.fovy = r226_fovyDie;
    parts = pl->getPartsPtr(0);
    PosToPos(&g->Cam.param.at, &parts->world, &r226_cam.param.at, 1.0f);
    cam->param.pos = g->Cam.param.pos;
    cam->up.x = 0.0f;
    cam->up.y = 1.0f;
    cam->up.z = 0.0f;
    cam->dist = vecDist(&r226_cam.param.pos, &r226_cam.param.at);
    CameraSetOrientationUp(cam);
    CamCtrl.m_pExtraCamera = (s32) cam;
}

// Starts the pillar `smdNo` falling once the player passed it by `dist`.
extern "C" void playerPillarDownCk__FP8cObjRoboiUlif(cObjRobo* robo, int smdNo, u32 flagNo, f32 dist, int idx)
{
    RoboWork* rw = &robo->robo;

    if (!FlagChkVar(&pG->Room_flg, (u32) flagNo)) {
        cObj* o = SmdGetObjPtr(smdNo);

        if (o) {
            if (pPL->pos.x < o->pos.x + dist) {
                rw->pillar = idx;
                SceExec(0x12, (TaskFunc) playerPillarDownTask, smdNo, 6, SCE_PRIO_DEF_2, 0);
                FlagOnVar(&pG->Room_flg, (u32) flagNo);
            }
        }
    }
}

// Task: one pillar of the bridge falls; sets the statue-hit / player-hit flags on the way down.
static void playerPillarDownTask(int smdNo)
{
    cPlayer* pl = pPL;
    cObj* o = SmdGetObjPtr(smdNo);
    int k;
    int i;
    int on;

    if (o == NULL) {
        return;
    }
    o->be_flag |= 0x20;
    k = smdNo == 9;
    if (smdNo == 10) {
        k = 2;
    }
    if (smdNo == 11) {
        k = 3;
    }
    if (smdNo == 12) {
        k = 4;
    }
    if (smdNo == 13) {
        k = 5;
    }
    if (smdNo == 14) {
        k = 6;
    }
    if (smdNo == 15) {
        k = 7;
    }
    int frames[8] = {60, 60, 60, 60, 60, 60, 60, 60};
    int hits[8] = {3, 3, 5, 3, 3, 4, 6, 3};
    i = 0;
    on = 1;
    if (smdNo == 14) {
        pG->Room_flg[0] |= 0x08000000;
    }
    if (smdNo >= 8 && smdNo <= 11) {
        MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x40), 0, 0, 1, 0);
    } else {
        MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x41), 0, 0, 1, 0);
    }
    o->be_flag |= 0x20;
    EstSet(0, -1, 0, 0, 1, (u8) hits[k], 1, 2, 0, 0);
    SndCall(6, 0xA, &o->pos, 0, 0, 0);
    while (1) {
        i++;
        if (i >= frames[k]) {
            SndCall(6, 0xB, &o->pos, 0, 0, 0);
            BitOff(pG->Room_flg[0], 0x20000000);
            o->be_flag &= ~2;
            return;
        }
        if (on == 1 && i >= frames[k] * 90 / 100) {
            pG->Room_flg[1] |= 0x80000000;
            on = 0;
        }
        if ((pG->Room_flg[0] & 0x20000000) && on == 1 && i >= frames[k] * 80 / 100) {
            if (pPL->pos.x > o->pos.x + -3000.0f - (f32) i * r226_pillarSpd) {
                pG->Room_flg[1] |= 0x80000000;
                on = 0;
            }
        }
        if (pG->Room_flg[0] & 0x10000000) {
            on = 0;
        }
        if (on == 1 && pl->pos.x < o->pos.x - 8000.0f) {
            pG->Room_flg[0] |= 0x20000000;
        }
        SceSleep(1);
    }
}
