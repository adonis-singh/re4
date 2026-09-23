#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "area.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emBarred.h"
#include "emrock.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "math_sub.h"
#include "rnd.h"
#include "snd.h"
#include "sscrn.h"

// Room 2-14 (D:/Bio4/Prog/r214.cpp): the three catapults, the patrols, the enemy waves behind the
// barred gates and the rotating bridge.

// One catapult and its crew.
class cCatapult214 {
public:
    u32 emNo;         // 0x00  crew enemy list entry
    cEmWrap em;       // 0x04
    cEmRock* rock;    // 0x10
    cObj* obj;        // 0x14  the catapult model
    int step;         // 0x18
    int timer;        // 0x1C
    u8 thrown;        // 0x20  a rock is in flight
    u8 rockReady;     // 0x21  the crew finished loading
    u8 fixedTarget;   // 0x22  1: aim at `target`
    u8 pad_23;
    Vec target;       // 0x24
    u8 active;        // 0x30
    u8 x31;
    s8 area[4];       // 0x32  areas the player must be in to fire
    s8 atArea[4];     // 0x36  areas the rock is aimed at (-1: the player)
    s8 nArea;         // 0x3A
    u8 hitIdx;        // 0x3B  area index of the last hit
    u8 fireAll;       // 0x3C  fires as soon as the player is in range
    u8 fire;          // 0x3D  one shot requested
    u8 x3E;
    u8 pad_3F;
    f32 height;       // 0x40  parabola height

    void setNewArea(s8 area, s8 atArea);
    void move();
    int checkHitArea();
    void setRock();
    void throwRock();
};

struct R214CatapultData {
    u32 objId;
    f32 ang;
    u32 emNo;
};

struct R214Work {
    int bridgeFlag;          // 0x000  1: the bridge rotation started from the entrance
    cCatapult214 cat[3];     // 0x004
    int hitWait;             // 0x0D0  frames before another catapult may fire at the player
    ScePrim* catTask;        // 0x0D4
    cEm* barred[2];          // 0x0D8
    cEmPatrol patrol[4];     // 0x0E0
    IdBinocular* bino;       // 0x550
    FocusAnimation* focus;   // 0x554
    u8 pad_558[4];
    IdBinocular binoObj;     // 0x55C
    FocusAnimation focusObj; // 0x59C
    cEmWrap emCat;           // 0x5AC  the crew leader of the catapult event
    cEmWrap em3[3];          // 0x5B8
    Vec ang3[3];             // 0x5DC
    Vec pos5[5];             // 0x600
    f32 angY5[5];            // 0x63C
    cEmWrap em5[5];          // 0x650
    cEmWrap em3b[3];         // 0x68C
    cEmWrap em2[2];          // 0x6B0
};


static R214Work* r214_work;
static u8 r214_debugMode;

static int r214_emTbl0[5] = {0xE4, 0xEC, 0xED, 0xEE, 0xEF};
static int r214_emTbl1[3] = {0xE9, 0xEA, 0xEB};
static int r214_emTbl2[3] = {0xCB, 0xCC, 0xCD};
static int r214_emTbl3[3] = {0xCF, 0xD0, 0xD1};
static int r214_emTbl4[10] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9};


// Local arrays of these get the constructor loop and the (empty) destructor loop.
class cEmWrapD : public cEmWrap {
public:
    ~cEmWrapD() {}
};

static void r214_execEvent00();
static void r214_checkBgmPlay();
static void r214_checkEmReset();
static void r214_exec3rdEmSet_end();
static void r214_exec3rdEmSet();
static void r214_execCatapult_end();
static void r214_execCatapult();
void r214_setFireAll();
void r214_initCatapult(R214CatapultData* tbl);
static void r214_checkCatapult();
static void r214_setRock(cCatapult214* c);
static void r214_throwRock(cCatapult214* c);
void r214_getTargetPos(Vec* out);
static void r214_BridgeRotate();
static void r214_BridgeRotateEndProc();
static void r214_BridgeRotateCamera();
void Evt_R214S00_Func(Event* e);

static const R214CatapultData r214_catTbl[3] = {
    {0x4E, 4.4505897f, 0xE9},
    {0x4F, 4.4505897f, 0xEB},
    {0x50, 4.4505897f, 0xEA},
};
static const Vec r214_rockOfs = {0.0f, 800.0f, -2400.0f};

