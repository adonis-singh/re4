// game/esp_app: application-side glue of the effect system (D:/Bio4/Prog/esp_app.cpp): the
// id -> Create/Trans function table (EffSetId), effect sound-effect dispatch (EspCallSeType, the
// per-room SE callback table pSeFunc), footstep/water splash effects for the player, the effect
// area state update (EffAreaUpdate, from the SstArea list of the room), and the laser sight /
// gatling / em2d tex-render helpers used by the weapons and enemies.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "player.h"
#include "snd.h"
#include "area.h"
#include "eprintf.h"
#include "math_sub.h"
#include "TexRender.h"
#include "esp.h"
#include "espgen.h"

// laser line: cEsp19 (game/esp19.cpp) work
struct Esp19Work {
    Vec Vec0;  // 0x00 end point of the line
    f32 max_laser_dist;     // 0x0C maximum length
};
class cEsp19 : public cEsp {
public:
    Esp19Work m_Free;  // 0xF8
};

class cEsp46;

cEsp* Esp00_Create();
cEsp* Esp01_Create();
cEsp* Esp02_Create();
cEsp* Esp03_Create();
cEsp* Esp04_Create();
cEsp* Esp05_Create();
cEsp* Esp06_Create();
cEsp* Esp07_Create();
cEsp* Esp09_Create();
cEsp* Esp0a_Create();
cEsp* Esp0b_Create();
cEsp* Esp0c_Create();
cEsp* Esp0d_Create();
cEsp* Esp0e_Create();
cEsp* Esp0f_Create();
cEsp* Esp10_Create();
cEsp* Esp11_Create();
cEsp* Esp12_Create();
cEsp* Esp13_Create();
cEsp* Esp14_Create();
cEsp* Esp15_Create();
cEsp* Esp16_Create();
cEsp* Esp17_Create();
cEsp* Esp19_Create();
cEsp* Esp1a_Create();
cEsp* Esp1b_Create();
cEsp* Esp3f_Create();
cEsp* Esp40_Create();
cEsp* Esp41_Create();
cEsp* Esp42_Create();
cEsp* Esp43_Create();
cEsp* Esp44_Create();
cEsp* Esp46_Create();
cEsp* Esp47_Create();
cEsp* Esp48_Create();
cEsp* Esp49_Create();
cEsp* Esp4a_Create();
cEsp* Esp4b_Create();
cEsp* Esp4c_Create();
cEsp* Esp4d_Create();
cEsp* Esp4f_Create();
void Esp11_Trans(cEsp* esp);
void Esp46_Trans(cEsp46* esp);
void Esp47_Trans(cEsp* esp);
void Esp4a_Trans();
void Esp4c_Trans();
void Esp4d_Trans();

extern "C" {
cEsp* Esp08_Create();
cEsp* Esp18_Create();
cEsp* Esp45_Create();
cEsp* Esp4e_Create();
void Esp01_Trans(cEsp* esp);
void Esp02_Trans(cEsp* esp);
void Esp03_Trans(cEsp* esp);
void Esp04_Trans(cEsp* esp);
void Esp08_Trans(cEsp* esp);
void Esp09_Trans(cEsp* esp);
void Esp0a_Trans(cEsp* esp);
void Esp0b_Trans(cEsp* esp);
void Esp0c_Trans(cEsp* esp);
void Esp0e_Trans(cEsp* esp);
void Esp0f_Trans(cEsp* esp);
void Esp12_Trans(cEsp* esp);
void Esp16_Trans(cEsp* esp);
void Esp18_Trans(cEsp* esp);
void Esp19_Trans(cEsp* esp);
void Esp45_Trans(cEsp* esp);
void Esp4e_Trans();
}

typedef void (*EffSeFunc)(Vec* pos);

EffSeFunc pSeFunc[8];
// The mask read after the `repType = 1` store goes through a struct-member view of the pointer
// (the pLog trick, db_log.h) so it stays below the store; the other reads use the plain global.
TexRenderMng* g_pMgr;
#define pMgr g_pMgr

