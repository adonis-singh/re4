// game/em_sub.cpp: the shared enemy helper library used by the enemy modules: damage position and
// blood effects, hit box (yarare) checks against boxes, lines and spheres, the weapon target
// lists, life and damage entry points for the player and the partner, the catch (grab) motion
// helpers, and the item drops.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "em_sub.h"
#include "emrack.h"
#include "atariInfo.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "esp.h"
#include "est.h"
#include "item.h"
#include "sce_at.h"
#include "obj.h"
#include "game.h"
#include "pad.h"
#include "dbmodule.h"
#include "main.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);   // motion.cpp
}

// EstSet with the two effect parameter bytes as u8 (the original prototype): an `int` passed to
// them is masked at the call (`clrlwi 24`, EmDmBloodSet2/3).
void EstSetB(int a, int b, Vec* pos, Vec* rot, int c, u8 d, int e, u8 f, u32 g, void* h) asm("EstSet");
// AtSphereCapsuleCk with the float arguments declared first (atari_init.h idiom): the original
// issues the `fmr` argument moves before the `addi r4` of the second point (emSphereAtCk).
u32 AtSphereCapsuleCkF(Vec* c, f32 r, f32 r2, Vec* p0, Vec* p1) asm("AtSphereCapsuleCk");

// The vehicle objects (objTrolley.cpp / objBull.cpp) as seen from here: the ride checks only.
class cObjTrolley : public cObj {
public:
    int ckTrolleyRide(Vec* pos, u8* partsNo, Vec* out);
    int ckTrolleyRideAdjust(Vec* pos, Vec* out);
};

class cObjBull : public cObj {
public:
    int ckBullRide(Vec* pos, u8* partsNo, Vec* out);
    int ckBullRideAdjust(Vec* pos, Vec* out);
};

// Entry `n` of a target list written index first: the sum is formed with the index as the base
// register (`add r9, r9, r31` / `stwx r29, r9, r31`) instead of the pointer.
#define WEP_LIST(n) ((WepTarget*) ((n) * sizeof(WepTarget) + (u32) list))

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Position offset by the trolley / bulldozer movement (adjust_add_set / VehicleAdjust).
static Vec adjust_add = {0.0f, 0.0f, 0.0f};

u32 No_drop_cnt = 0;
u32 No_drop_cnt2 = 0;

// Dead (upper 16 bits of cDmgInfo::flags set): the `li 1; andis.; bne; li 0; cmpwi` chain.
static inline int EmIsDead(cEm* em)
{
    return em->dmg.m_Flag || em->dmg.m_Timer;
}

// Player life at least `lim`: the compare keeps its `>=` form (`cmpwi 0x1f5; cror un,eq,gt`) because
// the constant only arrives at RTL inlining time, after fold's `>= C` -> `> C-1` rewrite.
static inline int PlLifeOver(int lim)
{
    return (s16) pG->pl_life >= lim;
}

// Pointer store through a scalar reference (the FSet mechanism, global.h): a following `pPL` load
// is not hoisted above / shared across it.
static inline void PSet(YARARE_INFO*& d, YARARE_INFO* v)
{
    d = v;
}

static inline void PSet(cEm*& d, cEm* v)
{
    d = v;
}

// Reference store helpers (see PSet above): keep the store after preceding loads in the target order.
static inline void ISet(int& d, int v)
{
    d = v;
}

// u16 store through a reference (same purpose as ISet).
static inline void HSet(u16& d, int v)
{
    d = v;
}

// `f &= mask` through a reference, same purpose (BitOff16 with its `~b` keeps a 32-bit mask).
static inline void MaskAnd16(u16& f, u16 mask)
{
    f &= mask;
}

// Struct-member view of pPL (the pGS trick, global.h): loads through it stay after preceding
// stores through other pointers instead of being shared across them.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// The parts a hit box belongs to (partsNo is 1-based, 0 = the model itself).
static inline cModel* HitParts(cEm* em, YARARE_INFO* p)
{
    if (p->partsNo != 0) {
        return em->getPartsPtr(p->partsNo - 1);
    }
    return em;
}

// Work `no` of the enemy manager through a local manager pointer (map_obj.h getWork): the range
// check survives at the top of the guarded do-while scans below (thread_jumps cannot fold it).
static inline cEm* emWork(u32 no)
{
    cEmMgr* m = &EmMgr;

    if (no >= m->nArray) {
        return 0;
    }
    return (cEm*) ((u8*) m->pArray + m->size * no);
}

// Shared Rno0 routine of the object classes (emdoor / emrack tables): the "scenario" state where an
// event script drives the object; just advances the current motion.
void Em_R0_Scenario(cEm* em)
{
    MotionMove(em, 0);
}

// Damage position / direction of the registered hit: the hit box centre line clamped to the box
// height along its axis, from the damage position (x328) mapped into the parts' space.
int EmGetDmPos(cEm* em, Vec* pos, Vec* dir)
{
    YARARE_INFO* p = em->dmg.m_pDamageYarare;
    u32 type;
    cModel* parts;
    Mtx m;
    Mtx inv;
    Vec d;
    Vec top;
    Vec bottom;
    Vec c;
    Vec v;
    Vec s;
    f32 mag;

    if (!VALID_PTR(p)) {
        return 0;
    }
    if (p->flags & 0x4000) {
        *pos = p->pos;
        dir->x = 0.0f;
        dir->y = GetXZAngle(pos, &em->dmg.m_PosFrom);
        dir->z = 0.0f;
        return 1;
    }
    if (p->flags & 6) {
        type = (p->flags & 2) ? 0 : 2;
    } else {
        type = 1;
    }
    parts = HitParts(em, p);
    bottom = p->ofs;
    top = p->ofs;
    switch (type) {
    case 0:
        top.x += p->height;
        break;
    case 1:
    default:
        top.y += p->height;
        break;
    case 2:
        top.z += p->height;
        break;
    }
    PSMTXMultVec(parts->mat, &top, &top);
    PSMTXMultVec(parts->mat, &bottom, &bottom);
    PSVECAdd(&top, &bottom, &c);
    PSVECScale(&c, &c, 0.5f);
    s.x = 0.0f;
    s.y = 0.0f;
    s.z = p->width;
    PSMTXMultVecSR(parts->mat, &s, &s);
    mag = PSVECMag(&s);
    PSMTXCopy(parts->mat, m);
    TransMatrix(m, &bottom);
    if (PSMTXInverse(m, inv) == 0) {
        PSMTXIdentity(inv);
    }
    PSMTXMultVec(inv, &em->dmg.m_PosFrom, &v);
    switch (type) {
    case 0:
        d.x = 0.0f;
        d.y = v.y;
        d.z = v.z;
        if (v.y == 0.0f && v.z == 0.0f) {
            d.y = 1.0f;
        }
#line 164 "D:/Bio4/Prog/em_sub.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, mag);
        d.x = v.x;
        if (v.x < 0.0f) {
            d.x = 0.0f;
        }
        if (v.x > p->height) {
            d.x = p->height;
        }
        PSMTXMultVec(m, &d, pos);
        break;
    case 1:
    default:
        d.x = v.x;
        d.y = 0.0f;
        d.z = v.z;
        if (v.x == 0.0f && v.z == 0.0f) {
            d.x = 1.0f;
        }
#line 181
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, mag);
        d.y = v.y;
        if (v.y < 0.0f) {
            d.y = 0.0f;
        }
        if (v.y > p->height) {
            d.y = p->height;
        }
        PSMTXMultVec(m, &d, pos);
        break;
    case 2:
        d.x = v.x;
        d.y = v.y;
        d.z = 0.0f;
        if (v.x == 0.0f && v.y == 0.0f) {
            d.x = 1.0f;
        }
#line 197
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, mag);
        d.z = v.z;
        if (v.z < 0.0f) {
            d.z = 0.0f;
        }
        if (v.z > p->height) {
            d.z = p->height;
        }
        PSMTXMultVec(m, &d, pos);
        break;
    }
    dir->x = 0.0f;
    dir->y = GetXZAngle(&c, &em->dmg.m_PosFrom);
    dir->z = 0.0f;
    return 1;
}

// Pool of a helper the original linker dropped (0.0 / 0.5 / 1.0 after EmGetDmPos's pool).
static void emGetDmPosDead(Vec* v)
{
    v->x = 0.0f;
    v->y = 0.5f;
    v->z = 1.0f;
}

// Blood burst at the damage position: the small burst for blades / the shotgun family, three
// spread bursts for everything else.
void EmDmBloodSet(cEm* em)
{
    Vec pos;
    Vec dir;
    Vec p;

    if (EmGetDmPos(em, &pos, &dir) == 0) {
        return;
    }
    switch (em->dmg.m_Wep) {
    default:
        EstSet(0, -1, &pos, &dir, 0, 1, 0, 0, 0, 0);
        p.x = fRand1_1() * 200.0f + pos.x;
        p.y = fRand0_1() * 200.0f + (pos.y + 200.0f);
        p.z = fRand1_1() * 200.0f + pos.z;
        EstSet(0, -1, &p, &dir, 0, 2, 0, 0, 0, 0);
        p.x = fRand1_1() * 250.0f + pos.x;
        p.y = fRand0_1() * 150.0f + (pos.y - 150.0f);
        p.z = fRand1_1() * 150.0f + pos.z;
        EstSet(0, -1, &p, &dir, 0, 2, 0, 0, 0, 0);
        p.x = fRand1_1() * 150.0f + pos.x;
        p.y = fRand0_1() * 150.0f + (pos.y - 150.0f);
        p.z = fRand1_1() * 250.0f + pos.z;
        EstSet(0, -1, &p, &dir, 0, 2, 0, 0, 0, 0);
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        EstSet(0, -1, &pos, &dir, 0, 0, 0, 0, 0, 0);
        if (Rnd() & 1) {
            p.x = fRand1_1() * 150.0f + pos.x;
            p.y = fRand1_1() * 150.0f + pos.y;
            p.z = fRand1_1() * 150.0f + pos.z;
            EstSet(0, -1, &p, &dir, 0, 2, 0, 0, 0, 0);
        }
        if (Rnd() & 1) {
            p.x = fRand1_1() * 150.0f + pos.x;
            p.y = fRand1_1() * 150.0f + pos.y;
            p.z = fRand1_1() * 150.0f + pos.z;
            EstSet(0, -1, &p, &dir, 0, 2, 0, 0, 0, 0);
        }
        break;
    }
}

// Effect `no` at the damage position (scattered by 50 when `rnd` is set), owned by `em`.
void EmDmBloodSet2(cEm* em, int no, int prm, int rnd, int esp_core_flg, int f)
{
    Vec pos;
    Vec dir;

    if (EmGetDmPos(em, &pos, &dir) == 0) {
        return;
    }
    if (rnd) {
        pos.x = fRand1_1() * 50.0f + pos.x;
        pos.y = fRand1_1() * 50.0f + pos.y;
        pos.z = fRand1_1() * 50.0f + pos.z;
    }
    EstSetB(0, -1, &pos, &dir, no, prm, esp_core_flg, f, (u32) em, 0);
}

// EmDmBloodSet2 with the effect aligned to the enemy's rotation instead of the damage direction.
void EmDmBloodSet3(cEm* em, int no, int prm, int rnd, int esp_core_flg, int f)
{
    Vec pos;
    Vec dir;

    if (EmGetDmPos(em, &pos, &dir) == 0) {
        return;
    }
    if (rnd) {
        pos.x = fRand1_1() * 50.0f + pos.x;
        pos.y = fRand1_1() * 50.0f + pos.y;
        pos.z = fRand1_1() * 50.0f + pos.z;
    }
    EstSetB(0, -1, &pos, &em->ang, no, prm, esp_core_flg, f, (u32) em, 0);
}

// Blood on the player at the height of `pos` (clamped to the player's hit box), facing the attacker.
void EmPlBloodSet(cEm* em, Vec* pos, int type, int eff_id, int est_id)
{
    cPlayer* pl = pPL;
    YARARE_INFO* hit = &pl->hitInfo;
    Mtx m;
    Vec p;
    Vec q;
    Vec rot;
    Vec s;
    cModel* parts;
    f32 h;
    f32 mag;

    parts = pl->getPartsPtr(0);
    p = parts->world;
    h = pos->y - p.y;
    if (h > hit->height * 0.5f) {
        h = hit->height * 0.5f;
    }
    if (h < -(hit->height * 0.7f)) {
        h = -(hit->height * 0.7f);
    }
    s.x = 0.0f;
    s.y = 0.0f;
    s.z = hit->width;
    PSMTXMultVecSR(parts->mat, &s, &s);
    mag = PSVECMag(&s);
    rot.x = 0.0f;
    rot.y = GetXZAngle(&p, pos);
    rot.z = 0.0f;
    RotMatrix(m, &rot);
    TransMatrix(m, &p);
    q.x = 0.0f;
    q.z = mag * 0.5f;
    q.y = h;
    q.y = fRand1_1() * 50.0f + q.y;
    q.x = fRand1_1() * 50.0f + q.x;
    PSMTXMultVec(m, &q, &q);
    if (eff_id == 0xFF || est_id == 0xFF) {
        if (type != 1) {
            EstSet(0, -1, &q, &rot, 0, 0, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &q, &rot, 0, 1, 0, 0, 0, 0);
            EstSet(0, -1, &q, &rot, 0, 2, 0, 0, 0, 0);
        }
    } else {
        EstSet(0, -1, &q, &rot, eff_id, est_id, 0, 0, 0, 0);
    }
}

