#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emdoor.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "room_data.h"
#include "route_ck.h"
#include "st_mgr_event.h"
#include "atari_init.h"
#include "mes.h"
#include "fade.h"
#include "sscrn.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "math_sub.h"
#include "cSceObj.h"

// Room 3-11 (D:/Bio4/Prog/r311.cpp): the iron ball crane terminal Ashley operates, the Ganado
// waves behind the sliding door, the kidnap attempt and the enemy reset counter.

// The crane terminal event: Ashley works the terminal while Leon fights, the ball is thrown
// three times.
struct R311Work {
    cObj* ball;           // 0x000  the iron ball (room arc 0x1F/0x20)
    cObj* crane;          // 0x004  the crane arm (0x23/0x24)
    u32 throwCnt;         // 0x008  throws so far
    int throwing;         // 0x00C  1 while r311_throwIronBall runs
    ScePrim* terminal;    // 0x010  r311_execAshleyOperateTerminal task
    ScePrim* appear;      // 0x014  r311_execEmAppear task
    ScePrim* doorTask;    // 0x018  r311_moveEmDoor task
    int eff;              // 0x01C  terminal effect kind (EspPullCoreKind)
    u32 resetCnt;         // 0x020  enemies reset so far
    cSceObj door;         // 0x024  the sliding door going up
    cSceObj door2;        // 0x11C  and coming down
    int doorOpen;         // 0x214
};

static R311Work* r311_work;

// The shutter/terminal bits live in the second word of the room's save record (RoomData).
#define R311_SAVE_FLAGS (*(u32*) (RoomData.getRoomSavePtr(pG->room_id) + 4))

// The skip-to-black of the crane cuts: FadeSet(0x80000000, black, clear, 10 frames).
static inline void r311_fadeOut()
{
    FadeColorPair col;

    *(u32*) &col.start = 0xFF;
    *(u32*) &col.end = 0;
    FadeSet(0x80000000, &col.start, &col.end, 10, 0, 0);
}

static void r311_checkEmMoveCtrl();
int r311_execAshleyEvent();
void r311_initEmDoor();
static void r311_moveEmDoor(int open);
void r311_execEmAppear_end();
static void r311_execEmAppear();
static void r311_checkEmReset();
static void r311_checkBgm();
static void r311_checkAshleyKidnap();
static void r311_throwIronBall_se();
static void r311_throwIronBall_HitCk();
static void r311_throwIronBall();
static void r311_execAshleyOperateTerminal();
static void r311_checkIronBallTerminal();
void r311_initIronBall();

static const AtEffInfo r311_effInfo = {
    1, {0xD2, 1}, {1, 3}, {0xD2, 1}, {0xD2, 1}, {0xD2, 1}, {0xD2, 1}, {1, 3}, {1, 3},
};
// Enemy list entries of the door wave (setPtr numbers).
static const u8 r311_emList[8] = {0xF3, 0xF4, 0xF5, 0xF6, 0xFB, 0xFC, 0xFD, 0};

// Room init (the crane hall): Ashley initialised as the follower; water hit effects; the enemy reset
// task; the sliding door and the iron ball / crane; the stream; doors 4/5 paired; the kidnap and the
// enemy goal-point tasks.
void R311Init()
{
    cEmDoor* door4;
    cEmDoor* door5;

    R311Work*& wp = r311_work;   // reference: the following `lwz pPL` stays below the store (r227 idiom)
#line 62 "D:/Bio4/Prog/r311.cpp"
    wp = (R311Work*) MEM_CALLOC(sizeof(R311Work), 1, 0xd);
    SubCharInit(1, &pPL->pos, pPL->ang.y);
    StaFlagOn(pG, STA_SUB_ASHLEY);
    EatMgr.registEffInfo(2, (AtEffInfo*) &r311_effInfo);
    SceExec(0x12, (TaskFunc) r311_checkEmReset, 0, 0, 2, 0);
    r311_initEmDoor();
    r311_initIronBall();
    SceExec(0x12, (TaskFunc) r311_checkBgm, 0, 0, 2, 0);
    if (getRoomEtcDoor(4, &door4, 1) != 0 && getRoomEtcDoor(5, &door5, 1) != 0) {
        door4->setDoor(door5);
    }
    SceExec(0x12, (TaskFunc) r311_checkAshleyKidnap, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r311_checkEmMoveCtrl, 0, 0, 2, 0);
}

