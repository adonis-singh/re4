// game/embarrel.cpp: barrel enemy (cEmBarrel): explosive barrels that blow up when shot and the
// burning barrel of room 227 that rolls down the EMI route, running over the player and enemies.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "embarrel.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "quake.h"
#include "pad.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "at_mod.h"
#include "em_sub.h"
#include "est.h"


typedef void (*EmBarrelFunc)(cEmBarrel*);

static EmBarrelFunc EmBarrel_R0_move_tbl[4] = {
    emBarrel_R0_Init,
    emBarrel_R0_Move,
    0,
    0,
};

static EmBarrelFunc EmBarrel_R1_move_tbl[3] = {
    emBarrel_R1_Set,
    emBarrel_R1_Break,
    emBarrel_R1_R227Roll,
};

// SetBarrel and SetR227Barrel share the failure report and the light area origin: both live in
// inline helpers parsed before either function, which is where the string and `ofs` sit in .rodata
// (before SetBarrel's own `size` and constant pool).
static inline void barrelInitFailed(cEmBarrel* em)
{
    pLog->err(0, 0, "SetBarrel() failed.");
    EmMgr.destroy(em);
}

// One `.rodata` copy of the light offset shared by SetBarrel and SetR227Barrel (an inline parsed
// before both); the address must be an argument expression so that `&ofs` is evaluated before `&size`.
static inline const Vec* barrelLightOfs()
{
    static const Vec ofs = { 0.0f, 0.0f, 0.0f };

    return &ofs;
}

