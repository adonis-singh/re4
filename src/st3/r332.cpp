#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj18.h"
#include "objPillar.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "em31.h"
#include "emhit.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "cockpit.h"
#include "act_btn.h"
#include "snd.h"
#include "est.h"
#include "esp.h"
#include "fade.h"
#include "game.h"
#include "shadow.h"
#include "etc_model.h"
#include "motion.h"
#include "math_sub.h"
#include "rnd.h"
#include "vec.h"
#include "cmath.h"
#include "eprintf.h"
#include "dvd.h"
#include "TexRender.h"
#include "db_log.h"
#include "st_mgr_event.h"
#include <string.h>

// Room 3-32 (D:/Bio4/Prog/r332.cpp): the final battle. The two cranes drop their steel beams on the
// boss (R332ExecCrane), the two bridges the boss opens and closes (R332BridgeTask, the player runs
// across them by the action button), the down / rocket cut scenes and the s00/s10/s20 events.


cObj* SetPillar(void* bin, void* tpl, Vec* pos, Vec* rot);
void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp

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


struct R332Bridge {
    int open;   // 0x0  1 while the bridge is open
    int cnt;    // 0x4  frames since the bridge closed
};

struct R332Work {
    cObj* crane[2];         // 0x000  the two cranes (the beam is model 1)
    cObj* chain[2][5];      // 0x008  five chain links hanging from parts 4 of each crane
    cObjPillar* pillar[3];  // 0x030  the fallen beams the boss throws
    cEmHit* hit[2];         // 0x03C  hit box following the beam of each crane
    cEmWrap em[2];          // 0x044  [0] the boss body (list 0xA8), [1] its tentacle (list 0xA9)
    TexRenderMng* tex;      // 0x05C
    f32 dieY;               // 0x060  fall of the player crushed on the bridge
    cObj* rocket;           // 0x064  the special rocket while the die cut plays
    int x68;                // 0x068
    Vec plPos;              // 0x06C  player position saved by the rocket cut
    Vec plRot;              // 0x078
    int btnCnt;             // 0x084  action button presses on the bridge
    int timer;              // 0x088
    int x8C;                // 0x08C
    int nearBridge;         // 0x090  R332ChkNearBridge result when the run started
    SCE_TASK* task[2];       // 0x094  the bridge tasks
    R332Bridge bridge[2];   // 0x09C
    cSat* sat[4];           // 0x0AC  collision pieces of the two bridges
    int strBlk;             // 0x0BC  SndStrPlayBlock handle
    CAMERA cam;             // 0x0C0  the crane camera
};

// The original object's .data is 8-aligned (0x260 in the REL after r330's 12-byte table).
asm(".section .data; .balign 8");
static SceElevatorData r332_elv = {1, 0x24, {-45093.0f, 15811.0f, 47400.0f}, {-45120.0f, 15800.0f, 47200.0f}, {0.0f, 1.54f, 0.0f}, 7, 0, 5, 0, 7, {-44950.0f, 1745.0f, 47280.0f}, {0.0f, 1.49f, 0.0f}, 0x331};
static f32 r332_craneUpY[2] = {21180.0f, 19670.0f};
static f32 r332_craneDownY[2] = {20910.0f, 19400.0f};
Vec r332_satPos[4] = {{-49272.0f, 17311.0f, 59394.0f}, {-38880.0f, 15811.0f, 59394.0f}, {-49272.0f, 17311.0f, 83192.0f}, {-38880.0f, 15811.0f, 83192.0f}};
static int r332_btnFrame = 40;

static f32 r332_craneRange;
static int r332_craneRangeSet;
static u8 r332_texTbl[0x20];
static R332Work* r332_work;

// Bridge open angles in radians (a: -ang[0], b: ang[1]) and the closed angle.
static const f32 r332_bridgeAng[2] = {83.0f * PI / 180.0f, 97.0f * PI / 180.0f};
#define R332_FLAT 0.0f

// st3.cpp's count-down helpers
void st3_setCountDownTimer(int frame);
void st3_startCountDown();
void st3_checkCountDown();

static void R332EmSetMain();
static void playerDieBridge(cPlayer* pl);
static void playerBridge(cPlayer* pl);
extern "C" int R332ChkNearBridge();
extern "C" void R332BridgeInit(int no, int open);
extern "C" void R332BridgeOpened(int no, int open);
extern "C" void R332BridgeOpen(int no, int open);
static void R332BridgeTask(int no);
static void R332BossDown();
static void R332BossDownEnd();
static void R332RocketShootMain(int type);
static void R332RocketShootEnd(int type);
static void R332RevaCommonMoveDw(int no);
static void R332RevaCommonMoveUp(int no);
extern "C" void R332RevaCommonMove(int no, int up);
static void R332ExecCrane(int no);
extern "C" void R332ExecCraneEnd(int no, int atNo);
static void R332EventS00();
static void R332EventS00Cancel();
extern "C" void R332EventS00End();
static void R332EventS10();
static void R332EventS20();
extern "C" void R332Em32RocketDie(cObj* obj);
extern "C" void R332ScrTrans(int on);
extern "C" void Evt_R332S00_Func(Event* e);
extern "C" void Evt_R332S10_Func(Event* e);
extern "C" void Evt_R332S20_Func(Event* e);
static void setTexRender();

// The room's flag words from pG->flags_174 on, one bit per number (0x40/0x41 are the bits of 0x17C).
static inline u32 r332_flgCk(u32* f, int no)
{
    return f[(u32) no >> 5] & (0x80000000 >> (no & 31));
}


// Through the manager pointer (an inline `this`): `&CamCtrl` in a register, the field at 0x250 off it.
static inline void CamCtrlSetCam(CameraControl* cc, CAMERA* cam)
{
    cc->SetExtraCamera(cam);
}

#define R332_FLAGS ((u32*) &pG->Room_flg[0])

// The bridge state is stored by byte offset from the first bridge's field (a cast-then-deref
// store: it kills the cached pG / r332_work like the other work stores).
#define R332_BRIDGE_SET(field, no, v)                        \
    {                                                        \
        int ofs_ = (no) * 8;                                 \
        int* p_ = &r332_work->bridge[0].field;               \
                                                             \
        *(int*) ((u8*) p_ + ofs_) = (v);                     \
    }
// The work's object arrays the same way (the object created first, then the index: the pillar loop's
// giv increments come out `i*4` before `&pillarPos[i]`, the reverse of the givs' discovery order).
#define R332_ARR_SET(field, ofs, v)                          \
    {                                                        \
        void* t_ = (v);                                      \
        int ofs_ = (ofs);                                    \
        void** p_ = (void**) &r332_work->field;              \
                                                             \
        *(void**) ((u8*) p_ + ofs_) = t_;                    \
    }
// The bridge task handles the same way (the task is started first).
#define R332_TASK_SET(no, v)                                 \
    {                                                        \
        SCE_TASK* t_ = (v);                                   \
        int ofs_ = (no) * 4;                                 \
        SCE_TASK** p_ = &r332_work->task[0];                  \
                                                             \
        *(SCE_TASK**) ((u8*) p_ + ofs_) = t_;                 \
    }


