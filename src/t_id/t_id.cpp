// t_id REL: ToolInterfaceDesign (2D interface / sub screen designer, D:/Bio4/Prog/t_id.cpp). Edits
// the IDSystem element tables (the .uwf files behind IdSys / IdSub: HUD, sub screens, title ...) as
// a tree of ID_DATA elements (texture, position with paths and jitter, size / colour / rotation
// curves, blend, mark id, groups), draws them live through the tool's own IDSystem
// (toolIdDataEncode every frame) and loads / saves x:\soft/Room/SubScreen/<lang>/uwf/<kind>NNN.uwf.
// Uses db_path (paths), db_sctrl (curves) and the DbRandom jitter editor.

#include "types.h"
#include "model.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "joy.h"
#include "scheduler.h"
#include "file.h"
#include "gx_sub.h"
#include "camera.h"
#include "cockpit.h"
#include "id_sys.h"
#include "eprintf.h"
#include "db_log.h"
#include "texture.h"
#include "dbmodule.h"
#include "tools.h"
#include "t_util.h"
#include "db_path.h"
#include "math_sub.h"
#include "hermite.h"
#include "t_id.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// The module's 0x34-byte COMMON block (uninitialised template statics of the original build; the split
// skeleton of this unit defines it as `common_<mod>`, see em10.cpp / st_room.h): make_rel refuses the
// link without it. REL_MODULE comes from configure.py.
#define T_ID_STR2(x) #x
#define T_ID_STR(x) T_ID_STR2(x)
asm(".comm common_" T_ID_STR(REL_MODULE) ",52,4");

// COMPILER-DIFF #4: the original passes the (u32) converted height without the u16 truncation.
void ScreenReSizeI(int w, u32 h) asm("ScreenReSize");

// TexAnm header as read by the id editor (texture.h keeps the first 8 bytes opaque)
struct TexAnmSize {
    u16 w;  // 0x00
    u16 h;  // 0x02
};

typedef void (*IdToolFunc)(IdTool*);

static const char* subScreenName[21] = {
    "tool", "ckpt", "option", "dead", "scope", "share", "case", "item", "map", "shop", "file",
    "term", "cap", "tool2", "event", "title", "save", "result", "chapter", "omk_t", "omk_r",
};
static const char* langName[7] = { "jpn", "eng", "ger", "fra", "spa", "ita", "cmn" };

static void toolIdQuit(IdTool* w);
static void toolIdPrev(IdTool* w);
static void toolIdMenu(IdTool* w);
static void toolIdMain(IdTool* w);
static void toolIdEdit(IdTool* w);
static void toolIdOption(IdTool* w);
static void toolIdFile(IdTool* w);

static IdToolFunc toolIdFunc[3] = { toolIdPrev, toolIdMenu, toolIdMain };

static ID_DATA idData[ID_DATA_NUM];
static ID_DATA idClip[ID_CLIP_NUM];
IDSystem toolIdSys;
static DbPathWork idPath;
static DbSctrlWork idSctrl0;
static DbSctrlWork idSctrl1;
static DbSctrlWork idSctrl2;
static DbSctrlWork idSctrl3;
static IdRandomWork idRandom;
void* pIdBuf0;
void* pIdBuf1;
void* pIdBuf2;
void* pIdBuf3;

// Debug-heap buffer for the tool's file images.
static inline void IdBufAlloc(void*& p, u32 size)
{
    p = Debug_alloc(size, 1);
}

// Tool start: 640x448 screen, tool flags (HUD off, Debug_flg bits), the work cleared (level 0, no
// parent, language / type from the previous run), every ID_DATA slot freed, the four file buffers
// (0x20000 .. 0x900000) and the tool's own IDSystem (0x200 units).
void toolIdInit(IdTool* w)
{
    GXColor col;
    int i;
    // COMPILER-DIFF: candidate (the target keeps a 1 in r30 from the prologue for the two lang = 1 arms)
    u8 one = 1;

    ToolArrayPush(0);
    IdDebugAllocBuffer();
    memclr_asm(w, sizeof(IdTool));
    memclr_asm(&idPath, sizeof(DbPathWork));
    memclr_asm(&idSctrl0, sizeof(DbSctrlWork));
    memclr_asm(&idSctrl1, sizeof(DbSctrlWork));
    TaskSuspend(0);
    col.b = 0x30;
    col.g = 0x30;
    col.r = 0x30;
    col.a = 0;
    bio4_GXSetCopyClear(col, 0xFFFFFF);
    ScreenReSize(0x280, 0x1C0);
    TaskSleep(3);
    DbgFlagOn(pG, DBG_DBG_CAM);
    DbgFlagOff(pG, DBG_PROC_BAR);
    DbgFlagOff(pG, DBG_COCKPIT_TOOL);
    pG->System_flg &= ~0x800;
    DbgFlagOn(pG, DBG_CINESCO_OFF);
    DbgFlagOn(pG, DBG_ID_TOOL);
    toolIdSetCamera(w);
    switch (pSys->language) {
    case 0:
        w->lang = 0;
        break;
    case 1:
        w->lang = one;
        break;
    case 2:
        w->lang = one;
        break;
    case 3:
        w->lang = 2;
        break;
    case 4:
        w->lang = 3;
        break;
    case 5:
        w->lang = 4;
        break;
    case 6:
        w->lang = 5;
        break;
    }
    w->lang2 = w->lang;
    // Store order = sched1 "dying source first" (INSN_REG_WEIGHT) then sched2 "more dependents first"
    // (r0/r9/r11 are rewritten after the calls); the QI zeros must all precede the SI chain, whose
    // subreg the later QI zero stores would otherwise take (see docs/research/ "t_id pass 6").
    w->type = 0;
    w->type2 = 0;
    w->pause = 0;
    w->cnt = 0;
    w->menuX = 100;
    w->menuY = 100;
    w->level = w->x24 = 0;
    w->drawSafe = 1;
    w->parentNo = 0xFF;
    toolIdClipboardClear();
    w->useCnt = toolIdClipboardCount(0xFF, 1);
    w->empCnt = toolIdClipboardCount(0xFF, 2);
    IdBufAlloc(pIdBuf0, 0x20000);
    IdBufAlloc(pIdBuf1, 0x100000);
    IdBufAlloc(pIdBuf2, 0x300000);
    IdBufAlloc(pIdBuf3, 0x900000);
    IdSys.roomInit();
    toolIdSys.gameInit(0x200);
    for (i = ID_DATA_NUM - 1; i >= 0; i--) {
        idData[i].be_flag = 0xFF;
    }
}

// Tool end: frees the IDSystem, back to 512 wide, restores the flags, ends the task.
static void toolIdQuit(IdTool* w)
{
    toolIdSys.free();
    ScreenReSize(0x200, 0x1C0);
    IdDebugFreeBuffer();
    DbgFlagOff(pG, DBG_TEST_MODE);
    DbgFlagOff(pG, DBG_DBG_CAM);
    DbgFlagOn(pG, DBG_PROC_BAR);
    pG->System_flg |= 0x800;
    DbgFlagOff(pG, DBG_CINESCO_OFF);
    DbgFlagOff(pG, DBG_ID_TOOL);
    pG->Camera = w->camSave;
    bio4_GXSetCopyClear(g_sysBgColor, 0xFFFFFF);
    ToolWorkPop(0);
    TaskSignal(0);
    Cckpt.roomInit();
    TaskExit();
}

// Interface design tool entry (debug menu 20): every frame the safe zone, toolIdFunc[mode] (0 the
// page / file preselect, 1 the menu, 2 the editor), then unless paused (X toggles) the edited
// elements are encoded (toolIdDataEncode) into the tool IDSystem and drawn as the game would.
void ToolInterfaceDesign()
{
    static IdTool idToolWork;
    static IdTool* pIdTool = &idToolWork;
    JOY* joy = &Joy[0];

    toolIdInit(pIdTool);
    while (1) {
        CameraMove();
        toolIdDrawSafeZone(pIdTool);
        pIdTool->drawSafe = 1;
        toolIdFunc[pIdTool->mode](pIdTool);
        if (joy->trg & 0x400) {
            pIdTool->pause = pIdTool->pause == 0;
        }
        if (pIdTool->pause == 0) {
            toolIdSys.roomInit();
            toolIdDataEncode(pIdBuf0, pIdTool);
            toolIdSys.set(pIdBuf0, 0xFF, IDC_TOOL, 0xC, 6, 0);
            toolIdSys.stop();
        } else {
            pIdTool->drawSafe = 0;
        }
        toolIdSys.move();
        toolIdSys.trans();
        if (Joy[0].trg != 0) {
            pIdTool->cnt = 0x10;
        } else {
            pIdTool->cnt++;
        }
        TaskSleep(1);
        // COMPILER-DIFF: dead test (haifa MAX_RGN_BLOCKS: the body has 9 blocks and forms an interblock
        // region that hoists the arms' li/addi above the branches; the target keeps them in their blocks,
        // so its loop had >= 11 blocks. The two compares are deleted by jump2, the set by flow.)
        {
            IdTool* dead;
            if (pIdTool->cnt == 0 && pIdTool->mode == 0) {
                dead = 0;
            }
        }
    }
}

// Draws the TV safe zone frame.
void toolIdDrawSafeZone(IdTool* w)
{
    if (w->drawSafe == 0) {
        return;
    }
    Vec a[4] = { { 0.8f, 0.8f, 0.0f }, { -0.8f, 0.8f, 0.0f }, { -0.8f, -0.8f, 0.0f }, { 0.8f, -0.8f, 0.0f } };
    Vec b[4] = { { 0.9f, 0.9f, 0.0f }, { -0.9f, 0.9f, 0.0f }, { -0.9f, -0.9f, 0.0f }, { 0.9f, -0.9f, 0.0f } };
    f32 sx, sy;
    int i;

    sx = (f32) w->scrW * 0.5f;
    sy = (f32) w->scrH * 0.5f;
    for (i = 0; i < 4; i++) {
        a[i].x *= sx;
        a[i].y *= sy;
        b[i].x *= sx;
        b[i].y *= sy;
    }
    for (i = 0; i < 4; i++) {
        Draw_line3d(&a[i], &a[(i + 1) % 4], 0xFF404040, 0);
        Draw_line3d(&b[i], &b[(i + 1) % 4], 0xFF202020, 0);
    }
}

// Places the edit sub menu (menuX / menuY) beside the element list.
void toolIdSubMenuPosition(IdTool* w)
{
    JOY* joy = &Joy[0];
    if (joy->substickX != 0) {
        w->menuX += (int) ((f32) joy->substickX / 10.0f);
    }
    if (joy->substickY != 0) {
        w->menuY -= (int) ((f32) joy->substickY / 10.0f);
    }
}

static const char* pageName[2] = { "Page 1", "Page 2" };

// Mode 0, PAGE select: Page 1 / Page 2 decide which base .eff sets (ckpt / share / tool2 and the
// sub screen's own) are read from x:\soft/Room/SubScreen/<lang>/ into the buffers; then the menu.
static void toolIdPrev(IdTool* w)
{
    JOY* joy = &Joy[0];
    char path[0x100];
    int i;

    if (joy->rep & 0x80008) {
        w->prevCur--;
    }
    if (joy->rep & 0x40004) {
        w->prevCur++;
    }
    w->prevCur = w->prevCur < 0 ? 2 : (w->prevCur > 2 ? 0 : w->prevCur);
    if (joy->trg & 0x200) {
        toolIdQuit(w);
        return;
    }
    if (joy->trg & 0x100) {
        IdTexRoomInit();
        switch (w->prevCur) {
        case 0:
            sprintf(path, "x:\\soft/Room/SubScreen/%s/ckpt.eff", langName[w->lang]);
            if (HDRead(path, pIdBuf1) > 0x100000) {
                pLog->err(0, 0, "toolIdInit(): Eff(%s) file is too large.", path);
            } else if (w->type != 0) {
                IdTexDataLoad(pIdBuf1, TEX_OWNER_ID_COCKPIT);
            }
            sprintf(path, "x:\\soft/Room/SubScreen/%s/share.eff", langName[w->lang]);
            if (HDRead(path, pIdBuf2) > 0x300000) {
                pLog->err(0, 0, "toolIdInit(): Eff(%s) file is too large.", path);
            } else {
                int t = w->type;
                if (t > 1 || w->type < 0) {
                    IdTexDataLoad(pIdBuf2, TEX_OWNER_ID_SHARE);
                }
            }
            sprintf(path, "x:\\soft/Room/SubScreen/%s/%s.eff", langName[w->lang], subScreenName[w->type]);
            if (HDRead(path, pIdBuf3) > 0x900000) {
                pLog->err(0, 0, "toolIdInit(): Eff(%s) file is too large.", path);
            } else if (w->type != 1 && w->type != 5) {
                IdTexDataLoad(pIdBuf3, TEX_OWNER_ID_TOOL);
            }
            w->reload = 0;
            break;
        case 1:
            sprintf(path, "x:\\soft/Room/SubScreen/%s/tool2.eff", langName[w->lang]);
            if (HDRead(path, pIdBuf3) > 0x900000) {
                pLog->err(0, 0, "toolIdInit(): Eff(%s) file is too large.", path);
            } else {
                IdTexDataLoad(pIdBuf3, TEX_OWNER_ID_TOOL);
            }
            w->reload = 0;
            break;
        }
        w->mode = 1;
    }
    eprintf(0xF0, 0x8C, 5, 0, "PAGE");
    if (w->cnt & 0x18) {
        eprintf(0xE8, w->prevCur * 0xE + 0x9A, 0x16, 0, ">");
    }
    for (i = 0; i < 2; i++) {
        eprintf(0xF0, 0x9A + i * 0xE, (w->prevCur == i) ? 4 : 0, 0, "%s", pageName[i]);
    }
}

static const char* mainMenuName[5] = { "Edit", "Load", "Save", "Opt.", "Exit" };
static IdToolFunc toolIdMainFunc[5] = { toolIdEdit, toolIdFile, toolIdFile, toolIdOption, toolIdQuit };

// Mode 1, MENU: Edit / Load / Save / Opt. / Exit (editSel -> toolIdMainFunc).
static void toolIdMenu(IdTool* w)
{
    JOY* joy = &Joy[0];
    int i;

    if (joy->rep & 0x80008) {
        w->menuCur--;
    }
    if (joy->rep & 0x40004) {
        w->menuCur++;
    }
    w->menuCur = w->menuCur < 0 ? 4 : (w->menuCur > 4 ? 0 : w->menuCur);
    if (joy->trg & 0x100) {
        w->mode = 2;
    }
    if (joy->trg & 0x200) {
        w->menuCur = 4;
    }
    eprintf(0xF0, 0x8C, 5, 0, "MENU");
    if (w->cnt & 0x18) {
        eprintf(0xE8, w->menuCur * 0xE + 0x9A, 0x16, 0, ">");
    }
    for (i = 0; i < 5; i++) {
        eprintf(0xF0, 0x9A + i * 0xE, (w->menuCur == i) ? 4 : 0, 0, "%s", mainMenuName[i]);
    }
}

// Mode 2: runs the chosen main function (edit, file load / save, option, quit).
static void toolIdMain(IdTool* w)
{
    toolIdMainFunc[w->menuCur](w);
}

// Default element: 64 x 64 at the origin, white, no texture / mark / parent, straight paths and
// constant curves.
void toolIdDataInit(ID_DATA* d)
{
    d->texId = 0xFF;
    d->mark = 0xFF;
    d->pos.x = 0.0f;
    d->pos.y = 0.0f;
    d->pos.z = 0.0f;
    d->vtxType = 0;
    d->sizeX = 0.0f;
    d->sizeY = 0.0f;
    d->be_flag = 0xD;
    d->kind = 0;
    d->xF4 = 0;
    d->parentNo = 0xFF;
    d->col0[0] = d->col0[1] = d->col0[2] = d->col0[3] = 0xFF;
    d->col1[0] = d->col1[1] = d->col1[2] = d->col1[3] = 0;
    memclr_asm(&d->path0, sizeof(d->path0));
    memclr_asm(&d->path1, sizeof(d->path1));
    memclr_asm(&d->curve0, sizeof(d->curve0));
    memclr_asm(&d->curve1, sizeof(d->curve1));
    memclr_asm(&d->curve2, sizeof(d->curve2));
    memclr_asm(&d->curve3, sizeof(d->curve3));
}

// The live element with unit number `unitNo`, 0 when none.
ID_DATA* toolIdGetPtrU(u8 unitNo)
{
    ID_DATA* d = idData;
    int i;

    for (i = 0; i < ID_DATA_NUM; i++, d++) {
        if (d->be_flag != 0xFF && unitNo == d->unitNo) {
            return d;
        }
    }
    return 0;
}

