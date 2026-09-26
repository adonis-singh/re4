// game/est: the effect set ("EST") front end (D:/Bio4/Prog/est.cpp). EstSet(owner, id) is how
// game code starts an effect: it looks the est table up in the loaded effect data
// (EspGetEstAddr) and starts a controller 10 sequence player on it. Also the room "SST" effects
// (per-room effect lists started by area / room key), the effect deletion front ends
// (EffectEspDelete / EffectDeleteAll / EventCutEffDelete ...), the eat (hit) effects
// (EspSetEatEffect) and a few water helpers. EspEvModList maps event model numbers to models.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "est.h"
#include "math_sub.h"
#include "rnd.h"
#include "player.h"
#include "area.h"
#include "flr_at.h"
#include "at_sub2.h"
#include "snd.h"
#include "db_log.h"

cEspEventModelList EspEvModList;

// The common entry: starts est table (owner c, id d) with parts b (-1 = the table's default) on the
// model a (0 = none), at pos/rot (NULL = the table's own), core flags e, kind f, Core_pEm g and an
// optional EspSeqOpt h.
void EstSet(cModel* a, int b, Vec* pos, Vec* rot, int c, u8 d, u16 e, u8 f, void* g, void* h)
{
    EspSeqData* head = EspGetEstAddr(c, d, 0);

    EstSet(a, b, pos, rot, head, e, f, g, c, h);
}

// Starts the sequence `head` on a front-pulled controller 10: stamps the owner info (Core_flg e, plus
// 0x2000 during a movie / bit 0 in the no-suspend mode from Status_flg[2]), the call number, parts,
// offset (pos != NULL sets Flg bit 1 = explicit position) and rotation (head->rot is in degrees),
// and a random seed. Debug_flg[1] 0x01000000 disables all effects.
void EstSet(cModel* model, int no, Vec* pos, Vec* rot, EspSeqData* head, u16 e, u8 f, void* g, u32 owner, void* h)
{
    EspgenWork* w;
    Espgen10Work* p;

    if (DbgFlagChk(pG, DBG_NO_EST_CALL)) {
        return;
    }
    if (head == NULL) {
        return;
    }
    if (head->num == 0) {
        pLog->warn(0, 0, "EstSet():EST is enpty.");
        return;
    }
    if (StaFlagChk(pG, STA_EVENT_SYSYTEM)) {
        e |= 0x2000;
    }
    if (StaFlagChk(pG, STA_ESP_COMPULSION_NOSUSPEND)) {
        e |= 1;
    }
    if (!PullEspEspgen(&w, e, f, (u8) EspgenGetCallNo(), g, owner, 1)) {
        return;
    }
    EspgenIncCallNo();
    w->id = 0x10;
    p = (Espgen10Work*) w->work;
    p->head = head;
    p->pMod = model;
    if (model != NULL) {
        p->Guid_pMod = model->guid;
    } else {
        p->Guid_pMod = (u32) model;
    }
    p->Seq_ptr = p->Time_cnt = 0;
    if (no == -1) {
        p->Null_parts_no = head->parts;
    } else {
        p->Null_parts_no = no;
    }
    if (pos == NULL) {
        p->Offset = head->pos;
    } else {
        p->Flg |= 2;
        p->Offset = *pos;
    }
    if (rot == NULL) {
        p->Ang = head->rot;
        PSVECScale(&p->Ang, &p->Ang, 3.14f / 180.0f);
    } else {
        p->Ang = *rot;
    }
    p->Rand_seed = Rnd() | (Rnd() << 8) | (Rnd() << 16);
    if (h != NULL) {
        p->p8 = &p->opt;
        p->opt = *(EspSeqOpt*) h;
    } else {
        p->p8 = (EspSeqOpt*) h;
    }
}

// Starts the SST effects of type 1 with key 0xC + area for every effect area the player currently
// stands in (used when a display flag `id` is switched on so its area effects appear at once).
// Sets the room effects whose area the player stands in.
void AreaSstSet(int id)
{
    cEspSystem* sys = g_pEspSys;
    SstAreaEnt* ent;
    Vec pos;
    u32 flag;
    u32 i;
    u32 j;

    if (sys->Area_addr == NULL) {
        return;
    }
    pos = pPL->pos;
    pos.y += 100.0f;
    flag = 0;
    ent = sys->Area_addr->ent;
    for (i = 0; i < sys->Area_addr->num; i++, ent++) {
        if (AreaHitCheck(ent->area, &pos) == 1) {
            flag |= 1 << ent->area_no;
        }
    }
    for (j = 0; j < 32; j++) {
        if (flag & (1 << j)) {
            SstSet(EFF_ROOM, (u16) j, (ESP_CORE_KIND) (ESP_CORE_KIND_ROOM_AREA00 + j), id, id, 0);
        }
    }
}

