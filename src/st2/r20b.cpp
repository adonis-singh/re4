// the split object's .rodata is 8-aligned (the double of the int->float conversions would do it
// anyway; keep the unit's head identical to the other rooms).
asm(".section .rodata\n\t.balign 8\n\t.text");
#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj18.h"
#include "em.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "read.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "item.h"
#include "rnd.h"
#include "snd.h"
#include "fade.h"
#include "sscrn.h"
#include "esp.h"
#include "est.h"
#include "TexRender.h"
#include "db_log.h"

// Room 2-0B (D:/Bio4/Prog/r20b.cpp): the great hall. The enemy waves released as the player
// crosses the hall, the church door key event, the room-entry camera and the S00 event with its
// render-to-texture cuts.

struct R20bWork {
    cEmWrap em[18];       // 0x000
    int cnt;              // 0x0D8  frames the last wave condition held
    TexRenderMng* tex0;   // 0x0DC  hall floor render target
    TexRenderMng* tex1;   // 0x0E0  event render targets
    TexRenderMng* tex2;   // 0x0E4
    u8 tbl0[0x80];        // 0x0E8
    u8 tbl1[0x80];        // 0x168
    TexRenderCam cam;     // 0x1E8
    u8 pad_4EC[0x7F0 - 0x4EC];
    void* tpl9300;        // 0x7F0  original texture palette of evm9300
    u32 str;              // 0x7F4  SndStrReq handle of the entry camera
    u8 pad_7F8[4];
    void* tpl0202;        // 0x7FC  original texture palette of ev0202
};


static u8 r20b_texTbl[0x20];
static R20bWork* r20b_work;


// `(f & a) || (f & b)` tested bit by bit: fold merges the two masks of a plain `||`; a helper keeps
// the two `andis.` on one load.
static inline u32 BitCk(u32 f, u32 b) { return f & b; }

static void R20bStartCameraMain();
static void R20bStartCameraCancel();
void R20bStartCameraEnd();
void R20bScrTrans(int on);
static void R20bOpenTerm();
static void OpenedBoxTreasure(int id);
static void OpenBoxTreasure(int id);
static void R20bEmSetMain();
int R20bCalcActiveEmWarp();
static void SceBgmCheck();
static void R20bDoorCheck();
static void R20bDoorEventMain();
static void R20bDoorEventEnd();
static void setTexRender();
static void R20bEventS00();
extern "C" void Evt_R20BS00_Func(Event* e);
void EvtTexRenderCamTrans(Event* e, int cut);

