// game/pl_body: cPlBody, the player's body helper kept at cEm::Body: the model infos of the head /
// hands / arms, the waist twist applied to the spine parts each frame (waistMove; aiming turns the
// upper body), the two face-morph (SPAE) records and the weapon-hand model the weapon module
// supplies.

#include "pl_body.h"
#include "atari.h"

// Body helper for `model` (the player): no head / hand / arm models yet, waist straight.
cPlBody::cPlBody(cModel* model)
{
    m_pMod = model;
    pHeadData = 0;
    pRightData = 0;
    pLeftData = 0;
    pWepHand = 0;
    m_pFace = 0;
    m_pArmR = 0;
    m_pArmL = 0;
    m_pHandR = 0;
    m_pHandL = 0;
    m_WaistY = 0.0f;
}

// Per frame: applies the waist twist.
void cPlBody::move()
{
    waistMove();
}

// Sets the waist twist (radians) the next waistMove applies — aiming turns the upper body.
void cPlBody::waistSet(f32 y)
{
    m_WaistY = y;
}

// Splits m_WaistY over the spine parts 1 and 2 (half each, flags 0x40000000 = extra rotation) and
// counter-rotates parts 3 (the hips) so the legs keep their direction.
void cPlBody::waistMove()
{
    cParts* p;
    f32 half = m_WaistY * 0.5f;

    p = m_pMod->getPartsPtr(1);
    p->motParts.flags |= 0x40000000;
    p->inv_offset.y = half;
    p->inv_offset.x = 0.0f;
    p->inv_offset.z = 0.0f;

    p = m_pMod->getPartsPtr(2);
    p->motParts.flags |= 0x40000000;
    p->inv_offset.y = half;
    p->inv_offset.x = 0.0f;
    p->inv_offset.z = 0.0f;

    p = m_pMod->getPartsPtr(3);
    p->motParts.flags |= 0x40000000;
    p->inv_offset.x = 0.0f;
    p->inv_offset.y = -m_WaistY;
    p->inv_offset.z = 0.0f;
}

// Builds the two face-morph (SPAE) records: shape 0 fades 1 -> 0 and shape 1 fades 0 -> 1 over
// 256 frames, used for the blink / mouth morphs.
void cPlBody::makeSpaeData()
{
    SpaeData* d = spae;
    u32 i;

    for (i = 0; i < 2; i++) {
        d->head.max_frame = 0x101;
        d->head.tbl_num = 2;
        d->tbl[0].offset = 0x18;
        d->tbl[0].shape_id = 0;
        d->tbl[0].key_num = 2;
        d->tbl[1].offset = 0x38;
        d->tbl[1].shape_id = 1;
        d->tbl[1].key_num = 2;
        d->mot[0].frame = 0;
        d->mot[0].value = 1.0f;
        d->mot[0].r_value = 0.0f;
        d->mot[0].l_value = 0.0f;
        d->mot[1].frame = 0x100;
        d->mot[1].value = 0.0f;
        d->mot[1].r_value = 0.0f;
        d->mot[1].l_value = 0.0f;
        d->mot[2].frame = 0;
        d->mot[2].value = 0.0f;
        d->mot[2].r_value = 0.0f;
        d->mot[2].l_value = 0.0f;
        d->mot[3].frame = 0x100;
        d->mot[3].value = 1.0f;
        d->mot[3].r_value = 0.0f;
        d->mot[3].l_value = 0.0f;
        d++;
    }
}

// Remembers the weapon-hand model data the weapon module supplies (setRightHand(1) uses it).
void cPlBody::initWepHand(u32 addr)
{
    // Unused; local static consts are still emitted (trailing 0, PI/2, 256 in .rodata).
    static const f32 hand_tbl[3] = {0.0f, PI * 0.5f, 256.0f};
    pWepHand = (void*) addr;
}
