// game/block.cpp: room block streaming. Large rooms are split into blocks ("st%x/r%03x_%02x.dat"
// cSmd data units) and the room's BLK file lists trigger areas plus, per area, which blocks must
// be in MRAM (displayed), parked in ARAM or dropped. cBlock::check (game loop) finds the area
// the player stands in and issues per-block commands; cBlockUnit runs the load / create /
// ARAM / delete state machine through cDataUnit, driven by checkCommand / checkCondition from
// the main task each frame. While a block loads to MRAM the game is frozen (Stop_flg forced on).

#include "atari.h"
#include "block.h"
#include "global.h"
#include "player.h"
#include "obj.h"
#include "scroll.h"
#include "datactrl.h"
#include "libgpu.h"
#include "main_mem.h"
#include "db_log.h"
#include "eprintf.h"
#include "joy.h"
#include "rnd.h"
#include <stdio.h>
#include <string.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os.h>
#include "scheduler.h"

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp

cBlock Block;





// Rewinds the trigger-area ordering table cursor to the last (highest priority) slot.
void cBlock::setOtStart()
{
    pOt = &ot[7];
}

// Next trigger area in ordering-table order (slots 7..0, each a linked list); NULL at the end.
u32* cBlock::getOtAddr()
{
    u32 v;

    while ((v = *pOt) != 0xFFFFFFFF) {
        pOt = (u32*) (v | 0x80000000);
        if ((s32) v < 0) {
            return pOt;
        }
    }
    return 0;
}

// Room start with the BLK file `data` (NULL = no streaming: everything stays resident): validates
// the header, resolves the tables, allocates the per-block units (debug heap under the t_block
// tool), links the areas into the ordering table, registers each block's data unit
// (getBlockWork) and sizes the shared model memory (checkBlockMemory).
void cBlock::roomInit(void* data)
{
    BlockHeader* h = (BlockHeader*) data;
    int i;

    pData = 0;
    pLink = 0;
    pArea = 0;
    pConnect = 0;
    stopFlagSet = 0;
    if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
        debugData = 0;
        debugMem = 0;
        allDisp = 0;
        noMemCtrl = 0;
    }
    if (h == 0) {
        nBlock = 0;
        return;
    }
    if (strcmp((char*) h, "BLK") != 0) {
        pLog->err(0, 0, "THIS DATA IS NOT BLOCK AREA DATA");
        return;
    }
    if (h->version != 0x100) {
        pLog->err(0, 0, "BLOCK AREA DATA IS OLD VERSION");
        return;
    }
    pData = h;
    nBlock = h->nBlock;
    pLink = (BlockLink*) ((u8*) h + h->ofsLink);
    pArea = (BlockArea*) ((u8*) h + h->ofsArea);
    pConnect = (BlockConnect*) ((u8*) h + h->ofsConnect);
    if (debugData == 0) {
#line 94 "D:/Bio4/Prog/block.cpp"
        pUnit = (cBlockUnit*) MEM_CALLOC(nBlock * sizeof(cBlockUnit), 1, 13);
    }
    if (nBlock == 0) {
        if (Block.debugData == 1) {
            if (pData != 0) {
                Debug_free(pData);
            }
            if (getUnitPtr(0) != 0) {
                Debug_free(getUnitPtr(0));
            }
            Block.debugData = 0;
        }
        pData = 0;
        return;
    }
    ClearOTagR(ot, 8);
    for (i = pData->nArea - 1; i >= 0; i--) {
        AddPrim(&ot[pArea[i].pri], &pArea[i].tag);
    }
    for (i = 0; i < nBlock; i++) {
        if (pLink[i].flags & 1) {
            getBlockWork(i);
        }
    }
    if (debugMem == 0 && checkBlockMemory() == 0) {
        if (Block.debugData == 1) {
            if (pData != 0) {
                Debug_free(pData);
            }
            if (getUnitPtr(0) != 0) {
                Debug_free(getUnitPtr(0));
            }
            Block.debugData = 0;
        }
        pData = 0;
        return;
    }
    pG->AreaNo = -1;
    check(1);
}