// Room init (the final battle arena): Ashley marked separated; area 0 = the elevator out; the s00/s10/
// s20 callbacks. Until the boss appeared (Room_flg bit 0) the s00 event task runs and the arena is set
// for it; after the boss is dead (bits 1/4) the revisit layout (R332EmSetMain); the special rocket area
// 0x84 only once earned (bit 2). Both bridges posed open, the two cranes (beams still hanging per the
// per-crane flags) with their lever areas 1/2, the render target.
void R332Init()
{
    int i;
    int j;

#line 143 "D:/Bio4/Prog/r332.cpp"
    r332_work = (R332Work*) MEM_CALLOC(sizeof(R332Work), 1, 0xd);
    StaFlagOn(pG, STA_SAVEDATA_NO_UPDATE);
    r332_work->rocket = 0;
    r332_work->x68 = 0;
    r332_work->task[0] = 0;
    r332_work->task[1] = 0;
    SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) SceElevator, &r332_elv, 1);
    SceAtSetActColor(0, 1);
    EvtMgr.SetFunc("evt_r332s00_func", (void*) Evt_R332S00_Func);
    EvtMgr.SetFunc("evt_r332s10_func", (void*) Evt_R332S10_Func);
    EvtMgr.SetFunc("evt_r332s20_func", (void*) Evt_R332S20_Func);
    EvtMgr.SetFunc("evt_r332s97_func", (void*) Evt_R332S00_Func);
    EvtMgr.SetFunc("evt_r332s98_func", (void*) Evt_R332S10_Func);
    EvtMgr.SetFunc("evt_r332s99_func", (void*) Evt_R332S20_Func);
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE) == 0) {
        u32 size0;
        u32 size1;
        u32 size;
        char* p;

        SceExec(0x12, (TaskFunc) R332EventS00, 0, 2, 2, 0);
        p = strstr("event/evd/r332s00.evd", "evd/");
        if (p) {
            Dvd.FileExistCheck(p, &size0);
        }
        p = strstr("event/evd/r332s10.evd", "evd/");
        if (p) {
            Dvd.FileExistCheck(p, &size1);
        }
        if (size0 > size1) {
            size = size0;
        } else {
            size = size1;
        }
        EvtMgr.EvtReadAram("event/evd/r332s00.evd", (u8) GetEmIdFromList(0xA9), 0, 0, size);
        SceAtSetEnable(0, 0);
        SceAtSetEnable(9, 1);
        KyfFlagOff(pG, KYF_ST1_24);
        SmdSetTrans(7, 0);
    } else {
        SceExec(0x12, (TaskFunc) R332EmSetMain, 0, 0, 2, 0);
        st3_startCountDown();
        SceAtSetEnable(0, 1);
        SceAtSetEnable(9, 0);
        KyfFlagOn(pG, KYF_ST1_24);
        SmdSetTrans(0xA, 1);
        SndBgmTblSet(0x332, 1);
        SndRoomStrStart(1, 0, 1);
        SndRoomBgmStart(0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xF, 1, ESP_CORE_KIND_NONE, 0, 0);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtSetEnable(0x84, 0);
    }
    R332BridgeOpened(0, 1);
    R332BridgeOpened(1, 1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_ROOM01, 0, 0);
    Vec zero = {0.0f, 0.0f, 0.0f};
    r332_work->sat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r332_satPos[0], &zero, 2);
    r332_work->sat[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r332_satPos[1], &zero, 4);
    r332_work->sat[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r332_satPos[2], &zero, 3);
    r332_work->sat[3] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &r332_satPos[3], &zero, 1);
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    Vec cranePos[2] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    Vec craneRot[2] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    void* craneMot[2] = {ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22)};
    void* chainMot[5] = {ROOM_ARC_PTR(pG->pRoom, 0x3A), ROOM_ARC_PTR(pG->pRoom, 0x3B), ROOM_ARC_PTR(pG->pRoom, 0x3C),
                         ROOM_ARC_PTR(pG->pRoom, 0x3D), ROOM_ARC_PTR(pG->pRoom, 0x3E)};
    int flagNo[2] = {6, 7};
    int smdNo[2] = {0x21, 0x20};

    for (i = 0; i < 2; i++) {
        cObj* crane;

        R332_ARR_SET(crane[0], i * 4, SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &cranePos[i], &craneRot[i], 0x10, 0));
        crane = r332_work->crane[i];
        if (crane) {
            cModelInfo* info;

            info = ModInfoMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x3F), ROOM_ARC_PTR(pG->pRoom, 0x20));
            if (info) {
                crane->addModel(info);
            }
            crane->be_flag |= 0x1000;
            MotionSetCore(crane, &crane->Motion, craneMot[i], 0, 0, 5, 0);
            // The reference-view store keeps the following `pG` load below it (r30c PSetPtr).
#line 372 "D:/Bio4/Prog/r332.cpp"
            (crane->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xd));
            R332_ARR_SET(hit[0], i * 4, SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), 0, 0, 1));
            if (r332_work->hit[i]) {
                r332_work->hit[i]->setParent(crane, 4, 0);
            }
            // `&flagNo` computed at the block top: loop.c hoists it (life 4 >= 242/68) into r14 ahead
            // of the hit block's `r332_work@ha` (a priority tie at 92, decided by the pseudo number).
            int* pf = flagNo;
            if (RsfCheck(G_ROOM_ID, pf[i]) == 0) {
                for (j = 0; j < 5; j++) {
                    Vec pos = {0.0f, 0.0f, 0.0f};
                    Vec rot = {0.0f, 0.0f, 0.0f};
                    cObj* chain;

                    R332_ARR_SET(chain[0][0], i * 0x14 + j * 4, SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x38), ROOM_ARC_PTR(pG->pRoom, 0x39), &pos, &rot, 0x10, 0));
                    chain = r332_work->chain[i][j];
                    if (chain) {
                        chain->be_flag |= 0x1000;
                        MotionSetCore(chain, &chain->Motion, chainMot[j], 0, 0, 4, 0);
                        chain->setParent(crane, 4, &pos, &rot);
                    }
                }
                {
                    cObj* smd = SmdGetObjPtr(smdNo[i]);

                    smd->setPos(smd->pos.x, r332_craneUpY[i], smd->pos.z);
                }
            } else {
                cObj* smd;

                ModelInfoSetTrans(crane, 1, 0);
                smd = SmdGetObjPtr(smdNo[i]);
                smd->setPos(smd->pos.x, r332_craneDownY[i], smd->pos.z);
            }
        }
    }
    if (ScfFlagChk(pG, SCF_R332_BOSS_DIE) == 0) {
        SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) R332ExecCrane, 0, 1);
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) R332ExecCrane, (void*) 1, 1);
    }
    Vec pillarPos[3] = {{-32800.0f, 15811.0f, 63600.0f}, {-32900.0f, 15811.0f, 73500.0f}, {-55000.0f, 17311.0f, 72000.0f}};
    Vec pillarRot[3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    for (i = 0; i < 3; i++) {
        R332_ARR_SET(pillar[0], i * 4, SetPillar(ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), &pillarPos[i], &pillarRot[i]));
        if (r332_work->pillar[i]) {
            r332_work->pillar[i]->setMotion(ROOM_ARC_PTR(pG->pRoom, 0x29));
        }
    }
    setTexRender();
}

// Per frame during the fight (bit 0 set, bits 1/3/4 clear): the special rocket hit (Room_flg[0]
// 0x08000000) or the boss's hp reaching 0 starts the rocket cut (type 0 / 1, bits 3 / 4); the special
// rocket is thrown (s20, bit 2) when the boss allows it; the crane beam knocks it down (bit 5); the
// bridges close when the fight state asks; after the fight both bridges are shut.
void R332Main()
{
    if (RsfCheck(G_ROOM_ID, 0) && RsfCheck(G_ROOM_ID, 1) == 0 && RsfCheck(G_ROOM_ID, 4) == 0 && RsfCheck(G_ROOM_ID, 3) == 0) {
        cEm31* em = (cEm31*) r332_work->em[0].getPtr();

        if (em) {
            if (RsfCheck(G_ROOM_ID, 3) == 0 && (pG->Room_flg[0] & 0x08000000)) {
                RsfSet(G_ROOM_ID, 3);
                SceExec(0x12, (TaskFunc) R332RocketShootMain, 0, 0, 2, 0);
                return;
            }
            if (RsfCheck(G_ROOM_ID, 4) == 0 && em->hp <= 0) {
                RsfSet(G_ROOM_ID, 4);
                SceExec(0x12, (TaskFunc) R332RocketShootMain, 1, 0, 2, 0);
                return;
            }
            if (RsfCheck(G_ROOM_ID, 2) == 0 && (em->ckRocketEnable() || DebugTrg(1))) {
                RsfSet(G_ROOM_ID, 2);
                SceExec(0x12, (TaskFunc) R332EventS20, 0, 0, 2, 0);
                return;
            }
        }
        em = (cEm31*) r332_work->em[1].getPtr();
        if (em && RsfCheck(G_ROOM_ID, 5) == 0 && em->ckDownEnable() == 1) {
            RsfSet(G_ROOM_ID, 5);
            SceExec(0x12, (TaskFunc) R332BossDown, 0, 0, 2, 0);
            return;
        }
        if (ScfFlagChk(pG, SCF_R332_BOSS_DIE) == 0 && (ItfFlagChk(pG, ITF_R332_ADA_ROCKET)) && (pG->Room_flg[0] & 0x01000000) == 0) {
            pG->Room_flg[0] |= 0x01000000;
            if (r332_work->task[0]) {
                SceKill(r332_work->task[0]);
            }
            if (r332_work->task[1]) {
                SceKill(r332_work->task[1]);
            }
            r332_work->task[0] = 0;
            r332_work->task[1] = 0;
            R332BridgeOpened(0, 0);
            R332BridgeOpened(1, 0);
        }
    }
    st3_checkCountDown();
}

// Revisit: the boss is already dead, only the handles are set.
static void R332EmSetMain()
{
    SceSleep(1);
    r332_work->em[0].setEm(0xA8, -1, 1, 1, 1);
    r332_work->em[1].setEm(0xA9, -1, 1, 1, 1);
    r332_work->em[0].destroy();
    r332_work->em[1].destroy();
    CamCtrl.clearAttachCamera();
}

