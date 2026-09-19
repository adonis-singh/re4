#include "types.h"
#include "light.h"
#include "atari.h"
#include "global.h"
#include "db_log.h"
#include "scheduler.h"
#include "main_mem.h"
#include "file.h"
#include "joy.h"
#include "eprintf.h"
#include "block.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "player.h"
#include "obj.h"
#include "scroll.h"
#include "dbmodule.h"
#include "main_sub.h"
#include "t_util.h"
#include "db_light.h"

extern "C" {
int sprintf(char* s, const char* fmt, ...);
unsigned int strlen(const char* s);
int strncmp(const char* a, const char* b, unsigned int n);
char* strncpy(char* dst, const char* src, unsigned int n);
}

// Scroll (room model placement) editor of the t_light REL (t_scroll.cpp; the file name is not in
// the binary). Edits the cObj scroll objects of the loaded room and writes the `.smx` parameter file.

// game/scroll.cpp: the id -> name table and its size (both `static` there; the REL resolves the
// names through the DOL symbol table).
struct ScrIdRefEnt {
    u8 type;
    const char* name;
};
extern ScrIdRefEnt ScrIdRefTbl[16];
extern const u8 ScrObjIdNum;

// per-object work of the rotating scroll objects (cObj::work)
struct ScrRotateWork {
    Vec spd;     // 0x00
    u8 local;    // 0x0C  1 = local rotation
};

// per-object work of the swinging scroll objects (cObj::work)
struct ScrSwingWork {
    f32 start3;  // 0x00
    f32 range3;  // 0x04
    f32 speed3;  // 0x08
    f32 xC;
    f32 start1;  // 0x10
    f32 range1;  // 0x14
    f32 speed1;  // 0x18
    f32 start2;  // 0x1C
    f32 range2;  // 0x20
    f32 speed2;  // 0x24
    f32 ini1;    // 0x28
    f32 ini2;    // 0x2C
    f32 ini3;    // 0x30
};

// one saved record of the .smx file (scroll.h SmxWork with the colour bytes split)
struct ScrSmxRec {
    u8 id;         // 0x00
    u8 type;       // 0x01
    u8 type2;      // 0x02
    u8 x3;         // 0x03
    u32 x4;        // 0x04
    u32 flags;     // 0x08
    u8 color[4];   // 0x0C
    u8 work[0x74]; // 0x10
    u8 color2[4];  // 0x84
    f32 uvScrollU; // 0x88
    f32 uvScrollV; // 0x8C
};

struct ScrollWork {
    u32 flags;        // 0x00  option bits (1: lock unknown models)
    u8 mode;          // 0x04  scrollFunc index
    u8 editMode;      // 0x05  editFunc index
    u8 sub;           // 0x06
    u8 sub2;          // 0x07
    u8 axis;          // 0x08  rotate editor cursor
    u8 pad_9[3];
    u8 xC;
    u8 pad_D[7];
    u8 id;            // 0x14  value being edited
    u8 pad_15[7];
    u8 ret;           // 0x1C  move() result (0 quit, 1 camera mode, 2 player mode)
    u8 modeCursor;    // 0x1D
    u8 pad_1E[2];
    u8 cursor;        // 0x20
    u8 counter;       // 0x21
    u8 debugBak;      // 0x22
    u8 rows;          // 0x23  edit table rows
    u8 areaNo;        // 0x24
    u8 subCursor;     // 0x25
    u8 modeSel;       // 0x26  0 edit, 1 camera, 2 player, 10 mode select
    u8 pad_27;
    f32 u;            // 0x28
    f32 v;            // 0x2C
    int col;          // 0x30  edit table column
    int row;          // 0x34  edit table row
    int top;          // 0x38  edit table first object
    JOY joy[2];       // 0x3C
    char* names;      // 0x50C  object names read from the header file
    char** nameTbl;   // 0x510  name per object id
    u32* flagTbl;     // 0x514  flags per object id (1 G, 2 N, 4 S)
    cLightTool* pLightTool;  // 0x518
    f32 logX;         // 0x51C
    f32 logY;         // 0x520
};

static ScrollWork scrollWork;
// the work pointer is a struct member: every store through it reloads it
struct ScrollWorkPtr {
    ScrollWork* p;
};
static ScrollWorkPtr scrollWorkPtr;
#define pWork scrollWorkPtr.p
// second view of the same symbol: an asm-labelled decl gets the assembler name "*scrollWorkPtr", a different
// string from "scrollWorkPtr", so gcse/cse hash `high(sym)` of the two views as different expressions while
// the assembler output is the same (edit_select_sub)
extern ScrollWorkPtr scrollWorkPtrV asm("scrollWorkPtr");

static char* localPath = "d:\\bio4/room/st%x/r%x%02x/r%x%02x%02d.smx";
static char* serverPath = "x:\\soft/room/st%x/r%x%02x/r%x%02x%02d.smx";
static char* cullName[3] = {"CULL BACK", "CULL FRONT", "CULL NONE"};
static char* cullShort[3] = {"BACK", "FRNT", "NONE"};

int init();
int loadBinName();
int move();
static void menu();
static void edit();
static void edit_select();
static void edit_select_sub();
static void edit_no();
static void edit_no_sub();
static void edit_name();
static void edit_id();
static void edit_id_normal();
void edit_id_rotate_ang(f32* ang, f32 step);
static void edit_id_rotate();
static void edit_id_swing_rot();
static void edit_id_sub();
static void edit_litmask();
static void edit_ot();
static void edit_flag();
void edit_flag_core(cObj* obj);
void SetColor(u8* c, int add, int mask, int minOne);
static void edit_col();
void edit_col_core(cObj* obj);
static void edit_tex();
static void edit_pos();
static void edit_ang();
static void edit_scale();
static void light();
static void texture();
static void load();
static void save();
int saveMain(const char* path);
static void option();
static void quit();
void setMirrorModel(cObj* obj, int on);
static void printEditTable();
void printCursor(int x, int y);
void clearWork();
int wkck(cObj* obj, int no);
int smxCk(cObj* obj);

static void (*scrollFunc[8])() = {menu, edit, light, texture, load, save, option, quit};

// SCROLL TOOL entry (debug menu 16): init, then move() every frame (result 2 = player mode runs the
// player / camera), restores the flags and ends the task.
int ToolScroll()
{
    int ret;

    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
    SmdClear(1);
    Block.dispAllBlock(1);
    init();
    while ((ret = move()) != 0) {
        if (ret == 2) {
            pPL->move();
            CameraMove();
        }
        TaskSleep(1);
    }
    SmdClear(1);
    Block.dispAllBlock(0);
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
    return 0;
}

// Tool start: work + name / flag tables on the Debug heap, the room's object names from the host
// header (loadBinName: r<room>smd.h SMD_ defines with their G/N/S flags), log window with the build
// stamp, debug mode 0xC, default flags.
int init()
{
    int i;

    pWork = &scrollWork;
    memclr_asm(pWork, sizeof(ScrollWork));
    pWork->ret = 1;
    pWork->flags = 1;
    pWork->rows = 5;
    pWork->names = (char*) Debug_alloc(ObjMgr.nArray * 17, 1);
    pWork->nameTbl = (char**) Debug_alloc(1000, 1);
    for (i = 0; i < 250; i++) {
        pWork->nameTbl[i] = 0;
    }
    pWork->flagTbl = (u32*) Debug_alloc(1000, 1);
    pWork->logX = 24.0f;
    pWork->logY = 140.0f;
    pLog->clear();
    pLog->modeSet((s16) pWork->logX, (s16) pWork->logY, 0x3C, 0xA);
    pLog->mes(0, 0, "BIO HAZARD 4 SCROLL TOOL");
    pLog->mes(0, 0, "MEM:%08X", pWork);
    pLog->mes(0, 0, "%s", "Nov 25 2004");  // __DATE__ of the original build
    pLog->mes(0, 0, "%s", "10:22:42");     // __TIME__
    pWork->debugBak = pG->debug_mode;
    pG->debug_mode = 0xC;
    loadBinName();
    return 0;
}

