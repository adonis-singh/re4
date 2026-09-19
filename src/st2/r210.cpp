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
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "mes.h"
#include "fade.h"
#include "snd.h"

// Room 2-10 (D:/Bio4/Prog/r210.cpp): the lift platform of the mine (r222 shares the code), the
// mine cart ride to and from r212, and Ashley's follow / wait areas.

struct R210Work {
    u8 dummy;
};

static R210Work* r210_work;

// Collision flag bits set through a raw (non-struct) store at the info's address: the following
// `pPL` load stays below it (pl_npc.cpp AtariOnRaw).
static inline void AtariOnRaw(cAtariInfo* at, u16 b) { *(u16*) ((u8*) at + 0x1a) |= b; }

// pl_npc.cpp: MotionMove is called with a second argument by the partner code.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// `flags &= 0xEFFF` through a reference with a u16 mask: a halfword `andi.` (BitOff16's promoted
// `~b` gives a word mask) whose store keeps the following `pSUB` load below it.
static inline void U16And(u16& d, u16 mask) { d &= mask; }
// Routine bytes through int parameters: one SI zero pseudo, the stores issued ff, fc, fd, fe.
static inline void EmRoutineSet(cEm* p, int fc, int fd, int fe, int ff)
{
    p->r_no_0 = fc;
    p->r_no_1 = fd;
    p->r_no_2 = fe;
    p->r_no_3 = ff;
}

static f32 r210_daiZ = -32012.0f;
static f32 r210_daiRotGo = -0.04f;
static f32 r210_daiZGo = -32012.0f;
static f32 r210_daiRotRet = 0.04f;
static f32 r210_daiZRet = -17345.0f;
static Vec r210_torokoGoPos0 = {-18606.17f, -2200.0f, -10500.0f};
static Vec r210_torokoGoPos1 = {-60406.152f, -2200.0f, 244499.9f};
static Vec r210_torokoRetPos0 = {-38271.953f, -2200.0f, -10559.35f};
static Vec r210_torokoRetPos1 = {-79771.914f, -2200.0f, 244440.62f};

static void asl_wait();
static void asl_chase();
static void r222_DummyDoorProc();
static void r222_dai_set();
static void funcAshley2(cEm* p);
static void r222_dai_go();
static void r222_dai_ret();
static void toroko_go(int dir);
static void toroko_ret(int dir);
static void plemRide(cPlayer* pl);

// Ashley waits at the cart (area 9) / follows again (area 0xA).
static void asl_wait()
{
    SubCharCtrl(SCC_STOP, 0);
    SceAtSetEnable(0xA, 1);
    SceAtSetEnable(9, 0);
}

// Area 0xA: Ashley follows Leon again (swap the wait / follow areas).
static void asl_chase()
{
    SubCharCtrl(SCC_CHASE, 0);
    SceAtSetEnable(0xA, 0);
    SceAtSetEnable(9, 1);
}

