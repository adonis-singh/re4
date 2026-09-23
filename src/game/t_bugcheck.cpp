// game/t_bugcheck: the debug "bug check" cheat menu (a debug-menu task): infinite ammo, player
// speed, no death for the player / enemies, free position move, life editing, the collision and
// event-area displays, shop unlock, sound stops and model / enemy display toggles — all through
// the Debug_flg / Disp_flg words.
#include "types.h"
#include "vec.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "model.h"
#include "player.h"
#include "t_util.h"
#include <string.h>
#include "item.h"
#include "dbmodule.h"
#include "pl_npc.h"




// Bug-check (cheat) menu tool.
class cToolBugcheck {
public:
    u32 m_stop_flag_bak;  // 0x00  saved pG stop flags
    u16 m_base_dx;         // 0x04  menu position
    u16 m_base_dy;         // 0x06
    u16 m_dx;        // 0x08  position used for this frame's drawing
    u16 m_dy;        // 0x0A
    s8 cursor;     // 0x0C
    u8 pad_D[3];

    void init();
    void main();
    void exit();
    static void menuPosMove();
    static void menuLife();
    void menu();
};

cToolBugcheck BC;

static inline int RoundToInt(f32 x) { return (int) (x + 0.5f); }

// Debug menu entry: runs the bug-check menu until B.
void ToolBugcheck()
{
    BC.main();
}

// Freezes the game (Stop_flg saved, all but 0x4000 set), menu at (80, 60).
void cToolBugcheck::init()
{
    m_stop_flag_bak = pG->Stop_flg;
    BitOn(pG->Stop_flg, ~0x4000);
    m_base_dx = 80;
    m_base_dy = 60;
    cursor = 0;
}

// Menu loop: the C-stick moves the menu, B leaves.
void cToolBugcheck::main()
{
    init();
    while (!(Joy[0].trg & JOY_B)) {
        if (Joy[0].on & 0x200000) {
            m_base_dx += 8;
        }
        if (Joy[0].on & 0x100000) {
            m_base_dx -= 8;
        }
        if (Joy[0].on & 0x400000) {
            m_base_dy += 8;
        }
        if (Joy[0].on & 0x800000) {
            m_base_dy -= 8;
        }
        m_dx = m_base_dx;
        m_dy = m_base_dy;
        menu();
        TaskSleep(1);
    }
    exit();
}

// Restores Stop_flg, clears the debug-menu-active bit, ends the task.
void cToolBugcheck::exit()
{
    DbgFlagOff(pG, DBG_TEST_MODE);
    pG->Stop_flg = m_stop_flag_bak;
    TaskExit();
}

// Sub menu: moves the player freely with the stick (A fast, L / R up / down, X ignores the scroll
// collision, Z brings Ashley along), snapping to the floor unless flying; prints the position.
void cToolBugcheck::menuPosMove()
{
    f32 speed;
    f32 floor;

    pG->Stop_flg &= ~0x10000000;
    pG->Stop_flg &= ~0x40000000;
    while (1) {
        if (Joy[0].on & JOY_X) {
            DbgFlagOn(pG, DBG_PL_NOHIT);
        } else {
            DbgFlagOff(pG, DBG_PL_NOHIT);
        }
        speed = (Joy[0].on & JOY_A) ? 8.0f : 2.0f;
        Vec v = {0.0f, 0.0f, 0.0f};
        JOY joy = Joy[0];
        joy.stickX = 0;
        CamStick2World(&pG->Camera, &joy, &v);
        PSVECScale(&v, &v, speed);
        v.y = 0.0f;
        if (Joy[0].on & 0x10000) {
            pPL->ang.y += 0.13962634f;
        }
        if (Joy[0].on & 0x20000) {
            pPL->ang.y -= 0.13962634f;
        }
        floor = SatMgr.getFloor(&pPL->pos, 0, 600.0f, 100000.0f, 0);
        if (pPL->pos.y > floor + 500.0f || DbgFlagChk(pG, DBG_PL_NOHIT)) {
            v.y = v.y + speed * (f32) (int) Joy[0].triggerRight - speed * (f32) (int) Joy[0].triggerLeft;
        }
        PSVECAdd(&pPL->pos, &v, &pPL->pos);
        pPL->setPosAng(&pPL->pos, &pPL->ang);
        {
            // COMPILER-DIFF: candidate (gcse hash bucket of the .LC label name): one more constant
            // pool label before "X:%.0f" gives the target's hoisted-high pseudo order. PS2's
            // cPlWaist::reset() (a 0.0f store, player.h) would add exactly this label, but a real
            // body there moves the body-count windows of esp and db_light.
            f32 lc0 = 1.0f;
        }
        eprintf(32, 56, 0, 0, "X:%.0f", pPL->pos.x);
        eprintf(32, 70, 0, 0, "Y:%.0f", pPL->pos.y);
        eprintf(32, 84, 0, 0, "Z:%.0f", pPL->pos.z);
        eprintf(32, 98, 0, 0, "R:%.2f", pPL->ang.y);
        eprintf(32, 126, DbgFlagChk(pG, DBG_PL_NOHIT) ? 0 : 0x14, 0, "[X]:SCR_NO_HIT");
        eprintf(32, 140, 0, 0, "[A]:SPPED_UP");
        eprintf(32, 154, 0, 0, "[L]:POS_UP");
        eprintf(32, 168, 0, 0, "[R]:POS_DOWN");
        eprintf(32, 182, 0, 0, "[Z]:CALL_ASHLEY");
        Draw_pos(&pPL->pos, 2000);
        if ((Joy[0].on & JOY_Z) && pSUB != NULL && (pSUB->be_flag & 0x201) == 1) {
            pSUB->setPos(&pPL->pos);
        }
        if (Joy[0].trg & JOY_B) {
            break;
        }
        TaskSleep(1);
    }
    DbgFlagOff(pG, DBG_PL_NOHIT);
    pG->Stop_flg |= 0x10000000;
    pG->Stop_flg |= 0x40000000;
}

