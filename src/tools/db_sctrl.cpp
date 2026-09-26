#include "types.h"
#include "vec.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "math_sub.h"
#include "hermite.h"
#include "dbmodule.h"
#include "db_log.h"
#include "db_sctrl.h"
#include <string.h>

// Hermite S-curve editor of the debug tools (D:/Bio4/Prog/db_sctrl.cpp; the same object in t_id and
// t_event). Screen space is 640x480 centred on the graph centre; the graph is drawn in world space
// through w->mtx (the camera matrix pushed 240 / tan(fovy / 2) in front of the camera).

#define SCTRL_MAX_KEY 64
#define SCTRL_GRAB_DIST 10.0f
#define SCTRL_HANDLE_LEN 10.0f
#define SCTRL_HANDLE_DRAW 40.0f
#define SCTRL_SCR_W 640.0f
#define SCTRL_SCR_H 480.0f
#define SCTRL_LINE_COL 0xFEFFFFFF
#define SCTRL_GRID_COL 0xFE404040

static int sctrlEdit(DbSctrlWork* w);
static int sctrlMenu(DbSctrlWork* w);
static int sctrlQuit(DbSctrlWork* w);

// Never called: the object starts with the pool of a function the original link dead-stripped (an
// int -> float conversion and 1.0f; STRIP_UNUSED in config/G4BE08/modules.py).
static f32 sctrlIndexF(int i)
{
    return (f32) i + 1.0f;
}


static int (*sctrl_routine_tbl[3])(DbSctrlWork*) = {sctrlEdit, sctrlMenu, sctrlQuit};

// Sets the graph's visible range (x = key time, y = key value) by hand.
void SctrlInitAxisRange(DbSctrlWork* w, f32 xMax, f32 xMin, f32 yMax, f32 yMin)
{
    w->xMax = xMax;
    w->xMin = xMin;
    w->yMax = yMax;
    w->yMin = yMin;
}

// Automatic range: x up to 1.2 x the last key time (down to -0.2 x), y around the value extremes.
void SctrlAdjustAxisRange(DbSctrlWork* w)
{
    Hermite1* c;
    f32 xmax;
    f32 xmin;
    f32 ymax;
    f32 ymin;
    f32 d;
    int i;

    if (!(w->flags & 2)) {
        return;
    }
    c = w->curve;
    if (c->num <= 1) {
        return;
    }
    xmax = c->key[0].t;
    ymin = c->key[0].v;
    xmin = xmax;
    ymax = ymin;
    // the .v reads go through `w->curve` (hoisted by loop.c into its own pseudo): the .t and .v address
    // givs then have different base registers and loop.c cannot combine them into one pointer with a
    // -4 displacement -- the target steps two pointers (&key[i].t at +0x14 and &key[i].v at +0x18)
    for (i = 1; i < c->num; i++) {
        if (c->key[i].t <= xmin) {
            xmin = c->key[i].t;
        }
        if (c->key[i].t >= xmax) {
            xmax = c->key[i].t;
        }
        if (w->curve->key[i].v <= ymin) {
            ymin = w->curve->key[i].v;
        }
        if (w->curve->key[i].v >= ymax) {
            ymax = w->curve->key[i].v;
        }
    }
    w->xMax = xmax * 1.2f;
    w->xMin = xmax * -0.2f;
    // the select as a ternary temp (a named `m` is a global pseudo that local-alloc cannot see, so the 0.1
    // pool load takes f0 and m falls to f13; the target has m in f0, the constant in f13)
    d = (fabsf(ymax) > fabsf(ymin) ? fabsf(ymax) : fabsf(ymin)) * 0.1f;
    w->yMax = ymax + d;
    w->yMin = ymin - d;
}

// Axis labels (up to 7 chars each) printed by drawAxis.
void SctrlSetAxisLabel(DbSctrlWork* w, const char* x, const char* y)
{
    strcpy(w->labelX, x);
    strcpy(w->labelY, y);
}

// Puts the screen cursor at graph coordinates (x, y).
void SctrlInitCursor(DbSctrlWork* w, f32 x, f32 y)
{
    Vec g = {0.0f, 0.0f, 0.0f};

    g.x = x;
    g.y = y;
    posGraph2Screen(w, &g, &w->pos);
}

