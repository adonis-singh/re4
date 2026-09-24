#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emBarred.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "esp.h"
#include "est.h"
#include "snd.h"

// Room 4-0D (D:/Bio4/Prog/r40d.cpp): the terminal that locks the two barred doors, the key item
// and the enemy waves.

struct R40dWork {
    cEmBarred* door[2];   // 0x00
    int eff[3];           // 0x08  EspPullCoreKind ids: the two door effects and the terminal one
    SCE_TASK* se;          // 0x14  the terminal sound task
};


static R40dWork* r40d_work;

void r40d_openShelf_main(int no, int mode);
static void r40d_openedShelf(int no);
static void r40d_openShelf(int no);
static void r40d_changeEmSet();
static void r40d_getItem();
void r40d_setEmA();
void r40d_setEmB();
void r40d_setEmC();
static void r40d_checkEmSetC();
static void r40d_operateTerminal_end();
static void r40d_operateTerminal();
static void r40d_callTerminalSe();
void r40d_setDoorEff(int no, int on);
static void r40d_execDoorLock_end();
static void r40d_execDoorLock();

// Room init (the terminal room): the two barred doors and three effect kinds; until the terminal was
// used (Room_flg bit 0) both doors show the locked effect, area 2 = the terminal, areas 3/4 off; after
// it door 1's exit hook switches the next room's enemies (bit 5), the door effects per bits 1/2, the
// waves A/B/C per bits 2/3/4 and the terminal SE; the key item (area 0x81, camera on area 7) until
// taken; the shelf item event.
void R40dInit()
{
#line 35 "D:/Bio4/Prog/r40d.cpp"
    r40d_work = (R40dWork*) MEM_CALLOC(sizeof(R40dWork), 1, 0xd);
    r40d_work->se = 0;
    getRoomEtcBarred(0, (cEm**) &r40d_work->door[0], 1);
    getRoomEtcBarred(1, (cEm**) &r40d_work->door[1], 1);
    r40d_work->eff[0] = EspPullCoreKind();
    r40d_work->eff[1] = EspPullCoreKind();
    r40d_work->eff[2] = EspPullCoreKind();
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        r40d_setDoorEff(0, 1);
        r40d_setDoorEff(1, 1);
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r40d_operateTerminal, 0, 1);
        SceAtSetEnable(3, 0);
        SceAtSetEnable(4, 0);
    } else {
        if (RsfCheck(G_ROOM_ID, 5) == 0) {
            SceAtSetDoorFunc(1, (TaskFunc) r40d_changeEmSet, 0);
        }
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            r40d_setDoorEff(0, 0);
            r40d_setDoorEff(1, 0);
            SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r40d_operateTerminal, 0, 1);
            if (r40d_work->door[0]) {
                r40d_work->door[0]->setLockMode(1);
            }
            if (r40d_work->door[1]) {
                r40d_work->door[1]->setLockMode(1);
            }
            r40d_work->se = SceExec(0x12, (TaskFunc) r40d_callTerminalSe, 0, 0, SCE_PRIO_DEF_2, 0);
        } else {
            r40d_setDoorEff(0, 1);
            r40d_setDoorEff(1, 1);
            SceAtSetEnable(3, 0);
            SceAtSetEnable(4, 0);
            if (RsfCheck(G_ROOM_ID, 4) == 0) {
                SceExec(0x12, (TaskFunc) r40d_checkEmSetC, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
    if (!ItfFlagChk(pG, ITF_R40D_SAMPLE00)) {
        cModel* m;

        SceAtSetEnable(0x81, 1);
        m = SceAtItemModelPtr(0x81);
        if (m) {
            m->setNoSuspend(1);
        }
        SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r40d_getItem, 0, 1);
    }
    SceSetItemEvent(8, 0x83, 6, 5, r40d_openShelf, r40d_openedShelf, 1, 0);
}

// Per-frame room main: nothing.
void R40dMain()
{
}

// Shelf 1 (duralumin case 0x30, lid up +Z) opens (mode 1: snap).
void r40d_openShelf_main(int no, int mode)
{
    if (no == 1) {
        OpenBoxMain(OpenBoxPartsUpZP, mode, 0x18, 0x30, -1, -1);
    }
}

// Item-event "already opened": the shelf posed open.
static void r40d_openedShelf(int no)
{
    r40d_openShelf_main(no, 1);
}

// Item-event opener: animate the shelf open.
static void r40d_openShelf(int no)
{
    r40d_openShelf_main(no, 0);
}

// Door 1: the enemies of the next room are switched on.
static void r40d_changeEmSet()
{
    RsfSet(G_ROOM_ID, 5);
    SceDestroyEm(0x10, 0x20);
    EmListSetAlive(0x89, 1);
    EmListSetAlive(0x8A, 1);
    EmListSetAlive(0x8B, 1);
}

// The key item: camera cut 9, then the pick-up locks the doors.
static void r40d_getItem()
{
    cModel* m;

    SceEventStart(0);
    LightMgr.onKind(0x7F);
    m = SceAtItemModelPtr(0x81);
    if (m) {
        m->setNoSuspend(1);
    }
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceAtExecute(0x81);
    SceSleep(1);
    if (SceAtItemFlgCk(0x81) == 1) {
        SceAtSetEnable(7, 0);
        SceExec(0x12, (TaskFunc) r40d_execDoorLock, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Wave A once (Room_flg bit 2): four alerted Ganados (0x95..0x98) after the terminal.
void r40d_setEmA()
{
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        RsfSet(G_ROOM_ID, 2);
        setEm(0x95, -1, 1, 1, 1);
        setEm(0x96, -1, 1, 1, 1);
        setEm(0x97, -1, 1, 1, 1);
        setEm(0x98, -1, 1, 1, 1);
    }
}

// Wave B once (bit 3): three Ganados (0x9A..0x9C) after the doors lock.
void r40d_setEmB()
{
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        RsfSet(G_ROOM_ID, 3);
        setEm(0x9A, -1, 1, 1, 1);
        setEm(0x9B, -1, 1, 1, 1);
        setEm(0x9C, -1, 1, 1, 1);
    }
}

// Wave C once (bit 4): three Ganados (0x90..0x92) when the player reaches area 5.
void r40d_setEmC()
{
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        RsfSet(G_ROOM_ID, 4);
        setEm(0x90, -1, 1, 1, 1);
        setEm(0x91, -1, 1, 1, 1);
        setEm(0x92, -1, 1, 1, 1);
    }
}

// Task: wave C when the player enters area 5.
static void r40d_checkEmSetC()
{
    while (SceAtHitCheck(5) != 1) {
        SceSleep(1);
    }
    r40d_setEmC();
}

// End of the terminal event (also its cancel path): both door effects to "locked", camera back, the SE
// task killed, SceEventEnd, wave A and the wave-C watcher.
static void r40d_operateTerminal_end()
{
    r40d_setDoorEff(0, 1);
    r40d_setDoorEff(1, 1);
    CamCtrl.Comeback(0);
    if (r40d_work->se) {
        SceKill(r40d_work->se);
    }
    SceEventEnd(0);
    r40d_setEmA();
    SceExec(0x12, (TaskFunc) r40d_checkEmSetC, 0, 0, SCE_PRIO_DEF_2, 0);
}

// The terminal: confirm, then both doors unlock with a camera cut each.
static void r40d_operateTerminal()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceMesSet(2, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
        SceExit();
    }
    SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case 1:
    default:
        SndCall(6, 7, 0, 0, 0, 0);
        SceAtSetEnable(2, 0);
        RsfSet(G_ROOM_ID, 1);
        SceAtSetEnable(3, 0);
        SceAtSetEnable(4, 0);
        if (r40d_work->door[0]) {
            r40d_work->door[0]->setLockMode(0);
        }
        if (r40d_work->door[1]) {
            r40d_work->door[1]->setLockMode(0);
        }
        SceSetEventCancel(1, (TaskFunc) r40d_operateTerminal_end, 0, -1, 1);
        SceEventStart(1);
        CamCtrl.CutCall(6);
        SceSleep(15);
        SndCall(6, 6, 0, 0, 0, 0);
        r40d_setDoorEff(0, 1);
        SceSleep(10);
        SndCall(6, 5, 0, 0, 0, 0);
        SceSleep(15);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(7);
        SceSleep(15);
        SndCall(6, 6, 0, 0, 0, 0);
        r40d_setDoorEff(1, 1);
        SceSleep(10);
        SndCall(6, 5, 0, 0, 0, 0);
        SceSleep(15);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        r40d_operateTerminal_end();
        break;
    case -1:
    case 0:
    case 2:
        break;
    }
}

