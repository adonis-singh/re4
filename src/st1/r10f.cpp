#include "types.h"
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
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj00.h"
#include "objGondola.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emwindow.h"
#include "etc_model.h"
#include "esp.h"
#include "est.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "motion.h"
#include "math_sub.h"
#include "route_ck.h"
#include "item.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "sscrn.h"
#include "fade.h"
#include "snd.h"

// Room 1-0f (D:/Bio4/Prog/r10f.cpp): the cable car; the ten gondola objects, getting on / off
// with the partner, the Ganados riding along, the false-eye door and the lockers.

struct R10fWork {
    cObjGondola* gondola[10];  // 0x00
    int idx;                   // 0x28  gondola the player rides
    int pad_2C;
};

static R10fWork* r10f_work;

// The gondola sub-motion works: 0xD0 bytes each in the original build (motion.h's MotionWork is
// the later 0xDC layout).
struct R10fMotWork {
    u8 buf[0xD0];
};


static void r10f_GondolaGetOn(int side);
static void r10f_GondolaGetOff(int side);
static void r10f_GondolaEmSet(int idx);
extern "C" cObj* r10f_setFalseEye();
extern "C" void r10f_DoorOpen();
static void r10f_DoorOpenCamera();
static void r10f_checkFalseEyeUse();
static void r10f_DoorClose(u32 no);
static void r10f_LockerOpen(int id);
static void r10f_LockerOpened(int id);
static void r10f_TreasureBoxOpen(int id);
static void r10f_TreasureBoxOpened(int id);

// Room init: the three locked doors (area 2 = the false-eye door with its key-use watcher, area 0 -> door
// 0x11D, area 3 -> 0x11E, each until its Key_flg[0] bit), ten cObjGondola cars with their loop
// motions phase-shifted by 0x1C2 frames and per-car sub-motion works; area 7/8 = get on (side 0/1);
// until Room_flg bit 0 (the ride done) area 9 = get off at side 1 and Ganados 0x32/0x35 get their
// gondola-riding motions. Window 0xC starts broken; three locker item events and one treasure box.
void R10fInit()
{
    Vec pos;
    Vec rot;
    cEmWindow* win;

#line 57 "D:/Bio4/Prog/r10f.cpp"
    r10f_work = (R10fWork*) MEM_CALLOC(sizeof(R10fWork), 1, 0xd);

    if (!KyfFlagChk(pG, KYF_R10F_TO_R200_DOOR)) {
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r10f_DoorClose, 0, 1);
        SceExec(0x12, (TaskFunc) r10f_checkFalseEyeUse, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (!KyfFlagChk(pG, KYF_R11D_TO_R10F_DOOR)) {
        SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r10f_DoorClose, (void*) 0x11D, 1);
    }
    if (!KyfFlagChk(pG, KYF_R11E_TO_R10F_DOOR)) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r10f_DoorClose, (void*) 0x11E, 1);
    }
    {
        R10fMotWork* m;
        u32 i;

        pos.x = 0.0f;
        pos.y = -1000.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
#line 93 "D:/Bio4/Prog/r10f.cpp"
        m = (R10fMotWork*) MEM_CALLOC(sizeof(R10fMotWork) * 10, 1, 0xd);
        for (i = 0; i < 10; i++) {
            r10f_work->gondola[i] = (cObjGondola*) SetGondola(ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24), &pos, &rot);
            if (r10f_work->gondola[i] != 0) {
                r10f_work->gondola[i]->setMoveMotion(ROOM_ARC_PTR(pG->pRoom, 0x26), (u16) (i * 0x1C2));
                if (m != 0) {
                    r10f_work->gondola[i]->setSubMotion((MotionWork*) m, ROOM_ARC_PTR(pG->pRoom, 0x30), ROOM_ARC_PTR(pG->pRoom, 0x31));
                    m++;
                }
            }
        }
    }
    SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r10f_GondolaGetOn, 0, 1);
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r10f_GondolaGetOn, (void*) 1, 1);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        cEm* em;

        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r10f_GondolaGetOff, (void*) 1, 1);
        if ((em = EmSetFromList2(0x32, 1)) != 0) {
            ((cEmGanado*) em)->setGondolaMotion(ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x32));
        }
        if ((em = EmSetFromList2(0x35, 1)) != 0) {
            ((cEmGanado*) em)->setGondolaMotion(ROOM_ARC_PTR(pG->pRoom, 0x2D), ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x32));
        }
    }
    if (getRoomEtcWindow(0xC, &win, 1)) {
        win->SetBreakModel();
    }
    SceSetItemEvent(0xA, 0x81, 1, 0xD, r10f_LockerOpen, r10f_LockerOpened, 0x35, 0);
    SceSetItemEvent(0xB, 0x8E, 2, 0xF, r10f_LockerOpen, r10f_LockerOpened, 0x36, 0);
    SceSetItemEvent(0xC, 0x8D, 3, 0xE, r10f_LockerOpen, r10f_LockerOpened, 0x38, 0);
    SceSetItemEvent(0xE, 0x87, 4, 0x10, r10f_TreasureBoxOpen, r10f_TreasureBoxOpened, 0x5E, 0);
}