// Reads the room's `rNNNsmd.h` header: `#define SMD_<name>\t<id>\t// G N S` lines give the object
// names and the G/N/S flags.
int loadBinName()
{
    char path[256];
    u8* buf;
    u8* p;
    char* name = pWork->names;
    u32 line;
    u32 i;
    u32 num;

    sprintf(path, "d:/bio4/prog/head/r%03xsmd.h", pG->room_id);
    if (HDReadMemAlloc(path, (void**) &buf) == 0) {
        return 0;
    }
    p = buf;
    line = 0;
SKIP_LINE:
    line++;
    while (*p != '\n') {
        p++;
    }
    p++;
    if (line <= 8) {
        goto SKIP_LINE;
    }
    do {
        if (strncmp((char*) p, "//", 2) == 0) {
            p += 2;
        }
        if (strncmp((char*) p, "#define", 7) != 0) {
            break;
        }
        p += 8;
        if (strncmp((char*) p, "SMD_", 4) == 0) {
            p += 4;
        }
        i = 0;
        while (*p != '\t') {
            name[i++] = *p++;
            if (i > 15) {
                while (*p != '\t') {
                    p++;
                }
                break;
            }
        }
        name[i] = 0;
        p += 3;
        num = 0;
        i++;
        do {
            s8 c = *p++;
            num *= 10;
            num += c - '0';
        } while (*p != '\t');
        if (pWork->nameTbl[num] == NULL) {
            pWork->nameTbl[num] = name;
        }
        name += i;
        while ((s8) *p != '\n' && (s8) *p != '/') {
            p++;
        }
        // the same (s8) view as the loop above: jump.c threads the loop's '\n' exit past this test
        if ((s8) *p != '\n') {
            p += 3;
            while (*p != '\n') {
                switch ((s8) *p) {
                case 'G':
                    pWork->flagTbl[num] |= 1;
                    break;
                case 'N':
                    pWork->flagTbl[num] |= 2;
                    break;
                case 'S':
                    pWork->flagTbl[num] |= 4;
                    break;
                }
                p++;
            }
        }
        p++;
        asm("" : : "r"(p)); // COMPILER-DIFF: #13 (keep-alive: the `addi p` outranks the exit compare in the original)
    } while (num <= 0xF8);
    Mem_free(buf);
    return 1;
}

// Frame: modeSel 0 edit (pad copies taken), 1 CAMERA MODE (pad 1 becomes the camera pad), 2 PLAYER
// MODE (returns 2), 10 MODE SELECT (SCROLL EDIT / CAMERA / PREVIEW, START opens it); then
// scrollFunc[mode]: menu, edit, light, texture, load, save, option, quit. Returns ret (0 = quit).
int move()
{
    int col;

    eprintf(0x18, 0xE, 0, 0, "SCROLL TOOL");
    switch (pWork->modeSel) {
    case 0:
        JOY_COPY(pWork, 0x3C, 0);
        JOY_COPY(pWork, 0x2A4, 1);
        pWork->ret = 1;
        break;
    case 1:
        col = 0;
        if (!(pG->Frame_cnt & 0x10)) {
            col = 0x14;
        }
        eprintf(0xD8, 0, col, 0, "CAMERA MODE");
        Joy[1] = Joy[0];
        Joy[1].trg &= ~0x1000;
        memclr_asm(&pWork->joy[0], sizeof(JOY));
        pWork->ret = 1;
        break;
    case 2:
        pWork->areaNo = CamCtrl.CurrentAreaNo();
        col = 0;
        if (!(pG->Frame_cnt & 0x10)) {
            col = 0x14;
        }
        eprintf(0xD8, 0, col, 0, "PLAYER MODE");
        JOY_COPY(pWork, 0x2A4, 1);
        pWork->ret = 2;
        break;
    case 10:
        eprintf(0xD8, 0x38, 4, 0, "MODE SELECT");
        eprintf(0xD8, 0x46, 0, 0, "SCROLL EDIT");
        eprintf(0xD8, 0x54, 0, 0, "CAMERA");
        eprintf(0xD8, 0x62, 0, 0, "PREVIEW");
        printCursor(0x1A, pWork->modeCursor + 5);
        if (Joy[0].rep & 8) {
            pWork->modeCursor = (pWork->modeCursor + 3 - 1) % 3;
        }
        if (Joy[0].rep & 4) {
            pWork->modeCursor = (pWork->modeCursor + 3 + 1) % 3;
        }
        if (Joy[0].trg & 0x1300) {
            pWork->modeSel = pWork->modeCursor;
            Joy[0].trg &= ~0x1000;
            switch (pWork->modeSel) {
            case 0:
                TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
                break;
            case 2:
                TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
                break;
            }
        }
        break;
    }
    if (Joy[0].trg & 0x1000) {
        if (pWork->modeSel != 10) {
            pWork->modeCursor = pWork->modeSel;
            pWork->modeSel = 10;
        }
    }
    pWork->counter++;
    if (pWork->joy[0].rep != 0) {
        pWork->counter = 0;
    }
    scrollFunc[pWork->mode]();
    ObjMgr.move();
    LightMgr.move();
    CameraMove();
    pWork->logX += (f32) Joy[0].substickX * 0.1f;
    pWork->logY -= (f32) Joy[0].substickY * 0.1f;
    {
        int x = (int) pWork->logX;
        int y = (int) pWork->logY;
        cLog* l = pLog.p;

        l->m_Bx = x;
        l->m_By = y;
    }
    return pWork->ret;
}

static char* menuName[7] = {"EDIT", "LIGHT", "TEXTURE", "LOAD", "SAVE", "OPTION", "QUIT"};

// menu list printer; inlined (the giv inits land after the PRE'd pointer high parts in the preheader)
static inline void dispList(int x, int y, char** tbl, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        eprintf(x, y + i * 14, 0, 0, tbl[i]);
    }
}

// MENU: EDIT / LIGHT / TEXTURE / LOAD / SAVE / OPTION / QUIT (mode = row + 1); B jumps to QUIT.
static void menu()
{
    eprintf(0x20, 0x2A, 4, 0, "MENU");
    dispList(0x20, 0x38, menuName, 7);
    printCursor(3, pWork->cursor + 4);
    if ((pWork->joy[0].rep & 8) || (pWork->joy[0].on & 0x80000)) {
        pWork->cursor = (pWork->cursor + 7 - 1) % 7;
    } else if ((pWork->joy[0].rep & 4) || (pWork->joy[0].on & 0x40000)) {
        pWork->cursor = (pWork->cursor + 7 + 1) % 7;
    }
    if (pWork->joy[0].rep & 0x100) {
        pWork->mode = pWork->cursor + 1;
        pWork->editMode = pWork->sub = pWork->sub2 = 0;
        clearWork();
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->cursor = 6;
    }
}

// Edit routines by editMode: 0 table cursor, 1..11 the column editors (no, name, id, litmask, ot,
// flag, col, tex, pos, ang, scale), 15 the table sub menu, 17/18/19 the id / litmask sub menus.
static void (*editFunc[30])() = {
    edit_select, edit_no,     edit_name, edit_id,  edit_litmask, edit_ot,   edit_flag, edit_col,
    edit_tex,    edit_pos,    edit_ang,  edit_scale, NULL,       NULL,      NULL,
    edit_select_sub, NULL,    edit_no_sub, edit_id_sub, edit_litmask,
};

// EDIT: runs editFunc[editMode] and prints the object table.
static void edit()
{
    editFunc[pWork->editMode]();
    printEditTable();
}

// Object table cursor: left/right pick the column, up/down the object (scrolling), A opens the
// column's editor (editMode = col + 1), Y the sub menu, B back to the menu.
static void edit_select()
{
    static const int colX[11] = {3, 7, 0x10, 0x13, 0x1C, 0x1F, 0x24, 0x29, 0x2D, 0x31, 0x35};
    cObj* obj;

    printCursor(colX[pWork->col], pWork->row + 0x1A);
    if (pWork->joy[0].rep & 2) {
        pWork->col = (pWork->col + 11 + 1) % 11;
    } else if (pWork->joy[0].rep & 1) {
        pWork->col = (pWork->col + 11 - 1) % 11;
    }
    if ((pWork->joy[0].rep & 8) || (pWork->joy[0].on & 0x80000)) {
        if (pWork->row == 0) {
            if (pWork->top != 0) {
                pWork->top--;
            } else if (pWork->joy[0].trg & 0x80008) {
                pWork->row = pWork->rows - 1;
                pWork->top = 250 - pWork->rows;
            }
        } else {
            pWork->row--;
        }
    } else if ((pWork->joy[0].rep & 4) || (pWork->joy[0].on & 0x40000)) {
        if (pWork->row < pWork->rows - 1) {
            pWork->row++;
        } else if (pWork->top < 250 - pWork->rows) {
            pWork->top++;
        } else if (pWork->joy[0].trg & 0x40004) {
            pWork->top = 0;
            pWork->row = 0;
        }
    }
    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (wkck(obj, pWork->top + pWork->row) == 0) {
        pWork->col = 0;
    }
    if (pWork->joy[0].rep & 0x100) {
        if (wkck(obj, pWork->top + pWork->row) != 0) {
            pWork->editMode = pWork->col + 1;
            pWork->sub = pWork->sub2 = 0;
        }
    }
    if (pWork->joy[0].rep & 0x800) {
        pWork->editMode += 0xF;
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->mode = 0;
        clearWork();
    }
}

static char* subMenuName[4] = {"CUT", "COPY", "INSERT", "DELETE"};

