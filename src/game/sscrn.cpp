// game/sscrn: the sub screen (inventory / map / puzzle / shop / files / radio terminal) front end:
// its data and REL live in ARAM and are swapped into the game heap while it is open (SubScreenExec
// links the Sscrn REL and chains into it; SubScreenExit applies the inventory changes — weapon
// swap, costume — when it closes), plus the radio call sequence (OpeSetOpenTerm).
// (D:/Bio4/Prog/sscrn.cpp)
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "event.h"
#include "dbg_button.h"
#include "item.h"
#include "cockpit.h"
#include "mercenaries.h"
#include "sce.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "obj.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main_sub.h"
#include "main.h"
#include "pad.h"
#include "scheduler.h"
#include "snd.h"
#include "db_log.h"
#include "datactrl.h"
#include "room_data.h"
#include "view.h"
#include "motion.h"
#include "model.h"
#include "sscrn.h"

extern "C" {
void OSReport(const char* fmt, ...);
void* memset(void* dst, int c, unsigned int n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, unsigned int n);
}

// The original cUnit::beginEvent takes an int (every caller passes one: sce_com's
// cManager<T>::beginEvent(int) loops, OpeSetOpenTerm's `pPL->beginEvent(0)`); the shared cUnit
// declaration still has the no-argument form, so the call goes through this view of the vtable.
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)

// Struct-member view of the cModel manager pointers: a plain scalar store lets the scheduler hoist
// the following pG load above it (the read.cpp EmInitFunc trick).
struct MgrPtr {
    void* p;
};
#define MGR_PTR(g) (((MgrPtr*) &(g))->p)

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

#define SS_ARAM 0xD00000
#define SS_ARAM_SIZE 0x300000