// Places the graph plane 240 / tan(fovy / 2) in front of the camera, facing it.
void dbSctrlScreenOrientation(DbSctrlWork* w, CAMERA* cam, f32 fovy)
{
    Vec dir;
    Vec pos;
    f32 dist;

    dist = 240.0 / tan(fovy * 0.5f * (PI / 180.0f));
    getColumn(cam->mat, 2, &dir);
    PSVECScale(&dir, &dir, -dist);
    getColumn(cam->mat, 3, &pos);
    PSVECAdd(&pos, &dir, &pos);
    {
        MtxPtr d_ = w->mtx;
        MtxPtr s_ = cam->mat;
        int i_ = 3;
        int j_;
        f32* sp_;
        f32* dp_;
        while (i_--) {
            dp_ = *d_;
            sp_ = *s_;
            for (j_ = 0; j_ < 4; j_++) {
                *dp_++ = *sp_++;
            }
            d_++;
            s_++;
        }
    }
    w->mtx[0][3] = pos.x;
    w->mtx[1][3] = pos.y;
    w->mtx[2][3] = pos.z;
}

// Runs the S-curve editor one frame: menu at text position (x, y), graph plane in front of the
// camera, axis / curve / cursor drawn, then the routine (0 sctrlEdit, 1 sctrlMenu, 2 sctrlQuit);
// returns the routine's result (0 once the editor quit).
int DbSctrl(DbSctrlWork* w, int x, int y)
{
    w->x = x;
    w->blink++;
    w->y = y;
    // struct-view read: the pG load then depends on the three member stores above (sched1 true
    // dependence), so `stw r4,x` loses the anti-dependence bonus of the later `lwz r4,pG` and the
    // stores come out in RTL order (y, x)
    dbSctrlScreenOrientation(w, &pG->Camera, pG->Camera.param.fovy);
    drawAxis(w);
    drawScurve(w);
    if (w->routine == 0) {
        drawCursor(w);
    }
    return sctrl_routine_tbl[w->routine](w);
}

// defined here: the menu strings follow SctrlAdjustAxisRange's constant pool in .rodata
static const char* sctrl_menu_name[6] = {"Scale  :", "Grid   :", "Reverse:", "Offset :", "Range  :", "Clear  :"};
// unreferenced (db_path.cpp's edit-mode static, kept by the compiler)
static s8 sctrl_edit_mode = 0;

// Routine 2: resets the editor state and returns 0 (the caller closes the editor).
static int sctrlQuit(DbSctrlWork* w)
{
    w->routine = w->step = w->x2 = w->x3 = 0;
    return 0;
}

