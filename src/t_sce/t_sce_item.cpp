#include "types.h"
#include "light.h"
#include "atari.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "db_cam.h"
#include "main_mem.h"
#include "file.h"
#include "dbmodule.h"
#include "db_log.h"
#include "area.h"
#include "sce_at.h"
#include "player.h"
#include "math_sub.h"
#include "obj.h"
#include "em.h"
#include "etc_model.h"
#include "debug.h"
#include "t_util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Item placement ("ITA" room file) editor of the t_sce REL (D:/Bio4/Prog/t_sce_item.cpp). The
// t_sce_at.cpp skeleton with the item payload editor, the flag auto-numbering and the XML export.

int SetToolLight(int no);  // t_sce's db_light_v2 copy

// ITA file: header + records (game/sce_at.cpp SceAtFileHead).
struct TSceItemFileHead {
    char magic[4];    // "ITA"
    u16 version;      // 0x04  0x105
    u16 num;          // 0x06
    u8 pad_8[8];
};

struct TSceItemFile {
    TSceItemFileHead head;   // 0x00
    SceAtWork work[128];     // 0x10
};

struct SceAtWorkTbl {
    SceAtWork a[128];
};

// game/sce_at.cpp's SceAtSysWork (static there; the REL imports it by name).
struct TSceItemSys {
    TSceItemFile* pAtData;   // 0x00
    u32 pAtWork;             // 0x04
    TSceItemFile* pItemData; // 0x08
    u8 pad_C[0x11D - 0xC];
    u8 m_use_tool_data_i;    // 0x11D  pItemData was allocated by the tool (PS2 SCE_AT_SYS m_use_tool_data_i)
    u8 pad_11E[2];
};
extern TSceItemSys SceAtSys;   // game/sce_at.cpp (static SceAtSysWork there, so not in sce_at.h; the REL link resolves the local symbol)

struct TSceItemWork {
    u8 mode;            // 0x00  routine index
    u8 sub;             // 0x01
    u8 step;            // 0x02
    u8 step2;           // 0x03
    u8 camMode;         // 0x04  debug camera
    s8 cursor;          // 0x05  main menu
    s8 editCursor;      // 0x06  area edit menu
    s8 inputCursor;     // 0x07  data input line
    s8 exitCursor;      // 0x08  exit menu / effect offset axis
    u8 copySrc;         // 0x09
    u8 copyValid;       // 0x0A
    s8 saveSel;         // 0x0B
    u8 pad_C[3];
    u8 light;           // 0x0F
    u8 pad_10[0x18];
    s16 timer;          // 0x28
    s16 areaNo;         // 0x2A
    s16 x;              // 0x2C
    s16 y;              // 0x2E
    s16 x0;             // 0x30
    s16 y0;             // 0x32
    s16 listTop;        // 0x34
    u8 pad_36[0xA];
    int saveNum;        // 0x40
    u8 pad_44[4];
    int loaded;         // 0x48  a file was loaded: DATA SAVE enabled
    char path[0x40];    // 0x4C  d:\ path
    char pathX[0x40];   // 0x8C  x:\ path
    char xmlPath[0x40]; // 0xCC  x:\ xml export path
    AreaData editArea;  // 0x10C  scratch area of the eye trigger editor
    TSceItemFileHead head;   // 0x13C
    SceAtWork area[128];     // 0x14C
    TSceItemFile file;       // 0x4F4C  load / save image
    SceAtWork areaLoad[128]; // 0x9D5C  the table as loaded / saved
    SceAtWork copyBuf;       // 0xEB5C
    u32 flgAuto[4];          // 0xEBF8  ITEM_SET flags handed out automatically
    char idName[512][64];    // 0xEC08  ITEM_ID_ names
    char idName2[512][64];   // 0x16C08  ITEM_ID_ names from ITEM_ID_SYSTEM_TOP on
    u8 pad_1EC08[4];
};

static char xmlTab[0x20];
static char xmlBuf[0x4000];
static char* xmlP;

static TSceItemWork* sceItemWk;
#define pW (sceItemWk)
struct SceAtWorkPtr {
    SceAtWork* p;
};
static SceAtWork* sceItemCur;
#define pCur (sceItemCur)

extern "C" {
void tSceItemInit_base();
void tSceItemInit();
static void set_filename();  // duplicated in t_block / t_sce_at: static here (they own the module names)
static void tSceItemExit();
static void tSceItemMainMenu();
static void tSceItemAreaEdit();
static void tSceItemAreaEdit_ListDisp();
void dispItemSetList1(s16 x, s16 y, int no);
static void tSceItemAreaEdit_EditMenu();
static void tSceItemAreaEdit_AreaCreate();
static void tSceItemAreaEdit_AreaCopy();
static void tSceItemAreaEdit_AreaPaste();
static void tSceItemAreaEdit_CopyBuffClear();
static void tSceItemAreaEdit_AreaDelete();
static void angle_arrow_disp(SceAtWork* a);
void tSceItemAreaEdit_disp();
static void tSceItemAreaEdit_AreaMove();
static void tSceItemAreaEdit_DataInput();
void tSceItemDataInput_basic_menu(int sel, TOOL_MENU* menu);
void tSceItemDataInput_item();
static void tSceItemDataInput_item_main();
static void tSceItemDataInput_item_ETedit();
static void tSceItemDataInput_item_EffPosSet();
void tSceItem_PointDisp(f32 x, f32 y, f32 z);
static void tSceItemDataLoad();
void tSceItemLoadDataCopy();
static void tSceItemDataSave();
void tSceItemSetRoomData(int size);
void tSceSaveXml();
void tSceItemSaveDataCreate();
int tSceItemSetItemFlgAuto_on();
int tSceItemSetItemFlgAuto_ck(u32 no);
void tSceItemSetItemFlgAutoDataCreate();
void tSceItemData_DebugCamera();
static void tSceItemPreview();
static void tSceItemPreview_init();
static void tSceItemPreview_main();
void tSceItemPreview_pl_pos();
static void tSceItemPreview_exit();
void loadItemIdName(const char* path, char* names, char* names2);
char* getItemIdStr(u32 id);
}

#define AREA_NUM 128

// pad masks of the +/- inputs (the sub stick bits are the header's SLEFT/SRIGHT swapped)

// +1 / -1 on a value
#define STEP(j, v)                     \
    if (Joy[0].j & REP_RIGHT) v++;     \
    if (Joy[0].j & REP_LEFT) v--;

// 0..hi clamp with a second variable (blt / li-at-end shape)
// n = v limited to [0, hi] (a statement, unlike CLAMP in math_sub_decl.h).
#define CLAMP_SET(v, n, hi)      \
    if ((v) >= 0) {          \
        n = v;               \
        if (n > (hi)) n = hi; \
    } else {                 \
        n = 0;               \
    }

// 0..hi wrap-around
#define WRAP(v, hi) ((v) < 0 ? (hi) : ((v) > (hi) ? 0 : (v)))

#define SUB_RESET() \
    pW->sub = 0;    \
    pW->step = 0;   \
    pW->step2 = 0;

#define MODE_RESET() \
    pW->mode = 0;    \
    pW->sub = 0;     \
    pW->step = 0;    \
    pW->step2 = 0;

// link types 1 (enemy) and 2 (etc model) place the item on another model
#define LINKED(a) ((a)->linkType == 1 || (a)->linkType == 2)


