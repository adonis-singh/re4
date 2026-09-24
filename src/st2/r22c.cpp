#include "types.h"
#include "pl_debug.h"
class cObjWep;
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "read.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "em3e.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "item.h"
#include "mes.h"
#include "rnd.h"
#include "snd.h"
#include "quake.h"
#include "esp.h"
#include "est.h"
#include "joy.h"
#include "pad.h"
#include "sscrn.h"
#include "eprintf.h"
#include "db_log.h"
#include "id_sys.h"
#include "cockpit.h"
#include "cam_ctrl.h"
#include "dvd.h"
#include "option.h"
#include "dbmodule.h"
#include "math_sub.h"
#include "room_data.h"
#include "fade.h"

// Room 2-2C (D:/Bio4/Prog/r22c.cpp): the shooting range. The target tables below are the
// shooting game's level scripts: every record is `{time, flag, x, y, z, spd, moves...}` where a
// move is `{2, 0xF0}` (straight) or `{3, x, y, z, 0x32}` (way points) and `1` ends the record,
// `{time, 0xFE}` is a pause; a level is `{count, time, records...}`.

// The result screen file (SS/<lang>/id22c.dat): offsets of its id-system data blocks.
struct R22cResultData {
    u8 pad_0[0x10];
    u32 ofsTexResult;   // 0x10  IdTexDataLoad(.., 7)
    u32 ofsIdResult;    // 0x14  IdSys.set(.., 0xFF, IDC_TITLE, ..)
    u32 ofsIdHigh;      // 0x18  the high-score variant of the result table
    u32 ofsIdReload;    // 0x1C  IdSys.set(.., 0xFF, IDC_EVENT, ..)
    u32 ofsTexReload;   // 0x20  IdTexDataLoad(.., 6)
};

// One target record of the level scripts (EmMarkData for cEmMark::init).
struct R22cMarkRec {
    int time;   // 0x0  frame the target appears
    int flag;   // 0x4  target type (0xFD: UFO wait, 0xFE: pause)
};

#define RES_PTR(d, ofs) ((void*) ((d)->ofs + (u32) (d)))

class ResultScreen {
public:
    int state;              // 0x0
    R22cResultData* data;   // 0x4

    void read();
    void reloadtime();
    void highscore(int score);
    void init();
    int move(int flag);
    void quit();
};

struct R22cWork {
    u8 step;             // 0x00  r22c_shootFunc index
    u8 resultStep;       // 0x01
    u8 pad_02[2];
    int timer;           // 0x04
    int time;            // 0x08  frames played (DispTime)
    int hits;            // 0x0C
    int score;           // 0x10
    int total;           // 0x14  targets created
    int ageSum;          // 0x18
    int level;           // 0x1C  1..4
    int state;           // 0x20
    int combo;           // 0x24
    int pause;           // 0x28  frames left of a `{time, 0xFE}` pause
    int ufoWait;         // 0x2C
    u32 shotTotal;       // 0x30  pG->shotTotal2 at the start
    u32 shotHit;         // 0x34  pG->shotHit2 at the start
    cSat* eat;           // 0x38
    int wepSel;          // 0x3C
    u32 effFlags;        // 0x40  R22cHitEffect bits
    u8 wepNo;            // 0x44
    u8 wepType;          // 0x45
    u8 cnt46;            // 0x46  type 3 hits
    u8 cnt47;            // 0x47  kind 0 hits
    u8 pad_48[4];
    s32** tbl;           // 0x4C  level script
    void* itemSaveBuf;   // 0x50
    int itemSel;         // 0x54
    int cap[24];         // 0x58
    u16 capId;           // 0xB8
    u8 pad_BA[2];
    cEmDoor* door[2];        // 0xBC
    u32 strId;           // 0xC4  SndStrReq handle
    cEm* wepMan;         // 0xC8
    int effTimer;        // 0xCC
    int itemNum[24];     // 0xD0
    ResultScreen result; // 0x130
    IDSystem score2;     // 0x138  the floating score numbers
    int scoreTimer[8];   // 0x188
};


static R22cWork* r22c_work;


