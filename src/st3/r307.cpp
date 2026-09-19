#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emwindow.h"
#include "emBarred.h"
#include "etc_model.h"
#include "player.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "mes.h"
#include "cam_ctrl.h"

// Room 3-07 (D:/Bio4/Prog/r307.cpp): the nine-piece rotation puzzle on the terminal (each terminal choice
// turns the pieces of one colour; two solution patterns), the barred door it opens, the regenerator that
// appears once the item is taken, and its camera cut.

// One puzzle piece: the terminal choice that turns it and its effect position.
struct R307Piece {
    int no;
    Vec pos;
};

// One piece of a solution pattern: the piece and the rotation it must show.
struct R307Sol {
    u8 piece;
    int rot;
};

struct R307Pattern {
    const R307Sol* sol;   // 0x00
    u8 num;               // 0x04
    u8 pad[6];
    u8 eff;               // 0x0B  effect of the solved pattern
};

// The pattern table seen through a pointer (r307_checkPiece's `eff` read, see the comment there).
struct R307PatTbl {
    R307Pattern p[2];
};

struct R307RotTbl {
    int rot[9];
};

struct R307Work {
    u32 str;          // 0x00  SndStrReq handle of the camera cut
    cEm* barred;      // 0x04
    u32 eff[9];       // 0x08  EspPullCoreKind per piece (the piece)
    u32 eff2[9];      // 0x2C  EspPullCoreKind per piece (the frame)
    u32 effBarred;    // 0x50
    u32 effTerm;      // 0x54
    R307RotTbl rot;   // 0x58  current rotation per piece
    u8 done[9];       // 0x7C  1 = the piece is part of a matched pattern
    u8 x85[9];        // 0x85
};

struct R307WorkPtr {
    R307Work* p;
};

static R307WorkPtr r307_work;

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);

static R307Piece r307_piece[9] = {
    {2, {-195.5f, 1179.6f, -2038.1f}},
    {1, {-195.5f, 1179.6f, -1933.1f}},
    {0, {-195.5f, 1179.6f, -1829.1f}},
    {3, {-273.5f, 1112.6f, -2038.1f}},
    {2, {-273.5f, 1112.6f, -1933.1f}},
    {1, {-273.5f, 1112.6f, -1829.1f}},
    {1, {-348.6f, 1043.6f, -2038.1f}},
    {3, {-348.6f, 1043.6f, -1933.1f}},
    {0, {-348.6f, 1043.6f, -1829.1f}},
};

static const R307RotTbl r307_initRot = {{1, 2, 0, 2, 2, 0, 1, 0, 3}};

static const R307Sol r307_sol0[5] = {{0, 2}, {3, 3}, {4, 3}, {5, 2}, {8, 2}};
static const R307Sol r307_sol1[7] = {{0, 2}, {3, 2}, {6, 3}, {7, 0}, {4, 3}, {5, 2}, {8, 2}};

static const R307Pattern r307_pattern[2] = {
    {r307_sol0, 5, {0, 0, 0, 0, 0, 0}, 0x14},
    {r307_sol1, 7, {0, 0, 0, 0, 0, 0}, 0x15},
};

// Effect of a matched piece by [choice][rotation], and of a plain piece.
static const u8 r307_effDone[4][4] = {
    {0x0C, 0x0F, 0x0E, 0x0D},
    {0x08, 0x0B, 0x0A, 0x09},
    {0x04, 0x07, 0x06, 0x05},
    {0x10, 0x13, 0x12, 0x11},
};
static const u8 r307_effPiece[4][4] = {
    {0x22, 0x25, 0x24, 0x23},
    {0x1E, 0x21, 0x20, 0x1F},
    {0x1A, 0x1D, 0x1C, 0x1B},
    {0x26, 0x29, 0x28, 0x27},
};

// Death bit of entry `no` of the loaded enemy list (0 while no list is loaded).
static inline u32 r307_emDead(int no)
{
    int list = pG->em_list_no;
    u32 v;

    if (list >= 0) {
        v = *(u32*) ((list << 5) + (u32) pG + 0x501C + (((u32) no >> 5) << 2)) & (0x80000000 >> (no & 31));
    } else {
        v = 0;
    }
    return v;
}

// The three effect deletes of one EspPullCoreKind slot (the work is re-read for each call).
#define R307_EFF_DELETE(f)                    \
    EffectEspDelete(0, (u8) (f), 0, 0);       \
    EffectEspgenDelete(0, (u8) (f), 0);       \
    EffectEfmDelete(0, (u8) (f), 0)

int r307_checkPiece();
void r307_turnPiece(int no);
void r307_delPiece();
void r307_initPiece();
static void r307_checkPuzzleTerminal();
void r307_initPuzzle();
static void r307_getItem();
static void r307_checkBgm();
static void r307_execEmCut_end();
static void r307_execEmCut();
void r307_setEmAppear();
static void r307_appearEm();