#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        MtxPtr s_ = (src);               \
        int i_ = 3;                      \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        while (i_--) {                   \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

SubScreenWork SubScreenWk;
IDSystem IdSub;
IDSystem IdNum;

// Save-game bytes the sub screen keeps (one word: SubScreenWk.save).
int SscrnDataSize()
{
    return 4;
}

// Writes the sub screen's save word.
void SscrnDataSave(u32* dst)
{
    *dst = SubScreenWk.save;
}

// Reads the sub screen's save word.
void SscrnDataLoad(u32* src)
{
    SubScreenWk.save = *src;
}

// Game start: loads the sub screen REL ("rel/Sscrn.rel"), the common data ("SS/<lang>/ss_cmmn.dat")
// and the puzzle data into the sub screen ARAM area, remembering each file's offset.
void SubScreenAramRead()
{
    SubScreenWork* wk = &SubScreenWk;
    int stat;
    int size;
    int req;

    wk->aramSize = 0;
#line 119 "D:/Bio4/Prog/sscrn.cpp"
    req = DVD_READ_N("rel/Sscrn.rel", 0, SS_ARAM, 0, 0, 9);
    wk->pPreplfOffs = wk->aramSize;
    Dvd.ReadCheck(req, &stat, &size, (void**) &wk->p_module);
    wk->aramSize += size;
    sscrnDataFilename(wk, "ss_cmmn.dat");
#line 130 "D:/Bio4/Prog/sscrn.cpp"
    req = DVD_READ_N(wk->path, 0, SS_ARAM + wk->aramSize, 0, 0, 9);
    wk->pCommonOffs = wk->aramSize;
    Dvd.ReadCheck(req, &stat, &size, 0);
    wk->aramSize += size;
    sscrnDataFilename(wk, "ss_pzzl.dat");
#line 140 "D:/Bio4/Prog/sscrn.cpp"
    req = DVD_READ_N(wk->path, 0, SS_ARAM + wk->aramSize, 0, 0, 9);
    wk->pzzlOfs = wk->aramSize;
    Dvd.ReadCheck(req, &stat, &size, 0);
    wk->aramSize += size;
    OSReport("SubScrn Data: 0x%08x\n", wk->aramSize);
    OSReport("SubScrn Free: 0x%08x\n", SS_ARAM_SIZE - wk->aramSize);
}

// Writes the language directory ("jpn" / "eng" / "ger" / "fra" / "esp" / "ita") into wk->path.
void sscrnSetLanguage(SubScreenWork* wk, int lang)
{
    char* p = strchr(wk->path, '/') + 1;

    switch (lang) {
    case 0:
        strncpy(p, "jpn", 3);
        break;
    case 1:
        strncpy(p, "eng", 3);
        break;
    case 2:
        strncpy(p, "eng", 3);
        break;
    case 3:
        strncpy(p, "ger", 3);
        break;
    case 4:
        strncpy(p, "fra", 3);
        break;
    case 5:
        strncpy(p, "esp", 3);
        break;
    case 6:
        strncpy(p, "ita", 3);
        break;
    default:
        strncpy(p, "jpn", 3);
        break;
    }
}

// Replaces the file name part of wk->path.
void sscrnDataFilename(SubScreenWork* wk, const char* name)
{
    strcpy(strrchr(wk->path, '/') + 1, name);
}

// Game start: language path, ARAM data, attache case size / map mode reset, the radio (ope) state
// cleared with message set 0x18; then the room init.
void SubScreenGameInit()
{
    SubScreenWork* wk = &SubScreenWk;

    strcpy(wk->path, "SS/___/");
    sscrnSetLanguage(wk, pSys->language);
    wk->relAddr = 0;
    SubScreenAramRead();
    wk->board_next = 0;
    wk->board_size = 0;
    wk->map_mode = 0;
    memset(&pG->ope_x82E8, 0, 0x44);
    pG->ope_mdt_no = 0x18;
    SubScreenRoomInit();
}

// Room start: sub screen closed and armed (Status_flg[0] 0x02000000 = may open), attache case
// size from the case items 0x7C..0x7F owned (0 for Ashley), map manager room init.
void SubScreenRoomInit()
{
    SubScreenWork* wk = &SubScreenWk;

    wk->type = 0;
    wk->flags = 0;
    wk->close_flag = 0;
    wk->wait = 0;
    if (ItemMgr.search(0x7C)) {
        wk->board_size = 0;
    }
    if (ItemMgr.search(0x7D)) {
        wk->board_size = 1;
    }
    if (ItemMgr.search(0x7E)) {
        wk->board_size = 2;
    }
    if (ItemMgr.search(0x7F)) {
        wk->board_size = 3;
    }
    wk->board_next = wk->board_size;
    if (pG->pl_type == 1) {
        wk->board_next = 0;
        wk->board_size = 0;
    }
    StaFlagOn(pG, STA_SSCRN_ENABLE);
    StaFlagOff(pG, STA_SUB_SCRN);
    StaFlagOff(pG, STA_SSCRN_REQUEST);
    MapMgr.roomInit();
}

// Blocks the sub screen from opening for `frames` frames (events, item pick-ups).
void SubScreenWait(int frames)
{
    SubScreenWk.wait = frames;
}

// Per frame (game loop): when the player (and Ashley) live, the screen is armed and the player
// state allows (subScrCheck), the inventory key (0x100000) or map key (0x200000, unless the map is
// disabled by Status_flg[2] 0x00200000) opens it; an opened screen starts the SubScreenExec task
// in slot 1 (or is cancelled when the slot is busy).
void SubScreenCall()
{
    SubScreenWork* wk = &SubScreenWk;

    if ((s16) pG->pl_life <= 0) {
        return;
    }
    if (pSUB && pSUB->id == 3 && (s16) pG->ashley_life <= 0) {
        return;
    }
    if (!StaFlagChk(pG, STA_SSCRN_ENABLE)) {
        return;
    }
    if (pPL->subScrCheck() == 1) {
        wk->wait--;
        if (wk->wait > 0) {
            return;
        }
        wk->wait = 0;
        if (Key.trg & 0x100000) {
            SubScreenOpen(SS_OPEN_NORMAL, 0);
        } else if (Key.trg & 0x200000) {
            if (!StaFlagChk(pG, STA_MAP_DISABLE)) {
                SubScreenOpen(SS_OPEN_MAP, 0);
            }
        }
    }
    if (wk->type) {
        StaFlagOff(pG, STA_SSCRN_ENABLE);
        if (TaskExec(1, SubScreenExec, 0) == 0) {
            SubScreenMiss();
            StaFlagOn(pG, STA_SSCRN_ENABLE);
        }
    }
}

// Map stage index from the story flags: 0 village, 1 after the church, 2 castle, 3 island.
int sscrnStageNo()
{
    if (ScfFlagChk(pG, SCF_ST3_IN)) {
        return 3;
    } else if (ScfFlagChk(pG, SCF_ST2_IN)) {
        return 2;
    } else if (ScfFlagChk(pG, SCF_ST1_MAP_DAY)) {
        return 1;
    }
    return 0;
}

// Map room number: the church-interior variants 0x111.. map onto their base rooms (-0x10).
u16 sscrnRoomNo(u16 room)
{
    switch (room) {
    case 0x111:
    case 0x112:
    case 0x113:
    case 0x118:
    case 0x119:
    case 0x11A:
    case 0x11B:
        return room - 0x10;
    }
    return room;
}

// Requests the sub screen of `type` (SS_OPEN_*: inventory, map, terminal / radio, shop...): flags
// bit0 = inside an event (SceEventStart), else the game is frozen (Stop_flg saved, keys stopped);
// bit1 is added when Ashley is carried. Returns 0 when one is already requested (Status_flg[2]
// 0x04000000).
int SubScreenOpen(int type, int flags)
{
    SubScreenWork* wk = &SubScreenWk;

    if (StaFlagChk(pG, STA_SSCRN_REQUEST)) {
        return 0;
    }
    StaFlagOn(pG, STA_SSCRN_REQUEST);
    wk->type = type;
    wk->flags = flags;
    wk->close_flag = 0;
    wk->model_flag = 0;
    if (flags & 1) {
        SceEventStart(0);
    } else {
        if (StaFlagChk(pG, STA_PL_BOAT)) {
            wk->flags = flags | 2;
        }
        wk->stop_bak = pG->Stop_flg;
        pG->Stop_flg = 0xFFFFFFFF;
        KeyStop(0xEFCF0000);
        SpfFlagOff(pG, SPF_ID_SYSTEM);
    }
    return 1;
}

// Cancels an open request (task slot busy): unfreezes / ends the event.
void SubScreenMiss()
{
    SubScreenWork* wk = &SubScreenWk;

    if (wk->flags & 1) {
        SceEventEnd(0);
    } else {
        pG->Stop_flg = wk->stop_bak;
    }
    wk->flags = 0;
    wk->type = 0;
    StaFlagOff(pG, STA_SSCRN_REQUEST);
}

// Sub screen task (slot 1): sounds down, fade to black, the room ids / effects hidden, the game
// heap swapped to ARAM and the sub screen data swapped in (MemorySwap), fonts and shared id data
// set up for the screen type, the Sscrn REL linked, then TaskChain into its prolog (the DLL runs
// the screen and calls SubScreenExit when done).
void SubScreenExec()
{
    SubScreenWork* wk = &SubScreenWk;
    int step = 0;
    int cnt = 0;

    for (;;) {
        switch (step) {
        case 0:
            SndSubScreenInit();
            wk->str_id = 0;
            if (!(wk->type & 0x20)) {
                SndCall(0, 2, 0, 0, 0, 0);
            }
            wk->alpha_flag = 0;
            wk->alpha_cnt = 0;
            if (pSUB && pSUB->id == 3) {
                wk->healing = SubCharCheckHealing();
            } else {
                wk->healing = 0;
            }
            StaFlagOn(pG, STA_SUB_SCRN);
            StaFlagOff(pG, STA_CAMERA);
            SpfFlagOn(pG, SPF_ACTBTN);
            SpfFlagOff(pG, SPF_ESP);
            MTX_COPY(pPL->mat, wk->pl_mat);
            if (pSUB) {
                MTX_COPY(pSUB->mat, wk->sub_mat);
            }
            wk->camera_bak = pG->Cam;
            step++;
            wk->stage = sscrnStageNo();
            wk->room_no = sscrnRoomNo(pG->room_id);
            FadeSetW(0, 3, 0, 0);
        case 1:
            if (Fade[0].flags & 1) {
                break;
            }
            step++;
        case 2:
            pG->weapon_no = WeaponId2WeaponNo(ItemMgr.m_wep_id);
            pG->weapon_type = WeaponId2WeaponType(ItemMgr.m_wep_id);
            if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
                CamCtrl.saveScopeParam();
                CamCtrl.endScope();
                wk->scope_flag = 1;
                if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
                    wk->scope_flag = 2;
                    StaFlagOff(pG, STA_THERMO_GRAPH);
                }
            } else {
                wk->scope_flag = 0;
            }
            if (StaFlagChk(pG, STA_BINOCULAR)) {
                CamCtrl.GetBinocularIDAddr(&wk->binoA, &wk->binoB);
                CamCtrl.LowerBinocular();
                wk->binocular_flag = 1;
            } else {
                wk->binocular_flag = 0;
            }
            if (ItemMgr.num(0xFE)) {
                wk->jacket_flag = 1;
            } else {
                wk->jacket_flag = 0;
            }
            wk->noBullet = 0;
            {
                ItemInfo info;
                itemInfo(ItemMgr.m_wep_id, &info);
                if (info.type == 3) {
                    if (ItemMgr.bulletNumCurrent() == 0) {
                        wk->noBullet = 1;
                    }
                }
            }
            {
                u32 t = StaFlagChk(pG, STA_SUSPEND);
                wk->suspend_flag = t;
            }
            StaFlagOff(pG, STA_SUSPEND);
            wk->disp_bak = pG->Disp_flg;
            BitSet(pG->Disp_flg, 0xFFFFFFFF);
            DpfFlagOff(pG, DPF_COCKPIT);
            DpfFlagOff(pG, DPF_ID_SYSTEM);
            DpfFlagOff(pG, DPF_MESSAGE);
            DpfFlagOff(pG, DPF_ESP);
            StaFlagOn(pG, STA_ITEM_GET);
            cnt = 0;
            step++;
            break;
        case 3:
            if (cnt++ > 0) {
                step++;
            }
            break;
        case 4:
            if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
                IdTexRelease(TEX_OWNER_ID_EVENT);
            }
            Cckpt.getCountDown()->saveDisp();
            systemVISetBlack(1);
            ScreenReSize(640, 448);
            systemVISetBlack(0);
            DpfFlagOn(pG, DPF_TEX_RENDER);
            FadeKill(FADE_NO_SCENARIO);
            switch (wk->type) {
            case 2:
            case 0x10:
            case 0x20:
            case 0x40:
            case 0x80:
                break;
            default:
                FadeSetW(0x80000000, 3, 0, 0);
                break;
            }
            TaskSuspend(0);
            RoomData.stopRelData();
            wk->pBuf = pG->pStFnt;
            DC.m_data_ctrl_flag = 0;
            MemorySwap(wk->pBuf, SS_ARAM, SS_ARAM_SIZE);
            MemSuspendHeap(4);
            if (wk->type & 0x10) {
                wk->pHeapOffs = wk->aramSize + 0x50000;
            } else if (wk->type & 0x20) {
                wk->pHeapOffs = wk->pzzlOfs;
            } else {
                wk->pHeapOffs = wk->pzzlOfs + 0xE4000;
            }
            if (wk->type & 0x30) {
                MemCreateHeap(12, (u32) wk->pBuf + wk->pHeapOffs, (u32) wk->pBuf + SS_ARAM_SIZE);
            } else {
                MemCreateHeap(12, (u32) wk->pBuf + wk->pHeapOffs, (u32) wk->pBuf + 0x2E5E00);
            }
            MemSetCurrentHeap(12);
            if (wk->relAddr >= 0) {
                wk->relAddr = wk->pPreplfOffs + (u32) wk->pBuf;
                wk->pCmmn = (SsArc*) (wk->pCommonOffs + (u32) wk->pBuf);
                wk->pPzzl = (SsArc*) (wk->pzzlOfs + (u32) wk->pBuf);
            }
            wk->p_module = (OSModuleHeader*) wk->relAddr;
            {
                MessageControl* mes = &cMes;
                int i;
                for (i = 0; i < 16; i++) {
                    mes->Delete(i);
                }
            }
            if (pSys->language == 0) {
                cMes.setupFont(28, 28, (TEXPalette*) SS_ARC_PTR(wk->pCmmn, 4), 3);
            }
            cMes.setLayout(1, LAYOUT_SUBSCRN);
            cMes.setLayout(7, LAYOUT_SUBSCRN);
            if (wk->type == 0x20) {
                IdSub.gameInit(0x80);
            } else {
                IdSub.gameInit(0x200);
            }
            IdTexDataLoad(SS_ARC_PTR(wk->pCmmn, 6), TEX_OWNER_ID_SHARE);
            if (wk->type == 0x20) {
                IdNum.gameInit(0);
            } else {
                IdNum.gameInit(0x1B2);
            }
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 7), 0xFF, 2, 0x13, 7, 0);
            if (!(wk->type & 0x10)) {
                IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xB), 0xFF, 0, 0xF, 0, 0);
            }
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 0x11), 0xFF, 4, 0x13, 9, 0);
            IdSub.set(SS_ARC_PTR(wk->pCmmn, 9), 0xFF, 1, 9, 3, 0);
            switch (wk->type) {
            case 2:
                IdSys.dispSw(0x21, 0);
            case 4:
            case 0x40:
            case 0x80:
                IdSys.dispSw(0x21, 1);
                IdSub.dispSw(0, 0);
                break;
            case 0x20:
                IdSub.dispSw(1, 0);
                IdSub.dispSw(0, 0);
                IdSub.dispSw(2, 0);
                break;
            case 0x10:
                IdSub.dispSw(1, 1);
                IdSub.dispSw(2, 1);
                break;
            default:
                IdSys.dispSw(0x21, 1);
                IdSub.dispSw(0, 1);
                break;
            }
            Cckpt.m_LifeMeter.fix(0);
