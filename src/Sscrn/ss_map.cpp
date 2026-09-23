// Sscrn/ss_map: the map screen of the sub screen DLL (D:/Bio4/Prog/ss_map.cpp). Room models of
// the current area (SS/cmn/map_objNN.dat), door models, the player / partner / goal / merchant /
// treasure / coin / typewriter marks, the zoom camera and the mark mode menu.
#include "types.h"
#include "global.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"

// End-of-file order of the target: [cManager<cLight> copies] LightSetModel2, ~Widget<SUB_SCREEN>,
// ~cSat, ~SsMapInit, ... (the original queues the synthesized cSat destructor when it is
// synthesized, COMPILER-DIFF candidate #8; ours queues it when atari.h's class is finished). Both
// inlines are therefore instantiated BEFORE atari.h: LightSetModel2 (the module's second copy,
// its address is taken in mapModelDisp) and the ~Widget instantiation through `delete`. The
// .rodata vtable order is unaffected (Widget's vtable still follows SsMapInit's).
extern "C" inline void LightSetModel2(cModel* m)
{
    LightMgr.setModel2(m);
}

struct SUB_SCREEN;
// The `delete` that instantiates ~Widget<SUB_SCREEN> here (SsMapMain::quit deletes its widgets).
static inline void ssMapWidgetDelete(Widget<SUB_SCREEN>* w)
{
    delete w;
}

#include "atari.h"
#include "at_sub.h"
#include "item.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "main_sub.h"
#include "snd.h"
#include "db_log.h"
#include "camera.h"
#include "view.h"
#include "gx.h"
#include "trans_ot.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "room_data.h"
#include "stage.h"
#include "sscrn.h"

class cSubChar;

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// GX viewport of the map (also the screen viewport restored after it).
struct MapViewport {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
};

// The map work at SUB_SCREEN::pMapWk (mem_alloc, 0x104C bytes).
struct SsMapWork {
    cModel goal;         // 0x000  the mark models are constructed in place
    cModel merchant;     // 0x320
    cModel treasure;     // 0x640
    cModel coin;         // 0x960
    cModel save;         // 0xC80
    cModel* pGoal;       // 0xFA0
    cModel* pMerchant;   // 0xFA4
    cModel* pTreasure;   // 0xFA8
    cModel* pCoin;       // 0xFAC
    cModel* pSave;       // 0xFB0
    s8 nTreasure;        // 0xFB4  parts of the mark models = mark positions
    s8 nCoin;            // 0xFB5
    s8 nSave;            // 0xFB6
    u8 pad_FB7;
    CameraParam from;    // 0xFB8  zoom start
    CameraParam to;      // 0xFD8  zoom end
    s8 area;             // 0xFF8  getAreaNo
    s8 roomIdx;          // 0xFF9  map_room index of the current room (-1 none)
    s8 modeCursor;       // 0xFFA  mark mode menu cursor
    s8 modeSel;          // 0xFFB  0 entire, 1 read: widget the mode menu returns to
    MapViewport vp;      // 0xFFC
    f32 cx;              // 0x100C  map centre on screen
    f32 cy;              // 0x1010
    f32 sw;              // 0x1014  map size on screen
    f32 sh;              // 0x1018
    Mtx subMapMat;       // 0x101C  partner matrix on the map
};

// One room model of the area: room number and its map_objNN.dat sub-file.
struct MapRoomData {
    u16 room;
    u16 pad;
    void* bin;
};

// Uninitialised statics before ss_main.h: its externs of ssPlModel / ssWepModel (defined here) would
// otherwise put those two first in .bss (first-declaration order).
static MapViewport map_vp_save;
static int map_vp_init[1];  // one-element array: the in-struct store is ordered against the vp frame copy
static f32 map_cam_speed;
static int map_read_req;
// Non-static: the REL's ADDR16 fields for these hold A only (global symbols in the original).
MapRoomData map_room[48];
int map_room_num;

#include "ss_main.h"
#include <stdio.h>
#include "pl_npc.h"

// One door model of an area (5 packed bytes).
struct MapDoor {
    s8 parts;     // 0x0  parts of the room model the door hangs on (-1: whole model)
    u8 ang;       // 0x1  rotation (degrees)
    u8 flagType;  // 0x2  1: stage flag, 2: door unlock flag decides open / locked
    u8 flagNo;    // 0x3
    u8 item;      // 0x4  key item (0xFF none)
};

struct MapDoorTbl {
    MapDoor* p;
    u8 n;
    u8 pad[3];
};

// Room display flags (mapColor): stage flag numbers.
struct MapDispFlag {
    u16 room;
    u16 pad;
    u32 hide;
    u32 open;
    u32 clear;
};

// The mode menu / zoom widgets of the map screen (SsMapMain::init creates them).
class MapFocus : public Widget<SUB_SCREEN> {
public:
    virtual void move(SUB_SCREEN* wk);
};

class MapEntire : public Widget<SUB_SCREEN> {
public:
    MapEntire() : Widget<SUB_SCREEN>(2) {}
    virtual void move(SUB_SCREEN* wk);
};

class MapZoomIn : public Widget<SUB_SCREEN> {
public:
    s8 count;  // 0x10

