#include "types.h"
#include "em_wrap.h"
#include "em_set.h"
#include "global.h"
#include "db_log.h"
#include "sce_sys.h"
#include "player.h"
#include "math_sub.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

// int store through a reference (keeps the following loads below it, like global.h BitOn)
static inline void IntSet(int& d, int v) { d = v; }

// Typed view of pG->emlist (the r400 idiom): the original indexes an EmListData array, so the
// element address is `pG + no * 32` (pG first in the add, the table offset in the displacement);
// EM_LIST's `&pG->emlist[no * 0x20]` is a pointer sum whose MULT term expand puts first.
struct EmListView {
    u8 pad[0x52E8];
    EmListData emlist[0x100];
};

// Room-script enemy handle (include/em_wrap.h): the first object of every stage REL (st1_0..st4_0). No
// __FILE__ string: the file name is not in the binary. The original REL link dead-stripped the members no
// room of the module calls (config/G4BE08/modules.py STRIP_UNUSED), leaving their strings and constant
// pools, so every member is written even where no module kept its body. Function order = .text and
// .rodata (string) order. st2_4/st4_0 were built without the cEmControl/cEmPatrol/cEmGuard part
// (src/st/em_wrap_v2.cpp defines EM_WRAP_NO_CONTROL and includes this file).

#ifndef EM_WRAP_NO_CONTROL
// Bind the controller to enemy list entry `no` (setPtr: existing enemy or spawn it) and load an
// n-point position route (max 15). Returns 0 (logging "SetPatrol" when errOn) if the enemy is missing.
int cEmControl::SetControl(s16 no, Vec* tbl, int n, int errOn)
{
    if (em.setPtr(no, -1, errOn) == 0) {
        if (errOn == 1) {
            pLog->err(0, 0, "cEmControl::SetPatrol");
        }
        return 0;
    }
    active = 1;
    SetTargetPos(tbl, n);
    return 1;
}

#ifdef EM_WRAP_ROUTE
// Same as the Vec* overload but the route entries carry their own goto mode (EmControlPoint::mode),
// used by cEmRouteExec.
int cEmControl::SetControl(s16 no, EmControlPoint* tbl, int n, int errOn)
{
    if (em.setPtr(no, -1, errOn) == 0) {
        if (errOn == 1) {
            pLog->err(0, 0, "cEmControl::SetPatrol");
        }
        return 0;
    }
    active = 1;
    SetTargetTbl(tbl, n);
    return 1;
}
#endif

// Load the route table from bare positions (mode stays 0); clamps to 15 points, rewinds prev/cur.
void cEmControl::SetTargetPos(Vec* tbl, int n)
{
    int i;

    prev = 0;
    cur = 0;
    memset(&point[0], 0, sizeof(EmControlPoint));
    nPoint = n;
    if (n > 15) {
        nPoint = 15;
    }
    for (i = 0; i < nPoint; i++) {
        point[i].pos = tbl[i];
    }
}

#ifdef EM_WRAP_ROUTE
// Load the route table from full EmControlPoint entries (position + goto mode); clamps to 15 points.
void cEmControl::SetTargetTbl(EmControlPoint* tbl, int n)
{
    int i;

    prev = 0;
    cur = 0;
    memset(&point[0], 0, sizeof(EmControlPoint));
    nPoint = n;
    if (n > 15) {
        nPoint = 15;
    }
    for (i = 0; i < nPoint; i++) {
        point[i] = tbl[i];
    }
}
#endif

// Stop the route task (active = 0) and hand the enemy back to its normal AI: goto the player, plain
// Character, and Rno0 = 1 (the Ganado's ordinary think routine).
void cEmControl::EndControl()
{
    active = 0;
    if (em.isActive() != 0) {
        cEm* p;

        em.setGoto(&pPL->pos, 0);
        em.setCharacter(0);
        p = em.getPtr();
        if (p != 0) {
            p->r_no_0 = 1;
            p->r_no_1 = 0;
            p->r_no_2 = 0;
            p->r_no_3 = 0;
        }
    }
}