// Room init: windows 0xA/0xB without fences; area 3 = the regenerator camera cut until Room_flg bit 1;
// area 4 = the item on the terminal until bit 0 (afterwards the item's table objects hidden, area 2 off,
// the stream if the regenerator 0x32 still lives, areas 7/8 off); the rotation puzzle.
void R307Init()
{
    cEm* win;

    R307Work*& wp = r307_work.p;
#line 43 "D:/Bio4/Prog/r307.cpp"
    wp = (R307Work*) MEM_CALLOC(sizeof(R307Work), 1, 0xd);
    if (getRoomEtcWindow(0xA, &win, 1)) {
        ((cEmWindow*) win)->SetEnableFence(0, 0);
    }
    if (getRoomEtcWindow(0xB, &win, 1)) {
        ((cEmWindow*) win)->SetEnableFence(0, 0);
    }
    void* zero = 0;
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) r307_execEmCut, 0, 1);
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) r307_getItem, 0, 1);
    } else {
        cObj* obj;

        SceAtSetEnable(2, 0);
        obj = SmdGetObjPtr(6);
        if (obj) {
            obj->be_flag &= ~2;
        }
        EstSet(0, -1, 0, 0, 1, 0, 0, 0, (u32) zero, zero);
        if (r307_emDead(0x32) == 0) {
            SceExec(0x12, (TaskFunc) r307_checkBgm, 0, 0, 2, 0);
        }
        SceAtSetEnable(7, 0);
        SceAtSetEnable(8, 0);
    }
    r307_initPuzzle();
}

// Per-frame room main: nothing.
void R307Main()
{
}

// Tests the two solution patterns; the matched pieces light up. 1 when a pattern is complete.
// The inner loop reads `r307_pattern[i].num` in its condition and exit test (one hoisted load, the
// `i*12` giv kept beside the base) and `zero` is declared inside the second loop body (init order).
// The tail's `eff` read is `add r9, r9, r10` = (plus sym pat*12), the sum tied to the dying sym
// register: cse swaps a (plus A B) to put a constant-equivalent A second, and it knows the fresh
// `r307_pattern` address (lo_sum of a high it folds) in any ebb that contains it, so the read goes
// through `tbl` (an array member through a pointer keeps the base first at expand) and the
// LOOP_END-blinded dead test `k`: cse1 ends its ebb at the `do {} while (0)` LOOP_END, so it cannot
// fold `k != 2` and follows the branch AROUND `tbl = 0`, invalidating `tbl`; cse2 folds the test
// into an unconditional jump and skips the rest of the tail, and jump/flow delete the test, the
// label and `k` before sched1 (the loop notes sit between two calls: the barrier they impose on
// `li r3, 15` changes nothing). The `ok != 1` arm first blocks jump1's range swap.
int r307_checkPiece()
{
    u32 i;
    u32 j;
    int ok = 0;
    u32 pat = 0;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < r307_pattern[i].num; j++) {
            const R307Sol* s = &r307_pattern[i].sol[j];
            u8 piece = s->piece;

            if (s->rot != r307_work.p->rot.rot[piece]) {
                break;
            }
            r307_work.p->done[piece] = 1;
        }
        if (j == r307_pattern[i].num) {
            ok = 1;
            pat = i;
        }
    }
    for (i = 0; i < 9; i++) {
        if (r307_work.p->done[i] == 1) {
            void* zero = 0;
            R307_EFF_DELETE(r307_work.p->eff[i]);
            R307_EFF_DELETE(r307_work.p->eff2[i]);
            EstSet(0, -1, &r307_piece[i].pos, 0, 1, r307_effDone[r307_piece[i].no][r307_work.p->rot.rot[i]], 1, (u8) r307_work.p->eff[i], (u32) zero, zero);
            EstSet(0, -1, &r307_piece[i].pos, 0, 1, 0x18, 1, (u8) r307_work.p->eff2[i], (u32) zero, zero);
        }
    }
    if (ok != 1) {
        SndCall(6, 4, 0, 0, 0, 0);
        return 0;
    }
    SndCall(6, 5, 0, 0, 0, 0);
    int k = 2;
    do {
    } while (0);
    SceSleep(15);
    const R307PatTbl* tbl = (const R307PatTbl*) r307_pattern;
    if (k != 2) {
        tbl = 0;
    }
    EstSet(0, -1, 0, 0, 1, tbl->p[pat].eff, 1, (u8) r307_work.p->effTerm, 0, 0);
    return 1;
}

