// game/t_option: the debug "option" tool task (ToolOption): a nested menu (cDbOption rno[] per
// level) over player settings — flag edit, weapon / upgrade level swap, position move, life,
// kill, face morphs — and scroll settings — debug display flags, block display and the scroll
// view distances. Runs in a scheduler slot with the game task suspended.
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
#include "item.h"
#include "block.h"
#include "file.h"
#include "main_mem.h"
#include "t_util.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

void DrawGage(int x, int y, int h, int w, int val, int max, int color);
void CamStick2World(Camera* cam, JOY* joy, Vec* out);
extern "C" void Draw_pos(Vec* pos, int size);
void PlSetDamage(int type, int dmg, int flag);
void PlSetCostume();
void PlChangeData();
int ShapeSet(void* shape, int no, void* data, int flag);
void ShapeEnd(void* shape);

extern u8 PlKaiou;
extern u8 PlDbFlag;
extern cModel* pSUB;

// Debug option menu tool (player / scroll settings).
class cDbOption {
public:
    JOY joy[2];   // 0x000
    u8 rno[8];    // 0x4D0  menu routine numbers per level
    u8 count;     // 0x4D8  frames since the last pad input (cursor blink)
    s8 cursor;    // 0x4D9
    u8 be_flag;    // 0x4DA  0 = leave the tool
    u8 pad_4DB;

    void clear(u8 flag);
    void joySet();
    void move();
    void setRno(u8 r0, u8 r1, u8 r2, u8 r3, u8 r4, u8 r5, u8 r6, u8 r7);
};

cDbOption dbPl;
static cDbOption* pT;

// joySet reloads pT between the two pad copies and the original compiler kept that load below
// the first copy's stores. GCC 2.95 only keeps the order when the load is a struct member
// (MEM_IN_STRUCT_P, see cLogPtr in db_log.h), so joySet reads pT through this view.
struct cDbOptionPtr {
    cDbOption* p;
};
#define PT_MEMBER (((cDbOptionPtr*) &pT)->p)

void tp_init();
void tp_menu();
void tp_quit();
void tp_pl();
void tp_pl_menu();
void tp_pl_flag();
void tp_pl_life();
void tp_pl_posmove();
void tp_pl_weapon();
void tp_pl_PlKill();
void tp_pl_mountweapon();
void tp_pl_face();
void tp_scr();
void tp_scr_menu();
void tp_scr_flag();
void tp_scr_view();
void printCursor(int x, int y);

#define PL_LIFE (*(s16*) ((u8*) pG + OFS_PL_LIFE))
#define PL_LIFE_MAX (*(s16*) ((u8*) pG + OFS_PL_LIFE_MAX))
#define SUB_LIFE (*(s16*) ((u8*) pG + OFS_SUB_LIFE))
#define SUB_LIFE_MAX (*(s16*) ((u8*) pG + OFS_SUB_LIFE_MAX))
#define WEP_NO (*(u8*) ((u8*) pG + 0x4FB0))
#define WEP_TYPE (*(u8*) ((u8*) pG + 0x4FB1))
#define WEP_LV_POWER (*(u8*) ((u8*) pG + 0x4FB3))
#define WEP_LV_SPEED (*(u8*) ((u8*) pG + 0x4FB4))
#define WEP_LV_BULLET (*(u8*) ((u8*) pG + 0x4FB5))
#define WEP_LV_RELOAD (*(u8*) ((u8*) pG + 0x4FBA))
#define PL_COSTUME (*(u8*) ((u8*) pG + 0x4FB8))

// Resets the menu state (all levels at 0, cursor 0); be_flag = tool active.
void cDbOption::clear(u8 flag)
{
    setRno(0, 0, 0, 0, 0, 0, 0, 0);
    count = 0;
    cursor = 0;
    be_flag = flag;
}

// Copies both pads for this frame.
void cDbOption::joySet()
{
    PT_MEMBER->joy[0] = Joy[0];
    PT_MEMBER->joy[1] = Joy[1];
}

// Counts idle frames (cursor blink), reset by any pad repeat.
void cDbOption::move()
{
    pT->count++;
    if (Joy[0].rep != 0) {
        pT->count = 0;
    }
}

// Sets the routine number of every menu level.
void cDbOption::setRno(u8 r0, u8 r1, u8 r2, u8 r3, u8 r4, u8 r5, u8 r6, u8 r7)
{
    rno[0] = r0;
    rno[1] = r1;
    rno[2] = r2;
    rno[3] = r3;
    rno[4] = r4;
    rno[5] = r5;
    rno[6] = r6;
    rno[7] = r7;
}

