// wep38 module: Ada's handgun (the Punisher class rebuilt without weapon types, object id 0x21).
// The module object carries the class, the entry points and the handgun routine registration
// (wep/pl_handgun.cpp).
//
// Ada's cObjRuger: one model (0x6) offset in her right hand (parts 10), driven by wep.mode /
// wep.step from the handgun routines (mode 2 fire: slide motion, the loud or the suppressed SE set
// by weapon_type, flash 0x35, cartridge; mode 4 reload with only the level-1 motion variants).
// Wep38_init is the WeaponInitFunc, PlHandgunMove the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp

class cObjRuger : public cObjWep {
public:
    virtual ~cObjRuger() {}
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before Wep38_init's string)
extern const u8 ruger_tbl[3];
const u8 ruger_tbl[3] = { 0x10, 0xE, 0xC };

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjRuger (ObjMgr id 0x21)
// as Wep->m_pWep, inits it on the player, installs its motions, loads the muzzle-flash effects
// (archive 0x4 as group 0x35) and points the debug preview PlWepMot at 0x26..0x28.
void Wep38_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(0x21);
    if (obj == 0) {
        pLog->err(0, 0, "Wep38_init() cObjWep CREATE FAILED");
    } else {
        pl->Wep->m_pWep = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x35, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x26);
        PlWepMot[1] = WEP_ARC_PTR(0x27);
        PlWepMot[2] = WEP_ARC_PTR(0x28);
    }
}

// cObjWep::init override (parent = the player): model 0x6 / texture 0x5, a 100-unit box atari
// with bits 8/9 off, hung on the right hand at (22.5, 0, -5), light area, idle motions 0x36
// (normal) / 0x3B (empty), the ruger_tbl bytes, default lock spread.
void cObjRuger::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    pParts->pos.x = 22.5f;
    pParts->pos.y = 0.0f;
    pParts->pos.z = -5.0f;
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x36));
    PSet(wep.pMotEmpty, WEP_ARC_PTR(0x3B));
    resetMotion();
    wep.x18 = ruger_tbl[0];
    wep.x19 = ruger_tbl[1];
    wep.x1A = ruger_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// wep.mode == 2 (fire): step 0 starts the slide motion (0x34, 0x39 on the last round), the six
// shot SEs of the loud (type != 1, Status_flg[0] bit23) or suppressed set, the muzzle flash 0x35
// (type 1 variant for the suppressed model), a cartridge and the pad vibration; mode 0 at the
// motion's end.
void cObjRuger::moveFire()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x34);
        } else {
            m = WEP_ARC_PTR(0x39);
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        if (pG->weapon_type != 1) {
            SndCall(2, 2, &pParts->world, 0, 0, 0);
            SndCall(2, 4, &pParts->world, 0, 0, 0);
            SndCall(2, 1, &pParts->world, 0, 0, 0);
            SndCall(2, 3, &pParts->world, 0, 0, 0);
            SndCall(2, 5, &pParts->world, 0, 0, 0);
            StaFlagOn(pG, STA_PL_FIRE);
            se = 0;
        } else {
            SndCall(2, 0x15, &pParts->world, 0, 0, 0);
            SndCall(2, 0x1A, &pParts->world, 0, 0, 0);
            SndCall(2, 0x1C, &pParts->world, 0, 0, 0);
            SndCall(2, 0x19, &pParts->world, 0, 0, 0);
            SndCall(2, 0x1B, &pParts->world, 0, 0, 0);
            SndCall(2, 0x1D, &pParts->world, 0, 0, 0);
            se = 0x18;
        }
        SndCall(2, se, &pParts->world, 0, 0, 0);
        switch (pG->weapon_type) {
        case 0:
            EstSet((int) this, -1, 0, 0, 0x35, 0, 0, 0xA, 0, 0);
            break;
        case 1:
            EstSet((int) this, -1, 0, 0, 0x35, 1, 0, 0xA, 0, 0);
            break;
        }
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

// wep.mode == 4 (reload): step 0 starts the reload motion (0x3D; 0x3C from an empty magazine at
// level 1) with the level's SE (0x16/0x20/0x21); at the level's frame (31/26/17) ItemMgr.reload
// refills. The player routine ends the mode.
void cObjRuger::moveReload()
{
    static const f32 reloadEnd[3] = { 31.0f, 26.0f, 17.0f };
    void* m = WEP_ARC_PTR(0x3D);

    if (wep.step == 0) {
        u16 se;

        if (ItemMgr.bulletNum()) {
            if (pG->weapon_lv_reload == 1) {
                m = WEP_ARC_PTR(0x3D);
            }
        } else {
            if (pG->weapon_lv_reload == 1) {
                m = WEP_ARC_PTR(0x3C);
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
    } else if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x8/0x9) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjRuger::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -109.0f;
    pos.y = -22.0f;
    pos.z = 90.0f;
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
    obj = SetObj10(WEP_ARC_PTR(0x8), WEP_ARC_PTR(0x9), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with Ada's handgun footwork motions (idle, walk, run, turns,
// back, the 0x39..0x42 damage set; 0x3D from the player archive's 0x5E) and sets the weapon hand
// model (right hand 1, bare left hand 0).
void cObjRuger::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x01], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x15));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x16));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x17));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x18));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x1B));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x1C));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x1D));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x1E));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x19));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x1A));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5E));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xA));
    pl->setRightHand(1);
    pl->setLeftHand(0);
}

// ObjInitFunc[0x21]: placement-constructs the class in the work cObjMgr::construct hands over.
static void ObjRuger_init(cObj* obj)
{
    new (obj) cObjRuger();
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep38_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x21] = ObjRuger_init;
    OSReport("Wep38 ADA-HANDGUN prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x21] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
