// game/read: room / core / option / enemy / player / weapon data loading (D:/Bio4/Prog/read.cpp).
// All 21 functions byte-identical (OptionDataRead's objdiff rows are reloc-only). readEmData: `m`
// (14 refs / 198 insns, 2121) lost r29 to `newSize` (6 / 54, 2222) in global.c priority; the
// `else do { ... } while (0);` loop notes around the DLL_MALLOC clear give that `m` ref weight 2
// (15 refs -> 2272 > 2222) without adding an instruction, and that block has nothing the sched1
// barrier could reorder (the DLL_MALLOC / DATA_MALLOC sets and the tail stores do).
#include "types.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "scheduler.h"
#include "dvd.h"
#include "db_log.h"
#include "eprintf.h"
#include "player.h"
#include "read.h"
#include "ref_access.h"
#include <stdio.h>
#include <string.h>
#include <dolphin/os.h>
#include "sce.h"
#include "trans.h"
#include "sce_sys.h"
#include "yz2code.h"

extern "C" {
extern void* EmInitFunc;                  // game/em.cpp (set by the enemy dll prolog)
}



// game/game.cpp: GAME_WORK Game seen through this unit's own view struct (not global.h's declaration)
struct GameWork {
    u8 pad_0[0x18];
    void* omake_wep_addr;   // 0x18  weapon data buffer (Klauser)
    u8 pad_1C[0x1C - 0x1C];
};
extern GameWork Game;

// One loaded data module. The first 0x80 bytes are the DLL's bss area (DLL_Link gets `this`).

// File table entry: FileTbl index of the data and of the dll, extra flag.
struct ReadFile {
    u16 file;
    u16 dll;
    u32 flag;
};

// Archive header behind GetDataExt: count, three words, offsets, then 4-byte tags.
struct DataExtHeader {
    u32 num;      // 0x00
    u32 pad_4[3];
    u32 ofs[1];   // 0x10
};

extern "C" {
void decodeData();
static void* readEm(int id, void* addr, u32 size);
static int checkAshleyId(int id);
void ReadAreaData();
void CoreDataRead();
void OptionDataRead();
void InitModule(ReadModule* m);
int readEmData(ReadModule* m, int id, void* addr, u32 size);
void setEmModule(ReadModule* m, int id);
void EmReadInit();
void* EmReadSearch(int id, void* addr, u32 size);
ReadModule* pullEmModule();
void ReadPlayerData(int type, int costume);
void ReleasePlData();
void ReleaseWepData();
void ReadWepData(u32 no, u32 type);
void ContinueWepData();
}
void* GetDataExt(void* arc, const char* tag, int no);

static void* in_data_addr;
u32 out_data_size;
u8 oldWepId;

ReadModule EmReadModule[4] __attribute__((aligned(32)));
ReadModule PlReadModule __attribute__((aligned(32)));
ReadModule WepReadModule __attribute__((aligned(32)));

// HALT() (db_log.h) is a plain block, not do/while(0): the loop notes of a do/while are a sched1
// barrier, and the original's argument order around HALT (`lwz r4` / `addi r4,r31,__FILE__` before
// the string `lis/addi r3` in decodeData, the `mfcr` form in ReadPlayerData) needs one scheduling
// region.

#define DVD_READ(no, dst, a, b, c, mode) DvdRead(no, dst, a, b, c, mode, __FILE__, __LINE__)
#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

#define READ_BUFF_OFS 0x142800
#define ROOM_ARC_SIZE 0x300000
#define CORE_DATA_ADDR ((void*) 0x80578000)
#define CORE_DATA_MAX 0x234000
#define OPTION_DATA_ADDR ((void*) 0x807AC000)
#define OPTION_DATA_MAX 0x40000
#define PL_DATA_ADDR 0x807EC000
#define WEP_DATA_ADDR ((void*) 0x80904000)
#define WEP_DATA_MAX 0x70000
#define DLL_BSS_MAX 0x80

// Pointer store through a reference: the original reloads pG after every pG->pXxx = ... store.
// Flag test through a reference: the flag address is materialised, and `&PlReadModule` right
// after it becomes `addr - 0x82` (cse related-value).
static inline int BitChk16(u16& f, u16 b) { return f & b; }

// Replace the extension of a FileTbl name with "drs".
#define SET_DRS_NAME(p)         \
    while (*(p) != '.') {       \
        (p)++;                  \
    }                           \
    (p)[1] = 'd';               \
    (p)[2] = 'r';               \
    (p)[3] = 's'

