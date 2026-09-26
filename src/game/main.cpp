// game/main: entry point, frame loop, system init and reset handling (D:/Bio4/Prog/main.cpp).
#include "types.h"
#include "global.h"
#include "joy.h"

// .sbss objects in the order the original lays them out (uninitialised globals are emitted in
// first-declaration order, header externs included: main.h's pRK must come after these).
void* pFrame_buff[2];
void* pCurrent_buff;
f32 ZNEAR;
f32 ZFAR;
f32 ORTHO_T;
f32 ORTHO_B;
f32 ORTHO_L;
f32 ORTHO_R;

#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "view.h"
#include "atari.h"
#include "light.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "card.h"
#include "event.h"
#include "sofdec.h"
#include "dvd.h"
#include "mes.h"
#include "id_sys.h"
#include "block.h"
#include "datactrl.h"
#include "em.h"
#include "obj.h"
#include "cockpit.h"
#include "room_data.h"
#include "db_log.h"
#include "scheduler.h"
#include "fade.h"
#include "cinesco.h"
#include "libgpu.h"
#include "trans_ot.h"
#include "pad.h"
#include "file.h"
#include "snd.h"
#include "dbmodule.h"
#include "cloth.h"
#include "filter.h"
#include "room_tex.h"
#include "TexRender.h"
#include "camera.h"
#include "gx_sub.h"
#include "gx.h"
#include "os_vi.h"
#include "rnd.h"
#include "sscrn.h"
#include <dolphin/vi/vifuncs.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/os/OSResetSW.h>
#include <dolphin/os/OSReset.h>
#include <dolphin/os.h>
#include <dolphin/os/OSAlarm.h>
#include <dolphin/db.h>
#include "title.h"
#include "debug.h"
#include "trans.h"
#include "shadow.h"
#include "item_model.h"
#include "read.h"
#include "eprintf.h"
#include "trans_lit.h"
#include "eff_sys.h"
#include "etc_model.h"
#include "exception.h"
#include "espgen.h"

extern "C" {
void __main();
}
void AllocDrawTmpBuf();   // game/TmpBuf.cpp
void DbmenuModuleInit();  // game/db_menu.cpp

// Stores through a scalar reference are not struct-member MEMs, so GCC 2.95 assumes they may
// alias pG/pSys/pRK and reloads the pointer after each one, as the original does.
// Word store at a byte offset from a member array: `*(u32*) ((u8*) base + ofs)` is an
// INDIRECT_REF of a cast (not of a PLUS_EXPR), so the MEM is not in-struct either.
static inline void U32SetOfs(void* base, int ofs, u32 v) { *(u32*) ((u8*) base + ofs) = v; }
static inline u32 U32GetOfs(void* base, int ofs) { return *(u32*) ((u8*) base + ofs); }

#line 40 "D:/Bio4/Prog/main.cpp"

const Vec vecZero = {0.0f, 0.0f, 0.0f};

GlobalWork Global;
SYSTEM_SAVE_WORK SystemSave;
JOY Joy[4];
KeyWork Key;
u32 MainOt[5];
ScreenInfo Screen;

GlobalWork* pG = &Global;
SYSTEM_SAVE_WORK* pSys = &SystemSave;
int vsync_cnt = 0;

RESET_KEEP_WORK* pRK;
char* pUser_name;
void* roomInfoAddr;
s8 system_vcnt;

static void systemScreenInit();