// Debug option tool task: suspends the game task and runs the menu (rno[0]: 0 top, 1 player, 2
// scroll, 3 quit) until it quits; then clears the tool-active bit and resumes the game.
void ToolOption()
{
    static void (*funcTbl[4])() = {tp_menu, tp_pl, tp_scr, tp_quit};

    TaskSuspend(0);
    TaskSleep(1);
    tp_init();
    while (pT->be_flag) {
        pT->joySet();
        funcTbl[pT->rno[0]]();
        pT->move();
        TaskSleep(1);
    }
    TOOL_FLAG(OFS_DEBUG_FLG) &= 0x7FFFFFFF;
    TaskSignal(0);
    TaskExit();
}

// Tool start: fresh menu state.
void tp_init()
{
    pT = &dbPl;
    pT->clear(1);
}

// Top menu: PLAYER / SCROLL / QUIT (B jumps to QUIT).
void tp_menu()
{
    static const char* menuStr[3] = {"PLAYER", "SCROLL", "QUIT"};
    int i;

    eprintf(32, 42, 4, 0, "MENU");
    for (i = 0; i < 3; i++) {
        eprintf(32, (i + 4) * 14, 0, 0, menuStr[i]);
    }
    printCursor(3, pT->cursor + 4);
    if (pT->joy[0].rep & JOY_UP) {
        pT->cursor = (pT->cursor + 2) % 3;
    } else if (pT->joy[0].rep & JOY_DOWN) {
        pT->cursor = (pT->cursor + 4) % 3;
    }
    if (pT->joy[0].rep & JOY_A) {
        pT->setRno(pT->cursor + 1, 0, 0, 0, 0, 0, 0, 0);
        pT->cursor = 0;
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->cursor = 2;
    }
}

// Ends the tool.
void tp_quit()
{
    pT->be_flag = 0;
}

// Player menu dispatcher (rno[1]).
void tp_pl()
{
    static void (*funcTbl[8])() = {tp_pl_menu, tp_pl_flag, tp_pl_weapon, tp_pl_posmove,
                                   tp_pl_life, tp_pl_PlKill, tp_pl_mountweapon, tp_pl_face};

    funcTbl[pT->rno[1]]();
}

// Player menu: flag edit, weapon, position move, life, player kill, face control; B back to the top.
void tp_pl_menu()
{
    static const char* menuStr[7] = {"FLAG EDIT", "WEAPON", "POS MOVE", "LIFE", "PLAYER KILL", "--------",
                                     "FACE CONTROL"};
    int i;

    eprintf(32, 42, 4, 0, "PLAYER");
    for (i = 0; i < 7; i++) {
        eprintf(32, (i + 4) * 14, 0, 0, menuStr[i]);
    }
    printCursor(3, pT->cursor + 4);
    if (pT->joy[0].rep & JOY_UP) {
        pT->cursor = (pT->cursor + 6) % 7;
    } else if (pT->joy[0].rep & JOY_DOWN) {
        pT->cursor = (pT->cursor + 8) % 7;
    }
    if (pT->joy[0].rep & JOY_A) {
        pT->setRno(1, pT->cursor + 1, 0, 0, 0, 0, 0, 0);
        pT->cursor = 0;
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->cursor = 0;
        pT->setRno(0, 0, 0, 0, 0, 0, 0, 0);
    }
}