    MapZoomIn() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class MapZoomOut : public Widget<SUB_SCREEN> {
public:
    s8 count;  // 0x10

    MapZoomOut() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

class MapRead : public Widget<SUB_SCREEN> {
public:
    MapRead() : Widget<SUB_SCREEN>(3) {}
    virtual void move(SUB_SCREEN* wk);
};

class MapModeSelect : public Widget<SUB_SCREEN> {
public:
    MapModeSelect() : Widget<SUB_SCREEN>(2) {}
    virtual void init(SUB_SCREEN* wk);
    virtual void quit(SUB_SCREEN* wk);
    virtual void move(SUB_SCREEN* wk);
};

extern "C" {
int getStageNo();
int getAreaNo(u32 room);
void mapInitViewport(SUB_SCREEN* wk);
void mapChangeViewport(SUB_SCREEN* wk);
void stageNameDisp(SUB_SCREEN* wk);
void markCharDisp(IdUnit* u, Mtx m);
void markPlayerDisp(SUB_SCREEN* wk, int sw);
void markGoalInit(SUB_SCREEN* wk);
void markGoalQuit(SUB_SCREEN* wk);
int markGoalPosition(SUB_SCREEN* wk, Vec* pos);
void markGoalDisp(SUB_SCREEN* wk, int sw);
void markMerchantInit(SUB_SCREEN* wk);
void markMerchantQuit(SUB_SCREEN* wk);
int markMerchantPosition(SUB_SCREEN* wk, int no, Vec* pos);
int getMerchantMarkNo(int no);
void markMerchantDisp(SUB_SCREEN* wk, int sw);
void markTreasureInit(SUB_SCREEN* wk);
void markTreasureQuit(SUB_SCREEN* wk);
int markTreasurePosition(SUB_SCREEN* wk, int no, Vec* pos);
int markTreasureExist(int no);
void markTreasureDisp(SUB_SCREEN* wk, int sw);
void markCoinInit(SUB_SCREEN* wk);
void markCoinQuit(SUB_SCREEN* wk);
int markCoinPosition(SUB_SCREEN* wk, int no, Vec* pos);
int markCoinExist(int stage, int no);
void markCoinDisp(SUB_SCREEN* wk, int sw);
void markSaveInit(SUB_SCREEN* wk);
void markSaveQuit(SUB_SCREEN* wk);
int markSavePosition(SUB_SCREEN* wk, int no, Vec* pos);
void markSaveDisp(SUB_SCREEN* wk, int sw);
int mapPos2screenPos(Vec* pos, Vec* out);
void mapPositionCheck(cSatHeader* hdrB, cSatHeader* hdrA, Mtx plMat, Mtx partsMat, Mtx out, int multi);
MapDispFlag* searchMapDispFlag(u16 room, MapDispFlag* tbl, int n);
int mapColor(u16 room);
int mapRoomNum(MapRoomData* p);
void* mapBinAddr(MapRoomData* p, int no);
cSatHeader* mapHitAddr(MapRoomData* p, int no);
int mapDataInit_St1(SUB_SCREEN* wk);
int mapDataInit_St2A(SUB_SCREEN* wk);
int mapDataInit_St2B(SUB_SCREEN* wk);
int mapDataInit_St2C(SUB_SCREEN* wk);
int mapDataInit_St3A(SUB_SCREEN* wk);
int mapDataInit_St3B(SUB_SCREEN* wk);
int mapDataInit_St3C(SUB_SCREEN* wk);
int mapDataInit_St3D(SUB_SCREEN* wk);
int mapDataInit_St3E(SUB_SCREEN* wk);
int mapDataInit_St3F(SUB_SCREEN* wk);
int mapDataInit_St4A(SUB_SCREEN* wk);
int mapDataInit_St4B(SUB_SCREEN* wk);
int mapDataInit_St4C(SUB_SCREEN* wk);
int mapDataInit_St4D(SUB_SCREEN* wk);
int mapDataInit_St4E(SUB_SCREEN* wk);
int mapDataInit_St4F(SUB_SCREEN* wk);
int mapDataInit_St4G(SUB_SCREEN* wk);
void mapTblInit(SUB_SCREEN* wk);
void mapModelAlloc(SUB_SCREEN* wk);
void mapModelInit(SUB_SCREEN* wk);
void mapModelDisp(SUB_SCREEN* wk);
void doorModelInit(SUB_SCREEN* wk);
void doorModelDisp(SUB_SCREEN* wk);
void mapCameraInit(SUB_SCREEN* wk, Camera* cam);
void mapCameraMove(SUB_SCREEN* wk);
f32 zoomOutLimit();
void mapCameraEntire(SUB_SCREEN* wk, CameraParam* out);
f32 zoomInLimit();
void mapCameraZoomIn(SUB_SCREEN* wk, CameraParam* out);
int zoomMove(SsMapWork* m, int max, int cnt);
void mapAreaFilename(int area, char* name);
int scf_check_merchant();
int scf_check_treasure();
int scf_check_submission();
int scf_check_typewriter();
void sscrn_map_out_init(SUB_SCREEN* wk);
int mapModeCheck(SUB_SCREEN* wk, s8 no);
void mapModeChange(SUB_SCREEN* wk, s8 no);
}

static int sscrn_map_out(SUB_SCREEN* wk);
static void setViewport(MapViewport* vp);

// Door models per area (index: getAreaNo).
MapDoor map_door_none[1] = {
    {-1, 0, 0, 0, 0xFF},
};
MapDoor map_door_st1[25] = {
    {-1, 0, 0, 0, 0xFF},  {0, 0, 1, 144, 0xFF},   {1, 90, 0, 0, 0xFF},   {2, 105, 2, 12, 0x8B},
    {3, 90, 2, 15, 0xFF}, {4, 64, 2, 2, 0x3B},    {5, 0, 0, 0, 0xFF},    {6, 124, 1, 150, 0xFF},
    {7, 110, 2, 3, 0x3C}, {8, 90, 1, 152, 0xFF},  {9, 164, 0, 0, 0xFF},  {10, 114, 1, 154, 0xFF},
    {11, 90, 2, 4, 0xFF}, {12, 43, 1, 155, 0xFF}, {13, 100, 0, 0, 0xFF}, {14, 90, 1, 157, 0xFF},
    {15, 90, 0, 0, 0xFF}, {16, 0, 0, 0, 0xFF},    {17, 0, 0, 0, 0xFF},   {18, 105, 2, 9, 0xA6},
    {19, 90, 2, 8, 0x3D}, {20, 90, 2, 11, 0x8C},  {21, 90, 1, 163, 0xFF}, {22, 90, 1, 163, 0xFF},
    {23, 110, 1, 159, 0xFF},
};
MapDoor map_door_st2a[47] = {
    {-1, 0, 0, 0, 0xFF},    {0, 0, 0, 0, 0xFF},     {1, 0, 0, 0, 0xFF},     {2, 0, 1, 128, 0xFF},
    {3, 0, 0, 0, 0xFF},     {4, 0, 0, 0, 0xFF},     {5, 0, 1, 128, 0xFF},   {6, 90, 0, 0, 0xFF},
    {7, 90, 1, 130, 0xFF},  {8, 90, 1, 127, 0xFF},  {9, 90, 0, 0, 0xFF},    {10, 0, 0, 0, 0xFF},
    {11, 135, 0, 0, 0xFF},  {12, 0, 0, 116, 0xFF},  {13, 0, 0, 0, 0xFF},    {14, 90, 2, 10, 0xFF},
    {15, 90, 0, 0, 0xFF},   {16, 90, 1, 127, 0xFF}, {17, 90, 0, 0, 0xFF},   {18, 90, 0, 0, 0xFF},
    {19, 0, 2, 14, 0xA7},   {20, 90, 0, 0, 0xFF},   {21, 0, 0, 0, 0xFF},    {22, 0, 0, 0, 0xFF},
    {23, 90, 0, 0, 0xFF},   {24, 90, 0, 0, 0xFF},   {25, 0, 1, 123, 0xFF},  {26, 90, 0, 0, 0xFF},
    {27, 0, 0, 0, 0xFF},    {28, 90, 0, 0, 0xFF},   {29, 90, 1, 122, 0xFF}, {30, 90, 0, 0, 0xFF},
    {31, 90, 1, 117, 0xFF}, {32, 0, 1, 112, 0xFF},  {33, 0, 1, 114, 0xFF},  {34, 0, 1, 129, 0xC3},
    {35, 90, 1, 140, 0xFF}, {36, 90, 1, 115, 0xA3}, {37, 90, 0, 0, 0xFF},   {38, 0, 1, 141, 0xFF},
    {39, 45, 1, 117, 0x7A}, {40, 90, 1, 125, 0xFF}, {41, 90, 1, 124, 0xFF}, {42, 0, 0, 0, 0xFF},
    {43, 90, 1, 139, 0xFF}, {44, 0, 0, 0, 0xFF},    {45, 90, 0, 0, 0xFF},
};
MapDoor map_door_st2b[12] = {
    {-1, 0, 0, 0, 0xFF},   {0, 90, 1, 131, 0xFF}, {1, 90, 0, 0, 0xFF},    {2, 0, 1, 132, 0xFF},
    {3, 90, 0, 0, 0xFF},   {4, 90, 1, 133, 0x82}, {5, 90, 0, 0, 0xFF},    {6, 90, 1, 137, 0xFF},
    {7, 67, 0, 0, 0xFF},   {8, 67, 1, 136, 0xFF}, {9, 109, 1, 137, 0xFF}, {10, 0, 1, 135, 0xFF},
};
MapDoor map_door_st2c[3] = {
    {-1, 0, 0, 0, 0xFF}, {0, 0, 1, 134, 0x7B}, {1, 90, 0, 0, 0xFF},
};
MapDoor map_door_st3a[2] = {
    {-1, 0, 0, 0, 0xFF}, {0, 90, 2, 24, 0xFF},
};
MapDoor map_door_st3b[7] = {
    {-1, 0, 0, 0, 0xFF}, {0, 0, 0, 0, 0xFF},  {1, 0, 0, 0, 0xFF},   {2, 90, 2, 20, 0xFF},
    {3, 0, 0, 0, 0xFF},  {4, 90, 0, 0, 0xFF}, {5, 90, 2, 25, 0xFF},
};
MapDoor map_door_st3c[11] = {
    {-1, 0, 0, 0, 0xFF},  {0, 90, 0, 0, 0xFF},  {1, 90, 0, 0, 0xFF},  {2, 0, 0, 0, 0xFF},
    {3, 0, 2, 18, 0xFF},  {4, 0, 2, 23, 0xFF},  {5, 90, 0, 0, 0xFF},  {6, 0, 0, 0, 0xFF},
    {7, 0, 2, 31, 0xFF},  {8, 90, 2, 30, 0xFF}, {9, 90, 2, 19, 0xFF},
};
MapDoor map_door_st3d[6] = {
    {-1, 0, 0, 0, 0xFF}, {0, 90, 2, 34, 0xFF}, {1, 90, 0, 0, 0xFF}, {2, 0, 2, 32, 0xFF},
    {3, 90, 0, 0, 0xFF}, {4, 0, 2, 33, 0xFF},
};
MapDoor map_door_st3e[26] = {
    {-1, 0, 0, 0, 0xFF},   {0, 45, 2, 42, 0xFF},  {1, 0, 2, 45, 0xFF},    {2, 0, 0, 0, 0xFF},
    {3, 90, 0, 0, 0xFF},   {4, 90, 0, 0, 0xFF},   {5, 0, 0, 0, 0xFF},     {6, 90, 2, 40, 0xFF},
    {7, 60, 0, 0, 0xFF},   {8, 90, 0, 0, 0xFF},   {9, 0, 0, 0, 0xFF},     {10, 0, 0, 0, 0xFF},
    {11, 0, 0, 0, 0xFF},   {11, 0, 0, 0, 0xFF},   {12, 90, 2, 35, 0xFF},  {13, 90, 2, 36, 0xFF},
    {14, 90, 2, 37, 0xFF}, {15, 135, 2, 38, 0xFF}, {16, 51, 2, 39, 0xFF}, {17, 0, 2, 46, 0xFF},
    {18, 90, 2, 51, 0xFF}, {19, 0, 2, 47, 0xFF},  {20, 21, 2, 44, 0xFF},  {21, 83, 2, 43, 0xFF},
    {22, 0, 2, 41, 0xFF},  {23, 90, 0, 0, 0xFF},
};
MapDoor map_door_st3f[7] = {
    {-1, 0, 0, 0, 0xFF}, {0, 0, 0, 0, 0xFF},   {1, 135, 0, 0, 0xFF}, {2, 90, 0, 0, 0xFF},
    {3, 90, 0, 0, 0xFF}, {4, 90, 2, 48, 0xFF}, {5, 90, 2, 50, 0xFF},
};
MapDoor map_door_st4g[8] = {
    {-1, 0, 0, 0, 0xFF}, {0, 0, 0, 0, 0xFF}, {1, 90, 0, 0, 0xFF}, {2, 90, 0, 0, 0xFF},
    {3, 0, 0, 0, 0xFF},  {4, 0, 0, 0, 0xFF}, {5, 90, 0, 0, 0xFF}, {6, 0, 0, 0, 0xFF},
};
MapDoorTbl map_door_tbl[18] = {
    {map_door_none, 1},  {map_door_st1, 25}, {map_door_st2a, 47}, {map_door_st2b, 12},
    {map_door_st2c, 3},  {map_door_st3a, 2}, {map_door_st3b, 7},  {map_door_st3c, 11},
    {map_door_st3d, 6},  {map_door_st3e, 26}, {map_door_st3f, 7}, {0, 0},
    {0, 0},              {0, 0},             {0, 0},              {0, 0},
    {0, 0},              {map_door_st4g, 8},
};

static u8 treasure_mark_num = 14;
static u8 coin_mark_num = 15;
static u8 save_mark_num = 12;
static f32 map_dbg_ofs0 = 1500.0f;
static f32 map_dbg_cam0[8] = {-1500.0f, -11180.0f, 39640.0f, 35.0f, -11180.0f, 0.0f, 35.0f, 55.0f};
static f32 map_cam_speed_base = 150.0f;
static f32 map_dbg_cam1[8] = {1000.0f, -13205.0f, 35000.0f, 1150.0f, -13205.0f, 0.0f, 1150.0f, 55.0f};
static int map_wait[1] = {0};  // one-element array (SsFileInit idiom): the case-0 store keeps two pseudos in its block
cModel* ssPlModel;
cModel* ssWepModel;
cModel* ssPlMotion = 0;
cModel* ssWepModel2 = 0;

// Whole-map camera per stage.
static const CameraParam map_cam_entire[4] = {
    {{0.0f, 10000.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 55.0f},
    {{21000.0f, 82850.0f, 12650.0f}, {21000.0f, 0.0f, 12650.0f}, 0.0f, 55.0f},
    {{-425.0f, 104000.0f, -3210.0f}, {-425.0f, 0.0f, -3210.0f}, 0.0f, 55.0f},
    {{-425.0f, 104000.0f, -3210.0f}, {-425.0f, 0.0f, -3210.0f}, 0.0f, 55.0f},
};

// Mark models per area: goal, merchant, treasure, coin, typewriter.
static const int mark_model_tbl[18][5] = {
    {0, 0, 0, 0, 0}, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {1, 0, 1, 0, 0}, {1, 0, 0, 0, 0},
    {1, 1, 1, 1, 1}, {1, 1, 0, 0, 1}, {1, 1, 0, 0, 1}, {1, 1, 1, 1, 1}, {1, 1, 0, 0, 1}, {0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0},
};

// Stage flag / door unlock flag / item flag bit tests (one bit per number, word tables in pG), the
// word address written index first (`(no >> 5) * 4 + table`: `add rIdx, rTbl` / `lwzx rIdx, rTbl`).
static inline u32 flagBit(u32 tbl, u32 no)
{
    return *(u32*) ((no >> 5) * 4 + tbl) & (0x80000000 >> (no & 0x1F));
}

// Map stage of the current progress: 4 the island (stage_no 4), 3 / 2 by the Scenario_flg chapter
// bits (castle / village-end), 1 the village once Scenario_flg[0] bit 2 is set, else 0 (prologue).
int getStageNo()
{
    if (pG->stage_no == 4) {
        return 4;
    }
    if (ScfFlagChk(pG, SCF_ST3_IN)) {
        return 3;
    }
    if (ScfFlagChk(pG, SCF_ST2_IN)) {
        return 2;
    }
    if (ScfFlagChk(pG, SCF_ST1_MAP_DAY)) {
        return 1;
    }
    return 0;
}

// Map area 0..17 of `room` within getStageNo(): 0/1 village, 2..4 the castle parts (2a/2b/2c),
// 5..10 the island parts (3a..3f), 11..17 the stage 4 rooms; the map area picks the
// SS/cmn/map_objNN.dat file (mapAreaFilename) and every per-area table.
int getAreaNo(u32 room)
{
    switch (getStageNo()) {
    case 0:
    default:
        return 0;
    case 1:
        return 1;
    case 2:
        switch (room) {
        case 0x200 ... 0x219:
        case 0x222:
            return 2;
        case 0x21D:
        case 0x220 ... 0x221:
        case 0x223 ... 0x22A:
            return 3;
        case 0x21A ... 0x21B:
            return 4;
        default:
            return 0;
        }
    case 3:
        switch (room) {
        case 0x300:
            return 5;
        case 0x301:
        case 0x303 ... 0x306:
            return 6;
        case 0x307 ... 0x30C:
        case 0x30E:
            return 7;
        case 0x30D:
        case 0x30F ... 0x312:
            return 8;
        case 0x315 ... 0x318:
        case 0x31A ... 0x31D:
        case 0x320 ... 0x321:
        case 0x325 ... 0x327:
            return 9;
        case 0x329:
        case 0x330 ... 0x333:
            return 10;
        default:
            return 0;
        }
    case 4:
        switch (room) {
        case 0x400:
            return 11;
        case 0x402:
            return 12;
        case 0x403:
            return 13;
        case 0x404:
            return 14;
        case 0x405:
            return 15;
        case 0x406:
            return 16;
        default:
            return 17;
        }
    }
}

// Derives the map viewport from the frame unit (IdSub 0xFE/0x10): a 4:3 box of the frame size at
// its screen position (SsMapWork cx/cy/sw/sh and the GX viewport in 640x448 field coordinates).
void mapInitViewport(SUB_SCREEN* wk)
{
    IdUnit* u = IdSub.unitPtr(0xFE, IDC_SSCRN_NEAR_0);
    f32 sx = fabsf(u->size_W);
    f32 sy = fabsf(u->size_H);
    Vec p;

    if (sy <= sx * 0.75f) {
        sy = sx * 0.75f;
    } else {
        sx = sy / 0.75f;
    }
    wk->map->cx = u->pos0.x;
    wk->map->cy = u->pos0.y;
    wk->map->sw = sx;
    wk->map->sh = sy;
    p.x = u->pos0.x;
    p.y = -u->pos0.y;
    wk->map->vp.x = p.x - sx * 0.5f + 320.0f;
    wk->map->vp.y = (p.y - sy * 0.5f + 240.0f) * 448.0f / 480.0f;
    wk->map->vp.w = sx;
    wk->map->vp.h = sy * 448.0f / 480.0f;
}

// Per frame: queues the map viewport before OT 9 (the room models) and the full screen viewport
// after OT 0xC; the screen viewport is captured once from Screen.
void mapChangeViewport(SUB_SCREEN* wk)
{
    if (!map_vp_init[0]) {
        MapViewport vp;

        vp.x = 0.0f;
        vp.y = 0.0f;
        map_vp_init[0] = 1;
        vp.w = Screen.width;
        vp.h = Screen.height;
        map_vp_save = vp;
    }
    AddOtDirect(9, &wk->map->vp, (void (*)()) setViewport, 0, 0x1000, 0, 0.0f);
    AddOtDirect(0xC, &map_vp_save, (void (*)()) setViewport, 7, 0x1000, 0, 0.0f);
}

// OT callback: GXSetViewport from a MapViewport.
static void setViewport(MapViewport* vp)
{
    GXSetViewport(vp->x, vp->y, vp->w, vp->h, 0.0f, 1.0f);
}

// Shows the stage title unit (IdSub 0x31..0x33 of 0x10) for wk->stage 1..3.
void stageNameDisp(SUB_SCREEN* wk)
{
    u8 id;

    IdSub.unitPtr(0x31, IDC_SSCRN_NEAR_0)->be_flag &= ~8;
    IdSub.unitPtr(0x32, IDC_SSCRN_NEAR_0)->be_flag &= ~8;
    IdSub.unitPtr(0x33, IDC_SSCRN_NEAR_0)->be_flag &= ~8;
    switch ((s8) wk->stage_no) {
    case 1:
        id = 0x31;
        break;
    case 2:
        id = 0x32;
        break;
    case 3:
        id = 0x33;
        break;
    default:
        id = 0x31;
        break;
    }
    IdSub.unitPtr(id, IDC_SSCRN_NEAR_0)->be_flag |= 8;
}

// Places a character mark unit at the map screen position of matrix `m`'s translation, rotated to
// its facing (z axis, degrees); skipped when the position projects behind the camera.
void markCharDisp(IdUnit* u, Mtx m)
{
    Vec pos;
    Vec scr;
    Vec dir;
    f32 ang;

    pos.x = m[0][3];
    pos.y = m[1][3];
    pos.z = m[2][3];
    dir.x = m[0][2];
    dir.y = m[1][2];
    dir.z = m[2][2];
    ang = atan2f(dir.x, dir.z);
    if (mapPos2screenPos(&pos, &scr)) {
        u->pos0 = scr;
        u->rot0.z = ang * 180.0f / 3.1415927f + 180.0f;
    }
}

// Shows (sw) or hides the player mark (IdSub 0/0x14, pl_mat_map) and the partner mark (1/0x14,
// subMapMat, only while a partner exists).
void markPlayerDisp(SUB_SCREEN* wk, int sw)
{
    IdUnit* u;

    if (!sw) {
        u = IdSub.unitPtr(0, IDC_SSCRN_0);
        u->be_flag &= ~8;
        u = IdSub.unitPtr(1, IDC_SSCRN_0);
        u->be_flag &= ~8;
    } else {
        u = IdSub.unitPtr(0, IDC_SSCRN_0);
        markCharDisp(u, wk->pl_mat_map);
        u->be_flag |= 8;
        u = IdSub.unitPtr(1, IDC_SSCRN_0);
        if (pSUB) {
            markCharDisp(u, wk->map->subMapMat);
            u->be_flag |= 8;
        }
    }
}

// Constructs the goal mark model in the map work: the area file's goal parts (sub-file 6, one
// parts per possible goal) or the common single mark; its parts positions are the goal candidates.
void markGoalInit(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    cModel* mdl;

    m->pGoal = new (&m->goal) cModel();
    if (mark_model_tbl[m->area][0]) {
        m->pGoal->modelInit(SS_ARC_PTR(wk->pMapObj, 6), SS_ARC_PTR(wk->pMapDat, 0x10));
    } else {
        m->pGoal->modelInit(SS_ARC_PTR(wk->pMapDat, 0xF), SS_ARC_PTR(wk->pMapDat, 0x10));
    }
    mdl = m->pGoal;
    RotMatrix(mdl->l_mat, &mdl->ang);
    TransMatrix(mdl->l_mat, &mdl->pos);
    ScaleMatrix(mdl->l_mat, &mdl->scale);
    PSMTXCopy(mdl->l_mat, mdl->mat);
    if (mdl->pList) {
        mdl->partsMatCalc();
        mdl->partsWorldCalc();
    }
    mdl->be_flag &= ~2;
}

// Destroys the goal mark model.
void markGoalQuit(SUB_SCREEN* wk)
{
    cModel* mdl = wk->map->pGoal;

    if (mdl) {
        delete mdl;
    }
}

// Current goal position: the parts of the goal model whose stage flag (per-area table, the highest
// set flag wins) is on, parts 0 when none. Always returns 1.
int markGoalPosition(SUB_SCREEN* wk, Vec* pos)
{
    int none[1] = {0};
    int st1[10] = {0, 0x11, 0x12, 0x13, 0x14, 0x15, 0x2C, 0x17, 0x0B, 0x2E};
    int st2a[6] = {0x28, 0x0F, 0x2D, 0x44, 0x23, 0x45};
    int st2b[3] = {0x46, 0x47, 0x48};
    int st3a[2] = {0, 0x49};
    int st3b[3] = {0, 0x4A, 0x4B};
    int st3c[4] = {0, 0x4C, 0x4D, 0x4E};
    int st3d[1] = {0};
    int st3e[6] = {0, 0x4F, 0x50, 0x51, 0x52, 0x53};
    int st3f[3] = {0, 0x54, 0x55};
    int st4e[1] = {0};
    int st4f[1] = {0};
    int st4g[1] = {0};
    SsMapWork* m = wk->map;
    int* tbl;
    int n;
    int no;
    int i;
    cParts* p;

    switch (m->area) {
    case 1:
        tbl = st1;
        n = 10;
        break;
    case 2:
        tbl = st2a;
        n = 6;
        break;
    case 3:
        tbl = st2b;
        n = 3;
        break;
    case 5:
        tbl = st3a;
        n = 2;
        break;
    case 6:
        tbl = st3b;
        n = 3;
        break;
    case 7:
        tbl = st3c;
        n = 4;
        break;
    case 8:
        tbl = st3d;
        n = 1;
        break;
    case 9:
        tbl = st3e;
        n = 6;
        break;
    case 10:
        tbl = st3f;
        n = 3;
        break;
    case 15:
        tbl = st4e;
        n = 1;
        break;
    case 16:
        tbl = st4f;
        n = 1;
        break;
    case 17:
        tbl = st4g;
        n = 1;
        break;
    default:
        tbl = none;
        n = 1;
        break;
    }
    no = 0;
    for (i = 1; i < n; i++) {
        if (FlagChkVar((u32) &pG->Scenario_flg[0], (u32) tbl[i])) {
            no = i;
        }
    }
    p = m->pGoal->getPartsPtr(no);
    *pos = p->pos;
    return 1;
}

// Shows (sw) the goal mark (IdSub 3/0x14) at the projected goal position, or hides it.
void markGoalDisp(SUB_SCREEN* wk, int sw)
{
    IdUnit* u = IdSub.unitPtr(3, IDC_SSCRN_0);
    Vec pos;
    Vec scr;

    if (!sw) {
        u->be_flag &= ~8;
    } else {
        u->be_flag &= ~8;
        if (markGoalPosition(wk, &pos)) {
            if (mapPos2screenPos(&pos, &scr)) {
                u->pos0 = scr;
                u->be_flag |= 8;
            }
        }
    }
}

// Constructs the merchant mark model (area file sub-file 7 or the common mark) whose parts are the
// merchant positions.
void markMerchantInit(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    cModel* mdl;

    m->pMerchant = new (&m->merchant) cModel();
    if (mark_model_tbl[m->area][1]) {
        m->pMerchant->modelInit(SS_ARC_PTR(wk->pMapObj, 7), SS_ARC_PTR(wk->pMapDat, 0x10));
    } else {
        m->pMerchant->modelInit(SS_ARC_PTR(wk->pMapDat, 0xF), SS_ARC_PTR(wk->pMapDat, 0x10));
    }
    mdl = m->pMerchant;
    RotMatrix(mdl->l_mat, &mdl->ang);
    TransMatrix(mdl->l_mat, &mdl->pos);
    ScaleMatrix(mdl->l_mat, &mdl->scale);
    PSMTXCopy(mdl->l_mat, mdl->mat);
    if (mdl->pList) {
        mdl->partsMatCalc();
        mdl->partsWorldCalc();
    }
    mdl->be_flag &= ~2;
}

// Destroys the merchant mark model.
void markMerchantQuit(SUB_SCREEN* wk)
{
    cModel* mdl = wk->map->pMerchant;

    if (mdl) {
        delete mdl;
    }
}

// Merchant mark `no` of the area: stage 1 picks the parts by the stage flags, the others use the
// parts in order.
int markMerchantPosition(SUB_SCREEN* wk, int no, Vec* pos)
{
    int st1a[2] = {0x22, 0};
    int st1b[6] = {0x22, 5, 0x0B, 1, 0x19, 2};
    int st1c[6] = {0x22, 6, 0x1C, 4, 0x0B, 3};
    int st1d[2] = {0x0B, 7};
    SsMapWork* m = wk->map;
    cParts* p;
    int ret;

    if (m->area == 1) {
        int n;
        int* tbl;
        int idx;
        int i;

        switch (no) {
        case 0:
            n = 1;
            tbl = st1a;
            break;
        case 1:
            n = 3;
            tbl = st1b;
            break;
        case 2:
            n = 3;
            tbl = st1c;
            break;
        case 3:
            n = 1;
            tbl = st1d;
            break;
        default:
            n = 0;
            tbl = 0;
            break;
        }
        idx = -1;
        for (i = 0; i < n; i++) {
            if (FlagChkVar((u32) &pG->Scenario_flg[0], (u32) (tbl[i * 2]))) {
                idx = i;
            }
        }
        if (idx != -1) {
            p = m->pMerchant->getPartsPtr(tbl[idx * 2 + 1]);
            *pos = p->pos;
            return 1;
        } else {
            return 0;
        }
    } else {
        cModel* mdl = m->pMerchant;

        if (no < mdl->nParts) {
            p = mdl->getPartsPtr(no);
            *pos = p->pos;
            return 1;
        }
        return 0;
    }
}

// IdSub unit (group 0x14) of merchant mark `no` 0..6: 2, then 5..0xA.
int getMerchantMarkNo(int no)
{
    int id = 2;

    switch (no) {
    case 0:
        id = 2;
        break;
    case 1:
        id = 5;
        break;
    case 2:
        id = 6;
        break;
    case 3:
        id = 7;
        break;
    case 4:
        id = 8;
        break;
    case 5:
        id = 9;
        break;
    case 6:
        id = 0xA;
        break;
    }
    return id;
}

// Shows (sw) the up to seven merchant marks at their projected positions, or hides them all.
void markMerchantDisp(SUB_SCREEN* wk, int sw)
{
    IdUnit* u;

    if (!sw) {
        int i;

        for (i = 0; i < 7; i++) {
            u = IdSub.unitPtr(getMerchantMarkNo(i), IDC_SSCRN_0);
            u->be_flag &= ~8;
        }
    } else {
        Vec pos[7];
        Vec scr;
        int i;

        for (i = 0; i < 7; i++) {
            u = IdSub.unitPtr(getMerchantMarkNo(i), IDC_SSCRN_0);
            if (markMerchantPosition(wk, i, &pos[i]) && mapPos2screenPos(&pos[i], &scr)) {
                u->pos0 = scr;
                u->be_flag |= 8;
            } else {
                u->be_flag &= ~8;
            }
        }
    }
}

// Constructs the treasure mark model (area sub-file 8 or the common mark); nTreasure = its parts.
void markTreasureInit(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    cModel* mdl;

    m->pTreasure = new (&m->treasure) cModel();
    if (mark_model_tbl[m->area][2]) {
        m->pTreasure->modelInit(SS_ARC_PTR(wk->pMapObj, 8), SS_ARC_PTR(wk->pMapDat, 0x10));
    } else {
        m->pTreasure->modelInit(SS_ARC_PTR(wk->pMapDat, 0xF), SS_ARC_PTR(wk->pMapDat, 0x10));
    }
    mdl = m->pTreasure;
    RotMatrix(mdl->l_mat, &mdl->ang);
    TransMatrix(mdl->l_mat, &mdl->pos);
    ScaleMatrix(mdl->l_mat, &mdl->scale);
    PSMTXCopy(mdl->l_mat, mdl->mat);
    if (mdl->pList) {
        mdl->partsMatCalc();
        mdl->partsWorldCalc();
    }
    mdl->be_flag &= ~2;
    m->nTreasure = mdl->nParts;
}

// Destroys the treasure mark model.
void markTreasureQuit(SUB_SCREEN* wk)
{
    cModel* mdl = wk->map->pTreasure;

    if (mdl) {
        delete mdl;
    }
}

// Position of treasure mark `no` (parts `no` of the treasure model).
int markTreasurePosition(SUB_SCREEN* wk, int no, Vec* pos)
{
    cParts* p = wk->map->pTreasure->getPartsPtr(no);

    *pos = p->pos;
    return 1;
}

// 1 while treasure `no` of the area is still in place (item flag not set).
int markTreasureExist(int no)
{
    u8 st1[14] = {0x0E, 0x06, 0x07, 0x05, 0x01, 0x0D, 0x10, 0x04, 0x08, 0x0F, 0x15, 0x16, 0x17, 0x18};
    u8 st2a[12] = {0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30};
    u8 st2b[3] = {0x31, 0x32, 0x33};
    u8 st2c[1] = {0x34};
    u8 st3b[3] = {0x35, 0x36, 0x37};
    u8 st3e[4] = {0x38, 0x39, 0x3A, 0x3B};
    u8 st4a[5] = {0x3C, 0x3D, 0x3E, 0x3F, 0x40};
    u8 st4b[6] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    u8 st4c[12] = {0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52};
    u8 st4d[12] = {0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E};

    switch (SubScreenWk.map->area) {
    case 1:
        return FlagChkVar((u32) pG->Item_flg, st1[no]) == 0;
    case 2:
        return FlagChkVar((u32) pG->Item_flg, st2a[no]) == 0;
    case 3:
        return FlagChkVar((u32) pG->Item_flg, st2b[no]) == 0;
    case 4:
        return flagBit((u32) pG->Item_flg, st2c[no]) == 0;
    case 6:
        return FlagChkVar((u32) pG->Item_flg, st3b[no]) == 0;
    case 9:
        return FlagChkVar((u32) pG->Item_flg, st3e[no]) == 0;
    case 11:
        return FlagChkVar((u32) pG->Item_flg, st4a[no]) == 0;
    case 12:
        return FlagChkVar((u32) pG->Item_flg, st4b[no]) == 0;
    case 13:
        return FlagChkVar((u32) pG->Item_flg, st4c[no]) == 0;
    case 14:
        return FlagChkVar((u32) pG->Item_flg, st4d[no]) == 0;
    }
    return 0;
}

// Shows (sw) the treasure marks (IdNum i/0x15) at their positions with the "taken" overlay
// (i + 0x10) on collected ones, or hides them.
void markTreasureDisp(SUB_SCREEN* wk, int sw)
{
    SsMapWork* m = wk->map;
    IdUnit* u;
    IdUnit* u2;

    if (!sw) {
        for (int i = 0; i < treasure_mark_num; i++) {
            u = IdNum.unitPtr(i, IDC_SSCRN_1);
            u2 = IdNum.unitPtr(i + 0x10, IDC_SSCRN_1);
            u->be_flag &= ~8;
            u2->be_flag &= ~8;
        }
    } else {
        Vec pos;
        Vec scr;

        for (int i = 0; i < treasure_mark_num; i++) {
            u = IdNum.unitPtr(i, IDC_SSCRN_1);
            u2 = IdNum.unitPtr(i + 0x10, IDC_SSCRN_1);
            if (i < m->nTreasure) {
                markTreasurePosition(wk, i, &pos);
                if (mapPos2screenPos(&pos, &scr)) {
                    u->pos0 = scr;
                    if (markTreasureExist(i)) {
                        u->be_flag |= 8;
                        u2->be_flag &= ~8;
                    } else {
                        u->be_flag |= 8;
                        u2->be_flag |= 8;
                    }
                } else {
                    u->be_flag &= ~8;
                    u2->be_flag &= ~8;
                }
            } else {
                u->be_flag &= ~8;
                u2->be_flag &= ~8;
            }
        }
    }
}

// Constructs the blue medallion (sub-mission target) mark model (area sub-file 9 or the common
// mark); nCoin = its parts.
void markCoinInit(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    cModel* mdl;

    m->pCoin = new (&m->coin) cModel();
    if (mark_model_tbl[m->area][3]) {
        m->pCoin->modelInit(SS_ARC_PTR(wk->pMapObj, 9), SS_ARC_PTR(wk->pMapDat, 0x10));
    } else {
        m->pCoin->modelInit(SS_ARC_PTR(wk->pMapDat, 0xF), SS_ARC_PTR(wk->pMapDat, 0x10));
    }
    mdl = m->pCoin;
    RotMatrix(mdl->l_mat, &mdl->ang);
    TransMatrix(mdl->l_mat, &mdl->pos);
    ScaleMatrix(mdl->l_mat, &mdl->scale);
    PSMTXCopy(mdl->l_mat, mdl->mat);
    if (mdl->pList) {
        mdl->partsMatCalc();
        mdl->partsWorldCalc();
    }
    mdl->be_flag &= ~2;
    m->nCoin = mdl->nParts;
}

// Destroys the medallion mark model.
void markCoinQuit(SUB_SCREEN* wk)
{
    cModel* mdl = wk->map->pCoin;

    if (mdl) {
        delete mdl;
    }
}

// Position of medallion mark `no`.
int markCoinPosition(SUB_SCREEN* wk, int no, Vec* pos)
{
    cParts* p = wk->map->pCoin->getPartsPtr(no);

    *pos = p->pos;
    return 1;
}

// 1 while medallion `no` of `stage` is still unbroken (checkSubMissionTarget).
int markCoinExist(int stage, int no)
{
    return checkSubMissionTarget(stage, no);
}

// Shows (sw) the medallion marks (IdNum i/0x16, broken ones with the shot overlay) and the
// remaining count digits (IdSub 0x14/0x15 of 0x10), or hides them.
void markCoinDisp(SUB_SCREEN* wk, int sw)
{
    SsMapWork* m = wk->map;
    IdUnit* u;

    if (!sw) {
        for (int i = 0; i < coin_mark_num; i++) {
            u = IdNum.unitPtr(i, IDC_SSCRN_2);
            u->be_flag &= ~8;
        }
        u = IdSub.unitPtr(0xF, IDC_SSCRN_NEAR_0);
        u->be_flag &= ~8;
    } else {
        Vec pos;
        Vec scr;
        int digit[2];
        int cnt;
        int v;
        int j;

        for (int i = 0; i < coin_mark_num; i++) {
            u = IdNum.unitPtr(i, IDC_SSCRN_2);
            if (i < m->nCoin) {
                markCoinPosition(wk, i, &pos);
                if (mapPos2screenPos(&pos, &scr)) {
                    u->pos0 = scr;
                    if (markCoinExist((s8) wk->stage_no, i)) {
                        u->be_flag |= 8;
                    } else {
                        u->be_flag &= ~8;
                    }
                } else {
                    u->be_flag &= ~8;
                }
            } else {
                u->be_flag &= ~8;
            }
        }
        u = IdSub.unitPtr(0xF, IDC_SSCRN_NEAR_0);
        if (m->area == 1) {
            u->be_flag |= 8;
        } else {
            u->be_flag &= ~8;
        }
        cnt = 0;
        for (int i = 0; i < m->nCoin; i++) {
            if (markCoinExist((s8) wk->stage_no, i)) {
                cnt++;
            }
        }
        v = m->nCoin - cnt;
        for (j = 0; j < 2; j++) {
            digit[j] = v % 10;
            v /= 10;
        }
        for (j = 0; j < 2; j++) {
            u = IdSub.unitPtr(0x12 - j, IDC_SSCRN_NEAR_0);
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = digit[j];
        }
        v = m->nCoin;
        for (j = 0; j < 2; j++) {
            digit[j] = v % 10;
            v /= 10;
        }
        for (j = 0; j < 2; j++) {
            u = IdSub.unitPtr(0x15 - j, IDC_SSCRN_NEAR_0);
            u->be_flag |= 8;
            u->tex_flag |= 2;
            u->texNo = digit[j];
        }
    }
}

// Constructs the typewriter mark model (area sub-file 10 or the common mark); nSave = its parts.
void markSaveInit(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    cModel* mdl;

    m->pSave = new (&m->save) cModel();
    if (mark_model_tbl[m->area][4]) {
        m->pSave->modelInit(SS_ARC_PTR(wk->pMapObj, 10), SS_ARC_PTR(wk->pMapDat, 0x10));
    } else {
        m->pSave->modelInit(SS_ARC_PTR(wk->pMapDat, 0xF), SS_ARC_PTR(wk->pMapDat, 0x10));
    }
    mdl = m->pSave;
    RotMatrix(mdl->l_mat, &mdl->ang);
    TransMatrix(mdl->l_mat, &mdl->pos);
    ScaleMatrix(mdl->l_mat, &mdl->scale);
    PSMTXCopy(mdl->l_mat, mdl->mat);
    if (mdl->pList) {
        mdl->partsMatCalc();
        mdl->partsWorldCalc();
    }
    mdl->be_flag &= ~2;
    m->nSave = mdl->nParts;
}

// Destroys the typewriter mark model.
void markSaveQuit(SUB_SCREEN* wk)
{
    cModel* mdl = wk->map->pSave;

    if (mdl) {
        delete mdl;
    }
}

// Position of typewriter mark `no`.
int markSavePosition(SUB_SCREEN* wk, int no, Vec* pos)
{
    cParts* p = wk->map->pSave->getPartsPtr(no);

    *pos = p->pos;
    return 1;
}

// Shows (sw) the typewriter marks (IdNum i/0x14) at their positions, or hides them.
void markSaveDisp(SUB_SCREEN* wk, int sw)
{
    SsMapWork* m = wk->map;
    IdUnit* u;
    int i;

    if (!sw) {
        for (i = 0; i < save_mark_num; i++) {
            u = IdNum.unitPtr(i, IDC_SSCRN_0);
            u->be_flag &= ~8;
        }
    } else {
        Vec pos;
        Vec scr;

        for (i = 0; i < save_mark_num; i++) {
            u = IdNum.unitPtr(i, IDC_SSCRN_0);
            if (i < m->nSave) {
                markSavePosition(wk, i, &pos);
                if (mapPos2screenPos(&pos, &scr)) {
                    u->pos0 = scr;
                    u->be_flag |= 8;
                    continue;
                }
            }
            u->be_flag &= ~8;
        }
    }
}

// World position -> map screen position (0 when behind the camera).
int mapPos2screenPos(Vec* pos, Vec* out)
{
    Mtx inv;
    SUB_SCREEN* wk = &SubScreenWk;
    f32 h;
    f32 w;
    f32 az;

    PSMTXInverse(pG->Camera.mat, inv);
    PSMTXMultVec(inv, pos, out);
    if (out->z > -fabsf(ZNEAR)) {
        return 0;
    }
    f32 ang = pG->Camera.param.fovy * 0.5f * 0.017453292f;
    f32 kx;
    f32 ky;

    az = fabsf(out->z);
    h = az * tanf(ang);
    w = h * 1.3333334f;
    kx = wk->map->sw * 0.5f / w;
    ky = wk->map->sh * 0.5f / h;
    out->x = out->x * kx;
    out->y = out->y * ky;
    out->z = out->z * 0.0f;
    out->x += wk->map->cx;
    out->y += wk->map->cy;
    return 1;
}

// Player (or partner) matrix `plMat` in the room's collision `hdrB` -> matrix on the map model
// (`partsMat`), through the barycentric position in the hit polygon of the map collision `hdrA`.
void mapPositionCheck(cSatHeader* hdrB, cSatHeader* hdrA, Mtx plMat, Mtx partsMat, Mtx out, int multi)
{
    cSat satB;
    cSat satA;
    Vec pl;
    Vec hit;
    Vec fwd = {0.0f, 0.0f, 1000.0f};
    Vec zero = {0.0f, 0.0f, 0.0f};
    Vec pos;
    Vec pos2;
    Vec a;
    Vec b;
    Vec hit2;
    Vec d;
    Vec c;
    f32 s;
    f32 t;
    f32 s0;
    f32 t0;
    f32 s1;
    f32 t1;
    f32 best;
    int idx;
    int i;
    AtPoly* poly;
    Vec* vtx;

    pl.x = plMat[0][3];
    pl.y = plMat[1][3];
    pl.z = plMat[2][3];
    PSMTXMultVecSR(plMat, &fwd, &fwd);
    satA.init(hdrA->getSat(0), &zero, &zero);
    {
        // COMPILER-DIFF: 5 (sched1 tie): the original issues the `&zero` argument copy before the
        // getSat result copy, i.e. its `&zero` was a separate pseudo (a copy of the hoisted address
        // that regmove then feeds into `mr r6,r5`) whose 3-insn chain to `init` outranks the result
        // copy's priority. The codeless asm keeps `z` a separate pseudo: the early-clobber output
        // stops local-alloc's tie, the duplicate input stops regmove's rename (count_occurrences > 1),
        // the memory input anchors it after the call, and reload emits the `mr` for the "0" match.
        cSatFile* sb = hdrB->getSat(0);
        Vec* z;
        asm("" : "=&r"(z) : "0"(&zero), "r"(&zero), "m"(zero));
        satB.init(sb, z, z);
    }
    a = pl;
    b = pl;
    best = 100000000.0f;
    idx = -1;
    if (multi) {
        a.y += map_dbg_ofs0;
        b.y += map_dbg_cam0[0];
    } else {
        a.y += 100000.0f;
        b.y -= 100000.0f;
    }
    poly = satB.poly_p;
    for (i = 0; i < satB.floor_num + satB.slope_num; i++, poly++) {
        if (At_poly_line_ck((AtPolyData*) &satB, &hit2, poly, &a, &b, 0, 0)) {
            if (hit2.y <= best) {
                idx = i;
                hit = hit2;
                best = hit.y;
            }
        }
    }
    if (idx == -1) {
        pLog->err(0, 0, "mapPositionCheck(): deviate from hit area");
        PSMTXIdentity(out);
    } else {
        poly = satB.poly_p;
        poly += idx;
        vtx = satB.vtx;
        PSVECSubtract(&vtx[poly->v[1]], &vtx[poly->v[0]], &a);
        PSVECSubtract(&vtx[poly->v[2]], &vtx[poly->v[0]], &b);
        c = hit;
        PSVECSubtract(&c, &vtx[poly->v[0]], &d);
        VecLinearDecomposition(&d, &a, &b, &s, &t);
        s0 = s;
        t0 = t;
        PSVECAdd(&hit, &fwd, &c);
        PSVECSubtract(&c, &vtx[poly->v[0]], &d);
        VecLinearDecomposition(&d, &a, &b, &s, &t);
        s1 = s;
        t1 = t;
        if (SubScreenWk.debug_menu & 0x10) {
            poly = satA.poly_p;
            vtx = satA.vtx;
            for (i = 0; i < satA.floor_num + satA.slope_num; i++, poly++) {
                u32 col;

                if (i != idx) {
                    col = poly->attr;
                } else {
                    col = 0xFFFF0000;
                }
                PSMTXMultVec(partsMat, &vtx[poly->v[0]], &a);
                PSMTXMultVec(partsMat, &vtx[poly->v[1]], &b);
                Draw_line3d(&a, &b, col, 0);
                PSMTXMultVec(partsMat, &vtx[poly->v[1]], &a);
                PSMTXMultVec(partsMat, &vtx[poly->v[2]], &b);
                Draw_line3d(&a, &b, col, 0);
                PSMTXMultVec(partsMat, &vtx[poly->v[2]], &a);
                PSMTXMultVec(partsMat, &vtx[poly->v[0]], &b);
                Draw_line3d(&a, &b, col, 0);
            }
        }
        poly = satB.poly_p;
        poly += idx;
        vtx = satA.vtx;
        PSVECSubtract(&vtx[poly->v[1]], &vtx[poly->v[0]], &a);
        PSVECSubtract(&vtx[poly->v[2]], &vtx[poly->v[0]], &b);
        VecLinearCombination(&a, s0, &b, t0, &pos);
        PSVECAdd(&pos, &vtx[poly->v[0]], &pos);
        VecLinearCombination(&a, s1, &b, t1, &pos2);
        PSVECAdd(&pos2, &vtx[poly->v[0]], &pos2);
        if (SubScreenWk.debug_menu & 0x10) {
            PSMTXMultVec(partsMat, &vtx[poly->v[0]], &hit2);
            PSMTXMultVec(partsMat, &pos, &d);
            Draw_line3d(&hit2, &d, 0xFF0000FF, 0);
            Draw_sphere(&d, 10.0f, 0xFFFF0000, 1, 1);
            PSMTXMultVec(partsMat, &pos, &hit2);
            PSMTXMultVec(partsMat, &pos2, &d);
            Draw_line3d(&hit2, &d, 0xFFFFFF00, 0);
        }
        PSVECSubtract(&pos2, &pos, &a);
        b.x = 0.0f;
        b.y = atan2f(a.x, a.z);
        b.z = 0.0f;
        PSMTXIdentity(out);
        RotMatrix(out, &b);
        TransMatrix(out, &pos);
        PSMTXConcat(partsMat, out, out);
    }
}

// Finds the display-flag row of `room` in a per-stage MapDispFlag table (0 when absent).
MapDispFlag* searchMapDispFlag(u16 room, MapDispFlag* tbl, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (room == tbl[i].room) {
            return &tbl[i];
        }
    }
    return 0;
}

// Room model colour: 0 current room, 1 visited, 2 open, 3 cleared, 4 hidden.
int mapColor(u16 room)
{
    MapDispFlag st1[22] = {
        {0x100, 0, 0, 29, 28}, {0x101, 0, 0, 29, 0},  {0x102, 0, 0, 29, 0},  {0x103, 0, 0, 29, 0},
        {0x104, 0, 0, 29, 28}, {0x105, 0, 0, 29, 28}, {0x106, 0, 0, 29, 28}, {0x107, 0, 0, 29, 28},
        {0x108, 0, 0, 29, 0},  {0x109, 0, 0, 29, 11}, {0x10A, 0, 0, 29, 11}, {0x10B, 0, 0, 29, 11},
        {0x10C, 0, 0, 29, 11}, {0x10D, 0, 0, 29, 11}, {0x10E, 0, 0, 29, 11}, {0x10F, 0, 0, 29, 0},
        {0x117, 0, 0, 29, 0},  {0x11C, 0, 0, 29, 0},  {0x11D, 0, 0, 29, 0},  {0x11E, 0, 0, 29, 0},
        {0x11F, 0, 0, 29, 0},  {0x200, 0, 0, 29, 0},
    };
    MapDispFlag st2a[27] = {
        {0x200, 0, 0, 40, 0}, {0x201, 0, 0, 40, 0}, {0x202, 0, 0, 40, 0}, {0x203, 0, 0, 40, 0},
        {0x204, 0, 0, 40, 0}, {0x205, 0, 0, 40, 0}, {0x206, 0, 0, 40, 0}, {0x207, 0, 0, 40, 0},
        {0x208, 0, 0, 40, 0}, {0x209, 0, 0, 40, 0}, {0x20A, 0, 0, 40, 0}, {0x20B, 0, 0, 40, 0},
        {0x20C, 0, 0, 40, 0}, {0x20D, 0, 0, 40, 0}, {0x20E, 0, 0, 40, 0}, {0x20F, 0, 0, 40, 0},
        {0x210, 0, 0, 40, 0}, {0x211, 0, 0, 40, 0}, {0x212, 0, 0, 40, 0}, {0x213, 0, 0, 40, 0},
        {0x214, 0, 0, 40, 0}, {0x215, 0, 0, 40, 0}, {0x216, 0, 0, 40, 0}, {0x217, 0, 0, 40, 0},
        {0x218, 0, 0, 40, 0}, {0x219, 0, 0, 40, 0}, {0x222, 0, 0, 40, 0},
    };
    MapDispFlag st2b[11] = {
        {0x21D, 0, 0, 40, 0}, {0x220, 0, 0, 40, 0}, {0x221, 0, 0, 40, 0}, {0x223, 0, 0, 40, 0},
        {0x224, 0, 0, 40, 0}, {0x225, 0, 0, 40, 0}, {0x226, 0, 0, 40, 0}, {0x227, 0, 0, 40, 0},
        {0x228, 0, 0, 40, 0}, {0x229, 0, 0, 40, 0}, {0x22A, 0, 0, 40, 0},
    };
    MapDispFlag st2c[2] = {
        {0x21A, 0, 0, 40, 0}, {0x21B, 0, 0, 40, 0},
    };
    MapDispFlag st3a[1] = {
        {0x300, 0, 0, 47, 0},
    };
    MapDispFlag st3b[5] = {
        {0x301, 0, 0, 47, 0}, {0x303, 0, 0, 47, 0}, {0x304, 0, 0, 47, 0}, {0x305, 0, 0, 47, 0},
        {0x306, 0, 0, 47, 0},
    };
    MapDispFlag st3c[8] = {
        {0x306, 0, 0, 47, 0}, {0x307, 0, 0, 47, 0}, {0x308, 0, 0, 47, 0}, {0x309, 0, 0, 47, 0},
        {0x30A, 0, 0, 47, 0}, {0x30B, 0, 0, 47, 0}, {0x30C, 0, 0, 47, 0}, {0x30E, 0, 0, 47, 0},
    };
    MapDispFlag st3d[5] = {
        {0x310, 0, 0, 47, 0}, {0x311, 0, 0, 47, 0}, {0x312, 0, 0, 47, 0}, {0x30D, 0, 0, 47, 0},
        {0x30F, 0, 0, 47, 0},
    };
    MapDispFlag st3e[13] = {
        {0x315, 0, 0, 47, 0}, {0x316, 0, 0, 47, 0}, {0x317, 0, 0, 47, 0}, {0x318, 0, 0, 47, 0},
        {0x31A, 0, 0, 47, 0}, {0x31B, 0, 0, 47, 0}, {0x31D, 0, 0, 47, 0}, {0x31C, 0, 0, 47, 0},
        {0x320, 0, 0, 47, 0}, {0x321, 0, 0, 47, 0}, {0x325, 0, 0, 47, 0}, {0x326, 0, 0, 47, 0},
        {0x327, 0, 0, 47, 0},
    };
    MapDispFlag st3f[5] = {
        {0x329, 0, 0, 47, 0}, {0x330, 0, 0, 47, 0}, {0x331, 0, 0, 47, 0}, {0x332, 0, 0, 47, 0},
        {0x333, 0, 0, 47, 0},
    };
    MapDispFlag st4a[1] = {
        {0x400, 0, 0, 29, 0},
    };
    MapDispFlag st4b[1] = {
        {0x402, 0, 0, 29, 0},
    };
    MapDispFlag st4c[1] = {
        {0x403, 0, 0, 29, 0},
    };
    MapDispFlag st4d[1] = {
        {0x404, 0, 0, 29, 0},
    };
    MapDispFlag st4e[1] = {
        {0x405, 0, 0, 29, 0},
    };
    MapDispFlag st4f[1] = {
        {0x406, 0, 0, 29, 0},
    };
    MapDispFlag st4g[8] = {
        {0x40A, 0, 0, 29, 0}, {0x40B, 0, 0, 29, 0}, {0x40C, 0, 0, 29, 0}, {0x40D, 0, 0, 29, 0},
        {0x40E, 0, 0, 29, 0}, {0x40F, 0, 0, 29, 0}, {0x410, 0, 0, 29, 0}, {0x411, 0, 0, 29, 0},
    };
    MapDispFlag* tbl = 0;
    int n = 0;
    MapDispFlag* p;
    int passed;

    switch (getAreaNo(room)) {
    case 0:
    case 1:
        tbl = st1;
        n = 22;
        break;
    case 2:
        tbl = st2a;
        n = 27;
        break;
    case 3:
        tbl = st2b;
        n = 11;
        break;
    case 4:
        tbl = st2c;
        n = 2;
        break;
    case 5:
        tbl = st3a;
        n = 1;
        break;
    case 6:
        tbl = st3b;
        n = 5;
        break;
    case 7:
        tbl = st3c;
        n = 8;
        break;
    case 8:
        tbl = st3d;
        n = 5;
        break;
    case 9:
        tbl = st3e;
        n = 13;
        break;
    case 10:
        tbl = st3f;
        n = 5;
        break;
    case 11:
        tbl = st4a;
        n = 1;
        break;
    case 12:
        tbl = st4b;
        n = 1;
        break;
    case 13:
        tbl = st4c;
        n = 1;
        break;
    case 14:
        tbl = st4d;
        n = 1;
        break;
    case 15:
        tbl = st4e;
        n = 1;
        break;
    case 16:
        tbl = st4f;
        n = 1;
        break;
    case 17:
        tbl = st4g;
        n = 8;
        break;
    }
    p = searchMapDispFlag(room, tbl, n);
    if (p == 0) {
        return 4;
    }
    if (room == SubScreenWk.room_no) {
        return 0;
    }
    if (FlagChkVar((u32) &pG->Scenario_flg[0], p->hide)) {
        return 4;
    }
    passed = RoomData.checkPassed(room, 0);
    // `if (open || passed) { .. } return 4;` (the function ends with the return 4 the `p == 0` test
    // shares): the clear arm's `li r3,3` stays inline and the `high pG` / 0x80000000 pseudos of the
    // three stageFlag tests are PRE'd into r29/r30 across the call (with `if (!open && !passed)
    // return 4;` the return-4 block is inline and the highs are re-materialised per test).
    if (FlagChkVar((u32) &pG->Scenario_flg[0], p->open) || passed) {
        if (FlagChkVar((u32) &pG->Scenario_flg[0], p->clear)) {
            return 3;
        }
        if (passed) {
            return 1;
        }
        return 2;
    }
    return 4;
}

// The room sub-file: word 0 = model count + 2, then the sub-file offsets from word 4.
int mapRoomNum(MapRoomData* p)
{
    return *(int*) p->bin - 2;
}

// Model `no` (bin) of a room sub-file.
void* mapBinAddr(MapRoomData* p, int no)
{
    return (u8*) p->bin + ((u32*) p->bin)[no + 4];
}

// Floor collision `no` of a room sub-file (the hit tables follow the model offsets).
cSatHeader* mapHitAddr(MapRoomData* p, int no)
{
    u32* ofs = (u32*) (mapRoomNum(p) * 4 + (u32) p->bin);

    return (cSatHeader*) ((u8*) p->bin + ofs[no + 4]);
}

#define MAP_ROOM(no, ofs)                       \
    p->room = no;                               \
    p->bin = SS_ARC_PTR(wk->pMapObj, ofs);     \
    p++;

// Room list of the village map (area 0/1): room numbers -> map_obj1.dat sub-files; returns the count.
int mapDataInit_St1(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x100, 0x0B);
    MAP_ROOM(0x101, 0x0C);
    MAP_ROOM(0x102, 0x0D);
    MAP_ROOM(0x103, 0x0E);
    MAP_ROOM(0x104, 0x0F);
    MAP_ROOM(0x105, 0x10);
    MAP_ROOM(0x106, 0x11);
    MAP_ROOM(0x107, 0x12);
    MAP_ROOM(0x108, 0x13);
    MAP_ROOM(0x109, 0x14);
    MAP_ROOM(0x10A, 0x15);
    MAP_ROOM(0x10B, 0x16);
    MAP_ROOM(0x10C, 0x17);
    MAP_ROOM(0x10D, 0x18);
    MAP_ROOM(0x10E, 0x19);
    MAP_ROOM(0x10F, 0x1A);
    MAP_ROOM(0x117, 0x1B);
    MAP_ROOM(0x11C, 0x1C);
    MAP_ROOM(0x11D, 0x1D);
    MAP_ROOM(0x11E, 0x1E);
    MAP_ROOM(0x11F, 0x1F);
    MAP_ROOM(0x200, 0x20);
    return p - map_room;
}

// Room list of castle area 2a (map_obj2a.dat).
int mapDataInit_St2A(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x200, 0x0B);
    MAP_ROOM(0x201, 0x0C);
    MAP_ROOM(0x202, 0x0D);
    MAP_ROOM(0x203, 0x0E);
    MAP_ROOM(0x204, 0x0F);
    MAP_ROOM(0x205, 0x10);
    MAP_ROOM(0x206, 0x11);
    MAP_ROOM(0x207, 0x12);
    MAP_ROOM(0x208, 0x13);
    MAP_ROOM(0x209, 0x14);
    MAP_ROOM(0x20A, 0x15);
    MAP_ROOM(0x20B, 0x16);
    MAP_ROOM(0x20C, 0x17);
    MAP_ROOM(0x20D, 0x18);
    MAP_ROOM(0x20E, 0x19);
    MAP_ROOM(0x20F, 0x1A);
    MAP_ROOM(0x210, 0x1B);
    MAP_ROOM(0x211, 0x1C);
    MAP_ROOM(0x212, 0x1D);
    MAP_ROOM(0x213, 0x1E);
    MAP_ROOM(0x214, 0x1F);
    MAP_ROOM(0x215, 0x20);
    MAP_ROOM(0x216, 0x21);
    MAP_ROOM(0x217, 0x22);
    MAP_ROOM(0x218, 0x23);
    MAP_ROOM(0x219, 0x24);
    MAP_ROOM(0x222, 0x25);
    return p - map_room;
}

