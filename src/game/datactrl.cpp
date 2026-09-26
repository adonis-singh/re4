// game/datactrl: streamed room data units in MRAM/ARAM (D:/Bio4/Prog/datactrl.cpp).
// All functions byte-identical (the static-initializer trio is only named differently); .rodata and
// all section sizes match.
//  - dispDebug: `x1` is ONE function-scope variable shared by the loop's second conversion and the
//    `over` block's bar width, so the allocno's conflicts are the union of both ranges (r0/r9/r11/r10/r8
//    temps of the over block, r6 = x0, r7 = the `x1 - x0` temp) and it lands in r5; a block-scoped x1
//    in the loop conflicts with nothing and takes the r7 preference set_preference inherits from the
//    local-alloc'd `subf` temp. The loop's `x0` stays block-scoped (p=r31 via reusing `p` for the tiles).
#include "types.h"
#include "global.h"
#include "datactrl.h"
#include "dvd.h"
#include "main.h"
#include "main_mem.h"
#include "db_log.h"
#include "eprintf.h"
#include "libgpu.h"
#include "snd.h"
#include <string.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os.h>

// Not the do { } while (0) form of the other units: the OSReport stays in the caller's block and
// the preceding pLog->err argument loads are scheduled against it (setData).

// The debug bar primitive is 0x20 bytes here (tile[2] is 0x40).
struct DcTile {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 w, h;         // 0x10
    s16 z0;           // 0x14
    u8 pad_16[0xA];
};

#define ARAM_END 0xD00000

cDataCtrl DC;

// Stores the unit's file name (at most 31 chars; longer is a fatal error), "" for NULL.
#line 52 "D:/Bio4/Prog/datactrl.cpp"
inline void cDataUnit::setName(char* s)
{
    if (s != NULL) {
        if (strlen(s) > 0x1F) {
            pLog->err(0, 0, "DATANAME STRING OVER: %s", s);
            HALT();
        }
        strcpy(m_name, s);
    } else {
        m_name[0] = 0;
    }
}

// Queues a command (1 load to MRAM, 2 load to ARAM, 3 clear, 4 delete) with the destination
// address `arg` (0 = allocate) and the synchronous flag `wait`; executed at once unless the
// controller holds commands back (m_nblock_read_stop).
void cDataUnit::setCommand(int cmd, u32 arg, u8 wait)
{
    m_command = cmd;
    this->m_arg_addr = arg;
    this->m_mode = wait;
    if (wait != 0 || DC.checkNBlkStop() != 1) {
        checkCommand();
    }
}

// The pending command.
int cDataUnit::getCommand()
{
    return m_command;
}

// Sets the unit state (see the condition codes in datactrl.h).
void cDataUnit::setCondition(int c)
{
    m_condition = c;
}

// The unit state: 0 none, 1 / 2 MRAM loading / ok, 3 / 4 ARAM loading / ok, 5..8 transfers.
int cDataUnit::getCondition()
{
    return m_condition;
}

// Frees the MRAM block the unit allocated for itself (m_be_flag bit1), if any.
void cDataUnit::checkMallocRelease()
{
    if (isBeMalloc() == 1) {
        if (DC.checkUseDebugMemFlag() == 1) {
            Debug_free_h(m_malloc_addr, m_malloc_heap);
        } else {
            Mem_free_h(m_malloc_addr, m_malloc_heap);
        }
        setBeMalloc(0);
    }
}

// Records (on) or forgets the unit-owned allocation `p` and its heap.
void cDataUnit::setMallocInfo(int set, void* malloc_addr)
{
    if (isBeMalloc() == 1) {
        checkMallocRelease();
    }
    if (set == 1) {
        setBeMalloc(1);
    } else {
        setBeMalloc(0);
    }
    m_malloc_addr = malloc_addr;
    if (DC.checkUseDebugMemFlag() == 1) {
        m_malloc_heap = MemGetCurrentDbgHeap();
    } else {
        m_malloc_heap = MemGetCurrentHeap();
    }
}

