// game/main_sub: render frame plumbing and system helpers (D:/Bio4/Prog/main_sub.cpp): the GX
// render mode (Rmode, 512x448 non-AA), frame buffers and FIFO set-up (Render_init), the per-frame
// begin/done/swap, screen/scissor size, near clip control, play-time accounting, screen shots,
// stopwatch, VI brightness filter, TPL/texture quad drawing and REL module link/unlink.
#include "types.h"
#include "global.h"
#include "gx.h"
#include "os_vi.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "vec.h"
#include "view.h"
#include "scheduler.h"
#include "db_log.h"
#include "eprintf.h"
#include "tpl.h"
#include "tv_mode.h"
#include <stdio.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/gx/GXManage.h>
#include <dolphin/vi/vifuncs.h>
#include "debug.h"
#include "sce.h"
#include "trans_ot.h"
#include <dolphin/os.h>
#include "sce_sys.h"

void SetDrawTmpBufType(int type);

// Low memory globals (OSPhysicalToCached(0x00F8) = bus clock); a struct member so the
// address splits into `lis 0x8000` + displacement. Replaces the SDK macros of the same names.
struct OSLowMem {
    u8 pad_0[0xF8];
    u32 busClock;  // 0xF8
};
#undef OS_BUS_CLOCK
#undef OS_TIMER_CLOCK
#undef OSTicksToSeconds
#undef OSTicksToMicroseconds
#define OS_BUS_CLOCK (((OSLowMem*) 0x80000000)->busClock)
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)
#define OSTicksToMicroseconds(ticks) (((ticks) * 8) / (OS_TIMER_CLOCK / 125000))
#define VIPadFrameBufferWidth(width) ((u16) (((u16) (width) + 15) & ~15))

// HALT() (db_log.h) is a plain block, not do/while(0): the loop notes of a do/while are a scheduling
// barrier, and the original issues `li r4,0` of the preceding pLog->err before the string address
// (anti-dependence on HALT's own `lis r4`), which needs one scheduling region (DLL_Link/DLL_Unlink,
// read.cpp too).

#line 30 "D:/Bio4/Prog/main_sub.cpp"

// GX verify callback: forwards warnings to OSReport.
// Dead-stripped in the original (its string survives in .rodata).
static void GXVerifyCallback(int level, u32 id, const char* msg)
{
    OSReport("Level %d, Warning %03d\n", level, id);
}

GXRenderModeObj Rmode = {
    0,       // viTVmode
    0x200,   // fbWidth
    0x1C0,   // efbHeight
    0x1C0,   // xfbHeight
    0x28,    // viXOrigin
    0x10,    // viYOrigin
    0x280,   // viWidth
    0x1C0,   // viHeight
    1,       // xFBmode
    0,       // field_rendering
    0,       // aa
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};

void* DefaultFifoObj;
int ScreenShotTriggerType;
void* DefaultFifo;
int flag_render_after = 0;
int lbl_80314BFC = 0;  // ScreenShotCount: unreferenced, Bio4.sym has no name (strip_unused keeps lbl_ names)
int AutoScreenShotExec = 0;
char ScreenShotFilename[11];
OSStopwatch SW;

static int AutoScreenShotFrame;
static char* AutoScreenShotFilename;
static int ScreenShotFrame;
static u8 Line;