// Room init (the mine lift and cart platform): on a fresh entry Ashley counts as following; when she
// is, she is initialised in chase mode. Coming from r222 (the lower level) normally she is placed at the
// lift; area 0 = the door back (takes Ashley), 9/0xA = wait / follow, 3/4 = ride the left / right cart
// to r212, a return from r210 itself (Part 1/2) arrives by cart; areas 5/6 = lift down / up, 7/8 re-arm
// the lift.
void R210Init()
{
#line 53 "D:/Bio4/Prog/r210.cpp"
    r210_work = (R210Work*) MEM_CALLOC(sizeof(R210Work), 1, 0xd);
    if (pG->room_id_prev == 0xFFF) {
        if (StaFlagChk(pG, STA_SUB_ASHLEY) == 0) {
            StaFlagOn(pG, STA_SUB_ASHLEY);
        }
    }
    {
        u32 flags = pG->Status_flg[3];

        if (flags & 0x04000000) {
            SubCharInit(1, &pPL->pos, pPL->ang.y);
            SubCharCtrl(SCC_CHASE, 0);
            ScfFlagOff(pG, SCF_NO_ASHLEY_DIST_CK);
        }
    }
    if (SysFlagChk(pG, SYS_LOAD_GAME) == 0 && pG->room_id_prev == 0x222) {
        if (ScfFlagChk(pG, SCF_R213_ASHLEY_LOST) == 0) {
            SubCharInit(1, &pPL->pos, pPL->ang.y);
            StaFlagOn(pG, STA_SUB_ASHLEY);
            if (pSUB) {
                Vec v;

                v.x = 0.0f;
                v.y = 0.0f;
                v.z = -14875.0f;
                pSUB->setPos(&v);
                {
                    cSubChar* sub = pSUB;

                    v.x = 0.0f;
                    v.y = 3.14f;
                    v.z = 0.0f;
                    sub->setAng(&v);
                }
                SubCharCtrl(SCC_STOP, 0);
            }
            ScfFlagOn(pG, SCF_NO_ASHLEY_DIST_CK);
        }
        SmdGetObjPtr(0x20)->be_flag |= 0x20;
        SmdGetObjPtr(0x21)->be_flag |= 0x20;
        SmdGetObjPtr(0x21)->pos.z = r210_daiZ;
        SmdGetObjPtr(0x20)->pos.z = r210_daiZ;
    }
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r222_DummyDoorProc, 0, 1);
    SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) asl_wait, 0, 1);
    SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) asl_chase, 0, 1);
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) toroko_go, 0, 1);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) toroko_go, (void*) 1, 1);
    if (SysFlagChk(pG, SYS_LOAD_GAME) == 0 && pG->room_id_prev == 0x210) {
        if (pG->Part == 1) {
            SceExec(0x12, (TaskFunc) toroko_ret, 0, 0, SCE_PRIO_DEF_2, 0);
        } else if (pG->Part == 2) {
            SceExec(0x12, (TaskFunc) toroko_ret, 1, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if (pSUB) {
        U16And(pSUB->atari.m_flag, 0xEFFF);
        BitOn16(pSUB->atari.m_flag, 0x2000);
    }
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r222_dai_go, 0, 1);
    SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) r222_dai_ret, 0, 1);
    SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r222_dai_set, 0, 1);
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r222_dai_set, 0, 1);
}

// Per frame: once Leon is past z -20000 (on the lift side) Ashley stops following (Scenario_flg[0] 0x80).
void R210Main()
{
    if (pPL->pos.z < -20000.0f) {
        SubCharCtrl(SCC_STOP, 0);
        ScfFlagOn(pG, SCF_NO_ASHLEY_DIST_CK);
    }
}

// Area 0: the door back to r222 takes Ashley away.
static void r222_DummyDoorProc()
{
    if (pSUB) {
        EmMgr.destroy(pSUB);
        StaFlagOff(pG, STA_SUB_ASHLEY);
    }
    SceAtDataReset(0);
    SceAtExecute(0);
}

// Areas 7/8: re-enable the lift areas 5/6; clears Room_flg[0] bit 31 (lift in use) and Scenario_flg[0] 0x80.
static void r222_dai_set()
{
    SceAtSetEnable(5, 1);
    SceAtSetEnable(6, 1);
    if (pG->Room_flg[0] & 0x80000000) {
        BitOff(pG->Room_flg[0], 0x80000000);
        ScfFlagOff(pG, SCF_NO_ASHLEY_DIST_CK);
    }
}

// Ashley's jump onto the lift (SetSubAux routine).
static void funcAshley2(cEm* p)
{
    if (p->r_no_2 == 0) {
        cAtariInfo* at = &pSUB->atari;

        at->throughOn();
        p->motionSet(ROOM_ARC_PTR(pGS->pRoom, 0x27), 0x2D, 0x2D, 1, 0);
        p->r_no_2 = 1;
        p->motSpeedRate = 0.2f;
    }
    if (p->motionMove()) {
        cAtariInfo* at;

        // Reference store: the `lwz pSUB` (fixed scalar) below depends on an unflagged MEM store but
        // not on an in-struct one, which is what ranks the 1.0 chain above the routine bytes.
        FSet(p->motSpeedRate, 1.0f);
        EmRoutineSet(p, 0, 0, 0, 0);
        at = &pSUB->atari;
        at->throughOff();
        SubCharCtrl(SCC_CHASE, 0);
    }
}

