#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emswitch.h"
#include "emBarred.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "motion.h"
#include "room_data.h"
#include "st_mgr_event.h"
#include "rnd.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "math_sub.h"
#include <string.h>

// Room 3-0D (D:/Bio4/Prog/r30d.cpp): the two-lever gate Leon and Ashley pull together (the coop
// switch with its countdown display), the power shutter, the front shutter Ashley crawls under and
// the two treasure chests.


// The coop switch state machine (r30d_work->coop, cleared with memset at every attempt).
struct R30dCoop {
    int mode;      // 0x00  0 pull, 1 success, 2 failure
    int step;      // 0x04
    int active;    // 0x08  the event loop runs while set
    int num;       // 0x0C  countdown digit being shown (4..0)
    int timer;     // 0x10  frames left for the digit
};

struct R30dWork {
    int cnt[2];       // 0x00  frames since a lever was seen open
    R30dCoop coop;    // 0x08
    cObj* obj[2];     // 0x1C  the two lever models
    cEmWrap em[2];    // 0x24
    ScePrim* timer;   // 0x3C  R30dTimerDisp task
};


static R30dWork* r30d_work;

// The coop state words are stored at raw offsets from the block pointer: scalar stores that keep the
// block pointer as their base (a struct member store would be folded onto the work pointer) and
// keep the following pG / work loads below them.
#define COOP_WORD(c, ofs) (*(int*) ((u32) (c) + (ofs)))
#define COOP_MODE(c) COOP_WORD(c, 0x0)
#define COOP_STEP(c) COOP_WORD(c, 0x4)
#define COOP_ACTIVE(c) COOP_WORD(c, 0x8)
#define COOP_NUM(c) COOP_WORD(c, 0xC)
#define COOP_TIMER(c) COOP_WORD(c, 0x10)

// The shutter bits live in the second word of the room's save record (RoomData); the word is
// addressed as a scalar (r209), so the pG load that follows a set stays below the store.
#define R30D_SAVE_FLAGS (*(u32*) (RoomData.getRoomSavePtr(pG->room_id) + 4))
// The door lock words are addressed by offset the same way (cast-then-deref, the rooms' flag word
// idiom): scalar accesses that keep the following pSUB / pG loads below the stores.
#define DOOR_UNLOCK(i) (*(u32*) ((u32) &pG->Key_flg[0] + (i) * 4))



// Area flag test through a helper: fold would merge two tests of the same word in one `&&`/`||` into a
// single masked compare; the original keeps one `andis.` per bit.
static inline int sceAtFlag(u32 bit) { return pG->Room_flg[2] & bit; }

int r30d_digit[4] = {4, 3, 2, 1};

static void OpenBoxTreasure(int id);
static void OpenedBoxTreasure(int id);
static void R30dShutterPowerMain();
static void R30dShutterPowerEnd();
static void R30dShutterFrontEvent();
static void R30dDoorCheck();
static void R30dCoopSwitch();
static void R30dTimerDisp();
static void funcAshleySwitch(cEm* p);
static void funcAshleyShutter(cEm* p);
static void SceBgmCheck();

