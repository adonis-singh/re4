// the split object's .rodata is 8-aligned (the double of the int->float conversion in
// R20cExecCageMain would do it anyway; keep the unit's head identical to the other rooms).
#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "stage.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "emdoor.h"
#include "emBarred.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "read.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "math_sub.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "db_log.h"

// Room 2-0C (D:/Bio4/Prog/r20c.cpp): the cage trap dropped on the player, the enemy waves it
// releases, the painting (kaiga) that swings open when shot and the barred door behind it.

struct R20cWork {
    cEmWrap em[11];       // 0x00
    int resetIdx[2];      // 0x84  em[] indices of the two enemies the reset task watches
    int resetCnt;         // 0x8C  entries of r20c_resetTbl used so far
    u8 pad_90[4];
    cSat* sat[1];         // 0x94
    cSat* eat[1];         // 0x98
    cEmHit* kaigaHit[2];  // 0x9C
    cEmHit* hit;          // 0xA4  the shot target that opens the painting
    f32 cageY;            // 0xA8  cage rest height
    f32 doorY[2];         // 0xAC  door rest heights
};


static R20cWork* r20c_work;

Vec r20c_emGoto0 = {35700.0f, 3500.0f, 17000.0f};
Vec r20c_emGoto1 = {34000.0f, 3500.0f, 10250.0f};
Vec r20c_emGoto2 = {26700.0f, 3500.0f, 14400.0f};
Vec r20c_emGoto3 = {30300.0f, 3500.0f, 18100.0f};
Vec r20c_emGoto4 = {31890.0f, 3500.0f, 16622.0f};
Vec r20c_plPos0 = {32000.0f, 3500.0f, 16100.0f};
Vec r20c_plPos1 = {30380.0f, 3500.0f, 15900.0f};
Vec r20c_plAng0 = {0.0f, 1.27f, 0.0f};
Vec r20c_plAng1 = {0.0f, 1.27f, 0.0f};
static int r20c_resetTbl[7] = {4, 9, 7, 5, 8, 6, 0};





static void R20cEmSetMain();
void R20cExecCageUp();
void R20cExecCageDown(int lock);
static void R20cExecCageMain();
static void R20cExecCageEnd();
cEmWrap* R20cExecCageEmResetSub(int slot);
static void R20cExecCageEmReset();
void R20cKaigaMoved(int mode);
void R20cKaigaMove(int mode);
static void R20cExecShootKaigaOpenMain();
static void R20cExecShootKaigaOpenEnd();
void R20cExecShootInit();
static void R20cExecShootMain();
static void R20cDoorOpenMain();
static void R20cDoorOpenCancel(int mode);
void R20cDoorOpenEnd(int mode);
static void OpenedBoxTreasure();
static void OpenBoxTreasure();
static void SceBgmCheck();

// Room init (the cage trap room): the painting's shot target (a hit cube on object 8), the painting /
// barred door state (R20cExecShootInit), the list-3 enemy pre-read when Ashley is not along; the cage
// collision piece with the cage raised, or dropped (Room_flg bit 3) with its enemies placed at once;
// area 3 = the cage trap; one treasure item event.
void R20cInit()
{
    cObj* obj;

#line 49 "D:/Bio4/Prog/r20c.cpp"
    r20c_work = (R20cWork*) MEM_CALLOC(sizeof(R20cWork), 1, 0xd);
    obj = SmdGetObjPtr(8);
    if (obj) {
        r20c_work->hit = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &obj->pos, 0, 1);
        if (r20c_work->hit) {
            YarareInitCube(r20c_work->hit, 0.0f, -100.0f, 0.0f, 100.0f, 200.0f, 100.0f, 0, YAT_FLAG_ON);
        }
    }
    R20cExecShootInit();
    if (checkEmListNo(pG->room_id) == 3 && !StaFlagChk(pG, STA_SUB_ASHLEY)) {
        EmReadSearch((u8) GetEmIdFromList(0xCB), 0, 0);
    }
    obj = SmdGetObjPtr(6);
    if (obj) {
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        int i;

        for (i = 0; i < 1; i++) {
            r20c_work->sat[i] = NULL;
            r20c_work->eat[i] = NULL;
        }
        r20c_work->sat[0] = SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
        r20c_work->eat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 0x12), 0, &pos, &rot, 1);
        R20cExecCageUp();
        if (RsfCheck(G_ROOM_ID, 3)) {
            R20cExecCageDown(0);
            SndBgmTblSet(0x20C, 2);
            SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
            if (checkEmListNo(pG->room_id) == 3 && !StaFlagChk(pG, STA_SUB_ASHLEY)) {
                SceExec(0x12, (TaskFunc) R20cEmSetMain, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        } else {
            SceAtDataSet_exec(3, SCE_LEVEL10, 0, (TaskFunc) R20cExecCageMain, 0, 1);
            SndBgmTblSet(0x20C, 0);
            SndRoomStrStart(1, 0, 1);
        }
        obj->matUpdate();
    }
    SceSetItemEvent(0xA, 0x85, 4, 0xD, (void (*)(int)) OpenBoxTreasure, (void (*)(int)) OpenedBoxTreasure, 0, 0);
}