// Program entry: systemStartInit (hardware/OS/GX/sound/memory), then per (soft) reset
// systemRestartInit, restores the reset-keep block (pRK: system flags, language, unlock flags,
// mercenaries records), starts the title task and runs the frame loop forever: pad read, debug,
// render set-up, TaskScheduler (game logic), id sprites, Trans (3D draw), DVD/sound watchers,
// fades, messages, cinescope, the 2D OT, log flush, interrupt tasks, render done, vsync wait
// (GetSystemVcnt frames), buffer swap and systemResetCheck (soft reset -> RESTART).
int main()
{
    int i;
    int j;
    int ret;

    systemStartInit();
RESTART:
    {
        systemRestartInit();
        if (pRK->reset_flag) {
            pSys->Config_flg = pRK->Config_flg;
            pSys->language = pRK->language;
            pSys->eff_country = pRK->eff_country;
            pG->game_country = pRK->game_country;
            pSys->Extra_flg = pRK->Extra_flg;
            for (i = 0; i < 16; i += 4) {
                U32SetOfs(pSys->MercSysRoom, i, U32GetOfs(pRK->MercSysRoom, i));
            }
            for (j = 0; j < 2; j++) {
                U32SetOfs(pSys->MercSysRank, j * 4, pRK->MercSysRank[j]);
            }
            if (FlagChkSignW(pRK->System_flg, SYS_OMAKE_ADA_GAME)) {
                SysFlagOn(pG, SYS_OMAKE_ADA_GAME);
            }
            if (FlagChk(&pRK->System_flg, SYS_OMAKE_ETC_GAME)) {
                SysFlagOn(pG, SYS_OMAKE_ETC_GAME);
            }
        }
        ret = 0;
        if (SysFlagChk(pG, SYS_PUBLICITY_VER)) {
            pG->room_id = 0x120;
            pG->JumpPoint = 0;
            pSys->language = 1;
            pG->debug_mode = 0;
            pG->pl_type = 0;
            DbgFlagOff(pG, DBG_FOG_FAR_GREEN);
        }
        TaskExec(0, Title_task, 0);
        for (;;) {
            StopwatchInit();
            ProcessTickInit();
            Render_before();
            ClearOTagR(MainOt, 5);
            PadRead();
            DebugControl();
            Render();
            ProcessTickGet(4, "RENDER SETUP");
            SetPrimBuffPtr();
            ClearOt();
            pG->Frame_cnt++;
            TaskScheduler();
            ProcessTickGet(5, "TaskScheduler");
            if (!SysFlagChk(pG, SYS_TRANS_STOP) || (StaFlagChk(pG, STA_SUB_SCRN))) {
                IdSys.move();
            }
            if (!SysFlagChk(pG, SYS_TRANS_STOP) || (StaFlagChk(pG, STA_SUB_SCRN))) {
                IdSys.trans();
            }
            if (!SysFlagChk(pG, SYS_TRANS_STOP)) {
                Trans();
            }
            Dvd.Watcher();
            SndWatcher();
            ProcessTickGet(5, "SndWatcher");
            FadeControl(0);
            cMes.Move();
            cMes.Trans();
            CinescoMove();
            if (!DbgFlagChk(pG, DBG_CINESCO_OFF)) {
                Draw_cinesco();
            }
            FadeControl(1);
            DrawOTag(&MainOt[4]);
            if (pG->debug_mode != 7) {
                pLog->disp();
            }
            EprintfFlush();
            ProcessTickGet(2, "PROCESS CPU");
            iTaskScheduler();
            Render_done();
            Block.checkCommand();
            DC.check();
            Block.checkCondition();
            ProcessTickGet(3, "DRAW REMAIN");
            while (vsync_cnt < GetSystemVcnt() - 1) {}
            Render_swap();
            PPCSync();
            while (vsync_cnt < GetSystemVcnt()) {}
            vsync_cnt = 0;
            systemVSyncPost();
            SysFlagOff(pG, SYS_RENDER_END);
            ProcessTickGet(0, "PROCESS TOTAL");
            ret = systemResetCheck();
            if (ret == 1) {
                goto RESTART;
            }
        }
    }
    OSPanic(__FILE__, __LINE__, "End of biohazard4");
    return 0;
}

// Hook after the vsync wait; empty.
void systemVSyncPost()
{
}

// VI post-retrace callback: counts vsyncs, resumes the interrupt task scheduler once the frame's
// vsync count is reached, and checks for a hang.
void postVSyncCallback()
{
    vsync_cnt++;
    if (vsync_cnt >= GetSystemVcnt()) {
        iTaskSuspend();
    }
    if (!SysFlagChk(pG, SYS_EXCEPTION)) {
        haltExecCheck();
    }
}

// Halts the game (write to 0x11111111) when no frame was presented for 3600 vsyncs and no
// debugger is attached (hang detector).
void haltExecCheck()
{
    int dbg = DBIsDebuggerPresent();
    if (dbg == 0 && vsync_cnt > 3599) {
#line 548 "D:/Bio4/Prog/main.cpp"
        HALT();
    }
}

// Once at boot: clears the global work, OS/font/language, dev-mode host file system, exception
// handler, VI (black), DVD, memory card, performance counters, heaps, task scheduler, screen
// (Render_init), lights, RNG, OT, sound, Sofdec, id system (0x80 units), log, debug module, DVD
// size table and messages.
void systemStartInit()
{
    memclr_asm(pG, sizeof(GlobalWork));
    OSInit();
    setLanguage();
    RomFontSetting();
    if (OSGetConsoleType() & 0xF0000000) {
        pG->IsDevConsole = 1;
    }
    if (pG->IsDevConsole == 1) {
        InitFile();
        if (file_path("d:\\bio4") == 0) {
            file_path("c:/");
        }
    }
    ExceptionInit();
    OSInitAlarm();
    VIInit();
    systemVISetBlack(1);
    LCEnable();
    VIWaitForRetrace();
    VISetPostRetraceCallback(postVSyncCallback);
    Dvd.Init();
    CardInit();
    asm("li 3, 4\n"
        "oris 3, 3, 4\n"
        "mtspr 914, 3\n"
        "li 3, 5\n"
        "oris 3, 3, 5\n"
        "mtspr 915, 3\n"
        "li 3, 6\n"
        "oris 3, 3, 6\n"
        "mtspr 916, 3\n"
        "li 3, 7\n"
        "oris 3, 3, 7\n"
        "mtspr 917, 3"
        :
        :
        : "r3");
    SystemMemInit();
    if (pG->IsDevConsole == 1) {
        CardDbgCacheSet();
    }
    TaskSchedulerInit();
    systemScreenInit();
    Render_init();
    LightSetInit();
    RndInit(0xD37);
    InitOt();
    SndInit();
    SofdecInit();
    IdSys.gameInit(0x80);
    IdTexGameInit();
    EprintfInit();
    LogInit();
    init_dbmodule();
    Dvd.SizeTableRead();
    cMes.init();
}

