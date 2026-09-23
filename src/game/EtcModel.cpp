// game/EtcModel.cpp: room "etc" models (doors, boxes, racks, torches, bars, ladders ...).
// g_EtcTbl holds the 64 models a room's etc list created (EtcModelSet / Et*_init load the model,
// texture and effect files from the room's etc archive), the getRoomEtc* functions look them up by
// slot and id, GetEtcFlgPtr addresses the per-room etc flags in the save record and EtcSetAddAmb
// applies the room's additive ambient colour.

#include "atari.h"
#include "light.h"
#include "em.h"
#include "obj.h"
#include "obj13.h"
#include "embox.h"
#include "emdoor.h"
#include "emrack.h"
#include "emtorch.h"
#include "emBarred.h"
#include "embarrel.h"
#include "emBar.h"
#include "emswitch.h"
#include "emitem.h"
#include "etc_model.h"
#include "global.h"
#include "room_data.h"
#include "db_log.h"
#include "eprintf.h"
#include "math_sub.h"
#include <string.h>
#include "esp.h"

// One file of the room etc archive: `size` bytes to the next header, name at 0x20, data at 0x40.
struct EtcArcFile {
    u32 size;         // 0x00
    u8 pad_4[0x20 - 0x4];
    char name[0x20];  // 0x20
    u8 data[1];       // 0x40
};

// Room etc archive: file count, then the files from 0x20.
struct EtcArc {
    u32 num;          // 0x00
    u8 pad_4[0x20 - 0x4];
    EtcArcFile file[1];  // 0x20
};

// Room etc list (EtcModelListSet): count, then the 0x28-byte records from 0x10.
struct EtcList {
    u16 num;          // 0x00
    u8 pad_2[0x10 - 0x2];
    EtcSetData data[1];  // 0x10
};

// Room save record as GetEtcFlgPtr sees it: the 64 etc flag words at 0x28.
struct EtcRoomSave {
    u8 pad_0[0x28];
    u16 etcFlag[0x40];   // 0x28
};

// Additive ambient colour per etc kind (EtcSetAddAmb argument), one row per ambient type.
struct EtcAmbRgb {
    u8 r;
    u8 g;
    u8 b;
};

extern "C" {
// game/obj13.cpp (obj13.h is not included: this unit keeps its own cObjLadder view)
cObj* SetLadder(void* bin, void* tpl, Vec* pos, Vec* rot, int no);

// game/et00.cpp: the window models
int Et00_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et07_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et1d_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et25_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et29_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et2c_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et35_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et36_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et44_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et48_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et4a_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et50_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et51_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et52_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et53_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et54_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et55_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et56_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et57_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et58_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5a_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5b_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5c_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5d_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5e_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et5f_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et60_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et64_init(void* arc, EtcSetData* d, cModel** out, int flag);
int Et65_init(void* arc, EtcSetData* d, cModel** out, int flag);

// this unit (C linkage: every caller uses the plain names)
void EtcModelDebugDisp();
int getRoomEtc(int no, ETCMODEL_ID id, cEm** out, int flag);
int getRoomEtc2(int no, cEm** out, int flag);
int getRoomEtcID(int no);
void EtcModelInit();
void EtcModelRoomInit();
int EtcModelDataLoad(void* addr);
int EtcModelGetLastNo();
int EtcModelListSet(EtcList* list);
int Et01_init(void* arc, EtcSetData* d, cModel** out);
int Et02_init(void* arc, EtcSetData* d, cModel** out);
int Et03_init(void* arc, EtcSetData* d, cModel** out);
static int Et04_init(void* arc, EtcSetData* d, cModel** out);
int Et05_init(void* arc, EtcSetData* d, cModel** out);
int Et15_init(void* arc, EtcSetData* d, cModel** out);
int Et06_init(void* arc, EtcSetData* d, cModel** out);
int Et08_init(void* arc, EtcSetData* d, cModel** out);
int Et09_init(void* arc, EtcSetData* d, cModel** out);
int Et0a_init(void* arc, EtcSetData* d, cModel** out);
int Et0b_init(void* arc, EtcSetData* d, cModel** out);
int Et0c_init(void* arc, EtcSetData* d, cModel** out);
int Et0d_init(void* arc, EtcSetData* d, cModel** out);
int Et10_init(void* arc, EtcSetData* d, cModel** out);
int Et0e_init(void* arc, EtcSetData* d, cModel** out);
int Et0f_init(void* arc, EtcSetData* d, cModel** out);
int Et11_init(void* arc, EtcSetData* d, cModel** out);
int Et12_init(void* arc, EtcSetData* d, cModel** out);
int Et13_init(void* arc, EtcSetData* d, cModel** out);
int Et14_init(void* arc, EtcSetData* d, cModel** out);
static int Et16_init(void* arc, EtcSetData* d, cModel** out);
int Et17_init(void* arc, EtcSetData* d, cModel** out);
int Et18_init(void* arc, EtcSetData* d, cModel** out);
int Et19_init(void* arc, EtcSetData* d, cModel** out);
int Et1a_init(void* arc, EtcSetData* d, cModel** out);
int Et1b_init(void* arc, EtcSetData* d, cModel** out);
int Et1c_init(void* arc, EtcSetData* d, cModel** out);
int Et1e_init(void* arc, EtcSetData* d, cModel** out);
int Et1f_init(void* arc, EtcSetData* d, cModel** out);
int Et20_init(void* arc, EtcSetData* d, cModel** out);
int Et21_init(void* arc, EtcSetData* d, cModel** out);
int Et22_init(void* arc, EtcSetData* d, cModel** out);
int Et23_init(void* arc, EtcSetData* d, cModel** out);
int Et24_init(void* arc, EtcSetData* d, cModel** out);
int Et26_init(void* arc, EtcSetData* d, cModel** out);
int Et27_init(void* arc, EtcSetData* d, cModel** out);
int Et28_init(void* arc, EtcSetData* d, cModel** out);
int Et2a_init(void* arc, EtcSetData* d, cModel** out);
int Et2b_init(void* arc, EtcSetData* d, cModel** out);
int Et2d_init(void* arc, EtcSetData* d, cModel** out);
int Et30_init(void* arc, EtcSetData* d, cModel** out);
static int Et2e_init(void* arc, EtcSetData* d, cModel** out);
int Et2f_init(void* arc, EtcSetData* d, cModel** out);
int Et31_init(void* arc, EtcSetData* d, cModel** out);
int Et32_init(void* arc, EtcSetData* d, cModel** out);
int Et33_init(void* arc, EtcSetData* d, cModel** out);
int Et34_init(void* arc, EtcSetData* d, cModel** out);
int Et37_init(void* arc, EtcSetData* d, cModel** out);
int Et38_init(void* arc, EtcSetData* d, cModel** out);
int Et39_init(void* arc, EtcSetData* d, cModel** out);
int Et3a_init(void* arc, EtcSetData* d, cModel** out);
int Et3b_init(void* arc, EtcSetData* d, cModel** out);
int Et3c_init(void* arc, EtcSetData* d, cModel** out);
int Et3d_init(void* arc, EtcSetData* d, cModel** out);
int Et3e_init(void* arc, EtcSetData* d, cModel** out);
int Et3f_init(void* arc, EtcSetData* d, cModel** out);
int Et40_init(void* arc, EtcSetData* d, cModel** out);
int Et41_init(void* arc, EtcSetData* d, cModel** out);
int Et42_init(void* arc, EtcSetData* d, cModel** out);
int Et43_init(void* arc, EtcSetData* d, cModel** out);
int Et45_init(void* arc, EtcSetData* d, cModel** out);
int Et46_init(void* arc, EtcSetData* d, cModel** out);
int Et47_init(void* arc, EtcSetData* d, cModel** out);
int Et49_init(void* arc, EtcSetData* d, cModel** out);
int Et4b_init(void* arc, EtcSetData* d, cModel** out);
int Et4c_init(void* arc, EtcSetData* d, cModel** out);
static int Et4d_init(void* arc, EtcSetData* d, cModel** out);
int Et4e_init(void* arc, EtcSetData* d, cModel** out);
int Et4f_init(void* arc, EtcSetData* d, cModel** out);
int Et59_init(void* arc, EtcSetData* d, cModel** out);
int Et61_init(void* arc, EtcSetData* d, cModel** out);
int Et62_init(void* arc, EtcSetData* d, cModel** out);
int Et63_init(void* arc, EtcSetData* d, cModel** out);
int Et66_init(void* arc, EtcSetData* d, cModel** out);
int Et67_init(void* arc, EtcSetData* d, cModel** out);
int EtcModelSet(EtcSetData* pDat);
int EtcGetDasAddr(int id, void** out);
int getRoomEtcBreak(int no, cEm** out, int flag);
int setRoomEtcDisp(int no, int on, int flag);
static int setRoomEtcBreakDisp(int no, int on, int flag);
int getRoomEtcWindow(int no, cEmWindow** out, int flag);
int getRoomEtcBox(int no, cEm** out, int flag);
int getRoomEtcDoor(int no, cEmDoor** out, int flag);
int getRoomEtcRack(int no, cEm** out, int flag);
int getRoomEtcLadder(int no, cObjLadder** out, int flag);
int getRoomEtcTorch(int no, cEm** out, int flag);
int getRoomEtcSwitch(int no, cEm** out, int flag);
int getRoomEtcBarred(int no, cEm** out, int flag);
int getRoomEtcDram(int no, cEm** out, int flag);
ETC_AMB_TYPE GetEtcAmbType();
int GetEm10EyeEffectEnable();
void EtcSetAddAmb(cModel* m, int no);
}


