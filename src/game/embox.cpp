// game/embox.cpp: box enemy (cEmBox): breakable boxes, barrels, vases and cabinets that drop an
// item when shot, kicked (action button) or caught in a damage volume.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "embox.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "mes.h"
#include "act_btn.h"
#include "joy.h"
#include "eprintf.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "at_mod.h"
#include "em_sub.h"
#include "sce_at.h"
#include "player.h"
#include "esp_efm.h"

typedef void (*EmBoxFunc)(cEmBox*);

static EmBoxFunc EmBox_R0_move_tbl[4] = {
    emBox_R0_Init,
    emBox_R0_Move,
    0,
    0,
};

EmBoxFunc EmBox_R1_move_tbl[2] = {
    emBox_R1_Set,
    emBox_R1_Break,
};

// Creates a box enemy (id 0x43) from a model / TPL at pos / rot. type 0..7 selects the size,
// ambient tint and collision: 0 / 2 crate, 1 large crate, 3 / 5 / 7 barrels and vases with a
// solid atari cylinder the player bumps into, 4 / 6 cabinet-like. 1000 hp; tied to room etc flag
// `etcNo` (bit0 = already broken -> starts in Break, not ACTIVE). NULL on failure.
cEmBox* SetBox(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo)
{
    cEmBox* em;
    EmBoxWork* w;
    u16* flg;
    int zero;

    em = (cEmBox*) EmMgr.create(0x43);
    if (em == 0) {
        return 0;
    }
    w = EMBOX_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetBox() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    switch (em->type) {
    default:
        EtcSetAddAmb(em, ETC_AMB_BOX);
        break;
    case 4:
        EtcSetAddAmb(em, ETC_AMB_NEST);
        break;
    case 6:
    case 7:
        EtcSetAddAmb(em, ETC_AMB_TUBO);
        break;
    case 3:
    case 5:
        EtcSetAddAmb(em, ETC_AMB_BARREL);
        break;
    }
    w->Eff_id = 0xFF;
    switch (em->type) {
    case 0:
    default:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 150.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 200.0f;
        w->size.y = 350.0f;
        w->size.z = 550.0f;
        break;
    case 1:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 450.0f;
        w->size.y = 600.0f;
        w->size.z = 1000.0f;
        break;
    case 2:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 200.0f;
        w->size.y = 350.0f;
        w->size.z = 550.0f;
        break;
    case 3:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 800.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 660.0f;
        w->size.y = 1250.0f;
        w->size.z = 660.0f;
        break;
    case 4:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 0.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 200.0f;
        w->size.y = 350.0f;
        w->size.z = 550.0f;
        break;
    case 5:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 630.0f;
        w->size.y = 1250.0f;
        w->size.z = 630.0f;
        break;
    case 6:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 250.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 250.0f;
        w->size.y = 500.0f;
        w->size.z = 250.0f;
        break;
    case 7:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 1000.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 550.0f;
        w->size.y = 1450.0f;
        w->size.z = 550.0f;
        break;
    }
    w->pSat = 0;
    w->pEat = 0;
    em->type = type;
    switch (em->type) {
    default: {
        cAtariInfo* at = &em->atari;

        at->init(0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f, 0, 2, 0);
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x300;
        break;
    }
    case 3: {
        cAtariInfo* at = &em->atari;

        at->init(0.0f, 750.0f, 0.0f, 350.0f, 350.0f, 350.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
        break;
    }
    case 5:
    case 7: {
        cAtariInfo* at = &em->atari;

        at->init(0.0f, 750.0f, 0.0f, 300.0f, 300.0f, 300.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
        break;
    }
    }
    emBoxYarareInit(em);
    em->hp_max = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->Etc_no = etcNo;
    w->Be_flg = 0;
    w->Break_bin = 0;
    w->Break_tpl = 0;
    w->Item_num = 0;
    w->itemNo = -1;
    flg = GetEtcFlgPtr(etcNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        em->clearStatus(EM_STATUS_ACTIVE);
    } else {
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        em->setStatus(EM_STATUS_ACTIVE);
    }
    return em;
}

// Damage check per frame: a damage volume hit (DmgMgr types 1/4/5/7) breaks the box; a registered
// weapon hit (not knife / grenades) takes 999 / 9999 damage by weapon class (shotguns by
// distance) and breaks it with the matching style when the hp is gone, else spawns the hit
// (blood-style) est 3 of Eff_id.
void emBoxDmCk(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    u8 wep;
    int dmg;
    Vec hit;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hit)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            emBoxSetBreak(em, 0);
            return;
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    wep = em->dmg.m_Wep;
    em->dmg.m_Flag = 0;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    switch (wep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        em->dmg.m_Timer = 0;
        break;
    }
    switch (em->dmg.m_Wep) {
    case 0x14:
    case 0x15:
    case 0x2A:
    case 0x2E:
    default:
        dmg = 1000;
        break;
    case 7:
    case 8:
    case 0x21:
        if (em->l_pl > 36000000.0f) {
            dmg = 1000;
        } else {
            dmg = 9999;
        }
        break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = 1000;
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
        dmg = 9999;
        break;
    }
    LifeDownSet(em, dmg, 0);
    if (em->hp <= 0) {
        switch (em->dmg.m_Wep) {
        case 0x14:
        case 0x15:
        case 0x2A:
        case 0x2E:
        default:
            emBoxSetBreak(em, 0);
            break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0x10:
        case 0x11:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x2B:
            emBoxSetBreak(em, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (em->dmg.m_Dist > 36000000.0f) {
                emBoxSetBreak(em, 0);
            } else {
                emBoxSetBreak(em, 1);
            }
            break;
        case 5:
        case 6:
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x29:
        case 0x2C:
        case 0x2D:
            emBoxSetBreak(em, 2);
            break;
        }
    }
    if (w->Eff_id != 0xFF && em->type != 4) {
        EmDmBloodSet2(em, w->Eff_id, 3, 0, 0, 0);
    }
}

// Each break kind carries its own EstSet + fallback pair (a macro in the original: the arms are
// full copies whose tails the compiler cross-jumps).
#define EMBOX_BREAK_EFF(no, fallback)                                                       \
    EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, no, 1, ESP_CORE_KIND_NONE, em, 0);                     \
    if (w->Break_bin == 0 && w->Break_tpl == 0) {                                            \
        EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, fallback, 1, ESP_CORE_KIND_NONE, em, 0);           \
    }

