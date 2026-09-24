#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
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
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "room_data.h"
#include "st_mgr_event.h"
#include "mes.h"
#include "fade.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "math_sub.h"
#include "vec.h"

// Room 3-10 (D:/Bio4/Prog/r310.cpp): the two crates Leon and Ashley push together, the lever pairs
// with their barred doors, the hiding spots and the S00 event with the Ganado that stands up.

struct R310Work {
    cObj* box1;         // 0x00  scroll object 3, pushed towards -x, then falls
    cObj* box2;         // 0x04  scroll object 4, pushed towards +x
    SCE_TASK* pushTask;  // 0x08  the Leon push task (r310_pushBoxN_leon)
    SCE_TASK* subTask;   // 0x0C  the Ashley push task (r310_pushBoxN_ashley)
    int pad_10;
    u32 se;             // 0x14  crate sliding sound (SndCall handle)
};

static R310Work* r310_work;

// The room bits live in the second word of the room's save record (RoomData).
#define R310_SAVE_FLAGS (*(u32*) (RoomData.getRoomSavePtr(pG->room_id) + 4))


// The event player model's per-model work word (cEm+0x328) the S00 event flags.
struct R310EvtModel {
    u8 pad_0[0x328];
    u32 flags;
};

static void r310_checkEmSwitch();
void r310_execHide_main(int on, int no);
static void r310_execHide0(int on);
static void r310_execHide1(int on);
static void r310_onFlag();
void r310_stopBoxSe(Vec* pos);
static void r310_pushBox2_ashley();
static void r310_pushBox2_leon();
static void r310_pushBox2();
static void r310_fallBox1();
static void r310_pushBox1_ashley();
static void r310_pushBox1_leon();
static void r310_pushBox1();
void r310_initBoxPush();
static void r310_checkBgm();
static void r310_checkEmStandUp_end();
static void r310_checkEmStandUp();
static void R310EventS00();
static void Evt_R310S00_Func(Event* e);

// Room init: JumpPoint 1/2 marks the S00 event seen (save record bit 31); Ashley initialised as the
// follower; the event pre-loaded (enemy of ESL 0x5A) and run on a first visit; the two lever pairs
// (etc switches 1/2 and 3/4) each linked to their barred door; the lever watcher, the stream, the
// crates, door 4's exit hook (save bit 0x20000000), the two hiding spots (areas 8/9).
void R310Init()
{
    cEmSwitch* sw0;
    cEmSwitch* sw1;
    cEmBarred* bar;

#line 52 "D:/Bio4/Prog/r310.cpp"
    r310_work = (R310Work*) MEM_CALLOC(sizeof(R310Work), 1, 0xd);
    if (pG->JumpPoint >= 1 && pG->JumpPoint <= 2) {
        R310_SAVE_FLAGS |= 0x80000000;
    }
    SubCharInit(1, &pPL->pos, pPL->ang.y);
    StaFlagOn(pG, STA_SUB_ASHLEY);
    EvtMgr.SetFunc("evt_r310s00_func", (void*) Evt_R310S00_Func);
    if ((int) R310_SAVE_FLAGS >= 0) {
        EvtMgr.EvtReadAram("event/evd/r310s00.evd", (u8) GetEmIdFromList(0x5A), 0, 1, 0);
        SceExec(0x12, (TaskFunc) R310EventS00, 0, 2, 2, 0);
    }
    getRoomEtcSwitch(1, (cEm**) &sw0, 1);
    getRoomEtcSwitch(2, (cEm**) &sw1, 1);
    getRoomEtcBarred(0, (cEm**) &bar, 1);
    if (sw0 && sw1 && bar) {
        sw0->setBarred(bar);
        sw0->setConnectSwitch(sw1);
        sw1->setBarred(bar);
        sw1->setConnectSwitch(sw0);
        sw0->setClosed();
        sw1->setClosed();
        bar->setClosed();
    }
    getRoomEtcSwitch(4, (cEm**) &sw0, 1);
    getRoomEtcSwitch(5, (cEm**) &sw1, 1);
    getRoomEtcBarred(3, (cEm**) &bar, 1);
    if (sw0 && sw1 && bar) {
        sw0->setBarred(bar);
        sw0->setConnectSwitch(sw1);
        sw1->setBarred(bar);
        sw1->setConnectSwitch(sw0);
        sw0->setClosed();
        sw1->setClosed();
        bar->setClosed();
    }
    SceExec(0x12, (TaskFunc) r310_checkEmSwitch, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r310_checkBgm, 0, 0, 2, 0);
    r310_initBoxPush();
    SceAtSetDoorFunc(4, (TaskFunc) r310_onFlag, 0);
    SceAtDataSet_hide(8, r310_execHide0);
    SceAtDataSet_hide(9, r310_execHide1);
}

