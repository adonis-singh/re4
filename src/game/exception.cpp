// game/exception: OS error handler with symbol lookup and register/memory dump (D:/Bio4/Prog/exception.cpp).
// ExceptionInit installs ErrorHandler for the CPU exceptions; on a crash it saves the context,
// walks the stack chain into call_stack, prints registers/FPSCR/heap state to the console, loads
// the .sym files of the DOL and every loaded REL (excepLoadSymbol) to name the PC and return
// addresses, and then loops forever drawing an on-screen dump (registers, call stack, a scrollable
// memory viewer driven by the pad) until the reset button is pressed.
#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "joy.h"
#include "pad.h"
#include "dvd.h"
#include "read.h"
#include "room_data.h"
#include "sscrn.h"
#include "eprintf.h"
#include "va_ppc.h"

#define _DOLPHIN_TYPES_H_
#include <dolphin/os/OSError.h>

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
char* strchr(const char* s, int c);
int DBIsDebuggerPresent();
void PPCMtmsr(u32 msr);
void OSEnableScheduler();
s32 OSCheckHeap(int heap);
u32 OSGetResetButtonState();
void OSResetSystem(int reset, u32 resetCode, int forceMenu);
void EprintfFlush();
// game/exception.cpp
void ExceptionInit();
int excepLoadSymbolSub(char* name, OSModuleHeader* module);
void excepLoadSymbol();
char* excepGetSymbolName(u32 addr);
void ErrorHandler(OSError error, OSContext* context, ...);
}

// Symbol file (Bio4*.sym) header: entries, file names and symbol names by offset.
struct SymEntry {
    u32 addr;  // 0x00
    u32 size;  // 0x04
    u16 file;  // 0x08  index into SymHeader::fileOfs
    u16 pad;
    u32 name;  // 0x0C  offset into the name block
};

struct SymHeader {
    u32 num;         // 0x00
    u32 ofsEntry;    // 0x04
    u32 ofsFile;     // 0x08  file name block
    u32 ofsName;     // 0x0C  symbol name block
    u32 fileOfs[1];  // 0x10
};

// A loaded symbol file and the load address of the module it describes.
struct SymbolInfo {
    SymHeader* symbol_ptr;  // 0x00
    u32 base;        // 0x04
};

// Memory dump window state (`test`).
struct MemDump {
    s8 x0;
    s8 mode;      // 0x01  0 browse, 1 edit the jump address
    s8 x2;
    s8 x3;
    s8 width;     // 0x04  0 bytes, 1 halfwords, 2 words
    u8 pad_5[3];
    u32 addr;     // 0x08  first address shown
    u32 curAddr;  // 0x0C  jump address
};

extern "C" {
char* excepGetSymbolNameSub(u32 addr, SymbolInfo* info);
void excepMemoryDumpMove(MemDump* w, int y);
void excepMemoryDump(MemDump* w, int y);
void excepRegConsoleDump(int error, u32 dsisr, u32 dar);
}

// A store through a scalar reference is not a struct-member MEM: the static `addr` is reloaded
// after it, as the original does.
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void ISet(int& d, int v) { d = v; }

#line 40 "D:/Bio4/Prog/exception.cpp"

static int x = 0;
static int y = 0;
MemDump test asm("test_802F1358");  // a second global `test` (db_menu.cpp has `test test`); the link needs the split object's name
OSContext sv_context;
static OSContext* pContext = &sv_context;
static u32 call_stack[16];
static int call_stack_num;
SymbolInfo symbolInfo[9];
static int nSymbolInfo;
char tmp_str[256];
static int symbol_err = 0;

// excepLoadSymbolSub results
const char* const symbol_err_tbl[] = {
    "ready",
    "success",
    "memory error",
    "read error",
    "open error",
};

