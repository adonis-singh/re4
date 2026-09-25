// game/card.cpp: the memory card save / load screen and the boot-time card check. cCard runs a
// state machine (MainLoop) per mode - load, save (game file or the system / options file) and
// first check - over the async CARD SDK steps (probe, mount, check, free space, open, read,
// write, create, delete, format), builds and verifies the 20 save files ("bh4_data%02d", CRC-32
// protected) and shows the messages / errors; CardID drives the screen's id sprites (file list,
// cursor). The dev kit's host disk is slot 2.
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "id_sys.h"
#include "mes.h"
#include "main.h"
#include "main_sub.h"
#include "scheduler.h"
#include "snd.h"
#include "cockpit.h"
#include "sscrn.h"
#include "sce.h"
#include "game.h"
#include "merchant.h"
#include "dvd.h"
#include "file.h"
#include "fade.h"
#include "eprintf.h"
#include "tpl.h"
#include "path.h"
#include "hermite.h"
#include "room_data.h"
#include <stdio.h>
#include <string.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os/OSReset.h>
#include <dolphin/os.h>
#include <dolphin/db.h>

extern "C" {
void debugInfoDisp(int slot, int type);
void CRCInit();
u32 CRCCalc(u8* data, u32 len);
int CRCVerify(u8* data, u32 len, u32 saved);
void setMsgBG(int a, int flag);
}

// Sub screen data archive (SndMem.sub_adr): offsets to its sub-files.
// Archive header shared by the sub screen sound data (SndMem.sub_adr: [0] icon/banner TPL,
// [1] message table) and ss/cmn/save_?.dat (CardID textures, save/load frames, file list, ...).
struct CardArc {
    u32 pad_0[4];
    u32 ofs[6];      // 0x10  offsets from the archive start
};

// Pointers of the game save block (pSaveData).
struct SaveDataPtrs {
    u8 pad_0[8];
    u8* p8;    // 0x08
    u8* pC;    // 0x0C
    u8* p10;   // 0x10  room save records (RoomData.num * 0xD8 + 0x10)
    u8* p14;   // 0x14  sub screen data
    u8* p18;   // 0x18  merchant data
};
#define SD ((SaveDataPtrs*) pSaveData)

// Save file record header: the first 0x200 bytes at SAVE_HDR are kept per file (pInfo).
struct SaveInfo {
    u32 crc;              // 0x00  of the 0x1FC bytes after it
    u32 magic;            // 0x04  0x116
    OSCalendarTime time;  // 0x08
    u32 mode;             // 0x30  1 normal, 2 (x8 & 0x20), 3 (x8 & 0x40)
    u8 pad_34[9];
    u8 game_mode_disp;    // 0x3D  pG->game_mode at save time (makeSaveData: buf[SAVE_HDR_X3D] = game_mode) (PS2 SAVE_WORK game_mode_disp)
    u8 chapter;           // 0x3E
    u8 pad_3F;
    u16 save_cnt;         // 0x40  pG->save_cnt (GameSaveBlock + 0xC) (PS2 SAVE_WORK save_cnt)
    u16 count;            // 0x42
    u8 pad_44[4];
    u32 playTime;         // 0x48
    u8 pad_4C[4];
    u16 room;             // 0x50
};

struct SaveHeader {
    u32 crc;              // 0x00
    u32 magic;            // 0x04
    OSCalendarTime time;  // 0x08
    u32 mode;             // 0x30
};

struct MesPos {
    u8 x;
    u8 y;
    u16 no;
};

#define ICON_NUM 1

// Save file image (0xEAFC bytes) and system file image (0x1E7C bytes).
#define SAVE_COMMENT2 0x20
#define SAVE_BANNER 0x40
#define SAVE_ICON 0x1840
#define SAVE_CLUT 0x1C40
#define SAVE_HDR 0x2000
#define SAVE_HDR_MAGIC 0x2004
#define SAVE_HDR_TIME 0x2008
#define SAVE_HDR_MODE 0x2030
#define SAVE_HDR_X3D 0x203D
#define SAVE_GAME 0x2034
#define SAVE_GAME_SIZE 0x36F8
#define SAVE_DATA2 0x572C
#define SAVE_DATA2_SIZE 0x1204
#define SAVE_ROOM 0x6930
#define SAVE_SSCRN 0xE7D0
#define SAVE_MERCHANT 0xE7D4
#define SAVE_CRC 0xEAF8
#define SAVE_SIZE 0xEAFC
#define SYS_WORK 0x1E40
#define SYS_CRC 0x1E78
#define SYS_SIZE 0x1E7C

static inline void U16Inc(u16& v) { v++; }
static inline u32 bitChk(u32 f, u32 b) { return f & b; }

#define KEY_START 0x00080000
#define KEY_Z 0x00040000

// Card screen widgets (CardID), 0x78 bytes.
class CardID {
public:
    u32 x0;
    void* pTex;      // 0x04
    void* pSaveDat;  // 0x08  save frame
    void* pFile;     // 0x0C  file list entries (types 0x40..0x46)
    void* pFrame;    // 0x10
    void* pLoadDat;  // 0x14  load frame
    void* pBg;       // 0x18  message background
private:
    s32 m_act_flag;      // 0x1C  1 up, 2 down, 4 decided, 8 moving
public:
    IDSystem m_IdSave;  // 0x20
    s32 m_mode;        // 0x70
    s8 rno0;        // 0x74
    s8 rno1;         // 0x75
    u8 rno2;
    u8 rno3;

    void updateSaveInfo(cCard* c);
    void init(int type, CardArc* data);
    void move(cCard* c);
    void wait(cCard* c);
    void start(cCard* c);
    void normal(cCard* c);
    void up_down(cCard* c);
    void save(cCard* c);
    void quit();
    void setAction(int a);
    s32 getAction() { return m_act_flag; }
};

void dispSaveInfo(int no, SaveInfo* info, int type, int broken);
// cCard::exit passes the saved int width as a full word (`lwz`, not the `lhz 0x41a` narrowing a u16
// parameter gets): int view of ScreenReSize.
extern "C" void ScreenReSizeI(int w, int h) asm("ScreenReSize");

int isDbgInfoAlloc = 0;
static int isDbgInfoCached = 0;

// Struct-member view of the cache bits (the pLog trick): a load through it stays below a
// preceding fileFlag store (saveFileCheck).
struct IntView {
    int v;
};
#define DBG_CACHED (((IntView*) &isDbgInfoCached)->v)
static cCard* pCard = 0;
static CardID* g_id = 0;

char idpath[] = "ss/cmn/save_j.dat";
char fileext[] = "jeegfsie";

MesPos mes_pos_tbl_jpn[] = {
    { 0x50, 0x87, 0x0000 }, { 0x78, 0xD2, 0x0001 }, { 0x32, 0x6E, 0x0002 }, { 0x3C, 0x82, 0x0003 },
    { 0x3C, 0x6E, 0x0004 }, { 0x3C, 0x64, 0x0005 }, { 0x3C, 0x96, 0x0006 }, { 0x2D, 0x6E, 0x0007 },
    { 0x50, 0x8C, 0x0008 }, { 0x50, 0x6E, 0x0009 }, { 0x8C, 0xA0, 0x000A }, { 0x8C, 0xD2, 0x000B },
    { 0x8C, 0xD2, 0x000C }, { 0x8C, 0xD2, 0x000D }, { 0x8C, 0xD2, 0x000E }, { 0x6E, 0x8C, 0x000F },
    { 0x8C, 0xD2, 0x0010 }, { 0x6E, 0x8C, 0x0011 }, { 0x8C, 0xD2, 0x0012 }, { 0x8C, 0xD2, 0x0013 },
    { 0x64, 0xA0, 0x0014 }, { 0x28, 0x6E, 0x0015 }, { 0x3C, 0x6E, 0x0016 }, { 0x28, 0x64, 0x0017 },
    { 0x28, 0x5A, 0x0018 }, { 0x28, 0x5A, 0x0019 }, { 0x28, 0x5A, 0x001A }, { 0x28, 0x50, 0x001B },
    { 0x28, 0x64, 0x001C }, { 0x3C, 0x78, 0x001D }, { 0x50, 0x6E, 0x001E }, { 0x8C, 0xA0, 0x001F },
    { 0x28, 0x82, 0x0020 }, { 0x28, 0x6E, 0x0021 }, { 0x28, 0x64, 0x0022 }, { 0x28, 0x64, 0x0023 },
    { 0x28, 0x64, 0x0024 }, { 0x28, 0x6E, 0x0025 }, { 0x8C, 0xD2, 0x0026 }, { 0x28, 0x6E, 0x0027 },
    { 0x28, 0x6E, 0x0028 }, { 0x28, 0x6E, 0x0029 }, { 0x28, 0x6E, 0x002A }, { 0x28, 0x6E, 0x002B },
    { 0x28, 0x6E, 0x002C }, { 0x8C, 0x6E, 0x0026 }, { 0x28, 0x6E, 0x0027 },
};

MesPos mes_pos_tbl_usa[] = {
    { 0x64, 0x6E, 0x0000 }, { 0x64, 0xD2, 0x0001 }, { 0x64, 0x5A, 0x0002 }, { 0x64, 0x6E, 0x0003 },
    { 0x64, 0x6E, 0x0004 }, { 0x64, 0x6E, 0x0005 }, { 0x64, 0x6E, 0x0006 }, { 0x64, 0x6E, 0x0007 },
    { 0x64, 0x82, 0x0008 }, { 0x64, 0x6E, 0x0009 }, { 0x64, 0x96, 0x000A }, { 0x64, 0xD2, 0x000B },
    { 0x64, 0xD2, 0x000C }, { 0x64, 0xD2, 0x000D }, { 0x64, 0xD2, 0x000E }, { 0xC8, 0x6E, 0x000F },
    { 0x64, 0xD2, 0x0010 }, { 0xC8, 0x6E, 0x0011 }, { 0x64, 0xD2, 0x0012 }, { 0x64, 0xD2, 0x0013 },
    { 0x64, 0xD2, 0x0014 }, { 0x64, 0x8C, 0x0015 }, { 0x64, 0x6E, 0x0016 }, { 0x64, 0x46, 0x0017 },
    { 0x64, 0x46, 0x0018 }, { 0x64, 0x46, 0x0019 }, { 0x64, 0x46, 0x001A }, { 0x64, 0x46, 0x001B },
    { 0x64, 0x6E, 0x001C }, { 0x64, 0x5A, 0x001D }, { 0x64, 0x82, 0x001E }, { 0x64, 0x96, 0x001F },
    { 0x64, 0x6E, 0x0020 }, { 0x64, 0x5A, 0x0021 }, { 0x64, 0x5A, 0x0022 }, { 0x64, 0x5A, 0x0023 },
    { 0x64, 0x5A, 0x0024 }, { 0x64, 0x5A, 0x0025 }, { 0x64, 0xD2, 0x0026 }, { 0x64, 0x6E, 0x0027 },
    { 0x64, 0x96, 0x0028 }, { 0x64, 0x96, 0x0029 }, { 0x64, 0x6E, 0x002A }, { 0x64, 0x82, 0x002B },
    { 0x64, 0x6E, 0x002C }, { 0x64, 0x82, 0x0026 }, { 0x64, 0xD2, 0x0027 },
};

MesPos* mes_pos_tbl[8] = {
    mes_pos_tbl_jpn, mes_pos_tbl_usa, mes_pos_tbl_usa, mes_pos_tbl_usa,
    mes_pos_tbl_usa, mes_pos_tbl_usa, mes_pos_tbl_usa, mes_pos_tbl_usa,
};

Vec g_pos0_org;
u8* pDbgSaveInfo[20];
u32 CRCTable[256];

// Aggregates (MEM_IN_STRUCT_P): the stores in CardID::init stay ordered against the `u->` loads,
// which plain pointer variables would not be (scalar at a fixed address vs struct member).
static void* g_p_path_org[1];
static void* g_p_hrmt_org[1];
static void* g_p_spln_org[1];

#define ROUNDUP(x, a) (((x) + ((a) - 1)) / (a) * (a))

// Free blocks of a slot (free bytes rounded up to the sector size).
#define FREE_BLOCKS(s) (((s).sectorSize ? ROUNDUP((s).freeBytes, (s).sectorSize) : 0) / (s).sectorSize)

// Dev mode: prints the slot list (CARD SLOT A / HARD DISK) with the selected one highlighted.
void debugInfoDisp(int slot_no, int type)
{
    static u8 col_tbl[2] = { 0x14, 0x05 };

    if (pG->IsDevConsole == 1) {
        eprintf2(12, 16, 32, 38, col_tbl[slot_no == 0], 0, "CARD SLOT A");
        eprintf2(12, 16, 32, 62, col_tbl[slot_no == 2], 0, "HARD DISK");
    }
}

// (empty)
cCard::cCard()
{
}

// (empty)
cCard::~cCard()
{
}

// Boot: initialises the CARD library and the CRC table, resets the card serial.
void CardInit()
{
    CARDInit();
    CRCInit();
    pG->card_serial = 1;
}

// Rno0 == 0 (load / save): picks the slot: on the retail build slot A at once, in dev mode the
// player chooses slot A or the host disk; B cancels (exit). Shows the "checking card" message.
void cCard::slotSelect()
{
    int i;

    m_Status = 0;
    switch (m_Rno1) {
    case 0:
        if (unmount(0) == 1) {
            for (i = 0; i < 20; i++) {
                m_Slot[0].fileFlag[i] = 0;
                m_Slot[2].fileFlag[i] = 0;
            }
            m_Rno1++;
        }
        break;
    case 1:
        if (pG->IsDevConsole == 1) {
            if (Key.trg & KEY_DOWN) {
                m_SlotNo = 2;
            } else if (Key.trg & KEY_UP) {
                m_SlotNo = 0;
            } else if (Key.trg & KEY_A) {
                m_Rno0 = 1;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
                formatted = 0;
            } else if (Key.trg & KEY_B) {
                m_Rno0 = 4;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            }
        } else {
            m_Rno0 = 1;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        }
        break;
    }
    if (0) {
        eprintf(0, 0, 0, 0, "MES NO:%d", 0);
    }
}

