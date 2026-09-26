// wep41 module: HUNK's hand grenade (the wep19 cObjHandGre build without the weapon-specific motion
// slots; object id 0x3C). The module object carries the class, the entry points and the grenade
// routine registration (wep/pl_grenade.cpp).
//
// HUNK's build of wep19: the display-only cObjHandGre (model by the item kind) at HUNK's hand
// (parts 0x11) and belt (parts 10) offsets, no damage motion set; Wep41_init is the WeaponInitFunc,
// PlGrenadeMove the WeaponMoveFunc (itemThrow creates the thrown cSubWep).

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "esp.h"
#include "pad.h"

void PlGrenadeMove(cPlayer* pl);   // wep/pl_grenade.cpp
cObjWep* equipWeapon(cPlayer* pl);

class cObjHandGre : public cObjWep {
public:
    virtual ~cObjHandGre() {}
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
    virtual int keyKamae();
};

// item kind of the equipped throwable (equipWeapon)
static u16 greType;

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the two grenade objects, installs
// the grenade footwork motions, loads the throw effects (archive 0x4 as group 0x4D) and points
// the debug preview PlWepMot at the aim idles 0x11/0x14/0x17.
void Wep41_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep41_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP19, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x11);
        PlWepMot[1] = WEP_ARC_PTR(0x14);
        PlWepMot[2] = WEP_ARC_PTR(0x17);
    }
}

// The two grenade objects: the hand one (parts 0x11) is pObj, the belt one (parts 0xA) pObj2.
// greType = the item kind of weapon_no (1 grenade, 2 incendiary, 0xE flash, 8/9/0xA eggs); the
// eggs are drawn at half scale; the hand one is hidden with <= 1 item, the belt one with none.
// Returns the hand object, NULL when a work could not be created.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj;
    Vec pos;
    Vec rot;

    switch (pG->weapon_no) {
    case 0x13:
    default:
        greType = 1;
        break;
    case 0x16:
        greType = 2;
        break;
    case 0x17:
        greType = 0xE;
        break;
    case 0x19:
        greType = 8;
        break;
    case 0x1F:
        greType = 9;
        break;
    case 0x20:
        greType = 0xA;
        break;
    }
    pl->Wep->m_pWep = 0;
    pl->Wep->m_pWepHand = 0;
    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_HANDGRE);
    if (obj == 0) {
        goto fail;
    }
    obj->init(pl);
    pl->Wep->m_pWep = obj;
    pos.x = -120.0f;
    pos.y = -80.0f;
    pos.z = -130.0f;
    rot.x = 0.5235988f;
    rot.y = 1.2217305f;
    rot.z = 0.34906584f;
    obj->parentSet(pl, 0x11, &pos, &rot);
    if (pG->weapon_no == 0x19 || pG->weapon_no == 0x1F || pG->weapon_no == 0x20) {
        obj->pList->scale.x = 0.5f;
        obj->pList->scale.y = 0.5f;
        obj->pList->scale.z = 0.5f;
    }
    if (ItemMgr.bulletNum() <= 1) {
        obj->setDisp(0, 0);
    }
    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_HANDGRE);
    if (obj == 0) {
    fail:
        pLog->err(0, 0, "Wep41_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    pos.x = -95.0f;
    pos.y = -30.0f;
    pos.z = 5.0f;
    rot.x = PI / 2.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj->parentSet(pl, 0xA, &pos, &rot);
    if (pG->weapon_no == 0x19 || pG->weapon_no == 0x1F || pG->weapon_no == 0x20) {
        obj->pList->scale.x = 0.5f;
        obj->pList->scale.y = 0.5f;
        obj->pList->scale.z = 0.5f;
    }
    pl->Wep->m_pWepHand = obj;
    if (ItemMgr.bulletNum() == 0) {
        obj->setDisp(0, 0);
    }
    return pl->Wep->m_pWep;
}

// ObjInitFunc[0x3C]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjHandGre_init(cObj* obj)
{
    new (obj) cObjHandGre();
}

// cObjWep::init override: the model of weapon_no from the player archive (grenade body 0x6A with
// the texture of the kind 0x6B/0x6D/0x6F, egg 0x7D with 0x7E/0x7F/0x80) and the item kind in
// itemId; no atari / parent (parentSet does that).
void cObjHandGre::init(cModel* parent)
{
    void* bin;
    void* tpl;

    switch (pG->weapon_no) {
    case 0x13:
    default:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6B);
        itemId = 1;
        break;
    case 0x16:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6D);
        itemId = 2;
        break;
    case 0x17:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6F);
        itemId = 0xE;
        break;
    case 0x19:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x7E);
        itemId = 8;
        break;
    case 0x1F:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x7F);
        itemId = 9;
        break;
    case 0x20:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x80);
        itemId = 0xA;
        break;
    }
    if (modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "cObjHandGre::init() failed.");
    }
}

// Fills the player's motion table with the grenade footwork motions (idle, no walk [1], run,
// turns, back; no damage set; 0x3D stays the player archive's), shows / hides the hand and belt
// grenades by the item count, and sets the right hand model (the grenade hand 0x7 while any is
// left, else the bare hand 0x12).
void cObjHandGre::setMotion(cPlayer* pl)
{
    u16 num;
    void* hand;

    WEP_MOT(pl, 0x00, 0x08);
    NO_MOT(pl, 0x01);
    WEP_MOT(pl, 0x02, 0x09);
    WEP_MOT(pl, 0x03, 0x1A);
    WEP_MOT(pl, 0x04, 0x09);
    WEP_MOT(pl, 0x05, 0x09);
    WEP_MOT(pl, 0x06, 0x0B);
    WEP_MOT(pl, 0x07, 0x1C);
    WEP_MOT(pl, 0x08, 0x0A);
    WEP_MOT(pl, 0x09, 0x1B);
    WEP_MOT(pl, 0x0A, 0x0A);
    WEP_MOT(pl, 0x0B, 0x0C);
    WEP_MOT(pl, 0x0C, 0x1D);
    WEP_MOT(pl, 0x0D, 0x0D);
    WEP_MOT(pl, 0x0E, 0x1E);
    WEP_MOT(pl, 0x0F, 0x0E);
    WEP_MOT(pl, 0x10, 0x1F);
    WEP_MOT(pl, 0x12, 0x0B);
    WEP_MOT(pl, 0x13, 0x0B);
    PLA_MOT(pl, 0x3D, 0x5D);
    num = ItemMgr.bulletNum();
    if (num > 1) {
        setDisp(0, 1);
    } else {
        setDisp(0, 0);
    }
    if (num) {
        pl->Wep->m_pWepHand->setDisp(0, 1);
    } else {
        pl->Wep->m_pWepHand->setDisp(0, 0);
    }
    if (bulletNum()) {
        hand = WEP_ARC_PTR(0x7);
    } else {
        hand = PL_ARC_PTR(pG->pPlayer, 0x12);
    }
    pl->Body->initWepHand((u32) hand);
    pl->setRightHand(1);
    pl->setLeftHand(0);
}

// Aim key check (cObjWep::keyKamae override): the aim button counts only while an item is left.
int cObjHandGre::keyKamae()
{
    if ((Key.on & 0x10) && ItemMgr.bulletNum()) {
        return 1;
    }
    return 0;
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep41_init;
    WeaponMoveFunc = PlGrenadeMove;
    ObjInitFunc[0x3C] = ObjHandGre_init;
    OSReport("Wep41 HUNK GRENADE prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x3C] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