const char* const e_str[] = {
    "OS_ERROR_SYSTEM_RESET",
    "OS_ERROR_MACHINE_CHECK",
    "OS_ERROR_DSI",
    "OS_ERROR_ISI",
    "OS_ERROR_EXTERNAL_INTERRUPT",
    "OS_ERROR_ALIGNMENT",
    "OS_ERROR_PROGRAM",
    "OS_ERROR_FLOATING_POINT",
    "OS_ERROR_DECREMENTER",
    "OS_ERROR_SYSTEM_CALL",
    "OS_ERROR_TRACE",
    "OS_ERROR_PERFORMANCE_MONITOR",
    "OS_ERROR_BREAKPOINT",
    "OS_ERROR_SYSTEM_INTERRUPT",
    "OS_ERROR_THERMAL_INTERRUPT",
    "OS_ERROR_PROTECTION",
    "OS_ERROR_FPE",
};

// Console print of the floating-point exception bits set in FPSCR.
// Dead-stripped in the original (STRIP_UNUSED): only its strings survive in .rodata.
static void excepFpscrDump(u32 fpscr)
{
    OSReport("-- %08x --", fpscr);
    if (fpscr & 0x20000000) {
        OSReport("FPE: Invalid operation: ");
        if (fpscr & 0x01000000) {
            OSReport("SNaN\n");
        }
        if (fpscr & 0x00800000) {
            OSReport("Infinity - Infinity\n");
        }
        if (fpscr & 0x00400000) {
            OSReport("Infinity / Infinity\n");
        }
        if (fpscr & 0x00200000) {
            OSReport("0 / 0\n");
        }
        if (fpscr & 0x00100000) {
            OSReport("Infinity * 0\n");
        }
        if (fpscr & 0x00080000) {
            OSReport("Invalid compare\n");
        }
        if (fpscr & 0x00000400) {
            OSReport("Software request\n");
        }
        if (fpscr & 0x00000200) {
            OSReport("Invalid square root\n");
        }
        if (fpscr & 0x00000100) {
            OSReport("Invalid integer convert\n");
        }
    }
    if (fpscr & 0x10000000) {
        OSReport("FPE: Overflow\n");
    }
    if (fpscr & 0x08000000) {
        OSReport("FPE: Underflow\n");
    }
    if (fpscr & 0x04000000) {
        OSReport("FPE: Zero division\n");
    }
    if (fpscr & 0x02000000) {
        OSReport("FPE: Inexact result\n");
    }
}

// Installs ErrorHandler for the fatal exceptions (program/trace/breakpoint only when no debugger
// is attached). Called from main at boot.
void ExceptionInit()
{
    OSSetErrorHandler(OS_ERROR_SYSTEM_RESET, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_MACHINE_CHECK, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_DSI, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_ISI, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_ALIGNMENT, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_PERFORMACE_MONITOR, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_SYSTEM_INTERRUPT, ErrorHandler);
    OSSetErrorHandler(OS_ERROR_THERMAL_INTERRUPT, ErrorHandler);
    if (!DBIsDebuggerPresent()) {
        OSSetErrorHandler(OS_ERROR_PROGRAM, ErrorHandler);
        OSSetErrorHandler(OS_ERROR_TRACE, ErrorHandler);
        OSSetErrorHandler(OS_ERROR_BREAKPOINT, ErrorHandler);
    }
}

// Loads one .sym file synchronously from disc into symbolInfo[] (max 9); `module` gives the REL whose
// text base offsets the entries (0 for the DOL). Returns a symbol_err_tbl code: 1 ok, 2 memory,
// 4 open error, 5 table full.
// Reads a symbol file; returns the symbol_err_tbl index (1 ok, 2 memory, 4 open, 5 table full).
int excepLoadSymbolSub(char* name, OSModuleHeader* module)
{
    int ret = 1;
    void* addr;
    int r;

    if (nSymbolInfo > 8) {
        return 5;
    }
#line 199 "D:/Bio4/Prog/exception.cpp"
    r = Dvd.ReadCheck(DvdReadN(name, 0, 0, 0, 0, 3, __FILE__, __LINE__), 0, 0, &addr);
    if (r < 0) {
        switch (r) {
        case -3:
            ret = 2;
            break;
        case -1:
            ret = 4;
            break;
        }
    } else {
        SymbolInfo* p = &symbolInfo[nSymbolInfo];
        p->symbol_ptr = (SymHeader*) addr;
        nSymbolInfo++;
        if (module) {
            p->base = module->sectionInfo[1].offset - 1;
        } else {
            p->base = (u32) module;
        }
    }
    return ret;
}