// Sizes the block model pool: the largest total of the MRAM block sizes over all connect entries;
// allocates it (memTop / memEnd). 0 when nothing is needed or the allocation failed.
int cBlock::checkBlockMemory()
{
    u32 max = 0;
    u32 size;
    u32 i;
    u32 j;

    for (i = 0; i < pData->nConnect; i++) {
        BlockConnect* c = (BlockConnect*) (i * sizeof(BlockConnect) + (u32) pConnect);
        size = 0;
        if (c->flags & 1) {
            checkBlockConnect(c, pLink, &mramSet, &aramSet);
            for (j = 0; j < nBlock; j++) {
                if (FlagChkVar(&mramSet, j)) {
                    size += getUnitPtr(j)->pData->m_size;
                }
            }
        }
        if (max < size) {
            max = size;
        }
    }
    if (max == 0) {
        return 0;
    }
#line 182 "D:/Bio4/Prog/block.cpp"
    memCur = memTop = MEM_ALLOC(max, 1, 13);
    if (memTop == 0) {
        pLog->err(0, 0, "checkBlockMemory: malloc failed");
        return 0;
    }
    memEnd = (u8*) memTop + max;
    return 1;
}

// Debug: swaps the block pool for a debug heap allocation of `size` (on) or restores it.
void cBlock::useDebugMemory(int on, u32 size)
{
    if (debugMem == 1) {
        if (memTop != 0) {
            Debug_free(memTop);
        }
        memTop = saveMemTop;
        memEnd = saveMemEnd;
        debugMem = 0;
    }
    if (on == 1) {
        saveMemTop = memTop;
        saveMemEnd = memEnd;
        memTop = Debug_alloc(size, 0);
        memEnd = (u8*) memTop + size;
        debugMem = on;
    }
}

// Debug ("BLOCK ALL DISP"): loads and creates every block at once in debug memory (on), or
// deletes them all and returns to normal streaming (off).
void cBlock::dispAllBlock(int on)
{
    u32 size;
    u32 i;

    if (nBlock == 0) {
        return;
    }
    if (on == 1) {
        if (allDisp != 0) {
            return;
        }
        size = 0;
        for (i = 0; i < nBlock; i++) {
            cBlockUnit* u = getUnitPtr(i);
            if (u->flags & 1) {
                size += u->pData->m_size;
                u->setBlockDelete();
                u->checkBlockDelete();
            }
        }
        useDebugMemory(1, size);
        allDisp = 1;
        for (i = 0; i < nBlock; i++) {
            cBlockUnit* u = getUnitPtr(i);
            if (u->flags & 1) {
                u->setBlockCommand(BLOCK_CMD_MRAM_LOAD, 1);
            }
        }
    } else {
        if (allDisp != 1) {
            return;
        }
        for (i = 0; i < nBlock; i++) {
            cBlockUnit* u = getUnitPtr(i);
            if (u->flags & 1) {
                while (u->state != BLOCK_CREATE) {
                    TaskSleep(1);
                }
                u->setBlockDelete();
                u->checkBlockDelete();
            }
        }
        for (i = 0; i < 2; i++) {
            ObjMgr.dieCheck();
        }
        useDebugMemory(0, 0);
        allDisp = 0;
        pG->AreaNo = -1;
    }
}