// Room init: Scenario_flg[1] 0x400; the two coop switches (etc 8/9) linked to each other and to the
// barred gate 0xC; the power shutter (area 0x14) and the front shutter Ashley crawls under (area 0x18)
// per the save record's bits 0x10000000 / 0x08000000; the coop gate (areas 0x10/0x11 with area 1 = the
// door check) until Key_flg[0] 0x400; three treasure item events; two Ganados (0x5E/0x5C) per flags;
// the battle stream.
void R30dInit()
{
    Vec v;
    cEmSwitch* sw0;
    cEmSwitch* sw1;
    cEmBarred* bar;
    cEm* b1;
    cEm* b2;
    int i;
    int n;

    R30dWork*& wp = r30d_work;   // reference: the following `lwz pG` stays below the store (r227 idiom)
#line 57 "D:/Bio4/Prog/r30d.cpp"
    wp = (R30dWork*) MEM_CALLOC(sizeof(R30dWork), 1, 0xd);
    ScfFlagOn(pG, SCF_R30D_ENTER);
    getRoomEtcSwitch(8, (cEm**) &sw0, 1);
    getRoomEtcSwitch(9, (cEm**) &sw1, 1);
    getRoomEtcBarred(0xC, (cEm**) &bar, 1);
    if (sw0 && sw1 && bar) {
        sw0->setBarred(bar);
        sw0->setConnectSwitch(sw1);
        sw1->setBarred(bar);
        sw1->setConnectSwitch(sw0);
        sw0->setClosed();
        sw1->setClosed();
        bar->setClosed();
        bar->setUnderCk();
    }
    getRoomEtcSwitch(0xA, (cEm**) &sw0, 1);
    getRoomEtcSwitch(0xB, (cEm**) &sw1, 1);
    getRoomEtcBarred(0xD, (cEm**) &bar, 1);
    if (sw0 && sw1 && bar) {
        sw0->setActButton(0);
        sw1->setActButton(0);
    }
    for (i = 0; i < 2; i++) {
        r30d_work->cnt[i] = 0;
    }
    if (!(R30D_SAVE_FLAGS & 0x10000000)) {
        SceAtDataSet_exec(0x14, 0x12, 0, (TaskFunc) R30dShutterPowerMain, 0, 1);
    } else {
        getRoomEtcBarred(0xD, &b1, 1);
        if (b1) {
            f32 x = b1->pos.x;
            f32 z = b1->pos.z;
            v.x = x;
            v.y = 800.0f;
            v.z = z;
            b1->setPos(&v);
        }
    }
    if (!(R30D_SAVE_FLAGS & 0x08000000)) {
        SceAtDataSet_exec(0x18, 0x12, 0, (TaskFunc) R30dShutterFrontEvent, 0, 1);
    } else {
        getRoomEtcBarred(0xD, &b2, 1);
        if (b2) {
            f32 x = b2->pos.x;
            f32 z = b2->pos.z;
            v.x = x;
            v.y = 3300.0f;
            v.z = z;
            b2->setPos(&v);
        }
    }
    if (!KyfFlagChk(pG, KYF_R30D_TO_R30F_DOOR)) {
        SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) R30dDoorCheck, 0, 1);
        SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) R30dCoopSwitch, 0, 1);
        SceAtDataSet_exec(0x11, 0x12, 0, (TaskFunc) R30dCoopSwitch, 0, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_ROOM01, 0, 0);
    } else {
        for (n = 0; n < 4; n++) {
            EstSet(0, -1, 0, 0, EFF_ROOM, r30d_digit[n], 1, ESP_CORE_KIND_ROOM00, 0, 0);
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_ROOM01, 0, 0);
    }
    {
        Vec pos[2] = {
            {10587.31f, 1500.0f, 26627.83f},
            {10587.31f, 1500.0f, 22807.6f},
        };
        Vec ang[2] = {
            {0.0f, -PI / 2, 0.0f},
            {0.0f, -PI / 2, 0.0f},
        };
        int k;

        for (k = 0; k < 2; k++) {
            r30d_work->obj[k] = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), &pos[k], &ang[k], 0x10, 1);
            if (r30d_work->obj[k]) {
                MotionSetCore(r30d_work->obj[k], &r30d_work->obj[k]->Motion, ROOM_ARC_PTR(pG->pRoom, 0x29), 0, 0, 1, 0);
            }
        }
    }
    SceSetItemEvent(4, 0x80, 0, 3, OpenBoxTreasure, OpenedBoxTreasure, 0x80, 0);
    SceSetItemEvent(5, 0x81, 1, 4, OpenBoxTreasure, OpenedBoxTreasure, 0x81, 0);
    SceSetItemEvent(0x1B, 0x87, 5, 9, OpenBoxTreasure, OpenedBoxTreasure, 0x87, 0);
    if (pG->room_id_prev == 0x30F) {
        r30d_work->em[0].setEm(0x5E, -1, 0, 1, 1);
        v.y = r30d_work->em[0].getAngY() + PI;
        v.x = 0.0f;
        v.z = 0.0f;
        r30d_work->em[0].setAng(&v);
        r30d_work->em[1].setEm(0x5C, -1, 0, 1, 1);
        v.y = r30d_work->em[1].getAngY() + PI;
        v.x = 0.0f;
        v.z = 0.0f;
        r30d_work->em[1].setAng(&v);
    }
    SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, 2, 0);
}

