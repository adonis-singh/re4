// Sscrn/ss_item: the key items / treasures screen of the sub screen DLL (D:/Bio4/Prog/ss_item.cpp):
// main menu tab 0. Two scrolling columns (item_list from cItemMgr::makeItemList: key items, then
// treasures) of eight slot frames each (IdNum 0x40.. digits parented to the frame units), a cursor
// in SUB_SCREEN::pItemWk (ItemScreenWork), the command menu (use / combine / examine), the combine
// target pick and the item examine view. Data: SS/<lang>/ss_item.dat (id textures, IdSub/IdNum
// tables, item names). Widgets: SsItemInit (load) -> SsItemMain running ItemSelect -> ItemCommand
// -> ItemCombine / SsItemExamine; links 0 case, 2 map, 3 files, 4 bottle caps, 5 exit. Also the
// debug ITEM MAKE menu (Z on pad 1).
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
#include "joy.h"
#include "pad.h"
#include "snd.h"
#include "db_log.h"
#include "eprintf.h"
#include "path.h"
#include "camera.h"
#include "sscrn.h"
#include "ss_main.h"

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// Item list select (two columns), command menu and combine widgets of the item screen.
class ItemSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10  0 none, 1 back to the game, 2 main menu

    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class ItemCommand : public Widget<SUB_SCREEN> {
public:
    IdUnit* id[16];    // 0x10  command menu units (setCommandId)
    IdUnit* sub[11];   // 0x50  sub menu units
    u8 pad_7C[0x90 - 0x7C];
    int mode;          // 0x90  0 command, 1 open sub menu, 2 sub menu
    s8 num;            // 0x94  commands
    u8 cursorOld;      // 0x95
    s8 subSel;         // 0x96  sub menu cursor (0 yes, 1 no)
    u8 pad_97;
    int state;         // 0x98  0 none, 1 used, 2 equip

    ItemCommand() : Widget<SUB_SCREEN>(3) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class ItemCombine : public Widget<SUB_SCREEN> {
public:
    ItemCombine() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

extern "C" {
void itemNameDisp(SUB_SCREEN* wk);
void itemCameraInit(SUB_SCREEN* wk, CAMERA* cam);
void sscrn_item_out_init(SUB_SCREEN* wk);
ItemWork* ITEM_PTR(int idx, int col);
int ITEM_AT(ItemWork* p, int col);
int itemTexNo(u16 id);
u8 frameMarkNo(int n, int col);
void itemFrameSet(SUB_SCREEN* wk, int col);
void itemFrameInit(SUB_SCREEN* wk);
void itemFrameMove(SUB_SCREEN* wk, int col);
void itemListMake();
int itemSelect(SUB_SCREEN* wk, int mode);
void setCommandId(u8 mode, IdUnit** tbl, s8* num);
void itemMakeInit(SUB_SCREEN* wk);
void itemMakeMove(SUB_SCREEN* wk);
void itemMakeDisp(SUB_SCREEN* wk, int x, int y);
}

static int sscrn_item_out(SUB_SCREEN* wk);

static s16 item_name_w[2] = {0, 0x12};
static s16 item_name_h[2] = {0, 0x18};
static s8 item_name_space[4] = {-1, -1, -1, -1};
// One-element array: the in-struct store keeps the `lis item_wait@ha` in r11 (SsFileInit::move idiom).
static int item_wait[1] = {0};
static int item_num_x = 0;
static int item_num_y = 0;
static int item_cmd_mode = 0;

static int item_read_req;
static ItemWork item_dummy;
// Non-static: the REL's ADDR16 fields for these hold A only (global symbols), see the em35 rule.
u8 item_list[0x180];
s8 item_num[2];
s8 item_total;
ItemWork* item_sel;
int item_frame_on;
void* item_path0[2];
Hermite1* item_curve[2];
void* item_path1[2];
Vec item_scr[2];
Vec item_pos[2];
int item_frame_state[2];

// Prints the name of the item under the cursor (message id = item id, font 0x12 x 0x18) at the name
// unit (IdSub 1/0x1E); deletes the message when the slot is empty (flags bit 0 clear).
void itemNameDisp(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    IdUnit* u = IdSub.unitPtr(1, IDC_SSCRN_CKPT_2);
    int x;
    int y;
    int del = 0;
    ItemWork* item = ITEM_PTR(iw->idx[iw->col], iw->col);
    x = (int) ((u->pos.x + 320.0f) * 0.8f);
    y = (int) ((240.0f - u->pos.y) * 0.8f);
    MessageControl* pm = &cMes;

    y -= cMes.getFontHeight(0) / 2;
    if (!(item->flags & 1)) {
        del = 1;
    }
    if (del) {
        pm->Delete(0);
    } else {
        cMes.setFontSize(0, item_name_w[1], item_name_h[1]);
        cMes.setLineGap(0, 0);
        cMes.setFontGap(0, item_name_space[3]);
        pm->MesSet(item->id, x, y, 0x20088, 0, 0, 4);
    }
}

// The item screen uses the common sub screen camera.
void itemCameraInit(SUB_SCREEN* wk, CAMERA* cam)
{
    sscrnCameraInit(wk, cam);
}

// Key items / treasures screen loader: skips the previous screen's exit routine and wait (state 2)
// when opened directly as SS_OPEN_ITEM (0x80, the pick-up screen).
void SsItemInit::init(SUB_SCREEN* wk)
{
    if (wk->open_flag == 0x80) {
        state = 2;
    } else {
        state = 0;
    }
}

// Loads the key items / treasures screen: state 0 run the previous screen's scrn_out_func, then drop
// its ids and set the common frame ids; 1 one frame wait; 2 show the HUD and read SS/<lang>/ss_item.dat
// into the puzzle slot (coming from the map, menu_old 2, rebuilds the models and life meter);
// 3 wait for the read (archive -> pItem); 4 fade in for SS_OPEN_ITEM and transit to SsItemMain.
void SsItemInit::move(SUB_SCREEN* wk)
{
    int st = state;

    switch (st) {
    case 0:
        if (wk->scrn_out_func(wk) != 1) {
            break;
        }
        if (wk->menu_old == 2) {
            wk->wait_cnt = 1;
        }
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_0, 0xC, 2, 0);
        item_wait[0] = st;
        // `state++` written out here too (jump2 cross-jumps it into case 1's tail): at allocation
        // time this block has two pseudos, so the `lis item_wait@ha` gets r11.
        state++;
        break;
    case 1:
        item_wait[0]--;
        if (item_wait[0] >= 0) {
            break;
        }
        state++;
        break;
    case 2:
        IdSys.dispSw(IDC_LIFE_METER, 1);
        IdSub.dispSw(IDC_SSCRN_PESETA, 1);
        sscrnDataFilename(wk, "ss_item.dat");
#line 202 "D:/Bio4/Prog/ss_item.cpp"
        item_read_req = DVD_READ_N(wk->filename, wk->pSwitchDat, 0, 0, 0, 0x10);
        if (item_read_req <= 0) {
            break;
        }
        if (wk->menu_old == 2 && wk->open_flag != 0x80) {
            sscrnModelFree(wk);
            generalModelAlloc(wk);
            playerModelInit();
            sscrnLightClear(wk);
            {
                Cockpit* ck = &Cckpt;
                ck->m_LifeMeter.fix(1);
                ck->m_LifeMeter.frameIn();
            }
        } else {
            sscrnModelClear(wk);
        }
        wk->wait_cnt = 0;
        state++;
    case 3: {
        int stat;
        int size;
        if (Dvd.ReadCheck(item_read_req, &stat, &size, 0) != 1) {
            break;
        }
        wk->pItemDat = wk->pSwitchDat;
        state++;
    }
    case 4:
        if (wk->open_flag == 0x80) {
            FadeSetW(0x80000000, 5, 0, 0);
        }
        transit(0, wk);
        break;
    }
}

// Builds the key items / treasures screen: widgets ItemSelect -> ItemCommand -> ItemCombine /
// SsItemExamine, the archive's id textures, the number units (IdNum 0x40.. one per visible slot,
// parented to the two 8-slot frame columns), the command menu ids (group 0x16, hidden), lights and
// item names (MesData 2); allocates the ItemScreenWork cursor.
void SsItemMain::init(SUB_SCREEN* wk)
{
    sel = new ItemSelect;
    cmd = new ItemCommand;
    comb = new ItemCombine;
    exam = new SsItemExamine;
    sel->connect(0, cmd);
    cmd->connect(0, sel);
    cmd->connect(1, comb);
    cmd->connect(2, exam);
    comb->connect(0, sel);
    comb->connect(1, cmd);
    exam->connect(0, sel);
    cur = sel;
    itemCameraInit(wk, &pG->Camera);
    IdTexDataLoad(SS_ARC_PTR(wk->pItemDat, 5), TEX_OWNER_ID_SSCRN);
    if (IdSub.setCk(IDC_SSCRN_0) == 0) {
        IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_0, 0xC, 2, 0);
    }
    IdNum.set(SS_ARC_PTR(wk->pItemDat, 7), 0xFF, IDC_SSCRN_1, 0xC, 6, 0);
    for (int i = 0; i < 32; i++) {
        int no = i + 0x40;
        IdNum.set(SS_ARC_PTR(wk->pCmmn, 8), 0xFF, no, 0xC, 5, 0);
        numDisp(no, 0, 0, 0);
    }
    for (int k = 0; k < 2; k++) {
        int type = k * 8 + 0x40;
        for (int n = -3; n <= 4; n++) {
            IdUnit* parent = IdNum.unitPtr(frameMarkNo(n, k) - 0x30, IDC_SSCRN_1);
            IdNum.unitParent(parent, IdNum.unitPtr(0, type));
            type++;
        }
    }
    IdSub.set(SS_ARC_PTR(wk->pItemDat, 6), 0xFF, IDC_SSCRN_2, 0xC, 4, 0);
    for (int i = 0; i < 2; i++) {
        IdUnit* tbl[16];
        s8 num;
        int j;
        setCommandId(i, tbl, &num);
        for (j = 0; j < num * 2 + 4; j++) {
            tbl[j]->rev_flag |= 0xF;
        }
    }
    IdSub.set(SS_ARC_PTR(wk->pCmmn, 0x10), 0xFF, IDC_SSCRN_CKPT_2, 0x13, 1, 0);
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_2)->be_flag &= ~8;
    IdSub.unitPtr(0x60, IDC_SSCRN_2)->rev_flag |= 0xF;
    IdSub.unitPtr(0x61, IDC_SSCRN_2)->be_flag &= ~8;
    IdSub.unitPtr(0x62, IDC_SSCRN_2)->be_flag &= ~8;
    IdSub.unitPtr(0x70, IDC_SSCRN_2)->rev_flag |= 0xF;
    IdSub.unitPtr(0x71, IDC_SSCRN_2)->be_flag &= ~8;
    IdSub.unitPtr(0x72, IDC_SSCRN_2)->be_flag &= ~8;
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x12));
    if (wk->menu_old == 2 && wk->open_flag != 0x80) {
        wk->alpha_flag = 0;
        wk->alpha_cnt = 10;
    }
    MesData.registData(2, (u8*) SS_ARC_PTR(wk->pItemDat, 4));
    {
#line 368 "D:/Bio4/Prog/ss_item.cpp"
        ItemScreenWork* p = (ItemScreenWork*) MEM_ALLOC(sizeof(ItemScreenWork), 1, 13);
        wk->item = p;
        memclr_asm(p, sizeof(ItemScreenWork));
    }
    sscrnMainMenuInit(wk, 0);
    state = 0;
    wk->item->comb[0] = -1;
    wk->item->comb[1] = -1;
    itemFrameInit(wk);
    itemMakeInit(wk);
    SndCall(0, 0x1E, 0, 0, 0, 0);
}