// Per frame, while the barred door is still closed (Room_flg bit 0) and the painting closed (Room_flg[0]
// 0x00800000 clear): a destroyed shot target starts the painting-open event.
void R20cMain()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        if (!(pG->Room_flg[0] & 0x00800000)) {
            if (r20c_work->hit && r20c_work->hit->ckStatus() == 1) {
                SceExec(0x12, (TaskFunc) R20cExecShootKaigaOpenMain, 0, 0, SCE_PRIO_DEF_2, 0);
            }
        }
    }
}

// Cage already down at entry: the enemies of the cage event are placed at once.
static void R20cEmSetMain()
{
    cEmDoor* door0;
    cEmDoor* door1;

    SceSleep(1);
    r20c_work->em[4].setEm(0xD6, -1, 0, 1, 1);
    R20cExecCageEmResetSub(0);
    getRoomEtcDoor(0xD, &door0, 1);
    getRoomEtcDoor(0xE, &door1, 1);
    if (door0) {
        door0->setDowned(0);
    }
    if (door1) {
        door1->setDowned(0);
    }
}

// Lifts the cage and the two doors out of sight, remembering their rest heights.
void R20cExecCageUp()
{
    cObj* obj;
    cEmDoor* door0;
    cEmDoor* door1;

    obj = SmdGetObjPtr(6);
    getRoomEtcDoor(0xD, &door0, 1);
    getRoomEtcDoor(0xE, &door1, 1);
    if (obj) {
        r20c_work->cageY = obj->pos.y;
        obj->setPos(obj->pos.x, obj->pos.y + 4000.0f, obj->pos.z);
    }
    if (door0) {
        r20c_work->doorY[0] = door0->pos.y;
        door0->setPos(door0->pos.x, door0->pos.y + 4000.0f, door0->pos.z);
        door0->Motion.Pos_world = door0->pos;
    }
    if (door1) {
        r20c_work->doorY[1] = door1->pos.y;
        door1->setPos(door1->pos.x, door1->pos.y + 4000.0f, door1->pos.z);
        door1->Motion.Pos_world = door1->pos;
    }
    if (r20c_work->sat[0]) {
        r20c_work->sat[0]->setDisable();
    }
    if (r20c_work->eat[0]) {
        r20c_work->eat[0]->setDisable();
    }
}

// Drops the cage and the doors to their rest heights; `lock` also locks the doors.
void R20cExecCageDown(int lock)
{
    cObj* obj;
    cEmDoor* door0;
    cEmDoor* door1;

    if (pG->Room_flg[0] & 0x80000000) {
        return;
    }
    pG->Room_flg[0] |= 0x80000000;
    obj = SmdGetObjPtr(6);
    getRoomEtcDoor(0xD, &door0, 1);
    getRoomEtcDoor(0xE, &door1, 1);
    if (obj) {
        obj->setPos(obj->pos.x, r20c_work->cageY, obj->pos.z);
    }
    if (door0) {
        door0->setPos(door0->pos.x, r20c_work->doorY[0], door0->pos.z);
        door0->Motion.Pos_world = door0->pos;
    }
    if (door1) {
        door1->setPos(door1->pos.x, r20c_work->doorY[1], door1->pos.z);
        door1->Motion.Pos_world = door1->pos;
    }
    if (lock == 1) {
        if (door0) {
            door0->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 1);
        }
        if (door1) {
            door1->setLock(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20), 0, 1);
        }
    }
    if (r20c_work->sat[0]) {
        r20c_work->sat[0]->setEnable();
    }
    if (r20c_work->eat[0]) {
        r20c_work->eat[0]->setEnable();
    }
}

