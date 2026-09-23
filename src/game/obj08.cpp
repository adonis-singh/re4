// game/obj08: object id 8, enemy-thrown object (D:/Bio4/Prog/obj08.cpp): the bottles, dynamite,
// axes and other projectiles the Ganados throw. Flies under gravity with an optional spin, hits
// the scenario (EatMgr), other enemies (GetWepTargetList box sweep, flags sign bit) and the player
// / partner (EmAtkHitCk with the thrower's attack record, flags 0x40000000); the four est pairs
// set by SetObj08Est give the trail, break, floor and character-hit effects.
#include "atari.h"
#include "light.h"
#include "obj.h"
#include "obj08.h"
#include "em.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "pad.h"
#include "dbmodule.h"
#include "player.h"
#include "motion.h"
#include "em_sub.h"
#include "pl_npc.h"


extern "C" {
void obj08AddSpeed(cObj08* obj);
int obj08ScrHitCk(cObj08* obj);
int obj08ToEmHitCk(cObj08* obj);
int obj08ToPlHitCk(cObj08* obj);
void obj08DmEstSet(cObj08* obj, cModel* em, Vec* oldPos, YARARE_INFO* part);
}

Vec obj08HitBox[8] = {
    { -500.0f, -500.0f, 0.0f },   { 500.0f, -500.0f, 0.0f },
    { -500.0f, -500.0f, 500.0f }, { 500.0f, -500.0f, 500.0f },
    { -500.0f, 500.0f, 0.0f },    { 500.0f, 500.0f, 0.0f },
    { -500.0f, 500.0f, 500.0f },  { 500.0f, 500.0f, 500.0f },
};