// Key items / treasures screen frame: state 0 runs the child widget (ItemSelect state 1 = exit the
// sub screen via link 5, 2 = up to the main menu; ItemCommand state 1 = item used -> exit, 2 = show
// the cap screen via link 4; R (Key bit 23) jumps to the attache case), state 1 runs the main menu
// tab row (0 back here, 1 case, 2 map, 3 files, 4 exit; down returns to the list). Z on pad 1 (with
// a second pad when System_flg bit 3) toggles the debug item-make menu, B closes it.
void SsItemMain::move(SUB_SCREEN* wk)
{
    int i;

    if (cur != exam && state == 0) {
        itemNameDisp(wk);
    }
    wk->cursor_mode = 0;
    switch (state) {
    case 0:
        if (wk->item_make_open == 0) {
            Widget<SUB_SCREEN>* w = cur;
            w->move(wk);
            next = w->cur;
            if (cur == sel) {
                switch (sel->state) {
                case 2:
                    state = 1;
                    wk->close_flag |= 2;
                    sscrnMainMenuInit(wk, 1);
                    break;
                case 1:
                    wk->close_flag |= 2;
                    transit(5, wk);
                    break;
                default:
                    if (wk->open_flag != 0x80 && next == cur) {
                        if (Key.trg & 0x00800000) {
                            wk->close_flag = 0;
                            wk->menu_old = 0;
                            transit(0, wk);
                        }
                    }
                    break;
                }
            }
            if (cur == cmd) {
                switch (cmd->state) {
                case 1:
                    wk->close_flag |= 2;
                    transit(5, wk);
                    break;
                case 2:
                    wk->close_flag |= 2;
                    transit(4, wk);
                    SndCall(0, 0x1A, 0, 0, 0, 0);
                    break;
                }
            }
        }
        cur = next;
        break;
    case 1:
        if (wk->item_make_open == 0) {
            if (sscrnMainMenu(wk)) {
                switch ((s8) wk->menu_no) {
                case 1:
                    transit(0, wk);
                    break;
                case 0:
                    wk->item->col = 0;
                    sscrnMainMenuInit(wk, 0);
                    state = 0;
                    SndCall(0, 6, 0, 0, 0, 0);
                    break;
                case 3:
                    transit(3, wk);
                    break;
                case 2:
                    transit(2, wk);
                    break;
                case 4:
                    transit(5, wk);
                    break;
                }
            }
            if (Key.trg & 0x02000000) {
                wk->item->col = 0;
                sscrnMainMenuInit(wk, 0);
                state = 0;
                SndCall(0, 6, 0, 0, 0, 0);
            }
        }
        break;
    }
    if (wk->item_make_open) {
        itemMakeMove(wk);
        if (Joy[0].trg & 0x200) {
            wk->item_make_open = wk->item_make_open == 0;
            if (wk->item_make_open) {
                pG->debug_mode = 1;
            } else {
                pG->debug_mode = wk->debugMode;
            }
        }
    }
    if (Joy[0].trg & 0x10) {
        if (!SysFlagChk(pG, SYS_PUBLICITY_VER) || PadCheckStatus(&Joy[1]) == 1) {
            wk->item_make_open = wk->item_make_open == 0;
        }
        if (wk->item_make_open) {
            pG->debug_mode = 1;
        } else {
            pG->debug_mode = wk->debugMode;
        }
    }
}

