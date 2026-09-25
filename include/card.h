#ifndef CARD_H
#define CARD_H

#include "types.h"
#include "db_log.h"
#include "cDataSwap.h"
#include "main_mem.h"

// Memory card save/load front end (game/card.cpp).

// Dolphin CARD SDK (the SDK header drags in the CodeWarrior libc).
struct CardFileInfo {
    s32 chan;    // 0x00
    s32 fileNo;  // 0x04
    s32 offset;  // 0x08
    s32 length;  // 0x0C
    u16 iBlock;  // 0x10
    u16 pad_12;
};

struct CardStat {
    char fileName[32];   // 0x00
    u32 length;          // 0x20
    u32 time;            // 0x24
    u8 gameName[4];      // 0x28
    u8 company[2];       // 0x2C
    u8 bannerFormat;     // 0x2E
    u8 pad_2F;
    u32 iconAddr;        // 0x30
    u16 iconFormat;      // 0x34
    u16 iconSpeed;       // 0x36
    u32 commentAddr;     // 0x38
    u32 offsetBanner;    // 0x3C
    u32 offsetBannerTlut;// 0x40
    u32 offsetIcon[8];   // 0x44
    u32 offsetIconTlut;  // 0x64
    u32 offsetData;      // 0x68
};                       // 0x6C

typedef void (*CardCallback)(s32 chan, s32 result);

extern "C" {
void CARDInit();
s32 CARDGetResultCode(s32 chan);
s32 CARDCheckAsync(s32 chan, CardCallback callback);
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed);
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CardFileInfo* fileInfo, CardCallback callback);
s32 CARDDeleteAsync(s32 chan, const char* fileName, CardCallback callback);
s32 CARDFormatAsync(s32 chan, CardCallback callback);
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);
s32 CARDMountAsync(s32 chan, void* workArea, CardCallback detachCallback, CardCallback attachCallback);
s32 CARDUnmount(s32 chan);
s32 CARDGetSerialNo(s32 chan, u64* serialNo);
s32 CARDOpen(s32 chan, const char* fileName, CardFileInfo* fileInfo);
s32 CARDClose(CardFileInfo* fileInfo);
s32 CARDReadAsync(CardFileInfo* fileInfo, void* buf, s32 length, s32 offset, CardCallback callback);
s32 CARDWriteAsync(CardFileInfo* fileInfo, void* buf, s32 length, s32 offset, CardCallback callback);
s32 CARDGetStatus(s32 chan, s32 fileNo, CardStat* stat);
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CardStat* stat, CardCallback callback);
}

// One memory card slot (chan 0 = slot A, 1 = slot B, 2 = the host "HARD DISK" of the dev kit).
struct CardSlot {
    void* workArea;         // 0x00  CARDMountAsync work area (0xA000, slot A only)
    s32 chan;               // 0x04
    u32 flags;              // 0x08  error bits (2 no card, 4 no space, 0x10 broken, 0x20 wrong device,
                            //       0x40 fatal, 0x80 sector size, 0x100 save file seen, 0x200 system
                            //       file seen, 0x400/0x800 which file lacks space)
    s32 memSize;            // 0x0C  Mbit
    s32 sectorSize;         // 0x10
    s32 freeBytes;          // 0x14
    s32 freeFiles;          // 0x18
    u32 fileFlag[20];       // 0x1C  per save file: 1 exists, 2 corrupted, 4 wrong version
    u32 pad_6C;
    u64 serial;             // 0x70
    CardFileInfo fileInfo;  // 0x78
    CardStat stat;          // 0x8C
};                          // 0xF8

// Stream slot saved across the card screen.
struct CardStr {
    u32 id;      // 0x00
    s8 vol;      // 0x04
    u8 pad_5[3];
};

