#include "types.h"
#include "light.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "file.h"
#include "area.h"
#include "block.h"
#include "obj.h"
#include "player.h"
#include "datactrl.h"
#include "db_cam.h"
#include "dbmodule.h"
#include "math_sub.h"
#include "t_util.h"
#include <stdio.h>
#include <string.h>

// Room block ("BLK" file) editor of the t_sce REL (D:/Bio4/Prog/t_block.cpp is not in the binary:
// no HALT string). Same skeleton as Tools' t_dr.cpp: block link table, trigger areas, per-area
// connect table, load/save of the file image the game's cBlock reads.

int SetToolLight(int no);  // t_sce's db_light_v2 copy

#define BLOCK_NUM 32
#define AREA_NUM 128
#define CONNECT_NUM 128

// The tool's view of the file header: nBlock is a halfword here.
struct TBlockHeader {
    char tag[4];      // 0x00  "BLK"
    u16 version;      // 0x04  0x100
    u16 nBlock;       // 0x06
    u16 nArea;        // 0x08
    u16 nConnect;     // 0x0A
    u32 ofsLink;      // 0x0C
    u32 ofsArea;      // 0x10
    u32 ofsConnect;   // 0x14
};

// The tool's view of BlockArea: the area number is unsigned here.
struct TBlockArea {
    u32 tag;          // 0x00
    AreaData area;    // 0x04
    u8 flags;         // 0x34  bit0: active, bit1: initialised
    u8 slot;          // 0x35  slot of the area in the tool's table (written with pW->areaNo, indexes pW->area)
    u8 areaNo;        // 0x36
    u8 pri;           // 0x37
};

struct TBlockFile {
    TBlockHeader hdr;               // 0x00
    BlockLink link[BLOCK_NUM];      // 0x18
    TBlockArea area[AREA_NUM];      // 0x198
    BlockConnect connect[CONNECT_NUM];  // 0x1D98
};

struct TBlockWork {
    u8 mode;             // 0x00  routine index
    u8 sub;              // 0x01
    u8 step;             // 0x02
    u8 step2;            // 0x03
    u8 blink;            // 0x04  frame counter (block model blink)
    u8 camMode;          // 0x05  debug camera
    s8 cursor;           // 0x06  main menu
    s8 areaCursor;       // 0x07  area edit menu
    s8 connectCursor;    // 0x08  connect menu (link slot)
    s8 infoCursor;       // 0x09  area info: mram/aram slot
    s8 infoMenuCursor;   // 0x0A  area info menu
    s8 exitCursor;       // 0x0B
    s8 saveSel;          // 0x0C
    u8 pad_D;
    s16 timer;           // 0x0E
    s16 areaNo;          // 0x10
    s16 blockNo;         // 0x12
    s16 connectNo;       // 0x14
    s16 listTop;         // 0x16
    s16 x;               // 0x18
    s16 y;               // 0x1A
    s16 x0;              // 0x1C
    s16 y0;              // 0x1E
    u32 saveStopFlag;    // 0x20  pG->flags_170 at start
    u32 saveDispFlag;    // 0x24  pG->flags_58 at start
    int nArea;           // 0x28  highest used area number + 1
    Vec pos[BLOCK_NUM];  // 0x2C  block box centres
    u32 mram;            // 0x1AC  cBlock::checkBlockConnect result of the current area
    u32 aram;            // 0x1B0
    u32 pad_1B4;
    u32 areaBits[4];     // 0x1B8  bit per used area number
    char path[0x40];     // 0x1C8  d:\ path
    char pathX[0x40];    // 0x208  x:\ path
    cBlockUnit unit[BLOCK_NUM];  // 0x248  cBlock::pUnit while the tool runs
    u8 pad_448[0x18];
    BlockLink link[BLOCK_NUM];   // 0x460
    TBlockArea area[AREA_NUM];   // 0x5E0
    BlockConnect connect[CONNECT_NUM];  // 0x21E0
    TBlockFile file;             // 0x2BE0  load / save image
    TBlockHeader* pFile;         // 0x5378
    BlockLink* pLink;            // 0x537C
    TBlockArea* pArea;           // 0x5380
    BlockConnect* pConnect;      // 0x5384
    u32 fileSize;                // 0x5388
};

static TBlockWork* blockWk;
#define pW (blockWk)

extern "C" {
void tBlockInit_base();
void tBlockInit();
void set_filename();
static void tBlockExit();
static void tBlockMainMenu();
static void tBlockConnect();
static void tBlockConnect_ListDisp();
void dispBlockList1(int x, int y, int no);
static void tBlockConnect_Menu();
static void tBlockConnect_Delete();
static void tBlockArea();
static void tBlockArea_ListDisp();
void dispAreaList1(int x, int y, int no);
static void tBlockArea_Menu();
static void tBlockArea_Create();
static void tBlockArea_Delete();
static void tBlockArea_Move();
static void tBlockAreaInfo();
static void tBlockAreaInfo_ListDisp();
void dispAreaInfoList1(int x, int y, int no);
static void tBlockAreaInfo_Menu();
void tBlockArea_disp();
void tBlockArea_dispBlockModel(int on);
void tBlockArea_dispBlockArea(u8 no, u32 col);
void tBlockArea_dispBlockBox(u8 no, u32 col);
static void tBlockDataLoad();
static void tBlockDataSave();
void tBlockSaveDataCreate();
void tBlock_DebugCamera();
}



#define SUB_RESET() \
    pW->sub = 0;    \
    pW->step = 0;   \
    pW->step2 = 0;

#define MODE_RESET() \
    pW->mode = 0;    \
    pW->sub = 0;     \
    pW->step = 0;    \
    pW->step2 = 0;

#define STEP_RESET() \
    pW->step = 0;    \
    pW->step2 = 0;

// s16 clamp to lo..hi through a u16 copy (the raw halfword is stored back when in range)
#define LIMIT(field, lo, hi)          \
    {                                 \
        TBlockWork* w = pW;           \
        u16 n = w->field;             \
        if ((s16) n >= (lo)) {        \
            if ((s16) n > (hi)) {     \
                n = hi;               \
            }                         \
        } else {                      \
            n = lo;                   \
        }                             \
        w->field = n;                 \
    }