#line 50 "D:/Bio4/Prog/read.cpp"
// iTask body of ReadAreaData: decompresses (Yz2) the room archive read into the top of the heap
// into a fresh pG->pRoom allocation sized to the output (at least the room budget ROOM_ARC_SIZE
// minus what the font already uses); HALTs when it would overlap the read buffer.
void decodeData()
{
    char buf[64];
    u32 used;

    out_data_size = Yz2DecodeSet((char*) in_data_addr, (void*) (MemGetHeapEndAddr(MemGetCurrentHeap()) - READ_BUFF_OFS));
#line 59 "D:/Bio4/Prog/read.cpp"
    (pG->pRoom = MEM_ALLOC(ROOM_ARC_SIZE, 1, 0xD));
    used = (u32) pG->pRoom - (u32) pG->pStFnt;
    Mem_free(pG->pRoom);
    if (out_data_size < ROOM_ARC_SIZE - used) {
        out_data_size = ROOM_ARC_SIZE - used;
    }
#line 71 "D:/Bio4/Prog/read.cpp"
    (pG->pRoom = MEM_ALLOC(out_data_size, 1, 0xD));
    if (pG->pRoom == NULL || (u32) pG->pRoom + out_data_size >= (u32) in_data_addr) {
        sprintf(buf, "r%03x.dat", pG->room_id);
        OSReport("-- %s DATA ENCODE ERROR!\n", buf);
        OSReport("-- %s DATA TOO LARGE!\n", buf);
        OSReport("--  pG->pRoom      = %x\n", pG->pRoom);
        OSReport("--  out_data_size  = %x\n", out_data_size);
        OSReport("--  read data addr = %x\n", in_data_addr);
#line 82 "D:/Bio4/Prog/read.cpp"
        HALT();
    }
    Yz2DecodeExec(pG->pRoom);
    iTaskExit();
}

// Loads the current room's archive "stX/rNNN.das" (skipped when System_flg 0x02000000 says it is
// already in memory), decompresses it on the iTask, and resolves the RTP (route points), MDT
// (messages), OSD and EMI sub-files into pG.
void ReadAreaData()
{
    char name[64];
    int req;
    u32 readTime;
    u32 decodeTime;

    sprintf(name, "st%x/r%03x.das", pG->stage_no, pG->room_id);
    if (SysFlagChk(pG, SYS_DATA_READ)) {
        SysFlagOff(pG, SYS_DATA_READ);
    } else {
        StopwatchStart();
#line 147 "D:/Bio4/Prog/read.cpp"
        req = DVD_READ_N(name, (void*) (MemGetHeapEndAddr(MemGetCurrentHeap()) - READ_BUFF_OFS), 0, 0, 0, 0x8120);
        while (Dvd.ReadCheck(req, 0, 0, &in_data_addr) != 1) {
            TaskSleep(1);
        }
        readTime = StopwatchStop(NULL);
        StopwatchStart();
        iTaskExec(decodeData);
        while (iTaskStatus() != 0) {
            TaskSleep(1);
        }
        decodeTime = StopwatchStop(NULL);
        OSReport("\n");
        OSReport("********** DataReadTime   %f\n", (f32) readTime / 1000.0f);
        OSReport("********** DataEncodeTime %f\n", (f32) decodeTime / 1000.0f);
        OSReport("\n");
        if (req < 0) {
            pLog->err(0, 0, "ReadAreaData(): DvdReadReqNAlloc error! R%03x", pG->room_id);
            return;
        }
    }
    pG->Rtp = GetDataExt(pG->pRoom, "RTP", 0);
    pG->RoomMes = GetDataExt(pG->pRoom, "MDT", 0);
    pG->pOsd = GetDataExt(pG->pRoom, "OSD", 0);
    pG->pEmi = (EmiData*) GetDataExt(pG->pRoom, "EMI", 0);
}

// Boot: reads the core archive (file 3) to CORE_DATA_ADDR (pG->pCore) and initialises the
// specular / illumination textures from it.
void CoreDataRead()
{
    DvdReadInfo info;
    int req;

    pG->pCore = (ArcFile*) CORE_DATA_ADDR;
#line 219 "D:/Bio4/Prog/read.cpp"
    req = DVD_READ(3, CORE_DATA_ADDR, 0, 0, 0, 0x8001);
    Dvd.ReadCheck(req, &info);
    SpecularInit((TEXPalette*) ARC_PTR(ofs_10), (TEXPalette*) ARC_PTR(ofs_44), (TEXPalette*) ARC_PTR(ofs_48),
                 (TEXPalette*) ARC_PTR(ofs_4C));
    GlobalIlmTexInit((TEXPalette*) ARC_PTR(ofs_40));
    if (info.size[0][0] > CORE_DATA_MAX) {
        pLog->err(0, 0, "CORE_DATA IS TOO LARGE(%d/%d)", 0, CORE_DATA_MAX);
        TaskSleep(60);
    }
}

// Boot: reads the language-dependent "SS/<lang>/option.dat" (the option menu id data) to
// OPTION_DATA_ADDR (pG->pOption).
void OptionDataRead()
{
    DvdReadInfo info;
    char name[64];
    const char* lang[12] = { "jpn", "eng", "eng", "ger", "fra", "esp", "ita", "eng" };
    int req;

    sprintf(name, "SS/%3s/option.dat", lang[pSys->language]);
    pG->pOption = OPTION_DATA_ADDR;
#line 262 "D:/Bio4/Prog/read.cpp"
    req = DVD_READ_N(name, OPTION_DATA_ADDR, 0, 0, 0, 0x11);
    Dvd.ReadCheck(req, &info);
    if (info.size[0][0] > OPTION_DATA_MAX) {
        pLog->err(0, 0, "OPTION_DAT IS TOO LARGE(%d/%d)", 0, OPTION_DATA_MAX);
        TaskSleep(60);
    }
}

