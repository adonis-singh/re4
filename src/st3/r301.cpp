#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "mes.h"
#include "game.h"
#include "cam_ctrl.h"
#include "math_sub.h"
#include "area.h"
#include "TexRender.h"
#include "cSceObj.h"
#include "db_log.h"

// Room 3-01 (D:/Bio4/Prog/r301.cpp): the water render target, the enemy waves of the beach (with the
// bowgun and rocket launcher Ganados), the rock wall, the shelf and the continue point (the rock that
// slides open).

struct R301Work {
    TexRenderMng* tex;   // 0x000
    cSceObj rock;        // 0x004  continue point rock
    u32 espKind;         // 0x0FC  effect kind of the rock slide dust
    u32 sndId;           // 0x100  SndCall handle of the slide sound
};

// The work pointer is a struct member: every store through the work reloads it.
struct R301WorkPtr {
    R301Work* p;
};

static u8 r301_texTbl[0x20];
static R301WorkPtr r301_work;

// Hit effects of attribute type 2 (water).
static const AtEffInfo r301_eff_info = {1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F}};

// COMPILER-DIFF: #4 (int table entries reach the s16 parameter untruncated)
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

static void r301_checkBgm();
static void r301_execContinuePoint_end();
static void r301_execContinuePoint();
void r301_initContinuePoint();
static void r301_setRocketLauncher_sub();
static void r301_setRocketLauncher();
static void r301_setEmBowgun2();
static void r301_checkEmSetStart();
static void r301_checkEmReset1();
static void r301_checkEmReset2();
static void r301_checkEmReset3();
static void r301_setEmBowgun();
static void r301_checkRockWall();
void r301_openShelf_main(int no, int mode);
static void r301_openShelf(int no);
static void r301_openedShelf(int no);
static void setTexRender();

// Room init (the beach): Debug_flg[1] 0x40000, water hit effects, the first-Ganado watcher; area 3 = the
// bowgun Ganados until Room_flg bit 1, area 5 = the rocket launcher Ganado until bit 3; the water render
// target, the player's room motions (the rock climb); the rock wall until bit 0; the continue-point rock;
// the shelf item event; the battle stream.
void R301Init()
{
    cObj* obj;
    R301Work*& wp = r301_work.p;   // the address is computed before the call

    DbgFlagOn(pG, DBG_CAST_ERR_NO_DISP);
#line 63 "D:/Bio4/Prog/r301.cpp"
    wp = (R301Work*) MEM_CALLOC(sizeof(R301Work), 1, 0xd);
    EatMgr.registEffInfo(2, (AtEffInfo*) &r301_eff_info);
    SceExec(0x12, (TaskFunc) r301_checkEmSetStart, 0, 0, 2, 0);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r301_setEmBowgun, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r301_setRocketLauncher, 0, 1);
    }
    setTexRender();
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0,
                   ROOM_ARC_PTR(pG->pRoom, 0x22), ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24),
                   ROOM_ARC_PTR(pG->pRoom, 0x25));
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceExec(0x12, (TaskFunc) r301_checkRockWall, 0, 0, 2, 0);
    } else {
        SceAtSetEnable(2, 0);
        obj = SmdGetObjPtr(0x2D);
        if (obj) {
            obj->be_flag &= ~2;
        }
    }
    SceSetItemEvent(0xA, 0x80, 4, 5, r301_openShelf, (void (*)()) r301_openedShelf, 0, 0);
    EspDataLoad((u32) ROOM_ARC_PTR(pG->pRoom, 0x21), 0xCA, 0);
    r301_initContinuePoint();
    SceExec(0x12, (TaskFunc) r301_checkBgm, 0, 0, 2, 0);
}

// Per-frame room main: nothing.
void R301Main()
{
}

// Battle music while enemies are alive and the player has been found.
static void r301_checkBgm()
{
    for (;;) {
        while (SceCkFindPL(0) == 0) {
            SceSleep(1);
        }
        SndRoomStrStart(1, 3, 1);
        while (SceCountEmAlive(0x10, 0x20) != 0) {
            SceSleep(1);
        }
        SndRoomStrStop(3);
        SceSleep(90);
    }
}