// Creates an explosive barrel enemy (id 0x48) from a model / TPL at pos / rot: type 0 / 2 the red
// explosive barrels (a solid atari cylinder the player bumps into, 1000 hp, break state saved in
// room etc flag `etcNo`), type 1 the rolling barrel layout. NULL on failure.
cEmBarrel* SetBarrel(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo)
{
    cEmBarrel* em;
    EmBarrelWork* w;
    u16* flg;
    int zero;

    em = (cEmBarrel*) EmMgr.create(0x48);
    if (em == 0) {
        return 0;
    }
    w = EMBARREL_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        barrelInitFailed(em);
        return 0;
    }
    em->type = type;
    switch (em->type) {
    case 0:
        EtcSetAddAmb(em, ETC_AMB_DRAM);
        break;
    case 1:
    default:
        EtcSetAddAmb(em, ETC_AMB_BARREL);
        break;
    case 2:
        EtcSetAddAmb(em, ETC_AMB_BARREL);
        break;
    }
    w->Eff_id = 0xFF;
    {
        cAtariInfo* at = &em->atari;

        at->init(0.0f, 750.0f, 0.0f, 300.0f, 300.0f, 300.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
    }
    if (em->type != 1) {
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, 330.0f, 1250.0f, 330.0f, 0, YAT_FLAG_ON);
    } else {
        YarareInit((cEmHit*) em, 0.0f, 0.0f, 0.0f, 700.0f, 1250.0f, 1, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    }
    em->hp_max = em->hp = 1000;
    {
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, barrelLightOfs(), &size, 0x10);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->Be_flg = zero;
    w->Seid = 0;
    w->Bomb_wait = 0;
    w->Etc_no = etcNo;
    flg = GetEtcFlgPtr(etcNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->clearStatus(EM_STATUS_ACTIVE);
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    } else {
        em->setStatus(EM_STATUS_ACTIVE);
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
    emBarrelEatSet(em);
    return em;
}

// Room 227 only: creates the burning barrel (type 1) from the room archive models 0x20 / 0x21 and
// starts it rolling (Rno1 2) with its own effect Core_kind. NULL outside room 227 or on failure.
cEmBarrel* SetR227Barrel(Vec* pPos, Vec* pAng)
{
    cEmBarrel* em;
    EmBarrelWork* w;
    int zero;

    if (pG->stage_no != 2 || pG->room_no != 0x27) {
        return 0;
    }
    em = (cEmBarrel*) EmMgr.create(0x48);
    if (em == 0) {
        return 0;
    }
    w = EMBARREL_WK(em);
    if (pPos) {
        em->pos = *pPos;
    }
    if (pAng) {
        em->ang = *pAng;
    }
    if (em->modelInit(ROOM_ARC_PTR(pG->pRoom, 0x20), ROOM_ARC_PTR(pG->pRoom, 0x21)) == 0) {
        barrelInitFailed(em);
        return 0;
    }
    em->type = 1;
    w->Eff_id = 0xFF;
    w->EffKindId = EspPullCoreKind();
    zero = 0;
    {
        cAtariInfo* at = &em->atari;

        at->init(0.0f, 750.0f, 0.0f, 300.0f, 300.0f, 300.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
    }
    YarareInit((cEmHit*) em, -350.0f, 0.0f, 0.0f, 700.0f, 1250.0f, 1, YAT_FLAG_ON | YAT_FLAG_X_AXIS);
    em->hp_max = em->hp = 1000;
    {
        static const Vec size = { 4000.0f, 4000.0f, 4000.0f };

        em->LightInfo.init2(0, 1, barrelLightOfs(), &size, 0x10);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    em->r_no_0 = 1;
    em->r_no_1 = 2;
    w->Be_flg = 0;
    em->r_no_2 = zero;
    em->r_no_3 = 0;
    return em;
}

// Explosive barrel damage check: a damage volume hit or any registered weapon hit (not knife /
// grenades) blows the barrel up, with the break style from the weapon class (shotguns by
// distance).
void emBarrelDmCk(cEmBarrel* pEm)
{
    u8 wep;
    Vec hit;

    if (pEm->hp > 0) {
        switch (DmgMgr.hitCheck(&pEm->pos, &hit)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            emBarrelSetBreak(pEm, 2);
            return;
        }
    }
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
    pEm->hp = 0;
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
        emBarrelSetBreak(pEm, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (pEm->dmg.m_Dist > 36000000.0f) {
            emBarrelSetBreak(pEm, 0);
        } else {
            emBarrelSetBreak(pEm, 1);
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
    default:
        emBarrelSetBreak(pEm, 2);
        break;
    }
}

// Rolling barrel damage check: a damage volume hit explodes it; a weapon hit takes damage by
// weapon class (shotguns by distance) and explodes it when the hp is gone, else spawns the hit
// est (owner 1, est 2).
void emBarrelDmCk2(cEmBarrel* pEm)
{
    YARARE_INFO* part;
    u8 wep;
    int dmg;
    int near;
    Vec hit;

    if (pEm->hp > 0) {
        switch (DmgMgr.hitCheck(&pEm->pos, &hit)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            emBarrelSetBreak(pEm, 2);
            return;
        }
    }
    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    pEm->dmg.m_Flag = 0;
    part = pEm->dmg.m_pDamageYarare;
    near = 0;
    if (part->len < 36000000.0f) {
        near = 1;
    }
    wep = pEm->dmg.m_Wep;
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
    pEm->dmg.m_Timer = 1;
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x11:
    case 0x26:
    case 0x2B:
        dmg = 500;
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        dmg = 400;
        break;
    case 9:
    case 0xA:
    case 0x10:
    case 0x14:
    case 0x15:
    case 0x28:
        dmg = 1000;
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            dmg = 9999;
        } else {
            dmg = 500;
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    LifeDownSet(pEm, dmg, 0);
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
            emBarrelSetBreak(pEm, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (pEm->dmg.m_Dist > 36000000.0f) {
                emBarrelSetBreak(pEm, 0);
            } else {
                emBarrelSetBreak(pEm, 1);
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
        default:
            emBarrelSetBreak(pEm, 2);
            break;
        }
    } else {
        EmDmBloodSet2(pEm, 1, 2, 0, 0, 0);
    }
}

// Destroys the barrel: hp 0, hidden, rolling SE stopped; explosive barrels run the blast
// (emBarrelSetBomb), the rolling barrel deletes its fire effects and either bursts with the
// smaller blast (when it was the burning variant, est 1/5) or just breaks (est 1/6). Then
// Rno1 1 Break.
void emBarrelSetBreak(cEmBarrel* em, int kind)
{
    EmBarrelWork* w = EMBARREL_WK(em);

    em->hp = 0;
    em->be_flag &= ~2;
    SndStop(w->Seid, 0);
    if (em->type != 1) {
        if (w->Eff_id != 0xFF) {
            emBarrelSetBomb(em);
        }
    } else {
        EffectEspDelete(0, w->EffKindId, em, 0);
        EffectEspgenDelete(0, w->EffKindId, em);
        EffectEfmDelete(0, w->EffKindId, em);
        if (w->rollSe != 0) {
            EstSet(0, -1, &em->pos, 0, EFF_ROOM, 5, 0, ESP_CORE_KIND_NONE, 0, 0);
            emBarrelSetBomb2(em);
            SndCall(6, 3, &em->pos, 0, 0, em);
        } else {
            EstSet(0, -1, &em->pos, 0, EFF_ROOM, 6, 0, ESP_CORE_KIND_NONE, 0, 0);
            SndCall(6, 3, &em->pos, 0, 0, em);
        }
    }
    em->r_no_0 = 1;
    em->r_no_1 = 1;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Per-frame: the type's damage check, the Rno0 routine, then while live the model-vs-player
// atari, the effect collision quad, and the delayed blast: Bomb_wait counts down to a
// PlWepHitCheck2 explosion (type 0x13) of radius Bomb_r at Bomb_pos (Status_flg[0] 0x00800000
// marks a barrel explosion this frame).
void cEmBarrel::move()
{
    EmBarrelWork* w = EMBARREL_WK(this);

    if (type != 1) {
        emBarrelDmCk(this);
    } else {
        emBarrelDmCk2(this);
    }
    be_flag &= ~0x4000;
    EmBarrel_R0_move_tbl[r_no_0](this);
    if ((be_flag & 0x201) == 1) {
        EmAtCheck(this);
        atari.move();
        emBarrelEatSet(this);
        if (w->Bomb_wait) {
            w->Bomb_wait--;
            if (w->Bomb_wait == 0) {
                PlWepHitCheck2(0, &w->Bomb_pos, &w->Bomb_pos, 0x13, 3, w->Bomb_r);
                StaFlagOn(pG, STA_PL_FIRE);
            }
        }
    }
}

// Rno0 == 0: resets to the Set state.
void emBarrel_R0_Init(cEmBarrel* pEm)
{
    pEm->r_no_0 = 1;
    pEm->r_no_1 = 0;
    pEm->r_no_2 = 0;
    pEm->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Break, 2 R227Roll).
void emBarrel_R0_Move(cEmBarrel* pEm)
{
    EmBarrel_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: intact barrel; builds the matrices once, then stays a hit-box-only work.
void emBarrel_R1_Set(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);

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

// Rno1 == 1: destroyed; on entry saves the etc flag (types 0 / 2), hides, drops the atari and
// ACTIVE; the rolling barrel destroys its work after 10 frames.
void emBarrel_R1_Break(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    u16* flg;

    switch (pEm->r_no_2) {
    case 0:
        if (pEm->type == 0 || pEm->type == 2) {
            flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
            if (flg) {
                *flg |= 1;
            }
        }
        pEm->be_flag &= ~2;
        pEm->hp = 0;
        pEm->atari.m_flag &= ~0x200;
        pEm->clearStatus(EM_STATUS_ACTIVE);
        w->Timer = 10;
        pEm->r_no_2++;
    case 1:
        if (w->Timer) {
            w->Timer--;
        } else if (pEm->type == 1) {
            EmMgr.destroy(pEm);
        }
        break;
    }
}

// Rno1 == 2: the room 227 barrel rolls down the EMI route (type 6 entries): Rno2 0 finds the
// route (one in four barrels burns and loops the fire SE), then each frame steers toward the next
// waypoint, falls with gravity 10 and bounces on the floor (dust est / SE on hard bounces), turns
// and spins with the travelled distance, runs the player over (emBarrelRollHitCk -> explode) or
// kills ganados in its path; the route end or a lost route destroys it.
void emBarrel_R1_R227Roll(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    cParts* p;
    f32 floor;
    f32 ang;
    f32 dist;
    f32 spin;

    switch (pEm->r_no_2) {
    case 0:
        if (emBarrelSetRollRoute(pEm) == 0) {
            pEm->pos.x = pEm->mat[0][3];
            pEm->pos.y = pEm->mat[1][3];
            pEm->pos.z = pEm->mat[2][3];
            Matrix2AxisAngle(pEm->mat, &pEm->ang);
            pEm->r_no_0 = 1;
            pEm->r_no_1 = 1;
            pEm->r_no_2 = 0;
            pEm->r_no_3 = 0;
            return;
        }
        pEm->atari.throughOn();
        w->rollSe = 0;
        if ((Rnd() & 3) == 0) {
            w->rollSe = 1;
            EstSet(pEm, -1, 0, 0, EFF_ROOM, 4, 0, w->EffKindId, pEm, 0);
        }
        w->Se_wait = 0;
        w->floorOfs = 700.0f;
        w->Roll_spd.x = 0.0f;
        w->Roll_spd.y = 0.0f;
        w->Roll_spd.z = 0.0f;
        pEm->r_no_2++;
    case 1:
        if (emBarrelSetRollSpd(pEm)) {
            emBarrelSetBreak(pEm, 0);
            return;
        }
    default:
        if (w->rollSe) {
            if (w->Se_wait) {
                w->Se_wait--;
            } else {
                w->Seid = SndCall(6, 6, &pEm->pos, 0, 0, pEm);
                w->Se_wait = 30;
            }
        }
        w->Roll_spd.y -= 10.0f;
        PSVECAdd(&pEm->pos, &w->Roll_spd, &pEm->pos);
        floor = EatMgr.getFloor(&pEm->pos, 0, 600.0f, 100000.0f, 0);
        if (pEm->pos.y < floor + w->floorOfs) {
            pEm->pos.y = floor + w->floorOfs;
            w->Roll_spd.y *= -0.3f;
            if (w->Roll_spd.y > 20.0f) {
                Vec v;

                v = pEm->pos;
                v.y -= w->floorOfs;
                EstSet(0, -1, &v, 0, EFF_ROOM, 3, 0, ESP_CORE_KIND_NONE, 0, 0);
                SndCall(6, 2, &pEm->pos, 0, 0, pEm);
            }
        }
        ang = GetXZAngle(&pEm->pos_old, &pEm->pos);
        pEm->ang.y += Muku2(pEm->ang.y, ang, 0.012271847f);
        pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
        dist = VEC_DIST(&pEm->pos, &pEm->pos_old);
        if (dist > 500.0f) {
            dist = 500.0f;
        }
        spin = dist * 0.002f * 0.31415927f;
        p = pEm->getPartsPtr(0);
        p->ang.x += spin;
        p->ang.x = LIMIT_ANGLE(p->ang.x);
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        pEm->partsMatCalc();
        pEm->partsWorldCalc();
        if (emBarrelRollHitCk(pEm)) {
            emBarrelSetBreak(pEm, 0);
        } else {
            emBarrelRunDownCk(pEm);
        }
        break;
    }
}

// Finds the first EMI route entry of type 6 as the roll start waypoint; 0 when there is none.
int emBarrelSetRollRoute(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    u8* emi;
    int i;
    int idx;

    emi = (u8*) pG->pEmi;
    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < pG->pEmi->n; i++) {
        u32 o = i * 0x40 + 8;

        if (((u8*) pG->pEmi)[o] == 6) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        return 0;
    }
    w->Route_no = idx;
    {
        u32 o = idx * 0x40 + 8;

        w->pRoute = (EmiEntry*) ((u8*) pG->pEmi + o);
    }
    return 1;
}

// Steers the roll: when within 500 units of the waypoint advances to the next type-6 entry
// (1 = route finished), then points Roll_spd (50..150 units / frame, accelerating by 1) at it.
int emBarrelSetRollSpd(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    u8* emi;
    EmiEntry* e;
    int idx;
    int i;
    f32 spd;
    Vec dir;

    emi = (u8*) pG->pEmi;
    if (emi == 0) {
        return 1;
    }
    e = w->pRoute;
    spd = (e->pos.x - pEm->pos.x) * (e->pos.x - pEm->pos.x) + (e->pos.z - pEm->pos.z) * (e->pos.z - pEm->pos.z);
    if (spd < 250000.0f) {
        idx = -1;
        for (i = w->Route_no + 1; i < pG->pEmi->n; i++) {
            u32 o = i * 0x40 + 8;

            if (((u8*) pG->pEmi)[o] == 6) {
                idx = i;
                break;
            }
        }
        if (idx == -1) {
            return 1;
        }
        w->Route_no = idx;
        {
            u32 o = idx * 0x40 + 8;

            e = (EmiEntry*) ((u8*) pG->pEmi + o);
        }
        w->pRoute = e;
    }
    PSVECSubtract(&e->pos, &pEm->pos, &dir);
    dir.y = 0.0f;
#line 1017 "D:/Bio4/Prog/embarrel.cpp"
    VECNormalize(&dir, &dir);
    spd = SQRTF(w->Roll_spd.x * w->Roll_spd.x + w->Roll_spd.z * w->Roll_spd.z) + 1.0f;
    if (spd < 50.0f) {
        spd = 50.0f;
    }
    if (spd > 150.0f) {
        spd = 150.0f;
    }
    PSVECScale(&dir, &dir, spd);
    w->Roll_spd.x = dir.x;
    w->Roll_spd.z = dir.z;
    return 0;
}

// Est id of the explosion.
void cEmBarrel::setEff(u8 eff_id)
{
    EmBarrelWork* w = EMBARREL_WK(this);

    w->Eff_id = eff_id;
}

// The explosion: hides the barrel, plays the type's blast SE (room 404 has its own), spawns est
// Eff_id, schedules the 6000 radius damage check 2 frames later at pos + 500 y and shakes the
// camera with a power falling off with distance (10 .. 4 within 20000 units).
void emBarrelSetBomb(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    Camera* cam;
    cParts* p;
    Vec v;
    f32 d2;
    f32 power;

    pEm->hp = 0;
    pEm->be_flag &= ~2;
    switch (pEm->type) {
    case 0:
    default:
        SndCall(6, 0x48, &pEm->pos, 0, 0, pEm);
        break;
    case 1:
        SndCall(6, 8, &pEm->pos, 0, 0, pEm);
        break;
    case 2:
        if (pG->room_id == 0x404) {
            SndCall(6, 0x53, &pEm->pos, 0, 0, pEm);
        } else {
            SndCall(6, 0x56, &pEm->pos, 0, 0, pEm);
        }
        break;
    }
    EstSet(0, -1, &pEm->pos, &pEm->ang, w->Eff_id, 0, 0, ESP_CORE_KIND_NONE, 0, 0);
    v = pEm->pos;
    v.y += 500.0f;
    w->Bomb_wait = 2;
    w->Bomb_pos = v;
    w->Bomb_r = 6000.0f;
    cam = &pG->Camera;
    p = pEm->getPartsPtr(1);
    d2 = (p->world.x - cam->param.pos.x) * (p->world.x - cam->param.pos.x) +
         (p->world.y - cam->param.pos.y) * (p->world.y - cam->param.pos.y) +
         (p->world.z - cam->param.pos.z) * (p->world.z - cam->param.pos.z);
    if (d2 < 400000000.0f) {
        power = 10.0f;
        if (d2 > 25000000.0f) {
            power = 8.0f;
        }
        if (d2 > 100000000.0f) {
            power = 6.0f;
        }
        if (d2 > 225000000.0f) {
            power = 4.0f;
        }
        QuakeExec(0, 0, 5, power, 2);
    }
}

// The smaller blast of the burning rolling barrel: same as emBarrelSetBomb without the est and
// with a 4000 radius damage check.
void emBarrelSetBomb2(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    Camera* cam;
    cParts* p;
    Vec v;
    f32 d2;
    f32 power;

    pEm->hp = 0;
    pEm->be_flag &= ~2;
    switch (pEm->type) {
    case 0:
    default:
        SndCall(6, 0x48, &pEm->pos, 0, 0, pEm);
        break;
    case 1:
        SndCall(6, 8, &pEm->pos, 0, 0, pEm);
        break;
    case 2:
        SndCall(6, 0x56, &pEm->pos, 0, 0, pEm);
        break;
    }
    v = pEm->pos;
    v.y += 500.0f;
    w->Bomb_wait = 2;
    w->Bomb_pos = v;
    w->Bomb_r = 4000.0f;
    cam = &pG->Camera;
    p = pEm->getPartsPtr(1);
    d2 = (p->world.x - cam->param.pos.x) * (p->world.x - cam->param.pos.x) +
         (p->world.y - cam->param.pos.y) * (p->world.y - cam->param.pos.y) +
         (p->world.z - cam->param.pos.z) * (p->world.z - cam->param.pos.z);
    if (d2 < 400000000.0f) {
        power = 10.0f;
        if (d2 > 25000000.0f) {
            power = 8.0f;
        }
        if (d2 > 100000000.0f) {
            power = 6.0f;
        }
        if (d2 > 225000000.0f) {
            power = 4.0f;
        }
        QuakeExec(0, 0, 5, power, 2);
    }
}

// Keeps an intact explosive barrel's effect collision quad (660 wide, 1250 high, attribute
// 0x400000) at its position; the rolling barrel has none.
void emBarrelEatSet(cEmBarrel* pEm)
{
    EmBarrelWork* w = EMBARREL_WK(pEm);
    Vec v[4];

    if (pEm->type == 1) {
        return;
    }
    if (w->sat) {
        w->sat->setDisable();
    }
    if (pEm->hp <= 0) {
        return;
    }
    if (w->sat == 0) {
        f32 r = 330.0f;

        v[0].x = -r;
        v[0].y = 0.0f;
        v[0].z = -r;
        v[1].x = r;
        v[1].y = 0.0f;
        v[1].z = -r;
        v[2].x = r;
        v[2].y = 0.0f;
        v[2].z = r;
        v[3].x = -r;
        v[3].y = 0.0f;
        v[3].z = r;
        w->sat = EatMgr.create(&pEm->pos, &pEm->ang, v, 1250.0f, 0x400000, 0);
    } else {
        w->sat->setEnable();
        w->sat->setCoord(&pEm->pos, &pEm->ang);
    }
}

// Rolling barrel vs player: when the live player stands inside the barrel's box (2500 x 700 x
// 1400 in barrel space) takes 600 life, starts damage motion 8, vibrates and shakes the camera;
// returns 1 (the barrel then explodes).
int emBarrelRollHitCk(cEmBarrel* pEm)
{
    Mtx inv;
    Vec v;
    int dead;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    PSMTXInverse(pEm->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 1250.0f) {
        return 0;
    }
    if (v.x < -1250.0f) {
        return 0;
    }
    if (v.y > 700.0f) {
        return 0;
    }
    if (v.y < -2500.0f) {
        return 0;
    }
    if (v.z > 700.0f) {
        return 0;
    }
    if (v.z < -700.0f) {
        return 0;
    }
    LifeDownSet(pPL, 600, 0);
    PlSetDamage(PL_DM_AUTO, 0, 0);
    VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    return 1;
}

// Rolling barrel vs enemies: every live ganado (ids 0x10..0x20) inside the barrel's box is killed
// outright (hp 0, Rno0 3 / Rno1 4 death routine).
void emBarrelRunDownCk(cEmBarrel* pEm)
{
    Mtx inv;
    Vec v;
    cEm* e;
    u32 i;

    PSMTXInverse(pEm->mat, inv);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        e = EmMgr.fastAt(i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == pEm) {
            continue;
        }
        PSMTXMultVec(inv, &e->pos, &v);
        if (v.x > 1250.0f) {
            continue;
        }
        if (v.x < -1250.0f) {
            continue;
        }
        if (v.y > 700.0f) {
            continue;
        }
        if (v.y < -2500.0f) {
            continue;
        }
        if (v.z > 700.0f) {
            continue;
        }
        if (v.z < -700.0f) {
            continue;
        }
        e->hp = 0;
        e->r_no_0 = 3;
        e->r_no_1 = 4;
        e->r_no_2 = 0;
        e->r_no_3 = 0;
    }
}
