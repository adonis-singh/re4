#ifndef EVENT_H
#define EVENT_H

#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "cManager.h"
#include "main_mem.h"

#line 8 "D:/Bio4/Prog/event.h"

// Event system header (game/event.cpp: the cutscene player). db_work includes it between atari.h
// and light.h; only string literals of its inline members land in the including units' .rodata:
// the file name (EventMgr::memAlloc's MEM_ALLOC) followed by the empty string.
class cEvent {
public:
    u8* pData;
    u32 nData;

    u8* getData(u32 no) {
        if (no >= nData) {
            dbgAssert(__FILE__, __LINE__);
        }
        return pData + no;
    }
    // some inline in the original event.h carries an empty string literal: 4 zero bytes follow
    // the file-name string in every including unit's .rodata
    const char* emptyName() { return ""; }
};

class cModel;
class cLit;

// Named data slot table (game/event.cpp `cDatTbl`, demangled `DatTbl`): `num` entries of 0x3C.
struct DatTblEntry {
    char Name[0x30];   // 0x00
    u8 FlagBe8;           // 0x30  bit0: in use, bit1: `dat2` is a debug-heap block freed with the entry
    u8 Etc;           // 0x31
    u16 Count;         // 0x32  reference count (SetDat of an existing name increments it)
    void* Dat;         // 0x34
    void* dat2;        // 0x38
};

class DatTbl {
public:
    int NumDatTbl;               // 0x00
    DatTblEntry* pWork;    // 0x04

    DatTbl();
    ~DatTbl();
    int init(int num);
    int end();
    int SetDat(const char* name, void* dat, u8 type, void* dat2, u8 flag, int* wkNo);
    int GetDat(void** pDat, u8* pEtc, const char* name, int* pNoWork);
    int ChkDat(const char* name);
    int GetWkNo(int* pNo, const char* name);
    int GetNumDat();
    int GetDatWkNo(void** pDat, u8* pEtc, int noWork);
    int ChkDatWkNoName(int noWork, const char* name);
    int DelDatWkNo(int noWork);
    int DelDat(const char* name);
    int DelAll(int flag);
};

// Event file ("even" "t" header) the room's evd data hands to EventMgr::SetEvt.
struct EvtBinEntry {
    char name[0x30];   // 0x00  file name of the bin/tpl
    u32 ofs;           // 0x30  offset of its data from the header
    u8 pad_34[0xC];
};

struct EvtHeader {
    char tag[8];       // 0x00  "even" "t"
    u8 pad_8[0x18];
    char room[8];      // 0x20  "r105"
    char no[0xC];      // 0x28  "s10"
    s32 sndFlag;       // 0x34  bit31: no SndEventInit / SndEventEnd
    u8 pad_38[8];
    u32 pacOfs;        // 0x40  first packet
    u32 pacSize;       // 0x44  size of the packet stream
    s32 nBin;          // 0x48  entries of the bin table
    u32 binOfs;        // 0x4C  EvtBinEntry[nBin]
};

// Event::StatusFlag bit numbers (PS2 EvtStatusFlag): FlgOnStatus(no) sets bit 31 - no, so the mask of flag
// f is EvtStfBit(f).
enum EvtStatusFlag {
    EvtStfToolStop = 0,
    EvtStfToolFrontExec = 1,
    EvtStfToolExec = 2,
    EvtStfEvtCancelCut = 3,
    EvtStfEvtCancelExe = 4,
    EvtStfEvtCancelOn = 5,
    EvtStfEvtCancelFalse = 6,
    EvtStfEvtInit = 7,
    EvtStfEventEnd = 8,
    EvtStfEndWaitOrder = 9,
    EvtStfEndWait = 10,
    EvtStfEndSleepOrder = 11,
    EvtStfEndSleep = 12,
    EvtStfSetEff = 13,
    EvtStfNoFunc = 14,
    EvtStfStrTime = 15,
    EvtStfStartWait = 16,
    EvtStfEvtCancelSet = 17,
    EvtStfSetMdt = 18,
    EvtStfTrial1St = 19,
    EvtStfPlPosNoSet = 20,
    EvtStfFadeOut = 21,
    EvtStfDiedemo = 22,
    EvtStfDiedemoSet = 23,
    EvtStfSceEventStartTrue = 24,
    EvtStfSubCharNoCtrl = 25,
    EvtStfActMesClear = 26,
    EvtStfEvtNoCancelSfd = 27,
    EvtStfMissionFailed = 28
};

#define EvtStfBit(f) (0x80000000 >> (f))

