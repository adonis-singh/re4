// wep19 module: the hand grenade (cObjHandGre, object id 0x3C; also the incendiary / flash grenades
// and the eggs by weapon number). Two grenade objects hang on the player (the one in the hand and the
// one on the belt); the module object carries the class, the entry points and the grenade routine
// registration (wep/pl_grenade.cpp).
//
// cObjHandGre is a display-only cObjWep: no fire / reload modes (the throw creates a cSubWep in
// pl_grenade's itemThrow), just the model of the item kind (grenade / incendiary / flash / the
// three eggs), hung on the player's parts by parentSet. The hand one is Wep->m_pWep (parts 0x11,
// hidden when only one item is left), the belt one Wep->pObj2 (parts 10). Wep19_init is the
// WeaponInitFunc, PlGrenadeMove the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "esp.h"
#include "pad.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

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
void Wep19_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep19_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x4D, 1);
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
    pl->Wep->pObj2 = 0;
    obj = (cObjWep*) ObjMgr.createBack(0x3C);
    if (obj == 0) {
        goto fail;
    }
    obj->init(pl);
    pl->Wep->m_pWep = obj;
    pos.x = -145.0f;
    pos.y = -75.0f;
    pos.z = -120.0f;
    rot.x = 2.7925267f;
    rot.y = 1.134464f;
    rot.z = 2.6179938f;
    obj->parentSet(pl, 0x11, &pos, &rot);
    if (pG->weapon_no == 0x19 || pG->weapon_no == 0x1F || pG->weapon_no == 0x20) {
        obj->pParts->scale.x = 0.5f;
        obj->pParts->scale.y = 0.5f;
        obj->pParts->scale.z = 0.5f;
    }
    if (ItemMgr.bulletNum() <= 1) {
        obj->setDisp(0, 0);
    }
    obj = (cObjWep*) ObjMgr.createBack(0x3C);
    if (obj == 0) {
    fail:
        pLog->err(0, 0, "Wep19_init() cObjWep CREATE FAILED");
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
        obj->pParts->scale.x = 0.5f;
        obj->pParts->scale.y = 0.5f;
        obj->pParts->scale.z = 0.5f;
    }
    pl->Wep->pObj2 = obj;
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
// wep.x24; no atari / parent (parentSet does that).
void cObjHandGre::init(cModel* parent)
{
    void* bin;
    void* tpl;

    switch (pG->weapon_no) {
    case 0x13:
    default:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6B);
        wep.x24 = 1;
        break;
    case 0x16:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6D);
        wep.x24 = 2;
        break;
    case 0x17:
        bin = PL_ARC_PTR(pG->pPlayer, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x6F);
        wep.x24 = 0xE;
        break;
    case 0x19:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x7E);
        wep.x24 = 8;
        break;
    case 0x1F:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x7F);
        wep.x24 = 9;
        break;
    case 0x20:
        bin = PL_ARC_PTR(pG->pPlayer, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlayer, 0x80);
        wep.x24 = 0xA;
        break;
    }
    if (modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "cObjHandGre::init() failed.");
    }
}

// Fills the player's motion table with the grenade footwork motions (idle, no walk [1], run,
// turns, back, the 0x39..0x42 damage set, 0x57/0x5B knife transitions; 0x3D stays the player
// archive's), shows / hides the hand and belt grenades by the item count, and sets the right
// hand model (the grenade hand 0x7 while any is left, else the bare hand).
void cObjHandGre::setMotion(cPlayer* pl)
{
    u16 num;
    void* hand;

    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x08));
    PSet(pl->m_MotTbl[0x01], (void*) 0);
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x09));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x1A));
    PSet(pl->m_MotTbl[0x04], WEP_ARC_PTR(0x09));
    PSet(pl->m_MotTbl[0x05], WEP_ARC_PTR(0x09));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x1C));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x0A));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x1B));
    PSet(pl->m_MotTbl[0x0A], WEP_ARC_PTR(0x0A));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x1D));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x1E));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x1F));
    PSet(pl->m_MotTbl[0x12], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x13], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x22));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x23));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x24));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x25));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x20));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x21));
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x26));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x27));
    num = ItemMgr.bulletNum();
    if (num > 1) {
        setDisp(0, 1);
    } else {
        setDisp(0, 0);
    }
    if (num) {
        pl->Wep->pObj2->setDisp(0, 1);
    } else {
        pl->Wep->pObj2->setDisp(0, 0);
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

// Aim key check used by the player (cObjWep::keyKamae override): the aim button counts only while
// an item is left to throw.
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
    WeaponInitFunc = Wep19_init;
    WeaponMoveFunc = PlGrenadeMove;
    ObjInitFunc[0x3C] = ObjHandGre_init;
    OSReport("Wep19 HAND GRENADE prolog Ok\n");
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
