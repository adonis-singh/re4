// game/snd: game-side sound interface (D:/Bio4/Prog/snd.cpp, -O2). Owns the game sound work
// (`Snd`, pSnd), the sound data memory map (`SndMem`) and the room BGM / stream tables, and calls
// into the C sound driver (src/game/snd_*.cpp, include/snd_drv.h).
// 92/97 byte-identical (DOL sweep 6, 2026-09-10): SndBgmTblSet reads pG through `GRefS` (the load
// stays inside the store loop) with `r` declared before `ret`; SndCall reads the address-taken
// parameters `blk`/`no` through `RefU16` where the target reloads them after word stores (their
// stack slots are MEM_SCALAR_P in ours, not in the original's alias.c), tests a single-use
// `int ok = 1` (the `li r0,1; cmpwi r0,0; bne`), and nests the curve test so `cs` is computed
// before `curve_ok == 1`. 97/97 (DOL sweep 11): SndCall's `flags_68` test reads the field through
// `RefU32` (an unflagged MEM conflicts with the u16 parameter stores, whose ready-delay 2 then ranks
// them above `lwz pG`); SndSetReverb's `p` is one variable assigned in both arms (global pseudo in
// r9, the then-arm pSnd load falls to r9 too); SndRoomBgmStart's SndCall sits in two nested
// do-while(0)s (seq/vol/no gain two weighted refs each: allocation order seq, vol, no, w);
// sndVolCalcSub has a dead `dist > vol` test after the `r` chain. debugDisp: the history
// row's y is the giv `i * 0x10 + 0x20` (its init lands after the hoisted table addresses and its extra
// loop insns keep `&History.svol` unhoisted like the target), `y2 = 0x72` before `total = 0`.
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#define SND_SDK_NO_GX
#define SND_DRV_GAME_API
#include "snd.h"
#include "snd_drv.h"
#include "dvd.h"
#include "main.h"
#include "main_mem.h"
#include "room_data.h"
#include "db_log.h"
#include "player.h"
#include "rnd.h"
#include "math_sub.h"
#include "eprintf.h"
#include "flr_at.h"
#include "area.h"
#include "esp.h"

extern "C" void ADXT_SetOutputMono(int sw);   // lib/adx_tlk.c (the CRI headers are not in include/)
void* GetDataExt(void* arc, const char* tag, int no);


#define SND_FILE "D:/Bio4/Prog/snd.cpp"
#define SND_DATA_TOP 0x80370000
#define LOOP_IDX(x, max) ((x) < 0 ? (max) : ((x) > (max) ? 0 : (x)))

SndWork Snd;
SndMemWork SndMem;
u32 UseAramSize[14];
SndHistory History;
SndRoomHdr DefEffTbl;
static u32 callErr[14][32];
u32 aram_buf[3];

u16 StrFileTbl[2] = { 1, 0x5F };
int str_flag = 1;
u32 ARAM_FREE_BASE;
SndWork* pSnd;
u32 SndStrAramAddr[4] = { 0x700000, 0x740000, 0x780000, 0x7C0000 };


// "Illegal SE No." error for block / number, printed once per SE (callErr bits) unless Debug_flg[2]
// bit2 asks for every occurrence.
void sndCallErr(int blk, int call_no)
{
    u32* p;

    if (DbgFlagChk(pG, DBG_SE_ERR_ALL)) {
        pLog->err(0, 0, "SndCall : blk %d No.%d Illegal SE No.", blk, call_no);
        return;
    }
    p = callErr[blk];
    if (SND_BIT_CK(p, (u16) call_no)) {
        return;
    }
    SND_BIT_SET(p, (u16) call_no);
    pLog->err(0, 0, "SndCall : blk %d No.%d Illegal SE No.", blk, call_no);
}

// AXFX allocator hook: reverb buffers from the game heap.
#line 78 SND_FILE
void* reverb_mem_alloc(u32 size)
{
    return MEM_ALLOC(size, 1, 13);
}

// Boot: ARAM setup, reads the sound system files into SND_DATA_TOP — the stream tables (file 0),
// the BGM file (0x60), the door SE table (0x68), the room BGM table (0x6A) and the sub data (0x59)
// — lays out the per-block MRAM buffers and the four stream buffers, starts the driver and takes
// the saved output mode.
void SndInit()
{
    int len;
    int r;
    u32 adr;
    int i;

    pSnd = &Snd;
    ARInit(aram_buf, 3);
    ARAlloc(0x6FC000);
    ARQInit();
    memclr_asm(pSnd, sizeof(SndWork));

#line 120 SND_FILE
    r = DvdRead(0, (void*) SND_DATA_TOP, 0, 0, 0, 0x11, __FILE__, __LINE__);
    str_flag = Dvd.ReadCheck(r, &len, 0, 0) > 0;
    adr = SND_DATA_TOP + ALIGN32(len);
    for (i = 0; i < 2; i++) {
        SndMem.str_file[i] = (SndStrFile*) (SND_DATA_TOP + ((u32*) SND_DATA_TOP)[i]);
        Snd_str_blk_init(i, SndMem.str_file[i]);
    }

    SndMem.bgm_file = (u32*) adr;
#line 133 SND_FILE
    r = DvdRead(0x60, (void*) adr, 0, 0, 0, 0x11, __FILE__, __LINE__);
    Dvd.ReadCheck(r, &len, 0, 0);
    adr += ALIGN32(len);

    SndMem.door_tbl = (SndDoorTbl*) adr;
#line 140 SND_FILE
    r = DvdRead(0x68, (void*) adr, 0, 0, 0, 0x11, __FILE__, __LINE__);
    Dvd.ReadCheck(r, &len, 0, 0);
    adr += ALIGN32(len);

    SndMem.bgm_tbl = (SndBgmTbl*) adr;
#line 147 SND_FILE
    r = DvdRead(0x6A, (void*) adr, 0, 0, 0, 0x11, __FILE__, __LINE__);
    Dvd.ReadCheck(r, &len, 0, 0);
    adr += ALIGN32(len);

    ARAM_FREE_BASE = 0x800000;
    SndMem.blk_mram[0] = (u8*) adr;
    SndMem.blk_mram[1] = SndMem.blk_mram[0] + 0x4000;
    SndMem.blk_mram[2] = SndMem.blk_mram[1] + 0x4000;
    SndMem.blk_mram[7] = SndMem.blk_mram[2] + 0x4000;
    SndMem.mram_end = SndMem.blk_mram[7] + 0x4000;
    adr = (u32) SndMem.mram_end;
    for (i = 3; i >= 0; i--) {
        SndMem.str_buf[i] = adr + 0x10000 + i * 0x8000;
    }
    SndMem.sub_adr = SndMem.str_buf[3] + 0x8000;
#line 170 SND_FILE
    r = DvdRead(0x59, (void*) SndMem.sub_adr, 0, 0, 0, 0x8001, __FILE__, __LINE__);
    Dvd.ReadCheck(r, &len, 0, 0);
    SndMem.sub_end = SndMem.sub_adr + len;

    SndDriverInit();
    pSys->SndMode = Snd_get_sound_mode();
}

// Game start: clears the sound work, sets the MRAM / ARAM allocation tops for enemy blocks and BGM,
// no enemy / BGM / door blocks loaded, the room BGM tables into the room saves.
void SndInit2()
{
    int i;

    memclr_asm(pSnd, sizeof(SndWork));
    pSnd->mram_top = SndMem.mram_end;
    pSnd->aram_base_addr = 0x1F4100;
    for (i = 0; i < 6; i++) {
        pSnd->snd_em_id[i] = 0xFF;
    }
    pSnd->mram_base_addr_bgm = SndMem.mram_end + 0x10000;
    pSnd->aram_base_addr_bgm = 0x700000;
    for (i = 0; i < 2; i++) {
        pSnd->snd_bgm_id[i] = 0xFF;
    }
    SndBgmTblInit();
    pSnd->doorse_id = -1;
    memclr_asm(UseAramSize, sizeof(UseAramSize));
    memclr_asm(callErr, sizeof(callErr));
}

// Copies the default BGM / stream numbers of every listed room from the BGM table file into that
// room's save record (SndRoomSave), so scripts can change them per save.
void SndBgmTblInit()
{
    SndBgmTbl* t = SndMem.bgm_tbl;
    u16* rl = (u16*) ((u8*) t + t->list_ofs);
    int i = 0;
    int j;

    while (*rl != 0xFFFF) {
        SndRoomSave* rs = (SndRoomSave*) RoomData.getRoomSavePtr(*rl);
        if (rs != NULL) {
            u32* ofs = (u32*) ((u8*) SndMem.bgm_tbl + SndMem.bgm_tbl->room_ofs);
            SndBgmRoom* room = (SndBgmRoom*) ((u8*) ofs + ofs[i]);
            for (j = 0; j < 6; j++) {
                rs->bgm[j] = room->e[0].bgm[j];
                rs->str[j] = room->e[0].str[j];
            }
        }
        rl++;
        i++;
    }
}

// Starts the sound driver: reverb hooks, master volumes (SE / BGM / stream, both output types) to
// 0x7F, the four stream ARAM addresses and MRAM buffers.
void SndDriverInit()
{
    int i;

    Snd_system_init();
    AXFXSetHooks(reverb_mem_alloc, Mem_free);
    SndSetMasterVol(0x10001, 0x7F);
    SndSetMasterVol(0x10002, 0x7F);
    SndSetMasterVol(0x20001, 0x7F);
    SndSetMasterVol(0x40001, 0x7F);
    SndSetMasterVol(0x20002, 0x7F);
    SndSetMasterVol(0x40002, 0x7F);
    for (i = 0; i < 4; i++) {
        Snd_str_aram_adrs_set(i, SndStrAramAddr[i]);
        Snd_str_buff[i] = (u8*) SndMem.str_buf[i];
    }
}

// Full driver restart (AX / mixer / sequencer down and up again) — used on a soft reset.
void SndSystemReset()
{
    SEQQuit();
    SYNQuit();
    AXARTQuit();
    MIXQuit();
    AXQuit();
    AIReset();
    SndDriverInit();
}

// Pan (0..127, 64 centre) from the angle of the source around the listener (radians; behind is
// mirrored to the front).
s8 sndPanCalc(f32 h_angle)
{
    f32 a = fabsf(h_angle);
    s8 pan;

    if (fabsf(h_angle) >= 1.5707964f) {
        a = 3.1415927f - a;
    }
    if (h_angle >= 0.0f) {
        pan = (s8) (a * 40.743664f + 64.0f);
    } else {
        pan = (s8) (64.0f - a * 40.743664f);
    }
    return pan;
}

// Surround pan (127 in front, falling with |angle|).
s8 sndSpanCalc(f32 h_angle)
{
    return (s8) (127.0f - fabsf(h_angle) * 40.743664f);
}

s8 sndVolCalcSub(SndCurveTbl* t, f32 dist, f32 vol);
s16 sndPitchCalcSub(SndCurveTbl* t, f32 dist);

// .text order of the original: the callers precede their curve helpers.
// Volume through the room's distance curve `no` (SndRoomHdr vol_ofs); `vol` unchanged when the
// room has none.
int sndVolCalc(int iss_vol, int tbl_no, f32 dist)
{
    SndRoomHdr* h;
    u32 ofs;

    if (tbl_no == -1) {
        return iss_vol;
    }
    h = pSnd->hdr;
    if (h == NULL) {
        return iss_vol;
    }
    ofs = h->vol_ofs[tbl_no];
    if (ofs == 0) {
        return iss_vol;
    }
    return sndVolCalcSub((SndCurveTbl*) ((u8*) h + ofs), dist, (s8) iss_vol);
}

// Interpolates the curve's value at `dist` (clamped to the ends) and scales `vol` by it / 128.
s8 sndVolCalcSub(SndCurveTbl* t, f32 dist, f32 vol)
{
    u32 i;
    SndCurveEnt* e = t->e;
    f32 r;

    for (i = 0; i < t->num; i++, e++) {
        if (dist < e->dist) {
            break;
        }
    }
    if (i == 0) {
        r = (s16) e->val;
    } else if (i == t->num) {
        r = (s16) e[-1].val;
    } else {
        u16 v = e[-1].val;
        f32 d = (f32) ((s16) v - (s16) e->val) / (e->dist - e[-1].dist) * (dist - e[-1].dist);
        r = (f32) (s16) v - d;
    }
    // Dead test (the store is deleted by flow, the compare by jump2): it keeps `dist` live past
    // the three `r` sets so r cannot take f1 and lands in f2 (vol moved to f9) like the original.
    if (dist > vol) {
        i = 0;
    }
    return (s8) (vol * 0.0078125f * r);
}