// Event packet type (PS2 EvpTp): EvtPacket::id, the row of Event::ExePacket packetTbl.
enum EvpTp {
    EvpTpBeginEvt = 0,
    EvpTpSetPl = 1,
    EvpTpSetEm = 2,
    EvpTpSetOm = 3,
    EvpTpSetParts = 4,
    EvpTpSetList = 5,
    EvpTpCam = 6,
    EvpTpCamPos = 7,
    EvpTpCamDammy = 8,
    EvpTpPos = 9,
    EvpTpPosPl = 10,
    EvpTpMot = 11,
    EvpTpShp = 12,
    EvpTpEsp = 13,
    EvpTpLit = 14,
    EvpTpStr = 15,
    EvpTpSe = 16,
    EvpTpMes = 17,
    EvpTpFunc = 18,
    EvpTpParentOn = 19,
    EvpTpParentOff = 20,
    EvpTpEndPl = 21,
    EvpTpEndEm = 22,
    EvpTpEndOm = 23,
    EvpTpEndParts = 24,
    EvpTpEndList = 25,
    EvpTpEndEvt = 26,
    EvpTpEndPac = 27,
    EvpTpSetEff = 28,
    EvpTpFade = 29,
    EvpTpFog = 30,
    EvpTpFocus = 31,
    EvpTpSetMdt = 32,
    EvpTpMax = 33
};

// Event packet stream: a 0x10 header followed by the per-id parameters (`size` bytes in total).
// The union members below are the shapes the handlers read.
struct EvtPacket {
    int id;            // 0x00  EvpTp: packetTbl index (0..0x20)
    u32 flag;          // 0x04  bit31: position relative to the base model, bit30: OyaSetObj18, bit29: Str one-shot
    s16 cut;           // 0x08
    s16 frame;         // 0x0A
    s16 size;          // 0x0C  offset of the next packet
    s16 pad_E;
    union {
        struct {
            char name[0xC];   // 0x10  model name
            char bin[0x30];   // 0x1C
            char tpl[0x30];   // 0x4C
        } mod;
        struct {
            char name[0xC];   // 0x10
            char oya[0xC];    // 0x1C
            char bin[0x30];   // 0x28
            char tpl[0x30];   // 0x58
        } parts;
        struct {
            char name[0xC];   // 0x10
            char oya[0xC];    // 0x1C
            s32 pos[3];       // 0x28
            s32 rot[3];       // 0x34  degrees
            s32 partsNo;      // 0x40
        } pos;
        struct {
            char name[0xC];   // 0x10
            s32 type;         // 0x1C
            u8 pad_20[3];
            u8 parts;         // 0x23
        } esp;
        struct {
            s32 no;           // 0x10
            s32 arg;          // 0x14
            s32 time;         // 0x18
        } val;
    };
};

// One event model registered by the debug tool (EventDebug::pModel, 0x644 bytes).
struct EvtDebugModel {
    char name[0x30];       // 0x00
    char bin[16][0x30];    // 0x30
    char tpl[16][0x30];    // 0x330
    s32 nBin;              // 0x630
    cModel* pModel;        // 0x634
    u8 otType;             // 0x638  cModel::ot_type
    u8 lightMask;          // 0x639  cModel::LightInfo.EnableMask
    u8 pad_63A[2];
    u32 flags;             // 0x63C  bit31: scroll object, bit30: be_flag bit12
    cModel* pScr;          // 0x640  the model when its name starts with "scr"
};

