// game/ctrl14.cpp: control 0x14, the dragon head statues of the stage 4 fire trap room: the
// scroll models of base, head and jaws of dragon `type` (0..2) with their collision pieces, moved
// along the rail (addWidth / addHeight) and turned (addDir) by the em10 ganados that operate
// them, and their flame (setFire: 30 frame wind-up, 60 frame jet) that burns the player.

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "atari.h"
#include "light.h"
#include "global.h"
#include "model.h"
#include "player.h"
#include "scroll.h"
#include "esp.h"
#include "snd.h"
#include "math_sub.h"
#include "est.h"

// Creates the control for dragon `type` from the room's scroll objects (base / head / jaws, ids
// 0xA.., 0xF.., 0x14..), gives it an effect Core_kind and three collision pieces from room
// collision file 5. NULL when the objects or a work are missing.
cCtrl* GetCtrlDragon(u32 type)
{
    cModel* obj[5];
    Vec pos;
    Vec rot;
    cCtrl* c;
    Ctrl14Work* w;
    u32 no;

    switch (type) {
    case 0:
        obj[0] = SmdGetObjPtr(0xA);
        obj[1] = SmdGetObjPtr(0xB);
        obj[3] = SmdGetObjPtr(0xD);
        no = 0xE;
        break;
    case 1:
        obj[0] = SmdGetObjPtr(0xF);
        obj[1] = SmdGetObjPtr(0x10);
        obj[3] = SmdGetObjPtr(0x12);
        no = 0x13;
        break;
    case 2:
        obj[0] = SmdGetObjPtr(0x14);
        obj[1] = SmdGetObjPtr(0x15);
        obj[3] = SmdGetObjPtr(0x16);
        no = 0x17;
        break;
    default:
        goto create;
    }
    obj[4] = SmdGetObjPtr(no);
    if (obj[0] == NULL || obj[1] == NULL || obj[3] == NULL || obj[4] == NULL) {
        return NULL;
    }
    obj[0]->setMove(1);
    obj[1]->setMove(1);
    obj[3]->setMove(1);
    obj[4]->setMove(1);
create:
    c = CtrlMgr.createBack(0x14);
    if (c == NULL) {
        return NULL;
    }
    w = (Ctrl14Work*) c->work;
    w->obj[0] = obj[0];
    w->obj[1] = obj[1];
    w->obj[3] = obj[3];
    w->obj[4] = obj[4];
    w->type = type;
    w->espKind = EspPullCoreKind();
    pos = w->obj[1]->pos;
    rot = w->obj[1]->ang;
    if (obj[0]) {
        w->sat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 1);
    }
    w->sat[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 2);
    // pG: the original loads pG after the sat[1] store; a plain pG here is hoisted above the argument setup
    w->sat[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &pos, &rot, 3);
    return c;
}

// Per-frame: the flame wind-up / jet timers (est 1/2 jet start, 1/4 jet end, SEs), keeps the
// collision pieces on the head, and plays the start / stop movement SEs (per dragon) when the
// moving flag (bit1, set by addWidth / addHeight this frame) changes.
void cCtrl14::move()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    Vec pos;
    Vec rot;
    u8 flags;

    if (w->fireDelay) {
        w->fireDelay--;
        if (w->fireDelay == 0) {
            EstSet(w->obj[1], -1, NULL, NULL, EFF_ROOM, 2, 1, w->espKind, w->obj[1], NULL);
            SndCall(6, 2, &w->obj[1]->pos, 0, 0, w->obj[1]);
            w->fireTimer = 60;
        }
    }
    if (w->fireTimer) {
        w->fireTimer--;
        if (w->fireTimer == 0) {
            EffectEspgenDelete(0, w->espKind, w->obj[1]);
            EstSet(w->obj[1], -1, NULL, NULL, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, w->obj[1], NULL);
        }
    }
    pos = w->obj[1]->getPartsPtr(0)->world;
    rot = w->obj[1]->ang;
    if (w->obj[0]) {
        w->sat[0]->setCoord(&pos, &rot);
    }
    w->sat[1]->setCoord(&pos, &rot);
    w->sat[2]->setCoord(&pos, &rot);
    flags = w->flags;
    if (flags & 1) {
        if (!(flags & 2)) {
            w->flags = flags & ~1;
            switch (w->type) {
            case 0:
                SndCall(6, 8, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 1:
                SndCall(6, 0xA, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 2:
                SndCall(6, 0xC, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            }
        }
    } else {
        if (flags & 2) {
            w->flags = flags | 1;
            switch (w->type) {
            case 0:
                SndCall(6, 7, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 1:
                SndCall(6, 9, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 2:
                SndCall(6, 0xB, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            }
        }
    }
    w->flags &= ~2;
}

// Matrix of piece `idx` (0 base, 1 head, ...).
void cCtrl14::getBaseMtx(Mtx m, int type)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[type];

    if (o) {
        PSMTXCopy(o->mat, m);
    }
}

// The head's position.
void cCtrl14::getPos(Vec* pPos)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o) {
        *pPos = o->pos;
    }
}

// The head's yaw.
f32 cCtrl14::getDir()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o == NULL) {
        return 0.0f;
    }
    return o->ang.y;
}

// The head's yaw relative to the base (how far it is turned).
f32 cCtrl14::getDir2()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    f32 ret = 0.0f;

    if (w->obj[1] && w->obj[0]) {
        ret = Muku2(w->obj[0]->ang.y, w->obj[1]->ang.y, PI);
    } else {
        ret = 0.0f;
    }
    return ret;
}