// Area 5: the lift goes down.
static void r222_dai_go()
{
    f32 dist = 2244.0f;
    f32 spd = 0.0f;
    Vec v;

    SceEventStart(0);
    pPL->setNoSuspend(1);
    if (pSUB) {
        cAtariInfo* at;

        pSUB->setNoSuspend(1);
        at = &pSUB->atari;
        at->throughOff();
        SubCharCtrl(SCC_STOP, 0);
        ScfFlagOn(pG, SCF_NO_ASHLEY_DIST_CK);
    }
    if (ScfFlagChk(pG, SCF_R213_ASHLEY_LOST) == 0 && CheckDoorJumpWithAshley() == 1) {
        CamCtrl.CutCall(5);
        SetPlDamage(0, plemRide);
        pPL->setNoSuspend(1);
        {
            // `cModel* m = pPL` at the block top: the pointer load leaves the store block and the two pool
            // constants' local-alloc order gives x f0 / z f13 (the st4_0 slide_move lever).
            cModel* m = pPL;

            v.x = -154.0f;
            v.z = -17194.0f;
            v.y = 0.0f;
            m->setPos(&v);
        }
        v.y = PI;
        v.x = 0.0f;
        v.z = 0.0f;
        pPL->setAng(&v);
        if (pSUB) {
            v.x = -549.0f;
            v.z = -14489.0f;
            v.y = 0.0f;
            pSUB->setPos(&v);
            v.x = 0.0f;
            v.y = 2.72f;
            v.z = 0.0f;
            pSUB->setAng(&v);
        }
        SetSubAux((int) funcAshley2, 0);
        SceSleep(10);
        SndCall(6, 5, &pPL->pos, 0, 0, 0);
        SceSleep(45);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        pSUB->motSpeedRate = 1.0f;
    }
    {
        cModel* m = pPL;

        v.x = -154.0f;
        v.z = -17194.0f;
        v.y = 0.0f;
        m->setPos(&v);
    }
    v.y = PI;
    v.x = 0.0f;
    v.z = 0.0f;
    pPL->setAng(&v);
    {
        cAtariInfo* at = &pPL->atari;

        at->throughOn();
    }
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 0);
    SmdGetObjPtr(0x20)->be_flag |= 0x20;
    SmdGetObjPtr(0x21)->be_flag |= 0x20;
    CamCtrl.CutCall(1);
    SndCall(6, 1, &SmdGetObjPtr(0x21)->pos, 0, 0x80000000, 0);
    v.x = 0.0f;  // x, z, y: the original issues y, z, x (the shared 0.0 register's stores bracket the z store)
    v.z = -17270.0f;
    v.y = 0.0f;
    pPL->setPos(&v);
    while (SmdGetObjPtr(0x21)->pos.z > r210_daiZGo) {
        f32 dz;

        spd += (r210_daiRotGo - spd) * 0.1f;
        dz = spd * dist;
        SmdGetObjPtr(0x20)->ang.x += spd;
        SmdGetObjPtr(0x21)->pos.z += dz;
        FAdd(SmdGetObjPtr(0x20)->pos.z, dz);
        pPL->pos.z += dz;
        SceSleep(1);
    }
    SndCall(6, 2, &SmdGetObjPtr(0x21)->pos, 0, 0x80000000, 0);
    {
        cAtariInfo* at = &pPL->atari;

        AtariOnRaw(at, 0x300);
    }
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 6: the lift comes back up.
static void r222_dai_ret()
{
    f32 dist = 2244.0f;
    f32 spd = 0.0f;
    Vec v;

    pG->Room_flg[0] |= 0x80000000;
    SceEventStart(0);
    pPL->setNoSuspend(1);
    if (pSUB) {
        cAtariInfo* at;

        pSUB->setNoSuspend(1);
        at = &pSUB->atari;
        at->throughOff();
        SubCharCtrl(SCC_STOP, 0);
        ScfFlagOn(pG, SCF_NO_ASHLEY_DIST_CK);
    }
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 0);
    SmdGetObjPtr(0x20)->be_flag |= 0x20;
    SmdGetObjPtr(0x21)->be_flag |= 0x20;
    CamCtrl.CutCall(2);
    SndCall(6, 1, &SmdGetObjPtr(0x21)->pos, 0, 0x80000000, 0);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = -31958.0f;
    pPL->setPos(&v);
    while (SmdGetObjPtr(0x21)->pos.z < r210_daiZRet) {
        f32 dz;

        spd += (r210_daiRotRet - spd) * 0.1f;
        dz = spd * dist;
        SmdGetObjPtr(0x20)->ang.x += spd;
        SmdGetObjPtr(0x21)->pos.z += dz;
        FAdd(SmdGetObjPtr(0x20)->pos.z, dz);
        pPL->pos.z += dz;
        SceSleep(1);
    }
    SndCall(6, 2, &SmdGetObjPtr(0x21)->pos, 0, 0, 0);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Areas 3/4: the cart ride to r212 (dir 0: left cart, 1: right cart).
