// wep08 module: the Striker shotgun (cObjStriker, object id 0x2F; routines wep/pl_shotgun.cpp).
//
// cObjStriker is the cObjWep (game/objWep.cpp) of the Striker (weapon_no 8, a semi-auto drum
// shotgun: 19 pellets in pl_shotgun, no pump), hanging on the player's right hand (parts 10) and
// driven by r_no_0 / r_no_1 from the shotgun routines: mode 2 -> moveFire (recoil motion, the
// shell ejected at frame 21), mode 4 -> moveReload (one motion by tune level, ItemMgr.reload at
// frame 35); both modes are ended by the player routine. Wep08_init is the WeaponInitFunc,
// PlShotgunMove the WeaponMoveFunc; the module object carries the class and the entry points.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlShotgunMove(cPlayer* pl);   // wep/pl_shotgun.cpp

class cObjStriker : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

void ObjStriker_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjStriker as Wep->m_pWep,
// inits it on the player, installs its motions, loads the muzzle-flash effects (archive 0x4 as
// group 0x3C) and points the debug preview PlWepMot at the aim idles 0x1A/0x20/0x22.
void Wep08_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_STRIKER);

    if (obj == 0) {
        pLog->err(0, 0, "Wep08_init() cObjWep CREATE FAILED");
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP08, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x1A);
    PlWepMot[1] = WEP_ARC_PTR(0x20);
    PlWepMot[2] = WEP_ARC_PTR(0x22);
}

// ObjInitFunc[0x2F]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjStriker_init(cObj* obj)
{
    new (obj) cObjStriker();
}

// cObjWep::init override (parent = the player): model 0x5 / texture 0x6, atari bits 8/9 off,
// hung on the right hand, light area, weapon list id 0x2D, idle motion 0x31, shotFrame[0..2] = 0x2E,
// default lock spread.
void cObjStriker::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x5), WEP_ARC_PTR(0x6)) == 0) {
        pLog->err(0, 0, "cObjStriker::init() failed.");
        return;
    }
    AtariFlagsAnd(&atari, 0xFCFF);
    pList->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    itemId = 0x2D;
    motReset[0] = WEP_ARC_PTR(0x31);
    resetMotion();
    shotFrame[0] = 0x2E;
    shotFrame[1] = 0x2E;
    shotFrame[2] = 0x2E;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire, set by the shotgun fire00): step 0 starts the recoil motion 0x30, the shot
// SEs, pad vibration, Status_flg[0] bit23 (shot noise) and the muzzle flash 0x3C; step 1 ejects
// the shell at frame 21. The player routine's next state resets the mode.
void cObjStriker::moveFire()
{
    if (r_no_1 == 0) {
        MotionSetCore(this, &this->Motion, WEP_ARC_PTR(0x30), 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 4, &pos, 0, 0, 0);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        StaFlagOn(pG, STA_PL_FIRE);
        EstSet(this, -1, 0, 0, EFF_WEP08, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        r_no_1 = 1;
    } else if (MotionCheckCrossFrame(&Motion, 21.0f)) {
        setCartridge();
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the tune level (0x2B/0x2D/0x2F) with
// the level's SE (2/0x20/0x21); at frame 35 ItemMgr.reload refills the drum.
void cObjStriker::moveReload()
{
    if (r_no_1 == 0) {
        void* m;
        u16 se;

        switch (pG->weapon_lv_reload) {
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
        motionSet(m, 0, 0, 1, 0);
        switch (pG->weapon_lv_reload) {
        default:
            se = 2;
            break;
        case 1:
            se = 0x20;
            break;
        case 2:
            se = 0x21;
            break;
        }
        m_StopSeId = SndCall(2, se, &pList->world, 0, 0, 0);
        r_no_1 = 1;
    } else if (MotionCheckCrossFrame(&Motion, 35.0f)) {
        ItemMgr.reload();
    }
}

// Ejects a spent shell: an obj10 model (archive 0x8/0x9) from the right hand's ejection port
// offset (-163.31, -6.87, 83.13) with a small random spread, gravity 10, 40 frames, effect 0x13.
void cObjStriker::setCartridge()
{
    cParts* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -163.31f;
    pos.y = -6.87f;
    pos.z = 83.13f;
    PSMTXMultVec(parts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 30.0f;
    spd.z = 20.0f;
    spd.x += fRand1_1() * 3.0f;
    spd.y += fRand1_1() * 5.0f;
    spd.z += fRand1_1() * 5.0f;
    PSMTXMultVecSR(parts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0x8), WEP_ARC_PTR(0x9), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with the Striker-carrying footwork motions (idle, walk (no
// [1]), run, turns, back, the 0x39..0x42 damage set, 0x57/0x5B knife transitions; 0x3D stays
// the player archive's), the weapon hand model (right hand 1) and the player archive's left hand 0x19.
void cObjStriker::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0A);
    NO_MOT(pl, 0x01);
    WEP_MOT(pl, 0x02, 0x10);
    WEP_MOT(pl, 0x03, 0x11);
    WEP_MOT(pl, 0x06, 0x14);
    WEP_MOT(pl, 0x07, 0x15);
    WEP_MOT(pl, 0x08, 0x12);
    WEP_MOT(pl, 0x09, 0x13);
    WEP_MOT(pl, 0x0B, 0x16);
    WEP_MOT(pl, 0x0C, 0x17);
    WEP_MOT(pl, 0x0D, 0x0C);
    WEP_MOT(pl, 0x0E, 0x0D);
    WEP_MOT(pl, 0x0F, 0x0E);
    WEP_MOT(pl, 0x10, 0x0F);
    WEP_MOT(pl, 0x5B, 0x33);
    WEP_MOT(pl, 0x57, 0x34);
    WEP_MOT(pl, 0x39, 0x26);
    WEP_MOT(pl, 0x3A, 0x27);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x28);
    WEP_MOT(pl, 0x42, 0x29);
    WEP_MOT(pl, 0x3F, 0x24);
    WEP_MOT(pl, 0x40, 0x25);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(1);
    pl->setLeftHand((u32) PL_ARC_PTR(pG->pPlayer, 0x19));
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep08_init;
    WeaponMoveFunc = PlShotgunMove;
    ObjInitFunc[0x2F] = ObjStriker_init;
    OSReport("Wep08 STRIKER prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x2F] = 0;
    OSReport("Wep08 STRIKER epilog Ok\n");
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