// Event work (game/event.cpp): a cUnit managed by EventMgr, 0x13C bytes.
class Event : public cUnit {
public:
    u8 EndRNo0;                 // 0x0C
    s8 EndRNo1;            // 0x0D  DelEvt: 0 run ExeEndEvt, 1 wait `endWait` frames
    s8 EndRNo2;            // 0x0E
    u8 EndRNo3;            // 0x0F
    u8 Id;                 // 0x10  constructor argument (EventMgr::construct id)
    u8 Type;               // 0x11
    u8 pad_12[2];
    int effNo;             // 0x14  effect owner slot: -1 none, 0/1 -> EspDataLoad owner 0xC4 + effNo
    char Name[0x20];       // 0x18  event name ("r105s10")
    EvtHeader* pData;      // 0x38
    EvtPacket* pPacket;    // 0x3C  current packet
    EvtPacket* pPrevPacket;  // 0x40  packet executed before it
    u32 StatusFlag;            // 0x44  EvtStfBit(EvtStatusFlag) bits (FlgOnStatus numbers them from bit 31 down)
    DatTbl ModTbl;         // 0x48  models of the event (name -> cModel*, type)
    Mtx MatCamOya;            // 0x50  camera base matrix (ExePacket_Pos "cam0000")
    cModel* PPl;          // 0x80  the "pl0000" object model (player stand-in)
    cModel* PModOya;       // 0x84  "oya0000" position base
    void* x88;             // 0x88
    u32 PFuncTbl;           // 0x8C  ExePacket_Func table (void (*[])(Event*, int)), kept as an address
    int NowTotalFrame;        // 0x90
    int MaxTotalFrame;     // 0x94
    int NowFrame;             // 0x98  frame in the cut
    int MaxFrame;          // 0x9C  frames of the cut
    int NowCut;               // 0xA0
    int MaxCut;            // 0xA4
    int BakNowTotalFrame;     // 0xA8  DebugDisp copies (DebugDispTool prints them)
    int BakMaxTotalFrame;  // 0xAC
    int BakNowFrame;          // 0xB0
    int BakMaxFrame;       // 0xB4
    int BakNowCut;            // 0xB8
    int BakMaxCut;         // 0xBC
    int NowStr[2];          // 0xC0  stream number per block (-1 = none)
    u32 SndId[2];          // 0xC8  SndStrReq id per block
    int EvtCancelCut;         // 0xD0  RunEvtCancel: cut the cancel skips to
    int TimerMes;          // 0xD4
    int NoEvt;               // 0xD8
    int NoLit;               // 0xDC
    int FFNowFrame;         // 0xE0  RunTool: frame the tool seeks to
    int actBtnOn;          // 0xE4
    int actBtnCount;       // 0xE8
    int actBtnNo;          // 0xEC ACTION_TYPE (PS2 EvtActBtnCtrl::ActType)
    int FuncType;          // 0xF0  ExeFunc mode the Evt_*_Func handler sees (0 begin, 1 run, 2 end, 3 cancel)
    int EmListNo;         // 0xF4  EspEvModList entries used
    void* pDatFog;            // 0xF8  fog Hermite curves (ExePacket_Fog)
    void* pDatFocus;          // 0xFC  focus Hermite curves (ExePacket_Focus)
    int MesNoOld;             // 0x100
    int DelTimer;           // 0x104
    int ChangeNoStr;           // 0x108  ExePacket_Str time override
    int ChangeNowCut;           // 0x10C  cut jump pending (CalNextFrame)
    int toolCut;           // 0x110
    int toolFrame2;        // 0x114
    cLit* pLit;            // 0x118  room lit set by ExePacket_Lit
    u8 pad_11C[0x13C - 0x11C];