// Per-frame room main: nothing.
void R10fMain()
{
}

// View of the three tables of GondolaGetOn/GetOff as one object: the target forms `&posA[side]` as
// `mulli r4,side,12; add r4,r4,&mot; addi r4,r4,24` (`&posB[side]`: `addi 48`) straight into the
// argument register, recomputed at every site (GetOff) or from one PRE'd `side*12` (GetOn).
struct R10fGondolaTbl {
    void* mot[2][3];
    Vec posA[2];
    Vec posB[2];
};

// The setPos argument goes through two inlines: integrate.c expands an inline's argument with
// EXPAND_SUM (`(plus (plus (mult side 12) t) 24)`, MULT first) and force_operand's it into the
// parameter copy as a chain of sets of ONE pseudo (`mulli T; add T,T,t; addi T,T,24`) -- a
// multi-set pseudo cse1 cannot share across the sites, so `side*12` is recomputed (GetOff) or PRE'd
// (GetOn: the posA site is in the block after the `sub` test). The table pointer argument
// `(R10fGondolaTbl*) mot` is `fp+8` copied into a fresh pseudo per site: in GetOff cse1 merges
// them with the mot copy's destination, in GetOn gcse PREs them into a copy of it (`mr r23,r8`).
static inline void r10f_setPos(cModel* m, Vec* p) { m->setPos(p); }