// Room init (the great hall): s00/s99 callbacks, Room_flg[0] bit 31 (lower floor) cleared, the upper
// floor objects shown, doors 3/8 paired; until s00 (Room_flg bit 0) it is pre-loaded with the enemy of
// list entry 0x11 and area 2 runs it (objects 0x51..0x53 swapped afterwards); doors 6/7 get lock models;
// item area 0x8C (the key) hidden until used; until bit 1 area 4 = the locked door with the key watcher,
// else area 5 off / insignia object 0x8F hidden; five treasure item events; the terminal once (bit 17);
// the entry camera once (bit 21); waves, stream and floor render target.
void R20bInit()
{
    cEmDoor* door0;
    cEmDoor* door1;
    cEmDoor* door;
    cModel* item;
    int zero;

#line 60 "D:/Bio4/Prog/r20b.cpp"
    r20b_work = (R20bWork*) MEM_CALLOC(sizeof(R20bWork), 1, 0xd);
    EvtMgr.SetFunc("evt_r20bs00_func", (void*) Evt_R20BS00_Func);
    EvtMgr.SetFunc("evt_r20bs99_func", (void*) Evt_R20BS00_Func);
    pG->Room_flg[0] &= 0x7FFFFFFF;
    R20bScrTrans(1);
    if (getRoomEtcDoor(3, &door0, 1) != 0 && getRoomEtcDoor(8, &door1, 1) != 0) {
        door0->setDoor(door1);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadAram("event/evd/r20bs00.evd", (u8) GetEmIdFromList(0x11), 0, 0, 0);
        SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) R20bEventS00, 0, 1);
        SmdSetTrans(0x51, 1);
        SmdSetTrans(0x52, 0);
        SmdSetTrans(0x53, 0);
    } else {
        SmdSetTrans(0x51, 0);
        SmdSetTrans(0x52, 1);
        SmdSetTrans(0x53, 1);
        SceAtSetEnable(0x1B, 0);
        if (!StaFlagChk(pG, STA_SUB_ASHLEY)) {
            EmReadSearch((u8) GetEmIdFromList(0x11), 0, 0);
        }
    }
    getRoomEtcDoor(6, &door, 1);
    if (door) {
        door->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0);
    }
    getRoomEtcDoor(7, &door, 1);
    if (door) {
        door->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 0);
    }
    SceAtSetEnable(0x8C, 1);
    item = SceAtItemModelPtr(0x8C);
    if (item) {
        item->LightInfo.EnableMask = (item->LightInfo.EnableMask & ~0x20) | 0x10;
        item->setNoSuspend(1);
    }
    SceAtSetEnable(0x8C, 0);
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) R20bDoorCheck, 0, 1);
        SceExec(0x12, (TaskFunc) R20bDoorEventMain, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceAtSetEnable(5, 0);
        SmdSetTrans(0x8F, 0);
    }
    SceSetItemEvent(0x12, 0x86, 0xD, 6, OpenBoxTreasure, OpenedBoxTreasure, 0x86, 0);
    zero = 0;
    SceSetItemEvent(0x13, 0x89, 0xE, 9, OpenBoxTreasure, OpenedBoxTreasure, 0x89, 0);
    SceSetItemEvent(0x14, 0x84, 0xF, 8, OpenBoxTreasure, OpenedBoxTreasure, 0x84, 0);
    SceSetItemEvent(0x15, 0x87, 0x10, 7, OpenBoxTreasure, OpenedBoxTreasure, 0x87, 0);
    SceSetItemEvent(0x18, 0x8D, 0x13, 0xB, OpenBoxTreasure, OpenedBoxTreasure, 0x8D, 0);
    if (RsfCheck(G_ROOM_ID, 17) == 0) {
        SceAtDataSet_exec(0x16, SCE_LEVEL10, 0, (TaskFunc) R20bOpenTerm, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 21) == 0) {
        SceExec(0x12, (TaskFunc) R20bStartCameraMain, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (!StaFlagChk(pG, STA_SUB_ASHLEY)) {
        r20b_work->cnt = zero;
        SceExec(0x12, (TaskFunc) R20bEmSetMain, 0, 0, SCE_PRIO_DEF_2, 0);
        SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    setTexRender();
    TexRenderInit(&r20b_work->tex1, 0, 1);
    TexRenderInit(&r20b_work->tex2, 0xE0, 2);
    SmdSetTrans(0xA1, 0);
    SetSstAddAreaFlag(0x800);
    r20b_work->str = zero;
}

// Per frame: switch between the upper and lower floor object sets (Room_flg[0] bit 31) from the
// player's height (y <= 100) or Room_flg[2] 0x00080000.
void R20bMain()
{
    if ((pG->Room_flg[2] & 0x00080000) || pPL->pos.y <= 100.0f) {
        if (!(pG->Room_flg[0] & 0x80000000)) {
            pG->Room_flg[0] |= 0x80000000;
            R20bScrTrans(0);
        }
    } else {
        if (pG->Room_flg[0] & 0x80000000) {
            pG->Room_flg[0] &= 0x7FFFFFFF;
            R20bScrTrans(1);
        }
    }
}

// The entry camera of the hall (once).
static void R20bStartCameraMain()
{
    if (RsfCheck(G_ROOM_ID, 21) == 0) {
        RsfSet(G_ROOM_ID, 21);
        SceEventStart(1);
        SceSetEventCancel(1, (TaskFunc) R20bStartCameraCancel, 0, -1, 1);
        r20b_work->str = SndStrReq(0, 0x15, 0x80000003, 0, 0, 0.0f);
        CamCtrl.CutCall(1);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R20bStartCameraEnd();
    }
}

// Cancel path of the entry camera: fade reset, its stream stopped, a 10-frame fade-in, then the common end.
static void R20bStartCameraCancel()
{
    FadeSetW(0, 0, 0, 0);
    if (r20b_work->str) {
        SndStrStopBlock(r20b_work->str);
        r20b_work->str = 0;
    }
    FadeSetW(0x80000000, 10, 0, 0);
    R20bStartCameraEnd();
}

// End of the entry camera: camera back, SceEventEnd.
void R20bStartCameraEnd()
{
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Upper floor scroll objects on / off.
void R20bScrTrans(int on)
{
    int i;

    SmdSetTrans(0x33, on);
    SmdSetTrans(0x34, on);
    SmdSetTrans(0x46, on);
    SmdSetTrans(0x4B, on);
    SmdSetTrans(0x50, on);
    SmdSetTrans(0x54, on);
    SmdSetTrans(0x55, on);
    SmdSetTrans(0x56, on);
    SmdSetTrans(0x59, on);
    SmdSetTrans(0x5A, on);
    for (i = 0x5C; i <= 0x60; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x63; i <= 0x66; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x68; i <= 0x6A; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x6D; i <= 0x72; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x76; i <= 0x7E; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x80; i <= 0x82; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x91; i <= 0x93; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x9C; i <= 0xA0; i++) {
        SmdSetTrans(i, on);
    }
}

// Area 0x16 once (Room_flg bit 17): typewriter terminal 0xF at its fixed spot.
static void R20bOpenTerm()
{
    SceAtSetEnable(0x16, 0);
    RsfSet(G_ROOM_ID, 17);
    OpeSetOpenTerm(0xF, 34050.0f, 4000.0f, -7340.0f, 2.45f);
}

// Item-event "already opened": pose the chest / shelf of item `id` open (per-item lid direction).
static void OpenedBoxTreasure(int id)
{
    if (id == 0x86) {
        OpenBoxMain(OpenBoxUpXP, 1, 0x5B, 0x87, -1, -1);
    }
    if (id == 0x89) {
        OpenBoxMain(OpenBoxUpXP, 1, 0x5B, 0x88, -1, -1);
    }
    if (id == 0x84) {
        OpenBoxMain(OpenBoxUpZM, 1, 0x5B, 0x89, -1, -1);
    }
    if (id == 0x87) {
        OpenBoxMain(OpenBoxUpXP, 1, 0x5B, 0x8A, -1, -1);
    }
    if (id == 0x8D) {
        OpenBoxMain(OpenBoxLR2, 1, 0x19, 0x9E, 0x9F, -1);
    }
}

// Item-event opener: animate the chest / shelf of item `id` open.
static void OpenBoxTreasure(int id)
{
    if (id == 0x86) {
        OpenBoxMain(OpenBoxUpXP, 0, 0x5B, 0x87, -1, -1);
    }
    if (id == 0x89) {
        OpenBoxMain(OpenBoxUpXP, 0, 0x5B, 0x88, -1, -1);
    }
    if (id == 0x84) {
        OpenBoxMain(OpenBoxUpZM, 0, 0x5B, 0x89, -1, -1);
    }
    if (id == 0x87) {
        OpenBoxMain(OpenBoxUpXP, 0, 0x5B, 0x8A, -1, -1);
    }
    if (id == 0x8D) {
        OpenBoxMain(OpenBoxLR2, 0, 0x19, 0x9E, 0x9F, -1);
    }
}

// The enemy waves of the hall, keyed to the at areas the player crosses.
static void R20bEmSetMain()
{
    cEmDoor* door0;
    cEmDoor* door1;
    cEmDoor* door;
    int hurt;
    int open;

    SceSleep(1);
    if (RsfCheck(G_ROOM_ID, 3)) {
        r20b_work->em[1].setEm(3, -1, 0, 1, 1);
        r20b_work->em[2].setEm(4, -1, 0, 1, 1);
        r20b_work->em[3].setEm(5, -1, 0, 1, 1);
        r20b_work->em[1].setFlag(1);
        r20b_work->em[2].setFlag(1);
        r20b_work->em[3].setFlag(1);
    }
    if (RsfCheck(G_ROOM_ID, 4)) {
        r20b_work->em[4].setEm(6, -1, 0, 1, 1);
        r20b_work->em[4].setFlag(1);
    }
    for (;;) {
        if (pG->Room_flg[2] & 0x80000000) {
            if (RsfCheck(G_ROOM_ID, 2) == 0) {
                RsfSet(G_ROOM_ID, 2);
                r20b_work->em[0].setEm(0x11, -1, 0, 1, 1);
                r20b_work->em[0].setFlag(1);
                r20b_work->em[4].setEm(6, -1, 0, 1, 1);
                r20b_work->em[1].setEm(3, -1, 0, 1, 1);
                r20b_work->em[2].setEm(4, -1, 0, 1, 1);
                r20b_work->em[3].setEm(5, -1, 0, 1, 1);
            }
        }
        if (BitCk(pG->Room_flg[2], 0x40000000) || BitCk(pG->Room_flg[2], 0x20000000)) {
            if (RsfCheck(G_ROOM_ID, 3) == 0) {
                RsfSet(G_ROOM_ID, 3);
                r20b_work->em[1].setEm(3, -1, 0, 1, 1);
                r20b_work->em[2].setEm(4, -1, 0, 1, 1);
                r20b_work->em[3].setEm(5, -1, 0, 1, 1);
                r20b_work->em[1].setFlag(1);
                r20b_work->em[2].setFlag(1);
                r20b_work->em[3].setFlag(1);
            }
        }
        hurt = 0;
        if (r20b_work->em[1].isActive()) {
            hurt = r20b_work->em[1].getHp() < r20b_work->em[1].getHpMax();
        }
        if (r20b_work->em[2].isActive()) {
            if (r20b_work->em[2].getHp() < r20b_work->em[2].getHpMax()) {
                hurt = 1;
            }
        }
        if (r20b_work->em[3].isActive()) {
            if (r20b_work->em[3].getHp() < r20b_work->em[3].getHpMax()) {
                hurt = 1;
            }
        }
        if ((pG->Room_flg[2] & 0x00400000) || hurt == 1) {
            if (RsfCheck(G_ROOM_ID, 9) == 0) {
                RsfSet(G_ROOM_ID, 9);
                if (r20b_work->em[1].isActive() || r20b_work->em[2].isActive() || r20b_work->em[3].isActive()) {
                    getRoomEtcDoor(6, &door0, 1);
                    {
                        Vec pos = {17000.0f, 87.0f, -3600.0f};

                        if (door0) {
                            door0->setOpen(&pos, 0, 0, 1);
                        }
                    }
                }
            }
        }
        if (BitCk(pG->Room_flg[2], 0x10000000) || BitCk(pG->Room_flg[2], 0x08000000)) {
            if (RsfCheck(G_ROOM_ID, 4) == 0) {
                RsfSet(G_ROOM_ID, 4);
                r20b_work->em[4].setEm(6, -1, 0, 1, 1);
                r20b_work->em[4].setFlag(1);
            }
        }
        if (pG->Room_flg[2] & 0x00100000) {
            if (RsfCheck(G_ROOM_ID, 11) == 0) {
                RsfSet(G_ROOM_ID, 11);
                if (r20b_work->em[4].isActive()) {
                    getRoomEtcDoor(7, &door1, 1);
                    {
                        Vec pos = {-1100.0f, 0.0f, -39000.0f};

                        if (door1) {
                            door1->setOpen(&pos, 0, 0, 1);
                        }
                    }
                }
            }
        }
        if (R20bCalcActiveEmWarp() <= 4) {
            if (pG->Room_flg[2] & 0x02000000) {
                if (RsfCheck(G_ROOM_ID, 6) == 0) {
                    RsfSet(G_ROOM_ID, 6);
                    r20b_work->em[8].setEm(0x17, -1, 0, 1, 1);
                    r20b_work->em[8].setFlag(1);
                }
            }
        }
        if (R20bCalcActiveEmWarp() <= 3) {
            if (pG->Room_flg[2] & 0x04000000) {
                if (RsfCheck(G_ROOM_ID, 5) == 0) {
                    u8 r;

                    RsfSet(G_ROOM_ID, 5);
                    r = Rnd() % 6;
                    switch (r) {
                    case 0:
                        r20b_work->em[5].setEm(0x14, -1, 0, 1, 1);
                        r20b_work->em[6].setEm(0x15, -1, 0, 1, 1);
                        break;
                    case 1:
                        r20b_work->em[6].setEm(0x15, -1, 0, 1, 1);
                        r20b_work->em[7].setEm(0x16, -1, 0, 1, 1);
                        break;
                    case 2:
                        r20b_work->em[5].setEm(0x14, -1, 0, 1, 1);
                        r20b_work->em[7].setEm(0x16, -1, 0, 1, 1);
                        break;
                    case 3:
                        r20b_work->em[5].setEm(0x14, -1, 0, 1, 1);
                        break;
                    case 4:
                        r20b_work->em[6].setEm(0x15, -1, 0, 1, 1);
                        break;
                    case 5:
                        r20b_work->em[7].setEm(0x16, -1, 0, 1, 1);
                        break;
                    }
                    r20b_work->em[5].setFlag(1);
                    r20b_work->em[6].setFlag(1);
                    r20b_work->em[7].setFlag(1);
                }
            }
        }
        if (SceAtItemFlgCk(0x81) == 1) {
            if (pG->Room_flg[2] & 0x01000000) {
                if (RsfCheck(G_ROOM_ID, 7) == 0) {
                    if (R20bCalcActiveEmWarp() <= 3) {
                        RsfSet(G_ROOM_ID, 7);
                        r20b_work->em[9].setEm(0x1A, -1, 0, 1, 1);
                        r20b_work->em[9].setFlag(1);
                        r20b_work->em[10].setEm(0x1B, -1, 0, 1, 1);
                        r20b_work->em[10].setFlag(1);
                    }
                }
            }
        }
        if (SceAtItemFlgCk(0x81) == 1) {
            if (pG->Room_flg[2] & 0x01000000) {
                if (RsfCheck(G_ROOM_ID, 20) == 0) {
                    if (R20bCalcActiveEmWarp() <= 3) {
                        u8 r;

                        RsfSet(G_ROOM_ID, 20);
                        r = Rnd() % 3;
                        switch (r) {
                        case 0:
                            r20b_work->em[17].setEm(0x1C, -1, 0, 1, 1);
                            r20b_work->em[11].setEm(0xD, -1, 0, 1, 1);
                            break;
                        case 1:
                            r20b_work->em[17].setEm(0x1C, -1, 0, 1, 1);
                            r20b_work->em[12].setEm(0xE, -1, 0, 1, 1);
                            break;
                        case 2:
                            r20b_work->em[11].setEm(0xD, -1, 0, 1, 1);
                            r20b_work->em[12].setEm(0xE, -1, 0, 1, 1);
                            break;
                        }
                        r20b_work->em[11].setFlag(1);
                        r20b_work->em[12].setFlag(1);
                    }
                }
            }
        }
        if (R20bCalcActiveEmWarp() <= 4) {
            if (SceAtItemFlgCk(0x86) == 1) {
                if (RsfCheck(G_ROOM_ID, 12) == 0) {
                    RsfSet(G_ROOM_ID, 12);
                    r20b_work->em[16].setEm(0x19, -1, 0, 1, 1);
                    r20b_work->em[16].setFlag(1);
                }
            }
        }
        if (R20bCalcActiveEmWarp() <= 4) {
            if (SceAtItemFlgCk(0x86) == 1) {
                if (RsfCheck(G_ROOM_ID, 10) == 0) {
                    r20b_work->cnt++;
                    if (r20b_work->cnt > 150) {
                        RsfSet(G_ROOM_ID, 10);
                        r20b_work->em[15].setEm(0x18, -1, 0, 1, 1);
                        r20b_work->em[15].setFlag(1);
                    }
                }
            }
        }
        open = 0;
        getRoomEtcDoor(6, &door, 1);
        if (door) {
            open = door->ckOpen() == 1;
        }
        getRoomEtcDoor(7, &door, 1);
        if (door) {
            if (door->ckOpen() == 1) {
                open = 1;
            }
        }
        if (R20bCalcActiveEmWarp() <= 3) {
            if (open == 1) {
                if (pG->Room_flg[2] & 0x00800000) {
                    if (RsfCheck(G_ROOM_ID, 8) == 0) {
                        RsfSet(G_ROOM_ID, 8);
                        r20b_work->em[13].setEm(0xA, -1, 0, 1, 1);
                        r20b_work->em[14].setEm(0xB, -1, 0, 1, 1);
                        r20b_work->em[13].setFlag(1);
                        r20b_work->em[14].setFlag(1);
                    }
                }
            }
        }
        SceSleep(1);
    }
}

// Number of the 18 wave enemies currently active.
int R20bCalcActiveEmWarp()
{
    int cnt = 0;
    int i;

    for (i = 0; i < 18; i++) {
        if (r20b_work->em[i].isActive() == 1) {
            cnt++;
        }
    }
    return cnt;
}

// Room stream: on while the player is found by an enemy.
static void SceBgmCheck()
{
    int playing = 0;

    for (;;) {
        if (SceCkFindPL(NULL) == 1) {
            if (playing == 0) {
                SndRoomStrStart(1, 0, 1);
                playing = 1;
            }
        } else if (playing == 1) {
            SndRoomStrStop(3);
            playing = 0;
        }
        SceSleep(1);
    }
}

// Standing at the locked church door: the camera cut and the inventory to use the key.
static void R20bDoorCheck()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceUpCut(1, 4, 2, UP_CUT_ATTR_CUT_FIX);
        if (ItemMgr.num(0x7A) != 0 || (ItemMgr.num(0x3A) != 0 && ItemMgr.num(0x69) != 0)) {
            SubScreenOpen(SS_OPEN_ITEM, SS_ATTR_EVENT);
        } else {
            CamCtrl.Comeback(0);
        }
    }
}

// The key used from the inventory: the insignia rises out of the floor.
static void R20bDoorEventMain()
{
    cObj* obj;
    cModel* item;
    int i;

    if (RsfCheck(G_ROOM_ID, 1)) {
        return;
    }
    while (ItemMgr.check(0x7A) != 1) {
        SceSleep(1);
    }
    SceEventStart(1);
    ScfFlagOn(pG, SCF_75);
    RsfSet(G_ROOM_ID, 1);
    SceAtSetEnable(4, 0);
    SceAtSetEnable(0x19, 0);
    SceAtSetEnable(0x8C, 1);
    SndCall(6, 0, 0, 0, 0, 0);
    CamCtrl.CutCall(4);
    SceMesSet(2, 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
    SceSetEventCancel(1, (TaskFunc) R20bDoorEventEnd, 0, -1, 1);
    CamCtrl.CutCall(5);
    obj = SmdGetObjPtr(0x8F);
    item = SceAtItemModelPtr(0x8C);
    if (obj && item) {
        // block-local const: the 4000 lands in the pool before the int->float conversion's double
        const f32 h = 4000.0f;

        SndCall(6, 1, 0, 0, 0, 0);
        obj->be_flag |= 0x20;
        for (i = 0; i < 60; i++) {
            f32 y = (f32) i * h / 60.0f + h;

            item->pos.y += y - obj->pos.y;
            obj->pos.y = y;
            SceSleep(1);
        }
    }
    SceSleep(30);
    SceSetEventCancel(0, 0, 0, -1, 1);
    R20bDoorEventEnd();
}

// End of the key event: area 5 off, the insignia object 0x8F hidden, item area 0x8C off, camera back, SceEventEnd.
static void R20bDoorEventEnd()
{
    SceAtSetEnable(5, 0);
    SmdSetTrans(0x8F, 0);
    SceAtSetEnable(0x8C, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// TexRender blend setup of one floor object.
#define R20B_TEX_OBJ(id)                    \
    obj = SmdGetObjPtr(id);                 \
    obj->pModelInfo->setTexBlendTbl(tbl);        \
    obj->pModelInfo->setBlendRatio(0xFF);        \
    obj->pModelInfo->setBlendType(1);            \
    obj->pModelInfo->color[3] = 0xF0;            \
    obj->Shader_type = 2;                          \
    obj->Refract_pow = 4;                          \
    obj->Refract_ratio = 0x20;                       \
    obj->invisible_factor = 0.7f;

// The hall floor: a render target blended into the two floor objects.
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r20b_texTbl;

    if (GetTexRenderMgr(&r20b_work->tex0) == 0) {
        pLog->err(0, 0, "setTexRender() : Manager alloc failed!!");
    } else {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r20b_work->tex0->m_Tex_no;
        r20b_work->tex0->m_Rep_type = 1;
        {
            TexRenderMng* t = r20b_work->tex0;

            t->m_W_size = 0x40;
            t->m_H_size = 0x40;
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, r20b_work->tex0->m_Core_flg | 1, ESP_CORE_KIND_NONE, 0, 0);
        R20B_TEX_OBJ(0x28);
        R20B_TEX_OBJ(0x29);
    }
}

// Area 2 once (Room_flg bit 0): destroys the 0x22 enemies, plays r20bs00 and ends chapter 3-2
// (SceSetChapterEnd(CHAPTER_3_2)).
static void R20bEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SceAtSetEnable(2, 0);
        SceDestroyEm(0x22, 0x22);
        SceSleep(1);
        EvtMgr.EvtReadExec("event/evd/r20bs00.evd", (u8) GetEmIdFromList(0x11), EvtReadFlagNone);
        SceSetChapterEnd(CHAPTER_3_2, -1);
    }
}

// Event r20bs00 (and s99) callback: swaps the ev0202 model's texture palette to the r20b variant for
// cuts after 0x21 (and back), the evm9300 palette per cut, feeds the two render-to-texture passes on
// cuts 0x21 / 0x24 (TexRenderModSet on the stand-in models, released after), and sets the event models'
// flags per cut; the end restores the palettes.
extern "C" void Evt_R20BS00_Func(Event* e)
{
    void* mod;
    void* bin;
    void* mod2;
    void* bin2;
    void* mod3;
    void* mod4;

    switch (e->FuncType) {
    case 0:
        break;
    case 1:
        if (!(pG->Room_flg[0] & 0x40000000)) {
            pG->Room_flg[0] |= 0x40000000;
            if (e->GetMod(&mod, "ev0202", 0, 0) == 1) {
                r20b_work->tpl0202 = ((cModelInfo*) mod)->tpl_addr;
            }
        }
        if (e->NowCut > 0x21) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "ev0202", 0, 0) == 1) {
                    if (EvtMgr.GetBin(&bin, "event/model/ev0200/ev0202_r20b.tpl", 0) == 1) {
                        ((cModelInfo*) mod)->setTplAddr(bin);
                    }
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "ev0202", 0, 0) == 1) {
                    ((cModelInfo*) mod)->setTplAddr(r20b_work->tpl0202);
                }
            }
        }
        switch (e->NowCut) {
        case 0x21:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evma100", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod2, 0, r20b_work->tbl0, r20b_work->tex1, 1, 0, 0, 0, 0.2f);
                }
            }
            EvtTexRenderCamTrans(e, 0x21);
            break;
        case 0x24:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evma100", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod2, 0, r20b_work->tbl0, r20b_work->tex1, 1, 0, 0, 0, 0.2f);
                }
            }
            EvtTexRenderCamTrans(e, 0x24);
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evma100", 0, 0) == 1) {
                    TexRenderModRes((cModel*) mod2, 0);
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
            if (e->GetMod(&mod, "evm9300", 0, 0) == 1) {
                r20b_work->tpl9300 = ((cModelInfo*) mod)->tpl_addr;
            }
            break;
        case 5:
        case 0x13:
        case 0x15:
        case 0x1C:
        case 0x21:
        case 0x24:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm9300", 0, 0) == 1) {
                    if (EvtMgr.GetBin(&bin2, "event/model/evm9300/evm9300_usu.tpl", 0) == 1) {
                        ((cModelInfo*) mod)->setTplAddr(bin2);
                    }
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod, "evm9300", 0, 0) == 1) {
                    ((cModelInfo*) mod)->setTplAddr(r20b_work->tpl9300);
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0x21:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evma100a", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod2, 0, r20b_work->tbl1, r20b_work->tex2, 1, 0, 0, 1, 0.35f);
                }
                EffectEspDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
                EffectEspgenDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
                EffectEfmDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 2, r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
            }
            break;
        case 0x24:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evma100a", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod2, 0, r20b_work->tbl1, r20b_work->tex2, 1, 0, 0, 1, 0.35f);
                }
                EffectEspDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
                EffectEspgenDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
                EffectEfmDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
                EstSet(0, -1, 0, 0, EFF_ROOM, 1, r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evma100a", 0, 0) == 1) {
                    TexRenderModRes((cModel*) mod2, 0);
                }
                EffectEspDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0, 0);
                EffectEspgenDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
                EffectEfmDelete(r20b_work->tex2->m_Core_flg | 0x3001, ESP_CORE_KIND_NONE, 0);
            }
            break;
        }
        switch (e->NowCut) {
        case 0xD:
        case 0xE:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod3, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod3, 2, 0);
                    ModelInfoSetTrans((cModel*) mod3, 6, 0);
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod4, "pl0000", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod4, 2, 1);
                    ModelInfoSetTrans((cModel*) mod4, 6, 1);
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0:
        case 0xB:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x13:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod2, 1, 0);
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod2, 1, 1);
                }
            }
            break;
        }
        if (e->NowCut == 0x2A) {
            if (e->NowFrame <= 0x10) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod2)->be_flag |= 0x40;
                }
            } else {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod2)->be_flag &= ~0x40;
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    OBJ18_WK((cObj18*) mod2)->be_flag &= ~0x40;
                }
            }
        }
        switch (e->NowCut) {
        case 0xB:
        case 0x11:
        case 0x13:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    Obj18Work* w = OBJ18_WK((cObj18*) mod2);

                    if (w && w->child) {
                        OBJ18_WK((cObj18*) mod2)->ObjChainFlagCommon |= 0x04000000;
                        w->child->be_flag &= ~2;
                    }
                }
            }
            break;
        default:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    Obj18Work* w = OBJ18_WK((cObj18*) mod2);

                    if (w && w->child) {
                        OBJ18_WK((cObj18*) mod2)->ObjChainFlagCommon &= ~0x04000000;
                        w->child->be_flag |= 2;
                    }
                }
            }
            break;
        }
        switch (e->NowCut) {
        case 0x16:
        case 0x1C:
        case 0x25:
        case 0x29:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x27, 0);
                SmdSetTrans(0x30, 0);
                SmdSetTrans(0x37, 0);
                SmdSetTrans(0x8C, 0);
                SmdSetTrans(0x8D, 0);
                SmdSetTrans(0x8E, 0);
                SmdSetTrans(0x3D, 0);
            }
            break;
        default:
            if (e->NowFrame == 0) {
                SmdSetTrans(0x27, 1);
                SmdSetTrans(0x30, 1);
                SmdSetTrans(0x37, 1);
                SmdSetTrans(0x8C, 1);
                SmdSetTrans(0x8D, 1);
                SmdSetTrans(0x8E, 1);
                SmdSetTrans(0x3D, 1);
            }
            break;
        }
        if (e->NowCut == 0) {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod2, 5, 0);
                }
            }
        } else {
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "pl0200", 0, 0) == 1) {
                    ModelInfoSetTrans((cModel*) mod2, 5, 1);
                }
            }
        }
        switch (e->NowCut) {
        case 0:
            if (e->NowFrame == 0) {
                if (e->GetMod(&mod2, "evm8200", 0, 0) == 1) {
                    ((cModel*) mod2)->LightInfo.EnableMask = 0x40;
                }
                if (e->GetMod(&mod2, "evm9300", 0, 0) == 1) {
                    ((cModel*) mod2)->ot_type = 1;
                    ((cModel*) mod2)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod2, "evma100", 0, 0) == 1) {
                    ((cModel*) mod2)->ot_type = 1;
                }
                if (e->GetMod(&mod2, "evma100a", 0, 0) == 1) {
                    ((cModel*) mod2)->ot_type = 1;
                }
            }
            break;
        case 0x21:
        case 0x24:
            SetNearClipDist(50.0f);
            break;
        case 0x1A:
        case 0x1C:
            SmdSetTrans(0x68, 1);
            break;
        case 0x1B:
            SmdSetTrans(0x68, 0);
            break;
        case 0x28:
            SmdSetTrans(0x51, 1);
            SmdSetTrans(0x52, 0);
            SmdSetTrans(0x53, 0);
            break;
        case 0x29:
        case 0x2A:
        case 0x2B:
            SmdSetTrans(0x51, 0);
            SmdSetTrans(0x52, 1);
            SmdSetTrans(0x53, 1);
            break;
        }
        if (e->NowCut == 0x25) {
            if (e->NowFrame == 0) {
                SmdSetTrans(0x5E, 0);
            }
        } else {
            SmdSetTrans(0x5E, 1);
        }
        if (e->NowCut == 0x15) {
            if (e->NowFrame == 0) {
                SetSstAddAreaFlag(0);
            }
        } else {
            SetSstAddAreaFlag(0x800);
        }
        break;
    case 2:
        SmdSetTrans(0x51, 0);
        SmdSetTrans(0x52, 1);
        SmdSetTrans(0x53, 1);
        SmdSetTrans(0x68, 1);
        SceAtSetEnable(0x1B, 0);
        SmdSetTrans(0x27, 1);
        SmdSetTrans(0x30, 1);
        SmdSetTrans(0x37, 1);
        SmdSetTrans(0x8C, 1);
        SmdSetTrans(0x8D, 1);
        SmdSetTrans(0x8E, 1);
        SmdSetTrans(0x3D, 1);
        SetSstAddAreaFlag(0x800);
        break;
    case 3:
        break;
    }
}