// 1 when room effect display flag `id` (0..31, cEspSystem::SstSetFlag) is on.
int GetSstDispFlag(u32 id)
{
    cEspSystem* sys = g_pEspSys;

    if (id > 0x1F) {
        pLog->err(0, 0, "GetSstDispFlag() : id[%02x] invalid .", id);
        return 0;
    }
    if (sys->SstSetFlag & (1 << id)) {
        return 1;
    }
    return 0;
}

// Turns room effect display flag `id` on/off; turning it on from off also starts the matching area
// effects for the player's current areas (AreaSstSet).
void SetSstDispFlag(u32 id, int flg)
{
    cEspSystem* sys = g_pEspSys;

    if (id > 0x1F) {
        pLog->err(0, 0, "GetSstDispFlag() : id[%02x] invalid .", id);
        return;
    }
    if (flg == 1) {
        if (GetSstDispFlag(id) == 0) {
            sys->SstSetFlag |= flg << id;
            AreaSstSet(id);
        } else {
            sys->SstSetFlag |= flg << id;
        }
    } else {
        sys->SstSetFlag &= ~(1 << id);
    }
}

// Sets the extra effect-area bits that EffAreaUpdate ORs into the area state every frame.
void SetSstAddAreaFlag(u32 flg)
{
    g_pEspSys->Add_area_bit = flg;
}

// Starts every SST entry of `owner` (0xD2 = none) whose key is in [lo, hi], whose type matches and
// whose display flag is on, as a permanent effect (Core_flg 0x4001, kind no, owner 0xD0). move != 0
// pre-runs the generators 200 frames so steady-state effects (smoke, dust) are already full.
// Starts every effect of owner `owner` whose room key lies in [lo, hi] and whose type is `type`.
void SstSet(u32 owner, int blk_no, ESP_CORE_KIND kind, int start_id, int end_id, int bTimeLoop)
{
    cEspSystem* sys = g_pEspSys;
    SstTbl* tbl;
    SstList* list;
    u32* ofs;
    u32 i;

    if (owner > EFF_NONE) {
        pLog->err(0, 0, "GetSstAddr():Invalid OWNER_ID[%x].", owner);
        return;
    }
    tbl = &sys->sstTbl[owner];
    if (tbl->owner == EFF_NONE) {
        return;
    }
    list = tbl->list;
    for (i = 0; i < list->num; i++) {
        if (list->ent[i].no < (u16) start_id || list->ent[i].no > (u16) end_id) {
            continue;
        }
        if (list->ent[i].type != blk_no) {
            continue;
        }
        if (!GetSstDispFlag(list->ent[i].b.id)) {
            continue;
        }
        ofs = tbl->data->ofs;
        ofs += i;
        EstSet(NULL, -1, NULL, NULL, (EspSeqData*) ((u8*) tbl->data + *ofs), 0x4001, (u8) kind, 0, EFF_SST, NULL);
    }
    if (bTimeLoop) {
        EspGenSetMoveLoop(200);
        EspGenLoopMove();
    }
}

// Deletes sprites by owner info (Core_flg a, kind b, Core_pEm c, attached model).
void EffectEspDelete(int a, int b, void* c, cModel* pMod)
{
    EspDelete(a, b, c, pMod);
}

// Deletes controllers by owner info.
void EffectEspgenDelete(int a, int b, void* c)
{
    EspgenDelete(a, b, c);
}

// Deletes effect models by owner info.
void EffectEfmDelete(int a, int b, void* c)
{
    EfmDelete(a, b, c);
}

// Removes every sprite, controller and effect model (room change).
void EffectDeleteAll()
{
    StaFlagOff(pG, STA_ESPGEN45_SET);
    EspArrayClear();
    EspgenArrayClear();
    EfmArrayClear();
}

// Removes every non-permanent, non-event effect (event end).
void EffectEventDelete()
{
    EspDeleteEvent();
    EspgenDeleteEvent();
    EfmDeleteEvent();
}