// Per-frame streaming decision (game loop): finds the player's trigger area (debug mode 0x12 in
// room 5: a random one on Y), and when it changed computes the wanted MRAM / ARAM block sets for
// that area and commands every block (MRAM load / ARAM load / delete); blocks heading to ARAM or
// deletion are hidden at once. Then the debug info page.
void cBlock::check(int arg)
{
    cBlockUnit* u;
    int area;
    u32 i;

    if (pData == 0) {
        pG->AreaNo = 0;
        return;
    }
    if (allDisp == 1) {
        return;
    }
    if (pG->debug_mode == 0x12 && pG->room_id == 5) {
        area = pG->AreaNo;
        if (Joy[0].trg & 0x10) {
            area = Rnd() % nBlock;
            if (pG->AreaNo == area) {
                area++;
            }
            area = area % nBlock;
        }
    } else {
        area = checkBlockArea(&pPL->pos, pG->AreaNo);
    }
    if (pG->AreaNo != area) {
        prevArea = pG->AreaNo;
        pG->AreaNo = area;
        checkBlockConnect(&pConnect[area], pLink, &mramSet, &aramSet);
        for (i = 0; i < nBlock; i++) {
            if ((!(pLink[i].flags & 1))) {
                continue;
            }
            u = getUnitPtr(i);
            if (FlagChkVar(&mramSet, i)) {
                u->setBlockCommand(BLOCK_CMD_MRAM_LOAD, arg);
            } else if (FlagChkVar(&aramSet, i)) {
                u->setBlockCommand(BLOCK_CMD_ARAM_LOAD, arg);
            } else {
                u->setBlockCommand(BLOCK_CMD_DELETE, arg);
            }
        }
    }
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        switch (u->command) {
        case BLOCK_CMD_ARAM_LOAD:
        case BLOCK_CMD_DELETE:
            u->setTrans(0);
            break;
        }
    }
    dispDebugInfo();
}

// The trigger area containing `pos` (+200 y), highest priority first; ties keep the current area
// `now`; `now` when none matches.
s8 cBlock::checkBlockArea(Vec* pos, int now)
{
    Vec p;
    BlockArea* a;
    u32 pri;
    s8 ret;

    if (pData == 0) {
        return 0;
    }
    pri = 0;
    ret = now;
    p = *pos;
    p.y += 200.0f;
    setOtStart();
    while ((a = (BlockArea*) getOtAddr()) != 0) {
        if ((!(a->flags & 1))) {
            continue;
        }
        if (AreaHitCheck(&a->area, &p) != 1) {
            continue;
        }
        if (pri > a->pri) {
            break;
        }
        ret = a->areaNo;
        pri = a->pri;
        if (ret == now) {
            break;
        }
    }
    return ret;
}

// Block sets for connect entry `c`: MRAM = the area's block plus its neighbours (link table),
// ARAM = the neighbours of those minus the MRAM set, then the entry's explicit mram[] additions
// and aram[] removals.
void cBlock::checkBlockConnect(BlockConnect* c, BlockLink* link, u32* mram, u32* aram)
{
    u32 i;

    memclr_asm(mram, 4);
    memclr_asm(aram, 4);
    if ((!(c->flags & 1))) {
        return;
    }
    FlagOnVar(mram, c->blockNo);
    checkBlockConnect_sub(c->blockNo, link, mram);
    for (i = 0; i < nBlock; i++) {
        if (FlagChkVar(mram, i)) {
            checkBlockConnect_sub(i, link, aram);
        }
    }
    for (i = 0; i < nBlock; i++) {
        if (FlagChkVar(mram, i)) {
            FlagOffVar(aram, i);
        }
    }
    for (i = 0; i < 8; i++) {
        if (c->mram[i] != -1) {
            FlagOnVar(aram, (u32) c->mram[i]);
        }
    }
    for (i = 0; i < 8; i++) {
        if (c->aram[i] != -1) {
            FlagOffVar(aram, (u32) c->aram[i]);
        }
    }
}

