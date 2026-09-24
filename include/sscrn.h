#ifndef SSCRN_H
#define SSCRN_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "main_sub.h"

// Sub screen (inventory / map / files / puzzle) front end, game/sscrn.cpp. The screen itself is
// the Sscrn.rel DLL, linked into the ARAM-swapped area while it is open.
class cObjWep;
class cMap;
struct ItemWork;
struct SUB_SCREEN;
struct SsFileWork;
struct ItemScreenWork;

// Sub screen data archive (ss_cmmn.dat / ss_pzzl.dat): a table of byte offsets to its sub-files.
struct SsArc {
    u32 ofs[0x12];
};
#define SS_ARC_PTR(arc, no) ((void*) ((arc)->ofs[no] + (u32) (arc)))

// The work is the `SUB_SCREEN` of the Sscrn module's `Widget<SUB_SCREEN>` template (the module's
// mangled names carry the tag); SubScreenWork is the DOL-side alias.
struct SUB_SCREEN {
    char filename[0x28];          // 0x000  "SS/<lang>/<file>" (sscrnSetLanguage / sscrnDataFilename)
    u8 Loop;                  // 0x028  1 while the sub screen main loop runs (sscrn opens, SubScreenTask exit clears)
    u8 pad_29[3];
    s32 open_flag;             // 0x02C  open type: 1 inventory, 2, 0x10, 0x20 puzzle, 0x40, 0x80
    s32 flags;                // 0x030  bit0 event, bit1 (flags_5010 bit21 at open), bit3 no sound
    s32 close_flag;           // 0x034  set by the screens as they close (2 item, 4 map, 8 term, 0x10 file, 0x10000 shop); cleared on menu change
    s32 attr_flag;
    s32 wait;                 // 0x03C  frames left before SubScreenCall may open (SubScreenWait)
    s32 model_flag;
    s32 wait_cnt;
    s32 str_id;
    int (*scrn_out_func)(SUB_SCREEN*);  // 0x04C  screen exit routine (Sscrn ss_*: sscrn_*_out), run until it returns 1
    u32 stop_bak;              // 0x050  pG->flags_170 while open
    u32 disp_bak;               // 0x054  pG->flags_58 while open
    CAMERA camera_bak;               // 0x058  pG->Camera while open
    Mtx pl_mat;                // 0x150  player matrix at open
    Mtx sub_mat;               // 0x180  partner matrix at open
    u8 stage_no;                 // 0x1B0  sscrnStageNo()
    u8 pad_1B1;
    u16 room_no;                 // 0x1B2  sscrnRoomNo()
    u8 scope_flag;                 // 0x1B4  1: the scope was up, 2: and flags_5010 bit 26
    u8 binocular_flag;                  // 0x1B5  the binocular was up
    u8 suspend_flag;                  // 0x1B6  flags_5010 bit 28 (always 0: the mask is stored as a byte)
    u8 swep_flag;              // 0x1B7  the equipped weapon (type 3) was empty
    u8 jacket_flag;                  // 0x1B8  item 0xFE owned
    u8 pad_1B9[3];
    s32 sub_cure_flag;              // 0x1BC  SubCharCheckHealing()
    void* pBuf;               // 0x1C0  MRAM area swapped with the ARAM copy (pG->pStageFont)
    u32 pFreeOffs;             // 0x1C4  bytes read to ARAM (SubScreenAramRead)
    u32 pHeapOffs;              // 0x1C8  heap 12 starts at pBuf + heapOfs
    u32 pPreplfOffs;               // 0x1CC  Sscrn.rel offset in the area
    u32 pCommonOffs;              // 0x1D0  ss_cmmn.dat offset
    u32 pSwitchOffs;              // 0x1D4  ss_pzzl.dat offset
    s32 relAddr;              // 0x1D8  Sscrn.rel address (0 while unlinked)
    SsArc* pCmmn;             // 0x1DC
    SsArc* pSwitchDat;        // 0x1E0  read buffer of the screen being switched to (item / map / puzzle .dat) (PS2 pSwitchDat)
    SsArc* pPzzlDat;          // 0x1E4  puzzle screen data (SubScreenTask: = pSwitchDat once read) (PS2 pPzzlDat)
    SsArc* pItemDat;             // 0x1E8  ss_item.dat archive (Sscrn ss_item)
    SsArc* pTermDat;             // 0x1EC  ss_term.dat archive (Sscrn ss_term)
    void* pTermMes;            // 0x1F0  op/opNN.das (Sscrn ss_term: the message/sequence archive at +0x400)
    SsArc* pMapDat;           // 0x1F4  ss_map.dat archive (Sscrn ss_map: common map data, pSwitchDat while the map is open)
    SsArc* pMapObj;          // 0x1F8  SS/cmn/map_objNN.dat archive of the current area (Sscrn ss_map)
    SsArc* pFileDat;             // 0x1FC  ss_file.dat archive (Sscrn ss_file)
    SsArc* pExam;             // 0x200  item examine id data archive (examine ItemExamine::idSet)
    SsArc* pShopDat;             // 0x204  ss_shop.dat archive (Sscrn ss_shop: read to pBuf + pFreeOffs)
    void* pTelDat;           // 0x208  SS/cmn/ss_ocNNN.dat (Sscrn ss_term: the partner model data)
    void* pTplDat;            // 0x20C  0x20000-byte file picture TPL buffer (Sscrn ss_file)
    void* pWepDat;               // 0x210  weapon model data (pBuf + 0x2E5E00, Sscrn SubScreenTask / weaponChangeTask)
    void* binoA;              // 0x214  CameraControl::GetBinocularIDAddr
    void* binoB;              // 0x218
    class cLight* p_light[8];    // 0x21C  screen lights (Sscrn sscrnLightCreate / sscrnLightClear)
    void* pExamDat;               // 0x23C  0x3E800-byte buffer
    void* pItemBin;               // 0x240  item examine model data (Sscrn SsItemExamine: x23C)
    void* pItemTpl;               // 0x244  item examine texture data
    ItemWork* p_exam_item;           // 0x248  selected item slot (Sscrn CapSelect)
    cMap* p_exam_model;               // 0x24C  MapMgr work 2 (Sscrn CapSelect)
    u8 wep_rno;                  // 0x250  Sscrn weapon change task state (3 = done)
    s8 wep_idx;                  // 0x251  weapon change request slot
    s16 wep_cnt;                 // 0x252  weapon change fade counter (Sscrn weaponChangeTask)
    struct {
        s32 req;              // 0x254  request pending
        u16 no;               // 0x258  weapon number
        u16 type;             // 0x25A  weapon type
    } wepChange[2];           // 0x254  Sscrn weaponChangeRequest
    u8 menu_no;                  // 0x264  2 for type 2, else 1
    u8 menu_next;                  // 0x265
    u8 menu_old;                  // 0x266  2: the player model is shown (Sscrn ss_file)
    u8 cursor_mode;                  // 0x267  Sscrn ss_item: 0 select, 1 command, 2 combine (cleared every frame)
    u8 cursor_flag;                  // 0x268  Sscrn ss_item: the cursor moved this frame
    u8 alpha_flag;                  // 0x269
    u16 alpha_cnt;                 // 0x26A
    s8 cmd_menu_no;                  // 0x26C  Sscrn ss_item: command cursor
    u8 pad_26D[3];
    Mtx pl_mat_map;             // 0x270  player matrix on the map (Sscrn ss_map mapPositionCheck)
    u8 map_help;                  // 0x2A0
    u8 map_obj_num;              // 0x2A1  Sscrn ss_map: model count of the area's rooms (door models start there)
    s8 floor_no;              // 0x2A2  Sscrn ss_map: player floor (y / 100 rounded)
    u8 pad_2A3[0x2AD - 0x2A3];
    u8 Key_disable;                  // 0x2AD  Sscrn ss_map mapCameraInit clears it
    u8 board_size;                  // 0x2AE  item 0x7C..0x7F owned -> 0..3
    u8 board_next;                  // 0x2AF
    class pzlPlayer* puzzlePlayer;    // 0x2B0  puzzle (case) player of the Sscrn puzzle screen
    u8 back2;                  // 0x2B4  Sscrn ss_shop: the bought piece is in hand (case placement)
    u8 pad_2B5[0x2FA - 0x2B5];
    u16 get_item_id;                 // 0x2FA  item id handed to the opened sub screen (sce_at sceAtGetItem)
    u16 get_item_num;                 // 0x2FC  its count
    u8 pad_2FE[2];
    ItemWork* p_get_item;           // 0x300  Sscrn ss_pzzl: the extra piece's slot (get() result)
    ItemScreenWork* item;  // 0x304  Sscrn ss_item cursor state (9 bytes)
    struct SsMapWork* map; // 0x308  Sscrn ss_map work (mark models, camera, viewport; 0x104C bytes)
    SsFileWork* file;      // 0x30C  Sscrn ss_file cursor/page state
    s8* pCapCursor;           // 0x310  Sscrn ss_cap cursor {row, column, row * 6 + column} (MEM_ALLOC(3))
    struct ShopWork* shop; // 0x314  Sscrn ss_shop list/cursor state (0x48 bytes)
    class Merchant* merchant;// 0x318  Sscrn ss_shop: the shop session (game/merchant.cpp Merchant)
    s32 opeMdtNo;                // 0x31C  OpeSetOpenTerm number
    s32 sndId;               // 0x320  SndStrPlayBlock handle
    cObjWep* pObjWep;            // 0x324  OpeSetOpenTerm weapon object
    Vec posBak;              // 0x328  player position before OpeSetOpenTerm
    Vec angBak;              // 0x334
    s32 cancel;               // 0x340  OpeSetOpenTermCancel
    OSModuleHeader* p_module;  // 0x344  linked sub screen DLL
    union {
        u32 save;             // 0x348  SscrnDataSave/Load word
        struct {
            u8 map_mode;      // 0x348  one bit per map display mode (ss_map mapModeCheck/Change; SubScreenGameInit clears it)
            u8 x349;
            u8 map_mark;
            u8 x34B;
        };
    };
    u32 debug_menu;           // 0x34C  Sscrn debug menu: bit0 open, bit4 debug disp, bit5 memory disp, bit6 reveil
    s32 debugMode;            // 0x350  pG->debug_mode while open
    s32 debug_flg_bak;        // 0x354  pG->Debug_flg[2] bit 30 while open
    u8 pad_358[0x366 - 0x358];
    u8 pzzl_debug_open;       // 0x366  Sscrn ss_pzzl: debug menu open (toggled like item_make_open)
    u8 item_make_open;        // 0x367  Sscrn ss_item: debug item-make menu open
    s8 item_make_cursor;      // 0x368  item-make menu cursor (0/1 = the two id slots, 2 = remove)
    u8 pad_369[3];
    int item_make_id[2];      // 0x36C  item-make menu item ids
};
typedef SUB_SCREEN SubScreenWork;