// Per-frame room main: nothing.
void R310Main()
{
}

// A lever held closed for 450 frames while the player stands in its area reopens.
static void r310_checkEmSwitch()
{
    cEmSwitch* sw1;
    cEmSwitch* sw4;
    u32 cnt1 = 0;
    u32 cnt4 = 0;

    getRoomEtcSwitch(1, (cEm**) &sw1, 1);
    getRoomEtcSwitch(4, (cEm**) &sw4, 1);
    for (;;) {
        if (sw1) {
            if (sw1->ckOpen() == 0 && SceAtHitCheck(0xB) == 1) {
                cnt1++;
                if (cnt1 > 0x1C1) {
                    sw1->setOpen();
                }
            } else {
                cnt1 = 0;
            }
        }
        if (sw4) {
            if (sw4->ckOpen() == 0 && SceAtHitCheck(0xA) == 1) {
                cnt4++;
                if (cnt4 > 0x1C1) {
                    sw4->setOpen();
                }
            } else {
                cnt4 = 0;
            }
        }
        SceSleep(1);
    }
}

// The hiding spot lids (scroll objects 8 / 10): the lid swings down (on == 0) or back up.
void r310_execHide_main(int on, int no)
{
    cObj* o = SmdGetObjPtr(no);

    o->be_flag |= 0x20;
    if (on == 0) {
        f32 lim;

        if (no == 8) {
            lim = -2.0948f;
        } else {
            lim = -1.745f;
        }
        // `const add` before `spd`: pool order 0.1, 0.0 (r11d).
        const f32 add = 0.1f;
        f32 spd = 0.0f;

        SndCall(6, 0x14, &pSUB->pos, 0, 0, 0);
        // The exit store on the break path: peeled exit test (docs/matching.md COMPILER-DIFF #7/#9).
        for (;;) {
            o->pList->ang.z -= spd;
            spd += add;
            if (o->pList->ang.z < lim) {
                o->pList->ang.z = lim;
                break;
            }
            SceSleep(1);
        }
    } else {
        SndCall(6, 0x13, &pSUB->pos, 0, 0, 0);
        for (;;) {
            o->pList->ang.z += 0.2f;
            if (o->pList->ang.z > 0.0f) {
                o->pList->ang.z = 0.0f;
                break;
            }
            SceSleep(1);
        }
    }
}

// Hiding spot 0 (area 8): lid object 8.
static void r310_execHide0(int on)
{
    r310_execHide_main(on, 8);
}

// Hiding spot 1 (area 9): lid object 10.
static void r310_execHide1(int on)
{
    r310_execHide_main(on, 10);
}

// Door 4's exit hook: save record bit 0x20000000 (the room was left through it).
static void r310_onFlag()
{
    R310_SAVE_FLAGS |= 0x20000000;
}

// Stops the crate sliding sound (with the stop sound at `pos` when given).
void r310_stopBoxSe(Vec* pos)
{
    if (r310_work->se) {
        if (pos) {
            SndCall(6, 1, pos, 0, 0, 0);
            SceSleep(5);
        }
        SndStop(r310_work->se, 2);
        r310_work->se = 0;
    }
}

