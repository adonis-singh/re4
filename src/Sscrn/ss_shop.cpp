// Sscrn/ss_shop: the merchant (shop) screen of the sub screen DLL (D:/Bio4/Prog/ss_shop.cpp).
// Sell / buy / tune-up menus over the game/merchant.cpp Merchant session; a bought item is placed
// on the attache case through the ss_pzzl widgets (PzzlThinking / PieceSelect / CaseChange).
#include "types.h"
#include "global.h"
#include "light.h"
#include "map_obj.h"
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
#include "snd.h"
#include "db_log.h"
#include "camera.h"
#include "trans_ot.h"
#include "math_sub.h"
#include "puzzle.h"
#include "merchant.h"
#include "examine.h"
#include "sscrn.h"
#include "ss_main.h"
#include "ss_pzzl.h"


#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// ss_main.cpp

// Shop screen state (SUB_SCREEN::pShopWk, MEM_ALLOC(0x48)).
struct ShopWork {
    int num;         // 0x00  entries of the current list
    int top;         // 0x04  first entry shown
    int cursor;      // 0x08
    int count;       // 0x0C  pieces to sell / buy
    u16 buyId;       // 0x10
    ItemWork buy;    // 0x12  slot template of the item being bought (case placement)
    int placed;      // 0x20  the bought piece was put on the case
    ItemWork* item;  // 0x24  item being sold / tuned
    int price;       // 0x28  tune-up price
    int lvType;      // 0x2C  tune type (0 fire, 1 magazine, 2 speed, 3 exclusive, 4 all)
    int lv[4];       // 0x30  tune levels after the purchase
    int noRoom;      // 0x40  the bought piece did not fit
    int coat;        // 0x44  the merchant's coat is open
};

// Message / voice stream pair of the merchant's lines.
struct ShopMsg {
    u8 msg;
    u8 str;
};

// The shop widgets (SsShopMain::init creates them).
class ShopTopMenu : public Widget<SUB_SCREEN> {
public:
    s8 state;       // 0x10  0 greeting, 1 wait for the coat, 2 menu
    s8 cursor;      // 0x11  0 sell, 1 buy, 2 tune up
    s8 exit;        // 0x12  the exit entry is selected
    u8 pad_13;
    int greetNum;   // 0x14
    int greet[3];   // 0x18
    int greetIdx;   // 0x24
    int greetStep;  // 0x28
    int result;     // 0x2C  1 leave the shop