// Pins the MRAM destination: loads go to `a` instead of allocating.
void cDataUnit::fixMramAddr(u32 a)
{
    m_fix_addr = a;
}

// 1 when the data is in MRAM and usable (condition 2); 0 while idle or elsewhere.
int cDataUnit::isUseOk()
{
    if (m_condition == COND_NO_DATA && m_command == 0) {
        setErr(5);
        return 0;
    }
    return getCondition() == COND_MRAM_OK;
}

// Blocks (TaskSleep) until the data is in MRAM; 0 when the unit is idle or the wait failed.
int cDataUnit::waitUseOk()
{
    if (m_condition == COND_NO_DATA && m_command == 0) {
        setErr(5);
        return 0;
    }
    while (isUseOk() == 0) {
        m_wait = 1;
        checkCondition();
        if (getCondition() == COND_ARAM_OK) {
            checkCommand();
        }
        if (getErr() != 0) {
            return 0;
        }
    }
    return 1;
}

// 1 when the data is resident in MRAM or ARAM (condition 2 or 4).
int cDataUnit::isLoadOk()
{
    if (m_condition == COND_NO_DATA && m_command == 0) {
        setErr(5);
        return 0;
    }
    if (m_condition == COND_MRAM_OK || m_condition == COND_ARAM_OK) {
        return 1;
    }
    return 0;
}

// Blocks until the data is resident somewhere.
int cDataUnit::waitLoadOk()
{
    if (m_condition == COND_NO_DATA && m_command == 0) {
        setErr(5);
        return 0;
    }
    while (isLoadOk() == 0) {
        m_wait = 1;
        checkCondition();
        if (getErr() != 0) {
            return 0;
        }
    }
    return 1;
}

// Executes "load to MRAM": from idle starts the DVD read into the fixed / given / allocated
// destination (condition 1; in dev mode a dummy.dat read is issued alongside), from ARAM starts
// the ARAM -> MRAM DMA (condition 5); already in MRAM just clears the command; transfers in
// flight are left alone.
void cDataUnit::setLoadToMram()
{
    int no;

    switch (m_condition) {
    case COND_MRAM_LOAD:
    case COND_ARAM_LOAD:
    case COND_ARAM_TO_MRAM:
    case COND_MRAM_TO_ARAM:
    case COND_ARAM_TO_ARAM:
    case COND_MRAM_TO_MRAM:
        break;
    case COND_NO_DATA:
        if (m_fix_addr == 0) {
            if (m_arg_addr == 0) {
                if (DC.checkUseDebugMemFlag() == 1) {
                    m_dest_addr = (u32) Debug_alloc(m_size, 1);
                } else {
#line 200 "D:/Bio4/Prog/datactrl.cpp"
                    m_dest_addr = (u32) MEM_ALLOC(m_size, 1, 0xD);
                }
                if (m_dest_addr == 0) {
                    setErr(1);
                    return;
                }
                setMallocInfo(1, (void*) m_dest_addr);
            } else {
                m_dest_addr = m_arg_addr;
                setMallocInfo(0, NULL);
            }
        } else {
            m_dest_addr = m_fix_addr;
            setMallocInfo(0, NULL);
        }
#line 222 "D:/Bio4/Prog/datactrl.cpp"
        no = DvdReadN(m_name, (void*) m_dest_addr, 0, 0, 0, m_mode | 0x10, __FILE__, __LINE__);
        if (pG->IsDevConsole == 1) {
#line 226 "D:/Bio4/Prog/datactrl.cpp"
            DC.setDummyId(DvdReadN("dummy.dat", DC.m_DummyDataMem, 0, 0, 0, m_mode | 0x10, __FILE__, __LINE__));
        }
        m_id = no;
        if (no >= 0) {
            m_command = 0;
            m_condition = COND_MRAM_LOAD;
            if (m_mode == 1) {
                checkLoadToMram();
            }
            OSReport("DC:%s set MRAM_LOAD\n", m_name);
        } else {
            setErr(2);
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToMram command error");
        }
        break;
    case COND_MRAM_OK:
        if (m_fix_addr == 0) {
            if (m_arg_addr != 0 && m_arg_addr != (u32) m_addr) {
                checkMallocRelease();
                memcpy((void*) m_arg_addr, m_addr, m_size);
                m_addr = (void*) m_arg_addr;
                DCFlushRange((void*) m_arg_addr, m_size);
            }
        } else if (m_fix_addr != (u32) m_addr) {
            checkMallocRelease();
            memcpy((void*) m_fix_addr, m_addr, m_size);
            m_addr = (void*) m_fix_addr;
            DCFlushRange((void*) m_arg_addr, m_size);
        }
        m_command = 0;
        OSReport("DC:%s set MRAM_TO_MRAM\n", m_name);
        break;
    case COND_ARAM_OK:
        if (m_fix_addr == 0) {
            if (m_arg_addr == 0) {
                if (DC.checkUseDebugMemFlag() == 1) {
                    m_dest_addr = (u32) Debug_alloc(m_size, 1);
                } else {
#line 290 "D:/Bio4/Prog/datactrl.cpp"
                    m_dest_addr = (u32) MEM_ALLOC(m_size, 1, 0xD);
                }
                if (m_dest_addr == 0) {
                    setErr(1);
                    return;
                }
                setMallocInfo(1, (void*) m_dest_addr);
            } else {
                m_dest_addr = m_arg_addr;
                setMallocInfo(0, NULL);
            }
        } else {
            m_dest_addr = m_fix_addr;
            setMallocInfo(0, NULL);
        }
        no = Aram.DmaTransReq(1, (u32) m_addr, m_dest_addr, m_size, m_mode);
        m_id = no;
        if (no >= 0) {
            m_command = 0;
            m_condition = COND_ARAM_TO_MRAM;
            if (m_mode == 1) {
                checkAramToMram();
            }
            OSReport("DC:%s set ARAM_TO_MRAM\n", m_name);
        } else {
            setErr(3);
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToMram command error");
        }
        break;
    }
}