// Ashley joins Leon at crate 2 (the +x push): she walks to the far side and pushes while he does.
static void r310_pushBox2_ashley()
{
    Vec subPos;
    Vec plPos;
    Vec goal;
    Vec d;
    Vec start;
    u32 i;

    if (pSUB == NULL) {
        r310_work->subTask = 0;
        return;
    }
    for (i = 0; i < 90; i++) {
        if (PSVECSquareDistance(&pSUB->pos, &pPL->pos) < 16000000.0f) {
            goto near;
        }
        SceSleep(1);
    }
    if (pSys->language == 0) {
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        // COMPILER-DIFF: #12 fallthrough-arm form. A literal 0 here is cse's known-zero `language` register (kept in
        // r30 across SceMesSet); the original's arm stored a fresh `li 0`. `(work & 4) >> 3` is 0 only to combine.
        r310_work->subTask = (SCE_TASK*) (((u32) r310_work & 4) >> 3);
    }
    SceExit();
near:
    plPos = pPL->pos;
    goal = pPL->pos;
    plPos.x += 1000.0f;
    if (SceAtHitCheck(7) == 1) {
        goal.z -= 800.0f;
    } else {
        goal.z += 800.0f;
    }
    if (PSVECSquareDistance(&pSUB->pos, &goal) >= 90000.0f) {
        SubCharMoveTo(goal.x, goal.y, goal.z, 193.0f, 0);
        while ((SubCharGetStatus() & 0x00800000) == 0) {
            SceSleep(1);
        }
    }
    while ((SubCharGetStatus() & 1) == 0 && (SubCharGetStatus() & 2) == 0) {
        SceSleep(1);
    }
    // `const ang` here: the pool order 193, pi/2, 500 (pi/2's entry before the push offset).
    const f32 ang = 1.5707964f;
    SubCharCtrl(5, 0);
    Vec t = {0.0f, 0.0f, 0.0f};
    t.x = pSUB->pos.x + 500.0f;
    t.y = pSUB->pos.y;
    t.z = pSUB->pos.z;
    start = t;
    PSVECSubtract(&start, &pSUB->pos, &d);
    PSVECScale(&d, &d, 1.0f / 12.0f);
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 10, 0, 1, 0);
    while (MotionGetState(pSUB) != 4) {
        cSubChar* sub;

        if ((SubCharGetStatus() & 0x80) || (SubCharGetStatus() & 0x02000000)) {
            goto end;
        }
        PSVECAdd(&pSUB->pos, &d, &pSUB->pos);
        pSUB->ang.y = pSUB->ang.y + Muku2(pSUB->ang.y, ang, 0.17453292f);
        sub = SUB_CHAR();
        sub->setPos(&sub->pos);
        sub->setAng(&sub->ang);
        asm("" : : "r"(sub)); // COMPILER-DIFF: 12 (regmove operand pick, r20d)
        SceSleep(1);
    }
    pSUB->ang.y = ang;
    pSUB->setAng(&pSUB->ang);
    subPos = pSUB->pos;
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 5, 0);
    pG->Room_flg[0] |= 0x40000000;
    while ((SubCharGetStatus() & 0x80) == 0 && (SubCharGetStatus() & 0x02000000) == 0) {
        pSUB->pos.z = subPos.z;
        pSUB->setPos(&pSUB->pos);
        SceSleep(1);
    }
end:
    pG->Room_flg[0] &= ~0x40000000;
    r310_work->subTask = 0;
    SubCharCtrl(1, 0);
}