// The cage event: the cage and doors come down over 20 frames, the player is put under it and
// the enemies of the wave are released.
static void R20cExecCageMain()
{
    cObj* obj;
    cEmDoor* door0;
    cEmDoor* door1;
    int i;
    int k;

    obj = SmdGetObjPtr(6);
    getRoomEtcDoor(0xD, &door0, 1);
    getRoomEtcDoor(0xE, &door1, 1);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        RsfSet(G_ROOM_ID, 3);
        SceAtSetEnable(3, 0);
        r20c_work->em[0].setEm(0xCB, -1, 0, 1, 1);
        r20c_work->em[1].setEm(0xCC, -1, 0, 1, 1);
        r20c_work->em[2].setEm(0xD2, -1, 0, 1, 1);
        r20c_work->em[3].setEm(0xD5, -1, 0, 1, 1);
        r20c_work->em[4].setEm(0xD6, -1, 0, 1, 1);
        r20c_work->em[5].setEm(0xD7, -1, 0, 1, 1);
        r20c_work->em[6].setEm(0xD8, -1, 0, 1, 1);
        r20c_work->em[7].setEm(0xD9, -1, 0, 1, 1);
        r20c_work->em[8].setEm(0xDA, -1, 0, 1, 1);
        r20c_work->em[9].setEm(0xEE, -1, 0, 1, 1);
        r20c_work->em[10].setEm(0xEF, -1, 0, 1, 1);
        SndRoomStrStop(3);
        SndBgmTblSet(0x20C, 1);
        SndRoomStrStart(1, 0, 1);
        SceExec(0x12, (TaskFunc) SceBgmCheck, 0, 0, SCE_PRIO_DEF_2, 0);
        SceEventStart(0);
        SceSetEventCancel(1, (TaskFunc) R20cExecCageEnd, 0, -1, 1);
        CamCtrl.CutCall(6);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, ESP_CORE_KIND_NONE, 0, 0);
        pPL->beginEvent(0);
        pPL->setNoSuspend(1);
        pPL->setPos(&r20c_plPos0);
        pPL->setAng(&r20c_plAng0);
        MotionSetCore(pPL, &pPL->Motion, ROOM_ARC_PTR(pG->pRoom, 0x21), 0, 0, 1, 0);
        SndCall(6, 0, &pPL->pos, 0, 0, 0);
        for (i = 0; i < 20; i++) {
            if (obj) {
                obj->setPos(obj->pos.x, r20c_work->cageY + 4000.0f - (f32) i * 4000.0f / 20.0f, obj->pos.z);
            }
            if (door0) {
                door0->setPos(door0->pos.x, r20c_work->doorY[0] + 4000.0f - (f32) i * 4000.0f / 20.0f, door0->pos.z);
                door0->Motion.Pos_world = door0->pos;
            }
            if (door1) {
                door1->setPos(door1->pos.x, r20c_work->doorY[1] + 4000.0f - (f32) i * 4000.0f / 20.0f, door1->pos.z);
                door1->Motion.Pos_world = door1->pos;
            }
            SceSleep(1);
        }
        SndCall(6, 1, &pPL->pos, 0, 0, 0);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        pPL->setPos(&r20c_plPos1);
        pPL->setAng(&r20c_plAng1);
        MotionSetCore(pPL, &pPL->Motion, PL_ARC_PTR(pG->pPlayer, 0x32), 0, 0, 4, 0);
        R20cExecCageDown(1);
        CamCtrl.CutCall(7);
        r20c_work->em[0].setGoto(&r20c_emGoto0, 1);
        r20c_work->em[1].setGoto(&r20c_emGoto1, 1);
        r20c_work->em[0].setNoSuspend(1);
        r20c_work->em[1].setNoSuspend(1);
        SceSleep(50);
        CamCtrl.CutCall(8);
        r20c_work->em[2].setGoto(&r20c_emGoto2, 1);
        r20c_work->em[3].setGoto(&r20c_emGoto3, 1);
        r20c_work->em[2].setNoSuspend(1);
        r20c_work->em[3].setNoSuspend(1);
        SceSleep(50);
        CamCtrl.CutCall(9);
        r20c_work->resetCnt = 0;
        for (k = 0; k < 2; k++) {
            R20cExecCageEmResetSub(k)->setNoSuspend(1);
        }
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSetEventCancel(0, 0, 0, -1, 1);
        R20cExecCageEnd();
    }
}