// Terminal choice `no`: the pieces of that colour turn a quarter; their effects are redrawn.
void r307_turnPiece(int no)
{
    u32 k;

    for (k = 0; k < 9; k++) {
        if (r307_piece[k].no == no || r307_work.p->done[k] == 1) {
            R307_EFF_DELETE(r307_work.p->eff[k]);
            R307_EFF_DELETE(r307_work.p->eff2[k]);
        }
    }
    for (k = 0; k < 9; k++) {
        if (r307_piece[k].no == no) {
            int next = r307_work.p->rot.rot[k];

            next++;
            next = (next < 0) ? 3 : ((next > 3) ? 0 : next);
            r307_work.p->rot.rot[k] = next;
            EstSet(0, -1, &r307_piece[k].pos, 0, 1, r307_effPiece[no][next], 1, (u8) r307_work.p->eff[k], 0, 0);
        } else if (r307_work.p->done[k] == 1) {
            EstSet(0, -1, &r307_piece[k].pos, 0, 1, r307_effPiece[r307_piece[k].no][r307_work.p->rot.rot[k]], 1, (u8) r307_work.p->eff[k], 0, 0);
        }
        r307_work.p->done[k] = 0;
    }
}

// Drop every piece / frame effect and the terminal effect (before the effects are redrawn or the puzzle ends).
void r307_delPiece()
{
    u32 k;

    for (k = 0; k < 9; k++) {
        R307_EFF_DELETE(r307_work.p->eff[k]);
        R307_EFF_DELETE(r307_work.p->eff2[k]);
    }
    R307_EFF_DELETE(r307_work.p->effTerm);
}

// The pieces at their start rotation, with their effects.
void r307_initPiece()
{
    u32 k;

    // byte-pointer destination: the work pointer is reloaded after the copy (joy.h JOY_COPY)
    memcpy((u8*) r307_work.p + 0x58, &r307_initRot, sizeof(R307RotTbl));
    memclr_asm(r307_work.p->done, 9);
    memclr_asm(r307_work.p->x85, 9);
    for (k = 0; k < 9; k++) {
        void* zero = 0;
        int no = r307_piece[k].no;
        u8 eff = r307_effPiece[no][r307_work.p->rot.rot[k]];

        EstSet(0, -1, &r307_piece[k].pos, 0, 1, eff, 1, (u8) r307_work.p->eff[k], (u32) zero, zero);
    }
    EstSet(0, -1, 0, 0, 1, 0x17, 1, (u8) r307_work.p->effTerm, 0, 0);
}

// Area 5: the terminal. Choices 1..4 turn a colour, 5 leaves; a solved pattern unlocks the barred door.
static void r307_checkPuzzleTerminal()
{
    int quit;
    int solved;
    int cur;
    int sel;

    MesWork* w = cMes.getWork();

    SceMesSet(0, 0, 1, 0x64, 0x150 - w->lineSpace - w->m_font_h - 1);
    switch (SceMesGetSelection()) {
    case -1:
    case 0:
    case 2:
        break;
    case 1:
    default:
        r307_initPiece();
        quit = 0;
        SndCall(6, 3, 0, 0, 0, 0);
        solved = 0;
        SceEventStart(0);
        CamCtrl.CutCall(7);
        cur = 1;
        do {
            SceMesSet(1, 0x230, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1);
            {
                // The s8 copy keeps the `extsb` before the `-1` (an `(s8) cur - 1` expression loses it
                // in the byte store).
                s8 c = cur;

                cMes.getWork()->m_cur = c - 1;
            }
            SceMesWait();
            sel = SceMesGetSelection();
            switch (sel) {
            case 1:
            case 2:
            case 3:
            case 4:
                cur = sel;
                r307_turnPiece(cur - 1);
                if (r307_checkPiece() == 1) {
                    quit = 1;
                    solved = 1;
                }
                break;
            case 5:
            case -1:
                quit = 1;
                break;
            }
            SceSleep(1);
        } while (quit == 0);
        if (solved == 1) {
            void* zero = 0;

            RsfSet(G_ROOM_ID, 2);
            pG->Key_flg[0] |= 2;
            SceAtSetEnable(5, 0);
            SceAtSetEnable(9, 0);
            SceSleep(10);
            SndCall(6, 6, 0, 0, 0, 0);
            SceSleep(50);
            r307_delPiece();
            CamCtrl.CutCall(8);
            pPL->setNoSuspend(0);
            if (r307_work.p->barred) {
                SceSleep(10);
                R307_EFF_DELETE(r307_work.p->effBarred);
                EstSet((int) r307_work.p->barred, -1, 0, 0, 1, 2, 1, (u8) r307_work.p->effBarred, (u32) zero, zero);
                SndCall(6, 7, 0, 0, 0, 0);
                SceSleep(30);
                ((cEmBarred*) r307_work.p->barred)->setLockMode(0);
                SceSleep(30);
            }
            CamCtrl.Comeback(0);
        } else {
            CamCtrl.Comeback(0);
            r307_delPiece();
        }
        SceEventEnd(0);
        break;
    }
}