// End of the continue point event (also the event cancel handler).
static void r301_execContinuePoint_end()
{
    if (pG->Room_flg[0] & 0x40000000) {
        EffectEspDelete(0, (u8) r301_work.p->espKind, 0, 0);
        EffectEspgenDelete(0, (u8) r301_work.p->espKind, 0);
        EffectEfmDelete(0, (u8) r301_work.p->espKind, 0);
        if (r301_work.p->sndId) {
            SndStop(r301_work.p->sndId, 0);
        }
        r301_work.p->rock.setEndPos();
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(0x10, 0);
    SceAtSetEnable(0x11, 0);
    GameSaveSave(&GameSave, pSaveData, -1);
}

// The continue point: ask, then slide the rock away with a dust effect and save.
static void r301_execContinuePoint()
{
    SceMesSet(1, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case -1:
    case 0:
    case 2:
        break;
    case 1:
    default: {
        u32 zero = 0;

        RsfSet(G_ROOM_ID, 5);
        pG->Key_flg[0] |= 0x40;
        SceAtSetEnable(0xF, 0);
        r301_work.p->espKind = 0;
        r301_work.p->sndId = 0;
        SceSetEventCancel(1, (TaskFunc) r301_execContinuePoint_end, 0, 1, 1);
        SceEventStart(0);
        CamCtrl.CutCall(6);
        r301_work.p->espKind = EspPullCoreKind();
        EstSet(0, -1, 0, 0, 1, 4, 1, (u8) r301_work.p->espKind, zero, (void*) zero);
        SndCall(6, 5, 0, 0, 0, 0);
        SceSleep(15);
        r301_work.p->sndId = SndCall(6, 6, 0, 0, 0, 0);
        while (r301_work.p->rock.move() == 1) {
            SceSleep(1);
        }
        SndCall(6, 7, 0, 0, 0, 0);
        r301_work.p->sndId = 0;
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSleep(15);
        SceSetEventCancel(0, 0, 0, -1, 1);
        r301_execContinuePoint_end();
        break;
    }
    }
}

// The continue-point rock (object 0x3C): a 90-frame move1 of 3270 up with a shake; area 0xF = the
// continue prompt until used (Room_flg bit 5), else posed open with areas 0x10/0x11 off.
void r301_initContinuePoint()
{
    cObj* obj = SmdGetObjPtr(0x3C);

    if (obj) {
        Vec d = {0.0f, 3270.0f, 0.0f};

        r301_work.p->rock.initMove1_pos(obj, 90, &d, 20.0f, 0.0f);
        r301_work.p->rock.setVibration(5, 5, 1.0f, 0.4f, 2.0f);
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) r301_execContinuePoint, 0, 1);
        } else {
            r301_work.p->rock.setEndPos();
            SceAtSetEnable(0x10, 0);
            SceAtSetEnable(0x11, 0);
        }
    }
}

// After the rocket launcher: the first reset wave 90 frames later, the second 300 frames after that.
static void r301_setRocketLauncher_sub()
{
    SceSleep(90);
    SceExec(0x12, (TaskFunc) r301_checkEmReset1, 0, 0, 2, 0);
    SceSleep(300);
    SceExec(0x12, (TaskFunc) r301_checkEmReset2, 0, 0, 2, 0);
}

// The rocket launcher Ganado: fires once he sees the player, then the waves of the reset tasks follow.
static void r301_setRocketLauncher()
{
    RsfSet(G_ROOM_ID, 3);
    {
    cEmWrap em;
    Vec a;
    Vec b;

    em.setPtr(0x11, -1, 0);
    SceSleep(1);
    while (em.isActive()) {
        a = pPL->pos;
        em.getPos(&b);
        a.y += 1700.0f;
        b.y += 1700.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            cModel* pl = pPL;

            if (Front_check(pl, em.getPtr(), PI / 6.0f)) {
                break;
            }
        }
        SceSleep(1);
    }
    em.setFlag(1);
    SceExec(0x12, (TaskFunc) r301_setRocketLauncher_sub, 0, 0, 2, 0);
    for (;;) {
        if (em.isActive() == 0) {
            SceExit();
        }
        if (em.getPtr() && ((cEmGanado*) em.getPtr())->ckWeapon() == 0) {
            break;
        }
        SceSleep(1);
    }
    AreaGetCenterPos(&a, &SceAtPtr(0xE)->area);
    em.setGoto(&a, 1);
    while (em.ckGoto()) {
        SceSleep(1);
    }
    em.clearFindPL();
    a.x = 10363.0f;
    a.y = 0.0f;
    a.z = -40259.0f;
    em.setGoto(&pPL->pos, 8);
    }
}

