// Sscrn/ss_pzzl: the attache case (puzzle) screen of the sub screen DLL (D:/Bio4/Prog/ss_pzzl.cpp).
// Pieces are the pzlPlayer / pzlBoard / pzlPiece of game/puzzle.cpp; this unit draws them
// (piece models, cursor, grid) and runs the select / command / combine / case change widgets.
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "atari.h"
#include "widget.h"
#include "item.h"
#include "cockpit.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "joy.h"
#include "pad.h"
#include "snd.h"
#include "db_log.h"
#include "camera.h"
#include "view.h"
#include "trans_ot.h"
#include "math_sub.h"
#include "puzzle.h"
#include "pl_sub.h"
#include "sscrn.h"
#include "ss_main.h"
#include "ss_pzzl.h"
#include "pl_npc.h"

class cSubChar;

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// Matrix copy written out as loops (the original never calls PSMTXCopy in this unit): MTX_COPY
// (vec.h) as a do/while with `i_ = 2` between the two pointer inits, so the counter `li` is issued
// between the `d_` and `s_` inits (docs/matching.md, ss_pzzl second pass).
#define MTX_COPY_DO(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        int i_ = 2;                      \
        MtxPtr s_ = (src);               \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        do {                             \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        } while (i_--);                  \
    }


// ss_main.cpp

// ss_debug.cpp: the attache case editor
class ssDbgPzzl {
public:
    void* m_p_save_bak;
    u8 m_size_bak;
    s8 m_menu_no;
    s8 m_set_no;
    s8 m_bllt_no;

    void init(SUB_SCREEN* wk);
    void quit(SUB_SCREEN* wk);
    void move(SUB_SCREEN* wk);
};

// Board cell size in world units (a template-static-like COMMON word: set by caseModelMove).
struct pzlGrid {
    static f32 size;
};
f32 pzlGrid::size;
// The module's 0x34-byte COMMON block (make_rel appends the COMMON symbols after .bss): pzlGrid::size
// is its first word, the other 0x30 bytes are the unnamed template statics the original objects
// carried; the second .comm of the same name widens the symbol to the block size.
asm(".comm _7pzlGrid.size,52,4");

// Cursor frame: the four corner vertices of the cell run under the cursor (drawCursor).
struct PzzlCursor {
    Vec v[4];
};

// The puzzle screen widgets (include/ss_pzzl.h; SsPzzlMain::init creates them, ss_shop.cpp shares
// PzzlThinking / PieceSelect / CaseChange).

extern "C" {
void pzzlClearZ(SUB_SCREEN* wk);
u32 colorRRGGBBAA(u32 r, u32 g, u32 b, u32 a);
int back2PieceSelect(SUB_SCREEN* wk);
void pzzlEquipDisp(SUB_SCREEN* wk, int sw);
void pzzlCursorDisp(SUB_SCREEN* wk, int sw);
void drawCursorInit(SUB_SCREEN* wk, PzzlCursor* c);
void cmpVer(PzzlCursor* c, Vec* v);
void drawCursor(SUB_SCREEN* wk, pzlBoard* b, int x, int y, PzzlCursor* c, int line);
void drawGridLine(SUB_SCREEN* wk);
void puzzleCameraInit(SUB_SCREEN* wk, CAMERA* cam);
int puzzlePos2screenPos(Vec* pos, Vec* out);
void screenPos2puzzlePos(Vec* pos, Vec* out);
void pieceTblInit(SUB_SCREEN* wk);
void pieceModelOrientation(SUB_SCREEN* wk, pzlPiece* p);
void pieceFrameDisp(cModel* m, u32 color, int type);
void getPieceVertex(pzlPiece* p, Vec* out, int corner);
void pieceModelDisp(SUB_SCREEN* wk);
void pieceModelSet(pzlPiece* p);
void caseModelMove(int sw);
void tempSpaceDisp(int sw);
int checkWeaponChange(int id, int bullets);
void sscrn_pzzl_out_init(SUB_SCREEN* wk);
void sscrn_pzzl_in_init(SUB_SCREEN* wk);
int isTerminable(SUB_SCREEN* wk);
int checkMsgWindow(SUB_SCREEN* wk);
void closeMsgWindow(SUB_SCREEN* wk);
void openMsgWindow(SUB_SCREEN* wk, int no);
int remarkMsgCombine(int a, int b, int* no);
int itemCommandType(ItemWork* item);
}
static void setCommandId(u8 type, IdUnit** tbl, s8* num, int lang);

static int sscrn_pzzl_out(SUB_SCREEN* wk);

int msg_open = 0; // non-static: the REL's ADDR16 fields for it hold A only (scope:global)
int pzzlDbgNo = -12;
f32 pzzlDbgPos = -300.0f;
static u16 cursor_line_w_ot = 0xF;
static u16 cursor_line_w_prio = 0;
static u32 cursor_line_col = 0x707040FF;
static int cursor_line_blend = 10;
static u16 grid_line_w0_ot = 0xD;
static u16 grid_line_w0_prio = 0;
static u16 grid_line_w1_ot = 0xF;
static u16 grid_line_w1_prio = 0;
static u32 grid_line_col0 = 0x404040FF;
static u32 grid_line_col1 = 0x505050FF;
static u16 frame_line_w_ot = 0xD;
static u16 frame_line_w_prio = 0;
static u32 frame_line_col = 0x5F4B37FF;
static int frame_line_blend = 12;
static f32 frame_line_len = 40.0f;
static u16 frame_tile_w1_ot = 0xD;
static u16 frame_tile_w1_prio = 0;
static u32 frame_tile_col1 = 0;
static int frame_tile_blend1 = 2;
static u16 frame_tile_w2_ot = 0xF;
static u16 frame_tile_w2_prio = 0;
static u32 frame_tile_col2 = 0;
static u16 frame_tile_w3_ot = 0xF;
static u16 frame_tile_w3_prio = 0;
static u32 frame_tile_col3 = 0;
static u16 frame_tile_w4_ot = 0xF;
static u16 frame_tile_w4_prio = 0;
static u32 frame_tile_col4 = 0x30503080;
static int frame_tile_blend4 = 0;
static f32 piece_hand_scale = 30.0f;
static Vec case_rot = {-0.57f, 0.0f, 0.0f};
static int pzzl_wait[1] = {0};  // one-element array (SsFileInit idiom)
static s16 pzzl_font_w[2] = {0, 0x12};
static s16 pzzl_font_h[2] = {0, 0x18};
static s8 pzzl_font_space[4] = {-1, -1, -1, -1};
static int msg_x = 100;
static int msg_y = 160;
static int command_id = 4;
static int pzzl_dbg_step = 0;
u8 pzzl_tbl_AD0[20] = {0xF6, 0xF6, 0xF6, 0xF6, 0x00, 0x00, 0x27, 0x10, 0x05, 0x0A, 0x0A, 0x0A,
                       0x14, 0x1E, 0x46, 0x1E, 0x0A, 0x00, 0x00, 0x00};

static void* pzzl_clear_z;
static int pzzl_read_req;
// Non-static like msg_open: the REL's ADDR16 fields for these three hold A only (scope:global).
PzzlCursor pzzl_cursor;
pzlPiece* pzzl_sel;
ssDbgPzzl pzzl_dbg;


#define CMES_FLAGS (*(u32*) ((u8*) &cMes + 0xFC))
#define CMES_RESULT (*(s8*) ((u8*) &cMes + 0x1D1))

// Queues clearZbuffer into OT 0xF before the case/piece models (they draw over the 2D background).
void pzzlClearZ(SUB_SCREEN* wk)
{
    AddOtDirect(0xF, &pzzl_clear_z, (void (*)()) clearZbuffer, 2, 0x1000, 0, 0.0f);
}

// Packs bytes into the RRGGBBAA colour word the ss_Draw_* primitives take.
u32 colorRRGGBBAA(u32 r, u32 g, u32 b, u32 a)
{
    return (r << 24) | (g << 16) | (b << 8) | a;
}

// Leaving PzzlThinking / PieceCommand: when the temporary space board is empty, restores the life
// meter, the case header unit and the main menu fade and hides the space; returns 1 then, else 0.
int back2PieceSelect(SUB_SCREEN* wk)
{
    int ret;

    if (wk->puzzlePlayer->m_space->getPieceNum() != 0) {
        ret = 0;
    } else {
        Cckpt.m_LifeMeter.frameIn();
        IdSub.unitPtr(0, IDC_SSCRN_PESETA)->rev_flag &= 0xF0;
        tempSpaceDisp(0);
        idMainMenuFade(wk, 1);
        ret = 1;
    }
    return ret;
}

// Marks the equipped weapon piece (IdSub 0x30/4 at its top-right corner + a type-3 frame) and its
// attached parts (0x40, 0x50), except the piece in hand; sw 0 hides the marks.
void pzzlEquipDisp(SUB_SCREEN* wk, int sw)
{
    pzlPlayer* pl = wk->puzzlePlayer;
    IdUnit* id[3];
    IdUnit* id2[3];
    Vec scr;
    u8 unused0[0x20];  // two unused 0x20-byte locals keep the original frame (0x38 / 0x68)
    Vec pos;
    u8 unused1[0x20];
    ItemInfo info;
    pzlPiece* arm;
    int i;

    id[0] = IdSub.unitPtr(0x30, IDC_SSCRN_ETC);
    id2[0] = IdSub.unitPtr(0x31, IDC_SSCRN_ETC);
    id2[0]->be_flag &= ~8;
    id[1] = IdSub.unitPtr(0x40, IDC_SSCRN_ETC);
    id2[1] = IdSub.unitPtr(0x41, IDC_SSCRN_ETC);
    id2[1]->be_flag &= ~8;
    id[2] = IdSub.unitPtr(0x50, IDC_SSCRN_ETC);
    id2[2] = IdSub.unitPtr(0x51, IDC_SSCRN_ETC);
    id2[2]->be_flag &= ~8;
    itemInfo(ItemMgr.m_wep_id, &info);
    switch (info.type) {
    case 1:
        arm = pl->piecePtr(ItemMgr.pArm);
        break;
    case 3:
    case 6:
        if (ItemMgr.num(ItemMgr.pArm) != 0 && ItemMgr.m_wep_id == ItemMgr.pArm->id) {
            arm = pl->piecePtr(ItemMgr.pArm);
        } else {
            arm = pl->piecePtr(ItemMgr.minimumSearch(ItemMgr.m_wep_id));
        }
        break;
    default:
        arm = 0;
        break;
    }
    if (sw && arm) {
        if (arm == pl->m_inhand) {
            id[0]->be_flag &= ~8;
        } else {
            u32 col;

            getPieceVertex(arm, &pos, 1);
            puzzlePos2screenPos(&pos, &scr);
            id[0]->be_flag |= 8;
            id[0]->pos0 = scr;
            col = colorRRGGBBAA(id[0]->col0[0], id[0]->col0[1], id[0]->col0[2], 0x20);
            pieceFrameDisp(arm->model, col, 3);
        }
        id[1]->be_flag &= ~8;
        id[2]->be_flag &= ~8;
        for (i = 0; i < 2; i++) {
            ItemWork* w = ItemMgr.weaponParts(ItemMgr.pArm, i);
            pzlPiece* p;

            if (w == 0) {
                break;
            }
            p = pl->piecePtr(w);
            if (p != pl->m_inhand) {
                u32 col;

                getPieceVertex(p, &pos, 1);
                puzzlePos2screenPos(&pos, &scr);
                id[i + 1]->be_flag |= 8;
                id[i + 1]->pos0 = scr;
                col = colorRRGGBBAA(id[i + 1]->col0[0], id[i + 1]->col0[1], id[i + 1]->col0[2], 0x20);
                pieceFrameDisp(p->model, col, 3);
            }
        }
    } else {
        id[0]->be_flag &= ~8;
        id2[0]->be_flag &= ~8;
        id[1]->be_flag &= ~8;
        id2[1]->be_flag &= ~8;
        id[2]->be_flag &= ~8;
        id2[2]->be_flag &= ~8;
    }
}