// Per-frame room main: nothing.
void R311Main()
{
}

// While the player is in area 10 the live Ganados are sent to the two goal points in turn.
static void r311_checkEmMoveCtrl()
{
    cEmWrap em;
    Vec pos;
    u32 i;
    int n;

    for (;;) {
        while (SceAtHitCheck(10) == 0) {
            SceSleep(1);
        }
        SceSleep(15);
        n = 0;
        for (i = 0; i < EmMgr.getArrayNum(); i++) {
            cEm* p = EmMgr.fastAt(i);

            if (p->id >= 0x10 && p->id <= 0x20 && (p->be_flag & 0x201) == 1) {
                em.setPtr(p, 1);
                if (n & 1) {
                    SceAtGetCenterPos(&pos, 0xC);
                } else {
                    SceAtGetCenterPos(&pos, 0xD);
                }
                n++;
                em.setGoto(&pos, 0xB);
            }
        }
        while (SceAtHitCheck(10) == 1) {
            SceSleep(1);
        }
        SceSleep(300);
    }
}

// A Ganado grabbed Ashley: she is carried to the exit unless the player frees her in time.
// Returns 1 when she was kidnapped (or the cut was skipped).
int r311_execAshleyEvent()
{
    Vec center;
    Vec save;
    Vec p;
    int ret = 0;
    u32 i;
    u32 se;

    if (pSUB != NULL && pSUB->l_pl < 25000000.0f && (SubCharGetCondition() & 1)) {
        pSUB->dmg.set(0, 0x80);
        while (pSUB->pos.y >= 50.0f) {
            if (!(SubCharGetCondition() & 1)) {
                return 0;
            }
            if (Key.trg & 0x20000000) {
                ret = 1;
                pSUB->dmg.clear();
                goto end;
            }
            SceSleep(1);
        }
        for (i = 0; i < 30; i++) {
            if (Key.trg & 0x20000000) {
                ret = 1;
                pSUB->dmg.clear();
                goto end;
            }
            SceSleep(1);
        }
        SceEventStart(1);
        CamCtrl.CutCall(7);
        pSUB->setNoSuspend(1);
        SubCharCtrl(5, 0);
        {
            Vec* pp = &p;
            save = pSUB->pos;
            pp->x = 5043.0f;
            pp->y = 0.0f;
            pp->z = -1564.0f;
            pSUB->setPos(pp);
        }
        SceAtGetCenterPos(&center, 0);
        p.y = GetXZAngle(&pSUB->pos, &center);
        p.x = 0.0f;
        p.z = 0.0f;
        pSUB->setAng(&p);
        SceSleep(1);
        se = SndCall(6, 5, 0, 0, 0, 0);
        pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 10, 0, 1, 0);
        SceMesSet(4, 0xF0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        while (CamCtrl.IsMotionEnd() == 0) {
            if (Key.trg & 0x20000000) {
                if (se) {
                    SndStop(se, 0);
                }
                ret = 1;
                break;
            }
            SceSleep(1);
        }
        MessageControl* m = &cMes;
        for (int k = 0; k < 16; k++) {
            m->Delete(k);
        }
        SubCharCtrl(1, 0);
        pSUB->setNoSuspend(0);
        pSUB->setPos(&save);
        pSUB->dmg.clear();
        SceEventEnd(0);
    } else {
        SceSleep(60);
    }
end:
    return ret;
}

// The sliding door (scroll object 0x9C): up by 2800 over 30 frames, down again with a bounce.
void r311_initEmDoor()
{
    const f32 height = 2800.0f;   // first in the constant pool (r102 openCover idiom)
    cObj* o = SmdGetObjPtr(0x9C);

    if (o) {
        Vec d = {0.0f, height, 0.0f};

        r311_work->door.initMove1_pos(o, 30, &d, 20.0f, 20.0f);
        Vec zero = {0.0f, 0.0f, 0.0f};
        r311_work->door2.initMove3_y(o, &zero, 20.0f, height, 0.1f);
        r311_work->door2.setReverse(1);
        r311_work->door.setStartPos();
        r311_work->doorOpen = 0;
    }
}

