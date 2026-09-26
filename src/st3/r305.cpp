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
#include "em.h"
#include "em_wrap.h"
#include "snd.h"
#include "rnd.h"

// Room 3-05 (D:/Bio4/Prog/r305.cpp): the shutter that the bomber Ganado open and close, the enemies reset
// to their start positions while it is closed, and the three item boxes.

// The shutter: a scroll object moved up (open) / down (close) with a shake at the end, its own collision
// pieces (SatMgr / EatMgr) toggled with it.
class cR305Shutter {
public:
    u8 mode;         // 0x00  0 wait, 1 open, 2 close (index into r305_shutter_tbl)
    u8 step;         // 0x01
    cModel* obj;     // 0x04
    Vec basePos;     // 0x08
    f32 height;      // 0x14
    f32 spd;         // 0x18
    int enable;      // 0x1C
    cSat* sat;       // 0x20
    cSat* eat;       // 0x24
    int status;      // 0x28  0 closed, 1 opened, 4 moving
    int shakeCnt;    // 0x2C
    u32 se;          // 0x30

    void init();
    void move();
    void wait();
    void open();
    void close();
    void setOpen();
    void setClose();
    int getStatus();
    void setOpened();
};

struct R305Work {
    cEmWrap em[5];         // 0x00
    Vec pos[5];            // 0x3C
    Vec ang[5];            // 0x78
    cR305Shutter shutter;  // 0xB4
    cSat* sat;             // 0xE8
};

static R305Work* r305_work;

static void (cR305Shutter::*r305_shutter_tbl[3])() = {&cR305Shutter::wait, &cR305Shutter::open, &cR305Shutter::close};

// Reference store: the work pointer is reloaded after it.

static void r305_GanadoDieCheck();
static void r305_RoomExitFunc();
static void r305_ItemBoxOpen(u32 no);
static void r305_ItemBoxOpened(u32 no);   // u32 (not int): an int parameter reorders the unit's functions
static void r305_ShutterCtrl();

// Room init: the shutter (posed open once Room_flg bit 1, else area 3 = the bomber Ganado's shutter
// control); after the first wave is done (bit 0) two Ganados (0x47/0x48, list 6), else the shutter
// collision, the five first-wave Ganados (list 6) with their start poses saved and the death watcher;
// the exit hook; three item boxes.
void R305Init()
{
#line 37 "D:/Bio4/Prog/r305.cpp"
    r305_work = (R305Work*) MEM_CALLOC(sizeof(R305Work), 1, 0xd);
    r305_work->shutter.init();
    if (RsfCheck(G_ROOM_ID, 1)) {
        r305_work->shutter.setOpened();
    } else {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r305_ShutterCtrl, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 0)) {
        setEm(0x47, 6, 0, 1, 0);
        setEm(0x48, 6, 0, 1, 0);
    } else {
        Vec zero = {0.0f, 0.0f, 0.0f};

        r305_work->sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 2);
        if (r305_work->em[0].setEm(0x2A, 6, 0, 1, 0)) {
            r305_work->em[0].getPos(&r305_work->pos[0]);
            r305_work->em[0].getAng(&r305_work->ang[0]);
        }
        if (r305_work->em[1].setEm(0x2C, 6, 0, 1, 0)) {
            r305_work->em[1].getPos(&r305_work->pos[1]);
            r305_work->em[1].getAng(&r305_work->ang[1]);
        }
        setEm(0xBC, 6, 0, 1, 0);
        SceExec(0x12, (TaskFunc) r305_GanadoDieCheck, 0, 0, 2, 0);
    }
    SceAtSetDoorFunc(1, (TaskFunc) r305_RoomExitFunc, 0);
    SceSetItemEvent(6, 0x81, 2, 1, (void (*)(int)) r305_ItemBoxOpen, (void (*)(int)) r305_ItemBoxOpened, 0x14, 0);
    SceSetItemEvent(7, 0x85, 3, 2, (void (*)(int)) r305_ItemBoxOpen, (void (*)(int)) r305_ItemBoxOpened, 0x16, 0);
    SceSetItemEvent(8, 0x86, 4, 3, (void (*)(int)) r305_ItemBoxOpen, (void (*)(int)) r305_ItemBoxOpened, 0x17, 0);
}

// Per frame: step the shutter state machine.
void R305Main()
{
    r305_work->shutter.move();
}

