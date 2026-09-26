// game/emobj.cpp: generic object enemy base (cEmObj): matrix update, scenario / effect
// collision quad registration, yarare setup.

#include "atari.h"
#include "emobj.h"
#include "math_sub.h"
#include "motion.h"
#include "emhit.h"

// Constructor-time defaults of an object enemy: no motion / collision flags, no collision
// pieces, effect and etc ids 0xFF.
void cEmObj::EmObjInit()
{
    EmObjWork* w = EMOBJ_WK(this);

    w->flags = 0;
    w->pSat = 0;
    w->pEat = 0;
    w->eff = 0xFF;
    w->etc = 0xFF;
}

// Shared per-frame step of the object classes: rebuilds mat from ang / pos / scale, advances the
// motion (EMOBJ_WK(this)->flags bit0) or just recomputes the parts matrices, then updates the world parts and
// re-seats the registered scenario (bit1) / effect (bit2) collision quads at the new coordinate.
void cEmObj::EmObjMove()
{
    EmObjWork* w = EMOBJ_WK(this);

    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    if (EMOBJ_WK(this)->flags & 1) {
        MotionMove(this, 0);
    } else {
        partsMatCalc();
        Motion.Mot_state = 0;
    }
    partsWorldCalc();
    if (w->flags & 2) {
        setSatMain();
    }
    if (w->flags & 4) {
        setEatMain();
    }
}

// Registers a scenario (walkable / blocking) collision quad of half size sx / sz and height sy at
// model-space `pos` (kept when NULL) with cSatMgr::create attribute `n` and `flag`; EMOBJ_WK(this)->flags bit1
// keeps it following the object.
void cEmObj::setSat(Vec* pos, int n, int flag, int cube, f32 sx, f32 sy, f32 sz)
{
    EmObjWork* w = EMOBJ_WK(this);

    if (pos) {
        w->satPos = *pos;
    }
    w->satSize.x = sx;
    w->satSize.y = sy;
    w->satSize.z = sz;
    w->satN = n;
    w->satFlag = flag;
    EMOBJ_WK(this)->flags |= 2;
    setSatMain();
}

// (Re)creates or re-activates the scenario collision piece at the object's pos / ang from the
// stored quad parameters.
void cEmObj::setSatMain()
{
    EmObjWork* w = EMOBJ_WK(this);
    Vec poly[4];

    if (w->pSat) {
        w->pSat->setDisable();
    }
    poly[0].x = w->satPos.x - w->satSize.x;
    poly[0].y = w->satPos.y;
    poly[0].z = w->satPos.z - w->satSize.z;
    poly[1].x = w->satPos.x + w->satSize.x;
    poly[1].y = w->satPos.y;
    poly[1].z = w->satPos.z - w->satSize.z;
    poly[2].x = w->satPos.x + w->satSize.x;
    poly[2].y = w->satPos.y;
    poly[2].z = w->satPos.z + w->satSize.z;
    poly[3].x = w->satPos.x - w->satSize.x;
    poly[3].y = w->satPos.y;
    poly[3].z = w->satPos.z + w->satSize.z;
    if (w->pSat == 0) {
        w->pSat = SatMgr.create(&pos, &ang, poly, w->satSize.y, w->satN, w->satFlag);
    } else {
        w->pSat->setEnable();
        w->pSat->setCoord(&pos, &ang);
    }
}

// Deactivates the scenario collision piece (m_Flag bit2 off) and stops following it.
void cEmObj::clrSat()
{
    EmObjWork* w = EMOBJ_WK(this);

    if (w->pSat) {
        w->pSat->setDisable();
    }
    EMOBJ_WK(this)->flags &= ~2;
}

// Registers an effect collision quad (EatMgr: bullets, effects, thrown objects) like setSat;
// EMOBJ_WK(this)->flags bit2 keeps it following the object.
void cEmObj::setEat(Vec* pos, int n, int flag, int cube, f32 sx, f32 sy, f32 sz)
{
    EmObjWork* w = EMOBJ_WK(this);

    if (pos) {
        w->eatPos = *pos;
    }
    w->eatSize.x = sx;
    w->eatSize.y = sy;
    w->eatSize.z = sz;
    w->eatN = n;
    w->eatFlag = flag;
    EMOBJ_WK(this)->flags |= 4;
    setEatMain();
}