// One slot of the room etc table.
class cEtcTbl {
public:
    int stat;            // 0x00  1: a model is registered
    EtcSetData* pData;   // 0x04  its etc list record
    cModel* m_pPtr;      // 0x08  the model

    cEtcTbl() { Init(); }
    void Init() {
        stat = 0;
        pData = 0;
        m_pPtr = 0;
    }
    void RegistData(EtcSetData* d, cModel* model) {
        if (stat == 1) {
            pLog->err(0, 0, "cEtcTbk::RegistData() : No[%2x] is already used.(id=%2x)", d->no, d->id);
        } else {
            stat = 1;
            pData = d;
            m_pPtr = model;
        }
    }
};

static void* g_addr;         // room etc archive (EtcModelDataLoad)
static u32 g_LastNo;         // highest etc slot the room's list used
static cEtcTbl g_EtcTbl[0x40];
u32 g_Etc_das_addr[0x68];


// Debug: prints each live etc model's list number at its screen position.
void EtcModelDebugDisp()
{
    cEtcTbl* t = g_EtcTbl;   // the dead initializer keeps `&g_EtcTbl` live past the loop pointer's init (`mr r31, r9`)
    u32 i;

    for (i = 0; i < 0x40; i++) {
        t = &g_EtcTbl[i];
        if (t->stat == 1) {
            Vec scr;
            Vec pos;

            pos = t->pData->pos;
            if (GetScreenPos(&pos, &scr) == 1) {
                eprintf2(8, 12, (u32) scr.x - 8, (u32) scr.y, 0, 0, "%d", t->pData->no);
            }
        }
    }
}

// The etc model in slot `no` when it has etc id `id`: 1 and *out; 0 (an error when flag == 1)
// for an empty slot or another id.
int getRoomEtc(int no, ETCMODEL_ID id, cEm** pRet, int bDispErr)
{
    cEtcTbl* t = &g_EtcTbl[no];

    *pRet = 0;
    if (t->stat != 1) {
        if (bDispErr == 1) {
            pLog->err(6, 0, "getRoomEtc() : No[%d] not Initialized.", no);
        }
        return 0;
    }
    if (id != t->pData->id) {
        if (bDispErr == 1) {
            pLog->err(6, 0, "getRoomEtc() : no[%d] is ID diff[%02x/%02x].", no, id, t->pData->id);
        }
        return 0;
    }
    *pRet = (cEm*) t->m_pPtr;
    return 1;
}

// The etc model in slot `no` whatever its id; 0 (error when flag) for an empty slot.
int getRoomEtc2(int no, cEm** pRet, int bDispErr)
{
    cEtcTbl* t = &g_EtcTbl[no];

    *pRet = 0;
    if (t->stat != 1) {
        if (bDispErr == 1) {
            pLog->err(6, 0, "getRoomEtc() : No[%d] not Initialized.", no);
        }
        return 0;
    }
    *pRet = (cEm*) t->m_pPtr;
    return 1;
}

// Etc id of slot `no`; 0x68 (none) with an error for an empty slot.
int getRoomEtcID(int no)
{
    cEtcTbl* t = &g_EtcTbl[no];

    if (t->stat != 1) {
        pLog->err(6, 0, "getRoomEtcID() : No[%d] not Initialized.", no);
        return 0x68;
    }
    return t->pData->id;
}

// Data of file `name` in an etc archive (linear search of the file headers); NULL with an error
// when missing.
void* GetEtcAddr(void* arc, const char* name)
{
    EtcArc* a = (EtcArc*) arc;
    EtcArcFile* f = a->file;
    u32 i;

    for (i = 0; i < a->num; i++) {
        if (strcmp(f->name, name) == 0) {
            return f->data;
        }
        f = (EtcArcFile*) ((u8*) f + f->size);
    }
    pLog->err(6, 0, "GetEtcAddr() : [%s] not found.", name);
    return 0;
}

// Boot: clears the 64 slots, the per-id archive table and the list bookkeeping.
void EtcModelInit()
{
    int i;
    u32 j;

    for (i = 0; i < 0x40; i++) {
        g_EtcTbl[i].stat = 0;
        g_EtcTbl[i].pData = 0;
        g_EtcTbl[i].m_pPtr = 0;
    }
    for (j = 0; j < 0x68; j++) {
        g_Etc_das_addr[j] = 0;
    }
    g_LastNo = 0;
    g_addr = 0;
}

// Room start: same reset (the room's etc models are re-created from its list).
void EtcModelRoomInit()
{
    int i;
    u32 j;

    for (i = 0; i < 0x40; i++) {
        g_EtcTbl[i].stat = 0;
        g_EtcTbl[i].pData = 0;
        g_EtcTbl[i].m_pPtr = 0;
    }
    for (j = 0; j < 0x68; j++) {
        g_Etc_das_addr[j] = 0;
    }
    g_LastNo = 0;
    g_addr = 0;
}

// Installs the room's etc archive table (per etc id); disabled by Debug_flg[3] 0x800.
int EtcModelDataLoad(void* addr)
{
    if (DbgFlagChk(pG, DBG_NO_ETC_SET)) {
        return 0;
    }
    g_addr = addr;
    return 1;
}

// Highest slot number the room list created.
int EtcModelGetLastNo()
{
    return g_LastNo;
}

// Creates every record of the room's etc list (EtcModelSet) and remembers the highest slot.
int EtcModelListSet(EtcList* list)
{
    EtcSetData* d;
    u32 i;

    if (DbgFlagChk(pG, DBG_NO_ETC_SET)) {
        return 0;
    }
    d = list->data;
    for (i = 0; i < list->num; i++) {
        if (EtcModelSet(&d[i]) == 1) {
            if (g_LastNo < d[i].no) {
                g_LastNo = d[i].no;
            }
        }
    }
    return 1;
}

// Etc id 01: creates a box enemy (cEmBox) box type 0 from et0100.bin with effect data et01.eff,
// effect owner 0x55. d->type is the etc flag number; 1 on success.
int Et01_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et01.eff"), EFF_ET01, 0);
    bin = GetEtcAddr(arc, "et0100.bin");
    if (EspGetEfmTplAddr(0x04, &tpl) == 0) {
        pLog->err(0, 0, "ET01:EFM[%02x] TPL err", 0x04);
        return 0;
    }
    em = SetBox(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(6, 0, "ET01:Mod err");
        return 0;
    }
    em->setEff(0x55);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 02: creates a box enemy (cEmBox) box type 1 from et0200.bin with effect data et02.eff,
// effect owner 0x56. d->type is the etc flag number; 1 on success.
int Et02_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et02.eff"), EFF_ET02, 0);
    bin = GetEtcAddr(arc, "et0200.bin");
    if (EspGetEfmTplAddr(0x04, &tpl) == 0) {
        pLog->err(0, 0, "ET02:EFM[%02x] TPL err", 0x04);
        return 0;
    }
    em = SetBox(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET02:Mod err");
        return 0;
    }
    em->setEff(0x56);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 03: creates a door enemy (cEmDoor) door type 0 from et0300.bin with effect data et03.eff,
// obm2b.eff, effect owner 0x57; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et03_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et03.eff"), EFF_ET03, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et0300.bin");
    if (EspGetEfmTplAddr(0x0A, &tpl) == 0) {
        pLog->err(0, 0, "ET03:EFM[%02x] TPL err", 0x0A);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET03:Mod err");
        return 0;
    }
    em->setEff(0x57);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 04: creates a rack enemy (cEmRack) rack type 0 from et0400.bin with effect data et04.eff,
// effect owner 0x58. d->type is the etc flag number; 1 on success.
static int Et04_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmRack* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et04.eff"), EFF_ET04, 0);
    bin = GetEtcAddr(arc, "et0400.bin");
    if (EspGetEfmTplAddr(0x00, &tpl) == 0) {
        pLog->err(0, 0, "ET04:EFM[%02x] TPL err", 0x00);
        return 0;
    }
    em = SetRack(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET04:Mod err");
        return 0;
    }
    em->setEff(0x58);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 05: creates a rack enemy (cEmRack) rack type 1 from et0500.bin with effect data et05.eff,
// effect owner 0x59. d->type is the etc flag number; 1 on success.
int Et05_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmRack* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et05.eff"), EFF_ET05, 0);
    bin = GetEtcAddr(arc, "et0500.bin");
    if (EspGetEfmTplAddr(0x03, &tpl) == 0) {
        pLog->err(0, 0, "ET05:EFM[%02x] TPL err", 0x03);
        return 0;
    }
    em = SetRack(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET05:Mod err");
        return 0;
    }
    em->setEff(0x59);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 15: creates a rack enemy (cEmRack) rack type 2 from et1500.bin with effect data et15.eff,
// effect owner 0x69. d->type is the etc flag number; 1 on success.
int Et15_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmRack* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et15.eff"), EFF_ET15, 0);
    bin = GetEtcAddr(arc, "et1500.bin");
    tpl = GetEtcAddr(arc, "et1500.tpl");
    em = SetRack(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET05:Mod err");
        return 0;
    }
    em->setEff(0x69);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 06: creates a ladder object (cObjLadder) from et0600.bin with effect data et06.eff; the
