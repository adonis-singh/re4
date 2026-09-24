#ifndef ESPGEN_H
#define ESPGEN_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "light.h"
#include "esp.h"
#include "gx.h"
#include "tpl.h"

// Optional 0x1C byte parameter block handed down the sequence calls (copied into the generator work)
// (PS2 ESPSEQ_CONTROL, packed on GC).
struct EspSeqOpt {
    u8 OverWrite_flg;  // 0x00 bit0: speed, bit1: size, bit2: colour replace the record's (esp_sub EspSeqSet)
    u8 Mul_flg;        // 0x01 same bits: multiply
    u8 Add_flg;        // 0x02 same bits: add
    u8 pad;            // 0x03
    Vec Speed;         // 0x04
    f32 Size_base_x;   // 0x10
    f32 Size_base_y;   // 0x14
    u8 Col_start_r;    // 0x18
    u8 Col_start_g;    // 0x19
    u8 Col_start_b;    // 0x1A
    u8 Col_start_a;    // 0x1B
};

// Effect system work (game/eff_sys.cpp cEspSystem, g_pEspSys). Partial layout.
// Room effect (sst) table entry: an effect list and the offsets of its EspSeqData blocks (game/est.cpp SstSet).
struct SstList {
    u32 num;           // 0x00
    struct {
        union {
            u16 no;    // 0x00 room number range key
            struct {
                u8 x0;
                u8 id; // 0x01 display flag bit (GetSstDispFlag)
            } b;
        };
        u16 type;      // 0x02
        u32 Flg;        // 0x04  (PS2 ESP_ID_WK.Flg)
    } ent[1];          // 0x04, 8 bytes each
};
struct SstData {
    u32 Num;            // 0x00  (PS2 ESP_COMMON_HEADER.Num)
    u32 ofs[1];        // 0x04 byte offsets of the EspSeqData blocks from this header
};
struct SstTbl {
    SstData* data;     // 0x00
    SstList* list;     // 0x04
    int owner;         // 0x08 0xD2 = unused entry
};

// Area list for the room effect display flags (game/est.cpp AreaSstSet): 0x10 header, 0x98 byte entries.
struct SstAreaEnt {
    u8 no;//  (PS2 ESP_AREA.no)
    u8 be_flag;//  (PS2 ESP_AREA.be_flag)
    u8 area_no;            // 0x02 display flag bit set while the player stands in the area  display flag bit set while the player stands in the area (PS2 ESP_AREA.area_no)
    u8 pad02;//  (PS2 ESP_AREA.pad02)
    u8 area[0x30];     // 0x04 AreaHitCheck data (AreaData)
    u32 flag;         // 0x34 bit0: the area counts as "in room" (esp_app EffAreaCheckInRoom)  (PS2 ESP_AREA.flag)
    u8 pad_38[0x98 - 0x38];
};
struct SstArea {   // (PS2 ESP_AREA_HEADER)
    u32 num;           // 0x00
    u32 ver_no;        // 0x04 (PS2 ver_no)
    u8 pad_8[8];
    SstAreaEnt ent[1]; // 0x10
};

// One registered effect texture set (eff_sys espTexRegist), 0x54 bytes; owner 0xD2 = free.
struct EspTexWk {
    GXTexObj* pTex_obj_start;   // 0x00 first of nTex objects pulled from cEspSystem::texObj
    u16 nTexObj;            // 0x04
    u8 pad_6[2];
    GXTlutObj tlut;      // 0x08
    TEXHeader* texHdr;   // 0x14 header of texture 0
    Mtx _Mtx;             // 0x18
    TEXPalette* Tpl_addr;    // 0x48
    EspAnmData* Anm_addr;    // 0x4C
    u32 Owner;           // 0x50
};

// Effect model (efm) registration (eff_sys efmRegist), 0x14 bytes.
struct EspEfmMotTbl {
    u32 num;           // 0x00
    u32 ofs[1];        // 0x04 byte offsets of the motions from this header
};
struct EspEfmWk {
    void* model;       // 0x00 model bin
    void* tpl;         // 0x04
    EspEfmMotTbl* mot; // 0x08 motion table (NULL when none)
    void* pShapeHeader;         // 0x0C  shape header of efmRegist (PS2 pShapeHeader)
    u32 owner;         // 0x10 0xD2 = free
};