    Event(u32 type);
    virtual ~Event();
    int init(char* name, EvtHeader* data);
    int Run();
    void EspSetModelPtr(cModel* pMod);
    int EspToolSetDat();
    void EspToolSetMod(int npMod, char* name);
    int GetModelPtrNo(int* pNoWork, cModel** pPtr, char* name);
    int RunTool(int mode, int subFrame);
    int RunEvtCancel();
    void CancelSet();
    void CancelNoSet();
    void ControlTransFlag();
    void DebugDisp();
    void DebugDispTool();
    int IsExePacket();
    int ExePacket();
    static int ExePacket_BeginEvt(Event* pEvt);
    static int ExePacket_SetPl(Event* pEvt);
    static int ExePacket_SetEm(Event* pEvt);
    static int ExePacket_SetOm(Event* pEvt);
    static int ExePacket_SetParts(Event* pEvt);
    int ExePacket_SetPartsSub(char* name, char* bin, char* tpl, char* oya);
    static int ExePacket_SetList(Event* pEvt);
    static int ExePacket_SetEff(Event* pEvt);
    static int ExePacket_SetMdt(Event* pEvt);
    static int ExePacket_Cam(Event* pEvt);
    static int ExePacket_CamPos(Event* pEvt);
    static int ExePacket_CamDammy(Event* pEvt);
    static int ExePacket_Pos(Event* pEvt);
    static int ExePacket_PosPl(Event* pEvt);
    static int ExePacket_Mot(Event* pEvt);
    static int ExePacket_Shp(Event* pEvt);
    static int ExePacket_Esp(Event* pEvt);
    static int ExePacket_Lit(Event* pEvt);
    static int ExePacket_Fog(Event* pEvt);
    static int ExePacket_Focus(Event* pEvt);
    static int ExePacket_Str(Event* pEvt);
    static int ExePacket_Se(Event* pEvt);
    static int ExePacket_Fade(Event* pEvt);
    static int ExePacket_Mes(Event* pEvt);
    static int ExePacket_Func(Event* pEvt);
    static int ExePacket_ParentOn(Event* pEvt);
    static int ExePacket_ParentOff(Event* pEvt);
    static int ExePacket_EndPl(Event* pEvt);
    static int ExePacket_EndEm(Event* pEvt);
    static int ExePacket_EndOm(Event* pEvt);
    static int ExePacket_EndParts(Event* pEvt);
    static int ExePacket_EndList(Event* pEvt);
    static int ExePacket_EndEvt(Event* pEvt);
    static int ExePacket_EndPac(Event* pEvt);
    void ExeBeginEvt(Event* pEvt, int FlagCommon);
    void ExeEndEvt(Event* pEvt, u32 FlagCommon);
    int ExeFunc(int mode, int param);
    void CalNextPacket();
    void CalNextFrame();
    void ChkCutZero();
    int CalMaxCut(int* maxCut);
    int CalMaxFrame(int* maxFrame, int cut);
    int CalMaxTotalFrame(int* maxCut, int* maxTotal);
    void SetDiedemoExec();
    void BeginActBtn(int act_type);
    void EndActBtn();
    int GetActBtnCount();
    void ExecActBtn();
    void MesSet(int noMes, int timer, int px, int py);
    void MesClear();
    void FogMove(Event* pEvt, void* pDatFog);
    void FocusMove(Event* pEvt, void* pDatFocus);
    void MotClear();
    int SetMod(char* name, void* mod, u8 type, void* dat2, u8 flag, int* wkNo);
    int GetMod(void** mod, char* name, u8* type, int* wkNo);
    void FlgOnStatus(u32 no)
    {
        u32* f = &StatusFlag;
        f[no >> 5] |= 0x80000000 >> (no & 0x1F);
    }
};

// Enemy module the manager loaded for an event (EventMgr::readEm[8], 4 bytes).
struct EvtReadEm {
    u8 em;       // 0x00  enemy id (0 = none)
    u8 swapped;  // 0x01  1 = its data was swapped into the module block
    u8 pad_2[2];
};

// EventMgr::EvtReadExec `evtReadFlag` bits (PS2 EvtReadFlag): how the event is read and started.
enum EvtReadFlag {
    EvtReadFlagNone = 0,
    EvtReadFlagNoBgmMute = 1,
    EvtReadFlagDiedemo = 2,
    EvtReadFlagFadeIn = 4,
    EvtReadFlagNotDelData = 8,
    EvtReadFlagFadeOut = 16,
    EvtReadFlagPlPosNoSet = 32,
    EvtReadFlagNoFree = 64,
    EvtReadFlagSceEventStartTrue = 128,
    EvtReadFlagSubCharNoCtrl = 256,
    EvtReadFlagPlCheckEvent = 512,
    EvtReadFlagMissionFailed = 1024
};

// Event manager (game/event.cpp `EvtMgr`, 0x180 bytes): a cManager<Event> (game.cpp instantiates
// roomInit / arrayAlloc / arrayFree / dispWorkNum on it).
class EventMgr : public cManager<Event> {
public:
    union {
        u32 NowExeEvtKey;  // 0x34  first word of the running event name as a key (sce_com SceChapterEnd: IsAliveEvt / GetEvt)
        char NowExeEvtName[0x30];  // 0x34  name of the running event ("" = none)
    };
    EvtReadEm ReadWkTbl[8];   // 0x64  enemy modules loaded per read slot
    char NameTmp[0x20];    // 0x84  NameChange result
    u32 pUnit[0x20];         // 0xA4  cleared by myRoomInit
    u8 pad_124[0x144 - 0x124];
    void* EmWindowFcvTbl[3];  // 0x144  window jump motions (emwindow ExeWindowEvent)
    DatTbl EvdTbl;         // 0x150  event data by name (0x20)
    DatTbl BinTbl;         // 0x158  bin/tpl files by name (0x140)
    DatTbl FuncTbl;        // 0x160  Evt_*_Func handlers by name (0x10)
    DatTbl ReadTbl;        // 0x168  data units being read (0x8)
    u8 pad_170[0x180 - 0x170];