// Routine 1, the Z menu (pad 1): rows Scale (x/y factors applied to every key), Grid (grid-lock
// step and drawn grid), Reverse (mirror the keys in time / value), Offset (shift all keys), Range
// (auto or manual axis range), Clear (delete all keys, YES/NO). step 0 picks a row (B leaves),
// 1 edits its columns with left/right / the stick, A applies. Returns 1.
static int sctrlMenu(DbSctrlWork* w)
{
    JOY* joy = &Joy[0];
    Hermite1* c = w->curve;
    f32 tx = c->key[0].t;
    f32 ty = c->key[0].v;
    int x;
    int y;
    int i;
    // 18 dead pool labels: the "%s" string must hash below `sctrl_menu_name` in gcse's expression
    // table (.LC40+, not .LC22) so its PRE pseudo is numbered first and wins the equal-priority
    // global-alloc tie for r20 (the original's TU numbered its labels differently)
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering)
    f32 lc0 = 3.5f;
    f32 lc1 = 10.5f;
    f32 lc2 = 17.5f;
    f32 lc3 = 24.5f;
    f32 lc4 = 31.5f;
    f32 lc5 = 38.5f;
    f32 lc6 = 45.5f;
    f32 lc7 = 52.5f;
    f32 lc8 = 59.5f;
    f32 lc9 = 66.5f;
    f32 lc10 = 73.5f;
    f32 lc11 = 80.5f;
    f32 lc12 = 87.5f;
    f32 lc13 = 94.5f;
    f32 lc14 = 101.5f;
    f32 lc15 = 108.5f;
    f32 lc16 = 115.5f;
    f32 lc17 = 122.5f;

    switch (w->step) {
    case 0:
        if (joy->trg & 0x200) {
            w->routine = 0;
            break;
        }
        if (joy->rep & 0x00080008) {
            w->cursor--;
        }
        if (joy->rep & 0x00040004) {
            w->cursor++;
        }
        if (joy->rep & 0x000C000C) {
            w->blink = 0x18;
        }
        w->cursor = w->cursor < 0 ? 0 : (w->cursor > 5 ? 5 : w->cursor);
        switch (w->cursor) {
        case 2:
            if (joy->trg & 0x100) {
                Hermite_1Reverse(w->curve);
            }
            break;
        case 5:
            if (joy->trg & 0x100) {
                w->yes = 0;
                w->step++;
            }
            break;
        default:
            if (joy->trg & 0x100) {
                w->step++;
            }
            break;
        }
        break;
    case 1:
        if (joy->trg & 0x200) {
            w->step = 0;
            break;
        }
        switch (w->cursor) {
        case 0:
            if (joy->rep & 0x00080008) {
                w->sub--;
            }
            if (joy->rep & 0x00040004) {
                w->sub++;
            }
            if (joy->rep & 0x000C000C) {
                w->blink = 0x18;
            }
            w->sub = w->sub < 0 ? 0 : (w->sub > 1 ? 1 : w->sub);
            switch (w->sub) {
            case 0:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->scaleX -= 0.1f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->scaleX += 0.1f;
                }
                w->scaleX = w->scaleX < 0.1f ? 0.1f : (w->scaleX > 10.0f ? 10.0f : w->scaleX);
                if (joy->trg & 0x100) {
                    Hermite_1Scale(w->curve, w->scaleX, w->scaleY);
                    w->scaleX = 1.0f;
                }
                break;
            case 1:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->scaleY -= 0.1f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->scaleY += 0.1f;
                }
                w->scaleY = w->scaleY < 0.1f ? 0.1f : (w->scaleY > 10.0f ? 10.0f : w->scaleY);
                if (joy->trg & 0x100) {
                    Hermite_1Scale(w->curve, w->scaleX, w->scaleY);
                    w->scaleY = 1.0f;
                }
                break;
            }
            break;
        case 1:
            if (joy->rep & 0x00080008) {
                w->sub--;
            }
            if (joy->rep & 0x00040004) {
                w->sub++;
            }
            if (joy->rep & 0x000C000C) {
                w->blink = 0x18;
            }
            w->sub = w->sub < 0 ? 0 : (w->sub > 2 ? 2 : w->sub);
            switch (w->sub) {
            case 0:
                if (joy->trg & 0x00010001) {
                    w->flags |= 1;
                }
                if (joy->trg & 0x00020002) {
                    w->flags &= ~1;
                }
                break;
            case 1:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->grid.x -= 1.0f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->grid.x += 1.0f;
                }
                w->grid.x = w->grid.x < 1.0f ? 1.0f : (w->grid.x > 60.0f ? 60.0f : w->grid.x);
                break;
            case 2:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->grid.y -= 0.01f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->grid.y += 0.01f;
                }
                w->grid.y = w->grid.y < 0.01f ? 0.01f : (w->grid.y > 10.0f ? 10.0f : w->grid.y);
                break;
            }
            break;
        case 3:
            if ((joy->rep & 1) || (joy->on & 0x10000)) {
                tx -= 1.0f;
            }
            if ((joy->rep & 2) || (joy->on & 0x20000)) {
                tx += 1.0f;
            }
            if ((joy->rep & 8) || (joy->on & 0x80000)) {
                ty += 0.1f;
            }
            if ((joy->rep & 4) || (joy->on & 0x40000)) {
                ty -= 0.1f;
            }
            Hermite_1Trans(w->curve, tx, ty);
            break;
        case 4:
            if (joy->rep & 0x00080008) {
                w->sub--;
            }
            if (joy->rep & 0x00040004) {
                w->sub++;
            }
            if (joy->rep & 0x000C000C) {
                w->blink = 0x18;
            }
            w->sub = w->sub < 0 ? 0 : (w->sub > 2 ? 2 : w->sub);
            switch (w->sub) {
            case 0:
                if (joy->trg & 0x00010001) {
                    w->flags |= 2;
                }
                if (joy->trg & 0x00020002) {
                    w->flags &= ~2;
                }
                break;
            case 1: {
                f32 a = w->yMax;
                f32 b = w->yMin;
                if (!(a - b < 1.0f)) {
                    f32 e;

                    // the original computes both fabs arms straight into f1 (the log10 argument): the
                    // SF->DF extend of the select gives its pseudo no f1 preference in ours (f0 + fmr f1)
                    register f32 m asm("fr1"); // COMPILER-DIFF: candidate #17 (FLOAT_EXTEND argument preference)
                    if (fabsf(a) > fabsf(b)) {
                        m = fabsf(a);
                    } else {
                        m = fabsf(b);
                    }
                    e = log10(m) - 2.0;
                    // the original issues the `lwz joy->rep` only after `frsp e`: a sched region
                    // split (LOOP_END anti-dependences) delays the load behind the FP chain
                    do { } while (0); // COMPILER-DIFF: #13 (region split)
                    if ((joy->rep & 1) || (joy->on & 0x10000)) {
                        w->yMax -= IPOW(10.0f, (int) e);
                        w->yMin += IPOW(10.0f, (int) e);
                    }
                    if ((joy->rep & 2) || (joy->on & 0x20000)) {
                        w->yMax += IPOW(10.0f, (int) e);
                        w->yMin -= IPOW(10.0f, (int) e);
                    }
                }
                break;
            }
            case 2: {
                f32 e = log10(w->xMax) - 2.0;
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->xMax -= IPOW(10.0f, (int) e);
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->xMax += IPOW(10.0f, (int) e);
                }
                if (w->xMax <= 1.0f) {
                    w->xMax = 1.0f;
                }
                w->xMin = w->xMax * -0.16666666f;
                break;
            }
            }
            break;
        case 5:
            if (joy->trg & 0x00010001) {
                w->yes = 1;
            }
            if (joy->trg & 0x00020002) {
                w->yes = 0;
            }
            if (joy->trg & 0x100) {
                if (w->yes) {
                    Hermite_1Clear(w->curve);
                }
                w->step = 0;
            }
            break;
        }
        break;
    }

    x = w->x;
    y = w->y;
    eprintf(x, y, 5, 0, "S-CURVE");
    y += 14;
    {
    int col = 0;
    for (i = 0; i <= 5; i++) {
        eprintf(x, y + i * 14, (i == w->cursor) ? 4 : 0, 0, "%s", sctrl_menu_name[i]);
        if (i == w->cursor) {
            if (w->step == 0) {
                if (w->blink & 0x18) {
                    eprintf(x - 8, y + i * 14, 0x16, 0, ">");
                }
            } else {
                eprintf(x - 8, y + i * 14, 0x16, 0, ">");
            }
        }
        col = 0;
        if (i == w->cursor && w->step == 1) {
            switch (i) {
            case 0:
                eprintf(x + 0x48, y + i * 14, 0, 0, "H: x%f", w->scaleX);
                eprintf(x + 0x48, y + (i + 1) * 14, 0, 0, "V: x%f", w->scaleY);
                if (w->blink & 0x18) {
                    eprintf(x + 0x40, y + (i + w->sub) * 14, 0, 0, ">");
                }
                break;
            case 1:
                eprintf(x + 0x48, y + i * 14, 0, 0, "LOCK:", w->grid.y);
                if (w->flags & 1) {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "ON-/---");
                } else {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "---/OFF");
                }
                eprintf(x + 0x48, y + (i + 1) * 14, (u8) col, 0, "X   : %f", w->grid.x);
                eprintf(x + 0x48, y + (i + 2) * 14, (u8) col, 0, "Y   : %f", w->grid.y);
                if (w->blink & 0x18) {
                    eprintf(x + 0x40, y + (i + w->sub) * 14, (u8) col, 0, ">");
                }
                break;
            case 3:
                eprintf(x + 0x48, y + i * 14, 0, 0, "X: %f", tx);
                eprintf(x + 0x48, y + (i + 1) * 14, 0, 0, "Y: %f", ty);
                break;
            case 4:
                eprintf(x + 0x48, y + i * 14, 0, 0, "AUTO:", w->xMax);
                eprintf(x + 0x48, y + (i + 1) * 14, 0, 0, "H   :", w->yMax);
                eprintf(x + 0x48, y + (i + 2) * 14, 0, 0, "V   :", w->xMax);
                if (w->flags & 2) {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "ON-/---");
                } else {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "---/OFF");
                }
                if (w->sub != 0) {
                    if (joy->on & 0x00010001) {
                        eprintf(x + 0x70, y + (i + w->sub) * 14, 0, 0, "-");
                    }
                    if (joy->on & 0x00020002) {
                        eprintf(x + 0x70, y + (i + w->sub) * 14, 0, 0, "+");
                    }
                }
                if (w->blink & 0x18) {
                    // the original issued only `li r5` in this block's second cycle: a free
                    // weight-0 insn took the other slot (lbz, addi r3 | li r5, X | extsb, li r6 |
                    // add, addi r7); the codeless non-volatile asm is that insn in both passes
                    asm("" : "=m"(w->blink)); // COMPILER-DIFF: #13 (free sched slot filler)
                    eprintf(x + 0x40, y + (i + w->sub) * 14, 0, 0, ">");
                }
                break;
            case 5:
                if (w->yes) {
                    eprintf(x + 0x48, y + i * 14, 0, 0, "YES/---");
                } else {
                    eprintf(x + 0x48, y + i * 14, 0, 0, "---/NO-");
                }
                break;
            }
        }
    }
    // keeps `col` live to the loop exit: its 4-ref/98-insn allocno otherwise outranks the ">"
    // string high, whose REG_EQUIV (high) doubled live length halves its priority in ours but
    // not in the original (">" r22, col r21)
    asm("" : : "r"(col)); // COMPILER-DIFF: #13 (REG_EQUIV live-length doubling of a hoisted high)
    }
    return 1;
}

