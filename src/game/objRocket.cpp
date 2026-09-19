#include "atari.h"
#include "light.h"
#include "at_sub2.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "item.h"
#include "pl_wep.h"
#include "player.h"
#include "pl_body.h"

// Rocket launcher (weapon 0x13) and its rocket: the launcher carries a loaded cObjRocket on its
// muzzle parts, launch() sends it along the marker line, drop() leaves an empty launcher model.

extern "C" {
int MotionMove(cModel* m, int a);
int MotionGetState(cModel* m);
double atan2(double y, double x);
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

// Weapon archive (pG->pWepArc): offsets to its sub-files like the player archive.
#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWep, no)

// Pointer store through a reference: the following `pG` load stays below it.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModel*& d, cModel* v) { d = v; }
static inline void PSet(cCoord*& d, cCoord* v) { d = v; }

// Builds the rocket model (player archive 0x70/0x71; pink tint for the special launcher,
// weapon_type 1), no collision, a 500-unit light; type 0 = loaded, waiting on the launcher.
void cObjRocket::init()
{
    cModelInfo* info;

    info = (cModelInfo*) modelInit(PL_ARC_PTR(pG->pPlayer, 0x70), PL_ARC_PTR(pG->pPlayer, 0x71));
    if (info == 0) {
        pLog->err(0, 0, "cObjRocket::init() failed.");
        return;
    }
    if (pG->weapon_type == 1) {
        info->color[0] = 0xFF;
        info->color[1] = 0x78;
        info->color[2] = 0x80;
        info->color[3] = 0xFF;
    }
    sub2B4.atari.throughOn();
    LightInfo.init2(1, 1, &lightPos, &lightSize, 1);
    type = 0;
}

const Vec cObjRocket::lightPos = { 0.0f, 0.0f, 0.0f };
const Vec cObjRocket::lightSize = { 500.0f, 0.0f, 0.0f };

// r_no_0 0: rides on the launcher (hidden while the weapon is transparent); 1: in flight — moves
// by its motion, explodes (0x12 blast hit check, 8000 wide) on water, on an enemy / object hit
// (0xD line check, 3000) or on the map, and rings the bell (bell_pos / bell_stat); 2: destroyed.
void cObjRocket::move()
{
    static f32 blastDmWidth = 8000.0f;
    static int blastDbg = 0;
    cPlayer* pl = pPL;

    switch (r_no_0) {
    case 0:
        if (pl->Wep->m_pWep && pl->Wep->m_pWep->isTrans()) {
            be_flag |= 2;
        } else {
            be_flag &= ~2;
        }
        matUpdate();
        break;
    case 1: {
        f32 wh;
        Vec hit;
        Vec sc;

        rocket.oldPos = pos;
        motionMove();
        if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
            AtEffInfo* info;

            pos.y = wh + 20.0f;
            info = EatMgr.getEffInfo(EAT_ET_WATER);
            if (info == 0) {
                pLog->err(0, 0, "GRENADE CANT FOUND WATER INFORMATION");
                pLog->err(0, 0, "  PLEASE SET EatMgr.registEffInfo()");
                return;
            }
            EstSet(0, -1, &pos, 0, info->eff0D[0], (u8) info->eff0D[1], 0, 0, 0, 0);
            if (info->eff0D[0] == 0 && info->eff0D[1] == 0x15) {
                Vec a;
                Vec b;
                Vec nrmW;
                Vec hitW;

                a.x = pos.x;
                a.y = pos.y - 300.0f;
                a.z = pos.z;
                b.x = pos.x;
                b.y = pos.y + 300.0f;
                b.z = pos.z;
                if (EatMgr.hitCheck(&a, &b, &hitW, &nrmW, 0, 0) == 0 || nrmW.y > 0.9f) {
                    EstSet(0, -1, &hitW, 0, 0, 0x28, 0, 0, 0, 0);
                }
            }
            AddWaterPower(&pos, 1.0f);
            SndCall(1, 0x17, &pos, 0, 0, 0);
            PlWepHitCheck2(0, &rocket.oldPos, &pos, 0x12, 0, blastDmWidth);
            r_no_0 = 2;
        } else {
            u32 res;

            res = PlWepHitCheck2(0, &rocket.oldPos, &pos, 0xD, 1, 3000.0f);
            if (res) {
                StaFlagOn(pG, STA_SE_BURST);
                memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
                pG->bell_stat = 1;
                PlWepHitCheck2(0, &rocket.oldPos, &pos, 0x12, 0, blastDmWidth);
                EstSet(0, -1, &pos, 0, 0, 0x27, 0, 10, 0, 0);
                SndCall(1, 0x14, &pos, 0, 0, 0);
                r_no_0 = 2;
            } else {
                Vec nrm;
                u32 attr;
                Vec a;
                Vec b;

                attr = EatMgr.hitCheck(&rocket.oldPos, &pos, &hit, &nrm, 0, 0);
                if (attr) {
                    AtEffInfo* info;

                    PSVECScale(&nrm, &sc, 1000.0f);
                    sc.y = 0.0f;
                    PSVECAdd(&hit, &sc, &hit);
                    info = EatMgr.getEffInfo(EatGetEffectType(attr));
                    if (info) {
                        EstSet(0, -1, &hit, 0, info->eff0D[0], (u8) info->eff0D[1], 0, 10, 0, 0);
                        if (info->eff0D[0] == 0 && info->eff0D[1] == 0x15) {
                            Vec nrm2;
                            Vec hit2;

                            a.x = pos.x;
                            a.y = pos.y - 300.0f;
                            a.z = pos.z;
                            b.x = pos.x;
                            b.y = pos.y + 300.0f;
                            b.z = pos.z;
                            if (EatMgr.hitCheck(&a, &b, &hit2, &nrm2, 0, 0) == 0 || nrm2.y > 0.9f) {
                                EstSet(0, -1, &hit2, 0, 0, 0x28, 0, 0, 0, 0);
                            }
                        }
                    } else {
                        EstSet(0, -1, &hit, 0, 0, 0x27, 0, 10, 0, 0);
                        EstSet(0, -1, &hit, 0, 0, 0x1A, 0, 0, 0, 0);
                    }
                    StaFlagOn(pG, STA_SE_BURST);
                    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
                    pG->bell_stat = 1;
                    SndCall(1, 0x14, &pos, 0, 0, 0);
                    PlWepHitCheck2(0, &rocket.oldPos, &pos, 0x12, 0, blastDmWidth);
                    r_no_0 = 2;
                }
            }
        }
        if (motFrame >= (f32) (motSeqMax - 1)) {
            r_no_0 = 2;
        }
        rocket.timer--;
        if (rocket.timer < 0) {
            r_no_0 = 2;
        }
        break;
    }
    case 2:
        ObjMgr.destroy(this);
        r_no_0 = 3;
        break;
    }
}

