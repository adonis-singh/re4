// Sscrn/ss_term: the codec call screen of the sub screen DLL (D:/Bio4/Prog/ss_term.cpp): Hunnigan
// / the partner and the player model talk through the op/opNN.das message sequences. Also carries a
// leftover debug button window (cDbgWindow) and a host file list (cFileList) nothing calls.
#include "types.h"
#include "global.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "sscrn.h"
#include "shape.h"

// Widget<SUB_SCREEN> is completed here (by the specialization declaration), before dbg_button.h:
// its vtable is the last one of the unit (vtables come out in reverse declaration order), after the
// cDbg* ones.
//
// COMPILER-DIFF: candidate #8 (end-of-file order: the round of Widget<SUB_SCREEN>::~Widget). The
// original outputs ~Widget in finish_file round 1, right after Widget::quit and before
// ~cDbgButtonBase / the cDbgWindowBase inlines, while its vtable is only written in round 2
// (nothing here constructs a widget). Our cc1plus outputs a comdat inline only once its assembler
// name has been referenced (decl2.c finish_file "stop lying"): ~Widget is inlined everywhere, so
// ours would wait for the round-2 vtable. This explicit specialization makes ss_main.h's
// ssWidgetDelete leave the destructor uninstantiated (saved_inlines position after quit) and lets
// the never-called ssTermWidgetKill below, compiled before the definition, emit a real
// `bl _._t6Widget...` that references the name; the body is defined inline after SsTermInit::move
// (same code as the template, inlined into the synthesized ~SsTermInit/~SsTermMain like the
// instantiation). The original REL link dead-stripped ssTermWidgetKill's body (modules.py
// STRIP_UNUSED).
template <> Widget<SUB_SCREEN>::~Widget();

// Never called; exists only to reference the Widget destructor (see the note above).
static void ssTermWidgetKill(Widget<SUB_SCREEN>* w)
{
    w->Widget<SUB_SCREEN>::~Widget();  // COMPILER-DIFF: candidate #8
}

#include "dbg_button.h"
#include "item.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "joy.h"
#include "snd.h"
#include "db_log.h"
#include "eprintf.h"
#include "file.h"
#include "dbmodule.h"
#include "camera.h"
#include "view.h"
#include "model.h"
#include "motion.h"
#include "math_sub.h"

void* GetModelInfoAddr(cModelInfo* info, int no);

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

extern "C" {
u32 MakeCol(f32 r, f32 g, f32 b, f32 a);
void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void partnerDataName(char* name, int no);
int partnerType(int no);
void termMotionSet(void* data, int no);
void termMotionCancel(void* data, int no);
void termModelAlloc(SUB_SCREEN* wk);
void terminalCameraInit(SUB_SCREEN* wk, CAMERA* cam);
}

// Packs 0..1 float components into an ARGB8 colour word (debug drawing helper).
u32 MakeCol(f32 r, f32 g, f32 b, f32 a)
{
    u32 col = 0;

    col += (u8) (a * 255.0f) << 24;
    col += (u8) (r * 255.0f) << 16;
    col += (u8) (g * 255.0f) << 8;
    col += (u8) (b * 255.0f);
    return col;
}

// Filled screen-space rectangle in a 0..1 float colour (the debug button window's cursor box).
void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec pos;
    Vec size;

    pos.x = x;
    pos.y = y;
    size.x = w;
    size.y = h;
    Draw_quad(&pos, &size, MakeCol(r, g, b, a));
}

// Debug button window: up to 128 buttons on a character grid, a cursor moved with the pad.
class cDbgWindow : public cDbgWindowBase {
public:
    u32 m_nBut;               // 0x28
    cDbgButton* m_pButList[128];  // 0x2C
    cDbgButton* m_pCurrentBut;       // 0x22C
    cDbgButton* m_pStartBut;       // 0x230
    cDbgButton* m_pEndBut;    // 0x234

    virtual ~cDbgWindow() {
        u32 i;
        for (i = 0; i < m_nBut; i++) {
            if (m_pButList[i]) {
                delete m_pButList[i];
            }
        }
    }
    virtual int GetCx() {
        if (m_pCurrentBut) {
            return m_pCurrentBut->m_cx;
        }
        return 0;
    }
    virtual int GetCy() {
        if (m_pCurrentBut) {
            return m_pCurrentBut->m_cy;
        }
        return 0;
    }
    virtual void SetCurrentTopButton() { m_pCurrentBut = m_pStartBut; }
    virtual void SetCurrentBottomButton() { m_pCurrentBut = m_pEndBut; }
    virtual void ButtonAllUpdate() {
        u32 i;
        for (i = 0; i < m_nBut; i++) {
            cDbgButton* b = m_pButList[i];
            if (b && b->m_pFuncUpdate) {
                b->m_pFuncUpdate(b);
            }
        }
    }
    int AddButton(int bx, int by, const char* name, int bcx, int bcy);
    int FindButton(int cx, int cy, cDbgButton** out);
    virtual int LocalUpdate();
    virtual void LocalDisp();
};