// Room list of castle area 2b.
int mapDataInit_St2B(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x21D, 0x0B);
    MAP_ROOM(0x220, 0x0C);
    MAP_ROOM(0x221, 0x0D);
    MAP_ROOM(0x223, 0x0E);
    MAP_ROOM(0x224, 0x0F);
    MAP_ROOM(0x225, 0x10);
    MAP_ROOM(0x226, 0x11);
    MAP_ROOM(0x227, 0x12);
    MAP_ROOM(0x228, 0x13);
    MAP_ROOM(0x229, 0x14);
    MAP_ROOM(0x22A, 0x15);
    return p - map_room;
}

// Room list of castle area 2c.
int mapDataInit_St2C(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x21A, 0x0B);
    MAP_ROOM(0x21B, 0x0C);
    return p - map_room;
}

// Room list of island area 3a.
int mapDataInit_St3A(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x300, 0x0B);
    return p - map_room;
}

// Room list of island area 3b.
int mapDataInit_St3B(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x301, 0x0B);
    MAP_ROOM(0x303, 0x0C);
    MAP_ROOM(0x304, 0x0D);
    MAP_ROOM(0x305, 0x0E);
    MAP_ROOM(0x306, 0x0F);
    return p - map_room;
}

// Room list of island area 3c.
int mapDataInit_St3C(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x306, 0x0B);
    MAP_ROOM(0x307, 0x0C);
    MAP_ROOM(0x308, 0x0D);
    MAP_ROOM(0x309, 0x0E);
    MAP_ROOM(0x30A, 0x0F);
    MAP_ROOM(0x30B, 0x10);
    MAP_ROOM(0x30C, 0x11);
    MAP_ROOM(0x30E, 0x12);
    return p - map_room;
}

