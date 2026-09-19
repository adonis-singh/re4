// game/db_menu.cpp: the debug tool menu (Start + Z style entry through DbMenuExec). Lists the
// debug tools (`menu` table: name, the REL module that holds the tool or a built-in function,
// and the tool id); selecting one loads the REL from disc, links it and runs the tool as a task
// while the game is frozen (Stop_flg); DbMenuExitAfterCheck returns to the game and unlinks the
// module.

#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "fade.h"
#include "main_mem.h"
#include "db_log.h"
#include "dvd.h"

extern "C" {
int strcmp(const char* a, const char* b);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
void* memset(void* dst, int c, unsigned int n);
u32 GetGameTime(int* h, int* m, int* s); // returns a value (main_sub.h): the call sets r3, so `addi r3,&h` loses its output dependence and issues last
void DLL_Link(void* module, void* bss);
void DLL_Unlink(void* module);
void TaskChain(void (*func)(), int arg);
}

// REL header (Dolphin OSModuleInfo + OSModuleHeader)
struct DllModule {
    u8 pad_0[0x20];
    u32 bssSize;      // 0x20
    u8 pad_24[0x10];
    void (*prolog)(); // 0x34
    void (*epilog)(); // 0x38
};

// One debug menu line (0x10 bytes)
struct DB_MENU {
    const char* name;  // 0x00
    const char* rel_name;   // 0x04  tool module to load (NULL = built-in tool)
    void (*func)();    // 0x08  built-in tool entry
    int id;            // 0x0C  DebugMenuSelected
};

// Debug menu task work (0x34 bytes)
struct test {
    u8 x0;             // 0x00
    u8 x1;             // 0x01
    u8 x2;             // 0x02
    u8 x3;             // 0x03
    s8 cursor;         // 0x04
    u8 pad_5;
    u8 flag;           // 0x06  1 = we set system flag 0x20000
    u8 exec_tool;      // 0x07  1 = start the tool named in `name`
    u8 stop_saved;     // 0x08  1 = stop flags in `stop_bak` must be restored
    u8 pad_9;
    s16 exit_wait;     // 0x0A  frames after the menu exits before it can reopen
    s16 x;             // 0x0C
    s16 y;             // 0x0E
    u32 stop_bak;      // 0x10  pG->flags_170
    char name[0x20];   // 0x14
};

void RoomJump();
void FlagEdit();
void ToolDebugPage();
void ToolOption();
void ToolLogView();
void ToolScreenShot();
void ToolBugcheck();

#define MENU_NUM 34

DB_MENU menu[MENU_NUM] = {
    {"AREA JUMP", NULL, RoomJump, 0},
    {"FLAG EDIT", NULL, FlagEdit, 1},
    {"DEBUG PAGE", NULL, ToolDebugPage, 2},
    {"MOVIE TEST", "t_movie.rel", NULL, 4},
    {"DEBUG OPTION", NULL, ToolOption, 5},
    {"MOT SEQUENCE", "tools.rel", NULL, 6},
    {"CAMERA", "t_camera.rel", NULL, 7},
    {"LIGHT TOOL", "t_light.rel", NULL, 8},
    {"ESP TOOL", "t_esp.rel", NULL, 9},
    {"EM_LIST TOOL", "t_emlist.rel", NULL, 10},
    {"SOUND TEST", "t_movie.rel", NULL, 11},
    {"ROUTE CHECK", "tools.rel", NULL, 12},
    {"ATARI TOOL", "tools.rel", NULL, 13},
    {"CONS TOOL", "tools.rel", NULL, 14},
    {"VIB TOOL", "tools.rel", NULL, 15},
    {"SCROLL TOOL", "t_light.rel", NULL, 16},
    {"MOT VIEWER", "tools.rel", NULL, 17},
    {"TPL VIEWER", "tools.rel", NULL, 18},
    {"INT DESIGN", "t_id.rel", NULL, 20},
    {"SCENARIO ATARI", "t_sce.rel", NULL, 19},
    {"FLOOR ATARI", "tools.rel", NULL, 21},
    {"LOG VIEWER", NULL, ToolLogView, 22},
    {"SOUND TABLE EDIT", "t_movie.rel", NULL, 23},
    {"SE ATARI EDIT", "t_movie.rel", NULL, 24},
    {"SCREEN SHOT TOOL", NULL, ToolScreenShot, 26},
    {"MESSAGE TEST", "tools.rel", NULL, 27},
    {"EVENT TOOL", "t_event.rel", NULL, 30},
    {"BLOCK AREA TOOL", "t_sce.rel", NULL, 31},
    {"ESP AREA TOOL", "tools.rel", NULL, 32},
    {"ITEM SET TOOL", "t_sce.rel", NULL, 34},
    {"EM INFO TOOL", "tools.rel", NULL, 35},
    {"LIGHT AREA TOOL", "tools.rel", NULL, 36},
    {"BUGCHECK TOOL", NULL, ToolBugcheck, 38},
    {"EXIT", NULL, NULL, 3},
};