// ITEM SET TOOL entry (debug menu 34): edits the room's ITA item placement records (SceAtWork with
// the item payload). Loops: sub stick moves the panel, START toggles the debug camera, Z the tool
// light, then routine[mode] (main menu, area edit, preview, load, save, exit).
void ToolSceItem()
{
    void (*routine[6])() = {tSceItemMainMenu, tSceItemAreaEdit, tSceItemPreview, tSceItemDataLoad, tSceItemDataSave,
                            tSceItemExit};

    pW = (TSceItemWork*) Debug_alloc(sizeof(TSceItemWork), 1);
    tSceItemInit();
    pW->mode = 3;
    pW->sub = 0;
    pW->step = 0;
    pW->step2 = 0;
    while (1) {
        if (pW->camMode == 0 && pW->mode != 2) {
            if (Joy[0].on & JOY_SSRIGHT) pW->x0 += 8;
            if (Joy[0].on & JOY_SSLEFT) pW->x0 -= 8;
            if (Joy[0].on & JOY_SSDOWN) pW->y0 += 8;
            if (Joy[0].on & JOY_SSUP) pW->y0 -= 8;
        }
        pW->x = pW->x0;
        pW->y = pW->y0;
        eprintf(pW->x, pW->y, 4, 0, "[ITEM SET TOOL]");
        pW->y += 0x20;
        pW->x += 8;
        tSceItemAreaEdit_disp();
        if (Joy[0].trg & JOY_START) {
            if (pW->mode != 2) pW->camMode ^= 1;
        }
        if (Joy[0].trg & JOY_Z) {
            if (pW->light) {
                pW->light = 0;
                SetToolLight(-1);
            } else {
                pW->light = 1;
                SetToolLight(1);
            }
        }
        if (pW->camMode) {
            tSceItemData_DebugCamera();
        } else {
            routine[pW->mode]();
        }
        TaskSleep(1);
    }
}

// Flag setup: pause the game, debug displays on, tool light 1.
void tSceItemInit_base()
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
    pG->Disp_flg |= 0x20000000;
    pG->Disp_flg |= 0x40000000;
    pG->Disp_flg |= 0x4000000;
    pG->Disp_flg |= 0x2000000;
    pG->Disp_flg |= 0x100000;
    DbgFlagOn(pG, DBG_DBG_CAM);
    pW->light = 1;
    SetToolLight(1);
}

// Tool start: default state, header ("ITA" 0x105, 128 records), item names from the host header
// d:\bio4/prog/item_id.h (idName / idName2), file names, the live item data (SceAtSys.pItemData)
// copied into the work, camera target type.
void tSceItemInit()
{
    char buf[0x100];

    TutilInitDefault();
    tSceItemInit_base();
    pW->x0 = 0x28;
    pW->y0 = 0xA;
    pW->head.magic[0] = 'I';
    pW->head.magic[1] = 'T';
    pW->head.magic[2] = 'A';
    pW->head.magic[3] = 0;
    pW->head.version = 0x105;
    pW->head.num = AREA_NUM;
    pW->copySrc = 0;
    pW->copyValid = 0;
    pW->loaded = 0;
    set_filename();
    file_lock(pW->pathX);
    if (SceAtSys.pItemData != NULL) {
        memcpy(&pW->file, SceAtSys.pItemData, SceAtSys.pItemData->head.num * sizeof(SceAtWork) + sizeof(TSceItemFileHead));
        tSceItemLoadDataCopy();
    }
    sprintf(buf, "d:\\bio4/prog/item_id.h");
    memclr_asm(pW->idName, sizeof(pW->idName));
    memclr_asm(pW->idName, sizeof(pW->idName));
    loadItemIdName(buf, (char*) pW->idName, (char*) pW->idName2);
}

// Server (d:) and local (x:) paths of the room's r<room>.ita.
static void set_filename()
{
    sprintf(pW->path, "d:\\bio4\\room\\st%1x\\r%03x\\r%03x.ita", pG->stage_no, pG->room_id, pG->room_id);
    sprintf(pW->pathX, "x:\\soft\\room\\st%1x\\r%03x\\r%03x.ita", pG->stage_no, pG->room_id, pG->room_id);
    sprintf(pW->xmlPath, "x:\\soft\\room\\xml\\aev\\r%03xaev.xml", pG->room_id);
}

static TOOL_MENU tSceItemExitMenu[2] = {
    {1, "YES", NULL},
    {1, "NO", NULL},
};

// EXIT? YES/NO; YES restores the camera target, tool light and flags, frees the work, ends the task.
static void tSceItemExit()
{
    s8 sel;

    eprintf(pW->x += 0x28, pW->y += 0x5A, 4, 0, "EXIT?");
    pW->x += 8;
    pW->y += 0x20;
    switch (pW->sub) {
    case 0:
        pW->exitCursor = 1;
        pW->sub++;
    case 1:
        sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->exitCursor, tSceItemExitMenu, sizeof(tSceItemExitMenu), &Joy[0]);
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
        file_unlock(pW->pathX);
        Debug_free(pW);
        DbgFlagOff(pG, DBG_DBG_CAM);
        SetToolLight(-1);
        TutilQuitDefault();
        TaskExit();
        break;
    }
}

static TOOL_MENU tSceItemMainMenuTbl[5] = {
    {1, "ITEM SET", NULL},
    {1, "PREVIEW", NULL},
    {1, "DATA LOAD", NULL},
    {1, "DATA SAVE", NULL},
    {1, "EXIT", NULL},
};

// Main menu: ITEM SET (area edit) / PREVIEW / DATA LOAD / DATA SAVE / EXIT.
static void tSceItemMainMenu()
{
    s8 sel;

    if (pW->loaded == 1) {
        tSceItemMainMenuTbl[3].Be_flg = 1;
    } else {
        tSceItemMainMenuTbl[3].Be_flg = 0;
    }
    sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->cursor, tSceItemMainMenuTbl, sizeof(tSceItemMainMenuTbl), &Joy[0]);
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
        }
        SUB_RESET();
        pW->listTop = 0;
    }
}

// ITEM SET: L/R pick the record (areaNo); sub routines edit menu, area move, data input, area
// create.
static void tSceItemAreaEdit()
{
    void (*routine[5])() = {tSceItemAreaEdit_ListDisp, tSceItemAreaEdit_EditMenu, tSceItemAreaEdit_AreaMove,
                            tSceItemAreaEdit_DataInput, tSceItemAreaEdit_AreaCreate};
    s16 old = pW->areaNo;
    u16 id;

    if (Joy[0].rep2 & (JOY_R | JOY_L | JOY_UP | JOY_DOWN)) {
        if (pW->sub == 0) {
            if (Joy[0].rep2 & (JOY_DOWN | JOY_SDOWN)) pW->areaNo++;
            if (Joy[0].rep2 & (JOY_UP | JOY_SUP)) pW->areaNo--;
        }
        if (Joy[0].rep2 & JOY_R) pW->areaNo++;
        if (Joy[0].rep2 & JOY_L) pW->areaNo--;
        pW->areaNo = WRAP(pW->areaNo, AREA_NUM - 1);
    }
    if (old != pW->areaNo) {
        pW->step = 0;
        pW->step2 = 0;
    }
    if (pW->copyValid) {
        eprintf(pW->x, pW->y - 0x10, 7, 0, "->ITEM_SET[ %d ]", pW->copySrc);
    }
    pCur = &pW->area[pW->areaNo];
    eprintf(pW->x, pW->y, 4, 0, "ITEM_SET[ %d ]", pW->areaNo);
    if (pCur->flag & 1) {
        id = pCur->item.id;
        eprintf(pW->x + 0x80, pW->y, 6, 0, "ID:%02X %s", id, getItemIdStr(id));
    } else {
        eprintf(pW->x + 0x80, pW->y, 2, 0, "no data:");
    }
    pW->x += 8;
    pW->y += 0x10;
    routine[pW->sub]();
}