// The lever pair (8 / 0xA): a lever seen open arms a 450-frame window; while it is closed and the
// window has run out, the switch reopens when the area flags say so.
void R30dMain()
{
    cEmSwitch* sw[2];
    int i;
    int n;

    for (i = 0; i < 2; i++) {
        r30d_work->cnt[i]--;
        if (r30d_work->cnt[i] < 0) {
            r30d_work->cnt[i] = 0;
        }
    }
    getRoomEtcSwitch(8, (cEm**) &sw[0], 1);
    getRoomEtcSwitch(0xA, (cEm**) &sw[1], 1);
    for (n = 0; n < 1; n++) {
        if (sw[n]) {
            if (sw[n]->ckOpen() == 0) {
                if (r30d_work->cnt[n] <= 0 && n == 0) {
                    if ((!(pG->Room_flg[2] & 0x80000000) && sceAtFlag(0x10000000)) ||
                        (!sceAtFlag(0x40000000) && sceAtFlag(0x08000000))) {
                        sw[n]->setOpen();
                    }
                }
            } else {
                r30d_work->cnt[n] = 0x1C2;
            }
        }
    }
}

// Item-event "already opened": pose the chest of item `id` (0x80 double lid, 0x81 / 0x87 single) open.
static void OpenedBoxTreasure(int id)
{
    if (id == 0x80) {
        OpenBoxMain(0, 1, 0x1C, 0x1A, 0x1B, -1);
    }
    if (id == 0x81) {
        OpenBoxMain(1, 1, 0, 0x1C, -1, -1);
    }
    if (id == 0x87) {
        OpenBoxMain(1, 1, 0x1C, 0x3E, -1, -1);
    }
}

// Item-event opener: animate the chest of item `id` open.
static void OpenBoxTreasure(int id)
{
    if (id == 0x80) {
        OpenBoxMain(0, 0, 0x1C, 0x1A, 0x1B, -1);
    }
    if (id == 0x81) {
        OpenBoxMain(1, 0, 0, 0x1C, -1, -1);
    }
    if (id == 0x87) {
        OpenBoxMain(1, 0, 0x1C, 0x3E, -1, -1);
    }
}

// The power shutter (barred 0xD) rises from 0 to 800 over 30 frames, shakes, then settles.
static void R30dShutterPowerMain()
{
    Vec p;
    cEm* bar;
    int i;
    int j;

    if (R30D_SAVE_FLAGS & 0x10000000) {
        return;
    }
    R30D_SAVE_FLAGS |= 0x10000000;
    SceAtSetEnable(0x14, 0);
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) R30dShutterPowerEnd, 0, -1, 1);
    SceMesCamSndSet(5, -1, 1, 0);
    CamCtrl.CutCall(8);
    getRoomEtcBarred(0xD, &bar, 1);
    if (bar) {
        SndCall(6, 5, &bar->pos, 0, 0, 0);
        for (i = 0; i < 30; i++) {
            f32 x = bar->pos.x;
            f32 z = bar->pos.z;
            p.x = x;
            p.y = (f32) i * (800.0f - 0.0f) / 30.0f + 0.0f;
            p.z = z;
            bar->setPos(&p);
            SceSleep(1);
        }
        {
            f32 x = bar->pos.x;
            f32 z = bar->pos.z;
            p.x = x;
            p.y = 800.0f;
            p.z = z;
            bar->setPos(&p);
        }
        for (j = 0; j < 10; j++) {
            cEm* b = bar;
            f32 y = fRand1_1() * 20.0f + 800.0f;
            f32 x = b->pos.x;
            f32 z = bar->pos.z;
            p.x = x;
            p.y = y;
            p.z = z;
            bar->setPos(&p);
            SceSleep(1);
        }
        {
            f32 x = bar->pos.x;
            f32 z = bar->pos.z;
            p.x = x;
            p.y = 800.0f;
            p.z = z;
            bar->setPos(&p);
        }
    }
    SceMesCamSndSet(6, -1, -1, 0);
    SceSetEventCancel(0, 0, 0, -1, 1);
    R30dShutterPowerEnd();
}

