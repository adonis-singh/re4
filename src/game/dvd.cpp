// game/dvd: DVD read queue, ARAM DMA queue, disc error screen (D:/Bio4/Prog/dvd.cpp).
// 54/55 functions byte-identical (LinkQueue/MesSysMessage/ErrCheck: objdiff shows reloc-only rows);
// every section has the original size and the data sections match. Initialize: no `n = name`
// local; `name` is used directly after the if/else, so the `&name` copy (`mr r28,r29`) is a gcse
// PRE copy inserted at the end of the else block, i.e. right after the DVDConvertPathToEntrynum
// call (C++ EH ends the block at the call); a source-level copy is hoisted above the call.
//  - ErrCheck: `pMes`/`pStr` (3 refs each, live around the loop, REG_EQUIV-doubled lengths
//    390/386 -> priority buckets 76/77, the later-declared pStr won r21) share one bucket once the
//    `flags_54` tests share one `msg = -1; cont = 0` body through a goto: the two duplicated arms
//    were cross-jumped only in jump2, after global alloc, so they counted 2 extra insns in both
//    ranges. Still off:
//  - DiscChange (matching): the `game[4]` template copy loads words 0,8,c,4 because the first
//    `pSys->eff_country` read goes through a reference (`SysRef`): a MEM without the scalar flag is not
//    exempt from the preceding stack stores, so all four stores rank equally in sched2 and the
//    copy keeps its template order (with a plain `pSys` only the word-4 store gated the load via
//    the r9 anti-dependence and its load ranked first).
#include "types.h"
#include "dvd.h"

// The file table is defined before the other headers are included: its strings precede the
// map_obj.h/light.h/widget.h/card.h/sofdec.h strings in the original .rodata.
FileTblEntry FileTbl[] = {
    {"bgm/bio4str.hed", 0},
    {"bgm/bio4bgm.sbb", 0},
    {"etc/moji8.tpl", 0},
    {"etc/core.das", 0},
    {"em/em1f.das", 0},
    {"rel/em1f.rel", 0},
    {"em/pl00.das", 0},
    {"em/wep01.das", 0},
    {"rel/wep01.rel", 0},
    {"em/wep02.das", 0},
    {"rel/wep02.rel", 0},
    {"etc/title.das", 0},
    {"em/em20.das", 0},
    {"rel/em20.rel", 0},
    {"em/em21.das", 0},
    {"rel/em21.rel", 0},
    {"em/em22.das", 0},
    {"rel/em22.rel", 0},
    {"em/em23.das", 0},
    {"rel/em23.rel", 0},
    {"em/em24.das", 0},
    {"rel/em24.rel", 0},
    {"em/em25.das", 0},
    {"rel/em25.rel", 0},
    {"em/em10.das", 0},
    {"rel/em10.rel", 0},
    {"rel/pl02.rel", 0},
    {"em/pl15.das", 0},
    {"em/em06.das", 0},
    {"rel/em06.rel", 0},
    {"em/em11.das", 0},
    {"rel/em11.rel", 0},
    {"em/em26.das", 0},
    {"rel/em26.rel", 0},
    {"em/pl01.das", 0},
    {"em/wep14.das", 0},
    {"rel/wep14.rel", 0},
    {"em/em27.das", 0},
    {"rel/em27.rel", 0},
    {"etc/youdied.tpl", 0},
    {"etc/thank.tpl", 0},
    {"em/em28.das", 0},
    {"rel/em28.rel", 0},
    {"em/em12.das", 0},
    {"rel/em12.rel", 0},
    {"em/wep99.das", 0},
    {"em/em15.das", 0},
    {"rel/em15.rel", 0},
    {"em/em16.das", 0},
    {"rel/em16.rel", 0},
    {"em/em17.das", 0},
    {"rel/em17.rel", 0},
    {"em/em18.das", 0},
    {"rel/em18.rel", 0},
    {"em/em19.das", 0},
    {"rel/em19.rel", 0},
    {"em/em13.das", 0},
    {"em/em14.das", 0},
    {"em/em1a.das", 0},
    {"rel/em1a.rel", 0},
    {"em/em1b.das", 0},
    {"rel/em1b.rel", 0},
    {"em/em29.das", 0},
    {"rel/em29.rel", 0},
    {"em/em2a.das", 0},
    {"rel/em2a.rel", 0},
    {"rel/st0.rel", 0},
    {"em/em2b.das", 0},
    {"rel/em2b.rel", 0},
    {"em/wep07.das", 0},
    {"rel/wep07.rel", 0},
    {"em/em2c.das", 0},
    {"rel/em2c.rel", 0},
    {"em/pl14.das", 0},
    {"rel/pl14.rel", 0},
    {"em/em2d.das", 0},
    {"rel/em2d.rel", 0},
    {"em/pl07.das", 0},
    {"rel/pl07.rel", 0},
    {"rel/em13.rel", 0},
    {"rel/em14.rel", 0},
    {"em/pl03.das", 0},
    {"rel/pl03.rel", 0},
    {"em/em2e.das", 0},
    {"rel/em2e.rel", 0},
    {"em/em2f.das", 0},
    {"rel/em2f.rel", 0},
    {"em/em30.das", 0},
    {"rel/em30.rel", 0},
    {"etc/memcard.das", 0},
    {"em/pl08.das", 0},
    {"em/pl09.das", 0},
    {"em/pl0a.das", 0},
    {"em/pl0f.das", 0},
    {"rel/pl0f.rel", 0},
    {"bgm/bio4evt.sbb", 0},
    {"bgm/bio4midi.hed", 0},
    {"bgm/bio4midi.dat", 0},
    {"em/pl11.das", 0},
    {"rel/pl11.rel", 0},
    {"em/em34.das", 0},
    {"rel/em34.rel", 0},
    {"em/wep19.das", 0},
    {"rel/wep19.rel", 0},
    {"bgm/doorse.hed", 0},
    {"bgm/doorse.dat", 0},
    {"bgm/bgmtbl.dat", 0},
    {"em/pl12.das", 0},
    {"rel/pl12.rel", 0},
    {"em/wep11.das", 0},
    {"rel/wep11.rel", 0},
    {"em/wep10.das", 0},
    {"rel/wep10.rel", 0},
    {"em/wep12.das", 0},
    {"rel/wep12.rel", 0},
    {"em/wep13.das", 0},
    {"rel/wep13.rel", 0},
    {"em/pl0e.das", 0},
    {"rel/pl0e.rel", 0},
    {"em/wep16.das", 0},
    {"rel/wep16.rel", 0},
    {"em/pl10.das", 0},
    {"rel/pl10.rel", 0},
    {"em/em35.das", 0},
    {"rel/em35.rel", 0},
    {"em/wep09.das", 0},
    {"rel/wep09.rel", 0},
    {"em/em3b.das", 0},
    {"rel/em3b.rel", 0},
    {"em/wep03.das", 0},
    {"em/em39.das", 0},
    {"rel/em39.rel", 0},
    {"em/wep15.das", 0},
    {"rel/wep15.rel", 0},
    {"em/wep05.das", 0},
    {"rel/wep05.rel", 0},
    {"em/wep06.das", 0},
    {"rel/wep06.rel", 0},
    {"em/wep04.das", 0},
    {"rel/wep04.rel", 0},
    {"em/em3c.das", 0},
    {"rel/em3c.rel", 0},
    {"em/wep08.das", 0},
    {"rel/wep08.rel", 0},
    {"em/wep17.das", 0},
    {"rel/wep17.rel", 0},
    {"rel/st1_0.rel", 0},
    {"rel/st2_0.rel", 0},
    {"rel/st3_0.rel", 0},
    {"rel/st1_1.rel", 0},
    {"em/em32.das", 0},
    {"rel/em32.rel", 0},
    {"rel/st1_2.rel", 0},
    {"em/em38.das", 0},
    {"rel/em38.rel", 0},
    {"em/em3d.das", 0},
    {"rel/em3d.rel", 0},
    {"em/em1c.das", 0},
    {"rel/em1c.rel", 0},
    {"rel/st2_1.rel", 0},
    {"rel/st2_2.rel", 0},
    {"em/wep00.das", 0},
    {"rel/wep00.rel", 0},
    {"em/em36.das", 0},
    {"rel/em36.rel", 0},
    {"rel/st1_3.rel", 0},
    {"em/em31.das", 0},
    {"rel/em31.rel", 0},
    {"em/pl0b.das", 0},
    {"rel/pl0b.rel", 0},
    {"em/em1d.das", 0},
    {"rel/em1d.rel", 0},
    {"em/wep18.das", 0},
    {"em/wep20.das", 0},
    {"em/wep33.das", 0},
    {"rel/wep33.rel", 0},
    {"em/em1e.das", 0},
    {"rel/em1e.rel", 0},
    {"em/wep30.das", 0},
    {"rel/wep30.rel", 0},
    {"em/pl0c.das", 0},
    {"rel/pl0c.rel", 0},
    {"em/em3e.das", 0},
    {"rel/emmark.rel", 0},
    {"em/em3a.das", 0},
    {"rel/em3a.rel", 0},
    {"rel/st4_0.rel", 0},
    {"rel/st2_3.rel", 0},
    {"em/pl06.das", 0},
    {"rel/pl06.rel", 0},
    {"em/pl05.das", 0},
    {"em/wep26.das", 0},
    {"rel/wep26.rel", 0},
    {"em/wep27.das", 0},
    {"rel/wep27.rel", 0},
    {"em/wep28.das", 0},
    {"rel/wep28.rel", 0},
    {"em/pl0a.das", 0},
    {"rel/pl0a.rel", 0},
    {"em/pl0d.das", 0},
    {"rel/pl0d.rel", 0},
    {"rel/st3_1.rel", 0},
    {"rel/st3_2.rel", 0},
    {"rel/st3_3.rel", 0},
    {"em/wep34.das", 0},
    {"rel/wep34.rel", 0},
    {"em/wep35.das", 0},
    {"rel/wep35.rel", 0},
    {"em/wep36.das", 0},
    {"rel/wep36.rel", 0},
    {"em/wep37.das", 0},
    {"rel/wep37.rel", 0},
    {"em/wep38.das", 0},
    {"rel/wep38.rel", 0},
    {"em/wep39.das", 0},
    {"rel/wep39.rel", 0},
    {"em/wep40.das", 0},
    {"rel/wep40.rel", 0},
    {"em/wep29.das", 0},
    {"rel/wep29.rel", 0},
    {"em/wep41.das", 0},
    {"rel/wep41.rel", 0},
    {"em/wep42.das", 0},
    {"rel/wep42.rel", 0},
    {"em/wep43.das", 0},
    {"rel/wep43.rel", 0},
    {"em/wep44.das", 0},
    {"rel/wep44.rel", 0},
    {"em/em46.das", 0},
    {"em/em4c.das", 0},
    {"em/em4d.das", 0},
    {"em/em4f.das", 0},
    {"em/em50.das", 0},
    {"em/em56.das", 0},
    {"em/em5c.das", 0},
    {"em/em5d.das", 0},
    {"em/em5f.das", 0},
    {"em/em60.das", 0},
    {"em/em66.das", 0},
    {"em/em6c.das", 0},
    {"em/em6d.das", 0},
    {"em/em6f.das", 0},
    {"em/em70.das", 0},
    {"rel/st2_4.rel", 0},
    {"em/wep21.das", 0},
    {"em/wep24.das", 0},
    {"em/wep31.das", 0},
    {"em/wep32.das", 0},
    {"em/wep25.das", 0},
    {"em/wep45.das", 0},
    {"rel/wep45.rel", 0},
    {"em/wep46.das", 0},
    {"em/wep47.das", 0},
    {"rel/wep47.rel", 0},
};