// The live element at slot `no` of parent `parentNo` (0xFF = root), 0 when none.
ID_DATA* toolIdGetPtrPR(u8 parentNo, u8 no)
{
    ID_DATA* d = idData;
    int i;

    for (i = 0; i < ID_DATA_NUM; i++, d++) {
        if (d->be_flag != 0xFF && parentNo == d->parentNo && no == d->no) {
            return d;
        }
    }
    return 0;
}

// Frees element `d` and, recursively, every child whose parent it is.
void toolIdPush(ID_DATA* d, IdTool* w)
{
    int i;

    if (d->kind == 1) {
        for (i = 0; i < ID_DATA_NUM; i++) {
            if (idData[i].be_flag != 0xFF && d->unitNo == idData[i].parentNo) {
                toolIdPush(&idData[i], w);
            }
        }
    }
    w->markUse[d->mark] = 0;
    toolIdDataInit(d);
    d->be_flag = 0xFF;
}

// Allocates a free element slot (unitNo = its index), 0 when full.
ID_DATA* toolIdPull()
{
    ID_DATA* d = idData;
    int i;

    for (i = 0; i < ID_DATA_NUM; i++, d++) {
        if (d->be_flag == 0xFF) {
            toolIdDataInit(d);
            d->unitNo = i;
            return d;
        }
    }
    return 0;
}

