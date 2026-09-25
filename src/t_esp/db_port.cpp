#include "types.h"
#include "db_widget.h"
#include "event.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "db_log.h"
#include "dbmodule.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "file.h"
#include "math_sub.h"
#include "vec.h"
#include "gx.h"
#include "gx_sub.h"
#include "scroll.h"
#include "em.h"
#include "esp.h"
#include "espgen.h"
#include "est.h"
#include "db_cam.h"
#include "db_mod.h"
#include "db_light.h"
#include "tools.h"
#include "t_util.h"
#include "scheduler.h"
#include "block.h"
#include "TexRender.h"
#include "main_sub.h"
#include "tpl.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dolphin/os.h>
#include "pl_mod.h"

// t_esp REL, D:/Bio4/Prog/db_port.cpp: the bridge between the effect tool (t_esp.cpp) and the game
// (model viewer, event debug data, light tool, drawing helpers, the sp_*_trans debug draw callbacks of
// the effect generators and the .cfg model set loader).

void DbMenuSetExecTool(const char* name);

// game/eprintf.cpp. Not in eprintf.h: a declaration of fontTexObj ahead of its definition reorders eprintf.cpp's
// .bss; fontTMtx is static there (the REL link resolves the local symbol).
extern GXTexObj fontTexObj;
extern Mtx fontTMtx;

// esp06 (path) work behind the cEsp
struct DbPathWork {
    u8 owner;         // 0x00
    u8 flags;         // 0x01  bit7: own matrix
    u16 id;           // 0x02
    u16 seg;          // 0x04
    u8 pad_06[2];
    void* path;       // 0x08
    u8 pad_0C[0x1C - 0x0C];
    Mtx mtx;          // 0x1C
};

struct DbPathEsp {
    cEsp esp;         // 0x00
    DbPathWork w;     // 0xF8
};

// Espgen02 (path generator) work fields behind EspgenWork::work
struct DbEspgen02 {
    u8 pad_0[0x19];
    u8 flags19;       // 0x19  bit2: scale
    u8 pad_1A[0x88 - 0x1A];
    u8 pathOwner;     // 0x88
    u8 pathId;        // 0x89
    u8 pad_8A[4];
    u8 rotX;          // 0x8E
    u8 rotY;          // 0x8F
    f32 scaleX;       // 0x90
    f32 scaleY;       // 0x94
    f32 scaleZ;       // 0x98
};

// the tool work of t_esp.cpp: only the motion request word is read here
struct DbToolWk {
    u8 pad_0[0xA4];
    int motionReq;    // 0xA4
};

// one entry of the model set config file
struct DbConfigModel {
    char name[0x20];  // 0x00
    int motNo;        // 0x20
    u8 pad_24[8];
    Vec pos;          // 0x2C
    Vec ang;          // 0x38
    int parOn;        // 0x44
    int parNo;        // 0x48
    int parPtNo;      // 0x4C
    Vec parPos;       // 0x50
    Vec parAng;       // 0x5C
};                    // 0x68

static int db_effLoaded = 0;
static int db_effOwner = 0xCF;
static int db_motionOn = 0;
static int db_workPushed = 0;
static int db_emArray = 1;
static int db_fog = 1;
static int db_cinesco = 0;
static int db_motionCam = 0;
static int db_roomCam = 0;
static u16 db_motNo = 0;
static int db_camCut = 0;
int db_modelNo = 0;
int db_frameCnt = 0;
int db_cutNo = 0;
void* db_fcvData = 0;
int db_nearClip = 0;
void* db_litData = 0;
void* db_camMotion = 0;
int db_x770 = 0xFF;
static s16 db_texX = 0x1A0;
static s16 db_texY = 0x108;
static s16 db_texZ = 1;
static s16 db_texW = 0x40;
static s16 db_texH = 0x40;

static char db_emName[0xC];
static cLightTool* db_pLightTool;

extern "C" int DB_isGetComeEventTool();
extern "C" void LoadModEff();
extern "C" void LightToolStart();
extern "C" void LightToolEnd();
extern "C" void DB_WorkPush(int flags, int emArray);
extern "C" void DB_WorkPop(int flags, int emArray);
extern "C" void SeqSet(EspSeqData* head, int mode);
extern "C" void DbModCarSet(cModel* m);
extern "C" int LoadModelInit();
extern "C" u32 MakeCol(f32 r, f32 g, f32 b, f32 a);
extern "C" int Sp_char_ck(int c);
extern "C" void font_draw(u8* c, f32 r, f32 g, f32 b, f32 a, s16 y, s16 x, s16 z, s16 w, s16 h);
extern "C" void EprintfDrawing(char* s, f32 x, f32 y, f32 r, f32 g, f32 b, f32 a);
extern "C" void DB_VecNullPartsPos(EspSeqData* head, Vec* in, Vec* out, Mtx* m);
extern "C" void DB_VecMulEmPartsMat(u32 parts, Vec* in, Vec* out, Mtx* m, EspGenWork* gen);
extern "C" void DB_GetCursorPos(EspSeqData* head, EspGenWork* gen, int flag, Vec* out, Mtx* m);
extern "C" void DB_DrawCross3D(Vec* pos, Mtx* m, f32 size);
extern "C" void drawTexture2(GXTexObj* obj, s16 x, s16 y, s16 z, s16 w, s16 h);
extern "C" int comment_check(char** pp);
extern "C" char* space_skip(char* p);
extern "C" int num_get(char** pp);
extern "C" int symbol_check(char** pp, const char* sym);
extern "C" void sp_PosRand_trans_1a(EspSeqData* head, EspGenWork* gen);
// COMPILER-DIFF: #1 (the original moves the cModel* argument before the f32 one: `mr r4; fmr f1`)

// flag word test: `li 1; cmp; bcc; li 0; cmpwi; beq` (EspToolInit)
static inline int flagOn(u32 f, u32 bit)
{
    int on = 1;
    if ((f & bit) == 0) {
        on = 0;
    }
    return on;
}

// the event's cut number as written in the debug data (two ascii digits) through the caller's buffer
// the room id read through the struct view of pG (global.h pG) right after the "x:/soft/room/" template copy: the
// pG load then depends on the copy's stores and the target's store order (word 1 last, `stw r9,4(r30)` right before
// `lwz r9,pG`) follows; the plain G_ROOM_ID read is a fixed scalar the stores do not order
#define G_ROOM_ID_S (*(u16*) &pG->stage_no)
// COMPILER-DIFF: #13 -- the j loop's `&EvtDebug` is a fresh `lis/addi` in the target (a REG_EQUIV lo_sum pseudo that the
// original never allocated, re-materialised at its copy); a distinct SYMBOL_REF ("*EvtDebug" string, so cse/gcse do not
// merge it with the pScr block's lo_sum) gives that with a symbol-based alias base for the loop's loads
extern EventDebug EvtDebug_j asm("EvtDebug");
#define EVT_CUT_NO_J(buf) ((buf)[0] = EvtDebug_j.pad_0[0x49], (buf)[1] = EvtDebug_j.pad_0[0x4A], (buf)[2] = 0, atoi(buf))
#define EVT_CUT_NO(buf) ((buf)[0] = EvtDebug.pad_0[0x49], (buf)[1] = EvtDebug.pad_0[0x4A], (buf)[2] = 0, atoi(buf))

// the event's cut number as written in the debug data (two ascii digits)
static inline int evtCutNo()
{
    char buf[3];

    buf[0] = EvtDebug.pad_0[0x49];
    buf[1] = EvtDebug.pad_0[0x4A];
    buf[2] = 0;
    return atoi(buf);
}

// The model an effect generator hangs on: the db_mod slot of its Parent_no, else the viewer's
// model 0.
extern "C" cModel* GetActiveModel(EspGenWork* gen)
{
    cModel* m;

    if (EvtDebug.FlagCkEtc(FlagEsp2Event)) {
        return dbModGetEmPtr(gen->Parent_no);
    }
    m = dbModGetEmPtr(db_modelNo);
    if (m == 0) {
        m = EmMgr.at(db_modelNo);
    }
    return m;
}

// 0..1 float components -> ARGB8 word.
extern "C" u32 MakeCol(f32 r, f32 g, f32 b, f32 a)
{
    u32 col = 0;

    col += (u8) (a * 255.0f) << 24;
    col += (u8) (r * 255.0f) << 16;
    col += (u8) (g * 255.0f) << 8;
    col += (u8) (b * 255.0f);
    return col;
}

// never called (dead-stripped body): a one-pixel point whose pool [0.0001, 1.0] survives between
// MakeCol's 255.0 and DB_DrawBox's 0.0001
static void DB_DrawPoint(f32 x, f32 y, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;

    p0.x = x;
    p0.y = y;
    p0.z = 0.0001f;
    p1.x = x + 1.0f;
    p1.y = y + 1.0f;
    p1.z = 0.0001f;
    Draw_quad(&p0, &p1, MakeCol(r, g, b, a));
}

// Window system hook: screen rectangle outline (four lines) in a float colour.
void DB_DrawBox(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;

    p0.x = x;
    p0.y = y;
    p0.z = 0.0001f;
    p1.x = x + w;
    p1.y = y;
    p1.z = 0.0001f;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x + w;
    p0.y = y;
    p0.z = 0.0001f;
    p1.x = x + w;
    p1.y = y + h;
    p1.z = 0.0001f;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x + w;
    p0.y = y + h;
    p0.z = 0.0001f;
    p1.x = x;
    p1.y = y + h;
    p1.z = 0.0001f;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x;
    p0.y = y + h;
    p0.z = 0.0001f;
    p1.x = x;
    p1.y = y;
    p1.z = 0.0001f;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
}

// Window system hook: filled screen rectangle.
void DB_DrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;

    p0.x = x;
    p0.y = y;
    p0.z = 0.0001f;
    p1.x = w;
    p1.y = h;
    p1.z = 0.0001f;
    Draw_quad(&p0, &p1, MakeCol(r, g, b, a));
}

// Window system hook: text at a pixel position (EprintfDrawing).
void DB_DrawString(f32 x, f32 y, const char* s, f32 r, f32 g, f32 b, f32 a)
{
    EprintfDrawing((char*) s, x, y, r, g, b, a);
}