// Patrol: bind the enemy to a looping route and start TaskMove as an SceExec 0x12 task with priority prio.
int cEmPatrol::SetPatrol(s16 no, Vec* tbl, int n, u8 prio, int errOn)
{
    if (SetControl(no, tbl, n, errOn) == 0) {
        return 0;
    }
    SceExec(0x12, (TaskFunc) TaskMove, (int) this, prio, SCE_PRIO_DEF_2, 0);
    return 1;
}

// Patrol task body: walks the enemy through the route points with goto mode 6 (walk), wrapping to point
// 0 at the end, until the controller is ended, the enemy dies, or it spots the player (ckFindPL).
void cEmPatrol::TaskMove(cEmPatrol* p)
{
    cEmWrap* em = &p->em;

    while (p->active != 0 && em->ckFindPL() != 1 && em->isActive() != 0) {
        if (em->ckGoto() != 6) {
            int cur;
            int next;
            int wrap;

            em->setGoto(&p->point[p->cur].pos, 6);
            cur = p->cur;
            next = cur + 1;
            p->prev = cur;
            p->cur = next;
            if (next < 0) {
                wrap = p->nPoint - 1;
            } else {
                wrap = (next > p->nPoint - 1) ? 0 : next;
            }
            p->cur = wrap;
        }
        SceSleep(1);
    }
}

#ifdef EM_WRAP_ROUTE
// Route run: bind the enemy to a one-shot route (goto mode 1, run) and start TaskMove with priority prio.
int cEmRouteRun::SetRouteRun(s16 no, Vec* tbl, int n, u8 prio, int errOn)
{
    if (SetControl(no, tbl, n, errOn) == 0) {
        return 0;
    }
    SceExec(0x12, (TaskFunc) TaskMove, (int) this, prio, 2, 0);
    return 1;
}

// Route-run task body: runs the enemy point to point (mode 1); at the last point issues goto mode 0xB
// (stop / free) and ends. Does not check ckFindPL, so the run is not interrupted by seeing the player.
void cEmRouteRun::TaskMove(cEmRouteRun* p)
{
    cEmWrap* em = &p->em;

    while (p->active != 0 && em->isActive() != 0) {
        if (em->ckGoto() != 1) {
            Vec* pos = &p->point[p->cur].pos;
            int cur;
            int next;

            em->setGoto(pos, 1);
            cur = p->cur;
            next = cur + 1;
            p->prev = cur;
            p->cur = next;
            if (next >= p->nPoint) {
                em->setGoto(pos, 0xB);
                break;
            }
        }
        SceSleep(1);
    }
}

// Route exec: like SetRouteRun but each EmControlPoint carries its own goto mode; starts TaskMove.
int cEmRouteExec::SetRouteExec(s16 no, EmControlPoint* tbl, int n, u8 prio, int errOn)
{
    if (SetControl(no, tbl, n, errOn) == 0) {
        return 0;
    }
    SceExec(0x12, (TaskFunc) TaskMove, (int) this, prio, 2, 0);
    return 1;
}

// Route-exec task body: issues setGoto(point.pos, point.mode) per point, advancing when the enemy's
// ckGoto no longer reports the previous point's mode; ends after the last point.
void cEmRouteExec::TaskMove(cEmRouteExec* p)
{
    cEmWrap* em = &p->em;

    while (p->active != 0 && em->isActive() != 0) {
        if (em->ckGoto() != p->point[p->prev].mode) {
            int cur;
            int next;

            em->setGoto(&p->point[p->cur].pos, p->point[p->cur].mode);
            cur = p->cur;
            next = cur + 1;
            p->prev = cur;
            p->cur = next;
            if (next >= p->nPoint) {
                break;
            }
        }
        SceSleep(1);
    }
}
#endif