// Executes "load to ARAM": from idle a DVD read straight to ARAM (condition 3, address from
// getAramFree), from MRAM the MRAM -> ARAM DMA (condition 6) freeing the MRAM copy afterwards;
// already in ARAM clears the command.
void cDataUnit::setLoadToAram()
{
    int no;

    switch (m_condition) {
    case COND_MRAM_LOAD:
    case COND_ARAM_LOAD:
    case COND_ARAM_TO_MRAM:
    case COND_MRAM_TO_ARAM:
    case COND_ARAM_TO_ARAM:
    case COND_MRAM_TO_MRAM:
        break;
    case COND_NO_DATA:
        if (m_arg_addr == 0) {
            m_dest_addr = DC.getAramFree(m_size);
            if (m_dest_addr == 0) {
                setErr(4);
                pLog->err(0, 0, "ARAM over: %s", m_name);
                break;
            }
        } else {
            m_dest_addr = m_arg_addr;
        }
#line 366 "D:/Bio4/Prog/datactrl.cpp"
        no = DvdReadN(m_name, NULL, m_dest_addr, 0, 0, m_mode | 0x8, __FILE__, __LINE__);
        if (pG->IsDevConsole == 1) {
#line 370 "D:/Bio4/Prog/datactrl.cpp"
            DC.setDummyId(DvdReadN("dummy.dat", DC.m_DummyDataMem, 0, 0, 0, m_mode | 0x10, __FILE__, __LINE__));
        }
        m_id = no;
        if (no >= 0) {
            m_command = 0;
            m_condition = COND_ARAM_LOAD;
            if (m_mode == 1) {
                checkLoadToAram();
            }
            OSReport("DC:%s set ARAM_LOAD\n", m_name);
        } else {
            setErr(2);
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToAram command error");
        }
        break;
    case COND_MRAM_OK:
        if (m_arg_addr == 0) {
            m_dest_addr = DC.getAramFree(m_size);
            if (m_dest_addr == 0) {
                setErr(4);
                pLog->err(0, 0, "ARAM over: %s", m_name);
                break;
            }
        } else {
            m_dest_addr = m_arg_addr;
        }
        no = Aram.DmaTransReq(0, (u32) m_addr, m_dest_addr, m_size, m_mode);
        m_id = no;
        if (no >= 0) {
            m_command = 0;
            m_condition = COND_MRAM_TO_ARAM;
            if (m_mode == 1) {
                checkMramToAram();
            }
            OSReport("DC:%s set MRAM_TO_ARAM\n", m_name);
        } else {
            setErr(3);
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToAram command error");
        }
        break;
    case COND_ARAM_OK:
        if (m_arg_addr != 0 && m_arg_addr != (u32) m_addr) {
            if (DC.checkUseDebugMemFlag() == 1) {
                m_dest_addr = (u32) Debug_alloc(m_size, 1);
            } else {
#line 452 "D:/Bio4/Prog/datactrl.cpp"
                m_dest_addr = (u32) MEM_ALLOC(m_size, 0, 0xD);
            }
            if (m_dest_addr != 0) {
                setMallocInfo(1, (void*) m_dest_addr);
                no = Aram.DmaTransReq(1, (u32) m_addr, m_dest_addr, m_size, m_mode);
                m_id = no;
                if (no >= 0) {
                    m_command = 0;
                    m_condition = COND_ARAM_TO_ARAM;
                    OSReport("DC:%s set ARAM_TO_ARAM\n", m_name);
                } else {
                    setErr(3);
                    checkMallocRelease();
                    pLog->err(0, 0, "cDataUnit::setLoadToAram command error");
                }
            } else {
                setClear();
                setCommand(CMND_ARAM_LOAD, 0, 0);
            }
        }
        break;
    }
}

