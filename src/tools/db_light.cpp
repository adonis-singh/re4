// cLight is 0x154 bytes in this file (light.h)
#define LIGHT_H_CLIGHT_154
#include "types.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "ctrl.h"
#include "dbg_var.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "db_log.h"
#include "dbmodule.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "obj.h"
#include "file.h"
#include "math_sub.h"
#include "vec.h"
#include "gx.h"
#include "scroll.h"
#include "em.h"
#include "etc_model.h"
#include "main.h"
#include "db_cam.h"
#include "player.h"
#include <stdio.h>
#include <string.h>
#include "db_menu.h"

// The module's 0x34-byte COMMON block (uninitialised template statics of the original build; the split
// skeleton defines it as `common_<mod>`, see em10.cpp / st_room.h): once db_light is compiled in a module
// the block must come from a compiled object, or make_rel refuses the link.
#define DB_LIGHT_STR2(x) #x
#define DB_LIGHT_STR(x) DB_LIGHT_STR2(x)
asm(".comm common_" DB_LIGHT_STR(REL_MODULE) ",52,4");

// Light editor (D:/Bio4/Prog/db_light.cpp): cLightTool (the editor), cDbLit (the .lit cuts being edited,
// one Debug_alloc'd cLightEnv per cut) and cLitPathTool (the light path table). The same object is in
// t_camera / t_light / t_event; Tools adds SetToolLight in front of it (tools/db_light_tools.cpp,
// DB_LIGHT_SET_TOOL_LIGHT), t_esp also cLightTool::setLogMode (tools/db_light_esp.cpp,
// DB_LIGHT_SET_LOG_MODE); t_sce / t_movie carry an older build (tools/db_light_v2.cpp, unwritten).

// A colour as one word (DrawTile swatches). The user copy constructor makes it BLKmode: every inlined
// drawColorTile shares one frame slot (see the FadeSet colour pair note in docs/matching.md).
struct GXColorW {
    u32 w;
    GXColorW() {}
    GXColorW(const GXColorW& c) { w = c.w; }
};

// Focus block of cLightEnv (0x28..0x30), copied as a whole by lightPasteFocus.
struct LightFocus {
    s32 depth;
    u8 x2C;
    u8 level;
    u8 mode;
    u8 blurAlpha;
};

// Light data file being edited: cut count / version / max light count like cLit, then one pointer per
// cut (Debug_alloc'd cLightEnv + its cLightWork entries; the tool keeps up to 256 cuts).
class cDbLit {
public:
    u16 nCut;             // 0x00
    u8 version;           // 0x02
    u8 nMaxLight;         // 0x03
    cLightEnv* pLh[256];  // 0x04

    cDbLit();
    u32 size();
    cLightEnv* getCut(u16 no);
    int isCut(u16 no) { return pLh[no] != NULL; }
    int fileLoad(const char* path);
    int init(cLit* lit);
    void preEventSave();
    int fileSave(const char* path);
    u32 createLit(cLit* dst);
};

// Light path table being edited: one Debug_alloc'd copy per path, plus the path under edit.
class cLitPathTool {
public:
    cLightPathData* Ofs[256];  // 0x000
    u8 Work[0x258];             // 0x400

    cLitPathTool();
    ~cLitPathTool();
    int expand(cLightPathHeader* hdr);
    int createPath(cLightPathHeader* dst);
};

class cLightTool {
public:
    u32 Flag;             // 0x00  bit0: object move, bit2: analyze, bit3: cut select follows the camera,
                           //       bit4: bounding boxes, bit5: log errors
    u8 rno0;            // 0x04  routine_tbl index
    u8 rno1;             // 0x05  edit_tbl index
    u8 rno2;                // 0x06  sub routine of the current editor
    u8 rno3;               // 0x07
    u8 rno4;                 // 0x08
    u8 rno5;                 // 0x09
    u8 rno6;                 // 0x0A
    u8 rno7;                 // 0x0B
    u8 sno0;                 // 0x0C
    u8 sno1;                 // 0x0D
    u8 sno2;                 // 0x0E
    u8 sno3;                 // 0x0F
    u8 pno0;                // 0x10
    u8 pno1;                // 0x11
    u8 pno2;                // 0x12
    u8 pno3;                // 0x13
    u8 cutNo;              // 0x14  cut being edited
    u8 EditCutNo;          // 0x15  cut the game camera selects
    u8 CutNum;            // 0x16
    u8 pad_17;
    int DataType;              // 0x18  0 room local, 1 room server, 2 event, 3 core, 4 tool, 5 item
    u8 be_flag;                // 0x1C  move() result: 1 = running, 2 = player mode, 0 = quit
    u8 Mode;              // 0x1D  0 init, 1 camera mode, 2 player mode, 10 mode select
    u8 PageNo;              // 0x1E  debug print page (pG->debug_mode) the eprintf calls draw on (PS2 PageNo; was `color`)
    u8 PrintNoBak;           // 0x1F
    cVarLoop<u8> modeSel;  // 0x20
    u8 cursor;             // 0x28
    u8 cursorCtr;              // 0x29
    u8 curSub;          // 0x2A
    u8 table_height;               // 0x2B  light table rows per page
    u8 LitAnaIdx;             // 0x2C
    u8 pad_2D[3];
    int cx;               // 0x30  light table column
    int cy;               // 0x34  light table row
    int table_y;               // 0x38  first light / cut shown
    u8* LitAnaWork;            // 0x3C  lightAnalysis: 4 bytes per scroll object
    cLight LitTmp;          // 0x40  copy buffer
    cLightEnv* CutTmp;   // 0x194
    JOY Pad1;               // 0x198
    JOY Pad2;              // 0x400
    f32 logx;              // 0x668
    f32 logy;              // 0x66C
    cDbLit Lit;            // 0x670
    cLitPathTool PathTool;  // 0xA74

    cLightTool();
    ~cLightTool();
    int move();
    u32 dblCk(u32 bit);
    int editEnable();
    void updateLit();
    void printCursor(int x, int y);
    void clearWork();
    void clearSubMenu();
#ifdef DB_LIGHT_SET_LOG_MODE
    void setLogMode(int on);
#endif
    int lightAnalysis();
};


int tcCurrentCameraNo();

// Debug heap pointers are checked for the MEM1 range (VALID_PTR) before use.
// Error messages go through pLog when bit 5 is set.
#define TOOL_ERR(args...)            \
    if (pTool->Flag & 0x20) {       \
        pLog->err(0, 0, args);       \
    }

// Menu tables (.data: `const char*` arrays, not const pointers)
static const char* light_id_name[] = {
    "NORMAL", "FLICK", "WAVE", "SPOT ROTATE", "SHADOW", "PATH", "FADE", "SHINE", "SPOT LOCK", "----",
};
static const char* light_type_name[] = {
    "CONSTANT", "LINEAR", "QUADRATIC", "SPOT LIGHT", "CUSTOM", "PARALLEL", "SPOT QUAD", "LOCAL AMBIENT",
    "----", "----", "----", "----", "----", "----", "----", "----",
};
static const char* shadow_type_name[] = {
    "NORMAL", "PARALLEL", "FIX", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----", "----", "----",
};
static const char* light_type_short[] = {
    "CNST", "LINE", "QUAD", "SPOT", "CSTM", "PARA", "SPQU", "LAMB", "----", "----", "----", "----", "----",
    "----", "----", "----",
};
static const char* shadow_type_short[] = {
    "NORM", "PARA", "FIX ", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----", "----",
};
static const char* self_shd_name[] = {
    "OFF", "1", "2", "3", "4", "5", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----",
};
static const char* soft_shd_name[] = {
    "OFF", "1", "2", "3", "----", "----", "----", "----", "----", "----", "----", "----", "----", "----",
    "----", "----",
};
static const char* aniso_name[] = {"GX_ANISO_1", "GX_ANISO_2", "GX_ANISO_4"};
static const char* parent_name[] = {"WORLD", "ENEMY", "SCROLL", "EtcModel", "OBJ"};
static const char* parent_short[] = {"WL", "EM", "SC", "ET", "OM"};
static const char* path_room_server = "y:/room/st%x/r%x%02x/r%x%02x%02x.lit";
static const char* path_room_local = "x:\\soft/room/St%x/r%x%02x/r%x%02x%02x.lit";
static const char* path_event = "x:\\soft/room/Event/r%x%02x/%s/etc/%s_%03d.lit";
static const char* path_event_action = "x:\\soft/room/Event/Action/%s/etc/%s_%03d.lit";
static const char* path_tool = "x:\\soft/room/tool%02x.lit";
static const char* path_item = "x:\\soft/room/SubScreen/light/item%03d.lit";
static const char* path_core = "x:\\soft/room/etc/core/core%02x.lit";
static const char* path_litpath = "x:\\soft/room/etc/core/litpath.bin";

static const GXColor blackTemplate = {0, 0, 0, 0};
static const f32 yAxis[3] = {0.0f, 1.0f, 0.0f};  // 4-aligned (an f32[3], not a Vec) after the vtables

static cLightPathHeader* LitPathPtr;
#define pLitPath (LitPathPtr)
static cLightTool* LightToolPtr;
#define pTool (LightToolPtr)
// The light count is a struct member: its load is not hoisted above the stores through pTool.
struct LightWorkNum {
    u32 n;
};
static LightWorkNum LightWorkNum_;
#define nLightWork (LightWorkNum_.n)
static cLightEnv* pLightEnv;

static void menu();
static void edit();
static void edit_menu();
static void edit_cutsel();
static void edit_cutsel_main();
static void edit_cutsel_sub();
int lightCopyCut(int no);
int lightPasteCut(int no);
int lightPasteAmbient(int no);
int lightPasteFog(int no);
int lightPasteMFog(int no);
int lightPasteShadow(int no);
int lightPasteFocus(int no);
int lightPasteBlur(int no);
int lightPasteTune(int no);
int lightPasteScale(int no);
int lightPasteCutAll(int no);
int lightPasteCutAll2(int no);
cLightEnv* copyCut(cLightEnv* src);
static void edit_light();
static void edit_light_select();
static void edit_light_select_sub();
void lightCopyWork(cLight* dst, cLight* src);
void lightCutWork(int no);
void lightInsertWork(int no);
static void edit_light_no();
static void edit_light_id();
static void edit_light_id_normal();
static void edit_light_id_flick();
static void edit_light_id_wave();
static void edit_light_id_round();
static void edit_light_id_shadow();
static void edit_light_id_path();
static void edit_light_id_fade();
static void edit_light_id_shine();
static void edit_light_id_spotlock();
static void edit_light_eid();
static void edit_light_parent();
void posTranslate(cLight* l, u8 type, u32 id);
static void edit_light_pos();
static void edit_light_radius();
static void edit_light_color();
static void edit_light_intensity();
static void edit_light_type();
void edit_light_type_shadow();
int shadow_select_type();
static void edit_light_type_shadow_fit();
static void edit_light_type_shadow_parallel();
static void edit_light_type_shadow_fix();
static void edit_light_kind();
static void edit_light_attr();
static void edit_light_priority();
int select_type();
static void edit_light_type_constant();
static void edit_light_type_quad();
static void edit_light_type_spotlight();
static void edit_light_type_direct();
static void edit_light_type_localamb();
f32 func_attn(cLight* l, f32 d);
void draw_light_graph(cLight* l);
static void edit_light_type_parallel();
static void edit_light_prop_sub();
static void edit_ambient();
static void edit_fog();
static void edit_mirror_fog();
void edit_fog_common(LightFog* fog);
static void edit_focus();
void draw_tone_curve();
static void edit_blur();
static void edit_mipmap();
static void edit_tune();
static void edit_scale();
static void edit_param();
static void edit_wind();
static void path();
static void load();
static void save();
static void option();
static void quit();
void printEditTable();
void DrawTile(int x, int y, int w, int h, GXColor* color);
// COMPILER-DIFF: candidate #18 (by-value aggregate view): under the V4 ABI a by-value GXColor is passed
// by invisible reference, so the caller copies it into a keep-0 stack temp (calls.c) that every call of
// the statement sequence reuses and passes its address in r7 -- the editColor shape. The real DrawTile
// takes a pointer (mangled P7GXColor); an inlined by-value wrapper allocates one keep-1 temp per call.
void DrawTileV(int x, int y, int w, int h, GXColor color) asm("DrawTile__FiiiiP7GXColor");
int LitLoadWork(cDbLit* lit, int no);
int LitSaveWork(cDbLit* lit, int no);
int editColor(int x, int y, GXColor* col);
const char* strFogType(int type);
int fogTypeNext(int type);
int fogTypeBack(int type);
void initLightWork(cLight* l);
void clear_move_free();
void clear_type_free();
int getCutNo();
void drawLightInfo_SpotShadow(cLight* l, u32 color);
void drawLightInfo(cLight* l, u32 color);
int pathSelect(int x, int y, u8 no, u8 flag, int mode);
int pathEdit(int x, int y, u8 no, u8 flag, int mode);
void drawPath(int x, int y, cLightPathData* p, u8 flag, u32 cur);


// Colour swatch: the colour word goes through a local of this inline, so its address is a fresh
// `addi r7, r1, ofs` before every DrawTile call (gcse never sees the hard-register argument set).
static inline void drawColorTile(int x, int y, int w, int h, u32 c)
{
    GXColorW col;
    col.w = c;
    DrawTile(x, y, w, h, (GXColor*) &col);
}

// The current light of the light table.
#define curLight() (LightMgr.at(pTool->table_y + pTool->cy))

#ifdef DB_LIGHT_SET_TOOL_LIGHT
// Tools / t_esp / t_sce / t_movie builds: load tool%02x.lit as the current light set (-1: reapply the
// current camera area's lit). The first function of those objects; global (called from t_mv, db_port's
// LightToolStart, t_sce_at / t_se_at).
int SetToolLight(int no)
{
    char path[0x100];

    if (no == -1) {
        int area = CamCtrl.areaNo;
        if (area == -1) {
            area = 0;
        }
        LightMgr.update(area, -1);
        return 1;
    }
    sprintf(path, path_tool, no);
    cDbLit lit;
    if (lit.fileLoad(path)) {
        if (LitLoadWork(&lit, 0) == 0) {
            return 0;
        }
        return 1;
    }
    return 0;
}
#endif

// Light editor start: takes the room's cLit (Lit.init), the cut count from the camera areas, the
// current cut (getCutNo) loaded into the LightMgr works, a copy buffer, the analysis table (4 bytes
// per scroll object); error logging (Flag 0x20) and analysis (Flag 4) on; menu routine 0.
cLightTool::cLightTool() : modeSel(0, 2, 0)
{
    pTool = this;
    be_flag = 1;
    table_height = 7;
    DataType = 1;
    rno0 = rno1 = rno2 = rno3 = rno4 = rno5 = rno6 = rno7 = sno0 = sno1 = sno2 = sno3 = pno0 = pno1 = pno2 = pno3 = 0;
    cursor = 0;
    cursorCtr = 0;
    curSub = 0;
    Mode = 0;
    cy = 0;
    cx = 0;
    table_y = 0;
    PageNo = pG->debug_mode;
    PrintNoBak = pG->debug_mode;
    pLightEnv = LightMgr.getEnvPtr();
    nLightWork = LightMgr.getArrayNum();
    Lit.init(*LightMgr.getLitPPtr());
    CutNum = CamCtrl.AreaNum();
    EditCutNo = cutNo = getCutNo();
    LitLoadWork(&Lit, cutNo);
    CutTmp = NULL;
    initLightWork(&LitTmp);
    pTool->Flag |= 0x20;
    LitAnaWork = (u8*) Debug_alloc(ObjMgr.getArrayNum() * 4, 1);
    if (!VALID_PTR(LitAnaWork)) {
        TOOL_ERR("cLightTool() MEMORY ERROR");
    }
    Flag |= 4;
    LitAnaIdx = 0;
    logx = 24.0f;
    logy = 140.0f;
}

// Resets the log display mode.
cLightTool::~cLightTool()
{
    pLog->modeReset();
}

// Light editor frame. Mode 0 copies the pads (pad 1 edits, pad 2 moves the camera), 1 CAMERA MODE
// (pad 1 doubles as the camera pad), 2 PLAYER MODE (the caller runs the player; returns 2), 10 the
// START mode select (LIGHT / CAMERA / PREVIEW). Runs routine_tbl[rno0]: menu, edit, path, load,
// save, option, quit; the header shows the tool mode and cut. Returns ret (1 running, 2 player
// mode, 0 quit).
int cLightTool::move()
{
    static void (*routine_tbl[])() = {menu, edit, path, load, save, option, quit};
    int i;

    eprintf(0x18, 0xE, 0, PageNo, "LIGHT TOOL");
    switch (DataType) {
    case 0:
    case 1:
        eprintf(0x1B0, 0xE, 0, PageNo, "CUT%02d/%02d", cutNo, CutNum);
        break;
    case 4:
        eprintf(0x1B8, 0xE, 0, PageNo, "TOOL %02d", cutNo);
        break;
    case 2:
        eprintf(0x1A8, 0xE, 0, PageNo, "ROOM%02d/%02d", cutNo, CutNum);
        break;
    case 3:
        eprintf(0x1B8, 0xE, 0, PageNo, "CORE %02d", cutNo);
        break;
    }
    switch (Mode) {
    case 0:
        Pad1 = Joy[0];
        Pad2 = Joy[1];
        be_flag = 1;
        break;
    case 1:
        eprintf(0xD8, 0, (pG->Frame_cnt & 0x10) ? 0 : 0x14, PageNo, "CAMERA MODE");
        Joy[1] = Joy[0];
        Joy[1].trg &= ~JOY_START;
        memclr_asm(&Pad1, sizeof(JOY));
        be_flag = 1;
        break;
    case 2:
        EditCutNo = cutNo = getCutNo();
        eprintf(0xD8, 0, (pG->Frame_cnt & 0x10) ? 0 : 0x14, PageNo, "PLAYER MODE");
        Pad2 = Joy[1];
        be_flag = 2;
        break;
    case 10:
        eprintf(0xD8, 0x38, 4, 0, "MODE SELECT");
        {
            int c;
            if (modeSel == 0) c = 0; else c = 0x14;
            eprintf(0xD8, 0x46, c, 0, "LIGHT");
            if (modeSel == 1) c = 0; else c = 0x14;
            eprintf(0xD8, 0x54, c, 0, "CAMERA");
            if (modeSel == 2) c = 0; else c = 0x14;
            eprintf(0xD8, 0x62, c, 0, "PREVIEW");
        }
        if (Joy[0].rep & JOY_UP) {
            modeSel--;
        }
        if (Joy[0].rep & JOY_DOWN) {
            modeSel++;
        }
        if (Joy[0].trg & (JOY_START | JOY_B | JOY_A)) {
            Mode = modeSel;
            Joy[0].trg &= ~(JOY_START | JOY_B | JOY_A);
            int st = Mode;
            switch (st) {
            case 0:
                DbgFlagOn(pG, DBG_DBG_CAM);
                PageNo = st;
                break;
            case 1:
                PageNo = st;
                break;
            case 2:
                PageNo = 1;
                updateLit();
                DbgFlagOff(pG, DBG_DBG_CAM);
                break;
            }
        }
        break;
    }
    if (Joy[0].trg & JOY_START) {
        if (Mode != 10) {
            modeSel.val = Mode;
            Mode = 10;
        }
    }
    cursorCtr++;
    if (Joy[0].rep) {
        cursorCtr = 0;
    }
    for (u32 n = 0; n < LightMgr.getArrayNum(); n++) {
        LightMgr.fastAt(n)->LitIndex = n;
    }
    routine_tbl[rno0]();
    LightMgr.move();
    if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
        if (Mode == 1) {
            CameraMove();
        } else {
            SpfFlagOff(pG, SPF_CAMERA);
            DbgFlagOff(pG, DBG_DBG_CAM);
        }
    } else {
        CameraMove();
    }
    if (pTool->Flag & 1) {
        ObjMgr.move();
    }
    CtrlMgr.move();
    if (pTool->Flag & 4) {
        lightAnalysis();
    }
    if (pTool->Flag & 0x10) {
        for (i = 0; i < ObjMgr.getArrayNum(); i++) {
            cObj* obj = ObjMgr.at(i);
            if (obj->isAlive() && obj->LightInfo.getLightNum()) {
                obj->drawAllBoundingBox(obj->pModelInfo);
            }
        }
    }
    logx += (f32) Joy[0].substickX * 0.1f;
    logy -= (f32) Joy[0].substickY * 0.1f;
    {
        int x = (int) logx;
        int y = (int) logy;
        cLog* l = pLog;
        l->m_Bx = x;
        l->m_By = y;
    }
    return be_flag;
}

// Flag bit test.
u32 cLightTool::dblCk(u32 bit)
{
    return Flag & bit;
}

// 1 when the edited cut may be written back: always for event / core / tool mode or with Flag 8
// (edit any cut), else only while the game camera is in the edited cut.
int cLightTool::editEnable()
{
    if (DataType == 3) {
        return 1;
    }
    if (DataType == 2) {
        return 1;
    }
    if (DataType == 4) {
        return 1;
    }
    if (dblCk(8)) {
        return 1;
    }
    return cutNo == EditCutNo;
}

// Writes the LightMgr works back into the edited cut (when allowed) and rebuilds the manager's
// debug cLit image (dbMem) from the whole cDbLit so the room lights follow the edit.
void cLightTool::updateLit()
{
    if (editEnable()) {
        LitSaveWork(&Lit, cutNo);
    }
    if (LightMgr.dbFlag & 1) {
        if (VALID_PTR(LightMgr.dbMem)) {
            Mem_free(LightMgr.dbMem);
        } else {
            TOOL_ERR("cLightTool::updateLit() PTR ERR %08X", LightMgr.dbMem);
        }
    }
    LightMgr.dbFlag |= 1;
#line 577 "D:/Bio4/Prog/db_light.cpp"
    LightMgr.dbMem = (cLit*) MEM_ALLOC(Lit.size(), 1, 13);
    if (!VALID_PTR(LightMgr.dbMem)) {
        TOOL_ERR("cLightTool::updateLit() MEM ALLOC FAILED");
        return;
    }
    LightMgr.dbSetRoomLit(LightMgr.dbMem);
    Lit.createLit(LightMgr.dbMem);
}

// Routine 0, the main MENU: EDIT / PATH / LOAD / SAVE / OPTION / QUIT; up/down, A enters (rno0 =
// row + 1), B jumps to QUIT.
static void menu()
{
    static const char* menu_name[] = {"EDIT", "PATH", "LOAD", "SAVE", "OPTION", "QUIT"};
    int i;
    const char** name;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "MENU");
    name = menu_name;
    for (i = 0; i < 6; i++) {
        eprintf(0x20, 0x38 + i * 14, 0, pTool->PageNo, *name++);
    }
    pTool->printCursor(3, pTool->cursor + 4);
    if (pTool->Pad1.rep & (JOY_UP | JOY_SUP)) {
        pTool->cursor = (pTool->cursor + 5) % 6;
    } else if (pTool->Pad1.rep & (JOY_DOWN | JOY_SDOWN)) {
        pTool->cursor = (pTool->cursor + 7) % 6;
    }
    if (pTool->Pad1.rep & JOY_A) {
        pTool->rno0 = pTool->cursor + 1;
        pTool->rno1 = pTool->rno2 = pTool->rno3 = 0;
        pTool->clearWork();
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->cursor = 5;
    }
}

// Routine 1: runs edit_tbl[rno1] (the EDIT WORK menu and its twelve editors).
static void edit()
{
    static void (*edit_tbl[])() = {
        edit_menu, edit_cutsel, edit_light, edit_ambient, edit_fog, edit_mirror_fog, edit_focus, edit_blur,
        edit_mipmap, edit_tune, edit_scale, edit_param, edit_wind,
    };

    edit_tbl[pTool->rno1]();
}

