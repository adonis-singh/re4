// game/pl_wep.cpp: player weapon control: weapon object release/load, hit checks, lock-on
// target search, aim control (PlWepLockCtrl), auto tracking, water shots.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "player.h"
#include "pl_sub.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "joy.h"
#include "item.h"
#include "snd.h"
#include "esp.h"
#include "obj.h"
#include "cam_ctrl.h"
#include "rnd.h"
#include "math_sub.h"
#include "game.h"
#include "est.h"
#include "read.h"

// GetWepTargetList entry (em_sub.cpp).
struct WepTarget {
    cEm* em;
    YARARE_INFO* part;
};

extern "C" {
u32 GetWepTargetListBomb(Vec* pos, WepTarget* list, u32 prio, int type, int flag, f32 len);  // game/em_sub.cpp (defined with another parameter list; this unit's prototype stays)
u32 GetWepTargetList2(Vec* p0, Vec* p1, WepTarget* list, u32 prio, Vec* hit, Vec* nrm, u32* attr, int type,
                      int flag, f32 len);
f32 rangeDist(Vec* pos, cEm* em, f32 range);
int lockEmCk(cEm* em, Vec* pos);
cEm* searchLockEm(Vec* pos, cEm* skip, f32 range);
int cnCkSub(Vec* pos, Vec* nrm, f32 len, Vec* outA, Vec* outB);
void wepSetWaterShot(Vec* p0, Vec* p1, u8 type);
void setWaterShot(Vec* pos);
}

void (*WeaponInitFunc)(cModel*) = 0;
u8 lockCtr;
static f32 lockRandCtr;

// No weapon objects yet.
cPlWep::cPlWep()
{
    m_pWep = 0;
    m_pWepHand = 0;
    m_EmRankPtr = 0;
}

// Drops the current weapon: the muzzle-flash objects (id 0xA), both weapon objects (destroyNow),
// the aim camera, the weapon's effect data (by weapon_no_old, unless stat bit0 says the
// data is shared) and the player-owned effects.
void cPlayer::weaponRelease()
{
    cObj* obj;
    cObj* objCur;
    cObj* next;

    // Guarded do-while testing `next` (a different pseudo than `obj`) at the bottom: jump2 cannot
    // merge the two tests, so the loop keeps the rotated shape with the entry test.
    obj = ObjMgr.getActiveWork();
    if (obj) {
        do {
            objCur = obj;
            next = ObjMgr.getNext(objCur);
            obj = next;
            if (objCur->id == 0xA) {
                ObjMgr.destroy(objCur);
            }
        } while (next);
    }
    if (Wep->m_pWep) {
        ObjMgr.destroyNow((cObj*) Wep->m_pWep);
        Wep->m_pWep = 0;
    }
    if (Wep->m_pWepHand) {
        ObjMgr.destroyNow((cObj*) Wep->m_pWepHand);
        Wep->m_pWepHand = 0;
    }
    endCamera();
    if ((stat.check(cPlayer::F_NO_WEP_EFF)) == 0) {
        switch (pG->weapon_no_old) {
        case 0:
            break;
        case 1:
            EspDataRelease(EFF_WEP01, 1, 1);
            break;
        case 2:
        case 3:
        case 0x12:
            EspDataRelease(EFF_WEP02, 1, 1);
            break;
        case 4:
            EspDataRelease(EFF_WEP04, 1, 1);
            break;
        case 5:
            EspDataRelease(EFF_WEP05, 1, 1);
            break;
        case 6:
            EspDataRelease(EFF_WEP06, 1, 1);
            break;
        case 7:
        case 0x21:
            EspDataRelease(EFF_WEP07, 1, 1);
            break;
        case 8:
            EspDataRelease(EFF_WEP08, 1, 1);
            break;
        case 9:
            EspDataRelease(EFF_WEP09, 1, 1);
            break;
        case 0xA:
            EspDataRelease(EFF_WEP10, 1, 1);
            break;
        case 0xB:
        case 0x14:
            EspDataRelease(EFF_WEP11, 1, 1);
            break;
        case 0xC:
            EspDataRelease(EFF_WEP12, 1, 1);
            break;
        case 0xD:
            EspDataRelease(EFF_WEP13, 1, 1);
            break;
        case 0xE:
            EspDataRelease(EFF_WEP14, 1, 1);
            break;
        case 0xF:
            EspDataRelease(EFF_WEP15, 1, 1);
            break;
        case 0x10:
            EspDataRelease(EFF_WEP16, 1, 1);
            break;
        case 0x11:
            EspDataRelease(EFF_WEP17, 1, 1);
            break;
        case 0x13:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1F:
        case 0x20:
            EspDataRelease(EFF_WEP19, 1, 1);
            break;
        case 0x1C:
            EspDataRelease(EFF_WEP28, 1, 1);
            break;
        }
    }
    stat.off(cPlayer::F_NO_WEP_EFF);
    EffectEspDelete(0, ESP_CORE_KIND_PL_WEP, this, 0);
    EffectEspgenDelete(0, ESP_CORE_KIND_PL_WEP, this);
    EffectEfmDelete(0, ESP_CORE_KIND_PL_WEP, this);
}