// Draws the case cursor every frame: the cell frame under the board cursor (drawCursor), the piece
// highlight by wk->cursor_mode (1 command: piece under the cursor in the 0x20/4 colour; 2 combine:
// target 0x21 and the source pzzl_sel 0x22), the equip marks and the grid lines. cursor_flag bits
// restart the highlight animation after a move; sw 0 hides everything.
void pzzlCursorDisp(SUB_SCREEN* wk, int sw)
{
    IdUnit* u0 = IdSub.unitPtr(0x20, IDC_SSCRN_ETC);
    IdUnit* u1 = IdSub.unitPtr(0x21, IDC_SSCRN_ETC);
    IdUnit* u2 = IdSub.unitPtr(0x22, IDC_SSCRN_ETC);
    pzlBoard* b = wk->puzzlePlayer->m_p_active_board;
    int x = b->m_cur_x;
    int y = b->m_cur_y;
    pzlPiece* p;
    u32 col;

    u0->be_flag &= ~8;
    u1->be_flag &= ~8;
    u2->be_flag &= ~8;
    if (!sw) {
        wk->cursor_flag = sw;
        return;
    }
    drawCursorInit(wk, &pzzl_cursor);
    drawCursor(wk, wk->puzzlePlayer->m_p_active_board, x, y, &pzzl_cursor, 0);
    switch (wk->cursor_mode) {
    case 1:
        if (wk->cursor_flag & 3) {
            IdSub.setTime(u0, 0);
        }
        pzzlEquipDisp(wk, 1);
        drawCursorInit(wk, &pzzl_cursor);
        drawCursor(wk, wk->puzzlePlayer->m_p_active_board, x, y, &pzzl_cursor, 1);
        col = colorRRGGBBAA((u8) u0->col[0], (u8) u0->col[1], (u8) u0->col[2], (u8) u0->col[3]);
        {
            // COMPILER-DIFF: 5 (sched1 tie). Case 1 of the target issues the `col` copy before the
            // `wk->puzzlePlayer` reload, so the load lands in r3 (`mr r30,r3; lwz r3,0x2b0(r31)`); a pseudo
            // load outranks the copy (load latency 2) and gives case 2's `lwz r0; mr r30,r3; mr r3,r0`
            // in both arms. The r3 pin makes the load's destination the hard register: its
            // anti-dependence on the `col` copy (which reads r3) orders it after the copy.
            register pzlPlayer* pl asm("r3");
            pl = wk->puzzlePlayer;
            p = pl->ptrPiece(pl->m_p_active_board);
        }
        if (p) {
            pieceFrameDisp(p->model, col, 1);
        }
        break;
    case 2:
        if (wk->cursor_flag & 3) {
            IdSub.setTime(u1, 0);
        }
        if (wk->cursor_flag & 2) {
            IdSub.setTime(u2, 0);
        }
        pzzlEquipDisp(wk, 1);
        drawCursorInit(wk, &pzzl_cursor);
        drawCursor(wk, wk->puzzlePlayer->m_p_active_board, x, y, &pzzl_cursor, 1);
        col = colorRRGGBBAA((u8) u1->col[0], (u8) u1->col[1], (u8) u1->col[2], (u8) u1->col[3]);
        p = wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board);
        if (p) {
            pieceFrameDisp(p->model, col, 1);
        }
        col = colorRRGGBBAA((u8) u2->col[0], (u8) u2->col[1], (u8) u2->col[2], (u8) u2->col[3]);
        if (pzzl_sel) {
            pieceFrameDisp(pzzl_sel->model, col, 2);
        }
        break;
    default:
        u0->be_flag &= ~8;
        u1->be_flag &= ~8;
        u2->be_flag &= ~8;
        pzzlEquipDisp(wk, 1);
        drawCursorInit(wk, &pzzl_cursor);
        drawCursor(wk, wk->puzzlePlayer->m_p_active_board, x, y, &pzzl_cursor, 0);
        break;
    }
    wk->cursor_flag = 0;
    drawGridLine(wk);
}

// Resets the cursor bounding corners (min/max seeds) and the boards' visited state (bit 7) before
// drawCursor walks the piece cells.
void drawCursorInit(SUB_SCREEN* wk, PzzlCursor* c)
{
    const f32 big = 1000.0f;

    wk->puzzlePlayer->m_board->clearState(0x80);
    wk->puzzlePlayer->m_space->clearState(0x80);
    memclr_asm(c, sizeof(PzzlCursor));
    c->v[0].x = big;
    c->v[0].y = -big;
    c->v[1].x = -big;
    c->v[1].y = -big;
    c->v[2].x = -big;
    c->v[2].y = big;
    c->v[3].x = big;
    c->v[3].y = big;
}

// Extends the cursor frame corners by a vertex.
void cmpVer(PzzlCursor* c, Vec* v)
{
    Vec* p = c->v;

    if (v->x < p->x) {
        p->x = v->x;
    }
    if (v->y > p->y) {
        p->y = v->y;
    }
    p++;
    if (v->x > p->x) {
        p->x = v->x;
    }
    if (v->y > p->y) {
        p->y = v->y;
    }
    p++;
    if (v->x > p->x) {
        p->x = v->x;
    }
    if (v->y < p->y) {
        p->y = v->y;
    }
    p++;
    if (v->x < p->x) {
        p->x = v->x;
    }
    if (v->y < p->y) {
        p->y = v->y;
    }
}

// Draws the cursor cell run (the cells of the piece under (x, y), or the cell itself): recursive
// flood over the neighbouring cells of the same piece, one edge line per outer side.
void drawCursor(SUB_SCREEN* wk, pzlBoard* b, int x, int y, PzzlCursor* c, int line)
{
    Vec p;
    Vec a;
    Vec d;
    Vec wa;
    Vec wd;
    Mtx mat;
    ItemWork* item;
    f32 g;
    pzlPiece* piece;
    Vec e;

    MTX_COPY_DO(b->m_mat, mat);
    g = pzlGrid::size;
    if (b->cellState((s8) x, (s8) y) & 0x82) {
        return;
    }
    item = 0;
    *b->cell((s8) x, (s8) y) |= 0x80;
    piece = b->getPiece((s8) x, (s8) y);
    if (piece) {
        item = piece->item;
    }
    p.x = g * (f32) x;
    p.y = -g * (f32) y;
    p.z = 0.0f;
    piece = b->getPiece((s8) x, (s8) (y + 1));
    if (piece && item == piece->item) {
        drawCursor(wk, b, x, y + 1, c, line);
    } else {
        a = p;
        a.y -= g;
        d = p;
        d.y -= g;
        d.x += g;
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &d, &wd);
        if (line) {
            ss_Draw_line3d(&wa, &wd, cursor_line_col, cursor_line_blend, 0, 0, cursor_line_w_ot, cursor_line_w_prio);
        }
        e = wa;
        cmpVer(c, &e);
        e = wd;
        cmpVer(c, &e);
    }
    piece = b->getPiece((s8) x, (s8) (y - 1));
    if (piece && item == piece->item) {
        drawCursor(wk, b, x, y - 1, c, line);
    } else {
        a = p;
        d = p;
        d.x += g;
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &d, &wd);
        if (line) {
            ss_Draw_line3d(&wa, &wd, cursor_line_col, cursor_line_blend, 0, 0, cursor_line_w_ot, cursor_line_w_prio);
        }
        e = wa;
        cmpVer(c, &e);
        e = wd;
        cmpVer(c, &e);
    }
    piece = b->getPiece((s8) (x - 1), (s8) y);
    if (piece && item == piece->item) {
        drawCursor(wk, b, x - 1, y, c, line);
    } else {
        a = p;
        d = p;
        d.y -= g;
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &d, &wd);
        if (line) {
            ss_Draw_line3d(&wa, &wd, cursor_line_col, cursor_line_blend, 0, 0, cursor_line_w_ot, cursor_line_w_prio);
        }
        e = wa;
        cmpVer(c, &e);
        e = wd;
        cmpVer(c, &e);
    }
    piece = b->getPiece((s8) (x + 1), (s8) y);
    if (piece && item == piece->item) {
        drawCursor(wk, b, x + 1, y, c, line);
    } else {
        a = p;
        a.x += g;
        d = p;
        d.x += g;
        d.y -= g;
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &d, &wd);
        if (line) {
            ss_Draw_line3d(&wa, &wd, cursor_line_col, cursor_line_blend, 0, 0, cursor_line_w_ot, cursor_line_w_prio);
        }
        e = wa;
        cmpVer(c, &e);
        e = wd;
        cmpVer(c, &e);
    }
}

// Draws the case board grid (cell size pzlGrid::size, two line colours on OT 0xD / 0xF) in the
// board's matrix.
void drawGridLine(SUB_SCREEN* wk)
{
    Mtx mat;
    Vec a;
    Vec b;
    Vec wa;
    Vec wb;
    Vec wc;
    f32 g = pzlGrid::size;
    int w;
    int h;
    int i;
    u32 col;
    u16 ot;
    u16 prio;

    MTX_COPY_DO(wk->puzzlePlayer->m_board->m_mat, mat);
    h = wk->puzzlePlayer->m_board->m_size_y;
    w = wk->puzzlePlayer->m_board->m_size_x;
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b = a;
    b.y -= (f32) h * g;
    col = grid_line_col0;
    ot = grid_line_w0_ot;
    prio = grid_line_w0_prio;
    for (i = 0; i <= w; i++) {
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &b, &wb);
        ss_Draw_line3d(&wa, &wb, col, 6, 0, 1, ot, prio);
        a.x += g;
        b.x += g;
    }
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b = a;
    b.x += (f32) w * g;
    for (i = 0; i <= h; i++) {
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &b, &wc);
        ss_Draw_line3d(&wa, &wc, col, 6, 0, 1, ot, prio);
        a.y -= g;
        b.y -= g;
    }
    MTX_COPY_DO(wk->puzzlePlayer->m_space->m_mat, mat);
    h = wk->puzzlePlayer->m_space->m_size_y;
    w = wk->puzzlePlayer->m_space->m_size_x;
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b = a;
    b.y -= (f32) h * g;
    col = grid_line_col1;
    ot = grid_line_w1_ot;
    prio = grid_line_w1_prio;
    for (i = 0; i <= w; i++) {
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &b, &wb);
        ss_Draw_line3d(&wa, &wb, col, 6, 0, 1, ot, prio);
        a.x += g;
        b.x += g;
    }
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b = a;
    b.x += (f32) w * g;
    for (i = 0; i <= h; i++) {
        PSMTXMultVec(mat, &a, &wa);
        PSMTXMultVec(mat, &b, &wb);
        ss_Draw_line3d(&wa, &wb, col, 6, 0, 1, ot, prio);
        a.y -= g;
        b.y -= g;
    }
}

// The case screen uses the common sub screen camera.
void puzzleCameraInit(SUB_SCREEN* wk, CAMERA* cam)
{
    sscrnCameraInit(wk, cam);
}

// World -> screen (320 x 240 half extents) through the current camera; 0 when behind the near plane.
int puzzlePos2screenPos(Vec* pos, Vec* out)
{
    Mtx inv;
    f32 az;
    f32 h;
    f32 w;
    f32 ang;

    PSMTXInverse(pG->Camera.mat, inv);
    PSMTXMultVec(inv, pos, out);
    if (out->z > -fabsf(ZNEAR)) {
        return 0;
    }
    ang = pG->Camera.param.fovy * 0.5f * 0.017453292f;
    az = fabsf(out->z);
    h = az * tanf(ang);
    w = h * 1.3333334f;
    out->x = out->x * (320.0f / w);
    out->y = out->y * (240.0f / h);
    out->z = out->z * 0.0f;
    return 1;
}

// Screen (+-240 half height) -> world x/y at the camera's z distance (the id unit positions of the
// case).
void screenPos2puzzlePos(Vec* pos, Vec* out)
{
    CAMERA* cam = &pG->Camera;
    f32 pz = cam->param.pos.z;
    f32 h = fabsf((f32) (pz * tan(cam->param.fovy * 0.5f * 3.1415927f / 180.0f)));

    out->x = pos->x * h / 240.0f;
    out->y = pos->y * h / 240.0f;
}