// EDIT WORK menu: CUT SELECT, LIGHT, AMBIENT, FOG, MIRROR FOG, FOCUS, BLUR, MIPMAP, LIT TUNE, LIT
// SCALE, PARAMETER, WIND; A enters (rno1 = row + 1), B back to the main menu.
static void edit_menu()
{
    static const char* edit_name[] = {
        "CUT SELECT", "LIGHT", "AMBIENT", "FOG", "MIRROR FOG", "FOCUS", "BLUR", "MIPMAP", "LIT TUNE",
        "LIT SCALE", "PARAMETER", "WIND",
    };
    u32 i;
    const char** name;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "EDIT WORK");
    name = edit_name;
    for (i = 0; i < sizeof(edit_name) / sizeof(char*); i++) {
        eprintf(0x20, 0x38 + i * 14, 0, pTool->PageNo, *name++);
    }
    pTool->printCursor(3, pTool->cursor + 4);
    if (pTool->Pad1.rep & (JOY_UP | JOY_SUP)) {
        pTool->cursor = (pTool->cursor + 11) % (sizeof(edit_name) / sizeof(char*));
    } else if (pTool->Pad1.rep & (JOY_DOWN | JOY_SDOWN)) {
        pTool->cursor = (pTool->cursor + 13) % (sizeof(edit_name) / sizeof(char*));
    }
    if (pTool->Pad1.rep & JOY_A) {
        pTool->rno1 = pTool->cursor + 1;
        pTool->rno2 = pTool->rno3 = pTool->rno4 = pTool->rno5 = pTool->rno6 = pTool->rno7 = 0;
        pTool->table_y = 0;
        pTool->clearWork();
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno0 = 0;
        pTool->rno1 = 0;
        pTool->clearWork();
    }
}

// CUT TABLE: lists the cuts with their light counts / on-off flags and runs cutsel_tbl[rno2]
// (main list, sub menu).
static void edit_cutsel()
{
    static const char* cut_onoff[] = {"1", "2", "4", "x"};
    static void (*cutsel_tbl[])() = {edit_cutsel_main, edit_cutsel_sub};
    int i;
    cLightEnv* env;
    // COMPILER-DIFF: #4 (reverse): the original stores `i + 6` into a narrow local as a plain `mr r30, r0` (no
    // mask); a `u8 line` gives `clrlwi 24`, an `int line` folds the copy into the `addi`, so the sum goes through a
    // pinned, laundered r0 temp into an `int` line (the copy is opaque to loop.c, so `line * 14` stays a mulli)
    int line;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "CUT TABLE");
    eprintf(0x20, 0x46, 4, pTool->PageNo, "NO  LI AMB FOG  MFOG SHDW FOCUS BLR TUNE SCL");
    for (i = 0; i < 20; i++) {
        env = pTool->Lit.getCut(pTool->table_y + i);
        eprintf(0x20, 0x54 + i * 14, pTool->table_y + i == pTool->cutNo ? 0 : 0x14, pTool->PageNo, "%03d", pTool->table_y + i);
        {
            register int t asm("r0"); // COMPILER-DIFF: 4 (unmasked narrow store, see `line`)
            t = i + 6;
            asm("" : "+r"(t)); // combine would fold the hard-reg copy into the addi
            line = t;
        }
        if (VALID_PTR(env)) {
            eprintf(0x40, 0x54 + i * 14, 0, pTool->PageNo, "%2d               %d%d%d  %5d %3d", env->nLight, 0, 0, 0,
                    env->FocusZ / 10, env->blur_rate);
            drawColorTile(0x58, 0x57 + i * 14, 0x18, 8, *(u32*) &env->AmbientScr);
            drawColorTile(0x78, 0x57 + i * 14, 0x20, 8, *(u32*) &env->Fog.Color);
            drawColorTile(0xA0, 0x57 + i * 14, 0x20, 8, *(u32*) &env->MirrorFog.Color);
            if (env->tuneOn & 1) {
                drawColorTile(0x140, 0x57 + i * 14, 0x20, 8, *(u32*) &env->Tune[0]);
            } else {
                eprintf(0x140, 0x54 + i * 14, 0, pTool->PageNo, "OFF");
            }
            eprintf(0x168, line * 14, 0, pTool->PageNo, "%s", cut_onoff[env->tev_scale[0] & 3]);
            eprintf(0x170, line * 14, 0, pTool->PageNo, "%s", cut_onoff[env->tev_scale[1] & 3]);
            eprintf(0x178, line * 14, 0, pTool->PageNo, "%s", cut_onoff[env->tev_scale[2] & 3]);
        }
    }
    cutsel_tbl[pTool->rno2]();
}

// Cut list cursor: saves the current works into the cut first, up/down (R pages) move over the
// cuts, A loads the cut under the cursor for editing (cutNo), Y opens the sub menu, B back.
static void edit_cutsel_main()
{
    static const int cutsel_col[9] = {3, 10, 14, 19, 24, 29, 35, 39, 44};
    int base;

    if (pTool->rno3 == 0) {
        LitSaveWork(&pTool->Lit, pTool->cutNo);
        pTool->table_y = 0;
        pTool->cursor = pTool->cutNo;
        pTool->cx = pTool->cy = 0;
        pTool->rno3 = 1;
    }
    base = pTool->table_y - 6;
    pTool->printCursor(cutsel_col[pTool->cx], pTool->cursor - base);
    if ((pTool->Pad1.rep & JOY_UP) || (pTool->Pad1.on & JOY_SUP)) {
        if (pTool->cursor != 0) {
            pTool->cursor--;
        }
        if (pTool->table_y > pTool->cursor) {
            pTool->table_y = pTool->cursor;
        }
    }
    if ((pTool->Pad1.rep & JOY_DOWN) || (pTool->Pad1.on & JOY_SDOWN)) {
        if (pTool->cursor <= 0xFE) {
            pTool->cursor++;
        }
        if (pTool->cursor > pTool->table_y + 19) {
            pTool->table_y = pTool->cursor - 19;
        }
    }
    if (pTool->Pad1.rep & JOY_R) {
        // if/else with a store in each arm (cross-jumped after reload): the join block is a new
        // cse ebb, so the next `pTool` load re-materialises `lis LightToolPtr@ha`; a clamp into a
        // temp (`if (n > 0xFE) n = 0xFF;`) is a skippable block that keeps the shared @ha register.
        if (pTool->cursor + 10 < 0xFF) {
            pTool->cursor += 10;
        } else {
            pTool->cursor = 0xFF;
        }
        if (pTool->cursor > pTool->table_y + 19) {
            pTool->table_y = pTool->cursor - 19;
        }
    }
    if (pTool->Pad1.rep & JOY_L) {
        if (pTool->cursor - 10 > 0) {
            pTool->cursor -= 10;
        } else {
            pTool->cursor = 0;
        }
        if (pTool->table_y > pTool->cursor) {
            pTool->table_y = pTool->cursor;
        }
    }
    // the stick bits are the other way round from joy.h's names here
    if ((pTool->Pad1.rep & JOY_RIGHT) || (pTool->Pad1.on & 0x20000)) {
        pTool->cx = (pTool->cx + 10) % 9;
    }
    if ((pTool->Pad1.rep & JOY_LEFT) || (pTool->Pad1.on & 0x10000)) {
        pTool->cx = (pTool->cx + 8) % 9;
    }
    if ((pTool->Pad1.rep & JOY_A) && pTool->cx == 0) {
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        pTool->cutNo = pTool->cursor;
        LitLoadWork(&pTool->Lit, pTool->cutNo);
        LitSaveWork(&pTool->Lit, pTool->cutNo);
    }
    if (pTool->Pad1.trg & JOY_Y) {
        pTool->clearSubMenu();
        pTool->rno2 = 1;
    }
    if (pTool->Pad1.rep & JOY_B) {
        if (!pTool->editEnable()) {
            pTool->cutNo = pTool->EditCutNo;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
        }
        pTool->rno1 = 0;
        pTool->rno2 = 0;
        pTool->rno3 = 0;
        pTool->clearWork();
    }
}

// Cut SUB MENU: CUT, COPY, PASTE, INSERT, COPY TO BLANK CUT, COPY TO ALL CUT (the lightCopyCut /
// lightPaste* helpers on the cursor cut); B closes it.
static void edit_cutsel_sub()
{
    static const char* cutsel_sub_name[] = {
        "CUT", "COPY", "PASTE", "INSERT", "COPY TO BLANK CUT", "COPY TO ALL CUT",
    };
    int i;
    const char** name;
    int no;

    eprintf(0x150, 0x62, 4, pTool->PageNo, "SUB MENU");
    name = cutsel_sub_name;
    for (i = 0; i < 6; i++) {
        eprintf(0x150, 0x70 + i * 14, 0, pTool->PageNo, *name++);
    }
    pTool->printCursor(0x29, pTool->curSub + 8);
    if ((pTool->Pad1.rep & JOY_A) || (pTool->Pad1.trg & JOY_Y)) {
        no = pTool->curSub;
        switch (no) {
        case 0:
            if (pTool->Lit.getCut(pTool->cursor) != NULL) {
                lightCopyCut(pTool->cursor);
                Debug_free(pTool->Lit.getCut(pTool->cursor));
                pTool->Lit.pLh[pTool->cursor] = 0;
            }
            break;
        case 1:
            lightCopyCut(pTool->cursor);
            break;
        case 2:
            switch (pTool->cx) {
            case 0:
                lightPasteCut(pTool->cursor);
                break;
            case 1:
                lightPasteAmbient(pTool->cursor);
                break;
            case 2:
                lightPasteFog(pTool->cursor);
                break;
            case 3:
                lightPasteMFog(pTool->cursor);
                break;
            case 4:
                lightPasteShadow(pTool->cursor);
                break;
            case 5:
                lightPasteFocus(pTool->cursor);
                break;
            case 6:
                lightPasteBlur(pTool->cursor);
                break;
            case 7:
                lightPasteTune(pTool->cursor);
                break;
            case 8:
                lightPasteScale(pTool->cursor);
                break;
            }
            if (pTool->cursor == pTool->cutNo) {
                LitLoadWork(&pTool->Lit, pTool->cutNo);
            }
            break;
        case 3:
            lightPasteCut(pTool->cursor);
            if (pTool->cursor == pTool->cutNo) {
                LitLoadWork(&pTool->Lit, pTool->cutNo);
            }
            break;
        case 4:
            lightPasteCutAll(pTool->cursor);
            break;
        case 5:
            lightPasteCutAll2(pTool->cursor);
            break;
        }
        pTool->rno2 = 0;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->curSub = (pTool->curSub + 5) % 6;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->curSub = (pTool->curSub + 7) % 6;
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// Copies cut `no` into the tool's cut clipboard (CutTmp); 0 when the cut is empty.
int lightCopyCut(int no)
{
    if (!pTool->Lit.isCut(no)) {
        return 0;
    }
    if (pTool->CutTmp) {
        Debug_free(pTool->CutTmp);
    }
    pTool->CutTmp = copyCut(pTool->Lit.getCut(no));
    return 1;
}

// Replaces cut `no` with a copy of the clipboard cut; 0 without a clipboard.
int lightPasteCut(int no)
{
    if (pTool->CutTmp == NULL) {
        return 0;
    }
    if (pTool->Lit.isCut(no)) {
        Debug_free(pTool->Lit.getCut(no));
    }
    pTool->Lit.pLh[(u16) no] = copyCut(pTool->CutTmp);
    return 1;
}

// Pastes only the clipboard cut's ambient colours into cut `no`.
int lightPasteAmbient(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        pTool->Lit.getCut(no)->AmbientScr = pTool->CutTmp->AmbientScr;
        return 1;
    }
    return 0;
}

// Pastes only the fog block.
int lightPasteFog(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        pTool->Lit.getCut(no)->Fog = pTool->CutTmp->Fog;
        return 1;
    }
    return 0;
}

// Pastes only the mirror fog block (cLightEnv + 0x18).
int lightPasteMFog(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        pTool->Lit.getCut(no)->MirrorFog = pTool->CutTmp->MirrorFog;
        return 1;
    }
    return 0;
}

// Nothing to paste (shadow settings live in the lights).
int lightPasteShadow(int no)
{
    return 0;
}

// Pastes only the focus block (FocusZ / level / mode).
int lightPasteFocus(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        *(LightFocus*) &pTool->Lit.getCut(no)->FocusZ = *(LightFocus*) &pTool->CutTmp->FocusZ;
        return 1;
    }
    return 0;
}

// Pastes only the blur rate.
int lightPasteBlur(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        pTool->Lit.getCut(no)->blur_rate = pTool->CutTmp->blur_rate;
        return 1;
    }
    return 0;
}

// Pastes only the lit tune block (cLightEnv + 0x30).
int lightPasteTune(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        *(LightFog*) ((u8*) pTool->Lit.getCut(no) + 0x30) = *(LightFog*) ((u8*) pTool->CutTmp + 0x30);
        return 1;
    }
    return 0;
}

// Pastes only the two TEV scale values and the two bytes after them.
int lightPasteScale(int no)
{
    if (pTool->Lit.isCut(no) && pTool->CutTmp) {
        pTool->Lit.getCut(no)->tev_scale[0] = pTool->CutTmp->tev_scale[0];
        pTool->Lit.getCut(no)->tev_scale[1] = pTool->CutTmp->tev_scale[1];
        pTool->Lit.getCut(no)->tev_scale[2] = pTool->CutTmp->tev_scale[2];
        pTool->Lit.getCut(no)->tev_scale[3] = pTool->CutTmp->tev_scale[3];
        return 1;
    }
    return 0;
}

// COPY TO BLANK CUT: copies cut `no` over every cut that has no lights.
int lightPasteCutAll(int no)
{
    int i;

    if (!pTool->Lit.isCut(no)) {
        return 0;
    }
    for (i = 0; i < pTool->CutNum; i++) {
        if (pTool->Lit.isCut(i)) {
            if (pTool->Lit.getCut(i)->nLight != 0) {
                continue;
            }
            Debug_free(pTool->Lit.getCut(i));
        }
        pTool->Lit.pLh[(u16) i] = copyCut(pTool->Lit.getCut(no));
    }
    return 1;
}

// COPY TO ALL CUT: copies cut `no` over every cut.
int lightPasteCutAll2(int no)
{
    int i;

    if (!pTool->Lit.isCut(no)) {
        return 0;
    }
    for (i = 0; i < pTool->CutNum; i++) {
        if (pTool->Lit.isCut(i)) {
            Debug_free(pTool->Lit.getCut(i));
        }
        pTool->Lit.pLh[(u16) i] = copyCut(pTool->Lit.getCut(no));
    }
    return 1;
}

// Debug-heap duplicate of a cut (env + its light works).
cLightEnv* copyCut(cLightEnv* src)
{
    u32 size = src->nLight * sizeof(cLightWork) + sizeof(cLightEnv);
    cLightEnv* dst = (cLightEnv*) Debug_alloc(size, 1);
    memcpy(dst, src, size);
    return dst;
}

// LIGHT editor: runs edit_light_tbl[rno2] (0 the table cursor, 1..12 the property editors picked by
// column: no, id, eid, parent, pos, radius, color, intensity, type, kind, attr, priority; 20/21 the
// sub menus) and prints the light table.
static void edit_light()
{
    static void (*edit_light_tbl[40])() = {
        edit_light_select, edit_light_no, edit_light_id, edit_light_eid, edit_light_parent, edit_light_pos,
        edit_light_radius, edit_light_color, edit_light_intensity, edit_light_type, edit_light_kind,
        edit_light_attr, edit_light_priority, NULL, NULL, NULL, NULL, NULL, NULL, NULL, edit_light_select_sub,
        edit_light_prop_sub,
    };

    edit_light_tbl[pTool->rno2]();
    printEditTable();
}

// Light table cursor: left/right pick a column, up/down a light (scrolling the page), A opens the
// column's editor (rno2 = col + 1), Y the sub menu (rno2 + 20), B back to the EDIT WORK menu.
static void edit_light_select()
{
    static int light_col[12] = {3, 6, 9, 0xF, 0x12, 0x20, 0x24, 0x28, 0x2C, 0x31, 0x36, 0x3B};
    static int light_col_w[12] = {6, 0x18, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32};
    u32 i;
    cLight* cur;

    pTool->printCursor(light_col[pTool->cx], pTool->cy + 0x18);
    if (pTool->Pad1.rep & 0x20002) {
        pTool->cx = (pTool->cx + 13) % 12;
    } else if (pTool->Pad1.rep & 0x10001) {
        pTool->cx = (pTool->cx + 11) % 12;
    }
    if ((pTool->Pad1.rep & JOY_UP) || (pTool->Pad1.on & JOY_SUP)) {
        if (pTool->cy == 0) {
            if (pTool->table_y != 0) {
                pTool->table_y--;
            } else {
                pTool->cy = pTool->table_height - 1;
                pTool->table_y = nLightWork - pTool->table_height - 1;
            }
        } else {
            pTool->cy--;
        }
    } else if ((pTool->Pad1.rep & JOY_DOWN) || (pTool->Pad1.on & JOY_SDOWN)) {
        if (pTool->cy < pTool->table_height - 1) {
            pTool->cy++;
        } else if (pTool->table_y < (int) (nLightWork - pTool->table_height - 1)) {
            pTool->table_y++;
        } else {
            pTool->table_y = 0;
            pTool->cy = 0;
        }
    }
    cur = curLight();
    if (!(cur->be_flag & 1)) {
        pTool->cx = 0;
    }
    if (pTool->Pad1.rep & JOY_A) {
        pTool->rno2 = pTool->cx + 1;
        pTool->rno3 = pTool->rno4 = pTool->rno5 = pTool->rno6 = pTool->rno7 = 0;
    } else if (pTool->Pad1.trg & JOY_Y) {
        pTool->clearSubMenu();
        pTool->rno2 += 20;
    } else if (pTool->Pad1.rep & JOY_B) {
        pTool->rno1 = 0;
        pTool->clearWork();
    }
    for (i = 0; i < nLightWork; i++) {
        cLight* l = LightMgr.at(i);
        if ((l->be_flag & 3) == 3) {
            if (l == cur) {
                u32 c = ((pG->Frame_cnt << 4) | 0xF) & 0xFF;
                drawLightInfo(l, c | (c << 24 | c << 16 | c << 8));
            } else {
                drawLightInfo(l, 0x40404040);
            }
        }
    }
}

// Light SUB MENU: CUT / COPY / INSERT / DELETE / PAGE on the cursor light (lightCutWork,
// lightInsertWork, the LitTmp clipboard) ; B closes.
static void edit_light_select_sub()
{
    static const char* light_sub_name[] = {
        "CUT", "COPY", "INSERT", "DELETE", "PAGE", "CUT", "COPY", "PASTE", "DELETE", "PAGE",
    };
    cLight* cur = curLight();
    u32 i;
    int y;
    int first;

    // First-entry init blocks carry a dead `first` flag set in both arms: the else arm makes the
    // block an if/else (the then arm ends in a jump, so cse cannot skip it), and the join block's
    // `pTool` load re-uses the block's own `lis LightToolPtr@ha` instead of the hoisted one.
    // A single dead set would be deleted by jump1 before cse; two sets survive until flow.
    if (pTool->sno0 == 0) {
        pTool->sno0 = 1;
        pTool->sno1 = pTool->cutNo;
        first = 1;
    } else {
        first = 0;
    }
    eprintf(0x140, 0x46, 4, pTool->PageNo, "SUB MENU");
    i = 0;
    y = 0x54;
    for (; i < 5; i++) {
        int idx = i;
        if (pTool->cx != 0) {
            idx = i + 5;
        }
        eprintf(0x140, y, 0, pTool->PageNo, light_sub_name[idx]);
        y += 14;
    }
    eprintf(0x168, 0x8C, pTool->editEnable() ? 0 : 0x16, pTool->PageNo, "%02d", pTool->cutNo);
    if (pTool->cutNo != pTool->sno1) {
        eprintf(0x180, 0x8C, pTool->editEnable() ? 0 : 0x16, pTool->PageNo, "--> %02d", pTool->sno1);
    }
    pTool->printCursor(0x27, pTool->curSub + 6);
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->curSub = (pTool->curSub + 4) % 5u;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->curSub = (pTool->curSub + 6) % 5u;
    }
    if ((pTool->Pad1.rep & JOY_A) || (pTool->Pad1.trg & JOY_Y)) {
        switch (pTool->curSub) {
        case 0:
            lightCopyWork(&pTool->LitTmp, cur);
            lightCutWork(pTool->table_y + pTool->cy);
            break;
        case 1:
            lightCopyWork(&pTool->LitTmp, cur);
            break;
        case 2:
            switch (pTool->cx) {
            case 0:
                if (pTool->LitTmp.be_flag & 1) {
                    lightInsertWork(pTool->table_y + pTool->cy);
                    LightMgr.cManager<cLight>::create(pTool->LitTmp.Type, pTool->table_y + pTool->cy);
                    lightCopyWork(cur, &pTool->LitTmp);
                }
                break;
            case 1:
                cur->Type = pTool->LitTmp.Type;
                cur->sub = pTool->LitTmp.sub;
                break;
            case 2:
                cur->xF = pTool->LitTmp.xF;
                break;
            case 3:
                cur->ParentType = pTool->LitTmp.ParentType;
                break;
            case 4:
                cur->Pos = pTool->LitTmp.Pos;
                break;
            case 5:
                cur->Radius = pTool->LitTmp.Radius;
                break;
            case 6:
                cur->Col = pTool->LitTmp.Col;
                break;
            case 7:
                cur->Intensity = pTool->LitTmp.Intensity;
                break;
            case 8:
                cur->xD = pTool->LitTmp.xD;
                cur->spot = pTool->LitTmp.spot;
                break;
            }
            cur->calcParent();
            break;
        case 3:
            lightCutWork(pTool->table_y + pTool->cy);
            break;
        case 4:
            if (pTool->editEnable()) {
                LitSaveWork(&pTool->Lit, pTool->cutNo);
            }
            pTool->cutNo = pTool->sno1;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            LitSaveWork(&pTool->Lit, pTool->cutNo);
            break;
        }
        pTool->rno2 -= 20;
    }
    if (pTool->curSub == 4) {
        u8 num = (pTool->CutNum < 60) ? 60 : pTool->CutNum;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            pTool->sno1 = (num + pTool->sno1 + 1) % num;
        } else if (pTool->Pad1.rep & JOY_LEFT) {
            pTool->sno1 = (num + pTool->sno1 - 1) % num;
        }
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 -= 20;
    }
}

// Copies every edited field of a light work (type, position, colour, parent, spot / sub / path
// blocks, display colour) into another work.
void lightCopyWork(cLight* dst, cLight* src)
{
    dst->be_flag = src->be_flag;
    dst->xC = src->xC;
    dst->xD = src->xD;
    dst->Type = src->Type;
    dst->xF = src->xF;
    dst->Pos = src->Pos;
    dst->Radius = src->Radius;
    dst->Col = src->Col;
    dst->Intensity = src->Intensity;
    dst->Kind = src->Kind;
    dst->Attribute = src->Attribute;
    dst->Priority = src->Priority;
    dst->HitRadius = src->HitRadius;
    dst->Dummy82 = src->Dummy82;
    dst->Dummy9 = src->Dummy9;
    dst->setParent(src->ParentType, src->ParentNo);
    dst->spot = src->spot;
    dst->sub = src->sub;
    dst->path = src->path;
    dst->Rno0 = src->Rno0;
    dst->Rno1 = src->Rno1;
    dst->Rno2 = src->Rno2;
    dst->Rno3 = src->Rno3;
    dst->DispCol = src->DispCol;
}