// End of the cage drop (also its cancel path): the cage and doors down and locked, Leon put inside
// facing 1.27 rad, the eleven enemies may suspend again, the first four run to their posts, the reset
// counter starts.
static void R20cExecCageEnd()
{
    int i;

    R20cExecCageDown(1);
    SndCall(6, 1, &pPL->pos, 0, 0, 0);
    pPL->setPos(&r20c_plPos1);
    pPL->setAng(&r20c_plAng1);
    r20c_work->em[0].setNoSuspend(0);
    r20c_work->em[1].setNoSuspend(0);
    r20c_work->em[2].setNoSuspend(0);
    r20c_work->em[3].setNoSuspend(0);
    r20c_work->em[4].setNoSuspend(0);
    r20c_work->em[5].setNoSuspend(0);
    r20c_work->em[6].setNoSuspend(0);
    r20c_work->em[7].setNoSuspend(0);
    r20c_work->em[8].setNoSuspend(0);
    r20c_work->em[9].setNoSuspend(0);
    r20c_work->em[10].setNoSuspend(0);
    r20c_work->em[0].setGoto(&r20c_emGoto0, 1);
    r20c_work->em[1].setGoto(&r20c_emGoto1, 1);
    r20c_work->em[2].setGoto(&r20c_emGoto2, 1);
    r20c_work->em[3].setGoto(&r20c_emGoto3, 1);
    r20c_work->resetCnt = 0;
    for (i = 0; i < 2; i++) {
        R20cExecCageEmResetSub(i);
    }
    SceExec(0x12, (TaskFunc) R20cExecCageEmReset, 0, 0, SCE_PRIO_DEF_2, 0);
    SceEventEnd(0);
    SceExit();
}

// Takes the next enemy of r20c_resetTbl for watch slot `slot` (the 5th entry is replaced by the
// 11th enemy on the low difficulty ranks) and releases it.
cEmWrap* R20cExecCageEmResetSub(int slot)
{
    int no = r20c_resetTbl[r20c_work->resetCnt];
    cEmWrap* em;

    if (no == 4 && pG->Game_level <= 2) {
        no = 10;
    }
    em = &r20c_work->em[no];
    if (em) {
        em->setFlag(1);
    }
    r20c_work->resetIdx[slot] = no;
    r20c_work->resetCnt++;
    return em;
}

// Releases the next enemy 6 seconds after one of the two watched ones goes inactive.
static void R20cExecCageEmReset()
{
    int slot;
    int wait;
    int i;

    SceSleep(1);
    for (;;) {
        wait = 1;
        slot = 0;
        do {
            for (i = 0; i < 2; i++) {
                if (r20c_work->em[r20c_work->resetIdx[i]].isActive() == 0) {
                    slot = i;
                    wait = 0;
                    break;
                }
            }
            SceSleep(1);
        } while (wait != 0);
        for (i = 0; i < 360; i++) {
            SceSleep(1);
        }
        R20cExecCageEmResetSub(slot);
        if (r20c_work->resetCnt > 5) {
            break;
        }
        SceSleep(1);
    }
}

// Painting end state: `mode` 1 = open (shot target hit), 0 = closed.
void R20cKaigaMoved(int mode)
{
    cObj* obj7;
    cObj* obj9;

    obj7 = SmdGetObjPtr(7);
    obj9 = SmdGetObjPtr(9);
    if (obj7 && obj9) {
        if (mode == 1) {
            obj7->setAng(obj7->ang.x, obj7->ang.y, 0.0f);
            obj9->setAng(obj9->ang.x, obj9->ang.y, 0.0f);
            pG->Room_flg[0] |= 0x01000000;
            if (r20c_work->kaigaHit[0]) {
                r20c_work->kaigaHit[0]->hp = 1;
            }
            if (r20c_work->kaigaHit[1]) {
                r20c_work->kaigaHit[1]->hp = 1;
            }
            SceAtSetEnable(9, 0);
        } else {
            s16 hp;

            obj7->setAng(obj7->ang.x, obj7->ang.y, 3.1415927f);
            obj9->setAng(obj9->ang.x, obj9->ang.y, 3.1415927f);
            pG->Room_flg[0] &= ~0x01000000;
            // COMPILER-DIFF: #5 — the original's `li r8,0` of the two `hp = 0` stores is one constant
            // hoisted above the first null test (interblock motion); a local set here gives the shape.
            hp = 0;
            if (r20c_work->kaigaHit[0]) {
                r20c_work->kaigaHit[0]->hp = hp;
            }
            if (r20c_work->kaigaHit[1]) {
                r20c_work->kaigaHit[1]->hp = hp;
            }
            SceAtSetEnable(9, 1);
        }
    }
}