// Rno0 == 1: the card check chain on the chosen slot: existCheck -> mount -> verifyCheck ->
// saveFileCheck (-> systemFileCheck), each an async step; any CARD error goes to errorDisp.
void cCard::inSlotCheck()
{
    int ret;
    CardSlot* s;

    switch (m_Rno1) {
    case 0:
        setMsgWindow(1, 1);
        if (pG->CardStatus & 0x80) {
            cardMesSet(MES_NOT_MAKE_NEW_FILE, 0, 0);
        } else {
            cardMesSet(MES_CARD_ACCESS, 0, 0);
        }
        if (unmount(0) == 1) {
            m_Rno1++;
        }
        break;
    case 1:
        ret = existCheck(m_SlotNo, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            errorSet(m_ResultCode);
        }
        break;
    case 2:
        ret = mount(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            errorSet(m_ResultCode);
        }
        break;
    case 3:
        ret = verifyCheck(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
            if (pG->CardStatus & 0x80) {
                m_Rno1++;
            }
        } else if (ret < 0) {
            errorSet(m_ResultCode);
        }
        break;
    case 4:
        if (saveFileCheck(&m_Rno2, &m_Slot[m_SlotNo]) == 1) {
            m_Rno1++;
        }
        break;
    case 5:
        if (systemFileCheck(&m_Rno2, &m_Slot[m_SlotNo]) == 1) {
            m_Rno1++;
        }
        break;
    case 6:
        ret = freeCheck(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            if (m_aMode == 2) {
                errorSet(m_ResultCode);
            } else {
                m_Rno1++;
            }
        }
        break;
    case 7:
        if (m_SlotNo != 2) {
            ret = CARDGetSerialNo(m_SlotNo, &m_Slot[m_SlotNo].serial);
            if (ret == -1) {
                break;
            }
            if (ret == 0) {
                if (pG->CardStatus & 0x80) {
                    m_Rno1++;
                } else {
                                        m_Rno0 = 2;
                    m_Rno1 = 0;
                    m_Rno2 = 0;
                    m_Rno3 = 0;
                }
            } else {
                errorSet(ret);
            }
        } else {
            if (pG->CardStatus & 0x80) {
                m_Rno0 = 8;
            } else {
                m_Rno0 = 2;
            }
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        }
        break;
    case 8:
        s = &m_Slot[m_SlotNo];
        if (s->flags & 0x200) {
                        m_Rno0 = 8;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
/*/BF*/
        } else {
            int blocks = FREE_BLOCKS(*s);
            if (blocks == 0 || s->freeFiles == 0) {
                errorSet(-0x20A);
            } else {
                                m_Rno0 = 8;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            }
        }
        break;
    }
}

// Rno0 == 2: the save slot list: shows the 20 files' headers (dispSaveInfo through CardID),
// Up / Down select, A confirms (load / save / overwrite prompt), B exits; a mismatching card
// serial or a corrupt header marks the file. Dev mode prints the card statistics.
void cCard::dataSelect()
{
    int i;
    CardSlot* s;
    int sel;
    SaveInfo* info;

    if (m_SlotNo != 2) {
        if (CARDProbeEx(m_SlotNo, 0, 0) == -3) {
            errorSet(-3);
            return;
        }
        eprintf(32, 300, 0, 0, "Card    : %2dMbit", m_Slot[m_SlotNo].memSize);
        eprintf(32, 320, 0, 0, "Sector  : 0x%x", m_Slot[m_SlotNo].sectorSize);
        eprintf(32, 340, 0, 0, "F size  : %d", m_Slot[m_SlotNo].freeBytes);
        eprintf(32, 360, 0, 0, "F entry : %d", m_Slot[m_SlotNo].freeFiles);
        eprintf(32, 380, 0, 0, "F block : %d", FREE_BLOCKS(m_Slot[m_SlotNo]));
    }
    if (m_Slot[m_SlotNo].fileFlag[m_SaveNo] & 1) {
        info = (SaveInfo*) m_pSaveInfo[m_SaveNo];
        if (info->magic != 0x116) {
            eprintf2(12, 16, 220, 380, 0, 0, "DATA IS CORRUPTED");
            m_Slot[m_SlotNo].fileFlag[m_SaveNo] |= 2;
        } else {
            eprintf2(12, 16, 220, 380, 0, 0, "R%03X", info->room);
            eprintf2(12, 16, 220, 400, 0, 0, "%02d/%02d/%02d %02d:%02d:%02d", info->time.year % 100, info->time.mon + 1,
                     info->time.mday, info->time.hour, info->time.min, info->time.sec);
        }
    } else {
        eprintf2(12, 16, 220, 380, 0, 0, "NO DATA");
    }

    switch (m_Rno1) {
    case 0: {
        if (pG->card_serial == m_Slot[m_SlotNo].serial) {
            m_SaveNo = pG->CardLastSelNo;
        } else {
            int found = 0;
            u32 n;
            for (n = 0; n < 20; n++) {
                if (m_Slot[m_SlotNo].fileFlag[n] & 1) {
                    m_SaveNo = n;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                m_SaveNo = 0;
            } else {
                n = m_SaveNo + 1;
                while (n <= 19) {
                    if (m_Slot[m_SlotNo].fileFlag[n] & 1) {
                        info = (SaveInfo*) m_pSaveInfo[m_SaveNo];
                        if (OSCalendarTimeToTicks(&((SaveInfo*) m_pSaveInfo[n])->time) > OSCalendarTimeToTicks(&info->time)) {
                            m_SaveNo = n;
                            n = m_SaveNo + 1;
                            continue;
                        }
                    }
                    n++;
                }
            }
        }
        m_Rno1++;
        setStatus(2);
        setMsgWindow(1, 0);
        cMes.Clear();
    }
        // fallthrough
    case 1:
        if (g_id->getAction() != 0) {
            break;
        }
        if (Key.rep & (KEY_UP | KEY_DOWN)) {
            if (Key.rep & KEY_UP) {
                m_SaveNo--;
                g_id->setAction(1);
                SndCall(0, 0x2F, 0, 0, 0, 0);
            } else if (Key.rep & KEY_DOWN) {
                m_SaveNo++;
                g_id->setAction(2);
                SndCall(0, 0x2C, 0, 0, 0, 0);
            }
            m_SaveNo = m_SaveNo < 0 ? 19 : (m_SaveNo > 19 ? 0 : m_SaveNo);
            break;
        } else if (Key.trg & KEY_B) {
            /*BF:ds1b*/
            m_Rno0 = 5;
            m_Rno1 = 2;
            m_Rno2 = 0;
            m_Rno3 = 0;
/*/BF*/
        } else if (Key.trg & KEY_A) {
            /*BF:ds1a*/
            m_Rno1++;
            m_Rno2 = 0;
            m_Rno3 = 0;
/*/BF*/
        }
        break;
    case 2: {
        int blocks;
        s = &m_Slot[m_SlotNo];
        blocks = FREE_BLOCKS(*s);
        if (s->fileFlag[m_SaveNo] != 0) {
            if (m_aMode == 1) {
                m_Rno1 = 3;
                if (m_SlotNo == 2) {
                    break;
                }
                if (s->flags & 0x200) {
                    break;
                }
                if (blocks != 0 && s->freeFiles != 0) {
                    break;
                }
                errorSet(-0x20A);
            } else {
                if (s->fileFlag[m_SaveNo] & 2) {
                    errorSet(-0x202);
                } else {
                    m_Rno1 = 5;
                }
            }
        } else {
            if (m_aMode == 1) {
                if (m_SlotNo == 2) {
                    m_Rno1 = 4;
                    break;
                }
                m_Rno1 = 4;
                if (!(s->flags & 0x200)) {
                    if ((u32) blocks < m_SaveSize + m_SysSize || s->freeFiles < 2) {
                        errorSet(-0x201);
                    }
                } else {
                    if ((u32) blocks < m_SaveSize || s->freeFiles < 1) {
                        errorSet(-0x201);
                    }
                }
            } else {
                m_Rno1 = 1;
            }
        }
        break;
    }
    case 3:
        setMsgWindow(1, 1);
        cardMesSet(MES_OVERWRITE_CONFIRM, 0, 0);
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1 = 6;
        break;
    case 4:
        setMsgWindow(1, 1);
        cardMesSet(MES_SAVEFILE_CONFIRM, 0, 0);
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1 = 6;
        break;
    case 5:
        setMsgWindow(1, 1);
        cardMesSet(MES_LOADFILE_CONFIRM, 0, 0);
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1 = 6;
        break;
    case 6:
        if (Key.trg & KEY_B) {
            sel = 2;
        } else {
            sel = cMes.GetSelectMessage(0);
        }
        switch (sel) {
        case 1:
            if (m_aMode == 0) {
                CoreSeCall(0x38, 0, 0, 0, 0);
            } else {
                CoreSeCall(4, 0, 0, 0, 0);
            }
            /*BF:ds6*/
            m_Rno0 = 3;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
/*/BF*/
            pG->CardLastSelNo = m_SaveNo;
            pG->card_serial = m_Slot[m_SlotNo].serial;
            cMes.Clear();
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            m_Rno1 = 1;
            cMes.Clear();
            setMsgWindow(1, 0);
            break;
        }
        break;
    }
    m_SaveNo = m_SaveNo < 0 ? 0 : (m_SaveNo > 19 ? 19 : m_SaveNo);
}

// Rno0 == 3 (load): opens "bh4_data%02d" (or the host file), reads the whole block, verifies the
// CRCs and version, and restores the game save (GameSaveLoad), room data, subscreen and merchant
// state; errors -0x202 (version) / -0x204 (open) go to errorDisp.
void cCard::loadMain()
{
    u8* buf = pSaveBuf;
    char* name;
    int ret;

    if (m_SlotNo == 2) {
        sprintf(m_Name, "d:\\bio4/room/savedata%02d.dat", m_SaveNo);
    } else {
        sprintf(m_Name, "bh4_data%02d", m_SaveNo);
    }
    name = m_Name;
    if (m_Slot[m_SlotNo].fileFlag[m_SaveNo] & 4) {
        errorSet(-0x202);
        return;
    }
    switch (m_Rno1) {
    case 0:
        setMsgWindow(1, 1);
        SysFlagOn(pG, SYS_CARD_ACCESS);
        cardMesSet(MES_LOADING, 0, 0);
        if (m_SlotNo == 2) {
            m_Rno1 = 2;
            break;
        }
        m_RetryCtr = 0;
        m_Rno1++;
        // fallthrough
    case 1:
        ret = fileOpen(&m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            errorSet(-0x204);
        }
        break;
    case 2:
        if (m_SlotNo == 2) {
            int dbg = 1;
            if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                SysFlagOn(pG, SYS_SN_PC_READ);
            }
            HDRead(name, pSaveBuf);
            if (dbg == 0) {
                SysFlagOff(pG, SYS_SN_PC_READ);
            }
            m_Rno1++;
        } else {
            ret = fileRead(&m_Rno2, pSaveBuf, m_SaveSize << 13, 0, &m_Slot[m_SlotNo]);
            if (ret == 0) {
            } else if (ret > 0) {
                m_Rno1++;
            } else {
                errorSet(-0x204);
            }
        }
        break;
    case 3:
        if (CRCVerify(pSaveBuf, SAVE_CRC, *(u32*) (pSaveBuf + SAVE_CRC)) == 0) {
            if (m_RetryCtr == 3) {
                m_Slot[m_SlotNo].fileFlag[m_SaveNo] |= 2;
                errorSet(-0x202);
            } else {
                m_Rno1 = 2;
                m_RetryCtr++;
            }
        } else {
            if (m_SlotNo == 2) {
                m_Rno1 = 5;
            } else {
                m_Rno1++;
            }
        }
        break;
    case 4:
        ret = fileClose(&m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            errorSet(-0x204);
        }
        break;
    case 5:
        cardMesSet(MES_SAVE_DONE, 0, 0);
        memcpy(SD->p8, buf + SAVE_GAME, SAVE_GAME_SIZE);
        memcpy(SD->pC, buf + SAVE_DATA2, SAVE_DATA2_SIZE);
        memcpy(SD->p10, buf + SAVE_ROOM, RoomData.num * 0xD8 + 0x10);
        memcpy(SD->p14, buf + SAVE_SSCRN, SscrnDataSize());
        memcpy(SD->p18, buf + SAVE_MERCHANT, MerchantDataSize());
        GameSave.load(pSaveData);
        GameSave.save(pSaveData, pG->SaveKind);
        pG->CardStatus |= 4;
        SysFlagOff(pG, SYS_CARD_ACCESS);
        setMsgWindow(1, 0);
                m_Rno0++;
        m_Rno1 = 0;
        m_Rno2 = 0;
        m_Rno3 = 0;
        break;
    }
}

// Builds the save file image in pSaveBuf: banner / icons / comment strings ("biohazard4 FILE%02d"),
// the header (mode, serial, play time...), then the game save blocks (GameSave.save, room data,
// subscreen, merchant) and both CRCs.
void cCard::makeSaveData()
{
    int i;
    TEXPalette* tpl = (TEXPalette*) (pSubData->ofs[0] + (u32) pSubData);
    u8* buf = pSaveBuf;
    TEXDescriptor* d;
    u8* src;
    OSCalendarTime cal;

    OSTicksToCalendarTime(OSGetTime(), &cal);
    for (i = 0; i < ICON_NUM; i++) {
        d = TEXGet(tpl, i + 2);
        src = (u8*) d->textureHeader->data;
        memcpy(buf + SAVE_ICON + (i << 10), src, 0x400);
    }
    src = (u8*) d->CLUTHeader->data;
    memcpy(buf + SAVE_CLUT, src, 0x200);
    if (pSys->eff_country == 0) {
        sprintf((char*) buf, "biohazard4 FILE%02d", m_SaveNo + 1);
        sprintf((char*) buf + SAVE_COMMENT2, "%04d/%02d/%02d %02d:%02d:%02d \x8dX\x90V", cal.year, cal.mon + 1, cal.mday,
                cal.hour, cal.min, cal.sec);
        i = 0;
    } else {
        sprintf((char*) buf, "resident evil 4");
        sprintf((char*) buf + SAVE_COMMENT2, "FILE %02d", m_SaveNo + 1);
        i = 1;
    }
    d = TEXGet(tpl, i);
    src = (u8*) d->textureHeader->data;
    memcpy(buf + SAVE_BANNER, src, 0x1800);
    pG->save_cnt++;
    if (!(pG->CardStatus & 0x60)) {
        SetGameTime();
    }
    if (pG->CardStatus & 0x20) {
        *(u32*) (buf + SAVE_HDR_MODE) = 2;
    } else if (pG->CardStatus & 0x40) {
        *(u32*) (buf + SAVE_HDR_MODE) = 3;
    } else {
        *(u32*) (buf + SAVE_HDR_MODE) = 1;
    }
    GameSave.save(pSaveData, *(u32*) (buf + SAVE_HDR_MODE));
    memcpy(buf + SAVE_GAME, SD->p8, SAVE_GAME_SIZE);
    memcpy(buf + SAVE_DATA2, SD->pC, SAVE_DATA2_SIZE);
    memcpy(buf + SAVE_ROOM, SD->p10, RoomData.num * 0xD8 + 0x10);
    memcpy(buf + SAVE_SSCRN, SD->p14, SscrnDataSize());
    memcpy(buf + SAVE_MERCHANT, SD->p18, MerchantDataSize());
    *(u32*) (buf + SAVE_HDR_MAGIC) = 0x116;
    buf[SAVE_HDR_X3D] = buf[0x5408];
    *(OSCalendarTime*) (buf + SAVE_HDR_TIME) = cal;
    *(u32*) (buf + SAVE_HDR) = CRCCalc(buf + SAVE_HDR_MAGIC, 0x1FC);
    *(u32*) (buf + SAVE_CRC) = CRCCalc(buf, SAVE_CRC);
    DCFlushRange(pSaveBuf, SAVE_SIZE);
}