// BLOCK DATA EDIT TOOL entry (debug menu 31): edits the room's BLK data (block link table, block
// trigger areas, per-area MRAM/ARAM load lists). Loops: sub stick moves the panel, START toggles
// the debug camera, then routine[mode] (main menu, connect, area edit, area info, load, save,
// exit) and the block / area display.
void ToolBlock()
{
    void (*routine[7])() = {tBlockMainMenu, tBlockConnect, tBlockArea,  tBlockAreaInfo,
                            tBlockDataLoad, tBlockDataSave, tBlockExit};

    pW = (TBlockWork*) Debug_alloc(sizeof(TBlockWork), 1);
    tBlockInit();
    pW->mode = 4;
    pW->sub = 0;
    pW->step = 0;
    pW->step2 = 0;
    while (1) {
        pW->blink++;
        if (pW->camMode == 0) {
            if (Joy[0].on & JOY_SSRIGHT) pW->x0 += 8;
            if (Joy[0].on & JOY_SSLEFT) pW->x0 -= 8;
            if (Joy[0].on & JOY_SSDOWN) pW->y0 += 8;
            if (Joy[0].on & JOY_SSUP) pW->y0 -= 8;
        }
        pW->x = pW->x0;
        pW->y = pW->y0;
        eprintf(pW->x, pW->y, 4, 0, "[BLOCK DATA EDIT TOOL]");
        pW->y += 0x20;
        pW->x += 8;
        tBlockArea_disp();
        if (Joy[0].trg & JOY_START) {
            pW->camMode ^= 1;
        }
        if (pW->camMode) {
            tBlock_DebugCamera();
        } else {
            routine[pW->mode]();
        }
        TaskSleep(1);
    }
}

// Flag setup shared with the other room editors: pause the game, debug displays on, tool light 1.
void tBlockInit_base()
{
    *((u8*) &pG->debug_mode) = 0x11;
    DbgFlagOn(pG, DBG_BACK_CLIP);
    pG->Stop_flg |= 0x20000000;
    pG->Stop_flg |= 0x10000000;
    pG->Stop_flg |= 0x8000000;
    pG->Stop_flg |= 0x800000;
    pG->Stop_flg |= 0x400000;
    pG->Stop_flg |= 0x10000;
    pG->Stop_flg |= 0x2000;
    pG->Stop_flg |= 0x200;
    pG->Disp_flg |= 0x20000000;
    pG->Disp_flg |= 0x40000000;
    pG->Disp_flg |= 0x80000000;
    pG->Disp_flg |= 0x4000000;
    pG->Disp_flg |= 0x2000000;
    pG->Disp_flg |= 0x100000;
    DbgFlagOn(pG, DBG_DBG_CAM);
    SetToolLight(1);
}

// Tool start: default tool state, file header ("BLK" 0x100), file names for the room, the live
// cBlock data (links, areas, connects) copied into the work and the tool's cBlockUnit table
// installed as cBlock::pUnit; block box centres computed.
void tBlockInit()
{
    u32 i;
    int j;

    TutilInitDefault();
    pW->saveStopFlag = pG->Stop_flg;
    pW->saveDispFlag = pG->Disp_flg;
    tBlockInit_base();
    pW->x0 = 0x28;
    pW->y0 = 0xA;
    for (i = 0; i < BLOCK_NUM; i++) {
        for (j = 0; j < 8; j++) {
            pW->link[i].link[j] = -1;
        }
    }
    for (i = 0; i < CONNECT_NUM; i++) {
        for (j = 0; j < 8; j++) {
            pW->connect[i].mram[j] = -1;
            pW->connect[i].aram[j] = -1;
        }
    }
    set_filename();
    file_lock(pW->pathX);
    for (i = 0; i < Block.nBlock; i++) {
        cBlockUnit* u = Block.getUnitPtr(i);

        if (u->flags & 1) {
            u->setBlockDelete();
        }
    }
    TaskSleep(2);
    DC.setUseDebugMemFlag(1);
    Block.noMemCtrl = 1;
    if (Block.debugData == 1) {
        Debug_free(Block.pData);
        Debug_free(Block.getUnitPtr(0));
    }
    Block.nBlock = BLOCK_NUM;
    Block.pUnit = pW->unit;
}

// Server (d:) and local (x:) paths of the room's r<room>.blk.
void set_filename()
{
    sprintf(pW->path, "d:\\bio4\\room\\st%1x\\r%03x\\r%03x.blk", pG->stage_no, pG->room_id, pG->room_id);
    sprintf(pW->pathX, "x:\\soft\\room\\st%1x\\r%03x\\r%03x.blk", pG->stage_no, pG->room_id, pG->room_id);
}

static TOOL_MENU tBlockExitMenu[2] = {
    {1, "YES", NULL},
    {1, "NO", NULL},
};

// EXIT? YES/NO; YES restores cBlock's unit table and the flags, frees the work, ends the task.
static void tBlockExit()
{
    s8 sel;
    u32 i;
    u32 total;
    void* buf;

    eprintf(pW->x += 0x28, pW->y += 0x5A, 4, 0, "EXIT?");
    pW->x += 8;
    pW->y += 0x20;
    switch (pW->sub) {
    case 0:
        pW->exitCursor = 1;
        pW->sub++;
    case 1:
        sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->exitCursor, tBlockExitMenu, sizeof(tBlockExitMenu), &Joy[0]);
        if (sel >= 0) {
            switch (sel) {
            case 0:
                pW->sub = 9;
                break;
            case 1:
                MODE_RESET();
                break;
            }
        }
        break;
    case 9:
        total = 0;
        for (i = 0; i < Block.nBlock; i++) {
            if (pW->link[i].flags & 1) {
                cBlockUnit* u = Block.getUnitPtr(i);

                total += u->pData->m_size;
                u->setBlockDelete();
            }
        }
        TaskSleep(2);
        Block.debugData = 1;
        Block.useDebugMemory(1, total);
        tBlockSaveDataCreate();
        buf = Debug_alloc(pW->fileSize, 0);
        memcpy(buf, &pW->file, pW->fileSize);
        Block.pUnit = (cBlockUnit*) Debug_alloc(pW->file.hdr.nBlock << 4, 0);
        Block.roomInit(buf);
        DC.setUseDebugMemFlag(0);
        Block.noMemCtrl = 0;
        file_unlock(pW->pathX);
        pG->Disp_flg = pW->saveDispFlag;
        pG->Stop_flg = pW->saveStopFlag;
        Debug_free(pW);
        DbgFlagOff(pG, DBG_DBG_CAM);
        SetToolLight(-1);
        TutilQuitDefault();
        TaskExit();
        break;
    }
}