// Frees a loaded module slot: unlinks the REL (flag bit1), frees the archive (bit2; debug heap
// when bit3) and the separately copied module (bit0), clears the slot.
void InitModule(ReadModule* m)
{
    if (m->pModule != NULL && m->ctrl_flag.check(MODULE_CTRL_DLL_LINK)) {
        DLL_Unlink(m->pModule);
    }
    if (m->pArc != NULL && m->ctrl_flag.check(MODULE_CTRL_DATA_MALLOC)) {
        if (m->ctrl_flag.check(MODULE_CTRL_DATA_DMALLOC)) {
            Debug_free(m->pArc);
        } else {
            Mem_free(m->pArc);
        }
    }
    if (m->pModule != NULL && m->ctrl_flag.check(MODULE_CTRL_DLL_MALLOC)) {
        Mem_free(m->pModule);
    }
    memclr_asm(m, sizeof(ReadModule));
}

// Loads enemy module `id` into a free slot (all display flags forced on during the load): reads
// the data / REL and links it. Returns the archive or NULL.
static void* readEm(int id, void* data_addr, u32 malloc_size)
{
    u32 flags = pG->Disp_flg;
    ReadModule* m;

    pG->Disp_flg = 0xFFFFFFFF;
    DpfFlagOff(pG, DPF_MESSAGE);
    m = pullEmModule();
    if (m == NULL) {
        return NULL;
    }
    if (readEmData(m, id, data_addr, malloc_size) == 0) {
        pG->Disp_flg = flags;
        return NULL;
    }
    setEmModule(m, id);
    pG->Disp_flg = flags;
    return m->pArc;
}