// Boot: TV mode, two XFBs at 0x80460000, the 0x70000-byte GX FIFO at 0x803F0000, GXInit, the
// screen GX state, first VI configure and the 512x448 screen size.
void Render_init()
{
    GXRenderModeObj* rm = &Rmode;

    SetTvMode(rm);
    pFrame_buff[0] = (void*) 0x80460000;
    pCurrent_buff = pFrame_buff[1] =
        (void*) (0x80460000 + VIPadFrameBufferWidth(rm->viWidth) * rm->xfbHeight * 2);
    DefaultFifo = (void*) 0x803F0000;
    VIConfigure(rm);
    DefaultFifoObj = GXInit(DefaultFifo, 0x70000);
    ScreenGXSet();
    GXSetDispCopyYScale((f32) rm->xfbHeight / (f32) rm->efbHeight);
    GXSetDispCopySrc(0, 0, rm->fbWidth, rm->xfbHeight);
    GXSetDispCopyDst(rm->fbWidth, rm->xfbHeight);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    if (rm->aa) {
        GXSetPixelFmt(2, 0);
    } else {
        GXSetPixelFmt(0, 0);
    }
    GXSetPixelFmt(1, 0);
    GXSetZCompLoc(0);
    GXCopyDisp(pCurrent_buff, 0);
    GXSetDispCopyGamma(0);
    VISetNextFrameBuffer(pFrame_buff[0]);
    pCurrent_buff = pFrame_buff[1];
    VIFlush();
    VIWaitForRetrace();
    ScreenReSize(512, 448);
}

// Frame start: field-rendering viewport jitter and the default (dim) copy filter.
void Render_before()
{
    if (Rmode.field_rendering) {
        GXSetViewportJitter(0.0f, 0.0f, Screen.width, Screen.height, 0.0f, 1.0f, VIGetNextField());
    } else {
        GXSetViewport(0.0f, 0.0f, Screen.width, Screen.height, 0.0f, 1.0f);
    }
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    Bg_brightness_set(64.0f);
}

// Frame end: waits for the scenario C-task when it is outside its frame range, runs the
// after-render OT (0x16), sets the brightness filter, copies the EFB to the XFB with the draw
// sync token 0xADEB.
void Render_done()
{
    GXSetZMode(1, 3, 1);
    GXSetColorUpdate(1);
    if (SceSys.checkCTaskRange() == 0) {
        GXDrawDone();
    } else {
        SceSys.setDrawDone(1);
        SceSleep(1);
    }
    after_render_proc();
    Bg_brightness_set((f32) pSys->brightness);
    if (DbgFlagChk(pG, DBG_SLOW_MODE)) {
        GXCopyDisp(pCurrent_buff, 0);
    } else {
        GXSetAlphaUpdate(1);
        GXCopyDisp(pCurrent_buff, 1);
        GXSetAlphaUpdate(0);
    }
    if (SceSys.checkCTaskRange() == 0) {
        GXDrawDone();
    } else {
        SceSys.setDrawDone(1);
        SceSleep(1);
    }
}

// Presents the current XFB (unless System_flg 0x400 holds the picture) and flips buffers.
void Render_swap()
{
    if (!SysFlagChk(pG, SYS_SCREEN_STOP)) {
        VISetNextFrameBuffer(pCurrent_buff);
        if (pCurrent_buff == pFrame_buff[0]) {
            pCurrent_buff = pFrame_buff[1];
        } else {
            pCurrent_buff = pFrame_buff[0];
        }
    }
    VIFlush();
}

// Start of the game frame: restores ZNEAR to 100 unless a SetNearClipDist request is pending
// (Status_flg[1] 0x1000), which it consumes.
void UpdateNearClipDist()
{
    GlobalWork* g = pG;
    if (!StaFlagChk(g, STA_NEARCLIP_TOUCH)) {
        ZNEAR = 100.0f;
    }
    StaFlagOff(g, STA_NEARCLIP_TOUCH);
}

// Requests a different near clip distance for this frame (water/ filter copies).
void SetNearClipDist(f32 dist)
{
    ZNEAR = dist;
    StaFlagOn(pG, STA_NEARCLIP_TOUCH);
}

// 1 in the game steps where post filters may run (Rno0 3 main loop, 4 door demo, 6 option).
int Render_checkBlurPermission()
{
    u8 mode = pG->Rno0;
    if (mode == 3 || mode == 4 || mode == 6) {
        return 1;
    }
    return 0;
}

// GX draw sync callback: token 0xADEB marks the end of the frame's rendering (System_flg 0x10000000).
void Render_DrawSyncCallback(u16 token)
{
    if (token == 0xADEB) {
        ProcessTickGet(1, "RENDER END");
        SysFlagOn(pG, SYS_RENDER_END);
    }
}