// Pitch offset from the room's distance curve `no` (pitch_ofs); 0 when none.
s16 sndPitchCalc(int tbl_no, f32 dist)
{
    SndRoomHdr* h;
    u32 ofs;

    if (tbl_no == -1) {
        return 0;
    }
    h = pSnd->hdr;
    if (h == NULL) {
        return 0;
    }
    ofs = h->pitch_ofs[tbl_no];
    if (ofs == 0) {
        return 0;
    }
    return sndPitchCalcSub((SndCurveTbl*) ((u8*) h + ofs), dist);
}

// Interpolated curve value at `dist`.
s16 sndPitchCalcSub(SndCurveTbl* t, f32 dist)
{
    u32 i;
    SndCurveEnt* e = t->e;
    f32 r;

    for (i = 0; i < t->num; i++, e++) {
        if (dist < e->dist) {
            break;
        }
    }
    if (i == 0) {
        r = (s16) e->val;
    } else if (i == t->num) {
        r = (s16) e[-1].val;
    } else {
        u16 v = e[-1].val;
        f32 d = (f32) ((s16) v - (s16) e->val) / (e->dist - e[-1].dist) * (dist - e[-1].dist);
        r = (f32) (s16) v - d;
    }
    return (s16) r;
}

// Low-pass filter value from the room's distance curve `no` (filter_ofs, stepped, not
// interpolated); -1 when there is no curve.
static int sndFilterCalc(int no, f32 dist)
{
    int ret = 0;
    SndRoomHdr* h;
    SndCurveTbl* t;
    SndCurveEnt* e;
    u32 i;
    u32 num;

    if (no == -1) {
        ret = -1;
    } else {
        h = pSnd->hdr;
        if (h != NULL) {
            no = h->filter_ofs[no];   // the offset reuses the parameter (r3 -> `num` lands in r0)
            if (no != 0) {
                t = (SndCurveTbl*) ((u8*) h + no);
                e = t->e;
                num = t->num;
                for (i = 0; i < num; i++, e++) {
                    if (dist < e->dist) {
                        ret = (s8) e->val;
                        break;
                    }
                }
                if (i == t->num) {
                    ret = (s8) e[-1].val;
                }
            }
        }
    }
    return ret;
}

// 1 when block `blk` is loaded and has SE `no` (type != 0x8000).
int sndExistCheck(int blk, u32 call_no)
{
    u32* f = pSnd->blk_flag;

    if (!SND_BIT_CK(f, (u16) blk)) {
        return 0;
    }
    if (call_no >= Snd_iss_blk[blk].num) {
        return 0;
    }
    if (Snd_iss_get_sit_type(blk, call_no) == 0x8000) {
        return 0;
    }
    return 1;
}

int sndWallCheckSub(Vec* pos);

// Muffles the SE (sit->wall_vol percent, 1..99) when a wall (effect collision 0x404000) lies
// between the source and the player's head.
void sndWallCheck(SND_SIT* pSit, u8* vol, u8* svol, Vec* pos)
{
    if (pos == NULL) {
        return;
    }
    if (pSit->wall_vol < 1 || pSit->wall_vol > 99) {
        return;
    }
    if (sndWallCheckSub(pos) != 1) {
        return;
    }
    if (*vol != 0) {
        *vol = (s8) ((f32) (s8) *vol * ((f32) (s8) pSit->wall_vol / 100.0f));
        if ((s8) *vol <= 0) {
            *vol = 1;
        }
    }
    if (*svol != 0) {
        *svol = (s8) ((f32) (s8) *svol * ((f32) (s8) pSit->wall_vol / 100.0f));
        if ((s8) *svol <= 0) {
            *svol = 1;
        }
    }
}

// 1 when the effect collision blocks the line from `pos` to the player + 1500.
int sndWallCheckSub(Vec* pos)
{
    Vec a;
    Vec b;
    int ret = 0;

    a.x = pos->x;
    a.y = pos->y;
    a.z = pos->z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 1500.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) != 0) {
        ret = 1;
    }
    return ret;
}

// SEs flagged se_flag 0x20 drop to volume 1 when the player stands in a volume-control floor area
// (FlrAt kind 1) that does not contain the source.
void sndVolCtrlAtCheck(SND_SIT* pSit, u8* vol, u8* svol, Vec* pos)
{
    FlrAt* at;

    if (pos == NULL) {
        return;
    }
    if (!(pSit->se_flag & 0x20)) {
        return;
    }
    at = FlrAtCheck(1, &pPL->pos, 0);
    if (at == NULL) {
        return;
    }
    if (AreaHitCheck(at->area, pos) != 0) {
        return;
    }
    if (*vol != 0) {
        *vol = 1;
    }
    if (*svol != 0) {
        *svol = 1;
    }
}

// While the player is in an "inner" floor area (FlrAt kind 3), SEs with inner_vol are scaled by
// that percent. Returns 1 when applied.
int sndInnerVolCheck(SND_SIT* pSit, u8* vol, u8* svol)
{
    int ret = 0;

    if (pSit->inner_vol != 0 && FlrAtCheck(3, &pPL->pos, 0) != NULL) {
        if (*vol != 0) {
            *vol = (s8) ((f32) (s8) *vol * ((f32) (s8) pSit->inner_vol / 100.0f));
            if ((s8) *vol <= 0) {
                *vol = 1;
            }
        }
        if (*svol != 0) {
            *svol = (s8) ((f32) (s8) *svol * ((f32) (s8) pSit->inner_vol / 100.0f));
            if ((s8) *svol <= 0) {
                *svol = 1;
            }
        }
        ret = 1;
    }
    return ret;
}

void seRandomCheck(int blk, u16* no);

// Footstep SE selection: numbers 0x10..0x13 pick the surface variant (+30 per surface index) from
// the floor attribute; the others use the water check (+30) or the floor's surface (with the foot
// effect for 0..3, else the floor system default); then the random table and existence check.
int footSeCheck(u16* call_no, Vec* pos)
{
    FlrAt* at;
    int ret;

    if (pos != NULL) {
        if (*call_no >= 0x10 && *call_no <= 0x13) {
            at = FlrAtCheck(0, pos, 2);
            if (at == NULL) {
                goto check;
            }
            *call_no += at->se.se_type * FOOT_SE_NUM;
        } else if (EspPlWaterCall(*call_no >> 1, pos) == 1) {
            *call_no += 30;
        } else {
            at = FlrAtCheck(0, pos, 1);
            if (at != NULL) {
                if (*call_no <= 3) {
                    EspFootCall(*call_no >> 1, at->se.eff_type, pos);
                }
                *call_no += at->se.se_type * FOOT_SE_NUM;
            } else {
                if (*call_no <= 3) {
                    EspFootCall(*call_no >> 1, pFlrSys->foot_esp[pFlrSys->group], pos);
                }
                *call_no += pFlrSys->foot_se[pFlrSys->group] * FOOT_SE_NUM;
            }
        }
    }
check:
    seRandomCheck(5, call_no);
    ret = sndExistCheck(5, *call_no);
    if (ret == 0) {
        sndCallErr(5, *call_no);
    }
    return ret;
}

// Enemy SE: finds the loaded enemy block (8..13) whose enemy id matches (family aliases: 0x10 group,
// 0x11 group, 0x1D group; 0xFF = block 8), applies the random table, and refuses a repeat of the
// same id / number already in the 32-entry recent history (SndEmHist). Returns 1 to play.
int emSeCheck(u16* blk, u16* call_no, int id)
{
    int i;
    int ret;
    SndEmHist* h;
    SndEmHist* free;

    *blk = 0xFFFF;
    for (i = 0; i < 6; i++) {
        switch (id) {
        case 0xFF:
            *blk = 8;
            break;
        case 0x10:
        case 0x12:
        case 0x13:
            if (pSnd->snd_em_id[i] == 0x10) {
                *blk = i + 8;
            }
            break;
        case 0x11:
        case 0x14:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
            if (pSnd->snd_em_id[i] == 0x11) {
                *blk = i + 8;
            }
            break;
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
            if (pSnd->snd_em_id[i] == 0x1D) {
                *blk = i + 8;
            }
            break;
        default:
            if (pSnd->snd_em_id[i] == id) {
                *blk = i + 8;
            }
            break;
        }
    }
    if (*blk == 0xFFFF) {
        ret = 0;
    } else {
        seRandomCheck(*blk, call_no);
        ret = sndExistCheck(*blk, *call_no);
        if (ret == 1) {
            free = NULL;
            h = pSnd->em_hist;
            for (i = 0; i < 32; i++, h++) {
                if (h->used == 0) {
                    free = h;
                    continue;
                }
                if (h->id == (u16) id && h->no == *call_no) {
                    return 0;
                }
            }
            if (free != NULL) {
                free->id = id;
                free->used = 1;
                free->no = *call_no;
                free->timer = 1;
            }
        } else {
            sndCallErr(*blk, *call_no);
        }
    }
    return ret;
}

// Weapon SE: number 0xF (shell drop) picks the surface variant from the floor attribute.
int wepSeCheck(u16* call_no, Vec* pos)
{
    int ret;

    if (pos != NULL && *call_no == 0xF) {
        FlrAt* at = FlrAtCheck(0, pos, 4);
        if (at != NULL) {
            *call_no += at->se.cartridge_type;
        } else if (pFlrSys->pData != NULL) {
            *call_no += ((FlrAtHead*) pFlrSys->pData)->cartridge_type;
        }
    }
    ret = sndExistCheck(2, *call_no);
    if (ret == 0) {
        sndCallErr(2, *call_no);
    }
    return ret;
}

struct SndRndTbl {
    u16 num;
    u16 last;
    u16 e[1];
};

// SEs with a random group (SIT rnd_no) are replaced by a random member of the block's random
// table, avoiding the last one played (5 tries).
void seRandomCheck(int blk, u16* call_no)
{
    s8 retry = 5;
    SND_SIT* sit;
    s8 g;
    u8* data;
    u32* tbl;
    SndRndTbl* t;

    if (blk == 3 || blk == 4) {
        return;
    }
    if (sndExistCheck(blk, *call_no) == 0) {
        return;
    }
    sit = Snd_get_sit_adrs(blk, *call_no);
    g = sit->rnd_no;
    if (g == 0) {
        return;
    }
    data = SndMem.blk_mram[blk];
    tbl = (u32*) (data + ((u32*) data)[1]);
    if (tbl[g] == 0) {
        return;
    }
    t = (SndRndTbl*) ((u8*) tbl + tbl[g]);
    do {
        *call_no = *(t->e + Rnd() % t->num);
        retry--;
    } while (*call_no == t->last && retry != 0);
    t->last = *call_no;
}

// Enemy SE `no` of enemy `id` (block 8 + the enemy's block).
u32 EmSeCall(u16 call_no, Vec* pos, u8 id, u8 vol, u32 flag, cUnit* pMod)
{
    return SndCall(8, call_no, pos, id, vol | flag, pMod);
}

// Room SE `no` (block 6).
u32 RoomSeCall(u16 call_no, Vec* pos, u8 vol, u32 flag, cUnit* pMod)
{
    return SndCall(6, call_no, pos, 0, flag | vol, pMod);
}

// Player SE `no` (block 1).
u32 PlSeCall(u16 call_no, Vec* pos, u8 vol, u32 flag, cUnit* pMod)
{
    return SndCall(1, call_no, pos, 0, vol | flag, pMod);
}

// Core (system / common) SE `no` (block 0).
u32 CoreSeCall(u16 call_no, Vec* pos, u8 vol, u32 flag, cUnit* pMod)
{
    return SndCall(0, call_no, pos, 0, vol | flag, pMod);
}

// Footstep SE `no` (block 5) at `pos`.
u32 FootSeCall(u16 call_no, Vec* pos, u8 vol, u32 flag)
{
    return SndCall(5, call_no, pos, 0, vol | flag, 0);
}

// Door SE `no` (block 7) when the door block is loaded.
u32 DoorSeCall(u16 call_no)
{
    if (!SND_BIT_CK(pSnd->blk_flag, 7)) {
        return 0;
    }
    return SndCall(7, call_no, 0, 0, 0, 0);
}

void getCam2SndAngle(f32* pan, f32* span, f32* dist, Vec* pos);