// OPEN (5 words): case 4 issues `li r4,0xfe` before `addi r3,r30,ItemMgr@l` in both arms in the
// original (ours addi first: the PRE'd high pseudo dies at the addi, weight -1). Local id, mgr
// pointer, void-returning views of dump/get, ternary and inverted test tried.
void tp_pl_flag()
{
    eprintf(32, 42, 4, 0, "FLAG EDIT");
    if (DbgFlagChk(pG, DBG_INF_BULLET2)) {
        eprintf(40, 56, 0, 0, "INF BULLET + RELOAD");
    } else if (DbgFlagChk(pG, DBG_INF_BULLET)) {
        eprintf(40, 56, 0, 0, "INF BULLET");
    } else {
        eprintf(40, 56, 20, 0, "INF BULLET");
    }
    eprintf(40, 70, DbgFlagChk(pG, DBG_NO_DEATH) ? 0 : 20, 0, "NO DEATH");
    eprintf(40, 84, DbgFlagChk(pG, DBG_KAIOUKEN) ? 0 : 20, 0, "KAIOUKEN x%d", PlKaiou + 2);
    eprintf(40, 98, PlDbFlag & 4 ? 0 : 20, 0, "KAIOU ATTACK");
    eprintf(40, 112, ItemMgr.num(0xFE) ? 0 : 20, 0, "ASSAULT JACKET");
    eprintf(40, 126, !(PlDbFlag & 1) ? 20 : 0, 0, "LOCK SPHERE");
    eprintf(40, 140, PlDbFlag & 2 ? 0 : 20, 0, "INFORMATION");
    eprintf(40, 154, DbgFlagChk(pG, DBG_PL_NOHIT) ? 0 : 20, 0, "ATARI NO-HIT");
    printCursor(4, pT->cursor + 4);
    if (pT->joy[0].rep & JOY_UP) {
        pT->cursor = (pT->cursor + 7) % 8;
    } else if (pT->joy[0].rep & JOY_DOWN) {
        pT->cursor = (pT->cursor + 9) % 8;
    }
    if (pT->cursor == 2) {
        if (pT->joy[0].rep & JOY_RIGHT) {
            PlKaiou++;
        }
        if (pT->joy[0].rep & JOY_LEFT) {
            PlKaiou--;
        }
        PlKaiou &= 7;
    }
    if (pT->joy[0].rep & JOY_A) {
        switch (pT->cursor) {
        case 0:
            if ((s32) TOOL_FLAG(OFS_DEBUG_FLG + 12) < 0) {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) &= 0x7FFFFFFF;
                TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 0x400000;
            } else if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x400000) {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) &= 0x7FFFFFFF;
                TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~0x400000;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) |= 0x80000000;
                TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~0x400000;
            }
            break;
        case 1:
            if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x800000) {
                TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~0x800000;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 0x800000;
            }
            break;
        case 2:
            if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x10000) {
                TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~0x10000;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 0x10000;
            }
            break;
        case 3:
            PlDbFlag ^= 4;
            break;
        case 4:
            // COMPILER-DIFF: candidate #1 (arg-move order): the original issues the `addi r3,r30,ItemMgr@l`
            // this-argument after the `li` constants in both arms; a codeless asm reading the num() result
            // in r3 gives the addi an anti-dependence (one cycle in the dump arm, two chained in the get arm).
            if (ItemMgr.num(0xFE)) {
                { register int n3 asm("r3"); asm("" : "=m"(PlKaiou) : "r"(n3)); }
                ItemMgr.dump(0xFE);
            } else {
                { register int n3 asm("r3"); asm("" : "=m"(PlKaiou) : "r"(n3)); asm("" : "=m"(PlDbFlag) : "r"(n3), "m"(PlKaiou)); }
                ItemMgr.get(0xFE, 0);
            }
            PlSetCostume();
            PlChangeData();
            break;
        case 5:
            PlDbFlag ^= 1;
            break;
        case 6:
            PlDbFlag ^= 2;
            break;
        case 7:
            if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 8) {
                TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~8;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 8;
            }
            break;
        }
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->setRno(1, 0, 0, 0, 0, 0, 0, 0);
        pT->cursor = 0;
    }
}

// Life editor: player / Ashley life and maximum with the d-pad.
void tp_pl_life()
{
    eprintf(32, 42, 4, 0, "LIFE");
    eprintf(32, 70, 0, 0, "PLAYER:%4d/%4d", PL_LIFE, PL_LIFE_MAX);
    DrawGage(168, 70, 8, 100, PL_LIFE, PL_LIFE_MAX, -1);
    eprintf(32, 84, 0, 0, "ASHLEY:%4d/%4d", SUB_LIFE, SUB_LIFE_MAX);
    DrawGage(168, 84, 8, 100, SUB_LIFE, SUB_LIFE_MAX, -1);
    switch (pT->cursor) {
    case 0:
        PL_LIFE += (int) (s16) ((f32) Joy[0].stickX * (pT->joy[0].on & JOY_A ? 0.4f : 0.05f));
        if (PL_LIFE < 0) {
            PL_LIFE = 0;
        } else if (PL_LIFE > PL_LIFE_MAX) {
            PL_LIFE = PL_LIFE_MAX;
        }
        break;
    case 1:
        SUB_LIFE += (int) (s16) ((f32) Joy[0].stickX * (pT->joy[0].on & JOY_A ? 0.4f : 0.05f));
        if (SUB_LIFE < 0) {
            SUB_LIFE = 0;
        } else if (SUB_LIFE > SUB_LIFE_MAX) {
            SUB_LIFE = SUB_LIFE_MAX;
        }
        break;
    }
    if ((pT->joy[0].rep & JOY_UP) || (pT->joy[0].trg & 0x80000)) {
        pT->cursor = 0;
    }
    if ((pT->joy[0].rep & JOY_DOWN) || (pT->joy[0].trg & 0x40000)) {
        pT->cursor = 1;
    }
    printCursor(3, pT->cursor + 5);
    if (pT->joy[0].rep & JOY_B) {
        pT->setRno(1, 0, 0, 0, 0, 0, 0, 0);
        pT->cursor = 3;
    }
}