// Room list of island area 3d.
int mapDataInit_St3D(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x310, 0x0B);
    MAP_ROOM(0x311, 0x0C);
    MAP_ROOM(0x30D, 0x0E);
    MAP_ROOM(0x30F, 0x0F);
    MAP_ROOM(0x312, 0x0D);
    return p - map_room;
}

// Room list of island area 3e.
int mapDataInit_St3E(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x315, 0x0B);
    MAP_ROOM(0x316, 0x0C);
    MAP_ROOM(0x317, 0x0D);
    MAP_ROOM(0x318, 0x0E);
    MAP_ROOM(0x31A, 0x0F);
    MAP_ROOM(0x31B, 0x10);
    MAP_ROOM(0x31D, 0x11);
    MAP_ROOM(0x31C, 0x12);
    MAP_ROOM(0x320, 0x13);
    MAP_ROOM(0x321, 0x14);
    MAP_ROOM(0x325, 0x15);
    MAP_ROOM(0x326, 0x16);
    MAP_ROOM(0x327, 0x17);
    return p - map_room;
}

// Room list of island area 3f.
int mapDataInit_St3F(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x329, 0x0B);
    MAP_ROOM(0x330, 0x0C);
    MAP_ROOM(0x331, 0x0D);
    MAP_ROOM(0x332, 0x0E);
    MAP_ROOM(0x333, 0x0F);
    return p - map_room;
}