// The element editor. editMode 0: the list of the current parent's slots (up/down, B up one level,
// A into a group, X a new element, Y the unit menu (Copy / Cut / Paste / Grp.), the column keys open
// the property editors); editMode 1..8 run idEdit* (unit, no, id, pos, size, color, rot, trans,
// mark) until they return 0; 2 the unit menu. Focus / selection highlight through toolIdFocusOn.
static void toolIdEdit(IdTool* w)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int r;

    w->focus = 1;
    if (w->pause != 0) {
        w->focus = 0;
    }
    if (w->parentNo != 0xFF) {
        toolIdCalcInitMatrix(toolIdGetPtrU(w->parentNo), w->mat);
    } else {
        PSMTXIdentity(w->mat);
    }
    toolIdFocusOn(w, d);
    switch (w->editMode) {
    case 0:
        if (joy->trg & 0x200) {
            if (toolIdCountSelected() != 0) {
                break;
            }
            if (w->level == 0) {
                w->mode = 1;
                break;
            }
            w->level--;
            w->no = toolIdGetPtrU(w->parentNo)->no;
            w->parentNo = toolIdGetPtrU(w->parentNo)->parentNo;
            break;
        }
        {
            u8 old = w->no;
            int n = old;

            if (joy->rep & 0x80008) {
                n--;
            }
            if (joy->rep & 0x40004) {
                n++;
            }
            n = n < 0 ? 0 : (n > 0xFF ? 0xFF : n);
            w->no = n;
            if (old != (u8) n) {
                toolIdFocusReset(w, d);
            }
        }
        if (joy->rep & 0x10001) {
            w->editSel--;
        }
        if (joy->rep & 0x20002) {
            w->editSel++;
        }
        w->editSel = w->editSel < 0 ? 0 : (w->editSel > 7 ? 7 : w->editSel);
        if (d == 0) {
            if (joy->trg & 0x100) {
                d = toolIdPull();
                toolIdDataInit(d);
                d->no = w->no;
                d->parentNo = w->parentNo;
                d->level = w->level;
                break;
            }
        } else if (w->editSel != 0) {
            if (toolIdCountSelected() != 0 && !(d->be_flag & 0x80)) {
                break;
            }
            if (joy->trg & 0x100) {
                w->editMode = 1;
                w->editStep = 0;
                break;
            }
        } else {
            idEditNo(w, w->menuX, w->menuY);
        }
        if (joy->trg & 0x800) {
            w->editMode = 2;
            w->subCur = 0;
            w->editStep = 0;
        }
        break;
    case 1:
        toolIdSubMenuPosition(w);
        // editSel 2 is the size editor and 3 the position editor (editDispFunc order), written as
        // case 3 then case 2 (arm layout); each arm repeats the `if (r == 0) editMode = r; focus = 0`
        // tail (jump2 cross-jumps the six identical `mr. r3,r3` tails; a shared `goto sub` block
        // gets a plain `cmpwi r3,0`)
        switch (w->editSel) {
        case 1:
            r = idEditId(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            break;
        case 3:
            r = idEditPos(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            w->focus = 0;
            break;
        case 2:
            r = idEditSize(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            w->focus = 0;
            break;
        case 4:
            r = idEditColor(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            w->focus = 0;
            break;
        case 5:
            r = idEditRot(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            w->focus = 0;
            break;
        case 6:
            r = idEditTrans(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            w->focus = 0;
            break;
        case 7:
            r = idEditMark(w, w->menuX, w->menuY);
            if (r == 0) {
                w->editMode = r;
            }
            w->focus = 0;
            break;
        default:
            w->editMode = 0;
            break;
        }
        break;
    case 2:
        r = idEditUnit(w, w->menuX, w->menuY);
        if (r == 0) {
            w->editMode = r;
            w->editStep = r;
        }
        break;
    }
    toolIdEditDisp(w);
}

static const char* unitMenuName[4] = { "Copy", "Cut", "Paste", "Grp." };

// Unit menu: Copy / Cut (the selected elements into the clipboard) / Paste (after the cursor slot)
// / Grp. (group the selection into a new group element); 0 when done.
int idEditUnit(IdTool* w, int x, int y)
{
    JOY* joy = &Joy[0];
    int ret = 1;
    ID_DATA* d;
    int i;
    int yy;

    switch (w->editStep) {
    case 0:
        if (joy->trg & 0x200) {
            ret = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->subCur--;
        }
        if (joy->rep & 0x40004) {
            w->subCur++;
        }
        w->subCur = w->subCur < 0 ? 0 : (w->subCur > 3 ? 3 : w->subCur);
        if (joy->trg & 0x100) {
            w->editStep = 1;
        }
        break;
    case 1:
        switch (w->subCur) {
        case 0:
            toolIdClipboardClear();
            if (toolIdCountSelected() != 0) {
                toolIdCopy(0, 0);
                toolIdSelectClear();
            } else {
                d = toolIdGetPtrPR(w->parentNo, w->no);
                if (d != 0) {
                    toolIdCopy(d, 0);
                }
            }
            goto count;
        case 1:
            toolIdClipboardClear();
            if (toolIdCountSelected() != 0) {
                toolIdCopy(0, 0);
                toolIdDelete(0, w);
                toolIdSelectClear();
            } else {
                d = toolIdGetPtrPR(w->parentNo, w->no);
                if (d != 0) {
                    toolIdCopy(d, 0);
                    toolIdDelete(d, w);
                }
            }
        count:
            w->useCnt = toolIdClipboardCount(0xFF, 1);
            ret = 0;
            w->empCnt = toolIdClipboardCount(0xFF, 2);
            break;
        case 2: {
            u8 parentNo = w->parentNo;
            u8 no = w->no;
            int n = toolIdClipboardCount(0, 0);

            ret = 0;
            toolIdSpace(parentNo, no, n);
            toolIdPaste(w->parentNo, w->no);
            toolIdSelectClear();
            break;
        }
        case 3:
            w->grpSw = 0;
            w->editStep++;
            break;
        }
        break;
    case 2:
        if (joy->trg & 0x200) {
            w->editStep = 0;
        } else if (joy->trg & 0x100) {
            if (w->grpSw != 0) {
                toolIdSpace(w->parentNo, w->no, 1);
                d = toolIdPull();
                toolIdDataInit(d);
                {
                    // COMPILER-DIFF: candidate (local-alloc qty order: with exactly 3 block-local
                    // qtys the `case 3` partial sort always allocates q0 (the `no` temp) first -> r0;
                    // a 4th codeless qty makes it a qsort by priority: level r0, no/parentNo r9)
                    int t;
                    asm("" : "=r"(t));
                    asm volatile("" : : "r"(t));
                }
                d->no = w->no;
                d->level = w->level;
                d->parentNo = w->parentNo;
                toolIdGroup(d);
            }
            ret = 0;
        } else {
            if (joy->rep & 0x10001) {
                w->grpSw = 1;
            }
            if (joy->rep & 0x20002) {
                // = 0, not 2: the target's `stb r10` stores the `trg & 0x100` pseudo cse knows to be
                // zero on this path (record_jump_equiv on the followed `beq`), not the editStep value
                w->grpSw = 0;
            }
        }
        break;
    }
    eprintf(x, y, 5, 0, "NO-MENU");
    y += 0xE;
    for (i = 0; i < 4; i++) {
        // giv form (`yy = y; ... yy += 0xE` gets cse's copy swap `yy = y + 14; y = yy`)
        yy = y + i * 0xE;
        eprintf(x, yy, (w->subCur == i) ? 4 : 0, 0, "%s", unitMenuName[i]);
        if (w->subCur == i && (w->cnt & 0x18)) {
            eprintf(x - 8, yy, 0x16, 0, ">");
        }
        if (i == w->subCur && i == 3 && w->editStep == 2) {
            if (w->grpSw != 0) {
                eprintf(x + 0x50, y + 0x38, 0, 0, "YES/---");
            } else {
                eprintf(x + 0x50, y + 0x38, 0, 0, "---/NO-");
            }
        }
        // as idEditMark/idEditId: 5 more insns at loop.c time (pass 2: 67 -> 72 > threshold 71 keeps
        // the ">" and "YES/---" highs inside the loop, as the target)
        d = (ID_DATA*) 0; // COMPILER-DIFF: 3 (loop.c insn_count, dead sets)
        d = (ID_DATA*) 4;
        d = (ID_DATA*) 0;
        d = (ID_DATA*) 4;
        d = (ID_DATA*) 0;
    }
    return ret;
}

static int selLimit0 = 15;
static int selLimit1 = 15;

// No column: A toggles the element on/off (be_flag bit 0), a held A starts a multi-select over the
// following slots (be_flag 0x80); 0 when done.
int idEditNo(IdTool* w, int x, int y)
{
    static int selNo;
    static int selCnt;
    static int selCnt2;
    static int selOver;
    JOY* joy = &Joy[0];
    ID_DATA* d;
    int i;

    switch (w->editStep) {
    case 0:
        if (joy->trg & 0x100) {
            d = toolIdGetPtrPR(w->parentNo, w->no);
            if (d != 0) {
                if (!(d->be_flag & 1)) {
                    d->be_flag |= 1;
                    break;
                }
                if (d->be_flag & 0x80) {
                    d->be_flag &= ~0x80;
                } else {
                    d->be_flag |= 0x80;
                }
                selNo = w->no;
                selCnt = 0;
                w->editStep = 1;
                selCnt2 = 0;
                selOver = 0;
            }
        } else if (joy->trg & 0x10) {
            d = toolIdGetPtrPR(w->parentNo, w->no);
            if (d != 0) {
                if (d->be_flag & 1) {
                    d->be_flag &= ~1;
                    d->be_flag &= ~0x80;
                } else {
                    d->be_flag |= 1;
                }
                selNo = w->no;
                selCnt = 0;
                w->editStep = 3;
                selCnt2 = 0;
                selOver = 0;
            }
        }
        break;
    case 1:
        w->editStep++;
    case 2:
        if (joy->on & 0x100) {
            for (i = 0; i <= 0xFF; i++) {
                d = toolIdGetPtrPR(w->parentNo, (u8) i);
                if (d == 0) {
                    continue;
                }
                if (d->be_flag & 0x40) {
                    d->be_flag &= ~0xC0;
                }
                if ((d->be_flag & 0x81) == 1) {
                    if (w->no >= selNo) {
                        if (i > selNo && i <= w->no) {
                            d->be_flag |= 0xC0;
                        }
                    } else {
                        if (i < selNo && i >= w->no) {
                            d->be_flag |= 0xC0;
                        }
                    }
                }
            }
            selCnt2++;
            if (selCnt++ > selLimit0) {
                selOver = 1;
            }
        } else {
            for (i = 0; i <= 0xFF; i++) {
                d = toolIdGetPtrPR(w->parentNo, (u8) i);
                if (d == 0) {
                    continue;
                }
                if (selOver == 0) {
                    if (d->be_flag & 0x40) {
                        if (i != selNo) {
                            d->be_flag &= ~0x80;
                        }
                        d->be_flag &= ~0x40;
                    }
                } else {
                    if (d->be_flag & 0x40) {
                        d->be_flag &= ~0x40;
                    }
                }
            }
            w->editStep = 0;
        }
        break;
    case 3:
        if (selCnt++ > 5) {
            w->editStep++;
        }
        break;
    case 4:
        if (joy->on & 0x10) {
            for (i = 0; i <= 0xFF; i++) {
                d = toolIdGetPtrPR(w->parentNo, (u8) i);
                if (d == 0) {
                    continue;
                }
                if (d->be_flag & 0x20) {
                    d->be_flag = (d->be_flag & ~0x20) | 1;
                }
                if (w->no >= selNo) {
                    if (i >= selNo && i <= w->no) {
                        d->be_flag = (d->be_flag & ~1) | 0x20;
                    }
                } else {
                    if (i <= selNo && i >= w->no) {
                        d->be_flag = (d->be_flag & ~1) | 0x20;
                    }
                }
            }
            selCnt2++;
            if (selCnt++ > selLimit1) {
                selOver = 1;
            }
        } else {
            for (i = 0; i <= 0xFF; i++) {
                d = toolIdGetPtrPR(w->parentNo, (u8) i);
                if (d == 0) {
                    continue;
                }
                if (!(d->be_flag & 1)) {
                    d->be_flag &= ~0x80;
                }
            }
            w->editStep = 0;
        }
        break;
    }
    return 0;
}

static const char* idMenuName[1] = { "Tex:" };

// Id column: Tex: picks the element's texture id (left/right, with a preview of the TPL image and
// its size; groups edit their selected children); 0 when done.
int idEditId(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int ret = 1;
    int i;
    int yy;

    switch (w->editStep) {
    case 0:
        if (d->kind == 1) {
            int n = toolIdCountSelected();

            if (n != 0) {
                return 0;
            }
            w->parentNo = d->unitNo;
            w->level++;
            toolIdFocusReset(w, d);
            w->listTop = n;
            w->no = n;
            return 0;
        }
        w->editStep++;
    case 1: {
        int old = d->texId;
        int step = 1;

        if (joy->on & 0x100) {
            step = 10;
        }
        if (joy->rep & 0x10001) {
            d->texId -= step;
        }
        if (joy->rep & 0x20002) {
            d->texId += step;
        }
        if (old != d->texId && !(d->flags10A & 0x80)) {
            struct { TexAnm* p; } anm;

            if (IdGetAnmAddr(d->texId, &anm.p) == 1) {
                // ONE f32 variable for both dimensions: `s` then has two deaths (the sizeX store and
                // the `+ 0.5f`), so it is a global pseudo (f0) and the rounding temp ties to the 0.5
                // constant (`fadds f13,f0,f13`); the `* 480` and `/ 448` are sets of s (no temp tied
                // to the 480 constant)
                f32 s;

                s = (f32) ((TexAnmSize*) anm.p)->w;
                d->sizeX = s;
                s = (f32) (int) ((TexAnmSize*) anm.p)->h;
                s = s * 480.0f;
                s = s / 448.0f;
                d->sizeY = (f32) (int) (s + 0.5f);
            }
        }
        if (joy->trg & 0x200) {
            ret = 0;
        }
        // +1 insn while w is live: w (19 refs / 175) is allocated ahead of i (15 / 104) by 4342 vs
        // 4326 global.c priority units; the original has i (r30) before w (r29)
        asm(""); // COMPILER-DIFF: anchor
        break;
    }
    }
    eprintf(x, y, 5, 0, "ID-MENU");
    y += 0xE;
    for (i = 0; i <= 0; i++) {
        int col;

        // giv form: the `mr r31,r25` init is emitted by strength_reduce after the pass-1 hoists
        // ("%s", ">") and before the pass-2 hoist ("%02X"); `yy = y; ... yy += 0xE` puts the copy
        // first and cse swaps it into `yy = y + 14; y = yy`
        yy = y + i * 0xE;
        eprintf(x, yy, (w->subCur == i) ? 4 : 0, 0, "%s", idMenuName[i]);
        if (w->subCur == i && (w->cnt & 0x18)) {
            eprintf(x - 8, yy, 0x16, 0, ">");
        }
        col = 7;
        if (w->subCur == i) {
            col = 0;
        }
        if (i == 0) {
            eprintf(x + 0x20, y, col, 0, "%02X", d->texId);
        }
        // as idEditMark: the original loop has 5 more insns at loop.c time (61 -> 66 > threshold 65
        // keeps the "%02X" high for pass 2)
        col = 7; // COMPILER-DIFF: 3 (loop.c pass-1 insn_count, dead sets)
        col = 6;
        col = 7;
        col = 6;
        col = 7;
    }
    return ret;
}

static const char* posMenuName[6] = { "Base  :", "Path  :", "Speed :", "Loop  :", "Grid  :", "Random:" };
static const char* anchorName[5] = { "CT", "LU", "RU", "RD", "LD" };
static const char* onOffName[2] = { "ON", "OFF" };

// Screen-space grid / guide lines of the position editor (written out in the original: its pool loads
// float above the preceding stores, which an inlined body never does)
#define ID_DRAW_GUIDE(w, d) \
{ \
    Vec p; \
    Vec a; \
    Vec b; \
    int i; \
    int sx, sy; \
    int dx, dy; \
 \
    p = *pos; \
    if (w->gridLv > 3) { \
        Vec g0; \
        Vec g1; \
 \
        memclr_asm(&g0, sizeof(Vec)); \
        memclr_asm(&g1, sizeof(Vec)); \
        g0.y = 240.0f; \
        g1.y = -240.0f; \
        Draw_line3d(&g0, &g1, 0x40404040, 0); \
        for (i = 1; (f32) i * w->grid.x < 320.0f; i++) { \
            g1.x = g0.x = (f32) i * w->grid.x; \
            Draw_line3d(&g0, &g1, 0x40404040, 0); \
        } \
        for (i = -1; (f32) i * w->grid.x > -320.0f; i--) { \
            g1.x = g0.x = (f32) i * w->grid.x; \
            Draw_line3d(&g0, &g1, 0x40404040, 0); \
        } \
        memclr_asm(&g0, sizeof(Vec)); \
        memclr_asm(&g1, sizeof(Vec)); \
        g0.x = 320.0f; \
        g1.x = -320.0f; \
        Draw_line3d(&g0, &g1, 0x40404040, 0); \
        for (i = 1; (f32) i * w->grid.y < 240.0f; i++) { \
            g1.y = g0.y = (f32) i * w->grid.y; \
            Draw_line3d(&g0, &g1, 0x40404040, 0); \
        } \
        for (i = -1; (f32) i * w->grid.y > -240.0f; i--) { \
            g1.y = g0.y = (f32) i * w->grid.y; \
            Draw_line3d(&g0, &g1, 0x40404040, 0); \
        } \
    } \
    if (w->parentNo != 0xFF) { \
        Vec t; \
 \
        t.x = w->mat[0][3]; \
        t.y = w->mat[1][3]; \
        t.z = w->mat[2][3]; \
        PSVECAdd(&p, &t, &p); \
        a = t; \
        b = t; \
        a.x = -320.0f; \
        b.x = 320.0f; \
        Draw_line3d(&a, &b, 0xFF808040, 0); \
        a = t; \
        b = t; \
        a.y = -240.0f; \
        b.y = 240.0f; \
        Draw_line3d(&a, &b, 0xFF808040, 0); \
    } \
    a = p; \
    b = p; \
    a.x = -320.0f; \
    b.x = 320.0f; \
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0); \
    a = p; \
    b = p; \
    a.y = -240.0f; \
    b.y = 240.0f; \
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0); \
    sx = (int) (p.x * 256.0f / 320.0f + 256.0f); \
    sy = (int) (224.0f - p.y * 224.0f / 240.0f); \
    dx = (sx > 0x198) ? -0x68 : 8; \
    dy = (sy > 0x1A4) ? -0x1C : 0xE; \
    eprintf(sx + dx, sy + dy, 0, 0, "(%3.0f, %3.0f)", pos->x, pos->y); \
}

// Pos column: Base (stick moves the position, grid-locked by gridLv), Path (the B-spline path
// editor on path0 / path1), Speed, Loop, Grid level, Random (DbRandom on the jitter values);
// 0 when done.
int idEditPos(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy;
    int ret = 1;
    Vec* pos;
    int i, j;
    int yy;

    w->pPath = &idPath;
    idPath.path = (FuncPathData*) &d->path0;
    w->pRandom = &idRandom;
    idRandom.pVal = (s16*) d->x17C8;
    joy = &Joy[0];
    pos = &d->pos;
    w->gridLv = d->vtxType >> 4;
    switch (w->editStep) {
    case 0: {
        f32 step;

        if (joy->trg & 0x10) {
            w->editStep = 1;
            break;
        }
        if (joy->trg & 0x200) {
            ret = 0;
        }
        step = (joy->on & 0xF0000) ? 5.0f : 1.0f;
        if (joy->on & 0x100) {
            step *= 10.0f;
        }
        if ((joy->rep & 1) || (joy->on & 0x10000)) {
            pos->x -= step;
        }
        if ((joy->rep & 2) || (joy->on & 0x20000)) {
            pos->x += step;
        }
        if ((joy->rep & 8) || (joy->on & 0x80000)) {
            pos->y += step;
        }
        if ((joy->rep & 4) || (joy->on & 0x40000)) {
            pos->y -= step;
        }
        if (!(joy->on & 0xF000F)) {
            toolIdGridLock(&w->grid, pos, pos);
        }
        if (w->pPath->path->n != 0) {
            Vec dif;

            PSVECSubtract(&d->pos, &w->pPath->path->pos[0], &dif);
            for (i = 0; i < w->pPath->path->n; i++) {
                PSVECAdd(&w->pPath->path->pos[i], &dif, &w->pPath->path->pos[i]);
            }
        }
        break;
    }
    case 1:
        if (joy->trg & 0x200) {
            w->editStep = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->subCur--;
        }
        if (joy->rep & 0x40004) {
            w->subCur++;
        }
        w->subCur = w->subCur < 0 ? 0 : (w->subCur > 5 ? 5 : w->subCur);
        switch (w->subCur) {
        case 0: {
            u8 vt;
            int a;

            toolIdCalcVertex(d);
            for (i = 0; i < 4; i++) {
                PSVECAdd(&d->vtx[i], &d->pos, &d->vtx[i]);
            }
            {
                int t = d->vtxType;

                a = t & 0xF;
            }
            vt = d->vtxType;
            if (joy->rep & 0x10001) {
                a--;
            }
            if (joy->rep & 0x20002) {
                a++;
            }
            a = a < 0 ? 4 : (a > 4 ? 0 : a);
            d->vtxType = (vt & 0xF0) | (a & 0xF);
            switch (d->vtxType & 0xF) {
            case 0:
                PSVECAdd(&d->vtx[0], &d->vtx[2], &d->pos);
                PSVECScale(&d->pos, &d->pos, 0.5f);
                break;
            case 1:
                d->pos = d->vtx[0];
                break;
            case 2:
                d->pos = d->vtx[1];
                break;
            case 3:
                d->pos = d->vtx[2];
                break;
            case 4:
                d->pos = d->vtx[3];
                break;
            }
            if (joy->trg & 0x100) {
                w->editStep--;
            }
            break;
        }
        case 1:
            if (joy->trg & 0x100) {
                w->subStep = 0;
                w->editStep++;
            }
            break;
        case 2:
            if (joy->trg & 0x100) {
                w->subStep = 0;
                w->editStep++;
            }
            break;
        case 3:
            if (joy->rep & 0x10001) {
                d->loop_flag |= 1;
            }
            if (joy->rep & 0x20002) {
                d->loop_flag &= ~1;
            }
            if (joy->trg & 0x100) {
                w->editStep--;
            }
            break;
        case 4:
            if (joy->trg & 0x10001) {
                w->gridLv--;
            }
            if (joy->trg & 0x20002) {
                w->gridLv++;
            }
            w->gridLv = w->gridLv < 0 ? 0 : (w->gridLv > 7 ? 7 : w->gridLv);
            w->grid.x = IPOW(2.0f, w->gridLv);
            w->grid.y = IPOW(2.0f, w->gridLv);
            d->vtxType &= 0xF;
            d->vtxType |= w->gridLv << 4;
            if (w->gridLv != 0) {
                w->pPath->gridLock = 1;
                w->pPath->grid.x = w->grid.x;
                w->pPath->grid.y = w->grid.y;
            } else {
                w->pPath->gridLock = 0;
            }
            break;
        case 5:
            if (joy->trg & 0x100) {
                w->subStep = 0;
                w->editStep++;
            }
            break;
        }
        eprintf(x, y, 5, 0, "POS-MENU");
        y += 0xE;
        for (i = 0; i <= 5; i++) {
            yy = y + i * 0xE;
            eprintf(x, yy, (w->subCur == i) ? 4 : 0, 0, "%s", posMenuName[i]);
            if (w->subCur == i && (w->cnt & 0x18)) {
                eprintf(x - 8, yy, 0x16, 0, ">");
            }
            switch (i) {
            case 0:
                for (j = 0; j <= 4; j++) {
                    int col = 7;

                    if (j == (d->vtxType & 0xF)) {
                        col = 0;
                    }

                    eprintf(x + 0x40 + j * 0x18, y + i * 0xE, col, 0, "%s", anchorName[j]);
                }
                break;
            case 3:
                for (j = 0; j <= 1; j++) {
                    int col;
                    u32 ofs = j * 4;

                    col = 7;
                    if ((j == 0) == (d->loop_flag & 1)) {
                        col = 0;
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32) onOffName));
                }
                break;
            case 4:
                for (j = 0; j <= 7; j++) {
                    register int col asm("r5"); // COMPILER-DIFF: #17 (col r5 vs r30, local ext pref)

                    col = 7;
                    if (j == w->gridLv) {
                        col = 0;
                    }
                    eprintf(x + 0x40 + j * 0x18, y + i * 0xE, (u8) col, 0, "%02d", (int) IPOW(2.0f, j));
                }
                break;
            }
        }
        break;
    case 2:
        switch (w->subCur) {
        case 1:
            switch (w->subStep) {
            case 0:
                if (w->pPath->path->n == 0) {
                    w->pPath->path->n = 1;
                    w->pPath->path->pos[0] = d->pos;
                    w->pPath->routine = 0;
                    w->pPath->step = 1;
                    w->pPath->path->k = 1;
                    w->pPath->path->n = 2;
                }
                if (w->parentNo != 0xFF) {
                    Vec t;
                    Vec* pt = &t;

                    pt->x = w->mat[0][3];
                    pt->y = w->mat[1][3];
                    pt->z = w->mat[2][3];
                    w->pPath->ofs.x = t.x;
                    w->pPath->ofs.y = t.y;
                    w->pPath->ofs.z = 0.0f;
                } else {
                    w->pPath->ofs.x = 0.0f;
                    w->pPath->ofs.y = 0.0f;
                    w->pPath->ofs.z = 0.0f;
                }
                w->subStep++;
                break;
            case 1:
                if (DbPath(w->pPath, w->menuX, w->menuY) == 0) {
                    w->editStep = 1;
                }
                break;
            }
            break;
        case 2:
            switch (w->subStep) {
            case 0:
                if (w->pPath->path->n <= 1) {
                    w->editStep = 1;
                    break;
                }
                w->pSctrl = &idSctrl0;
                idSctrl0.curve = (Hermite1*) &d->curve0;
                SctrlSetAxisLabel(w->pSctrl, "Frame", "Param");
                w->pSctrl->gridX = 15.0f;
                w->pSctrl->gridY = 0.5f;
                w->pSctrl->grid.x = 1.0f;
                w->pSctrl->grid.y = 0.01f;
                w->pSctrl->flags = 3;
                if (w->pSctrl->curve->num <= 1) {
                    f32 xr = 90.0f;
                    f32 n = (f32) (w->pPath->path->n - 1);

                    SctrlInitAxisRange(w->pSctrl, xr * 1.2f, xr * -0.2f, n * 1.2f, n * -0.2f);
                    SctrlInitCursor(w->pSctrl, 0.0f, 0.0f);
                } else {
                    SctrlAdjustAxisRange(w->pSctrl);
                    SctrlInitCursor(w->pSctrl, w->pSctrl->curve->key[0].t, w->pSctrl->curve->key[0].v);
                }
                w->subStep++;
                break;
            case 1:
                if (DbSctrl(w->pSctrl, w->menuX, w->menuY) == 0) {
                    w->editStep = 1;
                }
                w->drawSafe = 0;
                break;
            }
            break;
        case 5:
            switch (w->subStep) {
            case 0:
                w->subStep++;
                break;
            case 1:
                if (DbRandom(w->pRandom, w->menuX, w->menuY) == 0) {
                    w->editStep = 1;
                }
                break;
            }
            break;
        }
        break;
    }
    if (w->editStep != 2) {
        ID_DRAW_GUIDE(w, d);
    }
    return ret;
}

static const char* sizeMenuName[6] = { "Size  :", "Anima :", "Axis  :", "Loop  :", "Flag  :", "Random:" };
static const char* texFixName[2] = { "TEX", "FIX" };
static const char* onOffName2[2] = { "ON", "OFF" };
static const char* axisName[3] = { "X-Y", "-X-", "-Y-" };

// Size column: Size (stick, TEX = the texture's own size / FIX), Anima (the size curve in the
// S-curve editor), Axis (X-Y / X / Y), Loop, Flag, Random; 0 when done.
int idEditSize(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int ret = 1;
    Vec* pos = &d->pos;
    int i, j;
    int yy;
    const char** tbl;
    const char** tbl3;
    const char** tbl4;
    u32 ofs;

    switch (w->editStep) {
    case 0:
        if (joy->trg & 0x200) {
            ret = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->subCur--;
        }
        if (joy->rep & 0x40004) {
            w->subCur++;
        }
        w->subCur = w->subCur < 0 ? 0 : (w->subCur > 5 ? 5 : w->subCur);
        switch (w->subCur) {
        case 0:
            if (joy->trg & 0x100) {
                w->editStep++;
            }
            break;
        case 1:
            if (joy->trg & 0x100) {
                w->subStep = 0;
                w->editStep++;
            }
            break;
        case 2: {
            s8 cur = 0;
            s8 n;

            if (d->flags10A & 0x10) {
                cur = 1;
            }
            if (d->flags10A & 0x20) {
                cur = 2;
            }
            n = cur;
            if (joy->rep & 0x10001) {
                n = cur - 1;
            }
            if (joy->rep & 0x20002) {
                n = n + 1;
            }
            n = n < 0 ? 0 : (n > 2 ? 2 : n);
            if (n != cur) {
                d->flags10A = d->flags10A & 0xCF;
                switch (n) {
                case 0:
                    break;
                case 1:
                    d->flags10A |= 0x10;
                    break;
                case 2:
                    d->flags10A |= 0x20;
                    break;
                }
            }
            break;
        }
        case 3:
            if (joy->rep & 0x10001) {
                d->loop_flag |= 2;
            }
            if (joy->rep & 0x20002) {
                d->loop_flag &= 0xFD;
            }
            break;
        case 4:
            if (joy->rep & 0x10001) {
                d->flags10A &= 0x7F;
            }
            if (joy->rep & 0x20002) {
                d->flags10A |= 0x80;
            }
            break;
        }
        eprintf(x, y, 5, 0, "SIZE-MENU");
        y += 0xE;
        for (i = 0; i <= 5; i++) {
            int col;

            yy = y + i * 0xE;
            eprintf(x, yy, (w->subCur == i) ? 4 : 0, 0, "%s", sizeMenuName[i]);
            if (w->subCur == i && (w->cnt & 0x18)) {
                eprintf(x - 8, yy, 0x16, 0, ">");
            }
            col = 7;
            if (w->subCur == i) {
                col = 0;
            }
            switch (i) {
            case 2:
                // one table pointer PER ARM (tbl/tbl3/tbl4, each set in its own arm): a shared multi-set
                // `tbl` has 15 refs / 180 insns (0.250) and is allocated ahead of the PRE'd `i * 0xE` copy
                // (11 / 133, 0.248); three single-set pointers (5 refs each) come after it and share r23.
                // A single-set pointer is not hoisted out of the i loop because the set is in a case arm,
                // and the last arm's set in the for-init puts `li j,0` before the `lis` (LUID order)
                tbl = axisName;
                // `ofs = j * 4` as a giv with TWO uses (the name address and the `ofs * 8` column): loop.c
                // then combines the address giv into it (`lwzx r8,rOfs,rTbl`, `addi rOfs,4`) and emits the
                // `li rOfs,0` init after the hoisted invariants, as the original; a single-use DEST_REG giv
                // is never combined (the address giv gets its own stepped pointer, `lwz 0(rG)`)
                for (j = 0; j <= 2; j++) {
                    ofs = j * 4;
                    if (j == 0) {
                        col = 0;
                        if (d->flags10A & 0x30) {
                            col = 7;
                        }
                    }
                    if (j == 1) {
                        col = 7;
                        if (d->flags10A & 0x10) {
                            col = 0;
                        }
                    }
                    if (j == 2) {
                        col = 7;
                        if (d->flags10A & 0x20) {
                            col = 0;
                        }
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32)tbl));
                }
                break;
            case 3:
                tbl3 = onOffName2;
                for (j = 0; j <= 1; j++) {
                    ofs = j * 4;
                    if ((j != 0) != ((d->loop_flag >> 1) & 1)) {
                        col = 0;
                    } else {
                        col = 7;
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32)tbl3));
                }
                break;
            case 4:
                for (j = 0, tbl4 = texFixName; j <= 1; j++) {
                    ofs = j * 4;
                    if ((j != 1) != (d->flags10A >> 7)) {
                        col = 0;
                    } else {
                        col = 7;
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32)tbl4));
                }
                break;
            }
        }
        break;
    case 1:
        switch (w->subCur) {
        case 0: {
            f32 step;

            if (joy->trg & 0x200) {
                w->editStep = 0;
                break;
            }
            step = (joy->on & 0x100) ? 10.0f : 1.0f;
            if (joy->on & 0x800) {
                f32 ratio;
                int big;

                if (d->sizeX > d->sizeY) {
                    ratio = d->sizeY / d->sizeX;
                    big = 0;
                } else {
                    ratio = d->sizeX / d->sizeY;
                    big = 1;
                }
                if ((joy->rep & 9) || (joy->on & 0x90000)) {
                    if (big == 0) {
                        d->sizeX -= step;
                    } else {
                        d->sizeY -= step;
                    }
                }
                if ((joy->rep & 6) || (joy->on & 0x60000)) {
                    if (big == 0) {
                        d->sizeX += step;
                    } else {
                        d->sizeY += step;
                    }
                }
                if ((joy->rep & 0xF) || (joy->on & 0xF0000)) {
                    if (big == 0) {
                        d->sizeY = d->sizeX * ratio;
                    } else {
                        d->sizeX = d->sizeY * ratio;
                    }
                }
            } else {
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    d->sizeX -= step;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    d->sizeX += step;
                }
                if ((joy->rep & 8) || (joy->on & 0x80000)) {
                    d->sizeY -= step;
                }
                if ((joy->rep & 4) || (joy->on & 0x40000)) {
                    d->sizeY += step;
                }
            }
            break;
        }
        case 1:
            switch (w->subStep) {
            case 0:
                w->pSctrl = &idSctrl1;
                idSctrl1.curve = (Hermite1*) &d->curve1;
                SctrlSetAxisLabel(w->pSctrl, "Frame", "Scale");
                w->pSctrl->gridX = 15.0f;
                w->pSctrl->gridY = 0.5f;
                w->pSctrl->grid.x = 1.0f;
                w->pSctrl->grid.y = 0.01f;
                w->pSctrl->flags = 3;
                if (w->pSctrl->curve->num <= 1) {
                    f32 xr = 90.0f;
                f32 yr = 2.0f;

                SctrlInitAxisRange(w->pSctrl, xr * 1.2f, xr * -0.2f, yr * 1.2f, yr * -1.2f);
                    SctrlInitCursor(w->pSctrl, 0.0f, 0.0f);
                } else {
                    SctrlAdjustAxisRange(w->pSctrl);
                    SctrlInitCursor(w->pSctrl, w->pSctrl->curve->key[0].t, w->pSctrl->curve->key[0].v);
                }
                w->subStep++;
                break;
            case 1:
                if (DbSctrl(w->pSctrl, w->menuX, w->menuY) == 0) {
                    w->editStep = 0;
                }
                w->drawSafe = 0;
                break;
            }
            break;
        }
        break;
    }
    if (w->editStep == 1 && w->subCur == 0) {
        Vec p;
        Vec a;
        Vec c;
        Vec b;
        int sx, sy;
        int dy;

        p = *pos;
        if (!(d->vtxType & 0xF)) {
            p.x -= d->sizeX * 0.5f;
            p.y = d->sizeY * 0.5f + p.y;
        }
        a = p;
        a.x += d->sizeX;
        Draw_line3d(&p, &a, 0xFFFFFFFF, 0);
        b = a;
        b.x -= 10.0f;
        b.y += 10.0f;
        Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
        sx = (int) ((p.x + a.x) * 0.5f * 256.0f / 320.0f + 256.0f);
        sy = (int) (224.0f - p.y * 224.0f / 240.0f);
        dy = -0xE;
        if (sy <= 7) {
            dy = 0;
        }
        eprintf(sx - 0x10, sy + dy, 0, 0, "(%3.0f)", d->sizeX);
        c = p;
        c.y -= d->sizeY;
        Draw_line3d(&p, &c, 0xFFFFFFFF, 0);
        b = c;
        b.x -= 10.0f;
        b.y += 10.0f;
        Draw_line3d(&c, &b, 0xFFFFFFFF, 0);
        sx = (int) (p.x * 256.0f / 320.0f + 256.0f);
        sy = (int) (224.0f - (p.y + c.y) * 0.5f * 224.0f / 240.0f);
        dy = -0x30;
        if (sx <= 0x2F) {
            dy = 8;
        }
        eprintf(sx + dy, sy, 0, 0, "(%3.0f)", d->sizeY);
    }
    return ret;
}