ReadFile EmFileTbl[64] = {
    { 0x06, 0x00, 0 }, { 0x06, 0x00, 0 }, { 0x79, 0x7A, 1 }, { 0x62, 0x63, 1 },
    { 0x49, 0x4A, 1 }, { 0x1B, 0x63, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x75, 0x76, 1 }, { 0x5D, 0x5E, 1 },
    { 0x18, 0x19, 1 }, { 0x1E, 0x1F, 1 }, { 0x2B, 0x2C, 1 }, { 0x38, 0x4F, 1 },
    { 0x39, 0x50, 1 }, { 0x2E, 0x2F, 1 }, { 0x30, 0x31, 1 }, { 0x32, 0x33, 1 },
    { 0x34, 0x35, 1 }, { 0x36, 0x37, 1 }, { 0x3A, 0x3B, 1 }, { 0x3C, 0x3D, 1 },
    { 0x9D, 0x9E, 1 }, { 0xAA, 0xAB, 1 }, { 0xB0, 0xB1, 1 }, { 0x04, 0x05, 1 },
    { 0x0C, 0x0D, 1 }, { 0x0E, 0x0F, 1 }, { 0x10, 0x11, 1 }, { 0x12, 0x13, 1 },
    { 0x14, 0x15, 1 }, { 0x16, 0x17, 1 }, { 0x20, 0x21, 1 }, { 0x25, 0x26, 1 },
    { 0x29, 0x2A, 1 }, { 0x3E, 0x3F, 1 }, { 0x40, 0x41, 1 }, { 0x43, 0x44, 1 },
    { 0x47, 0x48, 1 }, { 0x4B, 0x4C, 1 }, { 0x53, 0x54, 1 }, { 0x55, 0x56, 1 },
    { 0x57, 0x58, 1 }, { 0xA6, 0xA7, 1 }, { 0x96, 0x97, 1 }, { 0x18, 0x19, 1 },
    { 0x64, 0x65, 1 }, { 0x7B, 0x7C, 1 }, { 0xA3, 0xA4, 1 }, { 0x18, 0x19, 1 },
    { 0x99, 0x9A, 1 }, { 0x82, 0x83, 1 }, { 0xB8, 0xB9, 1 }, { 0x7F, 0x80, 1 },
    { 0x8C, 0x8D, 1 }, { 0x9B, 0x9C, 1 }, { 0xB6, 0xB7, 1 }, { 0x18, 0x19, 1 },
};
ReadFile EmFileTbl_Ada[64] = {
    { 0x06, 0x00, 0 }, { 0x06, 0x00, 0 }, { 0x79, 0x7A, 1 }, { 0x62, 0x63, 1 },
    { 0x49, 0x4A, 1 }, { 0x1B, 0x63, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x75, 0x76, 1 }, { 0x5D, 0x5E, 1 },
    { 0x18, 0x19, 1 }, { 0x1E, 0x1F, 1 }, { 0x2B, 0x2C, 1 }, { 0x38, 0x4F, 1 },
    { 0x39, 0x50, 1 }, { 0x2E, 0x2F, 1 }, { 0xE4, 0x31, 1 }, { 0x32, 0x33, 1 },
    { 0x34, 0x35, 1 }, { 0x36, 0x37, 1 }, { 0x3A, 0x3B, 1 }, { 0x3C, 0x3D, 1 },
    { 0xE5, 0x9E, 1 }, { 0xE6, 0xAB, 1 }, { 0xB0, 0xB1, 1 }, { 0xE7, 0x05, 1 },
    { 0xE8, 0x0D, 1 }, { 0x0E, 0x0F, 1 }, { 0x10, 0x11, 1 }, { 0x12, 0x13, 1 },
    { 0x14, 0x15, 1 }, { 0x16, 0x17, 1 }, { 0x20, 0x21, 1 }, { 0x25, 0x26, 1 },
    { 0x29, 0x2A, 1 }, { 0x3E, 0x3F, 1 }, { 0x40, 0x41, 1 }, { 0x43, 0x44, 1 },
    { 0x47, 0x48, 1 }, { 0x4B, 0x4C, 1 }, { 0x53, 0x54, 1 }, { 0x55, 0x56, 1 },
    { 0x57, 0x58, 1 }, { 0xA6, 0xA7, 1 }, { 0x96, 0x97, 1 }, { 0x18, 0x19, 1 },
    { 0x64, 0x65, 1 }, { 0x7B, 0x7C, 1 }, { 0xA3, 0xA4, 1 }, { 0x18, 0x19, 1 },
    { 0x99, 0x9A, 1 }, { 0x82, 0x83, 1 }, { 0xB8, 0xB9, 1 }, { 0x7F, 0x80, 1 },
    { 0x8C, 0x8D, 1 }, { 0x9B, 0x9C, 1 }, { 0xB6, 0xB7, 1 }, { 0x18, 0x19, 1 },
};
ReadFile EmFileTbl_Wesker[64] = {
    { 0x06, 0x00, 0 }, { 0x06, 0x00, 0 }, { 0x79, 0x7A, 1 }, { 0x62, 0x63, 1 },
    { 0x49, 0x4A, 1 }, { 0x1B, 0x63, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x75, 0x76, 1 }, { 0x5D, 0x5E, 1 },
    { 0x18, 0x19, 1 }, { 0x1E, 0x1F, 1 }, { 0x2B, 0x2C, 1 }, { 0x38, 0x4F, 1 },
    { 0x39, 0x50, 1 }, { 0x2E, 0x2F, 1 }, { 0xE9, 0x31, 1 }, { 0x32, 0x33, 1 },
    { 0x34, 0x35, 1 }, { 0x36, 0x37, 1 }, { 0x3A, 0x3B, 1 }, { 0x3C, 0x3D, 1 },
    { 0xEA, 0x9E, 1 }, { 0xEB, 0xAB, 1 }, { 0xB0, 0xB1, 1 }, { 0xEC, 0x05, 1 },
    { 0xED, 0x0D, 1 }, { 0x0E, 0x0F, 1 }, { 0x10, 0x11, 1 }, { 0x12, 0x13, 1 },
    { 0x14, 0x15, 1 }, { 0x16, 0x17, 1 }, { 0x20, 0x21, 1 }, { 0x25, 0x26, 1 },
    { 0x29, 0x2A, 1 }, { 0x3E, 0x3F, 1 }, { 0x40, 0x41, 1 }, { 0x43, 0x44, 1 },
    { 0x47, 0x48, 1 }, { 0x4B, 0x4C, 1 }, { 0x53, 0x54, 1 }, { 0x55, 0x56, 1 },
    { 0x57, 0x58, 1 }, { 0xA6, 0xA7, 1 }, { 0x96, 0x97, 1 }, { 0x18, 0x19, 1 },
    { 0x64, 0x65, 1 }, { 0x7B, 0x7C, 1 }, { 0xA3, 0xA4, 1 }, { 0x18, 0x19, 1 },
    { 0x99, 0x9A, 1 }, { 0x82, 0x83, 1 }, { 0xB8, 0xB9, 1 }, { 0x7F, 0x80, 1 },
    { 0x8C, 0x8D, 1 }, { 0x9B, 0x9C, 1 }, { 0xB6, 0xB7, 1 }, { 0x18, 0x19, 1 },
};
ReadFile EmFileTbl_Klauser[64] = {
    { 0x06, 0x00, 0 }, { 0x06, 0x00, 0 }, { 0x79, 0x7A, 1 }, { 0x62, 0x63, 1 },
    { 0x49, 0x4A, 1 }, { 0x1B, 0x63, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 },
    { 0x18, 0x19, 1 }, { 0x18, 0x19, 1 }, { 0x75, 0x76, 1 }, { 0x5D, 0x5E, 1 },
    { 0x18, 0x19, 1 }, { 0x1E, 0x1F, 1 }, { 0x2B, 0x2C, 1 }, { 0x38, 0x4F, 1 },
    { 0x39, 0x50, 1 }, { 0x2E, 0x2F, 1 }, { 0xEE, 0x31, 1 }, { 0x32, 0x33, 1 },
    { 0x34, 0x35, 1 }, { 0x36, 0x37, 1 }, { 0x3A, 0x3B, 1 }, { 0x3C, 0x3D, 1 },
    { 0xEF, 0x9E, 1 }, { 0xF0, 0xAB, 1 }, { 0xB0, 0xB1, 1 }, { 0xF1, 0x05, 1 },
    { 0xF2, 0x0D, 1 }, { 0x0E, 0x0F, 1 }, { 0x10, 0x11, 1 }, { 0x12, 0x13, 1 },
    { 0x14, 0x15, 1 }, { 0x16, 0x17, 1 }, { 0x20, 0x21, 1 }, { 0x25, 0x26, 1 },
    { 0x29, 0x2A, 1 }, { 0x3E, 0x3F, 1 }, { 0x40, 0x41, 1 }, { 0x43, 0x44, 1 },
    { 0x47, 0x48, 1 }, { 0x4B, 0x4C, 1 }, { 0x53, 0x54, 1 }, { 0x55, 0x56, 1 },
    { 0x57, 0x58, 1 }, { 0xA6, 0xA7, 1 }, { 0x96, 0x97, 1 }, { 0x18, 0x19, 1 },
    { 0x64, 0x65, 1 }, { 0x7B, 0x7C, 1 }, { 0xA3, 0xA4, 1 }, { 0x18, 0x19, 1 },
    { 0x99, 0x9A, 1 }, { 0x82, 0x83, 1 }, { 0xB8, 0xB9, 1 }, { 0x7F, 0x80, 1 },
    { 0x8C, 0x8D, 1 }, { 0x9B, 0x9C, 1 }, { 0xB6, 0xB7, 1 }, { 0x18, 0x19, 1 },
};