// Free position / yaw move of the player with the stick (X ignores collision, Z brings the
// partner, Y switches to rotation), coordinates printed.
void tp_pl_posmove()
{
    static u32 sfb;
    f32 y;

    if (pT->rno[2] == 0) {
        TaskSignal(0);
        sfb = TOOL_FLAG(OFS_STOP_FLG);
        TOOL_FLAG(OFS_STOP_FLG) = 0xAFFFFFFF;
        pT->rno[2] = 1;
    }
    if (Joy[0].on & JOY_X) {
        TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 8;
    } else {
        TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~8;
    }
    f32 spd = pT->joy[0].on & JOY_A ? 10.0f : 1.0f;
    Vec mv = {0.0f, 0.0f, 0.0f};
    if (Joy[0].on & JOY_Y) {
        eprintf(32, 42, 4, 0, "ANG MOVE");
        pPL->ang.y -= (f32) Joy[0].stickX * 0.001f;
    } else {
        eprintf(32, 42, 4, 0, "POS MOVE");
        CamStick2World(&pG->Cam, &Joy[0], &mv);
        PSVECScale(&mv, &mv, spd);
        y = 0.0f;
        mv.y = y + spd * Joy[0].triggerRight - spd * Joy[0].triggerLeft;
        PSVECAdd(&pPL->pos, &mv, &pPL->pos);
    }
    eprintf(32, 56, 0, 0, "X: %6.0f", pPL->pos.x);
    eprintf(32, 70, 0, 0, "Y: %6.0f", pPL->pos.y);
    eprintf(32, 84, 0, 0, "Z: %6.0f", pPL->pos.z);
    eprintf(32, 98, 0, 0, "R: %6.3f", pPL->ang.y);
    eprintf(32, 126, Joy[0].on & JOY_X ? 0 : 20, 0, "X-BTN:SCR NO HIT");
    eprintf(32, 140, 0, 0, "Z-BTN:CALL SUB-CHAR");
    eprintf(32, 154, 0, 0, "Y-BTN:ROTATE Y");
    Draw_pos(&pPL->pos, 2000);
    if ((Joy[0].on & JOY_Z) && pSUB != NULL && (pSUB->be_flag & 0x201) == 1) {
        pSUB->setPos(&pPL->pos);
    }
    if (pT->joy[0].rep & JOY_B) {
        int cur = 2;  // kept in a callee-saved register across the calls
        TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~8;
        TaskSuspend(0);
        TOOL_FLAG(OFS_STOP_FLG) = sfb;
        pT->setRno(1, 0, 0, 0, 0, 0, 0, 0);
        pT->cursor = cur;
    }
}

