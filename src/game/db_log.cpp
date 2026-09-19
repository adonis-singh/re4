// game/db_log.cpp: the on-screen debug log (pLog). Every unit reports through pLog->err / warn /
// mes; lines go into a 100 entry ring (also echoed to the console), duplicates of the newest
// line are collapsed, and the window (position / duration / line count from modeSet) shows the
// last lines for m_DispTime frames after a new one (Start + Z re-opens it).

#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "db_log.h"

extern "C" {
void OSReport(const char* fmt, ...);
int printf(const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list ap);
int strcmp(const char* a, const char* b);
char* strncpy(char* dst, const char* src, unsigned int n);
}

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// Debug break with the source location.
#line 20 "D:/Bio4/Prog/db_log.cpp"
static inline void logHalt()
{
    HALT();
}

cLogPtr pLog;

// Allocates the log from the debug heap and initialises it (boot).
void LogInit()
{
    cLog* p = (cLog*) Debug_alloc(sizeof(cLog), 1);
    pLog.p = p;
    p->init();
}

// Default window and an empty ring.
void cLog::init()
{
    modeReset();
    clear();
}

// Adds a plain message line in colour `col` (printf format).
void cLog::mes(int flag, int col, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmes(flag, col, fmt, ap);
    va_end(ap);
}

// Adds an error line (red, 0x16); `errId` is a duplicate key (0 = none).
void cLog::err(int flag, int errId, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verr(flag, errId, fmt, ap);
    va_end(ap);
}

// Adds a warning line (yellow, 0x10).
void cLog::warn(int flag, int errId, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vwarn(flag, errId, fmt, ap);
    va_end(ap);
}

// va_list form of mes (suppressed entirely by Debug_flg[3] 0x04000000).
void cLog::vmes(int flag, int col, const char* fmt, va_list ap)
{
    if (!DbgFlagChk(pG, DBG_LOG_OFF)) {
        cLogWork* w = add(flag, 0, fmt, ap);
        w->m_Col = col;
    }
}

// va_list form of err.
void cLog::verr(int flag, int errId, const char* fmt, va_list ap)
{
    if (!DbgFlagChk(pG, DBG_LOG_OFF)) {
        cLogWork* w = add(flag, errId, fmt, ap);
        w->m_Col = 0x16;
    }
}

// va_list form of warn.
void cLog::vwarn(int flag, int errId, const char* fmt, va_list ap)
{
    if (!DbgFlagChk(pG, DBG_LOG_OFF)) {
        cLogWork* w = add(flag, errId, fmt, ap);
        w->m_Col = 0x10;
    }
}

// Empties the ring and hides the window.
void cLog::clear()
{
    cLogWork* w;
    timer = 0;
    m_BuffIdx = 0;
    for (w = work; w < &work[100]; w++) {
        w->clear();
    }
}

// The default window: x 160, y 378, 90 frames, 5 lines, no scroll.
int cLog::modeReset()
{
    modeSet(160, 378, 90, 5);
    timer = 0;
    m_ScrOfs = 0;
    return 1;
}

// Window position, display duration (frames, 0xFF = always) and visible line count.
int cLog::modeSet(int x, int y, int time, int lines)
{
    cLog* l = pLog.p;
    l->m_Bx = x;
    l->m_By = y;
    this->m_DispTime = time;
    this->m_DispNum = lines;
    return 1;
}

// Draws the window while its timer runs: the last m_DispNum lines (scrolled by m_ScrOfs) and a
// blinking `+` / `*` marker when a line was added this frame, `>` when it was a duplicate.
void cLog::disp()
{
    static int disp_rep_cnt = 0;
    int i;
    int idx;
    s16 xx;
    s16 yy;

    if ((Joy[0].on & JOY_START) && (Joy[0].trg & JOY_Z)) {
        timer = m_DispTime;
    }
    if (timer == 0) {
        return;
    }
    xx = m_Bx;
    yy = m_By;
    idx = (m_BuffIdx + 100 - m_DispNum - m_ScrOfs + 1) % 100;
    for (i = 0; i < m_DispNum; i++) {
        work[idx].print(xx, yy);
        yy += 14;
        idx = (idx + 1) % 100;
    }
    if (m_Flag & 2) {
        disp_rep_cnt++;
        if (disp_rep_cnt & 1) {
            eprintf(m_Bx + m_RepeatCtr - 16, m_By + (m_DispNum - 1) * 14, 0, 0, "+");
        } else {
            eprintf(m_Bx + m_RepeatCtr - 16, m_By + (m_DispNum - 1) * 14, 0, 0, "*");
        }
    } else if (m_Flag & 1) {
        eprintf(m_Bx + m_RepeatCtr - 16, m_By + (m_DispNum - 1) * 14, 0, 0, ">");
        m_Flag &= ~1;
        m_RepeatCtr = (m_RepeatCtr + 1) % 8;
    }
    if (timer != 0xFF) {
        timer--;
    }
    m_Flag &= ~2;
}

// Debug tool: prints the ring index of each visible line (t_log).
int cLog::dispLineNum(int x, int y)
{
    int i;
    for (i = 0; i < m_DispNum; i++) {
        eprintf(x, y, 0, 0, "%02d", i - (m_DispNum - 100) - m_ScrOfs);
        y = (s16) (y + 14);
    }
    return 1;
}

// Shows the window for `flag` frames (0xFF = until cleared).
int cLog::on(int flag)
{
    timer = flag;
    return 1;
}

// Scrolls the window by `n` lines, clamped to the ring.
int cLog::scrSet(s8 n)
{
    int s = m_ScrOfs + n;
    if (s < 0) {
        s = 0;
    }
    if (s > 100 - m_DispNum) {
        s = 100 - m_DispNum;
    }
    m_ScrOfs = s;
    return 1;
}

// Formats the line; when it repeats the newest entry (same text or same non-zero key) only the
// duplicate marker is raised, else it is appended to the ring (64 chars) and printed to the
// console (unless LOG_NO_PRINTF). Restarts the display timer unless LOG_NO_SHOW (LOG_DUP_QUIET
// for duplicates). Returns the entry so the caller can set its colour.
cLogWork* cLog::add(int flag, int key, const char* fmt, va_list ap)
{
    char buf[256];
    int dup = 0;
    cLogWork* w = &work[m_BuffIdx];

    vsprintf(buf, fmt, ap);
    m_Flag |= 2;
    if (key != 0) {
        if (strcmp(w->m_Str, buf) == 0 || w->key == key) {
            dup = 1;
        }
    }
    if (dup) {
        m_Flag |= 1;
        if (!(flag & LOG_DUP_QUIET)) {
            if (!(flag & LOG_NO_SHOW)) {
                timer = m_DispTime;
            }
        }
    } else {
        m_BuffIdx = (m_BuffIdx + 1) % 100;
        w = &work[m_BuffIdx];
        strncpy(w->m_Str, buf, 63);
        w->m_Str[63] = 0;
        if (!(flag & LOG_NO_PRINTF)) {
            printf("%s\n", buf);
        }
        w->key = key;
        if (!(flag & LOG_NO_SHOW)) {
            timer = m_DispTime;
        }
    }
    return w;
}

// Draws the line with eprintf in its colour.
void cLogWork::print(int x, int y)
{
    eprintf(x, y, m_Col, 0, m_Str);
}

// Empties the line.
void cLogWork::clear()
{
    m_Col = 0;
    key = 0;
    m_Str[0] = 0;
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