// Starts the flight: flight motion (player archive 0x74), exhaust effect 0x29, 300 frames of life.
void cObjRocket::fire()
{
    MotionSetCore(this, &pMotion, PL_ARC_PTR(pG->pPlayer, 0x74), 0, 0, 1, 0);
    MotionMove(this, 0);
    EstSet((int) this, -1, 0, 0, 0, 0x29, 0, 10, 0, 0);
    rocket.timer = 300;
    type = 1;
    r_no_0 = 1;
}

// A rocket in flight is dropped when an event starts.
void cObjRocket::beginEvent()
{
    if (r_no_0) {
        ObjMgr.destroy(this);
    }
}

// No rocket in flight, none loaded.
cObjLauncher::cObjLauncher()
{
    launcher.flags = 0;
    launcher.rocket = 0;
}

// Destroys the loaded rocket at once (destroyNow) when the launcher goes.
cObjLauncher::~cObjLauncher()
{
    if (launcher.rocket) {
        ObjMgr.destroyNow(launcher.rocket);
    }
}

// Launcher model (player archive 0x76/0x75; blue tint for the special one), gripped on the back
// (or in hand for the infinite launcher, weapon_type 2), idle motions from the weapon archive; a
// rocket is loaded when the item has ammo, else the player's "empty launcher" flag 0x400 is set.
void cObjLauncher::init(cModel* parent)
{
    cModelInfo* info;

    wep.x24 = 0x35;
    info = (cModelInfo*) modelInit(PL_ARC_PTR(pG->pPlayer, 0x76), PL_ARC_PTR(pG->pPlayer, 0x75));
    if (info == 0) {
        pLog->err(0, 0, "cObjLauncher::init() modelInit() failed.");
        return;
    }
    if (pG->weapon_type == 1) {
        info->color[0] = 0xA0;
        info->color[1] = 0xD0;
        info->color[2] = 0xE0;
        info->color[3] = 0xFF;
    }
    sub2B4.atari.throughOn();
    LightInfo.init2(1, 1, &cObjRocket::lightPos, &cObjRocket::lightSize, 1);
    grip(0);
    PSet(wep.parent, parent);
    if (pG->weapon_type != 2) {
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x1E));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x1E));
    } else {
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x1D));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x1D));
    }
    resetMotion();
    if (ItemMgr.bulletNum()) {
        loadRocket();
    }
    if (bulletNum() == 0) {
        pPL->stat |= 0x400;
    }
}

