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
#include "st_mgr_event.h"
#include "mes.h"
#include "motion.h"
#include "snd.h"
#include "fade.h"

// Room 2-19 (D:/Bio4/Prog/r219.cpp): the mine cart (toroko) platform: areas 2 / 3 board the cart from
// the near / far platform and ride out (toroko_go), a return from r219 itself (pG->Part 1 / 2) arrives
// by cart (toroko_ret). Ashley is placed on the cart with Leon. One shelf item event.

struct R219Work {
    u8 dummy;
};

static R219Work* r219_work;

static Vec r219_go_pos0 = {0.0f, -200.0f, -9782.0f};
static Vec r219_go_pos1 = {-81480.38f, -200.0f, -351366.0f};
static Vec r219_ret_pos0 = {-81575.8f, -200.0f, -332041.94f};
static Vec r219_ret_pos1 = {-19665.783f, -200.0f, -9782.0f};

static void toroko_go(int dir);
static void toroko_ret(int dir);
void r219_openShelf_main(int no, int mode);
static void r219_openShelf(int no);
static void r219_openedShelf(int no);

// Room init (the mine cart): System_flg 0x800 off; a fresh entry marks Ashley as following; the water
// object's refraction; areas 2/3 = ride out from the near / far platform; coming back from r219 itself
// (Part 1 / 2) arrive at the far / near platform; BGM when arriving from r201 or at the near platform;
// one shelf item event.
void R219Init()
{
    SysFlagOff(pG, SYS_SCISSOR_ON);
#line 48 "D:/Bio4/Prog/r219.cpp"
    r219_work = (R219Work*) MEM_CALLOC(sizeof(R219Work), 1, 0xd);
    if (pG->room_id_prev == 0xFFF) {
        if (!StaFlagChk(pG, STA_SUB_ASHLEY)) {
            StaFlagOn(pG, STA_SUB_ASHLEY);
        }
    }
    SmdGetObjPtr(0x27)->Shader_type = 2;
    SmdGetObjPtr(0x27)->Refract_pow = 0x10;
    SmdGetObjPtr(0x27)->Refract_ratio = 0x40;
    pPL->ot_type = 1;
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) toroko_go, 0, 1);
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) toroko_go, (void*) 1, 1);
    if (pG->room_id_prev == 0x219) {
        if (pG->Part == 1) {
            SceExec(0x12, (TaskFunc) toroko_ret, 0, 0, SCE_PRIO_DEF_2, 0);
        } else if (pG->Part == 2) {
            SceExec(0x12, (TaskFunc) toroko_ret, 1, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if ((pG->room_id_prev == 0x219 && pG->Part == 2) || pG->room_id_prev == 0x201) {
        SndRoomBgmStart(0, 0);
    }
    SceSetItemEvent(4, 0x84, 1, 3, r219_openShelf, r219_openedShelf, 0, 0);
}

// Place a model on the cart (an inline: the `&ang` arguments are recomputed per call, `&pos` shared).
static inline void torokoPlace(cModel* m, Vec* pos, Vec* ang)
{
    m->setPos(pos);
    m->setAng(ang);
}

// Fill a Vec as a Y-only rotation and return it.
static inline Vec* AngSetY(Vec* v, f32 y)
{
    v->x = 0.0f;
    v->y = y;
    v->z = 0.0f;
    return v;
}

// The cart ride out (dir 0: from the near platform, 1: from the far one).
static void toroko_go(int dir)
{
    cPlayer* pl = pPL;

    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
        return;
    }
    {
        Vec pos;
        cObj* obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
        Vec ang0 = {0.0f, 0.0f, 0.0f};
        Vec ang1 = {0.0f, 1.5707964f, 0.0f};
        Vec ang;

        SmdGetObjPtr(0x18)->be_flag &= ~2;
        SmdGetObjPtr(0x19)->be_flag &= ~2;
        if (dir == 0) {
            pos = r219_go_pos0;
            ang = ang0;
        } else {
            pos = r219_go_pos1;
            ang = ang1;
        }
        obj->be_flag |= 0x20;
        SceEventStart(0);
        SndStrReq(1, 0xE4, 0x80000003, 0, 0, 0.0f);
        pl->setRightHand(1);
        pl->Wep->setTrans(0, 0);
        PlSetHand(1, 0);
        SubCharCtrl(SCC_AUX_MOT, 0);
        if (pSUB) {
            pSUB->beginEvent(0);
        }
        torokoPlace(pPL, &pos, &ang);
        if (pSUB) {
            torokoPlace(pSUB, &pos, &ang);
        }
        torokoPlace(obj, &pos, &ang);
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
            SceAtDataReset(2);
            SceAtExecute(2);
        } else {
            SceAtDataReset(3);
            SceAtExecute(3);
        }
    }
}

