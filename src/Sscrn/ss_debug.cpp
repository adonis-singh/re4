// Sscrn/ss_debug: sub screen debug menu (pad 3) and the attache case debug editor
// (D:/Bio4/Prog/ss_debug.cpp). SscrnDebugMenu is run every frame by SubScreenTask (START on pad 3
// opens it: debug disp / memory disp / debug page / reveil toggles in SUB_SCREEN::debug_menu);
// ssDbgPzzl is the "CASE MAKE" editor SsPzzlMain opens with Z on pad 1 (item set presets, infinite
// ammo, case size, pesetas, case model angle, boss bar side). cManager<T>::dispWorkNum prints a
// manager's occupancy.
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "item.h"
#include "cockpit.h"
#include "puzzle.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "model.h"
#include "sscrn.h"
#include "ss_main.h"

// Attache case editor (ss_pzzl's SsPzzlMain owns one).
class ssDbgPzzl {
public:
    void* m_p_save_bak;    // 0x00  ItemMgr save image taken at init (restored by item set 0x17)
    u8 m_size_bak;    // 0x04  wk->x2AE at init
    s8 m_menu_no;      // 0x05  menu row
    s8 m_set_no;     // 0x06  cItemMgr::setUp number, 0x17 = the saved inventory
    s8 m_bllt_no;      // 0x07  0 normal, 1 infinite, 2 infinite + no reload

    void init(SUB_SCREEN* wk);
    void quit();
    void move(SUB_SCREEN* wk);
};

// ss_pzzl.cpp debug offsets edited by the "Case Rot" row
extern int pzzlDbgNo;
extern f32 pzzlDbgPos;

static int dbg_cursor = 0;
static u8 dbg_blink = 0;
static int dbg_x = 42;
static int dbg_y = 5;

// Sub screen debug menu on pad 3 (Joy[2]), run every frame by SubScreenTask: START toggles it
// (debug_menu bit 0), B closes, up/down pick a row, left/right set it. Rows: DEBUG DISP
// (debug_menu 0x10), MEMORY DISP (0x20 -> Debug_flg[2] bit 30), DEBUG PAGE (pG->debug_mode 0..0x18),
// REVEIL (0x40). Also prints the DLL parts/model-info manager occupancy.
void SscrnDebugMenu(SUB_SCREEN* wk)
{
    const char* menu[4] = {"DEBUG DISP :", "MEMORY DISP:", "DEBUG PAGE :", "REVEIL     :"};
    JOY* joy = &Joy[2];
    int cx;
    int i;
    int y;

    if (joy->trg & 0x1000) {
        if (wk->debug_menu & 1) {
            wk->debug_menu &= ~1;
            return;
        }
        wk->debug_menu |= 1;
        dbg_cursor = 0;
    }
    if (wk->debug_menu & 1) {
        if (joy->trg & 0x200) {
            wk->debug_menu &= ~1;
            return;
        }
        if (joy->trg & 0x80008) {
            dbg_cursor--;
        }
        if (joy->trg & 0x40004) {
            dbg_cursor++;
        }
        dbg_cursor = dbg_cursor < 0 ? 0 : (dbg_cursor > 3 ? 3 : dbg_cursor);
        switch (dbg_cursor) {
        case 0:
            if (joy->trg & 0x10001) {
                wk->debug_menu |= 0x10;
            }
            if (joy->trg & 0x20002) {
                wk->debug_menu &= ~0x10;
            }
            break;
        case 1:
            if (joy->trg & 0x10001) {
                wk->debug_menu |= 0x20;
            }
            if (joy->trg & 0x20002) {
                wk->debug_menu &= ~0x20;
            }
            break;
        case 2:
            if (joy->rep & 0x10001) {
                pG->debug_mode--;
            }
            if (joy->rep & 0x20002) {
                pG->debug_mode++;
            }
            pG->debug_mode = pG->debug_mode < 0 ? 0x18 : (pG->debug_mode > 0x18 ? 0 : pG->debug_mode);
            break;
        case 3:
            if (joy->trg & 0x10001) {
                wk->debug_menu |= 0x40;
            }
            if (joy->trg & 0x20002) {
                wk->debug_menu &= ~0x40;
            }
            break;
        }
        eprintf(0x50, 0x8C, 5, 0, "----- SUBSCRN DEBUG -----");
        cx = 10;
        for (i = 0; i < 4; i++) {
            y = 0x9A + i * 0xE;
            eprintf(cx * 8, y, i == dbg_cursor ? 4 : 0, 0, "%s", menu[i]);
            switch (i) {
            case 0:
                if (wk->debug_menu & 0x10) {
                    eprintf((cx + 13) * 8, y, 0, 0, "ON-/---");
                } else {
                    eprintf((cx + 13) * 8, y, 0, 0, "---/OFF");
                }
                break;
            case 1:
                if (wk->debug_menu & 0x20) {
                    eprintf((cx + 13) * 8, y, 0, 0, "ON-/---");
                } else {
                    eprintf((cx + 13) * 8, y, 0, 0, "---/OFF");
                }
                break;
            case 2:
                eprintf((cx + 13) * 8, y, 0, 0, "%02d", pG->debug_mode);
                break;
            case 3:
                if (wk->debug_menu & 0x40) {
                    eprintf((cx + 13) * 8, y, 0, 0, "ON-/---");
                } else {
                    eprintf((cx + 13) * 8, y, 0, 0, "---/OFF");
                }
                break;
            }
        }
        eprintf(0x180, 0x62, 0, 0, "PRT");
        ssPartsMgr.dispWorkNum(0x1A0, 0x62, 0, 0);
        eprintf(0x180, 0x70, 0, 0, "MI");
        ssModInfoMgr.dispWorkNum(0x1A0, 0x70, 0, 0);
    }
    if (wk->debug_menu & 0x20) {
        DbgFlagOn(pG, DBG_PROC_BAR);
    } else {
        DbgFlagOff(pG, DBG_PROC_BAR);
    }
}