// Loads bio4.sym and the .sym of each loaded module: the enemy modules (EmReadModule, with the
// pl10/pl11/pl14/pl0e/pl0f/emmark special names), the player module, the weapon module, the
// sub-screen module and the room module (name from FileTbl).
void excepLoadSymbol()
{
    int i;
    char buf[32];

    symbol_err = excepLoadSymbolSub("bio4.sym", 0);
    for (i = 0; i < 4; i++) {
        if (EmReadModule[i].pModule) {
            switch (EmReadModule[i].id) {
            case 2:
                sprintf(tmp_str, "Bio4.pl10.sym");
                break;
            case 3:
            case 5:
                sprintf(tmp_str, "Bio4.pl11.sym");
                break;
            case 4:
                sprintf(tmp_str, "Bio4.pl14.sym");
                break;
            case 0xE:
                sprintf(tmp_str, "Bio4.pl0e.sym");
                break;
            case 0xF:
                sprintf(tmp_str, "Bio4.pl0f.sym");
                break;
            case 0x3E:
                sprintf(tmp_str, "Bio4.emmark.sym");
                break;
            default:
                sprintf(tmp_str, "Bio4.em%02x.sym", (u8) EmReadModule[i].id);
                break;
            }
            symbol_err = excepLoadSymbolSub(tmp_str, EmReadModule[i].pModule);
        }
    }
    if (PlReadModule.pModule) {
        switch (PlReadModule.id) {
        default:
        case 0:
            sprintf(tmp_str, "Bio4.pl02.sym");
            break;
        case 0xBC:
            sprintf(tmp_str, "Bio4.pl06.sym");
            break;
        case 0xC5:
            sprintf(tmp_str, "Bio4.pl0a.sym");
            break;
        case 0xC7:
            sprintf(tmp_str, "Bio4.pl0d.sym");
            break;
        }
        symbol_err = excepLoadSymbolSub(tmp_str, PlReadModule.pModule);
    }
    if (WepReadModule.pModule) {
        sprintf(tmp_str, "Bio4.wep%02x.sym", (u8) WepReadModule.id);
        symbol_err = excepLoadSymbolSub(tmp_str, WepReadModule.pModule);
    }
    if (SubScreenWk.p_module) {
        OSModuleHeader* mod = SubScreenWk.p_module;
        if ((s32) mod < 0 && (u32) mod <= 0x82FFFFFF && (s32) mod->sectionInfo < 0) {
            symbol_err = excepLoadSymbolSub("Bio4.Sscrn.sym", mod);
        }
    }
    if (RoomData.pModule) {
        OSModuleHeader* mod = RoomData.pModule;
        if ((s32) mod < 0 && (u32) mod <= 0x82FFFFFF && (s32) mod->sectionInfo < 0) {
            strcpy(buf, FileTbl[RoomData.m_RelNo].name + 4);
            *strchr(buf, '.') = 0;
            sprintf(tmp_str, "Bio4.%s.sym", buf);
            symbol_err = excepLoadSymbolSub(tmp_str, RoomData.pModule);
        }
    }
}

// "name (file)" of the symbol containing addr across all loaded tables, or "unknown".
char* excepGetSymbolName(u32 addr)
{
    static char null_data[] = "unknown";
    int i;

    if (nSymbolInfo == 0) {
        return null_data;
    }
    for (i = 0; i < nSymbolInfo; i++) {
        char* s = excepGetSymbolNameSub(addr, &symbolInfo[i]);
        if (s) {
            return s;
        }
    }
    return null_data;
}

// Linear search of one symbol table for the entry whose [addr, addr+size) contains the address.
char* excepGetSymbolNameSub(u32 addr, SymbolInfo* info)
{
    SymHeader* h = info->symbol_ptr;
    int n = h->num;
    u32* fo = h->fileOfs;
    SymEntry* e = (SymEntry*) ((u8*) h + h->ofsEntry);
    char* files = (char*) h + h->ofsFile;
    char* names = (char*) h + h->ofsName;
    int i;

    for (i = 0; i < n; i++, e++) {
        u32 start = e->addr + info->base;
        u32 end = e->addr + e->size + info->base;
        if (start <= addr && addr < end) {
            sprintf(tmp_str, "%s (%s)", names + e->name, files + fo[e->file]);
            return tmp_str;
        }
    }
    return 0;
}