// Sets weapon_no / weapon_type and reads the weapon module's data (ReadWepData).
void cPlayer::weaponLoad(int wep_id, int wep_type)
{
    pG->weapon_no = wep_id;
    pG->weapon_type = wep_type;
    ReadWepData(wep_id, wep_type);
}

// Clears the weapon part of the motion table (0..0x5E) and lets the loaded weapon module fill it
// (WeaponInitFunc); back to routine 0/0 with a footwork unless carried / crouching.
void cPlayer::weaponInit()
{
    int i;

    for (i = 0; i < 0x5F; i++) {
        m_MotTbl[i] = 0;
    }
    if (WeaponInitFunc) {
        WeaponInitFunc(this);
    }
    if (!StaFlagChk(pG, STA_PL_BOAT) && !(stat.check(cPlayer::F_CROUCH))) {
        r_no_0 = 0;
        r_no_1 = 0;
        r_no_2 = 0;
        r_no_3 = 1;
        m_Hokan = 0;
        m_Frame = 0;
    }
}

// Dead-stripped by the original linker (STRIP_UNUSED): its pool (-1, 0, 0.25) opens the unit's
// constants, right before PlWepHitCheck2's.
static f32 wepRate(cPlWep* w)
{
    if (w->m_EmRankPtr) {
        return -1.0f;
    }
    if (w->pitch > 0.0f) {
        return 0.25f;
    }
    return w->pitch;
}

