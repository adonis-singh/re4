#include "types.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "db_cam.h"
#include "main_mem.h"
#include "main_sub.h"
#include "math_sub.h"
#include "file.h"
#include "player.h"
#include "db_log.h"
#include "t_prim.h"
#include "t_util.h"
#include <stdio.h>
#include <string.h>

// Route check point editor (Tools/t_rck.cpp): places route points, connects them (two-way / one-way
// lines), computes the next-hop table with Dijkstra and saves the room's .rtp file.


#define RCK_POINT_MAX 128


struct RckPoint {
    Vec pos;      // 0x00
    u16 lineOfs;  // 0x0C  save image: index of the point's first line record
    u16 nLine;    // 0x0E  lines leaving the point
};

struct RckLine {
    s16 to;   // 0x00  target point, -1 = none
    u16 len;  // 0x02  distance / 10
};

struct RckHeader {
    u32 magic;    // 0x00  "2RTP"
    u16 x4;       // 0x04
    u16 nPoint;   // 0x06
    u16 nLine;    // 0x08
    u16 nSq;      // 0x0A  nPoint * nPoint (next table size)
    u32 hdrSize;  // 0x0C  0x18
    u32 ofsLine;  // 0x10
    u32 ofsNext;  // 0x14
};

struct RckWork {
    int mode;           // 0x000  routine (rckFunc)
    int step;           // 0x004  save / load sub state
    int menuCursor;     // 0x008
    int subCursor;      // 0x00C
    u32 flags;          // 0x010  bit0: point caught, bit1: line start set, bit3: hide
    int result;         // 0x014  save / load result
    u32 editMode;       // 0x018  0 move, 1 line, 2 one way, 3 delete
    f32 curX;           // 0x01C  screen cursor
    f32 curY;           // 0x020
    int camMode;        // 0x024  1: the debug camera has the pad
    JOY joy;            // 0x028
    f32 x290;           // 0x290  screen centre
    f32 x294;           // 0x294
    f32 x298;           // 0x298
    f32 x29C;           // 0x29C
    u8 pad_2A0[8];
    int catchTimer;     // 0x2A8  double click window
    int cur;            // 0x2AC  caught point, -1 = none
    int near;           // 0x2B0  point nearest to the cursor
    int lineStart;      // 0x2B4
    void* savedRtp;     // 0x2B8  pG->pRoomRtp on entry
    RckHeader hdr;      // 0x2BC
    RckPoint pt[RCK_POINT_MAX];                    // 0x2D4
    RckLine line[RCK_POINT_MAX][RCK_POINT_MAX];    // 0xAD4
    s8 next[RCK_POINT_MAX][RCK_POINT_MAX];         // 0x10AD4
};

struct RckNode {
    u8 done;   // 0x0
    s8 prev;   // 0x1
    u16 pad;
    u32 dist;  // 0x4
};

static RckWork* rckWork;
static u8* rckSave;
#define RCK (rckWork)
#define RCK_SAVE (rckSave)
#define RCK_SAVE_SIZE 0x14818

void rckInit();
static void tool_quit();
static void mode_main();
static void mode_menu();
static void mode_save();
static void mode_clear();
static void mode_load();
void menu_sel(int* cur, int n);
void menu_print(int* pos, const char** str, int n, int cur);
void rckPointAdd();
void rckPointDelete();
void rckPointCatch();
void rckPointChange();
void rckPointCameraMove();
void rckPointMove();
int rckGetNearPoint(Vec* cur);
void rckPointLineStart();
void rckPointLineEnd();
void rckMainDisp();
void rckDrawPoint();
void rckDrawPointLine();
void rckDrawPointLineNow();
void rckPointInfoDisp();
void rckPointNextDisp();
void rckSetRoute();
void rckSetNextPoint(int start);
int rckFileSave(int no);
int rckMakeSaveData(void* buf, u32 size);
int rckFileLoad(int no);
void rckMakeEditData(void* data);
void rckSetFilename(char* path, int no);
void rckCameraMove();

static int menu_pos[2] = {0x18, 0x134};
static int info_pos[2] = {0x18, 0x1C};
static int menu_mode[5] = {2, 4, 5, 1, 0};

// Route check point tool entry (debug menu 12): init, then every frame the camera / pad snapshot,
// the cursor (TutilMoveCursor), rckFunc[mode] (0 quit, 1 clear, 2 main, 3 menu, 4 save, 5 load)
// and the display.
void ToolRctRouteCheck()
{
    void (*tbl[6])() = {tool_quit, mode_clear, mode_main, mode_menu, mode_save, mode_load};

    RckWork*& wp = rckWork;
    u8*& sp = rckSave;

    wp = (RckWork*) Debug_alloc(sizeof(RckWork), 1);
    sp = (u8*) Debug_alloc(RCK_SAVE_SIZE, 1);
    rckInit();
    for (;;) {
        rckCameraMove();
        if (RCK->camMode == 0) {
            if (RCK->catchTimer != 0) {
                RCK->catchTimer--;
            }
            tbl[RCK->mode]();
        }
        rckMainDisp();
        CameraMove();
        TaskSleep(1);
    }
}

