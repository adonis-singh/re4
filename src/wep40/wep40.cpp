// wep40 module: Ada's semi-auto rifle (own copy of the cObjHkSniper class, object id 0x30; routines
// wep/pl_rifle.cpp).
//
// Ada's cObjHkSniper: model 0xA or 0xB by weapon_type, offset in her right hand (parts 10), driven
// by mode / step from the rifle routines (mode 2 fire: SEs and vibration only, mode 4
// reload: one motion, ItemMgr.reload at frame 34; both ended by the player routine). Wep40_init
// is the WeaponInitFunc, PlRifleMove the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"

void PlRifleMove(cPlayer* pl);   // wep/pl_rifle.cpp
cObjWep* equipWeapon(cPlayer* pl);
void ObjHkSniper_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the rifle object as Wep->m_pWep,
// installs its motions, loads the effects (archive 0x8 as group 0x44) and points the debug preview
// PlWepMot at motion 0xE / the player archive's 0x5E. (The error string still says Wep02.)
void Wep40_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep02_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x8), EFF_WEP10, 1);
        PlWepMot[0] = WEP_ARC_PTR(0xE);
        PlWepMot[1] = PL_ARC_PTR(pG->pPlayer, 0x5E);
        PlWepMot[2] = WEP_ARC_PTR(0xE);
    }
}

// Creates the cObjHkSniper (ObjMgr id 0x30) and inits it on the player; NULL (stored as the
// player's weapon too) when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_HKSNIPER);

    if (obj == 0) {
        pLog->err(0, 0, "Wep09_init() cObjWep CREATE FAILED");
        pl->Wep->m_pWep = obj;
        return 0;
    }
    obj->init(pl);
    return obj;
}

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 hksniper_tbl[3];
const u8 hksniper_tbl[3] = { 0x14, 0xA, 0 };

// cObjWep::init override (parent = the player): weapon list id 0x2F, model 0xA (type 0) or 0xB
// with texture 0x9 (the object is destroyed when it fails), atari bits 8/9 off, hung on the right
// hand at (0.5, -5, -3), light area, the hksniper_tbl bytes, idle motion 0x22, default lock spread.
void cObjHkSniper::init(cModel* parent)
{
    void* bin;

    itemId = 0x2F;
    if (pG->weapon_type == 0) {
        bin = WEP_ARC_PTR(0xA);
    } else {
        bin = WEP_ARC_PTR(0xB);
    }
    if (modelInit(bin, WEP_ARC_PTR(0x9)) == 0) {
        pLog->err(0, 0, "cObjSniper::init() failed.");
        ObjMgr.destroy(this);
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    pParts->pos.x = 0.5f;
    pParts->pos.y = -5.0f;
    pParts->pos.z = -3.0f;
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    shotFrame[0] = hksniper_tbl[0];
    shotFrame[1] = hksniper_tbl[1];
    shotFrame[2] = hksniper_tbl[2];
    motReset[0] = WEP_ARC_PTR(0x22);
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire, set by the rifle fire00): step 0 drops the gun's motion, plays the shot
// SEs, sets Status_flg[0] bit23 (shot noise) and vibrates the pad; step 1 waits.
void cObjHkSniper::moveFire()
{
    if (step == 0) {
        Motion.pMot = 0;
        SndCall(2, 0, &pParts->world, 0, 0, 0);
        SndCall(2, 4, &pParts->world, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        step = 1;
    }
}

// mode == 4 (reload): step 0 starts the gun's reload motion 0x25 with the magazine SE; at
// frame 34 ItemMgr.reload refills the magazine.
void cObjHkSniper::moveReload()
{
    if (step == 0) {
        MotionSetCore(this, &this->Motion, WEP_ARC_PTR(0x25), 0, 0, 0, 0);
        m_StopSeId = SndCall(2, 2, &pParts->world, 0, 0, 0);
        step = 1;
    }
    if (MotionCheckCrossFrame(&Motion, 34.0f)) {
        ItemMgr.reload();
    }
}

// Fills the player's motion table with the rifle-carrying footwork motions (idle, no walk [1],
// run, turns, back, the 0x39..0x42 damage set; 0x3D stays the player archive's) and sets the
// weapon hand models (right hand 1, left hand 2).
void cObjHkSniper::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0E);
    NO_MOT(pl, 0x01);
    WEP_MOT(pl, 0x02, 0x11);
    WEP_MOT(pl, 0x03, 0x29);
    WEP_MOT(pl, 0x06, 0x13);
    WEP_MOT(pl, 0x07, 0x2B);
    WEP_MOT(pl, 0x08, 0x12);
    WEP_MOT(pl, 0x09, 0x2A);
    WEP_MOT(pl, 0x0B, 0x18);
    WEP_MOT(pl, 0x0C, 0x2C);
    WEP_MOT(pl, 0x0D, 0x0F);
    WEP_MOT(pl, 0x0E, 0x27);
    WEP_MOT(pl, 0x0F, 0x10);
    WEP_MOT(pl, 0x10, 0x28);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x3F, 0x38);
    WEP_MOT(pl, 0x40, 0x39);
    WEP_MOT(pl, 0x39, 0x3A);
    WEP_MOT(pl, 0x3A, 0x3B);
    WEP_MOT(pl, 0x41, 0x3C);
    WEP_MOT(pl, 0x42, 0x3D);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xD));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}

// ObjInitFunc[0x30]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjHkSniper_init(cObj* obj)
{
    new (obj) cObjHkSniper();
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep40_init;
    WeaponMoveFunc = PlRifleMove;
    ObjInitFunc[0x30] = ObjHkSniper_init;
    OSReport("Wep40 ADA SEMIAUTO-RIFLE prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x30] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