// Builds the system file image (banner / icons, the system flags) with its CRC.
void cCard::makeSystemSaveData()
{
    int i;
    TEXPalette* tpl = (TEXPalette*) (pSubData->ofs[0] + (u32) pSubData);
    u8* buf = pSysBuf;
    TEXDescriptor* d;
    u8* src;
    OSCalendarTime cal;

    OSTicksToCalendarTime(OSGetTime(), &cal);
    for (i = 0; i < ICON_NUM; i++) {
        d = TEXGet(tpl, i + 3);
        src = (u8*) d->textureHeader->data;
        memcpy(buf + SAVE_ICON + (i << 10), src, 0x400);
    }
    src = (u8*) d->CLUTHeader->data;
    memcpy(buf + SAVE_CLUT, src, 0x200);
    if (pSys->eff_country == 0) {
        sprintf((char*) buf, "biohazard4 \x83V\x83X\x83" "e\x83\x80\x83t\x83@\x83" "C\x83\x8b");
        sprintf((char*) buf + SAVE_COMMENT2, "%04d/%02d/%02d %02d:%02d:%02d \x8dX\x90V", cal.year, cal.mon + 1, cal.mday,
                cal.hour, cal.min, cal.sec);
        i = 0;
    } else {
        sprintf((char*) buf, "resident evil 4");
        sprintf((char*) buf + SAVE_COMMENT2, "Systemfile");
        i = 1;
    }
    d = TEXGet(tpl, i);
    src = (u8*) d->textureHeader->data;
    memcpy(buf + SAVE_BANNER, src, 0x1800);
    *(SYSTEM_SAVE_WORK*) (buf + SYS_WORK) = *pSys;
    *(u32*) (buf + SYS_WORK + 4) |= sysFlags;
    *(u32*) (buf + SYS_CRC) = CRCCalc(buf, SYS_CRC);
    DCFlushRange(pSaveBuf, SYS_SIZE);
}

// Rno0 == 3 (save) / 8: writes a save or the system file: builds the image, opens or creates
// the card file (fileCreate for the needed blocks), writes it, sets its status (icons, comment
// offsets), verifies by reading back and updates the file list; the "saving" message and error
// handling around it.
void cCard::saveMain()
{
    void (cCard::*makeFunc)();
    char* name;
    u8* buf;
    int blocks;
    int nextMode;
    int ret;

    if (m_Rno0 == 3) {
        m_mMode = 0;
    } else {
        m_mMode = 1;
    }
    if (m_mMode == 0) {
        if (m_SlotNo == 2) {
            name = m_Name;
            sprintf(name, "d:\\bio4/room/savedata%02d.dat", m_SaveNo);
        } else {
            name = m_Name;
            sprintf(name, "bh4_data%02d", m_SaveNo);
        }
        blocks = m_SaveSize;
        buf = pSaveBuf;
        makeFunc = &cCard::makeSaveData;
        nextMode = 8;
    } else {
        if (m_SlotNo == 2) {
            name = m_Name;
            sprintf(name, "d:\\bio4/room/sysdata.dat");
        } else {
            name = m_Name;
            sprintf(name, "bh4_system");
        }
        blocks = m_SysSize;
        buf = pSysBuf;
        makeFunc = &cCard::makeSystemSaveData;
        nextMode = 4;
    }

    switch (m_Rno1) {
    case 0:
        setMsgWindow(1, 1);
        SysFlagOn(pG, SYS_CARD_ACCESS);
        if (m_mMode == 0) {
            cardMesSet(MES_SAVING, 0, 0);
            m_Rno1 = 2;
            if (m_SlotNo == 2) {
                m_Rno1 = 5;
            }
        } else {
            if (pG->CardStatus & 0x80) {
                cardMesSet(MES_SYS_SAVING, 0, 0);
            }
            m_Rno1++;
        }
        m_Rno2 = 0;
        m_Rno3 = 0;
        break;
    case 1:
        ret = sysfileRead(&m_Rno2, &m_Rno3, 0);
        switch (ret) {
        case 0:
            break;
        case 1:
            sysFlags = *(u32*) (pSysBuf + SYS_WORK + 4);
            // fallthrough
        case -1:
            m_Rno1++;
            if (m_SlotNo == 2) {
                m_Rno1 = 5;
            }
            m_Rno2 = 0;
            m_Rno3 = 0;
            break;
        }
        break;
    case 2:
        ret = fileOpen(&m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            if (m_ResultCode == 0) {
                m_Rno1 = 4;
            } else {
                m_Rno1++;
            }
            m_Rno2 = 0;
            m_Rno3 = 0;
        } else if (ret < 0) {
            errorSet(-0x203);
        }
        break;
    case 3:
        ret = fileCreate(&m_Rno2, blocks, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno2 = 0;
            m_Rno3 = 0;
            m_Rno1++;
        } else if (ret < 0) {
            if (m_ResultCode == -5) {
                errorSet(-5);
            } else {
                errorSet(-0x203);
            }
        }
        break;
    case 4:
        ret = CARDGetStatus(m_SlotNo, m_Slot[m_SlotNo].fileInfo.fileNo, &m_Slot[m_SlotNo].stat);
        if (ret == -1) {
            break;
        }
        if (ret == 0) {
            m_Rno1++;
        } else {
            errorSet(-0x203);
        }
        break;
    case 5:
        (this->*makeFunc)();
        if (m_mMode == 0) {
            m_Slot[m_SlotNo].fileFlag[m_SaveNo] = 1;
            memcpy(m_pSaveInfo[m_SaveNo], pSaveBuf + 0x2000, 0x200);
            g_id->setAction(4);
            resetStatus(1);
        }
        m_Rno1++;
        // fallthrough
    case 6:
        if (m_SlotNo == 2) {
            int dbg = 1;
            if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                SysFlagOn(pG, SYS_SN_PC_READ);
            }
            HDWrite_only(name, buf, blocks << 13);
            if (dbg == 0) {
                SysFlagOff(pG, SYS_SN_PC_READ);
            }
            if (m_Rno0 == 3) {
                // The zero stored into step shares `no`'s pseudo with the shift count: its `li` then
                // trails the rotlw (sched1 anti-dependence), takes r0, and jump2 cross-jumps this
                // arm's `stb step` (and case 2's `step = 4`, the `step = 0xA` arm) into case 1's copy.
                // A fresh zero is hoisted above the extsb, lands in r10 and keeps its own stb.
                int no = m_SaveNo;
                isDbgInfoCached &= ~(1 << no);
                m_Rno0 = nextMode;
                no = 0;
                m_Rno1 = no;
            } else {
                if (pG->CardStatus & 0x80) {
                    m_Rno1 = 0xB;
                    m_Timer = 0xF;
                } else {
                    m_Rno1 = 0xA;
                }
            }
            m_Rno2 = 0;
            m_Rno3 = 0;
        } else {
            ret = fileWrite(&m_Rno2, buf, blocks, &m_Slot[m_SlotNo]);
            if (ret == 0) {
            } else if (ret > 0) {
                m_Rno1++;
            } else if (ret < 0) {
                if (m_ResultCode == -5) {
                    errorSet(-5);
                } else {
                    errorSet(-0x203);
                }
            }
        }
        break;
    case 7:
        makeCardStatus(&m_Slot[m_SlotNo]);
        CARDSetStatusAsync(m_SlotNo, m_Slot[m_SlotNo].fileInfo.fileNo, &m_Slot[m_SlotNo].stat, 0);
        m_Rno1++;
        // fallthrough
    case 8:
        switch (CARDGetResultCode(m_SlotNo)) {
        case -1:
            break;
        case 0:
            m_Rno1++;
            break;
        case -5:
            errorSet(-5);
            break;
        default:
            errorSet(-0x203);
            break;
        }
        break;
    case 9:
        ret = fileClose(&m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            if (m_Rno0 == 3) {
                m_Rno0 = nextMode;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            } else {
                if (pG->CardStatus & 0x80) {
                    m_Rno1 = 0xB;
                    m_Timer = 0xF;
                } else {
                    m_Rno1++;
                }
                m_Rno2 = 0;
                m_Rno3 = 0;
            }
        } else if (ret < 0) {
            errorSet(-0x203);
        }
        break;
    case 10:
        setStatus(1);
        SysFlagOff(pG, SYS_CARD_ACCESS);
        cardMesSet(MES_SAVE_DONE, 0, 0);
        if (Key.trg & (KEY_START | KEY_Z)) {
            cMes.Clear();
            setMsgWindow(1, 0);
            m_Rno0 = nextMode;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        }
        break;
    case 11:
        cardMesSet(MES_SYS_SAVE_DONE, 0, 0);
        if (m_Timer == 0) {
            cMes.Clear();
            setMsgWindow(1, 0);
            SysFlagOff(pG, SYS_CARD_ACCESS);
            m_Rno0 = 4;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        } else {
            m_Timer--;
        }
        break;
    }
}

// Rno0 == 4: leaves the card screen: unmounts, fades, restores the messages and the swapped
// memory, and for the first check records the card status bits in pG->CardStatus.
void cCard::exit()
{
    u32 i;
    u32 c0;
    u32 c1;

    switch (m_Rno1) {
    case 0:
        if (unmount(0) == 1) {
            m_Rno1++;
        }
        break;
    case 1:
        cMes.Clear();
        if (m_aMode == 2) {
            pG->CardStatus |= 0x80000000;
            systemVISetBlack(1);
            workDestroy();
            exitFlag = 1;
        } else {
            if (!(pG->CardStatus & 0x80)) {
                c0 = 0;
                c1 = 0xFF;
                FadeSet(0, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
            }
            m_Rno1++;
        }
        break;
    case 2:
        if (Fade[0].flags & 1) {
            break;
        }
        (u32&) dispFlag = 0;
        if (!(pG->CardStatus & 0x80)) {
            g_id->quit();
        }
        workDestroy();
        pG->Stop_flg = m_SPFbak;
        TaskSignal(0);
        if (!(pG->CardStatus & 0x80)) {
            pG->Disp_flg = m_DPFbak;
            SndStrReq(m_SndId, 4, 200, 0);
            ScreenReSizeI(m_Width_bak, 448);
            if (m_aMode != 0 || !(pG->CardStatus & 4)) {
                if (!(pG->CardStatus & 0x10)) {
                    c0 = 0xFF;
                    c1 = 0;
                    FadeSet(0x80000000, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
                }
                for (i = 0; i < 4; i++) {
                    if (m_StrInfo[i].id != 0 && SndStrStatusCk(m_StrInfo[i].id, 0x10) != 0) {
                        SndStrReq(m_StrInfo[i].id, 4, 100, m_StrInfo[i].vol);
                    }
                }
                SndSePauseAll(0);
                SndRoomBgmMuteAll(0, -1);
            }
        }
        pG->CardStatus &= ~0x7FFFFFF8;
        MesData.registData(0, (u8*) (pG->pCore->ofs_28 + (u32) pG->pCore));
        exitFlag = 1;
        break;
    }
}

// Frees the CARD work areas and the save / system / info buffers.
void cCard::workDestroy()
{
    if (m_Slot[0].workArea) {
        Mem_free(m_Slot[0].workArea);
    }
    if (pSaveBuf) {
        Mem_free(pSaveBuf);
    }
    if (pSysBuf) {
        Mem_free(pSysBuf);
    }
    if (m_IdDataAddr) {
        Mem_free(m_IdDataAddr);
    }
    if (m_pInfoAddr) {
        Mem_free(m_pInfoAddr);
    }
    m_DataSwap.SwapIn();
}

// Rno0 == 6: the format dialog: asks, formats the card (CARDFormatAsync) with the "formatting"
// message, then back to the check (or the first-check flow) / error.
void cCard::format()
{
    int noCard = 0;
    int ret;
    int sel;

    ret = CARDProbeEx(m_SlotNo, 0, 0);
    switch (m_Rno1) {
    case 0:
        ret = mount(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            if (m_aMode == 2) {
                m_Rno1 = 3;
            } else {
                m_Rno1++;
            }
        } else if (ret < 0) {
            if (m_aMode == 2) {
                m_Rno0 = 0;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            } else {
                errorSet(m_ResultCode);
            }
        }
        break;
    case 1:
        setMsgWindow(0, 1);
        cardMesSet(MES_NEED_FORMAT, 0, 0x800000);
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1++;
        // fallthrough
    case 2:
        if (ret == -3) {
            noCard = 1;
            break;
        }
        sel = cMes.GetSelectMessage(0);
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            m_Rno1++;
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            m_Rno0 = 0;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
            break;
        }
        break;
    case 3:
        cardMesSet(MES_FORMAT_CONFIRM, 0, 0x800000);
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1++;
        // fallthrough
    case 4:
        if (ret == -3) {
            noCard = 1;
            break;
        }
        sel = cMes.GetSelectMessage(0);
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            m_Timer = 0;
            m_Rno1++;
            CARDFormatAsync(m_SlotNo, 0);
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            m_ErrCode = -0x209;
            m_Rno0 = 5;
            m_Rno1 = sel;
            m_Rno2 = 0;
            m_Rno3 = 0;
            cMes.Clear();
            break;
        }
        break;
    case 5:
        cardMesSet(MES_FORMATTING, 0, 0);
        ret = CARDGetResultCode(m_SlotNo);
        switch (ret) {
        case 0:
            formatted = 1;
            m_Rno1++;
            break;
        case -3:
            if (m_aMode == 2) {
                m_Rno0 = 0;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            } else {
                errorSet(-3);
            }
            break;
        case -0x80:
        case -5:
            errorSet(ret);
            break;
        case -1:
            break;
        }
        break;
    case 6:
        cardMesSet(MES_FORMAT_DONE, 0, 0);
        if (ret == -3) {
            noCard = 1;
            break;
        }
        if (Key.trg & KEY_A) {
            cMes.Clear();
            if (m_aMode == 2) {
                m_Rno0 = 0;
            } else {
                m_Rno0 = 0;
                setMsgWindow(0, 0);
            }
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        }
        break;
    }
    if (noCard == 1) {
        if (m_aMode == 2) {
            m_Rno0 = 0;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        } else {
            errorSet(ret);
        }
    }
}