// Routine 0, curve editing (pad 1): d-pad / stick move the screen cursor (x5 with the sub stick).
// step 0 idle: A grabs a key (step 2 drag the point, 3 / 4 drag its in / out tangent handle), Y
// on the curve grabs a key or an insertion spot (step 5: YES/NO to delete / insert), Z opens the
// menu, B quits; step 1 appends keys (an empty curve starts here): A places one at the cursor
// (grid-locked, range re-fitted), B ends. Returns 1.
static int sctrlEdit(DbSctrlWork* w)
{
    Vec* cur = &w->pos;
    JOY* joy = &Joy[0];
    Vec g;
    Vec g2;

    if (w->step != 5) {
        f32 spd = (joy->on & 0x000F0000) ? 5.0f : 1.0f;

        if ((joy->rep & 1) || (joy->on & 0x10000)) {
            cur->x -= spd;
        }
        if ((joy->rep & 2) || (joy->on & 0x20000)) {
            cur->x += spd;
        }
        if ((joy->rep & 8) || (joy->on & 0x80000)) {
            cur->y += spd;
        }
        if ((joy->rep & 4) || (joy->on & 0x40000)) {
            cur->y -= spd;
        }
    }
    switch (w->step) {
    case 0:
        if (w->curve->num == 0) {
            w->curve->num = 1;
            w->step = 1;
            break;
        }
        if (joy->trg & 0x10) {
            w->step = 0;
            w->scaleX = 1.0f;
            w->routine = 1;
            w->scaleY = 1.0f;
            break;
        }
        if (joy->trg & 0x200) {
            w->routine = 2;
            break;
        }
        if (joy->trg & 0x100) {
            s8 hit = grabPoint(w);
            switch (hit) {
            case 0:
                w->step = 2;
                g2.x = w->curve->key[w->grab].t;
                g2.y = w->curve->key[w->grab].v;
                posGraph2Screen(w, &g2, cur);
                break;
            case 1:
                w->step = 3;
                break;
            case 2:
                w->step = 4;
                break;
            }
        } else if (joy->trg & 0x800) {
            if (grabLine(w)) {
                if (w->grab != -1) {
                    g.x = w->curve->key[w->grab].t;
                    g.y = w->curve->key[w->grab].v;
                    g.z = 0.0f;
                    posGraph2Screen(w, &g, &w->pos);
                } else if (w->insertIdx != -1) {
                    posGraph2Screen(w, &w->insertPos, &w->pos);
                }
                w->yes = 0;
                w->step = 5;
            }
        }
        break;
    case 1:
        if (joy->trg & 0x10) {
            w->routine = 1;
            w->step = 0;
            break;
        }
        posScreen2Graph(w, cur, &g);
        w->curve->key[w->curve->num - 1].t = g.x;
        w->curve->key[w->curve->num - 1].v = g.y;
        if ((joy->trg & 0x100) && w->curve->num <= SCTRL_MAX_KEY - 1) {
            posScreen2GridLock(w, cur, &g);
            w->curve->key[w->curve->num - 1].t = g.x;
            w->curve->key[w->curve->num - 1].v = g.y;
            if (w->curve->num > 1) {
                SctrlAdjustAxisRange(w);
                posGraph2Screen(w, &g, cur);
            }
            w->curve->num++;
        }
        if (joy->trg & 0x200) {
            if (w->curve->num > 1) {
                w->step = 0;
            } else {
                w->routine = 2;
            }
            w->curve->num--;
        }
        break;
    case 2:
        if (joy->on & 0x100) {
            posScreen2Graph(w, cur, &g2);
            w->curve->key[w->grab].t = g2.x;
            w->curve->key[w->grab].v = g2.y;
        } else {
            posScreen2GridLock(w, cur, &g2);
            w->curve->key[w->grab].t = g2.x;
            w->curve->key[w->grab].v = g2.y;
            w->step = 0;
            if (w->curve->num > 1) {
                SctrlAdjustAxisRange(w);
                posGraph2Screen(w, &g2, cur);
            }
        }
        break;
    case 3:
    case 4:
        if (joy->on & 0x100) {
            Hermite1* c;
            f32 dx;
            f32 dy;

            posScreen2Graph(w, cur, &g2);
            c = w->curve;
            dx = c->key[w->grab].t - g2.x;
            dy = c->key[w->grab].v - g2.y;
            if (w->step == 3) {
                c->key[w->grab].in = dy / dx;
            } else {
                c->key[w->grab].out = dy / dx;
            }
        } else {
            w->step = 0;
        }
        break;
    case 5:
        if (joy->on & 0x800) {
            if (joy->rep & 0x00010001) {
                w->yes = 1;
            }
            if (joy->rep & 0x00020002) {
                w->yes = 0;
            }
            if (joy->trg & 0x100) {
                if (w->yes) {
                    if (w->grab != -1) {
                        deletePoint(w);
                    } else if (w->insertIdx != -1) {
                        insertPoint(w);
                    }
                }
                w->step = 0;
            }
            if (w->grab != -1) {
                eprintf(0xF0, 0x118, 2, 0, "DELETE");
            } else {
                eprintf(0xF0, 0x118, 5, 0, "INSERT");
            }
            if (w->yes) {
                eprintf(0x100, 0x126, 0, 0, "YES/---");
            } else {
                eprintf(0x100, 0x126, 0, 0, "---/NO-");
            }
        } else {
            w->step = 0;
        }
        break;
    }
    return 1;
}

