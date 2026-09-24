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

// Debug tool helpers for the tool modules (D:/Bio4/Prog/t_util.cpp, the same object in every t_*/Tools
// REL with a tool). The DOL's game/t_util.cpp is a different version of this file: there the menu drawer
// is out of line and the cursor helpers were dead-stripped; here ToolMenuDisp_cur is an inline nobody
// calls (its strings and statics still land in the object) and TutilMoveCursor/TutilGetScreenPos exist.

// Copies of the pG flag words the tools modify, restored by TutilQuitDefault. The camera copy is a
// static (its relocations carry the section offset), the flag words are globals (theirs do not).
static CAMERA globalCamera;
u32 debug_flg_bak[4];   // pG->flags_60 .. flags_6C
u32 stop_flg_bak;       // pG->flags_170
u32 disp_flg_bak;       // pG->flags_58
u32 system_flg_bak;     // pG->flags_54
u32 status_flg_bak[4];  // pG->flags_500C .. 0x5018

// Common tool entry: ends the game stop mode, sets the 2D/3D primitive environment to the screen
// and the game camera, saves pG's camera and the system / stop / disp / debug / status flag words,
// then stops the game (Stop_flg 0x200 | 0x80), turns on Debug_flg[0] 0x8000 and [2] 0x800000 and
// clears [3] 0x2000 for the duration of the tool.
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

// Common tool exit: restores the camera and flag words saved by TutilInitDefault (keeping
// Debug_flg[0] bit 8 if the tool set it), clears Debug_flg[0] bit 31 and sets [3] 0x2000.
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

// Moves a 2D cursor with the analog stick (scaled by `speed`) and the digital pad (by `step`).
void TutilMoveCursor(Vec* pos, f32 anamv, f32 keymv)
{
    pos->x += (f32) Joy[0].stickX * anamv * 0.0078125f;
    pos->y -= (f32) Joy[0].stickY * anamv * 0.0078125f;
    pos->x += (Joy[0].on & JOY_RIGHT) ? keymv : ((Joy[0].on & JOY_LEFT) ? -keymv : 0.0f);
    pos->y += (Joy[0].on & JOY_DOWN) ? keymv : ((Joy[0].on & JOY_UP) ? -keymv : 0.0f);
}

// Screen position of a world point (GXProject with the current camera); `noSetup` skips TprimDraw3D.
int TutilGetScreenPos(Vec* mv, f32* sv, int mode)
{
    f32 proj[7];
    f32 viewport[6];

    if (mode == 0) {
        TprimDraw3D(0);
    }
    GXGetProjectionv(proj);
    GXGetViewportv(viewport);
    GXProject(mv->x, mv->y, mv->z, pG->Camera.v_mat, proj, viewport, &sv[0], &sv[1], &sv[2]);
    return 1;
}

#ifndef T_UTIL_FULL
// Never called in the tool modules. GCC 2.95 emits the initializer templates of local aggregates in
// inline functions at parse time; the original object has these 5 words between TutilMoveCursor's
// constant pool and ToolMenuDisp_cur's strings. Values are the original's, grouping and body a guess.
static inline void tutil_2d_env(f32* scale, Vec* size)
{
    f32 sc[2] = {0.5f, 0.0f};
    Vec sz = {1024.0f, 2048.0f, 0.0f};

    scale[0] = sc[0];
    scale[1] = sc[1];
    *size = sz;
}

#else
// The Tools REL has the full file (src/Tools/t_util.cpp): the 0.5/0, 1024 and 2048 words are the pools of
// these three functions (dead-stripped everywhere else, see tutil_2d_env above).

// Fits the XZ position whose screen projection is closest to `target`: a square of side `step` around
// `center` is projected corner by corner, the closest corner becomes the new origin and the side is
// halved until it underflows to 0.
int TutilGet3DPosXZ(Vec* sv, Vec* bmv, f32 width, Vec* mv)
{
    Vec p[4];
    f32 scr[4];
    f32 dist[4];
    int ok[4];
    f32 x = bmv->x - width * 0.5f;
    f32 z = bmv->z - width * 0.5f;
    int i;
    int j;
    int best;


    p[0].y = p[1].y = p[2].y = p[3].y = bmv->y;
    TprimDraw3D(0);
    do {
        p[0].x = x;
        p[0].z = z;
        p[1].x = x + width;
        p[1].z = z;
        p[2].x = x;
        p[2].z = z + width;
        p[3].x = x + width;
        p[3].z = z + width;
        for (i = 0; i < 4; i++) {
            ok[i] = TutilGetScreenPos(&p[i], scr, 1);
            if (ok[i]) {
                dist[i] = (scr[0] - sv->x) * (scr[0] - sv->x) + (scr[1] - sv->y) * (scr[1] - sv->y);
            }
        }
        if (ok[0] == 0 && ok[1] == 0 && ok[2] == 0 && ok[3] == 0) {
            return 0;
        }
        best = ok[0] ? 0 : -1;
        for (j = 1; j < 4; j++) {
            if (ok[j]) {
                // Two `best = j` statements (jump2 cross-jumps them into one `mr`): the extra
                // reference gives `j` 5 refs at global-alloc time, so it outranks the `j*4` giv
                // (4 refs) and takes r10 first; `best == -1 || ...` leaves them tied (giv first).
                if (best == -1) {
                    best = j;
                } else if (dist[j] < dist[best]) {
                    best = j;
                }
            }
        }
        switch (best) {
        case 0:
            break;
        case 1:
            x += width * 0.5f;
            break;
        case 2:
            z += width * 0.5f;
            break;
        case 3:
            x += width * 0.5f;
            z += width * 0.5f;
            break;
        }
        width *= 0.5f;
    } while (width != 0.0f);
    mv->x = x;
    mv->y = bmv->y;
    mv->z = z;
    return 1;
}

