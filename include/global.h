#ifndef GLOBAL_H
#define GLOBAL_H

#include "types.h"
#include "vec.h"
#include "camera.h"

// Archive header at pG->pCore: a table of file offsets to the sub-files. Only the entries that
// matched units use are named.
struct ArcFile {
    u8 pad_0[0x10];
    u32 ofs_10;   // 0x10  specular data (read: CoreDataRead -> SpecularInit)
    u32 ofs_14;   // 0x14  core effect data (eff_sys: EspDataLoad owner 0)
    u32 ofs_18;   // 0x18  room texture data (room_tex)
    u32 ofs_1C;   // 0x1C  vibration pattern table (pl_dmg: VibSetData)
    u32 ofs_20;   // 0x20  obstacle model bin (obj20 SetObaModel)
    u32 ofs_24;   // 0x24  obstacle model tpl
    u32 ofs_28;   // 0x28  message tables (mes: MesData.m_Data[0..2])
    u32 ofs_2C;   // 0x2C  core light data (game: cLightMgr::roomInit core cLit)
    u32 ofs_30;   // 0x30  core camera data (game: CameraControl::CoreDataRead)
    u32 ofs_34;   // 0x34
    u32 ofs_38;   // 0x38
    u32 ofs_3C;   // 0x3C  light path data (game: cLightMgr::initPath)
    u32 ofs_40;   // 0x40  global illumination texture (read: CoreDataRead -> GlobalIlmTexInit)
    u32 ofs_44;   // 0x44  specular data 2..4 (SpecularInit)
    u32 ofs_48;   // 0x48
    u32 ofs_4C;   // 0x4C
    u32 ofs_50;   // 0x50  debug effect data (eff_sys: EspDataLoad owner 0xD1)
    u32 ofs_54;   // 0x54  message table type 3 (mes: MesData.m_Data[3])
    u32 ofs_58;   // 0x58  item examine light cuts 0..4 (examine ItemExamine::init)
    u32 ofs_5C;   // 0x5C
    u32 ofs_60;   // 0x60
    u32 ofs_64;   // 0x64
    u32 ofs_68;   // 0x68
    u32 ofs_6C;   // 0x6C  system message table (dvd: MesData.m_Data[4])
    u32 ofs_70;   // 0x70  TV-mode message table (tv_mode)
    u32 ofs_74;   // 0x74  HUD id textures (cockpit: IdTexDataLoad(.., 4))
    u32 ofs_78;   // 0x78
    u32 ofs_7C;   // 0x7C  life meter id data (cockpit, type 0x21)
    u32 ofs_80;   // 0x80  action button id data (cockpit, type 0x20)
    u32 ofs_84;   // 0x84  count-down id data (cockpit, type 0x23)
    u32 ofs_88;   // 0x88  HUD id data type 0x30 (cockpit)
    u32 ofs_8C;   // 0x8C
    u32 ofs_90;   // 0x90
    u32 ofs_94;   // 0x94  message window id data (cockpit, type 0x2F)
    u32 ofs_98;   // 0x98  bullet icon id data (cockpit, type 0x32)
    u32 ofs_9C;   // 0x9C  sub-mission widget id data (stage)
};
// Sub-file `field` (an ofs_NN member) of the current archive.
#define ARC_PTR(field) ((void*) (pG->pCore->field + (u32) pG->pCore))

// Player archive at pG->pPlArc: a table of byte offsets to the player's sub-files (models, textures,
// motions, faces...). The pl_* units index it directly; the pointer is `ofs + (u32) arc`.
struct PlArc {
    u32 ofs[0x100];   // pl_knife indexes up to 0x87
};
#define PL_ARC_PTR(arc, no) ((void*) ((arc)->ofs[no] + (u32) (arc)))
// Model / motion data `no` of the player archive.
#define PL_ARC(no) PL_ARC_PTR(pG->pPlayer, no)
// Weapon archive (read: ReadWepData) at pG->pWepArc, indexed like the player archive.
#define WEP_ARC_PTR(no) PL_ARC_PTR(pG->pWep, no)
// Slot `idx` of player `pl`'s motion table (m_MotTbl) set to entry `no` of the weapon / player archive.
#define WEP_MOT(pl, idx, no) ((pl)->m_MotTbl[idx] = WEP_ARC_PTR(no))
#define PLA_MOT(pl, idx, no) ((pl)->m_MotTbl[idx] = PL_ARC_PTR(pG->pPlayer, no))
#define NO_MOT(pl, idx) ((pl)->m_MotTbl[idx] = (void*) 0)

// Room archive at pG->pRoomArc: offsets to its sub-files (GetDataExt finds them by tag; ctrl14 indexes it).
struct RoomArc {
    u32 ofs[0x10];
};
#define ROOM_ARC_PTR(arc, no) ((void*) (((RoomArc*) (arc))->ofs[no] + (u32) (arc)))

// TEV stage / texture map / texture coord counters the model renderer allocates from (pG+0x184).
struct GxStageWork {
    s32 tevStage;  // 0x00
    s32 texMap;    // 0x04
    s32 texCoord;  // 0x08
};

// Item left in a room (pG->save_item[256], game/sce_at.cpp), 16 bytes.
struct ITEM_SAVE_WORK {
    u8 item_type;          // 0x00  0 item area, 1 item handed to an area
    u8 item_at;          // 0x01
    s8 item_eff;       // 0x02
    u8 pad_3;
    u16 room_no;         // 0x04  0 = free
    u16 item_id;           // 0x06
    u16 item_num;          // 0x08
    s16 pos[3];       // 0x0A  / 10
};

// Position and kind of the last noise that enemies react to (PS2 EM_SE_INFO). Set together with STA_SE_BURST by
// the rung bell, explosions and door kicks; the enemy find checks read it.
struct EM_SE_INFO {
    Vec pos;           // 0x00
    u8 type;           // 0x0C  0, 1 or 2; 2 = bell rung
    u8 pad_0D[3];
};

// One entry of the room enemy list (ESL, pG->Em_list: 256 entries of 0x20 bytes, game/em_set.cpp).
struct EmListData {
    u8 be_flag;       // 0x00  bit0: alive flag (EmListSetAlive), bit1: set (an enemy was created from it), bit2/bit3: set toggles  (PS2 EM_LIST.be_flag)
    u8 id;          // 0x01  enemy id (0 = empty entry, 0xF / 0x25 are created at the back of the work array)
    u8 type;        // 0x02  -> cModel::type
    u8 set;          // 0x03  -> cEm::x38D  -> cEm::set
    u32 flag;     // 0x04  -> cEm::flags_3C8  -> cEm::flag (PS2 EM_LIST.flag)
    u16 hp;         // 0x08
    u8 emset_no;//  (PS2 EM_LIST.emset_no; unused on GC, the list index is stored)
    u8 Character;          // 0x0B  -> cEm::x3D0  -> cEm::Character (PS2 EM_LIST.Character)
    s16 pos[3];     // 0x0C  * 10
    s16 rot[3];     // 0x12  * (pi / 0x4000)
    u16 room;       // 0x18  stage << 8 | room
    s16 Guard_r;        // 0x1A  * 1000 -> cEm::x3CC  -> cEm::Guard_r (PS2 EM_LIST.Guard_r)
    u8 pad_1C[4];
};

struct EmiData;   // embarrel.h (PS2 EMINFO_DATA)