static TOOL_MENU tBlockMainMenuTbl[6] = {
    {1, "BLOCK CONNECT", NULL},
    {1, "BLOCK AREA EDIT", NULL},
    {1, "BLOCK AREA INFO", NULL},
    {1, "DATA LOAD", NULL},
    {1, "DATA SAVE", NULL},
    {1, "EXIT", NULL},
};

// Main menu: BLOCK CONNECT / BLOCK AREA EDIT / BLOCK AREA INFO / DATA LOAD / DATA SAVE / EXIT.
static void tBlockMainMenu()
{
    s8 sel;

    sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->cursor, tBlockMainMenuTbl, sizeof(tBlockMainMenuTbl), &Joy[0]);
    if (sel >= 0) {
        switch (sel) {
        case 0:
            pW->mode = 1;
            break;
        case 1:
            pW->mode = 2;
            break;
        case 2:
            pW->mode = 3;
            break;
        case 3:
            pW->mode = 4;
            break;
        case 4:
            pW->mode = 5;
            break;
        case 5:
            pW->mode = 6;
            break;
        }
        pW->listTop = 0;
        SUB_RESET();
    }
}

// BLOCK CONNECT_BLOCK: up/down (L/R) pick the block; sub 0 the list, 1 the link slot menu.
static void tBlockConnect()
{
    void (*routine[2])() = {tBlockConnect_ListDisp, tBlockConnect_Menu};

    if (Joy[0].rep2 & (JOY_DOWN | JOY_UP | JOY_R | JOY_L)) {
        if (pW->sub == 0) {
            if (Joy[0].rep2 & JOY_DOWN) pW->blockNo++;
            if (Joy[0].rep2 & JOY_UP) pW->blockNo--;
        }
        if (Joy[0].rep2 & JOY_R) pW->blockNo++;
        if (Joy[0].rep2 & JOY_L) pW->blockNo--;
        LIMIT(blockNo, 0, BLOCK_NUM - 1);
        STEP_RESET();
    }
    pW->x += 8;
    eprintf(pW->x, pW->y, 0, 0, "BLOCK CONNECT_BLOCK");
    pW->y += 0x10;
    routine[pW->sub]();
}

// Block list (scrolling window): A opens the link menu of an existing block (its unit blinks),
// B back to the main menu.
static void tBlockConnect_ListDisp()
{
    TBlockWork* w = pW;
    BlockLink* l = &w->link[w->blockNo];
    int end;
    int i;

    if (Joy[0].trg & JOY_A) {
        cBlockUnit* u = Block.getUnitPtr((u8) w->blockNo);

        if (!(l->flags & 1)) {
            if (u->state != BLOCK_CREATE) {
                if (Block.getBlockWork((u8) pW->blockNo) == 1) {
                    u->setBlockLoadToMram();
                    l->flags |= 1;
                }
            }
        } else {
            int created = u->state == BLOCK_CREATE;

            if (created == 1) {
                pW->sub = 1;
                STEP_RESET();
            }
        }
    }
    if (Joy[0].trg & JOY_B) {
        pW->connectCursor = 0;
        MODE_RESET();
    }
    if (pW->listTop < pW->blockNo - 4) {
        pW->listTop = pW->blockNo - 4;
    }
    if (pW->listTop > pW->blockNo) {
        pW->listTop = pW->blockNo;
    }
    // else arm re-reads the expression (t_sce_item ListDisp idiom: cse1 stops at the join label)
    if (pW->listTop + 5 > BLOCK_NUM) {
        end = BLOCK_NUM;
    } else {
        end = pW->listTop + 5;
    }
    eprintf(pW->x - 8, pW->y + (pW->blockNo - pW->listTop) * 16, 0, 0, ">");
    for (i = pW->listTop; i < end; i++) {
        dispBlockList1(pW->x, pW->y, i);
        pW->y += 0x10;
    }
}

// One block row: number and its link list (-- for none).
void dispBlockList1(int x, int y, int no)
{
    BlockLink* l = &pW->link[no];

    if (!(l->flags & 1)) {
        eprintf(x, y, 7, 0, " %d", no);
        eprintf(x + 0x30, y, 7, 0, "no block...");
    } else {
        cBlockUnit* u = Block.getUnitPtr((u8) no);

        if (u->state == BLOCK_CREATE) {
            int col = 0;
            u32 j;

            if (no == pW->blockNo) col = 6;
            eprintf(x, y, (u8) col, 0, "[%d]", no);
            for (j = 0; j < 8; j++) {
                if (l->link[j] == -1) {
                    eprintf(x + 0x30 + j * 0x18, y, (u8) col, 0, "--");
                } else {
                    eprintf(x + 0x30 + j * 0x18, y, (u8) col, 0, "%d", l->link[j]);
                }
            }
        } else {
            eprintf(x, y, 2, 0, "[%d]", no);
        }
    }
}

static TOOL_MENU tBlockConnectMenu[9] = {
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "CONNECT BLOCK", NULL},
    {1, "BLOCK DELETE", tBlockConnect_Delete},
};

// Link slot menu (8 CONNECT BLOCK rows): left/right set the linked block number of the row (-1 =
// none); B back to the list.
static void tBlockConnect_Menu()
{
    s16 y = pW->y;
    s16 x = pW->x;
    BlockLink* l = &pW->link[pW->blockNo];
    int j;

    dispBlockList1(x, y, pW->blockNo);
    x += 8;
    y += 0x20;
    ToolMenuDisp_cur(x, y, 0, &pW->connectCursor, tBlockConnectMenu, sizeof(tBlockConnectMenu), &Joy[0]);
    if (Joy[0].trg & JOY_B) {
        pW->connectCursor = 0;
        SUB_RESET();
    }
    if (pW->connectCursor <= 7) {
        if (pW->connectCursor >= 0) {
            int n = l->link[pW->connectCursor];
            int m;

            if (Joy[0].rep2 & 0x20002) n++;
            if (Joy[0].rep2 & 0x10001) n--;
            if (n >= -1) {
                m = n;
                if (m > BLOCK_NUM - 1) m = BLOCK_NUM - 1;
            } else {
                m = -1;
            }
            l->link[pW->connectCursor] = m;
        }
    }
    x += 0x78;
    for (j = 0; j < 8; j++) {
        int col = 0;

        if (pW->connectCursor == j) col = 6;
        if (l->link[j] == -1) {
            eprintf(x, y, col, 0, "--");
        } else {
            eprintf(x, y, col, 0, "%d", l->link[j]);
        }
        y += 0x10;
    }
    pW->y = y;
}