// SetPlDamage routine: crushed by the closing bridge.
static void playerDieBridge(cPlayer* pl)
{
    f32 start = 100.0f;
    f32 step = 10.0f;

    switch (pl->r_no_2) {
    case 0:
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2F), 0, 3, 0x201, 0);
        pl->atari.off();
        pl->be_flag &= ~0x10;
        PlSetDamageSe(0xA);
        r332_work->dieY = start;
        pl->r_no_2++;
    case 1:
        if (pl->Motion.Seq_frame > 22.7f && pl->Motion.Seq_frame < 23.3f) {
            PlSetDamageSe(0xD);
            pl->r_no_2++;
            pl->r_no_3 = 0;
        }
        MotionMove(pl, 0);
        break;
    case 2:
        pl->r_no_3++;
        if (pl->r_no_3 > 29) {
            FadeSetW(2, 60, 0, 0);
            pl->r_no_3 = 0;
            pl->r_no_2++;
        }
        break;
    case 3:
        pl->r_no_3++;
        if (pl->r_no_3 > 59) {
            StaFlagOn(pG, STA_EVENT_CANCEL);
            DiedemoExec(0, 1);
            pl->r_no_2++;
        }
        break;
    }
    pl->dmg.m_Timer = 0x78;
    r332_work->dieY += step;
    pl->setPos(pl->pos.x, pl->pos.y - r332_work->dieY, pl->pos.z);
    {
        f32 fovy = 50.0f;
        Vec camPos;
        Vec camAt;

        camPos.x = pl->pos.x;
        camPos.y = 30000.0f;
        camPos.z = pl->pos.z + 2000.0f;
        camAt.x = pl->pos.x;
        camAt.y = pl->pos.y;
        camAt.z = pl->pos.z;
        SceCamMove(&camPos, &camAt, fovy);
    }
}

// SetPlDamage routine: the run across the bridge (button mashing against the timer).
static void playerBridge(cPlayer* pl)
{
    int no = r332_work->nearBridge;
    Vec pos0[4] = {{-42094.2f, 15861.0f, 81852.7f}, {-45909.1f, 17360.9f, 82094.8f}, {-42094.0f, 15861.0f, 58030.1f}, {-45909.1f, 17360.9f, 58270.8f}};
    Vec pos1[4] = {{-39097.2f, 15861.0f, 81905.4f}, {-49058.3f, 17360.9f, 82042.0f}, {-39097.2f, 15861.0f, 58083.3f}, {-49058.3f, 17360.9f, 58271.2f}};
    Vec pos2[4] = {{-39098.9f, 15861.0f, 81898.6f}, {-48904.4f, 17360.9f, 82048.9f}, {-39098.9f, 15861.0f, 58076.5f}, {-48904.4f, 17360.9f, 58224.8f}};
    Vec rot[4] = {{0.0f, 1.5707964f, 0.0f}, {0.0f, -1.5707964f, 0.0f}, {0.0f, 1.5707964f, 0.0f}, {0.0f, -1.5707964f, 0.0f}};

    switch (pl->r_no_2) {
    case 0:
        Cckpt.lifeMeterDisp(0);
        pl->atari.off();
        pG->Room_flg[0] |= 0x10000000;
        pPL->pos.x = pos0[no].x;
        pPL->pos.y = pos0[no].y;
        pPL->pos.z = pos0[no].z;
        pPL->ang.x = rot[no].x;
        pPL->ang.y = rot[no].y;
        pPL->ang.z = rot[no].z;
        pPL->be_flag &= ~0x10;
        MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2C), 0, 0xA, 0x201, 0);
        r332_work->btnCnt = 0;
        r332_work->timer = 0;
        pl->r_no_2++;
    case 1:
        pl->dmg.m_Timer = 0x78;
        if (pl->Motion.Seq_frame >= (f32) r332_btnFrame) {
            if (Key.trg & 0x00080000) {
                r332_work->btnCnt = r332_work->btnCnt + 1;
            }
            ActBtn.set(ACT_CLIMB, 0xC, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
            SceDebugDisp("Button:[%d/%d]", r332_work->btnCnt, 0xA);
        }
        if (pl->Motion.Seq_frame > 9.7f && pl->Motion.Seq_frame < 10.3f) {
            SndCall(1, 0x10, &pPL->pos, 0, 0, 0);
        }
        if (pl->Motion.Seq_frame > 23.7f && pl->Motion.Seq_frame < 24.3f) {
            SndCall(1, 0x34, &pPL->pos, 0, 0, 0);
        }
        if (pl->Motion.Seq_frame > 29.7f && pl->Motion.Seq_frame < 30.3f) {
            SndCall(1, 0x4F, &pPL->pos, 0, 0, 0);
        }
        if (MotionMove(pl, 0)) {
            pPL->pos.x = pos1[no].x;
            pPL->pos.y = pos1[no].y;
            pPL->pos.z = pos1[no].z;
            pPL->ang.x = rot[no].x;
            pPL->ang.y = rot[no].y;
            pPL->ang.z = rot[no].z;
            MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2D), 0, 0xA, 0x205, 0);
            MotionMove(pl, 0);
            pl->r_no_2++;
        }
        break;
    case 2:
        pl->dmg.m_Timer = 0x78;
        if (Key.trg & 0x00080000) {
            r332_work->btnCnt = r332_work->btnCnt + 1;
        }
        ActBtn.set(ACT_CLIMB, 0xC, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
        MotionMove(pl, 0);
        r332_work->timer = r332_work->timer + 1;
        SceDebugDisp("Button:[%d/%d]", r332_work->btnCnt, 0xA);
        SceDebugDisp("Timer: [%d/%d]", r332_work->timer, 0x5A);
        if (r332_work->timer > 0x5A) {
            pPL->pos.x = pos2[no].x;
            pPL->pos.y = pos2[no].y;
            pPL->pos.z = pos2[no].z;
            pPL->ang.x = rot[no].x;
            pPL->ang.y = rot[no].y;
            pPL->ang.z = rot[no].z;
            if (r332_work->btnCnt > 0xA) {
                MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2E), 0, 3, 0x201, 0);
                MotionMove(pl, 0);
                r332_work->strBlk = SndStrPlayBlock(1, 0x2F, 0.0f);
                pl->r_no_2++;
            } else {
                pG->pl_life = 0;
                pl->atari.off();
                SndCall(1, 0x4A, &pPL->pos, 0, 0, 0);
                MotionSetCore(pl, &pl->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2F), 0, 3, 0x201, 0);
                MotionMove(pl, 0);
                pl->r_no_2 = 0xA;
            }
        }
        break;
    case 3:
        pl->dmg.m_Timer = 0x78;
        if (MotionMove(pl, 0)) {
            pPL->be_flag |= 0x10;
            pG->Room_flg[0] &= ~0x10000000;
            pl->atari.on();
            SceEventStart(0);
            SceEventEnd(0);
            EndPlDamage();
        }
        break;
    case 0xA:
        MotionMove(pl, 0);
        break;
    }
}

// Which bridge end the player stands at: 4 none, 3/2 bridge 0 (facing -/+), 1/0 bridge 1.
int R332ChkNearBridge()
{
    int ret = 4;

    if (pG->Room_flg[2] & 0x80000000) {
        ret = 3;
        if (pPL->ang.y > 0.0f) {
            ret = 2;
        }
    }
    if (pG->Room_flg[2] & 0x40000000) {
        ret = 1;
        if (pPL->ang.y > 0.0f) {
            ret = 0;
        }
    }
    return ret;
}

// Bridge `no`: open state and cycle counter reset.
void R332BridgeInit(int no, int open)
{
    R332_BRIDGE_SET(open, no, open);
    R332_BRIDGE_SET(cnt, no, 0);
}

// Put bridge `no` in its open / closed pose at once.
void R332BridgeOpened(int no, int open)
{
    int smdA;
    int smdB;
    int atNo;
    int estNo;
    int satA;
    int satB;
    int estPrm;
    cObj* a;
    cObj* b;

    R332_BRIDGE_SET(open, no, open);
    R332_BRIDGE_SET(cnt, no, 0);
    if (no == 0) {
        smdA = 0xF;
        smdB = 0x10;
        atNo = 5;
        estNo = 2;
        satA = 0;
        satB = 1;
    } else {
        smdA = 0x11;
        smdB = 0x12;
        atNo = 6;
        estNo = 3;
        satA = 2;
        satB = 3;
    }
    if (no == 0) {
        estPrm = 5;
        if (r332_work->bridge[no].open == 0) {
            estPrm = 7;
        }
    } else {
        estPrm = 6;
        if (r332_work->bridge[no].open == 0) {
            estPrm = 8;
        }
    }
    a = SmdGetObjPtr(smdA);
    if (a == 0) {
        return;
    }
    b = SmdGetObjPtr(smdB);
    if (b == 0) {
        return;
    }
    if (r332_work->bridge[no].open == 0) {
        EffectEspDelete(1, (u8) estNo, 0, 0);
        EffectEspgenDelete(1, (u8) estNo, 0);
        EffectEfmDelete(1, (u8) estNo, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, estPrm, 1, (u8) estNo, 0, 0);
        SceAtSetEnable(atNo, 1);
        a->setAng(a->ang.x, a->ang.y, -r332_bridgeAng[0]);
        b->setAng(b->ang.x, b->ang.y, r332_bridgeAng[1]);
        if (r332_work->sat[satA]) {
            r332_work->sat[satA]->setCoord(&r332_satPos[satA], &a->ang);
        }
        if (r332_work->sat[satB]) {
            r332_work->sat[satB]->setCoord(&r332_satPos[satB], &b->ang);
        }
    } else {
        a->setAng(a->ang.x, a->ang.y, -R332_FLAT);
        b->setAng(b->ang.x, b->ang.y, R332_FLAT);
        if (r332_work->sat[satA]) {
            r332_work->sat[satA]->setCoord(&r332_satPos[satA], &a->ang);
        }
        if (r332_work->sat[satB]) {
            r332_work->sat[satB]->setCoord(&r332_satPos[satB], &b->ang);
        }
        SceAtSetEnable(atNo, 0);
        EffectEspDelete(1, (u8) estNo, 0, 0);
        EffectEspgenDelete(1, (u8) estNo, 0);
        EffectEfmDelete(1, (u8) estNo, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, estPrm, 1, (u8) estNo, 0, 0);
    }
}