// Fills the window system's keyboard from pad 1: d-pad / A / B / X / Y / L / R / Z / START on
// flags, the stick as floats and direction flags, then DB_KEYBORD::Update for triggers / repeats.
extern "C" void DB_GetKeybordData(DB_KEYBORD* k)
{
    u8 unused[8];
    JOY* joy;

    k->ClearAllKey();
    joy = &Joy[0];
    if (joy->on & 0x8) {
        k->on[0] = 1;
    }
    if (joy->on & 0x4) {
        k->on[1] = 1;
    }
    if (joy->on & 0x1) {
        k->on[2] = 1;
    }
    if (joy->on & 0x2) {
        k->on[3] = 1;
    }
    if (joy->on & 0x100) {
        k->on[5] = 1;
    }
    if (joy->on & 0x200) {
        k->on[6] = 1;
    }
    if (joy->on & 0x800) {
        k->on[7] = 1;
    }
    if (joy->on & 0x400) {
        k->on[8] = 1;
    }
    if (joy->on & 0x10) {
        k->on[9] = 1;
    }
    if (joy->on & 0x40) {
        k->on[10] = 1;
    }
    if (joy->on & 0x20) {
        k->on[11] = 1;
    }
    if (joy->on & 0x1000) {
        k->on[12] = 1;
    }
    k->stickX = (f32) joy->stickX / 72.0f;
    k->stickY = (f32) joy->stickY / 72.0f;
    if (joy->on & 0x40) {
        k->trigger = (f32) -(int) joy->triggerLeft / 144.0f;
    }
    if (joy->on & 0x20) {
        k->trigger = (f32) (int) (u8) joy->triggerRight / 144.0f;
    }
    k->Update();
}

// No mouse on the GameCube: nothing.
extern "C" void DB_GetMouseData()
{
}

// 1 for the characters the tool font has no glyph for (skipped by EprintfDrawing).
extern "C" int Sp_char_ck(int c)
{
    switch (c) {
    case 1:
        return 0x7D;
    case 2:
        return 0x60;
    case 3:
        return 0x7C;
    case 4:
        return 0x7B;
    }
    return c;
}

// Draws one character of the eprintf font texture at (x, y) in a float colour.
extern "C" void font_draw(u8* c, f32 r, f32 g, f32 b, f32 a, s16 y, s16 x, s16 z, s16 w, s16 h)
{
    int code = *c;
    int idx;
    int s;
    int t;
    u8 cr;
    u8 cg;
    u8 cb;
    u8 ca;

    if (code == ' ' || code == 0) {
        return;
    }
    idx = Sp_char_ck(code) - 0x20;
    s = (idx & 0x1F) * 8;
    t = ((idx >> 5) & 7) * 16;
    cr = (u8) (r * 255.0f);
    cg = (u8) (g * 255.0f);
    cb = (u8) (b * 255.0f);
    ca = (u8) (a * 255.0f);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x, y, z);
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2s16(s, t);
    GXPosition3s16(x + w, y, z);
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2s16(s + 8, t);
    GXPosition3s16(x + w, y + h, z);
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2s16(s + 8, t + 16);
    GXPosition3s16(x, y + h, z);
    GXColor4u8(cr, cg, cb, ca);
    GXTexCoord2s16(s, t + 16);
}

