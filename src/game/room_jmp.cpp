// game/room_jmp: the debug room-jump menu (RoomJump task, from the debug menu) and cRoomJmp, the
// reader of the room info table (roomInfoAddr: per stage a list of CRoomInfo jump points with
// position, angle, room name, screen and programmer). GetNextPos lets the scenario use a jump
// point as the next room entry.
#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "main.h"
#include "mes.h"
#include "room_jmp.h"
#include "ref_access.h"

// Room jump tool work (0x38 bytes)
struct test {
    s8 state;      // 0x00  tbl index
    s8 mode;       // 0x01  cursor line: 0 stage, 1 room, 2 point
    s8 stage;      // 0x02
    s8 room[12];   // 0x03  selected room index per stage
    s8 point;      // 0x0F
    u8 flag;       // 0x10  1 = a jump was executed
    u8 pad_11[3];
    u32 stop_bak;  // 0x14  pG->flags_170
    u8 pad_18[0x38 - 0x18];
};

extern "C" {
void roomJumpInit(test* w);
void roomJumpMove(test* w);
void roomJumpExec(test* w);
void roomJumpExit(test* w);
}

// The original stores GlobalWork fields through references: GCC then reloads pG after every store.

// Stage offset table right after the count; as an inline the base stays a pointer register (lwzx).
static inline u32* ofsTbl(u32* tbl)
{
    return tbl + 1;
}

cRoomJmp* pRj;

// Makes this jump point the next room entry: NextPos / NextY from the record (zero when flag bit0
// says it has none), room_id_prev / Part_old kept, next_room = roomNo, next_point 0.
void CRoomInfo::setNextPos()
{
    if (flag & 1) {
        pG->NextPos.x = pos.x;
        pG->NextPos.y = pos.y;
        pG->NextPos.z = pos.z;
        pG->NextY = angle;
    } else {
        pG->NextPos.x = 0.0f;
        pG->NextPos.y = 0.0f;
        pG->NextPos.z = 0.0f;
        pG->NextY = 0.0f;
    }
    pG->room_id_prev = pG->room_id;
    pG->Part_old = pG->Part;
    pG->RoomNo_next = roomNo;
    pG->Part_next = 0;
}

// Wraps the room info table (count, per-stage offsets, CRoomInfo records) and, on first use,
// relocates its name / screen / programmer string offsets into pointers.
cRoomJmp::cRoomJmp(void* p)
{
    u32 stage;
    int i;
    CRoomInfo* info;

    tbl = (u32*) p;
    for (stage = 0; stage < tbl[0]; stage++) {
        if (getIndexNum(stage) == 0) {
            continue;
        }
        for (i = 0; i < getIndexNum(stage); i++) {
            info = getRoomInfo(stage, i);
            if (info == 0) {
                continue;
            }
            if ((u32) info->name >= 0x80000000 && (u32) info->name <= 0x82FFFFFF) {
                return;
            }
            info->name = (char*) ((u32) tbl + (u32) info->name);
            info->person = (char*) ((u32) tbl + (u32) info->person);
            info->person2 = (char*) ((u32) tbl + (u32) info->person2);
        }
    }
}

// Number of jump-point records of `stage` (0 when the stage has no table).
s8 cRoomJmp::getIndexNum(s8 stage)
{
    u32* p = tbl;
    u32 ofs = ofsTbl(p)[stage];

    if (ofs == 0) {
        return 0;
    }
    return *((s8*) p + ofs + 3);
}

// Number of jump points of `room`: consecutive records with the same room number.
s8 cRoomJmp::getPointNum(s8 stage, s8 room)
{
    int count = 1;
    u8 idx = getRoomIdx(stage, room);
    u32 n = getIndexNum(stage);

    do {
        idx = (n + idx + 1) % n;
        if (getRoomInfo(stage, idx)->room != room) {
            break;
        }
        count++;
    } while (1);
    return count;
}

// Record `idx` of `stage`, or 0 when out of range.
CRoomInfo* cRoomJmp::getRoomInfo(u8 st, u8 idx)
{
    u32* p = tbl;
    u32 ofs;
    u32 n;
    u32 base;

    if (st >= p[0]) {
        return 0;
    }
    // COMPILER-DIFF: tie. The loop notes double the weight of this `ofs` set, so local-alloc
    // allocates ofs before n (ofs r0, n r11) and global-alloc can give base the freed r0.
    do { ofs = (p + 1)[st]; } while (0);
    n = *(u32*) ((u8*) p + ofs);
    base = (u32) p + ofs;
    if (idx >= n) {
        return 0;
    }
    {
        u32 o = idx * sizeof(CRoomInfo) + 4;
        return (CRoomInfo*) (base + o);
    }
}