// Repeats the fit from a 1024 square until the position stops moving (at most 8 rounds).
int TutilGet3DPosXZ_Mov(Vec* sv, Vec* bmv, Vec* mv)
{
    Vec prev;
    Vec cur;
    int i = 0;

    cur.x = bmv->x;
    cur.y = bmv->y;
    cur.z = bmv->z;
    do {
        prev.x = cur.x;
        prev.y = cur.y;
        prev.z = cur.z;
        if (TutilGet3DPosXZ(sv, &prev, 1024.0f, &cur) == 0) {
            return 0;
        }
        i++;
        if (i > 7) {
            break;
        }
    } while (prev.x != cur.x || prev.y != cur.y || prev.z != cur.z);
    mv->x = cur.x;
    mv->y = cur.y;
    mv->z = cur.z;
    return 1;
}

// The same from a 2048 square, up to 64 rounds.
int TutilGet3DPosXZ_All(Vec* sv, Vec* bmv, Vec* mv)
{
    Vec prev;
    Vec cur;
    int i = 0;

    cur.x = bmv->x;
    cur.y = bmv->y;
    cur.z = bmv->z;
    do {
        prev.x = cur.x;
        prev.y = cur.y;
        prev.z = cur.z;
        if (TutilGet3DPosXZ(sv, &prev, 2048.0f, &cur) == 0) {
            return 0;
        }
        i++;
        if (i > 63) {
            break;
        }
    } while (prev.x != cur.x || prev.y != cur.y || prev.z != cur.z);
    mv->x = cur.x;
    mv->y = cur.y;
    mv->z = cur.z;
    return 1;
}
#endif

// t_light's build of this file has none of the menu statics (no .data at all; src/tools/t_util_nomenu.cpp)
#ifndef T_UTIL_NO_MENU_DATA
#ifdef T_UTIL_MENU_FUNCS
// t_event/t_sce: old_menu is local (the REL fields of its relocations hold the address) and the zero
// word is missing
static TOOL_MENU* old_menu = NULL;
#else
TOOL_MENU* old_menu = NULL;
#ifndef T_UTIL_NO_NUM
static int old_num = 0;  // unreferenced zero word between old_menu and the menu statics (name unknown)
#endif
#endif
#endif

#ifdef T_UTIL_FULL
// The zero word between TutilGet3DPosXZ_All's pool and the menu strings: a public const object (emitted
// at its definition, referenced by nothing).
extern const f32 TutilZero;
const f32 TutilZero = 0.0f;
#endif

// The menu drawer of the DOL's t_util (game/t_util.cpp ToolMenuDisp_cur). In t_emlist/t_camera/t_id it is
// an inline nothing calls: only its "%s" / ">" strings and the two statics are in the object. t_event and
// t_sce (src/tools/t_util_menu.cpp) have it out of line together with the cursor-less ToolMenuDisp wrapper.
#ifdef T_UTIL_MENU_FUNCS
// ToolMenuDisp_cur with the menu's own static cursor.
int ToolMenuDisp(int x, int y, int flg, TOOL_MENU* pMenu, int MenuSize, JOY* pJoy1)
{
    return ToolMenuDisp_cur(x, y, flg, NULL, pMenu, MenuSize, pJoy1);
}

int ToolMenuDisp_cur(int x, int y, int flag, s8* cursor, TOOL_MENU* menu, int size, JOY* joy)
#else
// Draws a TOOL_MENU list at text position (x, y) (greyed lines for Be_flg 0) with a blinking ">"
// cursor: up/down (repeat) move it with wrap, B jumps to the last entry when TOOL_MENU_B_LAST, a
// new menu starts at 0 or at the last entry (TOOL_MENU_START_LAST). A on an enabled line calls its
// pFunc and returns the index; else -1. `cursor` (may be NULL) holds the caller's cursor.
static inline int tutil_menu_disp(int x, int y, int flag, s8* cursor, TOOL_MENU* menu, int size, JOY* joy)
#endif
{
#ifndef T_UTIL_NO_MENU_DATA
    static s8 cursor_s = 0;
    static u8 flicker = 4;
#else
    s8 cursor_s = 0;
    u8 flicker = 4;
    TOOL_MENU* old_menu = NULL;
#endif
    TOOL_MENU* p = menu;
    int num;
    int i;
    int ret;
    int color;

    if (cursor != NULL) {
        cursor_s = *cursor;
    }
    num = size / sizeof(TOOL_MENU);
    if (old_menu != p) {
        if (cursor == NULL) {
            if (flag & TOOL_MENU_START_LAST) {
                cursor_s = num - 1;
            } else {
                cursor_s = 0;
            }
        }
        old_menu = p;
    }
    if (joy->rep & JOY_DOWN) {
        cursor_s++;
    }
    if (joy->rep & JOY_UP) {
        cursor_s--;
    }
    cursor_s = cursor_s < 0 ? num - 1 : (cursor_s > num - 1 ? 0 : cursor_s);
    if (joy->rep & (JOY_DOWN | JOY_UP)) {
        flicker = 8;
    }
    if ((joy->trg & JOY_B) && (flag & TOOL_MENU_B_LAST)) {
        flicker = 8;
        cursor_s = num - 1;
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
    if (cursor != NULL) {
        *cursor = cursor_s;
    }
    p = &menu[cursor_s];
    if ((joy->trg & JOY_A) && p->Be_flg) {
        if (p->pFunc != NULL) {
            p->pFunc();
        }
        ret = cursor_s;
        cursor_s = 0;
        return ret;
    }
    return -1;
}

