#include "types.h"
#include "global.h"
#include "main.h"
#include "joy.h"
#include "pad.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "file.h"
#include "db_log.h"
#include "t_prim.h"
#include "t_util.h"

// Pad vibration pattern editor (Tools/t_vib.cpp): 64 patterns of up to 16 keys (VibDataEntry), edited
// on a level/frame grid and saved as the room's .vib file.

extern "C" {
int sprintf(char* dst, const char* fmt, ...);
}

struct TvibData {
    u32 num;             // 0x00
    VibDataEntry e[16];  // 0x04
};

struct TvibFile {
    u32 num;      // 0x00  64
    u32 ofs[64];  // 0x04  offset of each pattern from the file start (0 = empty)
};

struct TvibWork {
    u32 dispFlag;         // 0x00  1 edit grid, 2 edit key, 4 menu, 8 loop test
    int mode;             // 0x04  routine (tvibFunc)
    int cursor;           // 0x08
    int listNo;           // 0x0C  pattern being edited
    int editNo;           // 0x10  key being edited
    int editSide;         // 0x14  0 start / 1 end of the key
    int level;            // 0x18
    int frame;            // 0x1C
    int scroll;           // 0x20  first frame shown on the grid
    int testFrame;        // 0x24
    int loopFrame;        // 0x28
    int pad_2C;
    int stage;            // 0x30
    int room;             // 0x34
    char path[0x100];     // 0x38
    TvibData list[64];    // 0x138
    TvibData copy;        // 0x2238
    u8 file[0x2204];      // 0x22BC  save/load image
};

// The work pointer is a struct member: every store through it reloads the pointer.
struct TvibWorkPtr {
    TvibWork* p;
};

static TvibWorkPtr tvib = {0};
#define V tvib.p

void ToolVibEdit();
void tvibInit();
void tvibExit();
static void tvib_R0_Select();
static void tvib_R0_SelectMenu();
static void tvib_R0_EditMenu();
static void tvib_R0_Edit();
static void tvib_R0_EditVib();
static void tvib_R0_VibLoopSet();
static void tvib_R0_VibLoopTest();
static void tvib_R0_Save();
static void tvib_R0_Load();
void tvibGetFileNameLocal(int core);
void tvibGetFileNameServer(int core);
void tvibMainFrameDisp();
void tvibSelectMenuDisp();
void tvibSaveMenuDisp();
void tvibLoadMenuDisp();
void tvibEditMenuDisp();
void tvibVibLoopSetDisp();
void tvibEditFrameDisp();
void tvibModeFrameDisp();
void tvibFrameVibDraw(TvibData* d);
void tvibFrameLineDraw(VibDataEntry* e, u32 col);
void tvibFrameMarkDraw(int lv, int frame, u32 col, u32 size);
void tvibListVibDraw();
void tvibListLineDraw(VibDataEntry* e, u32 col, int x, int y);
void tvibFileSave(const char* path);
void tvibFileLoad(const char* path);

// Tool routines by TvibWork::mode: 0 pattern list, 1 grid edit, 2 key edit, 3 list menu, 4 edit
// menu, 5 loop test setup, 6 loop test, 7 save, 8 load.
static void (*tvibFunc[9])() = {
    tvib_R0_Select,  tvib_R0_Edit,       tvib_R0_EditVib,     tvib_R0_SelectMenu, tvib_R0_EditMenu,
    tvib_R0_VibLoopSet, tvib_R0_VibLoopTest, tvib_R0_Save,    tvib_R0_Load,
};

static char menuName[5][16] = {
    "CANCEL    ", "SAVE      ", "LOAD      ", "LIST CLEAR", "EXIT      ",
};

static s16 list_x = 32;
static s16 list_y = 64;
static s16 menu_x = 64;
static s16 menu_y = 260;
static s16 cell_w = 14;
static s16 cell_h = 16;
static s16 frame_w = 30;

// list cell position of entry i (8 rows per column)
#define LIST_X(i) (list_x + (s16) ((i) / 8) * 230)
#define LIST_Y(i) (list_y + (s16) ((i) % 8) * 20)


// Store through a reference: keeps the following global (pG) load below the store.
static inline void ISet(int& d, int v) { d = v; }

// Vibration editor entry (debug menu 15): init, then tvibFunc[mode] and the display every frame.
void ToolVibEdit()
{
    tvibInit();
    for (;;) {
        V->dispFlag &= ~0xF;
        tvibFunc[V->mode]();
        TprimDraw2D(0);
        tvibMainFrameDisp();
        tvibEditFrameDisp();
        tvibModeFrameDisp();
        TaskSleep(1);
    }
}

// Tool start: work on the Debug heap (error text when it fails), default flags, current stage /
// room, pattern list mode.
void tvibInit()
{
    TvibWork*& wp = tvib.p;

    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    TOOL_FLAG(OFS_STOP_FLG) |= 0x800000;
    CfgFlagOn(pSys, CFG_VIBRATION);
    wp = (TvibWork*) Debug_alloc(sizeof(TvibWork), 1);
    if (wp == NULL) {
        u32 i;

        for (i = 0; i < 90; i++) {
            eprintf(100, 100, 0, 0, "MEMORY ALLOCATE ERROR");
            TaskSleep(1);
        }
        tvibExit();
    }
    V->mode = 0;
    V->cursor = 0;
}

// Frees the work, restores the flags, ends the task.
void tvibExit()
{
    TOOL_FLAG(OFS_STOP_FLG) &= ~0x800000;
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
}