    ShopTopMenu() : Widget<SUB_SCREEN>(3) { cursor = 1; }
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class SellMenuSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    SellMenuSelect() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class SellItemNum : public Widget<SUB_SCREEN> {
public:
    u16 repeat;  // 0x10  frames the stick was held
    u16 fast;    // 0x12  step by 8

    SellItemNum() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class SellConfirm : public Widget<SUB_SCREEN> {
public:
    int result;  // 0x10

    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class BuyMenuSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    BuyMenuSelect() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class BuyItemNum : public Widget<SUB_SCREEN> {
public:
    int state;   // 0x10  0 check, 1 confirm
    s16 stock;   // 0x14
    s8 unit;     // 0x16
    u8 pad_17;
    int result;  // 0x18
    int msg;     // 0x1C

    BuyItemNum() : Widget<SUB_SCREEN>(3) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class BuyConfirm : public Widget<SUB_SCREEN> {
public:
    int result;  // 0x10
    int msg;     // 0x14

    BuyConfirm() : Widget<SUB_SCREEN>(3) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class BuyPuzzleEnd : public Widget<SUB_SCREEN> {
public:
    int x10;

    BuyPuzzleEnd() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class LvUpMenuSelect : public Widget<SUB_SCREEN> {
public:
    int state;  // 0x10

    LvUpMenuSelect() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class LvUpItemSelect : public Widget<SUB_SCREEN> {
public:
    LvUpItemSelect() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class LvUpConfirm : public Widget<SUB_SCREEN> {
public:
    s8 cur[4];   // 0x10  current tune levels + 1
    s8 max[4];   // 0x14  merchant's max tune levels
    int result;  // 0x18
    int msg;     // 0x1C

    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

extern "C" {
void shopClearZ(SUB_SCREEN* wk);
void setShopMsgQueue(int on);
void shopStrInit(SUB_SCREEN* wk);
void shopStrStop(SUB_SCREEN* wk);
void shopStrPlay(SUB_SCREEN* wk, int no);
void shopModelAlloc(SUB_SCREEN* wk);
void closeCoat(SUB_SCREEN* wk);
int getGreetMsg(int* num, int* tbl);
void dispSellItemList(SUB_SCREEN* wk, int n, int cursor);
void listRangeCheck(SUB_SCREEN* wk);
void dispBuyItemList(SUB_SCREEN* wk, int n, int cursor);
int deleteExtraPiece(SUB_SCREEN* wk);
int buyItem(SUB_SCREEN* wk);
void dispLvUpItemList(SUB_SCREEN* wk, int n, int cursor);
void levelItemDisp(SUB_SCREEN* wk, int sw);
int specialCaption(int id);
void itemCaption(int id);
void weaponLevelDisp(ItemWork* item, u16 id, int sw, int level);
void stockNumDisp(int num, int sw);
void dispPrice(int type, int num, int price, Vec* pos, u32 flags);
void setOrientation(int id, cModel* m);
void dispItem(int id, int sw);
void screenPos2worldPos(Vec* scr, Vec* out);
void moveItem();
}


// The shop's own item texture table (ss_item.cpp has the inventory's `itemTexNo`).
static int itemTexNo(int id);

// dispPrice display flags
static u32 price_disp_price = 1;
static u32 price_disp_num = 2;
static u32 price_disp_sold = 4;

// Merchant line table: {message, voice stream} (indexes are the greeting / answer numbers).
// Not static: scope:global in the module symbols (the REL's ADDR16 fields hold A only), like
// shop_pos_save / shop_msg_buf below.
ShopMsg shop_msg[26] = {
    {0x13, 0xB9}, {0x14, 0xB6}, {0x15, 0xFF}, {0x16, 0xFF}, {0x17, 0xFF}, {0xFF, 0xBA}, {0x0B, 0xB7},
    {0x18, 0xB5}, {0xFF, 0xB4}, {0xFF, 0xBB}, {0x0C, 0xB7}, {0x19, 0xBC}, {0x1A, 0xBD}, {0x1B, 0xBE},
    {0x1C, 0xBF}, {0x1D, 0xC0}, {0x1E, 0xFF}, {0x1F, 0xC1}, {0x20, 0xC2}, {0x0A, 0xB8}, {0x0D, 0xFF},
    {0xFF, 0xB4}, {0xFF, 0xBB}, {0x0E, 0xB7}, {0x0A, 0xB8}, {0xFF, 0xB4},
};

static Vec sell_num_pos = {-10.0f, -105.0f, 0.0f};
static int sell_msg_x = 0x37;
static int sell_msg_y = 0x30;
static int buy_msg_x = 0x37;
static int buy_msg_y = 0x30;
static int buy_conf_x = 0x37;
static int buy_conf_y = 0x30;
static int lvup_msg_x = 0x37;
static int lvup_msg_y = 0x30;

// Model orientation / scale of the item shown on the counter, by item id.
struct ShopItemPlace {
    int id;
    Vec rot;    // degrees
    Vec scale;
};
static ShopItemPlace shop_item_place[51] = {
    {0x21, {15.0f, 230.0f, 20.0f}, {1.5f, 1.5f, 1.5f}},
    {0x23, {-30.0f, -30.0f, 15.0f}, {1.8f, 1.7f, 1.3f}},
    {0x25, {0.0f, -20.0f, 10.0f}, {1.6f, 1.6f, 1.6f}},
    {0x27, {-30.0f, -20.0f, -20.0f}, {1.6f, 1.6f, 1.6f}},
    {0x29, {30.0f, 180.0f, 20.0f}, {1.7f, 1.7f, 1.7f}},
    {0x2A, {15.0f, 240.0f, 36.0f}, {1.9f, 1.9f, 1.9f}},
    {0x2C, {0.0f, 0.0f, 20.0f}, {1.0f, 1.0f, 1.0f}},
    {0x94, {0.0f, 0.0f, -25.0f}, {1.0f, 1.0f, 1.0f}},
    {0x2D, {0.0f, -25.0f, 5.0f}, {1.4f, 1.3f, 1.3f}},
    {0x2E, {-20.0f, 250.0f, 35.0f}, {1.6f, 1.6f, 1.2f}},
    {0x2F, {-30.0f, 10.0f, 20.0f}, {1.1f, 1.1f, 1.0f}},
    {0x30, {0.0f, 240.0f, 0.0f}, {1.8f, 1.5f, 1.0f}},
    {0x35, {20.0f, -160.0f, 25.0f}, {1.0f, 1.0f, 1.0f}},
    {0x36, {-10.0f, -40.0f, -10.0f}, {1.6f, 1.4f, 1.3f}},
    {0x03, {0.0f, 250.0f, 10.0f}, {1.75f, 1.75f, 1.75f}},
    {0x6D, {20.0f, -160.0f, 25.0f}, {1.0f, 1.0f, 1.0f}},
    {0x37, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {0x34, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {0x04, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x20, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x18, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x07, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x6A, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x1A, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x00, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x46, {-30.0f, 30.0f, 30.0f}, {2.2f, 2.2f, 2.2f}},
    {0x3F, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {0x42, {-20.0f, 40.0f, 30.0f}, {2.0f, 1.4f, 1.0f}},
    {0x43, {-40.0f, -30.0f, 0.0f}, {2.5f, 1.9f, 1.3f}},
    {0x44, {-10.0f, -10.0f, -5.0f}, {1.5f, 1.5f, 1.5f}},
    {0x45, {30.0f, -35.0f, 10.0f}, {2.0f, 2.0f, 2.0f}},
    {0xAA, {0.0f, 65.0f, 0.0f}, {1.6f, 1.6f, 1.6f}},
    {0xC5, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {0x01, {20.0f, -40.0f, -30.0f}, {2.0f, 2.2f, 2.0f}},
    {0x02, {20.0f, -40.0f, -30.0f}, {2.0f, 2.2f, 2.0f}},
    {0x0E, {20.0f, -40.0f, -30.0f}, {2.0f, 2.2f, 2.0f}},
    {0x05, {0.0f, -15.0f, -50.0f}, {1.8f, 2.0f, 1.8f}},
    {0x08, {0.0f, 0.0f, -20.0f}, {2.2f, 2.2f, 2.2f}},
    {0x09, {0.0f, 0.0f, -20.0f}, {2.2f, 2.2f, 2.2f}},
    {0x0A, {0.0f, 0.0f, -20.0f}, {2.2f, 2.2f, 2.2f}},
    {0x95, {0.0f, -10.0f, 105.0f}, {1.3f, 1.3f, 1.3f}},
    {0x97, {0.0f, 0.0f, -70.0f}, {1.2f, 1.2f, 1.2f}},
    {0x06, {0.0f, 0.0f, 30.0f}, {2.0f, 2.0f, 2.0f}},
    {0x19, {0.0f, 0.0f, 30.0f}, {2.0f, 2.0f, 2.0f}},
    {0x1C, {0.0f, 0.0f, 30.0f}, {2.0f, 2.0f, 2.0f}},
    {0x14, {-50.0f, -70.0f, 0.0f}, {1.0f, 2.4f, 1.0f}},
    {0x16, {-50.0f, -70.0f, 0.0f}, {1.0f, 2.4f, 1.0f}},
    {0xA8, {-50.0f, -70.0f, 0.0f}, {1.0f, 2.4f, 1.0f}},
    {0x12, {-50.0f, -70.0f, 0.0f}, {1.0f, 2.4f, 1.0f}},
    {0x13, {-50.0f, -70.0f, 0.0f}, {1.0f, 2.4f, 1.0f}},
    {0x15, {-50.0f, -70.0f, 0.0f}, {1.0f, 2.4f, 1.0f}},
};

static void* shop_clear_z;
static int shop_read_req;
Vec shop_pos_save;
void* shop_msg_buf[5];

// Scalar-reference store: the MEM has neither the struct nor the scalar flag, so sched1 makes every
// following load (the `sw->` call arguments AND the fixed-scalar `pG`) wait for it.


// Queues a Z clear before the case (OT 0xF) and before the shown item model (OT 0x14).
void shopClearZ(SUB_SCREEN* wk)
{
    AddOtDirect(0xF, &shop_clear_z, (void (*)()) clearZbuffer, 2, 0x1000, 0, 0.0f);
    AddOtDirect(0x14, &shop_clear_z, (void (*)()) clearZbuffer, 2, 0x1000, 0, 0.0f);
}

// Gives message slots 8..12 their 0x1000-byte queue buffers (shop_msg_buf) or detaches them (on 0).
void setShopMsgQueue(int on)
{
    void** buf = shop_msg_buf;
    int i;

    for (i = 0; i < 5; i++, buf++) {
        if (on) {
            cMes.MesRegistQueue(8 + i, (MesQue*) *buf);
        } else {
            cMes.MesReleaseQueue(8 + i);
        }
    }
}

// No merchant voice stream playing.
void shopStrInit(SUB_SCREEN* wk)
{
    wk->str_id = 0;
}

// Stops the merchant's current voice stream (str_id).
void shopStrStop(SUB_SCREEN* wk)
{
    u32 str = (u32) wk->str_id;

    if (str) {
        SndStrReq(str, 8, 0, 0);
    }
}

// Plays merchant voice stream `no` (ShopMsg::str; 0xFF = none), replacing the current one.
void shopStrPlay(SUB_SCREEN* wk, int no)
{
    shopStrStop(wk);
    if (no != 0xFF) {
        wk->str_id = SndStrReq(1, no, 3, 0, 0, 0.0f);
    } else {
        shopStrInit(wk);
    }
}

// Shop model managers sized to the case: piece_max + 4 model infos / MapMgr works (0 merchant,
// 1 shown item, 3 case, 4.. pieces), 0xBE parts.
void shopModelAlloc(SUB_SCREEN* wk)
{
    pzlPlayer* pl = wk->puzzlePlayer;
    int i;

    wk->attr_flag |= 1;
    ssModInfoMgr.roomInit();
    ssModInfoMgr.arrayAlloc(pl->m_piece_max + 4);
    ssPartsMgr.roomInit();
    ssPartsMgr.arrayAlloc(0xBE);
    cModel::mm = &ssModInfoMgr;
    cModel::pm = &ssPartsMgr;
    MapMgr.roomInit();
    MapMgr.arrayAlloc(pl->m_piece_max + 4);
    for (i = 0; i < pl->m_piece_max + 4; i++) {
        MapMgr.create(0, i);
    }
}

// Plays the merchant's coat-closing animation (list panel, price and caption units fade out);
// ShopTopMenu waits for it (coat flag).
void closeCoat(SUB_SCREEN* wk)
{
    IdUnit* u;

    wk->shop->coat = 1;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0x50, IDC_SSCRN_CKPT_2);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0xF4, IDC_SSCRN_CKPT_2);
    u->rev_flag |= 0xF;
}

// Shop loader: starts at the data read (the shop opens straight from the game).
void SsShopInit::init(SUB_SCREEN* wk)
{
    state = 2;
}

// Loads the shop: state 2 drops the models/lights, reads SS/<lang>/ss_shop.dat behind the ARAM
// image (-> pShop) and releases the previous id textures, 3 waits (pPzzlDat = the puzzle archive),
// 4 fades in and transits to SsShopMain.
void SsShopInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
    case 1:
    case 2:
        sscrnModelFree(wk);
        sscrnLightClear(wk);
        wk->pShopDat = (SsArc*) (wk->pFreeOffs + (u32) wk->pBuf);
        sscrnDataFilename(wk, "ss_shop.dat");
#line 312 "D:/Bio4/Prog/ss_shop.cpp"
        shop_read_req = DVD_READ_N(wk->filename, wk->pShopDat, 0, 0, 0, 0x10);
        if (shop_read_req <= 0) {
            break;
        }
        IdSub.kill(0xFF, IDC_SSCRN_0);
        IdTexRelease(TEX_OWNER_ID_SSCRN);
        state++;
    case 3: {
        int result;
        int size;

        if (Dvd.ReadCheck(shop_read_req, &result, &size, 0) != 1) {
            break;
        }
        wk->pPzzlDat = wk->pSwitchDat;
        state++;
    }
    case 4:
        FadeSetW(0x80000000, 5, 0, 0);
        transit(0, wk);
        break;
    }
}

// Builds the shop: the Merchant session over merchantChar, the widget graph (ShopTopMenu -> Sell /
// Buy / LvUp chains; BuyItemNum/BuyConfirm hand a piece to PzzlThinking -> PieceSelect -> BuyPuzzleEnd
// for case placement, CaseChange for a bought case), the id groups (case 0x10, shop 0x1C..0x1F,
// digits IdNum 0x40..), lights, the pzlPlayer of board_size, the model managers, the message
// queue buffers, the ShopWork; starts in the top menu.
void SsShopMain::init(SUB_SCREEN* wk)
{
    IdUnit* u;

    thinking = new PzzlThinking;
    select = new PieceSelect;
    caseChange = new CaseChange;
    topMenu = new ShopTopMenu;
    lvSel = new LvUpMenuSelect;
    lvItem = new LvUpItemSelect;
    lvConf = new LvUpConfirm;
    sellSel = new SellMenuSelect;
    sellNum = new SellItemNum;
    sellConf = new SellConfirm;
    buySel = new BuyMenuSelect;
    buyNum = new BuyItemNum;
    buyConf = new BuyConfirm;
    buyEnd = new BuyPuzzleEnd;
    thinking->connect(0, select);
    select->connect(1, thinking);
    select->connect(2, caseChange);
    select->connect(3, buyEnd);
    topMenu->connect(0, sellSel);
    topMenu->connect(1, buySel);
    topMenu->connect(2, lvSel);
    lvSel->connect(0, lvItem);
    lvSel->connect(1, topMenu);
    lvItem->connect(0, lvConf);
    lvItem->connect(1, lvSel);
    lvConf->connect(0, lvItem);
    sellSel->connect(0, sellNum);
    sellSel->connect(1, topMenu);
    sellNum->connect(0, sellConf);
    sellNum->connect(1, sellSel);
    sellConf->connect(0, sellSel);
    buySel->connect(0, buyNum);
    buySel->connect(1, topMenu);
    buyNum->connect(0, buyConf);
    buyNum->connect(1, buySel);
    buyNum->connect(2, thinking);
    buyConf->connect(0, buySel);
    buyConf->connect(1, caseChange);
    buyConf->connect(2, thinking);
    buyEnd->connect(0, buyConf);
    buyEnd->connect(1, buySel);
    caseChange->connect(0, buySel);
    wk->merchant = new Merchant(&merchantChar);
    {
        int i;
        for (i = 0; i < 5; i++) {
#line 442 "D:/Bio4/Prog/ss_shop.cpp"
            shop_msg_buf[i] = MEM_ALLOC(0x1000, 1, 13);
        }
    }
    setShopMsgQueue(1);
    puzzleCameraInit(wk, &pG->Camera);
    IdTexDataLoad(SS_ARC_PTR(wk->pPzzlDat, 0x1AA), TEX_OWNER_ID_SSCRN);
    IdTexDataLoad(SS_ARC_PTR(wk->pShopDat, 4), TEX_OWNER_ID_SSCRN);
    IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_0, 0xC, 6, 0);
    IdSub.set(SS_ARC_PTR(wk->pPzzlDat, 0x1AB), 0xFF, IDC_SSCRN_NEAR_0, 0xF, 0, 0);
    tempSpaceDisp(0);
    {
        int i;
        for (i = 0; i < 0x3E; i++) {
            if (i == 0) {
                IdNum.set(SS_ARC_PTR(wk->pCmmn, 8), 0xFF, IDC_NUM_00, 0x13, 8, 0);
            } else {
                IdNum.set(SS_ARC_PTR(wk->pCmmn, 8), 0xFF, 0x40 + i, 0x13, 9, 0);
            }
        }
    }
    IdSub.set(SS_ARC_PTR(wk->pShopDat, 6), 0xFF, IDC_SSCRN_CKPT_1, 0x13, 7, 0);
    {
        int i;
        for (i = 0; i < 5; i++) {
            IdSub.set(SS_ARC_PTR(wk->pShopDat, 8), 0xFF, 0x80 + i, 0x13, 5, 0);
        }
    }
    IdSub.set(SS_ARC_PTR(wk->pShopDat, 7), 0xFF, IDC_SSCRN_CKPT_0, 0x13, 4, 0);
    IdSub.set(SS_ARC_PTR(wk->pShopDat, 9), 0xFF, IDC_SSCRN_CKPT_3, 0x13, 3, 0);
    IdSub.set(SS_ARC_PTR(wk->pShopDat, 5), 0xFF, IDC_SSCRN_CKPT_2, 0x15, 0, 0);
    u = IdSub.unitPtr(0x52, IDC_SSCRN_CKPT_2);
    u->be_flag &= ~8;
    u = IdSub.unitPtr(0x56, IDC_SSCRN_CKPT_2);
    u->be_flag &= ~8;
    u = IdSub.unitPtr(0x57, IDC_SSCRN_CKPT_2);
    u->be_flag &= ~8;
    shop_pos_save = IdSub.unitPtr(0, IDC_SSCRN_PESETA)->pos0;
    levelItemDisp(wk, 0);
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_3)->be_flag &= ~8;
    {
        int i;
        for (i = 0; i < 4; i++) {
            IdSub.unitPtr(0x21 + i, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
        }
    }
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0);
    u->be_flag &= ~8;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1);
    u->be_flag &= ~8;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0x50, IDC_SSCRN_CKPT_2);
    u->rev_flag |= 0xF;
    u = IdSub.unitPtr(0xF4, IDC_SSCRN_CKPT_2);
    u->rev_flag |= 0xF;
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x12));
    wk->puzzlePlayer = new pzlPlayer;
    if (!wk->puzzlePlayer->init((s8) wk->board_size)) {
        delete wk->puzzlePlayer;
    }
    shopModelAlloc(wk);
    pieceModelInit(wk);
    {
#line 578 "D:/Bio4/Prog/ss_shop.cpp"
        ShopWork* sw = (ShopWork*) MEM_ALLOC(sizeof(ShopWork), 1, 13);
        wk->shop = sw;
        sw->coat = 0;
    }
    cur = topMenu;
    cur->init(wk);
    MesData.registData(0, (u8*) SS_ARC_PTR(wk->pCmmn, 5));
    MesData.registData(2, (u8*) SS_ARC_PTR(wk->pPzzlDat, 0x1A4));
}

// Shop frame: leaves at once (close_flag 0x10000) when the merchant data vanished; draws the case
// and pieces, runs the current widget, saves the case layout and the merchant state every frame;
// ShopTopMenu result 1 (B / exit entry) leaves.
void SsShopMain::move(SUB_SCREEN* wk)
{
    MerchantCharacter* mc = &merchantChar;

    if (mc->m_p_data == 0) {
        wk->close_flag |= 0x10000;
        transit(0, wk);
        return;
    }
    caseModelMove(0);
    shopClearZ(wk);
    pieceModelDisp(wk);
    pzzlCursorDisp(wk, 1);
    {
        Widget<SUB_SCREEN>* w = cur;
        w->move(wk);
        next = w->cur;
    }
    wk->puzzlePlayer->save();
    wk->merchant->makeList();
    wk->merchant->save(mc->m_p_data);
    if (cur == topMenu && topMenu->result == 1) {
        wk->close_flag |= 0x10000;
        transit(0, wk);
    }
    cur = next;
}

// Frees the pzlPlayer, ShopWork and message buffers, stops the voice stream.
void SsShopMain::quit(SUB_SCREEN* wk)
{
    int i;

    delete wk->puzzlePlayer;
    Mem_free(wk->shop);
    setShopMsgQueue(0);
    for (i = 0; i < 5; i++) {
        Mem_free(shop_msg_buf[i]);
    }
    shopStrStop(wk);
}

// Picks the merchant's greeting lines (shop_msg indices into `tbl`): the first ever visit (Scenario
// bit 22 set here) gets 0 + 2, the first visit of a session (Status_flg[2] bit 18) gets "new stock"
// (1) or the plain greeting (0) plus the village hints 3/4; 0 when nothing is to be said.
int getGreetMsg(int* num, int* tbl)
{
    GlobalWork* g = pG;
    SUB_SCREEN* wk = &SubScreenWk;
    int ret = 1;

    if (ScfFlagChk(g, SCF_ST1_NIGHT)) {
        goto NG;
    }
    if (!ScfFlagChk(g, SCF_CONTACT_MERCHANT)) {
        ScfFlagOn(g, SCF_CONTACT_MERCHANT);
        StaFlagOn(pG, STA_INTO_SHOP);
        *num = 0;
        tbl[(*num)++] = 0;
        tbl[(*num)++] = 2;
        return ret;
    }
    if (StaFlagChk(g, STA_INTO_SHOP)) {
        goto NG;
    }
    StaFlagOn(g, STA_INTO_SHOP);
    *num = 0;
    if (wk->merchant->stockNew() || wk->merchant->levelNew()) {
        tbl[(*num)++] = 1;
    } else {
        tbl[(*num)++] = 0;
    }
    g = pG;
    if (g->stage_no == 1) {
        if (!ScfFlagChk(g, SCF_ST1_SUB_MISSION)) {
            if (!ScfFlagChk(g, SCF_ST1_NIGHT) && g->game_cnt == 0) {
                tbl[(*num)++] = 3;
            }
        } else if (!ItfFlagChk(g, ITF_FN57)) {
            tbl[(*num)++] = 4;
        }
    }
    return ret;
NG:
    return 0;
}

// Top menu entry: rebuilds the merchant lists, clears the price/item displays and starts the
// greeting (state 0) or goes straight to the menu (state 1).
void ShopTopMenu::init(SUB_SCREEN* wk)
{
    int i;

    wk->merchant->makeList();
    for (i = 0; i < 5; i++) {
        dispPrice(0x80 + i, 0, 0, 0, 0);
    }
    dispItem(0, 0);
    IdSub.unitPtr(0, IDC_SSCRN_PESETA)->pos0 = shop_pos_save;
    exit = 0;
    if (getGreetMsg(&greetNum, greet)) {
        IdSub.unitPtr(0, IDC_SSCRN_CKPT_2)->be_flag |= 8;
        state = greetIdx = greetStep = 0;
    } else {
        IdSub.unitPtr(0, IDC_SSCRN_CKPT_2)->be_flag &= ~8;
        IdSub.unitPtr(0, IDC_SSCRN_CKPT_2)->rev_flag |= 0xF;
        state = 1;
    }
    shopStrInit(wk);
}

// Top menu: state 0 plays the greeting lines one by one (message + voice, A skips), 1 waits for
// the coat animation, 2 the menu: B leaves (result 1), A enters Sell (link 0, only with sellable
// items) / Buy (1) / Tune-up (2, only with tunable weapons) or the Exit row, up/down move between
// the three rows and the exit entry (cursor / exit), with the merchant's line per choice.
void ShopTopMenu::move(SUB_SCREEN* wk)
{
    result = 0;
    switch ((s8) state) {
    case 0:
        switch (greetStep) {
        case 0: {
            int no = greet[greetIdx];
            IdUnit* u;
            int x;
            int y;
            int msg = shop_msg[no].msg;
            int str = shop_msg[no].str;

            u = IdSub.unitPtr(0xFE, IDC_SSCRN_CKPT_2);
            x = (int) ((u->pos0.x + 320.0f) * 0.8f);
            y = (int) ((240.0f - u->pos0.y) * 0.8f);
            cMes.MesSet(msg, x, y, 0x20001, 0, 0, 3);
            greetStep = 1;
            shopStrPlay(wk, str);
            break;
        }
        case 1:
            if (cMes.GetMesStatus(result) & 2) {
                if (++greetIdx == greetNum) {
                    IdSub.unitPtr(0, IDC_SSCRN_CKPT_2)->be_flag &= ~8;
                    IdSub.unitPtr(0, IDC_SSCRN_CKPT_2)->rev_flag |= 0xF;
                    state = 1;
                    SndCall(0, 0xA, 0, 0, 0, 0);
                } else {
                    greetStep = 0;
                }
            }
            break;
        }
        break;
    case 1:
        if (IdSub.unitPtr(0x50, IDC_SSCRN_CKPT_2)->anima_state & 1) {
            if (wk->shop->coat) {
                wk->shop->coat = 0;
                SndCall(0, 0x17, 0, 0, 0, 0);
            }
            state = 2;
        }
        break;
    case 2:
        if (Key.trg & 0x40000000) {
            result = 1;
            goto END;
        }
        if (Key.trg & 0x80000000) {
            int ok = 1;

            wk->shop->cursor = 0;
            wk->shop->top = 0;
            if (exit == 0) {
                switch ((s8) cursor) {
                case 0:
                    if (wk->merchant->exerciseItemNum()) {
                        transit(0, wk);
                    } else {
                        ok = 0;
                    }
                    break;
                case 1:
                    transit(1, wk);
                    break;
                case 2:
                    if (wk->merchant->levelupItemNum()) {
                        transit(2, wk);
                    } else {
                        ok = 0;
                    }
                    break;
                }
            } else {
                result = 1;
                IdSub.unitPtr(0x52, IDC_SSCRN_CKPT_2)->be_flag &= ~8;
                goto END;
            }
            if (exit == 0 && ok == 1) {
                IdUnit* sel = 0;
                IdUnit* u;
                int str;

                IdSub.unitPtr(0x10, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
                IdSub.unitPtr(0x11, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
                IdSub.unitPtr(0x12, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
                switch ((s8) cursor) {
                case 0:
                    sel = IdSub.unitPtr(0x11, IDC_SSCRN_CKPT_0);
                    break;
                case 1:
                    sel = IdSub.unitPtr(0x12, IDC_SSCRN_CKPT_0);
                    break;
                case 2:
                    sel = IdSub.unitPtr(0x10, IDC_SSCRN_CKPT_0);
                    break;
                }
                sel->be_flag |= 8;
                u = IdSub.unitPtr(0x50, IDC_SSCRN_CKPT_2);
                u->rev_flag &= 0xF0;
                u = IdSub.unitPtr(0xF4, IDC_SSCRN_CKPT_2);
                u->rev_flag &= 0xF0;
                u = IdSub.unitPtr(0, IDC_SSCRN_PESETA);
                u->pos0 = IdSub.unitPtr(7, IDC_SSCRN_CKPT_1)->pos0;
                IdSub.unitPtr(0x52, IDC_SSCRN_CKPT_2)->be_flag &= ~8;
                switch ((s8) cursor) {
                case 0:
                    str = 5;
                    break;
                case 2:
                    str = 0x16;
                    break;
                case 1:
                default:
                    str = 9;
                    break;
                }
                shopStrPlay(wk, shop_msg[str].str);
                SndCall(0, 0x16, 0, 0, 0, 0);
            }
            goto END;
        }
        {
            s8 oldCursor = cursor;
            s8 oldExit = exit;

            if (exit == 0) {
                if (Key.trg & 0x08000000) {
                    cursor--;
                } else if (Key.trg & 0x04000000) {
                    cursor++;
                } else if (Key.trg & 0x02000000) {
                    exit = 1;
                }
            } else if (Key.trg & 0x01000000) {
                exit = 0;
            }
            cursor = cursor < 0 ? 0 : (cursor > 2 ? 2 : cursor);
            if (oldCursor != cursor || oldExit != exit) {
                SndCall(0, 0xA, 0, 0, 0, 0);
            }
        }
        break;
    }
    {
        IdUnit* mark = IdSub.unitPtr(0x52, IDC_SSCRN_CKPT_2);
        IdUnit* u;

        if (exit == 0) {
            switch ((s8) cursor) {
            case 0:
                mark->pos0 = IdSub.unitPtr(0x51, IDC_SSCRN_CKPT_2)->pos0;
                break;
            case 1:
                mark->pos0 = IdSub.unitPtr(0x55, IDC_SSCRN_CKPT_2)->pos0;
                break;
            case 2:
                mark->pos0 = IdSub.unitPtr(0x53, IDC_SSCRN_CKPT_2)->pos0;
                break;
            }
        } else {
            mark->pos0 = IdSub.unitPtr(0x54, IDC_SSCRN_CKPT_2)->pos0;
        }
        if (state == 2) {
            mark->be_flag |= 8;
        } else {
            mark->be_flag &= ~8;
        }
        u = IdSub.unitPtr(0x56, IDC_SSCRN_CKPT_2);
        if (wk->merchant->stockNew()) {
            u->be_flag |= 8;
        } else {
            u->be_flag &= ~8;
        }
        u = IdSub.unitPtr(0x57, IDC_SSCRN_CKPT_2);
        if (wk->merchant->levelNew()) {
            u->be_flag |= 8;
        } else {
            u->be_flag &= ~8;
        }
    }
END:;
}

// Draws the sell list: scroll bar, `n` rows of item icon / name / count and the buy-up price
// (dispPrice 0x80 + row), the cursor row highlighted when `cursor`.
void dispSellItemList(SUB_SCREEN* wk, int n, int cursor)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    int top = sw->top;
    int i;
    int end;
    ItemWork* item;
    PriceEntry* pe;
    int row;

    {
        int k;
        for (k = 0; k < 5; k++) {
            IdSub.unitPtr(0x80 + k, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
        }
    }
    dispScrollBar(top, n, sw->num, IdSub.unitPtr(0xF8, IDC_SSCRN_CKPT_1), IdSub.unitPtr(0xFD, IDC_SSCRN_CKPT_1),
                  IdSub.unitPtr(0xFE, IDC_SSCRN_CKPT_1));
    i = top;
    // The LOOP_END note keeps the `add end` below the dispScrollBar call (sched1 hoists a free add).
    do { end = i + n; } while (0);
    // `goto TEST` into the `while (1)` makes loop.c reject the loop (no hoisting, no givs) while the
    // loop notes still weight every body reference (global-alloc order of sw/col/price/m).
    goto TEST;
    while (1) {
        int num;
        int col;
        IdUnit* frame;
        IdUnit* text;
        int price;
        int id;
        int x;
        int y;
        u8 slot;

        {
            ItemInfo info;
            itemInfo(pe->id, &info);
            if (info.type == 1) {
                num = 1;
            } else {
                num = ItemMgr.num(pe->id);
            }
        }
        col = 6;
        if (item) {
            col = 0;
        }
        frame = IdSub.unitPtr(row + 0x40, IDC_SSCRN_CKPT_1);
        text = IdSub.unitPtr(row, IDC_SSCRN_CKPT_1);
        if (i == sw->cursor) {
            PSVECAdd(&frame->pParent->pos, &frame->pos0, &IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1)->pos0);
        }
        {
            IdUnit* mark = IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1);
            if (cursor) {
                mark->be_flag |= 8;
            } else {
                mark->be_flag &= ~8;
            }
        }
        if (item) {
            id = item->id;
            price = m->buyupPrice(item, 1);
        } else {
            id = pe->id;
            price = m->buyupPrice(id, 1);
        }
        {
            Vec pos;
            PSVECAdd(&text->pParent->pos, &text->pos0, &pos);
            x = (int) ((pos.x + 320.0f) * 0.8f);
            y = (int) ((240.0f - pos.y) * 0.8f);
        }
        slot = row + 8;
        cMes.setLayout(slot, LAYOUT_SHOP_LIST);
        cMes.MesSet(id, x, y, 0x200A8, slot, col, 4);
        cMes.MesSetOt(slot, 0x13, 6);
        IdSub.unitPtr(row + 0x80, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
        if (i == sw->cursor) {
            if (itemType(pe->id) == 1) {
                if (item) {
                    weaponLevelDisp(item, item->id, 1, 1);
                } else {
                    weaponLevelDisp(0, pe->id, 1, 1);
                }
            } else {
                weaponLevelDisp(0, pe->id, 0, 1);
            }
            stockNumDisp(0, 0);
        }
        {
            IdUnit* u = IdSub.unitPtr(row + 0x40, IDC_SSCRN_CKPT_1);
            Vec pos;
            PSVECAdd(&u->pParent->pos, &u->pos0, &pos);
            dispPrice(row + 0x80, num, price, &pos, price_disp_num | price_disp_price);
        }
        i++;
    TEST:
        if (!(i < end)) {
            break;
        }
        item = m->exerciseItemPtr(i);
        row = i - top;
        pe = m->exerciseItemNo(i);
        if (pe == 0) {
            break;
        }
    }
}

// Clamps the list cursor to 0..num-1 and keeps it inside the 5-row window (top).
void listRangeCheck(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;

    if (sw->num == 0) {
        sw->cursor = 0;
    } else {
        int c = sw->cursor;
        if (c >= 0) {
            if (c > sw->num - 1) {
                c = sw->num - 1;
            }
        } else {
            c = 0;
        }
        sw->cursor = c;
    }
    if (sw->cursor > sw->top + 4) {
        sw->top = sw->cursor - 4;
    }
    if (sw->cursor < sw->top) {
        sw->top = sw->cursor;
    }
    if (sw->num > 4 && sw->top + 5 > sw->num) {
        sw->top = sw->num - 5;
    }
}

// Sell list open: the merchant's exercise (buy-up) list, list panel slide-in, the cursor item shown.
void SellMenuSelect::init(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;
    int i;

    m->makeList();
    sw->num = wk->merchant->exerciseItemNum();
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0);
    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1);
    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    for (i = 0; i < 5; i++) {
        dispPrice(0x80 + i, 0, 0, 0, 0);
    }
    listRangeCheck(wk);
    if (m->exerciseItemNo(sw->cursor)) {
        dispItem(m->exerciseItemNo(sw->cursor)->id, 1);
    } else {
        dispItem(0xFFFF, 0);
    }
    IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
    state = 0;
}

// Sell list: B (or an empty list) closes the coat and returns to the top menu; state 0 waits for
// the panel, 1: A picks the item (sw->item -> SellItemNum), up/down move the cursor.
void SellMenuSelect::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;

    if (sw->num > 0) {
        moveItem();
        itemCaption(m->exerciseItemNo(sw->cursor)->id);
        dispSellItemList(wk, 5, state != 0);
    }
    if ((Key.trg & 0x40000000) || sw->num == 0) {
        transit(1, wk);
        closeCoat(wk);
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    switch (state) {
    case 0:
        if (IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->anima_state & 1) {
            state = 1;
        }
        break;
    case 1:
        if (Key.trg & 0x80000000) {
            sw->item = m->exerciseItemPtr(sw->cursor);
            if (sw->item) {
                transit(0, wk);
                SndCall(0, 4, 0, 0, 0, 0);
                break;
            }
        }
        {
            int old = sw->cursor;

            if (Key.rep & 0x01000000) {
                sw->cursor = old - 1;
            }
            if (Key.rep & 0x02000000) {
                sw->cursor++;
            }
            listRangeCheck(wk);
            if (old != sw->cursor) {
                SndCall(0, 6, 0, 0, 0, 0);
                if (old != sw->cursor) {
                    dispItem(m->exerciseItemNo(sw->cursor)->id, 1);
                }
            }
        }
        break;
    }
}

// Nothing to release.
void SellMenuSelect::quit(SUB_SCREEN* wk) {}

// Sell count entry: starts at 1, hides the panel highlight.
void SellItemNum::init(SUB_SCREEN* wk)
{
    wk->shop->count = 1;
    IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
    repeat = 0;
    fast = 0;
}

// Sell count: draws the count and total buy-up price digits; B back, A opens the confirm message
// (shop_msg 6, or 7 "a valuable one" for expensive items; count 0 cancels), up/down change the
// count (held stick steps faster, wraps 1 <-> max).
void SellItemNum::move(SUB_SCREEN* wk)
{
    // COMPILER-DIFF: candidate #17. The target allocates `val` (17 refs / 270 insns) above `this`
    // (27 / 386): this r24, val r25. Pinning `this` to r24 (every member access below goes through
    // `self`) keeps r24 away from val, which then takes r25; a pin on val itself ties the `val % 10`
    // remainder to r25 (`sub 25,25,0`).
    register SellItemNum* self asm("r24") = this;
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;
    int x;
    int y;
    int k;
    int val = 0;
    int n = 0;
    int base = 0;
    int max;
    // COMPILER-DIFF: #13. `zero` is set once at the top of the k loop and stored once after it
    // (`self->fast = zero`): loop.c hoists the `li` to the k preheader, where sched1 issues it in
    // the MesSet call's free slot (the slot the PRE'd `&digit` addi took), and local-alloc's
    // update_equiv_regs then moves the `li` next to the store (no store-immediate), so the addi
    // lands after `li r5,31` like the target and the store keeps its `li r0,0`.
    int zero;

    dispSellItemList(wk, 5, 1);
    u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
    x = (int) ((u->pos0.x + 320.0f) * 0.8f);
    y = (int) ((240.0f - u->pos0.y) * 0.8f);
    cMes.setLayout(0, LAYOUT_SUBSCRN);
    cMes.MesSet(sw->item->id, x, y, 0x20088, 0, 0, 4);
    u = IdSub.unitPtr(0, IDC_SSCRN_CKPT_3);
    u->be_flag |= 8;
    u->pos0 = sell_num_pos;
    for (k = 0; k <= 1; k++) {
        int digit[10];
        int on;

        zero = 0; // COMPILER-DIFF: #13 (see above)
        switch (k) {
        case 0:
            n = 4;
            val = sw->count;
            base = 1;
            break;
        case 1:
            n = 7;
            base = 0x11;
            val = m->buyupPrice(sw->item, sw->count);
            break;
        }
        for (int i = 0; i < n; i++) {
            digit[i] = val % 10;
            val /= 10;
            IdSub.unitPtr(base + i, IDC_SSCRN_CKPT_3)->be_flag &= ~8;
        }
        on = 0;
        for (int i = n - 1; i >= 0; i--) {
            if (on == 0) {
                if (digit[i] == 0 && i != 0) {
                    continue;
                }
                on = 1;
            }
            u = IdSub.unitPtr(base + i, IDC_SSCRN_CKPT_3);
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = digit[i];
        }
    }
    {
        if (itemType(sw->item->id) == 1) {
            max = 1;
        } else {
            max = ItemMgr.num(sw->item->id);
        }
    }
    if (Key.trg & 0x40000000) {
        IdSub.unitPtr(0, IDC_SSCRN_CKPT_3)->be_flag &= ~8;
        self->transit(1, wk);
        SndCall(0, 5, 0, 0, 0, 0);
    } else if (Key.trg & 0x80000000) {
        if (sw->count == 0) {
            IdSub.unitPtr(0, IDC_SSCRN_CKPT_3)->be_flag &= ~8;
            self->transit(1, wk);
            SndCall(0, 5, 0, 0, 0, 0);
        } else {
            int msg = 6;
            int price = m->buyupPrice(sw->item, 1);

            if (pG->stage_no > 1) {
                if (price > 29999) {
                    msg = 7;
                }
            } else if (price > 9999) {
                msg = 7;
            }
            u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
            {
                int px = (int) ((u->pos0.x + 320.0f) * 0.8f) + sell_msg_x;
                cMes.MesSet(shop_msg[msg].msg, px, (int) ((240.0f - u->pos0.y) * 0.8f) + sell_msg_y, 0x20801, 1, 0, 3);
            }
            cMes.m_Msg[1].m_cur = 1;
            self->transit(0, wk);
            SndCall(0, 9, 0, 0, 0, 0);
            shopStrPlay(wk, shop_msg[msg].str);
        }
    } else {
        int old = sw->count;

        if (old == 1 && (Key.trg & 0x02000000)) {
            sw->count = max;
        } else {
            int step;

            if (sw->count == max && (Key.trg & 0x01000000)) {
                sw->count = 1;
            } else {
                if (Key.on & 0x03000000) {
                    self->repeat++;
                    if (self->repeat > 30) {
                        self->repeat = 30;
                        self->fast = 1;
                    } else {
                        self->fast = zero; // COMPILER-DIFF: #13 (see above)
                    }
                } else {
                    self->repeat = 0;
                    self->fast = 0;
                }
                step = self->fast ? 8 : 1;
                if (Key.rep & 0x02000000) {
                    sw->count -= step;
                } else if (Key.rep & 0x01000000) {
                    sw->count += step;
                }
                {
                    int min = 1;
                    int c = sw->count;
                    if (c >= min) {
                        if (c > max) {
                            c = max;
                        }
                    } else {
                        c = min;
                    }
                    sw->count = c;
                }
            }
        }
        if (old != sw->count) {
            SndCall(0, 0xA, 0, 0, 0, 0);
        }
    }
}

// Sell yes/no: yes sells `count` of the item (Merchant::buyup adds pesetas, the items are dumped,
// the armed weapon unequipped if gone), rebuilds the case and plays the thanks line; B or no
// returns to the list.
void SellConfirm::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;
    int x;
    int y;

    dispSellItemList(wk, 5, 1);
    u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
    x = (int) ((u->pos0.x + 320.0f) * 0.8f);
    y = (int) ((240.0f - u->pos0.y) * 0.8f);
    cMes.setLayout(0, LAYOUT_SUBSCRN);
    cMes.MesSet(sw->item->id, x, y, 0x20088, 0, 0, 4);
    if (Key.trg & 0x40000000) {
        cMes.Delete(1);
        transit(0, wk);
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    result = cMes.GetSelectMessage(1);
    if (result == 0) {
        return;
    }
    switch (result) {
    case 1: {

        m->buyup(sw->item, sw->count, (int*) &pG->peseta);
        if (itemType(sw->item->id) == 1) {
            ItemMgr.dumpAll(sw->item);
        } else {
            u16 left = (u16) sw->count;
            u16 id = sw->item->id;
            ItemWork* p;
            int i;

            for (;;) {
                p = ItemMgr.minimumSearch(id);
                if (p->num >= left) {
                    for (i = 0; i < left; i++) {
                        ItemMgr.dump(p);
                    }
                    break;
                }
                left -= p->num;
                ItemMgr.dumpAll(p);
            }
            if (id == ItemMgr.weaponId() && ItemMgr.num(id) == 0) {
                ItemMgr.arm(0);
            }
        }
        wk->puzzlePlayer->rehash();
        shopStrPlay(wk, shop_msg[8].str);
        SndCall(0, 0x25, 0, 0, 0, 0);
        break;
    }
    case 2:
        SndCall(0, 5, 0, 0, 0, 0);
        break;
    }
    transit(0, wk);
}

// Hides the confirm frame.
void SellConfirm::quit(SUB_SCREEN* wk)
{
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_3)->be_flag &= ~8;
}

// Draws the buy list: scroll bar, `n` rows of icon / name, sold-out or stock marks and the selling
// price (dispPrice 0x80 + row), cursor row highlighted when `cursor`.
void dispBuyItemList(SUB_SCREEN* wk, int n, int cursor)
{
    Merchant* m = wk->merchant;
    ShopWork* sw = wk->shop;
    int top = sw->top;
    int i;
    int end;
    PriceEntry* pe;
    int row;

    {
        int k;
        for (k = 0; k < 5; k++) {
            IdSub.unitPtr(0x80 + k, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
        }
    }
    dispScrollBar(top, n, sw->num, IdSub.unitPtr(0xF8, IDC_SSCRN_CKPT_1), IdSub.unitPtr(0xFD, IDC_SSCRN_CKPT_1),
                  IdSub.unitPtr(0xFE, IDC_SSCRN_CKPT_1));
    i = top;
    // The LOOP_END note keeps the `add end` below the dispScrollBar call (sched1 hoists a free add).
    do { end = i + n; } while (0);
    // `goto TEST` into the `while (1)` makes loop.c reject the loop (no hoisting, no givs) while the
    // loop notes still weight every body reference (global-alloc order of sw/col/price/m).
    goto TEST;
    while (1) {
        int col;
        IdUnit* frame;
        IdUnit* text;
        int price;
        int stock;
        int id;
        int x;
        int y;
        u8 slot;

        col = 0;
        if (wk->merchant->stockNum(pe->id) < wk->merchant->sellUnit(pe->id)) {
            col = 6;
        }
        frame = IdSub.unitPtr(row + 0x40, IDC_SSCRN_CKPT_1);
        text = IdSub.unitPtr(row, IDC_SSCRN_CKPT_1);
        if (i == sw->cursor) {
            PSVECAdd(&frame->pParent->pos, &frame->pos0, &IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1)->pos0);
        }
        {
            IdUnit* mark = IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1);
            if (cursor) {
                mark->be_flag |= 8;
            } else {
                mark->be_flag &= ~8;
            }
        }
        id = pe->id;
        price = m->sellPrice(id, 1);
        stock = wk->merchant->stockNum(pe->id);
        {
            Vec pos;
            PSVECAdd(&text->pParent->pos, &text->pos0, &pos);
            x = (int) ((pos.x + 320.0f) * 0.8f);
            y = (int) ((240.0f - pos.y) * 0.8f);
        }
        slot = row + 8;
        cMes.setLayout(slot, LAYOUT_SHOP_LIST);
        cMes.MesSet(id, x, y, 0x200A8, slot, col, 4);
        cMes.MesSetOt(slot, 0x13, 6);
        if (wk->merchant->stockNew(pe->id)) {
            IdSub.unitPtr(row + 0x80, IDC_SSCRN_CKPT_1)->be_flag |= 8;
        }
        if (pe->id == 0x40) {
            IdSub.unitPtr(row + 0x80, IDC_SSCRN_CKPT_1)->be_flag |= 8;
        }
        if (i == sw->cursor) {
            if (itemType(pe->id) == 1) {
                weaponLevelDisp(0, pe->id, 1, 1);
            } else {
                weaponLevelDisp(0, pe->id, 0, 1);
            }
            stockNumDisp(stock, 1);
        }
        {
            IdUnit* u = IdSub.unitPtr(row + 0x40, IDC_SSCRN_CKPT_1);
            Vec pos;
            u32 flags;
            PSVECAdd(&u->pParent->pos, &u->pos0, &pos);
            if (stock) {
                flags = price_disp_price;
            } else {
                flags = price_disp_price | price_disp_sold;
            }
            dispPrice(row + 0x80, 0, price, &pos, flags);
        }
        i++;
    TEST:
        if (!(i < end)) {
            break;
        }
        row = i - top;
        pe = m->sellingItemNo(i);
        if (pe == 0) {
            break;
        }
    }
}

// Buy list open: the merchant's selling list, panel slide-in, the cursor item shown.
void BuyMenuSelect::init(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;
    int i;

    m->makeList();
    sw->num = wk->merchant->sellingItemNum();
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0);
    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1);
    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    for (i = 0; i < 5; i++) {
        dispPrice(0x80 + i, 0, 0, 0, 0);
    }
    dispItem(m->sellingItemNo(sw->cursor)->id, 1);
    IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
    state = 0;
}

