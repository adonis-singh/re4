#ifndef T_ID_H
#define T_ID_H

// t_id REL (ToolInterfaceDesign): 2D interface (HUD/sub screen) designer.

#include "types.h"
#include "model.h"
#include "camera.h"
#include "db_sctrl.h"
#include "db_path.h"
#include "path.h"
#include "hermite.h"

#define ID_PATH_MAX 0x40
#define ID_CURVE_MAX 0x40

// FuncPathData with room for ID_PATH_MAX control points (0x308 bytes).
struct IdPathData {
    s8 k;              // 0x00
    u8 pad_1[6];
    s8 n;              // 0x07
    Vec pos[ID_PATH_MAX];  // 0x08
};

// Hermite1 with room for ID_CURVE_MAX keys (0x404 bytes).
struct IdCurve {
    s32 num;                    // 0x00
    HermiteKey key[ID_CURVE_MAX];  // 0x04
};

// One editable interface element (0x17D0 bytes, 0xC0 of them in idData[]). Derives from cCoord so
// the tool can reuse its matrices; the cUnit be_flag doubles as the tool state word (0xFF = free).
class ID_DATA : public cCoord {
public:
    int xF4;             // 0xF4
    u8 mark;             // 0xF8  mark number (0xFF = none)
    u8 unitNo;           // 0xF9
    u8 level;            // 0xFA
    u8 parentNo;         // 0xFB  (0xFF = root)
    u8 xFC;              // 0xFC
    u8 no;               // 0xFD  slot inside the parent
    u8 kind;             // 0xFE  1: group
    u8 Id;               // 0xFF  file record Id byte (PS2 ID_DATA_V2 Id)
    u8 texId;            // 0x100 (0xFF = none)
    u8 pad_101[7];
    u8 vtxType;          // 0x108 high nibble: anchor
    u8 loop_flag;        // 0x109 bit0..3: path0, path1, curve loops (IdRec loop) (PS2 ID_DATA_V2 loop_flag)
    u8 flags10A;         // 0x10A 0x80: no texture size fetch
    u8 rotAxis;          // 0x10B
    u8 dir;              // 0x10C
    u8 pad_10D[0xB];
    Vec pos;             // 0x118
    Vec vtx[4];          // 0x124
    f32 sizeX;           // 0x154
    f32 sizeY;           // 0x158
    u8 pad_15C[8];
    u8 col0[4];          // 0x164
    u8 col1[4];          // 0x168
    u8 pad_16C[0x10];
    Vec rot;             // 0x17C
    u8 pad_188[0xC];
    u8 transType;        // 0x194  0..4
    u8 pad_195[4];
    u8 transMode;        // 0x199  0..3
    u8 maskTex;          // 0x19A
    u8 maskSw;           // 0x19B  bit0
    u8 pad_19C[4];
    u8 power;            // 0x1A0
    u8 pad_1A1[7];
    IdPathData path0;    // 0x1A8
    IdPathData path1;    // 0x4B0
    IdCurve curve0;      // 0x7B8
    IdCurve curve1;      // 0xBBC
    IdCurve curve2;      // 0xFC0
    IdCurve curve3;      // 0x13C4
    u8 x17C8[8];         // 0x17C8

    virtual ~ID_DATA() {}
};

#define ID_DATA_NUM 0xC0
#define ID_CLIP_NUM 0x20

struct IdRandomWork {
    int x;      // 0x00
    int y;      // 0x04
    s8 cur;     // 0x08
    u8 cnt;     // 0x09
    u8 pad_A[2];
    s16* pVal;  // 0x0C  amp / cont / intr
};