// Removes light `no`: every following light moves one slot down (recreated in place, copied).
void lightCutWork(int no)
{
    int i;

    for (i = no; i < (int) nLightWork - 2; i++) {
        cLight* p = LightMgr.at(i);
        cLight* n;
        if (p && p->isAlive()) {
            LightMgr.destroy(p);
        }
        n = LightMgr.at(i + 1);
        if (n && n->isAlive()) {
            cLightMgr* m = &LightMgr;
            m->cManager<cLight>::create(n->Type, i);
            lightCopyWork(p, n);
            LightMgr.destroy(n);
        }
    }
}

// Opens slot `no`: every light from it on moves one slot up (the last one is lost).
void lightInsertWork(int no)
{
    int i;

    for (i = nLightWork - 2; i >= no; i--) {
        cLight* p = LightMgr.at(i + 1);
        cLight* n;
        if (p && p->isAlive()) {
            LightMgr.destroy(p);
        }
        n = LightMgr.at(i);
        if (n && n->isAlive()) {
            cLightMgr* m = &LightMgr;
            m->cManager<cLight>::create(n->Type, i + 1);
            lightCopyWork(p, n);
            LightMgr.destroy(n);
        }
    }
}

// "No" column action: toggles the light's active bit (be_flag 2) or creates a default light in an
// empty slot (initLightWork); returns to the table.
static void edit_light_no()
{
    u32 no = pTool->table_y + pTool->cy;
    cLight* cur = LightMgr.at(no);

    if (cur->be_flag & 1) {
        cur->be_flag ^= 2;
    } else {
        initLightWork(LightMgr.cManager<cLight>::create(0, no));
    }
    pTool->rno2 = 0;
}

// Per-type work areas at cLight+0x78 (light01.cpp .. light08.cpp keep their own copies).
struct Light01Work {
    u8 pad_0[4];
    s8 range;  // 0x04
};
struct Light02Work {
    f32 base;  // 0x00
    f32 amp;   // 0x04
    f32 freq;  // 0x08
};
struct Light03Work {
    f32 rot[3];  // 0x00
};
struct Light04Work {
    u16 flags;      // 0x00  bit0: inverse texture, bit1: light position set, bit2: use texture
    u8 kind;        // 0x02  0 normal, 1..4 cast, 5 foot
    s8 gndDist;     // 0x03
    s16 rotX;       // 0x04  (parallel / fix)
    s16 rotY;       // 0x06
    u8 texNo;       // 0x08  (fix: range)
    u8 selfShd;     // 0x09
    u8 softShd;     // 0x0A
    u8 multiShd;    // 0x0B
    Vec lightPos;   // 0x0C  (fit)
    u8 range;       // 0x18
};
struct Light05Work {
    cLightPathData* pStart;  // 0x00
    cLightPathData* pCur;    // 0x04
    u8 flag;                 // 0x08  bit0 loop, bit1 inverse
    u8 pad_9[3];
    u8 pathNo;               // 0x0C
    u8 pathIdx;              // 0x0D
};
struct Light06Work {
    f32 start;  // 0x00
    f32 speed;  // 0x04
    f32 rate;   // 0x08
};
struct Light07Work {
    u8 pad_0[8];
    f32 x8;        // 0x08  (the third editor line writes here: a bug in the original)
    f32 speed[3];  // 0x0C
};

// Stick step of the numeric editors: A held multiplies it by 10.
#define STICK_STEP(scale) ((f32) pTool->Pad1.stickX * step / (scale))
#define STICK_MUL() f32 step = (pTool->Pad1.on & JOY_A) ? 10.0f : 1.0f

// LIGHT PROPATY / id (cLight::Type, the animation kind: NORMAL, FLICK, WAVE, SPOT ROTATE, SHADOW,
// PATH, FADE, SHINE, SPOT LOCK): left/right pick, A applies (sub work cleared), the kind's own
// editor (light_id_tbl) edits its parameters; B back.
static void edit_light_id()
{
    static void (*light_id_tbl[])() = {
        edit_light_id_normal, edit_light_id_flick, edit_light_id_wave, edit_light_id_round,
        edit_light_id_shadow, edit_light_id_path, edit_light_id_fade, edit_light_id_shine,
        edit_light_id_spotlock, edit_light_id_normal,
    };
    cLight* cur = curLight();
    int first;

    if (pTool->rno6 == 0) {
        pTool->rno7 = cur->Type;
        pTool->rno6 = 1;
        first = 1;
    } else {
        first = 0;
    }
    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->Type, light_id_name[cur->Type]);
    if (cur->Type != pTool->rno7) {
        eprintf(0xC0, 0x9A, 6, pTool->PageNo, "-> %d %s", pTool->rno7, light_id_name[pTool->rno7]);
    }
    light_id_tbl[cur->Type]();
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
        pLog->modeSet(0x18, 0x8C, 0x1E, 10);
    }
}

// NORMAL light: no parameters.
static void edit_light_id_normal()
{
    cLight* cur = curLight();

    if (pTool->Pad1.rep & JOY_RIGHT) {
        pTool->rno7 = (pTool->rno7 + 11) % 10;
    } else if (pTool->Pad1.rep & JOY_LEFT) {
        pTool->rno7 = (pTool->rno7 + 9) % 10;
    } else if (pTool->Pad1.rep & JOY_A) {
        if (pTool->rno7 != cur->Type) {
            cur->Type = pTool->rno7;
            clear_move_free();
        }
    }
}

// FLICK: the colour flicker range.
static void edit_light_id_flick()
{
    cLight* cur = curLight();
    Light01Work* w = (Light01Work*) cur->work;
    int n;
    int v;

    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        drawColorTile(0x60, 0xD2, 0x30, 0x30, *(u32*) &cur->Col);
        if (pTool->Pad1.rep & 0x20002) {
            v = w->range;
            if (pTool->Pad1.on & JOY_A) {
                n = v + 10;
            } else {
                n = v + 1;
            }
            w->range = n;
            if (n & 0x80) {
                w->range = 0;
            }
        }
        if (pTool->Pad1.rep & 0x10001) {
            v = w->range;
            if (pTool->Pad1.on & JOY_A) {
                n = v - 10;
            } else {
                n = v - 1;
            }
            w->range = n;
            if (n & 0x80) {
                w->range = 0x7F;
            }
        }
        break;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 1) % 2;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 3) % 2;
    }
    if (pTool->Pad1.trg & JOY_Y) {
        w->range = 0;
    }
    pTool->printCursor(7, pTool->rno3 + 11);
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "COLOR FLICK RANGE %d", w->range);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// WAVE: intensity wave centre / range / speed.
static void edit_light_id_wave()
{
    cLight* cur = curLight();
    Light02Work* w = (Light02Work*) cur->work;

    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        {
            STICK_MUL();
            w->base += STICK_STEP(10000.0f);
        }
        break;
    case 2:
        {
            STICK_MUL();
            w->amp += STICK_STEP(10000.0f);
        }
        break;
    case 3:
        {
            STICK_MUL();
            w->freq += STICK_STEP(10000.0f);
        }
        break;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 3) % 4;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 5) % 4;
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "CENTER %3.3f", w->base);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "RANGE  %3.3f", w->amp);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "SPEED   %3.3f", w->freq);
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// SPOT ROTATE: rotation speed per axis (Y resets to 0).
static void edit_light_id_round()
{
    cLight* cur = curLight();
    Light03Work* w = (Light03Work*) cur->work;

    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->Pad1.trg & JOY_Y) {
            w->rot[0] = 0.0f;
        }
        {
            STICK_MUL();
            w->rot[0] += STICK_STEP(10000.0f);
        }
        break;
    case 2:
        if (pTool->Pad1.trg & JOY_Y) {
            w->rot[1] = 0.0f;
        }
        {
            STICK_MUL();
            w->rot[1] += STICK_STEP(10000.0f);
        }
        break;
    case 3:
        if (pTool->Pad1.trg & JOY_Y) {
            w->rot[2] = 0.0f;
        }
        {
            STICK_MUL();
            w->rot[2] += STICK_STEP(10000.0f);
        }
        break;
    }
    w->rot[0] = LIMIT_ANGLE(w->rot[0]);
    w->rot[1] = LIMIT_ANGLE(w->rot[1]);
    w->rot[2] = LIMIT_ANGLE(w->rot[2]);
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 3) % 4;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 5) % 4;
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "ROT X %3.3f", w->rot[0]);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "ROT Y %3.3f", w->rot[1]);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "ROT Z %3.3f", w->rot[2]);
    eprintf(0x40, 0xEE, 0, pTool->PageNo, "PUSH [Y] TO SET 0.0");
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// SHADOW: shadow kind (NORMAL / CAST / CAST_ADD / CAST2 / CAST_ADD2 / FOOT), texture use / invert
// / number and ground distance.
static void edit_light_id_shadow()
{
    static const char* shadow_kind_name[] = {"NORMAL", "CAST", "CAST_ADD", "CAST2", "CAST_ADD2", "FOOT"};
    static const char* shadow_onoff[] = {"OFF", "ON"};
    cLight* cur = curLight();
    Light04Work* w;
    int step;
    u8 col;

    pLog->modeSet(0xC8, 0x8C, 0x1E, 10);
    w = (Light04Work*) cur->work;
    if ((*(u32*) &cur->Col & 0xFFFFFF00) == 0) {
        cur->Col.r = 0xFF;
        w->texNo = 0x5A;
    }
    if (cur->xD > 2) {
        cur->xD = 0;
    }
    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->Pad1.rep & JOY_RIGHT) {
            w->kind++;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            w->kind--;
        }
        if (w->kind & 0x80) {
            w->kind = 5;
        }
        if (w->kind > 5) {
            w->kind = 0;
        }
        break;
    case 2:
        if (w->kind - 1 > 3u) {
            if (pTool->Pad1.rep & JOY_RIGHT) {
                w->flags |= 1;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                w->flags &= ~1;
            }
        }
        break;
    case 3:
        if (pTool->Pad1.rep & JOY_RIGHT) {
            w->flags |= 4;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            w->flags &= ~4;
        }
        break;
    case 4:
        if (pTool->Pad1.trg & JOY_Y) {
            w->gndDist = 0;
        }
        step = (pTool->Pad1.on & JOY_A) ? 16 : 1;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            w->gndDist += step;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            w->gndDist -= step;
        }
        break;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 4) % 5;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 6) % 5;
    }
    col = 0x14;
    if (w->flags & 1) {
        col = 0;
    }
    if (w->kind - 1 <= 3u) {
        col = 0;
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "KIND   : %s", shadow_kind_name[w->kind]);
    if (w->kind == 5) {
        eprintf(0x40, 0xB6, 0x14, pTool->PageNo, "USE_TEX: ");
        eprintf(0x40, 0xC4, 0x14, pTool->PageNo, "INV_TEX:");
        eprintf(0x40, 0xC4, col, pTool->PageNo, "         %s", shadow_onoff[(w->flags >> 2) & 1]);
        eprintf(0x40, 0xD2, 0, pTool->PageNo, "GND_DIST:");
        eprintf(0x40, 0xD2, 0, pTool->PageNo, "          %2d", w->gndDist);
    } else {
        // COMPILER-DIFF: #2 -- the original zero-extends the u8 `col` once before this arm's two
        // uses (`clrlwi r30,r30,24`); ours knows the promoted value fits. The int copy + (u8) casts
        // reproduce the mask and the register assignment.
        int c = col;
        eprintf(0x40, 0xB6, 0, pTool->PageNo, "USE_TEX: ");
        eprintf(0x40, 0xC4, 0, pTool->PageNo, "INV_TEX:");
        eprintf(0x40, 0xC4, (u8) c, pTool->PageNo, "         %s", shadow_onoff[(w->flags >> 2) & 1]);
        eprintf(0x40, 0xD2, 0, pTool->PageNo, "TEX_NO :");
        eprintf(0x40, 0xD2, (u8) c, pTool->PageNo, "         %2x", (u8) w->gndDist);
    }
    if (w->kind - 1 <= 3u) {
        eprintf(0x40, 0xB6, 0x17, pTool->PageNo, "         ON");
    } else {
        eprintf(0x40, 0xB6, 0, pTool->PageNo, "         %s", shadow_onoff[w->flags & 1]);
    }
    eprintf(0x40, 0xEE, 0, pTool->PageNo, "PUSH [Y] TO SET 0");
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// PATH: the light path number (A edits the path itself in the path editor), loop and inverse flags.
static void edit_light_id_path()
{
    cLight* cur = curLight();
    Light05Work* w = (Light05Work*) cur->work;

    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->Pad1.rep & 0x20002) {
            w->pathNo++;
            cur->Rno0 = 0;
        }
        if (pTool->Pad1.rep & 0x10001) {
            w->pathNo--;
            cur->Rno0 = 0;
        }
        if (pTool->Pad1.rep & JOY_A) {
            cLightPathData* p = pTool->PathTool.Ofs[w->pathNo];
            if (VALID_PTR(p)) {
                memcpy(pTool->PathTool.Work, p, p->getSize());
                pTool->pno0 = pTool->pno1 = pTool->pno2 = pTool->pno3 = 0;
                pTool->rno3 = 100;
            }
        }
        break;
    case 2:
        if (pTool->Pad1.rep & (JOY_LEFT | JOY_RIGHT)) {
            w->flag ^= 1;
        }
        break;
    case 3:
        if (pTool->Pad1.rep & (JOY_LEFT | JOY_RIGHT)) {
            w->flag ^= 2;
        }
        break;
    case 100:
        if (pTool->Pad1.rep & JOY_B) {
            pTool->Pad1.rep &= ~JOY_B;
            pTool->rno3 = 1;
        }
        break;
    }
    if (pTool->rno3 < 100) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 3) % 4;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 5) % 4;
        }
        pTool->printCursor(7, pTool->rno3 + 11);
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "PATH No:%d", w->pathNo);
        eprintf(0x40, 0xB6, 0, pTool->PageNo, "LOOP    %s", (w->flag & 1) ? "OFF" : "ON");
        eprintf(0x40, 0xC4, 0, pTool->PageNo, "INVERSE %s", (w->flag & 2) ? "ON" : "OFF");
        drawLightInfo(cur, 0xFFFFFFFF);
        if (VALID_PTR(w->pStart)) {
            drawPath(0xC8, 0x64, w->pStart, w->flag, 0xFFFFFFFF);
        } else {
            eprintf(0xC8, 0xA8, 0, pTool->PageNo, "NO DATA");
        }
    } else {
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "EDIT PATH No:%d", w->pathNo);
        pathEdit(0x20, 0xC4, w->pathNo, w->flag, 0);
    }
}

// FADE: start value and speed, PLAY previews.
static void edit_light_id_fade()
{
    cLight* cur = curLight();
    Light06Work* w = (Light06Work*) cur->work;

    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->Pad1.rep & JOY_RIGHT) {
            ((Light06Work*) cur->work)->start += 0.1f;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            ((Light06Work*) cur->work)->start -= 0.1f;
        }
        if (pTool->Pad1.trg & JOY_Y) {
            ((Light06Work*) cur->work)->start = 0.0f;
        }
        break;
    case 2:
        if (pTool->Pad1.rep & JOY_RIGHT) {
            w->speed += 0.01f;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            w->speed -= 0.01f;
        }
        if (pTool->Pad1.trg & JOY_Y) {
            w->speed = 0.0f;
        }
        break;
    case 3:
        if (pTool->Pad1.rep & JOY_A) {
            cur->Rno0 = 0;
        }
        break;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 3) % 4;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 5) % 4;
    }
    pTool->printCursor(7, pTool->rno3 + 11);
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "START %f", w->start);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "SPEED %f", w->speed);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "PLAY");
    drawLightInfo(cur, 0xFFFFFFFF);
}

// SHINE: rotation speed per axis (Y resets to 0).
static void edit_light_id_shine()
{
    cLight* cur = curLight();
    Light07Work* w = (Light07Work*) cur->work;

    switch (pTool->rno3) {
    case 0:
        edit_light_id_normal();
        break;
    case 1:
        if (pTool->Pad1.trg & JOY_Y) {
            w->speed[0] = 0.0f;
        }
        {
            STICK_MUL();
            w->speed[0] += STICK_STEP(10000.0f);
        }
        break;
    case 2:
        if (pTool->Pad1.trg & JOY_Y) {
            w->speed[1] = 0.0f;
        }
        {
            STICK_MUL();
            w->speed[1] += STICK_STEP(10000.0f);
        }
        break;
    case 3:
        if (pTool->Pad1.trg & JOY_Y) {
            w->speed[2] = 0.0f;
        }
        {
            STICK_MUL();
            w->x8 += STICK_STEP(10000.0f);
        }
        break;
    }
    w->speed[0] = LIMIT_ANGLE(w->speed[0]);
    w->speed[1] = LIMIT_ANGLE(w->speed[1]);
    w->speed[2] = LIMIT_ANGLE(w->speed[2]);
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 3) % 4;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 5) % 4;
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "SPEED X %3.3f", w->speed[0]);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "SPEED Y %3.3f", w->speed[1]);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "SPEED Z %3.3f", w->speed[2]);
    eprintf(0x40, 0xEE, 0, pTool->PageNo, "PUSH [Y] TO SET 0.0");
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// SPOT LOCK: no parameters.
static void edit_light_id_spotlock() {}
// ENABLE MASK (cLight::xF): which model kinds the light affects (PLAYER, ENEMY, OBJ, EFFECT, SCROLL,
// ITEM, SUBCHAR, THERMO); up/down pick, A toggles, B back.
static void edit_light_eid()
{
    cLight* cur = curLight();

    pTool->printCursor(0x14, pTool->rno3 + 11);
    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "ENABLE MASK");
    eprintf(0xA8, 0x9A, !(cur->xF & 1) ? 0x14 : 0, pTool->PageNo, "PLAYER");
    eprintf(0xA8, 0xA8, (cur->xF & 2) ? 0 : 0x14, pTool->PageNo, "ENEMY");
    eprintf(0xA8, 0xB6, (cur->xF & 4) ? 0 : 0x14, pTool->PageNo, "OBJ");
    eprintf(0xA8, 0xC4, (cur->xF & 8) ? 0 : 0x14, pTool->PageNo, "EFFECT");
    eprintf(0xA8, 0xD2, (cur->xF & 0x10) ? 0 : 0x14, pTool->PageNo, "SCROLL");
    eprintf(0xA8, 0xE0, (cur->xF & 0x20) ? 0 : 0x14, pTool->PageNo, "ITEM");
    eprintf(0xA8, 0xEE, (cur->xF & 0x40) ? 0 : 0x14, pTool->PageNo, "SUBCHAR");
    eprintf(0xA8, 0xFC, (cur->xF & 0x80) ? 0 : 0x14, pTool->PageNo, "THERMO");
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 7) % 8;
    } else if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 9) % 8;
    }
    if (pTool->Pad1.rep & JOY_A) {
        switch (pTool->rno3) {
        case 0:
            cur->xF ^= 1;
            break;
        case 1:
            cur->xF ^= 2;
            break;
        case 2:
            cur->xF ^= 4;
            break;
        case 3:
            cur->xF ^= 8;
            break;
        case 4:
            cur->xF ^= 0x10;
            break;
        case 5:
            cur->xF ^= 0x20;
            break;
        case 6:
            cur->xF ^= 0x40;
            break;
        case 7:
            cur->xF ^= 0x80;
            break;
        }
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
    if (cur->xD == 7) {
        eprintf(0xE0, 0xC4, 0x16, pTool->PageNo, "<- NOT SUPPORT");
        cur->xF &= ~8;
    }
}

// PARENT: attaches the light to WORLD / ENEMY / SCROLL / EtcModel / OBJ, with the target's id and
// parts number (enemy names, object ids and event model names shown); B back.
static void edit_light_parent()
{
    static int parent_num = 5;
    cLight* cur = curLight();
    u32 num = 0;
    cModel* etc;
    cObj* obj;
    u32 n;
    cModel* m;
    int first;
    u32 no;

    if (pTool->rno3 == 0) {
        pTool->cursor = 0;
        pTool->rno4 = cur->ParentType;
        pTool->rno3 = 1;
        first = 1;
    } else {
        first = 0;
    }
    switch (pTool->cursor) {
    case 0:
        if (pTool->Pad1.rep & JOY_RIGHT) {
            pTool->rno4 = (parent_num + pTool->rno4 + 1) % parent_num;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            pTool->rno4 = (parent_num + pTool->rno4 - 1) % parent_num;
        }
        if (pTool->Pad1.rep & JOY_A) {
            posTranslate(cur, pTool->rno4, 0);
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno4 = cur->ParentType;
        }
        break;
    case 1:
        switch (cur->ParentType) {
        default:
            TOOL_ERR("INVALED LIGHT PARENT %d", cur->ParentType);
        case 0:
            n = 0;
            break;
        case 1:
            n = 250;
            break;
        case 2:
            n = 250;
            break;
        case 3:
            n = 0x40;
            break;
        case 4:
            n = ObjMgr.getArrayNum();
            break;
        }
        if (pTool->Pad1.rep & JOY_RIGHT) {
            cur->ParentNo = (cur->ParentNo & 0xFFFF0000) | ((n + (cur->ParentNo & 0xFFFF) + 1) % n);
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            cur->ParentNo = (cur->ParentNo & 0xFFFF0000) | ((n + (cur->ParentNo & 0xFFFF) - 1) % n);
        }
        break;
    case 2:
        n = 100;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            no = cur->ParentNo >> 16;
            no = (no + 101) % n;
            cur->ParentNo = (cur->ParentNo & 0xFFFF) | (no << 16);
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            no = cur->ParentNo >> 16;
            no = (no + 99) % n;
            cur->ParentNo = (cur->ParentNo & 0xFFFF) | (no << 16);
        }
        break;
    }
    cur->calcParent();
    eprintf(0x40, 0x8C, 4, pTool->PageNo, "PARENT");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "TYPE  %s", parent_name[cur->ParentType]);
    if (cur->ParentType != pTool->rno4) {
        eprintf(0xB8, 0x9A, 0, pTool->PageNo, "-> %s", parent_name[pTool->rno4]);
    }
    switch (cur->ParentType) {
    case 0:
        num = 1;
        break;
    case 1:
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "ID    %02X - %s", cur->parent.no, cEmMgr::idName[cur->parent.no]);
        num = 3;
        eprintf(0x40, 0xB6, 0, pTool->PageNo, "PARTS %d", cur->parent.partsNo);
        break;
    case 2:
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "ID    %d", cur->parent.no);
        num = 3;
        eprintf(0x40, 0xB6, 0, pTool->PageNo, "PARTS %d", cur->parent.partsNo);
        break;
    case 3:
        if (getRoomEtcOnLight(cur->ParentNo, &etc, 0)) {
            eprintf(0x40, 0xA8, 0, pTool->PageNo, "No    %d", cur->parent.no);
        } else {
            eprintf(0x40, 0xA8, 0x14, pTool->PageNo, "No    %d", cur->parent.no);
        }
        num = 2;
        break;
    case 4:
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "ID    %d", cur->parent.no);
        eprintf(0x40, 0xB6, 0, pTool->PageNo, "PARTS %d", cur->parent.partsNo);
        obj = ObjMgr.fastAt(cur->parent.no);
        if (obj) {
            switch (obj->id) {
            case 2:
                eprintf(0x40, 0xD2, 0, pTool->PageNo, "OBJID %02x : SCR MODEL", 2);
                break;
            case 0x18: {
                Obj18Work* w;
                eprintf(0x40, 0xD2, 0, pTool->PageNo, "OBJID %02x : EVENT MODEL", 0x18);
                w = (Obj18Work*) ((cObjUnion*) obj)->work;
                eprintf(0x40, 0xE0, 0, pTool->PageNo, "NAME %s", ((Obj18Work*) ((cObjUnion*) obj)->work)->NameMod);
                eprintf(0x40, 0xEE, 0, pTool->PageNo, "TYPE %2d", w->obj18_type);
                break;
            }
            default:
                eprintf(0x40, 0xD2, 0, pTool->PageNo, "OBJID %02x", obj->id);
                break;
            }
        }
        num = 3;
        break;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->cursor = (num + pTool->cursor - 1) % num;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->cursor = (num + pTool->cursor + 1) % num;
    }
    pTool->printCursor(7, pTool->cursor + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
    m = cur->getCoord();
    if (m) {
        f32 r = cur->Radius * (f32) (pG->Frame_cnt % 30) / 0.1f;
        Draw_sphere(&m->world, r, 0xA0A0A0FF, 1, 1);
        Draw_pos(&m->world, (int) r);
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = pTool->rno3 = 0;
        pTool->cursor = 0;
    }
}

