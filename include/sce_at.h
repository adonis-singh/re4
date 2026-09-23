#ifndef SCE_AT_H
#define SCE_AT_H

#include "types.h"
#include "vec.h"
#include "area.h"
#include "scheduler.h"
#include "item.h"
#include "cFlag.h"

class cObj;
class cModel;
class cEm;
class cSat;

// Scenario trigger areas ("AT", game/sce_at.cpp): the room's AEV/ITA data records and the
// runtime-created ones, linked into an ordering table by their x44 priority.

// Item area payload (SceAtWork + 0x5C, type 3).
struct SceAtItem {
    Vec pos;          // 0x00 (0x5C)
    cModel* pModel;   // 0x0C (0x68)  item model: a cObj (setItemObj) or a cEmItem (shoot-down item)
    Vec ofs;          // 0x10 (0x6C)  effect offset
    u16 id;           // 0x1C (0x78)  item id
    u16 flagNo;       // 0x1E (0x7A)  ITEM_SET flag (pG->Item_flg), 0 = use the room save flag
    u16 num;          // 0x20 (0x7C)
    u16 findFlagNo;   // 0x22 (0x7E)  room save flag of the "found" state (0 = none)
    u8 effType;       // 0x24 (0x80)  item glow effect colour (sceAtCheckItemEffectCol)
    u8 effNo;         // 0x25 (0x81)  running effect number (EspPullCoreKind), 0 = none
    u8 modeMask;      // 0x26 (0x82)  game modes the item exists in (0 = all)
    u8 flag;          // 0x27 (0x83)  bit1 model loaded by itemZoom, bit2 model shown for the zoom, bit3 model kept in p_imodel_bak
    u8 flag2;         // 0x28 (0x84)  bit0 handed to an enemy, bit1 no auto area, bit2 no hit check, bit3 save work,
                      //              bit4 shoot-down item, bit5 disappear timer running, bit6 dropped item, bit7 no button
    u8 timer;         // 0x29 (0x85)  disappear timer (halves of a second)
    s16 saveNo;       // 0x2A (0x86)  pG->save_item index, -1 = none
    f32 size;         // 0x2C (0x88)  auto area radius (0 = 100)
    Vec rot;          // 0x30 (0x8C)  model rotation; rot.z != 0 = apply it
    u8 seFind;        // 0x3C (0x98)  SE when the item is found
    u8 seDamage;      // 0x3D (0x99)  SE when the item is shot
    u8 pad_3E[2];
};

// Door area payload (type 1).
struct SceAtDoor {
    Vec dstPos;       // 0x00 (0x5C)  destination position
    f32 dstAngle;     // 0x0C (0x68)
    u8 dstStage;      // 0x10 (0x6C)
    u8 dstRoom;       // 0x11 (0x6D)
    u8 lockType;      // 0x12 (0x6E)  1 locked, 2 locked until the flag is set
    u8 lockFlag;      // 0x13 (0x6F)  pG->flags_51DC bit
    void (*func)();   // 0x14 (0x70)  SceSys.x10 / x14 handed over at the jump (SceAtSetDoorFunc)
    u8 dstPart;       // 0x18 (0x74)  pG->Part in the destination room
    s8 se;            // 0x19 (0x75)  locked door SE
    u8 doorNo;        // 0x1A (0x76)  pG->door_no
    u8 fadeEff;       // 0x1B (0x77)  SceSys.m_door_fade_eff (2 = execute now, SceChapterEnd)
    int arg;          // 0x1C (0x78)
};

// Camera control area payload (type 0xC).
struct SceAtCamCtrl {
    Vec pos;          // 0x00 (0x5C)
    f32 angle;        // 0x0C (0x68)  (PS2 SCE_AT_DATA_CAM_CTRL ang_y)
    u8 pos_set;       // 0x10 (0x6C)  pos / ranges initialised (t_sce_at) (PS2 pos_set)
    u8 mode;          // 0x11 (0x6D)  0 heading, 1 direction to pos (PS2 type)
    u8 pad_12[2];
    f32 range;        // 0x14 (0x70)
    f32 range2;       // 0x18 (0x74)  added to range while this area is the current one
};

// Ladder area payload (type 0x10).
struct SceAtLadder {
    Vec pos;          // 0x00 (0x5C)
    f32 angle;        // 0x0C (0x68)
    s8 level;         // 0x10 (0x6C)
    u8 pad_11[2];
    u8 cut1;          // 0x13 (0x6F)  camera cuts + 1 (0 = none)
    u8 cut2;          // 0x14 (0x70)
    u8 cut3;          // 0x15 (0x71)
};