// Swings the painting open (`mode` 1) or closed.
void R20cKaigaMove(int mode)
{
    cObj* obj7;
    cObj* obj9;

    obj7 = SmdGetObjPtr(7);
    obj9 = SmdGetObjPtr(9);
    if (obj7 && obj9) {
        if (mode == 1) {
            SndCall(6, 3, &obj7->pos, 0, 0, 0);
            if (pG->Room_flg[0] & 0x01000000) {
                obj7->setAng(obj7->ang.x, obj7->ang.y, 0.0f);
                obj9->setAng(obj9->ang.x, obj9->ang.y, 0.0f);
            } else {
                pG->Room_flg[0] |= 0x01000000;
                while (obj7->ang.z >= 0.0f) {
                    obj7->setAng(obj7->ang.x, obj7->ang.y, obj7->ang.z - 0.08726647f);
                    obj9->setAng(obj9->ang.x, obj9->ang.y, obj9->ang.z - 0.08726647f);
                    SceSleep(1);
                }
                R20cKaigaMoved(mode);
            }
        } else {
            SndCall(6, 4, &obj7->pos, 0, 0, 0);
            if (!(pG->Room_flg[0] & 0x01000000)) {
                obj7->setAng(obj7->ang.x, obj7->ang.y, 3.1415927f);
                obj9->setAng(obj9->ang.x, obj9->ang.y, 3.1415927f);
            } else {
                pG->Room_flg[0] &= ~0x01000000;
                while (obj7->ang.z <= 3.1415927f) {
                    obj7->setAng(obj7->ang.x, obj7->ang.y, obj7->ang.z + 0.34906587f);
                    obj9->setAng(obj9->ang.x, obj9->ang.y, obj9->ang.z + 0.34906587f);
                    SceSleep(1);
                }
                R20cKaigaMoved(mode);
            }
        }
    }
}

// The shot target was hit (once, Room_flg[0] 0x00800000): SE, camera cut 0xB while the painting swings open; player-cancellable.
static void R20cExecShootKaigaOpenMain()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        if (!(pG->Room_flg[0] & 0x00800000)) {
            cObj* obj;

            pG->Room_flg[0] |= 0x00800000;
            obj = SmdGetObjPtr(8);
            if (obj) {
                SndCall(6, 2, &obj->pos, 0, 0, 0);
            }
            SceEventStart(0);
            SceSetEventCancel(1, (TaskFunc) R20cExecShootKaigaOpenEnd, 0, -1, 1);
            CamCtrl.CutCall(0xB);
            R20cKaigaMove(1);
            SceSleep(10);
            SceSetEventCancel(0, 0, 0, -1, 1);
            R20cExecShootKaigaOpenEnd();
        }
    }
}

// End of the painting opening: pose it open, start the two-target watcher behind it, SceEventEnd, task exit.
static void R20cExecShootKaigaOpenEnd()
{
    R20cKaigaMoved(1);
    SceExec(0x12, (TaskFunc) R20cExecShootMain, 0, 0, SCE_PRIO_DEF_2, 0);
    SceEventEnd(0);
    SceExit();
}

