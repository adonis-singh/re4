#include "types.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"
#include "scheduler.h"
#include "joy.h"
#include "eprintf.h"
#include "vec.h"
#include "camera.h"
#include "file.h"
#include "snd.h"
#include "dbmodule.h"
#include "light.h"
#include "t_util.h"
#include <stdio.h>
#include <string.h>

// SE attack (room sound area, "ESE" file) editor of the t_movie REL. No __FILE__ string: the real
// name is unknown (t_se_at.cpp by its function prefix).

void SetToolLight(int on);  // this module's db_light object

// Tool-side view of the SeAt record (block / se number as ints).
struct TSeAt {
    u8 flags;        // 0x00  bit 0 enabled, bit 1 created
    u8 no;           // 0x01
    u16 flags2;      // 0x02
    Vec pos;         // 0x04
    int blk;         // 0x10
    int se_no;       // 0x14
    u16 interval;    // 0x18
    u16 wait;        // 0x1A
    u16 cnt;         // 0x1C
    s16 repeat;      // 0x1E
    u16 rnd_base;    // 0x20
    u16 rnd_range;   // 0x22
    u8 pad_24[0x2C - 0x24];
};

struct SeAtWork {
    u8 mode;          // 0x00  seAtRoutine index
    u8 sub;           // 0x01  sub routine / step
    u8 step;          // 0x02
    u8 step2;         // 0x03
    int frame;        // 0x04
    Vec camPos;       // 0x08  game camera saved on entry
    Vec camAt;        // 0x14
    s8 cursor;        // 0x20  main menu
    s8 editCursor;    // 0x21  area edit menu
    s8 inputCursor;   // 0x22  data input line
    s8 areaNo;        // 0x23  edited area
    s8 rndCursor;     // 0x24  random interval line
    u8 server;        // 0x25  load from x:/ instead of d:/
    s8 stage;         // 0x26
    s8 room;          // 0x27
    u8 yesNo;         // 0x28
    s8 loadCursor;    // 0x29
    u8 pad_2A[2];
    int input;        // 0x2C  sub input active (random interval)
    u8 copySrc;       // 0x30
    u8 copyValid;     // 0x31
    s16 timer;        // 0x32
    s16 x;            // 0x34
    s16 y;            // 0x36
    s16 x0;           // 0x38
    s16 y0;           // 0x3A
    u32 saveStop;     // 0x3C  pG->flags_170
    u32 saveDisp;     // 0x40  pG->flags_58
    int flagCursor;   // 0x44
    char path[0x40];  // 0x48
    SeAtHead head;    // 0x88
    TSeAt area[64];   // 0x98
    SeAtHead fileHead;// 0xB98
    TSeAt file[64];   // 0xBA8
    TSeAt copyBuf;    // 0x16A8
};

static int seAtSaveNum;
static SeAtWork* seAtWk;
#define pW (seAtWk)
static TSeAt* seAtCur;
#define pCur (seAtCur)
static SeAtHead* seAtSaveHead;
static SeAt* seAtSaveList;

static const char* seAtBlockName[7] = {"CORE", "WEAPON", "BGM 0", "BGM 1", "DOOR", "FOOT", "ROOM"};

void seAtInit();
static void seAtExit();
static void seAtMainMenu();
static void seAtAreaEdit();
static void seAtAreaEdit_EditMenu();
static void seAtAreaEdit_AreaMove();
static void seAtAreaEdit_DataInput();
static void seAtAreaEdit_AreaCopy();
static void seAtAreaEdit_AreaPaste();
static void seAtAreaEdit_CopyBuffClear();
static void seAtAreaEdit_AreaDelete();
static void seAtAreaEdit_AreaCreate();
static void seAtDataLoad();
static void seAtDataSave();
static void seAtPreview();
static void preview_init();
static void preview_main();
static void preview_exit();

static void (*seAtRoutine[5])() = {seAtMainMenu, seAtAreaEdit, seAtDataLoad, seAtDataSave, seAtPreview};

// SE attack (room sound point, ESE file) editor entry: init, then every frame the panel position,
// START toggles the debug camera, and seAtRoutine[mode] (0 main menu, 1 area edit, 2 data load,
// 3 data save, 4 preview; EXIT runs seAtExit from the menu).
void ToolSeAt()
{
    {
        // reference view of the work pointer: the memclr argument is then a re-read of the just-stored
        // member (not the forwarded result copy), so the result copy carries the r3 death and sched2 issues
        // `lis seAtWk@ha` before it (the plain member form kept a `mr r3,r0` arg copy through sched2)
        SeAtWork*& wp = seAtWk;
        wp = (SeAtWork*) Debug_alloc(sizeof(SeAtWork), 1);
        memclr_asm(wp, sizeof(SeAtWork));
    }
    seAtInit();
    while (1) {
        pW->x = pW->x0;
        pW->y = pW->y0;
        eprintf(pW->x, pW->y, 5, 0, "SE ATARI EDIT TOOL");
        pW->y += 0x20;
        pW->x += 8;
        seAtRoutine[pW->mode]();
        pW->frame++;
        TaskSleep(1);
    }
}