// Leaving the item screen: deletes the child widgets, frees the cursor work and installs
// sscrn_item_out (frame slide-out animation) as scrn_out_func.
void SsItemMain::quit(SUB_SCREEN* wk)
{
    if (sel) {
        delete sel;
    }
    if (cmd) {
        delete cmd;
    }
    if (comb) {
        delete comb;
    }
    if (exam) {
        delete exam;
    }
    Mem_free(wk->item);
    sscrn_item_out_init(wk);
    wk->scrn_out_func = sscrn_item_out;
}

// Starts the item screen exit animation: fades the header/footer units, restores the two frame
// columns' original paths (saved by itemFrameInit) and plays them out; going to the map (menu_next 2)
// also frames the life meter out and fades the player model.
void sscrn_item_out_init(SUB_SCREEN* wk)
{
    IdUnit* u;

    u = IdSub.unitPtr(0, IDC_SSCRN_2);
    u->rev_flag |= 1;
    u = IdSub.unitPtr(5, IDC_SSCRN_2);
    u->rev_flag |= 4;
    u = IdNum.unitPtr(0x40, IDC_SSCRN_1);
    u->path0 = item_path0[0];
    u->curve[0] = item_curve[0];
    u->path1 = item_path1[0];
    FuncPathParametrize(u->path0, u->path1);
    if (item_frame_on) {
        u->rev_flag &= ~1;
        u->pos0 = item_scr[0];
        IdSub.setTime(u, 0);
        IdSub.movePos(u);
        IdSub.setTime(u, 15);
        IdSub.movePos(u);
    }
    u->rev_flag |= 1;
    u = IdNum.unitPtr(0x50, IDC_SSCRN_1);
    u->path0 = item_path0[1];
    u->curve[0] = item_curve[1];
    u->path1 = item_path1[1];
    FuncPathParametrize(u->path0, u->path1);
    if (item_frame_on) {
        u->rev_flag &= ~1;
        u->pos0 = item_scr[1];
        IdSub.setTime(u, 0);
        IdSub.movePos(u);
        IdSub.setTime(u, 15);
        IdSub.movePos(u);
    }
    u->rev_flag |= 1;
    u = IdSub.unitPtr(1, IDC_SSCRN_CKPT_2);
    u->rev_flag |= 1;
    if (wk->menu_next == 2) {
        Cckpt.m_LifeMeter.frameOut();
        wk->alpha_flag = 1;
    }
}

// Exit routine (scrn_out_func): 1 once the first frame column's path animation ended (end bit 0).
static int sscrn_item_out(SUB_SCREEN* wk)
{
    IdUnit* u = IdNum.unitPtr(0x40, IDC_SSCRN_1);
    int ret = 1;
    if ((u->anima_state & 1) == 0) {
        ret = 0;
    }
    return ret;
}

// Item slot `idx` of list column `col` (0 key items, 1 treasures) from item_list, or the empty
// item_dummy (flags 0) when out of range / 0xFF.
ItemWork* ITEM_PTR(int idx, int col)
{
    u8 no;

    item_dummy.flags = 0;
    // The out-of-range return is a `goto` to a label with two uses: cse does not follow it, so that
    // block recomputes `&item_dummy` (fresh `lis/addi` at the end) while the 0xFF return is the
    // fall-through of the flags store's extended block and reuses its address register (`mr r3, r8`).
    if (idx < 0 || idx > item_num[col] - 1) {
        goto DUMMY;
    }
    no = item_list[idx + col * item_num[0]];
    if (no == 0xFF) {
        return &item_dummy;
    }
    return ItemMgr.at(no);
DUMMY:
    return &item_dummy;
}

