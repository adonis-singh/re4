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
#include "em_set.h"
#include "em_wrap.h"
#include "emBarred.h"
#include "etc_model.h"
#include "player.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "mes.h"
#include "item.h"
#include "sscrn.h"
#include "cam_ctrl.h"
#include "read.h"
#include "math_sub.h"
#include "cSceObj.h"

// Room 3-08 (D:/Bio4/Prog/r308.cpp): the switch that opens the sample container (treasure box), the giant
// that is set loose once the sample is taken (with a timer that ends the fight), the card key door and the
// treasure box with its lid.

struct R308Work {
    cEmWrap em;   // 0x00
    int timer;    // 0x0C
    int se;       // 0x10  SndCall handle of the container hum (-1 = none)
};

static R308Work* r308_work;


void R308OpenBoxMain(int type, int mode, int se, int id1, int id2, int itemNo, int seNo);
static void OpenedBoxTreasure(int id);
static void OpenBoxTreasure(int id);
static void R308CardKeyCheck();
static void R308CardKeyUse();
static void R308EnemySetCheck();
static void R308EnemySetMain();
static void R308EnemySetEnd();
void R308HandOpen();
static void R308EnemyDieCheck();
static void R308EnemyDieMain();
void R308EnemyDieEnd();
static void R308SwitchMain();
static void R308SwitchEnd();
static void R308DoorCheck();
static void SceBgmCheck();

// Room init: the sample container as a treasure item event (item 0x80, area 3); until the switch was
// used (Room_flg bit 0) area 1 = the switch, item area 3 off, area 2 on, the closed-container effect
// and hum; else open. The giant (ESL 0x58) pre-read; until it was set loose (bit 2) the sample-taken
// watcher; the card key door (area 4 + key watcher) and the barred door per flags; the battle stream.
void R308Init()
{
    void* zero = 0;
    cEm* barred;

#line 54 "D:/Bio4/Prog/r308.cpp"
    r308_work = (R308Work*) MEM_CALLOC(sizeof(R308Work), 1, 0xd);
    SceSetItemEvent(3, 0x80, 1, 3, OpenBoxTreasure, OpenedBoxTreasure, 0x80, 1);
    r308_work->se = -1;
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) R308SwitchMain, 0, 1);
        SceAtSetEnable(3, 0);
        SceAtSetEnable(2, 1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM00, zero, zero);
        r308_work->se = SndCall(6, 2, 0, 0, 0, 0);
    } else {
        SceAtSetEnable(3, 1);
        SceAtSetEnable(2, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x2001, ESP_CORE_KIND_ROOM01, zero, zero);
    }
    EmReadSearch((u8) GetEmIdFromList(0x58), 0, 0);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceExec(0x12, (TaskFunc) R308EnemySetCheck, 0, 0, 2, 0);
    } else {
        R308HandOpen();
        r308_work->em.setEm(0x58, -1, 0, 1, 1);
        r308_work->em.setFlag(1);
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) R308CardKeyCheck, 0, 1);
        SceExec(0x12, (TaskFunc) R308CardKeyUse, 0, 0, 2, 0);
        SceAtSetEnable(0x83, 0);
    }
    if (getRoomEtcBarred(0, &barred, 1)) {
        ((cEmBarred*) barred)->setNoClose();
    }
    SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, 2, 0);
}

// Per-frame room main: nothing.
void R308Main()
{
}

// Box types 0x13/0x14: the lid (id1) is parented to the box (id2) and rotated open over 40 frames (r40c).
void R308OpenBoxMain(int type, int mode, int se, int id1, int id2, int itemNo, int seNo)
{
    cObj* lid = NULL;
    cObj* box = NULL;

    if (id1 != -1) {
        lid = SmdGetObjPtr(id1);
    }
    if (id2 != -1) {
        box = SmdGetObjPtr(id2);
    }
    if (lid) {
        lid->be_flag |= 0x20;
    }
    if (box) {
        box->be_flag |= 0x20;
    }
    switch (type) {
    case 0x13:
    case 0x14:
        if (lid && box) {
            Vec d;
            Vec rot = {0.0f, 0.0f, 0.0f};

            PSVECSubtract(&box->pos, &lid->pos, &d);
            box->setParent(lid, &d, &rot);
        }
        break;
    }
    if (mode == 0) {
        if (seNo != -1) {
            SndCall(6, seNo, 0, 0, 0, 0);
        }
        switch (type) {
        case 0x13:
        case 0x14:
            if (lid && box) {
                cSceObj mov;
                Vec ang = {0.0f, 0.0f, -PI};

                mov.initMove1_ang(box, 40, &ang, 20.0f, 20.0f, 4);
                while (mov.move() == 1) {
                    SceSleep(1);
                }
                SceSleep(15);
            }
            break;
        }
        OpenBoxMain(type, mode, se, id1, id2, itemNo);
    } else {
        OpenBoxMain(type, mode, se, id1, id2, itemNo);
    }
}