// Tool start: work and save buffer allocated, default flags, cursor at the screen centre; the
// room's route table (pG->Rtp) is expanded into the edit form, else the default .rtp is loaded.
void rckInit()
{
    int zero = 0;

    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    pG->Stop_flg |= 0x00200000;
    pG->Disp_flg |= 0x01000000;
    pG->Disp_flg |= 0x00800000;
    DbgFlagOn(pG, DBG_TEST_MODE);
    DbgFlagOn(pG, DBG_BACK_CLIP);
    pG->Stop_flg |= 0x00800000;
    pG->Stop_flg |= 0x00200000;
    pG->Disp_flg |= 0x04000000;
    pG->Disp_flg |= 0x02000000;
    memclr_asm(RCK, sizeof(RckWork));
    RCK->mode = 2;
    RCK->savedRtp = pG->Rtp;
    RCK->cur = -1;
    RCK->near = -1;
    RCK->catchTimer = zero;
    RCK->editMode = zero;
    RCK->x290 = RCK->x298 = RCK->curX = (Screen.x + Screen.width) * 0.5f;
    RCK->x294 = RCK->x29C = RCK->curY = (Screen.y + Screen.height) * 0.5f;
    RCK->camMode = zero;
    if (pG->Rtp != NULL) {
        rckMakeEditData(pG->Rtp);
    } else {
        rckFileLoad(0);
    }
    pG->Rtp = RCK_SAVE;
}

// Mode 0: restores pG->pRoomRtp and the flags, frees the buffers, ends the task.
static void tool_quit()
{
    pG->Rtp = RCK->savedRtp;
    TutilQuitDefault();
    StaFlagOff(pG, STA_BG_OFF);
    pG->Stop_flg &= ~0x00200000;
    pG->Stop_flg &= ~0x00200000;
    pG->Disp_flg &= ~0x04000000;
    pG->Disp_flg &= ~0x02000000;
    DbgFlagOff(pG, DBG_DBG_CAM);
    TaskSignal(0);
    TaskExit();
}

// Mode 2, editing: START opens the menu, Y cycles the edit mode (move / line / one way / delete),
// L/R change the caught point, X adds a point, A catches; then per edit mode: move the caught
// point, start / end a two-way or one-way line between points, delete; the next-hop table is
// recomputed every frame (rckSetRoute).
static void mode_main()
{
    JOY* joy = &Joy[0];

    if (joy->trg & 0x200) {
        RCK->mode = 3;
        RCK->menuCursor = 0;
        RCK->flags &= ~3;
        return;
    }
    TutilMoveCursor((Vec*) &RCK->curX, 40.0f, 1.0f);
    RCK->near = rckGetNearPoint((Vec*) &RCK->curX);
    if (joy->trg & 0x800) {
        RCK->editMode++;
        if (RCK->editMode > 3) {
            RCK->editMode = 0;
        }
        if (RCK->editMode == 3) {
            RCK->cur = -1;
            RCK->flags &= ~3;
        }
    }
    if (Joy[0].trg & 0x60) {
        rckPointChange();
    }
    if (Joy[0].trg & 0x400) {
        rckPointAdd();
    }
    if (Joy[0].trg & 0x100) {
        rckPointCatch();
    }
    switch (RCK->editMode) {
    case 0:
        rckPointMove();
        break;
    case 1:
        if (Joy[0].trg & 0x100) {
            rckPointLineStart();
        }
        if ((Joy[0].rel & 0x100) && (RCK->flags & 2)) {
            rckPointLineEnd();
        }
        break;
    case 2:
        if (Joy[0].trg & 0x100) {
            rckPointLineStart();
        }
        if ((Joy[0].rel & 0x100) && (RCK->flags & 2)) {
            rckPointLineEnd();
        }
        break;
    case 3:
        if (Joy[0].trg & 0x10) {
            rckPointDelete();
        }
        break;
    }
    rckSetRoute();
}

static const char* menu_str[5] = {"MAIN", "SAVE", "LOAD", "CLEAR", "QUIT"};

// Mode 3, - MENU -: MAIN / SAVE / LOAD / CLEAR / QUIT; B back to editing.
static void mode_menu()
{
    JOY* joy = &Joy[0];

    if (joy->trg & 0x1200) {
        RCK->mode = 2;
        return;
    }
    menu_sel(&RCK->menuCursor, 5);
    eprintf(menu_pos[0], menu_pos[1] - 14, 4, 0, "- MENU -");
    menu_print(menu_pos, menu_str, 5, RCK->menuCursor);
    if (joy->trg & 0x100) {
        RCK->mode = menu_mode[RCK->menuCursor];
        RCK->step = 0;
        RCK->subCursor = 0;
    }
}

static const char* save_str[5] = {"DEFAULT", "BACKUP 1", "BACKUP 2", "BACKUP 3", "BACKUP 4"};

// Mode 4, - SAVE -: DEFAULT (.rtp) or BACKUP 1..4 (.rt1..4); shows Complete. / Save Error!.
static void mode_save()
{
    JOY* joy = &Joy[0];

    if (joy->trg & 0x200) {
        RCK->mode = 3;
    }
    eprintf(menu_pos[0], menu_pos[1] - 14, 4, 0, "- SAVE -");
    switch (RCK->step) {
    case 0:
        menu_sel(&RCK->subCursor, 5);
        menu_print(menu_pos, save_str, 5, RCK->subCursor);
        if (joy->trg & 0x100) {
            RCK->result = rckFileSave(RCK->subCursor);
            RCK->step = 1;
        }
        break;
    case 1:
        if (RCK->result) {
            eprintf(menu_pos[0], menu_pos[1], 0, 0, "Complete.");
        } else {
            eprintf(menu_pos[0], menu_pos[1], 2, 0, "Save Error!");
        }
        if (Joy[0].trg & 0x300) {
            RCK->mode = 3;
        }
        break;
    }
}

