#ifndef DATACTRL_H
#define DATACTRL_H

#include "types.h"

enum DATA_COMMAND {
    CMND_NONE = 0,
    CMND_MRAM_LOAD = 1,
    CMND_ARAM_LOAD = 2,
    CMND_CLEAR_DATA = 3,
    CMND_DEL_DATA = 4
};

// Where a data unit's file is (PS2 DATA_CONDITION): cDataUnit::m_condition / setCondition.
enum DATA_CONDITION {
    COND_NO_DATA = 0,
    COND_MRAM_LOAD = 1,
    COND_MRAM_OK = 2,
    COND_ARAM_LOAD = 3,
    COND_ARAM_OK = 4,
    COND_ARAM_TO_MRAM = 5,
    COND_MRAM_TO_ARAM = 6,
    COND_ARAM_TO_ARAM = 7,
    COND_MRAM_TO_MRAM = 8
};

// One streamed data file (game/datactrl.cpp, 0x50 bytes).
class cDataUnit {
public:
    s32 m_condition;   // 0x00  DATA_CONDITION
    s32 m_command;     // 0x04  0 none, 1 load to MRAM, 2 load to ARAM, 3 clear, 4 delete
    s32 m_err;         // 0x08
    u8 m_be_flag;         // 0x0C  bit0 in use, bit1 memory allocated by the unit
    u8 m_mode;         // 0x0D  setCommand argument (1 = synchronous)
    u8 m_wait;     // 0x0E  set while waitUseOk/waitLoadOk spin
    u8 m_malloc_heap;         // 0x0F  heap the allocation came from
    void* m_addr;      // 0x10  current address of the data
    u32 m_arg_addr;         // 0x14  setCommand argument: destination (0 = allocate)
    void* m_malloc_addr;// 0x18
    u32 m_dest_addr;        // 0x1C  destination of the running transfer
    u32 m_fix_addr;     // 0x20  fixed MRAM destination (fixMramAddr)
    u32 m_size;        // 0x24
    u8 pad_28[4];
    char m_name[0x20]; // 0x2C
    int m_id;       // 0x4C  DVD / ARAM request number

    cDataUnit() {}
    ~cDataUnit() {}

    void setName(char* s);

    void setCommand(int cmd, u32 arg, u8 wait);
    int getCommand();
    void setCondition(int c);
    int getCondition();
    void checkMallocRelease();
    void setMallocInfo(int set, void* malloc_addr);
    void fixMramAddr(u32 a);
    int isUseOk();
    int waitUseOk();
    int isLoadOk();
    int waitLoadOk();
    void setLoadToMram();
    void setLoadToAram();
    int setClear();
    int setDelete();
    void checkLoadToMram();
    void checkLoadToAram();
    void checkAramToMram();
    void checkMramToAram();
    void checkAramToAram();
    void checkMramToMram();
    void checkCommand();
    void checkCondition();
    int getErr() { return m_err; }
    void setErr(int err) { m_err = err; }
    char* getName() { return m_name; }
    u8 getWait() { return m_wait; }
    void setWait(u8 wait) { m_wait = wait; }
    u8 getMode() { return m_mode; }
    void setMode(u8 mode) { m_mode = mode; }
    u32 getDestAddr() { return m_dest_addr; }
    void setDestAddr(u32 addr) { m_dest_addr = addr; }
    u32 getArgAddr() { return m_arg_addr; }
    void setArgAddr(u32 addr) { m_arg_addr = addr; }
    void* getAddr() { return m_addr; }
    void setAddr(void* addr) { m_addr = addr; }
    u32 getSize() { return m_size; }
    void setSize(u32 size) { m_size = size; }
    void setBeAlive(bool on)
    {
        if (on) {
            m_be_flag |= 1;
        } else {
            m_be_flag &= ~1;
        }
    }
    bool isBeAlive()
    {
        if (m_be_flag & 1) {
            return true;
        }
        return false;
    }
    void setBeMalloc(bool on)
    {
        if (on) {
            m_be_flag |= 2;
        } else {
            m_be_flag &= ~2;
        }
    }
    bool isBeMalloc()
    {
        if (m_be_flag & 2) {
            return true;
        }
        return false;
    }
};

// Room data unit controller (game/datactrl.cpp, `DC`, 0xAA4 bytes).
class cDataCtrl {
public:
    cDataUnit m_DataUnit[32];  // 0x000
    u32 m_aram_free;         // 0xA00  first free ARAM address above the loaded units
    s32 m_aram_sort_flag;        // 0xA04  1 = repack the ARAM units (checkAramSort)
    s32 m_data_ctrl_flag;  // 0xA08  0 while the sub screen owns the ARAM area (sscrn), 1 otherwise (PS2 m_data_ctrl_flag)
    s32 m_nblock_read_stop;  // 0xA0C  1 = commands are not executed immediately (PS2 m_nblock_read_stop)
    void* dispBuf;       // 0xA10  dispDebug tiles
    u32 m_heap_start;        // 0xA14
    u32 m_heap_end;         // 0xA18
    s32 m_UseDebugMemFlag;         // 0xA1C  1 = allocate from the debug heap
    s32 m_id_dummy[32];     // 0xA20  dummy.dat read requests (dev mode)
    void* m_DummyDataMem;      // 0xAA0

    void setNBlkStop(int flag) { m_nblock_read_stop = flag; }
    int checkNBlkStop() { return m_nblock_read_stop; }
    void setDataCtrl(int flag) { m_data_ctrl_flag = flag; }
    int checkDataCtrl() { return m_data_ctrl_flag; }
    void setUseDebugMemFlag(int flag) { m_UseDebugMemFlag = flag; }
    int checkUseDebugMemFlag() { return m_UseDebugMemFlag; }

    u32 getAramFree(u32 size);
    void init();
    void initDataUnit();
    void deleteAll();
    cDataUnit* setData(char* name);
    cDataUnit* getNewUnit();
    void setAramSort(int flag);
    int checkAramSort();
    void dispDebug();
    void initDummyId();
    void setDummyId(int dummy_id);
    void checkDummyId();
    void check();
};
extern cDataCtrl DC;

#endif
