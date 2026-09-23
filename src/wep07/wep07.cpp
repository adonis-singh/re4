// wep07 module: the shotgun (cObjShotgun, object id 0x2B; routines wep/pl_shotgun.cpp).
//
// cObjShotgun is the cObjWep (game/objWep.cpp) of the pump shotgun (weapon_no 7), hanging on the
// player's right hand (parts 10) and driven by mode / step from the shotgun routines:
// mode 2 -> moveFire (recoil motion, then the pump ejects the shell at frame 20), mode 4 ->
// moveReload (shell-by-shell motion by tune level; the mode ends itself with the motion).
// Wep07_init is the module's WeaponInitFunc, PlShotgunMove its WeaponMoveFunc; the module object
// carries the class, the entry points and the ObjInitFunc slot.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlShotgunMove(cPlayer* pl);   // wep/pl_shotgun.cpp

void ObjShotgun_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjShotgun as Wep->m_pWep,
// inits it on the player, installs its motions, loads the muzzle-flash effects (archive 0x4 as
// group 0x3B) and points the debug preview PlWepMot at the aim idles 0x1A/0x20/0x22.
void Wep07_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_SHOTGUN);

    if (obj == 0) {
        pLog->err(0, 0, "Wep07_init() cObjWep CREATE FAILED");
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP07, 1);
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
// hung on the right hand, light area, weapon list id 0x2C, idle motion 0x31, shotFrame[0..2] = 0x2E,
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
    m_pParent = parent;
    itemId = 0x2C;
    motReset[0] = WEP_ARC_PTR(0x31);
    resetMotion();
    shotFrame[0] = 0x2E;
    shotFrame[1] = 0x2E;
    shotFrame[2] = 0x2E;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire, set by the shotgun fire00): step 0 starts the recoil + pump motion (0x30,
// 0x32 on the last shell), shot SE, pad vibration, Status_flg[0] bit23 (shot noise), muzzle flash
// 0x3B; step 1 ejects the spent shell with the pump SE at frame 20 and returns to mode 0 at the
// motion's end.
void cObjShotgun::moveFire()
{
    if (step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x30);
        } else {
            m = WEP_ARC_PTR(0x32);
        }
        MotionSetCore(this, &this->Motion, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        StaFlagOn(pG, STA_PL_FIRE);
        EstSet(this, -1, 0, 0, EFF_WEP07, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        step = 1;
    } else {
        if (MotionCheckCrossFrame(&Motion, 20.0f)) {
            setCartridge();
            SndCall(2, 2, &getPartsPtr(0)->world, 0, 0, 0);
        }
        if (MotionGetState(this)) {
            mode = 0;
            step = 0;
        }
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the tune level (0x2B/0x2D/0x2F) with
// the level's SE (7/0x20/0x21); step 1 refills the shells at the level's frame (30/26/17), plays
// the pump SE at 66/57/40 and returns to mode 0 at the motion's end.
void cObjShotgun::moveReload()
{
    static const f32 reloadEnd[3] = { 30.0f, 26.0f, 17.0f };
    static const f32 reloadSe[3] = { 66.0f, 57.0f, 40.0f };
    int lv = pG->weapon_lv_reload;

    if (step == 0) {
        void* m;
        u16 se;

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
        switch (lv) {
        default:
            se = 7;
            break;
        case 1:
            se = 0x20;
            break;
        case 2:
            se = 0x21;
            break;
        }
        m_StopSeId = SndCall(2, se, &pParts->world, 0, 0, 0);
        step = 1;
    } else {
        if (MotionCheckCrossFrame(&Motion, reloadEnd[lv])) {
            ItemMgr.reload();
        }
        if (MotionCheckCrossFrame(&Motion, reloadSe[lv])) {
            SndCall(2, 2, &getPartsPtr(0)->world, 0, 0, 0);
        }
        if (MotionGetState(this)) {
            mode = 0;
            step = 0;
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
// turns, back, the 0x39..0x42 and 0x5D/0x5E damage set, 0x57/0x5B knife transitions; 0x3D stays
// the player archive's) and sets the weapon hand models (left hand 2, right hand 1).
void cObjShotgun::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0A);
    WEP_MOT(pl, 0x01, 0x0B);
    WEP_MOT(pl, 0x02, 0x10);
    WEP_MOT(pl, 0x03, 0x11);
    WEP_MOT(pl, 0x04, 0x10);
    WEP_MOT(pl, 0x05, 0x10);
    WEP_MOT(pl, 0x06, 0x14);
    WEP_MOT(pl, 0x07, 0x15);
    WEP_MOT(pl, 0x08, 0x12);
    WEP_MOT(pl, 0x09, 0x13);
    WEP_MOT(pl, 0x0A, 0x12);
    WEP_MOT(pl, 0x0B, 0x16);
    WEP_MOT(pl, 0x0C, 0x17);
    WEP_MOT(pl, 0x0D, 0x0C);
    WEP_MOT(pl, 0x0E, 0x0D);
    WEP_MOT(pl, 0x0F, 0x0E);
    WEP_MOT(pl, 0x10, 0x0F);
    WEP_MOT(pl, 0x12, 0x14);
    WEP_MOT(pl, 0x13, 0x14);
    WEP_MOT(pl, 0x5B, 0x33);
    WEP_MOT(pl, 0x57, 0x34);
    WEP_MOT(pl, 0x39, 0x26);
    WEP_MOT(pl, 0x3A, 0x27);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x28);
    WEP_MOT(pl, 0x42, 0x29);
    WEP_MOT(pl, 0x3F, 0x24);
    WEP_MOT(pl, 0x40, 0x25);
    WEP_MOT(pl, 0x5D, 0x1C);
    WEP_MOT(pl, 0x5E, 0x1D);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setLeftHand(2);
    pl->setRightHand(1);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep07_init;
    WeaponMoveFunc = PlShotgunMove;
    ObjInitFunc[0x2B] = ObjShotgun_init;
    OSReport("Wep07 SHOTGUN prolog Ok\n");
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