// Nearest key (0), in-tangent handle (1) or out-tangent handle (2) within SCTRL_GRAB_DIST screen units
// of the cursor -> w->grab; -1 when none.
int grabPoint(DbSctrlWork* w)
{
    Vec scr;
    Vec hnd;
    Vec gph;
    Vec dir;
    Hermite1* c = w->curve;
    f32 min = SCTRL_GRAB_DIST;
    int ret = -1;
    int i;

    for (i = 0; i < c->num; i++) {
        HermiteKey* k = &c->key[i];
        f32 ang;
        f32 d;

        gph.x = k->t;
        gph.y = k->v;
        posGraph2Screen(w, &gph, &scr);
        d = PSVECDistance(&scr, &w->pos);
        if (d < min) {
            w->grab = i;
            min = d;
            ret = 0;
        }

        ang = atanf(k->in) + PI;
        gph.x = cosf(ang) * SCTRL_HANDLE_LEN + k->t;
        gph.y = sinf(ang) * SCTRL_HANDLE_LEN + k->v;
        posGraph2Screen(w, &gph, &hnd);
        PSVECSubtract(&hnd, &scr, &dir);
#line 790 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&scr, &dir, &hnd);
        d = PSVECDistance(&hnd, &w->pos);
        if (d < min) {
            w->grab = i;
            min = d;
            ret = 1;
        }

        ang = atanf(k->out);
        gph.x = cosf(ang) * SCTRL_HANDLE_LEN + k->t;
        gph.y = sinf(ang) * SCTRL_HANDLE_LEN + k->v;
        posGraph2Screen(w, &gph, &hnd);
        PSVECSubtract(&hnd, &scr, &dir);
#line 811 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&scr, &dir, &hnd);
        d = PSVECDistance(&hnd, &w->pos);
        if (d < min) {
            w->grab = i;
            min = d;
            ret = 2;
        }
    }
    return ret;
}