// Pad control of the memory viewer: mode 0 browses (Z cycles byte/half/word width, X enters the
// address editor, stick/D-pad scroll), mode 1 edits the jump address digit by digit.
void excepMemoryDumpMove(MemDump* w, int y)
{
    static s8 index = 0;
    static u32 addr;
    static u32 wk;
    int shift;

    switch (w->mode) {
    case 0:
        if (Joy[0].trg & 0x800) {
            w->width++;
            if (w->width > 2) {
                w->width = 0;
            }
        }
        if (Joy[0].trg & 0x100) {
            addr = w->curAddr;
            w->mode++;
        }
        if (Joy[0].rep & 0x80008) {
            if (w->addr > 0x80000000) {
                w->addr -= 0x10;
            }
        }
        if (Joy[0].rep & 0x40004) {
            if (w->addr <= 0x817FFFEF) {
                w->addr += 0x10;
            }
        }
        break;
    case 1:
        if (Joy[0].trg & 0x1) {
            if (index != 0) {
                index--;
            }
        }
        if (Joy[0].trg & 0x2) {
            if (index <= 6) {
                index++;
            }
        }
        shift = 28 - index * 4;
        wk = (addr >> shift) & 0xF;
        if (Joy[0].trg & 0x8) {
            wk = (wk + 1) & 0xF;
        }
        if (Joy[0].trg & 0x4) {
            wk = (wk - 1) & 0xF;
        }
        addr = (addr & ~(0xF << shift)) | (wk << shift);
        eprintf(x + 0x140, y + 0x10, 0, 0, "JUMP TO : %08X", addr);
        eprintf(index * 8 + x + 0x190, y + 0x10, 2, 0, "%1X", wk);
        if (Joy[0].trg & 0x100) {
            w->mode = 0;
            if ((addr & ~0xF) >= 0x80000000 && (addr & ~0xF) <= 0x817FFFF0) {
                U32Set(w->addr, addr & ~0xF);
                w->curAddr = addr;
            }
        }
        if (Joy[0].trg & 0x200) {
            w->mode = 0;
        }
        break;
    }
}

// Draws 16 lines of memory from w->addr at the viewer's width, plus the address editor.
void excepMemoryDump(MemDump* w, int y)
{
    int i;
    u8* p;

    eprintf(x + 0xF0, y + 0x10, 0, 0, "MEM DUMP");
    excepMemoryDumpMove(w, y);
    p = (u8*) w->addr;
    switch (w->width) {
    case 0:
        for (i = 0; i < 16 && (u32) p <= 0x817FFFFF; i++) {
            eprintf(x + 0xF0, y + 0x20 + i * 0x10, 0, 0,
                    "%08X: %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X", p,
                    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14],
                    p[15]);
            p += 0x10;
        }
        break;
    case 1:
        for (i = 0; i < 16 && (u32) p <= 0x817FFFFF; i++) {
            u16* h = (u16*) p;
            eprintf(x + 0xF0, y + 0x20 + i * 0x10, 0, 0, "%08X: %04X %04X %04X %04X %04X %04X %04X %04X", p, h[0],
                    h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
            p += 0x10;
        }
        break;
    case 2:
        for (i = 0; i < 16 && (u32) p <= 0x817FFFFF; i++) {
            u32* l = (u32*) p;
            eprintf(x + 0xF0, y + 0x20 + i * 0x10, 0, 0, "%08X: %08X %08X %08X %08X", p, l[0], l[1], l[2], l[3]);
            p += 0x10;
        }
        break;
    }
}