// The second bowgun group: Ganado 0x12 alerted at once; 3 seconds later 0x22 and 0xF walk to areas 6 / 8
// and turn hostile on arrival.
static void r301_setEmBowgun2()
{
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    Vec pos;
    int f1 = 0;
    int f2 = 0;

    em0.setPtr(0x12, -1, 0);
    em0.setFindPL();
    SceSleep(180);
    em1.setPtr(0x22, -1, 0);
    em1.setFindPL();
    em2.setPtr(0xF, -1, 0);
    em2.setFindPL();
    AreaGetCenterPos(&pos, &SceAtPtr(6)->area);
    em1.setGoto(&pos, 0xB);
    AreaGetCenterPos(&pos, &SceAtPtr(8)->area);
    em2.setGoto(&pos, 0xB);
    for (;;) {
        if (f1 == 0 && em1.ckGoto() == 0) {
            em1.setFindPL();
            f1 = 1;
        }
        if (f2 == 0 && em2.ckGoto() == 0) {
            em2.setFindPL();
            f2 = 1;
        }
        if (f1 == 1 && f2 == 1) {
            break;
        }
        SceSleep(1);
    }
}

// The first Ganado of the beach: walks over once he sees the player, then the bowgun Ganados come.
static void r301_checkEmSetStart()
{
    cEmWrap em;
    Vec a;
    Vec b;

    em.setEm(0x16, -1, 0, 1, 1);
    SceSleep(1);
    if (em.isActive() == 1) {
        for (;;) {
            if (em.isActive() == 0) {
                goto out;
            }
            if (em.ckFindPL() == 1) {
                a = pPL->pos;
                em.getPos(&b);
                a.y += 1700.0f;
                b.y += 1700.0f;
                if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
                    goto found;
                }
            }
            SceSleep(1);
        }
        // Reached by goto (a `break` to the loop end would rotate the loop); the `&a` below is PRE's
        // copy (`mr r29, r31`) of the loop's hoisted struct-copy address.
    found:
        AreaGetCenterPos(&a, &SceAtPtr(4)->area);
        em.setGoto(&a, 1);
    }
out:
    SceSleep(90);
    SceExec(0x12, (TaskFunc) r301_setEmBowgun2, 0, 0, 2, 0);
    if (em.isActive() == 1) {
        // The exit test is rotated to the bottom and the goto-out arm stays outside the loop: its pPL
        // high is a fresh `lis` at the use, not a hoisted callee-saved one.
        for (;;) {
            if (em.isActive() == 0) {
                goto skip;
            }
            if (em.ckGoto() == 0) {
                break;
            }
            SceSleep(1);
        }
        em.setGoto(&pPL->pos, 8);
    }
skip:
    SceSleep(300);
    SceExec(0x12, (TaskFunc) r301_checkEmReset3, 0, 0, 2, 0);
}