// Mode 0, the 64-pattern list (8 rows per column): d-pad moves, A edits the pattern, START opens
// the list menu, X plays the pattern on the pad (L+X sets up a loop test), Y copies / L+Y pastes
// a pattern, B stops the vibration.
static void tvib_R0_Select()
{
    TvibData* d;

    if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
        V->listNo--;
    }
    if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
        V->listNo++;
    }
    if (Joy[0].rep & (JOY_LEFT | 0x10000)) {
        if (V->listNo > 7) {
            V->listNo -= 8;
        }
    }
    if (Joy[0].rep & (JOY_RIGHT | 0x20000)) {
        if (V->listNo < 56) {
            V->listNo += 8;
        }
    }
    while (V->listNo < 0) {
        V->listNo = 0;
    }
    while (V->listNo > 63) {
        V->listNo = 63;
    }
    if (Joy[0].trg & JOY_START) {
        V->mode = 3;
        V->cursor = 0;
        return;
    }
    if (Joy[0].trg & JOY_A) {
        V->editNo = 0;
        V->mode = 1;
        V->cursor = 0;
        return;
    }
    d = &V->list[V->listNo];
    if ((Joy[0].on & JOY_L) && (Joy[0].trg & JOY_X) && d->num != 0) {
        u32 i;

        V->testFrame = 0;
        for (i = 0; i < d->num; i++) {
            VibDataEntry* e = &d->e[i];
            int end = e->wait + e->time;

            if (V->testFrame < end) {
                V->testFrame = end;
            }
        }
        V->loopFrame = V->testFrame;
        V->mode = 5;
        V->cursor = 0;
        return;
    } else if ((Joy[0].trg & JOY_X) && d->num != 0) {
        VibSetDataCore((VibData*) d, 8);
    }
    if (Joy[0].trg & JOY_B) {
        VibSetClearType(8);
    }
    if (!(Joy[0].on & JOY_L) && (Joy[0].trg & JOY_Y)) {
        V->copy = *d;
    }
    if ((Joy[0].on & JOY_L) && (Joy[0].trg & JOY_Y)) {
        *d = V->copy;
    }
    if ((Joy[0].on & JOY_L) && (Joy[0].trg & JOY_Z)) {
        d->num = 0;
    }
}

// Mode 3, the list menu: CANCEL / SAVE / LOAD / LIST CLEAR / EXIT.
static void tvib_R0_SelectMenu()
{
    V->dispFlag |= 4;
    if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
        V->cursor--;
        if (V->cursor < 0) {
            V->cursor = 4;
        }
    }
    if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
        V->cursor++;
        if (V->cursor > 4) {
            V->cursor = 0;
        }
    }
    if (Joy[0].trg & (JOY_B | JOY_START)) {
        V->mode = 0;
        V->cursor = 0;
        return;
    }
    if (Joy[0].trg & JOY_A) {
        switch (V->cursor) {
        case 0:
            V->mode = 0;
            V->cursor = 0;
            return;
        case 1:
            ISet(V->stage, pG->stage_no);
            ISet(V->room, pG->room_no);
            V->mode = 7;
            V->cursor = 0;
            break;
        case 2:
            ISet(V->stage, pG->stage_no);
            ISet(V->room, pG->room_no);
            V->mode = 8;
            V->cursor = 0;
            break;
        case 3:
            break;
        case 4:
            tvibExit();
            return;
        }
    }
    tvibSelectMenuDisp();
}

// Mode 4, the edit menu (START in the grid): B / START back to the grid.
static void tvib_R0_EditMenu()
{
    V->dispFlag |= 4;
    if (Joy[0].trg & (JOY_B | JOY_START)) {
        V->mode = 1;
        V->cursor = 0;
    } else {
        tvibEditMenuDisp();
    }
}

// Mode 1, the level/frame grid of one pattern: up/down level 0..8, left/right frame (X x30);
// Y adds a key at the cursor (its end then edited), Z deletes the current key, L/R pick a key,
// A grabs the key start / end under the cursor (key edit mode), X plays the pattern, B back to
// the list, START the edit menu.
static void tvib_R0_Edit()
{
    TvibData* d = &V->list[V->listNo];
    VibDataEntry* e;
    int i;

    V->dispFlag |= 1;
    if ((Joy[0].rep & JOY_UP) || (Joy[0].on & JOY_SUP)) {
        V->level++;
    }
    if ((Joy[0].rep & JOY_DOWN) || (Joy[0].on & JOY_SDOWN)) {
        V->level--;
    }
    if (V->level < 0) {
        V->level = 0;
    }
    if (V->level > 8) {
        V->level = 8;
    }
    if ((Joy[0].rep & JOY_LEFT) || (Joy[0].on & 0x10000)) {
        if (Joy[0].on & JOY_X) {
            V->frame -= 30;
        } else {
            V->frame -= 1;
        }
    }
    if ((Joy[0].rep & JOY_RIGHT) || (Joy[0].on & 0x20000)) {
        if (Joy[0].on & JOY_X) {
            V->frame += 30;
        } else {
            V->frame += 1;
        }
    }
    if (V->frame < 0) {
        V->frame = 0;
    }
    if (V->frame > 1998) {
        V->frame = 1998;
    }
    if (Joy[0].trg & JOY_Y) {
        if (d->num < 16) {
            e = &d->e[d->num];
            e->type = 0;
            e->wait = V->frame;
            e->time = 1;
            e->lvl0 = V->level;
            e->lvl1 = V->level;
            V->editNo = d->num;
            d->num++;
            V->editSide = 1;
            V->mode = 2;
            V->cursor = 0;
            return;
        }
    }
    if ((Joy[0].trg & JOY_Z) && d->num != 0) {
        u32 j;

        for (j = V->editNo; j < d->num - 1; j++) {
            d->e[j] = d->e[j + 1];
        }
        d->num--;
        V->editNo--;
    }
    if (Joy[0].rep & JOY_L) {
        V->editNo--;
    }
    if (Joy[0].rep & JOY_R) {
        V->editNo++;
    }
    if (V->editNo < 0) {
        V->editNo = 0;
    }
    if (V->editNo >= (int) d->num) {
        V->editNo = d->num - 1;
    }
    if (d->num == 0) {
        V->editNo = 0;
    }
    if (Joy[0].rep & (JOY_L | JOY_R)) {
        e = &d->e[V->editNo];
        V->frame = e->wait;
        V->level = e->lvl0;
    }
    if ((Joy[0].trg & JOY_A) && d->num != 0) {
        for (i = 0; i < (int) d->num; i++) {
            e = &d->e[i];
            if (e->lvl0 == V->level && e->wait == V->frame) {
                V->editNo = i;
                V->editSide = 0;
                break;
            }
            if (e->lvl1 == V->level && e->wait + e->time == V->frame) {
                V->editNo = i;
                V->editSide = 1;
                break;
            }
        }
        if (i >= (int) d->num) {
            e = &d->e[V->editNo];
            V->level = e->lvl0;
            V->frame = e->wait;
            V->editSide = 0;
        } else {
            V->mode = 2;
            V->cursor = 0;
        }
    } else if (Joy[0].trg & JOY_B) {
        V->mode = 0;
        V->cursor = 0;
    } else if (Joy[0].trg & JOY_START) {
        V->mode = 4;
        V->cursor = 0;
    }
}