// At boot and after every soft reset: pad, clears the game part of the global work, heap 1
// (game), the OT/fade/cinescope/id/message/data cache/temp buffers, room inits of every manager,
// filters, cloth, cockpit, room data, tex render, item/etc models, events, camera, view, sound,
// background colour, config, brightness from the reset-keep block, debug/roomInfo.dat, log mode.
void systemRestartInit()
{
    int i;
    int ret;

    PadInit();
    memclr_asm(&pG->Rno0, sizeof(GlobalWork) - 0x20);
    MemReplaceHeap(0, 1);
    MemSetCurrentHeap(1);
    systemWorkInit();
    InitOt();
    FadeInit();
    CinescoInit();
    IdSys.roomInit();
    {
        cMes.Clear();
    }
    DC.init();
    AllocDrawTmpBuf();
    EmMgr.roomInit();
    ObjMgr.roomInit();
    LightMgr.init(0);
    EspInit();
    RoomTexInit();
    EspgenInit();
    CtrlMgr.roomInit();
    ShadowInit();
    FilterInit();
    ClothInit();
    Cckpt.gameInit();
    RoomData.init();
    TexRenderMgrInit();
    ItemModelInit();
    EtcModelInit();
    EvtMgr.init();
    CameraGameInit();
    View.gameInit(&pG->Camera);
    SndInit2();
    bio4_GXSetCopyClear(g_sysBgColor, 0xFFFFFF);
    GXCopyDisp(pCurrent_buff, 1);
    ConfigSet();
    if (DBIsDebuggerPresent() == 0) {
        SysFlagOff(pG, SYS_SN_PC_READ);
        SysFlagOff(pG, SYS_SN_PC_READ_TOOL);
    }
    if (pRK->base_brightness == 0) {
        pRK->base_brightness = 0x40;
    }
    pSys->brightness = pRK->base_brightness;
#line 752 "D:/Bio4/Prog/main.cpp"
    ret = DvdReadN("debug/roomInfo.dat", 0, 0, 0, 0, 5, __FILE__, __LINE__);
    if (Dvd.ReadCheck(ret, 0, 0, &roomInfoAddr) < 0) {
        roomInfoAddr = 0;
    }
    pLog->modeSet(200, 0, 150, 4);
    SelfScreenShotInit();
}

// Default projection constants (ZNEAR 100, ZFAR 1,000,000, 640x480 ortho), 2 vsyncs per frame,
// Screen = the render mode size and default brightness.
static void systemScreenInit()
{
    GXRenderModeObj* rm = &Rmode;

    ZNEAR = 100.0f;
    ZFAR = 1000000.0f;
    ORTHO_T = 240.0f;
    ORTHO_B = -240.0f;
    ORTHO_L = -320.0f;
    ORTHO_R = 320.0f;
    SetSystemVcnt(2);
    Screen.x = 0.0f;
    Screen.y = 0.0f;
    Screen.width = (f32) rm->fbWidth;
    Screen.height = (f32) rm->efbHeight;
    pSys->brightness = 0x40;
    OSReport("width = %d\n", rm->fbWidth);
    OSReport("height = %d\n", rm->efbHeight);
}

// Default global state: debug mode, start room r120 (Part 0), Leon, game mode 5, costumes 0,
// user name buffer and language 1.
void systemWorkInit()
{
    systemScreenInit();
    pG->debug_mode = 1;
    pG->debug_disp = -1;
    pG->room_id = 0x120;
    pG->RoomNo_next = pG->room_id;
    pG->pl_type = 0;
    pG->game_mode = 5;
    pG->game_costume = 0;
    pG->pl_costume = 0;
#line 823 "D:/Bio4/Prog/main.cpp"
    pUser_name = (char*) mem_calloc(0x40, __FILE__, __LINE__, 1, 13);
    pSys->language = 1;
    pSys->eff_country = 1;
    pG->game_country = 1;
}