// Clears the current block's link list.
static void tBlockConnect_Delete()
{
    BlockLink* l = &pW->link[pW->blockNo];
    cBlockUnit* u;

    l->flags &= ~1;
    u = Block.getUnitPtr((u8) pW->blockNo);
    if (u->state == BLOCK_CREATE) {
        u->setBlockDelete();
    }
    pW->connectCursor = 0;
    SUB_RESET();
}

// AREA LINK_BLOCK: L/R pick the area slot; sub 0 the list, 1 the area menu, 2 area move.
static void tBlockArea()
{
    void (*routine[3])() = {tBlockArea_ListDisp, tBlockArea_Menu, tBlockArea_Move};

    if (Joy[0].rep2 & (JOY_DOWN | JOY_UP | JOY_R | JOY_L)) {
        if (pW->sub == 0) {
            if (Joy[0].rep2 & JOY_DOWN) pW->areaNo++;
            if (Joy[0].rep2 & JOY_UP) pW->areaNo--;
        }
        if (Joy[0].rep2 & JOY_R) pW->areaNo++;
        if (Joy[0].rep2 & JOY_L) pW->areaNo--;
        LIMIT(areaNo, 0, AREA_NUM - 1);
        STEP_RESET();
    }
    pW->x += 8;
    eprintf(pW->x, pW->y, 0, 0, "     AREA  PRIORITY");
    pW->y += 0x10;
    routine[pW->sub]();
}

// Area list: A opens the slot's menu, B back.
static void tBlockArea_ListDisp()
{
    int end;
    int i;

    if (Joy[0].trg & JOY_A) {
        pW->sub = 1;
        STEP_RESET();
    }
    if (Joy[0].trg & JOY_B) {
        pW->areaCursor = 0;
        MODE_RESET();
    }
    if (pW->listTop < pW->areaNo - 4) {
        pW->listTop = pW->areaNo - 4;
    }
    if (pW->listTop > pW->areaNo) {
        pW->listTop = pW->areaNo;
    }
    // else arm re-reads the expression (t_sce_item ListDisp idiom: cse1 stops at the join label)
    if (pW->listTop + 5 > AREA_NUM) {
        end = AREA_NUM;
    } else {
        end = pW->listTop + 5;
    }
    eprintf(pW->x - 8, pW->y + (pW->areaNo - pW->listTop) * 16, 0, 0, ">");
    for (i = pW->listTop; i < end; i++) {
        dispAreaList1(pW->x, pW->y, i);
        pW->y += 0x10;
    }
}

// One area row: slot, area number and priority (or "no data").
void dispAreaList1(int x, int y, int no)
{
    TBlockArea* a = &pW->area[no];

    if (a->flags & 1) {
        eprintf(x, y, no == pW->areaNo ? 6 : 0, 0, "[\x04]", no);
        eprintf(x, y, no == pW->areaNo ? 6 : 0, 0, "     %d", a->areaNo);
        eprintf(x, y, no == pW->areaNo ? 6 : 0, 0, "           %d", a->pri);
    } else {
        eprintf(x, y, 7, 0, "[-]");
        eprintf(x, y, 7, 0, "     no data...");
    }
}

static TOOL_MENU tBlockCreateMenu[1] = {
    {1, "AREA CREATE", tBlockArea_Create},
};
static TOOL_MENU tBlockEditMenu[4] = {
    {1, "AREA MOVE", NULL},
    {1, "AREA NO", NULL},
    {1, "PRIORITY", NULL},
    {1, "AREA DELEAT", tBlockArea_Delete},
};

// Area slot menu: empty -> AREA CREATE; used -> AREA MOVE, AREA NO (left/right), PRIORITY
// (left/right), AREA DELEAT; B back.
static void tBlockArea_Menu()
{
    s16 x = pW->x;
    s16 y = pW->y;
    TBlockArea* a = &pW->area[pW->areaNo];
    s8 sel;

    dispAreaList1(x, y, pW->areaNo);
    x += 8;
    y += 0x20;
    if (!(a->flags & 1)) {
        ToolMenuDisp_cur(x, y, 0, &pW->areaCursor, tBlockCreateMenu, sizeof(tBlockCreateMenu), &Joy[0]);
    } else {
        sel = ToolMenuDisp_cur(x, y, 0, &pW->areaCursor, tBlockEditMenu, sizeof(tBlockEditMenu), &Joy[0]);
        if (sel == 0) {
            pW->sub = 2;
            pW->step = sel;
            pW->step2 = sel;
        }
        switch (pW->areaCursor) {
        case 0:
            break;
        case 1: {
            int n = a->areaNo;
            int m;

            if (Joy[0].rep2 & 0x20002) n++;
            if (Joy[0].rep2 & 0x10001) n--;
            if (n >= 0) {
                m = n;
                if (m > AREA_NUM - 1) m = AREA_NUM - 1;
            } else {
                m = 0;
            }
            a->areaNo = m;
            break;
        }
        case 2: {
            int n = a->pri;
            int m;

            if (Joy[0].rep2 & 0x20002) n++;
            if (Joy[0].rep2 & 0x10001) n--;
            if (n >= 0) {
                m = n;
                if (m > 7) m = 7;
            } else {
                m = 0;
            }
            a->pri = m;
            break;
        }
        }
        x += 0x80;
        y += 0x10;
        eprintf(x, y, 0, 0, "%d", a->areaNo);
        y += 0x10;
        eprintf(x, y, 0, 0, "%d", a->pri);
        eprintf(x, y, 6, 0, "      [0:low - 7:high]");
        y += 0x10;
    }
    if (Joy[0].trg & JOY_B) {
        pW->areaCursor = 0;
        SUB_RESET();
    }
    pW->y = y;
}

// Creates the slot's area at the player (first time) with the next free area number.
static void tBlockArea_Create()
{
    TBlockArea* a = &pW->area[pW->areaNo];

    if (!(a->flags & 2)) {
        AreaDataInit(&a->area, &pPL->pos, AREA_TYPE_XZ4, 10000.0f, 5000.0f);
    }
    a->flags |= 3;
    a->slot = pW->areaNo;
    a->areaNo = 0;
    a->pri = 0;
    pW->areaCursor = 0;
}

// Clears the slot.
static void tBlockArea_Delete()
{
    TBlockArea* a = &pW->area[pW->areaNo];

    a->flags &= ~1;
    pW->areaCursor = 0;
}