// Guard post: bind the enemy to a post table, remember the facing angle `ang` (radians, Y), the
// enemy's normal Guard_r and the `check` callback that says whether it may keep guarding; starts TaskMove.
int cEmGuard::SetGuard(s16 no, Vec* tbl, int n, int (*check)(cEmWrap*), f32 ang, u8 prio, int errOn)
{
    if (SetControl(no, tbl, n, errOn) == 0) {
        return 0;
    }
    SceExec(0x12, (TaskFunc) TaskMove, (int) this, prio, SCE_PRIO_DEF_2, 0);
    this->ang = ang;
    guard_r = em.getGuard_r();
    this->check = check;
    x130 = 0;
    return 1;
}

// Guard task body. While check(em) is true: step 0 walk to the post, step 1 on arrival turn to face
// `ang` (goto a point 1000 units ahead, Character 1, Guard_r 100 = passive), step 2 re-post if the
// Ganado leaves Rno 1/1. When check() fails once (alerted): goto the player, Character 0, restore Guard_r.
void cEmGuard::TaskMove(cEmGuard* g)
{
    Vec v;   // frame offset 0: its address is recomputed per call
    cEmWrap* em = &g->em;

    em->setGoto(&pPL->pos, 0xB);
    while (g->active != 0 && em->isActive() != 0) {
        if (g->check != 0 && g->check(em) != 0) {
            if (g->alerted != 0) {
                g->alerted = 0;
                g->step = 0;
            }
            switch (g->step) {
            case 0:
                em->setGoto(&g->point[g->cur].pos, 1);
                g->step++;
                break;
            case 1:
                if (em->ckGoto() != 1) {
                    Vec rot;
                    Mtx m;

                    rot.x = 0.0f;
                    rot.y = g->ang;
                    rot.z = 0.0f;
                    RotMatrix(m, &rot);
                    v.x = 0.0f;
                    v.y = 0.0f;
                    v.z = 1000.0f;
                    PSMTXMultVec(m, &v, &v);
                    PSVECAdd(&g->point[g->cur].pos, &v, &v);
                    em->setGoto(&v, 1);
                    em->setCharacter(1);
                    em->setGuard_r(100.0f);
                    g->step++;
                }
                break;
            case 2:
                if (em->ckGoto() != 1) {
                    if (em->ckRno01(1, 1) == 0) {
                        g->step = 0;
                    }
                }
                break;
            }
        } else if (g->alerted != 1) {
            IntSet(g->alerted, 1);
            IntSet(g->step, 0);
            em->setGoto(&pPL->pos, 0);
            em->setCharacter(0);
            em->setGuard_r(g->guard_r);
        }
        SceSleep(1);
    }
}
#endif

// Empty handle (pEm = 0, alive = 0).
cEmWrap::cEmWrap()
{
    initWork();
}

// Reset the handle to "no enemy"; also used after destroy().
void cEmWrap::initWork()
{
    pEm = 0;
    no = 0;
    list = 0;
    alive = 0;
}

// Report a failed access through pLog unless errOn is off or Debug_flg[1] bit 0x20000 silences it.
void cEmWrap::err(const char* msg, int no)
{
    if (errOn == 1 && !DbgFlagChk(pG, DBG_EMW_ERR_NO_DISP)) {
        pLog->err(0, 0, msg, no);
    }
}

// Take enemy list entry `no` of list `list` (-1: any): reuse the already-spawned enemy if the ESL entry's
// be_flag bit 2 is set, else spawn it with EmSetFromList2 (chkDead: refuse dead-flagged entries). setAlive
// marks the ESL entry alive. Returns 1 on success; on failure pEm = 0 and 0.
int cEmWrap::setEm(s16 no, s8 list, int errOn, int chkDead, int setAlive)
{
    this->errOn = errOn;
    this->no = no;
    this->list = list;
    if (list >= 0 && pG->em_list_no != list) {
        pEm = 0;
        err("EM_SET_NO(%d) cEmWrap::setEm list No. difference", this->no);
        return 0;
    }
    if (((EmListView*) pG)->emlist[no].be_flag & 2) {
        pEm = GetEmPtrFromList(no);
    } else {
        pEm = EmSetFromList2(no, chkDead == 1);
    }
    if (pEm == 0) {
        err("EM_SET_NO(%d) cEmWrap::setEm failed", this->no);
        return 0;
    }
    alive = 1;
    if (setAlive == 1) {
        EmListSetAlive(no, 1);
    }
    return 1;
}

