#ifndef DB_LOG_H
#define DB_LOG_H

#include "types.h"
#include "va_ppc.h"

// One debug log line (0x4C bytes).
struct cLogWork {
    int m_Id;        // 0x00  duplicate-suppression key (0 = none)
    u8 m_Col;       // 0x04  eprintf color
    u8 pad_5[3];
    char m_Str[64];   // 0x08
    u8 pad_48[4];

    void print(int x, int y);
    void clear();
};

// cLog::add flag bits
#define LOG_NO_SHOW    1  // do not restart the display timer
#define LOG_DUP_QUIET  2  // a duplicate line does not restart the display timer either
#define LOG_NO_PRINTF  4  // do not echo the line to the console

// Debug log (src/game/db_log.cpp), 0x1DC0 bytes, allocated from the debug heap by LogInit.
class cLog {
public:
    u8 m_Flag;             // 0x00  bit1 = a line was added this frame, bit0 = it was a duplicate
    u8 m_RepeatCtr;             // 0x01  duplicate marker animation counter (0..7)
    u8 pad_2[4];
    u8 m_DispTimer;             // 0x06  frames left to display (0xFF = always)
    u8 m_BuffIdx;               // 0x07  index of the newest line
    u8 m_DispTime;              // 0x08  display duration set by modeSet
    u8 m_DispNum;             // 0x09  visible lines
    s16 m_Bx;                // 0x0A
    s16 m_By;                // 0x0C
    u8 m_ScrOfs;               // 0x0E  scroll offset
    u8 pad_F;
    cLogWork m_Mes[100];   // 0x10

    void init();
    void mes(int a, int b, const char* fmt, ...);
    void err(int a, int b, const char* fmt, ...);
    void warn(int a, int b, const char* fmt, ...);
    void vmes(int flag, int col, const char* mes, va_list argptr);
    void verr(int flag, int errId, const char* mes, va_list argptr);
    void vwarn(int flag, int errId, const char* mes, va_list argptr);
    void clear();
    int modeReset();
    int modeSet(int x, int y, int dispTime, int dispNum);   // window position/duration/size (t_log: 0x30, 0x2A, 0xFF, 0x19)
    void disp();
    void setPos(int x, int y) { m_Bx = x; m_By = y; }
    int on(int time);
    cLogWork* add(int flag, int errId, const char* mes, va_list ap);
    int scrSet(s8 n);                          // scroll by `n`, clamped to [0, 100 - lines]
    int dispLineNum(int x, int y);
};

extern cLog* pLog;

void LogInit();

// Debug break with source location (used by the header-inline range checks).
extern void dbgAssert(const char* file, int line);

extern "C" void OSReport(const char* fmt, ...);

// Report the source location and stop (a write to an unmapped address). A plain block, not
// do/while(0): the loop notes of a do/while are a scheduling barrier (main_sub.cpp). The units keep
// the original line numbers at the use sites with #line.
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#endif