// Global game work (`pG`, game/main.cpp). Offsets come from the cam_ctrl unit; extend the
// pads as other units reveal more fields, never rewrite.
struct GlobalWork {
    s32 IsDevConsole;          // 0x00  1 = development hardware (main: OSGetConsoleType & 0xF0000000)
    u8 shooting_mode;      // 0x04  shooting range mode (title: shoot_mode[] name table; em10/em39: 9999 damage, marker lines)
    u8 CardLastSelNo;            // 0x05  save file number last loaded/saved (card dataSelect)
    u8 pad_6[2];
    u32 CardStatus;                // 0x08  card flags (card: 4 loaded, 8/0x10/0x20/0x40/0x80 CardSave modes, bit 31 first check done; main: bit 31 saved into pRK->x3C)
    u8 pad_C[4];
    u64 card_serial;       // 0x10  serial of the card the save file came from (card)
    void* FontData;           // 0x18  ROM font header (dvd: RomFontSetting)
    s32 IsMessageInit;     // 0x1C  1 = the message system is usable (mes sets it; dvd error screen tests it)
    u8 Rno0;        // 0x20  game task step (game_func_tbl index; main_sub: 3/4/6 allow the blur filter)
    u8 Rno1;        // 0x21  sub step (room_jmp roomJumpExit clears x21..x23 with x20 = 4)
    u8 Rno2;
    u8 Rno3;
    u32 DblBufIdx;        // 0x24  double-buffer index into cModelInfo::pPosBuf/pNrmBuf (mirror)
    union {
        u16 RoomNo_next;     // 0x28  room id (stage << 8 | room) being entered (snd: room BGM / door tables)
        struct {
            u8 Stage_next; // 0x28  (sce_at sceAtFunc_door stores the door destination byte by byte)
            u8 Room_next;  // 0x29
        };
    };
    u8 Part_next;         // 0x2A  spawn point in the next room (room_jmp CRoomInfo::setNextPos clears it)
    u8 pad_2B;
    Vec NextPos;          // 0x2C  player position in the next room (room_jmp)
    f32 NextY;        // 0x38
    void* pStFnt;      // 0x3C  stage/event font buffer (mes: MessageControl::stageInit)
    void* pRoom;        // 0x40  current room archive (GetDataExt(pG->pRoomArc, "STB", 0))
    PlArc* pWep;        // 0x44  weapon archive (read: ReadWepData)
    struct ArcFile* pCore;       // 0x48  current archive: offsets to its sub-files (room_tex, tv_mode)
    void* pOption;     // 0x4C  SS/<lang>/option.dat (read: OptionDataRead)
    struct PlArc* pPlayer;       // 0x50  player archive (pl_leon/pl_push: model, motion, face data offsets)
    u32 System_flg;          // 0x54
    u32 Disp_flg;          // 0x58
    u32 game_start_time;         // 0x5C  OSTicksToSeconds at the last InitGameTime/SetGameTime
    u32 Debug_flg[4];      // 0x60  debug option bits ([2] 0x04000000 / [3] 0x00200000 shown in the title debug page)
    f32 Speed;         // 0x70  motion frame step per game frame (MotionSequenceCtrl: speed * Speed)
    CAMERA Camera;            // 0x74 .. 0x16C  (Cam.param at 0x118)
    u32 room_start_addr[1]; // 0x16C
    u32 Stop_flg;          // 0x170  stop flags (debug tools save/restore it)
    u32 Room_flg[4];       // 0x174  per-room flag words: [0] room scripts (pl_sub joyFireOn 0x20000000 in room 11C), [1] objRobo WalkHitCk bit31 = the statue caught the player, [2]/[3] cleared by SceAtWorkLoopInit every frame
    GxStageWork gxStage;   // 0x184  TEV stage / texmap / texcoord counters of the model renderer (mirror)
    Mtx mtxPalette[0xF8];  // 0x190  skinning matrix palette (trans.cpp calcWeightMat / MakeWeightPalette)
    u8 pad_3010[0x4F10 - 0x3010];  // 0x3010  GXTexObj texObj[0xF8] (trans.cpp GxWork view of 0x184..0x4F14)
    s32 prim_base;         // 0x4F10  primitive buffer: first entry of the current frame (debug PrimitiveBuffDisp)
    f32 prim_rate;         // 0x4F14  worst free ratio of the primitive buffer seen so far
    s32 prim_cnt;          // 0x4F18  entries used so far this frame
    s32 nPrim;          // 0x4F1C  entries per frame (game: ConsGetRoomValue(8), 0x8000 while stopped)
    void* RoomMes;        // 0x4F20  room message table (mes: MesData.m_Data[1])
    void* pCamCore;    // 0x4F24  core camera data ("B40x")
    void* pCamRoom;    // 0x4F28  room camera data ("B40x")
    void* Rtp;        // 0x4F2C  room "RTP" data (read: ReadAreaData)
    EmiData* pEmi;     // 0x4F30  room "EMI" data (PS2 EMINFO_DATA*)
    void* pOsd;        // 0x4F34  room "OSD" data
    s8 AreaNo;            // 0x4F38  block trigger area the player stands in (block.cpp), -1 = none
    u8 pad_4F39[3];
    EM_SE_INFO SeInfo;     // 0x4F3C  last noise enemies react to (Status_flg STA_SE_BURST); the rung bell point is pos
    u8 pad_4F4C[0x4F70 - 0x4F4C];
    Vec quake_ofs;         // 0x4F70
    u8 weapon_no_old;
    u8 door_se;            // 0x4F7D  door used to enter the room (index into the DSE door SE table)
    u16 time_bonus;     // 0x4F7E  seconds to add to the count-down (cockpit CountDown::move consumes it)
    u8 save_data_start_addr[4];  // 0x4F80  start of the save block (game: cGameSave copies 0x4F80..0x8678)
    s32 point;             // 0x4F84  difficulty point (game GameAddPoint, 0..0x2AF7)
    u8 Game_level;         // 0x4F88  adaptive difficulty rank 1..10 = point / 1000 (em2d/em10 branch on > 1/3/6/== 10)
    u8 game_mode_disp;     // 0x4F89  save header copy of game_mode (card makeSaveData writes it into the header's byte 0x3D) (PS2 SAVE_WORK game_mode_disp)
    u8 chapter;            // 0x4F8A  chapters ended (sce_com SceChapterEnd: SceSys chapter + 1)
    u8 pad_4F8B;
    u16 save_cnt;          // 0x4F8C  times saved (card makeSaveData increments it)
    u16 game_cnt;          // 0x4F8E  games cleared: nonzero = new round (merchant full tables; 1 = Merchant2ndRoundInit on load)
    u16 r_continue_cnt;    // 0x4F90  continues in this room (GameContinue increments; room jump / scene change clear it)
    u8 terminal_no;         // 0x4F92  save terminal (typewriter) number: the card save sets it (card.cpp), snd.cpp indexes the room BGM / stream tables with it + 1 (PS2 terminal_no)
    u8 game_country;           // 0x4F93  game language (main: pSys->language; title: language_tbl[])
    u32 play_time;         // 0x4F94  seconds (SetGameTime accumulates into it)
    u32 peseta;            // 0x4F98  money (ss_shop buy/sell, item pickups; PlSelect swaps it with peseta_bak)
    union {
        u16 room_id;       // 0x4F9C  stage << 8 | room as one halfword (obj14: room 004 test)
        struct {
            u8 stage_no;   // 0x4F9C
            u8 room_no;    // 0x4F9D
        };
    };
    u8 Part;       // 0x4F9E  spawn point in the current room (copied to Part_old / next_point)
    u8 JumpPoint;  // 0x4F9F  room jump point (title/room_jmp debug jump; room scripts branch on 1/2)
    union {
        u16 room_id_prev;  // 0x4FA0  room_id of the previous room (room_jmp CRoomInfo::setNextPos)
        struct {
            u8 stage_prev; // 0x4FA0  stage the current room data was loaded for (stage.cpp)
            u8 room_prev;  // 0x4FA1
        };
    };
    u8 Part_old;              // 0x4FA2  copy of x4F9E (room_jmp)
    s8 em_list_no;          // 0x4FA3  enemy list currently loaded (stage.cpp), -1 = none
    u16 pl_life;           // 0x4FA4  (compared as s16 by the debug tools)
    u16 pl_life_max;       // 0x4FA6
    u16 ashley_life;          // 0x4FA8  Ashley
    u16 ashley_life_max;      // 0x4FAA
    u8 pad_4FAC[4];
    u8 weapon_no;             // 0x4FB0  equipped weapon (cPlayer::weaponLoad(no, type))
    u8 weapon_type;           // 0x4FB1
    u8 bullet_type;        // 0x4FB2  equipped weapon slot num >> 13 (sscrn SubScreenExit re-arms when it changed)
    u8 weapon_lv_power;    // 0x4FB3  firepower tune level (em_dm_val: WeaponLevelTbl column, clamped to 7)
    u8 weapon_lv_speed;    // 0x4FB4  firing speed tune level (PlShotFrameTbl column; item cItemMgr::arm)
    u8 weapon_lv_blt;      // 0x4FB5  capacity tune level (item cItemMgr::arm)
    u8 pad_4FB6[2];
    u8 pl_type;      // 0x4FB8  player character: 0 Leon, 1 Ashley, 2 Ada, 3 HUNK, 4 Krauser, 5 Wesker, 6 Leon+Ashley
    u8 pl_costume;    // 0x4FB9  player costume (pl_leon: 2 = no cloth simulation)
    u8 weapon_lv_reload;      // 0x4FBA
    u8 game_costume;   // 0x4FBB  Ashley costume (pl_cloth: 1 = ribbon + lapels instead of skirt + sweater)
    u8 pad_4FBC[2];
    u16 pl_flag;        // 0x4FBE  bit0: player data changed (pl_sub PlSelect/PlSetCostume/PlChangeData)
    Vec pl_pos;           // 0x4FC0  player position kept across rooms (game.cpp saves pPL->pos, cPlayer::init reads it; SubCharInit places the partner from it too) (PS2 pl_pos; was `sub_pos`)
    f32 pl_ang_y;         // 0x4FCC  (PS2 pl_ang_y; was `sub_angle`)
    u8 pad_4FD0[0x500C - 0x4FD0];
    u32 Status_flg[4];     // 0x500C  game status bits ([3] 0x10000000: main_sub letterbox scissor)
    u32 Em_flg[12][8];    // 0x501C  per enemy list (emlist_no): one bit per list entry, set when the enemy died (em_set)
    u32 Item_flg[8];     // 0x519C  "ITEM_SET" flag words (t_flag; merchant: [0] bit 0x10000000 = item 0x40 sold)
    // 0x51BC  scenario progress bits set by the room scripts; the flag editor's SCENARIO page
    // (game/t_flag.cpp scf_s) names them, index 0 being bit 31 of word 0. DoorFlagInit presets bits
    // of words 3 to 5.
    u32 Scenario_flg[8];
    u32 Key_flg[2];        // 0x51DC  one bit per locked door (t_flag KEY_LOCK; sce_at SceAtWork::lockFlag)
    u32 Frame_cnt;         // 0x51E4  frame counter (em: `& 3` vs emset_no staggers per-enemy work; tools blink on % 30)
    u32 save_free_work[64];      // 0x51E8  scenario free words (sce_com SetFree/GetFree)
    EmListData Em_list[256];     // 0x52E8  enemy list (ESL file) read by stage.cpp
    ITEM_SAVE_WORK item_save[0x100];  // 0x72E8  items left in rooms (sce_at SceAtSetSaveItem)
    u32 ope_x82E8;         // 0x82E8  sub screen "Ope" block (sscrn: memset(&pG->ope_x82E8, 0, 0x44) in SubScreenGameInit)
    u8 ope_ow_type;        // 0x82EC  (sscrn OpeOwTypeSet)
    u8 pad_82ED[3];
    u32 ope_mdt_bits[3];   // 0x82F0  one bit per mdt number (sscrn OpeSetMdtNo)
    u32 ope_x82FC;         // 0x82FC  (sscrn OpeOwTypeSet clears it)
    s32 ope_mdt_no;        // 0x8300  (sscrn OpeGetMdtNo / OpeSetMdtNo; SubScreenGameInit: 0x18)
    u8 pad_8304[0x832C - 0x8304];
    u32 peseta_bak;        // 0x832C  the other character's money (PlSelect swaps it with peseta; r206 adds it back)
    s16 shootingScore[4];  // 0x8330  shooting range scores (game clearGlobalSaveData keeps 0x8330..0x8338 across the clear)
    u16 c_continue_cnt;             // 0x8338  (sce_com SceChapterEnd clears it with the kill/shot counters)
    u16 g_continue_cnt;             // 0x833A  (option: result screen counter next to x8338)
    u32 c_kill_cnt;        // 0x833C  enemies killed (em_set EmSetDieCnt)
    u32 g_kill_cnt;       // 0x8340
    u32 c_hit_cnt;           // 0x8344  (pl_wep PlWepHitCheck2: shots that hit something)
    u32 g_hit_cnt;          // 0x8348
    u32 c_shot_cnt;         // 0x834C  shots fired
    u32 g_shot_cnt;        // 0x8350
    u8 game_mode;          // 0x8354  difficulty: 1 VERY_EASY, 3 EASY, 5 NORMAL, 6 HARD (title game_mode_tbl; main systemWorkInit: 5)
    u8 pad_8355[3];
    s32 SaveKind;          // 0x8358  save kind passed to cGameSave::save (stage: 3 = no enemy list reload; -1 on continue)
    u8 pad_835C[0x8678 - 0x835C];
    s8 debug_mode;         // 0x8678  debug page number (t_page), 0xF = camera rail debug draw
    s8 debug_disp;         // 0x8679  debug page shown by the game (0 = off); t_page/t_sc_shot edit it
    u8 pad_867A[0x8680 - 0x867A];  // sizeof == 0x8680 (main: memclr_asm(pG, sizeof(GlobalWork)))
};