#line 808 "D:/Bio4/Prog/sscrn.cpp"
            wk->pExamDat = MEM_ALLOC(0x3E800, 1, 13);
            if (wk->type == 2) {
                wk->menu_next = 2;
                wk->menu_no = 2;
            } else {
                wk->menu_next = 1;
                wk->menu_no = 1;
            }
            wk->Loop = 1;
            wk->wait_cnt = 0;
            LightMgr.inSscrn();
            LightMgr.create(0, 9, -2, 0);
            {
                int i;
                for (i = 0; i < 8; i++) {
                    wk->p_light[i] = 0;
                }
            }
            {
                void* bss;
                if (wk->p_module->bssSize == 0) {
                    bss = 0;
                } else {
#line 834 "D:/Bio4/Prog/sscrn.cpp"
                    bss = MEM_ALLOC(wk->p_module->bssSize, 1, 13);
                }
                DLL_Link(wk->p_module, bss);
            }
            wk->pzzl_debug_open = 0;
            wk->debugMode = pG->debug_mode;
            {
                int v = 1;
                if (DbgFlagChk(pG, DBG_PROC_BAR) == 0) {
                    v = 0;
                }
                wk->debug_flg_bak = v;
            }
            step++;
            DbgFlagOff(pG, DBG_PROC_BAR);
        case 5:
            SpfFlagOff(pG, SPF_KEY);
            TaskChain(wk->p_module->prolog, 0);
            break;
        }
        TaskSleep(1);
    }
}