// Frames per game update in vsyncs (1 = 60 Hz, 2 = 30 Hz).
void SetSystemVcnt(int v_cnt)
{
    if (v_cnt > 0) {
        system_vcnt = v_cnt;
    } else {
        system_vcnt = 1;
    }
}

// Current vsyncs per frame.
int GetSystemVcnt()
{
    return system_vcnt;
}

// Reset button handling: on release sets System_flg 0x8000 and, when the game allows it (no 0x200
// lock), performs the hard reset (systemHardReset + OSResetSystem). Returns 1 when resetting.
int checkHardReset()
{
    static u8 reset_check = 0;

    if (reset_check == 0) {
        if (OSGetResetButtonState() == 1) {
            reset_check = 1;
        }
    } else {
        if (OSGetResetButtonState() == 0) {
            reset_check = 0;
            SysFlagOn(pG, SYS_HARD_RESET);
        }
    }
    if (SysFlagChk(pG, SYS_HARD_RESET)) {
        if (!SysFlagChk(pG, SYS_CARD_ACCESS)) {
            systemHardReset();
            PADRecalibrate(0xF0000000);
            OSResetSystem(0, 0, 0);
            return 1;
        }
    }
    return 0;
}

// End of frame: the soft-reset combo (B+X+START held 30 frames) sets 0x4000000; then hard reset
// check, and a pending soft reset (unless locked by 0x200) runs systemSoftReset and returns 1.
int systemResetCheck()
{
    static u8 Soft_reset_cnt = 0;

    if ((Joy[0].on & 0x1600) == 0x1600) {
        Soft_reset_cnt += GetSystemVcnt();
        if (Soft_reset_cnt > 30) {
            if (pG->IsDevConsole == 1) {
                SysFlagOn(pG, SYS_SOFT_RESET);
            } else {
                SysFlagOn(pG, SYS_HARD_RESET);
            }
        }
    } else {
        Soft_reset_cnt = 0;
    }
    checkHardReset();
    if (!SysFlagChk(pG, SYS_HARD_RESET)) {
        if (SysFlagChk(pG, SYS_SOFT_RESET)) {
            if (!SysFlagChk(pG, SYS_CARD_ACCESS)) {
                systemSoftReset();
                return 1;
            }
        }
    }
    return 0;
}

// Shared reset work: closes the sub screen, clears tasks, releases enemy/player/weapon/room
// modules, and saves system flags/language/unlock/mercenaries/card state into the reset-keep block.
void systemResetCommon()
{
    int i;

    SubScreenExitCore(&SubScreenWk);
    TaskAllClear();
    EmReadInit();
    ReleasePlData();
    ReleaseWepData();
    RoomData.stopRelData();
    pRK->Config_flg = pSys->Config_flg;
    pRK->language = pSys->language;
    pRK->eff_country = pSys->eff_country;
    pRK->game_country = pG->game_country;
    pRK->Extra_flg = pSys->Extra_flg;
    pRK->System_flg = pG->System_flg;
    for (i = 0; i < 4; i++) {
        U32SetOfs(pRK->MercSysRoom, i * 4, pSys->MercSysRoom[i]);
    }
    for (i = 0; i < 2; i++) {
        U32SetOfs(pRK->MercSysRank, i * 4, pSys->MercSysRank[i]);
    }
    pRK->MemcardCheckDone = pG->CardStatus >> 31;
    pRK->reset_flag = 1;
}

// Blacks the screen and runs the common reset (the OS then reboots).
void systemHardReset()
{
    OSReport("--HARD_RESET START!!\n");
    systemVISetBlack(1);
    VIFlush();
    VIWaitForRetrace();
    systemResetCommon();
    OSReport("--HARD_RESET END!!\n");
}

// Blacks the screen, cancels DVD/ARAM transfers, resets sound, debug allocations and all heaps,
// runs the common reset and re-inits the ROM font (the frame loop then restarts).
void systemSoftReset()
{
    OSReport("--SOFT_RESET START!!\n");
    systemVISetBlack(1);
    VIFlush();
    VIWaitForRetrace();
    GXDrawDone();
    Dvd.ReadCancelAll();
    Aram.DmaCancelAll();
    SndSoftReset();
    DbmenuModuleInit();
    ResetDebugAlloc();
    MemClearAllHeap();
    SndSystemReset();
    systemResetCommon();
    RomFontSetting();
    pG->IsMessageInit = 0;
    OSReport("--SOFT_RESET END!!\n");
}

// Fixed language 1 / region 1 for this build.
void setLanguage()
{
    pSys->language = 1;
    pSys->eff_country = pSys->language;
    pG->game_country = pSys->language;
}
