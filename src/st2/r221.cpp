#include "types.h"
class cObjWep;
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "sofdec.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "game.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "motion.h"
#include "math_sub.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "fade.h"
#include "TexRender.h"
#include "cSceObj.h"
#include "db_log.h"

// Room 2-21 (D:/Bio4/Prog/r221.cpp): the insect boss arena — the shutter and the switchboard
// lever, the elevator with its doors and wires, the boss appearance and the gas bombs (the
// r201_* functions are the bomb code shared with room 2-01).

struct R221Work {
    cObj* bonbe[4];       // 0x000  gas bombs (SetObjSmd copies)
    TexRenderMng* tex;    // 0x010
    u8 eff;               // 0x014  EspPullCoreKind
    u8 pad_15[3];
    u32 bossStr;          // 0x018  boss BGM stream
    u32 str1C;            // 0x01C  second stream handle
    cSceObj door0;        // 0x020  elevator doors
    cSceObj door1;        // 0x118
    cSceObj lever;        // 0x210  switchboard lever
    cSceObj shutter0;     // 0x308  shutter (slide)
    cSceObj shutter1;     // 0x400  shutter (drop)
    u32 shutterSe;        // 0x4F8
    u32 elvSe0;           // 0x4FC
    u32 elvSe1;           // 0x500
    u32 doorSe;           // 0x504
    SCE_TASK* wireTask;    // 0x508
    SCE_TASK* wireTask2;   // 0x50C
    f32 elvY;             // 0x510  elevator rest height
};


static u8 r221_texTbl[0x20];
static R221Work* r221_work;

// The death bits of enemy list `list` (pG->Em_flg[list]), as an integer base (the r218 idiom).
static inline u32* emDeadWords(int list) { return EM_FLG_ROW(list); }

#define R221_MES_Y (0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1)

void r221_setShutterEff(int on);
static void r221_checkShutterOpen_end();
void r221_checkShutterOpen();
static void r221_checkShutter_end();
static void r221_checkShutter();
void r221_moveShutter(int a, int b);
void r221_initShutter();
static void r221_checkDoor229();
static void r221_breakDoor229();
void r221_moveElevatoDoor(int a, int b);
static void r221_moveWire(int mode);
static void r221_moveElevator(int dir);
void r221_setElevatorEff(int no);
static void r221_checkElevatorArrive_end();
static void r221_checkElevatorArrive();
static void r221_operateElevator();
static void r221_playBossBgm();
static void r221_checkBossAppear_end();
static void r221_checkBossAppear();
void r221_initSwitchboardLever();
void r221_moveSwitchboardLever(int a);
static void r221_checkSwitchboard_end();
static void r221_checkSwitchboard();
static void r221_appearBosstail();
static void r221_checkElevator();
static void r221_appearBosstail1_sub();
void r221_initInsectboss();
cObj* r201_setBonbe(int id, f32 ang);
void r201_initBonbe();
static void r221_callBonbeSe(int no);
static void r221_callFootSe();
static void r221_fadeoutBonbe(cObj* o);
static void r201_throwBonbe(int no);
static void setTexRender();

// Room init (the boss arena): the boss / shutter / switchboard / elevator setup (r221_initInsectboss),
// the gas bombs (r201_initBonbe) and the floor render target.
void R221Init()
{
#line 48 "D:/Bio4/Prog/r221.cpp"
    r221_work = (R221Work*) MEM_CALLOC(sizeof(R221Work), 1, 0xd);
    r221_initInsectboss();
    r201_initBonbe();
    setTexRender();
}

// Per-frame room main: nothing.
void R221Main()
{
}