// Replacements for the two Ganados of the pier once they are gone.
static void r301_checkEmReset1()
{
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    Vec pos;
    int f0 = 0;
    int f1 = 0;
    int f2 = 0;

    em0.setPtr(0x17, -1, 0);
    em1.setPtr(0x18, -1, 0);
    em0.setFlag(1);
    em1.setFlag(1);
    SceSleep(10);
    AreaGetCenterPos(&pos, &SceAtPtr(0xB)->area);
    em0.setGoto(&pos, 1);
    AreaGetCenterPos(&pos, &SceAtPtr(7)->area);
    em1.setGoto(&pos, 0xB);
    {
        int ids[4] = {0x1A, 0x1B, 0x1E, 0x1F};

        for (;;) {
            if ((em0.isActive() == 0 && em1.isActive() == 0) || SceAtHitCheck(0xD) == 1) {
                break;
            }
            SceSleep(1);
        }
        pG->Room_flg[0] |= 0x80000000;
        {
            cEmWrap em3;

            for (;;) {
                if (f0 == 0 && em0.isActive() == 0) {
                    cEmWrapSetEmI(&em2, ids[0], -1, 0, 1, 1);
                    f0 = 1;
                    em2.setFlag(1);
                    em2.setGoto(&pPL->pos, 0xB);
                    SceSleep(200);
                    cEmWrapSetEmI(&em3, ids[1], -1, 0, 1, 1);
                    em3.setFlag(1);
                    em3.setGoto(&pPL->pos, 0xB);
                }
                if (f1 == 0 && em1.isActive() == 0) {
                    f1 = 1;
                    SceSleep(200);
                    cEmWrapSetEmI(&em3, ids[2], -1, 0, 1, 1);
                    em3.setFlag(1);
                    em3.setGoto(&pPL->pos, 0xB);
                }
                if (f2 == 0 && em2.isActive() == 0) {
                    f2 = 1;
                    SceSleep(200);
                    cEmWrapSetEmI(&em3, ids[3], -1, 0, 1, 1);
                    em3.setFlag(1);
                    em3.setGoto(&pPL->pos, 0xB);
                }
                if (!(f0 == 1 && f1 == 1 && f2 == 1)) {
                    SceSleep(1);
                } else {
                    break;
                }
            }
        }
    }
}

