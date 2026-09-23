// game/emitem.cpp: item enemy (cEmItem): a pick-up model that follows a parent's parts, swings
// like a medal and breaks or drops when hit by a weapon or a damage volume.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emitem.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "motion.h"


typedef void (*EmItemFunc)(cEmItem*);

static EmItemFunc EmItem_R0_move_tbl[4] = {
    emItem_R0_Init,
    emItem_R0_Move,
    0,
    0,
};

static EmItemFunc EmItem_R1_move_tbl[5] = {
    emItem_R1_Set,
    emItem_R1_MedalSet,
    emItem_R1_Parent,
    emItem_R1_Drop,
    emItem_R1_Break,
};

// Creates an item enemy (id 0x4C) from a model / TPL at pos / rot. type 0: a hanging pick-up
// object (200 x 300 hit box) that drops to the floor when shot; type 1: a shootable medal
// (100 x 200 x 10) tied to room etc flag `etcNo` (already collected when its bit0 is set -> starts
// broken). Random swing phases / speeds are drawn for the medal rotation. NULL on failure.
cEmItem* SetEmItem(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int etcNo)
{
    cEmItem* em;
    EmItemWork* w;

    em = (cEmItem*) EmMgr.create(0x4C);
    if (em == 0) {
        return 0;
    }
    w = EMITEM_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetEmItem() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    em->be_flag |= 0x4000;
    EtcSetAddAmb(em, ETC_AMB_ITEM);
    w->Eff_id = 0xFF;
    switch (em->type) {
    case 0:
    default:
        w->size.x = 200.0f;
        w->size.y = 300.0f;
        w->size.z = 200.0f;
        break;
    case 1:
        w->size.x = 100.0f;
        w->size.y = 200.0f;
        w->size.z = 10.0f;
        break;
    }
    em->atari.init(0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f, 0, 2, 0);
    em->atari.setPriority(PRI_LV3);
    em->atari.m_flag &= ~0x300;
    emItemYarareInit(em);
    em->hp_max = em->hp = 1000;
    static const Vec ofs = { 0.0f, 0.0f, 0.0f };
    static const Vec size = { 1000.0f, 1000.0f, 0.0f };
    if (em->type != 1) {
        em->LightInfo.init2(0, 1, &ofs, &size, 0x20);
    } else {
        em->LightInfo.init2(0, 1, &ofs, &size, 0x20);
    }
    int rotType = 0;
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->Be_flg = 0;
    w->Rot_type = rotType;
    w->Status = 0;
    w->rotAng.x = fRand1_1() * 3.1415927f;
    w->rotAng.y = fRand1_1() * 3.1415927f;
    w->rotAng.z = fRand1_1() * 3.1415927f;
    w->rotSpd.x = fRand0_1() * 0.017453292f + 0.05235988f;
    w->rotSpd.y = fRand0_1() * 0.034906585f + 0.08726646f;
    w->rotSpd.z = fRand0_1() * 0.017453292f + 0.05235988f;
    if (Rnd() & 1) {
        w->rotSpd.x = -w->rotSpd.x;
    }
    if (Rnd() & 1) {
        w->rotSpd.y = -w->rotSpd.y;
    }
    if (Rnd() & 1) {
        w->rotSpd.z = -w->rotSpd.z;
    }
    w->rotAmp.x = 0.17453292f;
    w->rotAmp.y = 0.5235988f;
    w->rotAmp.z = 0.17453292f;
    if (em->type == 1) {
        u16* p;

        w->Etc_no = etcNo;
        p = GetEtcFlgPtr(etcNo, pG->room_id);
        if (p && (*p & 1)) {
            em->hp = 0;
        }
    }
    if (em->hp <= 0) {
        em->r_no_0 = 1;
        em->r_no_1 = 4;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    } else if (em->type != 1) {
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    } else {
        em->r_no_0 = 1;
        em->r_no_1 = 1;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
    return em;
}

// Damage check per frame: a damage volume hit (DmgMgr types 1/4/5/7) or a registered weapon hit
// (not knife / grenades) knocks type 0 down (Rno1 3 Drop, spark est 0x57 at the hit) or breaks
// the medal (Rno1 4 Break, its Eff_id est and SE 0x2E). Status 3 marks a weapon hit this frame.
void emItemDmCk(cEmItem* pEm)
{
    EmItemWork* w = EMITEM_WK(pEm);
    u8 wep;
    Vec hit;
    Vec dir;

    if (pEm->hp > 0) {
        switch (DmgMgr.hitCheck(&pEm->pos, &hit)) {
        case DMG_TYPE_FIRE:
        case DMG_TYPE_FLAME:
        case DMG_TYPE_LAMP:
        case DMG_TYPE_ENV_FIRE:
            switch (pEm->type) {
            case 0:
            default:
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 3;
                pEm->hp = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                break;
            case 1:
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 4;
                pEm->hp = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
                EstSet(pEm, -1, 0, 0, w->Eff_id, 0, 0, ESP_CORE_KIND_NONE, pEm, 0);
                break;
            }
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
    w->Status = 3;
    switch (pEm->type) {
    case 0:
    default:
        pEm->hp = 0;
        if (EmGetDmPos(pEm, &hit, &dir) == 0) {
            dir.x = 0.0f;
            dir.y = 0.0f;
            dir.z = 0.0f;
        }
        EstSet(0, -1, &pEm->getPartsPtr(0)->world, &dir, EFF_CORE, 0x57, 0, ESP_CORE_KIND_NONE, 0, 0);
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 3;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        break;
    case 1:
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 4;
        pEm->hp = 0;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
        EstSet(pEm, -1, 0, 0, w->Eff_id, 0, 0, ESP_CORE_KIND_NONE, pEm, 0);
        break;
    }
    if (pEm->type == 1) {
        SndCall(6, 0x2E, &pEm->pos, 0, 0, pEm);
    }
}

// Per-frame: damage check, clear the hit-box-only flag, run the Rno0 routine.
void cEmItem::move()
{
    emItemDmCk(this);
    be_flag &= ~0x4000;
    EmItem_R0_move_tbl[r_no_0](this);
}

// Rno0 == 0: resets to the Set state (medals: MedalSet).
void emItem_R0_Init(cEmItem* pEm)
{
    if (pEm->type != 1) {
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 0;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
    } else {
        pEm->r_no_0 = 1;
        pEm->r_no_1 = 1;
        pEm->r_no_2 = 0;
        pEm->r_no_3 = 0;
    }
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 MedalSet, 2 Parent, 3 Drop, 4 Break).
void emItem_R0_Move(cEmItem* pEm)
{
    EmItem_R1_move_tbl[pEm->r_no_1](pEm);
}

// Rno1 == 0: static object; builds the matrices once, then stays a hit-box-only work.
void emItem_R1_Set(cEmItem* pEm)
{
    if (pEm->r_no_2 == 0) {
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        ScaleMatrix(pEm->mat, &pEm->scale);
        pEm->partsMatCalc();
        pEm->partsWorldCalc();
        pEm->r_no_2++;
    }
    pEm->be_flag |= 0x4000;
}

// Rno1 == 1: the medal in place: rebuilds the matrices every frame and applies the swing.
void emItem_R1_MedalSet(cEmItem* pEm)
{
    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    pEm->partsMatCalc();
    pEm->partsWorldCalc();
    emItemRotMove(pEm);
}

// Rno1 == 2: follows parts `partsNo` of pParent (setParent), re-normalising the rotation unless
// noNormalize, plays its own motion when it has one, then applies the swing.
void emItem_R1_Parent(cEmItem* pEm)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmItemWork* w = EMITEM_WK(pEm);
    cModel* parent = w->pParent;

    RotMatrix(pEm->mat, &pEm->ang);
    TransMatrix(pEm->mat, &pEm->pos);
    ScaleMatrix(pEm->mat, &pEm->scale);
    if (parent && parent->pList) {
        PSMTXConcat(parent->getPartsPtr(w->oya_parts)->mat, pEm->mat, m);
        if (w->noNormalize == 0) {
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
#line 475 "D:/Bio4/Prog/emitem.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 477 "D:/Bio4/Prog/emitem.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 479 "D:/Bio4/Prog/emitem.cpp"
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
    emItemRotMove(pEm);
}

// Rno1 == 3: the shot object falls: Rno2 0 takes the world position from the matrix, 1 falls with
// gravity 10/frame until the effect collision floor (Status 1 on landing), 2 rests as a hit-box-
// only work.
void emItem_R1_Drop(cEmItem* pEm)
{
    EmItemWork* w = EMITEM_WK(pEm);
    f32 floor;

    switch (pEm->r_no_2) {
    case 0:
        pEm->pos.x = pEm->mat[0][3];
        pEm->pos.y = pEm->mat[1][3];
        pEm->pos.z = pEm->mat[2][3];
        Matrix2AxisAngle(pEm->mat, &pEm->ang);
        w->spd.x = 0.0f;
        w->spd.y = -10.0f;
        w->spd.z = 0.0f;
        pEm->r_no_2++;
    case 1:
        w->spd.y -= 10.0f;
        floor = EatMgr.getFloor(&pEm->pos, 0, 0.0f, 100000.0f, 0);
        PSVECAdd(&pEm->pos, &w->spd, &pEm->pos);
        if (pEm->pos.y < floor) {
            pEm->pos.y = floor;
            w->Status = 1;
            pEm->r_no_2++;
        }
        RotMatrix(pEm->mat, &pEm->ang);
        TransMatrix(pEm->mat, &pEm->pos);
        ScaleMatrix(pEm->mat, &pEm->scale);
        pEm->partsMatCalc();
        pEm->partsWorldCalc();
        break;
    case 2:
        pEm->be_flag |= 0x4000;
        break;
    }
}

// Rno1 == 4: the medal is destroyed: hides the model, hp 0, Status 2, sets bit0 of its etc flag so
// it stays collected; then hit-box-only.
void emItem_R1_Break(cEmItem* pEm)
{
    EmItemWork* w = EMITEM_WK(pEm);
    u16* flg;

    switch (pEm->r_no_2) {
    case 0:
        w->Status = 2;
        pEm->hp = 0;
        pEm->be_flag &= ~2;
        flg = GetEtcFlgPtr(w->Etc_no, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        pEm->r_no_2++;
    case 1:
        pEm->be_flag |= 0x4000;
        break;
    }
}

// Hit box by type: a cube around the object's centre (type 0) or, for the medal, a cylinder
// placed 1000 below (type 1).
void emItemYarareInit(cEmItem* pEm)
{
    EmItemWork* w = EMITEM_WK(pEm);

    switch (pEm->type) {
    case 0:
    default:
        YarareInitCube((cEmHit*) pEm, 0.0f, -(w->size.y * 0.5f), 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, YAT_FLAG_ON);
        break;
    case 1:
        YarareInitCube((cEmHit*) pEm, 0.0f, -1000.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 1, YAT_FLAG_ON);
        break;
    }
}

// Est id spawned when the medal breaks.
void cEmItem::setEff(u8 eff_id)
{
    EMITEM_WK(this)->Eff_id = eff_id;
}

// Frame status: 1 landed, 2 broken, 3 hit by a weapon.
int cEmItem::ckStatus()
{
    return EMITEM_WK(this)->Status;
}

// Attaches the item to parts `partsNo` of `parent` (Rno1 2) and disables the parent's atari flag
// 0x200 so shots reach the item.
void cEmItem::setParent(cModel* parent, int partsNo, int noNormalize)
{
    EmItemWork* w = EMITEM_WK(this);

    w->pParent = parent;
    w->oya_parts = partsNo;
    w->noNormalize = noNormalize;
    r_no_0 = 1;
    r_no_1 = 2;
    r_no_2 = 0;
    r_no_3 = 0;
    ((cEm*) parent)->atari.m_flag &= ~0x200;
}

// Swing mode: 1 = medal swing on x/z with a y wobble, 2 = fixed rotation from ang, 0 = none.
void cEmItem::setRotType(u8 type)
{
    EMITEM_WK(this)->Rot_type = type;
}

// Applies the swing to parts 0: mode 1 rotates by sin(rotAng) * rotAmp per axis and advances
// rotAng by rotSpd, mode 2 sets the parts rotation from em->ang.
void emItemRotMove(cEmItem* pEm)
{
    EmItemWork* w = EMITEM_WK(pEm);
    Mtx tmp;
    cParts* p;

    switch (w->Rot_type) {
    case 1:
        p = pEm->getPartsPtr(0);
        PSMTXRotRad(tmp, 'x', SINF(w->rotAng.x) * w->rotAmp.x);
        PSMTXConcat(tmp, p->mat, p->mat);
        TransMatrix(p->mat, &p->world);
        PSMTXRotRad(tmp, 'z', SINF(w->rotAng.z) * w->rotAmp.z);
        PSMTXConcat(tmp, p->mat, p->mat);
        TransMatrix(p->mat, &p->world);
        PSMTXRotRad(tmp, 'y', SINF(w->rotAng.y) * w->rotAmp.y);
        PSMTXConcat(p->mat, tmp, p->mat);
        TransMatrix(p->mat, &p->world);
        w->rotAng.x += w->rotSpd.x;
        w->rotAng.x = LIMIT_ANGLE(w->rotAng.x);
        w->rotAng.y += w->rotSpd.y;
        w->rotAng.y = LIMIT_ANGLE(w->rotAng.y);
        w->rotAng.z += w->rotSpd.z;
        w->rotAng.z = LIMIT_ANGLE(w->rotAng.z);
        break;
    case 2:
        p = pEm->getPartsPtr(0);
        RotMatrix(p->mat, &pEm->ang);
        TransMatrix(p->mat, &p->world);
        break;
    }
}