// The player's weapon hit: `type` = damage kind (0..0x11 guns / knife 0x10, 0x12 blast, 0x13
// grenade, 0x17 flash, 0x19 egg, ...), priority from the weapon's power level; a radius search
// around pPos for the blast types, else the line pPos-pPos2 (GetWepTargetList2) up to `len`.
// Every enemy hit gets dmg.set(type); the map hit spawns the surface effect / bell noise and water
// shots unless flag bit0 (no map effects); flag bit1 = don't count / score the shot, bit2 = f4
// (headshot-capable line); a miss gives the shooting-range points. Returns the number of enemies hit.
u32 PlWepHitCheck2(cModel* pPl, Vec* pPos, Vec* pPos2, int weapon_no, u32 flag, f32 radius)
{
    cPlayer* pl = (cPlayer*) pPl;
    WepTarget list[20];
    Vec hit;
    Vec nrm;
    u32 attr;
    f32 wh;
    u32 prio;
    int f4;
    u32 n;
    u32 i;

    // The decision tree (root 0xF, left root 7, right root 0x17, compares 3/1/2, 5, 0xB/9/0xD, 0x13/0x11/
    // 0x15, 0x1F/0x1A/0x19/0x1C, 0x21/0x2D) is the balanced tree over 29 SEPARATE case nodes: every value
    // has its own body (identical `prio = 1` bodies are only merged by the post-reload cross-jump), so
    // no two consecutive values share a label; 4/8/0xC share one, placed AFTER 5/6 (body layout = source
    // order). `case 7:` sits on the `default:` body: the 7 leaf is then a block also reached from the right
    // (> 0xF) subtree where `cmpwi cr7,type,0x17` (the second switch's compare, PRE-shared with the 0x17
    // root) is already available, so gcse's block LCM cannot delay the compare into it and inserts it at
    // the end of the left-root block instead of in every left-side leaf.
    switch (weapon_no) {
    case 1:
        if (pG->weapon_lv_power > 6) {
            prio = 5;
        } else {
            prio = 2;
        }
        break;
    case 2:
        prio = 1;
        break;
    case 3:
        prio = 1;
        break;
    case 5:
        prio = 3;
        break;
    case 6:
        prio = 3;
        break;
    case 4:
    case 8:
    case 0xC:
        prio = 1;
        break;
    case 9:
        prio = 5;
        break;
    case 0xA:
        prio = 5;
        break;
    case 0xB:
        prio = 1;
        break;
    case 0xD:
        prio = 1;
        break;
    case 0xE:
        prio = 1;
        break;
    case 0xF:
        prio = 5;
        break;
    case 0x10:
        prio = 0x14;
        break;
    case 0x11:
        prio = 1;
        break;
    case 0x12:
        prio = 0x14;
        break;
    case 0x13:
        prio = 0x14;
        break;
    case 0x14:
        prio = 0x14;
        break;
    case 0x15:
        prio = 1;
        break;
    case 0x16:
        prio = 0x14;
        break;
    case 0x17:
        prio = 0x14;
        break;
    case 0x19:
        prio = 1;
        break;
    case 0x1A:
        prio = 1;
        break;
    case 0x1C:
        prio = 1;
        break;
    case 0x1F:
        prio = 1;
        break;
    case 0x20:
        prio = 1;
        break;
    case 0x21:
        prio = 1;
        break;
    case 0x2D:
        prio = 0x14;
        break;
    case 7:
    default:
        prio = 1;
        break;
    }
    if (prio > 0x14) {
        prio = 0x14;
    }
    f4 = 0;
    if (flag & 4) {
        f4 = 1;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    switch (weapon_no) {
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x16:
    case 0x17:
    case 0x29:
    case 0x2D:
        n = GetWepTargetListBomb(pPos, list, prio, weapon_no, f4, radius);
        break;
    default:
        n = GetWepTargetList2(pPos, pPos2, list, prio, &hit, &nrm, &attr, weapon_no, f4, radius);
        break;
    }
    for (i = 0; i < n; i++) {
        cEm* em = list[i].em;
        YARARE_INFO* part = list[i].part;
        cDmgInfo* dmg = &em->dmg;

        switch (weapon_no) {
        case 0x14:
            if (em->id == 3) {
                continue;
            }
            break;
        case 9:
        case 0xA:
            if ((em->id == 3 || em->id == 4) && i != 0) {
                continue;
            }
            break;
        }
        if (!(dmg->m_Flag & 1)) {
            dmg->set(0, 10, weapon_no, pPos, part->len, part);
            if (part->flag & YAT_FLAG_THROUGH) {
                dmg->m_Flag |= 0x20;
            }
        }
        if (list[i].em->id == 0x38) {
            break;
        }
    }
    if (!(flag & 1)) {
        if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
            if (GetWaterHeight(&hit, &wh) == 0 || hit.y > wh) {
                // nested call: `&nrm` is evaluated into a pseudo before EatGetEffectType (`addi r30,r1,..`
                // ahead of the bl)
                EspSetEatEffect(&hit, &nrm, EatGetEffectType(attr), weapon_no);
                StaFlagOn(pG, STA_SE_BURST);
                pG->SeInfo.pos = hit;
                pG->SeInfo.type = 0;
            }
        }
    }
    if (pl != 0 && !(flag & 1)) {
        if (pl->Wep->m_pWep != 0) {
            wepSetWaterShot(pPos, pPos2, weapon_no);
            pG->SeInfo.pos = pl->Wep->m_pWep->m_ShotPos;
            switch (weapon_no) {
            case 0xD:
            case 0x12:
            case 0x13:
                pG->SeInfo.type = 1;
                break;
            default:
                pG->SeInfo.type = 0;
                break;
            }
        }
    }
    if (!(flag & 2) && n == 0) {
        StaFlagOn(pG, STA_PL_MISS_SHOT);
        switch (weapon_no) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 0x11:
            GameAddPoint(4);
            break;
        case 7:
        case 8:
        case 0x21:
            GameAddPoint(5);
            break;
        case 9:
        case 0xA:
            GameAddPoint(7);
            break;
        case 0xB:
        case 0xC: {
            u16 rest = ItemMgr.bulletNumCurrent() % 5;

            if (rest == 0) {
                GameAddPoint(8);
            }
            break;
        }
        case 0xF:
        case 0x13:
            GameAddPoint(6);
            break;
        case 0xD:
        case 0xE:
            break;
        }
    }
    if (pl != 0 && pl->id == 0) {
        switch (PlGetWeaponNo()) {
        case 0:
        case 0xD:
        case 0xE:
        case 0x10:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x16:
        case 0x17:
        case 0x19:
        case 0x1A:
        case 0x1F:
        case 0x20:
            break;
        case 7:
        case 8:
        case 0x21:
            if (flag & 4) {
                break;
            }
        default:
            if (!(flag & 2)) {
                if (n != 0) {
                    pG->c_hit_cnt++;
                    pG->g_hit_cnt++;
                }
                pG->c_shot_cnt++;
                pG->g_shot_cnt++;
            }
            break;
        }
    }
    return n;
}

// Radius damage at `pos` without a shooter: every enemy within `len` gets dmg.set(type) at the
// given priority (max 0x14). Ashley is spared by 0x14, Ashley / Luis by the bow types unless first.
u32 PlWepHitCheck3(Vec* pos, int type, u32 prio, f32 len)
{
    WepTarget list[20];
    u32 n;
    u32 i;

    if (prio > 0x14) {
        prio = 0x14;
    }
    n = GetWepTargetListBomb(pos, list, prio, type, 0, len);
    for (i = 0; i < n; i++) {
        cEm* em = list[i].em;
        cDmgInfo* dmg = &em->dmg;

        switch (type) {
        case 0x14:
            if (em->id == 3) {
                continue;
            }
            break;
        case 9:
        case 0xA:
            if ((em->id == 3 || em->id == 4) && i != 0) {
                continue;
            }
            break;
        }
        YARARE_INFO* part = list[i].part;
        if (!(dmg->m_Flag & 1)) {
            dmg->set(0, 10, type, pos, part->len, part);
        }
    }
    return n;
}

// `pPL` read directly in every test (no `cPlayer* pl` local): the `&&` join block cannot be
// reached by cse, so gcse re-loads pPL at the end of the first block and cse2 makes it the
// `mr r11,r9` copy every later block uses.
f32 cPlWep::getAngle()
{
    if (pPL->r_no_0 != 0) {
        return 0.0f;
    }
    if (pPL->r_no_1 != 6 && pPL->r_no_1 != 0xB) {
        return 0.0f;
    }
    if (pPL->r_no_2 == 3) {
        return 0.0f;
    }
    return pitch;
}