static const char* colMenuName[4] = { "COL-MENU", "Anima :", "Loop  :", "Random:" };
static const char* colName[8] = { "R:", "G:", "B:", "A:", "R:", "G:", "B:", "A:" };
static const char* onOffName3[2] = { "ON", "OFF" };

// Color column: COL-MENU (R G B A of col0 and col1 with the d-pad), Anima (colour curve), Loop,
// Random; 0 when done.
int idEditColor(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int ret = 1;
    u8* pc = 0;
    int i, j;
    int yy;
    int v = 0;
    int step;
    const char** tbl3;
    int x2;

    switch (w->editStep) {
    case 0: {

        if (joy->trg & 0x10) {
            w->editStep = ret;
            break;
        }
        if (joy->trg & 0x200) {
            ret = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->colCur--;
        }
        if (joy->rep & 0x40004) {
            w->colCur++;
        }
        w->colCur = w->colCur < 0 ? 7 : (w->colCur > 7 ? 0 : w->colCur);
        switch (w->colCur) {
        case 0: v = d->col0[0]; break;
        case 1: v = d->col0[1]; break;
        case 2: v = d->col0[2]; break;
        case 3: v = d->col0[3]; break;
        case 4: v = d->col1[0]; break;
        case 5: v = d->col1[1]; break;
        case 6: v = d->col1[2]; break;
        case 7: v = d->col1[3]; break;
        }
        step = (joy->on & 0x100) ? 10 : 1;
        if ((joy->rep & 1) || (joy->on & 0x10000)) {
            v -= step;
        }
        if ((joy->rep & 2) || (joy->on & 0x20000)) {
            v += step;
        }
        v = v < 0 ? 0 : (v > 255 ? 255 : v);
        switch (w->colCur) {
        case 0: d->col0[0] = v; break;
        case 1: d->col0[1] = v; break;
        case 2: d->col0[2] = v; break;
        case 3: d->col0[3] = v; break;
        case 4: d->col1[0] = v; break;
        case 5: d->col1[1] = v; break;
        case 6: d->col1[2] = v; break;
        case 7: d->col1[3] = v; break;
        }
        {
            GXColor c0;
            GXColor c1;
            GXColor c2;
            GXColor c3;
            int x1 = x + 0x18;
            int y2;

            x2 = x + 0xB0;
            y2 = y + 0x38;

            for (i = 0; i <= 7; i++) {
                int col;

                yy = y + i * 0xE;
                *(u32*) &c0 = 0;
                c0.a = 0xFF;
                *(u32*) &c1 = 0;
                c1.a = 0xFF;
                eprintf(x, yy, (w->colCur == i) ? 4 : 0, 0, "%s", colName[i]);
                if (w->colCur == i && (w->cnt & 0x18)) {
                    eprintf(x - 8, yy, 0x16, 0, ">");
                }
                col = 7;
            if (w->colCur == i) {
                col = 0;
            }
                switch (i) {
                case 0: pc = &d->col0[0]; break;
                case 1: pc = &d->col0[1]; break;
                case 2: pc = &d->col0[2]; break;
                case 3: pc = &d->col0[3]; break;
                case 4: pc = &d->col1[0]; break;
                case 5: pc = &d->col1[1]; break;
                case 6: pc = &d->col1[2]; break;
                case 7: pc = &d->col1[3]; break;
                }
                eprintf(x1, yy, col, 0, "%3d", *pc);
                switch (i) {
                case 0: c0.r = d->col0[0]; break;
                case 1: c0.g = d->col0[1]; break;
                case 2: c0.b = d->col0[2]; break;
                case 3: c0.r = d->col0[3]; c0.g = d->col0[3]; c0.b = d->col0[3]; break;
                case 4: c0.r = d->col1[0]; break;
                case 5: c0.g = d->col1[1]; break;
                case 6: c0.b = d->col1[2]; break;
                case 7: c0.r = d->col1[3]; c0.g = d->col1[3]; c0.b = d->col1[3]; break;
                }
                Draw_tile(x + 0x40, (int) ((f32) yy + 2.8f), (int) (*pc / 255.0f * 100.0f), 8, &c0);
                Draw_tile((int) ((f32) (x + 0x40) - 0.8f), (int) ((f32) yy + 1.4f), 0x65, 0xB, &c1);
            }
            c2.r = d->col0[0];
            c2.g = d->col0[1];
            c2.b = d->col0[2];
            c2.a = d->col0[3];
            Draw_tile(x2, y, 0x38, 0x38, &c2);
            c3.r = d->col1[0];
            c3.g = d->col1[1];
            c3.b = d->col1[2];
            c3.a = d->col1[3];
            Draw_tile(x2, y2, 0x38, 0x38, &c3);
        }
        break;
    }
    case 1:
        if (joy->trg & 0x200) {
            w->editStep = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->subCur--;
        }
        if (joy->rep & 0x40004) {
            w->subCur++;
        }
        w->subCur = w->subCur < 0 ? 0 : (w->subCur > 2 ? 2 : w->subCur);
        switch (w->subCur) {
        case 0:
            if (joy->trg & 0x100) {
                w->subStep = 0;
                w->editStep++;
            }
            break;
        case 1:
            if (joy->rep & 0x10001) {
                d->loop_flag |= 4;
            }
            if (joy->rep & 0x20002) {
                d->loop_flag &= 0xFB;
            }
            if (joy->trg & 0x100) {
                w->editStep--;
            }
            break;
        }
        for (i = 0; i <= 3; i++) {
            int col;

            if (i == 0) {
                col = 5;
            } else if (w->subCur + 1 == i) {
                col = 4;
            } else {
                col = 0;
            }
            yy = y + i * 0xE;
            eprintf(x, yy, col, 0, "%s", colMenuName[i]);
            if (w->subCur + 1 == i && (w->cnt & 0x18)) {
                eprintf(x - 8, yy, 0x16, 0, ">");
            }
            if (i == 2) {
                tbl3 = onOffName3;
                for (j = 0; j <= 1; j++) {
                    u32 ofs = j * 4;

                    if ((j != 0) != ((d->loop_flag >> 2) & 1)) {
                        col = 0;
                    } else {
                        col = 7;
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32) tbl3));
                }
            }
        }
        break;
    case 2:
        if (w->subCur != 0) {
            break;
        }
        switch (w->subStep) {
        case 0:
            w->pSctrl = &idSctrl2;
            idSctrl2.curve = (Hermite1*) &d->curve2;
            SctrlSetAxisLabel(w->pSctrl, "Frame", "Color");
            w->pSctrl->gridX = 15.0f;
            w->pSctrl->gridY = 50.0f;
            w->pSctrl->grid.x = 1.0f;
            w->pSctrl->grid.y = 1.0f;
            w->pSctrl->flags = 3;
            if (w->pSctrl->curve->num <= 1) {
                f32 xr = 90.0f;
                f32 yr = 256.0f;

                SctrlInitAxisRange(w->pSctrl, xr * 1.2f, xr * -0.2f, yr * 1.2f, yr * -1.2f);
                SctrlInitCursor(w->pSctrl, 0.0f, 0.0f);
            } else {
                SctrlAdjustAxisRange(w->pSctrl);
                SctrlInitCursor(w->pSctrl, w->pSctrl->curve->key[0].t, w->pSctrl->curve->key[0].v);
            }
            w->subStep++;
            break;
        case 1:
            if (DbSctrl(w->pSctrl, w->menuX, w->menuY) == 0) {
                w->editStep = ret;
            }
            w->drawSafe = 0;
            break;
        }
        break;
    }
    return ret;
}

static const char* rotMenuName[5] = { "ROT-MENU", "Anima :", "Axis  :", "Loop  :", "Random:" };
static const char* rotAxisName[3] = { "X", "Y", "Z" };
static const char* onOffName4[2] = { "ON", "OFF" };

// Rot column: ROT-MENU (angle per axis), Anima (rotation curve), Axis (X / Y / Z), Loop, Random;
// 0 when done.
int idEditRot(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int ret = 1;
    f32* pr = 0;
    int i, j;
    int yy;
    f32 step = 1.0f;
    f32 v = 0.0f;
    const char** tbl;
    const char** tbl3;
    u32 ofs;
    int n;

    switch (w->editStep) {
    case 0: {
        if (joy->trg & 0x10) {
            w->editStep = ret;
            break;
        }
        if (joy->trg & 0x200) {
            ret = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->rotCur--;
        }
        if (joy->rep & 0x40004) {
            w->rotCur++;
        }
        w->rotCur = w->rotCur < 0 ? 2 : (w->rotCur > 2 ? 0 : w->rotCur);
        switch (w->rotCur) {
        case 0: v = d->rot.x; break;
        case 1: v = d->rot.y; break;
        case 2: v = d->rot.z; break;
        }
        step = (joy->on & 0x100) ? 10.0f : 1.0f;
        if ((joy->rep & 1) || (joy->on & 0x10000)) {
            v -= step;
        }
        if ((joy->rep & 2) || (joy->on & 0x20000)) {
            v += step;
        }
        v = v < -180.0f ? 180.0f : (v > 180.0f ? -180.0f : v);
        switch (w->rotCur) {
        case 0: d->rot.x = v; break;
        case 1: d->rot.y = v; break;
        case 2: d->rot.z = v; break;
        }
        for (i = 0; i <= 2; i++) {
            int col;

            yy = y + i * 0xE;
            eprintf(x, yy, (w->rotCur == i) ? 4 : 0, 0, "%s:", rotAxisName[i]);
            if (w->rotCur == i && (w->cnt & 0x18)) {
                eprintf(x - 8, yy, 0x16, 0, ">");
            }
            col = 7;
            if (w->rotCur == i) {
                col = 0;
            }
            switch (i) {
            case 0: pr = &d->rot.x; break;
            case 1: pr = &d->rot.y; break;
            case 2: pr = &d->rot.z; break;
            }
            // the original loop has 5 more insns than ours at loop.c time, which keeps pass 2 from
            // hoisting the ">" string high (71 >= insn_count), and no block boundary before the latch
            // (the `i++` is scheduled before the eprintf call): four dead `col` sets (deleted by flow,
            // counted by loop.c) and a codeless use of `d` (its extra ref keeps d ahead of x + 0x18 in
            // global alloc, r26/r25)
            asm("" : : "r"(d)); // COMPILER-DIFF: 3 (loop.c pass-2 insn_count)
            eprintf(x + 0x18, yy, col, 0, "%4.0f", *pr);
            col = 7; // COMPILER-DIFF: 3 (loop.c pass-2 insn_count, dead sets)
            col = 6;
            col = 7;
            col = 6;
        }
        break;
    }
    case 1:
        if (joy->trg & 0x200) {
            w->editStep = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->subCur--;
        }
        if (joy->rep & 0x40004) {
            w->subCur++;
        }
        w->subCur = w->subCur < 0 ? 0 : (w->subCur > 3 ? 3 : w->subCur);
        switch (w->subCur) {
        case 0:
            if (joy->trg & 0x100) {
                w->subStep = 0;
                w->editStep++;
            }
            break;
        case 1:
            if (joy->rep & 0x10001) {
                d->rotAxis--;
            }
            if (joy->rep & 0x20002) {
                d->rotAxis++;
            }
            n = d->rotAxis;
            d->rotAxis = n < 0 ? 0 : (n > 2 ? 2 : n);
            break;
        case 2:
            if (joy->rep & 0x10001) {
                d->loop_flag |= 8;
            }
            if (joy->rep & 0x20002) {
                d->loop_flag &= 0xF7;
            }
            if (joy->trg & 0x100) {
                w->editStep--;
            }
            break;
        }
        for (i = 0; i <= 4; i++) {
            int col;

            if (i == 0) {
                col = 5;
            } else if (w->subCur + 1 == i) {
                col = 4;
            } else {
                col = 0;
            }
            yy = y + i * 0xE;
            eprintf(x, yy, col, 0, "%s", rotMenuName[i]);
            if (w->subCur + 1 == i && (w->cnt & 0x18)) {
                eprintf(x - 8, yy, 0x16, 0, ">");
            }
            switch (i) {
            case 2:
                tbl = rotAxisName;
                for (j = 0; j <= 2; j++) {
                    ofs = j * 4;
                    if (j == d->rotAxis) {
                        col = 0;
                    } else {
                        col = 7;
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32)tbl));
                }
                break;
            case 3:
                for (j = 0, tbl3 = onOffName4; j <= 1; j++) {
                    ofs = j * 4;
                    if ((j != 0) != ((d->loop_flag >> 3) & 1)) {
                        col = 0;
                    } else {
                        col = 7;
                    }
                    eprintf(x + 0x40 + ofs * 8, y + i * 0xE, col, 0, "%s", *(const char**)(ofs + (u32)tbl3));
                }
                break;
            }
        }
        break;
    case 2:
        if (w->subCur != 0) {
            break;
        }
        switch (w->subStep) {
        case 0:
            w->pSctrl = &idSctrl3;
            idSctrl3.curve = (Hermite1*) &d->curve3;
            SctrlSetAxisLabel(w->pSctrl, "Frame", "Degree");
            w->pSctrl->gridX = 15.0f;
            w->pSctrl->gridY = 45.0f;
            w->pSctrl->grid.x = 1.0f;
            w->pSctrl->grid.y = 1.0f;
            w->pSctrl->flags = 3;
            if (w->pSctrl->curve->num <= 1) {
                f32 xr = 90.0f;
                f32 yr = 360.0f;

                SctrlInitAxisRange(w->pSctrl, xr * 1.2f, xr * -0.2f, yr * 1.2f, yr * -1.2f);
                SctrlInitCursor(w->pSctrl, 0.0f, 0.0f);
            } else {
                SctrlAdjustAxisRange(w->pSctrl);
                SctrlInitCursor(w->pSctrl, w->pSctrl->curve->key[0].t, w->pSctrl->curve->key[0].v);
            }
            w->subStep++;
            break;
        case 1:
            if (DbSctrl(w->pSctrl, w->menuX, w->menuY) == 0) {
                w->editStep = ret;
            }
            w->drawSafe = 0;
            break;
        }
        break;
    }
    return ret;
}

