// Sscrn/ss_cap: the bottle cap collection screen of the sub screen DLL (D:/Bio4/Prog/ss_cap.cpp):
// the 4 x 6 grid of shooting gallery prize caps (items 0xDC..0xF3, cap_id_tbl), entered from the
// key items screen (SsItemMain link 4) or directly with SS_OPEN_CAP when a cap was just won.
// Data: SS/<lang>/ss_cap.dat (id textures, IdSub table 0x14, cap names). Widgets: SsCapInit loads
// the archive, SsCapMain runs CapSelect (grid cursor) and SsItemExamine (turntable view with the
// cap's voice line on X).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "item.h"
#include "cockpit.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "snd.h"
#include "db_log.h"
#include "sscrn.h"
#include "ss_main.h"

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// The widget classes (SsCapInit / SsCapMain / CapSelect) are declared in ss_main.h.

extern "C" {
void dispCapList(SUB_SCREEN* wk);
}

// item ids of the 4 x 6 case grid
u16 cap_id_tbl[24] = {
    0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xF0,
    0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xF1,
    0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2,
    0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF3,
};

int cap_read_req;

// Cap screen loader: starts at the fade-out, or straight at the data read (state 2) when the screen
// was opened as SS_OPEN_CAP (0x100: the shooting gallery shows a freshly won cap).
void SsCapInit::init(SUB_SCREEN* wk)
{
    if (wk->open_flag == 0x100) {
        state = 2;
    } else {
        state = 0;
    }
}

// Loads the bottle cap screen: state 0/1 fade out, 2 hide the HUD, read SS/<lang>/ss_cap.dat over the
// puzzle archive slot (pSwitchDat) and drop the previous screen's ids/models/lights, 3 wait for the read
// (the archive becomes pExam), 4 fade in and transit to SsCapMain.
void SsCapInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
        FadeSetW(0, 5, 0, 0);
        state++;
    case 1:
        if (Fade[0].flags & 1) {
            break;
        }
        state++;
    case 2:
        IdSys.dispSw(IDC_LIFE_METER, 0);
        IdSub.dispSw(IDC_SSCRN_PESETA, 0);
        IdSub.dispSw(IDC_SSCRN_MAIN_MENU, 0);
        sscrnDataFilename(wk, "ss_cap.dat");
#line 108 "D:/Bio4/Prog/ss_cap.cpp"
        cap_read_req = DVD_READ_N(wk->filename, wk->pSwitchDat, 0, 0, 0, 0x10);
        if (cap_read_req <= 0) {
            break;
        }
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        sscrnModelClear(wk);
        sscrnLightClear(wk);
        wk->alpha_flag = 1;
        wk->alpha_cnt = 0;
        state++;
    case 3: {
        int stat;
        int size;
        if (Dvd.ReadCheck(cap_read_req, &stat, &size, 0) != 1) {
            break;
        }
        wk->pExam = wk->pSwitchDat;
        state++;
    }
    case 4:
        FadeSetW(0x80000000, 5, 0, 0);
        transit(0, wk);
        break;
    }
}

// Builds the cap screen: CapSelect <-> SsItemExamine widget pair, the archive's id textures, id
// table (IdSub group 0x14: 24 grid slots + cursor 0xFE), lights and cap names (MesData 2); allocates
// the 3-byte cursor {row, column, index}. An SS_OPEN_CAP open starts in the examine widget.
void SsCapMain::init(SUB_SCREEN* wk)
{
    sel = new CapSelect;
    exam = new SsItemExamine;
    sel->connect(0, exam);
    exam->connect(0, sel);
    IdTexDataLoad(SS_ARC_PTR(wk->pExam, 5), TEX_OWNER_ID_SSCRN);
    IdSub.set(SS_ARC_PTR(wk->pExam, 6), 0xFF, IDC_SSCRN_0, 0xC, 6, 0);
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x14));
    MesData.registData(2, (u8*) SS_ARC_PTR(wk->pExam, 4));
    sscrnMainMenuInit(wk, 0);
    state = 0;
#line 191 "D:/Bio4/Prog/ss_cap.cpp"
    wk->pCapCursor = (s8*) MEM_ALLOC(3, 1, 13);
    wk->pCapCursor[0] = 0;
    wk->pCapCursor[1] = 0;
    wk->pCapCursor[2] = 0;
    if (wk->open_flag == 0x100) {
        cur = exam;
    } else {
        cur = sel;
    }
}

// Runs the current child widget; when CapSelect reports state 1 (B) it returns to the item screen
// (link 0), state 2 (Y) exits the sub screen (link 1); close_flag bit 19 marks the cap screen.
void SsCapMain::move(SUB_SCREEN* wk)
{
    cMes.setLayout(0, LAYOUT_SUBSCRN);
    if (state == 0) {
        Widget<SUB_SCREEN>* w = cur;
        w->move(wk);
        next = w->cur;
        if (cur == sel) {
            switch (sel->state) {
            case 1:
                wk->close_flag |= 0x80000;
                transit(0, wk);
                break;
            case 2:
                wk->close_flag |= 0x80000;
                transit(1, wk);
                break;
            }
        }
        cur = next;
    }
}

