// game/game: the game task (init / stage / room / main loop / door / ending / option steps), the
// save data front end (cGameSave), the died demo, difficulty points, the primitive buffer and
// the debug displays (D:/Bio4/Prog/game.cpp).
#include "types.h"
#include "light.h"
#include "atari.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "dmg.h"
#include "event.h"
#include "card.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "joy.h"
#include "game.h"
#include "em.h"
#include "obj.h"
#include "model.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "mes.h"
#include "cockpit.h"
#include "id_sys.h"
#include "option.h"
#include "scheduler.h"
#include "fade.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "datactrl.h"
#include "snd.h"
#include "dbmodule.h"
#include "scroll.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "merchant.h"
#include "sscrn.h"
#include "item.h"
#include "item_model.h"
#include "view.h"
#include "block.h"
#include "db_work.h"
#include "act_btn.h"
#include "dvd.h"
#include "room_data.h"
#include "est.h"
#include "stage.h"
#include "shadow.h"
#include "room_tex.h"
#include "esp.h"
#include "espgen.h"
#include "flr_at.h"
#include "filter.h"
#include "TexRender.h"
#include "cloth.h"
#include "trans_ot.h"
#include "debug.h"
#include "eprintf.h"
#include "pad.h"
#include "route_ck.h"
#include "math_sub.h"
#include "motion.h"
#include "em_set.h"
#include "db_log.h"
#include "gx.h"
#include <dolphin/os.h>
#include "read.h"
#include "trans.h"
#include "cons.h"
#include "eff_sys.h"
#include "etc_model.h"
#include "light_area.h"

// game/read.cpp (C++ linkage)
void* GetDataExt(void* arc, const char* tag, int no);
// game/cons.cpp (C++ linkage)
struct ConsRoom;
int ConsInitRoom(ConsRoom* p);
// game/db_menu.cpp (C++ linkage)
void DbMenuExec();
void DbMenuRoomInit();

// Stores through a scalar reference: not struct-member MEMs, so GCC 2.95 assumes they may alias
// pG and reloads it afterwards, as the original does after every GlobalWork store.
// One flag test per call: fold would merge `(f & A) || (f & B)` on one lvalue into a single mask.
static inline u32 Flag54(u32 b) { return pG->System_flg & b; }

union FadeColor {
    GXColor c;
    u32 w;
};

// FadeSet with the black/clear pair: sign bit set = fade from black to clear (fade in), clear = fade to black.
// game.cpp variant of FadeSetW: `black` is set before the start choice, so its `li` leads.
static inline void fadeSetG(int no, u32 time, u32 z, int late)
{
    FadeColorPair col;
    u32 black;

    black = 0xFF;
    if (no & 0x80000000) {
        *(u32*) &col.start = black;
    } else {
        *(u32*) &col.start = 0;
    }
    if (no & 0x80000000) {
        *(u32*) &col.end = 0;
    } else {
        *(u32*) &col.end = black;
    }
    FadeSet(no, &col.start, &col.end, time, z, late);
}

// Option archive (pG->pOptionData): offsets to the died demo id data.
struct OptionArc {
    u32 x0;
    u32 x4;
    u32 x8;
    u32 xC;
    u32 ofs_10;   // 0x10  died demo id textures
    u32 ofs_14;   // 0x14  "you are dead" id data
    u32 ofs_18;   // 0x18  continue / reset menu id data
    u32 ofs_1C;   // 0x1C  "you are dead" id data with the sub character alive
};

// Game task work (`Game`).
struct GAME_WORK {
    u32 Rno_bak;     // 0x00  pG->mode32 saved while the option screen runs
    u32 Map_addr;    // 0x04
    u32 Map_size;    // 0x08
    u32 Option_addr; // 0x0C
    u32 Option_size; // 0x10
    u32 Swap_addr;   // 0x14
    void* omake_wep_addr;      // 0x18  0xE8-byte buffer of the extra game modes (gameInit)
};

#line 40 "D:/Bio4/Prog/game.cpp"

int lbl_80314B90 = 0;

GAME_WORK Game;
u32 g_at_cnt[20];
u32 g_at_cyc[20];
u32 g_at2_cnt[20];
u32 g_at2_cyc[20];

extern "C" {
void DoorFlagInit();
void gameInit();
void gameStageInit();
void gameRoomInit();
void gameMainLoop();
void clearGlobalSaveData();
void gameEnding();
void gameOption();
void gameDoordemo();
void gameRoomMemInit();
void gameStopMove();
void gameDebugDisp();
void gameDebug();
}

SAVE_DATA_HEAD* pSaveData;
cGameSave GameSave;
cDbWork* DbWork;
static u32 g_at_total;
static u32 g_at_total_cyc;
u32 g_at2_total;
u32 g_at2_total_cyc;
DiedemoWork diedemo_work;

void (*LightFuncTbl[17])(cLight*) = {
    Light00_Move, Light01_Move, Light02_Move, Light03_Move, Light04_Move, Light05_Move,
    Light06_Move, Light07_Move, Light08_Move, Light00_Move, Light00_Move, Light00_Move,
    Light00_Move, Light00_Move, Light00_Move, Light00_Move, Light10_Move,
};

// Effect collision hit effects (EatMgr.registEffInfo): water (type 2) and the normal types 4..7.
static const AtEffInfo effInfoWater = {
    1, {0, 0x3A}, {0, 0x15}, {0, 0x19}, {0, 0xA}, {0, 0x14}, {0, 0x14}, {0, 0x39}, {0, 0x15},
};
static const AtEffInfo effInfoNormal = {
    0, {0xD2, 0}, {0, 0xD}, {0, 0xB}, {0, 0xC}, {0, 0x1F}, {0, 0x1F}, {0, 0x36}, {0, 0xD},
};
// Room water effect table of the player (PlRegistRoomEff).
static const PlRoomEff effRoom[6] = {
    {1, {0, 0, 0}, 0x21}, {1, {0, 0, 0}, 0x22}, {1, {0, 0, 0}, 0x23},
    {1, {0, 0, 0}, 0x21}, {1, {0, 0, 0}, 0x22}, {1, {0, 0, 0}, 0x23},
};

// New game: presets the door state flags (Scenario_flg[3]/51CC/51D0) of the doors that start
// locked/opened for the scenario.
void DoorFlagInit()
{
    ScfFlagOn(pG, SCF_96);
    ScfFlagOn(pG, SCF_98);
    ScfFlagOn(pG, SCF_9a);
    ScfFlagOn(pG, SCF_9b);
    ScfFlagOn(pG, SCF_9d);
    ScfFlagOn(pG, SCF_a3);
    ScfFlagOn(pG, SCF_a3);
    ScfFlagOn(pG, SCF_72);
    ScfFlagOn(pG, SCF_7a);
    ScfFlagOn(pG, SCF_7f);
    ScfFlagOn(pG, SCF_80);
    ScfFlagOn(pG, SCF_84);
    ScfFlagOn(pG, SCF_89);
}

// The game task (TaskExec'd by main): loops forever running game_func_tbl[pG->Rno0] once per frame
// (0 gameInit, 1 gameStageInit, 2 gameRoomInit, 3 gameMainLoop, 4 gameDoordemo, 5 gameEnding,
// 6 gameOption) after gameDebug and the play-time update.
void GameTask()
{
    static void (*game_func_tbl[7])() = {
        gameInit, gameStageInit, gameRoomInit, gameMainLoop, gameDoordemo, gameEnding, gameOption,
    };
    u32 h;
    u32 m;
    u32 s;

    pG->Rno0 = 0;
    pG->Rno1 = 0;
    pG->Rno2 = 0;
    pG->Rno3 = 0;
    for (;;) {
        gameDebug();
        GetGameTime(&h, &m, &s);
        eprintf(20, 16, 7, 0, "%d:%02d:%02d %08X", h, m, s, Joy[0].on);
        game_func_tbl[pG->Rno0]();
        TaskSleep(1);
    }
}

