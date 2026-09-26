// game/t_util: shared debug tool helpers — TutilInitDefault / QuitDefault save and restore the
// pG flag words and camera around a tool and freeze the game, and ToolMenuDisp_cur draws the
// standard tool menu with a blinking cursor.
#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_sub.h"
#include "t_prim.h"
#include "t_util.h"
#include "game.h"


// Copies of the pG flag words the tools modify, restored by TutilQuitDefault.
CAMERA globalCamera;
u32 debug_flg_bak[4];   // pG->flags_60 .. flags_6C
u32 status_flg_bak[4];  // pG->flags_500C .. 0x5018
u32 stop_flg_bak;       // pG->flags_170
u32 disp_flg_bak;       // pG->flags_58
static u32 system_flg_bak;  // pG->flags_54

// Common debug tool start: ends the game stop mode, sets the primitive environment to the current
// camera, saves the pG flag words (system / stop / disp / debug / status), then freezes the game
// (Stop_flg 0x200 | 0x80) and turns the tool display bits on.
void TutilInitDefault()
{
    TprimView view;

    GameStopModeEnd();
    view.rect.x = Screen.x;
    view.rect.y = Screen.y;
    view.rect.w = Screen.width;
    view.rect.h = Screen.height;
    view.nearz = 0.0f;
    view.farz = 1.0f;
    TprimInitEnv2D3D(&view, pG->Camera.ProjMat, pG->Camera.v_mat);
    globalCamera = pG->Camera;
    system_flg_bak = pG->System_flg;
    stop_flg_bak = pG->Stop_flg;
    disp_flg_bak = pG->Disp_flg;
    {
        u32* debug = (u32*) &pG->Debug_flg[0];

        memcpy(debug_flg_bak, debug, sizeof(debug_flg_bak));
    }
    memcpy(status_flg_bak, &pG->Status_flg[0], sizeof(status_flg_bak));
    pG->Stop_flg |= 0x200;
    pG->Stop_flg |= 0x80;
    DbgFlagOn(pG, DBG_CINESCO_OFF);
    DbgFlagOn(pG, DBG_NO_DEATH);
    DbgFlagOff(pG, DBG_FOG_FAR_GREEN);
}

// Common debug tool end: restores the camera and the saved flag words (keeping the debug 0x100 bit
// if it was set meanwhile), clears the tool-active bit.
void TutilQuitDefault()
{
    {
        u32* cam = (u32*) &pG->Camera;

        memcpy(cam, &globalCamera, sizeof(CAMERA));
    }
    pG->System_flg = system_flg_bak;
    pG->Stop_flg = stop_flg_bak;
    pG->Disp_flg = disp_flg_bak;
    if (DbgFlagChk(pG, DBG_ESPTOOL_MEM_USE)) {
        debug_flg_bak[0] |= 0x100;
    }
    {
        u32* debug = (u32*) &pG->Debug_flg[0];

        memcpy(debug, debug_flg_bak, sizeof(debug_flg_bak));
    }
    memcpy(&pG->Status_flg[0], status_flg_bak, sizeof(status_flg_bak));
    DbgFlagOff(pG, DBG_TEST_MODE);
    DbgFlagOn(pG, DBG_FOG_FAR_GREEN);
}

// Never called in this build. GCC 2.95 emits the initializer templates of local aggregates in
// inline functions at parse time; the original t_util.o has these 7 words between the
// TutilInitDefault constants and the ToolMenuDisp_cur strings. Values are the original's, the
// grouping and body are a guess that reproduces them.
static inline void tutil_2d_env(f32* scale, Vec* size)
{
    f32 sc[4] = {0.0078125f, 0.0f, 0.5f, 0.0f};
    Vec sz = {1024.0f, 2048.0f, 0.0f};

    scale[0] = sc[0];
    scale[1] = sc[2];
    *size = sz;
}

TOOL_MENU* old_menu = NULL;

// Draws a debug menu (`size` bytes of TOOL_MENU entries; greyed when Be_flg is 0) at (x, y) with a
// blinking cursor moved by up / down (flag TOOL_MENU_START_LAST starts at the end, B_LAST jumps
// there on B). `cursor` (optional) carries the position in and out. Returns the cursor.
int ToolMenuDisp_cur(int x, int y, int flg, s8* pCur, TOOL_MENU* pMenu, int MenuSize, JOY* pJoy1)
{
    static s8 cursor_s = 0;
    static u8 flicker = 4;
    TOOL_MENU* p = pMenu;
    int num;
    int i;
    int ret;
    int color;

    if (pCur != NULL) {
        cursor_s = *pCur;
    }
    num = MenuSize / sizeof(TOOL_MENU);
    if (old_menu != p) {
        if (pCur == NULL) {
            if (flg & TOOL_MENU_START_LAST) {
                cursor_s = num - 1;
            } else {
                cursor_s = 0;
            }
        }
        old_menu = p;
    }
    if (pJoy1->rep & JOY_DOWN) {
        cursor_s++;
    }
    if (pJoy1->rep & JOY_UP) {
        cursor_s--;
    }
    cursor_s = cursor_s < 0 ? num - 1 : (cursor_s > num - 1 ? 0 : cursor_s);
    if (pJoy1->rep & (JOY_DOWN | JOY_UP)) {
        flicker = 8;
    }
    if ((pJoy1->trg & JOY_B) && (flg & TOOL_MENU_B_LAST)) {
        cursor_s = num - 1;
        flicker = 8;
    }
    for (i = 0; i < num; i++) {
        color = 0x14;
        if (p->Be_flg) {
            color = 0;
        }
        eprintf(x, y + i * 16, color, 0, "%s", p->pName);
        p++;
    }
    if (flicker & 0x18) {
        eprintf(x - 8, y + cursor_s * 16, 0, 0, ">");
    }
    flicker++;
    if (pCur != NULL) {
        *pCur = cursor_s;
    }
    p = &pMenu[cursor_s];
    if ((pJoy1->trg & JOY_A) && p->Be_flg) {
        if (p->pFunc != NULL) {
            p->pFunc();
        }
        ret = cursor_s;
        cursor_s = 0;
        return ret;
    }
    return -1;
}

// The next unit's .sdata (TexRender: 32-byte aligned vfilter tables) starts 32-byte aligned in
// the original link, which leaves 0x1A zero bytes after `flicker`. The split object of TexRender
// does not carry that alignment yet, so pad this unit's .sdata to the same boundary here.
asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
