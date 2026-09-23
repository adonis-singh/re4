// wep28 module: Krauser's bow (cObjBow) and its arrow (cObjAllow); routines in wep/pl_bow.cpp.
//
// cObjBow (wep_mod.h) is the cObjWep hanging on the player's left forearm (parts 0x10); its parts
// 4 is the nocked arrow, shown / hidden by scale (setDispAllow). cObjAllow is the arrow model held
// in the right hand (parts 10) between shots: Wep->pObj2 and pAllow. Driven by mode /
// step from wep/pl_bow.cpp: mode 1 -> moveReady (draw motion on both), 2 -> moveFire (the
// arrow is launched as a cEmMine of type 2 by setAllow), 3 -> moveDown. Wep28_init is the
// WeaponInitFunc, PlBowMove the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "item.h"
#include "motion.h"
#include "snd.h"
#include "emmine.h"

void PlBowMove(cPlayer* pl);   // wep/pl_bow.cpp

void ObjKlauBow_init(cObj* obj);
void ObjKlauAllow_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the bow (ObjMgr id 0x11) as
// Wep->m_pWep with its motions, then the hand arrow (id 0x10) as Wep->pObj2 (display type 1 off)
// and links it as pAllow; loads the effects (archive 0x4 as group 0x50).
void Wep28_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_BOW);

    if (obj == 0) {
        pLog->err(0, 0, "Wep28_init() cObjWep CREATE FAILED");
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_ALLOW);
    if (obj == 0) {
        pLog->err(0, 0, "Wep28_init() cObjWep CREATE FAILED");
        return;
    }
    pl->Wep->m_pWepHand = obj;
    obj->init(pl);
    obj->setDisp(1, 0);
    ((cObjBow*) pl->Wep->m_pWep)->pAllow = obj;
    EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP28, 1);
}

// mode == 1 (ready, set by the bow ready00): the bow's draw motion 0x2E and the hand arrow's
// (player archive 0x73), the string SE 2/0 at frame 10; back to mode 0 at the motion's end.
void cObjBow::moveReady()
{
    if (step == 0) {
        MotionSetCore(this, &this->Motion, WEP_ARC_PTR(0x2E), 0, 0, 0, 0);
        if (pAllow) {
            pAllow->motionSet(PL_ARC_PTR(pG->pPlayer, 0x73), 0, 0, 1, 0);
        }
        step = 1;
    }
    if (MotionCheckCrossFrame(&Motion, 10.0f)) {
        SndCall(2, 0, &pParts->world, 0, 0, 0);
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// mode == 2 (fire, set by the bow fire00): the release motion (0x2F, 0x2C on the last arrow),
// the nocked arrow hidden and launched as a projectile (setAllow), release SE 2/1; back to mode 0
// at the motion's end.
void cObjBow::moveFire()
{
    if (step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x2F);
        } else {
            m = WEP_ARC_PTR(0x2C);
        }
        MotionSetCore(this, &this->Motion, m, 0, 0, 0, 0);
        setDispAllow(0);
        setAllow();
        SndCall(2, 1, &pParts->world, 0, 0, 0);
        step = 1;
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// mode == 3 (down, set by wepDown): hides the nocked arrow and returns the bow to its idle
// motion; mode 0 when that motion state reports done.
void cObjBow::moveDown()
{
    if (step == 0) {
        setDispAllow(0);
        resetMotion();
        step = 1;
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// Shared by both init()s: one error string and one pair of light-set constants in .rodata (a
// static inline's strings / statics are emitted when it is parsed, i.e. here, before init's pool).
static inline void wepInitErr()
{
    pLog->err(0, 0, "cObjWep::init() failed.");
}

static inline void wepLightInit(cObjWep* o)
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 500.0f, 0.0f, 0.0f };

    o->LightInfo.init2(1, 1, &p0, &p1, 1);
}

// cObjWep::init override (Wep28_init, parent = the player): model 0x5 / texture 0x6, a 100-unit
// box atari with bits 8/9 off, hung on the player's left forearm (parts 0x10), light area, idle
// motion 0x2D (also as the "empty" idle), the nocked arrow hidden, no hand arrow yet, default lock spread.
void cObjBow::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x5), WEP_ARC_PTR(0x6)) == 0) {
        wepInitErr();
        return;
    }
    sub2B4.atari.init(0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 1, 0, 0);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0x10);
    wepLightInit(this);
    m_pParent = parent;
    motReset[0] = WEP_ARC_PTR(0x2D);
    motReset[1] = WEP_ARC_PTR(0x2D);
    resetMotion();
    setDispAllow(0);
    pAllow = 0;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// Shows (on == 1: scale 1) or hides (scale 0) the arrow nocked on the bow (parts 4).
void cObjBow::setDispAllow(int on)
{
    cModel* parts = getPartsPtr(4);
    f32 s;

    if (on == 1) {
        s = 1.0f;
    } else {
        s = 0.0f;
    }
    parts->scale.x = s;
    parts->scale.y = s;
    parts->scale.z = s;
}