// Swing bridge `no` open / closed over a few frames. Each loop has its own counter (r27 / r30 / r28):
// one shared `i` would keep a single pseudo in r27 and push `estNo` out of r30.
void R332BridgeOpen(int no, int open)
{
    int smdA;
    int smdB;
    int atNo;
    int estNo;
    int satA;
    int satB;
    int estPrm;
    cObj* a;
    cObj* b;
    Vec center;

    R332_BRIDGE_SET(open, no, open);
    R332_BRIDGE_SET(cnt, no, 0);
    if (no == 0) {
        smdA = 0xF;
        smdB = 0x10;
        atNo = 5;
        estNo = 2;
        satA = 0;
        satB = 1;
    } else {
        smdA = 0x11;
        smdB = 0x12;
        atNo = 6;
        estNo = 3;
        satA = 2;
        satB = 3;
    }
    if (no == 0) {
        estPrm = 5;
        if (r332_work->bridge[no].open == 0) {
            estPrm = 7;
        }
    } else {
        estPrm = 6;
        if (r332_work->bridge[no].open == 0) {
            estPrm = 8;
        }
    }
    a = SmdGetObjPtr(smdA);
    if (a == 0) {
        return;
    }
    b = SmdGetObjPtr(smdB);
    if (b == 0) {
        return;
    }
    center.x = (a->pos.x + b->pos.x) * 0.5f;
    center.y = (a->pos.y + b->pos.y) * 0.5f;
    center.z = (a->pos.z + b->pos.z) * 0.5f;
    if (r332_work->bridge[no].open == 0) {
        EffectEspDelete(1, (u8) estNo, 0, 0);
        EffectEspgenDelete(1, (u8) estNo, 0);
        EffectEfmDelete(1, (u8) estNo, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, estPrm, 1, (u8) estNo, 0, 0);
        SndCall(6, 0, &center, 0, 0, 0);
        SceAtSetEnable(atNo, 1);
        for (int i = 0; i < 10; i++) {
            f32 za = (r332_bridgeAng[0] - R332_FLAT) * (f32) i / 10.0f + R332_FLAT;
            f32 zb = (r332_bridgeAng[1] - R332_FLAT) * (f32) i / 10.0f + R332_FLAT;

            a->setAng(a->ang.x, a->ang.y, -za);
            b->setAng(b->ang.x, b->ang.y, zb);
            if (r332_work->sat[satA]) {
                r332_work->sat[satA]->setCoord(&r332_satPos[satA], &a->ang);
            }
            if (r332_work->sat[satB]) {
                r332_work->sat[satB]->setCoord(&r332_satPos[satB], &b->ang);
            }
            SceSleep(1);
        }
        a->setAng(a->ang.x, a->ang.y, -r332_bridgeAng[0]);
        b->setAng(b->ang.x, b->ang.y, r332_bridgeAng[1]);
        if (r332_work->sat[satA]) {
            r332_work->sat[satA]->setCoord(&r332_satPos[satA], &a->ang);
        }
        if (r332_work->sat[satB]) {
            r332_work->sat[satB]->setCoord(&r332_satPos[satB], &b->ang);
        }
        for (int i = 0; i < 5; i++) {
            a->setAng(a->ang.x, a->ang.y, -r332_bridgeAng[0] + fRand1_1() * PI / 180.0f * 2.0f);
            b->setAng(b->ang.x, b->ang.y, r332_bridgeAng[1] + fRand1_1() * PI / 180.0f * 2.0f);
            SceSleep(1);
        }
    } else {
        SndCall(6, 1, &center, 0, 0, 0);
        for (int i = 0; i < 50; i++) {
            f32 za = (R332_FLAT - r332_bridgeAng[0]) * (f32) i / 50.0f + r332_bridgeAng[0];
            f32 zb = (R332_FLAT - r332_bridgeAng[1]) * (f32) i / 50.0f + r332_bridgeAng[1];

            a->setAng(a->ang.x, a->ang.y, -za);
            b->setAng(b->ang.x, b->ang.y, zb);
            if (r332_work->sat[satA]) {
                r332_work->sat[satA]->setCoord(&r332_satPos[satA], &a->ang);
            }
            if (r332_work->sat[satB]) {
                r332_work->sat[satB]->setCoord(&r332_satPos[satB], &b->ang);
            }
            SceSleep(1);
        }
        a->setAng(a->ang.x, a->ang.y, -R332_FLAT);
        b->setAng(b->ang.x, b->ang.y, R332_FLAT);
        if (r332_work->sat[satA]) {
            r332_work->sat[satA]->setCoord(&r332_satPos[satA], &a->ang);
        }
        if (r332_work->sat[satB]) {
            r332_work->sat[satB]->setCoord(&r332_satPos[satB], &b->ang);
        }
        SceAtSetEnable(atNo, 0);
        EffectEspDelete(1, (u8) estNo, 0, 0);
        EffectEspgenDelete(1, (u8) estNo, 0);
        EffectEfmDelete(1, (u8) estNo, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, (u8) estPrm, 1, (u8) estNo, 0, 0);
    }
}

// Bridge task: the closed bridge counts 180 frames (the player may dash across from frame 120 on),
// then swings to the other state and re-arms itself.
static void R332BridgeTask(int no)
{
    int flgArea;
    int flgOpen;

    if (no == 0) {
        flgArea = 0x40;
        flgOpen = 8;
    } else {
        flgArea = 0x41;
        flgOpen = 9;
    }
    if (r332_work->bridge[no].open == 0) {
        R332_BRIDGE_SET(cnt, no, 0);
        while (r332_work->bridge[no].cnt <= 0xB3) {
            SceDebugDisp("Timer: [%d/%d]", r332_work->bridge[no].cnt, 0xB4);
            if (r332_work->bridge[no].cnt == 0x5A) {
                int estPrm;
                int estNo;

                if (no == 0) {
                    estPrm = 2;
                    estNo = 2;
                } else {
                    estPrm = 4;
                    estNo = 3;
                }
                EffectEspDelete(1, (u8) estNo, 0, 0);
                EffectEspgenDelete(1, (u8) estNo, 0);
                EffectEfmDelete(1, (u8) estNo, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, estPrm, 1, (u8) estNo, 0, 0);
            }
            if (FlagChkVar(R332_FLAGS, (u32) flgArea) && r332_work->bridge[no].cnt > 0x77) {
                r332_work->nearBridge = R332ChkNearBridge();
                if (r332_work->nearBridge != 4) {
                    ActBtn.set(ACT_JUMP_AT, 0xC, 0, 0, ACTCTR_ENFORCE_EXEC | ACTCTR_EXACT_KEY, DISP_L_R, ACT_FUNC_SCE, 0);
                    if ((Key.trg & 0x00400000 && Key.on & 0x00800000) || (Key.on & 0x00400000 && Key.trg & 0x00800000)) {
                        SetPlDamage(0, playerBridge);
                        pG->Room_flg[0] |= 0x10000000;
                        break;
                    }
                    if (r332_work->bridge[no].cnt > 0x95 && r332_flgCk(R332_FLAGS, flgOpen) == 0) {
                        R332_BRIDGE_SET(cnt, no, 0x96);
                        SceDebugDisp("Timer Bonus!!");
                    }
                }
            }
            if (FlagChkVar(R332_FLAGS, (u32) flgArea)) {
                FlagOnVar(R332_FLAGS, (u32) flgOpen);
            } else {
                FlagOffVar(R332_FLAGS, (u32) flgOpen);
            }
            SceSleep(1);
            R332_BRIDGE_SET(cnt, no, r332_work->bridge[no].cnt + 1);
        }
        {
            u32* f = R332_FLAGS;

            if ((pG->Room_flg[0] & 0x10000000) == 0 && r332_flgCk(f, flgArea)) {
                SetPlDamage(0, playerDieBridge);
            }
        }
    } else {
        int i;

        for (i = 0; i < 0x78; i++) {
            SceSleep(1);
        }
    }
    R332BridgeOpen(no, r332_work->bridge[no].open);
    while (pG->Room_flg[0] & 0x10000000) {
        SceSleep(1);
    }
    if (r332_work->bridge[no].open == 0) {
        R332BridgeInit(no, 1);
    } else {
        R332BridgeInit(no, 0);
    }
    if (DbgFlagChk(pG, DBG_EVENT_TOOL) == 0) {
        R332_TASK_SET(no, SceExec(0x12, (TaskFunc) R332BridgeTask, no, 0, 2, 0));
    }
}

