#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "mes.h"
#include "t_util.h"
#include <string.h>


// Message debug tool (Tools/t_mes.cpp): shows one message of the current message data set, lets the pad
// move it and edit the message colour table.

// simple vertical text menu: tbl[0] is the header, the list ends with "\\"
class cIdToolMenu {
public:
    char** m_Menu;     // 0x00
    int m_maxMenu;        // 0x04  entries (without the header)
    int m_menuNo;     // 0x08
    int m_maxLen;     // 0x0C
    s16 m_x;          // 0x10
    s16 m_y;          // 0x12
    // 0x14 vptr

    cIdToolMenu(char** tbl, int x, int y);
    virtual ~cIdToolMenu() {}
    int ToolMenuMove(int flag);
    void ToolMenuLocate(int dx, int dy, int clamp);
};

class cMessageDebug {
public:
    u8 r_no_0;              // 0x00
    u8 r_no_1;               // 0x01
    u8 r_no_2;                // 0x02
    u8 r_no_3;                // 0x03
    u8* m_MesAddr;              // 0x04
    cIdToolMenu* m_pMenu;   // 0x08
    int m_language;               // 0x0C
    u32 m_type;             // 0x10
    s16 m_mes;            // 0x14
    s16 m_mesNum;           // 0x16
    s16 m_x;                // 0x18
    s16 m_y;                // 0x1A
    // 0x1C vptr

    cMessageDebug() { r_no_0 = r_no_1 = r_no_2 = r_no_3 = 0; }
    virtual ~cMessageDebug() {}
    void move();
    void init();
    void menu();
    void message();
    void color();
    void locate();
    void type();
    void language();
    void data();
    void quit();
};

static char* fileTbl[] = {
    "-MESSAGE FILES-", "cardmes.mdt", "commumes.mdt", "edmes.mdt",  "helpmes.mdt", "lobbymes.mdt",
    "optmes.mdt",      "shopmes.mdt", "sortiemes.mdt", "submes.mdt", "\\",
};
static char* menuTbl[] = {
    "-MENU-", "MESSAGE", "LOCATE", "COLOR", "TYPE", "LANGUAGE", "LOAD", "QUIT", "\\",
};

// Message tool entry (debug menu 27): runs the cMessageDebug loop with the game task suspended,
// clears Debug_flg[0] bit 31 and ends the task.
void ToolMes()
{
    cMessageDebug dbg;

    dbg.init();
    TaskSuspend(0);
    dbg.move();
    DbgFlagOff(pG, DBG_TEST_MODE);
    TaskSignal(0);
    TaskExit();
}

// Main loop by r_no_0: 0 menu, 1 message, 2 color, 3 locate, 4 type, 5 language, 6 data (load),
// 7 quit (then leave); the sub stick moves the open menu.
void cMessageDebug::move()
{
    int loop = 1;

    do {
        switch (r_no_0) {
        case 0:
            menu();
            break;
        case 1:
            message();
            break;
        case 2:
            color();
            break;
        case 3:
            locate();
            break;
        case 4:
            type();
            break;
        case 5:
            language();
            break;
        case 6:
            data();
            break;
        case 7:
            quit();
            break;
        default:
            loop = 0;
            break;
        }
        if (m_pMenu) {
            m_pMenu->ToolMenuLocate(Joy[0].substickX / 4, Joy[0].substickY / 4, 1);
        }
        TaskSleep(1);
    } while (loop);
}

// English, ROOM message type, message 0 at (30, 360), a 2 MB buffer for loading.
void cMessageDebug::init()
{
    MesData.setLanguage(0);
    m_type = 1;
    m_x = 30;
    m_y = 360;
    m_mes = 0;
    m_mesNum = 0;
    m_pMenu = 0;
    m_MesAddr = new u8[0x200000];
}

// -MENU-: MESSAGE / LOCATE / COLOR / TYPE / LANGUAGE / LOAD / QUIT (B = QUIT) -> r_no_0.
void cMessageDebug::menu()
{
    int ret;

    switch (r_no_1) {
    case 0:
        m_pMenu = new cIdToolMenu(menuTbl, 50, 120);
        r_no_1++;
        break;
    case 1:
        ret = m_pMenu->ToolMenuMove(0);
        if (ret != -1) {
            if (m_pMenu) {
                delete m_pMenu;
                m_pMenu = 0;
            }
            switch (ret) {
            case 0:
                r_no_0 = 7;
                break;
            case 1:
                r_no_0 = 1;
                break;
            case 2:
                r_no_0 = 3;
                break;
            case 3:
                r_no_0 = 2;
                break;
            case 4:
                r_no_0 = 4;
                break;
            case 5:
                r_no_0 = 5;
                break;
            case 6:
                r_no_0 = 6;
                break;
            case 7:
                r_no_0 = 7;
                break;
            }
            r_no_1 = 0;
        }
        break;
    }
}