// Nearest curve sample (1 -> insertPos / insertIdx) or key (2 -> grab) within SCTRL_GRAB_DIST of the
// cursor; 0 when neither.
int grabLine(DbSctrlWork* w)
{
    Vec gph;
    Vec scr;
    f32 v;
    Hermite1* c = w->curve;
    f32 min = SCTRL_GRAB_DIST;
    int ret = 0;
    int i;

    w->insertIdx = -1;
    for (i = 0; i < c->num - 1; i++) {
        HermiteKey* a = &c->key[i];
        HermiteKey* b = &c->key[i + 1];
        f32 t0 = a->t;
        f32 span = b->t - a->t;
        int j;

        for (j = 0; j <= 100.0f; j++) {
            f32 t = t0 + (f32) j * span / 100.0f;
            f32 d;

            Hermite_1(a, b, t, &v);
            gph.x = t;
            gph.y = v;
            gph.z = 0.0f;
            posGraph2Screen(w, &gph, &scr);
            d = PSVECDistance(&scr, &w->pos);
            if (d <= min) {
                min = d;
                ret = 1;
                w->insertIdx = i + 1;
                w->insertPos = gph;
            }
        }
    }
    w->grab = -1;
    for (i = 0; i < c->num; i++) {
        f32 d;

        gph.x = c->key[i].t;
        gph.y = c->key[i].v;
        gph.z = 0.0f;
        posGraph2Screen(w, &gph, &scr);
        d = PSVECDistance(&scr, &w->pos);
        if (d <= min) {
            w->grab = i;
            min = d;
            ret = 2;
        }
    }
    return ret;
}

// Removes key w->grab from the curve (the rest shift down, the freed slot is zeroed).
void deletePoint(DbSctrlWork* w)
{
    Hermite1* c = w->curve;
    int i;

    c->num--;
    for (i = w->grab; i < c->num; i++) {
        c->key[i] = c->key[i + 1];
    }
    memclr_asm(&c->key[c->num], sizeof(HermiteKey));
}

// Inserts a key at w->insertIdx with the grabbed curve position (insertPos); no-op at 64 keys.
void insertPoint(DbSctrlWork* w)
{
    Hermite1* c = w->curve;
    int i;

    if (c->num > SCTRL_MAX_KEY - 1) {
        return;
    }
    for (i = c->num; i > w->insertIdx; i--) {
        c->key[i] = c->key[i - 1];
    }
    memclr_asm(&c->key[w->insertIdx], sizeof(HermiteKey));
    c->key[w->insertIdx].t = w->insertPos.x;
    c->key[w->insertIdx].v = w->insertPos.y;
    c->num++;
}

