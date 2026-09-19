#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "esp.h"
#include "rnd.h"

// Room 1-12 (D:/Bio4/Prog/r112.cpp): the farm at night in the storm (the chapter 2-2 return through
// the village). Entry R112Init: rain without splashes, the thunder flash task (r112_ThunderMove) and
// r102's shared BGM task (the farm music lives in r102.cpp, same module).

struct R112Work {
    u8 pad[1];
};

static R112Work* r112_work;

void r102_checkBgm();
static void r112_ThunderMove();

// Room init: no water on the espgen42 (rain) effect, then the thunder task and r102's shared BGM
// check task (the farm music logic lives in r102.cpp).
void R112Init()
{
#line 37 "D:/Bio4/Prog/r112.cpp"
    r112_work = (R112Work*) MEM_CALLOC(sizeof(R112Work), 1, 0xd);

    Espgen42SetNoWater(1);
    SceExec(0x12, (TaskFunc) r112_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r102_checkBgm, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per-frame room main: nothing.
void R112Main()
{
}

// Thunder every 90..235 frames while the storm flag is set.
static void r112_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    for (;;) {
        if (cnt == 0) {
            if (StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
                EstSet(0, -1, 0, 0, 1, 3, 1, 0, 0, 0);
                SceSndCallThunder();
            }
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
        }
        cnt--;
        SceSleep(1);
    }
}
