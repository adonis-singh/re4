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
#include "player.h"
#include "cam_ctrl.h"
#include "snd.h"
#include "fade.h"
#include "cSceObj.h"

// Room 2-20 (D:/Bio4/Prog/r220.cpp): the castle elevator: two cSceObj door halves (r220_moveElevatoDoor)
// and the ride (r220_moveElevator) up on the lever (area 3) or down when arriving from r221 / r22B.

struct R220Work {
    cSceObj door[2];   // 0x000  the two door halves
};

static R220Work* r220_work;

void r220_moveElevatoDoor(int open, int init);
static void r220_moveElevator(int dir);
static void r220_operateElevator();
void r220_initElevator();

// Room init: clears System_flg 0x400 (room changes allowed again) and sets the elevator up.
void R220Init()
{
    SysFlagOff(pG, SYS_SCREEN_STOP);
#line 36 "D:/Bio4/Prog/r220.cpp"
    r220_work = (R220Work*) MEM_CALLOC(sizeof(R220Work), 1, 0xd);
    r220_initElevator();
}

// Per-frame room main: nothing.
void R220Main()
{
}

// open: 1 = the doors close (reverse); init: 1 = set the movers up, 0 = run them.
void r220_moveElevatoDoor(int open, int init)
{
    cObj* o7 = SmdGetObjPtr(7);
    cObj* o6 = SmdGetObjPtr(6);

    if (o7 && o6) {
        Vec p1 = {-195.0f, o7->pos.y, 8986.0f};
        Vec p2 = {-240.0f, o6->pos.y, 9029.0f};

        if (init == 1) {
            Vec d1;
            Vec d2;

            PSVECSubtract(&p1, &o7->pos, &d1);
            PSVECSubtract(&p2, &o6->pos, &d2);
            r220_work->door[0].initMove1_pos(o7, 60, &d1, 30.0f, 20.0f);
            r220_work->door[1].initMove1_pos(o6, 30, &d2, 30.0f, 20.0f);
            r220_work->door[0].setVibration(10, 10, 1.0f, 0.3f, 1.0f);
            r220_work->door[1].setVibration(10, 10, 1.0f, 0.3f, 1.0f);
            if (open == 1) {
                r220_work->door[0].setReverse(1);
                r220_work->door[1].setReverse(1);
            }
        } else {
            int second;
            u32 i;

            if (open == 1) {
                r220_work->door[0].setReverse(0);
                r220_work->door[1].setReverse(0);
            } else {
                r220_work->door[0].setReverse(1);
                r220_work->door[1].setReverse(1);
            }
            SndCall(6, 2, 0, 0, 0, 0);
            second = 0;
            for (i = 0; i < 60; i++) {
                if (i == 25) {
                    second = 1;
                }
                r220_work->door[0].move();
                if (second == 1) {
                    r220_work->door[1].move();
                }
                SceSleep(1);
            }
        }
    }
}

// The elevator ride (dir 0: up from the entrance, with the camera event).
static void r220_moveElevator(int dir)
{
    cObj* obj = SmdGetObjPtr(5);

    if (obj) {
        obj->be_flag |= 0x22;
        Vec d = {0.0f, 3000.0f, 0.0f};
        int evt = 1;
        int up = dir == 0;
        cSceObj elv;
        u32 i;

        elv.initMove1_pos(obj, 90, &d, 40.0f, 0.0f);
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
            if (up == 1) {
                CamCtrl.CutCall(1);
            }
        }
        if (up == 1) {
            r220_moveElevatoDoor(0, 0);
            SceSleep(15);
            SndCall(6, 4, 0, 0, 0, 0);
        } else {
            SndCall(6, 0, 0, 0, 0, 0);
            elv.setReverse(1);
        }
        for (i = 0; i < 90; i++) {
            if (up == 1 && i == 60) {
                FadeSetW(1, 30, 0, 0);
            }
            elv.move();
            SceSleep(1);
        }
        if (up == 0) {
            SndCall(6, 1, 0, 0, 0, 0);
            r220_moveElevatoDoor(1, 0);
        } else {
            SceAtExecute(0);
        }
        if (evt == 1) {
            pPL->setNoSuspend(0);
            CamCtrl.Comeback(0);
            SceEventEnd(2);
        }
    }
}

// Area 3, the elevator lever: stop room stream 3 and ride up (dir 0).
static void r220_operateElevator()
{
    SndRoomStrStop(3);
    r220_moveElevator(0);
}

// Arriving from r221 / r22B (above) on a normal transition (System_flg 0x100 / 0x80000 clear): doors
// closed and ride down (dir 1); otherwise the doors open. Area 3 becomes the elevator control (action colour).
void r220_initElevator()
{
    if ((pG->room_id_prev == 0x221 || pG->room_id_prev == 0x22B) && FlagChkSignW(pG->System_flg, SYS_LOAD_GAME) == 0
        && FlagChkSignW(pG->System_flg, SYS_CONTINUE) == 0) {
        r220_moveElevatoDoor(0, 1);
        SceExec(0x12, (TaskFunc) r220_moveElevator, 1, 0, SCE_PRIO_DEF_2, 0);
    } else {
        r220_moveElevatoDoor(1, 1);
    }
    SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) r220_operateElevator, 0, 1);
    SceAtSetActColor(3, 1);
}