// Record list "NO ID" (7-row window): A opens the record menu, B back to the main menu.
static void tSceItemAreaEdit_ListDisp()
{
    int end;
    int i;

    if (Joy[0].trg & JOY_A) {
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
    }
    if (Joy[0].trg & JOY_B) {
        pW->editCursor = 0;
        MODE_RESET();
    }
    if (pW->listTop < pW->areaNo - 6) pW->listTop = pW->areaNo - 6;
    if (pW->listTop > pW->areaNo) pW->listTop = pW->areaNo;
    // the else arm re-reads the expression: jump1 cannot hoist a load into `end = n`, so cse1 follows the
    // taken branch into the else arm and stops at the join label -> the eprintf block reloads pW
    if (pW->listTop + 7 > AREA_NUM) {
        end = AREA_NUM;
    } else {
        end = pW->listTop + 7;
    }
    eprintf(pW->x, pW->y, 0, 0, "NO    ID");
    pW->y += 0x10;
    eprintf(pW->x - 10, pW->y + (pW->areaNo - pW->listTop) * 16, 0, 0, ">");
    for (i = pW->listTop; i < end; i++) {
        dispItemSetList1(pW->x, pW->y, i);
        pW->y += 0x10;
    }
}

// one line of the item set list: number, and id + name when the record is live
void dispItemSetList1(s16 x, s16 y, int no)
{
    SceAtWork* a = &pW->area[no];
    int col;
    int col2; // a second variable: one `col` set in four places is global and loses r29 to `a`
    u16 id;

    if (a->flag & 1) {
        col = 0;
        if (no == pW->areaNo) col = 6;
        eprintf(x, y, col, 0, "[%02d]", no);
        col2 = 0;
        if (no == pW->areaNo) col2 = 6;
        id = a->item.id;
        eprintf(x, y, (u8) col2, 0, "      %02x %s", id, getItemIdStr(id));
    } else {
        eprintf(x, y, 7, 0, "[%02d]", no);
    }
}

static TOOL_MENU tSceItemCreateMenu[3] = {
    {1, "AREA CREATE", NULL},
    {0, "AREA PASTE", tSceItemAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", tSceItemAreaEdit_CopyBuffClear},
};

static TOOL_MENU tSceItemEditMenu[6] = {
    {1, "AREA MOVE", NULL},
    {1, "DATA INPUT", NULL},
    {1, "AREA COPY", tSceItemAreaEdit_AreaCopy},
    {0, "AREA PASTE", tSceItemAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", tSceItemAreaEdit_CopyBuffClear},
    {1, "AREA DELEAT", tSceItemAreaEdit_AreaDelete},
};

// Record menu: existing -> AREA MOVE / DATA INPUT / AREA COPY / PASTE / COPY BUFF CLEAR / AREA
// DELEAT, empty -> AREA CREATE / PASTE / CLEAR; B back.
static void tSceItemAreaEdit_EditMenu()
{
    s8 sel;
    u8 valid = pW->copyValid;

    tSceItemCreateMenu[1].Be_flg = tSceItemCreateMenu[2].Be_flg = tSceItemEditMenu[3].Be_flg =
        tSceItemEditMenu[4].Be_flg = valid;
    if (pCur->flag & 1) {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, tSceItemEditMenu, sizeof(tSceItemEditMenu), &Joy[0]);
        switch (sel) {
        case 0:
            pW->sub = 2;
            pW->step = sel;
            pW->step2 = sel;
            break;
        case 1:
            pW->inputCursor = 0;
            pW->sub = 3;
            pW->step = 0;
            pW->step2 = 0;
            break;
        }
    } else {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, tSceItemCreateMenu, sizeof(tSceItemCreateMenu),
                               &Joy[0]);
        if (sel == 0) {
            pW->sub = 4;
            pW->step = sel;
            pW->step2 = sel;
            pW->editCursor = sel;
        }
    }
    if (Joy[0].trg & JOY_B) {
        SUB_RESET();
    }
}

static TOOL_MENU tSceItemShapeMenu[3] = {
    {1, "SQUARE", NULL},
    {1, "CIRCLE", NULL},
    {0, "EYE TRG", NULL},
};

// AREA CREATE: SQUARE / CIRCLE / EYE TRG at the player, a fresh ITEM record.
static void tSceItemAreaEdit_AreaCreate()
{
    s8 sel;

    switch (pW->step) {
    case 0:
        if (pCur->flag) pW->editCursor = pCur->area.type - 1;
        pW->step++;
    case 1:
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, tSceItemShapeMenu, sizeof(tSceItemShapeMenu), &Joy[0]);
        if (sel >= 0) {
            if ((pW->editCursor != pCur->area.type - 1 && pCur->flag) || pCur->flag == 0) {
                switch (sel) {
                case 0:
                    AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_XZ4, 1500.0f, 1000.0f);
                    break;
                case 1:
                    AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_CYLINDER, 1000.0f, 1000.0f);
                    break;
                case 2:
                    AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_EYE, 200.0f, 1000.0f);
                    break;
                }
            }
            pCur->type = 3;
            pCur->flag |= 3;
            pCur->trigger = 2;
            pCur->checkType = 1;
            pCur->checkFlag |= 1;
            pCur->angle = 0;
            pCur->angleRange = 0x2D;
            pCur->otNo = 8;
            pW->editCursor = 0;
            pW->sub = 1;
            pW->step = 0;
            pW->step2 = 0;
        }
        break;
    }
    if (Joy[0].trg & JOY_B) {
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
    }
}

// Copies the record into the copy buffer.
static void tSceItemAreaEdit_AreaCopy()
{
    pW->copyBuf = *pCur;
    pW->copySrc = pW->areaNo;
    pW->copyValid = 1;
}

// Overwrites the record with the copy buffer.
static void tSceItemAreaEdit_AreaPaste()
{
    *pCur = pW->copyBuf;
}

// Empties the copy buffer.
static void tSceItemAreaEdit_CopyBuffClear()
{
    memclr_asm(&pW->copyBuf, sizeof(SceAtWork));
    pW->copySrc = 0;
    pW->copyValid = 0;
}

// Clears the record.
static void tSceItemAreaEdit_AreaDelete()
{
    pCur->flag &= ~1;
    pW->editCursor = 0;
}