// The boss goes down (the crane beam hit it): the down cut.
static void R332BossDown()
{
    cEm31* em;

    SceEventStart(1);
    StaFlagOff(pG, STA_SUSPEND);
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        CamCtrl.deleteAttachCamera(em->Motion.pAttachCam, em);
        em->setNoSuspend(1);
        em->setDownBody();
    }
    em = (cEm31*) r332_work->em[0].getPtr();
    if (em) {
        em->setNoSuspend(1);
    }
    CamCtrl.clearAttachCamera();
    SceSetEventCancel(1, (TaskFunc) R332BossDownEnd, 0, -1, 1);
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        while (MotionGetState(em) == 0) {
            SceSleep(1);
        }
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R332BossDownEnd();
}

// End of the down cut (also its cancel path): both boss handles may suspend, the down motion cancelled
// on the boss, camera back, SceEventEnd, task exit.
static void R332BossDownEnd()
{
    cEm31* em;

    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        em->setNoSuspend(0);
        em->setDownCancel();
    }
    em = (cEm31*) r332_work->em[0].getPtr();
    if (em) {
        em->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// The rocket cut: `type` 0 the special rocket kills the boss, 1 the boss died of its wounds.
static void R332RocketShootMain(int type)
{
    cEm31* em;

    EffectEspDelete(1, ESP_CORE_KIND_ROOM02, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM02, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM03, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xD, 0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
    SndRoomStrStop(3);
    SndRoomBgmStop(0, 3);
    SndEventStrStop(0);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        EvtMgr.EvtFree("event/evd/r332s20.evd");
    }
    EvtMgr.EvtReadAram("event/evd/r332s10.evd", (u8) GetEmIdFromList(0xA9), 0, 0, 0);
    StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    ScfFlagOn(pG, SCF_R332_BOSS_DIE);
    if (r332_work->task[0]) {
        SceKill(r332_work->task[0]);
    }
    if (r332_work->task[1]) {
        SceKill(r332_work->task[1]);
    }
    r332_work->task[0] = 0;
    r332_work->task[1] = 0;
    R332BridgeOpened(0, 1);
    R332BridgeOpened(1, 1);
    r332_work->strBlk = 0;
    VEC_COPY(r332_work->plPos, pPL->pos);
    VEC_COPY(r332_work->plRot, pPL->ang);
    pPL->atari.off();
    pPL->beginEvent(0);
    pPL->setNoSuspend(1);
    CamCtrl.deleteAttachCamera(pPL->Motion.pAttachCam, pPL);
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        CamCtrl.deleteAttachCamera(em->Motion.pAttachCam, em);
        em->setNoSuspend(1);
    }
    em = (cEm31*) r332_work->em[0].getPtr();
    if (em) {
        CamCtrl.deleteAttachCamera(em->Motion.pAttachCam, em);
        em->setNoSuspend(1);
    }
    CamCtrl.clearAttachCamera();
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        if (type == 0) {
            em->setDie();
        } else {
            em->setDieNormal();
        }
    }
    if (type == 0) {
        r332_work->strBlk = SndStrPlayBlock(1, 0xE8, 0.0f);
    } else {
        r332_work->strBlk = SndStrPlayBlock(1, 0xED, 0.0f);
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    SceSetEventCancel(1, (TaskFunc) R332RocketShootEnd, type, -1, 1);
    pG->Room_flg[0] |= 0x02000000;
    if (type == 0) {
        cObjLauncher* lau;
        cObj* obj;

        pPL->pos.x = -53000.0f;
        pPL->pos.y = 17500.0f;
        pPL->pos.z = 82500.0f;
        pPL->ang.x = 0.0f;
        pPL->ang.y = 0.0f;
        pPL->ang.z = 0.0f;
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x30), 0, 0, 0x200, 0);
        EstSet(pPL, -1, 0, 0, EFF_EM31, 0x2C, 0x2001, ESP_CORE_KIND_ROOM05, 0, 0);
        pPL->Wep->m_pWep->setDisp(0, 1);
        pPL->Wep->m_pWep->setDisp(1, 1);
        pPL->Wep->m_pWep->setDisp(2, 1);
        lau = (cObjLauncher*) pPL->Wep->m_pWep;
        if (lau) {
            if (lau->pModelInfo) {
                lau->pModelInfo->color[0] = 0xA0;
                lau->pModelInfo->color[1] = 0xD0;
                lau->pModelInfo->color[2] = 0xE0;
                lau->pModelInfo->color[3] = 0xFF;
            }
            lau->LightInfo.EnableMask |= 1;
            lau->LightInfo.EnableMask &= ~0x10;
            lau->grip(1);
            ObjMgr.destroy(lau->pRocket);
            lau->pRocket = 0;
        }
        pPL->Wep->m_pWep->setNoSuspend(1);
        {
            Vec pos = {-53000.0f, 17500.0f, 82500.0f};
            Vec rot = {0.0f, 0.0f, 0.0f};

            r332_work->rocket = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x31), ROOM_ARC_PTR(pG->pRoom, 0x32), &pos, &rot, 0x10, 1);
            obj = r332_work->rocket;
            if (obj) {
                if (obj->pModelInfo) {
                    obj->pModelInfo->color[0] = 0xFF;
                    obj->pModelInfo->color[1] = 0x78;
                    obj->pModelInfo->color[2] = 0x80;
                    obj->pModelInfo->color[3] = 0xFF;
                }
                obj->LightInfo.EnableMask |= 1;
                obj->LightInfo.EnableMask &= ~0x10;
#line 1648 "D:/Bio4/Prog/r332.cpp"
                obj->Motion.pAttachCam = (AttachCamera*) MEM_ALLOC(0x98, 1, 0xd);
                obj->be_flag |= 0x1000;
                obj->setNoSuspend(1);
                MotionSetCore(obj, &obj->Motion, ROOM_ARC_PTR(pG->pRoom, 0x33), 0, 0, 0x200, 0);
                EstSet(obj, -1, 0, 0, EFF_EM31, 0x2B, 0x2001, ESP_CORE_KIND_ROOM05, 0, 0);
            }
        }
        while (MotionGetState(pPL) == 0) {
            SceSleep(1);
            SysFlagOff(pG, SYS_SCREEN_STOP);
        }
        pPL->setNoSuspend(1);
        pPL->be_flag &= ~2;
        obj = r332_work->rocket;
        if (obj) {
            while (MotionGetState(obj) == 0) {
                SceSleep(1);
            }
            obj->be_flag &= ~2;
            CamCtrl.deleteAttachCamera(obj->Motion.pAttachCam, obj);
        }
        obj = r332_work->rocket;
        if (obj) {
            ObjMgr.destroy(obj);
            r332_work->rocket = 0;
        }
        EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM05, 0, 0);
        EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM05, 0);
        EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM05, 0);
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    pPL->setNoSuspend(1);
    pPL->be_flag &= ~2;
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        while (em->checkStatus(5) == 1) {
            SceSleep(1);
        }
    }
    pG->Room_flg[0] &= ~0x02000000;
    SceSetEventCancel(0, 0, 0, -1, 1);
    R332RocketShootEnd(type);
}

// End of the rocket cut (also its cancel path): System_flg 0x400, the boss's death cancelled into its
// dead pose, the rocket object destroyed, the arena restored and the s10 event queued.
static void R332RocketShootEnd(int type)
{
    cEm31* em;

    SysFlagOn(pG, SYS_SCREEN_STOP);
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        em->setNoSuspend(0);
        em->setDieCancel();
    }
    em = (cEm31*) r332_work->em[0].getPtr();
    if (em) {
        em->setNoSuspend(0);
    }
    if (r332_work->rocket) {
        ObjMgr.destroy(r332_work->rocket);
        r332_work->rocket = 0;
    }
    pPL->setNoSuspend(0);
    pPL->be_flag |= 2;
    pPL->endEvent(0);
    pPL->pos.x = -53000.0f;
    pPL->pos.y = 17500.0f;
    pPL->pos.z = 82500.0f;
    pPL->ang.x = 0.0f;
    pPL->ang.y = 3.14f;
    pPL->ang.z = 0.0f;
    pPL->atari.on();
    if (type == 0) {
        cObjLauncher* lau;

        pPL->Wep->m_pWep->setDisp(0, 0);
        pPL->Wep->m_pWep->setDisp(1, 0);
        pPL->Wep->m_pWep->setDisp(2, 0);
        lau = (cObjLauncher*) pPL->Wep->m_pWep;
        if (lau) {
            lau->grip(0);
            lau->drop(0);
            lau->pos.x = pPL->pos.x;
            lau->pos.y = pPL->pos.y + 200.0f;
            lau->pos.z = pPL->pos.z;
        }
        pPL->Wep->m_pWep->setNoSuspend(0);
    }
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM05, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM05, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM05, 0);
    SceExec(0x12, (TaskFunc) R332EventS10, 0, 0, 2, 0);
    SysFlagOff(pG, SYS_SCREEN_STOP);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceExit();
}