// Plays SE `no` of block `blk` (SIT entry): block-specific number fix-ups, then pan / surround
// pan from the camera angle, volume / pitch / filter from the room's distance curves (curve_sel)
// unless the SE is 2D (srd_type 1), wall muffling, volume-control and inner areas, a fixed `vol`
// override (low byte; bits 0x100..0x400 = ctrl flags, 0x80000000 = follow `pos` / `obj`); the
// request goes to the driver (Snd_iss_req_para), BGM blocks 3 / 4 fill bgm_work, positional
// sounds get a SndSurWork so sndSurroundCalc keeps updating them. Returns the sound id, 0 when not
// played (missing SE, muted, volume 0, debug off).
u32 SndCall(u16 blk, u16 no, Vec* pos, int id, int vol, cUnit* obj)
{
    SND_CTRL_WORK* c = &Snd_ctrl_work;
    SND_SIT* sit;
    u32 snd_id;
    int ret;
    f32 pan_f;
    f32 dist;
    s8 v;
    s8 sv;
    s8 pan;
    s8 span;
    int pan_calc;
    int vol_calc;
    int curve_ok;
    int pan_ok;
    s8 svol_ofs = 0;
    s8 vol_ofs = 0;
    s8 pitch_ofs = 0;
    s8 filter_ofs = 0;
    int seq = 0;
    int inner = 0;
    int ok = 1;
    int i;

    if (DbgFlagChk(pG, DBG_SE_STOP)) {
        return 0;
    }
    pan_calc = 1;
    pan_ok = 1;
    vol_calc = 1;
    curve_ok = 1;
    switch (blk) {
    case 8:
        ret = emSeCheck(&blk, &no, id);
        break;
    case 5:
        ret = footSeCheck(&no, pos);
        break;
    case 2:
        ret = wepSeCheck(&no, pos);
        break;
    case 3:
    case 4:
    default:
        seRandomCheck(blk, &no);
        ret = sndExistCheck(blk, no);
        if (ret == 0) {
            sndCallErr(blk, no);
        }
        break;
    }
    if (ret == 0) {
        return 0;
    }

    c->ovr_flag = 0;
    sit = Snd_get_sit_adrs(blk, no);
    v = Snd_iss_get_sit_vol(blk, no);
    sv = Snd_iss_get_sit_svol(blk, no);
    pan = Snd_iss_get_sit_pan(blk, no);
    span = Snd_iss_get_sit_span(blk, no);

    if (sit->srd_type == 1 || (StaFlagChk(pG, STA_SUB_SCRN))) {
        vol_calc = 0;
        curve_ok = 0;
        pan_calc = 0;
        pan_ok = 0;
    } else if (sit->curve_no == -1) {
        curve_ok = 0;
    }
    if (sit->flag & 0x4) {
        seq = 1;
    }

    if (pos != NULL) {
        getCam2SndAngle(&pan_f, 0, &dist, pos);
        if (seq == 0 && pan_ok == 1) {
            s8 p = sit->pan;
            if (p < 0) {
                c->ovr_flag |= 0x2;
                pan = sndPanCalc(pan_f);
                c->pan = pan;
            } else {
                c->pan = sit->pan;
                pan = p;
                pan_calc = 0;
            }
            p = sit->span;
            if (p < 0) {
                c->ovr_flag |= 0x4;
                span = sndSpanCalc(pan_f);
                c->span = span;
            } else {
                c->span = sit->span;
                span = p;
                pan_calc = 0;
            }
            if (c->ovr_flag & 0x6) {
                c->ovr_flag |= 0x100;
                c->srd_type_ovr = 1;
            }
        }
    } else {
        vol_calc = 0;
        c->srd_type_ovr = 0;
        curve_ok = 0;
        c->ovr_flag |= 0x100;
        pan_calc = 0;
    }

    if (sit->curve_no >= 0 && pSnd->hdr != NULL && pSnd->hdr->curve_sel[sit->curve_no] != 0) {
        s8* cs = (s8*) pSnd->hdr + pSnd->hdr->curve_sel[sit->curve_no];
        if (curve_ok == 1) {
            int m = 1;
            int f;
            if (pSys->SndMode == 2) {
                m = 0;
            }
            vol_ofs = cs[1];
            svol_ofs = cs[0];
            v = sndVolCalc(v, vol_ofs, dist);
            sv = sndVolCalc(sv, svol_ofs, dist);
            c->ovr_flag |= 0x400;
            pitch_ofs = (cs + m)[2];
            c->pitch_ofs = sndPitchCalc(pitch_ofs, dist);
            filter_ofs = (cs + m)[4];
            f = sndFilterCalc(filter_ofs, dist);
            if (f != -1) {
                c->lpf_no = f;
                c->ovr_flag |= 0x80;
            }
        } else {
            vol_calc = 0;
        }
    } else {
        vol_calc = 0;
    }

    if ((u8) vol != 0) {
        vol_calc = 0;
        v = (s8) vol;
        sv = (s8) vol;
    }

    if (sit->aux_a == -1) {
        c->ovr_flag |= 0x20;
        if (pSnd->hdr != NULL) {
            SndEfxParam* p = &pSnd->hdr->efx[0];
            if (pSys->SndMode != 2) {
                p = &pSnd->hdr->efx[1];
            }
            switch (blk) {
            case 0:
                c->aux_a = (u8) p->Aux_core;
                break;
            case 1:
                c->aux_a = (u8) p->Aux_core;
                break;
            case 2:
                c->aux_a = (u8) p->Aux_weapon;
                break;
            case 5:
                c->aux_a = (u8) p->Aux_room;
                break;
            case 6:
                c->aux_a = (u8) p->Aux_room;
                break;
            case 8:
                c->aux_a = (u8) p->Aux_enemy;
                break;
            default:
                c->aux_a = 0;
                break;
            }
        } else {
            c->aux_a = 0;
        }
    }

    c->aux_b = 0;
    c->se_flag = 0;
    c->ovr_flag |= 0x40;
    if (sit->se_flag != 0) {
        c->ovr_flag |= 0x840;
        if (sit->se_flag & 0x2) {
            c->se_flag = 1;
        }
        if (sit->se_flag & 0x4) {
            c->se_flag |= 0x2;
        }
        if (sit->se_flag & 0x1) {
            c->se_flag |= 0x4;
        }
    }
    if (vol & ~0xFF) {
        c->ovr_flag |= 0x800;
        if (vol & 0x100) {
            c->se_flag |= 0x1;
        }
        if (vol & 0x200) {
            c->se_flag |= 0x2;
        }
        if (vol & 0x400) {
            c->se_flag |= 0x4;
        }
    }

    sndWallCheck(sit, (u8*) &v, (u8*) &sv, pos);
    sndVolCtrlAtCheck(sit, (u8*) &v, (u8*) &sv, pos);
    if (sit->inner_vol != 0) {
        if (pos == NULL) {
            inner = 1;
        }
        vol_calc = 1;
        sndInnerVolCheck(sit, (u8*) &v, (u8*) &sv);
    }

    // `ok` (set once at the top, tested here): the target's `li r0,1; cmpwi r0,0; bne` -- a
    // single-use constant local whose `li` update_equiv_regs moves next to the compare.
    if (v == 0 || sv == 0 || ok == 0) {
        return 0;
    }

    c->vol = v;
    c->svol = sv;
    c->ovr_flag |= 0x18;
    if (sit->srd_type == 1) {
        c->ovr_flag &= 0x860;
    }
    snd_id = Snd_iss_req_para(blk, no, 0);

    if (blk == 3 || blk == 4) {
        pSnd->bgm_state[blk - 3].used = 1;
        pSnd->bgm_state[blk - 3].id = snd_id;
        pSnd->bgm_state[blk - 3].vol = v;
        pSnd->bgm_state[blk - 3].vol_def = sit->vol;
        pSnd->bgm_state[blk - 3].no = no;
        OSReport("BGM%d seq %d play\n", blk - 3, no);
    }
    if (sit->srd_type == 3) {
        vol_calc = 0;
        pan_calc = 0;
    }
    if (snd_id != 0) {
        if (pan_calc != 0 || vol_calc != 0) {
            for (i = 0; i < 48; i++) {
                SndSurWork* w = &pSnd->sur[i];
                if (w->type == 0) {
                    u32 t = seq | 0x80;
                    w->type = t;
                    w->id = snd_id;
                    w->no = no;
                    w->blk = blk;
                    w->svol_ofs = svol_ofs;
                    w->vol_ofs = vol_ofs;
                    w->pitch_ofs = pitch_ofs;
                    w->filter_ofs = filter_ofs;
                    w->pan_calc = pan_calc;
                    w->vol_calc = vol_calc;
                    w->inner = inner;
                    w->obj = obj;
                    if (pos != NULL) {
                        w->pos.x = pos->x;
                        w->pos.y = pos->y;
                        w->pos.z = pos->z;
                        if ((vol & 0x80000000) || obj != NULL) {
                            w->ppos = pos;
                        }
                    }
                    break;
                }
            }
        }

        History.idx++;
        History.idx = LOOP_IDX(History.idx, 24);
        History.blk[History.idx] = blk;
        History.no[History.idx] = no;
        History.vol[History.idx] = v;
        History.svol[History.idx] = sv;
        History.pan[History.idx] = pan;
        History.span[History.idx] = span;
        History.num++;
        if (History.num > 25) {
            History.disp_idx++;
        }
        History.disp_idx = LOOP_IDX(History.disp_idx, 24);
        History.num = CLAMP(History.num, 0, 25);
    }
    return snd_id;
}

// Sets the volume of a playing sound `id` by its type (1 SE at once, 2 sequence / 4 stream faded
// over `time` frames). Returns 1 on success.
int SndSetVol(u32 id, int vol, int time)
{
    SND_CTRL_WORK* c = &Snd_ctrl_work;
    int ret = 0;

    switch (Snd_get_play_type(id)) {
    case 0:
        break;
    case 1:
        c->vol = vol;
        c->ovr_flag = 0x18;
        c->svol = vol;
        ret = Snd_se_set_paras(id) == 0;
        break;
    case 2:
        ret = Snd_seq_req(id, 1, time * 200, vol) == 0;
        break;
    case 4:
        ret = SndStrReq(id, 4, time * 200, vol);
        break;
    }
    return ret;
}

// Doppler pitch offset on a playing SE.
int SndSetDopPitch(u32 id, int pitch)
{
    SND_CTRL_WORK* c = &Snd_ctrl_work;
    int ret = 0;

    if (Snd_get_play_type(id) == 1) {
        c->pitch_ofs = pitch;
        c->ovr_flag = 0x400;
        ret = Snd_se_set_paras(id) == 0;
    }
    return ret;
}

// Stops sound `id`: SE at once, sequence / stream faded over `time` frames. Returns 1 on success.
int SndStop(u32 id, int time)
{
    int ret = 0;

    switch (Snd_get_play_type(id)) {
    case 0:
        break;
    case 1:
        ret = Snd_se_stop_one(id) == 0;
        break;
    case 2:
        ret = Snd_seq_req(id, 2, time * 200, 0) == 0;
        break;
    case 4:
        ret = SndStrReq(id, 8, time * 200, 0);
        break;
    }
    return ret;
}

// Stops every SE voice playing from block `blk`.
void SndBlkStop(int blk)
{
    BOOL lv = OSDisableInterrupts();
    int i;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        SND_VOICE_WORK* v = &Snd_voice_work[i];
        if (v->status != 0 && v->type == 1 && v->blk_no == blk) {
            Snd_se_stop_one(v->snd_id);
        }
    }
    OSRestoreInterrupts(lv);
}

// 1 when sound `id` has finished (or never played).
int SndEndCheck(u32 id)
{
    int ret = 0;

    switch (Snd_get_play_type(id)) {
    case 1:
        ret = Snd_se_end_check(id) == 0;
        break;
    case 2:
        ret = Snd_seq_end_check(id) == 0;
        break;
    case 4:
        ret = Snd_str_end_check(id) == 0;
        break;
    case 0:
        ret = 1;
        break;
    }
    return ret;
}

s8 pullStrWorkNo();
SndPlayWork* getStrWork(int blk, int no);
SndPlayWork* getStrWork(u32 id);

