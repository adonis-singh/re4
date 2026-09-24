#ifndef SS_MAIN_H
#define SS_MAIN_H

#include "types.h"
#include "sscrn.h"
#include "widget.h"
#include "id_sys.h"
#include "model.h"
#include "examine.h"

// Sscrn module (the sub screen DLL, src/Sscrn/ss_*.cpp): declarations shared by its units.
// ss_main.cpp owns the task, the common id/model/light helpers and the exit/examine widgets.

class cLit;


// game/sscrn.cpp id systems of the sub screen (sub screen ids / number digits).
extern IDSystem IdSub;
extern IDSystem IdNum;

// Every unit's linkonce block has Widget<SUB_SCREEN>::~Widget right after the cManager<cLight>
// copies, before the unit's own (synthesized) widget destructors and before Widget::quit/init/move:
// the destructor is instantiated by this header, before any derived class is declared (the other
// three at their first use in the unit).
static inline void ssWidgetDelete(Widget<SUB_SCREEN>* w)
{
    delete w;
}

// The screen widgets. SubScreenTask (ss_main.cpp) creates every screen's Init/Main pair and wires
// their link tables, so all of them are declared here; each unit defines its own virtuals (its key
// function owns the vtable). The link count is the base constructor argument: the module was
// built with -fno-implement-inlines (config/G4BE08/modules.py CFLAGS), so these in-class
// constructors are inlined at the `new` and never emitted out of line. Vtables are emitted in
// reverse declaration order per unit: keep each unit's classes in this order.

// ss_main.cpp
class SsExitInit : public Widget<SUB_SCREEN> {
public:
    int _rno;  // 0x10

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

class SsExitMain : public Widget<SUB_SCREEN> {
public:
    SsExitMain() : Widget<SUB_SCREEN>(0) {}
    virtual void move(SUB_SCREEN* pWk);
};

// Item examine screen (ss_main.cpp; ss_cap/ss_file/ss_item chain into it).
class SsItemExamine : public Widget<SUB_SCREEN> {
public:
    u8 _rno;          // 0x10
    u8 pad_11[3];
    ItemExamine _itemExam;  // 0x14

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_cap.cpp (bottle cap collection grid)
class SsCapInit : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

class CapSelect;

class SsCapMain : public Widget<SUB_SCREEN> {
public:
    int state;                 // 0x10
    CapSelect* sel;            // 0x14
    SsItemExamine* exam;       // 0x18
    Widget<SUB_SCREEN>* cur;   // 0x1C
    Widget<SUB_SCREEN>* next;  // 0x20

    SsCapMain() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

class CapSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10  0 none, 1 back, 2 exit

    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_file.cpp (files)
class SsFileInit : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

class FileSelect;
class MessageDisplay;

class SsFileMain : public Widget<SUB_SCREEN> {
public:
    int state;                 // 0x10
    int sndWait;               // 0x14
    int sndCnt;                // 0x18
    FileSelect* sel;           // 0x1C
    MessageDisplay* disp;      // 0x20
    Widget<SUB_SCREEN>* cur;   // 0x24
    Widget<SUB_SCREEN>* next;  // 0x28

    SsFileMain() : Widget<SUB_SCREEN>(5) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

class FileSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10  0 none, 1 back to the game, 2 main menu

    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

class MessageDisplay : public Widget<SUB_SCREEN> {
public:
    u8 state;     // 0x10  0 reading, 1 closing, 2 wait for the close animation
    u8 tplState;  // 0x11  picture: 0 shown, 1 request, 2 reading
    u8 tplFirst;  // 0x12  1 until the first picture was read
    u8 pad_13;
    s16 x;        // 0x14
    s16 y;        // 0x16

    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_item.cpp (inventory)
class SsItemInit : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

class ItemSelect;
class ItemCommand;
class ItemCombine;

class SsItemMain : public Widget<SUB_SCREEN> {
public:
    int state;                 // 0x10  0 item screen, 1 main menu
    ItemSelect* sel;           // 0x14
    ItemCommand* cmd;          // 0x18
    ItemCombine* comb;         // 0x1C
    SsItemExamine* exam;       // 0x20
    Widget<SUB_SCREEN>* cur;   // 0x24
    Widget<SUB_SCREEN>* next;  // 0x28

    SsItemMain() : Widget<SUB_SCREEN>(6) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_map.cpp (map)
class SsMapInit : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

class MapFocus;
class MapEntire;
class MapZoomIn;
class MapZoomOut;
class MapRead;
class MapModeSelect;

class SsMapMain : public Widget<SUB_SCREEN> {
public:
    int state;                 // 0x10  0 map, 1 main menu, 2 loading
    int step;                  // 0x14  0 request the area data, 1 wait, 2 running
    int readReq;               // 0x18
    MapFocus* focus;           // 0x1C
    MapEntire* entire;         // 0x20
    MapZoomIn* zoomIn;         // 0x24
    MapZoomOut* zoomOut;       // 0x28
    MapRead* read;             // 0x2C
    MapModeSelect* modeSel;    // 0x30
    Widget<SUB_SCREEN>* cur;   // 0x34