// Effect system work (game/eff_sys.cpp, g_pEspSys, sizeof 0xC5E8).
struct cEspSystem {
    EspTexWk Esp_tex_tbl[0x100];       // 0x0000 by texture id
    EspEfmWk efmWk[0x100];       // 0x5400 by effect model id
    SstTbl estTbl[EFF_MAX];         // 0x6800 effect set tables by owner id
    SstTbl sstTbl[EFF_MAX];         // 0x71E4 room effect tables by owner id
    SstTbl pathTbl[EFF_MAX];        // 0x7BC8 path tables by owner id
    u8 ownerCnt[EFF_MAX];           // 0x85AC EspDataLoad count per owner
    u8 pad_867F;
    SstArea* Area_addr;           // 0x8680
    GXTexObj texObj[0x1F4];      // 0x8684 texture object pool
    u8 TexObj_flg[0x3F];         // 0xC504 one bit per pool entry
    u8 pad_C543[5];
    u32 ActiveEspNum;         // 0xC548 number of esp slots in use  (PS2 ActiveEspNum)
    u8* EspArray;       // 0xC54C esp pool (0x150 bytes per cEsp)
    u8* pEspBufSave;   // 0xC550 pool saved by EspArrayPush (esp.cpp)
    u32 nEsp;         // 0xC554 number of esp slots
    u32 nEspBack;       // 0xC558 slot count saved by EspArrayPush
    cEsp* pDmyEsp;        // 0xC55C dummy esp returned when the pool is full
    u8 CoreKindTop;       // 0xC560 next effect kind handed out by EspPullCoreKind (0x45..)
    u8 ToolState;      // 0xC561
    u8 pad_C562[2];
    u32 RstAreaState;     // 0xC564 bit per area (GetAreaState)
    EspLightList EspLightEnv;  // 0xC568 lights the effects draw with (esp.cpp EspTrans -> cLightMgr::setEsp)
    u8 pad_C58C[4];
    int finalColSet;   // 0xC590 1 while finalCol.r == 0xFF (EffSetFinalCol)
    GXColor Final_col;  // 0xC594
    f32 CameraPan;        // 0xC598 camera yaw in degrees (EspGetCameraPan)
    f32 CameraPan2;       // 0xC59C camera pitch in degrees (EspGetCameraPan2)
    u32 SstSetFlag;   // 0xC5A0 room effect display flags (bit per id)
    u32 Add_area_bit;  // 0xC5A4
    void (*pFuncOn[8])();   // 0xC5A8 tool state callbacks (state bits 0/1 set)
    void (*pFuncOff[8])();  // 0xC5C8 (state bits 0/1 clear)

    int GetTexObjFlag(u32 no);
    void SetTexObjFlag(u32 no, int flg);
    int GetActiveEspNum() { return ActiveEspNum; }
    int GetMaxEspNum() { return nEsp; }
};
extern cEspSystem* g_pEspSys;

// One effect generator instance (game/espgen.cpp array, stride 0xC8). Bytes 0x14.. are the
// per-generator work (Espgen00Work, Espgen10Work, Espgen44Work, ...).
struct EspgenWork {
    EspInfo info;      // 0x00 owner info (copied from the parent by SetEspCore)
    u8 flag;           // 0x0C bit0: in use, bit1: delete requested
    u8 id;             // 0x0D generator id (index into the Espgen*Tbl tables)
    u8 Type;             // 0x0E  (PS2 cEspgen::Type; EspGenWork Espgen_type)
    u8 Flg;             // 0x0F  (PS2 cEspgen::Flg)
    u8 step;           // 0x10 move step (Espgen*MoveTbl index)
    u8 pad_11[3];
    u8 work[0xC8 - 0x14];  // 0x14
};