static const char* transMenuName[5] = { "Type   :", "Trans  :", "Power  :", "Mask SW:", "MaskTex:" };
static const char* transTypeName[5] = { "BLND", "ADD ", "ADD2", "ADD3", "MULT" };
static const char* transModeName[5] = { "NORMAL", "NEGA  ", "R-NRML", "R-OFST", "R-RPLC" };

// Trans column: Type (BLND / ADD / ADD2 / ADD3 / MULT), Trans mode (NORMAL / NEGA / R-NRML /
// R-OFST / R-RPLC), Power, Mask SW, MaskTex; 0 when done.
int idEditTrans(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int ret = 1;
    int i, j;
    int yy;

    switch (w->editStep) {
    case 0:
        if (joy->trg & 0x200) {
            ret = 0;
            break;
        }
        if (joy->rep & 0x80008) {
            w->subCur--;
        }
        if (joy->rep & 0x40004) {
            w->subCur++;
        }
        w->subCur = w->subCur < 0 ? 0 : (w->subCur > 4 ? 4 : w->subCur);
        switch (w->subCur) {
        case 0: {
            int v = d->transType;

            if (joy->rep & 0x10001) {
                v--;
            }
            if (joy->rep & 0x20002) {
                v++;
            }
            v = v < 0 ? 4 : (v > 4 ? 0 : v);
            d->transType = v;
            break;
        }
        case 1: {
            int v = d->transMode;

            if (joy->rep & 0x10001) {
                v--;
            }
            if (joy->rep & 0x20002) {
                v++;
            }
            v = v < 0 ? 3 : (v > 3 ? 0 : v);
            d->transMode = v;
            break;
        }
        case 2: {
            register int step asm("r11") = (joy->on & 0x100) ? 10 : 1; // COMPILER-DIFF: pin (global-alloc order: the target allocates step (r11) before joy (r10))

            if (joy->rep & 0x10001) {
                d->power -= step;
            }
            if (joy->rep & 0x20002) {
                d->power += step;
            }
            break;
        }
        case 3:
            if (joy->trg & 0x10001) {
                d->maskSw |= 1;
            }
            if (joy->trg & 0x20002) {
                d->maskSw &= ~1;
            }
            break;
        case 4: {
            int step = (joy->on & 0x100) ? 10 : 1;

            if (joy->rep & 0x10001) {
                d->maskTex -= step;
            }
            if (joy->rep & 0x20002) {
                d->maskTex += step;
            }
            break;
        }
        }
        eprintf(x, y, 5, 0, "TRANS-MENU");
        y += 0xE;
        for (i = 0; i <= 4; i++) {
            int col;

            yy = y + i * 0xE;
            eprintf(x, yy, (w->subCur == i) ? 4 : 0, 0, "%s", transMenuName[i]);
            if (w->subCur == i && (w->cnt & 0x18)) {
                eprintf(x - 8, yy, 0x16, 0, ">");
            }
            col = 7;
            if (w->subCur == i) {
                col = 0;
            }
            switch (i) {
            case 0:
                eprintf(x + 0x48, y, 0, 0, "[%s]", transTypeName[d->transType]);
                for (j = 0; j <= 4; j++) {
                    int c = 7;
                    if (j == d->transType) {
                        c = 0;
                    }
                    eprintf(x + 0x80 + j * 0x18, y + i * 0xE, c, 0, "%02d", j);
                }
                break;
            case 1:
                for (j = 0; j <= 3; j++) {
                    int c = 7;
                    if (j == d->transMode) {
                        c = 0;
                    }
                    eprintf(x + 0x48 + j * 0x38, y + i * 0xE, c, 0, "%s", transModeName[j]);
                }
                break;
            case 2:
                eprintf(x + 0x48, y + 0x1C, 0, 0, "%5d", d->power);
                break;
            case 3:
                if (d->maskSw & 1) {
                    eprintf(x + 0x48, y + 0x2A, col, 0, "ON-/---");
                } else {
                    eprintf(x + 0x48, y + 0x2A, col, 0, "---/OFF");
                }
                break;
            case 4:
                eprintf(x + 0x48, y + 0x38, 0, 0, "%2x", d->maskTex);
                break;
            }
        }
        break;
    }
    return ret;
}

static const char* markMenuName[1] = { "Mark:" };

// Mark column: left/right pick the element's mark number (the id the game addresses it by; used
// marks are tracked in markUse), A applies; 0 when done.
int idEditMark(IdTool* w, int x, int y)
{
    ID_DATA* d = toolIdGetPtrPR(w->parentNo, w->no);
    JOY* joy = &Joy[0];
    int ret = 1;
    int i;
    int yy;
    int y2;

    switch (w->editStep) {
    case 0:
        w->editStep++;
    case 1: {
        int dir = 0;
        int step;

        if (joy->rep & 0x10001) {
            dir = -1;
        }
        if (joy->rep & 0x20002) {
            dir = 1;
        }
        step = dir;
        if (joy->on & 0x100) {
            step = dir << 4;
        }
        if (dir != 0) {
            w->markUse[d->mark] = 0;
            d->mark += step;
            while (w->markUse[d->mark] != 0) {
                d->mark += dir;
                if (d->mark == 0xFF) {
                    break;
                }
            }
            w->markUse[d->mark] = 1;
        }
        if (joy->trg & 0x200) {
            ret = 0;
        }
        break;
    }
    }
    eprintf(x, y, 5, 0, "MARK-MENU");
    y2 = y + 0xE;
    for (i = 0; i < 1; i++) {
        int col;

        yy = y2 + i * 0xE;
        eprintf(x, yy, (w->subCur == i) ? 4 : 0, 0, "%s", markMenuName[i]);
        if (w->subCur == i && (w->cnt & 0x18)) {
            eprintf(x - 8, yy, 0x16, 0, ">");
        }
        col = 7;
            if (w->subCur == i) {
                col = 0;
            }
        if (i == 0) {
            eprintf(x + 0x28, y2, col, 0, "%02X", d->mark);
        }
        // the original loop has 5 more insns at loop.c time (pass-1 threshold 65 < insn_count keeps the
        // "%02X" high for pass 2, after the yy giv init); dead sets are deleted by flow, counted by loop.c
        col = 7; // COMPILER-DIFF: 3 (loop.c pass-1 insn_count, dead sets)
        col = 6;
        col = 7;
        col = 6;
        col = 7;
    }
    return ret;
}

static int editDispNo(IdTool* w, int x, int y);
static int editDispId(IdTool* w, int x, int y);
static int editDispPos(IdTool* w, int x, int y);
static int editDispSize(IdTool* w, int x, int y);
static int editDispColor(IdTool* w, int x, int y);
static int editDispRot(IdTool* w, int x, int y);
static int editDispTrans(IdTool* w, int x, int y);
static int editDispMark(IdTool* w, int x, int y);

// Column text printers of the element list, in column order.
static int (*editDispFunc[8])(IdTool*, int, int) = {
    editDispNo, editDispId, editDispSize, editDispPos, editDispColor, editDispRot, editDispTrans, editDispMark,
};

// Draws the element list (8 rows from listTop, at the top or bottom of the screen): per slot the
// No / Id / Size / Pos / Color / Rot / Trans / Mark columns, the cursor column highlighted.
void toolIdEditDisp(IdTool* w)
{
    int row;
    int y;
    int cx = 2;
    int i;
    ID_DATA* d;
    int n1, n2;
    int k, x128;

    if (w->focus == 0) {
        return;
    }
    row = 2;
    if (w->dispTop == 0) {
        row = 0x14;
    }
    if (w->listTop + 7 < w->no) {
        w->listTop = w->no + 0xF9;
    } else if (w->listTop > w->no) {
        w->listTop = w->no;
    }
    y = row * 0xE;
    for (i = 0; i <= 7; i++) {
        int wdt = editDispFunc[i](w, cx << 3, y);

        if (i == w->editSel) {
            if (w->editMode != 0 || (w->cnt & 0x18)) {
                int top = w->listTop - 1;
                int yy = (row + (w->no - top)) * 0xE;

                eprintf((cx - 1) << 3, yy, 0x16, 0, ">");
                eprintf((cx + wdt) << 3, yy, 0x16, 0, "<");
            }
        }
        cx += wdt + 1;
    }
    {
        // COMPILER-DIFF: candidate (sched tie: the target's `li r24,0xc` sits between cmpwi and bne,
        // i.e. jump.c's "if (c) x = a; else x = b" -> "x = b; if (c) x = a" fired only in jump2
        // (after sched2), which puts `x = b` right before the branch. With a one-insn else arm the
        // transform fires in jump1 and both schedulers hoist the li to t=1; a multi-insn else arm
        // (a zero cse cannot fold, combine folds it) keeps the arms apart until jump2.)
        if (w->dispTop == 0) {
            row = 0x13;
        } else {
            int t2 = ((u32) w & 8) >> 4;
            row = t2 + 12;
        }
    }
    // COMPILER-DIFF: candidate (sched1 tie `li r3,0x128` vs `addi r7,fmt@l` at the CLIPBOARD eprintf:
    // the target's li has INSN_REG_WEIGHT 0, i.e. it was a copy of a dying pseudo at sched1 that
    // reload/update_equiv_regs later rematerialised as `li`. x128 is a single-set constant pseudo
    // with one use in another block (REG_EQUIV, cross-block single-use replacement); the constant
    // is hidden from cse1 by the LOOP_END note and from gcse cprop by the same-block `k`, so only
    // cse2 folds the def (adding the REG_EQUAL note) and nothing propagates it into the copy.
    k = 0;
    do { } while (0);
    x128 = k + 0x128;
    y = row * 0xE;
    d = toolIdGetPtrU(w->parentNo);
    for (i = w->level - 1; i >= 0; i--) {
        eprintf(i * 0x18 + 0x10, y, 0, 0, "/%02x", d->unitNo);
        d = toolIdGetPtrU(d->parentNo);
    }
    eprintf(0xC8, y, 0, 0, "Lang:[%s]", langName[w->lang]);
    n1 = toolIdCount(0xFF, 1);
    n2 = toolIdCount(0xFF, 2);
    eprintf(0x128, y, 0, 0, "Num of ID USE:%3d EMP:%3d", n1, n2);
    if (w->useCnt != 0) {
        int yy = ((w->dispTop != 0) ? row + 1 : row - 1) * 0xE;
        eprintf(x128, yy, 0, 0, "CLIPBOARD USE:%3d EMP:%3d", w->useCnt, w->empCnt);
    }
}

// No column text: slot number, on/off and group marks; returns the column width.
static int editDispNo(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "No");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "--");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 0) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            eprintf(x, y, (u8) col, 0, "%02x", d->unitNo);
        }
    }
    eprintf(x, y, 5, 0, "--");
    return 2;
}

// Id column text: texture id.
static int editDispId(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "Id");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "--");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 1) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            if (d->texId != 0xFF) {
                eprintf(x, y, (u8) col, 0, "%02X", d->texId);
            } else {
                eprintf(x, y, (u8) col, 0, "--");
            }
        }
    }
    eprintf(x, y, 5, 0, "--");
    return 2;
}

static const char* anchorName2[5] = { "CT", "LU", "RU", "RD", "LD" };

// Pos column text: position and anchor (CT / LU / RU / RD / LD).
static int editDispPos(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "-------POS-------");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        Vec* p = &d->pos;

        if (d == 0) {
            eprintf(x, y, 0x14, 0, "-- ---- ---- ----");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 3) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            eprintf(x, y, (u8) col, 0, "%s %4.0f %4.0f %4.0f", anchorName2[d->vtxType & 0xF], p->x, p->y, p->z);
        }
    }
    eprintf(x, y, 5, 0, "-----------------");
    return 0x11;
}

// Size column text.
static int editDispSize(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "-SIZE--");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "--- ---");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 2) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            eprintf(x, y, (u8) col, 0, "%3.0f %3.0f", d->sizeX, d->sizeY);
        }
    }
    eprintf(x, y, 5, 0, "-------");
    return 7;
}

// Color column text (a swatch of col0).
static int editDispColor(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;
    GXColor c;

    eprintf(x, y, 5, 0, "-COL-");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "-- --");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 4) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            c.r = d->col0[0];
            c.g = d->col0[1];
            c.b = d->col0[2];
            c.a = d->col0[3];
            Draw_tile(x, y, 0x10, 0xE, &c);
            eprintf(x + 0x18, y, (u8) col, 0, "%02x", d->col0[3]);
        }
    }
    eprintf(x, y, 5, 0, "-----");
    return 5;
}

// Rot column text.
static int editDispRot(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "-----ROT------");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "---- ---- ----");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 5) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            eprintf(x, y, (u8) col, 0, "%4.0f %4.0f %4.0f", d->rot.x, d->rot.y, d->rot.z);
        }
    }
    eprintf(x, y, 5, 0, "--------------");
    return 0xE;
}

// Trans column text (type / mode).
static int editDispTrans(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "TRNS");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "- --");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 6) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            eprintf(x, y, (u8) col, 0, "%1d %02d", d->transMode, d->transType);
        }
    }
    eprintf(x, y, 5, 0, "----");
    return 4;
}

// Mark column text.
static int editDispMark(IdTool* w, int x, int y)
{
    int i;
    ID_DATA* d;

    eprintf(x, y, 5, 0, "Mk");
    y += 0xE;
    for (i = w->listTop; i < w->listTop + 8; i++, y += 0xE) {
        d = toolIdGetPtrPR(w->parentNo, (u8) i);
        if (d == 0) {
            eprintf(x, y, 0x14, 0, "--");
        } else {
            int col;

            if (d->be_flag & 1) {
                if (i == w->no && w->editSel == 7) {
                    col = 4;
                } else {
                    col = toolIdColorAttr(d);
                }
            } else {
                col = 0x14;
            }
            if (d->mark != 0xFF) {
                eprintf(x, y, (u8) col, 0, "%02X", d->mark);
            } else {
                eprintf(x, y, (u8) col, 0, "--");
            }
        }
    }
    eprintf(x, y, 5, 0, "--");
    return 2;
}

