// wep15 module: the magnum (cObjMagnum, object id 0x2C; routines wep/pl_handgun.cpp).
//
// cObjMagnum is the cObjWep (game/objWep.cpp) of the Broken Butterfly revolver, hanging on the
// player's right hand (parts 10) and driven by wep.mode / wep.step from the handgun routines:
// mode 2 -> moveFire (recoil motion, SEs, flash 0x49, strong vibration; no cartridge), mode 4 ->
// moveReload (motion by tune level, ItemMgr.reload at frame 34). Both modes are ended by the
// player routine. Wep15_init is the WeaponInitFunc, PlHandgunMove the WeaponMoveFunc; the module
// object carries the class, the entry points and the ObjInitFunc slot.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp

class cObjMagnum : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
};

void ObjMagnum_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjMagnum as Wep->m_pWep,
// inits it on the player, installs its motions, loads the muzzle-flash effects (archive 0x4 as
// group 0x49) and points the debug preview PlWepMot at the aim motions 0x29..0x2B.
void Wep15_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x2C);

    if (obj == 0) {
        pLog->err(0, 0, "Wep15_init() cObjMagnum CREATE FAILED");
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x49, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x29);
    PlWepMot[1] = WEP_ARC_PTR(0x2A);
    PlWepMot[2] = WEP_ARC_PTR(0x2B);
}

// ObjInitFunc[0x2C]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjMagnum_init(cObj* obj)
{
    new (obj) cObjMagnum();
}

// cObjWep::init override (parent = the player): model 0x6 / texture 0x5, atari bits 8/9 off,
// hung on the right hand, light area, idle motion 0x34, wep.x18..x1A = 0x20, default lock spread.
void cObjMagnum::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMagnum::init() failed.");
        return;
    }
    sub2B4.atari.m_flag &= 0xFCFF;
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    PSet(wep.parent, parent);
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x34));
    resetMotion();
    wep.x18 = 0x20;
    wep.x19 = 0x20;
    wep.x1A = 0x20;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// wep.mode == 2 (fire): step 0 starts the recoil motion 0x32, plays the three shot SEs, the
// muzzle flash 0x49 and the strong pad vibration (pattern 7); step 1 waits for the player routine.
void cObjMagnum::moveFire()
{
    if (wep.step == 0) {
        MotionSetCore(this, &this->Motion, WEP_ARC_PTR(0x32), 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        SndCall(2, 4, &pos, 0, 0, 0);
        EstSet((int) this, -1, 0, 0, 0x49, 0, 0, 0xA, 0, 0);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        wep.step = 1;
    }
}

// wep.mode == 4 (reload): step 0 starts the cylinder reload motion of the tune level (0x33/0x36/
// 0x35) with the level's SE (0x16/0x20/0x18); at frame 34 ItemMgr.reload refills the cylinder.
void cObjMagnum::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        switch (pG->weapon_lv_reload) {
        default:
            m = WEP_ARC_PTR(0x33);
            break;
        case 1:
            m = WEP_ARC_PTR(0x36);
            break;
        case 2:
            m = WEP_ARC_PTR(0x35);
            break;
        }
        motionSet(m, 0, 0, 1, 0);
        switch (pG->weapon_lv_reload) {
        default:
            se = 0x16;
            break;
        case 1:
            se = 0x20;
            break;
        case 2:
            se = 0x18;
            break;
        }
        wep.seHandle = SndCall(2, se, &pParts->world, 0, 0, 0);
        wep.step = 1;
    } else if (MotionCheckCrossFrame(&Motion, 34.0f)) {
        ItemMgr.reload();
    }
}

// Fills the player's motion table with the magnum-carrying footwork motions (idle, walk, run,
// turns, back, the 0x39..0x42 and 0x5D/0x5E damage set, 0x57/0x5B knife transitions; 0x3D stays
// the player archive's), the weapon hand model (right hand 1) and the archive's left hand (0x9).
void cObjMagnum::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x01], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x15));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x16));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x0A], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x17));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x18));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x19));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x1A));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x1B));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x1C));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x1D));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x1E));
    PSet(pl->m_MotTbl[0x5D], WEP_ARC_PTR(0x1F));
    PSet(pl->m_MotTbl[0x5E], WEP_ARC_PTR(0x20));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x31));
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x30));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x8));
    pl->setRightHand(1);
    pl->setLeftHand((u32) WEP_ARC_PTR(0x9));
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep15_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x2C] = ObjMagnum_init;
    OSReport("Wep15 MAGNUM prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x2C] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