// Sub menu: edits the player's / Ashley's life and life maximum (stick / L / R), shows the level.
void cToolBugcheck::menuLife()
{
    int cur = 0;
    int n;
    int lv;

    {
        // COMPILER-DIFF: candidate (gcse hash bucket of the .LC label name): the nine hoisted string
        // highs are PRE reaching regs numbered in expr-hash bucket order of "*.LCn" ((7933 + h) % 183
        // buckets, h = h*129 + c); global-alloc ties (equal priority) go by that number. Thirteen dead
        // labels put the strings at .LC42-.LC50: the decade step 49 -> 50 wraps ">" (bucket 48) below
        // ASHLEY..L-TRIG (106-111), so ">" is allocated first at the shared priority 46 (r24) and the
        // six follow in eprintf order (r23-r18); LIFE/PLAYER (.LC42/43) sit at 45 (see the loop end).
        f32 lc1 = 1.0f;
        f32 lc2 = 2.0f;
        f32 lc3 = 3.0f;
        f32 lc4 = 4.0f;
        f32 lc5 = 5.0f;
        f32 lc6 = 6.0f;
        f32 lc7 = 7.0f;
        f32 lc8 = 8.0f;
        f32 lc9 = 9.0f;
        f32 lc10 = 10.0f;
        f32 lc11 = 11.0f;
        f32 lc12 = 12.0f;
        f32 lc13 = 13.0f;
    }
    while (1) {
        if (Joy[0].trg & JOY_UP) {
            cur--;
        }
        if (Joy[0].trg & JOY_DOWN) {
            cur++;
        }
        if (cur >= 0) {
            n = cur;
            if (n > 2) {
                n = 2;
            }
        } else {
            n = 0;
        }
        cur = n;
        switch (n) {
        case 0:
            pG->pl_life = pG->pl_life + (s16) (Joy[0].stickX * 0.4f);
            if (Joy[0].on & JOY_RIGHT) {
                pG->pl_life += 25;
            }
            if (Joy[0].on & JOY_LEFT) {
                pG->pl_life -= 25;
            }
            pG->pl_life = (s16) pG->pl_life < 0 ? 0 : ((s16) pG->pl_life > (s16) pG->pl_life_max ? pG->pl_life_max : pG->pl_life);
            if (Joy[0].rep2 & (JOY_R | JOY_L)) {
                lv = lifeLevel(20, pG->pl_life_max, 1200);
                if (Joy[0].rep2 & JOY_R) {
                    lv++;
                }
                if (Joy[0].rep2 & JOY_L) {
                    lv--;
                }
                if (lv >= 0) {
                    if (lv > 20) {
                        lv = 20;
                    }
                } else {
                    lv = 0;
                }
                pG->pl_life_max = 1200;
                pG->pl_life_max = pG->pl_life_max + RoundToInt((f32) (lv * 60));
                pG->pl_life = pG->pl_life_max;
            }
            break;
        case 1:
            pG->ashley_life = pG->ashley_life + (s16) (Joy[0].stickX * 0.4f);
            if (Joy[0].on & JOY_RIGHT) {
                pG->ashley_life += 25;
            }
            if (Joy[0].on & JOY_LEFT) {
                pG->ashley_life -= 25;
            }
            pG->ashley_life = (s16) pG->ashley_life < 0 ? 0 : ((s16) pG->ashley_life > (s16) pG->ashley_life_max ? pG->ashley_life_max : pG->ashley_life);
            if (Joy[0].rep2 & (JOY_R | JOY_L)) {
                lv = lifeLevel(5, pG->ashley_life_max, 600);
                if (Joy[0].rep2 & JOY_R) {
                    lv++;
                }
                if (Joy[0].rep2 & JOY_L) {
                    lv--;
                }
                if (lv >= 0) {
                    if (lv > 5) {
                        lv = 5;
                    }
                } else {
                    lv = 0;
                }
                pG->ashley_life_max = 600;
                pG->ashley_life_max = pG->ashley_life_max + RoundToInt((f32) (lv * 120));
                pG->ashley_life = pG->ashley_life_max;
            }
            break;
        case 2:
            if (Joy[0].trg & JOY_A) {
                pG->pl_life = pG->pl_life_max;
                pG->ashley_life = pG->ashley_life_max;
            }
            break;
        }
        eprintf(48, 56, 4, 0, "LIFE");
        lv = lifeLevel(20, pG->pl_life_max, 1200);
        eprintf(48, 70, 0, 0, "PLAYER:%4d/%4d[%2d]", (s16) pG->pl_life, (s16) pG->pl_life_max, lv);
        DrawGage(216, 70, 8, 100, (s16) pG->pl_life, (s16) pG->pl_life_max, -1);
        lv = lifeLevel(5, pG->ashley_life_max, 600);
        eprintf(48, 84, 0, 0, "ASHLEY:%4d/%4d[%2d]", (s16) pG->ashley_life, (s16) pG->ashley_life_max, lv);
        DrawGage(216, 84, 8, 100, (s16) pG->ashley_life, (s16) pG->ashley_life_max, -1);
        eprintf(48, 98, 0, 0, "LIFE MAX");
        eprintf(64, 126, 6, 0, "STICK-R or JOY-R life up");
        eprintf(64, 140, 6, 0, "STICK-L or JOY_L life down");
        eprintf(64, 154, 6, 0, "R-TRIGGER life max up");
        eprintf(64, 168, 6, 0, "L-TRIGGER life max down");
        eprintf(40, (n + 5) * 14, 0, 0, ">");
        if (Joy[0].trg & JOY_B) {
            break;
        }
        TaskSleep(1);
        // COMPILER-DIFF: tie (global-alloc live length): two more insns in the loop at global-alloc
        // time (+4 REG_LIVE_LENGTH for every loop-invariant pseudo). The nine string highs are born
        // in eprintf order in the preheader (2 per insn): LIFE 656 and PLAYER 654 drop to priority
        // int(30000/len) = 45 (allocated last, in bucket order: LIFE r17, PLAYER r16) while
        // ASHLEY..L-TRIG (652-642) and ">" (640) share 46. The second anchor must not be the same
        // asm again (cse deletes the repeated store as redundant).
        asm("" : "=m"(PlKaiou));
        asm("" : "+m"(PlKaiou));
    }
}