// the hit-angle arrow of an area: centre, direction and the +-range fan (the older RotMatrix build
// of t_sce_at's angle_arrow_disp)
static void angle_arrow_disp(SceAtWork* a)
{
    Vec center;
    Vec p;
    Mtx m;
    Vec v = {0.0f, 0.0f, 1000.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    f32 sweep;
    u32 i;

    AreaGetCenterPos(&center, &a->area);
    center.y += 100.0f;
    rot.y = (f32) (a->angle * 2) * DEG2RAD;
    PSMTXIdentity(m);
    RotMatrix(m, &rot);
    TransMatrix(m, &center);
    PSMTXMultVec(m, &v, &p);
    Draw_sphere(&center, 150.0f, 0xFFFFA0FF, 1, 1);
    Draw_sphere(&center, 80.0f, 0x808040FF, 0, 0);
    Draw_sphere(&p, 80.0f, 0xFFFFA0FF, 0, 0);
    Draw_line3d(&center, &p, 0xFEFFFFA0, 0);
    sweep = (f32) ((a->angle + a->angleRange) * 2) * DEG2RAD - (f32) ((a->angle - a->angleRange) * 2) * DEG2RAD;
    for (i = 0; i <= 10; i++) {
        if (i == 5) continue;
        rot.y = (f32) ((a->angle - a->angleRange) * 2) * DEG2RAD + sweep * (f32) i / 10.0f;
        rot.y = LIMIT_ANGLE(rot.y);
        PSMTXIdentity(m);
        RotMatrix(m, &rot);
        TransMatrix(m, &center);
        PSMTXMultVec(m, &v, &p);
        if (i == 0 || i == 10) {
            Draw_line3d(&center, &p, 0xFEFFFFA0, 0);
        } else {
            Draw_line3d(&center, &p, 0xFE40FFFF, 0);
        }
    }
}

// every live record: its trigger area (or the automatic one around the item / the linked model),
// the item position sphere and the hit-angle arrow
void tSceItemAreaEdit_disp()
{
    AreaData ad;
    AreaData save;
    cEm* obj;
    int i;
    u32 colA;
    u32 colB;
    u32 col;

    for (i = 0; i < AREA_NUM; i++) {
        if ((pW->area[i].flag & 1) == 0) continue;
        if (pW->areaNo == i) {
            colA = 0xA0FF8080;
            colB = 0x90300080;
        } else {
            colA = 0x60808080;
            colB = 0x3010002A;
        }
        col = colB;
        if (SceAtItemHitCheck(&pW->area[i], NULL) == 1) col = colA;
        obj = NULL;
        if (pW->area[i].linkType == 2 && !(pW->area[i].item.flag2 & 1)) {
            if (getRoomEtcBreak(pW->area[i].linkNo, &obj, 0) == 1) {
                col = colB;
                if (SceAtItemHitCheck(&pW->area[i], &obj->pos) == 1) col = colA;
                Draw_sphere(&obj->pos, 100.0f, col, 0, 0);
            }
        } else if (pW->area[i].item.flag & 1) {
            Vec pos;
            SceAtDataEyeTriggreCopy(&ad, &pW->area[i]);
            AreaDataDisp(&ad, col, 1, NULL);
            pos.x = pW->area[i].item.pos.x;
            pos.y = pW->area[i].item.pos.y;
            pos.z = pW->area[i].item.pos.z;
            Vec rot = {0.0f, 0.0f, 0.0f};
            Vec p;
            Vec ofs = {0.0f, 0.0f, 0.0f};
            Mtx m;
            ofs.x = pW->area[i].item.ofs.x;
            ofs.y = pW->area[i].item.ofs.y;
            ofs.z = pW->area[i].item.ofs.z;
            p = ofs;
            if (pW->area[i].item.rot.z > 0.0f) {
                rot.x = pW->area[i].item.rot.x;
                rot.y = pW->area[i].item.rot.y;
            }
            low_RotMatrix(m, &rot);
            TransMatrix(m, &pos);
            PSMTXMultVec(m, &p, &pos);
            Draw_sphere(&pos, 50.0f, 0xFFFF00FF, 1, 1);
        }
        if (pW->area[i].item.flag2 & 2) {
            AreaDataDisp(&pW->area[i].area, colA, 1, NULL);
            if (pW->area[i].checkFlag & 2) angle_arrow_disp(&pW->area[i]);
        } else {
            if (obj != NULL) {
                SceAtItemAutoArea(&ad, &obj->pos, pW->area[i].item.size);
                AreaDataDisp(&ad, colA, 1, NULL);
            } else if (!LINKED(&pW->area[i])) {
                SceAtItemAutoArea(&ad, &pW->area[i].item.pos, pW->area[i].item.size);
                AreaDataDisp(&ad, colA, 1, NULL);
            }
            if (pW->area[i].checkFlag & 2) {
                save = pW->area[i].area;
                pW->area[i].area = ad;
                angle_arrow_disp(&pW->area[i]);
                pW->area[i].area = save;
            }
        }
    }
    tSceItemPreview_pl_pos();
    eprintf(0x1AE, 0x24, 0, 0, "[PLAYER]");
    eprintf(0x1AE, 0x34, 0, 0, "X:%.0f", pPL->pos.x);
    eprintf(0x1AE, 0x44, 0, 0, "Y:%.0f", pPL->pos.y);
    eprintf(0x1AE, 0x54, 0, 0, "Z:%.0f", pPL->pos.z);
    eprintf(0x1AE, 0x64, 0, 0, "ANG:%f", pPL->ang.y);
}

// AREA MOVE: the shared AreaDataEdit editor on the record's area; B back.
static void tSceItemAreaEdit_AreaMove()
{
    if (pCur->flag & 1) {
        AreaDataEdit(&pCur->area, 0xA0FF8080, 0, NULL, 0.75f);
        AreaDataInfoDisp(&pCur->area, pW->x, pW->y);
        AreaDataHelpDisp(&pCur->area, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));
    }
    if (Joy[0].trg & JOY_B) {
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
    }
}