// Executes "clear": waits out a running DVD read, frees the unit's MRAM allocation and returns
// to condition 0 (the ARAM space is reclaimed by the sort).
int cDataUnit::setClear()
{
    m_command = 0;
    switch (m_condition) {
    case COND_ARAM_TO_MRAM:
    case COND_MRAM_TO_ARAM:
    case COND_ARAM_TO_ARAM:
        Aram.DmaCancel(m_id);
        break;
    case COND_MRAM_OK:
    case COND_ARAM_OK:
    case COND_MRAM_TO_MRAM:
        break;
    case COND_MRAM_LOAD:
    case COND_ARAM_LOAD:
        Dvd.ReadCancel(m_id, 0x40);
        Dvd.ReadCheck(m_id, NULL, NULL, NULL);
        break;
    case COND_NO_DATA:
        goto clear;
    default:
        goto ret;
    }
    OSReport("DC:%s set CLEAR\n", m_name);
clear:
    checkMallocRelease();
    m_addr = NULL;
    m_dest_addr = 0;
    m_condition = COND_NO_DATA;
ret:
    return 1;
}

// Executes "delete": clear plus the unit itself is freed (m_be_flag bit0 off).
int cDataUnit::setDelete()
{
    setClear();
    m_size = 0;
    setBeAlive(0);
    OSReport("DC:%s set DELETE\n", m_name);
    return 1;
}

// Condition 1 step: when the DVD read finished the data is in MRAM (condition 2).
void cDataUnit::checkLoadToMram()
{
    int ret;

    if (m_wait == 1) {
        Dvd.ReadNblk2Blk(m_id);
        m_wait = 0;
    }
    ret = Dvd.ReadCheck(m_id, NULL, NULL, NULL);
    if (ret > 0) {
        m_condition = COND_MRAM_OK;
        m_addr = (void*) m_dest_addr;
        OSReport("DC:%s check MRAM_OK\n", m_name);
    } else if (ret < 0) {
        setErr(2);
        checkMallocRelease();
        pLog->err(0, 0, "cDataUnit::checkLoadToMram command error");
    }
}

