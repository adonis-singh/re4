#ifndef SS_PZZL_H
#define SS_PZZL_H

#include "types.h"
#include "ss_main.h"

// Sscrn ss_pzzl.cpp: the attache case puzzle widgets. ss_pzzl.cpp owns their vtables (key
// functions); ss_shop.cpp creates PzzlThinking / PieceSelect / CaseChange for the case placement
// of a bought item. Declaration order = the reverse of ss_pzzl's vtable order (CaseChange lowest).
struct IdUnit;
class pzlPiece;

class PiecePopUp : public Widget<SUB_SCREEN> {
public:
    int count;  // 0x10

    virtual void init(SUB_SCREEN* wk) { count = 0; }  // in-class: eof order dtor, init
    virtual void move(SUB_SCREEN* pWk);
};

class PiecePopDown : public Widget<SUB_SCREEN> {
public:
    int count;  // 0x10

    virtual void init(SUB_SCREEN* wk) { count = 0; }  // in-class: eof order dtor, init
    virtual void move(SUB_SCREEN* pWk);
};

class PzzlThinking : public Widget<SUB_SCREEN> {
public:
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

class PieceSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10  0 select, 1 message, 2 message closed
    int mode;   // 0x14  bit3: message open; 1 exit, 2 main menu, 4 case change

    PieceSelect() : Widget<SUB_SCREEN>(4) {}
    virtual void init(SUB_SCREEN* wk) { state = 0; }  // in-class: eof order dtor, init
    virtual void move(SUB_SCREEN* pWk);
};

class PieceCommand : public Widget<SUB_SCREEN> {
public:
    IdUnit* id[16];   // 0x10
    IdUnit* sub[11];  // 0x50
    u8 pad_7C[0x90 - 0x7C];
    int mode;         // 0x90  0 command, 1 open sub menu, 2 sub menu, 3 message
    s8 num;           // 0x94
    u8 cursorOld;     // 0x95
    s8 subSel;        // 0x96
    s8 inSpace;       // 0x97  1: the piece is not on the case board (setCommandId's space set)
    u8 lower;         // 0x98  the piece is in the lower half (menu above it)

    PieceCommand() : Widget<SUB_SCREEN>(6) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

class PieceCombine : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    PieceCombine() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

class CaseChange : public Widget<SUB_SCREEN> {
public:
    IdUnit* a;  // 0x10
    IdUnit* b;  // 0x14

    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

extern "C" {
// ss_pzzl.cpp helpers the shop screen shares
void puzzleCameraInit(SUB_SCREEN* wk, CAMERA* cam);
void pieceModelDisp(SUB_SCREEN* wk);
void pieceModelSet(pzlPiece* p);
void pzzlCursorDisp(SUB_SCREEN* wk, int sw);
void caseModelMove(int sw);
void tempSpaceDisp(int sw);
}
// ss_pzzl.cpp debug: piece index / position the debug menu edits (ss_debug.cpp)
extern int pzzlDbgNo;
extern f32 pzzlDbgPos;

#endif