extern GlobalWork* pG;
extern GlobalWork Global;  // the instance pG points at (game/main.cpp); static initializers take its address

// The first room flag word read through a helper: a plain scalar access, not a member chain.
static inline u32* eventFlags() { return &pG->Room_flg[0]; }

// The system save block (game/main.cpp `SystemSave`, 0x38 bytes), which pSys also points at.
struct SYSTEM_SAVE_WORK {
    u32 Config_flg;       // 0x00  CFG_* bits (bit 30 = progressive / 60Hz screen scaling)
    u32 Extra_flg;        // 0x04  EXT_* bits: mercenaries characters, stages and extra content
    u8 language;          // 0x08  0 JP, 1/2/7 EN, 3 DE, 4 FR, 5 ES, 6 IT (dvd error messages)
    u8 eff_country;       // 0x09  1 US, 2..6 EU, 7 ? (dvd: disc id game name)
    u8 brightness;        // 0x0A  background brightness (Render_done -> Bg_brightness_set)
    u8 pad_type;          // 0x0B  Key_type_tbl row (controller layout)
    u8 SndMode;           // 0x0C  0 mono, 1 stereo, 2 DPL2 (Snd_get_sound_mode / SndSetOutputMode)
    u8 __ssd_padding0[3];
    u32 MercSysRoom[4];   // 0x10  mercenaries record per stage: score / 10 | mode << 28 | new << 31
    u32 MercSysRank[2];   // 0x20  mercenaries rank bits, 3 per (stage, character), MSB first
    u32 dummy32[4];       // 0x28
};
extern SYSTEM_SAVE_WORK SystemSave;

// stage_no/room_no read as one u16 (stage << 8 | room), as cRoomData::getRoomSavePtr wants it.
#define G_ROOM_ID (*(u16*) &pG->stage_no)

// Flag helpers: `f |= b` / `f &= ~b` through a reference. Not an aliasing device: the pG reload after a
// store is the compiler's own (docs/matching.md, "Compiler", mem-flags patch), and a plain `pG->x = v` is
// the form. They are kept where the original keeps consecutive updates of one word as separate
// read-modify-write pairs with the constant in its own register; use them where the asm shows that.
static inline void BitOn(u32& f, u32 b) { f |= b; }
static inline void BitOff(u32& f, u32 b) { f &= ~b; }

// Flag bit indices, names taken from a mix of t_flag.cpp names and PS2 symbols.
// t_flag.cpp had some typos that showed the names were manually entered there, instead of being
// generated from the enums in the code, with some even using outdated flag names from the games
// prototype ("HOOK_STALKING")
// PS2 symbols gave us proper names for them instead, but a few were shifted around during the port,
// along with new ones added for the new game modes in PS2.
// Any PS2-specific flag names have been skipped, and names that disagreed were checked to confirm
// usage before adding here.