// player motions it uses. d->type is the etc flag number; 1 on success.
int Et06_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cObjLadder* em;
    void* mot[20];

    bin = GetEtcAddr(arc, "et0600.bin");
    tpl = GetEtcAddr(arc, "et0600.tpl");
    em = (cObjLadder*) SetLadder(bin, tpl, &d->pos, &d->ang, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET06:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et06.eff"), EFF_ET06, 0);
    mot[0] = GetEtcAddr(arc, "pl00017.fcv");
    mot[1] = GetEtcAddr(arc, "pl00018.fcv");
    mot[2] = GetEtcAddr(arc, "pl00019.fcv");
    mot[3] = GetEtcAddr(arc, "pl00020.fcv");
    mot[4] = GetEtcAddr(arc, "pl00021.fcv");
    mot[5] = GetEtcAddr(arc, "pl00024.fcv");
    mot[6] = GetEtcAddr(arc, "et06000.fcv");
    mot[7] = GetEtcAddr(arc, "et06001.fcv");
    mot[8] = GetEtcAddr(arc, "et06002.fcv");
    mot[9] = GetEtcAddr(arc, "pl00025.fcv");
    mot[10] = GetEtcAddr(arc, "et06003.fcv");
    mot[11] = GetEtcAddr(arc, "et060000.seq");
    mot[12] = GetEtcAddr(arc, "et060010.seq");
    mot[13] = GetEtcAddr(arc, "et060020.seq");
    mot[14] = GetEtcAddr(arc, "et060030.seq");
    mot[15] = GetEtcAddr(arc, "et060031.seq");
    mot[16] = GetEtcAddr(arc, "pl01106.fcv");
    mot[17] = GetEtcAddr(arc, "pl01107.fcv");
    mot[18] = GetEtcAddr(arc, "pl01108.fcv");
    mot[19] = GetEtcAddr(arc, "pl01118.fcv");
    em->setMotion(mot);
    em->setLadderInfo(6, 0);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 08: creates a ladder object (cObjLadder) from et0800.bin with effect data et08.eff; the
// player motions it uses. d->type is the etc flag number; 1 on success.
int Et08_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cObjLadder* em;
    void* mot[20];

    bin = GetEtcAddr(arc, "et0800.bin");
    tpl = GetEtcAddr(arc, "et0800.tpl");
    em = (cObjLadder*) SetLadder(bin, tpl, &d->pos, &d->ang, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET08:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et08.eff"), EFF_ET08, 0);
    mot[0] = GetEtcAddr(arc, "pl00017.fcv");
    mot[1] = GetEtcAddr(arc, "pl00018.fcv");
    mot[2] = GetEtcAddr(arc, "pl00019.fcv");
    mot[3] = GetEtcAddr(arc, "pl00020.fcv");
    mot[4] = GetEtcAddr(arc, "pl00021.fcv");
    mot[5] = GetEtcAddr(arc, "pl00024.fcv");
    mot[6] = GetEtcAddr(arc, "et06000.fcv");
    mot[7] = GetEtcAddr(arc, "et06001.fcv");
    mot[8] = GetEtcAddr(arc, "et06002.fcv");
    mot[9] = GetEtcAddr(arc, "pl00025.fcv");
    mot[10] = GetEtcAddr(arc, "et06003.fcv");
    mot[11] = GetEtcAddr(arc, "et060000.seq");
    mot[12] = GetEtcAddr(arc, "et060010.seq");
    mot[13] = GetEtcAddr(arc, "et060020.seq");
    mot[14] = GetEtcAddr(arc, "et060030.seq");
    mot[15] = GetEtcAddr(arc, "et060031.seq");
    mot[16] = GetEtcAddr(arc, "pl01106.fcv");
    mot[17] = GetEtcAddr(arc, "pl01107.fcv");
    mot[18] = GetEtcAddr(arc, "pl01108.fcv");
    mot[19] = GetEtcAddr(arc, "pl01118.fcv");
    em->setMotion(mot);
    em->setLadderInfo(8, 1);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 09: creates a door enemy (cEmDoor) door type 3 from et0900.bin, obm4c00.bin with effect
// data et09.eff, obm4c.eff, effect owner 0x5D; with a chain (obm4c). d->type is the etc flag
// number; 1 on success.
int Et09_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et09.eff"), EFF_ET09, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm4c.eff"), EFF_OBM4C, 0);
    bin = GetEtcAddr(arc, "et0900.bin");
    tpl = GetEtcAddr(arc, "et0900.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 3, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET09:Mod err");
        return 0;
    }
    em->setEff(0x5D);
    em->setNoSuspend(1);
    bin = GetEtcAddr(arc, "obm4c00.bin");
    tpl = GetEtcAddr(arc, "obm4c00.tpl");
    em->setChain(bin, tpl);
    *out = em;
    return 1;
}

// Etc id 0A: creates a torch enemy (cEmTorch) torch type 5 from et0a00.bin with effect data
// et0a.eff, effect owner 0x5E. d->type is the etc flag number; 1 on success.
int Et0a_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et0a.eff"), EFF_ET0A, 0);
    bin = GetEtcAddr(arc, "et0a00.bin");
    if (EspGetEfmTplAddr(0x4B, &tpl) == 0) {
        pLog->err(0, 0, "ET0a:EFM[%02x] TPL err", 0x4B);
        return 0;
    }
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 5, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET0a:Mod err");
        return 0;
    }
    em->setEff(0x5E);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 0B: creates a torch enemy (cEmTorch) torch type 1 from et0b00.bin with effect data
// et0b.eff, effect owner 0x5F. d->type is the etc flag number; 1 on success.
int Et0b_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et0b.eff"), EFF_ET0B, 0);
    bin = GetEtcAddr(arc, "et0b00.bin");
    if (EspGetEfmTplAddr(0x0D, &tpl) == 0) {
        pLog->err(0, 0, "ET0b:EFM[%02x] TPL err", 0x0D);
        return 0;
    }
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET0B:Mod err");
        return 0;
    }
    em->setEff(0x5F);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 0C: creates a torch enemy (cEmTorch) torch type 2 from et0c00.bin with effect data
// et0c.eff, effect owner 0x60. d->type is the etc flag number; 1 on success.
int Et0c_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    bin = GetEtcAddr(arc, "et0c00.bin");
    tpl = GetEtcAddr(arc, "et0c00.tpl");
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET0C:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0c.eff"), EFF_ET0C, 0);
    em->setEff(0x60);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 0D: creates a door enemy (cEmDoor) door type 1 from et0d00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x61. d->type is the etc flag number; 1 on success.
int Et0d_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et0d00.bin");
    tpl = GetEtcAddr(arc, "et0d00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET0D:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET0D, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x61);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 10: creates a torch enemy (cEmTorch) torch type 4 from et1000.bin with effect data
// et10.eff, effect owner 0x64. d->type is the etc flag number; 1 on success.
int Et10_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et10.eff"), EFF_ET10, 0);
    bin = GetEtcAddr(arc, "et1000.bin");
    if (EspGetEfmTplAddr(0x0F, &tpl) == 0) {
        pLog->err(0, 0, "ET10:EFM[%02x] TPL err", 0x0F);
        return 0;
    }
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 4, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET10:Mod err");
        return 0;
    }
    em->setEff(0x64);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 0E: creates a lever switch (cEmSwitch) from et0e00.bin with effect data (none). d->type is
// the etc flag number; 1 on success.
int Et0e_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmSwitch* em;

    bin = GetEtcAddr(arc, "et0e00.bin");
    tpl = GetEtcAddr(arc, "et0e00.tpl");
    em = SetEmSwitch(bin, tpl, &d->pos, &d->ang, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET0E:Mod err");
        return 0;
    }
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 0F: creates a barred gate (cEmBarred) gate type 1 from et0f00.bin with effect data
// et0f.eff, effect owner 0x63. d->type is the etc flag number; 1 on success.
int Et0f_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et0f00.bin");
    tpl = GetEtcAddr(arc, "et0f00.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 1);
    if (em == 0) {
        pLog->err(0, 0, "ET0F:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0f.eff"), EFF_ET0F, 0);
    em->setEff(0x63);
    *out = em;
    return 1;
}

// Etc id 11: creates a box enemy (cEmBox) box type 3 from et1100.bin with effect data et11.eff,
// effect owner 0x65. d->type is the etc flag number; 1 on success.
int Et11_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et11.eff"), EFF_ET11, 0);
    bin = GetEtcAddr(arc, "et1100.bin");
    if (EspGetEfmTplAddr(0x09, &tpl) == 0) {
        pLog->err(0, 0, "ET11:EFM[%02x] TPL err", 0x09);
        return 0;
    }
    em = SetBox(bin, tpl, &d->pos, &d->ang, 3, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET11:Mod err");
        return 0;
    }
    em->setEff(0x65);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 12: creates a barrel enemy (cEmBarrel) barrel type 0 from et1200.bin with effect data
// et12.eff, effect owner 0x66. d->type is the etc flag number; 1 on success.
int Et12_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarrel* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et12.eff"), EFF_ET12, 0);
    bin = GetEtcAddr(arc, "et1200.bin");
    if (EspGetEfmTplAddr(0x0B, &tpl) == 0) {
        pLog->err(0, 0, "ET12:EFM[%02x] TPL err", 0x0B);
        return 0;
    }
    em = SetBarrel(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET12:Mod err");
        return 0;
    }
    em->setEff(0x66);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 13: creates a door enemy (cEmDoor) door type 0 from et1300.bin with effect data et13.eff,