// Relocates the piece_info model / texture offsets of ss_pzzl.dat into pointers.
void pieceTblInit(SUB_SCREEN* wk)
{
    // The target keeps `tbl` (`lis r9; addi r11,r9,@l`) as a register: the entry load is `lhz 0(r11)`
    // and the loop base is a copy (`mr r7,r11`). A plain `tbl->id` load is folded into the lo_sum by
    // cse's find_best_addr (same address cost, higher rtx cost wins), so the entry test indexes with a
    // zero pseudo: `(mem (plus z tbl))` is not a register address for cse (the folded `(reg tbl)`
    // is the cheaper form, so it is kept as written), gcse's cprop only propagates sets that reach
    // the block entry (`z = 0` is in the same block), and combine folds the single-use `z = 0` into
    // `(mem (reg tbl))` after both cse passes. COMPILER-DIFF: 13.
    PieceInfo* tbl;
    PieceInfo* base;
    void** mp;
    u32 ofs;

    tbl = piece_info;
    {
        u32 z = 0;
        if (((PieceInfo*) (z + (u32) tbl))->id == 0xFFFF) {
            return;
        }
    }
    // The dead `tbl = 0` (deleted by flow) kills the copy `base = tbl` for gcse's copy propagation
    // and for cse's canon of `mp` (tbl leaves base's class), so the copy survives; base/mp keep
    // cse's REG_EQUAL notes (symbol, symbol+84), which halves their global-alloc length below the
    // byte-offset biv like the target (r7/r6 after r8). COMPILER-DIFF: 13.
    base = tbl;
    tbl = 0;
    // Explicit byte-offset biv (user init: `li r8,0` precedes the hoisted 0xFFFF constant) with a
    // separate store pointer biv based at model[4] (`stw -4(r6)`/`stw 0(r6)`); the id re-reads are
    // volatile so that gcse's PRE does not merge the identical `(mem (plus ofs base))` loads (the
    // `tbl[i].id` index form keeps them apart through the PRE'd `i*120` copies, but its giv init
    // comes after the constant).
    mp = (void**) ((u32) base + 84);
    ofs = 0;
    do {
        int mdl;
        int tex;

        switch (((PieceInfo*) (ofs + (u32) base))->id) {
        case 0x40:
            mdl = 0x21;
            break;
        case 0x17:
        case 0x35:
        case 0x6D:
            mdl = 0x35;
            break;
        case 0x12 ... 0x16:
        case 0xA8:
            mdl = 0x12;
            break;
        case 1 ... 2:
        case 0xE:
            mdl = 1;
            break;
        case 8 ... 0xA:
            mdl = 8;
            break;
        case 6:
        case 0x19:
        case 0x1C:
            mdl = 6;
            break;
        case 0:
        case 4:
        case 7:
        case 0x18:
        case 0x1A:
        case 0x20:
        case 0x6A:
        case 0xA0:
            mdl = 0;
            break;
        default:
            mdl = *(volatile u16*) (ofs + (u32) base);
            break;
        }
        mdl = mdl * 2 + 4;
        switch (*(volatile u16*) (ofs + (u32) base)) {
        case 0x40:
            tex = 0x21;
            break;
        case 0x95:
        case 0x97:
            tex = 0x95;
            break;
        case 0x6D:
            tex = 0x35;
            break;
        default:
            tex = *(volatile u16*) (ofs + (u32) base);
            break;
        }
        // Index-first `lwzx` (offset + arc, not arc->ofs[no]); the tex index goes through a block-local
        // so the +20 is not folded into the load displacement.
        mp[-1] = (void*) (*(u32*) (mdl * 4 + (u32) wk->pPzzlDat) + (u32) wk->pPzzlDat);
        {
            u32 tix = (tex * 2 + 5) * 4;
            mp[0] = (void*) (*(u32*) (tix + (u32) wk->pPzzlDat) + (u32) wk->pPzzlDat);
        }
        mp += 30;
        ofs += 120;
    } while (((PieceInfo*) (ofs + (u32) base))->id != 0xFFFF);
}

// Sets the piece model's rotation and cell position from pzlPiece::m_orientation (0..3 quarter
// turns) and its board coordinates (or the in-hand position above the cursor).
void pieceModelOrientation(SUB_SCREEN* wk, pzlPiece* p)
{
    cModel* m;
    pzlBoard* b;

    if (p == 0) {
        return;
    }
    m = p->model;
    switch (p->m_orientation) {
    case 0:
        m->ang.x = 0.0f;
        m->ang.y = 0.0f;
        m->ang.z = 0.0f;
        break;
    case 1:
        m->ang.x = 0.0f;
        m->ang.y = 0.0f;
        m->ang.z = -1.5707964f;
        break;
    case 2:
        m->ang.x = 0.0f;
        m->ang.y = 0.0f;
        m->ang.z = -3.1415927f;
        break;
    case 3:
        m->ang.x = 0.0f;
        m->ang.y = 0.0f;
        m->ang.z = -4.712389f;
        break;
    case 4:
        m->ang.x = 3.1415927f;
        m->ang.z = 3.1415927f;
        m->ang.y = 0.0f;
        break;
    case 5:
        m->ang.x = 3.1415927f;
        m->ang.y = 0.0f;
        m->ang.z = 1.5707964f;
        break;
    case 6:
        m->ang.x = 3.1415927f;
        m->ang.y = 0.0f;
        m->ang.z = 0.0f;
        break;
    case 7:
        m->ang.x = 3.1415927f;
        m->ang.y = 0.0f;
        m->ang.z = -1.5707964f;
        break;
    }
    if (!p->isInHand()) {
        if (wk->puzzlePlayer->m_board->search(p)) {
            b = wk->puzzlePlayer->m_board;
        } else {
            if (!wk->puzzlePlayer->m_space->search(p)) {
                pLog->err(0, 0, "pieceModelOrientation(): lost piece");
            }
            b = wk->puzzlePlayer->m_space;
        }
    } else {
        b = wk->puzzlePlayer->m_p_active_board;
    }
    m->pos.x = pzlGrid::size * (p->m_pos_x + 0.5f);
    m->pos.y = -pzlGrid::size * (p->m_pos_y + 0.5f);
    m->matUpdate();
    PSMTXConcat(b->m_mat, m->mat, m->mat);
    m->partsWorldCalc();
}


// Frame around a piece model: type 0 corner lines, 1..3 tiles, 4 the whole piece (scaled).
void pieceFrameDisp(cModel* m, u32 color, int type)
{
    Vec size;
    Vec ofs;
    Vec v[4];
    Vec c;
    int i;

    if (m == 0 || m->pModelInfo == 0) {
        return;
    }
    {
        ModelBound* bd = &m->pModelInfo->bound;

        size.x = bd->size.x;
        size.y = bd->size.y;
    }
    size.z = 0.0f;
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            size.x = fabsf(size.x);
            size.y = fabsf(size.y);
            break;
        case 1:
            size.x = -fabsf(size.x);
            size.y = fabsf(size.y);
            break;
        case 2:
            size.x = -fabsf(size.x);
            size.y = -fabsf(size.y);
            break;
        case 3:
            size.x = fabsf(size.x);
            size.y = -fabsf(size.y);
            break;
        }
        PSMTXMultVecSR(m->mat, &size, &ofs);
        c.x = m->mat[0][3];
        c.y = m->mat[1][3];
        c.z = m->mat[2][3];
        PSVECAdd(&c, &ofs, &v[i]);
    }
    // COMPILER-DIFF: candidate #13 (gcse expression-table size). Six dead stores (flow1 deletes
    // them) add six insns at gcse time, so the expression hash table has 159 buckets instead of
    // 155 and the PRE'd `&v[3]` (fp+84) reaching register is numbered before `&v[1]` (fp+60):
    // case 4's tile call then has `&v[3]` in r31 and `&v[1]` in r27.
    i = 0;
    i = 1;
    i = 2;
    i = 3;
    i = 4;
    i = 5;
    switch (type) {
    case 0:
        for (i = 0; i < 4; i++) {
            int prev = i - 1;
            int next = i + 1;
            int j;

            if (prev < 0) {
                prev = i + 3;
            }
            if (next > 3) {
                next = i - 3;
            }
            for (j = 0; j < 2; j++) {
                int k = j == 0 ? next : prev;

                PSVECSubtract(&v[k], &v[i], &c);
#line 1158 "D:/Bio4/Prog/ss_pzzl.cpp"
                VECNormalize(&c, &c);
                PSVECScale(&c, &c, frame_line_len);
                PSVECAdd(&c, &v[i], &c);
                ss_Draw_line3d(&v[i], &c, frame_line_col, frame_line_blend, 0, 1, frame_line_w_ot, frame_line_w_prio);
            }
        }
        break;
    case 1:
        ss_Draw_tile3d(&v[0], &v[1], &v[3], &v[2], color, frame_tile_col1, frame_tile_blend1, frame_tile_w1_ot, frame_tile_w1_prio);
        break;
    case 2:
        ss_Draw_tile3d(&v[0], &v[1], &v[3], &v[2], color, frame_tile_col2, 1, frame_tile_w2_ot, frame_tile_w2_prio);
        break;
    case 3:
        ss_Draw_tile3d(&v[0], &v[1], &v[3], &v[2], color, frame_tile_col3, 1, frame_tile_w3_ot, frame_tile_w3_prio);
        break;
    case 4:
        c.x = m->mat[0][2];
        c.y = m->mat[1][2];
        c.z = m->mat[2][2];
        PSVECScale(&c, &c, m->pos.z);
        for (i = 0; i < 4; i++) {
            PSVECSubtract(&v[i], &c, &v[i]);
        }
        ss_Draw_tile3d(&v[0], &v[1], &v[3], &v[2], frame_tile_col4, frame_tile_blend4, 1, frame_tile_w4_ot, frame_tile_w4_prio);
        break;
    }
}

// World position of a piece corner (0 upper right, 1 upper left).
void getPieceVertex(pzlPiece* p, Vec* out, int corner)
{
    cModel* m = p->model;
    ModelBound* bd = &m->pModelInfo->bound;
    Vec c;

    out->x = bd->size.x;
    out->y = bd->size.y;
    out->z = 0.0f;
    PSMTXMultVecSR(m->mat, out, out);
    switch (corner) {
    case 0:
        out->x = fabsf(out->x);
        out->y = -fabsf(out->y);
        out->z = fabsf(out->z);
        break;
    case 1:
        out->x = -fabsf(out->x);
        out->y = fabsf(out->y);
        out->z = fabsf(out->z);
        break;
    }
    c.x = m->mat[0][3];
    {
        Vec* pc = &c;

        pc->y = m->mat[1][3];
        pc->z = m->mat[2][3];
        PSVECAdd(pc, out, out);
    }
}

// The &info argument as an inlined helper parameter: integrate substitutes the frame address into the
// argument register set, so each call recomputes `addi r4, r1, ofs` instead of gcse PRE hoisting one
// pseudo (COMPILER-DIFF: 3).
static inline void pzzlItemInfo(int id, ItemInfo* info)
{
    itemInfo(id, info);
}

// Per frame: places every piece model (pieceModelOrientation), shows its count / ammo digits
// (numDisp 0x40 + i at the piece's corner, hidden for single items), the in-hand piece raised.
void pieceModelDisp(SUB_SCREEN* wk)
{
    pzlPlayer* pl;
    pzlPiece* hand;
    int no = 0;

    for (int i = 0; i < 0x3E; i++) {
        numDisp(0x40 + i, 0, 0, 0);
    }
    pl = wk->puzzlePlayer;
    hand = pl->m_inhand;
    for (int i = 0; i < wk->puzzlePlayer->pieceNum(); i++) {
        pzlPiece* p = wk->puzzlePlayer->piecePtr(i);
        cModel* m = p->model;
        Vec pos;
        Vec scr;
        ItemInfo info;
        ItemWork* item;
        int id;

        if (wk->puzzlePlayer->m_space->search(p)) {
            m->ot_type = 1;
        } else {
            m->ot_type = 3;
        }
        if (p->isOnBoard() || (hand && hand == p)) {
            m->be_flag |= 2;
            id = 0x41 + no;
            if (hand && p == hand) {
                m->pos.z = piece_hand_scale;
                m->ot_type = 1;
                id = 0x40;
            } else {
                m->pos.z = 0.0f;
            }
            pieceModelOrientation(wk, p);
            getPieceVertex(p, &pos, 0);
            puzzlePos2screenPos(&pos, &scr);
            item = p->item;
            pzzlItemInfo(item->id, &info);
            if (info.type == 1) {
                u32 x8 = item->bullet;
                u32 num = x8 & 0x1FFF;

                if ((x8 >> 13) == 1) {
                    numDisp(id, num, &scr, 3);
                } else {
                    numDisp(id, num, &scr, 1);
                }
                no++;
            } else {
                pzzlItemInfo(item->id, &info);
                if (info.type != 9) {
                    pzzlItemInfo(item->id, &info);
                    if (info.maxNum != 1 || item->num != 1) {
                        numDisp(id, item->num, &scr, 1);
                        no++;
                    }
                }
            }
            if (hand && hand == p) {
                pieceFrameDisp(m, 0, 4);
            } else {
                pieceFrameDisp(m, 0, 0);
            }
        } else {
            m->be_flag &= ~2;
        }
    }
    MapMgr.move();
}

// Light setting shared by the case and piece models (one static pair in .rodata).
static inline void pzzlModelLight(cModel* m)
{
    static const Vec ofs = {0.0f, 0.0f, 0.0f};
    static const Vec size = {1000.0f, 1000.0f, 0.0f};

    m->LightInfo.init2(0, 0, &ofs, &size, 0x10);
}