// Table SUB MENU: CUT / COPY / INSERT / DELETE on the cursor object; B closes.
static void edit_select_sub()
{
    if (pWork->xC == 0) {
        pWork->xC = 1;
    }
    eprintf(0x140, 0x46, 4, 0, "SUB MENU");
    dispList(0x140, 0x54, subMenuName, 4);
    printCursor(0x27, pWork->subCursor + 6);
    if (pWork->joy[0].rep & 8) {
        pWork->subCursor = (pWork->subCursor + 4 - 1) % 4;
    }
    if (pWork->joy[0].rep & 4) {
        pWork->subCursor = (pWork->subCursor + 4 + 1) % 4;
    }
    // COMPILER-DIFF: 3 (gcse PRE of high(sym)) -- the original keeps ONE high(scrollWorkPtr) register (r10)
    // for the last two work loads: cse1's AROUND path shares the high pseudo into the join block. Our
    // gcse (lcm.c) makes every later occurrence of an expression redundant once it is anticipated at bb 0
    // and a loop breaks the delay chain: it PREs high(scrollWorkPtr) to the end of bb 0, turns this
    // block's set into a copy of the PRE reg, cprop propagates the PRE reg into the join's load, and
    // update_equiv_regs moves the 2-ref PRE reg next to that load (`lis r9` twice). Two levers, both
    // needed: (1) the loads read the `*scrollWorkPtr` view, a different gcse expression from the
    // `scrollWorkPtr` highs of the blocks above; (2) an always-true test that cse1 cannot fold (the
    // LOOP_END note ends its ebb; cse2 folds it, 0 code) gives the blocks above an exit path that skips
    // this block, so the view's high is not anticipated before it (EARLYIN lands here, LATEIN = 1,
    // not redundant, no insertion). Without (2) the view's high is PRE'd above the dispList loop.
    {
        int z = 0;
        do {
        } while (0);
        if (z < 128) {
            if (scrollWorkPtrV.p->joy[0].rep & 0x900) {
                scrollWorkPtrV.p->sub -= 0x14;
            }
            if (scrollWorkPtrV.p->joy[0].rep & 0x200) {
                scrollWorkPtrV.p->editMode -= 0xF;
            }
        }
    }
}

// "NO" column: toggles the display bit (be_flag 2) of the object and its whole group; back to the
// table.
static void edit_no()
{
    cObj* obj;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (obj == NULL) {
        return;
    }
    do {
        obj->be_flag ^= 2;
        obj = SmdGetGroupNext(obj);
    } while (obj);
    pWork->editMode = 0;
}

// Unused sub menu slot.
static void edit_no_sub()
{
}

// NAME column: shows the object's SMD_ name from the header (read-only); B back.
static void edit_name()
{
    cObj* obj;
    char* name;
    int i;
    int y;

    eprintf(0x40, 0x8C, 4, 0, "OBJ PROPATY");
    eprintf(0x40, 0x9A, 0, 0, "NAME:");
    i = -pWork->sub;
    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    y = 0x9A + i * 14;
    name = pWork->nameTbl[pWork->top + pWork->row];
    // exits by goto: a `break` would let expand_end_loop rotate the range test to the bottom
    while (1) {
        if ((u32) name >= 0x80000000 && (u32) name <= 0x82FFFFFF) {
            if (i >= 0) {
                eprintf(0x68, y, 0, 0, "%s", name);
            }
            obj = SmdGetGroupNext(obj);
            if (obj == NULL) {
                goto done;
            }
            if (i > 8) {
                goto done;
            }
            y += 14;
            name += strlen(name) + 1;
            i++;
        } else {
            goto done;
        }
    }
done:
    if (pWork->joy[0].rep & 8) {
        pWork->sub++;
    }
    if (pWork->joy[0].rep & 4) {
        if (pWork->sub != 0) {
            pWork->sub--;
        }
    }
    if (pWork->joy[0].rep & 0x300) {
        pWork->editMode = 0;
    }
}

// Parameter editors by scroll object type (ScrIdRefTbl): 1 rotate, 2 swing, others none.
static void (*idFunc[16])() = {
    edit_id_normal, edit_id_rotate, edit_id_swing_rot, edit_id_normal, edit_id_normal, edit_id_normal,
    edit_id_normal, edit_id_normal, edit_id_normal,    edit_id_normal, edit_id_normal, edit_id_normal,
    edit_id_normal, edit_id_normal, edit_id_normal,    edit_id_normal,
};