// Render-to-texture pass of the event camera cuts 0x21 / 0x24.
void EvtTexRenderCamTrans(Event* e, int cut)
{
    void* mod;
    void* bin;
    int skip = 1;

    if ((e->StatusFlag & EvtStfBit(EvtStfToolFrontExec)) == 0) {
        skip = 0;
    }
    if (skip == 0) {
        if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
            TexRenderModAddOt(1, (cModel*) mod);
        }
        if (e->GetMod(&mod, "pl0200", 0, 0) == 1) {
            TexRenderModAddOt(1, (cModel*) mod);
        }
        mod = SmdGetObjPtr(0xA1);
        if (mod) {
            TexRenderModAddOt(1, (cModel*) mod);
        }
        switch (cut) {
        case 0x21:
            if (EvtMgr.GetBin(&bin, "event/r20b/s00/cam/etc_s00_033.fcv", 0) == 1) {
                TexRenderCamAddOt(1, &r20b_work->cam, (TexRenderEvt*) e, bin);
            }
            break;
        case 0x24:
            if (EvtMgr.GetBin(&bin, "event/r20b/s00/cam/etc_s00_036.fcv", 0) == 1) {
                TexRenderCamAddOt(1, &r20b_work->cam, (TexRenderEvt*) e, bin);
            }
            break;
        }
    }
}