#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "sofdec.h"
#include "dvd.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "scheduler.h"
#include "mes.h"
#include "pad.h"
#include "eprintf.h"
#include "gx.h"

extern "C" {
void ADXGC_SetupDvdFs(int mode);   // lib/adx_sugc.c (the CRI headers are not in include/)
void trans2aram_cb(u32 req);
void dvdread_callback(s32 result, DVDFileInfo* fi);
void aram_cb(u32 req);
void readcancel_cb(s32 result, DVDCommandBlock* cb);
}


#include <dolphin/os.h>

// Low memory globals (OSPhysicalToCached(0x00F8) = bus clock); a struct member so the
// address splits into `lis 0x8000` + displacement. Replaces the SDK macros of the same names.
struct OSLowMem {
    u8 pad_0[0xF8];
    u32 busClock;  // 0xF8
};
#undef OS_BUS_CLOCK
#undef OS_TIMER_CLOCK
#undef OSTicksToMilliseconds
#define OS_BUS_CLOCK (((OSLowMem*) 0x80000000)->busClock)
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))

#include "snd.h"
#include <stdio.h>
#include <string.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os/OSMemory.h>

// Stream work (snd_ram `Snd_str_work[4]`, 0x14C bytes), only the debug display fields.
struct DvdSndStrWork {
    u16 status;   // 0x00
    s16 no;       // 0x02
    u8 pad_4[5];
    s8 state;     // 0x09
    u8 pad_A[0x20 - 0xA];
    u32 req;      // 0x20
    u8 err;       // 0x24
    u8 cancel;    // 0x25
    u8 pad_26[0x14C - 0x26];
};
extern "C" DvdSndStrWork Snd_str_work[4];   // game/snd_ram.cpp SND_STR_WORK Snd_str_work[SND_STR_MAX], seen through the view struct above

#define DVD_BUFF ((void*) 0x80350000)
#define DVD_BUFF2 ((void*) 0x80360000)

// status field of cDvdQueue::flag
enum {
    ST_PUSH = 0,
    ST_READ = 1,
    ST_COMPLETE = 2,
    ST_CANCEL = 3,
    ST_ERROR = 4
};


DvdReq DvdReqWork;
cDvd Dvd;
cAram Aram;
static u8 header_buff[0x800] __attribute__((aligned(32)));
u8 header_save[0x800] __attribute__((aligned(32)));
DvdHeader* pFilehead[2];
static DvdHeader* pFilehead_save[2];

// Queues a read of FileTbl entry `fileNo` (optionally `length` bytes from `ofs`) to `dst` / ARAM
// `aram` with the DvdReqWork mode bits; the caller's file / line are kept for the error screen.
// Returns the request number for ReadCheck.
int DvdRead(int fileNo, void* dst, u32 aram, u32 ofs, u32 length, int mode, const char* file, int line)
{
    DvdReq* w = &DvdReqWork;

    memclr_asm(w, sizeof(DvdReq));
    w->dst = dst;
    w->fileNo = fileNo;
    w->aram = aram;
    w->ofs = ofs;
    w->length = length;
    w->prio = 4;
    w->mode = mode;
    sprintf(w->file, "%s", file);
    w->line = line;
    return Dvd.ReadReq();
}

// Same for a file given by path (mode 3 = allocate the destination on the main heap, bit 0x10 =
// through the data controller, 0x11 debug heap). Every unit's DVD_READ_N / DvdReadN goes here.
int DvdReadN(const char* name, void* dst, int a, int b, int c, int mode, const char* file, int line)
{
    DvdReq* w = &DvdReqWork;

    memclr_asm(w, sizeof(DvdReq));
    sprintf(w->name, "%s", name);
    w->dst = dst;
    w->fileNo = 0xFFFF;
    w->aram = a;
    w->ofs = b;
    w->length = c;
    w->prio = 4;
    w->mode = mode;
    sprintf(w->file, "%s", file);
    w->line = line;
    return Dvd.ReadReq();
}

// Exchanges `size` bytes between MRAM and ARAM in 64 KB pieces through the DVD scratch buffers
// (synchronous DMAs), after letting a running read finish its current piece.
void MemorySwap(void* mram, u32 aram, u32 size)
{
    u8* p;
    u32 q;
    u32 rest;
    u32 n;

    if (Dvd.pCur_queue) {
        while (Dvd.pCur_queue->CkFlag(0x20)) {
            Dvd.pCur_queue->Read();
        }
    }
    rest = ALIGN32(size);
    q = aram;
    OSReport("MemorySwap Mram:%08x Aram:%08x Size:%08x\n", mram, q, size);
    p = (u8*) mram;
    DCFlushRange(mram, size);
    while (rest) {
        n = rest;
        if (n > 0x10000) {
            n = 0x10000;
        }
        rest -= n;
        memcpy(DVD_BUFF, p, n);
        Aram.DmaTransReq(1, q, (u32) DVD_BUFF2, n, 1);
        memcpy(p, DVD_BUFF2, n);
        p += n;
        Aram.DmaTransReq(0, (u32) DVD_BUFF, q, n, 1);
        q += n;
    }
    DCFlushRange(mram, size);
}

// Part transfer: copies the read piece from the DVD buffer to its MRAM destination.
void cDvdQueue::trans2mram(void* buf, u32 addr, u32 size)
{
    memcpy((void*) addr, buf, size);
    mramSize += size;
    DCFlushRange((void*) addr, size);
}

// ARQ callback: the queue's MRAM -> ARAM DMA finished (clears busy bit 0x01000000).
void trans2aram_cb(u32 arq_req)
{
    ARQRequest* r = (ARQRequest*) arq_req;
    cDvdQueue* q = (cDvdQueue*) r->owner;

    q->ResetFlag(0x01000000);
    q->aramSize += r->length;
}

// Part transfer: DMAs the read piece from the DVD buffer to its ARAM destination.
void cDvdQueue::trans2aram(void* buf, u32 addr, u32 size)
{
    u32 n = ALIGN32(size);

    DCFlushRange(buf, n);
    SetFlag(0x01000000);
    ARQPostRequest(&m_ArqReq, (u32) this, ARQ_TYPE_MRAM_TO_ARAM, 1, (u32) buf, addr, n, trans2aram_cb);
}

// DVD callback: the queue's read finished (clears 0x02000000); result -3 = cancelled (0x100000),
// other negatives = read error (0x200000).
void dvdread_callback(s32 result, DVDFileInfo* pInfo)
{
    cDvdQueue* q = (cDvdQueue*) pInfo->cb.userData;

    q->ResetFlag(0x02000000);
    if (result == -3) {
        q->SetFlag(0x100000);
    } else if (result != ALIGN32(q->m_DivReadSize)) {
        q->SetFlag(0x200000);
    }
}