// Re-parents the light, keeping its world position: the applied position is converted into the new
// parent's coordinates.
void posTranslate(cLight* l, u8 type, u32 id)
{
    Vec pos = l->World;
    cModel* m;
    Mtx inv;

    l->setParent(type, id);
    if (type == 0) {
        l->Pos = pos;
    } else {
        m = l->getCoord();
        if (m) {
            PSMTXInverse(m->mat, inv);
            PSMTXMultVec(inv, &pos, &l->Pos);
        } else {
            l->Pos.x = 0.0f;
            l->Pos.y = 0.0f;
            l->Pos.z = 0.0f;
        }
    }
}

// Position: the stick moves the light on the XZ plane relative to the camera (A x7), the L/R
// triggers its height, the d-pad by 1 unit, X resets to the origin, Y puts it at the camera target;
// B back.
static void edit_light_pos()
{
    cLight* cur = curLight();
    f32 step = (pTool->Pad1.on & JOY_A) ? 7.0f : 1.0f;
    Vec* pos = &cur->Pos;

    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "%6.0f %6.0f %6.0f", cur->Pos.x, cur->Pos.y, cur->Pos.z);
    Vec v = {0.0f, 0.0f, 0.0f};
    v.x += (f32) pTool->Pad1.stickX * step;
    v.y += (f32) pTool->Pad1.stickY * step;
    moveOnPlaneXZ(&v, &v);
    v.y += (f32) pTool->Pad1.triggerRight * step * 0.5f;
    v.y -= (f32) pTool->Pad1.triggerLeft * step * 0.5f;
    PSVECAdd(pos, &v, pos);
    if (pTool->Pad1.rep & JOY_UP) {
        cur->Pos.z += 1.0f;
    } else if (pTool->Pad1.rep & JOY_DOWN) {
        cur->Pos.z -= 1.0f;
    }
    if (pTool->Pad1.rep & JOY_RIGHT) {
        cur->Pos.x += 1.0f;
    } else if (pTool->Pad1.rep & JOY_LEFT) {
        cur->Pos.x -= 1.0f;
    }
    if (pTool->Pad1.rep & JOY_X) {
        cur->Pos = vecZero;
    }
    if (pTool->Pad1.trg & JOY_Y) {
        cur->Pos = pG->Camera.param.at;
    }
    drawLightInfo(cur, 0x80808080);
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// RADIUS (stick, A x7; infinite when 0) and HIT RADIUS (the on-model adjust distance); B back.
static void edit_light_radius()
{
    cLight* cur = curLight();
    f32 step = (pTool->Pad1.on & JOY_A) ? 7.0f : 1.0f;
    int r;

    switch (pTool->rno3) {
    case 0:
        pTool->cursor = 0;
        pTool->rno3 = 1;
    case 1:
        cur->Radius += (f32) pTool->Pad1.stickX * step * 0.5f;
        cur->Radius += (f32) pTool->Pad1.stickY * step * 0.5f;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            cur->Radius += 100.0f;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            cur->Radius -= 100.0f;
        }
        if (cur->Radius < 0.0f) {
            cur->Radius = 0.0f;
        }
        drawLightInfo(cur, 0xFFFFFFFF);
        break;
    case 2:
        r = (int) ((f32) pTool->Pad1.stickX * step * 0.5f + (f32) (int) cur->HitRadius);
        if (r < 0) {
            r = 0;
        }
        cur->HitRadius = r;
        drawLightInfo(cur, 0xFFFFFFFF);
        break;
    }
    pTool->printCursor(7, pTool->rno3 + 10);
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = 1;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = 2;
    }
    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    if (cur->Radius != 0.0f) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "RADIUS     %6.0f", cur->Radius);
    } else {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "RADIUS INFINITY");
    }
    if ((f32) (int) cur->HitRadius != 0.0f) {
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "HIT RADIUS %6d", cur->HitRadius);
    } else {
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "HIT RADIUS");
        eprintf(0x98, 0xA8, 0x14, pTool->PageNo, "NO HIT ADJUST");
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// Light colour through editColor; back when it closes.
static void edit_light_color()
{
    cLight* cur = curLight();

    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    if (editColor(8, 11, &cur->Col) == 0) {
        pTool->rno2 = 0;
    }
}

// INTENSITY (stick; not used by some types); B back.
static void edit_light_intensity()
{
    cLight* cur = curLight();
    f32 step = (Joy[0].on & JOY_A) ? 10.0f : 1.0f;

    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    if (cur->xD == 7) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "NOT USED");
        cur->Intensity = 1.0f;
    } else {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "INTENSITY %3.3f", cur->Intensity);
    }
    cur->Intensity += (f32) pTool->Pad1.stickX * step / 1000.0f;
    cur->Intensity += (f32) pTool->Pad1.stickY * step / 1000.0f;
    if (pTool->Pad1.rep & (JOY_UP | JOY_RIGHT)) {
        cur->Intensity += 0.1f;
    }
    if (pTool->Pad1.rep & (JOY_DOWN | JOY_LEFT)) {
        cur->Intensity -= 0.1f;
    }
    if (cur->Intensity < 0.1f) {
        cur->Intensity = 0.1f;
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// Attenuation type editor (cLight::xD: CONSTANT, LINEAR, QUADRATIC, SPOT LIGHT, CUSTOM, PARALLEL,
// SPOT QUAD, LOCAL AMBIENT) through light_type_tbl; SHADOW lights (Type 4) get the shadow editor.
static void edit_light_type()
{
    static void (*light_type_tbl[16])() = {
        edit_light_type_constant, edit_light_type_constant, edit_light_type_quad, edit_light_type_spotlight,
        edit_light_type_direct, edit_light_type_parallel, edit_light_type_spotlight, edit_light_type_localamb,
        edit_light_type_constant, edit_light_type_constant, edit_light_type_constant, edit_light_type_constant,
        edit_light_type_constant, edit_light_type_constant, edit_light_type_constant, edit_light_type_constant,
    };
    cLight* cur = curLight();

    if (cur->Type == 4) {
        edit_light_type_shadow();
        return;
    }
    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    light_type_tbl[cur->xD]();
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// SHADOW PROPATY: shadow projection type NORMAL (fit) / PARALLEL / FIX through shadow_type_tbl.
void edit_light_type_shadow()
{
    cLight* cur = curLight();

    static void (*shadow_type_tbl[3])() = {
        edit_light_type_shadow_fit, edit_light_type_shadow_parallel, edit_light_type_shadow_fix,
    };

    eprintf(0x40, 0x8C, 4, pTool->PageNo, "SHADOW PROPATY");
    shadow_type_tbl[cur->xD]();
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// Shadow type row: left/right pick, A applies (spot block cleared); 1 while the shown type is the
// light's.
int shadow_select_type()
{
    cLight* cur = curLight();
    int first;

    if (pTool->rno4 == 0) {
        pTool->rno5 = cur->xD;
        pTool->rno4 = 1;
        first = 1;
    } else {
        first = 0;
    }
    if (pTool->Pad1.rep & JOY_RIGHT) {
        pTool->rno5 = (pTool->rno5 + 4) % 3;
    }
    if (pTool->Pad1.rep & JOY_LEFT) {
        pTool->rno5 = (pTool->rno5 + 2) % 3;
    }
    if (pTool->Pad1.rep & JOY_A) {
        cur->xD = pTool->rno5;
    }
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", pTool->rno5, shadow_type_name[pTool->rno5]);
    return 1;
}

// Shadow sub-editor lines shared by the three shadow types: self / soft / multi shadow levels.
#define SHADOW_LEVEL_EDIT(field, max)                       \
    if (pTool->Pad1.rep & JOY_RIGHT) {                       \
        w->field++;                                         \
    }                                                       \
    if (pTool->Pad1.rep & 0x20000) {                         \
        w->field++;                                         \
    }                                                       \
    if (w->field > max) {                                   \
        w->field = max;                                     \
    }                                                       \
    if (w->field != 0) {                                    \
        if (pTool->Pad1.rep & JOY_LEFT) {                    \
            w->field--;                                     \
        }                                                   \
        if (pTool->Pad1.rep & 0x10000) {                     \
            w->field--;                                     \
        }                                                   \
    }
#define SHADOW_MULTI_EDIT()                                 \
    if (pTool->Pad1.rep & JOY_RIGHT) {                       \
        w->multiShd = 1;                                    \
    }                                                       \
    if (pTool->Pad1.rep & 0x20000) {                         \
        w->multiShd = 1;                                    \
    }                                                       \
    if (w->multiShd != 0) {                                 \
        if (pTool->Pad1.rep & JOY_LEFT) {                    \
            w->multiShd = 0;                                \
        }                                                   \
        if (pTool->Pad1.rep & 0x10000) {                     \
            w->multiShd = 0;                                \
        }                                                   \
    }
#define SHADOW_ROT_EDIT(field)                                          \
    if (pTool->Pad1.trg & JOY_Y) {                                       \
        w->field = 0;                                                   \
    }                                                                   \
    w->field += (int) ((f32) pTool->Pad1.stickY * step / 100.0f);            \
    w->field += (int) ((f32) pTool->Pad1.stickX * step / 100.0f);            \
    while (w->field & 0x8000) {                                         \
        w->field += 360;                                                \
    }                                                                   \
    while (w->field > 359) {                                            \
        w->field -= 360;                                                \
    }

// NORMAL shadow: light position (set from the camera with A), self / soft / multi shadow levels and
// range.
static void edit_light_type_shadow_fit()
{
    cLight* cur = curLight();
    Light04Work* w = (Light04Work*) cur->work;
    f32 step = 5.0f;
    int ret = 1;
    int c;

    switch (pTool->rno3) {
    case 0:
        ret = shadow_select_type();
        break;
    case 1:
        if (pTool->Pad1.rep & JOY_A) {
            if (((Light04Work*) cur->work)->flags & 2) {
                ((Light04Work*) cur->work)->flags &= ~2;
            } else {
                ((Light04Work*) cur->work)->flags |= 2;
            }
        }
        break;
    case 2:
        w->lightPos.x += (f32) pTool->Pad1.stickX * step;
        w->lightPos.z -= (f32) pTool->Pad1.stickY * step;
        w->lightPos.y += (f32) pTool->Pad1.triggerRight * step * 0.5f;
        w->lightPos.y -= (f32) pTool->Pad1.triggerLeft * step * 0.5f;
        Draw_pos(&w->lightPos, 500);
        if (pTool->Pad1.trg & JOY_Y) {
            w->lightPos = cur->Pos;
        }
        break;
    case 3:
        SHADOW_LEVEL_EDIT(selfShd, 4);
        break;
    case 4:
        SHADOW_LEVEL_EDIT(softShd, 3);
        break;
    case 5:
        SHADOW_MULTI_EDIT();
        break;
    case 6:
        w->range += (int) ((f32) pTool->Pad1.stickY * step / 100.0f);
        w->range += (int) ((f32) pTool->Pad1.stickX * step / 100.0f);
        if (Joy[0].on & JOY_LEFT) {
            if (w->range != 0) {
                w->range--;
            }
        }
        if (Joy[0].on & JOY_RIGHT) {
            if (w->range <= 0x59) {
                w->range++;
            }
        }
        if (w->range > 0x80) {
            w->range = 0;
        }
        if (w->range > 0x5A) {
            w->range = 0x5A;
        }
        break;
    }
    DbgFlagOn(pG, DBG_DRAW_SH_TEX);
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 6) % 7;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 8) % 7;
        }
    }
    if (pTool->rno3 != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, shadow_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "LIT_POS SET ");
    if (w->flags & 2) {
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "                ON");
        c = 0;
    } else {
        eprintf(0x40, 0xA8, 0, pTool->PageNo, "                OFF");
        c = 0x14;
    }
    eprintf(0x40, 0xB6, (u8) c, pTool->PageNo, "LIGHT_POS %6f %6f %6f", w->lightPos.x, w->lightPos.y, w->lightPos.z);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "SELF_SHD %s", self_shd_name[w->selfShd]);
    eprintf(0x40, 0xD2, 0, pTool->PageNo, "SOFT_SHD %s", soft_shd_name[w->softShd]);
    if (w->multiShd == 0) {
        eprintf(0x40, 0xE0, 0, pTool->PageNo, "MULTI_SHD OFF");
    } else {
        eprintf(0x40, 0xE0, 0, pTool->PageNo, "MULTI_SHD ON");
    }
    eprintf(0x40, 0xEE, 0, pTool->PageNo, "RANGE     %d", w->range);
    eprintf(0x40, 0xFC, (u8) c, pTool->PageNo, " [Y_BUTTON] Position Reset");
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// PARALLEL shadow: direction angles, self / soft / multi shadow levels and range.
static void edit_light_type_shadow_parallel()
{
    cLight* cur = curLight();
    Light04Work* w = (Light04Work*) cur->work;
    f32 step = 5.0f;
    int ret = 1;

    switch (pTool->rno3) {
    case 0:
        ret = shadow_select_type();
        break;
    case 1:
        SHADOW_ROT_EDIT(rotX);
        break;
    case 2:
        SHADOW_ROT_EDIT(rotY);
        break;
    case 3:
        SHADOW_LEVEL_EDIT(selfShd, 4);
        break;
    case 4:
        SHADOW_LEVEL_EDIT(softShd, 3);
        break;
    case 5:
        SHADOW_MULTI_EDIT();
        break;
    case 6:
        w->range += (int) ((f32) pTool->Pad1.stickY * step / 100.0f);
        w->range += (int) ((f32) pTool->Pad1.stickX * step / 100.0f);
        if (Joy[0].on & JOY_LEFT) {
            if (w->range != 0) {
                w->range--;
            }
        }
        if (Joy[0].on & JOY_RIGHT) {
            if (w->range <= 0x59) {
                w->range++;
            }
        }
        if (w->range > 0x80) {
            w->range = 0;
        }
        if (w->range > 0x5A) {
            w->range = 0x5A;
        }
        break;
    }
    DbgFlagOn(pG, DBG_DRAW_SH_TEX);
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 6) % 7;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 8) % 7;
        }
    }
    if (pTool->rno3 != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, shadow_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "ROT_X : %d", w->rotX);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "ROT_Y : %d", w->rotY);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "SELF_SHD  %s", self_shd_name[w->selfShd]);
    eprintf(0x40, 0xD2, 0, pTool->PageNo, "SOFT_SHD  %s", soft_shd_name[w->softShd]);
    if (w->multiShd == 0) {
        eprintf(0x40, 0xE0, 0, pTool->PageNo, "MULTI_SHD OFF");
    } else {
        eprintf(0x40, 0xE0, 0, pTool->PageNo, "MULTI_SHD ON");
    }
    eprintf(0x40, 0xEE, 0, pTool->PageNo, "RANGE     %d", w->range);
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// FIX shadow: direction angles and the shadow texture number.
static void edit_light_type_shadow_fix()
{
    cLight* cur = curLight();
    Light04Work* w = (Light04Work*) cur->work;
    f32 step = 5.0f;
    int ret = 1;

    switch (pTool->rno3) {
    case 0:
        ret = shadow_select_type();
        break;
    case 1:
        SHADOW_ROT_EDIT(rotX);
        break;
    case 2:
        SHADOW_ROT_EDIT(rotY);
        break;
    case 3:
        w->texNo += (int) ((f32) pTool->Pad1.stickY * step / 100.0f);
        w->texNo += (int) ((f32) pTool->Pad1.stickX * step / 100.0f);
        if (w->texNo > 0x80) {
            w->texNo = 0;
        }
        if (w->texNo > 0x5A) {
            w->texNo = 0x5A;
        }
        break;
    }
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 3) % 4;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 5) % 4;
        }
    }
    if (pTool->rno3 != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, shadow_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "ROT_X : %d", w->rotX);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "ROT_Y : %d", w->rotY);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "RANGE : %d", w->texNo);
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// KIND byte (LightMgr::offKind groups); left/right; B back.
static void edit_light_kind()
{
    cLight* cur = curLight();

    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "KIND  %02x", cur->Kind);
    if (pTool->Pad1.rep & JOY_RIGHT) {
        cur->Kind++;
    }
    if (pTool->Pad1.rep & JOY_LEFT) {
        cur->Kind--;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        cur->Kind += 0x10;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        cur->Kind -= 0x10;
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// ATTRIBUTE bits: NO TUNE, ELEC LIGHT, TAIMATU (torch); up/down, A toggles, B back.
static void edit_light_attr()
{
    cLight* cur = curLight();

    pTool->printCursor(0x14, pTool->rno3 + 11);
    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "ATTRIBUTE");
    eprintf(0xA8, 0x9A, !(cur->Attribute & 1) ? 0x14 : 0, pTool->PageNo, "NO TUNE");
    eprintf(0xA8, 0xA8, (cur->Attribute & 2) ? 0 : 0x14, pTool->PageNo, "ELEC LIGHT");
    eprintf(0xA8, 0xB6, (cur->Attribute & 4) ? 0 : 0x14, pTool->PageNo, "TAIMATU");
    eprintf(0xA8, 0xC4, (cur->Attribute & 8) ? 0 : 0x14, pTool->PageNo, "--------");
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->rno3 = (pTool->rno3 + 3) % 4;
    } else if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->rno3 = (pTool->rno3 + 5) % 4;
    }
    if (pTool->Pad1.rep & JOY_A) {
        switch (pTool->rno3) {
        case 0:
            cur->Attribute ^= 1;
            break;
        case 1:
            cur->Attribute ^= 2;
            break;
        case 2:
            cur->Attribute ^= 4;
            break;
        case 3:
            cur->Attribute ^= 8;
            break;
        }
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// PRIORITY (which lights survive the per-model limit); left/right; B back.
static void edit_light_priority()
{
    cLight* cur = curLight();

    eprintf(0x40, 0x8C, 4, pTool->PageNo, "LIGHT PROPATY");
    eprintf(0x40, 0x9A, 0, pTool->PageNo, "PRIORITY %d", cur->Priority);
    if (pTool->Pad1.rep & (JOY_UP | JOY_RIGHT)) {
        cur->Priority = (cur->Priority + 8) % 7;
    } else if (pTool->Pad1.rep & (JOY_DOWN | JOY_LEFT)) {
        cur->Priority = (cur->Priority + 6) % 7;
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno2 = 0;
    }
}

// Attenuation type row: left/right pick over the 8 types, A applies (spot block cleared); 1 while
// the shown type is the light's.
int select_type()
{
    cLight* cur = curLight();
    int ret;
    int col;
    int first;

    if (pTool->rno4 == 0) {
        pTool->rno5 = cur->xD;
        pTool->rno4 = 1;
        first = 1;
    } else {
        first = 0;
    }
    if (pTool->Pad1.rep & JOY_RIGHT) {
        pTool->rno5 = (pTool->rno5 + 9) % 8;
    }
    if (pTool->Pad1.rep & JOY_LEFT) {
        pTool->rno5 = (pTool->rno5 + 7) % 8;
    }
    if (pTool->Pad1.rep & JOY_A) {
        if (cur->xD != pTool->rno5) {
            cur->xD = pTool->rno5;
            clear_type_free();
        }
    }
    if (cur->xD == pTool->rno5) {
        col = 0;
        ret = 1;
    } else {
        col = 6;
        ret = 0;
    }
    eprintf(0x40, 0x9A, col, pTool->PageNo, "%2d %s", pTool->rno5, light_type_name[pTool->rno5]);
    return ret;
}

// CONSTANT / LINEAR: type row and intensity.
static void edit_light_type_constant()
{
    cLight* cur = curLight();
    int ret = 1;

    switch (pTool->rno3) {
    case 0:
        ret = select_type();
        break;
    case 1: {
        f32 step = (Joy[0].on & JOY_A) ? 10.0f : 1.0f;
        cur->Intensity += (f32) pTool->Pad1.stickX * step / 10000.0f;
        cur->Intensity += (f32) pTool->Pad1.stickY * step / 10000.0f;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            cur->Intensity += 0.1f;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            cur->Intensity -= 0.1f;
        }
        if (cur->Intensity < 0.001f) {
            cur->Intensity = 0.001f;
        }
        break;
    }
    }
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 1) % 2;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 3) % 2;
        }
    }
    if (pTool->rno3 != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "INTENSITY %3.3f", cur->Intensity);
    pTool->printCursor(7, pTool->rno3 + 11);
}

// Quadratic / local ambient: the smooth-edge width in the spot block.
#define EDIT_SMOOTH_EDGE()                                                              \
    cLight* cur = curLight();                                                                   \
    LightSpot* sp = &cur->spot;                                                                 \
    int ret = 1;                                                                                \
                                                                                                \
    switch (pTool->rno3) {                                                                      \
    case 0:                                                                                     \
        ret = select_type();                                                                    \
        break;                                                                                  \
    case 1: {                                                                                   \
        f32 step = (pTool->Pad1.on & JOY_A) ? 5.0f : 1.0f;                                       \
        sp->Normal.x += (f32) pTool->Pad1.stickX * step;                                             \
        sp->Normal.x += (f32) pTool->Pad1.stickY * step;                                             \
        if (pTool->Pad1.trg & JOY_Y) {                                                           \
            sp->Normal.x = 0.0f;                                                                \
        }                                                                                       \
        break;                                                                                  \
    }                                                                                           \
    }                                                                                           \
    if (ret) {                                                                                  \
        if (pTool->Pad1.rep & JOY_UP) {                                                          \
            pTool->rno3 = (pTool->rno3 + 1) % 2;                                                \
        }                                                                                       \
        if (pTool->Pad1.rep & JOY_DOWN) {                                                        \
            pTool->rno3 = (pTool->rno3 + 3) % 2;                                                \
        }                                                                                       \
    }                                                                                           \
    if (pTool->rno3 != 0) {                                                                     \
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, light_type_name[cur->xD]);      \
    }                                                                                           \
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "SMOOTH EDGE %6f", sp->Normal.x);                      \
    pTool->printCursor(7, pTool->rno3 + 11);                                                    \
    drawLightInfo(cur, 0xFFFFFFFF);

// QUADRATIC: type row and the smooth edge value.
static void edit_light_type_quad()
{
    EDIT_SMOOTH_EDGE();
}