// Never called: the original REL link dead-stripped the body (modules.py STRIP_UNUSED) and kept
// its string after MakeCol's pool. Compiled at parse time it inlines cDbgButton's implicit
// constructor, which references `_vt.10cDbgButton`: the vtable is then written in finish_file
// round 1 (fourth of the unit's vtables) and ~cDbgButton is the first end-of-file function, while
// `_vt.14cDbgButtonBase` / ~cDbgButtonBase wait for round 2 (the base vptr store is elided).
int cDbgWindow::AddButton(int bx, int by, const char* name, int bcx, int bcy)
{
    cDbgButton* b;

    m_pButList[m_nBut] = b = new cDbgButton;
    if (b == 0) {
        pLog->err(0, 0, "AddButton(): new failed.");
        return 0;
    }
    m_nBut++;
    return 1;
}

// Finds the button at grid cell (cx, cy); 1 and *out when found.
int cDbgWindow::FindButton(int cx, int cy, cDbgButton** out)
{
    u32 i;

    *out = 0;
    for (i = 0; i < m_nBut; i++) {
        cDbgButton* b = m_pButList[i];
        if (b->m_cx == cx && b->m_cy == cy) {
            *out = b;
            return 1;
        }
    }
    return 0;
}

// Debug button window input (pad 1 d-pad/stick, repeat): moves the cursor over the button grid with
// wrap-around, runs every button's update callback; returns 0 on B (close the window).
int cDbgWindow::LocalUpdate()
{
    int ret = 1;
    int cx = GetCx();
    int cy = GetCy();

    if (Joy[0].rep & 0x00010001) {
        cx--;
    }
    if (Joy[0].rep & 0x00020002) {
        cx++;
    }
    if (Joy[0].rep & 0x00080008) {
        cy--;
    }
    if (Joy[0].rep & 0x00040004) {
        cy++;
    }
    if (cx < 0) {
        cx = m_max_cx;
    }
    if (cy < 0) {
        cy = m_max_cy;
    }
    if (cx > m_max_cx) {
        cx = 0;
    }
    if (cy > m_max_cy) {
        cy = 0;
    }
    if (cx != GetCx() || cy != GetCy()) {
        cDbgButton* b;
        if (FindButton(cx, cy, &b)) {
            m_pCurrentBut = b;
        }
    }
    ButtonAllUpdate();
    if (Joy[0].trg & 0x200) {
        ret = 0;
    }
    return ret;
}

// Text row of a button (the window's row 0 is its title line).
static inline int dbgWindowRow(int y)
{
    return y + 1;
}

// Draws the buttons' labels, the blinking ">" before the current one and its highlight box.
void cDbgWindow::LocalDisp()
{
    u32 i;
    cDbgButton* c;

    for (i = 0; i < m_nBut; i++) {
        cDbgButton* b = m_pButList[i];
        eprintf2(8, 0xC, (m_px + b->m_px) * 8, (dbgWindowRow(m_py) + b->m_py) * 14, 0x10, 0, b->m_pStr);
    }
    c = m_pCurrentBut;
    if (c) {
        int wx = m_px;
        int wy = dbgWindowRow(m_py);
        if (pG->Frame_cnt & 4) {
            eprintf2(8, 0xC, (wx + c->m_px - 1) * 8, (wy + c->m_py) * 14, 0, 0, ">");
        }
        eprintf2(8, 0xC, (wx + c->m_px) * 8, (wy + c->m_py) * 14, 0, 0, c->m_pStr);
        {
            f32 px = (f32) ((wx + c->m_px) * 8);
            f32 py = (f32) ((wy + c->m_py) * 14);
            f32 pw = (f32) (c->m_strlen * 8);
            f32 ph = 14.0f;
            f32 bd = 2.0f;
            DbgDrawBoxFill(px - bd, py - bd, pw + 0.0f, ph + bd, 0.7f, 0.7f, 0.0f, 0.3f);
        }
    }
}

#include "ss_main.h"
#include <stdio.h>
#include <string.h>
#include "pl_mod.h"


// Host file list (d:\bio4\room\filelist.txt through the SN file server): a scrolling list of the
// names under one directory.
class cFileList {
public:
    char* filter;  // 0x00  prefix stripped from every name
    char* pattern; // 0x04  search pattern
    char* text;    // 0x08  file list text
    char** list;   // 0x0C  one pointer per line
    int num;       // 0x10
    s16 cursor;    // 0x14
    s16 top;       // 0x16

