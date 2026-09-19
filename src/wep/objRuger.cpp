// Punisher (Ruger) weapon object (wep02 second object / wep43 first object, the same object in both;
// real file name unknown): model by weapon type, fire with cartridge ejection, reload by tune level.
// wep38 carries its own build of the class (no weapon types).
//
// cObjRuger is the cObjWep (game/objWep.cpp) of the Punisher handgun, hanging on the player's
// right hand (parts 10) and driven by wep.mode / wep.step from the handgun routines
// (wep/pl_handgun.cpp): mode 2 -> moveFire (slide motion, SEs, flash, cartridge), mode 4 ->
// moveReload (reload motion by tune level, ItemMgr.reload at its frame). weapon_type 1 is the
// upgraded (exclusive) model 0x7 with the silenced-style SE. setMotion installs the Punisher
// footwork motions into the player's table; ruger_tbl fills wep.x18..x1A.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjRuger : public cObjWep {
public:
    virtual ~cObjRuger() {}
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
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
// weapon list id wep.x24 0x23 / 0x24), a 100-unit box atari with bits 8/9 off, hung on the
// player's right hand, light area, idle motions 0x36 (normal) / 0x3B (empty), the three
// ruger_tbl bytes and the default lock random spread.
void cObjRuger::init(cModel* parent)
{
    void* bin;

    if (pG->weapon_type != 1) {
        bin = WEP_ARC_PTR(0x6);
        wep.x24 = 0x23;
    } else {
        bin = WEP_ARC_PTR(0x7);
        wep.x24 = 0x24;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
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

// wep.mode == 2 (fire, set by the handgun fire00): step 0 starts the slide motion (0x34, 0x39 on
// the last round = slide locked back), plays the shot SEs (type 0: three SEs and Status_flg[0]
// bit23 shot noise; type 1: the single SE 0x18), muzzle flash 0x36, a cartridge and the pad
// vibration; the motion's end returns to mode 0.
void cObjRuger::moveFire()
{
    if (wep.step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x34);
        } else {
            m = WEP_ARC_PTR(0x39);
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        if (pG->weapon_type == 0) {
            SndCall(2, 0, &pParts->world, 0, 0, 0);
            SndCall(2, 1, &pParts->world, 0, 0, 0);
            SndCall(2, 2, &pParts->world, 0, 0, 0);
            StaFlagOn(pG, STA_PL_FIRE);
        } else {
            SndCall(2, 0x18, &pParts->world, 0, 0, 0);
        }
        EstSet((int) this, -1, 0, 0, 0x36, 0, 0, 0xA, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

// wep.mode == 4 (reload): step 0 starts the reload motion of the reload tune level (0x38/0x3E/
// 0x3F, or 0x35/0x3C/0x3D from an empty magazine) with the level's reload SE (0x16/0x20/0x21);
// at the level's frame (31/26/17) ItemMgr.reload refills the magazine. The player routine ends
// the mode.
void cObjRuger::moveReload()
{
    static const f32 reloadEnd[3] = { 31.0f, 26.0f, 17.0f };

    if (wep.step == 0) {
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

// Fills the player's motion table with the handgun-carrying footwork motions (idle, walk, run,
// turns, back, 0x57/0x5B knife transitions, 0x39..0x42 and 0x5D/0x5E the damage set; 0x3D stays
// the player archive's) and sets the weapon hand model (right hand 1, left hand 4). Called by
// cPlayer::weaponInit / PlReloadBullet.
void cObjRuger::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x0B));
    PSet(pl->m_MotTbl[0x01], WEP_ARC_PTR(0x0C));
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x15));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x16));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x17));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x18));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x32));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x33));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x19));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x1A));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x1B));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x1C));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x1D));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x1E));
    PSet(pl->m_MotTbl[0x5D], WEP_ARC_PTR(0x1F));
    PSet(pl->m_MotTbl[0x5E], WEP_ARC_PTR(0x20));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xA));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}