static const char* clear_str[2] = {"Clear Yes", "Clear No"};

// Mode 1: Clear Yes / No; yes empties the point and line tables.
static void mode_clear()
{
    JOY* joy = &Joy[0];

    if (joy->trg & 0x200) {
        RCK->mode = 3;
    }
    eprintf(menu_pos[0], menu_pos[1] - 14, 4, 0, "- SAVE -");
    menu_sel(&RCK->subCursor, 2);
    menu_print(menu_pos, clear_str, 2, RCK->subCursor);
    if (joy->trg & 0x100) {
        if (RCK->subCursor == 0) {
            RCK->hdr.nPoint = 0;
            RCK->hdr.nLine = 0;
            RCK->hdr.nSq = 0;
            memclr_asm(RCK->pt, sizeof(RCK->pt));
            memclr_asm(RCK->line, sizeof(RCK->line));
            memclr_asm(RCK->next, sizeof(RCK->next));
            RCK->cur = -1;
            RCK->near = -1;
            RCK->catchTimer = 0;
            RCK->editMode = 0;
        }
        RCK->mode = 3;
    }
}

static const char* load_str[5] = {"DEFAULT", "BACKUP 1", "BACKUP 2", "BACKUP 3", "BACKUP 4"};
static GXColor cursor_col[2] = {{0x80, 0, 0, 0xFF}, {0xFF, 0x40, 0x40, 0xFF}};
static GXColor col_catch = {0x60, 0, 0, 0xFF};
static GXColor col_cur = {0x80, 0x10, 0x10, 0xFF};
static GXColor col_near = {0, 0, 0x80, 0xFF};
static GXColor col_point = {0x40, 0x40, 0x40, 0xFF};
static GXColor col_htr = {0x80, 0x80, 0x80, 0xFF};
asm(".section .data; .balign 8; .text");

// Mode 5, - LOAD -: DEFAULT or BACKUP 1..4; shows Complete. / Load Error!.
static void mode_load()
{
    JOY* joy = &Joy[0];

    if (joy->trg & 0x200) {
        RCK->mode = 3;
    }
    eprintf(menu_pos[0], menu_pos[1] - 14, 4, 0, "- LOAD -");
    switch (RCK->step) {
    case 0:
        menu_sel(&RCK->subCursor, 5);
        menu_print(menu_pos, load_str, 5, RCK->subCursor);
        if (joy->trg & 0x100) {
            RCK->result = rckFileLoad(RCK->subCursor);
            RCK->step = 1;
        }
        break;
    case 1:
        if (RCK->result) {
            eprintf(menu_pos[0], menu_pos[1], 0, 0, "Complete.");
        } else {
            eprintf(menu_pos[0], menu_pos[1], 2, 0, "Load Error!");
        }
        if (Joy[0].trg & 0x300) {
            RCK->mode = 3;
        }
        break;
    }
}

// Up/down (repeat) move a menu cursor over `n` rows with wrap.
void menu_sel(int* cur, int n)
{
    if (Joy[0].rep & 0x00040004) {
        (*cur)++;
    } else if (Joy[0].rep & 0x00080008) {
        (*cur)--;
    }
    if (*cur < 0) {
        *cur = n - 1;
    } else if (*cur >= n) {
        *cur = 0;
    }
}

// Prints `n` menu rows at `pos`, the cursor row in the highlight colour.
void menu_print(int* pos, const char** str, int n, int cur)
{
    int x = pos[0];
    int y = pos[1];
    int i;

    for (i = 0; i < n; i++, y += 14) {
        if (i == cur) {
            eprintf(x, y, 0, 0, "%s", str[i]);
        } else {
            eprintf(x, y, 7, 0, "%s", str[i]);
        }
    }
}

// Rounds a height to the 500 grid.
#define RCK_GRID_Y(y) (t = (s8) (((y) + 62.5f) / 500.0f), (f32) t * 500.0f)

// X: adds a point under the cursor (ground position from TutilGet3DPosXZ_All), no lines; it
// becomes the nearest point. Up to RCK_POINT_MAX.
void rckPointAdd()
{
    RckWork* w = RCK;
    u16 n = w->hdr.nPoint;
    RckPoint* p;
    Vec c;
    Vec out;
    int i;
    int no;
    f32 gy;
    int t;
    RckLine* l;

    if (n > 0x7F) {
        return;
    }
    p = &w->pt[n];
    gy = RCK_GRID_Y(pPL->pos.y);
    c.x = pG->Camera.param.pos.x;
    c.y = gy;
    c.z = pG->Camera.param.pos.z;
    TutilGet3DPosXZ_All((Vec*) &w->curX, &c, &out);
    memclr_asm(p, sizeof(RckPoint));
    gy = RCK_GRID_Y(out.y);
    p->pos.x = out.x;
    p->pos.y = gy;
    p->pos.z = out.z;
    RCK->hdr.nPoint++;
    RCK->hdr.nSq = RCK->hdr.nPoint * RCK->hdr.nPoint;
    no = RCK->hdr.nPoint - 1;
    RCK->near = no;
    rckPointCatch();
    for (i = 0; i < RCK_POINT_MAX; i++) {
        l = &RCK->line[no][i];
        l->len = 0;
        l->to = -1;
        l = &RCK->line[i][no];
        l->len = 0;
        l->to = -1;
    }
}