// Rno0 == 0: opens the file and reads the 1 KB part header (headered files, and nested headers
// for type 4 parts) or fakes a one-part header for a plain file; type 3 headers allocate the
// whole destination up front. Errors go to Rno0 3 (exit).
void cDvdQueue::readInit()
{
    u32 size;
    void* buf;
    int heap;
    DvdHeader* h;

    switch (m_Rno1) {
    case 0:
        memclr_asm(header_buff, sizeof(header_buff));
        if (fileOpen() == 0) {
            m_Rno0 = 3;
            m_Rno1 = 0;
            SetFlag(0x200000);
            m_ResultCode = -1;
            OSReport("DVD: File not found : %s\n", m_Name);
            break;
        }
        if (length == 0) {
            length = fileGetLength();
        }
        SetQueueStatus(ST_READ);
        m_Rno1++;
    case 1:
        pFilehead[m_NestDepth] = (DvdHeader*) &header_buff[m_NestDepth << 10];
        if (CkFlag(0x80000000) || m_NestDepth != 0) {
            u32* p;
            SetFlag(0x20);
            m_BaseOffset[m_NestDepth] = m_NestDepth ? pFilehead[m_NestDepth - 1]->ofs : m_Offset;
            p = m_BaseOffset;
            m_DivReadSize = 0x400;
            fileReadAsync(&header_buff[m_NestDepth << 10], 0x400, p[m_NestDepth]);
            m_Rno1++;
        } else {
            pFilehead[m_NestDepth]->type = m_Kind;
            pFilehead[m_NestDepth]->size = length;
            pFilehead[m_NestDepth]->ofs = m_Offset;
            pFilehead[m_NestDepth]->sndType = 0;
            pFilehead[m_NestDepth]->dest = 0;
            pFilehead[m_NestDepth][1].type = 0xFFFFFFFF;
            if (CkFlag(0x6) && m_NestDepth == 0) {
                m_Rno1 = 3;
            } else {
                m_Rno1 = 0xA;
            }
        }
        if (m_NestDepth == 0) {
            OSReport("DVD: Read File: %s\n", m_Name);
        }
        break;
    case 2:
        if (CkFlag(0x02000000)) {
            break;
        }
        ResetFlag(0x20);
        if (CkFlag(0x100000)) {
            m_Rno1 = 0;
            m_Rno0 = 3;
            break;
        }
        if (CkFlag(0x6) && m_NestDepth == 0) {
            m_Rno1++;
        } else {
            m_Rno1 = 0xA;
        }
        break;
    case 3:
        if (CkFlag(0x80000000)) {
            size = 0;
            for (h = (DvdHeader*) header_buff + 1; h->type != TRANS_EOF; h++) {
                if (h->type == TRANS_MRAM) {
                    size += ALIGN32(h->size);
                }
            }
        } else {
            size = length;
            pFilehead[m_NestDepth]->type = 0;
        }
        if (CkFlag(0x2)) {
            heap = MemGetCurrentHeap();
            if (this->m_HeapNo != heap) {
                MemSetCurrentHeap(this->m_HeapNo);
            }
            buf = mem_alloc(ALIGN32(size), reqfile, reqline, 1, 0xD);
            MemSetCurrentHeap(heap);
        } else {
            heap = MemGetCurrentDbgHeap();
            if (this->m_HeapNo != heap) {
                MemSetCurrentDbgHeap(this->m_HeapNo);
            }
            buf = Debug_alloc(ALIGN32(size), 1);
            MemSetCurrentDbgHeap(this->m_HeapNo);
        }
        if (buf == 0) {
            OSReport("DVD: Memory allocate failed\n");
            m_Rno0 = 3;
            m_Rno1 = 0;
            SetFlag(0x200000);
            m_ResultCode = -3;
            break;
        }
        OSReport("DVD: Mem Alloc: %08x Size:%08x\n", buf, size);
        pBuff = buf;
        m_Rno1 = 0xA;
        break;
    case 0xA:
        m_Rno0 = 1;
        m_Rno1 = 0;
        if (m_NestDepth == 0) {
            if (CkFlag(0x10)) {
                if (CkFlag(0x80000000)) {
                    size = 0;
                    for (h = (DvdHeader*) header_buff + 1; h->type != TRANS_EOF; h++) {
                        if (h->type == TRANS_MRAM) {
                            size += ALIGN32(h->size);
                        }
                    }
                } else {
                    size = pFilehead[0]->size;
                }
                pBuff = (u8*) pBuff - ALIGN32(size);
            }
            m_MramAddr = (u32) pBuff;
            m_AramAddr = aram;
        }
        if (CkFlag(0x80000000)) {
            pFilehead[m_NestDepth]++;
        }
        break;
    }
}

// Rno0 == 1: walks the header parts: for each part decides the destination (MRAM / ARAM / sound
// block / explicit address / nested header), reads it in DVD_BUFF sized pieces, transfers each
// piece (trans2mram / trans2aram / the sound data readers) and records the part address / size
// in addrTbl / sizeTbl; the end marker sets "all parts done" (0x04000000) and Rno0 3.
void cDvdQueue::readMain()
{
    static void (cDvdQueue::*trans_proc_tbl[])(void*, u32, u32) = {
        &cDvdQueue::trans2mram, &cDvdQueue::trans2mram, &cDvdQueue::trans2aram, &cDvdQueue::trans2aram
    };
    static char* blk_tbl[] = {
        "CORE", "PL", "WEP", "BGM0", "BGM1", "FOOT", "ROOM", "DOOR",
        "ENEMY0", "ENEMY1", "ENEMY2", "ENEMY3", "ENEMY4", "ENEMY5"
    };
    DvdHeader** ph = &pFilehead[m_NestDepth];
    int t;
    int r;

    switch (m_Rno1) {
    case 0:
        switch ((*ph)->type) {
        case TRANS_EOF:
            if (m_NestDepth == 0) {
                m_Rno0 = 3;
                SetFlag(0x04000000);
                m_Rno1 = 0;
            } else {
                m_NestDepth--;
                ph[-1]++;
            }
            break;
        case TRANS_NONE:
            (*ph)++;
            break;
        case TRANS_NEST:
            if (m_NestDepth == 1) {
                OSReport("DVD: \230A\214\213\203t\203@\203C\203\213\202\314\203l\203X\203g\202\315\202\261\202\352\210\310\217\343\202\305\202\253\202\334\202\271\202\361\n");
                (*ph)++;
            } else {
                m_NestDepth++;
                m_Rno0 = 0;
                m_Rno1 = 1;
            }
            break;
        case TRANS_MRAM:
        case TRANS_ARAM:
            m_LeftSize = (*ph)->size;
            if ((*ph)->dest) {
                m_TransAddr = (*ph)->dest;
            } else if ((*ph)->type == TRANS_MRAM) {
                m_TransAddr = m_MramAddr;
            } else {
                m_TransAddr = m_AramAddr;
            }
            addrTbl[m_NestDepth][m_FileIdx[m_NestDepth]] = m_TransAddr;
            m_Offset = m_BaseOffset[m_NestDepth] + (*ph)->ofs;
            m_Rno1++;
            if ((*ph)->type == TRANS_MRAM) {
                OSReport("DVD: Trans MRAM  addr: %08x size: %08x\n", m_TransAddr, m_LeftSize);
            } else {
                OSReport("DVD: Trans ARAM  addr: %08x size: %08x\n", m_TransAddr, m_LeftSize);
            }
            break;
        case TRANS_SND_BLK:
            t = (*ph)->sndType;
            switch ((*ph)->sndType) {
            case 8:
                r = SndEmDataReadCheck((*ph)->sndArg);
                if (r == -1) {
                    OSReport("DVD: Snd File not Read!!!\n");
                    *ph += 2;
                    return;
                }
                (*ph)->sndNo = r;
                (*ph)[1].sndNo = r;
                t = r + 8;
            case 5:
            case 6:
                SndMem.blk_mram[t] = Snd.mram_top;
                Snd.mram_top += ALIGN32((*ph)->size);
                break;
            case 3:
                r = SndBgmDataReadCheck((*ph)->sndArg);
                if (r == -1) {
                    OSReport("DVD: Snd File not Read!!!\n");
                    *ph += 2;
                    return;
                }
                (*ph)->sndNo = r;
                (*ph)[1].sndNo = r;
                t = r + 3;
                Snd.mram_base_addr_bgm -= ALIGN32((*ph)->size);
                SndMem.blk_mram[t] = Snd.mram_base_addr_bgm;
                break;
            }
            m_TransAddr = (u32) SndMem.blk_mram[t];
            Snd.blk_flag[0] &= ~(1 << t);
            m_LeftSize = (*ph)->size;
            m_Offset = m_BaseOffset[m_NestDepth] + (*ph)->ofs;
            m_Rno1++;
            OSReport("DVD: Trans MRAM Snddata  addr: %08x size: %08x %s\n", m_TransAddr, m_LeftSize, blk_tbl[t]);
            break;
        case TRANS_SND_PCM:
            t = (*ph)->sndType;
            switch ((*ph)->sndType) {
            case 8:
                t = (*ph)->sndNo + 8;
            case 5:
            case 6:
                SndMem.blk_aram[t] = Snd.aram_base_addr;
                Snd.aram_base_addr += ALIGN32((*ph)->size);
                break;
            case 3:
                t = (*ph)->sndNo + 3;
                Snd.aram_base_addr_bgm -= ALIGN32((*ph)->size);
                SndMem.blk_aram[t] = Snd.aram_base_addr_bgm;
                break;
            case 0:
                SndMem.blk_aram[0] = 0x4100;
                break;
            case 1:
                SndMem.blk_aram[1] = 0x44100;
                break;
            case 2:
                SndMem.blk_aram[2] = 0x174100;
                break;
            case 7:
                SndMem.blk_aram[7] = 0x1B4100;
                break;
            }
            m_TransAddr = SndMem.blk_aram[t];
            m_LeftSize = (*ph)->size;
            m_Offset = m_BaseOffset[m_NestDepth] + (*ph)->ofs;
            m_Rno1++;
            OSReport("DVD: Trans ARAM Snddata  addr: %08x size: %08x %s\n", m_TransAddr, m_LeftSize, blk_tbl[t]);
            UseAramSize[t] = (m_LeftSize + 0x1F) / 0x20 * 0x20;
            break;
        }
        break;
    case 1:
        m_Counter = OSGetTick();
        SetFlag(0x20);
        m_DivReadSize = m_LeftSize > 0x20000 ? 0x20000 : m_LeftSize;
        fileReadAsync(DVD_BUFF, m_DivReadSize, m_Offset);
        m_Rno1++;
        break;
    case 2:
        if (CkFlag(0x02000000)) {
            break;
        }
        if (CkFlag(0x100000)) {
            m_Rno1 = 0;
            m_Rno0 = 3;
            break;
        }
        if (CkFlag(0x200000)) {
            OSReport("DVD: Read Error!!!\n");
            ResetFlag(0x200020);
            m_Rno1--;
            break;
        }
        (this->*trans_proc_tbl[(*ph)->type])(DVD_BUFF, m_TransAddr, m_DivReadSize);
        m_Rno1++;
        ResetFlag(0x20);
        break;
    case 3:
        if (CkFlag(0x01000000)) {
            break;
        }
        m_LeftSize -= m_DivReadSize;
        m_Offset += m_DivReadSize;
        m_TransAddr += m_DivReadSize;
        if (m_LeftSize <= 0) {
            switch ((*ph)->type) {
            case 2:
                SndBlkInit((*ph)->sndType, (*ph)->sndArg, (*ph)->sndNo);
                break;
            case 0:
            case 3:
                if ((*ph)->dest == 0) {
                    if ((*ph)->type == TRANS_MRAM) {
                        m_MramAddr += ALIGN32((*ph)->size);
                    } else {
                        m_AramAddr += ALIGN32((*ph)->size);
                    }
                }
                sizeTbl[m_NestDepth][m_FileIdx[m_NestDepth]] = (*ph)->size;
                break;
            }
            (*ph)++;
            m_Rno1 = 0;
            m_FileIdx[m_NestDepth]++;
        } else {
            m_Rno1 = 1;
        }
        break;
    }
}