// Task: the sliding door 0x9C goes up (open = 1: area 6 off, SE 6 then 7, the up move1) or comes down
// (SE 8, the drop move3, area 6 on).
static void r311_moveEmDoor(int open)
{
    cObj* o = SmdGetObjPtr(0x9C);

    if (o) {
        r311_work->doorOpen = open;
        if (r311_work->doorOpen == 1) {
            SceAtSetEnable(6, 0);
            SndCall(6, 6, &o->pos, 0, 0, 0);
            r311_work->door.setStart();
            while (r311_work->door.move() == 1) {
                SceSleep(1);
            }
            SndCall(6, 7, &o->pos, 0, 0, 0);
        } else {
            SndCall(6, 8, &o->pos, 0, 0, 0);
            r311_work->door2.setStart();
            while (r311_work->door2.move() == 1) {
                SceSleep(1);
            }
            SndCall(6, 9, &o->pos, 0, 0, 0);
            SceAtSetEnable(6, 1);
        }
        r311_work->doorTask = 0;
    }
}

// The wave was skipped: every wave enemy is put at the goal point right away.
void r311_execEmAppear_end()
{
    Vec center;
    cEmWrap em[7];
    u32 i;

    if (r311_work->appear) {
        SceKill(r311_work->appear);
    }
    if (r311_work->doorTask) {
        SceKill(r311_work->doorTask);
    }
    if (r311_work->doorOpen == 1) {
        SceExec(0x12, (TaskFunc) r311_moveEmDoor, 0, 0, 2, 0);
    }
    SceAtGetCenterPos(&center, 5);
    const u8* list = r311_emList;
    for (i = 0; i < 7; i++) {
        em[i].setPtr(list[i], -1, 0);
        em[i].setPos(&center);
    }
    pG->Room_flg[0] |= 0x80000000;
    r311_work->appear = 0;
}

// The door wave: the four behind the door come out first, then the three from the side.
static void r311_execEmAppear()
{
    Vec center;
    cEmWrap em[7];
    u32 i;

    SceAtGetCenterPos(&center, 5);
    const u8* list = r311_emList;
    for (i = 0; i < 7; i++) {
        em[i].setPtr(list[i], -1, 0);
    }
    if (em[0].isAlive() == 1 || em[1].isAlive() == 1 || em[2].isAlive() == 1 || em[3].isAlive() == 1) {
        r311_work->doorTask = SceExec(0x12, (TaskFunc) r311_moveEmDoor, 1, 0, 2, 0);
        SceSleep(20);
        em[0].setGoto(&center, 1);
        SceSleep(10);
        em[1].setGoto(&center, 1);
        SceSleep(5);
        em[2].setGoto(&center, 1);
        SceSleep(15);
        em[3].setGoto(&center, 1);
        SceSleep(30);
        SceExec(0x12, (TaskFunc) r311_moveEmDoor, 0, 0, 2, 0);
    }
    SceSleep(30);
    em[4].setGoto(&center, 0xB);
    SceSleep(10);
    em[5].setGoto(&center, 0xB);
    SceSleep(15);
    em[6].setGoto(&center, 0xB);
    SceSleep(25);
    pG->Room_flg[0] |= 0x80000000;
    r311_work->appear = 0;
}