// Debug_flg bits
enum DBG_FLAG {
    DBG_TEST_MODE = 0,
    DBG_SCR_TEST = 1,
    DBG_BACK_CLIP = 2,
    DBG_DBG_CAM = 3,
    DBG_SAT_DISP = 4,
    DBG_EAT_DISP = 5,
    DBG_EVENT_TOOL = 6,
    DBG_SLOW_ON = 7,
    DBG_SHADOW_POLYGON = 8,
    DBG_SCE_AT_DISP = 9,
    DBG_SCR2_TEST = 10,
    DBG_SHADOW_FRAME = 11,
    DBG_MIRROR_POLYGON = 12,
    DBG_GROUND_DISP = 13,
    DBG_SKELETON_DISP = 14,
    DBG_ESPTOOL_ONSCR = 15,
    DBG_CINESCO_OFF = 16,
    DBG_RTP_DISP = 17,
    DBG_WIRE_DISP = 18,
    DBG_YARARE_DISP = 19,
    DBG_CAM_AREA_OFF = 20,
    DBG_CLOTH_AT_DISP = 21,
    DBG_WIND_ON = 22,
    DBG_ESPTOOL_MEM_USE = 23,
    DBG_TEX_RENDER_ALL = 24,
    DBG_ESPTOOL_ONEM = 25,
    DBG_EMINFO_DISP = 26,
    DBG_LIGHT_TOOL = 27,
    DBG_EM_COUNT = 28,
    DBG_1d = 29,
    DBG_EM3F_ADA_SADLER = 30,
    DBG_1f = 31,
    DBG_COCKPIT_TOOL = 32,
    DBG_BOUNDING_DISP = 33,
    DBG_ADJUST_CAM = 34,
    DBG_FLAT_FLOOR = 35,
    DBG_OBJ_SKELETON = 36,
    DBG_DRAW_SH_TEX = 37,
    DBG_EM_NO_ATK = 38,
    DBG_NO_EST_CALL = 39,
    DBG_IN_ESP_TOOL = 40,
    DBG_TERM_TOOL = 41,
    DBG_WARN_LEVEL_LOW = 42,
    DBG_ID_TOOL = 43,
    DBG_TITLE_CHECK = 44,
    DBG_CAST_ERR_NO_DISP = 45,
    DBG_EMW_ERR_NO_DISP = 46,
    DBG_TIMER_STOP = 47,
    DBG_SCREEN_SHOT = 48,
    DBG_SERIES_SHOT = 49,
    DBG_ASHLEY_NO_CATCH = 50,
    DBG_33 = 51,
    DBG_34 = 52,
    DBG_35 = 53,
    DBG_36 = 54,
    DBG_37 = 55,
    DBG_38 = 56,
    DBG_39 = 57,
    DBG_3a = 58,
    DBG_3b = 59,
    DBG_3c = 60,
    DBG_3d = 61,
    DBG_3e = 62,
    DBG_3f = 63,
    DBG_ROOMJMP = 64,
    DBG_PROC_BAR = 65,
    DBG_SCA_VIEW = 66,
    DBG_OBA_VIEW = 67,
    DBG_SLOW_MODE = 68,
    DBG_NO_SCE_EXE = 69,
    DBG_SINGLE_DISK = 70,
    DBG_BUGCHECK_MODE = 71,
    DBG_NO_DEATH = 72,
    DBG_INF_BULLET = 73,
    DBG_NO_ENEMY = 74,
    DBG_BGM_STOP = 75,
    DBG_SE_STOP = 76,
    DBG_PL_LOCK_FOLLOW = 77,
    DBG_EM_NO_DEATH = 78,
    DBG_KAIOUKEN = 79,
    DBG_PAD_INFO = 80,
    DBG_UNDER_CONST = 81,
    DBG_EM_WEAK = 82,
    DBG_EM_LIFE_DISP = 83,
    DBG_SHADOW_LIGHT = 84,
    DBG_CAPTION_OFF = 85,
    DBG_TEST_MODE_CK = 86,
    DBG_LIGHT_ERR_CHECK = 87,
    DBG_EST_CALL_CHK = 88,
    DBG_GX_WARN_ALL = 89,
    DBG_GX_WARN_MIDIUM = 90,
    DBG_GX_WARN_SEVERE = 91,
    DBG_PL_NOHIT = 92,
    DBG_SE_ERR_ALL = 93,
    DBG_QUICK_EM_STOP = 94,
    DBG_AV_TEST = 95,
    DBG_INF_BULLET2 = 96,
    DBG_BATTLE_CAM = 97,
    DBG_PL_KLAUSER = 98,
    DBG_MAP_ZOOM_ORG = 99,
    DBG_SCISSOR_OFF = 100,
    DBG_LOG_OFF = 101,
    DBG_SCR_CHECK = 102,
    DBG_OBJ_SERVER = 103,
    DBG_START_ST2 = 104,
    DBG_ERROR_CK = 105,
    DBG_APP_USE_DBMEM = 106,
    DBG_REFRACT_CK = 107,
    DBG_EM_NO_DIE_FLAG = 108,
    DBG_START_ST3 = 109,
    DBG_START_LAST = 110,
    DBG_DOOR_SET_MODE = 111,
    DBG_EFF_NUM_DISP = 112,
    DBG_SET_HITMARK_ALL = 113,
    DBG_FOG_FAR_GREEN = 114,
    DBG_SSCRN_PROC_BAR = 115,
    DBG_NO_ETC_SET = 116,
    DBG_NO_DEATH2 = 117,
    DBG_NO_PARASITE = 118,
    DBG_ESP_CHK = 119,
    DBG_NO_EVENT = 120,
    DBG_NO_LASER_LINE = 121,
    DBG_DATA_113 = 122,
    DBG_SHOP_FULL = 123,
    DBG_ADA_OMAKE_EV = 124,
    DBG_FOG_FAR_COLOR = 125,
    DBG_TOOL_EMINFO_DISP = 126,
    DBG_CHAR_MACHINE = 127,
};

// Status_flg bits
enum STA_FLAG {
    STA_BG_OFF = 0,
    STA_PL_CHECK = 1,
    STA_PL_CHECK2 = 2,
    STA_MOVIE_ON = 3,
    STA_CUTCHG = 4,
    STA_MOVIE2_ON = 5,
    STA_SSCRN_ENABLE = 6,
    STA_CINESCO = 7,
    STA_PL_FIRE = 8,
    STA_09 = 9,
    STA_ACT_DONT_FIRE = 10,
    STA_DIEDEMO = 11,
    STA_BLUR = 12,
    STA_SUB_SCRN = 13,
    STA_CARD_ACCESS = 14,
    STA_PAD_SENSITIVE = 15,
    STA_LOOK_THROUGH = 16,
    STA_PL_ACTION = 17,
    STA_PL_INVISIBLE = 18,
    STA_EVENT = 19,
    STA_ASHLEY_HIDE = 20,
    STA_BINOCULAR = 21,
    STA_WATER_ALIVE = 22,
    STA_CAMERA = 23,
    STA_BLACKOUT = 24,
    STA_SCOPE_CAMERA = 25,
    STA_RIDE_GONDOLA = 26,
    STA_WIND_ON = 27,
    STA_PL_JUMP_OFF = 28,
    STA_MIRROR = 29,
    STA_SAND_ALIVE = 30,
    STA_SELF_SHADOW = 31,
    STA_PL_SE_FOOT = 32,
    STA_PL_SE_WHISTLE = 33,
    STA_SE_BURST = 34,
    STA_SUSPEND = 35,
    STA_TEX_RENDER = 36,
    STA_THERMO_GRAPH = 37,
    STA_CAMERA_IN_ROOM = 38,
    STA_NO_LIGHTMASK = 39,
    STA_PL_SPEAR_SET = 40,
    STA_PL_SWIM = 41,
    STA_PL_BOAT = 42,
    STA_WATER_CAMERA = 43,
    STA_PL_SWIM_CAMERA = 44,
    STA_PL_LADDER = 45,
    STA_CRITICAL = 46,
    STA_TAKEAWAY = 47,
    STA_PL_CATCHED = 48,
    STA_SHADOW_EQCOL = 49,
    STA_PL_CATCHHOLD = 50,
    STA_NEARCLIP_TOUCH = 51,
    STA_CAMERA_SET_ROOM = 52,
    STA_ROOM_RAIN = 53,
    STA_USE_CAST_SHADOW = 54,
    STA_PROC_SHD_TEX = 55,
    STA_ALPHA_DRAW2 = 56,
    STA_SET_BG_COLOR = 57,
    STA_ESPGEN45_SET = 58,
    STA_EFFEM2D_TEXRND = 59,
    STA_SUB_LADDER = 60,
    STA_SUBCHAR_CTRL = 61,
    STA_ITEM_GET = 62,
    STA_LASERSITE_NOADD = 63,
    STA_PL_DONT_FIRE = 64,
    STA_PL_EM_ACTION = 65,
    STA_SUB_CATCHED = 66,
    STA_CUT_CHANGE = 67,
    STA_NO_FENCE = 68,
    STA_SSCRN_REQUEST = 69,
    STA_ESP_COMPULSION_NOSUSPEND = 70,
    STA_PL_MISS_SHOT = 71,
    STA_SUB_BULLDOZER = 72,
    STA_LIT_NO_UPDATE = 73,
    STA_MAP_DISABLE = 74,
    STA_USE_SHADOW_LIGHT = 75,
    STA_EVENT_SYSYTEM = 76,
    STA_INTO_SHOP = 77,
    STA_TIMER_NO_PAUSE = 78,
    STA_EFFAREA_USE_CAM = 79,
    STA_TITLE = 80,
    STA_51 = 81, // PS2: STA_MAP_CK_UNLIMIT, unused in GC?
    STA_52 = 82, // PS2: STA_RIFLE_READY
    STA_53 = 83, // PS2: STA_LOWPOLY_USE
    STA_54 = 84, // PS2: STA_TERMINAL
    STA_55 = 85, // PS2: STA_HIGH_TEMPERATURE_THERMO
    STA_56 = 86,
    STA_57 = 87,
    STA_58 = 88,
    STA_59 = 89,
    STA_5a = 90,
    STA_5b = 91,
    STA_5c = 92,
    STA_5d = 93,
    STA_5e = 94,
    STA_5f = 95,
    STA_SAVEDATA_NO_UPDATE = 96,
    STA_BEHIND_CAM = 97,
    STA_62 = 98,
    STA_SCISSOR = 99,
    STA_SLOW = 100,
    STA_SUB_ASHLEY = 101,
    STA_BIG_MARKER = 102,
    STA_EVENT_CANCEL = 103,
    STA_KLAUSER_TRANSFORM = 104,
    STA_69 = 105, // PS2: STA_ELEVATOR
    STA_6a = 106,
    STA_6b = 107,
    STA_6c = 108,
    STA_6d = 109,
    STA_6e = 110,
    STA_6f = 111,
    STA_70 = 112, // PS2: STA_DIV_EVENT_SET
    STA_71 = 113, // PS2: STA_DIV_EVENT_ING
    STA_72 = 114, // PS2: STA_NOW_LOADING
    STA_73 = 115, // PS2: STA_SCREEN_STOP_RESERVE
    STA_74 = 116, // PS2: STA_TYPEWRITER
    STA_75 = 117,
    STA_76 = 118,
    STA_77 = 119,
    STA_78 = 120,
    STA_79 = 121,
    STA_7a = 122,
    STA_7b = 123,
    STA_7c = 124,
    STA_7d = 125,
    STA_7e = 126,
    STA_7f = 127,
};