// Console dump after a crash: error name, DSISR/DAR, PC/LR with symbol names, GPRs, FPSCR, the
// call stack, and the free size of the game and debug heaps.
void excepRegConsoleDump(int error, u32 dsisr, u32 dar)
{
    int i;
    s32 freeMem;
    s32 freeDbg;
    u32 pc;

    OSReport("\n-------EXCEPTION INFORMATION--------\n");
    OSReport("%s %d\n", e_str[error], error);
    OSReport(" PC: %08X  DAR: %08X  SP: %08X\n", pContext->srr0, dar, pContext->gpr[1]);
    OSReport("\n");
    for (i = 0; i < 8; i++) {
        OSReport("r%02d: %08X  r%02d: %08X  r%02d: %08X  r%02d: %08X\n", i, pContext->gpr[i], i + 8,
                 pContext->gpr[i + 8], i + 16, pContext->gpr[i + 16], i + 24, pContext->gpr[i + 24]);
    }
    OSReport("\n");
    OSReport("  CR: %08X    LR: %08X    CTR: %08X  XER: %08X\n", pContext->cr, pContext->lr, pContext->ctr,
             pContext->xer);
    OSReport("SRR0: %08X  SRR1: %08X  DSISR: %08X  DAR: %08X\n", pContext->srr0, pContext->srr1, dsisr, dar);
    OSReport("\n");
    freeMem = OSCheckHeap(Heap[MemGetCurrentHeap()].handle);
    freeDbg = OSCheckHeap(Heap[MemGetCurrentDbgHeap()].handle);
    OSReport("free mem: %08X   free dbg: %08X\n", freeMem, freeDbg);
    OSReport("\n");
    OSReport("CALL STACK (%s)\n", symbol_err_tbl[symbol_err]);
    pc = pContext->srr0;
    OSReport("%08X %s\n", pc, excepGetSymbolName(pc));
    for (i = 1; i < call_stack_num; i++) {
        pc = call_stack[i];
        OSReport("%08X %s\n", pc, excepGetSymbolName(pc));
    }
    OSReport("-------EXCEPTION INFORMATION--------\n");
    OSReport("\n");
}

