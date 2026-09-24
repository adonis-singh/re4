// game/puzzle: the attache case packing puzzle behind the inventory — pzlBoard is a cell grid
// (the case, and a spare board) holding pzlPiece pieces (one per item, shapes from piece_info,
// rotated / mirrored in 8 orientations), pzlPlayer moves a cursor and a hand piece between the
// boards (pick / put / swap / cancel) and writes the layout back into the ItemWork records.
// PutInCase is the game-side entry that fits a picked-up item into the case (or stacks ammo).
#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "joy.h"
#include "item.h"
#include "model.h"
#include "math_sub.h"
#include "db_log.h"
#include "eprintf.h"
#include "puzzle.h"

// Attache case packing puzzle: pieces (items) placed on the case board or the spare board.

extern "C" {
void* __builtin_new(unsigned int size);
void __builtin_delete(void* p);
void* __builtin_vec_new(unsigned int size);
void __builtin_vec_delete(void* p);
}

static u8 piece_max = 0x79;
static u8 space_w = 6;
static u8 space_h = 0xC;

PieceInfo piece_info[] = {
    { 0x0003, 0, 5, 2, { 0, 0 }, 2.0f, 0.5f, "1111111111", { 0 } },
    { 0x0021, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0040, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0023, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0025, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x0027, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0029, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x002A, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x002C, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x002D, 0, 5, 2, { 0, 0 }, 2.0f, 0.5f, "1111111111", { 0 } },
    { 0x002E, 0, 9, 1, { 0, 0 }, 4.0f, 0.0f, "111111111", { 0 } },
    { 0x002F, 0, 7, 2, { 0, 0 }, 3.0f, 0.5f, "11111111111111", { 0 } },
    { 0x0030, 0, 3, 2, { 0, 0 }, 1.0f, 0.5f, "111111", { 0 } },
    { 0x0034, 0, 7, 3, { 0, 0 }, 3.0f, 1.0f, "111111111111111111111", { 0 } },
    { 0x0035, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x0036, 0, 5, 2, { 0, 0 }, 2.0f, 0.5f, "1111111111", { 0 } },
    { 0x0037, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x0038, 0, 1, 3, { 0, 0 }, 0.0f, 1.0f, "111", { 0 } },
    { 0x0094, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x0017, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x006D, 0, 8, 2, { 0, 0 }, 3.5f, 0.5f, "1111111111111111", { 0 } },
    { 0x003E, 0, 4, 2, { 0, 0 }, 1.5f, 0.5f, "11111111", { 0 } },
    { 0x0052, 0, 7, 3, { 0, 0 }, 3.0f, 1.0f, "111111111111111111111", { 0 } },
    { 0x0004, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0020, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0018, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0007, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x006A, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x001A, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0000, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0046, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0072, 0, 4, 1, { 0, 0 }, 1.5f, 0.0f, "1111", { 0 } },
    { 0x003F, 0, 2, 1, { 0, 0 }, 0.5f, 0.0f, "11", { 0 } },
    { 0x0042, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x0043, 0, 2, 2, { 0, 0 }, 0.5f, 0.5f, "1111", { 0 } },
    { 0x0044, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x0045, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x00AA, 0, 2, 2, { 0, 0 }, 0.5f, 0.5f, "1111", { 0 } },
    { 0x00C5, 0, 3, 1, { 0, 0 }, 1.0f, 0.0f, "111", { 0 } },
    { 0x0001, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0002, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x000E, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0005, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0008, 0, 1, 1, { 0, 0 }, 0.0f, 0.0f, "1", { 0 } },
    { 0x0009, 0, 1, 1, { 0, 0 }, 0.0f, 0.0f, "1", { 0 } },
    { 0x000A, 0, 1, 1, { 0, 0 }, 0.0f, 0.0f, "1", { 0 } },
    { 0x0095, 0, 1, 3, { 0, 0 }, 0.0f, 1.0f, "111", { 0 } },
    { 0x0097, 0, 2, 6, { 0, 0 }, 0.5f, 2.5f, "111111111111", { 0 } },
    { 0x0006, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0019, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x001C, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0014, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0016, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x00A8, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0012, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0013, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x0015, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0x000C, 0, 1, 2, { 0, 0 }, 0.0f, 0.5f, "11", { 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 }, 0.0f, 0.0f, "", { 0 } },
};

// Shape record of item `id` in the piece table (ends with id 0xFFFF); 0 when the item has none.
PieceData* searchItemPieceData(int item_id, PieceInfo* p_info)
{
    int i;

    for (i = 0;; i++) {
        if (p_info[i].id == item_id) {
            return &p_info[i].data;
        }
        if (p_info[i].id == 0xFFFF) {
            return 0;
        }
    }
}

// Model record of item `id` in the piece table; 0 when none.
u8* searchItemModelData(int item_id, PieceInfo* p_info)
{
    int i;

    for (i = 0;; i++) {
        if (p_info[i].id == item_id) {
            return p_info[i].model;
        }
        if (p_info[i].id == 0xFFFF) {
            return 0;
        }
    }
}

// The second loop counts from 0 to `o - 4` (not from 4 to `o`): the runtime bound is computed
// before the entry test (`addic. r0,r29,-4; ble`), check_dbra_loop reverses the compare-only biv
// with that count, and the count temp takes the ctrsi pattern's CTR preference (`mtctr r0; mfctr
// r31`; the loop body's call keeps the biv itself in r31).
void pzlPiece::orientation(int orientation_no)
{
    int i;

    m_center_x = m_p_data->center_x;
    m_center_y = m_p_data->center_y;
    m_orientation = 0;
    if (orientation_no <= 3) {
        for (i = 0; i < orientation_no; i++) {
            rotate(0);
        }
    } else {
        mirror(0);
        for (i = 0; i < orientation_no - 4; i++) {
            rotate(0);
        }
    }
    if (orientation_no != m_orientation) {
        pLog->err(0, 0, "pzlPiece::orientation() failed.");
    }
}

// Rotates the piece a quarter turn (dir 0 clockwise, 1 counter-clockwise) within its orientation
// group (0..3 normal, 4..7 mirrored) and rotates the centre offset with it.
void pzlPiece::rotate(int dir)
{
    switch (dir) {
    case 0: {
        if (m_orientation >= 0) {
            if (m_orientation <= 3) {
                m_orientation++;
                if (m_orientation > 3) {
                    m_orientation -= 4;
                }
            } else if (m_orientation <= 7) {
                m_orientation++;
                if (m_orientation > 7) {
                    m_orientation -= 4;
                }
            }
        }
        f32 t = m_center_x;
        m_center_x = -m_center_y;
        m_center_y = t;
        break;
    }
    case 1: {
        if (m_orientation >= 0) {
            if (m_orientation <= 3) {
                m_orientation--;
                if (m_orientation < 0) {
                    m_orientation += 4;
                }
            } else if (m_orientation <= 7) {
                m_orientation--;
                if (m_orientation < 4) {
                    m_orientation += 4;
                }
            }
        }
        f32 t = m_center_x;
        m_center_x = m_center_y;
        m_center_y = -t;
        break;
    }
    }
}