// Condition 3 step: when the DVD read finished the data is in ARAM (condition 4).
void cDataUnit::checkLoadToAram()
{
    int ret;

    if (m_wait == 1) {
        Dvd.ReadNblk2Blk(m_id);
        m_wait = 0;
    }
    ret = Dvd.ReadCheck(m_id, NULL, NULL, NULL);
    if (ret > 0) {
        m_condition = COND_ARAM_OK;
        m_addr = (void*) m_dest_addr;
        OSReport("DC:%s check ARAM_OK\n", m_name);
    } else if (ret < 0) {
        setErr(2);
        checkMallocRelease();
        pLog->err(0, 0, "cDataUnit::checkLoadToAram command error");
    }
}

// Condition 5 step: when the DMA finished the data is in MRAM (condition 2).
void cDataUnit::checkAramToMram()
{
    int done = 0;

    if (m_wait == 1) {
        while (Aram.TransCheck(m_id) != 1) {
        }
        m_wait = 0;
        done = 1;
    }
    if (Aram.TransCheck(m_id) == 1 || done == 1) {
        m_condition = COND_MRAM_OK;
        m_addr = (void*) m_dest_addr;
        OSReport("DC:%s check MRAM_OK\n", m_name);
    }
}

// Condition 6 step: when the DMA finished the data is in ARAM (condition 4) and the MRAM copy is
// freed.
void cDataUnit::checkMramToAram()
{
    int done = 0;

    if (m_wait == 1) {
        while (Aram.TransCheck(m_id) != 1) {
        }
        m_wait = 0;
        done = 1;
    }
    if (Aram.TransCheck(m_id) == 1 || done == 1) {
        checkMallocRelease();
        m_condition = COND_ARAM_OK;
        m_addr = (void*) m_dest_addr;
        OSReport("DC:%s check ARAM_OK\n", m_name);
    }
}

// Condition 7 step (ARAM repack through MRAM): when the ARAM -> MRAM half finished, starts the
// MRAM -> new ARAM half (condition 6).
void cDataUnit::checkAramToAram()
{
    int done = 0;

    if (m_wait == 1) {
        while (Aram.TransCheck(m_id) != 1) {
        }
        m_wait = 0;
        done = 1;
    }
    if (Aram.TransCheck(m_id) == 1 || done == 1) {
        m_addr = (void*) m_dest_addr;
        m_id = Aram.DmaTransReq(0, m_dest_addr, m_arg_addr, m_size, m_mode);
        m_dest_addr = m_arg_addr;
        if (m_id >= 0) {
            m_condition = COND_MRAM_TO_ARAM;
            OSReport("DC:%s set MRAM_TO_ARAM\n", m_name);
        } else {
            setErr(3);
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::checkAramToAram command error");
        }
    }
}

// Condition 8: nothing to do (MRAM moves are immediate).
void cDataUnit::checkMramToMram()
{
}

// Runs the pending command against the current condition (setLoadToMram / setLoadToAram /
// setClear / setDelete).
void cDataUnit::checkCommand()
{
    switch (getCommand()) {
    case 0:
        break;
    case 1:
        setLoadToMram();
        break;
    case 2:
        setLoadToAram();
        break;
    case 3:
        setClear();
        break;
    case 4:
        setDelete();
        break;
    }
}

// Advances the running transfer of the unit (the check* step for its condition).
void cDataUnit::checkCondition()
{
    switch (getCondition()) {
    case 0:
        break;
    case 1:
        checkLoadToMram();
        break;
    case 2:
        break;
    case 3:
        checkLoadToAram();
        break;
    case 4:
        break;
    case 5:
        checkAramToMram();
        break;
    case 6:
        checkMramToAram();
        break;
    case 7:
        checkAramToAram();
        break;
    case 8:
        checkMramToMram();
        break;
    }
}