// List index of item `p` in column `col`, -1 when it is not listed.
int ITEM_AT(ItemWork* p, int col)
{
    int i;

    for (i = 0; i < item_num[col]; i++) {
        if (p == ItemMgr.at(item_list[i + col * item_num[0]])) {
            return i;
        }
    }
    return -1;
}

// Icon texture number of item `id` in the ss_item id texture set (table position; 0 = unknown).
int itemTexNo(u16 id)
{
    u8 tbl[105] = {
        0xFF, 0xA2, 0x3B, 0x3C, 0x3D, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60,
        0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x6E, 0x6F, 0x70, 0x77, 0x82, 0x83, 0x84, 0x85,
        0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8F, 0x90, 0x91, 0xD2, 0x93, 0xD3, 0xD4, 0x96,
        0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x98, 0xA1, 0x74, 0xA3, 0xA4, 0xA5, 0xA6, 0x0F, 0xA7, 0xB8,
        0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0x92, 0x1D, 0x1E, 0x1F,
        0x39, 0x80, 0x3A, 0x69, 0x7A, 0x7B, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB,
    };
    int i;

    for (i = 0; i < sizeof(tbl); i++) {
        if (id == tbl[i]) {
            return i;
        }
    }
    return 0;
}

// IdNum unit number of the slot frame n (-3..4 around the cursor) in column `col`: 0x41..0x48 for
// key items (reversed), 0x51..0x58 for treasures.
u8 frameMarkNo(int n, int col)
{
    switch (col) {
    case 0:
        switch (n) {
        case -3:
            return 0x48;
        case -2:
            return 0x47;
        case -1:
            return 0x46;
        case 0:
            return 0x45;
        case 1:
            return 0x44;
        case 2:
            return 0x43;
        case 3:
            return 0x42;
        case 4:
            return 0x41;
        }
        break;
    case 1:
        switch (n) {
        case -3:
            return 0x58;
        case -2:
            return 0x57;
        case -1:
            return 0x56;
        case 0:
            return 0x55;
        case 1:
            return 0x54;
        case 2:
            return 0x53;
        case 3:
            return 0x52;
        case 4:
            return 0x51;
        }
        break;
    }
    return 0x41;
}

// Refreshes the eight visible slots of column `col` around the cursor: icon texture, count (numDisp
// unit 0x40 + slot, hidden for single items) and hides empty slots and the combine source.
void itemFrameSet(SUB_SCREEN* wk, int col)
{
    ItemScreenWork* iw = wk->item;
    IdUnit* u;
    int n;
    int no;

    itemListMake();
    switch (col) {
    case 0:
        u = IdNum.unitPtr(0x40, IDC_SSCRN_1);
        break;
    case 1:
        u = IdNum.unitPtr(0x50, IDC_SSCRN_1);
        break;
    }
    no = col * 8 + 0x40;
    for (n = -3; n <= 4; n++) {
        IdUnit* m = IdNum.unitPtr(frameMarkNo(n, col), IDC_SSCRN_1);
        ItemWork* item = ITEM_PTR(n + iw->idx[col], col);
        int off;
        if (iw->comb[col] != -1 && iw->sel[col] == n + iw->idx[col]) {
            goto HIDE;
        }
        off = 0;
        if (!(item->flags & 1)) {
            off = 1;
        }
        if (off) {
        HIDE:
            m->be_flag &= ~8;
            numDisp(no, 0, 0, 0);
        } else {
            m->be_flag |= 8;
            m->tex_flag |= 2;
            // do {} while (0): the loop notes weight `item`'s refs one depth deeper, so global-alloc
            // ranks it above `col` (item r30, col r29).
            u16 maxNum;
            do {
                m->texNo = itemTexNo(item->id);
                maxNum = itemMaxNum(item->id);
            } while (0);
            if (maxNum != 1) {
                Vec pos;
                pos.x = (f32) item_num_x;
                pos.y = (f32) item_num_y;
                pos.z = 0.0f;
                numDisp(no, item->num, &pos, 1);
            } else {
                numDisp(no, 0, 0, 0);
            }
        }
        no++;
    }
}

// Remembers the two frame columns' rest paths/positions (for the exit animation), places them at
// the path end and fills both columns; frame state 3 = waiting for the path end.
void itemFrameInit(SUB_SCREEN* wk)
{
    IdUnit* u = 0;
    int i;

    item_frame_on = 0;
    for (i = 0; i < 2; i++) {
        switch (i) {
        case 0:
            u = IdNum.unitPtr(0x40, IDC_SSCRN_1);
            break;
        case 1:
            u = IdNum.unitPtr(0x50, IDC_SSCRN_1);
            break;
        }
        item_path0[i] = u->path0;
        item_curve[i] = u->curve[0];
        item_path1[i] = u->path1;
        item_scr[i] = u->pos0;
        IdSub.setTime(u, 15);
        IdSub.movePos(u);
        item_pos[i] = u->pos;
        IdSub.setTime(u, 0);
        item_frame_state[i] = 3;
        itemFrameSet(wk, i);
    }
}

// Scroll animation of frame column `col`: state 1/2 (cursor moved up/down) plays the column along
// the up/down path (IdNum 6/7 of 0x15) and refills the slots, 3 waits for the path end, 0 idle.
void itemFrameMove(SUB_SCREEN* wk, int col)
{
    IdUnit* u = 0;

    switch (col) {
    case 0:
        u = IdNum.unitPtr(0x40, IDC_SSCRN_1);
        break;
    case 1:
        u = IdNum.unitPtr(0x50, IDC_SSCRN_1);
        break;
    }
    switch (item_frame_state[col]) {
    case 0:
        break;
    case 1:
    case 2:
        item_frame_on = 1;
        u->pos0 = item_pos[col];
        switch (item_frame_state[col]) {
        case 1:
            u->path0 = IdNum.unitPtr(6, IDC_SSCRN_1)->path0;
            u->curve[0] = IdNum.unitPtr(6, IDC_SSCRN_1)->curve[0];
            u->path1 = IdNum.unitPtr(6, IDC_SSCRN_1)->path1;
            break;
        case 2:
            u->path0 = IdNum.unitPtr(7, IDC_SSCRN_1)->path0;
            u->curve[0] = IdNum.unitPtr(7, IDC_SSCRN_1)->curve[0];
            u->path1 = IdNum.unitPtr(7, IDC_SSCRN_1)->path1;
            break;
        }
        FuncPathParametrize(u->path0, u->path1);
        IdSub.setTime(u, 0);
        u->anima_state &= ~1;
        itemFrameSet(wk, col);
        item_frame_state[col] = 3;
        break;
    case 3:
        if (u->anima_state & 1) {
            item_frame_state[col] = 0;
        }
        break;
    }
}