// Draws the cross-hair cursor at the screen position (world space through w->mtx) and prints its
// graph coordinates.
void drawCursor(DbSctrlWork* w)
{
    Vec a;
    Vec b;
    Vec wa;
    Vec wb;
    Vec gph;
    int sx;
    int sy;
    int dx = 8;
    int dy;

    a = w->pos;
    b = w->pos;
    a.y += 50.0f;
    b.y -= 50.0f;
    posScreen2World(w, &a, &wa);
    posScreen2World(w, &b, &wb);
    Draw_line3d(&wa, &wb, SCTRL_LINE_COL, 0);
    a = w->pos;
    b = w->pos;
    a.x += 50.0f;
    b.x -= 50.0f;
    posScreen2World(w, &a, &wa);
    posScreen2World(w, &b, &wb);
    Draw_line3d(&wa, &wb, SCTRL_LINE_COL, 0);
    sx = (int) (w->pos.x * 256.0f / 320.0f + 256.0f);
    sy = (int) (224.0f - w->pos.y * 224.0f / 240.0f);
    if (sx > 0x198) {
        dx = -0x68;
    }
    if (sy > 0x1A4) {
        dy = -0x1C;
    } else {
        dy = 14;
    }
    posScreen2Graph(w, &w->pos, &gph);
    eprintf(sx + dx, sy + dy, 0, 0, "(%3.3f, %3.3f)", gph.x, gph.y);
}

