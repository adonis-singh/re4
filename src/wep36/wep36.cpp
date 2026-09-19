// wep36 module: a hand "weapon" (cObjHand, no weapon model). Creates the object, gives the
// player the hand model and fills the player's motion table from the weapon archive.

#include "wep_mod.h"

cObjWep* equipWeapon(cPlayer* pl);

// Motion table stores go through wep_mod.h's PSet: the original reloads pG after every one.
// One of the four per-character empty-hand modules (wep34..wep37, the same code with the
// character's hand model / motion table); like wep00 there is no weapon routine.

// WeaponInitFunc of the module (cPlayer::weaponInit with the player): creates the hand object,
// stores it as Wep->m_pWep and installs the hand motions. (The error string still says Wep13.)
static void Wep36_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (obj == 0) {
        pLog->err(0, 0, "Wep13_init() failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
    }
}

// WeaponMoveFunc: the empty hand has no weapon routine.
void Wep36_move(cPlayer* pl)
{
}

// Creates the cObjHand (ObjMgr id 0x3D), inits it on the player and gives the player the bare
// hand model from the player archive (0x12; right hand 1, left hand 0). NULL when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x3d);

    if (obj == 0) {
        pLog->err(0, 0, "Wep36_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    pl->Body->initWepHand((u32) PL_ARC_PTR(pG->pPlayer, 0x12));
    pl->setRightHand(1);
    pl->setLeftHand(0);
    return obj;
}

// ObjInitFunc[0x3D]: placement-constructs the hand object in the work cObjMgr::construct hands over.
void ObjHand_init(cObj* obj)
{
    new (obj) cObjHand();
}

// Fills the player's motion table (m_MotTbl) with the unarmed footwork set of the weapon archive
// (idle, walk, run, turns, back, the 0x39..0x42 damage set; 0x3D stays the player archive's)
// and re-sets both hands to the bare hand model (0x12, right 0 / left 0).
void cObjHand::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x04));
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x05));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x07));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x06));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x08));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x09));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x0A));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x15));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x16));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x12));
    pl->Body->initWepHand((u32) PL_ARC_PTR(pG->pPlayer, 0x12));
    pl->setRightHand(0);
    pl->setLeftHand(0);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep36_init;
    WeaponMoveFunc = Wep36_move;
    ObjInitFunc[0x3d] = ObjHand_init;
    OSReport("Wep36 HAND prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x3d] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