// Weapon swap: picks a weapon number / type and the four upgrade levels, then reloads the player's
// weapon (weaponRelease / Load / Init) and puts it in the inventory (ItemMgr.debugWeapon).
void tp_pl_weapon()
{
    static const char* strWepName[46] = {
        "00 HAND",
        "01 HANDGUN      FN57",
        "02 HANDGUN      RUGER",
        "03 HANDGUN      MAUSER",
        "04 HANDGUN      XD95",
        "05 HANDGUN      CIVILIAN",
        "06 HANDGUN      GAVAMENT",
        "07 SHOTGUN",
        "08 SHOTGUN      STRIKER",
        "09 SNIPER LIFLE",
        "10 SNIPER LIFLE HEKKELER",
        "11 MACHINEGUN",
        "12 MACNINEGUN   TOMPSON",
        "13 ROCKET RUNCHER",
        "14 MINE THROWER",
        "15 SUPER MAGNUM",
        "16 KNIFE",
        "17 HANDGUN      VP70",
        "18 --------",
        "19 GRENADE      BOMB",
        "20 --------",
        "21 --------",
        "22 GRENADE      FIRE",
        "23 GRENADE      LIGHT",
        "24 --------",
        "25 EGG          NORMAL",
        "26 KURAUZA KNIFE",
        "27 KURAUZA MACHINE-GUN",
        "28 BOW",
        "29 HUNK MACHINE-GUN",
        "30 ADA GRENADE",
        "31 EGG 2",
        "32 EGG 3",
        "33 SHOTGUN      NEW",
        "34 ADA HAND",
        "35 HUNK HAND",
        "36 KLAUSER HAND",
        "37 WESKER HAND",
        "38 ADA HANDGUN",
        "39 ADA MACHINE-GUN",
        "40 ADA RIFLE",
        "41 HUNK GRENADE",
        "42 KLAUSER LIGHT-GRENADE",
        "43 WESKER HANDGUN",
        "44 WESKER MAGNUM",
        NULL,
    };
    static const char* wepType[4] = {"BASIC", "SILENCER", "STOCK", "SILENCER+STOCK"};
    static const char* bltType[2] = {"NORMAL", "SCOPE"};
    static const char* scopeType[3] = {"NORMAL", "HIGH", "THERMO"};
    static const char* rocketType[3] = {"NORMAL", "LAST BOSS", "MUGEN"};
    int i;

    switch (pT->rno[2]) {
    case 0:
        TOOL_FLAG(OFS_DISP_FLG) |= 0x40000000;
        TOOL_FLAG(OFS_DISP_FLG) |= 0x10000000;
        TaskSleep(1);
        TaskSuspend(0);
        TaskSleep(1);
        pT->cursor = WEP_NO;
        pT->rno[3] = 0;
        pT->rno[4] = 0;
        pT->rno[2] = 1;
    case 1:
        if (pT->joy[0].rep & JOY_UP) {
            pT->cursor = (pT->cursor + 45) % 46;
            if (pT->cursor == 45) {
                pT->rno[4] = 21;
            } else if (pT->cursor < pT->rno[4]) {
                pT->rno[4] = pT->cursor;
            }
            pT->rno[3] = 0;
        } else if (pT->joy[0].rep & JOY_DOWN) {
            pT->cursor = (pT->cursor + 47) % 46;
            if (pT->cursor == 0) {
                pT->rno[4] = 0;
            } else if (pT->cursor >= pT->rno[4] + 25) {
                pT->rno[4] = pT->cursor - 24;
            }
            pT->rno[3] = 0;
        }
        switch (pT->cursor) {
        case 1:
            if (pT->joy[0].rep & (JOY_LEFT | JOY_RIGHT)) {
                pT->rno[3] = pT->rno[3] == 0;
            }
            eprintf(200, (pT->cursor + 4) * 14, 0, 0, wepType[pT->rno[3]]);
            break;
        case 2:
            if (pT->joy[0].rep & (JOY_LEFT | JOY_RIGHT)) {
                pT->rno[3] = pT->rno[3] == 0;
            }
            eprintf(208, (pT->cursor + 4) * 14, 0, 0, wepType[pT->rno[3]]);
            break;
        case 3:
            if (pT->joy[0].rep & (JOY_LEFT | JOY_RIGHT)) {
                pT->rno[3] = pT->rno[3] == 0 ? 2 : 0;
            }
            eprintf(216, (pT->cursor + 4) * 14, 0, 0, wepType[pT->rno[3]]);
            break;
        case 11:
            if (pT->joy[0].rep & JOY_RIGHT) {
                pT->rno[3] = (pT->rno[3] + 1) % 4;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                pT->rno[3] = (pT->rno[3] + 3) % 4;
            }
            eprintf(160, (pT->cursor + 4) * 14, 0, 0, wepType[pT->rno[3]]);
            break;
        case 14:
            if (pT->joy[0].rep & JOY_RIGHT) {
                pT->rno[3] = (pT->rno[3] + 1) % 2;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                pT->rno[3] = (pT->rno[3] + 1) % 2;
            }
            eprintf(160, (pT->cursor + 4) * 14, 0, 0, bltType[pT->rno[3]]);
            break;
        case 9:
            if (pT->joy[0].rep & JOY_RIGHT) {
                pT->rno[3] = (pT->rno[3] + 1) % 3;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                pT->rno[3] = (pT->rno[3] + 2) % 3;
            }
            eprintf(160, (pT->cursor + 4) * 14, 0, 0, scopeType[pT->rno[3]]);
            break;
        case 10:
            if (pT->joy[0].rep & JOY_RIGHT) {
                pT->rno[3] = (pT->rno[3] + 1) % 3;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                pT->rno[3] = (pT->rno[3] + 2) % 3;
            }
            eprintf(232, (pT->cursor + 4) * 14, 0, 0, scopeType[pT->rno[3]]);
            break;
        case 13:
            if (pT->joy[0].rep & JOY_RIGHT) {
                pT->rno[3] = (pT->rno[3] + 1) % 3;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                pT->rno[3] = (pT->rno[3] + 2) % 3;
            }
            eprintf(176, (pT->cursor + 4) * 14, 0, 0, rocketType[pT->rno[3]]);
            break;
        }
        if (pT->joy[0].rep & JOY_A) {
            if (pT->cursor != WEP_NO || pT->rno[3] != WEP_TYPE) {
                cPlayer* pl = pPL;
                pl->weaponRelease();
                pl->weaponLoad(pT->cursor, pT->rno[3]);
                pl->weaponInit();
                pl->r_no_0 = 0;
                pl->r_no_1 = 0;
                pl->r_no_2 = 0;
                pl->r_no_3 = 0;
                ItemMgr.debugWeapon(WeaponNo2WeaponId(pT->cursor, pT->rno[3]));
            } else {
                pT->cursor = 0;
                pT->rno[3] = 0;
                pT->rno[2] = 2;
            }
        }
        if (pT->joy[0].rep & JOY_X) {
            pT->cursor = 0;
            pT->rno[3] = 0;
            pT->rno[2] = 2;
        }
        printCursor(3, pT->cursor + 4 - pT->rno[4]);
        break;
    case 2:
        if (pT->joy[0].rep & JOY_UP) {
            pT->cursor = (pT->cursor + 3) % 4;
            pT->rno[3] = 0;
        } else if (pT->joy[0].rep & JOY_DOWN) {
            pT->cursor = (pT->cursor + 5) % 4;
            pT->rno[3] = 0;
        }
        switch (pT->cursor) {
        case 0:
            if (pT->joy[0].rep & JOY_RIGHT) {
                WEP_LV_POWER = (WEP_LV_POWER + 1) % 7;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                WEP_LV_POWER = (WEP_LV_POWER + 6) % 7;
            }
            break;
        case 1:
            if (pT->joy[0].rep & JOY_RIGHT) {
                WEP_LV_SPEED = (WEP_LV_SPEED + 1) % 3;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                WEP_LV_SPEED = (WEP_LV_SPEED + 2) % 3;
            }
            break;
        case 2:
            if (pT->joy[0].rep & JOY_RIGHT) {
                WEP_LV_BULLET = (WEP_LV_BULLET + 1) % 6;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                WEP_LV_BULLET = (WEP_LV_BULLET + 5) % 6;
            }
            break;
        case 3:
            if (pT->joy[0].rep & JOY_RIGHT) {
                WEP_LV_RELOAD = (WEP_LV_RELOAD + 1) % 5;
            }
            if (pT->joy[0].rep & JOY_LEFT) {
                WEP_LV_RELOAD = (WEP_LV_RELOAD + 4) % 5;
            }
            break;
        }
        if (pT->joy[0].rep & (JOY_A | JOY_X)) {
            pT->cursor = WEP_NO;
            pT->rno[3] = 0;
            pT->rno[2] = 1;
        }
        printCursor(34, pT->cursor + 5);
        break;
    }
    if (pT->rno[2] == 1) {
        eprintf(32, 42, 4, 0, "WEAPON");
        eprintf(96, 42, 0, 0, "%s", strWepName[pT->cursor]);
    } else {
        eprintf(32, 42, 19, 0, "WEAPON");
    }
    for (i = 0; i < 25; i++) {
        eprintf(32, (i + 4) * 14, WEP_NO == i ? 0 : 20, 0, strWepName[i + pT->rno[4]]);
    }
    eprintf(280, 56, pT->rno[2] == 2 ? 4 : 19, 0, "LEVEL");
    eprintf(280, 70, 0, 0, "POWER  Lv.%d", WEP_LV_POWER + 1);
    eprintf(280, 84, 0, 0, "SPEED  Lv.%d", WEP_LV_SPEED + 1);
    eprintf(280, 98, 0, 0, "BULET  Lv.%d", WEP_LV_BULLET + 1);
    eprintf(280, 112, 0, 0, "RELOAD Lv.%d", WEP_LV_RELOAD + 1);
    if (pT->joy[0].rep & JOY_B) {
        int zero = 0;  // callee-saved zero reused as setRno's stack argument
        TOOL_FLAG(OFS_DISP_FLG) &= ~0x40000000;
        TOOL_FLAG(OFS_DISP_FLG) &= ~0x10000000;
        TaskSignal(0);
        pT->setRno(1, 0, 0, 0, 0, 0, 0, zero);
        pT->cursor = 1;
    }
}