// Releases every live sprite whose owner info matches (a/b/c each skipped when 0) and, when a model is
// given, that is attached to that model instance (pointer and serial).
void EspDelete(int a, int b, void* c, cModel* pMod)
{
    cEspSystem* sys = g_pEspSys;
    u32 i;

    for (i = 0; i < sys->nEsp; i++) {
        cEsp* esp = (cEsp*) (sys->EspArray + i * 0x150);

        if ((esp->m_Be_flg & 1) == 0) {
            continue;
        }
        if (a != 0 && esp->info.Core_flg != a) {
            continue;
        }
        if (b != 0 && esp->info.Core_kind != b) {
            continue;
        }
        if (c != 0 && esp->info.Core_pEm != c) {
            continue;
        }
        if (pMod != NULL) {
            if (esp->m_pMod != pMod) {
                continue;
            }
            if (esp->m_Guid_pMod != pMod->guid) {
                continue;
            }
        }
        PushEsp(esp);
    }
}

// Releases every live sprite that is neither permanent (Core_flg bit 0) nor event-owned (bit 0x800).
void EspDeleteEvent()
{
    cEspSystem* sys = g_pEspSys;
    u32 i;

    for (i = 0; i < sys->nEsp; i++) {
        cEsp* esp = (cEsp*) (sys->EspArray + i * 0x150);

        if (esp->m_Be_flg & 1) {
            int ev = !(esp->info.Core_flg & 1);

            if (ev && !(esp->info.Core_flg & 0x800)) {
                PushEsp(esp);
            }
        }
    }
}

// Water explosion splash: est 1/0x2F in the lake rooms (r10a/b, r11a/b), else the generic 0/0x15.
void EspSetWaterBomb(Vec* pos)
{
    if (pG->room_id == 0x10A || pG->room_id == 0x10B || pG->room_id == 0x11A || pG->room_id == 0x11B) {
        EstSet(0, -1, pos, NULL, EFF_ROOM, 0x2F, 0, ESP_CORE_KIND_NONE, 0, NULL);
    } else {
        EstSet(0, -1, pos, NULL, EFF_CORE, 0x15, 0, ESP_CORE_KIND_NONE, 0, NULL);
    }
}

// Bullet-hits-water splash: est 1/0x20 in the lake rooms, else 0/0x14; none in stage 3-11 / 2-24.
void EspSetWaterHitmark(Vec* pos)
{
    if (pG->stage_no == 3 && pG->room_no == 0x11 || pG->stage_no == 2 && pG->room_no == 0x24) {
        return;
    }
    if (pG->room_id == 0x10A || pG->room_id == 0x10B || pG->room_id == 0x11A || pG->room_id == 0x11B) {
        EstSet(0, -1, pos, NULL, EFF_ROOM, 0x20, 0, ESP_CORE_KIND_NONE, 0, NULL);
    } else {
        EstSet(0, -1, pos, NULL, EFF_CORE, 0x14, 0, ESP_CORE_KIND_NONE, 0, NULL);
    }
}

// Never called: the eat effect messages by type.
static inline void EspEatEffectMessage(int type)
{
    switch (type) {
    case 0:
        pLog->err(0, 0, "ESP: EAT no set(type=0)");
        break;
    case 1:
        pLog->err(0, 0, "ESP: EAT no set(type=1)");
        break;
    case 2:
        pLog->err(0, 0, "ESP: EAT no set(type=2)");
        break;
    case 3:
        pLog->err(0, 0, "ESP: EAT no set(type=3)");
        break;
    }
}

// 1 when the hit point lies on a near-horizontal floor whose FlrAt entry is marked as a puddle (se.eff_type 1).
int EspChkInPuddle(Vec* pos, Vec* nor)
{
    if (nor->y > 0.9f) {
        FlrAt* at = FlrAtCheck(0, pos, 1);

        if (at != NULL && at->se.eff_type == 1) {
            return 1;
        }
    }
    return 0;
}