// Delete mode + A: removes the nearest point and every line to / from it (tables shift down).
void rckPointDelete()
{
    int del;
    int i;
    int j;
    RckLine* l;
    RckPoint* pt;

    if (RCK->cur == -1) {
        return;
    }
    del = 0;
    RCK->hdr.nPoint--;
    RCK->hdr.nSq = RCK->hdr.nPoint * RCK->hdr.nPoint;
    for (i = 0; i < RCK_POINT_MAX; i++) {
        l = &RCK->line[i][RCK->cur];
        if (l->to != -1) {
            l->len = 0;
            del++;
            l->to = -1;
            pt = &RCK->pt[i];
            pt->nLine--;
        }
        l = &RCK->line[RCK->cur][i];
        if (l->to != -1) {
            l->len = 0;
            del++;
            l->to = -1;
            pt = &RCK->pt[RCK->cur];
            pt->nLine--;
        }
    }
    RCK->hdr.nLine -= del;
    for (i = 0; i < RCK_POINT_MAX; i++) {
        for (j = RCK->cur; j < RCK_POINT_MAX - 1; j++) {
            RCK->line[i][j] = RCK->line[i][j + 1];
        }
        l = &RCK->line[i][RCK_POINT_MAX - 1];
        l->len = 0;
        l->to = -1;
    }
    for (i = RCK->cur + 1; i < RCK_POINT_MAX; i++) {
        for (j = 0; j < RCK_POINT_MAX; j++) {
            RCK->line[i - 1][j] = RCK->line[i][j];
        }
    }
    for (j = 0; j < RCK_POINT_MAX; j++) {
        l = &RCK->line[RCK_POINT_MAX - 1][j];
        l->len = 0;
        l->to = -1;
    }
    for (i = RCK->cur + 1; i < RCK_POINT_MAX; i++) {
        RCK->pt[i - 1] = RCK->pt[i];
    }
    rckSetRoute();
    RCK->cur = -1;
    RCK->flags &= ~3;
}

// A: catches the nearest point (flags bit 0) for dragging; a second A within 10 frames releases
// it.
void rckPointCatch()
{
    Vec p;
    f32 scr[4];

    if (RCK->catchTimer == 0) {
        int n = RCK->near;

        RCK->cur = n;
        if (n != -1) {
            RckPoint* pt = &RCK->pt[RCK->cur];
            p.x = pt->pos.x;
            p.y = pt->pos.y;
            p.z = pt->pos.z;
            if (TutilGetScreenPos(&p, scr, 0)) {
                RCK->curX = scr[0];
                RCK->curY = scr[1];
                RCK->flags |= 1;
                RCK->flags &= ~2;
                RCK->catchTimer = 10;
            }
        } else {
            RCK->flags &= ~1;
        }
    } else {
        RCK->flags &= ~1;
    }
}

// L/R: steps the caught point through the list (-1 = none) and moves the cursor onto it.
void rckPointChange()
{
    Vec scr;
    Vec p;

    if (Joy[0].trg & 0x60) {
        int n = RCK->hdr.nPoint;

        if (Joy[0].trg & 0x40) {
            RCK->cur--;
            if (RCK->cur < 0) {
                RCK->cur = n - 1;
            }
        } else {
            RCK->cur++;
            if (RCK->cur >= n) {
                if (n > 0) {
                    RCK->cur = 0;
                } else {
                    RCK->cur = -1;
                }
            }
        }
        if (RCK->cur != -1) {
            RckPoint* pt = &RCK->pt[RCK->cur];
            p = pt->pos;
            GetScreenPos(&p, &scr);
            if (scr.x < 50.0f || scr.x > 450.0f || scr.y < 50.0f || scr.y > 400.0f) {
                rckPointCameraMove();
            }
        }
    }
}

// Keeps the cursor on the caught point while the camera moves.
void rckPointCameraMove()
{
    CAMERA* cam = &pG->Camera;
    RckPoint* p = &RCK->pt[RCK->cur];
    Vec d;

    PSVECSubtract(&cam->param.pos, &cam->param.at, &d);
#line 711 "D:/Bio4/Prog/t_rck.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 15000.0f);
    d.y = 10000.0f;
    cam->param.at = p->pos;
    PSVECAdd(&cam->param.at, &d, &cam->param.pos);
    RCK->x290 = RCK->x298 = RCK->curX = (Screen.x + Screen.width) * 0.5f;
    RCK->x294 = RCK->x29C = RCK->curY = (Screen.y + Screen.height) * 0.5f;
}