// Draws a string with font_draw, 8 pixels per character.
extern "C" void EprintfDrawing(char* s, f32 x, f32 y, f32 r, f32 g, f32 b, f32 a)
{
    f32 cr = r * 0.7f;
    f32 cg = g * 0.7f;
    f32 cb = b * 0.7f;
    f32 ca = a * 0.95f;

    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetTevOp(0, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetChanCtrl(4, 0, 1, 1, 0, 0, 2);
    GXSetNumTexGens(1);
    GXLoadTexObj(&fontTexObj, 0);
    GXLoadTexMtxImm(fontTMtx, 30, 1);
    GXSetTexCoordGen2(0, 1, 4, 30, 0, 0x7D);
    GXSetBlendMode(1, 4, 5, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 3, 8);
    while (*s) {
        font_draw((u8*) s, cr, cg, cb, ca, (s16) y, (s16) x, 1, 8, 14);
        x += 8.0f;
        s++;
    }
}

// Starts the edited effect sequence on the viewer model (EstSet with owner 0xCF): mode 0 plain,
// mode n the flag 8 << (n - 1) (loop / once variants of the tool's PLAY buttons).
extern "C" void SeqSet(EspSeqData* head, int mode)
{
    cModel* m;
    u16 f;
    m = dbModGetEmPtr(db_modelNo);
    if (m == 0) {
        m = EmMgr.at(db_modelNo);
    }
    if (m != 0 && !(m->be_flag & 1)) {
        m = 0;
    }
    if (mode == 0) {
        f = 0;
    } else {
        f = (u16) (8 << (mode - 1));
    }
    if (EvtDebug.FlagCkEtc(FlagEsp2Event)) {
        f |= 0x1000;
    }
    EstSet(m, -1, 0, 0, head, f | 1, ESP_CORE_KIND_NONE, m, EFF_DEBUG, 0);
}

// Loads the event's camera data for the event-tool preview (EvtDebug camName).
extern "C" void DB_EventCamLoad()
{
}

// Switches to the event camera (the tool's Debug_flg[0] bit 28 cleared).
extern "C" void DB_EventCamStart()
{
    if (db_camMotion) {
        DbgFlagOff(pG, DBG_DBG_CAM);
        CamCtrl.MotionSet(db_camMotion, 0, 0.0f);
        CameraMove();
    }
}

// Switches to room camera cut `cut` (CamCtrl) for the preview.
extern "C" void DB_RoomCamStart(int cut)
{
    db_camCut = cut;
    DbgFlagOff(pG, DBG_DBG_CAM);
    CamCtrl.CutCall((s8) cut);
    CameraMove();
    SpfFlagOff(pG, SPF_LIGHT);
    db_roomCam = 1;
}

// Starts the db_mod viewer (dbModelInit); 1.
extern "C" int LoadModelInit()
{
    dbModelInit();
    dbModSetViewFlag(4);
    EffectDeleteAll();
    return 1;
}

// After a model set loaded into slot 0: derives the enemy name (emNN / obm2 special cases) and
// reads its x:/soft/room/esp/<name>.eff effect data so the editor can pick from it.
extern "C" void LoadModEff()
{
    char path[0x100];
    char name[5];
    void* buf;
    char* s;
    char* top;

    if (dbModelIsAlive(0) == 0) {
        return;
    }
    s = dbModBinName();
    top = s;
    if (s == 0) {
        return;
    }
    while (*s != '.' && *s != 0) {
        s++;
    }
    while (*s != '/' && s >= top) {
        s--;
    }
    s++;
    if (s[0] == db_emName[0] && s[1] == db_emName[1] && s[2] == db_emName[2] && s[3] == db_emName[3]) {
        return;
    }
    u8 zero = 0;
    name[0] = s[0];
    name[1] = s[1];
    name[2] = s[2];
    name[3] = s[3];
    name[4] = zero;
    if (db_emName[0] != 0) {
        EspDataRelease(db_effOwner, 1, 1);
    }
    db_emName[0] = s[0];
    db_emName[1] = s[1];
    db_emName[2] = s[2];
    db_emName[3] = s[3];
    db_emName[4] = zero;
    if (strcmp(name, "em11") == 0 || strcmp(name, "em12") == 0 || strcmp(name, "em13") == 0 ||
        strcmp(name, "em14") == 0 || strcmp(name, "em15") == 0 || strcmp(name, "em16") == 0 ||
        strcmp(name, "em17") == 0 || strcmp(name, "obm2") == 0) {
        db_emName[0] = 'e';
        db_emName[1] = 'm';
        db_emName[2] = '1';
        db_emName[3] = '0';
    }
    sprintf(path, "x:/soft/room/esp/%s.eff", db_emName);
    if (HDReadDebugAlloc(path, &buf, 1)) {
        db_effLoaded = 1;
        EspDataLoad((u32) buf, db_effOwner, 0);
    }
}

// Runs the db_mod menu (dbModel) for the model selection; when it leaves (2) loads the model's
// effects (LoadModEff). Returns the menu's result.
extern "C" int LoadModel()
{
    int ret = 1;

    EffectDeleteAll();
    if (dbModel(0) == 2) {
        ret = 0;
    }
    dbModMotionMove();
    if (ret == 0) {
        LoadModEff();
    }
    return ret;
}

// 1 when a viewer model is alive (slot 0 or the event models).
extern "C" int DB_IsEmLoad()
{
    cModel* m = dbModGetEmPtr(db_modelNo);

    if (m && (m->be_flag & 1)) {
        return 1;
    }
    return 0;
}

// 1 while the room's manager arrays are parked (DB_WorkPush).
extern "C" int DB_IsWorkPush()
{
    return db_workPushed;
}

// Parks the room's manager arrays (ToolArrayPush with `flags`, plus 10 enemy works when emArray)
// and turns off the room display / camera target for the tool's own scene.
extern "C" void DB_WorkPush(int flags, int emArray)
{
    if (db_workPushed && emArray == db_emArray) {
        return;
    }
    db_emArray = emArray != 0;
    db_workPushed = 1;
    EffectDeleteAll();
    DbgFlagOff(pG, DBG_ESPTOOL_ONSCR);
    ToolArrayPush(flags);
    ToolEmArraySet(emArray);
    if (emArray) {
        DbgFlagOn(pG, DBG_ESPTOOL_ONEM);
    } else {
        DbgFlagOff(pG, DBG_ESPTOOL_ONEM);
    }
    CamDbg.m_target_type = 0;
}

// Restores the room's arrays and display (inverse of DB_WorkPush).
extern "C" void DB_WorkPop(int flags, int emArray)
{
    if (db_workPushed == 0 && emArray == db_emArray) {
        return;
    }
    db_emArray = emArray != 0;
    db_workPushed = 0;
    CamDbg.m_target_type = 4;
    EffectDeleteAll();
    DbgFlagOn(pG, DBG_ESPTOOL_ONSCR);
    ToolWorkPop(flags);
    Block.dispAllBlock(1);
    ToolEmArraySet(emArray);
    if (emArray) {
        DbgFlagOn(pG, DBG_ESPTOOL_ONEM);
    } else {
        DbgFlagOff(pG, DBG_ESPTOOL_ONEM);
    }
}

// Hides parts `no` of a model.
static inline void carPartsClear(cModel* m, int no)
{
    cParts* p = m->getPartsPtr(no);
    if (p) {
        p->scale.z = p->scale.y = p->scale.x = 0.0f;
    }
}

// Sets up a car model for the viewer: be_flag 0x10 / 0x2000000 and parts 0x12..0x17 hidden.
extern "C" void DbModCarSet(cModel* m)
{
    m->ot_type = 4;
    m->be_flag |= 0x10;
    m->be_flag |= 0x2000000;
    carPartsClear(m, 0x12);
    carPartsClear(m, 0x13);
    carPartsClear(m, 0x14);
    carPartsClear(m, 0x15);
    carPartsClear(m, 0x16);
    carPartsClear(m, 0x17);
    carPartsClear(m, 0xF);
}

// name[15..] of an event model file name (a macro: an inline's `&&` chain ends in a setcc)
#define nameIs4(name, a, b, c, d) ((name)[0xF] == (a) && (name)[0x10] == (b) && (name)[0x11] == (c) && (name)[0x12] == (d))

// a macro: the info chain (`em->pInfo->pNext->..`) is re-walked for every call
// (a plain block: a do-while's loop notes end cse's path and split `&tbl` into two pseudos)
#define texBlendSet(info, tbl) \
    { \
        (info)->setTexBlendTbl(tbl); \
        (info)->setBlendRatio(0xFF); \
        (info)->setBlendType(1); \
    }

// Fills a TEV blend table from a texture render manager's modes.
static inline void texBlendTbl(u8* tbl, TexRenderMng* t)
{
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = t->GetTexNo();
}

#define INFO0(m) ((m)->pModelInfo)
#define INFO1(m) (INFO0(m)->pList)
#define INFO2(m) (INFO1(m)->pList)
#define INFO3(m) (INFO2(m)->pList)
#define INFO4(m) (INFO3(m)->pList)
#define INFO5(m) (INFO4(m)->pList)
#define INFO6(m) (INFO5(m)->pList)
#define INFO7(m) (INFO6(m)->pList)
#define INFO8(m) (INFO7(m)->pList)
#define INFO9(m) (INFO8(m)->pList)

// the name copies: an inline wrapper gives strcpy's destination as a fresh `addi r3,r1,0x110` per call (integrate
// substitutes `&name` into the hard-register argument set) while strcat's `name` stays the PRE'd pseudo; the source
// goes through a `char*` local so the element address is `pModel + ofs` (base first) like the target
static inline void StrCpy(char* d, const char* s) { strcpy(d, s); }
#define NAME_SET(src_) { char* src = (src_); StrCpy(name, src); }
// every model field is re-read as EvtDebug.pModel[i].field (no `m` pointer: the target reloads pModel per use)
// every model field is read as pModel + rr (the target reloads pModel per use and keeps only the product copy `rr`):
// field reads sum `rr + pModel` (`add rX,r26,rP`), the name/bin/tpl addresses `pModel + rr` (`add r4,r4,r26`, the natural
// EXPAND_SUM order of `&pModel[i].name`); M0 is the pScr block's view through its own `lis/addi` (#13, see ed0), MJ the
// j loop's (EvtDebug_j)
#define M (*(EvtDebugModel*) (rr + (u32) EvtDebug.PMod))
#define MA ((EvtDebugModel*) ((u32) EvtDebug.PMod + rr))
#define M0 ((EvtDebugModel*) (rr + (u32) ed0->PMod))
#define MJ ((EvtDebugModel*) ((u32) EvtDebug_j.PMod + rr))

// Effect tool start: tool flags, viewer started; when entered from the event tool, loads the
// event's stage / cut numbers (from EvtDebug), its .eff, camera and model set (every event model
// with its parents through dbModelLoad / LoadModelSetName, special-cased ev3001 / kizu files), else
// waits for a model set from the config. `out` gets the entry mode.
extern "C" void EspToolInit(int* out, u8* pStage, u8* pCut)
{
    char buf3[3];
    char path[0x100];
    char name[0x30];
    void* buf;
    int parent;
    int nModel;
    int i;
    u32 room;
    int one = 1;

    db_effOwner = 0xCF;
    db_motionOn = 0;
    db_emName[0] = 0;
    db_frameCnt = 0;
    db_effLoaded = 0;
    db_nearClip = 0;
    db_workPushed = 0;
    DbgFlagOn(pG, DBG_IN_ESP_TOOL);
    pLog->clear();
    pLog->modeSet(0x68, 0xE, 0x3C, 8);
    TaskSleep(1);
    TutilInitDefault();
    pG->debug_mode = 0xD;
    StaFlagOff(pG, STA_CINESCO);
    StaFlagOff(pG, STA_SUSPEND);
    SpfFlagOn(pG, SPF_LIGHT);
    DpfFlagOff(pG, DPF_WATER);
    SpfFlagOff(pG, SPF_WATER);
    db_fcvData = 0;
    db_emArray = one;
    db_fog = one;
    // COMPILER-DIFF: #13 -- the target issues the five stores fcv, fog, emArray, cinesco, workPushed although `one`'s
    // dying store (the second `one` store, weight -1) would lead in ours; the asm's output dependence on a different-
    // mode view of the fcvData store gives that store the priority to stay first (no barrier, no code). The lis order
    // (fcv, emArray, fog, cinesco = the target's) is the statement order; the store order follows from the death.
    asm("" : "=m"(*(u16*) &db_fcvData));
    db_cinesco = 0;
    db_workPushed = 0;
    DB_WorkPush(1, 1);
    DbgFlagOn(pG, DBG_GROUND_DISP);
    DbgFlagOn(pG, DBG_BACK_CLIP);
    SpfFlagOn(pG, SPF_PL);
    DpfFlagOn(pG, DPF_SHADOW);
    SpfFlagOn(pG, SPF_SCE);
    DbgFlagOn(pG, DBG_DBG_CAM);
    LightToolStart();
    LoadModelInit();
    SetLoopFlag(0, 0);
    SetLoopFlag(0, 1);
    SetLoopFlag(0, 2);
    SetLoopFlag(0, 3);
    SetLoopFlag(0, 4);
    if (EvtDebug.FlagCkEtc(FlagEvent2Esp)) {
        int n;
        u8 c;
        u8 hi;
        u32 nLit;
        int k;

        EvtDebug.FlagOffEtc(FlagEvent2Esp);
        EvtDebug.FlagOnEtc(FlagEsp2Event);
        StaFlagOn(pG, STA_SUSPEND);
        StaFlagOn(pG, STA_EVENT_SYSYTEM);
        StaFlagOn(pG, STA_EFFAREA_USE_CAM);
        EspEvModList.Clear();
        n = EVT_CUT_NO(buf3);
        c = n;
        // `hi` is set twice: a single-set quotient has nonzero_bits <= 0x1F and combine drops the target's `clrlslwi` mask
        hi = c / 10;
        *pStage = (hi << 4) + c % 10;
        c = EvtDebug.pad_0[0xDB];
        hi = c / 10;
        *pCut = (hi << 4) + c % 10;
        db_cutNo = *pCut;
        sprintf(path, "x:/soft/room/esp/r%03xs%02x.eff", G_ROOM_ID, *pStage);
        if (HDReadDebugAlloc(path, &buf, 1)) {
            db_effLoaded = 1;
            EspDataLoad((u32) buf, db_effOwner, 0);
        }
        *out = 0;
        DB_WorkPop(3, db_emArray);
        StrCpy(name, EvtDebug.evName);
        strcpy(path, "x:/soft/room/");
        strcat(path, name);
        if (HDReadDebugAlloc(path, &db_camMotion, 1) == 0) {
            db_camMotion = 0;
        }
        StrCpy(name, EvtDebug.camName);
        strcpy(path, "x:/soft/room/");
        strcat(path, name);
        if (HDReadDebugAlloc(path, &db_litData, 1) == 0) {
            db_litData = 0;
        }
        LightMgr.beginEvent();
        SpfFlagOff(pG, SPF_LIGHT);
        LightMgr.roomLitSet((cLit*) db_litData);
        LightMgr.update(0, -1);
        nLit = LightMgr.getArrayNum();
        for (k = 0; k < nLit; k++) {
            cLight* l = LightMgr.at(k);
            if ((l->be_flag & 3) == 3 && l->ParentType == 1) {
                l->be_flag &= 2; // sic: the original masks with 2, not ~2 (`rlwinm 0,30,30`)
            }
        }
        nModel = EvtDebug.GetNumMod();
        for (i = 0; i < nModel; i++) {
            // set before the static guards: a set after their `bne`s is `maybe_never` for loop.c and stays in the body
            int* pParent = &parent;
            static DB_MODEL_FILES bin;
            static DB_MODEL_FILES tpl;
            static DB_MODEL_FILES xtra;
            cModel* em;
            int slot;
            int j;
            int nBin;
            TexRenderMng* t;
            cModel* s;
            u32 prod;
            u32 rr;
            EventDebug* ed0;

            bin.init();
            tpl.init();
            xtra.init();
            // COMPILER-DIFF: candidate #12 (AROUND form) -- the target's parent/else arms recompute `pModel + i*0x644` from
            // gcse's reaching-reg copy of the product (`mr r26,r9`) although the flags test is on cse's AROUND path (ours
            // folds the sum into the test's pseudo). The tied codeless asm is that copy (regmove splits it into `rr = prod`
            // + the asm); its `"=m"(buf3[2])` output makes the flags load a dependent, so sched1 issues the copy before
            // the add (priority 6 > 5) and `prod` dies at the add (tied into the sum's r9 like the target).
            prod = i * sizeof(EvtDebugModel);
            asm("" : "=r"(rr), "=m"(buf3[2]) : "0"(prod));
            if (flagOn(*(u32*) (prod + (u32) EvtDebug.PMod + 0x63C), 0x20000000)) {
                int idx;
                int parentModel;

                idx = (s8) M.pad_63A[0];
                parent = (s8) M.pad_63A[1];
                Vec ofs = {0.0f, 0.0f, 0.0f};
                // an int local: `(s8)` of the field itself narrows the load to `lbz 0x637`
                parentModel = (int) EvtDebug.PMod[idx].pModel;
                dbModelParentChild((s8) i, (s8) parentModel, ((s8*) pParent)[3], &ofs, &ofs);
            } else {
                NAME_SET(MA->name);
                strcpy(path, "x:/soft/room/");
                strcat(path, name);
                xtra.append(path);
            }
            // COMPILER-DIFF: #13 -- the pScr block's `&EvtDebug` is a REG_EQUIV symbol pseudo the original never allocated:
            // re-materialised here as `lis r9; addi r9,r9; lwz 0xe0(r9)` (the offset stays outside the @l because the
            // equivalence is the bare symbol) and again in the j loop (EvtDebug_j). The plain pointer local gives the
            // same: loop.c hoists the lo_sum to the i preheader and leaves `ed0 = copy` with REG_EQUIV EvtDebug; the copy
            // spans the whole body (96 calls), global.c spills it and reload re-materialises the symbol at the use.
            ed0 = &EvtDebug;
            s = M0->pScr;
            if (s) {
                bin.append(s->pModelInfo->model_addr);
                tpl.append(s->pModelInfo->tpl_addr);
            } else {
                nBin = M0->nBin;
                for (j = 0; j < nBin; j++) {
                    NAME_SET(MJ->bin[j]);
                    strcpy(path, "x:/soft/room/");
                    if (G_ROOM_ID_S == 0x332 && EVT_CUT_NO_J(buf3) == 0 && db_cutNo == 0x19 &&
                        strcmp(name, "event/model/ev3000/ev3001.bin") == 0) {
                        strcat(path, "event/model/ev3000/ev3001a.bin");
                    } else {
                        strcat(path, name);
                    }
                    bin.append(path);
                    NAME_SET(MJ->tpl[j]);
                    strcpy(path, "x:/soft/room/");
                    if (G_ROOM_ID_S == 0x317 && strcmp(name, "event/model/ev0000/ev0001.tpl") == 0) {
                        strcat(path, "event/model/ev0000/ev0001_kizu.tpl ");
                    } else if (G_ROOM_ID == 0x332 && EVT_CUT_NO_J(buf3) == 0 && db_cutNo == 0x19 &&
                               strcmp(name, "event/model/ev3000/ev3001.tpl") == 0) {
                        strcat(path, "event/model/ev3000/ev3001a.tpl");
                    } else {
                        strcat(path, name);
                    }
                    tpl.append(path);
                }
            }
            NAME_SET(MA->name);
            if (G_ROOM_ID == 0x11B && nameIs4(name, 'p', 'l', '0', '0')) {
                strcpy(path, "x:/soft/room/event/model/ev0000/ev0001a.bin");
                bin.append(path);
                strcpy(path, "x:/soft/room/event/model/ev0000/ev0001a.tpl");
                tpl.append(path);
                strcpy(path, "x:/soft/room/event/model/ev0000/ev000a.bin");
                bin.append(path);
                strcpy(path, "x:/soft/room/event/model/ev0000/ev000a.tpl");
                tpl.append(path);
                strcpy(path, "x:/soft/room/event/model/ev0000/ev000b.bin");
                bin.append(path);
                strcpy(path, "x:/soft/room/event/model/ev0000/ev000b.tpl");
                tpl.append(path);
            }
            NAME_SET(MA->name);
            if (G_ROOM_ID == 0x11C) {
                if (nameIs4(name, 'p', 'l', '0', '0')) {
                    strcpy(path, "x:/soft/room/event/model/ev0000/ev000c.bin");
                    bin.append(path);
                    strcpy(path, "x:/soft/room/event/model/ev0000/ev000c.tpl");
                    tpl.append(path);
                }
                if (G_ROOM_ID == 0x11C && nameIs4(name, 'p', 'l', '0', '4')) {
                    strcpy(path, "x:/soft/room/event/model/ev0400/ev040a.bin");
                    bin.append(path);
                    strcpy(path, "x:/soft/room/event/model/ev0400/ev040a.tpl");
                    tpl.append(path);
                }
            }
            NAME_SET(MA->name);
            if (G_ROOM_ID == 0x325) {
                if (db_cutNo == 4 && nameIs4(name, 'p', 'l', '0', '0')) {
                    strcpy(path, "x:/soft/room/event/model/ev0000/ev000f.bin");
                    bin.append(path);
                    strcpy(path, "x:/soft/room/event/model/ev0000/ev000f.tpl");
                    tpl.append(path);
                }
                if (G_ROOM_ID == 0x325 && db_cutNo == 7 && nameIs4(name, 'p', 'l', '0', '0')) {
                    strcpy(path, "x:/soft/room/event/model/evmb400/evmb400.bin");
                    bin.append(path);
                    strcpy(path, "x:/soft/room/event/model/evmb400/evmb400.tpl");
                    tpl.append(path);
                }
            }
            slot = (int) M.pModel;
            dbModelLoad(slot, &bin, &tpl, &xtra);
            SetLoopFlag(0, slot);
            em = dbModGetEmPtr(slot);
            if (em) {
                Vec size;
                Vec center;
                cModel* p;
                cModelInfo* info;
                ModelBound* b;
                u8 lit;

                em->setNoSuspend(1);
                p = dbModGetEmPtr(slot);
                EspEvModList.SetModelPtr(slot, p);
                em->ot_type = M.otType;
                if (flagOn(M.flags, 0x80000000)) {
                    em->z_mode = 1;
                }
                if (flagOn(M.flags, 0x40000000)) {
                    em->be_flag |= 0x1000;
                }
                info = em->pModelInfo;
                b = &info->bound;
                lit = M.lightMask;
                size.x = b->size.x;
                size.y = b->size.y;
                size.z = b->size.z;
                PSVECSubtract(&b->center, &em->pList->pos, &center);
                em->LightInfo.init2(2, 1, &center, &size, lit);
            }
            NAME_SET(MA->name);
            if (nameIs4(name, 'o', 'b', 'm', '1') && name[0x13] == 'a') {
                DbModCarSet(em);
            }
            if (G_ROOM_ID == 0x10B && nameIs4(name, 'p', 'l', '0', 'f')) {
                cParts* p = em->getPartsPtr(3);
                p->scale.z = p->scale.y = p->scale.x = 0.0f;
            }
            NAME_SET(MA->name);
            if (G_ROOM_ID == 0x11B && nameIs4(name, 'p', 'l', '0', '0')) {
                static u8 tbl0[0x20];
                static u8 tbl1[0x20];

                t = GetTexRenderMgrAddr(0);
                if (t->IsAlive()) {
                    texBlendTbl(tbl0, t);
                    texBlendSet(INFO6(em), tbl0);
                    INFO0(em)->be_flag |= 4;
                    INFO1(em)->be_flag |= 4;
                    INFO2(em)->be_flag |= 4;
                    INFO3(em)->be_flag |= 4;
                    INFO4(em)->be_flag |= 4;
                    INFO5(em)->be_flag |= 4;
                    em->Shader_type = 2;
                    em->Refract_pow = 0x10;
                    em->Refract_ratio = 0x90;
                }
                t = GetTexRenderMgrAddr(1);
                if (t->IsAlive()) {
                    texBlendTbl(tbl1, t);
                    texBlendSet(INFO7(em), tbl1);
                    texBlendSet(INFO8(em), tbl1);
                    em->Shader_type = 2;
                    em->Refract_pow = 0x10;
                    em->Refract_ratio = 0x90;
                }
                if (db_cutNo == 6) {
                    INFO7(em)->be_flag |= 8;
                    INFO8(em)->be_flag &= ~8;
                }
                if (db_cutNo == 7) {
                    INFO7(em)->be_flag &= ~8;
                    INFO8(em)->be_flag |= 8;
                }
                if (db_cutNo == 8) {
                    INFO7(em)->be_flag |= 8;
                    INFO8(em)->be_flag &= ~8;
                }
                // a second `if` after the reference stores (BitOn: a non-struct store invalidates the db_cutNo load in cse)
                if (db_cutNo == 8) {
                    db_nearClip = 1;
                }
            }
            NAME_SET(MA->name);
            if (G_ROOM_ID == 0x11C) {
                static u8 tbl2[0x20];
                static u8 tbl3[0x20];

                if (nameIs4(name, 'p', 'l', '0', '0')) {
                    t = GetTexRenderMgrAddr(0);
                    if (t->IsAlive()) {
                        texBlendTbl(tbl2, t);
                        texBlendSet(INFO6(em), tbl2);
                    }
                }
                if (G_ROOM_ID == 0x11C && nameIs4(name, 'p', 'l', '0', '4')) {
                    t = GetTexRenderMgrAddr(0);
                    if (t->IsAlive()) {
                        texBlendTbl(tbl3, t);
                        texBlendSet(INFO4(em), tbl3);
                    }
                }
            }
            if (G_ROOM_ID == 0x206 && nameIs4(name, 'e', 'v', 'm', 'c') && name[0x13] == '8' &&
                name[0x14] == '0' && name[0x15] == '0') {
                static u8 tbl4[0x20];

                t = GetTexRenderMgrAddr(0);
                if (t->IsAlive()) {
                    texBlendTbl(tbl4, t);
                    texBlendSet(em->pModelInfo, tbl4);
                }
            }
            if (G_ROOM_ID == 0x228 && nameIs4(name, 'e', 'm', '3', '8') && name[0x13] == '0' &&
                name[0x14] == '0' && name[0x15] == '/') {
                static u8 tbl5[0x20];

                t = GetTexRenderMgrAddr(2);
                if (t->IsAlive()) {
                    texBlendTbl(tbl5, t);
                    texBlendSet(em->pModelInfo->pList, tbl5);
                }
            }
            if (G_ROOM_ID == 0x317 && *pStage == 3 && (u32) db_cutNo > 0x10 && nameIs4(name, 'e', 'm', '3', '9') &&
                name[0x13] == '0' && name[0x14] == '0' && name[0x15] == '/') {
                ModelInfoSetTrans(em, 0, 0);
            }
            if (G_ROOM_ID == 0x325) {
                static u8 tbl6[0x20];
                static u8 tbl7[0x20];

                if (db_cutNo == 4 && nameIs4(name, 'p', 'l', '0', '0')) {
                    t = GetTexRenderMgrAddr(0);
                    if (t->IsAlive()) {
                        texBlendTbl(tbl6, t);
                        texBlendSet(INFO6(em), tbl6);
                    }
                }
                if (G_ROOM_ID == 0x325 && db_cutNo == 7 && nameIs4(name, 'p', 'l', '0', '0')) {
                    t = GetTexRenderMgrAddr(0);
                    if (t->IsAlive()) {
                        texBlendTbl(tbl7, t);
                        texBlendSet(INFO6(em), tbl7);
                    }
                }
            }
            if (G_ROOM_ID == 0x20B) {
                static u8 tbl8[0x20];

                if ((db_cutNo == 0x33 || db_cutNo == 0x36) && nameIs4(name, 'e', 'v', 'm', 'a') && name[0x13] == '1' &&
                    name[0x14] == '0' && name[0x15] == '0' && name[0x16] == 'a') {
                    t = GetTexRenderMgrAddr(2);
                    if (t->IsAlive()) {
                        texBlendTbl(tbl8, t);
                        texBlendSet(em->pModelInfo, tbl8);
                        em->pModelInfo->blend_mode = 1;
                    }
                    db_nearClip = 1;
                }
                if (G_ROOM_ID == 0x20B && (db_cutNo == 0x33 || db_cutNo == 0x36) && nameIs4(name, 'e', 'v', 'm', 'a') &&
                    name[0x13] == '1' && name[0x14] == '0' && name[0x15] == '0' && name[0x16] == '/') {
                    em->be_flag &= ~2;
                }
            }
        }
    }
    if (G_ROOM_ID == 0x332 && *pStage == 0 && db_cutNo == 0x19) {
        HDReadDebugAlloc("X:\\Soft\\Room\\Event\\r332\\s00\\em3000a\\face\\ev3001_s00_019.fcv", &db_fcvData, 1);
    }
}
#undef M

// Tool exit hook when leaving to the game: stops the viewer and restores the arrays; with `on`
// re-plays the edited sequence on the game model.
extern "C" void EspToolExitEstSet(EspSeqData* head, int on, int mode)
{
    pLog->modeSet(0xA0, 0x17A, 0x5A, 5);
    DB_WorkPush(3, 1);
    dbModelQuit();
    DB_WorkPush(3, 0);
    DB_WorkPop(1, 0);
    EffectDeleteAll();
    if (on) {
        SeqSet(head, mode);
    }
}

// Effect tool end: clears the tool flags, frees the .eff buffers; when entered from the event tool
// hands back to it (DbMenuSetExecTool "EVENT TOOL").
extern "C" void EspToolExit()
{
    // through a volatile pointer: the store keeps `&CamDbg` in a register (`stb 0xf(rX)`, t_lightarea idiom)
    volatile debugCamera* dbg = &CamDbg;
    GXColor col;

    DbgFlagOff(pG, DBG_GROUND_DISP);
    SpfFlagOff(pG, SPF_PL);
    SpfFlagOff(pG, SPF_EM);
    DpfFlagOff(pG, DPF_SHADOW);
    SpfFlagOff(pG, SPF_SCE);
    SpfFlagOff(pG, SPF_LIGHT);
    DbgFlagOff(pG, DBG_DBG_CAM);
    *(u32*) &col = 0;
    dbg->m_target_type = 0;
    bio4_GXSetCopyClear(col, 0xFFFFFF);
    LightMgr.setFog();
    LightToolEnd();
    Block.dispAllBlock(0);
    if (EvtDebug.FlagCkEtc(FlagEsp2Event)) {
        DbMenuSetExecTool("EVENT TOOL");
        StaFlagOff(pG, STA_SUSPEND);
        StaFlagOff(pG, STA_EVENT_SYSYTEM);
        StaFlagOff(pG, STA_EFFAREA_USE_CAM);
        LightMgr.endEvent();
        LightMgr.roomLitSet(0);
        LightMgr.update(0, -1);
    }
    if (db_effLoaded == 1) {
        EspDataRelease(db_effOwner, 1, 1);
    }
    CamCtrl.Comeback(0);
    DbgFlagOff(pG, DBG_IN_ESP_TOOL);
    TutilQuitDefault();
    TaskExit();
}

// 1 when the tool was started from the event tool.
extern "C" int DB_isGetComeEventTool()
{
    if (EvtDebug.FlagCkEtc(FlagEsp2Event)) {
        return 1;
    }
    return 0;
}

// Screen clear colour of the tool scene.
extern "C" void DB_SetBgColor(u8 r, u8 g, u8 b, u8 a)
{
    GXColor col;

    col.r = r;
    col.g = g;
    col.b = b;
    col.a = a;
    bio4_GXSetCopyClear(col, 0xFFFFFF);
}

// Ground grid on / off (Debug_flg bit).
extern "C" void DB_DrawGrid(int on)
{
    if (on) {
        DbgFlagOn(pG, DBG_GROUND_DISP);
    } else {
        DbgFlagOff(pG, DBG_GROUND_DISP);
    }
}

// Draws the current viewer model's skeleton this frame when `on`.
extern "C" void DB_DrawMod_sk(int on)
{
    if (on) {
        cModel* m = dbModGetEmPtr(db_modelNo);
        if (m && (m->be_flag & 1)) {
            m->debugSkeletonDisp();
        }
    }
}

// Remembers the fog switch for the tool scene.
extern "C" void DB_SetFog(int on)
{
    db_fog = on;
}

// Remembers the cinemascope switch.
extern "C" void DB_SetCinesco(int on)
{
    db_cinesco = on;
}

// Remembers the motion camera switch.
extern "C" void DB_SetMotionCam(int on)
{
    db_motionCam = on;
}

// Per frame: animates the viewer models, serves the tool's motion request (restart the motion,
// camera, the face fcv), and applies the selected texture render manager's blend table.
extern "C" void EspToolUpdate(DbToolWk* wk, int texNo)
{
    SpfFlagOn(pG, SPF_PL);
    if (db_emArray == 0) {
        SpfFlagOn(pG, SPF_EM);
    }
    if (db_fog) {
        DpfFlagOff(pG, DPF_FOG);
    } else {
        DpfFlagOn(pG, DPF_FOG);
    }
    if (db_cinesco) {
        StaFlagOn(pG, STA_CINESCO);
    } else {
        StaFlagOff(pG, STA_CINESCO);
    }
    LightMgr.setFog();
    if (db_motionOn) {
        dbModMotionMove();
        db_frameCnt++;
    }
    if (DB_isGetComeEventTool() == 0 && db_roomCam == 0) {
        if (db_motionCam) {
            DbgFlagOff(pG, DBG_DBG_CAM);
            CameraMove();
        } else {
            DbgFlagOn(pG, DBG_DBG_CAM);
        }
    }
    if (wk->motionReq) {
        if (G_ROOM_ID == 0x228 && db_camCut == 0xA) {
            db_motNo = 0x46;
        } else if (G_ROOM_ID == 0x228 && db_camCut == 0xB) {
            db_motNo = 0xA5;
        } else {
            db_motNo = 0;
        }
        dbModMotionSet(db_motNo);
        db_motionOn = 1;
        db_frameCnt = 0;
        dbModelSetCamera(0, &pG->Camera);
        if (G_ROOM_ID == 0x332 && evtCutNo() == 0 && db_cutNo == 0x19 && db_fcvData) {
            ShapeSet(INFO5(dbModSlot[0].pModel), 0, db_fcvData, 2);
        }
    }
    if (texNo) {
        TexRenderMng* t = GetTexRenderMgrAddr(texNo - 1);
        if (t->IsAlive()) {
            drawTexture2(t->GetTexObj(), 0x14C, 0x36, 0, 0xA0, 0xA0);
        }
    }
    if (db_nearClip == 1) {
        SetNearClipDist(1.0f);
    }
}

// Screen cross-hair at `pos`.
extern "C" void DB_DrawCursor2D(Vec* pos)
{
    Vec p0;
    Vec p1;

    p0 = *pos;
    p1 = *pos;
    p0.x += 50.0f;
    p1.x -= 50.0f;
    Draw_line(&p0, &p1, MakeCol(1.0f, 1.0f, 1.0f, 1.0f));
    p0 = *pos;
    p1 = *pos;
    p0.y += 50.0f;
    p1.y -= 50.0f;
    Draw_line(&p0, &p1, MakeCol(1.0f, 1.0f, 1.0f, 1.0f));
}

// World position / matrix of a generator's origin: the parent parts (Parts_no; 0xFE = the null
// parts position, 0xFF or flag = world) applied to gen->Pos.
extern "C" void DB_GetCursorPos(EspSeqData* head, EspGenWork* gen, int flag, Vec* out, Mtx* m)
{
    int parts = gen->Parts_no;

    if (parts == 0xFF || flag != 0) {
        DB_VecNullPartsPos(head, &gen->Pos, out, m);
    } else if (parts == 0xFE) {
        *out = gen->Pos;
        PSMTXIdentity(*m);
        (*m)[0][3] = out->x;
        (*m)[1][3] = out->y;
        (*m)[2][3] = out->z;
    } else {
        DB_VecMulEmPartsMat(parts, &gen->Pos, out, m, gen);
    }
}

// Draws the 3D cross at a generator's origin.
extern "C" void DB_DrawCursor3D(EspSeqData* head, EspGenWork* gen, int flag, f32 size)
{
    Vec pos;
    Mtx m;

    DB_GetCursorPos(head, gen, flag, &pos, &m);
    DB_DrawCross3D(&pos, &m, size);
}

// Three axis lines of length `size` at `pos` oriented by `m`.
extern "C" void DB_DrawCross3D(Vec* pos, Mtx* m, f32 size)
{
    Vec p0;
    Vec p1;
    Vec ax;
    Vec ay;
    Vec az;

    size *= 320.0f;
    ax.x = size;
    ax.y = 0.0f;
    ax.z = 0.0f;
    ay.x = 0.0f;
    ay.y = size;
    ay.z = 0.0f;
    az.x = 0.0f;
    az.y = 0.0f;
    az.z = size;
    if (m) {
        PSMTXMultVecSR(*m, &ax, &ax);
        PSMTXMultVecSR(*m, &ay, &ay);
        PSMTXMultVecSR(*m, &az, &az);
    }
    p0 = *pos;
    PSVECAdd(&p0, &ax, &p1);
    Draw_line3d(&p0, &p1, 0xFF4040, 0);
    PSVECSubtract(&p0, &ax, &p1);
    Draw_line3d(&p0, &p1, 0xFFFFFF, 0);
    p0 = *pos;
    PSVECAdd(&p0, &ay, &p1);
    Draw_line3d(&p0, &p1, 0x40FF40, 0);
    PSVECSubtract(&p0, &ay, &p1);
    Draw_line3d(&p0, &p1, 0xFFFFFF, 0);
    p0 = *pos;
    PSVECAdd(&p0, &az, &p1);
    Draw_line3d(&p0, &p1, 0x4040FF, 0);
    PSVECSubtract(&p0, &az, &p1);
    Draw_line3d(&p0, &p1, 0xFFFFFF, 0);
}

// never called (dead-stripped body): its 0.0f pool word follows DB_DrawCross3D's pool
static void DB_VecClear(Vec* v)
{
    v->x = 0.0f;
    v->y = 0.0f;
    v->z = 0.0f;
}

// the translation part of *m from the computed position; written out in each arm (jump2 merges the tails)
#define SET_MTX_POS   \
    PSMTXIdentity(*m);     \
    (*m)[0][3] = out->x;   \
    (*m)[1][3] = out->y;   \
    (*m)[2][3] = out->z

// Transforms `in` by the sequence's null parts (head->parts) of the active model (0xFE = the
// model itself); logs an error for a bad parts number.
extern "C" void DB_VecNullPartsPos(EspSeqData* head, Vec* in, Vec* out, Mtx* m)
{
    cModel* em = dbModGetEmPtr(db_modelNo);
    Vec zero;
    Vec v;
    Mtx rm;
    Vec rot;

    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
    if (em && (em->be_flag & 1)) {
        u32 parts = head->parts;
        if (parts == 0xFE) {
            out->x = in->x + head->pos.x;
            out->y = in->y + head->pos.y;
            out->z = in->z + head->pos.z;
            SET_MTX_POS;
        } else if (parts < em->nParts) {
            cParts* p = em->getPartsPtr(parts);
            zero.x = 0.0f;
            zero.y = 0.0f;
            zero.z = 0.0f;
            PSMTXMultVec(p->mat, &zero, out);
            out->x += in->x + head->pos.x;
            out->y += in->y + head->pos.y;
            out->z += in->z + head->pos.z;
            SET_MTX_POS;
        } else {
            PSMTXIdentity(*m);
            out->z = out->y = out->x = 0.0f;
        }
        return;
    }
    if (head->parts <= 0xF7) {
        pLog->err(0, 0, "ESP_NULL: NULLpt[%d] Invalid.", head->parts);
    }
    PSMTXIdentity(*m);
    rot = head->rot;
    PSVECScale(&rot, &rot, 0.017453292f);
    RotMatrix(rm, &rot);
    PSMTXMultVec(rm, in, &v);
    out->x += v.x + head->pos.x;
    out->y += v.y + head->pos.y;
    out->z += v.z + head->pos.z;
    (*m)[0][3] = out->x;
    (*m)[1][3] = out->y;
    (*m)[2][3] = out->z;
}

// the "no parts" exit, written out in each arm (jump2 cross-jumps the copies into the last one)
#define NONE_BODY   \
    PSMTXIdentity(*m); \
    out->y = 0.0f;     \
    out->x = 0.0f

// Transforms `in` by parts `parts` of the generator's parent (a db_mod slot, or a scroll object
// for Parent_no > 0); no-op for a dead model or bad parts.
extern "C" void DB_VecMulEmPartsMat(u32 parts, Vec* in, Vec* out, Mtx* m, EspGenWork* gen)
{
    cModel* em = GetActiveModel(gen);
    Vec v;

    if (DB_isGetComeEventTool() == 1) {
        em = EspEvModList.GetModelPtr(gen->Parent_no);
        if (em == 0) {
            NONE_BODY;
            return;
        }
    } else if (gen->Parent_no != 0) {
        em = SmdGetObjPtr(gen->Parent_no - 1);
        if (em == 0) {
            NONE_BODY;
            return;
        }
    } else if (em == 0) {
        return;
    }
    if ((em->be_flag & 1) == 0) {
        return;
    }
    if (parts >= em->nParts) {
        goto none;
    }
    if (gen->Tool_flg & 0x20) {
        cParts* p = em->getPartsPtr(parts);
        PSMTXIdentity(*m);
        RotMatrix(*m, &em->ang);
        PSMTXMultVecSR(*m, in, &v);
        (*m)[0][3] = p->mat[0][3] + v.x;
        (*m)[1][3] = p->mat[1][3] + v.y;
        (*m)[2][3] = p->mat[2][3] + v.z;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(*m, &v, out);
    } else {
        cParts* p = em->getPartsPtr(parts);
        MtxPtr pm = p->mat;
        PSMTXMultVec(pm, in, out);
        PSMTXCopy(pm, *m);
        PSMTXMultVecSR(*m, in, &v);
        (*m)[0][3] += v.x;
        (*m)[1][3] += v.y;
        (*m)[2][3] += v.z;
    }
    return;
none:
    NONE_BODY;
}

// Writes `size` bytes to the host file (HDWrite).
extern "C" void SaveData(const char* path, void* buf, int size)
{
    int ret = HDWrite(path, buf, size);

    OSReport("EspTool:write end.\n");
    if (ret == 0) {
        OSReport("EspTool:write error\n");
        pLog->err(0, 0, "ESP_TOOL: DataSave failed!!!  '%s' ", path);
    }
    pLog->clear();
    pLog->warn(0, 0, "'%s' Save OK!", path);
}

// Reads a host file into `buf`; the byte count (0 when missing).
extern "C" int LoadData(const char* path, void* buf)
{
    int ret = HDRead(path, buf);

    OSReport("EspTool:read end.\n");
    if (ret == 0) {
        OSReport("EspTool:read error\n");
        pLog->err(0, 0, "ESP_TOOL: DataLoad failed!!!  '%s' ", path);
        return 0;
    }
    pLog->clear();
    pLog->warn(0, 0, "'%s' Load OK!", path);
    return ret;
}

int SetToolLight(int no);

// Opens the embedded db_light editor (tool light on).
extern "C" void LightToolStart()
{
    db_pLightTool = new cLightTool();
    SetToolLight(-1);
    db_pLightTool->setLogMode(0);
}

// Runs the embedded light editor one frame.
extern "C" void LightToolExec()
{
    db_pLightTool->move();
}

// Closes the embedded light editor.
extern "C" void LightToolEnd()
{
    if (db_pLightTool) {
        delete db_pLightTool;
    }
}

// Runs the debug camera on pad 1 (CamDbg).
extern "C" void EspToolCameraMode()
{
    CamDbg.move(&pG->Camera, &Joy[0], 1);
    eprintf(0xD0, 0x10, 0, 0, "CAMERA MODE");
}

// pG->stage_no.
extern "C" u8 DB_GetStageNo()
{
    return pG->stage_no;
}

// pG->room_no.
extern "C" u8 DB_GetRoomNo()
{
    return pG->room_no;
}

// World point `dist` units in front of the camera (new generator default position).
extern "C" void DB_GetCamFrontPos(f32 dist, f32* x, f32* y, f32* z)
{
    Vec dir;
    Vec pos;
    CAMERA* cam = &pG->Camera;

    dir.x = cam->param.at.x - cam->param.pos.x;
    dir.y = cam->param.at.y - cam->param.pos.y;
    dir.z = cam->param.at.z - cam->param.pos.z;
#line 2047 "D:/Bio4/Prog/db_port.cpp"
    VECNormalize(&dir, &dir);
    pos.x = cam->param.pos.x;
    pos.y = cam->param.pos.y;
    pos.z = cam->param.pos.z;
    PSVECScale(&dir, &dir, dist);
    PSVECAdd(&dir, &pos, &pos);
    *x = pos.x;
    *y = pos.y;
    *z = pos.z;
}

// TaskSleep(n).
extern "C" void DB_Sleep(int n)
{
    TaskSleep(n);
}

// Debug draw of a ctrl01 generator: its origin (parent parts applied) and direction vector.
extern "C" void sp_ctrl01_trans(EspGenWork* gen)
{
    cModel* em = GetActiveModel(gen);
    Mtx ry;
    Mtx rx;
    Mtx base;
    Vec dir;
    Vec p;
    int onModel;
    f32 ax;
    f32 ay;
    f32 half;
    u32 i;

    if (gen->Vec2.z == 0.0f) {
        return;
    }
    PSMTXIdentity(base);
    if (em && (em->be_flag & 1) && gen->Parts_no != ESP_PARTS_WORLD && gen->Parts_no != ESP_PARTS_SCREEN && gen->Parts_no != ESP_PARTS_NULL) {
        if (gen->Parts_no >= em->nParts) {
            return;
        }
        onModel = 1;
        PSMTXCopy(em->getPartsPtr(gen->Parts_no)->mat, base);
    } else {
        onModel = 0;
        base[0][3] = gen->Pos.x;
        base[1][3] = gen->Pos.y;
        base[2][3] = gen->Pos.z;
    }
    dir.x = 0.0f;
    dir.y = 0.0f;
    dir.z = 1500.0f;
    half = gen->Vec2.z * 6.2831855f / 360.0f * 0.5f;
    ay = gen->Vec2.x * 6.2831855f / 360.0f;
    ax = gen->Vec2.y * 6.2831855f / 360.0f;
    ay = LIMIT_ANGLE(ay);
    ax = LIMIT_ANGLE(ax);
    half = LIMIT_ANGLE(half);
    PSMTXRotRad(ry, 'Y', ax);
    PSMTXRotRad(rx, 'X', ay);
    PSMTXConcat(ry, rx, ry);
    PSMTXConcat(base, ry, ry);
    if (onModel) {
        PSMTXMultVec(ry, &gen->Pos, &p);
        PSVECAdd(&gen->Pos, &dir, &dir);
    } else {
        p = gen->Pos;
    }
    PSMTXMultVec(ry, &dir, &dir);
    Draw_line3d(&p, &dir, 0xFFFFFFFF, 0);
    for (i = 0; i < 32; i++) {
        f32 ang = (f32) i * 0.03125f * 2.0f * 3.1415927f;
        dir.x = sinf(ang) * 1500.0f;
        dir.y = cosf(ang) * 1500.0f;
        dir.z = 1500.0f;
        dir.x *= tanf(half);
        dir.y *= tanf(half);
        if (half > 1.570796f) {
            dir.z = -dir.z;
        }
        PSMTXRotRad(ry, 'Y', ax);
        PSMTXRotRad(rx, 'X', ay);
        PSMTXConcat(ry, rx, ry);
        PSMTXConcat(base, ry, ry);
        if (onModel) {
            PSVECAdd(&gen->Pos, &dir, &dir);
        }
        PSMTXMultVec(ry, &dir, &dir);
        Draw_line3d(&p, &dir, 0xFFFFFFFF, 0);
    }
}

// Debug draw of a generator's emission sphere (radius Vec0.z).
extern "C" void sp_sphere(EspSeqData* head, EspGenWork* gen)
{
    Vec pos;
    Mtx m;

    DB_GetCursorPos(head, gen, 0, &pos, &m);
    Draw_sphere(&pos, gen->Vec0.z, 0xFFFFFFFF, 1, 1);
}

// Debug draw of a generator's emission box (Vec0 half sizes) in green.
extern "C" void sp_3dgrid_trans(EspSeqData* head, EspGenWork* gen)
{
    Mtx m;
    Mtx rm;
    Vec pos;
    Vec v[4];
    Vec rot;
    EspAnmData* anm;
    f32 w;
    f32 h;
    f32 gw;
    f32 gh;
    f32 sx;
    f32 sy;

    if (EspGetAnmAddr(gen->Tex_id, &anm) == 0) {
        return;
    }
    DB_GetCursorPos(head, gen, 0, &pos, &m);
    gw = gen->Size_base_x;
    gh = gen->Size_base_y;
    w = (f32) -anm->Cx;
    h = (f32) anm->Cy;
    if (w == 0.0f) {
        w = (f32) -(int) anm->Width * 0.5f;
    }
    if (h == 0.0f) {
        h = (f32) (int) anm->Height * 0.5f;
    }
    sx = w * gw / (f32) (int) anm->Width;
    sy = h * gh / (f32) (int) anm->Height;
    v[0].x = sx;
    v[0].y = sy;
    v[0].z = 1.0f;
    v[1].x = sx + gw;
    v[1].y = sy;
    v[1].z = 1.0f;
    v[2].x = sx + gw;
    v[2].y = sy - gh;
    v[2].z = 1.0f;
    v[3].x = sx;
    v[3].y = sy - gh;
    v[3].z = 1.0f;
    PSVECScale(&gen->Ang, &rot, 0.017453289f);
    RotMatrix(rm, &rot);
    PSMTXConcat(m, rm, m);
    PSMTXMultVecSR(m, &v[0], &v[0]);
    PSMTXMultVecSR(m, &v[1], &v[1]);
    PSMTXMultVecSR(m, &v[2], &v[2]);
    PSMTXMultVecSR(m, &v[3], &v[3]);
    PSVECAdd(&pos, &v[0], &v[0]);
    PSVECAdd(&pos, &v[1], &v[1]);
    PSVECAdd(&pos, &v[2], &v[2]);
    PSVECAdd(&pos, &v[3], &v[3]);
    Draw_line3d(&v[0], &v[1], 0xFF00FF00, 0);
    Draw_line3d(&v[1], &v[2], 0xFF00FF00, 0);
    Draw_line3d(&v[2], &v[3], 0xFF00FF00, 0);
    Draw_line3d(&v[3], &v[0], 0xFF00FF00, 0);
}

// Debug draw of an esp06 path effect: the path sampled along its length (on the parent model when
// it has one).
extern "C" void sp_path_trans(EspSeqData* head, EspGenWork* gen)
{
    cModel* em = GetActiveModel(gen);
    cEsp* esp;
    DbPathEsp* pe;
    DbPathWork* pw;
    Vec pos;
    Vec old;
    struct { u32 v; } seed;  // in-struct store: the vptr load of the virtual call stays below it
    f32 len;
    f32 t;
    u32 i;

    if (gen->Parent_no != 0) {
        em = SmdGetObjPtr(gen->Parent_no - 1);
        if (em == 0) {
            return;
        }
    }
    if (PullEsp(&esp, 6) == 0) {
        return;
    }
    seed.v = 0x12345678;
    pe = (DbPathEsp*) esp;
    pw = &pe->w;
    if (esp->SetFreeWork(gen, &seed.v) == 0) {
        PushEsp(&pe->esp);
        return;
    }
    len = PathGetLength(pw->path);
    EspGetPathAddr(pw->id, pw->owner);
    t = 0.0f;
    for (i = 0; i < 256; i++) {
        if (em && (em->be_flag & 1) && gen->Parts_no <= 0xF7) {
            PathGetPosEm(pw->path, em, t, &pw->seg, &pos);
        } else {
            PathGetPos(pw->path, t, &pw->seg, &pos);
        }
        if (pw->flags & 0x80) {
            PSMTXMultVec(pw->mtx, &pos, &pos);
        }
        PSVECAdd(&pos, &gen->Pos, &pos);
        if (i != 0) {
            Draw_line3d(&pos, &old, 0xFFFFFFFF, 0);
        }
        old = pos;
        t += len * 0.00390625f;
    }
    PushEsp(&pe->esp);
}

// Debug draw of an Espgen02 path generator: the path with the generator's rotation / scale.
extern "C" void sp_path_trans2(EspSeqData* head, EspGenWork* gen)
{
    cModel* em = GetActiveModel(gen);
    Vec pos;
    Vec old;
    EspgenWork wk;
    Mtx mtx;
    Mtx sm;
    Vec p158;
    Vec p168;
    u16 seg;
    EspgenWork* pw = &wk;
    DbEspgen02* w = (DbEspgen02*) wk.work;
    void* path;
    f32 len;
    f32 t;
    u32 i;

    memclr_asm(pw, sizeof(EspgenWork));
    seg = 0;
    if (gen->Parent_no != 0) {
        em = SmdGetObjPtr(gen->Parent_no - 1);
        if (em == 0) {
            return;
        }
    }
    if (Espgen02_SetFreeWork(pw, gen, head, em, 0xFE, &mtx, &p168, &p158, 0, 0) == 0) {
        return;
    }
    path = EspGetPathAddr(w->pathOwner, w->pathId);
    if (path == 0) {
        return;
    }
    len = PathGetLength(path);
    t = 0.0f;
    for (i = 0; i < 256; i++) {
        if (em && PathHasWeight(path)) {
            PathGetPosEm(path, em, t, &seg, &pos);
        } else {
            PathGetPos(path, t, &seg, &pos);
        }
        if (w->flags19 & 4) {
            PSMTXIdentity(sm);
            PSMTXScale(sm, w->scaleX, w->scaleY, w->scaleZ);
            PSMTXMultVec(sm, &pos, &pos);
        }
        Vec rot;
        Mtx rm;
        rot.x = (f32) w->rotX * 6.2831855f * 0.00390625f;
        rot.y = (f32) w->rotY * 6.2831855f * 0.00390625f;
        rot.z = 0.0f;
        RotMatrix(rm, &rot);
        PSMTXMultVec(rm, &pos, &pos);
        if (em && (em->be_flag & 1) && gen->Parts_no <= 0xF7) {
            PSMTXMultVec(em->getPartsPtr(gen->Parts_no)->mat, &pos, &pos);
        }
        PSVECAdd(&pos, &gen->Pos, &pos);
        if (i != 0) {
            Draw_line3d(&pos, &old, 0xFFFFFFFF, 0);
        }
        old = pos;
        t += len * 0.00390625f;
    }
}

// Debug draw of a generator's extension limit rectangle (Vec0 x/z).
extern "C" void sp_nobigenkai_trans(EspSeqData* head, EspGenWork* gen)
{
    Vec c;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec v3;

    if (gen->Vec0.x == 0.0f && gen->Vec0.z == 0.0f) {
        return;
    }
    PSVECAdd(&gen->Pos, &gen->Vec1, &c);
    v0 = c;
    v0.x += gen->Vec0.x;
    v0.z += gen->Vec0.z;
    v1 = c;
    v1.x -= gen->Vec0.x;
    v1.z += gen->Vec0.z;
    v2 = c;
    v2.x -= gen->Vec0.x;
    v2.z -= gen->Vec0.z;
    v3 = c;
    v3.x += gen->Vec0.x;
    v3.z -= gen->Vec0.z;
    Draw_line3d(&v0, &v1, 0xFFFFFFFF, 0);
    Draw_line3d(&v1, &v2, 0xFFFFFFFF, 0);
    Draw_line3d(&v2, &v3, 0xFFFFFFFF, 0);
    Draw_line3d(&v3, &v0, 0xFFFFFFFF, 0);
}

// Debug draw of a position-random generator bound to a parts: a sphere at the parts.
extern "C" void sp_PosRand_trans_1a(EspSeqData* head, EspGenWork* gen)
{
    cModel* em = GetActiveModel(gen);
    cParts* p0;
    cParts* p1;
    Vec a;
    Vec b;
    Vec ofs;
    f32 r;

    if (em == 0 || !(em->be_flag & 1)) {
        return;
    }
    if (gen->Parts_no >= em->nParts) {
        return;
    }
    p0 = em->getPartsPtr(gen->Parts_no);
    if ((s8) gen->Work8[0] >= em->nParts) {
        return;
    }
    p1 = em->getPartsPtr((s8) gen->Work8[0]);
    ofs = gen->Pos;
    PSMTXMultVec(p0->mat, &ofs, &a);
    Vec v = {0.0f, 0.01f, 0.0f};
    Mtx inv;
    PSMTXMultVec(p1->mat, &v, &v);
    PSMTXInverse(p0->mat, inv);
    PSMTXMultVec(inv, &v, &v);
    PSVECAdd(&v, &gen->Pos, &v);
    PSMTXMultVec(p0->mat, &v, &b);
    r = gen->R_pos.z;
    if (r == 0.0f) {
        r = 100.0f;
    }
    Draw_sphere(&a, r, 0xFFFFFFFF, 1, 1);
    Draw_sphere(&b, r, 0xFFFFFFFF, 1, 1);
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
}

// Debug draw of a position-random generator's box (yellow) in the parent's frame.
extern "C" void sp_PosRand_trans(EspSeqData* head, EspGenWork* gen)
{
    f32 rx = gen->R_pos.x;
    f32 ry = gen->R_pos.y;
    f32 rz = gen->R_pos.z;
    Mtx m;  // the first local: its address is the frame pointer itself, so every `m` use is a fresh `addi r3,r1,8`
    Vec pos;
    Vec v[8];

    if (gen->Id == 0x1A) {
        sp_PosRand_trans_1a(head, gen);
        return;
    }
    if (rx == 0.0f && ry == 0.0f && rz == 0.0f) {
        return;
    }
    DB_GetCursorPos(head, gen, 0, &pos, &m);
    v[0].x = -rx;
    v[0].y = -ry;
    v[0].z = -rz;
    v[1].x = rx;
    v[1].y = -ry;
    v[1].z = -rz;
    v[2].x = rx;
    v[2].y = ry;
    v[2].z = -rz;
    v[3].x = -rx;
    v[3].y = ry;
    v[3].z = -rz;
    v[4].x = -rx;
    v[4].y = -ry;
    v[4].z = rz;
    v[5].x = rx;
    v[5].y = -ry;
    v[5].z = rz;
    v[6].x = rx;
    v[6].y = ry;
    v[6].z = rz;
    v[7].x = -rx;
    v[7].y = ry;
    v[7].z = rz;
    PSMTXMultVec(m, &v[0], &v[0]);
    PSMTXMultVec(m, &v[1], &v[1]);
    PSMTXMultVec(m, &v[2], &v[2]);
    PSMTXMultVec(m, &v[3], &v[3]);
    PSMTXMultVec(m, &v[4], &v[4]);
    PSMTXMultVec(m, &v[5], &v[5]);
    PSMTXMultVec(m, &v[6], &v[6]);
    PSMTXMultVec(m, &v[7], &v[7]);
    Draw_line3d(&v[0], &v[1], 0xFFF0FF00, 0);
    Draw_line3d(&v[1], &v[2], 0xFFF0FF00, 0);
    Draw_line3d(&v[2], &v[3], 0xFFF0FF00, 0);
    Draw_line3d(&v[3], &v[0], 0xFFF0FF00, 0);
    Draw_line3d(&v[4], &v[5], 0xFFF0FF00, 0);
    Draw_line3d(&v[5], &v[6], 0xFFF0FF00, 0);
    Draw_line3d(&v[6], &v[7], 0xFFF0FF00, 0);
    Draw_line3d(&v[7], &v[4], 0xFFF0FF00, 0);
    Draw_line3d(&v[0], &v[4], 0xFFF0FF00, 0);
    Draw_line3d(&v[1], &v[5], 0xFFF0FF00, 0);
    Draw_line3d(&v[2], &v[6], 0xFFF0FF00, 0);
    Draw_line3d(&v[3], &v[7], 0xFFF0FF00, 0);
}

// the definition takes `int` (t_esp.cpp declares it `u8`): the entry `clrlwi r28,r3,24` is the (u8) cast
extern "C" void sp_tex_trans(int no)
{
    void* tpl;
    u32 owner;
    Vec p0;
    Vec p1;
    GXTexObj obj;
    GXTlutObj tlut;
    f32 r;
    f32 g;
    f32 b;
    u32 n;

    n = (u8) no;
    p0.z = 1.0f;
    p1.z = 1.0f;
    r = 0.2f;
    g = 0.6f;
    b = 0.8f;
    if (EspGetTplAddr(n, &tpl)) {
        GXTexObj* o = &obj;
        GXTlutObj* t = &tlut;
        TEXDescriptor* tex = TEXGet((TEXPalette*) tpl, 0);
        GXInitTexObjCI(o, tex->textureHeader->data, tex->textureHeader->width, tex->textureHeader->height,
                       tex->textureHeader->format, 0, 0, 0, 1);
        if (tex->textureHeader->format - 8 <= 1) {
            GXInitTlutObj(t, tex->CLUTHeader->data, tex->CLUTHeader->format, tex->CLUTHeader->numEntries);
            GXLoadTlut(t, 1);
        }
        DrawTexture(o, db_texX, db_texY, db_texZ, db_texW, db_texH);
        if (EspGetTexOwner(n, &owner)) {
            if (owner == 0) {
                g = b;
                b = r;
            }
            if (owner == 1) {
                asm("" : "+r"(r)); // COMPILER-DIFF: candidate #12 (cse skip-block knowledge: the original reloads 0.2 from the pool)
                b = 0.2f;
                r = 0.8f;
                g = b;
            }
        }
    } else {
        p0.x = (f32) db_texX;
        p0.y = (f32) db_texY;
        p1.x = (f32) (db_texX + db_texW);
        p1.y = (f32) (db_texY + db_texH);
        Draw_line(&p0, &p1, MakeCol(0.1f, 0.1f, 0.1f, 0.7f));
        p0.x = (f32) (db_texX + db_texW);
        p0.y = (f32) db_texY;
        p1.x = (f32) db_texX;
        p1.y = (f32) (db_texY + db_texH);
        Draw_line(&p0, &p1, MakeCol(0.1f, 0.1f, 0.1f, 0.7f));
        g = 0.4f;
        r = g;
        b = g;
    }
    p0.x = (f32) db_texX;
    p0.y = (f32) db_texY;
    p1.x = (f32) (db_texX + db_texW);
    p1.y = (f32) db_texY;
    Draw_line(&p0, &p1, MakeCol(r, g, b, 0.8f));
    p0.x = (f32) (db_texX + db_texW);
    p0.y = (f32) db_texY;
    p1.x = (f32) (db_texX + db_texW);
    p1.y = (f32) (db_texY + db_texH);
    Draw_line(&p0, &p1, MakeCol(r, g, b, 0.8f));
    p0.x = (f32) (db_texX + db_texW);
    p0.y = (f32) (db_texY + db_texH);
    p1.x = (f32) db_texX;
    p1.y = (f32) (db_texY + db_texH);
    Draw_line(&p0, &p1, MakeCol(r, g, b, 0.8f));
    p0.x = (f32) db_texX;
    p0.y = (f32) (db_texY + db_texH);
    p1.x = (f32) db_texX;
    p1.y = (f32) db_texY;
    Draw_line(&p0, &p1, MakeCol(r, g, b, 0.8f));
}

// Deletes every running effect.
extern "C" void DB_EffDelete()
{
    EffectDeleteAll();
}

// Frame counter display: frames since the last X press, and the current event cut / frame.
extern "C" void DB_DispProc()
{
    static int start;
    int d;

    if (start == 0xFFFF) {
        start = g_proc_cnt;
    }
    if (Joy[0].trg & 0x400) {
        start = 0xFFFF;
    }
    d = g_proc_cnt - start;
    if (d < 0) {
        d = 0;
    }
    eprintf2(10, 16, 0x28, 0x10, 0, 0xD, "/%d", d);
    if (db_frameCnt) {
        eprintf2(10, 16, 0x78, 0x10, 0, 0xD, "cut=%x %d", db_cutNo, db_frameCnt);
    }
}

// Config parser: skips a [[ ]], /* */ or // comment at *pp; 0 when one was skipped, -1 otherwise.
extern "C" int comment_check(char** pp)
{
    char* p = *pp;

    if (strncmp(p, "[[", 2) == 0) {
        p += 2;
        while (strncmp(p, "]]", 2) != 0) {
            p++;
        }
        p += 2;
    } else if (strncmp(p, "/*", 2) == 0) {
        p += 2;
        while (strncmp(p, "*/", 2) != 0) {
            p++;
        }
        p += 2;
    } else if (strncmp(p, "//", 2) == 0) {
        while (*p != '\n') {
            p++;
        }
        p++;
    } else {
        return 0;
    }
    *pp = p;
    return -1;
}

// Config parser: skips blanks / tabs / newlines.
extern "C" char* space_skip(char* p)
{
    do {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
    } while (comment_check(&p));
    return p;
}

// Config parser: reads a decimal or 0x hex number at *pp.
extern "C" int num_get(char** pp)
{
    *pp = space_skip(*pp);
    if (strncmp(*pp, "0x", 2) == 0) {
        return strtol(*pp, pp, 16);
    }
    return strtol(*pp, pp, 10);
}

// a macro, not an inline: the "ON"/"on" strings must be emitted at the TRANS branch of DB_ConfigLoad
// (an inline body emits them at its definition, before "ESPTOOL:open error!")
#define symbolOnOff(pp) ((symbol_check(pp, "ON") || symbol_check(pp, "on")) ? 1 : 0)

// Reads a model set config (.cfg) from the host: MODEL_NAME / MOTION_NO / TRANS / LOOP / FLIP /
// POS_* / ANG_* / PAR_* entries per model, then loads every model set into the viewer slots with
// its motion flags, position and parent link. 0 when the file is missing.
extern "C" int DB_ConfigLoad(const char* file)
{
    DbConfigModel tbl[10];
    DbConfigModel* cur = tbl;
    char* p;
    char* buf;
    char* end;
    int num;
    int len;
    u32 i;

    memclr_asm(tbl, sizeof(tbl));
#line 2752 "D:/Bio4/Prog/db_port.cpp"
    buf = (char*) MEM_CALLOC(0x1000, 1, 0xD);
    len = HDRead(file, buf);
    if (len == 0) {
        pLog->err(0, 0, "ESPTOOL:open error! [%s]", file);
        Mem_free(buf);
        return 0;
    }
    end = buf + len;
    num = 0;
    p = buf;
    while (p < end) {
        p = space_skip(p);
        if (*p++ == '[') {
            if (symbol_check(&p, "MODEL_NAME")) {
                // `if (num++ != 0)` puts the increment between the compare and the branch; the name index
                // is the function's `i` (reused by the load loop below: one pseudo, r30 in both) and `*p` is
                // read before the byte store so `p++` re-reads p after it (the store may alias the slot)
                if (num++ != 0) {
                    cur++;
                }
                i = 0;
                while (*p != '\r') {
                    cur->name[i++] = *p;
                    p++;
                }
            } else if (symbol_check(&p, "MOTION_NO")) {
                cur->motNo = num_get(&p);
            } else if (symbol_check(&p, "TRANS")) {
                int mode;
                if (symbol_check(&p, "ON") || symbol_check(&p, "on")) {
                    mode = 0;
                } else if (symbol_check(&p, "OFF") || symbol_check(&p, "off")) {
                    mode = 1;
                } else {
                    mode = 2;
                }
                SetTransMode(mode, num - 1);
            } else if (symbol_check(&p, "LOOP")) {
                SetLoopFlag(symbolOnOff(&p), num - 1);
            } else if (symbol_check(&p, "FLIP")) {
                SetXFlipFlag(symbolOnOff(&p), num - 1);
            } else if (symbol_check(&p, "POS_X")) {
                cur->pos.x = (f32) num_get(&p);
            } else if (symbol_check(&p, "POS_Y")) {
                cur->pos.y = (f32) num_get(&p);
            } else if (symbol_check(&p, "POS_Z")) {
                cur->pos.z = (f32) num_get(&p);
            } else if (symbol_check(&p, "ANG_X")) {
                cur->ang.x = (f32) (num_get(&p) * 2) * 3.1415927f / 360.0f;
            } else if (symbol_check(&p, "ANG_Y")) {
                cur->ang.y = (f32) (num_get(&p) * 2) * 3.1415927f / 360.0f;
            } else if (symbol_check(&p, "ANG_Z")) {
                cur->ang.z = (f32) (num_get(&p) * 2) * 3.1415927f / 360.0f;
            } else if (symbol_check(&p, "PAR_ON")) {
                cur->parOn = num_get(&p);
            } else if (symbol_check(&p, "PAR_NO")) {
                cur->parNo = num_get(&p);
            } else if (symbol_check(&p, "PAR_PT_NO")) {
                cur->parPtNo = num_get(&p);
            } else if (symbol_check(&p, "PAR_POS_X")) {
                cur->parPos.x = (f32) num_get(&p);
            } else if (symbol_check(&p, "PAR_POS_Y")) {
                cur->parPos.y = (f32) num_get(&p);
            } else if (symbol_check(&p, "PAR_POS_Z")) {
                cur->parPos.z = (f32) num_get(&p);
            } else if (symbol_check(&p, "PAR_ANG_X")) {
                cur->parAng.x = (f32) (num_get(&p) * 2) * 3.1415927f / 360.0f;
            } else if (symbol_check(&p, "PAR_ANG_Y")) {
                cur->parAng.y = (f32) (num_get(&p) * 2) * 3.1415927f / 360.0f;
            } else if (symbol_check(&p, "PAR_ANG_Z")) {
                cur->parAng.z = (f32) (num_get(&p) * 2) * 3.1415927f / 360.0f;
            }
        }
    }
    for (i = 0; i < num; i++) {
        DbConfigModel* c = &tbl[i];
        if (c->name[0] == 0) {
            continue;
        }
        if (LoadModelSetName(c->name, c->motNo, i) == 0) {
            continue;
        }
        dbModelSetPos0(i, &c->pos);
        dbModelSetAng0(i, &c->ang);
        LoadModEff();
        if (c->parOn) {
            dbModelParentChild((s8) i, ((s8*) &c->parNo)[3], ((s8*) &c->parPtNo)[3], &c->parPos, &c->parAng);
        }
    }
    Mem_free(buf);
    return 1;
}

// Config parser: 1 and advance when `sym` is at *pp.
extern "C" int symbol_check(char** pp, const char* sym)
{
    int len = strlen(sym);

    if (strncmp(*pp, sym, len) == 0) {
        char* p = *pp;
        *pp = p + len;
        if (p[len] == ']') {
            *pp = *pp + 1;
        }
        *pp = space_skip(*pp);
        return 1;
    }
    return 0;
}

// Plays core effect `id` at the origin (EstSet without an owner).
extern "C" void CoreEstSet(u8 id)
{
    EstSet(0, -1, 0, 0, EFF_CORE, id, 1, ESP_CORE_KIND_NONE, 0, 0);
}

// Blits a texture object to the screen (the texture preview window).
extern "C" void drawTexture2(GXTexObj* obj, s16 x, s16 y, s16 z, s16 w, s16 h)
{
    GXColor col;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    col.a = 0xFF;
    col.b = 0xFF;
    col.g = 0xFF;
    col.r = 0xFF;
    GXSetChanAmbColor(0, col);
    GXSetChanMatColor(0, col);
    Mtx m;
    Mtx44 proj;
    PSMTXIdentity(m);
    GXLoadTexObj(obj, 0);
    GXLoadTexMtxImm(m, 30, 1);
    GXSetTexCoordGen2(0, 1, 4, 30, 0, 0x7D);
    GXSetNumTexGens(1);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 3);
    C_MTXOrtho(proj, 0.0f, (f32) (u32) Screen.height, 0.0f, (f32) (u32) Screen.width, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(m);
    GXLoadPosMtxImm(m, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3s16(x, y, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3s16(x + w, y, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(x + w, y + h, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(x, y + h, z);
    GXTexCoord2f32(0.0f, 1.0f);
}