// Rno0 == 7: deletes the selected save file (or the corrupt system file in the first check)
// after confirmation; then back to the list / errorDisp.
void cCard::fileDelete()
{
    int ret;
    int sel;

    switch (m_Rno1) {
    case 0:
        if (m_aMode == 2) {
            cardMesSet(MES_SYS_DELETE, 0, 0x800000);
            sprintf(m_Name, "bh4_system");
        } else {
            setMsgWindow(0, 1);
            cardMesSet(MES_DATA_DELETE, 0, 0x800000);
            sprintf(m_Name, "bh4_data%02d", m_SaveNo);
        }
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1++;
        // fallthrough
    case 1:
        if (CARDProbeEx(m_SlotNo, 0, 0) == -3) {
            if (m_aMode == 2) {
                m_Rno0 = 0;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            } else {
                errorSet(-3);
            }
            break;
        }
        sel = cMes.GetSelectMessage(0);
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            m_Timer = 0;
            m_Rno1++;
            CARDDeleteAsync(m_SlotNo, m_Name, 0);
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            m_Rno2 = 0;
            m_ErrCode = -0x208;
            m_Rno0 = 5;
            m_Rno1 = sel;
            m_Rno3 = 0;
            break;
        }
        break;
    case 2:
        cardMesSet(MES_DELETING, 0, 0);
        ret = CARDGetResultCode(m_SlotNo);
        switch (ret) {
        case -1:
            break;
        case 0:
            m_Rno1++;
            break;
        case -3:
            if (m_aMode == 2) {
                m_Rno0 = 0;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
            } else {
                errorSet(-3);
            }
            break;
        default:
            errorSet(ret);
            break;
        }
        break;
    case 3:
        if (m_aMode != 2) {
            m_Slot[m_SlotNo].fileFlag[m_SaveNo] &= ~1;
        }
        cardMesSet(MES_DELETE_DONE, 0, 0);
        if (Key.trg & KEY_A) {
            cMes.Clear();
            if (m_aMode == 2) {
                m_Rno0 = 0;
            } else {
                m_Rno0 = 0;
                setMsgWindow(0, 0);
            }
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        }
        break;
    }
}

// Rno0 == 5: shows the message for m_ErrCode (no card, wrong device, broken, no space, wrong
// version...) and offers the recovery (retry, format, delete, continue without saving); the
// first-check flow continues into the game on cancel.
void cCard::errorDisp()
{
    static int cardcheck;
    u32 attr = 0x800000;
    int mesNo = 0;
    int probe = 0;

    if (m_SlotNo != 2) {
        probe = CARDProbeEx(m_SlotNo, 0, 0);
    }
    SysFlagOff(pG, SYS_CARD_ACCESS);
    switch (m_Rno1) {
    case 0:
        CoreSeCall(0x2A, 0, 0, 0, 0);
        cardcheck = 1;
        switch (m_ErrCode) {
        case -3:
            if (m_aMode == 2) {
                mesNo = MES_NO_CARD_START;
            } else {
                setMsgWindow(0, 1);
                mesNo = MES_NO_CARD;
            }
            break;
        case -2:
            if (m_aMode == 2) {
                mesNo = MES_WRONG_DEVICE_START;
            } else {
                setMsgWindow(0, 1);
                mesNo = MES_WRONG_DEVICE;
            }
            break;
        case -0x80:
        case -5:
            if (m_aMode == 2) {
                mesNo = MES_CARD_BROKEN_START;
            } else {
                setMsgWindow(0, 1);
                mesNo = MES_CARD_BROKEN;
            }
            break;
        case -6:
            if (formatted == 1) {
                if (m_aMode == 2) {
                    mesNo = MES_CARD_BROKEN_START;
                } else {
                    setMsgWindow(0, 1);
                    mesNo = MES_CARD_BROKEN;
                }
                break;
            }
            // fallthrough
        case -0xD:
            if (m_aMode == 2) {
                mesNo = MES_NEED_FORMAT_START;
            } else {
                m_Rno0 = 6;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
                return;
            }
            break;
        case -0x200:
            eprintf2(10, 16, 80, 170, 0, 0, "The Memory Card in Slot %c is not supported.", m_SlotNo + 'A');
            break;
        case -0x201:
            cMes.MesSetNumber(0, m_SaveSize + m_SysSize, 2);
            if (m_aMode == 2) {
                mesNo = MES_NO_CAPA_START;
            } else {
                setMsgWindow(0, 1);
                mesNo = MES_NO_CAPA;
                attr = 0;
            }
            break;
        case -0x205:
            setMsgWindow(0, 1);
            attr = 0;
            mesNo = MES_SYS_SAVE_FAILURE;
            cardcheck = 0;
            break;
        case -0x203:
            // `attr = 0` in each arm (as in -0x20A): the arms then share a tail, jump1 cannot hoist
            // `mesNo = 1` above the branch (jump2 does, after sched1), and `cardcheck = 0` after the join
            // gets its own zero.
            if (pG->CardStatus & 0x80) {
                mesNo = MES_SYS_SAVE_FAILURE;
                attr = 0;
            } else {
                mesNo = MES_DATA_SAVE_FAILURE;
                attr = 0;
            }
            cardcheck = 0;
            break;
        case -0x204:
            // mesNo first: `cardcheck = 0` then takes the switch index's zero (step == 0 here) instead of
            // mesNo's, which gives the index the refs that put it in r31 before mesNo (r29).
            mesNo = MES_DATALOAD_FAILURE;
            cardcheck = 0;
            attr = 0;
            break;
        case -0x202:
            m_Rno0 = 7;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
            return;
        case -0x206:
            mesNo = MES_SYS_LOAD_FAILURE;
            cardcheck = 0;
            break;
        case -0x207:
            mesNo = MES_NO_SYSFILE;
            break;
        case -0x20A:
            if (m_aMode == 2) {
                mesNo = MES_SYSFILE_NOT_SAVE_START;
            } else if (pG->CardStatus & 0x80) {
                mesNo = MES_NO_CAPA_SYSFILE;
                attr = 0;
            } else {
                mesNo = MES_SYSFILE_NOT_SAVE;
                attr = 0;
            }
            break;
        case -0x20B:
            mesNo = MES_DATFILE_NOT_SAVE;
            break;
        }
        cardMesSet(mesNo, 0, attr);
        if (attr == 0) {
            m_Rno1 = 1;
        } else {
            m_Rno1 = 3;
            cMes.m_Msg[0].m_cur = 1;
        }
        m_Rno2 = 0;
        m_Rno3 = 0;
        break;
    case 1:
        if (Key.trg & (KEY_START | KEY_Z)) {
            m_Rno1++;
        }
        break;
    case 2:
        switch (m_aMode) {
        case 0:
            setMsgWindow(0, 1);
            mesNo = MES_LOAD_CONFIRM;
            break;
        case 1:
            if (pG->CardStatus & 0x80) {
                mesNo = MES_SYS_SAVE_CONFIRM;
            } else {
                setMsgWindow(0, 1);
                mesNo = MES_SAVE_CONFIRM;
            }
            break;
        case 2:
            m_Rno0 = 3;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
            cMes.Clear();
            return;
        }
        cardMesSet(mesNo, 0, 0x800000);
        if (pG->CardStatus & 0x80) {
            cMes.m_Msg[0].m_cur = 0;
        } else {
            cMes.m_Msg[0].m_cur = 1;
        }
        m_Rno1++;
        break;
    case 3:
        switch (cMes.GetSelectMessage(0)) {
        case 1:
            if (m_aMode == 2) {
                CoreSeCall(4, 0, 0, 0, 0);
                m_Rno0 = 3;
            } else if (pG->CardStatus & 0x80) {
                CoreSeCall(4, 0, 0, 0, 0);
                m_Rno0 = 0;
            } else {
                CoreSeCall(5, 0, 0, 0, 0);
                m_Rno0 = 4;
            }
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
            cMes.Clear();
            break;
        case 2:
            if (m_aMode == 2) {
                CoreSeCall(5, 0, 0, 0, 0);
                m_Rno0 = 0;
            } else if (pG->CardStatus & 0x80) {
                CoreSeCall(5, 0, 0, 0, 0);
                m_Rno0 = 4;
            } else {
                CoreSeCall(4, 0, 0, 0, 0);
                m_Rno0 = 0;
            }
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
            cMes.Clear();
            break;
        case 3:
            CoreSeCall(4, 0, 0, 0, 0);
            cMes.Clear();
            switch (m_ErrCode) {
            case -0x201:
            case -0x20A:
            case -0x20B:
                OSResetSystem(1, 1, 1);
                break;
            case -0xD:
            case -6:
                m_Rno0 = 6;
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
                break;
            }
            break;
        }
        break;
    }

    if (m_SlotNo != 2 && cardcheck == 1 && probe != -1) {
        switch (probe) {
        case -3:
            if (!(m_Slot[m_SlotNo].flags & 2)) {
                cMes.Clear();
                if (m_aMode == 2) {
                    m_Rno0 = 0;
                } else {
                    m_Rno0 = 1;
                }
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
                formatted = 0;
            }
            break;
        case -0x80:
        case -2:
        case 0:
            if (m_Slot[m_SlotNo].flags & 2) {
                cMes.Clear();
                if (m_aMode == 2) {
                    m_Rno0 = 0;
                } else {
                    m_Rno0 = 1;
                }
                m_Rno1 = 0;
                m_Rno2 = 0;
                m_Rno3 = 0;
                formatted = 0;
            }
            break;
        }
    }
    if (m_aMode != 2 && m_Rno0 != 5) {
        setMsgWindow(0, 0);
    }
    eprintf(470, 10, 0, 0, "%d", m_ErrCode);
}

// Records the error code and switches to errorDisp.
void cCard::errorSet(int code)
{
    m_ErrCode = code;
    m_Rno0 = 5;
    m_Rno1 = 0;
}