// Blood at the player's registered damage position.
void EmPlBloodSet2(cModel* m, Vec* p, int type, int eff_id, int est_id)
{
    Vec pos;
    Vec dir;

    if (EmGetDmPos(pPL, &pos, &dir) == 0) {
        return;
    }
    if (eff_id == 0xFF || est_id == 0xFF) {
        if (type != 1) {
            EstSet(0, -1, &pos, &dir, 0, 0, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &pos, &dir, 0, 1, 0, 0, 0, 0);
            EstSet(0, -1, &pos, &dir, 0, 2, 0, 0, 0, 0);
        }
    } else {
        EstSet(0, -1, &pos, &dir, eff_id, est_id, 0, 0, 0, 0);
    }
}

// EmPlBloodSet for the partner.
void EmSubBloodSet(cEm* em, Vec* pos, int type, int eff_id, int est_id)
{
    cSubChar* sub = pSUB;
    YARARE_INFO* hit;
    Mtx m;
    Vec p;
    Vec q;
    Vec rot;
    cModel* parts;
    f32 h;

    if (sub == 0) {
        return;
    }
    hit = &sub->hitInfo;
    parts = sub->getPartsPtr(0);
    PSMTXMultVec(parts->mat, &hit->ofs, &p);
    h = pos->y - p.y;
    if (h > hit->height * 0.7f) {
        h = hit->height * 0.7f;
    }
    if (h < -(hit->height * 0.7f)) {
        h = -(hit->height * 0.7f);
    }
    rot.x = 0.0f;
    rot.y = GetXZAngle(&p, pos);
    rot.z = 0.0f;
    RotMatrix(m, &rot);
    TransMatrix(m, &p);
    q.x = 0.0f;
    q.y = h;
    q.z = hit->width * 0.5f;
    PSMTXMultVec(m, &q, &q);
    if (eff_id == 0xFF || est_id == 0xFF) {
        if (type != 1) {
            EstSet(0, -1, &q, &rot, 0, 0, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &q, &rot, 0, 1, 0, 0, 0, 0);
            EstSet(0, -1, &q, &rot, 0, 2, 0, 0, 0, 0);
        }
    } else {
        EstSet(0, -1, &q, &rot, eff_id, est_id, 0, 0, 0, 0);
    }
}

// Hit boxes of `em` inside the capsule box (8 corners) of a melee weapon: the one nearest to the
// box axis, with rad = squared distance to `pos` and dist = squared distance from the axis.
YARARE_INFO* emBoxAtCk(cEm* em, Vec* box, Vec* pos, int flag)
{
    Mtx mat;
    Mtx rm;
    Vec top;
    Vec bottom;
    Vec center;
    Vec dir;
    Vec bc;
    Vec up;
    YARARE_INFO* p;
    YARARE_INFO* ret;
    cModel* parts;
    f32 best;
    f32 ang;
    f32 d2;
    f32 r;
    int mask;

    PSVECAdd(&box[2], &box[3], &bc);
    PSVECAdd(&box[6], &bc, &bc);
    PSVECAdd(&box[7], &bc, &bc);
    PSVECScale(&bc, &bc, 0.25f);
    PSVECSubtract(&bc, pos, &dir);
#line 731
    VECNormalize(&dir, &dir);
    PSMTXIdentity(mat);
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    ang = acosf(PSVECDotProduct(&up, &dir));
    if (ang > 0.0f) {
        if (ang < PI) {
            PSVECCrossProduct(&up, &dir, &up);
            PSMTXRotAxisRad(rm, &up, ang);
            PSMTXConcat(rm, mat, mat);
        } else {
            PSMTXRotRad(rm, 'y', PI);
            PSMTXConcat(mat, rm, mat);
        }
    }
    mat[0][3] = pos->x;
    mat[1][3] = pos->y;
    mat[2][3] = pos->z;
    if (PSMTXInverse(rm, mat) == 0) {
        PSMTXIdentity(mat);
    }
    ret = 0;
    best = 1e16f;
    for (p = &em->hitInfo; p != 0; p = p->next) {
        if (!(p->flags & 1)) {
            continue;
        }
        if ((p->flags & 0x10) && HandgunCk(flag)) {
            continue;
        }
        bottom = p->ofs;
        top = p->ofs;
        if (p->flags & 6) {
            if (p->flags & 2) {
                top.x += p->height;
            } else {
                top.z += p->height;
            }
        } else {
            top.y += p->height;
        }
        parts = HitParts(em, p);
        PSMTXMultVec(parts->mat, &top, &top);
        PSMTXMultVec(parts->mat, &bottom, &bottom);
        PSVECAdd(&top, &bottom, &center);
        PSVECScale(&center, &center, 0.5f);
        up.y = up.x = 0.0f;
        up.z = p->width;
        PSMTXMultVecSR(parts->mat, &up, &up);
        r = PSVECMag(&up);
        if (AtBoxCapsuleCk3(box, &top, r, &bottom) == 0) {
            continue;
        }
        PSMTXMultVec(mat, &center, &up);
        d2 = up.x * up.x + up.z * up.z;
        if (d2 > best) {
            continue;
        }
        mask = 0;
        if (flag != 0x10) {
            mask = 0x400000;
        }
        if (EatMgr.hitCheck(pos, &center, 0, 0, 0, mask) != 0) {
            continue;
        }
        PSVECSubtract(&center, pos, &up);
        p->rad = up.x * up.x + up.y * up.y + up.z * up.z;
        p->dist = d2;
        best = d2;
        ret = p;
    }
    return ret;
}

// Hit boxes of `em` crossed by the line a-b (within `len` squared of `a`): the nearest one, with
// pos = hit point, rad = squared distance a -> hit, dist = squared distance hit -> a.
YARARE_INFO* emLineAtCk(cEm* em, Vec* pPos, Vec* pPos2, f32 len, int flag)
{
    Vec top;
    Vec bottom;
    Vec center;
    Vec hit;
    Vec s;
    YARARE_INFO* p;
    YARARE_INFO* ret = 0;
    cModel* parts;
    f32 best = len;
    f32 d2;
    f32 r;

    for (p = &em->hitInfo; p != 0; p = p->next) {
        if (!(p->flags & 1)) {
            continue;
        }
        if ((p->flags & 0x10) && HandgunCk(flag)) {
            continue;
        }
        parts = HitParts(em, p);
        if (p->flags & 8) {
            if (emLineCubeCrossCk(pPos, pPos2, parts->mat, p->width, p->height, p->depth, &p->ofs, &hit) == 0) {
                continue;
            }
        } else {
            bottom = p->ofs;
            top = p->ofs;
            if (p->flags & 6) {
                if (p->flags & 2) {
                    top.x += p->height;
                } else {
                    top.z += p->height;
                }
            } else {
                top.y += p->height;
            }
            PSMTXMultVec(parts->mat, &top, &top);
            PSMTXMultVec(parts->mat, &bottom, &bottom);
            PSVECAdd(&top, &bottom, &center);
            PSVECScale(&center, &center, 0.5f);
            s.x = 0.0f;
            s.y = 0.0f;
            s.z = p->width;
            PSMTXMultVecSR(parts->mat, &s, &s);
            r = PSVECMag(&s);
            if (emLineCapsuleCrossCk(pPos, pPos2, &top, &bottom, r, &hit) == 0) {
                continue;
            }
        }
        PSVECSubtract(pPos, &hit, &s);
        d2 = s.x * s.x + s.y * s.y + s.z * s.z;
        if (d2 > best) {
            continue;
        }
        PSVECSubtract(&hit, pPos, &s);
        p->rad = s.x * s.x + s.y * s.y + s.z * s.z;
        p->dist = d2;
        p->pos = hit;
        best = d2;
        ret = p;
    }
    return ret;
}

// emLineAtCk sorted by the XZ distance only, hit point returned in `out`.
YARARE_INFO* emLineAtCk2(cEm* em, Vec* pPos, Vec* pPos2, f32 len, Vec* out, int flag)
{
    Vec top;
    Vec bottom;
    Vec center;
    Vec hit;
    Vec s;
    YARARE_INFO* p;
    YARARE_INFO* ret = 0;
    cModel* parts;
    f32 best = len;
    f32 d2;
    f32 r;

    for (p = &em->hitInfo; p != 0; p = p->next) {
        if (!(p->flags & 1)) {
            continue;
        }
        if ((p->flags & 0x10) && HandgunCk(flag)) {
            continue;
        }
        parts = HitParts(em, p);
        if (p->flags & 8) {
            if (emLineCubeCrossCk(pPos, pPos2, parts->mat, p->width, p->height, p->depth, &p->ofs, &hit) == 0) {
                continue;
            }
        } else {
            bottom = p->ofs;
            top = p->ofs;
            if (p->flags & 6) {
                if (p->flags & 2) {
                    top.x += p->height;
                } else {
                    top.z += p->height;
                }
            } else {
                top.y += p->height;
            }
            PSMTXMultVec(parts->mat, &top, &top);
            PSMTXMultVec(parts->mat, &bottom, &bottom);
            PSVECAdd(&top, &bottom, &center);
            PSVECScale(&center, &center, 0.5f);
            s.x = 0.0f;
            s.y = 0.0f;
            s.z = p->width;
            PSMTXMultVecSR(parts->mat, &s, &s);
            r = PSVECMag(&s);
            if (emLineCapsuleCrossCk(pPos, pPos2, &top, &bottom, r, &hit) == 0) {
                continue;
            }
        }
        PSVECSubtract(pPos, &hit, &s);
        d2 = s.x * s.x + s.z * s.z;
        if (d2 > best) {
            continue;
        }
        best = d2;
        ret = p;
        *out = hit;
    }
    return ret;
}

// Pool of a helper the original linker dropped (a float -> u32 -> float round trip and a double
// zero between emLineAtCk2's and emLineCapsuleCrossCk's pools).
static f32 emLineAtCkDead(f32 len, f32 step)
{
    u32 n;

    if (step == 0.0f) {
        return 0.0f;
    }
    n = (u32) (len / step);
    if ((f32) n != 0.0) {
        return (f32) n;
    }
    return 0.0f;
}

// Segment a-b against the capsule top-bottom of radius r: 1 with the entry point in `hit`.
int emLineCapsuleCrossCk(Vec* a, Vec* b, Vec* top, Vec* bottom, f32 r, Vec* hit)
{
    Mtx m;
    Mtx inv;
    Vec d;
    Vec la;
    Vec lb;
    Vec c;
    Vec up;
    Vec e;
    Vec f;
    Vec g;
    f32 len;
    f32 ang;
    f32 dist;
    f32 dxz;
    f32 t;
    f32 h;

    PSMTXIdentity(m);
    PSVECSubtract(top, bottom, &d);
    len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
    if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
        d.y = 1.0f;
    }
#line 1250
    VECNormalize(&d, &d);
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    ang = acosf(PSVECDotProduct(&d, &up));
    if (ang > 0.01f && ang < PI - 0.01f) {
        PSVECCrossProduct(&up, &d, &up);
        PSMTXRotAxisRad(inv, &up, ang);
        PSMTXConcat(inv, m, m);
    }
    TransMatrix(m, bottom);
    if (PSMTXInverse(m, inv) == 0) {
        PSMTXIdentity(inv);
    }
    PSMTXMultVec(inv, a, &la);
    PSMTXMultVec(inv, b, &lb);
    dist = (a->x - top->x) * (a->x - top->x) + (a->y - top->y) * (a->y - top->y) + (a->z - top->z) * (a->z - top->z);
    if (dist < r * r) {
        *hit = *a;
        return 1;
    }
    dist = (a->x - bottom->x) * (a->x - bottom->x) + (a->y - bottom->y) * (a->y - bottom->y) +
           (a->z - bottom->z) * (a->z - bottom->z);
    if (dist < r * r) {
        *hit = *a;
        return 1;
    }
    dist = la.x * la.x + la.z * la.z;
    if (la.y > 0.0f && la.y < len && dist < r * r) {
        *hit = *a;
        return 1;
    }
    if ((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z) <= 0.1f) {
        return 0;
    }
    if (LineSphereCrossCk(a, b, top, &c, r)) {
        PSMTXMultVec(inv, &c, &d);
        if (d.y < 0.0f || d.y > len) {
            *hit = c;
            return 1;
        }
    }
    if (LineSphereCrossCk(a, b, bottom, &c, r)) {
        PSMTXMultVec(inv, &c, &d);
        if (d.y < 0.0f || d.y > len) {
            *hit = c;
            return 1;
        }
    }
    dxz = (la.x - lb.x) * (la.x - lb.x) + (la.z - lb.z) * (la.z - lb.z);
    if (dxz <= 0.1f) {
        return 0;
    }
    PSVECSubtract(&lb, &la, &e);
    PSVECScale(&la, &f, -1.0f);
    t = (e.x * f.x + e.z * f.z) / dxz;
    PSVECScale(&e, &g, t);
    PSVECAdd(&g, &la, &g);
    if (g.x * g.x + g.z * g.z >= r * r) {
        return 0;
    }
    h = SQRTF(r * r - (g.x * g.x + g.z * g.z));
    PSVECSubtract(&la, &g, &d);
    PSVECScale(&d, &d, h / PSVECMag(&d));
    PSVECAdd(&d, &g, &d);
    // The entry-point distance reuses `dist` (a function-scope pseudo, so its sum schedules ahead of
    // h's) and is computed first.
    dist = (la.x - d.x) * (la.x - d.x) + (la.y - d.y) * (la.y - d.y) + (la.z - d.z) * (la.z - d.z);
    h = (la.x - lb.x) * (la.x - lb.x) + (la.y - lb.y) * (la.y - lb.y) + (la.z - lb.z) * (la.z - lb.z);
    if (dist > h || d.y < 0.0f || d.y > len) {
        return 0;
    }
    PSMTXMultVec(m, &d, &la);
    PSVECSubtract(a, &la, &d);
    PSVECSubtract(a, b, &up);
    if (PSVECDotProduct(&d, &up) < 0.0f) {
        return 0;
    }
    PSVECSubtract(b, &la, &d);
    PSVECSubtract(b, a, &up);
    if (PSVECDotProduct(&d, &up) < 0.0f) {
        return 0;
    }
    *hit = la;
    return 1;
}