// Rno0 == 0: game start. Inits cloth, messages, sub screen, cockpit, lights, scenario, player, items
// (System_flg 0x2000 = new game), merchant and play time; System_flg 0x100 (continue/load) loads
// the save; sets the difficulty points and door flags, then Rno0 = 1.
void gameInit()
{
    ClothInit();
    {
        FadeColor c;
        c.w = 0;
        GXSetCopyClear(c.c, 0xFFFFFF);
    }
    if (pG->game_mode == 6) {
        SysFlagOn(pG, SYS_HARD_MODE);
    }
    if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME) || pG->pl_type == 4) {
#line 232 "D:/Bio4/Prog/game.cpp"
        Game.omake_wep_addr = MEM_ALLOC(0x70000, 1, 13);
    }
    if (pG->game_mode == 0) {
        pG->game_mode = 5;
    }
    cMes.gameInit();
    SubScreenGameInit();
    Cckpt.gameInit();
    ObjMgr.warnDiv = 100;
    LightMgr.init(LightFuncTbl);
    LightMgr.initPath((LightPathHeader*) (pG->pCore->ofs_3C + (u32) pG->pCore));
    ScenarioInit();
    PlayerInit();
    pG->ashley_life = 600;
    if (SysFlagChk(pG, SYS_NEW_GAME)) {
        ItemMgr.gameInit();
        SceAtInitSaveItem();
    }
    SysFlagOff(pG, SYS_DOOR_AFTER);
    MerchantGameInit();
    pG->Speed = 1.0f;
    InitGameTime();
    if (SysFlagChk(pG, SYS_LOAD_GAME)) {
        GameLoad();
    }
    if (FlagChkSignW(pG->System_flg, SYS_OMAKE_ADA_GAME) || (SysFlagChk(pG, SYS_OMAKE_ETC_GAME))) {
        pG->game_mode = 5;
    }
    if (SysFlagChk(pG, SYS_NEW_GAME) || pG->SaveKind == 3) {
        GamePointInit(0);
        DoorFlagInit();
    }
    systemVISetBlack(0);
    pG->Rno0 = 1;
}

// Rno0 == 1: stage/room entry. Marks continue mode (System_flg 0x80), offers the Ashley costume
// choice at r120 on a new game (unlocked extras), swaps to the disc of the stage (disc 2 from stage
// 3 on, except r22c), saves the game (GameSave.save) when allowed and starts the room load
// (StageSet); Rno0 = 2.
void gameStageInit()
{
    pLog->warn(1, 0, "-- R%03x ----------", pG->room_id);
    if (Flag54(0x80000) || Flag54(0x100)) {
        SysFlagOn(pG, SYS_CONTINUE_AFTER);
    } else {
        SysFlagOff(pG, SYS_CONTINUE_AFTER);
    }
    if (ExtFlagChk(pSys, EXT_COSTUME)) {
        if (!SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
            if (!SysFlagChk(pG, SYS_OMAKE_ETC_GAME) && pG->room_id == 0x120 &&
                (SysFlagChk(pG, SYS_NEW_GAME) || pG->SaveKind == 3)) {
                int res;

                DpfFlagOff(pG, DPF_MESSAGE);
                cMes.setLayout(0, 0);
                cMes.MesSet(150, 100, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
                if ((res = cMes.GetSelectMessage(0)) == 0) {
                    do {
                        TaskSleep(1);
                    } while ((res = cMes.GetSelectMessage(0)) == 0);
                }
                switch (res) {
                case 1:
                default:
                    pG->game_costume = 1;
                    break;
                case 2:
                    pG->game_costume = 0;
                    break;
                }
                PlSetCostume();
            }
        }
    }
    if (!DbgFlagChk(pG, DBG_SINGLE_DISK)) {
        switch (pG->stage_no) {
        case 0:
            break;
        case 1:
        case 2:
            if (pG->room_id != 0x22C && Dvd.GetDiscNo() == 1) {
                Dvd.DiscChange(0);
            }
            break;
        case 3:
            if (Dvd.GetDiscNo() == 0) {
                Dvd.DiscChange(1);
            }
            break;
        }
    }
    SetGameTime();
    if (!StaFlagChk(pG, STA_SAVEDATA_NO_UPDATE) && !SysFlagChk(pG, SYS_LOAD_GAME)) {
        GameSave.save(pSaveData, -1);
    }
    StageSet();
    pG->Rno0 = 2;
}

// Rno0 == 2: room set-up after the room archive is loaded: player/area data, every manager's room
// init + array allocation sized by the room "CNS" counts (models, parts, enemies, objects, sprites,
// controllers, ctrl, lights, damage, SAT/EAT collision, events), the room data blocks (SMD/SMX
// scroll objects, LIT lights, SHD shadows, EFF effects, EAR/SAR areas, TEX/ITM/ETM models, CAM,
// BLK, EVS, FSE, AEV/ITA scenario collision), the room SST effects, BGM, then the fade-in and
// Rno0 = 3.
void gameRoomInit()
{
    int n;
    void* p;

    DC.setNBlkStop(1);
    SndReadAddrInit();
    gameRoomMemInit();
    if (pG->shooting_mode != 0) {
        pG->debug_mode = 0;
        DbgFlagOn(pG, DBG_LOG_OFF);
        DbgFlagOn(pG, DBG_NO_DEATH2);
        DbgFlagOn(pG, DBG_INF_BULLET2);
        DbgFlagOn(pG, DBG_BGM_STOP);
        DbgFlagOn(pG, DBG_NO_PARASITE);
        DbgFlagOff(pG, DBG_FOG_FAR_GREEN);
    }
    DbgFlagOn(pG, DBG_WIND_ON);
    ReadPlayerData(pG->pl_type, pG->pl_costume);
    ReadAreaData();
    DC.initDataUnit();
    ActBtn.init();
    ConsInitRoom((ConsRoom*) GetDataExt(pG->pRoom, "CNS", 0));
    {
        cSmd* smd = (cSmd*) GetDataExt(pG->pRoom, "SMD", 0);
        cSmx* smx = (cSmx*) GetDataExt(pG->pRoom, "SMX", 0);
        SmdInit(smd, smx, (cSmd*) GetDataExt(pG->pRoom, "SMD", 1));
    }
    ModInfoMgr.roomInit();
    n = ConsGetRoomValue(CONS_R_NMODELINFO) + SmdGetObjNum();
    if (DbgFlagChk(pG, DBG_APP_USE_DBMEM)) {
        n *= 2;
    }
    ModInfoMgr.arrayAlloc(n);
    PartsMgr.roomInit();
    n = ConsGetRoomValue(CONS_R_NPARTS) + SmdGetObjNum();
    if (DbgFlagChk(pG, DBG_APP_USE_DBMEM)) {
        n *= 2;
    }
    PartsMgr.arrayAlloc(n);
    EmMgr.roomInit();
    n = ConsGetRoomValue(CONS_R_NEM);
    if (DbgFlagChk(pG, DBG_APP_USE_DBMEM)) {
        n *= 2;
    }
    EmMgr.arrayAlloc(n);
    ObjMgr.roomInit();
    n = ConsGetRoomValue(CONS_R_NOBJ) + SmdGetObjNum();
    if (DbgFlagChk(pG, DBG_APP_USE_DBMEM)) {
        n *= 2;
    }
    ObjMgr.arrayAlloc(n);
    EspRoomInit();
    EspArrayAlloc(ConsGetRoomValue(CONS_R_NESP));
    RoomTexRoomInit();
    EspgenRoomInit();
    EspgenArrayAlloc(ConsGetRoomValue(CONS_R_NESPGEN));
    CtrlMgr.roomInit();
    CtrlMgr.arrayAlloc(ConsGetRoomValue(CONS_R_NCTRL));
    LightMgr.roomInit((cLit*) (pG->pCore->ofs_2C + (u32) pG->pCore), (cLit*) GetDataExt(pG->pRoom, "LIT", 0),
                      (cLit*) GetDataExt(pG->pRoom, "LIT", 1));
    LightMgr.arrayAlloc(ConsGetRoomValue(CONS_R_NLIGHT));
    LightMgr.initPath((LightPathHeader*) (pG->pCore->ofs_3C + (u32) pG->pCore));
    ShadowRoomInit();
    DmgMgr.roomInit();
    DmgMgr.arrayAlloc(20);
    fadeSetG(2, 0, 0, 0);
    FilterRoomInit();
    TexRenderMgrRoomInit();
    ItemModelRoomInit();
    EtcModelRoomInit();
    LightAreaInit();
    if (SysFlagChk(pG, SYS_DOORDEMO)) {
        pG->nPrim = 0x8000;
    } else {
        pG->nPrim = ConsGetRoomValue(CONS_R_NPRIM);
    }
    primInit();
    {
        Vec pos;
        Vec rot;

        p = GetDataExt(pG->pRoom, "SAT", 0);
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        SatMgr.roomInit();
        SatMgr.arrayAlloc(ConsGetRoomValue(CONS_R_NSAT));
        SatMgr.create(p, 0, &pos, &rot, 0);
        p = GetDataExt(pG->pRoom, "EAT", 0);
        EatMgr.roomInit();
        EatMgr.arrayAlloc(ConsGetRoomValue(CONS_R_NEAT));
        EatMgr.create(p, 0, &pos, &rot, 0);
        SatMgr.type = 0;
        EatMgr.type = 1;
        EatMgr.initEffInfo();
        EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &effInfoWater);
        EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &effInfoNormal);
        EatMgr.registEffInfo(EAT_ET_ROOM1, (AtEffInfo*) &effInfoNormal);
        EatMgr.registEffInfo(EAT_ET_ROOM2, (AtEffInfo*) &effInfoNormal);
        EatMgr.registEffInfo(EAT_ET_ROOM3, (AtEffInfo*) &effInfoNormal);
    }
    SceAtInit(GetDataExt(pG->pRoom, "AEV", 0), GetDataExt(pG->pRoom, "ITA", 0));
    EvtMgr.roomInit();
    EvtMgr.arrayAlloc(2);
    EvtMgr.myRoomInit();
    EvtDebug.myRoomInit();
    if (!SysFlagChk(pG, SYS_DOORDEMO)) {
        EmMgr.create(0, 0);
        PlRegistRoomEff((PlRoomEff*) effRoom);
    }
    SmdSetup(-1);
    ShdInit((ShdHeader*) GetDataExt(pG->pRoom, "SHD", 0));
    if ((p = GetDataExt(pG->pRoom, "EFF", 0)) != 0) {
        EspDataLoad((u32) p, EFF_ROOM, 0);
    }
    if ((p = GetDataExt(pG->pRoom, "EAR", 0)) != 0) {
        EffAreaDataLoad((SstArea*) p);
    }
    if ((p = GetDataExt(pG->pRoom, "SAR", 0)) != 0) {
        LightAreaDataLoad((LightAreaHed*) p);
    }
    if ((p = GetDataExt(pG->pRoom, "TEX", 0)) != 0) {
        RoomTexDataLoad((TexData*) p, 2);
    }
    if ((p = GetDataExt(pG->pRoom, "ITM", 0)) != 0) {
        ItemModelDataLoad(p);
    }
    if ((p = GetDataExt(pG->pRoom, "ETM", 0)) != 0) {
        EtcModelDataLoad(p);
    }
    ClothRoomInit();
    if ((p = GetDataExt(pG->pRoom, "ETS", 0)) != 0) {
        EtcModelListSet((EtcList*) p);
    }
    LightMgr.update(0, -1);
    FlrAtInit();
    SeAtInit();
    CameraRoomInit();
    p = GetDataExt(pG->pRoom, "CAM", 0);
    if (p != 0) {
        CamCtrl.RoomDataRead((CameraDataHeader*) p);
    } else {
        pG->pCamRoom = p;
    }
    CamCtrl.CoreDataRead((CameraDataHeader*) (pG->pCore->ofs_30 + (u32) pG->pCore));
    CamCtrl.roomInit();
    View.roomInit();
    p = GetDataExt(pG->pRoom, "BLK", 0);
    Block.roomInit(p);
    if ((p = GetDataExt(pG->pRoom, "EVS", 0)) != 0) {
        EvtMgr.SetEvs(p);
    }
    EmSetRoomInit();
    RoomData.initRoomSet();
    SndRoomStartInit();
    ItemMgr.roomInit();
    SubScreenRoomInit();
    IdSys.roomInit();
    Cckpt.roomInit();
    if (pG->shooting_mode == 0) {
        LightMgr.setItemLight();
    }
    DbMenuRoomInit();
    SstSet(EFF_ROOM, 0xFFFF, ESP_CORE_KIND_SST, 0, 0x2F, 1);
    cMes.roomInit();
    SndRoomBgmLoad();
    DbWork = new cDbWork;
    pG->Disp_flg = 0;
    if (SysFlagChk(pG, SYS_DOORDEMO)) {
        DpfFlagOn(pG, DPF_PL);
        SpfFlagOn(pG, SPF_PL);
        SpfFlagOn(pG, SPF_SCE_AT);
    }
    if (DbgFlagChk(pG, DBG_SCISSOR_OFF)) {
        SysFlagOff(pG, SYS_SCISSOR_ON);
    } else {
        SysFlagOn(pG, SYS_SCISSOR_ON);
    }
    MerchantRoomInit();
    fadeSetG(0x80000001, 0, 0, 0);
    ScenarioRoomInit();
    SndRoomBgmStartCheck(0);
    SndRoomStrStartCheck();
    RoomData.setPassed(pG->room_id, pG->Part);
    if (!Flag54(0x2000) && !Flag54(0x100) && !Flag54(0x80000)) {
        DoorSeCall(1);
    }
    SysFlagOff(pG, SYS_NEW_GAME);
    SysFlagOff(pG, SYS_LOAD_GAME);
    SysFlagOff(pG, SYS_CONTINUE);
    SysFlagOff(pG, SYS_DOOR_AFTER);
    DbgFlagOff(pG, DBG_ROOMJMP);
    Block.check(0);
    Filter09SetbUse(0, 1);
    fadeSetG(0x80000002, 0, 0, 0);
    fadeSetG(0x80000000, 20, 0, 0);
    SubScreenWait(15);
    SysFlagOff(pG, SYS_TRANS_STOP);
    DC.setNBlkStop(0);
    pG->Rno0 = 3;
    pG->SaveKind = 0;
}