// Unlinks the Sscrn REL, swaps the game memory back and restarts the room REL.
void SubScreenExitCore(SubScreenWork* wk)
{
    if (StaFlagChk(pG, STA_SUB_SCRN)) {
        MapMgr.roomInit();
        DLL_Unlink(wk->p_module);
        wk->relAddr = 0;
        MemDestroyHeap(12);
        MemSignalHeap(4);
        MemSetCurrentHeap(4);
        MemorySwap(wk->pBuf, SS_ARAM, SS_ARAM_SIZE);
        DC.m_data_ctrl_flag = 1;
        RoomData.restartRelData();
        MGR_PTR(cModel::mm) = &ModInfoMgr;
        MGR_PTR(cModel::pm) = &PartsMgr;
        StaFlagOff(pG, STA_SUB_SCRN);
    }
}

// Fade from black to clear with the clear word passed in: SubScreenExit sets it to 0 before
// cMes.roomInit() (the `li r30,0` before that call), which keeps the zero a loop-body pseudo that
// loop.c does not hoist and does not merge with the `type = flags = 0` zero (the FadeSetW inline's
// own zero constant is combined with it by combine_movables and hoisted to the prologue).
static inline void FadeSetBlackOut(u32 clear, u32 time, u32 z, int late)
{
    FadeColorPair col;

    *(u32*) &col.start = 0xFF;
    *(u32*) &col.end = clear;
    FadeSet(0x80000000, &col.start, &col.end, time, z, late);
}