// Segment a-b against the box (sx, sy, sz) at `ofs` in the space of `m`: the six faces as quads.
int emLineCubeCrossCk(Vec* a, Vec* b, Mtx m, f32 sx, f32 sy, f32 sz, Vec* ofs, Vec* hit)
{
    Mtx mat;
    Vec poly[4];
    Vec c;
    Vec v[8];

    PSMTXMultVec(m, ofs, &c);
    PSMTXCopy(m, mat);
    TransMatrix(mat, &c);
    v[0].x = -sx;
    v[0].y = 0.0f;
    v[0].z = sz;
    v[1].x = sx;
    v[1].y = 0.0f;
    v[1].z = sz;
    v[2].x = sx;
    v[2].y = sy;
    v[2].z = sz;
    v[3].x = -sx;
    v[3].y = sy;
    v[3].z = sz;
    v[4].x = -sx;
    v[4].y = 0.0f;
    v[4].z = -sz;
    v[5].x = sx;
    v[5].y = 0.0f;
    v[5].z = -sz;
    v[6].x = sx;
    v[6].y = sy;
    v[6].z = -sz;
    v[7].x = -sx;
    v[7].y = sy;
    v[7].z = -sz;
    PSMTXMultVec(mat, &v[0], &v[0]);
    PSMTXMultVec(mat, &v[1], &v[1]);
    PSMTXMultVec(mat, &v[2], &v[2]);
    PSMTXMultVec(mat, &v[3], &v[3]);
    PSMTXMultVec(mat, &v[4], &v[4]);
    PSMTXMultVec(mat, &v[5], &v[5]);
    PSMTXMultVec(mat, &v[6], &v[6]);
    PSMTXMultVec(mat, &v[7], &v[7]);
    poly[0] = v[0];
    poly[1] = v[1];
    poly[2] = v[2];
    poly[3] = v[3];
    if (emLinePolyCrossCk(a, b, poly, hit)) {
        return 1;
    }
    poly[0] = v[1];
    poly[1] = v[5];
    poly[2] = v[6];
    poly[3] = v[2];
    if (emLinePolyCrossCk(a, b, poly, hit)) {
        return 1;
    }
    poly[0] = v[5];
    poly[1] = v[4];
    poly[2] = v[7];
    poly[3] = v[6];
    if (emLinePolyCrossCk(a, b, poly, hit)) {
        return 1;
    }
    poly[0] = v[4];
    poly[1] = v[0];
    poly[2] = v[3];
    poly[3] = v[7];
    if (emLinePolyCrossCk(a, b, poly, hit)) {
        return 1;
    }
    poly[0] = v[3];
    poly[1] = v[2];
    poly[2] = v[6];
    poly[3] = v[7];
    if (emLinePolyCrossCk(a, b, poly, hit)) {
        return 1;
    }
    poly[0] = v[1];
    poly[1] = v[0];
    poly[2] = v[4];
    poly[3] = v[5];
    if (emLinePolyCrossCk(a, b, poly, hit)) {
        return 1;
    }
    return 0;
}