// Effect controller 10 work (game/espgen10.cpp): plays an effect sequence (EspSeqData) record by
// record. est.cpp EstSet fills it directly.
struct Espgen10Work {
    EspSeqData* head;  // 0x14
    cModel* pMod;     // 0x18
    u32 Guid_pMod;        // 0x1C model serial the controller was set up with
    u16 Time_cnt;           // 0x20 frame counter
    u8 Seq_ptr;             // 0x22 next record
    u8 Flg;          // 0x23 bit0: parts matrix fixed, bit1: pass the rotation on
    u16 Null_parts_no;         // 0x24 parts number (0xFE: free position, 0xFF: none)
    u8 pad_26[2];
    u32 Rand_seed;          // 0x28
    Mtx Mat;           // 0x2C
    Vec Offset;           // 0x5C
    Vec Ang;           // 0x68
    EspSeqOpt opt;     // 0x74 copy of the option block p8 points at
    EspSeqOpt* p8;     // 0x90
};

// Water surface work shared by generators 42 (room water, game/Espgen42.cpp) and 45 (weather water,
// game/espgen45.cpp): a (nx+1) x (ny+1) height field with two ping-pong height buffers, drawn through
// a prebuilt display list with an indirect bump texture.
struct Espgen42Work {
    Vec pos0;          // 0x14 surface centre (espgen45 SetWaterWork45; .y -> Base_y) (PS2 Pos)
    Mtx mat;           // 0x20 grid -> world
    Mtx inv;           // 0x50 world -> grid
    u16 nx;            // 0x80 grid cells along x
    u16 ny;            // 0x82 grid cells along z
    u8 pad_84[4];
    f32 size;          // 0x88 cell size
    f32* hA;           // 0x8C height buffers (pG->flags_51E4 bit 0 selects the current one)
    f32* hB;           // 0x90
    Vec* nrm;          // 0x94
    Vec* pos;          // 0x98
    u8* dl;            // 0x9C display list
    u32 dlSize;        // 0xA0
    u8* bump;          // 0xA4 I8 bump texture (nx x ny)
    GXColor col;       // 0xA8 tev colour
    GXColor amb;       // 0xAC ambient colour (amb.a: light alpha)
    u8 mode;           // 0xB0 wave model (1: second variant)
    s8 stages;         // 0xB1 number of extra tev stages
    u8 texId;          // 0xB2
    u8 rotY;           // 0xB3 (espgen45) EspGenWork xFE
    u16 indS;          // 0xB4 indirect matrix parameters
    u16 indT;          // 0xB6
    f32 Prm_a;         // 0xB8  wave coefficient (PS2 ESPGEN45_WK Prm_a; was `damp`)
    f32 Prm_dmp;       // 0xBC  decay per frame (PS2 Prm_dmp; was `spread`)
    f32 Base_y;           // 0xC0 (espgen45)  espgen45: pos0.y at set-up (PS2 Base_y)
    u8 flag;           // 0xC4 (espgen45) bit0: bounded grid (EspGenWork flags bit0), bit1: EspGenWork flags 0x4000
    u8 Mask_Tex;            // 0xC5 (espgen45) EspGenWork xC5  espgen45: EspGenWork MaskTex_id (PS2 Mask_Tex)
};

typedef void (*EspgenMoveFunc)(EspgenWork* w);
typedef void (*EspgenTransFunc)(EspgenWork* w);
typedef int (*EspgenSetFreeWorkFunc)(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts,
                                     Mtx* mtx, Vec* pos, Vec* rot, EspSeqOpt* p8, int flag);
// the application generators (Espgen4x) take no flag argument
typedef int (*EspgenSetFreeWorkAppFunc)(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model,
                                        u16 parts, Mtx* mtx, Vec* pos, Vec* rot, EspSeqOpt* p8);
typedef void (*EspgenDestructFunc)(EspgenWork* w);

extern "C" {
// game/espgen.cpp
u32 GetEspgenIdMax();
int PullEspgen(EspgenWork** ppEspgen);
int PullEspgenFront(EspgenWork** ppEspgen);
void PushEspgen(EspgenWork* pEspgen);
int EspgenSetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                      Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag);
int EspgenSeqSet(EspSeqData* head, int no, EspInfo* info, cModel* model, u16 parts, Mtx* mtx, Vec* pos, Vec* rot,
                 EspSeqOpt* pSct, int flag);
void EspgenArrayClear();
void EspgenDelete(int a, int b, void* c);
void EspgenDeleteEvent();
int EspgenGetCallNo();
void EspgenIncCallNo();

// game/esp_sub.cpp
int EspSeqSet(EspGenWork* rec, EspInfo* info, u32* seed, cModel* model, Mtx* mtx, int flg, f32 f, cEsp** out,
              EspSeqOpt* pSct, Vec* pos);