// Card screen setup for `type` (0 load, 1 save, 2 first check): saves the stop / display flags,
// swaps out the room heap for the card buffers (cDataSwap), allocates the works, loads the
// screen ids and messages. 0 when memory could not be made.
int cCard::initialize(int type)
{
    u32 c0;
    u32 c1;
    u8 heap;
    u32 addr;

    this->m_aMode = type;
    if (pSys->eff_country == 0) {
        idpath[12] = fileext[0];
    } else if (pSys->eff_country == 1) {
        idpath[12] = fileext[1];
    } else if (isEurope(pSys->eff_country)) {
        idpath[12] = fileext[pSys->language];
    } else {
        idpath[12] = fileext[7];
    }
    m_NeedMemSize = getUseMemSize();
    if (m_NeedMemSize == 0) {
        return 0;
    }
    if (type == 2) {
        if (initSub() == 0) {
            return 0;
        }
    } else {
        if (type == 0) {
            pG->CardStatus &= ~4;
        }
        heap = MemGetCurrentHeap();
        TaskSuspend(0);
        if (pG->CardStatus & 0x80) {
            addr = (u32) pG->pOption;
        } else {
            addr = MemGetHeapStartAddr(heap);
            if (!(pG->CardStatus & 8)) {
                c0 = 0;
                c1 = 0xFF;
                FadeSet(0, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
            }
            while (Fade[0].flags & 1) {
                TaskSleep(1);
            }
            m_DPFbak = pG->Disp_flg;
            pG->Disp_flg = 0xFFFFFFFF;
            DpfFlagOff(pG, DPF_MESSAGE);
            DpfFlagOff(pG, DPF_ID_SYSTEM);
        }
        if (m_DataSwap.SwapOut(addr, m_NeedMemSize, 0) == 0) {
            return 0;
        }
        m_SPFbak = pG->Stop_flg;
        pG->Stop_flg = 0xFFFFFFFF;
        SpfFlagOff(pG, SPF_KEY);
        SpfFlagOff(pG, SPF_ID_SYSTEM);
        if (initSub() == 0) {
            return 0;
        }
        if (!(pG->CardStatus & 0x80)) {
            SndPlayWork* s;
            int i;
            g_id->init(this->m_aMode, (CardArc*) m_IdDataAddr);
            m_Width_bak = (int) Screen.width;
            ScreenReSize(640, 448);
            c0 = 0xFF;
            c1 = 0;
            FadeSet(0x80000000, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
            FadeKill(FADE_NO_ROOM);
            FadeKill(FADE_NO_SCENARIO);
            s = Snd.str_state;
            i = 0;
            do {
                if (s->used == 1 && s->stat == 0) {
                    m_StrInfo[i].id = s->id;
                    m_StrInfo[i].vol = s->vol;
                    SndStrReq(s->id, 4, 100, 1);
                } else {
                    m_StrInfo[i].id = 0;
                }
                i++;
                s++;
            } while (s <= &Snd.str_state[3]);
            SndRoomBgmMuteAll(1, -1);
            SndSeAbsPause();
        }
    }
    dispFlag = 1;
    CRCInit();
    m_Slot[0].chan = 0;
    m_Slot[1].chan = 1;
    m_Slot[2].chan = 2;
    return 1;
}

// Common per-run state reset (slot, file numbers, timers, status).
int cCard::initSub()
{
    if (workAlloc() == 0) {
        return 0;
    }
    pSubData = (CardArc*) SndMem.sub_adr;
    calcTplAddr((TEXPalette*) (pSubData->ofs[0] + (u32) pSubData));
    if (!(pG->CardStatus & 0x80)) {
        void* addr;
        int req;
#line 2170 "D:/Bio4/Prog/card.cpp"
        req = DvdReadN(idpath, 0, 0, 0, 0, 4, __FILE__, __LINE__);
        while (Dvd.ReadCheck(req, 0, 0, &addr) != 1) {
            TaskSleep(1);
        }
        m_IdDataAddr = addr;
    }
    MesData.registData(0, (u8*) (pSubData->ofs[1] + (u32) pSubData));
    return 1;
}

// Allocates the CARD mount work area (slot A), the 20 save headers, the save and system file
// buffers; 0 on failure.
int cCard::workAlloc()
{
    int i;

#line 2190 "D:/Bio4/Prog/card.cpp"
    m_Slot[0].workArea = MEM_ALLOC(0xA000, 1, 13);
    if (m_Slot[0].workArea == 0) {
        OSReport("CARD workarea alloc error!!\n");
        return 0;
    }
#line 2197 "D:/Bio4/Prog/card.cpp"
    pSysBuf = (u8*) MEM_CALLOC((sysBufSize + 0x1FFF) & ~0x1FFF, 1, 13);
    if (pSysBuf == 0) {
        OSReport("System Savedata workarea alloc error!!\n");
        return 0;
    }
    if (!(pG->CardStatus & 0x80)) {
#line 2205 "D:/Bio4/Prog/card.cpp"
        pSaveBuf = (u8*) MEM_CALLOC((saveBufSize + 0x1FFF) & ~0x1FFF, 1, 13);
        if (pSaveBuf == 0) {
            OSReport("Savedata workarea alloc error!!\n");
            return 0;
        }
#line 2211 "D:/Bio4/Prog/card.cpp"
        m_pInfoAddr = (u8*) MEM_CALLOC(0x2800, 1, 13);
        if (m_pInfoAddr == 0) {
            OSReport("Savedata Infomation workarea alloc error!!\n");
            return 0;
        }
        for (i = 0; i < 20; i++) {
            m_pSaveInfo[i] = m_pInfoAddr + i * 0x200;
        }
    }
    return 1;
}

// Total memory the card screen needs (used to size the heap swap).
u32 cCard::getUseMemSize()
{
    u32 size;

    sysBufSize = SYS_SIZE;
    m_SysSize = 1;
    size = 0;
    if (!(pG->CardStatus & 0x80)) {
        saveBufSize = SAVE_SIZE;
        m_SaveSize = 8;
        if (Dvd.FileExistCheck(idpath, &size) < 0) {
            return 0;
        }
        size += 0x10000;
        size += (saveBufSize + 0x1FFF) & ~0x1FFF;
        size += 0x6000;
    }
    size += 0x10000;
    size += (sysBufSize + 0x1FFF) & ~0x1FFF;
    size -= 0x4000;
    return size;
}

// Async step: creates `fileName` with `blocks` x 8 KB on the card; 1 when done, negative CARD
// result on failure.
int cCard::fileCreate(u8* sub, int blocks, CardSlot* s)
{
    int ret = 0;

    switch (*sub) {
    case 0:
        CARDCreateAsync(s->chan, m_Name, blocks << 13, &s->fileInfo, 0);
        (*sub)++;
        // fallthrough
    case 1:
        m_ResultCode = CARDGetResultCode(s->chan);
        if (m_ResultCode == -1) {
            break;
        }
        if (m_ResultCode == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        *sub = 0;
    }
    return ret;
}

// commentAddr stored before iconAddr (the zero's qty is born first and takes r0); the tail mask
// through a two-use temp (see below).
void cCard::makeCardStatus(CardSlot* s)
{
    u32 fmt;
    u32 spd;
    u32 fmt2;
    u32 spd2;
    int i;

    s->stat.bannerFormat = (u8) ((s->stat.bannerFormat & ~3) | 2);
    s->stat.commentAddr = 0;
    s->stat.iconAddr = 0x40;
    fmt = s->stat.iconFormat;
    spd = s->stat.iconSpeed;
    for (i = 0; i < ICON_NUM; i++) {
        fmt2 = (fmt & ~(3 << (2 * i))) | (1 << (2 * i));
        spd2 = (spd & ~(3 << (2 * i))) | (3 << (2 * i));
        fmt = fmt2;
        spd = spd2;
    }
    s->stat.iconFormat = fmt2;
    {
        // The mask must stay a 32-bit `rlwinm` (a two-use temp keeps combine from folding it into
        // the u16 store as `andi. 0xfff3`); spd2 keeps 5 refs so spd (r11) is coloured before it.
        u32 t = spd2 & ~(3 << (2 * ICON_NUM));
        s->stat.iconSpeed = t;
        spd2 = t;  // COMPILER-DIFF: dead statement (second use of t)
    }
    s->stat.bannerFormat |= 4;
    DCFlushRange(&s->stat, sizeof(CardStat));
}

// Rno0 == 0 (first check at boot): walks slot A (and the host disk in dev mode): unmount, exist,
// mount, verify, free space, then the save / system file checks.
void cCard::firstCheck00()
{
    int ret;

    switch (m_Rno1) {
    case 0:
        m_SlotNo = 0;
        m_Rno1++;
        // fallthrough
    case 1:
        if (unmount(m_SlotNo) == 1) {
            m_Rno1++;
        }
        break;
    case 2:
        ret = existCheck(m_SlotNo, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            m_Rno1 = 2;
            m_SlotNo++;
        }
        break;
    case 3:
        ret = mount(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            m_Rno1 = 2;
            m_SlotNo++;
        }
        break;
    case 4:
        ret = verifyCheck(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            m_Rno1 = 2;
            m_SlotNo++;
        }
        break;
    case 5:
        if (systemFileCheck(&m_Rno2, &m_Slot[m_SlotNo]) == 1) {
            m_Rno1++;
        }
        break;
    case 6:
        if (saveFileCheck(&m_Rno2, &m_Slot[m_SlotNo]) == 1) {
            m_Rno1++;
        }
        break;
    case 7:
        if (freeCheck(&m_Rno2, &m_Slot[m_SlotNo]) != 0) {
            m_Rno1 = 2;
            m_SlotNo++;
        }
        break;
    }
    if (m_SlotNo == 1) {
        m_SlotNo = 0;
        if (m_Slot[0].flags & 0x200) {
            m_Rno0++;
        } else if (pG->IsDevConsole == 1) {
            m_SlotNo = 2;
            m_Rno0++;
        } else {
            m_Rno0 = 2;
        }
        m_Rno1 = 0;
        m_Rno2 = 0;
        systemVISetBlack(0);
    }
}

// Rno0 == 1 (first check): reads the system file (sysfileRead) and applies its settings; a
// missing / broken one leads to the create-system-file prompt or the error screen.
void cCard::firstCheck10()
{
    u8* buf = pSysBuf;
    int ret;

    sprintf(m_Name, "%s", "bh4_system");
    switch (m_Rno1) {
    case 0:
        m_Timer = 30;
        if (m_SlotNo != 2) {
            cardMesSet(MES_SYSFILE_LOADING, 0, 0);
        }
        m_RetryCtr = 0;
        m_Rno1++;
        break;
    case 1:
        ret = sysfileRead(&m_Rno2, &m_Rno3, 1);
        if (ret == 1) {
            m_Rno1++;
        } else if (ret == -1) {
            m_Rno1 = 3;
        }
        break;
    case 2:
        *pSys = *(SYSTEM_SAVE_WORK*) (buf + SYS_WORK);
        pG->CardStatus |= 1;
        SndSetOutputMode(pSys->SndMode, 1);
        m_Rno1++;
        break;
    case 3:
        if (m_Timer == 0) {
            cMes.Clear();
            if (m_SlotNo == 2) {
                m_Rno0 = 3;
            } else {
                m_Rno0++;
            }
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
            if (pG->IsDevConsole == 1) {
                pSys->language = 1;
            }
        }
        break;
    }
    if (m_Timer != 0) {
        m_Timer--;
    }
}

// Rno0 == 2 (first check): decides from the slot flags: errors (-3 no card, -0x20A / -0x20B space,
// -0x201 broken, -5 / -6 / -2 device) to errorDisp, no system file to createSysfile (Rno0 8),
// otherwise done.
void cCard::firstCheck20()
{
    u32 f = m_Slot[0].flags;

    if (f & 2) {
        errorSet(-3);
    } else if (f & 4) {
        if (f & 0x400) {
            errorSet(-0x20A);
        } else if (f & 0x800) {
            errorSet(-0x20B);
        } else {
            errorSet(-0x201);
        }
    } else if (f & 0x40) {
        errorSet(-5);
    } else if (f & 0x10) {
        errorSet(-6);
    } else if (bitChk(f, 0x20) || bitChk(f, 0x80)) {
        errorSet(-2);
    } else if (!(f & 0x200)) {
        m_Rno0 = 8;
        m_Rno1 = 0;
        m_Rno2 = 0;
        m_Rno3 = 0;
    } else {
        m_Rno1 = 0;
        m_Rno0++;
    }
}

// Rno0 == 3 (first check): done, exit.
void cCard::firstCheck30()
{
    m_Rno0 = 4;
}

// 1 once the boot card check has finished (pG->CardStatus bit31).
int CardCheckDone()
{
    return (pG->CardStatus & 0x80000000) != 0;
}

// The card screen task body: initialise for mode `arg` (0 load, 1 save, 2 first check), then run
// the Rno0 state of the mode's column every frame (slotSelect / inSlotCheck / dataSelect /
// load-save / exit / errorDisp / format / fileDelete / system save) until exitFlag, with the id
// (CardID) animation and message updates.
void cCard::MainLoop(int arg)
{
    static void (cCard::*tbl[9][3])() = {
        { &cCard::slotSelect, &cCard::slotSelect, &cCard::firstCheck00 },
        { &cCard::inSlotCheck, &cCard::inSlotCheck, &cCard::firstCheck10 },
        { &cCard::dataSelect, &cCard::dataSelect, &cCard::firstCheck20 },
        { &cCard::loadMain, &cCard::saveMain, &cCard::firstCheck30 },
        { &cCard::exit, &cCard::exit, &cCard::exit },
        { &cCard::errorDisp, &cCard::errorDisp, &cCard::errorDisp },
        { &cCard::format, &cCard::format, &cCard::format },
        { &cCard::fileDelete, &cCard::fileDelete, &cCard::fileDelete },
        { 0, &cCard::saveMain, &cCard::createSysfile },
    };

    if (pCard->initialize(arg) == 0) {
        m_Rno0 = 4;
        m_Rno1 = 2;
    }
    while (exitFlag == 0) {
        if (m_aMode != 2) {
            if (m_StrTimer == 0) {
                m_SndId = SndStrReq(0, 0x1D, 0x80000003, 0, 0, 0.0f);
                m_StrTimer = 0xA8C;
            } else {
                m_StrTimer--;
            }
            eprintf(24, 32, 0, 0, "%d", m_StrTimer);
        }
        TaskSleep(1);
        (this->*tbl[m_Rno0][arg])();
        if (dispFlag == 1) {
            screenTrans();
        }
    }
}

// Task entry of the card screen: creates the cCard and CardID, loads the language font and the
// memcard message layouts, runs MainLoop, tears everything down.
void CardMainTask(int mode)
{
    SysFlagOn(pG, SYS_TYPEWRITER);
    pCard = new cCard;
    g_id = new CardID;
    if (pSys->language == 0) {
        cMes.loadFont(28, 28, "Font/common_j.fnt", 0);
    } else {
        cMes.loadFont(32, 32, "Font/common_p.fnt", 0);
    }
    cMes.setLanguage(pSys->language);
    cMes.setLayout(0, LAYOUT_MEMCARD);
    cMes.setLayout(1, LAYOUT_MEMCARD);
    cMes.setLayout(2, LAYOUT_MEMCARD);
    pCard->MainLoop(mode);
    if (pCard) {
        delete pCard;
    }
    pCard = 0;
    delete g_id;
    g_id = 0;
    cMes.setLayout(0, 0);
    cMes.setLayout(1, 0);
    cMes.setLayout(2, 0);
    SysFlagOff(pG, SYS_TYPEWRITER);
    TaskExit();
}

// Starts the load screen as a task and waits for it; 1 when a file was loaded.
int CardLoad()
{
    int ret = 0;

    systemVISetBlack(0);
    TaskExec(1, (TaskFunc) CardMainTask, 0);
    TaskSleep(1);
    if (pG->CardStatus & 4) {
        SndAllFadeOut();
        ret = 1;
    }
    return ret;
}

// Starts the save screen (save slot `no` preselected, `f` the save flags) and waits for it.
void CardSave(int no, int f)
{
    if (f & 2) {
        pG->CardStatus |= 8;
    }
    if (f & 4) {
        pG->CardStatus |= 0x10;
    }
    if (f & 8) {
        pG->CardStatus |= 0x20;
    }
    if (f & 0x10) {
        pG->CardStatus |= 0x40;
    }
    if (f & 0x20) {
        pG->CardStatus |= 0x98;
    }
    pG->terminal_no = no;
    TaskExec(1, (TaskFunc) CardMainTask, 1);
    TaskSleep(1);
}

// Saves the system file (options) through the save task and waits.
void CardSysSave()
{
    pG->CardStatus |= 0x98;
    TaskExec(1, (TaskFunc) CardMainTask, 1);
    TaskSleep(1);
}

// Boot: chains the first-check card screen.
void CardFirstCheck()
{
    if (pRK->reset_flag != 0 && pRK->MemcardCheckDone == 1) {
        pG->CardStatus |= 0x80000000;
        TaskExit();
    }
    TaskChain((TaskFunc) CardMainTask, 2);
}

// Probes slot `chan` (CARDProbeEx): records size / sector size or the error flag (no card, wrong
// device, fatal, bad sector size). 1 when a usable card is there (the host disk always).
int cCard::existCheck(int slot, CardSlot* s)
{
    int ret = 0;

    if (slot == 2) {
        return 1;
    }
    m_ResultCode = CARDProbeEx(slot, &s->memSize, &s->sectorSize);
    switch (m_ResultCode) {
    case 0:
        ret = 1;
        if (s->sectorSize != 0x2000) {
            s->flags |= 0x80;
            ret = -1;
            m_ResultCode = -0x200;
        }
        break;
    case -3:
        s->flags |= 2;
        ret = -1;
        break;
    case -2:
        s->flags |= 0x20;
        ret = -1;
        break;
    case -0x80:
        s->flags |= 0x40;
        ret = -1;
        break;
    case -1:
        break;
    }
    return ret;
}

// Async step: mounts the card (CARDMountAsync), setting the slot's error flags on failure. 1 when
// mounted.
int cCard::mount(u8* Rno, CardSlot* s)
{
    int ret = 0;

    if (s->chan == 2) {
        return 1;
    }
    switch (*Rno) {
    case 0:
        CARDMountAsync(s->chan, s->workArea, 0, 0);
        (*Rno)++;
        // fallthrough
    case 1:
        m_ResultCode = CARDGetResultCode(s->chan);
        switch (m_ResultCode) {
        case 0:
        case -6:
        case -0xD:
            OSReport("Slot %c Mount\n", s->chan + 'A');
            ret = 1;
            break;
        case -3:
            s->flags |= 2;
            ret = -1;
            break;
        case -2:
            s->flags |= 0x20;
            ret = -1;
            break;
        case -5:
        case -0x80:
            s->flags |= 0x40;
            ret = -1;
            break;
        case -1:
            break;
        }
        break;
    }
    if (ret != 0) {
        *Rno = 0;
    }
    return ret;
}

// Unmounts slot `chan`; 1 when done.
int cCard::unmount(int slot)
{
    int ret = 0;

    m_ResultCode = CARDUnmount(slot);
    switch (m_ResultCode) {
    case -3:
        ret = 1;
        break;
    case -1:
        break;
    case 0:
        ret = 1;
        break;
    default:
        ret = 1;
        break;
    }
    if (ret == 1) {
        OSReport("Slot %c Unmount\n", slot + 'A');
        m_Slot[slot].flags = 0;
    }
    return ret;
}

// Async step: CARDCheckAsync (file system check); broken -> flag 0x10.
int cCard::verifyCheck(u8* Rno, CardSlot* s)
{
    int ret = 0;

    if (s->chan == 2) {
        return 1;
    }
    switch (*Rno) {
    case 0:
        CARDCheckAsync(s->chan, 0);
        (*Rno)++;
        // fallthrough
    case 1:
        m_ResultCode = CARDGetResultCode(s->chan);
        switch (m_ResultCode) {
        case 0:
            *Rno = 0;
            ret = 1;
            break;
        case -6:
        case -0xD:
            s->flags |= 0x10;
            ret = -1;
            break;
        case -3:
            s->flags |= 2;
            ret = -1;
            break;
        case -5:
        case -0x80:
            s->flags |= 0x40;
            ret = -1;
            break;
        case -1:
            break;
        }
        break;
    }
    if (ret != 0) {
        *Rno = 0;
    }
    return ret;
}

// Reads the free blocks / files of the card and flags "no space" (0x4 with 0x400 / 0x800 for
// which file) when a save or the system file would not fit.
int cCard::freeCheck(u8* Rno, CardSlot* s)
{
    int ret = 0;

    if (s->chan == 2) {
        return 1;
    }
    switch (*Rno) {
    case 0:
        switch (CARDFreeBlocks(s->chan, &s->freeBytes, &s->freeFiles)) {
        case 0:
            (*Rno)++;
            break;
        case -6:
            s->flags |= 0x10;
            ret = -1;
            break;
        case -3:
            s->flags |= 2;
            ret = -1;
            break;
        case -0x80:
            s->flags |= 0x40;
            ret = -1;
            break;
        case -1:
            break;
        }
        break;
    case 1: {
        u32 f = s->flags & 0x300;
        if (f == 0x200) {
            if (s->freeFiles > 0 && (u32) (s->freeBytes + 0x1FFF) / 0x2000 >= m_SaveSize) {
                ret = 1;
            } else {
                ret = -1;
                m_ResultCode = -0x201;
                s->flags |= 0x804;
            }
        } else if (f == 0x100) {
            if (s->freeFiles > 0 && (u32) (s->freeBytes + 0x1FFF) / 0x2000 >= m_SysSize) {
                ret = 1;
            } else {
                ret = -1;
                m_ResultCode = -0x201;
                s->flags |= 0x404;
            }
        } else if (f == 0) {
            if (s->freeFiles > 1 && (u32) (s->freeBytes + 0x1FFF) / 0x2000 >= m_SaveSize + m_SysSize) {
                ret = 1;
            } else {
                ret = -1;
                m_ResultCode = -0x201;
                s->flags |= 4;
            }
        } else {
            ret = 1;
        }
        break;
    }
    }
    if (ret != 0) {
        *Rno = 0;
    }
    return ret;
}

// Opens `fileName` on the slot; 1 when open, error flags otherwise.
int cCard::fileOpen(CardSlot* s)
{
    int ret = 0;

    m_ResultCode = CARDOpen(s->chan, m_Name, &s->fileInfo);
    switch (m_ResultCode) {
    case 0:
    case -4:
        ret = 1;
        break;
    case -0x80:
        s->flags |= 0x40;
        ret = -1;
        break;
    case -3:
        s->flags |= 2;
        ret = -1;
        break;
    case -6:
    case -0xA:
        ret = -1;
        break;
    case -1:
        break;
    }
    return ret;
}

// Closes the slot's open file.
int cCard::fileClose(CardSlot* s)
{
    int ret = 0;

    m_ResultCode = CARDClose(&s->fileInfo);
    switch (m_ResultCode) {
    case 0:
        ret = 1;
        break;
    case -3:
        s->flags |= 2;
        ret = -1;
        break;
    case -0x80:
        s->flags |= 0x40;
        ret = -1;
        break;
    case -1:
        break;
    }
    return ret;
}

// Async step over the 20 save files: opens each, reads its 0x200 header into pInfo[], verifies
// the header CRC / version (fileFlag bits 1 exists, 2 corrupt, 4 wrong version) and records the
// card serial. 1 when all files were checked.
int cCard::saveFileCheck(u8* Rno, CardSlot* s)
{
    int ret = 0;
    int r;
    int bit;

    switch (*Rno) {
    case 0:
        CfgFlagOff(pSys, CFG_06);
        m_SaveNo = 0;
        memclr_asm(s->fileFlag, sizeof(s->fileFlag));
        m_RetryCtr = 0;
        (*Rno)++;
        // fallthrough
    case 1:
        if (m_SaveNo == 20) {
            m_SaveNo = 0;
            ret = 1;
            *Rno = 0;
            break;
        }
        if (s->chan == 2) {
            int dbg = 1;
            if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                SysFlagOn(pG, SYS_SN_PC_READ);
            }
            sprintf(m_Name, "d:\\bio4/room/savedata%02d.dat", m_SaveNo);
            if (file_exist(m_Name)) {
                CfgFlagOn(pSys, CFG_06);
                s->fileFlag[m_SaveNo] |= 1;
                bit = 1 << m_SaveNo;
                if (DBG_CACHED & bit) {
                    memcpy(m_pSaveInfo[m_SaveNo], pDbgSaveInfo[m_SaveNo], 0x200);
                    OSReport("save Info data%d from cache.\n", m_SaveNo);
                } else {
                    HDReadSeekLen(m_Name, m_pSaveInfo[m_SaveNo], 0x2000, 0x200);
                    memcpy(pDbgSaveInfo[m_SaveNo], m_pSaveInfo[m_SaveNo], 0x200);
                    isDbgInfoCached |= 1 << m_SaveNo;
                    OSReport("save Info data%d cached.\n", m_SaveNo);
                }
                if (((SaveInfo*) m_pSaveInfo[m_SaveNo])->magic != 0x116) {
                    s->fileFlag[m_SaveNo] |= 4;
                }
            } else {
                memclr_asm(pDbgSaveInfo[m_SaveNo], 0x200);
            }
            if (dbg == 0) {
                SysFlagOff(pG, SYS_SN_PC_READ);
            }
            m_SaveNo++;
        } else {
            sprintf(m_Name, "bh4_data%02d", m_SaveNo);
            r = fileOpen(s);
            if (r == 0) {
            } else if (r > 0) {
                if (m_ResultCode == 0) {
                    CfgFlagOn(pSys, CFG_06);
                    s->fileFlag[m_SaveNo] |= 1;
                    s->flags |= 0x100;
                    if (m_aMode == 2) {
                        *Rno = 3;
                    } else {
                        (*Rno)++;
                    }
                } else {
                    m_RetryCtr = 0;
                    m_SaveNo++;
                }
            } else if (r < 0) {
                if (m_aMode == 2) {
                    m_Rno0 = 0;
                    m_Rno1 = 0;
                } else {
                    errorSet(m_ResultCode);
                }
            }
        }
        break;
    case 2:
        r = CARDGetStatus(m_SlotNo, m_Slot[m_SlotNo].fileInfo.fileNo, &m_Slot[m_SlotNo].stat);
        if (r == -1) {
            break;
        }
        if (r == 0) {
            if (s->stat.commentAddr == 0xFFFFFFFF) {
                s->fileFlag[m_SaveNo] |= 2;
                *Rno = 4;
            } else {
                (*Rno)++;
            }
        } else {
            errorSet(r);
        }
        break;
    case 3:
        r = fileRead(&m_Rno3, m_pSaveInfo[m_SaveNo], 0x200, 0x2000, s);
        if (r == 0) {
        } else if (r > 0) {
            if (CRCVerify(m_pSaveInfo[m_SaveNo] + 4, 0x1FC, ((SaveInfo*) m_pSaveInfo[m_SaveNo])->crc) == 0) {
                if (m_RetryCtr == 3) {
                    s->fileFlag[m_SaveNo] |= 2;
                    (*Rno)++;
                } else {
                    m_RetryCtr++;
                }
            } else {
                (*Rno)++;
                if (((SaveInfo*) m_pSaveInfo[m_SaveNo])->magic != 0x116) {
                    s->fileFlag[m_SaveNo] |= 6;
                }
            }
        } else {
            if (m_RetryCtr == 3) {
                s->fileFlag[m_SaveNo] |= 2;
                (*Rno)++;
            } else {
                m_RetryCtr++;
            }
        }
        break;
    case 4:
        if (fileClose(s) != 0) {
            *Rno = 1;
            m_RetryCtr = 0;
            m_SaveNo++;
        }
        break;
    }
    return ret;
}

// Async step: looks for the system file (flag 0x200 when present) and reads its status.
int cCard::systemFileCheck(u8* Rno, CardSlot* s)
{
    int ret = 0;
    int r;

    if (s->chan == 2) {
        return 1;
    }
    switch (*Rno) {
    case 0:
        m_SaveNo = 0;
        sprintf(m_Name, "bh4_system");
        (*Rno)++;
        // fallthrough
    case 1:
        r = fileOpen(s);
        if (r == 0) {
        } else if (r > 0) {
            if (m_ResultCode == 0) {
                (*Rno)++;
                s->flags |= 0x200;
            } else {
                ret = 1;
                *Rno = 0;
            }
        } else if (r < 0) {
            if (m_aMode == 2) {
                m_Rno0 = 0;
                m_Rno1 = 0;
            } else {
                errorSet(m_ResultCode);
            }
        }
        break;
    case 2:
        if (fileClose(s) != 0) {
            ret = 1;
            *Rno = 0;
        }
        break;
    }
    return ret;
}

// Async step: reads `len` bytes at `ofs` of the open file; 1 done, negative on error.
int cCard::fileRead(u8* Rno, void* addr, s32 size, s32 offset, CardSlot* s)
{
    int ret = 0;

    switch (*Rno) {
    case 0:
        CARDReadAsync(&s->fileInfo, addr, size, offset, 0);
        (*Rno)++;
        // fallthrough
    case 1:
        m_ResultCode = CARDGetResultCode(s->chan);
        if (m_ResultCode == -1) {
            break;
        }
        if (m_ResultCode == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        DCFlushRange(addr, size);
        *Rno = 0;
    }
    return ret;
}

// Async step: writes `blocks` x 8 KB from `buf` to the open file; 1 done.
int cCard::fileWrite(u8* Rno, void* addr, int wblock, CardSlot* s)
{
    int ret = 0;

    switch (*Rno) {
    case 0:
        CARDWriteAsync(&s->fileInfo, addr, wblock << 13, 0, 0);
        (*Rno)++;
        // fallthrough
    case 1:
        m_ResultCode = CARDGetResultCode(s->chan);
        if (m_ResultCode == -1) {
            break;
        }
        if (m_ResultCode == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        *Rno = 0;
    }
    return ret;
}

// Reads and validates the system file (status, CRC, version) into pSysBuf and applies the saved
// options (sysFlags); errMode selects how failures are reported. 1 when read.
int cCard::sysfileRead(u8* Rno0, u8* Rno1, int err_set)
{
    int ret = 0;
    int r;

    switch (*Rno0) {
    case 0:
        if (m_SlotNo == 2) {
            *Rno0 = 2;
            break;
        }
        r = fileOpen(&m_Slot[m_SlotNo]);
        if (r == 0) {
        } else if (r > 0) {
            (*Rno0)++;
        } else if (r < 0) {
            if (err_set != 0) {
                errorSet(-0x206);
                return 0;
            }
            ret = -1;
        }
        break;
    case 1:
        r = CARDGetStatus(m_SlotNo, m_Slot[m_SlotNo].fileInfo.fileNo, &m_Slot[m_SlotNo].stat);
        if (r == -1) {
            break;
        }
        if (r != 0) {
            if (err_set != 0) {
                errorSet(-0x206);
                return 0;
            }
            ret = -1;
            break;
        }
        if (m_Slot[m_SlotNo].stat.commentAddr != 0xFFFFFFFF) {
            (*Rno0)++;
        } else {
            if (err_set != 0) {
                errorSet(-0x202);
                return 0;
            }
            ret = -1;
        }
        break;
    case 2:
        if (m_SlotNo == 2) {
            int dbg = 1;
            if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                SysFlagOn(pG, SYS_SN_PC_READ);
            }
            sprintf(m_Name, "d:\\bio4/room/sysdata.dat");
            r = HDRead(m_Name, pSysBuf);
            if (dbg == 0) {
                SysFlagOff(pG, SYS_SN_PC_READ);
            }
            if (r == 0) {
                ret = -1;
            } else {
                *Rno0 = 3;
            }
        } else {
            r = fileRead(Rno1, pSysBuf, m_SysSize << 13, 0, &m_Slot[m_SlotNo]);
            if (r == 0) {
            } else if (r > 0) {
                (*Rno0)++;
            } else {
                if (err_set != 0) {
                    errorSet(-0x206);
                    return 0;
                }
                ret = -1;
            }
        }
        break;
    case 3:
        if (CRCVerify(pSysBuf, SYS_CRC, *(u32*) (pSysBuf + SYS_CRC)) == 0) {
            if (m_RetryCtr == 3) {
                if (err_set != 0) {
                    errorSet(-0x202);
                    return 0;
                }
                ret = -1;
            } else {
                *Rno1 = 1;
                m_RetryCtr++;
            }
        } else {
            if (m_SlotNo == 2) {
                ret = 1;
            } else {
                (*Rno0)++;
            }
        }
        break;
    case 4:
        r = fileClose(&m_Slot[m_SlotNo]);
        if (r == 0) {
        } else if (r > 0) {
            ret = 1;
        } else if (r < 0) {
            if (err_set != 0) {
                errorSet(-0x206);
                return 0;
            }
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        *Rno0 = 0;
        *Rno1 = 0;
    }
    return ret;
}

// Rno0 == 8 (first check): asks whether to create the system file, then creates and writes it
// (saveMain with isSystem), or continues without one.
void cCard::createSysfile()
{
    int noCard = 0;
    int ret;
    int sel;

    ret = CARDProbeEx(m_SlotNo, 0, 0);
    sprintf(m_Name, "bh4_system");
    switch (m_Rno1) {
    case 0:
        cardMesSet(MES_SYS_MAKE_CONFIRM, 0, 0x800000);
        cMes.m_Msg[0].m_cur = 1;
        m_Rno1++;
        // fallthrough
    case 1:
        if (ret == -3) {
            noCard = 1;
            break;
        }
        sel = cMes.GetSelectMessage(0);
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            cardMesSet(MES_SYS_SAVING, 0, 0);
            SysFlagOn(pG, SYS_CARD_ACCESS);
            m_Rno1++;
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            m_ErrCode = -0x208;
            m_Rno0 = 5;
            m_Rno1 = sel;
            m_Rno2 = 0;
            m_Rno3 = 0;
            cMes.Clear();
            break;
        }
        break;
    case 2:
        ret = mount(&m_Rno2, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            errorSet(m_ResultCode);
        }
        break;
    case 3:
        ret = fileCreate(&m_Rno2, m_SysSize, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno2 = 0;
            m_Rno3 = 0;
            m_Rno1++;
        } else if (ret < 0) {
            if (m_ResultCode == -5) {
                errorSet(-5);
            } else {
                errorSet(-0x205);
            }
        }
        break;
    case 4:
        ret = CARDGetStatus(m_SlotNo, m_Slot[m_SlotNo].fileInfo.fileNo, &m_Slot[m_SlotNo].stat);
        if (ret == -1) {
            break;
        }
        if (ret == 0) {
            m_Rno1++;
        } else {
            errorSet(-0x205);
        }
        break;
    case 5:
        makeSystemSaveData();
        m_Rno1++;
        // fallthrough
    case 6:
        ret = fileWrite(&m_Rno2, pSysBuf, m_SysSize, &m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Rno1++;
        } else if (ret < 0) {
            if (m_ResultCode == -5) {
                errorSet(-5);
            } else {
                errorSet(-0x205);
            }
        }
        break;
    case 7:
        makeCardStatus(&m_Slot[m_SlotNo]);
        CARDSetStatusAsync(m_SlotNo, m_Slot[m_SlotNo].fileInfo.fileNo, &m_Slot[m_SlotNo].stat, 0);
        m_Rno1++;
        // fallthrough
    case 8:
        ret = CARDGetResultCode(m_SlotNo);
        switch (ret) {
        case -1:
            break;
        case 0:
            m_Rno1++;
            break;
        case -5:
            errorSet(-5);
            break;
        default:
            errorSet(-0x205);
            break;
        }
        break;
    case 9:
        ret = fileClose(&m_Slot[m_SlotNo]);
        if (ret == 0) {
        } else if (ret > 0) {
            m_Timer = 0xF;
            m_Rno1++;
            cardMesSet(MES_SYS_SAVE_DONE, 0, 0);
        } else if (ret < 0) {
            errorSet(-0x205);
        }
        break;
    case 10:
        if (m_Timer == 0) {
            cMes.Clear();
            SysFlagOff(pG, SYS_CARD_ACCESS);
            m_Rno0 = 4;
            m_Rno1 = 0;
            m_Rno2 = 0;
            m_Rno3 = 0;
        } else {
            m_Timer--;
        }
        break;
    }
    if (noCard == 1) {
        m_Rno0 = 0;
        m_Rno1 = 0;
        m_Rno2 = 0;
        m_Rno3 = 0;
    }
}

// Debug: prints the state numbers (dev mode).
void cCard::screenTrans()
{
    if (m_aMode != 2) {
        debugInfoDisp(m_SlotNo, m_aMode);
    }
    eprintf(24, 16, 0, 0, "%02d%02d%02d%02d", m_Rno0, m_Rno1, m_Rno2, m_Rno3);
    if (m_aMode != 2 && !(pG->CardStatus & 0x80)) {
        g_id->move(this);
        g_id->m_IdSave.move();
        g_id->m_IdSave.trans();
    }
}

// Shows memcard message `no` from the message table at its layout position in window `slot`.
void cCard::cardMesSet(int mes_no, int wk_no, u32 attr)
{
    MesPos* p = &mes_pos_tbl[pSys->language][mes_no];
    cMes.MesSet(p->no, p->x, p->y, attr | 0x01020051, wk_no, 0, 4);
}

// Relocates an in-file TPL in place (offsets -> pointers).
void cCard::calcTplAddr(TEXPalette* tpl)
{
    u32 i;
    TEXDescriptor* desc;

    if ((s32) tpl->descriptorArray < 0) {
        return;
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
    desc = tpl->descriptorArray;
    for (i = 0; i < tpl->numDescriptors; i++, desc++) {
        desc->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) desc->textureHeader);
        desc->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) desc->CLUTHeader);
        if (desc->textureHeader->unpacked == 0) {
            desc->textureHeader->data = (u8*) tpl + (u32) desc->textureHeader->data;
            desc->textureHeader->unpacked = 1;
        }
        if (desc->CLUTHeader->unpacked == 0) {
            desc->CLUTHeader->data = (u8*) tpl + (u32) desc->CLUTHeader->data;
            desc->CLUTHeader->unpacked = 1;
        }
    }
}