// Reads the enemy file for `id` from the per-character file table (EmFileTbl*; entries with a DLL
// become "em*.drs"): to `addr`, or to a new allocation (grown to `size` if smaller) when addr is
// NULL; the REL part after the data offset at +4 is copied out when it does not fit. Sleeps on the
// scenario task if inside it. Fills `m`; returns 1 on success.
int readEmData(ReadModule* m, int id, void* addr, u32 size)
{
    DvdReadInfo info;
    u32 len;
    ReadFile* e;
    char* name;
    int req;
    int ret;
    int mode;
    void* pArc;
    void* pModule = NULL;
    u32 bssSize = 0;
    u32 newSize;
    u32 dataSize;

    switch (pG->pl_type) {
    case 0:
    case 1:
    default:
        e = &EmFileTbl[id];
        break;
    case 3:
    case 5:
        e = &EmFileTbl_Wesker[id];
        break;
    case 2:
        e = &EmFileTbl_Ada[id];
        break;
    case 4:
        e = &EmFileTbl_Klauser[id];
        break;
    }
    if (e->file == 0) {
        return 0;
    }
    name = (char*) FileTbl[e->file].name;
    if (e->dll != 0) {
        SET_DRS_NAME(name);
        name = (char*) FileTbl[e->file].name;
    }
    if (Dvd.FileExistCheck(name, &len) < 0) {
        pLog->err(0, 0, "readEmData(): error! %s", name);
        return 0;
    }
    mode = 1;
    if (pG->Rno0 == 2) {
        mode = 0x100;
    }
    if (addr == NULL) {
#line 725 "D:/Bio4/Prog/read.cpp"
        req = DVD_READ_N(name, NULL, 0, 0, 0, mode | 0x8004);
    } else {
#line 728 "D:/Bio4/Prog/read.cpp"
        req = DVD_READ_N(name, addr, 0, 0, 0, mode | 0x8000);
    }
    while ((ret = Dvd.ReadCheck(req, &info)) != 1) {
        if (ret < 0) {
            pLog->err(0, 0, "readEmData(): error! %s", name);
            return 0;
        }
        if (SceSys.checkCTaskRange() == 1) {
            SceSys.m_init_loop_flag = 1;
            SceSleep(1);
            SceSys.m_init_loop_flag = 0;
        } else {
            TaskSleep(1);
        }
    }
    len = info.size[0][0];
    if (addr == NULL) {
        m->ctrl_flag.on(MODULE_CTRL_DATA_MALLOC);
        pArc = (void*) info.addr[0][0];
        if (len < size) {
            void* old = pArc;
            newSize = size;
            Mem_free(old);
#line 776 "D:/Bio4/Prog/read.cpp"
            pArc = MEM_ALLOC(newSize, 1, 0xD);
            if (pArc != old) {
                return 0;
            }
        } else {
            newSize = len;
        }
    } else {
        m->ctrl_flag.off(MODULE_CTRL_DATA_MALLOC);
        pArc = addr;
        newSize = len;
    }
    if (e->dll != 0) {
        pModule = (void*) (*(u32*) ((u8*) pArc + 4) + (u32) pArc);
        dataSize = (u32) pModule - (u32) pArc;
        bssSize = len - dataSize;
        if (addr == NULL && dataSize < size) {
            void* old = pModule;
#line 814 "D:/Bio4/Prog/read.cpp"
            pModule = MEM_ALLOC(bssSize, 1, 0xD);
            memcpy(pModule, old, bssSize);
            m->ctrl_flag.on(MODULE_CTRL_DLL_MALLOC);
        } else do {  // loop notes: `m` weighted ref 15 > newSize in global-alloc (m r29, newSize r28)
            newSize = dataSize;
            m->ctrl_flag.off(MODULE_CTRL_DLL_MALLOC);
        } while (0);
    }
    m->id = id;
    m->pArc = pArc;
    m->size = newSize;
    m->pModule = (OSModuleHeader*) pModule;
    m->bssSize = bssSize;
    return 1;
}