    EventMgr();
    virtual ~EventMgr();
#line 1107 "D:/Bio4/Prog/event.h"
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 0xD); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(Event* p, u32 size) { memclr_asm(p, size); }
    virtual int construct(Event* pSat, u32 room_no);

    int init();
    int myRoomInit();           // room start (game gameRoomInit, after arrayAlloc(2))
    int DelAll();
    int SetEvs(void* evs);      // room "EVS" data (game gameRoomInit)
    int Run();
    int IsAliveEvt(u32* pName, Event** ppEvt, int aliveEvtType);
    int EvtReadAram(char* name, int emId, int* pPtr, int blockType, u32 size);
    int EvtReadMram(char* name, int emId, int* pPtr, int blockType, u32 size);
    int NameCheck(char* name);
    // Copies the event file name into the manager (Ashley costume 1 swaps the 'r' of the room
    // name for 's'); returns the stored copy (the rooms hand it to DC.setData).
    char* NameChange(char* name);
    int EvtReadSub(char* name, int loadType, int emId, int* pPtr, int blockType, u32 size);
    int EvtReadExec(char* name, int emId, u32 evtReadFlag);   // flags: EvtReadFlag bits
    int EvtFree(char* name);
    void ToolCoreEvdDel();
    // Starts the loaded event data ("even" "t" header); `key` (optional) receives its key.
    int SetEvt(void* data, u32* key);
    int SetEvt(char* name, Event** out);
    int GetEvt(u32* pName, void** ppEvt);
    int DelEvt(void* evt, int a);
    int SetBin(char* name, void* data, void* dat2, int flag);
    // Looks a file of the running event up by name; 0 when it is not loaded.
    int GetBin(void** pAddr, const char* name, int flagGet);
    int DelBin(char* name);
    int SetEvd(char* name, void* data, void* dat2, int flag);
    int GetEvd(void** pAddr, char* name, int flagGet);
    int DelEvd(char* name);
    int SetFunc(char* name, void* func);
    int GetFunc(void** pAddr, char* name);
    int SetRead(char* name, int* wkNo, void* unit);
    int GetRead(void** pDat, int* pWkNo, char* name);
    int DelRead(char* name);
    int EvtSndStrStop(u32* pName, int noTar, int wait);
    void EvtSndStrPlay(u32* pName, int noTar, int noStr, int wait, f32 s_time);
    int GetZeroPartsWorldPos(cModel* pMod, Vec* pPos, Vec* pAng);
    void ClearEmWindowFcv();
    void SetEmWindowFcv(void* a, void* b, void* c);
    // Replaces the three window jump motions of the running event (emwindow ExeWindowEvent).
    void GetEmWindowFcv(void** win1FIn, void** win1FOut, void** win2FOut);
};

extern EventMgr EvtMgr;

// The running event's name key of the event manager, read through a helper: a plain scalar access, not a member chain.
static inline u32* evtKey(EventMgr* m) { return &m->NowExeEvtKey; }

// Event::StatusFlag bit test as a 0/1 value: an inline gives the `li 1; and.; bne; li 0` chain.
static inline int EvtStatusCk(Event* e, u32 bit) { int on = 1; if ((e->StatusFlag & bit) == 0) { on = 0; } return on; }
// The skip bit of StatusFlag (0x40000000) the same way.
static inline int EvtSkipCk(Event* e) { int skip = 1; if ((e->StatusFlag & EvtStfBit(EvtStfToolFrontExec)) == 0) { skip = 0; } return skip; }

// Event debug tool work (game/event.cpp `EvtDebug`, 0xE8 bytes).
class EventDebug {
public:
    u8 pad_0[0x60];
    char evName[0x30];     // 0x60  packet 6 name (EspToolSetDat)
    char camName[0x30];    // 0x90  packet 0xE name
    s32 mesCnt[3];         // 0xC0
    int NowStr[2];          // 0xCC  last stream number per block
    s32 StfStrTimer;           // 0xD4
    int NowCut;           // 0xD8
    s32 NumMod;            // 0xDC
    EvtDebugModel* PMod; // 0xE0  0x60 entries
    u32 FlagEtc;             // 0xE4  tool switches (bit19 fog off, bit20 focus off, bit18 lit off, bit21 mes off)

    EventDebug();
    ~EventDebug();
    int myRoomInit();          // room start (game gameRoomInit)
    char* getEvName() { return evName; }
    char* getCamName() { return camName; }
    void ClrModelFiles();
    int AddNameBinTpl(int npMod, char* pNameBin, char* pNameTpl);
};

extern EventDebug EvtDebug;

// game/event.cpp (C linkage): streamed sound blocks of the running event
extern "C" {
int SndStrPlayBlock(int blk, int no, f32 s_time);
void SndStrStopBlock(int sndId);
}

#endif