// Binds the pzlPlayer pieces to MapMgr works 4.., builds their models, and the case model (work 3,
// ss_pzzl.dat 0x1A6..0x1A9 by board_size) tilted by case_rot; first frame draw.
void pieceModelInit(SUB_SCREEN* wk)
{
    pzlPlayer* pl = wk->puzzlePlayer;
    cModel* m;
    int i;

    pieceTblInit(wk);
    for (i = 0; i < pl->m_piece_max; i++) {
        pzlPiece* p = &pl->m_piece[i];
        pzlPiece* q;

        p->model = MapMgr.getWork(i + 4);
        q = &pl->m_piece[i];
        q->model->be_flag &= ~2;
    }
    for (i = 0; i < pl->pieceNum(); i++) {
        pieceModelSet(pl->piecePtr(i));
    }
    m = MapMgr.getWork(3);
    switch ((s8) wk->board_size) {
    case 0:
        m->modelInit(SS_ARC_PTR(wk->pPzzlDat, 0x1A6), SS_ARC_PTR(wk->pPzzlDat, 0x1A5));
        break;
    case 1:
        m->modelInit(SS_ARC_PTR(wk->pPzzlDat, 0x1A7), SS_ARC_PTR(wk->pPzzlDat, 0x1A5));
        break;
    case 2:
        m->modelInit(SS_ARC_PTR(wk->pPzzlDat, 0x1A8), SS_ARC_PTR(wk->pPzzlDat, 0x1A5));
        break;
    case 3:
        m->modelInit(SS_ARC_PTR(wk->pPzzlDat, 0x1A9), SS_ARC_PTR(wk->pPzzlDat, 0x1A5));
        break;
    }
    pzzlModelLight(m);
    m->CullMode = 2;
    m->ang = case_rot;
    m->matUpdate();
    m->ot_type = 3;
    caseModelMove(1);
    pzzlClearZ(wk);
    pieceModelDisp(wk);
    pzzlCursorDisp(wk, 1);
    m->be_flag |= 2;
}

// Builds the model of piece `p` from the piece_info entry of its item id (bin/tpl pair).
void pieceModelSet(pzlPiece* p)
{
    void** data = (void**) searchItemModelData(p->item->id, piece_info);
    cModel* m;

    if (data == 0) {
        return;
    }
    m = p->model;
    m->modelInit(data[0], data[1]);
    pzzlModelLight(m);
    m->CullMode = 2;
    if (m->pModelInfo->be_flag & 2) {
        m->pModelInfo->be_flag &= ~2;
        pLog->warn(0, 0, "pieceModelSet(): 0x%02x flag SHAPE_MODEL clear", p->item->id);
    }
    m->ot_type = 3;
}

// Places the case model under the case id unit, sets the board matrices from it (sw: opening
// position from the id unit, else the fixed position) and the case-space cursor.
void caseModelMove(int sw)
{
    Vec ofsA = {-1280.0f, 0.0f, 0.0f};
    Vec ofsB = {1280.0f, 0.0f, 0.0f};
    SUB_SCREEN* wk = &SubScreenWk;
    IdUnit* u = IdSub.unitPtr(0xFE, IDC_SSCRN_BACK_GROUND);
    IdUnit* u2 = IdSub.unitPtr(0xFD, IDC_SSCRN_BACK_GROUND);
    cModel* m = MapMgr.getWork(3);
    cParts* parts = m->getPartsPtr(1);
    Vec scr;
    Vec q;
    pzlBoard* b;
    const f32 size = 100.0f;

    if (sw) {
        scr = ofsA;
    } else {
        scr = u->pos;
    }
    scr.y += (f32) pzzlDbgNo;
    screenPos2puzzlePos(&scr, &m->pos);
    PSVECScale(&u2->rot, &parts->ang, 0.017453292f);
    m->pos.z = pzzlDbgPos;
    m->matUpdate();
    pzlGrid::size = size;
    b = wk->puzzlePlayer->m_board;
    {
        Mtx tmp;
        Vec p;
        Vec ax;
        Vec ay;
        Vec az;
        int w = b->m_size_x;

        p.x = (f32) w * -0.5f * pzlGrid::size;
        p.y = -0.0f;
        p.z = 0.0f;
        PSMTXMultVec(m->mat, &p, &p);
        MTX_COPY_DO(m->mat, tmp);
        ax.x = m->mat[0][0];
        ax.y = m->mat[1][0];
        ax.z = m->mat[2][0];
        {
            f32* col = &m->mat[0][1];
            ay.x = m->mat[0][1];
            ay.y = col[4];
            ay.z = m->mat[2][1];
        }
        az.x = m->mat[0][2];
        az.y = m->mat[1][2];
        az.z = m->mat[2][2];
        tmp[0][0] = ax.x;
        tmp[1][0] = ax.y;
        tmp[2][0] = ax.z;
        tmp[0][1] = ay.x;
        tmp[1][1] = ay.y;
        tmp[2][1] = ay.z;
        tmp[0][2] = az.x;
        tmp[1][2] = az.y;
        tmp[2][2] = az.z;
        tmp[0][3] = p.x;
        tmp[1][3] = p.y;
        tmp[2][3] = p.z;
        MTX_COPY_DO(tmp, b->m_mat);
    }
    u = IdSub.unitPtr(0, IDC_SSCRN_NEAR_0);
    b = wk->puzzlePlayer->m_space;
    screenPos2puzzlePos(&u->pos, &q);
    if (sw) {
        q = ofsB;
    }
    {
        Vec scr2;
        Mtx mat;
        Vec t;
        Vec p;
        Mtx mat2;

        MTX_COPY_DO(wk->puzzlePlayer->m_board->m_mat, mat2);
        p.x = 0.0f;
        p.y = pzlGrid::size + pzlGrid::size;
        p.z = 0.0f;
        PSMTXMultVecSR(mat2, &p, &p);
        t.x = mat2[0][3];
        t.y = mat2[1][3];
        t.z = mat2[2][3];
        PSVECAdd(&t, &p, &t);
        MTX_COPY_DO(mat2, mat);
        mat[0][3] = q.x;
        mat[1][3] = t.y;
        mat[2][3] = t.z;
        MTX_COPY_DO(mat, b->m_mat);
        puzzlePos2screenPos(&q, &scr2);
        u->pos0.y = scr2.y;
    }
}

// Loads the attache case screen: state 0 run the previous screen's scrn_out_func and drop its ids,
// 1 one frame wait, 2 show the HUD and read SS/<lang>/ss_pzzl.dat (coming from the map rebuilds
// the character model and life meter), 3 wait (archive -> pPzzlDat), 4 fade in for SS_OPEN_PZZL (type
// 4, item pick-up) and transit to SsPzzlMain.
void SsPzzlInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
        if (wk->scrn_out_func(wk) == 1) {
            if (wk->menu_old == 2) {
                wk->wait_cnt = 1;
            }
            IdSubErase();
            IdNumErase();
            IdFreeBuffer();
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_0, 0xC, 6, 0);
            pzzl_wait[0] = 0;
            state++;
        }
        break;
    case 1:
        if (--pzzl_wait[0] >= 0) {
            break;
        }
        state++;
        break;
    case 2:
        IdSys.dispSw(IDC_LIFE_METER, 1);
        IdSub.dispSw(IDC_SSCRN_PESETA, 1);
        sscrnDataFilename(wk, "ss_pzzl.dat");
#line 1682 "D:/Bio4/Prog/ss_pzzl.cpp"
        pzzl_read_req = DVD_READ_N(wk->filename, wk->pSwitchDat, 0, 0, 0, 0x10);
        if (pzzl_read_req <= 0) {
            break;
        }
        if (wk->menu_old == 2 && wk->open_flag != 4) {
            sscrnModelFree(wk);
            generalModelAlloc(wk);
            playerModelInit();
            sscrnLightClear(wk);
            {
                LifeMeter* life = &Cckpt.m_LifeMeter;

                life->fix(1);
                life->frameIn();
            }
        } else {
            sscrnModelClear(wk);
        }
        wk->wait_cnt = 0;
        state++;
    case 3: {
        int result;
        int size;

        if (Dvd.ReadCheck(pzzl_read_req, &result, &size, 0) != 1) {
            break;
        }
        wk->pPzzlDat = wk->pSwitchDat;
        state++;
    }
    case 4:
        if (wk->open_flag == 4) {
            FadeSetW(0x80000000, 5, 0, 0);
        }
        transit(0, wk);
        break;
    }
}

// Shows (1) or fades out (0) the temporary space board frame (IdSub 0/0x10).
void tempSpaceDisp(int sw)
{
    IdUnit* u = IdSub.unitPtr(0, IDC_SSCRN_NEAR_0);

    switch (sw) {
    case 1:
        u->be_flag |= 8;
        u->rev_flag &= 0xF0;
        break;
    case 0:
        u->rev_flag |= 0xF;
        break;
    }
}

// 1 when the equipped weapon changed (or ran dry: unarmed).
int checkWeaponChange(int id, int bullets)
{
    if (id != ItemMgr.m_wep_id) {
        return 1;
    }
    if (bullets && ItemMgr.bulletNum() == 0) {
        ItemMgr.arm(0);
        return 1;
    }
    return 0;
}

// Builds the case screen: widgets PzzlThinking -> PieceSelect -> PieceCommand -> PieceCombine /
// SsItemExamine, CaseChange; the id groups (case 0x10, command menus 0x1C/0x1D, name 0x1E, digits
// IdNum 0x40..), the pzlPlayer of board_size; on a pick-up open (get_item_id) the new item is
// appended as the extra piece and put in hand; the debug editor; caseMove = first frame.
void SsPzzlMain::init(SUB_SCREEN* wk)
{
    IdUnit* tbl[16];
    s8 num;
    int lang;
    int i;
    int j;

    thinking = new PzzlThinking;
    popUp = new PiecePopUp;
    popDown = new PiecePopDown;
    select = new PieceSelect;
    combine = new PieceCombine;
    command = new PieceCommand;
    exam = new SsItemExamine;
    caseChange = new CaseChange;
    thinking->connect(0, select);
    select->connect(0, command);
    select->connect(1, thinking);
    select->connect(2, caseChange);
    command->connect(0, select);
    command->connect(1, combine);
    command->connect(2, exam);
    combine->connect(0, select);
    combine->connect(1, command);
    exam->connect(0, select);
    caseChange->connect(0, select);
    puzzleCameraInit(wk, &pG->Camera);
    IdTexDataLoad(SS_ARC_PTR(wk->pPzzlDat, 0x1AA), TEX_OWNER_ID_SSCRN);
    if (!IdSub.setCk(IDC_SSCRN_0)) {
        IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_0, 0xC, 6, 0);
    }
    IdSub.set(SS_ARC_PTR(wk->pPzzlDat, 0x1AB), 0xFF, IDC_SSCRN_NEAR_0, 0xF, 0, 0);
    tempSpaceDisp(0);
    idMainMenuFade(wk, 1);
    for (i = 0; i < 0x3E; i++) {
        if (i == 0) {
            IdNum.set(SS_ARC_PTR(wk->pCmmn, 8), 0xFF, IDC_NUM_00, 0x13, 8, 0);
        } else {
            IdNum.set(SS_ARC_PTR(wk->pCmmn, 8), 0xFF, 0x40 + i, 0x13, 9, 0);
        }
    }
    IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xD), 0xFF, IDC_SSCRN_CKPT_0, 0x13, 2, 0);
    IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xE), 0xFF, IDC_SSCRN_CKPT_1, 0x13, 2, 0);
    for (lang = 0; lang < 2; lang++) {
        u8 type;

        // block-scoped counters per loop (the 0x3E counter r31, this one r30, the two 11-loops r29 with
        // the `tbl[k]` giv in r31); `no = k + 1` after the call is the itemSelect `i = no` idiom
        // (`addi r3,r30,1` after `bl setCommandId`, `mr r30,r3` at the latch).
        for (int k = 0; k < 10;) {
            setCommandId(k, tbl, &num, lang);
            int no = k + 1;
            for (j = 0; j < num * 2 + 6; j++) {
                tbl[j]->rev_flag |= 0xF;
                tbl[j]->be_flag &= ~8;
            }
            k = no;
        }
        type = lang == 0 ? 0x1C : 0x1D;
        // `(u8) k`: with a plain int `0x80 + k` combine narrows the plus under the u8 truncation and
        // emits `addi -128`; the target keeps `addi 4,29,128; clrlwi`.
        for (int k = 0; k < 11; k++) {
            tbl[k] = IdSub.unitPtr(0x70 + (u8) k, type);
            tbl[k]->be_flag &= ~8;
            tbl[k]->rev_flag |= 0xF;
        }
        for (int k = 0; k < 11; k++) {
            tbl[k] = IdSub.unitPtr(0x80 + (u8) k, type);
            tbl[k]->be_flag &= ~8;
            tbl[k]->rev_flag |= 0xF;
        }
    }
    IdSub.set(SS_ARC_PTR(wk->pCmmn, 0x10), 0xFF, IDC_SSCRN_CKPT_2, 0x13, 1, 0);
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x12));
    if (wk->menu_old == 2 && wk->open_flag != 4) {
        wk->alpha_flag = 0;
        wk->alpha_cnt = 10;
    }
    wk->puzzlePlayer = new pzlPlayer;
    if (!wk->puzzlePlayer->init((s8) wk->board_size)) {
        delete wk->puzzlePlayer;
    }
    pieceModelInit(wk);
    if (wk->open_flag & 4) {
        pzlPlayer* pl;
        pzlPiece* p;
        int w;
        int h;
        int x;
        int y;

        ItemMgr.get(wk->get_item_id, wk->get_item_num);
        ItemWork* last = ItemMgr.pLast;  // local: `mr r4,r0` for the argument instead of a re-read after the store

        wk->p_get_item = last;
        wk->puzzlePlayer->appendExtraPiece(last);
        wk->puzzlePlayer->inHandExtraPiece();
        wk->get_item_num = wk->p_get_item->num;
        pl = wk->puzzlePlayer;
        p = pl->m_extra;
        h = pl->m_space->m_size_y;
        w = pl->m_space->m_size_x;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                p->set_ver0_x((f32) x);
                p->set_ver0_y((f32) y);
                if (pl->putPiece(pl->m_space)) {
                    goto PUT;
                }
            }
        }
        p->orientation(1);
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                p->set_ver0_x((f32) x);
                p->set_ver0_y((f32) y);
                if (pl->putPiece(pl->m_space)) {
                    goto PUT;
                }
            }
        }
    PUT:
        pl->m_p_active_board = pl->m_space;
        pl->getPiece(pl->m_space);
        pieceModelSet(wk->puzzlePlayer->m_extra);
        cur = thinking;
        cur->init(wk);
    } else {
        if ((wk->flags & 4) && wk->puzzlePlayer->m_space->getPieceNum() != 0) {
            wk->puzzlePlayer->m_p_active_board = wk->puzzlePlayer->m_space;
            thinking->init(wk);
        }
        cur = select;
    }
    sscrnMainMenuInit(wk, 0);
    state = 0;
    caseMove = 1;
    sscrn_pzzl_in_init(wk);
    // setPtr(0) / setPtr(2): `stwx r0,r11,rZERO` with the `state = 0` zero pseudo (r30, live across the
    // sscrn_pzzl_in_init call) as the index, then `stw 8(r11)`.
    MesData.setPtr(0, (u8*) SS_ARC_PTR(wk->pCmmn, 5));
    MesData.setPtr(2, (u8*) SS_ARC_PTR(wk->pPzzlDat, 0x1A4));
    pzzl_dbg.init(wk);
    SndCall(0, 0x1E, 0, 0, 0, 0);
}