// Leon pushes crate 2 towards +x while the push button is held; Ashley's task is started once he
// is in position.
static void r310_pushBox2_leon()
{
    Vec d;
    Vec goal;

    pPL->beginEvent(0);
    pPL->atari.onSca();
    PlSetHand(1, 0);
    Vec plPos = {0.0f, 0.0f, 0.0f};
    plPos.x = pPL->pos.x + 500.0f;
    plPos.y = pPL->pos.y;
    plPos.z = pPL->pos.z;
    goal = plPos;
    if (SceAtHitCheck(7) == 1) {
        goal.z = -16400.0f;
    } else {
        goal.z = -17100.0f;
    }
    PSVECSubtract(&goal, &pPL->pos, &d);
    PSVECScale(&d, &d, 1.0f / 12.0f);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 10, 0, 1, 0);
    while (MotionGetState(pPL) != 4) {
        cPlayer* pl;

        if (!(PlGetStatus() & 0x00020000)) {
            goto done;
        }
        PSVECAdd(&pPL->pos, &d, &pPL->pos);
        pPL->ang.y = pPL->ang.y + Muku2(pPL->ang.y, +1.5707964f, 0.17453292f);
        pl = pPL;
        pl->setPos(&pl->pos);
        pl->setAng(&pl->ang);
        asm("" : : "r"(pl)); // COMPILER-DIFF: 12 (regmove operand pick, r20d): `pl` must not die at the
                             // `addi r4,pl,0xa0` argument insn.
        SceSleep(1);
    }
    pPL->ang.y = +1.5707964f;
    pPL->setAng(&pPL->ang);
    plPos = pPL->pos;
    r310_work->subTask = SceExec(0x12, (TaskFunc) r310_pushBox2_ashley, 0, 0, 2, 0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 5, 0);
    pG->Room_flg[0] |= 0x80000000;
    while (PlGetStatus() & 0x00020000) {
        if (R310_SAVE_FLAGS & 0x20000000) {
            goto done;
        }
        if (r310_work->subTask == 0) {
            goto finish;
        }
        if (!(Key.on & 0x00080000)) {
            pG->Room_flg[0] &= ~0x80000000;
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
            while (MotionGetState(pPL) != 4 && (PlGetStatus() & 0x00020000)) {
                pPL->pos.z = plPos.z;
                pPL->setPos(&pPL->pos);
                SceSleep(1);
            }
            goto done;
        }
        pPL->pos.z = plPos.z;
        pPL->setPos(&pPL->pos);
        SceSleep(1);
    }
done:
    if (r310_work->subTask) {
        SCE_TASK* none = 0;

        pG->Room_flg[0] &= ~0x40000000;
        SceKill(r310_work->subTask);
        r310_work->subTask = none;
        SubCharCtrl(1, 0);
    }
finish:
    PlSetHand(0, 0);
    pPL->endEvent(2);
    r310_work->pushTask = 0;
}

// Crate 2 (area 6): moves while both push, done when it passes -12500.
static void r310_pushBox2()
{
    SceAtSetEnable(6, 0);
    r310_work->pushTask = SceExec(0x12, (TaskFunc) r310_pushBox2_leon, 0, 0, 2, 0);
    r310_stopBoxSe(0);
    while (r310_work->pushTask != 0) {
        if (FlagChkSign(pG->Room_flg, 0) && (pG->Room_flg[0] & 0x40000000)) {
            r310_work->box2->pos.x = r310_work->box2->pos.x + 10.0f;
            if (r310_work->se == 0) {
                r310_work->se = SndCall(6, 0x55, &r310_work->box2->pos, 0, 0, 0);
            }
            if (r310_work->box2->pos.x > -13200.0f) {
                SceAtSetEnable(0xE, 0);
            }
            if (r310_work->box2->pos.x > -12500.0f) {
                r310_stopBoxSe(&r310_work->box2->pos);
                R310_SAVE_FLAGS |= 0x20000000;
                SceExit();
            }
        } else {
            r310_stopBoxSe(&r310_work->box2->pos);
        }
        SceSleep(1);
    }
    r310_stopBoxSe(&r310_work->box2->pos);
    SceAtSetEnable(6, 1);
}