// "First-entry init" blocks (db_light idiom): the dead `first` sets make the then arm an if/else whose
// join reloads the work pointer from its own `lis` (two sets survive to cse, one would be deleted).
static void edit_id()
{
    cObj* obj;
    int first;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (pWork->sub == 0) {
        pWork->id = obj->type;
        pWork->sub = 1;
        first = 1;
    } else {
        first = 0;
    }
    eprintf(0x40, 0x8C, 4, 0, "OBJ PROPATY");
    eprintf(0x40, 0x9A, 0, 0, "%2d %s", obj->type, ScrIdRefTbl[obj->type].name);
    if (obj->type != pWork->id) {
        eprintf(0xB0, 0x9A, 6, 0, "--> %2d %s", pWork->id, ScrIdRefTbl[pWork->id].name);
    }
    idFunc[obj->type]();
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// Types without parameters: left/right pick a new type id, A applies (the work is reset).
static void edit_id_normal()
{
    cObj* obj;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (pWork->joy[0].rep & 2) {
        pWork->id = (ScrObjIdNum + pWork->id + 1) % ScrObjIdNum;
    } else if (pWork->joy[0].rep & 1) {
        pWork->id = (ScrObjIdNum + pWork->id - 1) % ScrObjIdNum;
    }
    if (pWork->joy[0].rep & 0x100) {
        do {
            memclr_asm(obj->work, 0xAF);
            if (pWork->id == 0xF) {
                setMirrorModel(obj, 1);
            } else if (obj->type == 0xF) {
                setMirrorModel(obj, 0);
            }
            obj->type = pWork->id;
            if (obj->type == 1) {
                ((ScrRotateWork*) obj->work)->local = 1;
            }
            if (obj->type == 0) {
                obj->be_flag &= ~0x20;
            } else {
                obj->be_flag |= 0x20;
            }
            obj = SmdGetGroupNext(obj);
        } while (obj);
    }
}

// +-step on a rotation speed with the d-pad / stick, X clears it.
void edit_id_rotate_ang(f32* ang, f32 step)
{
    *ang += (f32) pWork->joy[0].stickX * step / 10000.0f;
    if (pWork->joy[0].rep & 2) {
        *ang += (pWork->joy[0].on & 0x100) ? 0.001f : 0.00001f;
    }
    if (pWork->joy[0].rep & 1) {
        *ang -= (pWork->joy[0].on & 0x100) ? 0.001f : 0.00001f;
    }
}

static char* rotName[2] = {"WORLD ROTATION", "LOCAL ROTATION"};

// Rotate type: X/Y/Z rotation speeds (WORLD / LOCAL ROTATION), Y opens a x0.5 / x1.5 / x2.0 /
// CLEAR sub menu on the speeds.
static void edit_id_rotate()
{
    cObj* obj;
    ScrRotateWork* w;
    f32 step;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    step = (pWork->joy[0].on & 0x100) ? 7.0f : 1.0f;
    switch (pWork->sub2) {
    case 0:
        pWork->axis = 0;
        pWork->sub2 = 1;
        break;
    case 1:
        obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
        do {
            w = (ScrRotateWork*) obj->work;
            switch (pWork->axis) {
            case 0:
                edit_id_normal();
                break;
            case 1:
                edit_id_rotate_ang(&w->spd.x, step);
                break;
            case 2:
                edit_id_rotate_ang(&w->spd.y, step);
                break;
            case 3:
                edit_id_rotate_ang(&w->spd.z, step);
                break;
            case 4:
                if (pWork->joy[0].rep & 0x103) {
                    w->local ^= 1;
                }
                break;
            }
            obj = SmdGetGroupNext(obj);
        } while (obj);
        if (pWork->joy[0].rep & 8) {
            pWork->axis = (pWork->axis + 5 - 1) % 5;
        }
        if (pWork->joy[0].rep & 4) {
            pWork->axis = (pWork->axis + 5 + 1) % 5;
        }
        if (pWork->joy[0].rep & 0x800) {
            pWork->sub2 = 2;
            pWork->subCursor = 0;
        }
        printCursor(7, pWork->axis + 0xB);
        break;
    case 2:
        eprintf(0x140, 0x46, 4, 0, "SUB MENU");
        eprintf(0x140, 0x54, 0, 0, "x0.5");
        eprintf(0x140, 0x62, 0, 0, "x1.5");
        eprintf(0x140, 0x70, 0, 0, "x2.0");
        eprintf(0x140, 0x7E, 0, 0, "CLEAR");
        printCursor(0x27, pWork->subCursor + 6);
        if (pWork->joy[0].rep & 8) {
            pWork->subCursor = (pWork->subCursor + 4 - 1) % 4;
        }
        if (pWork->joy[0].rep & 4) {
            pWork->subCursor = (pWork->subCursor + 4 + 1) % 4;
        }
        if (pWork->joy[0].rep & 0x900) {
            obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
            do {
                w = (ScrRotateWork*) obj->work;
                switch (pWork->subCursor) {
                case 0:
                    switch (pWork->axis) {
                    case 1:
                        w->spd.x *= 0.5f;
                        break;
                    case 2:
                        w->spd.y *= 0.5f;
                        break;
                    case 3:
                        w->spd.z *= 0.5f;
                        break;
                    }
                    break;
                case 1:
                    switch (pWork->axis) {
                    case 1:
                        w->spd.x *= 1.5f;
                        break;
                    case 2:
                        w->spd.y *= 1.5f;
                        break;
                    case 3:
                        w->spd.z *= 1.5f;
                        break;
                    }
                    break;
                case 2:
                    switch (pWork->axis) {
                    case 1:
                        w->spd.x *= 2.0f;
                        break;
                    case 2:
                        w->spd.y *= 2.0f;
                        break;
                    case 3:
                        w->spd.z *= 2.0f;
                        break;
                    }
                    break;
                case 3:
                    switch (pWork->axis) {
                    case 1:
                        w->spd.x = 0.0f;
                        break;
                    case 2:
                        w->spd.y = 0.0f;
                        break;
                    case 3:
                        w->spd.z = 0.0f;
                        break;
                    }
                    break;
                }
                obj = SmdGetGroupNext(obj);
            } while (obj);
            pWork->sub2 = 1;
        }
        if (pWork->joy[0].rep & 0x200) {
            pWork->sub2 = 1;
        }
        break;
    }
    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    w = (ScrRotateWork*) obj->work;
    eprintf(0x40, 0xA8, 0, 0, "ROTATE X SPEED %3.5f", w->spd.x);
    eprintf(0x40, 0xB6, 0, 0, "ROTATE Y SPEED %3.5f", w->spd.y);
    eprintf(0x40, 0xC4, 0, 0, "ROTATE Z SPEED %3.5f", w->spd.z);
    eprintf(0x40, 0xD2, 0, 0, rotName[w->local & 1]);
}

// stick / pad step of one swing parameter
#define SWING_EDIT(v, scale)                                                        \
    v += (f32) pWork->joy[0].stickX * step / scale;                                     \
    if (pWork->joy[0].rep & 2) {                                                    \
        v += (pWork->joy[0].on & 0x100) ? 0.001f : 0.00001f;                        \
    }                                                                               \
    if (pWork->joy[0].rep & 1) {                                                    \
        v -= (pWork->joy[0].on & 0x100) ? 0.001f : 0.00001f;                        \
    }                                                                               \
    if (pWork->joy[0].rep & 0x800) {                                                \
        v = 0.0f;                                                                   \
    }

// Swing type: three axes of INI R / START / SPEED / RANGE (the pendulum parameters).
static void edit_id_swing_rot()
{
    ScrSwingWork* w;
    f32 step;

    w = (ScrSwingWork*) SmdGetGroupObjPtr(pWork->top + pWork->row)->work;
    step = (pWork->joy[0].on & 0x100) ? 10.0f : 1.0f;
    switch (pWork->sub2) {
    case 0:
        edit_id_normal();
        break;
    case 1:
        SWING_EDIT(w->ini1, 10000.0f);
        break;
    case 2:
        SWING_EDIT(w->start1, 10000.0f);
        break;
    case 3:
        SWING_EDIT(w->speed1, 100000.0f);
        break;
    case 4:
        SWING_EDIT(w->range1, 100000.0f);
        break;
    case 5:
        SWING_EDIT(w->ini2, 10000.0f);
        break;
    case 6:
        SWING_EDIT(w->start2, 10000.0f);
        break;
    case 7:
        SWING_EDIT(w->speed2, 100000.0f);
        break;
    case 8:
        SWING_EDIT(w->range2, 100000.0f);
        break;
    case 9:
        SWING_EDIT(w->ini3, 10000.0f);
        break;
    case 10:
        SWING_EDIT(w->start3, 10000.0f);
        break;
    case 11:
        SWING_EDIT(w->speed3, 100000.0f);
        break;
    case 12:
        SWING_EDIT(w->range3, 100000.0f);
        break;
    }
    if (pWork->joy[0].rep & 8) {
        pWork->sub2 = (pWork->sub2 + 13 - 1) % 13;
    }
    if (pWork->joy[0].rep & 4) {
        pWork->sub2 = (pWork->sub2 + 13 + 1) % 13;
    }
    printCursor(7, pWork->sub2 + 0xB);
    eprintf(0x40, 0xA8, 0, 0, "INI R %3.5f", w->ini1);
    eprintf(0x40, 0xB6, 0, 0, "START %3.5f", w->start1);
    eprintf(0x40, 0xC4, 0, 0, "SPEED %3.5f", w->speed1);
    eprintf(0x40, 0xD2, 0, 0, "RANGE %3.5f", w->range1);
    eprintf(0x40, 0xE0, 0, 0, "INI R %3.5f", w->ini2);
    eprintf(0x40, 0xEE, 0, 0, "START %3.5f", w->start2);
    eprintf(0x40, 0xFC, 0, 0, "SPEED %3.5f", w->speed2);
    eprintf(0x40, 0x10A, 0, 0, "RANGE %3.5f", w->range2);
    eprintf(0x40, 0x118, 0, 0, "INI R %3.5f", w->ini3);
    eprintf(0x40, 0x126, 0, 0, "START %3.5f", w->start3);
    eprintf(0x40, 0x134, 0, 0, "SPEED %3.5f", w->speed3);
    eprintf(0x40, 0x142, 0, 0, "RANGE %3.5f", w->range3);
}

// Unused sub menu slot.
static void edit_id_sub()
{
}

// LIT_MASK column: the model's LightInfo.SelectMask (which of the room's lights may light it, V /
// A marks); left/right pick the light, A toggles its bit; B back.
static void edit_litmask()
{
    u32 num = 32;
    cObj* obj;
    u32 i;

    if (LightMgr.nArray < 32) {
        num = LightMgr.nArray;
    }
    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (pWork->sub2 == 0) {
        edit_id_normal();
        pWork->id = 0;
        pWork->sub2 = 1;
    }
    eprintf(0x40, 0x8C, 4, 0, "MODEL PROPATY");
    if ((u32) LightMgr.getWorkPtr(pWork->id) >= 0x80000000 && (u32) LightMgr.getWorkPtr(pWork->id) <= 0x82FFFFFF &&
        (LightMgr.getWorkPtr(pWork->id)->be_flag & 1)) {
        eprintf(0x40, 0x9A, 0, 0, "LIGHT-%02d %s", pWork->id,
                (obj->LightInfo.SelectMask & (1 << pWork->id)) ? "ENABLE" : "DISABLE");
    } else {
        eprintf(0x40, 0x9A, 0, 0, "LIGHT-%02d NOT USED", pWork->id);
    }
    if (pG->Frame_cnt & 8) {
        eprintf((pWork->id + 8) * 8, 0xA8, 0, 0, "V");
        eprintf((pWork->id + 8) * 8, 0xC4, 0, 0, "A");
    }
    for (i = 0; i < num; i++) {
        cLight* l = LightMgr.getWorkPtr(i);
        int col = 0;

        if (!(obj->LightInfo.SelectMask & (1 << i))) {
            col = 0x14;
        }
        if (!(l->be_flag & 1)) {
            eprintf((8 + i) * 8, 0xB6, col, 0, "-");
        } else {
            eprintf((8 + i) * 8, 0xB6, col, 0, "%d", i % 10);
            if (i == pWork->id && i <= 31) {
                Vec pos = l->World;

                Draw_sphere(&pos, l->Radius, -1, 1, 1);
                Draw_pos(&pos, 1000);
            }
        }
    }
    if (pWork->joy[0].rep & 2) {
        pWork->id = (num + pWork->id + 1) % num;
    }
    if (pWork->joy[0].rep & 1) {
        pWork->id = (num + pWork->id - 1) % num;
    }
    for (i = 0; i < num; i++) {
        cLight* l = LightMgr.getWorkPtr(i);

        if (!(l->be_flag & 1)) {
            obj->LightInfo.SelectMask |= 1 << i;
        }
    }
    // `one` is set here and used once in the next block: local-alloc's reg_equiv_replace moves the `li 1` to
    // just before the `slw` (after the x54 load), which is where the original has it
    u32 one = 1;
    if (pWork->joy[0].rep & 0x100) {
        u32 mask = obj->LightInfo.SelectMask ^ (one << pWork->id);

        do {
            obj->LightInfo.SelectMask = mask;
            obj = SmdGetGroupNext(obj);
        } while (obj);
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

static char* otName[6] = {"NORMAL", "SORT", "SCROLL PRE", "SCROLL NORMAL", "SCROLL POST", "EFFECT"};

// OT column: draw order group (NORMAL / SORT / SCROLL PRE / SCROLL NORMAL / SCROLL POST / EFFECT).
static void edit_ot()
{
    cObj* obj;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (pWork->sub2 == 0) {
        edit_id_normal();
        pWork->id = obj->ot_type;
        pWork->sub2 = 1;
    }
    eprintf(0x40, 0x8C, 4, 0, "MODEL PROPATY");
    eprintf(0x40, 0x9A, 0, 0, "%02d %s", obj->ot_type, otName[obj->ot_type]);
    if (obj->ot_type != pWork->id) {
        eprintf(0xD0, 0x9A, 6, 0, "--> %02d %s", pWork->id, otName[pWork->id]);
    }
    if (pWork->joy[0].rep & 2) {
        pWork->id = (pWork->id + 6 + 1) % 6;
    }
    if (pWork->joy[0].rep & 1) {
        pWork->id = (pWork->id + 6 - 1) % 6;
    }
    if (pWork->joy[0].rep & 0x100) {
        int id;

        obj->ot_type = pWork->id;
        id = obj->ot_type;  // read back the just-stored member: forwarded as a plain copy
        while ((obj = SmdGetGroupNext(obj)) != NULL) {
            obj->ot_type = id;
        }
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

static char* flagName[10] = {
    "SHADOW", "VERTEX COLOR", "CAST ON", "ALPHATEST OFF", "POINT LIGHT ALL CUT",
    "TAIMATU CUT", "06 ----", "07 ----", "08 ----", "09 ----",
};

// FLAG column: the smx flag bits (SHADOW, VERTEX COLOR, CAST ON, ALPHATEST OFF, POINT LIGHT ALL
// CUT, TAIMATU CUT, ...); up/down pick, A toggles (edit_flag_core), B back.
static void edit_flag()
{
    cObj* obj;
    int i;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    eprintf(0x40, 0x8C, 4, 0, "MODEL PROPATY");
    for (i = 0; i < 10; i++) {
        u32 flags = SmxGetFlag(obj);
        int col = 0;

        if (!(flags & (1 << i))) {
            col = 0x14;
        }
        eprintf(0x40, 0x9A + i * 14, col, 0, flagName[i]);
    }
    printCursor(7, pWork->sub2 + 0xB);
    if (pWork->joy[0].rep & 8) {
        pWork->sub2 = (pWork->sub2 + 10 - 1) % 10;
    }
    if (pWork->joy[0].rep & 4) {
        pWork->sub2 = (pWork->sub2 + 10 + 1) % 10;
    }
    if (pWork->joy[0].rep & 0x100) {
        do {
            edit_flag_core(obj);
            obj = SmdGetGroupNext(obj);
        } while (obj);
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// Toggles the flag under the cursor on `obj` and applies its side effects (mirror model, shadow /
// vertex colour setup) at once.
void edit_flag_core(cObj* obj)
{
    ModelData* data = obj->pModelInfo->pData;
    s8 sel = pWork->sub2;

    switch (sel) {
    case 0:
        obj->be_flag ^= 0x10;
        break;
    case 1:
        data->flags ^= 0x40000000;
        break;
    case 2:
        obj->be_flag ^= 0x2000000;
        break;
    case 3:
        if (obj->alpha_omit == 0xFF) {
            obj->alpha_omit = 0x80;
        } else {
            obj->alpha_omit = 0xFF;
        }
        break;
    case 4:
        obj->be_flag ^= 0x8000;
        break;
    case 5:
        obj->x3D0 ^= 1;
        break;
    }
}

// adds `add` to the RGB bytes selected by `mask`, clamped to 0..255 (1..255 with minOne)
void SetColor(u8* c, int add, int mask, int minOne)
{
    int r = c[0];
    int g = c[1];
    int b = c[2];

    if (mask & 1) {
        r += add;
    }
    if (mask & 2) {
        g += add;
    }
    if (mask & 4) {
        b += add;
    }
    if (r > 0xFF) {
        r = 0xFF;
    }
    if (g > 0xFF) {
        g = 0xFF;
    }
    if (b > 0xFF) {
        b = 0xFF;
    }
    if (minOne == 1) {
        if (r <= 0) {
            r = 1;
        }
        if (g <= 0) {
            g = 1;
        }
        if (b <= 0) {
            b = 1;
        }
    } else {
        if (r < 0) {
            r = 0;
        }
        if (g < 0) {
            g = 0;
        }
        if (b < 0) {
            b = 0;
        }
    }
    c[0] = r;
    c[1] = g;
    c[2] = b;
}

static char* colName[10] = {"COLOR", " R", " G", " B", "SPECULAR", " R", " G", " B", "BLEND TYPE", "CULL MODE"};
static char* blendName[5] = {"NORMAL", "ADD", "ADD2", "ADD3", "NO_BLEND"};

// COL column: model colour / specular R G B, BLEND TYPE (NORMAL / ADD / ADD2 / ADD3 / NO_BLEND)
// and CULL MODE rows; up/down pick, left/right change (edit_col_core), B back.
static void edit_col()
{
    cObj* obj;
    cModelInfo* info;
    u32 i;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    if (pWork->joy[0].rep & 8) {
        pWork->sub2 = (pWork->sub2 + 10 - 1) % 10;
    }
    if (pWork->joy[0].rep & 4) {
        pWork->sub2 = (pWork->sub2 + 10 + 1) % 10;
    }
    eprintf(0x40, 0x8C, 4, 0, "PROPATY");
    {
        char** name = colName;

        for (i = 0; i < 10; i++) {
            eprintf(0x40, 0x9A + i * 14, 0, 0, "%s", *name++);
        }
    }
    info = obj->pModelInfo;
    eprintf(0xA0, 0xA8, 0, 0, "%d", info->color[0]);
    eprintf(0xA0, 0xB6, 0, 0, "%d", info->color[1]);
    eprintf(0xA0, 0xC4, 0, 0, "%d", info->color[2]);
    eprintf(0xA0, 0xE0, 0, 0, "%d", info->color2[0]);
    eprintf(0xA0, 0xEE, 0, 0, "%d", info->color2[1]);
    eprintf(0xA0, 0xFC, 0, 0, "%d", info->color2[2]);
    eprintf(0xA0, 0x10A, 0, 0, "%s", blendName[info->blend_mode]);
    eprintf(0xA0, 0x118, 0, 0, "%s", cullName[obj->CullMode]);
    eprintf(0x38, (pWork->sub2 + 0xB) * 14, 0, 0, ">");
    do {
        edit_col_core(obj);
        obj = SmdGetGroupNext(obj);
    } while (obj);
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// Applies the cursor row's change to `obj`'s colour bytes / blend / cull mode.
void edit_col_core(cObj* obj)
{
    cModelInfo* info;
    int add = 0;
    s8 sel;

    if (pWork->joy[0].rep & 2) {
        add = 1;
    }
    if (pWork->joy[0].rep & 1) {
        add = -1;
    }
    add += (int) ((f32) pWork->joy[0].stickX * 5.0f / 255.0f);
    if (pWork->joy[0].on & 0x100) {
        add *= 10;
    }
    info = obj->pModelInfo;
    sel = pWork->sub2;
    switch (sel) {
    case 0:
        SetColor(info->color, add, 7, 1);
        break;
    case 1:
        SetColor(info->color, add, 1, 1);
        break;
    case 2:
        SetColor(info->color, add, 2, 1);
        break;
    case 3:
        SetColor(info->color, add, 4, 1);
        break;
    case 4:
        SetColor(info->color2, add, 7, 0);
        break;
    case 5:
        SetColor(info->color2, add, 1, 0);
        break;
    case 6:
        SetColor(info->color2, add, 2, 0);
        break;
    case 7:
        SetColor(info->color2, add, 4, 0);
        break;
    case 8:
        if (pWork->joy[0].rep & 2) {
            info->blend_mode = (info->blend_mode + 6 + 1) % 6;
        }
        if (pWork->joy[0].rep & 1) {
            info->blend_mode = (info->blend_mode + 6 - 1) % 6;
        }
        break;
    case 9:
        if (pWork->joy[0].rep & 2) {
            obj->CullMode = (obj->CullMode + 3 + 1) % 3;
        }
        if (pWork->joy[0].rep & 1) {
            obj->CullMode = (obj->CullMode + 3 - 1) % 3;
        }
        break;
    }
    if (*(u32*) info->color2 & 0xFFFFFF00) {
        info->color2[3] = 0xFF;
    } else {
        info->color2[3] = 0;
    }
}

// TEX column: the texture UV scroll speeds U / V; B back.
static void edit_tex()
{
    cObj* obj;
    cModelInfo* info;
    f32 step;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    info = obj->pModelInfo;
    step = (pWork->joy[0].on & 0x100) ? 5.0f : 1.0f;
    if (pWork->sub == 0) {
        if (info->flagsDC & 1) {
            pWork->u = info->uvScrollU;
            pWork->v = info->uvScrollV;
        } else {
            pWork->u = 0.0f;
            pWork->v = 0.0f;
        }
        pWork->sub = 1;
    }
    eprintf(0x40, 0x8C, 4, 0, "TEXTURE  MODEL PROPATY");
    eprintf(0x40, 0xA8, 0, 0, "U:%3.5f", pWork->u);
    eprintf(0x40, 0xB6, 0, 0, "V:%3.5f", pWork->v);
    pWork->u += (f32) pWork->joy[0].stickX * step / 1000000.0f;
    pWork->v += (f32) pWork->joy[0].stickY * step / 1000000.0f;
    if (pWork->joy[0].rep & 0x800) {
        pWork->u = 0.0f;
        pWork->v = 0.0f;
    }
    do {
        info = obj->pModelInfo;
        if (pWork->u != 0.0f || pWork->v != 0.0f) {
            info->flagsDC |= 1;
        } else {
            info->flagsDC &= ~1;
        }
        info->uvScrollU = pWork->u;
        info->uvScrollV = pWork->v;
        obj = SmdGetGroupNext(obj);
    } while (obj);
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// POS column: stick moves the object on the XZ plane, L/R triggers its height, d-pad by 1 unit (A
// x7); B back.
static void edit_pos()
{
    cObj* obj;
    f32 step;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    step = (pWork->joy[0].on & 0x100) ? 7.0f : 1.0f;
    eprintf(0x40, 0x8C, 4, 0, "MODEL PROPATY");
    eprintf(0x40, 0x9A, 0, 0, "%6.0f %6.0f %6.0f", obj->pos.x, obj->pos.y, obj->pos.z);
    Draw_pos(&obj->pos, 1000);
    do {
        obj->pos.x += (f32) pWork->joy[0].stickX * step;
        obj->pos.z -= (f32) pWork->joy[0].stickY * step;
        obj->pos.y += (f32) pWork->joy[0].triggerRight * step * 0.5f;
        obj->pos.y -= (f32) pWork->joy[0].triggerLeft * step * 0.5f;
        if (pWork->joy[0].rep & 8) {
            obj->pos.z += 1.0f;
        } else if (pWork->joy[0].rep & 4) {
            obj->pos.z -= 1.0f;
        }
        if (pWork->joy[0].rep & 2) {
            obj->pos.x += 1.0f;
        } else if (pWork->joy[0].rep & 1) {
            obj->pos.x -= 1.0f;
        }
        if (pWork->joy[0].rep & 0x800) {
            obj->pos.x = 0.0f;
            obj->pos.y = 0.0f;
            obj->pos.z = 0.0f;
        }
        obj->matUpdate();
        obj = SmdGetGroupNext(obj);
    } while (obj);
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// ANG column: stick / L/R rotate the object about its axes; B back.
static void edit_ang()
{
    cObj* obj;
    f32 step;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    step = (pWork->joy[0].on & 0x100) ? 10.0f : 1.0f;
    eprintf(0x40, 0x8C, 4, 0, "MODEL PROPATY");
    eprintf(0x40, 0x9A, 0, 0, "%3.5f %3.5f %3.5f", obj->ang.x, obj->ang.y, obj->ang.z);
    Draw_pos(&obj->pos, 1000);
    do {
        obj->ang.x -= (f32) pWork->joy[0].stickY * step / 50000.0f;
        obj->ang.y += (f32) pWork->joy[0].stickX * step / 50000.0f;
        obj->ang.z += (f32) pWork->joy[0].triggerRight * step * 0.5f / 50000.0f;
        obj->ang.z -= (f32) pWork->joy[0].triggerLeft * step * 0.5f / 50000.0f;
        if (pWork->joy[0].rep & 8) {
            obj->ang.z += 1.0f;
        } else if (pWork->joy[0].rep & 4) {
            obj->ang.z -= 1.0f;
        }
        if (pWork->joy[0].rep & 2) {
            obj->ang.x += 1.0f;
        } else if (pWork->joy[0].rep & 1) {
            obj->ang.x -= 1.0f;
        }
        if (pWork->joy[0].rep & 0x800) {
            obj->ang.x = 0.0f;
            obj->ang.y = 0.0f;
            obj->ang.z = 0.0f;
        }
        obj->matUpdate();
        obj = SmdGetGroupNext(obj);
    } while (obj);
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// SCL column: stick / L/R change the object's scale per axis; B back.
static void edit_scale()
{
    cObj* obj;
    f32 step;

    obj = SmdGetGroupObjPtr(pWork->top + pWork->row);
    step = (pWork->joy[0].on & 0x100) ? 10.0f : 1.0f;
    eprintf(0x40, 0x8C, 4, 0, "MODEL PROPATY");
    eprintf(0x40, 0x9A, 0, 0, "%3.5f %3.5f %3.5f", obj->scale.x, obj->scale.y, obj->scale.z);
    Draw_pos(&obj->pos, 1000);
    do {
        obj->scale.x += (f32) pWork->joy[0].stickX * step / 50000.0f;
        obj->scale.z += (f32) pWork->joy[0].stickY * step / 50000.0f;
        obj->scale.y += (f32) pWork->joy[0].triggerRight * step * 0.5f / 50000.0f;
        obj->scale.y -= (f32) pWork->joy[0].triggerLeft * step * 0.5f / 50000.0f;
        if (pWork->joy[0].rep & 8) {
            obj->scale.z += 1.0f;
        } else if (pWork->joy[0].rep & 4) {
            obj->scale.z -= 1.0f;
        }
        if (pWork->joy[0].rep & 2) {
            obj->scale.x += 1.0f;
        } else if (pWork->joy[0].rep & 1) {
            obj->scale.x -= 1.0f;
        }
        if (pWork->joy[0].rep & 0x800) {
            obj->scale.x = 1.0f;
            obj->scale.y = 1.0f;
            obj->scale.z = 1.0f;
        }
        obj->matUpdate();
        obj = SmdGetGroupNext(obj);
    } while (obj);
    if (pWork->joy[0].rep & 0x200) {
        pWork->editMode = 0;
    }
}

// LIGHT: runs an embedded db_light cLightTool until it quits (player mode passes through), then
// back to the menu.
static void light()
{
    int ret;

    switch (pWork->editMode) {
    case 0:
        pWork->pLightTool = new cLightTool;
        pWork->editMode = 1;
    case 1:
        ret = pWork->pLightTool->move();
        switch (ret) {
        case 0:
            delete pWork->pLightTool;
            pWork->mode = 0;
            pWork->editMode = 0;
            break;
        case 1:
            break;
        case 2:
            pPL->move();
            CameraMove();
            break;
        }
        break;
    }
}

// TEXTURE VIEWER: left/right pick a room texture number and draw it; B back.
static void texture()
{
    void* tpl;

    eprintf(0x20, 0x2A, 4, 0, "TEXTURE VIEWER");
    switch (pWork->editMode) {
    case 0:
        pWork->editMode = 1;
    case 1:
        if (pWork->joy[0].rep & 0xA) {
            pWork->sub++;
        }
        if (pWork->joy[0].rep & 5) {
            if (pWork->sub != 0) {
                pWork->sub--;
            }
        }
        break;
    }
    eprintf(0x20, 0x38, 0, 0, "NO %d", pWork->sub);
    tpl = SmdGetTplPtr(pWork->sub);
    if ((u32) tpl - 0x80000000 <= 0x02FFFFFF) {
        DrawTpl((TEXPalette*) tpl, 0x32, 0x32, 0xC8, 0xC8);
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->mode = pWork->editMode = 0;
        clearWork();
    }
}

// LOAD: LOCAL (d:) / SERVER (x:), then the file number (r<room>NN.smx); reads the smx records into
// the objects (SmxSet); "FILE OPEN ERROR" when missing.
static void load()
{
    eprintf(0x20, 0x2A, 4, 0, "LOAD");
    switch (pWork->editMode) {
    case 0:
        clearWork();
        pWork->editMode = 1;
    case 1:
        eprintf(0x20, 0x38, 0, 0, "LOCAL");
        eprintf(0x20, 0x46, 0, 0, "SERVER");
        printCursor(3, pWork->cursor + 4);
        if (pWork->joy[0].rep & 8) {
            pWork->cursor = 0;
        } else if (pWork->joy[0].rep & 4) {
            pWork->cursor = 1;
        }
        if (pWork->joy[0].rep & 0x200) {
            pWork->mode = pWork->editMode = 0;
            clearWork();
        }
        break;
    }
}

// SAVE: LOCAL / SERVER, file number; writes one ScrSmxRec per registered object (id, type, flags,
// colours, work, UV scroll) to the .smx.
static void save()
{
    char path[256];

    eprintf(0x20, 0x2A, 4, 0, "SAVE");
    switch (pWork->editMode) {
    case 0:
        clearWork();
        pWork->editMode = 1;
    case 1:
        eprintf(0x20, 0x38, 0, 0, "LOCAL");
        eprintf(0x20, 0x46, 0, 0, "SERVER");
        printCursor(3, pWork->cursor + 4);
        if (pWork->joy[0].rep & 8) {
            pWork->cursor = 0;
        } else if (pWork->joy[0].rep & 4) {
            pWork->cursor = 1;
        }
        if (pWork->joy[0].rep & 0x100) {
            if (pWork->cursor == 0) {
                pWork->editMode = 2;
            } else {
                pWork->editMode = 4;
            }
            pWork->cursor = 0;
        }
        if (pWork->joy[0].rep & 0x200) {
            pWork->mode = pWork->editMode = 0;
            clearWork();
        }
        break;
    case 2:
        sprintf(path, localPath, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no, pWork->cursor);
        eprintf(0x20, 0x38, 0, 0, "FILE NO:%02d", pWork->cursor);
        eprintf(0x20, 0x54, 0, 0, "%s", path);
        if (pWork->joy[0].rep & 0xA) {
            pWork->cursor++;
        } else if (pWork->joy[0].rep & 5) {
            pWork->cursor += 99;
        }
        pWork->cursor %= 100;
        if (pWork->joy[0].rep & 0x100) {
            pWork->editMode = 3;
        } else if (pWork->joy[0].rep & 0x200) {
            clearWork();
            pWork->editMode = 1;
        }
        break;
    case 3:
        sprintf(path, localPath, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no, pWork->cursor);
        if (saveMain(path) == 0) {
            pWork->editMode = 10;
        } else {
            pWork->mode = 0;
            pWork->editMode = 0;
            clearWork();
        }
        break;
    case 4:
        sprintf(path, serverPath, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no, pWork->cursor);
        eprintf(0x20, 0x38, 0, 0, "FILE NO:%02d", pWork->cursor);
        eprintf(0x20, 0x54, 0, 0, "%s", path);
        if (pWork->joy[0].rep & 0xA) {
            pWork->cursor++;
        } else if (pWork->joy[0].rep & 5) {
            pWork->cursor += 99;
        }
        pWork->cursor %= 100;
        if (pWork->joy[0].rep & 0x100) {
            pWork->editMode = 5;
        } else if (pWork->joy[0].rep & 0x200) {
            clearWork();
            pWork->editMode = 1;
        }
        break;
    case 5:
        sprintf(path, serverPath, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no, pWork->cursor);
        if (saveMain(path) != 0) {
            file_unlock(path);
            pWork->mode = 0;
            pWork->editMode = 0;
            clearWork();
        } else {
            pWork->editMode = 10;
        }
        break;
    case 10:
        eprintf(0x20, 0x38, 0, 0, "FILE OPEN ERROR");
        eprintf(0x20, 0x46, 0, 0, "PUSH BUTTON TO CONTINUE");
        if (pWork->joy[0].rep & 0x300) {
            pWork->editMode = 0;
        }
        break;
    }
}

// Writes the .smx file: one record per registered, non-default scroll object.
int saveMain(const char* path)
{
    u8* buf;
    ScrSmxRec* rec;
    cObj* obj;
    int n;
    int i;
    int size;

    buf = (u8*) Debug_alloc(0x8CB0, 1);
    if (buf == NULL) {
        return 0;
    }
    memclr_asm(buf, 0x8CB0);
    rec = (ScrSmxRec*) (buf + 0x10);
    n = 0;
    buf[0] = 0x10;
    for (i = 0; i < 250; i++) {
        obj = SmdGetGroupObjPtr(i);
        if (obj == NULL) {
            continue;
        }
        if (!obj->isAlive()) {
            continue;
        }
        if (smxCk(obj) == 0) {
            continue;
        }
        rec->id = i;
        rec->type = obj->type;
        rec->x4 = obj->LightInfo.SelectMask;
        rec->type2 = obj->ot_type;
        rec->flags = SmxGetFlag(obj);
        rec->x3 = obj->CullMode;
        *(u32*) rec->color = obj->pModelInfo->colorWord;
        rec->color[3] = obj->pModelInfo->blend_mode;
        *(u32*) rec->color2 = *(u32*) obj->pModelInfo->color2;
        rec->color2[3] = 0;
        rec->uvScrollU = obj->pModelInfo->uvScrollU;
        rec->uvScrollV = obj->pModelInfo->uvScrollV;
        memcpy((u32*) rec->work, (u32*) obj->work, sizeof(rec->work));
        rec++;
        n++;
    }
    buf[1] = n;
    size = n * sizeof(ScrSmxRec) + 0x10;
    if (HDWrite(path, buf, size) != size) {
        return 0;
    }
    Debug_free(buf);
    return 1;
}

static char* optionName[3] = {"LOCK UNKNOWN MODEL", "------------------", "SCROLL CHECK MODE"};
static int optionDummy0 = 0;
static int optionDummy1 = 0;
static u32 optionKey = 0x40000;

// OPTION: LOCK UNKNOWN MODEL / SCROLL CHECK MODE bits of ScrollWork::flags; B back.
static void option()
{
    u32 i;

    eprintf(0x20, 0x2A, 4, 0, "OPTION");
    for (i = 0; i < 3; i++) {
        int on;
        int col;

        if (i != 2) {
            on = (pWork->flags & (1 << i)) ? 1 : 0;
        } else {
            on = 1;
            if (!DbgFlagChk(pG, DBG_SCR_CHECK)) {
                on = 0;
            }
        }
        col = 0;
        if (on == 0) {
            col = 0x14;
        }
        eprintf(0x40, 0x62 + i * 14, col, 0, "%s", optionName[i]);
    }
    printCursor(7, pWork->cursor + 7);
    if (pWork->joy[0].rep & 8) {
        pWork->cursor = (pWork->cursor + 3 - 1) % (sizeof(optionName) / sizeof(char*));
    }
    if (pWork->joy[0].rep & 4) {
        pWork->cursor = (pWork->cursor + 3 + 1) % (sizeof(optionName) / sizeof(char*));
    }
    if (pWork->joy[0].rep & 0x100) {
        switch (pWork->cursor) {
        default:
            pWork->flags ^= 1 << pWork->cursor;
            break;
        case 1:
            break;
        case 2:
            if (DbgFlagChk(pG, DBG_SCR_CHECK)) {
                DbgFlagOff(pG, DBG_SCR_CHECK);
            } else {
                DbgFlagOn(pG, DBG_SCR_CHECK);
            }
            break;
        }
    }
    if (pWork->joy[0].rep & 0x200) {
        pWork->mode = 0;
    }
}

// QUIT ? YES/NO: YES ends the tool (ret 0).
static void quit()
{
    eprintf(0x20, 0x2A, 4, 0, "QUIT ?");
    if (pWork->editMode == 0) {
        pWork->cursor = 1;
        pWork->editMode = 1;
    }
    eprintf(0x30, 0x46, pWork->cursor != 0 ? 0x14 : 0, 0, "YES");
    eprintf(0x30, 0x54, pWork->cursor != 1 ? 0x14 : 0, 0, "NO");
    if (pWork->joy[0].rep & 8) {
        pWork->cursor = 0;
    } else if (pWork->joy[0].rep & 4) {
        pWork->cursor = 1;
    }
    if (pWork->joy[0].rep & 0x100) {
        if (pWork->cursor == 0) {
            pWork->ret = 0;
            pG->debug_mode = pWork->debugBak;
            pLog->clear();
            pLog->modeReset();
        } else {
            clearWork();
            pWork->mode = 0;
        }
    }
    if (pWork->joy[0].rep & 0x200) {
        clearWork();
        pWork->mode = 0;
    }
}

// Makes `obj` a mirror model (kindid 4, be_flag 0x100, not drawn normally) or a plain scroll model
// (kindid 2).
void setMirrorModel(cObj* obj, int on)
{
    switch (on) {
    case 1:
        obj->kindid = 4;
        obj->be_flag &= ~2;
        obj->be_flag |= 0x100;
        break;
    case 0:
        obj->kindid = 2;
        obj->be_flag |= 2;
        obj->be_flag &= ~0x100;
        break;
    }
}

// Draws the object table page: per object NO, NAME (NO REGIST / UNKNOWN MODEL for unmatched ids),
// ID, LIT_MASK, OT, FLAG, COL, TEX, POS, ANG, SCL columns, cursor cell highlighted.
static void printEditTable()
{
    cObj* obj;
    int i;
    int y;
    int no;
    int x;
    int col;
    int xb;  // copy of x that delays cprop's fold of the tail base (see the tail)
    // COMPILER-DIFF: #17 -- the tail column base pinned to r27 (x for 4/8 lives in r3 and never crosses a
    // call; the 0x11 base is callee-saved and shared with the dead `no`). The pin makes r27 ever-live
    // (global.c pass 0 `regs_used_so_far`), so `no` takes r27 in pass 0 and y r31, i r26 as in the original;
    // and the hard register carries the base through the tail without a REG_EQUIV (see the tail).
    // `obj->be_flag` is re-read at every use (the target's `mr r11,r0` is gcse's PRE copy of the isAlive
    // load, not a `flag` local).
    register int x2 asm("r27");
    char name[8];

    {
        // COMPILER-DIFF: candidate #17 -- a volatile asm at the function top is a sched1 barrier: the PRE'd
        // high(scrollWorkPtr) (bb 0 end) is scheduled right after it, above the first eprintf call, which
        // gives it the original's live length / calls crossed (534/17: priority 262 < the "SCL" string
        // high's 270, so it is the one left without a register and re-materialised at each use; scheduled
        // after the call it is 512/16 -> 273 and takes r14 from the string high). Emits nothing.
        register int pin asm("r27");
        asm volatile("" : "=r"(pin));
    }
    eprintf(0x20, 0x15E, 4, 0, "NO= NAME==== ID LIT_MASK OT FLAG COL  TEX POS ANG SCL ========");
    // increment order y, no, i: gcse's PRE insertions of the three `+1` follow the first-occurrence order,
    // and i+1 last / y+1 first gives the target's live lengths (i in place r26, y+1 r23, no+1 r25)
    for (i = 0, y = 0x1A, no = pWork->top; i < pWork->rows; y++, no++, i++) {
        obj = SmdGetGroupObjPtr(no);
        x = 4;
        if (obj == NULL || !obj->isAlive()) {
            col = 0x14;
        } else {
            if (obj->kindid != 2 && obj->kindid != 4) {
                col = 5;
            } else if (!(obj->be_flag & 4)) {
                col = 0x14;
            } else {
                col = 0;
            }
        }
        eprintf(x * 8, y * 14, col, 0, "%03d", no);
        x = 8;
        if (obj == NULL || !obj->isAlive()) {
            eprintf(x * 8, y * 14, 0x14, 0, "NO REGIST");
            continue;
        }
        // two `!=` tests with re-reads (not a `switch`): each re-read is its own load/zero_extend pair, so
        // thread_jumps can walk the `== 4` test back to its load and thread the `beq` past the `== 4`
        // re-test below (a switch index is one promoted pseudo and the walk stops at the `== 2` jump)
        if (obj->kindid != 2 && obj->kindid != 4) {
            if (pWork->flags & 1) {
                eprintf(x * 8, y * 14, 0x14, 0, "UNKNOWN MODEL");
                continue;
            }
        }
        if (obj->kindid == 4) {
            col = 6;
        } else if (!FlagChkSignW(obj->be_flag, 29) || !FlagChkSignW(obj->be_flag, 30)) {
            col = 0x14;
        } else {
            col = 0;
            if (obj->x3D0 & 4) {
                col = 5;
            }
        }
        // the tail base is `xb + 9` (see the tail): cprop pass 1 copy-propagates xb -> x in that insn and
        // cannot fold `x + 9` in the same pass, so PRE moves it into the arms; the LOOP_END ends cse1's ebb
        // so cse1 does not rewrite the UNKNOWN arm's `x * 8` note to xb (cse2 would then fold it to 0x40)
        xb = x;
        do {
        } while (0);
        if (obj->kindid != 2 && obj->kindid != 4) {
            eprintf(x * 8, y * 14, col, 0, "UNKNOWN");
        } else {
            char* n;
            int j;

            for (j = 0; j < 8; j++) {
                name[j] = 0;
            }
            // COMPILER-DIFF: candidate #17 (global.c live length) -- the high of this load is the PRE'd
            // scrollWorkPtr high (7 refs, no register left: reload materialises `lis r11` here), so at
            // global-alloc time this block is one insn shorter than the original's and `x*8` (4 refs/38)
            // outranks `no+1` (10 refs/144) for r25. The codeless insn restores the count (39 vs 145).
            asm volatile("");
            n = pWork->nameTbl[no];
            if ((u32) n >= 0x80000000 && (u32) n <= 0x82FFFFFF) {
                strncpy(name, n, 7);
            }
            name[7] = 0;
            eprintf(x * 8, y * 14, col, 0, "%8s", name);
        }
        // COMPILER-DIFF: #13 / candidate #12 -- the two `li r27,0x11` are gcse PRE insertions at the arm ends
        // (their LUID puts them after `no*4` and the "%8s" high in sched1). A plain `x2 = 0x11` (or `x + 9`)
        // is folded by cprop pass 1 and never PRE'd; `t = xb + 9` (xb = x above) is copy-propagated to
        // `x + 9` in pass 1, PRE'd into the arms, folded to 0x11 by pass 2, and cse2 gives both sets
        // REG_EQUAL 0x11, so the PRE reg R is REG_EQUIV (live length x4). R therefore cannot be the base
        // itself (priority 0.73 -> 0.18, allocated after no/y/col); the base is the hard register r27 and
        // R is copied into it: R prefers r27 (copy preference; the copy is a no-op after allocation), and
        // R must be allocated before the "%8s" string high (4 refs/70 -> 1142) or that takes r27 in
        // pass 1: R's refs are 2 sets + `t * 8` + the copy, the copy inside a do-while so flow weights it
        // by loop depth 3: 4 + 2 + 3 = 9 refs / 4*58 -> 1163. `a0` and `tid` are computed before the
        // copy so the loop notes' sched barrier falls on the ternary's `lbz` (after the notes), not on the
        // `slwi`; a0 is allocated r3 (arg copy preference), so `slwi r3,r27,3` stays before the branch.
        {
            int t = xb + 9;
            int a0 = t * 8;
            int tid = obj->kindid == 2 ? obj->type : obj->id;
            do {
                x2 = t;
            } while (0);
            eprintf(a0, y * 14, col, 0, "%02d", tid);
        }
        {
            // COMPILER-DIFF: candidate #1 (arg copy): the target issues `addi r3,x2,3` before `lwz r8,x54`;
            // in ours the load (2 dependents: the call and the next call's r8 set) outranks the addi chain.
            // The launder keeps the argument copy `r3 = cx` (priority +1 for the chain); it emits nothing.
            int cx = (x2 + 3) * 8;
            asm("" : "+r"(cx));
            eprintf(cx, y * 14, col, 0, "%08x", obj->LightInfo.SelectMask);
        }
        eprintf((x2 + 0xC) * 8, y * 14, col, 0, "%02d", obj->ot_type);
        eprintf((x2 + 0xF) * 8, y * 14, col, 0, "FLAG");
        eprintf((x2 + 0x14) * 8, y * 14, col, 0, "%s", cullShort[obj->CullMode]);
        eprintf((x2 + 0x19) * 8, y * 14, col, 0, "TEX");
        eprintf((x2 + 0x1D) * 8, y * 14, col, 0, "POS");
        eprintf((x2 + 0x21) * 8, y * 14, col, 0, "ANG");
        eprintf((x2 + 0x25) * 8, y * 14, col, 0, "SCL");
    }
}

// Blinking ">" at text cell (x, y).
void printCursor(int x, int y)
{
    if (!(pWork->counter & 8)) {
        eprintf(x * 8, y * 14, 0, 0, ">");
    }
}

// Resets the menu cursor and table position.
void clearWork()
{
    pWork->cursor = 0;
    pWork->col = pWork->row = 0;
}

// 1 when `obj` is an editable scroll object (registered, and a known model unless the lock is off)
int wkck(cObj* obj, int no)
{
    if (obj == NULL) {
        return 0;
    }
    if (!obj->isAlive()) {
        return 0;
    }
    if (pWork->flags & 1) {
        if (obj->kindid != 2 && obj->kindid != 4) {
            return 0;
        }
    }
    return 1;
}

// 1 when the object carries non-default parameters (needs an .smx record)
int smxCk(cObj* obj)
{
    int ret = 0;

    if (obj->kindid == 2 && obj->id == 2) {
        cModelInfo* info;

        if (!(obj->be_flag & 4)) {
            return 0;
        }
        info = obj->pModelInfo;
        if (obj->type != 0 || obj->LightInfo.SelectMask != -1 || obj->ot_type != 3 || SmxGetFlag(obj) != 0 || obj->CullMode != 0 ||
            (obj->pModelInfo->colorWord & 0xFFFFFF00) != 0xFFFFFF00 || (*(u32*) obj->pModelInfo->color2 & 0xFFFFFF00) != 0 ||
            obj->pModelInfo->blend_mode != 0 || obj->pModelInfo->uvScrollU != 0.0f || obj->pModelInfo->uvScrollV != 0.0f) {
            ret = 1;
        }
    } else if (obj->kindid == 4) {
        ret = 1;
    }
    return ret;
}