// Blanks / unblanks the video output and mirrors it in System_flg 0x40000.
void systemVISetBlack(int sw)
{
    if (sw == 1) {
        VISetBlack(1);
        SysFlagOn(pG, SYS_SET_BLACK);
    } else {
        VISetBlack(0);
        SysFlagOff(pG, SYS_SET_BLACK);
    }
}

// Restores the normal scissor (full frame, or none when Status_flg[3] 0x10000000).
void SetScissorState()
{
    if (StaFlagChk(pG, STA_SCISSOR)) {
        GXSetScissor(0, 56, (u32) Screen.width, (u32) Screen.height - 111);
    } else {
        SetNoScissor();
    }
}

// Disables the scissor rectangle (full-screen copies).
void SetNoScissor()
{
    GXSetScissor((u32) Screen.x, (u32) Screen.y, (u32) Screen.width, (u32) Screen.height);
}

// 1 when the screen origin is 0.
// Dead-stripped in the original (its SF 0.0 constant survives in .rodata).
static int ScreenIsOrigin()
{
    return Screen.x == 0.0f;
}

// Sets viewport, scissor, display copy source/destination and pixel format for the current screen size.
void ScreenGXSet()
{
    GXSetViewport(Screen.x, Screen.y, Screen.width, Screen.height, 0.0f, 1.0f);
    SetScissorState();
}

// Changes the EFB/screen size (e.g. 512x448 normal, smaller for the movie player), reconfigures VI
// and the GX state.
void ScreenReSize(u16 w, u16 h)
{
    Mtx44 mtx;
    GXRenderModeObj* rm = &Rmode;

    Screen.width = (f32) w;
    Screen.height = (f32) h;
    rm->fbWidth = w;
    rm->efbHeight = h;
    DCFlushRange(pFrame_buff[0], 0x8C000);
    DCFlushRange(pFrame_buff[1], 0x8C000);
    VIConfigure(rm);
    VIFlush();
    ScreenGXSet();
    GXSetDispCopyYScale((f32) rm->xfbHeight / (f32) rm->efbHeight);
    GXSetDispCopySrc(0, 0, rm->fbWidth, rm->xfbHeight);
    GXSetDispCopyDst(rm->fbWidth, rm->xfbHeight);
    C_MTXOrtho(mtx, 0.0f, 448.0f, 0.0f, (f32) rm->fbWidth, 0.0f, -10000.0f);
    GXSetProjection(mtx, 1);
}

// Sets the Screen size only (viewport/scissor follow on the next ScreenGXSet).
void EFBReSize(int w, int h)
{
    Screen.width = (f32) w;
    Screen.height = (f32) h;
    GXSetViewport(Screen.x, Screen.y, Screen.width, Screen.height, 0.0f, 1.0f);
    GXSetScissor((u32) Screen.x, (u32) Screen.y, (u32) Screen.width, (u32) Screen.height);
}

// Splits seconds into h/m/s (any output may be NULL).
void SecToTime(u32 sec, u32* ret_time, u32* ret_min, u32* ret_sec)
{
    u32 hour = sec / 3600;
    if (ret_time) {
        *ret_time = hour;
    }
    if (ret_min) {
        *ret_min = (sec - hour * 3600) / 60;
    }
    if (ret_sec) {
        *ret_sec = sec % 60;
    }
}

// Marks the start of a play-time segment (game_start_time = now).
void InitGameTime()
{
    OSTime t = OSGetTime();
    pG->game_start_time = OSTicksToSeconds(t);
}

// Total play time in seconds (saved play_time + the running segment), also split into h/m/s.
u32 GetGameTime(u32* ret_time, u32* ret_min, u32* ret_sec)
{
    u32 sec;
    OSTime t = OSGetTime();
    sec = OSTicksToSeconds(t) - pG->game_start_time + pG->play_time;
    SecToTime(sec, ret_time, ret_min, ret_sec);
    return sec;
}