// Crate 1 tips over the edge and falls.
static void r310_fallBox1()
{
    f32 da;
    f32 dy;
    f32 y0;

    for (;;) {
        r310_work->box1->ang.z = r310_work->box1->ang.z + 0.034906585f;
        if (r310_work->box1->ang.z > 0.7853982f) {
            break;
        }
        SceSleep(1);
    }
    // Pool order 13, 1000, pi/2, 0.0, 0.01: the single-use constants declared first (r103 idiom).
    const f32 grav = 13.0f;
    const f32 fall = 1000.0f;
    const f32 lim = PI / 2;
    da = 0.034906585f;
    y0 = r310_work->box1->pos.y;
    dy = 0.0f;
    // Un-rotated: the sleep in the `if` arm, the exit in its `else`.
    for (;;) {
        da += 0.01f;
        if (r310_work->box1->ang.z < lim) {
            r310_work->box1->ang.z += da;
        } else {
            r310_work->box1->ang.z = lim;
        }
        r310_work->box1->pos.y = r310_work->box1->pos.y - dy;
        dy += grav;
        if (!(y0 - r310_work->box1->pos.y > fall)) {
            SceSleep(1);
        } else {
            break;
        }
    }
    do { } while (0); // ends the cse path: the exit block re-forms high(r310_work) (loop-exit form)
    EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
    SndCall(6, 0, &r310_work->box1->pos, 0, 0, 0);
    SceAtSetEnable(0, 0);
    SceAtSetEnable(0xC, 0);
    SceAtSetEnable(3, 0);
}

// Ashley at crate 1 (the -x push): the same as crate 2 mirrored.
static void r310_pushBox1_ashley()
{
    Vec subPos;
    Vec plPos;
    Vec goal;
    Vec d;
    Vec start;
    u32 i;

    if (pSUB == NULL) {
        r310_work->subTask = 0;
        return;
    }
    for (i = 0; i < 90; i++) {
        if (PSVECSquareDistance(&pSUB->pos, &pPL->pos) < 16000000.0f) {
            goto near;
        }
        SceSleep(1);
    }
    if (pSys->language == 0) {
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        // COMPILER-DIFF: #12 fallthrough-arm form. A literal 0 here is cse's known-zero `language` register (kept in
        // r30 across SceMesSet); the original's arm stored a fresh `li 0`. `(work & 4) >> 3` is 0 only to combine.
        r310_work->subTask = (SCE_TASK*) (((u32) r310_work & 4) >> 3);
    }
    SceExit();
near:
    plPos = pPL->pos;
    goal = pPL->pos;
    plPos.x += 1000.0f;
    if (SceAtHitCheck(2) == 1) {
        goal.z += 800.0f;
    } else {
        goal.z -= 800.0f;
    }
    if (PSVECSquareDistance(&pSUB->pos, &goal) >= 90000.0f) {
        SubCharMoveTo(goal.x, goal.y, goal.z, 193.0f, 0);
        while ((SubCharGetStatus() & 0x00800000) == 0) {
            SceSleep(1);
        }
    }
    while ((SubCharGetStatus() & 1) == 0 && (SubCharGetStatus() & 2) == 0) {
        SceSleep(1);
    }
    // `const ang` here: the pool order 193, pi/2, 500 (pi/2's entry before the push offset).
    const f32 ang = -1.5707964f;
    SubCharCtrl(5, 0);
    Vec t = {0.0f, 0.0f, 0.0f};
    t.x = pSUB->pos.x - 500.0f;
    t.y = pSUB->pos.y;
    t.z = pSUB->pos.z;
    start = t;
    PSVECSubtract(&start, &pSUB->pos, &d);
    PSVECScale(&d, &d, 1.0f / 12.0f);
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 10, 0, 1, 0);
    while (MotionGetState(pSUB) != 4) {
        cSubChar* sub;

        if ((SubCharGetStatus() & 0x80) || (SubCharGetStatus() & 0x02000000)) {
            goto end;
        }
        PSVECAdd(&pSUB->pos, &d, &pSUB->pos);
        pSUB->ang.y = pSUB->ang.y + Muku2(pSUB->ang.y, ang, 0.17453292f);
        sub = SUB_CHAR();
        sub->setPos(&sub->pos);
        sub->setAng(&sub->ang);
        asm("" : : "r"(sub)); // COMPILER-DIFF: 12 (regmove operand pick, r20d)
        SceSleep(1);
    }
    pSUB->ang.y = ang;
    pSUB->setAng(&pSUB->ang);
    subPos = pSUB->pos;
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0, 0, 5, 0);
    pG->Room_flg[0] |= 0x40000000;
    while ((SubCharGetStatus() & 0x80) == 0 && (SubCharGetStatus() & 0x02000000) == 0) {
        pSUB->pos.z = subPos.z;
        pSUB->setPos(&pSUB->pos);
        SceSleep(1);
    }