    // Empty: the file-scope instance below gives the unit its (empty) static init/destroy pair.
    cFileList() {}
    ~cFileList() {}
    void init();
    char* disp(int x, int y, int rows);
    int update();
    void dir(char* d, char* f);
};

// Host file list start: default directory (\bio4\data\*.*) and a first read (dir() reads two
// uninitialised locals here, as the original does).
void cFileList::init()
{
    char* d;
    char* f;

    text = 0;
    list = 0;
    cursor = 0;
    pattern = 0;
    filter = 0;
    dir(d, f);
    update();
}

// Scrolling list display at (x, y) with `rows` visible lines: up/down (fast repeat) move the
// cursor, stick up/down half a page; returns the selected name.
char* cFileList::disp(int x, int y, int rows)
{
    JOY* joy = &Joy[0];
    int end;
    int i;

    if (joy->rep2 & 0x000C000C) {
        if (joy->rep2 & 4) {
            cursor++;
        }
        if (joy->rep2 & 8) {
            cursor--;
        }
        if (joy->rep2 & 0x40000) {
            cursor += rows / 2;
        }
        if (joy->rep2 & 0x80000) {
            cursor -= rows / 2;
        }
        cursor = cursor < 0 ? 0 : (cursor > num - 1 ? num - 1 : cursor);
    }
    if (top < cursor - rows + 1) {
        top = cursor - rows + 1;
    }
    if (top > cursor) {
        top = cursor;
    }
    if (top + rows > num) {
        end = num;
    } else {
        end = top + rows;
    }
    for (i = top; i < end; i++) {
        int col = 0;
        if (i == cursor) {
            col = 6;
        }
        eprintf(x, y, col, 0, "%s", list[i]);
        y += 16;
    }
    return list[cursor];
}

// Re-reads d:\bio4\room\filelist.txt from the host, converts the backslashes, splits the CRLF
// lines into `list` (stripping `filter` from each). 0 when the file is missing.
int cFileList::update()
{
    char* p;
    int i;

    if (text) {
        Debug_free(text);
    }
    if (list) {
        Debug_free(list);
    }
    HDReadDebugAlloc("d:\\bio4\\room\\filelist.txt", (void**) &text, 1);
    num = 0;
    if (text == 0) {
        pLog->err(0, 0, "cFileList::update : file not found");
        return 0;
    }
    p = text;
    while ((p = strchr(p, '\\')) != 0) {
        *p = '/';
    }
    p = text;
    num = 0;
    while ((p = strchr(p, '\r')) != 0) {
        *p = 0;
        p++;
        num++;
    }
    list = (char**) Debug_alloc(num * 4, 1);
    p = text;
    for (i = 0; i < num; i++) {
        if (filter) {
            p = strstr(p, filter);
            p += strlen(filter);
        }
        list[i] = p;
        p += strlen(p);
        p += 2;
    }
    return 1;
}

// Sets the search pattern `d` and name prefix `f` (copied, backslashes converted); d == 0 gives the
// defaults \bio4\data\*.* and /bio4/data/.
void cFileList::dir(char* d, char* f)
{
    if (pattern) {
        Debug_free(pattern);
    }
    if (filter) {
        Debug_free(filter);
    }
    if (d == 0) {
        char defDir[15] = "\\bio4\\data\\*.*";
        pattern = (char*) Debug_alloc(strlen(defDir), 1);
        strcpy(pattern, defDir);
        char defFilter[12] = "/bio4/data/";
        filter = (char*) Debug_alloc(strlen(defFilter), 1);
        strcpy(filter, defFilter);
    } else {
        pattern = (char*) Debug_alloc(strlen(d), 1);
        strcpy(pattern, d);
        if (f) {
            char* p;
            filter = (char*) Debug_alloc(strlen(f), 1);
            strcpy(filter, f);
            p = filter;
            while ((p = strchr(p, '\\')) != 0) {
                *p = '/';
            }
        } else {
            filter = f;
        }
    }
}

// The op table: 24 ops, message / sequence data filled from op/opNN.das at init.
TermOpe term_ope_tbl[24] = {
    {0x8C}, {0x8D}, {0x8E}, {0x8F}, {0x90}, {0x91}, {0x92}, {0x93}, {0x94}, {0x95}, {0x96}, {0x97},
    {0x98}, {0x99}, {0x9A}, {0x9B}, {0x9C}, {0x9D}, {0x9E}, {0x9F}, {0xA0}, {0xA1}, {0xA2}, {0xA3},
};