// Tool start: default tool flags, the work on the Debug heap, the live Snd.se_at_list detached and
// kept (seAtSaveList) for the exit, the game camera saved; starts with DATA LOAD.
void seAtInit()
{
    GlobalWork* g = pG;
    int zero = 0;
    Camera* cam = &g->Camera;

    TutilInitDefault();
    pW->saveStop = pG->Stop_flg;
    pG->Stop_flg |= 0x20000000;
    pG->Stop_flg |= 0x10000000;
    pG->Stop_flg |= 0x800000;
    pG->Stop_flg |= 0x400000;
    pG->Stop_flg |= 0x10000;
    pG->Stop_flg |= 0x2000;
    pW->saveDisp = pG->Disp_flg;
    pG->Disp_flg |= 0x40000000;
    pG->Disp_flg |= 0x80000000;
    pG->Disp_flg |= 0x2000000;
    pG->Disp_flg |= 0x100000;
    DbgFlagOn(pG, DBG_DBG_CAM);
    SetToolLight(1);
    pW->head.magic[0] = 'E';
    pW->head.magic[1] = 'S';
    pW->head.magic[2] = 'E';
    pW->head.magic[3] = zero;
    pW->head.version = 0x100;
    pW->head.num = 64;
    pW->x0 = 0x2D;
    pW->y0 = 0x2D;
    CameraSetOrientationRoll(cam);
    CameraCamposDistance(cam, 2500.0f);
    {
        SeAtHead* head = Snd.pSeAtHeader;
        SeAt* list = Snd.pSeAtData;
        Snd.pSeAtData = NULL;
        Snd.pSeAtHeader = NULL;
        // the original's `lwz pW` waits for the four stores (ours floats it to the block top:
        // alias.c separates the symbol bases); a codeless memory-input anchor gives the load that
        // dependence
        seAtSaveHead = head;
        seAtSaveList = list;
        asm("" : "=m"(seAtWk) : "m"(Snd.pSeAtHeader), "m"(Snd.pSeAtData)); // COMPILER-DIFF: #13 (memory anchor)
    }
    pW->camPos = g->Camera.param.pos;
    pW->camAt = g->Camera.param.at;
    pW->mode = 2;
    pW->sub = zero;
    pW->step = zero;
    pW->step2 = zero;
}

// EXIT: restores the se_at list, camera and flags, frees the work, ends the task.
static void seAtExit()
{
    pG->Disp_flg = pW->saveDisp;
    pG->Stop_flg = pW->saveStop;
    DbgFlagOff(pG, DBG_DBG_CAM);
    SetToolLight(-1);
    Snd.pSeAtHeader = seAtSaveHead;
    Snd.pSeAtData = seAtSaveList;
    TutilQuitDefault();
    TaskExit();
}

static TOOL_MENU seAtMainMenuTbl[5] = {
    {1, "AREA EDIT", NULL},
    {1, "PREVIEW", NULL},
    {1, "DATA LOAD", NULL},
    {1, "DATA SAVE", NULL},
    {1, "EXIT", seAtExit},
};

