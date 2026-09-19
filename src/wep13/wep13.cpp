// wep13 module: the rocket launcher. The weapon object is the DOL's cObjLauncher (id 0x23); the
// module supplies the player routines (wep/pl_rocket.cpp) and this entry object.
// Wep13_init is the module's WeaponInitFunc (creates the launcher as the player's weapon and
// loads the effects), PlRocketMove its WeaponMoveFunc; there is no ObjInitFunc slot to register
// because the DOL's cObjMgr::construct already knows id 0x23.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlRocketMove(cPlayer* pl);   // wep/pl_rocket.cpp

// WeaponInitFunc (cPlayer::weaponInit with the player): clears stat bit10 (launcher thrown
// away), creates the cObjLauncher (ObjMgr id 0x23), inits it on the player (loads its rocket),
// stores it as Wep->m_pWep, installs its motions, loads the launch effects (archive 0x6 as group
// 0x47) and points the debug preview PlWepMot at the aim idles 0xF/0x12/0x14.
void Wep13_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    pl->stat &= ~0x400;
    obj = (cObjWep*) ObjMgr.createBack(0x23);
    if (obj == 0) {
        pLog->err(0, 0, "Wep13_init() cObjWep CREATE FAILED");
    } else {
        obj->init(pl);
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x6), 0x47, 1);
        PlWepMot[0] = WEP_ARC_PTR(0xF);
        PlWepMot[1] = WEP_ARC_PTR(0x12);
        PlWepMot[2] = WEP_ARC_PTR(0x14);
    }
}

// REL entry: registers the weapon init / move routines.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep13_init;
    WeaponMoveFunc = PlRocketMove;
    OSReport("Wep13 ROCKET-RUNCHER prolog Ok\n");
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
