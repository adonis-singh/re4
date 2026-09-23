// game/emtorch.cpp: torch enemy (cEmTorch): candles, braziers and lamps that follow a parent's
// parts, burn a flame effect and break or fall when shot.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emtorch.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"
#include "em_sub.h"
#include "est.h"


typedef void (*EmTorchFunc)(cEmTorch*);

static EmTorchFunc EmTorch_R0_move_tbl[4] = {
    emTorch_R0_Init,
    emTorch_R0_Move,
    0,
    0,
};

EmTorchFunc EmTorch_R1_move_tbl[4] = {
    emTorch_R1_Set,
    emTorch_R1_Parent,
    emTorch_R1_Break,
    emTorch_R1_Fall,
};

// Creates a torch enemy (id 0x47) from a model / TPL at pos / rot. type: 0 brazier (1000 hp,
// hit box only, vanishes when broken), 1 / 4 standing candle / lamp (1 hp, vanishes), 2 / 3 wall
// lamp (1 hp, no hit box, only the flame goes out), 5 hanging lamp (1 hp, falls and burns the
// floor). Tied to room etc flag `etcNo` (bit0 = already broken -> starts in Break). NULL on
// failure.
cEmTorch* SetTorch(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int etcNo)
{
    cEmTorch* em;
    EmTorchWork* w;
    u16* flg;

    em = (cEmTorch*) EmMgr.create(0x47);
    if (em == 0) {
        return 0;
    }
    w = EMTORCH_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetTorch() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    if (type != 5) {
        EtcSetAddAmb(em, ETC_AMB_TORCH);
    } else {
        EtcSetAddAmb(em, ETC_AMB_FALL_LANTERN);
    }
    w->Eff_id = 0xFF;
    switch (em->type) {
    case 0:
    default:
        w->size.x = 300.0f;
        w->size.y = 300.0f;
        w->size.z = 450.0f;
        break;
    case 4:
        w->size.x = 250.0f;
        w->size.y = 500.0f;
        w->size.z = 250.0f;
        break;
    case 2:
        w->size.x = 300.0f;
        w->size.y = 300.0f;
        w->size.z = 300.0f;
        break;
    case 3:
        w->size.x = 300.0f;
        w->size.y = 600.0f;
        w->size.z = 300.0f;
        break;
    case 1:
        w->size.x = 250.0f;
        w->size.y = 700.0f;
        w->size.z = 250.0f;
        break;
    case 5:
        w->size.x = 400.0f;
        w->size.y = 600.0f;
        w->size.z = 400.0f;
        break;
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f, 0, 2, 0);
    em->atari.setPriority(PRI_LV3);
    em->atari.throughOn();
    emTorchYarareInit(em);
    em->hp_max = em->hp;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 1000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    switch (em->type) {
    case 0:
    default:
        em->hp = 1000;
        break;
    case 1:
        em->hp = 1;
        break;
    case 2:
        em->hp = 1;
        break;
    case 3:
        em->hp = 1;
        break;
    case 4:
        em->hp = 1;
        break;
    case 5:
        em->hp = 1;
        break;
    }
    w->EffKindId = 50;
    w->Be_flg = 0;
    w->Etc_no = etcNo;
    flg = GetEtcFlgPtr(etcNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    } else {
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
    return em;
}

// Weapon hit reaction: consumes the registered hit (ignoring knife / grenades), takes 999 or 9999
// damage by weapon class (shotguns by distance), spawns the hit est (parameter 1) or plays the
// hit SE, and when the hp is gone breaks the torch with the style decided by the weapon.
void emTorchDmCk(cEmTorch* pEm)
{
    EmTorchWork* w = EMTORCH_WK(pEm);
    u8 wep;
    int dmg;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    wep = pEm->dmg.m_Wep;
    pEm->dmg.m_Flag = 0;
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
        pEm->dmg.m_Timer = 0;
        break;
    }
    switch (pEm->dmg.m_Wep) {
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
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = 999;
        break;
    case 7:
    case 8:
    case 0x21:
        if (pEm->l_pl > 36000000.0f) {
            dmg = 999;
        } else {
            dmg = 9999;
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
    case 0x2A:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    }
    LifeDownSet(pEm, dmg, 0);
    if (pEm->type == 5) {
        EstSet(0, -1, &pEm->pos, &pEm->ang, w->Eff_id, 1, 0, ESP_CORE_KIND_NONE, 0, 0);
    }
    if (pEm->hp <= 0) {
        switch (pEm->dmg.m_Wep) {
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
        case 0x14:
        case 0x15:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x2B:
            emTorchSetBreak(pEm, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (pEm->dmg.m_Dist > 36000000.0f) {
                emTorchSetBreak(pEm, 0);
            } else {
                emTorchSetBreak(pEm, 1);
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
        case 0x2A:
        case 0x2C:
        case 0x2D:
        default:
            emTorchSetBreak(pEm, 2);
            break;
        }
    } else {
        if (pEm->type == 0) {
            SndCall(1, 0x3F, &pEm->pos, 0, 0, pEm);
        }
        if (w->Eff_id != 0xFF) {
            EstSet(pEm, -1, 0, 0, w->Eff_id, 1, 0, ESP_CORE_KIND_NONE, pEm, 0);
        }
    }
}

// Puts the torch out: deletes its flame effects (Core_kind EffKindId) and spawns the break est
// (parameter 2, or 3 for kind 2 = melee), then by type hides the model and plays the break SE
// (0, 1, 4 -> Rno1 2 Break), only plays the SE (2, 3), or starts the fall (5 -> Rno1 3).
void emTorchSetBreak(cEmTorch* em, u32 kind)
{
    EmTorchWork* w = EMTORCH_WK(em);

    em->hp = 0;
    if (w->Eff_id != 0xFF && em->type != 5) {
        EffectEspDelete(1, w->EffKindId, em, 0);
        EffectEspgenDelete(1, w->EffKindId, em);
        EffectEfmDelete(1, w->EffKindId, em);
        switch (kind) {
        default:
            EstSet(em, -1, 0, 0, w->Eff_id, 2, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 0:
            EstSet(em, -1, 0, 0, w->Eff_id, 2, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 1:
            EstSet(em, -1, 0, 0, w->Eff_id, 2, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        case 2:
            EstSet(em, -1, 0, 0, w->Eff_id, 3, 0, ESP_CORE_KIND_NONE, em, 0);
            break;
        }
    }
    switch (em->type) {
    case 0:
        em->be_flag &= ~2;
        SndCall(1, 0x40, &em->pos, 0, 0, em);
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        break;
    case 2:
    case 3:
        SndCall(6, 0x12, &em->pos, 0, 0, em);
        break;
    case 1:
    case 4:
        em->be_flag &= ~2;
        SndCall(6, 0x2A, &em->pos, 0, 0, em);
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        break;
    case 5:
        em->r_no_0 = 1;
        em->r_no_1 = 3;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
        break;
    }
}

// Per-frame: weapon hit check, clear the hit-box-only flag, run the Rno0 routine.
void cEmTorch::move()
{
    emTorchDmCk(this);
    be_flag &= ~0x4000;
    EmTorch_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: resets to the Set state.
void emTorch_R0_Init(cEmTorch* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 0;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Parent, 2 Break, 3 Fall).
void emTorch_R0_Move(cEmTorch* pEm)
{
    EmTorch_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: a fixed torch; builds the matrices once, then stays a hit-box-only work.
void emTorch_R1_Set(cEmTorch* pEm)
{
    EmTorchWork* w = EMTORCH_WK(pEm);

    if (pEm->r_no_2 == 0) {
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        ScaleMatrix(pEm->mat, &pEm->scale);
        pEm->partsMatCalc();
        pEm->partsWorldCalc();
        w->Timer = 30;
        pEm->r_no_2++;
    }
    pEm->be_flag |= 0x4000;
}

// Rno1 == 1: carried torch: follows parts `partsNo` of pParent (rotation re-normalised unless
// Be_flg bit0) and plays its own motion when it has one.
void emTorch_R1_Parent(cEmTorch* pEm)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmTorchWork* w = EMTORCH_WK(pEm);
    cModel* parent = w->pParent;

    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    if (parent && parent->pList) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, pEm->mat, m);
        if (!(w->Be_flg & 1)) {
            v0.x = m[0][0];
            v0.y = m[1][0];
            v0.z = m[2][0];
            v1.x = m[0][1];
            v1.y = m[1][1];
            v1.z = m[2][1];
            v2.x = m[0][2];
            v2.y = m[1][2];
            v2.z = m[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 617 "D:/Bio4/Prog/emtorch.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 619 "D:/Bio4/Prog/emtorch.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 621 "D:/Bio4/Prog/emtorch.cpp"
            VECNormalize(&v2, &v2);
            m[0][0] = v0.x;
            m[1][0] = v0.y;
            m[2][0] = v0.z;
            m[0][1] = v1.x;
            m[1][1] = v1.y;
            m[2][1] = v1.z;
            m[0][2] = v2.x;
            m[1][2] = v2.y;
            m[2][2] = v2.z;
        }
        PSMTXCopy(m, pEm->mat);
    }
    if (pEm->Motion.pMot) {
        pEm->Motion.Mot_flag |= 0x40000000;
        MotionMove(pEm, 0);
    } else {
        pEm->partsMatCalc();
    }
    pEm->partsWorldCalc();
}

// Rno1 == 2: broken; on entry sets bit0 of the etc flag, hp 0, hides the model; then hit-box-only.
void emTorch_R1_Break(cEmTorch* pEm)
{
    EmTorchWork* w = EMTORCH_WK(pEm);
    u16* flg;

    if (pEm->r_no_2 == 0) {
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        w->Lost_wait = 150;
        pEm->r_no_2++;
    }
    pEm->be_flag |= 0x4000;
}

// Rno1 == 3: the hanging lamp drops (gravity 20 / frame) until the effect collision floor, where
// it deletes its flame, spawns the break est, plays the crash SE, sets a 2500 radius fire damage
// volume (DmgMgr type 5) for 1500 frames and hides.
void emTorch_R1_Fall(cEmTorch* pEm)
{
    EmTorchWork* w = EMTORCH_WK(pEm);
    u16* flg;
    f32 floor;

    switch (pEm->r_no_2) {
    case 0:
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        SndCall(6, 0x2D, &pEm->pos, 0, 0, pEm);
        pEm->r_no_2++;
    case 1:
        PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
        w->spd.y -= 20.0f;
        floor = EatMgr.getFloor(&pEm->pos, 0, 600.0f, 100000.0f, 0);
        if (pEm->pos.y < floor) {
            pEm->pos.y = floor;
            EffectEspDelete(1, w->EffKindId, pEm, 0);
            EffectEspgenDelete(1, w->EffKindId, pEm);
            EffectEfmDelete(1, w->EffKindId, pEm);
            EstSet(0, -1, &pEm->pos, 0, w->Eff_id, 2, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(6, 0x58, &pEm->pos, 0, 0, pEm);
            DmgMgr.set(DMG_TYPE_LAMP, 0x4B, &pEm->pos, 2500.0f, 1500.0f);
            pEm->be_flag &= ~2;
            pEm->r_no_2++;
        } else {
            RotMatrix(pEm->mat, &pEm->ang);
            TransMatrix(pEm->mat, &pEm->pos);
            ScaleMatrix(pEm->mat, &pEm->scale);
            if (pEm->Motion.pMot) {
                pEm->Motion.Mot_flag |= 0x40000000;
                MotionMove(pEm, 0);
            } else {
                pEm->partsMatCalc();
            }
            pEm->partsWorldCalc();
            pEm->be_flag |= 0x4000;
        }
        break;
    case 2:
        pEm->be_flag |= 0x4000;
        break;
    }
}

// Hit box by type: a cube around the origin (0 / 4), below it (1), further below (5); wall lamps
// (2 / 3) have none.
void emTorchYarareInit(cEmTorch* pEm)
{
    EmTorchWork* w = EMTORCH_WK(pEm);

    switch (pEm->type) {
    case 0:
    case 4:
    default:
        YarareInitCube((cEmHit*) pEm, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, YAT_FLAG_ON);
        break;
    case 1:
        YarareInitCube((cEmHit*) pEm, 0.0f, -w->size.y, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, YAT_FLAG_ON);
        break;
    case 2:
    case 3:
        break;
    case 5:
        YarareInitCube((cEmHit*) pEm, 0.0f, -w->size.y - 100.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, YAT_FLAG_ON);
        break;
    }
}

// Script entry: puts out an intact torch as if shot (style 0).
void cEmTorch::setBreak()
{
    if (hp > 0) {
        emTorchSetBreak(this, 0);
    }
}

// Script entry: jumps to the Break state without effects.
void cEmTorch::setDelete()
{
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Sets the est id of the flame / break effects and lights the flame (est parameter 0, Core_kind
// EffKindId) on an intact torch.
void cEmTorch::setEff(u8 eff_id)
{
    EmTorchWork* w = EMTORCH_WK(this);

    w->Eff_id = eff_id;
    if (hp > 0) {
        EstSet(this, -1, 0, 0, w->Eff_id, 0, 1, w->EffKindId, this, 0);
    }
}

// Attaches the torch to parts `partsNo` of `parent` (Rno1 1); flag skips the matrix
// normalisation.
void cEmTorch::setParent(cModel* parent, int partsNo, int flag)
{
    EmTorchWork* w = EMTORCH_WK(this);

    w->pParent = parent;
    w->oya_parts = partsNo;
    if (flag) {
        w->Be_flg |= 1;
    } else {
        w->Be_flg &= ~1;
    }
    r_no_0 = 1;
    r_no_1 = 1;
    r_no_2 = 0;
    r_no_3 = 0;
}