// Runtime scenario collision payload (type 0xB).
struct SceAtScrAt {
    cSat* pSat;       // 0x00 (0x5C)
    cSat* pEat;       // 0x04 (0x60)
    u8 created;       // 0x08 (0x64)
    u8 pad_9[3];
    int attr;         // 0x0C (0x68)
    int attr2;        // 0x10 (0x6C)
    u32 flags;        // 0x14 (0x70)  bit0 no EatMgr piece, bit1 no SatMgr piece, bit2 keep attr
    int flag;         // 0x18 (0x74)
};

// Field info payload (type 0xD, SceAtCreateFieldAt; emwindow reads it through SceAtCheckFieldInfo).
struct SceAtField {
    int value;        // 0x00 (0x5C)  0 = the model inside gets litArea.x0 bit0
    cModel* pModel;   // 0x04 (0x60)  creator
};

// Damage area payload (type 0xA).
struct SceAtDamage {
    int time;         // 0x00 (0x5C)  frames (0 = 1); its low byte doubles as the setDamage 5th argument
    u8 kind;          // 0x04 (0x60)
    u8 flags;         // 0x05 (0x61)  bit0, bit1 use `power`
    u8 pad_6[2];
    int arg;          // 0x08 (0x64)
    f32 power;        // 0x0C (0x68)
};

// Hide area payload (type 0x12).
struct SceAtHide {
    u8 mode;          // 0x00 (0x5C)
    u8 pad_1[2];
    u8 step;          // 0x03 (0x5F)
    u8 pad_4[0x24 - 0x4];
    Vec pos;          // 0x24 (0x80)
    void (*func)(int);  // 0x30 (0x8C)  SceAtDataSet_hide
    u8 cut;           // 0x34 (0x90)
};

// Flag area payload (type 4).
struct SceAtFlg {
    u8 kind;          // 0x00 (0x5C)  0 event flag, 1 room save flag, 2 pG->flags_51BC
    u8 off;           // 0x01 (0x5D)  1 = clear
    u16 no;           // 0x02 (0x5E)
};

// Shadow display area payload (type 9).
struct SceAtShdDisp {
    u16 no;           // 0x00 (0x5C)
    u8 on;            // 0x02 (0x5E)
    u8 done;          // 0x03 (0x5F)
};

// Special key area payload (type 0xF, SceAtDataSet_exec fills it).
struct SceAtSkey {
    void* obj;        // 0x00 (0x5C)
    TaskFunc func;    // 0x04 (0x60)
    u8 prio;          // 0x08 (0x64)
    u8 flag;          // 0x09 (0x65)
};

// Message request handed to SceAtSetMes (sce_com SceUpCut), 0xC bytes (type 5 payload).
struct SceAtMesData {
    s16 type;         // 0x00
    s16 no;           // 0x02  < 0: no message
    u8 camCut;        // 0x04  camera cut + 1 (CamCtrl.CutCall)
    u8 seBlk;         // 0x05  SE block select (0: SndCall block 6, else block 0)
    u16 se;           // 0x06  SE + 1
    u8 flag;          // 0x08  bit2: keep the camera cut after the message
    u8 pad_9[3];
};

// Area type (PS2 SCEAT_ID): SceAtWork::type, the row of sceAtFunc_tbl. SCEAT_ID_ADA_WIRE is PS2-only.
enum SCEAT_ID {
    SCEAT_ID_NORMAL = 0,
    SCEAT_ID_DOOR = 1,
    SCEAT_ID_EXEC = 2,
    SCEAT_ID_ITEM = 3,
    SCEAT_ID_FLG = 4,
    SCEAT_ID_MES = 5,
    SCEAT_ID_PLANTER = 6,
    SCEAT_ID_JUMP = 7,
    SCEAT_ID_SAVE = 8,
    SCEAT_ID_SHD_DISP = 9,
    SCEAT_ID_DAMAGE = 10,
    SCEAT_ID_SCR_AT = 11,
    SCEAT_ID_CAM_CTRL = 12,
    SCEAT_ID_FIELD_INFO = 13,
    SCEAT_ID_STOOP = 14,
    SCEAT_ID_SKEY = 15,
    SCEAT_ID_LADDER = 16,
    SCEAT_ID_USE = 17,
    SCEAT_ID_HIDE = 18,
    SCEAT_ID_POS_JUMP = 19,
    SCEAT_ID_ITEM_PARENT = 20,
    SCEAT_ID_ADA_WIRE = 21,
    SCEAT_ID_MAX = 22
};

// Countries an area is disabled in (SceAtWork::country, applied at room start).
enum SCEAT_COUNTRY {
    SCEAT_COUNTRY_USA = 0,
    SCEAT_COUNTRY_JPN = 1
};