// The archive inside op/opNN.das starts 0x400 bytes in. Read through an inline (not a macro on the
// member): the table stores may alias wk->pOpData, so the pointer is reloaded per statement, and
// `ofs + (u32) arc` is not reassociated with the +0x400.
static inline SsArc* opArc(SUB_SCREEN* wk)
{
    return (SsArc*) ((u8*) wk->pTermMes + 0x400);
}
#define OP_ARC_PTR(wk, no) SS_ARC_PTR(opArc(wk), no)

// Points the 24 op entries (term_ope_tbl) at their message and sequence blocks inside the loaded
// op/opNN.das archive: ops 0..12 use sub-files 4..16 / 17..29, 13..18 and 19..23 (the later
// stages' disc layout) reuse lower slots.
void SsTermMain::OpeMesTblInit(SUB_SCREEN* wk)
{
    term_ope_tbl[0].mes = OP_ARC_PTR(wk, 4);
    term_ope_tbl[1].mes = OP_ARC_PTR(wk, 5);
    term_ope_tbl[2].mes = OP_ARC_PTR(wk, 6);
    term_ope_tbl[3].mes = OP_ARC_PTR(wk, 7);
    term_ope_tbl[4].mes = OP_ARC_PTR(wk, 8);
    term_ope_tbl[5].mes = OP_ARC_PTR(wk, 9);
    term_ope_tbl[6].mes = OP_ARC_PTR(wk, 10);
    term_ope_tbl[7].mes = OP_ARC_PTR(wk, 11);
    term_ope_tbl[8].mes = OP_ARC_PTR(wk, 12);
    term_ope_tbl[9].mes = OP_ARC_PTR(wk, 13);
    term_ope_tbl[10].mes = OP_ARC_PTR(wk, 14);
    term_ope_tbl[11].mes = OP_ARC_PTR(wk, 15);
    term_ope_tbl[12].mes = OP_ARC_PTR(wk, 16);
    term_ope_tbl[13].mes = OP_ARC_PTR(wk, 4);
    term_ope_tbl[14].mes = OP_ARC_PTR(wk, 5);
    term_ope_tbl[15].mes = OP_ARC_PTR(wk, 6);
    term_ope_tbl[16].mes = OP_ARC_PTR(wk, 7);
    term_ope_tbl[17].mes = OP_ARC_PTR(wk, 8);
    term_ope_tbl[18].mes = OP_ARC_PTR(wk, 9);
    term_ope_tbl[19].mes = OP_ARC_PTR(wk, 4);
    term_ope_tbl[20].mes = OP_ARC_PTR(wk, 5);
    term_ope_tbl[21].mes = OP_ARC_PTR(wk, 6);
    term_ope_tbl[22].mes = OP_ARC_PTR(wk, 7);
    term_ope_tbl[23].mes = OP_ARC_PTR(wk, 8);
    term_ope_tbl[0].seq = OP_ARC_PTR(wk, 17);
    term_ope_tbl[1].seq = OP_ARC_PTR(wk, 18);
    term_ope_tbl[2].seq = OP_ARC_PTR(wk, 19);
    term_ope_tbl[3].seq = OP_ARC_PTR(wk, 20);
    term_ope_tbl[4].seq = OP_ARC_PTR(wk, 21);
    term_ope_tbl[5].seq = OP_ARC_PTR(wk, 22);
    term_ope_tbl[6].seq = OP_ARC_PTR(wk, 23);
    term_ope_tbl[7].seq = OP_ARC_PTR(wk, 24);
    term_ope_tbl[8].seq = OP_ARC_PTR(wk, 25);
    term_ope_tbl[9].seq = OP_ARC_PTR(wk, 26);
    term_ope_tbl[10].seq = OP_ARC_PTR(wk, 27);
    term_ope_tbl[11].seq = OP_ARC_PTR(wk, 28);
    term_ope_tbl[12].seq = OP_ARC_PTR(wk, 29);
    term_ope_tbl[13].seq = OP_ARC_PTR(wk, 10);
    term_ope_tbl[14].seq = OP_ARC_PTR(wk, 11);
    term_ope_tbl[15].seq = OP_ARC_PTR(wk, 12);
    term_ope_tbl[16].seq = OP_ARC_PTR(wk, 13);
    term_ope_tbl[17].seq = OP_ARC_PTR(wk, 14);
    term_ope_tbl[18].seq = OP_ARC_PTR(wk, 15);
    term_ope_tbl[19].seq = OP_ARC_PTR(wk, 9);
    term_ope_tbl[20].seq = OP_ARC_PTR(wk, 10);
    term_ope_tbl[21].seq = OP_ARC_PTR(wk, 11);
    term_ope_tbl[22].seq = OP_ARC_PTR(wk, 12);
    term_ope_tbl[23].seq = OP_ARC_PTR(wk, 13);
}