// Crane `no`'s lever slides down (the player took it).
static void R332RevaCommonMoveDw(int no)
{
    R332RevaCommonMove(no, 0);
}

// Crane `no`'s lever slides back up and its area (1 / 2) is re-enabled.
static void R332RevaCommonMoveUp(int no)
{
    int atNo;

    if (no == 0) {
        atNo = 1;
    } else {
        atNo = 2;
    }
    R332RevaCommonMove(no, 1);
    SceAtSetEnable(atNo, 1);
}

// The crane lever (scroll object 0x21 / 0x20) slides down (`up` 0) or back up.
void R332RevaCommonMove(int no, int up)
{
    cObj* obj;
    int id;

    if (no == 1) {
        id = 0x20;
    } else {
        id = 0x21;
    }
    obj = SmdGetObjPtr(id);
    f32 acc = 6.475f;
    f32 spd = 0.0f;
    SndCall(6, 2, &obj->pos, 0, 0, 0);
    obj->be_flag |= 0x20;
    for (;;) {
        spd += acc;
        if (up == 1) {
            obj->pos.y += spd;
            if (obj->pos.y >= r332_craneUpY[no]) {
                obj->pos.y = r332_craneUpY[no];
                break;
            }
        } else {
            obj->pos.y -= spd;
            if (obj->pos.y <= r332_craneDownY[no]) {
                obj->pos.y = r332_craneDownY[no];
                break;
            }
        }
        SceSleep(1);
    }
}

// Crane `no`: the player takes the lever, aims the crane camera with the stick and drops the beam.
static void R332ExecCrane(int no)
{
    cParts* parts4;
    cParts* parts0;
    int flgNo;
    int hitDone;
    int endDone;
    int estNo;
    Vec plPos;
    Vec plRot;
    void* mot;
    int atNo;
    int rsfNo;
    f32 angMin;
    f32 angMax;
    cObj* crane;
    cEm31* em;
    int step;
    int loopOn;

    if (r332_craneRangeSet == 0) {
        r332_craneRange = SQRTF(80000000.0f);
        r332_craneRangeSet = 1;
    }
    hitDone = 0;
    endDone = 0;
    if (no == 0) {
        atNo = 1;
        flgNo = 1;
        mot = ROOM_ARC_PTR(pG->pRoom, 0x23);
        rsfNo = 6;
        estNo = 0x13;
        plPos.x = -51330.0f;
        plPos.y = 19311.3f;
        plPos.z = 51790.0f;
        plRot.x = 0.0f;
        plRot.y = 0.0f;
        plRot.z = 0.0f;
        angMin = -15.0f;
        angMax = 35.0f;
    } else {
        atNo = 2;
        flgNo = 2;
        mot = ROOM_ARC_PTR(pG->pRoom, 0x24);
        rsfNo = 7;
        estNo = 0x12;
        plPos.x = -36800.0f;
        plPos.y = 17811.0f;
        plPos.z = 88220.0f;
        plRot.x = 0.0f;
        plRot.y = 3.14f;
        plRot.z = 0.0f;
        angMin = 165.0f;
        angMax = -145.0f;
    }
    crane = r332_work->crane[no];
    if (crane == 0) {
        return;
    }
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em == 0) {
        return;
    }
    parts0 = crane->getPartsPtr(0);
    if (parts0 == 0) {
        return;
    }
    parts4 = crane->getPartsPtr(4);
    if (parts4 == 0) {
        return;
    }
    SceAtSetEnable(atNo, 0);
    pPL->beginEvent(0);
    pPL->setPos(plPos.x, plPos.y, plPos.z);
    pPL->setAng(plRot.x, plRot.y, plRot.z);
    loopOn = 1;
    step = 0;
    do {
        // Two cancel tests, each with its own copy of the exit: loop.c (find_and_verify_loops) moves a
        // block that ends in a jump out of the loop and is only jumped around to the last barrier
        // before the function end (after the hit arm); jump2 then cross-jumps the two copies into one.
        // An `||` form leaves the exit inline (its label stops the block scan).
        if ((PlGetStatus() & 0x00020000) == 0) {
            R332ExecCraneEnd(no, atNo);
            SceAtSetEnable(atNo, 1);
            return;
        }
        if (Key.trg & 0x40000000) {
            R332ExecCraneEnd(no, atNo);
            SceAtSetEnable(atNo, 1);
            return;
        }
        switch (step) {
        case 0:
            pPL->atari.off();
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x34), 3, 0, 0x200, 0);
            step = 1;
            break;
        case 1:
            if (MotionGetState(pPL)) {
                pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x35), 5, 0, 0x204, 0);
                r332_work->cam = pG->Camera;
                step++; // `li r8,2` before the block copy, `mr r28,r8` after it (cse folds step == 1 in the case arm)
            }
            break;
        case 2: {
            Vec dir;
            Mtx m;
            f32 ang;

            ActBtn.set(ACT_OPERATION, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
            CAMERA* cam = &r332_work->cam;
            f32 roll = 0.0f;
            dir.x = cam->param.at.x - cam->param.pos.x;
            dir.y = cam->param.at.y - cam->param.pos.y;
            dir.z = cam->param.at.z - cam->param.pos.z;
            ang = atan2f(dir.x, dir.z) * (180.0f / 3.14159265f);
            eprintf(0x14C, 0xC8, 0, 0, "%f", ang);
            if (no == 0) {
                if (ang < angMax && (Key.on & 8)) {
                    roll = 2.5f;
                }
                if (ang > angMin && (Key.on & 4)) {
                    roll -= 2.5f;
                }
            } else {
                if ((ang >= 0.0f || ang < angMax) && (Key.on & 8)) {
                    roll = 2.5f;
                }
                if ((ang <= 0.0f || ang > angMin) && (Key.on & 4)) {
                    roll -= 2.5f;
                }
            }
            PSMTXRotRad(m, 'y', roll * (3.14159265f / 180.0f));
            PSMTXMultVecSR(m, &dir, &dir);
            PSVECAdd(&dir, &cam->param.pos, &cam->param.at);
            cam->param.roll = 0.0f;
            CameraSetOrientationRoll(cam);
            CamCtrlSetCam(&CamCtrl, cam);
            if (Key.trg & 0x00080000) {
                pPL->dmg.set(0, 0x80);
                loopOn = 0;
                step++;
                pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x36), 5, 0, 0x200, 0);
                SceExec(0x12, (TaskFunc) R332RevaCommonMoveDw, no, 0, 2, 0);
            }
            break;
        }
        }
        SceSleep(1);
    } while (loopOn);
    while (MotionGetState(pPL) == 0) {
        SceSleep(1);
    }
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x37), 5, 0, 0x200, 0);
    {
        f32 reach = SQRTF(GetDistanceXZ(&parts0->world, &parts4->world));
        f32 d = SQRTF(GetDistance(&parts0->world, &em->pos));

        if (d < r332_craneRange + reach && em->hp > 0) {
            hitDone = 1;
            em->setCranePos((u8) no);
            CamCtrl.deleteAttachCamera(pPL->Motion.pAttachCam, pPL);
            MotionSetCore(crane, &crane->Motion, mot, 0, 3, 0x201, 0);
            while (MotionGetState(pPL) == 0) {
                SceSleep(1);
            }
            pPL->motionSet(pPL->m_MotTbl[0], pPL->m_MotTbl[1], pPL->m_MotTbl[0x5F], pPL->m_MotTbl[0x60], 0xC, 0);
        } else {
            MotionSetCore(crane, &crane->Motion, mot, 0, 3, 0x301, 0);
            while (MotionGetState(pPL) == 0) {
                SceSleep(1);
            }
            endDone = 1;
            R332ExecCraneEnd(no, atNo);
        }
    }
    SndCall(6, 3, &pPL->pos, 0, 0, 0);
    while (MotionGetState(crane) == 0) {
        if (FlagChkVar(R332_FLAGS, (u32) flgNo) == 0 && hitDone == 1) {
            cEmHit* hit = r332_work->hit[no];

            if (hit && em) {
                f32 d = SQRTF(GetDistance(&hit->pList->world, &em->pos));

                if (d < r332_craneRange) {
                    int k;

                    em->setHitCrane(&hit->pList->world);
                    FlagOnVar(R332_FLAGS, (u32) flgNo);
                    RsfSet(G_ROOM_ID, rsfNo);
                    for (k = 0; k < 5; k++) {
                        r332_work->chain[no][k]->be_flag &= ~2;
                    }
                    ModelInfoSetTrans(crane, 1, 0);
                    EstSet(crane, -1, 0, 0, EFF_ROOM, 0xE, 1, ESP_CORE_KIND_NONE, 0, 0);
                    EstSet(0, -1, 0, 0, EFF_ROOM, (u8) estNo, 1, ESP_CORE_KIND_NONE, 0, 0);
                    SndCall(6, 9, &pPL->pos, 0, 0, 0);
                }
            }
        }
        SceSleep(1);
    }
    if (endDone == 0) {
        CamCtrl.deleteAttachCamera(crane->Motion.pAttachCam, crane);
        R332ExecCraneEnd(no, atNo);
    }
    if (RsfCheck(G_ROOM_ID, rsfNo) == 0) {
        SceExec(0x12, (TaskFunc) R332RevaCommonMoveUp, no, 0, 2, 0);
    }
    FlagOffVar(R332_FLAGS, (u32) flgNo);
}

