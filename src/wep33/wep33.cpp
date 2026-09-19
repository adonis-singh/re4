// wep33 module: the pump shotgun (cObjShotgun, object id 0x2B; routines wep/pl_shotgun.cpp).
//
// The mercenaries' build of wep07 (weapon_no 0x21): the same cObjShotgun hanging on the player's
// right hand (parts 10), driven by wep.mode / wep.step from the shotgun routines (mode 2 fire
// with the pump shell ejection at frame 20, mode 4 shell-by-shell reload; both end themselves),
// with a slightly different motion table. Wep33_init is the WeaponInitFunc, PlShotgunMove the
// WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlShotgunMove(cPlayer* pl);   // wep/pl_shotgun.cpp

class cObjShotgun : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

void ObjShotgun_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjShotgun as Wep->m_pWep,
// inits it on the player, installs its motions, loads the muzzle-flash effects (archive 0x4 as
// group 0x3B) and points the debug preview PlWepMot at the aim idles 0x1A/0x20/0x22.
void Wep33_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x2B);

    if (obj == 0) {
        pLog->err(0, 0, "Wep33_init() cObjWep CREATE FAILED");
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x3B, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x1A);
    PlWepMot[1] = WEP_ARC_PTR(0x20);
    PlWepMot[2] = WEP_ARC_PTR(0x22);
}

// ObjInitFunc[0x2B]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjShotgun_init(cObj* obj)
{
    new (obj) cObjShotgun();
}

// cObjWep::init override (parent = the player): model 0x5 / texture 0x6, atari bits 8/9 off,
// hung on the right hand, light area, weapon list id 0x2C, idle motion 0x31, wep.x18..x1A = 0x2E,
// default lock spread.
void cObjShotgun::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x5), WEP_ARC_PTR(0x6)) == 0) {
        pLog->err(0, 0, "cObjShotgun::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    U16Set(wep.x24, 0x2C);
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x31));
    resetMotion();
    wep.x18 = 0x2E;
    wep.x19 = 0x2E;
    wep.x1A = 0x2E;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// wep.mode == 2 (fire, set by the shotgun fire00): step 0 starts the recoil + pump motion (0x30,
// 0x32 on the last shell), the shot SE, Status_flg[0] bit23 (shot noise) and the muzzle flash
// 0x3B; step 1 ejects the spent shell with the pump SE at frame 20 and returns to mode 0 at the
// motion's end.
void cObjShotgun::moveFire()
{
    if (wep.step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x30);
        } else {
            m = WEP_ARC_PTR(0x32);
        }
        MotionSetCore(this, &this->Motion, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        // the EstSet stack zeros come from an SImode pseudo set after the SndCall (wep44)
        int type = 0;
        StaFlagOn(pG, STA_PL_FIRE);
        EstSet((int) this, -1, 0, 0, 0x3B, type, 0, 0xA, 0, 0);
        wep.step = 1;
    } else {
        if (MotionCheckCrossFrame(&Motion, 20.0f)) {
            setCartridge();
            SndCall(2, 2, &getPartsPtr(0)->world, 0, 0, 0);
        }
        if (MotionGetState(this)) {
            wep.mode = 0;
            wep.step = 0;
        }
    }
}

// wep.mode == 4 (reload): step 0 starts the reload motion of the tune level (0x2B/0x2D/0x2F) with
// the shell SE 7; step 1 refills the shells at the level's frame (30/26/17), plays the pump SE at
// 66/60/40 and returns to mode 0 at the motion's end.
void cObjShotgun::moveReload()
{
    static const f32 reloadEnd[3] = { 30.0f, 26.0f, 17.0f };
    static const f32 reloadSe[3] = { 66.0f, 60.0f, 40.0f };
    int lv = pG->weapon_lv_reload;

    if (wep.step == 0) {
        void* m;

        switch (lv) {
        default:
            m = WEP_ARC_PTR(0x2B);
            break;
        case 1:
            m = WEP_ARC_PTR(0x2D);
            break;
        case 2:
            m = WEP_ARC_PTR(0x2F);
            break;
        }
        motionSet(m, 0, 0, 0, 0);
        wep.seHandle = SndCall(2, 7, &pParts->world, 0, 0, 0);
        wep.step = 1;
    } else {
        if (MotionCheckCrossFrame(&Motion, reloadEnd[lv])) {
            ItemMgr.reload();
        }
        if (MotionCheckCrossFrame(&Motion, reloadSe[lv])) {
            wep.seHandle = SndCall(2, 2, &getPartsPtr(0)->world, 0, 0, 0);
        }
        if (MotionGetState(this)) {
            wep.mode = 0;
            wep.step = 0;
        }
    }
}

// Ejects a spent shell: an obj10 model (archive 0x8/0x9) from the right hand's ejection port
// offset (-201, -6.9, 1.8) with a random +-15 spread, gravity 10, 40 frames, landing effect 0x13.
void cObjShotgun::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -201.0f;
    pos.y = -6.9f;
    pos.z = 1.8f;
    PSMTXMultVec(parts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 60.0f;
    spd.z = 60.0f;
    spd.x += fRand1_1() * 15.0f;
    spd.y += fRand1_1() * 15.0f;
    spd.z += fRand1_1() * 15.0f;
    PSMTXMultVecSR(parts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0x8), WEP_ARC_PTR(0x9), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with the shotgun-carrying footwork motions (idle, walk, run,
// turns, back, the 0x39..0x42 and 0x5D/0x5E damage set with 0x3B/0x3C cleared, 0x57/0x5B knife
// transitions; 0x3D stays the player archive's) and sets the weapon hand models (left 2, right 1).
void cObjShotgun::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x0A));
    PSet(pl->m_MotTbl[0x01], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x04], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x05], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x15));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x0A], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x16));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x17));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x12], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x13], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x26));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x27));
    PSet(pl->m_MotTbl[0x3B], 0);
    PSet(pl->m_MotTbl[0x3C], 0);
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x28));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x29));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x24));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x25));
    PSet(pl->m_MotTbl[0x5D], WEP_ARC_PTR(0x1C));
    PSet(pl->m_MotTbl[0x5E], WEP_ARC_PTR(0x1D));
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x40));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x41));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setLeftHand(2);
    pl->setRightHand(1);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep33_init;
    WeaponMoveFunc = PlShotgunMove;
    ObjInitFunc[0x2B] = ObjShotgun_init;
    OSReport("Wep33 SHOTGUN prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x2B] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