// Task: the terminal's beep (SE 9) every 30 frames at its position.
static void r40d_callTerminalSe()
{
    SceSleep(1);
    Vec pos = {-5257.0f, 1000.0f, -2704.0f};
    for (;;) {
        SndCall(6, 9, &pos, 0, 0, 0);
        SceSleep(30);
    }
}

// The lock effect of door `no` (on: the locked-in effect, off: the two open ones).
void r40d_setDoorEff(int no, int on)
{
    int eff;
    int a;
    int b;

    switch (no) {
    case 0:
    default:
        eff = r40d_work->eff[0];
        a = 3;
        b = 1;
        break;
    case 1:
        eff = r40d_work->eff[1];
        a = 4;
        b = 2;
        break;
    }
    EffectEspDelete(0, (u8) eff, 0, 0);
    EffectEspgenDelete(0, (u8) eff, 0);
    EffectEfmDelete(0, (u8) eff, 0);
    EffectEspDelete(0, (u8) r40d_work->eff[2], 0, 0);
    EffectEspgenDelete(0, (u8) r40d_work->eff[2], 0);
    EffectEfmDelete(0, (u8) r40d_work->eff[2], 0);
    if (on == 1) {
        EstSet(0, -1, 0, 0, EFF_ROOM, a, 1, (u8) eff, 0, 0);
    } else {
        void* zero = 0;

        EstSet(0, -1, 0, 0, EFF_ROOM, b, 1, (u8) eff, zero, zero);
        EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, (u8) r40d_work->eff[2], zero, zero);
    }
}