// Move mode: the caught point follows the cursor's ground position.
void rckPointMove()
{
    RckWork* w = RCK;
    RckPoint* p;
    Vec out;
    Vec c;

    if (w->cur == -1) {
        return;
    }
    p = &w->pt[w->cur];
    if ((Joy[0].on & 0x100) && (w->flags & 1)) {
        c.x = p->pos.x;
        c.y = p->pos.y;
        c.z = p->pos.z;
        TutilGet3DPosXZ_Mov((Vec*) &w->curX, &c, &out);
        p->pos.x = out.x;
        p->pos.y = out.y;
        p->pos.z = out.z;
    } else {
        RCK->flags &= ~1;
    }
    if (Joy[0].rep & 0x00800000) {
        p->pos.y += 500.0f;
    }
    if (Joy[0].rep & 0x00400000) {
        p->pos.y -= 500.0f;
    }
}

// Index of the point whose screen position is nearest the cursor (within the pick radius), -1
// when none.
int rckGetNearPoint(Vec* cur)
{
    RckPoint* p = RCK->pt;
    int ret = -1;
    f32 best = 640000.0f;
    Vec pos;
    f32 scr[4];
    int i;

    for (i = 0; i < RCK->hdr.nPoint; i++, p++) {
        pos.x = p->pos.x;
        pos.y = p->pos.y;
        pos.z = p->pos.z;
        if (TutilGetScreenPos(&pos, scr, 0)) {
            f32 d = (cur->x - scr[0]) * (cur->x - scr[0]) + (cur->y - scr[1]) * (cur->y - scr[1]);

            if (d < best) {
                best = d;
                ret = i;
            }
        }
    }
    return ret;
}

// Line modes + A on a point: remembers it as the line start (flags bit 1).
void rckPointLineStart()
{
    if (RCK->cur != -1) {
        RCK->lineStart = RCK->cur;
        RCK->flags &= ~1;
        RCK->flags |= 2;
    }
}

// Line length of the a -> b connection in 10 units.
static inline u16 rckLineLen(RckPoint* pa, RckPoint* pb)
{
    return (s16) (VEC_DISTXZ(&pa->pos, &pb->pos) * 0.1f);
}

// Line modes + A on a second point: toggles the connection start -> near (two-way in mode 1, one
// way in mode 2; an existing line is removed), lengths from rckLineLen.
void rckPointLineEnd()
{
    RckWork* w;
    int a;
    int b;
    RckLine* ab;
    RckLine* ba;
    RckPoint* pa;
    RckPoint* pb;

    RCK->flags &= ~2;
    w = RCK;
    b = w->near;
    if (b == -1) {
        return;
    }
    a = w->lineStart;
    if (b == a) {
        return;
    }
    ab = &w->line[a][b];
    ba = &w->line[b][a];
    pa = &w->pt[a];
    pb = &w->pt[b];
    switch (w->editMode) {
    case 1:
    default:
        if (ab->to == b) {
            ab->to = -1;
            ba->to = -1;
            pa->nLine--;
            pb->nLine--;
            RCK->hdr.nLine -= 2;
            ab->len = 0;
            ba->len = ab->len;
        } else {
            ab->to = w->near;
            ba->to = RCK->lineStart;
            pa->nLine++;
            pb->nLine++;
            RCK->hdr.nLine += 2;
            ab->len = rckLineLen(pa, pb);
            ba->len = ab->len;
        }
        break;
    case 2:
        if (ab->to == b) {
            ab->to = -1;
            pa->nLine--;
            RCK->hdr.nLine--;
            ab->len = 0;
        } else {
            ab->to = w->near;
            pa->nLine++;
            RCK->hdr.nLine++;
            ab->len = rckLineLen(pa, pb);
        }
        break;
    }
}

// Mode label beside the cursor and its help line.
// written out per mode in the original: the help string is emitted between the big and the small one
#define rckModeDisp(big, small, on, dx, help)                                          \
    if (on) {                                                                          \
        eprintf2(10, 16, (u32) RCK->curX - (dx), (u32) RCK->curY + 16, 4, 0, big);     \
        eprintf(368, 126, 4, 0, help);                                                 \
    } else {                                                                           \
        eprintf2(10, 16, (u32) RCK->curX - (dx), (u32) RCK->curY + 16, 7, 0, small);   \
        eprintf(368, 126, 7, 0, help);                                                 \
    }

// Draws the points, lines and the line being drawn (3D), the 2D cursor, the caught point's info,
// the room number and the HELP column.
void rckMainDisp()
{
    int c;

    if (RCK->flags & 8) {
        return;
    }
    TprimDraw3D(1);
    rckDrawPoint();
    rckDrawPointLine();
    if (RCK->flags & 2) {
        rckDrawPointLineNow();
    }
    TprimDraw2D(0);
    c = 0;
    if (RCK->flags & 1) {
        c = 1;
    }
    TprimDrawCursor((Vec*) &RCK->curX, 0.0f, &cursor_col[c]);
    rckPointInfoDisp();
    eprintf2(10, 16, 440, 8, 0, 0, "%03x", pG->room_id);
    eprintf(menu_pos[0], menu_pos[1] - 14, 4, 0, "- MENU -");
    eprintf(368, 28, 4, 0, "-- HELP --");
    eprintf(368, 42, 0, 0, "Start: Menu");
    eprintf(368, 56, 0, 0, "A:     Catch Point");
    eprintf(368, 70, 0, 0, "X:     Add Point");
    eprintf(368, 84, 0, 0, "Y:     Mode change");
    eprintf(368, 98, 0, 0, "A:     Catch Point");
    eprintf(368, 112, 0, 0, "A dclick:CamCenter");
    eprintf(368, 140, 0, 0, "L:\t\tTarget--");
    eprintf(368, 140, 0, 0, "L:\t\tTarget++");
    switch (RCK->editMode) {
    case 0:
        rckModeDisp("MOVE", "move", RCK->flags & 1, 20, "A on:  Move Point");
        break;
    case 1:
        rckModeDisp("LINE", "line", RCK->flags & 2, 20, "A on:  Line connect");
        break;
    case 2:
        rckModeDisp("ONE WAY", "one way", RCK->flags & 2, 40, "A on:  Line connect");
        break;
    case 3:
        rckModeDisp("Z:DELETE", "Z:delete", RCK->cur != -1, 30, "Z:     Delete Point");
        break;
    }
    TprimDrawCursor((Vec*) &RCK->curX, 0.0f, &cursor_col[c]);
}