// Main menu: AREA EDIT / PREVIEW / DATA LOAD / DATA SAVE / EXIT.
static void seAtMainMenu()
{
    s8 sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->cursor, seAtMainMenuTbl, sizeof(seAtMainMenuTbl), &Joy[0]);

    if (sel >= 0) {
        switch (sel) {
        case 0:
            pW->mode = 1;
            break;
        case 1:
            pW->mode = 4;
            break;
        case 2:
            pW->mode = sel;
            break;
        case 3:
            pW->mode = sel;
            break;
        }
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static void (*seAtEditRoutine[4])() = {seAtAreaEdit_EditMenu, seAtAreaEdit_AreaMove, seAtAreaEdit_DataInput,
                                        seAtAreaEdit_AreaCreate};

// AREA EDIT: L/R pick the record (areaNo), its point and number drawn; sub routines edit menu,
// area move, data input, area create.
static void seAtAreaEdit()
{
    // one array, not `Vec a, b`: two aggregate locals are 8-aligned slots (8 and 24), the target's
    // pair sits at 8 and 20 (one 24-byte object); `cam` is a per-arm local so it is a block-local qty
    // that local-alloc gives r30 (a function-scope `cam` set in both arms is global and takes r31)
    Vec v[2];

    u8 no = pW->areaNo;  // read at the top, used by the first arm only (the target's `lbz` above the `sub` test)

    if (pW->sub == 0) {
        if (Joy[0].rep2 & JOY_R) pW->areaNo = no + 1;
        if (Joy[0].rep2 & JOY_L) pW->areaNo--;
        pW->areaNo = pW->areaNo < 0 ? 63 : (pW->areaNo > 63 ? 0 : pW->areaNo);
    }
    pCur = &pW->area[pW->areaNo];
    eprintf(pW->x, pW->y, 4, 0, "AREA[ %d ]", pW->areaNo);
    if (pCur->flags & 1) {
        f32 dist;
        Camera* cam = &pG->Camera;
        dist = cam->Distance;
        cam->param.at = pCur->pos;
        CameraSetOrientationRoll(cam);
        CameraCamposDistance(cam, dist);
        eprintf(pW->x + 0x58, pW->y, 0, 0, "POS( %f, %f, %f )", pCur->pos.x, pCur->pos.y, pCur->pos.z);
        v[0] = pCur->pos;
        v[1] = pCur->pos;
        v[0].x += 200.0f;
        v[1].x -= 200.0f;
        Draw_line3d(&v[0], &v[1], 0xFFFFFF00, 0);
        v[0] = pCur->pos;
        v[1] = pCur->pos;
        v[0].y += 200.0f;
        v[1].y -= 200.0f;
        Draw_line3d(&v[0], &v[1], 0xFFFF00FF, 0);
        v[0] = pCur->pos;
        v[1] = pCur->pos;
        v[0].z += 200.0f;
        v[1].z -= 200.0f;
        Draw_line3d(&v[0], &v[1], 0xFF00FFFF, 0);
    } else {
        Camera* cam = &pG->Camera;
        cam->param.pos = pW->camPos;
        cam->param.at = pW->camAt;
        CameraSetOrientationRoll(cam);
        CameraCamposDistance(cam, 2500.0f);
        eprintf(pW->x + 0x58, pW->y, 2, 0, "NO DATA:");
    }
    pW->x += 8;
    pW->y += 0x20;
    seAtEditRoutine[pW->sub]();
}

static TOOL_MENU seAtCreateMenu[3] = {
    {1, "AREA CREATE", NULL},
    {0, "AREA PASTE", seAtAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", seAtAreaEdit_CopyBuffClear},
};

static TOOL_MENU seAtEditMenu[6] = {
    {1, "AREA MOVE", NULL},
    {1, "DATA INPUT", NULL},
    {1, "AREA COPY", seAtAreaEdit_AreaCopy},
    {0, "AREA PASTE", seAtAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", seAtAreaEdit_CopyBuffClear},
    {1, "AREA DELETE", seAtAreaEdit_AreaDelete},
};

// Record menu: existing -> AREA MOVE / DATA INPUT / AREA COPY / PASTE / COPY BUFF CLEAR / AREA
// DELETE, empty -> AREA CREATE / PASTE / CLEAR; B back.
static void seAtAreaEdit_EditMenu()
{
    s8 sel;
    u8 valid = pW->copyValid;
    // pointer locals, the create menu declared first: the address pseudo created first takes r8, and
    // the four stores then come out last-statement-first (c[1], e[4], e[3], c[2]) like the target
    TOOL_MENU* c = seAtCreateMenu;
    TOOL_MENU* e = seAtEditMenu;

    e[4].Be_flg = valid;
    e[3].Be_flg = valid;
    c[2].Be_flg = valid;
    c[1].Be_flg = valid;
    if (pCur->flags & 1) {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, seAtEditMenu, sizeof(seAtEditMenu), &Joy[0]);
        switch (sel) {
        case 0:
            pW->sub = 1;
            pW->step = 0;
            pW->step2 = 0;
            break;
        case 1:
            pW->sub = 2;
            pW->step = 0;
            pW->step2 = 0;
            break;
        }
    } else {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, seAtCreateMenu, sizeof(seAtCreateMenu), &Joy[0]);
        if (sel == 0) {
            pW->sub = 3;
            pW->step = 0;
            pW->step2 = 0;
        }
    }
    if (Joy[0].trg & JOY_B) {
        pW->mode = 0;
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

// AREA MOVE: the camera target is the point: stick / sub stick move and orbit the camera, the
// record's pos follows the camera target; B back.
static void seAtAreaEdit_AreaMove()
{
    GlobalWork* g = pG;
    JOY* joy = &Joy[0];
    Vec right;
    Vec up;
    Vec dir;
    Vec t;
    Vec d = {0.0f, 0.0f, 0.0f};
    Camera* cam = &g->Camera;

    {
        // written through a pointer: the original stores right.y/.z via `addi r9,r1,8` (cse keeps
        // `(mem (plus P 4))`, only the offset-0 store folds to the frame address)
        Vec* rp = &right;
        rp->x = g->Camera.mat[0][0];
        rp->y = g->Camera.mat[1][0];
        rp->z = g->Camera.mat[2][0];
    }
    up.x = g->Camera.mat[0][1];
    up.y = g->Camera.mat[1][1];
    up.z = g->Camera.mat[2][1];
    dir.x = g->Camera.mat[0][2];
    dir.y = g->Camera.mat[1][2];
    dir.z = g->Camera.mat[2][2];
    if (joy->on & (JOY_R | JOY_L)) {
        f32 dist = cam->Distance;
        if (joy->on & JOY_R) {
            dist -= joy->triggerRight * 3.0f;
        } else {
            dist += joy->triggerLeft * 3.0f;
        }
        if (dist < 200.0f) dist = 200.0f;
        CameraCamposDistance(cam, dist);
    }
    if (joy->stickX) {
        d.x = (f32) joy->stickX * 5.0f;
    }
    if (joy->stickY) {
        if (joy->on & JOY_Z) {
            d.y = (f32) joy->stickY * 5.0f;
        } else {
            PSVECScale(&dir, &t, (f32) joy->stickY * -5.0f);
            CameraDolly(cam, &t);
        }
    }
    if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) {
        PSMTXMultVecSR(cam->mat, &d, &d);
        CameraDolly(cam, &d);
    }
    if (joy->substickX) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(cam, &axis, &cam->param.at, (f32) joy->substickX * 0.05f * 0.017453292f);
    }
    if (joy->substickY) {
        CameraCamposRot(cam, 'x', (f32) joy->substickY * -0.05f * 0.017453292f);
    }
    pCur->pos = cam->param.at;
    if (Joy[0].trg & JOY_B) {
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static const char* seAtInputName[6] = {"SE BLOCK ", "SE NO    ", "INTERVAL ", "WAIT TIME", "CALL NUM ", "FLAG     "};
static const char* seAtRndName[2] = {"RANDOM BASE    ", "RANDOM INTERVAL"};
static const char* seAtFlagName[16] = {"NO SET POS", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};

// pad masks of the +/- inputs (the sub stick bits are the header's SLEFT/SRIGHT swapped)

// +-1 / +-10 (with X) on a value driven by the fast auto-repeat
#define SEAT_STEP(v)                          \
    if (Joy[0].rep2 & REP_RIGHT) {            \
        if (Joy[0].on & JOY_X) v += 10;       \
        else v += 1;                          \
    } else if (Joy[0].rep2 & REP_LEFT) {      \
        if (Joy[0].on & JOY_X) v -= 10;       \
        else v -= 1;                          \
    }

// DATA INPUT rows: BLOCK (CORE / WEAPON / BGM 0 / BGM 1 / DOOR / FOOT / ROOM), REQUEST NO,
// INTERVAL (a value or RANDOM: base + max), DELAY TIME, CALL NUM (0 infinite, -1 none), FLAG bits;
// up/down pick, left/right change; B back.
static void seAtAreaEdit_DataInput()
{
    int col;
    u32 i;
    int j;
    s16 x;
    s16 y;
    int v;
    int n;
    int num;

    // `num = 6` HERE, not at the loop: cse1's ebb from the arms' join does not know it, so the name loop's
    // entry test `0 < num` survives gcse (the (u8) col mask is then not anticipated at the loop entry and stays
    // in the body); gcse's cprop gives the compare its 6 and cse2 folds the test away. `v` is one
    // function-scope variable for every case (it conflicts with the kept Joy pointer r9 of cases 1-4, so
    // case 0's `lwz v` lands in r11 too); `n` is per-arm (ties to the dying v without a `mr`).
    num = 6;
    if (pW->input == 0) {
        if (Joy[0].trg & JOY_DOWN) {
            pW->inputCursor++;
            pW->flagCursor = 0;
        } else if (Joy[0].trg & JOY_UP) {
            pW->inputCursor--;
            pW->flagCursor = 0;
        }
        // cursor clamps: one pointer local, one store at the join. The store then shares the sched2 block with
        // `col = 0` (anti-dependence on the pointer's r9 ranks the store above the next `lis r9`, and `li col,0`
        // fills the second issue slot before it); the two-store form cross-jumps the stores behind a label.
        {
            SeAtWork* w = pW;
            int c = w->inputCursor;
            if (c >= 0) {
                if (c > 5) c = 5;
            } else {
                c = 0;
            }
            w->inputCursor = c;
        }
        col = 0;
        if (pW->frame & 0x18) {
            eprintf(pW->x - 8, pW->y + pW->inputCursor * 16, 0, 0, ">");
        }
    } else {
        col = 7;
        eprintf(pW->x - 8, pW->y + pW->inputCursor * 16, 7, 0, ">");
    }
    // int col + (u8) casts: the original masks the colour at every eprintf (`clrlwi r5,col,24`; the tail's
    // first mask is PRE-shared as `mr col,r5`), a promoted u8 would pass unmasked. The name loops use the
    // `j * 16` giv (init `li 0` issued last in the preheader, after gcse's inserts).
    for (j = 0; j < num; j++) {
        eprintf(pW->x, pW->y + j * 16, (u8) col, 0, "%s", seAtInputName[j]);
    }
    switch (pW->inputCursor) {
    case 0:
        v = pCur->blk;
        if (Joy[0].rep & REP_RIGHT) v++;
        else if (Joy[0].rep & REP_LEFT) v--;
        if (v >= 0) {
            n = v;
            if (n > 6) n = 6;
        } else {
            n = 0;
        }
        pCur->blk = n;
        break;
    case 1:
        if (pW->step == 0) {
            // cases 1-4 clamp through per-arm `v`/`n` locals (block-local qtys: n ties to the dying v,
            // `ori r11,r11,0x8000` without the `mr`); case 0 keeps the function-scope pair (`mr r0,r11`)
            int n;
            v = pCur->se_no;
            SEAT_STEP(v);
            if (v >= 0) {
                n = v;
                if (n > 0x8000) n = 0x8000;
            } else {
                n = 0;
            }
            pCur->se_no = n;
            eprintf(0x30, 0x190, 0, 0, "REQUEST NO");
        }
        break;
    case 2:
        switch (pW->step) {
        case 0: {
            int n;
            pW->input = 0;
            v = pCur->interval;
            SEAT_STEP(v);
            if (v >= 0) {
                n = v;
                if (n > 0x8000) n = 0x8000;
            } else {
                n = 0;
            }
            pCur->interval = n;
            eprintf(0x30, 0x190, 0, 0, "INTERVAL (1-65535 or RANDOM)");
            if (pCur->interval == 0 && (Joy[0].trg & JOY_A)) {
                pW->step++;
                pW->rndCursor = 0;
                pW->input = 1;
            }
            break;
        }
        case 1:
            num = 2;
            for (j = 0; j < num; j++) {
                eprintf(0x38, 0xF0 + j * 16, 0, 0, "%s", seAtRndName[j]);
            }
            if (Joy[0].trg & JOY_DOWN) pW->rndCursor++;
            else if (Joy[0].trg & JOY_UP) pW->rndCursor--;
            {
                SeAtWork* w = pW;
                int c = w->rndCursor;
                if (c >= 0) {
                    if (c > 1) c = 1;
                } else {
                    c = 0;
                }
                w->rndCursor = c;
            }
            if (pW->frame & 0x18) {
                eprintf(0x30, 0xF0 + pW->rndCursor * 16, 0, 0, ">");
            }
            switch (pW->rndCursor) {
            case 0: {
                int n;
                v = pCur->rnd_base;
                SEAT_STEP(v);
                if (v >= 0) {
                    n = v;
                    if (n > 0xFFFF) n = 0xFFFF;
                } else {
                    n = 0;
                }
                pCur->rnd_base = n;
                eprintf(0x30, 0x190, 0, 0, "RANDOM BASE VALUE(0-65535)");
                break;
            }
            case 1: {
                int n;
                v = pCur->rnd_range;
                SEAT_STEP(v);
                if (v > 0) {
                    n = v;
                    if (n > 0xFFFF) n = 0xFFFF;
                } else {
                    n = 1;
                }
                pCur->rnd_range = n;
                eprintf(0x30, 0x190, 0, 0, "RANDOM MAX INTERVAL(1-65535,+BASE VALUE)");
                break;
            }
            }
            if (Joy[0].trg & JOY_B) pW->step--;
            eprintf(0xC8, 0xF0, 0, 0, "%d", pCur->rnd_base);
            eprintf(0xC8, 0x100, 0, 0, "%d", pCur->rnd_range);
            break;
        }
        break;
    case 3: {
        int n;
        v = pCur->wait;
        SEAT_STEP(v);
        if (v >= 0) {
            n = v;
            if (n > 0x8000) n = 0x8000;
        } else {
            n = 0;
        }
        pCur->wait = n;
        eprintf(0x30, 0x190, 0, 0, "DELAY TIME (0 - 65535)");
        break;
    }
    case 4: {
        int n;
        v = pCur->repeat;
        SEAT_STEP(v);
        if (v >= -1) {
            n = v;
            if (n > 0x7FFF) n = 0x7FFF;
        } else {
            n = -1;
        }
        pCur->repeat = n;
        eprintf(0x30, 0x190, 0, 0, "CALL NUM (1 - 32767,0:INFINITY,-1:NO CALL)");
        break;
    }
    case 5:
        x = pW->x + 0x60;
        y = pW->y + 0x50;
        if (Joy[0].rep & REP_RIGHT) pW->flagCursor--;
        if (Joy[0].rep & REP_LEFT) pW->flagCursor++;
        {
            SeAtWork* w = pW;
            int c = w->flagCursor;
            if (c >= 0) {
                if (c > 15) c = 15;
            } else {
                c = 0;
            }
            w->flagCursor = c;
        }
        for (i = 0; i < 16; i++) {
            eprintf(x + i * 8, y, (pW->flagCursor == 15 - i) ? 4 : 0, 0, "%d", (pCur->flags2 >> (15 - i)) & 1);
        }
        eprintf(x + 0x90, y, 4, 0, "%s", seAtFlagName[pW->flagCursor]);
        if (Joy[0].trg & JOY_A) {
            pCur->flags2 ^= 1 << pW->flagCursor;
        }
        break;
    }
    x = pW->x + 0x60;
    y = pW->y;
    if (pW->input == 0) {
        col = 0;
        if (Joy[0].trg & JOY_B) {
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
        }
    } else {
        col = 7;
    }
    eprintf(x, y, (u8) col, 0, "%s", (u32) pCur->blk <= 6 ? seAtBlockName[pCur->blk] : "...no string");
    y += 16;
    eprintf(x, y, (u8) col, 0, "%d", pCur->se_no);
    y += 16;
    if (pCur->interval == 0) {
        eprintf(x, y, (u8) col, 0, "RANDOM  >>");
    } else {
        eprintf(x, y, (u8) col, 0, "%d", pCur->interval);
    }
    y += 16;
    eprintf(x, y, (u8) col, 0, "%d", pCur->wait);
    y += 16;
    switch (pCur->repeat) {
    case -1:
        eprintf(x, y, (u8) col, 0, "NO CALL");
        break;
    case 0:
        eprintf(x, y, (u8) col, 0, "INFINITY");
        break;
    default:
        eprintf(x, y, (u8) col, 0, "%d", pCur->repeat);
        break;
    }
    if (pW->inputCursor != 5) {
        u32 k;
        y += 16;
        for (k = 0; k < 16; k++) {
            eprintf(x + k * 8, y, 0, 0, "%d", (pCur->flags2 >> (15 - k)) & 1);
        }
    }
}

// Copies the record into the copy buffer.
static void seAtAreaEdit_AreaCopy()
{
    pW->copyBuf = *pCur;
    pW->copySrc = pW->areaNo;
    pW->copyValid = 1;
}

// Overwrites the record with the copy buffer.
static void seAtAreaEdit_AreaPaste()
{
    *pCur = pW->copyBuf;
}

// Empties the copy buffer.
static void seAtAreaEdit_CopyBuffClear()
{
    memclr_asm(&pW->copyBuf, sizeof(TSeAt));
    pW->copySrc = 0;
    pW->copyValid = 0;
}

// Clears the record.
static void seAtAreaEdit_AreaDelete()
{
    pCur->flags &= ~1;
    pW->editCursor = 0;
}

// Creates the record at the camera target with default values.
static void seAtAreaEdit_AreaCreate()
{
    pCur->pos = pG->Camera.param.at;
    pCur->flags |= 3;
    pW->editCursor = 0;
    pW->sub = 0;
    pW->step = 0;
    pW->step2 = 0;
}

// DATA LOAD: server (x:) / local (d:), stage and room picked with the d-pad, "DATA LOAD OK?";
// reads r<room>.ese into the work and installs it as Snd.se_at_list.
static void seAtDataLoad()
{
    u16 roomId = (pW->stage << 8) | pW->room;
    int ret;
    u32 i;

    eprintf(pW->x, pW->y, 4, 0, "[DATA LOAD]");
    switch (pW->sub) {
    case 0:
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
        pW->stage = pG->stage_no;
        pW->room = pG->room_no;
        pW->server = 1;
    case 1:
        if (Joy[0].rep2 & REP_UP) {
            // case 0 toggles the server like the DOWN switch: the target's `ble` from this tree lands in
            // the DOWN tree's `cmpwi 0; beq` (jump2 cross-jumped the identical case-0 paths)
            switch (pW->loadCursor) {
            case 0:
                pW->server ^= 1;
                break;
            case 1:
                pW->stage++;
                break;
            case 2:
                pW->room++;
                break;
            }
        } else if (Joy[0].rep2 & REP_DOWN) {
            switch (pW->loadCursor) {
            case 0:
                pW->server ^= 1;
                break;
            case 1:
                pW->stage--;
                break;
            case 2:
                pW->room--;
                break;
            }
        } else if (Joy[0].trg & REP_LEFT) {
            pW->loadCursor--;
        } else if (Joy[0].trg & REP_RIGHT) {
            pW->loadCursor++;
        } else if (Joy[0].trg & JOY_B) {
            pW->mode = 0;
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
        } else if (Joy[0].trg & JOY_A) {
            pW->sub = 2;
            pW->step = 0;
            pW->step2 = 0;
            pW->yesNo = 1;
        }
        pW->loadCursor = pW->loadCursor < 0 ? 0 : (pW->loadCursor > 2 ? 2 : pW->loadCursor);
        pW->stage = pW->stage < 0 ? 0 : (pW->stage > 9 ? 9 : pW->stage);
        pW->room = pW->room < 0 ? 0 : (pW->room > 0x70 ? 0x70 : pW->room);
        if (pW->server == 0) {
            sprintf(pW->path, "d:/bio4/room/st%1x/r%03x/r%03x.ese", pW->stage, roomId, roomId);
        } else {
            sprintf(pW->path, "x:/soft/room/st%1x/r%03x/r%03x.ese", pW->stage, roomId, roomId);
        }
        break;
    case 2:
        eprintf(pW->x, pW->y + 0x60, 0, 0, "DATA LOAD OK?");
        eprintf(pW->x, pW->y + 0x70, pW->yesNo == 0 ? 6 : 7, 0, "YES");
        eprintf(pW->x + 0x28, pW->y + 0x70, pW->yesNo == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & JOY_B) {
            pW->sub = 1;
            pW->step = 0;
            pW->step2 = 0;
        } else if (Joy[0].trg & JOY_A) {
            // the step stores repeated in both arms: separate `stb` arms (no jump.c else-set hoist),
            // their tails cross-jumped with the JOY_B arm's
            if (pW->yesNo == 0) {
                pW->sub = 3;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->sub = 1;
                pW->step = 0;
                pW->step2 = 0;
            }
        } else if (Joy[0].trg & (REP_LEFT | REP_RIGHT)) {
            pW->yesNo ^= 1;
        }
        break;
    case 3:
        ret = HDRead(pW->path, &pW->fileHead);
        if (ret == 0) {
            pLog->err(0, 0, "%s : LOAD ERROR !!!!", pW->path);
            pW->sub = 9;
            pW->step = ret;
            pW->step2 = ret;
        } else {
            if (strcmp(pW->fileHead.magic, "ESE") != 0) {
                pW->sub = 9;
                pW->step = 0;
                pW->step2 = 0;
                break;
            }
            for (i = 0; i < pW->fileHead.num; i++) {
                pW->area[pW->file[i].no] = pW->file[i];
            }
            pW->sub = 8;
            pW->step = 0;
            pW->step2 = 0;
        }
        pW->timer = 30;
        break;
    case 8:
        eprintf(pW->x, pW->y + 0x60, 6, 0, "DATA LOAD COMPLETE.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->mode = 0;
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
        }
        pW->timer--;
        break;
    case 9:
        eprintf(pW->x, pW->y + 0x60, 2, 0, "DATA LOAD ERROR.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->sub = 1;
            pW->step = 0;
            pW->step2 = 0;
        }
        pW->timer--;
        break;
    }
    {
        int col5;  // COMPILER-DIFF: 2 (the original masks the u8 colour at the join; the draw_light_graph form)
        int x = pW->x;
        int y = pW->y + 0x20;
        col5 = pW->sub == 1 ? (pW->loadCursor == 0 ? 6 : 0) : 0;
        eprintf(x, y, (u8) col5, 0, "%s", pW->server == 0 ? "LOCAL" : "SERVER");
    }
    eprintf(pW->x + 0x40, pW->y + 0x20, pW->sub == 1 ? (pW->loadCursor == 1 ? 6 : 0) : 0, 0, "STAGE %2d", pW->stage);
    eprintf(pW->x + 0x90, pW->y + 0x20, pW->sub == 1 ? (pW->loadCursor == 2 ? 6 : 0) : 0, 0, "ROOM %02x", pW->room);
    eprintf(pW->x, pW->y + 0x40, 0, 0, "%s", pW->path);
}

static TOOL_MENU seAtSaveMenu[3] = {
    {1, "SERVER", NULL},
    {1, "LOCAL", NULL},
    {1, "DON'T SAVE", NULL},
};

// DATA SAVE: SERVER / LOCAL / DON'T SAVE; packs the live records (header + SeAt) and writes them.
static void seAtDataSave()
{
    char pathX[0x40];
    char pathD[0x40];
    int ret = 0;
    u32 i;
    s8 sel;

    eprintf(pW->x, pW->y, 4, 0, "[DATA SAVE]");
    pW->y += 0x10;
    sprintf(pathX, "x:\\soft\\room\\st%1x\\r%03x\\r%03x.ese", pG->stage_no, pG->room_id, pG->room_id);
    sprintf(pathD, "d:\\bio4\\room\\st%1x\\r%03x\\r%03x.ese", pG->stage_no, pG->room_id, pG->room_id);
    switch (pW->sub) {
    case 0:
        seAtSaveNum = 0;
        for (i = 0; i < 64; i++) {
            if (pW->area[i].flags & 1) {
                pW->area[i].no = i;
                pW->file[seAtSaveNum] = pW->area[i];
                // reference view: the counter's load stays below the block copy's stores (a MEM with
                // neither the struct nor the scalar flag); an inline helper form differs by 2 words
                { int& n = seAtSaveNum; n++; }
            }
        }
        pW->fileHead.magic[0] = 'E';
        pW->fileHead.magic[1] = 'S';
        pW->fileHead.magic[2] = 'E';
        pW->fileHead.magic[3] = 0;
        pW->fileHead.version = 0x100;
        { int& n = seAtSaveNum; pW->fileHead.num = n; }  // same: the `lhz` stays below the version store
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
    case 1:
        sel = ToolMenuDisp(pW->x, pW->y, 0, seAtSaveMenu, sizeof(seAtSaveMenu), &Joy[0]);
        eprintf(pW->x + 0x64, pW->y, 6, 0, "%s", pathX);
        eprintf(pW->x + 0x64, pW->y + 0x10, 6, 0, "%s", pathD);
        if (sel >= 0) {
            switch (sel) {
            case 0:
            case 1:
                ret = HDWrite_only(pathX + sel * 0x40, &pW->fileHead, seAtSaveNum * sizeof(TSeAt) + 0x10);
                break;
            case 2:
                pW->mode = 0;
                pW->sub = 0;
                pW->step = 0;
                pW->step2 = 0;
                return;
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
        }
        if (Joy[0].trg & JOY_B) {
            pW->mode = 0;
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
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

static void (*seAtPreviewRoutine[3])() = {preview_init, preview_main, preview_exit};

// PREVIEW: init / main / exit sub routines.
static void seAtPreview()
{
    seAtPreviewRoutine[pW->sub]();
}

// Un-pauses the game and gives the sound driver the edited list (Snd.se_at_list = the tool image).
static void preview_init()
{
    u32 i;
    int n = 0;

    pG->Stop_flg &= ~0x10000000;
    pG->Disp_flg &= ~0x40000000;
    DbgFlagOff(pG, DBG_DBG_CAM);
    for (i = 0; i < 64; i++) {
        if (pW->area[i].flags & 1) {
            pW->area[i].no = i;
            pW->file[n] = pW->area[i];
            n++;
        }
    }
    pW->fileHead.magic[0] = 'E';
    pW->fileHead.magic[1] = 'S';
    pW->fileHead.magic[2] = 'E';
    pW->fileHead.magic[3] = 0;
    pW->fileHead.version = 0x100;
    pW->fileHead.num = n;
    Snd.pSeAtHeader = &pW->fileHead;
    Snd.pSeAtData = (SeAt*) pW->file;
    pW->sub++;
}

// PREVIEW MODE: the game runs with the edited sound points; START ends it.
static void preview_main()
{
    pW->timer++;
    if (pW->timer & 8) {
        eprintf(0xD0, 0x10, 6, 0, "PREVIEW MODE");
    }
    if (Joy[0].trg & JOY_START) {
        pW->sub++;
    }
}

// Re-pauses the game, back to the main menu.
static void preview_exit()
{
    pG->Stop_flg |= 0x20000000;
    pG->Stop_flg |= 0x10000000;
    pG->Stop_flg |= 0x800000;
    pG->Stop_flg |= 0x400000;
    pG->Stop_flg |= 0x10000;
    pG->Stop_flg |= 0x2000;
    pG->Disp_flg |= 0x40000000;
    pG->Disp_flg |= 0x80000000;
    DbgFlagOn(pG, DBG_DBG_CAM);
    Snd.pSeAtHeader = NULL;
    Snd.pSeAtData = NULL;
    pW->mode = 0;
}
