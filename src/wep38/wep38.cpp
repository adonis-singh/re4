// wep38 module: Ada's handgun (the Punisher class rebuilt without weapon types, object id 0x21).
// The module object carries the class, the entry points and the handgun routine registration
// (wep/pl_handgun.cpp).
//
// Ada's cObjRuger: one model (0x6) offset in her right hand (parts 10), driven by mode /
// step from the handgun routines (mode 2 fire: slide motion, the loud or the suppressed SE set
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

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before Wep38_init's string)
extern const u8 ruger_tbl[3];
const u8 ruger_tbl[3] = { 0x10, 0xE, 0xC };

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjRuger (ObjMgr id 0x21)
// as Wep->m_pWep, inits it on the player, installs its motions, loads the muzzle-flash effects
// (archive 0x4 as group 0x35) and points the debug preview PlWepMot at 0x26..0x28.
void Wep38_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_RUGER);
    if (obj == 0) {
        pLog->err(0, 0, "Wep38_init() cObjWep CREATE FAILED");
    } else {
        pl->Wep->m_pWep = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP01, 1);
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
    sub2B4.atari.init(0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 1, 0, 0);
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
    m_pParent = parent;
    motReset[0] = WEP_ARC_PTR(0x36);
    motReset[1] = WEP_ARC_PTR(0x3B);
    resetMotion();
    shotFrame[0] = ruger_tbl[0];
    shotFrame[1] = ruger_tbl[1];
    shotFrame[2] = ruger_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire): step 0 starts the slide motion (0x34, 0x39 on the last round), the six
// shot SEs of the loud (type != 1, Status_flg[0] bit23) or suppressed set, the muzzle flash 0x35
// (type 1 variant for the suppressed model), a cartridge and the pad vibration; mode 0 at the
// motion's end.
void cObjRuger::moveFire()
{
    if (step == 0) {
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
            EstSet(this, -1, 0, 0, EFF_WEP01, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
            break;
        case 1:
            EstSet(this, -1, 0, 0, EFF_WEP01, 1, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
            break;
        }
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        step = 1;
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// mode == 4 (reload): step 0 starts the reload motion (0x3D; 0x3C from an empty magazine at
// level 1) with the level's SE (0x16/0x20/0x21); at the level's frame (31/26/17) ItemMgr.reload
// refills. The player routine ends the mode.
void cObjRuger::moveReload()
{
    static const f32 reloadEnd[3] = { 31.0f, 26.0f, 17.0f };
    void* m = WEP_ARC_PTR(0x3D);

    if (step == 0) {
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
        m_StopSeId = SndCall(2, se, &pParts->world, 0, 0, 0);
        step = 1;
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
    WEP_MOT(pl, 0x00, 0x0B);
    WEP_MOT(pl, 0x01, 0x0C);
    WEP_MOT(pl, 0x02, 0x0D);
    WEP_MOT(pl, 0x03, 0x0E);
    WEP_MOT(pl, 0x06, 0x11);
    WEP_MOT(pl, 0x07, 0x12);
    WEP_MOT(pl, 0x08, 0x0F);
    WEP_MOT(pl, 0x09, 0x10);
    WEP_MOT(pl, 0x0B, 0x13);
    WEP_MOT(pl, 0x0C, 0x14);
    WEP_MOT(pl, 0x0D, 0x15);
    WEP_MOT(pl, 0x0E, 0x16);
    WEP_MOT(pl, 0x0F, 0x17);
    WEP_MOT(pl, 0x10, 0x18);
    WEP_MOT(pl, 0x39, 0x1B);
    WEP_MOT(pl, 0x3A, 0x1C);
    WEP_MOT(pl, 0x41, 0x1D);
    WEP_MOT(pl, 0x42, 0x1E);
    WEP_MOT(pl, 0x3F, 0x19);
    WEP_MOT(pl, 0x40, 0x1A);
    PLA_MOT(pl, 0x3D, 0x5E);
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