// Rno0 == 3: one frame of play. Order: stop-mode keys, difficulty update, died-demo check,
// scenario collision + action button, ScenarioMove, EmMgr.move (every other_slow-th frame during
// the weapon zoom slow-motion), player move, ObjMgr/CtrlMgr, camera, effect areas/controllers/
// sprites, lights, damage, debug displays, light areas, cockpit, sub screen; the Z (Key 0x2000)
// button opens the option screen (Rno0 = 6) when OptionOpenCheck allows.
void gameMainLoop()
{
    static int other_slow = 3;
    static int preb_slow_flg = 1;
    static f32 player_Seq_speed = 0.6f;
    int slow;
    int nObj;

    StaFlagOff(pG, STA_CUT_CHANGE);
    UpdateNearClipDist();
    gameStopMove();
    GameAddPoint(0);
    pG->quake_ofs.x = 0.0f;
    pG->quake_ofs.y = 0.0f;
    pG->quake_ofs.z = 0.0f;
    gameDiedemoCheck();
    ProcessTickGet(5, "CamCtrl.Check()");
    SceAtCheck();
    ActBtn.move();
    if (!SpfFlagChk(pG, SPF_SCE)) {
        ScenarioMove();
    }
    ProcessTickGet(5, "ScenarioMove");
    if (StaFlagChk(pG, STA_SLOW)) {
        pPL->setSlow(player_Seq_speed);
        preb_slow_flg = 1;
    } else {
        if (preb_slow_flg == 1) {
            pPL->setSlow(1.0f);
        }
        preb_slow_flg = 0;
    }
    if (StaFlagChk(pG, STA_SLOW)) {
        slow = (pG->Frame_cnt % other_slow) == 0;
    } else {
        slow = 1;
    }
    if (slow) {
        EmMgr.move();
        EmListWaitDelete();
        StaFlagOff(pG, STA_SE_BURST);
        ProcessTickGet(5, "EmMgr.move");
    }
    if (!SpfFlagChk(pG, SPF_PL) && (pPL->be_flag & 0x20) &&
        (!StaFlagChk(pG, STA_SUSPEND) || (pPL->be_flag & 0x800))) {
        pPL->move();
    }
    ProcessTickGet(5, "Player");
    if (StaFlagChk(pG, STA_SLOW)) {
        slow = (pG->Frame_cnt % other_slow) == 0;
    } else {
        slow = 1;
    }
    if (slow) {
        StaFlagOff(pG, STA_NO_FENCE);
        if (!SpfFlagChk(pG, SPF_OBJ)) {
            ObjMgr.move();
        }
        ProcessTickGet(5, "ObjMgr.move");
        if (!SpfFlagChk(pG, SPF_CTRL)) {
            CtrlMgr.move();
        }
    }
    CameraMove();
    ProcessTickGet(5, "CameraMove");
    if (StaFlagChk(pG, STA_SLOW)) {
        slow = (pG->Frame_cnt % other_slow) == 0;
    } else {
        slow = 1;
    }
    if (slow) {
        if (!SpfFlagChk(pG, SPF_ESP)) {
            EffAreaUpdate();
            EffClearToolState();
            EspgenMove();
            EspMove();
            EspGenLoopMove();
            EffCallToolStateCallBack();
        }
        ProcessTickGet(5, "EspMove");
        if (!SpfFlagChk(pG, SPF_LIGHT)) {
            LightMgr.move();
        }
        ProcessTickGet(5, "LightMove");
        DmgMgr.move();
        SatMgr.dieCheck();
        EatMgr.dieCheck();
        if (pG->debug_mode == 12) {
            nObj = SmdGetObjNum();
            eprintf(0x180, 0x1C, 0, 12, "EM");
            EmMgr.dispWorkNum(0x1A0, 0x1C, 12, 0);
            eprintf(0x180, 0x2A, 0, 12, "OB");
            ObjMgr.dispWorkNum(0x1A0, 0x2A, 12, nObj);
            eprintf(0x180, 0x38, 0, 12, "EP");
            EspDispInfo();
            eprintf(0x180, 0x46, 0, 12, "EPG");
            EspgenDispInfo();
            eprintf(0x180, 0x54, 0, 12, "CTR");
            CtrlMgr.dispWorkNum(0x1A0, 0x54, 12, 0);
            eprintf(0x180, 0x62, 0, 12, "PRT");
            PartsMgr.dispWorkNum(0x1A0, 0x62, 12, nObj);
            eprintf(0x180, 0x70, 0, 12, "MI");
            ModInfoMgr.dispWorkNum(0x1A0, 0x70, 12, nObj);
            eprintf(0x180, 0x7E, 0, 12, "PRM");
            PrimDispWorkNum(0x1A8, 0x7E, 12);
            eprintf(0x180, 0x8C, 0, 12, "EV");
            EvtMgr.dispWorkNum(0x1A0, 0x8C, 12, 0);
            eprintf(0x180, 0x9A, 0, 12, "SAT");
            SatMgr.dispWorkNum(0x1A0, 0x9A, 12, 0);
            eprintf(0x180, 0xA8, 0, 12, "EAT");
            EatMgr.dispWorkNum(0x1A0, 0xA8, 12, 0);
            eprintf(0x180, 0xB6, 0, 12, "SCR         %4d", nObj);
            eprintf(0x180, 0xC4, 0, 12, "LIT");
            LightMgr.dispWorkNum(0x1A0, 0xC4, 12, 0);
            for (int i = 0; i <= 19; i++) {
                if (i == 19) {
                    eprintf(40, 0x150, 0, 14, "[%2d -inf] : %3d  %6d %4.2f", i, g_at_cnt[i], g_at_cyc[i],
                            (f32) g_at_cyc[i] / (f32) g_at_total_cyc * 100.0f);
                } else {
                    eprintf(40, (i + 2) * 16, 0, 14, "[%2d - %2d] : %3d  %6d %4.2f", i, i + 1, g_at_cnt[i],
                            g_at_cyc[i], (f32) g_at_cyc[i] / (f32) g_at_total_cyc * 100.0f);
                }
                g_at_cnt[i] = 0;
                g_at_cyc[i] = 0;
            }
            eprintf(40, 0x160, 0, 14, "[TOTAL]:%6d CYC:%d", g_at_total, g_at_total_cyc);
            g_at_total = 0;
            g_at_total_cyc = 0;
            for (int i = 0; i <= 19; i++) {
                if (i == 19) {
                    eprintf(40, 0x150, 0, 20, "[%2d -inf] : %3d  %6d %4.2f", i, g_at2_cnt[i], g_at2_cyc[i],
                            (f32) g_at2_cyc[i] / (f32) g_at2_total_cyc * 100.0f);
                } else {
                    eprintf(40, (i + 2) * 16, 0, 20, "[%2d - %2d] : %3d  %6d %4.2f", i, i + 1, g_at2_cnt[i],
                            g_at2_cyc[i], (f32) g_at2_cyc[i] / (f32) g_at2_total_cyc * 100.0f);
                }
                g_at2_cnt[i] = 0;
                g_at2_cyc[i] = 0;
            }
            eprintf(40, 0x160, 0, 20, "[TOTAL]:%6d CYC:%d", g_at2_total, g_at2_total_cyc);
            g_at2_total = 0;
            g_at2_total_cyc = 0;
        }
    }
    LightAreaUpdate();
    Cckpt.move();
    if (!FlagChkSign(pG->Debug_flg, DBG_TEST_MODE)) {
        SubScreenCall();
    }
    if (pG->debug_mode == 0x10) {
        ItemMgr.debugNumDisp(0x10);
    }
    if (!SysFlagChk(pG, SYS_DOORDEMO)) {
        gameDebugDisp();
    }
    if ((Key.trg & 0x2000) && !(Key.on & 0x400000) && !DbgFlagChk(pG, DBG_TEST_MODE) && OptionOpenCheck() == 1) {
        Game.Rno_bak = *(u32*)&pG->Rno0;
        pG->Rno0 = 6;
        pG->Rno1 = 0;
        pG->Rno2 = 0;
        pG->Rno3 = 0;
    }
    if (!SpfFlagChk(pG, SPF_BLOCK)) {
        Block.check(0);
    }
    if (SysFlagChk(pG, SYS_PUBLICITY_VER) || pG->debug_mode == 0) {
        DbgFlagOff(pG, DBG_FOG_FAR_GREEN);
    }
}

