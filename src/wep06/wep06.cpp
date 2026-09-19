// wep06 module: the Government (Matilda) handgun (cObjGovernment, object id 0x31). The module object
// carries the class, the entry points and the handgun routine registration (wep/pl_handgun.cpp).
//
// cObjGovernment is the cObjWep (game/objWep.cpp) of the Matilda, hanging on the player's right
// hand (parts 10) and driven by wep.mode / wep.step from the handgun routines: mode 2 -> moveFire
// (slide motion, SEs, flash, cartridge; the mode is left by the player routine, not by the motion
// end), mode 4 -> moveReload. weapon_type 1 is the model with the stock (0x7, its own idle 0x39,
// weapon list id 0x2B). Wep06_init is the WeaponInitFunc, PlHandgunMove the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp

class cObjGovernment : public cObjWep {
public:
    virtual ~cObjGovernment() {}
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjGovernment (ObjMgr id
// 0x31) as Wep->m_pWep, inits it on the player, installs its motions, loads the muzzle-flash
// effects (archive 0x4 as group 0x3A) and points the debug preview PlWepMot at 0x26..0x28.
void Wep06_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(0x31);
    if (obj == 0) {
        pLog->err(0, 0, "Wep06_init() cObjWep CREATE FAILED");
    } else {
        pl->Wep->m_pWep = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x3A, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x26);
        PlWepMot[1] = WEP_ARC_PTR(0x27);
        PlWepMot[2] = WEP_ARC_PTR(0x28);
    }
}

// ObjInitFunc[0x31]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjGovernment_init(cObj* obj)
{
    new (obj) cObjGovernment();
}

// cObjWep::init override (parent = the player): model 0x6 with idle 0x34 (type 1: model 0x7 with
// idle 0x39; empty idle 0x3A; weapon list id 0x2A / 0x2B), atari bits 8/9 off, hung on the right
// hand, light area, wep.x18..x1A = 0x14, default lock spread.
void cObjGovernment::init(cModel* parent)
{
    void* bin;

    if (pG->weapon_type != 1) {
        bin = WEP_ARC_PTR(0x6);
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x34));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x3A));
        wep.x24 = 0x2A;
    } else {
        bin = WEP_ARC_PTR(0x7);
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x39));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x3A));
        wep.x24 = 0x2B;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    sub2B4.atari.m_flag &= 0xFCFF;
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    resetMotion();
    wep.x1A = wep.x19 = wep.x18 = 0x14;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// wep.mode == 2 (fire): step 0 starts the slide motion (0x32, 0x35 on the last round), plays the
// shot SEs, sets Status_flg[0] bit23 (shot noise), muzzle flash 0x3A, a cartridge and the pad
// vibration. Step 1 does nothing: the handgun routine's next state resets the mode.
void cObjGovernment::moveFire()
{
    if (wep.step == 0) {
        void* m;
        int zero;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x32);
        } else {
            m = WEP_ARC_PTR(0x35);
        }
        SndCall(2, 2, &pos, 0, 0, 0);
        zero = 0;
        StaFlagOn(pG, STA_PL_FIRE);
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        EstSet((int) this, -1, 0, 0, 0x3A, 0, 0, 0xA, zero, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
}

// wep.mode == 4 (reload): step 0 starts the reload motion of the tune level (0x36/0x3D/0x3E, or
// 0x33/0x3B/0x3C from an empty magazine) with the level's SE (0x16/0x20/0x21); at the level's
// frame (44/37/22) ItemMgr.reload refills the magazine. The player routine ends the mode.
void cObjGovernment::moveReload()
{
    static const f32 reloadEnd[3] = { 44.0f, 37.0f, 22.0f };

    if (wep.step == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x36);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3D);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3E);
                break;
            }
        } else {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x33);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3B);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3C);
                break;
            }
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
            se = 0x21;
            break;
        }
        wep.seHandle = SndCall(2, se, &pParts->world, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x3F/0x40) from the right hand's ejection
// port offset (-163, -163, 100) with a random +-15 spread, gravity 10, 30 frames, effect 0x13.
void cObjGovernment::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -163.0f;
    pos.y = -163.0f;
    pos.z = 100.0f;
    PSMTXMultVec(parts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 60.0f;
    spd.z = 60.0f;
    spd.x += fRand1_1() * 15.0f;
    spd.y += fRand1_1() * 15.0f;
    spd.z += fRand1_1() * 15.0f;
    PSMTXMultVecSR(parts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0x3F), WEP_ARC_PTR(0x40), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with the handgun-carrying footwork motions (idle, walk, run,
// turns, back, the 0x39..0x42 and 0x5D/0x5E damage set, 0x57/0x5B knife transitions; 0x3D stays
// the player archive's), the weapon hand model (right hand 1) and the archive's left hand (0x9).
void cObjGovernment::setMotion(cPlayer* pl)
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
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x30));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x31));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x8));
    pl->setRightHand(1);
    pl->setLeftHand((u32) WEP_ARC_PTR(0x9));
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep06_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x31] = ObjGovernment_init;
    OSReport("Wep06 GOVERNMENT prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x31] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