// Breaks the box: hides the model, spawns the break est of Eff_id (kind 0 shot / 1 blast / 2
// melee; barrels use 5..7; a plain fallback debris est when no break model is set), places the
// break model (setBreakModel) when there is one, plays the type's break SE, disables the
// collision, drops the set item and moves to Rno1 1 Break.
void emBoxSetBreak(cEmBox* em, u32 kind)
{
    EmBoxWork* w = EMBOX_WK(em);

    em->hp = 0;
    em->be_flag &= ~2;
    if (w->Eff_id != 0xFF) {
        switch (em->type) {
        default:
            switch (kind) {
            case 0:
            default:
                EMBOX_BREAK_EFF(0, 4);
                break;
            case 1:
                EMBOX_BREAK_EFF(1, 4);
                break;
            case 2:
                EMBOX_BREAK_EFF(2, 4);
                break;
            }
            break;
        case 5:
            switch (kind) {
            case 0:
            default:
                EMBOX_BREAK_EFF(5, 8);
                break;
            case 1:
                EMBOX_BREAK_EFF(6, 8);
                break;
            case 2:
                EMBOX_BREAK_EFF(7, 8);
                break;
            }
            break;
        case 4:
            EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, 0, 1, ESP_CORE_KIND_NONE, em, 0);
            break;
        }
    }
    if (w->Break_bin && w->Break_tpl) {
        SetEffModel(w->Break_bin, w->Break_tpl, &em->pos, &em->ang);
    }
    switch (em->type) {
    default:
        SndCall(1, 0x1A, &em->pos, 0, 0, em);
        break;
    case 3:
    case 5:
        SndCall(1, 0x3C, &em->pos, 0, 0, em);
        break;
    case 4:
        SndCall(6, 0x34, &em->pos, 0, 0, em);
        break;
    case 6:
        SndCall(6, 0x5A, &em->pos, 0, 0, em);
        break;
    case 7:
        SndCall(6, 0x57, &em->pos, 0, 0, em);
        break;
    }
    emBoxSatClear(em);
    if (w->itemNo != -1) {
        emBoxSetItem(em);
    }
    em->r_no_0 = 1;
    em->r_no_1 = 1;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Per-frame: (debug pad 3 Y: ambient tweak), damage check, the Rno0 routine, the model-vs-player