// One area work (0x9C bytes; the AEV/ITA records have the same layout).
struct SceAtWork {
    u32 next;         // 0x00  OTag link
    AreaData area;    // 0x04 .. 0x34
    u8 flag;          // 0x34  bit0 enabled, bit2 allocated (SceAtCreate*), bit3 parent rotation ignored
    u8 type;          // 0x35  SCEAT_ID area type (index into sceAtFunc_tbl)
    u8 no;            // 0x36  area number (SceAtPtr key; ITA records + 0x80)
    u8 checkFlag;     // 0x37  bit0 test the front point instead of the position, bit1 angle check
    u8 trigger;       // 0x38  hit state bits that fire the area (1/2/4), bit3 (8) action button, bit7 disable after use
    u8 checkType;     // 0x39  who may trigger it: 1 player, 2 enemy, 8 partner (SceAtCheck type mask)
    u8 prio;          // 0x3A  SceExec priority (0 = call func directly)
    u8 prioBak;       // 0x3B  trigger saved by SceAtDataSet_exec
    int arg;          // 0x3C
    TaskFunc func;    // 0x40
    u8 otNo;          // 0x44  ordering table index (0..15) passed to SceExec / ActBtn.set
    u8 execFlag;      // 0x45  SceExec flag
    u8 linkType;      // 0x46  1 enemy list entry, 2 etc model
    u8 linkNo;        // 0x47
    s8 angle;         // 0x48  * 2 degrees
    s8 angleRange;    // 0x49  * 2 degrees
    u8 actBtnKind;    // 0x4A  action button kind (ActBtn.set)
    u8 pad_4B;
    cModel* pParent;  // 0x4C
    s16 parentParts;  // 0x50  -1 = the model itself
    cFlag<u8, SCEAT_COUNTRY> country;  // 0x52
    u8 actBtnColor;   // 0x53  action button colour (1 = alternate)
    u8 pad_54[8];
    union {
        cModel* hitModel[16];  // 0x5C  type 0: models inside this frame
        SceAtItem item;
        struct {
            Vec dstPos;       // 0x5C  door: destination position
            f32 dstAngle;     // 0x68
            u8 dstStage;      // 0x6C
            u8 dstRoom;       // 0x6D
            u8 lockType;      // 0x6E
            u8 lockFlag;      // 0x6F
            void (*doorFunc)();  // 0x70
            u8 dstPart;       // 0x74  pG->Part in the destination room
            s8 doorSe;        // 0x75
            u8 doorNo;        // 0x76
            u8 doorFadeEff;   // 0x77  -> SceSys.m_door_fade_eff (2 = execute now, SceChapterEnd)
            void* doorArg;    // 0x78
        };
        SceAtCamCtrl cam;
        SceAtLadder ladder;
        SceAtScrAt scr;
        SceAtDamage dmg;
        SceAtHide hide;
        SceAtFlg flg;
        SceAtShdDisp shd;
        SceAtMesData mes;
        SceAtSkey skey;
        SceAtField field;
        int value;            // 0x5C  save argument
        u16 useItem[2];       // 0x5C  type 0x11: [1] = item id
        Vec jumpPos;          // 0x5C  type 0x13
        u8 data[0x40];
    };
};