// AREA MOVE: the shared AreaDataEdit editor on the slot's area; B back.
static void tBlockArea_Move()
{
    TBlockArea* a = &pW->area[pW->areaNo];

    if (a->flags & 1) {
        AreaDataEdit(&a->area, 0x00FF8080, 0, NULL, 5.0f);
        AreaDataInfoDisp(&a->area, pW->x, pW->y);
        AreaDataHelpDisp(&a->area, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));
    }
    if (Joy[0].trg & JOY_B) {
        pW->sub = 1;
        STEP_RESET();
    }
}

// BLOCK AREA INFO: L/R pick the area number (connectNo); the current lists from
// cBlock::checkBlockConnect shown; sub 0 the list, 1 the menu.
static void tBlockAreaInfo()
{
    void (*routine[2])() = {tBlockAreaInfo_ListDisp, tBlockAreaInfo_Menu};

    if (pW->nArea != 0) {
        if (Joy[0].rep2 & (JOY_DOWN | JOY_UP | JOY_R | JOY_L)) {
            TBlockWork* w;
            int v;

            if (pW->sub == 0) {
                if (Joy[0].rep2 & JOY_DOWN) pW->connectNo++;
                if (Joy[0].rep2 & JOY_UP) pW->connectNo--;
            }
            if (Joy[0].rep2 & JOY_R) pW->connectNo++;
            if (Joy[0].rep2 & JOY_L) pW->connectNo--;
            w = pW;
            v = w->connectNo;
            if (v >= 0) {
                if (v > w->nArea - 1) v = w->nArea - 1;
            } else {
                v = 0;
            }
            w->connectNo = v;
            STEP_RESET();
        }
    }
    pW->x += 8;
    eprintf(pW->x, pW->y, 0, 0, "AREA  LINK_BLOCK");
    pW->y += 0x10;
    routine[pW->sub]();
}

// Area info list: A opens the connect menu, B back.
static void tBlockAreaInfo_ListDisp()
{
    int n;
    int end;
    int i;

    if (Joy[0].trg & JOY_A) {
        if (FlagChkVar(pW->areaBits, (u32) pW->connectNo)) {
            pW->sub = 1;
            STEP_RESET();
        }
    }
    if (Joy[0].trg & JOY_B) {
        pW->infoCursor = 0;
        pW->infoMenuCursor = 0;
        MODE_RESET();
    }
    if (pW->listTop < pW->connectNo - 4) {
        pW->listTop = pW->connectNo - 4;
    }
    if (pW->listTop > pW->connectNo) {
        pW->listTop = pW->connectNo;
    }
    n = pW->listTop + 5;
    if (n > pW->nArea) {
        end = pW->nArea;
    } else {
        end = n;
    }
    eprintf(pW->x - 8, pW->y + (pW->connectNo - pW->listTop) * 16, 0, 0, ">");
    for (i = pW->listTop; i < end; i++) {
        dispAreaInfoList1(pW->x, pW->y, i);
        pW->y += 0x10;
    }
}

// One connect row: area number, its LINK / MRAM / ARAM block lists.
void dispAreaInfoList1(int x, int y, int no)
{
    BlockConnect* c = &pW->connect[no];
    int col;

    if (FlagChkVar(pW->areaBits, (u32) no) == 0) {
        col = 7;
    } else if (no == pW->connectNo) {
        col = 6;
    } else {
        col = 0;
    }
    eprintf(x, y, (u8) col, 0, "[%d]", no);
    eprintf(x + 0x30, y, (u8) col, 0, "%d", c->blockNo);
}

static TOOL_MENU tBlockAreaInfoMenu[3] = {
    {1, "LINK BLOCK", NULL},
    {1, "LOAD BLOCK", NULL},
    {1, "UNLOAD BLOCK", NULL},
};

// Connect menu of the area: LINK BLOCK (the block the area belongs to), LOAD BLOCK (MRAM list
// slots), UNLOAD BLOCK (ARAM list slots); left/right set the block numbers (-1 = none); B back.
static void tBlockAreaInfo_Menu()
{
    s16 y = pW->y;
    s16 x = pW->x;
    BlockConnect* c = &pW->connect[pW->connectNo];
    int j;

    dispAreaInfoList1(x, y, pW->connectNo);
    x += 8;
    y += 0x20;
    ToolMenuDisp_cur(x, y, 0, &pW->infoMenuCursor, tBlockAreaInfoMenu, sizeof(tBlockAreaInfoMenu), &Joy[0]);
    if (Joy[0].trg & JOY_B) {
        pW->infoCursor = 0;
        pW->infoMenuCursor = 0;
        SUB_RESET();
    }
    if (FlagChkVar(pW->areaBits, (u32) pW->connectNo)) {
        if (!(Joy[0].on & JOY_A)) {
            TBlockWork* w;
            u8 v;

            if (Joy[0].rep2 & JOY_RIGHT) pW->infoCursor++;
            if (Joy[0].rep2 & JOY_LEFT) pW->infoCursor--;
            w = pW;
            v = w->infoCursor;
            if ((s8) v >= 0) {
                if ((s8) v > 7) v = 7;
            } else {
                v = 0;
            }
            w->infoCursor = v;
        }
        switch (pW->infoMenuCursor) {
        case 0: {
            register int n asm("r11");  // COMPILER-DIFF: candidate #17 (global-alloc order of n vs the rep2 load)

            n = c->blockNo;
            if (Joy[0].rep2 & 0x20002) n++;
            if (Joy[0].rep2 & 0x10001) n--;
            if (n >= 0) {
                if (n > BLOCK_NUM - 1) n = BLOCK_NUM - 1;
            } else {
                n = 0;
            }
            c->blockNo = n;
            break;
        }
        case 1:
            if (Joy[0].on & JOY_A) {
                int n = c->mram[pW->infoCursor];
                int m;

                if (Joy[0].rep2 & 0x20002) n++;
                if (Joy[0].rep2 & 0x10001) n--;
                if (n >= -1) {
                    m = n;
                    if (m > BLOCK_NUM - 1) m = BLOCK_NUM - 1;
                } else {
                    m = -1;
                }
                c->mram[pW->infoCursor] = m;
            }
            break;
        case 2:
            if (Joy[0].on & JOY_A) {
                int n = c->aram[pW->infoCursor];
                int m;

                if (Joy[0].rep2 & 0x20002) n++;
                if (Joy[0].rep2 & 0x10001) n--;
                if (n >= -1) {
                    m = n;
                    if (m > BLOCK_NUM - 1) m = BLOCK_NUM - 1;
                } else {
                    m = -1;
                }
                c->aram[pW->infoCursor] = m;
            }
            break;
        }
        x += 0x78;
        eprintf(x, y, 0, 0, "%d", c->blockNo);
        y += 0x10;
        for (j = 0; j < 8; j++) {
            int col;

            if (pW->infoMenuCursor == 1 && pW->infoCursor == j) {
                col = (Joy[0].on & JOY_A) ? 4 : 6;
            } else {
                col = 0;
            }
            if (c->mram[j] == -1) {
                eprintf(x + j * 0x18, y, col, 0, "--");
            } else {
                eprintf(x + j * 0x18, y, col, 0, "%d", c->mram[j]);
            }
        }
        y += 0x10;
        for (j = 0; j < 8; j++) {
            int col;

            if (pW->infoMenuCursor == 2 && pW->infoCursor == j) {
                col = (Joy[0].on & JOY_A) ? 4 : 6;
            } else {
                col = 0;
            }
            if (c->aram[j] == -1) {
                eprintf(x + j * 0x18, y, col, 0, "--");
            } else {
                eprintf(x + j * 0x18, y, col, 0, "%d", c->aram[j]);
            }
        }
        y += 0x10;
        pW->y = y;
    }
}