// Room list of stage 4 area a (one room).
int mapDataInit_St4A(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x400, 0x0B);
    return p - map_room;
}

// Room list of stage 4 area b.
int mapDataInit_St4B(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x402, 0x0B);
    return p - map_room;
}

// Room list of stage 4 area c.
int mapDataInit_St4C(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x403, 0x0B);
    return p - map_room;
}

// Room list of stage 4 area d.
int mapDataInit_St4D(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x404, 0x0B);
    return p - map_room;
}

// Room list of stage 4 area e.
int mapDataInit_St4E(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x405, 0x0B);
    return p - map_room;
}

// Room list of stage 4 area f.
int mapDataInit_St4F(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x406, 0x0B);
    return p - map_room;
}

// Room list of stage 4 area g (the remaining rooms).
int mapDataInit_St4G(SUB_SCREEN* wk)
{
    MapRoomData* p = map_room;

    MAP_ROOM(0x40A, 0x0B);
    MAP_ROOM(0x40B, 0x0C);
    MAP_ROOM(0x40C, 0x0D);
    MAP_ROOM(0x40D, 0x0E);
    MAP_ROOM(0x40E, 0x0F);
    MAP_ROOM(0x40F, 0x10);
    MAP_ROOM(0x410, 0x11);
    MAP_ROOM(0x411, 0x12);
    return p - map_room;
}

