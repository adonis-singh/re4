#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "pl_wep.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "st_mgr_event.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "math_sub.h"
#include "motion.h"
#include "db_log.h"

// Room 2-2A (D:/Bio4/Prog/r22a.cpp): the mine shaft; the rope down / up (a copy of r10c's ladder
// event, message and all), the lift and the s00 event.

void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp

struct R22aWork {
    u8 dummy;
};

static R22aWork* r22a_work;

static Vec r22a_plOfs0 = {-158.44f, -2608.9001f, -524.0f};
static Vec r22a_plOfs1 = {-158.44f, -4911.3799f, 120.060005f};

static void r22a_RopeMove(int side);
extern "C" void R22A_Event();
extern "C" void Evt_R22AS00_Func(Event* e);
static void r22a_EleDown();
static void r22a_EleUp();

// pPL stores through references: the pPL reload after each one.
static inline void FSetP(f32& d, f32 v) { d = v; }

// Room init: areas 2/3 = climb down / up the rope; the s00 (and s99) callback; until Room_flg bit 0 area
// 6 = the s00 event (pre-loaded), else off; object 0x50 hidden; areas 4/5 = the lift down / up; arriving
// by a jump (System_flg 0x100) the lift cage 0x4F starts at the bottom (y -8500).
void R22aInit()
{
#line 50 "D:/Bio4/Prog/r22a.cpp"
    r22a_work = (R22aWork*) MEM_CALLOC(sizeof(R22aWork), 1, 0xd);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r22a_RopeMove, 0, 1);
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r22a_RopeMove, (void*) 1, 1);
    EvtMgr.SetFunc("evt_r22as00_func", (void*) Evt_R22AS00_Func);
    EvtMgr.SetFunc("evt_r22as99_func", (void*) Evt_R22AS00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtSetEnable(6, 1);
        SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) R22A_Event, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r22as00.evd", 0, 0, 0, 0);
    } else {
        SceAtSetEnable(6, 0);
    }
    SmdSetTrans(0x50, 0);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r22a_EleDown, 0, 1);
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r22a_EleUp, 0, 1);
    if (SysFlagChk(pG, SYS_LOAD_GAME)) {
        SmdGetObjPtr(0x4F)->be_flag |= 0x20;
        SmdGetObjPtr(0x4F)->pos.y = -8500.0f;
    }
}

// Per-frame room main: nothing.
void R22aMain()
{
}