// atari, then the action button check for the kick.
void cEmBox::move()
{
    if (Joy[2].on & 0x10) {
        u8 amb = (u8) tubo_amb;

        if (amb == 0) {
            be_flag &= ~8;
        } else {
            be_flag |= 8;
        }
        AddAmb_r = amb;
        AddAmb_g = amb;
        AddAmb_b = amb;
        eprintf(80, 200, 0, 0, "amb = %d", tubo_amb);
    }
    emBoxDmCk(this);
    be_flag &= ~0x4000;
    EmBox_R0_move_tbl[r_no_0](this);
    EmAtCheck(this);
    atari.move();
    emBoxActEvtCk(this);
}

// Rno0 == 0: resets to the Set state.
void emBox_R0_Init(cEmBox* em)
{
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Break).
void emBox_R0_Move(cEmBox* em)
{
    EmBox_R1_move_tbl[em->r_no_1](em);
}

// Rno1 == 0: intact box; builds the matrices once, then stays a hit-box-only work.
void emBox_R1_Set(cEmBox* em)
{
    if (em->r_no_2 == 0) {
        RotMatrix(em->mat, &em->ang);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        em->r_no_2++;
    }
    em->be_flag |= 0x4000;
}

// Rno1 == 1: broken; on entry sets bit0 of the etc flag, hp 0, hides the model, clears ACTIVE and
// lets the player walk through; then hit-box-only.
void emBox_R1_Break(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    u16* flg;

    if (em->r_no_2 == 0) {
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        em->hp = 0;
        em->be_flag &= ~2;
        em->clearStatus(EM_STATUS_ACTIVE);
        w->Lost_wait = 150;
        em->atari.throughOn();
        em->r_no_2++;
    }
    em->be_flag |= 0x4000;
}

// Deactivates the box's scenario / effect collision pieces.
void emBoxSatClear(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);

    if (w->pSat) {
        w->pSat->setDisable();
    }
    if (w->pEat) {
        w->pEat->setDisable();
    }
}

// Unused in the shipped build (nothing calls it and the linker dropped it from .text; the unit
// is in STRIP_UNUSED), but its constants (0.5f, 0.0f, 225000000.0f) are still in the unit's
// constant pool between cEmBox::move's string and emBoxYarareInit's pool. The body only has
// to reproduce that pool.
static void emBoxSatSet(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    f32 hx = w->size.x * 0.5f;

    if (hx == 0.0f) {
        return;
    }
    if (w->pSat != 0 && em->l_pl > 225000000.0f) {
        return;
    }
    emBoxSatClear(em);
}

// Hit boxes: the full-size cube plus a smaller inner cube (hit) for the precise hit.
void emBoxYarareInit(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);

    YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, YAT_FLAG_ON);
    YarareAddCube((cEmHit*) em, &w->hit, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f * 0.5f, w->size.y * 0.8f, w->size.z * 0.5f * 0.8f, 0, YAT_FLAG_ON);
}

