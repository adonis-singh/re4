// Punisher (Ruger) weapon object (wep02 second object / wep43 first object, the same object in both;
// real file name unknown): model by weapon type, fire with cartridge ejection, reload by tune level.
// wep38 carries its own build of the class (no weapon types).
//
// cObjRuger is the cObjWep (game/objWep.cpp) of the Punisher handgun, hanging on the player's
// right hand (parts 10) and driven by r_no_0 / r_no_1 from the handgun routines
// (wep/pl_handgun.cpp): mode 2 -> moveFire (slide motion, SEs, flash, cartridge), mode 4 ->
// moveReload (reload motion by tune level, ItemMgr.reload at its frame). weapon_type 1 is the
// upgraded (exclusive) model 0x7 with the silenced-style SE. setMotion installs the Punisher
// footwork motions into the player's table; ruger_tbl fills shotFrame[0..2].

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 ruger_tbl[3];
const u8 ruger_tbl[3] = { 0x10, 0xE, 0xC };

// ObjInitFunc entry: placement-constructs the class in the work cObjMgr::construct hands over.
#ifndef OBJRUGER_NO_INIT   // pl0d (Wesker) carries this object without the entry point (src/pl0d/objRuger.cpp)
void ObjRuger_init(cObj* obj)
{
    new (obj) cObjRuger();
}
#endif

// cObjWep::init override (cPlayer::weaponInit, parent = the player): model 0x6 (type 1: 0x7,
// weapon list id itemId 0x23 / 0x24), a 100-unit box atari with bits 8/9 off, hung on the
// player's right hand, light area, idle motions 0x36 (normal) / 0x3B (empty), the three
// ruger_tbl bytes and the default lock random spread.
void cObjRuger::init(cModel* parent)
{
    void* bin;

    if (pG->weapon_type != 1) {
        bin = WEP_ARC_PTR(0x6);
        itemId = 0x23;
    } else {
        bin = WEP_ARC_PTR(0x7);
        itemId = 0x24;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    atari.init(0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 1, 0, 0);
    AtariFlagsAnd(&atari, 0xFCFF);
    pList->pParent = parent->getPartsPtr(0xA);
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

// mode == 2 (fire, set by the handgun fire00): step 0 starts the slide motion (0x34, 0x39 on
// the last round = slide locked back), plays the shot SEs (type 0: three SEs and Status_flg[0]
// bit23 shot noise; type 1: the single SE 0x18), muzzle flash 0x36, a cartridge and the pad
// vibration; the motion's end returns to mode 0.
void cObjRuger::moveFire()
{
    if (r_no_1 == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x34);
        } else {
            m = WEP_ARC_PTR(0x39);
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        if (pG->weapon_type == 0) {
            SndCall(2, 0, &pList->world, 0, 0, 0);
            SndCall(2, 1, &pList->world, 0, 0, 0);
            SndCall(2, 2, &pList->world, 0, 0, 0);
            StaFlagOn(pG, STA_PL_FIRE);
        } else {
            SndCall(2, 0x18, &pList->world, 0, 0, 0);
        }
        EstSet(this, -1, 0, 0, EFF_WEP02, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        r_no_1 = 1;
    }
    if (MotionGetState(this)) {
        r_no_0 = 0;
        r_no_1 = 0;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the reload tune level (0x38/0x3E/
// 0x3F, or 0x35/0x3C/0x3D from an empty magazine) with the level's reload SE (0x16/0x20/0x21);
// at the level's frame (31/26/17) ItemMgr.reload refills the magazine. The player routine ends
// the mode.
void cObjRuger::moveReload()
{
    static const f32 reloadEnd[3] = { 31.0f, 26.0f, 17.0f };

    if (r_no_1 == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x38);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3E);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3F);
                break;
            }
        } else {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x35);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3C);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3D);
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
    } else if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x8/0x9) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjRuger::setCartridge()
{
    cParts* parts = pPL->getPartsPtr(0xA);
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
// turns, back, 0x57/0x5B knife transitions, 0x39..0x42 and 0x5D/0x5E the damage set; 0x3D stays
// the player archive's) and sets the weapon hand model (right hand 1, left hand 4). Called by
// cPlayer::weaponInit / PlReloadBullet.
void cObjRuger::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0B);
    WEP_MOT(pl, 0x01, 0x0C);
    WEP_MOT(pl, 0x02, 0x11);
    WEP_MOT(pl, 0x03, 0x12);
    WEP_MOT(pl, 0x06, 0x15);
    WEP_MOT(pl, 0x07, 0x16);
    WEP_MOT(pl, 0x08, 0x13);
    WEP_MOT(pl, 0x09, 0x14);
    WEP_MOT(pl, 0x0B, 0x17);
    WEP_MOT(pl, 0x0C, 0x18);
    WEP_MOT(pl, 0x0D, 0x0D);
    WEP_MOT(pl, 0x0E, 0x0E);
    WEP_MOT(pl, 0x0F, 0x0F);
    WEP_MOT(pl, 0x10, 0x10);
    WEP_MOT(pl, 0x5B, 0x32);
    WEP_MOT(pl, 0x57, 0x33);
    WEP_MOT(pl, 0x39, 0x19);
    WEP_MOT(pl, 0x3A, 0x1A);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x1B);
    WEP_MOT(pl, 0x42, 0x1C);
    WEP_MOT(pl, 0x3F, 0x1D);
    WEP_MOT(pl, 0x40, 0x1E);
    WEP_MOT(pl, 0x5D, 0x1F);
    WEP_MOT(pl, 0x5E, 0x20);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xA));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}