// Area 4: the kidnap attempt, the wave, then the reinforcements (list -9..-6) as the wave thins out,
// with the reset budget by difficulty.
static void r311_checkEmReset()
{
    u8 list2[4] = {0xF7, 0xF8, 0xF9, 0xFA};
    cEmWrap em[7];
    cEmWrap em2[4];
    u32 i;
    u32 cnt;
    u32 n;
    u32 lim;
    int can;

    pG->Room_flg[0] &= ~0x80000000;
    if ((int) R311_SAVE_FLAGS >= 0) {
        int r;

        while (SceAtHitCheck(4) == 0) {
            SceSleep(1);
        }
        r = r311_execAshleyEvent();
        SceEventStart(1);
        CamCtrl.CutCall(8);
        {
            cEmWrap em3[7];
            const u8* list = r311_emList;

            for (i = 0; i < 7; i++) {
                em3[i].setPtr(list[i], -1, 0);
                em3[i].setNoSuspend(1);
            }
            r311_work->doorTask = 0;
            r311_work->appear = SceExec(0x12, (TaskFunc) r311_execEmAppear, 0, 0, 2, 0);
            while (CamCtrl.IsMotionEnd() == 0) {
                if (r == 1 || (Key.trg & 0x20000000)) {
                    r311_fadeOut();
                    r311_execEmAppear_end();
                    SubScreenWait(20);
                    break;
                }
                SceSleep(1);
            }
            CamCtrl.Comeback(0);
            for (i = 0; i < 7; i++) {
                em3[i].setNoSuspend(0);
            }
        }
        SceEventEnd(0);
    } else {
        r311_execEmAppear();
    }
    R311_SAVE_FLAGS |= 0x80000000;
    while (!(pG->Room_flg[0] & 0x80000000)) {
        SceSleep(1);
    }
    SceSleep(30);
    {
        const u8* list = r311_emList;

        for (i = 0; i < 7; i++) {
            em[i].setPtr(list[i], -1, 0);
            em[i].setNoSuspend(0);
        }
    }
    n = 0;
    for (;;) {
        cnt = 0;
        for (i = 0; i < 7; i++) {
            if (em[i].isActive() == 1) {
                cnt++;
            }
        }
        for (i = 0; i < 4; i++) {
            if (em2[i].isActive() == 1) {
                cnt++;
            }
        }
        if (cnt <= 5) {
            for (i = 0; i < 2; i++) {
                em2[n].setEm(list2[n], -1, 0, 1, 1);
                n++;
            }
            if (n > 3) {
                break;
            }
        }
        SceSleep(1);
    }
    r311_work->resetCnt = 4;
    can = 1;
    switch (pG->Game_level) {
    case 0:
    case 1:
    case 2:
    default:
        lim = 0;
        break;
    case 3:
    case 4:
    case 5:
    case 6:
        lim = 8;
        break;
    case 7:
    case 8:
    case 9:
    case 10:
        lim = 16;
        break;
    }
    for (;;) {
        cnt = 0;
        for (i = 0; i < 4; i++) {
            if (em2[i].isActive() == 1) {
                cnt++;
            }
        }
        if (can == 1 && cnt <= 5) {
            u32 k = 0;

            for (i = 0; i <= 3; i++) {
                if (em2[i].ckResetEnable() == 1) {
                    em2[i].setReset();
                    k++;
                    r311_work->resetCnt++;
                }
                if (k > 1) {
                    break;
                }
            }
        }
        if (!(R311_SAVE_FLAGS & 0x40000000)) {
            can = r311_work->resetCnt <= lim;
        } else {
            can = r311_work->resetCnt <= 4;
        }
        SceSleep(1);
    }
}

// Battle stream 3 from a Ganado spotting the player until no Ganado (0x10..0x20) is alive, repeatedly.
static void r311_checkBgm()
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

// The door follows Ashley when she is being carried through area 9.
static void r311_checkAshleyKidnap()
{
    int on = 0;

    for (;;) {
        if (on == 0) {
            if ((SubCharGetCondition() & 8) && SceAtHitCheck(9) == 1) {
                on = 1;
                r311_moveEmDoor(1);
            }
        } else {
            if (!(SubCharGetCondition() & 8) || SceAtHitCheck(9) == 0) {
                on = 0;
                r311_moveEmDoor(0);
            }
        }
        SceSleep(1);
    }
}

// The ball's impact SE 59 frames into the swing (SE 3 on the third, gate-breaking throw, else SE 2).
static void r311_throwIronBall_se()
{
    SceSleep(59);
    if (r311_work->throwCnt == 3) {
        SndCall(6, 3, &r311_work->ball->pos, 0, 0, 0);
    } else {
        SndCall(6, 2, &r311_work->ball->pos, 0, 0, 0);
    }
}

// The ball's hit check along its swing: 80 frames of a hit ray at the ball part.
static void r311_throwIronBall_HitCk()
{
    u32 i;

    if (r311_work->ball) {
        for (i = 0; i < 80; i++) {
            cModel* part = r311_work->ball->getPartsPtr(10);
            Vec p = {0.0f, -1100.0f, 0.0f};

            PSMTXMultVec(part->mat, &p, &p);
            PlWepHitCheck2(0, &p, &p, 0x12, 3, 1500.0f);
            SceSleep(1);
        }
    }
}