// Stream request: req bit0 = start stream `no` of block `blk` (0 room streams, 1 events) in a free
// str_work slot (a paused one of the same number just resumes; `pos` = start seconds), bit1 =
// make it the room's playing stream, 4 = volume `vol` over `time`, 8 = stop; other reqs address
// the stream by blk / no (no -1 = slot blk). Returns the stream id, 0 on failure / debug off.
u32 SndStrReq(int blk, int no, int flg, int time, int vol, f32 s_time)
{
    SndPlayWork* w;
    u32 smp = 0;

    if (DbgFlagChk(pG, DBG_BGM_STOP)) {
        return 0;
    }
    if (str_flag == 0) {
        OSReport("SND: No STR Header.\n");
        return 0;
    }
    if (flg & 0x1) {
        s8 wk;
        SndStrFile* sf;
        SndStrEnt* e;

        if (FileTbl[StrFileTbl[blk]].entrynum == -1) {
            OSReport("SND: File Not Found : %s\n", FileTbl[StrFileTbl[blk]].name);
            return 0;
        }
        if (no >= (int) SndMem.str_file[blk]->num) {
            pLog->err(0, 0, "SND : Stream Req No. %d Illegal Req No.", no);
            return 0;
        }
        w = getStrWork(blk, no);
        if (blk == 1 || flg >= 0) {
            w = NULL;
        }
        if (w == NULL) {
            wk = pullStrWorkNo();
            if (wk == -1) {
                return 0;
            }
            w = &pSnd->str_state[wk];
            if (s_time != 0.0f) {
                SND_SHD* shd = Snd_get_shd_adrs(blk, no);
                u32 bs = Snd_str_get_buff_smp(blk, no);
                smp = (u32) ((f32) shd->rate * s_time) / bs;
            }
        } else {
            if (w->stat != 1) {
                return 0;
            }
            SndSetVol(w->id, w->vol_def, 2);
            w->stat = 0;
            return w->id;
        }
        sf = SndMem.str_file[blk];
        e = (SndStrEnt*) ((u8*) sf + sf->ent_ofs);
        e += no;
        Snd_str_blk_init(blk, sf);
        w->id = Snd_str_prepare(blk, no, (char*) FileTbl[StrFileTbl[blk]].name, wk);
        w->blk = blk;
        w->no = no;
        if (blk == 1) {
            Snd_str_init_para(w->id, 0x1000, 2);
        }
        if (vol != 0) {
            w->vol = vol;
        } else {
            w->vol = e->vol;
            if (time != 0) {
                vol = (s8) e->vol;
            }
        }
        w->vol_def = e->vol;
        w->used = 1;
        if (s_time != 0.0f) {
            Snd_str_init_pos(w->id, smp);
        }
    } else {
        if (no != -1) {
            w = getStrWork(blk, no);
            if (w == NULL) {
                return 0;
            }
        } else {
            w = &pSnd->str_state[blk];
        }
    }
    if (w->id == 0) {
        w->used = 0;
        w->no = -1;
        w->stat = 0;
        return 0;
    }
    w->stat = (flg == 8 || (flg == 4 && vol == 0)) ? 1 : 0;
    if (flg & 0x2) {
        pSnd->play_str_no[blk] = no;
    }
    return Snd_str_req(w->id, flg, time, vol) ? 0 : w->id;
}

// Stream request on a playing stream id (4 volume, 8 stop...). Returns 1 on success.
int SndStrReq(u32 snd_id, int flg, int time, int vol)
{
    SndPlayWork* w;

    if (DbgFlagChk(pG, DBG_BGM_STOP)) {
        return 0;
    }
    w = getStrWork(snd_id);
    if (w == NULL) {
        return 0;
    }
    w->stat = (flg == 8 || (flg == 4 && vol == 0)) ? 1 : 0;
    return Snd_str_req(w->id, flg, time, vol) == 0;
}

// 1 when stream blk / no (no -1 = slot blk) is playing with one of the `status` bits; a dead
// stream frees its slot.
int SndStrStatusCk(int blk, int no, u32 status)
{
    SndPlayWork* w;
    int s;

    if (no == -1) {
        w = &pSnd->str_state[blk];
    } else {
        w = getStrWork(blk, no);
    }
    if (w == NULL) {
        return 0;
    }
    s = Snd_str_get_status(w->id);
    if (s != -1 && (s & 0x1)) {
        if (s & status) {
            return 1;
        }
        return 0;
    }
    w->id = 0;
    w->stat = 0;
    w->used = 0;
    return 0;
}

// 1 when stream `id` is playing with one of the `status` bits.
int SndStrStatusCk(u32 snd_id, u32 status)
{
    int s = Snd_str_get_status(snd_id);

    if (s != -1 && (s & 0x1) && (s & status)) {
        return 1;
    }
    return 0;
}

// A free stream slot (0..3), -1 when none.
s8 pullStrWorkNo()
{
    int i;

    for (i = 0; i < 4; i++) {
        if (pSnd->str_state[i].used == 0) {
            return i;
        }
    }
    return -1;
}

// The slot playing stream blk / no, or NULL.
SndPlayWork* getStrWork(int blk, int no)
{
    SndPlayWork* w;
    int i;

    for (i = 0; i < 4; i++) {
        if (pSnd->str_state[i].used == 0) {
            continue;
        }
        w = &pSnd->str_state[i];
        if (w->blk == blk && w->no == no) {
            return w;
        }
    }
    return NULL;
}

// The slot playing stream `id`, or NULL.
SndPlayWork* getStrWork(u32 snd_id)
{
    SndPlayWork* w;
    int i;

    for (i = 0; i < 4; i++) {
        if (pSnd->str_state[i].used == 0) {
            continue;
        }
        w = &pSnd->str_state[i];
        if (w->id == snd_id) {
            return w;
        }
    }
    return NULL;
}

// Fades stream blk / no to `vol` over `time`.
int SndStrVolSet(int blk, int no, int time, int vol)
{
    SndPlayWork* w = getStrWork(blk, no);
    int ret = 0;

    if (w != NULL) {
        ret = SndStrReq(w->id, 4, vol, time);
    }
    return ret;
}

// Fades stream blk / no back to its file volume.
int SndStrVolReset(int blk, int no, int time)
{
    SndPlayWork* w = getStrWork(blk, no);
    int ret = 0;

    if (w != NULL) {
        ret = SndStrReq(w->id, 4, time, w->vol_def);
    }
    return ret;
}

void sndSurroundCalc();
void debug_mute_check();
void debugDisp();

// Once per frame (main loop): positional SE update and ambient emitters (unless Stop_flg 0x800),
// the driver tick, house-keeping of the BGM / stream slots (finished ones freed, a stream paused
// for 300 frames is stopped), the enemy SE history timers, and the floor-attribute BGM control
// (FlrAt kind 2: per-slot volume set / reset and a stream start / stop while the player stands on
// it). Skipped while Status_flg[0] 0x10000000.
void SndWatcher()
{
    u32 i;
    FlrAt* at;
    FLR_AT_BGM_VOL* b;

    if (StaFlagChk(pG, STA_MOVIE_ON)) {
        return;
    }
    if (!SpfFlagChk(pG, SPF_SE_CALC)) {
        sndSurroundCalc();
        SeAtCheck();
    }
    Snd_iss_control();

    for (i = 0; i < 2; i++) {
        SndPlayWork* w = &pSnd->bgm_state[i];
        if (w->used == 1) {
            if (Snd_seq_end_check(w->id) == 0) {
                OSReport("SND: BGM %d STOP\n", i);
                memclr_asm(w, sizeof(SndPlayWork));
            } else {
                SND_SEQ_WORK* s = Snd_search_seq_work_snd_id(w->id);
                if (s != NULL) {
                    w->vol = s->vol2 >> 8;
                }
            }
        }
    }

    for (i = 0; i < 4; i++) {
        SndPlayWork* w = &pSnd->str_state[i];
        if (w->used == 1) {
            if (w->stat == 1) {
                w->timer++;
                if (w->timer > 299) {
                    SndStrReq(w->id, 8, 0, 0);
                    pLog->err(0, 0, "SND: str stop");
                    w->timer = 0;
                }
            } else {
                w->timer = 0;
            }
            if (Snd_str_end_check(w->id) == 0) {
                OSReport("SND: STREAM %d STOP\n", i);
                memclr_asm(w, sizeof(SndPlayWork));
            } else {
                SND_STR_WORK* s = Snd_search_str_work_snd_id(w->id);
                if (s != NULL) {
                    w->vol = s->vol2 >> 8;
                }
            }
        }
    }

    for (i = 0; i < 32; i++) {
        if (pSnd->em_hist[i].used != 0) {
            SndEmHist* h = &pSnd->em_hist[i];
            h->timer--;
            if (h->timer == 0) {
                memclr_asm(h, sizeof(SndEmHist));
            }
        }
    }

    if (!StaFlagChk(pG, STA_SUB_SCRN) && !SysFlagChk(pG, SYS_TYPEWRITER) && pSnd->room_ok != 0) {
        at = FlrAtCheck(2, &pPL->pos, 0xFF);
        if (at != NULL) {
            b = &at->bgmctrl;
            for (i = 0; i < 2; i++) {
                if ((b->blk_no >> i) & 0x1) {
                    if (pSnd->flrat_last_hit[i] != at->no) {
                        int r;
                        if ((b->sw >> i) & 0x1) {
                            r = SndRoomBgmVolSet(i, b->set_vol[i], b->time[i]);
                        } else {
                            r = SndRoomBgmVolReset(i, b->time[i]);
                        }
                        pSnd->flrat_last_hit[i] = (r == 1) ? at->no : -1;
                    }
                }
            }
            if (b->blk_no & 0x10) {
                if (b->sw & 0x10) {
                    if (b->str_fade_time != 0) {
                        SndStrReq(b->str_blk, b->str_no, 0x80000003, b->str_fade_time, 0, 0.0f);
                    } else {
                        SndStrReq(b->str_blk, b->str_no, 0x80000003, 0, 0, 0.0f);
                    }
                } else {
                    if (b->str_fade_time != 0) {
                        SndStrReq(b->str_blk, b->str_no, 4, b->str_fade_time, 0, 0.0f);
                    } else {
                        SndStrReq(b->str_blk, b->str_no, 8, 0, 0, 0.0f);
                    }
                }
            }
        }
    }
    debug_mute_check();
    debugDisp();
}

// Room change: streams not continued by the next room's save record (same number with the
// "keep" bits 0x8000 | 0x4000) are faded (200) or stopped.
void nextRoomStreamCheck()
{
    SndRoomSave* rs = (SndRoomSave*) RoomData.getRoomSavePtr(pG->RoomNo_next);
    int i;

    for (i = 0; i < 4; i++) {
        SndPlayWork* w = &pSnd->str_state[i];
        int stop = 0;
        if (rs == NULL) {
            stop = 1;
        } else {
            u16 s = (u16) rs->str[0];
            if (w->used != 0) {
                if (SysFlagChk(pG, SYS_LOAD_GAME) || ((pG->System_flg >> 19) & 1)) { // two tests, not merged into one mask
                    stop = 1;
                } else {
                    SND_STR_WORK* sw = Snd_search_str_work_snd_id(w->id);
                    if (sw->status & 0x8000) {
                        SndStrReq(w->id, 8, 0, 0);
                        continue;
                    }
                    if (!(s & 0x8000) || (u8) s != w->no || !(s & 0x4000)) {
                        stop = 1;
                    }
                }
            }
        }
        if (stop == 1) {
            if (SndStrStatusCk(w->blk, w->no, 0x10) != 0) {
                SndStrReq(i, -1, 4, 200, 0, 0.0f);
            } else {
                SndStrReq(w->id, 8, 0, 0);
            }
        }
    }
}