// Get on the cable car at `side` (0: the village side, 1: the far side): Leon and Ashley step
// on, the gondolas move to their positions and the ride starts.
static void r10f_GondolaGetOn(int side)
{
    void* mot[2][3] = {
        {ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x29)},
        {ROOM_ARC_PTR(pG->pRoom, 0x27), ROOM_ARC_PTR(pG->pRoom, 0x28), ROOM_ARC_PTR(pG->pRoom, 0x2C)},
    };
    Vec posA[2] = {{22905.39f, 10254.28f, -36713.24f}, {22905.39f, -18215.59f, -151632.31f}};
    Vec posB[2] = {{22633.78f, 10254.28f, -36313.7f}, {22633.78f, -18215.59f, -151232.8f}};
    Vec zero;
    Vec p;
    cObj* obj;
    cSubChar* sub = SUB_CHAR();

    if (sub != 0 && RouteCkPosToPosDis(&pPL->pos, &sub->pos) > 10000.0f) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        SceExit();
    }
    SceEventStart(0);
    SndStrReq(1, 0xE1, 0x80000003, 0, 0, 0.0f);
    pPL->setNoSuspend(1);
    {
        Vec ang;
        Vec* pa = &ang;
        f32 ry = 1.5707964f;

        {
            cPlayer* pl = pPL;

            r10f_setPos(pl, &((R10fGondolaTbl*) mot)->posA[side]);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x27), 3, 0, 0x201, 0);
        if (pSUB != 0) {
            SubCharCtrl(SCC_AUX_MOT, 0);
            pSUB->setNoSuspend(1);
            {
                cSubChar* s = SUB_CHAR();

                r10f_setPos(s, &((R10fGondolaTbl*) mot)->posB[side]);
                ang.x = 0.0f;
                pa->y = ry;
                ang.z = 0.0f;
                s->setAng(&ang);
            }
            pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x28), 3, 0, 1, 0);
        }
    }
    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 0.0f;
    obj = SetObj00(ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24), &zero, &p);
    obj->setNoSuspend(1);
    MotSetObj00(obj, mot[side][2], 1, 0);
    while (!(MotionGetState(pPL) & 4)) {
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 0)) {
        int cut;

        // A for-scope counter per loop: one shared `i` aggregates the three loops' references and
        // outranks the loops' givs in global-alloc (r29 instead of r28). `int f` + `(u16) f` at the
        // calls: the mask (`clrlwi r5`) sits at each use, a promoted u16 local masks once at the store.
        for (u32 i = 0; i < 10; i++) {
            if (r10f_work->gondola[i] != 0) {
                int f;

                if (side == 0) {
                    f = (150 + i * 450) % 4500;
                } else {
                    f = (400 + i * 450) % 4500;
                }
                if (i == 0) {
                    r10f_work->gondola[i]->setMoveMotion(ROOM_ARC_PTR(pG->pRoom, 0x25), (u16) f);
                } else {
                    r10f_work->gondola[i]->setMoveMotion(ROOM_ARC_PTR(pG->pRoom, 0x26), (u16) f);
                }
                r10f_work->gondola[i]->setNoSuspend(1);
            }
        }
        // p is written in full in both arms (jump2 cross-jumps the four stores into the join; the 0.0
        // and angle pool loads stay in the arms).
        if (side == 0) {
            r10f_work->idx = 1;
            p.x = 0.0f;
            p.y = 1.5707964f;
            p.z = 0.0f;
            cut = 6;
        } else {
            r10f_work->idx = 4;
            p.x = 0.0f;
            p.y = -1.5707964f;
            p.z = 0.0f;
            cut = 5;
        }
        r10f_work->gondola[r10f_work->idx]->setRidePL();
        pPL->setAng(&p);
        if (pSUB != 0) {
            pSUB->setAng(&p);
        }
        CamCtrl.CutCall(cut);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        FadeSetW(0, 10, 0, 0);
        while (Fade[0].flags & 1) {
            SceSleep(1);
        }
        for (u32 i = 0; i < 10; i++) {
            if (r10f_work->gondola[i] != 0) {
                r10f_work->gondola[i]->setNoSuspend(0);
            }
        }
        CamCtrl.Comeback(0);
    } else {
        for (u32 i = 0; i < 10; i++) {
            if (r10f_work->gondola[i] != 0) {
                r10f_work->gondola[i]->setMoveMotion(ROOM_ARC_PTR(pG->pRoom, 0x26), (u16) (i * 0x1C2));
            }
        }
        r10f_work->idx = 0;
        r10f_work->gondola[0]->setRidePL();
        SceExec(0x12, (TaskFunc) r10f_GondolaEmSet, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    obj->setNoSuspend(0);
    ObjMgr.destroy(obj);
    SceEventEnd(0);
    if (RsfCheck(G_ROOM_ID, 0)) {
        r10f_GondolaGetOff(side == 0);
    }
    SubCharCtrl(SCC_CHASE, 0);
}

// Get off at `side`.
static void r10f_GondolaGetOff(int side)
{
    void* mot[2][3] = {
        {ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B), ROOM_ARC_PTR(pG->pRoom, 0x29)},
        {ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B), ROOM_ARC_PTR(pG->pRoom, 0x2C)},
    };
    Vec posA[2] = {{24039.85f, 10254.28f, -35429.5f}, {24182.15f, -18215.59f, -150270.25f}};
    Vec posB[2] = {{24323.65f, 10254.28f, -34651.58f}, {24465.95f, -18215.59f, -149492.94f}};
    Vec zero;
    Vec p;
    cObj* obj;

    SceAtSetEnable(9, 0);
    SceEventStart(0);
    SndStrReq(1, 0xE2, 0x80000003, 0, 0, 0.0f);
    pPL->setNoSuspend(1);
    {
        Vec ang;
        Vec* pa = &ang;
        f32 ry = -1.5707964f;

        {
            cPlayer* pl = pPL;

            r10f_setPos(pl, &((R10fGondolaTbl*) mot)->posA[side]);
            ang.x = 0.0f;
            pa->y = ry;
            ang.z = 0.0f;
            pl->setAng(&ang);
        }
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, 0, 0x201, 0);
        if (pSUB != 0) {
            SubCharCtrl(SCC_AUX_MOT, 0);
            pSUB->setNoSuspend(1);
            {
                cSubChar* s = SUB_CHAR();

                r10f_setPos(s, &((R10fGondolaTbl*) mot)->posB[side]);
                ang.x = 0.0f;
                pa->y = ry;
                ang.z = 0.0f;
                s->setAng(&ang);
            }
            pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 3, 0, 1, 0);
        }
    }
    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 0.0f;
    obj = SetObj00(ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24), &zero, &p);
    obj->setNoSuspend(1);
    MotSetObj00(obj, mot[side][2], 1, 0);
    if (RsfCheck(G_ROOM_ID, 0)) {
        FadeSetW(0x80000000, 10, 0, 0);
    }
    while (!(MotionGetState(pPL) & 4)) {
        SceSleep(1);
    }
    obj->setNoSuspend(0);
    ObjMgr.destroy(obj);
    SceEventEnd(0);
    SubCharCtrl(SCC_CHASE, 0);
    r10f_work->gondola[r10f_work->idx]->setGetOffPL();
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
    }
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        RsfSet(G_ROOM_ID, 5);
        GameSave.save(pSaveData, -1);
    }
}