// Links the module's REL (once, flag bit1; hangs with "BSS SIZE OVER" when its bss exceeds
// DLL_BSS_MAX), runs its prolog and takes the EmInitFunc it registered.
void setEmModule(ReadModule* m, int id)
{
    ReadFile* e;
    void* bss;

    switch (pG->pl_type) {
    case 0:
    default:
        e = &EmFileTbl[id];
        break;
    case 2:
        e = &EmFileTbl_Ada[id];
        break;
    case 4:
        e = &EmFileTbl_Klauser[id];
        break;
    }
    if (m->pModule != NULL) {
        if (!m->ctrl_flag.check(MODULE_CTRL_DLL_LINK)) {
            bss = NULL;
            if (m->pModule->bssSize != 0) {
                bss = m;
            }
            if (m->pModule->bssSize > DLL_BSS_MAX) {
                for (;;) {
                    eprintf(100, 100, 0, 0, "BSS SIZE OVER!!!");
                    if (e->dll != 0) {
                        eprintf(100, 116, 0, 0, " %s", e->dll);
                    } else {
                        eprintf(100, 116, 0, 0, " %s", FileTbl[e->file].name);
                    }
                    TaskSleep(1);
                }
            }
            DLL_Link(m->pModule, bss);
            m->ctrl_flag.on(MODULE_CTRL_DLL_LINK);
        }
        DLL_PROLOG(m->pModule)();
        m->pInitFunc = EmInitFunc;
    } else {
        m->pModule = NULL;
        m->ctrl_flag.off(MODULE_CTRL_DLL_LINK);
        m->pInitFunc = NULL;
    }
}

// Frees the four enemy module slots (stage / game start).
void EmReadInit()
{
    int i;

    for (i = 0; i < 4; i++) {
        InitModule(&EmReadModule[i]);
    }
}

// Ashley's module: id 3 normal, 5 in the alternate costume (game_costume).
static int checkAshleyId(int id)
{
    if (id == 3 || id == 5) {
        if (pG->game_costume == 0) {
            id = 3;
        } else {
            id = 5;
        }
    }
    return id;
}

// The archive of enemy module `id`: the loaded slot (its init function becomes EmInitFunc), or
// loads it (readEm).
void* EmReadSearch(int id, void* data_addr, u32 malloc_size)
{
    ReadModule* m;

    id = checkAshleyId(id);
    m = SearchEmModule(id);
    if (m != NULL) {
        EmInitFunc = m->pInitFunc;
        return m->pArc;
    }
    return readEm(id, data_addr, malloc_size);
}

// The loaded slot for enemy module `id`, or NULL.
ReadModule* SearchEmModule(int id)
{
    ReadModule* m;
    int i;

    id = checkAshleyId(id);
    for (m = EmReadModule, i = 0; i < 4; i++, m++) {
        if (m->id == (u16) id) {
            return m;
        }
    }
    return NULL;
}

// A free enemy module slot (pArc NULL), or NULL when all four are used.
ReadModule* pullEmModule()
{
    int i;

    for (i = 0; i < 4; i++) {
        if (EmReadModule[i].pArc == NULL) {
            return &EmReadModule[i];
        }
    }
    return NULL;
}

// Reloads the player archive (pG->pPlayer at PL_DATA_ADDR) when pl_flag bit0 asks for it: the file
// by character `type` and `costume` (Leon: 6 / 0x5A / 0x5B / 0x79; Ashley 0x22 / 0xBE; Ada 0xA8 /
// 0xB4 + DLL; HUNK / Krauser / Wesker with their DLLs), links the character REL when there is one.
void ReadPlayerData(int type, int costume)
{
    DvdReadInfo info;
    int req;
    int ret;
    int file;
    char* name;
    int dll;
    u8* data;
    u32 total;
    u32 max;
    u32 size;
    u32 dataSize;
    u32 bssSize;
    void* pArc;
    OSModuleHeader* pModule;
    void* bss;

    if (!(pG->pl_flag & 1)) {
        return;
    }
    pG->pl_flag &= ~1;
    ReleasePlData();
    pG->pPlayer = (PlArc*) PL_DATA_ADDR;
    data = (u8*) PL_DATA_ADDR;
    dll = 0;
    switch (type) {
    case 0:
    default:
        switch (costume) {
        default:
            file = 6;
            break;
        case 1:
            file = 0x5A;
            break;
        case 2:
            file = 0x5B;
            break;
        case 3:
            file = 0x79;
            break;
        }
        dll = 0;
        break;
    case 1:
        if (costume != 1) {
            file = 0x22;
        } else {
            file = 0xBE;
        }
        break;
    case 2:
        if (costume != 1) {
            file = 0xA8;
        } else {
            file = 0xB4;
        }
        dll = 0x1A;
        break;
    case 3:
        file = 0xBC;
        dll = 0xBD;
        break;
    case 4:
        file = 0xC5;
        dll = 0xC6;
        break;
    case 5:
        file = 0xC7;
        dll = 0xC8;
        break;
    }
    name = (char*) FileTbl[file].name;
    SET_DRS_NAME(name);
#line 1124 "D:/Bio4/Prog/read.cpp"
    req = DVD_READ_N(FileTbl[file].name, (void*) PL_DATA_ADDR, 0, 0, 0, 0x8100);
    while ((ret = Dvd.ReadCheck(req, &info)) != 1) {
        if (ret < 0) {
            pLog->err(0, 0, "ReadPlayerData(): error! %s", FileTbl[file].name);
#line 1139 "D:/Bio4/Prog/read.cpp"
            HALT();
        }
        TaskSleep(1);
    }
    total = info.size[0][0] + info.size[0][1];
    if (type == 0) {
        max = 0x118000;
    } else {
        max = 0x188000;
    }
    if (total >= max) {
        pLog->err(0, 0, "ERROR: PLAYER_DATA IS TOO LARGE");
    }
    size = total;
    bssSize = 0;
    pArc = NULL;
    pModule = NULL;
    if (dll != 0) {
        ReleasePlData();
        pArc = (void*) PL_DATA_ADDR;
        pModule = (OSModuleHeader*) (*(u32*) (data + 4) + (u32) data);
        dataSize = (u32) pModule - (u32) data;
        bssSize = size - dataSize;
        size = dataSize;
        if (!PlReadModule.ctrl_flag.check(MODULE_CTRL_DLL_LINK)) {
            bss = NULL;
            if (pModule->bssSize != 0) {
                bss = &PlReadModule;
            }
            if (pModule->bssSize > DLL_BSS_MAX) {
                for (;;) {
                    eprintf(100, 100, 0, 0, "PL_DLL BSS SIZE OVER!!!");
                    TaskSleep(1);
                }
            }
            PlReadModule.ctrl_flag.on(MODULE_CTRL_DLL_LINK);
            DLL_Link(pModule, bss);
            DLL_PROLOG(pModule)();
        } else {
            pModule = NULL;
        }
    }
    PlReadModule.id = file;
    PlReadModule.pArc = pArc;
    PlReadModule.size = size;
    PlReadModule.pModule = pModule;
    PlReadModule.bssSize = bssSize;
}