// Rno0 == 2: waits for the cancelled DVD command to return, then exit.
void cDvdQueue::readCancelWait()
{
    if (CkFlag(0x100000)) {
        m_Rno0 = 3;
    }
}

// Rno0 == 3: closes the file, frees the destination on a cancelled / failed allocating read,
// logs the cancel, marks the slot finished (0x400000).
void cDvdQueue::readExit()
{
    fileClose();
    if (CkFlag(0x04000000)) {
        SetQueueStatus(ST_COMPLETE);
        OSReport("DVD: Read Ok ");
    } else if (CkFlag(0x100000)) {
        SetQueueStatus(ST_CANCEL);
        OSReport("DVD: Read Cancel: %s ", m_Name);
    } else if (CkFlag(0x200000)) {
        SetQueueStatus(ST_ERROR);
        OSReport("DVD: Read Error : %s\n", m_Name);
    }
    SetFlag(0x400000);
}

// One step of the queue's routine (Rno0 table); 1 while the read is still in progress.
int cDvdQueue::Read()
{
    static void (cDvdQueue::*func_tbl[])() = {
        &cDvdQueue::readInit, &cDvdQueue::readMain, &cDvdQueue::readCancelWait, &cDvdQueue::readExit
    };
    int ret = 1;
    int pc = 1;

    if (SysFlagChk(pG, SYS_SN_PC_READ) == 0) {
        pc = 0;
    }
    if (pcMode) {
        SysFlagOn(pG, SYS_SN_PC_READ);
    } else {
        SysFlagOff(pG, SYS_SN_PC_READ);
    }
    (this->*func_tbl[m_Rno0])();
    if (pc) {
        SysFlagOn(pG, SYS_SN_PC_READ);
    } else {
        SysFlagOff(pG, SYS_SN_PC_READ);
    }
    if (CkFlag(0x400000)) {
        ret = 0;
    } else if (!CkFlag(1)) {
        ret = 0;
    }
    return ret;
}

// Fills the slot from the DvdReqWork of the current request: entry number (table or path),
// name, destinations, priority, and the flag bits from `mode` (sync, interrupt task, headered,
// heap, keep).
void cDvdQueue::Initialize()
{
    DvdReq* w = &DvdReqWork;
    char buf[0x40];
    int hed;
    int pc;

    if (w->fileNo != 0xFFFF) {
        entrynum = FileTbl[w->fileNo].entrynum;
        sprintf(m_Name, "%s", FileTbl[w->fileNo].name);
    } else {
        entrynum = DVDConvertPathToEntrynum(w->name);
        sprintf(m_Name, "%s", w->name);
    }
    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        sprintf(buf, "d:\\bio4/data/%s", m_Name);
        sprintf(m_Name, "%s", buf);
    }
    m_FileNo = w->fileNo;
    pBuff = w->dst;
    aram = w->aram;
    m_Offset = w->ofs;
    length = w->length;
    m_Prio = w->prio;
    hed = w->mode & 0x8;
    if (hed) {
        hed = 3;
    }
    m_Kind = hed;
    if (w->mode & 0x1) {
        SetFlag(0x40000000);
    } else if (w->mode & 0x100) {
        SetFlag(0x100);
    }
    if (w->mode & 0x8000) {
        SetFlag(0x80000000);
    }
    if (w->mode & 0x4) {
        m_HeapNo = MemGetCurrentHeap();
        SetFlag(0x2);
    } else if (w->mode & 0x2) {
        m_HeapNo = MemGetCurrentDbgHeap();
        SetFlag(0x4);
    }
    if (w->mode & 0x20) {
        SetFlag(0x10);
    }
    if (w->mode & 0x40) {
        SetFlag(0x20000000);
    }
    sprintf(reqfile, "%s", w->file);
    reqline = w->line;
    pc = 1;
    if (SysFlagChk(pG, SYS_SN_PC_READ) == 0) {
        pc = 0;
    }
    pcMode = pc;
}

// Inserts the slot into the pending list ordered by priority (higher first); returns its
// request number.
int cDvdQueue::LinkQueue()
{
    cDvdQueue* p;
    cDvdQueue* n;

    if (Dvd.pQueue_list == 0) {
        Dvd.pQueue_list = this;
        m_Next = 0;
    } else {
        p = Dvd.pQueue_list;
        for (;;) {
            if (p == Dvd.pQueue_list) {
                if (m_Prio < p->m_Prio) {
                    m_Next = p;
                    Dvd.pQueue_list = this;
                    break;
                }
            }
            n = p->m_Next;
            if (n == 0) {
                p->m_Next = this;
                m_Next = 0;
                break;
            }
            if (m_Prio < n->m_Prio) {
                m_Next = n;
                p->m_Next = this;
                break;
            }
            p = n;
        }
    }
    SetQueueStatus(ST_PUSH);
    return m_Id;
}

// Frees the slot.
void cDvdQueue::PushQueue()
{
    ResetFlag(1);
}

// Frees the MRAM the slot allocated for a read that failed / was cancelled.
void cDvdQueue::ErrMemFree()
{
    if (CkFlag(0x6)) {
        void* p = pBuff;
        if (p) {
            if (CkFlag(0x2)) {
                MemFree(p);
            } else {
                Debug_free(p);
            }
        }
    }
}

// Opens the file by entry number; 0 when it is not on the disc.
int cDvdQueue::fileOpen()
{
    int ret = 1;

    if (entrynum == -1) {
        ret = 0;
    } else {
        DVDFastOpen(entrynum, &m_Info);
    }
    return ret;
}

// The open file's length.
int cDvdQueue::fileGetLength()
{
    return m_Info.length;
}

// Starts an asynchronous read of `size` bytes at `ofs` into `buf` (busy bit 0x02000000).
int cDvdQueue::fileReadAsync(void* addr, u32 size, u32 offset)
{
    m_Info.cb.userData = this;
    SetFlag(0x02000000);
    return DVDReadAsyncPrio(&m_Info, addr, ALIGN32(size), offset, dvdread_callback, 2);
}

// Closes the file.
int cDvdQueue::fileClose()
{
    int ret = 0;

    if (entrynum != -1) {
        DVDClose(&m_Info);
        ret = 1;
    }
    return ret;
}