// Once the first two Ganado are dead and the shutter is closed, the second wave appears behind it.
static void r305_GanadoDieCheck()
{
    while (r305_work->em[0].isActive() != 0 || r305_work->em[1].isActive() != 0) {
        SceSleep(1);
    }
    while (r305_work->shutter.getStatus() != 0) {
        SceSleep(1);
    }
    if (r305_work->em[2].setEm(0x2B, 6, 0, 1, 0)) {
        r305_work->em[2].getPos(&r305_work->pos[2]);
        r305_work->em[2].getAng(&r305_work->ang[2]);
    }
    if (r305_work->em[3].setEm(0x46, 6, 0, 1, 0)) {
        r305_work->em[3].getPos(&r305_work->pos[3]);
        r305_work->em[3].getAng(&r305_work->ang[3]);
    }
    if (r305_work->em[4].setEm(0xC7, 6, 0, 1, 0)) {
        r305_work->em[4].getPos(&r305_work->pos[4]);
        r305_work->em[4].getAng(&r305_work->ang[4]);
    }
    while (r305_work->em[2].isActive() != 0 || r305_work->em[3].isActive() != 0 || r305_work->em[4].isActive() != 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 1);
}

// Room exit hook: Room_flg bit 0 (the first wave counts as done).
static void r305_RoomExitFunc()
{
    RsfSet(G_ROOM_ID, 0);
}

// Item-event opener: box `no` (0x14 double door, 0x17 / 0x16 lidded boxes with their items) opens.
static void r305_ItemBoxOpen(u32 no)
{
    switch (no) {
    case 0x14:
        OpenBoxMain(0, 0, 2, 0x14, 0x15, -1);
        break;
    case 0x17:
        OpenBoxMain(0xF, 0, 1, 0x17, -1, 0x86);
        break;
    case 0x16:
        OpenBoxMain(0x12, 0, 1, 0x16, -1, 0x85);
        break;
    }
}

// Item-event "already opened": box `no` posed open.
static void r305_ItemBoxOpened(u32 no)
{
    switch (no) {
    case 0x14:
        OpenBoxMain(0, 1, 2, 0x14, 0x15, -1);
        break;
    case 0x17:
        OpenBoxMain(0xF, 1, 1, 0x17, -1, 0x86);
        break;
    case 0x16:
        OpenBoxMain(0x12, 1, 1, 0x16, -1, 0x85);
        break;
    }
}

// Area 3: the shutter control. Closed: the enemies are put back and it opens again once the player has left
// the area; opened: a bomber ready to throw or a bowgun shot closes it after a short delay; then it stays
// closed for a random while.
static void r305_ShutterCtrl()
{
    int tbl[5] = {0, 1, 2, 3, 4};
    int wait = 0;
    u32 timer = 0;
    u32 mode = 0;
    int i;
    int st;
    cEmGanado* em;

    r305_work->shutter.setOpen();
    do {
        switch (mode) {
        case 0:
            st = r305_work->shutter.getStatus();
            if (st != 0) {
                if (st == 1) {
                    if (r305_work->em[0].isActive()) {
                        em = (cEmGanado*) r305_work->em[0].getPtr();
                        if (em->ckR305BomberEnable() == 1) {
                            r305_work->em[0].setFlag(1);
                            timer = 40;
                        }
                    }
                    if (r305_work->em[1].isActive()) {
                        em = (cEmGanado*) r305_work->em[1].getPtr();
                        if (em->ckR305BomberEnable() == 1) {
                            r305_work->em[1].setFlag(1);
                            timer = 40;
                        }
                    }
                    if (r305_work->em[2].isActive()) {
                        if (r305_work->em[2].ckBowgunFire() == 1) {
                            timer = 40;
                        }
                    }
                    if (r305_work->em[3].isActive()) {
                        if (r305_work->em[3].ckBowgunFire() == 1) {
                            timer = 40;
                        }
                    }
                    if (r305_work->em[4].isActive()) {
                        if (r305_work->em[4].ckBowgunFire() == 1) {
                            timer = 40;
                        }
                    }
                }
            } else {
                if (SceAtHitCheck(2) == 0) {
                    for (i = 0; i < 5; i++) {
                        if (r305_work->em[tbl[i]].isActive()) {
                            r305_work->em[tbl[i]].setPos(&r305_work->pos[tbl[i]]);
                            r305_work->em[tbl[i]].setAng(&r305_work->ang[tbl[i]]);
                        }
                    }
                    r305_work->shutter.setOpen();
                }
            }
            if (timer != 0) {
                mode++;
            }
            break;
        case 1:
            SceDebugDisp("T[%d]", timer);
            if (timer != 0) {
                timer--;
            } else {
                mode = 2;
                r305_work->shutter.setClose();
                wait = (u8) (Rnd() % 30u) + 180;
            }
            break;
        case 2:
            if (wait != 0) {
                wait--;
            } else {
                mode = 0;
            }
            break;
        }
        if (SceAtHitCheck(2) != 0) {
            if (timer == 0) {
                r305_work->shutter.setClose();
            } else if (timer > 30) {
                timer = 30;
            }
        }
        SceSleep(1);
    } while (RsfCheck(G_ROOM_ID, 1) == 0);
    r305_work->shutter.setOpen();
    SatMgr.destroy(r305_work->sat);
}