// Slides the whole dragon by `x` along the base's local x, clamped to the rail limits of its
// type (type 2 does not slide); marks it moving.
void cCtrl14::addWidth(f32 add)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[0];
    Vec v;
    Vec p;

    if (o) {
        v.x = add;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVecSR(o->mat, &v, &v);
        PSVECAdd(&w->obj[0]->pos, &v, &p);
        switch (w->type) {
        case 0:
            if (p.x > -6300.0f) {
                p.x = -6300.0f;
            }
            if (p.x < -65600.0f) {
                p.x = -65600.0f;
            }
            break;
        case 1:
            if (p.x > -32000.0f) {
                p.x = -32000.0f;
            }
            if (p.x < -65600.0f) {
                p.x = -65600.0f;
            }
            break;
        case 2:
            return;
        }
        PSVECSubtract(&p, &w->obj[0]->pos, &v);
        v.y = 0.0f;
        if (w->obj[0]) {
            PSVECAdd(&w->obj[0]->pos, &v, &w->obj[0]->pos);
        }
        if (w->obj[1]) {
            PSVECAdd(&w->obj[1]->pos, &v, &w->obj[1]->pos);
        }
        if (w->obj[3]) {
            PSVECAdd(&w->obj[3]->pos, &v, &w->obj[3]->pos);
        }
        if (w->obj[4]) {
            PSVECAdd(&w->obj[4]->pos, &v, &w->obj[4]->pos);
        }
    }
}

// Raises / lowers the whole dragon by `y` (upper limits per type); marks it moving.
void cCtrl14::addHeight(f32 add)
{
    Ctrl14Work* w = (Ctrl14Work*) work;

    if (w->type == 2) {
        if (w->obj[0]->pos.y > 30000.0f && add > 0.0f) {
            return;
        }
    } else {
        if (w->obj[0]->pos.y > 5000.0f && add > 0.0f) {
            return;
        }
    }
    if (w->obj[0]) {
        w->obj[0]->pos.y += add;
    }
    if (w->obj[1]) {
        w->obj[1]->pos.y += add;
    }
    if (w->obj[3]) {
        w->obj[3]->pos.y += add;
    }
    if (w->obj[4]) {
        w->obj[4]->pos.y += add;
    }
    add = fabsf(add);
    if (add > 1.0f) {
        w->flags |= 2;
    }
}

// Turns the base by `add` radians; the head follows within +-45 degrees of the base.
void cCtrl14::addDir(f32 add)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o) {
        o->ang.y += add;
        if (w->obj[0]) {
            w->obj[1]->ang.y = w->obj[0]->ang.y + Muku2(w->obj[0]->ang.y, w->obj[1]->ang.y, PI / 4.0f);
            w->obj[1]->ang.y = LIMIT_ANGLE(w->obj[1]->ang.y);
        }
    }
}

// Sets the head's yaw relative to the base.
void cCtrl14::setDir(f32 dir)
{
    Ctrl14Work* w = (Ctrl14Work*) work;

    if (w->obj[1] && w->obj[0]) {
        w->obj[1]->ang.y = w->obj[0]->ang.y + dir;
        w->obj[1]->ang.y = LIMIT_ANGLE(w->obj[1]->ang.y);
    }
}

// Eases the head back to the base's direction (0.35 degrees per frame).
void cCtrl14::resetDir()
{
    Ctrl14Work* w = (Ctrl14Work*) work;

    if (w->obj[1]) {
        w->obj[1]->ang.y += Muku2(w->obj[1]->ang.y, w->obj[0]->ang.y, 0.0061359233f);
        w->obj[1]->ang.y = LIMIT_ANGLE(w->obj[1]->ang.y);
    }
}

// Starts a flame: cancels a running jet, 30 frame wind-up est (1/3) with the SE, then the jet.
void cCtrl14::setFire()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o) {
        if (w->fireTimer) {
            w->fireTimer = 0;
            EffectEspgenDelete(0, w->espKind, o);
            EstSet(w->obj[1], -1, NULL, NULL, EFF_ROOM, 4, 1, ESP_CORE_KIND_NONE, w->obj[1], NULL);
        }
        w->fireDelay = 30;
        EstSet(w->obj[1], -1, NULL, NULL, EFF_ROOM, 3, 1, w->espKind, w->obj[1], NULL);
        SndCall(6, 1, &w->obj[1]->pos, 0, 0, w->obj[1]);
    }
}

// 1 when the jet is burning and `p` lies in the flame box in head space (5000..15000 ahead,
// +-1500 wide, +-5000 high).
int cCtrl14::ckHitFire(Vec* pPos)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];
    Mtx inv;
    Vec v;

    if (o == NULL) {
        return 0;
    }
    if (w->fireTimer == 0) {
        return 0;
    }
    PSMTXInverse(o->mat, inv);
    PSMTXMultVec(inv, pPos, &v);
    if (v.z < 5000.0f) {
        return 0;
    }
    if (v.z > 15000.0f) {
        return 0;
    }
    if (v.x < -1500.0f) {
        return 0;
    }
    if (v.x > 1500.0f) {
        return 0;
    }
    if (v.y < -5000.0f) {
        return 0;
    }
    if (v.y > 5000.0f) {
        return 0;
    }
    return 1;
}

// 1 when a wall (effect collision, attribute 0x400000) stands between the head and the player
// at chest height, so the flame does not reach him.
int cCtrl14::ckHitFireBlocked()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    Vec a;
    Vec b;

    if (w->obj[1]) {
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = -2000.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->obj[1]->mat, &a, &a);
        PSMTXMultVec(w->obj[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        if (EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000)) {
            return 0;
        }
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->obj[1]->mat, &a, &a);
        PSMTXMultVec(w->obj[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        if (EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000)) {
            return 0;
        }
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = 2000.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->obj[1]->mat, &a, &a);
        PSMTXMultVec(w->obj[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        return EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000) == 0;
    }
    return 0;
}