struct AramArea {
    u32 addr;
    u32 size;
};

// An ARAM address for `size` bytes: the first gap between the resident ARAM units (sorted by
// address) that fits, else the end of the used area; 0 when it would pass ARAM_END.
u32 cDataCtrl::getAramFree(u32 size)
{
    AramArea tbl[32];
    AramArea tmp;
    int n;
    int i, j;
    u32 k;
    u32 base;
    cDataUnit* u;

    n = 0;
    for (i = 0; i < 32; i++) {
        u = &m_DataUnit[i];
        if (u->isBeAlive()) {
            switch (u->getCondition()) {
            case COND_ARAM_TO_ARAM:
                tbl[n].addr = u->getArgAddr();
                tbl[n].size = u->getSize();
                n++;
                // fallthrough
            case COND_ARAM_OK:
            case COND_ARAM_TO_MRAM:
                tbl[n].addr = (u32) u->getAddr();
                tbl[n].size = u->getSize();
                n++;
                break;
            case COND_ARAM_LOAD:
            case COND_MRAM_TO_ARAM:
                tbl[n].addr = u->getDestAddr();
                tbl[n].size = u->getSize();
                n++;
                break;
            case COND_NO_DATA:
            case COND_MRAM_LOAD:
            case COND_MRAM_OK:
            case COND_MRAM_TO_MRAM:
                break;
            }
        }
    }
    if (n != 0) {
        for (i = 0; i < n - 1; i++) {
            for (j = i; j < n; j++) {
                if (tbl[i].addr > tbl[j].addr) {
                    tmp = tbl[j];
                    tbl[j] = tbl[i];
                    tbl[i] = tmp;
                }
            }
        }
        base = ARAM_FREE_BASE;
        for (i = 0; i < n; i++) {
            if ((int) (tbl[i].addr - base) >= (int) size) {
                for (k = 0; k < 32; k++) {
                    u = &m_DataUnit[k];
                    if (u->isBeAlive() && base == u->getDestAddr()) {
#line 852 "D:/Bio4/Prog/datactrl.cpp"
                        HALT();
                    }
                }
                return base;
            }
            base = tbl[i].addr + tbl[i].size;
        }
        m_aram_free = base + size;
        if (m_aram_free > ARAM_END - 1) {
            return 0;
        }
        return base;
    }
    return ARAM_FREE_BASE;
}

// Boot: allocates the dummy read buffer and the debug bar tiles, resets the units and the ARAM
// allocator.
void cDataCtrl::init()
{
    initDataUnit();
    m_DummyDataMem = Debug_alloc(0x100, 1);
    dispBuf = Debug_alloc(0x400, 1);
}

// Clears all 32 units and the ARAM allocator (room start).
void cDataCtrl::initDataUnit()
{
    cDataUnit* u;
    u32 i;

    m_aram_free = ARAM_FREE_BASE;
    setAramSort(1);
    setDataCtrl(1);
    setUseDebugMemFlag(0);
    for (i = 0; i < 32; i++) {
        u = &m_DataUnit[i];
        memclr_asm(u, sizeof(cDataUnit));
        u->setBeAlive(0);
        u->setCondition(COND_NO_DATA);
        u->setCommand(0, 0, 0);
        u->setErr(0);
        u->fixMramAddr(0);
    }
    initDummyId();
}

// Deletes every unit (frees MRAM allocations).
void cDataCtrl::deleteAll()
{
    u32 i;

    for (i = 0; i < 32; i++) {
        m_DataUnit[i].setDelete();
    }
}

// Registers file `name` as a new unit (size from the DVD file table); NULL when the file does
// not exist or the 32 units are used.
cDataUnit* cDataCtrl::setData(char* name)
{
    u32 len;
    cDataUnit* u;

    if (Dvd.FileExistCheck(name, &len) < 0) {
        pLog->err(0, 0, "cDataCtrl::setData(\"%s\") not found", name);
        return NULL;
    }
    u = getNewUnit();
    if (u != NULL) {
        OSReport("DataCtrl::setData(\"%s\") %d succeed\n", name, len);
        u->setSize(len);
        u->setName(name);
    }
    return u;
}