// Loads pSaveData into the game (cGameSave::load) and sets the continue-from-save state: pl_flag 1,
// System_flg 0x100, next room/point = the saved room.
void GameLoad()
{
    GameSave.load(pSaveData);
    pG->pl_flag = 1;
    SysFlagOn(pG, SYS_LOAD_GAME);
    SysFlagOff(pG, SYS_CONTINUE);
    SysFlagOff(pG, SYS_START_EVT_SKIP);
    pG->RoomNo_next = pG->room_id;
    pG->Part_next = pG->Part;
}

// Continue after death: reloads the save keeping play time and the continue counters (+1 for mode
// 0), re-applies costume/weapon data, restores the saved position/angle and jumps to the door demo
// (Rno0 = 4) of the saved room.
void GameContinue(int option_flag)
{
    u32 time = pG->play_time;
    u16 x4F90 = pG->r_continue_cnt;
    u16 x8338 = pG->c_continue_cnt;
    u16 g_continue_cnt = pG->g_continue_cnt;

    GameSave.load(pSaveData);
    if (pG->SaveKind == -1) {
        SysFlagOn(pG, SYS_CONTINUE);
    } else {
        SysFlagOn(pG, SYS_LOAD_GAME);
    }
    if (option_flag == 0) {
        pG->r_continue_cnt = x4F90 + 1;
        pG->c_continue_cnt = x8338 + 1;
        pG->g_continue_cnt = g_continue_cnt + 1;
    }
    pG->play_time = time;
    PlSetCostume();
    ContinueWepData();
    pG->NextPos = pG->pl_pos;
    pG->NextY = pG->pl_ang_y;
    pG->RoomNo_next = pG->room_id;
    pG->Part_next = pG->Part;
    pG->Rno0 = 4;
    pG->Rno1 = 0;
    pG->Rno2 = 0;
    pG->Rno3 = 0;
}

// GlobalWork 0x4FA4 .. 0x500C: the part of the save block that survives clearGlobalSaveData.
struct GlobalKeep {
    u8 b[0x68];
};
// GlobalWork 0x8330 .. 0x8338: also kept.
struct GlobalKeep2 {
    u32 x8330;
    u32 x8334;
};

// New-round reset of the save block (0x36F8 bytes at save_data_start_addr) keeping game count,
// pesetas, language, game mode, save kind and the two keep blocks; life refilled, play time reset.
void clearGlobalSaveData()
{
    GlobalKeep keep;
    GlobalKeep2 keep2;
    u16 x4F8E = pG->game_cnt;
    u32 x4F98 = pG->peseta;
    u8 x4F93 = pG->game_country;
    u8 x8354 = pG->game_mode;
    s32 game_mode = pG->SaveKind;

    memcpy(&keep2, &pG->shootingScore, sizeof(keep2));
    memcpy(&keep, &pG->pl_life, sizeof(keep));
    memclr_asm(pG->save_data_start_addr, 0x36F8);
    {
        u32* score = (u32*) &pG->shootingScore;

        memcpy(score, &keep2, sizeof(keep2));
    }
    memcpy(&pG->pl_life, &keep, sizeof(keep));
    pG->game_cnt = x4F8E;
    pG->peseta = x4F98;
    pG->game_country = x4F93;
    pG->game_mode = x8354;
    pG->SaveKind = game_mode;
    pG->pl_life = pG->pl_life_max;
    pG->ashley_life = pG->ashley_life_max;
    InitGameTime();
}