// Draws every block's box (the current one blinking) with its links, and every area with its
// number in the priority colour; hides / shows the block models by the current selection.
void tBlockArea_disp()
{
    int i;
    int j;
    u32 col;

    for (i = 0; i < BLOCK_NUM; i++) {
        BlockLink* l = &pW->link[i];
        cBlockUnit* u = Block.getUnitPtr((u8) i);

        if ((l->flags & 1) && u->state == BLOCK_CREATE && pW->mode == 1) {
            if (pW->blockNo == i) {
                col = 0x00FF8080;
            } else {
                for (int j = 0; j < 8; j++) {
                    col = 0x00808080;
                    if (l->link[j] == pW->blockNo) {
                        col = 0x0080FF80;
                        break;
                    }
                    if (pW->link[pW->blockNo].link[j] == i) {
                        col = 0x0080FF80;
                        break;
                    }
                }
            }
            tBlockArea_dispBlockBox((u8) i, col);
        }
    }
    for (i = 0; i < BLOCK_NUM; i++) {
        BlockLink* l = &pW->link[i];

        for (j = 0; j < 8; j++) {
            if (l->link[j] == -1) continue;
            if ((pW->link[l->link[j]].flags & 1) == 0) continue;
            if (i == pW->blockNo || l->link[j] == pW->blockNo) {
                col = 0x00FF8080;
            } else {
                col = 0x00808080;
            }
            Draw_line3d(&pW->pos[i], &pW->pos[l->link[j]], col | 0xFF000000, 0);
        }
    }
    pW->nArea = 0;
    memclr_asm(pW->areaBits, sizeof(pW->areaBits));
    for (i = 0; i < AREA_NUM; i++) {
        TBlockArea* a = &pW->area[i];

        if (a->flags & 1) {
            if (pW->nArea <= a->areaNo) {
                pW->nArea = a->areaNo + 1;
            }
            FlagOnVar(pW->areaBits, (u32) a->areaNo);
            switch (pW->mode) {
            case 2:
                col = 0x00808080;
                if (pW->areaNo == i) col = 0x00FF8080;
                tBlockArea_dispBlockArea((u8) i, col);
                break;
            case 3:
                col = 0x00808080;
                if (pW->connectNo == a->areaNo) {
                    col = 0x00FF8080;
                    tBlockArea_dispBlockArea((u8) i, col);
                }
                break;
            case 1:
                col = 0x00808080;
                break;
            }
            // the original's loop had 5 more real insns at loop.c time (72 in pass 1 vs the 71 * savings *
            // lifetime limit), so FlagOnVar's 0x80000000 mask is hoisted in loop pass 2, after the pass-1 giv
            // init `li 1504`; dead sets of a used variable are deleted by flow and counted by loop.c
            col = 7; // COMPILER-DIFF: 3 (loop.c pass-1 insn_count, dead sets)
            col = 6;
            col = 7;
        }
    }
    for (i = 0; i < CONNECT_NUM; i++) {
        if (FlagChkVar(pW->areaBits, (u32) i)) {
            pW->connect[i].flags |= 1;
        } else {
            pW->connect[i].flags &= ~1;
        }
    }
    if (pW->mode == 3) {
        Block.checkBlockConnect(&pW->connect[pW->connectNo], pW->link, &pW->mram, &pW->aram);
        tBlockArea_dispBlockModel(1);
    } else {
        tBlockArea_dispBlockModel(0);
    }
    eprintf(0x1AE, 0x24, 0, 0, "[PLAYER]");
    eprintf(0x1AE, 0x34, 0, 0, "X:%.0f", pPL->pos.x);
    eprintf(0x1AE, 0x44, 0, 0, "Y:%.0f", pPL->pos.y);
    eprintf(0x1AE, 0x54, 0, 0, "Z:%.0f", pPL->pos.z);
    eprintf(0x1AE, 0x64, 0, 0, "ANG:%f", pPL->ang.y);
}


// Shows only the scroll models of the current block (on) or every block's models (off) by toggling
// their be_flag bit 1.
void tBlockArea_dispBlockModel(int on)
{
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObj* obj = ObjMgr.fastAt(i);
        int blk;

        if (!obj->isAlive()) continue;
        blk = obj->blk;
        if (blk < 0) continue;
        if (obj->id != 2) continue;
        if (on == 1) {
            if (blk == pW->connect[pW->connectNo].blockNo) {
                obj->be_flag |= 2;
            } else if (FlagChkVar(&pW->mram, (u32) blk)) {
                if (pW->blink & 4) {
                    obj->be_flag |= 2;
                } else {
                    obj->be_flag &= ~2;
                }
            } else if (FlagChkVar(&pW->aram, (u32) blk)) {
                if (pW->blink & 0x10) {
                    obj->be_flag |= 2;
                } else {
                    obj->be_flag &= ~2;
                }
            } else {
                obj->be_flag &= ~2;
            }
        } else {
            obj->be_flag |= 2;
        }
    }
}

// Draws area slot `no` in colour `col`.
void tBlockArea_dispBlockArea(u8 no, u32 col)
{
    TBlockArea* a = &pW->area[no];
    AreaData* area = &a->area;
    Vec c;

    AreaDataDisp(area, col | 0x40000000, 1, NULL);
    AreaGetCenterPos(&c, area);
    Vec cCopy = c;
    GetScreenPos(&cCopy, &c);
    eprintf2(0xA, 0x14, (int) c.x - 5, (int) c.y - 10, 0, 0, "%d", a->areaNo);
}