// Current aim blend rate (m3r, -1..1) while aiming / knife stance, else 0.
f32 cPlWep::getPitch()
{
    if (pPL->r_no_0 != 0) {
        return 0.0f;
    }
    if (pPL->r_no_1 != 6 && pPL->r_no_1 != 0xB) {
        return 0.0f;
    }
    if (pPL->r_no_2 == 3) {
        return 0.0f;
    }
    return m3r;
}

// Updates the weapon object's matrices.
void cPlWep::move()
{
    if (m_pWep) {
        m_pWep->matUpdate();
    }
}

// The laser marker (aim end) while in the weapon-ready state (stat 0x0601xx, r_no_3 != 0); 0 otherwise.
int cPlWep::getMarkerPos(Vec* pPos)
{
    cPlayer* pl = pPL;

    if ((pl->r_no_0 != 0 || pl->r_no_1 != 6 || pl->r_no_2 != 1) || pl->r_no_3 == 0) {
        return 0;
    }
    *pPos = m_pWep->m_ShotPos;
    return 1;
}

// Display type 2 of the weapon objects: the main object (only with type bit0 for the launcher /
// grenade / egg types; always for the bow) and m_pWepHand.
void cPlWep::setTrans(int on_off, int flag)
{
    if (m_pWep == 0) {
        return;
    }
    switch (pG->weapon_no) {
    default:
        m_pWep->setDisp(2, on_off);
        break;
    case 0xD:
        flag = 1;
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1F:
    case 0x20:
        if (flag & 1) {
            m_pWep->setDisp(2, on_off);
        }
        break;
    }
    if (m_pWepHand) {
        m_pWepHand->setDisp(2, on_off);
    }
}

// Aim start: picks the lock-on target nearest the hand (SearchLockEm) as m_pEm and arms 10
// frames of auto tracking (m_LockTime) when it is in front. Returns the target.
cEm* cPlWep::lockInit()
{
    cPlayer* pl = pPL;
    cEm* em;

    em = SearchLockEm(&pl->getPartsPtr(3)->world, 0);
    pl->m_pEm = em;
    if (em) {
        Vec v;
        f32 ang;

        PSMTXMultVec(em->getPartsPtr(em->lockParts)->mat, &pl->m_pEm->lockOfs, &v);
        ang = GetXZAngleLocal(&pl->pos, &v, pl->ang.y);
        if (ang <= PI && ang >= -PI) {
            m_LockTime = 10;
        } else {
            m_LockTime = 0;
        }
    } else {
        m_LockTime = 0;
    }
    return pl->m_pEm;
}

// Distance penalty by direction: inlined into rangeDist; its constants precede rangeDist's own.
// Lock-on distance score: the whole body (range clamp, far penalty, direction penalties) is one
// inline with its constants as const locals declared first, which fixes the pool order
// (0.87, 1.6e7, 2.25e8, 1e10, 4e10, 9e10, 1.6e11 before rangeDist's 0.0, 1e8, 1e12) and puts
// the twice-used 1.6e7 high half into a callee-saved register hoisted above the calls.
static inline f32 rangeAdd(Vec* pos, Vec* v, f32 d, f32& range)
{
    const f32 angLim = 0.87266463f;
    const f32 near = 16000000.0f;
    const f32 far = 225000000.0f;
    const f32 add0 = 10000000000.0f;
    const f32 add1 = 40000000000.0f;
    const f32 add2 = 90000000000.0f;
    const f32 add3 = 160000000000.0f;

    if (range == 0.0f) {
        range = 100000000.0f;
    }
    if (d > range) {
        return 1000000000000.0f;
    }
    if (d > far) {
        d += add3;
    } else if (fabsf(GetXZAngleLocal(pos, v, pPL->ang.y)) < angLim) {
        if (d > near) {
            d += add1;
        }
    } else if (d > near) {
        d += add2;
    } else {
        d += add0;
    }
    return d;
}

// Lock-on score of `em` from `pos`: the distance to its lock point plus penalties (far away,
// outside the 50-degree front cone, beyond 4000 / 15000); 1e12 beyond `range` (0 = 10000).
f32 rangeDist(Vec* pPos, cEm* pEm, f32 range_limit)
{
    Vec v;

    PSMTXMultVec(pEm->getPartsPtr(pEm->lockParts)->mat, &pEm->lockOfs, &v);
    return rangeAdd(pPos, &v, GetDistance(pPos, &v), range_limit);
}

// While aiming: a C-stick move cancels the lock time; with auto-aim on (pSys->Config_flg 0x20000000)
// the gun tracks the target for the remaining lock frames.
void cPlWep::lockMove()
{
    cPlayer* pl = pPL;

    if (Joy[0].on & 0xF0000) {
        m_LockTime = 0;
    }
    if (pl->m_pEm && m_LockTime != 0 && (CfgFlagChk(pSys, CFG_LOCK_ON))) {
        PlWepAutoTrack(pl, 0, 1.0f);
    }
}