// Adds every neighbour of block `blk` (both directions of the link table) to `set`.
void cBlock::checkBlockConnect_sub(u8 blk, BlockLink* link, u32* set)
{
    u32 i;
    int j;

    for (i = 0; i < nBlock; i++) {
        BlockLink* l = &link[i];
        if (i == blk) {
            for (j = 0; j < 8; j++) {
                if (l->link[j] != -1) {
                    FlagOnVar(set, (u32) l->link[j]);
                }
            }
        } else {
            for (j = 0; j < 8; j++) {
                if (l->link[j] == blk) {
                    FlagOnVar(set, i);
                }
            }
        }
    }
}

// Queues a BLOCK_CMD for the block with argument `a` (passed on to the data unit).
void cBlockUnit::setBlockCommand(int cmd, int a)
{
    command = cmd;
    arg = a;
}

// Shows / hides every scroll object (cObj id 2) that belongs to this block.
void cBlockUnit::setTrans(int on)
{
    u32 i;

    for (i = 0; i < ObjMgr.getArrayNum(); i++) {
        cObj* o = ObjMgr.fastAt(i);
        if (o->isAlive() && o->id == 2 && o->blk == no) {
            if (on == 1) {
                o->be_flag |= 2;
            } else {
                o->be_flag &= ~2;
            }
        }
    }
}

// Applies the MRAM load command: from NO_DATA / ARAM_OK enters MRAM_LOAD_SET; already loading or
// created just drops the command; ARAM / delete transitions in flight are left to finish.
void cBlockUnit::setBlockLoadToMram()
{
    switch (state) {
    case BLOCK_ARAM_LOAD_SET:
    case BLOCK_ARAM_LOAD:
    case BLOCK_DELETE:
        break;
    case BLOCK_NO_DATA:
    case BLOCK_ARAM_OK:
        state = BLOCK_MRAM_LOAD_SET;
        command = BLOCK_CMD_NONE;
        OSReport("BLOCK:%2d MRAM_LOAD_SET\n", no);
        break;
    case BLOCK_MRAM_LOAD_SET:
    case BLOCK_MRAM_LOAD:
    case BLOCK_CREATE:
        command = BLOCK_CMD_NONE;
        break;
    }
}

// Applies the ARAM load command: a created block is destroyed first (models released), then
// ARAM_LOAD_SET; MRAM transitions in flight are left to finish.
void cBlockUnit::setBlockLoadToAram()
{
    switch (state) {
    case BLOCK_MRAM_LOAD_SET:
    case BLOCK_MRAM_LOAD:
    case BLOCK_DELETE:
        break;
    case BLOCK_CREATE:
        recalcModelAddr(0);
        BlockDestroy(no);
        OSReport("BLOCK:%2d BlockDestroy\n", no);
    case BLOCK_NO_DATA:
        state = BLOCK_ARAM_LOAD_SET;
        command = BLOCK_CMD_NONE;
        OSReport("BLOCK:%2d ARAM_LOAD_SET\n", no);
        break;
    case BLOCK_ARAM_LOAD_SET:
    case BLOCK_ARAM_LOAD:
    case BLOCK_ARAM_OK:
        command = BLOCK_CMD_NONE;
        break;
    }
}

// Applies the delete command: a created block is destroyed, then DELETE; loads in flight are
// left to finish.
void cBlockUnit::setBlockDelete()
{
    switch (state) {
    case BLOCK_MRAM_LOAD_SET:
    case BLOCK_MRAM_LOAD:
    case BLOCK_ARAM_LOAD_SET:
    case BLOCK_ARAM_LOAD:
        break;
    case BLOCK_CREATE:
        BlockDestroy(no);
        OSReport("BLOCK:%2d BlockDestroy\n", no);
    case BLOCK_NO_DATA:
    case BLOCK_ARAM_OK:
    case BLOCK_DELETE:
        state = BLOCK_DELETE;
        command = BLOCK_CMD_NONE;
        OSReport("BLOCK:%2d DELETE\n", no);
        break;
    }
}