// Free-function form: spawn/fetch list entry `no` and return the raw cEm* (0 on failure).
cEm* setEm(s16 no, s8 list, int errOn, int chkDead, int setAlive)
{
    cEmWrap em;

    if (em.setEm(no, list, errOn, chkDead, setAlive) == 1) {
        return em.getPtr();
    }
    return 0;
}

// Attach to list entry `no` if it is already spawned (GetEmPtrFromList), otherwise spawn it via setEm.
int cEmWrap::setPtr(s16 no, s8 list, int errOn)
{
    cEm* p;

    if (list >= 0 && pG->em_list_no != list) {
        pEm = 0;
        err("EM_SET_NO(%d) cEmWrap::setEm list No. difference", no);
        return 0;
    }
    p = GetEmPtrFromList(no);
    if (p != 0) {
        return setPtr(p, errOn);
    }
    return setEm(no, list, errOn, 1, 1);
}

// Attach to a bare enemy pointer: accepted only when be_flag has bit 1 (alive) and not 0x200 (dying);
// then no = list = -1. Returns 1 on success.
int cEmWrap::setPtr(cEm* em, int errOn)
{
    this->errOn = errOn;
    if (em != 0) {
        int a = (em->be_flag & 0x201) == 1;

        if (a == 1) {
            alive = a;
            no = -1;
            pEm = em;
            list = -1;
            return 1;
        }
    }
    err("EM_SET_NO(%d) cEmWrap::setPtr failed", no);
    return 0;
}

// The enemy pointer, or 0 (with an error log) once it has died.
cEm* cEmWrap::getPtr()
{
    if (isAlive() == 1) {
        return pEm;
    }
    err("EM_SET_NO(%d) cEmWrap::getPtr error", no);
    return 0;
}

// 1 while the handle was set and the enemy still has be_flag alive (1) without the 0x200 kill bit.
int cEmWrap::isAlive()
{
    if (alive != 1) {
        return 0;
    }
    if (pEm == 0) {
        return 0;
    }
    return (pEm->be_flag & 0x201) == 1;
}

// 1 when alive and the enemy reports EM_STATUS_ACTIVE (not suspended / not dead).
int cEmWrap::isActive()
{
    if (isAlive() == 1 && pEm->checkStatus(EM_STATUS_ACTIVE) == 1) {
        return 1;
    }
    return 0;
}

// Kill the enemy through EmMgr and clear its ESL entry's be_flag bit 1 (so it is not re-spawned), then reset the handle.
void cEmWrap::destroy()
{
    if (isAlive() == 1) {
        EmListData* d = GetListPtrFromEm(pEm);

        if (d != 0) {
            d->be_flag &= ~1;
        }
        EmMgr.destroy(pEm);
    } else {
        err("EM_SET_NO(%d) cEmWrap::destroy error", no);
    }
    initWork();
}

// be_flag bit 2 (0x2): 1 = the enemy is not drawn (transparent), 0 = visible.
void cEmWrap::setTrans(int on)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (on == 1) {
            p->be_flag |= 2;
        } else {
            p->be_flag &= ~2;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setTrans error", no);
    }
}

// 1 when be_flag bit 0x2 (not drawn) is set.
int cEmWrap::isTrans()
{
    if (isAlive() == 1) {
        return (pEm->be_flag & 2) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isTrans error", no);
    return 0;
}

// be_flag bit 0x4: 1 = the enemy's move routine is skipped (frozen in place), 0 = normal.
void cEmWrap::setMove(int on)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (on == 1) {
            p->be_flag |= 4;
        } else {
            p->be_flag &= ~4;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setMove error", no);
    }
}