static const char* optMenuName[3] = { "Language  :[   ]", "Proc bar  :", "Resolution:" };
static const char* langName2[7] = { "jpn", "eng", "ger", "fra", "spa", "ita", "cmn" };

// Opt.: Language (jpn .. cmn: which SubScreen/<lang> files are used), Proc bar, Resolution; B back.
static void toolIdOption(IdTool* w)
{
    static int optCur;
    JOY* joy = &Joy[0];
    int i;
    int col;
    int cx, mx, vx, sx;
    int r0, r1, r2;

    switch (w->editStep) {
    case 0:
        optCur = 0;
        w->editStep++;
    case 1:
        if (joy->trg & 0x200) {
            w->mode = 1;
            break;
        }
        if (joy->rep & 0x80008) {
            optCur--;
        }
        if (joy->rep & 0x40004) {
            optCur++;
        }
        optCur = optCur < 0 ? 2 : (optCur > 2 ? 0 : optCur);
        switch (optCur) {
        case 0:
            if (joy->trg & 0x100) {
                w->editStep++;
            }
            break;
        case 1:
            if (joy->rep & 0x10001) {
                DbgFlagOn(pG, DBG_PROC_BAR);
            }
            if (joy->rep & 0x20002) {
                DbgFlagOff(pG, DBG_PROC_BAR);
            }
            break;
        case 2:
            if (Screen.width == 640.0f) {
                if (joy->rep & 0x20002) {
                    ScreenReSizeI(0x200, (u32) Screen.height);
                }
            } else {
                if (joy->rep & 0x10001) {
                    ScreenReSizeI(0x280, (u32) Screen.height);
                }
            }
            break;
        }
        break;
    case 2:
        if (optCur != 0) {
            break;
        }
        if (joy->trg & 0x200) {
            w->editStep--;
            w->lang = w->lang2;
        } else if (joy->trg & 0x100) {
            if (w->lang2 != w->lang) {
                w->lang2 = w->lang;
                w->reload |= 2;
            }
            w->editStep--;
        } else {
            if (joy->rep & 0x80008) {
                w->lang--;
            }
            if (joy->rep & 0x40004) {
                w->lang++;
            }
            w->lang = w->lang < 0 ? 0 : (w->lang > 6 ? 6 : w->lang);
        }
        break;
    }
    cx = 0x23;
    r0 = 0xB;
    eprintf(cx << 3, (r0 - 1) * 0xE, 5, 0, "OPTION");
    for (i = 0; i <= 2; i++) {
        int y = (r0 + i) * 0xE;

        // col/sx before the menu-name eprintf: `sx = 0x2E` is a pass-1 movable
        // moved ahead of the optMenuName lo_sum, so the lo_sum's threshold drops
        // to 65*1*2 = 130 < 131 real insns and it stays until loop pass 2, whose
        // hoists land after the pass-1 langName2/Screen/pool pairs (the target's
        // preheader order). r1/r2 stay in the same ebb as the loop top, before the
        // if/switch so cse1 does not fold the case constants; mx/vx computed from
        // cx right before use so they hoist in loop pass 2 (after the giv inits).
        col = (i == optCur) ? 4 : 0;
        sx = 0x2E;
        eprintf(cx << 3, y, col, 0, "%s", optMenuName[i]);
        r1 = 0xC;
        r2 = 0xD;
        if (i == optCur && (w->cnt & 0x18)) {
            mx = cx - 1;
            eprintf(mx << 3, y, 0x16, 0, ">");
        }
        col = 0;
        if (w->editStep == 2) {
            col = 7;
        }
        switch (i) {
        case 0:
            vx = cx + 0xC;
            eprintf(vx << 3, r0 * 0xE, 0, 0, "%s", langName2[w->lang]);
            break;
        case 1:
            if (DbgFlagChk(pG, DBG_PROC_BAR)) {
                eprintf(sx << 3, r1 * 0xE, col, 0, "ON-/---");
            } else {
                eprintf(sx << 3, r1 * 0xE, col, 0, "---/OFF");
            }
            break;
        case 2:
            if (Screen.width == 640.0f) {
                eprintf(sx << 3, r2 * 0xE, col, 0, "640/---");
            } else {
                eprintf(sx << 3, r2 * 0xE, col, 0, "---/512");
            }
            break;
        }
    }
    if (w->editStep == 2) {
        int sx;

        col = 0;
        for (i = 0, sx = 0x2E; i <= 6; i++) {
            if (i != w->lang) {
                eprintf(sx << 3, (r0 - w->lang + i) * 0xE, (u8) col, 0, "-%s-", langName2[i]);
            }
        }
    }
}

// Load / Save (menuCur 1 = save): picks the sub screen kind (tool .. omk_r) and the file number,
// then reads x:\soft/Room/SubScreen/<lang>/uwf/<kind>NNN.uwf (decoded into the elements; the .eff
// texture set reloaded) or writes it (toolIdDataEncode); B back to the menu.
static void toolIdFile(IdTool* w)
{
    JOY* joy = &Joy[0];
    char path[0x100];
    char effPath[0x100];
    int mode = (w->menuCur == 1);
    int size;
    int cy;
    int row;
    int col;

    switch (w->editStep) {
    case 0:
        w->grpSw = 0;
        w->editStep++;
    case 1:
        if (joy->trg & 0x200) {
            w->mode = 1;
            break;
        }
        if (joy->rep & 0x10001) {
            w->type--;
        }
        if (joy->rep & 0x20002) {
            w->type++;
        }
        w->type = w->type < 0 ? 0x14 : (w->type > 0x14 ? 0 : w->type);
        if (joy->rep & 0x30003) {
            w->fileNo = 0;
        }
        if (joy->trg & 0x100) {
            if ((s8) w->type2 != w->type) {
                w->type2 = w->type;
                w->reload |= 1;
            }
            w->editStep++;
        }
        break;
    case 2:
        if (joy->trg & 0x200) {
            w->editStep--;
            break;
        }
        if (joy->rep & 0x10001) {
            w->fileNo--;
        }
        if (joy->rep & 0x20002) {
            w->fileNo++;
        }
        if (joy->trg & 0x100) {
            w->editStep++;
        }
        break;
    case 3:
        if (joy->trg & 0x200) {
            w->editStep--;
            break;
        }
        if (joy->rep & 0x10001) {
            w->grpSw = 1;
        }
        if (joy->rep & 0x20002) {
            w->grpSw = 0;
        }
        if (joy->trg & 0x100) {
            w->editStep++;
        }
        break;
    case 4:
        if (w->grpSw == 1) {
            sprintf(effPath, "x:\\soft/Room/SubScreen/%s/%s.eff", langName[w->lang], subScreenName[w->type]);
            sprintf(path, "x:\\soft/Room/SubScreen/%s/uwf/%s%03d.uwf", langName[w->lang], subScreenName[w->type],
                    w->fileNo);
            switch (mode) {
            case 0:
                file_unlock(path);
                size = toolIdDataEncode(pIdBuf0, w);
                HDWrite(path, pIdBuf0, size);
                break;
            case 1:
                if (w->reload != 0) {
                    if (HDRead(effPath, pIdBuf3) > 0x900000) {
                        pLog->err(0, 0, "toolIdFile(): Eff(%s) file is too large.", effPath);
                    } else {
                        switch (w->type) {
                        case 1:
                            IdTexRelease(TEX_OWNER_ID_COCKPIT);
                            IdTexRelease(TEX_OWNER_ID_SHARE);
                            IdTexRelease(TEX_OWNER_ID_TOOL);
                            IdTexDataLoad(pIdBuf1, TEX_OWNER_ID_COCKPIT);
                            break;
                        case 5:
                            IdTexRelease(TEX_OWNER_ID_COCKPIT);
                            IdTexRelease(TEX_OWNER_ID_SHARE);
                            IdTexRelease(TEX_OWNER_ID_TOOL);
                            IdTexDataLoad(pIdBuf1, TEX_OWNER_ID_COCKPIT);
                            IdTexDataLoad(pIdBuf2, TEX_OWNER_ID_COCKPIT);
                            break;
                        case 0:
                        case 0xD:
                        case 0xE:
                        case 0xF:
                        case 0x10:
                        case 0x11:
                        case 0x12:
                        case 0x13:
                        case 0x14:
                            IdTexRelease(TEX_OWNER_ID_COCKPIT);
                            IdTexRelease(TEX_OWNER_ID_SHARE);
                            IdTexRelease(TEX_OWNER_ID_TOOL);
                            IdTexDataLoad(pIdBuf3, TEX_OWNER_ID_TOOL);
                            break;
                        default:
                            IdTexRelease(TEX_OWNER_ID_COCKPIT);
                            IdTexRelease(TEX_OWNER_ID_SHARE);
                            IdTexRelease(TEX_OWNER_ID_TOOL);
                            IdTexDataLoad(pIdBuf1, TEX_OWNER_ID_COCKPIT);
                            IdTexDataLoad(pIdBuf2, TEX_OWNER_ID_SHARE);
                            IdTexDataLoad(pIdBuf3, TEX_OWNER_ID_TOOL);
                            break;
                        }
                        w->reload &= ~1;
                    }
                    if (w->reload & 2) {
                        sprintf(effPath, "x:\\soft/Room/SubScreen/%s/ckpt.eff", langName[w->lang]);
                        if (HDRead(effPath, pIdBuf1) > 0x100000) {
                            pLog->err(0, 0, "toolIdFile(): Eff(%s) file is too large.", effPath);
                        } else {
                            switch (w->type) {
                            case 0:
                            case 0xD:
                            case 0xE:
                            case 0xF:
                            case 0x10:
                            case 0x11:
                            case 0x12:
                            case 0x13:
                            case 0x14:
                                break;
                            default:
                                IdTexRelease(TEX_OWNER_ID_COCKPIT);
                                IdTexDataLoad(pIdBuf1, TEX_OWNER_ID_COCKPIT);
                                break;
                            }
                            sprintf(effPath, "x:\\soft/Room/SubScreen/%s/share.eff", langName[w->lang]);
                            if (HDRead(effPath, pIdBuf2) > 0x300000) {
                                pLog->err(0, 0, "toolIdFile(): Eff(%s) file is too large.", effPath);
                            } else {
                                switch (w->type) {
                                case 0:
                                case 1:
                                case 0xD:
                                case 0xE:
                                case 0xF:
                                case 0x10:
                                case 0x11:
                                case 0x12:
                                case 0x13:
                                case 0x14:
                                    break;
                                default:
                                    IdTexRelease(TEX_OWNER_ID_SHARE);
                                    IdTexDataLoad(pIdBuf2, TEX_OWNER_ID_SHARE);
                                    break;
                                }
                                w->reload &= ~2;
                            }
                        }
                    }
                }
                file_lock(path);
                if (HDRead(path, pIdBuf0) > 0x20000) {
                    pLog->err(0, 0, "toolIdFile(): Uwf(%s) file is too large.", path);
                } else {
                    int i;

                    for (i = 0; i < ID_DATA_NUM; i++) {
                        toolIdDataInit(&idData[i]);
                        idData[i].be_flag = 0xFF;
                    }
                    toolIdDataDecode(pIdBuf0, w);
                    w->pause = 0;
                }
                break;
            }
        }
        w->mode = 1;
        w->editStep = 0;
        break;
    }

    cy = 0x23;
    switch (mode) {
    case 0:
        eprintf(0x118, 0x8C, 5, 0, "SAVE");
        break;
    case 1:
        eprintf(0x118, 0x8C, 5, 0, "LOAD");
        break;
    }
    eprintf(cy << 3, 0x9A, 0, 0, "file: ");
    eprintf(0x148, 0x9A, (w->editStep == 1) ? 4 : 0, 0, "%s", subScreenName[w->type]);
    cy = strlen(subScreenName[w->type]) + 0x29;
    eprintf(cy << 3, 0x9A, (w->editStep == 2) ? 4 : 0, 0, "%03d", w->fileNo);
    cy += 3;
    eprintf(cy << 3, 0x9A, 0, 0, ".uwf");
    col = (w->editStep == 3) ? 4 : 0;
    if (w->grpSw) {
        eprintf(0x118, 0xA8, col, 0, "YES/---");
    } else {
        eprintf(0x118, 0xA8, col, 0, "---/NO-");
    }
    row = 0xB;
    if (w->editStep > 2) {
        row = 0xC;
    }
    if (w->cnt & 0x18) {
        eprintf(0x110, row * 0xE, 0x16, 0, ">");
    }
}


// File record of one element (IdData2 with the state word at 0).
struct IdRec {
    u32 flags;      // 0x00
    u8 mark;        // 0x04
    u8 unitNo;      // 0x05
    u8 level;       // 0x06
    u8 parentNo;    // 0x07
    u8 no;          // 0x08
    u8 kind;        // 0x09
    u8 Id;          // 0x0A  (PS2 ID_DATA_V2 Id)
    u8 texId;       // 0x0B
    u8 vtxType;     // 0x0C
    u8 loop;        // 0x0D
    u8 scaleType;   // 0x0E
    u8 rotAxis;     // 0x0F
    u8 dir;         // 0x10
    u8 pad_11[3];
    Vec pos;        // 0x14
    Vec vtx[4];     // 0x20
    f32 sizeX;      // 0x50
    f32 sizeY;      // 0x54
    u8 col0[4];     // 0x58
    u8 col1[4];     // 0x5C
    Vec rot;        // 0x60
    u8 blendType;   // 0x6C
    u8 transType;   // 0x6D
    u8 maskId;      // 0x6E
    u8 flags_7F;    // 0x6F
    u8 transSub;    // 0x70
    u8 pad_71[3];
    u32 ofs[6];     // 0x74
};

struct IdSortEnt {
    u8 idx;
    u8 no;
};

extern "C" {
// qsort order of the save: by level, then parent, then slot.
static int id_cmp(const void* a, const void* b)
{
    return ((IdSortEnt*) a)->no - ((IdSortEnt*) b)->no;
}
}

// Serialises the live elements into the IDSystem data image (header, one IdRec per element sorted
// by level / parent / slot, then the variable path / curve blocks); returns the byte size. The
// same image drives the game's IdSub tables.
int toolIdDataEncode(void* buf, IdTool* w)
{
    u8* base = (u8*) buf;
    int maxLevel = -1;
    IdRec* rec = (IdRec*) (base + 8);
    int cnt;
    int lv;
    int i, k;
    int n;
    u8* ext;
    IdSortEnt sort[ID_DATA_NUM];
    ID_DATA* d;

    for (i = 0; i < ID_DATA_NUM; i++) {
        if (idData[i].be_flag != 0xFF && idData[i].level > maxLevel) {
            maxLevel = idData[i].level;
        }
    }
    cnt = 0;
    for (i = 0; i < ID_DATA_NUM; i++) {
        if (idData[i].be_flag != 0xFF) {
            cnt++;
        }
    }
    ext = (u8*) (rec + cnt);
    cnt = 0;
    for (lv = 0; lv <= maxLevel; lv++) {
        n = 0;
        for (i = 0; i < ID_DATA_NUM; i++) {
            d = &idData[i];
            if (d->be_flag != 0xFF && lv == d->level) {
                sort[n].idx = i;
                sort[n].no = d->no;
                n++;
            }
        }
        qsort(sort, n, sizeof(IdSortEnt), id_cmp);
        for (i = 0; i < n; i++) {
            d = &idData[sort[i].idx];
            rec->flags = d->be_flag;
            rec->mark = d->mark;
            rec->unitNo = d->unitNo;
            rec->level = d->level;
            rec->parentNo = d->parentNo;
            rec->no = d->no;
            rec->kind = d->kind;
            rec->Id = d->Id;
            rec->texId = d->texId;
            rec->vtxType = d->vtxType;
            rec->loop = d->loop_flag;
            rec->scaleType = d->flags10A;
            rec->rotAxis = d->rotAxis;
            rec->dir = d->dir;
            rec->pos = d->pos;
            rec->vtx[0] = d->vtx[0];
            rec->vtx[1] = d->vtx[1];
            rec->vtx[2] = d->vtx[2];
            rec->vtx[3] = d->vtx[3];
            rec->sizeX = d->sizeX;
            rec->sizeY = d->sizeY;
            rec->col0[0] = d->col0[0];
            rec->col0[1] = d->col0[1];
            rec->col0[2] = d->col0[2];
            rec->col0[3] = d->col0[3];
            rec->col1[0] = d->col1[0];
            rec->col1[1] = d->col1[1];
            rec->col1[2] = d->col1[2];
            rec->col1[3] = d->col1[3];
            rec->rot = d->rot;
            rec->blendType = d->transType;
            rec->transType = d->transMode;
            rec->maskId = d->maskTex;
            rec->flags_7F = d->maskSw;
            rec->transSub = d->power;
            if (d->path0.n > 0) {
                FuncPathData* p = (FuncPathData*) ext;
                FuncPathWork* q;

                rec->ofs[0] = ext - base;
                p->k = d->path0.k;
                p->n = d->path0.n;
                for (k = 0; k < p->n; k++) {
                    p->pos[k] = d->path0.pos[k];
                }
                ext += p->n * sizeof(Vec) + 8;
                rec->ofs[1] = ext - base;
                q = (FuncPathWork*) ext;
                for (k = 0; k < p->n; k++) {
                    q->alpha[k].x = 0.0f;
                    q->alpha[k].y = 0.0f;
                    q->alpha[k].z = 0.0f;
                }
                ext += p->n * sizeof(Vec) + 8;
            } else {
                rec->ofs[0] = 0;
                rec->ofs[1] = 0;
            }
            if (d->curve0.num > 0) {
                Hermite1* h = (Hermite1*) ext;

                rec->ofs[2] = ext - base;
                h->num = d->curve0.num;
                for (k = 0; k < h->num; k++) {
                    h->key[k] = d->curve0.key[k];
                }
                ext += h->num * sizeof(HermiteKey) + 4;
            } else {
                rec->ofs[2] = 0;
            }
            if (d->curve1.num > 0) {
                Hermite1* h = (Hermite1*) ext;

                rec->ofs[3] = ext - base;
                h->num = d->curve1.num;
                for (k = 0; k < h->num; k++) {
                    h->key[k] = d->curve1.key[k];
                }
                ext += h->num * sizeof(HermiteKey) + 4;
            } else {
                rec->ofs[3] = 0;
            }
            if (d->curve2.num > 0) {
                Hermite1* h = (Hermite1*) ext;

                rec->ofs[4] = ext - base;
                h->num = d->curve2.num;
                for (k = 0; k < h->num; k++) {
                    h->key[k] = d->curve2.key[k];
                }
                ext += h->num * sizeof(HermiteKey) + 4;
            } else {
                rec->ofs[4] = 0;
            }
            if (d->curve3.num > 0) {
                Hermite1* h = (Hermite1*) ext;

                rec->ofs[5] = ext - base;
                h->num = d->curve3.num;
                for (k = 0; k < h->num; k++) {
                    h->key[k] = d->curve3.key[k];
                }
                ext += h->num * sizeof(HermiteKey) + 4;
            } else {
                rec->ofs[5] = 0;
            }
            rec++;
        }
        cnt += n;
    }
    strncpy((char*) base, "2.00", 4);
    base[5] = cnt;
    return ext - base;
}