// Item-event "already opened": the container lid (0x15 on box 0x14) posed open.
static void OpenedBoxTreasure(int id)
{
    if (id == 0x80) {
        R308OpenBoxMain(0x13, 1, 10, 0x15, 0x14, -1, 9);
    }
}

// Item-event opener: the container lid swings open (40 frames, SE 10 / 9).
static void OpenBoxTreasure(int id)
{
    if (id == 0x80) {
        R308OpenBoxMain(0x13, 0, 10, 0x15, 0x14, -1, 9);
    }
}

// Area 4: the card key door is locked.
static void R308CardKeyCheck()
{
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceUpCut(8, 8, -1, 4);
        if (ItemMgr.num(0x84) == 0) {
            CamCtrl.Comeback(0);
        } else {
            SubScreenOpen(0x80, 1);
            CamCtrl.Comeback(0);
        }
    }
}

// The card key is used up on the door.
static void R308CardKeyUse()
{
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        while (ItemMgr.check(0x84) != 1) {
            SceSleep(1);
        }
        SceUpCut(9, 8, 0xB, 4);
        ItemMgr.dump(0x84);
        RsfSet(G_ROOM_ID, 3);
        SceAtSetEnable(4, 0);
        SceAtSetEnable(5, 0);
        SceAtSetEnable(0x83, 1);
        SceAtExecute(0x83);
        CamCtrl.Comeback(0);
    }
}

// Once the sample is in the inventory the giant is set loose.
static void R308EnemySetCheck()
{
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        while (ItemMgr.num(0xC5) == 0) {
            SceSleep(1);
        }
        SceExec(0x12, (TaskFunc) R308EnemySetMain, 0, 0, 2, 0);
    }
}

// Once (Room_flg bit 2): battle stream, Status_flg[2] 0x02000000, area 0 = the door message; up-cut
// 5/4/6 and camera cut 5 while the container hands open, the giant (0x58) is set alerted, cut 7; cancellable.
static void R308EnemySetMain()
{
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        RsfSet(G_ROOM_ID, 2);
        SndRoomStrStart(1, 0, 1);
        SceEventStart(0);
        StaFlagOn(pG, STA_ESP_COMPULSION_NOSUSPEND);
        r308_work->timer = 0;
        SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) R308DoorCheck, 0, 1);
        SceUpCut(5, 4, 6, 0);
        SceSetEventCancel(1, (TaskFunc) R308EnemySetEnd, 0, -1, 1);
        CamCtrl.CutCall(5);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        R308HandOpen();
        r308_work->em.setEm(0x58, -1, 0, 1, 1);
        r308_work->em.setFlag(1);
        r308_work->em.setNoSuspend(1);
        CamCtrl.CutCall(7);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
        R308EnemySetEnd();
    }
}

// End of the giant's release (also its cancel path): the giant set alerted and free to suspend, its
// ESL set byte cleared, the fight timer task, the hands open, SceEventEnd, Status_flg[2] bit off.
static void R308EnemySetEnd()
{
    r308_work->em.setEm(0x58, -1, 0, 1, 1);
    r308_work->em.setFlag(1);
    r308_work->em.setNoSuspend(0);
    pG->Em_list[0x58].set = 0;
    SceExec(0x12, (TaskFunc) R308EnemyDieCheck, 0, 0, 2, 0);
    R308HandOpen();
    SceEventEnd(0);
    StaFlagOff(pG, STA_ESP_COMPULSION_NOSUSPEND);
    SceExit();
}

// The two hands of the container open.
void R308HandOpen()
{
    cObj* obj;
    Vec ang;

    obj = SmdGetObjPtr(0xA);
    if (obj) {
        f32 rx = obj->ang.x;
        f32 ry = obj->ang.y;

        ang.x = rx;
        ang.y = ry;
        ang.z = PI / 4.0f;
        obj->setAng(&ang);
    }
    obj = SmdGetObjPtr(0xB);
    if (obj) {
        f32 rx = obj->ang.x;
        f32 ry = obj->ang.y;

        ang.x = rx;
        ang.y = ry;
        ang.z = -PI / 4.0f;
        obj->setAng(&ang);
    }
}