// Segment a-b (a on the front side) against the quad poly[4]: 1 with the crossing point in `hit`.
int emLinePolyCrossCk(Vec* pPos, Vec* pPos2, Vec* poly, Vec* hit)
{
    Vec e1;
    Vec e2;
    Vec n;
    Vec p;
    Vec c;
    f32 da;
    f32 db;
    f32 t;

    PSVECSubtract(&poly[2], &poly[1], &e1);
    PSVECSubtract(&poly[0], &poly[1], &e2);
    PSVECCrossProduct(&e1, &e2, &n);
    if (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f) {
        return 0;
    }
#line 1552
    VECNormalize(&n, &n);
    da = PSVECDotProduct(&n, pPos) - PSVECDotProduct(&n, &poly[0]);
    if (da <= 0.0f) {
        return 0;
    }
    db = PSVECDotProduct(&n, pPos2) - PSVECDotProduct(&n, &poly[0]);
    if (db >= 0.0f) {
        return 0;
    }
    db = fabsf(db);
    t = db / (da + db);
    PSVECSubtract(pPos, pPos2, &p);
    PSVECScale(&p, &p, t);
    PSVECAdd(&p, pPos2, &p);
    PSVECSubtract(&p, &poly[1], &e1);
    PSVECSubtract(&poly[0], &poly[1], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    PSVECSubtract(&p, &poly[2], &e1);
    PSVECSubtract(&poly[1], &poly[2], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    PSVECSubtract(&p, &poly[3], &e1);
    PSVECSubtract(&poly[2], &poly[3], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    PSVECSubtract(&p, &poly[0], &e1);
    PSVECSubtract(&poly[3], &poly[0], &e2);
    PSVECCrossProduct(&e1, &e2, &c);
    if (PSVECDotProduct(&n, &c) < 0.0f) {
        return 0;
    }
    *hit = p;
    return 1;
}

// Hit boxes of `em` touched by the sphere (pos, r): the one best facing the pos2 -> pos direction
// (or the nearest when pos2 is at pos); rad = squared distance centre -> pos.
YARARE_INFO* emSphereAtCk(cEm* em, Vec* pos, Vec* pos2, f32 r, int flag, f32 r2)
{
    Vec top;
    Vec bottom;
    Vec center;
    Vec d;
    Vec s;
    YARARE_INFO* p;
    YARARE_INFO* ret;
    cModel* parts;
    f32 dist;
    f32 bestRad;
    f32 bestDot;
    f32 rr;
    f32 dp;

    dist = (pos->x - pos2->x) * (pos->x - pos2->x) + (pos->y - pos2->y) * (pos->y - pos2->y) +
           (pos->z - pos2->z) * (pos->z - pos2->z);
    if (dist > 100.0f) {
        PSVECSubtract(pos, pos2, &d);
#line 1658
        VECNormalize(&d, &d);
    } else {
        dist = 0.0f;
    }
    bestRad = 1e16f;
    ret = 0;
    bestDot = -PI;
    for (p = &em->hitInfo; p != 0; p = p->next) {
        if (!(p->flags & 1)) {
            continue;
        }
        if ((p->flags & 0x10) && HandgunCk(flag)) {
            continue;
        }
        bottom = p->ofs;
        top = p->ofs;
        if (p->flags & 6) {
            if (p->flags & 2) {
                top.x += p->height;
            } else {
                top.z += p->height;
            }
        } else {
            top.y += p->height;
        }
        parts = HitParts(em, p);
        PSMTXMultVec(parts->mat, &top, &top);
        PSMTXMultVec(parts->mat, &bottom, &bottom);
        PSVECAdd(&top, &bottom, &center);
        PSVECScale(&center, &center, 0.5f);
        if (p->flags & 8) {
            Vec box[8] = {
                {-500.0f, -450.0f, 0.0f},   {500.0f, -450.0f, 0.0f},   {-3000.0f, -800.0f, 15000.0f}, {3000.0f, -800.0f, 15000.0f},
                {-500.0f, 450.0f, 0.0f},    {500.0f, 450.0f, 0.0f},    {-3000.0f, 800.0f, 15000.0f},  {3000.0f, 800.0f, 15000.0f},
            };

            box[0].x = -p->width;
            box[0].y = 0.0f;
            box[0].z = -p->depth;
            box[1].x = p->width;
            box[1].y = 0.0f;
            box[1].z = -p->depth;
            box[2].x = -p->width;
            box[2].y = 0.0f;
            box[2].z = p->depth;
            box[3].x = p->width;
            box[3].y = 0.0f;
            box[3].z = p->depth;
            box[4].x = -p->width;
            box[4].y = p->height;
            box[4].z = -p->depth;
            box[5].x = p->width;
            box[5].y = p->height;
            box[5].z = -p->depth;
            box[6].x = -p->width;
            box[6].y = p->height;
            box[6].z = p->depth;
            box[7].x = p->width;
            box[7].y = p->height;
            box[7].z = p->depth;
            PSMTXMultVec(parts->mat, &box[0], &box[0]);
            PSMTXMultVec(parts->mat, &box[1], &box[1]);
            PSMTXMultVec(parts->mat, &box[2], &box[2]);
            PSMTXMultVec(parts->mat, &box[3], &box[3]);
            PSMTXMultVec(parts->mat, &box[4], &box[4]);
            PSMTXMultVec(parts->mat, &box[5], &box[5]);
            PSMTXMultVec(parts->mat, &box[6], &box[6]);
            PSMTXMultVec(parts->mat, &box[7], &box[7]);
            if (At_box_sphere_ck(box, pos, r) == 0) {
                continue;
            }
        } else {
            s.x = 0.0f;
            s.y = 0.0f;
            s.z = p->width;
            PSMTXMultVecSR(parts->mat, &s, &s);
            rr = PSVECMag(&s);
            if (AtSphereCapsuleCkF(pos, r, rr, &top, &bottom) == 0) {
                continue;
            }
        }
        PSVECSubtract(&center, pos, &s);
        if (r != r2) {
            if (fabsf(s.y) > r2 + p->width) {
                continue;
            }
        }
        p->rad = s.x * s.x + s.y * s.y + s.z * s.z;
        if (dist > 0.0f) {
#line 1781
            VECNormalize(&s, &s);
            dp = PSVECDotProduct(&d, &s);
            if (dp < bestDot) {
                if (dp < 0.6f) {
                    continue;
                }
                if (p->rad >= bestRad) {
                    continue;
                }
            }
            bestDot = dp;
            bestRad = p->rad;
            ret = p;
        } else {
            p->rad = (pos->x - center.x) * (pos->x - center.x) + (pos->y - center.y) * (pos->y - center.y) +
                     (pos->z - center.z) * (pos->z - center.z);
            if (p->rad >= bestRad) {
                continue;
            }
            bestRad = p->rad;
            ret = p;
        }
    }
    return ret;
}

// Enemies hit by the melee box: up to `max` entries, the farthest replaced when the list is full.
u32 GetWepTargetList(Vec* box, Vec* pos, WepTarget* list, u32 max, int flag)
{
    u32 cnt = 0;
    u32 i;
    u32 j;
    u32 worst;
    cEm* em;
    YARARE_INFO* part;
    YARARE_INFO* q;
    WepTarget* wp;
    f32 wr;

    i = 0;
    if (i < EmMgr.nArray) {
        do {
        em = emWork(i);
        if (!(em->be_flag & 1)) {
            continue;
        }
        if (!(em->be_flag & 0x20)) {
            continue;
        }
        if (em->id <= 0xF && em->id != 3 && em->id != 4) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (EmIsDead(em)) {
            continue;
        }
        if (flag == 0xE && em->id == 0x4F) {
            continue;
        }
        part = emBoxAtCk(em, box, pos, flag);
        if (part == 0) {
            continue;
        }
        part->flags &= ~0x4000;
        if (cnt < max) {
            WEP_LIST(cnt)->part = part;
            WEP_LIST(cnt)->em = em;
            cnt++;
            continue;
        }
        worst = 0;
        wr = WEP_LIST(0)->part->rad;
        for (j = 1; j < max; j++) {
            q = WEP_LIST(j)->part;
            if (q->dist <= 250000.0f) {
                if (WEP_LIST(worst)->part->dist > 250000.0f) {
                    continue;
                }
                if (q->rad < wr) {
                    continue;
                }
                wr = q->rad;
                worst = j;
            } else {
                if (WEP_LIST(worst)->part->dist <= 250000.0f && WEP_LIST(worst)->part->dist > q->dist) {
                    continue;
                }
                wr = q->rad;
                worst = j;
            }
        }
        // The replacement stores are written in BOTH arms (jump2 cross-jumps them into one tail):
        // with a single tail after the join, the tail's `worst * 8` is a gcse recomputation whose
        // copy from the first `slwi` survives as `mr r10,r0` (the first `slwi` is block-local and
        // local-alloc gives it r0, which the `stwx` index cannot use).
        wp = WEP_LIST(worst);
        if (wp->part->dist <= 250000.0f) {
            if (part->dist > 250000.0f) {
                continue;
            }
            if (wp->part->rad < part->rad) {
                continue;
            }
            wp->part = part;
            WEP_LIST(worst)->em = em;
        } else {
            if (part->dist <= 250000.0f) {
                if (wp->part->dist < part->dist) {
                    continue;
                }
            }
            wp->part = part;
            WEP_LIST(worst)->em = em;
        }
        } while (++i < EmMgr.nArray);
    }
    return cnt;
}

// Enemies crossed by the shot line p0-p1 (stopped at the scenario hit), nearest first; the
// hit-only 0x41/0x4E enemies are added last. Returns the count; `hit` / `nrm` / `attr` receive the
// scenario hit (nrm zero when an enemy was hit).
u32 GetWepTargetList2(Vec* p0, Vec* p1, WepTarget* list, u32 max, Vec* hit, Vec* nrm, u32* attr, int type,
                      int flag)
{
    Mtx m;
    Vec d;
    u32 cnt = 0;
    int i;
    int j;
    int k;
    int worst;
    u32 mask;
    f32 dist;
    f32 l;
    cEm* bestEm;
    YARARE_INFO* bestPart;
    YARARE_INFO* part;
    cModel* parts;
    cEm* em;  // one variable for both scans and the sort swap (r31 throughout); `i` is the sort's outer counter too
    YARARE_INFO* part2;

    mask = 0;
    if (type != 0x10) {
        mask = 0x400000;
    }
    *attr = EatMgr.hitCheck(p0, p1, hit, nrm, 0, mask);
    if (*attr) {
        dist = (p0->x - hit->x) * (p0->x - hit->x) + (p0->y - hit->y) * (p0->y - hit->y) +
               (p0->z - hit->z) * (p0->z - hit->z);
    } else {
        *hit = *p1;
        dist = 1e16f;
        nrm->x = 0.0f;
        nrm->y = 0.0f;
        nrm->z = 0.0f;
    }
    if (DbgFlagChk(pG, DBG_YARARE_DISP)) {
        Draw_line3d(p0, hit, 0xFFFFFFFF, 0);
    }
    PSVECSubtract(p1, p0, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, p0);
    if (PSMTXInverse(m, m) == 0) {
        PSMTXIdentity(m);
    }
    bestPart = 0;
    bestEm = 0;
    i = 0;
    if (i < (int) EmMgr.nArray) {
        do {
        em = emWork(i);

        if ((em->be_flag & 0x201) != 1) {
            continue;
        }
        if (em->id != 0x41 && em->id != 0x4E) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        parts = em->getPartsPtr(0);
        switch (em->id) {
        case 0x2B:
        case 0x2F:
        case 0x31:
        case 0x37:
        case 0x38:
        case 0x3B:
        case 0x3E:
        case 0x4D:
            break;
        default:
            PSMTXMultVec(m, &parts->world, &d);
            if (d.z < -10000.0f) {
                continue;
            }
            if (d.x > 10000.0f) {
                continue;
            }
            if (d.x < -10000.0f) {
                continue;
            }
            break;
        }
        part = emLineAtCk(em, p0, p1, dist, type);
        if (part == 0) {
            continue;
        }
        part->flags |= 0x4000;
        if (bestPart != 0 && part->rad > bestPart->rad) {
            continue;
        }
        bestPart = part;
        bestEm = em;
        } while (++i < (int) EmMgr.nArray);
    }
    if (bestPart) {
        if (!(bestPart->flags & 0x20)) {
            nrm->x = 0.0f;
            nrm->y = 0.0f;
            nrm->z = 0.0f;
            dist = bestPart->rad;
        } else {
            if (max <= 0x13) {
                max++;
            }
        }
    }
    i = 0;
    if (i < (int) EmMgr.nArray) {
        do {
        em = emWork(i);

        if (!(em->be_flag & 1)) {
            continue;
        }
        if (!(em->be_flag & 0x20)) {
            continue;
        }
        if (em->id <= 0xF && em->id != 3 && em->id != 4) {
            continue;
        }
        if ((flag & 1) && (em->id == 3 || em->id == 4)) {
            continue;
        }
        switch (em->id) {
        case 0x41:
        case 0x4E:
            continue;
        case 0x42:
        case 0x4F:
            if (type == 0xE) {
                continue;
            }
            break;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (em->id != 0x50) {
            if (EmIsDead(em)) {
                continue;
            }
        }
        parts = em->getPartsPtr(0);
        switch (em->id) {
        case 0x2B:
        case 0x2F:
        case 0x31:
        case 0x37:
        case 0x38:
        case 0x3B:
        case 0x3E:
        case 0x4D:
            break;
        default:
            PSMTXMultVec(m, &parts->world, &d);
            if (d.z < -10000.0f) {
                continue;
            }
            if (d.x > 10000.0f) {
                continue;
            }
            if (d.x < -10000.0f) {
                continue;
            }
            break;
        }
        l = dist;
        if (type == 0x10 && (em->id == 0x43 || em->id == 0x4C)) {
            l = 1e16f;
        }
        part = emLineAtCk(em, p0, p1, l, type);
        if (part == 0) {
            continue;
        }
        part->flags |= 0x4000;
        if (part->flags & 0x20) {
            if (max <= 0x13) {
                max++;
            }
        }
        if ((int) cnt < (int) max) {
            list[cnt].part = part;
            list[cnt].em = em;
            cnt++;
            continue;
        }
        worst = 0;
        for (j = 1; j < (int) max; j++) {
            if (list[worst].part->rad <= list[j].part->rad) {
                worst = j;
            }
        }
        if (list[worst].part->rad > part->rad) {
            list[worst].part = part;
            list[worst].em = em;
        }
        } while (++i < (int) EmMgr.nArray);
    }
    for (i = 0; i < (int) cnt - 1; i++) {
        for (j = i + 1; j < (int) cnt; j++) {
            if (list[i].part->rad > list[j].part->rad) {
                em = list[i].em;
                part2 = list[i].part;
                list[i].part = list[j].part;
                list[i].em = list[j].em;
                list[j].part = part2;
                list[j].em = em;
            }
        }
    }
    if (bestPart) {
        if ((int) cnt <= (int) max - 1 || cnt == 0) {
            list[cnt].part = bestPart;
            list[cnt].em = bestEm;
            cnt++;
        }
    }
    if (cnt != 0) {
        nrm->x = 0.0f;
        nrm->y = 0.0f;
        nrm->z = 0.0f;
    }
    return cnt;
}

// Enemies inside the blast sphere (pos, r), nearest first.
int GetWepTargetListBomb(Vec* pos, f32 r, WepTarget* list, int max, int type, int flag)
{
    Vec center;
    Vec bottom;
    Vec top;
    int cnt;
    int i;
    int j;
    int worst;
    u32 axis;
    u32 mask;
    f32 rr;
    f32 r2;
    YARARE_INFO* part;
    cModel* parts;
    cEm* em;  // one variable for the scan and the sort swap (r24 in both loops)
    YARARE_INFO* part2;

    switch (type) {
    case 0xD:
    case 0x12:
    case 0x13:
        PlBombHitCk(pos, r);
        break;
    }
    if (DbgFlagChk(pG, DBG_YARARE_DISP)) {
        Draw_sphere(pos, r, 0xFFFF00FF, 1, 1);
    }
    cnt = 0;
    i = 0;
    if (i < (int) EmMgr.nArray) {
        do {
        em = emWork(i);

        if (!(em->be_flag & 1)) {
            continue;
        }
        if (!(em->be_flag & 0x20)) {
            continue;
        }
        if (em->id <= 0xF && em->id != 3 && em->id != 4) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (EmIsDead(em)) {
            continue;
        }
        if (type == 0xE && em->id == 0x4F) {
            continue;
        }
        if ((flag & 1) && (em->id == 3 || em->id == 4)) {
            continue;
        }
        switch (em->id) {
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
            rr = r;
            break;
        default:
            rr = r;
            break;
        case 3:
            rr = r;
            if (rr > 2500.0f) {
                rr = 2500.0f;
            }
            break;
        case 4:
            rr = r;
            if (rr > 1500.0f) {
                rr = 1500.0f;
            }
            break;
        }
        r2 = rr;
        switch (type) {
        case 0xD:
        case 0x12:
        case 0x13:
        case 0x2D:
            if (rr > 2000.0f) {
                r2 = 2000.0f;
            }
            break;
        }
        part = emSphereAtCk(em, pos, pos, rr, type, r2);
        if (part == 0) {
            continue;
        }
        part->flags &= ~0x4000;
        if (part->flags & 6) {
            axis = (part->flags & 2) ? 0 : 2;
        } else {
            axis = 1;
        }
        parts = HitParts(em, part);
        bottom = part->ofs;
        top = part->ofs;
        switch (axis) {
        case 0:
            top.x += part->height;
            break;
        case 1:
        default:
            top.y += part->height;
            break;
        case 2:
            top.z += part->height;
            break;
        }
        PSMTXMultVec(parts->mat, &top, &top);
        PSMTXMultVec(parts->mat, &bottom, &bottom);
        PSVECAdd(&top, &bottom, &center);
        PSVECScale(&center, &center, 0.5f);
        if (!(part->flags & 0x80)) {
            mask = 0;
            if (type != 0x10) {
                mask = 0x400000;
            }
            if ((pos->x - center.x) * (pos->x - center.x) + (pos->y - center.y) * (pos->y - center.y) +
                        (pos->z - center.z) * (pos->z - center.z) >
                    (rr * 0.3f) * (rr * 0.3f) ||
                em->id == 3 || em->id == 4 || em->id == 0x39) {
                if (EatMgr.hitCheck(pos, &center, 0, 0, 0, mask) != 0) {
                    continue;
                }
            }
        }
        if (cnt < max) {
            list[cnt].part = part;
            list[cnt].em = em;
            cnt++;
            continue;
        }
        worst = 0;
        for (j = 1; j < max; j++) {
            if (list[worst].part->rad <= list[j].part->rad) {
                worst = j;
            }
        }
        if (list[worst].part->rad > part->rad) {
            list[worst].part = part;
            list[worst].em = em;
        }
        } while (++i < (int) EmMgr.nArray);
    }
    for (i = 0; i < cnt - 1; i++) {
        for (j = i + 1; j < cnt; j++) {
            if (list[i].part->rad > list[j].part->rad) {
                em = list[i].em;
                part2 = list[i].part;
                list[i].part = list[j].part;
                list[i].em = list[j].em;
                list[j].part = part2;
                list[j].em = em;
            }
        }
    }
    return cnt;
}

// Player inside the blast sphere: damage 9 (knock down) beyond the inner radius, 8 with a life
// loss inside it. 1 when the player was hit.
int PlBombHitCk(Vec* pos, f32 r)
{
    cModel* parts;
    f32 d2;
    f32 lim;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (EmIsDead(pPL)) {
        return 0;
    }
    parts = pPL->getPartsPtr(0);
    d2 = (pos->x - parts->world.x) * (pos->x - parts->world.x) +
         (pos->y - parts->world.y) * (pos->y - parts->world.y) +
         (pos->z - parts->world.z) * (pos->z - parts->world.z);
    if (d2 > 36000000.0f) {
        return 0;
    }
    if (d2 > (r + 300.0f) * (r + 300.0f)) {
        return 0;
    }
    if (pG->weapon_no == 0xD && pG->weapon_type == 2) {
        lim = 1500.0f;
    } else {
        lim = 2500.0f;
    }
    if (d2 > lim * lim) {
        PlSetDamage(9, 0, 0);
        return 1;
    }
    if (EatMgr.hitCheck(pos, &parts->world, 0, 0, 0, 0x400000) != 0) {
        return 0;
    }
    LifeDownSet2(pPL, 1200, 0, PlLifeOver(501));
    PlSetDamage(8, 0, 0);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    return 1;
}

// Point the weapon line p0-p1 hits: the scenario (1), an enemy (2, 3 with flag 0x40) or nothing (0);
// p1 is moved to the hit point.
int GetWepTargetPos(Vec* pPos, Vec* pPos2, int plCheck, int wepNo, cEm** outEm, int* outAttr)
{
    Mtx m;
    Vec hit;
    Vec h2;
    Vec d;
    int ret = 0;
    int attr;
    u32 i;
    cEm* em;
    cModel* parts;
    YARARE_INFO* part;
    f32 dist;
    f32 len;
    f32 e2;
    s16 life;

    if (outEm) {
        *outEm = 0;
    }
    attr = EatMgr.hitCheck(pPos, pPos2, &hit, 0, 0, 0x400000);
    if (attr) {
        ret = 1;
        if (outAttr) {
            *outAttr = attr;
        }
    } else {
        hit = *pPos2;
    }
    dist = (pPos->x - hit.x) * (pPos->x - hit.x) + (pPos->y - hit.y) * (pPos->y - hit.y) + (pPos->z - hit.z) * (pPos->z - hit.z);
    len = dist;
    PSVECSubtract(pPos2, pPos, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, pPos);
    if (PSMTXInverse(m, m) == 0) {
        PSMTXIdentity(m);
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((em->be_flag & 0x201) != 1) {
            continue;
        }
        if (em->be_flag & 0x10000000) {
            continue;
        }
        if (plCheck) {
            if (i != 0) {
                continue;
            }
            life = pG->pl_life;
        } else {
            if (i == 0) {
                continue;
            }
            life = em->hp;
        }
        if (life <= 0) {
            continue;
        }
        parts = em->getPartsPtr(0);
        switch (em->id) {
        case 0x2B:
        case 0x2F:
        case 0x31:
        case 0x37:
        case 0x38:
        case 0x3B:
        case 0x3E:
        case 0x4D:
            break;
        default:
            PSMTXMultVec(m, &parts->world, &d);
            if (d.z < -10000.0f) {
                continue;
            }
            if (d.x > 10000.0f) {
                continue;
            }
            if (d.x < -10000.0f) {
                continue;
            }
            if (Front_check(pPL, em, PI / 2.0f) == 0) {
                continue;
            }
            break;
        }
        part = emLineAtCk2(em, pPos, pPos2, len, &h2, 0);
        if (part == 0) {
            continue;
        }
        e2 = (pPos->x - h2.x) * (pPos->x - h2.x) + (pPos->y - h2.y) * (pPos->y - h2.y) + (pPos->z - h2.z) * (pPos->z - h2.z);
        if (e2 > dist) {
            continue;
        }
        dist = e2;
        hit = h2;
        if (outEm) {
            *outEm = em;
        }
        ret = 2;
        if (part->flags & 0x40) {
            ret = 3;
        }
    }
    *pPos2 = hit;
    return ret;
}

// Hit box of `em` the sphere (pos, r) touches, stepping along each capsule's axis; the contact
// point on the axis goes to `out`.
YARARE_INFO* EmYarareContactCk(cEm* em, Vec* pos, Vec* out, f32 r)
{
    Vec top;
    Vec bottom;
    Vec s;
    Vec q;
    YARARE_INFO* p;
    cModel* parts;
    f32 rr;
    f32 len;  // the axis length, then the step (one variable: it lives across the VECNormalize call)
    u32 n;
    register s16 hm asm("r5"); // COMPILER-DIFF: #8

    // COMPILER-DIFF: #8 -- the original ranks `mr r26,r5` (out) after `fmr f28,f1`, i.e. as if r5
    // did not die at the copy; the HImode read of r5 keeps it live past the copy (docs/matching.md #8).
    // The dummy memory output is a stack local: naming `em->hitInfo.flags` here gave `em` one more
    // reference than `pos`, which swaps their r23/r24 global-alloc order.
    asm("" : "=m"(q) : "r"(hm));
    if (em->hp <= 0) {
        return 0;
    }
    for (p = &em->hitInfo; p != 0; p = p->next) {
        if (!(p->flags & 1)) {
            continue;
        }
        if (p->flags & 8) {
            continue;
        }
        bottom = p->ofs;
        top = p->ofs;
        if (p->flags & 6) {
            if (p->flags & 2) {
                top.x += p->height;
            } else {
                top.z += p->height;
            }
        } else {
            top.y += p->height;
        }
        parts = HitParts(em, p);
        PSMTXMultVec(parts->mat, &bottom, &bottom);
        PSMTXMultVec(parts->mat, &top, &top);
        s.x = 0.0f;
        s.y = 0.0f;
        s.z = p->width;
        PSMTXMultVecSR(parts->mat, &s, &s);
        rr = PSVECMag(&s);
        if (SphereHitCk(pos, &top, r, rr)) {
            if (out) {
                *out = top;
            }
            return p;
        }
        if (SphereHitCk(pos, &bottom, r, rr)) {
            if (out) {
                *out = bottom;
            }
            return p;
        }
        PSVECSubtract(&top, &bottom, &s);
        len = RootSumSquare3(&s);
        n = (u32) (len / (rr + rr)) + 2;
        len = len / (f32) n;
        if (len < 0.01f) {
            continue;
        }
#line 2785
        VECNormalize(&s, &s);
        PSVECScale(&s, &s, len);
        q = bottom;
        // n - 1 steps as a post-decrement countdown on `n` itself: combine folds `--n != -1` into the
        // compare-and-add parallel (`cmpwi 0; addi -1; bne`), and the duplicated entry test becomes
        // `cmpwi n,1; addi n,-2; beq` in the one register.
        n--;
        while (n-- != 0) {
            PSVECAdd(&q, &s, &q);
            if (SphereHitCk(pos, &q, r, rr)) {
                if (out) {
                    *out = q;
                }
                return p;
            }
        }
    }
    return 0;
}

// Debug draw of the hit boxes (grey; red when the box took the current damage; off when dead).
void EmYarareDisp(cEm* em)
{
    Vec top;
    Vec bottom;
    Vec s;
    YARARE_INFO* p;
    cModel* parts;
    u32 color;

    if (!DbgFlagChk(pG, DBG_YARARE_DISP)) {
        return;
    }
    for (p = &em->hitInfo; p != 0; p = p->next) {
        // Every `p->flags` read is spelled out: the later `& 1` / `& 8` reads are fully redundant, so
        // gcse PRE deletes them and inserts the reaching-register copy (`mr r11,r0`) after the first load.
        if (!(p->flags & 1)) {
            continue;
        }
        color = 0x60606060;
        if (EmIsDead(em) && p == em->dmg.m_pDamageYarare) {
            color = 0xFF000000;
        }
        if (em->hp <= 0) {
            color = 0;
        }
        if (!(p->flags & 1)) {
            color = 0;
        }
        if (p->flags & 8) {
            parts = HitParts(em, p);
            AtCubeDisp(parts->mat, p->width, p->height, p->depth, &p->ofs, color);
        } else {
            bottom = p->ofs;
            top = p->ofs;
            if (p->flags & 6) {
                if (p->flags & 2) {
                    top.x += p->height;
                } else {
                    top.z += p->height;
                }
            } else {
                top.y += p->height;
            }
            parts = HitParts(em, p);
            PSMTXMultVec(parts->mat, &bottom, &bottom);
            PSMTXMultVec(parts->mat, &top, &top);
            s.x = 0.0f;
            s.y = 0.0f;
            s.z = p->width;
            PSMTXMultVecSR(parts->mat, &s, &s);
            AtCapsuleDisp(&top, &bottom, PSVECMag(&s), color);
        }
    }
}

// Pool of a helper the original linker dropped (a double zero before LifeDownSet2's pool).
static int emYarareDead(f32 v)
{
    return v != 0.0;
}

// Runs the per-enemy scenario hook (em->pScenario) when the room event installed one.
void EmScenario(cEm* em)
{
    if (em->pScenario) {
        em->pScenario(em);
    }
}

// LifeDownSet2 without the random spread: take `dmg` life from `em`, flag bit0 keeps 1 point.
int LifeDownSet(cEm* em, int dmg, int flag)
{
    return LifeDownSet2(em, dmg, flag, 0);
}

// Take `dmg` (+-rnd) off the life of `em`: the player (id 0), the partner (ids 1..0xD) or an enemy.
// flag bit0 leaves 1 life point. Returns the remaining life.
int LifeDownSet2(cEm* em, int dmg, int rnd, int flag)
{
    int r;
    int ret;
    f32 rate;

    r = (Rnd() << 8) | Rnd();
    if (rnd != 0) {
        dmg += r % (rnd * 2) - rnd;
        // Dead store: it puts the signed int->float magic first in the pool (flow deletes the code); in
        // this skipped block its constant pseudos stay off the cse path of the live conversions.
        rate = (f32) dmg;
    }
    if (em->id == 0) {
        if ((s16) pG->pl_life <= 0) {
            return 0;
        }
        rate = (f32) pG->Game_level * 0.1f + 0.5f;
        if (PlIsArmor()) {
            rate *= 0.7f;
        }
        dmg = (int) ((f32) dmg * rate);
        if (dmg > 100) {
            if (dmg > 500) {
                GameAddPoint(LVADD_PL_BIG_DAMAGE);
            } else {
                GameAddPoint(LVADD_PL_DAMAGE);
            }
        }
        if (pG->Game_level <= 2 && (s16) pG->pl_life > 300) {
            flag |= 1;
        }
        if ((s16) pG->pl_life > 300 && (Rnd() & 3) == 0) {
            flag |= 1;
        }
        if ((s16) pG->pl_life < dmg) {
            dmg = (s16) pG->pl_life;
        }
        HSet(pG->pl_life, pG->pl_life - dmg);
        if ((s16) pG->pl_life <= 0) {
            if (flag & 1) {
                HSet(pG->pl_life, 1);
            }
            if ((s16) pG->pl_life < 0) {
                HSet(pG->pl_life, 0);
            }
        }
        if (DbgFlagChk(pG, DBG_NO_DEATH)) {
            HSet(pG->pl_life, pG->pl_life_max);
        }
        if (DbgFlagChk(pG, DBG_NO_DEATH2) && (s16) pG->pl_life <= 1) {
            HSet(pG->pl_life, 2);
        }
        ret = (s16) pG->pl_life;
    } else if (em->id <= 0xD) {
        if ((s16) pG->ashley_life <= 0) {
            return 0;
        }
        rate = (f32) pG->Game_level * 0.1f + 0.5f;
        dmg = (int) ((f32) dmg * rate);
        if (dmg > 100) {
            if (dmg > 500) {
                GameAddPoint(LVADD_PL_BIG_DAMAGE);
            } else {
                GameAddPoint(LVADD_PL_DAMAGE);
            }
        }
        if ((s16) pG->ashley_life < dmg) {
            dmg = (s16) pG->ashley_life;
        }
        HSet(pG->ashley_life, pG->ashley_life - dmg);
        if ((s16) pG->ashley_life <= 0) {
            if (flag & 1) {
                HSet(pG->ashley_life, 1);
            }
            if ((s16) pG->ashley_life < 0) {
                HSet(pG->ashley_life, 0);
            }
        }
        if (DbgFlagChk(pG, DBG_NO_DEATH)) {
            HSet(pG->ashley_life, pG->ashley_life_max);
        }
        if (DbgFlagChk(pG, DBG_NO_DEATH2) && (s16) pG->ashley_life <= 1) {
            HSet(pG->ashley_life, 2);
        }
        ret = (s16) pG->ashley_life;
    } else {
        if (DbgFlagChk(pG, DBG_EM_NO_DEATH)) {
            return em->hp;
        }
        if (em->hp <= 0) {
            return 0;
        }
        switch (em->id) {
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
        case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26:
        case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D:
        case 0x2F: case 0x30: case 0x31: case 0x32:
        case 0x34: case 0x35:
        case 0x37: case 0x38: case 0x39: case 0x3A:
        case 0x3C:
            if (pG->Game_level > 5) {
                rate = 1.0f - (f32) (int) (pG->Game_level - 5) * 0.03f;
            } else {
                rate = 2.0f - (f32) pG->Game_level * 0.2f;
            }
            dmg = (int) ((f32) dmg * rate);
            if (em->id >= 0x10 && em->id <= 0x3F && dmg > 100) {
                GameAddPoint(LVADD_EM_DAMAGE);
            }
            break;
        }
        if (DbgFlagChk(pG, DBG_EM_WEAK)) {
            dmg = em->hp;
        }
        if (em->hp < dmg) {
            dmg = em->hp;
        }
        em->hp -= dmg;
        if (em->hp <= 0 && (flag & 1)) {
            em->hp = 1;
        }
        ret = em->hp;
        if (ret <= 0 && (u32) (em->id - 0x10) <= 0x2F) {
            GameAddPoint(LVADD_EM_DIE);
        }
    }
    return ret;
}

// Player damage entry: register the hit, take the life, and start the damage motion `type`
// (6/7 die, 8 knocked down; on 0 life 8 becomes 7 and the invincibility flags turn 6/7 into 2/8).
void PlSetDamage(int type, int dmg, int flag)
{
    pPL->dmg.set(0, 0x1E);
    pPL->subArc = pPL->subArc2;
    if (dmg != 0) {
        LifeDownSet2(pPLS, dmg, 0, flag);
    }
    if ((s16) pG->pl_life <= 0) {
        if (type == 8) {
            type = 7;
        }
        if (DbgFlagChk(pG, DBG_NO_DEATH)) {
            HSet(pG->pl_life, pG->pl_life_max);
            if (type == 6) {
                type = 2;
            }
            if (type == 7) {
                type = 8;
            }
        }
    }
    if ((s16) pG->pl_life <= 1 && (DbgFlagChk(pG, DBG_NO_DEATH2))) {
        HSet(pG->pl_life, 2);
        if (type == 6) {
            type = 2;
        }
        if (type == 7) {
            type = 8;
        }
    }
    if ((s16) pG->pl_life <= 0 && type != 6 && type != 7) {
        cPlayer* p;

        pG->pl_life = 0;
        pPLS->dmg.m_Timer = 0x80;
        p = pPL;
        p->r_no_0 = 2;
        p->r_no_1 = 0;
        p->r_no_2 = 0;
        p->r_no_3 = 0;
    } else {
        pPL->setDamage((u8) type, 0, 123.0f, 0, 0xFF);
    }
}

// Never called (dead-stripped by the original linker; only its PI/2 pool entry survives).
static void EmSubDead0(f32* p)
{
    *p = PI / 2.0f;
}

// Attack sphere of `info` at a (from b) against the player (and the partner unless noSub):
// bit0 player hit, bit1 partner hit.
int EmAtkHitCk(EmAtkInfo* info, Vec* pPos, Vec* pPosOld, int noSub)
{
    int ret = 0;
    int hit;
    int keep;
    YARARE_INFO* part;

    hit = EmAtkHitCk2(info, pPos, pPosOld);
    if (hit) {
        keep = 0;
        if (info->flag & 4) {
            keep = 1;
        }
        LifeDownSet2(pPL, info->dmg, 0, keep);
        if (info->flag & 8) {
            pG->pl_life = 0;
        }
        PlSetDamage(hit - 1, 0, 0);
        ret = 1;
    }
    if (noSub) {
        return ret;
    }
    part = EmAtkHitSubCk2(info, pPos, pPosOld);
    if (part) {
        pSUB->dmg.set(0, 10, 0x18, pPosOld, part->rad, part);
        ret |= 2;
    }
    return ret;
}

// Attack sphere against the player: 0 = miss, else the damage motion type + 1 (front/back, and the
// height: 4 low, 2 middle).
int EmAtkHitCk2(EmAtkInfo* info, Vec* pPos, Vec* pPosOld)
{
    Vec d;
    Vec fwd;
    cModel* parts;
    YARARE_INFO* part;
    int ret;
    f32 dy;

    if (DbgFlagChk(pG, DBG_YARARE_DISP)) {
        Draw_sphere(pPos, info->range, 0xFFFF00FF, 1, 1);
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (EmIsDead(pPL)) {
        return 0;
    }
    parts = pPL->getPartsPtr(0);
    if (EatMgr.hitCheck(&parts->world, pPos, 0, 0, 0, 0) != 0) {
        return 0;
    }
    part = emSphereAtCk(pPL, pPos, pPosOld, info->range, 0x18, info->range);
    if (part == 0) {
        return 0;
    }
    MaskAnd16(part->flags, 0xBFFF);
    PSet(pPL->dmg.m_pDamageYarare, part);
    if ((pPos->x - pPosOld->x) * (pPos->x - pPosOld->x) + (pPos->z - pPosOld->z) * (pPos->z - pPosOld->z) < 10000.0f) {
        PSVECSubtract(&pPL->pos, pPos, &d);
    } else {
        PSVECSubtract(pPos, pPosOld, &d);
    }
    fwd.x = 0.0f;
    fwd.y = 0.0f;
    fwd.z = 1.0f;
    RotVector(&fwd, &pPL->ang, &fwd);
    ret = PSVECDotProduct(&fwd, &d) >= 0.0f;
    dy = pPos->y - pPL->pos.y;
    if (dy < 800.0f) {
        ret += 4;
    } else if (dy < 1300.0f) {
        ret += 2;
    }
    return ret + 1;
}

// Line a-b against the scenario and the player's hit boxes: the hit box (as the emhit.h cEm* view),
// with the scenario hit in `hit` / `nrm` / `attr`.
cEm* EmAtkLineHitCk(Vec* pPos, Vec* pPos2, Vec* hit, Vec* nrm, u32* attr)
{
    Mtx m;
    Vec d;
    cPlayer* pl;
    cModel* parts;
    YARARE_INFO* part;
    int at;
    f32 len;

    at = EatMgr.hitCheck(pPos, pPos2, hit, nrm, 0, 0x400000);
    if (at) {
        len = (pPos->x - hit->x) * (pPos->x - hit->x) + (pPos->y - hit->y) * (pPos->y - hit->y) + (pPos->z - hit->z) * (pPos->z - hit->z);
    } else {
        *hit = *pPos2;
        len = 1e16f;
        nrm->x = 0.0f;
        nrm->y = 0.0f;
        nrm->z = 0.0f;
    }
    if (attr) {
        *attr = at;
    }
    pl = pPL;
    if (!(pl->be_flag & 1)) {
        return 0;
    }
    if (!(pl->be_flag & 0x20)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (EmIsDead(pl)) {
        return 0;
    }
    PSVECSubtract(pPos2, pPos, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, pPos);
    if (PSMTXInverse(m, m) == 0) {
        PSMTXIdentity(m);
    }
    parts = pl->getPartsPtr(0);
    PSMTXMultVec(m, &parts->world, &d);
    if (d.z < -10000.0f) {
        return 0;
    }
    if (d.x > 10000.0f) {
        return 0;
    }
    if (d.x < -10000.0f) {
        return 0;
    }
    part = emLineAtCk(pl, pPos, pPos2, len, 0x18);
    if (part == 0) {
        return 0;
    }
    part->flags |= 0x4000;
    return (cEm*) part;
}

// EmAtkLineHitCk for the partner.
YARARE_INFO* EmAtkLineHitCkSub(Vec* pPos, Vec* pPos2, Vec* hit, Vec* nrm)
{
    Mtx m;
    Vec d;
    cSubChar* sub;
    cModel* parts;
    YARARE_INFO* part;
    f32 len;

    if (pSUB == 0) {
        return 0;
    }
    if (EatMgr.hitCheck(pPos, pPos2, hit, nrm, 0, 0x400000)) {
        len = (pPos->x - hit->x) * (pPos->x - hit->x) + (pPos->y - hit->y) * (pPos->y - hit->y) + (pPos->z - hit->z) * (pPos->z - hit->z);
    } else {
        *hit = *pPos2;
        len = 1e16f;
        nrm->x = 0.0f;
        nrm->y = 0.0f;
        nrm->z = 0.0f;
    }
    sub = pSUB;
    if (!(sub->be_flag & 1)) {
        return 0;
    }
    if (!(sub->be_flag & 0x20)) {
        return 0;
    }
    if ((s16) pG->ashley_life <= 0) {
        return 0;
    }
    if (EmIsDead(sub)) {
        return 0;
    }
    PSVECSubtract(pPos2, pPos, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, pPos);
    if (PSMTXInverse(m, m) == 0) {
        PSMTXIdentity(m);
    }
    parts = sub->getPartsPtr(0);
    PSMTXMultVec(m, &parts->world, &d);
    if (d.z < -10000.0f) {
        return 0;
    }
    if (d.x > 10000.0f) {
        return 0;
    }
    if (d.x < -10000.0f) {
        return 0;
    }
    part = emLineAtCk(sub, pPos, pPos2, len, 0x18);
    if (part == 0) {
        return 0;
    }
    part->flags |= 0x4000;
    return part;
}

// Damage from a line attack that hit the player's box `part`: life loss and the damage motion.
void EmAtkSetDamagePL(cEm* part, EmAtkInfo* info, Vec* pPos, Vec* pPos2)
{
    Vec d;
    Vec fwd;
    int type;
    int keep;
    f32 dy;

    PSet(pPL->dmg.m_pDamageYarare, (YARARE_INFO*) part);
    if ((pPos->x - pPos2->x) * (pPos->x - pPos2->x) + (pPos->z - pPos2->z) * (pPos->z - pPos2->z) < 10000.0f) {
        PSVECSubtract(&pPL->pos, pPos, &d);
    } else {
        PSVECSubtract(pPos2, pPos, &d);
    }
    fwd.x = 0.0f;
    fwd.y = 0.0f;
    fwd.z = 1.0f;
    RotVector(&fwd, &pPL->ang, &fwd);
    type = PSVECDotProduct(&fwd, &d) >= 0.0f;
    dy = pPos->y - pPL->pos.y;
    if (dy < 800.0f) {
        type += 4;
    } else if (dy < 1300.0f) {
        type += 2;
    }
    keep = 0;
    if (info->flag & 4) {
        keep = 1;
    }
    LifeDownSet2(pPL, info->dmg, 0, keep);
    if (info->flag & 8) {
        pG->pl_life = 0;
    }
    PlSetDamage(type, 0, 0);
}

// Damage from a line attack that hit the partner's box `part`.
void EmAtkSetDamageSub(YARARE_INFO* part, EmAtkInfo* info, Vec* pPos, Vec* pPos2)
{
    if (pSUB) {
        pSUB->dmg.set(0, 10, 0x18, pPos, part->rad, part);
    }
}

// Attack sphere against the partner: the hit box or NULL.
YARARE_INFO* EmAtkHitSubCk2(EmAtkInfo* info, Vec* pPos, Vec* pPosOld)
{
    cModel* parts;
    YARARE_INFO* part;

    if (DbgFlagChk(pG, DBG_YARARE_DISP)) {
        Draw_sphere(pPos, info->range, 0xFFFF00FF, 1, 1);
    }
    if (pSUB == 0) {
        return 0;
    }
    if (pSUB->hp <= 0) {
        return 0;
    }
    if (EmIsDead(pSUB)) {
        return 0;
    }
    parts = pSUB->getPartsPtr(0);
    if (EatMgr.hitCheck(&parts->world, pPos, 0, 0, 0, 0) != 0) {
        return 0;
    }
    part = emSphereAtCk(pSUB, pPos, pPosOld, info->range, 0x18, info->range);
    if (part == 0) {
        return 0;
    }
    part->flags &= ~0x4000;
    return part;
}

// Start the catch: turn the enemy and the player to face each other (ang offset for the player),
// place the player at (x, y, z) in front of the enemy and run SetPlDamage(a).
void EmCatchPLSet(cEm* em, f32 ang, u32 type, int a, f32 x, f32 y, f32 z)
{
    Mtx m;
    Vec p;
    Vec d;
    f32 r;

    r = em->ang.y;
    r = LIMIT_ANGLE(r + Muku(&em->pos, &pPL->pos, r, PI));
    FSet(em->catchTurn, Muku2(em->ang.y, r, PI));
    r = pPL->ang.y;
    r += Muku(&pPL->pos, &em->pos, r, PI);
    r = LIMIT_ANGLE(r + ang);
    FSet(pPL->catchTurn, Muku2(pPL->ang.y, r, PI));
    PSMTXRotRad(m, 'y', LIMIT_ANGLE(pPL->ang.y + pPL->catchTurn));
    TransMatrix(m, &pPL->pos);
    p.x = x;
    p.y = y;
    p.z = z;
    PSMTXMultVec(m, &p, &p);
    PSVECSubtract(&p, &em->pos, &d);
    pPL->atari.setPriority(PRI_LV1);
    em->atari.setPriority(PRI_LV1);
    switch (type) {
    case 0:
    default:
        FSet(pPL->catchOfs.x, 0.0f);
        FSet(pPL->catchOfs.y, 0.0f);
        FSet(pPL->catchOfs.z, 0.0f);
        em->catchOfs = d;
        break;
    case 1:
        PSVECScale(&d, &pPL->catchOfs, -1.0f);
        em->catchOfs.x = 0.0f;
        em->catchOfs.y = 0.0f;
        em->catchOfs.z = 0.0f;
        break;
    case 2:
        PSVECScale(&d, &d, 0.5f);
        PSVECScale(&d, &pPL->catchOfs, -1.0f);
        em->catchOfs = d;
        break;
    }
    em->x3A8 = em->pos;
    pPL->x3A8 = pPL->pos;
    PSet(em->pEmCatch, pPLS);
    PSet(pPL->pEmCatch, em);
    pPL->subArc = em->subArc;
    SetPlDamage(em, (void (*)(cPlayer*)) a);
}

// Never called (dead-stripped by the original linker; only its PI pool entry survives).
static void EmSubDead1(f32* p)
{
    *p = PI;
}

// EmCatchPLSet for the partner `sub`, in the enemy's frame.
static void EmCatchSubSet(cEm* em, cEm* sub, u32 type, int a, f32 ang, f32 x, f32 y, f32 z)
{
    Mtx m;
    Vec p;
    Vec d;
    f32 r;

    r = em->ang.y;
    r = LIMIT_ANGLE(r + Muku(&em->pos, &sub->pos, r, PI));
    em->catchTurn = Muku2(em->ang.y, r, PI);
    r = sub->ang.y;
    r += Muku(&sub->pos, &em->pos, r, PI);
    r = LIMIT_ANGLE(r + ang);
    sub->catchTurn = Muku2(sub->ang.y, r, PI);
    PSMTXRotRad(m, 'y', LIMIT_ANGLE(em->ang.y + em->catchTurn));
    TransMatrix(m, &em->pos);
    p.x = x;
    p.y = y;
    p.z = z;
    PSMTXMultVec(m, &p, &p);
    PSVECSubtract(&p, &sub->pos, &d);
    PSVECScale(&d, &d, -1.0f);
    sub->atari.setPriority(PRI_LV1);
    em->atari.setPriority(PRI_LV1);
    switch (type) {
    case 0:
    default:
        sub->catchOfs.x = 0.0f;
        sub->catchOfs.y = 0.0f;
        sub->catchOfs.z = 0.0f;
        em->catchOfs = d;
        break;
    case 1:
        PSVECScale(&d, &sub->catchOfs, -1.0f);
        em->catchOfs.x = 0.0f;
        em->catchOfs.y = 0.0f;
        em->catchOfs.z = 0.0f;
        break;
    case 2:
        PSVECScale(&d, &d, 0.5f);
        PSVECScale(&d, &sub->catchOfs, -1.0f);
        em->catchOfs = d;
        break;
    }
    em->x3A8 = em->pos;
    sub->x3A8 = sub->pos;
    em->pEmCatch = sub;
    sub->pEmCatch = em;
    sub->subArc = em->subArc;
    SetSubDamage(em, (void*) a);
}

// Per-frame motion of a caught model: follow the catcher's movement, close the catch offset by
// `rate2`, turn by `rate` of the remaining angle.
// One `tmp` for both the rot.y load and the turn step: a pseudo with two deaths is not a local-alloc
// candidate, so it goes to global.c (f13) and neither the `ry = tmp` copy nor the `tmp * rate` product
// is tied into ry / rate by local-alloc; rate then ranks below rate2 (f29 / f30).
int EmCatchMotionMove(cEm* em, f32 rate, f32 rate2)
{
    cEm* target = em->pEmCatch;
    Vec d;
    f32 ry;
    f32 tmp;
    int ret;

    PSVECSubtract(&target->pos, &target->x3A8, &d);
    d.y = 0.0f;
    PSVECAdd(&em->pos, &d, &em->pos);
    PSVECScale(&em->catchOfs, &d, rate2);
    d.y = 0.0f;
    PSVECAdd(&em->pos, &d, &em->pos);
    PSVECSubtract(&em->catchOfs, &d, &em->catchOfs);
    tmp = em->ang.y;
    ry = tmp;
    em->ang.y = ry + em->catchTurn;
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    ret = MotionMove(em, 0);
    tmp = em->catchTurn * rate;
    ry += tmp;
    em->catchTurn -= tmp;
    em->ang.y = ry;
    em->ang.y = LIMIT_ANGLE(em->ang.y);
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->x3A8 = em->pos;
    return ret;
}

// Never called (dead-stripped by the original linker; only its 0.0f pool entry survives).
static void EmSubDead2(f32* p)
{
    *p = 0.0f;
}

// Rack (id 0x45) in the way of `em` moving to `pos` heading `ang`: 0 when one of the rack's
// corner / edge points falls into the box in front of the position.
// The seven compare constants are hoisted by loop.c; 400.0 only in the second loop pass (it is the
// last preheader load and so the highest-priority FPR, f27). Its corner-1 `lis` has savings 2 x life 2,
// which is desirable only while at most three earlier movables were moved in that pass (threshold 71,
// -3 per move, 243 insns): the element address is therefore computed as `off = size * i` first, so
// `lis EmMgr@ha` lives long enough (5 insns) to be hoisted in pass 1 instead of pass 2.
int EmRackCk(cEm* em, Vec* pos, f32 ang)
{
    Vec v;
    Mtx m;
    u32 i;
    cEm* e;
    FREE_EMRACK* w;
    f32 hx;
    f32 hz;
    u32 off;

    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, pos);
    if (PSMTXInverse(m, m) == 0) {
        PSMTXIdentity(m);
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        off = EmMgr.size * i;
        e = (cEm*) ((u8*) EmMgr.pArray + off);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x45) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            25000000.0f) {
            continue;
        }
        w = EMRACK_WK(e);
        hx = w->Size_x + 50.0f;
        hz = w->Size_z + 50.0f;
        v.x = hx;
        v.y = 0.0f;
        v.z = hz;
        PSMTXMultVec(e->mat, &v, &v);
        PSMTXMultVec(m, &v, &v);
        if (v.x < 400.0f && v.x > -400.0f && v.z < 2000.0f && v.z > 0.0f && v.y < 1000.0f && v.y > -1000.0f) {
            return 0;
        }
        v.x = hx;
        v.y = 0.0f;
        v.z = -hz;
        PSMTXMultVec(e->mat, &v, &v);
        PSMTXMultVec(m, &v, &v);
        if (v.x < 400.0f && v.x > -400.0f && v.z < 2000.0f && v.z > 0.0f && v.y < 1000.0f && v.y > -1000.0f) {
            return 0;
        }
        v.x = -hx;
        v.y = 0.0f;
        v.z = hz;
        PSMTXMultVec(e->mat, &v, &v);
        PSMTXMultVec(m, &v, &v);
        if (v.x < 400.0f && v.x > -400.0f && v.z < 2000.0f && v.z > 0.0f && v.y < 1000.0f && v.y > -1000.0f) {
            return 0;
        }
        v.x = -hx;
        v.y = 0.0f;
        v.z = -hz;
        PSMTXMultVec(e->mat, &v, &v);
        PSMTXMultVec(m, &v, &v);
        if (v.x < 400.0f && v.x > -400.0f && v.z < 2000.0f && v.z > 0.0f && v.y < 1000.0f && v.y > -1000.0f) {
            return 0;
        }
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -hz;
        PSMTXMultVec(e->mat, &v, &v);
        PSMTXMultVec(m, &v, &v);
        if (v.x < 400.0f && v.x > -400.0f && v.z < 2000.0f && v.z > 0.0f && v.y < 1000.0f && v.y > -1000.0f) {
            return 0;
        }
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = hz;
        PSMTXMultVec(e->mat, &v, &v);
        PSMTXMultVec(m, &v, &v);
        if (v.x < 400.0f && v.x > -400.0f && v.z < 2000.0f && v.z > 0.0f && v.y < 1000.0f && v.y > -1000.0f) {
            return 0;
        }
    }
    return 1;
}

// Ammunition score of the inventory: the drop tables hold ammo back above it.
int GetBulletPoint()
{
    u32 n;
    int pt;

    n = ItemMgr.bulletNumTotal(4);
    if (pG->stage_no <= 1) {
        pt = n;
    } else {
        pt = n / 2 + 1;
    }
    pt += ItemMgr.bulletNumTotal(0x18) * 2;
    pt += (u32) ItemMgr.bulletNumTotal(0x20) / 5 + 1;
    pt += (u32) ItemMgr.bulletNumTotal(0x6A) / 5 + 1;
    ItemMgr.bulletNumTotal(0x72);
    return pt + 5;
}

// Random ammunition drop by the weapons carried (item id / count), for the current character.
void GetDropBullet(int* id, int* num)
{
    int i = 4;
    int n = 0;  // also holds the handgun ammo total in the third branch (its register: r31 there)
    u8 r;
    u32 f;

    // The bit test as a variable: the `andis.` result stays (cse later reuses it as the zero stored
    // for `*num = 0` on the other path); a plain `flags & 0x80000000` folds to a signed compare.
    f = SysFlagChk(pG, SYS_OMAKE_ADA_GAME);
    if (f) {
        r = Rnd() % 100;
        if (r <= 0x27) {
            *id = i;
            if (Rnd() % 10 > 6) {
                *num = 15;
            } else {
                *num = 10;
            }
            return;
        }
        if (r <= 0x59) {
            i = 0x20;
            n = 25;
            if (Rnd() % 10 > 5) {
                n = 0;
            }
            *id = i;
            *num = n;
            return;
        } else if (r <= 0x5E) {
            i = 7;
            n = 3;
            if (Rnd() % 10 > 7) {
                n = 0;
            }
            *id = i;
            *num = n;
            return;
        } else {
            i = 1;
            *id = i;
            *num = n;
            return;
        }
    } else if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        r = Rnd() % 100;
        switch (pG->pl_type) {
        case 0:
        default:
            if (r <= 0x36) {
                *id = 4;
                *num = 0;
                return;
            }
            if (r <= 0x59) {
            // One copy of the handgun-ammo body: the third branch's `r <= 0x13` arm jumps here (the
            // original's three `bne` land on this block; a duplicated body is cross-jumped arm by arm
            // and its head survives).
            ammo18:
                i = 0x18;
                if ((Rnd() & 0xF) != 5) {
                    n = 5;
                    if (Rnd() % 10 > 4) {
                        n = 3;
                    }
                } else {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            } else {
                i = 1;
                *id = i;
                *num = 0;
                return;
            }
        case 2:
            if (r <= 0x1D) {
                *id = i;
                *num = n;
                return;
            } else if (r <= 0x40) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            } else if (r <= 0x54) {
                i = 2;
                n = 0;
                *id = i;
                *num = n;
                return;
            } else {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            }
        case 3:
            if (r <= 0x4A) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            } else {
                i = 1;
                *id = i;
                *num = n;
                return;
            }
        case 4:
            if (r <= 0x4A) {
                i = 0x72;
                n = 5;
                if (Rnd() % 10 > 5) {
                    n = 10;
                }
                *id = i;
                *num = n;
                return;
            } else {
                i = 0xE;
                *id = i;
                *num = n;
                return;
            }
        case 5:
            if (r <= 0x18) {
                *id = i;
                *num = n;
                return;
            }
            if (r <= 0x45) {
                i = 0;
                n = (Rnd() % 10 <= 7) ? 2 : 0;
                *id = i;
                *num = n;
            }
            if (r <= 0x4F) {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            } else if (r <= 0x59) {
                i = 1;
                *id = i;
                *num = 0;
                return;
            } else if (r <= 0x5E) {
                i = 0xE;
                *id = i;
                *num = 0;
                return;
            } else {
                i = 2;
                *id = i;
                *num = 0;
                return;
            }
        }
    } else {
        n = ItemMgr.bulletNumTotal(4);
        r = Rnd() % 100;
        if (r > 0x28) {
            if (pG->game_mode == 1) {
                if (ItemMgr.num(0x2C) || ItemMgr.num(0x2D) || ItemMgr.num(0x94)) {
                    if (Rnd() % 10 > 4) {
                        i = 0x18;
                        if ((Rnd() & 0xF) != 5) {
                            n = 5;
                            if (Rnd() % 10 > 4) {
                                n = 3;
                            }
                        } else {
                            n = 0;
                        }
                        *id = i;
                        *num = n;
                        return;
                    }
                }
            }
            if ((u32) n <= 0x3B) {
                goto fallback;
            }
        }
        r = Rnd() % 100;
        if (r <= 0x13) {
            if (ItemMgr.num(0x2C) || ItemMgr.num(0x2D) || ItemMgr.num(0x94)) {
                goto ammo18;
            }
        }

        if ((u32) (r - 0x14) <= 0x13) {
            if (ItemMgr.num(0x2E) || ItemMgr.num(0x2F)) {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            }
        }
        if ((u32) (r - 0x28) <= 0x13) {
            if (ItemMgr.num(0x30) || ItemMgr.num(0x31) || ItemMgr.num(0x32) || ItemMgr.num(0x32) || ItemMgr.num(0x33) ||
                ItemMgr.num(0x3E)) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            }
        }
        if ((u32) (r - 0x3C) <= 0x13) {
            if (ItemMgr.num(0x37)) {
                if (Rnd() % 10 > 1) {
                    i = 0x1A;
                    n = (Rnd() % 10 <= 7) ? 2 : 0;
                    *id = i;
                    *num = n;
                    return;
                }
            }
            if (ItemMgr.num(0x29) || ItemMgr.num(0x2A) || ItemMgr.num(0x2B)) {
                i = 0;
                n = (Rnd() % 10 <= 7) ? 2 : 0;
                *id = i;
                *num = n;
                return;
            }
        }
        if ((u32) (r - 0x50) <= 9) {
            if (ItemMgr.num(0x36) || ItemMgr.num(0xAB)) {
                i = 0x46;
                n = (Rnd() % 10 <= 7) ? 2 : 0;
                *id = i;
                *num = n;
                return;
            }
        }
        if ((u32) (r - 0x5A) <= 9) {
            switch (Rnd() % 3) {
            default:
                i = 1;
                break;
            case 1:
                i = 2;
                break;
            case 2:
                i = 0xE;
                break;
            }
            *id = i;
            // Codeless launder: keeps the zero and its store from being cross-jumped two insns deep
            // into the fallback's else arm (see `fallback:`); the `*num` input is an anti-dependence
            // on the store, so sched2 still issues the `*id` store first.
            int z = 0;
            asm("" : "+r"(z) : "m"(*num));
            *num = z;
            return;
        }
        if (ItemMgr.num(0x2C) || ItemMgr.num(0x2D) || ItemMgr.num(0x94)) {
            if (Rnd() % 10 > 2) {
                i = 0x18;
                if ((Rnd() & 0xF) != 5) {
                    n = 5;
                    if (Rnd() % 10 > 4) {
                        n = 3;
                    }
                } else {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            }
        }
        if (ItemMgr.num(0x30) || ItemMgr.num(0x31) || ItemMgr.num(0x32) || ItemMgr.num(0x32) || ItemMgr.num(0x33) ||
            ItemMgr.num(0x3E)) {
            if (Rnd() % 10 > 4) {
                i = 0x20;
                n = 25;
                if (Rnd() % 10 > 5) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            }
        }
        if (ItemMgr.num(0x2E) || ItemMgr.num(0x2F)) {
            if (Rnd() % 10 > 4) {
                i = 7;
                n = 3;
                if (Rnd() % 10 > 7) {
                    n = 0;
                }
                *id = i;
                *num = n;
                return;
            }
        }
        if (ItemMgr.num(0x36)) {
            if (Rnd() % 10 > 4) {
                i = 0x46;
                n = (Rnd() % 10 <= 7) ? 4 : 0;
                *id = i;
                *num = n;
                return;
            }
        }
        if (ItemMgr.num(0x29) || ItemMgr.num(0x2A) || ItemMgr.num(0x2B) || ItemMgr.num(0x37)) {
            if (ItemMgr.num(0x37)) {
                if (Rnd() % 10 > 4) {
                    i = 0x1A;
                    n = (Rnd() % 10 <= 7) ? 2 : 0;
                    *id = i;
                    *num = n;
                    return;
                }
            }
            if (ItemMgr.num(0x29) || ItemMgr.num(0x2A) || ItemMgr.num(0x2B)) {
                if (Rnd() % 10 > 4) {
                    i = 0;
                    n = (Rnd() % 10 <= 7) ? 2 : 0;
                    *id = i;
                    *num = n;
                    return;
                }
            }
        }
        if (Rnd() % 10 > 7) {
            switch (Rnd() % 3) {
            default:
                i = 1;
                break;
            case 1:
                i = 2;
                break;
            case 2:
                i = 0xE;
                break;
            }
            *id = i;
            // Codeless launder: keeps the zero and its store from being cross-jumped two insns deep
            // into the fallback's else arm (see `fallback:`); the `*num` input is an anti-dependence
            // on the store, so sched2 still issues the `*id` store first.
            int z = 0;
            asm("" : "+r"(z) : "m"(*num));
            *num = z;
            return;
        }
    fallback:
        // `li r0,0; ble; li r0,0x14; stw`: jump.c's post-reload "if (..) x = a; else x = b" hoist, after
        // the then-arm's store cross-jumped into the else arm's store (a fresh label, so the two
        // `Rnd() % 3` sites above keep their own `li r0,0`). A plain `*num = 0` there is matched two
        // insns deep by those sites first (their `li r0,0; stw` tails), which pins their jumps on the
        // else label and blocks the hoist; the codeless launder between each site's zero and its
        // store (see `z` above) is the insn find_cross_jump compares against this `li`, so the sites
        // match one insn (the store) only. COMPILER-DIFF: candidate (jump2 cross-jump order vs the
        // x = a / x = b hoist)
        *id = 4;
        if (pG->stage_no > 1) {
            *num = 20;
        } else {
            *num = 0;
        }
    }
}

// Healing score of the inventory: herbs and sprays, the first aid spray counting double.
int GetRecoveryPoint()
{
    int pt;

    pt = ItemMgr.num(6);
    pt += ItemMgr.num(5);
    pt += ItemMgr.num(0x12);
    pt += ItemMgr.num(0x13);
    pt += ItemMgr.num(0x15);
    pt += ItemMgr.num(0x14);
    pt += ItemMgr.num(0x16) * 2;
    return pt;
}

// Drop the enemy's item (setItem) at its position, once: flagged items go through the system item
// table, item 0 rolls a random drop.
void EmSetDropItem(cEm* em)
{
    Vec rot;
    int id;
    int num;

    if (em->Item_id == 0xFFFF) {
        return;
    }
    if (em->be_flag & 0x10000) {
        return;
    }
    em->be_flag |= 0x10000;
    if (em->Item_id != 0) {
        if (SceAtItemFlgCk(em->Item_flg, em->Auto_item_flg)) {
            return;
        }
        SceAtItemFlgOn(em->Item_flg, em->Auto_item_flg);
        rot.x = 0.0f;
        rot.y = em->ang.y;
        rot.z = 1.0f;
        if (SceAtCheckSystemItemSet(em->Item_id, &id, &num, &em->pos, &rot) != 1) {
            return;
        }
        if (em->Item_id != id) {
            em->Item_id = id;
            em->Item_num = num;
        }
        if (TrolleyItemSetCk(&em->pos, em->Item_id, em->Item_num)) {
            return;
        }
        if (BullItemSetCk(&em->pos, em->Item_id, em->Item_num)) {
            return;
        }
        SceAtCancelItemAt((int) em);
        SceAtCreateItemAt(&em->pos, em->Item_id, em->Item_num, (s8) em->itemFlag, -1, 0, -1);
    } else {
        RandomItemSet(em);
    }
}

// Reserve the enemy's item drop at its position (the enemy leaves before dying).
void EmReserveDropItem(cEm* em)
{
    int id;
    int num;

    if (em->Item_id == 0xFFFF) {
        return;
    }
    if (em->be_flag & 0x10000) {
        return;
    }
    if (em->Item_id == 0) {
        return;
    }
    if (SceAtCheckSystemItemSet(em->Item_id, &id, &num, (Vec*) &vecZero, (Vec*) &vecZero) != 1) {
        return;
    }
    if (em->Item_id != id) {
        em->Item_id = id;
        em->Item_num = num;
    }
    SceAtReserveItemAt((int) em, &em->pos, em->Item_id, em->Item_num, (s8) em->itemFlag, -1);
}

// Random drop for the enemy type (RandomItemCk) placed at the enemy.
void RandomItemSet(cEm* em)
{
    int id;
    int num;

    if (RandomItemCk(em->id, &id, &num, 0) != 1) {
        return;
    }
    if (TrolleyItemSetCk(&em->pos, id, num)) {
        return;
    }
    if (BullItemSetCk(&em->pos, id, num)) {
        return;
    }
    SceAtCreateItemAt(&em->pos, id, num, -1, -1, 0, -1);
}

// Random drop table by enemy id: money (0x78), ammunition (GetDropBullet), healing items (5/6/0x19),
// or the treasure of the special enemies; 1 with the item in outId / outNum.
// Handgun ammo drop: four dice (the second offset by `base`) times 5, rounded down to tens; a 1/64
// chance of `big` (or 330). Inline with the offsets as parameters: the `+ base` reaches RTL as a
// separate add (fold would otherwise fold the literal into the sum) and the four Rnd() calls of one
// expression are pre-expanded before any of the `% 6`.
// The result goes through a reference to the caller's `num`: every site (and the other cases) then
// sets ONE global pseudo, whose global.c preference comes from the sum insn's first operand (the
// first-dice chain, local-alloc r29), so num shares r29; with an own local per inline copy each num
// took the first free register (r30). The `* 5` is a separate statement: inside the sum the
// preference would come from the `slwi` scratch (r0) instead.
static inline void RandomHandgunAmmo(u32& num, int base, int big)
{
    num = (u8) (Rnd() % 6) + ((u8) (Rnd() % 6) + base) + (u8) (Rnd() % 6) + (u8) (Rnd() % 6);
    num *= 5;
    num = num / 10 * 10;
    if ((Rnd() & 0x3F) == 0x1E) {
        num = big;
        if (Rnd() & 0xF) {
            num = 330;
        }
    }
}

// Random enemy drop table by enemy id: rolls whether enemy `id` drops anything (ganados ~20%
// handgun ammo, then a ~60% chance of a table item with a "no drop" streak breaker No_drop_cnt;
// flag bit0 forces a drop) and picks the item id / count from the ammo (GetBulletPoint) and
// recovery (GetRecoveryPoint) point budgets. 1 and *outId / *outNum when something drops.
int RandomItemCk(int id, int* outId, int* outNum, int flag)
{
    u8 r;
    u8 r0;
    int bullet;
    int recov;
    int itemId;
    u32 num;
    u32 lim;

    r = Rnd() % 100;
    bullet = GetBulletPoint();
    recov = GetRecoveryPoint();
    switch (id) {
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
    case 0x20:
    case 0x22:
    case 0x36:
        if (r <= 0x13) {
            if (SysFlagChk(pG, SYS_OMAKE_ADA_GAME)) {
                return 0;
            }
            if (SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
                return 0;
            }
            switch (id) {
            case 0x11:
            case 0x14:
            case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F: case 0x20:
            case 0x22:
            case 0x36:
                itemId = 0x78;
                RandomHandgunAmmo(num, 20, 1980);
                *outId = itemId;
                *outNum = num;
                return 1;
            default:
                itemId = 0x78;
                RandomHandgunAmmo(num, 10, 990);
                *outId = itemId;
                *outNum = num;
                return 1;
            }
        }
        /* fallthrough */
    case 0x25:
        if (r <= 0x3B || (No_drop_cnt > 2 && Rnd() % 10 > 4) || No_drop_cnt > 5 || (flag & 1)) {
            No_drop_cnt = 0;
        } else {
            No_drop_cnt++;
            return 0;
        }
        break;
    case 0x23:
        itemId = 0x78;
        num = (u8) (Rnd() % 3) * 10 + 20;
        if ((Rnd() & 3) == 0) {
            if (Rnd() & 1) {
                num = (Rnd() & 1) * 10 + 10;
            } else {
                num = (Rnd() & 1) * 10 + 40;
            }
        }
        *outId = itemId;
        *outNum = num;
        return 1;
    case 0x3C:
        if (r > 0x28) {
            return 0;
        }
        break;
    case 0x2D:
        if (r > 0x3C) {
            r0 = Rnd() % 100;
            itemId = 0xB9;
            if (r0 <= 0x31) {
                itemId = 0xBA;
            }
            if (r0 <= 0xC) {
                itemId = 0xBB;
            }
            *outId = itemId;
            num = 1;
            *outNum = num;
            return 1;
        }
        break;
    case 0x3A:
        break;
    default:
        return 0;
    }
    // The bullet thresholds go through the `lim` variable: a literal `< 30` is folded to `<= 29`
    // (`cmplwi 0x1d; bgt`), the variable keeps `cmplwi 0x1e; bge`.
    lim = 30;
    if (pG->pl_type != 1 && (u32) bullet < lim && Rnd() % 10 > 4) {
        GetDropBullet(outId, outNum);
        return 1;
    }
    lim = 1;
    if (pG->Game_level <= 2) {
        lim = 3;
    }
    if (pG->Game_level > 7) {
        lim = 0;
    }
    if ((u32) recov <= lim) {
        if (recov == 0) {
            No_drop_cnt2++;
        }
        if (Rnd() % 10 > 7 || No_drop_cnt2 > 2) {
            if ((s16) pG->pl_life <= 500) {
                itemId = 5;
                if (Rnd() % 100 > 0x18) {
                    itemId = 6;
                }
            } else {
                itemId = 6;
                if (ItemMgr.num(6) != 0 && Rnd() % 100 > 0x4A) {
                    itemId = 0x19;
                }
            }
            num = 0;
            No_drop_cnt2 = 0;
            *outId = itemId;
            *outNum = num;
            return 1;
        }
    }
    if (pG->pl_type == 1) {
        return 0;
    }
    lim = 0x96;
    if ((u32) bullet < lim) {
        GetDropBullet(outId, outNum);
        return 1;
    }
    if (id == 0x2D) {
        // The else arm starts with the Rnd() call: with a plain `itemId = 0xBB` first, jump.c hoists
        // the `itemId = 0xB9` of the then arm above the test.
        if (Rnd() & 3) {
            itemId = 0xB9;
        } else if (Rnd() & 3) {
            itemId = 0xBA;
        } else {
            itemId = 0xBB;
        }
        *outId = itemId;
        num = 1;
        *outNum = num;
        return 1;
    }
    if (flag & 1) {
        itemId = 0x78;
        RandomHandgunAmmo(num, 20, 990);
        *outId = itemId;
        *outNum = num;
        return 1;
    }
    return 0;
}

// Model under the water surface (parts `parts` no more than 300 above it when given).
int CheckInWater(cModel* m, int parts)
{
    f32 h;

    if (GetWaterHeight(&m->pos, &h) == 0 || m->pos.y > h) {
        return 0;
    }
    if (parts != 0) {
        if (m->getPartsPtr(parts)->world.y + 300.0f < h) {
            return 0;
        }
    }
    return 1;
}

// Weapon ids the hit boxes flagged 0x10 ignore (handguns, the TMP and the knife-like weapons).
int HandgunCk(int wep)
{
    // Each group has its own `return 1`: the distinct case labels make the switch tree emit the
    // greater-than side inline (`beq; ble left; ...`), a shared body emits the left side first.
    switch (wep) {
    case 1:
    case 2:
    case 3:
    case 4:
        return 1;
    case 0x11:
        return 1;
    case 0x26:
        return 1;
    case 0x2B:
        return 1;
    }
    return 0;
}

// Position of `em` (the player when NULL) plus `t` of its parts 0 movement this frame.
void GetPlPos(Vec* out, cEm* em, f32 t)
{
    Vec d;
    cModel* parts;

    if (em == 0) {
        em = pPL;
    }
    parts = em->getPartsPtr(0);
    PSVECSubtract(&parts->world, &parts->world_old2, &d);
    PSVECScale(&d, &d, t);
    PSVECAdd(&em->pos, &d, out);
}

// Item dropped on the mine cart (room 21B): created on the cart the position is above; 1 when so.
int TrolleyItemSetCk(Vec* pos, u16 id, int num)
{
    Vec out;
    u8 parts;
    u32 i;
    cObj* obj;

    if (pG->room_id != 0x21B) {
        return 0;
    }
    for (i = 0; i < ObjMgr.nArray; i++) {
        obj = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
        if ((obj->be_flag & 0x201) != 1) {
            continue;
        }
        if (obj->id != 0x3B) {
            continue;
        }
        if (((cObjTrolley*) obj)->ckTrolleyRide(pos, &parts, &out)) {
            SceAtCreateItemAt(&out, id, num, -1, -1, obj, parts);
            return 1;
        }
    }
    return 0;
}

// Item dropped on the bulldozer (room 30F).
int BullItemSetCk(Vec* pos, u16 id, int num)
{
    Vec out;
    u8 parts;
    u32 i;
    cObj* obj;

    if (pG->room_id != 0x30F) {
        return 0;
    }
    for (i = 0; i < ObjMgr.nArray; i++) {
        obj = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
        if ((obj->be_flag & 0x201) != 1) {
            continue;
        }
        if (obj->id != 0x3E) {
            continue;
        }
        if (((cObjBull*) obj)->ckBullRide(pos, &parts, &out)) {
            SceAtCreateItemAt(&out, id, num, -1, -1, obj, parts);
            return 1;
        }
    }
    return 0;
}

// Sets the offset VehicleAdjust adds to positions on the bulldozer stage.
void adjust_add_set(Vec* v)
{
    adjust_add = *v;
}

// Move `pos` with the vehicle it stands on (mine cart in room 21B, bulldozer in room 30F): 1 when a
// vehicle adjusted it; on the bulldozer stage the vehicle offset is added otherwise.
int VehicleAdjust(Vec* pos)
{
    Vec out;
    cObj* obj;

    if (pG->room_id == 0x21B) {
        for (obj = ObjMgr.pAlive; obj != 0; obj = (cObj*) obj->pNext) {
            if (obj->id != 0x3B) {
                continue;
            }
            if (((cObjTrolley*) obj)->ckTrolleyRideAdjust(pos, &out)) {
                *pos = out;
                return 1;
            }
        }
    }
    if (pG->room_id == 0x30F) {
        for (obj = ObjMgr.pAlive; obj != 0; obj = (cObj*) obj->pNext) {
            if (obj->id != 0x3E) {
                continue;
            }
            if (((cObjBull*) obj)->ckBullRideAdjust(pos, &out)) {
                *pos = out;
                return 1;
            }
        }
        PSVECAdd(&adjust_add, pos, pos);
    }
    return 0;
}