// The Ganados ride the gondolas behind the player's: three per car from the list table, each
// car once it has left the station.
static void r10f_GondolaEmSet(int idx)
{
    cEmWrap em;
    s16 tbl[6][3] = {
        {0x36, -1, -1}, {0x37, 0x38, 0x39}, {0x3A, 0x3B, -1}, {0x4B, -1, -1}, {0x4C, 0x4D, 0x4E}, {0x4F, -1, -1},
    };
    int cur;
    u32 k;

    cur = idx;
    if (cur > 2) {
        cur -= 3;
    } else {
        cur = idx + 7;
    }
    // Byte arithmetic `t + (k*6 + n*2)`: the inner sum expands to (plus n2 k6) (expr.c both_summands
    // swaps a MULT second operand to the front) and the outer plus then keeps the base first, `(plus t
    // (plus n2 k6))`. The peeled entry test folds n = 0 to `lhax r0,t,k6` (base first, like the target) while
    // loop.c's simplify_giv_expr associates the address as `(plus n2 (plus k6 t))`, so the stepping
    // pointer's init stays `add p,k6,t`. `t[k][n]` gives `(plus (mult k 6) t)` in both places.
    u8* t = (u8*) tbl;

    for (k = 0; k < 6; k++) {
        u32 n;

        while (r10f_work->gondola[cur]->Motion.Seq_frame <= 2000.0f) {
            SceSleep(1);
        }
        for (n = 0; n < 3 && *(s16*) (t + (k * 6 + n * 2)) != -1; n++) {
            if (em.setEm(*(s16*) (t + (k * 6 + n * 2)), -1, 0, 1, 0) == 1) {
                r10f_work->gondola[cur]->setRideEm(em.getPtr());
            }
        }
        cur--;
        cur = cur < 0 ? 9 : (cur > 9 ? 0 : cur);
    }
}

// The false eye in Leon's hand for the door.
extern "C" cObj* r10f_setFalseEye()
{
    Vec pos = {-139.3f, -43.01f, 32.75f};
    Vec rot = {-0.34927526f, -2.473737f, 1.6477758f};
    cObj* obj;

    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
    obj->be_flag |= 0x20;
    obj->setParent(pPL, 0xA, &pos, &rot);
    return obj;
}