// Reset wave 2 (after Room_flg[0] bit 31): when Ganado 0x11 / 0x12 die, the replacements 0x13 / 0x1C /
// 0x1D walk in (goto 0xB) to their posts and then after the player.
static void r301_checkEmReset2()
{
    while (!(pG->Room_flg[0] & 0x80000000)) {
        SceSleep(1);
    }
    {
        cEmWrap em0;
        cEmWrap em1;
        int f0 = 0;
        int f1 = 0;

        em0.setPtr(0x11, -1, 0);
        em1.setPtr(0x12, -1, 0);
        {
            int ids[3] = {0x13, 0x1C, 0x1D};

            SceSleep(1);
            {
                cEmWrap em2;

                for (;;) {
                    // The flag is set at the arm's end: the block's last insn is then the flag's `li`,
                    // and the setGoto call is a dependent of the setEm `li`s but not of the `mr r3`
                    // this copy, which is then scheduled last.
                    if (f0 == 0 && em0.isActive() == 0) {
                        SceSleep(300);
                        cEmWrapSetEmI(&em2, ids[0], -1, 0, 1, 1);
                        em2.setGoto(&pPL->pos, 0xB);
                        f0 = 1;
                    }
                    if (f1 == 0 && em1.isActive() == 0) {
                        SceSleep(300);
                        cEmWrapSetEmI(&em2, ids[1], -1, 0, 1, 1);
                        em2.setGoto(&pPL->pos, 0xB);
                        SceSleep(300);
                        cEmWrapSetEmI(&em2, ids[2], -1, 0, 1, 1);
                        em2.setGoto(&pPL->pos, 0xB);
                        f1 = 1;
                    }
                    if (!(f0 == 1 && f1 == 1)) {
                        SceSleep(1);
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

// Reset wave 3 (after Room_flg[0] bit 31): when Ganado 0xF dies, 5 seconds later 0x10 / 0x15 walk in to its post.
static void r301_checkEmReset3()
{
    while (!(pG->Room_flg[0] & 0x80000000)) {
        SceSleep(1);
    }
    {
        cEmWrap em0;
        int f = 0;

        em0.setPtr(0xF, -1, 0);
        {
            int ids[2] = {0x10, 0x15};

            SceSleep(1);
            {
            cEmWrap em1;
            cEmWrap em2;
            Vec pos;

            for (;;) {
                if (f == 0 && em0.isActive() == 0) {
                    SceSleep(300);
                    AreaGetCenterPos(&pos, &SceAtPtr(0xC)->area);
                    while (SceAtHitCheck(0x13) == 1) {
                        SceSleep(1);
                    }
                    cEmWrapSetEmI(&em1, ids[0], -1, 0, 1, 1);
                    em1.setGoto(&pos, 0xB);
                    SceSleep(30);
                    while (SceAtHitCheck(0x13) == 1) {
                        SceSleep(1);
                    }
                    cEmWrapSetEmI(&em2, ids[1], -1, 0, 1, 1);
                    em2.setGoto(&pos, 0xB);
                    f = 1;
                }
                if (f != 1) {
                    SceSleep(1);
                } else {
                    break;
                }
            }
            {
                int f1 = 0;
                int f2 = 0;

                for (;;) {
                    if (f1 == 0 && em1.ckGoto() == 0) {
                        em1.setGoto(&pPL->pos, 0xB);
                        f1 = 1;
                    }
                    if (f2 == 0 && em2.ckGoto() == 0) {
                        em2.setGoto(&pPL->pos, 0xB);
                        f2 = 1;
                    }
                    if (!(f1 == 1 && f2 == 1)) {
                        SceSleep(1);
                    } else {
                        break;
                    }
                }
            }
            }
        }
    }
}

// The bowgun Ganados of the cliff: once the player reaches the beach they and up to four other
// Ganados come after him.
static void r301_setEmBowgun()
{
    RsfSet(G_ROOM_ID, 1);
    {
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    cEmWrap em3;
    cEmWrap em4;
    u32 i;
    int cnt;

    em0.setEm(0xD, -1, 0, 1, 1);
    em1.setEm(0xE, -1, 0, 1, 1);
    em2.setEm(0x20, -1, 0, 1, 1);
    em3.setEm(0xCF, -1, 0, 1, 1);
    em4.setEm(0xD0, -1, 0, 1, 1);
    while (SceAtHitCheck(9) == 0) {
        SceSleep(1);
    }
    i = 0;
    em0.setFindPL();
    cnt = 0;
    em1.setFindPL();
    em2.setFindPL();
    em3.setFindPL();
    em4.setFindPL();
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (em->id == 0x20 && em0.getPtr() != em && em1.getPtr() != em && em2.getPtr() != em && em->type != 6) {
            cEmWrap w;

            w.setPtr(em, 1);
            if (w.isActive() == 1) {
                w.setGoto(&pPL->pos, 0xB);
                cnt++;
            }
        }
        if (cnt == 4) {
            break;
        }
    }
    }
}

// The breakable rock wall: a hit object that only the explosives break.
static void r301_checkRockWall()
{
    Vec pos = {9293.0f, -6600.0f, 8202.0f};
    cEm* dram;

    if (getRoomEtcDram(0x12, &dram, 1)) {
        pos = dram->pos;
    }
    {
        Vec rot = {0.0f, 0.0f, 0.0f};
        cEmHit* em = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &pos, &rot, 1);

        YarareInit(em, 0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 0, 0x41);
        SceSleep(1);
        for (;;) {
            if (em->ckStatus() == 1) {
                u32 wep = em->ckDmgWeapon();

                switch (wep) {
                case 0xD:
                case 0x12:
                case 0x13: {
                    cObj* obj;
                    u32 zero = 0;

                    RsfSet(G_ROOM_ID, 0);
                    SceAtSetEnable(2, 0);
                    EmMgr.destroy(em);
                    obj = SmdGetObjPtr(0x2D);
                    if (obj) {
                        obj->be_flag &= ~2;
                    }
                    SndCall(6, 2, &obj->pos, 0, 0, 0);
                    EstSet(0, -1, 0, 0, 1, 0, 0, 0, zero, (void*) zero);
                    SceExit();
                    break;
                }
                }
            }
            SceSleep(1);
        }
    }
}

// Shelf 0 (chest 0x2E, type 0x5B lid style 9) opens (mode 1: snap).
void r301_openShelf_main(int no, int mode)
{
    if (no == 0) {
        OpenBoxMain(9, mode, 0x5B, 0x2E, -1, -1);
    }
}

// Item-event opener: animate the shelf open.
static void r301_openShelf(int no)
{
    r301_openShelf_main(no, 0);
}

// Item-event "already opened": the shelf posed open.
static void r301_openedShelf(int no)
{
    r301_openShelf_main(no, 1);
}

// The water surface: a render target blended into the water object.
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r301_texTbl;

    if (GetTexRenderMgr(&r301_work.p->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r301_work.p->tex->texId;
        r301_work.p->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 3, r301_work.p->tex->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x15);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
}