// Draws every point as a cursor mark: caught / current / nearest / plain colours.
void rckDrawPoint()
{
    RckPoint* p = RCK->pt;
    Vec pos;
    Vec scr;
    Vec q;
    int i;

    for (i = 0; i < RCK->hdr.nPoint; i++, p++) {
        pos.x = p->pos.x;
        pos.y = p->pos.y;
        pos.z = p->pos.z;
        TprimSetBlend(2);
        TprimDrawHtr(&pos, &col_htr);
        TprimSetBlend(1);
        if (i == RCK->cur) {
            if (RCK->flags & 1) {
                TprimDrawHtrCone(&pos, &col_cur);
            } else {
                TprimDrawHtrCone(&pos, &col_catch);
            }
        } else if (i == RCK->near) {
            TprimDrawHtrCone(&pos, &col_near);
        } else {
            TprimDrawHtrCone(&pos, &col_point);
        }
        q = p->pos;
        GetScreenPos(&q, &scr);
        if (scr.z < 1.0f) {
            if (i == RCK->cur) {
                eprintf2(10, 16, (u32) scr.x - 20, (u32) scr.y + 16, 4, 0, "[%02d]", i);
            } else {
                eprintf2(10, 16, (u32) scr.x - 20, (u32) scr.y + 16, 7, 0, "[%02d]", i);
            }
        }
    }
}

// Draws every line of the table (two-way lines once, one-way lines with a direction mark).
void rckDrawPointLine()
{
    u8 red = 0xFF;
    GXColor colBoth = {red, 0xFF, 0xFF, 0xFF};
    GXColor colOne = {0, 0, 0xFF, 0xFF};
    GXColor colHit = {red, 0xFF, 0, 0xFF};
    Vec v[2];
    Vec a;
    Vec b;
    Mtx m;
    Vec seg0;
    Vec seg1;
    Vec dir;
    Vec tip;
    Vec mid;
    int i;
    int j;

    TprimSetBlend(0);
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        for (j = 0; j < RCK->hdr.nPoint; j++) {
            if (i == j) {
                continue;
            }
            RckWork* w = RCK;
            RckLine* lij = &w->line[i][j];
            if (lij->to == -1) {
                continue;
            }
            RckLine* l = &w->line[j][i];
            if (l->to != -1 && i > j) {
                continue;
            }
            RckPoint* p = &w->pt[i];
            v[0].x = p->pos.x;
            v[0].y = p->pos.y;
            v[0].z = p->pos.z;
            a.x = p->pos.x;
            a.y = p->pos.y + 500.0f;
            a.z = p->pos.z;
            p = &w->pt[j];
            v[1].x = p->pos.x;
            v[1].y = p->pos.y;
            v[1].z = p->pos.z;
            b.x = p->pos.x;
            b.y = p->pos.y + 500.0f;
            b.z = p->pos.z;
            if (SatMgr.hitCheck(&a, &b, NULL, NULL, 0x6000, 0x3C3070)) {
                TprimDrawLineFn(v, &colHit, 2);
            } else if (l->to == -1) {
                TprimDrawLineFn(v, &colOne, 2);
            } else {
                TprimDrawLineFn(v, &colBoth, 2);
            }
            if (l->to == -1) {
                seg0.x = v[0].x;
                seg0.y = v[0].y;
                seg0.z = v[0].z;
                seg1.x = v[1].x;
                seg1.y = v[1].y;
                seg1.z = v[1].z;
                PSVECSubtract(&seg0, &seg1, &dir);
                PSVECScale(&dir, &dir, 0.5f);
                PSVECAdd(&seg1, &dir, &mid);
                if (dir.x == 0.0f && dir.z == 0.0f) {
                    continue;
                }
#line 1173 "D:/Bio4/Prog/t_rck.cpp"
                VECNormalize(&dir, &dir);
                PSVECScale(&dir, &dir, 500.0f);
                PSMTXRotRad(m, 'y', 0.7853982f);
                PSMTXMultVec(m, &dir, &tip);
                PSVECAdd(&mid, &tip, &tip);
                v[0].x = mid.x;
                v[0].y = mid.y;
                v[0].z = mid.z;
                v[1].x = tip.x;
                v[1].y = tip.y;
                v[1].z = tip.z;
                TprimDrawLineFn(v, &colOne, 2);
                PSMTXRotRad(m, 'y', -0.7853982f);
                PSMTXMultVec(m, &dir, &tip);
                PSVECAdd(&mid, &tip, &tip);
                v[0].x = mid.x;
                v[0].y = mid.y;
                v[0].z = mid.z;
                v[1].x = tip.x;
                v[1].y = tip.y;
                v[1].z = tip.z;
                TprimDrawLineFn(v, &colOne, 2);
            }
        }
    }
}