// Mirrors the piece (axis 0 horizontal: flips cx, 1 vertical: flips the centre y) — moves between
// the normal and mirrored orientation groups.
void pzlPiece::mirror(int dir)
{
    switch (dir) {
    case 0:
        switch (m_orientation) {
        case 0:
            m_orientation = 4;
            break;
        case 1:
            m_orientation = 7;
            break;
        case 2:
            m_orientation = 6;
            break;
        case 3:
            m_orientation = 5;
            break;
        case 4:
            m_orientation = 0;
            break;
        case 5:
            m_orientation = 3;
            break;
        case 6:
            m_orientation = 2;
            break;
        case 7:
            m_orientation = 1;
            break;
        }
        m_center_x = -m_center_x;
        break;
    case 1:
        switch (m_orientation) {
        case 0:
            m_orientation = 6;
            break;
        case 1:
            m_orientation = 5;
            break;
        case 2:
            m_orientation = 4;
            break;
        case 3:
            m_orientation = 7;
            break;
        case 4:
            m_orientation = 2;
            break;
        case 5:
            m_orientation = 1;
            break;
        case 6:
            m_orientation = 0;
            break;
        case 7:
            m_orientation = 3;
            break;
        }
        m_center_y = -m_center_y;
        break;
    }
}

// Takes the piece into use with shape `p_data`, orientation 0, not on a board.
void pzlPiece::init(PieceData* p_data)
{
    m_p_data = p_data;
    be_flag |= 1;
    m_center_x = p_data->center_x;
    m_center_y = p_data->center_y;
    m_place = 0;
    m_orientation = 0;
}

// Board x of the piece's top-left cell (centre minus the centre offset).
f32 pzlPiece::ver0_x()
{
    return m_pos_x - m_center_x;
}

// Board y of the piece's top-left cell.
f32 pzlPiece::ver0_y()
{
    return m_pos_y - m_center_y;
}

// Signed width in cells for the current orientation (negative = the shape runs leftwards from
// ver0); 0 without shape data.
int pzlPiece::size_x()
{
    s8 size;

    if (m_p_data == 0) {
        return 0;
    }
    switch (m_orientation) {
    case 0:
    case 6:
        size = m_p_data->size_x;
        break;
    case 1:
    case 5:
        size = -m_p_data->size_y;
        break;
    case 2:
    case 4:
        size = -m_p_data->size_x;
        break;
    case 3:
    case 7:
        size = m_p_data->size_y;
        break;
    default:
        goto none;
    }
    return size;
none:
    return 0;
}

// The last arm's `neg; extsb; blr` tail is cross-jumped into the previous arm in the original:
// jump2 only pairs RETURN insns, so every arm returns on its own (the `break` form's last arm falls
// into the shared return and is never a candidate). With per-arm returns the byte value prefers r3
// (global.c's sign_extend preference); the original keeps it in r0.
// Signed height in cells for the current orientation.
int pzlPiece::size_y()
{
    register s8 size asm("r0");  // COMPILER-DIFF: #17 (value pin)

    if (m_p_data == 0) {
        return 0;
    }
    switch (m_orientation) {
    case 0:
    case 4:
        size = m_p_data->size_y;
        return size;
    case 1:
    case 7:
        size = m_p_data->size_x;
        return size;
    case 2:
    case 6:
        size = -m_p_data->size_y;
        return size;
    case 3:
    case 5:
        size = -m_p_data->size_x;
        return size;
    }
    return 0;
}

// Rounds the piece position so its top-left cell sits on the cell grid.
void pzlPiece::snap()
{
    s8 vx;
    s8 vy;

    // The `+=` in both arms: jump2 merges the tails from the conversion on and each arm keeps
    // its own `mr r3,this` for the third call.
    if (ver0_x() >= 0.0f) {
        vx = ver0_x() + 0.5f;
        m_pos_x += ver0_x() - (f32) vx;
    } else {
        vx = ver0_x() - 0.5f;
        m_pos_x += ver0_x() - (f32) vx;
    }
    if (ver0_y() >= 0.0f) {
        vy = ver0_y() + 0.5f;
        m_pos_y += ver0_y() - (f32) vy;
    } else {
        vy = ver0_y() - 0.5f;
        m_pos_y += ver0_y() - (f32) vy;
    }
}

// s8 rotation matrix kept in one word (rlwimi inserts); the products are (s8)px * s8 entries so
// convert_to_integer narrows the multiplies to QImode (the low byte of the word is used raw,
// byte 2 comes out as `extsh; srawi 8`); the negated sine goes through an s8 local (its
// `extsb; neg`); the mirror test is a `case 4..7` range (`cmpwi 7; bgt` then `cmpwi 4; blt`).
int pzlPiece::shape(int x, int y)
{
    s8 rot[2][2];
    f32 ang;
    int oi;
    s8 o;
    s8 s;
    int rx;
    int ry;
    // COMPILER-DIFF: px as a one-byte struct. A RECORD_TYPE local is not PROMOTE_MODEd, so `w` is
    // an unpromoted QImode pseudo and `w.v` expands to that REG (extract_bit_field's lsb-subreg
    // path returns op0 itself); `(s8) px` expands to `(subreg:QI px)`, and expand_binop swaps a
    // commutative multiply whenever op1 is a REG and op0 is not -- byte 2 of `rot` is such a REG
    // (the HImode extraction is copied into a fresh QI pseudo), which put `px` second in that one
    // `mullw`. cse folds the paradoxical `(subreg:SI w)` back to px (like the force_reg temps of
    // the other operands), so the copy is dead and no instruction is added (pass 5).
    struct { s8 v; } w;

    // The join `extsb r0,r0` is the promoted store of `o` from the int `oi` on both paths: the
    // compare's extension IS `oi` (r0), the else arm copies it into `o` (extsb of r0 -> r0), so the
    // `ble` lands on that extsb (pass 4).
    oi = m_orientation;
    if (oi > 3) {
        o = m_orientation - 4;
    } else {
        o = oi;
    }
    ang = (f32) o * -3.1415927f * 0.5f;
    rot[0][0] = cosf(ang);
    s = sinf(ang);
    rot[0][1] = -s;
    rot[1][0] = sinf(ang);
    rot[1][1] = cosf(ang);
    w.v = x;
    rx = (s8) (w.v * rot[0][0] + (s8) y * rot[0][1]);
    ry = (s8) (w.v * rot[1][0] + (s8) y * rot[1][1]);
    switch (m_orientation) {
    case 4:
    case 5:
    case 6:
    case 7:
        rx = (s8) -rx;
        break;
    }
    if (rx < 0 || rx >= m_p_data->size_x || ry < 0 || ry >= m_p_data->size_y) {
        return 0;
    }
    return m_p_data->shape[rx + m_p_data->size_x * ry] == '1';
}