// Restores the game from a SAVE_DATA_HEAD image: the global block (0x4F80..), room flags, sub screen,
// merchant and item data; SaveKind 3 (new round) clears the block, resets rooms/BGM and starts at
// r120 with the carried-over merchant/items (2nd round bonuses).
bool cGameSave::load(SAVE_DATA_HEAD* head)
{
    if (head->base == 0) {
        return 0;
    }
    checkAddr(head);
    {
        u32* save = (u32*) &pG->save_data_start_addr;

        memcpy(save, head->pGlobal, sizeof(GameSaveBlock));
    }
    if (pG->SaveKind == 3) {
        clearGlobalSaveData();
        RoomData.clear(head->pRm);
        SndBgmTblInit();
        MerchantDataLoad(head->pMr);
        ItemMgr.load(head->pItm);
        if (pG->game_cnt == 1) {
            Merchant2ndRoundInit();
        }
        ItemMgr.dumpType(7);
        pG->room_id = 0x120;
    } else {
        RoomData.load(head->pRm);
        SscrnDataLoad(head->pSscrn);
        MerchantDataLoad(head->pMr);
        ItemMgr.load(head->pItm);
        PlSetCostume();
    }
    return 1;
}

// cGameSave::save: writes the game state into the image (player position/angle when in play,
// SaveKind = mode, global block, room flags, sub screen, merchant, items).
bool cGameSave::save(SAVE_DATA_HEAD* data, int mode)
{
    if (data->base == 0) {
        return 0;
    }
    checkAddr(data);
    if (pG->Rno0 == 3) {
        VEC_COPY(pG->pl_pos, pPL->pos);
        pG->pl_ang_y = pPL->ang.y;
    }
    pG->SaveKind = mode;
    *data->pGlobal = *(GameSaveBlock*) pG->save_data_start_addr;
    RoomData.save(data->pRm);
    SscrnDataSave(data->pSscrn);
    MerchantDataSave(data->pMr);
    ItemMgr.save(data->pItm);
    return 1;
}

// Re-bases the image's pointers when it was copied from another address (memory card load).
void cGameSave::checkAddr(SAVE_DATA_HEAD* head)
{
    SAVE_DATA_HEAD* base = head->base;

    if (base != 0 && base != head) {
        calcOffset(head, (u32) base);
        calcAddr(head);
    }
}

// Converts the image's section pointers to offsets from base (before writing to card).
void cGameSave::calcOffset(SAVE_DATA_HEAD* head, u32 headaddr)
{
    u32 p;

    if (head->base == 0) {
        return;
    }
    if (headaddr == 0) {
        headaddr = (u32) head;
    }
    // One shared temporary: its anti-dependences keep each load below the previous add/sub.
    head->base = 0;
    p = (u32) head->pGlobal;
    head->pGlobal = (GameSaveBlock*) (p - headaddr);
    p = (u32) head->pRm;
    head->pRm = (void*) (p - headaddr);
    p = (u32) head->pSscrn;
    head->pSscrn = (u32*) (p - headaddr);
    p = (u32) head->pMr;
    head->pMr = (void*) (p - headaddr);
    p = (u32) head->pItm;
    head->pItm = (void*) (p - headaddr);
}

// Converts the image's section offsets back to pointers (base = the image itself).
void cGameSave::calcAddr(SAVE_DATA_HEAD* head)
{
    u32 p;

    if (head->base != 0) {
        return;
    }
    head->base = head;
    p = (u32) head->pGlobal;
    head->pGlobal = (GameSaveBlock*) ((u32) head + p);
    p = (u32) head->pRm;
    head->pRm = (void*) ((u32) head + p);
    p = (u32) head->pSscrn;
    head->pSscrn = (u32*) ((u32) head + p);
    p = (u32) head->pMr;
    head->pMr = (void*) ((u32) head + p);
    p = (u32) head->pItm;
    head->pItm = (void*) ((u32) head + p);
}

// Allocates the save image: global block at 0x40, room data at 0x3740, then sub screen, merchant
// and item sections (32-byte aligned), and fixes the pointers.
SAVE_DATA_HEAD* cGameSave::alloc()
{
    u32 globalOfs = 0x40;
    u32 roomOfs = 0x3740;
    u32 sscrnOfs;
    u32 merchantOfs;
    u32 itemOfs;
    u32 size;
    u32 roomSize;
    u32 sscrnSize;
    u32 merchantSize;
    u32 itemSize;
    SAVE_DATA_HEAD* d;

    roomSize = ALIGN32(RoomData.num * 0xD8 + 0x10);
    sscrnSize = ALIGN32(SscrnDataSize());
    sscrnOfs = roomOfs + roomSize;
    merchantSize = ALIGN32(MerchantDataSize());
    merchantOfs = sscrnOfs + sscrnSize;
    itemSize = ALIGN32(ItemMgr.saveDataSize());
    itemOfs = merchantOfs + merchantSize;
    size = itemOfs + itemSize;
#line 1385 "D:/Bio4/Prog/game.cpp"
    d = (SAVE_DATA_HEAD*) MEM_CALLOC(size, 1, 13);
    d->pGlobal = (GameSaveBlock*) globalOfs;
    d->pRm = (void*) roomOfs;
    d->pSscrn = (u32*) sscrnOfs;
    d->pMr = (void*) merchantOfs;
    d->pItm = (void*) itemOfs;
    d->size = size;
    d->base = 0;
    calcAddr(d);
    return d;
}

// Rno0 == 5: the ending screen: loads Etc/Ending.tpl, fades in and shows it until START, then
// requests the soft reset (System_flg 0x4000000).
void gameEnding()
{
    static TEXPalette* pTpl;

    switch (pG->Rno1) {
    case 0: {
        int req;

        gameRoomMemInit();
        pG->Disp_flg = 0xFFFFFFFF;
#line 1419 "D:/Bio4/Prog/game.cpp"
        req = DvdReadN("Etc/Ending.tpl", 0, 0, 0, 0, 5, __FILE__, __LINE__);
        Dvd.ReadCheck(req, 0, 0, (void**) &pTpl);
        fadeSetG(0x80000000, 30, 0, 0);
        pG->Rno1++;
        break;
    }
    case 1:
        DrawTpl(pTpl, 0, 0, 512, 448);
        if (Joy[0].trg & 0x100) {
            SysFlagOn(pG, SYS_SOFT_RESET);
        }
        break;
    }
}

// Rno0 == 6: the option screen over the paused game: Rno1 0 stops everything (Stop_flg) and opens
// OptScrn, 1 runs it, 2 restores Stop_flg, the HUD ids and returns to the saved mode (Game.Rno_bak).
void gameOption()
{
    static u32 stop_bak;

    switch (pG->Rno1) {
    case 0:
        SetGameTime();
        stop_bak = pG->Stop_flg;
        pG->Stop_flg = 0xFFFFFFFF;
        SpfFlagOff(pG, SPF_KEY);
        SpfFlagOff(pG, SPF_ID_SYSTEM);
        OptScrn.init(0);
        SndSePauseAll(1);
        pG->Rno1++;
        /* fallthrough */
    case 1:
        if (OptScrn.move()) {
            InitGameTime();
            pG->Rno1++;
        }
        break;
    case 2:
        pG->Stop_flg = stop_bak;
        IdSys.dispSw(IDC_LIFE_METER, 1);
        IdSys.dispSw(IDC_ACT_BUTTON, 1);
        IdSys.dispSw(IDC_COUNT_DOWN, 1);
        OptScrn.quit();
        SndSePauseAll(0);
        *(u32*)&pG->Rno0 = Game.Rno_bak;
        break;
    }
}

// Starts the death demo after `time` frames (type 0 Leon, 2 Ada/Separate Ways): sets Status_flg[0]
// 0x100000, stops input/movement, hides the HUD and runs gameDiedemo as a task.
void DiedemoExec(int time, int type)
{
    if (StaFlagChk(pG, STA_DIEDEMO)) {
        return;
    }
    diedemo_work.exec_frame = time;
    diedemo_work.demo_type = type;
    StaFlagOn(pG, STA_DIEDEMO);
    KeyStop(0xEFCF0000);
    SpfFlagOn(pG, SPF_SCE_AT);
    SpfFlagOn(pG, SPF_ACTBTN);
    IdSys.kill(0xFF, IDC_ACT_BUTTON);
    Cckpt.endCountDownTimer();
    PlEndCamera();
    TaskExec(1, (TaskFunc) gameDiedemo, (int) &diedemo_work);
}

// Per-frame: starts the death demo when the partner's life (ashley_life) or the player's life
// (pl_life) is <= 0 (skipped in debug no-death mode).
void gameDiedemoCheck()
{
    if (DbgFlagChk(pG, DBG_TEST_MODE)) {
        return;
    }
    if (pSUB != 0 && (s16) pG->ashley_life <= 0) {
        DiedemoExec(90, 0);
    }
    if ((s16) pG->pl_life <= 0) {
        if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
            DiedemoExec(90, 2);
        } else {
            DiedemoExec(90, 0);
        }
    }
}

