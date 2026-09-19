#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "global.h"
#include "flag_rsf.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "em_set.h"

// Room 1-0e (D:/Bio4/Prog/r10e.cpp): the village path; opens the door area after a delay and sets the
// enemies of the list depending on how the room was entered.

struct R10eWork {
    u8 pad[4];
};

static R10eWork* r10e_work;


static void R10e_door_set();

// Room init. Entered fresh (room_id_prev 0xFFF) it counts as coming from r119 and sets Scenario_flg[1]
// bit 0x01000000. Before that flag: collision area 1 on (the path is blocked). After it: coming back
// from r10e itself (a re-entry, System_flg 0x100 clear) disables areas 4/5 for 120 frames (R10e_door_set)
// and spawns enemy list 0x10 / 0xE / 0xF by pG->Part (2 / 1 / other), recording Part 1 in Room_flg bit 0;
// any other entry spawns list 0xD if that bit is set and 0xF otherwise. Always leaves room_id_prev = 0x119.
void R10eInit()
{
#line 34 "D:/Bio4/Prog/r10e.cpp"
    r10e_work = (R10eWork*) MEM_CALLOC(4, 1, 0xd);

    if (pG->room_id_prev == 0xFFF) {
        U16Set(pG->room_id_prev, 0x119);
        ScfFlagOn(pG, SCF_ST1_NIGHT);
    }
    if (!ScfFlagChk(pG, SCF_ST1_NIGHT)) {
        SceAtSetEnable(1, 0);
    } else {
        SceAtSetEnable(0, 0);
        if (pG->room_id_prev == 0x10E && !SysFlagChk(pG, SYS_LOAD_GAME)) {
            SceAtSetEnable(4, 0);
            SceAtSetEnable(5, 0);
            SceExec(0x12, (TaskFunc) R10e_door_set, 0, 0, SCE_PRIO_DEF_2, 0);
            if (pG->Part == 2) {
                RsfClear(G_ROOM_ID, 0);
                EmSetFromList2(0x10, 1);
            } else if (pG->Part == 1) {
                RsfSet(G_ROOM_ID, 0);
                EmSetFromList2(0xE, 1);
            } else {
                EmSetFromList2(0xF, 1);
            }
        } else {
            if (RsfCheck(G_ROOM_ID, 0)) {
                EmSetFromList2(0xD, 1);
            }
            if (RsfCheck(G_ROOM_ID, 0) == 0) {
                EmSetFromList2(0xF, 1);
            }
        }
        pG->room_id_prev = 0x119;
    }
}

// Per-frame room main: nothing.
void R10eMain()
{
}

// Task: re-enable collision areas 4 and 5 two seconds after entry.
static void R10e_door_set()
{
    SceSleep(120);
    SceAtSetEnable(4, 1);
    SceAtSetEnable(5, 1);
}