// Debug shape display. Dead: the original linker dropped the body (STRIP_UNUSED); its strings
// ("#", "") and constant pool (the int->f32 double trick, 8.0f, 14.0f) stay in .rodata between
// shape's pool and pzlBoard::init's strings.
static void dispShape(pzlPiece* p, int px, int py)
{
    int i;
    int j;

    for (j = 0; j < p->m_p_data->size_y; j++) {
        for (i = 0; i < p->m_p_data->size_x; i++) {
            f32 x = (f32) (px + i) * 8.0f + 14.0f;
            eprintf((int) x, py + j, 0, 0, p->shape(i, j) ? "#" : "");
        }
    }
}

#line 540 "D:/Bio4/Prog/puzzle.cpp"
// Allocates a w x h cell board (all cells free) and a table for `pieceMax_` pieces; 0 on failure.
int pzlBoard::init(int w, int h, int piece_max)
{
    int i;

#line 543 "D:/Bio4/Prog/puzzle.cpp"
    m_cell = (u8*) MEM_ALLOC(w * h, 1, 13);
    if (m_cell == 0) {
        m_size_x = 0;
        m_size_y = 0;
        return 0;
    }
    m_size_x = w;
    m_size_y = h;
    clearState(0xFF);
#line 560 "D:/Bio4/Prog/puzzle.cpp"
    m_p_piece = (pzlPiece**) MEM_ALLOC(piece_max * 4, 1, 13);
    if (m_p_piece == 0) {
        Mem_free(m_cell);
        return 0;
    }
    // Guarded count-down (`cmpwi n,0; beq` + `mtctr n` after the guard): the counter is a local set
    // inside the guard, so its CTR copy is initialised after the branch, not before it.
    if (piece_max != 0) {
        int n = piece_max;
        i = 0;
        do {
            m_p_piece[i] = 0;
            i++;
            n--;
        } while (n != 0);
    }
    m_piece_max = piece_max;
    return 1;
}

// Frees the cell and piece tables.
void pzlBoard::quit()
{
    if (m_cell) {
        Mem_free(m_cell);
    }
    if (m_p_piece) {
        Mem_free(m_p_piece);
    }
}

// Pieces currently placed on the board.
int pzlBoard::getPieceNum()
{
    u8 n = 0;
    int i;

    for (i = 0; i < m_piece_max; i++) {
        if (m_p_piece[i]) {
            n++;
        }
    }
    return n;
}

// 1 when `p` is one of the placed pieces.
int pzlBoard::search(pzlPiece* p)
{
    int i;

    for (i = 0; i < m_piece_max; i++) {
        if (m_p_piece[i] && p == m_p_piece[i]) {
            return 1;
        }
    }
    return 0;
}

// 1 when every filled cell of `p` is on the board or in the one-cell border ring around it; else
// 0 with m_wall_miss_flag = the side it left (1 left, 2 right, 3 up, 4 down).
int pzlBoard::ckInsideWall(pzlPiece* p_piece)
{
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;

    m_wall_miss_flag = 0;
    sx = p_piece->size_x();
    sy = p_piece->size_y();
    vx = p_piece->ver0_x();
    vy = p_piece->ver0_y();
    m_wall_miss_flag = 0;
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p_piece->shape((s8) i, (s8) j)) {
                int cx_ = vx + i;
                int cy_ = vy + j;
                if ((cellState((s8) cx_, (s8) cy_) & 2) && !(cellState((s8) cx_, (s8) cy_) & 0x40)) {
                    if (cx_ < -1) {
                        m_wall_miss_flag = 1;
                    }
                    if (cx_ > m_size_x) {
                        m_wall_miss_flag = 2;
                    }
                    if (cy_ < -1) {
                        m_wall_miss_flag = 3;
                    }
                    if (cy_ > m_size_y) {
                        m_wall_miss_flag = 4;
                    }
                    return 0;
                }
            }
        }
    }
    return 1;
}

// 1 when the piece is entirely off the board (m_out_miss_flag = the side it is beyond, or 0 when
// it straddles the ring), 0 when any of its cells is on the board.
int pzlBoard::outPiece(pzlPiece* p_piece)
{
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;
    int left = 1;
    int right = 1;
    int up = 1;
    int down = 1;

    m_out_miss_flag = 0;
    sx = p_piece->size_x();
    sy = p_piece->size_y();
    vx = p_piece->ver0_x();
    vy = p_piece->ver0_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p_piece->shape((s8) i, (s8) j)) {
                int cx_ = vx + i;
                int cy_ = vy + j;
                if (!(cellState((s8) cx_, (s8) cy_) & 2)) {
                    return 0;
                }
                if (cx_ >= 0) {
                    left = 0;
                }
                if (cx_ <= m_size_x - 1) {
                    right = 0;
                }
                if (cy_ >= 0) {
                    up = 0;
                }
                if (cy_ <= m_size_y - 1) {
                    down = 0;
                }
            }
        }
    }
    if (left) {
        m_out_miss_flag = 1;
    }
    if (right) {
        m_out_miss_flag = 2;
    }
    if (up) {
        m_out_miss_flag = 3;
    }
    if (down) {
        m_out_miss_flag = 4;
    }
    return 1;
}

// Places `p` (snapped) if none of its cells is occupied or off the board: marks the cells (bit0),
// registers it, state 1. Returns 1 on success.
int pzlBoard::putPiece(pzlPiece* p_piece)
{
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;

    p_piece->snap();
    sx = p_piece->size_x();
    sy = p_piece->size_y();
    vx = p_piece->ver0_x();
    vy = p_piece->ver0_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p_piece->shape((s8) i, (s8) j)) {
                if (cellState((s8) (vx + i), (s8) (vy + j)) & 1) {
                    return 0;
                }
            }
        }
    }
    // The slot search has its own counter (r10: no call crossed); the marking nest reuses i/j,
    // which then conflict with the cx_/cy_ temps (r31/r30) and take r28/r29 in both nests.
    for (int n = 0; n < m_piece_max; n++) {
        if (m_p_piece[n] == 0) {
            m_p_piece[n] = p_piece;
            for (i = 0; i != sx; sx > 0 ? i++ : i--) {
                for (j = 0; j != sy; sy > 0 ? j++ : j--) {
                    if (p_piece->shape((s8) i, (s8) j)) {
                        s8 cx_ = vx + i;
                        s8 cy_ = vy + j;
                        if (!(cellState(cx_, cy_) & 1)) {
                            *cell(cx_, cy_) |= 1;
                        }
                    }
                }
            }
            p_piece->setOnBoard();
            return 1;
        }
    }
    return 0;
}