// The fight timer: 1800 frames or the giant's death end it (a short wait after the death).
static void R308EnemyDieCheck()
{
    int i;

    SceSleep(1);
    for (;;) {
        SceDebugDisp("Timer:[%d/%d]", r308_work->timer, 1800);
        if (r308_work->timer > 1799) {
            break;
        }
        if (r308_work->em.getHp() <= 0) {
            break;
        }
        r308_work->timer++;
        SceSleep(1);
    }
    if (r308_work->em.getHp() <= 0) {
        for (i = 0; i < 270; i++) {
            SceDebugDisp("TimerWait:[%d/%d]", i, 270);
            SceSleep(1);
        }
    }
    SceExec(0x12, (TaskFunc) R308EnemyDieMain, 0, 0, 2, 0);
}

// The fight is over: once the player is idle, the message camera set 7 (time ran out at 1800 frames)
// or 6 (the giant died), then the common end.
static void R308EnemyDieMain()
{
    cPlayer* pl = pPL;

    while (pl->checkEvent() != 1) {
        SceSleep(1);
    }
    SceEventStart(1);
    if (r308_work->timer > 1799) {
        SceMesCamSndSet(7, 4, 7, 0);
    } else {
        SceMesCamSndSet(6, 4, 7, 0);
    }
    R308EnemyDieEnd();
}

// End of the fight: area 0 reset, camera back, SceEventEnd.
void R308EnemyDieEnd()
{
    SceAtDataReset(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// The three effect deletes of one effect slot.
#define R308_EFF_DELETE(no)                       \
    EffectEspDelete(0x2001, no, 0, 0);            \
    EffectEspgenDelete(0x2001, no, 0);            \
    EffectEfmDelete(0x2001, no, 0)

// Area 1: the switch on the container.
static void R308SwitchMain()
{
    void* zero = 0;

    SceEventStart(1);
    R308_EFF_DELETE(2);
    R308_EFF_DELETE(3);
    R308_EFF_DELETE(5);
    EstSet(0, -1, 0, 0, EFF_ROOM, 2, 0x2001, ESP_CORE_KIND_ROOM02, zero, zero);
    CamCtrl.CutCall(2);
    SceMesSet(0, 0x20, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    if (SceMesGetSelection() != 1) {
        R308_EFF_DELETE(3);
        R308_EFF_DELETE(4);
        R308_EFF_DELETE(5);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM00, zero, zero);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
        SceExit();
    } else {
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            cModel* m;
            cObj* obj;

            RsfSet(G_ROOM_ID, 0);
            SceAtSetEnable(1, 0);
            SceSetEventCancel(1, (TaskFunc) R308SwitchEnd, 0, -1, 1);
            m = SceAtItemModelPtr(0x80);
            if (m) {
                m->setNoSuspend(1);
            }
            obj = SmdGetObjPtr(0x16);
            if (obj) {
                SndCall(6, 0, &obj->pos, 0, 0, 0);
            }
            if (r308_work->se != -1) {
                SndStop(r308_work->se, 1);
                r308_work->se = -1;
            }
            SndCall(6, 4, 0, 0, 0, 0);
            SndCall(6, 5, 0, 0, 0, 0);
            CamCtrl.CutCall(6);
            R308_EFF_DELETE(2);
            R308_EFF_DELETE(3);
            R308_EFF_DELETE(4);
            EstSet(0, -1, 0, 0, EFF_ROOM, 3, 0x2001, ESP_CORE_KIND_ROOM03, 0, 0);
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            R308SwitchEnd();
        }
    }
}

// End of the switch event (also its cancel path): item area 3 on / 2 off, the container effects swapped
// to the open one, the hum stopped, the sample model may suspend, camera back, SceEventEnd, task exit.
static void R308SwitchEnd()
{
    cModel* m;

    SceAtSetEnable(3, 1);
    SceAtSetEnable(2, 0);
    R308_EFF_DELETE(2);
    R308_EFF_DELETE(4);
    R308_EFF_DELETE(5);
    EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x2001, ESP_CORE_KIND_ROOM01, 0, 0);
    if (r308_work->se != -1) {
        SndStop(r308_work->se, 0);
        r308_work->se = -1;
    }
    m = SceAtItemModelPtr(0x80);
    if (m) {
        m->setNoSuspend(0);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Area 0 during the fight: up-cut 4/4/8 (the door is sealed).
static void R308DoorCheck()
{
    SceUpCut(4, 4, 8, 0);
}

// The battle stream while a Ganado has found the player.
static void SceBgmCheck()
{
    int on = 0;

    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}