// Spawns the hit effect for an eat (environment collision) attribute: `type` is the EAT type (0 dirt
// / puddle, 1 spark pair, 2 and 4..7 per-weapon effects from the AtEffInfo table, 3 unused), `nrm`
// the surface normal (rotation for the decal, flipped for flag-bit-0 infos), `wep` the weapon id.
// Hit effect for the eat (effect collision) attribute type.
void EspSetEatEffect(Vec* pos, Vec* nor, int type, u8 wepNo)
{
    AtEffInfo* info = EatMgr.getEffInfo(type);
    Vec rot;
    u32 eff1;
    u32 eff2;
    f32 len;

    if (info != NULL && (info->flag & 1)) {
        rot.x = atan2f(SQRTF(nor->x * nor->x + nor->z * nor->z), nor->y);
        rot.y = atan2f(nor->x, nor->z);
        rot.z = 0.0f;
    } else {
        len = SQRTF(nor->x * nor->x + nor->z * nor->z);
        rot.x = -atan2f(nor->y, len);
        rot.y = atan2f(nor->x, nor->z);
        rot.z = 0.0f;
    }
    switch (type) {
    case 0:
        if (EspChkInPuddle(pos, nor) == 1) {
            EstSet(0, -1, pos, NULL, EFF_CORE, 0x11, 0, ESP_CORE_KIND_NONE, (void*) type, (void*) type);
            SndCall(2, 0xC, pos, 0, 0, NULL);
        } else {
            EstSet(0, -1, pos, &rot, EFF_CORE, 0x1F, 0, ESP_CORE_KIND_NONE, (void*) type, (void*) type);
            if (DbgFlagChk(pG, DBG_SET_HITMARK_ALL)) {
                EstSet(0, -1, pos, &rot, EFF_CORE, 0x87, 0, ESP_CORE_KIND_NONE, (void*) type, (void*) type);
            }
        }
        break;
    case 1:
        EstSet(0, -1, pos, &rot, EFF_CORE, 0x1F, 0, ESP_CORE_KIND_NONE, 0, NULL);
        EstSet(0, -1, pos, &rot, EFF_CORE, 0x20, 0, ESP_CORE_KIND_NONE, 0, NULL);
        break;
    case 2:
        if (info == NULL || eff1 == EFF_NONE) {
            pLog->err(0, 0, "NOT REGIST EAT EFF INFO %d", type);
            break;
        }
        info->getWepEff(wepNo, &eff1, &eff2);
        if (eff1 != EFF_NONE && eff2 != 1) {
            EstSet(0, -1, pos, &rot, eff1, (u8) eff2, 0, ESP_CORE_KIND_NONE, 0, NULL);
        }
        SndCall(2, 0xB, pos, 0, 0, NULL);
        break;
    case 3:
        pLog->err(0, 0, "ESP: EAT no set(type=EAT_ET_PAD)");
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        if (info == NULL || eff1 == EFF_NONE) {
            pLog->err(0, 0, "NOT REGIST EAT EFF INFO %d", type);
            break;
        }
        info->getWepEff(wepNo, &eff1, &eff2);
        if (eff1 != EFF_NONE && eff2 != 1) {
            EstSet(0, -1, pos, &rot, eff1, (u8) eff2, 0, ESP_CORE_KIND_NONE, 0, NULL);
        }
        if (info != NULL && (info->flag & 1)) {
            SndCall(2, 0xB, pos, 0, 0, NULL);
        }
        break;
    }
}

// Event script: starts est `no` (decimal digits -> BCD id) of `owner` as an event-cut effect
// (Core_flg 0x1001), if the table exists.
void EventCutEstSet(int owner, u32 cut_no)
{
    u8 id = (cut_no / 10) * 16 + cut_no % 10;

    if (EspGetEstAddr(owner, id, 1) != NULL) {
        EstSet(0, -1, NULL, NULL, owner, id, 0x1001, ESP_CORE_KIND_NONE, 0, NULL);
    }
}

// Deletes the effects started by the current event cut (Core_flg 0x3001).
void EventCutEffDelete()
{
    EffectEspDelete(0x3001, ESP_CORE_KIND_NONE, 0, NULL);
    EffectEspgenDelete(0x3001, ESP_CORE_KIND_NONE, 0);
    EffectEfmDelete(0x3001, ESP_CORE_KIND_NONE, 0);
}

// Deletes the cut effects and the whole-event effects (Core_flg 0x2001).
void EventAllEffDelete()
{
    EventCutEffDelete();
    EffectEspDelete(0x2001, ESP_CORE_KIND_NONE, 0, NULL);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_NONE, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_NONE, 0);
}

// 1 when water effects are on (Status_flg[1] 0x400) and the point is not in a flagged effect area.
int ChkWaterEffectEnable(Vec* pos)
{
    if (StaFlagChk(pG, STA_ROOM_RAIN)) {
        if (EffAreaCheckInRoom(pos) == 0) {
            return 1;
        }
    }
    return 0;
}

// Ganado falling into water: est 1/0x32 when the room has it, else the generic 0x10/0x8D; the
// position pointer doubles as the owner key.
void EstSetEm10WaterFall(Vec* pMod)
{
    EspSeqData* head = EspGetEstAddr(EFF_ROOM, 0x32, 1);

    if (head != NULL) {
        EstSet((cModel*) pMod, -1, NULL, NULL, EFF_ROOM, 0x32, 0, ESP_CORE_KIND_NONE, pMod, NULL);
    } else {
        EstSet((cModel*) pMod, -1, NULL, NULL, EFF_EM10, 0x8D, 0, ESP_CORE_KIND_NONE, pMod, NULL);
    }
}

asm(".section .rodata; .balign 8");
