// wep16 module: Leon's knife (cObjKnife; routines wep/pl_knife.cpp = the DOL's game/pl_knife.cpp).
//
// The knife as an equipped "weapon" (the mercenaries / Krauser-style knife slot): cObjKnife
// (wep_mod.h) is a cObjWep hanging on the player's right hand with the player archive's knife
// motion, no fire / reload modes of its own; PlKnifeMove (the knife routine) is registered as the
// WeaponMoveFunc so routine 6 slashes. Wep16_init is the WeaponInitFunc.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "motion.h"


void ObjKnife_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjKnife (ObjMgr id 0x24),
// inits it (no parent argument: the player is pPL), stores it as Wep->m_pWep, installs the knife
// footwork motions, loads the effects (archive 0x4 as group 0x4A) and points the debug preview
// PlWepMot at the stance motions.
void Wep16_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjKnife* obj = (cObjKnife*) ObjMgr.createBack(cObjMgr::ID_WEP_KNIFE);

    if (obj == 0) {
        pLog->err(0, 0, "Wep16_init() cObjWep CREATE FAILED");
        return;
    }
    obj->init();
    pl->Wep->m_pWep = obj;
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP16, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x2A);
    PlWepMot[1] = WEP_ARC_PTR(0x2C);
    PlWepMot[2] = WEP_ARC_PTR(0x2E);
}

// The knife's own init (the cObjWep::init(parent) virtual is not overridden): model 0x6 / texture
// 0x5, atari bits 8/9 off, hung on the player's right hand (parts 10), light area, and the player
// archive's knife-in-hand motion 0x1B started on it.
void cObjKnife::init()
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjKnife::init() failed.");
        return;
    }
    AtariFlagsAnd(&atari, 0xFCFF);
    pParts->pParent = pPL->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = pPL;
    resetMotion();
    MotionSetCore(this, &this->Motion, PL_ARC_PTR(pG->pPlayer, 0x1B), 0, 0, 0, 0);
}

// ObjInitFunc[0x24]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjKnife_init(cObj* obj)
{
    new (obj) cObjKnife();
}

// Fills the player's motion table with the knife-in-hand footwork motions (idle, no walk [1],
// run, turns, back; no damage set of its own; 0x3D stays the player archive's) and sets the
// weapon hand model (right hand 1, bare left hand 0).
void cObjKnife::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x08);
    NO_MOT(pl, 0x01);
    WEP_MOT(pl, 0x02, 0x09);
    WEP_MOT(pl, 0x03, 0x1B);
    WEP_MOT(pl, 0x04, 0x09);
    WEP_MOT(pl, 0x05, 0x09);
    WEP_MOT(pl, 0x06, 0x0C);
    WEP_MOT(pl, 0x07, 0x1D);
    WEP_MOT(pl, 0x08, 0x0A);
    WEP_MOT(pl, 0x09, 0x1C);
    WEP_MOT(pl, 0x0A, 0x0A);
    WEP_MOT(pl, 0x0B, 0x0D);
    WEP_MOT(pl, 0x0C, 0x1E);
    WEP_MOT(pl, 0x0D, 0x0E);
    WEP_MOT(pl, 0x0E, 0x1F);
    WEP_MOT(pl, 0x0F, 0x0F);
    WEP_MOT(pl, 0x10, 0x20);
    WEP_MOT(pl, 0x12, 0x0C);
    WEP_MOT(pl, 0x13, 0x0C);
    PLA_MOT(pl, 0x3D, 0x5D);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(1);
    pl->setLeftHand(0);
}

// REL entry: registers the weapon init routine, the knife routine as the weapon move and the
// object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep16_init;
    WeaponMoveFunc = PlKnifeMove;
    ObjInitFunc[0x24] = ObjKnife_init;
    OSReport("Wep16 KNIFE prolog Ok\n");
}

// REL exit: nothing to free (the ObjInitFunc slot is left set).
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