// Case screen frame: moves the case model in, runs the child widget (PieceSelect mode 1 exit via
// link 4, 2 main menu; L (Key bit 22) jumps to the key items screen), or the tab row (state 1:
// 0 items, 1 back, 2 map, 3 files, 4 exit). Re-equips through weaponChangeRequest when the armed
// weapon changed (checkWeaponChange). Z / B on pad 1 toggle the CASE MAKE debug editor.
void SsPzzlMain::move(SUB_SCREEN* wk)
{
    int old;
    int bullets;
    u16 armId;

    caseModelMove(caseMove);
    pzzlClearZ(wk);
    pieceModelDisp(wk);
    pzzlCursorDisp(wk, 1);
    if (cur != exam) {
        IdUnit* u = IdSub.unitPtr(1, IDC_SSCRN_CKPT_2);
        int id = 0;
        int on = 0;
        int x;
        int y;
        pzlPlayer* pl = wk->puzzlePlayer;
        ItemWork* item;

        if (pl->m_inhand) {
            item = pl->m_inhand->item;
            on = 1;
            id = item->id;
        } else if (pl->ptrPiece(pl->m_p_active_board)) {
            on = 1;
            item = wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board)->item;
            id = item->id;
        }
        x = (int) ((u->pos.x + 320.0f) * 0.8f);
        y = (int) ((240.0f - u->pos.y) * 0.8f);
        y -= cMes.getMes(0)->m_font_h / 2;
        if (on && caseMove == 0) {
            cMes.setFontSize(0, pzzl_font_w[1], pzzl_font_h[1]);
            cMes.getMes(0)->m_line_gap = 0;
            cMes.getMes(0)->m_char_gap = pzzl_font_space[3];
            cMes.MesSet(id, x, y, 0x20088, 0, 0, 4);
        } else {
            cMes.Delete(0);
        }
    }
    armId = ItemMgr.m_wep_id;
    bullets = ItemMgr.bulletNum();
    old = state;
    wk->cursor_mode = 0;
    msg_open = 0;
    switch (state) {
    case 0:
        if (wk->pzzl_debug_open == 0) {
            Widget<SUB_SCREEN>* w = cur;

            w->move(wk);
            next = w->cur;
            wk->puzzlePlayer->save();
            if (cur == select) {
                switch (select->mode) {
                case 2:
                    state = 1;
                    wk->close_flag |= 1;
                    sscrnMainMenuInit(wk, 1);
                    break;
                case 1:
                    wk->close_flag |= 1;
                    transit(4, wk);
                    break;
                default:
                    if (wk->open_flag != 4 && next == cur && wk->puzzlePlayer->m_space->getPieceNum() == 0 &&
                        (Key.trg & 0x00400000)) {
                        wk->close_flag = 0;
                        wk->menu_old = 1;
                        transit(0, wk);
                    }
                    break;
                }
            }
        }
        cur = next;
        break;
    case 1:
        if (wk->pzzl_debug_open == 0) {
            if (sscrnMainMenu(wk)) {
                switch ((s8) wk->menu_no) {
                case 1:
                    select->mode = 0;
                    sscrnMainMenuInit(wk, 0);
                    state = 0;
                    SndCall(0, 6, 0, 0, 0, 0);
                    break;
                case 0:
                    transit(0, wk);
                    break;
                case 3:
                    transit(3, wk);
                    break;
                case 2:
                    transit(2, wk);
                    break;
                case 4:
                    transit(4, wk);
                    break;
                }
            }
            if (Key.trg & 0x03000000) {
                select->mode = 0;
                sscrnMainMenuInit(wk, 0);
                state = 0;
                wk->puzzlePlayer->m_p_active_board = wk->puzzlePlayer->m_board;
                if (Key.trg & 0x01000000) {
                    int h = wk->puzzlePlayer->m_board->m_size_y;

                    wk->puzzlePlayer->m_board->m_cur_y = h - 1;
                } else {
                    wk->puzzlePlayer->m_board->m_cur_y = 0;
                }
                SndCall(0, 6, 0, 0, 0, 0);
            }
        }
        break;
    }
    if (checkWeaponChange(armId, bullets)) {
        // COMPILER-DIFF: 4 (the u8 results assigned to u16 locals are masked with `clrlwi 16`)
        u16 no = WeaponId2WeaponNo(ItemMgr.m_wep_id);
        u16 type = WeaponId2WeaponType(ItemMgr.m_wep_id);

        weaponChangeRequest(no, type);
    }
    if (old != state) {
        wk->cursor_flag |= 2;
    }
    if (wk->pzzl_debug_open) {
        pzzl_dbg.move(wk);
        if (Joy[0].trg & 0x200) {
            wk->pzzl_debug_open = wk->pzzl_debug_open == 0;
            if (wk->pzzl_debug_open) {
                pG->debug_mode = 1;
            } else {
                pG->debug_mode = *((u8*) &wk->debugMode + 3);
            }
        }
    }
    if (wk->puzzlePlayer->m_inhand == 0 && (Joy[0].trg & 0x10)) {
        if (!SysFlagChk(pG, SYS_PUBLICITY_VER) || PadCheckStatus(&Joy[1]) == 1) {
            wk->pzzl_debug_open = wk->pzzl_debug_open == 0;
        }
        if (wk->pzzl_debug_open) {
            pG->debug_mode = 1;
        } else {
            pG->debug_mode = *((u8*) &wk->debugMode + 3);
        }
    }
    if (caseMove) {
        caseMove = 0;
    }
}

// Leaving the case: on a pick-up open an extra piece still in the space is discarded (dumpAll,
// unarmed if equipped; model_flag 0), otherwise the layout is saved (pzlPlayer::save, model_flag
// 1). Deletes the widgets and player, installs sscrn_pzzl_out.
void SsPzzlMain::quit(SUB_SCREEN* wk)
{
    pzzlCursorDisp(wk, 0);
    pzzlEquipDisp(wk, 0);
    if (wk->open_flag & 4) {
        pzlPlayer* pl = wk->puzzlePlayer;

        if (pl->m_extra && pl->m_space->search(pl->m_extra)) {
            wk->puzzlePlayer->removeExtraPiece();
            ItemMgr.dumpAll(wk->p_get_item);
            if (wk->p_get_item == ItemMgr.pArm) {
                ItemMgr.arm(0);
            }
            // x300 first: with x40 first the arm's tail is the else arm's `stw x40` insn, which our
            // jump2 cross-jumps as a single-insn tail (COMPILER-DIFF: 6)
            wk->p_get_item = 0;
            wk->model_flag = 0;
        } else {
            wk->puzzlePlayer->save();
            wk->model_flag = 1;
        }
    }
    pzzl_dbg.quit(wk);
    ssWidgetDelete(thinking);
    ssWidgetDelete(popUp);
    ssWidgetDelete(popDown);
    ssWidgetDelete(select);
    ssWidgetDelete(combine);
    ssWidgetDelete(command);
    ssWidgetDelete(exam);
    ssWidgetDelete(caseChange);
    wk->puzzlePlayer->quit();
    delete wk->puzzlePlayer;
    sscrn_pzzl_out_init(wk);
    wk->scrn_out_func = sscrn_pzzl_out;
}

// Starts the case close animation (IdSub 0xFE/0xFD of group 1 reversed, name unit fade); going to
// the map frames the life meter out and fades the character.
void sscrn_pzzl_out_init(SUB_SCREEN* wk)
{
    IdUnit* u;

    u = IdSub.unitPtr(0xFE, IDC_SSCRN_BACK_GROUND);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0xFD, IDC_SSCRN_BACK_GROUND);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(1, IDC_SSCRN_CKPT_2);
    u->rev_flag |= 1;
    if (wk->menu_next == 2) {
        Cckpt.m_LifeMeter.frameOut();
        wk->alpha_flag = 1;
    }
}

// Exit routine (scrn_out_func): keeps drawing the case, pieces and cursor; 1 once both case units
// finished their animation.
static int sscrn_pzzl_out(SUB_SCREEN* wk)
{
    IdUnit* u;
    IdUnit* u2;

    caseModelMove(0);
    pzzlClearZ(wk);
    pieceModelDisp(wk);
    pzzlCursorDisp(wk, 1);
    u = IdSub.unitPtr(0xFE, IDC_SSCRN_BACK_GROUND);
    u2 = IdSub.unitPtr(0xFD, IDC_SSCRN_BACK_GROUND);
    if ((u->anima_state & 1) && (u2->anima_state & 4)) {
        return 1;
    }
    return 0;
}

// Plays the case units' open animation forward (CaseChange / shop re-entry).
void sscrn_pzzl_in_init(SUB_SCREEN* wk)
{
    IdUnit* u;

    u = IdSub.unitPtr(0xFE, IDC_SSCRN_BACK_GROUND);
    u->rev_flag &= 0xF0;
    u = IdSub.unitPtr(0xFD, IDC_SSCRN_BACK_GROUND);
    u->rev_flag &= 0xF0;
}

// One frame delay widget (pick-up animation placeholder) then back to the linked widget.
void PiecePopUp::move(SUB_SCREEN* wk)
{
    if (count++ > 0) {
        transit(0, wk);
    }
}

// One frame delay widget then back to the linked widget.
void PiecePopDown::move(SUB_SCREEN* wk)
{
    if (count++ > 0) {
        transit(0, wk);
    }
}

// Piece-in-hand mode open: frames the life meter out, hides the case header, shows the space board
// and dims the main menu.
void PzzlThinking::init(SUB_SCREEN* wk)
{
    Cckpt.m_LifeMeter.frameOut();
    IdSub.unitPtr(0, IDC_SSCRN_PESETA)->rev_flag |= 0xF;
    tempSpaceDisp(1);
    idMainMenuFade(wk, 0);
}