// The false eye opens the door: Leon walks up, uses it, the camera cuts follow.
extern "C" void r10f_DoorOpen()
{
    cObj* eye;
    cPlayer* pl;
    int eff;
    SCE_TASK* cam;

    SceEventStart(0);
    eye = r10f_setFalseEye();
    pl = pPL;
    pl->setNoSuspend(1);
    pl->Wep->setTrans(0, 0);
    pl->setRightHand((int) ROOM_ARC_PTR(pG->pRoom, 0x22));
    pl->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 5, 0, 1, 0);
    eff = EspPullCoreKind();
    EstSet(eye, -1, 0, 0, EFF_ROOM, 0, 1, (u8) eff, 0, 0);
    Vec pos = {13293.0f, 4000.0f, 14581.0f};
    Vec ofs = {45.54f, 0.0f, -984.69f};
    f32 ry = 1.5707964f;
    Vec rot = {0.0f, 0.0f, 0.0f};
    Mtx m;
    Vec out;
    Vec ang;
    rot.y = ry;
    low_RotMatrix(m, &rot);
    TransMatrix(m, &pos);
    PSMTXMultVec(m, &ofs, &out);
    pl->setPos(&out);
    ang.x = 0.0f;
    ang.y = ry;
    ang.z = 0.0f;
    pl->setAng(&ang);
    SndStrReq(1, 0xE3, 0x80000003, 0, 0, 0.0f);
    cam = SceExec(0x12, (TaskFunc) r10f_DoorOpenCamera, 0, 0, SCE_PRIO_DEF_2, 0);
    SceSleep(30);
    while (MotionGetState(pl) != 4) {
        SceSleep(1);
    }
    SndCall(6, 8, 0, 0, 0, 0);
    SceSleep(30);
    SceKill(cam);
    CamCtrl.Comeback(0);
    EffectEspDelete(0, (u8) eff, 0, 0);
    EffectEspgenDelete(0, (u8) eff, 0);
    EffectEfmDelete(0, (u8) eff, 0);
    pl->Wep->setTrans(1, 0);
    pl->setRightHand(1);
    pl->setNoSuspend(0);
    eye->be_flag &= ~2;
    SceEventEnd(0);
    KyfFlagOn(pG, KYF_R10F_TO_R200_DOOR);
    SceAtDataReset(2);
}

// Camera cuts 7, 8, 9 of the door event, then wait to be killed.
static void r10f_DoorOpenCamera()
{
    CamCtrl.CutCall(7);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(8);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    for (;;) {
        SceSleep(1);
    }
}

// Task: waits until the player uses the False Eye (item 0x3D) and runs the door-opening event.
static void r10f_checkFalseEyeUse()
{
    while (ItemMgr.check(0x3D) != 1) {
        SceSleep(1);
    }
    r10f_DoorOpen();
}

// The locked doors: the false-eye door opens the sub screen when the eye is held; the messages.
static void r10f_DoorClose(u32 no)
{
    if (no == 0 && ItemMgr.num(0x3D) != 0) {
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        SceExit();
    }
    SndCall(6, 7, 0, 0, 0, 0);
    switch (no) {
    case 0:
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        break;
    case 0x11D:
        SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        break;
    case 0x11E:
        SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        break;
    }
}

// Item-event opener: the locker (OpenBoxMain type 0x1C) swings open for item `id`.
static void r10f_LockerOpen(int id)
{
    OpenBoxMain(0, 0, 0x1C, 0xFFFFFFFF, id, -1);
}

// Item-event "already opened": pose the locker open without the animation.
static void r10f_LockerOpened(int id)
{
    OpenBoxMain(0, 1, 0x1C, 0xFFFFFFFF, id, -1);
}

// Item-event opener: the treasure chest (type 0x5B, upward lid) opens for item `id`.
static void r10f_TreasureBoxOpen(int id)
{
    OpenBoxMain(OpenBoxUpXP, 0, 0x5B, id, 0xFFFFFFFF, -1);
}

// Item-event "already opened": pose the chest open.
static void r10f_TreasureBoxOpened(int id)
{
    OpenBoxMain(OpenBoxUpXP, 1, 0x5B, id, 0xFFFFFFFF, -1);
}