// Fills the effect function table: for every effect id 0x00..0x52 registers its Create function and
// its Trans (draw) function (EspCommonTrans for plain sprites). Called once from the effect system
// init (eff_sys.cpp).
void EffSetId()
{
    EspFuncTblSet(0x00, Esp00_Create, EspCommonTrans);
    EspFuncTblSet(0x01, Esp01_Create, Esp01_Trans);
    EspFuncTblSet(0x02, Esp02_Create, Esp02_Trans);
    EspFuncTblSet(0x03, Esp03_Create, Esp03_Trans);
    EspFuncTblSet(0x04, Esp04_Create, Esp04_Trans);
    EspFuncTblSet(0x05, Esp05_Create, EspCommonTrans);
    EspFuncTblSet(0x06, Esp06_Create, EspCommonTrans);
    EspFuncTblSet(0x07, Esp07_Create, EspCommonTrans);
    EspFuncTblSet(0x08, Esp08_Create, Esp08_Trans);
    EspFuncTblSet(0x09, Esp09_Create, Esp09_Trans);
    EspFuncTblSet(0x0a, Esp0a_Create, Esp0a_Trans);
    EspFuncTblSet(0x0b, Esp0b_Create, Esp0b_Trans);
    EspFuncTblSet(0x0c, Esp0c_Create, Esp0c_Trans);
    EspFuncTblSet(0x0d, Esp0d_Create, EspCommonTrans);
    EspFuncTblSet(0x0e, Esp0e_Create, Esp0e_Trans);
    EspFuncTblSet(0x0f, Esp0f_Create, Esp0f_Trans);
    EspFuncTblSet(0x10, Esp10_Create, EspCommonTrans);
    EspFuncTblSet(0x11, Esp11_Create, Esp11_Trans);
    EspFuncTblSet(0x12, Esp12_Create, Esp12_Trans);
    EspFuncTblSet(0x13, Esp13_Create, EspCommonTrans);
    EspFuncTblSet(0x14, Esp14_Create, EspCommonTrans);
    EspFuncTblSet(0x15, Esp15_Create, EspCommonTrans);
    EspFuncTblSet(0x16, Esp16_Create, Esp16_Trans);
    EspFuncTblSet(0x17, Esp17_Create, EspCommonTrans);
    EspFuncTblSet(0x18, Esp18_Create, Esp18_Trans);
    EspFuncTblSet(0x19, Esp19_Create, Esp19_Trans);
    EspFuncTblSet(0x1a, Esp1a_Create, EspCommonTrans);
    EspFuncTblSet(0x1b, Esp1b_Create, EspCommonTrans);
    EspFuncTblSet(0x3f, Esp3f_Create, NULL);
    EspFuncTblSet(0x40, Esp40_Create, EspCommonTrans);
    EspFuncTblSet(0x41, Esp41_Create, EspCommonTrans);
    EspFuncTblSet(0x42, Esp42_Create, EspCommonTrans);
    EspFuncTblSet(0x43, Esp43_Create, EspCommonTrans);
    EspFuncTblSet(0x44, Esp44_Create, EspCommonTrans);
    EspFuncTblSet(0x45, Esp45_Create, Esp45_Trans);
    EspFuncTblSet(0x46, Esp46_Create, (EspTransFunc) Esp46_Trans);
    EspFuncTblSet(0x47, Esp47_Create, Esp47_Trans);
    EspFuncTblSet(0x48, Esp48_Create, EspCommonTrans);
    EspFuncTblSet(0x49, Esp49_Create, EspCommonTrans);
    EspFuncTblSet(0x4a, Esp4a_Create, (EspTransFunc) Esp4a_Trans);
    EspFuncTblSet(0x4b, Esp4b_Create, EspCommonTrans);
    EspFuncTblSet(0x4c, Esp4c_Create, (EspTransFunc) Esp4c_Trans);
    EspFuncTblSet(0x4d, Esp4d_Create, (EspTransFunc) Esp4d_Trans);
    EspFuncTblSet(0x4e, Esp4e_Create, (EspTransFunc) Esp4e_Trans);
    EspFuncTblSet(0x4f, Esp4f_Create, EspCommonTrans);
}

// Debug hook of the effect init (eff_sys.cpp): empty in the retail build.
void EspFreeSizeCheckAll()
{
}

// Plays the sound effect selected by an effect record's SeType (esp07): 1 = SE 0x2F, 2 = SE 0x0D,
// 0/3 = silent. Skipped while the generator runs in loop-preview mode (EspGenGetMoveLoop).
void EspCallSeType(int type, Vec* pPos)
{
    if (EspGenGetMoveLoop()) {
        return;
    }
    switch (type) {
    case 0:
        break;
    case 1:
        SndCall(6, 0x2F, pPos, 0, 0, NULL);
        break;
    case 2:
        SndCall(6, 0x0D, pPos, 0, 0, NULL);
        break;
    case 3:
        break;
    default:
        pLog->err(0, 0, "EspCallSeType() : SeType[%d] invalid.", type);
        break;
    }
}