// Piece in hand: A/X put it down (pzlPlayer::putPiece; on the space board the sound depends on the
// item type), B puts it back where it came from, otherwise movePiece (d-pad / rotate) with the move
// and blocked sounds. Back to PieceSelect once the hand is empty.
void PzzlThinking::move(SUB_SCREEN* wk)
{
    pzlPlayer* pl;

    if (Key.trg & 0x80020000) {
        if (wk->puzzlePlayer->putPiece(wk->puzzlePlayer->m_p_active_board)) {
            transit(0, wk);
            SndCall(0, 0xC, 0, 0, 0, 0);
        } else {
            pzlPiece* p = wk->puzzlePlayer->cmbPiece(wk->puzzlePlayer->m_p_active_board);

            if (p) {
                ItemInfo info;

                pieceModelSet(p);
                pieceModelOrientation(wk, p);
                wk->puzzlePlayer->rehash();
                if (wk->puzzlePlayer->m_inhand == 0) {
                    transit(0, wk);
                }
                itemInfo(p->item->id, &info);
                {
                    // one SndCall after a `switch (type)` value select, bodies laid out 6, 2, default
                    // (the PieceCombine::move recipe): the shared tail starts at `li r3,0`.
                    int se;

                    switch (info.type) {
                    case 6:
                        se = 0x27;
                        break;
                    case 2:
                        se = 0x29;
                        break;
                    default:
                        se = 0x28;
                        break;
                    }
                    SndCall(0, se, 0, 0, 0, 0);
                }
            } else if (wk->puzzlePlayer->chgPiece(wk->puzzlePlayer->m_p_active_board)) {
                SndCall(0, 0xD, 0, 0, 0, 0);
            }
        }
    } else if (Key.trg & 0x40000000) {
        if (wk->puzzlePlayer->relPiece(wk->puzzlePlayer->m_p_active_board)) {
            transit(0, wk);
            SndCall(0, 0xC, 0, 0, 0, 0);
        }
    } else {
        switch (wk->puzzlePlayer->movePiece()) {
        case 1:
            SndCall(0, 0xA, 0, 0, 0, 0);
            break;
        case 2:
            SndCall(0, 0xB, 0, 0, 0, 0);
            break;
        }
    }
}

// Restores the case header / life meter when the space board is empty.
void PzzlThinking::quit(SUB_SCREEN* wk)
{
    back2PieceSelect(wk);
}

// 1 when the screen may close: nothing (or only the extra piece) on the space board.
int isTerminable(SUB_SCREEN* wk)
{
    pzlPlayer* pl = wk->puzzlePlayer;
    int n = pl->m_space->getPieceNum();
    int ret;

    if (n != 0) {
        if (n == 1 && pl->m_space->search(pl->m_extra)) {
            ret = 1;
        } else {
            ret = 0;
        }
    } else {
        ret = 1;
    }
    return ret;
}

// 1 while the yes/no message window (cMes flag bit 1) is open.
int checkMsgWindow(SUB_SCREEN* wk)
{
    if (CMES_FLAGS & 2) {
        return 1;
    }
    return 0;
}

// Kills the message window frame ids (group 3) and message slot 1.
void closeMsgWindow(SUB_SCREEN* wk)
{
    IdSub.kill(0xFF, IDC_SSCRN_CONFIRM);
    cMes.Delete(1);
}

// Opens message `no` in slot 1 with the sub screen layout and the window frame ids (pCmmn 0xA):
// the "space not empty" (0), combine remark and discard prompts.
void openMsgWindow(SUB_SCREEN* wk, int no)
{
    msg_open = 1;
    cMes.Delete(1);
    cMes.Delete(2);
    cMes.setLayout(1, LAYOUT_SUBSCRN);
    cMes.MesSet(no, msg_x, msg_y, 0x11, 1, 0, 3);
    IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xA), 0xFF, IDC_SSCRN_CONFIRM, 0x13, 0, 0);
}

// Case cursor. A pending case size change (board_size != board_next) transits to CaseChange
// first. state 0: Y (mode 1) leaves; B on a pick-up open leaves too, otherwise goes up to the main
// menu (mode 2) or, in the shop (no link 3), returns to the shop (mode 4); pieces left on the space
// board instead open the "items left" message (mode | 8, state 1). A on a piece opens PieceCommand
// (pzzl_sel), X picks it up (PzzlThinking); d-pad moves the cursor between the case and space
// boards. state 1 waits for the message answer (yes: continue, no: back), 2 closes the window.
void PieceSelect::move(SUB_SCREEN* wk)
{
    pzlBoard* b;
    pzlBoard* other;
    pzlBoard* space;

    // x267 store first, then a block-local `pl` for the three board loads only (r11, dies at
    // `space`); every later statement re-reads wk->puzzlePlayer (the target reloads it per call). The
    // if/else for `other` gives the hoisted else-set `mr r26,r0` copy. `st` (the state load,
    // r27) is the zero register of the r==2 arm's x264/x265 stores (cse's zero class on the path
    // from `beq CASE0`); the r==1 arm's zero is the getPieceNum result `mr. r9,r3` (see `n`).
    wk->cursor_mode = 1;
    {
        pzlPlayer* pl = wk->puzzlePlayer;
        b = pl->m_p_active_board;
        if (b == pl->m_board) {
            other = pl->m_space;
        } else {
            other = pl->m_board;
        }
        space = pl->m_space;
    }
    if (wk->board_size != wk->board_next) {
        transit(2, wk);
        return;
    }
    int st = state;
    switch (st) {
    case 0:
        mode = st;
        if (Key.trg & 0x00100000) {
            mode = 1;
            if (!isTerminable(wk)) {
                mode |= 8;
                openMsgWindow(wk, 0);
                state = 1;
                SndCall(0, 0x2A, 0, 0, 0, 0);
            }
        } else if (Key.trg & 0x40000000) {
            if (wk->open_flag & 4) {
                mode = 1;
                if (!isTerminable(wk)) {
                    mode |= 8;
                    openMsgWindow(wk, 0);
                    state = 1;
                    SndCall(0, 0x2A, 0, 0, 0, 0);
                } else {
                    Cckpt.m_LifeMeter.frameIn();
                    IdSub.unitPtr(0, IDC_SSCRN_PESETA)->rev_flag &= 0xF0;
                }
            } else if (link[3] == 0) {
                mode = 2;
                if (!isTerminable(wk)) {
                    mode |= 8;
                    openMsgWindow(wk, 0);
                    state = 1;
                    SndCall(0, 0x2A, 0, 0, 0, 0);
                } else {
                    wk->menu_next = 4;
                    wk->menu_no = 4;
                    SndCall(0, 0xA, 0, 0, 0, 0);
                }
            } else {
                mode = 4;
                if (!isTerminable(wk)) {
                    mode |= 8;
                    openMsgWindow(wk, 0);
                    state = 1;
                    SndCall(0, 9, 0, 0, 0, 0);
                } else {
                    transit(3, wk);
                }
            }
        } else if (Key.trg & 0x80000000) {
            if (wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board) && link[0]) {
                pzlPiece** psel = &pzzl_sel;

                *psel = wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board);
                transit(0, wk);
            }
        } else if (Key.trg & 0x00020000) {
            if (wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board)) {
                wk->puzzlePlayer->getPiece(wk->puzzlePlayer->m_p_active_board);
                transit(1, wk);
                SndCall(0, 0xD, 0, 0, 0, 0);
            }
        } else {
            int r = wk->puzzlePlayer->selPiece(wk->puzzlePlayer->m_p_active_board);
            int n;

            if (r == 5) {
                wk->cursor_flag |= 1;
                SndCall(0, 6, 0, 0, 0, 0);
            }
            switch (r) {
            case 1:
                if (link[3] == 0 && (n = space->getPieceNum()) == 0 && wk->open_flag != 4) {
                    mode = 2;
                    wk->menu_next = 0;
                    wk->menu_no = 0;
                    SndCall(0, 0xA, 0, 0, 0, 0);
                } else {
                    int h = b->m_size_y;

                    b->m_cur_y = h - 1;
                    SndCall(0, 6, 0, 0, 0, 0);
                }
                break;
            case 2:
                if (link[3] == 0 && space->getPieceNum() == 0 && wk->open_flag != 4) {
                    mode = r;
                    wk->menu_next = st;
                    wk->menu_no = st;
                    SndCall(0, 0xA, 0, 0, 0, 0);
                } else {
                    b->m_cur_y = 0;
                    SndCall(0, 6, 0, 0, 0, 0);
                }
                break;
            case 3:
                if (other->getPieceNum() != 0) {
                    int w;

                    wk->puzzlePlayer->m_p_active_board = other;
                    b = wk->puzzlePlayer->m_p_active_board;
                    w = b->m_size_x;
                    b->m_cur_x = w - 1;
                } else {
                    int w = b->m_size_x;

                    b->m_cur_x = w - 1;
                }
                SndCall(0, 6, 0, 0, 0, 0);
                break;
            case 4:
                if (other->getPieceNum() != 0) {
                    wk->puzzlePlayer->m_p_active_board = other;
                    b = wk->puzzlePlayer->m_p_active_board;
                    b->m_cur_x = 0;
                } else {
                    b->m_cur_x = 0;
                }
                SndCall(0, 6, 0, 0, 0, 0);
                // COMPILER-DIFF: candidate #12 (cse2 qty order). Dead store (flow1 deletes it)
                // that moves REGNO_LAST_UID of the r==1 arm's getPieceNum result `n` past case 4's
                // Key-zero store, so cse2's make_regs_eqv keeps `n` (not the Key `or.` result) as
                // the first register of the zero quantity in the r==1 arm: `mr. r9,r3; stb r9`.
                space = (pzlBoard*) n;
                break;
            }
        }
        break;
    case 1: {
        int r;

        msg_open = st;
        r = CMES_RESULT;
        if (r == 0) {
            break;
        }
        closeMsgWindow(wk);
        if (r == 1) {
            if (wk->open_flag & 4) {
                if (wk->puzzlePlayer->m_extra) {
                    ItemMgr.offboardDump(wk->p_get_item);
                } else {
                    ItemMgr.offboardDump(0);
                }
            } else {
                ItemMgr.offboardDump(0);
            }
            wk->puzzlePlayer->rehash();
            // COMPILER-DIFF: candidate (global-alloc priority, dead test). Extends `st`'s live
            // length below `b`'s priority (b r28, st r27) while this/wk (both live here) keep
            // their order; the compare goes in jump2, the store in flow1.
            if (st == r) {
                b = 0;
            }
            back2PieceSelect(wk);
            state = 2;
            SndCall(0, 0xD, 0, 0, 0, 0);
        } else if (r == 2) {
            state = 0;
            SndCall(0, 5, 0, 0, 0, 0);
        }
        break;
    }
    case 2:
        mode &= ~8;
        switch ((u32) mode) {
        case 1:
            break;
        case 2:
            wk->menu_next = 4;
            wk->menu_no = 4;
            break;
        case 4:
            transit(3, wk);
            break;
        }
        state = 0;
        break;
    }
}

// Message number for a failed combine of the two item ids (0 when there is none).
int remarkMsgCombine(int a, int b, int* no)
{
    int i;

    for (i = 0; i < 2; i++) {
        switch (i == 0 ? a : b) {
        case 0x12:
            *no = 0x21;
            return 1;
        case 0x14:
            *no = 0x22;
            return 1;
        case 0x16:
            *no = 0x23;
            return 1;
        case 0xA8:
            *no = 0x24;
            return 1;
        }
    }
    return 0;
}

// Combine target selection start.
void PieceCombine::init(SUB_SCREEN* wk)
{
    state = 0;
}

// Combine mode: B back to the command menu, A combines the piece under the cursor with pzzl_sel
// (ItemMgr.combine; success rebuilds the pieces, a remark message may follow), error sound
// otherwise; d-pad moves the cursor (state 1 waits for the remark message).
void PieceCombine::move(SUB_SCREEN* wk)
{
    pzlPlayer* pl = wk->puzzlePlayer;
    pzlBoard* b;
    pzlBoard* other;

    wk->cursor_mode = 2;
    b = pl->m_p_active_board;
    if (b == pl->m_board) {
        other = pl->m_space;
    } else {
        other = pl->m_board;
    }
    switch (state) {
    case 0:
        if (Key.trg & 0x40000000) {
            pzlPiece** psel = &pzzl_sel;
            wk->puzzlePlayer->loadCursor();
            *psel = wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board);
            transit(1, wk);
        } else if (Key.trg & 0x80000000) {
            if (wk->puzzlePlayer->ptrPiece(b)) {
                pzlPiece* p = wk->puzzlePlayer->ptrPiece(wk->puzzlePlayer->m_p_active_board);
                pzlPiece* sel = pzzl_sel;
                int extra = p == wk->puzzlePlayer->m_extra;
                u16 idA;
                u16 idB;

                if (sel == wk->puzzlePlayer->m_extra) {
                    extra = 1;
                }
                idA = p->item->id;
                idB = sel->item->id;
                if (p != sel) {
                    if (ItemMgr.combine(p->item, sel->item, 0)) {
                        ItemInfo info;

                        if (extra == 1) {
                            wk->puzzlePlayer->giveupExtraPiece();
                        }
                        if (idA != p->item->id) {
                            pieceModelSet(p);
                            pieceModelOrientation(wk, p);
                        }
                        if (idB != pzzl_sel->item->id) {
                            pieceModelSet(pzzl_sel);
                            pieceModelOrientation(wk, pzzl_sel);
                        }
                        wk->puzzlePlayer->rehash();
                        transit(0, wk);
                        itemInfo(idB, &info);
                        {
                            int se;
                            switch (info.type) {
                            case 6:
                                se = 0x27;
                                break;
                            case 2:
                                se = 0x29;
                                break;
                            default:
                                se = 0x28;
                                break;
                            }
                            SndCall(0, se, 0, 0, 0, 0);
                        }
                    } else {
                        int no;

                        if (remarkMsgCombine(p->item->id, pzzl_sel->item->id, &no)) {
                            openMsgWindow(wk, no);
                            state = 1;
                        }
                        SndCall(0, 7, 0, 0, 0, 0);
                    }
                } else {
                    SndCall(0, 7, 0, 0, 0, 0);
                }
            } else {
                SndCall(0, 7, 0, 0, 0, 0);
            }
        } else {
            int r;
            // The do-while(0) counts the `b` argument ref at loop depth 1: `b` outranks `wk` in
            // global-alloc priority (b r31, wk r29); without it wk takes r31.
            do {
                r = wk->puzzlePlayer->selPiece(b);
            } while (0);

            if (r == 5) {
                wk->cursor_flag |= 1;
                SndCall(0, 6, 0, 0, 0, 0);
            }
            switch (r) {
            case 1: {
                int h = b->m_size_y;
                b->m_cur_y = h - 1;
                break;
            }
            case 2:
                b->m_cur_y = 0;
                break;
            case 3: {
                int w;
                if (other->getPieceNum() != 0) {
                    wk->puzzlePlayer->m_p_active_board = other;
                    b = wk->puzzlePlayer->m_p_active_board;
                }
                w = b->m_size_x;
                b->m_cur_x = w - 1;
                break;
            }
            // Dead loop before the case label: the LOOP_END note ends cse's path, so case 4's
            // `curX = 0` gets a fresh `li 0,0` instead of the Key.trg `or` result known to be 0.
            do {
            } while (0);
            case 4:
                if (other->getPieceNum() != 0) {
                    wk->puzzlePlayer->m_p_active_board = other;
                    b = wk->puzzlePlayer->m_p_active_board;
                }
                b->m_cur_x = 0;
                break;
            }
        }
        break;
    case 1:
        if (checkMsgWindow(wk)) {
            closeMsgWindow(wk);
            state = 0;
        }
        break;
    }
}