// End of the door-lock event (also its cancel path): both door effects to "open", camera back,
// SceEventEnd, wave B, the terminal SE task.
static void r40d_execDoorLock_end()
{
    r40d_setDoorEff(0, 0);
    r40d_setDoorEff(1, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r40d_setEmB();
    r40d_work->se = SceExec(0x12, (TaskFunc) r40d_callTerminalSe, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Both doors lock with a camera cut each once the key item was taken.
static void r40d_execDoorLock()
{
    RsfSet(G_ROOM_ID, 0);
    SceAtSetDoorFunc(1, (TaskFunc) r40d_changeEmSet, 0);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r40d_operateTerminal, 0, 1);
    SceAtSetEnable(3, 1);
    SceAtSetEnable(4, 1);
    if (r40d_work->door[0]) {
        r40d_work->door[0]->setLockMode(1);
        r40d_work->door[0]->setClosed();
    }
    if (r40d_work->door[1]) {
        r40d_work->door[1]->setLockMode(1);
        r40d_work->door[1]->setClosed();
    }
    SceSetEventCancel(1, (TaskFunc) r40d_execDoorLock_end, 0, -1, 1);
    SceEventStart(1);
    CamCtrl.CutCall(6);
    SceSleep(15);
    SndCall(6, 6, 0, 0, 0, 0);
    r40d_setDoorEff(0, 0);
    SceSleep(10);
    SndCall(6, 4, 0, 0, 0, 0);
    SceSleep(15);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(7);
    SceSleep(15);
    SndCall(6, 6, 0, 0, 0, 0);
    r40d_setDoorEff(1, 0);
    SceSleep(10);
    SndCall(6, 4, 0, 0, 0, 0);
    SceSleep(15);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r40d_execDoorLock_end();
}