// A kills the player (9999 damage).
void tp_pl_PlKill()
{
    eprintf(32, 42, 4, 0, "PRESS A TO PLAYER WILL DIE.");
    if (pT->joy[0].rep & JOY_A) {
        PlSetDamage(0, 9999, 0);
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->setRno(1, 0, 0, 0, 0, 0, 0, 0);
    }
}

// Placeholder page ("MOUNT-WEAPON", nothing to do).
void tp_pl_mountweapon()
{
    eprintf(32, 42, 4, 0, "MOUNT-WEAPON");
    if (pT->joy[0].rep & JOY_B) {
        pT->setRno(1, 0, 0, 0, 0, 0, 0, 0);
    }
}

// Face morph tester: plays one of the player archive's shape morphs on the head, or resets it.
void tp_pl_face()
{
    static const char* pFileName[6][3] = {
        {"d:/bio4/room/em/pl00/motion/pl00f0.fcv", "d:/bio4/room/em/pl00/motion/pl00f1.fcv",
         "d:/bio4/room/em/pl00/motion/pl00f2.fcv"},
        {"d:/bio4/room/em/pl01/motion/pl01f0.fcv", "d:/bio4/room/em/pl01/motion/pl01f1.fcv",
         "d:/bio4/room/em/pl01/motion/pl01f2.fcv"},
        {"d:/bio4/room/em/pl01/motion/pl01f0.fcv", "d:/bio4/room/em/pl01/motion/pl01f1.fcv",
         "d:/bio4/room/em/pl01/motion/pl01f2.fcv"},
        {"d:/bio4/room/em/pl00/motion/pl00f0.fcv", "d:/bio4/room/em/pl00/motion/pl00f1.fcv",
         "d:/bio4/room/em/pl00/motion/pl00f2.fcv"},
        {"d:/bio4/room/em/pl00/motion/pl00f0.fcv", "d:/bio4/room/em/pl00/motion/pl00f1.fcv",
         "d:/bio4/room/em/pl00/motion/pl00f2.fcv"},
        {"d:/bio4/room/em/pl00/motion/pl00f0.fcv", "d:/bio4/room/em/pl00/motion/pl00f1.fcv",
         "d:/bio4/room/em/pl00/motion/pl00f2.fcv"},
    };
    static u32 sfb;
    static void* pData = NULL;
    cPlayer* pl = pPL;
    u32 i;

    if (pT->rno[2] == 0) {
        TaskSignal(0);
        sfb = TOOL_FLAG(OFS_STOP_FLG);
        TOOL_FLAG(OFS_STOP_FLG) = 0xAFFFFFFF;
        pData = NULL;
        pT->rno[2] = 1;
    }
    eprintf(32, 42, 4, 0, "FACE CONTROL");
    printCursor(3, pT->cursor + 4);
    eprintf(32, 56, 0, 0, "RESET");
    for (i = 0; i < 3; i++) {
        eprintf(32, (i + 5) * 14, 0, 0, pFileName[PL_COSTUME][i]);
    }
    if (pT->joy[0].rep & JOY_UP) {
        pT->cursor = (u32) (pT->cursor + 3) % 4;
    } else if (pT->joy[0].rep & JOY_DOWN) {
        pT->cursor = (u32) (pT->cursor + 5) % 4;
    }
    if (pT->joy[0].rep & JOY_A) {
        if (pT->cursor == 0) {
            ShapeEnd(pl->Body->pShape);
        } else {
            if (pData != NULL) {
                Debug_free(pData);
            }
            if (HDReadDebugAlloc(pFileName[PL_COSTUME][pT->cursor - 1], &pData, 1)) {
                ShapeSet(pl->Body->pShape, 0, pData, 2);
            } else {
                ShapeEnd(pl->Body->pShape);
            }
        }
    }
    if (pT->joy[0].rep & JOY_B) {
        if (pData != NULL) {
            Debug_free(pData);
        }
        TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~8;
        pT->setRno(1, 0, 0, 0, 0, 0, 0, 0);
        TaskSuspend(0);
        TOOL_FLAG(OFS_STOP_FLG) = sfb;
    }
}

