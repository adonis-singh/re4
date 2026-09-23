// wep27 module: the Klauser machine gun (own copy of the cObjMachinegun class, routines wep/pl_machine.cpp).
//
// Krauser's (mercenaries) build of the TMP: the same cObjMachinegun as wep/objMachinegun.cpp with
// its own fire motions / SEs per weapon_type (0/2: the loud type with Status_flg[0] bit23; 1/3: the
// suppressed type) and reload frames, hanging on the player's right hand (parts 10) and driven by
// r_no_0 / r_no_1 from the machine gun routines (mode 2 fire, mode 4 reload). Wep27_init is
// the WeaponInitFunc, PlMachineMove the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "item.h"
#include "motion.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlMachineMove(cPlayer* pl);   // wep/pl_machine.cpp
// The module's own copy of the class (wep_mod.h declares the wep11 one); its ObjInitFunc entry is
// static here (the REL field holds S+A).
void ObjKlauMGun_init(cObj* obj);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the machine gun (ObjMgr id 0x2D)
// as Wep->m_pWep, inits it on the player, installs its motions and loads the muzzle-flash
// effects (archive 0x4 as group 0x45).
void Wep27_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_MACHINE);

    if (obj == 0) {
        pLog->err(0, 0, "Wep27_init() cObjWep CREATE FAILED");
    } else {
        pl->Wep->m_pWep = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP11, 1);
    }
}

// cObjWep::init override (parent = the player): one model 0x6 / texture 0x5 (the object is
// destroyed when it fails), atari bits 8/9 off, hung on the right hand, light area; weapon_type
// 0..3 picks the idle motions (0x2A / 0x2B normal, 0x2F empty), the weapon list id 0x30..0x33
// and the lock random spread.
void cObjMachinegun::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMachinegun::init() modelInit() failed.");
        ObjMgr.destroy(this);
        return;
    }
    AtariFlagsAnd(&atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    switch (pG->weapon_type) {
    case 0:
    default:
        motReset[0] = WEP_ARC_PTR(0x2A);
        motReset[1] = WEP_ARC_PTR(0x2F);
        itemId = 0x30;
        setAbility(7.0f, 2.1f, 0.2864f * 0.7f, 0.2864f * 0.7f);
        break;
    case 1:
        motReset[0] = WEP_ARC_PTR(0x2A);
        motReset[1] = WEP_ARC_PTR(0x2F);
        itemId = 0x31;
        setAbility(5.73f * 0.7f, 2.86f * 0.7f, 0.2864f * 0.7f, 0.2864f * 0.7f);
        break;
    case 2:
        motReset[0] = WEP_ARC_PTR(0x2B);
        motReset[1] = WEP_ARC_PTR(0x2F);
        itemId = 0x32;
        setAbility(5.73f * 0.2f, 2.86f * 0.2f, 0.2864f * 0.5f, 0.2864f * 0.5f);
        break;
    case 3:
        motReset[0] = WEP_ARC_PTR(0x2B);
        motReset[1] = WEP_ARC_PTR(0x2F);
        itemId = 0x33;
        setAbility(5.73f * 0.2f, 2.86f * 0.2f, 0.2864f * 0.5f, 0.2864f * 0.5f);
        break;
    }
    resetMotion();
}

