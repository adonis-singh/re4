// game/pl_push: cPlPush, the player's push-object control (routine 0/9, pl_R1_ObjPush): catches a
// pushable cEmRack (id 0x45) the player walks into, remembers which side he stands on (m_Dir), and
// each push frame plays the object's motion facing that way while checking that no wall, enemy or
// range limit blocks it; plAdjust turns the player to the object.

#include "pl_push.h"
#include "global.h"
#include "atari.h"
#include "db_log.h"
#include "math_sub.h"
#include "motion.h"
#include "at_mod.h"
#include "em_sub.h"

// Looks for a pushable object (cEmRack, id 0x45, not type 4, alive, within 500 in height, not
// behind a wall) touching the player moved 300 forward; remembers it as m_Target with the side the
// player stands on (m_Dir 0..3 in the object's frame). Returns 1 when one was caught.
int cPlPush::catchCheck()
{
    cPlayer* pl = pPl;
    Vec bak;
    Vec pos;
    Vec rot;
    static const Vec sp = {0.0f, 0.0f, 300.0f};
    u32 i;

    bak = pl->pos;
    AddSpeed(pl, &sp);
    m_Target = 0;
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        if (!em->isAlive()) {
            continue;
        }
        if (em->id != 0x45) {
            continue;
        }
        if (em->type == 4) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (fabsf(pPl->pos.y - em->pos.y) > 500.0f) {
            continue;
        }
        pos.x = em->pos.x;
        pos.y = em->pos.y + 300.0f;
        pos.z = em->pos.z;
        if (SatMgr.hitCheck(&pPl->getPartsPtr(0)->world, &pos, 0, 0, 0, 0x800) != 0) {
            continue;
        }
        if (At_em_rect_rect_ck(pPl, em)) {
            m_Target = em;
            break;
        }
    }
    pl->pos = bak;
    if (m_Target == 0) {
        return 0;
    }

    PSVECSubtract(&pl->pos, &m_Target->pos, &pos);
    rot.x = 0.0f;
    rot.y = -m_Target->ang.y;
    rot.z = 0.0f;
    RotVector(&pos, &rot, &pos);
    {
        f32 sx = m_Target->atari.m_radius;
        f32 sz = m_Target->atari.m_radius2;

        m_Dir = 4;
        if (pos.z < sz && pos.z > -sz) {
            if (pos.x > sx) {
                m_Dir = 3;
            } else if (pos.x < -sx) {
                m_Dir = 1;
            }
        }
        if (pos.x < sx && pos.x > -sx) {
            if (pos.z > sz) {
                m_Dir = 0;
            } else if (pos.z < -sz) {
                m_Dir = 2;
            }
        }
    }
    if (m_Dir == 4) {
        return 0;
    }
    x8 = 0;
    return 1;
}

// Starts the object's push motion (archive 0x59); flag bit0 = getWHY reads the sides rotated.
void cPlPush::pushTargetInit(u8 flag)
{
    PlArc* arc = pG->pPlayer;

    MotionSetCore(m_Target, &m_Target->Motion, PL_ARC_PTR(arc, 0x59), 0, 0, 5, 0);
    x9 = flag;
}