// The puzzle: the barred door 0x32 lock-locked with its effect and area 5 = the terminal until solved
// (Room_flg bit 2), the per-piece effect kinds pulled; solved -> area 9 off and the open-door effect.
void r307_initPuzzle()
{
    void* zero = 0;
    u32 k;

    U32Set(r307_work.p->effBarred, EspPullCoreKind());
    getRoomEtcBarred(0x32, &r307_work.p->barred, 1);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(5, 0x12, 0, (TaskFunc) r307_checkPuzzleTerminal, 0, 1);
        if (r307_work.p->barred) {
            ((cEmBarred*) r307_work.p->barred)->setLockMode(1);
            EstSet((int) r307_work.p->barred, -1, 0, 0, 1, 1, 1, (u8) r307_work.p->effBarred, (u32) zero, zero);
        }
        for (k = 0; k < 9; k++) {
            r307_work.p->eff[k] = EspPullCoreKind();
            r307_work.p->eff2[k] = EspPullCoreKind();
        }
        r307_work.p->effTerm = EspPullCoreKind();
    } else {
        SceAtSetEnable(9, 0);
        if (r307_work.p->barred) {
            EstSet((int) r307_work.p->barred, -1, 0, 0, 1, 3, 1, (u8) r307_work.p->effBarred, (u32) zero, zero);
        }
    }
}

// Area 4: the item on the terminal; taking it brings the regenerator.
static void r307_getItem()
{
    cModel* m;

    SceAtSetEnable(4, 0);
    SceEventStart(0);
    LightMgr.onKind(0x7F);
    m = SceAtItemModelPtr(0x80);
    if (m) {
        m->setNoSuspend(1);
    }
    CamCtrl.CutCall(6);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceAtExecute(0x80);
    while (SceAtItemFlgCk(0x80) == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    r307_setEmAppear();
}

// Battle stream 3 while the regenerator (0x36) lives.
static void r307_checkBgm()
{
    SndRoomStrStart(1, 3, 1);
    SceSleep(30);
    while (SceCountEmAlive(0x36, -1) != 0) {
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// End of the regenerator cut (also its cancel path): camera back, stream faded (200 frames), the
// regenerator (0x32) may suspend, SceEventEnd.
static void r307_execEmCut_end()
{
    CamCtrl.Comeback(0);
    SndStrReq(r307_work.p->str, 4, 200, 0);
    cEmWrap em;
    em.setPtr(0x32, -1, 1);
    em.setNoSuspend(0);
    SceEventEnd(0);
}

// Area 3: the camera shows the regenerator behind the bars.
static void r307_execEmCut()
{
    RsfSet(G_ROOM_ID, 1);
    SceSetEventCancel(1, (TaskFunc) r307_execEmCut_end, 0, -1, 1);
    r307_work.p->str = SndStrReq(0, 3, 0x80000003, 0, 0, 0.0f);
    SceEventStart(0);
    cEmWrap em;
    em.setPtr(0x32, -1, 1);
    em.setNoSuspend(1);
    CamCtrl.CutCall(4);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r307_execEmCut_end();
}

// After the item is taken: area 1 = the regenerator's release, and its cEm36 virtual 0x50 (wake) is called.
void r307_setEmAppear()
{
    SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r307_appearEm, 0, 1);
    cEmWrap em;
    em.setPtr(0x32, -1, 1);
    if (em.getPtr() != 0) {
        ((cEm36*) em.getPtr())->v50();
    }
    SceSleep(15);
}

// Area 1: the regenerator comes out of its cell.
static void r307_appearEm()
{
    void* zero = 0;
    cObj* obj;

    RsfSet(G_ROOM_ID, 0);
    ScfFlagOn(pG, SCF_R307_REGENERATER_APPEAR);
    SceAtSetEnable(7, 0);
    SceAtSetEnable(8, 0);
    Vec pos = {-4168.0f, 0.0f, -3932.0f};
    SndCall(6, 0, &pos, 0, 0, 0);
    obj = SmdGetObjPtr(6);
    if (obj) {
        obj->be_flag &= ~2;
    }
    EstSet(0, -1, 0, 0, 1, 0, 0, 0, (u32) zero, zero);
    cEmWrap em;
    em.setPtr(0x32, -1, 1);
    em.setNoSuspend(1);
    em.setFlag(1);
    SceAtSetEnable(2, 0);
    em.setNoSuspend(0);
    SceExec(0x12, (TaskFunc) r307_checkBgm, 0, 0, 2, 0);
    *EM_LIST(0x32) = *EM_LIST(0x33);
    EmListSetAlive(0x32, 1);
}