// The first free unit, cleared and marked in use; NULL with an error when none.
cDataUnit* cDataCtrl::getNewUnit()
{
    cDataUnit* u;
    u32 i;

    for (i = 0; i < 32; i++) {
        u = &m_DataUnit[i];
        if (!u->isBeAlive()) {
            u->setBeAlive(1);
            u->setCondition(COND_NO_DATA);
            u->setCommand(0, 0, 0);
            u->setErr(0);
            u->setSize(0);
            u->m_name[0] = 0;
            u->setAddr(NULL);
            return u;
        }
    }
    pLog->err(0, 0, "cDataCtrl::setData  DC work over");
    return NULL;
}

// Requests a repack of the ARAM units (after deletions left gaps).
void cDataCtrl::setAramSort(int flag)
{
    m_aram_sort_flag = flag;
}

// Per-frame repack: when requested and no transfer is running, moves the resident ARAM units
// down to close gaps (one ARAM -> ARAM transfer per call, condition 7) and lowers m_aram_free;
// 1 while a move was started.
int cDataCtrl::checkAramSort()
{
    cDataUnit* tbl[32];
    cDataUnit* tmp;
    u8 n;
    int i, j;
    u32 base;
    cDataUnit* u;

    if (m_aram_sort_flag == 0) {
        return 0;
    }
    n = 0;
    for (i = 0; i < 32; i++) {
        u = &m_DataUnit[i];
        if (u->isBeAlive()) {
            if (u->getCommand() == 0) {
                switch (u->getCondition()) {
                case COND_NO_DATA:
                case COND_MRAM_LOAD:
                case COND_MRAM_OK:
                    break;
                case COND_ARAM_OK:
                    tbl[n] = u;
                    n++;
                    break;
                case COND_ARAM_LOAD:
                case COND_ARAM_TO_MRAM:
                case COND_MRAM_TO_ARAM:
                case COND_ARAM_TO_ARAM:
                    return 0;
                case COND_MRAM_TO_MRAM:
                    break;
                }
            } else {
                return 0;
            }
        }
    }
    if (n != 0) {
        for (i = 0; i < n - 1; i++) {
            for (j = i; j < n; j++) {
                if ((u32) tbl[i]->getAddr() > (u32) tbl[j]->getAddr()) {
                    tmp = tbl[j];
                    tbl[j] = tbl[i];
                    tbl[i] = tmp;
                }
            }
        }
        base = ARAM_FREE_BASE;
        for (i = 0; i < n; i++) {
            if (base < (u32) tbl[i]->getAddr()) {
                tbl[i]->setCommand(CMND_ARAM_LOAD, base, 0);
                tbl[i]->setLoadToAram();
                return 1;
            }
            base += tbl[i]->getSize();
        }
        m_aram_free = base;
    }
    return 0;
}