// One throw of the iron ball: the crane winds up, the ball swings; the first throw is a camera cut,
// the second checks hits, the third breaks the gate.
static void r311_throwIronBall()
{
    int skip = 0;
    u32 i;

    r311_work->throwing = 1;
    if (r311_work->ball && r311_work->crane) {
        SndCall(6, 0, &r311_work->crane->pos, 0, 0, 0);
        r311_work->crane->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x26), 10, 0, 1, 0);
        while (MotionGetState(r311_work->crane) != 4) {
            SceSleep(1);
        }
        SndCall(6, 1, &r311_work->ball->pos, 0, 0, 0);
        r311_work->ball->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x22), 10, 0, 1, 0);
        SceExec(0x12, (TaskFunc) r311_throwIronBall_se, 0, 2, 2, 0);
        r311_work->throwCnt = r311_work->throwCnt + 1;
        switch (r311_work->throwCnt) {
        case 1:
            SceEventStart(1);
            EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, 0, 0);
            CamCtrl.CutCall(4);
            while (CamCtrl.IsMotionEnd() == 0) {
                if (Key.trg & 0x20000000) {
                    r311_fadeOut();
                    SubScreenWait(20);
                    break;
                }
                SceSleep(1);
            }
            CamCtrl.Comeback(0);
            SceEventEnd(0);
            break;
        case 2:
            SceExec(0x12, (TaskFunc) r311_throwIronBall_HitCk, 0, 2, 2, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, 0, 0);
            break;
        case 3:
            EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, 0, 0);
            for (i = 0; i < 30; i++) {
                if (Key.trg & 0x20000000) {
                    skip = 1;
                    break;
                }
                SceSleep(1);
            }
            SceEventStart(1);
            CamCtrl.CutCall(5);
            for (i = 0; i < 29; i++) {
                if (Key.trg & 0x20000000) {
                    skip = 1;
                    break;
                }
                SceSleep(1);
            }
            R311_SAVE_FLAGS |= 0x40000000;
            KyfFlagOn(pG, KYF_ST1_08);
            r311_work->resetCnt = 0;
            SceAtSetEnable(2, 0);
            SceAtSetEnable(0, 1);
            if (r311_work->eff) {
                EffectEspDelete(0, (u8) r311_work->eff, 0, 0);
                EffectEspgenDelete(0, (u8) r311_work->eff, 0);
                EffectEfmDelete(0, (u8) r311_work->eff, 0);
            }
            while (CamCtrl.IsMotionEnd() == 0) {
                if ((Key.trg & 0x20000000) || skip == 1) {
                    r311_fadeOut();
                    SubScreenWait(20);
                    break;
                }
                SceSleep(1);
            }
            CamCtrl.Comeback(0);
            SceEventEnd(0);
            break;
        }
        while (MotionGetState(r311_work->ball) != 4) {
            SceSleep(1);
        }
        r311_work->crane->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x28), 10, 0, 1, 0);
        while (MotionGetState(r311_work->crane) != 4) {
            SceSleep(1);
        }
    }
    r311_work->throwing = 0;
}

// Ashley at the terminal: placed at the crane, she pulls the lever again whenever the ball is idle.
static void r311_execAshleyOperateTerminal()
{
    int wait;
    u32 step;

    if (r311_work->crane) {
        Vec p = {12.87f, -1500.0f, 619.92004f};
        cSubChar* sub;
        Vec* rot;

        PSMTXMultVec(r311_work->crane->mat, &p, &p);
        pSUB->ang.y = r311_work->crane->ang.y + PI;
        sub = SUB_CHAR();
        rot = &sub->ang;
        sub->setPos(&p);
        sub->setAng(rot);
    }
    SubCharCtrl(0, 0);
    SubCharCtrl(5, 2);
    step = 0;
    wait = 0;
    pSUB->atari.setPriority(3);
    pSUB->atari.set(0, 100.0f, 200.0f);
    AtariOnV(&pSUB->atari, 0x300);
    pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2B), 10, 0, 1, 0);
    if (r311_work->throwing == 1) {
        step = 3;
    }
    for (;;) {
        switch (step) {
        case 0:
            pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x29), 10, 0, 1, 0);
            step = 1;
        case 1:
            if (MotionGetState(pSUB) == 4) {
                pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 10, 0, 1, 0);
                step++;
                SceExec(0x12, (TaskFunc) r311_throwIronBall, 0, 0, 2, 0);
            }
            break;
        case 2:
            if (MotionGetState(pSUB) == 4) {
                step = 3;
            }
            break;
        case 3:
            if (r311_work->throwing == 0) {
                wait = 60;
                step = 4;
            }
            break;
        case 4:
            if (wait != 0) {
                wait--;
            } else {
                step = 0;
            }
            break;
        }
        if (!((R311_SAVE_FLAGS & 0x40000000) || (SubCharGetStatus() & 0x80) || (SubCharGetStatus() & 0x20000000) ||
              (SubCharGetStatus() & 0x02000000))) {
            SceSleep(1);
        } else {
            break;
        }
    }
    SubCharCtrl(1, 0);
    pSUB->atari.setPriority(0);
    AtariOnV(&pSUB->atari, 0x300);
    r311_work->terminal = 0;
}