// Closing task (chained by the DLL): fade, core exit, then applies what changed in the inventory —
// a different equipped weapon / type / upgrade level is reloaded (weaponRelease / Load / Init),
// the armor costume, ammo display; unfreezes the game or ends the event; fades back in.
void SubScreenExit()
{
    SubScreenWork* wk = &SubScreenWk;
    int step = 0;
    int cnt = 0;
    int wepNo = 0;
    int wepType = 0;
    int wepLv = 0;

    for (;;) {
        switch (step) {
        case 0:
            if (!(wk->close_flag & 8)) {
                SndCall(0, 3, 0, 0, 0, 0);
            }
            cMes.Delete(0);
            wepNo = WeaponId2WeaponNo(ItemMgr.m_wep_id);
            wepType = WeaponId2WeaponType(ItemMgr.m_wep_id);
            if (ItemMgr.pArm) {
                wepLv = ItemMgr.pArm->bullet >> 13;
            } else {
                wepLv = 0;
            }
            cnt = 0;
            step++;
            break;
        case 1:
            if (cnt++ > 0) {
                step = 2;
            }
            break;
        case 2:
            SubScreenExitCore(wk);
            cnt = 0;
            step = 3;
            sscrnDataFilename(wk, "ss_pzzl.dat");
#line 979 "D:/Bio4/Prog/sscrn.cpp"
            Dvd.ReadCheck(DVD_READ_N(wk->path, 0, SS_ARAM + wk->pzzlOfs, 0, 0, 9), 0, 0, 0);
            DpfFlagOff(pG, DPF_TEX_RENDER);
            break;
        case 3:
            if (cnt++ > 0) {
                step++;
            }
            break;
        case 4:
            if (pG->pl_type != 1 && (pG->weapon_no != wepNo || pG->weapon_type != wepType || pG->bullet_type != wepLv)) {
                cPlayer* pl;
                if (wk->flags & 2) {
                    ItemMgr.arm(0);
                    wepLv = 0;
                    wepNo = WeaponId2WeaponNo(ItemMgr.m_wep_id);
                    wepType = WeaponId2WeaponType(ItemMgr.m_wep_id);
                }
                pl = pPL;
                SndBlkStop(2);
                pl->weaponRelease();
                pl->weaponLoad(wepNo, wepType);
                pG->bullet_type = wepLv;
                pl->weaponInit();
                wk->scope_flag = 0;
                wk->noBullet = 0;
            }
            {
                int change = 0;
                if (pG->pl_type == 0) {
                    if (ItemMgr.num(0xFE)) {
                        change = wk->jacket_flag == 0;
                    } else if (wk->jacket_flag == 1) {
                        change = 1;
                    }
                }
                if (change) {
                    PlSetCostume();
                    PlChangeData();
                }
            }
            systemVISetBlack(1);
            ScreenReSize(512, 448);
            systemVISetBlack(0);
            pG->Cam = wk->camera_bak;
            View.move();
            BitSet(pG->Disp_flg, wk->disp_bak);
            if (wk->binocular_flag == 0) {
                SpfFlagOff(pG, SPF_KEY);
            }
            StaFlagOff(pG, STA_ITEM_GET);
            if (wk->suspend_flag) {
                StaFlagOn(pG, STA_SUSPEND);
            }
            {
                u32 i;
                for (i = 0; i < 10; i++) {
                    if (pPL->Wep->m_pWep) {
                        pPL->Wep->m_pWep->move();
                    }
                }
            }
            IdSys.dispSw(0x21, 1);
            Cckpt.m_LifeMeter.fix(0);
            if (wk->scope_flag) {
                CamCtrl.startScope(0, 0);
                CamCtrl.loadScopeParam();
            }
            if (wk->binocular_flag) {
                CamCtrl.HoldBinocular(wk->binoA, wk->binoB, 0, 0);
            }
            if (wk->noBullet) {
                if (ItemMgr.bulletNumCurrent()) {
                    PlReloadBullet();
                }
            }
            {
                u32 clear = 0;
                cMes.roomInit();
                if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
                    mercId.set();
                }
                {
                    Cockpit* ck = &Cckpt;
                    ck->countDown.loadDisp();
                }
                FadeSetBlackOut(clear, 3, 0, 0);
            }
            TaskSignal(0);
            SndSubScreenExit();
            StaFlagOn(pG, STA_SSCRN_ENABLE);
            StaFlagOn(pG, STA_CAMERA);
            StaFlagOff(pG, STA_SSCRN_REQUEST);
            {
                u32 mode;
                if (wk->scope_flag == 2) {
                    mode = 2;
                } else if (CamCtrl.areaNo != -1) {
                    mode = 1;
                } else {
                    mode = 0;
                }
                LightMgr.outSscrn(mode);
            }
            if (wk->flags & 1) {
                SceEventEnd(0);
            } else {
                pG->Stop_flg = wk->stop_bak;
            }
            wk->type = 0;
            wk->flags = 0;
            pG->debug_mode = wk->debugMode;
            if (wk->debug_flg_bak) {
                DbgFlagOn(pG, DBG_PROC_BAR);
            }
            step++;
        case 5:
            TaskExit();
            break;
        }
        TaskSleep(1);
    }
}