static inline f32 FCRef(const f32& v) { return v; }
// Areas 3/4 (dir 0/1: left / right cart): refused with message 0x67 unless Ashley can jump with Leon;
// else the cart object is created, both board it (Leon's hand / weapon put away, Ashley's aux motion),
// stream 0xE4 plays and the cart rolls out to r212 (Part 1/2 tells r212 which cart).
static void toroko_go(int dir)
{
    cPlayer* pl = pPL;
    cObj* obj;
    Vec pos;

    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        return;
    }
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
    SmdGetObjPtr(0x1E)->be_flag &= ~2;
    SmdGetObjPtr(0x1F)->be_flag &= ~2;
    if (dir == 0) {
        pos = r210_torokoGoPos0;
    } else {
        pos = r210_torokoGoPos1;
    }
    obj->be_flag |= 0x20;
    {
        // the 0.0 volume is loaded AFTER the be_flag store (a pool constant would float above it)
        static const f32 vol = 0.0f;
        SndStrReq(1, 0xE4, 0x80000003, 0, 0, FCRef(vol));
    }
    SceEventStart(0);
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    SubCharCtrl(SCC_AUX_MOT, 0);
    {
        cPlayer* p = pPL;
        Vec* pp = &pos;
        Vec* zero = (Vec*) &vecZero;

        p->setPos(pp);
        p->setAng(zero);
        {
            cSubChar* sub = pSUB;

            if (sub) {
                sub->setPos(pp);
                sub->setAng(zero);
            }
        }
        obj->setPos(pp);
        obj->setAng(zero);
    }
    pPL->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x1F), 0, 0, 1, 0);
    if (pSUB) {
        MotionSetCore(pSUB, &pSUB->Motion, ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 1, 0);
    }
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 10, 0, 1, 0);
    SceSleep(115);
    SndCall(6, 3, &pPL->pos, 0, 0, 0);
    SndCall(6, 4, &pPL->pos, 0, 0, 0);
    SceSleep(50);
    FadeSetW(2, 15, 0, 0);
    SceSleep(15);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    SceEventEnd(0);
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    if (dir == 0) {
        SceAtDataReset(3);
        SceAtExecute(3);
    } else {
        SceAtDataReset(4);
        SceAtExecute(4);
    }
}

// Inline taking the angle by pointer: the caller's `&ang` becomes a hard-register argument set at each
// call (never a gcse occurrence), so it is recomputed `addi r4,r1,24` instead of sharing the template copy's
// address pseudo.
static inline void r210_setAng(cModel* m, Vec* a)
{
    m->setAng(a);
}