// Sscrn ss_item cursor state (SUB_SCREEN::item, MEM_ALLOC(9)): two item columns.
struct ItemScreenWork {
    s8 x0;
    s8 col;      // 0x1  current column (-1 = main menu)
    s8 idx[2];   // 0x2  cursor index per column
    s8 sel[2];   // 0x4  selected index per column
    s8 comb[2];  // 0x6  combine partner index per column (-1 = none)
    s8 x8;
};

extern SubScreenWork SubScreenWk;

extern "C" {
int SscrnDataSize();
void SscrnDataSave(u32* dst);
void SscrnDataLoad(u32* pData);
void SubScreenAramRead();
void sscrnSetLanguage(SubScreenWork* pSscrn, int language);
void sscrnDataFilename(SubScreenWork* pSscrn, const char* name);
void SubScreenGameInit();
void SubScreenRoomInit();
void SubScreenWait(int frame);
void SubScreenCall();
int sscrnStageNo();
u16 sscrnRoomNo(u16 room_no);
enum SS_OPEN_FLAG {
    SS_OPEN_NULL = 0,
    SS_OPEN_NORMAL = 1,
    SS_OPEN_MAP = 2,
    SS_OPEN_PZZL = 4,
    SS_OPEN_SHOP = 16,
    SS_OPEN_TERM = 32,
    SS_OPEN_FILE = 64,
    SS_OPEN_ITEM = 128,
    SS_OPEN_CAP = 256
};

enum SS_ATTR_FLAG {
    SS_ATTR_NULL = 0,
    SS_ATTR_EVENT = 1,
    SS_ATTR_BOAT = 2,
    SS_ATTR_ASHLEY = 4
};

int SubScreenOpen(int type, int flags);
void SubScreenMiss();
void SubScreenExec();
void SubScreenExitCore(SubScreenWork* pSscrn);
void SubScreenExit();
int OpeGetMdtNo();
void OpeSetMdtNo(u32 mdtNo);
int OpeMdtSetInit();
void OpeOwTypeSet(u8 owType);
void OpeSetOpenTerm(int no, f32 x, f32 y, f32 z, f32 ang);
void OpeSetOpenTermCancel();
void OpeSetOpenTermEnd();
}


#endif