// SPOT LIGHT / SPOT QUAD: type row, cone direction (stick, drawn as a cone), range angle and smooth
// edge.
static void edit_light_type_spotlight()
{
    // direction editor scratch: the unit's first .bss object (a function static of the first function
    // with one; the .sym's "global" scope at .bss+0 cannot be told from a local S+A field of 0)
    static Vec spotRot;
    cLight* cur = curLight();
    LightSpot* sp = &cur->spot;
    int ret = 1;
    Mtx m;
    Vec pos;
    Vec dir;
    f32 step;

    switch (pTool->rno3) {
    case 0:
        ret = select_type();
        break;
    case 1:
        if (PSVECMag(&sp->Normal) < 0.9f) {
            sp->Normal.x = 0.0f;
            sp->Normal.y = 0.0f;
            sp->Normal.z = 1.0f;
        }
        spotRot.x = 0.0f;
        {
            f32 sx = -(f32) pTool->Pad1.stickX / 1000.0f;
            Vec* rot = &spotRot;
            rot->z = 0.0f;
            rot->y = sx;
            RotMatrix(m, rot);
            PSMTXMultVec(m, &sp->Normal, &sp->Normal);
            PSVECCrossProduct(&sp->Normal, (const Vec*) yAxis, rot);
            PSMTXRotAxisRad(m, rot, (f32) pTool->Pad1.stickY / 1000.0f);
            PSMTXMultVec(m, &sp->Normal, &sp->Normal);
        }
#line 2977 "D:/Bio4/Prog/db_light.cpp"
        VECNormalize(&sp->Normal, &sp->Normal);
        break;
    case 2: {
        step = (pTool->Pad1.on & JOY_A) ? 3.0f : 1.0f;
        sp->A0 += (f32) pTool->Pad1.stickY * step / 100.0f;
        sp->A0 += (f32) pTool->Pad1.stickX * step / 100.0f;
        if (sp->A0 < 1.0f) {
            sp->A0 = 1.0f;
        }
        if (sp->A0 > 90.0f) {
            sp->A0 = 90.0f;
        }
        spotRot.x = 0.0f;
        spotRot.y = 0.0f;
        spotRot.z = 0.0f;
        spotRot.x = atan2f(-sp->Normal.y, sp->Normal.z);
        break;
    }
    case 3: {
        step = (pTool->Pad1.on & JOY_A) ? 5.0f : 1.0f;
        sp->A1 += (f32) pTool->Pad1.stickX * step;
        sp->A1 += (f32) pTool->Pad1.stickY * step;
        if (pTool->Pad1.trg & JOY_Y) {
            sp->A1 = 0.0f;
        }
        break;
    }
    }
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 3) & 3;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 5) & 3;
        }
    }
    if (Joy[0].on & JOY_A) {
        f32 r;
        cur->getPos(&pos);
        cur->getNormal(&sp->Normal, &dir);
        if (cur->Radius == 0.0f) {
            r = 3000.0f;
        } else {
            r = cur->Radius;
        }
        PSVECScale(&dir, &dir, r);
        Draw_corn3(&pos, &dir, sp->A0, 0xFFFFFFFF);
    }
    if (pTool->rno3 != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "DIRECTION");
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "SPOT LIGHT RANGE %3.2f", sp->A0);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "SMOOTH EDGE %6f", sp->A1);
    pTool->printCursor(7, pTool->rno3 + 11);
    drawLightInfo(cur, 0xFFFFFFFF);
}

// Custom attenuation editor: A0..K2 of the spot block, stepped by the gear table.
#define EDIT_DIRECT_PARAM(field)                                              \
    sp->field += (f32) pTool->Pad1.stickX * gear_step[gear] * step;                \
    if (pTool->Pad1.trg & JOY_Y) {                                             \
        sp->field = 0.0f;                                                     \
    }

// CUSTOM: the raw GX attenuation coefficients A0..A2 / K0..K2 (stick, gear steps 1e-9..1e-3),
// direction; the attenuation graph is drawn.
static void edit_light_type_direct()
{
    static Vec rot;
    static int gear;
    static f32 gear_step[4] = {1e-9f, 1e-7f, 1e-5f, 1e-3f};
    cLight* cur = curLight();
    LightSpot* sp = &cur->spot;
    f32 step = (pTool->Pad1.on & JOY_A) ? 100.0f : 1.0f;
    int ret = 1;
    Mtx m;

    if (PSVECMag(&sp->Normal) < 0.9f) {
        sp->Normal.x = 0.0f;
        sp->Normal.z = 1.0f;
        sp->Normal.y = 0.0f;
    }
    switch (pTool->rno3) {
    case 0:
        ret = select_type();
        gear = 0;
        break;
    case 1:
        EDIT_DIRECT_PARAM(A0);
        break;
    case 2:
        EDIT_DIRECT_PARAM(A1);
        break;
    case 3:
        EDIT_DIRECT_PARAM(A2);
        break;
    case 4:
        EDIT_DIRECT_PARAM(K0);
        break;
    case 5:
        EDIT_DIRECT_PARAM(K1);
        break;
    case 6:
        EDIT_DIRECT_PARAM(K2);
        break;
    case 7:
        if (PSVECMag(&sp->Normal) < 0.9f) {
            sp->Normal.x = 0.0f;
            sp->Normal.z = 1.0f;
            sp->Normal.y = 0.0f;
        }
        rot.x = 0.0f;
        {
            f32 sx = -(f32) pTool->Pad1.stickX / 1000.0f;
            Vec* r = &rot;
            r->z = 0.0f;
            r->y = sx;
            RotMatrix(m, r);
            PSMTXMultVec(m, &sp->Normal, &sp->Normal);
            PSVECCrossProduct(&sp->Normal, (const Vec*) yAxis, r);
            PSMTXRotAxisRad(m, r, (f32) pTool->Pad1.stickY / 1000.0f);
            PSMTXMultVec(m, &sp->Normal, &sp->Normal);
        }
#line 3099 "D:/Bio4/Prog/db_light.cpp"
        VECNormalize(&sp->Normal, &sp->Normal);
        drawLightInfo(cur, 0xFFFFFFFF);
        break;
    }
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno3 = (pTool->rno3 + 7) % 8;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno3 = (pTool->rno3 + 9) % 8;
        }
    }
    if (pTool->Pad1.rep & JOY_RIGHT) {
        gear = (gear + 1) % 4;
    }
    if (pTool->Pad1.rep & JOY_LEFT) {
        gear = (gear + 3) % 4;
    }
    pTool->printCursor(7, pTool->rno3 + 11);
    if (pTool->rno3 != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "A0 %5.4f", sp->A0);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "A1 %5.4f", sp->A1);
    eprintf(0x40, 0xC4, 0, pTool->PageNo, "A2 %5.4f", sp->A2);
    eprintf(0x40, 0xD2, 0, pTool->PageNo, "K0 %5.4f", sp->K0);
    eprintf(0x40, 0xE0, 0, pTool->PageNo, "K1 %5.4f", sp->K1);
    eprintf(0x40, 0xEE, 0, pTool->PageNo, "K2 %5.4f", sp->K2);
    eprintf(0x40, 0xFC, 0, pTool->PageNo, "NORMAL");
    eprintf(0x40, 0x118, 0, pTool->PageNo, "GEAR %d", gear + 1);
    draw_light_graph(cur);
}

// LOCAL AMBIENT: type row and the smooth edge value.
static void edit_light_type_localamb()
{
    EDIT_SMOOTH_EDGE();
}

// Attenuation of a custom light at distance d (the angular term is evaluated at 0).
f32 func_attn(cLight* l, f32 d)
{
    LightSpot* s = &l->spot;
    f32 c = 0.0f;
    return (s->A0 + s->A1 * c + s->A2 * c) / (s->K0 + d * s->K1 + d * d * s->K2);
}

// Attenuation curve of a custom light: a gx x gy .. gw x gh graph, the player distance and 1000-unit marks.
// (the static names decide the gcse hash order of their `high` pseudos and thus the r20/r21
// assignment of the loop's gx/gy address registers: x0/y0 reproduce it, gx/gy do not)
void draw_light_graph(cLight* l)
{
    static f32 x0 = 180.0f;
    static f32 y0 = 250.0f;
    static f32 w0 = 300.0f;
    static f32 h0 = 200.0f;
    static f32 s0 = 100.0f;
    Vec a;
    Vec b;
    f32 scale;
    int i;
    f32 x;
    f32 t;
    f32 v;
    u32 lcol;

    if (l->Radius != 0.0f) {
        scale = l->Radius / w0;
    } else {
        scale = 10000000.0f / w0;
    }
    a.x = x0;
    a.y = y0;
    a.z = 0.0f;
    b.x = x0 + w0;
    b.y = y0;
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    a.x = x0;
    a.y = y0;
    a.z = 0.0f;
    b.x = x0;
    b.y = y0 - h0;
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    for (i = 1; i < (int) w0; i++) {
        v = func_attn(l, (f32) i * scale) * s0;
        if (v > h0) {
            v = h0;
        }
        a.x = x0 + (f32) i;
        a.y = y0 - v;
        a.z = 0.0f;
        v = func_attn(l, (f32) (i + 1) * scale) * s0;
        if (v > h0) {
            v = h0;
        }
        b.x = x0 + (f32) (i + 1);
        b.y = y0 - v;
        b.z = 0.0f;
        Draw_line(&a, &b, 0xE0E0E0E0);
    }
    a = pPL->pos;
    a.y += 1200.0f;
    x = GetDistance3(&l->Pos, &a);
    if (x < l->Radius || l->Radius == 0.0f) {
        t = x / scale;
        a.x = x0 + t;
        a.y = y0;
        a.z = 0.0f;
        b.x = x0 + t;
        b.y = y0 - h0;
        b.z = 0.0f;
        lcol = 0xFFFF0000;
    } else {
        a.x = x0 + w0;
        a.y = y0;
        a.z = 0.0f;
        b.x = x0 + w0;
        b.y = y0 - h0;
        b.z = 0.0f;
        lcol = 0xFF000080;
    }
    Draw_line(&a, &b, lcol);
    eprintf((int) x0 + 0x78, (int) y0 + 8, 0, pTool->PageNo, "%3.6f", func_attn(l, x));
    for (x = 1000.0f; x < l->Radius || l->Radius == 0.0f; x += 1000.0f) {
        a.x = x0 + x / scale;
        a.y = y0;
        a.z = 0.0f;
        b.x = x0 + x / scale;
        b.y = y0 - h0;
        b.z = 0.0f;
        Draw_line(&a, &b, 0x80808080);
    }
    eprintf((int) x0, (int) y0 + 8, 0, pTool->PageNo, "%1.6f", func_attn(l, 1.0f));
    v = func_attn(l, w0 * scale);
    {
        // COMPILER-DIFF: 2 + #17: the original's colour lives in r5 (a copy preference ours never gets)
        // and is zero-extended for the int argument before the nested call; the pin gives both.
        register int col5 asm("r5");
        col5 = 0;
        if (v > 0.04f) {
            col5 = 6;
        }
        eprintf((int) x0 + 0xE6, (int) y0 + 8, (u8) col5, pTool->PageNo, "%3.6f", func_attn(l, w0 * scale));
    }
}
// Parallel light: the direction is edited as two angles (static `ang`: x = pitch, y = yaw, z unused),
// converted back to the unit normal (scaled by 1e6 in the light).
static void edit_light_type_parallel()
{
    static Vec ang;
    const f32 k = 1000000.0f;  // pool order: 1e6 before the step constants
    cLight* cur = curLight();
    LightSpot* sp = &cur->spot;
    f32 step = (pTool->Pad1.on & JOY_A) ? 10.0f : 1.0f;
    int ret = 1;

    switch (pTool->rno3) {
    case 0: {
        Vec* a = &ang;
        f32 cx;
        f32 cz;
        pTool->cursor = 0;
        a->x = asinf(sp->Normal.y / 1000000.0f);
        cx = sp->Normal.x / 1000000.0f;
        cx = cx / cosf(a->x);
        cz = sp->Normal.z / 1000000.0f;
        cz = cz / cosf(a->x);
        a->y = atan2f(cx, cz);
        a->z = 0.0f;
        pTool->rno3 = 1;
    }
    case 1: {
        Vec* a = &ang;
        f32 c;
        f32 d;
        a->x = LIMIT_ANGLE(a->x);
        a->y = LIMIT_ANGLE(a->y);
        c = cosf(a->x);
        c *= sinf(a->y);
        c *= k;
        sp->Normal.x = c;
        sp->Normal.y = sinf(a->x) * k;
        d = cosf(a->x);
        d *= cosf(a->y);
        d *= k;
        sp->Normal.z = d;
        break;
    }
    }
    switch (pTool->cursor) {
    case 0:
        ret = select_type();
        break;
    case 1: {
        Vec* a = &ang;
        a->y += (f32) pTool->Pad1.stickX * step / 1000.0f;
        a->y += (f32) pTool->Pad1.stickY * step / 1000.0f;
        if (pTool->Pad1.trg & JOY_Y) {
            a->y = 0.0f;
        }
        break;
    }
    case 2:
        ang.x += (f32) pTool->Pad1.stickX * step / 1000.0f;
        ang.x += (f32) pTool->Pad1.stickY * step / 1000.0f;
        if (pTool->Pad1.trg & JOY_Y) {
            ang.x = 0.0f;
        }
        break;
    case 3:
        if (pTool->Pad1.rep & JOY_A) {
            sp->flags ^= 1;
        }
        break;
    case 4:
        step = (pTool->Pad1.on & JOY_A) ? 5.0f : 1.0f;
        sp->A1 += (f32) pTool->Pad1.stickX * step;
        sp->A1 += (f32) pTool->Pad1.stickY * step;
        if (pTool->Pad1.trg & JOY_Y) {
            sp->A1 = 0.0f;
        }
        break;
    }
    if (ret) {
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 4) % 5;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 6) % 5;
        }
    }
    pTool->printCursor(7, pTool->cursor + 11);
    if (pTool->cursor != 0) {
        eprintf(0x40, 0x9A, 0, pTool->PageNo, "%2d %s", cur->xD, light_type_name[cur->xD]);
    }
    eprintf(0x40, 0xA8, 0, pTool->PageNo, "DIR Y:%3.0f", ang.y * 180.0f / 3.1415927f);
    eprintf(0x40, 0xB6, 0, pTool->PageNo, "DIR X:%3.0f", ang.x * 180.0f / 3.1415927f);
    eprintf(0x40, 0xC4, (sp->flags & 1) ? 0 : 0x14, pTool->PageNo, "LOCAL DIR");
    eprintf(0x40, 0xD2, 0, pTool->PageNo, "SMOOTH EDGE %6f", sp->A1);
}
// Unused sub menu slot (rno2 21).
static void edit_light_prop_sub() {}
// Ambient colours of the cut: model / enemy+object / effect.
static void edit_ambient()
{
    static GXColor amb_copy = { 0, 0, 0, 0 };
    cLightEnv* env = LightMgr.getEnvPtr();
    int ret = 0;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "AMBIENT");
    switch (pTool->rno4) {
    case 0:
        pTool->rno4 = 1;
        pTool->rno5 = 0;
    case 1:
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno5 = (pTool->rno5 + 2) % 3;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno5 = (pTool->rno5 + 4) % 3;
        }
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno4 = 2;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->rno1 = pTool->rno2 = 0;
            pTool->clearWork();
        } else if (pTool->Pad1.trg & JOY_Y) {
            pTool->rno4 = 3;
            pTool->rno6 = 0;
        }
        break;
    case 2:
        switch (pTool->rno5) {
        case 0:
            ret = editColor(4, 8, &env->AmbientScr);
            break;
        case 1:
            ret = editColor(4, 8, &env->AmbientEm);
            break;
        case 2:
            ret = editColor(4, 8, &env->AmbientEsp);
            break;
        }
        if (ret == 0) {
            pTool->rno4 = 1;
        }
        break;
    case 3:
        eprintf(0x140, 0x46, 4, pTool->PageNo, "SUB MENU");
        eprintf(0x140, 0x54, 0, pTool->PageNo, "COPY");
        eprintf(0x140, 0x62, 0, pTool->PageNo, "PASTE");
        pTool->printCursor(0x27, pTool->rno6 + 6);
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno6 = (pTool->rno6 + 1) % 2;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno6 = (pTool->rno6 + 3) % 2;
        }
        if ((pTool->Pad1.rep & JOY_A) || (pTool->Pad1.trg & JOY_Y)) {
            switch (pTool->rno6) {
            case 0:
                switch (pTool->rno5) {
                case 0:
                    amb_copy = env->AmbientScr;
                    break;
                case 1:
                    amb_copy = env->AmbientEm;
                    break;
                case 2:
                    amb_copy = env->AmbientEsp;
                    break;
                }
                break;
            case 1:
                switch (pTool->rno5) {
                case 0:
                    env->AmbientScr = amb_copy;
                    break;
                case 1:
                    env->AmbientEm = amb_copy;
                    break;
                case 2:
                    env->AmbientEsp = amb_copy;
                    break;
                }
                break;
            }
            pTool->rno4 = 1;
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno4 = 1;
        }
        break;
    }
    eprintf(0x20, 0x38, 0, pTool->PageNo, "SCROLL");
    drawColorTile(0x60, 0x38, 0x30, 0xD, *(u32*) &env->AmbientScr);
    eprintf(0x20, 0x46, 0, pTool->PageNo, "EM+OBJ");
    drawColorTile(0x60, 0x46, 0x30, 0xD, *(u32*) &env->AmbientEm);
    eprintf(0x20, 0x54, 0, pTool->PageNo, "EFFECT");
    drawColorTile(0x60, 0x54, 0x30, 0xD, *(u32*) &env->AmbientEsp);
    pTool->printCursor(3, pTool->rno5 + 4);
}
// FOG page of the cut (edit_fog_common on cLightEnv::Fog).
static void edit_fog()
{
    cLightEnv* env = LightMgr.getEnvPtr();

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "FOG");
    edit_fog_common(&env->Fog);
}

// MIRROR FOG page (the fog of the mirror render).
static void edit_mirror_fog()
{
    cLightEnv* env = LightMgr.getEnvPtr();

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "MIRROR FOG");
    edit_fog_common(&env->MirrorFog);
}