// Index of the first record of `room` in `stage` (0 when not found).
u8 cRoomJmp::getRoomIdx(u8 st, u8 room)
{
    int i;
    u8 idx;

    for (i = 0; i < getIndexNum(st); i++) {
        idx = i;
        if (getRoomInfo(st, idx)->room == room) {
            return idx;
        }
    }
    return 0;
}

// Next room entry = the first jump point of stage / room.
void cRoomJmp::setNextPos(u8 Stage, u8 Room)
{
    getRoomInfo(Stage, getRoomIdx(Stage, Room))->setNextPos();
}

// Next (dir +1) / previous (-1) stage that has a table, wrapping.
s8 cRoomJmp::getNextStageNo(s8 stage, int add)
{
    u32* p = tbl;
    u32 n = p[0];

    do {
        stage = (n + stage + add) % n;
    } while (ofsTbl(p)[stage] == 0);
    return stage;
}

// Index of the first record of the next / previous room after the one at `idx`, wrapping.
s8 cRoomJmp::getNextRoomNo(s8 stage, s8 idx, int add)
{
    u32 n;
    CRoomInfo* cur;
    CRoomInfo* info;
    s8 next;

    if (add == 0) {
        return idx;
    }
    n = getIndexNum(stage);
    cur = getRoomInfo(stage, idx);
    for (;;) {
        idx = (n + idx + add) % n;
        info = getRoomInfo(stage, idx);
        if (cur == info) {
            return idx;
        }
        if (cur->roomNo != info->roomNo) {
            break;
        }
    }
    if (add >= 0) {
        return idx;
    }
    cur = info;
    for (;;) {
        next = (n + idx - 1) % n;
        info = getRoomInfo(stage, next);
        if (cur == info) {
            return idx;
        }
        if (info->roomNo != cur->roomNo) {
            return idx;
        }
        idx = next;
    }
}

// Next / previous jump point of `room` (stays when the neighbour belongs to another room).
s8 cRoomJmp::getNextPointNo(s8 stage, s8 room, s8 point, s8 add)
{
    u32 n = getIndexNum(stage);
    s8 idx = getRoomIdx(stage, room) + point;
    u16 room_id = (stage << 8) | room;
    s8 next;

    next = (n + idx + add) % n;
    if (getRoomInfo(stage, next)->roomNo == room_id) {
        idx = next;
    }
    return idx - getRoomIdx(stage, room);
}

// `room` when it is a valid record of `stage`, else -1.
s8 cRoomJmp::checkRoomNo(s8 stage, s8 room)
{
    if ((u8) stage >= tbl[0] || getIndexNum(stage) == 0 || getRoomInfo(stage, room) == 0) {
        return -1;
    }
    return room;
}

// Debug room-jump menu task (bugcheck controller): init -> move (menu) -> exec / exit.
void RoomJump()
{
    static test test;
    static void (*tbl[])(struct test*) = {roomJumpInit, roomJumpMove, roomJumpExec, roomJumpExit};
    struct test* w = &test;

    memclr_asm(w, sizeof(test));
    for (;;) {
        tbl[w->state](w);
        TaskSleep(1);
    }
}

// Freezes the game (Stop_flg), builds the cRoomJmp on the room info table and starts the cursor at
// the current stage / room / jump point.
void roomJumpInit(test* pTest)
{
    pTest->state++;
    pTest->stop_bak = pG->Stop_flg;
    BitOn(pG->Stop_flg, 0xFFFFBFFF);
    pRj = new cRoomJmp(roomInfoAddr);
    pTest->stage = pG->stage_no;
    pTest->room[pTest->stage] = pRj->getRoomIdx(pG->stage_no, pG->room_no);
    pTest->point = pG->JumpPoint;
    pTest->flag = 0;
}

