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
#include "emhit.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "snd.h"
#include "vec.h"

// Room 3-26 (D:/Bio4/Prog/r326.cpp): the island cold-storage room: the corpse bag hanging from the
// ceiling (a SetObjSmd with the dark light set until the lights come on, then it drops with a hit box),
// three item boxes, Ashley's room motions and the door message while she is along.

struct R326Work {
    int x0;
    cObj* bag;      // 0x4  the corpse bag (SetObjSmd)
    cEmHit* hit;    // 0x8  its hit box
};

static R326Work* r326_work;

// Reference stores: the work pointer (and pG) are reloaded after them.
static inline void PSet(cObj*& d, cObj* v) { d = v; }
static inline void PSet(cEmHit*& d, cEmHit* v) { d = v; }

static void r326_setSubCharMotion();
static void r326_DoorLock();
void set_bag_eid();
static void r326_setCorpseBag();
void r326_openBox_main(u32 id, int opened);
static void r326_openedBox(int id);
static void r326_openBox(int id);

// Room init: the corpse bag task, three box item events (items 0x80/0x81 and a flagless third), the
// player's room motions, Ashley's motions a frame later.
void R326Init()
{
#line 36 "D:/Bio4/Prog/r326.cpp"
    r326_work = (R326Work*) MEM_CALLOC(sizeof(R326Work), 1, 0xd);
    SceExec(0x12, (TaskFunc) r326_setCorpseBag, 0, 0, 2, 0);
    SceSetItemEvent(3, 0x80, 0, 3, r326_openBox, (void (*)()) r326_openedBox, 0, 0);
    SceSetItemEvent(4, 0x81, 1, 4, r326_openBox, (void (*)()) r326_openedBox, 1, 0);
    SceSetItemEvent(5, -1, 2, 5, r326_openBox, (void (*)()) r326_openedBox, 2, 0);
    PlRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    SceExec(0x12, (TaskFunc) r326_setSubCharMotion, 0, 0, 2, 0);
}

// Per-frame room main: nothing.
void R326Main()
{
}

// A frame in, if Ashley (pSUB) is along: register her room motions and area 0 = the door message.
static void r326_setSubCharMotion()
{
    SceSleep(1);
    if (pSUB != 0) {
        SubCharRegistMotion(ROOM_ARC_PTR(pG->pRoom, 0x25), ROOM_ARC_PTR(pG->pRoom, 0x26));
        SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) r326_DoorLock, 0, 1);
    }
}

// Area 0 (with Ashley): up-cut 1 (the door does not open).
static void r326_DoorLock()
{
    SceUpCut(1, -1, -1, 0);
}

// Light set of the bag: the dark-room set while the lights are off.
void set_bag_eid()
{
    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        r326_work->bag->LightInfo.EnableMask = 0x80;
    } else {
        r326_work->bag->LightInfo.EnableMask = 0x10;
    }
}

// Task: the corpse bag (SetObjSmd at the ceiling); done if Room_flg bit 3. Area 6 off; until the lights
// are on (bit 2) and for 90 frames after, the bag uses the dark light set; then its drop SE, the fall
// motion and a hit box on it.
static void r326_setCorpseBag()
{
    Vec pos = {-3142.0f, 3863.0f, -1793.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};

    PSet(r326_work->bag, SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x21), ROOM_ARC_PTR(pG->pRoom, 0x22), &pos, &rot, 0x10, 1));
    if (RsfCheck(G_ROOM_ID, 3)) {
        SceExit();
    }
    SceAtSetEnable(6, 0);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        u32 i;

        while (RsfCheck(G_ROOM_ID, 2) == 0) {
            set_bag_eid();
            SceSleep(1);
        }
        for (i = 0; i < 90; i++) {
            set_bag_eid();
            SceSleep(1);
        }
    }
    RoomSeCall(0, &r326_work->bag->pos, 0, 0, r326_work->bag);
    r326_work->bag->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x23), 0xA, 0, 4, 0);
    {
        Vec hpos = {300.0f, 0.0f, 0.0f};
        Vec hrot = {0.0f, 0.0f, 1.5707964f};

        PSet(r326_work->hit, SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc),
                                      &hpos, &hrot, 0));
    }
    r326_work->hit->setParent(r326_work->bag, 0, 0);
    YarareInit(r326_work->hit, 0.0f, 0.0f, 0.0f, 260.0f, 600.0f, 0, 1);
    while (1) {
        if (r326_work->hit->ckStatus() == 1) {
            RsfSet(G_ROOM_ID, 3);
            RoomSeCall(1, &r326_work->bag->pos, 0, 0, r326_work->bag);
            r326_work->bag->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x24), 0xA, 0, 1, 0);
            SceSleep(0x46);
            SceAtSetEnable(6, 1);
            return;
        }
        set_bag_eid();
        SceSleep(1);
    }
}

// Opens item box `id` (0..2): the lid model and, for box 0, the item model inside move over 30 frames
// (or jump to the open pose when `opened`).
void r326_openBox_main(u32 id, int opened)
{
    Vec mv = {0.0f, 0.0f, 0.0f};
    Vec rmv = {0.0f, 0.0f, 0.0f};
    cObj* obj = 0;
    cModel* item = 0;
    int se = 0;

    switch (id) {
    case 0:
        se = 0x1B;
        obj = SmdGetObjPtr(0x30);
        item = SceAtItemModelPtr(0x80);
        mv.z = 300.0f;
        break;
    case 1:
        se = 0x18;
        obj = SmdGetObjPtr(0x31);
        rmv.y = 2.7925267f;
        break;
    case 2:
        se = 0x13;
        obj = SmdGetObjPtr(0x32);
        rmv.x = 1.5707964f;
        break;
    default:
        SceExit();
        break;
    }
    if (obj != 0) {
        obj->be_flag |= 0x20;
        if (opened == 1) {
            PSVECAdd(&obj->pos, &mv, &obj->pos);
            PSVECAdd(&obj->pParts->ang, &rmv, &obj->pParts->ang);
            if (item != 0) {
                PSVECAdd(&item->pos, &mv, &item->pos);
                PSVECAdd(&item->pParts->ang, &rmv, &item->pParts->ang);
            }
        } else {
            Vec dmv;
            Vec drmv;
            int i;

            PSVECScale(&mv, &dmv, 1.0f / 30.0f);
            PSVECScale(&rmv, &drmv, 1.0f / 30.0f);
            RoomSeCall(se, 0, 0, 0, 0);
            for (i = 0; i < 30; i++) {
                PSVECAdd(&obj->pos, &dmv, &obj->pos);
                PSVECAdd(&obj->pParts->ang, &drmv, &obj->pParts->ang);
                if (item != 0) {
                    PSVECAdd(&item->pos, &dmv, &item->pos);
                    PSVECAdd(&item->pParts->ang, &drmv, &item->pParts->ang);
                }
                SceSleep(1);
            }
        }
    }
}

// Item-event "already opened": box `id` posed open.
static void r326_openedBox(int id)
{
    r326_openBox_main(id, 1);
}

// Item-event opener: animate box `id` open.
static void r326_openBox(int id)
{
    r326_openBox_main(id, 0);
}