// Scroll menu dispatcher (rno[1]).
void tp_scr()
{
    static void (*funcTbl[3])() = {tp_scr_menu, tp_scr_flag, tp_scr_view};

    funcTbl[pT->rno[1]]();
}

// Scroll menu: flag edit, view; B back to the top.
void tp_scr_menu()
{
    static const char* menuStr[2] = {"FLAG EDIT", "DISP MODE"};
    int i;

    eprintf(32, 42, 4, 0, "SCROLL");
    for (i = 0; i < 2; i++) {
        eprintf(32, (i + 4) * 14, 0, 0, menuStr[i]);
    }
    printCursor(3, pT->cursor + 4);
    if (pT->joy[0].rep & JOY_UP) {
        pT->cursor = (pT->cursor + 1) % 2;
    } else if (pT->joy[0].rep & JOY_DOWN) {
        pT->cursor = (pT->cursor + 3) % 2;
    }
    if (pT->joy[0].rep & JOY_A) {
        pT->setRno(2, pT->cursor + 1, 0, 0, 0, 0, 0, 0);
        pT->cursor = 0;
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->cursor = 0;
        pT->setRno(0, 0, 0, 0, 0, 0, 0, 0);
    }
}

// Scroll debug flags: green background, log off, fog off, all blocks displayed, error check.
void tp_scr_flag()
{
    eprintf(32, 42, 4, 0, "FLAG EDIT");
    eprintf(40, 56, DbgFlagChk(pG, DBG_FOG_FAR_GREEN) ? 0 : 20, 0, "BG COLOR GREEN");
    eprintf(40, 70, DbgFlagChk(pG, DBG_LOG_OFF) ? 0 : 20, 0, "LOG OFF");
    eprintf(40, 84, DpfFlagChk(pG, DPF_FOG) ? 0 : 20, 0, "FOG OFF");
    eprintf(40, 98, Block.allDisp == 1 ? 0 : 20, 0, "BLOCK ALL DISP");
    eprintf(40, 112, DbgFlagChk(pG, DBG_ERROR_CK) ? 0 : 20, 0, "ERROR CHECK");
    int num = 5;
    printCursor(4, pT->cursor + 4);
    if (pT->joy[0].rep & JOY_UP) {
        pT->cursor = (pT->cursor + num - 1) % num;
    } else if (pT->joy[0].rep & JOY_DOWN) {
        pT->cursor = (pT->cursor + num + 1) % num;
    }
    if (pT->joy[0].rep & JOY_A) {
        switch (pT->cursor) {
        case 0:
            if (TOOL_FLAG(OFS_DEBUG_FLG + 12) & 0x2000) {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) &= ~0x2000;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) |= 0x2000;
            }
            break;
        case 1:
            if (TOOL_FLAG(OFS_DEBUG_FLG + 12) & 0x4000000) {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) &= ~0x4000000;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) |= 0x4000000;
            }
            break;
        case 2:
            if (TOOL_FLAG(OFS_DISP_FLG) & 0x4000) {
                TOOL_FLAG(OFS_DISP_FLG) &= ~0x4000;
            } else {
                TOOL_FLAG(OFS_DISP_FLG) |= 0x4000;
            }
            break;
        case 3:
            if (Block.allDisp == 1) {
                Block.dispAllBlock(0);
            } else {
                Block.dispAllBlock(1);
            }
            break;
        case 4:
            if (TOOL_FLAG(OFS_DEBUG_FLG + 12) & 0x400000) {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) &= ~0x400000;
            } else {
                TOOL_FLAG(OFS_DEBUG_FLG + 12) |= 0x400000;
            }
            break;
        }
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->setRno(2, 0, 0, 0, 0, 0, 0, 0);
        pT->cursor = 0;
    }
}