// May `em` be locked from `pos`? Alive, active, lockable (be_flag 0x20, not EM_STATUS_LOCKOFF), a
// real enemy (id > 0xF), in front for the 0x41 / 0x43 bosses, and its lock point not behind a wall.
int lockEmCk(cEm* pEm, Vec* pPos)
{
    Vec v;

    if (!pEm->isAlive()) {
        return 0;
    }
    if (!(pEm->be_flag & 0x20)) {
        return 0;
    }
    if (pEm->id <= 0xF) {
        return 0;
    }
    if (pEm->hp <= 0) {
        return 0;
    }
    if (pEm->checkStatus(EM_STATUS_LOCKOFF)) {
        return 0;
    }
    if (pEm->checkStatus(EM_STATUS_ACTIVE) == 0) {
        return 0;
    }
    if (pEm->pList == 0) {
        return 0;
    }
    if (pEm->id == 0x43 || pEm->id == 0x41) {
        if (Front_check(pPL, pEm, 1.0471976f) == 0) {
            return 0;
        }
    }
    PSMTXMultVec(pEm->getPartsPtr(pEm->lockParts)->mat, &pEm->lockOfs, &v);
    return EatMgr.hitCheck(pPos, &v, 0, 0, 0x800, 0) == 0;
}

// Dead-stripped by the original linker (STRIP_UNUSED): pools -2pi, 0, 2pi and 0, pi between
// lockEmCk's and SearchLockEm's constants.
static f32 lockAngleWrap(f32 a)
{
    if (a < -2.0f * PI) {
        a = 0.0f;
    }
    if (a > 2.0f * PI) {
        a = 0.0f;
    }
    return a;
}

// |a| < PI (unused helper).
static int lockAngleFront(f32 a)
{
    if (a < 0.0f) {
        a = -a;
    }
    return a < PI;
}

// Cycles the lock-on to the next best target (skipping the current one); 10 lock frames.
cModel* cPlWep::lockNext()
{
    cPlayer* pl = pPL;

    if ((pl->m_pEm = SearchLockEm(&pl->getPartsPtr(3)->world, pl->m_pEm)) != 0) {
        m_LockTime = 10;
    }
    return pl->m_pEm;
}

// Best lock-on target from `pos` when auto-aim is enabled (pSys->Config_flg 0x20000000), else 0.
cEm* SearchLockEm(Vec* pPos, cEm* pEm_now)
{
    if (CfgFlagChk(pSys, CFG_LOCK_ON)) {
        return searchLockEm(pPos, pEm_now, 0.0f);
    }
    return 0;
}

// Best target within `range` regardless of the auto-aim option (enemy / scenario use).
cEm* SearchTargetEm(Vec* pPos, cEm* pEm_now, f32 range_limit)
{
    return searchLockEm(pPos, pEm_now, range_limit);
}

// Enemy with the lowest rangeDist score that passes lockEmCk, excluding `skip` (which is returned
// again when nothing else qualifies).
cEm* searchLockEm(Vec* pPos, cEm* pEm_now, f32 range_limit)
{
    Vec* p;
    cEm* skip = pEm_now;
    f32 range = range_limit;
    cEm* best = 0;
    f32 bestD = 1000000000000.0f;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        f32 d;

        if (em == skip) {
            continue;
        }
        if (lockEmCk(em, pPos) == 0) {
            continue;
        }
        d = rangeDist(pPos, em, range);
        if (d < bestD) {
            best = em;
            bestD = d;
        }
    }
    if (best == 0 && skip != 0) {
        // A local for pos, in its own scope: without it best/EmMgr loses its register tie.
        do {
            p = pPos;
            if (lockEmCk(skip, p)) {
                best = skip;
            }
        } while (0);
    }
    return best;
}

// Dead-stripped by the original linker (STRIP_UNUSED): only its `vecz` (.data vecz.1341, right
// before PlCornerCheck's) survives.
static int cornerCheckOld()
{
    static Vec vecz = {0.0f, 0.0f, 500.0f};
    Vec dir = {0.0f, 0.0f, 0.0f};
    Vec rot;

    dir.y = pPL->ang.y;
    RotVector(&vecz, &dir, &rot);
    PSVECAdd(&rot, &pPL->pos, &rot);
    return SatMgr.hitCheck(&pPL->pos, &rot, 0, 0, 0, 0);
}