// Version 1 record (IdData with the state word at 0, no col1).
struct IdRec1 {
    u32 flags;      // 0x00
    u8 mark;        // 0x04
    u8 unitNo;      // 0x05
    u8 level;       // 0x06
    u8 parentNo;    // 0x07
    u8 no;          // 0x08
    u8 kind;        // 0x09
    u8 Id;          // 0x0A  (PS2 ID_DATA_V2 Id)
    u8 texId;       // 0x0B
    u8 vtxType;     // 0x0C
    u8 loop;        // 0x0D
    u8 scaleType;   // 0x0E
    u8 rotAxis;     // 0x0F
    u8 dir;         // 0x10
    u8 pad_11[3];
    Vec pos;        // 0x14
    Vec vtx[4];     // 0x20
    f32 sizeX;      // 0x50
    f32 sizeY;      // 0x54
    u8 col0[4];     // 0x58
    Vec rot;        // 0x5C
    u8 blendType;   // 0x68
    u8 transType;   // 0x69
    u8 maskId;      // 0x6A
    u8 flags_7F;    // 0x6B
    u8 transSub;    // 0x6C
    u8 pad_6D[3];
    u32 ofs[6];     // 0x70
};

// Copies a file Hermite curve into an element's IdCurve.
static inline void idDecodeCurve(IdCurve* c, Hermite1* h)
{
    int k;

    c->num = h->num;
    for (k = 0; k < h->num; k++) {
        c->key[k] = h->key[k];
    }
}

// Expands an IDSystem data image into the elements (slots, parents, paths, curves); marks in use
// are recorded.
int toolIdDataDecode(void* buf, IdTool* w)
{
    u8* base = (u8*) buf;
    int ver = (int) (f32) strtod((char*) base, 0);
    ID_DATA* d = 0;
    int sysVer = (int) (f32) strtod("2.00", 0);
    IdRec1* r1;
    IdRec* r2;
    int i, k;

    if (ver < sysVer) {
        pLog->err(0, 0, "toolIdDataDecode(): Dat ver.%d < Sys ver.%d", ver, sysVer);
    }
    toolIdMarkUseReset(w);
    r1 = (IdRec1*) (base + 8);
    r2 = (IdRec*) (base + 8);
    for (i = 0; i < base[5]; i++) {
        switch (ver) {
        case 1:
            if (r1->unitNo > ID_DATA_NUM - 1) {
                return -1;
            }
            d = &idData[r1->unitNo];
            d->be_flag = r1->flags;
            d->mark = r1->mark;
            if (d->mark != 0xFF) {
                if (w->markUse[d->mark] != 0) {
                    pLog->warn(0, 0, "toolIdDataDecode(): Mark 0x%02x is already used.", d->mark);
                }
                w->markUse[d->mark] = 1;
            }
            d->unitNo = r1->unitNo;
            d->level = r1->level;
            d->parentNo = r1->parentNo;
            d->no = r1->no;
            d->kind = r1->kind;
            d->Id = r1->Id;
            d->texId = r1->texId;
            d->vtxType = r1->vtxType;
            d->loop_flag = r1->loop;
            d->flags10A = r1->scaleType;
            d->rotAxis = r1->rotAxis;
            d->dir = r1->dir;
            d->pos = r1->pos;
            d->vtx[0] = r1->vtx[0];
            d->vtx[1] = r1->vtx[1];
            d->vtx[2] = r1->vtx[2];
            d->vtx[3] = r1->vtx[3];
            d->sizeX = r1->sizeX;
            d->sizeY = r1->sizeY;
            d->col0[0] = r1->col0[0];
            d->col0[1] = r1->col0[1];
            d->col0[2] = r1->col0[2];
            d->col0[3] = r1->col0[3];
            d->col1[0] = 0;
            d->col1[1] = 0;
            d->col1[2] = 0;
            d->col1[3] = 0;
            d->rot = r1->rot;
            d->transType = r1->blendType;
            d->transMode = r1->transType;
            d->maskTex = r1->maskId;
            d->maskSw = r1->flags_7F;
            d->power = r1->transSub;
            if (r1->ofs[0] != 0) {
                FuncPathData* p = (FuncPathData*) (r1->ofs[0] + (u32) base);

                d->path0.k = p->k;
                d->path0.n = p->n;
                for (k = 0; k < p->n; k++) {
                    d->path0.pos[k] = p->pos[k];
                }
            }
            if (r1->ofs[2] != 0) {
                idDecodeCurve(&d->curve0, (Hermite1*) (r1->ofs[2] + (u32) base));
            }
            if (r1->ofs[3] != 0) {
                idDecodeCurve(&d->curve1, (Hermite1*) (r1->ofs[3] + (u32) base));
            }
            if (r1->ofs[4] != 0) {
                idDecodeCurve(&d->curve2, (Hermite1*) (r1->ofs[4] + (u32) base));
            }
            if (r1->ofs[5] != 0) {
                idDecodeCurve(&d->curve3, (Hermite1*) (r1->ofs[5] + (u32) base));
            }
            break;
        case 2:
            if (r2->unitNo > ID_DATA_NUM - 1) {
                return -1;
            }
            d = &idData[r2->unitNo];
            d->be_flag = r2->flags;
            d->mark = r2->mark;
            if (d->mark != 0xFF) {
                if (w->markUse[d->mark] != 0) {
                    pLog->warn(0, 0, "toolIdDataDecode(): Mark 0x%02x is already used.", d->mark);
                }
                w->markUse[d->mark] = 1;
            }
            d->unitNo = r2->unitNo;
            d->level = r2->level;
            d->parentNo = r2->parentNo;
            d->no = r2->no;
            d->kind = r2->kind;
            d->Id = r2->Id;
            d->texId = r2->texId;
            d->vtxType = r2->vtxType;
            d->loop_flag = r2->loop;
            d->flags10A = r2->scaleType;
            d->rotAxis = r2->rotAxis;
            d->dir = r2->dir;
            d->pos = r2->pos;
            d->vtx[0] = r2->vtx[0];
            d->vtx[1] = r2->vtx[1];
            d->vtx[2] = r2->vtx[2];
            d->vtx[3] = r2->vtx[3];
            d->sizeX = r2->sizeX;
            d->sizeY = r2->sizeY;
            d->col0[0] = r2->col0[0];
            d->col0[1] = r2->col0[1];
            d->col0[2] = r2->col0[2];
            d->col0[3] = r2->col0[3];
            d->col1[0] = r2->col1[0];
            d->col1[1] = r2->col1[1];
            d->col1[2] = r2->col1[2];
            d->col1[3] = r2->col1[3];
            d->rot = r2->rot;
            d->transType = r2->blendType;
            d->transMode = r2->transType;
            d->maskTex = r2->maskId;
            d->maskSw = r2->flags_7F;
            d->power = r2->transSub;
            if (r2->ofs[0] != 0) {
                FuncPathData* p = (FuncPathData*) (r2->ofs[0] + (u32) base);

                d->path0.k = p->k;
                d->path0.n = p->n;
                for (k = 0; k < p->n; k++) {
                    d->path0.pos[k] = p->pos[k];
                }
            }
            if (r2->ofs[2] != 0) {
                idDecodeCurve(&d->curve0, (Hermite1*) (r2->ofs[2] + (u32) base));
            }
            if (r2->ofs[3] != 0) {
                idDecodeCurve(&d->curve1, (Hermite1*) (r2->ofs[3] + (u32) base));
            }
            if (r2->ofs[4] != 0) {
                idDecodeCurve(&d->curve2, (Hermite1*) (r2->ofs[4] + (u32) base));
            }
            if (r2->ofs[5] != 0) {
                idDecodeCurve(&d->curve3, (Hermite1*) (r2->ofs[5] + (u32) base));
            }
            break;
        }
        d->be_flag |= 0xD;
        r1++;
        r2++;
    }
    return 0;
}

// Sets the nesting level of `d` and, for a group, of its children recursively.
void toolIdLevel(ID_DATA* d, u8 level)
{
    int i;

    if (d->kind == 1) {
        for (i = 0; i < ID_DATA_NUM; i++) {
            if (idData[i].be_flag != 0xFF && d->unitNo == idData[i].parentNo) {
                toolIdLevel(&idData[i], level + 1);
            }
        }
    }
    d->level = level;
}

// Fits group `d`'s position / size to the bounding box of its children; 0 without children.
int toolIdGroup(ID_DATA* d)
{
    Vec v0 = { 10000.0f, -10000.0f, 0.0f };
    Vec v1 = { -10000.0f, 10000.0f, 0.0f };
    Vec a;
    Vec b;
    ID_DATA* p;
    int i;
    int n;

    d->kind = 1;
    d->vtxType = 0;
    d->pos.x = 0.0f;
    d->pos.y = 0.0f;
    d->pos.z = 0.0f;
    n = 0;
    for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
        if (p->be_flag != 0xFF && (p->be_flag & 0x80)) {
            p->parentNo = d->unitNo;
            toolIdLevel(p, d->level + 1);
            p->no = n++;
        }
    }
    toolIdSelectClear();
    for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
        if (p->parentNo == d->unitNo) {
            if ((p->vtxType & 0xF) == 0) {
                a.x = p->pos.x - p->sizeX * 0.5f;
                a.y = p->pos.y + p->sizeY * 0.5f;
                b.x = p->pos.x + p->sizeX * 0.5f;
                b.y = p->pos.y - p->sizeY * 0.5f;
            } else {
                a.x = p->pos.x;
                a.y = p->pos.y;
                b.x = p->pos.x + p->sizeX;
                b.y = p->pos.y - p->sizeY;
            }
            PSVECAdd(&a, &d->pos, &a);
            PSVECAdd(&b, &d->pos, &b);
            if (a.x <= v0.x) {
                v0.x = a.x;
            }
            if (a.y >= v0.y) {
                v0.y = a.y;
            }
            if (b.x >= v1.x) {
                v1.x = b.x;
            }
            if (b.y <= v1.y) {
                v1.y = b.y;
            }
        }
    }
    d->pos.x = (v0.x + v1.x) * 0.5f;
    d->pos.y = (v0.y + v1.y) * 0.5f;
    d->pos.z = 0.0f;
    d->sizeX = v1.x - v0.x;
    d->sizeY = v0.y - v1.y;
    for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
        if (p->parentNo == d->unitNo) {
            PSVECSubtract(&p->pos, &d->pos, &p->pos);
        }
    }
    return 1;
}

// Clears the multi-select bit (be_flag 0x80) of every element.
void toolIdSelectClear()
{
    ID_DATA* p;
    int i;

    for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
        if (p->be_flag != 0xFF && (p->be_flag & 0x80)) {
            p->be_flag &= ~0x80;
        }
    }
}

// Number of selected elements.
int toolIdCountSelected()
{
    ID_DATA* p;
    int i;
    int n = 0;

    for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
        if (p->be_flag != 0xFF && (p->be_flag & 0x80)) {
            n++;
        }
    }
    return n;
}

// Copies `d` (and, for a group, its children) into the clipboard at `level`; returns the copy.
ID_DATA* toolIdCopy(ID_DATA* d, u8 level)
{
    ID_DATA* c;
    int i;

    if (d == 0) {
        ID_DATA* p;
        for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
            if (p->be_flag != 0xFF && (p->be_flag & 0x80)) {
                toolIdCopy(p, level);
            }
        }
        return 0;
    }
    for (i = 0; i < ID_CLIP_NUM; i++) {
        c = &idClip[i];
        if (c->be_flag == 0xFF) {
            memcpy(c, d, sizeof(ID_DATA));
            c->mark = 0xFF;
            c->unitNo = i;
            c->level = level;
            if (d->kind == 1) {
                int j;
                ID_DATA* p;

                for (j = 0, p = idData; j < ID_DATA_NUM; j++, p++) {
                    if (p->be_flag != 0xFF && d->unitNo == p->parentNo) {
                        ID_DATA* q = toolIdCopy(p, level + 1);

                        if (q != 0) {
                            q->parentNo = i;
                        }
                    }
                }
            }
            return c;
        }
    }
    pLog->err(0, 0, "toolIdCopy(): Overflow!");
    return 0;
}

struct IdParentMap {
    int oldNo;
    int newNo;
};

// Inserts the clipboard elements under parent `parentNo` from slot `no` (later slots pushed back).
void toolIdPaste(u8 parentNo, u8 no)
{
    ID_DATA* c;
    ID_DATA* p;
    IdParentMap tbl[0x10];
    int nParent = 0;
    int cnt;
    int lv;
    int i, k;
    int level0;
    u8 unitNo;

    if (parentNo != 0xFF) {
        level0 = toolIdGetPtrU(parentNo)->level + 1;
    } else {
        level0 = 0;
    }
    for (lv = 0; lv <= 7; lv++) {
        c = idClip;
        cnt = 0;
        for (i = 0; i < ID_CLIP_NUM; i++, c++) {
            if (c->be_flag != 0xFF && lv == c->level) {
                p = toolIdPull();
                unitNo = p->unitNo;
                memcpy(p, c, sizeof(ID_DATA));
                p->unitNo = unitNo;
                if (lv == 0) {
                    p->parentNo = parentNo;
                    p->no = no + cnt;
                    cnt++;
                } else {
                    for (k = 0; k < nParent; k++) {
                        if (c->parentNo == tbl[k].oldNo) {
                            p->parentNo = tbl[k].newNo;
                            break;
                        }
                    }
                }
                p->level = level0 + lv;
                if (p->kind == 1) {
                    if (nParent > 0xF) {
                        pLog->err(0, 0, "toolIdPaste(): overflow parent table size %d", 0x10);
                    } else {
                        tbl[nParent].oldNo = c->unitNo;
                        tbl[nParent].newNo = p->unitNo;
                        nParent++;
                    }
                }
            }
        }
    }
}

// Deletes `d` (with children) and closes the slot gap under its parent.
void toolIdDelete(ID_DATA* d, IdTool* w)
{
    ID_DATA* p;
    int i;

    if (d == 0) {
        for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
            if (p->be_flag != 0xFF && (p->be_flag & 0x80)) {
                toolIdPush(p, w);
            }
        }
    } else {
        toolIdPush(d, w);
    }
}