// Mode 2, one key: L/R select its start / end; up/down change that side's level, left/right move
// the start (keeping the end) or stretch the end; X plays; A/B/Y back to the grid.
static void tvib_R0_EditVib()
{
    TvibData* d = &V->list[V->listNo];
    VibDataEntry* e = &d->e[V->editNo];
    u16 end;

    V->dispFlag |= 3;
    if (V->cursor == 0) {
        if (V->editSide == 0) {
            V->level = e->lvl0;
            V->frame = e->wait;
        } else {
            V->level = e->lvl1;
            V->frame = e->wait + e->time;
        }
        V->cursor++;
    }
    if (Joy[0].trg & JOY_L) {
        V->editSide = 0;
        V->level = e->lvl0;
        V->frame = e->wait;
    }
    if (Joy[0].trg & JOY_R) {
        V->editSide = 1;
        V->level = e->lvl1;
        V->frame = e->wait + e->time;
    }
    end = e->wait + e->time;
    if (V->editSide == 0) {
        if ((Joy[0].rep & JOY_UP) || (Joy[0].on & JOY_SUP)) {
            if (e->lvl0 < 8) {
                e->lvl0++;
            }
        }
        if ((Joy[0].rep & JOY_DOWN) || (Joy[0].on & JOY_SDOWN)) {
            if (e->lvl0 != 0) {
                e->lvl0--;
            }
        }
        if ((Joy[0].rep & JOY_LEFT) || (Joy[0].on & 0x10000)) {
            if (e->wait != 0) {
                e->wait--;
                e->time++;
            }
        }
        if ((Joy[0].rep & JOY_RIGHT) || (Joy[0].on & 0x20000)) {
            if (e->wait < end - 1) {
                e->wait++;
                e->time--;
            }
        }
    } else {
        if ((Joy[0].rep & JOY_UP) || (Joy[0].on & JOY_SUP)) {
            if (e->lvl1 < 8) {
                e->lvl1++;
            }
        }
        if ((Joy[0].rep & JOY_DOWN) || (Joy[0].on & JOY_SDOWN)) {
            if (e->lvl1 != 0) {
                e->lvl1--;
            }
        }
        if ((Joy[0].rep & JOY_LEFT) || (Joy[0].on & 0x10000)) {
            if (e->time > 1) {
                e->time--;
            }
        }
        if ((Joy[0].rep & JOY_RIGHT) || (Joy[0].on & 0x20000)) {
            if (end <= 1998) {
                e->time++;
            }
        }
    }
    if (V->editSide == 0) {
        V->level = e->lvl0;
        V->frame = e->wait;
    } else {
        V->level = e->lvl1;
        V->frame = e->wait + e->time;
    }
    if (Joy[0].trg & JOY_X) {
        if (e->type & 0x8000) {
            e->type &= ~0x8000;
        } else {
            e->type |= 0x8000;
        }
    }
    if (Joy[0].trg & (JOY_A | JOY_B | JOY_Y)) {
        V->mode = 1;
        V->cursor = 0;
    }
}

// Mode 5: left/right set the loop length in frames (1..1999); A/X start the loop test, B/START
// back.
static void tvib_R0_VibLoopSet()
{
    // the work pointer is read BEFORE the first test (`lwz r11,tvib@l` above the `beq`, and the PRE'd
    // high stays the reaching register at every later site: with `V->` in the arm ours had a fresh
    // `lis` per arm); the other statements re-read V through the macro
    TvibWork* v = V;

    if (Joy[0].rep & (JOY_RIGHT | 0x20000)) {
        v->loopFrame++;
    }
    if (Joy[0].rep & (JOY_LEFT | 0x10000)) {
        V->loopFrame--;
    }
    if (V->loopFrame < 1) {
        V->loopFrame = 1;
    }
    if (V->loopFrame > 1999) {
        V->loopFrame = 1999;
    }
    V->dispFlag |= 4;
    if (Joy[0].trg & (JOY_B | JOY_START)) {
        V->mode = 0;
        V->cursor = 0;
    } else if (Joy[0].trg & (JOY_A | JOY_X)) {
        V->mode = 6;
        V->cursor = 0;
        V->testFrame = V->loopFrame;
    } else {
        tvibVibLoopSetDisp();
    }
}