// The single placed piece that `p` (snapped) overlaps; 0 when it overlaps none, more than one, or
// leaves the board.
pzlPiece* pzlBoard::lapPiece(pzlPiece* p_piece)
{
    pzlPiece* hit = 0;
    int sx;
    int sy;
    s8 vx;
    s8 vy;
    int i;
    int j;

    p_piece->snap();
    sx = p_piece->size_x();
    sy = p_piece->size_y();
    vx = p_piece->ver0_x();
    vy = p_piece->ver0_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p_piece->shape((s8) i, (s8) j)) {
                s8 cx_ = vx + i;
                s8 cy_ = vy + j;
                pzlPiece* q;
                if (cellState(cx_, cy_) & 2) {
                    return 0;
                }
                q = getPiece(cx_, cy_);
                if (q) {
                    if (hit == 0) {
                        hit = q;
                    } else if (hit != q) {
                        return 0;
                    }
                }
            }
        }
    }
    return hit;
}

// The piece covering cell (x, y), or 0.
pzlPiece* pzlBoard::getPiece(int x, int y)
{
    pzlPiece* p = 0;
    int i;

    if (!(cellState(x, y) & 1)) {
        return 0;
    }
    for (i = 0; i < m_piece_max; i++) {
        if (m_p_piece[i]) {
            s8 vx = m_p_piece[i]->ver0_x();
            s8 vy = m_p_piece[i]->ver0_y();
            if (m_p_piece[i]->shape((s8) (x - vx), (s8) (y - vy))) {
                p = m_p_piece[i];
                break;
            }
        }
    }
    return p;
}

// Removes `p` from the board: frees its cells and table slot, state 0. Always 1.
int pzlBoard::rmPiece(pzlPiece* p_piece)
{
    s8 vx;
    s8 vy;
    int sx;
    int sy;
    int i;
    int j;

    vx = p_piece->ver0_x();
    vy = p_piece->ver0_y();
    sx = p_piece->size_x();
    sy = p_piece->size_y();
    for (i = 0; i != sx; sx > 0 ? i++ : i--) {
        for (j = 0; j != sy; sy > 0 ? j++ : j--) {
            if (p_piece->shape((s8) i, (s8) j)) {
                *cell((s8) (vx + i), (s8) (vy + j)) &= ~1;
            }
        }
    }
    // Own counter for the slot search (r11, no call crossed: sy then takes r30 and i r29).
    for (int n = 0; n < m_piece_max; n++) {
        if (p_piece == m_p_piece[n]) {
            m_p_piece[n] = 0;
            break;
        }
    }
    p_piece->setNoPlace();
    return 1;
}

// Removes and returns the piece covering (x, y), or 0.
pzlPiece* pzlBoard::rmPiece(int x, int y)
{
    pzlPiece* p = getPiece(x, y);

    if (p) {
        rmPiece(p);
    }
    return p;
}

// Address of the state byte of cell (x, y) (no range check).
u8* pzlBoard::cell(int x, int y)
{
    return m_cell + (x + y * m_size_x);
}

// Cell state: on the board the cell byte (bit0 occupied); off the board 3 (occupied + outside), or
// 0x43 for the one-cell ring just outside (outside + wall) where pieces may hover.
int pzlBoard::cellState(int x, int y)
{
    if (x >= m_size_x || x < 0 || y >= m_size_y || y < 0) {
        if ((x == -1 || x == m_size_x) && y >= -1 && y <= m_size_y) {
            return 0x43;
        }
        if ((y == -1 || y == m_size_y) && x >= -1 && x <= m_size_x) {
            return 0x43;
        }
        return 3;
    }
    return *cell(x, y);
}

// Clears `mask` bits in every cell.
void pzlBoard::clearState(u8 state)
{
    int i;
    int j;

    for (i = 0; i < m_size_x; i++) {
        for (j = 0; j < m_size_y; j++) {
            *cell((s8) i, (s8) j) &= ~state;
        }
    }
}

// Debug cell display. Dead (STRIP_UNUSED); its "%c" sits between pzlBoard::init's file string
// and pzlPlayer::init's strings.
static void dispCell(pzlBoard* b, int x, int y)
{
    eprintf(x, y, 0, 0, "%c", (b->cellState(x, y) & 1) ? '1' : '0');
}

// Case sizes: the switch is on an unsigned index with a `case 0` sharing the default label
// (balanced tree root 1, `cmplwi/blt` to default for 0, case bodies laid out 3, 2, 1, default).
// One `p` for both piece loops (its priority then beats `item`'s: p r31, item r30); the flag
// clear loop has its own counter (r10, no call crossed); item positions are stored halved
// (save() doubles them back).
int pzlPlayer::init(int size)
{
    int w;
    int h;
    int i;
    int extraGame;
    pzlPiece* p;

    extraGame = pG->pl_type == 1;
    switch ((u32) size) {
    case 3:
        w = 0xF;
        h = 8;
        break;
    case 2:
        w = 0xC;
        h = 8;
        break;
    case 1:
        w = 0xB;
        h = 7;
        break;
    case 0:
    default:
        w = 0xA;
        h = 6;
        break;
    }
    m_board = (pzlBoard*) __builtin_new(sizeof(pzlBoard));
    if (m_board == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        return 0;
    }
    if (m_board->init(w, h, piece_max) == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        __builtin_delete(m_board);
        return 0;
    }
    m_space = (pzlBoard*) __builtin_new(sizeof(pzlBoard));
    if (m_space == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        m_board->quit();
        __builtin_delete(m_board);
        return 0;
    }
    if (m_space->init(space_w, space_h, piece_max) == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        m_board->quit();
        __builtin_delete(m_board);
        __builtin_delete(m_space);
        return 0;
    }
    m_piece = (pzlPiece*) __builtin_vec_new(piece_max * sizeof(pzlPiece));
    if (m_piece == 0) {
        pLog->err(0, 0, "Can't create pzlPlayer()");
        m_board->quit();
        __builtin_delete(m_board);
        m_space->quit();
        __builtin_delete(m_space);
        return 0;
    }
    m_piece_max = piece_max;
    {
        int j;
        for (j = 0; j < m_piece_max; j++) {
            m_piece[j].clear();
        }
    }
    {
        int k = 0;
        for (i = 0; i < ItemMgr.m_array_num; i++) {
            ItemWork* item = ItemMgr.at(i);
            if (item->isAlive(extraGame)) {
                p = &m_piece[k];
                PieceData* d = searchItemPieceData(item->id, piece_info);
                if (d) {
                    k++;
                    p->init(d);
                    p->m_pos_x = (f32) item->x * 0.5f;
                    p->m_pos_y = (f32) item->y * 0.5f;
                    p->orientation((s8) item->orient);
                    p->item = item;
                }
            }
        }
    }
    for (i = 0; i < m_piece_max; i++) {
        p = &m_piece[i];
        // `!(bool)`: the flag test is `xori; andi.; bne` (the negated bool materialised).
        if (p->isEmpty()) {
            continue;
        }
        {
            pzlBoard* b = p->item->board ? m_board : m_space;
            if (b->putPiece(p) == 0) {
                p->item->board = 0;
                pLog->err(0, 0, "pzlPlayer::pzlPlayer() Can't locate piece.");
            }
        }
    }
    m_p_active_board = m_board;
    return 1;
}