static s32 r22c_d0[] = {0, 0, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d24[] = {300, 0, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d48[] = {600, 0, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d6C[] = {900, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d90[] = {900, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_dB4[] = {1200, 0, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_dD8[] = {1200, 0, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_dFC[] = {1500, 1, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d120[] = {1800, 0, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d144[] = {1800, 0, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d168[] = {2100, 254};
static s32 r22c_d170[] = {3000, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d194[] = {3000, 0, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d1B8[] = {3300, 1, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d1DC[] = {3300, 0, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d200[] = {3600, 0, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d224[] = {3600, 0, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d248[] = {3600, 0, 0, 200, -18000, 0, 2, 240, 1};
static s32 r22c_d26C[] = {3900, 0, 2500, 200, -26000, 0, 3, -2500, 200, -26000, 50, 3, 2500, 200, -26000, 50, 3, -2500, 200, -26000, 50, 3, 2500, 200, -26000, 50, 1};
static s32 r22c_d2D8[] = {3900, 0, 2500, 200, -22000, 0, 3, -2500, 200, -22000, 50, 3, 2500, 200, -22000, 50, 3, -2500, 200, -22000, 50, 3, 2500, 200, -22000, 50, 1};
static s32 r22c_d344[] = {4200, 1, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d368[] = {4200, 0, 0, 200, -22000, 0, 2, 4200, 1};
static s32 r22c_d38C[] = {4200, 1, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d3B0[] = {4500, 1, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d3D4[] = {4500, 0, -2500, 200, -26000, 0, 3, -2500, 200, -22000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -22000, 100, 3, -2500, 200, -26000, 100, 1};
static s32 r22c_d440[] = {4500, 0, 2500, 200, -26000, 0, 3, 2500, 200, -22000, 100, 3, 2500, 200, -26000, 100, 3, 2500, 200, -22000, 100, 3, 2500, 200, -26000, 100, 1};
static s32* r22c_d4AC[] = {(s32*) 26, (s32*) 4800, r22c_d0, r22c_d24, r22c_d48, r22c_d6C, r22c_d90, r22c_dB4, r22c_dD8, r22c_dFC, r22c_d120, r22c_d144, r22c_d168, r22c_d170, r22c_d194, r22c_d1B8, r22c_d1DC, r22c_d200, r22c_d224, r22c_d248, r22c_d26C, r22c_d2D8, r22c_d344, r22c_d368, r22c_d38C, r22c_d3B0, r22c_d3D4, r22c_d440};
static s32 r22c_d51C[] = {0, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d540[] = {300, 0, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d564[] = {600, 2, 0, 200, -22000, 0, 2, 600, 1};
static s32 r22c_d588[] = {630, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d5AC[] = {630, 1, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d5D0[] = {900, 1, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d5F4[] = {900, 0, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d618[] = {300, 0, -800, 200, -22000, 0, 2, 120, 1};
static s32 r22c_d63C[] = {300, 0, 0, 200, -22000, 0, 2, 120, 1};
static s32 r22c_d660[] = {300, 0, 800, 200, -22000, 0, 2, 120, 1};
static s32 r22c_d684[] = {300, 0, 0, 200, -26000, 0, 2, 120, 1};
static s32 r22c_d6A8[] = {300, 0, 0, 200, -26000, 0, 2, 120, 1};
static s32 r22c_d6CC[] = {300, 0, 0, 200, -22000, 0, 2, 120, 1};
static s32 r22c_d6F0[] = {300, 0, 0, 200, -22000, 0, 2, 120, 1};
static s32 r22c_d714[] = {300, 2, 0, 200, -18000, 0, 2, 240, 1};
static s32 r22c_d738[] = {540, 1, 2500, 200, -22000, 0, 3, -2500, 200, -22000, 100, 3, 2500, 200, -22000, 100, 3, -2500, 200, -22000, 100, 3, 2500, 200, -22000, 100, 1};
static s32 r22c_d7A4[] = {540, 0, -2500, 200, -26000, 0, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 1};
static s32 r22c_d810[] = {300, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d834[] = {300, 0, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d858[] = {300, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d87C[] = {300, 2, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d8A0[] = {540, 0, 2500, 200, -26000, 0, 3, -2500, 200, -26000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, 2500, 200, -26000, 100, 1};
static s32 r22c_d90C[] = {540, 1, -2500, 200, -18000, 0, 3, 2500, 200, -18000, 100, 3, -2500, 200, -18000, 100, 3, 2500, 200, -18000, 100, 3, -2500, 200, -18000, 100, 1};
static s32 r22c_d978[] = {540, 0, -2500, 200, -26000, 0, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 1};
static s32 r22c_d9E4[] = {540, 0, -2500, 200, -26000, 0, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 1};
static s32* r22c_dA50[] = {(s32*) 22, (s32*) 1800, r22c_d51C, r22c_d540, r22c_d564, r22c_d588, r22c_d5AC, r22c_d5D0, r22c_d5F4, r22c_d618, r22c_d63C, r22c_d660, r22c_d684, r22c_d6A8, r22c_d6CC, r22c_d6F0, r22c_d714, r22c_d738, r22c_d7A4, r22c_d810, r22c_d834, r22c_d858, r22c_d87C, r22c_d8A0, r22c_d90C, r22c_d978, r22c_d9E4};
static s32 r22c_dABC[] = {0, 0, -2500, 200, -22000, 0, 2, 180, 1};
static s32 r22c_dAE0[] = {0, 0, 0, 200, -22000, 0, 2, 180, 1};
static s32 r22c_dB04[] = {180, 0, 2500, 200, -26000, 0, 2, 180, 1};
static s32 r22c_dB28[] = {180, 1, -2500, 200, -18000, 0, 2, 180, 1};
static s32 r22c_dB4C[] = {360, 2, 0, 200, -18000, 0, 2, 180, 1};
static s32 r22c_dB70[] = {360, 0, 0, 200, -26000, 0, 2, 180, 1};
static s32 r22c_dB94[] = {540, 0, 2500, 200, -18000, 0, 3, 2500, 200, -26000, 100, 3, 2500, 200, -18000, 100, 3, 2500, 200, -26000, 100, 3, 2500, 200, -18000, 100, 1};
static s32 r22c_dC00[] = {1050, 254};
static s32 r22c_dC08[] = {1200, 1, -2500, 200, -26000, 0, 3, -2500, 200, -18000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -18000, 100, 3, -2500, 200, -26000, 100, 1};
static s32 r22c_dC74[] = {1500, 0, -2500, 200, -22000, 0, 2, 90, 1};
static s32 r22c_dC98[] = {1500, 1, 2500, 200, -22000, 0, 2, 90, 1};
static s32 r22c_dCBC[] = {1620, 2, 0, 200, -22000, 0, 3, 2500, 200, -22000, 100, 3, 0, 200, -22000, 100, 3, 2500, 200, -22000, 100, 3, 0, 200, -22000, 100, 1};
static s32 r22c_dD28[] = {1620, 1, 0, 200, -26000, 0, 3, -2500, 200, -26000, 100, 3, 0, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, 0, 200, -26000, 100, 1};
static s32 r22c_dD94[] = {1620, 0, 0, 200, -18000, 0, 3, -2500, 200, -18000, 100, 3, 0, 200, -18000, 100, 3, -2500, 200, -18000, 100, 3, 0, 200, -18000, 100, 1};
static s32 r22c_dE00[] = {2100, 0, -2500, 200, -26000, 0, 2, 120, 1};
static s32 r22c_dE24[] = {2100, 0, -2500, 200, -22000, 0, 2, 120, 1};
static s32 r22c_dE48[] = {2100, 0, -2500, 200, -18000, 0, 2, 120, 1};
static s32 r22c_dE6C[] = {2250, 0, 2500, 200, -26000, 0, 2, 120, 1};
static s32 r22c_dE90[] = {2250, 2, 2500, 200, -22000, 0, 2, 120, 1};
static s32 r22c_dEB4[] = {2250, 0, 2500, 200, -18000, 0, 2, 120, 1};
static s32 r22c_dED8[] = {2400, 1, -2500, 200, -26000, 0, 2, 150, 1};
static s32 r22c_dEFC[] = {2400, 0, 0, 200, -26000, 0, 2, 150, 1};
static s32 r22c_dF20[] = {2400, 1, 2500, 200, -26000, 0, 2, 150, 1};
static s32 r22c_dF44[] = {2400, 2, -2500, 200, -22000, 0, 3, 2500, 200, -22000, 50, 3, -2500, 200, -22000, 50, 3, 2500, 200, -22000, 50, 3, -2500, 200, -22000, 50, 1};
static s32* r22c_dFB0[] = {(s32*) 24, (s32*) 2700, r22c_dABC, r22c_dAE0, r22c_dB04, r22c_dB28, r22c_dB4C, r22c_dB70, r22c_dB94, r22c_dC00, r22c_dC08, r22c_dC74, r22c_dC98, r22c_dCBC, r22c_dD28, r22c_dD94, r22c_dE00, r22c_dE24, r22c_dE48, r22c_dE6C, r22c_dE90, r22c_dEB4, r22c_dED8, r22c_dEFC, r22c_dF20, r22c_dF44};
static s32 r22c_d1018[] = {0, 0, -2500, 200, -31000, 0, 3, 0, 200, -31000, 80, 3, -2500, 200, -31000, 80, 3, 0, 200, -31000, 80, 3, -2500, 200, -31000, 80, 1};
static s32 r22c_d1084[] = {0, 0, 2500, 200, -26000, 0, 3, 0, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 0, 200, -26000, 80, 3, 2500, 200, -26000, 80, 1};
static s32 r22c_d10F0[] = {0, 0, -2500, 200, -22000, 0, 3, 0, 200, -22000, 80, 3, -2500, 200, -22000, 80, 3, 0, 200, -22000, 80, 3, -2500, 200, -22000, 80, 1};
static s32 r22c_d115C[] = {2100, 0, -2500, 200, -26000, 0, 2, 180, 1};
static s32 r22c_d1180[] = {2100, 0, 0, 200, -26000, 0, 2, 180, 1};
static s32 r22c_d11A4[] = {2100, 0, 2500, 200, -26000, 0, 2, 180, 1};
static s32 r22c_d11C8[] = {2100, 0, -2500, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d11EC[] = {2100, 0, 0, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d1210[] = {2100, 0, 2500, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d1234[] = {2100, 0, -2500, 200, -18000, 0, 2, 180, 1};
static s32 r22c_d1258[] = {2100, 0, 0, 200, -18000, 0, 2, 180, 1};
static s32 r22c_d127C[] = {2100, 0, 2500, 200, -18000, 0, 2, 180, 1};
static s32 r22c_d12A0[] = {2400, 0, 2500, 200, -36000, 0, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 1};
static s32 r22c_d130C[] = {2400, 0, 0, 200, -31000, 0, 3, 0, 200, -22000, 80, 3, 0, 200, -31000, 80, 3, 0, 200, -22000, 80, 3, 0, 200, -31000, 80, 1};
static s32 r22c_d1378[] = {2400, 0, -2500, 200, -26000, 0, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 1};
static s32 r22c_d13E4[] = {2700, 0, -2500, 200, -31000, 0, 2, 180, 1};
static s32 r22c_d1408[] = {2700, 0, 2500, 200, -31000, 0, 2, 180, 1};
static s32 r22c_d142C[] = {2700, 6, 0, 200, -26000, 0, 2, 180, 1};
static s32 r22c_d1450[] = {2700, 0, -2500, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d1474[] = {2700, 0, 2500, 200, -22000, 0, 2, 180, 1};
static s32* r22c_d1498[] = {(s32*) 20, (s32*) 3000, r22c_d1018, r22c_d1084, r22c_d10F0, r22c_d115C, r22c_d1180, r22c_d11A4, r22c_d11C8, r22c_d11EC, r22c_d1210, r22c_d1234, r22c_d1258, r22c_d127C, r22c_d12A0, r22c_d130C, r22c_d1378, r22c_d13E4, r22c_d1408, r22c_d142C, r22c_d1450, r22c_d1474};
static s32 r22c_d14F0[] = {0, 0, -2500, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d1514[] = {0, 0, 2500, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d1538[] = {300, 0, -2500, 200, -26000, 0, 2, 180, 1};
static s32 r22c_d155C[] = {300, 0, 2500, 200, -26000, 0, 2, 180, 1};
static s32 r22c_d1580[] = {600, 1, 2500, 200, -18000, 0, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 3, 2500, 200, -18000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 3, 2500, 200, -18000, 80, 1};
static s32 r22c_d163C[] = {600, 0, -2500, 200, -26000, 0, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 3, 2500, 200, -26000, 80, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 3, 2500, 200, -26000, 80, 3, -2500, 200, -26000, 80, 1};
static s32 r22c_d16F8[] = {1500, 2, 0, 200, -18000, 0, 2, 300, 1};
static s32 r22c_d171C[] = {1500, 1, -2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d1740[] = {1500, 0, 2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d1764[] = {1500, 1, 0, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d1788[] = {2100, 254};
static s32 r22c_d1790[] = {2400, 0, -2500, 200, -26000, 0, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 1};
static s32 r22c_d17FC[] = {2400, 1, 0, 200, -22000, 0, 3, 0, 200, -18000, 80, 3, 0, 200, -26000, 80, 3, 0, 200, -18000, 80, 3, 0, 200, -26000, 80, 3, 0, 200, -22000, 80, 1};
static s32 r22c_d187C[] = {2400, 0, 2500, 200, -18000, 0, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 1};
static s32 r22c_d18E8[] = {3000, 1, 2500, 200, -26000, 0, 3, 2500, 200, -18000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 1};
static s32 r22c_d19A4[] = {3000, 1, -2500, 200, -18000, 0, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -18000, 80, 3, -2500, 200, -18000, 80, 1};
static s32 r22c_d1A60[] = {3600, 0, 0, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d1A84[] = {3600, 1, -2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d1AA8[] = {3600, 2, 0, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d1ACC[] = {3600, 1, 2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d1AF0[] = {3600, 0, 0, 200, -18000, 0, 2, 300, 1};
static s32* r22c_d1B14[] = {(s32*) 21, (s32*) 4500, r22c_d14F0, r22c_d1514, r22c_d1538, r22c_d155C, r22c_d1580, r22c_d163C, r22c_d16F8, r22c_d171C, r22c_d1740, r22c_d1764, r22c_d1788, r22c_d1790, r22c_d17FC, r22c_d187C, r22c_d18E8, r22c_d19A4, r22c_d1A60, r22c_d1A84, r22c_d1AA8, r22c_d1ACC, r22c_d1AF0};
static s32 r22c_d1B70[] = {0, 0, -2500, 200, -26000, 0, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 3, -2500, 200, -26000, 80, 1};
static s32 r22c_d1BDC[] = {0, 0, 2500, 200, -26000, 0, 3, 2500, 200, -36000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 3, 2500, 200, -26000, 80, 1};
static s32 r22c_d1C48[] = {600, 1, 0, 200, -31000, 0, 3, 2500, 200, -31000, 80, 3, -2500, 200, -31000, 80, 3, 2500, 200, -31000, 80, 3, -2500, 200, -31000, 80, 3, 0, 200, -31000, 80, 1};
static s32 r22c_d1CC8[] = {600, 2, 0, 200, -22000, 0, 3, -2500, 200, -22000, 80, 3, 2500, 200, -22000, 80, 3, -2500, 200, -22000, 80, 3, 2500, 200, -22000, 80, 3, 0, 200, -22000, 80, 1};
static s32 r22c_d1D48[] = {1200, 1, 2500, 200, -36000, 0, 3, 2500, 200, -26000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 1};
static s32 r22c_d1DA0[] = {1800, 254};
static s32 r22c_d1DA8[] = {2100, 2, 0, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d1DCC[] = {2100, 2, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d1DF0[] = {2220, 0, -2500, 200, -36000, 0, 3, 2500, 200, -36000, 80, 2, 300, 1};
static s32 r22c_d1E28[] = {2220, 0, -2500, 200, -26000, 0, 3, 2500, 200, -26000, 80, 2, 300, 1};
static s32 r22c_d1E60[] = {2220, 1, -2500, 200, -18000, 0, 3, 2500, 200, -18000, 80, 2, 300, 1};
static s32 r22c_d1E98[] = {2340, 0, -2500, 200, -36000, 0, 2, 240, 1};
static s32 r22c_d1EBC[] = {2340, 0, 2500, 200, -36000, 0, 2, 240, 1};
static s32 r22c_d1EE0[] = {2340, 1, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d1F04[] = {2340, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d1F28[] = {2340, 0, -2500, 200, -18000, 0, 2, 240, 1};
static s32 r22c_d1F4C[] = {2340, 1, 2500, 200, -18000, 0, 2, 240, 1};
static s32 r22c_d1F70[] = {2460, 0, -2500, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d1F94[] = {2460, 0, 2500, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d1FB8[] = {2460, 1, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d1FDC[] = {2460, 0, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d2000[] = {2460, 0, 2500, 200, -22000, 0, 2, 240, 1};
static s32* r22c_d2024[] = {(s32*) 22, (s32*) 3000, r22c_d1B70, r22c_d1BDC, r22c_d1C48, r22c_d1CC8, r22c_d1D48, r22c_d1DA0, r22c_d1DA8, r22c_d1DCC, r22c_d1DF0, r22c_d1E28, r22c_d1E60, r22c_d1E98, r22c_d1EBC, r22c_d1EE0, r22c_d1F04, r22c_d1F28, r22c_d1F4C, r22c_d1F70, r22c_d1F94, r22c_d1FB8, r22c_d1FDC, r22c_d2000};
static s32 r22c_d2084[] = {0, 1, -2500, 200, -36000, 0, 3, -2500, 200, -22000, 80, 3, -2500, 200, -36000, 80, 3, -2500, 200, -22000, 80, 3, -2500, 200, -36000, 80, 1};
static s32 r22c_d20F0[] = {0, 1, 0, 200, -22000, 0, 3, 0, 200, -36000, 80, 3, 0, 200, -22000, 80, 3, 0, 200, -36000, 80, 3, 0, 200, -22000, 80, 1};
static s32 r22c_d215C[] = {0, 1, 2500, 200, -36000, 0, 3, 2500, 200, -22000, 80, 3, 2500, 200, -36000, 80, 3, 2500, 200, -22000, 80, 3, 2500, 200, -36000, 80, 1};
static s32 r22c_d21C8[] = {900, 2, 0, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d21EC[] = {900, 0, 2500, 200, -22000, 0, 3, -2500, 200, -22000, 80, 3, -2500, 200, -26000, 80, 3, -2500, 200, -18000, 80, 1};
static s32 r22c_d2244[] = {900, 0, -2500, 200, -31000, 0, 3, 2500, 200, -31000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 1};
static s32 r22c_d229C[] = {1500, 2, -2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d22C0[] = {1500, 2, 2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d22E4[] = {1500, 2, 0, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d2308[] = {1500, 2, -2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d232C[] = {1500, 2, 2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2350[] = {1530, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2374[] = {1530, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2398[] = {1800, 2, -2500, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d23BC[] = {1800, 2, 0, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d23E0[] = {1800, 2, 2500, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d2404[] = {1800, 2, 0, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2428[] = {1830, 1, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d244C[] = {2100, 2, 0, 200, -36000, 0, 2, 300, 1};
static s32 r22c_d2470[] = {2100, 2, -2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d2494[] = {2100, 2, 2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d24B8[] = {2100, 2, 0, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d24DC[] = {2130, 1, 0, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d2500[] = {2400, 254};
static s32 r22c_d2508[] = {2700, 2, 0, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d252C[] = {2700, 2, 0, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2550[] = {2730, 0, 0, 200, -26000, 0, 3, 2500, 200, -26000, 80, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, -2500, 200, -26000, 80, 3, 0, 200, -26000, 80, 1};
static s32 r22c_d25F8[] = {2730, 0, 0, 200, -36000, 0, 3, -2500, 200, -36000, 80, 3, 2500, 200, -36000, 80, 3, -2500, 200, -36000, 80, 3, 2500, 200, -36000, 80, 3, -2500, 200, -36000, 80, 3, 2500, 200, -36000, 80, 3, 0, 200, -36000, 80, 1};
static s32 r22c_d26A0[] = {3000, 2, 0, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d26C4[] = {3030, 0, -2500, 200, -36000, 0, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 1};
static s32 r22c_d27D0[] = {3030, 0, 2500, 200, -26000, 0, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 1};
static s32 r22c_d28DC[] = {4500, 0, -2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2900[] = {4500, 0, 0, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2924[] = {4500, 0, 2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2948[] = {4530, 0, -2500, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d296C[] = {4530, 6, 0, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d2990[] = {4530, 0, 2500, 200, -26000, 0, 2, 300, 1};
static s32 r22c_d29B4[] = {4560, 0, -2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d29D8[] = {4560, 0, 0, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d29FC[] = {4560, 0, 2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d2A20[] = {6000, 2, -2500, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d2A44[] = {6000, 2, 0, 200, -31000, 0, 2, 300, 1};
static s32 r22c_d2A68[] = {6000, 2, 0, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2A8C[] = {6000, 2, 2500, 200, -22000, 0, 2, 300, 1};
static s32 r22c_d2AB0[] = {6030, 1, -2500, 200, -26000, 0, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 3, -2500, 200, -36000, 80, 1};
static s32 r22c_d2B08[] = {6030, 0, -2500, 200, -22000, 0, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 3, 0, 200, -36000, 80, 1};
static s32 r22c_d2B74[] = {6030, 0, -2500, 200, -18000, 0, 3, -2500, 200, -26000, 80, 3, 2500, 200, -26000, 80, 3, 2500, 200, -36000, 80, 1};
static s32* r22c_d2BCC[] = {(s32*) 47, (s32*) 6300, r22c_d2084, r22c_d20F0, r22c_d215C, r22c_d21C8, r22c_d21EC, r22c_d2244, r22c_d229C, r22c_d22C0, r22c_d22E4, r22c_d2308, r22c_d232C, r22c_d2350, r22c_d2374, r22c_d2398, r22c_d23BC, r22c_d23E0, r22c_d2404, r22c_d2428, r22c_d244C, r22c_d2470, r22c_d2494, r22c_d24B8, r22c_d24DC, r22c_d2500, r22c_d2508, r22c_d252C, r22c_d2550, r22c_d25F8, r22c_d26A0, r22c_d26C4, r22c_d27D0, r22c_d28DC, r22c_d2900, r22c_d2924, r22c_d2948, r22c_d296C, r22c_d2990, r22c_d29B4, r22c_d29D8, r22c_d29FC, r22c_d2A20, r22c_d2A44, r22c_d2A68, r22c_d2A8C, r22c_d2AB0, r22c_d2B08, r22c_d2B74};
static s32 r22c_d2C90[] = {0, 2, -2500, 200, -31000, 0, 2, 270, 1};
static s32 r22c_d2CB4[] = {0, 2, 2500, 200, -31000, 0, 2, 270, 1};
static s32 r22c_d2CD8[] = {0, 2, 0, 200, -26000, 0, 2, 270, 1};
static s32 r22c_d2CFC[] = {0, 2, -2500, 200, -22000, 0, 2, 270, 1};
static s32 r22c_d2D20[] = {0, 2, 2500, 200, -22000, 0, 2, 270, 1};
static s32 r22c_d2D44[] = {60, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2D68[] = {60, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2D8C[] = {120, 0, 0, 200, -22000, 0, 2, 180, 1};
static s32 r22c_d2DB0[] = {120, 0, 0, 200, -31000, 0, 2, 180, 1};
static s32 r22c_d2DD4[] = {300, 2, -2500, 200, -31000, 0, 3, 2500, 200, -31000, 50, 1};
static s32 r22c_d2E04[] = {300, 2, 2500, 200, -22000, 0, 3, -2500, 200, -22000, 50, 1};
static s32 r22c_d2E34[] = {330, 0, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2E58[] = {330, 0, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2E7C[] = {330, 0, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2EA0[] = {600, 2, -2500, 200, -26000, 0, 2, 270, 1};
static s32 r22c_d2EC4[] = {600, 2, 0, 200, -26000, 0, 2, 270, 1};
static s32 r22c_d2EE8[] = {600, 2, 2500, 200, -26000, 0, 2, 270, 1};
static s32 r22c_d2F0C[] = {630, 0, -1250, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2F30[] = {630, 0, 1250, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d2F54[] = {900, 0, -2500, 200, -36000, 0, 3, 2500, 200, -36000, 200, 3, 2500, 200, -18000, 200, 3, -2500, 200, -18000, 200, 3, -2500, 200, -36000, 200, 1};
static s32 r22c_d2FC0[] = {900, 0, 2500, 200, -18000, 0, 3, -2500, 200, -18000, 200, 3, -2500, 200, -36000, 200, 3, 2500, 200, -36000, 200, 3, 2500, 200, -18000, 200, 1};
static s32 r22c_d302C[] = {1500, 254};
static s32 r22c_d3034[] = {3000, 2, -2500, 200, -36000, 0, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -18000, 100, 3, 2500, 200, -18000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 1};
static s32 r22c_d30F0[] = {3030, 1, -2500, 200, -36000, 0, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -18000, 100, 3, 2500, 200, -18000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 1};
static s32 r22c_d31AC[] = {3060, 2, -2500, 200, -36000, 0, 3, 2500, 200, -36000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -18000, 100, 3, 2500, 200, -18000, 100, 3, 2500, 200, -26000, 100, 3, -2500, 200, -26000, 100, 3, -2500, 200, -36000, 100, 1};
static s32 r22c_d3268[] = {3300, 2, 2500, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d328C[] = {3360, 0, 2500, 200, -31000, 0, 2, 90, 1};
static s32 r22c_d32B0[] = {3450, 2, -2500, 200, -22000, 0, 2, 90, 1};
static s32 r22c_d32D4[] = {3510, 1, -2500, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d32F8[] = {3600, 2, 0, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d331C[] = {3660, 0, -2500, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d3340[] = {3660, 0, 0, 200, -31000, 0, 2, 90, 1};
static s32 r22c_d3364[] = {3660, 0, 2500, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d3388[] = {3660, 0, 0, 200, -22000, 0, 2, 90, 1};
static s32 r22c_d33AC[] = {3900, 1, 0, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d33D0[] = {3900, 0, 0, 200, -26000, 0, 2, 90, 1};
static s32 r22c_d33F4[] = {3960, 2, -2500, 200, -26000, 0, 2, 150, 1};
static s32 r22c_d3418[] = {3960, 2, 2500, 200, -26000, 0, 2, 150, 1};
static s32 r22c_d343C[] = {3960, 2, -2500, 200, -22000, 0, 2, 150, 1};
static s32 r22c_d3460[] = {3960, 2, 0, 200, -22000, 0, 2, 150, 1};
static s32 r22c_d3484[] = {3960, 2, 2500, 200, -22000, 0, 2, 150, 1};
static s32 r22c_d34A8[] = {3960, 2, -2500, 200, -31000, 0, 2, 150, 1};
static s32 r22c_d34CC[] = {3960, 2, 0, 200, -31000, 0, 2, 150, 1};
static s32 r22c_d34F0[] = {3960, 2, 2500, 200, -31000, 0, 2, 150, 1};
static s32 r22c_d3514[] = {4500, 0, -2500, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d3538[] = {4500, 1, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d355C[] = {4530, 2, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d3580[] = {4530, 2, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d35A4[] = {4530, 2, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d35C8[] = {4530, 2, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d35EC[] = {4530, 2, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d3610[] = {4530, 2, 0, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d3634[] = {4530, 2, 2500, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d3658[] = {4800, 1, -2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d367C[] = {4800, 0, 2500, 200, -36000, 0, 2, 240, 1};
static s32 r22c_d36A0[] = {4830, 2, 0, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d36C4[] = {4830, 2, 2500, 200, -26000, 0, 2, 240, 1};
static s32 r22c_d36E8[] = {4830, 2, -2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d370C[] = {4830, 2, 0, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d3730[] = {4830, 2, 2500, 200, -22000, 0, 2, 240, 1};
static s32 r22c_d3754[] = {4830, 2, -2500, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d3778[] = {4830, 2, 0, 200, -31000, 0, 2, 240, 1};
static s32 r22c_d379C[] = {4830, 2, 2500, 200, -31000, 0, 2, 240, 1};
static s32* r22c_d37C0[] = {(s32*) 63, (s32*) 6000, r22c_d2C90, r22c_d2CB4, r22c_d2CD8, r22c_d2CFC, r22c_d2D20, r22c_d2D44, r22c_d2D68, r22c_d2D8C, r22c_d2DB0, r22c_d2DD4, r22c_d2E04, r22c_d2E34, r22c_d2E58, r22c_d2E7C, r22c_d2EA0, r22c_d2EC4, r22c_d2EE8, r22c_d2F0C, r22c_d2F30, r22c_d2F54, r22c_d2FC0, r22c_d302C, r22c_d3034, r22c_d30F0, r22c_d31AC, r22c_d3268, r22c_d328C, r22c_d32B0, r22c_d32D4, r22c_d32F8, r22c_d331C, r22c_d3340, r22c_d3364, r22c_d3388, r22c_d33AC, r22c_d33D0, r22c_d33F4, r22c_d3418, r22c_d343C, r22c_d3460, r22c_d3484, r22c_d34A8, r22c_d34CC, r22c_d34F0, r22c_d3514, r22c_d3538, r22c_d355C, r22c_d3580, r22c_d35A4, r22c_d35C8, r22c_d35EC, r22c_d3610, r22c_d3634, r22c_d3658, r22c_d367C, r22c_d36A0, r22c_d36C4, r22c_d36E8, r22c_d370C, r22c_d3730, r22c_d3754, r22c_d3778, r22c_d379C};

// Hit points per target type (rows) and hit kind (columns); kind 4 always scores 200.
static const int r22c_scoreTbl[7][5] = {
    {50, 100, 10, 25, 0},
    {50, 200, 20, 50, 0},
    {-1000, -1000, -1000, -1000, 0},
    {500, 500, 100, 100, 0},
    {100, 250, 20, 30, 0},
    {100, 250, 20, 30, 0},
    {50, 100, 10, 25, 0},
};

void r22c_exitDoor();
cObj* getCap(int a, int b, int c, int d, int e);
void checkBottleCap();
void getBottleCap();
int weaponSelect(int sel);
void itemSave();
void gameEnd();
static void r22c_startShootingGame();
static void shootInit();
static void shootReady();
static void shootMain();
static void shootResult();
static void shootEnd();
int isWepmanAlive();
static void r22cSetWepMan();
static void r22cGateCtrl();
static void r22c_checkShootingScore();
static void r22c_checkExitDoor();
static void r22c_AshleyCtrl();
void ScoreInit();
void ScoreSet(int pt, Vec* pos);

extern "C" {
void getBonus();
int r22c_checkGameLevel();
int r22c_checkGame();
int countMark();
static void funcUfo();
void deleteAllMark();
void scoreRegist();
void setWepmanKilled();
void ScoreClear();
void ScoreMove();
}

// Room init (the shooting range): the result screen data file, the floating scores, the target enemy
// (0x3E) pre-read; the game level (A..D) from the room the player came from (r204 / r211 / ... each
// range entrance is a level); the range keeper and gate tasks, area 0 = start the game, area 7 = the
// high-score board, area 2 = the exit door; the two range doors paired and close-locked; Ashley's wait
// task; the bottle caps owned are counted.
void R22cInit()
{
#line 1978 "D:/Bio4/Prog/r22c.cpp"
    r22c_work = (R22cWork*) MEM_CALLOC(sizeof(R22cWork), 1, 0xd);
    r22c_work->result.read();
    ScoreInit();
    EmReadSearch(0x3E, 0, 0);
    switch (pG->room_id_prev) {
    case 0x204:
    default:
        r22c_work->level = 1;
        break;
    case 0x211:
        r22c_work->level = 2;
        break;
    case 0x220:
        r22c_work->level = 3;
        break;
    case 0x305:
    case 0x31D:
        r22c_work->level = 4;
        break;
    }
    switch (pG->JumpPoint) {
    case 1:
        r22c_work->level = 2;
        break;
    case 2:
        r22c_work->level = 3;
        break;
    }
    if (Joy[0].on & 0x40) {
        r22c_work->level = 4;
    }
    SceExec(0x12, (TaskFunc) r22cSetWepMan, 0, 0, SCE_PRIO_DEF_2, 0);
    SceExec(0x12, (TaskFunc) r22cGateCtrl, 0, 0, SCE_PRIO_DEF_2, 0);
    SceAtDataSet_exec(0, SCE_LEVEL10, 0, (TaskFunc) r22c_startShootingGame, 0, 1);
    SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) r22c_checkShootingScore, 0, 1);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r22c_checkExitDoor, 0, 1);
    SceAtSetActColor(2, 1);
    if (getRoomEtcDoor(0, &r22c_work->door[0], 1) && getRoomEtcDoor(1, &r22c_work->door[1], 1)) {
        r22c_work->door[0]->setDoor(r22c_work->door[1]);
        r22c_work->door[0]->setCloseLock();
        r22c_work->door[1]->setCloseLock();
    }
    SmdGetObjPtr(0)->be_flag &= ~2;
    SmdGetObjPtr(1)->be_flag &= ~2;
    SmdGetObjPtr(2)->be_flag &= ~2;
    SmdGetObjPtr(3)->be_flag &= ~2;
    SceExec(0x12, (TaskFunc) r22c_AshleyCtrl, 0, 0, SCE_PRIO_DEF_2, 0);
    LightMgr.onKind(1);
    LightMgr.offKind(2);
    {
        int i;

        for (i = 0; i < 24; i++) {
            r22c_work->itemNum[i] = ItemMgr.num((u16) (i + 0xDC));
        }
    }
}

static const char* r22c_levelName[5] = {"-", "A", "B", "C", "D"};

// Per frame: debug-print the level letter and game state.
void R22cMain()
{
    eprintf(0x130, 0x1A4, 0, 0, "LEVEL:%s-%d", r22c_levelName[r22c_work->level], r22c_work->state);
}

// Background gag: the birds fly off (effect 3, SE 0xE / 0xF) for 8 seconds.
static void r22c_BirdsFly()
{
    EstSet(0, -1, 0, 0, EFF_ROOM, 3, 0, ESP_CORE_KIND_MARK, 0, 0);
    SndCall(6, 0xE, 0, 0, 0, 0);
    SceSleep(480);
    SndCall(6, 0xF, 0, 0, 0, 0);
}

// Background gag: the bees (effect 1, SE 0x10 / 0x11) for 20 seconds.
static void r22c_BeeFly()
{
    EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0, ESP_CORE_KIND_MARK, 0, 0);
    SndCall(6, 0x10, 0, 0, 0, 0);
    SceSleep(1200);
    SndCall(6, 0x11, 0, 0, 0, 0);
}

// Background gag: fireworks (effect 5) worth 300 points at the range's far end.
static void r22c_FireWorks()
{
    Vec pos = {1000.0f, 3000.0f, -25200.0f};

    ScoreSet(300, &pos);
    EstSet(0, -1, 0, 0, EFF_ROOM, 5, 0, ESP_CORE_KIND_MARK, 0, 0);
    SndCall(6, 0x12, 0, 0, 0, 0);
    SceSleep(1200);
    SndCall(6, 0x13, 0, 0, 0, 0);
}

// Background gag: a shooting star (effect 4) that then drops a bonus object across the range.
static void r22c_ShootingStar()
{
    Vec pos;

    EstSet(0, -1, 0, 0, EFF_ROOM, 4, 0, ESP_CORE_KIND_MARK, 0, 0);
    SndCall(6, 0x14, 0, 0, 0, 0);
    SceSleep(90);
    SndCall(6, 0x15, 0, 0, 0, 0);
    SceSleep(90);
    cObj* o = ObjMgr.create(cObjMgr::ID_SCROLL);
    pos.x = -3951.0f;
    pos.y = 9000.0f;
    pos.z = -25337.0f;
    o->setPos(&pos);
    o->modelInit(ROOM_ARC_PTR(pG->pRoom, 0x1F), ROOM_ARC_PTR(pG->pRoom, 0x20));
    EstSet(o, -1, 0, 0, EFF_ROOM, 6, 0, ESP_CORE_KIND_NONE, o, 0);
    SceSleep(10);
    pos.x = -3951.0f;
    pos.y = 3000.0f;
    pos.z = -25337.0f;
    o->setPos(&pos);
    SceSleep(1);
    pos.x = -2999.0f;
    pos.y = 2000.0f;
    pos.z = -22657.0f;
    o->setPos(&pos);
    SceSleep(1);
    pos.x = -2055.0f;
    pos.y = 1000.0f;
    pos.z = -19877.0f;
    o->setPos(&pos);
    SceSleep(1);
    pos.x = -1113.0f;
    pos.y = 400.0f;
    pos.z = -17100.0f;
    o->setPos(&pos);
    EffectEspgenDelete(0, ESP_CORE_KIND_MARK, o);
    EstSet(0, -1, &o->pos, 0, EFF_ROOM, 7, 0, ESP_CORE_KIND_NONE, 0, 0);
    QuakeExec(0, 0, 5, 22.0f, 2);
    SndCall(6, 0x16, 0, 0, 0, 0);
    PlWepHitCheck2(0, &o->pos, &o->pos, 0x13, 0, 6000.0f);
    SceSleep(1);
    ObjMgr.destroy(o);
}

// Task: Ashley waits by the range.
static void r22c_AshleyCtrl()
{
    Vec v;

    SceSleep(1);
    if (pSUB) {
        SubCharCtrl(SCC_STOP, 1);
        cEm* sub = pSUB;
        v.x = -1600.0f;
        v.y = 0.0f;
        v.z = -270.0f;
        f32 ry = 3.0f;
        sub->setPos(&v);
        v.y = ry;
        v.x = 0.0f;
        v.z = 0.0f;
        sub->setAng(&v);
        pSUB->atari.setPriority(PRI_LV1);
    }
}

// Task: the exit door.
static void r22c_checkExitDoor()
{
    if (pG->Room_flg[0] & 0x80000000) {
        SceAtSetEnable(2, 0);
        r22c_exitDoor();
        if (isWepmanAlive() == 0) {
            SceAtSetEnable(1, 0);
        }
        SceAtSetEnable(2, 1);
    } else {
        int no;

        switch (pG->room_id_prev) {
        case 0x204:
        default:
            no = 3;
            break;
        case 0x211:
            no = 4;
            break;
        case 0x220:
            no = 5;
            break;
        case 0x305:
            no = 6;
            break;
        case 0x31D:
            no = 8;
            break;
        }
        SceAtExecute(no);
    }
}

// Task: talking to the range keeper.
static void r22c_talkWepMan()
{
    SceAtSetEnable(1, 0);
    SndCall(8, 9, &r22c_work->wepMan->pos, r22c_work->wepMan->id, 0, 0);
    if (pG->Room_flg[0] & 0x80000000) {
        SceMesSet(6, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        switch (SceMesGetSelection()) {
        case 1:
            SndCall(0, 4, 0, 0, 0, 0);
            gameEnd();
            break;
        case 2:
            SndCall(0, 4, 0, 0, 0, 0);
        WEP:
            weaponSelect(0);
            break;
        case 3:
            SndCall(0, 5, 0, 0, 0, 0);
            break;
        }
    } else {
        // The else arm's weaponSelect(0) call was cross-jumped into case 2's copy by the original
        // (its fallthrough copy survives in ours, COMPILER-DIFF 6 shape): the jump reproduces it.
        goto WEP;
    }
    SceAtSetEnable(1, 1);
}

// The exit door prompt: message 0xD yes (1) -> leave the game (gameEnd), no (2) -> stay.
void r22c_exitDoor()
{
    SceMesSet(0xD, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    switch (SceMesGetSelection()) {
    case 1:
        SndCall(0, 4, 0, 0, 0, 0);
        gameEnd();
        break;
    case 2:
        SndCall(0, 4, 0, 0, 0, 0);
        break;
    }
}

// The first bottle cap of the five the player does not own yet, starting at a random one.
cObj* getCap(int a, int b, int c, int d, int e)
{
    int tbl[5];
    u8 r;
    int i;

    tbl[0] = a;
    tbl[1] = b;
    tbl[2] = c;
    tbl[3] = d;
    tbl[4] = e;
    r = Rnd() % 5;
    for (i = 0; i < 5; i++) {
        int id = tbl[(r + i) % 5];

        if (ItemMgr.num((u16) (id + 0xDB)) == 0) {
            return (cObj*) tbl[(r + i) % 5];
        }
    }
    return 0;
}

// After a game: pick the bottle cap prize for the level / difficulty / score (0xF0 = the special one at
// 4000+ on the hard set, else the first unowned of the level's five caps above the score threshold).
void checkBottleCap()
{
    int cap = 0;

    switch (r22c_work->level) {
    case 1:
    default:
        if (r22c_work->state == 2 && r22c_work->score > 3999) {
            cap = 0xF0;
        } else if (r22c_work->score > 2999) {
            cap = (int) getCap(1, 2, 3, 4, 5);
        }
        break;
    case 2:
        if (r22c_work->state == 2 && r22c_work->score > 3999) {
            cap = 0xF1;
        } else if (r22c_work->score > 2999) {
            cap = (int) getCap(6, 7, 8, 9, 0xA);
        }
        break;
    case 3:
        if (r22c_work->state == 2 && r22c_work->hits > 0x18) {
            cap = 0xF2;
        } else if (r22c_work->score > 2999) {
            cap = (int) getCap(0xB, 0xC, 0xD, 0xE, 0xF);
        }
        break;
    case 4:
        if (r22c_work->state == 2 && r22c_work->hits > 0x18) {
            cap = 0xF3;
        } else if (r22c_work->score > 2999) {
            cap = (int) getCap(0x10, 0x11, 0x12, 0x13, 0x14);
        }
        break;
    }
    r22c_work->capId = -1;
    if (cap) {
        r22c_work->cap[cap - 1]++;
        r22c_work->capId = cap + 0xDB;
        PlCapNum[cap - 1]++;
    }
}

// Give the caps in cap[] (debug: L+R+Y grants all 24) as items 0xDC.. and update the owned counts; a
// message when a whole set is complete.
void getBottleCap()
{
    int i;
    int total;

    if ((Joy[0].on & 0x640) == 0x640) {
        for (i = 0; i < 24; i++) {
            r22c_work->cap[i] = 1;
        }
    }
    for (i = 0, total = 0; i < 24; i++) {
        if (r22c_work->cap[i]) {
            ItemMgr.get((u16) (i + 0xDC), (u16) r22c_work->cap[i]);
            r22c_work->itemNum[i] += r22c_work->cap[i];
            total += r22c_work->cap[i];
            r22c_work->cap[i] = 0;
        }
    }
    if (total > 0) {
        cMes.MesSetNumber(0, total, 0);
        SceMesSet(5, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        SndCall(0, 0x13, &r22c_work->wepMan->pos, 0, 0, 0);
        if (ItemMgr.num(0xA2) == 0) {
            ItemMgr.get(0xA2, 0);
        }
    }
    r22c_work->score = 0;
}

// The weapon choice (sel 0 asks with message 4: handgun / TMP / rifle; 3 = cancel with SE 5): equips
// the range weapon with full ammo into the player and remembers the previous weapon.
int weaponSelect(int sel)
{
    cPlayer* pl = pPL;
    int ask = sel == 0;
    u8 wep;

    if (ask) {
        SceMesSet(4, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        sel = SceMesGetSelection();
        if (sel != 3) {
            SndCall(0, 4, 0, 0, 0, 0);
        } else {
            SndCall(0, 5, 0, 0, 0, 0);
        }
    }
    wep = pG->weapon_no;
    switch (sel) {
    case 1:
        if (!(pG->Room_flg[0] & 0x80000000)) {
            itemSave();
        }
        ItemMgr.clear();
        SubScreenWk.board_size = SubScreenWk.board_next = ItemMgr.set_range(0);
        pl->weaponRelease();
        if (wep != 7 && wep != 0xB && wep != 0x13) {
            wep = 7;
        }
        pl->weaponLoad(wep, 0);
        pl->weaponInit();
        ItemMgr.arm(ItemMgr.search(WeaponNo2WeaponId(wep, 0)));
        if (ask) {
            SubScreenOpen(SS_OPEN_NORMAL, 0);
        }
        break;
    case 2:
        if (!(pG->Room_flg[0] & 0x80000000)) {
            itemSave();
        }
        ItemMgr.clear();
        SubScreenWk.board_size = SubScreenWk.board_next = ItemMgr.set_range(1);
        pl->weaponRelease();
        if (wep != 2 && wep != 9 && wep != 0x13) {
            wep = 2;
        }
        pl->weaponLoad(wep, 0);
        pl->weaponInit();
        ItemMgr.arm(ItemMgr.search(WeaponNo2WeaponId(wep, 0)));
        if (ask) {
            SubScreenOpen(SS_OPEN_NORMAL, 0);
        }
        break;
    }
    r22c_work->wepSel = sel;
    return sel;
}

// Entering the game: remember the player's weapon, Room_flg[0] bit 31 (in game), area 9 off, the doors
// normal, and back the whole inventory up (ItemMgr.save into itemSaveBuf).
void itemSave()
{
    r22c_work->wepNo = pG->weapon_no;
    r22c_work->wepType = pG->weapon_type;
    pG->Room_flg[0] |= 0x80000000;
    SceAtSetEnable(9, 0);
    r22c_work->door[0]->setNormal();
    r22c_work->door[1]->setNormal();
#line 2565 "D:/Bio4/Prog/r22c.cpp"
    r22c_work->itemSaveBuf = MEM_ALLOC(ItemMgr.saveDataSize(), 1, 0xd);
    if (r22c_work->itemSaveBuf == 0) {
        pLog->err(0, 0, "ITEM BACKUP FAILED.");
    } else {
        ItemMgr.save(r22c_work->itemSaveBuf);
        r22c_work->itemSel = (s8) SubScreenWk.board_size;
    }
}

// One set of six bottle caps complete: award the bonus item (`no`: message / scenario item flag).
#define R22C_BONUS(bit, base, mes, flg)                                     \
    if ((pG->Scenario_flg[1] & (bit)) == 0) {                                    \
        for (i = 0, n = 0; i < 6; i++) {                                    \
            if (ItemMgr.num((u16) (i + (base))) != 0) {                     \
                n++;                                                        \
            }                                                               \
        }                                                                   \
        if (n == 6) {                                                       \
            pG->Scenario_flg[1] |= (bit);                                        \
            SceMesSet((mes), 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);                            \
            SceAtExecute(flg);                                              \
            while (SceAtItemFlgCk(flg) == 0) {                              \
                SceSleep(1);                                                \
            }                                                               \
        }                                                                   \
    }

// The set-completion bonuses: for each cap set (base 0xDC/0xE2/0xE8/0xEE) not yet rewarded
// (Scenario_flg[1] bits 8/4/2/1) with all six caps owned, a message and the flag.
void getBonus()
{
    int n;
    int i;

    R22C_BONUS(8, 0xDC, 0xE, 0x80)
    R22C_BONUS(4, 0xE2, 0xF, 0x81)
    R22C_BONUS(2, 0xE8, 0x10, 0x82)
    R22C_BONUS(1, 0xEE, 0x11, 0x83)
}

// Leaving the game: fade, restore the backed-up inventory (ItemMgr.load) and the previous weapon,
// free the buffer, the doors locked again, Room_flg[0] bit 31 off.
void gameEnd()
{
    cPlayer* pl = pPL;

    pl->setNoSuspend(1);
    r22c_work->wepMan->setNoSuspend(1);
    SceEventStart(0);
    FadeSetW(2, 5, 0, 0);
    SceSleep(5);
    ItemMgr.clear();
    SubScreenWk.board_size = SubScreenWk.board_next = (u8) r22c_work->itemSel;
    ItemMgr.load(r22c_work->itemSaveBuf);
    Mem_free(r22c_work->itemSaveBuf);
    r22c_work->itemSaveBuf = 0;
    pl->weaponRelease();
    pl->weaponLoad(r22c_work->wepNo, r22c_work->wepType);
    pl->weaponInit();
    pG->Room_flg[0] &= ~0x80000000;
    SceAtSetEnable(9, 1);
    r22c_work->door[0]->setCloseLock();
    r22c_work->door[1]->setCloseLock();
    FadeSetW(0x80000002, 10, 0, 0);
    SceSleep(1);
    getBottleCap();
    getBonus();
    SceEventEnd(0);
    r22c_work->wepMan->setNoSuspend(0);
    pl->setNoSuspend(0);
}

// Debug level select (Z held while starting): returns 0 when the game was cancelled.
int r22c_checkGameLevel()
{
    int sel;

    while (1) {
        SceMesSet(0xA, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        sel = SceMesGetSelection();
        if (sel == 7) {
            SceMesSet(0xB, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
            sel = SceMesGetSelection();
            if (sel == 4) {
                sel = 0;
                break;
            }
            if (sel > 2) {
                continue;
            }
            sel += 6;
        }
        break;
    }
    if (sel != 0) {
        SndCall(0, 4, 0, 0, 0, 0);
    } else {
        SndCall(0, 5, 0, 0, 0, 0);
    }
    switch (sel) {
    case 1:
        r22c_work->level = 1;
        r22c_work->state = 1;
        break;
    case 2:
        r22c_work->level = 1;
        r22c_work->state = 2;
        break;
    case 3:
        r22c_work->level = 2;
        r22c_work->state = 1;
        break;
    case 4:
        r22c_work->level = 2;
        r22c_work->state = 2;
        break;
    case 5:
        r22c_work->level = 3;
        r22c_work->state = 1;
        break;
    case 6:
        r22c_work->level = 3;
        r22c_work->state = 2;
        break;
    case 7:
        r22c_work->level = 4;
        r22c_work->state = 1;
        break;
    case 8:
        r22c_work->level = 4;
        r22c_work->state = 2;
        break;
    default:
        sel = 0;
        break;
    }
    return sel != 0 ? 1 : 0;
}

// Game select of the current level: returns the level (0 = cancelled).
int r22c_checkGame()
{
    int sel;

    switch (pG->room_id_prev) {
    case 0x204:
    default:
        sel = 1;
        break;
    case 0x211:
        SceMesSet(1, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        sel = SceMesGetSelection();
        if (sel == 3) {
            sel = 0;
        }
        break;
    case 0x220:
        SceMesSet(2, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        sel = SceMesGetSelection();
        if (sel == 4) {
            sel = 0;
        }
        break;
    case 0x305:
        SceMesSet(3, 0, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        sel = SceMesGetSelection();
        if (sel == 5) {
            sel = 0;
        }
        break;
    }
    if (sel == 0) {
        r22c_work->state = 0;
        return 0;
    }
    r22c_work->level = sel;
    switch (sel) {
    case 1:
        if (r22c_work->itemNum[8] != 0 && r22c_work->itemNum[9] != 0) {
            r22c_work->state = 2;
        } else {
            r22c_work->state = 1;
        }
        break;
    case 2:
        if (r22c_work->itemNum[13] != 0 && r22c_work->itemNum[16] != 0) {
            r22c_work->state = 2;
        } else {
            r22c_work->state = 1;
        }
        break;
    case 3:
        r22c_work->state = 1;
        break;
    case 4:
        r22c_work->state = 1;
        break;
    }
    return sel;
}

static void (*r22c_shootFunc[5])() = {shootInit, shootReady, shootMain, shootResult, shootEnd};

// Task: the shooting game.
static void r22c_startShootingGame()
{
    SndCall(6, 5, &pPL->pList->world, 0, 0, 0);
    pG->Room_flg[0] |= 0x20000000;
    if (Joy[0].on & 0x400) {
        r22c_checkGameLevel();
    } else {
        r22c_checkGame();
    }
    if (r22c_work->state == 0) {
        SceExit();
    }
    r22c_work->step = 0;
    ScoreClear();
    for (;;) {
        r22c_shootFunc[r22c_work->step]();
        DispTime(0x28, 0x2A, 0, r22c_work->time, 7);
        ScoreMove();
        SceSleep(1);
    }
}

// Game step 0: area 0 off, the weapon chosen and reloaded, all counters zeroed, the level script
// selected (r22c_tbl by level / state), the range lights, the stream, the counter display.
static void shootInit()
{
    SceAtSetEnable(0, 0);
    int zero = 0;
    pG->Room_flg[0] &= ~0x40000000;
    weaponSelect(r22c_work->wepSel);
    ItemMgr.reload();
    r22c_work->timer = zero;
    r22c_work->hits = zero;
    r22c_work->score = zero;
    r22c_work->time = zero;
    r22c_work->combo = zero;
    r22c_work->pause = zero;
    r22c_work->ufoWait = zero;
    r22c_work->total = zero;
    r22c_work->ageSum = zero;
    r22c_work->cnt46 = zero;
    r22c_work->cnt47 = zero;
    r22c_work->shotHit = pG->g_hit_cnt;
    r22c_work->shotTotal = pG->g_shot_cnt;
    r22c_work->effTimer = zero;
    r22c_work->effFlags = zero;
    LightMgr.onKind(1);
    LightMgr.offKind(2);
    {
        R22cWork* w = r22c_work;

        switch (w->level) {
        case 1:
        default:
            switch (w->state) {
            case 1:
            default:
                w->tbl = r22c_d4AC;
                break;
            case 2:
                w->tbl = r22c_dA50;
                break;
            }
            break;
        case 2:
            switch (w->state) {
            case 1:
            default:
                w->tbl = r22c_dFB0;
                break;
            case 2:
                w->tbl = r22c_d1498;
                break;
            }
            break;
        case 3:
            switch (w->state) {
            case 1:
            default:
                w->tbl = r22c_d1B14;
                break;
            case 2:
                w->tbl = r22c_d2024;
                break;
            }
            break;
        case 4:
            switch (w->state) {
            case 1:
            default:
                w->tbl = r22c_d2BCC;
                break;
            case 2:
                w->tbl = r22c_d37C0;
                break;
            }
            break;
        }
    }
    r22c_work->strId = SndStrReq(0, 0x32, 0x80000003, 0, 0, 0.0f);
    r22c_work->step = 1;
}

static const char* r22c_startMsg = "START";

// Level B/C game 2 and level D: the targets move faster (flags_174 bit 28).
#define R22C_HARD_MODE(w)                                                                             \
    ((((w)->level == 2 || (w)->level == 3) && (w)->state == 2) || ((w)->level == 4 && (w)->state == 1) || \
     ((w)->level == 4 && (w)->state == 2))

// Game step 1: the "start" text after 30 frames, the game runs from frame 60 (step 2); Room_flg[0]
// 0x10000000 = the hard set (R22C_HARD_MODE).
static void shootReady()
{
    r22c_work->timer++;
    if (r22c_work->timer > 30) {
        eprintf(0xDC, 0x8C, 0, 0, r22c_startMsg);
    }
    if (r22c_work->timer > 60) {
        r22c_work->timer = 0;
        r22c_work->step = 2;
    }
    if (R22C_HARD_MODE(r22c_work)) {
        pG->Room_flg[0] |= 0x10000000;
    } else {
        pG->Room_flg[0] &= ~0x10000000;
    }
}

// Game step 2: the level script's records are spawned as cEmMark targets when their time comes
// (pauses via `{time, 0xFE}`, the UFO wait), hits are scored, the time counts down; the game ends
// into step 3 when the script is exhausted / cancelled.
static void shootMain()
{
    if (r22c_work->ufoWait == 0 && r22c_work->pause != 0) {
        if (!(pG->Room_flg[0] & 0x40000000)) {
            if (r22c_work->pause == 0x78) {
                r22c_work->result.reloadtime();
            }
            r22c_work->pause--;
        }
    } else {
        JOY* joy = &Joy[0];

        for (;;) {
            int n = (int) r22c_work->tbl[0];
            int i;

            for (i = 0; i < n; i++) {
                R22cMarkRec* d = (R22cMarkRec*) r22c_work->tbl[i + 2];

                if (d->time == r22c_work->timer) {
                    switch (d->flag) {
                    default:
                        if (d->flag != 2) {
                            r22c_work->total++;
                        }
                        ((cEmMark*) EmMgr.create(0x3E))->init((EmMarkData*) d);
                        break;
                    case 0xFE:
                        r22c_work->pause = 0x78;
                        SndStrReq(r22c_work->strId, 4, 0x320, 0);
                        r22c_work->strId = SndStrReq(0, 0x3E, 0x80000003, 0, 0, 0.0f);
                        break;
                    case 0xFD:
                        r22c_work->ufoWait = 30;
                        break;
                    }
                }
            }
            if (r22c_work->timer > (int) r22c_work->tbl[1]) {
                r22c_work->timer = 0;
                r22c_work->resultStep = 0;
                r22c_work->step = 3;
                break;
            }
            eprintf(0xA0, 0x1A4, 5, 0, "GIVE UP: (X) button");
            if (joy->trg & 0x400) {
                deleteAllMark();
                r22c_work->timer = 0;
                r22c_work->resultStep = 0;
                r22c_work->step = 3;
                break;
            }
            if (StaFlagChk(pG, STA_PL_MISS_SHOT)) {
                r22c_work->combo = 0;
            }
            if (r22c_work->combo == 5) {
                r22c_work->combo = 0;
                if (!(pG->Room_flg[0] & 0x40000000)) {
                    SceExec(0x12, (TaskFunc) funcUfo, 0, 0, SCE_PRIO_DEF_2, 0);
                    r22c_work->ufoWait = 3;
                }
            }
            r22c_work->timer++;
            if (countMark() != 0) {
                break;
            }
            if (r22c_work->pause != 0) {
                break;
            }
            if (r22c_work->ufoWait != 0) {
                break;
            }
        }
    }
    if (R22C_HARD_MODE(r22c_work)) {
        pG->Room_flg[0] |= 0x10000000;
    }
    if (r22c_work->ufoWait) {
        r22c_work->ufoWait--;
    }
    if (r22c_work->effTimer) {
        r22c_work->effTimer--;
    }
    r22c_work->time++;
    eprintf(0x20, 0x54, 0, 0, "%3d:%d", r22c_work->timer / 30, r22c_work->timer);
    eprintf(0x20, 0x62, 0, 0, "%3d:%d", r22c_work->time / 30, r22c_work->time);
    eprintf(0x20, 0x70, 0, 0, "%d", r22c_work->effTimer / 30);
    eprintf(0x20, 0x7E, 0, 0, "%d", r22c_work->ufoWait / 30);
}

// Game step 3: once no target is left, the score is registered and the bottle cap picked, then the
// result screen (ResultScreen) is shown until the player dismisses it, with the cap / bonus messages.
static void shootResult()
{
    switch (r22c_work->resultStep) {
    case 0: {
        int n = countMark();

        if (n == 0) {
            if (r22c_work->eat) {
                SmdGetObjPtr(3)->be_flag &= ~2;
                EatMgr.destroy(r22c_work->eat);
                r22c_work->eat = 0;
            }
            scoreRegist();
            checkBottleCap();
            EffectEspDelete(0, ESP_CORE_KIND_MARK, 0, 0);
            EffectEspgenDelete(0, ESP_CORE_KIND_MARK, 0);
            EffectEfmDelete(0, ESP_CORE_KIND_MARK, 0);
            KeyStop(0xEFCF0000);
            r22c_work->timer = 0;
            r22c_work->resultStep = 1;
        }
        break;
    }
    case 1:
        if (r22c_work->timer > 60) {
            r22c_work->resultStep = 2;
            r22c_work->result.init();
        }
        break;
    case 2: {
        int key = 0;

        if (r22c_work->timer > 90) {
            if (Key.trg & 0xC0000000) {
                r22c_work->resultStep = 3;
                key = 1;
            }
        }
        r22c_work->result.move(key);
        break;
    }
    case 3:
        if (r22c_work->result.move(0)) {
            r22c_work->resultStep = 0;
            r22c_work->step = 4;
            r22c_work->result.quit();
        }
        break;
    }
    r22c_work->timer++;
}

// Game step 4: stream faded, lights back, Stop_flg bit 31 / Room_flg[0] 0x20000000 off, area 0 on, task exit.
static void shootEnd()
{
    SndStrReq(r22c_work->strId, 4, 0xC8, 0);
    LightMgr.onKind(1);
    LightMgr.offKind(2);
    SpfFlagOff(pG, SPF_KEY);
    pG->Room_flg[0] &= ~0x20000000;
    SceAtSetEnable(0, 1);
    SceExit();
}

// Number of live targets (routines 0/1/3/4/5).
int countMark()
{
    int n = 0;
    cEm* em;

    for (em = EmMgr.getActiveWork(); em; em = EmMgr.getNext(em)) {
        if (em->isAlive() && em->id == 0x3E && em->hp > 0) {
            switch (em->type) {
            case 0:
            case 1:
            case 3:
            case 4:
            case 5:
                n++;
                break;
            case 2:
            default:
                break;
            }
        }
    }
    return n;
}

// Called by a hit target (emmark.cpp): score `pt` by type / kind (kind 4 = 200 points), the combo
// and hit counters, a floating score number at `pos`.
void R22cHitMark(int type, int kind, Vec* pos, int hit, int age)
{
    int pt;

    if (type == 3) {
        r22c_work->cnt46++;
    }
    if (kind == 4) {
        pt = 200;
    } else {
        pt = r22c_scoreTbl[type][kind];
    }
    r22c_work->score += pt;
    if (kind == 0) {
        r22c_work->cnt47++;
    }
    ScoreSet(pt, pos);
    if (hit == 1) {
        if (pt > 0) {
            r22c_work->hits++;
        }
        r22c_work->ageSum += age;
    }
    if (pt > 0) {
        r22c_work->combo++;
    }
}

static s32 r22c_d38F0[] = {0, 3, 2200, 3500, -37500, 0, 3, -2200, 3500, -37500, 30, 1};

// Task: the bonus UFO target.
static void funcUfo()
{
    int i;
    cEmMark* m;
    u32 se;

    pG->Room_flg[0] |= 0x40000000;
    for (i = 0; i < 30; i++) {
        pG->Room_flg[0] |= 0x10000000;
        SceSleep(1);
    }
    m = (cEmMark*) EmMgr.create(0x3E);
    m->init((EmMarkData*) r22c_d38F0);
    se = SndCall(6, 6, &m->pos, 0, 0, 0);
    r22c_work->total++;
    while (m->isAlive() && m->type == 3 && m->hp > 0) {
        if (se) {
            SndStop(se, 0);
        }
        pG->Room_flg[0] |= 0x10000000;
        SceSleep(1);
    }
    for (i = 0; i < 15; i++) {
        pG->Room_flg[0] |= 0x10000000;
        SceSleep(1);
    }
    pG->Room_flg[0] &= ~0x40000000;
}

// Knock down every live target (id 0x3E, types 0..9) when the game ends.
void deleteAllMark()
{
    cEm* em;

    for (em = EmMgr.getActiveWork(); em; em = EmMgr.getNext(em)) {
        if (em->id == 0x3E && em->type <= 9 && em->hp > 0) {
            ((cEmMark*) em)->setDown();
        }
    }
}

// Task: the range keeper.
static void r22cSetWepMan()
{
    if (isWepmanAlive() != 0) {
        EmListData d;
        cEm* em;

        memclr_asm(&d, sizeof(d));
        d.id = 0x18;
        d.type = 0;
        d.set = 0;
        d.flag = 0;
        d.pos[0] = 0x122;
        d.pos[1] = 0;
        d.pos[2] = -0x234;
        d.rot[0] = 0;
        d.rot[1] = -0x205B;
        d.rot[2] = 0;
        d.hp = 0x3E8;
        d.Guard_r = 0;
        d.Character = 0;
        em = EmSetEvent(&d);
        r22c_work->wepMan = em;
        em->dmg.set(0, 0x80);
        SceAtDataSet_exec(1, SCE_LEVEL10, 0, (TaskFunc) r22c_talkWepMan, 0, 1);
        SceSleep(10);
        cEmWrap w;
        w.setPtr(em, 1);
        for (;;) {
            if (w.getHp() == 0) {
                setWepmanKilled();
                SceExit();
            }
            SceSleep(1);
        }
    }
}

// High score per level (pG->shootingScore).
void scoreRegist()
{
    R22cWork* w = r22c_work;
    int lv = w->level - 1;
    if (w->score > pG->shootingScore[lv]) {
        pG->shootingScore[lv] = w->score;
    }
}

static int r22c_d3920 = 4;

// Task: the two gates in front of the range (raised while flags_174 bit 28 is set).
static void r22cGateCtrl()
{
    u8 type;
    cEmMark* up;
    cEmMark* down;
    int open;
    f32 lim;
    const f32 spd = 100.0f;

    SmdGetObjPtr(5)->be_flag &= ~2;
    SmdGetObjPtr(6)->be_flag &= ~2;
    SmdGetObjPtr(0x33)->be_flag &= ~2;
    SmdGetObjPtr(0x34)->be_flag &= ~2;
    SmdGetObjPtr(0x35)->be_flag &= ~2;
    SmdGetObjPtr(0x36)->be_flag &= ~2;
    switch (r22c_work->level) {
    case 1:
    default:
        type = 0xC;
        break;
    case 2:
        type = 0xC;
        break;
    case 3:
        type = 0xE;
        break;
    case 4:
        type = 0xE;
        break;
    }
    up = (cEmMark*) EmMgr.create(0x3E);
    up->init(type | 1, (EmMarkInst*) &r22c_d3920, 0.0f, 0.0f, 0.0f);
    down = (cEmMark*) EmMgr.create(0x3E);
    down->init(type, (EmMarkInst*) &r22c_d3920, 0.0f, 0.0f, 0.0f);
    open = 0;
    for (;;) {
        if (pG->Room_flg[0] & 0x10000000) {
            pG->Room_flg[0] &= ~0x10000000;
            lim = 5000.0f;
            if (up->pos.x < lim) {
                up->pos.x += spd;
                if (open == 0) {
                    SndCall(6, 3, &pPL->pList->world, 0, 0, 0);
                    open = 1;
                }
            } else if (open != 0) {
                SndCall(6, 4, &pPL->pList->world, 0, 0, 0);
                open = 0;
                up->pos.x = lim;
            }
            if (down->pos.x > -5000.0f) {
                down->pos.x -= spd;
            }
        } else {
            if (up->pos.x > 0.0f) {
                up->pos.x -= spd;
                if (open == 0) {
                    SndCall(6, 3, &pPL->pList->world, 0, 0, 0);
                    open = 1;
                }
            } else if (open != 0) {
                SndCall(6, 4, &pPL->pList->world, 0, 0, 0);
                open = 0;
                up->pos.x = 0.0f;
            }
            if (down->pos.x < 0.0f) {
                down->pos.x += spd;
            } else {
                down->pos.x = 0.0f;
            }
        }
        up->matUpdate();
        down->matUpdate();
        SceSleep(1);
    }
}

// Room save flag `level - 1`: the range keeper was killed.
int isWepmanAlive()
{
    int no;

    switch (r22c_work->level) {
    case 1:
    default:
        no = 0;
        break;
    case 2:
        no = 1;
        break;
    case 3:
        no = 2;
        break;
    case 4:
        no = 3;
        break;
    }
    return RsfCheck(G_ROOM_ID, no) == 0;
}

// The range keeper was killed: room save flag `level - 1`.
void setWepmanKilled()
{
    int no;

    switch (r22c_work->level) {
    case 1:
    default:
        no = 0;
        break;
    case 2:
        no = 1;
        break;
    case 3:
        no = 2;
        break;
    case 4:
        no = 3;
        break;
    }
    RsfSet(G_ROOM_ID, no);
    SceAtSetEnable(1, 0);
    SceAtSetEnable(0, 0);
}

// Background effect `no` fired by a target hit (emmark.cpp).
void R22cHitEffect(int no)
{
    if (r22c_work->effTimer != 0) {
        return;
    }
    switch (no) {
    case 0:
        if (r22c_work->effFlags & 0x10) {
            return;
        }
        r22c_work->effFlags |= 0x10;
        SceExec(0x12, (TaskFunc) r22c_BirdsFly, 0, 0, SCE_PRIO_DEF_2, 0);
        r22c_work->effTimer = 0x1E0;
        break;
    case 1:
        if (r22c_work->effFlags & 0x20) {
            return;
        }
        EstSet(0, -1, 0, 0, EFF_ROOM, 2, 0, ESP_CORE_KIND_MARK, 0, 0);
        r22c_work->effFlags |= 0x20;
        r22c_work->effTimer = 0x4B0;
        break;
    case 2:
        if (r22c_work->effFlags & 0x40) {
            return;
        }
        SceExec(0x12, (TaskFunc) r22c_BeeFly, 0, 0, SCE_PRIO_DEF_2, 0);
        r22c_work->effFlags |= 0x40;
        r22c_work->effTimer = 0x4B0;
        break;
    case 4:
        if (r22c_work->effFlags & 0x80) {
            return;
        }
        r22c_work->effFlags |= 0x80;
        SceExec(0x12, (TaskFunc) r22c_ShootingStar, 0, 0, SCE_PRIO_DEF_2, 0);
        r22c_work->effTimer = 0x4B0;
        break;
    case 3:
        if (r22c_work->effFlags & 4) {
            return;
        }
        SndCall(6, 0xC, 0, 0, 0, 0);
        r22c_work->effFlags |= 4;
        break;
    case 5:
        if (FlagChkSignW(r22c_work->effFlags, 31) && !(r22c_work->effFlags & 2)) {
            r22c_work->effFlags |= 2;
            SceExec(0x12, (TaskFunc) r22c_FireWorks, 0, 0, SCE_PRIO_DEF_2, 0);
            r22c_work->effTimer = 0x4B0;
        } else {
            if (r22c_work->effFlags & 8) {
                return;
            }
            SndCall(6, 0xD, 0, 0, 0, 0);
            r22c_work->effFlags |= 8;
        }
        break;
    case 6:
        if (r22c_work->effFlags & 1) {
            return;
        }
        r22c_work->effFlags |= 1;
        LightMgr.offKind(1);
        LightMgr.onKind(2);
        break;
    default:
        pLog->err(0, 0, "R22cHitEffect() INVALIED ID");
        break;
    }
}

// The language directory is patched into the path at run time.
static char r22c_fname[] = "SS/___/id22c.dat";

// Load the result screen file SS/<lang>/id22c.dat (blocking DVD read).
void ResultScreen::read()
{
    void* p;

    setLangExt3(r22c_fname + 3);
#line 3687 "D:/Bio4/Prog/r22c.cpp"
    Dvd.ReadCheck(DvdReadN(r22c_fname, 0, 0, 0, 0, 5, __FILE__, __LINE__), 0, 0, &p);
    data = (R22cResultData*) p;
}

// Show the "reload" text (id table 0x2C) with its textures during a script pause.
void ResultScreen::reloadtime()
{
    IdTexRelease(TEX_OWNER_ID_EVENT);
    IdTexDataLoad(RES_PTR(data, ofsTexReload), TEX_OWNER_ID_EVENT);
    IdSys.kill(0xFF, IDC_EVENT);
    IdSys.set(RES_PTR(data, ofsIdReload), 0xFF, IDC_EVENT, 0x13, 6, 0);
}

// Show the high-score variant of the result board with `score` split into seven digits.
void ResultScreen::highscore(int score)
{
    int digit[7];
    int i;
    register int pin asm("r28"); // COMPILER-DIFF: candidate #17 (see below)

    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    IdTexDataLoad(RES_PTR(data, ofsTexResult), TEX_OWNER_ID_TITLE);
    IdSys.set(RES_PTR(data, ofsIdHigh), 0xFF, IDC_TITLE, 0x13, 6, 0);
    for (i = 0; i < 7; i++) {
        digit[i] = score % 10;
        score /= 10;
    }
    // COMPILER-DIFF: candidate #17 -- the original allocates `score` to r28 although r31 is free,
    // i.e. r28 was in global.c's regs_used_so_far (pass 0) when the highest-priority pseudo was
    // allocated; nothing in the final code uses r28 there. Two codeless asms make r28 ever-live in
    // the gap where `score` is dead; the loop's IdSys high then falls to r31 in pass 1.
    asm("" : "=r"(pin));
    asm("" : : "r"(pin));
    score = 0;
    for (i = 6; i >= 0; i--) {
        IdUnit* u = IdSys.unitPtr(i + 1, IDC_TITLE);

        if (score == 0 && digit[i] == 0 && i != 0) {
            u->be_flag &= ~8;
        } else {
            score = 1;
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = digit[i];
        }
    }
}

// Show the result board (id table 0x28); SE 8 with a cap won, 0xA without.
void ResultScreen::init()
{
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    IdTexDataLoad(RES_PTR(data, ofsTexResult), TEX_OWNER_ID_TITLE);
    IdSys.set(RES_PTR(data, ofsIdResult), 0xFF, IDC_TITLE, 0x13, 6, 0);
    if (r22c_work->capId == 0xFFFF) {
        SndCall(6, 0xA, 0, 0, 0, 0);
    } else {
        SndCall(6, 8, 0, 0, 0, 0);
    }
}

// Per-frame result board: fills the digit units (score, hits, time), animates them in, waits for the
// button; returns 1 when the board is dismissed.
int ResultScreen::move(int flag)
{
    int ret = 0;
    IdUnit* u0;
    IdUnit* u3;
    IdUnit* u;
    int n;
    int digit[6];
    int i;
    int on;
    int sum;

    u0 = IdSys.unitPtr(0, IDC_TITLE);
    u3 = IdSys.unitPtr(3, IDC_TITLE);
    if (flag != 0) {
        u0->rev_flag |= 0xF;
        u3->rev_flag |= 0xF;
        state = 1;
    }
    if (state != 0) {
        if (state == 1) {
            if ((s16) u0->timer[3] <= 0) {
                ret = 1;
            }
        }
    }
    n = r22c_work->hits;
    u = IdSys.unitPtr(1, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = n % 10;
    n /= 10;
    u = IdSys.unitPtr(2, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = n % 10;
    n = r22c_work->total;
    u = IdSys.unitPtr(0x11, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = n % 10;
    n /= 10;
    u = IdSys.unitPtr(0x12, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = n % 10;
    n = r22c_work->score;
    for (i = 0; i < 6; i++) {
        digit[i] = n % 10;
        n /= 10;
    }
    on = 0;
    for (i = 5; i >= 0; i--) {
        u = IdSys.unitPtr(i + 0x21, IDC_TITLE);
        if (on == 0 && digit[i] == 0 && i != 0) {
            u->be_flag &= ~8;
        } else {
            on = 1;
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = digit[i];
        }
    }
    sum = 0;
    for (i = 0; i < 24; i++) {
        sum += r22c_work->cap[i];
    }
    n = sum;
    u = IdSys.unitPtr(0x31, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = n % 10;
    n /= 10;
    u = IdSys.unitPtr(0x32, IDC_TITLE);
    u->tex_flag |= 2;
    u->texNo = n % 10;
    switch (r22c_work->capId) {
    case 0xFFFF:
        IdSys.unitPtr(0xFD, IDC_TITLE)->be_flag &= ~8;
        IdSys.unitPtr(0xFE, IDC_TITLE)->be_flag &= ~8;
        break;
    case 0xE3:
        IdSys.unitPtr(0xFD, IDC_TITLE)->be_flag &= ~8;
        IdSys.unitPtr(0xFE, IDC_TITLE)->be_flag |= 8;
        break;
    default:
        IdSys.unitPtr(0xFD, IDC_TITLE)->be_flag |= 8;
        IdSys.unitPtr(0xFE, IDC_TITLE)->be_flag &= ~8;
        break;
    }
    return ret;
}

// Close the board: cockpit ids re-initialised and stepped once.
void ResultScreen::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

// The floating score numbers: their own IDSystem (0x80 units) and cleared timers.
void ScoreInit()
{
    r22c_work->score2.gameInit(0x80);
    ScoreClear();
}

// Kill all floating score numbers (timers zeroed, id system re-initialised).
void ScoreClear()
{
    u32 i;

    for (i = 0; i < 8; i++) {
        r22c_work->scoreTimer[i] = 0;
    }
    r22c_work->score2.roomInit();
}

// Per frame: count the eight floating score slots down and kill the expired ones (id 0x40 + slot); step / draw them.
void ScoreMove()
{
    int i;

    for (i = 0; i < 8; i++) {
        if (r22c_work->scoreTimer[i]) {
            r22c_work->scoreTimer[i]--;
        } else {
            r22c_work->scoreTimer[i] = 0;
        }
        if (r22c_work->scoreTimer[i] == 0) {
            r22c_work->score2.kill(0xFF, 0x40 + i);
        }
    }
    r22c_work->score2.move();
    r22c_work->score2.trans();
}

// Floating score number `pt` at world position `pos` (id table 0x40 + slot).
void ScoreSet(int pt, Vec* pos)
{
    int slot;
    int type;
    IdUnit* u;
    Vec scr;
    int digit[4];
    Vec v;
    int n;
    int i;
    int j;
    int* d;

    slot = -1;
    {
        int k;

        for (k = 0; k < 8; k++) {
            if (r22c_work->scoreTimer[k] == 0) {
                slot = k;
                break;
            }
        }
    }
    if (slot < 0) {
        return;
    }
    r22c_work->scoreTimer[slot] = 30;
    type = slot + 0x40;
    r22c_work->score2.set(ROOM_ARC_PTR(pG->pRoom, 0x21), 0xFF, type, 0x13, 6, 0);
    u = r22c_work->score2.unitPtr(0, type);
    v = *pos;
    GetScreenPos(&v, &scr);
    scr.x = (scr.x - 256.0f) * 1.25f;
    scr.y = (scr.y - 224.0f) * -1.0714285f;
    u->pos0 = scr;
    if (pt < 0) {
        IdUnit* m;

        pt = -pt;
        r22c_work->score2.unitPtr(0xFE, type)->be_flag &= ~8;
        m = r22c_work->score2.unitPtr(0xFD, type);
        u->col0[0] = m->col0[0];
        u->col0[1] = m->col0[1];
        u->col0[2] = m->col0[2];
        u->col0[3] = m->col0[3];
    } else {
        r22c_work->score2.unitPtr(0xFE, type)->be_flag |= 8;
    }
    d = digit;
    {
        int val = pt;
        int k;

        for (k = 0; k < 4; k++) {
            d[k] = val % 10;
            val /= 10;
        }
    }
    n = 0;
    j = 3;
    if (d[3] == 0) {
        n = 1;
        int* p = &d[3];
    NEXT:
        j--;
        if (j >= 0 && *--p == 0) {
            n++;
            goto NEXT;
        }
    }
    for (i = 3; i >= 0; i--) {
        IdUnit* du = r22c_work->score2.unitPtr(i + 1, type);

        if (i - n >= 0) {
            du->tex_flag = 2;
            du->texNo = d[i - n];
        } else {
            du->be_flag &= ~8;
        }
    }
}

// Task: the high-score board.
static void r22c_checkShootingScore()
{
    SceEventStart(0);
    SceAtSetEnable(7, 0);
    CamCtrl.CutCall(4);
    SceSleep(0x23);
    r22c_work->result.highscore(1000);
    while ((Joy[0].on & 0x300) == 0) {
        SceSleep(1);
    }
    r22c_work->result.quit();
    SceSleep(1);
    CamCtrl.Comeback(0);
    SceAtSetEnable(7, 1);
    SceEventEnd(0);
}

asm(".section .data\n\t.balign 8\n\t.text");