// The shutter (scroll object 2): base position, 2700-unit travel, its own collision / attribute pieces
// (archive 5 / 0x12 set 1) and blocking area 4 on.
void cR305Shutter::init()
{
    Vec zero = {0.0f, 0.0f, 0.0f};

    enable = 0;
    obj = SmdGetObjPtr(2);
    if (obj) {
        int on = 1;

        obj->be_flag |= 0x20;
        basePos = obj->pos;
        height = 2700.0f;
        sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &zero, &zero, 1);
        eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &zero, &zero, 1);
        SceAtSetEnable(4, 1);
        enable = on;
    }
}

// Per-frame step: run the current mode (wait / open / close) of r305_shutter_tbl.
void cR305Shutter::move()
{
    if (enable) {
        (this->*r305_shutter_tbl[mode])();
    }
}

// Mode 0: idle.
void cR305Shutter::wait()
{
}

// Mode 1: SE 3, rises 200 units a frame to base + height, then its collision off / area 4 off, SE 4 and
// 5 frames of shake; status 1.
void cR305Shutter::open()
{
    switch (step) {
    case 0:
        SndStop(se, 0);
        se = RoomSeCall(3, &obj->pos, 0, 0, obj);
        step++;
    case 1:
        obj->pos.y += 200.0f;
        if (obj->pos.y > basePos.y + height) {
            obj->pos.y = basePos.y + height;
            sat->setDisable();
            eat->setDisable();
            SceAtSetEnable(4, 0);
            shakeCnt = 5;
            step++;
            se = RoomSeCall(4, &obj->pos, 0, 0, obj);
        }
        break;
    case 2:
        obj->pos.x = fRand1_1() * 20.0f + basePos.x;
        obj->pos.y = fRand0_1() * 20.0f + (basePos.y + height);
        obj->pos.z = fRand1_1() * 20.0f + basePos.z;
        if (shakeCnt) {
            shakeCnt--;
        } else {
            obj->pos = basePos;
            obj->pos.y += height;
            status = 1;
            mode = 0;
        }
        break;
    }
}

// Mode 2: collision on / area 4 on, SE 5, drops with growing speed (40/frame^2) to the base, SE 6 and a shake; status 0.
void cR305Shutter::close()
{
    switch (step) {
    case 0:
        sat->setEnable();
        eat->setEnable();
        SceAtSetEnable(4, 1);
        spd = -40.0f;
        SndStop(se, 0);
        se = RoomSeCall(5, &obj->pos, 0, 0, obj);
        step++;
    case 1:
        obj->pos.y += spd;
        spd -= 40.0f;
        if (obj->pos.y < basePos.y) {
            obj->pos.y = basePos.y;
            shakeCnt = 5;
            step++;
            se = RoomSeCall(6, &obj->pos, 0, 0, obj);
        }
        break;
    case 2:
        obj->pos.x = fRand1_1() * 20.0f + basePos.x;
        obj->pos.y = fRand0_1() * 20.0f + basePos.y;
        obj->pos.z = fRand1_1() * 20.0f + basePos.z;
        if (shakeCnt == 0) {
            obj->pos = basePos;
            shakeCnt = 30;
            step++;
        } else {
            shakeCnt--;
        }
        break;
    case 3:
        if (shakeCnt) {
            shakeCnt--;
        } else {
            status = 0;
            mode = 0;
        }
        break;
    }
}

// Request opening (mode 1) unless open / opening / disabled (status 2/3); status 4 = moving.
void cR305Shutter::setOpen()
{
    if (!enable) {
        return;
    }
    if (status == 2) {
        return;
    }
    if (status == 3) {
        return;
    }
    if (status == 1) {
        return;
    }
    if (mode == 1) {
        return;
    }
    status = 4;
    mode = 1;
    step = 0;
}

// Request closing (mode 2) unless closed / closing / disabled.
void cR305Shutter::setClose()
{
    if (!enable) {
        return;
    }
    if (status == 2) {
        return;
    }
    if (status == 3) {
        return;
    }
    if (status == 0) {
        return;
    }
    if (mode == 2) {
        return;
    }
    status = 4;
    mode = 2;
    step = 0;
}

// 0 closed, 1 opened, 4 moving; -1 when there is no shutter object.
int cR305Shutter::getStatus()
{
    if (enable) {
        return status;
    }
    return -1;
}

// Snap the shutter open (collision off, area 4 off, SE stopped).
void cR305Shutter::setOpened()
{
    if (enable) {
        status = 1;
        obj->pos.y = basePos.y + height;
        sat->setDisable();
        eat->setDisable();
        SceAtSetEnable(4, 0);
        mode = 0;
        step = 0;
        SndStop(se, 0);
    }
}