// Fills map_room[] for the current area, counts the room models (map_obj_num: door models follow
// them) and finds the current room's index (roomIdx, -1 when it is not on this map).
void mapTblInit(SUB_SCREEN* wk)
{
    int i;

    switch (wk->map->area) {
    case 0:
    case 1:
        map_room_num = mapDataInit_St1(wk);
        break;
    case 2:
        map_room_num = mapDataInit_St2A(wk);
        break;
    case 3:
        map_room_num = mapDataInit_St2B(wk);
        break;
    case 4:
        map_room_num = mapDataInit_St2C(wk);
        break;
    case 5:
        map_room_num = mapDataInit_St3A(wk);
        break;
    case 6:
        map_room_num = mapDataInit_St3B(wk);
        break;
    case 7:
        map_room_num = mapDataInit_St3C(wk);
        break;
    case 8:
        map_room_num = mapDataInit_St3D(wk);
        break;
    case 9:
        map_room_num = mapDataInit_St3E(wk);
        break;
    case 10:
        map_room_num = mapDataInit_St3F(wk);
        break;
    case 11:
        map_room_num = mapDataInit_St4A(wk);
        break;
    case 12:
        map_room_num = mapDataInit_St4B(wk);
        break;
    case 13:
        map_room_num = mapDataInit_St4C(wk);
        break;
    case 14:
        map_room_num = mapDataInit_St4D(wk);
        break;
    case 15:
        map_room_num = mapDataInit_St4E(wk);
        break;
    case 16:
        map_room_num = mapDataInit_St4F(wk);
        break;
    case 17:
        map_room_num = mapDataInit_St4G(wk);
        break;
    }
    wk->map_obj_num = 0;
    wk->map->roomIdx = -1;
    for (i = 0; i < map_room_num; i++) {
        wk->map_obj_num += mapRoomNum(&map_room[i]);
        if (wk->room_no == map_room[i].room) {
            wk->map->roomIdx = i;
        }
    }
}

// Map screen model managers: 0x80 model infos / 0x100 parts / 0x80 MapMgr works (no player model).
void mapModelAlloc(SUB_SCREEN* wk)
{
    wk->attr_flag |= 1;
    ssModInfoMgr.roomInit();
    ssModInfoMgr.arrayAlloc(0x80);
    ssPartsMgr.roomInit();
    ssPartsMgr.arrayAlloc(0x100);
    cModel::mm = &ssModInfoMgr;
    cModel::pm = &ssPartsMgr;
    MapMgr.roomInit();
    MapMgr.arrayAlloc(0x80);
}

// Light set and draw flags of every map model.
static inline void mapModelLight(cModel* m)
{
    static const Vec ofs = {0.0f, 0.0f, 0.0f};
    static const Vec size = {1000.0f, 1000.0f, 0.0f};

    m->LightInfo.init2(0, 0, &ofs, &size, 4);
    m->CullMode = 2;
    m->ot_type = 3;
}

// Builds the area's room models (one MapMgr work per model; room 0x10E is scaled x10), projects
// the player / partner matrices onto the map floor (mapPositionCheck -> pl_mat_map, subMapMat) and
// the player's floor number, then colours every model by mapColor (in the current room, other
// floors get colour 5 + floor) from the colour units IdSub 0..10 of group 0x19.
void mapModelInit(SUB_SCREEN* wk)
{
    IdUnit* id[11];
    int i;
    int no;
    int j;  // one `j` for both room loops: expand_preferences hands the second loop's hoisted `j + 1`
            // the r4 preference of mapBinAddr(.., j) (block-scoped j gives it r11)
    cModel* mdl;
    cSatHeader* hitA;
    cSatHeader* hitB;
    cParts* parts;
    f32 y;
    SsMapWork* m;

    mapTblInit(wk);
    id[0] = IdSub.unitPtr(0, IDC_SSCRN_FAR_1);
    id[1] = IdSub.unitPtr(1, IDC_SSCRN_FAR_1);
    id[2] = IdSub.unitPtr(2, IDC_SSCRN_FAR_1);
    id[3] = IdSub.unitPtr(3, IDC_SSCRN_FAR_1);
    id[4] = IdSub.unitPtr(4, IDC_SSCRN_FAR_1);
    id[5] = IdSub.unitPtr(5, IDC_SSCRN_FAR_1);
    id[6] = IdSub.unitPtr(6, IDC_SSCRN_FAR_1);
    id[7] = IdSub.unitPtr(7, IDC_SSCRN_FAR_1);
    id[8] = IdSub.unitPtr(8, IDC_SSCRN_FAR_1);
    id[9] = IdSub.unitPtr(9, IDC_SSCRN_FAR_1);
    id[10] = IdSub.unitPtr(10, IDC_SSCRN_FAR_1);
    no = 0;
    for (i = 0; i < map_room_num; i++) {
        int n = mapRoomNum(&map_room[i]);

        for (j = 0; j < n; j++) {
            MapMgr.create(i, no);
            mdl = MapMgr.getWork(no);
            mdl->modelInit(mapBinAddr(&map_room[i], j), SS_ARC_PTR(wk->pMapDat, 0xE));
            if (map_room[i].room == 0x10E) {
                mdl->scale.x = 10.0f;
                mdl->scale.y = 10.0f;
                mdl->scale.z = 10.0f;
                RotMatrix(mdl->mat, &mdl->ang);
                TransMatrix(mdl->mat, &mdl->pos);
                ScaleMatrix(mdl->mat, &mdl->scale);
            }
            mdl->partsMatCalc();
            mdl->partsWorldCalc();
            mapModelLight(mdl);
            no++;
        }
    }
    m = wk->map;
    if (m->roomIdx == -1) {
        PSMTXIdentity(wk->pl_mat_map);
    } else {
        hitA = mapHitAddr(&map_room[m->roomIdx], 0);
        hitB = mapHitAddr(&map_room[wk->map->roomIdx], 1);
        parts = MapMgr.room(wk->map->roomIdx, 0)->getPartsPtr(0);
        mapPositionCheck(hitB, hitA, wk->pl_mat, parts->mat, wk->pl_mat_map,
                         mapRoomNum(&map_room[wk->map->roomIdx]) - 1);
        if (pSUB) {
            parts = MapMgr.room(wk->map->roomIdx, 0)->getPartsPtr(0);
            mapPositionCheck(hitB, hitA, wk->sub_mat, parts->mat, wk->map->subMapMat,
                             mapRoomNum(&map_room[wk->map->roomIdx]) - 1);
        }
    }
    y = wk->pl_mat_map[1][3] / 100.0f;
    if (y > 0.0f) {
        wk->floor_no = (s8) (y + 0.5f);
    } else {
        wk->floor_no = (s8) (y - 0.5f);
    }
    no = 0;
    for (i = 0; i < map_room_num; i++) {
        int n = mapRoomNum(&map_room[i]);

        for (j = 0; j < n; j++) {
            cModelInfo* info;
            IdUnit* u;
            int col;

            mdl = MapMgr.getWork(no);
            col = mapColor(map_room[i].room);
            if (col == 0 && n > 1) {
                s8 floor = (s8) (mdl->getPartsPtr(0)->mat[1][3] / 100.0f + 0.5f);

                if (floor == wk->floor_no) {
                    col = 0;
                } else {
                    col = floor + 5;
                }
            }
            info = mdl->pModelInfo;
            u = id[col];
            if (info->be_flag & 2) {
                info->be_flag &= ~2;
                pLog->warn(0, 0, "mapModelInit(): R%1x%02x flag SHAPE_MODEL clear", 1, i);
            }
            for (; info; info = info->pList) {
                info->color[0] = u->col0[0];
                info->color[1] = u->col0[1];
                info->color[2] = u->col0[2];
                info->color[3] = u->col0[3];
            }
            no++;
        }
    }
}

// Per frame: moves the MapMgr models and registers each one with the light manager (LightSetModel2).
void mapModelDisp(SUB_SCREEN* wk)
{
    cModel* m;
    void (*func)(cModel*);

    MapMgr.move();
    func = LightSetModel2;
    m = MapMgr.getActiveWork();
    while (m) {
        cModel* p = m;

        m = (cModel*) m->getNext();
        func(p);
    }
}

// Creates the area's door models (map_door_tbl) after the room models: parts -1 = the area file's
// door set (sub-file 4), else the common door mesh placed on the given room parts with its angle.
// Also relocates pMapArea from an offset to a pointer on the first call.
void doorModelInit(SUB_SCREEN* wk)
{
    SsMapWork* m;
    MapDoor* e;
    int base;
    int n;
    int i;
    cModel* mdl;
    void* tpl;
    void* bin;

    if ((int) wk->pMapObj >= 0) {
        wk->pMapObj = (SsArc*) ((u8*) wk->pMapObj + (u32) wk->pBuf);
    }
    m = wk->map;
    base = (s8) wk->map_obj_num;
    e = map_door_tbl[m->area].p;
    n = map_door_tbl[m->area].n;
    for (i = 0; i < n; i++) {
        tpl = SS_ARC_PTR(wk->pMapDat, 0xD);
        MapMgr.create(e[i].parts, base + i);
        mdl = MapMgr.getWork(base + i);
        if (e[i].parts == -1) {
            bin = SS_ARC_PTR(wk->pMapObj, 4);
        } else {
            bin = SS_ARC_PTR(wk->pMapDat, 0xC);
        }
        mdl->modelInit(bin, tpl);
        if (e[i].parts == -1) {
            mdl->partsMatCalc();
            mdl->partsWorldCalc();
            mdl->be_flag &= ~2;
        } else {
            Mtx rot;

            mdl->partsMatCalc();
            PSMTXRotRad(rot, 'y', (f32) (int) e[i].ang * 3.1415927f / 180.0f);
            PSMTXConcat(MapMgr.getWork(base)->getPartsPtr(e[i].parts)->mat, rot, mdl->mat);
            mdl->partsWorldCalc();
        }
        mapModelLight(mdl);
    }
}

// Per frame door colouring: open (stage / unlock flag set, or no flag) takes IdSub 0x10/0x19's
// colour, locked 0x11, locked with the key item in the inventory 0x12; parts bit 7 skips the entry.
void doorModelDisp(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    MapDoor* e = map_door_tbl[m->area].p;
    int base = (s8) wk->map_obj_num;
    int n = map_door_tbl[m->area].n;
    IdUnit* id[3];
    int i;

    id[0] = IdSub.unitPtr(0x10, IDC_SSCRN_FAR_1);
    id[1] = IdSub.unitPtr(0x11, IDC_SSCRN_FAR_1);
    id[2] = IdSub.unitPtr(0x12, IDC_SSCRN_FAR_1);
    for (i = 0; i < n; i++, e++) {
        cModel* mdl;
        cModelInfo* info;
        IdUnit* u;
        int open;

        if (e->parts & 0x80) {
            continue;
        }
        mdl = MapMgr.getWork(base + i);
        if (e->flagType == 1) {
            open = 1;
            if (!FlagChkVar((u32) &pG->Scenario_flg[0], e->flagNo)) {
                open = 0;
            }
        } else if (e->flagType == 2) {
            open = 1;
            if (!FlagChkVar((u32) pG->Key_flg, e->flagNo)) {
                open = 0;
            }
        } else {
            open = 1;
        }
        if (open) {
            u = id[0];
        } else {
            u = id[1];
            if (ItemMgr.num(e->item)) {
                u = id[2];
            }
        }
        for (info = mdl->pModelInfo; info; info = info->pList) {
            info->color[0] = (u8) u->col[0];
            info->color[1] = (u8) u->col[1];
            info->color[2] = (u8) u->col[2];
            info->color[3] = (u8) u->col[3];
        }
    }
}

// Map camera: top-down (up = -z) at the stage's whole-map position; re-enables Key input.
void mapCameraInit(SUB_SCREEN* wk, Camera* cam)
{
    mapCameraEntire(wk, &cam->param);
    cam->Up.x = 0.0f;
    cam->Up.y = 0.0f;
    cam->Up.z = -1.0f;
    CameraSetOrientationUp(cam);
    wk->Key_disable = 0;
}