// DATA INPUT: the item payload editor (tSceItemDataInput_item); B back.
static void tSceItemAreaEdit_DataInput()
{
    if (pCur->flag & 1) {
        tSceItemDataInput_item();
    }
    if (Joy[0].trg & JOY_B) {
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static const char* tSceItemHitTypeName[4] = {"UNDER", "FRONT", "UNDER+ANGLE", "FRONT+ANGLE"};

// the common head of the data input menu: hit type, hit angle, open angle, priority
// The first pCur read of each case is a fresh `lis` in the original; our cse1 substitutes the
// earlier high pseudo along the dispatch tree and gcse then copy-propagates the PRE reg into the
// later sites of the case (the t_sce_at basic_menu mechanism).  Distinct SYMBOL_REFs keep them apart.
extern SceAtWorkPtr sceItemCur_c0 asm("sceItemCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceItemCur_c1 asm("sceItemCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceItemCur_c2 asm("sceItemCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceItemCur_c3 asm("sceItemCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
#define PC(n) (sceItemCur_##n.p)
// The shared rows HIT TYPE, HIT ANGLE, OPEN ANGLE, PRIORITY: `sel` is the cursor row, left/right
// change it, values printed beside the menu.
void tSceItemDataInput_basic_menu(int sel, TOOL_MENU* menu)
{
    s16 x;
    s16 y;
    int n;
    int m;
    u8 on;

    on = pCur->checkFlag & 2;
    if (on) on = 1;
    menu[1].Be_flg = on;
    menu[2].Be_flg = on;
    // dead test (n is re-set before every read): its branch splits the sched1 region so the
    // pW `lis` is not issued with the first menu store; deleted at flow2, emits nothing
    if (menu == 0) n = 0; // COMPILER-DIFF: #13 (region split, dead test)
    x = pW->x + 0x80;
    y = pW->y;
    switch (sel) {
    case 0:
        n = PC(c0)->checkFlag;
        if (Joy[0].rep & REP_RIGHT) n--;
        if (Joy[0].rep & REP_LEFT) n++;
        m = WRAP(n, 3);
        pCur->checkFlag = m;
        break;
    case 1: {
        SceAtWork* a = PC(c1);
        if (a->checkFlag & 2) {
            n = a->angle;
            if (Joy[0].rep & 0x20000) n += 0x2D;
            if (Joy[0].rep & 0x10000) n -= 0x2D;
            if (Joy[0].rep & JOY_RIGHT) n += 5;
            if (Joy[0].rep & JOY_LEFT) n -= 5;
            if (n > 0x59) n -= 0xB4;
            if (n < -0x5A) n += 0xB4;
            a->angle = n;
        }
        break;
    }
    case 2:
        if (PC(c2)->checkFlag & 2) {
            n = PC(c2)->angleRange;
            if (Joy[0].rep & REP_RIGHT) n += 5;
            if (Joy[0].rep & REP_LEFT) n -= 5;
            CLAMP_SET(n, m, 0x5A);
            pCur->angleRange = m;
        }
        break;
    case 3:
        n = PC(c3)->otNo;
        STEP(rep, n);
        CLAMP_SET(n, m, 0xF);
        pCur->otNo = m;
        break;
    default:
        eprintf(x + 0x40, y, 5, 0, "(push Y:data init.)");
        if (Joy[0].trg & JOY_Y) memclr_asm(pCur->data, sizeof(pCur->data));
        break;
    }
    eprintf(x, y, 0, 0, "%s", tSceItemHitTypeName[pCur->checkFlag]);
    y += 0x10;
    if (pCur->checkFlag & 2) {
        eprintf(x, y, 0, 0, "%d", pCur->angle * 2);
        y += 0x10;
        eprintf(x, y, 0, 0, "%d", pCur->angleRange * 2);
        y += 0x10;
    } else {
        y += 0x20;
    }
    if (pCur->otNo == 8) {
        eprintf(x, y, 0, 0, "default");
    } else {
        eprintf(x, y, 0, 0, "%d", pCur->otNo);
    }
    eprintf(x, y, 6, 0, "        [0:low - 15:high]");
    y += 0x10;
    pW->y = y;
}

// Item record editor: step 0 the row menu, 1 the position / effect offset editors.
void tSceItemDataInput_item()
{
    void (*routine[3])() = {tSceItemDataInput_item_main, tSceItemDataInput_item_ETedit,
                            tSceItemDataInput_item_EffPosSet};

    routine[pW->step]();
}

static TOOL_MENU tSceItemMenu[21] = {
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "PRIORITY", NULL},
    {1, " ITEM_ID", NULL},
    {1, " ITEM_FLG", NULL},
    {1, " ITEM_NUM", NULL},
    {1, " SET_WAIT_TYPE", NULL},
    {1, "  SET_TARGET_NO", NULL},
    {1, " POSITION_AUTO", NULL},
    {1, " POSITION_SET", NULL},
    {1, " HIT_AREA_AUTO", NULL},
    {1, " SEE_CHECK", NULL},
    {1, " EFF_TYPE", NULL},
    {1, " EFF_OFFSET", NULL},
    {1, " RADIUS", NULL},
    {1, " DROP_TYPE", NULL},
    {1, " SE_NO_DROP", NULL},
    {1, " SE_NO_HIT", NULL},
    {1, " HIDE_SET", NULL},
    {1, " COUNTRY", NULL},
};

static const char* tSceItemEffTypeName[10] = {"none",       "KIRA EFF",     "EM DROP(W)",   "EM DROP(B)", "EM DROP(G)",
                                              "EM DROP(R)", "AUTO",         "EM DROP(BIG)", "EM DROP(KEY)", "SANDGLASS"};
static const char* tSceItemWaitTypeName[4] = {"NORMAL_SET", "EM_DEAD", "ETC_BREAK", "SHOT_DROP"};

#define NAME(tbl, n, hi) ((u32) (n) <= (hi) ? tbl[n] : "...no string")

// Item rows: basic + ITEM_ID (stepped through item_id.h names), ITEM_FLG, ITEM_NUM, SET_WAIT_TYPE,
// SET_TARGET_NO, POSITION_AUTO / POSITION_SET (drop position from the area or the eye editor),
// HIT_AREA_AUTO, EFFECT_OFFSET_X/Y/Z; left/right change, A opens the editors.
static void tSceItemDataInput_item_main()
{
    s16 x;
    s16 y;
    int n;
    int k;
    u16 id;

    if (LINKED(pCur)) {
        tSceItemMenu[8].Be_flg = 1;
        tSceItemMenu[9].Be_flg = 1;
    } else {
        tSceItemMenu[8].Be_flg = 0;
        tSceItemMenu[9].Be_flg = 0;
    }
    if (LINKED(pCur) && !(pCur->item.flag2 & 1)) {
        tSceItemMenu[10].Be_flg = 0;
    } else {
        tSceItemMenu[10].Be_flg = 1;
    }
    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceItemMenu, sizeof(tSceItemMenu), &Joy[0]);
    tSceItemDataInput_basic_menu(pW->inputCursor, tSceItemMenu);
    switch (pW->inputCursor) {
    case 4: {
        int m;
        n = pCur->item.id;
        if (Joy[0].rep2 & REP_RIGHT) {
            if (n == 0xFE) {
                n = 0x1000;
            } else {
                if (Joy[0].rep2 & JOY_RIGHT) n++;
                if (Joy[0].rep2 & 0x20000) n += 0x10;
                if (n >= 0xFF && n <= 0xFFF) n = 0xFE;
            }
        }
        if (Joy[0].rep2 & REP_LEFT) {
            if (n == 0x1000) {
                n = 0xFE;
            } else {
                if (Joy[0].rep2 & JOY_LEFT) n--;
                if (Joy[0].rep2 & 0x10000) n -= 0x10;
                if (n >= 0xFF && n <= 0xFFF) n = 0x1000;
            }
        }
        m = WRAP(n, 0x1005);
        pCur->item.id = m;
        break;
    }
    case 5: {
        int m;
        n = pCur->item.flagNo;
        STEP(rep2, n);
        m = WRAP(n, 0xFF);
        pCur->item.flagNo = m;
        break;
    }
    case 6: {
        int m;
        n = pCur->item.num;
        if (Joy[0].rep2 & JOY_RIGHT) n++;
        if (Joy[0].rep2 & JOY_LEFT) n--;
        if (Joy[0].rep2 & 0x20000) n += 100;
        if (Joy[0].rep2 & 0x10000) n -= 100;
        m = WRAP(n, 60000);
        pCur->item.num = m;
        break;
    }
    case 7: {
        int m;
        n = pCur->linkType;
        STEP(rep2, n);
        CLAMP_SET(n, m, 2);
        pCur->linkType = m;
        break;
    }
    case 8:
        if (pCur->linkType == 1) {
            int m;
            n = pCur->linkNo;
            STEP(rep2, n);
            m = WRAP(n, 0xFF);
            pCur->linkNo = m;
        }
        if (pCur->linkType == 2) {
            n = pCur->linkNo;
            STEP(rep2, n);
            if (n < 0) {
                k = EtcModelGetLastNo();
            } else {
                k = n > EtcModelGetLastNo() ? 0 : n;
            }
            pCur->linkNo = k;
        }
        break;
    case 9:
        if (LINKED(pCur)) {
            if (Joy[0].rep2 & (REP_RIGHT | REP_LEFT)) pCur->item.flag2 ^= 1;
        }
        break;
    case 0xA:
        if (LINKED(pCur)) {
            if (!(pCur->item.flag2 & 1)) break;
        }
        if (Joy[0].trg & JOY_A) pW->step = 1;
        break;
    case 0xB:
        if (Joy[0].rep2 & (REP_RIGHT | REP_LEFT)) pCur->item.flag2 ^= 2;
        break;
    case 0xC:
        if (Joy[0].rep2 & (REP_RIGHT | REP_LEFT)) pCur->item.flag2 ^= 4;
        break;
    case 0xD: {
        int m;
        n = pCur->item.effType;
        STEP(rep, n);
        CLAMP_SET(n, m, 9);
        pCur->item.effType = m;
        break;
    }
    case 0xE:
        if (Joy[0].trg & JOY_A) pW->step = 2;
        break;
    case 0xF:
        if (Joy[0].rep & REP_RIGHT) pCur->item.size += 100.0f;
        if (Joy[0].rep & REP_LEFT) pCur->item.size -= 100.0f;
        if (pCur->item.size < 0.0f) pCur->item.size = 0.0f;
        break;
    case 0x10: {
        u8 f = pCur->item.flag2;
        if (f & 0x10) {
            if (Joy[0].rep2 & REP_RIGHT) {
                pCur->item.flag2 = f & ~0x10;
                pCur->item.flag2 |= 0x40;
            }
            if (Joy[0].rep2 & REP_LEFT) {
                pCur->item.flag2 &= ~0x10;
            }
        } else if (f & 0x40) {
            if (Joy[0].rep2 & REP_LEFT) {
                pCur->item.flag2 = f & ~0x40;
                pCur->item.flag2 |= 0x10;
            }
        } else {
            if (Joy[0].rep2 & REP_RIGHT) pCur->item.flag2 = f | 0x10;
        }
        break;
    }
    case 0x11: {
        int m;
        n = pCur->item.seFind;
        STEP(rep2, n);
        CLAMP_SET(n, m, 0xFF);
        pCur->item.seFind = m;
        break;
    }
    case 0x12: {
        int m;
        n = pCur->item.seDamage;
        STEP(rep2, n);
        CLAMP_SET(n, m, 0xFF);
        pCur->item.seDamage = m;
        break;
    }
    case 0x13:
        if (Joy[0].rep2 & REP_RIGHT) pCur->item.flag2 |= 0x80;
        if (Joy[0].rep2 & REP_LEFT) pCur->item.flag2 &= 0x7F;
        break;
    case 0x14: {
        int m;
        n = pCur->country.get();
        STEP(rep2, n);
        CLAMP_SET(n, m, 2);
        pCur->country.set(m);
        break;
    }
    }
    tSceItem_PointDisp(pCur->item.pos.x, pCur->item.pos.y, pCur->item.pos.z);
    id = pCur->item.id;
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%02X %s", id, getItemIdStr(id));
    y += 0x10;
    if (pCur->item.flagNo == 0) {
        eprintf(x, y, 0, 0, "auto");
    } else {
        eprintf(x, y, 0, 0, "%02X", pCur->item.flagNo);
    }
    y += 0x10;
    if (pCur->item.num == 0) {
        eprintf(x, y, 0, 0, "default");
    } else {
        eprintf(x, y, 0, 0, "%d", pCur->item.num);
    }
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", NAME(tSceItemWaitTypeName, pCur->linkType, 3));
    y += 0x10;
    if (LINKED(pCur)) eprintf(x, y, 0, 0, "%d", pCur->linkNo);
    y += 0x10;
    if (LINKED(pCur)) eprintf(x, y, 0, 0, "%s", (pCur->item.flag2 & 1) ? "OFF" : "ON");
    y += 0x20;
    eprintf(x, y, 0, 0, "%s", (pCur->item.flag2 & 2) ? "OFF" : "ON");
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (pCur->item.flag2 & 4) ? "OFF" : "ON");
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", NAME(tSceItemEffTypeName, pCur->item.effType, 9));
    y += 0x20;
    eprintf(x, y, 0, 0, "%f", pCur->item.size);
    y += 0x10;
    if (pCur->item.flag2 & 0x10) {
        eprintf(x, y, 0, 0, "SHOT & FALL");
    } else if (pCur->item.flag2 & 0x40) {
        eprintf(x, y, 0, 0, "FALL");
    } else {
        eprintf(x, y, 0, 0, "FIX");
    }
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->item.seFind);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->item.seDamage);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (pCur->item.flag2 & 0x80) ? "ON" : "OFF");
    y += 0x10;
    {
        // defined here: its strings follow the ones above in .rodata
        static const char* countryName[3] = {"JPN USA", "JPN", "    USA"};
        eprintf(x, y, 0, 0, "%s", NAME(countryName, pCur->country.get(), 2));
    }
    y += 0x10;
    pW->y = y;
}