end:
    pG->Room_flg[0] &= ~0x40000000;
    r310_work->subTask = 0;
    SubCharCtrl(1, 0);
}

// Leon's side of the crate-1 push: event mode, hands free, he slides 500 units to the crate's -x side
// (goal z by area 2) over 12 frames, pushes while Ashley pushes too, then (unless the crate already
// fell) waits, releases and leaves event mode.
static void r310_pushBox1_leon()
{
    Vec d;
    Vec goal;

    pPL->beginEvent(0);
    pPL->atari.onSca();
    PlSetHand(1, 0);
    Vec plPos = {0.0f, 0.0f, 0.0f};
    plPos.x = pPL->pos.x - 500.0f;
    plPos.y = pPL->pos.y;
    plPos.z = pPL->pos.z;
    goal = plPos;
    if (SceAtHitCheck(2) == 1) {
        goal.z = -26400.0f;
    } else {
        goal.z = -25600.0f;
    }
    PSVECSubtract(&goal, &pPL->pos, &d);
    PSVECScale(&d, &d, 1.0f / 12.0f);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 10, 0, 1, 0);
    while (MotionGetState(pPL) != 4) {
        cPlayer* pl;

        if (!(PlGetStatus() & 0x00020000)) {
            goto done;
        }
        PSVECAdd(&pPL->pos, &d, &pPL->pos);
        pPL->ang.y = pPL->ang.y + Muku2(pPL->ang.y, -1.5707964f, 0.17453292f);
        pl = pPL;
        pl->setPos(&pl->pos);
        pl->setAng(&pl->ang);
        asm("" : : "r"(pl)); // COMPILER-DIFF: 12 (regmove operand pick, r20d): `pl` must not die at the
                             // `addi r4,pl,0xa0` argument insn.
        SceSleep(1);
    }
    pPL->ang.y = -1.5707964f;
    pPL->setAng(&pPL->ang);
    plPos = pPL->pos;
    r310_work->subTask = SceExec(0x12, (TaskFunc) r310_pushBox1_ashley, 0, 0, 2, 0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 5, 0);
    pG->Room_flg[0] |= 0x80000000;
    while (PlGetStatus() & 0x00020000) {
        if (R310_SAVE_FLAGS & 0x40000000) {
            goto done;
        }
        if (r310_work->subTask == 0) {
            goto finish;
        }
        if (!(Key.on & 0x00080000)) {
            pG->Room_flg[0] &= ~0x80000000;
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
            while (MotionGetState(pPL) != 4 && (PlGetStatus() & 0x00020000)) {
                pPL->pos.z = plPos.z;
                pPL->setPos(&pPL->pos);
                SceSleep(1);
            }
            goto done;
        }
        pPL->pos.z = plPos.z;
        pPL->setPos(&pPL->pos);
        SceSleep(1);
    }
done:
    if (r310_work->subTask) {
        SCE_TASK* none = 0;

        pG->Room_flg[0] &= ~0x40000000;
        SceKill(r310_work->subTask);
        r310_work->subTask = none;
        SubCharCtrl(1, 0);
    }