// Current radio (ope) message set number.
int OpeGetMdtNo()
{
    return pG->ope_mdt_no;
}

// Selects radio message set `no` and marks it heard (ope_mdt_bits).
void OpeSetMdtNo(u32 no)
{
    u32* tbl = pG->ope_mdt_bits;

    BitOn(tbl[no >> 5], 0x80000000 >> (no & 0x1F));
    pG->ope_mdt_no = no;
}

// Applies the pending radio message set (SubScreenWk.opeMdtNo). Returns it.
int OpeMdtSetInit()
{
    int no = SubScreenWk.opeMdtNo;

    OpeSetMdtNo(no);
    return no;
}

// Radio caller type shown on the screen (Hunnigan / Saddler...).
void OpeOwTypeSet(u8 type)
{
    pG->ope_ow_type = type;
    pG->ope_x82FC = 0;
}

// setPos through an inline helper: the caller's `&pos` (a `(plus vsv N)`) is substituted for the
// read-only pointer parameter in the hard-register argument set (fresh `addi r4, r1, 0x68`, never
// a pseudo), so the following `Vec* r = &pos` for setAng is a fresh pseudo that cse cannot merge
// with it (`addi r9, r1, 0x68`, `stfs f31, 8(r9)`). See docs/matching.md "FadeSet colour pair".
static inline void PlSetPosW(cPlayer* pl, Vec* v)
{
    pl->setPos(v);
}