// End of the power shutter rise (also its cancel path): the shutter (barred 0xD) snapped to y 800, camera back, SceEventEnd, task exit.
static void R30dShutterPowerEnd()
{
    Vec p;
    cEm* bar;
    f32 x;
    f32 z;

    getRoomEtcBarred(0xD, &bar, 1);
    if (bar) {
        f32 x = bar->pos.x;
        f32 z = bar->pos.z;
        p.x = x;
        p.y = 800.0f;
        p.z = z;
        bar->setPos(&p);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// The front shutter: Ashley crawls under it and raises it from 800 to 3300.
static void R30dShutterFrontEvent()
{
    Vec p;
    cEm* bar;
    int i;

    if (!(R30D_SAVE_FLAGS & 0x10000000)) {
        SceUpCut(9, -1, -1, 0);
        return;
    }
    if (!(pG->Room_flg[0] & 0x20000000)) {
        SceAtWork* at;

        SceUpCut(0xA, -1, -1, 0);
        pG->Room_flg[0] |= 0x20000000;
        at = SceAtPtr(0x18);
        if (at) {
            at->actBtnKind = 0x33;
        }
        return;
    }
    if (!(pG->Room_flg[2] & 0x00100000)) {
        SceUpCut(0xB, -1, -1, 0);
        return;
    }
    SceAtSetEnable(0x18, 0);
    R30D_SAVE_FLAGS |= 0x08000000;
    KyfFlagOn(pG, KYF_ST1_09);
    pSUB->dmg.m_Timer = 0x80;
    SubCharMoveTo(2670.0f, 0.0f, 15200.0f, 0.0f, 0);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    SetSubAux(funcAshleyShutter, 0);
    while ((pG->Room_flg[0] & 0x40000000) == 0) {
        SceSleep(1);
    }
    SubCharMoveTo(3944.0f, 0.0f, 17837.0f, 193.0f, 0);
    while ((SubCharGetStatus() & 0x00800000) == 0) {
        SceSleep(1);
    }
    EmRoutineSet(pSUB, 0, 0, 0, 0);
    SubCharCtrl(0, 0);
    for (i = 0; i < 40; i++) {
        SceSleep(1);
    }
    SndCall(6, 0x23, &pSUB->pos, 0, 0, 0);
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
    getRoomEtcBarred(0xD, &bar, 1);
    if (bar) {
        SndCall(6, 0x24, &bar->pos, 0, 0, 0);
        for (i = 0; i < 30; i++) {
            f32 x = bar->pos.x;
            f32 z = bar->pos.z;
            p.x = x;
            p.y = (f32) i * (3300.0f - 800.0f) / 30.0f + 800.0f;
            p.z = z;
            bar->setPos(&p);
            SceSleep(1);
        }
        f32 x = bar->pos.x;
        f32 z = bar->pos.z;
        p.x = x;
        p.y = 3300.0f;
        p.z = z;
        bar->setPos(&p);
        SndCall(6, 0x25, &bar->pos, 0, 0, 0);
    }
    pSUB->dmg.clear();
    EmRoutineSet(pSUB, 0, 0, 0, 0);
    SubCharCtrl(1, 0);
}

// Area 1, the coop gate: up-cut 0/2 while locked (Key_flg[0] 0x400 clear), else run the door area.
static void R30dDoorCheck()
{
    if (!KyfFlagChk(pG, KYF_R30D_TO_R30F_DOOR)) {
        SceUpCut(0, -1, 2, 0);
    } else {
        SceAtExecute(1);
    }
}

// Both levers pulled together: Leon and Ashley are placed at their lever, pull, and the countdown
// decides whether the gate opens (mode 1) or the levers snap back (mode 2).
static void R30dCoopSwitch()
{
    R30dCoop* c = &r30d_work->coop;
    Vec v;

    memset(c, 0, sizeof(R30dCoop));
    if (pSUB == NULL) {
        return;
    }
    if (!(sceAtFlag(0x01000000) && sceAtFlag(0x00200000)) && !(sceAtFlag(0x00800000) && sceAtFlag(0x00400000))) {
        SceUpCut(1, -1, -1, 0);
        return;
    }
    CamCtrl.CutCall(7);
    pPL->beginEvent(0);
    SetSubAux(funcAshleySwitch, 0);
    if (sceAtFlag(0x01000000)) {
        cObj* o1 = r30d_work->obj[1];
        if (o1) {
            f32 x = o1->pos.x - 828.79f;
            f32 z = o1->pos.z - 55.0f;
            v.x = x;
            v.z = z;
            v.y = 0.0f;
            pPL->setPos(&v);
            v.x = 0.0f;
            v.y = PI / 2;
            v.z = 0.0f;
            pPL->setAng(&v);
        }
        cObj* o0 = r30d_work->obj[0];
        if (o0) {
            cSubChar* sub = SUB_CHAR();
            f32 x = o0->pos.x - 619.92f;
            f32 z = o0->pos.z + 12.8f;
            v.x = x;
            v.z = z;
            v.y = 0.0f;
            sub->setPos(&v);
            v.x = 0.0f;
            v.y = PI / 2;
            v.z = 0.0f;
            pSUB->setAng(&v);
        }
    } else {
        cObj* o0 = r30d_work->obj[0];
        if (o0) {
            f32 x = o0->pos.x - 828.79f;
            f32 z = o0->pos.z - 55.0f;
            v.x = x;
            v.z = z;
            v.y = 0.0f;
            pPL->setPos(&v);
            v.x = 0.0f;
            v.y = PI / 2;
            v.z = 0.0f;
            pPL->setAng(&v);
        }
        cObj* o1 = r30d_work->obj[1];
        if (o1) {
            cSubChar* sub = SUB_CHAR();
            f32 x = o1->pos.x - 619.92f;
            f32 z = o1->pos.z + 12.8f;
            v.x = x;
            v.z = z;
            v.y = 0.0f;
            sub->setPos(&v);
            v.x = 0.0f;
            v.y = PI / 2;
            v.z = 0.0f;
            pSUB->setAng(&v);
        }
    }
    COOP_MODE(c) = 0;
    COOP_STEP(c) = 0;
    r30d_work->timer = 0;
    SceUpCut(2, 6, -1, 0);
    CamCtrl.CutCall(7);
    COOP_ACTIVE(c) = 1;
    do {
        if ((PlGetStatus() & 0x00020000) == 0) {
            if (r30d_work->timer) {
                SceKill(r30d_work->timer);
            }
            r30d_work->timer = 0;
            COOP_ACTIVE(c) = 0;
            EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
            break;
        } else if ((SubCharGetStatus() & 0x01000000) == 0) {
            if (r30d_work->timer) {
                SceKill(r30d_work->timer);
            }
            r30d_work->timer = 0;
            COOP_ACTIVE(c) = 0;
            EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
            break;
        } else if (Key.trg & 0x40000000) {
            if (r30d_work->timer) {
                SceKill(r30d_work->timer);
            }
            r30d_work->timer = 0;
            COOP_ACTIVE(c) = 0;
            EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
            break;
        } else {
            switch (COOP_MODE(c)) {
            case 0:
                switch (COOP_STEP(c)) {
                case 0:
                    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 3, 0, 1, 0);
                    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 3, 0, 1, 0);
                    for (int k = 0; k < 2; k++) {
                        cObj* o = r30d_work->obj[k];
                        if (o) {
                            MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x29), 0, 0, 1, 0);
                        }
                    }
                    COOP_STEP(c)++;
                    break;
                case 1:
                    while (MotionGetState(pPL) != 0) {
                        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 5, 0, 5, 0);
                        pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 5, 0, 5, 0);
                        r30d_work->timer = SceExec(0x12, (TaskFunc) R30dTimerDisp, 0, 0, 2, 0);
                        COOP_STEP(c)++;
                        break;
                    }
                    break;
                case 2:
                    if (COOP_TIMER(c) == 0 && COOP_NUM(c) == 0) {
                        COOP_MODE(c) = 2;
                        COOP_STEP(c) = 0;
                    } else {
                        ActBtn.set(ACT_OPERATION, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
                        if (Key.trg & 0x00080000) {
                            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 5, 0, 1, 0);
                            pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 5, 0, 1, 0);
                            for (int k = 0; k < 2; k++) {
                                cObj* o = r30d_work->obj[k];
                                if (o) {
                                    MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2A), 0, 0, 1, 0);
                                }
                            }
                            SndCall(6, 3, &pPL->pos, 0, 0, 0);
                            if ((COOP_NUM(c) == 1 && COOP_TIMER(c) <= 10) || (COOP_NUM(c) == 0 && COOP_TIMER(c) > 19)) {
                                COOP_STEP(c) = 0;
                                COOP_MODE(c) = 1;
                            } else {
                                COOP_STEP(c) = 0;
                                COOP_MODE(c) = 2;
                            }
                        }
                    }
                    break;
                }
                break;
            case 1:
                if (COOP_STEP(c) == 0) {
                    while (MotionGetState(pPL) != 0) {
                        SceAtSetEnable(0x10, 0);
                        SceAtSetEnable(0x11, 0);
                        if (r30d_work->timer) {
                            SceKill(r30d_work->timer);
                        }
                        r30d_work->timer = 0;
                        EffectEspDelete(1, ESP_CORE_KIND_ROOM01, 0, 0);
                        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM01, 0);
                        EffectEfmDelete(1, ESP_CORE_KIND_ROOM01, 0);
                        EstSet(0, -1, 0, 0, EFF_ROOM, 6, 1, ESP_CORE_KIND_ROOM01, 0, 0);
                        EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
                        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
                        EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
                        for (int k = 0; k < 4; k++) {
                            EstSet(0, -1, 0, 0, EFF_ROOM, r30d_digit[k], 1, ESP_CORE_KIND_ROOM00, 0, 0);
                        }
                        // The final `COOP_ACTIVE(c) = 0` stores the reversed digit-loop counter (`stw r31`): its zero
                        // is a pseudo set here, in the block after the loop exit, and used in the store's block, so
                        // loop.c scan_loop skips it (not reg_in_basic_block_p while maybe_never) instead of merging it
                        // into the while loop's hoisted zero, and cse2 canonicalizes it to the counter it knows is 0
                        // on the `bne` fall-through (record_jump_equiv). A literal 0 at the store is a same-block
                        // movable and merges.
                        int zero = 0;
                        if (!(pG->Room_flg[0] & 0x10000000)) {
                            SndCall(6, 9, 0, 0, 0, 0);
                        }
                        KyfFlagOn(pG, KYF_R30D_TO_R30F_DOOR);
                        KyfFlagOn(pG, KYF_ST1_10);
                        SceUpCut(3, 6, 4, 0);
                        COOP_ACTIVE(c) = zero;
                        break;
                    }
                }
                break;
            case 2:
                if (COOP_STEP(c) == 0) {
                    while (MotionGetState(pPL) != 0) {
                        if (r30d_work->timer) {
                            SceKill(r30d_work->timer);
                        }
                        r30d_work->timer = 0;
                        SceUpCut(4, 6, -1, 0);
                        COOP_ACTIVE(c) = 0;
                        EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
                        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
                        EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
                        for (int k = 0; k < 2; k++) {
                            cObj* o = r30d_work->obj[k];
                            if (o) {
                                MotionSetCore(o, &o->Motion, ROOM_ARC_PTR(pG->pRoom, 0x2C), 0, 0, 1, 0);
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
        SceSleep(1);
    } while (COOP_ACTIVE(c) != 0);
    CamCtrl.Comeback(0);
    pPL->endEvent(2);
    EmRoutineSet(pSUB, 0, 0, 0, 0);
    SubCharCtrl(1, 0);
}

// The countdown display: digits 4..1 for 30 frames each, then the "0" that ends the attempt.
static void R30dTimerDisp()
{
    R30dCoop* c = &r30d_work->coop;

    COOP_TIMER(c) = 0;
    COOP_NUM(c) = 4;
    pG->Room_flg[0] &= ~0x10000000;
    for (;;) {
        COOP_TIMER(c)--;
        if (COOP_TIMER(c) <= 0) {
            COOP_NUM(c)--;
            if (COOP_NUM(c) < 0) {
                COOP_TIMER(c) = 0;
                COOP_NUM(c) = 0;
                break;
            }
            COOP_TIMER(c) = 30;
            EstSet(0, -1, 0, 0, EFF_ROOM, r30d_digit[COOP_NUM(c)], 1, ESP_CORE_KIND_ROOM00, 0, 0);
            if (COOP_NUM(c) != 0) {
                SndCall(6, 7, 0, 0, 0, 0);
            } else {
                pG->Room_flg[0] |= 0x10000000;
                SndCall(6, 8, 0, 0, 0, 0);
            }
        }
        SceSleep(1);
    }
}

// Ashley's aux routine at the coop lever: just advance her motion while she exists.
static void funcAshleySwitch(cEm* p)
{
    if (pSUB) {
        p->motionMove();
    }
}

// Ashley at the front shutter: turn, crawl under it (three motions), then release the shutter.
static void funcAshleyShutter(cEm* p)
{
    Vec v;

    if (pSUB == NULL) {
        return;
    }
    switch (p->r_no_2) {
    case 0:
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        p->setAng(&v);
        p->atari.off();
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2D), 3, 0, 0x101, 0);
        SndCall(6, 6, &p->pos, 0, 0, 0);
        p->r_no_2++;
        break;
    case 1:
        if (p->motionMove() != 0) {
            p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2E), 3, 0, 0x101, 0);
            p->r_no_2++;
        }
        break;
    case 2:
        if (p->motionMove() != 0) {
            p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2F), 3, 0, 0x101, 0);
            p->r_no_2++;
        }
        break;
    case 3:
        if (p->motionMove() != 0) {
            pG->Room_flg[0] |= 0x40000000;
            p->atari.on();
            p->r_no_2++;
        }
        break;
    }
    pSUB->atari.setPriority(3);
}

// Battle stream on while a Ganado has found the player, off when none does.
static void SceBgmCheck()
{
    int on = 0;

    for (;;) {
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