// 1 when be_flag bit 0x4 (move suppressed) is set.
int cEmWrap::isMove()
{
    if (isAlive() == 1) {
        return (pEm->be_flag & 4) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isMove error", no);
    return 0;
}

// Set (on = 1) or clear an arbitrary be_flag bit mask on the enemy.
void cEmWrap::setBeFlag(u32 bit, int on)
{
    if (isAlive() == 1) {
        if (on == 1) {
            pEm->be_flag |= bit;
        } else {
            pEm->be_flag &= ~bit;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setBeFlag error", no);
    }
}

// 1 when any of the be_flag bits in `bit` is set.
int cEmWrap::isBeFlag(u32 bit)
{
    if (isAlive() == 1) {
        return (pEm->be_flag & bit) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isBeFlag error", no);
    return 0;
}

// Non-zero when the enemy is in a damage reaction (EM_STATUS_LOCKOFF: not lockable by the aim).
int cEmWrap::isDamage()
{
    if (isAlive() == 1) {
        return pEm->checkStatus(EM_STATUS_LOCKOFF);
    }
    err("EM_SET_NO(%d) cEmWrap::isDamage error", no);
    return 0;
}

// Forward cEm::setNoSuspend: keep the enemy updating when it is off-screen / far away.
void cEmWrap::setNoSuspend(int on)
{
    if (isAlive() == 1) {
        pEm->setNoSuspend(on);
    } else {
        err("EM_SET_NO(%d) cEmWrap::setNoSuspend error", no);
    }
}

// 1 when be_flag bit 0x400 (no suspend) is set.
int cEmWrap::isNoSuspend()
{
    if (isAlive() == 1) {
        return (pEm->be_flag & 0x400) != 0;
    }
    err("EM_SET_NO(%d) cEmWrap::isNoSuspend error", no);
    return 0;
}

// Force the enemy's routine numbers r_no_0 / r_no_1 (its state machine: Rno0 = routine, Rno1 = sub-step).
void cEmWrap::setRno(u8 r0, u8 r1)
{
    if (isAlive() == 1) {
        pEm->r_no_0 = r0;
        pEm->r_no_1 = r1;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setRno error", no);
    }
}

// 1 when the enemy is currently in routine r0 / sub-step r1.
int cEmWrap::ckRno01(int r0, int r1)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (p->r_no_0 == r0 && p->r_no_1 == r1) {
            return 1;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckRno01 error", no);
    }
    return 0;
}

// Overwrite the enemy's current hp.
void cEmWrap::setHp(s16 hp)
{
    if (isAlive() == 1) {
        pEm->hp = hp;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setHp error", no);
    }
}

// Current hp (0 with an error log if the enemy is gone).
s16 cEmWrap::getHp()
{
    if (isAlive() == 1) {
        return pEm->hp;
    }
    err("EM_SET_NO(%d) cEmWrap::getHp error", no);
    return 0;
}

// The enemy's hp_max from its parameter table.
s16 cEmWrap::getHpMax()
{
    if (isAlive() == 1) {
        return pEm->hp_max;
    }
    err("EM_SET_NO(%d) cEmWrap::getHpMax error", no);
    return 0;
}

// The Ganado Character byte (aggressiveness class: 0 = hostile / chase, 1 = passive / guard walk).
u8 cEmWrap::Character()
{
    if (isAlive() == 1) {
        return pEm->Character;
    }
    err("EM_SET_NO(%d) cEmWrap::Character error", no);
    return 0;
}

// Set the Character byte (see Character()).
void cEmWrap::setCharacter(u8 c)
{
    if (isAlive() == 1) {
        pEm->Character = c;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setCharacter error", no);
    }
}

// The enemy's Guard_r: the radius (game units) inside which it notices the player.
f32 cEmWrap::getGuard_r()
{
    if (isAlive() == 1) {
        return pEm->Guard_r;
    }
    err("EM_SET_NO(%d) cEmWrap::Guard_r error", no);
    return 0.0f;
}

// Set Guard_r (cEmGuard uses 100 while posted so the guard ignores the player until check() fails).
void cEmWrap::setGuard_r(f32 r)
{
    if (isAlive() == 1) {
        pEm->Guard_r = r;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setGuard_r error", no);
    }
}

// 1 when cEm::checkStatus(stat) reports the EM_STATUS bit set.
int cEmWrap::checkStatus(int stat)
{
    if (isAlive() == 1) {
        return pEm->checkStatus(stat) != 0 ? 1 : 0;
    }
    err("EM_SET_NO(%d) cEmWrap::checkStatus error", no);
    return 0;
}

// Teleport the enemy (cEm::setPos also refreshes its old position / matrix).
void cEmWrap::setPos(Vec* pos)
{
    if (isAlive() == 1) {
        pEm->setPos(pos);
    } else {
        err("EM_SET_NO(%d) cEmWrap::setPos error", no);
    }
}

// Set the enemy's rotation (radians).
void cEmWrap::setAng(Vec* ang)
{
    if (isAlive() == 1) {
        pEm->setAng(ang);
    } else {
        err("EM_SET_NO(%d) cEmWrap::setAng error", no);
    }
}

// Set the enemy's model scale.
void cEmWrap::setSca(Vec* sca)
{
    if (isAlive() == 1) {
        pEm->scale = *sca;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setSca error", no);
    }
}

// OR bits into the enemy's `flag` word (the per-enemy behaviour flags the ESL sets).
void cEmWrap::setFlag(u32 bit)
{
    if (isAlive() == 1) {
        pEm->flag |= bit;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setFlag error", no);
    }
}

// 1 when the enemy's `flag` word has any of `bit` set (logs with the setFlag text: vendor copy-paste).
int cEmWrap::ckFlag(u32 bit)
{
    if (isAlive() != 1) {
        err("EM_SET_NO(%d) cEmWrap::setFlag error", no);
    } else if (pEm->flag & bit) {
        return 1;
    }
    return 0;
}

// Copy the enemy position out; zero vector (and error log) if it is gone.
void cEmWrap::getPos(Vec* pos)
{
    if (isAlive() == 1) {
        *pos = pEm->pos;
    } else {
        err("EM_SET_NO(%d) cEmWrap::getPos error", no);
        pos->x = 0.0f;
        pos->y = 0.0f;
        pos->z = 0.0f;
    }
}

// pos.x, or 0 if the enemy is gone.
f32 cEmWrap::getPosX()
{
    if (isAlive() == 1) {
        return pEm->pos.x;
    }
    err("EM_SET_NO(%d) cEmWrap::getPosX error", no);
    return 0.0f;
}

// pos.y, or 0 if the enemy is gone.
f32 cEmWrap::getPosY()
{
    if (isAlive() == 1) {
        return pEm->pos.y;
    }
    err("EM_SET_NO(%d) cEmWrap::getPosY error", no);
    return 0.0f;
}

// pos.z, or 0 if the enemy is gone.
f32 cEmWrap::getPosZ()
{
    if (isAlive() == 1) {
        return pEm->pos.z;
    }
    err("EM_SET_NO(%d) cEmWrap::getPosZ error", no);
    return 0.0f;
}

// Copy the enemy rotation out; zero vector if it is gone.
void cEmWrap::getAng(Vec* ang)
{
    if (isAlive() == 1) {
        *ang = pEm->ang;
    } else {
        err("EM_SET_NO(%d) cEmWrap::getAng error", no);
        ang->x = 0.0f;
        ang->y = 0.0f;
        ang->z = 0.0f;
    }
}

// ang.x, or 0 if the enemy is gone.
f32 cEmWrap::getAngX()
{
    if (isAlive() == 1) {
        return pEm->ang.x;
    }
    err("EM_SET_NO(%d) cEmWrap::getAngX error", no);
    return 0.0f;
}

// ang.y (facing, radians), or 0 if the enemy is gone.
f32 cEmWrap::getAngY()
{
    if (isAlive() == 1) {
        return pEm->ang.y;
    }
    err("EM_SET_NO(%d) cEmWrap::getAngY error", no);
    return 0.0f;
}

// ang.z, or 0 if the enemy is gone.
f32 cEmWrap::getAngZ()
{
    if (isAlive() == 1) {
        return pEm->ang.z;
    }
    err("EM_SET_NO(%d) cEmWrap::getAngZ error", no);
    return 0.0f;
}

// Forward cEm::motionSet (motion data, id, frame, blend, flags): rooms that script an enemy's animation.
void cEmWrap::motionSet(void* data, int a, int b, int c, int d)
{
    if (isAlive() == 1) {
        pEm->motionSet(data, a, b, c, d);
    } else {
        err("EM_SET_NO(%d) cEmWrap::motionSet error", no);
    }
}

// Forward cEm::motionMove: advance the enemy's motion one frame (rooms that drive a paused enemy by hand).
void cEmWrap::motionMove()
{
    if (isAlive() == 1) {
        pEm->motionMove();
    } else {
        err("EM_SET_NO(%d) cEmWrap::motionMove error", no);
    }
}

// be_flag bit 0x40: 1 = the enemy's motion is frozen, 0 = plays.
void cEmWrap::motionPause(int on)
{
    if (isAlive() == 1) {
        cEm* p = pEm;

        if (on == 1) {
            p->be_flag |= 0x40;
        } else {
            p->be_flag &= ~0x40;
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::motionPause error", no);
    }
}

// Attach an extra model (cModelInfo) to the enemy.
void cEmWrap::addModel(cModelInfo* info)
{
    if (isAlive() == 1) {
        pEm->addModel(info);
    } else {
        err("EM_SET_NO(%d) cEmWrap::addModel error", no);
    }
}

// Set pParent: the enemy's matrix is then relative to this model (riding a vehicle, held object).
void cEmWrap::setParent(cModel* parent)
{
    if (isAlive() == 1) {
        pEm->pParent = parent;
    } else {
        err("EM_SET_NO(%d) cEmWrap::setParent error", no);
    }
}

// Forward cEm::beginEvent: put the enemy into event mode (AI off, event motions).
void cEmWrap::beginEvent()
{
    if (isAlive() == 1) {
        pEm->beginEvent();
    } else {
        err("EM_SET_NO(%d) cEmWrap::beginEvent error", no);
    }
}

// Forward cEm::endEvent: leave event mode.
void cEmWrap::endEvent()
{
    if (isAlive() == 1) {
        pEm->endEvent();
    } else {
        err("EM_SET_NO(%d) cEmWrap::endEvent error", no);
    }
}

// Rebuild the enemy's model matrix now (after setPos/setAng outside the normal update).
void cEmWrap::matCalc()
{
    if (isAlive() == 1) {
        pEm->matUpdate();
    } else {
        err("EM_SET_NO(%d) cEmWrap::matCalc error", no);
    }
}

// 1 when the enemy id is a Ganado (0x10..0x20 except 0x18): the ids whose cEm is a cEmGanado with
// the goto/findPL virtuals. All the *Goto/*FindPL/*Reset members below require this.
int cEmWrap::isNormalGanade()
{
    if (isAlive() == 1) {
        u8 id = pEm->id;

        if (id > 0xF) {
            if (id <= 0x20) {
                if (id == 0x18) {
                    return 0;
                }
                return 1;
            }
        }
    }
    return 0;
}

// Ganado only: walk to `pos` and operate switch object `sw` on arrival (mode as setGoto).
void cEmWrap::setGotoSwitch(Vec* pos, int mode, void* sw)
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setGotoSwitch(pos, mode, sw);
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setGotoSwitch]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setGotoSwitch() error", no);
    }
}