// Draws the graph frame, the grid lines at gridX/gridY spacing and the axis labels with the range
// values.
void drawAxis(DbSctrlWork* w)
{
    Vec g0;
    Vec g1;
    Vec w0;
    Vec w1;
    f32 zero = 0.0f;
    int i;
    // function-scope sx/sy (set in both label blocks): a block-local sx is a local-alloc qty and the
    // fix_trunc load `(set sx (unspec [fpmem P]))` ties the dying stack-slot address pseudo P to it
    // (r3, the argument register); a multi-block sx has no qty, P is allocated alone (r9, r11)
    int sx;
    int sy;

    g0.x = w->xMin;
    g1.x = w->xMax;
    g0.y = zero;
    g1.y = zero;
    posGraph2World(w, &g0, &w0);
    posGraph2World(w, &g1, &w1);
    Draw_line3d(&w0, &w1, SCTRL_LINE_COL, 0);
    if (strlen(w->labelX) != 0) {
        sx = (int) (w1.x * 256.0f / 320.0f + 256.0f);
        sy = (int) (224.0f - w1.y);
        eprintf(sx - 0x48, sy, 0, 0, "%s", w->labelX);
    }
    g0.y = w->yMin;
    g1.y = w->yMax;
    g0.x = zero;
    g1.x = zero;
    posGraph2World(w, &g0, &w0);
    posGraph2World(w, &g1, &w1);
    Draw_line3d(&w0, &w1, SCTRL_LINE_COL, 0);
    if (strlen(w->labelY) != 0) {
        sx = (int) (w1.x * 256.0f / 320.0f + 256.0f);
        sy = (int) (224.0f - w1.y);
        eprintf(sx - 0x40, sy + 0x2A, 0, 0, "%s", w->labelY);
    }
    if (w->gridX > zero) {
        memclr_asm(&g0, sizeof(Vec));
        memclr_asm(&g1, sizeof(Vec));
        g0.y = w->yMin;
        g1.y = w->yMax;
        for (i = 1; (f32) i * w->gridX < w->xMax; i++) {
            g1.x = g0.x = (f32) i * w->gridX;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
        for (i = -1; (f32) i * w->gridX > w->xMin; i--) {
            g1.x = g0.x = (f32) i * w->gridX;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
    }
    if (w->gridY > 0.0f) {
        memclr_asm(&g0, sizeof(Vec));
        memclr_asm(&g1, sizeof(Vec));
        g0.x = w->xMin;
        g1.x = w->xMax;
        for (i = 1; (f32) i * w->gridY < w->yMax; i++) {
            g1.y = g0.y = (f32) i * w->gridY;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
        for (i = -1; (f32) i * w->gridY > w->yMin; i--) {
            g1.y = g0.y = (f32) i * w->gridY;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
    }
}

// Draws the Hermite curve sampled between the keys, every key point (the grabbed one highlighted)
// and the in/out tangent handles of the grabbed key.
void drawScurve(DbSctrlWork* w)
{
    Vec gph;
    Vec wp;
    Vec prev;
    Vec hnd;
    Vec wh;
    Vec dir;
    f32 v;
    Hermite1* c = w->curve;
    HermiteKey* k;
    int i;

    // k as `&c->key[i]` inside the body: loop.c reduces it to a pointer giv whose `addi rK,c,4` init is
    // emitted in the loop preheader (a `k = c->key; ... k++` form puts the init in the entry block
    // before the exit test, which shifts the callee-saved allocation of i/k/&wp)
    for (i = 0; i < c->num; i++) {
        f32 ang;

        k = &c->key[i];
        gph.x = k->t;
        gph.y = k->v;
        posGraph2World(w, &gph, &wp);
        Draw_sphere(&wp, 2.0f, 0xFFFFFFFF, 0, 0);

        ang = atanf(k->in) + PI;
        hnd.x = cosf(ang) * SCTRL_HANDLE_LEN + k->t;
        hnd.y = sinf(ang) * SCTRL_HANDLE_LEN + k->v;
        posGraph2World(w, &hnd, &wh);
        PSVECSubtract(&wh, &wp, &dir);
#line 1116 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&wp, &dir, &wh);
        Draw_sphere(&wh, 2.0f, 0xFFFFFFFF, 0, 0);
        Draw_line3d(&wp, &wh, SCTRL_LINE_COL, 0);

        ang = atanf(k->out);
        hnd.x = cosf(ang) * SCTRL_HANDLE_LEN + k->t;
        hnd.y = sinf(ang) * SCTRL_HANDLE_LEN + k->v;
        posGraph2World(w, &hnd, &wh);
        PSVECSubtract(&wh, &wp, &dir);
#line 1133 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&wp, &dir, &wh);
        Draw_sphere(&wh, 2.0f, 0xFFFFFFFF, 0, 0);
        Draw_line3d(&wp, &wh, SCTRL_LINE_COL, 0);
    }
    for (i = 0; i < c->num - 1; i++) {
        HermiteKey* a = &c->key[i];
        HermiteKey* b = &c->key[i + 1];
        f32 t0 = a->t;
        f32 span = b->t - a->t;
        int j;

        for (j = 0; j <= 99; j++) {
            f32 t = t0 + (f32) j * span / 99.0f;

            Hermite_1(a, b, t, &v);
            gph.x = t;
            gph.y = v;
            gph.z = 0.0f;
            posGraph2World(w, &gph, &wp);
            if (j != 0) {
                Draw_line3d(&prev, &wp, SCTRL_LINE_COL, 0);
            }
            prev = wp;
        }
    }
}

// Screen (640 x 480 centred) -> graph coordinates by the current range.
void posScreen2Graph(DbSctrlWork* w, Vec* scr, Vec* gph)
{
    f32 dx = w->xMax - w->xMin;
    f32 dy = w->yMax - w->yMin;

    gph->x = scr->x * (dx / SCTRL_SCR_W) + dx * 0.5f + w->xMin;
    gph->y = scr->y * (dy / SCTRL_SCR_H) + dy * 0.5f + w->yMin;
    gph->z = 0.0f;
}

// Graph -> screen coordinates.
void posGraph2Screen(DbSctrlWork* w, Vec* gph, Vec* scr)
{
    f32 dx = w->xMax - w->xMin;
    f32 dy = w->yMax - w->yMin;

    scr->x = (gph->x - w->xMin - dx * 0.5f) * (SCTRL_SCR_W / dx);
    scr->y = (gph->y - w->yMin - dy * 0.5f) * (SCTRL_SCR_H / dy);
    scr->z = 0.0f;
}

// Screen -> world through the graph plane matrix (for the GX primitives).
void posScreen2World(DbSctrlWork* w, Vec* scr, Vec* out)
{
    PSMTXMultVec(w->mtx, scr, out);
}

// Graph -> world (posGraph2Screen then posScreen2World).
void posGraph2World(DbSctrlWork* w, Vec* gph, Vec* out)
{
    Vec scr;

    posGraph2Screen(w, gph, &scr);
    posScreen2World(w, &scr, out);
}

// Snaps in->x/y to the grid (a zero grid step counts as 1 / 0.01).
void posGridLock(Vec* grid, Vec* in, Vec* out)
{
    if (grid->x == 0.0f) {
        grid->x = 1.0f;
    }
    if (grid->y == 0.0f) {
        grid->y = 0.01f;
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

// Screen -> graph, snapped to the grid-lock step when flags bit 0 is set.
void posScreen2GridLock(DbSctrlWork* w, Vec* scr, Vec* gph)
{
    posScreen2Graph(w, scr, gph);
    if (w->flags & 1) {
        posGridLock(&w->grid, gph, gph);
        posGraph2Screen(w, gph, scr);
    }
}