    SsMapMain() : Widget<SUB_SCREEN>(5) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_pzzl.cpp (attache case puzzle)
class SsPzzlInit : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    // in-class: the body is queued right after the synthesized dtor (eof order dtor, init)
    virtual void init(SUB_SCREEN* wk) { state = 0; }
    virtual void move(SUB_SCREEN* pWk);
};

class PzzlThinking;
class PiecePopUp;
class PiecePopDown;
class PieceSelect;
class PieceCombine;
class PieceCommand;
class CaseChange;

class SsPzzlMain : public Widget<SUB_SCREEN> {
public:
    int state;                 // 0x10  0 puzzle, 1 main menu
    int caseMove;              // 0x14  1 on the first frame (case model at the opening position)
    PzzlThinking* thinking;    // 0x18
    PiecePopUp* popUp;         // 0x1C
    PiecePopDown* popDown;     // 0x20
    PieceSelect* select;       // 0x24
    PieceCommand* command;     // 0x28
    PieceCombine* combine;     // 0x2C
    SsItemExamine* exam;       // 0x30
    CaseChange* caseChange;    // 0x34
    Widget<SUB_SCREEN>* cur;   // 0x38
    Widget<SUB_SCREEN>* next;  // 0x3C

    SsPzzlMain() : Widget<SUB_SCREEN>(6) {}
    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_shop.cpp (merchant)
class SsShopInit : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

class PzzlThinking;
class PieceSelect;
class CaseChange;
class ShopTopMenu;
class SellMenuSelect;
class SellItemNum;
class SellConfirm;
class BuyMenuSelect;
class BuyItemNum;
class BuyConfirm;
class BuyPuzzleEnd;
class LvUpMenuSelect;
class LvUpItemSelect;
class LvUpConfirm;

class SsShopMain : public Widget<SUB_SCREEN> {
public:
    PzzlThinking* thinking;    // 0x10  (ss_pzzl.cpp widgets: case placement of a bought item)
    int x14;
    int x18;
    PieceSelect* select;       // 0x1C
    CaseChange* caseChange;    // 0x20
    ShopTopMenu* topMenu;      // 0x24
    SellMenuSelect* sellSel;   // 0x28
    SellItemNum* sellNum;      // 0x2C
    SellConfirm* sellConf;     // 0x30
    BuyMenuSelect* buySel;     // 0x34
    BuyItemNum* buyNum;        // 0x38
    BuyConfirm* buyConf;       // 0x3C
    BuyPuzzleEnd* buyEnd;      // 0x40
    LvUpMenuSelect* lvSel;     // 0x44
    LvUpItemSelect* lvItem;    // 0x48
    LvUpConfirm* lvConf;       // 0x4C
    Widget<SUB_SCREEN>* cur;   // 0x50
    Widget<SUB_SCREEN>* next;  // 0x54

    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);
};

// ss_term.cpp (the radio / codec call screen: Hunnigan and the partner models talk through the
// op/opNN.das message sequences)
class SsTermInit : public Widget<SUB_SCREEN> {
public:
    int _counter;
    int _rno;  // 0x14  starts at 2

    virtual void init(SUB_SCREEN* pWk);
    virtual void move(SUB_SCREEN* pWk);
};

// One entry of the op message sequence table (0x10 bytes).
struct TermSeq {
    u16 x0;
    s16 x2;    // 0x02  copied to SsTermMain::TermSub::x14
    int time;  // 0x04  frame the entry fires at
    int mesNo; // 0x08  message number (-1: wait for the message end)
    int arg;   // 0x0C  message clear time; -1 ends the sequence
};

// One op number of ss_term's op table (SsTermMain::OpeMesTblInit, 0x14 bytes, 24 entries in .data).
struct TermOpe {
    int mdtNo;  // 0x00  0x8C ..
    void* seq;  // 0x04  TermSeq table
    void* mes;  // 0x08  message data (MesData type 2)
    void* xC;
    void* x10;
};

class SsTermMain : public Widget<SUB_SCREEN> {
public:
    // The op message player (memset at init).
    struct TermOpeWork {
        u32 flags;    // 0x1C  0x08000000 voice stream started, 0x10000000 sequence ended / skipped
        u32 str;      // 0x20  SndStrReq handle
        int mesNo;    // 0x24
        int seqIdx;   // 0x28
        int mesWait;  // 0x2C  frames until the message is cleared
        int seqCnt;   // 0x30  frame counter
        int wait;     // 0x34  frames before the op starts (0x1E)
        int mdtNo;    // 0x38
        TermSeq* seq; // 0x3C
        void* mes;    // 0x40
        int x44;
        int _rno;
    };
    struct TermSub {
        u8 pad_0[0x14];
        int x14;      // 0x60  TermSeq::x2 of the last entry
        int x18;      // 0x64  TermSeq::mesNo of the last entry
        int count;    // 0x68  messages set / cleared
        u8 pad_20[0x40 - 0x20];  // sizeof == 0x40 (SsTermMain is 0x8C: SubScreenTask's `li r3, 0x8c`)
    };