// Ganado only: order a walk to `pos`. mode: 0 = target the player normally, 1 = run, 6 = walk (patrol),
// 0xB = stop; the room scripts also pass the modes directly.
void cEmWrap::setGoto(Vec* pos, int mode)
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setGoto(pos, mode);
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setGoto]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setGoto() error", no);
    }
}

// Ganado only: the mode of the goto order still in progress (0 when it has arrived / has none).
int cEmWrap::ckGoto()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckGoto();
        }
        pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckGoto]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckGoto() error", no);
    }
    return 0;
}

// Ganado or dog (0x2D): 1 when the enemy may be reset (moved back to its spawn / respawned) because
// it is off-screen and idle.
int cEmWrap::ckResetEnable()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckResetEnable();
        } else {
            u8 id = pEm->id;

            if (id == 0x2D) {
                return ((cEmDog*) pEm)->ckResetEnable();
            }
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckResetEnable]", id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckResetEnable() error", no);
    }
    return 0;
}

// Ganado only: reset the enemy to its list position (used by rooms that recycle enemies out of view).
void cEmWrap::setReset()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setReset();
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setReset]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setReset() error", no);
    }
}

// Dog (0x2D) only: cEmDog::setReset(a, b).
void cEmWrap::setReset(int a, int b)
{
    if (isAlive() == 1) {
        u8 id = pEm->id;

        if (id == 0x2D) {
            ((cEmDog*) pEm)->setReset(a, b);
        } else {
            pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[setReset]", id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setReset() error", no);
    }
}