// Draws block `no`'s bounding box (from its cBlockUnit) and number label.
void tBlockArea_dispBlockBox(u8 no, u32 col)
{
    f32 minX = 100000000.0f;
    f32 maxX = -100000000.0f;
    f32 minY = 100000000.0f;
    f32 maxY = -100000000.0f;
    f32 minZ = 100000000.0f;
    f32 maxZ = -100000000.0f;
    int found = 0;
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObj* obj = ObjMgr.fastAt(i);
        cModelInfo* info;
        Mtx m;
        Mtx r;
        Vec box[8];
        Vec c;

        if (!obj->isAlive()) continue;
        if (obj->blk != no) continue;
        if (obj->id != 2) continue;
        info = obj->pModelInfo;
        PSMTXScale(m, obj->scale.x, obj->scale.y, obj->scale.z);
        RotMatrix(r, &obj->ang);
        PSMTXConcat(m, r, m);
        for (; info; info = info->pList) {
            f32 hx = info->bound.size.x;
            f32 hy = info->bound.size.y;
            f32 hz = info->bound.size.z;
            int j;

            box[0].x = -hx;
            box[0].y = -hy;
            box[0].z = -hz;
            box[1].x = hx;
            box[1].y = -hy;
            box[1].z = -hz;
            box[2].x = -hx;
            box[2].y = -hy;
            box[2].z = hz;
            box[3].x = hx;
            box[3].y = -hy;
            box[3].z = hz;
            box[4].x = -hx;
            box[4].y = hy;
            box[4].z = -hz;
            box[5].x = hx;
            box[5].y = hy;
            box[5].z = -hz;
            box[6].x = -hx;
            box[6].y = hy;
            box[6].z = hz;
            box[7].x = hx;
            box[7].y = hy;
            box[7].z = hz;
            PSMTXMultVecArray(m, box, box, 8);
            PSMTXMultVec(m, &info->bound.center, &c);
            for (j = 0; j < 8; j++) {
                box[j].x += obj->pos.x + c.x;
                box[j].y += obj->pos.y + c.y;
                box[j].z += obj->pos.z + c.z;
            }
            for (j = 0; j < 8; j++) {
                if (minX > box[j].x) minX = box[j].x;
                if (maxX < box[j].x) maxX = box[j].x;
                if (minY > box[j].y) minY = box[j].y;
                if (maxY < box[j].y) maxY = box[j].y;
                if (minZ > box[j].z) minZ = box[j].z;
                if (maxZ < box[j].z) maxZ = box[j].z;
            }
            found = 1;
        }
    }
    if (found) {
        Vec center;
        Vec box[8];
        Vec scr;
        int c;

        box[0].x = minX;
        box[0].y = minY;
        box[0].z = minZ;
        box[1].x = maxX;
        box[1].y = minY;
        box[1].z = minZ;
        box[2].x = minX;
        box[2].y = minY;
        box[2].z = maxZ;
        box[3].x = maxX;
        box[3].y = minY;
        box[3].z = maxZ;
        box[4].x = minX;
        box[4].y = maxY;
        box[4].z = minZ;
        box[5].x = maxX;
        box[5].y = maxY;
        box[5].z = minZ;
        box[6].x = minX;
        box[6].y = maxY;
        box[6].z = maxZ;
        box[7].x = maxX;
        box[7].y = maxY;
        box[7].z = maxZ;
        Draw_box(box, col | 0x20000000, 1);
        center.x = (minX + maxX) * 0.5f;
        center.y = (minY + maxY) * 0.5f;
        center.z = (minZ + maxZ) * 0.5f;
        Draw_sphere(&center, 1000.0f, 0x80808080, 0, 0);
        pW->pos[no] = center;
        Vec centerCopy = center;
        GetScreenPos(&centerCopy, &scr);
        c = 7;
        if (col == 0x00FF8080) c = 6;
        eprintf2(8, 0x10, (int) scr.x - 5, (int) scr.y - 10, c, 0, "B%d", no);
    }
}

static TOOL_MENU tBlockLoadMenu[3] = {
    {1, "SERVER", NULL},
    {0, "LOCAL", NULL},
    {1, "don't load", NULL},
};

// DATA LOAD: SERVER / LOCAL / don't load; reads the .blk image and expands the link / area /
// connect tables into the work; the tool starts here.
static void tBlockDataLoad()
{
    s8 sel;
    u32 i;
    int j;
    u8* data;

    eprintf(pW->x, pW->y, 4, 0, "[DATA LOAD]");
    pW->y += 0x10;
    switch (pW->sub) {
    case 0:
        sel = ToolMenuDisp(pW->x, pW->y, 0, tBlockLoadMenu, sizeof(tBlockLoadMenu), &Joy[0]);
        eprintf(pW->x + 0x5A, pW->y, 6, 0, "%s", pW->pathX);
        eprintf(pW->x + 0x5A, pW->y + 0x10, 6, 0, "%s", pW->path);
        if (sel >= 0) {
            int ret = 0;

            switch (sel) {
            case 0:
                ret = HDRead(pW->pathX, &pW->file);
                break;
            case 1:
                ret = HDRead(pW->path, &pW->file);
                break;
            case 2:
                pW->mode = ret;
                pW->sub = ret;
                pW->step = ret;
                pW->step2 = ret;
                return;
            }
            pW->timer = 30;
            if (ret != 0) {
                pW->sub = 1;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->sub = 9;
                pW->step = ret;
                pW->step2 = ret;
            }
        }
        if (Joy[0].trg & JOY_B) {
            MODE_RESET();
        }
        break;
    case 1:
        pW->pFile = &pW->file.hdr;
        if (strcmp((char*) pW->pFile, "BLK") != 0) {
            pW->sub = 9;
            pW->step = 0;
            pW->step2 = 0;
        } else if (pW->pFile->version != 0x100) {
            pW->sub = 9;
            pW->step = 0;
            pW->step2 = 0;
        } else {
            memclr_asm(pW->link, sizeof(pW->link));
            memclr_asm(pW->area, sizeof(pW->area));
            memclr_asm(pW->connect, sizeof(pW->connect));
            for (i = 0; i < BLOCK_NUM; i++) {
                for (j = 0; j < 8; j++) {
                    pW->link[i].link[j] = -1;
                }
            }
            for (i = 0; i < CONNECT_NUM; i++) {
                for (j = 0; j < 8; j++) {
                    pW->connect[i].mram[j] = -1;
                    pW->connect[i].aram[j] = -1;
                }
            }
            data = (u8*) pW->pFile;
            pW->pLink = (BlockLink*) (data + pW->pFile->ofsLink);
            pW->pArea = (TBlockArea*) (data + pW->pFile->ofsArea);
            pW->pConnect = (BlockConnect*) (data + pW->pFile->ofsConnect);
            for (i = 0; i < pW->pFile->nBlock; i++) {
                pW->link[i] = pW->pLink[i];
                if (pW->link[i].flags & 1) {
                    u8 no = i;

                    Block.getBlockWork(no);
                    Block.getUnitPtr(no)->setBlockLoadToMram();
                }
            }
            for (i = 0; i < pW->pFile->nArea; i++) {
                TBlockArea* src = (TBlockArea*) (i * sizeof(TBlockArea) + (u32) pW->pArea);

                *(TBlockArea*) (src->slot * sizeof(TBlockArea) + (u32) pW->area) = *src;
            }
            for (i = 0; i < pW->pFile->nConnect; i++) {
                pW->connect[i] = pW->pConnect[i];
            }
            pW->sub = 8;
            pW->step = 0;
            pW->step2 = 0;
        }
        break;
    case 8:
        eprintf(pW->x, pW->y, 6, 0, "DATA LOAD COMPLETE.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            MODE_RESET();
        }
        pW->timer--;
        break;
    case 9:
        eprintf(pW->x, pW->y, 2, 0, "DATA LOAD ERROR.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            SUB_RESET();
        }
        pW->timer--;
        break;
    }
}

