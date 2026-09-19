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
#include "obj00.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emwindow.h"
#include "etc_model.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "cam_ctrl.h"
#include "read.h"
#include "cSceObj.h"
#include "math_sub.h"

// Room 3-09 (D:/Bio4/Prog/r309.cpp): the regenerator's appearance event (its glow effect object), the
// shelf, the stream and the BGM table rewrite once the key item is taken.

struct R309Work {
    u32 kind0;     // 0x00  EspPullCoreKind of the glow on the enemy
    u32 kind1;     // 0x04  EspPullCoreKind of the appearance flash
    cEmWrap em;    // 0x08
};

static R309Work* r309_work;

// Death bit of entry `no` of the loaded enemy list (0 while no list is loaded).
static inline u32 r309_emDead(int no)
{
    int list = pG->em_list_no;
    u32 v;

    if (list >= 0) {
        v = *(u32*) ((list << 5) + (u32) pG + 0x501C + (((u32) no >> 5) << 2)) & (0x80000000 >> (no & 31));
    } else {
        v = 0;
    }
    return v;
}

static void r309_checkBgmTblRewrite();
static void r309_execEmAppear_end();
static void r309_execEmAppear();
static void r309_setEffOnEm(s16 no);
void r309_openShelf_main(int no, int mode);
static void r309_openedShelf(int no);
static void r309_openShelf(int no);
static void r309_checkBgm();

// Room init: until the regenerator appeared (Room_flg bit 1) enemy 0x36 is pre-read, area 3 = the
// appearance event and window 0x1C takes no damage; afterwards the glow effect rides on the regenerator
// (0x34) while it lives. The stream watcher, two shelf item events, the BGM table rewrite once (bit 4).
void R309Init()
{
    cEm* win;

#line 40 "D:/Bio4/Prog/r309.cpp"
    r309_work = (R309Work*) MEM_CALLOC(sizeof(R309Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        EmReadSearch(0x36, 0, 0);
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r309_execEmAppear, 0, 1);
        if (getRoomEtcWindow(0x1C, &win, 1)) {
            ((cEmWindow*) win)->SetEnableDamage(0);
        }
    } else {
        if (r309_emDead(0x34) == 0) {
            SceExec(0x12, (TaskFunc) r309_setEffOnEm, 0x34, 0, 2, 0);
        } else {
            EmListSetAlive(0x34, 0);
        }
    }
    SceExec(0x12, (TaskFunc) r309_checkBgm, 0, 0, 2, 0);
    SceSetItemEvent(2, 0x81, 0, 4, r309_openShelf, (void (*)()) r309_openedShelf, 0, 0);
    SceSetItemEvent(4, 0x85, 3, 5, r309_openShelf, (void (*)()) r309_openedShelf, 1, 0);
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceExec(0x12, (TaskFunc) r309_checkBgmTblRewrite, 0, 0, 2, 0);
    }
}

// Per frame: once the key item (Item_flg[0] 0x40) is taken and item 0x83 is no longer saved, set Scenario_flg[2] 0x00040000.
void R309Main()
{
    if (!ScfFlagChk(pG, SCF_R309_GET_KEY) && (ItfFlagChk(pG, ITF_R309_KEY)) && SceAtCheckSaveItemId(0x83) == 0) {
        ScfFlagOn(pG, SCF_R309_GET_KEY);
    }
}

// Once the key item is taken the neighbouring rooms switch to the battle BGM tables.
static void r309_checkBgmTblRewrite()
{
    // The work sits inside the poll's arm (a `return` ends it): the arm's pG high is a fresh `lis`,
    // not the loop's hoisted r31 (a post-loop block gets the loop's high through cse's around-path).
    while (1) {
        if (ItfFlagChk(pG, ITF_R309_KEY)) {
            RsfSet(G_ROOM_ID, 4);
            SndBgmTblSet(0x30E, 1);
            SndBgmTblSet(0x30B, 1);
            SndBgmTblSet(0x30C, 1);
            return;
        }
        SceSleep(1);
    }
}