// Element count: mode 0 live at `level`, 1 all live, 2 free slots.
int toolIdCount(u8 level, u32 mode)
{
    ID_DATA* p;
    int i;
    int n = 0;

    for (i = 0, p = idData; i < ID_DATA_NUM; i++, p++) {
        switch (mode) {
        case 2:
            if (p->be_flag == 0xFF) {
                n++;
            }
            break;
        case 1:
            if (p->be_flag != 0xFF) {
                n++;
            }
            break;
        case 0:
        default:
            if (p->be_flag != 0xFF && level == p->level) {
                n++;
            }
            break;
        }
    }
    return n;
}

// Empties the clipboard.
void toolIdClipboardClear()
{
    ID_DATA* p;
    int i;

    for (i = 0, p = idClip; i < ID_CLIP_NUM; i++, p++) {
        if (p->be_flag != 0xFF) {
            p->be_flag = 0xFF;
        }
    }
}

// Clipboard count with the same modes as toolIdCount.
int toolIdClipboardCount(u8 level, u32 mode)
{
    ID_DATA* p;
    int i;
    int n = 0;

    for (i = 0, p = idClip; i < ID_CLIP_NUM; i++, p++) {
        switch (mode) {
        case 2:
            if (p->be_flag == 0xFF) {
                n++;
            }
            break;
        case 1:
            if (p->be_flag != 0xFF) {
                n++;
            }
            break;
        case 0:
        default:
            if (p->be_flag != 0xFF && level == p->level) {
                n++;
            }
            break;
        }
    }
    return n;
}

// Shifts the slots of parent `parentNo` from `no` on by `n` (room for a paste / delete).
void toolIdSpace(u8 parentNo, u8 no, int n)
{
    ID_DATA* p;
    int i;

    i = 0;
    while (toolIdGetPtrPR(parentNo, no + i) == 0 && i++ < n - 1) {
    }
    n -= i;
    {
        // a SEPARATE counter: reusing `i` makes loop.c emit the reversed biv's final value (`i = 0xC0`)
        // after the loop because i's first uid is the while loop's init, and that dead insn's empty block
        // (only successor = EXIT) disables every haifa region of the function (no `cmpw cr7` hoist in the
        // while loop). The increment inside the arm is speculated into the test block by the region
        // scheduler (`add r11` before `cmplw`, fresh register).
        int j;

        for (j = 0, p = idData; j < ID_DATA_NUM; j++, p++) {
            if (p->be_flag != 0xFF && p->parentNo == parentNo && p->no >= no) {
                p->no += n;
            }
        }
    }
}

// Text colour of a list row: off / selected / group variants.
int toolIdColorAttr(ID_DATA* d)
{
    int col = 0;

    if (d->kind & 1) {
        col = 0x17;
        if (d->be_flag & 0x80) {
            col = 0x16;
        }
    } else if (d->be_flag & 0x80) {
        col = 6;
    }
    return col;
}

// Puts the game camera in the id system's 2D view (saved in camSave).
void toolIdSetCamera(IdTool* w)
{
    Vec pos = { 0.0f, 0.0f, 0.0f };
    Vec at = { 0.0f, 0.0f, 0.0f };
    f32 roll = 0.0f;
    f32 fovy = 55.0f;
    f32 t;

    w->camSave = pG->Camera;
    w->scrW = 640;
    w->scrH = 480;
    t = tanf(fovy * 0.5f * PI / 180.0f);
    pos.z = (f32) w->scrH * 0.5f / t;
    pG->Camera.param.pos = pos;
    pG->Camera.param.at = at;
    pG->Camera.param.roll = roll;
    pG->Camera.param.fovy = fovy;
    CameraSetOrientationRoll(&pG->Camera);
}

// Snaps `in` to the grid step.
void toolIdGridLock(Vec* grid, Vec* in, Vec* out)
{
    if (grid->x == 0.0f) {
        grid->x = 1.0f;
    }
    if (grid->y == 0.0f) {
        grid->y = 1.0f;
    }
    if (in->x >= 0.0f) {
        out->x = (f32) (int) (in->x / grid->x + 0.5f) * grid->x;
    } else {
        out->x = (f32) (int) (in->x / grid->x - 0.5f) * grid->x;
    }
    if (in->y >= 0.0f) {
        out->y = (f32) (int) (in->y / grid->y + 0.5f) * grid->y;
    } else {
        out->y = (f32) (int) (in->y / grid->y - 0.5f) * grid->y;
    }
}

// World matrix of an element from its parents' positions.
void toolIdCalcInitMatrix(ID_DATA* d, Mtx out)
{
    Vec rot;
    Mtx m;

    rot.x = d->rot.x * PI / 180.0f;
    rot.y = d->rot.y * PI / 180.0f;
    rot.z = d->rot.z * PI / 180.0f;
    RotMatrix(m, &rot);
    PSMTXTransApply(m, m, d->pos.x, d->pos.y, d->pos.z);
    if (d->parentNo != 0xFF) {
        Mtx pm;

        toolIdCalcInitMatrix(toolIdGetPtrU(d->parentNo), pm);
        PSMTXConcat(pm, m, pm);
    } else {
        PSMTXCopy(m, out);
    }
}

// Highlights the cursor element: a frame that slides towards its corners (trail of 8 frames),
// selected elements framed too.
void toolIdFocusOn(IdTool* w, ID_DATA* d)
{
    static Vec focusTrail[8][4];
    static int focusTrailCnt[8];
    static u32 focusTrailCol[8];
    static Vec focusVtx[4];
    static Vec focusCur[4];
    Vec pos;
    Vec dv;
    int i, k;

    if (d != 0) {
        int off = !(d->be_flag & 1);

        if (off) {
            return;
        }
        if (w->focusCnt == 0xF) {
            focusCur[0].x = -320.0f;
            focusCur[0].y = 240.0f;
            focusCur[0].z = 0.0f;
            focusCur[1].x = 320.0f;
            focusCur[1].y = 240.0f;
            focusCur[1].z = 0.0f;
            focusCur[2].x = 320.0f;
            focusCur[2].y = -240.0f;
            focusCur[2].z = 0.0f;
            focusCur[3].x = -320.0f;
            focusCur[3].y = -240.0f;
            focusCur[3].z = 0.0f;
            switch (d->vtxType & 0xF) {
            case 0:
                focusVtx[0].x = -d->sizeX * 0.5f;
                focusVtx[0].y = d->sizeY * 0.5f;
                focusVtx[0].z = 0.0f;
                focusVtx[1].x = d->sizeX * 0.5f;
                focusVtx[1].y = d->sizeY * 0.5f;
                focusVtx[1].z = 0.0f;
                focusVtx[2].x = d->sizeX * 0.5f;
                focusVtx[2].y = -d->sizeY * 0.5f;
                focusVtx[2].z = 0.0f;
                focusVtx[3].x = -d->sizeX * 0.5f;
                focusVtx[3].y = -d->sizeY * 0.5f;
                focusVtx[3].z = 0.0f;
                break;
            case 1:
                focusVtx[0].x = focusVtx[0].y = focusVtx[0].z = 0.0f;
                focusVtx[1].x = d->sizeX;
                focusVtx[1].y = 0.0f;
                focusVtx[1].z = 0.0f;
                focusVtx[2].x = d->sizeX;
                focusVtx[2].y = -d->sizeY;
                focusVtx[2].z = 0.0f;
                focusVtx[3].x = 0.0f;
                focusVtx[3].y = -d->sizeY;
                focusVtx[3].z = 0.0f;
                break;
            case 2:
                focusVtx[0].x = -d->sizeX;
                focusVtx[0].y = 0.0f;
                focusVtx[0].z = 0.0f;
                focusVtx[1].x = focusVtx[1].y = focusVtx[1].z = 0.0f;
                focusVtx[2].x = 0.0f;
                focusVtx[2].y = -d->sizeY;
                focusVtx[2].z = 0.0f;
                focusVtx[3].x = -d->sizeX;
                focusVtx[3].y = -d->sizeY;
                focusVtx[3].z = 0.0f;
                break;
            case 3:
                focusVtx[0].x = -d->sizeX;
                focusVtx[0].y = d->sizeY;
                focusVtx[0].z = 0.0f;
                focusVtx[1].x = 0.0f;
                focusVtx[1].y = d->sizeY;
                focusVtx[1].z = 0.0f;
                focusVtx[2].x = focusVtx[2].y = focusVtx[2].z = 0.0f;
                focusVtx[3].x = -d->sizeX;
                focusVtx[3].y = 0.0f;
                focusVtx[3].z = 0.0f;
                break;
            case 4:
                focusVtx[0].x = 0.0f;
                focusVtx[0].y = d->sizeY;
                focusVtx[0].z = 0.0f;
                focusVtx[1].x = d->sizeX;
                focusVtx[1].y = d->sizeY;
                focusVtx[1].z = 0.0f;
                focusVtx[2].x = d->sizeX;
                focusVtx[2].y = 0.0f;
                focusVtx[2].z = 0.0f;
                focusVtx[3].x = focusVtx[3].y = focusVtx[3].z = 0.0f;
                break;
            }
            PSMTXMultVec(w->mat, &d->pos, &pos);
            for (i = 0; i < 4; i++) {
                PSVECAdd(&focusVtx[i], &pos, &focusVtx[i]);
            }
        }
        if (w->focusCnt != 0) {
            f32 t;
            u32 c;
            u32 col;

            for (i = 0; i < 4; i++) {
                PSVECSubtract(&focusVtx[i], &focusCur[i], &dv);
                PSVECScale(&dv, &dv, 1.0f / (f32) (int) w->focusCnt);
                PSVECAdd(&focusCur[i], &dv, &focusCur[i]);
            }
            t = (f32) (0xF - w->focusCnt) * (16.0f / 3.0f) + 64.0f;
            c = (u32) t;
            col = 0xFF000000 | ((c << 16) & 0xFF0000) | ((c << 8) & 0xFF00) | (c & 0xFF);
            for (i = 0; i <= 7; i++) {
                if (focusTrailCnt[i] == 0) {
                    focusTrailCnt[i] = 6;
                    for (k = 0; k < 4; k++) {
                        focusTrail[i][k] = focusCur[k];
                    }
                    focusTrailCol[i] = col;
                    break;
                }
            }
            w->focusCnt--;
        } else {
            w->focusCnt = 0;
        }
    } else {
        w->focusCnt = 0xF;
    }
    for (i = 0; i <= 7; i++) {
        if (focusTrailCnt[i] != 0) {
            for (k = 0; k <= 3; k++) {
                Draw_line3d(&focusTrail[i][k], &focusTrail[i][(k + 1) % 4], focusTrailCol[i], 0);
            }
            focusTrailCnt[i]--;
        }
    }
}

// Restarts the focus frame animation.
void toolIdFocusReset(IdTool* w, ID_DATA* d)
{
    w->focusCnt = 0xF;
}

// The four corners of an element from its size and anchor (vtxType low nibble: centre / LU / RU /
// RD / LD).
void toolIdCalcVertex(ID_DATA* d)
{
    switch (d->vtxType & 0xF) {
    case 0:
        d->vtx[0].x = -d->sizeX * 0.5f;
        d->vtx[0].y = d->sizeY * 0.5f;
        d->vtx[0].z = 0.0f;
        d->vtx[1].x = d->sizeX * 0.5f;
        d->vtx[1].y = d->sizeY * 0.5f;
        d->vtx[1].z = 0.0f;
        d->vtx[2].x = d->sizeX * 0.5f;
        d->vtx[2].y = -d->sizeY * 0.5f;
        d->vtx[2].z = 0.0f;
        d->vtx[3].x = -d->sizeX * 0.5f;
        d->vtx[3].y = -d->sizeY * 0.5f;
        d->vtx[3].z = 0.0f;
        break;
    case 1:
        d->vtx[0].x = d->vtx[0].y = d->vtx[0].z = 0.0f;
        d->vtx[1].x = d->sizeX;
        d->vtx[1].y = 0.0f;
        d->vtx[1].z = 0.0f;
        d->vtx[2].x = d->sizeX;
        d->vtx[2].y = -d->sizeY;
        d->vtx[2].z = 0.0f;
        d->vtx[3].x = 0.0f;
        d->vtx[3].y = -d->sizeY;
        d->vtx[3].z = 0.0f;
        break;
    case 2:
        d->vtx[0].x = -d->sizeX;
        d->vtx[0].y = 0.0f;
        d->vtx[0].z = 0.0f;
        d->vtx[1].x = d->vtx[1].y = d->vtx[1].z = 0.0f;
        d->vtx[2].x = 0.0f;
        d->vtx[2].y = -d->sizeY;
        d->vtx[2].z = 0.0f;
        d->vtx[3].x = -d->sizeX;
        d->vtx[3].y = -d->sizeY;
        d->vtx[3].z = 0.0f;
        break;
    case 3:
        d->vtx[0].x = -d->sizeX;
        d->vtx[0].y = d->sizeY;
        d->vtx[0].z = 0.0f;
        d->vtx[1].x = 0.0f;
        d->vtx[1].y = d->sizeY;
        d->vtx[1].z = 0.0f;
        d->vtx[2].x = d->vtx[2].y = d->vtx[2].z = 0.0f;
        d->vtx[3].x = -d->sizeX;
        d->vtx[3].y = 0.0f;
        d->vtx[3].z = 0.0f;
        break;
    case 4:
        d->vtx[0].x = 0.0f;
        d->vtx[0].y = d->sizeY;
        d->vtx[0].z = 0.0f;
        d->vtx[1].x = d->sizeX;
        d->vtx[1].y = d->sizeY;
        d->vtx[1].z = 0.0f;
        d->vtx[2].x = d->sizeX;
        d->vtx[2].y = 0.0f;
        d->vtx[2].z = 0.0f;
        d->vtx[3].x = d->vtx[3].y = d->vtx[3].z = 0.0f;
        break;
    }
}

// Rebuilds the mark-in-use table from the live elements.
void toolIdMarkUseReset(IdTool* w)
{
    int i;

    u8* p = &w->markUse[0xFE];

    for (i = 0; i < 0xFF; i++) {
        *p-- = 0;
    }
}

static const char* randMenuName[4] = { "Amp :", "Cont:    Frame", "Intr:    Frame", "Axis:" };

// Random jitter editor: rows Amp / Cont (frames) / Intr (frames) / Axis of the s16 triple at
// w->pVal; up/down pick, left/right change; 0 on B.
int DbRandom(IdRandomWork* w, int x, int y)
{
    JOY* joy = &Joy[0];
    s16* v;
    int step = 1;
    int i;

    w->x = x;
    w->y = y;
    w->cnt++;
    v = w->pVal;
    if (joy->trg & 0x200) {
        return 0;
    }
    if (joy->rep & 0x80008) {
        w->cur--;
    }
    if (joy->rep & 0x40004) {
        w->cur++;
    }
    if (joy->rep & 0xC000C) {
        w->cnt = 0x10;
    }
    w->cur = w->cur < 0 ? 0 : (w->cur > 3 ? 3 : w->cur);
    switch (w->cur) {
    case 0:
        if (joy->on & 0x100) {
            step *= 10;
        }
        if (joy->rep & 0x10001) {
            v[0] -= step;
        }
        if (joy->rep & 0x20002) {
            v[0] += step;
        }
        if (v[0] <= 0) {
            v[0] = 0;
        }
        break;
    case 1:
        if (joy->on & 0x100) {
            step *= 10;
        }
        if (joy->rep & 0x10001) {
            v[1] -= step;
        }
        if (joy->rep & 0x20002) {
            v[1] += step;
        }
        if (v[1] <= 0) {
            v[1] = 0;
        }
        break;
    case 2:
        if (joy->on & 0x100) {
            step *= 10;
        }
        if (joy->rep & 0x10001) {
            v[2] -= step;
        }
        if (joy->rep & 0x20002) {
            v[2] += step;
        }
        if (v[2] <= 0) {
            v[2] = 0;
        }
        break;
    }
    x = w->x;
    y = w->y;
    eprintf(x, y, 5, 0, "RND-SET");
    y += 14;
    for (i = 0; i <= 3; i++) {
        eprintf(x, y + i * 14, (i == w->cur) ? 4 : 0, 0, "%s", randMenuName[i]);
        if (i == w->cur && (w->cnt & 0x18)) {
            eprintf(x - 8, y + i * 14, 0x16, 0, ">");
        }
        switch (i) {
        case 0:
            eprintf(x + 40, y, 0, 0, "%3d", v[0]);
            break;
        case 1:
            eprintf(x + 40, y + 14, 0, 0, "%3d", v[1]);
            break;
        case 2:
            eprintf(x + 40, y + 28, 0, 0, "%3d", v[2]);
            break;
        }
    }
    return 1;
}