// Folds the running segment into pG->play_time and restarts the segment (before saves/pauses).
void SetGameTime()
{
    OSTime t = OSGetTime();
    pG->play_time += OSTicksToSeconds(t) - pG->game_start_time;
    t = OSGetTime();
    pG->game_start_time = OSTicksToSeconds(t);
}

// Tool: starts an automatic screen shot sequence (name prefix, frame count).
void ScreenShotStart(char* file_name, int start_frame, int flag336)
{
    static int AutoScreenShotExecFlag = 1;
    AutoScreenShotExecFlag = flag336;
    AutoScreenShotExec = 1;
    AutoScreenShotFrame = start_frame;
    AutoScreenShotFilename = file_name;
}

// Tool: stops the automatic screen shots.
void ScreenShotEnd()
{
    AutoScreenShotExec = 0;
}

int ScreenShotExec = 0;
int lbl_80314C0C = 0;  // ScreenShotWait: unreferenced, Bio4.sym has no name

// Boot: builds the screen shot file name stamp (_MMDDhhmm_) and clears the shot state.
void SelfScreenShotInit()
{
    OSCalendarTime ct;
    OSTicksToCalendarTime(OSGetTime(), &ct);
    sprintf(ScreenShotFilename, "_%02d%02d%02d%02d_", ct.mon + 1, ct.mday, ct.hour, ct.min);
    ScreenShotFrame = 0;
    ScreenShotTriggerType = 0;
    ScreenShotExec = 0;
    AutoScreenShotExec = 0;
}

// Tool: writes the current frame as d:\bio4/Room/Sc_shot/r<room><stamp><frame>.bmp on the host.
// Dead-stripped in the original (strings and constant pool survive in .rodata).
static void ScreenShotMain(int frame)
{
    char buf[64];
    char name[64];
    sprintf(name, "%s_%05d.bmp", ScreenShotFilename, frame);
    sprintf(buf, "d:\\bio4/Room/Sc_shot/r%03x%s%06d.bmp", G_ROOM_ID, ScreenShotFilename, frame);
    if (Screen.x != 1.0f) {
        Screen.y = (f32) frame;
    }
}

// Debug profiling: inits the stopwatch and the print line.
void StopwatchInit()
{
    OSInitStopwatch(&SW, "");
    Line = 0;
}

// Debug profiling: restarts the stopwatch.
void StopwatchStart()
{
    OSResetStopwatch(&SW);
    OSStartStopwatch(&SW);
}

// Debug profiling: stops and returns the elapsed microseconds (printed with `name` when given).
u32 StopwatchStop(const char* pName)
{
    OSTime us;
    OSStopStopwatch(&SW);
    us = OSTicksToMicroseconds(SW.total);
    if (pName) {
        eprintf2(10, 16, 50, Line + 50, 0, 1, "%s %d", (u32) us, pName);
        Line += 16;
    }
    return us;
}

// Runs the after-render OT list (0x16: filters that need the finished frame) and releases the draw
// temp buffer.
void after_render_proc()
{
    flag_render_after = 1;
    ExecOt(0x16);
    SetDrawTmpBufType(0);
    flag_render_after = 0;
}

// Sets the VI vertical copy filter from a brightness (64 = normal): the 7 taps are scaled and the
// remainder distributed in the `order` sequence.
void Bg_brightness_set(f32 bright)
{
    u8 vf[7] = {8, 8, 10, 12, 10, 8, 8};
    u8 order[7] = {5, 1, 3, 0, 4, 2, 6};
    GXRenderModeObj* rm = &Rmode;
    f32 rate = bright * (1.0f / 64.0f);
    int i;
    int j;

    for (i = 0; i < 7; i++) {
        rm->vfilter[i] = (u8) ((f32) (int) vf[i] * rate);
        bright -= (f32) (int) rm->vfilter[i];
    }
    i = 0;
    while (bright >= 1.0f) {
        for (j = 0; j < 7; j++) {
            if (order[j] == i) {
                break;
            }
        }
        rm->vfilter[j]++;
        bright -= 1.0f;
        if (i++ > 6) {
            i = 0;
        }
    }
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
}