// Starts the op selected by the DOL (OpeMdtSetInit = the pending call number).
void SsTermMain::OpeMdtSet()
{
    OpeMdtSetNo(OpeMdtSetInit());
}

// Starts op `no` (0..23) of term_ope_tbl: its voice stream number, sequence and messages.
void SsTermMain::OpeMdtSetNo(int no)
{
    if (no > 0x17) {
        pLog->err(0, 0, "SsTermMain::OpeMdtSetNo [%d]", no);
    } else {
        OpeMdtSetSub(term_ope_tbl[no].mdtNo, term_ope_tbl[no].seq, term_ope_tbl[no].mes);
    }
}

// Resets the op player (TermOpeWork) onto a voice stream number, a TermSeq table and a message
// block (MesData slot 2): sequence index / counters to 0, stream not started.
void SsTermMain::OpeMdtSetSub(int mdtNo, void* seq, void* mes)
{
    ope.mdtNo = mdtNo;
    ope.seq = (TermSeq*) seq;
    ope.mes = mes;
    MesData.registData(2, (u8*) mes);
    ope.seqIdx = 0;
    ope.mesWait = 0;
    ope.flags &= ~0x08000000;
    ope.seqCnt = 0;
    ope.str = 0;
}

// Runs the op one frame: starts the voice stream (SndStrReq mdtNo) and waits for it to be ready,
// then plays the talking motions and shows the frame units; fires every TermSeq entry whose time
// has come (OpeSeqMove). B (Key bit 18) skips: the stream stops and A/B step the remaining entries
// by hand. Returns 1 when the sequence ended (arg == -1 entry).
int SsTermMain::OpeMesMove()
{
    if (ope.seqCnt == 0 && ope.str == 0) {
        if (ope.mdtNo != 0) {
            SndStrStopBlock(SubScreenWk.sndId);
            ope.str = SndStrReq(1, ope.mdtNo, 1, 0, 0, 0.0f);
            ope.flags |= 0x08000000;
            return 0;
        }
    }
    if (ope.str != 0 && (ope.flags & 0x08000000)) {
        IdUnit* u;
        if (SndStrStatusCk(ope.str, 2) == 0) {
            return 0;
        }
        SndStrReq(ope.str, 2, 0, 0);
        ope.flags &= ~0x08000000;
        FadeKillAll();
        termMotionSet(SubScreenWk.pTelDat, 10);
        modelOn = 1;
        u = IdSub.unitPtr(0x13, IDC_SSCRN_NEAR_0);
        IdSub.setTime(u, 0);
        u->be_flag |= 8;
        u->rev_flag &= 0xF0;
        u = IdSub.unitPtr(0x14, IDC_SSCRN_NEAR_0);
        IdSub.setTime(u, 0);
        u->be_flag |= 8;
        u->rev_flag &= 0xF0;
    }
    OpeMesClear();
    if (!(ope.flags & 0x10000000)) {
        if (Key.trg & 0x40000) {
            OpeSndStrStop();
            ope.flags |= 0x10000000;
        }
        for (;;) {
            TermSeq* s = &ope.seq[ope.seqIdx];
            if (!(s->time > ope.seqCnt)) {
                if (OpeSeqMove(s) == 0) {
                    return 1;
                }
            } else {
                break;
            }
        }
        ope.seqCnt++;
    } else {
        if ((Key.trg & 0x80000) || (Key.trg & 0x40000)) {
            if (OpeSeqMove(&ope.seq[ope.seqIdx]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

// Executes one sequence entry: arg -1 ends the op (stream stopped, messages deleted; returns 0),
// otherwise shows message mesNo for `arg` frames and advances seqIdx.
int SsTermMain::OpeSeqMove(TermSeq* s)
{
    TermSub* w = &sub;

    w->x14 = s->x2;
    w->x18 = s->mesNo;
    if (s->arg == -1) {
        OpeSndStrStop();
        cMes.Clear();
        return 0;
    }
    OpeMesSet(s->mesNo, s->arg);
    ope.seqIdx++;
    return 1;
}

// Shows subtitle message `no` at the frame unit (IdSub 0xFE/0x10) with the operator layout,
// replacing every message; no == -1 just waits for the current message to end. `wait` frames
// until OpeMesClear hides it (Disp_flg bit 11).
void SsTermMain::OpeMesSet(int no, int wait)
{
    DpfFlagOff(pG, DPF_MESSAGE);
    if (no == -1) {
        cMes.WaitEnd(0);
    } else {
        IdUnit* u = IdSub.unitPtr(0xFE, IDC_SSCRN_NEAR_0);
        int x = (int) ((u->pos0.x + 320.0f) * 0.8f);
        int y = (int) ((240.0f - u->pos0.y) * 0.8f);
        cMes.Clear();
        cMes.MesSet(no, x, y, 0x03000054, 0, 0, 4);
    }
    ope.mesNo = no;
    ope.mesWait = wait;
    sub.count++;
}

// Counts the current subtitle's display time down; sets Disp_flg bit 11 (message hidden) at 0.
void SsTermMain::OpeMesClear()
{
    if (ope.mesWait > 0) {
        ope.mesWait--;
        if (ope.mesWait <= 0) {
            ope.mesWait = 0;
            DpfFlagOn(pG, DPF_MESSAGE);
        }
    }
    sub.count++;
}

// Stops the op's voice stream (SndStrReq command 8) if one is playing.
void SsTermMain::OpeSndStrStop()
{
    if (ope.str) {
        SndStrReq(ope.str, 8, 0, 0);
        ope.str = 0;
    }
}

// File name of op `no`'s partner data: SS/cmn/ss_oc<call>.dat (101..112 chapter 1, 201..206, 301..305).
void partnerDataName(char* name, int no)
{
    const char* tbl[24] = {
        "101", "102", "103a", "103b", "104", "105", "106", "107", "108", "109", "110", "111",
        "112", "201", "202", "203", "204", "205", "206", "301", "302", "303", "304", "305",
    };
    sprintf(name, "SS/cmn/ss_oc%s.dat", tbl[no]);
}

// Which model talks in op `no`: 0 Hunnigan (ops 0..13), 1 (14..18), 2 (19..22), 3 (23); passed to
// hunniganModelInit.
int partnerType(int no)
{
    int tbl[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3};
    return tbl[no];
}

// The partner data file (SS/cmn/ss_ocNNN.dat) is an offset table like the other archives: 4 = the
// cancel motion, 12/13 = the player model's motion / shape data, 14/15 = the partner's.
void termMotionSet(void* data, int no)
{
    SsArc* d = (SsArc*) data;
    cModel* m;

    m = MapMgr.getWork(0);
    MotionSetCore(m, &((cMotModel*) m)->Motion, SS_ARC_PTR(d, 12), 0, (u8) no, 0x8000, 0);
    ShapeSet(GetModelInfoAddr(m->pModelInfo, 3), 0, SS_ARC_PTR(d, 13), 2);
    m = MapMgr.getWork(2);
    MotionSetCore(m, &((cMotModel*) m)->Motion, SS_ARC_PTR(d, 14), 0, (u8) no, 0x8000, 0);
    ShapeSet(GetModelInfoAddr(m->pModelInfo, 3), 0, SS_ARC_PTR(d, 15), 0xA);
}

// Sets the player's (pTerm sub-file 14) and the partner's (partner data sub-file 4) idle/cancel
// motion `no` with blending; init (no 0) and after the call was skipped (no 10).
void termMotionCancel(void* data, int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    SsArc* d = (SsArc*) data;
    cModel* m;

    m = MapMgr.getWork(0);
    MotionSetCore(m, &((cMotModel*) m)->Motion, SS_ARC_PTR(wk->pTermDat, 14), 0, (u8) no, 0x8004, 0);
    m = MapMgr.getWork(2);
    MotionSetCore(m, &((cMotModel*) m)->Motion, SS_ARC_PTR(d, 4), 0, (u8) no, 0x8004, 0);
}

static cFileList term_file_list;

// Codec screen loader: starts at the data read (there is no previous screen to fade).
void SsTermInit::init(SUB_SCREEN* wk)
{
    _rno = 2;
}

// Loads the codec screen with blocking reads (mode 5): state 2 drops the HUD/models/lights and
// reads SS/<lang>/ss_term.dat (-> pTerm, at the puzzle area offset), 4 op/op<stage>.das (-> pOpData),
// 5 the partner data of wk->opeMdtNo (-> pPartner), 6 fade in and transit to SsTermMain.
void SsTermInit::move(SUB_SCREEN* wk)
{
    // .bss order: the read request is the unit's first .bss word, before the file-scope
    // term_file_list (a constructed object is emitted with the static-init function, a plain
    // file-scope static only at the end): a function-local static is emitted at its function.
    static int term_read_req;
    void* term;
    void* op;
    void* partner;

    switch (_rno) {
    case 0:
    case 1:
    case 2:
        IdSys.dispSw(IDC_LIFE_METER, 0);
        IdSub.dispSw(IDC_SSCRN_PESETA, 0);
        sscrnModelFree(wk);
        sscrnLightClear(wk);
        wk->pTermDat = (SsArc*) (wk->pSwitchOffs + (u32) wk->pBuf);
        sscrnDataFilename(wk, "ss_term.dat");
#line 1101 "D:/Bio4/Prog/ss_term.cpp"
        term_read_req = DVD_READ_N(wk->filename, 0, 0, 0, 0, 5);
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        _rno++;
    case 3:
        Dvd.ReadCheck(term_read_req, 0, 0, &term);
        wk->pTermDat = (SsArc*) term;
        _rno++;
    case 4: {
        char name[32];
        sprintf(name, "op/op%02d.das", pG->stage_no);
#line 1134 "D:/Bio4/Prog/ss_term.cpp"
        term_read_req = DVD_READ_N(name, 0, 0, 0, 0, 5);
        Dvd.ReadCheck(term_read_req, 0, 0, &op);
        wk->pTermMes = op;
        _rno++;
    }
    case 5: {
        char name[32];
        partnerDataName(name, wk->opeMdtNo);
#line 1156 "D:/Bio4/Prog/ss_term.cpp"
        term_read_req = DVD_READ_N(name, 0, 0, 0, 0, 5);
        Dvd.ReadCheck(term_read_req, 0, 0, &partner);
        wk->pTelDat = partner;
        _rno++;
    }
    case 6:
        if (wk->open_flag == 0x20) {
            FadeSetW(0x80000000, 5, 0, 0);
        }
        transit(0, wk);
        break;
    }
}

// The Widget<SUB_SCREEN> destructor body (see the specialization declaration at the top): defined
// after SsTermInit::move, whose inlined transit instantiates Widget::quit, so the deferred
// destructor follows quit at the end of the file.
template <> inline Widget<SUB_SCREEN>::~Widget()  // COMPILER-DIFF: candidate #8
{
    Mem_free(link);
}

// Like generalModelAlloc with the codec screen's sizes: 0x10 model infos / 0x180 parts / 0x10 MapMgr
// works (the two character models are large).
void termModelAlloc(SUB_SCREEN* wk)
{
    int i;

    wk->attr_flag |= 1;
    ssModInfoMgr.roomInit();
    ssModInfoMgr.arrayAlloc(0x10);
    ssPartsMgr.roomInit();
    ssPartsMgr.arrayAlloc(0x180);
    cModel::mm = &ssModInfoMgr;
    cModel::pm = &ssPartsMgr;
    MapMgr.roomInit();
    MapMgr.arrayAlloc(0x10);
    for (i = 0; i < 0x10; i++) {
        MapMgr.create(0, i);
    }
}

// Codec screen camera: eye at (0, 0, 2000) looking at the origin, 50 degree fov.
void terminalCameraInit(SUB_SCREEN* wk, CAMERA* cam)
{
    const f32 zero = 0.0f;

    cam->param.pos.z = 2000.0f;
    cam->Up.y = 1.0f;
    cam->param.at.x = 0.0f;
    cam->param.at.y = 0.0f;
    cam->param.at.z = 0.0f;
    cam->param.pos.x = 0.0f;
    cam->param.pos.y = 0.0f;
    cam->Up.x = 0.0f;
    cam->Up.z = 0.0f;
    cam->param.fovy = 50.0f;
    CameraSetOrientationUp(cam);
    C_MTXPerspective(cam->ProjMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
    cam->Distance = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->v_mat, &cam->param.pos, &cam->Up, &cam->param.at);
}


// Never called (the ss_pzzl screenPos2puzzlePos formula): the original REL link dead-stripped the
// body (modules.py STRIP_UNUSED) and kept its pool, the four floats 0.5 / pi / 180 / 240 that
// follow terminalCameraInit's 1.3333334 in .rodata.
static void screenPos2terminalPos(Vec* pos, Vec* out)
{
    CAMERA* cam = &pG->Camera;
    f32 pz = cam->param.pos.z;
    f32 h = fabsf((f32) (pz * tan(cam->param.fovy * 0.5f * 3.1415927f / 180.0f)));

    out->x = pos->x * h / 240.0f;
    out->y = pos->y * h / 240.0f;
}

static Vec term_cam_pos = {435.0f, -1580.0f, 850.0f};
static Vec term_pl_pos = {0.0f, -1600.0f, 950.0f};
static Vec term_zero0 = {0.0f, 0.0f, 0.0f};
static Vec term_zero1 = {0.0f, 0.0f, 0.0f};

// Builds the codec call: id textures / groups 0x14, 0x10 (frame units hidden), operator font and
// layout, the op table, lights, camera and the two models (work 0 = the player's radio model from
// ss_term.dat, work 2 = the partner from ss_ocNNN.dat) at their fixed positions in the cancel pose.
// The op starts after ope.wait (30 frames).
void SsTermMain::init(SUB_SCREEN* wk)
{
    IdUnit* u;
    cModel* m;

    IdTexDataLoad(SS_ARC_PTR(wk->pTermDat, 5), TEX_OWNER_ID_SSCRN);
    IdSub.set(SS_ARC_PTR(wk->pTermDat, 8), 0xFF, IDC_SSCRN_0, 0xC, 5, 0);
    IdSub.set(SS_ARC_PTR(wk->pTermDat, 9), 0xFF, IDC_SSCRN_NEAR_0, 0xF, 2, 0);
    u = IdSub.unitPtr(0x12, IDC_SSCRN_NEAR_0);
    u->be_flag &= ~8;
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0x13, IDC_SSCRN_NEAR_0);
    u->be_flag &= ~8;
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0x14, IDC_SSCRN_NEAR_0);
    u->be_flag &= ~8;
    u->rev_flag |= 0xF;
    sscrnMainMenuInit(wk, 0);
    x10 = 0;
    if (pSys->language == 0) {
        cMes.setupFont(0x1C, 0x1C, (TEXPalette*) SS_ARC_PTR(wk->pTermDat, 4), 3);
    }
    cMes.setLayout(0, LAYOUT_OPERATOR);
    memset(&ope, 0, sizeof(ope));
    SndCall(0, 0x14, 0, 0, 0, 0);
    OpeMesTblInit(wk);
    ope.wait = 0x1E;
    IdAllocBuffer();
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x15));
    terminalCameraInit(wk, &pG->Camera);
    termModelAlloc(wk);
    ssPlModel = MapMgr.getWork(0);
    ssWepModel = MapMgr.getWork(1);
    ssPlMotion = 0;
    ssWepModel2 = 0;
    tel00ModelInit(MapMgr.getWork(0), wk->pTermDat);
    m = MapMgr.getWork(2);
    hunniganModelInit(m, wk->pTelDat, partnerType(wk->opeMdtNo));
    modelOn = 0;
    ended = 0;
    {
        Vec pos;
        Vec ang = {0.0f, 0.0f, 0.0f};
        pos = term_pl_pos;
        MapMgr.getWork(2)->pos = pos;
        MapMgr.getWork(2)->ang = ang;
        MapMgr.getWork(2)->matUpdate();
        {
            Vec d;
            Vec ang2;
            pos = term_cam_pos;
            PSVECSubtract(&pG->Camera.param.pos, &pos, &d);
            ang2.x = 0.0f;
            ang2.y = atan2f(d.x, d.z);
            ang2.z = 0.0f;
            MapMgr.getWork(0)->pos = pos;
            MapMgr.getWork(0)->ang = ang2;
            MapMgr.getWork(0)->matUpdate();
        }
    }
    termMotionCancel(wk->pTelDat, 0);
    modelOn = 1;
}

// Codec call frame: animates both models (shape + motion); when the call was skipped (flags
// 0x10000000) once switches them to the end pose. Counts ope.wait down then starts the op; when
// OpeMesMove reports the end or START (Key bit 29) is pressed, stops the stream, deletes the
// messages and transits to the exit widget (close_flag bit 3).
void SsTermMain::move(SUB_SCREEN* wk)
{
    if (modelOn != 0) {
        if (ended == 0 && (ope.flags & 0x10000000)) {
            IdUnit* u;
            termMotionCancel(wk->pTelDat, 10);
            ClrShape(MapMgr.getWork(0));
            ClrShape(MapMgr.getWork(2));
            ended = 1;
            u = IdSub.unitPtr(0x13, IDC_SSCRN_NEAR_0);
            IdSub.setTime(u, 0);
            u->be_flag |= 8;
            u->rev_flag &= 0xF0;
            u = IdSub.unitPtr(0x14, IDC_SSCRN_NEAR_0);
            IdSub.setTime(u, 0);
            u->be_flag |= 8;
            u->rev_flag &= 0xF0;
        }
        MotionMove(MapMgr.getWork(0), 0);
        ShapeMove(MapMgr.getWork(0)->pModelInfo);
        MotionMove(MapMgr.getWork(2), 0);
        ShapeMove(MapMgr.getWork(2)->pModelInfo);
    }
    if (x10 == 0) {
        if (ope.wait != 0) {
            ope.wait--;
            if (ope.wait <= 0) {
                OpeMdtSet();
            }
        } else {
            if (OpeMesMove() == 1 || (Key.trg & 0x20000000)) {
                OpeSndStrStop();
                wk->close_flag |= 8;
                cMes.Clear();
                transit(0, wk);
            }
        }
    }
}

// Codec screen close sound, releases the Japanese operator font; close_flag bit 3.
void SsTermMain::quit(SUB_SCREEN* wk)
{
    SndCall(0, 0x15, 0, 0, 0, 0);
    if (pSys->language == 0) {
        cMes.releaseFont(3);
    }
    wk->close_flag |= 8;
}