// Rebuilds item_list from the inventory (key items then treasures; item_num[] per column).
void itemListMake()
{
    item_total = ItemMgr.makeItemList(item_list, 0, &item_num[0], &item_num[1]);
}

// List cursor input: mode 0 up/down (Key bits 24/25) switch column (-1 = leave to the main menu,
// returns 1), left/right (26/27) move within the column and set sel[]; mode 1 (combine) only moves
// and sets comb[]. Starts the frame scroll animation, highlights the column tab, runs itemFrameMove.
int itemSelect(SUB_SCREEN* wk, int mode)
{
    ItemScreenWork* iw = wk->item;
    int ret = 0;
    int i;
    s8 col;

    if (wk->open_flag == 0x80) {
        iw->col = 0;
    } else if (mode == 0) {
        s8 old = iw->col;
        if (Key.rep & 0x01000000) {
            iw->col--;
        }
        if (Key.rep & 0x02000000) {
            iw->col++;
        }
        iw->col = iw->col < -1 ? -1 : (iw->col > 1 ? 1 : iw->col);
        if (old != iw->col) {
            if (iw->col < 0) {
                ret = 1;
                goto END;
            }
            SndCall(0, 6, 0, 0, 0, 0);
            goto END;
        }
    }
    col = iw->col;
    if (item_num[col] != 0) {
        s8 old = iw->idx[col];
        s8 idx;
        if (Key.rep & 0x08000000) {
            iw->idx[col]--;
        }
        if (Key.rep & 0x04000000) {
            iw->idx[col]++;
        }
        iw->idx[col] = iw->idx[col] < 0 ? 0 : (iw->idx[col] > item_num[col] - 1 ? item_num[col] - 1 : iw->idx[col]);
        switch (mode) {
        case 0:
            iw->sel[col] = iw->idx[col];
            iw->comb[col] = -1;
            break;
        case 1:
            iw->comb[col] = iw->idx[col];
            break;
        }
        idx = iw->idx[col];
        if (old != idx) {
            if (item_frame_state[iw->col] == 3) {
                iw->idx[col] = old;
                item_frame_state[iw->col] = 4;
                itemFrameSet(wk, iw->col);
                iw->idx[col] = idx;
            }
            if (old > iw->idx[col]) {
                item_frame_state[iw->col] = 1;
            } else {
                item_frame_state[iw->col] = 2;
            }
            wk->cursor_flag = 1;
            SndCall(0, 6, 0, 0, 0, 0);
        }
    }
END:
    // `i = no` as the increment (the `mr r31, r30` after the arms; `i++` is a separate `addi`).
    // The r30 pin (live where `i` is and `no` is not) makes r30 used-so-far and conflicting with
    // `i`: the k loop's counter takes r30 in global-alloc pass 0 and `i` falls to r31 in pass 1.
    for (i = 0; i < 2;) {
        int no;
        {
            register int pin asm("r30"); // COMPILER-DIFF: candidate #17
            asm("" : "=r"(pin) : "r"(i));
            asm("" : : "r"(pin));
        }
        no = i + 1;
        IdUnit* u = IdSub.unitPtr(no, IDC_SSCRN_2);
        if (i == iw->col) {
            u->be_flag |= 8;
        } else {
            u->be_flag &= ~8;
        }
        i = no;
    }
    for (int k = 0; k < 2; k++) {
        itemFrameMove(wk, k);
    }
    return ret;
}

// List cursor widget entry: no combine partner, refresh both columns.
void ItemSelect::init(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    int i;

    iw->comb[0] = -1;
    iw->comb[1] = -1;
    for (i = 0; i < 2; i++) {
        itemFrameSet(wk, i);
    }
}

// List cursor: Y/B-to-game (state 1), B (state 2) up to the main menu, A on an occupied slot opens
// the command menu (item_sel), otherwise itemSelect (leaving upwards also gives state 2).
void ItemSelect::move(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    s8 col = iw->col;
    int i;

    state = 0;
    wk->cursor_mode = 1;
    if (sscrnKey2Game(wk)) {
        state = 1;
        return;
    }
    if (Key.trg & 0x40000000) {
        state = 2;
        SndCall(0, 0xA, 0, 0, 0, 0);
        for (i = 0; i < 2; i++) {
            IdSub.unitPtr(i + 1, IDC_SSCRN_2)->be_flag |= 8;
        }
        return;
    }
    if (Key.trg & 0x80000000) {
        int off;
        item_sel = ITEM_PTR(iw->sel[col], col);
        off = 0;
        if (!(item_sel->flags & 1)) {
            off = 1;
        }
        if (off) {
            return;
        }
        transit(0, wk);
        return;
    }
    if (itemSelect(wk, 0)) {
        state = 2;
        SndCall(0, 0xA, 0, 0, 0, 0);
        for (i = 0; i < 2; i++) {
            IdSub.unitPtr(i + 1, IDC_SSCRN_2)->be_flag |= 8;
        }
    }
}

// Command menu open: shows the column's command ids (key items: Use / Combine / Examine, treasures:
// Examine / Combine) with the cursor on the first entry.
void ItemCommand::init(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    int i;

    setCommandId(iw->col != 0, id, &num);
    for (i = 0; i < num * 2 + 4; i++) {
        id[i]->rev_flag &= 0xF0;
        id[i]->be_flag |= 8;
    }
    cursorOld = 1;
    mode = 0;
    state = 0;
    wk->cmd_menu_no = 0;
    SndCall(0, 4, 0, 0, 0, 0);
}