// Card screen message (PS2 CARD_MES_NO): cCard::cardMesSet `mes_no`, errorDisp mesNo.
enum CARD_MES_NO {
    MES_NO_CARD = 0,
    MES_DATA_SAVE_FAILURE = 1,
    MES_NO_CAPA = 2,
    MES_CARD_BROKEN = 3,
    MES_WRONG_DEVICE = 4,
    MES_NEED_FORMAT = 5,
    MES_NEED_FORMAT_LOAD = 6,
    MES_FORMAT_CONFIRM = 7,
    MES_SYSFILE_LOADING = 8,
    MES_FORMATTING = 9,
    MES_FORMAT_DONE = 10,
    MES_SAVING = 11,
    MES_SAVE_DONE = 12,
    MES_LOADING = 13,
    MES_LOAD_DONE = 14,
    MES_SAVE_CONFIRM = 15,
    MES_SAVEFILE_CONFIRM = 16,
    MES_LOAD_CONFIRM = 17,
    MES_LOADFILE_CONFIRM = 18,
    MES_OVERWRITE_CONFIRM = 19,
    MES_DATALOAD_FAILURE = 20,
    MES_FORMAT_FAILED = 21,
    MES_DATA_DELETE = 22,
    MES_NO_CAPA_START = 23,
    MES_NO_CARD_START = 24,
    MES_CARD_BROKEN_START = 25,
    MES_WRONG_DEVICE_START = 26,
    MES_NEED_FORMAT_START = 27,
    MES_NO_SYSFILE = 28,
    MES_SYS_DELETE = 29,
    MES_DELETING = 30,
    MES_DELETE_DONE = 31,
    MES_DELETE_CANCEL = 32,
    MES_FORMAT_CANCEL = 33,
    MES_SYSFILE_NOT_SAVE = 34,
    MES_SYSFILE_NOT_SAVE_START = 35,
    MES_DATFILE_NOT_SAVE = 36,
    MES_SYS_LOAD_FAILURE = 37,
    MES_CARD_ACCESS = 38,
    MES_SYS_SAVING = 39,
    MES_SYS_SAVE_DONE = 40,
    MES_SYS_SAVE_FAILURE = 41,
    MES_NO_CAPA_SYSFILE = 42,
    MES_SYS_SAVE_CONFIRM = 43,
    MES_SYS_MAKE_CONFIRM = 44,
    MES_NOT_MAKE_NEW_FILE = 45,
    MES_LANG_SELECT = 46,
    MES_CARD_ACCESS2 = 47,
    MES_NEED_FORMAT_SAVE = 48,
    MES_FORMATTING_SAVE = 49,
    MES_DIFF_SCENARIO_LOAD = 50,
    MES_DIFF_SCENARIO_SAVE = 51
};