// Corner-peek test: a wall within 500 ahead of the hand (behind when backing up, stat 0x0D) with
// open space 500 / 1000 to one side: 1 = the wall's left is open, 2 = right, 0 = no corner.
int PlCornerCheck()
{
    static Vec vecz = {0.0f, 0.0f, 500.0f};
    Vec* hand = &pPL->getPartsPtr(3)->world;
    Vec dir;
    Vec rot = {0.0f, 0.0f, 0.0f};
    Vec hit;
    Vec nrm;
    Vec a;
    Vec b;

    rot.y = pPL->ang.y;
    dir = rot;
    if (pPL->r_no_0 == 0 && pPL->r_no_1 == 0xD) {
        dir.y += PI;
        dir.y = LIMIT_ANGLE(dir.y);
    }
    RotVector(&vecz, &dir, &rot);
    PSVECAdd(&rot, hand, &rot);
    if (SatMgr.hitCheck(hand, &rot, &hit, &nrm, 0, 0)) {
        if (cnCkSub(hand, &nrm, 500.0f, &a, &b)) {
            return 1;
        }
        if (cnCkSub(hand, &nrm, -500.0f, &a, &b)) {
            return 2;
        }
        if (cnCkSub(hand, &nrm, 1000.0f, &a, &b)) {
            return 1;
        }
        if (cnCkSub(hand, &nrm, -1000.0f, &a, &b)) {
            return 2;
        }
    }
    return 0;
}

// One side of the corner test: `len` along the wall (sign = side) and 1000 through it must be
// clear; returns the two probe points.
int cnCkSub(Vec* pPlPos, Vec* pNorm, f32 sideDist, Vec* rPos, Vec* rAng)
{
    static Vec angR = {0.0f, PI / 2.0f, 0.0f};
    static Vec angB = {0.0f, PI, 0.0f};
    Vec v;
    Vec w;

    RotVector(pNorm, &angR, &v);
    PSVECScale(&v, &v, sideDist);
    PSVECAdd(&v, pPlPos, &v);
    if (SatMgr.hitCheck(pPlPos, &v, 0, 0, 0, 0)) {
        return 0;
    }
    RotVector(pNorm, &angB, &w);
    PSVECScale(&w, &w, 1000.0f);
    PSVECAdd(&w, &v, &w);
    if (SatMgr.hitCheck(&v, &w, 0, 0, 0, 0)) {
        return 0;
    }
    *rPos = v;
    *rAng = w;
    return 1;
}

// Aim control each frame while aiming: stick / d-pad move the pitch blend (m3r) and the waist yaw
// m_Fwork0 within the weapon's limit `lim` (turning the body past it), with acceleration repCtr
// and per-weapon speeds; then the weapon sway (PlWepLockRand), debug auto-track, the three-way
// motion blend and the waist twist.
void PlWepLockCtrl(cModel* plm)
{
    static f32 repCtr = 0.0f;
    cPlayer* pl = (cPlayer*) plm;
    // Pool order (target 0x108: 0.8, 12deg, 1.0, ...): a dead 0.8f initialiser expanded before the
    // switch, `default:` written first, `lim` assigned before `spd` in every case, and the 0x16 group
    // includes 0x19 (the compare tree's `cmpwi r0,25`).
    f32 lim = 0.8f;
    f32 spd;
    f32 spd2;
    f32 d;
    int moved;
    f32 tmp;

    switch (pG->weapon_no) {
    default:
        lim = 0.20943952f;
        spd = 1.0f;
        spd2 = spd;
        break;
    case 4:
        lim = 0.062831853f;
        spd = 1.5f;
        spd2 = 1.2f;
        break;
    case 7:
        lim = 0.10471976f;
        spd = 0.8f;
        spd2 = spd;
        break;
    case 8:
        lim = 0.052359879f;
        spd = 0.9f;
        spd2 = spd;
        break;
    case 0x21:
        lim = 0.10471976f;
        spd = 1.04f;
        spd2 = spd;
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        lim = 0.0065449847f;
        spd = 1.15f;
        spd2 = spd;
        break;
    case 0xE:
    case 0x13:
        if (pG->weapon_type == 0) {
            lim = 0.20943952f;
            spd = 1.0f;
            spd2 = spd;
            break;
        }
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x29:
        lim = 0.0065449847f;
        spd = 1.2f;
        spd2 = 1.5f;
        break;
    case 0x10:
        lim = 0.10471976f;
        spd = 1.2f;
        spd2 = spd;
        break;
    case 0xD:
        lim = 0.0065449847f;
        spd = 1.0f;
        spd2 = spd;
        break;
    }
    if (Joy[0].stickX || Joy[0].stickY) {  // main stick deflected
        if (repCtr < 7.0f) {
            repCtr = repCtr + 1.0f;
        }
    } else {
        repCtr = 0.0f;
    }
    moved = 0;
    if (joyKamae() || joyLKamae()) {
        if (pl->m_pEm && lockCtr != 0 && (DbgFlagChk(pG, DBG_PL_LOCK_FOLLOW))) {
            goto rand;
        }
        d = 0.0f;
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            if (Joy[0].on & 8) {
                d -= 0.035f;
            }
            if (Joy[0].on & 4) {
                d += 0.035f;
            }
            d -= spd2 * (f32) Joy[0].stickY * repCtr * 0.15f / 200.0f / 10.0f;
        } else {
            if (Joy[0].on & 8) {
                d = 0.035f;
            }
            if (Joy[0].on & 4) {
                d -= 0.035f;
            }
            d += spd2 * (f32) Joy[0].stickY * repCtr * 0.15f / 200.0f / 10.0f;
        }
        if (m3r > 0.0f) {
            d *= 0.8f;
        }
        if (d != 0.0f) {
            moved = 1;
        }
        m3r += d;
        m3r.limit(-1.0f, 1.0f);
        d = 0.0f;
        if (Joy[0].on & 2) {
            d -= 0.05f;
        }
        if (Joy[0].on & 1) {
            d += 0.05f;
        }
        d -= (f32) Joy[0].stickX * repCtr * PI / 10.0f / 200.0f / 20.0f;
        if (d != 0.0f) {
            moved = 1;
        }
        pl->m_Fwork0 += d;
        if (pl->m_Fwork0 > lim) {
            pl->m_Fwork0 = lim;
            if (Joy[0].on & 1) {
                pl->ang.y += 0.039269908f;
            } else {
                pl->ang.y -= spd * (f32) Joy[0].stickX * repCtr * PI / 10.0f / 200.0f / 20.0f;
            }
        }
        if (pl->m_Fwork0 < -lim * 0.8f) {
            pl->m_Fwork0 = -lim * 0.8f;
            if (Joy[0].on & 2) {
                pl->ang.y -= 0.039269908f;
            } else {
                pl->ang.y -= spd * (f32) Joy[0].stickX * repCtr * PI / 10.0f / 200.0f / 20.0f;
            }
        }
    }