// Command menu: mode 0 B cancels, A runs the entry (item_cmd_mode 0 use -> state 1 on success,
// 1 examine (item 0xA2, the cap collection, opens the cap screen: state 2), 2 combine ->
// ItemCombine, 3 discard -> the yes/no sub menu; no command sets 3 in this build), up/down move the
// cursor; mode 1 opens the sub menu, mode 2 runs the yes/no (yes = ItemMgr.dumpAll).
void ItemCommand::move(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;

    wk->cursor_mode = 1;
    switch (mode) {
    case 0:
        if (Key.trg & 0x40000000) {
            {
                int j;
                for (j = 0; j < num * 2 + 4; j++) {
                    id[j]->rev_flag |= 0xF;
                }
            }
            transit(0, wk);
            SndCall(0, 5, 0, 0, 0, 0);
            return;
        }
        if (Key.trg & 0x80000000) {
            int used = 0;
            switch (iw->col) {
            case 0:
                switch (wk->cmd_menu_no) {
                case 0:
                    item_cmd_mode = used;
                    break;
                case 1:
                    item_cmd_mode = 2;
                    break;
                case 2:
                    item_cmd_mode = 1;
                    break;
                }
                break;
            case 1:
                switch (wk->cmd_menu_no) {
                case 0:
                    item_cmd_mode = iw->col;
                    break;
                case 1:
                    item_cmd_mode = 2;
                    break;
                }
                break;
            }
            if (!(item_cmd_mode > 2)) {
                int min = 1;  // a literal 1 folds to `<= 0`; the target keeps `cmpwi 1; blt`
                if (!(item_cmd_mode < min)) {
                {
                    int j;
                    for (j = 0; j < num * 2 + 4; j++) {
                        id[j]->rev_flag |= 0xF;
                    }
                }
                }
            }
            switch (item_cmd_mode) {
            case 2:
                transit(1, wk);
                SndCall(0, 9, 0, 0, 0, 0);
                return;
            case 0:
                used = ItemMgr.use(item_sel);
                break;
            case 1:
                // Both stores through PSet: the x24C store may then alias `item_sel`, so its `lis`
                // and load stay below it (the store is on the critical path).
                wk->p_exam_model = MapMgr.getWork(2);
                wk->p_exam_item = item_sel;
                if (item_sel->id == 0xA2) {
                    state = 2;
                    return;
                }
                transit(2, wk);
                SndCall(0, 0x1A, 0, 0, 0, 0);
                return;
            case 3:
                mode = 1;
                subSel = 1;
                SndCall(0, 0xB, 0, 0, 0, 0);
                return;
            }
            if (used == 1) {
                {
                    int j;
                    for (j = 0; j < num * 2 + 4; j++) {
                        id[j]->rev_flag |= 0xF;
                    }
                }
                state = 1;
                SndCall(0, 8, 0, 0, 0, 0);
                return;
            }
            SndCall(0, 7, 0, 0, 0, 0);
        }
        if (Key.rep & 0x01000000) {
            wk->cmd_menu_no--;
        } else if (Key.rep & 0x02000000) {
            wk->cmd_menu_no++;
        }
        wk->cmd_menu_no = wk->cmd_menu_no < 0 ? num - 1 : (wk->cmd_menu_no > num - 1 ? 0 : wk->cmd_menu_no);
        if (Key.rep & 0x03000000) {
            SndCall(0, 0xA, 0, 0, 0, 0);
        }
        if (cursorOld != wk->cmd_menu_no) {
            int j;
            for (j = 0; j < num; j++) {
                if (j == wk->cmd_menu_no) {
                    id[j * 2 + 4]->be_flag |= 8;
                } else {
                    id[j * 2 + 4]->be_flag &= ~8;
                }
            }
            cursorOld = wk->cmd_menu_no;
        }
        break;
    case 1: {
        int base = 0;
        switch (item_cmd_mode) {
        case 0:
            base = 0x70;
            break;
        case 3:
            base = 0x80;
            break;
        }
        int j;
        for (j = 0; j < 11; j++) {
            sub[j] = IdSub.unitPtr(base + j, IDC_SSCRN_CKPT_0);
            sub[j]->be_flag |= 8;
            sub[j]->rev_flag &= 0xF0;
        }
        PSVECAdd(&id[wk->cmd_menu_no * 2 + 4]->pos0, &id[wk->cmd_menu_no * 2 + 4]->pParent->pos0, &sub[0]->pos0);
        mode = 2;
    }
        // fall through: the sub menu is processed in the frame that opens it
    case 2:
        if (Key.trg & 0x40000000) {
            {
                int j;
                for (j = 0; j < 11; j++) {
                    sub[j]->rev_flag |= 0xF;
                }
            }
            mode = 0;
            SndCall(0, 5, 0, 0, 0, 0);
            return;
        }
        if (Key.trg & 0x80000000) {
            if (item_cmd_mode == 3) {
                if (subSel == 0) {
                    ItemMgr.dumpAll(item_sel);
                    SndCall(0, 0xD, 0, 0, 0, 0);
                } else {
                    SndCall(0, 5, 0, 0, 0, 0);
                }
            }
            {
                int j;
                for (j = 0; j < num * 2 + 4; j++) {
                    id[j]->rev_flag |= 0xF;
                }
            }
            {
                int j;
                for (j = 0; j < 11; j++) {
                    sub[j]->rev_flag |= 0xF;
                }
            }
            transit(0, wk);
            return;
        }
        {
            s8 old = subSel;
            if (Key.trg & 0x01000000) {
                subSel--;
            } else if (Key.trg & 0x02000000) {
                subSel++;
            }
            subSel = subSel < 0 ? 0 : (subSel > 1 ? 1 : subSel);
            if (old != subSel) {
                SndCall(0, 0xA, 0, 0, 0, 0);
            }
        }
        sub[5]->be_flag &= ~8;
        sub[7]->be_flag &= ~8;
        if (subSel == 0) {
            sub[5]->be_flag |= 8;
        } else {
            sub[7]->be_flag |= 8;
        }
        break;
    }
    {
        int j;
        for (j = 0; j < 2; j++) {
            itemFrameMove(wk, j);
        }
    }
}