// Areas 2 / 3: Leon climbs down (side 0) or up (side 1) the rope.
static void r22a_RopeMove(int side)
{
    // The ang copy's word order (4 before 8) is decided in sched2 by the `flags` load below: read
    // through the struct view, the pG load is not a fixed scalar, so the frame stores of the copy
    // conflict with it and the word-4 store chain gets the same priority as the word-8 one (whose
    // r9 is reused by that load); the tie then falls to the dependents count / source order.
    static const Vec r22a_ropePos = {5634.0f, 51500.0f, -32822.0f};
    static const Vec r22a_ropeAng = {0.0f, -1.5707964f, 0.0f};
    Vec pos = r22a_ropePos;
    Vec ang = r22a_ropeAng;
    Vec out;
    cPlayer* pl = pPL;
    u32 flags = pGS->Stop_flg;
    cObj* obj;

    KeyStop(0xEFCF0000ULL);
    U32Set(pG->Stop_flg, 0xFFFFFFFF);
    SpfFlagOff(pG, SPF_SCE);
    FadeSetW(2, 10, 0, 0);
    SceSleep(10);
    SmdSetTrans(0x2F, 0);
    pG->Stop_flg = flags;
    FadeSetW(0x80000002, 10, 0, 0);
    SceEventStart(0);
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    if (side == 0) {
        SndStrReq(1, 0x25, 0x80000003, 0, 0, 0.0f);
        {
            Mtx m;
            cPlayer* p;

            low_RotMatrix(m, &ang);
            TransMatrix(m, &pos);
            PSMTXMultVec(m, &r22a_plOfs0, &out);
            p = pPL;
            p->setPos(&out);
            p->setAng(&ang);
            obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &ang, 0x10, 1);
            if (obj == 0) {
                pLog->err(0, 0, "R10cTestPosMove : set failed");
                return;
            }
            pPL->setNoSuspend(1);
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 10, 0, 0x201, 0);
            obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 10, 0, 1, 0);
            SceSleep((u32) MotionGetMaxFrame(&pPL->Motion) - 30);
            FadeSetW(2, 30, 0, 0);
            SceSleep(30);
            pPL->setNoSuspend(0);
            ObjMgr.destroy(obj);
        }
        {
            Vec pos2 = {6650.0f, 26500.0f, -33167.0f};
            Vec ang2;
            f32 ry = 2.02f;
            cPlayer* p;
            Vec* pa = &ang2;

            p = pPL;
            p->setPos(&pos2);
            ang2.x = 0.0f;
            pa->y = ry;
            ang2.z = 0.0f;
            p->setAng(&ang2);
        }
    } else {
        SndStrReq(1, 0x26, 0x80000003, 0, 0, 0.0f);
        {
            Mtx m;
            cPlayer* p;

            low_RotMatrix(m, &ang);
            TransMatrix(m, &pos);
            PSMTXMultVec(m, &r22a_plOfs1, &out);
            p = pPL;
            p->setPos(&out);
            p->setAng(&ang);
            obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), &pos, &ang, 0x10, 1);
            if (obj == 0) {
                pLog->err(0, 0, "R10cTestPosMove : set failed");
                return;
            }
            pPL->setNoSuspend(1);
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 10, 0, 1, 0);
            obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 10, 0, 1, 0);
            SceSleep((u32) MotionGetMaxFrame(&pPL->Motion) - 30);
            FadeSetW(2, 30, 0, 0);
            SceSleep(30);
            pPL->setNoSuspend(0);
            ObjMgr.destroy(obj);
        }
        {
            Vec pos2 = {4224.0f, 51500.0f, -32759.0f};
            Vec ang2;
            f32 ry = -1.61f;
            cPlayer* p;
            Vec* pa = &ang2;

            p = pPL;
            p->setPos(&pos2);
            ang2.x = 0.0f;
            pa->y = ry;
            ang2.z = 0.0f;
            p->setAng(&ang2);
        }
    }
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    SceEventEnd(0);
    CamCtrl.m_QuasiFPS.setPlayerLocation(pPL->mat, pPL->pFloor_norm);
    FadeSetW(0x80000002, 30, 0, 0);
    SmdSetTrans(0x2F, 1);
}

// Area 6: the s00 event (end of chapter 2-3 part).
extern "C" void R22A_Event()
{
    SceEventStart(0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(6, 0);
        EvtMgr.EvtReadExec("event/evd/r22as00.evd", 0, 0x50);
        SceSleep(1);
        SceEventEnd(0);
        ScfFlagOn(pG, SCF_ST3_IN);
        SceAtInitSaveItem();
        SceSetChapterEnd(CHAPTER_4_4, 1);
    }
}

// Event r22as00 callback: the knife model wep0200 shown on cut 0 and hidden from cut 1.
extern "C" void Evt_R22AS00_Func(Event* e)
{
    void* mod;

    if (e->funcMode == 1) {
        switch (e->NowCut) {
        case 0:
            if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                Obj18CmfOn((cObj*) mod, 5);
                ((cObj*) mod)->be_flag &= ~2;
            }
            break;
        case 1:
            if (e->GetMod(&mod, "wep0200", 0, 0) == 1) {
                Obj18CmfOn((cObj*) mod, 5);
                ((cObj*) mod)->be_flag |= 2;
            }
            break;
        }
    }
}