// The cart ride back from r212 (dir 0: left cart, 1: right cart).
static void toroko_ret(int dir)
{
    cPlayer* pl = pPL;
    cObj* obj;
    Vec pos;

    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        return;
    }
    obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
    Vec ang = {0.0f, -0.07f, 0.0f};
    SmdGetObjPtr(0x1E)->be_flag &= ~2;
    SmdGetObjPtr(0x1F)->be_flag &= ~2;
    if (dir == 0) {
        pos = r210_torokoRetPos0;
    } else {
        pos = r210_torokoRetPos1;
    }
    obj->be_flag |= 0x20;
    SceEventStart(0);
    SndStrReq(1, 0xE5, 0x80000003, 0, 0, 0.0f);
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    SceSleep(1);
    SubCharCtrl(SCC_AUX_MOT, 0);
    {
        cPlayer* p = pPL;
        Vec* pp = &pos;

        p->setPos(pp);
        r210_setAng(p, &ang);
        {
            cSubChar* sub = pSUB;

            if (sub) {
                sub->setPos(pp);
                r210_setAng(sub, &ang);
            }
        }
        obj->setPos(pp);
        r210_setAng(obj, &ang);
    }
    pPL->setNoSuspend(1);
    if (pSUB) {
        pSUB->setNoSuspend(1);
    }
    MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x24), 0, 0, 1, 0);
    if (pSUB) {
        MotionSetCore(pSUB, &pSUB->Motion, ROOM_ARC_PTR(pG->pRoom, 0x25), 0, 0, 1, 0);
    }
    obj->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 10, 0, 1, 0);
    SceSleep(20);
    SndCall(6, 3, &pPL->pos, 0, 0, 0);
    SndCall(6, 4, &pPL->pos, 0, 0, 0);
    SceSleep(140);
    pPL->setNoSuspend(0);
    if (pSUB) {
        pSUB->setNoSuspend(0);
    }
    SubCharCtrl(SCC_CHASE, 1);
    ObjMgr.destroy(obj);
    SmdGetObjPtr(0x1E)->be_flag |= 2;
    SmdGetObjPtr(0x1F)->be_flag |= 2;
    SceEventEnd(0);
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    if (dir == 0) {
        Vec p;
        Vec* pp = &p;
        f32 y;

        {
            cPlayer* p1 = pPL;

            p.x = -19403.0f;
            y = -2000.0f;
            pp->y = y;
            pp->z = -8345.0f;
            p1->setPos(pp);
        }
        {
            cPlayer* p1 = pPL;

            p.x = 0.0f;
            pp->y = 1.7f;
            p.z = 0.0f;
            p1->setAng(pp);
        }
        if (pSUB) {
            p.x = -19697.0f;
            pp->y = y;
            pp->z = -7950.0f;
            pSUB->setPos(pp);
            {
                cSubChar* s = pSUB;

                p.x = 0.0f;
                pp->y = 1.75f;
                p.z = 0.0f;
                s->setAng(pp);
            }
        }
    } else {
        Vec p;
        Vec* pp = &p;
        f32 y;

        {
            cPlayer* p1 = pPL;

            p.x = -61000.0f;
            y = -2000.0f;
            pp->y = y;
            pp->z = 246652.0f;
            p1->setPos(pp);
        }
        {
            cPlayer* p1 = pPL;

            p.x = 0.0f;
            pp->y = 1.67f;
            p.z = 0.0f;
            p1->setAng(pp);
        }
        if (pSUB) {
            p.x = -61850.0f;
            pp->y = y;
            pp->z = 247150.0f;
            pSUB->setPos(pp);
            {
                cSubChar* s = pSUB;

                p.x = 0.0f;
                pp->y = 1.83f;
                p.z = 0.0f;
                s->setAng(pp);
            }
        }
    }
}

// Leon's ride motion on the lift (SetPlDamage routine).
static void plemRide(cPlayer* pl)
{
    switch (pl->r_no_2) {
    case 0:
        pPL->setNoSuspend(1);
        MotionSetCore(pPL, &pPL->Motion, pl->m_MotTbl[11], 0, 0, 0x201, 0);
        pl->r_no_2++;
        pl->r_no_3 = 0;
    case 1:
        pl->r_no_3++;
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
}

// The module's .data continues 8-aligned.
asm(".section .data; .balign 8");