// Shows / hides the message window backdrop ids.
void cCard::setMsgWindow(int a, int sw)
{
    if (m_aMode != 2) {
        if (pG->CardStatus & 0x80) {
            Cckpt.msgWindow(sw);
        } else {
            setMsgBG(a, sw);
        }
    }
}

// Builds the CRC-32 table used for the save file checksums.
void CRCInit()
{
    u32 i;
    int j;

    for (i = 0; i < 256; i++) {
        u32 c = i << 24;
        for (j = 0; j < 8; j++) {
            if ((s32) c < 0) {
                c = (c << 1) ^ 0x04C11DB7;
            } else {
                c = c << 1;
            }
        }
        CRCTable[i] = c;
    }
}

// CRC-32 of `len` bytes.
u32 CRCCalc(u8* bufPtr, u32 nByte)
{
    u32 crc = 0;
    u32 i;

    for (i = 0; i < nByte; i++) {
        crc = (crc << 8) ^ CRCTable[(crc >> 24) ^ bufPtr[i]];
    }
    return crc;
}

// 1 when the CRC of the data matches `saved` (logs both on mismatch).
int CRCVerify(u8* bufPtr, u32 nByte, u32 ChkCRC)
{
    u32 crc = CRCCalc(bufPtr, nByte);
    if (ChkCRC == crc) {
        return 1;
    }
    OSReport("CRCVerifyCRC Error: CRCs doesn't match!!\nSaved CRC  = 0x%x\nActual CRC = 0x%x\n", ChkCRC, crc);
    return 0;
}