// Debug page 0x17 "ARAM DATA DISP": a bar of the ARAM area with each resident unit's span and
// the per-unit name / condition / address list.
void cDataCtrl::dispDebug()
{
    static DcTile tile[2];
    DcTile* p;
    int over;
    int y;
    cDataUnit* u;
    u32 i;
    int x;
    u32 x1;

    m_heap_start = ARAM_FREE_BASE;
    m_heap_end = ARAM_END;
    over = 0;
    p = NULL;
    if (dispBuf != NULL) {
        p = (DcTile*) dispBuf;
    } else {
        over = 1;
    }
    eprintf(0x28, 0x2E, 0, 0x17, "[ARAM DATA DISP]");
    y = 0x2E;
    x = 0x1F8;
    for (i = 0; i < 32; i++) {
        u = &m_DataUnit[i];
        if (u->isBeAlive()) {
            u32 addr = 0;
            u32 size = 0;
            u32 x0;
            switch (u->getCondition()) {
            case 0:
            case 1:
            case 2:
            case 8:
                continue;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                addr = (u32) u->getAddr();
                size = u->getSize();
                break;
            }
            y += 0x10;
            eprintf(0x28, y, 0, 0x17, "%08x:%s", u->m_addr, u->m_name);
            x0 = (u32) ((f32) (addr - m_heap_start) * 400.0f / (f32) (m_heap_end - m_heap_start));
            x1 = (u32) ((f32) (addr + size - m_heap_start) / (f32) (m_heap_end - m_heap_start) * 400.0f);
            if (over == 0) {
                p->code = GPU_TILE;
                p->x0 = x;
                p->y0 = x0 + 0x1E;
                p->w = 5;
                p->h = x1 - x0;
                p->z0 = 0;
                p->c0.r = 0x90;
                p->c0.g = 0x50;
                p->c0.b = 0x50;
                p->c0.cd = 0xFF;
                AddPrim(&MainOt[1], (u32*) p);
                p++;
                if (u == &m_DataUnit[32]) {
                    over = 1;
                }
            }
        }
    }
    if (over == 1) {
        p = &tile[0];
        x1 = (u32) ((f32) (m_aram_free - m_heap_start) * 400.0f / (f32) (m_heap_end - m_heap_start));
        p->code = GPU_TILE;
        p->x0 = x;
        p->y0 = 0x1E;
        p->z0 = 0;
        p->w = 5;
        p->h = x1;
        p->c0.r = 0x90;
        p->c0.g = 0x50;
        p->c0.b = 0x50;
        p->c0.cd = 0xFF;
        AddPrim(&MainOt[1], (u32*) p);
    }
    p = &tile[1];
    p->code = GPU_TILE;
    p->x0 = x;
    p->y0 = 0x1E;
    p->z0 = 0;
    p->w = 5;
    p->h = 400;
    p->c0.r = 0x20;
    p->c0.g = 0x20;
    p->c0.b = 0x20;
    p->c0.cd = 0xFF;
    AddPrim(&MainOt[1], (u32*) p);
}

// Clears the dummy.dat request slots (dev mode reads that mirror each data read).
void cDataCtrl::initDummyId()
{
    int i;

    for (i = 0; i < 32; i++) {
        m_id_dummy[i] = -1;
    }
}

// Records a dummy.dat request id to be waited on.
void cDataCtrl::setDummyId(int dummy_id)
{
    int i;

    if (pG->IsDevConsole != 0 && dummy_id >= 0) {
        for (i = 0; i < 32; i++) {
            if (m_id_dummy[i] == -1) {
                m_id_dummy[i] = dummy_id;
                return;
            }
        }
        pLog->err(0, 0, "cDataCtrl::setDummyId: DummyId work over!!");
    }
}

// Retires finished dummy.dat requests.
void cDataCtrl::checkDummyId()
{
    int i;
    int ret;

    if (pG->IsDevConsole != 0) {
        for (i = 0; i < 32; i++) {
            if (m_id_dummy[i] != -1) {
                ret = Dvd.ReadCheck(m_id_dummy[i], NULL, NULL, NULL);
                if (ret > 0) {
                    m_id_dummy[i] = -1;
                } else if (ret < 0) {
                    pLog->err(0, 0, "cDataCtrl::checkDummyId: failed");
                }
            }
        }
    }
}

// Main task, every frame: unless the sub screen owns the ARAM area or commands are held, runs
// every unit's pending command and transfer step, the dummy requests and the ARAM repack.
void cDataCtrl::check()
{
    cDataUnit* u;
    int i;

    if (checkDataCtrl() != 0 && checkNBlkStop() != 1) {
        while (checkAramSort() == 1) {
        }
        for (i = 0; i < 32; i++) {
            u = &m_DataUnit[i];
            if (u->isBeAlive()) {
                u->setErr(0);
                u->checkCommand();
                u->checkCondition();
            }
        }
        checkDummyId();
    }
}