rand:
    tmp = m3r;
    PlWepLockRand(pl, moved, &tmp, &pl->m_Fwork0);
    m3r = tmp;
    if (DbgFlagChk(pG, DBG_PL_LOCK_FOLLOW) && lockCtr != 0) {
        PlWepAutoTrack(pl, 1, 0.03f);
    }
    m3r.move();
    mot3.move(m3r);
    pl->Waist->set(pl->m_Fwork0, 0.4f);
}

// Resets the sway amplitude factor (lockRandCtr 0.2).
void PlWepLockRandInit()
{
    lockRandCtr = 0.2f;
}

// Weapon sway around the aim centre (wep.pitch / m_CenterY) with the weapon's lockRand* ranges:
// flag bit0 = the player moved the aim, re-centre; bit1 = a fresh random offset; else a random
// walk by the step values clamped to the range. pitch is in blend units (-1..1 = -90..90 degrees).
void PlWepLockRand(cModel* pEm, int mflag, f32* ang_x, f32* ang_y)
{
    cPlayer* pl = (cPlayer*) pEm;
    cPlWep* wep = pl->Wep;
    f32 rP;
    f32 rY;
    f32 sP;
    f32 sY;

    *ang_x *= PI / 2.0f;
    wep->m_pWep->getAbility(&rP, &rY, &sP, &sY);
    if (mflag & 1) {
        wep->pitch = *ang_x;
        wep->m_CenterY = *ang_y;
    } else if (mflag & 2) {
        *ang_x = fRand1_1() * rP * lockRandCtr + wep->pitch;
        *ang_y = fRand1_1() * rY * lockRandCtr + wep->m_CenterY;
    } else {
        *ang_x = sP * fRand1_1() + *ang_x;
        *ang_y = sY * fRand1_1() + *ang_y;
        if (*ang_x > wep->pitch + rP) {
            *ang_x = wep->pitch + rP;
        } else if (*ang_x < wep->pitch - rP) {
            *ang_x = wep->pitch - rP;
        }
        if (*ang_y > wep->m_CenterY + rP) {
            *ang_y = wep->m_CenterY + rP;
        } else if (*ang_y < wep->m_CenterY - rP) {
            *ang_y = wep->m_CenterY - rP;
        }
    }
    *ang_x *= 2.0f / PI;
}

// Turns the aim toward the locked enemy's lock point: yaw by at most 30 degrees * rate (mode 1
// moves the waist within 12 degrees, else the body), pitch blend by at most 0.05 per frame.
void PlWepAutoTrack(cModel* plm, int mode, f32 rate)
{
    cPlayer* pl = (cPlayer*) plm;
    Vec* hand;
    Vec tgt;
    f32 dist;
    f32 d;
    f32 e;
    f32 na = 0.20943952f;  // function-scope, dead initialiser: puts 12deg before 400.0f in the pool (target 0x178)
    register s16 hm asm("r4"); // COMPILER-DIFF: #8

    // COMPILER-DIFF: #8 -- the original ranks `mr r29,r4` (mode) after `fmr f31,f1`, i.e. as if r4
    // did not die at the copy; the HImode read of r4 keeps it live past the copy (regmove only moves
    // the death when the dying mode matches the copy's), see docs/matching.md #8.
    asm("" : "=m"(pl->m_Fwork0) : "r"(hm));
    if (pl->m_pEm == 0) {
        return;
    }
    hand = &pl->getPartsPtr(10)->world;
    PSMTXMultVec(pl->m_pEm->getPartsPtr(pl->m_pEm->lockParts & 7)->mat, &pl->m_pEm->lockOfs,
                 &tgt);
    dist = GetDistance3(hand, &tgt);
    if (dist > 400.0f) {
        d = Muku(hand, &tgt, pl->ang.y + pl->m_Fwork0, rate * PI);
        if (d > 0.52359879f) {
            d = 0.52359879f;
        }
        if (d < -0.52359879f) {
            d = -0.52359879f;
        }
        if (mode != 0) {
            na = pl->m_Fwork0 + d;
            if (na <= 0.20943952f && na >= -0.20943952f) {
                pl->m_Fwork0 = na;
            } else if (pl->m_Fwork0 + d > 0.20943952f) {
                pl->ang.y += 0.052359879f;
            } else {
                pl->ang.y -= 0.052359879f;
            }
        } else {
            pl->ang.y += d;
            pl->ang.y = LIMIT_ANGLE(pl->ang.y);
        }
    }
    e = atan2(tgt.y - hand->y, dist) / (PI / 4.0f) - m3r;
    if (e > 0.05f) {
        e = 0.05f;
    }
    if (e < -0.05f) {
        e = -0.05f;
    }
    m3r += e;
    m3r.limit(-1.0f, 1.0f);
}