// "CASE MAKE" editor open: saves the whole inventory (ItemMgr.save) so item set 0x17 can restore
// it, remembers the case size; cursor on the item set row.
void ssDbgPzzl::init(SUB_SCREEN* wk)
{
    m_menu_no = 0;
    m_set_no = 0x17;
#line 179 "D:/Bio4/Prog/ss_debug.cpp"
    m_p_save_bak = MEM_ALLOC(ItemMgr.saveDataSize(), 1, 13);
    if (m_p_save_bak) {
        ItemMgr.save(m_p_save_bak);
        m_size_bak = wk->board_size;
    }
}

// Frees the inventory backup.
void ssDbgPzzl::quit()
{
    if (m_p_save_bak) {
        Mem_free(m_p_save_bak);
    }
}

// "CASE MAKE" attache case editor (ss_pzzl debug menu, pad 1): rows Item Set (cItemMgr::setUp
// presets 0..0x16, 0x17 = saved inventory), Bullet (Debug_flg infinite ammo / no reload), Case Size
// (gives case item 0x7C..0x7F -> board_next), Peseta (+-1000, x10 with A), Case Rot (case model
// angle and the ss_pzzl pzzlDbgNo/pzzlDbgPos offsets) and Boss Bar (g_boss_bar_flag side).
// Left/right edit the row; the case is rebuilt by the caller when `changed`.
void ssDbgPzzl::move(SUB_SCREEN* wk)
{
    JOY* joy = &Joy[0];
    int changed = 0;
    int i;
    int x;
    int y;

    if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
        m_bllt_no = 2;
    } else if (DbgFlagChk(pG, DBG_INF_BULLET)) {
        m_bllt_no = 1;
    } else {
        m_bllt_no = 0;
    }
    dbg_blink++;
    if (joy->trg & 0x80008) {
        m_menu_no--;
    }
    if (joy->trg & 0x40004) {
        m_menu_no++;
    }
    if (joy->trg & 0xC000C) {
        dbg_blink = 0x18;
    }
    m_menu_no = m_menu_no < 0 ? 0 : (m_menu_no > 5 ? 5 : m_menu_no);
    switch (m_menu_no) {
    case 0: {
        s8 old = m_set_no;
        if (joy->trg & 0x10001) {
            m_set_no--;
        }
        if (joy->trg & 0x20002) {
            m_set_no++;
        }
        m_set_no = m_set_no < 0 ? 0x17 : (m_set_no > 0x17 ? 0 : m_set_no);
        if (old != m_set_no) {
            ItemMgr.dumpType(1);
            ItemMgr.dumpType(2);
            ItemMgr.dumpType(3);
            ItemMgr.dumpType(4);
            ItemMgr.dumpType(6);
            ItemMgr.dumpType(9);
            ItemMgr.dumpType(0xE);
            if (m_set_no == 0x17) {
                if (m_p_save_bak) {
                    ItemMgr.load(m_p_save_bak);
                    wk->board_next = m_size_bak;
                } else {
                    m_set_no = 0;
                    wk->board_next = ItemMgr.setUp(m_set_no);
                }
            } else {
                wk->board_next = ItemMgr.setUp(m_set_no);
            }
            changed = 1;
        }
        break;
    }
    case 1:
        if (joy->trg & 0x10001) {
            m_bllt_no--;
        }
        if (joy->trg & 0x20002) {
            m_bllt_no++;
        }
        m_bllt_no = m_bllt_no < 0 ? 2 : (m_bllt_no > 2 ? 0 : m_bllt_no);
        DbgFlagOff(pG, DBG_INF_BULLET);
        DbgFlagOff(pG, DBG_INF_BULLET2);
        switch (m_bllt_no) {
        case 2:
            DbgFlagOn(pG, DBG_INF_BULLET2);
            break;
        case 1:
            DbgFlagOn(pG, DBG_INF_BULLET);
            break;
        }
        break;
    case 2: {
        s8 old = wk->board_next;
        if (joy->trg & 0x10001) {
            wk->board_next--;
        }
        if (joy->trg & 0x20002) {
            wk->board_next++;
        }
        wk->board_next = (s8) wk->board_next < 0 ? 3 : ((s8) wk->board_next > 3 ? 0 : wk->board_next);
        if (old != (s8) wk->board_next) {
            ItemMgr.dump(0x7C);
            ItemMgr.dump(0x7D);
            ItemMgr.dump(0x7E);
            ItemMgr.dump(0x7F);
            switch ((s8) wk->board_next) {
            case 0:
                ItemMgr.get(0x7C, 0);
                break;
            case 1:
                ItemMgr.get(0x7D, 0);
                break;
            case 2:
                ItemMgr.get(0x7E, 0);
                break;
            case 3:
                ItemMgr.get(0x7F, 0);
                break;
            }
            changed = 1;
        }
        break;
    }
    case 3: {
        int step = 1000;
        if (joy->on & 0x100) {
            step = 10000;
        }
        if (joy->rep2 & 0x10001) {
            pG->peseta -= step;
        }
        if (joy->rep2 & 0x20002) {
            pG->peseta += step;
        }
        {
            GlobalWork* g = pG;
            int p = g->peseta;
            if (p >= 0) {
                if (p > 100000000) {
                    p = 100000000;
                }
            } else {
                p = 0;
            }
            g->peseta = p;
        }
        break;
    }
    case 4: {
        cMap* m = MapMgr.getWork(3);
        f32 step = 0.01f;
        f32 step2 = 10.0f;
        int step3 = 1;
        if (joy->on & 0x100) {
            step *= 10.0f;
            step3 = 10;
            step2 *= 10.0f;
        }
        if (joy->rep2 & 0x20) {
            m->ang.x -= step;
        }
        if (joy->rep2 & 0x40) {
            m->ang.x += step;
        }
        m->ang.x = m->ang.x < -1.5707964f ? 1.5707964f : (m->ang.x > 1.5707964f ? -1.5707964f : m->ang.x);
        if (joy->rep2 & 0x00800000) {
            pzzlDbgNo += step3;
        }
        if (joy->rep2 & 0x00400000) {
            pzzlDbgNo -= step3;
        }
        if (joy->rep2 & 0x00100000) {
            pzzlDbgPos -= step2;
        }
        if (joy->rep2 & 0x00200000) {
            pzzlDbgPos += step2;
        }
        break;
    }
    case 5:
        if (joy->rep2 & 0x10001) {
            g_boss_bar_flag--;
        }
        if (joy->rep2 & 0x20002) {
            g_boss_bar_flag++;
        }
        g_boss_bar_flag = (u32) g_boss_bar_flag > 3 ? 0 : g_boss_bar_flag;
        break;
    }
    if (changed) {
        wk->board_size = wk->board_next;
        wk->puzzlePlayer->quit();
        delete wk->puzzlePlayer;
        wk->puzzlePlayer = new pzlPlayer;
        if (wk->puzzlePlayer->init((s8) wk->board_size) == 0) {
            delete wk->puzzlePlayer;
        }
        pieceModelInit(wk);
        wk->puzzlePlayer->save();
    }
    {
        const char* items[6] = {"Item Set :", "Bullet   :", "Case Size:", "Peseta   :", "Case Rot :", "Boss Bar :"};
        x = dbg_x;
        y = dbg_y;
        eprintf(x * 8, y * 14, 5, 0, "----- CASE MAKE -----");
        y++;
        if (dbg_blink & 0x18) {
            eprintf((x - 1) * 8, (y + m_menu_no) * 14, 0x16, 0, ">");
        }
        for (i = 0; i < 6; i++) {
            int col = i == m_menu_no ? 4 : 0;
            eprintf(x * 8, (y + i) * 14, col, 0, "%s", items[i]);
            switch (i) {
            case 0: {
                const char* tbl[24] = {
                    "GAME INIT", "TRIAL VER", "STAGE 1-1", "STAGE 1-2", "STAGE 2-1", "STAGE 2-2",
                    "STAGE 3-1", "STAGE 3-2", "SHOOTING1", "SHOOTING2", "INIT+CURE", "STAGE 1DX",
                    "STAGE 2DX", "HANDGUN  ", "XXXXGUN",   "MINE RPG",  "NETA 1",    "NETA 2",
                    "NETA 3",    "NETA 4",    "NETA 5",    "KLAUSER",   "WESKER",    "---------",
                };
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[m_set_no]);
                break;
            }
            case 1: {
                const char* tbl[3] = {"-----", "INF", "INF+RELOAD"};
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[m_bllt_no]);
                break;
            }
            case 2: {
                const char* tbl[4] = {"SMALL", "MEDIUM", "LARGE", "HUGE"};
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[(s8) wk->board_next]);
                break;
            }
            case 3: {
                int c = i == m_menu_no ? 0x16 : 0;
                eprintf((x + 11) * 8, (y + i) * 14, c, 0, "%d", pG->peseta);
                break;
            }
            case 4: {
                cMap* m = MapMgr.getWork(3);
                eprintf((x + 11) * 8, (y + i) * 14, col, 0, "%.3f", m->ang.x);
                break;
            }
            case 5: {
                const char* tbl[4] = {"UP", "DOWN", "LEFT", "RIGHT"};
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[g_boss_bar_flag]);
                break;
            }
            }
        }
    }
}

template <class T>
// Debug print "alive/maxAlive/total" of a manager array at screen (x, y), less `sub` reserved works;
// returns the alive count (be_flag 0x601). Skipped when the array is not in MRAM.
int cManager<T>::dispWorkNum(int x, int y, int col, int sub)
{
    u32 n;
    u32 i;

    if ((u32) pArray < 0x80000000 || (u32) pArray > 0x82FFFFFF) {
        return 0;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        T* p = (T*) ((u8*) pArray + size * i);
        if (p->be_flag & 0x601) {
            n++;
        }
    }
    eprintf(x, y, 0, col, "%3d/%3d/%4d", n - sub, maxAlive - sub, nArray - sub);
    return n;
}
