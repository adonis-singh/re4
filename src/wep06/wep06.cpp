// wep06 module: the Government (Matilda) handgun (cObjGovernment, object id 0x31). The module object
// carries the class, the entry points and the handgun routine registration (wep/pl_handgun.cpp).
//
// cObjGovernment is the cObjWep (game/objWep.cpp) of the Matilda, hanging on the player's right
// hand (parts 10) and driven by r_no_0 / r_no_1 from the handgun routines: mode 2 -> moveFire
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

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjGovernment (ObjMgr id
// 0x31) as Wep->m_pWep, inits it on the player, installs its motions, loads the muzzle-flash
// effects (archive 0x4 as group 0x3A) and points the debug preview PlWepMot at 0x26..0x28.
void Wep06_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_GOVERNMENT);
    if (obj == 0) {
        pLog->err(0, 0, "Wep06_init() cObjWep CREATE FAILED");
    } else {
        pl->Wep->m_pWep = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP06, 1);
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
// hand, light area, shotFrame[0..2] = 0x14, default lock spread.
void cObjGovernment::init(cModel* parent)
{
    void* bin;

    if (pG->weapon_type != 1) {
        bin = WEP_ARC_PTR(0x6);
        motReset[0] = WEP_ARC_PTR(0x34);
        motReset[1] = WEP_ARC_PTR(0x3A);
        itemId = 0x2A;
    } else {
        bin = WEP_ARC_PTR(0x7);
        motReset[0] = WEP_ARC_PTR(0x39);
        motReset[1] = WEP_ARC_PTR(0x3A);
        itemId = 0x2B;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    atari.m_flag &= 0xFCFF;
    pList->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    resetMotion();
    shotFrame[2] = shotFrame[1] = shotFrame[0] = 0x14;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire): step 0 starts the slide motion (0x32, 0x35 on the last round), plays the
// shot SEs, sets Status_flg[0] bit23 (shot noise), muzzle flash 0x3A, a cartridge and the pad
// vibration. Step 1 does nothing: the handgun routine's next state resets the mode.
void cObjGovernment::moveFire()
{
    if (r_no_1 == 0) {
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
        EstSet(this, -1, 0, 0, EFF_WEP06, 0, 0, ESP_CORE_KIND_PL_WEP, (void*) zero, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        r_no_1 = 1;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the tune level (0x36/0x3D/0x3E, or
// 0x33/0x3B/0x3C from an empty magazine) with the level's SE (0x16/0x20/0x21); at the level's
// frame (44/37/22) ItemMgr.reload refills the magazine. The player routine ends the mode.
void cObjGovernment::moveReload()
{
    static const f32 reloadEnd[3] = { 44.0f, 37.0f, 22.0f };

    if (r_no_1 == 0) {
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
        m_StopSeId = SndCall(2, se, &pList->world, 0, 0, 0);
        r_no_1 = 1;
    }
    if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x3F/0x40) from the right hand's ejection
// port offset (-163, -163, 100) with a random +-15 spread, gravity 10, 30 frames, effect 0x13.
void cObjGovernment::setCartridge()
{
    cParts* parts = pPL->getPartsPtr(0xA);
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