// Ganado, dog (0x2D) or em36: 1 when the enemy has found/noticed the player.
int cEmWrap::ckFindPL()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckFindPL();
        } else {
            u8 id = pEm->id;

            if (id == 0x2D) {
                return ((cEmDog*) pEm)->ckFindPL();
            }
            if (id == 0x36) {
                return ((cEm36*) pEm)->ckFindPL();
            }
            err("EM_SET_NO(%d) id[%2x] invalid.[ckFindPL]", id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckFindPL() error", no);
    }
    return 0;
}

// Ganado or em36: force "player found" (the enemy turns hostile at once).
void cEmWrap::setFindPL()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->setFindPL();
        } else {
            u8 id = pEm->id;

            if (id == 0x36) {
                ((cEm36*) pEm)->setFindPL();
            } else {
                err("EM_SET_NO(%d) id[%2x] invalid.[setFindPL]", id);
            }
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::setFindPL() error", no);
    }
}

// Ganado only: distance-to-player value; -1.0 when unavailable. Dead in every module (see note).
f32 cEmWrap::get_l_pl()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return pEm->Guard_r;   // dead in every module: the field is a guess (only the strings and the -1.0 pool survive)
        }
        err("EM_SET_NO(%d) id[%2x] invalid.[get_l_pl]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::get_l_pl() error", no);
    }
    return -1.0f;
}