// Buy list: B closes the coat and returns to the top menu; state 0 waits for the panel, 1: A picks
// an in-stock item (buyId -> BuyItemNum), up/down move the cursor.
void BuyMenuSelect::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;

    moveItem();
    itemCaption(m->sellingItemNo(sw->cursor)->id);
    dispBuyItemList(wk, 5, state != 0);
    if (Key.trg & 0x40000000) {
        transit(1, wk);
        closeCoat(wk);
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    switch (state) {
    case 0:
        if (IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->anima_state & 1) {
            state = 1;
        }
        break;
    case 1:
        if (Key.trg & 0x80000000) {
            u16 id = m->sellingItemNo(sw->cursor)->id;

            if (wk->merchant->stockNum(id) >= wk->merchant->sellUnit(id)) {
                sw->buyId = id;
                transit(0, wk);
                break;
            }
        }
        {
            int old = sw->cursor;

            if (Key.rep & 0x01000000) {
                sw->cursor = old - 1;
            }
            if (Key.rep & 0x02000000) {
                sw->cursor++;
            }
            {
                int c = sw->cursor;
                if (c >= 0) {
                    if (c > sw->num - 1) {
                        c = sw->num - 1;
                    }
                } else {
                    c = 0;
                }
                sw->cursor = c;
            }
            if (old != sw->cursor) {
                SndCall(0, 6, 0, 0, 0, 0);
            }
            if (sw->cursor > sw->top + 4) {
                sw->top = sw->cursor - 4;
            }
            if (sw->cursor < sw->top) {
                sw->top = sw->cursor;
            }
            if (old != sw->cursor) {
                dispItem(m->sellingItemNo(sw->cursor)->id, 1);
            }
        }
        break;
    }
}