// MRAM_LOAD_SET step: when the data unit is idle, grabs pool memory (unless noMemCtrl) and issues
// its MRAM load, moving to MRAM_LOAD; an ARAM-resident unit is transferred instead. Freezes the
// game (stopFlagSet) while it waits. 1 while busy.
int cBlockUnit::checkBlockLoadToMramSet()
{
    void* p;

    switch (pData->getCondition()) {
    case COND_NO_DATA:
    case COND_MRAM_LOAD:
        Block.stopFlagSet = 1;
    case COND_ARAM_OK:
        if (Block.noMemCtrl == 1) {
            pData->setCommand(CMND_MRAM_LOAD, 0, arg);
        } else {
            p = Block.getBlockMemFree(pData->m_size);
            if (p == 0) {
                return 1;
            }
            pData->setCommand(CMND_MRAM_LOAD, (u32) p, arg);
        }
    case COND_MRAM_OK:
    case COND_ARAM_TO_MRAM:
    case COND_MRAM_TO_MRAM:
        state = BLOCK_MRAM_LOAD;
        break;
    case COND_ARAM_LOAD:
    case COND_MRAM_TO_ARAM:
    case COND_ARAM_TO_ARAM:
        Block.stopFlagSet = 1;
        break;
    }
    return 1;
}

// MRAM_LOAD step: once the data is in MRAM creates the block's scroll models (BlockCreate) and
// enters CREATE (running one ObjMgr move so they appear); 1 while still loading.
int cBlockUnit::checkBlockLoadToMram()
{
    switch (pData->getCondition()) {
    case COND_NO_DATA:
    case COND_MRAM_LOAD:
        Block.stopFlagSet = 1;
        break;
    case COND_MRAM_OK:
        BlockCreate(no, (cSmd*) GetDataExt(pData->m_addr, "SMD", 0));
        setTrans(1);
        state = BLOCK_CREATE;
        if (Block.allDisp == 1) {
            ObjMgr.move();
        }
        OSReport("BLOCK:%2d BlockCreate\n", no);
        return 1;
    }
    return 0;
}

// ARAM_LOAD_SET step: issues the ARAM load on an idle unit (or notes it is already there), then
// ARAM_LOAD.
int cBlockUnit::checkBlockLoadToAramSet()
{
    int ret = 0;

    switch (pData->getCondition()) {
    case COND_MRAM_LOAD:
    case COND_ARAM_TO_MRAM:
        break;
    case COND_NO_DATA:
        ret = 1;
    case COND_MRAM_OK:
        pData->setCommand(CMND_ARAM_LOAD, 0, arg);
    case COND_MRAM_TO_ARAM:
        state = BLOCK_ARAM_LOAD;
        break;
    case COND_ARAM_LOAD:
    case COND_ARAM_OK:
    case COND_ARAM_TO_ARAM:
        state = BLOCK_ARAM_LOAD;
        ret = 1;
        break;
    }
    return ret;
}

// ARAM_LOAD step: ARAM_OK once the unit reports the data in ARAM; 1 while busy.
int cBlockUnit::checkBlockLoadToAram()
{
    switch (pData->getCondition()) {
    case COND_ARAM_OK:
        state = BLOCK_ARAM_OK;
        return 1;
    case COND_ARAM_LOAD:
    case COND_ARAM_TO_ARAM:
        return 1;
    }
    return 0;
}

// DELETE step: once the unit is idle clears it and returns to NO_DATA; 1 while busy.
int cBlockUnit::checkBlockDelete()
{
    int ret = 0;

    switch (pData->getCondition()) {
    case COND_MRAM_LOAD:
    case COND_ARAM_TO_MRAM:
    case COND_MRAM_TO_ARAM:
    case COND_MRAM_TO_MRAM:
        break;
    case COND_NO_DATA:
    case COND_MRAM_OK:
    case COND_ARAM_OK:
        pData->setClear();
        state = BLOCK_NO_DATA;
    case COND_ARAM_LOAD:
    case COND_ARAM_TO_ARAM:
        ret = 1;
        break;
    }
    return ret;
}