// game/est.cpp: event model number -> model (Event::EspSetModelPtr fills it, effect records with Core_flg 0x1000 read it)
class cEspEventModelList {
private:
    cModel* pModTbl[0x80];
public:
    void Clear()
    {
        u32 i;

        for (i = 0; i < 0x80; i++) {
            pModTbl[i] = 0;
        }
    }
    void SetModelPtr(u32 no, cModel* pMod)
    {
        if (no < 0x80) {
            pModTbl[no] = pMod;
        }
    }
    cModel* GetModelPtr(u32 no)
    {
        cModel* p;

        if (no > 0x7F) {
            p = 0;
        } else {
            p = pModTbl[no];
        }
        return p;
    }
};
extern cEspEventModelList EspEvModList;

// game/espgen10.cpp
int EspgenDataSet(EspSeqData* head, int no, EspInfo* info, u32* seed, cModel* model, u16 parts, Mtx* mtx, Vec* pos,
                  Vec* rot, EspSeqOpt* pSct, int flag);
void SetEspCore(EspgenWork* pCore, int Core_flg, u32 Call_no, u8 Core_kind, void* Core_pEm, int owner);
int PullEspEspgen(EspgenWork** ppEspgen, int Core_flg, int Core_kind, u32 Call_no, void* Core_pEm, int owner, int type);
void Espgen10_Move(EspgenWork* pEspgen);

// game/espgen00.cpp
void Espgen00_Move(EspgenWork* pEspgen);
int Espgen00_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag);

// game/espgen01.cpp
void Espgen01_Move(EspgenWork* pEspgen);
void Espgen01_Trans(EspgenWork* pEspgen);
int Espgen01_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag);

// game/espgen02.cpp
void Espgen02_Move(EspgenWork* pEspgen);
int Espgen02_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag);

// game/espgen44.cpp
void Espgen44_Move(EspgenWork* pGen);
void Espgen44_Trans(EspgenWork* pGen);
void Espgen44_Destruct(EspgenWork* pGen);
int Espgen44_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct);

// game/Espgen42.cpp
void Espgen42_Move(EspgenWork* pGen);
void Espgen42_Trans(EspgenWork* pGen);
void Espgen42_Destruct(EspgenWork* pGen);
int Espgen42_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct);

// game/Espgen43.cpp
void Espgen43_Move(EspgenWork* pGen);
void Espgen43_Trans(EspgenWork* pGen);
void Espgen43_Destruct(EspgenWork* pGen);
int Espgen43_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct);

// game/espgen45.cpp
void Espgen45_static_init();
void Estgen45SetTargetCamera(int bTc);
void Estgen45SetTargetHeight(int bTc);
void Estgen45SetSizeOverWrite(int bTc);
void Estgen45SetColorOverWrite(int bTc);
void Estgen45SetColorMul(int bTc);
void Estgen45SetParamOverWrite(int bTc);
void Estgen45SetTargetPos(f32 x, f32 z);
void Estgen45SetHeight(f32 y);
void Estgen45SetSize(f32 size);
void Estgen45SetColor(u8 r, u8 g, u8 b, u8 a, f32 sr, f32 sg, f32 sb, f32 sa);
struct Esp4cWork;
void Estgen45SetParam(Esp4cWork* pFree);
void Espgen45_Move(EspgenWork* pGen);
void Espgen45_Trans(EspgenWork* pGen);
void Espgen45_Destruct(EspgenWork* pGen);
int Espgen45_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct);
}

// Debug tools (tools.cpp ToolArrayPush/ToolWorkPop): swap the espgen pool like EspArrayPush.
extern "C" int EspgenArrayPush(int num);
extern "C" int EspgenArrayPop();

// game/espgen.cpp: the generator pool's init / room setup / frame update / draw and its debug view (C linkage).
extern "C" {
int EspgenInit();
int EspgenRoomInit();
int EspgenArrayAlloc(int workNum);
int EspgenMove();
int EspgenTrans();
int EspgenDispInfo();
}
extern EspgenWork* g_pWater45;   // espgen45.cpp: the running water surface generator (Espgen42 rain hits it)

#endif