// Queues an ARAM DMA (type 0 MRAM -> ARAM, 1 ARAM -> MRAM) of `len` bytes; wait 1 runs it now
// and blocks, else it is appended to the list. Returns the request slot, -1 when full.
int cAram::DmaTransReq(int type, u32 src, u32 dst, u32 len, int mode)
{
    int no;
    cAramQueue* r;
    cAramQueue* p;

    r = pullAramQueue(&no);
    if (r == 0) {
        OSReport("No ARAM Queue\n");
        return -1;
    }
    r->type = type;
    r->src = src;
    r->dst = dst;
    r->len = len;
    if (mode) {
        pCur_queue = r;
        DmaTrans(r, 1);
    } else {
        if (pQueue_list == 0) {
            pQueue_list = r;
            r->next = 0;
        } else {
            for (p = pQueue_list; p->next; p = p->next) {}
            p->next = r;
            r->next = 0;
        }
    }
    return no;
}

// A free ARAM request slot (marked in use) and its index.
cAramQueue* cAram::pullAramQueue(int* id)
{
    int i;
    cAramQueue* r;

    for (i = 0; i < 16; i++) {
        if (AramQueue[i].be_flag == 0) {
            r = &AramQueue[i];
            memclr_asm(r, sizeof(cAramQueue));
            r->SetFlag(1);
            *id = i;
            return r;
        }
    }
    return 0;
}

// ARQ callback: marks the request done and frees the current pointer.
void aram_cb(u32 arq_req)
{
    cAramQueue* r = (cAramQueue*) ((ARQRequest*) arq_req)->owner;

    r->SetFlag(0x04000000);
    Aram.pCur_queue = 0;
}

// Posts the request to the ARQ; with wait spins until done and frees the slot.
void cAram::DmaTrans(cAramQueue* pQueue, int mode)
{
    pQueue->len = ALIGN32(pQueue->len);
    DCFlushRange((void*) (pQueue->type == 0 ? pQueue->src : pQueue->dst), pQueue->len);
    ARQPostRequest(&ArqReq, (u32) pQueue, pQueue->type, 1, pQueue->src, pQueue->dst, pQueue->len, aram_cb);
    if (mode & 1) {
        while (pQueue->CkFlag(0x04000000) == 0) {}
        pQueue->clear();
    }
}

// 1 when request `no` finished (the slot is freed).
int cAram::TransCheck(int id)
{
    cAramQueue* r = &AramQueue[id];

    if (r->be_flag == 0 || r->CkFlag(0x04000000) == 1) {
        r->clear();
        return 1;
    }
    return 0;
}

// Removes pending request `no` from the list (the running one cannot be cancelled: 1).
int cAram::DmaCancel(int id)
{
    cAramQueue* p;
    int ret = 0;

    if (pCur_queue == &AramQueue[id]) {
        while (TransCheck(id) == 0) {}
        return 1;
    }
    if (pQueue_list) {
        for (p = pQueue_list; p->next; p = p->next) {
            if (p->next == &AramQueue[id]) {
                p->next = p->next->next;
                (&AramQueue[id])->clear();
                ret = 1;
                break;
            }
        }
    }
    return ret;
}

// Drops every queued request and flushes the ARQ.
void cAram::DmaCancelAll()
{
    int i;

    if (pCur_queue) {
        for (i = 0; i < 16; i++) {
            if (pCur_queue == &AramQueue[i]) {
                while (TransCheck(i) == 0) {}
                break;
            }
        }
    }
    while (pQueue_list) {
        pQueue_list->clear();
        pQueue_list = pQueue_list->next;
    }
    ARQFlushQueue();
}

// Boot: resolves the file table entry numbers, resets the queues, reads the size table and
// records the memory above the FST.
void cDvd::Init()
{
    u32 size;
    u32 fst;

    ADXGC_SetupDvdFs(0);
    FileTblExistCheck();
    memclr_asm(this, sizeof(cDvd));
    memclr_asm(&Aram, sizeof(cAram));
    ReadID = 1;
    OSReport("FST Address = 0x%8x\n", DVDGetFSTLocation());
    size = OSGetConsoleSimulatedMemSize();
    fst = (u32) DVDGetFSTLocation() - 0x80000000;
    FstSize = size - fst;
}

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// Loads the file size table (used by FileExistCheck) once its read completes.
#line 98 "D:/Bio4/Prog/dvd.cpp"
void cDvd::SizeTableRead()
{
    int req = DVD_READ_N("etc/sizetbl.dat", 0, 0, 0, 0, 5);
    int r = Dvd.ReadCheck(req, 0, 0, &pSizeTbl);

    if (req < 0 || r < 0) {
        pSizeTbl = 0;
    }
}

// Task entry: the DVD read pump.
void DvdReadProc()
{
    Dvd.ReadProc();
}

// Read pump: drives the current request until it finishes (readProcMain) and reports errors;
// then the slot is released unless kept.
void cDvd::ReadProc()
{
    cDvdQueue* q = pCur_queue;
    int sync = 1;
    int intr = 1;

    q->tick = OSGetTick();
    readProcMain(q);
    pCur_queue = 0;
    sync = q->CkFlag(0x40000000);
    intr = q->CkFlag(0x100);
    OSReport(" %d\n", OSTicksToMilliseconds(OSGetTick() - q->tick));
    if (q->CkFlag(0x20000000) == 1) {
        q->PushQueue();
    } else {
        q->SetFlag(0x800);
    }
    if (sync == 0) {
        if (intr == 1) {
            iTaskExit();
        } else {
            TaskExit();
        }
    }
}

// Steps request `q` until it is done, running the disc error check when the read fails.
void cDvd::readProcMain(cDvdQueue* pQueue)
{
    while (pQueue->Read() == 1) {
        if (!pQueue->CkFlag(0x40000000)) {
            if (!pQueue->CkFlag(0x100)) {
                TaskSleep(1);
            }
        } else {
            ErrCheck(-1, 0);
        }
    }
}

// Turns pending request `no` into a blocking read: pulls it from the current / pending list and
// runs blockRead on it (a unit needs its data now).
void cDvd::ReadNblk2Blk(int id)
{
    cDvdQueue* q = getQueuePtr(id);
    cDvdQueue* p;

    if (q == 0) {
        return;
    }
    switch (q->GetQueueStatus()) {
    case ST_READ:
        q->SetFlag(0x40000000);
        if (q->CkFlag(0x100) == 1) {
            iTaskKill();
        } else {
            TaskKill(3);
        }
        readProcMain(q);
        q->SetFlag(0x800);
        pCur_queue = 0;
        OSReport(" %d\n", OSTicksToMilliseconds(OSGetTick() - q->tick));
        break;
    case ST_PUSH:
        q->SetFlag(0x40000000);
        if (q == pCur_queue) {
            pCur_queue = 0;
            if (q->CkFlag(0x100) == 1) {
                iTaskKill();
            } else {
                TaskKill(3);
            }
        } else if (q == pQueue_list) {
            pQueue_list = q->m_Next;
        } else {
            for (p = pQueue_list; p->m_Next != q; p = p->m_Next) {}
            p->m_Next = q->m_Next;
        }
        blockRead(q);
        break;
    }
}

// Resolves every FileTbl name to its disc entry number (-1 when absent).
void cDvd::FileTblExistCheck()
{
    int i;

    for (i = 0; i < sizeof(FileTbl) / sizeof(FileTbl[0]); i++) {
        FileTbl[i].entrynum = DVDConvertPathToEntrynum(FileTbl[i].name);
    }
}

// Entry number of `name` (and its length in *pLength); negative when not on the disc.
int cDvd::FileExistCheck(const char* name, u32* pLength)
{
    DVDFileInfo fi;
    int ret;

    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        ret = -1;
    } else {
        ret = DVDConvertPathToEntrynum(name);
        if (pLength && ret != -1) {
            DVDFastOpen(ret, &fi);
            *pLength = fi.length;
            DVDClose(&fi);
        }
    }
    return ret;
}

// Creates a queue slot for the pending DvdReqWork: synchronous requests are read to completion
// here (blockRead), others are linked into the pending list. Returns the request number, -2
// when no slot is free.
int cDvd::ReadReq()
{
    cDvdQueue* q = pullReadQueue();
    u8 no;

    if (q == 0) {
        return -2;
    }
    q->Initialize();
    if (q->CkFlag(0x40000000) == 1) {
        no = q->m_Id;
        blockRead(q);
        return no;
    }
    return q->LinkQueue();
}

// Runs request `q` to completion right now (a running request finishes its piece first and is
// resumed afterwards).
void cDvd::blockRead(cDvdQueue* pQueue)
{
    cDvdQueue* save = 0;

    if (pCur_queue) {
        while (pCur_queue->CkFlag(0x20) == 1) {
            pCur_queue->Read();
        }
        save = pCur_queue;
        memcpy(header_save, header_buff, sizeof(header_buff));
        memcpy(pFilehead_save, pFilehead, sizeof(pFilehead));
    }
    pCur_queue = pQueue;
    ReadProc();
    if (save) {
        pCur_queue = save;
        memcpy(header_buff, header_save, sizeof(header_buff));
        memcpy(pFilehead, pFilehead_save, sizeof(pFilehead));
    }
}

// Per-frame: when idle, starts the highest-priority pending request (interrupt-task requests get
// their own task), then the disc error check.
void cDvd::Watcher()
{
    if (pCur_queue == 0 && pQueue_list) {
        bool ok;
        if (pQueue_list->CkFlag(0x100) == 1) {
            ok = iTaskExec(DvdReadProc);
        } else {
            ok = TaskExec(3, DvdReadProc, 0);
        }
        if (ok) {
            pCur_queue = pQueue_list;
            pQueue_list = pQueue_list->m_Next;
        }
    }
    if (Aram.pCur_queue == 0 && Aram.pQueue_list) {
        cAramQueue* r = Aram.pQueue_list;
        Aram.pCur_queue = r;
        Aram.pQueue_list = r->next;
        Aram.DmaTrans(r, 0);
    }
    ErrCheck(-1, 0);
    queueStatusDisp();
    DiscReadInfo();
}