// Mode 6: replays the pattern every loopFrame frames (the grid cursor follows); B stops it.
static void tvib_R0_VibLoopTest()
{
    TvibData* d = &V->list[V->listNo];

    V->testFrame++;
    if (V->testFrame >= V->loopFrame) {
        V->testFrame = 0;
        VibSetDataCore((VibData*) d, 8);
    }
    V->dispFlag |= 9;
    V->frame = V->testFrame;
    V->level = 0;
    if (Joy[0].trg & 0x1F00) {
        V->mode = 5;
        V->cursor = 0;
        VibSetClearType(8);
    }
}

// Mode 7, SAVE: rows server / local (x: / d:), core or room file, stage and room numbers; A writes
// the .vib, B/START back to the list menu.
static void tvib_R0_Save()
{
    V->dispFlag |= 4;
    switch (V->cursor) {
    case 0:
        if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
            V->cursor = 1;
        }
        break;
    case 1:
        if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
            V->cursor = 0;
        }
        if (Joy[0].rep & (JOY_RIGHT | 0x20000)) {
            V->cursor = 2;
        }
        break;
    case 2:
        if (Joy[0].rep & (JOY_LEFT | 0x10000)) {
            V->cursor = 1;
        }
        if (Joy[0].rep & (JOY_RIGHT | 0x20000)) {
            V->cursor = 3;
        }
        if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
            V->stage++;
        }
        if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
            V->stage--;
        }
        break;
    case 3:
        if (Joy[0].rep & (JOY_LEFT | 0x10000)) {
            V->cursor = 2;
        }
        if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
            V->room++;
        }
        if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
            V->room--;
        }
        break;
    }
    if (V->stage < 0) {
        V->stage = 0;
    }
    if (V->room < 0) {
        V->room = 0;
    }
    if (Joy[0].trg & (JOY_B | JOY_START)) {
        V->mode = 3;
        V->cursor = 0;
    } else {
        tvibGetFileNameServer(V->cursor);
        if (Joy[0].trg & JOY_A) {
            tvibFileSave(V->path);
            tvibGetFileNameLocal(V->cursor);
            tvibFileSave(V->path);
            V->mode = 3;
            V->cursor = 0;
        } else {
            tvibSaveMenuDisp();
        }
    }
}

// Mode 8, LOAD: same rows as SAVE; A reads the .vib.
static void tvib_R0_Load()
{
    V->dispFlag |= 4;
    switch (V->cursor) {
    case 0:
        if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
            V->cursor = 1;
        }
        break;
    case 1:
        if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
            V->cursor = 0;
        }
        if (Joy[0].rep & (JOY_RIGHT | 0x20000)) {
            V->cursor = 2;
        }
        break;
    case 2:
        if (Joy[0].rep & (JOY_LEFT | 0x10000)) {
            V->cursor = 1;
        }
        if (Joy[0].rep & (JOY_RIGHT | 0x20000)) {
            V->cursor = 3;
        }
        if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
            V->stage++;
        }
        if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
            V->stage--;
        }
        break;
    case 3:
        if (Joy[0].rep & (JOY_LEFT | 0x10000)) {
            V->cursor = 2;
        }
        if (Joy[0].rep & (JOY_UP | JOY_SUP)) {
            V->room++;
        }
        if (Joy[0].rep & (JOY_DOWN | JOY_SDOWN)) {
            V->room--;
        }
        break;
    }
    if (V->stage < 0) {
        V->stage = 0;
    }
    if (V->room < 0) {
        V->room = 0;
    }
    if (Joy[0].trg & (JOY_B | JOY_START)) {
        V->mode = 3;
        V->cursor = 0;
    } else {
        tvibGetFileNameServer(V->cursor);
        if (Joy[0].trg & JOY_A) {
            tvibFileLoad(V->path);
            V->mode = 3;
            V->cursor = 0;
        } else {
            tvibLoadMenuDisp();
        }
    }
}

// d:\bio4/room/etc/core/core.vib or d:\bio4/room/st<n>/r<room>/r<room>.vib.
void tvibGetFileNameLocal(int core)
{
    if (core == 0) {
        sprintf(V->path, "%s/room/etc/core/core.vib", "d:\\bio4");
    } else {
        sprintf(V->path, "%s/room/st%1x/r%1x%02x/r%1x%02x.vib", "d:\\bio4", V->stage, V->stage, V->room, V->stage,
                V->room);
    }
}

// x:\soft/Room/etc/core/core.vib or x:\soft/Room/st<n>/r<room>/r<room>.vib.
void tvibGetFileNameServer(int core)
{
    if (core == 0) {
        sprintf(V->path, "%s/Room/etc/core/core.vib", "x:\\soft");
    } else {
        sprintf(V->path, "%s/Room/st%1x/r%1x%02x/r%1x%02x.vib", "x:\\soft", V->stage, V->stage, V->room, V->stage,
                V->room);
    }
}