// The boss (ESL 0x8C) attacks again: alerted, Room_flg[0] 0x00200000, the boss BGM task.
static void r221_appearBoss2nd()
{
    cEmWrap em;

    em.setPtr(0x8C, -1, 0);
    em.setFlag(1);
    pG->Room_flg[0] |= 0x00200000;
    SceExec(0x12, (TaskFunc) r221_playBossBgm, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Swap the shutter's effect: on = 1 the open-shutter effect 0xE, else the closed one 0xF.
void r221_setShutterEff(int on)
{
    EffectEspDelete(0, r221_work->eff, 0, 0);
    EffectEspgenDelete(0, r221_work->eff, 0);
    EffectEfmDelete(0, r221_work->eff, 0);
    if (on == 1) {
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xE, 1, r221_work->eff, 0, 0);
    } else {
        EstSet(0, -1, 0, 0, EFF_ROOM, 0xF, 1, r221_work->eff, 0, 0);
    }
}

// End of the shutter re-open (also its cancel path): Room_flg bit 11, SE stopped, the shutter snapped open, camera back, SceEventEnd.
static void r221_checkShutterOpen_end()
{
    RsfSet(G_ROOM_ID, 11);
    if (r221_work->shutterSe) {
        SndStop(r221_work->shutterSe, 0);
    }
    r221_moveShutter(1, 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Task: the shutter opens again once the boss has gone (or after a while).
void r221_checkShutterOpen()
{
    cEmWrap em;
    u32 i;

    em.setPtr(0x8C, -1, 1);
    if (!((pG->Room_flg[0] & 0x00800000) && em.checkStatus(EM_STATUS_ACTIVE) == 0)) {
        i = 0;
        do {
            SceDebugDisp("SHT[%d]", 1800 - i);
            em.setPtr(0x8C, -1, 1);
            if ((pG->Room_flg[0] & 0x00800000) && em.checkStatus(EM_STATUS_ACTIVE) == 0) {
                SceSleep(30);
                break;
            }
            SceSleep(1);
            i++;
        } while (i < 1800);
        while (SceCheckEventStart() != 1) {
            SceSleep(1);
        }
    }
    SceSetEventCancel(1, (TaskFunc) r221_checkShutterOpen_end, 0, -1, 1);
    r221_work->shutterSe = 0;
    SceEventStart(1);
    r221_moveShutter(1, 0);
    SceSetEventCancel(0, 0, 0, -1, 1);
    r221_checkShutterOpen_end();
}

// End of the shutter switch event: if the boss was due (Room_flg[0] 0x00400000, bit 3 once) it appears
// now; all messages cleared, the shutter effect on, camera back, SceEventEnd, Room_flg bit 8, then the
// shutter re-open watcher.
static void r221_checkShutter_end()
{
    int i;

    if ((pG->Room_flg[0] & 0x00400000) && RsfCheck(G_ROOM_ID, 3) == 0) {
        RsfSet(G_ROOM_ID, 3);
        pG->Room_flg[0] |= 0x20000000;
        SceEventStart(0);
        r221_checkBossAppear_end();
    }
    MessageControl* m = &cMes;
    for (i = 0; i < 16; i++) {
        m->Delete(i);
    }
    r221_setShutterEff(1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    RsfSet(G_ROOM_ID, 8);
    r221_checkShutterOpen();
}

// Task: the shutter switch.
static void r221_checkShutter()
{
    SceMesSet(9, 0, 1, 0x64, R221_MES_Y);
    switch (SceMesGetSelection()) {
    case 1:
    default:
        break;
    case -1:
    case 0:
    case 2:
        return;
    }
    SceAtSetEnable(0x18, 0);
    cEmWrap em;
    SceSetEventCancel(1, (TaskFunc) r221_checkShutter_end, 0, 9, 1);
    SceEventStart(1);
    CamCtrl.CutCall(8);
    SceSleep(10);
    SndCall(6, 7, 0, 0, 0, 0);
    r221_setShutterEff(1);
    SceSleep(10);
    em.setPtr(0x8C, -1, 1);
    if (em.checkStatus(EM_STATUS_ACTIVE) == 1) {
        SceMesSet(0xA, 0x30, 1, 0x64, R221_MES_Y);
        SceMesWait();
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r221_checkShutter_end();
}

// Moves the shutter (a 1: open, 0: close; b 1: set the end position only).
void r221_moveShutter(int a, int b)
{
    if (SmdGetObjPtr(0x46) == 0) {
        return;
    }
    if (a == 1) {
        SceAtSetEnable(0x17, 0);
        SceAtSetEnable(0x18, 0);
    } else {
        SceAtSetEnable(0x17, 1);
        SceAtSetEnable(0x18, 1);
        SceAtDataSet_exec(0x18, SCE_LEVEL10, 0, (TaskFunc) r221_checkShutter, 0, 1);
    }
    if (b == 1) {
        if (a == 1) {
            r221_work->shutter0.setEndPos();
        } else {
            r221_work->shutter1.setEndPos();
        }
        return;
    }
    CamCtrl.CutCall(8);
    SceSleep(15);
    SndCall(6, 0x11, 0, 0, 0, 0);
    r221_setShutterEff(a);
    SceSleep(15);
    r221_work->shutterSe = SndCall(6, 5, 0, 0, 0, 0);
    if (a == 1) {
        r221_work->shutter0.setStart();
        while (r221_work->shutter0.move() == 1) {
            SceSleep(1);
        }
    } else {
        r221_work->shutter1.setStart();
        while (r221_work->shutter1.move() == 1) {
            SceSleep(1);
        }
    }
    SndCall(6, 6, 0, 0, 0, 0);
    r221_work->shutterSe = 0;
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
}

// The shutter (object 0x46): a 90-frame move1 up to y 4386 (posed open) and a move3 gravity drop with
// bounce for the slam; its effect kind; effect on when already shut (Room_flg bit 8).
void r221_initShutter()
{
    cObj* o = SmdGetObjPtr(0x46);

    if (o) {
        Vec d;
        f32 dy = 4386.0f - o->pos.y;
        const f32 grav = -10.0f;
        Vec t = {0.0f, dy, 0.0f};
        d = t;

        r221_work->shutter0.initMove1_pos(o, 90, &d, 0.0f, 0.0f);
        r221_work->shutter0.setEndPos();
        Vec v = {0.0f, 0.0f, 0.0f};
        r221_work->shutter1.initMove3_y(o, &v, grav, -dy, 0.1f);
        r221_work->eff = EspPullCoreKind();
        r221_moveShutter(1, 1);
        if (RsfCheck(G_ROOM_ID, 8)) {
            r221_setShutterEff(1);
        }
    }
}

// Area 0 (Japanese only): message 7 at the door to r229.
static void r221_checkDoor229()
{
    if (pSys->language == 0) {
        SceMesSet(7, 0, 1, 0x64, R221_MES_Y);
    }
}

// Task: the door to room 2-29 breaks (the movie).
static void r221_breakDoor229()
{
    RsfSet(G_ROOM_ID, 9);
    SceEventStart(1);
    pPL->setNoSuspend(1);
    systemVISetBlack(1);
    Sofdec.Initialize("movie/r229_ev.sfd", 0);
    SceSleep(1);
    SeAtSndCall(0);
    SceEventEnd(0);
    Vec pos = {-23572.0f, 3000.0f, -5848.0f};
    SndCall(6, 0x10, &pos, 0, 0, 0);
    if (pSys->language == 0) {
        SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r221_checkDoor229, 0, 1);
    } else {
        SceAtSetEnable(0, 0);
    }
    {
        cObj* o = SmdGetObjPtr(0x44);

        if (o) {
            o->be_flag &= ~2;
        }
    }
    {
        cObj* o = SmdGetObjPtr(0x45);

        if (o) {
            o->be_flag |= 2;
        }
    }
    SceSleep(1);
    pPL->setNoSuspend(0);
}

// The elevator doors (a 1: open, 0: close; b 1: set the position only).
void r221_moveElevatoDoor(int a, int b)
{
    cObj* o9 = SmdGetObjPtr(9);
    cObj* o10 = SmdGetObjPtr(0xA);
    int second;
    u32 i;

    if (o9 == 0 || o10 == 0) {
        return;
    }
    if (b == 1) {
        if (a == 1) {
            SceAtSetEnable(7, 0);
            r221_work->door0.setReverse(1);
            r221_work->door1.setReverse(1);
        } else {
            SceAtSetEnable(7, 1);
        }
        return;
    }
    r221_work->doorSe = SndCall(6, 0xB, 0, 0, 0, 0);
    if (a == 1) {
        r221_work->door0.setReverse(0);
        r221_work->door1.setReverse(0);
    } else {
        SceAtSetEnable(7, 1);
        r221_work->door0.setReverse(1);
        r221_work->door1.setReverse(1);
    }
    second = 0;
    for (i = 0; i < 60; i++) {
        if (i == 25) {
            second = 1;
        }
        r221_work->door0.move();
        if (second == 1) {
            r221_work->door1.move();
        }
        SceSleep(1);
    }
    if (a == 1) {
        SceAtSetEnable(7, 0);
    }
}

// Task: the elevator wires scroll (mode 0 up, 1 down, 2 stop).
static void r221_moveWire(int mode)
{
    cObj* w0 = SmdGetObjPtr(0x42);
    cObj* w1 = SmdGetObjPtr(0x43);
    const f32 lim = 0.04f;
    const f32 step0 = 0.001f;
    f32 s;

    if (w0 == 0 || w1 == 0) {
        return;
    }
    w0->pModelInfo->flagsDC |= 1;
    w1->pModelInfo->flagsDC |= 1;
    switch (mode) {
    case 0:
        SceSleep(3);
        s = 0.0f;
        goto up;
    wait0:
        SceSleep(1);
    up:
        s += step0;
        w0->pModelInfo->uvScrollV = s;
        w1->pModelInfo->uvScrollV = -s;
        if (!(s > lim)) {
            goto wait0;
        }
        s = lim;
        w0->pModelInfo->uvScrollV = s;
        w1->pModelInfo->uvScrollV = -s;
        break;
    case 1:
        s = lim;
        goto down;
    wait1:
        SceSleep(1);
    down:
        s -= 0.002f;
        w0->pModelInfo->uvScrollV = s;
        w1->pModelInfo->uvScrollV = -s;
        if (!(s <= 0.0f)) {
            goto wait1;
        }
        s = 0.0f;
        w0->pModelInfo->uvScrollV = s;
        w1->pModelInfo->uvScrollV = -s;
        break;
    }
    r221_work->wireTask = 0;
    r221_work->wireTask2 = 0;
}

// Task: the elevator ride (dir 0: down with the camera event, 1: up with the event, 2: up).
static void r221_moveElevator(int dir)
{
    cObj* o = SmdGetObjPtr(0x41);

    if (o) {
        int evt = 0;
        int down = 0;

        o->be_flag |= 0x22;
        switch ((u32) dir) {
        case 0:
            evt = 1;
            down = 1;
            break;
        case 1:
            evt = 1;
            break;
        case 2:
            break;
        }
        Vec d = {0.0f, -3000.0f, 0.0f};
        cSceObj elv;
        u32 i;

        elv.initMove1_pos(o, 90, &d, 40.0f, 0.0f);
        elv.setVibration(10, 10, 2.0f, 0.5f, 2.0f);
        if (evt == 1) {
            SceEventStart(0);
            {
                cPlayer* pl = pPL;
                u32 n;

                if (pl) {
                    for (n = 0; n < 4; n++) {
                        if (elv.sub[n] == NULL) {
                            elv.sub[n] = pl;
                            break;
                        }
                    }
                }
            }
            pPL->setNoSuspend(1);
            if (down == 1) {
                CamCtrl.CutCall(7);
            }
        }
        if (down == 1) {
            r221_moveElevatoDoor(0, 0);
            SceSleep(15);
        } else {
            elv.setReverse(1);
        }
        if (evt == 1) {
            SceExec(0x12, (TaskFunc) r221_moveWire, 0, 2, SCE_PRIO_DEF_2, 0);
        }
        if (down == 0 && evt == 1) {
            r221_work->elvSe1 = SndCall(6, 0xF, &o->pos, 0, 0, 0);
        } else {
            r221_work->elvSe1 = SndCall(6, 0, &o->pos, 0, 0, 0);
        }
        for (i = 0; i < 90; i++) {
            if (down == 1 && i == 60) {
                FadeSetW(1, 30, 0, 0);
            }
            elv.move();
            SceSleep(1);
        }
        if (down == 1) {
            if (RsfCheck(G_ROOM_ID, 10) == 0) {
                RsfSet(G_ROOM_ID, 10);
                SceAtExecRoomJump(0x22B, (Vec*) &vecZero, (Vec*) &vecZero, 0);
            } else {
                SceAtExecute(0xF);
            }
        } else {
            r221_work->wireTask = SceExec(0x12, (TaskFunc) r221_moveWire, 1, 2, SCE_PRIO_DEF_2, 0);
            SndCall(6, 1, 0, 0, 0, 0);
            r221_work->elvSe0 = 0;
            r221_work->elvSe1 = 0;
            r221_moveElevatoDoor(1, 0);
        }
        if (evt == 1) {
            pPL->setNoSuspend(0);
            CamCtrl.Comeback(0);
            SceEventEnd(2);
        }
    }
}

// Start elevator effect `no` (0..8 -> effect types 0x18 down to 0x10): the wire sparks and the boss's attacks on the cage.
void r221_setElevatorEff(int no)
{
    switch ((u32) no) {
    case 0:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x18, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 1:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x17, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 2:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x16, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 3:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x15, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 4:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x14, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 5:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x13, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 6:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x12, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 7:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x11, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    case 8:
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 1, ESP_CORE_KIND_NONE, 0, 0);
        break;
    }
}

// End of the elevator ride (also its cancel path, Room_flg[0] 0x20000000): the last effect, the cage
// object 0x41 snapped to elvY and shown, SEs stopped, wires stopped, the doors opened, the boss dealt
// with, camera back, SceEventEnd.
static void r221_checkElevatorArrive_end()
{
    int i;

    if (pG->Room_flg[0] & 0x20000000) {
        if (!(pG->Room_flg[0] & 0x02000000)) {
            r221_setElevatorEff(8);
        }
        cObj* o = SmdGetObjPtr(0x41);
        if (o) {
            o->pos.y = r221_work->elvY;
            o->setPos(&o->pos);
            o->be_flag |= 2;
        }
        if (r221_work->elvSe0) {
            SndStop(r221_work->elvSe0, 0);
        }
        if (r221_work->elvSe1) {
            SndStop(r221_work->elvSe1, 0);
        }
        if (r221_work->wireTask2) {
            SceKill(r221_work->wireTask2);
        }
        if (r221_work->wireTask) {
            SceKill(r221_work->wireTask);
        }
        cObj* w0 = SmdGetObjPtr(0x42);
        cObj* w1 = SmdGetObjPtr(0x43);
        if (w0 && w1) {
            w0->pModelInfo->flagsDC |= 1;
            w1->pModelInfo->flagsDC |= 1;
            w0->pModelInfo->uvScrollV = 0.0f;
            w1->pModelInfo->uvScrollV = 0.0f;
        }
        r221_moveElevatoDoor(1, 1);
        if (r221_work->doorSe) {
            SndStop(r221_work->doorSe, 0);
        }
    }
    MessageControl* m = &cMes;
    for (i = 0; i < 16; i++) {
        m->Delete(i);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceAtSetEnable(8, 0);
}

// Task: the elevator arrives; the boss may attack it on the way.
static void r221_checkElevatorArrive()
{
    u32 i;
    u32 k = 1;
    u32 thr = 7200 * k / 7;

    for (i = 0; i < 7200; i++) {
        SceDebugDisp("ELV[%d]", 7200 - i);
        if (i >= thr) {
            k++;
            thr = 7200 * k / 7;
            r221_setElevatorEff(k);
        }
        cEmWrap em;
        em.setPtr(0x8C, -1, 1);
        if ((pG->Room_flg[0] & 0x00800000) && em.checkStatus(EM_STATUS_ACTIVE) == 0) {
            u32 n = k + 1;
            u32 j;
            u32 m;

            for (j = 0; j < 180; j++) {
                SceDebugDisp("ELV[%d]", 180 - j);
                SceSleep(1);
            }
            while (1) {
                if (SceCheckEventStart() == 1 && RsfCheck(G_ROOM_ID, 11)) {
                    break;
                }
                SceSleep(1);
            }
            SceEventStart(0);
            pPL->setNoSuspend(1);
            em.setNoSuspend(1);
            SceSleep(15);
            FadeSetW(1, 15, 0, 0);
            SceSleep(30);
            for (m = n; m <= 7; m++) {
                r221_setElevatorEff(m);
            }
            FadeSetW(0x80000001, 15, 0, 0);
            pPL->setNoSuspend(0);
            em.setNoSuspend(0);
            SceEventEnd(0);
            break;
        }
        SceSleep(1);
    }
    while (1) {
        if (SceCheckEventStart() == 1 && RsfCheck(G_ROOM_ID, 11)) {
            break;
        }
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 6);
    ScfFlagOn(pG, SCF_88);
    r221_work->wireTask = 0;
    r221_work->doorSe = 0;
    r221_work->elvSe1 = 0;
    pG->Room_flg[0] &= ~0x02000000;
    SceSetEventCancel(1, (TaskFunc) r221_checkElevatorArrive_end, 0, 2, 1);
    SceEventStart(1);
    CamCtrl.CutCall(4);
    SceSleep(15);
    SndCall(6, 0xC, 0, 0, 0, 0);
    pG->Room_flg[0] |= 0x02000000;
    r221_setElevatorEff(8);
    SceSleep(15);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(5);
    r221_moveElevator(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r221_checkElevatorArrive_end();
}

// Task: the elevator switch.
static void r221_operateElevator()
{
    if (!FlagChk(pG->Em_flg[5], 140) && (pG->Room_flg[0] & 0x00200000)) {
        cEmWrap em;

        em.setPtr(0x8C, -1, 1);
        pG->Em_list[0x8C].set = 2;
        pG->Em_list[0x8C].hp = em.getHp();
        pG->Em_list[0x8C].pos[0] = -1458;
        pG->Em_list[0x8C].pos[1] = 0x32;
        pG->Em_list[0x8C].pos[2] = -577;
        pG->Em_list[0x8C].rot[1] = 0x17D2;
    }
    if (r221_work->str1C) {
        SndStrReq(r221_work->str1C, 4, 200, 0);
    }
    r221_moveElevator(0);
}

// Task: the boss BGM.
static void r221_playBossBgm()
{
    cEmWrap em;

    if (r221_work->bossStr) {
        SndStrReq(r221_work->bossStr, 4, 200, 0);
    }
    r221_work->str1C = SndStrReq(0, 0x24, 0x80000003, 0, 0, 0.0f);
    for (;;) {
        em.setPtr(0x8C, -1, 0);
        if (em.isActive() == 0) {
            break;
        }
        SceSleep(1);
    }
    SndStrReq(r221_work->str1C, 4, 200, 0);
}

// End of the boss appearance (also its cancel path): camera back, SceEventEnd, Status_flg[2]
// 0x02000000 off, boss points reset; when cancelled the event boss is destroyed and ESL 0x8C's death
// bit / set byte cleared so it respawns fresh, then the fight state is set.
static void r221_checkBossAppear_end()
{
    cEmWrap em;
    u8 zero = 0;

    em.setPtr(0x8C, -1, 1);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    GamePointBossReset();
    if (pG->Room_flg[0] & 0x20000000) {
        em.destroy();
        {
            int list = pG->em_list_no;

            if (list >= 0) {
                emDeadWords(list)[0x8C >> 5] &= ~(0x80000000 >> (0x8C & 31));
            }
        }
        pG->Em_list[0x8C].set = zero;
        pG->Em_list[0x8C].be_flag = zero;
        pG->Em_list[0x8C].pos[0] = -0x4F8;
        pG->Em_list[0x8C].pos[1] = 0x58;
        pG->Em_list[0x8C].pos[2] = -0x1FA1;
        pG->Em_list[0x8C].rot[1] = -0xBBB;
        em.setEm(0x8C, -1, 1, 1, 1);
        SceAtSetEmItem(em.getPtr(), 0x85);
        if (!(pG->Room_flg[0] & 0x01000000)) {
            SceExec(0x12, (TaskFunc) r221_playBossBgm, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if (em.getPtr()) {
        EffectEspDelete(1, ESP_CORE_KIND_ROOM00, em.getPtr(), 0);
        EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, em.getPtr());
        EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, em.getPtr());
    }
    SceAtPtr(1)->actBtnKind = 0x35;
    SceAtPtr(2)->actBtnKind = 0x35;
    SceAtPtr(3)->actBtnKind = 0x35;
    SceAtPtr(0x10)->actBtnKind = 0x35;
    SceAtPtr(1)->trigger |= 0x80;
    SceAtPtr(2)->trigger |= 0x80;
    SceAtPtr(3)->trigger |= 0x80;
    SceAtPtr(0x10)->trigger |= 0x80;
    em.setNoSuspend(0);
    pG->Room_flg[0] |= 0x00800000;
    pG->Room_flg[0] |= 0x00200000;
}

// Task: the boss appears.
static void r221_checkBossAppear()
{
    cEmWrap em0;
    cEmWrap em1;
    u32 i;

    em0.setPtr(0x8D, -1, 1);
    em0.setFlag(2);
    em1.setPtr(0x8C, -1, 1);
    em1.setFlag(2);
    for (i = 0; i < 900; i++) {
        SceDebugDisp("BOS[%d]", 900 - i);
        if (RsfCheck(G_ROOM_ID, 8)) {
            break;
        }
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        RsfSet(G_ROOM_ID, 3);
        pG->Room_flg[0] &= ~0x01000000;
        SceSetEventCancel(1, (TaskFunc) r221_checkBossAppear_end, 0, 2, 1);
        SceEventStart(1);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        EstSet(em1.getPtr(), -1, 0, 0, EFF_EM2C, 1, 1, ESP_CORE_KIND_ROOM00, em1.getPtr(), 0);
        pG->Room_flg[0] |= 0x01000000;
        CamCtrl.clearAttachCamera();
        em1.setFlag(1);
        em1.setNoSuspend(1);
        SceExec(0x12, (TaskFunc) r221_playBossBgm, 0, 0, SCE_PRIO_DEF_2, 0);
        while (em1.ckFlag(8) == 0) {
            SceSleep(1);
        }
        em1.setNoSuspend(0);
        SceSetEventCancel(0, 0, 0, -1, 1);
        r221_checkBossAppear_end();
    }
}

// The switchboard lever (object 0x3A): a 7-frame move1 down by 407 units.
void r221_initSwitchboardLever()
{
    cObj* o = SmdGetObjPtr(0x3A);

    if (o) {
        Vec d = {0.0f, -407.00006f, 0.0f};

        r221_work->lever.initMove1_pos(o, 7, &d, 0.0f, 0.0f);
    }
}

// Pull the lever: a = 1 snaps it down, else SE 2 and the move1 plays out.
void r221_moveSwitchboardLever(int a)
{
    if (SmdGetObjPtr(0x3A) == 0) {
        return;
    }
    if (a == 1) {
        r221_work->lever.setEndPos();
        return;
    }
    SndCall(6, 2, 0, 0, 0, 0);
    while (r221_work->lever.move() == 1) {
        SceSleep(1);
    }
}

// End of the switchboard event (also its cancel path, Room_flg[0] 0x20000000): lever down, shutter
// closed with its effect, SE stopped, the wires start moving (unless 0x10000000), the elevator SE, then
// the elevator call becomes available and the boss appearance is armed.
static void r221_checkSwitchboard_end()
{
    int i;

    if (pG->Room_flg[0] & 0x20000000) {
        r221_moveSwitchboardLever(1);
        r221_moveShutter(0, 1);
        r221_setShutterEff(0);
        if (r221_work->shutterSe) {
            SndStop(r221_work->shutterSe, 0);
        }
        if (!(pG->Room_flg[0] & 0x10000000)) {
            r221_work->wireTask2 = SceExec(0x12, (TaskFunc) r221_moveWire, 0, 2, SCE_PRIO_DEF_2, 0);
        }
        if (r221_work->elvSe0 == 0) {
            cObj* o = SmdGetObjPtr(0x41);

            if (o) {
                r221_work->elvSe0 = SndCall(6, 0, &o->pos, 0, 0, 0);
            }
        }
        if (!(pG->Room_flg[0] & 0x08000000)) {
            r221_setElevatorEff(0);
        }
        if (!(pG->Room_flg[0] & 0x04000000)) {
            r221_setElevatorEff(1);
        }
    }
    MessageControl* m = &cMes;
    for (i = 0; i < 16; i++) {
        m->Delete(i);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r221_checkBossAppear, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r221_checkElevatorArrive, 0, 0, SCE_PRIO_DEF_2, 0);
    SceAtSetEnable(0xD, 0);
}

// Task: the switchboard lever event.
static void r221_checkSwitchboard()
{
    SceMesSet(2, 0, 1, 0x64, R221_MES_Y);
    switch (SceMesGetSelection()) {
    case 1:
    default:
        break;
    case -1:
    case 0:
    case 2:
        return;
    }
    r221_work->shutterSe = 0;
    r221_work->elvSe0 = 0;
    r221_work->wireTask2 = 0;
    SceSetEventCancel(1, (TaskFunc) r221_checkSwitchboard_end, 0, 2, 1);
    SceEventStart(0);
    RsfSet(G_ROOM_ID, 5);
    CamCtrl.CutCall(3);
    r221_moveSwitchboardLever(0);
    SceMesSet(3, 0x30, 1, 0x64, R221_MES_Y);
    SceMesWait();
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    {
        MessageControl* m = &cMes;
        int i;

        for (i = 0; i < 16; i++) {
            m->Delete(i);
        }
    }
    r221_moveShutter(0, 0);
    CamCtrl.CutCall(6);
    {
        cObj* o = SmdGetObjPtr(0x41);

        if (o) {
            r221_work->elvSe0 = SndCall(6, 0, &o->pos, 0, 0, 0);
        }
    }
    SCE_TASK* wire = SceExec(0x12, (TaskFunc) r221_moveWire, 0, 2, SCE_PRIO_DEF_2, 0);
    r221_work->wireTask2 = wire;
    pG->Room_flg[0] |= 0x10000000;
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(4);
    r221_setElevatorEff(0);
    pG->Room_flg[0] |= 0x08000000;
    SceSleep(15);
    r221_setElevatorEff(1);
    pG->Room_flg[0] |= 0x04000000;
    SceMesSet(4, 0x30, 1, 0x64, R221_MES_Y);
    SceMesWait();
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r221_checkSwitchboard_end();
}

// Task: the boss appears from the ceiling (the tail).
static void r221_appearBosstail()
{
    RsfSet(G_ROOM_ID, 4);
    cEmWrap em;
    em.setPtr(0x8D, -1, 1);
    em.setFlag(1);
    setEm(0x8C, -1, 1, 1, 1);
    SceAtSetEnable(9, 0);
    SceAtSetEnable(0xA, 0);
    SceAtSetEnable(0xB, 0);
    SceAtSetEnable(0xC, 0);
    while (1) {
        if (RsfCheck(G_ROOM_ID, 3)) {
            SceExit();
        }
        if (pG->Room_flg[0] & 0x40000000) {
            break;
        }
        SceSleep(1);
    }
    r221_work->bossStr = SndStrReq(0, 0x23, 0x80000003, 0, 0, 0.0f);
}

// Task: the elevator call button before the power is on.
static void r221_checkElevator()
{
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        if (RsfCheck(G_ROOM_ID, 4) == 0) {
            SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) r221_appearBosstail, 0, 1);
        }
        SceEventStart(0);
        SceMesSet(0, 0, 1, 0x64, R221_MES_Y);
        if (SceMesGetSelection() == 1) {
            SceMesSet(1, 0, 1, 0x64, R221_MES_Y);
        }
        SceEventEnd(0);
    } else if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceMesSet(6, 0, 1, 0x64, R221_MES_Y);
    }
}

// Arm area 0xA with the boss's ceiling appearance unless it already happened (Room_flg bit 4).
static void r221_appearBosstail1_sub()
{
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceAtDataSet_exec(0xA, SCE_LEVEL10, 0, (TaskFunc) r221_appearBosstail, 0, 1);
    }
}

// The boss arena setup: the switchboard lever (area 0xD until pulled, bit 5), the shutter, the door
// to r229 (breaks on area 0x1A until bit 9, then the Japanese message), the elevator and its doors /
// wires per the saved state, the boss appearance areas and the boss BGM / fight state per the flags.
void r221_initInsectboss()
{
    u32 id;

    r221_initSwitchboardLever();
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceAtDataSet_exec(0xD, SCE_LEVEL10, 0, (TaskFunc) r221_checkSwitchboard, 0, 1);
    } else {
        r221_moveSwitchboardLever(1);
    }
    r221_initShutter();
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtDataSet_exec(0x1A, SCE_LEVEL10, 0, (TaskFunc) r221_breakDoor229, 0, 1);
        id = 0x45;
    } else {
        if (pSys->language == 0) {
            SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r221_checkDoor229, 0, 1);
        } else {
            SceAtSetEnable(0, 0);
        }
        id = 0x44;
    }
    {
        cObj* o = SmdGetObjPtr(id);

        if (o) {
            o->be_flag &= ~2;
        }
    }
    {
        cObj* o9 = SmdGetObjPtr(9);
        cObj* o10 = SmdGetObjPtr(0xA);

        if (o9 && o10) {
            Vec a = {3277.0f, o9->pos.y, 4196.0f};
            Vec b = {3305.0f, o10->pos.y, 4212.0f};
            Vec da;
            Vec db;

            PSVECSubtract(&a, &o9->pos, &da);
            PSVECSubtract(&b, &o10->pos, &db);
            r221_work->door0.initMove1_pos(o9, 60, &da, 30.0f, 20.0f);
            r221_work->door1.initMove1_pos(o10, 30, &db, 30.0f, 20.0f);
            r221_work->door0.setVibration(10, 10, 1.0f, 0.3f, 1.0f);
            r221_work->door1.setVibration(10, 10, 1.0f, 0.3f, 1.0f);
        }
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        cObj* e = SmdGetObjPtr(0x41);

        if (e) {
            e->be_flag &= ~2;
            r221_work->elvY = e->pos.y;
        }
        SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) r221_checkElevator, 0, 1);
        if (RsfCheck(G_ROOM_ID, 5)) {
            SceExec(0x12, (TaskFunc) r221_checkElevatorArrive, 0, 0, SCE_PRIO_DEF_2, 0);
        }
        r221_moveElevatoDoor(0, 1);
    } else {
        if (pG->room_id_prev == 0x220 && FlagChkSignW(pG->System_flg, SYS_LOAD_GAME) == 0 && FlagChkSignW(pG->System_flg, SYS_CONTINUE) == 0) {
            r221_moveElevatoDoor(0, 1);
            SceExec(0x12, (TaskFunc) r221_moveElevator, 1, 0, SCE_PRIO_DEF_2, 0);
        } else {
            r221_moveElevatoDoor(1, 1);
        }
        r221_setElevatorEff(0);
        r221_setElevatorEff(1);
        r221_setElevatorEff(2);
        r221_setElevatorEff(3);
        r221_setElevatorEff(4);
        r221_setElevatorEff(5);
        r221_setElevatorEff(6);
        r221_setElevatorEff(7);
        r221_setElevatorEff(8);
    }
    SceAtDataSet_exec(0x16, SCE_LEVEL10, 0, (TaskFunc) r221_operateElevator, 0, 1);
    SceAtSetActColor(0x16, 1);
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceAtDataSet_exec(0xB, SCE_LEVEL10, 0, (TaskFunc) r221_appearBosstail, 0, 1);
        SceAtDataSet_exec(0xC, SCE_LEVEL10, 0, (TaskFunc) r221_appearBosstail1_sub, 0, 1);
    } else {
        if (RsfCheck(G_ROOM_ID, 3) == 0) {
            setEm(0x8D, -1, 1, 1, 1);
        } else {
            cEmWrap em;

            em.setEm(0x8C, -1, 0, 1, 1);
        }
        if (!FlagChk(pG->Em_flg[5], 140)) {
            SceAtDataSet_exec(0x19, SCE_LEVEL10, 0, (TaskFunc) r221_appearBoss2nd, 0, 1);
        }
    }
    EmReadSearch(0x2C, 0, 0);
}