class cCard {
private:
    u32 m_NeedMemSize;      // 0x000  heap range parked in `swap`
    u8 m_Rno0;             // 0x004  state (row of the MainLoop table)
    u8 m_Rno1;             // 0x005
    u8 m_Rno2;              // 0x006  async sub-step of the CARD helpers
    u8 m_Rno3;             // 0x007
    u32 m_SPFbak;    // 0x008  pG->flags_170 while the card screen runs
    u32 m_DPFbak;     // 0x00C  pG->flags_58
    u32 m_Status;            // 0x010  bit 0: file written, bit 1: file list changed
    s32 m_aMode;            // 0x014  0 load, 1 save, 2 first check
    s32 m_mMode;        // 0x018  saveMain: 1 = writing the system file
    u8 m_SlotNo;             // 0x01C
    s8 m_SaveNo;           // 0x01D  0..19
    u8 pad_1E;
    u8 m_RetryCtr;            // 0x01F
    s32 m_ErrCode;         // 0x020  CARD result / -0x2xx game error shown by errorDisp
    u8 pad_24[4];
    CardSlot m_Slot[3];   // 0x028
    u8* pSaveBuf;        // 0x310
    u32 saveBufSize;     // 0x314  0xEAFC
    u32 m_SaveSize;      // 0x318  8
    u8* m_pInfoAddr;        // 0x31C  20 * 0x200 save headers
    u8* m_pSaveInfo[20];       // 0x320
    u8* pSysBuf;         // 0x370
    u32 sysBufSize;      // 0x374  0x1E7C
    u32 m_SysSize;       // 0x378  1
    struct CardArc* pSubData;  // 0x37C  sub screen data archive (SndMem.sub_adr)
    void* m_IdDataAddr;       // 0x380  ss/cmn/save_?.dat
    s32 m_Timer;           // 0x384
    s32 m_StrTimer;        // 0x388
    s32 m_ResultCode;          // 0x38C  last CARD result code
    CardStr m_StrInfo[4];      // 0x390
    u32 m_SndId;        // 0x3B0
    s32 formatted;       // 0x3B4
    s32 exitFlag;        // 0x3B8
    s32 dispFlag;        // 0x3BC
    char m_Name[0x40]; // 0x3C0
    cDataSwap m_DataSwap;      // 0x400
    s32 m_Width_bak;        // 0x418
    u32 sysFlags;        // 0x41C
                         // 0x420

public:
    cCard();
    ~cCard();
    void slotSelect();
    void inSlotCheck();
    void dataSelect();
    void loadMain();
    void makeSaveData();
    void makeSystemSaveData();
    void saveMain();
    void exit();
    void workDestroy();
    void format();
    void fileDelete();
    void errorDisp();
    void errorSet(int code);
    int initialize(int type);
    int initSub();
    int workAlloc();
    u32 getUseMemSize();
    int fileCreate(u8* sub, int blocks, CardSlot* s);
    void makeCardStatus(CardSlot* s);
    void firstCheck00();
    void firstCheck10();
    void firstCheck20();
    void firstCheck30();
    void MainLoop(int arg);
    int existCheck(int slot, CardSlot* s);
    int mount(u8* Rno, CardSlot* s);
    int unmount(int slot);
    int verifyCheck(u8* Rno, CardSlot* s);
    int freeCheck(u8* Rno, CardSlot* s);
    int fileOpen(CardSlot* s);
    int fileClose(CardSlot* s);
    int saveFileCheck(u8* Rno, CardSlot* s);
    int systemFileCheck(u8* Rno, CardSlot* s);
    int fileRead(u8* Rno, void* addr, s32 size, s32 offset, CardSlot* s);
    int fileWrite(u8* Rno, void* addr, int wblock, CardSlot* s);
    int sysfileRead(u8* Rno0, u8* Rno1, int err_set);
    void createSysfile();
    void screenTrans();
    void cardMesSet(int mes_no, int wk_no, u32 attr);   // no: CARD_MES_NO
    void calcTplAddr(struct TEXPalette* tpl);
    void setMsgWindow(int a, int sw);
    bool ckStatus(u32 bit)
    {
        bool on = true;

        if ((m_Status & bit) == 0) {
            on = false;
        }
        return on;
    }
    s8 getSaveNo() { return m_SaveNo; }
    u8 getSlotNo() { return m_SlotNo; }
    CardSlot* getSlotInfo(int no) { return &m_Slot[no]; }
    u8* getSaveInfo(int no) { return m_pSaveInfo[no]; }
    u8 getRno0() { return m_Rno0; }
    void setStatus(u32 bit) { m_Status |= bit; }
    void resetStatus(u32 bit) { m_Status &= ~bit; }

#line 386 "D:/Bio4/Prog/card.h"
    void* operator new(unsigned int size) { return MEM_CALLOC(size, 1, 13); }
    void operator delete(void* p) { Mem_free(p); }
};

extern "C" {
void CardFirstCheck();
int CardCheckDone();
void CardSave(int terminal_no, int attr);
int CardLoad();
void CardSysSave();
void CardInit();
void CardDbgCacheSet();
void CardMainTask(int mode);
}

// pSys->language == n (0 jpn, 1 eng(US), 2 eng(EU), 3 ger, 4 fra, 5 esp, 6 ita, 7 eng) (card, option).
static inline int isLang(u8 lang, int n)
{
    return lang == n;
}

// One of the five European languages (2..6).
static inline int isEurope(u8 lang)
{
    if (isLang(lang, 2) || isLang(lang, 3) || isLang(lang, 4) || isLang(lang, 5) || isLang(lang, 6)) {
        return 1;
    }
    return 0;
}

#endif