    int x10;          // 0x10
    int modelOn;      // 0x14  models are set up
    int ended;        // 0x18  end pose set
    TermOpeWork ope;  // 0x1C
    TermSub sub;      // 0x4C .. 0x8C

    virtual void init(SUB_SCREEN* pWk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* pWk);

    void OpeMesTblInit(SUB_SCREEN* wk);
    void OpeMdtSet();
    void OpeMdtSetNo(int no);
    void OpeMdtSetSub(int mdtNo, void* seq, void* mes);
    int OpeMesMove();
    int OpeSeqMove(TermSeq* s);
    void OpeMesSet(int no, int wait);
    void OpeMesClear();
    void OpeSndStrStop();
};

// The DLL's own model managers (ss_main.cpp; the DOL's PartsMgr/ModInfoMgr are swapped out while the sub
// screen is open).
extern cPartsMgr ssPartsMgr;
extern cModInfoMgr ssModInfoMgr;

// ss_map.cpp: the sub screen's character model (MapMgr work 0), its weapon model (work 1), the
// motion-driven flag of the character and the optional second weapon model (both .data, zeroed at
// map init; the weapon change task fades / animates them).
extern cModel* ssPlModel;
extern cModel* ssWepModel;
extern cModel* ssPlMotion;
extern cModel* ssWepModel2;

extern "C" {
// ss_main.cpp
void IdSubErase();
void IdNumErase();
void sscrnModelClear(SUB_SCREEN* wk);
void sscrnLightClear(SUB_SCREEN* wk);
void sscrnLightCreate(SUB_SCREEN* wk, cLit* lit);
void sscrnMainMenuInit(SUB_SCREEN* wk, int no);
void numDisp(int id, int num, Vec* pos, u32 flags);
void sscrnCameraInit(SUB_SCREEN* wk, CAMERA* cam);
void sscrnModelFree(SUB_SCREEN* wk);
void generalModelAlloc(SUB_SCREEN* wk);
int sscrnMainMenu(SUB_SCREEN* wk);
int sscrnKey2Game(SUB_SCREEN* wk);
// ss_debug.cpp
void SscrnDebugMenu(SUB_SCREEN* wk);
// ss_item_draw.cpp (OT primitives: texture, 3D line, 3D tile)
void ss_Draw_tpl(void* tpl, u32 id, int x, int y, int w, int h, int ot, int prio);
void ss_Draw_tpl_local(struct TEXPalette* tpl, u32 id, int x, int y, int w, int h);
void ss_Draw_line3d(Vec* a, Vec* b, u32 color, int width, int blend, int zupd, int ot, int prio);
void ss_Draw_line3d_local(Vec* a, Vec* b, Mtx mtx, u32 color, u32 blend, int zupd);
void ss_Draw_tile3d(Vec* a, Vec* b, Vec* c, Vec* d, u32 color, int x34, int blend, int ot, u16 prio);
void ss_Draw_tile3d_local(Vec* a, Vec* b, Vec* c, Vec* d, Mtx mtx, u32 color, u32 blend, int zupd);
// ss_pzzl.cpp
void pieceModelInit(SUB_SCREEN* wk);
// ss_model.cpp
void weaponFilename(char* name, int no);
void playerModelInit();
void leonModelInit(u16 no, u16 type);
void ashleyModelInit();
void adaModelInit(u16 no, u16 type);
void klauserModelInit(u16 no, u16 type);
void hunkModelInit(u16 no, u16 type);
void weskerModelInit(u16 no, u16 type);
void tel00ModelInit(cModel* m, SsArc* arc);
void hunniganModelInit(cModel* m, void* data, u32 type);
// ss_main.cpp helpers the screens share
void clearZbuffer();
void dispScrollBar(u32 top, u32 n, u32 num, IdUnit* bar, IdUnit* up, IdUnit* down);
void idMainMenuFade(SUB_SCREEN* wk, int sw);
void weaponChangeRequest(u16 no, u16 type);
}

#endif