// System_flg bits
enum SYS_FLAG {
    SYS_OMAKE_ADA_GAME = 0,
    SYS_OMAKE_ETC_GAME = 1,
    SYS_EXCEPTION = 2,
    SYS_RENDER_END = 3,
    SYS_SP_USED = 4,
    SYS_SOFT_RESET = 5,
    SYS_DATA_READ = 6,
    SYS_ROOMJUMP = 7,
    SYS_INVISIBLE = 8,
    SYS_DOOR_AFTER = 9,
    SYS_DOORDEMO = 10,
    SYS_TRANS_STOP = 11,
    SYS_CONTINUE = 12,
    SYS_SET_BLACK = 13,
    SYS_SN_PC_READ = 14,
    SYS_SN_PC_READ_TOOL = 15,
    SYS_HARD_RESET = 16,
    SYS_SCREEN_SHOT = 17,
    SYS_NEW_GAME = 18,
    SYS_TYPEWRITER = 19,
    SYS_SCISSOR_ON = 20,
    SYS_SCREEN_STOP = 21,
    SYS_CARD_ACCESS = 22,
    SYS_LOAD_GAME = 23,
    SYS_CONTINUE_AFTER = 24,
    SYS_START_EVT_SKIP = 25,
    SYS_HARD_MODE = 26,
    SYS_MESSAGE_INIT = 27,
    SYS_PUBLICITY_VER = 28,
    SYS_1d = 29, // PS2: SYS_SAVEDATA_EXIST
    SYS_1e = 30, // PS2: SYS_PAL
    SYS_1f = 31, // PS2: SYS_DTV480P
};

// Stop_flg bits
enum SPF_FLAG {
    SPF_KEY = 0,
    SPF_CAMERA = 1,
    SPF_EM = 2,
    SPF_PL = 3,
    SPF_ESP = 4,
    SPF_OBJ = 5,
    SPF_CTRL = 6,
    SPF_LIGHT = 7,
    SPF_SCE = 8,
    SPF_SCE_AT = 9,
    SPF_CCHG = 10,
    SPF_PL_CCHG = 11,
    SPF_NOTSUBSCR = 12,
    SPF_WATER = 13,
    SPF_SPECULAR = 14,
    SPF_EARTHQUAKE = 15,
    SPF_VIBRATION = 16,
    SPF_CINESCO = 17,
    SPF_MIST = 18,
    SPF_SUBCHAR = 19,
    SPF_SE_CALC = 20,
    SPF_EVT = 21,
    SPF_BLOCK = 22,
    SPF_ACTBTN = 23,
    SPF_DATAREAD_AT = 24,
    SPF_ID_SYSTEM = 25,
    SPF_ESP_AREA = 26,
    SPF_1b = 27,
    SPF_1c = 28,
    SPF_1d = 29,
    SPF_1e = 30,
    SPF_1f = 31,
};

// Disp_flg bits
enum DPF_FLAG {
    DPF_EM = 0,
    DPF_PL = 1,
    DPF_SUBCHAR = 2,
    DPF_OBJ = 3,
    DPF_SCR = 4,
    DPF_ESP = 5,
    DPF_SHADOW = 6,
    DPF_WATER = 7,
    DPF_MIRROR = 8,
    DPF_CTRL = 9,
    DPF_CINESCO = 10,
    DPF_FILTER = 11,
    DPF_GLB_ILM = 12,
    DPF_CAST_SHADOW = 13,
    DPF_CLOTH = 14,
    DPF_COCKPIT = 15,
    DPF_SELF_SHADOW = 16,
    DPF_FOG = 17,
    DPF_ID_SYSTEM = 18,
    DPF_ACTBTN = 19,
    DPF_MESSAGE = 20,
    DPF_TEX_RENDER = 21,
    DPF_16 = 22,
    DPF_17 = 23,
    DPF_18 = 24,
    DPF_19 = 25,
    DPF_1a = 26,
    DPF_1b = 27,
    DPF_1c = 28,
    DPF_1d = 29,
    DPF_1e = 30,
    DPF_1f = 31,
};