// Map scrolling in MapRead: d-pad/stick (Key bits 24..27) move the camera over the map at a speed
// scaled by the zoom, L/R (bits 22/23) change height between zoomInLimit and zoomOutLimit.
void mapCameraMove(SUB_SCREEN* wk)
{
    Vec d;

    map_cam_speed = map_cam_speed_base * (pG->Camera.Distance / 5000.0f);
    memclr_asm(&d, sizeof(Vec));
    if (Key.on & 0x0C000000) {
        if (Key.on & 0x08000000) {
            d.x = -map_cam_speed;
        }
        if (Key.on & 0x04000000) {
            d.x = map_cam_speed;
        }
    }
    if (Key.on & 0x03000000) {
        if (Key.on & 0x01000000) {
            d.z = -map_cam_speed;
        }
        if (Key.on & 0x02000000) {
            d.z = map_cam_speed;
        }
    }
    if (Key.on & 0x00C00000) {
        if (Key.on & 0x00400000) {
            d.y = map_dbg_cam1[0];
        }
        if (Key.on & 0x00800000) {
            d.y = -map_dbg_cam1[0];
        }
    }
    if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) {
        PSVECAdd(&pG->Camera.param.pos, &d, &pG->Camera.param.pos);
        d.y = 0.0f;
        PSVECAdd(&pG->Camera.param.at, &d, &pG->Camera.param.at);
        if (pG->Camera.param.pos.y <= zoomInLimit()) {
            pG->Camera.param.pos.y = zoomInLimit();
        }
        if (pG->Camera.param.pos.y >= zoomOutLimit()) {
            pG->Camera.param.pos.y = zoomOutLimit();
        }
        CameraSetOrientationUp(&pG->Camera);
    }
}

// Camera height of the whole-stage view (map_cam_entire of the stage).
f32 zoomOutLimit()
{
    return map_cam_entire[(s8) SubScreenWk.stage_no].pos.y;
}

// Whole-stage camera of the stage into `out`; swaps the "zoom in" / "zoom out" button hints.
void mapCameraEntire(SUB_SCREEN* wk, CameraParam* out)
{
    *out = map_cam_entire[(s8) wk->stage_no];
    IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->be_flag |= 8;
    IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
}

// Closest camera height: 4000 units of half-width at the current fov.
f32 zoomInLimit()
{
    return 4000.0f / tanf(pG->Camera.param.fovy * 0.5f * 3.1415927f / 180.0f);
}

// Zoomed camera into `out`: centred between the player and the goal, high enough to frame both
// (4:3), clamped to the zoom limits; swaps the button hints.
void mapCameraZoomIn(SUB_SCREEN* wk, CameraParam* out)
{
    Vec pl;
    Vec goal;
    Vec mid;
    Vec d;
    f32 h;

    pl.x = wk->pl_mat_map[0][3];
    pl.y = wk->pl_mat_map[1][3];
    pl.z = wk->pl_mat_map[2][3];
    markGoalPosition(wk, &goal);
    PSVECAdd(&pl, &goal, &mid);
    PSVECScale(&mid, &mid, 0.5f);
    PSVECSubtract(&pl, &goal, &d);
    d.x = fabsf(d.x);
    d.y = fabsf(d.y);
    d.z = fabsf(d.z);
    if (!(d.z / d.x >= 0.75f)) {
        d.z = d.x * 0.75f;
    }
    h = d.z / tanf(pG->Camera.param.fovy * 0.5f * 3.1415927f / 180.0f);
    if (h <= zoomInLimit()) {
        h = zoomInLimit();
    }
    if (h >= zoomOutLimit()) {
        h = zoomOutLimit();
    }
    out->pos = mid;
    out->at = mid;
    out->pos.y += h;
    IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->be_flag |= 8;
}

// Camera interpolation from `from` to `to` over `max` frames; 1 when done.
int zoomMove(SsMapWork* m, int max, int cnt)
{
    f32 t = (f32) cnt / (f32) max;
    Vec up = {0.0f, 0.0f, -1.0f};
    Vec a;
    Vec b;
    f32 s;

    CameraParam* from = &m->from;
    CameraParam* to = &m->to;

    PSVECScale(&to->pos, &a, t);
    s = 1.0f - t;
    PSVECScale(&from->pos, &b, s);
    PSVECAdd(&a, &b, &pG->Camera.param.pos);
    PSVECScale(&to->at, &a, t);
    PSVECScale(&from->at, &b, s);
    PSVECAdd(&a, &b, &pG->Camera.param.at);
    // pG loads pG separately from the earlier pG loads, so pG is reloaded for the call after the copy
    pG->Camera.Up = up;
    CameraSetOrientationUp(&pG->Camera);
    return t >= 1.0f;
}

// Area data file name: SS/cmn/map_obj<1|2a..2c|3a..3f|4a..4g>.dat for map area 0..17.
void mapAreaFilename(int area, char* name)
{
    switch (area) {
    case 0:
        sprintf(name, "SS/cmn/map_obj1.dat");
        break;
    case 1:
        sprintf(name, "SS/cmn/map_obj1.dat");
        break;
    case 2:
        sprintf(name, "SS/cmn/map_obj2a.dat");
        break;
    case 3:
        sprintf(name, "SS/cmn/map_obj2b.dat");
        break;
    case 4:
        sprintf(name, "SS/cmn/map_obj2c.dat");
        break;
    case 5:
        sprintf(name, "SS/cmn/map_obj3a.dat");
        break;
    case 6:
        sprintf(name, "SS/cmn/map_obj3b.dat");
        break;
    case 7:
        sprintf(name, "SS/cmn/map_obj3c.dat");
        break;
    case 8:
        sprintf(name, "SS/cmn/map_obj3d.dat");
        break;
    case 9:
        sprintf(name, "SS/cmn/map_obj3e.dat");
        break;
    case 10:
        sprintf(name, "SS/cmn/map_obj3f.dat");
        break;
    case 11:
        sprintf(name, "SS/cmn/map_obj4a.dat");
        break;
    case 12:
        sprintf(name, "SS/cmn/map_obj4b.dat");
        break;
    case 13:
        sprintf(name, "SS/cmn/map_obj4c.dat");
        break;
    case 14:
        sprintf(name, "SS/cmn/map_obj4d.dat");
        break;
    case 15:
        sprintf(name, "SS/cmn/map_obj4e.dat");
        break;
    case 16:
        sprintf(name, "SS/cmn/map_obj4f.dat");
        break;
    case 17:
        sprintf(name, "SS/cmn/map_obj4g.dat");
        break;
    }
}

// Map screen loader: skips the previous screen's exit (state 2) when opened directly as
// SS_OPEN_MAP (type 2, the in-game map key).
void SsMapInit::init(SUB_SCREEN* wk)
{
    if (wk->open_flag == 2) {
        state = 2;
    } else {
        state = 0;
    }
}

// Loads the map screen: state 0 run the previous screen's scrn_out_func and drop its ids, 1 one
// frame wait, 2 hide the HUD, read SS/<lang>/ss_map.dat into the puzzle slot and switch to the map
// model managers (no character model), 3 wait (archive -> pMapCmn, the area archive offset ->
// pMapArea), 4 fade in for SS_OPEN_MAP and transit to SsMapMain.
void SsMapInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
        if (wk->scrn_out_func(wk) == 1) {
            if (wk->menu_old == 2) {
                wk->wait_cnt = 1;
            }
            IdSubErase();
            IdNumErase();
            IdFreeBuffer();
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_NEAR_1, 0xF, 1, 0);
            map_wait[0] = 0;
            state++;
        }
        break;
    case 1:
        if (--map_wait[0] >= 0) {
            break;
        }
        state++;
        break;
    case 2:
        IdSys.dispSw(IDC_LIFE_METER, 0);
        sscrnDataFilename(wk, "ss_map.dat");
        if (wk->open_flag == 2) {
#line 3250 "D:/Bio4/Prog/ss_map.cpp"
            map_read_req = DVD_READ_N(wk->filename, wk->pSwitchDat, 0, 0, 0, 0x11);
        } else {
#line 3253 "D:/Bio4/Prog/ss_map.cpp"
            map_read_req = DVD_READ_N(wk->filename, wk->pSwitchDat, 0, 0, 0, 0x10);
        }
        if (map_read_req <= 0) {
            break;
        }
        sscrnModelFree(wk);
        mapModelAlloc(wk);
        sscrnLightClear(wk);
        ssPlModel = 0;
        ssWepModel = 0;
        ssPlMotion = 0;
        ssWepModel2 = 0;
        IdAllocBuffer();
        wk->wait_cnt = 0;
        state++;
    case 3: {
        int result;
        int size;

        if (Dvd.ReadCheck(map_read_req, &result, &size, 0) != 1) {
            break;
        }
        wk->pMapDat = wk->pSwitchDat;
        wk->pMapObj = (SsArc*) ((u8*) wk->pSwitchDat + result);
        state++;
    }
    case 4:
        if (wk->open_flag == 2) {
            FadeSetW(0x80000000, 5, 0, 0);
        }
        transit(0, wk);
        break;
    }
}

// Builds the map screen: the zoom/mode widgets (MapFocus -> MapZoomIn <-> MapRead/MapZoomOut ->
// MapEntire, MapModeSelect from Entire/Read), id textures and unit groups (0x19 room colours,
// IdNum 0x14..0x16 typewriter/treasure/medallion marks, 0x14 character marks, 0x10 frame, 0x1D
// button hints; hints for marks not yet available hidden), lights, the SsMapWork, viewport and
// camera; area from the room number; state 2 / step 0 = load the area data.
void SsMapMain::init(SUB_SCREEN* wk)
{
    focus = new MapFocus;
    entire = new MapEntire;
    zoomIn = new MapZoomIn;
    zoomOut = new MapZoomOut;
    read = new MapRead;
    modeSel = new MapModeSelect;
    focus->connect(0, zoomIn);
    entire->connect(0, zoomIn);
    entire->connect(1, modeSel);
    read->connect(0, zoomIn);
    read->connect(1, zoomOut);
    read->connect(2, modeSel);
    zoomIn->connect(0, read);
    zoomIn->connect(1, zoomOut);
    zoomOut->connect(0, entire);
    zoomOut->connect(1, zoomIn);
    modeSel->connect(0, entire);
    modeSel->connect(1, read);
    cur = focus;
    IdTexDataLoad(SS_ARC_PTR(wk->pMapDat, 4), TEX_OWNER_ID_SSCRN);
    if (!IdSub.setCk(IDC_SSCRN_NEAR_1)) {
        IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, IDC_SSCRN_NEAR_1, 0xF, 1, 0);
    }
    IdSub.set(SS_ARC_PTR(wk->pMapDat, 5), 0xFF, IDC_SSCRN_FAR_1, 9, 2, 0);
    IdNum.set(SS_ARC_PTR(wk->pMapDat, 0xA), 0xFF, IDC_SSCRN_2, 0xC, 6, 0);
    IdNum.set(SS_ARC_PTR(wk->pMapDat, 9), 0xFF, IDC_SSCRN_1, 0xC, 6, 0);
    IdNum.set(SS_ARC_PTR(wk->pMapDat, 8), 0xFF, IDC_SSCRN_0, 0xC, 6, 0);
    IdSub.set(SS_ARC_PTR(wk->pMapDat, 6), 0xFF, IDC_SSCRN_0, 0xC, 5, 0);
    IdSub.set(SS_ARC_PTR(wk->pMapDat, 7), 0xFF, IDC_SSCRN_NEAR_0, 0xF, 2, 0);
    IdSub.set(SS_ARC_PTR(wk->pMapDat, 0xB), 0xFF, IDC_SSCRN_CKPT_1, 0x13, 8, 0);
    IdSub.unitPtr(0x10, IDC_SSCRN_NEAR_0)->rev_flag |= 0xF;
    IdSub.unitPtr(0x10, IDC_SSCRN_NEAR_0)->be_flag &= ~8;
    IdSub.unitPtr(0, IDC_SSCRN_NEAR_0)->be_flag &= ~8;
    IdSub.unitPtr(0, IDC_SSCRN_NEAR_0)->rev_flag |= 0xF;
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x13));
#line 3409 "D:/Bio4/Prog/ss_map.cpp"
    wk->map = (SsMapWork*) MEM_ALLOC(sizeof(SsMapWork), 1, 0xD);
    mapInitViewport(wk);
    mapCameraInit(wk, &pG->Camera);
    IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    IdSub.unitPtr(2, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    if (!ScfFlagChk(pG, SCF_R104_MEET_MERCHANT)) {
        IdSub.unitPtr(0x12, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    }
    if (!ItemMgr.search(0xA9)) {
        IdSub.unitPtr(0x14, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    }
    if (ItemMgr.num(0xB0) == 0 && !ScfFlagChk(pG, SCF_CONTACT_MERCHANT)) {
        IdSub.unitPtr(0x13, IDC_SSCRN_CKPT_1)->be_flag &= ~8;
    }
    sscrnMainMenuInit(wk, 0);
    markGoalDisp(wk, 0);
    markPlayerDisp(wk, 0);
    markMerchantDisp(wk, 0);
    markTreasureDisp(wk, 0);
    markCoinDisp(wk, 0);
    markSaveDisp(wk, 0);
    wk->map->area = getAreaNo(wk->room_no);
    state = 2;
    step = 0;
    SndCall(0, 0x1E, 0, 0, 0, 0);
}

// Mark availability of the area (the mode menu entries).
int scf_check_merchant()
{
    SsMapWork* m = SubScreenWk.map;

    if (m->area == 1) {
        if (ScfFlagChk(pG, SCF_R104_MEET_MERCHANT)) {
            return 1;
        }
        return 0;
    }
    return mark_model_tbl[m->area][1];
}

// Treasure marks are shown when the area has them and the stage's treasure map item is owned
// (0xA9 village, 0x54 castle, 0x55 island).
int scf_check_treasure()
{
    int area = SubScreenWk.map->area;

    switch (area) {
    case 1:
        if (ItemMgr.search(0xA9) == 0) {
            return 0;
        }
        return mark_model_tbl[1][2];
    case 2:
    case 3:
    case 4:
        if (ItemMgr.search(0x54) == 0) {
            return 0;
        }
        break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
        if (ItemMgr.search(0x55) == 0) {
            return 0;
        }
        break;
    }
    return mark_model_tbl[area][2];
}

// Medallion marks are shown when the area has them; in the village only with the request note
// (item 0xB0) or the mission started (Scenario_flg bit 22).
int scf_check_submission()
{
    SsMapWork* m = SubScreenWk.map;

    if (m->area == 1) {
        if (ItemMgr.num(0xB0) != 0 || (ScfFlagChk(pG, SCF_CONTACT_MERCHANT))) {
            return 1;
        }
        return 0;
    }
    return mark_model_tbl[m->area][3];
}

// Typewriter marks are shown when the area has them (mark_model_tbl column 4).
int scf_check_typewriter()
{
    return mark_model_tbl[SubScreenWk.map->area][4];
}

// Map screen frame. state 0 runs the zoom widget chain (in MapRead: Y / B-or-Z on the in-game map
// exit the sub screen via link 4, B opens the main menu tab row), 1 the tab row (0 items, 1 case,
// 2 back, 3 files, 4 exit), 2 waits for the area load. step 0 reads the area file into pMapArea,
// 1 builds the room/door/mark models, 2 draws them every frame with the marks enabled by the mode
// bits (map_mode) and the scf_check_* availability.
void SsMapMain::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0: {
        Widget<SUB_SCREEN>* w = cur;

        w->move(wk);
        cur = w->cur;
        if (cur == read) {
            if (wk->open_flag == 2) {
                if (Key.trg & 0x40200000) {
                    wk->close_flag |= 4;
                    transit(4, wk);
                }
            } else {
                if (Key.trg & 0x00100000) {
                    wk->close_flag |= 4;
                    transit(4, wk);
                } else if (Key.trg & 0x40000000) {
                    state = 1;
                    wk->close_flag |= 4;
                    sscrnMainMenuInit(wk, 1);
                    SndCall(0, 0xA, 0, 0, 0, 0);
                }
            }
        }
        break;
    }
    case 1:
        if (sscrnMainMenu(wk)) {
            switch ((s8) wk->menu_no) {
            case 1:
                transit(0, wk);
                break;
            case 0:
                transit(1, wk);
                break;
            case 3:
                transit(3, wk);
                break;
            case 2:
                sscrnMainMenuInit(wk, 0);
                state = 0;
                break;
            case 4:
                transit(4, wk);
                break;
            }
        }
        break;
    case 2:
        if (step > 1) {
            state = 0;
        }
        break;
    }
    switch (step) {
    case 0: {
        char name[64];

        stageNameDisp(wk);
        mapAreaFilename(wk->map->area, name);
#line 3637 "D:/Bio4/Prog/ss_map.cpp"
        readReq = DVD_READ_N(name, wk->pMapObj, 0, 0, 0, 0x10);
        if (readReq <= 0) {
            break;
        }
        step++;
    }
    case 1:
        if (Dvd.ReadCheck(readReq, 0, 0, 0) != 1) {
            break;
        }
        mapModelInit(wk);
        doorModelInit(wk);
        markGoalInit(wk);
        markMerchantInit(wk);
        markTreasureInit(wk);
        markCoinInit(wk);
        markSaveInit(wk);
        step++;
        SndCall(0, 0x1B, 0, 0, 0, 0);
    case 2:
        mapChangeViewport(wk);
        mapModelDisp(wk);
        doorModelDisp(wk);
        markGoalDisp(wk, 1);
        markPlayerDisp(wk, 1);
        if (!mapModeCheck(wk, 0)) {
            if (scf_check_typewriter()) {
                markSaveDisp(wk, 1);
            } else {
                markSaveDisp(wk, 0);
            }
        } else {
            markSaveDisp(wk, 0);
        }
        if (!mapModeCheck(wk, 1)) {
            if (scf_check_merchant()) {
                markMerchantDisp(wk, 1);
            } else {
                markMerchantDisp(wk, 0);
            }
        } else {
            markMerchantDisp(wk, 0);
        }
        if (!mapModeCheck(wk, 2)) {
            if (scf_check_treasure()) {
                markTreasureDisp(wk, 1);
            } else {
                markTreasureDisp(wk, 0);
            }
        } else {
            markTreasureDisp(wk, 0);
        }
        if (!mapModeCheck(wk, 3)) {
            if (scf_check_submission()) {
                markCoinDisp(wk, 1);
            } else {
                markCoinDisp(wk, 0);
            }
        } else {
            markCoinDisp(wk, 0);
        }
        break;
    }
}