// One push frame: plays the object's motion facing the push direction (m_Dir), lets the rack
// clamp itself (adjustRange), runs its collision and the scroll / enemy-sandwich checks, rebuilds
// its matrices. Returns 1 (and stops the motion) when the object cannot move further.
int cPlPush::pushTarget()
{
    int ret;
    cModel* t;

    switch (m_Dir) {
    case 0:
        m_Target->ang.y += PI;
        break;
    case 1:
        m_Target->ang.y += PI * 0.5f;
        break;
    case 3:
        m_Target->ang.y += PI * 1.5f;
        break;
    case 2:
    case 4:
        break;
    }
    m_Target->ang.y = LIMIT_ANGLE(m_Target->ang.y);
    RotMatrix(m_Target->mat, &m_Target->ang);
    MotionMove(m_Target, 0);
    switch (m_Dir) {
    case 0:
        m_Target->ang.y -= PI;
        break;
    case 1:
        m_Target->ang.y -= PI * 0.5f;
        break;
    case 3:
        m_Target->ang.y -= PI * 1.5f;
        break;
    case 2:
    case 4:
        break;
    }
    ret = 0;
    m_Target->ang.y = LIMIT_ANGLE(m_Target->ang.y);
    if (((cEmRack*) m_Target)->adjustRange(m_Dir)) {
        ret = 1;
    }
    EmAtCheck(m_Target);
    if (scrHitCheck()) {
        ret = 1;
    }
    t = m_Target;
    RotMatrix(t->l_mat, &t->ang);
    TransMatrix(t->l_mat, &t->pos);
    ScaleMatrix(t->l_mat, &t->scale);
    PSMTXCopy(t->l_mat, t->mat);
    m_Target->partsWorldCalc();
    if (ret == 1) {
        m_Target->Motion.pMot = 0;
    }
    return ret;
}

// Stops the object's push motion.
void cPlPush::stopTarget()
{
    m_Target->Motion.pMot = 0;
}

// Half width / half depth of the object as seen from the push side and the world yaw of that side
// (from the collision radii, m_Dir; x9 bit0 rotates the sides by 180 degrees).
void cPlPush::getWHY(f32* w, f32* h, f32* dy)
{
    f32 sz = m_Target->atari.m_radius2;
    f32 sx = m_Target->atari.m_radius;
    u8 d;

    if (x9 & 1) {
        switch (m_Dir) {
        default:
            pLog->err(0, 0, "cPlPush::getWHY() DIR ERR %d", m_Dir);
        case 0:
            d = 2;
            break;
        case 1:
            d = 3;
            break;
        case 2:
            d = 0;
            break;
        case 3:
            d = 1;
            break;
        }
    } else {
        d = m_Dir;
    }
    switch (d) {
    case 0:
        *w = sx;
        *h = sz;
        *dy = m_Target->ang.y + PI;
        break;
    case 1:
        *w = sz;
        *h = sx;
        *dy = m_Target->ang.y + PI * 0.5f;
        break;
    case 2:
        *w = sx;
        *h = sz;
        *dy = m_Target->ang.y;
        break;
    case 3:
        *w = sz;
        *h = sx;
        *dy = m_Target->ang.y + PI * 1.5f;
        break;
    }
    *dy = LIMIT_ANGLE(*dy);
}

// 1 when the object is blocked: an enemy would be sandwiched (emSandCheck) or the scroll collision
// (0x800 lines) is hit on either side of its front.
int cPlPush::scrHitCheck()
{
    f32 w;
    f32 h;
    f32 y;
    int ret;

    getWHY(&w, &h, &y);
    if (emSandCheck(w, h, y, &m_Target->pos)) {
        return 1;
    }
    ret = 0;
    if (scrHitCheckSub(w, h, y, &m_Target->pos, 1.0f)) {
        ret = 1;
    }
    if (scrHitCheckSub(w, h, y, &m_Target->pos, -1.0f)) {
        ret = 1;
    }
    return ret;
}