static void sscrn_cap_out_init(SUB_SCREEN* wk);
static int sscrn_cap_out(SUB_SCREEN* wk);

// Leaving the cap screen: restores the common lights (pCmmn 0x12), frees the cursor and the child
// widgets and installs sscrn_cap_out as the exit routine (scrn_out_func).
void SsCapMain::quit(SUB_SCREEN* wk)
{
    sscrnLightClear(wk);
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x12));
    Mem_free(wk->pCapCursor);
    if (sel) {
        delete sel;
    }
    if (exam) {
        delete exam;
    }
    sscrn_cap_out_init(wk);
    wk->scrn_out_func = sscrn_cap_out;
}

// Starts the 7-frame fade-out of the cap screen exit.
static void sscrn_cap_out_init(SUB_SCREEN* wk)
{
    FadeSetW(0, 7, 0, 0);
}

// Exit routine (scrn_out_func) of the cap screen: waits for the fade, restores the HUD ids and life
// meter, starts the fade-in; returns 1 when done.
static int sscrn_cap_out(SUB_SCREEN* wk)
{
    int ret;

    if (Fade[0].flags & 1) {
        ret = 0;
    } else {
        IdSub.dispSw(IDC_SSCRN_MAIN_MENU, 1);
        wk->alpha_flag = 0;
        wk->alpha_cnt = 0;
        Cckpt.m_LifeMeter.fix(0);
        FadeSetW(0x80000000, 7, 0, 0);
        ret = 1;
    }
    return ret;
}

// Draws the 4 x 6 bottle cap grid: slot i shows texture cap_id_tbl[i] + 0x25 when the cap item is
// owned, else hidden; the cursor unit (0xFE) is moved onto the selected slot.
void dispCapList(SUB_SCREEN* wk)
{
    s8* sel = wk->pCapCursor;
    IdUnit* u;
    IdUnit* c;
    int i;

    for (i = 0; i < 24; i++) {
        u = IdSub.unitPtr(i + 1, IDC_SSCRN_0);
        if (ItemMgr.search(cap_id_tbl[i])) {
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = cap_id_tbl[i] + 0x25;
        } else {
            u->be_flag &= ~8;
        }
    }
    u = IdSub.unitPtr(0xFE, IDC_SSCRN_0);
    c = IdSub.unitPtr(sel[2] + 1, IDC_SSCRN_0);
    u->pos0 = c->pos0;
}

// Cap grid cursor: nothing to set up (the cursor lives in wk->pCapCursor).
void CapSelect::init(SUB_SCREEN* wk)
{
}

// Cap grid input: Y (state 2) leaves the sub screen, B (state 1) goes back, A examines the selected
// cap if owned (SsItemExamine on MapMgr work 2), X (debug) gives the cap; up/down and left/right
// (repeat) move the row (0..3) and column (0..5), sel[2] = row * 6 + column.
void CapSelect::move(SUB_SCREEN* wk)
{
    s8* sel = wk->pCapCursor;

    dispCapList(wk);
    state = 0;
    if (Key.trg & 0x100000) {
        state = 2;
        return;
    }
    if (Key.trg & 0x40000000) {
        state = 1;
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    if (Key.trg & 0x80000000) {
        wk->p_exam_item = ItemMgr.search(cap_id_tbl[sel[2]]);
        if (wk->p_exam_item) {
            wk->p_exam_model = MapMgr.getWork(2);
            transit(0, wk);
            SndCall(0, 0x1A, 0, 0, 0, 0);
        }
        return;
    }
    if (Key.trg & 0x20000) {
        if (ItemMgr.search(cap_id_tbl[sel[2]]) == 0) {
            ItemMgr.get(cap_id_tbl[sel[2]], 0);
        }
        return;
    }
    {
        s8 old = sel[2];
        if (Key.rep & 0x02000000) {
            sel[0]--;
        }
        if (Key.rep & 0x01000000) {
            sel[0]++;
        }
        sel[0] = sel[0] < 0 ? 3 : (sel[0] > 3 ? 0 : sel[0]);
        if (Key.rep & 0x08000000) {
            sel[1]--;
        }
        if (Key.rep & 0x04000000) {
            sel[1]++;
        }
        sel[1] = sel[1] < 0 ? 5 : (sel[1] > 5 ? 0 : sel[1]);
        sel[2] = sel[1] + sel[0] * 6;
        if (old != sel[2]) {
            SndCall(0, 0xA, 0, 0, 0, 0);
        }
    }
}

// Nothing to release.
void CapSelect::quit(SUB_SCREEN* wk)
{
}