// Draws the frame of the current mode: the pattern list or the edit grid with the key graph, the
// cursor and the help lines.
void tvibMainFrameDisp()
{
    S16Vec v[4];
    u32 col;
    s16 i;
    s16 page;
    s16 top;
    s16 x;
    s16 y;

    eprintf2(10, 16, 180, 20, 0, 0, "VIBRATION EDITOR");
    eprintf2(8, 14, 320, 42, 0, 0, "-START Menu & HELP-");
    eprintf2(8, 14, list_x, list_y - 20, 0, 0, "List [ %02d / 64 ]", V->listNo);
    page = V->listNo % 16;
    top = (u16) V->listNo - page;
    for (i = 0; i < 16 && top + i < 64; i++) {
        if (V->list[top + i].num != 0) {
            col = 0;
        } else {
            col = 7;
        }
        eprintf2(8, 14, LIST_X(i), LIST_Y(i), ((GXColor*) &col)->a, 0, "0x%02x:", top + i);
    }
    v[0].x = 0;
    v[0].y = 0;
    v[0].z = 0;
    v[1].x = 512;
    v[1].y = 0;
    v[1].z = 0;
    v[2].x = 512;
    v[2].y = 448;
    v[2].z = 0;
    col = 0;
    TprimDrawPolyFn_s16(v, (GXColor*) &col, 3);
    v[0].x = 0;
    v[0].y = 0;
    v[0].z = 0;
    v[1].x = 512;
    v[1].y = 448;
    v[1].z = 0;
    v[2].x = 0;
    v[2].y = 448;
    v[2].z = 0;
    col = 0;
    TprimDrawPolyFn_s16(v, (GXColor*) &col, 3);
    col = 0x60606060;
    y = list_y - 2;
    for (i = 0; i < 3; i++) {
        x = (list_x - 2) + i * 230;
        v[0].x = x;
        v[0].y = y;
        v[0].z = 0;
        v[1].x = x;
        v[1].y = y + 160;
        v[1].z = 0;
        TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
    }
    x = list_x - 2;
    for (i = 0; i < 9; i++) {
        y = (list_y - 2) + i * 20;
        v[0].x = x;
        v[0].y = y;
        v[0].z = 0;
        v[1].x = x + 460;
        v[1].y = y;
        v[1].z = 0;
        TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
    }
    x = list_x + (s16) (page / 8) * 230 - 1;
    y = list_y + (s16) (page % 8) * 20 - 1;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;
    v[1].x = x + 228;
    v[1].y = y;
    v[1].z = 0;
    v[2].x = x + 228;
    v[2].y = y + 18;
    v[2].z = 0;
    v[3].x = x;
    v[3].y = y + 18;
    v[3].z = 0;
    col = 0xFF404080;
    TprimDrawFrameFn_s16(v, (GXColor*) &col, 4);
    tvibListVibDraw();
}

// Draws the list menu rows with the cursor.
void tvibSelectMenuDisp()
{
    s16 x = menu_x;
    s16 y = menu_y;
    s16 i;

    eprintf2(8, 14, x, y, 0, 0, "-- MENU --------");
    x += 8;
    for (i = 0; i < 5; i++) {
        y += 16;
        eprintf(x, y, (V->cursor == i) ? 4 : 0, 0, "%s", menuName[i]);
        if (V->cursor == i) {
            eprintf(x - 8, y, 0, 0, ">");
        }
    }
    x = menu_x + 200;
    y = menu_y;
    eprintf2(8, 14, x, y, 0, 0, "-- HELP --------");
    eprintf2(8, 14, x, y + 16, 0, 0, "A:   SELECT LIST");
    eprintf2(8, 14, x, y + 32, 0, 0, "X:   TEST VIBRATION");
    eprintf2(8, 14, x, y + 48, 0, 0, "L+X: TEST VIBRATION LOOP");
    eprintf2(8, 14, x, y + 64, 0, 0, "B:   VIBRATION STOP");
    eprintf2(8, 14, x, y + 80, 0, 0, "Y:   COPY DATA");
    eprintf2(8, 14, x, y + 96, 0, 0, "L+Y: PASTE DATA");
    eprintf2(8, 14, x, y + 112, 0, 0, "L+Z: DELETE DATA ");
}

// Draws the SAVE rows (server / local, core / room, stage, room, path).
void tvibSaveMenuDisp()
{
    s16 x = menu_x + 200;
    s16 y = menu_y;
    int y1;
    int y2;

    eprintf2(8, 14, x, y, 0, 0, "-- HELP --------");
    eprintf2(8, 14, x, y + 16, 0, 0, "A:   Save");
    eprintf2(8, 14, x, y + 32, 0, 0, "B:   Cancel");
    x = menu_x;
    y = menu_y;
    eprintf2(8, 14, x, y, 0, 0, "-- SAVE --------");
    eprintf2(8, 14, x, y + 64, 0, 0, "%s", V->path);
    if (V->cursor == 0) {
        y1 = y + 16;
        eprintf2(8, 14, x, y1, 4, 0, "CORE");
        y2 = y + 32;
        eprintf2(8, 14, x, y2, 0, 0, "ROOM");
        eprintf2(8, 14, x + 48, y2, 0, 0, "Stage=%01x,  Room=%02x", V->stage, V->room);
    } else {
        y1 = y + 16;
        eprintf2(8, 14, x, y1, 0, 0, "CORE");
        y2 = y + 32;
        eprintf2(8, 14, x, y2, 4, 0, "ROOM");
        eprintf2(8, 14, x + 48, y2, 4, 0, "Stage=%01x,  Room=%02x", V->stage, V->room);
    }
    switch (V->cursor) {
    case 0:
        x -= 8;
        y = y1;
        break;
    case 1:
        x -= 8;
        y = y2;
        break;
    case 2:
        x += 40;
        y = y2;
        break;
    case 3:
        x += 120;
        y = y2;
        break;
    }
    eprintf2(8, 14, x, y, 4, 0, ">");
}