// MESSAGE: up/down pick the message number of the current set, A shows it at (m_x, m_y) in slot 0,
// B deletes it and returns to the menu.
void cMessageDebug::message()
{
    MessageControl* pm = &cMes;
    int i;

    if (pm->GetMesStatus(0) & 1) {
        pm->Move();
        return;
    }
    if (Joy[0].trg & 0x100) {
        pm->MesSet(m_mes, m_x, m_y, m_type, 0, 0, 4);
        for (i = 0; i < 3; i++) {
            cMes.MesSetJump(0, 0xFFFF);
        }
    } else if (Joy[0].trg & 0x200) {
        pm->Delete(0);
        r_no_0 = 0;
    } else if (Joy[0].rep & 0x80008) {
        m_mes--;
        if (m_mes < 0) {
            m_mes = m_mesNum - 1;
            if (m_mes < 0) {
                m_mes = 0;
            }
        }
    } else if (Joy[0].rep & 0x40004) {
        m_mes++;
        if (m_mes >= m_mesNum) {
            m_mes = 0;
        }
    }
    eprintf(50, 120, 0, 0, "MESSAGE: %d / %d", m_mes, m_mesNum);
    eprintf(50, 136, 0, 0, "SELECT UP/DOWN");
}

static u8 colIdx = 0;
static u8 colCur = 0;

// COLOR: edits the message colour table entry (WHITE / RED / GREEN / BLUE / ORANGE / BLACK) R/G/B/A
// bytes with the d-pad; B back.
void cMessageDebug::color()
{
    const char* names[6] = {"WHITE", "RED", "GREEN", "BLUE", "ORANGE", "BLACK"};
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    JOY* joy = &Joy[0];
    u8* p;

    eprintf(40, 140, colCur == 0 ? 4 : 0, 15, "MODE: %s", names[colIdx]);
    if (joy->trg & 0x200) {
        r_no_0 = 0;
        return;
    }
    if (joy->trg & 0x80008) {
        if (colCur != 0) {
            colCur--;
        }
    } else if (joy->trg & 0x40004) {
        if (colCur != 4) {
            colCur++;
        }
    }
    if (colCur == 0) {
        if (joy->trg & 0x10001) {
            if (colIdx != 0) {
                colIdx--;
            }
        } else if (joy->trg & 0x20002) {
            if (colIdx != 5) {
                colIdx++;
            }
        }
    }
    r = mes_col_tbl[colIdx] >> 24;
    g = (mes_col_tbl[colIdx] >> 16) & 0xFF;
    b = (mes_col_tbl[colIdx] >> 8) & 0xFF;
    a = mes_col_tbl[colIdx] & 0xFF;
    switch (colCur) {
    case 1:
        p = &r;
        break;
    case 2:
        p = &g;
        break;
    case 3:
        p = &b;
        break;
    case 4:
        p = &a;
        break;
    default:
        p = 0;
        break;
    }
    if (p) {
        if (joy->rep & 0x10001) {
            if (*p != 0) {
                (*p)--;
            }
        } else if (joy->rep & 0x20002) {
            if (*p != 0xFF) {
                (*p)++;
            }
        }
    }
    mes_col_tbl[colIdx] = (r << 24) | (g << 16) | (b << 8) | a;
    eprintf(40, 220, colCur == 1 ? 4 : 0, 0, "R: %02x", r);
    eprintf(40, 240, colCur == 2 ? 4 : 0, 0, "G: %02x", g);
    eprintf(40, 260, colCur == 3 ? 4 : 0, 0, "B: %02x", b);
    eprintf(40, 280, colCur == 4 ? 4 : 0, 0, "A: %02x", a);
}

// LOCATE: d-pad moves the message origin (m_x, m_y); B back.
void cMessageDebug::locate()
{
    JOY* joy = &Joy[0];

    m_x += joy->stickX / 4;
    m_y -= joy->stickY / 4;
    if (joy->rep & 8) {
        m_y--;
    } else if (joy->rep & 4) {
        m_y++;
    } else if (joy->rep & 1) {
        m_x--;
    } else if (joy->rep & 2) {
        m_x++;
    } else if (joy->trg & 0x100) {
        m_x = 30;
        m_y = 360;
    } else if (joy->trg & 0x200) {
        r_no_0 = 0;
    }
    eprintf(m_x, m_y, 0, 0, "LOCATE: %d / %d", m_x, m_y);
}