// Sets the break / hit est id; on an already broken box (room re-entry) places the debris est 4
// (8 for type 5) or the break model at once.
void cEmBox::setEff(u8 eff_id)
{
    EmBoxWork* w = EMBOX_WK(this);

    w->Eff_id = eff_id;
    if (hp > 0) {
        return;
    }
    if (w->Break_bin == 0 && w->Break_tpl == 0 && w->Eff_id != 0xFF && type != 4) {
        if (type != 5) {
            EstSet(0, -1, &pos, &ang, w->Eff_id, 4, 1, ESP_CORE_KIND_NONE, this, 0);
        } else {
            EstSet(0, -1, &pos, &ang, w->Eff_id, 8, 1, ESP_CORE_KIND_NONE, this, 0);
        }
    }
    if (w->Break_bin && w->Break_tpl) {
        SetEffModel(w->Break_bin, w->Break_tpl, &pos, &ang);
    }
}

// The item (id, count, item flags, auto flags) dropped when the box breaks (-1 = none).
void cEmBox::setItem(int no, int num, u16 c, u16 d)
{
    EmBoxWork* w = EMBOX_WK(this);

    w->itemNo = no;
    w->Item_num = num;
    w->Item_flg = c;
    w->Auto_item_flg = d;
}

// Offers the action button 1 (kick / break) when the player faces the intact box within its
// extended bounds and no wall is between them; only in village rooms 0x100 / 0x101 / 0x103 /
// 0x106 (the tutorial hint rooms).
void emBoxActEvtCk(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    Mtx inv;
    Vec lp;
    Vec a;
    Vec b;

    if (em->hp <= 0) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->ang.y, 3.1415927f)) > 0.7853982f) {
        return;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &lp);
    if (lp.x > w->size.x + 1000.0f) {
        return;
    }
    if (lp.x < -(w->size.x + 1000.0f)) {
        return;
    }
    if (lp.y > w->size.y + 0.0f) {
        return;
    }
    if (lp.y < -(w->size.y + 2000.0f)) {
        return;
    }
    if (lp.z > w->size.z + 1000.0f) {
        return;
    }
    if (lp.z < -(w->size.z + 1000.0f)) {
        return;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 200.0f;
    b.y = a.y;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    if (pG->room_id == 0x100 || pG->room_id == 0x101 || pG->room_id == 0x103 || pG->room_id == 0x106) {
        ActBtn.set(ACT_CHECK, 5, (void*) emBoxAction, em, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
}

// 1 when another intact barrel (room etc kinds 0x11 / 0x1E) stands within 1500 units (the hint
// message then mentions several barrels).
int checkNearOtherBarrel(cEmBox* em)
{
    u32 i;
    cEm* other;

    for (i = 0; i <= 0x3F; i++) {
        if (getRoomEtc(i, ETC_WOODBOX_BARREL, &other, 0) == 1 || getRoomEtc(i, ETC_WOODBOX_BARREL2, &other, 0) == 1) {
            if (em != other && other->hp > 0 && PSVECSquareDistance(&em->pos, &other->pos) < 2250000.0f) {
                return 1;
            }
        }
    }
    return 0;
}

// Action button callback: shows the tutorial message (3 for boxes, 4 / 5 for a single / several
// barrels) telling the player to shoot them.
void emBoxAction(cEmBox* em)
{
    switch (em->type) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 6:
    case 7:
    default:
        cMes.MesSet(3, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        break;
    case 3:
    case 5:
        if (checkNearOtherBarrel(em) == 1) {
            cMes.MesSet(5, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        } else {
            cMes.MesSet(4, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->m_font_h - 1, 1, 0, 0, 4);
        }
        break;
    }
}

// Spawns the set item at the box position (SceAtCreateItemAt).
void emBoxSetItem(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);

    if (w->itemNo != -1) {
        SceAtCreateItemAt(&em->pos, w->itemNo, w->Item_num, -1, -1, 0, -1);
    }
}

// The debris model placed when the box breaks (instead of the fallback est).
void cEmBox::setBreakModel(void* bin, void* tpl)
{
    EmBoxWork* w = EMBOX_WK(this);

    w->Break_bin = bin;
    w->Break_tpl = tpl;
}