// Frees both boards and the piece array.
void pzlPlayer::quit()
{
    if (m_board) {
        m_board->quit();
        __builtin_delete(m_board);
    }
    if (m_space) {
        m_space->quit();
        __builtin_delete(m_space);
    }
    if (m_piece) {
        __builtin_vec_delete(m_piece);
    }
}

// Pieces in use.
int pzlPlayer::pieceNum()
{
    int n = 0;
    int i;

    for (i = 0; i < m_piece_max; i++) {
        if (m_piece[i].isEmpty()) {
            continue;
        }
        n++;
    }
    return n;
}

// The `no`-th piece in use (skipping free slots), or 0.
pzlPiece* pzlPlayer::piecePtr(int no)
{
    int n = 0;
    int i;

    for (i = 0; i < m_piece_max; i++) {
        if (m_piece[i].isEmpty()) {
            continue;
        }
        if (n == no) {
            return &m_piece[i];
        }
        n++;
    }
    return 0;
}

// The piece that represents inventory item `item`, or 0.
pzlPiece* pzlPlayer::piecePtr(ItemWork* item)
{
    int i;

    for (i = 0; i < m_piece_max; i++) {
        if (m_piece[i].isEmpty()) {
            continue;
        }
        if (item == m_piece[i].item) {
            return &m_piece[i];
        }
    }
    return 0;
}

// Writes every piece's position (in half cells), orientation and board (1 case, 0 spare) back into
// its ItemWork — the layout the save game / item screen keeps.
void pzlPlayer::save()
{
    int i;

    for (i = 0; i < m_piece_max; i++) {
        pzlPiece* p = &m_piece[i];
        ItemWork* item;
        if (p->isEmpty()) {
            continue;
        }
        item = p->item;
        item->x = (s8) (p->m_pos_x + p->m_pos_x);
        item->y = (s8) (p->m_pos_y + p->m_pos_y);
        item->orient = p->m_orientation;
        if (m_board->search(p)) {
            item->board = 1;
        } else if (m_space->search(p)) {
            item->board = 0;
        }
    }
}

// Adds a new piece for `item` (a picked-up item not yet in the case) in a free slot at (0, 0) as
// m_extra. 0 when the item has no shape or no slot is free.
int pzlPlayer::appendExtraPiece(ItemWork* pItem)
{
    pzlPiece* p = 0;
    PieceData* d;
    int i;

    if (pItem == 0) {
        return 0;
    }
    d = searchItemPieceData(pItem->id, piece_info);
    if (d == 0) {
        return 0;
    }
    for (i = 0; i < m_piece_max; i++) {
        pzlPiece* q = &m_piece[i];
        if (q->isEmpty()) {
            p = q;
            break;
        }
    }
    if (p == 0) {
        return 0;
    }
    p->init(d);
    p->item = pItem;
    p->m_pos_x = 0.0f;
    p->m_pos_y = 0.0f;
    m_extra = p;
    return 1;
}

// Discards the extra piece: off its board, the item erased from the inventory, the model freed.
int pzlPlayer::removeExtraPiece()
{
    if (m_extra->isOnBoard()) {
        pzlBoard* b;
        if (m_board->search(m_extra)) {
            b = m_board;
        } else if (m_space->search(m_extra)) {
            b = m_space;
        } else {
            pLog->err(0, 0, "pzlPlayer::removeExtraPiece(): Piece not found.");
            return 0;
        }
        if (b->rmPiece(m_extra) == 0) {
            pLog->err(0, 0, "pzlPlayer::removeExtraPiece(): Can't remove piece.");
            return 0;
        }
    }
    ItemMgr.erase(m_extra->item);
    m_extra->model->push();
    m_extra->clear();
    m_inhand = 0;
    m_extra = 0;
    return 1;
}

// Puts the extra piece in the player's hand (state 2).
void pzlPlayer::inHandExtraPiece()
{
    m_extra->setInHand();
    m_inhand = m_extra;
}

// Forgets the extra piece (it stays wherever it was placed).
void pzlPlayer::giveupExtraPiece()
{
    m_extra = 0;
}

// Cursor clamp after a move: written twice per axis in the original (a macro): once in the loop's
// `q == 0` else arm and once on the `p == 0` break path INSIDE the loop; the `cur < 0` compare is
// shared between the outer `||` and the inner `if` (cr7). The clamp on the break path makes the
// rotated loop's exit code (`cur += d; if (p == 0) { clamp; break; }`) longer than 20 insns, so
// jump1's duplicate_loop_exit_test never peels it and the loop is entered with a plain `b INC`
// (docs/matching.md COMPILER-DIFF #9, closed: a source form, not a compiler difference).
#define SEL_CHECK(cur, size, lo, hi, done)                                                          \
    if (b->cur < 0 || b->cur > b->size - 1) {                                                        \
        if (b->cur < 0) {                                                                            \
            ret = lo;                                                                                \
        }                                                                                            \
        if (b->cur > b->size - 1) {                                                                  \
            ret = hi;                                                                                \
        }                                                                                            \
        b->cur = save;                                                                               \
    } else {                                                                                         \
        goto done;                                                                                   \
    }

// Per-axis block locals: `d` and `save` are one pseudo per axis (half the live length), so `ret`
// (r29) is allocated before the saves (both r28) and the saves before the two `d`s (both r27).
// `d` is s8: the QImode step is a REG operand of the byte add, so expand_binop keeps it first
// (`add r0,r27,r0`); the `d != 0` test drops the extension (combine's simplify_comparison).
int pzlPlayer::selPiece(pzlBoard* b)
{
    int ret = 0;
    pzlPiece* p;

    if (Key.rep & 0x0F000000) {
        ret = 5;
    }
    {
        s8 d = 0;
        s8 save;
        if (Key.rep & 0x08000000) {
            d = -1;
        }
        if (Key.rep & 0x04000000) {
            d = 1;
        }
        if (d != 0) {
            save = b->m_cur_x;
            p = b->getPiece(save, b->m_cur_y);
            for (;;) {
                b->m_cur_x += d;
                if (p == 0) {
                    SEL_CHECK(m_cur_x, m_size_x, 3, 4, doneX);
                    break;
                }
                {
                    pzlPiece* q = b->getPiece(b->m_cur_x, b->m_cur_y);
                    if (q) {
                        if (q->item != p->item) {
                            goto doneX;
                        }
                    } else {
                        SEL_CHECK(m_cur_x, m_size_x, 3, 4, doneX);
                        goto doneX;
                    }
                }
            }
        }
    }
doneX:
    {
        s8 d = 0;
        s8 save;
        if (Key.rep & 0x01000000) {
            d = -1;
        }
        if (Key.rep & 0x02000000) {
            d = 1;
        }
        if (d != 0) {
            save = b->m_cur_y;
            p = b->getPiece(b->m_cur_x, save);
            for (;;) {
                b->m_cur_y += d;
                if (p == 0) {
                    SEL_CHECK(m_cur_y, m_size_y, 1, 2, doneY);
                    break;
                }
                {
                    pzlPiece* q = b->getPiece(b->m_cur_x, b->m_cur_y);
                    if (q) {
                        if (q->item != p->item) {
                            goto doneY;
                        }
                    } else {
                        SEL_CHECK(m_cur_y, m_size_y, 1, 2, doneY);
                        goto doneY;
                    }
                }
            }
        }
    }
doneY:
    return ret;
}

