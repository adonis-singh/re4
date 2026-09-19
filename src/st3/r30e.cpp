#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "snd.h"
#include "em_set.h"
#include "em_wrap.h"

// Room 3-0E (D:/Bio4/Prog/r30e.cpp): an island corridor whose enemy set switches once the key item
// (Item_flg[0] 0x40) is taken (the first six list entries die, the second six appear) and whose
// stream changes from the plain room stream to the find-player battle stream.

struct R30eWork {
    u8 dummy;
};

static R30eWork* r30e_work;

static void r30e_checkEmSet();
static void r30e_checkBgm();
static void r30e_checkBgm2();

// Room init: the enemy set by the item flag; the plain stream before the item (Item_flg[0] 0x40) is
// taken, the find-player stream afterwards.
void R30eInit()
{
#line 30 "D:/Bio4/Prog/r30e.cpp"
    r30e_work = (R30eWork*) MEM_CALLOC(sizeof(R30eWork), 1, 0xd);
    r30e_checkEmSet();
    if (!ItfFlagChk(pG, ITF_R309_KEY)) {
        SceExec(0x12, (TaskFunc) r30e_checkBgm, 0, 0, 2, 0);
    } else {
        SceExec(0x12, (TaskFunc) r30e_checkBgm2, 0, 0, 2, 0);
    }
}

// Per-frame room main: nothing.
void R30eMain()
{
}

// Once the item is taken the first six list entries are dead and the second six appear.
static void r30e_checkEmSet()
{
    u8 dead[6] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75};
    u8 set[6] = {0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B};
    u32 i;

    if (ItfFlagChk(pGS, ITF_R309_KEY)) {
        for (i = 0; i < 6; i++) {
            EmListSetAlive(dead[i], 0);
        }
        for (i = 0; i < 6; i++) {
            setEm(set[i], -1, 0, 1, 1);
        }
    }
}

// Before the item: room stream 3 plays at once.
static void r30e_checkBgm()
{
    SndRoomStrStart(1, 3, 1);
}

// The stream starts when the player is found and stops when every enemy is dead.
static void r30e_checkBgm2()
{
    SceSleep(30);
    while (SceCkFindPL(0) == 0) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}