// Footstep effect for the player's foot `type` (0 left / 1 right): `no` is the ground material's
// FootSeNo (1/3 dust, 2 splash, 4 room-specific effect from pl->m_pEffRoom[4|5] spawned at the
// floor height under the player). Spawns through EstSet.
void EspFootCall(int type, int no, Vec* pPos)
{
    cPlayer* pl = pPL;
    Vec fpos;
    f32 h;

    switch (type) {
    case 0:
        switch (no) {
        case 0:
            break;
        case 1:
            EstSet(0, -1, pPos, NULL, EFF_CORE, 4, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        case 2:
            EstSet(0, -1, pPos, NULL, EFF_CORE, 0x16, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        case 3:
            EstSet(0, -1, pPos, NULL, EFF_CORE, 4, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        case 4:
            h = EatMgr.getFloor(&pl->pos, NULL, 600.0f, 100000.0f, 0);
            fpos.x = pl->pos.x;
            fpos.y = h;
            fpos.z = pl->pos.z;
            EstSet(0, -1, &fpos, NULL, pl->m_pEffRoom[4].id, pl->m_pEffRoom[4].type, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        default:
            pLog->err(0, 0, "EspFootCall() : FootSeNo[%d] invalid.", no);
            break;
        }
        break;
    case 1:
        switch (no) {
        case 0:
            break;
        case 1:
            EstSet(0, -1, pPos, NULL, EFF_CORE, 5, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        case 2:
            EstSet(0, -1, pPos, NULL, EFF_CORE, 0x17, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        case 3:
            EstSet(0, -1, pPos, NULL, EFF_CORE, 5, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        case 4:
            h = EatMgr.getFloor(&pl->pos, NULL, 600.0f, 100000.0f, 0);
            fpos.x = pl->pos.x;
            fpos.y = h;
            fpos.z = pl->pos.z;
            EstSet(0, -1, &fpos, NULL, pl->m_pEffRoom[5].id, pl->m_pEffRoom[5].type, 0, ESP_CORE_KIND_NONE, 0, NULL);
            break;
        default:
            pLog->err(0, 0, "EspFootCall() : FootSeNo[%d] invalid.", no);
            break;
        }
        break;
    default:
        pLog->err(0, 0, "EspFootCall() : FootSeType[%d] invalid.", type);
        break;
    }
}

// Called from the sound system for the player's water footsteps: when the point is more than 90 units
// below the water surface, pushes the water at the player position (AddWaterPower 0.25) and returns
// 1; otherwise 0.
int EspPlWaterCall(int type, Vec* pPos)
{
    Vec wpos;
    f32 h;
    int ret = 0;

    if (GetWaterHeight(pPos, &h)) {
        if (pPos->y < h - 90.0f) {
            AddWaterPower(pPL->pos, 0.25f);
            ret = 1;
            wpos = *pPos;
            wpos.y = h;
        }
    }
    return ret;
}

// Registers a room's effect sound callback in slot 0..7 of pSeFunc (called by room code).
void EffSetRoomSeFunc(int no, EffSeFunc func)
{
    if (no < 0 || no > 7) {
        pLog->err(0, 0, "EffSetRoomSeFunc() : SeNo[%d] invalid.", no);
        return;
    }
    pSeFunc[no] = func;
}

// Clears the 8 room effect sound callbacks (room change).
void EffCrearRoomSeFunc()
{
    EffSeFunc* p = pSeFunc;
    int i;

    for (i = 0; i < 8; i++) {
        *p++ = NULL;
    }
}

// Calls room effect sound callback `no` with the effect position; logs when the slot is out of range
// or not registered.
void EffCallRoomSeFunc(int no, Vec* pPos)
{
    if (no < 0 || no > 7) {
        pLog->err(0, 0, "EffCallRoomSeFunc() : SeNo[%d] invalid.", no);
        return;
    }
    if (pSeFunc[no] == NULL) {
        pLog->err(0, 0, "EffCallRoomSeFunc() : SeFunc[%d] not init.", no);
        return;
    }
    pSeFunc[no](pPos);
}

// Per-frame update of the effect area states: tests the player position (+100 y; the camera position
// when Status_flg[2] bit 0x10000) against every SstAreaEnt of the room, ORs in cEspSystem::Add_area_bit
// and turns each of the 32 area states on/off (EffSetAreaState). An area with flag bit 0 sets
// Status_flg[1] bit 0x02000000 (player in a "special" effect area, also mirrored from bit 0x800).
// Skipped while Stop_flg bit 0x20 is set. Debug_flg[3] bit 0x8000 prints the hit area numbers.
void EffAreaUpdate()
{
    cEspSystem* sys = g_pEspSys;
    Vec pos;
    u32 flag;
    u32 i;
    u32 j;
    int y;
    SstAreaEnt* ent;

    if (StaFlagChk(pG, STA_CAMERA_SET_ROOM)) {
        StaFlagOn(pG, STA_CAMERA_IN_ROOM);
    } else {
        StaFlagOff(pG, STA_CAMERA_IN_ROOM);
    }
    if (SpfFlagChk(pG, SPF_ESP_AREA)) {
        return;
    }
    if (sys->Area_addr == NULL) {
        return;
    }
    if (StaFlagChk(pG, STA_EFFAREA_USE_CAM) == 0) {
        pos = pPL->pos;
        pos.y += 100.0f;
    } else {
        pos = pG->Camera.param.pos;
    }
    flag = 0;
    ent = sys->Area_addr->ent;
    for (i = 0; i < sys->Area_addr->num; i++, ent++) {
        if (AreaHitCheck(ent->area, &pos) == 1) {
            flag |= 1 << ent->area_no;
            if (ent->flag & 1) {
                StaFlagOn(pG, STA_CAMERA_IN_ROOM);
            }
        }
    }
    flag |= sys->Add_area_bit;
    asm("" : "=m"(*(u32*) &pos)); // COMPILER-DIFF: candidate (sched2 issue-slot filler)
    y = 0;
    // y is the hit count; the row `0xE8 + y * 0x10` is a strength-reduced giv (its `li 0xE8` is
    // the last preheader insn). The codeless asm above is an issue-slot filler: sched2 (2 insns
    // per cycle) issues it with the `lwz sstAddAreaFlag`, so `li j` and `li 1` take cycle 2 and
    // the hoisted `lis "%d"` is issued after the `or` like the target (the original's block had
    // one more insn there). A scalar frame MEM (`*(u32*) &pos`) has no dependence on the in-struct
    // load; `pos.x` (in-struct) would delay the lwz.
    for (j = 0; j < 32; j++) {
        if (flag & (1 << j)) {
            if (DbgFlagChk(pG, DBG_EFF_NUM_DISP)) {
                eprintf(0x1D8, 0xE8 + y * 0x10, 0x16, 0, "%d", j);
                y++;
            }
            EffSetAreaState(j, 1);
        } else {
            EffSetAreaState(j, 0);
        }
    }
}

// 1 when the point is inside any effect area whose flag bit 0 is set (the flagged areas of the room).
int EffAreaCheckInRoom(Vec* pos)
{
    cEspSystem* sys = g_pEspSys;
    SstAreaEnt* ent;
    u32 i;

    ent = sys->Area_addr->ent;
    for (i = 0; i < sys->Area_addr->num; i++, ent++) {
        if (AreaHitCheck(ent->area, pos) == 1) {
            if (ent->flag & 1) {
                return 1;
            }
        }
    }
    return 0;
}

// 1 when the point is inside the effect area with number `areaNo` (esp4f gating).
int EffAreaCheckNo(Vec* pos, u8 areaNo)
{
    cEspSystem* sys = g_pEspSys;
    SstAreaEnt* ent;
    u32 i;

    ent = sys->Area_addr->ent;
    for (i = 0; i < sys->Area_addr->num; i++, ent++) {
        if (areaNo == ent->area_no) {
            if (AreaHitCheck(ent->area, pos) == 1) {
                return 1;
            }
        }
    }
    return 0;
}

// Gives the model a texture-render blend table (4 stages onto the TexRender manager's texture) so it
// shows the screen-rendered texture; on first use (Status_flg[1] bit 0x10 clear) allocates the
// manager, grows its buffer and spawns the est 0x25/0x1F render effect.
void EffEm2d_setTexRender(cModel* pMod)
{
    static u8 buf[0x80];
    u8* tbl = buf;
    int repType = 1;

    if (StaFlagChk(pG, STA_EFFEM2D_TEXRND) == 0) {
        TexRenderMng* mgr;
        TexRenderMng* mgr2;

        if (!GetTexRenderMgr(&pMgr)) {
            pLog->err(0, 0, "EffEm2d_setTexRender() : Manager alloc failed!!");
            return;
        }
        StaFlagOn(pG, STA_EFFEM2D_TEXRND);
        mgr = pMgr;
        mgr->m_W_size = 0x40;
        mgr->m_H_size = 0x40;
        pMgr->ReAllocBuf();
        tbl[0] = 4;
        tbl[1] = 0;
        tbl[4] = 0;
        mgr2 = pMgr;
        tbl[5] = mgr2->m_Tex_no;
        tbl[6] = 2;
        tbl[7] = mgr2->m_Tex_no;
        tbl[8] = 4;
        tbl[9] = mgr2->m_Tex_no;
        tbl[0xA] = 6;
        tbl[0xB] = mgr2->m_Tex_no;
        mgr2->m_Rep_type = repType;
        EstSet(0, -1, NULL, NULL, EFF_EM2D, 0x1F, pMgr->m_Core_flg | 0x801, ESP_CORE_KIND_NONE, 0, NULL);
    }
    pMod->pModelInfo->setTexBlendTbl(tbl);
    pMod->pModelInfo->setBlendRatio(0);
}

// Draws one frame of the laser sight line (est owner 0 id 3, effect 0x19) from `from` to `to`;
// `width` scales the record's max_laser_dist. In the pG Status_flg[1] bit 0 mode (night vision /
// scope) the blend is switched to additive-ish and alpha is reduced to 80%. Debug_flg[3] bit 0x40 hides it.
void EspDrawLaserLine(Vec lpos, Vec lcross, f32 rate)
{
    cEsp* esp;
    cEsp19* e;
    Esp19Work* w;

    if (DbgFlagChk(pG, DBG_NO_LASER_LINE)) {
        return;
    }
    if (!EspEstSetSelect(EFF_CORE, 3, 0, &esp, 0)) {
        return;
    }
    e = (cEsp19*) esp;
    w = &e->m_Free;
    e->m_Pos = lpos;
    w->Vec0 = lcross;
    w->max_laser_dist *= rate;
    if (StaFlagChk(pG, STA_LASERSITE_NOADD)) {
        esp->m_Blend_mode = 1;
        esp->m_Src_factor = 4;
        esp->m_Dst_factor = 5;
        esp->m_Logic_op = 0;
        esp->m_Col_a *= 0.8f;
    }
}

// Draws a laser line of the given colour in both directions (two est-3 lines so it is visible from
// either side), without suspending on effect-count limits.
void EspDrawLaserLine2(Vec* from, Vec* to, u8 r, u8 g, u8 b, u8 a)
{
    cEsp* esp;

    if (EspEstSetSelect(EFF_CORE, 3, 0, &esp, 1)) {
        cEsp19* e = (cEsp19*) esp;
        e->m_Pos = *from;
        e->m_Free.Vec0 = *to;
        esp->m_Col_r = (f32) r;
        esp->m_Col_g = (f32) g;
        esp->m_Col_b = (f32) b;
        esp->m_Col_a = (f32) a;
    }
    if (EspEstSetSelect(EFF_CORE, 3, 0, &esp, 1)) {
        cEsp19* e = (cEsp19*) esp;
        e->m_Pos = *to;
        e->m_Free.Vec0 = *from;
        esp->m_Col_r = (f32) r;
        esp->m_Col_g = (f32) g;
        esp->m_Col_b = (f32) b;
        esp->m_Col_a = (f32) a;
    }
}

// Spawns the gatling muzzle effect (est id 0x52) at `pos` travelling 3000 units/frame along `dir`;
// Status_flg[2] bit 0x02000000 marks it with Core_flg bit 0.
void EspSetGatling(Vec pos, Vec dir)
{
    cEsp* esp;

    if (!EspEstSetSelect(EFF_CORE, 0x52, 0, &esp, 0)) {
        return;
    }
    esp->m_Pos = pos;
    esp->m_Speed = dir;
#line 689 "D:/Bio4/Prog/esp_app.cpp"
    VECNormalize(&esp->m_Speed, &esp->m_Speed);
    PSVECScale(&esp->m_Speed, &esp->m_Speed, 3000.0f);
    if (StaFlagChk(pG, STA_ESP_COMPULSION_NOSUSPEND)) {
        esp->info.Core_flg |= 1;
    }
}

// Called by water rooms: puts the player into draw order type 0 while parts 3 (waist) is under the
// water surface, otherwise type 7 (drawn with the water refraction pass).
void setPlWaterOtType()
{
    Vec* wp = &pPL->getPartsPtr(3)->world;
    f32 h;

    if (GetWaterHeight(&pPL->pos, &h) && wp->y < h) {
        pPL->ot_type = 0;
    } else {
        pPL->ot_type = 7;
    }
}

// The original's .rodata is 8-aligned (0x2A0, 4 bytes of end padding after the last pool).
asm(".section .rodata; .balign 8");