// Area 4: the lift goes down (camera cuts 2 / 3).
static void r22a_EleDown()
{
    u32 i;
    cObj* obj;

    pPL->setNoSuspend(1);
    SceEventStart(0);
    ((cUnitEventView*) pPL)->beginEvent(0);
    DpfFlagOn(pG, DPF_SHADOW);
    CamCtrl.CutCall(2);
    obj = SmdGetObjPtr(0x4F);
    BitOn(obj->be_flag, 0x20);
    f32 spd = 0.0f;
    f32 max = 100.0f;
    SndCall(6, 0, &pPL->pos, 0, 0, 0);
    for (i = 0; i < 80; i++) {
        SceSleep(1);
        spd += (max - spd) * 0.1f;
        pPL->pos.y -= spd;
        SmdGetObjPtr(0x4F)->pos.y -= spd;
        if (i < 65) {
            FadeSetW(2, 15, 0, 0);
        }
    }
    SceSleep(30);
    FSetP(pPL->pos.y, -3500.0f);
    FSetP(pPL->pos.x, 15430.0f);
    FSetP(pPL->pos.z, -38962.0f);
    SmdGetObjPtr(0x4F)->pos.y = -3500.0f;
    FadeSetW(0x80000002, 15, 0, 0);
    CamCtrl.CutCall(3);
    while (pPL->pos.y > -8500.0f) {
        SceSleep(1);
        spd += (max - spd) * 0.1f;
        pPL->pos.y -= spd;
        FSub(SmdGetObjPtr(0x4F)->pos.y, spd);
    }
    SndCall(6, 1, &pPL->pos, 0, 0, 0);
    pPL->pos.y = -8500.0f;
    SmdGetObjPtr(0x4F)->pos.y = -8500.0f;
    SceSleep(15);
    DpfFlagOff(pG, DPF_SHADOW);
    SceEventEnd(0);
    ((cUnitEventView*) pPL)->endEvent(0);
    pPL->setNoSuspend(0);
}

// Area 5: the lift goes up (camera cuts 4 / 5).
static void r22a_EleUp()
{
    u32 i;
    cObj* obj;

    pPL->setNoSuspend(1);
    SceEventStart(0);
    ((cUnitEventView*) pPL)->beginEvent(0);
    DpfFlagOn(pG, DPF_SHADOW);
    CamCtrl.CutCall(4);
    obj = SmdGetObjPtr(0x4F);
    BitOn(obj->be_flag, 0x20);
    f32 spd = 0.0f;
    f32 max = 100.0f;
    SndCall(6, 0, &pPL->pos, 0, 0, 0);
    for (i = 0; i < 80; i++) {
        SceSleep(1);
        spd += (max - spd) * 0.1f;
        pPL->pos.y += spd;
        SmdGetObjPtr(0x4F)->pos.y += spd;
        if (i < 65) {
            FadeSetW(2, 15, 0, 0);
        }
    }
    SceSleep(30);
    FSetP(pPL->pos.y, 21500.0f);
    FSetP(pPL->pos.x, 15430.0f);
    FSetP(pPL->pos.z, -38962.0f);
    SmdGetObjPtr(0x4F)->pos.y = 21500.0f;
    FadeSetW(0x80000002, 15, 0, 0);
    CamCtrl.CutCall(5);
    while (pPL->pos.y < 26500.0f) {
        SceSleep(1);
        spd += (max - spd) * 0.1f;
        pPL->pos.y += spd;
        FAdd(SmdGetObjPtr(0x4F)->pos.y, spd);
    }
    SndCall(6, 1, &pPL->pos, 0, 0, 0);
    pPL->pos.y = 26500.0f;
    SmdGetObjPtr(0x4F)->pos.y = 26500.0f;
    SceSleep(15);
    DpfFlagOff(pG, DPF_SHADOW);
    SceEventEnd(0);
    ((cUnitEventView*) pPL)->endEvent(0);
    pPL->setNoSuspend(0);
}
