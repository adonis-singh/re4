#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em27.h"
#include "emdoor.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "em_wrap.h"

// Room 1-07 (D:/Bio4/Prog/r107.cpp): the lake house; the kiln item events (shared with r104), the
// fish water height and the battle BGM.

struct R107Work {
    u8 pad[1];
};

static R107Work* r107_work;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r107_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

static void r107_setFish();
void r104_openKiln_main(int type, int opened);
static void r104_openedKiln(int type);
static void r104_openKiln(int type);
static void r107_checkBgmPlay();

// Room init: pairs door 0 with door 0x15 (double door), starts the battle-BGM task, sets Room_flg bit 0
// on first visit, registers the two kiln item events (item 0x92 at area 3 -> kiln 0, 0x93 at area 5 ->
// kiln 1, reusing r104's opener), the water hit effects and the fish height task.
void R107Init()
{
    cEmDoor* door0;
    cEmDoor* door1;

#line 50 "D:/Bio4/Prog/r107.cpp"
    r107_work = (R107Work*) MEM_CALLOC(1, 1, 0xd);

    if (getRoomEtcDoor(0, &door0, 1) && getRoomEtcDoor(0x15, &door1, 1)) {
        door0->setDoor(door1);
    }
    SceExec(0x12, (TaskFunc) r107_checkBgmPlay, 0, 0, SCE_PRIO_DEF_2, 0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
    }
    SceSetItemEvent(3, 0x92, 1, 0xB, r104_openKiln, r104_openedKiln, 0, 0);
    SceSetItemEvent(5, 0x93, 2, 0xC, r104_openKiln, r104_openedKiln, 1, 0);
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r107_eff_info);
    SceExec(0x12, (TaskFunc) r107_setFish, 0, 0, SCE_PRIO_DEF_2, 0);
}

// Per frame: switch the player's footstep/OT type to the water variant.
void R107Main()
{
    setPlWaterOtType();
}

// Put every fish enemy at the lake's water level.
static void r107_setFish()
{
    u32 i;

    SceSleep(1);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if (em->id == 0x27 && (em->be_flag & 0x201) == 1) {
            ((cEm27*) em)->setWaterHeight(-13700.0f);
        }
    }
}

// Open (opened != 0: already open) kiln `type`: the two door objects turn 1.9 rad over 30 frames.
void r104_openKiln_main(int type, int opened)
{
    f32 step = 0.0f;
    cObj* a = 0;
    cObj* b = 0;

    switch (type) {
    case 0:
        a = SmdGetObjPtr(0x3F);
        b = SmdGetObjPtr(0x40);
        step = -1.9f;
        break;
    case 1:
        a = SmdGetObjPtr(0x41);
        b = SmdGetObjPtr(0x42);
        step = 1.9f;
        break;
    default:
        SceExit();
        break;
    }
    if (a != 0 && b != 0) {
        a->be_flag |= 0x20;
        b->be_flag |= 0x20;
        if (opened == 1) {
            a->pList->ang.y -= step;
            b->pList->ang.y += step;
        } else {
            int i;

            step /= 30.0f;
            SndCall(6, 0x1C, 0, 0, 0, 0);
            for (i = 30; i != 0; i--) {
                a->pList->ang.y -= step;
                b->pList->ang.y += step;
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened" callback: put kiln `type`'s doors in the open pose without animating.
static void r104_openedKiln(int type)
{
    r104_openKiln_main(type, 1);
}

// Item-event opener callback: animate kiln `type` open (30-frame swing) when the item is taken.
static void r104_openKiln(int type)
{
    r104_openKiln_main(type, 0);
}

// Battle BGM while the enemies of list entries 0x10..0x20 live.
static void r107_checkBgmPlay()
{
    while (SceCkFindPL(0) != 1) {
        SceSleep(1);
    }
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}