// Scroll view mode: what the scenery draws as — polygons, scroll collision, effect collision,
// polygons + either, or off (Disp_flg / Debug_flg bits).
void tp_scr_view()
{
    static const char* strMode[6] = {"POLYGON", "SCR AT", "EFF AT", "POLYGON + SAT", "POLYGON + EAT", "OFF"};
    int chg = 0;

    if (pT->rno[2] == 0) {
        chg = 1;
        pT->rno[2] = 1;
    }
    eprintf(32, 42, 4, 0, "SCROLL VIEW");
    eprintf(32, 56, 0, 0, strMode[pT->cursor]);
    if (pT->joy[0].rep & (JOY_UP | JOY_RIGHT)) {
        pT->cursor = (pT->cursor + 7) % 6;
        chg = 1;
    } else if (pT->joy[0].rep & (JOY_DOWN | JOY_LEFT)) {
        pT->cursor = (pT->cursor + 5) % 6;
        chg = 1;
    }
    if (chg) {
        TOOL_FLAG(OFS_DISP_FLG) |= 0x8000000;
        TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x8000000;
        TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x4000000;
        switch (pT->cursor) {
        case 0:
            TOOL_FLAG(OFS_DISP_FLG) &= ~0x8000000;
            break;
        case 1:
            TOOL_FLAG(OFS_DISP_FLG) |= 0x8000000;
            TOOL_FLAG(OFS_DEBUG_FLG) |= 0x8000000;
            break;
        case 2:
            TOOL_FLAG(OFS_DISP_FLG) |= 0x8000000;
            TOOL_FLAG(OFS_DEBUG_FLG) |= 0x4000000;
            break;
        case 3:
            TOOL_FLAG(OFS_DISP_FLG) &= ~0x8000000;
            TOOL_FLAG(OFS_DEBUG_FLG) |= 0x8000000;
            break;
        case 4:
            TOOL_FLAG(OFS_DISP_FLG) &= ~0x8000000;
            TOOL_FLAG(OFS_DEBUG_FLG) |= 0x4000000;
            break;
        case 5:
            break;
        }
    }
    if (pT->joy[0].rep & JOY_B) {
        pT->setRno(2, 0, 0, 0, 0, 0, 0, 0);
        pT->cursor = 0;
    }
    if (TOOL_FLAG(OFS_DEBUG_FLG) & 0x8000000) {
        SatMgr.disp(0);
    }
    if (TOOL_FLAG(OFS_DEBUG_FLG) & 0x4000000) {
        EatMgr.disp(0);
    }
}

// Draws the blinking ">" cursor at text cell (x, y).
void printCursor(int x, int y)
{
    if (!(pT->count & 8)) {
        eprintf(x * 8, y * 14, 0, 0, ">");
    }
}

// .sdata is 8-aligned in the split object (4-byte pad after the last static).
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