// Painting setup: door already open (Room_flg bit 0) -> barred door opened, painting posed open, object
// 9 hidden, ambient effect; else the barred door closed, area 9 = open the painting, the two hit cubes
// behind the painting (objects 7 and the second target) for the door puzzle.
void R20cExecShootInit()
{
    void* zero = 0;

    if (RsfCheck(G_ROOM_ID, 0)) {
        R20cDoorOpenCancel(0);
        R20cKaigaMoved(1);
        SmdSetTrans(9, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_NONE, zero, zero);
    } else {
        cEm* barred;
        cObj* obj;

        getRoomEtcBarred(0, &barred, 1);
        if (barred) {
            ((cEmBarred*) barred)->setClosed();
        }
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) R20cExecShootKaigaOpenMain, 0, 1);
        obj = SmdGetObjPtr(7);
        if (obj) {
            r20c_work->kaigaHit[0] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &obj->pos, 0, 1);
            if (r20c_work->kaigaHit[0]) {
                YarareInitCube(r20c_work->kaigaHit[0], 0.0f, 0.0f, -300.0f, 150.0f, 450.0f, 200.0f, 0, YAT_FLAG_ON);
            }
            r20c_work->kaigaHit[1] = SetEmHit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore), (void*) (pG->pCore->ofs_24 + (u32) pG->pCore), &obj->pos, 0, 1);
            if (r20c_work->kaigaHit[1]) {
                YarareInitCube(r20c_work->kaigaHit[1], 0.0f, -850.0f, 0.0f, 50.0f, 1700.0f, 1100.0f, 0, YAT_FLAG_ON);
            }
        }
        obj = SmdGetObjPtr(9);
        if (obj) {
            obj->ot_type = 4;
        }
        R20cKaigaMoved(0);
    }
}

// Waits for a shot on the open painting's targets: the upper one opens the barred door, the
// lower one closes the painting again.
static void R20cExecShootMain()
{
    for (;;) {
        if (r20c_work->kaigaHit[0] && r20c_work->kaigaHit[0]->ckStatus() == 1) {
            SceExec(0x12, (TaskFunc) R20cDoorOpenMain, 0, 0, SCE_PRIO_DEF_2, 0);
            return;
        }
        if (r20c_work->kaigaHit[1] && r20c_work->kaigaHit[1]->ckStatus() == 1) {
            goto found;
        }
        SceSleep(1);
    }
found:
    R20cKaigaMove(0);
    pG->Room_flg[0] &= ~0x00800000;
}

// Both targets behind the painting destroyed (once, Room_flg bit 0): their hp zeroed, area 9 off,
// camera cut 0xE while the bar object 9 drops with SE / effect and the barred door opens; cancellable.
static void R20cDoorOpenMain()
{
    cEm* barred;
    int i;

    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        cObj* obj;

        RsfSet(G_ROOM_ID, 0);
        if (r20c_work->kaigaHit[0]) {
            r20c_work->kaigaHit[0]->hp = 0;
        }
        if (r20c_work->kaigaHit[1]) {
            r20c_work->kaigaHit[1]->hp = 0;
        }
        SceAtSetEnable(9, 0);
        SceEventStart(0);
        SceSetEventCancel(1, (TaskFunc) R20cDoorOpenCancel, 1, -1, 1);
        CamCtrl.CutCall(0xE);
        SmdSetTrans(9, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, 0, 0);
        obj = SmdGetObjPtr(9);
        if (obj) {
            SndCall(6, 5, &obj->pos, 0, 0, 0);
        }
        for (i = 0; i < 30; i++) {
            SceSleep(1);
        }
        CamCtrl.CutCall(0xC);
        getRoomEtcBarred(0, &barred, 1);
        if (barred) {
            ((cEmBarred*) barred)->setOpen(0);
        }
        SceSleep(40);
        SceSetEventCancel(0, 0, 0, -1, 1);
        R20cDoorOpenEnd(1);
    }
}

// Cancel / end path of the door opening: snap the barred door open, then the common end (mode 1 saves).
static void R20cDoorOpenCancel(int mode)
{
    cEm* barred;

    getRoomEtcBarred(0, &barred, 1);
    if (barred) {
        ((cEmBarred*) barred)->setOpened();
    }
    R20cDoorOpenEnd(mode);
}

// End of the door opening (mode 1): SceEventEnd, autosave, task exit.
void R20cDoorOpenEnd(int mode)
{
    if (mode) {
        SceEventEnd(0);
        GameSave.save(pSaveData, -1);
        SceExit();
    }
}

// Item-event "already opened": the chest (object 0x11) posed open.
static void OpenedBoxTreasure()
{
    OpenBoxMain(OpenBoxUpZM, 1, 0x5B, 0x11, -1, -1);
}

// Item-event opener: the chest (object 0x11) lid up (-Z).
static void OpenBoxTreasure()
{
    OpenBoxMain(OpenBoxUpZM, 0, 0x5B, 0x11, -1, -1);
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