// POSITION_SET: the item position / rotation edited as an eye trigger area
static void tSceItemDataInput_item_ETedit()
{
    SceAtItem* it = &pCur->item;
    Vec c;

    switch (pW->step2) {
    case 0:
        if (!(it->flag & 1)) {
            AreaGetCenterPos(&c, &pCur->area);
            AreaDataInit(&pW->editArea, &c, AREA_TYPE_EYE, 200.0f, 1000.0f);
            it->flag |= 1;
        } else {
            SceAtDataEyeTriggreCopy(&pW->editArea, pCur);
        }
        pW->step2++;
    case 1:
        AreaDataEdit(&pW->editArea, 0xA0FF8080, 0, NULL, 0.5f);
        if (SceAtItemHitCheck(pCur, NULL) == 1) {
            AreaDataDisp(&pW->editArea, 0xA0FF8080, 1, NULL);
        } else {
            AreaDataDisp(&pW->editArea, 0x60000080, 1, NULL);
        }
        AreaDataInfoDisp(&pW->editArea, pW->x, pW->y);
        AreaDataHelpDisp(&pW->editArea, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));
        it->pos.x = pW->editArea.u.eye.xz;
        it->pos.y = pW->editArea.u.eye.floor;
        it->pos.z = pW->editArea.u.eye.z;
        it->rot.z = pW->editArea.u.eye.open_ang;
        it->rot.x = pW->editArea.u.eye.ang_x;
        it->rot.y = pW->editArea.u.eye.ang_y;
        if (!((u32) it->pModel < 0x80000000 || (u32) it->pModel > 0x82FFFFFF)) {
            if (it->rot.z > 0.0f) {
                it->pModel->ang.x = it->rot.x;
                it->pModel->ang.y = it->rot.y;
            }
        }
        if (Joy[0].trg & JOY_B) {
            pW->step = 0;
            pW->step2 = 0;
            Joy[0].trg = 0;
        }
        break;
    }
}

static TOOL_MENU tSceItemEffOfsMenu[3] = {
    {1, " EFFECT_OFFSET_X", NULL},
    {1, " EFFECT_OFFSET_Y", NULL},
    {1, " EFFECT_OFFSET_Z", NULL},
};

// EFF_OFFSET: the effect offset by axis
static void tSceItemDataInput_item_EffPosSet()
{
    s16 x;
    s16 y;

    switch (pW->step2) {
    case 0:
        pW->exitCursor = 0;
        pW->step2++;
    case 1:
        ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->exitCursor, tSceItemEffOfsMenu, sizeof(tSceItemEffOfsMenu), &Joy[0]);
        switch (pW->exitCursor) {
        case 0:
            if (Joy[0].rep & REP_RIGHT) pCur->item.ofs.x += 50.0f;
            if (Joy[0].rep & REP_LEFT) pCur->item.ofs.x -= 50.0f;
            break;
        case 1:
            if (Joy[0].rep & REP_RIGHT) pCur->item.ofs.y += 50.0f;
            if (Joy[0].rep & REP_LEFT) pCur->item.ofs.y -= 50.0f;
            break;
        case 2:
            if (Joy[0].rep & REP_RIGHT) pCur->item.ofs.z += 50.0f;
            if (Joy[0].rep & REP_LEFT) pCur->item.ofs.z -= 50.0f;
            break;
        }
        if (Joy[0].trg & JOY_B) {
            pW->step = 0;
            pW->step2 = 0;
            Joy[0].trg = 0;
        }
        x = pW->x + 0xA0;
        y = pW->y;
        eprintf(x, y, 0, 0, "%f", pCur->item.ofs.x);
        y += 0x10;
        eprintf(x, y, 0, 0, "%f", pCur->item.ofs.y);
        y += 0x10;
        eprintf(x, y, 0, 0, "%f", pCur->item.ofs.z);
        y += 0x10;
        pW->y = y;
        break;
    }
}

// Draws a cross mark at a world point (the item drop position).
void tSceItem_PointDisp(f32 x, f32 y, f32 z)
{
    Vec p;
    Vec f;

    p.x = x;
    p.y = y;
    p.z = z;
    Draw_sphere(&p, 100.0f, 0xFFFFFF80, 1, 1);
    f = p;
    f.y = SatMgr.getFloor(&p, NULL, 600.0f, 100000.0f, 0);
    Draw_line3d(&p, &f, 0x80808020, 0);
    p = f;
    p.x += 200.0f;
    f.x -= 200.0f;
    Draw_line3d(&p, &f, 0x80808020, 0);
    p.x -= 200.0f;
    f.x += 200.0f;
    p.z += 200.0f;
    f.z -= 200.0f;
    Draw_line3d(&p, &f, 0x80808020, 0);
}