// DVD callback of a cancelled read.
void readcancel_cb(s32 result, DVDCommandBlock* block)
{
    cDvdQueue* q = (cDvdQueue*) block->userData;

    q->SetFlag(0x100000);
}

// Cancels request `no`: pending ones are dropped, the running one gets DVDCancelAsync and waits
// (Rno0 2); mode 0x40 keeps the slot for a ReadCheck. Returns 1 when something was cancelled.
int cDvd::ReadCancel(int id, int mode)
{
    cDvdQueue* q;
    cDvdQueue* p;
    char* n;
    int ret = 0;

    q = getQueuePtr(id);
    if (q == 0) {
        ret = -2;
    } else if (q->GetQueueStatus() == 0) {
        if (q == pCur_queue) {
            q->SetFlag(0x100000);
            if (mode == 0x40) {
                q->SetFlag(0x20000000);
            }
            q->m_Rno0 = 3;
            n = q->m_Name;
        } else {
            if (q == pQueue_list) {
                pQueue_list = q->m_Next;
                n = q->m_Name;
            } else {
                n = q->m_Name;
                for (p = pQueue_list; p->m_Next != q; p = p->m_Next) {}
                p->m_Next = q->m_Next;
            }
            if (mode == 0x40) {
                q->PushQueue();
            } else {
                q->SetQueueStatus(ST_CANCEL);
            }
        }
        OSReport("DVD: Read Cancel: %s\n", n);
        ret = 0;
    } else if (q->GetQueueStatus() == ST_READ) {
        q->m_Info.cb.userData = q;
        DVDCancelAsync(&q->m_Info.cb, readcancel_cb);
        if (mode == 0x40) {
            q->SetFlag(0x20000000);
        }
        q->m_Rno0 = 2;
    } else {
        ret = -1;
    }
    return ret;
}

// Cancels every request and the running DVD command.
void cDvd::ReadCancelAll()
{
    if (pCur_queue) {
        DVDCancelAll();
        if (pCur_queue->CkFlag(0x100) == 1) {
            iTaskKill();
        } else {
            TaskKill(3);
        }
    }
    memclr_asm(DvdQueue, sizeof(DvdQueue));
    pCur_queue = 0;
    pQueue_list = 0;
}

// A free queue slot marked in use with a fresh request number (1..255, never 0); NULL when the
// 16 are busy.
cDvdQueue* cDvd::pullReadQueue()
{
    int i;
    cDvdQueue* q;

    for (i = 0; i < 16; i++) {
        q = &DvdQueue[i];
        if (q->EmptyCk()) {
            memclr_asm(q, sizeof(cDvdQueue));
            q->SetFlag(1);
            q->m_Id = ReadID++;
            if (ReadID == 0) {
                ReadID = 1;
            }
            return q;
        }
    }
    return 0;
}

// Polls request `req`: 1 when done (result word / total size / first destination through the
// pointers, the slot released unless kept), 0 while reading, negative on cancel / error.
int cDvd::ReadCheck(int id, int* mram_size, int* aram_size, void** addr)
{
    DvdReadInfo info;

    if (readCheckMain(id, &info) == 1) {
        if (mram_size) {
            *mram_size = info.mramSize;
        }
        if (aram_size) {
            *aram_size = info.aramSize;
        }
        if (addr) {
            *addr = (void*) info.addr[0][0];
        }
    }
}

// Poll variant used by read.cpp that also fills a DvdReadInfo.
int cDvd::ReadCheck(int id, DvdReadInfo* pInfo)
{
    return readCheckMain(id, pInfo);
}

// The poll: by slot status (READ pending, COMPLETE copies the part address / size tables and
// releases, CANCEL / ERROR release with a negative result).
int cDvd::readCheckMain(int id, DvdReadInfo* pInfo)
{
    cDvdQueue* q;
    int ret = 0;

    if (id >= 0) {
        q = getQueuePtr(id);
        if (q != 0) {
            switch (q->GetQueueStatus()) {
            case ST_READ:
                break;
            case ST_COMPLETE:
                if (q->CkFlag(0x800) == 1) {
                    if (pInfo) {
                        memcpy(pInfo->addr, q->addrTbl, sizeof(pInfo->addr));
                        memcpy(pInfo->size, q->sizeTbl, sizeof(pInfo->size));
                        pInfo->mramSize = q->mramSize;
                        pInfo->aramSize = q->aramSize;
                    }
                    ret = 1;
                    q->PushQueue();
                }
                break;
            case ST_CANCEL:
                if (q->CkFlag(0x800) == 1) {
                    ret = -4;
                    q->ErrMemFree();
                    q->PushQueue();
                }
                break;
            case ST_ERROR:
                if (q->CkFlag(0x800) == 1) {
                    ret = q->m_ResultCode;
                    q->ErrMemFree();
                    q->PushQueue();
                }
                break;
            }
        } else {
            ret = -5;
        }
    } else {
        ret = id;
    }
    return ret;
}

// The slot with request number `no`; NULL when none.
cDvdQueue* cDvd::getQueuePtr(u8 id)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (DvdQueue[i].m_Id == id) {
            return &DvdQueue[i];
        }
    }
    return 0;
}

static const GXColor BkBlack = {0, 0, 0, 0};

// The disc error handler: polls the drive state and, for cover open / no disc / wrong disc /
// retry / fatal, freezes the game and shows the system message (RomFontMessage or the message
// system when available) until the state clears; handles the disc 1 / 2 change (DiscChange).
// Returns 1 when an error screen was shown.
int cDvd::ErrCheck(int disc_new, int proc)
{
    int cont = 1;
    int discNo = GetDiscNo();
    int shown = 0;
    u8** pMes = MesData.m_Data;
    SndPlayWork* pStr = Snd.str_state;
    int paused = 0;
    int msg;
    int stat;
    SndPlayWork* str;

    do {
        stat = DVDGetDriveStatus();
        msg = -1;
        m_ErrCode = stat;
        switch (stat) {
        case DVD_STATE_BUSY:
            if (proc == 0) {
                cont = 0;
            } else {
                msg = 0;
            }
            break;
        case DVD_STATE_END:
            if (discChanged == 1) {
                discChanged = stat;
            }
            cont = 0;
            break;
        case DVD_STATE_FATAL_ERROR:
            msg = DVD_MES_FATAL_ERROR;
            break;
        case DVD_STATE_COVER_OPEN:
            msg = DVD_MES_COVER_OPEN;
            break;
        case DVD_STATE_NO_DISK:
            msg = DVD_MES_NO_DISC;
            break;
        case DVD_STATE_WRONG_DISK:
            msg = DVD_MES_WRONG_DISC;
            break;
        case DVD_STATE_RETRY:
            msg = DVD_MES_RETRY_ERROR;
            break;
        case DVD_STATE_MOTOR_STOPPED:
            msg = DVD_MES_DISC_CHANGE;
            break;
        case DVD_STATE_PAUSING:
            break;
        }
        // One shared body (goto) instead of two identical arms: the duplicated `li r30,-1;
        // li r27,0` that jump2 would cross-jump later still counts at global-alloc time and
        // puts pMes/pStr (3 refs each, live around the loop) into different priority buckets.
        if (SysFlagChk(pG, SYS_HARD_RESET)) {
            goto stop;
        } else if (SysFlagChk(pG, SYS_CARD_ACCESS)) {
        stop:
            msg = -1;
            cont = 0;
        }
        if (msg != -1) {
            SysFlagOff(pG, SYS_SCREEN_STOP);
            if (shown == 0) {
                if (SysFlagChk(pG, SYS_SET_BLACK)) {
                    paused = 1;
                }
                PADControlMotor(0, 2);
                if (Sofdec.isPlay()) {
                    Sofdec.PlayPause(1);
                }
                if (pG->IsMessageInit == 1) {
                    pMes[4] = (u8*) (pG->pCore->ofs_6C + (u32) pG->pCore);
                    cMes.setLayout(0xF, LAYOUT_SYSTEM);
                }
                systemVISetBlack(0);
                GXSetCopyClear(BkBlack, 0xFFFFFF);
                GXCopyDisp(pCurrent_buff, 1);
                ScreenReSize(0x200, 0x1C0);
                str = pStr;
                do {
                    if (str->used == 1 && str->blk == 1) {
                        if (SndStrStatusCk(str->id, 0x10)) {
                            SndStrReq(str->id, 8, 0, 0);
                        }
                    }
                } while (++str <= &pStr[3]);
                shown = 1;
            }
            Render_before();
            PadRead();
            DiscReadInfo();
            if (msg != DVD_MES_NO_DISP) {
                if (pG->IsMessageInit == 1) {
                    MesSysMessage(msg, discNo);
                } else {
                    RomFontMessage(msg, discNo);
                }
            }
            if (Sofdec.isPlay()) {
                ADXM_ExecMain();
            }
            if (eprintf_init == 1) {
                EprintfFlush();
            }
            Render_done();
            Render_swap();
            while (vsync_cnt < (int) GetSystemVcnt()) {}
            vsync_cnt = 0;
            if (m_ErrCode != -1) {
                systemResetCheck();
            }
        }
    } while (cont);
    if (shown) {
        if (paused == 1) {
            systemVISetBlack(1);
        }
        Render_before();
        Render_done();
        Render_swap();
        while (vsync_cnt < (int) GetSystemVcnt()) {}
        vsync_cnt = 0;
        if (Sofdec.isPlay()) {
            Sofdec.PlayPause(0);
        }
    }
    return 1;
}