// The death demo task: waits exec_frame, shows "YOU ARE DEAD" (variant when the partner is alive),
// fades and stops the sound, after 270 frames or START shows the Continue / Load Game menu, then
// GameContinue(0) + LVADD_DIE or the soft reset.
void gameDiedemo(DiedemoWork* pDw)
{
    int cnt = 0;
    u32 step = 0;
    int sel = 1;
    int kind;
    int id;
    u64 trg;

    OSReport("--DIEDEMO START!!\n");
    int timer = 0;
    int cnt2 = 0;
    for (;;) {
        switch (step) {
        case 0:
            if (cnt >= pDw->exec_frame) {
                step++;
            }
            break;
        case 1:
            IdTexDataLoad((void*) (((OptionArc*) pG->pOption)->ofs_10 + (u32) pG->pOption), TEX_OWNER_ID_DEAD);
            IdSys.kill(0xFF, IDC_LIFE_METER);
            kind = pDw->demo_type;
            if (kind == 0) {
                kind = 1;
                if (pSUB != 0 && (s16) pG->pl_life != 0) {
                    kind = 2;
                }
            }
            switch (kind) {
            case 1:
                IdSys.set((void*) (((OptionArc*) pG->pOption)->ofs_14 + (u32) pG->pOption), 0xFF, IDC_DEAD, 0x13, 6, 0);
                break;
            case 2:
                IdSys.set((void*) (((OptionArc*) pG->pOption)->ofs_1C + (u32) pG->pOption), 0xFF, IDC_DEAD, 0x13, 6, 0);
                break;
            }
            if (StaFlagChk(pG, STA_EVENT_CANCEL)) {
                IdSys.unitPtr(0, IDC_DEAD)->be_flag |= 8;
                fadeSetG(0x80000002, 1, 0, 0);
            } else {
                IdSys.unitPtr(0, IDC_DEAD)->be_flag &= ~8;
            }
            SndAllFadeOut();
            step++;
            SndStrReq(0, 0, (int) 0x80000003, 0, 0, 0.0f);
            /* fallthrough */
        case 2:
            if (cnt >= pDw->exec_frame + 0x10E || (Key.trg & 0x80000000)) {
                IdSys.set((void*) (((OptionArc*) pG->pOption)->ofs_18 + (u32) pG->pOption), 0xFF, IDC_CONTINUE, 0x13, 5, 0);
                cnt2 = 0;
                step++;
                IdSys.beMove(IdSys.unitPtr(0x30, IDC_CONTINUE), 0);
                IdSys.beMove(IdSys.unitPtr(0x40, IDC_CONTINUE), 0);
                IdSys.unitPtr(0, IDC_CONTINUE)->be_flag |= 8;
                IdSys.unitPtr(1, IDC_CONTINUE)->be_flag &= ~8;
                pG->Stop_flg = 0xFFFFFFFF;
                SpfFlagOff(pG, SPF_ID_SYSTEM);
            }
            break;
        case 3:
            cnt2++;
            if (cnt2 > 14) {
                step = 4;
            }
            break;
        case 4:
            trg = Key.trg & 0x80000000;
            if (trg) {
                if (sel) {
                    timer = 0xB1;
                    id = 0x40;
                    SndCall(0, 8, 0, 0, 0, 0);
                } else {
                    timer = 0x96;
                    id = 0x30;
                    SndCall(0, 5, 0, 0, 0, 0);
                }
                IdSys.beMove(IdSys.unitPtr(id, IDC_CONTINUE), 1);
                DpfFlagOn(pG, DPF_PL);
                DpfFlagOn(pG, DPF_OBJ);
                step++;
            } else {
                int old = sel;

                if ((Key.trg & 0x08000000)) {
                    sel = 1;
                }
                if ((Key.trg & 0x04000000)) {
                    sel = 0;
                }
                if (old != sel) {
                    IdSys.unitPtr(0, IDC_CONTINUE)->timer[2] = 0;
                    IdSys.unitPtr(1, IDC_CONTINUE)->timer[2] = 0;
                    SndCall(0, 6, 0, 0, 0, 0);
                }
                if (sel) {
                    IdSys.unitPtr(0, IDC_CONTINUE)->be_flag |= 8;
                    IdSys.unitPtr(1, IDC_CONTINUE)->be_flag &= ~8;
                } else {
                    IdSys.unitPtr(0, IDC_CONTINUE)->be_flag &= ~8;
                    IdSys.unitPtr(1, IDC_CONTINUE)->be_flag |= 8;
                }
            }
            break;
        case 5:
            if (--timer <= 0) {
                step = 6;
                CamCtrl.endScope();
            }
            break;
        case 6:
            if (sel == 1) {
                OSReport("--CONTINUE SELECT!!\n");
                GameContinue(0);
                GameAddPoint(LVADD_DIE);
            } else {
                OSReport("--SOFT_RESET SELECT!!\n");
                SysFlagOn(pG, SYS_SOFT_RESET);
            }
            TaskExit();
            break;
        }
        cnt++;
        TaskSleep(1);
    }
}

// Rno0 == 4: the room transition: stops everything, fades out (m_door_fade_eff 0 = 120-frame stop
// filter, 1 = 15-frame fade, 2 = instant), runs the pending door/exit callbacks, cancels loads,
// plays the door sound, moves the player to NextPos/NextY, sets room_id/Part to the next room and
// goes back to Rno0 = 1.
void gameDoordemo()
{
    OSReport("--DOORDEMO START!!\n");
    SysFlagOn(pG, SYS_ROOMJUMP);
    pG->Stop_flg = 0xFFFFFFFF;
    KeyStop(0xEFCF0000);
    if (DbgFlagChk(pG, DBG_ROOMJMP)) {
        fadeSetG(0, 0, 0, 0);
        TaskSleep(1);
    } else if (Flag54(0x80000) || Flag54(0x100)) {
        fadeSetG(0, 0, 0, 0);
    } else {
        switch (SceSys.m_door_fade_eff) {
        case 0:
        default: {
            Filter09GetEFB_801D19E0();
            Filter09SetbUse(1, 1);
            fadeSetG(0, 120, 0, 0);
            break;
        }
        case 1: {
            fadeSetG(0, 15, 0, 0);
            while (Fade[0].flags & 1) {
                TaskSleep(1);
            }
            break;
        }
        case 2: {
            fadeSetG(0, 0, 0, 0);
            break;
        }
        }
    }
    if (!Flag54(0x80000) && !Flag54(0x100)) {
        cSceSys* s = &SceSys;
        if (s->pDoorFunc != 0) {
            ((void (*)(void*)) s->pDoorFunc)(s->pDoorParam);
            s->pDoorFunc = 0;
        }
        if (s->pExitFunc != 0) {
            ((void (*)(void*)) s->pExitFunc)(s->pExitParam);
            s->pExitFunc = 0;
        }
    }
    TaskSleep(1);
    pG->Disp_flg = 0xFFFFFFFF;
    TaskSleep(2);
    SysFlagOn(pG, SYS_TRANS_STOP);
    TaskSleep(2);
    DC.initDataUnit();
    Dvd.ReadCancelAll();
    Aram.DmaCancelAll();
    EmReadInit();
    ClearOt();
    {
        int req = SndDoorSeLoad();
        SndNextRoomInit();
        while (SndStatDisp(req) == 0) {
            TaskSleep(1);
        }
    }
    if (!Flag54(0x80000) && !Flag54(0x100)) {
        DoorSeCall(0);
    }
    pG->pl_pos = pG->NextPos;
    pG->pl_ang_y = pG->NextY;
    pG->room_id = pG->RoomNo_next;
    pG->Part = pG->Part_next;
    if (!FlagChkSign(pG->Debug_flg, DBG_ROOMJMP) && !SysFlagChk(pG, SYS_CONTINUE)) {
        pG->JumpPoint = 0;
    }
    pG->Rno0 = 1;
    pG->Rno1 = 0;
    pG->Rno2 = 0;
    pG->Rno3 = 0;
    OSReport("--DOORDEMO END!!\n");
}

// Frees the room heap: in the shooting-range mode swaps a 0x188000 block with ARAM and creates
// heap 10, otherwise replaces heap 3/4; clears the room part of the global work (pad_16C..) and
// the debug/status flags.
void gameRoomMemInit()
{
    if (SysFlagChk(pG, SYS_DOORDEMO)) {
        MemReplaceHeap(3, 4);
        MemorySwap((void*) 0x807EC000, ARAM_FREE_BASE, 0x188000);
        memclr_asm((void*) 0x807EC000, 0x188000);
        MemCreateHeap(10, 0x807EC000, 0x80974000);
        MemSetCurrentHeap(10);
    } else {
        if (SysFlagChk(pG, SYS_DOOR_AFTER)) {
            MemDestroyHeap(10);
            MemorySwap((void*) 0x807EC000, ARAM_FREE_BASE, 0x188000);
        } else {
            MemReplaceHeap(3, 4);
        }
        MemSetCurrentHeap(4);
    }
    memclr_asm(pG->room_start_addr, 0x4E00);
    pG->Debug_flg[0] = 0;
    pG->Debug_flg[1] = 0;
    pG->Status_flg[0] = 0;
    pG->Status_flg[1] = 0;
    pG->Status_flg[2] = 0;
    if (DbgFlagChk(pG, DBG_DOOR_SET_MODE)) {
        DbgFlagOn(pG, DBG_TEST_MODE);
        SpfFlagOn(pG, SPF_SCE);
        SpfFlagOn(pG, SPF_EM);
    }
}