// The piece under the cursor of board `b`.
pzlPiece* pzlPlayer::ptrPiece(pzlBoard* b)
{
    return b->getPiece(b->m_cur_x, b->m_cur_y);
}

// Picks the piece under the cursor of `b` into the hand, remembering its position / orientation /
// board so relPiece can put it back.
void pzlPlayer::getPiece(pzlBoard* b)
{
    pzlPiece* p = b->rmPiece(b->m_cur_x, b->m_cur_y);

    if (p) {
        p->setInHand();
        m_piece_bak_pos_x = p->m_pos_x;
        m_piece_bak_pos_y = p->m_pos_y;
        m_piece_bak_orientation = p->m_orientation;
        m_piece_bak_board = m_p_active_board;
    }
    m_inhand = p;
}

// Drops the hand piece onto `b` at its position; the cursor moves to it. 0 when it does not fit.
int pzlPlayer::putPiece(pzlBoard* b)
{
    if (m_inhand == 0) {
        return 0;
    }
    if (b->putPiece(m_inhand)) {
        b->m_cur_x = (s8) m_inhand->m_pos_x;
        b->m_cur_y = (s8) m_inhand->m_pos_y;
        m_inhand = 0;
        return 1;
    } else {
        return 0;
    }
}

// Cancels the pick-up: the hand piece returns to where it was taken from (position, orientation,
// board). Returns 1 when it went back.
int pzlPlayer::relPiece(pzlBoard* b)
{
    if (m_inhand) {
        m_inhand->m_pos_x = m_piece_bak_pos_x;
        m_inhand->m_pos_y = m_piece_bak_pos_y;
        m_inhand->orientation(m_piece_bak_orientation);
        if (m_piece_bak_board) {
            if (putPiece(m_piece_bak_board)) {
                m_p_active_board = m_piece_bak_board;
                return 1;
            } else {
                m_p_active_board = m_piece_bak_board;
                return 0;
            }
        }
    }
    return 0;
}

// Swaps the hand piece with the single piece it overlaps on `b`: the overlapped piece goes into
// the hand (with its old place remembered), the hand piece is placed. 0 when no clean overlap.
int pzlPlayer::chgPiece(pzlBoard* b)
{
    pzlPiece* p;

    if (m_inhand == 0) {
        return 0;
    }
    p = b->lapPiece(m_inhand);
    if (p != 0) {
        b->rmPiece(p);
        b->putPiece(m_inhand);
        p->setInHand();
        m_inhand = p;
        m_piece_bak_pos_x = p->m_pos_x;
        m_piece_bak_pos_y = p->m_pos_y;
        m_piece_bak_orientation = p->m_orientation;
        m_piece_bak_board = m_p_active_board;
        return 1;
    }
    return 0;
}

// `ex = 0` after the lapPiece check (its `li` follows the call); the success path is the then-arm
// of `if (combine())` so the failing `return 0` is laid out last; `if (!used) {...} else hand = 0`.
pzlPiece* pzlPlayer::cmbPiece(pzlBoard* b)
{
    pzlPiece* p;
    pzlPiece* h;
    ItemWork* ex;
    int rel = 0;
    int used;

    if (m_inhand == 0) {
        return 0;
    }
    p = b->lapPiece(m_inhand);
    if (p == 0) {
        return 0;
    }
    ex = 0;
    h = m_inhand;
    if (m_extra == p || m_extra == h) {
        ex = m_extra->item;
        int type = itemType(ex->id);
        if (type != 2 && type != 6) {
            return 0;
        }
    }
    if (itemType(p->item->id) == 9) {
        rel = 1;
    } else {
        if (itemType(h->item->id) == 9) {
            rel = 1;
        }
    }
    if (ItemMgr.combine(p->item, h->item, 0)) {
        if (ex) {
            giveupExtraPiece();
        }
        used = h->item->isEmpty();
        if (!used) {
            if (rel) {
                relPiece(m_p_active_board);
            }
        } else {
            m_inhand = 0;
        }
        return p;
    }
    return 0;
}