// Combine mode open: shows the column's combine slot ids (0x60/0x70 of group 0x16) with the
// selected item's icon and marks it as the combine source (comb[col]).
void ItemCombine::init(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    s8 col = iw->col;
    u8 base = 0;
    ItemWork* item;
    IdUnit* u;

    switch (col) {
    case 0:
        base = 0x60;
        break;
    case 1:
        base = 0x70;
        break;
    }
    IdSub.unitPtr(base, IDC_SSCRN_2)->rev_flag &= 0xF0;
    IdSub.unitPtr(base | 1, IDC_SSCRN_2)->be_flag |= 8;
    IdSub.unitPtr(base | 2, IDC_SSCRN_2)->be_flag |= 8;
    item = ITEM_PTR(iw->sel[col], col);
    u = IdSub.unitPtr(base | 1, IDC_SSCRN_2);
    u->tex_flag |= 2;
    u->texNo = itemTexNo(item->id);
    iw->comb[col] = iw->sel[col];
    itemFrameSet(wk, col);
}

// Combine mode close: hides the combine slot ids and clears the source.
void ItemCombine::quit(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    s8 col = iw->col;
    u8 base = 0;

    switch (col) {
    case 0:
        base = 0x60;
        break;
    case 1:
        base = 0x70;
        break;
    }
    IdSub.unitPtr(base, IDC_SSCRN_2)->rev_flag |= 0xF;
    IdSub.unitPtr(base | 1, IDC_SSCRN_2)->be_flag &= ~8;
    IdSub.unitPtr(base | 2, IDC_SSCRN_2)->be_flag &= ~8;
    iw->comb[col] = -1;
}

// Combine target pick: B returns the cursor to the source and reopens the command menu, A combines
// source into the target (ItemMgr.combine; list rebuilt, cursor on the result) or plays the error
// sound, otherwise itemSelect mode 1 moves the target cursor.
void ItemCombine::move(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    s8 col = iw->col;

    wk->cursor_mode = 2;
    if (Key.trg & 0x40000000) {
        s8 old = iw->idx[col];
        int d;
        iw->idx[col] = iw->sel[col];
        d = old - iw->idx[col];
        iw->comb[col] = -1;
        if (d > 0) {
            item_frame_state[col] = 1;
        } else if (d < 0) {
            item_frame_state[col] = 2;
        } else {
            itemFrameSet(wk, col);
        }
        transit(1, wk);
    } else if (Key.trg & 0x80000000) {
        ItemWork* a = ITEM_PTR(iw->sel[col], col);
        ItemWork* b = ITEM_PTR(iw->comb[col], col);
        if (b != a && ItemMgr.combine(b, a, 0)) {
            ItemMgr.makeItemList(item_list, 0, &item_num[0], &item_num[1]);
            iw->idx[col] = ITEM_AT(b, col);
            transit(0, wk);
            SndCall(0, 0x27, 0, 0, 0, 0);
        } else {
            SndCall(0, 7, 0, 0, 0, 0);
        }
    } else {
        itemSelect(wk, 1);
    }
}

// Collects the command menu units of column `mode` (0 key items 0x20.., 1 treasures 0x30.. of
// group 0x16): tbl[0..3] frame parts, then per command its label and highlight; *num = commands.
void setCommandId(u8 mode, IdUnit** tbl, s8* num)
{
    switch (mode) {
    case 0:
        tbl[0] = IdSub.unitPtr(0x20, IDC_SSCRN_2);
        tbl[1] = IdSub.unitPtr(0x21, IDC_SSCRN_2);
        tbl[2] = IdSub.unitPtr(0x22, IDC_SSCRN_2);
        tbl[3] = IdSub.unitPtr(0x29, IDC_SSCRN_2);
        *num = 3;
        tbl[4] = IdSub.unitPtr(0x23, IDC_SSCRN_2);
        tbl[5] = IdSub.unitPtr(0x26, IDC_SSCRN_2);
        tbl[6] = IdSub.unitPtr(0x24, IDC_SSCRN_2);
        tbl[7] = IdSub.unitPtr(0x27, IDC_SSCRN_2);
        tbl[8] = IdSub.unitPtr(0x25, IDC_SSCRN_2);
        tbl[9] = IdSub.unitPtr(0x28, IDC_SSCRN_2);
        break;
    case 1:
        tbl[0] = IdSub.unitPtr(0x30, IDC_SSCRN_2);
        tbl[1] = IdSub.unitPtr(0x31, IDC_SSCRN_2);
        tbl[2] = IdSub.unitPtr(0x32, IDC_SSCRN_2);
        tbl[3] = IdSub.unitPtr(0x37, IDC_SSCRN_2);
        *num = 2;
        tbl[4] = IdSub.unitPtr(0x33, IDC_SSCRN_2);
        tbl[5] = IdSub.unitPtr(0x35, IDC_SSCRN_2);
        tbl[6] = IdSub.unitPtr(0x34, IDC_SSCRN_2);
        tbl[7] = IdSub.unitPtr(0x36, IDC_SSCRN_2);
        break;
    }
}

// Debug item-make menu (defined here: its strings follow itemFrameSet's pool in .rodata). Its state
// is the tail of the sub screen's debug block at SUB_SCREEN+0x34C (x34C .. x36C), addressed as one
// struct (`addi rX, wk, 0x34c` + displacements).
struct SsItemMakeWork {
    u8 pad_0[0x1C];  // 0x00  SUB_SCREEN::x34C .. x367
    s8 cursor;       // 0x1C  SUB_SCREEN::x368
    u8 pad_1D[3];
    int id[2];       // 0x20  SUB_SCREEN::x36C
};
#define ITEM_MAKE_WORK(wk) ((SsItemMakeWork*) &(wk)->debug_menu)

static const char* item_make_name[3] = {"KEY ITEM", "TREASURE", "REMOVE  "};
static int item_make_mes_x = 0;
static int item_make_mes_y = -0x13;


#define ITEM_MAKE_CLAMP(v) ((v) < 0 ? 0xFE : ((v) > 0xFE ? 0 : (v)))