// Sets the starting difficulty points by game mode (normal 5500, professional 11000, easy 4500,
// Separate Ways 4000 / room specific, Assignment Ada 9999) and applies them (GameAddPoint(0)).
void GamePointInit(u32 type)
{
    switch (type) {
    case 0:
    default:
        switch (pG->game_mode) {
        case 0:
        case 5:
        default:
            pG->point = 0x157C;
            break;
        case 6:
            pG->point = 0x2AF7;
            break;
        case 3:
            pG->point = 0x1194;
            break;
        case 1:
            pG->point = 0xFA0;
            break;
        }
        break;
    case 1:
        pG->point = 0x270F;
        break;
    case 2:
        switch (pG->room_id) {
        case 0x401:
        default:
            pG->point = 0x157C;
            break;
        case 0x402:
            pG->point = 0x1388;
            break;
        case 0x403:
        case 0x404:
            pG->point = 0xFA0;
            break;
        }
        break;
    }
    GameAddPoint(0);
}

// Dynamic difficulty: adds the LVADD_* delta for the event (death -800, damage -400/-500, misses
// -1..-50, critical hit / kill / recovery +1..+75), scaled by the current rank (upTbl/dnTbl), clamps
// to 0..11000 (fixed 9999 for Ada, max in professional/shooting range, min 1000 outside Japan) and
// recomputes pG->Game_level (points/1000; /1833 easy, /2750 Separate Ways, fixed 10 professional).
void GameAddPoint(int type)
{
    int add;

    switch (type) {
    default:
        add = 0;
        break;
    case 0:
        add = 0;
        break;
    case 1:
        add = -800;
        break;
    case 2:
        add = -400;
        break;
    case 3:
        add = -500;
        break;
    case 4:
        add = -5;
        break;
    case 5:
        add = -1;
        break;
    case 6:
        add = -25;
        break;
    case 7:
        add = -50;
        break;
    case 8:
        add = -1;
        break;
    case 9:
        add = 50;
        break;
    case 10:
        add = 1;
        break;
    case 11:
        add = 50;
        break;
    case 12:
        add = 2;
        break;
    case 13:
        add = 75;
        break;
    case 14:
        add = 1;
        break;
    }
    {
        f32 upTbl[11] = {2.0f, 1.5f, 1.3f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f};
        f32 dnTbl[11] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.5f, 2.0f};

        if (type != 0) {
            if (type != 14) {
                u8 rank = pG->point / 1000;
                f32 rate;

                if (rank > 10) {
                    rank = 10;
                }
                if (add > 0) {
                    rate = upTbl[rank];
                } else {
                    rate = dnTbl[rank];
                }
                if (add > 0) {
                    add = (int) (((f32) add + 0.5f) * rate);
                } else {
                    add = (int) (((f32) add - 0.5f) * rate);
                }
            }
            pG->point = pG->point + add;
        }
    }
    if (pG->point > 0x2AF7) {
        pG->point = 0x2AF7;
    }
    if (pG->point < 0) {
        pG->point = 0;
    }
    if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
        pG->point = 0x270F;
    }
    if (SysFlagChk(pG, SYS_HARD_MODE)) {
        pG->point = 0x2AF7;
    }
    if (pG->shooting_mode != 0) {
        pG->point = 0x2AF7;
    }
    if (pG->game_country != 0) {
        if (pG->point < 1000) {
            pG->point = 1000;
        }
    }
    if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        switch (pG->room_id) {
        case 0x401:
            break;
        case 0x402:
            pG->point = 0x1388;
            break;
        case 0x403:
        case 0x404:
            pG->point = 0xFA0;
            break;
        }
        pG->Game_level = pG->point / 1000;
    } else {
        switch (pG->game_mode) {
        case 0:
        case 5:
        default:
            pG->Game_level = pG->point / 1000;
            break;
        case 6:
            pG->Game_level = 10;
            break;
        case 3:
            pG->Game_level = pG->point / 1833;
            break;
        case 1:
            pG->Game_level = pG->point / 2750;
            break;
        }
    }
    if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
        pG->Game_level = 6;
    }
}

// Before a boss: raises the points to at least 5500 (except Ada / professional / continue) and
// mirrors them into the save block.
void GamePointBossReset()
{
    if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
        return;
    }
    if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        return;
    }
    if (SysFlagChk(pG, SYS_CONTINUE_AFTER)) {
        return;
    }
    if (pG->point < 0x157C) {
        pG->point = 0x157C;
    }
    pSaveData->pGlobal->point = pG->point;
}

// Allocates the room's primitive (line/poly) buffer of nPrim words, halving the count until the
// allocation succeeds, and registers it with the draw code.
void primInit()
{
    pG->prim_cnt = 0;
    pG->nPrim *= 2;
    do {
        pG->nPrim = pG->nPrim / 2;
#line 2215 "D:/Bio4/Prog/game.cpp"
        (pG->prim_cnt = (s32) MEM_ALLOC(pG->nPrim * 2, 1, 13));
        if ((u32) pG->prim_cnt < 0x80000000 || (u32) pG->prim_cnt > 0x82FFFFFF) {
            pLog->err(0, 0, "workInit() PRIM BUFFER SIZE WAS REDUCE %08X", pG->nPrim);
        }
    } while (pG->prim_cnt == 0);
    memclr_asm((void*) pG->prim_cnt, pG->nPrim * 2);
    SetPrimBuffPtr();
}

// Frees the primitive buffer.
void primFree()
{
    Mem_free((void*) pG->prim_cnt);
    pG->prim_cnt = 0;
}

// Debug: prints the primitive buffer usage at (x, y).
void PrimDispWorkNum(int x, int y, int page)
{
    eprintf(x, y, 0, page, "%5X/%5X", (int) ((f32) pG->nPrim * pG->prim_rate), pG->nPrim);
}

u32 stop_rno = 0;
static int lbl_80314BA4 = 0;
static u32 stop_bak;

// Ends the debug pause (Stop_flg restored) if it was active.
void GameStopModeEnd()
{
    if (stop_rno != 0) {
        pG->Stop_flg = stop_bak;
        stop_rno = 0;
    }
}

// Debug pause on pad 2 START: stop_rno 1 = paused (all move Stop_flg bits on), 2 = one-frame step
// when START is pressed again; released by another START.
void gameStopMove()
{
    if (stop_rno == 0 && !SpfFlagChk(pG, SPF_PL)) {
        stop_rno = 0;
    }
    switch (stop_rno) {
    case 0:
        if (Joy[1].trg & 0x1000) {
            stop_bak = pG->Stop_flg;
            stop_rno = 1;
        }
        break;
    case 1:
        SpfFlagOn(pG, SPF_EM);
        SpfFlagOn(pG, SPF_PL);
        SpfFlagOn(pG, SPF_SUBCHAR);
        SpfFlagOn(pG, SPF_OBJ);
        SpfFlagOn(pG, SPF_CTRL);
        SpfFlagOn(pG, SPF_ESP);
        SpfFlagOn(pG, SPF_LIGHT);
        SpfFlagOn(pG, SPF_SCE);
        SpfFlagOn(pG, SPF_EVT);
        SpfFlagOn(pG, SPF_SCE_AT);
        SpfFlagOn(pG, SPF_CAMERA);
        SpfFlagOn(pG, SPF_EARTHQUAKE);
        stop_rno = 2;
        /* fallthrough */
    case 2:
        if (pG->Frame_cnt & 0x10) {
            eprintf(0xA0, 0xE8, 4, 0, "STOP MODE");
        }
        if (Joy[1].trg & 0x1000) {
            pG->Stop_flg = stop_bak;
            stop_rno = 0;
        } else if (Joy[1].rep & 0x800) {
            pG->Stop_flg = stop_bak;
            stop_rno = 1;
        }
        break;
    }
}