// Water hit of a shot line: the splash where p0-p1 crosses the water surface (if not behind a
// wall); shotguns (5, 6, 0xF, 0x2C) add five random splashes within 2000 of the end.
void wepSetWaterShot(Vec* p0, Vec* p1, u8 wepNo)
{
    Vec d;
    Vec cross;
    Vec r;
    int i;

    PSVECSubtract(p1, p0, &d);
    if (GetWaterCrossPos(p0, &d, &cross)) {
        if (EatMgr.hitCheck(p0, &cross, 0, 0, 0x800, 0) == 0) {
            setWaterShot(&cross);
        }
    }
    switch (wepNo) {
    case 5:
    case 6:
    case 0xF:
    case 0x2C:
        for (i = 0; i < 3; i++) {   // reversed by loop.c: `li r31,3` after the hoisted `addi`/`lfs`
            r.x = fRand1_1() * 2000.0f + p1->x;
            r.y = fRand1_1() * 2000.0f + p1->y;
            r.z = fRand1_1() * 2000.0f + p1->z;
            wepSetWaterShot(p0, &r, 2);
        }
        break;
    }
}

// Splash at `pos`: hit-mark effect, wave push, SE.
void setWaterShot(Vec* pos)
{
    EspSetWaterHitmark(pos);
    AddWaterPower(*pos, 0.6f);
    SndCall(2, 0xB, pos, 0, 0, 0);
}

// Aim start pitch: the elevation to the locked enemy (auto-aim on) or the camera pitch (doubled
// when looking up), stored as wep.pitch and as the m3r blend rates.
void PlSetLockPitch(cModel* pEm)
{
    cPlayer* pl = (cPlayer*) pEm;
    f32 p;

    if (CfgFlagChk(pSys, CFG_LOCK_ON)) {
        if (pl->m_pEm) {
            Vec d;

            PSVECSubtract(&pl->m_pEm->getPartsPtr(pl->m_pEm->lockParts)->world, &pl->pList->world,
                          &d);
            p = VecElevation(&d);
        } else {
            p = 0.0f;
        }
    } else {
        p = CamCtrl.getCameraPitch();
        if (p > 0.0f) {
            p += p;
        }
    }
    {
        // COMPILER-DIFF: 13 (value pin): the target's 2/PI high sits in r11 and pWep in r9 -- the
        // original rematerialises the pool constant's high with a reload register that avoids the
        // live pWep; local-alloc here hands the shorter-lived high r9 first.
        register cPlWep* w asm("r9") = pl->Wep;
        w->pitch = p;
    }
    p *= 2.0f / PI;
    do {
        // COMPILER-DIFF: candidate (sched barrier): the first insn after LOOP_BEG is the sched1
        // barrier. Ours would be the `lis m3r` of the setDelay store's address, the target's order
        // (`lis 0.0; lis m3r; lfs z; addi`) is what the ready list gives when the barrier insn
        // emits no code.
        asm("" : : "f"(p));
        m3r.setDelay(0.0f);
        m3r = p;
        m3r.move();
    } while (0);
}

// Weapon size class for the window-break enemy: 0 handguns / small, 1 shotguns / rifles, 2 heavy.
int GetWepSizeGroup(int wepId)
{
    switch (wepId) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x11:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1F:
    case 0x20:
    case 0x26:
    case 0x27:
    case 0x2A:
    case 0x2B:
        return 0;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x16:
    case 0x1E:
    case 0x21:
    case 0x28:
    case 0x29:
    case 0x2C:
        return 1;
    case 0x10:
    case 0x1A:
        return 2;
    case 0:
    case 0x14:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    default:
        return 3;
    }
}

// Dead-stripped by the original linker (STRIP_UNUSED): it instantiates cManager<cLight>::destroyAll, whose
// end-of-unit copy inlines destroy() and leaves the five `%s::destroy()` / `%s::deleteList()` log strings
// after the create() ones at the end of .rodata (0x290..0x350). A direct `LightMgr.destroy(l)` would emit
// them at the function's own position (before the create strings).
static void wepLightReleaseAll()
{
    LightMgr.destroyAll();
}