// Shifts the model data pointers of this block's scroll objects by `ofs` (after the block data
// moved in memory).
void cBlockUnit::recalcModelAddr(int ofs)
{
    cObj* o;

    for (o = ObjMgr.getActiveWork(); o != 0; o = ObjMgr.getNext(o)) {
        if (o->id == 2 && o->blk == no) {
            o->moveDataAddr(ofs);
        }
    }
}

// Moves the block's data to `dst` (pool compaction), flushes the cache and fixes the model
// pointers.
void cBlockUnit::moveBlockData(void* dst)
{
    int ofs;

    memcpy(dst, pData->m_addr, pData->m_size);
    ofs = (int) dst - (int) pData->m_addr;
    pData->m_addr = dst;
    DCFlushRange(dst, pData->m_size);
    recalcModelAddr(ofs);
    ((cSmd*) GetDataExt(dst, "SMD", 0))->slide(ofs);
}

// The unit of block `no`; NULL out of range.
cBlockUnit* cBlock::getUnitPtr(u8 no)
{
    if (no >= nBlock) {
        return 0;
    }
    return &pUnit[no];
}

// Registers block `no`'s file ("st%x/r%03x_%02x.dat") as a data unit with the data controller;
// 0 when missing.
int cBlock::getBlockWork(u8 no)
{
    char name[64];
    cBlockUnit* u;

    if (no >= nBlock) {
        return 0;
    }
    u = getUnitPtr(no);
    sprintf(name, "st%x/r%03x_%02x.dat", pG->stage_no, pG->room_id, no);
    u->pData = DC.setData(name);
    if (u->pData == 0) {
        return 0;
    }
    u->flags |= 1;
    u->no = no;
    return 1;
}

// Main task, every frame: applies each block's queued command to its state machine.
void cBlock::checkCommand()
{
    cBlockUnit* u;
    u32 i;

    if (noMemCtrl == 0 && pData == 0) {
        return;
    }
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if (u->flags & 1) {
            switch (u->command) {
            case BLOCK_CMD_NONE:
                break;
            case BLOCK_CMD_MRAM_LOAD:
                u->setBlockLoadToMram();
                break;
            case BLOCK_CMD_ARAM_LOAD:
                u->setBlockLoadToAram();
                break;
            case BLOCK_CMD_DELETE:
                u->setBlockDelete();
                break;
            }
        }
    }
}

// Main task, every frame: restores Stop_flg saved by a previous freeze, steps every block's
// state machine, compacts the pool when a block finished creating, and re-freezes the game
// (Stop_flg = all bits) while a block is still loading to MRAM.
void cBlock::checkCondition()
{
    cBlockUnit* u;
    int ok;
    int r;
    u32 i;

    if (noMemCtrl == 0 && pData == 0) {
        return;
    }
    if (stopFlagSet == 1) {
        pG->Stop_flg = stopFlag;
    }
    stopFlagSet = 0;
    ok = 1;
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if (u->flags & 1) {
            r = 1;
            switch (u->state) {
            case BLOCK_MRAM_LOAD:
                r = u->checkBlockLoadToMram();
                break;
            case BLOCK_ARAM_LOAD_SET:
                r = u->checkBlockLoadToAramSet();
                break;
            case BLOCK_ARAM_LOAD:
                r = u->checkBlockLoadToAram();
                break;
            case BLOCK_DELETE:
                r = u->checkBlockDelete();
                break;
            case BLOCK_NO_DATA:
            case BLOCK_MRAM_LOAD_SET:
            case BLOCK_CREATE:
            case BLOCK_ARAM_OK:
                break;
            }
            if (r == 0) {
                ok = 0;
            }
        }
    }
    if (ok == 1) {
        if (Block.noMemCtrl == 0) {
            checkBlockMemSort();
        }
        for (i = 0; i < nBlock; i++) {
            u = getUnitPtr(i);
            if ((u->flags & 1) && u->state == BLOCK_MRAM_LOAD_SET) {
                u->checkBlockLoadToMramSet();
            }
        }
    }
    if (stopFlagSet == 1) {
        stopFlag = pG->Stop_flg;
        pG->Stop_flg = 0xFFFFFFFF;
    }
}