// Structure notes (bytes): the Joy arms set `ret = 2` and `goto cursor` past the wall block (a
// `do {} while (0)` would be a loop: its invariants get hoisted), so they skip the `Key.rep & 0x0F000000` test and fall into the cursor
// update; `Joy` is read through a pointer (`&Joy` materialised in block 0); `out` is `== 1`
// (`xori; subfic; adde`); the board swap writes `ny` (0.0f on the impossible third path, the step
// is the -2.0f constant); `edge` and `step` are ints converted with the double trick; the
// `size_y < 0` clamp adds `cur->h` implicitly (int -> float, magic) where the compare casts (psq_l);
// the `dir` shuffle is a two-case switch. Pass 2: both dir switches have `case 0:` (their lower
// halves cross-jump), the compares convert the member `cur->h` directly (raw byte to psq_l) while
// the stores use the int, `caseBoard` goes through a local before the swap (load order). Pass 4
// (matched): the y clamp uses two ints. `ch` (compare arm only, single set -> r9) and `h`, declared
// with `edge` and set in BOTH y arms (`h = (int)(fabsf(...) - 1.0f) - 1` and `h = edge - sy`): a
// multi-set pseudo cannot be tied to the fix result (so the fix ties to the fctiwz temp, r9, and
// the `- 1` lands in the global r0), and its `subf` gets no r3 suggestion (`subf r0,r3,r30`). Arm 2
// re-extends and increments `edge` itself (`extsb r30,r30; addi r30,r30,1`); the volatile launder
// after the size_y call keeps those two below the call (edge crosses calls, so the scheduler has no
// anti-dependence to hold them there) and keeps the (s8) from folding away.
int pzlPlayer::movePiece()
{
    pzlPiece* p = m_inhand;
    int ret = 0;
    JOY* joy = Joy;
    int dir;

    {
        if (Key.rep & 0x08000000) {
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->m_pos_y -= 0.5f;
            }
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->m_pos_x -= 0.5f;
            } else {
                p->m_pos_x -= 1.0f;
            }
            ret = 1;
        } else if (Key.rep & 0x04000000) {
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->m_pos_y += 0.5f;
            }
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->m_pos_x += 0.5f;
            } else {
                p->m_pos_x += 1.0f;
            }
            ret = 1;
        } else if (Key.rep & 0x01000000) {
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->m_pos_x -= 0.5f;
            }
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->m_pos_y -= 0.5f;
            } else {
                p->m_pos_y -= 1.0f;
            }
            ret = 1;
        } else if (Key.rep & 0x02000000) {
            if ((f32) (int) p->ver0_x() != p->ver0_x()) {
                p->m_pos_x += 0.5f;
            }
            if ((f32) (int) p->ver0_y() != p->ver0_y()) {
                p->m_pos_y += 0.5f;
            } else {
                p->m_pos_y += 1.0f;
            }
            ret = 1;
        } else if (joy->trg & 0x20) {
            p->rotate(0);
            if (fabsf((f32) (s8) p->size_y()) > (f32) (m_p_active_board->m_size_y + 2)) {
                p->rotate(0);
            }
            ret = 2;
            goto cursor;
        } else if (joy->trg & 0x40) {
            p->rotate(1);
            if (fabsf((f32) (s8) p->size_y()) > (f32) (m_p_active_board->m_size_y + 2)) {
                p->rotate(1);
            }
            ret = 2;
            goto cursor;
        } else if (joy->trg & 0x00C00000) {
            p->mirror(1);
            ret = 2;
            goto cursor;
        } else if (joy->trg & 0x00300000) {
            p->mirror(0);
            ret = 2;
            goto cursor;
        }
        if (Key.rep & 0x0F000000) {
            int out = m_p_active_board->outPiece(p) == 1;
            int wall = m_p_active_board->ckInsideWall(p) == 0;
            if (out || wall) {
                dir = 0;
                if (wall) {
                    switch (m_p_active_board->m_wall_miss_flag) {
                    case 0:
                        dir = 0;
                        break;
                    case 1:
                        dir = 1;
                        break;
                    case 2:
                        dir = 2;
                        break;
                    case 3:
                        dir = 3;
                        break;
                    case 4:
                        dir = 4;
                        break;
                    }
                } else if (out) {
                    switch (m_p_active_board->m_out_miss_flag) {
                    case 0:
                        break;
                    case 1:
                        dir = 1;
                        break;
                    case 2:
                        dir = 2;
                        break;
                    case 3:
                        dir = 3;
                        break;
                    case 4:
                        dir = 4;
                        break;
                    }
                }
                if (dir == 1 || dir == 2) {
                    int edge;
                    int h;
                    f32 fy;
                    pzlBoard* cb = m_board;
                    f32 ny = 0.0f;
                    if (m_p_active_board == cb) {
                        m_p_active_board = m_space;
                        ny = p->m_pos_y - -2.0f;
                    } else if (m_p_active_board == m_space) {
                        m_p_active_board = cb;
                        ny = p->m_pos_y + -2.0f;
                    }
                    p->m_pos_y = ny;
                    if (fabsf((f32) (s8) p->size_y()) > (f32) (m_p_active_board->m_size_y + 2)) {
                        p->rotate(1);
                        p->snap();
                    }
                    if (dir == 1) {
                        edge = m_p_active_board->m_size_x;
                        if (p->size_x() > 0) {
                            edge -= (int) (fabsf((f32) (s8) p->size_x()) - 1.0f);
                        }
                    } else {
                        edge = -1;
                        if (p->size_x() < 0) {
                            edge = (int) (fabsf((f32) (s8) p->size_x()) - 1.0f) - 1;
                        }
                    }
                    p->set_ver0_x((f32) edge);
                    if (p->size_y() < 0) {
                        f32 vy = p->ver0_y();
                        int ch = m_p_active_board->m_size_y;
                        if (vy > (f32) m_p_active_board->m_size_y) {
                            p->set_ver0_y((f32) ch);
                        }
                        fy = p->ver0_y() + (f32) (p->size_y() + 1);
                        if (fy < -1.0f) {
                            h = (int) (fabsf((f32) (s8) p->size_y()) - 1.0f) - 1;
                            p->set_ver0_y((f32) h);
                        }
                    } else {
                        fy = p->ver0_y() + (f32) (p->size_y() - 1);
                        edge = m_p_active_board->m_size_y;
                        if (fy > (f32) m_p_active_board->m_size_y) {
                            int sy = p->size_y();
                            asm volatile("" : "+r"(edge)); // COMPILER-DIFF: the target re-extends edge after the size_y call (extsb r30,r30; addi r30,r30,1); ours proves it sign-extended and, edge crossing calls, would hoist the two above the call
                            edge = (s8) edge;
                            edge += 1;
                            h = edge - sy;
                            p->set_ver0_y((f32) h);
                        }
                        if (p->ver0_y() < -1.0f) {
                            p->set_ver0_y(-1.0f);
                        }
                    }
                    if (m_p_active_board->outPiece(p) == 1) {
                        if (fabsf((f32) (s8) p->size_x()) == 1.0f && fabsf((f32) (s8) p->size_y()) == 1.0f) {
                            switch (dir) {
                            case 1:
                                p->m_pos_x -= 1.0f;
                                break;
                            case 2:
                                p->m_pos_x += 1.0f;
                                break;
                            }
                        }
                        if (fabsf((f32) (s8) p->size_y()) == 1.0f && m_p_active_board->outPiece(p) == 1) {
                            int step = -1;
                            if (p->m_pos_y < (f32) (s8) (m_p_active_board->m_size_y / 2)) {
                                step = 1;
                            }
                            do {
                                p->m_pos_y += (f32) step;
                            } while (m_p_active_board->outPiece(p) == 1);
                        }
                        if (fabsf((f32) (s8) p->size_x()) == 1.0f && m_p_active_board->outPiece(p) == 1) {
                            int step = -1;
                            if (p->m_pos_x < (f32) (s8) (m_p_active_board->m_size_x / 2)) {
                                step = 1;
                            }
                            do {
                                p->m_pos_x += (f32) step;
                            } while (m_p_active_board->outPiece(p) == 1);
                        }
                    }
                } else {
                    if (fabsf((f32) (s8) p->size_y()) == 1.0f) {
                        if (dir == 3) {
                            do {
                                p->m_pos_y += 1.0f;
                            } while (m_p_active_board->outPiece(p) == 0);
                            p->m_pos_y -= 1.0f;
                        } else {
                            do {
                                p->m_pos_y -= 1.0f;
                            } while (m_p_active_board->outPiece(p) == 0);
                            p->m_pos_y += 1.0f;
                        }
                    } else {
                        if (dir == 3) {
                            do {
                                p->m_pos_y += 1.0f;
                            } while (m_p_active_board->ckInsideWall(p) == 1);
                            p->m_pos_y -= 1.0f;
                        } else {
                            do {
                                p->m_pos_y -= 1.0f;
                            } while (m_p_active_board->ckInsideWall(p) == 1);
                            p->m_pos_y += 1.0f;
                        }
                    }
                }
            }
        }
    }