static u16 mes_pos[8][9][2] = {
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x32, 0xAA}, {0x37, 0x8C}, {0x32, 0x64}, {0x3C, 0x8C}, {0x3C, 0x78}},
    {{0, 0}, {0x32, 0x78}, {0x50, 0x78}, {0x32, 0xAA}, {0x32, 0xAA}, {0x28, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x32, 0xAA}, {0x37, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x64, 0x64}, {0x37, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x64, 0x64}, {0x37, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x64, 0x64}, {0x37, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x64, 0x64}, {0x37, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
    {{0, 0}, {0x3C, 0x78}, {0x32, 0x78}, {0x32, 0xAA}, {0x64, 0x64}, {0x37, 0x8C}, {0x32, 0x64}, {0, 0}, {0, 0}},
};

// Shows disc error message `msg` (disc-specific variants for "insert disc N") through cMes.
void MesSysMessage(int msg, int disc)
{
    int f = 1;

    if (DpfFlagChk(pG, DPF_MESSAGE) == 0) {
        f = 0;
    }
    u16* pos = mes_pos[pSys->language][0];
    u16 mes_no[12] = {0, 0, 1, 3, 3, 6, 7, 8, 2, 4, 9, 9};
    u16 no = msg;

    if (msg == DVD_MES_NO_DISC || msg == DVD_MES_WRONG_DISC) {
        msg += disc * 7;
    } else if (msg == DVD_MES_DISC_CHANGE) {
        msg = disc + 6;
    }
    pos += no * 2;
    cMes.MesSet(mes_no[msg], pos[0], pos[1], 0x01020090, 0xF, 0, 1);
    DpfFlagOff(pG, DPF_MESSAGE);
    cMes.Move();
    cMes.Trans();
    if (f == 1) {
        DpfFlagOn(pG, DPF_MESSAGE);
    }
}

// Prints a string with the IPL ROM font (used before the game font is loaded).
void RomFontPrint(int x, int y, const char* mes_ptr)
{
    RomFont* font = new RomFont(pG->FontData);
    void* image;
    s32 cx;
    s32 cy;
    s32 w;

    while (*mes_ptr) {
        mes_ptr = OSGetFontTexture(mes_ptr, &image, &cx, &cy, &w);
        font->setup(image);
        font->draw(x, y, cx, cy);
        x += w;
    }
    delete font;
}

// The disc error messages in the ROM font, per region language.
void RomFontMessage(u32 mes_no, int disc_no)
{
    switch (mes_no) {
    case DVD_MES_FATAL_ERROR:
        switch (pSys->language) {
        case 0:
            RomFontPrint(0x46, 0x78, "\203G\203\211\201[\202\252\224\255\220\266\202\265\202\334\202\265\202\275\201B");
            RomFontPrint(0x46, 0xA0, "\226{\221\314\202\314\203p\203\217\201[\203{\203^\203\223\202\360\211\237\202\265\202\304\223d\214\271\202\360");
            RomFontPrint(0x46, 0xC8, "\202n\202e\202e\202\311\202\265\201A\226{\221\314\202\314\216\346\210\265\220\340\226\276\217\221\202\314\216w\216\246\202\311");
            RomFontPrint(0x46, 0xF0, "\217]\202\301\202\304\202\255\202\276\202\263\202\242\201B");
            break;
        case 3:
            RomFontPrint(0x32, 0x64, "Ein Fehler ist aufgetreten.");
            RomFontPrint(0x32, 0x8C, "Bitte schalten Sie den");
            RomFontPrint(0x32, 0xB4, "Nintendo GameCube aus und lesen");
            RomFontPrint(0x32, 0xDC, "Sie die Bedienungsanleitung,");
            RomFontPrint(0x32, 0x104, "um weitere Informationen zu erhalten.");
            break;
        case 4:
            RomFontPrint(0x3C, 0x64, "Une erreur est survenue.");
            RomFontPrint(0x3C, 0x8C, "Eteignez la console et r\351f\351rez-vous");
            RomFontPrint(0x3C, 0xB4, "au manuel d'instructions");
            RomFontPrint(0x3C, 0xDC, "Nintendo GameCube pour de plus");
            RomFontPrint(0x3C, 0x104, "amples informations.");
            break;
        case 5:
            RomFontPrint(0x46, 0x64, "Se ha producido un error.");
            RomFontPrint(0x46, 0x8C, "Apaga la consola y consulta el");
            RomFontPrint(0x46, 0xB4, "manual de instrucciones de");
            RomFontPrint(0x46, 0xDC, "Nintendo GameCube para obtener");
            RomFontPrint(0x46, 0x104, "m\341s informaci\363n.");
            break;
        case 6:
            RomFontPrint(0x32, 0x78, "Si \350 verificato un errore.");
            RomFontPrint(0x32, 0xA0, "Spegni e consulta il manuale");
            RomFontPrint(0x32, 0xC8, "di istruzioni del Nintendo GameCube");
            RomFontPrint(0x32, 0xF0, "per ulteriori indicazioni.");
            break;
        case 1:
        case 2:
        case 7:
            RomFontPrint(0x32, 0x78, "An error has occurred.");
            RomFontPrint(0x32, 0xA0, "Turn the power off and refer to");
            RomFontPrint(0x32, 0xC8, "the Nintendo GameCube Instruction");
            RomFontPrint(0x32, 0xF0, "Booklet for further instructions.");
            break;
        }
        break;
    case DVD_MES_COVER_OPEN:
        switch (pSys->language) {
        case 0:
            RomFontPrint(0x32, 0x8C, "\203f\203B\203X\203N\203J\203o\201[\202\252\212J\202\242\202\304\202\242\202\334\202\267\201B");
            RomFontPrint(0x32, 0xB4, "\203Q\201[\203\200\202\360\221\261\202\257\202\351\217\352\215\207\202\315\201A\203f\203B\203X\203N\203J\203o\201[\202\360");
            RomFontPrint(0x32, 0xDC, "\225\302\202\337\202\304\202\255\202\276\202\263\202\242\201B");
            break;
        case 3:
            RomFontPrint(0x46, 0x8C, "Der Disc-Deckel ist ge\366ffnet.");
            RomFontPrint(0x46, 0xB4, "Bitte den Disc-Deckel schlie\337en,");
            RomFontPrint(0x46, 0xDC, "um mit dem Spiel fortzufahren.");
            break;
        case 4:
            RomFontPrint(0x5A, 0x8C, "Le couvercle est ouvert.");
            RomFontPrint(0x5A, 0xB4, "Pour continuer \340 jouer,");
            RomFontPrint(0x5A, 0xDC, "veuillez fermer le couvercle.");
            break;
        case 5:
            RomFontPrint(0x6E, 0x8C, "La tapa est\341 abierta.");
            RomFontPrint(0x6E, 0xB4, "Si quieres seguir jugando,");
            RomFontPrint(0x6E, 0xDC, "debes cerrar la tapa.");
            break;
        case 6:
            RomFontPrint(0x46, 0x8C, "Il coperchio del disco \350 aperto.");
            RomFontPrint(0x46, 0xB4, "Se vuoi proseguire nel gioco,");
            RomFontPrint(0x46, 0xDC, "chiudi il coperchio del disco.");
            break;
        case 1:
        case 2:
        case 7:
            RomFontPrint(0x46, 0x8C, "The Disc Cover is open.");
            RomFontPrint(0x46, 0xB4, "If you want to continue the game,");
            RomFontPrint(0x46, 0xDC, "please close the Disc Cover.");
            break;
        }
        break;
    case DVD_MES_NO_DISC:
    case DVD_MES_WRONG_DISC:
        switch (pSys->language) {
        case 0:
            if (disc_no == 0) {
                RomFontPrint(0x46, 0xA0, "\202\202\202\211\202\217\202\210\202\201\202\232\202\201\202\222\202\204\202S\202\314\203f\203B\203X\203N\202P\202\360");
            } else {
                RomFontPrint(0x46, 0xA0, "\202\202\202\211\202\217\202\210\202\201\202\232\202\201\202\222\202\204\202S\202\314\203f\203B\203X\203N\202Q\202\360");
            }
            RomFontPrint(0x46, 0xC8, "\203Z\203b\203g\202\265\202\304\202\255\202\276\202\263\202\242\201B");
            break;
        case 3:
            RomFontPrint(0x46, 0xA0, "Bitte legen Sie die resident evil 4");
            if (disc_no == 0) {
                RomFontPrint(0x46, 0xC8, "Disc 1 ein.");
            } else {
                RomFontPrint(0x46, 0xC8, "Disc 2 ein.");
            }
            break;
        case 4:
            RomFontPrint(0x46, 0xA0, "Veuillez ins\351rer le disque de jeu");
            if (disc_no == 0) {
                RomFontPrint(0x46, 0xC8, "1 de resident evil 4");
            } else {
                RomFontPrint(0x46, 0xC8, "2 de resident evil 4");
            }
            break;
        case 5:
            if (disc_no == 0) {
                RomFontPrint(0x32, 0xA0, "Coloca el disco 1 de resident evil 4");
            } else {
                RomFontPrint(0x32, 0xA0, "Coloca el disco 2 de resident evil 4");
            }
            break;
        case 6:
            if (disc_no == 0) {
                RomFontPrint(0x5A, 0xA0, "Inserisci il disco di gioco 1 di");
            } else {
                RomFontPrint(0x5A, 0xA0, "Inserisci il disco di gioco 2 di");
            }
            RomFontPrint(0x5A, 0xC8, "resident evil 4");
            break;
        case 1:
        case 2:
        case 7:
            RomFontPrint(0x5A, 0xA0, "Please insert the resident evil 4");
            if (disc_no == 0) {
                RomFontPrint(0x5A, 0xC8, "Game Disc 1.");
            } else {
                RomFontPrint(0x5A, 0xC8, "Game Disc 2.");
            }
            break;
        }
        break;
    case DVD_MES_RETRY_ERROR:
        switch (pSys->language) {
        case 0:
            RomFontPrint(0x3C, 0x8C, "\203f\203B\203X\203N\202\360\223\307\202\337\202\334\202\271\202\361\202\305\202\265\202\275\201B");
            RomFontPrint(0x3C, 0xB4, "\202\255\202\355\202\265\202\255\202\315\201A\226{\221\314\202\314\216\346\210\265\220\340\226\276\217\221\202\360\202\250\223\307\202\335");
            RomFontPrint(0x3C, 0xDC, "\202\255\202\276\202\263\202\242\201B");
            break;
        case 3:
            RomFontPrint(0x1E, 0x64, "Diese Game Disc kann nicht gelesen");
            RomFontPrint(0x1E, 0x8C, "werden.");
            RomFontPrint(0x1E, 0xB4, "Bitte lesen Sie die Bedienungsanleitung");
            RomFontPrint(0x1E, 0xDC, "des Nintendo GameCube, um weitere");
            RomFontPrint(0x1E, 0x104, "Informationen zu erhalten.");
            break;
        case 4:
            RomFontPrint(0x46, 0x78, "La lecture du disque a \351chou\351.");
            RomFontPrint(0x46, 0xA0, "Veuillez vous r\351f\351rer au manuel");
            RomFontPrint(0x4C, 0xC8, "d'instructions Nintendo GameCube");
            RomFontPrint(0x46, 0xF0, "pour de plus amples informations.");
            break;
        case 5:
            RomFontPrint(0x32, 0x78, "No se puede leer el disco.");
            RomFontPrint(0x32, 0xA0, "Consulta el manual de instrucciones");
            RomFontPrint(0x32, 0xC8, "de Nintendo GameCube para obtener");
            RomFontPrint(0x32, 0xF0, "m\341s informaci\363n.");
            break;
        case 6:
            RomFontPrint(0x32, 0x78, "Impossibile leggere il disco di gioco.");
            RomFontPrint(0x32, 0xA0, "Per ulteriori indicazioni consulta il");
            RomFontPrint(0x32, 0xC8, "manuale di istruzioni del");
            RomFontPrint(0x32, 0xF0, "Nintendo GameCube.");
            break;
        case 1:
        case 2:
        case 7:
            RomFontPrint(0x32, 0x78, "The Game Disc could not be read.");
            RomFontPrint(0x32, 0xA0, "Please read the Nintendo GameCube");
            RomFontPrint(0x32, 0xC8, "Instruction Booklet for more");
            RomFontPrint(0x32, 0xF0, "information.");
            break;
        }
        break;
    }
}

// European region codes share one disc id. The tests are inline calls: a `||` chain on the same
// lvalue is range-folded (`subi; cmplwi`), separate `if`s make the last test a setcc; a chain of
// CALL_EXPRs (side effects) is never merged by fold and gives the five compares to one `li 1`.
static inline int SysRegionIs(int r)
{
    return pSys->eff_country == r;
}

// 1 for the European regions.
static inline int SysIsEurope()
{
    if (SysRegionIs(2) || SysRegionIs(3) || SysRegionIs(4) || SysRegionIs(5) || SysRegionIs(6)) {
        return 1;
    }
    return 0;
}

// Requests the swap to `disc` (1 / 2): builds the disc id for the region's game code and starts
// DVDChangeDiskAsync, then runs ErrCheck until the right disc is in. 1 when started.
int cDvd::DiscChange(int disc_no)
{
    DVDDiskID id;
    DVDCommandBlock cb;
    int region = 0;
    char company[] = "08";
    const char* game[] = {"G4BJ", "G4BE", "G4BJ", "G4BJ"};

    if (pSys->eff_country == 1) {
        region = 1;
    } else if (SysIsEurope() == 1) {
        region = 2;
    } else if (pSys->eff_country == 7) {
        region = 3;
    }
    DVDGenerateDiskID(&id, game[region], company, (u8) disc_no, 0xFF);
    if (DVDCompareDiskID(DVDGetCurrentDiskID(), &id) == 1) {
        return 0;
    }
    discChanged = 1;
    DVDChangeDiskAsync(&cb, &id, 0);
    ErrCheck(disc_no, 1);
    FileTblExistCheck();
    return 1;
}

// The current disc number (from the disc id, or the pending change).
int cDvd::GetDiscNo()
{
    int no = DVDGetCurrentDiskID()->diskNumber;

    if (discChanged) {
        no ^= 1;
    }
    return no;
}

// Loads the IPL ROM font into pG->FontData for the error screens.
void RomFontSetting()
{
    if (OSGetFontEncode() == 1) {
        pG->FontData = (void*) 0x816D3100;
    } else {
        pG->FontData = (void*) 0x817D3EE0;
    }
    OSInitFont((OSFontHeader*) pG->FontData);
}

char* queue_stat[] = {"PUSH", "READ", "COMPLETE", "CANCEL", "ERROR"};

// Debug page 0x15: every queue slot's status and file, the running request and the pending list.
void cDvd::queueStatusDisp()
{
    cDvdQueue* p;
    int i;
    int y = 0x40;
    int y2;
    int x;

    for (i = 0; i < 16; y += 0x10, i++) {
        eprintf(0x20, y, 0, 0x15, "[%2d]", i);
        p = &Dvd.DvdQueue[i];
        if (Dvd.DvdQueue[i].CkFlag(1) == 1) {
            eprintf(0x48, y, 0, 0x15, "%s %s", queue_stat[Dvd.DvdQueue[i].GetQueueStatus()], p->m_Name);
        }
    }
    y += 0x10;
    if (Dvd.pCur_queue) {
        i = 0;
        y2 = y + 0x10;
        for (; i < 16; i++) {
            if (Dvd.pCur_queue == &Dvd.DvdQueue[i]) {
                eprintf(0x20, y, 0, 0x15, "READ : [%2d]", i);
            }
        }
    } else {
        eprintf(0x20, y, 0, 0x15, "READ :");
        y2 = y + 0x10;
    }
    if (Dvd.pQueue_list) {
        eprintf(0x20, y2, 0, 0x15, "LIST : ");
        x = 0x58;
        for (p = Dvd.pQueue_list; p; p = p->m_Next) {
            for (i = 0; i < 16; i++) {
                if (p == &Dvd.DvdQueue[i]) {
                    if (p->m_Next == 0) {
                        eprintf(x, y2, 0, 0x15, "[%2d]", i);
                    } else {
                        eprintf(x, y2, 0, 0x15, "[%2d]-", i);
                    }
                    x += 0x28;
                }
            }
        }
    } else {
        eprintf(0x20, y2, 0, 0x15, "LIST :");
    }
}

// Debug text: the running read's error code, name, disc address / length and transfer progress.
void cDvd::DiscReadInfo()
{
    cDvdQueue* q = pCur_queue;
    DVDFileInfo* fi = &q->m_Info;
    DvdSndStrWork* s;
    int y;

    if (eprintf_init == 0) {
        return;
    }
    if (q) {
        eprintf2(0xA, 0x10, 0x20, 0x140, 0, 0, "%2d  %s", m_ErrCode, q->m_Name);
        eprintf2(0xA, 0x10, 0x20, 0x150, 0, 0, "%08x %06x", fi->startAddr, fi->length);
        eprintf2(0xA, 0x10, 0x20, 0x160, 0, 0, "%08x %06x %06x", fi->cb.offset, fi->cb.length, pCur_queue->m_LeftSize);
        eprintf2(0xA, 0x10, 0x20, 0x170, 0, 0, "%08x %06x %06x", fi->cb.currTransferSize, fi->cb.transferredSize, DVDGetTransferredSize(fi));
    }
    s = Snd_str_work;
    y = 0x1AA;
    do {
        if (s->status & 1) {
            eprintf2(0xA, 0x10, 0xF0, y, 0, 0, "%d %04x %04x %02x %02x %02x", s->no, s->status, s->req, s->err, s->cancel, s->state);
            y -= 0x10;
        }
    } while (++s <= &Snd_str_work[3]);
}

// The original's .rodata is 8-aligned (0x1D98: 4 bytes of end padding after the last pool).
asm(".section .rodata; .balign 8");