// Frees the player module slot.
void ReleasePlData()
{
    if (PlReadModule.pArc != NULL) {
        InitModule(&PlReadModule);
        PlReadModule.pArc = NULL;
    }
}

// Frees the weapon module slot and forgets the loaded weapon (weapon_no_old = 0xFF).
void ReleaseWepData()
{
    if (pG->weapon_no_old != 0xFF) {
        InitModule(&WepReadModule);
        pG->weapon_no_old = 0xFF;
        oldWepId = 0xFF;
    }
}

ReadFile wep_data_leon[46] = {
    { 0xA1, 0xA2, 0 }, { 0x07, 0x08, 0 }, { 0x09, 0x0A, 0 }, { 0x81, 0x0A, 0 },
    { 0x8A, 0x8B, 0 }, { 0x86, 0x87, 0 }, { 0x88, 0x89, 0 }, { 0x45, 0x46, 0 },
    { 0x8E, 0x8F, 0 }, { 0x7D, 0x7E, 0 }, { 0x6F, 0x70, 0 }, { 0x6D, 0x6E, 0 },
    { 0x71, 0x72, 0 }, { 0x73, 0x74, 0 }, { 0x23, 0x24, 0 }, { 0x84, 0x85, 0 },
    { 0x77, 0x78, 0 }, { 0x90, 0x91, 0 }, { 0xAC, 0x0A, 0 }, { 0x66, 0x67, 0 },
    { 0xAD, 0x6E, 0 }, { 0xF4, 0x7E, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0xF5, 0x7E, 0 }, { 0xF8, 0x24, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0xFB, 0x74, 0 }, { 0xF6, 0x70, 0 },
    { 0xF7, 0x70, 0 }, { 0xAE, 0xAF, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
};
ReadFile wep_data_ada[46] = {
    { 0xCC, 0xCD, 0 }, { 0xD4, 0xD5, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0xD8, 0xD9, 0 }, { 0xD6, 0xD7, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0xB2, 0xB3, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
};
static ReadFile wep_data_hunk[46] = {
    { 0xCE, 0xCF, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0xDA, 0xDB, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0xDC, 0xDD, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
};
static ReadFile wep_data_wesker[46] = {
    { 0xD2, 0xD3, 0 }, { 0x00, 0x00, 0 }, { 0xE0, 0xE1, 0 }, { 0x00, 0x00, 0 },
    { 0xF9, 0xFA, 0 }, { 0xFC, 0xFD, 0 }, { 0xE2, 0xE3, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
};
ReadFile wep_data_klauser[46] = {
    { 0xD0, 0xD1, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0xDE, 0xDF, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0xC3, 0xC4, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
    { 0x00, 0x00, 0 }, { 0x00, 0x00, 0 },
};

// Loads the weapon module for weapon `no` / variant `type`: maps the variants onto module numbers
// (grenades / eggs -> 0x13, the special shotgun / rifles / launcher variants) in the per-character
// table, skips when it is already loaded (oldWepId), reads it to WEP_DATA_ADDR (Game.pWepBuf for
// Krauser), links the REL and runs its prolog (which registers WeaponInitFunc). pG->pWep = data.
void ReadWepData(u32 no, u32 type)
{
    DvdReadInfo info;
    int req;
    int ret;
    u8* data;
    char* name;
    ReadFile* e;
    u32 total;
    u32 size;
    u32 bssSize;
    OSModuleHeader* pModule;
    void* bss;

    if (pG->pl_type == 4) {
        data = (u8*) Game.omake_wep_addr;
    } else {
        data = (u8*) WEP_DATA_ADDR;
    }
    switch (pG->pl_type) {
    case 0:
    default:
        if (no == 0x19 || no == 0x1F || no == 0x20 || no == 0x16 || no == 0x17) {
            no = 0x13;
        } else if (no == 3 && type == 2) {
            no = 0x12;
        } else if (no == 0xB && type > 1) {
            no = 0x14;
        } else if (no == 9) {
            switch (type) {
            case 1:
                no = 0x15;
                break;
            case 2:
                no = 0x18;
                break;
            }
        } else if (no == 0xA) {
            switch (type) {
            case 1:
                no = 0x1F;
                break;
            case 2:
                no = 0x20;
                break;
            }
        } else if (no == 0xE && type == 1) {
            no = 0x19;
        } else if (no == 0xD && type == 2) {
            no = 0x1E;
        }
        e = &wep_data_leon[no];
        break;
    case 2:
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1F:
        case 0x20:
            no = 0x13;
            break;
        }
        e = &wep_data_ada[no];
        break;
    case 3:
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1F:
        case 0x20:
            no = 0x13;
            break;
        }
        e = &wep_data_hunk[no];
        break;
    case 5:
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1F:
        case 0x20:
            no = 4;
            break;
        case 0x0A:
            no = 5;
            break;
        }
        e = &wep_data_wesker[no];
        break;
    case 4:
        switch (no) {
        case 0x13:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1F:
        case 0x20:
            no = 0x17;
            break;
        }
        e = &wep_data_klauser[no];
        break;
    }
    if (e->file == 0) {
        return;
    }
    if (oldWepId != 0xFF && no == oldWepId) {
        return;
    }
    oldWepId = no;
    ReleaseWepData();
    pG->weapon_no_old = pG->weapon_no;
    if (e->dll != 0) {
        name = (char*) FileTbl[e->file].name;
        SET_DRS_NAME(name);
    }
#line 1538 "D:/Bio4/Prog/read.cpp"
    req = DVD_READ_N(FileTbl[e->file].name, data, 0, 0, 0, 0x8001);
    while ((ret = Dvd.ReadCheck(req, &info)) != 1) {
        if (ret < 0) {
            pLog->err(0, 0, "ReadWepData() DATA LOAD FAILED");
#line 1547 "D:/Bio4/Prog/read.cpp"
            HALT();
        }
        TaskSleep(1);
    }
    total = info.size[0][0] + info.size[0][1];
    if (total > WEP_DATA_MAX) {
        pLog->err(0, 0, "WEAPON_DATA IS TOO LARGE (DATA)");
#line 1560 "D:/Bio4/Prog/read.cpp"
        HALT();
    }
    pG->pWep = (PlArc*) info.addr[0][0];
    pModule = (OSModuleHeader*) (*(u32*) (data + 4) + (u32) data);
    size = (u32) pModule - (u32) data;
    bssSize = total - size;
    if (!WepReadModule.ctrl_flag.check(MODULE_CTRL_DLL_LINK)) {
        bss = NULL;
        if (pModule->bssSize != 0) {
            bss = &WepReadModule;
        }
        if (pModule->bssSize > DLL_BSS_MAX) {
            for (;;) {
                eprintf(100, 100, 0, 0, "BSS SIZE OVER!!!");
                TaskSleep(1);
            }
        }
        WepReadModule.ctrl_flag.on(MODULE_CTRL_DLL_LINK);
        DLL_Link(pModule, bss);
        DLL_PROLOG(pModule)();
    } else {
        pModule = NULL;
    }
    WepReadModule.bssSize = bssSize;
    WepReadModule.id = no;
    WepReadModule.size = size;
    WepReadModule.pModule = pModule;
    WepReadModule.pArc = data;
    pG->pWep = (PlArc*) data;
}

// After a continue: if the weapon changed since the module was loaded, reloads the weapon.
void ContinueWepData()
{
    u8 old;
    u8 wep;

    if (pG->pl_type != 1) {
        old = pG->weapon_no_old;
        wep = pG->weapon_no;
        if (wep != old) {
            pG->weapon_no = old;
            pPL->weaponRelease();
            pPL->weaponLoad(wep, pG->weapon_type);
            pPL->weaponInit();
        }
    }
}

// The `no`-th sub-file with the three-letter `tag` ("RTP", "MDT", ...) in a tagged archive
// (offset table + tag table); NULL when missing.
void* GetDataExt(void* pData, const char* pName, int no)
{
    DataExtHeader* h = (DataExtHeader*) pData;
    u32 num;
    u32 i;
    int cnt;
    u8* p;

    if (pData == NULL || pName == NULL) {
        pLog->err(0, 0, "GetDataExt() NULL POINTER %08x %08x", pData, pName);
        return NULL;
    }
    num = h->num;
    cnt = 0;
    // tag table follows the offsets; p starts one entry early, the loop pre-increments (lbzu)
    p = (u8*) pData + num * 4;
    p += 0xC;
    for (i = 0; i < num; i++) {
        p += 4;
        if (p[0] == pName[0] && p[1] == pName[1] && p[2] == pName[2]) {
            if (cnt == no) {
                return (void*) (*(u32*) (i * 4 + (u32) pData + 0x10) + (u32) pData);
            }
            cnt++;
        }
    }
    return NULL;
}

asm(".section .bss; .balign 32");