// Nothing to release.
void PieceCombine::quit(SUB_SCREEN* wk)
{
    back2PieceSelect(wk);
}

// Command menu open for pzzl_sel: picks the id set by itemCommandType and board (case / space),
// anchors it at the piece's cursor corner (below or above the piece by `lower`), cursor on the
// first command.
void PieceCommand::init(SUB_SCREEN* wk)
{
    u8 type = itemCommandType(pzzl_sel->item);
    pzlPlayer* pl = wk->puzzlePlayer;
    Vec pos;
    Vec half;
    Vec pc;
    int i;
    int corner;

    half.x = (f32) pl->m_p_active_board->m_size_x * 0.5f;
    half.y = (f32) pl->m_p_active_board->m_size_y * 0.5f;
    pc.x = pzzl_sel->m_pos_x;
    pc.y = pzzl_sel->m_pos_y;
    if (pl->m_board->search(pzzl_sel)) {
        inSpace = 0;
    } else {
        inSpace = 1;
    }
    if (pc.y > half.y - 0.5f) {
        lower = 1;
    } else {
        lower = 0;
    }
    setCommandId(type, id, &num, inSpace);
    for (i = 0; i < num * 2 + 6; i++) {
        id[i]->rev_flag &= 0xF0;
        id[i]->be_flag |= 8;
    }
    if (inSpace) {
        corner = lower ? 3 : 0;
    } else {
        corner = lower ? 2 : 1;
    }
    pos = pzzl_cursor.v[corner];
    puzzlePos2screenPos(&pos, &id[0]->pos0);
    if (lower) {
        id[0]->pos0.y += id[2]->size_H;
    }
    cursorOld = 1;
    mode = 0;
    wk->cmd_menu_no = 0;
    SndCall(0, 4, 0, 0, 0, 0);
}

// Command menu: mode 0 B cancels, A runs the command (per type row -> command_id: 0 equip (blocked
// in events: message 0x27), 1 reload, 2 combine -> PieceCombine, 3 detach the weapon part, 4 use
// (with Ashley present: the "who" sub menu), 5 examine -> SsItemExamine, 6 discard -> yes/no), up/
// down move the cursor; mode 1/2 the sub menu (yes/no or Leon/Ashley), mode 3 waits for a message.
void PieceCommand::move(SUB_SCREEN* wk)
{
    Vec pos;
    int corner;

    wk->cursor_mode = 1;
    if (inSpace) {
        corner = lower ? 3 : 0;
    } else {
        corner = lower ? 2 : 1;
    }
    pos = pzzl_cursor.v[corner];
    puzzlePos2screenPos(&pos, &id[0]->pos0);
    if (lower) {
        id[0]->pos0.y += id[2]->size_H;
    }
    switch (mode) {
    case 0:
        if (Key.trg & 0x40000000) {
            for (int i = 0; i < num * 2 + 6; i++) {
                id[i]->rev_flag |= 0xF;
            }
            transit(0, wk);
            SndCall(0, 5, 0, 0, 0, 0);
            break;
        }
        if (Key.trg & 0x80000000) {
            int used = 0;
            int type = itemCommandType(pzzl_sel->item);

            switch (type) {
            case 0:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = used;
                    break;
                case 1:
                    command_id = 5;
                    break;
                case 2:
                    command_id = 6;
                    break;
                }
                break;
            case 1:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = used;
                    break;
                case 1:
                    command_id = type;
                    break;
                case 2:
                    command_id = 5;
                    break;
                case 3:
                    command_id = 6;
                    break;
                }
                break;
            case 2:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = used;
                    break;
                case 1:
                    command_id = 1;  // reload (the target stores the switch register: `beq` straight to the shared `lis; stw`)
                    break;
                case 2:
                    command_id = type;
                    break;
                case 3:
                    command_id = 5;
                    break;
                case 4:
                    command_id = 6;
                    break;
                }
                break;
            case 3:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = 2;
                    break;
                case 1:
                    command_id = 5;
                    break;
                case 2:
                    command_id = 6;
                    break;
                }
                break;
            case 6:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = 3;
                    break;
                case 1:
                    command_id = 5;
                    break;
                case 2:
                    command_id = type;
                    break;
                }
                break;
            case 4:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = type;
                    break;
                case 1:
                    command_id = 5;
                    break;
                case 2:
                    command_id = 6;
                    break;
                }
                break;
            case 7:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = 5;
                    break;
                case 1:
                    command_id = 6;
                    break;
                }
                break;
            case 8:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = 4;
                    break;
                case 1:
                    command_id = used;
                    break;
                case 2:
                    command_id = 5;
                    break;
                case 3:
                    command_id = 6;
                    break;
                }
                break;
            case 5:
            default:
                switch (wk->cmd_menu_no) {
                case 0:
                    command_id = 4;
                    break;
                case 1:
                    command_id = 2;
                    break;
                case 2:
                    command_id = 5;
                    break;
                case 3:
                    command_id = 6;
                    break;
                }
                break;
            case 9:
                if (wk->cmd_menu_no == 0) {
                    command_id = 5;
                }
                break;
            }
            if (command_id == 2 || command_id == 5) {
                for (int i = 0; i < num * 2 + 6; i++) {
                    id[i]->rev_flag |= 0xF;
                }
                wk->puzzlePlayer->saveCursor();
            }
            switch (command_id) {
            case 0:
                if (pG->pl_type == 1) {
                    break;
                }
                if (wk->flags & 2) {
                    for (int i = 0; i < num * 2 + 6; i++) {
                        id[i]->rev_flag |= 0xF;
                    }
                    mode = 3;
                    openMsgWindow(wk, 0x27);
                } else {
                    used = ItemMgr.arm(pzzl_sel->item);
                    if (used) {
                        wk->puzzlePlayer->rehash();
                        for (int i = 0; i < num * 2 + 6; i++) {
                            id[i]->rev_flag |= 0xF;
                        }
                    }
                }
                break;
            case 1: {
                int extraNum = 0;
                pzlPiece* extra = 0;

                if (wk->puzzlePlayer->m_extra) {
                    extra = wk->puzzlePlayer->m_extra;
                    extraNum = extra->item->num;
                }
                used = ItemMgr.reload(pzzl_sel->item, 1);
                if (wk->puzzlePlayer->m_extra && extraNum != extra->item->num) {
                    wk->puzzlePlayer->giveupExtraPiece();
                }
                break;
            }
            case 2:
                transit(1, wk);
                SndCall(0, 9, 0, 0, 0, 0);
                return;
            case 3: {
                ItemInfo info;

                pzzl_sel->item->lv = 0;
                used = 1;
                itemInfo(ItemMgr.m_wep_id, &info);
                if (info.type == 1) {
                    ItemMgr.arm(ItemMgr.pArm);
                }
                break;
            }
            case 4:
                if (pSUB && ((cModel*) pSUB)->id == 3) {
                    mode = 1;
                    subSel = 0;
                    SndCall(0, 4, 0, 0, 0, 0);
                    return;
                }
                ItemMgr.m_to_whom = 0;
                used = ItemMgr.use(pzzl_sel->item);
                if (used) {
                    PlMotionReset();
                }
                break;
            case 5:
                wk->p_exam_item = pzzl_sel->item;
                wk->p_exam_model = MapMgr.getWork(2);
                transit(2, wk);
                SndCall(0, 0x1A, 0, 0, 0, 0);
                return;
            case 6: {
                int one = 1;  // one SImode pseudo for the byte and word stores (`stb r0; stw r0`), mode first so subSel's store dies first
                mode = one;
                subSel = one;
                SndCall(0, 0x2A, 0, 0, 0, 0);
                return;
            }
            }
            if (used == 1) {
                wk->puzzlePlayer->rehash();
                for (int i = 0; i < num * 2 + 6; i++) {
                    id[i]->rev_flag |= 0xF;
                }
                transit(0, wk);
                SndCall(0, 8, 0, 0, 0, 0);
                return;
            }
            SndCall(0, 7, 0, 0, 0, 0);
        }
        {
            int cur;

            if (Key.rep & 0x01000000) {
                wk->cmd_menu_no--;
            } else if (Key.rep & 0x02000000) {
                wk->cmd_menu_no++;
            }
            // ternary straight into the s8 member (MapModeSelect idiom: raw-byte `mr`, hoisted `li 0`, one stb)
            wk->cmd_menu_no = wk->cmd_menu_no < 0 ? num - 1 : (wk->cmd_menu_no > num - 1 ? 0 : wk->cmd_menu_no);
            if (Key.rep & 0x03000000) {
                SndCall(0, 0xA, 0, 0, 0, 0);
            }
            if (cursorOld != wk->cmd_menu_no) {
                for (int i = 0; i < num; i++) {
                    if (i == wk->cmd_menu_no) {
                        id[6 + i * 2]->be_flag |= 8;
                    } else {
                        id[6 + i * 2]->be_flag &= ~8;
                    }
                }
                cursorOld = wk->cmd_menu_no;
            }
        }
        break;
    case 1: {
        u8 type = 0x1C;
        int base = 0;

        // two-case switches: both compares before the arms (`cmpwi 4; beq; cmpwi 6; beq; b`)
        switch (command_id) {
        case 4:
            base = 0x70;
            break;
        case 6:
            base = 0x80;
            break;
        }
        switch (inSpace) {
        case 0:
            type = 0x1C;
            break;
        case 1:
            type = 0x1D;
            break;
        }
        for (int i = 0; i < 11; i++) {
            sub[i] = IdSub.unitPtr(base + i, type);
            sub[i]->be_flag |= 8;
            sub[i]->rev_flag &= 0xF0;
        }
        PSVECAdd(&id[6 + wk->cmd_menu_no * 2]->pos0, &id[6 + wk->cmd_menu_no * 2]->pParent->pos0, &sub[0]->pos0);
        mode = 2;
    }
    case 2:
        if (Key.trg & 0x40000000) {
            for (int i = 0; i < 11; i++) {
                sub[i]->rev_flag |= 0xF;
            }
            mode = 0;
            SndCall(0, 5, 0, 0, 0, 0);
        } else if (Key.trg & 0x80000000) {
            int used = 0;
            int msg = 0;

            switch (command_id) {
            case 4:
                if (subSel == 0) {
                    ItemMgr.m_to_whom = 0;
                } else {
                    ItemMgr.m_to_whom = 1;
                }
                if (ItemMgr.m_to_whom == 1) {
                    switch (wk->sub_cure_flag) {
                    case -1:
                        mode = 3;
                        openMsgWindow(wk, 0x26);
                        used = 0;
                        msg = 1;
                        break;
                    case 0:
                        mode = 3;
                        openMsgWindow(wk, 0x25);
                        used = 0;
                        msg = 1;
                        break;
                    case 1:
                        used = ItemMgr.use(pzzl_sel->item);
                        break;
                    }
                } else {
                    used = ItemMgr.use(pzzl_sel->item);
                }
                if (used) {
                    if (ItemMgr.m_to_whom == 0) {
                        PlMotionReset();
                    } else {
                        SubCharMotionReset();
                    }
                    SndCall(0, 8, 0, 0, 0, 0);
                } else {
                    SndCall(0, 7, 0, 0, 0, 0);
                }
                break;
            case 6:
                if (subSel == 0) {
                    used = ItemMgr.dumpAll(pzzl_sel->item);
                    SndCall(0, 0xD, 0, 0, 0, 0);
                } else {
                    SndCall(0, 5, 0, 0, 0, 0);
                }
                break;
            }
            for (int i = 0; i < num * 2 + 6; i++) {
                id[i]->rev_flag |= 0xF;
            }
            for (int i = 0; i < 11; i++) {
                sub[i]->rev_flag |= 0xF;
            }
            if (used) {
                wk->puzzlePlayer->rehash();
                if (wk->puzzlePlayer->m_space->getPieceNum() == 0) {
                    wk->puzzlePlayer->salvCursor();
                }
            }
            if (msg == 0) {
                transit(0, wk);
            }
        } else {
            int old = subSel;
            int cur;

            if (Key.trg & 0x08000000) {
                subSel--;
            } else if (Key.trg & 0x04000000) {
                subSel++;
            }
            subSel = subSel < 0 ? 0 : (subSel > 1 ? 1 : subSel);
            if (old != subSel) {
                SndCall(0, 0xA, 0, 0, 0, 0);
            }
            sub[5]->be_flag &= ~8;
            sub[7]->be_flag &= ~8;
            if (subSel == 0) {
                sub[5]->be_flag |= 8;
            } else {
                sub[7]->be_flag |= 8;
            }
        }
        break;
    case 3:
        if (checkMsgWindow(wk)) {
            closeMsgWindow(wk);
            transit(0, wk);
        }
        break;
    }
}

