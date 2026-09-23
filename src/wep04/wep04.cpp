// wep04 module: the XD9 handgun (cObjXd9, object id 0x33). The module object carries the class,
// the entry points and the handgun routine registration (wep/pl_handgun.cpp).
//
// cObjXd9 is the cObjWep (game/objWep.cpp) hanging on the player's right hand (parts 10), driven
// by r_no_0 / r_no_1 from the handgun routines: mode 2 -> moveFire (slide motion by weapon
// type, SEs, flash, cartridge), mode 4 -> moveReload (motion by reload tune level, ItemMgr.reload
// at its frame). weapon_type 1 is the upgraded model 0x7 (weapon list id 0x28). Wep04_init is the
// module's WeaponInitFunc, PlHandgunMove its WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
cObjWep* equipWeapon(cPlayer* pl);

class cObjXd9 : public cObjWep {
public:
    virtual ~cObjXd9() {}
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// shotFrame of the object (an extern-linkage const: emitted here, before Wep04_init's strings)
extern const u8 xd9_tbl[4];
const u8 xd9_tbl[4] = { 0xE, 0xC, 0x8, 0x8 };

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the weapon object as Wep->m_pWep,
// installs its motions, loads the muzzle-flash effects (archive 0x4 as group 0x38) and points the
// debug preview PlWepMot at the aim motions 0x26..0x28.
void Wep04_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep04_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP04, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x26);
        PlWepMot[1] = WEP_ARC_PTR(0x27);
        PlWepMot[2] = WEP_ARC_PTR(0x28);
    }
}

// Creates the cObjXd9 (ObjMgr id 0x33) and inits it on the player; NULL when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_XD9);

    if (obj == 0) {
        pLog->err(0, 0, "Wep04_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

// ObjInitFunc[0x33]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjXd9_init(cObj* obj)
{
    new (obj) cObjXd9();
}

// cObjWep::init override (parent = the player): model 0x6 (type 1: 0x7, weapon list id 0x27 /
// 0x28), atari bits 8/9 off, hung on the right hand, light area, idle motions 0x34 (normal) /
// 0x39 (empty), the four xd9_tbl bytes, default lock spread.
void cObjXd9::init(cModel* parent)
{
    void* bin;

    if (pG->weapon_type != 1) {
        bin = WEP_ARC_PTR(0x6);
        itemId = 0x27;
    } else {
        bin = WEP_ARC_PTR(0x7);
        itemId = 0x28;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    motReset[0] = WEP_ARC_PTR(0x34);
    motReset[1] = WEP_ARC_PTR(0x39);
    resetMotion();
    shotFrame[0] = xd9_tbl[0];
    shotFrame[1] = xd9_tbl[1];
    shotFrame[2] = xd9_tbl[2];
    shotFrame[3] = xd9_tbl[3];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire): step 0 starts the slide motion (0x32 / 0x35 by type, 0x37 / 0x38 on the
// last round), plays the shot SEs, sets Status_flg[0] bit23 (shot noise), muzzle flash 0x38 (type
// 1 for the upgraded model), a cartridge and the pad vibration; the motion's end returns to mode 0.
void cObjXd9::moveFire()
{
    if (r_no_1 == 0) {
        void* m;
        int type;
        int zero;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_type) {
            case 0:
            default:
                m = WEP_ARC_PTR(0x32);
                break;
            case 1:
                m = WEP_ARC_PTR(0x35);
                break;
            }
        } else {
            switch (pG->weapon_type) {
            case 0:
            default:
                m = WEP_ARC_PTR(0x37);
                break;
            case 1:
                m = WEP_ARC_PTR(0x38);
                break;
            }
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        SndCall(2, 4, &pos, 0, 0, 0);
        zero = 0;
        SndCall(2, zero, &pos, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        type = 0;
        if (pG->weapon_type == 1) {
            type = 1;
        }
        EstSet(this, -1, 0, 0, EFF_WEP04, type, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        r_no_1 = 1;
    } else if (MotionGetState(this)) {
        r_no_0 = 0;
        r_no_1 = 0;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the tune level (0x36/0x3C/0x3D, or
// 0x33/0x3A/0x3B from an empty magazine) with the level's SE (0x16/0x20/0x21); at the level's
// frame (32/27/17) ItemMgr.reload refills the magazine. The player routine ends the mode.
void cObjXd9::moveReload()
{
    static const f32 reloadEnd[3] = { 32.0f, 27.0f, 17.0f };

    if (r_no_1 == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x36);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3C);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3D);
                break;
            }
        } else {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x33);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3A);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3B);
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
        m_StopSeId = SndCall(2, se, &pParts->world, 0, 0, 0);
        r_no_1 = 1;
    } else if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x8/0x9) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjXd9::setCartridge()
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

// Fills the player's motion table with the handgun-carrying footwork motions (idle, walk, run,
// turns, back, the 0x39..0x42 and 0x5D/0x5E damage set, 0x57/0x5B knife transitions; 0x3D stays
// the player archive's) and sets the weapon hand model (right hand 1, left hand 4).
void cObjXd9::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0B);
    WEP_MOT(pl, 0x01, 0x0C);
    WEP_MOT(pl, 0x02, 0x11);
    WEP_MOT(pl, 0x03, 0x12);
    WEP_MOT(pl, 0x06, 0x15);
    WEP_MOT(pl, 0x07, 0x16);
    WEP_MOT(pl, 0x08, 0x13);
    WEP_MOT(pl, 0x09, 0x14);
    WEP_MOT(pl, 0x0A, 0x13);
    WEP_MOT(pl, 0x0B, 0x17);
    WEP_MOT(pl, 0x0C, 0x18);
    WEP_MOT(pl, 0x0D, 0x0D);
    WEP_MOT(pl, 0x0E, 0x0E);
    WEP_MOT(pl, 0x0F, 0x0F);
    WEP_MOT(pl, 0x10, 0x10);
    WEP_MOT(pl, 0x39, 0x19);
    WEP_MOT(pl, 0x3A, 0x1A);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x1B);
    WEP_MOT(pl, 0x42, 0x1C);
    WEP_MOT(pl, 0x3F, 0x1D);
    WEP_MOT(pl, 0x40, 0x1E);
    WEP_MOT(pl, 0x5D, 0x1F);
    WEP_MOT(pl, 0x5E, 0x20);
    WEP_MOT(pl, 0x5B, 0x30);
    WEP_MOT(pl, 0x57, 0x31);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xA));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep04_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x33] = ObjXd9_init;
    OSReport("Wep04 XD9 prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x33] = 0;
    OSReport("Wep04 XD9 epilog Ok\n");
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
