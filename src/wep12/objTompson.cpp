// Thompson weapon object (wep12 module, first object; real file name unknown): a machine gun
// without weapon types (fixed model / ability), cartridge ejection and level-dependent reload.
//
// cObjTompson (wep_mod.h) is the cObjWep of the Chicago Typewriter, hanging on the player's right
// hand (parts 10) and driven by wep.mode / wep.step from the machine gun routines
// (wep/pl_machine.cpp): mode 2 -> moveFire (one round: gun motion, flash 0x46, SEs, cartridge),
// mode 4 -> moveReload (motion by tune level, ItemMgr.reload at its frame). Both modes are ended
// by the player routine.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

// ObjInitFunc[0x25]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjTompson_init(cObj* obj)
{
    new (obj) cObjTompson();
}

// cObjWep::init override (equipWeapon, parent = the player): model 0x6 / texture 0x5, atari
// bits 8/9 off, hung on the right hand, light area, weapon list id 0x34, idle motion 0x29,
// default lock spread.
void cObjTompson::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjTompson::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    U16Set(wep.x24, 0x34);
    wep.pMotNormal = WEP_ARC_PTR(0x29);
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// wep.mode == 2 (fire, one round per wep11_r3_fire00): step 0 starts the gun's recoil motion
// (0x27, 0x2A on the last round), the muzzle flash 0x46, the shot SEs, Status_flg[0] bit23 (shot
// noise), a cartridge and the pad vibration; step 1 waits for the player routine.
void cObjTompson::moveFire()
{
    if (wep.step == 0) {
        void* mot;

        if (ItemMgr.bulletNum()) {
            mot = WEP_ARC_PTR(0x27);
        } else {
            mot = WEP_ARC_PTR(0x2A);
        }
        MotionSetCore(this, &this->Motion, mot, 0, 0, 0, 0);
        EstSet((int) this, -1, 0, 0, 0x46, 0, 0, 0, (u32) this, 0);
        SndCall(2, 0, &pParts->world, 0, 0, 0);
        SndCall(2, 0x15, &pos, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xA, 1);
        wep.step = 1;
    }
}

// wep.mode == 4 (reload): step 0 starts the gun's reload motion of the tune level (0x28/0x2B/
// 0x2C) with the level's SE (2/0x20/0x21); at the level's frame (40/34/28) ItemMgr.reload
// refills the drum.
void cObjTompson::moveReload()
{
    static const f32 reloadEnd[3] = { 40.0f, 34.0f, 28.0f };

    if (wep.step == 0) {
        void* mot;
        u16 se;

        switch (pG->weapon_lv_reload) {
        default:
            mot = WEP_ARC_PTR(0x28);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x2B);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x2C);
            break;
        }
        motionSet(mot, 0, 0, 1, 0);
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
        wep.seHandle = SndCall(2, se, &pParts->world, 0, 0, 0);
        wep.step = 1;
    } else if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0xA/0xB) from the right hand's ejection port
// offset (-216, -24, 105.9) with a random +-15 spread, gravity 10, 40 frames, landing effect 0x13.
void cObjTompson::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -216.0f;
    pos.y = -24.0f;
    pos.z = 105.9f;
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
    obj = SetObj10(WEP_ARC_PTR(0xA), WEP_ARC_PTR(0xB), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with the Thompson-carrying footwork motions (idle, no walk [1],
// run, turns, back, the 0x39..0x42 damage set, 0x57/0x5B knife transitions, 0x52; the 0x5D/0x5E
// damage motions are cleared; 0x3D stays the player archive's) and sets the weapon hand models
// (right hand 1, left hand 2).
void cObjTompson::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x0D));
    PSet(pl->m_MotTbl[0x01], 0);
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x16));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x17));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x14));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x15));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x18));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x19));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x5D], 0);
    PSet(pl->m_MotTbl[0x5E], 0);
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x25));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x26));
    PSet(pl->m_MotTbl[0x52], WEP_ARC_PTR(0x29));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x36));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x37));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x38));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x39));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x34));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x35));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xC));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}