// Restores the header when the space board is empty.
void PieceCommand::quit(SUB_SCREEN* wk)
{
    back2PieceSelect(wk);
}

// Case size change: plays the case units' close animation.
void CaseChange::init(SUB_SCREEN* wk)
{
    a = IdSub.unitPtr(0xFE, IDC_SSCRN_BACK_GROUND);
    b = IdSub.unitPtr(0xFD, IDC_SSCRN_BACK_GROUND);
    a->rev_flag |= 0xF;
    b->rev_flag |= 0xF;
}

// When the close animation ended, rebuilds the pzlPlayer with board_next (the bigger case) and its
// models, then returns to PieceSelect.
void CaseChange::move(SUB_SCREEN* wk)
{
    if ((a->anima_state & 1) && (b->anima_state & 4)) {
        wk->puzzlePlayer->quit();
        delete wk->puzzlePlayer;
        wk->board_size = wk->board_next;
        wk->puzzlePlayer = new pzlPlayer;
        if (!wk->puzzlePlayer->init((s8) wk->board_size)) {
            delete wk->puzzlePlayer;
        }
        pieceModelInit(wk);
        transit(0, wk);
    }
}

// Plays the case units' open animation.
void CaseChange::quit(SUB_SCREEN* wk)
{
    a->rev_flag &= 0xF0;
    b->rev_flag &= 0xF0;
}

// Command menu type of an item: 0 weapon, 3 ammo / treasure, 4 usable, 5 combinable, 6 weapon
// part, 7 key, 8 herb, 9 file.
int itemCommandType(ItemWork* item)
{
    ItemInfo info;

    itemInfo(item->id, &info);
    switch (info.type) {
    case 1:
    case 3:
        return 0;
    case 4:
        return 7;
    case 9:
        if (item->lv != 0) {
            return 6;
        }
        return 3;
    case 2:
    case 5:
    case 0xC:
        return 3;
    case 6:
        switch (item->id) {
        case 8 ... 0xA:
            return 8;
        case 0x19:
        case 0x1C:
        case 0xA8:
            return 3;
        }
        if (itemCombineCheck(item->id) == 0) {
            return 4;
        }
        return 5;
    case 0xE:
        return 9;
    }
    if (itemCombineCheck(item->id) == 0) {
        return 4;
    }
    return 5;
}

// The command menu id units of a command type (`lang`: 0 the case board set, 1 the space set).
static void setCommandId(u8 type, IdUnit** tbl, s8* num, int lang)
{
    u8 t = 0x1D;

    if (lang == 0) {
        t = 0x1C;
    }
    switch (type) {
    case 0:
        tbl[0] = IdSub.unitPtr(0, t);
        tbl[1] = IdSub.unitPtr(1, t);
        tbl[2] = IdSub.unitPtr(2, t);
        tbl[3] = IdSub.unitPtr(3, t);
        tbl[4] = IdSub.unitPtr(0xA, t);
        tbl[5] = IdSub.unitPtr(0xB, t);
        *num = 3;
        tbl[6] = IdSub.unitPtr(4, t);
        tbl[7] = IdSub.unitPtr(5, t);
        tbl[8] = IdSub.unitPtr(6, t);
        tbl[9] = IdSub.unitPtr(7, t);
        tbl[10] = IdSub.unitPtr(8, t);
        tbl[11] = IdSub.unitPtr(9, t);
        break;
    case 1:
        tbl[0] = IdSub.unitPtr(0x10, t);
        tbl[1] = IdSub.unitPtr(0x11, t);
        tbl[2] = IdSub.unitPtr(0x12, t);
        tbl[3] = IdSub.unitPtr(0x13, t);
        tbl[4] = IdSub.unitPtr(0x1C, t);
        tbl[5] = IdSub.unitPtr(0x1D, t);
        *num = 4;
        tbl[6] = IdSub.unitPtr(0x14, t);
        tbl[7] = IdSub.unitPtr(0x15, t);
        tbl[8] = IdSub.unitPtr(0x16, t);
        tbl[9] = IdSub.unitPtr(0x17, t);
        tbl[10] = IdSub.unitPtr(0x18, t);
        tbl[11] = IdSub.unitPtr(0x19, t);
        tbl[12] = IdSub.unitPtr(0x1A, t);
        tbl[13] = IdSub.unitPtr(0x1B, t);
        break;
    case 2:
        tbl[0] = IdSub.unitPtr(0x20, t);
        tbl[1] = IdSub.unitPtr(0x21, t);
        tbl[2] = IdSub.unitPtr(0x22, t);
        tbl[3] = IdSub.unitPtr(0x23, t);
        tbl[4] = IdSub.unitPtr(0x2E, t);
        tbl[5] = IdSub.unitPtr(0x2F, t);
        *num = 5;
        tbl[6] = IdSub.unitPtr(0x24, t);
        tbl[7] = IdSub.unitPtr(0x25, t);
        tbl[8] = IdSub.unitPtr(0x26, t);
        tbl[9] = IdSub.unitPtr(0x27, t);
        tbl[10] = IdSub.unitPtr(0x28, t);
        tbl[11] = IdSub.unitPtr(0x29, t);
        tbl[12] = IdSub.unitPtr(0x2A, t);
        tbl[13] = IdSub.unitPtr(0x2B, t);
        tbl[14] = IdSub.unitPtr(0x2C, t);
        tbl[15] = IdSub.unitPtr(0x2D, t);
        break;
    case 3:
        tbl[0] = IdSub.unitPtr(0x30, t);
        tbl[1] = IdSub.unitPtr(0x31, t);
        tbl[2] = IdSub.unitPtr(0x32, t);
        tbl[3] = IdSub.unitPtr(0x33, t);
        tbl[4] = IdSub.unitPtr(0x3A, t);
        tbl[5] = IdSub.unitPtr(0x3B, t);
        *num = type;
        tbl[6] = IdSub.unitPtr(0x34, t);
        tbl[7] = IdSub.unitPtr(0x35, t);
        tbl[8] = IdSub.unitPtr(0x36, t);
        tbl[9] = IdSub.unitPtr(0x37, t);
        tbl[10] = IdSub.unitPtr(0x38, t);
        tbl[11] = IdSub.unitPtr(0x39, t);
        break;
    case 4:
        tbl[0] = IdSub.unitPtr(0x40, t);
        tbl[1] = IdSub.unitPtr(0x41, t);
        tbl[2] = IdSub.unitPtr(0x42, t);
        tbl[3] = IdSub.unitPtr(0x43, t);
        tbl[4] = IdSub.unitPtr(0x4A, t);
        tbl[5] = IdSub.unitPtr(0x4B, t);
        *num = 3;
        tbl[6] = IdSub.unitPtr(0x44, t);
        tbl[7] = IdSub.unitPtr(0x45, t);
        tbl[8] = IdSub.unitPtr(0x46, t);
        tbl[9] = IdSub.unitPtr(0x47, t);
        tbl[10] = IdSub.unitPtr(0x48, t);
        tbl[11] = IdSub.unitPtr(0x49, t);
        break;
    case 5:
        tbl[0] = IdSub.unitPtr(0x50, t);
        tbl[1] = IdSub.unitPtr(0x51, t);
        tbl[2] = IdSub.unitPtr(0x52, t);
        tbl[3] = IdSub.unitPtr(0x53, t);
        tbl[4] = IdSub.unitPtr(0x5C, t);
        tbl[5] = IdSub.unitPtr(0x5D, t);
        *num = 4;
        tbl[6] = IdSub.unitPtr(0x54, t);
        tbl[7] = IdSub.unitPtr(0x55, t);
        tbl[8] = IdSub.unitPtr(0x56, t);
        tbl[9] = IdSub.unitPtr(0x57, t);
        tbl[10] = IdSub.unitPtr(0x58, t);
        tbl[11] = IdSub.unitPtr(0x59, t);
        tbl[12] = IdSub.unitPtr(0x5A, t);
        tbl[13] = IdSub.unitPtr(0x5B, t);
        break;
    case 6:
        tbl[0] = IdSub.unitPtr(0x90, t);
        tbl[1] = IdSub.unitPtr(0x91, t);
        tbl[2] = IdSub.unitPtr(0x92, t);
        tbl[3] = IdSub.unitPtr(0x93, t);
        tbl[4] = IdSub.unitPtr(0x9A, t);
        tbl[5] = IdSub.unitPtr(0x9B, t);
        *num = 3;
        tbl[6] = IdSub.unitPtr(0x94, t);
        tbl[7] = IdSub.unitPtr(0x95, t);
        tbl[8] = IdSub.unitPtr(0x96, t);
        tbl[9] = IdSub.unitPtr(0x97, t);
        tbl[10] = IdSub.unitPtr(0x98, t);
        tbl[11] = IdSub.unitPtr(0x99, t);
        break;
    case 7:
        tbl[0] = IdSub.unitPtr(0x60, t);
        tbl[1] = IdSub.unitPtr(0x61, t);
        tbl[2] = IdSub.unitPtr(0x62, t);
        tbl[3] = IdSub.unitPtr(0x63, t);
        tbl[4] = IdSub.unitPtr(0x68, t);
        tbl[5] = IdSub.unitPtr(0x69, t);
        *num = 2;
        tbl[6] = IdSub.unitPtr(0x64, t);
        tbl[7] = IdSub.unitPtr(0x65, t);
        tbl[8] = IdSub.unitPtr(0x66, t);
        tbl[9] = IdSub.unitPtr(0x67, t);
        break;
    case 8:
        tbl[0] = IdSub.unitPtr(0xA0, t);
        tbl[1] = IdSub.unitPtr(0xA1, t);
        tbl[2] = IdSub.unitPtr(0xA2, t);
        tbl[3] = IdSub.unitPtr(0xA3, t);
        tbl[4] = IdSub.unitPtr(0xAC, t);
        tbl[5] = IdSub.unitPtr(0xAD, t);
        *num = 4;
        tbl[6] = IdSub.unitPtr(0xA4, t);
        tbl[7] = IdSub.unitPtr(0xA5, t);
        tbl[8] = IdSub.unitPtr(0xA6, t);
        tbl[9] = IdSub.unitPtr(0xA7, t);
        tbl[10] = IdSub.unitPtr(0xA8, t);
        tbl[11] = IdSub.unitPtr(0xA9, t);
        tbl[12] = IdSub.unitPtr(0xAA, t);
        tbl[13] = IdSub.unitPtr(0xAB, t);
        break;
    case 9:
        tbl[0] = IdSub.unitPtr(0xB0, t);
        tbl[1] = IdSub.unitPtr(0xB1, t);
        tbl[2] = IdSub.unitPtr(0xB2, t);
        tbl[3] = IdSub.unitPtr(0xB3, t);
        tbl[4] = IdSub.unitPtr(0xB6, t);
        tbl[5] = IdSub.unitPtr(0xB7, t);
        *num = 1;
        tbl[6] = IdSub.unitPtr(0xB4, t);
        tbl[7] = IdSub.unitPtr(0xB5, t);
        break;
    }
}