// Room init (the castle wall with the three catapults): JumpPoint 1 skips ahead (Scenario_flg[1]
// 0x40000000, Room_flg bits 2/5); the s00 callback; both barred gates (etc 0x11/0x12) closed. Part 2
// (arriving on the rotating bridge) plays the bridge rotation. Before the catapults are silenced
// (Scenario_flg[1] 0x40000000): the s00 event area once (bit 5), the two patrols (0xEC/0xED between areas
// 0xF..0x11), area 9 = the catapult crew event until bit 0, the catapults; the third wave behind the
// gates on area 0xD until bit 1 with the reset task (bit 4); the battle stream.
void R214Init()
{
    cEm* barred;

#line 98 "D:/Bio4/Prog/r214.cpp"
    R214Work*& wp = r214_work;   // reference: the following `lwz pG` stays below the store (r227 idiom)
    wp = (R214Work*) MEM_CALLOC(sizeof(R214Work), 1, 0xD);
    if (pG->JumpPoint == 1) {
        ScfFlagOn(pG, SCF_R217_PUZZLE_CLEAR);
        RsfSet(G_ROOM_ID, 2);
        RsfSet(G_ROOM_ID, 5);
    }
    EvtMgr.SetFunc("evt_r214s00_func", (void*) Evt_R214S00_Func);
    if (getRoomEtcBarred(0x11, &r214_work->barred[0], 1)) {
        ((cEmBarred*) r214_work->barred[0])->setClosed();
    }
    if (getRoomEtcBarred(0x12, &r214_work->barred[1], 1)) {
        ((cEmBarred*) r214_work->barred[1])->setClosed();
    }
    if (pG->Part == 2) {
        r214_work->bridgeFlag = 1;
        SceExec(0x12, (TaskFunc) r214_BridgeRotate, 0, 0, SCE_PRIO_DEF_2, 0);
    } else if (!ScfFlagChk(pG, SCF_R217_PUZZLE_CLEAR)) {
        u32 i;

        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            EvtMgr.EvtReadAram("event/evd/r214s00.evd", (u8) GetEmIdFromList(0xE4), 0, 0, 0);
            SceAtDataSet_exec(0x12, SCE_LEVEL10, 0, (TaskFunc) r214_execEvent00, 0, 1);
        }
        SceAtSetEnable(5, 0);
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 1);
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            setEm(0xE4, 4, 1, 1, 0);
            setEm(0xEC, 4, 1, 1, 0);
            setEm(0xED, 4, 1, 1, 0);
            setEm(0xEE, 4, 1, 1, 0);
            setEm(0xEF, 4, 1, 1, 0);
        } else {
            for (i = 0; i < 5; i++) {
                setEm(r214_emTbl0[i], 4, 0, 1, 0);
            }
            if (RsfCheck(G_ROOM_ID, 0)) {
                for (i = 0; i < 3; i++) {
                    setEm(r214_emTbl1[i], 4, 0, 1, 0);
                }
            }
        }
        cEmWrap w;
        Vec pt[3];
        AreaGetCenterPos(&pt[0], &SceAtPtr(0xF)->area);
        AreaGetCenterPos(&pt[1], &SceAtPtr(0x10)->area);
        AreaGetCenterPos(&pt[2], &SceAtPtr(0x11)->area);
        r214_work->patrol[0].SetPatrol(0xEC, pt, 3, 0, 0);
        w.setPtr(0xEC, -1, 0);
        w.setPos(&pt[0]);
        AreaGetCenterPos(&pt[0], &SceAtPtr(0x10)->area);
        AreaGetCenterPos(&pt[1], &SceAtPtr(0x11)->area);
        AreaGetCenterPos(&pt[2], &SceAtPtr(0xF)->area);
        r214_work->patrol[1].SetPatrol(0xED, pt, 3, 0, 0);
        w.setPtr(0xED, -1, 0);
        w.setPos(&pt[0]);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r214_execCatapult, 0, 1);
        } else {
            r214_initCatapult((R214CatapultData*) r214_catTbl);
        }
    } else {
        u32 i;

        SmdGetObjPtr(0x15)->ang.y = 1.5707964f;
        SmdGetObjPtr(0x15)->matUpdate();
        SmdGetObjPtr(0x16)->ang.y = 1.5707964f;
        SmdGetObjPtr(0x16)->matUpdate();
        SceAtSetEnable(6, 0);
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 2);
        for (i = 0; i < 3; i++) {
            setEm(r214_emTbl3[i], 4, 0, 1, 0);
        }
        if (RsfCheck(G_ROOM_ID, 2) || pG->Part == 1) {
            RsfSet(G_ROOM_ID, 2);
            for (i = 0; i < 3; i++) {
                setEm(r214_emTbl2[i], 4, 0, 1, 0);
            }
        }
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            SceAtDataSet_exec(0xD, SCE_LEVEL10, 0, (TaskFunc) r214_exec3rdEmSet, 0, 1);
        } else {
            for (i = 0; i < 10; i++) {
                setEm(r214_emTbl4[i], 4, 0, 1, 0);
            }
            if (RsfCheck(G_ROOM_ID, 4) == 0) {
                SceExec(0x12, (TaskFunc) r214_checkEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    if (getRoomEtcBarred(0x11, &barred, 1)) {
        barred->LightInfo.SelectMask &= ~0x40;
    }
    if (getRoomEtcBarred(0x12, &barred, 1)) {
        barred->LightInfo.SelectMask &= ~0x40;
    }
    SceExec(0x12, (TaskFunc) r214_checkBgmPlay, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per frame: debug trigger 0 (once, Room_flg[1] 0x40000000) squares the bridge halves and plays the rotation.
void R214Main()
{
    if (DebugTrg(0) == 1) {
        if (!(pG->Room_flg[1] & 0x40000000)) {
            pG->Room_flg[1] |= 0x40000000;
            SmdGetObjPtr(0x15)->ang.y = 0.0f;
            SmdGetObjPtr(0x15)->matUpdate();
            SmdGetObjPtr(0x16)->ang.y = 0.0f;
            SmdGetObjPtr(0x16)->matUpdate();
            SceExec(0x12, (TaskFunc) r214_BridgeRotate, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
}

// Area 0x12 once (Room_flg bit 5): typewriter 0x12 opened, then event r214s00 with the enemy of ESL 0xE4.
static void r214_execEvent00()
{
    RsfSet(G_ROOM_ID, 5);
    OpeSetOpenTerm(0x12, 0.0f, 0.0f, 0.0f, 0.0f);
    SceEventStart(0);
    EvtMgr.EvtReadExec("event/evd/r214s00.evd", (u8) GetEmIdFromList(0xE4), EvtReadFlagPlPosNoSet);
    SceEventEnd(0);
}

// Task: the fight stream while enemies are alive and have found the player.
static void r214_checkBgmPlay()
{
    while (SceCkFindPL(0) != 1) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// Task: keeps four enemies of the last wave coming until enough are alive.
static void r214_checkEmReset()
{
    u32 lim;
    u32 i;

    if (r214_work->barred[0]) {
        ((cEmBarred*) r214_work->barred[0])->setOpened();
    }
    if (r214_work->barred[1]) {
        ((cEmBarred*) r214_work->barred[1])->setOpened();
    }
    int emNo[4] = {0xFB, 0xFC, 0xFD, 0xFE};
    int done[4];
    u32 n = 0;
    for (i = 0; i < 4; i++) {
        done[i] = 0;
    }
    cEmWrapD em[4];
    lim = SceCountEmAlive(0x10, 0x20);
    if (lim <= 4) {
        lim = 5;
    }
    for (;;) {
        if ((u32) SceCountEmAlive(0x10, 0x20) < lim) {
            for (i = 0; i < 4; i++) {
                if (done[i] == 0) {
                    int r = em[i].setEm(emNo[i], 4, 1, 0, 0);

                    if (r == 1) {
                        em[i].setFlag(1);
                        n++;
                        done[i] = r;
                    }
                    SceSleep(30);
                    break;
                } else if (em[i].ckResetEnable() == 1) {
                    n++;
                    em[i].setReset();
                    em[i].setFlag(1);
                    SceSleep(30);
                    break;
                }
            }
        }
        if (n > 3) {
            break;
        }
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 4);
}

// End of the third-wave cutscene (also its cancel path): the wave Ganados and Leon may suspend, the five
// em5 are put back at their saved positions / yaws, the three em3b and two em2 likewise, SceEventEnd,
// the reset task starts.
static void r214_exec3rdEmSet_end()
{
    Vec ang;

    r214_work->em2[0].setNoSuspend(0);
    r214_work->em2[1].setNoSuspend(0);
    r214_work->em5[0].setNoSuspend(0);
    r214_work->em5[1].setNoSuspend(0);
    r214_work->em5[2].setNoSuspend(0);
    r214_work->em5[3].setNoSuspend(0);
    r214_work->em5[4].setNoSuspend(0);
    r214_work->em3b[0].setNoSuspend(0);
    r214_work->em3b[1].setNoSuspend(0);
    r214_work->em3b[2].setNoSuspend(0);
    pPL->setNoSuspend(0);
    {
        cEmWrap* w = &r214_work->em5[0];
        f32 y = r214_work->angY5[0];

        w->setPos(&r214_work->pos5[0]);
        ang.x = 0.0f;
        ang.y = y;
        ang.z = 0.0f;
        w->setAng(&ang);
    }
    {
        cEmWrap* w = &r214_work->em5[1];
        f32 y = r214_work->angY5[1];

        w->setPos(&r214_work->pos5[1]);
        ang.x = 0.0f;
        ang.y = y;
        ang.z = 0.0f;
        w->setAng(&ang);
    }
    {
        cEmWrap* w = &r214_work->em5[2];
        f32 y = r214_work->angY5[2];

        w->setPos(&r214_work->pos5[2]);
        ang.x = 0.0f;
        ang.y = y;
        ang.z = 0.0f;
        w->setAng(&ang);
    }
    {
        cEmWrap* w = &r214_work->em5[3];
        f32 y = r214_work->angY5[3];

        w->setPos(&r214_work->pos5[3]);
        ang.x = 0.0f;
        ang.y = y;
        ang.z = 0.0f;
        w->setAng(&ang);
    }
    {
        cEmWrap* w = &r214_work->em5[4];
        f32 y = r214_work->angY5[4];

        w->setPos(&r214_work->pos5[4]);
        ang.x = 0.0f;
        ang.y = y;
        ang.z = 0.0f;
        w->setAng(&ang);
    }
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r214_checkEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
}

// The third wave: the gates open, the enemies come through with two camera cuts.
// Position + angle of a wrapped enemy through ONE inline whose float arguments are all expanded at
// the call head (integrate.c evaluates the actual arguments before the body): the angle constant is
// loaded before the setPos call and shared by the later blocks (`lfs f30,-0.05` before `bl setPos`).
// The Vec is the inline's own local: its frame temp is freed after each statement and reused by the
// next expansion (one 16-byte slot), and its address is recomputed `addi r4,r1,8` per call.
static inline void r214_emPosAng(cEmWrap* w, f32 x, f32 y, f32 z, f32 rx, f32 ry, f32 rz)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    w->setPos(&v);
    v.x = rx;
    v.y = ry;
    v.z = rz;
    w->setAng(&v);
}

// Back to the saved position and yaw (same slot as above).
static inline void r214_emRestore(cEmWrap* w, Vec* pos, f32 y)
{
    Vec v;

    w->setPos(pos);
    v.x = 0.0f;
    v.y = y;
    v.z = 0.0f;
    w->setAng(&v);
}

// Area 0xD once (Room_flg bit 1): both barred gates open, the wave Ganados (list 4: 0xF0..0xF2, 0xF8,
// 0xF9 and more) are spawned, their positions saved, then posed for the camera cuts of the cutscene
// while Leon is frozen; player-cancellable.
static void r214_exec3rdEmSet()
{
    RsfSet(G_ROOM_ID, 1);
    if (r214_work->barred[0]) {
        r214_work->barred[0]->setNoSuspend(1);
        ((cEmBarred*) r214_work->barred[0])->setOpen(0);
    }
    if (r214_work->barred[1]) {
        r214_work->barred[1]->setNoSuspend(1);
        ((cEmBarred*) r214_work->barred[1])->setOpen(0);
    }
    r214_work->em5[0].setEm(0xF0, 4, 1, 1, 0);
    r214_work->em5[1].setEm(0xF1, 4, 1, 1, 0);
    r214_work->em5[2].setEm(0xF2, 4, 1, 1, 0);
    r214_work->em5[3].setEm(0xF8, 4, 1, 1, 0);
    r214_work->em5[4].setEm(0xF9, 4, 1, 1, 0);
    r214_work->em5[0].getPos(&r214_work->pos5[0]);
    r214_work->em5[1].getPos(&r214_work->pos5[1]);
    r214_work->em5[2].getPos(&r214_work->pos5[2]);
    r214_work->em5[3].getPos(&r214_work->pos5[3]);
    r214_work->em5[4].getPos(&r214_work->pos5[4]);
    r214_work->angY5[0] = r214_work->em5[0].getAngY();
    r214_work->angY5[1] = r214_work->em5[1].getAngY();
    r214_work->angY5[2] = r214_work->em5[2].getAngY();
    r214_work->angY5[3] = r214_work->em5[3].getAngY();
    r214_work->angY5[4] = r214_work->em5[4].getAngY();
    r214_work->em3b[0].setEm(0xF5, 4, 1, 1, 0);
    r214_work->em3b[1].setEm(0xF6, 4, 1, 1, 0);
    r214_work->em3b[2].setEm(0xF7, 4, 1, 1, 0);
    SceSetEventCancel(1, (TaskFunc) r214_exec3rdEmSet_end, 0, -1, 1);
    SceEventStart(0);
    CamCtrl.CutCall(0xA);
    r214_work->em2[0].setEm(0xF3, 4, 1, 1, 0);
    r214_work->em2[1].setEm(0xF4, 4, 1, 1, 0);
    r214_work->em2[0].setNoSuspend(1);
    r214_work->em2[1].setNoSuspend(1);
    r214_work->em2[0].setFlag(1);
    r214_work->em2[1].setFlag(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r214_work->em2[0].setNoSuspend(0);
    r214_work->em2[1].setNoSuspend(0);
    CamCtrl.CutCall(8);
    r214_work->em5[0].setNoSuspend(1);
    r214_work->em5[1].setNoSuspend(1);
    r214_work->em5[2].setNoSuspend(1);
    r214_work->em5[3].setNoSuspend(1);
    r214_work->em5[4].setNoSuspend(1);
    r214_emPosAng(&r214_work->em5[0], 28427.0f, 0.0f, -30253.0f, 0.0f, -0.05f, 0.0f);
    r214_emPosAng(&r214_work->em5[1], 28320.0f, 0.0f, -31210.0f, 0.0f, -0.05f, 0.0f);
    r214_emPosAng(&r214_work->em5[2], 29250.0f, 0.0f, -15550.0f, 0.0f, -3.14f, 0.0f);
    r214_emPosAng(&r214_work->em5[3], 28427.0f, 0.0f, -30253.0f, 0.0f, -0.05f, 0.0f);
    r214_emPosAng(&r214_work->em5[4], 28590.0f, 0.0f, -13870.0f, 0.0f, -3.14f, 0.0f);
    r214_work->em5[0].setGoto(&r214_work->pos5[0], 1);
    r214_work->em5[1].setGoto(&r214_work->pos5[1], 1);
    r214_work->em5[2].setGoto(&r214_work->pos5[2], 1);
    r214_work->em5[3].setGoto(&r214_work->pos5[3], 1);
    r214_work->em5[4].setGoto(&r214_work->pos5[4], 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r214_work->em5[0].setNoSuspend(0);
    r214_work->em5[1].setNoSuspend(0);
    r214_work->em5[2].setNoSuspend(0);
    r214_work->em5[3].setNoSuspend(0);
    r214_work->em5[4].setNoSuspend(0);
    r214_emRestore(&r214_work->em5[0], &r214_work->pos5[0], r214_work->angY5[0]);
    r214_emRestore(&r214_work->em5[1], &r214_work->pos5[1], r214_work->angY5[1]);
    r214_emRestore(&r214_work->em5[2], &r214_work->pos5[2], r214_work->angY5[2]);
    r214_emRestore(&r214_work->em5[3], &r214_work->pos5[3], r214_work->angY5[3]);
    r214_emRestore(&r214_work->em5[4], &r214_work->pos5[4], r214_work->angY5[4]);
    CamCtrl.CutCall(9);
    r214_work->em3b[0].setNoSuspend(1);
    r214_work->em3b[1].setNoSuspend(1);
    r214_work->em3b[2].setNoSuspend(1);
    pPL->setNoSuspend(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r214_exec3rdEmSet_end();
}

// End of the crew cutscene: the three crew Ganados back at their saved poses and free to suspend,
// SceEventEnd, the catapults initialised, the crew leader walks to area 0xF.
static void r214_execCatapult_end()
{
    Vec pos;

    r214_work->em3[0].setPos(&r214_work->pos5[0]);
    r214_work->em3[1].setPos(&r214_work->pos5[1]);
    r214_work->em3[2].setPos(&r214_work->pos5[2]);
    r214_work->em3[0].setAng(&r214_work->ang3[0]);
    r214_work->em3[1].setAng(&r214_work->ang3[1]);
    r214_work->em3[2].setAng(&r214_work->ang3[2]);
    r214_work->em3[0].setNoSuspend(0);
    r214_work->em3[1].setNoSuspend(0);
    r214_work->em3[2].setNoSuspend(0);
    SceEventEnd(0);
    r214_initCatapult((R214CatapultData*) r214_catTbl);
    SceSleep(10);
    AreaGetCenterPos(&pos, &SceAtPtr(0xF)->area);
    r214_work->emCat.setGoto(&pos, 1);
    r214_work->emCat.setNoSuspend(0);
}

// The catapult event: the crews arrive and the catapults start.
// Same on a caller Vec that is not at frame offset 0: the pointer parameter keeps its pseudo
// (`mr r4,r31`, `stfs 4(r31)`; the .x store folds to the frame offset). The yaw-only angle has its
// zeros as body literals: those pool loads lose RTX_UNCHANGING_P and stay below the setPos call.
static inline void r214_emPosAngY(cEmWrap* w, Vec* v, f32 x, f32 y, f32 z, f32 ry)
{
    v->x = x;
    v->y = y;
    v->z = z;
    w->setPos(v);
    v->x = 0.0f;
    v->y = ry;
    v->z = 0.0f;
    w->setAng(v);
}

// Position an enemy from three components through the caller's Vec.
static inline void r214_emPosV(cEmWrap* w, Vec* v, f32 x, f32 y, f32 z)
{
    v->x = x;
    v->y = y;
    v->z = z;
    w->setPos(v);
}

// Area 9 once (Room_flg bit 3): whichever patrol Ganado (0xEC / 0xED) is alive becomes the crew leader
// (0xE0) and its patrol ends; the crew Ganados are posed at the catapults for the cutscene and the
// binocular view; player-cancellable.
static void r214_execCatapult()
{
    static const Vec gotoPos = {-15330.0f, 8021.0f, -12526.0f};   // local static: output before the pool
    u32 i;

    RsfSet(G_ROOM_ID, 3);
    cEmWrap w0;
    cEmWrap w1;
    Vec v;
    Vec v2;
    w0.setPtr(0xEC, -1, 1);
    w1.setPtr(0xED, -1, 1);
    if (w0.getHp() > 0) {
        r214_work->emCat.setEm(0xE0, -1, 1, 1, 0);
        r214_work->emCat.setPtr(0xE0, -1, 1);
        r214_work->patrol[0].EndControl();
    } else if (w1.getHp() > 0) {
        r214_work->emCat.setEm(0xE0, -1, 1, 1, 0);
        r214_work->emCat.setPtr(0xE0, -1, 1);
        r214_work->patrol[1].EndControl();
    } else {
        for (i = 0; i < 5; i++) {
            setEm(r214_emTbl0[i], 4, 1, 1, 0);
        }
        SceExit();
    }
    r214_work->em3[0].setEm(0xE9, 4, 1, 1, 0);
    r214_work->em3[1].setEm(0xEA, 4, 1, 1, 0);
    r214_work->em3[2].setEm(0xEB, 4, 1, 1, 0);
    r214_work->em3[0].getPos(&r214_work->pos5[0]);
    r214_work->em3[1].getPos(&r214_work->pos5[1]);
    r214_work->em3[2].getPos(&r214_work->pos5[2]);
    r214_work->em3[0].getAng(&r214_work->ang3[0]);
    r214_work->em3[1].getAng(&r214_work->ang3[1]);
    r214_work->em3[2].getAng(&r214_work->ang3[2]);
    RsfSet(G_ROOM_ID, 0);
    SndRoomStrStart(1, 0, 1);
    SceSetEventCancel(1, (TaskFunc) r214_execCatapult_end, 0, -1, 1);
    SceEventStart(0);
    CamCtrl.CutCall(6);
    r214_work->emCat.setNoSuspend(1);
    r214_emPosAngY(&r214_work->emCat, &v, -18813.0f, 8021.0f, -17090.0f, 0.44f);
    r214_work->emCat.setFindPL();
    SceSleep(1);
    v = gotoPos;
    r214_work->emCat.setGoto(&v, 1);
    while (r214_work->emCat.ckGoto() != 0) {
        SceSleep(1);
    }
    r214_work->emCat.setGoto(&pPL->pos, 8);
    SceSleep(60);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r214_work->emCat.setNoSuspend(0);
    CamCtrl.CutCall(7);
    r214_work->em3[0].setNoSuspend(1);
    r214_work->em3[1].setNoSuspend(1);
    r214_work->em3[2].setNoSuspend(1);
    r214_emPosV(&r214_work->em3[1], &v2, 51170.0f, 10500.0f, 6130.0f);
    r214_emPosV(&r214_work->em3[2], &v2, 51720.0f, 10500.0f, 2360.0f);
    r214_emPosV(&r214_work->em3[0], &v2, 52000.0f, 10500.0f, -1300.0f);
    r214_work->em3[0].setGoto(&r214_work->pos5[0], 1);
    r214_work->em3[1].setGoto(&r214_work->pos5[1], 1);
    r214_work->em3[2].setGoto(&r214_work->pos5[2], 1);
    SceSleep(80);
    SceSetEventCancel(0, 0, 0, -1, 1);
    r214_execCatapult_end();
}

// Order all three catapults to fire and set Room_flg[0] 0x40000000 (catapults active).
void r214_setFireAll()
{
    u32 i;

    for (i = 0; i < 3; i++) {
        r214_work->cat[i].fire = 1;
    }
    pG->Room_flg[0] |= 0x40000000;
}

// The three catapults from r214_catTbl: model, yaw, crew list entry, an 8000-unit parabola, the three
// fire areas (0xE->0xA, 0xB->0xB, 0xC->0xC); then fire-all and the catapult task.
void r214_initCatapult(R214CatapultData* tbl)
{
    int i;

    for (i = 0; i < 3; i++) {
        r214_work->cat[i].active = 1;
        r214_work->cat[i].x31 = 1;
        r214_work->cat[i].obj = SmdGetObjPtr(tbl[i].objId);
        r214_work->cat[i].obj->be_flag |= 0x20;
        r214_work->cat[i].obj->ang.y = LIMIT_ANGLE(tbl[i].ang);
        r214_work->cat[i].step = 0;
        r214_work->cat[i].timer = 0;
        r214_work->cat[i].thrown = 0;
        r214_work->cat[i].nArea = 0;
        r214_work->cat[i].height = 8000.0f;
        r214_work->cat[i].emNo = tbl[i].emNo;
        r214_work->cat[i].setNewArea(0xE, 0xA);
        r214_work->cat[i].setNewArea(0xB, 0xB);
        r214_work->cat[i].setNewArea(0xC, 0xC);
    }
    r214_setFireAll();
    r214_work->catTask = SceExec(0x12, (TaskFunc) r214_checkCatapult, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Add a (player area -> target area) pair to the catapult's fire table (max 4).
void cCatapult214::setNewArea(s8 a, s8 at)
{
    if (nArea > 3) {
        return;
    }
    area[nArea] = a;
    atArea[nArea] = at;
    nArea++;
}

// Task: runs the three catapults.
static void r214_checkCatapult()
{
    int i;

    SceSleep(1);
    for (i = 0; i < 3; i++) {
        r214_work->cat[i].em.setPtr(r214_work->cat[i].emNo, 4, 0);
    }
    for (;;) {
        for (i = 0; i < 3; i++) {
            r214_work->cat[i].fireAll = 1;
            r214_work->cat[i].move();
        }
        r214_work->hitWait--;
        SceSleep(1);
    }
}

// Per-frame catapult state machine (while its crew lives): 0 wait timer, 1 order the crew to load a
// rock (r214_setRock task), 2 wait for it and for the catapults being active, 3/4 wait for the player
// in a fire area (with the shared hitWait gap of 15 frames), 5/6 aim, 7 launch (r214_throwRock task),
// 8 wait for the rock to land then a random reload delay.
void cCatapult214::move()
{
    if (em.isActive() == 0 || em.checkStatus(EM_STATUS_ACTIVE) == 0 || active == 0) {
        return;
    }
    switch (step) {
    case 0:
        if (timer <= 0) {
            step = 1;
        }
        timer--;
        break;
    case 1:
        SceExec(0x12, (TaskFunc) r214_setRock, (int) this, 0, SCE_PRIO_DEF_2, 0);
        rockReady = 0;
        step = 2;
        break;
    case 2:
        if (rockReady == 1) {
            if (pG->Room_flg[0] & 0x40000000) {
                step = 3;
            }
        }
        break;
    case 3:
        rock->setEffAlways(1, 0);
        step = 4;
        timer = 60;
        break;
    case 4:
        if (timer <= 0 && r214_work->hitWait <= 0 && checkHitArea() == 1 && (fireAll == 1 || fire == 1)) {
            fire = 0;
            r214_work->hitWait = 15;
            step = 5;
        }
        timer--;
        break;
    case 5:
        if (fireAll == 1) {
            r214_setFireAll();
        }
        r214_work->hitWait = 15;
        timer = 30;
        step = 6;
        break;
    case 6:
        if (timer <= 0) {
            step = 7;
        }
        timer--;
        break;
    case 7:
        SceExec(0x12, (TaskFunc) r214_throwRock, (int) this, 0, SCE_PRIO_DEF_2, 0);
        step = 8;
        break;
    case 8:
        if (thrown == 1) {
            u8 r;

            thrown = 1;
            r = Rnd();
            step = 0;
            timer = (u8) (r / 10);
        }
        break;
    }
}

// 1 when the player stands in one of the fire areas (hitIdx receives which).
int cCatapult214::checkHitArea()
{
    int i;

    for (i = 0; i < nArea; i++) {
        if (SceAtHitCheck(area[i]) == 1) {
            hitIdx = i;
            return 1;
        }
    }
    return 0;
}

// Task: the crew loads a rock (dies -> the task ends).
static void r214_setRock(cCatapult214* c)
{
    u32 i;

    c->em.setFlag(1);
    c->em.motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 10, 0, 1, 0);
    SceSleep(54);
    if (c->em.getHp() <= 0) {
        SceExit();
    }
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    c->rock = SetRock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &rot, 0);
    if (c->em.getPtr()) {
        c->rock->setParent(c->em.getPtr(), 10, 0);
    }
    for (i = 0; i < 50; i++) {
        if (c->em.getHp() <= 0) {
            c->rock->setTransMode(0);
            SceExit();
        }
        SceSleep(1);
    }
    c->setRock();
    c->obj->pList->ang.x = 0.0f;
    for (i = 0; i < 10; i++) {
        c->obj->pList->ang.x += -0.024137001f;
        SceSleep(1);
    }
    c->rockReady = 1;
}

// Puts the rock into the catapult's cup.
void cCatapult214::setRock()
{
    Vec p;
    Vec v = {0.0f, 0.0f, 0.0f};

    v.x = r214_rockOfs.x / obj->scale.x;
    v.y = r214_rockOfs.y / obj->scale.y;
    v.z = r214_rockOfs.z / obj->scale.z;
    p = v;
    rock->pos = p;
    rock->setParent((cEm*) obj, 0, 0);
}

// Task: the arm swings, the rock flies, the arm comes back.
static void r214_throwRock(cCatapult214* c)
{
    u32 i;
    f32 spd;
    f32 lim;

    c->obj->pList->ang.x = -0.24137f;
    for (i = 10; i > 0; i--) {
        c->obj->pList->ang.x += 0.13986999f;
        SceSleep(1);
    }
    c->throwRock();
    // Loop 2 is peeled by the post-loop jump pass (duplicate_loop_exit_test), not by jump1: with
    // `lim` in the compare but the LITERAL in the store, the exit code has > 20 insns before cse1
    // (no early peel, so loop.c still hoists) and < 20 after loop.c (peel), and the store's constant
    // is its own hoisted pseudo (`fmr f28,f30` in the preheader). `acc` is only pool order.
    const f32 acc = -0.034906585f;
    spd = -0.06981317f;
    lim = -0.24137f;
    for (;;) {
        c->obj->pList->ang.x += spd;
        spd += acc;
        if (c->obj->pList->ang.x < lim) {
            c->obj->pList->ang.x = -0.24137f;
            break;
        }
        SceSleep(1);
    }
    // Loop 3: a plain `for (;;)` inner loop (rotated by expand_end_loop into the target's sleep-first
    // layout, its 0.0 compare constant hoisted only to the outer body's head) inside a counted outer
    // loop whose `(int) i < 4` on the u32 counter gives the target's signed `cmpwi r31,3` without
    // loop.c reversing it (a plain `int` counter is reversed: `li 3; addi -1; cmpwi 0`).
    for (i = 0; (int) i < 4; i++) {
        lim *= 0.5f;
        spd *= -0.5f;
        for (;;) {
            c->obj->pList->ang.x += spd;
            spd += -0.034906585f;
            if (c->obj->pList->ang.x < lim && spd < 0.0f) {
                break;
            }
            SceSleep(1);
        }
        c->obj->pList->ang.x = lim;
    }
}

// Launches the rock at the target area (or the player).
void cCatapult214::throwRock()
{
    Vec from;
    Vec to;
    Vec spd;

    from.x = rock->pList->mat[0][3];
    from.y = rock->pList->mat[1][3];
    from.z = rock->pList->mat[2][3];
    if (fixedTarget == 1) {
        to = target;
    } else if (atArea[hitIdx] >= 0) {
        AreaGetInsidePos(&to, &SceAtPtr(atArea[hitIdx])->area);
    } else {
        r214_getTargetPos(&to);
    }
    CalcParabolaVector(&spd, &from, &to, height);
    rock->setThrow2(&spd, 0);
    thrown = 1;
}

// A point between the player and a spot ahead of him.
void r214_getTargetPos(Vec* out)
{
    Vec a = {0.0f, 0.0f, 1000.0f};
    Vec b = {0.0f, 0.0f, -1000.0f};

    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    SatMgr.hitCheck(&pPL->pos, &a, &a, 0, 0, 0);
    PSVECSubtract(&a, &b, &a);
    PSVECScale(&a, &a, fRand0_1());
    PSVECAdd(&a, &b, out);
}

// The bridge turns (with the screenshot mode on the debug bit).
static void r214_BridgeRotate()
{
    cObj* o15 = SmdGetObjPtr(0x15);
    cObj* o16 = SmdGetObjPtr(0x16);

    if (pG->Room_flg[1] & 0x80000000) {
        r214_debugMode = pG->debug_mode;
        pG->debug_mode = 0;
        ScreenShotStart("D:/bio4/Room/Sc_shot/r214_ev", 0, 1);
    }
    SceEventStart(0);
    SceExec(6, (TaskFunc) r214_BridgeRotateCamera, 0, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(30);
    if (!(pG->Room_flg[1] & 0x80000000)) {
        SceSetEventCancel(1, (TaskFunc) r214_BridgeRotateEndProc, 0, -1, 1);
    }
    while (o15->ang.y < 1.5707964f) {
        o15->ang.y += 0.01f;
        o15->matUpdate();
        o16->ang.y += 0.01f;
        o16->matUpdate();
        SceSleep(1);
    }
    while (!(pG->Room_flg[0] & 0x80000000)) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r214_BridgeRotateEndProc();
}

// End of the bridge rotation (also its cancel path): both halves 0x15/0x16 snapped to 90 degrees,
// camera back, SceEventEnd, the screenshot debug mode restored if it was on; when the rotation was
// started from the entrance the door area 8 runs.
static void r214_BridgeRotateEndProc()
{
    cObj* o15 = SmdGetObjPtr(0x15);
    cObj* o16 = SmdGetObjPtr(0x16);

    o15->ang.y = 1.5707964f;
    o15->matUpdate();
    o16->ang.y = 1.5707964f;
    o16->matUpdate();
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (pG->Room_flg[1] & 0x80000000) {
        pG->Room_flg[1] &= ~0x80000000;
        ScreenShotEnd();
        pG->debug_mode = r214_debugMode;
    }
    pG->Room_flg[1] &= ~0x40000000;
    if (r214_work->bridgeFlag == 1) {
        SceAtExecute(8);
    }
}

// Camera cuts 3..5 during the bridge rotation, then Room_flg[0] bit 31 (the camera part is over).
static void r214_BridgeRotateCamera()
{
    u32 cut;

    for (cut = 3; cut < 6; cut++) {
        CamCtrl.CutCall((s8) cut);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    SceSleep(30);
    pG->Room_flg[0] |= 0x80000000;
}

// Event r214s00 callback: scroll object 0x18 hidden; light mask 8 on evma900; cut 0 closes the
// binocular view if it was up (Status_flg[0] 0x400); later cuts set model flags.
void Evt_R214S00_Func(Event* e)
{
    void* mod;

    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0x18, 0);
        break;
    case 1:
        if (e->NowCut == 0 && e->NowFrame == 0) {
            if (e->GetMod(&mod, "evma900", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 8;
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0 && (StaFlagChk(pG, STA_BINOCULAR))) {
                StaFlagOff(pG, STA_BINOCULAR);
                r214_work->bino->quit(&pG->Camera);
                r214_work->bino->~IdBinocular();
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            if (e->NowFrame == 0 && !StaFlagChk(pG, STA_BINOCULAR)) {
                StaFlagOn(pG, STA_BINOCULAR);
                r214_work->bino = new (&r214_work->binoObj) IdBinocular;
                r214_work->bino->init(&pG->Camera, ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23));
                if (e->NowCut != 1) {
                    r214_work->bino->cutin(0);
                }
                r214_work->focus = &r214_work->focusObj;
                r214_work->focus->init(-1);
            }
            r214_work->bino->move(&pG->Camera);
            break;
        }
        break;
    case 2:
        SmdSetTrans(0x18, 1);
        if (StaFlagChk(pG, STA_BINOCULAR)) {
            StaFlagOff(pG, STA_BINOCULAR);
            r214_work->bino->quit(&pG->Camera);
            r214_work->bino->~IdBinocular();
        }
        break;
    }
}