// Fog rows: TYPE (GX fog kinds through fogTypeNext/Back), START, END (stick, A x20), COLOR
// (editColor), FAR PLAY ratio; B back to the EDIT WORK menu.
void edit_fog_common(LightFog* fog)
{
    f32 step = (pTool->Pad1.on & JOY_A) ? 20.0f : 1.0f;
    cLightEnv* env = LightMgr.getEnvPtr();

    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
    case 1:
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 4) % 5;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 6) % 5;
        }
        switch (pTool->cursor) {
        case 0:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                fog->Type = fogTypeNext(fog->Type);
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                fog->Type = fogTypeBack(fog->Type);
            }
            break;
        case 1:
            fog->Start += (f32) pTool->Pad1.stickX * step;
            break;
        case 2:
            fog->End += (f32) pTool->Pad1.stickX * step;
            break;
        case 3:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->rno2 = 2;
            }
        case 4:
            env->far_play_ratio += (f32) pTool->Pad1.stickX * step * 0.0001f;
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->far_play_ratio -= 0.1f;
            }
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->far_play_ratio += 0.1f;
            }
            if (env->far_play_ratio > 1.0f) {
                env->far_play_ratio = 1.0f;
            }
            if (env->far_play_ratio < 0.0f) {
                env->far_play_ratio = 0.0f;
            }
            break;
        }
        if (fog->End < fog->Start) {
            fog->End = fog->Start + 1.0f;
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
        }
        break;
    case 2:
        if (editColor(0x14, 10, &fog->Color) == 0) {
            pTool->cursor = 3;
            pTool->rno2 = 0;
        }
        break;
    }
    eprintf(0x20, 0x38, 0, pTool->PageNo, "TYPE     %s", strFogType(fog->Type));
    eprintf(0x20, 0x46, 0, pTool->PageNo, "START    %6.0f", fog->Start);
    eprintf(0x20, 0x54, 0, pTool->PageNo, "END      %6.0f", fog->End);
    eprintf(0x20, 0x62, 0, pTool->PageNo, "COLOR");
    eprintf(0x20, 0x70, 0, pTool->PageNo, "FAR PLAY %1.2f", env->far_play_ratio);
    drawColorTile(0x50, 0x62, 0x30, 0xE, *(u32*) &fog->Color);
    LightMgr.setFog();
}
// FOCUS (depth of field) rows: DIST, LEVEL, MODE (NEAR / FAR / FollowPL NEAR / FollowPL FAR); B back.
static void edit_focus()
{
    static const char* focus_mode_name[] = {"NEAR", "FAR", "FollowPL NEAR", "FollowPL FAR"};
    cLightEnv* env = LightMgr.getEnvPtr();
    f32 step = (pTool->Pad1.on & JOY_A) ? 10.0f : 1.0f;

    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
    case 1:
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 2) % 3;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 4) % 3;
        }
        switch (pTool->cursor) {
        case 0:
            env->FocusZ += (int) ((f32) pTool->Pad1.stickX * step);
            break;
        case 1:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->FocusLevel = (env->FocusLevel + 12) % 11u;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->FocusLevel = (env->FocusLevel + 10) % 11u;
            }
            break;
        case 2:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->FocusMode++;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->FocusMode--;
            }
            env->FocusMode &= 3;
            break;
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "FOCUS");
    eprintf(0x20, 0x38, 0, pTool->PageNo, "DIST %7d", env->FocusZ);
    eprintf(0x20, 0x46, 0, pTool->PageNo, "LEVEL %d", env->FocusLevel);
    eprintf(0x20, 0x54, 0, pTool->PageNo, "MODE  %s", focus_mode_name[env->FocusMode]);
}
// Contrast tone curve of the blur filter: axes, the (in, out) knee and the end segments.
void draw_tone_curve()
{
    static f32 sz = 0.25f;
    static f32 gamma2 = 0.25f;
    static f32 gamma3 = 0.1f;
    cLightEnv* env = LightMgr.getEnvPtr();
    Vec a;
    Vec b;
    f32 g;
    f32 in;
    f32 out;

    a.x = 0.0f; a.y = 0.0f; a.z = 0.0f; b.x = 0.0f; b.y = 0.0f; b.z = 0.0f;
    if ((u8) env->contrast[0] == 0) {
        return;
    }
    a.x = sz * 255.0f + 64.0f;
    a.y = 150.0f;
    b = a;
    b.x += sz * 255.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    b = a;
    b.y -= sz * 255.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    b = a;
    b.x += (f32) (int) (u8) env->contrast[2] * sz;
    b.y -= (f32) (int) (u8) env->contrast[2] * sz;
    Draw_line(&a, &b, 0xFFFFFFFF);
    g = 0.5f;
    if ((u8) env->contrast[0] == 2) {
        g = gamma2;
    }
    if ((u8) env->contrast[0] == 3) {
        g = gamma3;
    }
    a = b;
    out = (255.0f - (f32) (int) (u8) env->contrast[2]) / 255.0f;
    in = (f32) (int) (u8) env->contrast[1] / 255.0f;
    out = out - out * ((1.0f - g) * in);
    b.x += out * 255.0f * sz;
    b.y -= (255.0f - (f32) (int) (u8) env->contrast[2]) * sz;
    Draw_line(&a, &b, 0xFFFFFFFF);
    a = b;
    b.x = sz * 255.0f + 64.0f + sz * 255.0f;
    b.y = 150.0f - sz * 255.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
}
// Blur filter of the cut: type / rate / power and the contrast level / power / bias.
static void edit_blur()
{
    int step = (pTool->Pad1.on & JOY_A) ? 10 : 1;
    cLightEnv* env = LightMgr.getEnvPtr();
    f32 fstep = (pTool->Pad1.on & JOY_A) ? 1.0f : 0.1f;
    const char* type_name[] = {"NORMAL", "SPREAD", "ADD", "SUBTRACT"};
    f32 f;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "BLUR");
    if (pTool->cursor <= 2) {
        pTool->printCursor(3, pTool->cursor + 4);
    } else {
        pTool->printCursor(3, pTool->cursor + 6);
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->cursor = (pTool->cursor + 5) % 6;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->cursor = (pTool->cursor + 7) % 6;
    }
    switch (pTool->cursor) {
    case 0:
        env->blur_type += (u8) ((f32) pTool->Pad1.stickX * fstep);
        if (pTool->Pad1.rep & JOY_RIGHT) {
            env->blur_type += step;
        }
        if (env->blur_type > 2) {
            env->blur_type = 0;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            env->blur_type -= step;
        }
        if (env->blur_type > 2) {
            env->blur_type = 2;
        }
        break;
    case 1:
        f = (f32) env->blur_rate + (f32) pTool->Pad1.stickX * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 255.0f) {
            f = 255.0f;
        }
        env->blur_rate = f;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            env->blur_rate += step;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            env->blur_rate -= step;
        }
        break;
    case 2:
        env->blur_power += (s8) ((f32) pTool->Pad1.stickX * fstep * 0.2f);
        if (pTool->Pad1.rep & JOY_RIGHT) {
            env->blur_power += step;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            env->blur_power -= step;
        }
        break;
    case 3:
        f = (f32) (u8) env->contrast[0] + (f32) pTool->Pad1.stickX * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 3.0f) {
            f = 3.0f;
        }
        env->contrast[0] = (u8) f;
        if ((u8) env->contrast[0] != 3 && (pTool->Pad1.rep & JOY_RIGHT)) {
            env->contrast[0] += step;
        }
        if ((u8) env->contrast[0] != 0 && (pTool->Pad1.rep & JOY_LEFT)) {
            env->contrast[0] -= step;
        }
        if ((u8) env->contrast[0] > 3) {
            env->contrast[0] = 3;
        }
        break;
    case 4:
        f = (f32) (u8) env->contrast[1] + (f32) pTool->Pad1.stickX * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 255.0f) {
            f = 255.0f;
        }
        env->contrast[1] = (u8) f;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            env->contrast[1] += step;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            env->contrast[1] -= step;
        }
        break;
    case 5:
        f = (f32) (u8) env->contrast[2] + (f32) pTool->Pad1.stickX * fstep;
        if (f < 0.0f) {
            f = 0.0f;
        }
        if (f > 255.0f) {
            f = 255.0f;
        }
        env->contrast[2] = (u8) f;
        if (pTool->Pad1.rep & JOY_RIGHT) {
            env->contrast[2] += step;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            env->contrast[2] -= step;
        }
        break;
    }
    if (env->blur_type > 2) {
        env->blur_type = 0;
    }
    eprintf(0x20, 0x38, 0, pTool->PageNo, "TYPE   %s", type_name[env->blur_type]);
    if (env->blur_rate == 0) {
        eprintf(0x20, 0x46, 0, pTool->PageNo, "RATE   OFF");
    } else {
        eprintf(0x20, 0x46, 0, pTool->PageNo, "RATE   %d", env->blur_rate);
    }
    eprintf(0x20, 0x54, 0, pTool->PageNo, "POW    %d", env->blur_power);
    eprintf(0x20, 0x70, 4, pTool->PageNo, "CONTRAST");
    if ((u8) env->contrast[0] == 0) {
        eprintf(0x20, 0x7E, 0, pTool->PageNo, "LEVEL  OFF");
    } else {
        eprintf(0x20, 0x7E, 0, pTool->PageNo, "LEVEL  %d", (u8) env->contrast[0]);
    }
    eprintf(0x20, 0x8C, 0, pTool->PageNo, "POW    %d", (u8) env->contrast[1]);
    eprintf(0x20, 0x9A, 0, pTool->PageNo, "BIAS   %d", (u8) env->contrast[2]);
    if (pTool->Pad1.rep & JOY_B) {
        pTool->rno1 = 0;
    }
    draw_tone_curve();
    LightMgr.setEnv(env, -1);
}
// Mipmap settings of the cut: min / max LOD, LOD bias and anisotropy.
static void edit_mipmap()
{
    cLightEnv* env = LightMgr.getEnvPtr();
    f32 step = (pTool->Pad1.on & JOY_A) ? 0.01f : 0.001f;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "MIPMAP");
    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
        env->min_lod %= 10;
        env->max_lod %= 10;
    case 1:
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 3) % 4;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 5) % 4;
        }
        switch (pTool->cursor) {
        case 0:
            if ((pTool->Pad1.rep & JOY_RIGHT) && env->min_lod <= 4) {
                env->min_lod++;
            }
            if ((pTool->Pad1.rep & JOY_LEFT) && env->min_lod != 0) {
                env->min_lod--;
            }
            break;
        case 1:
            if ((pTool->Pad1.rep & JOY_RIGHT) && env->max_lod <= 4) {
                env->max_lod++;
            }
            if ((pTool->Pad1.rep & JOY_LEFT) && env->max_lod != 0) {
                env->max_lod--;
            }
            break;
        case 2:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->lod_bias += 1.0f;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->lod_bias -= 1.0f;
            }
            if (pTool->Pad1.trg & JOY_Y) {
                env->lod_bias = 0.0f;
            }
            env->lod_bias += (f32) pTool->Pad1.stickX * step;
            if (env->lod_bias < -4.0f) {
                env->lod_bias = -4.0f;
            }
            if (env->lod_bias > 3.99f) {
                env->lod_bias = 3.99f;
            }
            break;
        case 3:
            if ((pTool->Pad1.rep & JOY_RIGHT) && env->aniso <= 1) {
                env->aniso++;
            }
            if ((pTool->Pad1.rep & JOY_LEFT) && env->aniso != 0) {
                env->aniso--;
            }
            break;
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
        }
        break;
    }
    eprintf(0x20, 0x38, 0, pTool->PageNo, "MIN LOD %d", env->min_lod);
    eprintf(0x20, 0x46, 0, pTool->PageNo, "MAX LOD %d", env->max_lod);
    eprintf(0x20, 0x54, 0, pTool->PageNo, "LODBIAS %3.2f", env->lod_bias);
    eprintf(0x20, 0x62, 0, pTool->PageNo, "ANISO   %s", aniso_name[env->aniso]);
    LightMgr.setMipmap(env);
}
// Lit tune: the room / core switch, the three tune colours and the manager's colour blend rate.
static void edit_tune()
{
    static const char* tune_name[] = {"", "LIGHT", "AMBIENT", "EFFECT"};
    cLightEnv* env = LightMgr.getEnvPtr();
    u32 i;
    const char** name;
    f32 d;

    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->tuneOn &= ~1;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->tuneOn |= 1;
            }
            break;
        case 1:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->rno2 = 2;
            }
            break;
        case 2:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->rno2 = 3;
            }
            break;
        case 3:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->cursor = 0;
                pTool->rno2 = 4;
            }
            break;
        }
        d = (f32) pTool->Pad1.stickX * 0.0005f;
        LightMgr.m_ColBrendRate += (pTool->Pad1.on & JOY_A) ? d * 10.0f : d;
        if (LightMgr.m_ColBrendRate < 0.0f) {
            LightMgr.m_ColBrendRate = 0.0f;
        }
        if (LightMgr.m_ColBrendRate > 1.0f) {
            LightMgr.m_ColBrendRate = 1.0f;
        }
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 3) & 3;
        } else if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 5) & 3;
        }
        if (!(env->tuneOn & 1)) {
            pTool->cursor = 0;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        if (editColor(4, 0x14, &env->Tune[0]) == 0) {
            pTool->rno2 = 1;
        }
        break;
    case 3:
        if (editColor(4, 0x14, &env->Tune[1]) == 0) {
            pTool->rno2 = 1;
        }
        break;
    case 4:
        if (editColor(4, 0x14, &env->Tune[2]) == 0) {
            pTool->rno2 = 1;
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "LIT TUNE");
    eprintf(0x20, 0x38, !(env->tuneOn & 1) ? 0x14 : 0, pTool->PageNo, "ROOM");
    eprintf(0x40, 0x38, 0, pTool->PageNo, "/");
    eprintf(0x48, 0x38, (env->tuneOn & 1) ? 0x14 : 0, pTool->PageNo, "CORE");
    eprintf(0x78, 0x38, 0, pTool->PageNo, "%3.0f%%", LightMgr.m_ColBrendRate * 100.0f);
    name = tune_name;
    for (i = 0; i < 4; i++) {
        eprintf(0x20, 0x38 + i * 0xE, 0, pTool->PageNo, *name++);
    }
    if (env->tuneOn & 1) {
        drawColorTile(0x60, 0x49, 0x38, 8, *(u32*) &env->Tune[0]);
        drawColorTile(0x60, 0x57, 0x38, 8, *(u32*) &env->Tune[1]);
        drawColorTile(0x60, 0x65, 0x38, 8, *(u32*) &env->Tune[2]);
    } else {
        drawColorTile(0x60, 0x49, 0x38, 8, 0xC8C0F080);
        drawColorTile(0x60, 0x57, 0x38, 8, 0);
        drawColorTile(0x60, 0x65, 0x38, 8, 0);
    }
    LightMgr.setTune(env);
}
// TEV colour scale of the models and of the player.
static void edit_scale()
{
    static const char* scale_name[] = {"x1", "x2", "x4", "err"};
    cLightEnv* env = LightMgr.getEnvPtr();

    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
        break;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->tev_scale[0] = (env->tev_scale[0] + 4) % 3;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->tev_scale[0] = (env->tev_scale[0] + 2) % 3;
            }
            break;
        case 1:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->tev_scale[1] = (env->tev_scale[1] + 4) % 3;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->tev_scale[1] = (env->tev_scale[1] + 2) % 3;
            }
            break;
        }
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 1) & 1;
        } else if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 3) & 1;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "LIT TUNE");
    eprintf(0x20, 0x38, 0, pTool->PageNo, "MODEL  TEV SCALE %s", scale_name[env->tev_scale[0]]);
    eprintf(0x20, 0x46, 0, pTool->PageNo, "PLAYER TEV SCALE %s", scale_name[env->tev_scale[1]]);
    LightMgr.setEnv(env, -1);
}
// Fog interpolation frames.
static void edit_param()
{
    cLightEnv* env = LightMgr.getEnvPtr();

    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
        break;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->Hokan = (env->Hokan + 251) % 250;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->Hokan = (env->Hokan + 249) % 250;
            }
            break;
        }
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 0) % 1;
        } else if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 2) % 1;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    }
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "PARAMETER");
    eprintf(0x20, 0x38, 0, pTool->PageNo, "HOKAN %d", env->Hokan);
}
// Cloth wind of the cut: direction (set from the stick through the camera), power, frequency.
// Draws the wind as an arrow at the camera target.
static void edit_wind()
{
    cLightEnv* env = LightMgr.getEnvPtr();
    Vec stick;
    Vec a;
    Vec b;
    Vec c;
    Vec rot;

    switch (pTool->rno2) {
    case 0:
        pTool->cursor = 0;
        pTool->rno2 = 1;
        break;
    case 1:
        switch (pTool->cursor) {
        case 0:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->wind.direction++;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->wind.direction--;
            }
            CamStick2World(&pG->Camera, &Joy[0], &stick);
            if (Joy[0].on & 0xF0000) {
                env->wind.direction = (int) (atan2(stick.x, stick.z) * 127.0 / 3.14159265f);  // f32 PI widened: pool 0x400921FB60000000
            }
            break;
        case 1:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->wind.power++;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->wind.power--;
            }
            env->wind.power += (Joy[0].stickX + Joy[0].stickY) / 10;
            if (pTool->Pad1.trg & JOY_Y) {
                env->wind.power = 0;
            }
            break;
        case 2:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                env->wind.frequency++;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                env->wind.frequency--;
            }
            env->wind.frequency += (Joy[0].stickX + Joy[0].stickY) / 10;
            if (pTool->Pad1.trg & JOY_Y) {
                env->wind.frequency = 0;
            }
            break;
        }
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 2) % 3u;
        } else if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 4) % 3u;
        }
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno2 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    }
    env->wind.set();
    pPL->moveCloth();
    a = pG->Camera.param.pos;
    b = pG->Camera.param.at;
    PSVECSubtract(&b, &a, &b);
#line 4082 "D:/Bio4/Prog/db_light.cpp"
    VECNormalize(&b, &b);
    PSVECScale(&b, &b, 1000.0f);
    PSVECAdd(&a, &b, &b);
    a.x = b.x;
    a.y = b.y - 100.0f;
    a.z = b.z;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    c.x = 0.0f;
    c.z = 300.0f;
    c.y = 0.0f;
    rot.x = 0.0f;
    rot.y = (f32) env->wind.direction * 3.14159265f / 127.0f;
    rot.z = 0.0f;
    RotVector(&c, &rot, &c);
    PSVECAdd(&c, &a, &c);
    Draw_line3d(&a, &c, 0xFFFFFFFF, 0);
    Draw_line3d(&b, &c, 0xFFFFFFFF, 0);
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "WIND (CLOTH)");
    eprintf(0x20, 0x38, 0, pTool->PageNo, "DIRECTION %2.2f", (f32) env->wind.direction * 3.14159265f / 127.0f);
    eprintf(0x20, 0x46, 0, pTool->PageNo, "POWER     %3.2f", (f32) env->wind.power * 0.01f * 20.0f);
    eprintf(0x20, 0x54, 0, pTool->PageNo, "FREQUENCY %3.2f", (f32) env->wind.frequency * 0.01f * 1.0471976f);
}
// Light path table editor: select a path, then edit it.
static void path()
{
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "PATH EDIT");
    switch (pTool->rno1) {
    case 0:
        pTool->pno0 = pTool->pno1 = pTool->pno2 = pTool->pno3 = 0;
        pTool->rno1 = 1;
    case 1:
        pTool->rno7 = pathSelect(0x20, 0x70, 0, 0, 0);
        if (pTool->Pad1.rep & JOY_A) {
            pTool->pno0 = pTool->pno1 = pTool->pno2 = pTool->pno3 = 0;
            pTool->rno1 = 2;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->rno0 = pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        eprintf(0x20, 0x38, 0, pTool->PageNo, "PATH %d", pTool->rno7);
        pTool->rno6 = pathEdit(0x20, 0x70, pTool->rno7, 0, 0);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->pno0 = pTool->pno2 = pTool->pno3 = 0;
            pTool->pno1 = pTool->rno7;
            pTool->rno1 = 1;
        }
        break;
    }
}
// File number selector shared by the load / save pages: UP / DOWN step 0x10, LEFT / RIGHT step 1.
#define FILE_NO_SELECT()                     \
    if (pTool->Pad1.rep & JOY_UP) {           \
        pTool->cursor += 0x10;               \
    }                                        \
    if (pTool->Pad1.rep & JOY_DOWN) {         \
        pTool->cursor += 0xF0;               \
    }                                        \
    if (pTool->Pad1.rep & JOY_RIGHT) {        \
        pTool->cursor++;                     \
    }                                        \
    if (pTool->Pad1.rep & JOY_LEFT) {         \
        pTool->cursor += 0xFF;               \
    }                                        \
    pTool->cursor %= 0x100

// The event tool's names the load / save pages use (EventDebug+0x20 / +0x48, inside its pad_0).
struct EvtDebugNames {
    u8 pad_0[0x20];
    char name[0x28];  // 0x20
    char str[0x18];   // 0x48
};