extern "C" {
void SceAtInit(void* pHeader, void* pHeader_i);
SceAtWork* sceAtSetOtStart();
SceAtWork* sceAtGetOtAddr(SceAtWork* p);
void SceAtClearHitFlg();
void SceAtSetHitFlg(u32 at_no);
void SceAtClearExecFlg();
void SceAtSetExecFlg(u32 at_no);
void SceAtWorkLoopInit();
void SceAtCheck();
int sceAtCheck_main(cEm* em, int target_type);
void sceAtGetArea(AreaData* ret_area, SceAtWork* w);
int sceAtHitCheck(SceAtWork* w, cModel* pModel, Vec* pos_f, Vec* pos);
int CheckAshleyActive();
int CheckDoorJumpWithAshley();
int itemZoom(SceAtWork* w);
void releaseModel(SceAtWork* w, int disp_flg);
void SceAtSetMes(SceAtMesData* pMes);
void sceAtFunc_shd_disp_reverse(SceAtWork* w);
void sceAtLadder(SceAtWork* w);
void sceAtGetLadderPos(SceAtLadder* ladder, Vec* pos, f32* ladder_ang);
int sceAtCheckLadderUp(SceAtLadder* ladder, cModel* pEm);
void SceAtDataSet_hide(int no, void (*func)(int));
int SceAtCheckHideActive();
void SceAtCheckHideProc();
void SceAtStopSemiautoCheck();
void SceAtRoomSet();
void sceAtSetScrAt(SceAtWork* w);
void sceAtDeleteScrAt(SceAtWork* w);
void SceAtCheckMoveScrAt();
SceAtWork* SceAtPtr(int at_no);
int sceAtPullAtNo(u8* out);
void SceAtSetDoorFunc(int no, TaskFunc func, void* arg);
// Area `no`: run `func(obj)` (prio, otPrio) when the player enters it.
enum SCE_LEVEL {
    SCE_NO_TASK = 0,
    SCE_LEVEL_EV = 7,
    SCE_LEVEL00 = 8,
    SCE_LEVEL01 = 9,
    SCE_LEVEL02 = 10,
    SCE_LEVEL03 = 11,
    SCE_LEVEL04 = 12,
    SCE_LEVEL05 = 13,
    SCE_LEVEL06 = 14,
    SCE_LEVEL07 = 15,
    SCE_LEVEL08 = 16,
    SCE_LEVEL09 = 17,
    SCE_LEVEL10 = 18,
    SCE_LEVEL11 = 19,
    SCE_LEVEL_ANY = 20
};

void SceAtDataSet_exec(int no, int prio, int a, TaskFunc func, void* obj, int b);
void SceAtDataReset(int at_no);
void SceAtSetEnable(int at_no, int sw);
int SceAtHitCheck(u32 at_no);
void SceAtExecute(int at_no);
int SceAtCheckHitModel(int at_no, cModel* pModel);
void SceAtSetActColor(int at_no, int col);
void SceAtGetCenterPos(Vec* ret_pos, int at_no);
int SceAtSetParent(SceAtWork* w, cModel* parent, int flag);
int InScreenCheck(Vec* pos);
void SceAtExecRoomJump(u16 room, Vec* pos, Vec* rot, int a);
SceAtField* SceAtCheckFieldInfo(Vec* pos);
int SceAtCheckLadder(cModel* pEm, Vec* pos, f32* ladder_ang, u8* ladder_height);
int SceAtSearchLadder(cModel* pEm, Vec* pos, f32* ladder_ang, u8* ladder_height);
void SceAtDataEyeTriggreCopy(AreaData* area, SceAtWork* w);
void SceAtItemFlgOn(u16 item_flg, u16 saveFlagNo);
int SceAtItemFlgCk(u16 item_flg, u16 saveFlagNo);
int SceAtItemFindFlgCk(int at_no);
void sceAtItemFlgOn(SceAtItem* it);
int sceAtItemFlgCk(SceAtItem* it);
void sceAtItemFindFlgOn(SceAtItem* it);
int sceAtItemFindFlgCk(SceAtItem* it);
int SceAtDestroy(int at_no);
// Area of the four corners `pos` around `m`: (x37, x38, x39, height, x44, angle, angle range, x4A, prio, func, arg, flag).
int SceAtCreateExecAt(cModel* m, Vec* pos, int a, int b, int c, f32 h, int d, f32 ang, f32 range, int e, int prio, TaskFunc func, int arg, u8 flag);
int SceAtCreateFieldAt(cModel* m, Vec* pos, int a, int b, int c, f32 h, int d, f32 ang, int e, f32 range, int val, SceAtField** out);
int SceAtCreateItemAt(Vec* pos, ITEM_ID id, int num, int effType, int saveNo, cModel* parent, int parts);
void SceAtReserveItemAt(cEm* pEm, Vec* pos, ITEM_ID item_id, int item_num, int item_eff, int save_no);
void SceAtCancelItemAt(cEm* pEm);
int sceAtCheckItemEffectCol(ITEM_ID item_id);
int sceAtCheckSaveItem(u16 id);
void SceAtLinkEtcDead(int at_no, int etc_no, int on_off);
void sceAtLink_check();
int SceAtSetEmItem(cEm* em, int no);
void SceAtSetSaveItem();
int sceAtPullItemSaveWork();
void SceAtInitSaveItem();
int SceAtCheckSaveItemId(int id);
void sceAtCheckItemModelParent(SceAtWork* w);
void sceAtSetItemModelParent(SceAtWork* w);
int SceAtSetItemModel(int no, cModel* m);
int SceAtSetShootDownItem(SceAtWork* w, void* bin, void* tpl);
cModel* SceAtItemModelPtr(int at_no);
int SceAtItemHitCheck(SceAtWork* w, Vec* pos);
int SceAtCheckSystemItemSet(u32 id, int* outId, int* outNum, Vec* pos, Vec* rot);
void sceAtSetItem(SceAtWork* w);
void SceAtItemAutoArea(AreaData* area, Vec* pos, f32 radius);
void sceAtItemEffDelete(SceAtItem* it);
void sceAtItemEffSet(SceAtWork* w, cModel* pModel);
void sceAtItemDisappearEffSet(SceAtWork* w, cModel* pModel);
}

// C++ overloads of the C entry points above.
// Area `no` follows parts `parts` of `obj`; 0 when the area does not exist.
int SceAtSetParent(int no, cObj* obj, int parts);
int SceAtItemFlgCk(int at_no);
int SceAtSetEmItem(cEm* em, SceAtWork* w);
int SceAtSetItemModel(SceAtWork* w, cModel* m);

#endif