// Draws the LOAD rows.
void tvibLoadMenuDisp()
{
    s16 x = menu_x + 200;
    s16 y = menu_y;
    int y1;
    int y2;

    eprintf2(8, 14, x, y, 0, 0, "-- HELP --------");
    eprintf2(8, 14, x, y + 16, 0, 0, "A:   Load");
    eprintf2(8, 14, x, y + 32, 0, 0, "B:   Cancel");
    x = menu_x;
    y = menu_y;
    eprintf2(8, 14, x, y, 0, 0, "-- LOAD --------");
    eprintf2(8, 14, x, y + 64, 0, 0, "%s", V->path);
    if (V->cursor == 0) {
        y1 = y + 16;
        eprintf2(8, 14, x, y1, 4, 0, "CORE");
        y2 = y + 32;
        eprintf2(8, 14, x, y2, 0, 0, "ROOM");
        eprintf2(8, 14, x + 48, y2, 0, 0, "Stage=%01x,  Room=%02x", V->stage, V->room);
    } else {
        y1 = y + 16;
        eprintf2(8, 14, x, y1, 0, 0, "CORE");
        y2 = y + 32;
        eprintf2(8, 14, x, y2, 4, 0, "ROOM");
        eprintf2(8, 14, x + 48, y2, 4, 0, "Stage=%01x,  Room=%02x", V->stage, V->room);
    }
    switch (V->cursor) {
    case 0:
        x -= 8;
        y = y1;
        break;
    case 1:
        x -= 8;
        y = y2;
        break;
    case 2:
        x += 40;
        y = y2;
        break;
    case 3:
        x += 120;
        y = y2;
        break;
    }
    eprintf2(8, 14, x, y, 4, 0, ">");
}

// Draws the edit menu.
void tvibEditMenuDisp()
{
    s16 x = menu_x + 200;
    s16 y = menu_y;

    eprintf2(8, 14, x, y, 0, 0, "-- HELP --------");
    eprintf2(8, 14, x, y + 16, 0, 0, "A:   Edit select vib");
    eprintf2(8, 14, x, y + 32, 0, 0, "Y:   Make a new vib");
    eprintf2(8, 14, x, y + 48, 0, 0, "B:   Return List");
    eprintf2(8, 14, x, y + 64, 0, 0, "Z:   Delete select vib");
    eprintf2(8, 14, x, y + 80, 0, 0, "Xon: 30 frame skip mode");
    eprintf2(8, 14, x, y + 96, 0, 0, "R:   Next vib");
    eprintf2(8, 14, x, y + 112, 0, 0, "L:   Back vib");
}

// Draws the loop length row.
void tvibVibLoopSetDisp()
{
    s16 x = menu_x;
    s16 y = menu_y;

    eprintf2(8, 14, x, y, 0, 0, "-- VIB LOOP SET --------");
    eprintf2(8, 14, x, y + 16, 4, 0, "LOOP FRAME = %d", V->loopFrame);
}

// Draws the edit grid: level rows, frame columns from `scroll`, the pattern's keys as lines and
// the cursor cell.
void tvibEditFrameDisp()
{
    TvibData* d = &V->list[V->listNo];
    S16Vec v[4];
    u32 col;
    s16 i;
    s16 x;
    s16 y;

    if (V->dispFlag & 4) {
        return;
    }
    if (V->scroll > V->frame - 10) {
        V->scroll = V->frame - 10;
        if (V->scroll < 0) {
            V->scroll = 0;
        }
    }
    if (V->scroll < V->frame - 20) {
        V->scroll = V->frame - 20;
        if (V->scroll > 1970) {
            V->scroll = 1970;
        }
    }
    col = 0x80808080;
    x = menu_x;
    for (i = 0; i < 9; i++) {
        y = menu_y + i * cell_h;
        v[0].x = x;
        v[0].y = y;
        v[0].z = 0;
        v[1].x = cell_w * (frame_w - 1) + x;
        v[1].y = y;
        v[1].z = 0;
        TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
    }
    y = menu_y;
    for (i = 0; i < frame_w; i++) {
        if ((i + V->scroll) % 10 == 0) {
            col = 0x808080FF;
        } else {
            col = 0x404040FF;
        }
        x = menu_x + i * cell_w;
        v[0].x = x;
        v[0].y = y;
        v[0].z = 0;
        v[1].x = x;
        v[1].y = y + cell_h * 8;
        v[1].z = 0;
        TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
    }
    eprintf2(8, 14, menu_x, menu_y - 20, 0, 0, "[ Lv=%d, Frame=%04d/%04d ],  Edit_no[ %02d /%02d ]", V->level,
             V->frame, 2000, V->editNo, d->num);
    for (i = 0; i < 9; i++) {
        eprintf2(8, 14, menu_x - 16, menu_y - 8 + i * 16, 0, 0, "%1d", 8 - i);
    }
    if (V->dispFlag & 1) {
        col = 0xFF000080;
        if (V->dispFlag & 2) {
            col = 0x40FFFFFF;
        }
        tvibFrameMarkDraw(V->level, V->frame, col, 0);
        col = 0xFFFF00FF;
        x = menu_x + (V->frame - V->scroll) * cell_w;
        y = menu_y;
        v[0].x = x;
        v[0].y = y - 8;
        v[0].z = 0;
        v[1].x = x;
        v[1].y = y + 8 + cell_h * 8;
        v[1].z = 0;
        TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
        if ((V->dispFlag & 8) && V->scroll <= V->loopFrame && V->scroll + 30 > V->loopFrame) {
            col = 0x6020FFFF;
            x = menu_x + (V->loopFrame - V->scroll) * cell_w;
            y = menu_y;
            v[0].x = x;
            v[0].y = y - 8;
            v[0].z = 0;
            v[1].x = x;
            v[1].y = y + 8 + cell_h * 8;
            v[1].z = 0;
            TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
        }
    }
    tvibFrameVibDraw(d);
    x = menu_x;
    y = menu_y + cell_h * 9;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;
    v[1].x = x + cell_w * 29;
    v[1].y = y;
    v[1].z = 0;
    col = 0x0000FFFF;
    TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
    x = menu_x + cell_w * 29 * V->frame / 2000;
    y = menu_y + cell_h * 9;
    v[0].x = x;
    v[0].y = y - 5;
    v[0].z = 0;
    v[1].x = x;
    v[1].y = y + 5;
    v[1].z = 0;
    col = 0xFFFF00FF;
    TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
}