// The terminal prompt (area 1): Leon can pull the lever himself or send Ashley.
static void r311_checkIronBallTerminal()
{
    if (R311_SAVE_FLAGS & 0x40000000) {
        SceMesSet(3, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        return;
    }
    if (pSUB != NULL && RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) < 4000.0f && !StaFlagChk(pG, STA_SUB_CATCHED)) {
        SceUpCut(1, 9, -1, 0);
        switch (SceMesGetSelection()) {
        case 0:
            break;
        case 1:
            if (r311_work->throwing == 1) {
                SceUpCut(2, 9, -1, 0);
                return;
            }
            if (r311_work->terminal) {
                SceKill(r311_work->terminal);
                r311_work->terminal = 0;
                SubCharCtrl(1, 0);
                pSUB->atari.setPriority(0);
            }
            SceExec(0x12, (TaskFunc) r311_throwIronBall, 0, 0, 2, 0);
            break;
        case 2:
            if (r311_work->terminal == 0) {
                r311_work->terminal = SceExec(0x12, (TaskFunc) r311_execAshleyOperateTerminal, 0, 0, 2, 0);
            }
            break;
        }
        return;
    }
    if (r311_work->throwing == 1) {
        SceUpCut(2, 9, -1, 0);
        return;
    }
    SceUpCut(0, 9, -1, 0);
    switch (SceMesGetSelection()) {
    case -1:
    case 0:
    case 2:
        break;
    case 1:
    default:
        if (r311_work->terminal) {
            SceKill(r311_work->terminal);
        }
        SceExec(0x12, (TaskFunc) r311_throwIronBall, 0, 0, 2, 0);
        break;
    }
}

// The iron ball (arc 0x1F/0x20, hit box 1100 x 2000 below it) and the crane (0x23/0x24).
void r311_initIronBall()
{
    r311_work->ball = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), (Vec*) &vecZero, (Vec*) &vecZero, 0x10, 1);
    if (r311_work->ball) {
        r311_work->ball->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
        r311_work->ball->be_flag |= 0x1000;
        r311_work->ball->atari.init(0.0f, -2000.0f, 0.0f, 0.0f, 1100.0f, 1100.0f, 2000.0f, 10, 0x18, 0);
    }
    {
        Vec pos = {-9219.7f, 2309.3f, -4995.9f};
        Vec rot = {0.0f, -2.9146998f, 0.0f};

        r311_work->crane = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x23), ROOM_ARC_PTR(pG->pRoom, 0x24), &pos, &rot, 0x10, 1);
    }
    if (r311_work->crane) {
        r311_work->crane->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x25), 10, 0, 1, 0);
        r311_work->crane->be_flag |= 0x1000;
    }
    r311_work->throwing = 0;
    r311_work->terminal = 0;
    r311_work->eff = 0;
    if (!(R311_SAVE_FLAGS & 0x40000000)) {
        SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r311_checkIronBallTerminal, 0, 1);
        SceAtSetEnable(0, 0);
        r311_work->throwCnt = 0;
        r311_work->eff = EspPullCoreKind();
        EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, r311_work->eff, 0, 0);
    } else {
        SceAtSetEnable(2, 0);
    }
}