// Debug item-make menu: sets the two id slots to the first key item (types 7) and treasure
// (types 5/0xC) ids, cursor on the first row.
void itemMakeInit(SUB_SCREEN* wk)
{
    SsItemMakeWork* mk = ITEM_MAKE_WORK(wk);
    ItemInfo info;
    int i;

    for (i = 0; i < 2; i++) {
        // COMPILER-DIFF: 12. The target keeps both arm `li`s in place and masks `& 0xFF`; with a plain
        // pseudo jump.c hoists the else set above the compare (`x = b; if (c) x = a`) and combine
        // narrows the mask to `& 0xF` (reg_nonzero_bits = the union of the two constants). The r29 pin
        // (the target's register) keeps combine from tracking the constants (no reg_nonzero_bits for a
        // hard register); the dead `types = 0` in each arm makes the arm two insns until flow deletes
        // it (jump1/cse-jump cannot hoist) and gives each `li` a REG_WAS_0 note next to cse's REG_EQUAL
        // (two notes: jump2 cannot hoist either).
        register int types asm("r29");  // COMPILER-DIFF: 12
        mk->id[i] = 0;
        if (i == 0) {
            types = 0;  // COMPILER-DIFF: 12 (dead set, see above)
            types = 0x0707;
        } else {
            types = 0;  // COMPILER-DIFF: 12 (dead set, see above)
            types = 0x050C;
        }
        // The match test is written in the loop condition (a comma expression): an inline
        // returning `a || b` materialises the 0/1 (`li r11` + `cmpwi`) where the target branches.
        // `(u16) types` keeps the `clrlwi 16` (2 uses: combine cannot fold it into the shift).
        while (itemInfo(mk->id[i], &info), !((u16) types >> 8 == info.type || ((u16) types & 0xFF) == info.type)) {
            mk->id[i]++;
            mk->id[i] = ITEM_MAKE_CLAMP(mk->id[i]);
        }
    }
    mk->cursor = 0;
}

// Debug item-make menu input (pad 1): up/down pick KEY ITEM / TREASURE / REMOVE, left/right step the
// row's id through the matching item types (x16 with A held), A gives the item (cursor moves onto
// it) or removes the item under the list cursor.
void itemMakeMove(SUB_SCREEN* wk)
{
    ItemScreenWork* iw = wk->item;
    JOY* joy = &Joy[0];
    SsItemMakeWork* mk = ITEM_MAKE_WORK(wk);
    ItemWork* got = 0;
    ItemWork* cur = ITEM_PTR(iw->idx[iw->col], iw->col);
    ItemInfo info;
    int i;

    if (joy->rep & 0x00080008) {
        mk->cursor--;
    }
    if (joy->rep & 0x00040004) {
        mk->cursor++;
    }
    mk->cursor = mk->cursor < 0 ? 0 : (mk->cursor > 2 ? 2 : mk->cursor);
    switch (mk->cursor) {
    case 0:
    case 1: {
        int d = 0;
        register int hi asm("r28"); // COMPILER-DIFF: candidate #17
        register int lo asm("r29"); // COMPILER-DIFF: candidate #17
        int n;
        if (mk->cursor == 0) {
            hi = 7;
            n = 0;
            lo = 7;
        } else {
            hi = 5;
            n = 1;
            lo = 0xC;
        }
        if (joy->trg & 0x100) {
            ItemMgr.get((u16) mk->id[n], 0);
            got = ItemMgr.newbie();
        } else {
            if (joy->rep & 0x00010001) {
                if (joy->on & 0x100) {
                    mk->id[n] -= 0x10;
                } else {
                    mk->id[n] -= 1;
                }
                mk->id[n] = ITEM_MAKE_CLAMP(mk->id[n]);
                d = -1;
            }
            if (joy->rep & 0x00020002) {
                if (joy->on & 0x100) {
                    mk->id[n] += 0x10;
                } else {
                    mk->id[n] += 1;
                }
                mk->id[n] = ITEM_MAKE_CLAMP(mk->id[n]);
                d = 1;
            }
            if (d != 0) {
                while (itemInfo(mk->id[n], &info), !(hi == info.type || lo == info.type)) {
                    mk->id[n] += d;
                    mk->id[n] = ITEM_MAKE_CLAMP(mk->id[n]);
                }
            }
        }
        break;
    }
    case 2:
        if (joy->trg & 0x100) {
            ItemMgr.dumpAll(cur);
        }
        break;
    }
    itemMakeDisp(wk, 0x28, 5);
    if (got) {
        iw->idx[mk->cursor] = ITEM_AT(got, mk->cursor);
    }
    for (i = 0; i < 2; i++) {
        itemFrameSet(wk, i);
    }
}

// Draws the "ITEM MAKE" debug menu at text cell (x, y) with the current ids and the selected item's
// name message.
void itemMakeDisp(SUB_SCREEN* wk, int x, int y)
{
    ItemScreenWork* iw = wk->item;
    SsItemMakeWork* mk = ITEM_MAKE_WORK(wk);
    ItemWork* cur = ITEM_PTR(iw->idx[iw->col], iw->col);
    int i;
    u16 id;

    eprintf(x * 8, y++ * 14, 5, 0, "----- ITEM MAKE -----");
    for (i = 0; i < 3; i++, y++) {
        u8 col;
        int c;
        if (i == mk->cursor) {
            eprintf(x * 8, y * 14, 0x16, 0, ">");
            c = 4;
        } else {
            c = 0;
        }
        col = c;
        eprintf((x + 1) * 8, y * 14, col, 0, "%s", item_make_name[i]);
        eprintf((x + 9) * 8, y * 14, 0, 0, ": ID[0x  ]");
        switch (i) {
        case 0:
        case 1:
            eprintf((x + 9) * 8, y * 14, col, 0, "       %02x", mk->id[i]);
            break;
        case 2:
            eprintf((x + 9) * 8, y * 14, col, 0, "       %02x", cur->id);
            break;
        }
    }
    eprintf(x * 8, y++ * 14, 0, 0, "---------------------");
    id = 0;
    switch (mk->cursor) {
    case 0:
    case 1:
        id = mk->id[mk->cursor];
        break;
    case 2:
        id = cur->id;
        break;
    }
    cMes.MesSet(id, (x + 1) * 8 + item_make_mes_x, y++ * 14 + item_make_mes_y, 0x20088, 0, 0, 4);
    y++;
    eprintf(x * 8, y * 14, 0, 0, "---------------------");
}