// Draws the mode-specific help / status lines under the grid.
void tvibModeFrameDisp()
{
    S16Vec v[4];
    u32 col;

    // per-arm locals: shared function-scope x0..y1 would be one global pseudo each, allocated to the
    // same registers in both arms, and jump2 then cross-jumps the identical store tails
    switch (V->mode) {
    case 0: {
        int x0 = (u16) list_x - 6;
        int y0 = (u16) list_y - 24;
        int x1 = (u16) list_x + 462;
        int y1 = (u16) list_y + 162;
        v[0].x = x0;
        v[0].y = y0;
        v[0].z = 0;
        v[1].x = x1;
        v[1].y = y0;
        v[1].z = 0;
        v[2].x = x1;
        v[2].y = y1;
        v[2].z = 0;
        v[3].x = x0;
        v[3].y = y1;
        v[3].z = 0;
        break;
    }
    case 1:
    case 2: {
        int x0 = (u16) menu_x - 34;
        int y0 = (u16) menu_y - 24;
        int x1 = (u16) menu_x + 420;
        int y1 = (u16) menu_y + 154;
        v[0].x = x0;
        v[0].y = y0;
        v[0].z = 0;
        v[1].x = x1;
        v[1].y = y0;
        v[1].z = 0;
        v[2].x = x1;
        v[2].y = y1;
        v[2].z = 0;
        v[3].x = x0;
        v[3].y = y1;
        v[3].z = 0;
        break;
    }
    default:
        return;
    }
    col = 0xFFFFFFFF;
    switch (TOOL_FLAG(0x51E4) & 0xF) {
    case 0:
        col = 0x808080FF;
        break;
    case 1:
        col = 0x909090FF;
        break;
    case 2:
        col = 0xA0A0A0FF;
        break;
    case 3:
        col = 0xB0B0B0FF;
        break;
    case 4:
        col = 0xC0C0C0FF;
        break;
    case 5:
        col = 0xD0D0D0FF;
        break;
    case 6:
        col = 0xE0E0E0FF;
        break;
    case 7:
        col = 0xF0F0F0FF;
        break;
    case 8:
        col = 0xF0F0F0FF;
        break;
    case 9:
        col = 0xE0E0E0FF;
        break;
    case 10:
        col = 0xD0D0D0FF;
        break;
    case 11:
        col = 0xC0C0C0FF;
        break;
    case 12:
        col = 0xB0B0B0FF;
        break;
    case 13:
        col = 0xA0A0A0FF;
        break;
    case 14:
        col = 0x909090FF;
        break;
    case 15:
        col = 0x808080FF;
        break;
    }
    TprimDrawFrameFn_s16(v, (GXColor*) &col, 4);
}

// Draws every key of pattern `d` on the grid (the edited one highlighted).
void tvibFrameVibDraw(TvibData* d)
{
    s16 i;

    for (i = 0; i < (s16) d->num; i++) {
        VibDataEntry* e = &d->e[i];
        u32 col = 0xFFFFFFFF;
        int rnd = e->type & 0x8000;
        int size;
        u16 end;

        if (rnd) {
            col = 0xFF0000FF;
        }
        if ((V->dispFlag & 1) && i == V->editNo) {
            col = 0xFFFF00FF;
            if (rnd) {
                col = 0xFF8080FF;
            }
        }
        tvibFrameLineDraw(e, col);
        col = 0xFFFFFFFF;
        size = 1;
        end = e->wait + e->time;
        if (i == V->editNo) {
            col = 0xFFFF00FF;
            size = 2;
        }
        tvibFrameMarkDraw(e->lvl0, e->wait, col, size);
        tvibFrameMarkDraw(e->lvl1, end, col, size);
    }
}

// One key as a line from (wait, lvl0) to (wait + time, lvl1) on the grid, clipped to the shown
// frame window.
void tvibFrameLineDraw(VibDataEntry* e, u32 col)
{
    S16Vec v[2];
    GXColor c;
    int scroll;
    int start;
    int end;
    int n;
    int i;
    f32 step;
    f32 lv;
    f32 cur;

    *(u32*) &c = col;
    scroll = V->scroll;
    start = e->wait;
    end = start + e->time;
    if (start < scroll + frame_w - 1 && end > scroll) {
        step = ((f32) e->lvl1 - (f32) e->lvl0) / (f32) e->time;
        lv = (f32) e->lvl0;
        n = end - scroll;
        for (i = start - scroll; i < n; i++) {
            cur = lv;
            lv += step;
            if (i >= 0) {
                if (i >= frame_w - 1) {
                    break;
                }
                v[0].x = (u16) menu_x + cell_w * i;
                v[1].x = v[0].x + cell_w;
                v[0].y = (u16) menu_y + (u16) (s16) ((8.0f - cur) * (f32) cell_h);
                v[1].y = (u16) menu_y + (u16) (s16) ((8.0f - lv) * (f32) cell_h);
                v[0].z = 0;
                v[1].z = 0;
                TprimDrawFrameFn_s16(v, &c, 2);
            }
        }
    }
}