finish:
    PlSetHand(0, 0);
    pPL->endEvent(2);
    r310_work->pushTask = 0;
}

// Crate 1 (area 1): moves towards -x while both push; past -23300 it falls and the Ganado of list
// entry 0x69 (copied from 0x7D) appears.
static void r310_pushBox1()
{
    SceAtSetEnable(1, 0);
    r310_work->pushTask = SceExec(0x12, (TaskFunc) r310_pushBox1_leon, 0, 0, 2, 0);
    r310_stopBoxSe(0);
    while (r310_work->pushTask != 0) {
        if (FlagChkSign(pG->Room_flg, 0) && (pG->Room_flg[0] & 0x40000000)) {
            r310_work->box1->pos.x = r310_work->box1->pos.x - 10.0f;
            if (r310_work->se == 0) {
                r310_work->se = SndCall(6, 0x55, &r310_work->box1->pos, 0, 0, 0);
            }
            if (r310_work->box1->pos.x < -23300.0f) {
                r310_stopBoxSe(&r310_work->box1->pos);
                R310_SAVE_FLAGS |= 0x40000000;
                SceExec(0x12, (TaskFunc) r310_fallBox1, 0, 0, 2, 0);
                SceSleep(30);
                setEm(0x69, -1, 1, 1, 1);
                *&pG->Em_list[0x69] = *&pG->Em_list[0x7D];
                EmListSetAlive(0x69, 1);
                SceExit();
            }
        } else {
            r310_stopBoxSe(&r310_work->box1->pos);
        }
        SceSleep(1);
    }
    r310_stopBoxSe(&r310_work->box1->pos);
    SceAtSetEnable(1, 1);
}

// The two crates: crate 1 (object 3) with its push area 1 and the areas riding on it, unless it already
// fell (save bit 0x40000000: posed fallen on its side, areas off); crate 2 (object 4) likewise with area
// 6 / save bit 0x10000000.
void r310_initBoxPush()
{
    r310_work->box1 = SmdGetObjPtr(3);
    if (r310_work->box1) {
        r310_work->box1->be_flag |= 0x20;
        if (!(R310_SAVE_FLAGS & 0x40000000)) {
            SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r310_pushBox1, 0, 1);
            SceAtSetParent(1, r310_work->box1, 0);
            SceAtSetParent(0, r310_work->box1, 0);
            SceAtSetParent(0xC, r310_work->box1, 0);
            SceAtSetParent(2, r310_work->box1, 0);
        } else {
            SceAtSetEnable(0, 0);
            SceAtSetEnable(0xC, 0);
            SceAtSetEnable(3, 0);
            Vec pos = {-23300.0f, -1008.0f, -25845.0f};
            Vec rot = {0.0f, 0.0f, 1.5707964f};
            cObj* o = r310_work->box1;
            o->setPos(&pos);
            o->setAng(&rot);
        }
    }
    r310_work->box2 = SmdGetObjPtr(4);
    if (r310_work->box2) {
        r310_work->box2->be_flag |= 0x20;
        if (!(R310_SAVE_FLAGS & 0x20000000)) {
            SceAtDataSet_exec(6, 0x12, 0, (TaskFunc) r310_pushBox2, 0, 1);
            SceAtSetParent(6, r310_work->box2, 0);
            SceAtSetParent(5, r310_work->box2, 0);
            SceAtSetParent(0xD, r310_work->box2, 0);
            SceAtSetParent(7, r310_work->box2, 0);
        } else {
            SceAtSetParent(5, r310_work->box2, 0);
            SceAtSetParent(0xD, r310_work->box2, 0);
            SceAtSetEnable(0xE, 0);
            r310_work->box2->pos.x = -12500.0f;
        }
    }
}

// Battle stream 3 from a Ganado spotting the player until no regenerator (0x36) is alive, repeatedly.
static void r310_checkBgm()
{
    for (;;) {
        while (SceCkFindPL(0) == 0) {
            SceSleep(1);
        }
        SndRoomStrStart(1, 3, 1);
        while (SceCountEmAlive(0x36, -1) != 0) {
            SceSleep(1);
        }
        SndRoomStrStop(3);
        SceSleep(90);
    }
}