// Wall test for one corner (`side` +1 / -1) of the object's front: a line across the front, one
// along the side and a floor probe; 1 when the scroll collision blocks it.
int cPlPush::scrHitCheckSub(f32 w, f32 h, f32 dy, Vec* pos, f32 sign)
{
    Vec v0;
    Vec v1;
    Vec v2;
    Vec hit;
    Vec rot;
    int ret;

    rot.x = 0.0f;
    rot.y = dy;
    rot.z = 0.0f;
    v0.x = -sign * w;
    v0.y = 100.0f;
    v0.z = h;
    RotVector(&v0, &rot, &v0);
    PSVECAdd(&v0, pos, &v0);
    v1.x = sign * w * 2.0f;
    v1.y = 0.0f;
    v1.z = 0.0f;
    RotVector(&v1, &rot, &v1);
    PSVECAdd(&v1, &v0, &v1);
    ret = SatMgr.hitCheck(&v0, &v1, &hit, 0, 0, 0x800);
    v2.x = 0.0f;
    v2.y = 0.0f;
    v2.z = h * 2.0f;
    RotVector(&v2, &rot, &v2);
    if (ret & 0x1000000) {
        PSVECSubtract(&v1, &v2, &v0);
        if (SatMgr.hitCheck(&v0, &v1, &hit, 0, 0, 0x800)) {
            PSVECSubtract(&hit, &v1, &v0);
            PSVECAdd(pos, &v0, pos);
            return 1;
        }
    }
    PSVECSubtract(&hit, &v2, &v2);
    if (SatMgr.hitCheck(&v2, &hit, &v1, 0, 0, 0x800)) {
        PSVECSubtract(&v1, &hit, &v2);
        PSVECAdd(pos, &v2, pos);
        return 1;
    }
    return 0;
}

// `const f32` locals: each takes a 4-byte frame slot and creates its pool entry at the declaration
// (800 before 300 before the 0.0 of rot.x) without emitting code, so `h + sand` is computed where it
// is used (after the first two calls) with only the `lis` hoisted into a callee-saved register.
int cPlPush::emSandCheck(f32 w, f32 h, f32 dy, Vec* pos)
{
    Vec v0;
    Vec v1;
    Vec rot;
    const f32 sand = 800.0f;
    const f32 base = 300.0f;
    f32 len;

    rot.x = 0.0f;
    rot.y = dy;
    rot.z = 0.0f;
    v0.x = w;
    v0.y = base;
    v0.z = 0.0f;
    RotVector(&v0, &rot, &v0);
    PSVECAdd(&v0, pos, &v0);
    v1.x = 0.0f;
    v1.y = 0.0f;
    v1.z = h + sand;
    RotVector(&v1, &rot, &v1);
    PSVECAdd(&v1, &v0, &v1);
    if (SatMgr.hitCheck(&v0, &v1, 0, 0, 0, 0) == 0) {
        v0.x = -w;
        v0.y = base;
        v0.z = 0.0f;
        RotVector(&v0, &rot, &v0);
        PSVECAdd(&v0, pos, &v0);
        v1.x = 0.0f;
        v1.y = 0.0f;
        v1.z = h + sand;
        RotVector(&v1, &rot, &v1);
        PSVECAdd(&v1, &v0, &v1);
        if (SatMgr.hitCheck(&v0, &v1, 0, 0, 0, 0) == 0) {
            return 0;
        }
    }
    len = h + 400.0f;
    v0.x = w;
    v0.y = 300.0f;
    v0.z = len;
    RotVector(&v0, &rot, &v0);
    PSVECAdd(&v0, pos, &v0);
    v1.x = -w;
    v1.y = 300.0f;
    v1.z = len;
    RotVector(&v1, &rot, &v1);
    PSVECAdd(&v1, pos, &v1);
    return GetWepTargetPos(&v0, &v1, 0, 0, 0, 0) == 2;
}

// Turns the player toward the pushed side of the object (15 degrees / frame at most); 0 when there
// is no target or the side is invalid.
int cPlPush::plAdjust()
{
    cEm* t = m_Target;
    f32 ang;

    if (t == 0) {
        return 0;
    }
    switch (m_Dir) {
    case 0:
        ang = t->ang.y + PI;
        break;
    case 1:
        ang = t->ang.y + PI * 0.5f;
        break;
    case 2:
        ang = t->ang.y;
        break;
    case 3:
        ang = t->ang.y + PI * 1.5f;
        break;
    default:
        pLog->err(0, 0, "cPlPush::plAdjust() DIR ERR %d", m_Dir);
        return 0;
    }
    pPl->ang.y += Muku2(pPl->ang.y, ang, PI / 12.0f);
    return 1;
}

// the split object's .rodata is 8-aligned (0xE0, the pool ends at 0xDC)
asm(".section .rodata; .balign 8");