// Compacts the block pool: moves every created block's data down to fill gaps and resets memCur.
void cBlock::checkBlockMemSort()
{
    cBlockUnit* u;
    cBlockUnit* tbl[nBlock];
    u8 cnt = 0;
    int i;
    int j;
    u8* addr;

    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if ((!(u->flags & 1))) {
            continue;
        }
        if (u->state != BLOCK_CREATE) {
            continue;
        }
        tbl[cnt] = u;
        cnt++;
    }
    if (cnt != 0) {
        for (i = 0; i < cnt - 1; i++) {
            for (j = i; j < cnt; j++) {
                if ((u32) tbl[i]->pData->m_addr > (u32) tbl[j]->pData->m_addr) {
                    u = tbl[j];
                    tbl[j] = tbl[i];
                    tbl[i] = u;
                }
            }
        }
        addr = (u8*) memTop;
        for (i = 0; i < cnt; i++) {
            u = tbl[i];
            if ((u32) addr < (u32) u->pData->m_addr) {
                u->moveBlockData(addr);
            }
            addr += tbl[i]->pData->m_size;
        }
        memCur = addr;
    } else {
        memCur = memTop;
    }
}

// Bump-allocates `size` bytes from the block pool; NULL with an error when it does not fit.
void* cBlock::getBlockMemFree(u32 size)
{
    u8* p = (u8*) memCur;

    if (p + size > (u8*) memEnd) {
        pLog->err(0, 0, "cBlock::getBlockMemFree: error");
        return 0;
    }
    memCur = p + size;
    return p;
}

// Debug page 18: area number, Stop_flg, and per block its command / state and the data unit's
// command / condition / addresses.
void cBlock::dispDebugInfo()
{
    cBlockUnit* u;
    int y;
    u32 i;

    if (pG->debug_mode != 0x12) {
        return;
    }
    const char* cmdName[4] = {"NONE", "MRAM_LOAD", "ARAM_LOAD", "DELETE"};
    const char* stateName[8] = {"NO_DATA", "MRAM_LOAD_SET", "MRAM_LOAD", "CREATE", "ARAM_LOAD_SET", "ARAM_LOAD", "ARAM_OK", "DELETE"};
    const char* dataCmdName[5] = {"NONE", "MRAM_LOAD", "ARAM_LOAD", "CLEAR_DATA", "DEL_DATA"};
    const char* dataCondName[9] = {"NO_DATA", "MRAM_LOAD", "MRAM_OK", "ARAM_LOAD", "ARAM_OK", "ARAM_TO_MRAM", "MRAM_TO_ARAM", "ARAM_TO_ARAM", "MRAM_TO_MRAM"};

    eprintf(40, 30, 0, 18, "[BLOCK INFO]  (now area:%d) stop_flg %08X", pG->AreaNo, pG->Stop_flg);
    eprintf(228, 58, 0, 18, "ADDR     DEST     ARG      SIZE");
    y = 58;
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if ((!(u->flags & 1))) {
            continue;
        }
        y += 16;
        eprintf(20, y, 4, 18, "%2d %9s %13s", i, cmdName[u->command], stateName[u->state]);
        y += 16;
        eprintf(20, y, 7, 18, "   %9s %12s %8X %8X %8X %X", dataCmdName[u->pData->getCommand()],
                dataCondName[u->pData->getCondition()], u->pData->getAddr(), u->pData->getDest(), u->pData->getArg(),
                u->pData->getSize());
    }
}
