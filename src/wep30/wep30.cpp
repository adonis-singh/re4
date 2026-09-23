// wep30 module: Ada's hand grenade (the wep19 cObjHandGre build for one grenade kind; object id
// 0x3C). The module object carries the class, the entry points and the grenade routine registration
// (wep/pl_grenade.cpp).
//
// Ada's build of wep19: the display-only cObjHandGre (always the hand grenade model, player
// archive 0x6A/0x6B) hung at Ada's hand (parts 0x11) and belt (parts 10) offsets; Wep30_init is
// the WeaponInitFunc, PlGrenadeMove the WeaponMoveFunc (itemThrow creates the thrown cSubWep).

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

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the two grenade objects, installs
// the grenade footwork motions and loads the throw effects (archive 0x4 as group 0x4D).
void Wep30_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep30_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP19, 1);
    }
}

// The two grenade objects: the hand one (parts 0x11) is pObj, the belt one (parts 0xA) pObj2.
// The hand one is hidden with <= 1 item, the belt one with none (the egg half-scale branch is
// kept although this module only shows the grenade model). Returns the hand object, NULL on failure.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj;
    Vec pos;
    Vec rot;

    pl->Wep->m_pWep = 0;
    pl->Wep->m_pWepHand = 0;
    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_HANDGRE);
    if (obj == 0) {
        goto fail;
    }
    obj->init(pl);
    pl->Wep->m_pWep = obj;
    pos.x = -170.0f;
    pos.y = -60.0f;
    pos.z = -40.0f;
    rot.x = -0.13439035f;
    rot.y = 2.4958208f;
    rot.z = -0.2268928f;
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
        pLog->err(0, 0, "Wep30_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    pos.x = -70.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
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

// cObjWep::init override: the hand grenade model from the player archive (0x6A/0x6B); no atari /
// parent (parentSet does that).
void cObjHandGre::init(cModel* parent)
{
    if (modelInit(PL_ARC_PTR(pG->pPlayer, 0x6A), PL_ARC_PTR(pG->pPlayer, 0x6B)) == 0) {
        pLog->err(0, 0, "cObjHandGre::init() failed.");
    }
}

// Fills the player's motion table with the grenade footwork motions (idle, run, turns, back, the
// 0x39..0x42 damage set; 0x3D stays the player archive's), shows / hides the hand and belt
// grenades by the item count, and sets the right hand model (the grenade hand 0x7 while any is
// left, else the bare hand 0x11).
void cObjHandGre::setMotion(cPlayer* pl)
{
    u16 num;
    void* hand;

    WEP_MOT(pl, 0x00, 0x08);
    WEP_MOT(pl, 0x02, 0x09);
    WEP_MOT(pl, 0x03, 0x1A);
    WEP_MOT(pl, 0x06, 0x0B);
    WEP_MOT(pl, 0x07, 0x1C);
    WEP_MOT(pl, 0x08, 0x0A);
    WEP_MOT(pl, 0x09, 0x1B);
    WEP_MOT(pl, 0x0B, 0x0C);
    WEP_MOT(pl, 0x0C, 0x1D);
    WEP_MOT(pl, 0x0D, 0x0D);
    WEP_MOT(pl, 0x0E, 0x1E);
    WEP_MOT(pl, 0x0F, 0x0E);
    WEP_MOT(pl, 0x10, 0x1F);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x3F, 0x20);
    WEP_MOT(pl, 0x40, 0x21);
    WEP_MOT(pl, 0x39, 0x22);
    WEP_MOT(pl, 0x3A, 0x23);
    WEP_MOT(pl, 0x41, 0x24);
    WEP_MOT(pl, 0x42, 0x25);
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
        hand = PL_ARC_PTR(pG->pPlayer, 0x11);
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
    WeaponInitFunc = Wep30_init;
    WeaponMoveFunc = PlGrenadeMove;
    ObjInitFunc[0x3C] = ObjHandGre_init;
    OSReport("Wep30 ADA|GRENADE prolog Ok\n");
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