// Fills the player's motion table with the bow-carrying footwork motions (idle, run, turns, back,
// the 0x39..0x42 damage set; 0x3D stays the player archive's); the bow hand model is the left
// hand (7), the right hand stays bare until an arrow is taken.
void cObjBow::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0A);
    WEP_MOT(pl, 0x02, 0x0B);
    WEP_MOT(pl, 0x03, 0x11);
    WEP_MOT(pl, 0x06, 0x0D);
    WEP_MOT(pl, 0x07, 0x13);
    WEP_MOT(pl, 0x08, 0x0C);
    WEP_MOT(pl, 0x09, 0x12);
    WEP_MOT(pl, 0x0B, 0x0E);
    WEP_MOT(pl, 0x0C, 0x14);
    WEP_MOT(pl, 0x0D, 0x0F);
    WEP_MOT(pl, 0x0E, 0x15);
    WEP_MOT(pl, 0x0F, 0x10);
    WEP_MOT(pl, 0x10, 0x16);
    WEP_MOT(pl, 0x39, 0x19);
    WEP_MOT(pl, 0x3A, 0x1A);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x1B);
    WEP_MOT(pl, 0x42, 0x1C);
    WEP_MOT(pl, 0x3F, 0x17);
    WEP_MOT(pl, 0x40, 0x18);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(0);
    pl->setLeftHand(7);
}

// Shoots the arrow: a cEmMine of type 2 (the arrow model, player archive 0x70/0x71) from the
// right hand (parts 10) at 1500 units/frame along the hand's -X; a hand inside an effect
// collision is first pushed 2 m out along the hit normal.
void cObjBow::setAllow()
{
    static const Vec dir = { -1500.0f, 0.0f, 0.0f };
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec spd;
    Vec hit;
    Vec nrm;

    PSMTXMultVecSR(parts->mat, &dir, &spd);
    if (EatMgr.hitCheck(&pPL->getPartsPtr(0)->world, &parts->world, &hit, &nrm, 0, 0)) {
        PSVECScale(&nrm, &nrm, 2000.0f);
        PSVECAdd(&hit, &nrm, &parts->world);
        setPos(&parts->world);
    }
    SetMine(PL_ARC_PTR(pG->pPlayer, 0x70), PL_ARC_PTR(pG->pPlayer, 0x71), &parts->world, &spd, 2);
}

// Aim key check (cObjWep::keyKamae override): the aim button counts only while arrows are left,
// unless Status_flg[3] bit23 (an event / debug override) lets it through regardless.
int cObjBow::keyKamae()
{
    if (StaFlagChk(pG, STA_KLAUSER_TRANSFORM)) {
        return (Key.on >> 4) & 1;
    }
    if ((Key.on & 0x10) && ItemMgr.bulletNum()) {
        return 1;
    }
    return 0;
}

// Weapon interrupt (damage / event cuts the routine): the base reset, both arrows hidden, the
// bow back to its idle, mode 0.
void cObjBow::interrupt()
{
    cObjWep::interrupt();
    setDispAllow(0);
    if (pAllow) {
        pAllow->setDisp(1, 0);
    }
    resetMotion();
    mode = 0;
    step = 0;
}

// ObjInitFunc[0x11]: placement-constructs the bow in the work cObjMgr::construct hands over.
void ObjKlauBow_init(cObj* obj)
{
    new (obj) cObjBow();
}

// The hand arrow never fires by itself (the bow's setAllow does).
void cObjAllow::moveFire()
{
}

// cObjWep::init override (Wep28_init, parent = the player): the arrow model (player archive
// 0x70/0x71), a 100-unit box atari with bits 8/9 off, light area, hung on the player's right hand
// (parts 10) at (-850, 5, 24) turned -90 degrees about Y.
void cObjAllow::init(cModel* parent)
{
    if (modelInit(PL_ARC_PTR(pG->pPlayer, 0x70), PL_ARC_PTR(pG->pPlayer, 0x71)) == 0) {
        wepInitErr();
        return;
    }
    sub2B4.atari.init(0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 1, 0, 0);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    wepLightInit(this);
    pos.x = -850.0f;
    pos.y = 5.0f;
    pos.z = 24.0f;
    ang.x = 0.0f;
    ang.y = -(PI / 2.0f);
    ang.z = 0.0f;
    parentSet(parent, 0xA, &pos, &ang);
}

// The hand arrow installs no player motions (the bow's setMotion does).
void cObjAllow::setMotion(cPlayer* pl)
{
}

// ObjInitFunc[0x10]: placement-constructs the arrow in the work cObjMgr::construct hands over.
void ObjKlauAllow_init(cObj* obj)
{
    new (obj) cObjAllow();
}

// REL entry: registers the weapon init / move routines and both object constructor slots.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep28_init;
    WeaponMoveFunc = PlBowMove;
    ObjInitFunc[0x11] = ObjKlauBow_init;
    ObjInitFunc[0x10] = ObjKlauAllow_init;
    OSReport("Wep28 KLAUSER-BOW prolog Ok\n");
}

// REL exit: frees the object constructor slots.
extern "C" void _epilog()
{
    ObjInitFunc[0x11] = 0;
    ObjInitFunc[0x10] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