// Menu frame: up/down pick the line (stage / room / point), left/right change it (repeat keys);
// prints the room name, screen and programmer; button 0x100 jumps, 0x200 cancels.
void roomJumpMove(test* w)
{
    JOY* joy = GetBugCheckController();
    CRoomInfo* info;
    int no;
    s8 room;
    int pt;

    if (joy->rep & 0x40004) {
        w->mode++;
    }
    if (joy->rep & 0x80008) {
        w->mode--;
    }
    w->mode = (w->mode < 0) ? 2 : ((w->mode > 2) ? 0 : w->mode);
    info = pRj->getRoomInfo(w->stage, w->room[w->stage] + w->point);
    eprintf(0xD8, 0x38, 0, 0, "STAGE = %d", w->stage);
    eprintf(0xD8, 0x46, 0, 0, "ROOM  = %02x", info->room);
    eprintf(0xD8, 0x54, 0, 0, "POINT = %d", w->point);
    eprintf(0xD8, 0x2A, 4, 0, "%s", info->name);
    if (info->person2[0] != 0) {
        eprintf(0xD8, 0x1C, 0, 0, "     SOFT(%s)", info->person2);
    }
    if (info->person[0] != 0) {
        eprintf(0xD8, 0xE, 0, 0, "     SCR(%s)", info->person);
    }
    eprintf(0xD0, (w->mode + 4) * 0xE, 0, 0, ">");
    if (joy->trg & 0x100) {
        w->state = 2;
    }
    if (joy->trg & 0x200) {
        w->state = 3;
    }
    no = w->mode;
    switch (no) {
    case 0:
        if (joy->rep2 & 0x20002) {
            w->stage = pRj->getNextStageNo(w->stage, 1);
            w->point = 0;
        }
        if (joy->rep2 & 0x10001) {
            w->stage = pRj->getNextStageNo(w->stage, -1);
            w->point = 0;
        }
        no = pRj->checkRoomNo(w->stage, w->room[w->stage]);
        if (no >= 0) {
            w->room[w->stage] = no;
        }
        break;
    case 1:
        if (joy->rep2 & 0x20002) {
            w->room[w->stage] = pRj->getNextRoomNo(w->stage, w->room[w->stage], 1);
            w->point = 0;
        }
        if (joy->rep2 & 0x10001) {
            w->room[w->stage] = pRj->getNextRoomNo(w->stage, w->room[w->stage], -1);
            w->point = 0;
        }
        break;
    case 2:
        room = pRj->getRoomInfo(w->stage, w->room[w->stage])->room;
        if (joy->rep2 & 0x20002) {
            w->point = pRj->getNextPointNo(w->stage, room, w->point, 1);
        }
        if (joy->rep2 & 0x10001) {
            w->point = pRj->getNextPointNo(w->stage, room, w->point, -1);
        }
        // Nested s8 ternary through an s8& setter: the value is a QImode temp (a promoted int local
        // needs two insns for the byte load), so jump1 hoists the `li 0` above the compare, the temp
        // conflicts with r3 and takes r9, and case 1's `stb r9` is cross-jumped into the final store.
        pt = w->point;
        S8Set(w->point, (pt < 0) ? pRj->getPointNum(w->stage, room) - 1
                                 : ((pt > pRj->getPointNum(w->stage, room) - 1) ? (s8) 0 : w->point));
        break;
    }
}

// Performs the jump: everything stopped, Debug_flg[2] bit31 (debug jump), the chosen point becomes
// the next room entry, messages cleared, life refilled.
void roomJumpExec(test* pTest)
{
    pTest->state++;
    pG->Stop_flg = 0xFFFFFFFF;
    DbgFlagOn(pG, DBG_ROOMJMP);
    pRj->getRoomInfo(pTest->stage, pTest->room[pTest->stage] + pTest->point)->setNextPos();
    pG->JumpPoint = pTest->point;
    cMes.roomInit();
    cMes.Clear();
    pG->pl_life = pG->pl_life_max;
    pG->r_continue_cnt = 0;
    pTest->flag = 1;
}

// Leaves the menu: restores Stop_flg; after a jump sets the game routine to 4 (room change) and
// clears System_flg 0x40.
void roomJumpExit(test* pTest)
{
    delete pRj;
    if (pTest->flag == 1) {
        pG->Rno0 = 4;
        pG->Rno1 = 0;
        pG->Rno2 = 0;
        pG->Rno3 = 0;
        SysFlagOff(pG, SYS_START_EVT_SKIP);
    }
    pG->Stop_flg = pTest->stop_bak;
    DbgFlagOff(pG, DBG_TEST_MODE);
    TaskExit();
}

// One-shot: sets the next room entry to the first jump point of stage / room (used by the scenario).
void GetNextPos(u8 Stage, u8 Room)
{
    pRj = new cRoomJmp(roomInfoAddr);
    pRj->setNextPos(Stage, Room);
    delete pRj;
}