// Leaving the map: deletes the widgets and mark models, frees the map work, installs sscrn_map_out.
void SsMapMain::quit(SUB_SCREEN* wk)
{
    ssWidgetDelete(focus);
    ssWidgetDelete(entire);
    ssWidgetDelete(zoomIn);
    ssWidgetDelete(zoomOut);
    ssWidgetDelete(read);
    ssWidgetDelete(modeSel);
    markGoalQuit(wk);
    markMerchantQuit(wk);
    markTreasureQuit(wk);
    markCoinQuit(wk);
    markSaveQuit(wk);
    Mem_free(wk->map);
    wk->close_flag |= 4;
    sscrn_map_out_init(wk);
    wk->scrn_out_func = sscrn_map_out;
}

// Starts the map frame's close animation (IdSub 0/0x10) and fades the button hints.
void sscrn_map_out_init(SUB_SCREEN* wk)
{
    IdUnit* u = IdSub.unitPtr(0, IDC_SSCRN_NEAR_0);

    u->be_flag |= 8;
    u->rev_flag &= 0xF0;
    IdSub.setTime(u, 0);
    IdSub.unitPtr(3, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
}

// Exit routine (scrn_out_func): at frame 15 of the close animation frees the map models and kills
// the map id groups; 1 when the animation ended.
static int sscrn_map_out(SUB_SCREEN* wk)
{
    IdUnit* u = IdSub.unitPtr(0, IDC_SSCRN_NEAR_0);

    if ((s16) u->timer[0] == 0xF) {
        sscrnModelFree(wk);
        IdSub.unitPtr(1, IDC_SSCRN_NEAR_0)->be_flag &= ~8;
        IdSub.kill(0xFF, IDC_SSCRN_FAR_1);
        IdSub.kill(0xFF, IDC_SSCRN_0);
        IdNumErase();
    }
    if (u->anima_state & 1) {
        return 1;
    }
    return 0;
}

// First widget: starts the zoom from the current camera to the player/goal view (MapZoomIn).
void MapFocus::move(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;

    m->from = pG->Camera.param;
    mapCameraZoomIn(wk, &wk->map->to);
    transit(0, wk);
}

// Whole-stage view: A/B zoom in (link 0), X (Key bit 17) opens the mark mode menu (link 1, returns
// here).
void MapEntire::move(SUB_SCREEN* wk)
{
    if (Key.trg & 0xC0000000) {
        SsMapWork* m = wk->map;

        m->from = pG->Camera.param;
        mapCameraZoomIn(wk, &wk->map->to);
        transit(0, wk);
    } else if (Key.trg & 0x00020000) {
        wk->map->modeSel = 0;
        transit(1, wk);
    }
}

// Zoom-in animation start (10 frames).
void MapZoomIn::init(SUB_SCREEN* wk)
{
    count = 0;
    SndCall(0, 4, 0, 0, 0, 0);
}

// Interpolates the camera to the zoomed view; A reverses into MapZoomOut, done -> MapRead.
void MapZoomIn::move(SUB_SCREEN* wk)
{
    if (Key.trg & 0x80000000) {
        SsMapWork* m = wk->map;

        m->from = pG->Camera.param;
        mapCameraEntire(wk, &wk->map->to);
        transit(1, wk);
    } else {
        if (zoomMove(wk->map, 10, count++)) {
            transit(0, wk);
        }
    }
}

// Zoom-out animation start (10 frames).
void MapZoomOut::init(SUB_SCREEN* wk)
{
    count = 0;
    SndCall(0, 5, 0, 0, 0, 0);
}

// Interpolates the camera to the whole-stage view; B reverses into MapZoomIn, done -> MapEntire.
void MapZoomOut::move(SUB_SCREEN* wk)
{
    if (Key.trg & 0x40000000) {
        SsMapWork* m = wk->map;

        m->from = pG->Camera.param;
        mapCameraZoomIn(wk, &wk->map->to);
        transit(1, wk);
    } else {
        if (zoomMove(wk->map, 10, count++)) {
            transit(0, wk);
        }
    }
}

// Zoomed map reading: A zooms out (link 1), X opens the mark mode menu (link 2, returns here),
// otherwise the d-pad scrolls the camera (mapCameraMove).
void MapRead::move(SUB_SCREEN* wk)
{
    if (Key.trg & 0x80000000) {
        SsMapWork* m = wk->map;

        m->from = pG->Camera.param;
        mapCameraEntire(wk, &wk->map->to);
        transit(1, wk);
    } else if (Key.trg & 0x00020000) {
        wk->map->modeSel = 1;
        transit(2, wk);
    } else {
        mapCameraMove(wk);
    }
}

// Mark mode menu open: shows the menu panel (IdSub 0x10/0x10), cursor on row 0 (typewriter), hides
// the rows whose marks are not yet available (merchant before Scenario_flg bit 29, treasure
// without item 0xA9, medallions without 0xB0 / the mission), swaps the button hints.
void MapModeSelect::init(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    IdUnit* u;
    IdUnit* u2;

    IdSub.unitPtr(0x10, IDC_SSCRN_NEAR_0)->be_flag |= 8;
    IdSub.unitPtr(0x10, IDC_SSCRN_NEAR_0)->rev_flag &= 0xF0;
    m->modeCursor = 0;
    u = IdSub.unitPtr(0x20, IDC_SSCRN_NEAR_0);
    u2 = IdSub.unitPtr(0x60, IDC_SSCRN_NEAR_0);
    u->pos0 = u2->pos0;
    u->timer[3] = 0;
    u->timer[2] = 0;
    u->timer[1] = 0;
    u->timer[0] = 0;
    if (ScfFlagChk(pG, SCF_R104_MEET_MERCHANT)) {
        u = IdSub.unitPtr(0x61, IDC_SSCRN_NEAR_0);
        u->be_flag &= ~8;
    }
    if (ItemMgr.num(0xA9)) {
        u = IdSub.unitPtr(0x62, IDC_SSCRN_NEAR_0);
        u->be_flag &= ~8;
    }
    if (ItemMgr.num(0xB0) || (ScfFlagChk(pG, SCF_CONTACT_MERCHANT))) {
        u = IdSub.unitPtr(0x63, IDC_SSCRN_NEAR_0);
        u->be_flag &= ~8;
    }
    IdSub.unitPtr(2, IDC_SSCRN_CKPT_1)->be_flag |= 8;
    IdSub.unitPtr(2, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
    IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
    IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
    SndCall(0, 9, 0, 0, 0, 0);
}

// 1 when mark kind `no` (0 typewriter, 1 merchant, 2 treasure, 3 medallion) is switched OFF
// (map_mode bit set).
int mapModeCheck(SUB_SCREEN* wk, s8 no)
{
    u32 bit = 1 << no;

    if (wk->map_mode & bit) {
        return 1;
    }
    return 0;
}

// Toggles mark kind `no` in map_mode (saved with the game).
void mapModeChange(SUB_SCREEN* wk, s8 no)
{
    u32 bit = 1 << no;

    if (wk->map_mode & bit) {
        wk->map_mode &= ~bit;
    } else {
        wk->map_mode |= bit;
    }
}

// Mark mode menu: B/X close it back to Entire (modeSel 0) or Read (1); A toggles the mark kind
// under the cursor when available; up/down move the cursor (rows 0x60.. positions, check marks
// 0x50.. shown for enabled kinds).
void MapModeSelect::move(SUB_SCREEN* wk)
{
    SsMapWork* m = wk->map;
    IdUnit* u;

    if (Key.trg & 0x40020000) {
        switch (m->modeSel) {
        case 0:
            IdSub.unitPtr(2, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
            IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->be_flag |= 8;
            IdSub.unitPtr(1, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
            transit(0, wk);
            return;
        case 1:
            IdSub.unitPtr(2, IDC_SSCRN_CKPT_1)->rev_flag |= 0xF;
            IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->be_flag |= 8;
            IdSub.unitPtr(0, IDC_SSCRN_CKPT_1)->rev_flag &= 0xF0;
            transit(1, wk);
            return;
        }
    }
    // not `else if`: the switch's fall-out reaches this test too, so its label has two uses and cse
    // reloads Key.trg here
    if (Key.trg & 0x80000000) {
        switch (m->modeCursor) {
        case 1:
            if (!ScfFlagChk(pG, SCF_R104_MEET_MERCHANT)) {
                return;
            }
            break;
        case 3:
            if (ItemMgr.num(0xB0) == 0 && !ScfFlagChk(pG, SCF_CONTACT_MERCHANT)) {
                return;
            }
            break;
        case 2:
            if (ItemMgr.search(0xA9) == 0) {
                return;
            }
            break;
        }
        mapModeChange(wk, m->modeCursor);
        SndCall(0, 0x1D, 0, 0, 0, 0);
    } else {
        int i;
        int old;

        for (i = 0; i < 4; i++) {
            u = IdSub.unitPtr(0x50 + i, IDC_SSCRN_NEAR_0);
            if (mapModeCheck(wk, i)) {
                u->be_flag &= ~8;
            } else {
                u->be_flag |= 8;
            }
        }
        old = m->modeCursor;
        if (Key.rep & 0x01000000) {
            m->modeCursor--;
        }
        if (Key.rep & 0x02000000) {
            m->modeCursor++;
        }
        m->modeCursor = m->modeCursor < 0 ? 3 : (m->modeCursor > 3 ? 0 : m->modeCursor);
        if (old != m->modeCursor) {
            IdUnit* u2;

            u = IdSub.unitPtr(0x20, IDC_SSCRN_NEAR_0);
            u2 = IdSub.unitPtr(0x60 + m->modeCursor, IDC_SSCRN_NEAR_0);
            u->pos0 = u2->pos0;
            u->timer[3] = 0;
            u->timer[2] = 0;
            u->timer[1] = 0;
            u->timer[0] = 0;
            SndCall(0, 6, 0, 0, 0, 0);
        }
    }
}

// Hides the mode menu panel.
void MapModeSelect::quit(SUB_SCREEN* wk)
{
    IdSub.unitPtr(0x10, IDC_SSCRN_NEAR_0)->rev_flag |= 0xF;
    SndCall(0, 5, 0, 0, 0, 0);
}