// Debug overlays by debug_mode: player position/routine/life line, the enemy line-of-sight lines
// (Debug_flg[2] 0x1000), SAT/EAT collision, camera paths, enemy info, room wireframe, ...
void gameDebugDisp()
{
    int col;
    u32 i;

    if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
        col = 0;
        if (pPL->dmg.m_Flag || pPL->dmg.m_Timer) {
            col = 2;
        }
        eprintf(60, 0x18C, col, 0, "Rank[%d,%d],Kill[%d]", pG->Game_level, pG->point, pG->g_kill_cnt);
        eprintf(60, 0x19B, col, 0, "C:SHOT[%d],HIT[%d]", pG->c_shot_cnt, pG->c_hit_cnt);
        eprintf(60, 0x1AA, col, 0, "G:SHOT[%d],HIT[%d]", pG->g_shot_cnt, pG->g_hit_cnt);
        if (pG->debug_mode == 7) {
            eprintf2(8, 14, 32, 0x19C, col, 7, "POS[%.2f, %.2f, %.2f], Dir[%.2f]", pPL->pos.x, pPL->pos.y + 0.01f,
                     pPL->pos.z, pPL->ang.y);
            eprintf2(8, 14, 32, 0x1AA, col, 7, "RNO[%02x][%02x][%02x][%02x], HP[%04d],FRAME[%03d/%03d]", pPL->r_no_0,
                     pPL->r_no_1, pPL->r_no_2, pPL->r_no_3, (s16) pG->pl_life, (u32) pPL->Motion.Seq_frame, pPL->Motion.Seq_frame_num);
        }
        if (!StaFlagChk(pG, STA_EVENT)) {
            eprintf(20, 30, 0, 0, "P[%.0f,%.0f,%.0f]", pPL->pos.x, pPL->pos.y, pPL->pos.z);
            {
                int c0 = 'O';
                int c1 = 'O';
                if (pPL->dmg.m_Flag || pPL->dmg.m_Timer) {
                    c0 = 'X';
                }
                if (!(pPL->atari.m_flag & 0x100)) {
                    c1 = 'X';
                }
                eprintf(20, 45, 0, 0, "[%c%c:%d,%d,%d,%d]", c0, c1, pPL->r_no_0, pPL->r_no_1, pPL->r_no_2, pPL->r_no_3);
            }
            if (pSUB != 0) {
                eprintf(20, 60, 0, 0, "A[%.0f,%.0f,%.0f]", pSUB->pos.x, pSUB->pos.y, pSUB->pos.z);
                {
                    int c0 = 'O';
                    int c1 = 'O';
                    if (pSUB->dmg.m_Flag || pSUB->dmg.m_Timer) {
                        c0 = 'X';
                    }
                    if (!(pSUB->atari.m_flag & 0x100)) {
                        c1 = 'X';
                    }
                    eprintf(20, 75, 0, 0, "[%c%c:%d,%d,%d,%d]", c0, c1, pSUB->r_no_0, pSUB->r_no_1, pSUB->r_no_2, pSUB->r_no_3);
                }
            }
        }
        if (DbgFlagChk(pG, DBG_EM_LIFE_DISP)) {
            for (i = 0; i < EmMgr.getArrayNum(); i++) {
                cEm* em = EmMgr.fastAt(i);
                Vec pos2;
                Vec pos;
                Vec scr2;
                Vec scr;
                int hit;

                if (!em->isAlive()) {
                    continue;
                }
                if (em->hp <= 0) {
                    continue;
                }
                switch (em->id) {
                case 0x10:
                case 0x11:
                case 0x12:
                case 0x13:
                case 0x14:
                case 0x15:
                case 0x16:
                case 0x17:
                case 0x18:
                case 0x19:
                case 0x1A:
                case 0x1B:
                case 0x1C:
                case 0x1D:
                case 0x1E:
                case 0x1F:
                case 0x20:
                case 0x22:
                case 0x23:
                case 0x25:
                case 0x27:
                case 0x2B:
                case 0x2C:
                case 0x2D:
                case 0x2F:
                case 0x32:
                case 0x33:
                case 0x34:
                case 0x35:
                case 0x36:
                case 0x37:
                case 0x38:
                case 0x39:
                    pos = em->pos;
                    pos2 = pos;
                    pos2.y += 1200.0f;
                    break;
                case 0x30:
                case 0x31:
                    pos = em->getPartsPtr(0)->world;
                    pos2 = pos;
                    if (em->type == 0) {
                        pos2.y += -400.0f;
                    } else {
                        pos2.y += 0.0f;
                    }
                    break;
                default:
                    continue;
                }
                scr = pos2;
                if (GetScreenPos(&scr, &scr2) != 1) {
                    continue;
                }
                scr = pPL->pos;
                scr.y += 1200.0f;
                hit = EatMgr.hitCheck(&scr, &pos2, 0, 0, 0, 0);
                if (hit != 0 && !(hit & 0x404000)) {
                    continue;
                }
                Draw_line3d(&pos, &pos2, 0xFFFFFFFF, 0);
                eprintf2(8, 12, (u32) scr2.x - 16, (u32) scr2.y, 0, 0, "%d", em->hp);
            }
        }
        if (pG->debug_mode == 0x18) {
            EtcModelDebugDisp();
        }
    }
    DbWork->move();
    if (DbgFlagChk(pG, DBG_SAT_DISP)) {
        SatMgr.disp(0);
    }
    if (DbgFlagChk(pG, DBG_EAT_DISP)) {
        EatMgr.disp(0);
    }
    if (DbgFlagChk(pG, DBG_RTP_DISP)) {
        Draw_rtp();
    }
    if (DbgFlagChk(pG, DBG_EMINFO_DISP)) {
        Draw_eminfo();
    }
    if (DbgFlagChk(pG, DBG_WIRE_DISP)) {
        DrawRoomWireframe();
    }
    if (DbgFlagChk(pG, DBG_UNDER_CONST)) {
        DrawTpl((TEXPalette*) (pG->pCore->ofs_90 + (u32) pG->pCore), 0x118, 0x186, 0xDC, 0x1E);
    }
}

// Debug input: START + pad button opens the debug menu; in the shooting range mode pad 4 moves and
// turns the player directly.
void gameDebug()
{
    if ((Joy[0].trg & 0x1000) && (Joy[0].on & 0x40) && !FlagChkSign(pG->Debug_flg, DBG_TEST_MODE)) {
        if (SysFlagChk(pG, SYS_PUBLICITY_VER)) {
            if (PadCheckStatus(&Joy[1]) == 1) {
                DbMenuExec();
            }
        } else {
            DbMenuExec();
        }
    }
    eprintf2(10, 16, 0x1AE, 8, 0, 0, "%03x ", pG->room_id);
    eprintf(0x1DA, 8, 0, 0, "%d", CamCtrl.CurrentAreaNo());
    eprintf(0x1F2, 8, 0, 0, "%d", pG->AreaNo);
    if (pG->shooting_mode != 0) {
        Vec v = {0.0f, 0.0f, 0.0f};

        if (Joy[3].on & 0x40) {
            if (Joy[3].on & 2) {
                v.x += 50.0f;
            }
            if (Joy[3].on & 1) {
                v.x -= 50.0f;
            }
            if (Joy[3].on & 8) {
                v.y += 50.0f;
            }
            if (Joy[3].on & 4) {
                v.y -= 50.0f;
            }
            if (Joy[3].on & 0x100000) {
                pPL->ang.y += 0.09817477f;
            }
            if (Joy[3].on & 0x200000) {
                pPL->ang.y -= 0.09817477f;
            }
        } else {
            if (Joy[3].on & 2) {
                v.x += 15.0f;
            }
            if (Joy[3].on & 1) {
                v.x -= 15.0f;
            }
            if (Joy[3].on & 8) {
                v.y += 15.0f;
            }
            if (Joy[3].on & 4) {
                v.y -= 15.0f;
            }
            if (Joy[3].on & 0x100000) {
                pPL->ang.y += 0.024543693f;
            }
            if (Joy[3].on & 0x200000) {
                pPL->ang.y -= 0.024543693f;
            }
        }
        if (Joy[3].on & 0xF) {
            VecToCamVec(&v, &v);
            PSVECAdd(&v, &pPL->pos, &pPL->pos);
            PartsWorldPosCalc(pPL);
        }
    }
}

template <class T>
// Debug: prints the manager's alive/peak/array counts at (x, y) (sub = entries reserved for the scroll objects).
int cManager<T>::dispWorkNum(int x, int y, int col, int sub)
{
    u32 n;
    u32 i;

    if ((u32) pArray < 0x80000000 || (u32) pArray > 0x82FFFFFF) {
        return 0;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        T* p = fastAt(i);
        if (!p->isEmpty()) {
            n++;
        }
    }
    eprintf(x, y, 0, col, "%3d/%3d/%4d", n - sub, maxAlive - sub, nArray - sub);
    return n;
}