// A gas bomb: the scroll object is hidden and a copy placed as a scroll dummy.
cObj* r201_setBonbe(int id, f32 ang)
{
    cObj* o = SmdGetObjPtr(id);

    if (o) {
        o->be_flag &= ~2;
        Vec rot = {0.0f, 0.0f, 0.0f};
        rot.y = LIMIT_ANGLE(ang);
        cObj* s = SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21), &o->pos, &rot, 0x10, 1);
        s->be_flag |= 0x1000;
        return s;
    }
    return 0;
}

// The four gas bombs (objects 0x1E/0x20/0x1F/0x21 at their yaws) as throwable scroll dummies; their
// throw areas 1/2/3/0x10 use action button kind 1 (before the boss, Room_flg bit 3) or 0x35, areas
// 4/5/6/0x11 off; the bombs already thrown (per flags) are removed.
void r201_initBonbe()
{
    r221_work->bonbe[0] = r201_setBonbe(0x1E, 0.15280247f);
    r221_work->bonbe[1] = r201_setBonbe(0x20, 3.85f);
    r221_work->bonbe[2] = r201_setBonbe(0x1F, 5.471593f);
    r221_work->bonbe[3] = r201_setBonbe(0x21, 3.1518683f);
    SceAtSetEnable(4, 0);
    SceAtSetEnable(5, 0);
    SceAtSetEnable(6, 0);
    SceAtSetEnable(0x11, 0);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtPtr(1)->actBtnKind = 1;
        SceAtPtr(2)->actBtnKind = 1;
        SceAtPtr(3)->actBtnKind = 1;
        SceAtPtr(0x10)->actBtnKind = 1;
    } else {
        SceAtPtr(1)->actBtnKind = 0x35;
        SceAtPtr(2)->actBtnKind = 0x35;
        SceAtPtr(3)->actBtnKind = 0x35;
        SceAtPtr(0x10)->actBtnKind = 0x35;
        SceAtPtr(1)->trigger |= 0x80;
        SceAtPtr(2)->trigger |= 0x80;
        SceAtPtr(3)->trigger |= 0x80;
        SceAtPtr(0x10)->trigger |= 0x80;
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r201_throwBonbe, 0, 1);
    } else {
        SceAtSetEnable(0x12, 0);
        r221_work->bonbe[0]->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r201_throwBonbe, (void*) 1, 1);
    } else {
        SceAtSetEnable(0x13, 0);
        r221_work->bonbe[1]->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r201_throwBonbe, (void*) 2, 1);
    } else {
        SceAtSetEnable(0x14, 0);
        r221_work->bonbe[2]->be_flag &= ~2;
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtDataSet_exec(0x10, SCE_LEVEL10, 0, (TaskFunc) r201_throwBonbe, (void*) 3, 1);
    } else {
        SceAtSetEnable(0x15, 0);
        r221_work->bonbe[3]->be_flag &= ~2;
    }
}