// Nothing to release.
void BuyMenuSelect::quit(SUB_SCREEN* wk) {}

// Buy count entry: count = the item's sell unit, stock from the merchant.
void BuyItemNum::init(SUB_SCREEN* wk)
{
    unit = wk->merchant->sellUnit(wk->shop->buyId);
    stock = wk->merchant->stockNum(wk->shop->buyId);
    wk->shop->count = unit;
    state = 0;
    IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
}

// Buy step: state 0 picks the merchant's line (per-item pitch 0xB..0x12, 0xA default, 0x13 not
// enough pesetas; items without a piece shape are bought directly) and opens the confirm; state 1
// yes: a case-piece item is handed to PzzlThinking for placement (link 2), a case is bought (link
// 0 -> BuyConfirm/CaseChange), else buyItem; B / no back to the list.
void BuyItemNum::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;

    dispBuyItemList(wk, 5, 1);
    switch (state) {
    case 0:
        if (searchItemPieceData(sw->buyId, piece_info)) {
            state = 1;
            if ((int) pG->peseta >= m->sellPrice(sw->buyId, sw->count)) {
                switch (sw->buyId) {
                case 0x3:
                    msg = 0xB;
                    break;
                case 0x34:
                    msg = 0xC;
                    break;
                case 0x36:
                    msg = 0xD;
                    break;
                case 0x37:
                    msg = 0xE;
                    break;
                case 0x21:
                    if (ScfFlagChk(pG, SCF_ST1_SUB_MISSION) && !ItfFlagChk(pG, ITF_FN57)) {
                        msg = 0x10;
                    } else {
                        msg = 0xF;
                    }
                    break;
                case 0x25:
                    msg = 0x11;
                    break;
                case 0x29:
                    msg = 0x12;
                    break;
                default:
                    msg = 0xA;
                    break;
                }
                SndCall(0, 9, 0, 0, 0, 0);
            } else {
                msg = 0x13;
                SndCall(0, 0x19, 0, 0, 0, 0);
            }
            {
                IdUnit* u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
                int x = (int) ((u->pos0.x + 320.0f) * 0.8f);
                int y = (int) ((240.0f - u->pos0.y) * 0.8f);

                x += buy_msg_x;
                if (msg == 0xA || msg == 0x13) {
                    y += buy_msg_y;
                }
                cMes.MesSet(shop_msg[msg].msg, x, y, 0x20801, 1, 0, 3);
                shopStrPlay(wk, shop_msg[msg].str);
            }
        } else {
            sw->placed = 0;
            sw->noRoom = 0;
            transit(0, wk);
            break;
        }
    case 1:
        // The cancel arm stops the stream too: its `bl shopStrStop; b END` tail is cross-jumped
        // into the switch's shared tail and then its `li r8,0; bl SndCall` into case 1's.
        if (Key.trg & 0x40000000) {
            cMes.Delete(1);
            transit(1, wk);
            SndCall(0, 5, 0, 0, 0, 0);
            shopStrStop(wk);
            break;
        }
        if (msg != 0x13) {
            result = cMes.GetSelectMessage(1);
            if (result == 0) {
                break;
            }
            sw->placed = 0;
            switch (result) {
            case 1: {
                pzlPlayer* pl;
                pzlPiece* p;
                int w;
                int h;
                int x;
                int y;

                ItemMgr.construct(&sw->buy, sw->buyId);
                sw->buy.flags |= 1;
                sw->buy.num = (u16) sw->count;
                wk->puzzlePlayer->appendExtraPiece(&sw->buy);
                wk->puzzlePlayer->inHandExtraPiece();
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
                wk->back2 = 1;
                sw->placed = 1;
                dispItem(0, 0);
                transit(2, wk);
                IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0)->rev_flag |= 0xF;
                IdSub.unitPtr(0xF9, IDC_SSCRN_CKPT_0)->rev_flag |= 0xF;
                SndCall(0, 0x18, 0, 0, 0, 0);
                break;
            }
            case 2:
                transit(1, wk);
                SndCall(0, 5, 0, 0, 0, 0);
                break;
            }
            shopStrStop(wk);
        } else if (Key.trg & 0x80000000) {
            cMes.Delete(1);
            transit(1, wk);
            SndCall(0, 5, 0, 0, 0, 0);
        }
        break;
    }
}