// Scenario: the radio call `no` — waits for the player to be free, optionally moves him to
// (x, y, z, ang), starts an event, plays the radio stream (strTbl block), the "take out the radio"
// motion with the radio model in the left hand (parts 0x10), then opens the terminal sub screen
// (SS_OPEN_TERM); the skip key cancels. Restores the position afterwards.
void OpeSetOpenTerm(int no, f32 x, f32 y, f32 z, f32 ang)
{
    SubScreenWork* wk = &SubScreenWk;
    wk->cancel = 0;
    cPlayer* pl = pPL;
    int strTbl[24] = {3, 3, 0x33, 3, 0x33, 3, 3, 0x33, 3, 0x33, 0x33, 3, 3, 3, 0x33, 3, 3, 3, 3, 0x33, 3, 3, 3, 3};
    Vec pos;
    Vec rot;
    int i;

    while (pl->checkEvent() != 1) {
        SceSleep(1);
    }
    if (x != 0.0f) {
        wk->posBak = pPL->pos;
        wk->angBak = pPL->ang;
        pos.x = x;
        pos.y = y;
        pos.z = z;
        PlSetPosW(pPL, &pos);
        {
            Vec* r = &pos;
            pos.x = 0.0f;
            pos.y = ang;
            r->z = 0.0f;
            pPL->setAng(r);
        }
    }
    SceEventStart(0);
    SysFlagOn(pG, SYS_SCREEN_STOP);
    wk->pObjWep = 0;
    wk->opeMdtNo = no;
    OpeMdtSetInit();
    BEGIN_EVENT(pl, 0);
    pl->setNoSuspend(1);
    PlSetEyeMode(1);
    wk->sndId = SndStrPlayBlock(1, strTbl[no], 0.0f);
    MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlayer, 0x79), 0, 0, 0x201, 0);
    SceSleep(1);
    SysFlagOff(pG, SYS_SCREEN_STOP);
    for (i = 0; i <= 20; i++) {
        if (Key.trg & 0x20000000) {
            OpeSetOpenTermCancel();
            goto END;
        }
        SceSleep(1);
    }
    wk->pObjWep = (cObjWep*) ObjMgr.createBack(0xB);
    if (wk->pObjWep == 0) {
        pLog->err(0, 0, "OpeSetOpenTerm cObjWep CREATE FAILED");
        return;
    }
    if (wk->pObjWep->modelInit(PL_ARC_PTR(pG->pPlayer, 0x77), PL_ARC_PTR(pG->pPlayer, 0x78)) == 0) {
        pLog->err(0, 0, "OpeSetOpenTerm modelInit() failed.");
        ObjMgr.destroy(wk->pObjWep);
        return;
    }
    {
        pos.x = 111.0f;
        pos.y = -22.0f;
        pos.z = 66.0f;
        rot.x = -0.48869219f;
        rot.y = 0.31415927f;
        rot.z = -0.73303829f;
        wk->pObjWep->parentSet(pl, 0x10, &pos, &rot);
    }
    wk->pObjWep->setNoSuspend(1);
    pl->setLeftHand(2);
    while (MotionGetState(pl) == 0) {
        if (Key.trg & 0x20000000) {
            OpeSetOpenTermCancel();
            goto END;
        }
        SceSleep(1);
    }
    SubScreenOpen(SS_OPEN_TERM, 0);
    SubScreenWait(0);
    SceSleep(1);