// End of a crane use: the player's damage state cleared, out of event mode, walk state 0xC, collision back.
void R332ExecCraneEnd(int no, int atNo)
{
    cPlayer* pl = pPL;

    pl->dmg.clear();
    pl->endEvent(0);
    pl->m_Hokan = 0xC;
    pPL->atari.on();
}

// The s00 event (the boss appears).
static void R332EventS00()
{
    cEm31* em;

    if (RsfCheck(G_ROOM_ID, 0)) {
        return;
    }
    RsfSet(G_ROOM_ID, 0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    SceSleep(1);
    EvtMgr.EvtReadExec("event/evd/r332s00.evd", (u8) GetEmIdFromList(0xA9), EvtReadFlagNone);
    GamePointBossReset();
    r332_work->em[0].setEm(0xA8, -1, 1, 1, 1);
    r332_work->em[1].setEm(0xA9, -1, 1, 1, 1);
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em == 0) {
        return;
    }
    r332_work->em[1].setFlag(1);
    r332_work->em[0].setFlag(1);
    r332_work->em[1].setNoSuspend(1);
    r332_work->em[0].setNoSuspend(1);
    pPL->beginEvent(0);
    pPL->setNoSuspend(1);
    pPL->setPos(-29161.0f, 15811.0f, 61102.0f);
    pPL->setAng(0.0f, 3.14f, 0.0f);
    if (pG->Room_flg[0] & 0x04000000) {
        SceSetEventCancel(0, 0, 0, -1, 1);
        R332EventS00End();
    } else {
        SceSetEventCancel(1, (TaskFunc) R332EventS00Cancel, 0, -1, 1);
        while (MotionGetState(em) == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R332EventS00End();
    }
}

// Cancel path of the s00 event: BGM table 0x332 set 0 and the fight stream started, then the common end.
static void R332EventS00Cancel()
{
    SndBgmTblSet(0x332, 0);
    SndRoomStrStart(1, 0, 1);
    R332EventS00End();
}

// End of the s00 event: both boss handles may suspend and leave their appearance motion, the boss
// registered for the life meter (PlRegistBoss), the bridges reset closed, the fight begins.
void R332EventS00End()
{
    cEm31* em;

    r332_work->em[1].setNoSuspend(0);
    r332_work->em[0].setNoSuspend(0);
    em = (cEm31*) r332_work->em[1].getPtr();
    if (em) {
        em->setAppearCancel();
        PlRegistBoss(em, (void*) 4);
    }
    em = (cEm31*) r332_work->em[0].getPtr();
    if (em) {
        em->setAppearCancel();
    }
    R332BridgeInit(0, 0);
    R332BridgeInit(1, 0);
    r332_work->task[0] = SceExec(0x12, (TaskFunc) R332BridgeTask, 0, 0, 2, 0);
    r332_work->task[1] = SceExec(0x12, (TaskFunc) R332BridgeTask, 1, 0, 2, 0);
    pPL->setNoSuspend(0);
    pPL->setPos(-32900.0f, 15811.0f, 47140.0f);
    pPL->setAng(0.0f, 0.766f, 0.0f);
    pPL->endEvent(0);
    EvtMgr.EvtReadAram("event/evd/r332s20.evd", (u8) GetEmIdFromList(0xA9), 0, 0, 0);
    em = (cEm31*) r332_work->em[0].getPtr();
    if (em) {
        Cckpt.m_LifeMeter.flags = (u32) em;
    }
    SndRoomBgmStart(0, 0);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 9, 1, ESP_CORE_KIND_ROOM02, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 1, ESP_CORE_KIND_ROOM02, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xB, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 1, ESP_CORE_KIND_ROOM03, 0, 0);
    SceUpCut(2, 6, 8, 0);
}

// The s10 event (after the rocket): the count-down to the island's destruction starts.
static void R332EventS10()
{
    if (RsfCheck(G_ROOM_ID, 1)) {
        return;
    }
    RsfSet(G_ROOM_ID, 1);
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM04, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM04, 0);
    if (r332_work->task[0]) {
        SceKill(r332_work->task[0]);
    }
    if (r332_work->task[1]) {
        SceKill(r332_work->task[1]);
    }
    r332_work->task[0] = 0;
    r332_work->task[1] = 0;
    R332BridgeOpened(0, 1);
    R332BridgeOpened(1, 1);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    if ((pG->Room_flg[0] & 0x02000000) == 0) {
        EvtMgr.EvtReadExec("event/evd/r332s10.evd", (u8) GetEmIdFromList(0xA9), EvtReadFlagNone);
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    st3_setCountDownTimer(0x127D);
    st3_startCountDown();
    ScfFlagOn(pG, SCF_R332_BOSS_DIE);
    SceAtSetEnable(0, 1);
    SceAtSetEnable(9, 0);
    KyfFlagOn(pG, KYF_ST1_24);
    SceAtSetEnable(1, 0);
    SceAtSetEnable(2, 0);
    SceSleep(1);
    SceAtExecute(0x85);
    ScfFlagOn(pG, SCF_R332_KEY_GET);
    while (SceAtItemFlgCk(0x85) == 0) {
        SceSleep(1);
    }
    SceUpCut(3, 6, 8, 0);
    SndBgmTblSet(0x332, 1);
    SndRoomStrStart(1, 0, 1);
    SndRoomBgmStart(1, 0);
    SndBgmTblSet(0x331, 2);
    GameSave.save(pSaveData, -1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0xF, 1, ESP_CORE_KIND_NONE, 0, 0);
}

// The s20 event (the special rocket is thrown to the player).
static void R332EventS20()
{
    RsfSet(G_ROOM_ID, 2);
    EvtMgr.EvtReadExec("event/evd/r332s20.evd", (u8) GetEmIdFromList(0xA9), EvtReadFlagPlCheckEvent);
    SceAtSetEnable(0x84, 1);
}

// Darken every model info of the dead boss's event model.
void R332Em32RocketDie(cObj* obj)
{
    cModelInfo* info;

    if (obj == 0) {
        return;
    }
    for (info = obj->pModelInfo; info != 0; info = info->pList) {
        info->color[0] = 0x30;
        info->color[1] = 0x30;
        info->color[2] = 0x30;
    }
}

// Show / hide the scroll objects of the arena (the events swap them for their own models).
void R332ScrTrans(int on)
{
    int i;
    int j;

    SmdSetTrans(0, on);
    SmdSetTrans(1, on);
    SmdSetTrans(4, on);
    SmdSetTrans(5, on);
    SmdSetTrans(6, on);
    SmdSetTrans(0x19, on);
    SmdSetTrans(0x1A, on);
    SmdSetTrans(0x2F, on);
    SmdSetTrans(0x30, on);
    SmdSetTrans(0x32, on);
    SmdSetTrans(0x33, on);
    SmdSetTrans(0x35, on);
    SmdSetTrans(0x46, on);
    SmdSetTrans(0x5D, on);
    SmdSetTrans(0x61, on);
    SmdSetTrans(0x62, on);
    SmdSetTrans(0x73, on);
    for (i = 8; i <= 0x17; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x1E; i <= 0x27; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x29; i <= 0x2C; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x38; i <= 0x41; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x48; i <= 0x5A; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x79; i <= 0x7B; i++) {
        SmdSetTrans(i, on);
    }
    for (j = 0; j < 2; j++) {
        cObj* crane = r332_work->crane[j];

        if (crane) {
            if (on == 1) {
                crane->be_flag |= 2;
            } else {
                crane->be_flag &= ~2;
            }
        }
    }
}

// Ashley's skirt / ribbon model (pl0200): hide (`on` 0) or show its cloth child (uses the caller's `mod`).
#define R332_PL_CHILD_TRANS(e, on)                                    \
    if ((e)->NowFrame == 0) {                                            \
        if ((e)->GetMod(&mod, "pl0200", 0, 0) == 1) {                 \
            Obj18Work* w = OBJ18_WK((cObj18*) mod);                       \
                                                                      \
            if (w && w->pObjChain) {                                      \
                if ((on) == 0) {                                      \
                    OBJ18_WK((cObj18*) mod)->ObjChainFlagCommon |= 0x04000000;\
                    w->pObjChain->be_flag &= ~2;                          \
                } else {                                              \
                    OBJ18_WK((cObj18*) mod)->ObjChainFlagCommon &= ~0x04000000;\
                    w->pObjChain->be_flag |= 2;                           \
                }                                                     \
            }                                                         \
        }                                                             \
    }