// Draws the line from the line start point to the cursor while a line is being placed.
void rckDrawPointLineNow()
{
    u8 red = 0xFF;
    GXColor colNow = {red, 0, 0, 0xFF};
    GXColor colBack = {0, 0, 0xFF, 0xFF};
    RckWork* w = RCK;
    RckPoint* p = &w->pt[w->lineStart];
    Vec v[2];
    Vec c;
    Vec out;

    c.x = p->pos.x;
    c.y = p->pos.y;
    c.z = p->pos.z;
    TutilGet3DPosXZ_All((Vec*) &w->curX, &c, &out);
    v[0].x = p->pos.x;
    v[0].y = p->pos.y;
    v[0].z = p->pos.z;
    v[1].x = out.x;
    v[1].y = out.y;
    v[1].z = out.z;
    TprimDrawLineFn(v, &colNow, 2);
    RckWork* w2 = RCK;
    if (w2->near != -1 && w2->near != w2->lineStart) {
        // row pointer `w + (near << 9) + 0xAD4` then `lhax row, ls << 2` (a plain 2-D index folds 0xAD4
        // into the base and sums the two index terms; the u32 sum keeps the row as the first operand)
        RckLine* row = w2->line[w2->near];
        if (((RckLine*) ((u32) row + (w2->lineStart << 2)))->to != -1) {
            p = &w2->pt[w2->near];
            v[0].x = p->pos.x;
            v[0].y = p->pos.y;
            v[0].z = p->pos.z;
            p = &w2->pt[w2->lineStart];
            v[1].x = p->pos.x;
            v[1].y = p->pos.y;
            v[1].z = p->pos.z;
            TprimDrawLineFn(v, &colBack, 2);
        }
    }
}

// Prints the caught point's number, position and its outgoing lines.
void rckPointInfoDisp()
{
    RckWork* w = RCK;
    int x = info_pos[0];
    int y = info_pos[1];

    if (w->cur == -1) {
        eprintf(x, y, 0, 0, "wk[--]");
        eprintf(x, y + 14, 0, 0, "Pos[----- -----(F--) -----] Rad:----");
        eprintf(x, y + 28, 0, 0, "Lines:-");
    } else {
        RckPoint* p = &w->pt[w->cur];

        eprintf(x, y, 0, 0, "wk[%02d]", w->cur);
        eprintf(x, y + 14, 0, 0, "Pos[%5.0f %5.0f %5.0f]", p->pos.x, p->pos.y, p->pos.z);
        eprintf(x, y + 28, 0, 0, "Lines:%d", p->nLine);
        rckPointNextDisp();
    }
}

// Prints the caught point's next-hop row.
void rckPointNextDisp()
{
    int x = info_pos[0];
    int y = info_pos[1] + 42;
    int i;

    if (RCK->cur == -1) {
        return;
    }
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        eprintf(x + (i % 8) * 24, y + (i / 8) * 14, 0, 0, "%02d", RCK->next[RCK->cur][i]);
    }
}

// Refreshes every line's target / length and recomputes the whole next-hop table (Dijkstra from
// each point).
void rckSetRoute()
{
    int i;
    int j;

    for (i = 0; i < RCK->hdr.nPoint; i++) {
        RckPoint* pa = &RCK->pt[i];

        for (j = 0; j < RCK->hdr.nPoint; j++) {
            RckLine* l = &RCK->line[i][j];

            if (l->to != -1) {
                RckPoint* pb;
                l->to = j;
                pb = &RCK->pt[j];
                l->len = rckLineLen(pa, pb);
            }
        }
    }
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        rckSetNextPoint(i);
    }
}

// Dijkstra from `start`: fills next[start][] with the first hop towards every point.
void rckSetNextPoint(int start)
{
    RckNode node[RCK_POINT_MAX];
    int i;
    int k;
    int best;

    memclr_asm(node, sizeof(node));
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        node[i].dist = 0xFFFFFFFF;
        node[i].prev = -1;
    }
    node[start].dist = 0;
    best = start;
    for (k = 0; k < RCK->hdr.nPoint; k++) {
        u32 min = 0xFFFFFFFF;
        RckNode* nb;

        for (i = 0; i < RCK->hdr.nPoint; i++) {
            RckNode* n = &node[i];

            if ((n->done & 1) == 0 && min > n->dist) {
                min = n->dist;
                best = i;
            }
        }
        nb = &node[best];
        nb->done |= 1;
        for (i = 0; i < RCK->hdr.nPoint; i++) {
            RckLine* l = &RCK->line[best][i];

            if (l->to != -1) {
                RckNode* n = &node[i];

                if ((n->done & 1) == 0) {
                    u32 d = nb->dist + l->len;

                    if (n->dist > d) {
                        n->dist = d;
                        n->prev = best;
                    }
                }
            }
        }
    }
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        if (i == start) {
            RCK->next[start][i] = i;
        } else if (node[i].prev == -1) {
            RCK->next[start][i] = -1;
        } else {
            int n = i;

            while (node[n].prev != start) {
                n = node[n].prev;
            }
            RCK->next[start][i] = n;
        }
    }
}