// mode == 2 (fire, one round per wep11_r3_fire00): step 0 starts the gun's recoil motion
// (types 0/2: 0x27, 0x2D on the last round; types 1/3: 0x2C / 0x2E), the shot SEs (loud types
// set Status_flg[0] bit23, the suppressed ones play 0x18 + 0x15), the pad vibration, a cartridge
// and the muzzle flash 0x45 (type 1 for the suppressed models); the motion's end returns to mode 0.
void cObjMachinegun::moveFire()
{
    int type = 0;

    if (r_no_1 == 0) {
        void* mot;

        switch (pG->weapon_type) {
        case 2:
        default:
            if (ItemMgr.bulletNum()) {
                mot = WEP_ARC_PTR(0x27);
            } else {
                mot = WEP_ARC_PTR(0x2D);
            }
            break;
        case 1:
        case 3:
            if (ItemMgr.bulletNum()) {
                mot = WEP_ARC_PTR(0x2C);
            } else {
                mot = WEP_ARC_PTR(0x2E);
            }
            break;
        }
        MotionSetCore(this, &this->Motion, mot, 0, 0, 0, 0);
        switch (pG->weapon_type) {
        case 2:
        default:
            SndCall(2, 0, &pos, 0, 0, 0);
            StaFlagOn(pG, STA_PL_FIRE);
            break;
        case 1:
        case 3:
            SndCall(2, 0x18, &pos, 0, 0, 0);
            SndCall(2, 0x15, &pos, 0, 0, 0);
            break;
        }
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0xA, 1);
        setCartridge();
        switch (pG->weapon_type) {
        case 0:
        case 2:
            type = 0;
            break;
        case 1:
        case 3:
            type = 1;
            break;
        }
        EstSet(this, -1, 0, 0, EFF_WEP11, type, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        r_no_1 = 1;
    }
    if (MotionGetState(this)) {
        r_no_0 = 0;
        r_no_1 = 0;
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0xA/0xB) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjMachinegun::setCartridge()
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
    obj = SetObj10(WEP_ARC_PTR(0xA), WEP_ARC_PTR(0xB), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the reload tune level (0x29/0x32/
// 0x33, or 0x28/0x30/0x31 from an empty magazine) with the magazine-out SE; at the weapon type's
// frame (40 / 52) the magazine-in SE plays and ItemMgr.reload refills. The player routine ends the mode.
void cObjMachinegun::moveReload()
{
    static const int reloadEnd[4] = { 40, 40, 52, 52 };

    if (r_no_1 == 0) {
        void* mot;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_lv_reload) {
            default:
                mot = WEP_ARC_PTR(0x29);
                break;
            case 1:
                mot = WEP_ARC_PTR(0x32);
                break;
            case 2:
                mot = WEP_ARC_PTR(0x33);
                break;
            }
        } else {
            switch (pG->weapon_lv_reload) {
            default:
                mot = WEP_ARC_PTR(0x28);
                break;
            case 1:
                mot = WEP_ARC_PTR(0x30);
                break;
            case 2:
                mot = WEP_ARC_PTR(0x31);
                break;
            }
        }
        motionSet(mot, 0, 0, 1, 0);
        m_StopSeId = SndCall(2, 2, &pParts->world, 0, 0, 0);
        r_no_1 = 1;
    }
    if (MotionCheckCrossFrame(&Motion, (f32) reloadEnd[pG->weapon_type])) {
        SndCall(2, 4, &pParts->world, 0, 0, 0);
        ItemMgr.reload();
    }
}

// Fills the player's motion table with Krauser's TMP-carrying footwork motions (idle, run, turns,
// back, the 0x39..0x42 damage set; no knife transitions; 0x3D stays the player archive's) and
// sets the weapon hand models (right hand 1, left hand 2).
void cObjMachinegun::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0D);
    WEP_MOT(pl, 0x02, 0x0E);
    WEP_MOT(pl, 0x03, 0x14);
    WEP_MOT(pl, 0x06, 0x10);
    WEP_MOT(pl, 0x07, 0x16);
    WEP_MOT(pl, 0x08, 0x0F);
    WEP_MOT(pl, 0x09, 0x15);
    WEP_MOT(pl, 0x0B, 0x11);
    WEP_MOT(pl, 0x0C, 0x17);
    WEP_MOT(pl, 0x0D, 0x12);
    WEP_MOT(pl, 0x0E, 0x18);
    WEP_MOT(pl, 0x0F, 0x13);
    WEP_MOT(pl, 0x10, 0x19);
    WEP_MOT(pl, 0x39, 0x36);
    WEP_MOT(pl, 0x3A, 0x37);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x38);
    WEP_MOT(pl, 0x42, 0x39);
    WEP_MOT(pl, 0x3F, 0x34);
    WEP_MOT(pl, 0x40, 0x35);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xC));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}

// ObjInitFunc[0x2D]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjKlauMGun_init(cObj* obj)
{
    new (obj) cObjMachinegun();
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep27_init;
    WeaponMoveFunc = PlMachineMove;
    ObjInitFunc[0x2D] = ObjKlauMGun_init;
    OSReport("Wep27 KLAUSER-MACHINEGUN prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x2D] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