// Event r332s00 callback (Saddler appears): scroll object 0xA hidden; fades and the evmc200 / pl8200 /
// evmd100 / Ashley (pl0200) models' flags per cut; the end shows 0xA again.
void Evt_R332S00_Func(Event* e)
{
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    cObj* obj;

    switch (e->FuncType) {
    case 0:
        SmdSetTrans(0xA, 0);
        break;
    case 1: {
        void* mod;

        if (e->NowCut == 0xB) {
            if (e->NowFrame == 0) {
                SetShadowCamMoveSize(0.0f);
            }
        } else if (e->NowFrame == 0) {
            ResetShadowCamMoveSize();
        }
        if (e->NowCut == 0 && e->NowFrame == 0) {
            int skip = e->FlgCkStatus(EvtStfToolFrontExec);

            if (skip == 0) {
                FadeSetW(0x80000002, 40, 0, 0);
            }
            obj = SmdGetObjPtr(0x24);
            if (obj) {
                e->SetMod("scr0000", obj, 5, 0, 2, 0);
                obj->setPos(&pos);
                obj->setAng(&rot);
                obj->be_flag |= 0x20;
                e->EspSetModelPtr(obj);
            }
            if (e->GetMod(&mod, "evmc200", 0, 0) == 1) {
                ((cModel*) mod)->LightInfo.EnableMask = 0x10;
                ((cModel*) mod)->be_flag |= 0x80;
            }
        }
        switch (e->NowCut) {
        case 2:
        case 0xC:
        case 0x11:
            R332_PL_CHILD_TRANS(e, 0)
            break;
        default:
            R332_PL_CHILD_TRANS(e, 1)
            break;
        }
        if (pG->game_costume == 1 && e->NowFrame == 0) {
            if (e->GetMod(&mod, "pl8200", 0, 0) == 1) {
                Obj18CmfOn((cObj*) mod, 5);
                ((cModel*) mod)->be_flag &= ~2;
            }
            if (e->GetMod(&mod, "evmd100", 0, 0) == 1) {
                Obj18CmfOn((cObj*) mod, 5);
                ((cModel*) mod)->be_flag &= ~2;
            }
        }
        switch (e->NowCut) {
        case 0xB:
        case 0xC:
        case 0xD:
        case 0x15:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod)->be_flag |= 0x40;
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod)->be_flag &= ~0x40;
                }
            }
            break;
        }
        break;
    }
    case 2: {
        SmdWork* w = SmdGetWorkPtr(0x24);

        if ((obj = SmdGetObjPtr(0x24)) != 0 && w != 0) {
            obj->setPos(&w->pos);
            obj->setAng(&w->rot);
        }
        SmdSetTrans(0xA, 1);
        break;
    }
    case 3:
        SndBgmTblSet(0x332, 0);
        SndRoomStrStart(1, 0, 1);
        pG->Room_flg[0] |= 0x04000000;
        break;
    }
}

// Event r332s10 callback (after the kill): the pillar state, the obm3d00 / evma500 / evmb500 / evm9500
// models per cut, the dead boss models darkened (R332Em32RocketDie), the arena scroll objects swapped;
// cut 0xD starts the escape count-down (0x1518 frames); the end sets it to 0x127D and restarts it.
void Evt_R332S10_Func(Event* e)
{
    switch (e->FuncType) {
    case 0:
        setRoomEtcDisp(1, 0, 1);
        if (r332_work->pillar[2]->isTrans() == 1) {
            pG->Room_flg[0] |= 0x00200000;
            r332_work->pillar[2]->be_flag &= ~2;
        }
        break;
    case 1:
        switch (e->NowCut) {
        case 0x11:
            if (e->NowFrame == 0) {
                void* mod;
                cLight* light;

                if (e->GetMod(&mod, "obm3d00", 0, 0) == 1) {
                    light = LightMgr.getKindLight(1);
                    if (light) {
                        light->setParent((cModel*) mod);
                    }
                }
                if (e->GetMod(&mod, "obm3d00", 0, 0) == 1) {
                    light = LightMgr.getKindLight(2);
                    if (light) {
                        light->setParent((cModel*) mod);
                    }
                }
            }
            break;
        case 0x12:
        case 0x13:
            if (e->NowFrame == 0) {
                void* mod;
                cLight* light;

                if (e->GetMod(&mod, "evma500", 0, 0) == 1) {
                    light = LightMgr.getKindLight(1);
                    if (light) {
                        light->setPartsNo(1);
                        light->setParent((cModel*) mod);
                    }
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                R332ScrTrans(0);
            }
            break;
        case 1:
            if (e->NowFrame == 0) {
                R332ScrTrans(1);
            }
            break;
        }
        {
            void* mod;

            switch (e->NowCut) {
            case 0:
                if (e->NowFrame == 0) {
                    if (e->GetMod(&mod, "evmb500", 0, 0) == 1) {
                        ((cModel*) mod)->LightInfo.EnableMask = 0x20;
                    }
                    if (e->GetMod(&mod, "evm9500", 0, 0) == 1) {
                        ((cModel*) mod)->LightInfo.EnableMask = 0x20;
                        ((cModel*) mod)->be_flag |= 0x10;
                    }
                    if (e->GetMod(&mod, "obm3d00", 0, 0) == 1) {
                        ((cModel*) mod)->be_flag |= 0x80;
                    }
                    if (e->GetMod(&mod, "em3100", 0, 0) == 1) {
                        ((cModel*) mod)->be_flag |= 0x10;
                        if (RsfCheck(G_ROOM_ID, 3)) {
                            R332Em32RocketDie((cObj*) mod);
                        }
                    }
                    if (e->GetMod(&mod, "em3100a", 0, 0) == 1) {
                        ((cModel*) mod)->be_flag |= 0x10;
                        if (RsfCheck(G_ROOM_ID, 3)) {
                            R332Em32RocketDie((cObj*) mod);
                            EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 0x1001, ESP_CORE_KIND_NONE, 0, 0);
                        } else {
                            EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 0x1001, ESP_CORE_KIND_NONE, 0, 0);
                        }
                    }
                }
                break;
            case 0xD:
                if (e->NowFrame == 0x18 && DbgFlagChk(pG, DBG_EVENT_TOOL) == 0) {
                    st3_setCountDownTimer(0x1518);
                    st3_startCountDown();
                }
                break;
            }
            switch (e->NowCut) {
            case 9:
            case 0xA:
            case 0xC:
            case 0xD:
                R332_PL_CHILD_TRANS(e, 0)
                break;
            default:
                R332_PL_CHILD_TRANS(e, 1)
                break;
            }
        }
        break;
    case 2:
        if (pG->Room_flg[0] & 0x00200000) {
            r332_work->pillar[2]->be_flag |= 2;
        }
        setRoomEtcDisp(1, 1, 1);
        R332ScrTrans(1);
        st3_setCountDownTimer(0x127D);
        st3_startCountDown();
        break;
    case 3:
        pG->Room_flg[0] |= 0x02000000;
        break;
    }
}

// Event r332s20 callback (the special rocket is thrown down): per-cut model flags for the rocket case.
void Evt_R332S20_Func(Event* e)
{
    void* mod;

    if (e->FuncType != 1) {
        return;
    }
    if (e->NowCut == 0 && e->NowFrame == 0) {
        if (e->GetMod(&mod, "evm8900", 0, 0) == 1) {
            ((cModel*) mod)->pModelInfo->color[0] = 0xA0;
            ((cModel*) mod)->pModelInfo->color[1] = 0xD0;
            ((cModel*) mod)->pModelInfo->color[2] = 0xE0;
            ((cModel*) mod)->pModelInfo->color[3] = 0xFF;
            ((cModel*) mod)->be_flag |= 0x10;
        }
        if (e->GetMod(&mod, "evm9200", 0, 0) == 1) {
            ((cModel*) mod)->pModelInfo->color[0] = 0xFF;
            ((cModel*) mod)->pModelInfo->color[1] = 0x78;
            ((cModel*) mod)->pModelInfo->color[2] = 0x80;
            ((cModel*) mod)->pModelInfo->color[3] = 0xFF;
            ((cModel*) mod)->be_flag |= 0x10;
        }
    }
    if (e->NowCut == 1) {
        R332_PL_CHILD_TRANS(e, 0)
    } else {
        R332_PL_CHILD_TRANS(e, 1)
    }
    if (pG->game_costume == 1 && e->NowFrame == 0) {
        if (e->GetMod(&mod, "pl8200", 0, 0) == 1) {
            Obj18CmfOn((cObj*) mod, 5);
            ((cModel*) mod)->be_flag &= ~2;
        }
    }
}

// The render target blended over the arena's water object.
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r332_texTbl;

    if (GetTexRenderMgr(&r332_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r332_work->tex->m_Tex_no;
        r332_work->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r332_work->tex->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    r332_work->tex->m_H_size = r332_work->tex->m_W_size = 0x40;
    obj = SmdGetObjPtr(0x77);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->setBlendType(1);
    obj->pModelInfo->color[3] = 0xF0;
}