static char* typeTbl[] = {"-TYPE-", " CORE", " ROOM", " FREE", "\\"};

// -TYPE-: CORE / ROOM / FREE picks the message data set (m_type 1 / 2 / 4, count from MesData).
void cMessageDebug::type()
{
    int ret;

    switch (r_no_1) {
    case 0:
        m_pMenu = new cIdToolMenu(typeTbl, 50, 120);
        r_no_1++;
        break;
    case 1:
        ret = m_pMenu->ToolMenuMove(0);
        if (ret != -1) {
            if (m_pMenu) {
                delete m_pMenu;
                m_pMenu = 0;
            }
            switch (ret) {
            case 0:
                break;
            case 1:
                m_type = 1;
                m_mesNum = MesData.getMesNum(0);
                break;
            case 2:
                m_type = 2;
                m_mesNum = MesData.getMesNum(1);
                break;
            case 3:
                m_type = 4;
                m_mesNum = MesData.getMesNum(2);
                break;
            }
            m_mes = 0;
            r_no_0 = 0;
            r_no_1 = 0;
        }
        break;
    }
}

static char* langTbl[] = {"-LANGUAGE-", " ENGLISH", "\\"};
static int langBak = 0;  // unreferenced (.data 0x21E4)

// -LANGUAGE-: ENGLISH sets MesData.lang.
void cMessageDebug::language()
{
    int ret;

    switch (r_no_1) {
    case 0:
        m_pMenu = new cIdToolMenu(langTbl, 50, 120);
        r_no_1++;
        break;
    case 1:
        ret = m_pMenu->ToolMenuMove(0);
        if (ret != -1) {
            if (m_pMenu) {
                delete m_pMenu;
                m_pMenu = 0;
            }
            switch (ret) {
            case 0:
                break;
            case 1:
                MesData.setLanguage(1);
                break;
            }
            r_no_0 = 0;
            r_no_1 = 0;
        }
        break;
    }
}

// LOAD: not implemented (back to the menu).
void cMessageDebug::data()
{
    r_no_0 = 0;
}

// QUIT: ends the loop (r_no_0 past the table).
void cMessageDebug::quit()
{
    delete m_MesAddr;
    r_no_0 = 8;
}

// Menu over the "\\"-terminated string table `t` (t[0] = header) at text position (px, py).
cIdToolMenu::cIdToolMenu(char** t, int px, int py)
{
    int i;

    m_Menu = t;
    m_x = px;
    m_y = py;
    m_menuNo = 0;
    m_maxMenu = 0;
    while (t[m_maxMenu][0] != '\\') {
        m_maxMenu++;
    }
    if (m_maxMenu > 0) {
        m_maxMenu--;
    }
    m_maxLen = 0;
    for (i = 0; i < m_maxMenu; i++) {
        int len = strlen(m_Menu[i]);

        if (m_maxLen < len) {
            m_maxLen = len;
        }
    }
}

// Draws the menu; up/down move the cursor (wrap), A returns the 1-based entry, B jumps to the last
// entry (returns it when already there); else 0.
int cIdToolMenu::ToolMenuMove(int sw)
{
    int ret = -1;
    int i;

    eprintf(m_x, m_y, 4, 0, "%s", m_Menu[0]);
    for (i = 1; i <= m_maxMenu; i++) {
        int col = 0;

        if (m_menuNo == i - 1) {
            col = 5;
        }
        eprintf(m_x, i * 16 + m_y, col, 0, "%s", m_Menu[i]);
    }
    if (Joy[0].trg & 0x100) {
        ret = m_menuNo + 1;
    } else if (Joy[0].trg & 0x200) {
        if (sw) {
            ret = 0;
        } else {
            if (m_menuNo != m_maxMenu - 1) {
                m_menuNo = m_maxMenu - 1;
            } else {
                ret = 0;
            }
        }
    } else if (Joy[0].rep & 0x80008) {
        m_menuNo--;
        if (m_menuNo < 0) {
            m_menuNo = m_maxMenu - 1;
        }
    } else if (Joy[0].rep & 0x40004) {
        m_menuNo++;
        if (m_menuNo > m_maxMenu - 1) {
            m_menuNo = 0;
        }
    }
    return ret;
}

// Moves the menu by (dx, dy) pixels, kept on screen when `clamp`.
void cIdToolMenu::ToolMenuLocate(int x, int y, int flag)
{
    m_x += x;
    m_y -= y;
    if (flag) {
        if (m_x < 16) {
            m_x = 16;
        } else if (m_x > 496) {
            m_x = 496;
        }
        if (m_y < 0) {
            m_y = 0;
        } else if (m_y > 432) {
            m_y = 432;
        }
    }
}