// The Ganado that stood up (list entry 0x5A) becomes a normal enemy; skipped, it is rebuilt from
// entry 0xBE with the key item, else from 0x7C.
static void r310_checkEmStandUp_end()
{
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    CamCtrl.Comeback(0);
    cEmWrap em;
    em.setEm(0x5A, -1, 1, 1, 1);
    em.setNoSuspend(0);
    if (pG->Room_flg[0] & 0x20000000) {
        em.destroy();
        if (pG->em_list_no >= 0) {
            u32* tbl = EM_FLG_ROW(pG->em_list_no);

            tbl[2] &= ~0x20;
        }
        *&pG->Em_list[0x5A] = *&pG->Em_list[0xBE];
        em.setEm(0x5A, -1, 1, 1, 1);
        if (em.getPtr() != 0) {
            SceAtSetEmItem(em.getPtr(), 0x85);
        }
    }
    SndCall(6, 2, 0, 0, 0, 0);
    *&pG->Em_list[0x5A] = *&pG->Em_list[0x7C];
    EmListSetAlive(0x5A, 1);
}

// Once lever 1 opens, the camera shows the Ganado standing up.
static void r310_checkEmStandUp()
{
    cEmWrap em;
    cEmSwitch* sw;

    em.setEm(0x5A, -1, 1, 1, 1);
    getRoomEtcSwitch(1, (cEm**) &sw, 1);
    if (sw) {
        while (sw->ckOpen() == 0) {
            SceSleep(1);
        }
        SceSleep(30);
        SceSetEventCancel(1, (TaskFunc) r310_checkEmStandUp_end, 0, 2, 1);
        SceEventStart(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        em.setPtr(0x5A, -1, 1);
        em.setFlag(1);
        em.setNoSuspend(1);
        if (em.getPtr() != 0) {
            while (MotionGetState(em.getPtr()) == 0) {
                SceSleep(1);
            }
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r310_checkEmStandUp_end();
    }
}

// The S00 event (first visit): Leon put at the door, the crates' Ganado set, the stage BGM.
static void R310EventS00()
{
    Vec p;
    if ((int) R310_SAVE_FLAGS >= 0) {
        R310_SAVE_FLAGS |= 0x80000000;
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadExec("event/evd/r310s00.evd", (u8) GetEmIdFromList(0x5A), EvtReadFlagNone);
        cPlayer* pl = pPL;
        p.x = -6877.0f;
        p.y = 0.0f;
        p.z = -25923.0f;
        pl->setPos(&p);
        pPL->be_flag |= 0x00200000;
        if (pSUB) {
            pSUB->be_flag |= 0x00200000;
        }
        SceExec(0x12, (TaskFunc) r310_checkEmStandUp, 0, 0, 2, 0);
        SndBgmTblSet(0x310, 1);
    }
}

// Event r310s00 callback: cut 0 fades in (unless skipped) and flags the pl0100 model's status 0x40;
// frame 50 fades again.
static void Evt_R310S00_Func(Event* e)
{
    if (e->FuncType == 1 && e->NowCut == 0) {
        if (e->NowFrame == 0) {
            void* mod;
            int skip = 1;

            if ((e->StatusFlag & EvtStfBit(EvtStfToolFrontExec)) == 0) {
                skip = 0;
            }
            if (skip == 0) {
                FadeSetW(2, 0, 0, 0);
            }
            if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                ((R310EvtModel*) mod)->flags |= 0x40;
            }
        }
        if (e->NowFrame == 50) {
            int skip = 1;

            if ((e->StatusFlag & EvtStfBit(EvtStfToolFrontExec)) == 0) {
                skip = 0;
            }
            if (skip == 0) {
                FadeSetW(0x80000002, 40, 0, 0);
            }
        }
    }
}