// Room change: BGM slot 0 keeps playing when the next room's record names the same BGM with the
// keep bit (0x8000; 0x4000 clear = fade it), else both slots fade out and their blocks are freed
// (the BGM MRAM / ARAM tops reset).
void nextRoomBgmCheck()
{
    SndRoomSave* rs = (SndRoomSave*) RoomData.getRoomSavePtr(pG->RoomNo_next);
    int i;
    int flag = 1;

    for (i = 0; i < 2; i++) {
        SndPlayWork* w = &pSnd->bgm_state[i];
        if (i == 0) {
            if (rs != NULL) {
                u16 b = (u16) rs->bgm[0];
                if ((b & 0x8000) && (u8) b == pSnd->snd_bgm_id[0]) {
                    flag = 0;
                    if (!(b & 0x4000) && w->used != 0) {
                        Snd_seq_req(w->id, 1, 200, 0);
                        w->stat = 1;
                    }
                }
            }
            if (SysFlagChk(pG, SYS_LOAD_GAME) || ((pG->System_flg >> 19) & 1)) { // two tests, not merged into one mask
                flag = 1;
            }
            if (flag == 1) {
                pSnd->mram_base_addr_bgm = SndMem.mram_end + 0x10000;
                pSnd->aram_base_addr_bgm = 0x700000;
                pSnd->snd_bgm_id[i] = 0xFF;
                SND_BIT_CLR(pSnd->blk_flag, i + 3);
                if (w->used != 0) {
                    Snd_seq_req(w->id, 1, 400, 0);
                    w->stat = flag;
                }
            } else {
                pSnd->mram_base_addr_bgm = SndMem.blk_mram[i + 3];
                pSnd->aram_base_addr_bgm = SndMem.blk_aram[i + 3];
            }
        } else {
            if (w->used != 0) {
                Snd_seq_req(w->id, 1, 400, 0);
                w->stat = 1;
            }
            pSnd->snd_bgm_id[i] = 0xFF;
            SND_BIT_CLR(pSnd->blk_flag, i + 3);
        }
    }
}

// Room change: stream / BGM continuation, all SEs faded (100 x 5 ms), the room-owned blocks (5
// foot, 6 room, 8..13 enemies) unloaded and the allocation tops reset.
void SndNextRoomInit()
{
    int i;

    nextRoomStreamCheck();
    nextRoomBgmCheck();
    SndSeAbsFadeOutAll_5msec(100);
    Snd_seq_fade_out_type(2, 100);
    memclr_asm(&pSnd->room_ok, sizeof(SndWork) - 0x90);
    SND_BIT_CLR(pSnd->blk_flag, 6);
    SND_BIT_CLR(pSnd->blk_flag, 5);
    memclr_asm(&Snd_iss_blk[6], sizeof(SND_ISS_BLK));
    memclr_asm(&Snd_iss_blk[5], sizeof(SND_ISS_BLK));
    UseAramSize[6] = 0;
    memclr_asm(callErr[6], sizeof(callErr[6]));
    UseAramSize[5] = 0;
    memclr_asm(callErr[5], sizeof(callErr[5]));
    for (i = 0; i < 6; i++) {
        pSnd->snd_em_id[i] = 0xFF;
        SND_BIT_CLR(pSnd->blk_flag, i + 8);
        memclr_asm(&Snd_iss_blk[i + 8], sizeof(SND_ISS_BLK));
        UseAramSize[i + 8] = 0;
        memclr_asm(callErr[i + 8], sizeof(callErr[i + 8]));
    }
    SndReadAddrInit();
}

// Resets the MRAM / ARAM allocation pointers for the room's sound blocks.
void SndReadAddrInit()
{
    pSnd->mram_top = SndMem.mram_end;
    pSnd->aram_base_addr = 0x1F4100;
}

// Pitch curve entries hold a signed value (lha/sth); SndCurveEnt::val is u16 for the other curves.
struct SndCurveEntS {
    f32 dist;
    u16 x4;
    s16 val;
};

// Room start: takes the room's "STB" sound header (reverb parameters, distance curves — scaled by
// their `scale`, pitch values x100) or a default one, sets the reverb, loads the room's BGM /
// stream numbers from its save record, clears the SE history. Always 0.
int SndRoomStartInit()
{
    u32 i;
    SndRoomSave* rs;
    SndEfxParam* e;

    pSnd->hdr = (SndRoomHdr*) GetDataExt(pG->pRoom, "STB", 0);
    memclr_asm(&DefEffTbl, sizeof(SndRoomHdr));
    for (i = 0, e = DefEffTbl.efx; i < 2; i++, e++) {
        e->Aux_core = 0;
        e->Aux_enemy = 0;
        e->Aux_weapon = 0;
        e->Aux_room = 0;
        e->Delay = 0.05f;
        e->Time = 1.0f;
        e->Coloration = 0.5f;
        e->Damping = 0.5f;
        e->Mix = 0.5f;
        e->Crosstalk = 0.5f;
    }
    if (pSnd->hdr == NULL) {
        pSnd->hdr = &DefEffTbl;
    }
    if (pSnd->hdr != NULL) {
        SndSetReverb();
        for (i = 0; i < 32; i++) {
            u32 ofs = pSnd->hdr->vol_ofs[i];
            SndCurveTbl* t;
            if (ofs != 0) {
                u32 j;
                SndCurveEnt* ce;
                t = (SndCurveTbl*) ((u8*) pSnd->hdr + ofs);
                ce = t->e;
                for (j = 0; j < t->num; j++, ce++) {
                    ce->dist *= t->scale;
                }
            }
            ofs = pSnd->hdr->pitch_ofs[i];
            if (ofs != 0) {
                u32 j;
                SndCurveEntS* ce;
                t = (SndCurveTbl*) ((u8*) pSnd->hdr + ofs);
                ce = (SndCurveEntS*) t->e;
                for (j = 0; j < t->num; j++, ce++) {
                    ce->dist *= t->scale;
                    ce->val *= 100;
                }
            }
            ofs = pSnd->hdr->filter_ofs[i];
            if (ofs != 0) {
                u32 j;
                SndCurveEnt* ce;
                t = (SndCurveTbl*) ((u8*) pSnd->hdr + ofs);
                ce = t->e;
                for (j = 0; j < t->num; j++, ce++) {
                    ce->dist *= t->scale;
                }
            }
        }
    }
    rs = (SndRoomSave*) RoomData.getRoomSavePtr(G_ROOM_ID);
    if (rs != NULL) {
        for (i = 0; i < 6; i++) {
            pSnd->room_bgm_tbl[i] = rs->bgm[i];
            pSnd->room_str_tbl[i] = rs->str[i];
        }
    }
    memclr_asm(&History, sizeof(SndHistory));
    History.idx = -1;
    pSnd->flrat_last_hit[0] = -1;
    pSnd->flrat_last_hit[1] = -1;
    pSnd->room_ok = 1;
    return 0;
}

// `DSE` sub-file of the room archive: door SE numbers per door for each room.
struct SndDoorRoom {
    u16 room;
    u16 door[5];
};
struct SndDoorSe {
    u32 num;
    SndDoorRoom e[1];
};

// Room change: looks the door SE up in the room's "DSE" table (by next_room and door_no) and reads
// its file (0x69 entry) into block 7 unless already loaded. Returns the read request, -1 when none.
int SndDoorSeLoad()
{
    u32 i;
    u16 cnt = *(u16*) ((u8*) SndMem.door_tbl + SndMem.door_tbl->num_ofs);
    u32 no = 0xFFFF;
    int ret = -1;
    SndDoorSe* d;

    d = (SndDoorSe*) GetDataExt(pG->pRoom, "DSE", 0);
    SND_BIT_CLR(pSnd->blk_flag, 7);
    memclr_asm(callErr[7], sizeof(callErr[7]));
    if (d != NULL && d->num != 0) {
        for (i = 0; i < d->num; i++) {
            if (d->e[i].room == pG->RoomNo_next) {
                no = d->e[i].door[pG->door_se];
                pG->door_se = 0;
                break;
            }
        }
        if (no < cnt && no != pSnd->doorse_id) {
#line 2389 SND_FILE
            ret = DvdRead(0x69, 0, 0, ((u32*) ((u8*) SndMem.door_tbl + SndMem.door_tbl->file_ofs))[no], 0, 0x8000, __FILE__, __LINE__);
            pSnd->doorse_id = no;
        } else if (no != 0xFFFF) {
            SND_BIT_SET(pSnd->blk_flag, 7);
        }
    }
    return ret;
}

// Room start: loads the BGM sequence blocks the room table names (bit15 set) for slots 0 / 1 that
// are not already resident (waits for a fading previous BGM first).
void SndRoomBgmLoad()
{
    int i;

    for (i = 0; i < 2; i++) {
        u16 b = (u16) (pSnd->room_bgm_tbl[0] >> (i * 16));
        if (b & 0x8000) {
            SndPlayWork* w = &pSnd->bgm_state[i];
            while (w->stat != 0) {
                SndWatcher();
            }
            if (w->used == 0) {
                SndBgmLoad((u8) b);
            }
        }
    }
}

// Room start (or `reset`): starts the BGM slots whose table entry (room_bgm_tbl[0], or the
// alternate table pG->snd_tbl_no after a continue / reset) has the auto-start bit 0x4000 (0x2000 =
// start at volume 1, faded in later).
void SndRoomBgmStartCheck(int flag)
{
    int i;
    int vol = 0;
    int start = 0;

    if (flag != 0) {
        pG->terminal_no = 0;
    }
    for (i = 0; i < 2; i++) {
        u16 b;
        if (SysFlagChk(pG, SYS_LOAD_GAME) || flag != 0) {
            b = (u16) (pSnd->room_bgm_tbl[pG->terminal_no + 1] >> (i * 16));
        } else {
            b = (u16) (pSnd->room_bgm_tbl[0] >> (i * 16));
        }
        if (b & 0x8000) {
            if (b & 0x4000) {
                start = 1;
            }
            if (b & 0x2000) {
                vol = 1;
                start = 1;
            }
            if (start == 1) {
                SndRoomBgmStart(i, vol);
            }
        }
    }
}

// Starts (or restarts at `vol`, 0 = default) BGM slot `no` with the sequence its table entry names
// (bits 8-9); a different running sequence is stopped first. Returns 1 when the slot has a BGM.
int SndRoomBgmStart(u8 blk_no, int vol)
{
    u16 b = (u16) (pSnd->room_bgm_tbl[0] >> (blk_no * 16));
    int seq = (b >> 8) & 0x3;
    int ret = 0;
    SndPlayWork* w;

    if (b & 0x8000) {
        w = &pSnd->bgm_state[blk_no];
        if (w->used == 0) {
            goto call;
        }
        if (seq != w->no) {
            Snd_seq_req(w->id, 2, 0, 0);
        call:
            do { do { SndCall(blk_no + 3, seq, 0, 0, vol, 0); } while (0); } while (0);
        } else {
            if (vol == 0) {
                vol = w->vol_def;
            }
            Snd_seq_req(w->id, 1, 1, vol);
        }
        w->stat = 0;
        ret = 1;
    }
    return ret;
}

// Stops BGM slot `no`, faded over `time` seconds (0 = at once); stat 1 = stopped by the game.
void SndRoomBgmStop(u8 blk_no, int fade_time)
{
    SndPlayWork* w = &pSnd->bgm_state[blk_no];

    if (w->used != 1 || w->stat != 0) {
        return;
    }
    w->stat = 1;
    if (fade_time != 0) {
        Snd_seq_req(w->id, 1, fade_time * 200, 0);
    } else {
        Snd_seq_req(w->id, 2, 0, 0);
    }
}

// Fades BGM slot `no` to `vol` over `time` (driver units) when it is playing.
int SndRoomBgmVolSet(u8 blk_no, int vol, int time)
{
    SndPlayWork* w = &pSnd->bgm_state[blk_no];
    int ret = 0;

    if (w->used == 1 && w->stat == 0) {
        ret = Snd_seq_req(w->id, 1, time, vol) == 0;
    }
    return ret;
}

// Fades BGM slot `no` back to its default volume.
int SndRoomBgmVolReset(u8 blk_no, int time)
{
    SndPlayWork* w = &pSnd->bgm_state[blk_no];
    int ret = 0;

    if (w->used == 1 && w->stat == 0) {
        ret = Snd_seq_req(w->id, 1, time, w->vol_def) == 0;
    }
    return ret;
}

// Ducks BGM slot `no` to volume 1 (remembering the current / fading volume) or restores it, over
// `time` seconds (-1 = at once). Returns 1 when something changed.
int SndRoomBgmMute(u8 blk_no, int sw, int time)
{
    SndPlayWork* w = &pSnd->bgm_state[blk_no];
    int ret = 0;
    int t = (time == -1) ? 1 : time * 200;

    if (w->used == 1 && w->stat == 0) {
        if (sw == 1) {
            if (Snd_seq_fade_check(w->id) == 1) {
                w->mute_vol = Snd_search_seq_work_snd_id(w->id)->fade_vol;
            } else {
                w->mute_vol = w->vol;
            }
            Snd_seq_req(w->id, 1, t, 1);
            ret = 1;
        } else {
            if (w->mute_vol != 0) {
                Snd_seq_req(w->id, 1, t, w->mute_vol);
                ret = 1;
            }
            w->mute_vol = 0;
        }
    }
    return ret;
}