// obm2b.eff, effect owner 0x67; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et13_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et13.eff"), EFF_ET13, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et1300.bin");
    if (EspGetEfmTplAddr(0x0C, &tpl) == 0) {
        pLog->err(0, 0, "ET13:EFM[%02x] TPL err", 0x0C);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET13:Mod err");
        return 0;
    }
    em->setEff(0x67);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 14: creates a torch enemy (cEmTorch) torch type 0 from et1400.bin with effect data
// et14.eff, effect owner 0x68. d->type is the etc flag number; 1 on success.
int Et14_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    bin = GetEtcAddr(arc, "et1400.bin");
    tpl = GetEtcAddr(arc, "et1400.tpl");
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET14:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et14.eff"), EFF_ET14, 0);
    em->setEff(0x68);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 16: creates a door enemy (cEmDoor) door type 1 from et1600.bin with effect data et16.eff,
// obm2b.eff, effect owner 0x6A. d->type is the etc flag number; 1 on success.
static int Et16_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et1600.bin");
    tpl = GetEtcAddr(arc, "et1600.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET16:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et16.eff"), EFF_ET16, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x6A);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 17: creates a door enemy (cEmDoor) door type 1 from et1700.bin with effect data et17.eff,
// obm2b.eff, effect owner 0x6B. d->type is the etc flag number; 1 on success.
int Et17_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et1700.bin");
    tpl = GetEtcAddr(arc, "et1700.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET17:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et17.eff"), EFF_ET17, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x6B);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 18: creates a door enemy (cEmDoor) door type 0 from et1800.bin with effect data et18.eff,
// obm2b.eff, effect owner 0x6C; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et18_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et18.eff"), EFF_ET18, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et1800.bin");
    if (EspGetEfmTplAddr(0x1E, &tpl) == 0) {
        pLog->err(0, 0, "ET18:EFM[%02x] TPL err", 0x1E);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET18:Mod err");
        return 0;
    }
    em->setEff(0x6C);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 19: creates a torch enemy (cEmTorch) torch type 0 from et1400.bin with effect data
// et19.eff, effect owner 0x6D. d->type is the etc flag number; 1 on success.
int Et19_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    bin = GetEtcAddr(arc, "et1400.bin");
    tpl = GetEtcAddr(arc, "et1400.tpl");
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET19:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et19.eff"), EFF_ET19, 0);
    em->setEff(0x6D);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 1A: creates a item enemy (cEmItem) emitem type 1 from et1a00.bin with effect data
// et1a.eff, effect owner 0x6E. d->type is the etc flag number; 1 on success.
int Et1a_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmItem* em;

    bin = GetEtcAddr(arc, "et1a00.bin");
    tpl = GetEtcAddr(arc, "et1a00.tpl");
    em = SetEmItem(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET1a:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et1a.eff"), EFF_ET1A, 0);
    em->setEff(0x6E);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 1B: creates a barred gate (cEmBarred) gate type 0 from et1b00.bin with effect data
// et0f.eff, effect owner 0x63. d->type is the etc flag number; 1 on success.
int Et1b_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et1b00.bin");
    tpl = GetEtcAddr(arc, "et1b00.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 0);
    if (em == 0) {
        pLog->err(0, 0, "ET1b:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0f.eff"), EFF_ET0F, 0);
    em->setEff(0x63);
    *out = em;
    return 1;
}

// Etc id 1C: creates a torch enemy (cEmTorch) torch type 2 from et1c00.bin with effect data
// et1c.eff, effect owner 0x70. d->type is the etc flag number; 1 on success.
int Et1c_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    bin = GetEtcAddr(arc, "et1c00.bin");
    tpl = GetEtcAddr(arc, "et1c00.tpl");
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET1C:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et1c.eff"), EFF_ET1C, 0);
    em->setEff(0x70);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 1E: creates a box enemy (cEmBox) box type 5 from et1e00.bin with effect data et11.eff,
// effect owner 0x65. d->type is the etc flag number; 1 on success.
int Et1e_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et11.eff"), EFF_ET11, 0);
    bin = GetEtcAddr(arc, "et1e00.bin");
    if (EspGetEfmTplAddr(0x09, &tpl) == 0) {
        pLog->err(0, 0, "ET1e:EFM[%02x] TPL err", 0x09);
        return 0;
    }
    em = SetBox(bin, tpl, &d->pos, &d->ang, 5, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET1e:Mod err");
        return 0;
    }
    em->setEff(0x65);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 1F: creates a box enemy (cEmBox) box type 4 from et1f00.bin with effect data et1f.eff,
// effect owner 0x73. d->type is the etc flag number; 1 on success.
int Et1f_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et1f.eff"), EFF_ET1F, 0);
    bin = GetEtcAddr(arc, "et1f00.bin");
    tpl = GetEtcAddr(arc, "et1f00.tpl");
    em = SetBox(bin, tpl, &d->pos, &d->ang, 4, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET1f:Mod err");
        return 0;
    }
    em->setEff(0x73);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 20: creates a door enemy (cEmDoor) door type 0 from et2000.bin with effect data et20.eff,
// obm2b.eff, effect owner 0x74; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et20_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et20.eff"), EFF_ET20, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et2000.bin");
    if (EspGetEfmTplAddr(0x27, &tpl) == 0) {
        pLog->err(0, 0, "ET20:EFM[%02x] TPL err", 0x27);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET20:Mod err");
        return 0;
    }
    em->setEff(0x74);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 21: creates a door enemy (cEmDoor) door type 1 from et2100.bin with effect data et21.eff,
// obm2b.eff, effect owner 0x75. d->type is the etc flag number; 1 on success.
int Et21_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et2100.bin");
    tpl = GetEtcAddr(arc, "et2100.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET21:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et21.eff"), EFF_ET21, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x75);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 22: creates a door enemy (cEmDoor) door type 0 from et2200.bin with effect data et22.eff,
// obm2b.eff, effect owner 0x76; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et22_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et22.eff"), EFF_ET22, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et2200.bin");
    if (EspGetEfmTplAddr(0x2C, &tpl) == 0) {
        pLog->err(0, 0, "ET22:EFM[%02x] TPL err", 0x2C);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET22:Mod err");
        return 0;
    }
    em->setEff(0x76);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 23: creates a door enemy (cEmDoor) door type 2 from et2300.bin with effect data et23.eff,
// obm2b.eff, effect owner 0x77. d->type is the etc flag number; 1 on success.
int Et23_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et2300.bin");
    tpl = GetEtcAddr(arc, "et2300.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET23:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et23.eff"), EFF_ET23, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x77);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 24: creates a door enemy (cEmDoor) door type 1 from et2400.bin with effect data et24.eff,
// obm2b.eff, effect owner 0x78. d->type is the etc flag number; 1 on success.
int Et24_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et2400.bin");
    tpl = GetEtcAddr(arc, "et2400.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET24:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et24.eff"), EFF_ET24, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x78);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 26: creates a barred gate (cEmBarred) gate type 2 from et2600.bin with effect data
// et0f.eff, effect owner 0x63. d->type is the etc flag number; 1 on success.
int Et26_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et2600.bin");
    tpl = GetEtcAddr(arc, "et2600.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 2);
    if (em == 0) {
        pLog->err(0, 0, "ET26:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0f.eff"), EFF_ET0F, 0);
    em->setEff(0x63);
    *out = em;
    return 1;
}

// Etc id 27: creates a door enemy (cEmDoor) door type 6 from et2700.bin with effect data et27.eff,
// obm2b.eff, effect owner 0x7B. d->type is the etc flag number; 1 on success.
int Et27_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et2700.bin");
    tpl = GetEtcAddr(arc, "et2700.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 6, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET21:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et27.eff"), EFF_ET27, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x7B);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 28: creates a barred gate (cEmBarred) gate type 3 from et2800.bin with effect data
// et0f.eff, effect owner 0x63. d->type is the etc flag number; 1 on success.
int Et28_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et2800.bin");
    tpl = GetEtcAddr(arc, "et2800.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 3);
    if (em == 0) {
        pLog->err(0, 0, "ET28:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0f.eff"), EFF_ET0F, 0);
    em->setEff(0x63);
    *out = em;
    return 1;
}