// The cart ride back in (dir 0: arriving at the far platform, 1: at the near one).
static void toroko_ret(int dir)
{
    cPlayer* pl = pPL;

    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
        return;
    }
    {
        Vec pos;
        cObj* obj = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
        Vec ang0 = {0.0f, 1.5007963f, 0.0f};
        Vec ang1 = {0.0f, -0.07f, 0.0f};
        Vec ang;

        SmdGetObjPtr(0x18)->be_flag &= ~2;
        SmdGetObjPtr(0x19)->be_flag &= ~2;
        if (dir == 0) {
            pos = r219_ret_pos0;
            ang = ang0;
        } else {
            pos = r219_ret_pos1;
            ang = ang1;
        }
        obj->be_flag |= 0x20;
        SceEventStart(0);
        SndStrReq(1, 0xE5, 0x80000003, 0, 0, 0.0f);
        pl->setRightHand(1);
        pl->Wep->setTrans(0, 0);
        PlSetHand(1, 0);
        SceSleep(1);
        SubCharCtrl(SCC_AUX_MOT, 0);
        if (pSUB) {
            pSUB->beginEvent(0);
        }
        torokoPlace(pPL, &pos, &ang);
        if (pSUB) {
            torokoPlace(pSUB, &pos, &ang);
        }
        torokoPlace(obj, &pos, &ang);
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
            pSUB->endEvent(0);
        }
        SubCharCtrl(SCC_CHASE, 1);
        ObjMgr.destroy(obj);
        SmdGetObjPtr(0x18)->be_flag |= 2;
        SmdGetObjPtr(0x19)->be_flag |= 2;
        SceEventEnd(0);
        PlSetHand(0, 0);
        pl->setRightHand(1);
        pl->Wep->setTrans(1, 0);
        if (dir == 0) {
            Vec v;

            pPL->setPos(VecSet(&v, -78700.0f, 0.0f, -351315.0f));
            pPL->setAng(AngSetY(&v, 3.02f));
            if (pSUB) {
                pSUB->setPos(VecSet(&v, -79287.0f, 0.0f, -351366.0f));
                pSUB->setAng(AngSetY(&v, -3.02f));
            }
        } else {
            Vec v;

            pPL->setPos(VecSet(&v, 0.0f, 0.0f, -6314.0f));
            pPL->setAng(AngSetY(&v, 0.0f));
            if (pSUB) {
                pSUB->setPos(VecSet(&v, -593.0f, 0.0f, -7130.0f));
                pSUB->setAng(AngSetY(&v, 0.3f));
            }
        }
    }
}

// Per-frame room main: nothing.
void R219Main()
{
}

// The shelf (object 0x17) falls open (OpenBoxFall type 0x15, mode 0 animate / 1 snap) and is then
// pinned at its fallen position / rotation.
void r219_openShelf_main(int no, int mode)
{
    cObj* obj;

    OpenBoxMain(OpenBoxFall, mode, 0x15, 0x17, -1, -1);
    obj = SmdGetObjPtr(0x17);
    if (obj) {
        Vec* pos = &obj->pos;
        Vec* rot = &obj->ang;

        obj->pos.x = -85522.0f;
        obj->pos.y = 45.0f;
        obj->pos.z = -353737.0f;
        obj->ang.x = 1.28598f;
        obj->ang.y = -0.072f;
        obj->ang.z = 1.57173f;
        obj->pList->ang.x = 0.0f;
        obj->pList->ang.y = 0.0f;
        obj->pList->ang.z = 0.0f;
        obj->setPos(pos);
        obj->setAng(rot);
    }
}

// Item-event opener: the shelf falls.
static void r219_openShelf(int no)
{
    r219_openShelf_main(no, 0);
}

// Item-event "already opened": the shelf posed fallen.
static void r219_openedShelf(int no)
{
    r219_openShelf_main(no, 1);
}