// Creates the projectile for thrower `parent` at pos/rot (bin NULL = the invisible dummy model):
// flags sign bit enables enemy hits (low 16 bits = the GetWepTargetList attribute mask),
// 0x40000000 enables the player hit with attack record `atk`. Speed comes from SetObj08Spd.
cObj* SetObj08(cModel* parent, void* bin, void* tpl, Vec* pos, Vec* rot, int flags, void* atk)
{
    cObj* obj;
    Obj08Work* w;

    obj = ObjMgr.create(cObjMgr::ID_MISSILE);
    if (obj == 0) {
        return 0;
    }
    w = OBJ08_WK((cObj08*) obj);
    obj->id = 8;
    if (bin == 0) {
        if (obj->modelInit((void*) (pG->pCore->ofs_20 + (u32) pG->pCore),
                           (void*) (pG->pCore->ofs_24 + (u32) pG->pCore)) == 0) {
            ObjMgr.destroy(obj);
            return 0;
        }
        obj->be_flag &= ~2;
    } else {
        if (obj->modelInit(bin, tpl) == 0) {
            ObjMgr.destroy(obj);
            return 0;
        }
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->atari.off();
    obj->LightInfo.init2(0, 1, &p0, &p1, 4);
    w->pEm = parent;
    obj->pos = *pos;
    obj->pos_old = *pos;
    obj->ang = *rot;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->gravity = 0.0f;
    w->be_flag = 0;
    w->r = 500.0f;
    w->timer = -1;
    w->blk_no = 0xFFFF;
    w->call_no = 0xFFFF;
    w->hit_type = 0;
    if (flags < 0) {
        w->be_flag = 0x10;
    }
    if (flags & 0x40000000) {
        w->be_flag |= 0x20;
    }
    w->pAtk = (EmAtkInfo*) atk;
    w->wep_id = flags & 0xFFFF;
    return obj;
}

// Sets speed, life in frames (-1 = until it hits), gravity per frame and hit radius (min 1).
void SetObj08Spd(cObj* obj, Vec* spd, int life, f32 grav, f32 rad)
{
    Obj08Work* w;

    if (obj == 0) {
        return;
    }
    if (!obj->isAlive()) {
        return;
    }
    if (obj->id != 8) {
        return;
    }
    w = OBJ08_WK((cObj08*) obj);
    w->spd = *spd;
    w->gravity = grav;
    w->timer = life;
    w->r = rad;
    if (w->r < 1.0f) {
        w->r = 1.0f;
    }
}

// Sets the four est (owner, id) pairs: [1] break/expire, [2] floor landing (normal.y > 0.7), [3]
// character hit; `flag` (hit_type) attaches the hit effect to the victim instead of the surface.
void SetObj08Est(cObj* obj, int no0, int prm0, int no1, int prm1, int no2, int prm2, int no3, int prm3, u8 flag)
{
    Obj08Work* w;

    if (obj == 0) {
        return;
    }
    if (!obj->isAlive()) {
        return;
    }
    if (obj->id != 8) {
        return;
    }
    w = OBJ08_WK((cObj08*) obj);
    w->eff1 = no0;
    w->eff2 = no1;
    w->eff3 = no2;
    w->eff4 = no3;
    w->est1 = prm0;
    w->est2 = prm1;
    w->est3 = prm2;
    w->est4 = prm3;
    w->hit_type = flag;
}

// Sets the impact sound (block, number), played with the thrower's id.
void SetObj08Se(cObj* obj, u16 blk, u16 no)
{
    Obj08Work* w;

    if (obj == 0) {
        return;
    }
    if (!obj->isAlive()) {
        return;
    }
    if (obj->id != 8) {
        return;
    }
    w = OBJ08_WK((cObj08*) obj);
    w->blk_no = blk;
    w->call_no = no;
}

// Per-frame: life countdown (expire effect [1] on 0), motion start/advance, gravity move, then the
// enemy hit, player hit and scenario hit checks (the latter destroys the object); spins when
// be_flag bit 3.
void cObj08::move()
{
    Obj08Work* w = OBJ08_WK(this);

    if (w->timer == 0) {
        if (w->eff2 && w->est2) {
            EstSet(0, -1, &pos, &ang, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        ObjMgr.destroy(this);
        return;
    }
    w->timer--;
    if (w->be_flag & 1) {
        MotionSetCore(this, &Motion, w->pMot, 0, 0, w->motPrm, 0);
        w->be_flag = (w->be_flag & ~1) | 2;
    }
    if (w->be_flag & 2) {
        MotionMove(this, 0);
    }
    obj08AddSpeed(this);
    obj08ToEmHitCk(this);
    obj08ToPlHitCk(this);
    if (obj08ScrHitCk(this)) {
        return;
    }
    if (w->be_flag & 8) {
        PSVECAdd(&ang, &w->rot_spd, &ang);
        ang.x = LIMIT_ANGLE(ang.x);
        ang.y = LIMIT_ANGLE(ang.y);
        ang.z = LIMIT_ANGLE(ang.z);
    }
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    partsMatCalc();
    partsWorldCalc();
}

// Gravity + move.
void obj08AddSpeed(cObj08* pObj)
{
    Obj08Work* w = OBJ08_WK(pObj);

    w->spd.y -= w->gravity;
    PSVECAdd(&pObj->pos, &w->spd, &pObj->pos);
}

// Scenario sweep pos_old -> pos: on a hit plays the sound, spawns the floor effect [2] (on a
// horizontal surface) or the break effect [1] oriented by the normal, destroys the object; returns 1.
int obj08ScrHitCk(cObj08* pObj)
{
    Obj08Work* w = OBJ08_WK(pObj);
    Vec hit;
    Vec nrm;
    Vec est;

    if (EatMgr.hitCheck(&pObj->pos_old, &pObj->pos, &hit, &nrm, 0, 0x4000)) {
        if (w->blk_no != 0xFFFF) {
            int id = 0;
            if (w->pEm) {
                id = w->pEm->id;
            }
            SndCall(w->blk_no, w->call_no, &pObj->pos, id, 0, 0);
        }
        if (nrm.y > 0.7f) {
            if (w->eff3 && w->est3) {
                est.x = 0.0f;
                est.y = pObj->ang.y;
                est.z = 0.0f;
                hit.y += 10.0f;
                EstSet(0, -1, &hit, &est, w->eff3, (u8) w->est3, 0, ESP_CORE_KIND_NONE, 0, 0);
            }
        } else if (w->eff2 && w->est2) {
            f32 len = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
            est.x = -atan2f(-nrm.y, len);
            est.y = atan2f(-nrm.x, -nrm.z);
            est.z = 0.0f;
            EstSet(0, -1, &hit, &est, w->eff2, (u8) w->est2, 0, ESP_CORE_KIND_NONE, 0, 0);
        }
        ObjMgr.destroy(pObj);
        return 1;
    }
    return 0;
}

// Enemy hit (once, be_flag 0x10): sweeps a box of radius r along the frame's movement through
// GetWepTargetList (up to 3 targets) and damages each (dmg.set kind 0, power 10, the attribute
// mask), spawning the hit effect. Returns 1 on a hit.
int obj08ToEmHitCk(cObj08* pObj)
{
    Obj08Work* w = OBJ08_WK(pObj);
    Vec box[8];
    WepTarget list[10];
    Vec ang;
    f32 len;
    u32 n;
    u32 i;

    if (!(w->be_flag & 0x10)) {
        return 0;
    }
    if (w->wep_id == 0) {
        return 0;
    }
    ang.x = 0.0f;
    ang.y = 0.0f;
    ang.z = 0.0f;
    len = GetDistance3(&pObj->pos, &pObj->pos_old);
    if (len < 1.0f) {
        len = w->r;
    } else {
        Vec d;
        PSVECSubtract(&pObj->pos, &pObj->pos_old, &d);
        ang.x = -atan2f(d.y, len);
        ang.y = atan2f(d.x, d.z);
    }
    if (len < w->r) {
        len = w->r;
    }
    obj08HitBox[0].x = -w->r;
    obj08HitBox[1].x = w->r;
    obj08HitBox[2].x = -w->r;
    obj08HitBox[3].x = w->r;
    obj08HitBox[4].x = -w->r;
    obj08HitBox[5].x = w->r;
    obj08HitBox[6].x = -w->r;
    obj08HitBox[7].x = w->r;
    obj08HitBox[0].y = -w->r;
    obj08HitBox[1].y = -w->r;
    obj08HitBox[2].y = -w->r;
    obj08HitBox[3].y = -w->r;
    obj08HitBox[4].y = w->r;
    obj08HitBox[5].y = w->r;
    obj08HitBox[6].y = w->r;
    obj08HitBox[7].y = w->r;
    obj08HitBox[2].z = len;
    obj08HitBox[3].z = len;
    obj08HitBox[6].z = len;
    obj08HitBox[7].z = len;
    BoxWorldCalc(obj08HitBox, box, &pObj->pos_old, &ang);
    if (DbgFlagChk(pG, DBG_YARARE_DISP)) {
        Draw_box(box, 0x20FFFFFF, 0);
    }
    n = GetWepTargetList(box, &pObj->pos, list, 3, (u16) w->wep_id);
    if (n == 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        YARARE_INFO* part = list[i].part;
        list[i].em->dmg.set(0, 10, (u8) w->wep_id, &pObj->pos, part->len, part);
        if (w->eff4 && w->est4) {
            obj08DmEstSet(pObj, pPL, &pObj->pos_old, part);
        }
    }
    w->be_flag &= ~0x10;
    return 1;
}

// Player / partner hit (once, be_flag 0x20) through the thrower's attack record: hit effect on the
// victim's hit info, sound, controller vibration. Returns 1 on a hit.
int obj08ToPlHitCk(cObj08* pObj)
{
    Obj08Work* w = OBJ08_WK(pObj);
    int hit;

    if (w->pEm == 0) {
        return 0;
    }
    if (w->pAtk && (w->be_flag & 0x20)) {
        hit = EmAtkHitCk(w->pAtk, &pObj->pos, &pObj->pos_old, 0);
        if (hit) {
            if (w->eff4 && w->est4) {
                if (hit & 1) {
                    obj08DmEstSet(pObj, pPL, &pObj->pos_old, &pPL->hitInfo);
                }
                if (hit & 2) {
                    if (pSUB) {
                        obj08DmEstSet(pObj, pSUB, &pObj->pos_old, &((cEm*) pSUB)->hitInfo);
                    }
                }
            } else {
                if (w->blk_no != 0xFFFF) {
                    int id = 0;
                    if (w->pEm) {
                        id = w->pEm->id;
                    }
                    SndCall(w->blk_no, w->call_no, &pObj->pos, id, 0, 0);
                }
                VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 7, 1);
            }
            w->be_flag &= ~0x20;
            return 1;
        }
    }
    return 0;
}

// Spawns the character-hit effect [3]: attached to the victim when hit_type, otherwise on the
// surface of the hit parts (facing the projectile, clamped to 70% of the parts height), plus the sound.
void obj08DmEstSet(cObj08* pObj, cModel* pEm, Vec* pPos, YARARE_INFO* pAt)
{
    Obj08Work* w = OBJ08_WK(pObj);
    Mtx m;
    Vec p;
    Vec o;
    Vec rot;
    f32 dy;
    f32 lim;

    if (w->blk_no != 0xFFFF) {
        int id = 0;
        if (w->pEm) {
            id = w->pEm->id;
        }
        SndCall(w->blk_no, w->call_no, &pObj->pos, id, 0, 0);
    }
    if (w->hit_type) {
        EstSet(pEm, -1, 0, 0, w->eff4, (u8) w->est4, 0, ESP_CORE_KIND_NONE, pEm, 0);
        return;
    }
    if (pAt->parts_no != 0) {
        p = pEm->getPartsPtr(pAt->parts_no - 1)->world;
    } else {
        p = pEm->pos;
    }
    dy = pPos->y - p.y;
    lim = pAt->height * 0.7f;
    if (dy > lim) {
        dy = lim;
    }
    if (dy < -lim) {
        dy = -lim;
    }
    rot.x = 0.0f;
    rot.y = GetXZAngle(&p, pPos);
    rot.z = 0.0f;
    RotMatrix(m, &rot);
    TransMatrix(m, &p);
    o.x = 0.0f;
    o.y = dy;
    o.z = pAt->radius * 0.5f;
    PSMTXMultVec(m, &o, &o);
    EstSet(0, -1, &o, &rot, w->eff4, (u8) w->est4, 0, ESP_CORE_KIND_NONE, 0, 0);
}
