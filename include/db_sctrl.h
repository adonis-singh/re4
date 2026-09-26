#ifndef DB_SCTRL_H
#define DB_SCTRL_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "hermite.h"

// Hermite S-curve editor of the debug tools (tools/db_sctrl.cpp, D:/Bio4/Prog/db_sctrl.cpp; the same
// object in t_id and t_event): a 2D graph drawn in world space in front of the camera, a screen-space
// cursor, key points with in/out tangent handles and a settings menu.
struct DbSctrlWork {
    s8 routine;      // 0x00  0 edit, 1 menu, 2 quit
    s8 step;         // 0x01
    s8 x2;
    s8 x3;
    u8 pad_4[4];
    int x;           // 0x08  menu position
    int y;           // 0x0C
    s8 cursor;       // 0x10  menu row
    s8 sub;          // 0x11  menu column
    u8 blink;        // 0x12  frame counter (bits 3/4 blink the cursor)
    u8 pad_13;
    Vec pos;         // 0x14  screen-space cursor
    u8 pad_20[4];
    f32 yMax;        // 0x24  graph range
    f32 yMin;        // 0x28
    f32 xMax;        // 0x2C
    f32 xMin;        // 0x30
    f32 gridX;       // 0x34  drawn grid spacing (0 = none)
    f32 gridY;       // 0x38
    Vec grid;        // 0x3C  grid-lock step (x, y)
    f32 scaleX;      // 0x48  Scale menu factors
    f32 scaleY;      // 0x4C
    Mtx mtx;         // 0x50  screen -> world (the camera matrix moved in front of the camera)
    Hermite1* curve; // 0x80
    s8 grab;         // 0x84  grabbed key, -1 = none
    s8 insertIdx;    // 0x85  insertion index found on the curve, -1 = none
    u8 pad_86[2];
    Vec insertPos;   // 0x88
    u8 yes;          // 0x94  YES/NO of the delete / insert / clear prompts
    u8 pad_95[3];
    u32 flags;       // 0x98  bit0 grid lock, bit1 automatic range
    char labelX[8];  // 0x9C
    char labelY[8];  // 0xA4
};

void SctrlInitAxisRange(DbSctrlWork* w, f32 xMax, f32 xMin, f32 yMax, f32 yMin);
void SctrlAdjustAxisRange(DbSctrlWork* w);
void SctrlSetAxisLabel(DbSctrlWork* w, const char* x, const char* y);
void SctrlInitCursor(DbSctrlWork* w, f32 x, f32 y);
void dbSctrlScreenOrientation(DbSctrlWork* w, CAMERA* cam, f32 fovy);
// returns the routine's result (0 once the editor quits; t_event's fog / focus tools test it)
int DbSctrl(DbSctrlWork* w, int x, int y);
int grabPoint(DbSctrlWork* w);
int grabLine(DbSctrlWork* w);
void deletePoint(DbSctrlWork* w);
void insertPoint(DbSctrlWork* w);
void drawCursor(DbSctrlWork* w);
void drawAxis(DbSctrlWork* w);
void drawScurve(DbSctrlWork* w);
void posScreen2Graph(DbSctrlWork* w, Vec* scr, Vec* gph);
void posGraph2Screen(DbSctrlWork* w, Vec* gph, Vec* scr);
void posScreen2World(DbSctrlWork* w, Vec* scr, Vec* out);
void posGraph2World(DbSctrlWork* w, Vec* gph, Vec* out);
void posGridLock(Vec* grid, Vec* in, Vec* out);
void posScreen2GridLock(DbSctrlWork* w, Vec* scr, Vec* gph);

#endif