// Tool work (0x284 bytes; idToolWork, reached through pIdTool).
struct IdTool {
    s8 mode;            // 0x00  0 prev, 1 menu, 2 main
    s8 menuCur;         // 0x01
    s8 editMode;        // 0x02
    s8 editStep;        // 0x03
    s8 subStep;         // 0x04
    u8 pad_5[6];
    s8 subCur;          // 0x0B
    int scrW;           // 0x0C
    int scrH;           // 0x10
    int menuX;          // 0x14
    int menuY;          // 0x18
    u8 cnt;             // 0x1C
    u8 x1D;             // 0x1D
    u8 level;           // 0x1E
    s8 editSel;         // 0x1F
    u8 no;              // 0x20
    u8 listTop;         // 0x21  first row of the 8-row unit list shown
    u8 pad_22[2];
    int x24;            // 0x24
    u8 parentNo;        // 0x28
    u8 pad_29[3];
    Mtx mat;            // 0x2C
    u8 pause;           // 0x5C
    u8 grpSw;           // 0x5D
    u8 focus;           // 0x5E
    u8 drawSafe;        // 0x5F
    u8 dispTop;         // 0x60  edit list drawn at the top rows
    u8 pad_61[3];
    DbPathWork* pPath;  // 0x64
    DbSctrlWork* pSctrl;  // 0x68
    IdRandomWork* pRandom;  // 0x6C
    CAMERA camSave;     // 0x70 .. 0x168
    s8 colCur;          // 0x168
    s8 rotCur;          // 0x169
    s8 gridLv;          // 0x16A
    u8 pad_16B;
    Vec grid;           // 0x16C
    s8 lang;            // 0x178
    s8 lang2;           // 0x179  (s8 like lang: toolIdOption's `lang2 != lang` is a QI compare, loads lang2 first)
    s8 type;            // 0x17A  sub screen kind (index of subScreenName)
    u8 type2;           // 0x17B  type of the loaded .eff (as lang2 is to lang)
    u8 fileNo;          // 0x17C  <sub screen>%03d.uwf file number
    u8 reload;          // 0x17D  toolIdFile: bit0 reload the type's .eff, bit1 reload ckpt/share (lang changed)
    s8 prevCur;         // 0x17E
    u8 focusCnt;        // 0x17F
    u8 pad_180[2];
    u8 useCnt;          // 0x182
    u8 empCnt;          // 0x183
    u8 markUse[0x100];  // 0x184
};

extern "C" {
void toolIdDrawSafeZone(IdTool* w);
void toolIdSubMenuPosition(IdTool* w);
void toolIdDataInit(ID_DATA* d);
ID_DATA* toolIdGetPtrU(u8 unitNo);
ID_DATA* toolIdGetPtrPR(u8 parentNo, u8 no);
void toolIdPush(ID_DATA* d, IdTool* w);
ID_DATA* toolIdPull();
int idEditUnit(IdTool* w, int x, int y);
int idEditNo(IdTool* w, int x, int y);
int idEditId(IdTool* w, int x, int y);
int idEditPos(IdTool* w, int x, int y);
int idEditSize(IdTool* w, int x, int y);
int idEditColor(IdTool* w, int x, int y);
int idEditRot(IdTool* w, int x, int y);
int idEditTrans(IdTool* w, int x, int y);
int idEditMark(IdTool* w, int x, int y);
void toolIdEditDisp(IdTool* w);
int toolIdDataEncode(void* buf, IdTool* w);
int toolIdDataDecode(void* buf, IdTool* w);
void toolIdLevel(ID_DATA* d, u8 level);
int toolIdGroup(ID_DATA* d);
void toolIdSelectClear();
int toolIdCountSelected();
ID_DATA* toolIdCopy(ID_DATA* d, u8 level);
void toolIdPaste(u8 parentNo, u8 no);
void toolIdDelete(ID_DATA* d, IdTool* w);
int toolIdCount(u8 level, u32 mode);
void toolIdClipboardClear();
int toolIdClipboardCount(u8 level, u32 mode);
void toolIdSpace(u8 parentNo, u8 no, int n);
int toolIdColorAttr(ID_DATA* d);
void toolIdSetCamera(IdTool* w);
void toolIdGridLock(Vec* grid, Vec* in, Vec* out);
void toolIdCalcInitMatrix(ID_DATA* d, Mtx m);
void toolIdFocusOn(IdTool* w, ID_DATA* d);
void toolIdFocusReset(IdTool* w, ID_DATA* d);
void toolIdCalcVertex(ID_DATA* d);
void toolIdMarkUseReset(IdTool* w);
int DbRandom(IdRandomWork* w, int x, int y);
}

void ToolInterfaceDesign();

#endif