// Clears the price displays.
void BuyItemNum::quit(SUB_SCREEN* wk)
{
    int i;

    for (i = 0; i < 5; i++) {
        dispPrice(0x80 + i, 0, 0, 0, 0);
    }
}

// Removes the bought-but-unplaced extra piece from the case player; 1 when one was removed.
int deleteExtraPiece(SUB_SCREEN* wk)
{
    pzlPlayer* pl = wk->puzzlePlayer;

    if (pl->m_extra) {
        if (pl->removeExtraPiece()) {
            return 1;
        }
    }
    return 0;
}

// Completes a purchase: Merchant::sell takes the pesetas, ItemMgr.get adds `count`; a placed piece
// copies its case position from the template (sw->buy); a bought case (0x7C..0x7F) sets board_next
// and returns 1 (the caller runs CaseChange). Saves the layout.
int buyItem(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    int ret = 0;

    wk->merchant->sell(sw->buyId, sw->count, (int*) &pG->peseta);
    ItemMgr.get(sw->buyId, (u16) sw->count);
    if (sw->placed) {
        ItemWork* p = ItemMgr.newbie();
        if (p) {
            p->x = sw->buy.x;
            p->y = sw->buy.y;
            p->orient = sw->buy.orient;
            p->board = sw->buy.board;
            wk->puzzlePlayer->m_extra->item = p;
        }
    } else {
        ItemWork* p = ItemMgr.newbie();
        if (p) {
            switch (p->id) {
            case 0x7C:
                wk->board_next = ret;
                ret = 1;
                break;
            case 0x7D:
                wk->board_next = 1;
                ret = 1;
                break;
            case 0x7E:
                wk->board_next = 2;
                ret = 1;
                break;
            case 0x7F:
                wk->board_next = 3;
                ret = 1;
                break;
            }
        }
    }
    wk->puzzlePlayer->save();
    return ret;
}

// Buy confirm open: message 0xA (affordable) / 0x13 (too expensive) / 0x14 (no room in the case,
// noRoom) with the merchant's voice.
void BuyConfirm::init(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;

    if (sw->noRoom == 0) {
        if ((int) pG->peseta >= m->sellPrice(sw->buyId, sw->count)) {
            msg = 0xA;
            SndCall(0, 9, 0, 0, 0, 0);
        } else {
            msg = 0x13;
            SndCall(0, 0x19, 0, 0, 0, 0);
        }
    } else {
        msg = 0x14;
    }
    u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
    {
        int x = (int) ((u->pos0.x + 320.0f) * 0.8f) + buy_conf_x;
        cMes.MesSet(shop_msg[msg].msg, x, (int) ((240.0f - u->pos0.y) * 0.8f) + buy_conf_y, 0x20801, 1, 0, 3);
    }
    shopStrPlay(wk, shop_msg[msg].str);
}

// Buy confirm: yes buys (buyItem; a case -> CaseChange link 1, a piece -> back to placement link
// 2), no / B drop the extra piece and return to the list; the no-room message only waits for A.
void BuyConfirm::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    int act;

    if (sw->noRoom == 0) {
        dispBuyItemList(wk, 5, 1);
    }
    act = 0;
    if (Key.trg & 0x40000000) {
        if (sw->noRoom == 0) {
            act = 1;
        } else {
            act = 3;
        }
        SndCall(0, 5, 0, 0, 0, 0);
    } else if (msg == 0x13) {
        if (Key.trg & 0x80000000) {
            act = 1;
            SndCall(0, 5, 0, 0, 0, 0);
        }
    } else {
        result = cMes.GetSelectMessage(1);
        if (result) {
            switch (result) {
            case 1:
                if (sw->noRoom == 0) {
                    act = 2;
                    SndCall(0, 0x27, 0, 0, 0, 0);
                } else {
                    act = 1;
                    SndCall(0, 4, 0, 0, 0, 0);
                }
                break;
            case 2:
                if (sw->noRoom == 0) {
                    act = 1;
                    SndCall(0, 5, 0, 0, 0, 0);
                } else {
                    act = 3;
                    SndCall(0, 0xD, 0, 0, 0, 0);
                }
                break;
            }
        }
    }
    if (!(act > 3)) {
        int min = 1;  // a literal folds to `<= 0`; the target keeps `cmpwi 1; blt`
        if (!(act < min)) {
            cMes.Delete(1);
        }
    }
    switch (act) {
    case 1:
        if (sw->placed) {
            deleteExtraPiece(wk);
            tempSpaceDisp(0);
            Cckpt.lifeMeterFrameIn();
            IdSub.unitPtr(0, IDC_SSCRN_PESETA)->rev_flag &= 0xF0;
        }
        transit(0, wk);
        break;
    case 2:
        if (buyItem(wk)) {
            transit(1, wk);
        } else {
            transit(0, wk);
        }
        break;
    case 3:
        wk->puzzlePlayer->m_space->rmPiece(wk->puzzlePlayer->m_extra);
        wk->puzzlePlayer->inHandExtraPiece();
        transit(2, wk);
        IdSub.unitPtr(0xF9, IDC_SSCRN_CKPT_0)->rev_flag |= 0xF;
        break;
    }
}

