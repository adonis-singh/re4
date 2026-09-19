// wep26 module: Krauser's knife (cObjKnife; routines wep/pl_knife.cpp = the DOL's game/pl_knife.cpp).
//
// The wep16 module rebuilt for Krauser (pl_type 4): the same cObjKnife (a cObjWep hanging on the
// right hand with the player archive's knife motion, no fire / reload modes) with a smaller
// footwork motion set; PlKnifeMove is the WeaponMoveFunc so routine 6 slashes (1200-unit reach).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "motion.h"

void PlKnifeMove(cPlayer* pl);   // wep/pl_knife.cpp

void ObjKnife_init(cObj* obj);

// pPL read as a struct member: the load stays below the collision-flag store before it.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjKnife (ObjMgr id 0x24),
// inits it, stores it as Wep->m_pWep, installs the knife footwork motions, loads the effects
// (archive 0x4 as group 0x4E) and points the debug preview PlWepMot at the stance motions.
// (The error string still says Wep16.)
void Wep26_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjKnife* obj = (cObjKnife*) ObjMgr.createBack(0x24);

    if (obj == 0) {
        pLog->err(0, 0, "Wep16_init() cObjWep CREATE FAILED");
        return;
    }
    obj->init();
    pl->Wep->m_pWep = obj;
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x4E, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x2A);
    PlWepMot[1] = WEP_ARC_PTR(0x2C);
    PlWepMot[2] = WEP_ARC_PTR(0x2E);
}

// The knife's own init (cObjWep::init(parent) is not overridden): model 0x6 / texture 0x5, atari
// bits 8/9 off, hung on the player's right hand (parts 10), light area, the player archive's
// knife-in-hand motion 0x1B started on it.
void cObjKnife::init()
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjKnife::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = pPLS->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = pPL;
    resetMotion();
    MotionSetCore(this, &this->Motion, PL_ARC_PTR(pG->pPlayer, 0x1B), 0, 0, 0, 0);
}

// ObjInitFunc[0x24]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjKnife_init(cObj* obj)
{
    new (obj) cObjKnife();
}

// Fills the player's motion table with Krauser's knife-in-hand footwork motions (idle, run,
// turns, back; no walk [1] / [3]; 0x3D stays the player archive's) and sets the weapon hand
// model (right hand 1, bare left hand 0).
void cObjKnife::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x08));
    PSet(pl->m_MotTbl[0x01], 0);
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x09));
    PSet(pl->m_MotTbl[0x03], 0);
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x1D));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x0A));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x1B));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x1E));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x1F));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x20));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(1);
    pl->setLeftHand(0);
}

// REL entry: registers the weapon init routine, the knife routine as the weapon move and the
// object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep26_init;
    WeaponMoveFunc = PlKnifeMove;
    ObjInitFunc[0x24] = ObjKnife_init;
    OSReport("Wep26 KLAUSER KNIFE prolog Ok\n");
}

// REL exit: nothing to free (the ObjInitFunc slot is left set).
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