// The 18-entry menu: toggles for WEP_MUGEN (infinite ammo / +reload), PL_SPEED (kaiouken x2..x5),
// PL / EM no death, position move, life, the five collision / event area displays, shop unlock,
// BGM / SE stop, object / scroll hide, quick enemy death and enemy life display; each writes its
// Debug_flg / Disp_flg bit.
void cToolBugcheck::menu()
{
    static TOOL_MENU menu[18] = {
        {1, "WEP_MUGEN", NULL},
        {1, "PL_SPEED", NULL},
        {1, "PL_NO_DEATH", NULL},
        {1, "EM_NO_DEATH", NULL},
        {1, "POS_MOVE", menuPosMove},
        {1, "LIFE", menuLife},
        {1, "OBJ_OBJ_AT_DISP", NULL},
        {1, "OBJ_SCR_AT_DISP", NULL},
        {1, "SCROLL_AT_DISP", NULL},
        {1, "EFFECT_AT_DISP", NULL},
        {1, "EVENT_AT_DISP", NULL},
        {1, "SHOP_ALL", NULL},
        {1, "BGM_STOP", NULL},
        {1, "SE_STOP", NULL},
        {1, "OBJ_NO_DISP", NULL},
        {1, "SCR_NO_DISP", NULL},
        {1, "EM_QUICK_DEAD", NULL},
        {1, "EM_LIFE_DISP", NULL},
    };
    static const char* wep_mugen_str[3] = {"OFF", "MUGEN", "MUGEN+RELOAD"};
    static const char* pl_speed_str[5] = {"OFF", "x2", "x3", "x4", "x5"};
    s16 px = m_dx;
    s16 py = m_dy;
    int i;

    ToolMenuDisp_cur(px, py, 0, &cursor, menu, sizeof(menu), Joy);
    switch (cursor) {
    case 0:
        if (DbgFlagChk(pG, DBG_INF_BULLET)) {
            i = 1;
        } else if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
            i = 2;
        } else {
            i = 0;
        }
        if (Joy[0].rep & 0x20002) {
            i++;
        }
        if (Joy[0].rep & 0x10001) {
            i--;
        }
        if (Joy[0].rep & JOY_A) {
            i++;
        }
        i = i < 0 ? 2 : (i > 2 ? 0 : i);
        DbgFlagOff(pG, DBG_INF_BULLET);
        DbgFlagOff(pG, DBG_INF_BULLET2);
        switch (i) {
        case 0:
            break;
        case 1:
            DbgFlagOn(pG, DBG_INF_BULLET);
            break;
        case 2:
            DbgFlagOn(pG, DBG_INF_BULLET2);
            break;
        }
        break;
    case 1:
        if (DbgFlagChk(pG, DBG_KAIOUKEN)) {
            i = PlKaiou + 1;
        } else {
            i = 0;
        }
        if (Joy[0].rep & 0x20002) {
            i++;
        }
        if (Joy[0].rep & 0x10001) {
            i--;
        }
        if (Joy[0].rep & JOY_A) {
            i++;
        }
        i = i < 0 ? 4 : (i > 4 ? 0 : i);
        if (i == 0) {
            DbgFlagOff(pG, DBG_KAIOUKEN);
        } else {
            DbgFlagOn(pG, DBG_KAIOUKEN);
            PlKaiou = i - 1;
        }
        break;
    case 2:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_NO_DEATH);
        }
        break;
    case 3:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_EM_NO_DEATH);
        }
        break;
    case 4:
    case 5:
        break;
    case 6:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_OBA_VIEW);
        }
        break;
    case 7:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_SCA_VIEW);
        }
        break;
    case 8:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_SAT_DISP);
        }
        break;
    case 9:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_EAT_DISP);
        }
        break;
    case 10:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_SCE_AT_DISP);
        }
        break;
    case 11:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_SHOP_FULL);
        }
        break;
    case 12:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_BGM_STOP);
        }
        break;
    case 13:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_SE_STOP);
        }
        break;
    case 14:
        if (Joy[0].trg & 0x30103) {
            FlagXor(&pG->Disp_flg, 1);
            if (pG->Disp_flg & 0x40000000) {
                pG->Disp_flg |= 0x80000000;
                pG->Disp_flg |= 0x10000000;
                pG->Stop_flg |= 0x20000000;
                pG->Stop_flg |= 0x4000000;
            } else {
                pG->Disp_flg &= ~0x80000000;
                pG->Disp_flg &= ~0x10000000;
                pG->Stop_flg &= ~0x20000000;
                pG->Stop_flg &= ~0x4000000;
            }
        }
        break;
    case 15:
        if (Joy[0].trg & 0x30103) {
            pG->Disp_flg ^= 0x8000000;
        }
        break;
    case 16:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_EM_WEAK);
        }
        break;
    case 17:
        if (Joy[0].trg & 0x30103) {
            DbgFlagXor(pG, DBG_EM_LIFE_DISP);
        }
        break;
    }

    px += 128;
    if (DbgFlagChk(pG, DBG_INF_BULLET)) {
        i = 1;
    } else if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
        i = 2;
    } else {
        i = 0;
    }
    eprintf(px, py, 0, 0, "%s", i >= 0 && i <= 2 ? wep_mugen_str[i] : "...no string");
    py += 16;
    if (!DbgFlagChk(pG, DBG_KAIOUKEN)) {
        i = 0;
    } else {
        i = PlKaiou + 1;
    }
    eprintf(px, py, 0, 0, "%s", i >= 0 && i <= 4 ? pl_speed_str[i] : "...no string");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_NO_DEATH) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_EM_NO_DEATH) ? "ON" : "OFF");
    py += 48;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_OBA_VIEW) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_SCA_VIEW) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_SAT_DISP) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_EAT_DISP) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_SCE_AT_DISP) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_SHOP_FULL) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_BGM_STOP) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_SE_STOP) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (pG->Disp_flg & 0x40000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (pG->Disp_flg & 0x8000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_EM_WEAK) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", DbgFlagChk(pG, DBG_EM_LIFE_DISP) ? "ON" : "OFF");
}
