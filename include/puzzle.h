#ifndef PUZZLE_H
#define PUZZLE_H

#include "types.h"
#include "vec.h"
#include "item.h"

// Attache case packing puzzle (game/puzzle.cpp): pieces on a grid board.
struct ItemWork;
class cModel;

// Piece shape data (piece_info entry + 4).
struct PieceData {
    s8 size_x;             // 0x00
    s8 size_y;             // 0x01
    u8 pad_2[2];
    f32 center_x;           // 0x04  centre offset in cells
    f32 center_y;           // 0x08
    char shape[0x40]; // 0x0C  row-major, '1' = filled
};

// One piece_info entry (0x78 bytes; the table ends with id 0xFFFF).
struct PieceInfo {
    u16 id;           // 0x00  item id
    u16 pad_2;
    PieceData data;   // 0x04
    u8 model[0x28];   // 0x50  model data (searchItemModelData)
};

class pzlPiece {
private:
    u8 be_flag;         // 0x00  bit0 in use
    u8 pad_1[3];
public:
    PieceData* m_p_data;  // 0x04
    u32 x8;           // 0x08
    f32 m_center_x;           // 0x0C  rotated centre offset
    f32 m_center_y;           // 0x10
    f32 m_pos_x;            // 0x14  centre position on the board (cells)
    f32 m_pos_y;            // 0x18
    u32 x1C;
    s8 m_orientation;        // 0x20  0..3 rotations, 4..7 mirrored
private:
    u8 m_place;         // 0x21  1 = on a board, 2 = in hand
    u8 pad_22[2];
public:
    ItemWork* item;   // 0x24
    cModel* model;    // 0x28

    void orientation(int orientation_no);
    void rotate(int dir);
    void mirror(int dir);
    void init(PieceData* p_data);
    void clear() { be_flag = 0; }
    f32 ver0_x();
    f32 ver0_y();
    void set_ver0_x(f32 x) { m_pos_x = x + m_center_x; }
    void set_ver0_y(f32 y) { m_pos_y = y + m_center_y; }
    int size_x();
    int size_y();
    void snap();
    int shape(int x, int y);
    int isAlive() { return be_flag & 1; }
    int isEmpty() { return !(be_flag & 1); }
    int isOnBoard() { return m_place & 1; }
    int isInHand() { return m_place & 2; }
    void setNoPlace() { m_place = 0; }
    void setOnBoard() { m_place = 1; }
    void setInHand() { m_place = 2; }
};

class pzlBoard {
private:
    u8* m_cell;        // 0x00  w * h state bytes (bit0 occupied, bit1 inside, bit6 wall)
public:
    // read directly by two float compares in pzlPlayer::movePiece: through size_y() the
    // conversion stores the sign-extended register instead of the loaded byte
    s8 m_size_x;             // 0x04
    s8 m_size_y;             // 0x05
private:
    u8 m_piece_max;      // 0x06
    u8 pad_7;
    pzlPiece** m_p_piece;// 0x08
    Mtx m_mat;          // 0x0C  board -> world matrix (Sscrn ss_pzzl caseModelMove)
public:
    s8 m_cur_x;          // 0x3C  cursor
    s8 m_cur_y;          // 0x3D
    s8 m_wall_miss_flag;       // 0x3E  ckInsideWall result side (1 left, 2 right, 3 up, 4 down)
    s8 m_out_miss_flag;        // 0x3F  outPiece result side

    int init(int w, int h, int pieceMax);
    void quit();
    Mtx* orientation() { return &m_mat; }
    int getPieceNum();
    int search(pzlPiece* p);
    int ckInsideWall(pzlPiece* p_piece);
    int outPiece(pzlPiece* p_piece);
    int putPiece(pzlPiece* p_piece);
    pzlPiece* lapPiece(pzlPiece* p_piece);
    pzlPiece* getPiece(int x, int y);
    int rmPiece(pzlPiece* p_piece);
    pzlPiece* rmPiece(int x, int y);
    u8* cell(int x, int y);
    int cellState(int x, int y);
    void clearState(u8 state);
    s8 size_x() { return m_size_x; }
    s8 size_y() { return m_size_y; }
};

class pzlPlayer {
private:
    pzlBoard* m_board;   // 0x00
    pzlBoard* m_space;  // 0x04
    pzlPiece* m_piece;      // 0x08
    u8 m_piece_max;          // 0x0C
    u8 pad_D[3];
    pzlPiece* m_inhand;        // 0x10
    pzlPiece* m_extra;       // 0x14
    f32 m_piece_bak_pos_x;             // 0x18
    f32 m_piece_bak_pos_y;             // 0x1C
    s8 m_piece_bak_orientation;         // 0x20
    u8 pad_21[3];
    pzlBoard* m_piece_bak_board;   // 0x24
    pzlBoard* m_board_sav;   // 0x28
    s8 m_cur_x_sav;              // 0x2C
    s8 m_cur_y_sav;              // 0x2D
    u8 pad_2E[2];
public:
    pzlBoard* m_p_active_board;         // 0x30

    int init(int size);
    void quit();
    pzlBoard* boardPtr() { return m_board; }
    pzlBoard* spacePtr() { return m_space; }
    int pieceNum();
    int pieceMax() { return m_piece_max; }
    pzlPiece* pieceAt(int no) { return &m_piece[no]; }
    pzlPiece* piecePtr(int no);
    pzlPiece* piecePtr(ItemWork* item);
    pzlPiece* pieceInHand() { return m_inhand; }
    pzlPiece* pieceExtra() { return m_extra; }
    void save();
    int appendExtraPiece(ItemWork* pItem);
    int removeExtraPiece();
    void inHandExtraPiece();
    void giveupExtraPiece();
    int selPiece(pzlBoard* b);
    pzlPiece* ptrPiece(pzlBoard* b);
    void getPiece(pzlBoard* b);
    int putPiece(pzlBoard* b);
    int relPiece(pzlBoard* b);
    int chgPiece(pzlBoard* b);
    pzlPiece* cmbPiece(pzlBoard* b);
    int movePiece();
    void rehash();
    void saveCursor();
    void loadCursor();
    void salvCursor();
    int selPiece() { return selPiece(m_p_active_board); }
    pzlPiece* ptrPiece() { return ptrPiece(m_p_active_board); }
    void getPiece() { getPiece(m_p_active_board); }
    int putPiece() { return putPiece(m_p_active_board); }
    int relPiece() { return relPiece(m_p_active_board); }
};

extern PieceInfo piece_info[];

extern "C" {
PieceData* searchItemPieceData(int item_id, PieceInfo* p_info);
u8* searchItemModelData(int item_id, PieceInfo* p_info);
int PutInCase(ITEM_ID item_id, u16 item_num, int size);
}

#endif