// The installed OS error handler: copies the context, re-enables interrupts and the scheduler, walks
// the stack chain (up to 16 frames), dumps to the console, then loops forever rendering the
// on-screen crash display (loading symbols after 60 frames), scrolled with the C stick, with the
// memory viewer, until the reset button is pressed.
// The loop-invariant `lis` of "DSISR: %08X  DAR: %08X", symbol_err_tbl and "CALL STACK (%s)" are
// gcse PRE pseudos of equal priority, numbered (and so allocated r16/r15/r14) in hash-bucket order:
// bucket = (h(name) + 90) % 253 with h = h*129 + c per char ("*.LCn" for a string label), 253 =
// n_insns/2|1 buckets. Our TU numbered the strings .LC64/.LC65 (buckets 158/159, both above the
// table's 150); the original's were .LC79/.LC80 (39/159: the only wrap that straddles 150 besides
// .LC59/.LC60), i.e. its TU had 15 more constants before them. The 15 dead `f32 lcN = K;` locals
// below consume 15 pool labels (force_const_mem numbers them at expand; the dead loads are deleted
// before gcse and the unreferenced pool entries are never output, so .rodata and the insn count are
// unchanged). The hash formula was calibrated on the -dG dump (.LC60..66 -> 154..160).
void ErrorHandler(OSError error, OSContext* context, ...)
{
    va_list ap;
    u32 dsisr;
    u32 dar;
    u32* sp;
    u32* cs;
    u32 pc;
    int i;
    int n;
    MemDump* w;
    int col;
    static int timer = 0;
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering): 15 dead pool constants shift the string
    // labels to .LC79/.LC80 (see the comment above the function).
    f32 lc0 = 1.5f;
    f32 lc1 = 2.5f;
    f32 lc2 = 3.5f;
    f32 lc3 = 4.5f;
    f32 lc4 = 5.5f;
    f32 lc5 = 6.5f;
    f32 lc6 = 7.5f;
    f32 lc7 = 8.5f;
    f32 lc8 = 9.5f;
    f32 lc9 = 10.5f;
    f32 lc10 = 11.5f;
    f32 lc11 = 12.5f;
    f32 lc12 = 13.5f;
    f32 lc13 = 14.5f;
    f32 lc14 = 15.5f;

    *pContext = *context;
    va_start(ap, context);
    dsisr = va_arg(ap, u32);
    dar = va_arg(ap, u32);
    PPCMtmsr(0xB032);
    OSEnableScheduler();
    SysFlagOn(pG, SYS_EXCEPTION);
    SysFlagOff(pG, SYS_SCREEN_STOP);
    w = &test;
    n = 0;
    memclr_asm(w, sizeof(MemDump));
    ISet(call_stack_num, n); // the reference store keeps `lwz pContext` below it
    sp = (u32*) pContext->gpr[1];
    if (sp != 0 && sp != (u32*) -1) {
        i = 0;
        do {
            call_stack[i + 1] = sp[1];
            call_stack_num++;
            sp = (u32*) sp[0];
            i++;
        } while (sp != 0 && sp != (u32*) -1 && i < 16);
    }
    nSymbolInfo = 0;
    w->addr = 0x80400000;
    w->curAddr = 0x80400000;
    pG->debug_mode = 1;
    excepRegConsoleDump(error, dsisr, dar);
    systemVISetBlack(0);
    for (;;) {
        timer++;
        if (timer == 60) {
            excepLoadSymbol();
            excepRegConsoleDump(error, dsisr, dar);
        }
        Render_before();
        PadRead();
        if (Joy[0].substickX != 0) {
            x -= Joy[0].substickX;
            x = x < -200 ? -200 : (x > 0 ? 0 : x);
        }
        if (Joy[0].substickY != 0) {
            y += Joy[0].substickY;
            y = y < -256 ? -256 : (y > 0 ? 0 : y);
        }
        if ((u32) (timer & 3) > 1) {
            col = 2;
        } else {
            col = 0;
        }
        eprintf(x + 10, y + 0x10, col, 0, "%s", e_str[error]);
        eprintf(x + 10, y + 0x20, 0, 0, " PC: %08X", pContext->srr0);
        eprintf(x + 10, y + 0x30, 0, 0, "DAR: %08X", dar);
        eprintf(x + 10, y + 0x40, 0, 0, " SP: %08X", pContext->gpr[1]);
        eprintf(x + 0x7A, y + 0x20, 0, 0, "mem: %08X", OSCheckHeap(Heap[MemGetCurrentHeap()].handle));
        eprintf(x + 0x7A, y + 0x30, 0, 0, "dbg: %08X", OSCheckHeap(Heap[MemGetCurrentDbgHeap()].handle));
        eprintf(x + 0x7A, y + 0x40, 0, 0, "FPSCR:%08X", pContext->fpscr);
        for (i = 0; i < 16; i++) {
            eprintf(x + 10, y + 0x60 + i * 0x10, 0, 0, "r%02d: %08X r%02d: %08X", i, pContext->gpr[i], i + 16,
                    pContext->gpr[i + 16]);
        }
        eprintf(x - 6, y + 0x170, 0, 0, "   CR: %08X   LR: %08X", pContext->cr, pContext->lr);
        eprintf(x - 6, y + 0x180, 0, 0, "  CTR: %08X  XER: %08X", pContext->ctr, pContext->xer);
        eprintf(x - 6, y + 0x190, 0, 0, " SRR0: %08X SRR1: %08X", pContext->srr0, pContext->srr1);
        eprintf(x - 6, y + 0x1A0, 0, 0, "DSISR: %08X  DAR: %08X", dsisr, dar);
        eprintf(x + 0xF0, y + 0x120, 0, 0, "CALL STACK (%s)", symbol_err_tbl[symbol_err]);
        pc = pContext->srr0;
        eprintf(x + 0xF0, y + 0x130, 0, 0, "%08X %s", pc, excepGetSymbolName(pc));
        i = 1;
        if (i < call_stack_num) {
            cs = &call_stack[i];
            do {
                u32 addr = *cs++;
                eprintf(x + 0xF0, y + 0x130 + i * 0x10, 0, 0, "%08X %s", addr, excepGetSymbolName(addr));
                i++;
            } while (i < call_stack_num);
        }
        excepMemoryDump(w, y);
        EprintfFlush();
        Render_done();
        Render_swap();
        while (vsync_cnt < GetSystemVcnt()) {}
        vsync_cnt = 0;
        if (OSGetResetButtonState() == 1) {
            OSResetSystem(1, 0, 0);
        }
    }
}