// Creates the cObjRocket (ObjMgr id 0x22) and hangs it on parts 0 at the muzzle offset.
void cObjLauncher::loadRocket()
{
    static Vec pos0 = { -136.0f, -30.72f, 118.85f };
    static Vec ang0 = { 1.5707964f, 0.0f, -1.5707964f };

    if (launcher.rocket) {
        return;
    }
    launcher.rocket = (cObjRocket*) ObjMgr.createBack(0x22);
    if (launcher.rocket == 0) {
        pLog->err(0, 0, "Wep13_init() cObjRocket CREATE FAILED");
        return;
    }
    launcher.rocket->init();
    launcher.rocket->setParent(getPartsPtr(0), &pos0, &ang0);
    launcher.rocket->move();
}

// wep.mode 2 (fire): step 0 launches the loaded rocket (unless ckBoss took the shot) and reloads
// for the infinite launcher / debug infinite ammo; step 1 waits for the fire motion to end.
void cObjLauncher::moveFire()
{
    if (wep.step == 0) {
        if (DbgFlagChk(pG, DBG_INF_BULLET) || (DbgFlagChk(pG, DBG_INF_BULLET2))) {
            if (launcher.rocket == 0) {
                loadRocket();
            }
        }
        if (launcher.rocket) {
            if (ckBoss() == 0) {
                launch();
                if (pG->weapon_type == 2) {
                    loadRocket();
                } else if (DbgFlagChk(pG, DBG_INF_BULLET) || (DbgFlagChk(pG, DBG_INF_BULLET2))) {
                    loadRocket();
                }
            }
            wep.step = 1;
        }
    } else {
        if (MotionGetState(this)) {
            wep.mode = 0;
            wep.step = 0;
        }
    }
}

// Special launcher (weapon_type 1) fired within 30 degrees of the living boss: instead of a rocket,
// sets System_flg 0x400 and the boss's room flag (m_pBossRmf) so the scenario kills it. Returns 1.
int cObjLauncher::ckBoss()
{
    cEm* boss = (cEm*) pPL->m_pBoss;

    if (pG->weapon_type == 1 && boss && boss->hp > 0) {
        Vec a;
        Vec b;

        if (fabsf(Muku(&pPL->pos, &boss->pos, pPL->ang.y, 6.2831855f)) > 0.5235988f) {
            return 0;
        }
        partsWorldCalc();
        getMarkerPos(&a, &b);
        PSVECSubtract(&b, &a, &a);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        SND_BIT_SET(&pG->Room_flg[0], (u32) pPL->m_pBossRmf);
        return 1;
    }
    return 0;
}

// Sends the loaded rocket along the marker line from/to (yaw / elevation from the difference),
// muzzle flash 0x47, launch SE, Status_flg[0] 0x00800000; flags bit0 = a rocket is flying.
void cObjLauncher::launch()
{
    Vec d;

    PSVECSubtract(&launcher.to, &launcher.from, &d);
    launcher.rocket->ang.x = -VecElevation(&d);
    launcher.rocket->ang.y = atan2(d.x, d.z);
    launcher.rocket->ang.z = 0.0f;
    launcher.rocket->pos = launcher.from;
    launcher.rocket->pParts->pParent = launcher.rocket;
    launcher.rocket->be_flag |= 2;
    launcher.rocket->fire();
    launcher.flags |= 1;
    launcher.rocket = 0;
    EstSet((int) this, -1, 0, 0, 0x47, 0, 0, 10, 0, 0);
    SndCall(2, 0, &pos, 0, 0, 0);
    StaFlagOn(pG, STA_PL_FIRE);
}

// wep.mode 5 (drop): the launcher is thrown away once.
void cObjLauncher::moveDrop()
{
    if (wep.step == 0) {
        drop(1);
        wep.step = 1;
    }
}