static TOOL_MENU tSceItemLoadMenu[3] = {
    {1, "SERVER", NULL},
    {0, "LOCAL", NULL},
    {1, "don't load", NULL},
};

// DATA LOAD: SERVER / LOCAL / don't load; reads r<room>.ita into the work (magic checked); the tool
// starts here.
static void tSceItemDataLoad()
{
    s8 sel;
    int cmp;

    eprintf(pW->x, pW->y, 4, 0, "[DATA LOAD]");
    pW->y += 0x10;
    switch (pW->sub) {
    case 0:
        sel = ToolMenuDisp(pW->x, pW->y, 0, tSceItemLoadMenu, sizeof(tSceItemLoadMenu), &Joy[0]);
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
        if (pW->file.head.version != 0x105) {
            pW->sub = 9;
            pW->step = 0;
            pW->step2 = 0;
        } else {
            cmp = strcmp(pW->file.head.magic, "ITA");
            if (cmp != 0) {
                pW->sub = 9;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                tSceItemLoadDataCopy();
                pW->sub = 8;
                pW->step = cmp;
                pW->step2 = cmp;
            }
        }
        break;
    case 8:
        pW->loaded = 1;
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

// spreads the loaded records over the area table by their number (ITA records carry it + 0x80)
void tSceItemLoadDataCopy()
{
    u32 i;
    int no;

    memclr_asm(pW->area, sizeof(pW->area));
    memclr_asm(pW->areaLoad, sizeof(pW->areaLoad));
    memclr_asm(pW->flgAuto, sizeof(pW->flgAuto));
    for (i = 0; i < pW->file.head.num; i++) {
        if (pW->file.work[i].no & 0x80) {
            no = pW->file.work[i].no - 0x80;
        } else {
            no = pW->file.work[i].no;
        }
        pW->area[no] = pW->file.work[i];
    }
    *(SceAtWorkTbl*) pW->areaLoad = *(SceAtWorkTbl*) pW->area;
}

static TOOL_MENU tSceItemSaveMenu[3] = {
    {1, "SERVER", NULL},
    {0, "LOCAL", NULL},
    {1, "don't save", NULL},
};

// DATA SAVE: SERVER / LOCAL / don't save; auto-numbers the item flags
// (tSceItemSetItemFlgAutoDataCreate), writes the .ita, exports the AEV xml (tSceSaveXml) and
// installs the image as the room's live item data.
static void tSceItemDataSave()
{
    int ret = 0;
    int size;

    eprintf(pW->x, pW->y, 4, 0, "[DATA SAVE]");
    pW->y += 0x10;
    switch (pW->sub) {
    case 0:
        pW->sub = 1;
        pW->step = ret;
        pW->step2 = ret;
    case 1:
        pW->saveSel = ToolMenuDisp(pW->x, pW->y, 0, tSceItemSaveMenu, sizeof(tSceItemSaveMenu), &Joy[0]);
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
        tSceItemSetItemFlgAutoDataCreate();
        tSceItemSaveDataCreate();
        size = pW->saveNum * sizeof(SceAtWork) + sizeof(TSceItemFileHead);
        switch (pW->saveSel) {
        case 0:
            ret = HDWrite(pW->pathX, &pW->file, size);
            break;
        case 1:
            ret = HDWrite(pW->path, &pW->file, size);
            break;
        }
        tSceItemSetRoomData(size);
        tSceSaveXml();
        pW->timer = 30;
        if (ret != 0) {
            pW->sub = 8;
            pW->step = 0;
            pW->step2 = 0;
            *(SceAtWorkTbl*) pW->areaLoad = *(SceAtWorkTbl*) pW->area;
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

// hands the saved image to the game: the item objects (ObjMgr id 0x19) are destroyed and the
// scenario attribute system re-initialised with a copy of the file
void tSceItemSetRoomData(int size)
{
    u32 i;
    cObj* o;
    TSceItemFile* p;

    if (SceAtSys.m_use_tool_data_i == 1) Mem_free(SceAtSys.pItemData);
    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        o = ObjMgr.fastAt(i);
        if (o->isAlive() && o->id == 0x19) ObjMgr.destroy(o);
    }
#line 1694 "D:/Bio4/Prog/t_sce_item.cpp"
    p = (TSceItemFile*) MEM_ALLOC(size, 1, 0xD);
    memcpy(p, &pW->file, size);
    SceAtInit(SceAtSys.pAtData, p);
    SceAtRoomSet();
    SceAtSys.m_use_tool_data_i = 1;
}

static s8 xmlDepth = 0;

// the item records as an AEV xml file
void tSceSaveXml()
{
    u32 i;
    u8* d;

    xmlP = xmlBuf;
    xmlTab[xmlDepth] = 0;
    xmlP += sprintf(xmlP, "<?xml version=\"1.0\" encoding=\"Shift_JIS\"?>\n");
    xmlP += sprintf(xmlP, "%s<aev>\n\n", xmlTab);
    xmlP += sprintf(xmlP, "<format attrib=\"format_file=Aev_fmt.xml\">\n");
    xmlP += sprintf(xmlP, "</format>\n\n");
    for (i = 0; i < AREA_NUM; i++) {
        if ((pW->area[i].flag & 1) == 0) continue;
        xmlP += sprintf(xmlP, "%s<data>\n", xmlTab);
        xmlTab[xmlDepth++] = '\t';
        xmlTab[xmlDepth] = 0;
        xmlP += sprintf(xmlP, "%s<id>ITEM</id>\n", xmlTab);
        d = pW->area[i].data;
        xmlP += sprintf(xmlP, "%s<item_id>%02x</item_id>\n", xmlTab, *(u16*) (d + 0x1C));
        xmlP += sprintf(xmlP, "%s<item_flg>%02x</item_flg>\n", xmlTab, *(u16*) (d + 0x1E));
        xmlP += sprintf(xmlP, "%s<item_num>%02x</item_num>\n", xmlTab, *(u16*) (d + 0x20));
        xmlP += sprintf(xmlP, "%s<eff_type>%02x</eff_type>\n", xmlTab, d[0x24]);
        xmlP += sprintf(xmlP, "%s<eff_setno>%02x</eff_setno>\n", xmlTab, d[0x25]);
        xmlP += sprintf(xmlP, "%s<player_type>%02x</player_type>\n", xmlTab, d[0x26]);
        xmlP += sprintf(xmlP, "%s<item_flg_auto>%02x</item_flg_auto>\n", xmlTab, *(u16*) (d + 0x22));
        xmlDepth--;
        xmlTab[xmlDepth] = 0;
        xmlP += sprintf(xmlP, "%s</data>\n\n", xmlTab);
    }
    xmlP += sprintf(xmlP, "%s</aev>\n", xmlTab);
    HDWrite_only(pW->xmlPath, xmlBuf, xmlP - xmlBuf);
}

// packs the live areas into the file image (runtime links cleared)
void tSceItemSaveDataCreate()
{
    u32 i;

    pW->saveNum = 0;
    for (i = 0; i < AREA_NUM; i++) {
        if (pW->area[i].flag & 1) {
            pW->area[i].no = i;
            pW->area[i].func = NULL;
            pW->area[i].arg = 0;
            pW->area[i].prioBak = 0;
            pW->area[i].pParent = NULL;
            pW->area[i].item.pModel = NULL;
            pW->area[i].item.effNo = 0;
            pW->file.work[pW->saveNum] = pW->area[i];
            pW->saveNum++;
        }
    }
    pW->file.head.magic[0] = 'I';
    pW->file.head.magic[1] = 'T';
    pW->file.head.magic[2] = 'A';
    pW->file.head.magic[3] = 0;
    pW->file.head.version = 0x105;
    pW->file.head.num = pW->saveNum;
}

// the first free automatic ITEM_SET flag (1..127), taken
int tSceItemSetItemFlgAuto_on()
{
    u32 n;

    for (n = 1; n < 128; n++) {
        if (!FlagChkVar(pW->flgAuto, n)) {
            FlagOnVar(pW->flgAuto, n);
            return n;
        }
    }
    while (1) {
        pLog->err(0, 0, "ITEM SET FLAG (AUTO) OVERFLOW");
        TaskSleep(1);
    }
}

// nonzero when flag `no` is not usable as is (taken already, 0 or out of range)
int tSceItemSetItemFlgAuto_ck(u32 no)
{
    if (no > 0x7F) {
        pLog->err(0, 0, "ITEM SET FLAG (AUTO) OVERFLOW");
        return 1;
    }
    if (no == 0) return 1;
    return FlagChkVar(pW->flgAuto, no);
}

// gives every item without an explicit ITEM_SET flag an automatic one
void tSceItemSetItemFlgAutoDataCreate()
{
    u32 i;
    SceAtWork* tbl = pW->area;
    SceAtItem* it;

    memclr_asm(pW->flgAuto, sizeof(pW->flgAuto));
    for (i = 0; i < AREA_NUM; i++) {
        it = &tbl[i].item;
        if ((tbl[i].flag & 1) == 0) continue;
        if (tbl[i].type != 3) continue;
        if (it->flagNo == 0) {
            if (tSceItemSetItemFlgAuto_ck(it->findFlagNo)) {
                it->findFlagNo = tSceItemSetItemFlgAuto_on();
            } else {
                FlagOnVar(pW->flgAuto, it->findFlagNo);
            }
        } else {
            it->findFlagNo = 0;
        }
    }
}

// CAMERA MODE (START): the debug camera moves with pad 1.
void tSceItemData_DebugCamera()
{
    CamDbg.move(&pG->Camera, &Joy[0], 0);
    pW->timer++;
    if (pW->timer & 8) {
        eprintf(0xD0, 0x10, 6, 0, "CAMERA MODE");
    }
}

// PREVIEW: init / main / exit sub routines.
static void tSceItemPreview()
{
    void (*routine[3])() = {tSceItemPreview_init, tSceItemPreview_main, tSceItemPreview_exit};

    routine[pW->sub]();
}

// Un-pauses the player / HUD for the preview.
static void tSceItemPreview_init()
{
    pG->Stop_flg &= ~0x10000000;
    pG->Disp_flg &= ~0x40000000;
    pG->Disp_flg &= ~0x80000000;
    DbgFlagOff(pG, DBG_DBG_CAM);
    pW->sub = 1;
    pW->step = 0;
    pW->step2 = 0;
}

// PREVIEW MODE: the game runs, the player's hit points are drawn (lit inside an item area); START
// ends it.
static void tSceItemPreview_main()
{
    tSceItemPreview_pl_pos();
    pW->timer++;
    if (pW->timer & 8) {
        eprintf(0xD0, 0x10, 6, 0, "PREVIEW MODE");
    }
    if (Joy[0].trg & JOY_START) {
        pW->sub = 2;
        pW->step = 0;
        pW->step2 = 0;
    }
}

// the player position and a point in front of him, lit when inside an area
void tSceItemPreview_pl_pos()
{
    Vec pos;
    Vec front;
    u32 i;
    int hit;
    int hitFront;

    pos = pPL->pos;
    pos.y += 250.0f;
    front.x = 0.0f;
    front.y = 250.0f;
    front.z = 550.0f;
    PSMTXMultVec(pPL->mat, &front, &front);
    SatMgr.hitCheck(&pos, &front, &front, NULL, 0, 0);
    hit = 0;
    hitFront = 0;
    for (i = 0; i < pW->head.num; i++) {
        if (pW->area[i].flag & 1) {
            if (AreaHitCheck(&pW->area[i].area, &pos) == 1) hit = 1;
            if (AreaHitCheck(&pW->area[i].area, &front) == 1) hitFront = 1;
        }
    }
    if (hit) {
        Draw_sphere(&pos, 150.0f, 0xFF404080, 0, 0);
    } else {
        Draw_sphere(&pos, 150.0f, 0xFFFFFF80, 0, 0);
    }
    if (hitFront) {
        Draw_sphere(&front, 150.0f, 0xFF404080, 0, 0);
    } else {
        Draw_sphere(&front, 150.0f, 0xFFFFFF80, 0, 0);
    }
    Draw_line3d(&pos, &front, 0xFF40FF80, 0);
    front = pos;
    front.y += 1000.0f;
    Draw_line3d(&pos, &front, 0xFF40FF80, 0);
}

// Restores the tool flags, back to the main menu.
static void tSceItemPreview_exit()
{
    pG->Stop_flg |= 0x20000000;
    pG->Stop_flg |= 0x10000000;
    pG->Stop_flg |= 0x8000000;
    pG->Stop_flg |= 0x800000;
    pG->Stop_flg |= 0x400000;
    pG->Stop_flg |= 0x10000;
    pG->Stop_flg |= 0x2000;
    pG->Disp_flg |= 0x40000000;
    pG->Disp_flg |= 0x80000000;
    DbgFlagOn(pG, DBG_DBG_CAM);
    MODE_RESET();
}

// reads the `ITEM_ID_xxx = 0x..,` enumerators of the item id header into `names` (64 chars each);
// the ones from ITEM_ID_SYSTEM_TOP (0x1000) on go to `names2`
void loadItemIdName(const char* path, char* names, char* names2)
{
    void* buf;
    char* end;
    char* p;
    char* e;
    char* q;
    char* dst;
    u32 no;
    int len;
    int sys;
    {
        // The original allocates `e`/`no` to r30 and `p` to r31 although e outranks p in global-alloc
        // priority: r30 was ever-live before global-alloc there.  A codeless asm reading r30 makes it
        // used-so-far, so e/no take it in pass 0 and p falls to r31 in pass 1.
        register int pin asm("r30"); // COMPILER-DIFF: candidate #17
        asm("" : : "r"(pin));
    }

    sys = 0;
    if (HDReadDebugAlloc(path, &buf, 1) == 0) return;
    p = strstr((char*) buf, "enum");
    if (p == NULL) goto done;
    p += 4;
    while ((p = strstr(p, "ITEM_ID_")) != NULL) {
        p += 8;
        if (strncmp(p, "NUM", 3) == 0) {
            p = strstr(p, "ITEM_ID_SYSTEM_TOP");
            if (p == NULL) break;
            p += 0x12;
            sys = 1;
            continue;
        }
        e = strchr(p, '\r');
        if (e == NULL) break;
        end = strchr(p, '=');
        if (end == NULL) break;
        if (end > e) break;
        end++;
        end = space_skip(end);
        no = strtoul(end, &end, 16);
        q = strpbrk(p, "\t (/\r\n");
        if (q == NULL) break;
        len = q - p;
        if (len > 62) len = 63;
        if (sys == 0) {
            no *= 64;
            e = (char*) (no + (u32) names);
            memcpy(e, p, len);
            e[len] = 0;
        } else {
            no *= 64;
            e = (char*) (no + (u32) names2);
            memcpy(e - 0x40000, p, len);
            {
                int zero = 0; // declared before the `e += len` so its `li` precedes the `subis` in LUID order
                e += len;
                e -= 0x40000;
                *e = zero;
            }
        }
    }
done:
    Debug_free(buf);
}

// Item name for `id` (0..0xFE from idName, 0x1000.. from idName2).
char* getItemIdStr(u32 id)
{
    if (id <= 0xFE) return pW->idName[id];
    return pW->idName2[id - 0x1000];
}

asm(".section .data; .balign 8");