// Ganado only: forget the player (back to idle / patrol behaviour).
void cEmWrap::clearFindPL()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            ((cEmGanado*) pEm)->clearFindPL();
        } else {
            err("EM_SET_NO(%d) id[%2x] invalid.[clearFindPL]", pEm->id);
        }
    } else {
        err("EM_SET_NO(%d) cEmWrap::clearFindPL() error", no);
    }
}

// Ganado only: 1 when a crossbow Ganado is in its firing state (rooms wait for the shot before an event).
int cEmWrap::ckBowgunFire()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckBowgunFire();
        }
        pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckBowgunFire]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckBowgunFire() error", no);
    }
    return 0;
}

// Ganado only: 1 when the Plaga has burst out of this Ganado's head.
int cEmWrap::ckParasite()
{
    if (isAlive() == 1) {
        if (isNormalGanade() == 1) {
            return ((cEmGanado*) pEm)->ckParasite();
        }
        pLog->err(0, 0, "EM_SET_NO(%d) id[%2x] invalid.[ckParasite]", pEm->id);
    } else {
        err("EM_SET_NO(%d) cEmWrap::ckParasite() error", no);
    }
    return 0;
}

// Scan every live enemy: returns 1 if any has found the player. With `dist` non-null also writes the
// distance to the nearest such enemy (0 and return 0 when none).
int SceCkFindPL(f32* dist)
{
    f32 min = 100000000000000.0f;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* p = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        cEmWrap em;
        Vec pos;

        em.setPtr(p, 0);
        if (em.ckFindPL() == 1) {
            f32 d;

            if (dist == 0) {
                return 1;
            }
            em.getPos(&pos);
            d = PSVECSquareDistance(&pos, &pPL->pos);
            if (min > d) {
                min = d;
            }
        }
    }
    if (dist != 0) {
        if (min < 100000000000000.0f) {
            *dist = SQRTF(min);
            return 1;
        }
        *dist = 0.0f;
        return 0;
    }
    return 0;
}