// Ducks / restores both BGM slots.
void SndRoomBgmMuteAll(int sw, int time)
{
    SndRoomBgmMute(0, sw, time);
    SndRoomBgmMute(1, sw, time);
}

// Room start: starts the room stream when its table entry (room_str_tbl, or the alternate table
// on a continue) has the auto-start bit.
void SndRoomStrStartCheck()
{
    u32 s;

    if (SysFlagChk(pG, SYS_LOAD_GAME)) {
        s = pSnd->room_str_tbl[pG->terminal_no + 1];
    } else {
        s = pSnd->room_str_tbl[0];
    }
    if (s & 0x8000) {
        if (s & 0x4000) {
            SndRoomStrStart(1, 0, 1);
        }
    }
}

// Starts the room's stream (room_str_tbl[0], low byte = number) looping when `loop`, faded in over
// `time` seconds; a paused one is faded back to its default volume.
void SndRoomStrStart(int flag, int time, int play_ck)
{
    int req = 3;

    if (play_ck == 1) {
        req = 0x80000003;
    }
    if (pSnd->room_str_tbl[0] & 0x8000) {
        u8 no = (u8) pSnd->room_str_tbl[0];
        if (SndStrStatusCk(0, no, 0x10) == 0) {
            if (time != 0) {
                SndStrReq(0, no, req, time * 200, 0, 0.0f);
            } else {
                SndStrReq(0, no, req, 0, 0, 0.0f);
            }
        } else {
            SndPlayWork* w = getStrWork(0, no);
            if (w->stat != 0) {
                SndStrReq(w->id, 4, 600, w->vol_def);
            }
        }
    }
}

// Stops the room stream, faded over `time` seconds (0 = at once).
void SndRoomStrStop(int fade_time)
{
    SndPlayWork* w = getStrWork(0, (u8) pSnd->room_str_tbl[0]);

    if (w == NULL || w->stat != 0) {
        return;
    }
    if (fade_time != 0) {
        SndStrReq(w->id, 4, fade_time * 200, 0);
    } else {
        SndStrReq(w->id, 8, 0, 0);
    }
}

// Fades the room stream to `vol`.
int SndRoomStrVolSet(int vol, int time)
{
    SndPlayWork* w = getStrWork(0, (u8) pSnd->room_str_tbl[0]);
    int ret = 0;

    if (w != NULL) {
        ret = SndStrReq(w->id, 4, time, vol);
    }
    return ret;
}

// Fades the room stream back to its default volume.
int SndRoomStrVolReset(int time)
{
    SndPlayWork* w = getStrWork(0, (u8) pSnd->room_str_tbl[0]);
    int ret = 0;

    if (w != NULL) {
        ret = SndStrReq(w->id, 4, time, w->vol_def);
    }
    return ret;
}

void sndMuteSetMain(SndMute* m, u32 type, int on);

// Mutes / unmutes output groups: bit4 SE (headphones type), bit5 BGM, bit6 SE (TV), bit7 BGM (TV).
void SndMuteSet(int kind, int sw)
{
    if (kind & 0x10) {
        sndMuteSetMain(&Snd.mute[0], 0x20001, sw);
    }
    if (kind & 0x20) {
        sndMuteSetMain(&Snd.mute[1], 0x40001, sw);
    }
    if (kind & 0x40) {
        sndMuteSetMain(&Snd.mute[2], 0x20002, sw);
    }
    if (kind & 0x80) {
        sndMuteSetMain(&Snd.mute[3], 0x40002, sw);
    }
}

// Mutes one group by saving its master volume and setting 0, or restores it.
void sndMuteSetMain(SndMute* pMute, u32 type, int sw)
{
    if (sw == 1) {
        if (pMute->on == 0) {
            pMute->vol = SndGetMasterVol(type);
            SndSetMasterVol(type, 0);
            pMute->on = sw;
        }
    } else if (pMute->on == 1) {
        SndSetMasterVol(type, pMute->vol);
        pMute->on = 0;
    }
}

// Master volume `vol` (0..0x7F) of a driver channel: type bit0 / bit1 = output kind, 0x10000 SE,
// 0x20000 sequence, 0x40000 stream. Returns 1 when the type is valid.
int SndSetMasterVol(u32 kind, int vol)
{
    int ret = 0;
    u32 t = 0;

    if (kind & 0x1) {
        if (kind & 0x10000) {
            t = 0x2;
        } else if (kind & 0x20000) {
            t = 0x8;
        } else if (kind & 0x40000) {
            t = 0x20;
        }
    } else if (kind & 0x2) {
        if (kind & 0x10000) {
            t = 0x1;
        } else if (kind & 0x20000) {
            t = 0x4;
        } else if (kind & 0x40000) {
            t = 0x10;
        }
    }
    if (t != 0) {
        Snd_set_system_vol(t, vol);
        Snd_reset_vol_all();
        ret = 1;
    }
    return ret;
}

// Current master volume of a driver channel (see SndSetMasterVol), -1 for an invalid type.
int SndGetMasterVol(u32 kind)
{
    u32 t = 0;

    if (kind & 0x1) {
        if (kind & 0x10000) {
            t = 0x2;
        } else if (kind & 0x20000) {
            t = 0x8;
        } else if (kind & 0x40000) {
            t = 0x20;
        }
    } else if (kind & 0x2) {
        if (kind & 0x10000) {
            t = 0x1;
        } else if (kind & 0x20000) {
            t = 0x4;
        } else if (kind & 0x40000) {
            t = 0x10;
        }
    }
    return (t != 0) ? (s8) Snd_get_system_vol(t) : -1;
}

// Output mode 0 mono / 1 stereo / 2 surround (DPL2): from the save at `init`, else applied now
// (reverb re-set, pans / volumes recomputed); the movie output follows (ADXT mono).
void SndSetOutputMode(int mode, int flg)
{
    int efx = Snd_efx_get_status(0) == 1;

    if (flg != 0) {
        pSys->SndMode = Snd_sound_mode_init_load(mode);
    } else {
        if (efx == 1) {
            Snd_efx_req(0, 0);
        }
        pSys->SndMode = mode;
        Snd_set_sound_mode(mode);
        if (efx == 1) {
            SndSetReverb();
        }
        Snd_reset_pan_all();
        Snd_reset_vol_all();
    }
    if (pSys->SndMode == 0) {
        ADXT_SetOutputMono(1);
    } else {
        ADXT_SetOutputMono(0);
    }
}