// Scenario_flg bits
enum SCF_FLAG {
    SCF_NULL = 0,
    SCF_R104_EVENT_4 = 1,
    SCF_R110_FLOOR_MOVE = 2,
    SCF_ACTIVATOR_SP_MES = 3,
    SCF_BLUE_HERB_MES = 4,
    SCF_R102_VIRUS_OCCUR = 5,
    SCF_R01E_TEST = 6,
    SCF_R100_TEST00 = 7,
    SCF_R100_TEST01 = 8,
    SCF_R100_TEST02 = 9,
    SCF_R106_EVENT = 10,
    SCF_R117_FIND_ASHLEY = 11,
    SCF_R100_DOG_RUN = 12,
    SCF_ST1_SUB_MISSION = 13,
    SCF_R11C_BESIEGED_EVENT = 14,
    SCF_R201_EVENT00 = 15,
    SCF_R108_PUZZLE_CLEAR = 16,
    SCF_R100_KILL_GANADE_1ST = 17,
    SCF_R101_ENTER = 18,
    SCF_R103_ENTER = 19,
    SCF_R106_ENTER = 20,
    SCF_R106_CONFINEED_WITH_LUIS = 21,
    SCF_R108_CHECK_DOOR = 22,
    SCF_R10C_GET_CREST = 23,
    SCF_NO_ASHLEY_DIST_CK = 24,
    SCF_R11C_BESIEGED_END_EVENT = 25,
    SCF_R103_CLOSE_COVER = 26,
    SCF_R103_ITEM_IN_CESSPIT = 27,
    SCF_R11B_END_SALAMANDER = 28,
    SCF_ST1_MAP_DAY = 29,
    SCF_ST1_MAP_NIGHT = 30,
    SCF_ST2_MAP = 31,
    SCF_ST3_MAP = 32,
    SCF_R217_PUZZLE_CLEAR = 33,
    SCF_R104_MEET_MERCHANT = 34,
    SCF_R206_ASHLEY_RESCUE = 35,
    SCF_R101_IMPRISON = 36,
    SCF_R103_OPEN_COVER = 37,
    SCF_R20D_END_OF_ASHLEY_PLAY = 38,
    SCF_ST1_NIGHT = 39,
    SCF_ST2_IN = 40,
    SCF_CONTACT_MERCHANT = 41,
    SCF_R10E_STOCK_DAY = 42,
    SCF_R10E_STOCK_NIGHT = 43,
    SCF_R108_OPERATOR = 44,
    SCF_R204_ASHLEY_SPLIT = 45,
    SCF_R11C_OPERATOR = 46,
    SCF_ST3_IN = 47,
    SCF_ST1_SUB_PERFECT = 48,
    SCF_R104_MERCHANT_MARK = 49,
    SCF_ST1_NIGHT_LV_ADD = 50,
    SCF_R307_REGENERATER_APPEAR = 51,
    SCF_R316_TO_R30A_CUTBACK_EVENT = 52,
    SCF_R30D_ENTER = 53,
    SCF_R332_BOSS_DIE = 54,
    SCF_ADA_COUNT_DOWN_START = 55,
    SCF_ST3_COUNT_DOWN_START = 56,
    SCF_R213_ASHLEY_LOST = 57,
    SCF_R317_LEON_WOUND = 58,
    SCF_R120_EVENT_CANCEL = 59,
    SCF_R22C_BONUS_1 = 60,
    SCF_R22C_BONUS_2 = 61,
    SCF_R22C_BONUS_3 = 62,
    SCF_R22C_BONUS_4 = 63,
    SCF_R321_HERI_DOWN = 64,
    SCF_R329_ASHLEY_HELP = 65,
    SCF_R405_ADA_GAME_INIT = 66,
    SCF_R31C_TOWER_EXPLODE = 67,
    SCF_R206_ASHLEY_GAME = 68,
    SCF_R201_SET_3OBJ = 69,
    SCF_R229_IN = 70,
    SCF_R225_IN = 71,
    SCF_R226_IN = 72,
    SCF_R300_00 = 73,
    SCF_R303_IN = 74,
    SCF_R304_00 = 75,
    SCF_R30C_ASHLEY_SCREAM = 76,
    SCF_R309_GET_KEY = 77,
    SCF_R30C_SAVE_ASHLEY = 78,
    SCF_R317_KNIFE_BATTLE = 79,
    SCF_R31A_IN = 80,
    SCF_R31B_U3 = 81,
    SCF_R31C_IN = 82,
    SCF_R31C_OPEN_DOOR = 83,
    SCF_R330_END_OPE = 84,
    SCF_R332_KEY_GET = 85,
    SCF_ST3_COUNT_DOWN_DIE = 86,
    SCF_KEY_LOCK = 87,
    SCF_R204_MERCHANT_MOVE = 88,
    SCF_OMAKE_MAP = 89,
    SCF_R100_OFFICERS_FOUND = 90,
    SCF_R21A_R21B_IN = 91,
    SCF_ADA_SHOP_1ST = 92,
    SCF_ADA_SHOP_2ND = 93,
    SCF_ASHLEY_PARA_OFF = 94,
    SCF_5f = 95,
    SCF_60 = 96,
    SCF_61 = 97,
    SCF_62 = 98,
    SCF_63 = 99,
    SCF_64 = 100,
    SCF_65 = 101,
    SCF_66 = 102,
    SCF_67 = 103,
    SCF_68 = 104,
    SCF_69 = 105,
    SCF_6a = 106,
    SCF_6b = 107,
    SCF_6c = 108,
    SCF_6d = 109,
    SCF_6e = 110,
    SCF_6f = 111,
    SCF_70 = 112,
    SCF_71 = 113,
    SCF_72 = 114,
    SCF_73 = 115,
    SCF_74 = 116,
    SCF_75 = 117,
    SCF_76 = 118,
    SCF_77 = 119,
    SCF_78 = 120,
    SCF_79 = 121,
    SCF_7a = 122,
    SCF_7b = 123,
    SCF_7c = 124,
    SCF_7d = 125,
    SCF_7e = 126,
    SCF_7f = 127,
    SCF_80 = 128,
    SCF_81 = 129,
    SCF_82 = 130,
    SCF_83 = 131,
    SCF_84 = 132,
    SCF_85 = 133,
    SCF_86 = 134,
    SCF_87 = 135,
    SCF_88 = 136,
    SCF_89 = 137,
    SCF_8a = 138,
    SCF_8b = 139,
    SCF_8c = 140,
    SCF_8d = 141,
    SCF_8e = 142,
    SCF_8f = 143,
    SCF_90 = 144,
    SCF_91 = 145,
    SCF_92 = 146,
    SCF_93 = 147,
    SCF_94 = 148,
    SCF_95 = 149,
    SCF_96 = 150,
    SCF_97 = 151,
    SCF_98 = 152,
    SCF_99 = 153,
    SCF_9a = 154,
    SCF_9b = 155,
    SCF_9c = 156,
    SCF_9d = 157,
    SCF_9e = 158,
    SCF_9f = 159,
    SCF_a0 = 160,
    SCF_a1 = 161,
    SCF_a2 = 162,
    SCF_a3 = 163,
    SCF_a4 = 164,
    SCF_a5 = 165,
    SCF_a6 = 166,
    SCF_FILE_07_GET = 167,
    SCF_FILE_08_GET = 168,
    SCF_a9 = 169,
    SCF_aa = 170,
    SCF_ab = 171,
    SCF_ac = 172,
    SCF_ad = 173,
    SCF_ae = 174,
    SCF_af = 175,
    SCF_b0 = 176,
    SCF_b1 = 177,
    SCF_b2 = 178,
    SCF_b3 = 179,
    SCF_b4 = 180,
    SCF_b5 = 181,
    SCF_b6 = 182,
    SCF_b7 = 183,
    SCF_R316_IN = 184,
    SCF_R102_MEET_MERCHANT = 185,
    SCF_ba = 186,
    SCF_bb = 187,
    SCF_bc = 188,
    SCF_bd = 189,
    SCF_be = 190,
    SCF_bf = 191,
    SCF_c0 = 192,
    SCF_c1 = 193,
    SCF_c2 = 194,
    SCF_c3 = 195,
    SCF_c4 = 196,
    SCF_c5 = 197,
    SCF_c6 = 198,
    SCF_c7 = 199,
    SCF_c8 = 200,
    SCF_c9 = 201,
    SCF_ca = 202,
    SCF_cb = 203,
    SCF_cc = 204,
    SCF_cd = 205,
    SCF_ce = 206,
    SCF_cf = 207,
    SCF_d0 = 208,
    SCF_d1 = 209,
    SCF_d2 = 210,
    SCF_d3 = 211,
    SCF_d4 = 212,
    SCF_d5 = 213,
    SCF_d6 = 214,
    SCF_d7 = 215,
    SCF_d8 = 216,
    SCF_d9 = 217,
    SCF_da = 218,
    SCF_db = 219,
    SCF_dc = 220,
    SCF_dd = 221,
    SCF_de = 222,
    SCF_df = 223,
    SCF_e0 = 224,
    SCF_e1 = 225,
    SCF_e2 = 226,
    SCF_e3 = 227,
    SCF_e4 = 228,
    SCF_e5 = 229,
    SCF_e6 = 230,
    SCF_e7 = 231,
    SCF_e8 = 232,
    SCF_e9 = 233,
    SCF_ea = 234,
    SCF_eb = 235,
    SCF_ec = 236,
    SCF_ed = 237,
    SCF_ee = 238,
    SCF_ef = 239,
    SCF_f0 = 240,
    SCF_f1 = 241,
    SCF_f2 = 242,
    SCF_f3 = 243,
    SCF_f4 = 244,
    SCF_f5 = 245,
    SCF_f6 = 246,
    SCF_f7 = 247,
    SCF_f8 = 248,
    SCF_f9 = 249,
    SCF_fa = 250,
    SCF_fb = 251,
    SCF_fc = 252,
    SCF_fd = 253,
    SCF_fe = 254,
    SCF_ff = 255,
};

// Item_flg bits
enum ITF_FLAG {
    ITF_DUMMY = 0,
    ITF_R108_ITEM = 1,
    ITF_R105_ITEM = 2,
    ITF_FN57 = 3,
    ITF_R10A_ITEM06 = 4,
    ITF_R102_ITEM = 5,
    ITF_R103_ITEM00 = 6,
    ITF_R103_ITEM01 = 7,
    ITF_R10B_ITEM = 8,
    ITF_R11C_ITEM = 9,
    ITF_R216_ITEM = 10,
    ITF_R207_GOLDEN_SWORD = 11,
    ITF_R207_SILVER_SWORD = 12,
    ITF_ITEM_5F = 13,
    ITF_ITEM_59 = 14,
    ITF_ITEM_5D = 15,
    ITF_ITEM_61 = 16,
    ITF_R20E_SALAZAR_CREST = 17,
    ITF_R209_KEY = 18,
    ITF_R21A_ITEM = 19,
    ITF_R103_FILE = 20,
    ITF_R11D_ITEM = 21,
    ITF_R11E_ITEM = 22,
    ITF_R117_ITEM = 23,
    ITF_R10D_ITEM = 24,
    ITF_R309_KEY = 25,
    ITF_R308_THERMO_RIFLE = 26,
    ITF_R40E_SAMPLE00 = 27,
    ITF_R40B_SAMPLE00 = 28,
    ITF_R40C_SAMPLE00 = 29,
    ITF_R31C_CREST_A = 30,
    ITF_FILE_07 = 31,
    ITF_FILE_08 = 32,
    ITF_R410_SAMPLE00 = 33,
    ITF_22 = 34,
    ITF_R40D_SAMPLE00 = 35,
    ITF_R332_ADA_ROCKET = 36,
    ITF_ST2A_00 = 37,
    ITF_ST2A_01 = 38,
    ITF_ST2A_02 = 39,
    ITF_ST2A_03 = 40,
    ITF_ST2A_04 = 41,
    ITF_ST2A_05 = 42,
    ITF_ST2A_06 = 43,
    ITF_ST2A_07 = 44,
    ITF_ST2A_08 = 45,
    ITF_ST2A_09 = 46,
    ITF_ST2A_10 = 47,
    ITF_ST2A_11 = 48,
    ITF_ST2B_00 = 49,
    ITF_ST2B_01 = 50,
    ITF_ST2B_02 = 51,
    ITF_ST2C_00 = 52,
    ITF_ST3B_00 = 53,
    ITF_ST3B_01 = 54,
    ITF_ST3B_02 = 55,
    ITF_ST3D_00 = 56,
    ITF_ST3D_01 = 57,
    ITF_ST3D_02 = 58,
    ITF_ST3D_03 = 59,
    ITF_R400_00 = 60,
    ITF_R400_01 = 61,
    ITF_R400_02 = 62,
    ITF_R400_03 = 63,
    ITF_R400_04 = 64,
    ITF_R402_00 = 65,
    ITF_R402_01 = 66,
    ITF_R402_02 = 67,
    ITF_R402_03 = 68,
    ITF_R402_04 = 69,
    ITF_R402_05 = 70,
    ITF_R403_00 = 71,
    ITF_R403_01 = 72,
    ITF_R403_02 = 73,
    ITF_R403_03 = 74,
    ITF_R403_04 = 75,
    ITF_R403_05 = 76,
    ITF_R403_06 = 77,
    ITF_R403_07 = 78,
    ITF_R403_08 = 79,
    ITF_R403_09 = 80,
    ITF_R403_10 = 81,
    ITF_R403_11 = 82,
    ITF_R404_00 = 83,
    ITF_R404_01 = 84,
    ITF_R404_02 = 85,
    ITF_R404_03 = 86,
    ITF_R404_04 = 87,
    ITF_R404_05 = 88,
    ITF_R404_06 = 89,
    ITF_R404_07 = 90,
    ITF_R404_08 = 91,
    ITF_R404_09 = 92,
    ITF_R404_10 = 93,
    ITF_R404_11 = 94,
    ITF_ST2A_12 = 95,
    ITF_R119_ITEM0 = 96,
    ITF_R119_ITEM1 = 97,
    ITF_R119_ITEM2 = 98,
    ITF_R119_ITEM3 = 99,
    ITF_R119_ITEM4 = 100,
    ITF_R119_ITEM5 = 101,
    ITF_R11E_ITEM0 = 102,
    ITF_R11E_ITEM1 = 103,
    ITF_R11E_ITEM2 = 104,
    ITF_R11E_ITEM3 = 105,
    ITF_R11E_ITEM4 = 106,
    ITF_R11E_ITEM5 = 107,
    ITF_R11E_ITEM6 = 108,
    ITF_R11E_ITEM7 = 109,
    ITF_R11E_ITEM8 = 110,
    ITF_R11E_ITEM9 = 111,
    ITF_R11E_ITEM10 = 112,
    ITF_R11E_ITEM11 = 113,
    ITF_72 = 114,
    ITF_73 = 115,
    ITF_74 = 116,
    ITF_75 = 117,
    ITF_76 = 118,
    ITF_77 = 119,
    ITF_78 = 120,
    ITF_79 = 121,
    ITF_7a = 122,
    ITF_ADA_01_TRES_00 = 123,
    ITF_ADA_01_TRES_01 = 124,
    ITF_ADA_01_TRES_02 = 125,
    ITF_ADA_01_TRES_03 = 126,
    ITF_ADA_01_TRES_04 = 127,
};