// Shows the "placed" frame unit after the piece placement.
void BuyPuzzleEnd::init(SUB_SCREEN* wk)
{
    IdUnit* u = IdSub.unitPtr(0xF9, IDC_SSCRN_CKPT_0);

    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
}

// After placement: the piece on the case board completes the purchase (buyItem, thanks line ->
// BuySel link 1); still in the space -> noRoom and back to BuyConfirm (link 0).
void BuyPuzzleEnd::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;

    if (wk->puzzlePlayer->m_board->search(wk->puzzlePlayer->m_extra)) {
        buyItem(wk);
        transit(1, wk);
        shopStrPlay(wk, shop_msg[21].str);
        SndCall(0, 5, 0, 0, 0, 0);
    } else {
        sw->noRoom = 1;
        transit(0, wk);
        SndCall(0, 9, 0, 0, 0, 0);
    }
}

// Draws the tune-up weapon list: scroll bar, `n` rows of icon / name, cursor row highlighted.
void dispLvUpItemList(SUB_SCREEN* wk, int n, int cursor)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    int top = sw->top;
    int i;
    int end;

    {
        int k;
        for (k = 0; k < 5; k++) {
            dispPrice(0x80 + k, 0, 0, 0, 0);
        }
    }
    dispScrollBar(top, n, sw->num, IdSub.unitPtr(0xF8, IDC_SSCRN_CKPT_1), IdSub.unitPtr(0xFD, IDC_SSCRN_CKPT_1),
                  IdSub.unitPtr(0xFE, IDC_SSCRN_CKPT_1));
    end = top + n;
    {
        int k;
        for (k = 0; k < 5; k++) {
            IdSub.unitPtr(0x80 + k, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
        }
    }
    stockNumDisp(0, 0);
    for (i = top; i < end; i++) {
        ItemWork* item = m->levelupItemPtr(i);
        LevelEntry* le = m->levelupItemNo(i);
        int row = i - top;
        int col;
        IdUnit* frame;
        IdUnit* text;
        int id;
        int x;
        int y;
        int slot;

        if (le == 0) {
            break;
        }
        if (item) {
            col = 6;
            if (m->tunable(item)) {
                col = 0;
            }
        } else {
            col = 6;
        }
        frame = IdSub.unitPtr(row + 0x40, IDC_SSCRN_CKPT_1);
        text = IdSub.unitPtr(row, IDC_SSCRN_CKPT_1);
        if (i == sw->cursor) {
            PSVECAdd(&frame->pParent->pos, &frame->pos0, &IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1)->pos0);
        }
        {
            IdUnit* mark = IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1);
            if (cursor) {
                mark->be_flag |= 8;
            } else {
                mark->be_flag &= ~8;
            }
        }
        id = le->id;
        {
            Vec pos;
            PSVECAdd(&text->pParent->pos, &text->pos0, &pos);
            x = (int) ((pos.x + 320.0f) * 0.8f);
            y = (int) ((240.0f - pos.y) * 0.8f);
        }
        slot = (u8) (row + 8);
        cMes.setLayout(slot, LAYOUT_SHOP_LIST);
        cMes.MesSet(id, x, y, 0x200A8, slot, col, 4);
        cMes.MesSetOt(slot, 0x13, 6);
        if (wk->merchant->levelNew(le->id)) {
            IdSub.unitPtr(row + 0x80, IDC_SSCRN_CKPT_1)->be_flag |= 8;
        }
        if (i == sw->cursor) {
            if (itemType(le->id) == 1) {
                if (item) {
                    weaponLevelDisp(item, le->id, 1, 1);
                } else {
                    weaponLevelDisp(0, le->id, 1, 1);
                }
            } else {
                weaponLevelDisp(item, le->id, 0, 1);
            }
        }
    }
}