// Writes the packed route file (rckMakeSaveData) to slot `no` (0 .rtp, 1..4 .rtN).
int rckFileSave(int no)
{
    char path[256];
    int size;

    size = rckMakeSaveData(RCK_SAVE, RCK_SAVE_SIZE);
    rckSetFilename(path, no);
    return HDWrite(path, RCK_SAVE, size);
}

// Packs the edit tables into the file image: header ("2RTP"), points with their line offsets and
// counts, the line records, the next-hop table; returns the byte size (0 when `size` is too small).
int rckMakeSaveData(void* buf, u32 size)
{
    u8* p;
    s16 ofs = 0;
    int i;
    int j;
    u32 total;
    u32 o;

    RCK->hdr.magic = 0x32525450;
    o = sizeof(RckHeader);
    RCK->hdr.hdrSize = o;
    o += RCK->hdr.nPoint * sizeof(RckPoint);
    RCK->hdr.ofsLine = o;
    o += RCK->hdr.nLine * 4;
    RCK->hdr.ofsNext = o;
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        RCK->pt[i].lineOfs = ofs;
        ofs += RCK->pt[i].nLine;
    }
    memclr_asm(buf, size);
    p = (u8*) buf;
    *(RckHeader*) p = RCK->hdr;
    p += sizeof(RckHeader);
    memcpy(p, RCK->pt, RCK->hdr.nPoint * sizeof(RckPoint));
    p += RCK->hdr.nPoint * sizeof(RckPoint);
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        for (j = 0; j < RCK->hdr.nPoint; j++) {
            RckLine* l = &RCK->line[i][j];

            if (l->to != -1) {
                *(u32*) p = *(u32*) l;
                p += 4;
            }
        }
    }
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        memcpy(p, RCK->next[i], RCK->hdr.nPoint);
        p += RCK->hdr.nPoint;
    }
    o = RCK->hdr.nPoint * sizeof(RckPoint) + sizeof(RckHeader);
    o += RCK->hdr.nLine * 4;
    o += RCK->hdr.nSq;
    total = o + 0x20;
    return total - (o & 0x1F);
}

// Reads slot `no` into the save buffer and expands it (rckMakeEditData); 0 on failure.
int rckFileLoad(int no)
{
    char path[256];
    int ret;

    rckSetFilename(path, no);
    ret = HDRead(path, RCK_SAVE);
    if (ret == 0) {
        return 0;
    }
    rckMakeEditData(RCK_SAVE);
    return ret;
}

// Expands a route file image into the edit tables (points, full line matrix, next-hop table).
void rckMakeEditData(void* data)
{
    u8* p = (u8*) data;
    int i;
    int j;

    memclr_asm(&RCK->hdr, sizeof(RckHeader));
    memclr_asm(RCK->pt, sizeof(RCK->pt));
    memset_asm(RCK->line, 0xFF, sizeof(RCK->line));
    memset_asm(RCK->next, 0xFF, sizeof(RCK->next));
    if (*(u32*) p != 0x32525450) {
        return;
    }
    RCK->hdr = *(RckHeader*) p;
    p += sizeof(RckHeader);
    memcpy(RCK->pt, p, RCK->hdr.nPoint * sizeof(RckPoint));
    p += RCK->hdr.nPoint * sizeof(RckPoint);
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        RckPoint* pt = &RCK->pt[i];

        for (j = 0; j < pt->nLine; j++) {
            RckLine l;
            RckLine* d;

            *(u32*) &l = *(u32*) p;
            p += 4;
            d = &RCK->line[i][l.to];
            d->to = l.to;
            d->len = l.len;
        }
    }
    for (i = 0; i < RCK->hdr.nPoint; i++) {
        memcpy(RCK->next[i], p, RCK->hdr.nPoint);
        p += RCK->hdr.nPoint;
    }
}

// x:\soft\room\st<n>\r<room>\r<room>.rtp (no 0) or .rt<no>.
void rckSetFilename(char* path, int no)
{
    if (no == 0) {
        sprintf(path, "x:\\soft\\room\\st%1x\\r%1x%02x\\r%1x%02x.rtp", pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no);
    } else {
        sprintf(path, "x:\\soft\\room\\st%1x\\r%1x%02x\\r%1x%02x.rt%d", pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no, no);
    }
}

// Pad snapshot; START toggles 1P CAMERA MODE where the debug camera takes the pad.
void rckCameraMove()
{
    RCK->joy = Joy[0];
    if (Joy[0].trg & 0x1000) {
        RCK->camMode ^= 1;
    }
    if (RCK->camMode) {
        CamDbg.move(&pG->Camera, Joy, 0);
        RCK->joy.trg = 0;
        RCK->joy.on = 0;
        RCK->joy.rep = 0;
        DbgFlagOn(pG, DBG_DBG_CAM);
        if (pG->Frame_cnt & 0x10) {
            eprintf(320, 24, 4, 0, "1P CAMERA MODE");
        }
    }
}