static TOOL_MENU tBlockSaveMenu[3] = {
    {1, "SERVER", NULL},
    {0, "LOCAL", NULL},
    {1, "don't save", NULL},
};

// DATA SAVE: SERVER / LOCAL / don't save; builds the image (tBlockSaveDataCreate), writes it and
// installs it as the room's live block data.
static void tBlockDataSave()
{
    int ret = 0;

    eprintf(pW->x, pW->y, 4, 0, "[DATA SAVE]");
    pW->y += 0x10;
    switch (pW->sub) {
    case 0:
        pW->sub = 1;
        pW->step = ret;
        pW->step2 = ret;
    case 1:
        pW->saveSel = ToolMenuDisp(pW->x, pW->y, 0, tBlockSaveMenu, sizeof(tBlockSaveMenu), &Joy[0]);
        eprintf(pW->x + 0x64, pW->y, 6, 0, "%s", pW->pathX);
        eprintf(pW->x + 0x64, pW->y + 0x10, 6, 0, "%s", pW->path);
        if (pW->saveSel >= 0) {
            if (pW->saveSel == 2) {
                MODE_RESET();
            } else {
                pW->sub = 2;
                pW->step = 0;
                pW->step2 = 0;
            }
        }
        if (Joy[0].trg & JOY_B) {
            MODE_RESET();
        }
        break;
    case 2:
        tBlockSaveDataCreate();
        switch (pW->saveSel) {
        case 0:
            ret = HDWrite(pW->pathX, &pW->file, pW->fileSize);
            break;
        case 1:
            ret = HDWrite(pW->path, &pW->file, pW->fileSize);
            break;
        }
        pW->timer = 30;
        if (ret != 0) {
            pW->sub = 8;
            pW->step = 0;
            pW->step2 = 0;
        } else {
            pW->sub = 9;
            pW->step = ret;
            pW->step2 = ret;
        }
        break;
    case 8:
        eprintf(pW->x, pW->y, 6, 0, "DATA SAVE COMPLETE.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->mode = ret;
            pW->sub = ret;
            pW->step = ret;
            pW->step2 = ret;
        }
        pW->timer--;
        break;
    case 9:
        eprintf(pW->x, pW->y, 2, 0, "DATA SAVE ERROR.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->sub = 1;
            pW->step = ret;
            pW->step2 = ret;
        }
        pW->timer--;
        break;
    }
}

// Builds the .blk image: header counts (blocks used, live areas, area numbers) and offsets, then
// the link table, the areas and the connect table packed one after another.
void tBlockSaveDataCreate()
{
    int nBlock = 0;
    int nArea = 0;
    int nConnect = 0;
    int i = BLOCK_NUM - 1;
    u32 connectSize;

    pW->pLink = pW->file.link;
    if (pW->link[BLOCK_NUM - 1].flags & 1) {
        nBlock = BLOCK_NUM;
    } else {
    again:
        i--;
        if (i >= 0) {
            if ((pW->link[i].flags & 1) == 0) goto again;
            nBlock = i + 1;
        }
    }
    memcpy(pW->pLink, pW->link, nBlock * sizeof(BlockLink));
    pW->pLink = (BlockLink*) ((u8*) pW->pLink + nBlock * sizeof(BlockLink));
    pW->pArea = (TBlockArea*) pW->pLink;
    for (i = 0; i < AREA_NUM; i++) {
        if (pW->area[i].flags & 1) {
            *pW->pArea = pW->area[i];
            nArea++;
            pW->pArea++;
            if (nConnect <= pW->area[i].areaNo) {
                nConnect = pW->area[i].areaNo + 1;
            }
        }
    }
    pW->pConnect = (BlockConnect*) pW->pArea;
    connectSize = nConnect * sizeof(BlockConnect);
    memcpy(pW->pConnect, pW->connect, connectSize);
    u32 linkSize = nBlock * sizeof(BlockLink);
    pW->file.hdr.tag[0] = 'B';
    pW->file.hdr.tag[1] = 'L';
    pW->file.hdr.tag[2] = 'K';
    pW->file.hdr.tag[3] = 0;
    pW->file.hdr.version = 0x100;
    pW->file.hdr.nBlock = nBlock;
    pW->file.hdr.nArea = nArea;
    pW->file.hdr.nConnect = nConnect;
    pW->file.hdr.ofsLink = sizeof(TBlockHeader);
    pW->file.hdr.ofsArea = sizeof(TBlockHeader) + linkSize;
    {
        u32 areaSize = nArea * sizeof(TBlockArea) + sizeof(TBlockHeader);
        u32 ofsConnect = linkSize + areaSize;
        pW->file.hdr.ofsConnect = ofsConnect;
        pW->fileSize = ofsConnect + connectSize;
    }
}

// CAMERA MODE (START): the debug camera moves with pad 1.
void tBlock_DebugCamera()
{
    CamDbg.move(&pG->Camera, &Joy[0], 0);
    pW->timer++;
    if (pW->timer & 8) {
        eprintf2(0xE, 0x12, 0xAA, 0x18, 6, 0, "CAMERA MODE");
    }
}

asm(".section .data; .balign 8");