// Key_flg bits (KEY_LOCK in t_flag, one per locked door). GC has two words; the PS2 list runs on
// into its st5 additions and is kept verbatim.
enum KEY_FLAG {
    KYF_NULL = 0,
    KYF_R100_IRON_DOOR = 1,
    KYF_R101_IRON_DOOR = 2,
    KYF_R118_TO_R117_DOOR = 3,
    KYF_R113_TO_R11C_DOOR = 4,
    KYF_R21E_DOOR = 5,
    KYF_R105_TO_R101_DOOR = 6,
    KYF_R118_TO_R117_INLOCK = 7,
    KYF_R10F_TO_R200_DOOR = 8,
    KYF_R104_TO_R107_DOOR = 9,
    KYF_R20D_TO_R206_DOOR = 10,
    KYF_R11D_IRON_DOOR = 11,
    KYF_R11E_TO_R10F_DOOR = 12,
    KYF_R206_TO_R211_DOOR = 13,
    KYF_R203_TO_R201_DOOR = 14,
    KYF_R11D_TO_R10F_DOOR = 15,
    KYF_R201_DOOR = 16,
    KYF_R201_TO_R210_DOOR = 17,
    KYF_R306_TO_R308_DOOR = 18,
    KYF_R30C_DOOR = 19,
    KYF_R306_TO_R303_DOOR = 20,
    KYF_R30D_TO_R30F_DOOR = 21,
    KYF_R31C_TO_R320_DOOR = 22,
    KYF_R306_TO_R30B_DOOR = 23,
    KYF_ST1_00 = 24,
    KYF_ST1_01 = 25,
    KYF_ST1_02 = 26,
    KYF_ST1_03 = 27,
    KYF_ST1_04 = 28,
    KYF_ST1_05 = 29,
    KYF_ST1_06 = 30,
    KYF_ST1_07 = 31,
    KYF_ST1_08 = 32,
    KYF_ST1_09 = 33,
    KYF_ST1_10 = 34,
    KYF_ST1_12 = 35,
    KYF_ST1_13 = 36,
    KYF_ST1_14 = 37,
    KYF_ST1_15 = 38,
    KYF_ST1_16 = 39,
    KYF_ST1_17 = 40,
    KYF_ST1_18 = 41,
    KYF_ST1_19 = 42,
    KYF_ST1_20 = 43,
    KYF_ST1_21 = 44,
    KYF_ST1_22 = 45,
    KYF_ST1_23 = 46,
    KYF_ST1_24 = 47,
    KYF_ST1_25 = 48,
    KYF_ST1_26 = 49,
    KYF_ST2_00 = 50,
    KYF_ST2_01 = 51,
    KYF_ST2_02 = 52,
    KYF_ST2_03 = 53,
    KYF_ST2_04 = 54,
    KYF_ST2_05 = 55,
    KYF_ST2_06 = 56,
    KYF_ST2_07 = 57,
    KYF_ST2_08 = 58,
    KYF_ST2_09 = 59,
    KYF_ST2_10 = 60,
    KYF_ST2_11 = 61,
    KYF_ST2_12 = 62,
    KYF_ST2_13 = 63,
    KYF_ST2_14 = 64,
    KYF_ST2_15 = 65,
    KYF_ST2_16 = 66,
    KYF_ST2_17 = 67,
    KYF_ST2_18 = 68,
    KYF_ST2_19 = 69,
    KYF_ST2_20 = 70,
    KYF_ST2_21 = 71,
    KYF_ST2_22 = 72,
    KYF_ST2_23 = 73,
    KYF_ST2_24 = 74,
    KYF_ST2_25 = 75,
    KYF_ST2_26 = 76,
    KYF_ST2_27 = 77,
    KYF_ST2_28 = 78,
    KYF_ST2_29 = 79,
    KYF_ST2_30 = 80,
    KYF_ST2_31 = 81,
    KYF_ST2_32 = 82,
    KYF_ST2_33 = 83,
    KYF_ST2_34 = 84,
    KYF_ST2_35 = 85,
    KYF_ST2_36 = 86,
    KYF_ST2_37 = 87,
    KYF_ST2_38 = 88,
    KYF_ST2_39 = 89,
    KYF_ST3_00 = 90,
    KYF_ST3_01 = 91,
    KYF_ST3_02 = 92,
    KYF_ST3_03 = 93,
    KYF_ST3_04 = 94,
    KYF_ST3_05 = 95,
    KYF_ST3_06 = 96,
    KYF_ST3_07 = 97,
    KYF_ST3_08 = 98,
    KYF_ST3_09 = 99,
    KYF_ST3_10 = 100,
    KYF_ST3_11 = 101,
    KYF_ST3_12 = 102,
    KYF_ST3_13 = 103,
    KYF_ST3_14 = 104,
    KYF_ST3_15 = 105,
    KYF_ST3_16 = 106,
    KYF_ST3_17 = 107,
    KYF_ST3_18 = 108,
    KYF_ST3_19 = 109,
    KYF_ST3_20 = 110,
    KYF_ST3_21 = 111,
    KYF_ST3_22 = 112,
    KYF_ST3_23 = 113,
    KYF_ST3_24 = 114,
    KYF_ST3_25 = 115,
    KYF_ST3_26 = 116,
    KYF_ST3_27 = 117,
    KYF_ST3_28 = 118,
    KYF_ST3_29 = 119,
    KYF_ST3_30 = 120,
    KYF_ST3_31 = 121,
    KYF_ST3_32 = 122,
    KYF_ST3_33 = 123,
    KYF_ST3_34 = 124,
    KYF_ST4_00 = 125,
    KYF_ST4_01 = 126,
    KYF_ST4_02 = 127,
    KYF_ST4_03 = 128,
    KYF_ST5_00 = 129,
    KYF_ST5_01 = 130,
    KYF_ST5_02 = 131,
    KYF_ST5_03 = 132,
    KYF_ST5_04 = 133,
    KYF_ST5_05 = 134,
    KYF_ST5_06 = 135,
    KYF_ST5_07 = 136,
    KYF_ST5_08 = 137,
    KYF_ST5_09 = 138,
    KYF_ST5_10 = 139,
    KYF_ST5_11 = 140,
    KYF_ST5_12 = 141,
    KYF_ST5_13 = 142,
    KYF_ST5_14 = 143,
    KYF_ST5_15 = 144,
    KYF_ST5_16 = 145,
    KYF_ST5_17 = 146,
    KYF_ST5_18 = 147,
    KYF_ST5_19 = 148,
    KYF_ST5_20 = 149,
    KYF_ST5_21 = 150,
    KYF_ST5_22 = 151,
    KYF_ST5_23 = 152,
    KYF_MAX = 153,
};

// Test flag `no` in the word array at `base` (bit 31 - (no & 31) of word no >> 5).  The base is an
// address rather than a field so a check can read the flags through whichever pointer the caller
// holds.
#define FlagChk(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) & (0x80000000 >> ((no) & 31)))