// Dev mode: allocates the debug copy of the 20 save headers (so the host disk saves show their
// info).
void CardDbgCacheSet()
{
    u8* p;
    int i;

    if (pG->IsDevConsole == 1 && isDbgInfoAlloc == 0) {
        p = (u8*) Debug_alloc(0x2800, 0);
        if (p != 0) {
            memclr_asm(p, 0x2800);
            for (i = 19; i >= 0; i--) {
                pDbgSaveInfo[i] = p + i * 0x200;
            }
            isDbgInfoAlloc = 1;
        }
    }
}

// Digits of `num` into id units idNo, idNo-1, ... (ones first). A macro: every expansion shares
// dispSaveInfo's `u` (the unit pointer is copied to the same register each time).
#define putNumber(id, num_, idNo_, digits_, type)         \
    {                                                     \
        int d[3];                                         \
        int i;                                            \
        num = (num_);                                     \
        for (i = 0; i <= (digits_) - 1; i++) {            \
            d[i] = num % 10;                              \
            num /= 10;                                    \
            u = (id)->unitPtr((idNo_) - i, type);         \
            u->tex_flag |= 2;                             \
            u->texNo = d[i];                                 \
        }                                                 \
    }

// Fills save slot `no`'s list entry ids: chapter / difficulty / play time / save count digits
// from the header (or the "broken" / "no data" variants).
void dispSaveInfo(int no, SaveInfo* p_info, int type, int flag)
{
    IDSystem* id = &g_id->m_IdSave;
    IdUnit* u;
    int chapter;
    int special;
    int chap;
    int sec;
    u32 h;
    u32 m;
    u32 s;
    int num;

    putNumber(id, no + 1, 2, 2, type);
    u = id->unitPtr(0x16, type);
    if (p_info == 0) {
        u->be_flag &= ~8;
        return;
    }
    u->be_flag |= 8;
    special = 0;
    chapter = p_info->chapter;
    id->unitPtr(0x20, type)->be_flag &= ~8;
    id->unitPtr(7, type)->be_flag &= ~8;
    id->unitPtr(6, type)->be_flag &= ~8;
    id->unitPtr(0x21, type)->be_flag &= ~8;
    id->unitPtr(0x19, type)->be_flag &= ~8;
    if (flag) {
        u = id->unitPtr(0x20, type);
    } else {
        switch (p_info->mode) {
        case 1:
            if (chapter == 0x12) {
                u = id->unitPtr(0x19, type);
                special = 1;
            } else {
                u = id->unitPtr(7, type);
            }
            break;
        case 2:
            chapter--;
            u = id->unitPtr(6, type);
            break;
        case 3:
            u = id->unitPtr(0x21, type);
            special = 1;
            break;
        default:
            goto skip;
        }
    }
    u->be_flag |= 8;
skip:
    getChapterSection(chapter, &chap, &sec);
    u = id->unitPtr(5, type);
    u->tex_flag |= 2;
    u->texNo = sec;
    u = id->unitPtr(3, type);
    u->tex_flag |= 2;
    u->texNo = chap;
    if (flag || special) {
        id->unitPtr(3, type)->be_flag &= ~8;
        id->unitPtr(4, type)->be_flag &= ~8;
        id->unitPtr(5, type)->be_flag &= ~8;
    } else {
        id->unitPtr(3, type)->be_flag |= 8;
        id->unitPtr(4, type)->be_flag |= 8;
        id->unitPtr(5, type)->be_flag |= 8;
    }
    putNumber(id, p_info->save_cnt, 0xA, 3, type);
    if (flag) {
        id->unitPtr(8, type)->be_flag &= ~8;
        id->unitPtr(9, type)->be_flag &= ~8;
        id->unitPtr(0xA, type)->be_flag &= ~8;
    } else {
        id->unitPtr(8, type)->be_flag |= 8;
        id->unitPtr(9, type)->be_flag |= 8;
        id->unitPtr(0xA, type)->be_flag |= 8;
    }
    SecToTime(p_info->playTime, &h, &m, &s);
    putNumber(id, h, 0xC, 2, type);
    u = id->unitPtr(0xD, type);
    u->texNo = 0xB;
    u->tex_flag |= 2;
    putNumber(id, m, 0xF, 2, type);
    u = id->unitPtr(0x10, type);
    u->texNo = 0xB;
    u->tex_flag |= 2;
    putNumber(id, s, 0x12, 2, type);
    if (flag) {
        id->unitPtr(0xB, type)->be_flag &= ~8;
        id->unitPtr(0xC, type)->be_flag &= ~8;
        id->unitPtr(0xD, type)->be_flag &= ~8;
        id->unitPtr(0xE, type)->be_flag &= ~8;
        id->unitPtr(0xF, type)->be_flag &= ~8;
        id->unitPtr(0x10, type)->be_flag &= ~8;
        id->unitPtr(0x11, type)->be_flag &= ~8;
        id->unitPtr(0x12, type)->be_flag &= ~8;
    } else {
        id->unitPtr(0xB, type)->be_flag |= 8;
        id->unitPtr(0xC, type)->be_flag |= 8;
        id->unitPtr(0xD, type)->be_flag |= 8;
        id->unitPtr(0xE, type)->be_flag |= 8;
        id->unitPtr(0xF, type)->be_flag |= 8;
        id->unitPtr(0x10, type)->be_flag |= 8;
        id->unitPtr(0x11, type)->be_flag |= 8;
        id->unitPtr(0x12, type)->be_flag |= 8;
    }
    num = p_info->count + 1;
    if (p_info->mode == 3) {
        num = p_info->count;
    }
    putNumber(id, num, 0x14, 2, type);
    if (flag) {
        id->unitPtr(0x13, type)->be_flag &= ~8;
        id->unitPtr(0x14, type)->be_flag &= ~8;
    } else {
        id->unitPtr(0x13, type)->be_flag |= 8;
        id->unitPtr(0x14, type)->be_flag |= 8;
    }
    id->unitPtr(0x22, type)->be_flag &= ~8;
    id->unitPtr(0x17, type)->be_flag &= ~8;
    id->unitPtr(0x18, type)->be_flag &= ~8;
    if (pSys->language == 0) {
        switch (p_info->game_mode_disp) {
        case 1:
            id->unitPtr(0x22, type)->be_flag |= 8;
            break;
        case 3:
        default:
            id->unitPtr(0x17, type)->be_flag |= 8;
            break;
        case 5:
            id->unitPtr(0x18, type)->be_flag |= 8;
            break;
        }
    } else if (pSys->language == 1) {
        switch (p_info->game_mode_disp) {
        case 5:
        default:
            id->unitPtr(0x17, type)->be_flag |= 8;
            break;
        case 6:
            id->unitPtr(0x18, type)->be_flag |= 8;
            break;
        }
    } else {
        switch (p_info->game_mode_disp) {
        case 3:
            id->unitPtr(0x22, type)->be_flag |= 8;
            break;
        case 5:
        default:
            id->unitPtr(0x17, type)->be_flag |= 8;
            break;
        case 6:
            id->unitPtr(0x18, type)->be_flag |= 8;
            break;
        }
    }
}