// A square mark at grid cell (frame, level).
void tvibFrameMarkDraw(int lv, int frame, u32 col, u32 size)
{
    S16Vec v[4];
    GXColor c;
    int fx;
    s16 x;
    s16 y;

    *(u32*) &c = col;
    fx = frame - V->scroll;
    if (fx < 0) {
        return;
    }
    if (fx > frame_w - 1) {
        return;
    }
    x = (u16) menu_x + fx * cell_w;
    y = (u16) menu_y + (8 - lv) * cell_h;
    switch (size) {
    case 0:
        v[0].x = x - 9;
        v[0].y = y - 9;
        v[0].z = 0;
        v[1].x = x + 9;
        v[1].y = y - 9;
        v[1].z = 0;
        v[2].x = x + 9;
        v[2].y = y + 9;
        v[2].z = 0;
        v[3].x = x - 9;
        v[3].y = y + 9;
        v[3].z = 0;
        break;
    case 1:
    default:
        v[0].x = x;
        v[0].y = y - 5;
        v[0].z = 0;
        v[1].x = x + 5;
        v[1].y = y;
        v[1].z = 0;
        v[2].x = x;
        v[2].y = y + 5;
        v[2].z = 0;
        v[3].x = x - 5;
        v[3].y = y;
        v[3].z = 0;
        break;
    case 2:
        v[0].x = x;
        v[0].y = y - 7;
        v[0].z = 0;
        v[1].x = x + 7;
        v[1].y = y;
        v[1].z = 0;
        v[2].x = x;
        v[2].y = y + 7;
        v[2].z = 0;
        v[3].x = x - 7;
        v[3].y = y;
        v[3].z = 0;
        break;
    }
    TprimDrawFrameFn_s16(v, (GXColor*) &c, 4);
}

// Draws a miniature of every pattern in the list cells.
void tvibListVibDraw()
{
    S16Vec v[2];
    u32 col;
    s16 i;
    s16 j;
    s16 page;
    s16 top;

    page = V->listNo % 16;
    top = (u16) V->listNo - page;
    for (i = 0; i < 16; i++) {
        if (i >= 64) {
            break;
        }
        TvibData* d = &V->list[i + top];
        int x = LIST_X(i);
        int y = LIST_Y(i);
        int xl = x + 49;

        col = 0x60606060;
        v[0].x = x + 48;
        v[0].y = y;
        v[0].z = 0;
        v[1].x = x + 48;
        v[1].y = y + 16;
        v[1].z = 0;
        TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
        for (j = 0; j < (s16) d->num; j++) {
            VibDataEntry* e = &d->e[j];

            if (e->type & 0x8000) {
                col = 0xFF0000FF;
            } else {
                col = 0xFFFFFFFF;
            }
            tvibListLineDraw(e, col, xl, y);
        }
        if (i + top == V->listNo) {
            xl = LIST_X(i) + 49;
            if (V->frame <= 177) {
                xl += V->frame;
            } else {
                xl += 178;
            }
            y = LIST_Y(i);
            col = 0xFFFF00FF;
            v[0].x = xl;
            v[0].y = y - 2;
            v[0].z = 0;
            v[1].x = xl;
            v[1].y = y + 20;
            v[1].z = 0;
            TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
        }
    }
}

// One key as a line inside a list cell at (x, y).
void tvibListLineDraw(VibDataEntry* e, u32 col, int x, int y)
{
    S16Vec v[2];
    f32 h0 = (8 - e->lvl0) * 2;
    f32 h1 = (8 - e->lvl1) * 2;
    int start = e->wait;
    int end = e->wait + e->time;

    if (start > 178) {
        start = 178;
    }
    if (end > 178) {
        end = 178;
    }
    v[0].x = x + start;
    v[0].y = y + (int) h0;
    v[0].z = 0;
    v[1].x = x + end;
    v[1].y = y + (int) h1;
    v[1].z = 0;
    TprimDrawFrameFn_s16(v, (GXColor*) &col, 2);
}

// Writes the .vib image: 64 offsets (0 = empty pattern) followed by the used patterns (count +
// keys).
void tvibFileSave(const char* path)
{
    TvibFile* f = (TvibFile*) V->file;
    u32 size;
    u32 hsize;
    u8* p = (u8*) f;
    u32 ofs = sizeof(TvibFile);
    u32 i;
    TvibData* d;

    f->num = 64;
    for (i = 0; i < 64; i++) {
        d = &V->list[i];
        if (d->num != 0) {
            f->ofs[i] = ofs;
        } else {
            f->ofs[i] = 0;
        }
        ofs += 4 + d->num * 8;
    }
    hsize = sizeof(TvibFile);
    memcpy(p, (u8*) f, hsize);
    p += hsize;
    size = hsize;
    for (i = 0; i < 64; i++) {
        u32 n;

        d = &V->list[i];
        n = d->num;

        if (n != 0) {
            u32 len = n * 8 + 4;
            memcpy(p, (u8*) d, len);
            p += len;
            size += len;
        }
    }
    if (HDWrite(path, V->file, size) != size) {
        pLog->err(0, 0, "%s: SAVE ERROR !!!", path);
    }
}

// Reads a .vib image into the 64-pattern list (empty offsets clear the pattern).
void tvibFileLoad(const char* path)
{
    int size;
    TvibFile* f;
    u32 i;

    size = HDRead(path, V->file);
    if (size <= 0) {
        pLog->err(0, 0, "%s: LOAD ERROR !!!", path);
        return;
    }
    if (size > 0x2204) {
        pLog->err(0, 0, "tvibFileLoad(): memory over!");
        return;
    }
    f = (TvibFile*) V->file;
    for (i = 0; i < f->num; i++) {
        u32 ofs = f->ofs[i];

        if (ofs != 0) {
            TvibData* src = (TvibData*) (V->file + ofs);
            u32 j;

            V->list[i].num = src->num;
            for (j = 0; j < src->num; j++) {
                V->list[i].e[j] = src->e[j];
            }
        } else {
            V->list[i].num = 0;
        }
    }
}