// Task: the bomb sounds.
static void r221_callBonbeSe(int no)
{
    switch (no) {
    case 0:
        SndCall(6, 8, 0, 0, 0, 0);
        SceSleep(60);
        break;
    case 1:
        SndCall(6, 9, 0, 0, 0, 0);
        SceSleep(95);
        break;
    }
    SndCall(6, 0xA, 0, 0, 0, 0);
}

// Task: the boss's landing footsteps (SE 0xD then 0xE) 162 frames in.
static void r221_callFootSe()
{
    SceSleep(162);
    SndCall(5, 0xD, 0, 0, 0, 0);
    SceSleep(2);
    SndCall(5, 0xE, 0, 0, 0, 0);
}

// Task: the thrown bomb fades out.
static void r221_fadeoutBonbe(cObj* o)
{
    u32 i;

    SceSleep(60);
    for (i = 0; i < 90; i++) {
        o->invisible_factor = (f32) (90 - i) / 90.0f;
        SceSleep(1);
    }
    o->invisible_factor = 0.0f;
}

// Task: the player throws bomb `no` down the shaft.
static void r201_throwBonbe(int no)
{
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceMesSet(8, 0, 1, 0x64, R221_MES_Y);
        SceExit();
    }
    Vec pos = {0.0f, 0.0f, 0.0f};
    Mtx m;
    int atNo = 0;
    // COMPILER-DIFF: 2 (pin): the original keeps both `clrlwi r8,r16,24` for `(u8) eff0`; our combine
    // deletes them from reg_nonzero_bits (the OR of the four constant sets 0/2/4/0xB fits a byte).
    // That summary exists only for pseudos (set_nonzero_bits_and_sign_copies skips hard registers),
    // so pinning eff0 to its own r16 keeps the masks with no other change (no extra ref, same
    // global-alloc order for eff3/r17).
    register int eff0 asm("r16") = 0;
    int eff1 = 0;
    int eff2 = 0;
    int eff3 = 0;
    void* mot0 = 0;
    void* mot1 = 0;
    cObj* bonbe = 0;
    u32 t1 = 0;
    u32 t2 = 0;
    int k0;
    int k1;
    u32 i;
    // COMPILER-DIFF: candidate #17 (global-alloc priority tie eff2 vs the pG high). `evNo` is set
    // exactly once (case 0) and read once (SceEventStart), so update_equiv_regs folds the constant
    // into the argument move after sched1 and deletes the set; its only effect is to take the
    // post-RsfSet issue slot in sched1 so that eff2's `li` is issued later there (its live length
    // drops below the high's priority), while sched2 still puts `li eff2` right after the `bl`.
    int evNo;

    switch ((u32) no) {
    case 0:
        bonbe = r221_work->bonbe[0];
        atNo = 4;
        eff1 = 6;
        RsfSet(G_ROOM_ID, 0);
        evNo = 0; // COMPILER-DIFF: candidate #17 (see the declaration)
        eff2 = 1;
        eff3 = 9;
        t2 = 0;
        t1 = 0;
        mot0 = ROOM_ARC_PTR(pG->pRoom, 0x24);
        mot1 = ROOM_ARC_PTR(pG->pRoom, 0x25);
        pos.x = -17.89f;
        pos.y = 0.0f;
        pos.z = -1793.51f;
        SceAtSetEnable(0x12, 0);
        SceExec(0x12, (TaskFunc) r221_callBonbeSe, 0, 2, SCE_PRIO_DEF_2, 0);
        break;
    case 1:
        bonbe = r221_work->bonbe[1];
        atNo = 5;
        eff0 = 2;
        RsfSet(G_ROOM_ID, 1);
        eff1 = 7;
        eff2 = 3;
        eff3 = 0xA;
        t1 = 0x32;
        t2 = 0x5A;
        mot0 = ROOM_ARC_PTR(pG->pRoom, 0x22);
        mot1 = ROOM_ARC_PTR(pG->pRoom, 0x23);
        pos.x = -1410.2101f;
        pos.y = 0.0f;
        pos.z = -1350.1799f;
        SceAtSetEnable(0x13, 0);
        SceExec(0x12, (TaskFunc) r221_callBonbeSe, 1, 2, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r221_callFootSe, 0, 2, SCE_PRIO_DEF_2, 0);
        break;
    case 2:
        bonbe = r221_work->bonbe[2];
        atNo = 6;
        eff0 = 4;
        RsfSet(G_ROOM_ID, 2);
        eff1 = 8;
        eff2 = 5;
        eff3 = 9;
        t2 = 0;
        t1 = 0;
        mot0 = ROOM_ARC_PTR(pG->pRoom, 0x24);
        mot1 = ROOM_ARC_PTR(pG->pRoom, 0x25);
        pos.x = -17.89f;
        pos.y = 0.0f;
        pos.z = -1793.51f;
        SceAtSetEnable(0x14, 0);
        SceExec(0x12, (TaskFunc) r221_callBonbeSe, 0, 2, SCE_PRIO_DEF_2, 0);
        break;
    case 3:
        bonbe = r221_work->bonbe[3];
        atNo = 0x11;
        eff0 = 0xB;
        RsfSet(G_ROOM_ID, 7);
        eff1 = 0xD;
        eff2 = 0xC;
        eff3 = 0xA;
        t1 = 0x32;
        t2 = 0x5A;
        mot0 = ROOM_ARC_PTR(pG->pRoom, 0x22);
        mot1 = ROOM_ARC_PTR(pG->pRoom, 0x23);
        pos.x = -1410.2101f;
        pos.y = 0.0f;
        pos.z = -1350.1799f;
        SceAtSetEnable(0x15, 0);
        SceExec(0x12, (TaskFunc) r221_callBonbeSe, 1, 2, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) r221_callFootSe, 0, 2, SCE_PRIO_DEF_2, 0);
        break;
    }
    k0 = EspPullCoreKind();
    k1 = EspPullCoreKind();
    SceEventStart(evNo); // COMPILER-DIFF: candidate #17 (always 0, see the declaration)
    SceAtSetEnable(atNo, 1);
    pPL->setNoSuspend(1);
    bonbe->setNoSuspend(1);
    PlSetHand(1, 0);
    bonbe->ang.y = LIMIT_ANGLE(bonbe->ang.y);
    low_RotMatrix(m, &bonbe->ang);
    TransMatrix(m, &bonbe->pos);
    PSMTXMultVec(m, &pos, &pPL->pos);
    {
        cPlayer* pl = pPL;

        pl->setPos(&pl->pos);
        pl->setAng(&bonbe->ang);
    }
    if (mot0 != 0) {
        pPL->motionSet(mot0, 10, 0, 1, 0);
        if (mot1 != 0) {
            bonbe->motionSet(mot1, 10, 0, 1, 0);
        }
        i = 0;
        while (MotionGetState(pPL) != 4) {
            if (i == t1) {
                EstSet(bonbe, -1, 0, 0, EFF_ROOM, eff3, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            if (t2 != 0 && t2 == i) {
                EstSet(0, -1, 0, 0, EFF_ROOM, (u8) eff0, 1, (u8) k0, 0, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, eff1, 1, (u8) k1, 0, 0);
            }
            i++;
            SceSleep(1);
        }
    }
    if (t2 == 0) {
        EstSet(0, -1, 0, 0, EFF_ROOM, (u8) eff0, 1, (u8) k0, 0, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, eff1, 1, (u8) k1, 0, 0);
    }
    PlSetHand(0, 0);
    pPL->setNoSuspend(0);
    bonbe->setNoSuspend(0);
    SceEventEnd(0);
    pG->Room_flg[0] &= ~0x80000000;
    for (u32 j = 0; j < 300; j++) {
        if (pG->Room_flg[0] & 0x80000000) {
            SceAtSetEnable(atNo, 0);
        }
        pG->Room_flg[0] &= ~0x80000000;
        SceSleep(1);
    }
    SceAtSetEnable(atNo, 0);
    EffectEspDelete(0, (u8) k0, 0, 0);
    EffectEspgenDelete(0, (u8) k0, 0);
    EffectEfmDelete(0, (u8) k0, 0);
    EffectEspgenDelete(0, (u8) k1, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, (u8) eff2, 0, ESP_CORE_KIND_NONE, 0, 0);
    SceExec(0x12, (TaskFunc) r221_fadeoutBonbe, (int) bonbe, 0, SCE_PRIO_DEF_2, 0);
}

// The floor render target blended over object 0x28 (refraction shader 2).
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r221_texTbl;

    if (GetTexRenderMgr(&r221_work->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r221_work->tex->m_Tex_no;
        r221_work->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, EFF_ROOM, 0x1F, r221_work->tex->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x28);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->color[3] = 0xF0;
    obj->Shader_type = 2;
    obj->Refract_pow = 0x10;
    obj->Refract_ratio = 0x30;
}