test test;
int DebugMenuSelected;
void DbmenuModuleInit();
static DllModule* pModule;
void* pModule_bss;

void init(struct test* t);
static void exit(struct test* t);
void move(struct test* t);

// Menu index of the tool called `name`; -1 when unknown.
int dbMenuGetMenuNo(const char* name)
{
    int i;
    int n = sizeof(menu) / sizeof(menu[0]);
    for (i = 0; i < n; i++) {
        if (strcmp(menu[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// The menu task: init, then move() every frame.
void MenuTask()
{
    struct test* t = &test;
    init(t);
    TaskSleep(1);
    while (1) {
        move(t);
        TaskSleep(1);
    }
}

// Opens the menu: sets Debug_flg[0] bit31 (menu active), freezes the game (Stop_flg saved),
// switches debug_mode to the menu page and starts MenuTask.
void DbMenuExec()
{
    struct test* t = &test;
    DbgFlagOn(pG, DBG_TEST_MODE);
    t->stop_bak = pG->Stop_flg;
    BitOn(pG->Stop_flg, ~0x4000);
    pG->debug_disp = pG->debug_mode;
    pG->debug_mode = 1;
    if (SysFlagChk(pG, SYS_SN_PC_READ_TOOL)) {
        if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
            SysFlagOn(pG, SYS_SN_PC_READ);
            t->flag = 1;
        } else {
            t->flag = 0;
        }
    }
    SetDebugAlloc();
    TaskExec(2, MenuTask, 0);
}

// Main loop hook: when the menu / tool has ended (Debug_flg[0] bit31 cleared) restores the
// debug page, unlinks the tool module and, when a tool was queued by DbMenuSetExecTool, re-opens
// the menu to run it.
void DbMenuExitAfterCheck()
{
    struct test* t = &test;
    if (!DbgFlagChk(pG, DBG_TEST_MODE_CK)) {
        if (DbgFlagChk(pG, DBG_TEST_MODE)) {
            DbgFlagOn(pG, DBG_TEST_MODE_CK);
        }
        if (t->exit_wait > 0) {
            t->exit_wait--;
        }
        return;
    }
    if (DbgFlagChk(pG, DBG_TEST_MODE)) {
        return;
    }
    if (!(pG->debug_disp & 0x80)) {
        pG->debug_mode = pG->debug_disp;
        pG->debug_disp = -1;
    }
    if (SysFlagChk(pG, SYS_SN_PC_READ_TOOL) && t->flag == 1) {
        SysFlagOff(pG, SYS_SN_PC_READ);
    }
    DbmenuModuleInit();
    ResetDebugAlloc();
    t->exit_wait = 30;
    DbgFlagOff(pG, DBG_TEST_MODE_CK);
    if (t->exec_tool == 1) {
        DbMenuExec();
    }
}

// Queues tool `name` to be started by the next menu open (other debug code jumps into a tool).
void DbMenuSetExecTool(const char* name)
{
    struct test* t = &test;
    strcpy(t->name, name);
    t->exec_tool = 1;
}

// 1 while the menu or a tool is active.
int DbMenuActiveCheck()
{
    if (test.exit_wait > 0) {
        return 1;
    }
    return 0;
}

// Restores the Stop_flg saved when the menu opened (the tool runs the game underneath).
void DbMenuRestoreStopFlag()
{
    struct test* t = &test;
    if (t->stop_saved == 1) {
        BitSet(pG->Stop_flg, t->stop_bak);
        SpfFlagOff(pG, SPF_KEY);
        t->stop_saved = 0;
    }
}

// Room start: forgets a queued tool.
void DbMenuRoomInit()
{
    test.exit_wait = 0;
    test.exec_tool = 0;
}

// Menu setup: cursor / position, and a queued tool selects itself.
void init(struct test* t)
{
    int no;
    FadeKill(0);
    FadeKill(FADE_NO_SCENARIO);
    t->x = 176;
    t->y = 30;
    t->x3 = 0;
    t->x2 = 0;
    t->x1 = 0;
    t->x0 = 0;
    t->cursor = 0;
    t->stop_saved = 0;
    DebugMenuSelected = -1;
    if (t->exec_tool == 1) {
        no = dbMenuGetMenuNo(t->name);
        if (no >= 0) {
            t->cursor = no;
        } else {
            t->exec_tool = 0;
        }
    }
}

// Closes the menu: Stop_flg restored, menu flag cleared, task ends.
static void exit(struct test* t)
{
    BitSet(pG->Stop_flg, t->stop_bak);
    DbgFlagOff(pG, DBG_TEST_MODE);
    TaskExit();
}

// Menu per frame: draws the tool list with the cursor, C-stick / D-pad moves it, B closes, A (or
// a queued tool) starts the tool: a REL tool is read from "tools/<name>" into the debug heap,
// linked and its prolog run; a built-in tool is chained as the task.
void move(struct test* t)
{
    JOY* joy;
    int i;
    int n = sizeof(menu) / sizeof(menu[0]);
    int color;
    int h, m, s;

    joy = GetBugCheckController();
    t->x += joy->substickX / 16;
    t->y -= joy->substickY / 16;
    GetGameTime(&h, &m, &s);
    eprintf(t->x, t->y, 0, 0, "WELCOME TO TOOL MENU");
    eprintf(t->x + 160, t->y + 405, 0, 0, "MOVE BY SUB-STICK");
    for (i = 0; i < n; i++) {
        color = ((i / 2) & 1) ? 0x18 : 0;
        eprintf(t->x + ((i & 1) * 20 - 4) * 8, t->y + (i / 2 + 2) * 15, color, 0, "%s", menu[i].name);
    }
    eprintf(t->x + ((t->cursor & 1) * 20 - 5) * 8, t->y + (t->cursor / 2 + 2) * 15, 0, 0, ">");
    if (joy->rep & 0x80008) {
        t->cursor -= 2;
        if (t->cursor < 0) {
            // COMPILER-DIFF: candidate (jump2-only deleted arm). The target keeps a dead
            // `andi. r11,r9,1` (cursor & 1 on the register) whose jump was deleted in jump2, then
            // re-reads cursor for the value-form test `xori; andi.; beq; li r0,1; cmpwi; li 32; bne;
            // li 33` = `(!(cursor & 1) && !(n & 1)) ? n - 2 : n - 1` (n the local: cprop folds the
            // second operand to `li 1`, combine leaves the compare). The parity test's arm must hold
            // an insn flow2 keeps and jump2 removes: two identical codeless "=m" asms in both arms
            // are cross-jumped, the condjump becomes a jump-to-next and only the compare survives;
            // their memory output also keeps cse from folding the re-read (fresh `lbz`). Both asms must
            // sit on ONE source line: ASM_OPERANDS carries the line number and rtx_equal_p compares it.
            if (t->cursor & 1) { asm("" : "=m"(t->cursor)); } else { asm("" : "=m"(t->cursor)); }
            t->cursor = (!(t->cursor & 1) && !(n & 1)) ? n - 2 : n - 1;
        }
    }
    if (joy->rep & 0x40004) {
        t->cursor += 2;
        if (t->cursor >= n) {
            t->cursor = t->cursor & 1;
        }
    }
    if (joy->rep & 0x30003) {
        t->cursor ^= 1;
        if (t->cursor >= n) {
            t->cursor ^= 1;
        }
    }
    if (joy->trg & 0x1200) {
        exit(t);
    }
    if ((joy->trg & 0x100) || t->exec_tool == 1) {
        t->stop_saved = 1;
        t->exec_tool = 0;
        SpfFlagOff(pG, SPF_KEY);
        if (menu[t->cursor].func == NULL && menu[t->cursor].rel_name == NULL) {
            exit(t);
        }
        DebugMenuSelected = menu[t->cursor].id;
        if (menu[t->cursor].rel_name != NULL) {
            char buf[32] = "rel/";
            int req;
            strcat(buf, menu[t->cursor].rel_name);
#line 397 "D:/Bio4/Prog/db_menu.cpp"
            req = DvdReadN(buf, NULL, 0, 0, 0, 3, __FILE__, __LINE__);
            if (Dvd.ReadCheck(req, NULL, NULL, (void**) &pModule) >= 0) {
                if (pModule->bssSize != 0) {
                    pModule_bss = Debug_alloc(pModule->bssSize, 1);
                } else {
                    pModule_bss = NULL;
                }
                DLL_Link(pModule, pModule_bss);
                TaskChain(pModule->prolog, 0);
            } else {
                pLog->err(0, 0, "%s FILE NOT FOUND", menu[t->cursor].rel_name);
                exit(t);
            }
        } else {
            TaskChain(menu[t->cursor].func, 0);
        }
    }
}

// Unlinks and frees the loaded tool REL (and its bss).
void DbmenuModuleInit()
{
    if (pModule != NULL) {
        pModule->epilog();
        DLL_Unlink(pModule);
        Debug_free(pModule);
        pModule = NULL;
        if (pModule_bss != NULL) {
            Debug_free(pModule_bss);
            pModule_bss = NULL;
        }
    }
}