END:
    OpeSetOpenTermEnd();
    // COMPILER-DIFF: tie (global-alloc live length): x (4 refs / 216 insns) and z (2 / 54) both truncate
    // to priority 370 and the lower pseudo (x) took f30; the original allocated z first. One codeless
    // real insn inside x's range but past z's death makes it 217 -> 368.
    asm("" : "=m"(pos.x));
    if (x != 0.0f) {
        pPL->setPos(&wk->posBak);
        pPL->setAng(&wk->angBak);
    }
    SysFlagOff(pG, SYS_SCREEN_STOP);
    SceEventEnd(0);
}

// The radio call was skipped (SubScreenWk.cancel).
void OpeSetOpenTermCancel()
{
    SubScreenWk.cancel = 1;
}

// Radio call end: stream stopped, radio model removed, hand restored, fade back in.
void OpeSetOpenTermEnd()
{
    SubScreenWork* wk = &SubScreenWk;
    cPlayer* pl = pPL;

    SndStrStopBlock(wk->sndId);
    if (wk->pObjWep) {
        ObjMgr.destroy(wk->pObjWep);
        pl->setLeftHand(0x63);
        wk->pObjWep = 0;
    }
    PlSetEyeMode(0);
    FadeSetW(0x80000000, 3, 0, 0);
    FadeKill(FADE_NO_ROOM);
    FadeSetW(0x80000001, 10, 0, 0);
}

// The next unit (lib/ppcdown.c, an SDK library) starts 32-byte aligned in .text and .bss and the
// split object carries the padding: 12 zero bytes after cManager<cMap>::roomInit in .text and
// 0x1C bytes of .bss after IdNum. The .text gap comes from lib/ppcdown.s's `.balign 32` (a
// `.long 0, 0, 0` here would land before the folded roomInit instantiation); the .bss gap is a
// zero-initialised static referenced only by a never-called inline (the dmg.cpp trick).
static u8 sscrn_pad[0x1C];
// Keeps the 0x1C-byte pad in .bss (matching helper).
static inline u8* sscrnPad()
{
    return sscrn_pad;
}