// The same test written as a shift into the sign bit, for a condition that tests two bits of one
// word: two mask tests fold into a single mask and stop matching, two shifts stay two tests.
// FlagChkSignW takes the flag word itself, FlagChkSign a word array.
#define FlagChkSignW(flg, no) ((s32) ((flg) << ((no) & 31)) < 0)
#define FlagChkSign(flg, no) FlagChkSignW((flg)[(no) >> 5], no)

// Config_flg bits, in SystemSave rather than pG
enum CFG_FLAG {
    CFG_AIM_REVERSE = 0,
    CFG_WIDE_MODE = 1,
    CFG_LOCK_ON = 2,
    CFG_BONUS_GET = 3,
    CFG_VIBRATION = 4,
    CFG_KNIFE_MODE = 5,
    CFG_06 = 6,
    CFG_07 = 7,
    CFG_08 = 8,
    CFG_09 = 9,
    CFG_0a = 10,
    CFG_0b = 11,
    CFG_0c = 12,
    CFG_0d = 13,
    CFG_0e = 14,
    CFG_0f = 15,
    CFG_10 = 16,
    CFG_11 = 17,
    CFG_12 = 18,
    CFG_13 = 19,
    CFG_14 = 20,
    CFG_15 = 21,
    CFG_16 = 22,
    CFG_17 = 23,
    CFG_18 = 24,
    CFG_19 = 25,
    CFG_1a = 26,
    CFG_1b = 27,
    CFG_1c = 28,
    CFG_1d = 29,
    CFG_1e = 30,
    CFG_1f = 31,
};

// Extra_flg bits, in SystemSave (t_flag EXTRA page shows the CONFIG names by mistake!)
enum EXT_FLAG {
    EXT_COSTUME = 0,
    EXT_HARD_MODE = 1,
    EXT_GET_SW500 = 2,
    EXT_GET_TOMPSON = 3,
    EXT_GET_ADA = 4,
    EXT_GET_HUNK = 5,
    EXT_GET_KLAUSER = 6,
    EXT_GET_WESKER = 7,
    EXT_GET_OMAKE_ADA_GAME = 8,
    EXT_GET_OMAKE_ETC_GAME = 9,
    EXT_ASHLEY_ARMOR = 10,
    EXT_0b = 11,
    EXT_0c = 12,
    EXT_0d = 13,
    EXT_0e = 14,
    EXT_0f = 15,
    EXT_10 = 16,
    EXT_11 = 17,
    EXT_12 = 18,
    EXT_13 = 19,
    EXT_14 = 20,
    EXT_15 = 21,
    EXT_16 = 22,
    EXT_17 = 23,
    EXT_18 = 24,
    EXT_19 = 25,
    EXT_1a = 26,
    EXT_1b = 27,
    EXT_1c = 28,
    EXT_1d = 29,
    EXT_1e = 30,
    EXT_1f = 31,
};

// The per-field tests go through a real inline rather than the FlagChk macro: the flag number
// arrives as a parameter, which changes how sched1 orders the constants that follow the test
// (objWep drawPoint and esp_app EspDrawLaserLine only match this way).
static inline u32 FlagBitChk(u32* flg, u32 no) { return flg[no >> 5] & (0x80000000 >> (no & 31)); }

#define DbgFlagChk(g, n) FlagBitChk((g)->Debug_flg, n)
#define StaFlagChk(g, n) FlagBitChk((g)->Status_flg, n)
#define SysFlagChk(g, n) FlagBitChk(&(g)->System_flg, n)
#define SpfFlagChk(g, n) FlagBitChk(&(g)->Stop_flg, n)
#define DpfFlagChk(g, n) FlagBitChk(&(g)->Disp_flg, n)
#define ScfFlagChk(g, n) FlagBitChk((g)->Scenario_flg, n)
#define ItfFlagChk(g, n) FlagBitChk((g)->Item_flg, n)
#define KyfFlagChk(g, n) FlagBitChk((g)->Key_flg, n)
// Room_flg: the bit numbers are per room, see the R<xxx>_FLAG enum at the top of the room source.
#define RmfFlagChk(g, n) FlagBitChk((g)->Room_flg, n)

// Set and clear, against the same base and index as FlagChk.
#define FlagOn(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) |= (0x80000000 >> ((no) & 31)))
#define FlagOff(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) &= ~(0x80000000 >> ((no) & 31)))
#define FlagXor(base, no) (*(u32*) ((((no) >> 5) << 2) + (u32) (base)) ^= (0x80000000 >> ((no) & 31)))

// The same four, for a call site whose flag number is a variable or a struct field rather than an
// enumerator.  Both arguments are copied into locals: substituted twice the field would be loaded
// twice, where the original loads it once.  The number keeps the type the call site gives it, so an
// index cast to u32 folds its shift into a single rlwinm where a signed one takes two instructions.
#define FLAG_WORD_VAR(base, no, op) ({ u32 flagBase_ = (u32) (base); __typeof__(no) flagNo_ = (no); \
                                       *(u32*) (((flagNo_ >> 5) << 2) + flagBase_) op; })
#define FlagChkVar(base, no) FLAG_WORD_VAR(base, no, & (0x80000000 >> (flagNo_ & 31)))
#define FlagOnVar(base, no) FLAG_WORD_VAR(base, no, |= (0x80000000 >> (flagNo_ & 31)))
#define FlagOffVar(base, no) FLAG_WORD_VAR(base, no, &= ~(0x80000000 >> (flagNo_ & 31)))
#define FlagXorVar(base, no) FLAG_WORD_VAR(base, no, ^= (0x80000000 >> (flagNo_ & 31)))

#define DbgFlagOn(g, n) FlagOn(&(g)->Debug_flg, n)
#define DbgFlagOff(g, n) FlagOff(&(g)->Debug_flg, n)
#define DbgFlagXor(g, n) FlagXor(&(g)->Debug_flg, n)
#define StaFlagOn(g, n) FlagOn(&(g)->Status_flg, n)
#define StaFlagOff(g, n) FlagOff(&(g)->Status_flg, n)
#define SysFlagOn(g, n) FlagOn(&(g)->System_flg, n)
#define SysFlagOff(g, n) FlagOff(&(g)->System_flg, n)
#define SpfFlagOn(g, n) FlagOn(&(g)->Stop_flg, n)
#define SpfFlagOff(g, n) FlagOff(&(g)->Stop_flg, n)
#define DpfFlagOn(g, n) FlagOn(&(g)->Disp_flg, n)
#define DpfFlagOff(g, n) FlagOff(&(g)->Disp_flg, n)
#define ScfFlagOn(g, n) FlagOn(&(g)->Scenario_flg, n)
#define ScfFlagOff(g, n) FlagOff(&(g)->Scenario_flg, n)
#define ItfFlagOn(g, n) FlagOn(&(g)->Item_flg, n)
#define ItfFlagOff(g, n) FlagOff(&(g)->Item_flg, n)
#define KyfFlagOn(g, n) FlagOn(&(g)->Key_flg, n)
#define KyfFlagOff(g, n) FlagOff(&(g)->Key_flg, n)
#define RmfFlagOn(g, n) FlagOn(&(g)->Room_flg, n)
#define RmfFlagOff(g, n) FlagOff(&(g)->Room_flg, n)
#define CfgFlagChk(g, n) FlagBitChk(&(g)->Config_flg, n)
#define CfgFlagOn(g, n) FlagOn(&(g)->Config_flg, n)
#define CfgFlagOff(g, n) FlagOff(&(g)->Config_flg, n)
#define ExtFlagChk(g, n) FlagBitChk(&(g)->Extra_flg, n)
#define ExtFlagOn(g, n) FlagOn(&(g)->Extra_flg, n)
#define ExtFlagOff(g, n) FlagOff(&(g)->Extra_flg, n)

// The stored value is a register of its own, as when passed as a parameter: a constant assigned
// directly is scheduled elsewhere.
static inline void U16Set(u16& d, u16 v) { d = v; }

// Vec copy whose destination is a word pointer variable. That block move is a store the compiler cannot
// place against the cached pG / pPL loads, so they are reloaded afterwards; `memcpy(&pG->field, ...)`, a Vec*
// variable and a struct assignment keep them. A plain block: a do/while wrapper changes the generated code.
#define VEC_COPY(dst, src)                     \
    {                                          \
        u32* copyDst_ = (u32*) &(dst);         \
        memcpy(copyDst_, &(src), sizeof(Vec)); \
    }

// Offset of a GlobalWork member, written with the null-pointer idiom. Address arithmetic that adds it to pG
// keeps the offset as the last term (`pG->field` adds it first), which some callers need.
#define PG_OFS(f) ((u32) &((GlobalWork*) 0)->f)
// Death words of enemy list `list` (Em_flg row: eight u32, one bit per entry). The scaled index is added to pG
// first and the member offset last; written as `pG->Em_flg[list]` the address is built differently.
#define EM_FLG_ROW(list) ((u32*) ((list) * 0x20 + (u32) pG + PG_OFS(Em_flg)))

#endif