// Leaves an empty launcher model (cObjWep id 0x23, mode 5) on the ground under the player (only on
// a floor hit with attr 0x01000000 and not 0x40), plays the drop SE, hides this weapon.
void cObjLauncher::drop(int se)
{
    cObjWep* w;
    f32 len = 10000.0f;   // unused in the original too: it only puts 10000 before 0.0 in the constant pool

    w = (cObjWep*) ObjMgr.createBack(0x23);
    if (w) {
        Vec a;
        Vec b;
        Vec hit;
        u32 attr;

        w->init(pPL);
        w->parentRelease();
        w->pMotion = 0;
        w->pParts->ang.x = 0.0f;
        w->pParts->ang.y = 0.0f;
        w->pParts->ang.z = 0.0f;
        w->wep.mode = 5;
        w->wep.step = 1;
        a = w->pos;
        b.x = w->pos.x;
        b.y = w->pos.y - 10000.0f;
        b.z = w->pos.z;
        attr = EatMgr.hitCheck(&a, &b, &hit, 0, 0, 0);
        if ((attr & 0x01000040) != 0x01000000) {
            ObjMgr.destroy(w);
        } else {
            w->pos = hit;
            FSet(w->ang.z, 0.0f);
            FSet(w->ang.x, 0.0f);
            FSet(w->pos.y, w->pos.y + 100.0f);
            w->ang.y = LIMIT_ANGLE(pPL->ang.y + 1.5707964f);
            if (se) {
                SndCall(2, 3, &w->pParts->world, 0, 0, 0);
            }
        }
    }
    launcher.flags &= ~1;
    setDisp(0, 0);
}

// Hangs the launcher in the right hand (parts 10) with the hand motion, or on the back (gripBack).
void cObjLauncher::grip(int onoff)
{
    if (pG->weapon_type == 2 || onoff == 1) {
        PSet(pParts->pParent, pPL->getPartsPtr(10));
        motionSet(WEP_ARC_PTR(0x1D), 0, 0, 1, 0);
    } else {
        gripBack();
    }
}

// Hangs the launcher on the player's back (parts 2) with the back motion.
void cObjLauncher::gripBack()
{
    PSet(pParts->pParent, pPL->getPartsPtr(2));
    motionSet(WEP_ARC_PTR(0x1F), 0, 0, 1, 0);
}

// Weapon interrupted (event / damage): the normal launcher is dropped if it was fired, else goes
// back to the back grip.
void cObjLauncher::interrupt()
{
    cObjWep::interrupt();
    if (pG->weapon_type != 2) {
        if (launcher.flags & 1) {
            drop(1);
        } else {
            grip(0);
        }
    }
}

// Aim key (Key.on 0x10) counts only when there is a rocket in the inventory.
int cObjLauncher::keyKamae()
{
    if (Key.on & 0x10) {
        if (ItemMgr.bulletNum()) {
            return 1;
        }
    }
    return 0;
}

// Fills the player's motion table (m_MotTbl) with the launcher's stand / walk / aim / damage motions
// from the weapon archive, and shows the launcher in the hands (or the empty-handed set when
// stat 0x400: no rocket).
void cObjLauncher::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0], WEP_ARC_PTR(0x8));
    PSet(pl->m_MotTbl[2], WEP_ARC_PTR(0x9));
    PSet(pl->m_MotTbl[6], WEP_ARC_PTR(0xB));
    PSet(pl->m_MotTbl[8], WEP_ARC_PTR(0xA));
    PSet(pl->m_MotTbl[0xB], WEP_ARC_PTR(0xC));
    PSet(pl->m_MotTbl[0xD], WEP_ARC_PTR(0xD));
    PSet(pl->m_MotTbl[0xF], WEP_ARC_PTR(0xE));
    PSet(pl->m_MotTbl[1], WEP_ARC_PTR(0x21));
    PSet(pl->m_MotTbl[3], WEP_ARC_PTR(0x22));
    PSet(pl->m_MotTbl[7], WEP_ARC_PTR(0x24));
    PSet(pl->m_MotTbl[9], WEP_ARC_PTR(0x23));
    PSet(pl->m_MotTbl[0xC], WEP_ARC_PTR(0x25));
    PSet(pl->m_MotTbl[0xE], WEP_ARC_PTR(0x26));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x27));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    if (pG->weapon_type != 2) {
        PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x2A));
        PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x2B));
        PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x2C));
        PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x2D));
        PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x28));
        PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x29));
    }
    PSet(pl->m_MotTbl[0x55], WEP_ARC_PTR(0x1A));
    PSet(pl->m_MotTbl[0x59], WEP_ARC_PTR(0x2E));
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x1B));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x1C));
    if (!(pl->stat & 0x400)) {
        pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
        pl->setRightHand(1);
        pl->setLeftHand(4);
        setDisp(0, 1);
        loadRocket();
    } else {
        pl->Body->initWepHand((u32) PL_ARC_PTR(pG->pPlayer, 0x12));
        pl->setRightHand(1);
        pl->setLeftHand(0);
        setDisp(0, 0);
    }
}

template <class T>
// destroy() with the deferred flag cleared: the object is removed immediately.
void cManager<T>::destroyNow(T* p)
{
    u8 f = flag;

    flag = 0;
    destroy(p);
    flag = f;
}