// Angle of `pos` around the listener: yaw (pan) and elevation (span) in a frame at the player's
// position aligned with the camera, and the distance from the camera.
void getCam2SndAngle(f32* h_angle, f32* v_angle, f32* dist, Vec* pos)
{
    Camera* cam = &pG->Camera;
    Vec out;
    Vec fwd;

    if (pos == NULL) {
        return;
    }
    Vec up = { 0.0f, 1.0f, 0.0f };
    Vec right;
    Mtx m;
    Mtx inv;
    fwd.x = pG->Camera.mat[0][0];
    fwd.y = pG->Camera.mat[1][0];
    fwd.z = pG->Camera.mat[2][0];
    PSVECCrossProduct(&fwd, &up, &right);
    MTXSetColumns(m, fwd, up, right, pPL->pos);
    PSMTXInverse(m, inv);
    PSMTXMultVec(inv, pos, &out);
    if (h_angle != NULL) {
        *h_angle = atan2f(out.x, -out.z);
    }
    if (v_angle != NULL) {
        *v_angle = atan2f(out.y, -out.z);
    }
    if (dist != NULL) {
        f32 dx = pos->x - cam->param.pos.x;
        f32 dy = pos->y - cam->param.pos.y;
        f32 dz = pos->z - cam->param.pos.z;
        *dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
}

// Per frame: every tracked positional sound (SndSurWork) is re-panned and re-attenuated from its
// current position (following `ppos` / the owning unit while alive): distance curves, wall /
// volume-control / inner areas, pitch and filter; a sound that attenuates to 0 is stopped;
// finished ones free their slot. Sequences only get their volume refreshed.
void sndSurroundCalc()
{
    static int (*end_check_tbl[2])(u32) = { Snd_se_end_check, Snd_seq_end_check };
    int i;
    SND_CTRL_WORK* c = &Snd_ctrl_work;
    f32 pan;
    f32 dist;

    for (i = 0; i < 48; i++) {
        SndSurWork* w = &pSnd->sur[i];
        SND_SIT* sit;
        int idx;
        if (w->type == 0) {
            continue;
        }
        c->ovr_flag = 0;
        idx = 0;
        if (w->type & 0x1) {
            idx = 1;
        }
        switch (end_check_tbl[idx](w->id)) {
        case 0:
            memclr_asm(w, sizeof(SndSurWork));
            break;
        case 1:
            sit = Snd_get_sit_adrs(w->blk, w->no);
            if (w->obj != NULL) {
                if (w->obj->isAlive()) {
                    getCam2SndAngle(&pan, 0, &dist, w->ppos);
                    w->pos.x = w->ppos->x;
                    w->pos.y = w->ppos->y;
                    w->pos.z = w->ppos->z;
                } else {
                    getCam2SndAngle(&pan, 0, &dist, &w->pos);
                }
            } else if (w->ppos != NULL) {
                getCam2SndAngle(&pan, 0, &dist, w->ppos);
                w->pos.x = w->ppos->x;
                w->pos.y = w->ppos->y;
                w->pos.z = w->ppos->z;
            } else {
                getCam2SndAngle(&pan, 0, &dist, &w->pos);
            }
            if (w->inner != 0) {
                c->ovr_flag |= 0x18;
                c->vol = sit->vol;
                c->svol = sit->svol;
                sndInnerVolCheck(sit, &c->vol, &c->svol);
            } else {
                if (w->vol_calc != 0) {
                    c->ovr_flag |= 0x418;
                    c->vol = sndVolCalc(Snd_iss_get_sit_vol(w->blk, w->no), w->vol_ofs, dist);
                    c->svol = sndVolCalc(Snd_iss_get_sit_svol(w->blk, w->no), w->svol_ofs, dist);
                    sndWallCheck(sit, &c->vol, &c->svol, &w->pos);
                    sndVolCtrlAtCheck(sit, &c->vol, &c->svol, &w->pos);
                    sndInnerVolCheck(sit, &c->vol, &c->svol);
                    c->pitch_ofs = sndPitchCalc(w->pitch_ofs, dist);
                    c->lpf_no = sndFilterCalc(w->filter_ofs, dist);
                    c->ovr_flag |= 0x80;
                }
                if (w->pan_calc != 0) {
                    c->ovr_flag |= 0x6;
                    if (sit->pan & 0x80) {
                        c->pan = sndPanCalc(pan);
                    } else {
                        c->ovr_flag &= ~0x2;
                        c->pan = sit->pan;
                    }
                    if (sit->span & 0x80) {
                        c->span = sndSpanCalc(pan);
                    } else {
                        c->ovr_flag &= ~0x4;
                        c->span = sit->span;
                    }
                }
            }
            if (c->vol == 0 || c->svol == 0) {
                SndStop(w->id, 0);
            } else {
                Snd_se_set_paras(w->id);
            }
            break;
        case 2: {
            int vol;
            int svol;
            sit = Snd_get_sit_adrs(w->blk, w->no);
            getCam2SndAngle(&pan, 0, &dist, &w->pos);
            vol = sndVolCalc(Snd_iss_get_sit_vol(w->blk, w->no), w->vol_ofs, dist);
            svol = sndVolCalc(Snd_iss_get_sit_svol(w->blk, w->no), w->svol_ofs, dist);
            if (pSys->SndMode == 2) {
                Snd_seq_req(w->id, 1, 10, svol);
            } else {
                Snd_seq_req(w->id, 1, 10, vol);
            }
            break;
        }
        }
    }
}

// 1 when no SE and no type 2 sequence is sounding any more (and the reverb is switched off).
int SndStopCheck()
{
    if (Snd_se_pronounce_ck_all() != 0) {
        return 0;
    }
    if (Snd_seq_pronounce_ck_type(2) != 0) {
        return 0;
    }
    Snd_efx_req(0, 0);
    return 1;
}

// Stops everything at once: SEs, sequences (type 3) and the four streams.
void SndAllStop()
{
    int i;

    SndSeAbsFadeOutAll_sec(0);
    Snd_seq_fade_out_type(3, 0);
    for (i = 0; i < 4; i++) {
        if (pSnd->str_state[i].used != 0) {
            SndStrReq(pSnd->str_state[i].id, 8, 0, 0);
        }
    }
}

// Fades everything out over 2 seconds.
void SndAllFadeOut()
{
    int i;

    SndSeAbsFadeOutAll_sec(2);
    SndSeqFadeOutAll_sec(3, 2);
    for (i = 0; i < 4; i++) {
        if (pSnd->str_state[i].used != 0) {
            SndStrReq(i, -1, 4, 400, 0, 0.0f);
        }
    }
}

// Pauses / resumes the SEs of `type` (-1 = all).
void SndSePause(int sw, s16 blk)
{
    if (sw == 1) {
        Snd_se_pause_on2(blk);
    } else {
        Snd_se_pause_off2(blk);
    }
}

// Pauses all SEs unconditionally (pause menu).
void SndSeAbsPause()
{
    Snd_se_pause_on3();
}

// Pauses / resumes all SEs.
void SndSePauseAll(int sw)
{
    SndSePause(sw, -1);
}

// Soft reset of the driver: waits for the reset to complete, reverb off.
void SndSoftReset()
{
    Snd_soft_reset_req();
    while (Snd_soft_reset_ck() != 0) {
        Snd_iss_control();
    }
    Snd_efx_req(0, 0);
}

// Scenario: switches room `room` to BGM / stream table entry `no` of the BGM table file (its six
// BGM and stream words into the room's save record, and into the live tables when it is the
// current room). Returns 1 when found.
int SndBgmTblSet(u16 room_no, int tbl_no)
{
    SndRoomSave* rs = (SndRoomSave*) RoomData.getRoomSavePtr(room_no);
    SndBgmRoom* r = NULL;
    int ret = 0;
    u16* rl;
    u8 i;
    u8 j;
    u8 k;

    if (rs != NULL) {

        rl = (u16*) ((u8*) SndMem.bgm_tbl + SndMem.bgm_tbl->list_ofs);
        i = 0;
        // no block-scope declaration inside the body: that would stop GCC duplicating the exit test
        while (*rl != 0xFFFF) {
            if (*rl == room_no) {
                r = (SndBgmRoom*) ((u8*) SndMem.bgm_tbl + SndMem.bgm_tbl->room_ofs + ((u32*) ((u8*) SndMem.bgm_tbl + SndMem.bgm_tbl->room_ofs))[i]);
                break;
            }
            rl++;
            i++;
        }
        if (r != NULL) {
            for (j = 0; j < r->num; j++) {
                if (r->e[j].id == tbl_no) {
                    for (k = 0; k < 6; k++) {
                        rs->bgm[k] = r->e[j].bgm[k];
                        rs->str[k] = r->e[j].str[k];
                        if (room_no == pG->room_id) {
                            pSnd->room_bgm_tbl[k] = r->e[j].bgm[k];
                            pSnd->room_str_tbl[k] = r->e[j].str[k];
                        }
                    }
                    ret = 1;
                    break;
                }
            }
        }
    }
    return ret;
}

// Sets the auto-start bits of the room's BGM slot 0 (type bit0) / slot 1 (bit1) / stream (bit2)
// table words, also in the save record when `save`.
void SndBgmTblSetEnable(int kind, int tbl_update)
{
    SndRoomSave* rs = (SndRoomSave*) RoomData.getRoomSavePtr(G_ROOM_ID);
    int i;

    for (i = 0; i < 6; i++) {
        if (kind & 0x1) {
            if (pSnd->room_bgm_tbl[i] & 0x8000) {
                pSnd->room_bgm_tbl[i] |= 0x4000;
            }
            if (tbl_update == 1 && rs != NULL) {
                if (rs->bgm[i] & 0x8000) {
                    rs->bgm[i] |= 0x4000;
                }
            }
        }
        if (kind & 0x2) {
            if (pSnd->room_bgm_tbl[i] & 0x80000000) {
                pSnd->room_bgm_tbl[i] |= 0x40000000;
            }
            if (tbl_update == 1 && rs != NULL) {
                if (rs->bgm[i] & 0x80000000) {
                    rs->bgm[i] |= 0x40000000;
                }
            }
        }
        if (kind & 0x4) {
            if (pSnd->room_str_tbl[i] & 0x8000) {
                pSnd->room_str_tbl[i] |= 0x4000;
            }
            if (tbl_update == 1 && rs != NULL) {
                if (rs->str[i] & 0x8000) {
                    rs->str[i] |= ~0x4000;
                }
            }
        }
    }
}

// Clears the auto-start bits (see SndBgmTblSetEnable).
void SndBgmTblSetDisable(int kind, int tbl_update)
{
    SndRoomSave* rs = (SndRoomSave*) RoomData.getRoomSavePtr(G_ROOM_ID);
    int i;

    for (i = 0; i < 6; i++) {
        if (kind & 0x1) {
            if (pSnd->room_bgm_tbl[i] & 0x8000) {
                pSnd->room_bgm_tbl[i] &= ~0x4000;
            }
            if (tbl_update == 1 && rs != NULL) {
                if (rs->bgm[i] & 0x8000) {
                    rs->bgm[i] &= ~0x4000;
                }
            }
        }
        if (kind & 0x2) {
            if (pSnd->room_bgm_tbl[i] & 0x80000000) {
                pSnd->room_bgm_tbl[i] &= ~0x40000000;
            }
            if (tbl_update == 1 && rs != NULL) {
                if (rs->bgm[i] & 0x80000000) {
                    rs->bgm[i] &= ~0x40000000;
                }
            }
        }
        if (kind & 0x4) {
            if (pSnd->room_str_tbl[i] & 0x8000) {
                pSnd->room_str_tbl[i] &= ~0x4000;
            }
            if (tbl_update == 1 && rs != NULL) {
                if (rs->str[i] & 0x8000) {
                    rs->str[i] &= ~0x4000;
                }
            }
        }
    }
}

// Sub screen opened: SEs paused, BGM (TV) volume halved, Stop_flg 0x800 (no positional update).
void SndSubScreenInit()
{
    SpfFlagOn(pG, SPF_SE_CALC);
    SndSetMasterVol(0x10002, 0x3F);
    SndSePauseAll(1);
}

// Sub screen closed: volumes and SEs back.
void SndSubScreenExit()
{
    SndSetMasterVol(0x10002, 0x7F);
    SndSePauseAll(0);
    SpfFlagOff(pG, SPF_SE_CALC);
}

// Stops the event streams (block 1), faded over `time` seconds.
void SndEventStrStop(int time)
{
    u32 i;

    for (i = 0; i < 4; i++) {
        SndPlayWork* w = &pSnd->str_state[i];
        if (w->used != 0 && w->blk == 1) {
            if (time != 0) {
                SndStrReq(w->id, 4, time * 200, 0);
            } else {
                SndStop(w->id, 0);
            }
        }
    }
}

// Event start: SEs faded out (400) and paused, BGM ducked, Stop_flg 0x800.
void SndEventInit()
{
    SpfFlagOn(pG, SPF_SE_CALC);
    Snd_se_fade_out_all(400);
    SndSePauseAll(1);
    SndRoomBgmMuteAll(1, 2);
}

// Event end: SEs resume, BGM slots unducked — a slot that was not playing is started at its SIT
// wall_vol volume when the room table names one.
void SndEventEnd()
{
    int i;

    SpfFlagOff(pG, SPF_SE_CALC);
    SndSePauseAll(0);
    for (i = 0; i < 2; i++) {
        u8 no = i;
        if (SndRoomBgmMute(no, 0, 1) == 0 && pSnd->bgm_state[i].used == 0) {
            u16 b = (u16) (pSnd->room_bgm_tbl[0] >> (i * 16));
            if (b & 0x8000) {
                SND_SIT* sit = Snd_get_sit_adrs((u16) (i + 3), (b >> 8) & 0x3);
                s8 vol = sit->wall_vol;
                if (vol != 0) {
                    SndRoomBgmStart(no, vol);
                }
            }
        }
    }
}

// Enemy block slot (0..5) to load enemy `id`'s sounds into: the first free one; -1 when the id is
// already loaded or all six are used.
int SndEmDataReadCheck(int em_id)
{
    int i;
    u32* f = pSnd->blk_flag;

    for (i = 0; i < 6; i++) {
        if (!SND_BIT_CK(f, i + 8)) {
            return i;
        }
        if (pSnd->snd_em_id[i] == em_id) {
            return -1;
        }
    }
    return -1;
}

// Registers a loaded sound block with the driver: type 8 = enemy block `no` for enemy `id` (block
// 8 + no), 3 = BGM slot `no` (block 3 + no), else block `type`; ARAM / MRAM addresses from SndMem.
void SndBlkInit(int type, int id, int no)
{
    int blk = type;
    u32 adr;

    switch (type) {
    case 8:
        pSnd->snd_em_id[no] = id;
        blk = no + 8;
        OSReport("SND: blk %d, ID 0x%x\n", blk, id);
        break;
    case 3:
        pSnd->snd_bgm_id[no] = id;
        blk = no + 3;
        break;
    }
    Snd_iss_blk[blk].aram = SndMem.blk_aram[blk];
    adr = (u32) SndMem.blk_mram[blk];
    if (blk != 3 && blk != 4) {
        adr += *(u32*) adr;
    }
    Snd_iss_blk_init(blk, (void*) adr);
    SND_BIT_SET(pSnd->blk_flag, blk);
}

// Reads BGM `no`'s data (file 0x61 entry) unless already resident.
void SndBgmLoad(int bgm_no)
{
    int r;

    if (SndBgmDataReadCheck(bgm_no) == -1) {
        return;
    }
#line 3784 SND_FILE
    r = DvdRead(0x61, 0, 0, SndMem.bgm_file[bgm_no], 0, 0x8001, __FILE__, __LINE__);
    Dvd.ReadCheck(r, 0, 0, 0);
}

// BGM slot (0 / 1) to load BGM `id` into: the first free one; -1 when loaded or both used.
int SndBgmDataReadCheck(int bgm_no)
{
    int i;
    u32* f = pSnd->blk_flag;

    for (i = 0; i < 2; i++) {
        if (!SND_BIT_CK(f, i + 3)) {
            return i;
        }
        if (pSnd->snd_bgm_id[i] == bgm_no) {
            return -1;
        }
    }
    return -1;
}

// Applies the room's reverb parameters (STB efx[0] for DPL2, efx[1] for the HI reverb) to the
// driver.
void SndSetReverb()
{
    SND_EFX_WORK* w = &Snd_efx_work[0];
    SndEfxParam* p;

    if (pSys->SndMode == 2) {
        p = &pSnd->hdr->efx[0];
        w->fx.dpl2.tempDisableFX = 0;
        w->fx.dpl2.preDelay = p->Delay;
        w->fx.dpl2.time = p->Time;
        w->fx.dpl2.coloration = p->Coloration;
        w->fx.dpl2.damping = p->Damping;
        w->fx.dpl2.mix = p->Mix;
        Snd_efx_req(0, 5);
    } else {
        p = &pSnd->hdr->efx[1];
        w->fx.hi.tempDisableFX = 0;
        w->fx.hi.preDelay = p->Delay;
        w->fx.hi.time = p->Time;
        w->fx.hi.coloration = p->Coloration;
        w->fx.hi.damping = p->Damping;
        w->fx.hi.crosstalk = p->Crosstalk;
        w->fx.hi.mix = p->Mix;
        Snd_efx_req(0, 1);
    }
}

// Debug: Debug_flg[2] 0x80000 / 0x100000 mute the SE / BGM groups while set.
void debug_mute_check()
{
    static u8 flag_bak = 0; // explicit `= 0` puts it in .sdata (GCC 2.95 keeps zero initializers out of bss)
    u8 f = 0;
    u32 chg;
    u32 off;
    u32 on;

    if (DbgFlagChk(pG, DBG_SE_STOP)) {
        f = 1;
    }
    if (DbgFlagChk(pG, DBG_BGM_STOP)) {
        f |= 0x2;
    }
    {
        u8 bak = flag_bak;
        chg = f ^ bak;
        on = f & chg;
        off = bak & chg;
    }
    if (on & 0x1) {
        SndMuteSet(0x30, 1);
    } else if (off & 0x1) {
        SndMuteSet(0x30, 0);
    }
    if (on & 0x2) {
        SndMuteSet(0xC0, 1);
    } else if (off & 0x2) {
        SndMuteSet(0xC0, 0);
    }
    flag_bak = f;
}

// Room-change wait screen: prints the BGM / stream slots while sounds are still stopping and the
// read `req` is pending; returns 1 when both are done.
int SndStatDisp(int read_id)
{
    u32 i;
    int y = 0x10;
    int y2 = 0x10;
    int ret = 0;
    int stop = SndStopCheck();
    int read = Dvd.ReadCheck(read_id, 0, 0, 0);

    if (stop == 0) {
        eprintf(0x18, 0x10, 0, 0, "Stop WAIT");
        y = 0x20;
        for (i = 0; i < 2; i++) {
            SndPlayWork* w = &pSnd->bgm_state[i];
            eprintf2(0xA, 0x10, 0x78, y2, 0, 0, "%2d %2d %3d %3d %3d %3d", w->used, w->stat, w->vol,
                     w->vol_def, w->no, w->blk);
            y2 += 0x10;
        }
        y2 += 0x10;
        for (i = 0; i < 4; i++) {
            SndPlayWork* w = &pSnd->str_state[i];
            eprintf2(0xA, 0x10, 0x78, y2, 0, 0, "%2d %2d %3d %3d %3d %3d", w->used, w->stat, w->vol,
                     w->vol_def, w->no, w->blk);
            y2 += 0x10;
        }
    }
    if (read == 0) {
        eprintf(0x18, y, 0, 0, "Read WAIT");
    }
    if (stop != 0 && read != 0) {
        ret = 1;
    }
    return ret;
}

// Debug (Debug_flg): the SE call history, output / reverb mode, ARAM use per block and the voice list.
void debugDisp()
{
    static const char* mode_tbl[3] = { "  MONO", "STEREO", "  DPL2" };
    static const char* rev_tbl[8] = { "   OFF", "    HI", "   STD", "CHORUS", " DELAY", "  DPL2", "  STOP", " ERROR" };
    static const char* se_blk_tbl[14] = { "CORE", "PL  ", "WEP ", "BGM0", "BGM1", "FOOT", "ROOM", "DOOR",
                                          "EM1 ", "EM2 ", "EM3 ", "EM4 ", 0, 0 };
    int i;
    s8 idx = History.disp_idx;
    u16 y2;
    u32 total;
    int d;

    if (History.num != 0) {
        eprintf2(7, 0xD, 0x20, 8, 4, 9, "BLK   NO VOL PAN SVOL SPAN");
    }
    for (i = 0; i < History.num; i++) {
        int col = 0;
        if (i == History.num - 1) {
            col = 6;
        }
        eprintf2(7, 0xD, 0x20, i * 0x10 + 0x20, col, 9, "%s %3d %3d %3d %4d %4d", se_blk_tbl[History.blk[idx]],
                 History.no[idx], History.vol[idx], History.pan[idx], History.svol[idx], History.span[idx]);
        idx++;
        idx = LOOP_IDX(idx, 24);
    }

    y2 = 0x72;
    total = 0;
    eprintf2(7, 0xE, 0x20, 0x10, 6, 0xA, "BLK     NAME      ADDR   SIZE   USED   FREE");
    eprintf2(7, 0xE, 0x20, 0x1E, 0, 0xA, "     OS RESERVE  %06x %06x", 0, 0x4000);
    eprintf2(7, 0xE, 0x20, 0x2C, 0, 0xA, "     ZERO BUFFER %06x %06x", 0x4000, 0x100);
    eprintf2(7, 0xE, 0x20, 0x3A, 0, 0xA, " %02d  CORE        %06x %06x %06x", 0, SndMem.blk_aram[0], 0x40000,
             UseAramSize[0]);
    d = 0x40000 - UseAramSize[0];
    eprintf2(7, 0xE, 0x12A, 0x3A, d < 0 ? 2 : 0, 0xA, "%06x", __builtin_abs(d));
    eprintf2(7, 0xE, 0x20, 0x48, 0, 0xA, " %02d  PLAYER      %06x %06x %06x", 1, SndMem.blk_aram[1], 0x130000,
             UseAramSize[1]);
    d = 0x130000 - UseAramSize[1];
    eprintf2(7, 0xE, 0x12A, 0x48, d < 0 ? 2 : 0, 0xA, "%06x", __builtin_abs(d));
    eprintf2(7, 0xE, 0x20, 0x56, 0, 0xA, " %02d  WEP         %06x %06x %06x", 2, SndMem.blk_aram[2], 0x40000,
             UseAramSize[2]);
    d = 0x40000 - UseAramSize[2];
    eprintf2(7, 0xE, 0x12A, 0x56, d < 0 ? 2 : 0, 0xA, "%06x", __builtin_abs(d));
    eprintf2(7, 0xE, 0x20, 0x64, 0, 0xA, " %02d  DOOR        %06x %06x %06x %06x", 7, SndMem.blk_aram[7], 0x40000,
             UseAramSize[7], 0x40000 - UseAramSize[7]);
    if (SND_BIT_CK(pSnd->BlkFlag(), 6)) {
        y2 += 0xE;
        eprintf2(7, 0xE, 0x20, y2, 0, 0xA, " %02d    ROOM", 6);
        eprintf2(7, 0xE, 0x97, y2, 0, 0xA, "%06x %06x", SndMem.blk_aram[6], UseAramSize[6]);
    }
    if (SND_BIT_CK(pSnd->BlkFlag(), 5)) {
        y2 += 0xE;
        eprintf2(7, 0xE, 0x20, y2, 0, 0xA, " %02d    FOOT", 5);
        eprintf2(7, 0xE, 0x97, y2, 0, 0xA, "%06x %06x", SndMem.blk_aram[5], UseAramSize[5]);
    }
    for (i = 0; i < 6; i++) {
        if (pSnd->EmId(i) == 0xFF) {
            continue;
        }
        y2 += 0xE;
        if (pSnd->EmId(i) > 0xF) {
            eprintf2(7, 0xE, 0x20, y2, 0, 0xA, " %02d    EM%02X", i + 8, pSnd->EmId(i));
        } else if (pSnd->EmId(i) != 0xF) {
            eprintf2(7, 0xE, 0x20, y2, 0, 0xA, " %02d    PL%02X", i + 8, pSnd->EmId(i) + 0xE);
        } else {
            eprintf2(7, 0xE, 0x20, y2, 0, 0xA, " %02d    PL%02X", i + 8, pSnd->EmId(i));
        }
        eprintf2(7, 0xE, 0x97, y2, 0, 0xA, "%06x %06x", SndMem.blk_aram[i + 8], UseAramSize[i + 8]);
        total += UseAramSize[i + 8];
    }
    for (i = 1; i >= 0; i--) {
        if (pSnd->BgmId(i) == 0xFF) {
            continue;
        }
        y2 += 0xE;
        eprintf2(7, 0xE, 0x20, y2, 0, 0xA, " %02d    MI%03X", i + 3, pSnd->BgmId(i));
        eprintf2(7, 0xE, 0x97, y2, 0, 0xA, "%06x %06x", SndMem.blk_aram[i + 3], UseAramSize[i + 3]);
        total += UseAramSize[i + 3];
    }
    total += UseAramSize[6];
    total += UseAramSize[5];
    eprintf2(7, 0xE, 0x20, 0x72, 0, 0xA, "     ROOM FREE   %06x %06x %06x", 0x1F4100, 0x50BF00, total);
    d = 0x50BF00 - total;
    eprintf2(7, 0xE, 0x12A, 0x72, d < 0 ? 2 : 0, 0xA, "%06x", __builtin_abs(d));
    y2 += 0xE;
    eprintf2(7, 0xE, 0x20, y2, 0, 0xA, "     STREAM      %06x %06x", 0x700000, 0x100000);
    y2 += 0xE;
    eprintf2(7, 0xE, 0x20, y2, 0, 0xA, "     SUB SCREEN  %06x %06x", 0xD00000, 0x300000);
    y2 += 0x1C;
    eprintf2(7, 0xE, 0x20, y2, 0, 0xA, "FREE BASE   %06x", ARAM_FREE_BASE);
    y2 += 0xE;
    eprintf2(7, 0xE, 0x20, y2, 0, 0xA, "FREE LAST   %06x", 0xD00000);
    y2 += 0xE;
    eprintf2(7, 0xE, 0x20, y2, 0, 0xA, "FREE SIZE   %06x", 0xD00000 - ARAM_FREE_BASE);

    eprintf2(7, 0xE, 0x1B0, 0x13B, 0, 9, "TOTAL %2d", Snd_ctrl_work.total_num);
    eprintf2(7, 0xE, 0x1B0, 0x149, 0, 9, "SE    %2d", Snd_ctrl_work.axv_num);
    eprintf2(7, 0xE, 0x1B0, 0x157, 0, 9, "STR   %2d", Snd_ctrl_work.str_num);
    eprintf2(7, 0xE, 0x1B0, 0x165, 0, 9, "SEQ   %2d", Snd_ctrl_work.seq_num);
    for (i = 0; i < 64; i++) {
        if (Snd_voice_work[i].status != 0) {
            eprintf2(8, 0xB, (i / 8) * 20 + 0xFA, (i % 8) * 14 + 0x13B, 4, 9, "%02d ", i);
        } else {
            eprintf2(8, 0xB, (i / 8) * 20 + 0xFA, (i % 8) * 14 + 0x13B, 7, 9, "%02d ", i);
        }
    }

    if (0) {
        eprintf2(7, 0xE, 0x20, 0x10, 6, 0xA, "SOUND MODE");
        eprintf2(7, 0xE, 0x20, 0x1E, 0, 0xA, "%s", mode_tbl[pSys->SndMode]);
        eprintf2(7, 0xE, 0x20, 0x2C, 6, 0xA, "REVERB TYPE");
        eprintf2(7, 0xE, 0x20, 0x3A, 0, 0xA, "%s", rev_tbl[Snd_efx_work[0].type]);
        eprintf2(7, 0xE, 0x20, 0x48, 6, 0xA, "REVERB SETTINGS");
        eprintf2(7, 0xE, 0x20, 0x56, 0, 0xA, "DELAY        %2.2f", Snd_efx_work[0].fx.hi.preDelay);
        eprintf2(7, 0xE, 0x20, 0x64, 0, 0xA, "TIME         %2.2f", Snd_efx_work[0].fx.hi.time);
        eprintf2(7, 0xE, 0x20, 0x72, 0, 0xA, "COLORATION   %2.2f", Snd_efx_work[0].fx.hi.coloration);
        eprintf2(7, 0xE, 0x20, 0x80, 0, 0xA, "DAMPING      %2.2f", Snd_efx_work[0].fx.hi.damping);
        eprintf2(7, 0xE, 0x20, 0x8E, 0, 0xA, "CROSSTALK    %2.2f", Snd_efx_work[0].fx.hi.crosstalk);
        eprintf2(7, 0xE, 0x20, 0x9C, 0, 0xA, "MIX          %2.2f", Snd_efx_work[0].fx.hi.mix);
        eprintf2(7, 0xE, 0x20, 0xAA, 6, 0xA, "DEFAULT AUX A");
        eprintf2(7, 0xE, 0x20, 0xB8, 0, 0xA, "CORE           %3d", pSnd->Hdr()->efx[0].Aux_core);
        eprintf2(7, 0xE, 0x20, 0xC6, 0, 0xA, "WEAPON         %3d", pSnd->Hdr()->efx[0].Aux_weapon);
        eprintf2(7, 0xE, 0x20, 0xD4, 0, 0xA, "ENEMY          %3d", pSnd->Hdr()->efx[0].Aux_enemy);
        eprintf2(7, 0xE, 0x20, 0xE2, 0, 0xA, "ROOM           %3d", pSnd->Hdr()->efx[0].Aux_room);
    }
}

// Fades all SEs out over `sec` seconds.
void SndSeAbsFadeOutAll_sec(int sec)
{
    Snd_se_fade_out_all2(sec * 200);
}

// Fades all SEs out over `time` x 5 ms.
void SndSeAbsFadeOutAll_5msec(s16 time)
{
    Snd_se_fade_out_all2(time);
}

// Fades all sequences of `type` out over `sec` seconds.
void SndSeqFadeOutAll_sec(u8 type, int time)
{
    Snd_seq_fade_out_type(type, time * 200);
}

// The split object's .sdata is 8-aligned (0x18 bytes: the u8 flag_bak is followed by 7 bytes of pad).
asm(".section .sdata; .balign 8");