cursor:
    m_p_active_board->m_cur_x = (s8) (p->m_pos_x + 0.5f);
    m_p_active_board->m_cur_y = (s8) (p->m_pos_y + 0.5f);
    if (m_p_active_board->m_cur_x & 0x80) {
        m_p_active_board->m_cur_x = 0;
    }
    {
        int wm = m_p_active_board->m_size_x - 1;
        if (m_p_active_board->m_cur_x > wm) {
            m_p_active_board->m_cur_x = wm;
        }
    }
    if (m_p_active_board->m_cur_y & 0x80) {
        m_p_active_board->m_cur_y = 0;
    }
    {
        int hm = m_p_active_board->m_size_y - 1;
        if (m_p_active_board->m_cur_y > hm) {
            m_p_active_board->m_cur_y = hm;
        }
    }
    return ret;
}

// Drops the pieces whose item was used up (ItemWork flags 0): removed from their board, model freed.
void pzlPlayer::rehash()
{
    int i;

    for (i = 0; i < m_piece_max; i++) {
        pzlPiece* p = &m_piece[i];
        if (p->isEmpty()) {
            continue;
        }
        if (p->item->flags == 0) {
            if (p->isOnBoard()) {
                if (m_board->search(p)) {
                    m_board->rmPiece(p);
                } else if (m_space->search(p)) {
                    m_space->rmPiece(p);
                }
            }
            p->model->push();
            p->clear();
        }
    }
}

// Remembers the current board and cursor.
void pzlPlayer::saveCursor()
{
    m_board_sav = m_p_active_board;
    m_cur_x_sav = m_p_active_board->m_cur_x;
    m_cur_y_sav = m_p_active_board->m_cur_y;
}

// Restores the board and cursor saved by saveCursor.
void pzlPlayer::loadCursor()
{
    m_p_active_board = m_board_sav;
    m_p_active_board->m_cur_x = m_cur_x_sav;
    m_p_active_board->m_cur_y = m_cur_y_sav;
}

// If the cursor was on the spare board, moves it to the case at (0, 0).
void pzlPlayer::salvCursor()
{
    if (m_space == m_p_active_board) {
        m_p_active_board = m_board;
        m_p_active_board->m_cur_x = 0;
        m_p_active_board->m_cur_y = 0;
    }
}

// `num` is u16 (its copies into `rest` are plain moves and cse propagates `num` into the peeled
// first order entry, so jump2 cannot cross-jump the peeled head), `max` u16 (shorten_compare gives the
// unsigned compares); `item` is declared before `info`; each loop has its own counter (the two
// placement nests share i/j); the fill-up loop is a guarded do-while (`cmpwi nOrder,0; ble`).
// `last` is r31 in the target = an allocno that crosses a call; the three COMPILER-DIFF lines below
// give it a codeless def before get() (see docs/research/ "DOL puzzle final closer").
int PutInCase(ITEM_ID item_id, u16 item_num, int size)
{
    ItemWork item;
    ItemInfo info;
    pzlPlayer* pl;
    u16 max;
    int total;
    int i;
    int j;
    int ok = 0;
    pzlPiece* p;
    ItemWork* last;

    itemInfo(item_id, &info);
    if (info.type == 1 || info.type == 9) {
        if (item_num == 0) {
            item_num = 1;
        }
        max = 1;
    } else {
        if (item_num == 0) {
            item_num = info.defNum;
        }
        max = info.maxNum;
    }
    if (item_num > max) {
        item_num = max;
        pLog->err(0, 0, "PutInCase(): Volume of ITEM(0x%02x) is OOL.", item_id);
    }
    ItemMgr.ordering(item_id);
    total = 0;
    for (int i = 0; i < ItemMgr.m_order_tbl_num; i++) {
        total += max - ItemMgr.m_p_order_tbl[i].p_item->num;
    }
    if (total >= item_num) {
        u16 rest = item_num;
        for (int i = 0; i < ItemMgr.m_order_tbl_num; i++) {
            ItemWork* w = ItemMgr.m_p_order_tbl[i].p_item;
            u16 room = max - w->num;
            if (room >= rest) {
                w->num = rest + w->num;
                break;
            }
            w->num = max;
            rest -= room;
        }
        return 1;
    }
    pl = (pzlPlayer*) __builtin_new(sizeof(pzlPlayer));
    if (pl == 0) {
        return 0;
    }
    if (pl->init(size) == 0) {
        __builtin_delete(pl);
        return 0;
    }
    ItemMgr.construct(&item, item_id);
    item.num = item_num;
    item.flags |= 1;
    pl->appendExtraPiece(&item);
    pl->inHandExtraPiece();
    {
        p = pl->m_extra;
        s8 bh = pl->m_board->m_size_y;
        s8 bw = pl->m_board->m_size_x;
        for (i = 0; i < bh; i++) {
            for (j = 0; j < bw; j++) {
                p->set_ver0_x((f32) j);
                p->set_ver0_y((f32) i);
                if (pl->putPiece(pl->m_board)) {
                    ok = 1;
                    goto placed;
                }
            }
        }
        p->orientation(1);
        for (i = 0; i < bh; i++) {
            for (j = 0; j < bw; j++) {
                p->set_ver0_x((f32) j);
                p->set_ver0_y((f32) i);
                if (pl->putPiece(pl->m_board)) {
                    ok = 1;
                    goto placed;
                }
            }
        }
    }
placed:
    pl->save();
    last = (ItemWork*) p; // COMPILER-DIFF: codeless copy of the dead r31 value: `last` then crosses the get() call and takes the first callee-saved reg, r31
    asm("" : "+r"(p)); // COMPILER-DIFF: makes the copy unavailable to gcse's copy propagation; flow deletes it (p is dead)
    if (ok) {
        u16 rest;
        int n;
        ItemMgr.ordering(item_id);
        rest = item_num;
        n = 0;
        if (ItemMgr.m_order_tbl_num > 0) {
            do {
                ItemWork* w = ItemMgr.m_p_order_tbl[n].p_item;
                u16 room = max - w->num;
                w->num = max;
                rest -= room;
                n++;
            } while (n < ItemMgr.m_order_tbl_num);
        }
        ItemMgr.get(item_id, rest);
        asm("" : "=m"(item.x) : "r"(last)); // COMPILER-DIFF: keeps the copy live across the call (an output-less asm is volatile and flushes cse's ItemMgr high)
        last = ItemMgr.newbie();
        if (last) {
            last->x = item.x;
            last->y = item.y;
            last->orient = item.orient;
            last->board = item.board;
        }
    }
    pl->quit();
    __builtin_delete(pl);
    return ok;
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