// Load page: source select (room local / server, event, tool, core, item), then the file number.
// Odd editNo values load the file the even one selected.
static void load()
{
    static u32 evtKey;
    static int evtAction;
    static u32 evName[8];
    static char evStr[8];
    char path[0x100];
    void* evt;
    EvtDebugNames* ev;
    u32* key;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "LOAD");
    switch (pTool->rno1) {
    case 0:
        pTool->clearWork();
        pTool->rno1 = 1;
        pTool->cursor = (DbgFlagChk(pG, DBG_EVENT_TOOL)) ? 2 : 1;
    case 1:
        eprintf(0x20, 0x38, 0, pTool->PageNo, "ROOM LOCAL");
        eprintf(0x20, 0x46, 0, pTool->PageNo, "ROOM SERVER");
        eprintf(0x20, 0x54, 0, pTool->PageNo, "EVENT");
        eprintf(0x20, 0x62, 0, pTool->PageNo, "TOOL");
        eprintf(0x20, 0x70, 0, pTool->PageNo, "CORE");
        eprintf(0x20, 0x7E, 0, pTool->PageNo, "ITEM");
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 5) % 6;
        } else if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 7) % 6;
        }
        if (pTool->Pad1.rep & JOY_A) {
            switch (pTool->cursor) {
            case 0:
                pTool->rno1 = 2;
                pTool->cursor = 0;
                break;
            case 1:
                pTool->rno1 = 4;
                pTool->cursor = 0;
                break;
            case 2:
                pTool->rno1 = 12;
                ev = (EvtDebugNames*) &EvtDebug;
                key = evName;
                strcpy((char*) key, ev->name);
                strcpy(evStr, ev->str);
                if (EvtMgr.GetEvt(key, &evt) == 1) {
                    evtKey = ((Event*) evt)->NowCut;
                    if (evStr[0] == 's' || evStr[0] == 'S') {
                        evtAction = 0;
                    } else {
                        evtAction = 1;
                    }
                }
                pTool->cursor = (u8) evtKey;
                break;
            case 3:
                pTool->rno1 = 6;
                pTool->cursor = 0;
                break;
            case 4:
                pTool->rno1 = 8;
                pTool->cursor = 0;
                break;
            case 5:
                pTool->rno1 = 0x10;
                pTool->cursor = 0;
                break;
            }
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno0 = pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        FILE_NO_SELECT();
        if (Joy[0].on & JOY_Y) {
            eprintf(0x50, 0x70, 0x16, pTool->PageNo, "OLD VERSION");
        }
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 3;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 3:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->Lit.fileLoad(path)) {
            pTool->cutNo = pTool->EditCutNo;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            pTool->DataType = (pTool->cursor != 1) ? 1 : 2;
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 4:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->PageNo, "%s", path);
        FILE_NO_SELECT();
        if (Joy[0].on & JOY_Y) {
            eprintf(0x50, 0x70, 0x16, pTool->PageNo, "OLD VERSION");
        }
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 5;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 5:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->Lit.fileLoad(path)) {
            file_lock(path);
            pTool->cutNo = pTool->EditCutNo;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            pTool->DataType = (pTool->cursor != 1) ? 1 : 2;
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 6:
        sprintf(path, path_tool, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        FILE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 7;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 7:
        sprintf(path, path_tool, pTool->cursor);
        if (pTool->Lit.fileLoad(path)) {
            pTool->cutNo = 0;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            pTool->DataType = 4;
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 8:
        sprintf(path, path_core, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        FILE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 9;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 9:
        sprintf(path, path_core, pTool->cursor);
        if (pTool->Lit.fileLoad(path)) {
            file_lock(path);
            pTool->cutNo = 0;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            pTool->DataType = 3;
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 10:
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE OPEN ERROR");
        eprintf(0x20, 0x46, 0, pTool->PageNo, "PUSH BUTTON TO CONTINUE");
        if (pTool->Pad1.rep & (JOY_A | JOY_B)) {
            pTool->rno1 = 0;
        }
        break;
    case 12:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%03d", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->PageNo, "%s", path);
        FILE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 13;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 13:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        if (pTool->Lit.fileLoad(path)) {
            file_lock(path);
            pTool->cutNo = pTool->EditCutNo = 0;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            pTool->DataType = (pTool->cursor != 1) ? 1 : 2;
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 0x10:
        sprintf(path, path_item, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        FILE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 0x11;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 0x11:
        sprintf(path, path_item, pTool->cursor);
        if (pTool->Lit.fileLoad(path)) {
            pTool->cutNo = 0;
            LitLoadWork(&pTool->Lit, pTool->cutNo);
            pTool->DataType = 4;
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    }
}
// File number selector of the save pages: UP / RIGHT step 1, DOWN / LEFT step -1.
#define SAVE_NO_SELECT()                                 \
    if (pTool->Pad1.rep & (JOY_UP | JOY_RIGHT)) {         \
        pTool->cursor++;                                 \
    } else if (pTool->Pad1.rep & (JOY_DOWN | JOY_LEFT)) { \
        pTool->cursor += 0xFF;                           \
    }                                                    \
    pTool->cursor %= 0x100

// Save page: destination select, then the file number; odd editNo values write the file.
static void save()
{
    static u32 evtKey;
    static int evtAction;
    static u32 evName[8];
    static char evStr[8];
    char path[0x100];
    void* evt;
    EvtDebugNames* ev;
    u32* key;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "SAVE");
    switch (pTool->rno1) {
    case 0:
        pTool->clearWork();
        switch (pTool->DataType) {
        default:
            pTool->cursor = (DbgFlagChk(pG, DBG_EVENT_TOOL)) ? 2 : 1;
            break;
        case 4:
            pTool->cursor = 2;
            break;
        case 3:
            pTool->cursor = 3;
            break;
        }
        pTool->rno1 = 1;
    case 1:
        eprintf(0x20, 0x38, 0, pTool->PageNo, "ROOM LOCAL");
        eprintf(0x20, 0x46, 0, pTool->PageNo, "ROOM SERVER");
        eprintf(0x20, 0x54, 0, pTool->PageNo, "EVENT");
        eprintf(0x20, 0x62, 0, pTool->PageNo, "TOOL");
        eprintf(0x20, 0x70, 0, pTool->PageNo, "CORE");
        eprintf(0x20, 0x7E, 0, pTool->PageNo, "PATH");
        eprintf(0x20, 0x8C, 0, pTool->PageNo, "ITEM");
        pTool->printCursor(3, pTool->cursor + 4);
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->cursor = (pTool->cursor + 6) % 7;
        } else if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->cursor = (pTool->cursor + 8) % 7;
        }
        if (pTool->Pad1.rep & JOY_A) {
            switch (pTool->cursor) {
            case 0:
                pTool->rno1 = 2;
                pTool->cursor = pTool->DataType == 2;
                break;
            case 1:
                pTool->rno1 = 4;
                pTool->cursor = pTool->DataType == 2;
                break;
            case 2:
                pTool->rno1 = 14;
                ev = (EvtDebugNames*) &EvtDebug;
                key = evName;
                strcpy((char*) key, ev->name);
                strcpy(evStr, ev->str);
                if (EvtMgr.GetEvt(key, &evt) == 1) {
                    evtKey = ((Event*) evt)->NowCut;
                    if (evStr[0] == 's' || evStr[0] == 'S') {
                        evtAction = 0;
                    } else {
                        evtAction = 1;
                    }
                }
                pTool->cursor = (u8) evtKey;
                break;
            case 3:
                pTool->rno1 = 6;
                pTool->cursor = pTool->DataType == 2;
                break;
            case 4:
                pTool->rno1 = 8;
                pTool->cursor = pTool->DataType == 2;
                break;
            case 5:
                pTool->rno1 = 12;
                pTool->cursor = pTool->DataType == 2;
                break;
            case 6:
                pTool->rno1 = 0x10;
                pTool->cursor = pTool->DataType == 2;
                break;
            }
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno0 = pTool->rno1 = 0;
            pTool->clearWork();
        }
        break;
    case 2:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 3;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 3:
        sprintf(path, path_room_server, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        if (pTool->Lit.fileSave(path)) {
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 4:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->PageNo, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 5;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 5:
        sprintf(path, path_room_local, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no,
                pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        if (pTool->Lit.fileSave(path)) {
            file_unlock(path);
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 6:
        sprintf(path, path_tool, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 7;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 7:
        sprintf(path, path_tool, pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        if (pTool->Lit.fileSave(path)) {
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 8:
        sprintf(path, path_core, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->PageNo, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 9;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 9:
        sprintf(path, path_core, pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        if (pTool->Lit.fileSave(path)) {
            file_unlock(path);
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 10:
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE OPEN ERROR");
        eprintf(0x20, 0x46, 0, pTool->PageNo, "PUSH BUTTON TO CONTINUE");
        if (pTool->Pad1.rep & (JOY_A | JOY_B)) {
            pTool->rno1 = 0;
        }
        break;
    case 12:
        sprintf(path, path_litpath);
        eprintf(0x20, 0x54, 0x16, pTool->PageNo, "%s", path);
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 13;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 13:
        if (HDWrite(path_litpath, pLitPath, pLitPath->getSize())) {
            file_unlock(path);
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 14:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%03d", pTool->cursor);
        eprintf(0x20, 0x54, 0x16, pTool->PageNo, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 15;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 15:
        if (evtAction == 0) {
            sprintf(path, path_event, pG->stage_no, pG->room_no, evStr, evStr, pTool->cursor);
        } else {
            sprintf(path, path_event_action, evStr, evStr, pTool->cursor);
        }
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        pTool->Lit.preEventSave();
        if (pTool->Lit.fileSave(path)) {
            file_unlock(path);
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    case 0x10:
        sprintf(path, path_item, pTool->cursor);
        eprintf(0x20, 0x38, 0, pTool->PageNo, "FILE NO:%02x", pTool->cursor);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "%s", path);
        SAVE_NO_SELECT();
        if (pTool->Pad1.rep & JOY_A) {
            pTool->rno1 = 0x11;
        } else if (pTool->Pad1.rep & JOY_B) {
            pTool->clearWork();
            pTool->rno1 = 1;
        }
        break;
    case 0x11:
        sprintf(path, path_item, pTool->cursor);
        if (pTool->editEnable()) {
            LitSaveWork(&pTool->Lit, pTool->cutNo);
        }
        if (pTool->Lit.fileSave(path)) {
            pTool->rno0 = 0;
            pTool->rno1 = 0;
            pTool->clearWork();
        } else {
            pTool->rno1 = 10;
        }
        break;
    }
}
// Tool options: object move, cut select, elec power / path, analyze, kind on/off, player light mask,
// bounding box display.
static void option()
{
    static const char* onoff[] = {"OFF", "ON"};
    f32 step = (pTool->Pad1.on & JOY_A) ? 0.3f : 0.1f;
    cLightPathData* p;
    cLightPathData* q;
    u32 i;
    int c;

    eprintf(0x20, 0x2A, 4, pTool->PageNo, "OPTION");
    switch (pTool->rno1) {
    case 0:
        switch (pTool->rno2) {
        case 0:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->Flag ^= 1;
            }
            break;
        case 1:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->Flag ^= 8;
            }
            break;
        case 2:
            LightMgr.setElecPower(step * 0.01f * (f32) pTool->Pad1.stickX);
            if (pTool->Pad1.rep & JOY_RIGHT) {
                LightMgr.setElecPower(step);
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                LightMgr.setElecPower(-step);
            }
            if (pTool->Pad1.trg & JOY_Y) {
                step = (LightMgr.ElecPower == 1.0f) ? -1.0f : 1.0f;
                LightMgr.setElecPower(step);
            }
            break;
        case 3:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                pTool->rno3++;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                pTool->rno3--;
            }
            if (pTool->Pad1.rep & JOY_A) {
                LightMgr.setElecPower2(pTool->rno3, 1);
            }
            if (pTool->Pad1.trg & JOY_Y) {
                p = pTool->PathTool.Ofs[pTool->rno3];
                if (p != NULL) {
                    memcpy(pTool->PathTool.Work, p, p->getSize());
                    pTool->pno0 = pTool->pno1 = pTool->pno2 = pTool->pno3 = 0;
                    pTool->rno1 = 1;
                }
            }
            q = LightMgr.getPathPtr(pTool->rno3);
            if (VALID_PTR(q)) {
                drawPath(0x32, 0xFA, q, 0, 0xFFFFFFFF);
            } else {
                eprintf(0x32, 0xFA, 0, pTool->PageNo, "NO DATA");
            }
            break;
        case 4:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->Flag ^= 4;
            }
            break;
        case 5:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->rno1 = 2;
                pTool->rno2 = 0;
            }
            break;
        case 6:
            if (pTool->Pad1.rep & JOY_A) {
                switch (pPL->LightInfo.EnableMask) {
                case 1:
                    pPL->LightInfo.EnableMask = 2;
                    break;
                case 2:
                    pPL->LightInfo.EnableMask = 4;
                    break;
                case 4:
                    pPL->LightInfo.EnableMask = 8;
                    break;
                case 8:
                    pPL->LightInfo.EnableMask = 0x10;
                    break;
                case 0x10:
                    pPL->LightInfo.EnableMask = 0x40;
                    break;
                case 0x40:
                    pPL->LightInfo.EnableMask = 1;
                    break;
                }
            }
            break;
        case 7:
            if (pTool->Pad1.rep & JOY_A) {
                pTool->Flag ^= 0x10;
            }
            break;
        }
        if (pTool->Pad1.rep & JOY_UP) {
            pTool->rno2 = (pTool->rno2 + 7) % 8;
        }
        if (pTool->Pad1.rep & JOY_DOWN) {
            pTool->rno2 = (pTool->rno2 + 9) % 8;
        }
        eprintf(0x20, 0x38, 0, pTool->PageNo, "OBJ MOVE      %s", onoff[(pTool->Flag & 1) ? 1 : 0]);
        eprintf(0x20, 0x46, 0, pTool->PageNo, "CUT SELECT    %s", onoff[(pTool->Flag & 8) ? 1 : 0]);
        eprintf(0x20, 0x54, 0, pTool->PageNo, "ELEC POWER    %1.2f", LightMgr.ElecPower);
        eprintf(0x20, 0x62, 0, pTool->PageNo, "ELEC PATH     %d", pTool->rno3);
        eprintf(0x20, 0x70, 0, pTool->PageNo, "ANALYZE       %s", onoff[(pTool->Flag & 4) >> 2]);
        eprintf(0x20, 0x7E, 0, pTool->PageNo, "KIND ON/OFF");
        switch (pPL->LightInfo.EnableMask) {
        case 1:
            eprintf(0x20, 0x8C, 0, pTool->PageNo, "PL EMASK      PLAYER");
            break;
        case 2:
            eprintf(0x20, 0x8C, 0, pTool->PageNo, "PL EMASK      ENEMY");
            break;
        case 4:
            eprintf(0x20, 0x8C, 0, pTool->PageNo, "PL EMASK      OBJ");
            break;
        case 8:
            eprintf(0x20, 0x8C, 0, pTool->PageNo, "PL EMASK      EFFECT");
            break;
        case 0x10:
            eprintf(0x20, 0x8C, 0, pTool->PageNo, "PL EMASK      SCROLL");
            break;
        case 0x40:
            eprintf(0x20, 0x8C, 0, pTool->PageNo, "PL EMASK      SUBCHAR");
            break;
        }
        eprintf(0x20, 0x9A, 0, pTool->PageNo, "BB DISP       %s", onoff[(pTool->Flag & 0x10) ? 1 : 0]);
        pTool->printCursor(3, pTool->rno2 + 4);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno0 = 0;
        }
        break;
    case 1:
        pathEdit(0x32, 0xFA, pTool->rno3, 0, 0);
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno1 = 0;
        }
        break;
    case 2:
        eprintf(0x40, 0x8C, 4, pTool->PageNo, "KIND");
        for (i = 0; i < 32; i++) {
            eprintf(0x40 + i * 8, 0x9A, LightMgr.checkKind(i) ? 0 : 0x14, pTool->PageNo, "%d", i % 10);
        }
        if (pTool->Pad1.rep & JOY_RIGHT) {
            pTool->rno2++;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            pTool->rno2--;
        }
        if (pTool->Pad1.rep & JOY_A) {
            if (LightMgr.checkKind(pTool->rno2)) {
                LightMgr.offKind(pTool->rno2);
            } else {
                LightMgr.onKind(pTool->rno2);
            }
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->rno1 = 0;
        }
        c = (pG->Frame_cnt % 30 > 14) ? 0x14 : 0;
        eprintf((pTool->rno2 + 8) << 3, 0xA8, c, pTool->PageNo, "^");
        break;
    }
}
// Quit confirmation: YES leaves the tool (restoring the debug page colour), NO goes back to the menu.
static void quit()
{
    eprintf(0x20, 0x2A, 4, pTool->PageNo, "QUIT ?");
    if (pTool->rno1 == 0) {
        pTool->cursor = 1;
        pTool->rno1 = 1;
    }
    eprintf(0x30, 0x46, pTool->cursor == 0 ? 0 : 0x14, pTool->PageNo, "YES");
    eprintf(0x30, 0x54, pTool->cursor == 1 ? 0 : 0x14, pTool->PageNo, "NO");
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->cursor = 0;
    } else if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->cursor = 1;
    }
    if (pTool->Pad1.rep & JOY_A) {
        if (pTool->cursor == 0) {
            pTool->be_flag = 0;
            pG->debug_mode = pTool->PrintNoBak;
            pLog->modeReset();
            pTool->updateLit();
            pTool->clearWork();
            pTool->rno0 = pTool->rno1 = pTool->rno2 = pTool->rno3 = pTool->rno4 = pTool->rno5 = pTool->rno6 = pTool->rno7 = 0;
            pTool->sno0 = pTool->sno1 = pTool->sno2 = pTool->sno3 = 0;
        } else {
            pTool->clearWork();
            pTool->rno0 = 0;
        }
    }
    if (pTool->Pad1.rep & JOY_B) {
        pTool->clearWork();
        pTool->rno0 = 0;
    }
}
// The light table header: defined before the inline row printer so that its strings precede the
// row printer's strings in .rodata (an inline's string literals are emitted at parse time).
static const char* table_head[] = {
    "NO ID EMASK PA POSITION===== RAD COL INT TYPE KIND ATTR PR",
    "NO =======================================================",
};

// One row of the light table (first page of columns), written as a MACRO over printEditTable's own
// `x`, `y` and `c`: the target keeps ONE `x` pseudo (r30) for the outer `x = 4` and the row's columns,
// so the row is not an inline with its own locals. The E.. columns use the loop's `y` variable while
// the "%02d"/"P" columns read `0x150 + i * 14` (see printEditTable).
#define PRINT_EDIT_TAIL(l, Y, c)                                                                    \
    {                                                                                               \
        GXColor col;                                                                                \
                                                                                                    \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%s", (l->xF & 2) ? "E" : "-");                        \
        x++;                                                                                        \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%s", (l->xF & 4) ? "O" : "-");                        \
        x++;                                                                                        \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%s", (l->xF & 8) ? "E" : "-");                        \
        x++;                                                                                        \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%s", (l->xF & 0x10) ? "S" : "-");                     \
        x += 2;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%s", parent_short[l->ParentType]);                    \
        x += 3;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%4.0f %3.0f %4.0f", l->Pos.x / 1000.0f,               \
                l->Pos.y / 1000.0f, l->Pos.z / 1000.0f);                                            \
        x += 14;                                                                                    \
        if (l->Radius != 0.0f) {                                                                       \
            eprintf(x * 8, (Y), c, pTool->PageNo, "%3d", (int) (l->Radius / 1000.0f));                  \
        } else {                                                                                    \
            eprintf(x * 8, (Y), c, pTool->PageNo, "INF");                                            \
        }                                                                                           \
        x += 4;                                                                                     \
        if (l->Type == 4) {                                                                         \
            /* a four-member chain: `a` gets its own load, the re-evaluated rhs of the innermost   \
               assignment is the one load shared by b, g, r (a three-member chain reloads) */       \
            col.r = col.g = col.b = col.a = l->Col.r;                                             \
        } else {                                                                                    \
            col = l->Col;                                                                         \
        }                                                                                           \
        /* COMPILER-DIFF: candidate #18 (by-value aggregate view): the swatch's 4-byte temp at frame \
           offset 0 with `col` in the next slot (the editColor form) */                             \
        DrawTileV(x * 8 + 1, (Y) + 1, 0x16, 0xC, col);                                              \
        x += 4;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%1.1f", l->Intensity);                                    \
        x += 4;                                                                                     \
        if (l->Type != 4) {                                                                         \
            if (l->xD <= 7) {                                                                       \
                eprintf(x * 8, (Y), c, pTool->PageNo, "%s", light_type_short[l->xD]);               \
            } else {                                                                                \
                eprintf(x * 8, (Y), c, pTool->PageNo, "ERR!");                                       \
            }                                                                                       \
        } else {                                                                                    \
            if (l->xD <= 2) {                                                                       \
                eprintf(x * 8, (Y), c, pTool->PageNo, "%s", shadow_type_short[l->xD]);              \
            } else {                                                                                \
                eprintf(x * 8, (Y), c, pTool->PageNo, "ERR!");                                       \
            }                                                                                       \
        }                                                                                           \
        x += 5;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%s %02x", (l->Kind & 0x80) ? "E" : " ", l->Kind);     \
        x += 5;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%02X", l->Attribute);                                      \
        x += 5;                                                                                     \
        eprintf(x * 8, (Y), c, pTool->PageNo, "%d", l->Priority);                                         \
    }

// The light table: one row per light of the current cut.
void printEditTable()
{
    int page = pTool->cx > 11;
    int i;
    int y;
    int no;
    int x;
    u8 c;
    cLight* l;

    eprintf(0x20, 0x142, 4, pTool->PageNo, table_head[page]);
    for (i = 0, no = pTool->table_y; i < pTool->table_height; i++, no++) {
        l = LightMgr.at(no);
        x = 4;
        if (pTool->editEnable()) {
            if ((l->be_flag & 3) == 3) {
                c = (l->Type == 4) ? 5 : 0;
            } else {
                c = 0x14;
            }
        } else {
            c = 0x16;
        }
        eprintf(x * 8, 0x150 + i * 14, c, pTool->PageNo, "%02d", no);
        // The row's `y`: a SECOND giv of the same value, spelled so that cse cannot fold it into the
        // `0x150 + i * 14` pseudo above (loop.c combines the two givs: `mr r26,r23` for the "P" column's
        // expression, `mr r29,r23` for this variable).
        y = (i + 24) * 14;
        if (l->be_flag & 1) {
            if (page == 0) {
                eprintf(7 * 8, 0x150 + i * 14, c, pTool->PageNo, "%02d", l->Type);
                // COMPILER-DIFF: candidate #12 (gcse cprop): the target keeps `li r30,10` and `li r3,80`
                // as pseudos of the pre-diamond block (`x + 1` and the "P" call's r3 not folded); ours
                // const-propagates both into the "P" join block unless the two constants are laundered
                // after cse folded them (the launders are the last sets of the block: nothing to propagate,
                // and `t80` is not a single-set constant).
                x = 10;
                {
                    int t80 = x * 8;
                    asm("" : "+r"(t80));
                    asm("" : "+r"(x));
                    eprintf(t80, 0x150 + i * 14, c, pTool->PageNo, "%s", (l->xF & 1) ? "P" : "-");
                }
                x++;
                PRINT_EDIT_TAIL(l, y, c);
            }
        } else {
            eprintf(0x38, 0x150 + i * 14, 0x14, pTool->PageNo, "EMPTY WORK");
        }
    }
}

// Blinking ">" at text cell (x, y).
void cLightTool::printCursor(int x, int y)
{
    if (!(cursorCtr & 8)) {
        eprintf(x * 8, y * 14, 0, pTool->PageNo, ">");
    }
}

// White ambient for DrawTile: a struct returned by value (its temporary is the last frame slot).
static inline GXColor whiteCol()
{
    GXColor c;
    c.r = c.g = c.b = c.a = 0xFF;
    return c;
}

// Filled 2D rectangle in screen pixels (colour swatches of the editors).
void DrawTile(int x, int y, int w, int h, GXColor* color)
{
    GXColor col = *color;

    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    GXSetChanAmbColor(0, whiteCol());
    GXSetChanMatColor(0, col);
    Mtx44 proj;
    Mtx mtx;
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x, y, 2);
    GXPosition3s16(x + w, y, 2);
    GXPosition3s16(x + w, y + h, 2);
    GXPosition3s16(x, y + h, 2);
}

// Resets the menu cursor and table position.
void cLightTool::clearWork()
{
    cursor = 0;
    cy = 0;
    cx = 0;
}

// Resets the sub menu cursors.
void cLightTool::clearSubMenu()
{
    curSub = sno0 = sno1 = sno2 = sno3 = 0;
}

#ifdef DB_LIGHT_SET_LOG_MODE
// t_esp build: error logging on / off (the log switch of the tool and of the manager).
void cLightTool::setLogMode(int on)
{
    if (on == 1) {
        pTool->Flag |= 0x20;
    } else {
        pTool->Flag &= ~0x20;
    }
    LightMgr.m_logMode = on;
}
#endif

// Empty light file.
cDbLit::cDbLit()
{
    u32 i;

    nCut = 0;
    version = 0;
    nMaxLight = 0;
    for (i = 0; i < 256; i++) {
        pLh[i] = NULL;
    }
}

// Byte size of the cLit image this file would produce (header + offset table + every cut);
// updates nCut to the last used cut + 1.
u32 cDbLit::size()
{
    u32 i;
    u32 size;

    for (i = 0; i < 256; i++) {
        if (pLh[i]) {
            nCut = i + 1;
        }
    }
    size = nCut * 4 + 4;
    for (i = 0; i < nCut; i++) {
        if (pLh[i]) {
            size += pLh[i]->getSize();
        }
    }
    return size;
}

// Cut `no` (NULL when absent).
cLightEnv* cDbLit::getCut(u16 no)
{
    return pLh[no];
}

// Loads cut `no` of the file into the LightMgr: env + light works; an absent cut clears the env
// and returns 0.
int LitLoadWork(cDbLit* lit, int no)
{
    cLightEnv* env = lit->getCut(no);

    LightMgr.destroyAll();
    if (env != NULL) {
        LightMgr.setEnv(env, -1);
        LightMgr.loadLit(env->getLightWork(0), env->nLight);
        return 1;
    }
    memclr_asm(pLightEnv, sizeof(cLightEnv));
    return 0;
}

// Stores the LightMgr env + scroll light works into cut `no` of the file (reallocated).
int LitSaveWork(cDbLit* lit, int no)
{
    u32 n;

    if (lit->isCut(no)) {
        Debug_free(lit->getCut(no));
    }
    n = LightMgr.countScr();
    pLightEnv->nLight = n;
    lit->pLh[(u16) no] = (cLightEnv*) Debug_alloc(n * sizeof(cLightWork) + sizeof(cLightEnv), 1);
    *lit->getCut(no) = *pLightEnv;
    LightMgr.saveLit(lit->getCut(no)->getLightWork(0));
    return 1;
}

// Reads a .lit file from the host and expands it (init); 0 when missing.
int cDbLit::fileLoad(const char* path)
{
    cLit* buf;
    int ret;

    if (HDReadDebugAlloc(path, (void**) &buf, 1) == 0) {
        TOOL_ERR("cDbLit::fileLoad() FILE NOT FOUND [%s]", path);
        return 0;
    }
    buf->versionUp();
    ret = init(buf);
    Debug_free(buf);
    return ret;
}

// Expands a cLit image: one Debug-heap copy per cut (cut count, version, max light count kept).
int cDbLit::init(cLit* lit)
{
    u32 i;
    u32* tbl;

    if (!VALID_PTR(lit)) {
        TOOL_ERR("cDbLit::init() MEMORY ERROR");
        return 0;
    }
    for (i = 0; i < 256; i++) {
        if (pLh[i]) {
            Debug_free(pLh[i]);
        }
        pLh[i] = NULL;
    }
    *(u32*) this = *(u32*) lit;
    tbl = (u32*) (lit + 1);
    for (i = 0; i < nCut; i++) {
        u32 ofs = tbl[i];
        if (ofs) {
            cLightEnv* src = (cLightEnv*) ((u8*) lit + ofs);
            u32 size = src->nLight * sizeof(cLightWork) + sizeof(cLightEnv);
            cLightEnv* dst = (cLightEnv*) Debug_alloc(size, 1);
            memcpy(dst, src, size);
            pLh[i] = dst;
        } else {
            pLh[i] = (cLightEnv*) ofs;
        }
    }
    version = 0x2C;
    return 1;
}

// Drops every cut but 0 (event light files hold one cut).
void cDbLit::preEventSave()
{
    u32 i;

    for (i = 1; i < 256; i++) {
        if (pLh[i]) {
            Debug_free(pLh[i]);
            pLh[i] = NULL;
        }
    }
}

// Writes the file as a cLit image to the host path; 0 when empty or the write failed.
int cDbLit::fileSave(const char* path)
{
    u32 size;
    cLit* buf;
    int ret = 0;

    size = this->size();
    if (size == 0) {
        return 0;
    }
    buf = (cLit*) Debug_alloc(size, 1);
    if (buf == NULL) {
        return 0;
    }
    size = createLit(buf);
    if (size) {
        if (HDWrite(path, buf, size)) {
            ret = 1;
        }
    }
    Debug_free(buf);
    return ret;
}

// Serialises the cuts into a cLit image (header, offset table, cuts); returns the byte size.
u32 cDbLit::createLit(cLit* dst)
{
    u32 i;
    u32 ofs;
    u32 size;
    u32* tbl;
    u32 n;
    cLightEnv* c;

    if (!VALID_PTR(dst)) {
        TOOL_ERR("cDbLit::createLit() POINTER ERR %08X", dst);
        return 0;
    }
    version = 0x2C;
    nMaxLight = nCut = 0;
    for (i = 0; i < 256; i++) {
        if (pLh[i]) {
            nCut = i + 1;
            if (pLh[i]->nLight > nMaxLight) {
                nMaxLight = pLh[i]->nLight;
            }
        }
    }
    *(u32*) dst = *(u32*) this;
    tbl = (u32*) (dst + 1);
    ofs = nCut * 4 + 4;
    for (i = 0; i < nCut; i++) {
        c = pLh[i];
        if (c) {
            tbl[i] = ofs;
            ofs += sizeof(cLightEnv) + c->nLight * sizeof(cLightWork);
        } else {
            tbl[i] = (u32) c;
        }
    }
    dst = (cLit*) &tbl[nCut];
    size = nCut * 4 + 4;
    // the same `c` as the table loop: the memcpy argument ties it to r4 in both loops
    for (i = 0; i < nCut; i++) {
        c = pLh[i];
        if (c) {
            n = c->nLight * sizeof(cLightWork) + sizeof(cLightEnv);
            memcpy(dst, c, n);
            dst = (cLit*) ((u8*) dst + n);
            size += n;
        }
    }
    return size;
}

// RGBA editor at text cell (x, y): the colour is edited as floats (kept across calls) and written back
// every frame; Y held links R / G / B. Returns 0 when B leaves the editor.
int editColor(int x, int y, GXColor* col)
{
    static int state = 0;
    static f32 r;
    static f32 g;
    static f32 b;
    static f32 a;
    int ret = 1;
    f32 step;
    int link;
    GXColor c;

    switch (state) {
    case 0:
        r = col->r;
        g = col->g;
        b = col->b;
        a = col->a;
        state = 1;
    case 1:
        col->r = r;
        col->g = g;
        col->b = b;
        col->a = a;
        break;
    }
    step = (pTool->Pad1.on & JOY_A) ? 1.5f : 0.1f;
    link = pTool->Pad1.on & JOY_Y;
    pTool->printCursor(x - 1, y + pTool->cursor);
    eprintf(x << 3, y * 14, 0, pTool->PageNo, "R %3d", col->r);
    // the original copies the deferred .rodata template into a register once (`lwz r24`) and stores
    // that word into the swatch argument's copy temp before each DrawTile (a `{0,0,0,0}` initializer
    // folds to `li 0`); the by-value DrawTileV view gives the one reused 4-byte temp at frame offset 0
    // (its address is a fresh `addi r7,r1,8` per call) with `c` purged into the slot after it.
    GXColor black = blackTemplate;
    c.r = 0xFF;
    c.g = 0;
    c.b = 0;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->r >> 1, 6, c);
    y++;
    eprintf(x << 3, y * 14, 0, pTool->PageNo, "G %3d", col->g);
    c.r = 0;
    c.g = 0xFF;
    c.b = 0;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->g >> 1, 6, c);
    y++;
    eprintf(x << 3, y * 14, 0, pTool->PageNo, "B %3d", col->b);
    c.r = 0;
    c.g = 0;
    c.b = 0xFF;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->b >> 1, 6, c);
    y++;
    eprintf(x << 3, y * 14, 0, pTool->PageNo, "A %1.1f", (f32) col->a * 0.0078125f);
    c.r = 200;
    c.g = 200;
    c.b = 200;
    DrawTileV(((x + 6) << 3), y * 14 + 4, 0x80, 6, black);
    DrawTileV(((x + 6) << 3), y * 14 + 4, col->a >> 1, 6, c);
    y += 2;
    DrawTileV(((x + 8) << 3), y * 14, 0x2A, 0x2A, *col);
    if (link) {
        eprintf(x << 3, y * 14, 0, pTool->PageNo, "LINK");
    }
    y++;
    if (pTool->Pad1.on & JOY_A) {
        eprintf(x << 3, y * 14, 0, pTool->PageNo, "TURBO");
    }
    if (link) {
        if (pTool->Pad1.rep & JOY_RIGHT) {
            r = r + step * 10.0f;
            g = g + step * 10.0f;
            b = b + step * 10.0f;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            r -= step * 10.0f;
            g -= step * 10.0f;
            b -= step * 10.0f;
        }
        r += (f32) pTool->Pad1.stickX * step / 20.0f;
        if (r < 0.0f) {
            r = 0.0f;
        } else if (r > 255.0f) {
            r = 255.0f;
        }
        g += (f32) pTool->Pad1.stickX * step / 20.0f;
        if (g < 0.0f) {
            g = 0.0f;
        } else if (g > 255.0f) {
            g = 255.0f;
        }
        b += (f32) pTool->Pad1.stickX * step / 20.0f;
        if (b < 0.0f) {
            b = 0.0f;
        } else if (b > 255.0f) {
            b = 255.0f;
        }
    } else {
        switch (pTool->cursor) {
        case 0:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                r = r + step * 10.0f;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                r -= step * 10.0f;
            }
            r += (f32) pTool->Pad1.stickX * step / 20.0f;
            if (r < 0.0f) {
                r = 0.0f;
            } else if (r > 255.0f) {
                r = 255.0f;
            }
            break;
        case 1:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                g = g + step * 10.0f;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                g -= step * 10.0f;
            }
            g += (f32) pTool->Pad1.stickX * step / 20.0f;
            if (g < 0.0f) {
                g = 0.0f;
            } else if (g > 255.0f) {
                g = 255.0f;
            }
            break;
        case 2:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                b = b + step * 10.0f;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                b -= step * 10.0f;
            }
            b += (f32) pTool->Pad1.stickX * step / 20.0f;
            if (b < 0.0f) {
                b = 0.0f;
            } else if (b > 255.0f) {
                b = 255.0f;
            }
            break;
        case 3:
            if (pTool->Pad1.rep & JOY_RIGHT) {
                a = a + step * 10.0f;
            }
            if (pTool->Pad1.rep & JOY_LEFT) {
                a -= step * 10.0f;
            }
            a += (f32) pTool->Pad1.stickX * step / 20.0f;
            if (a < 0.0f) {
                a = 0.0f;
            } else if (a > 255.0f) {
                a = 255.0f;
            }
            break;
        }
    }
    if (pTool->cursor == 3 && (pTool->Pad1.trg & JOY_Y)) {
        a = 128.0f;
    }
    if (pTool->Pad1.rep & JOY_UP) {
        pTool->cursor = (pTool->cursor + 3) % 4;
    } else if (pTool->Pad1.rep & JOY_DOWN) {
        pTool->cursor = (pTool->cursor + 5) % 4;
    }
    if (!(pTool->Pad1.on & 0x30000)) {
        if (pTool->Pad1.trg & 0x80000) {
            pTool->cursor = (pTool->cursor + 3) % 4;
        }
        if (pTool->Pad1.trg & 0x40000) {
            pTool->cursor = (pTool->cursor + 5) % 4;
        }
    }
    if (pTool->Pad1.rep & JOY_B) {
        state = 0;
        ret = 0;
    }
    return ret;
}