// Draws the tune-up type panel of the cursor weapon: per type (lvType 0 firepower, 1 capacity,
// 2 firing speed, 3 exclusive) the current level bar, the next-level cost and the "MAX" marks;
// sw 1 shows the type cursor (lvType) and its price, 0 the overview.
void levelItemDisp(SUB_SCREEN* wk, int sw)
{
    // swk before m: `wk` then dies at the m load, which sched1 issues first (weight rule), and the
    // swk load's later slot shortens its live length below sw's (swk r30, sw r29 in global-alloc).
    ShopWork* swk = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* bar = 0;
    IdUnit* lvNum = 0;
    IdUnit* arrow = 0;
    int lv = 0;
    int i;
    int x;
    int y;

    for (i = 0; i < 5; i++) {
        IdSub.unitPtr(0x80 + i, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    }
    if (sw == 0) {
        int id = 0;

        for (i = 0; i < 4; i++) {
            switch (i) {
            case 0:
                id = 0x30;
                break;
            case 1:
                id = 0x50;
                break;
            case 2:
                id = 0x60;
                break;
            case 3:
                id = 0x70;
                break;
            }
            IdSub.unitPtr(id, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
        }
        return;
    }
    {
        ItemWork* item = m->levelupItemPtr(swk->cursor);
        int type;
        int val[2];
        char tag[2];
        int max;
        // COMPILER-DIFF: 13. `cur` is the digit block's `val[]` index: a single constant set whose
        // only register use is the digit loop's index shift, so it stays a REG_EQUIV pseudo that
        // loses the callee-saved race (it is live across the whole type loop) and reload
        // rematerialises it as `li r0,1` right before the `slwi` -- the extra reload is what puts the
        // doloop count reload on r9 (round-robin over the spill registers). `tag[cur]` folds to `lbz 1(r16)`.
        int cur = 1;

        for (type = 0; type < 4; type++) {
            int slot = type + 8;
            int j;

            {
                Vec pos;
                {
                    IdUnit* u = IdSub.unitPtr(type, IDC_SSCRN_CKPT_1);
                    Vec* scr = &u->pos0;
                    asm("" : "+r"(scr)); // COMPILER-DIFF: 3
                    PSVECAdd(&u->pParent->pos, scr, &pos);
                }
                x = (int) ((pos.x + 320.0f) * 0.8f);
                y = (int) ((240.0f - pos.y) * 0.8f);
            }
            cMes.setLayout(slot, LAYOUT_SHOP_LIST);
            cMes.MesSet(type + 6, x, y, 0x200A1, slot, 0, 3);
            cMes.MesSetOt(slot, 0x13, 6);
            switch (type) {
            case 0:
                bar = IdSub.unitPtr(0x30, IDC_SSCRN_CKPT_1);
                lvNum = IdSub.unitPtr(0x39, IDC_SSCRN_CKPT_1);
                arrow = IdSub.unitPtr(0x37, IDC_SSCRN_CKPT_1);
                break;
            case 1:
                bar = IdSub.unitPtr(0x50, IDC_SSCRN_CKPT_1);
                lvNum = IdSub.unitPtr(0x59, IDC_SSCRN_CKPT_1);
                arrow = IdSub.unitPtr(0x57, IDC_SSCRN_CKPT_1);
                break;
            case 2:
                bar = IdSub.unitPtr(0x60, IDC_SSCRN_CKPT_1);
                lvNum = IdSub.unitPtr(0x69, IDC_SSCRN_CKPT_1);
                arrow = IdSub.unitPtr(0x67, IDC_SSCRN_CKPT_1);
                break;
            case 3:
                bar = IdSub.unitPtr(0x70, IDC_SSCRN_CKPT_1);
                lvNum = IdSub.unitPtr(0x79, IDC_SSCRN_CKPT_1);
                arrow = IdSub.unitPtr(0x77, IDC_SSCRN_CKPT_1);
                break;
            }
            bar->be_flag |= 8;
            switch (type) {
            case 0:
                lv = (item->lv >> 12) + 2;
                break;
            case 1:
                lv = ((item->lv >> 8) & 0xF) + 2;
                break;
            case 2:
                lv = ((item->lv >> 4) & 0xF) + 2;
                break;
            case 3:
                lv = (item->lv8[1] & 0xF) + 2;
                break;
            }
            max = m->levelMax(item->id, type);
            if (lv > max || lv > WeaponId2MaxLevel(item->id, type)) {
                arrow->be_flag &= ~8;
            } else {
                arrow->be_flag |= 8;
                lvNum->texNo = lv;
                lvNum->tex_flag |= 2;
            }
            {
                u16 id = item->id;

                switch (type) {
                case 0:
                    val[0] = (int) (getPowerRatio(id, lv - 1) * 10.0f + 0.5f);
                    val[1] = (int) (getPowerRatio(id, lv) * 10.0f + 0.5f);
                    tag[0] = '1';
                    tag[1] = '4';
                    break;
                case 1:
                    val[0] = (int) (getSpeedRatio(id, lv - 1) * 100.0f + 0.5f);
                    val[1] = (int) (getSpeedRatio(id, lv) * 100.0f + 0.5f);
                    tag[0] = 'Q';
                    tag[1] = 'T';
                    break;
                case 2:
                    val[0] = (int) (getReloadRatio(id, lv - 1) * 100.0f + 0.5f);
                    val[1] = (int) (getReloadRatio(id, lv) * 100.0f + 0.5f);
                    tag[0] = 'a';
                    tag[1] = 'd';
                    break;
                case 3:
                    val[0] = (int) getBulletRatio(id, lv - 1);
                    val[1] = (int) getBulletRatio(id, lv);
                    tag[0] = 'q';
                    tag[1] = 't';
                    break;
                }
            }
            {
            int digit[3];
            for (j = 0; j < 3; j++) {
                digit[j] = val[cur] % 10;  // `cur * 4`: gcse's cprop cannot fold a constant into `ashift`
                val[cur] /= 10;            // (operand 1 must be a register); loop.c hoists it (`slwi r7,r0,2`)
            }
            i = 0;  // the leading-zero flag reuses the function's `i` (r31: it outranks `j` in global-alloc)
            for (j = 2; j >= 0; j--) {
                IdUnit* u = IdSub.unitPtr(tag[cur] + j, IDC_SSCRN_CKPT_1);

                u->tex_flag |= 2;
                u->texNo = digit[j];
                if (type == 3) {
                    if (i == 0 && digit[j] == 0) {
                        u->be_flag &= ~8;
                    } else {
                        i = 1;
                        u->be_flag |= 8;
                    }
                } else if (type == 0 && j == 2 && digit[j] == 0) {  // digit[j]: `lwz 8(rDigit)` through the array pseudo (weaponLevelDisp idiom)
                    u->be_flag &= ~8;
                } else {
                    u->be_flag |= 8;
                }
            }
            }
            if (lv <= max && lv <= WeaponId2MaxLevel(item->id, type)) {
                IdUnit* u = IdSub.unitPtr(type + 0x40, IDC_SSCRN_CKPT_1);
                Vec pos;
                PSVECAdd(&u->pParent->pos, &u->pos0, &pos);
                dispPrice(type + 0x80, 0, m->levelupPrice(item, type, lv), &pos, price_disp_price);
            } else {
                dispPrice(type + 0x80, 0, 0, 0, 0);
            }
        }
        if (m->specialTunable(item)) {
            int total = 0;
            Vec pos;

            {
                IdUnit* u = IdSub.unitPtr(0x44, IDC_SSCRN_CKPT_1);
                PSVECAdd(&u->pParent->pos, &u->pos0, &pos);
            }
            for (i = 0; i < 4; i++) {
                int mx = m->levelMax(item->id, i);
                if (mx > WeaponId2MaxLevel(item->id, i)) {
                    total += m->levelupPrice(item, i, mx);
                }
            }
            dispPrice(0x84, 0, total, &pos, price_disp_price);
            {
                IdUnit* u = IdSub.unitPtr(4, IDC_SSCRN_CKPT_1);
                Vec pos2;
                Vec* scr = &u->pos0;
                asm("" : "+r"(scr)); // COMPILER-DIFF: 3
                PSVECAdd(&u->pParent->pos, scr, &pos2);
                x = (int) ((pos2.x + 320.0f) * 0.8f);
                y = (int) ((240.0f - pos2.y) * 0.8f);
            }
            {
                // The slot number is a variable: `cMes.mes[no]` expands to `&cMes + no * 0xEC`
                // (+0x20 in the store displacements) and cse folds `no * 0xEC` to the constant
                // 0xB10 added to the `&cMes` register (`addi r30,r30,cMes@l; addi r30,r30,2832`);
                // a literal index is folded into the relocation at expand.
                int no = 0xC;
                cMes.setLayout(no, LAYOUT_SHOP_LIST);
                cMes.MesSet(0x28, x, y, 0x200A1, no, 0, 3);
                cMes.MesSetOt(no, 0x13, 6);
            }
        }
    }
}

// Tune-up list open: the merchant's levelup list, panel slide-in, the cursor weapon shown.
void LvUpMenuSelect::init(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;
    int i;

    sw->num = m->levelupItemNum();
    for (i = 0; i < 4; i++) {
        IdSub.unitPtr(0x21 + i, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
    }
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_0);
    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    u = IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1);
    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    levelItemDisp(wk, 0);
    dispItem(m->levelupItemNo(sw->cursor)->id, 1);
    state = 0;
}

// Tune-up list: B closes the coat and returns to the top menu; A picks the weapon (sw->item ->
// LvUpItemSelect), up/down move the cursor.
void LvUpMenuSelect::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;

    moveItem();
    itemCaption(m->levelupItemNo(sw->cursor)->id);
    dispLvUpItemList(wk, 5, state != 0);
    if (Key.trg & 0x40000000) {
        transit(1, wk);
        closeCoat(wk);
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    switch (state) {
    case 0:
        if (IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->anima_state & 1) {
            state = 1;
        }
        break;
    case 1:
        if (Key.trg & 0x80000000) {
            sw->item = m->levelupItemPtr(sw->cursor);
            if (sw->item && m->tunable(sw->item)) {
                if (m->specialTunable(sw->item)) {
                    sw->lvType = 4;
                } else {
                    sw->lvType = 0;
                }
                transit(0, wk);
                SndCall(0, 4, 0, 0, 0, 0);
                break;
            }
        }
        {
            int old = sw->cursor;

            if (Key.rep & 0x01000000) {
                sw->cursor = old - 1;
            }
            if (Key.rep & 0x02000000) {
                sw->cursor++;
            }
            {
                int c = sw->cursor;
                if (c >= 0) {
                    if (c > sw->num - 1) {
                        c = sw->num - 1;
                    }
                } else {
                    c = 0;
                }
                sw->cursor = c;
            }
            if (old != sw->cursor) {
                SndCall(0, 6, 0, 0, 0, 0);
            }
            if (sw->cursor > sw->top + 4) {
                sw->top = sw->cursor - 4;
            }
            if (sw->cursor < sw->top) {
                sw->top = sw->cursor;
            }
            if (old != sw->cursor) {
                dispItem(m->levelupItemNo(sw->cursor)->id, 1);
            }
        }
        break;
    }
}

void LvUpMenuSelect::quit(SUB_SCREEN* wk) {}

// Message number of the "all tune-ups" caption per weapon id.
int specialCaption(int id)
{
    switch (id) {
    case 0x23:
        return 0x29;
    case 0x25:
        return 0x2A;
    case 0x03:
        return 0x2B;
    case 0x21:
        return 0x2C;
    case 0x27:
        return 0x2D;
    case 0x29:
        return 0x2E;
    case 0x2C:
        return 0x2F;
    case 0x2D:
        return 0x30;
    case 0x94:
        return 0x31;
    case 0x2E:
        return 0x32;
    case 0x2F:
        return 0x33;
    case 0x30:
        return 0x34;
    case 0x36:
        return 0x35;
    case 0x34:
        return 0x36;
    case 0x2A:
        return 0;
    case 0x37:
        return 0x37;
    }
    return 0;
}

// Nothing to set up (lvType keeps its value).
void LvUpItemSelect::init(SUB_SCREEN* wk) {}

// Tune type pick for the chosen weapon: shows its level table and the type's description message
// (0xF + lvType; the Mine Thrower's special text); B (or nothing tunable) back to the list, A on a
// tunable type computes the price (type 4 = every type at once) and opens LvUpConfirm; up/down
// move lvType over the available rows.
void LvUpItemSelect::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    ItemWork* item = m->levelupItemPtr(sw->cursor);
    IdUnit* u;
    int x;
    int y;
    int i;

    weaponLevelDisp(item, item->id, 1, 1);
    levelItemDisp(wk, 1);
    u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
    x = (int) ((u->pos0.x + 320.0f) * 0.8f);
    y = (int) ((240.0f - u->pos0.y) * 0.8f);
    cMes.setLayout(1, LAYOUT_SUBSCRN);
    if (sw->lvType == 4) {
        cMes.MesSet(specialCaption(item->id), x, y, 0x20081, 1, 0, 3);
    } else {
        int no = sw->lvType + 0xF;
        if (item->id == 0x36 && sw->lvType == 0) {
            no = 0x38;
        }
        cMes.MesSet(no, x, y, 0x20081, 1, 0, 3);
    }
    if ((Key.trg & 0x40000000) || m->tunable(item) == 0) {
        transit(1, wk);
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    if (Key.trg & 0x80000000) {
        int lv = 0;
        int ok = 0;

        if (m->specialTunable(sw->item)) {
            if (sw->lvType == 4) {
                sw->price = ok;
                for (lv = 0; lv < 4; lv++) {
                    int mx = m->levelMax(item->id, lv);
                    if (mx > WeaponId2MaxLevel(item->id, lv)) {
                        sw->price += m->levelupPrice(sw->item, lv, mx);
                    }
                }
                ok = 1;
            }
        } else {
            int mx;

            switch (sw->lvType) {
            case 0:
                lv = (item->lv >> 12) + 2;
                break;
            case 1:
                lv = ((item->lv >> 8) & 0xF) + 2;
                break;
            case 2:
                lv = ((item->lv >> 4) & 0xF) + 2;
                break;
            case 3:
                lv = (item->lv8[1] & 0xF) + 2;
                break;
            }
            mx = m->levelMax(item->id, sw->lvType);
            sw->price = m->levelupPrice(sw->item, sw->lvType, lv);
            if (lv <= WeaponId2MaxLevel(item->id, sw->lvType) && lv <= mx) {
                ok = 1;
            }
        }
        if (ok) {
            transit(0, wk);
        }
        return;
    }
    {
        int old = sw->lvType;

        if (Key.rep & 0x01000000) {
            sw->lvType = old - 1;
        }
        if (Key.rep & 0x02000000) {
            sw->lvType++;
        }
        if (m->specialTunable(sw->item) == 1) {
            sw->lvType = sw->lvType < 0 ? 0 : (sw->lvType > 4 ? 4 : sw->lvType);
        } else {
            sw->lvType = sw->lvType < 0 ? 0 : (sw->lvType > 3 ? 3 : sw->lvType);
        }
        if (old != sw->lvType) {
            SndCall(0, 6, 0, 0, 0, 0);
        }
    }
    // The frame loop counts with `x` (the message x of the MesSet above): the shared pseudo has the
    // refs that put it above `item` in global-alloc (x r30 / item r28, then `i` r31 below).
    for (x = 0; x < 5; x++) {
        IdUnit* frame = IdSub.unitPtr(0x40 + x, IDC_SSCRN_CKPT_1);
        if (x == sw->lvType) {
            IdSub.unitPtr(0x3F, IDC_SSCRN_CKPT_1)->pos0 = frame->pos0;
        }
    }
    for (i = 0; i < 4; i++) {
        IdSub.unitPtr(0x21 + i, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
    }
    if (sw->lvType != 4) {
        IdSub.unitPtr((u8) sw->lvType + 0x21, IDC_SSCRN_CKPT_0)->be_flag |= 8;
    }
}

// Tune confirm open: current levels (cur[]) and the merchant's maxima (max[]); message 0x17 (buy?)
// with the price, or the "cannot afford" / "already max" lines.
void LvUpConfirm::init(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;
    Merchant* m = wk->merchant;
    IdUnit* u;

    cur[0] = (sw->item->lv >> 12) + 1;
    cur[1] = ((sw->item->lv >> 8) & 0xF) + 1;
    cur[2] = ((sw->item->lv >> 4) & 0xF) + 1;
    cur[3] = (sw->item->lv8[1] & 0xF) + 1;
    max[0] = m->levelMax(sw->item->id, 0);
    max[1] = m->levelMax(sw->item->id, 1);
    max[2] = m->levelMax(sw->item->id, 2);
    max[3] = m->levelMax(sw->item->id, 3);
    sw->lv[0] = cur[0];
    sw->lv[1] = cur[1];
    sw->lv[2] = cur[2];
    sw->lv[3] = cur[3];
    if (m->specialTunable(sw->item) == 1) {
        int i;
        for (i = 0; i < 4; i++) {
            sw->lv[i] = max[i];
        }
    } else {
        sw->lv[sw->lvType] = cur[sw->lvType] + 1;
    }
    if (sw->price <= (int) pG->peseta) {
        msg = 0x17;
        SndCall(0, 9, 0, 0, 0, 0);
    } else {
        msg = 0x18;
        SndCall(0, 0x19, 0, 0, 0, 0);
    }
    u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
    {
        int x = (int) ((u->pos0.x + 320.0f) * 0.8f) + lvup_msg_x;
        cMes.MesSet(shop_msg[msg].msg, x, (int) ((240.0f - u->pos0.y) * 0.8f) + lvup_msg_y, 0x20801, 1, 0, 3);
    }
    IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
    shopStrPlay(wk, shop_msg[msg].str);
}

// ItemWork::x6 as its four tune-level nibbles.
struct TuneLevel {
    u16 fire : 4;
    u16 mag : 4;
    u16 speed : 4;
    u16 ex : 4;
};


// Tune confirm: yes applies the levels (Merchant::levelup / ItemMgr, pesetas paid, sw->lv[]
// stored) with the thanks line, no / B back to the type pick.
void LvUpConfirm::move(SUB_SCREEN* wk)
{
    ShopWork* sw = wk->shop;

    levelItemDisp(wk, 1);
    if (Key.trg & 0x40000000) {
        cMes.Delete(1);
        transit(0, wk);
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    if (msg == 0x17 && (result = cMes.GetSelectMessage(1)) != 0) {
        switch (result) {
        case 1: {
            TuneLevel* t;

            t = (TuneLevel*) &sw->item->lv;
            // COMPILER-DIFF: 12 (combine). The target keeps `extsb` before `addi -1; clrlwi 24; slwi 12`; our
            // combine strips the sign extension under the u8 truncation when both are one expression.
            {
                int v = (s8) sw->lv[0];
                t->fire = (u8) (v - 1);
            }
            // Byte first, item pointer second in every nibble: the `lbz` between the previous `sth`
            // and the next `lwz item` keeps local-alloc's fake lifetimes of the four item-pointer
            // qtys apart (all r11; adjacent `sth; lwz` alternates r11/r10), and the LUID puts the
            // `lbz` first where sched2 ties (nibble 4).
            {
                int v = (s8) sw->lv[1];
                t = (TuneLevel*) &sw->item->lv;
                t->mag = v - 1;
            }
            {
                int v = (s8) sw->lv[2];
                t = (TuneLevel*) &sw->item->lv;
                t->speed = v - 1;
            }
            {
                int v = (s8) sw->lv[3];
                t = (TuneLevel*) &sw->item->lv;
                t->ex = v - 1;
            }
            if (sw->lvType == 3 || sw->lvType == 4) {
                ItemWork* item = sw->item;
                item->bullet = (item->bullet & 0xE000) | (WeaponId2ChargeNum(item->id, (item->lv8[1] & 0xF) + 1) & 0x1FFF);
            }
            if (ItemMgr.weapon() == sw->item) {
                ItemMgr.arm(ItemMgr.weapon());
            }
            pG->peseta -= sw->price;
            shopStrPlay(wk, shop_msg[25].str);
            SndCall(0, 0x18, 0, 0, 0, 0);
            break;
        }
        case 2:
            SndCall(0, 5, 0, 0, 0, 0);
            break;
        }
        transit(0, wk);
    } else if (msg == 0x18) {
        if (Key.trg & 0x80000000) {
            cMes.Delete(1);
            transit(0, wk);
            SndCall(0, 5, 0, 0, 0, 0);
        }
    }
}

// Hides the confirm frame.
void LvUpConfirm::quit(SUB_SCREEN* wk)
{
    IdSub.unitPtr(0xFA, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
}

// Prints item `id`'s name at the caption unit (IdSub 0xFC/0x1C) in message slot 1.
void itemCaption(int id)
{
    IdUnit* u = IdSub.unitPtr(0xFC, IDC_SSCRN_CKPT_0);
    int x = (int) ((u->pos0.x + 320.0f) * 0.8f);
    int y = (int) ((240.0f - u->pos0.y) * 0.8f);

    cMes.MesSet(id, x, y, 0x200A4, 1, 0, 4);
}

// Tune level bars / values of weapon `id` (item = the owned slot, 0 for the shop's copy at `level`).
void weaponLevelDisp(ItemWork* item, u16 id, int sw, int level)
{
    IdUnit* u = IdSub.unitPtr(5, IDC_SSCRN_CKPT_0);
    int lv = 0;
    int type;

    if (sw == 0) {
        u->be_flag &= ~8;
        return;
    }
    u->be_flag |= 8;
    for (type = 0; type < 4; type++) {
        int digit[3];
        int barBase = 0;
        int numBase = 0;
        int val = 0;
        int on;

        if (item) {
            switch (type) {
            case 0:
                lv = (item->lv >> 12) + 1;
                break;
            case 1:
                lv = ((item->lv >> 8) & 0xF) + 1;
                break;
            case 2:
                lv = ((item->lv >> 4) & 0xF) + 1;
                break;
            case 3:
                lv = (item->lv8[1] & 0xF) + 1;
                break;
            }
        } else {
            lv = level;
            switch (id) {
            case 0x40:
                switch (type) {
                case 0:
                    lv = 1;
                    if (ScfFlagChk(pG, SCF_ST1_SUB_PERFECT)) {
                        lv = 2;
                    }
                    break;
                case 1:
                    lv = 1;
                    break;
                case 2:
                    lv = 1;
                    break;
                case 3:
                    lv = 1;
                    break;
                }
                break;
            case 0x34:
                switch (type) {
                case 0:
                    lv = 7;
                    break;
                case 1:
                    lv = 1;
                    break;
                case 2:
                    lv = 3;
                    break;
                case 3:
                    lv = 6;
                    break;
                }
                break;
            }
        }
        switch (type) {
        case 0:
            val = (int) (getPowerRatio(id, lv) * 10.0f + 0.5f);
            numBase = 0x61;
            barBase = 0x65;
            break;
        case 1:
            val = (int) (getSpeedRatio(id, lv) * 100.0f + 0.5f);
            numBase = 0x71;
            barBase = 0x75;
            break;
        case 2:
            val = (int) (getReloadRatio(id, lv) * 100.0f + 0.5f);
            numBase = 0x81;
            barBase = 0x85;
            break;
        case 3:
            val = (int) getBulletRatio(id, lv);
            numBase = 0x91;
            barBase = 0x95;
            break;
        }
        for (int i = 0; i < 6; i++) {
            IdUnit* b = IdSub.unitPtr(barBase + i, IDC_SSCRN_CKPT_0);
            IdUnit* colOff;
            IdUnit* colOn;
            IdUnit* src;

            if (i < WeaponId2MaxLevel(id, type)) {
                b->be_flag |= 8;
            } else {
                b->be_flag &= ~8;
            }
            colOff = IdSub.unitPtr(1, IDC_SSCRN_CKPT_0);
            colOn = IdSub.unitPtr(2, IDC_SSCRN_CKPT_0);
            src = IdSub.unitPtr(3, IDC_SSCRN_CKPT_0);
            if (i < lv) {
                // if/else (jump1 hoists the else-set between the compare and the branch)
                if (lv > WeaponId2MaxLevel(id, type)) {
                    src = colOff;
                } else {
                    src = colOn;
                }
            }
            b->col0[0] = src->col0[0];
            b->col0[1] = src->col0[1];
            b->col0[2] = src->col0[2];
            b->col0[3] = src->col0[3];
        }
        for (int i = 0; i < 3; i++) {
            digit[i] = val % 10;
            val /= 10;
        }
        on = 0;
        for (int i = 2; i >= 0; i--) {
            IdUnit* d = IdSub.unitPtr(numBase + i, IDC_SSCRN_CKPT_0);

            d->tex_flag |= 2;
            d->texNo = digit[i];
            if (type == 3) {
                if (on == 0 && digit[i] == 0) {
                    d->be_flag &= ~8;
                } else {
                    on = 1;
                    d->be_flag |= 8;
                }
            } else if (type == 0 && i == 2 && digit[i] == 0) {
                d->be_flag &= ~8;
            } else {
                d->be_flag |= 8;
            }
        }
    }
}

// Shows the stock count digits (IdSub 6/0x1C) or the sold-out unit (0xF2) for the cursor item;
// sw 0 hides both.
void stockNumDisp(int num, int sw)
{
    IdUnit* u = IdSub.unitPtr(6, IDC_SSCRN_CKPT_0);
    IdUnit* sold;

    if (sw == 0) {
        u->be_flag &= ~8;
        IdSub.unitPtr(0xF2, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
        return;
    }
    sold = IdSub.unitPtr(0xF2, IDC_SSCRN_CKPT_0);
    if (num) {
        int digit[3];
        int n = num;
        int on;

        u->be_flag |= 8;
        sold->be_flag &= ~8;
        for (int i = 0; i < 3; i++) {
            digit[i] = n % 10;
            n /= 10;
            IdSub.unitPtr(0xA1 + i, IDC_SSCRN_CKPT_0)->be_flag &= ~8;
        }
        on = 0;
        for (int i = 2; i >= 0; i--) {
            IdUnit* d;

            if (on == 0) {
                if (digit[i] == 0 && i != 0) {
                    continue;
                }
                on = 1;
            }
            d = IdSub.unitPtr(0xA1 + i, IDC_SSCRN_CKPT_0);
            d->be_flag |= 8;
            d->tex_flag |= 2;
            d->texNo = digit[i];
        }
    } else {
        u->be_flag &= ~8;
        sold->be_flag |= 8;
    }
}

// Price row `type` (id table 0x80 + row): the count (units 0x10..0x14), the price (0xFE, 1..7) and
// the sold-out mark (0x20).
void dispPrice(int type, int num, int price, Vec* pos, u32 flags)
{
    int n;

    if (flags == 0) {
        IdSub.unitPtr(0, type)->be_flag &= ~8;
        return;
    }
    IdSub.unitPtr(0, type)->be_flag |= 8;
    if (pos) {
        IdSub.unitPtr(0, type)->pos0 = *pos;
    }
    if (price_disp_num & flags) {
        int digit[4];
        int on;

        IdSub.unitPtr(0x10, type)->be_flag |= 8;
        n = num;
        for (int i = 0; i < 4; i++) {
            digit[i] = n % 10;
            n /= 10;
        }
        for (int i = 0; i < 4; i++) {
            IdSub.unitPtr(0x11 + i, type)->be_flag &= ~8;
        }
        on = 0;
        for (int i = 3; i >= 0; i--) {
            IdUnit* d;

            if (on == 0) {
                if (digit[i] == 0 && i != 0) {
                    continue;
                }
                on = 1;
            }
            d = IdSub.unitPtr(0x11 + i, type);
            d->be_flag |= 8;
            d->tex_flag |= 2;
            d->texNo = digit[i];
        }
    } else {
        IdSub.unitPtr(0x10, type)->be_flag &= ~8;
    }
    if (price_disp_price & flags) {
        int digit[7];
        int on;

        IdSub.unitPtr(0xFE, type)->be_flag |= 8;
        n = price;
        for (int i = 0; i < 7; i++) {
            digit[i] = n % 10;
            n /= 10;
        }
        for (int i = 0; i < 7; i++) {
            IdSub.unitPtr(1 + i, type)->be_flag &= ~8;
        }
        on = 0;
        for (int i = 6; i >= 0; i--) {
            IdUnit* d;

            if (on == 0) {
                if (digit[i] == 0 && i != 0) {
                    continue;
                }
                on = 1;
            }
            d = IdSub.unitPtr(1 + i, type);
            d->be_flag |= 8;
            d->tex_flag |= 2;
            d->texNo = digit[i];
        }
    } else {
        IdSub.unitPtr(0xFE, type)->be_flag &= ~8;
    }
    if (price_disp_sold & flags) {
        IdSub.unitPtr(0x20, type)->be_flag |= 8;
    } else {
        IdSub.unitPtr(0x20, type)->be_flag &= ~8;
    }
}

// Texture frame of item `id` in the shop's item texture (0 = none: a model is shown instead).
static int itemTexNo(int id)
{
    u8 tbl[77] = {
        0xFF, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65,
        0x66, 0x67, 0x68, 0x70, 0x77, 0x89, 0x8A, 0x8F, 0x90, 0x91, 0x93, 0x96, 0x9A, 0x9B, 0x9C, 0x9D,
        0x9E, 0x9F, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC6, 0xC7, 0xC8,
        0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
        0xD9, 0xDA, 0xDB, 0xA1, 0x56, 0xA2, 0xFE, 0xA9, 0x7F, 0x7D, 0x7E, 0xFF, 0x98,
    };
    u16 key = id;
    int hi = 0x55;
    int lo = 0x54;
    int i;

    if (id <= hi && id >= lo) {
        key = 0xA9;
    }
    for (i = 0; i < sizeof(tbl); i++) {
        if (key == tbl[i]) {
            return i;
        }
    }
    return 0;
}

// Rotation / scale of the shown item model from the shop_item_place table (degrees), identity when
// the item has no entry.
void setOrientation(int id, cModel* m)
{
    int i;

    m->ang.x = m->ang.y = m->ang.z = 0.0f;
    m->scale.x = m->scale.y = m->scale.z = 1.0f;
    for (i = 0; i < 51; i++) {
        if (id == shop_item_place[i].id) {
            PSVECScale(&shop_item_place[i].rot, &m->ang, 0.017453292f);
            m->scale = shop_item_place[i].scale;
        }
    }
    m->matUpdate();
}

// Shows the cursor item in the display box (IdSub 0xF3/0x1C): a texture icon when itemTexNo knows
// it, else its 3D piece model on MapMgr work 1 (lit, oriented, placed under the unit); sw 0 hides.
void dispItem(int id, int sw)
{
    IdUnit* u = IdSub.unitPtr(0xF3, IDC_SSCRN_CKPT_0);
    cMap* m = MapMgr.getWork(1);

    if (sw == 0) {
        u->be_flag &= ~8;
        m->be_flag &= ~2;
        return;
    }
    if (itemTexNo(id)) {
        u->be_flag |= 8;
        u->tex_flag |= 2;
        u->texNo = itemTexNo(id);
        m->be_flag &= ~2;
        return;
    }
    u->be_flag &= ~8;
    {
        u8* data = searchItemModelData(id, piece_info);
        if (data) {
            static const Vec light_p0 = {0.0f, 0.0f, 0.0f};
            static const Vec light_p1 = {1000.0f, 1000.0f, 0.0f};

            m->modelInit(((void**) data)[0], ((void**) data)[1]);
            m->LightInfo.init2(0, 0, &light_p0, &light_p1, 0x10);
            m->CullMode = 2;
            m->ot_type = 6;
            setOrientation(id, m);
            moveItem();
            m->be_flag |= 2;
        } else {
            m->be_flag &= ~2;
        }
    }
    itemCamera = pG->Camera;
}

// Screen (+-240 half height) -> world x/y at the camera distance, z 0.
void screenPos2worldPos(Vec* scr, Vec* out)
{
    CAMERA* cam = &pG->Camera;
    f32 z = cam->param.pos.z;
    f32 h = fabsf((f32) (z * tan(cam->param.fovy * 0.5f * 3.1415927f / 180.0f)));

    out->x = scr->x * h / 240.0f;
    out->y = scr->y * h / 240.0f;
    out->z = 0.0f;
}

// Per frame: keeps the shown item model under the display box unit's screen position.
void moveItem()
{
    IdUnit* u = IdSub.unitPtr(0xF3, IDC_SSCRN_CKPT_0);
    cMap* m = MapMgr.getWork(1);
    Vec scr;
    Vec pos;

    scr.x = u->mat[0][3];
    scr.y = u->mat[1][3];
    scr.z = u->mat[2][3];
    screenPos2worldPos(&scr, &pos);
    m->pos.x = pos.x;
    m->pos.y = pos.y;
    m->pos.z = pos.z;
    m->matUpdate();
}