// Etc id 2A: creates a door enemy (cEmDoor) door type 1 from et2a00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x7E. d->type is the etc flag number; 1 on success.
int Et2a_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et2a00.bin");
    tpl = GetEtcAddr(arc, "et2a00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET2a:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET2A, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x7E);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 2B: creates a door enemy (cEmDoor) door type 1 from et2b00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x7F. d->type is the etc flag number; 1 on success.
int Et2b_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et2b00.bin");
    tpl = GetEtcAddr(arc, "et2b00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET2b:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET2B, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x7F);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 2D: creates a barrel enemy (cEmBarrel) barrel type 2 from et2d00.bin with effect data
// et2d.eff, effect owner 0x81. d->type is the etc flag number; 1 on success.
int Et2d_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarrel* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et2d.eff"), EFF_ET2D, 0);
    bin = GetEtcAddr(arc, "et2d00.bin");
    if (EspGetEfmTplAddr(0x44, &tpl) == 0) {
        pLog->err(0, 0, "ET2d:EFM[%02x] TPL err", 0x44);
        return 0;
    }
    em = SetBarrel(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET2D:Mod err");
        return 0;
    }
    em->setEff(0x81);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 30: creates a rack enemy (cEmRack) rack type 3 from et3000.bin with effect data et30.eff,
// effect owner 0x84. d->type is the etc flag number; 1 on success.
int Et30_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmRack* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et30.eff"), EFF_ET30, 0);
    bin = GetEtcAddr(arc, "et3000.bin");
    tpl = GetEtcAddr(arc, "et3000.tpl");
    em = SetRack(bin, tpl, &d->pos, &d->ang, 3, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET30:Mod err");
        return 0;
    }
    em->setEff(0x84);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 2E: creates a box enemy (cEmBox) box type 6 from et2e00.bin, et2e01.bin with effect data
// et2e.eff, effect owner 0x82; with a separate break model. d->type is the etc flag number; 1 on
// success.
static int Et2e_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et2e.eff"), EFF_ET2E, 0);
    bin = GetEtcAddr(arc, "et2e00.bin");
    if (EspGetEfmTplAddr(0x45, &tpl) == 0) {
        pLog->err(0, 0, "ET2e:EFM[%02x] TPL err", 0x45);
        return 0;
    }
    em = SetBox(bin, tpl, &d->pos, &d->ang, 6, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET2e:Mod err");
        return 0;
    }
    bin = GetEtcAddr(arc, "et2e01.bin");
    em->setBreakModel(bin, tpl);
    em->setEff(0x82);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 2F: creates a box enemy (cEmBox) box type 7 from et2f00.bin, et2f01.bin with effect data
// et2f.eff, effect owner 0x83; with a separate break model. d->type is the etc flag number; 1 on
// success.
int Et2f_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBox* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et2f.eff"), EFF_ET2F, 0);
    bin = GetEtcAddr(arc, "et2f00.bin");
    if (EspGetEfmTplAddr(0x46, &tpl) == 0) {
        pLog->err(0, 0, "ET2f:EFM[%02x] TPL err", 0x46);
        return 0;
    }
    em = SetBox(bin, tpl, &d->pos, &d->ang, 7, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET2f:Mod err");
        return 0;
    }
    bin = GetEtcAddr(arc, "et2f01.bin");
    em->setBreakModel(bin, tpl);
    em->setEff(0x83);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 31: creates a door enemy (cEmDoor) door type 2 from et3100.bin with effect data et31.eff,
// obm2b.eff, effect owner 0x85. d->type is the etc flag number; 1 on success.
int Et31_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3100.bin");
    tpl = GetEtcAddr(arc, "et3100.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET31:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et31.eff"), EFF_ET31, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x85);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 32: creates a door enemy (cEmDoor) door type 4 from et3200.bin with effect data et32.eff,
// obm2b.eff, effect owner 0x86; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et32_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3200.bin");
    tpl = GetEtcAddr(arc, "et3200.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 4, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET32:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et32.eff"), EFF_ET32, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x86);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 33: creates a door enemy (cEmDoor) door type 1 from et3300.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x87; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et33_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3300.bin");
    tpl = GetEtcAddr(arc, "et3300.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET33:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET33, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x87);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 34: creates a door enemy (cEmDoor) door type 1 from et3400.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x88. d->type is the etc flag number; 1 on success.
int Et34_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3400.bin");
    tpl = GetEtcAddr(arc, "et3400.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET34:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET34, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x88);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 37: creates a door enemy (cEmDoor) door type 2 from et3700.bin with effect data et37.eff,
// effect owner 0x8B. d->type is the etc flag number; 1 on success.
int Et37_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3700.bin");
    tpl = GetEtcAddr(arc, "et3700.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET37:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et37.eff"), EFF_ET37, 0);
    em->setEff(0x8B);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 38: creates a torch enemy (cEmTorch) torch type 2 from et0c00.bin with effect data
// et38.eff, effect owner 0x8C. d->type is the etc flag number; 1 on success.
int Et38_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmTorch* em;

    bin = GetEtcAddr(arc, "et0c00.bin");
    tpl = GetEtcAddr(arc, "et0c00.tpl");
    em = SetTorch(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET38:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et38.eff"), EFF_ET38, 0);
    em->setEff(0x8C);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 39: creates a door enemy (cEmDoor) door type 5 from et3900.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x8D. d->type is the etc flag number; 1 on success.
int Et39_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3900.bin");
    tpl = GetEtcAddr(arc, "et3900.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 5, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET39:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET39, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x8D);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 3A: creates a barred gate (cEmBarred) gate type 4 from et3a00.bin with effect data
// et3a.eff, effect owner 0x8E. d->type is the etc flag number; 1 on success.
int Et3a_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et3a00.bin");
    tpl = GetEtcAddr(arc, "et3a00.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 4);
    if (em == 0) {
        pLog->err(0, 0, "ET3a:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et3a.eff"), EFF_ET3A, 0);
    em->setEff(0x8E);
    *out = em;
    return 1;
}

// Etc id 3B: creates a door enemy (cEmDoor) door type 1 from et3b00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x8F. d->type is the etc flag number; 1 on success.
int Et3b_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3b00.bin");
    tpl = GetEtcAddr(arc, "et3b00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET3b:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET3B, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x8F);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 3C: creates a barrel enemy (cEmBarrel) barrel type 2 from et3c00.bin with effect data
// et3c.eff, effect owner 0x90. d->type is the etc flag number; 1 on success.
int Et3c_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarrel* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et3c.eff"), EFF_ET3C, 0);
    bin = GetEtcAddr(arc, "et3c00.bin");
    if (EspGetEfmTplAddr(0x51, &tpl) == 0) {
        pLog->err(0, 0, "ET3c:EFM[%02x] TPL err", 0x51);
        return 0;
    }
    em = SetBarrel(bin, tpl, &d->pos, &d->ang, 2, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET3c:Mod err");
        return 0;
    }
    em->setEff(0x90);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 3D: creates a door enemy (cEmDoor) door type 0 from et3d00.bin with effect data et3d.eff,
// obm2b.eff, effect owner 0x91; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et3d_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et3d.eff"), EFF_ET3D, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et3d00.bin");
    if (EspGetEfmTplAddr(0x55, &tpl) == 0) {
        pLog->err(0, 0, "ET3d:EFM[%02x] TPL err", 0x55);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET3d:Mod err");
        return 0;
    }
    em->setEff(0x91);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 3E: creates a door enemy (cEmDoor) door type 1 from et3e00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x92. d->type is the etc flag number; 1 on success.
int Et3e_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3e00.bin");
    tpl = GetEtcAddr(arc, "et3e00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET3e:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET3E, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x92);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 3F: creates a door enemy (cEmDoor) door type 1 from et3f00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x93. d->type is the etc flag number; 1 on success.
int Et3f_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et3f00.bin");
    tpl = GetEtcAddr(arc, "et3f00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET3f:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET3F, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x93);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 40: creates a door enemy (cEmDoor) door type 0 from et4000.bin with effect data et40.eff,
// obm2b.eff, effect owner 0x94; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et40_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et40.eff"), EFF_ET40, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    bin = GetEtcAddr(arc, "et4000.bin");
    if (EspGetEfmTplAddr(0x83, &tpl) == 0) {
        pLog->err(0, 0, "ET40:EFM[%02x] TPL err", 0x83);
        return 0;
    }
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 0, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET40:Mod err");
        return 0;
    }
    em->setEff(0x94);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 41: creates a door enemy (cEmDoor) door type 7 from et4100.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x95. d->type is the etc flag number; 1 on success.
int Et41_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4100.bin");
    tpl = GetEtcAddr(arc, "et4100.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 7, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET41:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET41, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x95);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 42: creates a wooden bar (cEmBar) from et4200.bin with effect data et42.eff, effect owner
// 0x96; the player motions it uses. d->type is the etc flag number; 1 on success.
int Et42_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBar* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et42.eff"), EFF_ET42, 0);
    bin = GetEtcAddr(arc, "et4200.bin");
    if (EspGetEfmTplAddr(0x5D, &tpl) == 0) {
        pLog->err(0, 0, "ET42:EFM[%02x] TPL err", 0x5D);
        return 0;
    }
    em = SetBar(bin, tpl, &d->pos, &d->ang, d->type);
    if (em == 0) {
        pLog->err(6, 0, "ET42:Mod err");
        return 0;
    }
    em->setEff(0x96);
    em->setMotion(GetEtcAddr(arc, "pl00591.fcv"));
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 43: creates a barred gate (cEmBarred) gate type 5 from et4300.bin with effect data
// et43.eff, effect owner 0x97. d->type is the etc flag number; 1 on success.
int Et43_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et4300.bin");
    tpl = GetEtcAddr(arc, "et4300.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 5);
    if (em == 0) {
        pLog->err(0, 0, "ET43:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et43.eff"), EFF_ET43, 0);
    em->setEff(0x97);
    *out = em;
    return 1;
}

// Etc id 45: creates a door enemy (cEmDoor) door type 7 from et4500.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x99. d->type is the etc flag number; 1 on success.
int Et45_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4500.bin");
    tpl = GetEtcAddr(arc, "et4500.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 7, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET45:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET45, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x99);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 46: creates a door enemy (cEmDoor) door type 4 from et4600.bin with effect data et46.eff,
// obm2b.eff, effect owner 0x9A; pane hit boxes. d->type is the etc flag number; 1 on success.
int Et46_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4600.bin");
    tpl = GetEtcAddr(arc, "et4600.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 4, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET46:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et46.eff"), EFF_ET46, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x9A);
    em->setNoSuspend(1);
    em->setYarare();
    *out = em;
    return 1;
}

// Etc id 47: creates a door enemy (cEmDoor) door type 7 from et4700.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x9B. d->type is the etc flag number; 1 on success.
int Et47_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4700.bin");
    tpl = GetEtcAddr(arc, "et4700.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 7, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET47:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET47, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x9B);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 49: creates a door enemy (cEmDoor) door type 1 from et4900.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0x9D. d->type is the etc flag number; 1 on success.
int Et49_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4900.bin");
    tpl = GetEtcAddr(arc, "et4900.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET49:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET49, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0x9D);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 4B: creates a barred gate (cEmBarred) gate type 6 from et4b00.bin with effect data
// et4b.eff, effect owner 0x9F. d->type is the etc flag number; 1 on success.
int Et4b_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et4b00.bin");
    tpl = GetEtcAddr(arc, "et4b00.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 6);
    if (em == 0) {
        pLog->err(0, 0, "ET4b:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et4b.eff"), EFF_ET4B, 0);
    em->setEff(0x9F);
    *out = em;
    return 1;
}

// Etc id 4C: creates a barred gate (cEmBarred) gate type 7 from et4c00.bin with effect data
// et4c.eff, effect owner 0xA0. d->type is the etc flag number; 1 on success.
int Et4c_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et4c00.bin");
    tpl = GetEtcAddr(arc, "et4c00.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 7);
    if (em == 0) {
        pLog->err(0, 0, "ET4c:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et4c.eff"), EFF_ET4C, 0);
    em->setEff(0xA0);
    *out = em;
    return 1;
}

// Etc id 4D: creates a door enemy (cEmDoor) door type 1 from et4d00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0xA1. d->type is the etc flag number; 1 on success.
static int Et4d_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4d00.bin");
    tpl = GetEtcAddr(arc, "et4d00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET4d:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET4D, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0xA1);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 4E: creates a door enemy (cEmDoor) door type 5 from et4e00.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0xA2. d->type is the etc flag number; 1 on success.
int Et4e_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et4e00.bin");
    tpl = GetEtcAddr(arc, "et4e00.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 5, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET4e:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET4E, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0xA2);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 4F: creates a barred gate (cEmBarred) gate type 6 from et4f00.bin with effect data
// et4b.eff, effect owner 0xA3. d->type is the etc flag number; 1 on success.
int Et4f_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et4f00.bin");
    tpl = GetEtcAddr(arc, "et4f00.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 6);
    if (em == 0) {
        pLog->err(0, 0, "ET4f:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et4b.eff"), EFF_ET4F, 0);
    em->setEff(0xA3);
    *out = em;
    return 1;
}

// Etc id 59: creates a door enemy (cEmDoor) door type 1 from et5900.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0xAD. d->type is the etc flag number; 1 on success.
int Et59_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et5900.bin");
    tpl = GetEtcAddr(arc, "et5900.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET59:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET59, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0xAD);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 61: creates a barred gate (cEmBarred) gate type 8 from et6100.bin with effect data
// et61.eff, effect owner 0xB5. d->type is the etc flag number; 1 on success.
int Et61_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et6100.bin");
    tpl = GetEtcAddr(arc, "et6100.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 8);
    if (em == 0) {
        pLog->err(0, 0, "ET61:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et61.eff"), EFF_ET61, 0);
    em->setEff(0xB5);
    *out = em;
    return 1;
}

// Etc id 62: creates a door enemy (cEmDoor) door type 7 from et6200.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0xB6. d->type is the etc flag number; 1 on success.
int Et62_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et6200.bin");
    tpl = GetEtcAddr(arc, "et6200.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 7, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET62:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET62, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0xB6);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 63: creates a door enemy (cEmDoor) door type 1 from et6300.bin with effect data et0d.eff,
// obm2b.eff, effect owner 0xB7. d->type is the etc flag number; 1 on success.
int Et63_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmDoor* em;

    bin = GetEtcAddr(arc, "et6300.bin");
    tpl = GetEtcAddr(arc, "et6300.tpl");
    em = SetDoor(bin, tpl, &d->pos, &d->ang, 1, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET63:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et0d.eff"), EFF_ET63, 0);
    EspDataLoad((u32) GetEtcAddr(arc, "obm2b.eff"), EFF_OBM2B, 0);
    em->setEff(0xB7);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 66: creates a rack enemy (cEmRack) rack type 5 from et6600.bin with effect data et66.eff,
// effect owner 0xBA. d->type is the etc flag number; 1 on success.
int Et66_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmRack* em;

    EspDataLoad((u32) GetEtcAddr(arc, "et66.eff"), EFF_ET66, 0);
    bin = GetEtcAddr(arc, "et6600.bin");
    tpl = GetEtcAddr(arc, "et6600.tpl");
    em = SetRack(bin, tpl, &d->pos, &d->ang, 5, d->type);
    if (em == 0) {
        pLog->err(0, 0, "ET66:Mod err");
        return 0;
    }
    em->setEff(0xBA);
    em->setNoSuspend(1);
    *out = em;
    return 1;
}

// Etc id 67: creates a barred gate (cEmBarred) gate type 8 from et6700.bin with effect data
// et67.eff, effect owner 0xBB. d->type is the etc flag number; 1 on success.
int Et67_init(void* arc, EtcSetData* d, cModel** out)
{
    void* bin;
    void* tpl;
    cEmBarred* em;

    bin = GetEtcAddr(arc, "et6700.bin");
    tpl = GetEtcAddr(arc, "et6700.tpl");
    em = SetEmBarred(bin, tpl, &d->pos, &d->ang, d->type, 8);
    if (em == 0) {
        pLog->err(0, 0, "ET67:Mod err");
        return 0;
    }
    EspDataLoad((u32) GetEtcAddr(arc, "et67.eff"), EFF_ET67, 0);
    em->setEff(0xBB);
    *out = em;
    return 1;
}


// Creates one etc model record: looks up the etc id's archive (EtcGetDasAddr), runs the id's
// Et??_init (the window ids from et00.cpp with their variant index), and registers the model in
// slot pDat->no (max 0x3F). 1 on success, errors logged.
int EtcModelSet(EtcSetData* pDat)
{
    void* arc;
    int ret = 0;
    cModel* model = 0;

    if (pDat->no > 0x3F) {
        pLog->err(0, 0, "EtcModelSet() : Invalid EtcModel No[%d](MAX:%d)", pDat->no, 0x40);
        return 0;
    }
    if (EtcGetDasAddr(pDat->id, &arc)) {
        switch (pDat->id) {
    case ETC_WINDOW00:
        ret = Et00_init(arc, pDat, &model, 0);
        break;
    case ETC_WOODBOX_SML:
        ret = Et01_init(arc, pDat, &model);
        break;
    case ETC_WOODBOX_MDL:
        ret = Et02_init(arc, pDat, &model);
        break;
    case ETC_DOOR00:
        ret = Et03_init(arc, pDat, &model);
        break;
    case ETC_TANA00:
        ret = Et04_init(arc, pDat, &model);
        break;
    case ETC_TANA01:
        ret = Et05_init(arc, pDat, &model);
        break;
    case ETC_HASIGO00:
        ret = Et06_init(arc, pDat, &model);
        break;
    case ETC_WINDOW07:
        ret = Et07_init(arc, pDat, &model, 1);
        break;
    case ETC_HASIGO01:
        ret = Et08_init(arc, pDat, &model);
        break;
    case ETC_DOOR_SP00:
        ret = Et09_init(arc, pDat, &model);
        break;
    case ETC_TAIMATU02:
        ret = Et0a_init(arc, pDat, &model);
        break;
    case ETC_LANTERN_A:
        ret = Et0b_init(arc, pDat, &model);
        break;
    case ETC_DENKYUU:
        ret = Et0c_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR00:
        ret = Et0d_init(arc, pDat, &model);
        break;
    case ETC_SWITCH:
        ret = Et0e_init(arc, pDat, &model);
        break;
    case ETC_BARRED00:
        ret = Et0f_init(arc, pDat, &model);
        break;
    case ETC_LANTERN_B:
        ret = Et10_init(arc, pDat, &model);
        break;
    case ETC_WOODBOX_BARREL:
        ret = Et11_init(arc, pDat, &model);
        break;
    case ETC_DRAM:
        ret = Et12_init(arc, pDat, &model);
        break;
    case ETC_DOOR01:
        ret = Et13_init(arc, pDat, &model);
        break;
    case ETC_TAIMATU01:
        ret = Et14_init(arc, pDat, &model);
        break;
    case ETC_TANA_BOX:
        ret = Et15_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR01:
        ret = Et16_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR02:
        ret = Et17_init(arc, pDat, &model);
        break;
    case ETC_DOOR02:
        ret = Et18_init(arc, pDat, &model);
        break;
    case ETC_TAIMATU03:
        ret = Et19_init(arc, pDat, &model);
        break;
    case ETC_MEDAL00:
        ret = Et1a_init(arc, pDat, &model);
        break;
    case ETC_BARRED01:
        ret = Et1b_init(arc, pDat, &model);
        break;
    case ETC_DENKYUU01:
        ret = Et1c_init(arc, pDat, &model);
        break;
    case ETC_WINDOW1D:
        ret = Et1d_init(arc, pDat, &model, 2);
        break;
    case ETC_WOODBOX_BARREL2:
        ret = Et1e_init(arc, pDat, &model);
        break;
    case ETC_NEST:
        ret = Et1f_init(arc, pDat, &model);
        break;
    case ETC_DOOR03:
        ret = Et20_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR03:
        ret = Et21_init(arc, pDat, &model);
        break;
    case ETC_DOOR04:
        ret = Et22_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR_DOWN00:
        ret = Et23_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR04:
        ret = Et24_init(arc, pDat, &model);
        break;
    case ETC_WINDOW25:
        ret = Et25_init(arc, pDat, &model, 3);
        break;
    case ETC_BARRED02:
        ret = Et26_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR05:
        ret = Et27_init(arc, pDat, &model);
        break;
    case ETC_BARRED03:
        ret = Et28_init(arc, pDat, &model);
        break;
    case ETC_WINDOW29:
        ret = Et29_init(arc, pDat, &model, 4);
        break;
    case ETC_IRON_DOOR06:
        ret = Et2a_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR07:
        ret = Et2b_init(arc, pDat, &model);
        break;
    case ETC_WINDOW2C:
        ret = Et2c_init(arc, pDat, &model, 5);
        break;
    case ETC_BOMB_BARREL:
        ret = Et2d_init(arc, pDat, &model);
        break;
    case ETC_TUBO_A_S:
        ret = Et2e_init(arc, pDat, &model);
        break;
    case ETC_TUBO_A_L:
        ret = Et2f_init(arc, pDat, &model);
        break;
    case ETC_YOROI:
        ret = Et30_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR_DOWN02:
        ret = Et31_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR11:
        ret = Et32_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR12:
        ret = Et33_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR13:
        ret = Et34_init(arc, pDat, &model);
        break;
    case ETC_WINDOW35:
        ret = Et35_init(arc, pDat, &model, 6);
        break;
    case ETC_WINDOW36:
        ret = Et36_init(arc, pDat, &model, 7);
        break;
    case ETC_IRON_DOOR_DOWN01:
        ret = Et37_init(arc, pDat, &model);
        break;
    case ETC_DENKYUU02:
        ret = Et38_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR08:
        ret = Et39_init(arc, pDat, &model);
        break;
    case ETC_BARRED04:
        ret = Et3a_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR10:
        ret = Et3b_init(arc, pDat, &model);
        break;
    case ETC_GUS_BOMBE:
        ret = Et3c_init(arc, pDat, &model);
        break;
    case ETC_DOOR05:
        ret = Et3d_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR14:
        ret = Et3e_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR15:
        ret = Et3f_init(arc, pDat, &model);
        break;
    case ETC_DOOR06:
        ret = Et40_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR17:
        ret = Et41_init(arc, pDat, &model);
        break;
    case ETC_DEKA_ITA:
        ret = Et42_init(arc, pDat, &model);
        break;
    case ETC_AUTO_DOOR:
        ret = Et43_init(arc, pDat, &model);
        break;
    case ETC_WINDOW44:
        ret = Et44_init(arc, pDat, &model, 8);
        break;
    case ETC_IRON_DOOR18:
        ret = Et45_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR19:
        ret = Et46_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR20:
        ret = Et47_init(arc, pDat, &model);
        break;
    case ETC_WINDOW48:
        ret = Et48_init(arc, pDat, &model, 9);
        break;
    case ETC_IRON_DOOR21:
        ret = Et49_init(arc, pDat, &model);
        break;
    case ETC_WINDOW4A:
        ret = Et4a_init(arc, pDat, &model, 10);
        break;
    case ETC_AUTO_DOOR2:
        ret = Et4b_init(arc, pDat, &model);
        break;
    case ETC_BARRED05:
        ret = Et4c_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR22:
        ret = Et4d_init(arc, pDat, &model);
        break;
    case ETC_BARRED06:
        ret = Et4e_init(arc, pDat, &model);
        break;
    case ETC_AUTO_DOOR3:
        ret = Et4f_init(arc, pDat, &model);
        break;
    case ETC_WINDOW50:
        ret = Et50_init(arc, pDat, &model, 11);
        break;
    case ETC_WINDOW51:
        ret = Et51_init(arc, pDat, &model, 12);
        break;
    case ETC_WINDOW52:
        ret = Et52_init(arc, pDat, &model, 13);
        break;
    case ETC_WINDOW53:
        ret = Et53_init(arc, pDat, &model, 14);
        break;
    case ETC_WINDOW54:
        ret = Et54_init(arc, pDat, &model, 15);
        break;
    case ETC_WINDOW55:
        ret = Et55_init(arc, pDat, &model, 16);
        break;
    case ETC_WINDOW56:
        ret = Et56_init(arc, pDat, &model, 17);
        break;
    case ETC_WINDOW57:
        ret = Et57_init(arc, pDat, &model, 18);
        break;
    case ETC_WINDOW58:
        ret = Et58_init(arc, pDat, &model, 19);
        break;
    case ETC_WINDOW5A:
        ret = Et5a_init(arc, pDat, &model, 20);
        break;
    case ETC_WINDOW5B:
        ret = Et5b_init(arc, pDat, &model, 21);
        break;
    case ETC_WINDOW5C:
        ret = Et5c_init(arc, pDat, &model, 22);
        break;
    case ETC_WINDOW5D:
        ret = Et5d_init(arc, pDat, &model, 23);
        break;
    case ETC_WINDOW5E:
        ret = Et5e_init(arc, pDat, &model, 24);
        break;
    case ETC_WINDOW5F:
        ret = Et5f_init(arc, pDat, &model, 25);
        break;
    case ETC_WINDOW60:
        ret = Et60_init(arc, pDat, &model, 26);
        break;
    case ETC_WINDOW64:
        ret = Et64_init(arc, pDat, &model, 27);
        break;
    case ETC_WINDOW65:
        ret = Et65_init(arc, pDat, &model, 28);
        break;
    case ETC_IRON_DOOR23:
        ret = Et59_init(arc, pDat, &model);
        break;
    case ETC_AUTO_DOOR4:
        ret = Et61_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR25:
        ret = Et62_init(arc, pDat, &model);
        break;
    case ETC_IRON_DOOR24:
        ret = Et63_init(arc, pDat, &model);
        break;
    case ETC_ZOU:
        ret = Et66_init(arc, pDat, &model);
        break;
    case ETC_AUTO_DOOR5:
        ret = Et67_init(arc, pDat, &model);
        break;
        default:
            pLog->err(0, 0, "EtcModelSet() : Invalid EtcModelID[%02x]", pDat->id);
            return 0;
        }
    }
    if (ret == 1) {
        g_EtcTbl[pDat->no].RegistData(pDat, model);
    } else {
        pLog->err(0, 0, "EtcModelSet() : Init failed.");
        return 0;
    }
    return ret;
}

// The etc archive registered for etc id `id`; 0 when none.
int EtcGetDasAddr(int id, void** pRet_addr)
{
    if (g_addr == 0) {
        *pRet_addr = 0;
        return 0;
    }
    *pRet_addr = g_addr;
    return 1;
}

// The persistent flag word of etc slot `no` in room `room`'s save record (broken / opened /
// collected bits the object classes keep); NULL for a bad slot or a room without a record.
u16* GetEtcFlgPtr(u32 etc_no, u16 room_no)
{
    u8* p;

    if (etc_no < 0 || etc_no > 0x3F) {
        pLog->err(6, 0, "GetEtcFlgPtr() : Invalid EtcModel No[%d](MAX:%d)", etc_no, 0x40);
        return 0;
    }
    p = RoomData.getRoomSavePtr(room_no);
    if (p == 0) {
        return 0;
    }
    return &((EtcRoomSave*) p)->etcFlag[etc_no];
}

// display flag (be_flag bit1) of an etc model
static inline void etcDispSet(cUnit* u, int on)
{
    if (on == 1) {
        u->be_flag |= 2;
    } else {
        u->be_flag &= ~2;
    }
}

// The breakable etc object in slot `no` of any kind (window, box, door, torch, rack, drum); 0
// (error when flag) when it is none of those.
int getRoomEtcBreak(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtcWindow(no, (cEmWindow**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtcBox(no, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtcDoor(no, (cEmDoor**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtcTorch(no, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtcRack(no, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtcDram(no, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcBreak() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// Shows / hides etc slot `no` (breakable objects through their own display logic, others by the
// model flag); 0 with an error for an empty slot.
int setRoomEtcDisp(int no, int bDisp, int bErrDisp)
{
    cEm* em;

    if (setRoomEtcBreakDisp(no, bDisp, 0) == 1) {
        return 1;
    }
    if (getRoomEtc2(no, &em, 0)) {
        etcDispSet(em, bDisp);
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "setRoomEtcDisp() : no[%d] is no data.", no);
    }
    return 0;
}

// Shows / hides a breakable etc object.
static int setRoomEtcBreakDisp(int no, int bDisp, int bErrDisp)
{
    cEm* em;

    if (getRoomEtcBreak(no, &em, 0)) {
        if (em->hp > 0) {
            etcDispSet(em, bDisp);
        }
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "setRoomEtcBreakDisp() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The torch (light-bearing etc) in slot `id`.
int getRoomEtcOnLight(u32 no, cModel** ppEm, int bErrDisp)
{
    if (getRoomEtcTorch(no, (cEm**) ppEm, 0)) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcOnLight() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The window (any of the et00.cpp window ids) in slot `no`.
int getRoomEtcWindow(int no, cEmWindow** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_WINDOW00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW07, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW1D, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW25, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW29, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW2C, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW35, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW36, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW44, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW48, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW4A, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW50, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW51, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW52, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW53, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW54, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW55, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW56, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW57, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW58, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW5A, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW5B, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW5C, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW5D, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW5E, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW5F, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW60, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW64, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WINDOW65, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcWindow() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The box / vase / crate (box etc ids) in slot `no`.
int getRoomEtcBox(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_WOODBOX_SML, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WOODBOX_MDL, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WOODBOX_BARREL, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_WOODBOX_BARREL2, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_NEST, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_TUBO_A_S, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_TUBO_A_L, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcBox() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The door (any door etc id) in slot `no`.
int getRoomEtcDoor(int no, cEmDoor** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_DOOR00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR01, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR02, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR03, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR04, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR05, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR06, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR01, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR02, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR03, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR04, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR05, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR06, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR07, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR08, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR10, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR11, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR12, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR13, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR14, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR15, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR17, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR18, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR19, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR20, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR21, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR22, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR23, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR24, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR25, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR_DOWN00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR_DOWN01, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_IRON_DOOR_DOWN02, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BARRED06, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DOOR_SP00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcDoor() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The rack / pillar (rack etc ids) in slot `no`.
int getRoomEtcRack(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_TANA00, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_TANA01, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_TANA_BOX, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_YOROI, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_ZOU, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcRack() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The ladder (ids 06 / 08) in slot `no`.
int getRoomEtcLadder(int no, cObjLadder** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_HASIGO00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_HASIGO01, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcLadder() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The torch / lamp (torch etc ids) in slot `no`.
int getRoomEtcTorch(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_TAIMATU01, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_TAIMATU02, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_TAIMATU03, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_LANTERN_A, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_LANTERN_B, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DENKYUU, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DENKYUU01, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_DENKYUU02, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcTorch() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The lever switch (id 0E) in slot `no`.
int getRoomEtcSwitch(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_SWITCH, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcSwitch() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The barred gate (gate etc ids) in slot `no`.
int getRoomEtcBarred(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_BARRED00, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BARRED01, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BARRED02, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BARRED03, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BARRED04, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BARRED05, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_AUTO_DOOR, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_AUTO_DOOR2, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_AUTO_DOOR3, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_AUTO_DOOR4, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_AUTO_DOOR5, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcBarred() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The barrel / drum (ids 12 / 2D / 3C) in slot `no`.
int getRoomEtcDram(int no, cEm** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_DRAM, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_BOMB_BARREL, ppEm, 0) == 1) {
        return 1;
    }
    if (getRoomEtc(no, ETC_GUS_BOMBE, ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcDram() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// The shootable item medal (id 1A) in slot `no`.
int getRoomEtcItem(int no, EtcItem** ppEm, int bErrDisp)
{
    if (getRoomEtc(no, ETC_MEDAL00, (cEm**) ppEm, 0) == 1) {
        return 1;
    }
    if (bErrDisp) {
        pLog->err(6, 0, "getRoomEtcItem() : no[%d] is ID diff[%02x].", no, getRoomEtcID(no));
    }
    return 0;
}

// COMPILER-DIFF: 6 (cross-jump survivor). The original lays the 0x404..0x411 `return 1` block out
// in place and the 0x108 / 0x10C..0x11E tests jump back to it. In a leaf function our jump2 keeps
// the first `li r3,1; blr` copy too, except that the last copy is followed by a block starting with
// a set of r3 (`lhz r3` of the else arm), which makes jump.c's "x = a; goto l" path skip that
// copy's cross-jump in iteration 1 and delete the first copy into it in iteration 2. The empty
// `asm volatile("")` at the top of the else arm (no code) keeps the else block from starting with
// a set of r3, so the first copy survives as in the original.
ETC_AMB_TYPE GetEtcAmbType()
{
    if (pG->room_id == 0x400 || pG->room_id == 0x403) {
        return ETC_AMB_DAY;
    } else if (pG->room_id == 0x402) {
        return ETC_AMB_DAY3;
    } else if (pG->room_id >= 0x404 && pG->room_id <= 0x411) {
        return ETC_AMB_NIGHT;
    } else if (pG->room_id == 0x331) {
        return ETC_AMB_DAY2;
    } else if (pG->room_id == 0x108) {
        return ETC_AMB_NIGHT;
    } else if (pG->room_id == 0x106) {
        return ETC_AMB_DAY4;
    } else if (pG->room_id == 0x102 || pG->room_id == 0x109 || pG->room_id == 0x119 || pG->room_id == 0x10A ||
               pG->room_id == 0x10B || pG->room_id == 0x327 || pG->room_id == 0x30F || pG->room_id == 0x31D) {
        return ETC_AMB_DAY2;
    } else if (pG->room_id >= 0x100 && pG->room_id <= 0x10B) {
        return ETC_AMB_DAY;
    } else if (pG->room_id >= 0x10C && pG->room_id <= 0x11E) {
        return ETC_AMB_NIGHT;
    } else {
        asm volatile("");
        u32 room = pG->room_id;
        if (room >= 0x200 && room <= 0x22A) {
            return ETC_AMB_DAY3;
        }
        return (ETC_AMB_TYPE) (room >= 0x300);   // ETC_AMB_NIGHT in stage 3 and up, ETC_AMB_DAY below
    }
}

// 1 when the ganados' glowing eye effect may be shown in the current room (off in the bright
// village rooms 102 / 106 / 108 / 109 / 10A / 119).
int GetEm10EyeEffectEnable()
{
    if (pG->room_id == 0x108) {
        return 0;
    }
    if (pG->room_id == 0x106) {
        return 0;
    }
    if (pG->room_id == 0x102) {
        return 0;
    }
    if (pG->room_id == 0x109) {
        return 0;
    }
    if (pG->room_id == 0x119) {
        return 0;
    }
    if (pG->room_id == 0x10A) {
        return 0;
    }
    if (pG->room_id == 0x10B) {
        return 0;
    }
    if (pG->room_id >= 0x100 && pG->room_id <= 0x10B) {
        return 0;
    }
    if (pG->room_id >= 0x10C && pG->room_id <= 0x11E) {
        return 1;
    }
    if (pG->room_id >= 0x200 && pG->room_id <= 0x22A) {
        return 0;
    }
    return pG->room_id >= 0x300;
}

static EtcAmbRgb etc_day_rgb[17] = {
    {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00}, {0x32, 0x32, 0x32},
    {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x1E, 0x1E, 0x1E},
    {0x28, 0x28, 0x28}, {0x32, 0x32, 0x32},
};
EtcAmbRgb etc_night_rgb[17] = {
    {0x00, 0x00, 0x00}, {0x32, 0x32, 0x32}, {0x00, 0x00, 0x00}, {0x10, 0x10, 0x10}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00}, {0x10, 0x10, 0x10},
    {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x1E, 0x1E, 0x1E},
    {0x28, 0x28, 0x28}, {0x32, 0x32, 0x32},
};
EtcAmbRgb etc_day2_rgb[17] = {
    {0x00, 0x00, 0x00}, {0x2D, 0x2D, 0x2D}, {0x00, 0x00, 0x00}, {0x28, 0x28, 0x28}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00}, {0x1E, 0x1E, 0x1E},
    {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x1E, 0x1E, 0x1E},
    {0x28, 0x28, 0x28}, {0x32, 0x32, 0x32},
};
EtcAmbRgb etc_day3_rgb[17] = {
    {0x00, 0x00, 0x00}, {0x40, 0x40, 0x40}, {0x00, 0x00, 0x00}, {0x40, 0x40, 0x40}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00}, {0x28, 0x28, 0x28},
    {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20}, {0x1E, 0x1E, 0x1E},
    {0x28, 0x28, 0x28}, {0x32, 0x32, 0x32},
};
EtcAmbRgb etc_day4_rgb[17] = {
    {0x00, 0x00, 0x00}, {0x50, 0x50, 0x50}, {0x00, 0x00, 0x00}, {0x50, 0x50, 0x50}, {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x5A, 0x5A, 0x5A}, {0x00, 0x00, 0x00}, {0x46, 0x46, 0x46},
    {0x20, 0x20, 0x20}, {0x28, 0x28, 0x28}, {0x14, 0x14, 0x14}, {0x20, 0x20, 0x20}, {0x1E, 0x1E, 0x1E},
    {0x28, 0x28, 0x28}, {0x32, 0x32, 0x32},
};

// Applies the room's additive ambient colour for etc ambient kind `no` (the etc classes pass
// their kind) from the day / night / variant tables chosen by the room id (GetEtcAmbType).
void EtcSetAddAmb(cModel* pMod, int kind)
{
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;

    switch (GetEtcAmbType()) {
    case ETC_AMB_DAY:
        r = etc_day_rgb[kind].r;
        g = etc_day_rgb[kind].g;
        b = etc_day_rgb[kind].b;
        break;
    case ETC_AMB_NIGHT:
        r = etc_night_rgb[kind].r;
        g = etc_night_rgb[kind].g;
        b = etc_night_rgb[kind].b;
        break;
    case ETC_AMB_DAY2:
        r = etc_day2_rgb[kind].r;
        g = etc_day2_rgb[kind].g;
        b = etc_day2_rgb[kind].b;
        break;
    case ETC_AMB_DAY3:
        r = etc_day3_rgb[kind].r;
        g = etc_day3_rgb[kind].g;
        b = etc_day3_rgb[kind].b;
        break;
    case ETC_AMB_DAY4:
        r = etc_day4_rgb[kind].r;
        g = etc_day4_rgb[kind].g;
        b = etc_day4_rgb[kind].b;
        break;
    default:
        pLog->err(6, 0, "EtcSetAddAmb() : invalid type.");
        break;
    }
    if (r == 0 && g == 0 & b == 0) {   // `&`: the original tests g and b with a bitwise and
        pMod->be_flag &= ~8;
    } else {
        pMod->be_flag |= 8;
    }
    pMod->AddAmb_r = r;
    pMod->AddAmb_g = g;
    pMod->AddAmb_b = b;
}