// End of the appearance event (also its cancel path): the regenerator set alerted with its glow if the
// event had not, ESL 0x34 rewritten from 0x66 and marked alive, the flash effect dropped, SceEventEnd,
// window 0x1C breakable again.
static void r309_execEmAppear_end()
{
    cEm* win;

    if (r309_work->em.isAlive() == 0) {
        r309_work->em.setEm(0x34, -1, 1, 1, 1);
        r309_work->em.setFlag(1);
        SceExec(0x12, (TaskFunc) r309_setEffOnEm, 0x34, 0, 2, 0);
    } else {
        r309_work->em.setNoSuspend(0);
    }
    *EM_LIST(0x34) = *EM_LIST(0x66);
    EmListSetAlive(0x34, 1);
    EffectEspDelete(0, (u8) r309_work->kind1, 0, 0);
    EffectEspgenDelete(0, (u8) r309_work->kind1, 0);
    EffectEfmDelete(0, (u8) r309_work->kind1, 0);
    SceEventEnd(0);
    if (getRoomEtcWindow(0x1C, &win, 1)) {
        ((cEmWindow*) win)->SetEnableDamage(1);
    }
}

// Area 3: the regenerator appears in a flash while the camera shows it.
static void r309_execEmAppear()
{
    RsfSet(G_ROOM_ID, 1);
    r309_work->kind1 = EspPullCoreKind();
    SceSetEventCancel(1, (TaskFunc) r309_execEmAppear_end, 0, -1, 1);
    SceEventStart(0);
    CamCtrl.CutCall(1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    EstSet(0, -1, 0, 0, 1, 0, 1, (u8) r309_work->kind1, 0, 0);
    SndRoomStrStart(1, 0, 1);
    CamCtrl.CutCall(2);
    SceSleep(45);
    r309_work->em.setEm(0x34, -1, 1, 1, 1);
    r309_work->em.setFlag(1);
    r309_work->em.setNoSuspend(1);
    SceExec(0x12, (TaskFunc) r309_setEffOnEm, 0x34, 0, 2, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(3);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r309_execEmAppear_end();
}

// The glow object attached to enemy `no` while it is active.
static void r309_setEffOnEm(s16 no)
{
    SceSleep(1);
    cEmWrap em;
    em.setPtr(no, -1, 1);
    if (em.isActive() == 1) {
        Vec ofs = {0.0f, 0.0f, 200.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        cObj* obj;

        obj = SetObj00(ROOM_ARC_PTR(pG->pArc, 8), ROOM_ARC_PTR(pG->pArc, 9), &ofs, &rot);
        OyaSetObj00(obj, em.getPtr(), 0x11);
        obj->setNoSuspend(1);
        U32Set(r309_work->kind0, EspPullCoreKind());
        EstSet((int) obj, -1, 0, 0, 0, 0x2D, 0xC01, (u8) r309_work->kind0, 0, 0);
        while (em.isActive() == 1) {
            SceSleep(1);
        }
        ObjMgr.destroy(obj);
        EffectEspDelete(0, (u8) r309_work->kind0, 0, 0);
        EffectEspgenDelete(0, (u8) r309_work->kind0, 0);
        EffectEfmDelete(0, (u8) r309_work->kind0, 0);
        EmListSetAlive(0x34, 0);
    }
}

// Shelf 0: the lid is parented to the box and swings open; shelf 1 is a plain box.
void r309_openShelf_main(int no, int mode)
{
    // The NULL inits precede the mover: cse feeds the constructor's byte and word zero stores from
    // the SImode `lid` zero (one callee-saved register) instead of two constant pseudos.
    cObj* box = NULL;
    cObj* lid = NULL;
    cSceObj mov;

    switch (no) {
    case 0:
        box = SmdGetObjPtr(0xC8);
        lid = SmdGetObjPtr(0xC9);
        if (box && lid) {
            Vec d;

            box->be_flag |= 0x20;
            lid->be_flag |= 0x20;
            Vec rot = {0.0f, 0.0f, 0.0f};
            PSVECSubtract(&lid->pos, &box->pos, &d);
            lid->setParent(box, &d, &rot);
            if (mode == 0) {
                Vec ang = {-PI, 0.0f, 0.0f};

                mov.initMove1_ang(lid, 40, &ang, 20.0f, 20.0f, 4);
                SndCall(6, 0, 0, 0, 0, 0);
                while (mov.move() == 1) {
                    SceSleep(1);
                }
                SceSleep(15);
            }
        }
        OpenBoxMain(2, mode, 1, 0xC8, -1, -1);
        break;
    case 1:
        OpenBoxMain(9, mode, 0x18, 0x30, -1, -1);
        break;
    }
}

// Item-event "already opened": shelf `no` posed open.
static void r309_openedShelf(int no)
{
    r309_openShelf_main(no, 1);
}

// Item-event opener: animate shelf `no` open.
static void r309_openShelf(int no)
{
    r309_openShelf_main(no, 0);
}

// The stream starts when the player is found and stops when every enemy is dead.
static void r309_checkBgm()
{
    SceSleep(30);
    while (SceCkFindPL(0) == 0) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x36, -1) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}