const char* strFogType(int type)
{
    const char* s;

    switch (type) {
    case 0:
        s = "NONE";
        break;
    case 2:
        s = "LINEAR";
        break;
    case 4:
        s = "EXP";
        break;
    case 5:
        s = "EXP2";
        break;
    case 6:
        s = "REV EXP";
        break;
    case 7:
        s = "REV EXP2";
        break;
    default:
        s = "????";
        break;
    }
    return s;
}

// Next GX fog type in the editor's cycle 0 -> 2 -> 4 -> 5 -> 6 -> 7 -> 0 (none, linear, exp, exp2,
// reverse exp, reverse exp2).
int fogTypeNext(int type)
{
    switch (type) {
    case 0:
        return 2;
    case 2:
        return 4;
    case 4:
        return 5;
    case 5:
        return 6;
    case 6:
        return 7;
    case 7:
        return 0;
    }
    return 0;
}

// Previous fog type of the cycle.
int fogTypeBack(int type)
{
    switch (type) {
    case 0:
        return 7;
    case 2:
        return 0;
    case 4:
        return 2;
    case 5:
        return 4;
    case 6:
        return 5;
    case 7:
        return 6;
    }
    return 0;
}

// Default light: active, NORMAL, quadratic, all model kinds but effect / thermo, grey 0x80,
// radius 3000, intensity 1, world parent, priority 3, blocks cleared.
void initLightWork(cLight* l)
{
    l->be_flag |= 6;
    l->Type = 0;
    l->xD = 2;
    l->xF |= 0x57;
    l->Col.r = 0x80;
    l->Col.g = 0x80;
    l->Col.b = 0x80;
    l->Col.a = 0x80;
    l->Pos.x = 0.0f;
    l->Pos.y = 0.0f;
    l->Pos.z = 0.0f;
    l->Radius = 3000.0f;
    l->Intensity = 1.0f;
    l->setParent(0, 0);
    l->Kind = 0;
    l->Attribute = 0;
    l->Priority = 3;
    memclr_asm(&l->spot, sizeof(LightSpot));
    memclr_asm(&l->sub, 0x40);
    memclr_asm(&l->path, sizeof(LightPath));
    l->Rno0 = l->Rno1 = l->Rno2 = l->Rno3 = 0;
    l->DispCol.r = 0x80;
    l->DispCol.g = 0x80;
    l->DispCol.b = 0x80;
    l->DispCol.a = 0x80;
}

// Clears the current light's animation sub work (FLICK keeps its base colour).
void clear_move_free()
{
    cLight* cur = curLight();

    memclr_asm(&cur->sub, sizeof(LightSub));
    if (cur->Type == 1) {
        cur->sub.color = cur->Col;
    }
}

// Clears the current light's spot block (type change).
void clear_type_free()
{
    memclr_asm(&curLight()->spot, sizeof(LightSpot));
}

// Light cut of the current camera: 0 with Debug_flg[0] bit 25, the camera tool's camera number
// while it runs (bit 31 + menu 7), else the cLit's safe cut for CamCtrl's camera.
int getCutNo()
{
    int no;

    if (DbgFlagChk(pG, DBG_EVENT_TOOL)) {
        return 0;
    }
    if (DbgFlagChk(pG, DBG_TEST_MODE) && DebugMenuSelected == 7) {
        no = tcCurrentCameraNo();
    } else {
        int cam = CamCtrl.CurrentCameraNo();
        no = (*LightMgr.getLitPPtr())->getSafeCutNo(cam);
    }
    return (u8) no;
}

// Shadow light with a parallel (type 2) direction: draws its position and the shadow cone.
void drawLightInfo_SpotShadow(cLight* l, u32 color)
{
    Light04Work* w = (Light04Work*) l->work;
    Vec dir;
    Vec n;
    Vec rot;
    Vec pos;
    Vec axis = {0.0f, 1.0f, 0.0f};
    Mtx m;
    Mtx m2;
    f32 len;

    l->getPos(&pos);
    Draw_pos(&pos, 300);
    dir.x = 0.0f;
    dir.y = -1.0f;
    dir.z = 0.0f;
    PSVECNormalize(&dir, &dir);
    rot.x = (f32) (s16) (u16) w->rotX * 3.1415927f * 2.0f / 360.0f;
    rot.y = (f32) (s16) (u16) w->rotY * 3.1415927f * 2.0f / 360.0f;
    rot.z = 0.0f;
    PSMTXRotRad(m, 'x', rot.x);
    PSMTXRotAxisRad(m2, &axis, rot.y);
    PSMTXConcat(m2, m, m);
    PSMTXMultVecSR(m, &dir, &dir);
    l->getNormal(&dir, &n);
    {
        // COMPILER-DIFF: 5 (the original's interblock scheduler hoists Draw_corn2's `mr r3/r4` above
        // the `len == 0` branch; the laundered pointers put the copies there)
        Vec* pp = &pos;
        Vec* pn = &n;
        asm("" : "+r"(pp), "+r"(pn));
        len = l->Radius;
        if (len == 0.0f) {
            len = 2000.0f;
        }
        Draw_corn2(pp, pn, len, (f32) w->texNo, 0xFFFFFFFF);
    }
}
// Position sphere / hit radius / direction line of a light.
void drawLightInfo(cLight* l, u32 color)
{
    Vec pos;
    Vec t;
    Vec n;

    if ((*(u32*) &l->xC & 0x00FFFF00) == 0x00020400) {
        drawLightInfo_SpotShadow(l, color);
        return;
    }
    pos = l->World;
    if (l->Radius != 0.0f) {
        Draw_sphere(&pos, l->Radius, color, 1, 1);
    }
    if ((f32) (int) l->HitRadius != 0.0f) {
        Draw_sphere(&pos, (f32) l->HitRadius, 0xFFFF0044, 1, 1);
    }
    Draw_pos(&pos, 300);
    if (l->xD == 3 || l->xD == 4 || l->xD == 6) {
        l->getNormal(&l->normal, &n);
        PSVECScale(&n, &t, 500.0f);
        PSVECAdd(&t, &pos, &t);
        Draw_line3d(&pos, &t, (l->Col.a << 24) | (l->Col.r << 16) | (l->Col.g << 8) | l->Col.b, 0);
    }
}
// Path table: pick a path (LEFT / RIGHT), create / delete with A through a YES / NO prompt.
int pathSelect(int x, int y, u8 no, u8 flag, int mode)
{
    cLightPathData* p;
    cLightPathData* np;

    eprintf(x + 0x88, y - 0xE, 0, pTool->PageNo, "SELECT A PATH NO: %d", pTool->pno1);
    switch (pTool->pno0) {
    case 0:
        if (pTool->Pad1.rep & JOY_RIGHT) {
            pTool->pno1++;
            memset_asm(pTool->PathTool.Work, 0xFF, sizeof(pTool->PathTool.Work));
            p = pTool->PathTool.Ofs[pTool->pno1];
            if (p != NULL) {
                memcpy(pTool->PathTool.Work, p, p->getSize());
            }
            pTool->cx = 0;
        }
        if (pTool->Pad1.rep & JOY_LEFT) {
            pTool->pno1--;
            memset_asm(pTool->PathTool.Work, 0xFF, sizeof(pTool->PathTool.Work));
            p = pTool->PathTool.Ofs[pTool->pno1];
            if (p != NULL) {
                memcpy(pTool->PathTool.Work, p, p->getSize());
            }
            pTool->cx = 0;
        }
        if ((pTool->Pad1.rep & JOY_A) && pTool->PathTool.Ofs[pTool->pno1] == NULL) {
            pTool->Pad1.rep &= ~JOY_A;
            pTool->pno2 = 0;
            pTool->pno0 = 1;
        }
        if (pTool->Pad1.trg & JOY_Y) {
            pTool->pno2 = 0;
            pTool->pno0 = 1;
        }
        break;
    case 1:
        if (pTool->PathTool.Ofs[pTool->pno1] != NULL) {
            eprintf(0xA0, 0x54, 0, pTool->PageNo, "DELETE?");
            eprintf(0xB0, 0x62, pTool->pno2 == 0 ? 0x14 : 0, pTool->PageNo, "YES");
            eprintf(0xB0, 0x70, pTool->pno2 != 0 ? 0x14 : 0, pTool->PageNo, "NO");
            if (pTool->Pad1.rep & JOY_UP) {
                pTool->pno2 = 1;
            } else if (pTool->Pad1.rep & JOY_DOWN) {
                pTool->pno2 = 0;
            }
            if (pTool->Pad1.rep & JOY_A) {
                if (pTool->pno2 == 1) {
                    Debug_free(pTool->PathTool.Ofs[pTool->pno1]);
                    pTool->PathTool.Ofs[pTool->pno1] = NULL;
                    pTool->PathTool.createPath(pLitPath);
                }
                pTool->pno0 = 0;
            }
        } else {
            eprintf(0xA0, 0x54, 0, pTool->PageNo, "CREATE?");
            eprintf(0xB0, 0x62, pTool->pno2 == 0 ? 0x14 : 0, pTool->PageNo, "YES");
            eprintf(0xB0, 0x70, pTool->pno2 != 0 ? 0x14 : 0, pTool->PageNo, "NO");
            if (pTool->Pad1.rep & JOY_UP) {
                pTool->pno2 = 1;
            } else if (pTool->Pad1.rep & JOY_DOWN) {
                pTool->pno2 = 0;
            }
            if (pTool->Pad1.rep & JOY_A) {
                if (pTool->pno2 == 1) {
                    np = (cLightPathData*) Debug_alloc(2, 1);
                    pTool->PathTool.Ofs[pTool->pno1] = np;
                    np->data[0] = 200;
                    np->data[1] = 0xFF;
                    pTool->PathTool.Work[0] = 200;
                    pTool->PathTool.Work[1] = 0xFF;
                }
                pTool->pno0 = 0;
            }
        }
        if (pTool->Pad1.rep & JOY_B) {
            pTool->pno0 = 0;
        }
        break;
    }
    if (VALID_PTR(pTool->PathTool.Ofs[pTool->pno1])) {
        drawPath(x, y, (cLightPathData*) pTool->PathTool.Work, 0, 0xFFFFFFFF);
        eprintf(x + 0x140, y + 0x68, 0, pTool->PageNo, "%2.2fsec",
                (f32) (((cLightPathData*) pTool->PathTool.Work)->getSize() - 1) / 30.0f);
    } else {
        eprintf(x + 0x20, y + 0x2A, 0, pTool->PageNo, "NO DATA");
    }
    return pTool->pno1;
}
// Path editor: LEFT / RIGHT move along the steps (RIGHT past the end appends one), UP / DOWN and the
// stick set the brightness of the step, Y ends the path there.
int pathEdit(int x, int y, u8 no, u8 flag, int mode)
{
    static f32 val;
    cLightPathData* p;
    u32 size;

    if (pTool->pno0 == 0) {
        val = (f32) pTool->PathTool.Work[0];
        pTool->pno0 = 1;
    }
    if (pTool->Pad1.rep & 0x20002) {
        pTool->pno1++;
        if (pTool->PathTool.Work[pTool->pno1] == 0xFF) {
            pTool->PathTool.Work[pTool->pno1] = (pTool->pno1 != 0) ? pTool->PathTool.Work[pTool->pno1 - 1] : 0;
            pTool->PathTool.Work[pTool->pno1 + 1] = 0xFF;
        }
        val = (f32) pTool->PathTool.Work[pTool->pno1];
    }
    if (pTool->Pad1.rep & 0x10001) {
        if (pTool->pno1 != 0) {
            pTool->pno1--;
            val = (f32) pTool->PathTool.Work[pTool->pno1];
        }
    }
    if (pTool->Pad1.rep & JOY_UP) {
        val += (pTool->Pad1.on & JOY_A) ? 6.0f : 2.0f;
    }
    if (pTool->Pad1.rep & JOY_DOWN) {
        val -= (pTool->Pad1.on & JOY_A) ? 6.0f : 2.0f;
    }
    val += (f32) pTool->Pad1.stickY * ((pTool->Pad1.on & JOY_A) ? 0.15f : 0.04f);
    if (val < 0.0f) {
        val = 0.0f;
    } else if (val >= 200.0f) {
        val = 200.0f;
    }
    pTool->PathTool.Work[pTool->pno1] = (u8) val;
    if (pTool->PathTool.Ofs[no] != NULL) {
        Debug_free(pTool->PathTool.Ofs[no]);
        size = ((cLightPathData*) pTool->PathTool.Work)->getSize();
        pTool->PathTool.Ofs[no] = (cLightPathData*) Debug_alloc(size, 1);
        memcpy(pTool->PathTool.Ofs[no], pTool->PathTool.Work, size);
    }
    pTool->PathTool.createPath(pLitPath);
    if (pTool->Pad1.trg & JOY_Y) {
        pTool->PathTool.Work[pTool->pno1 + 1] = 0xFF;
    }
    if (VALID_PTR(pTool->PathTool.Ofs[no])) {
        drawPath(x, y, (cLightPathData*) pTool->PathTool.Work, 0, pTool->pno1);
        eprintf(x + 0x120, y + 0x68, 0, pTool->PageNo, "%3d%% %2.2f/%2.2f", (pTool->PathTool.Work[pTool->pno1] + 1) >> 1,
                (f32) pTool->pno1 / 30.0f, (f32) (((cLightPathData*) pTool->PathTool.Work)->getSize() - 1) / 30.0f);
    } else {
        eprintf(x + 0x20, y + 0x2A, 0, pTool->PageNo, "NO DATA");
    }
    return pTool->pno1;
}
// Brightness graph of a light path: axes, one bar per step (flag bit1 inverts), step `cur` highlighted.
void drawPath(int x, int y, cLightPathData* p, u8 flag, u32 cur)
{
    Vec a;
    Vec b;
    u32 i;
    u32 xi;
    f32 f;
    u8* d = p->data;  // the data pointer steps (`lbzu`), `i` stays a counter for the `i == cur` tests

    a.x = (f32) x;
    a.y = (f32) (y - 10);
    a.z = 0.0f;
    b.x = (f32) x;
    b.y = (f32) (y + 110);
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    a.x = (f32) (x - 10);
    a.y = (f32) (y + 100);
    a.z = 0.0f;
    b.x = (f32) (x + 410);
    b.y = (f32) (y + 100);
    b.z = 0.0f;
    Draw_line(&a, &b, 0xFFFFFFFF);
    for (i = 0, xi = x; *d <= 200; xi += 4, i++, d++) {
        a.x = (f32) xi;
        a.y = (f32) y;
        a.z = 0.0f;
        b.x = (f32) xi;
        b.y = (f32) (y + 100);
        b.z = 0.0f;
        Draw_line(&a, &b, (i == cur) ? 0x40A0A0A0 : 0x40404040);
        f = (f32) *d * 0.5f;
        if (flag & 2) {
            f = 100.0f - f;
        }
        a.x = (f32) xi;
        a.y = (f32) (y + 100);
        a.z = 0.0f;
        b.x = (f32) xi;
        b.y = (f32) (y + 100) - f;
        b.z = 0.0f;
        Draw_line(&a, &b, (i == cur) ? 0xFFFF0000 : 0xFFA0A0A0);
    }
}


// Light usage analysis (Flag 4): per scroll object the lights hitting it (anaTbl: count, .., id),
// printed with the player's light count (red above 3); 0 without the table.
int cLightTool::lightAnalysis()
{
    u32 i;
    u32 n;

    if (!VALID_PTR(LitAnaWork)) {
        return 0;
    }
    n = ObjMgr.getArrayNum();
    memclr_asm(LitAnaWork, n * 4);
    LitAnaIdx = 0;
    for (i = 0; i < n; i++) {
        if (ObjMgr.fastAt(i)->isAlive()) {
            cObj* obj = ObjMgr.fastAt(i);
            if (obj->kindid == 2) {
                if (obj->LightInfo.getLightNum() > 4) {
                    int id = SmdGetWorkId(obj);
                    u8* p = (u8*) (LitAnaIdx * 4 + (u32) LitAnaWork);
                    if (id != -1) {
                        p[3] = id;
                    } else {
                        p[3] = 0xFF;
                    }
                    LitAnaWork[LitAnaIdx * 4] = obj->LightInfo.getLightNum();
                    LitAnaIdx++;
                }
            }
        }
    }
    {
        u32 pln = pPL->LightInfo.getLightNum();
        eprintf(0x1C8, 0x1C, 0, 0, "PL");
        eprintf(0x1E0, 0x1C, pln > 3 ? 0x16 : 0, 0, "%2d", pln);
    }
    for (i = 0; i < LitAnaIdx; i++) {
        if (LitAnaWork[i * 4 + 3] == 0xFF) {
            eprintf(0x1C8, 0x2A + i * 14, 0x16, 0, "-- %2d", LitAnaWork[i * 4]);
        } else {
            eprintf(0x1C8, 0x2A + i * 14, 0x16, 0, "%2d %2d", LitAnaWork[i * 4 + 3], LitAnaWork[i * 4]);
        }
    }
    return 1;
}

// Light path table start: copies the manager's path header to the Debug heap, expands the paths
// and loads path 0 into the edit buffer.
cLitPathTool::cLitPathTool()
{
    memclr_asm(Ofs, sizeof(Ofs));
    memclr_asm(Work, sizeof(Work));
    if (!(LightMgr.dbFlag & 2)) {
        cLightPathHeader* hdr;
        u32 size;
        LightMgr.dbFlag |= 2;
        hdr = (cLightPathHeader*) LightMgr.getPathHeader();
        size = hdr->getSize() + 0x2800;
        if (size <= 0xC7FF) {
            size = 0xC800;
        }
        pLitPath = (cLightPathHeader*) Debug_alloc(size, 0);
        if (!VALID_PTR(pLitPath)) {
            TOOL_ERR("cLitPathTool() Memory Alloc Failed");
            return;
        }
        memcpy(pLitPath, hdr, size);
        LightMgr.initPath((LightPathHeader*) pLitPath);
    } else {
        // through a reference: the store address (`lis LitPathPtr@ha`) is evaluated before the call
        // and kept in a callee-saved register; a plain store forms it after the call
        cLightPathHeader*& p = pLitPath;
        p = (cLightPathHeader*) LightMgr.getPathHeader();
    }
    expand(pLitPath);
    if (VALID_PTR(Ofs[0])) {
        memcpy(Work, Ofs[0], Ofs[0]->getSize());
    }
}

// Nothing to free here (the paths are released by the tool's quit).
cLitPathTool::~cLitPathTool()
{
}

// One Debug-heap copy per path of the header's offset table; 0 for a bad header.
int cLitPathTool::expand(cLightPathHeader* hdr)
{
    u32 i;

    if (!VALID_PTR(hdr)) {
        return 0;
    }
    memclr_asm(Ofs, sizeof(Ofs));
    for (i = 0; i < hdr->nPath; i++) {
        u32 ofs = ((u32*) (hdr + 1))[i];
        if (ofs) {
            cLightPathData* src = hdr->getPathData(i);
            u32 size = src->getSize();
            cLightPathData* dst = (cLightPathData*) Debug_alloc(size, 1);
            Ofs[i] = dst;
            memcpy(dst, src, size);
        } else {
            Ofs[i] = (cLightPathData*) ofs;
        }
    }
    return 1;
}

// Path header as the tool writes it: count, then the offset table (cLightPathHeader keeps the table
// implicit; the tool indexes it).
struct LitPathHdr {
    u8 num;      // 0x00
    u8 pad_1[3];
    u32 ofs[1];  // 0x04
};

// Serialises the paths back into a header + offset table + data image.
int cLitPathTool::createPath(cLightPathHeader* dst)
{
    LitPathHdr* h = (LitPathHdr*) dst;
    u32 i;
    u8* p;

    h->pad_1[0] = h->pad_1[1] = h->pad_1[2] = h->num = 0;
    for (i = 0; i < 256; i++) {
        if (Ofs[i]) {
            h->num = i + 1;
        }
    }
    p = (u8*) &h->ofs[h->num];
    for (i = 0; i < h->num; i++) {
        if (Ofs[i]) {
            u8* s = Ofs[i]->data;
            ((u32*) dst)[i + 1] = (u32) p - (u32) dst;
            *p = *s;
            while (*s != 0xFF) {
                s++;
                p++;
                *p = *s;
            }
            p++;
        } else {
            ((u32*) dst)[i + 1] = (u32) Ofs[i];
        }
    }
    return 1;
}

// the next object's .data is 8-aligned: the split object carries the 4-byte pad
asm(".section .data; .balign 8");