// (Re)creates or re-activates the effect collision piece at the object's pos / ang.
void cEmObj::setEatMain()
{
    EmObjWork* w = EMOBJ_WK(this);
    Vec poly[4];

    if (w->pEat) {
        w->pEat->setDisable();
    }
    poly[0].x = w->eatPos.x - w->eatSize.x;
    poly[0].y = w->eatPos.y;
    poly[0].z = w->eatPos.z - w->eatSize.z;
    poly[1].x = w->eatPos.x + w->eatSize.x;
    poly[1].y = w->eatPos.y;
    poly[1].z = w->eatPos.z - w->eatSize.z;
    poly[2].x = w->eatPos.x + w->eatSize.x;
    poly[2].y = w->eatPos.y;
    poly[2].z = w->eatPos.z + w->eatSize.z;
    poly[3].x = w->eatPos.x - w->eatSize.x;
    poly[3].y = w->eatPos.y;
    poly[3].z = w->eatPos.z + w->eatSize.z;
    if (w->pEat == 0) {
        w->pEat = EatMgr.create(&pos, &ang, poly, w->eatSize.y, w->eatN, w->eatFlag);
    } else {
        w->pEat->setEnable();
        w->pEat->setCoord(&pos, &ang);
    }
}

// Deactivates the effect collision piece and stops following it.
void cEmObj::clrEat()
{
    EmObjWork* w = EMOBJ_WK(this);

    if (w->pEat) {
        w->pEat->setDisable();
    }
    EMOBJ_WK(this)->flags &= ~4;
}

// Adds hit box `no` to the object: a cylinder (cube == 0: YarareInitCube with radius `rad`) or a
// box of width `w` / height `h` at model-space `pos` (origin when NULL); flag bit0 is always set.
void cEmObj::setYarare(s16 no, Vec* pos, u16 flag, int cube, f32 w, f32 h, f32 rad)
{
    Vec p;

    if (pos == 0) {
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = 0.0f;
    } else {
        p.x = pos->x;
        p.y = pos->y;
        p.z = pos->z;
    }
    // The original re-extends both narrow parameters at the calls (`extsh r4, r4`, `clrlwi r5, r6, 16`
    // after `ori r6, r6, 1`): its compiler does not assume promoted incoming arguments (the
    // narrow-argument compiler difference). Ours does (combine's setup_incoming_promotions), so every
    // int/narrow/cast form folds the extensions away; the empty asms hide the promotion from combine
    // and `f` is pinned to flag's incoming r6 so the `ori` stays in place.
    register int f asm("r6") = flag;
    int n = no;
    asm("" : "+r"(n));
    asm("" : "+r"(f));
    f |= 1;
    if (cube == 0) {
        YarareInitCube(this, p.x, p.y, p.z, w, h, rad, n, f);
    } else {
        YarareInit(this, p.x, p.y, p.z, w, h, n, f);
    }
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool (one 0.0f) survives after
// setYarare's pool at the end of .rodata.
static void emObjPosClear(Vec* p)
{
    p->x = 0.0f;
    p->y = 0.0f;
    p->z = 0.0f;
}

// Sets the object's effect id byte (per class: effect / est number used on break).
void cEmObj::setEff(u8 eff_id)
{
    EMOBJ_WK(this)->eff = eff_id;
}

// The object's effect id byte.
u8 cEmObj::getEff()
{
    return EMOBJ_WK(this)->eff;
}

// Sets the object's extra parameter byte (per class meaning).
void cEmObj::setEtc(u8 etc_no)
{
    EMOBJ_WK(this)->etc = etc_no;
}

// The object's extra parameter byte.
u8 cEmObj::getEtc()
{
    return EMOBJ_WK(this)->etc;
}