// Draws texture 0 of a TPL (relocating its offsets on first use, CI formats with TLUT) as a screen
// quad at (x, y) of w x h pixels.
void DrawTpl(TEXPalette* tpl, int x, int y, int w, int h)
{
    GXTexObj texObj;
    GXTlutObj tlutObj;
    TEXDescriptor* desc;
    TEXHeader* hdr;
    CLUTHeader* clut;
    u32 addr = (u32) tpl;

    if (addr < 0x80000000 || addr > 0x82FFFFFF) {
        return;
    }
    desc = (TEXDescriptor*) (tpl + 1);
    if (!VALID_PTR(desc)) {
        return;
    }
    if ((s32) desc->textureHeader >= 0) {
        desc->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) desc->textureHeader);
        if (!VALID_PTR(desc->textureHeader)) {
            return;
        }
        desc->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) desc->CLUTHeader);
        desc->textureHeader->data = (u8*) tpl + (u32) desc->textureHeader->data;
        desc->CLUTHeader->data = (u8*) tpl + (u32) desc->CLUTHeader->data;
        if ((s32) desc->textureHeader >= 0) {
            return;
        }
    }
    hdr = desc->textureHeader;
    if ((u32) hdr > 0x82FFFFFF) {
        return;
    }
    if (hdr->format - 8 <= 1) {
        GXInitTexObjCI(&texObj, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0, 0);
        clut = desc->CLUTHeader;
        if (!VALID_PTR(clut)) {
            return;
        }
        GXInitTlutObj(&tlutObj, clut->data, clut->format, clut->numEntries);
        GXLoadTlut(&tlutObj, 0);
    } else {
        GXInitTexObj(&texObj, hdr->data, hdr->width, hdr->height, hdr->format, 0, 0, 0);
    }
    DrawTexture(&texObj, x, y, 1, w, h);
}

// Draws a texture object as a white screen-space quad (ortho 448 x fbWidth).
void DrawTexture(GXTexObj* texobj, s16 x, s16 y, s16 z, s16 w, s16 h)
{
    GXColor color;
    s16 x2;
    s16 y2;

    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 0, 0, 0, 2);
    color.r = color.g = color.b = color.a = 255;
    GXSetChanAmbColor(0, color);
    GXSetChanMatColor(0, color);
    Mtx mtx;
    Mtx44 proj;
    PSMTXIdentity(mtx);
    GXLoadTexObj(texobj, 0);
    GXLoadTexMtxImm(mtx, 30, 1);
    GXSetTexCoordGen(0, 1, 4, 30);
    GXSetNumTexGens(1);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 3);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, (f32) Rmode.fbWidth, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXBegin(0x80, 0, 4);
    x2 = x + w;
    y2 = y + h;
    GXPosition3s16(x, y, z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3s16(x2, y, z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3s16(x2, y2, z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3s16(x, y2, z);
    GXTexCoord2f32(0.0f, 1.0f);
}

// Runs a REL's epilog and unlinks it; a failure logs and halts after 60 frames.
void DLL_Unlink(OSModuleHeader* pModule)
{
    if (pModule->epilog) {
        DLL_EPILOG(pModule)();
    }
    if (OSUnlink(&pModule->info) != 1) {
        pLog->err(0, 0, "OSUnlink failed : 0x%08x", pModule);
        TaskSleep(60);
#line 1424 "D:/Bio4/Prog/main_sub.cpp"
        HALT();
    }
    OSReport("The unlink of DLL was completed.\n");
}

// Links a REL with its bss; a failure logs and halts after 60 frames.
void DLL_Link(OSModuleHeader* pModule, void* pBss)
{
    if (OSLink(&pModule->info, pBss) != 1) {
        pLog->err(0, 0, "OSLink failed : 0x%08x", pModule);
        TaskSleep(60);
#line 1440 "D:/Bio4/Prog/main_sub.cpp"
        HALT();
    }
    OSReport("The link of DLL was completed.\n");
}