// Refreshes every list entry from the card's file flags / headers.
void CardID::updateSaveInfo(cCard* pCard)
{
    int i;

    for (i = 0; i < 7; i++) {
        int type = 0x40 + i;
        s8 sl = pCard->getSlotNo();
        int base = pCard->getSaveNo() - 3;
        int no = base + i;
        u32 f;
        // The original loop body spans more than 100 insn slots (notes included), so haifa never
        // schedules it as one interblock region; the empty blocks add the missing BLOCK notes.
        {{{{{{{{{{{{{{{{{{{{}}}}}}}}}}}}}}}}}}}}
        if (no < 0) {
            no += 20;
        }
        if (no > 19) {
            no -= 20;
        }
        g_id->m_IdSave.unitPtr(0x15, type)->be_flag |= 8;
        f = pCard->getSlotInfo(sl)->fileFlag[no];
        if (f & 1) {
            if (f & 2) {
                dispSaveInfo(no, (SaveInfo*) pCard->getSaveInfo((s8) no), type, 1);
            } else {
                dispSaveInfo(no, (SaveInfo*) pCard->getSaveInfo((s8) no), type, 0);
            }
        } else {
            dispSaveInfo(no, 0, type, 0);
        }
    }
}

// Creates the card screen ids (background, list, cursor) from the save screen archive for mode
// `type`, killing the HUD ids.
void CardID::init(int type, CardArc* data)
{
    int i;
    IdUnit* u;
    IdUnit* v;
    f32 zero;

    this->m_mode = type;
    pTex = (u8*) (data->ofs[0] + (u32) data);
    pSaveDat = (u8*) (data->ofs[1] + (u32) data);
    pFile = (u8*) (data->ofs[2] + (u32) data);
    pFrame = (u8*) (data->ofs[3] + (u32) data);
    pLoadDat = (u8*) (data->ofs[4] + (u32) data);
    pBg = (u8*) (data->ofs[5] + (u32) data);
    m_IdSave.gameInit(0x100);
    IdTexRoomInit();
    IdTexDataLoad(pTex, TEX_OWNER_ID_EVENT);
    IdSys.kill(0xFF, IDC_TITLE);
    IdSys.kill(0xFF, IDC_TITLE_MENU);
    IdSys.kill(0xFF, IDC_LIFE_METER);
    IdSys.kill(0xFF, IDC_ACT_BUTTON);
    IdSys.kill(0xFF, IDC_COUNT_DOWN);
    IdSys.kill(0xFF, IDC_CINESCO);
    IdSys.kill(0xFF, IDC_OPTION_BG);
    IdSys.kill(0xFF, IDC_OPTION);
    m_IdSave.set(pFrame, 0xFF, IDC_SSCRN_FAR_0, 9, 3, 0);
    for (i = 0; i < 7; i++) {
        m_IdSave.set(pFile, 0xFF, 0x40 + i, 0xC, 6, 0);
    }
    if (this->m_mode == 1) {
        IdSys.set(pSaveDat, 0xFF, IDC_SSCRN_NEAR_0, 0xF, 2, 0);
        IdSys.unitPtr(1, IDC_SSCRN_NEAR_0)->rev_flag |= 0xF;
    } else if (this->m_mode == 0) {
        IdSys.set(pLoadDat, 0xFF, IDC_SSCRN_NEAR_0, 0xF, 2, 0);
    }
    IdSys.set(pBg, 0xFF, IDC_SSCRN_NEAR_1, 0xF, 1, 0);
    IdSys.unitPtr(0, IDC_SSCRN_NEAR_1)->be_flag &= ~8;
    IdSys.unitPtr(0, IDC_SSCRN_NEAR_1)->rev_flag |= 0xF;
    IdSys.unitPtr(1, IDC_SSCRN_NEAR_1)->be_flag &= ~8;
    IdSys.unitPtr(1, IDC_SSCRN_NEAR_1)->rev_flag |= 0xF;
    zero = 0.0f;
    for (int j = 0; j < 7; j++) {
        IdUnit* p = g_id->m_IdSave.unitPtr(0x15, 0x40 + j);
        IdUnit* q = g_id->m_IdSave.unitPtr((u8) (j + 0x10), IDC_SSCRN_FAR_0);
        q->type = 1;
        p->pos0.z = zero;
        p->pos0.y = zero;
        p->pos0.x = zero;
        g_id->m_IdSave.unitParent(q, p);
    }
    u = g_id->m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0);
    g_p_path_org[0] = u->path0;
    g_p_hrmt_org[0] = u->curve[0];
    g_p_spln_org[0] = u->path1;
    g_pos0_org = u->pos0;
    v = g_id->m_IdSave.unitPtr(0xA, IDC_SSCRN_FAR_0);
    u->path0 = 0;
    u->curve[0] = 0;
    u->path1 = 0;
    u->pos0 = v->pos0;
    u->rev_flag |= 0xF;
    IdSys.unitPtr(5, IDC_SSCRN_NEAR_0)->rev_flag |= 0xF;
    {
        IdUnit* w = IdSys.unitPtr(2, IDC_SSCRN_NEAR_0);
        w->texNo = 0;
        w->tex_flag |= 2;
    }
    rno0 = 0;
    rno1 = 0;
    rno2 = 0;
    rno3 = 0;
}

// Per-frame id animation: runs the current mode (wait / start / normal / up_down / save) and the
// error-screen dimming.
void CardID::move(cCard* pCard)
{
    static void (CardID::*tbl[6])(cCard*) = {
        &CardID::wait, &CardID::start, &CardID::normal, &CardID::up_down, &CardID::up_down, &CardID::save,
    };
    IdUnit* a;
    IdUnit* b;

    (this->*tbl[rno0])(pCard);
    if (rno0 == 3) {
        m_IdSave.unitPtr(1, IDC_SSCRN_FAR_0)->be_flag &= ~8;
        m_IdSave.unitPtr(0x15, IDC_NUM_00)->be_flag &= ~8;
    } else {
        m_IdSave.unitPtr(1, IDC_SSCRN_FAR_0)->be_flag |= 8;
        m_IdSave.unitPtr(0x15, IDC_NUM_00)->be_flag |= 8;
    }
    if (pCard->getRno0() == 5) {
        a = m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0);
        b = m_IdSave.unitPtr(0xA, IDC_SSCRN_FAR_0);
        if ((a->rev_flag & 0xF) == 0) {
            SndCall(0, 0x2A, 0, 0, 0, 0);
            a->path0 = b->path0;
            a->curve[0] = b->curve[0];
            a->path1 = b->path1;
            a->pos0 = b->pos0;
            FuncPathParametrize(a->path0, a->path1);
            m_IdSave.setTime(a, (s8) a->curve[0]->key[a->curve[0]->num - 1].t);
            a->rev_flag |= 0xF;
            setAction(0);
            IdSys.unitPtr(5, IDC_SSCRN_NEAR_0)->rev_flag |= 0xF;
            rno0 = 0;
        }
    }
}

// Id mode: idle list; applies a pending action (highlight / hide) to the cursor ids.
void CardID::wait(cCard* pCard)
{
    IdUnit* a;
    IdUnit* b;

    if (pCard->ckStatus(2)) {
        pCard->resetStatus(2);
        a = g_id->m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0);
        b = g_id->m_IdSave.unitPtr(0xA, IDC_SSCRN_FAR_0);
        a->path0 = b->path0;
        a->curve[0] = b->curve[0];
        a->path1 = b->path1;
        a->pos0 = b->pos0;
        FuncPathParametrize(a->path0, a->path1);
        IdSys.setTime(a, 0);
        a->rev_flag &= ~0xF;
        IdSys.unitPtr(5, IDC_SSCRN_NEAR_0)->rev_flag &= ~0xF;
        updateSaveInfo(pCard);
        rno0 = 1;
        SndCall(0, 0x2D, 0, 0, 0, 0);
    }
}

// Id mode: the list slides in, then normal.
void CardID::start(cCard* pCard)
{
    rno0 = 2;
    if (m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0)->anima_state & 1) {
        rno0 = 2;
    }
}

// Id mode: cursor on the selected file, Up / Down start the scroll animation (up_down).
void CardID::normal(cCard* pCard)
{
    IdUnit* u;

    if (getAction() & 4) {
        u = IdSys.unitPtr(1, IDC_SSCRN_NEAR_0);
        u->rev_flag &= ~0xF;
        IdSys.setTime(u, 0);
        rno0 = 5;
        SndCall(0, 0x2D, 0, 0, 0, 0);
    } else if (getAction() & 1) {
        rno0 = 3;
        updateSaveInfo(pCard);
        up_down(pCard);
    } else if (getAction() & 2) {
        rno0 = 4;
        updateSaveInfo(pCard);
        up_down(pCard);
    } else {
        u = IdSys.unitPtr(2, IDC_SSCRN_NEAR_0);
        u->tex_flag |= 2;
    }
}

// Id mode: animates the list scroll by one entry and updates the cursor / entry ids.
void CardID::up_down(cCard* pCard)
{
    IdUnit* a;
    IdUnit* b;
    IdUnit* w;

    switch (rno1) {
    case 0:
        a = m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0);
        setAction(8);
        if (rno0 == 4) {
            u8 ids[2];
            int i;
            b = m_IdSave.unitPtr(8, IDC_SSCRN_FAR_0);
            if (m_mode == 1) {
                ids[0] = 3;
                ids[1] = 4;
            } else {
                ids[0] = 1;
                ids[1] = 3;
            }
            for (i = 0; i < 2; i++) {
                IdSys.setTime(IdSys.unitPtr(ids[0], IDC_SSCRN_NEAR_0), 0);
                IdSys.setTime(IdSys.unitPtr(ids[1], IDC_SSCRN_NEAR_0), 0);
            }
            w = IdSys.unitPtr(2, IDC_SSCRN_NEAR_0);
            w->tex_ptn_no = 0;
            w->tex_flag |= 2;
        } else {
            b = m_IdSave.unitPtr(9, IDC_SSCRN_FAR_0);
            w = IdSys.unitPtr(2, IDC_SSCRN_NEAR_0);
            w->tex_ptn_no = 0;
            w->tex_flag &= ~2;
        }
        a->path0 = b->path0;
        a->curve[0] = b->curve[0];
        a->path1 = b->path1;
        a->pos0 = b->pos0;
        FuncPathParametrize(a->path0, a->path1);
        IdSys.setTime(a, 0);
        rno1++;
        break;
    case 1:
        if (m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0)->anima_state & 1) {
            IdUnit* u = m_IdSave.unitPtr(0, IDC_SSCRN_FAR_0);
            u->path0 = g_p_path_org[0];
            u->curve[0] = (Hermite1*) g_p_hrmt_org[0];
            u->path1 = g_p_spln_org[0];
            u->pos0 = g_pos0_org;
            FuncPathParametrize(u->path0, u->path1);
            IdSys.setTime(u, 0);
            setAction(0);
            rno0 = 2;
            rno1 = 0;
        } else {
            if (getAction() & 1) {
                rno0 = 3;
                rno1 = 0;
            } else if (getAction() & 2) {
                rno0 = 4;
                rno1 = 0;
            }
        }
        if (rno0 == 3) {
            w = IdSys.unitPtr(2, IDC_SSCRN_NEAR_0);
            if (w->tex_ptn_no > 4) {
                w->tex_ptn_no = 0;
                w->tex_flag |= 2;
            }
        }
        break;
    }
}

// Id mode during a save / load: the selected entry blinks, then returns to normal.
void CardID::save(cCard* pCard)
{
    IdUnit* u = IdSys.unitPtr(1, IDC_SSCRN_NEAR_0);
    Hermite1* h = u->curve[0];
    int n = ((s8*) h)[3];
    int i;

    if (pCard->getRno0() == 5) {
        u->rev_flag |= 0xF;
        return;
    }
    if (u->anima_state & 1) {
        setAction(0);
        return;
    }
    for (i = 0; i < n; i++) {
        if ((s8) h->key[i].t == (s16) u->timer[0]) {
            switch (i) {
            case 0:
                break;
            case 1:
                updateSaveInfo(pCard);
                break;
            case 2:
            case 3:
                break;
            default:
                SndCall(0, 0x2E, 0, 0, 0, 0);
                break;
            }
            break;
        }
    }
}

// Kills the card screen ids.
void CardID::quit()
{
    m_IdSave.free();
    if (!StaFlagChk(pG, STA_TITLE)) {
        Cckpt.roomInit();
    }
}

// Queues a cursor action (bit0 show, bit1 hide, bit2 highlight) for the id modes.
void CardID::setAction(int a)
{
    m_act_flag = a;
}

// Shows / hides the message backdrop id `a`.
void setMsgBG(int a, int flag)
{
    IdUnit* u;

    if (a == 0) {
        u = IdSys.unitPtr(0, IDC_SSCRN_NEAR_1);
    } else {
        u = IdSys.unitPtr(1, IDC_SSCRN_NEAR_1);
    }
    switch (flag) {
    case 1:
        u->be_flag |= 8;
        u->rev_flag &= ~0xF;
        break;
    case 0:
        u->rev_flag |= 0xF;
        break;
    }
}

asm(".section .sdata; .balign 32");
