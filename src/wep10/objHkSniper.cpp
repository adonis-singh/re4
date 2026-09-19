// Semi-auto rifle weapon object (wep10 module, first object; real file name unknown): the
// cObjHkSniper class shared with wep40/wep47 (which carry their own copies).
//
// cObjHkSniper is the cObjWep (game/objWep.cpp) of the semi-auto rifle (weapon_no 0xA), hanging
// on the player's right hand (parts 10) and driven by wep.mode / wep.step from the rifle routines
// (wep/pl_rifle.cpp): mode 2 -> moveFire (the shot's SEs and vibration; no gun motion, the
// pointer is cleared), mode 4 -> moveReload (motion by tune level, ItemMgr.reload at frame 34).
// Both modes are ended by the player routine.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 hksniper_tbl[3];
const u8 hksniper_tbl[3] = { 0x14, 0xA, 0 };

// ObjInitFunc[0x30]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjHkSniper_init(cObj* obj)
{
    new (obj) cObjHkSniper();
}

// cObjWep::init override (Wep10_init, parent = the player): model 0xA / texture 0x9 (the object
// is destroyed when the model fails), atari bits 8/9 off, hung on the right hand, light area, the
// hksniper_tbl bytes, idle motion 0x22, default lock spread.
void cObjHkSniper::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0xA), WEP_ARC_PTR(0x9)) == 0) {
        pLog->err(0, 0, "cObjSniper::init() failed.");
        ObjMgr.destroy(this);
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
    wep.x18 = hksniper_tbl[0];
    wep.x19 = hksniper_tbl[1];
    wep.x1A = hksniper_tbl[2];
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x22));
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// wep.mode == 2 (fire, set by the rifle fire00 for the semi-auto): step 0 drops the gun's motion,
// plays the shot SEs, sets Status_flg[0] bit23 (shot noise) and vibrates the pad; step 1 waits.
void cObjHkSniper::moveFire()
{
    if (wep.step == 0) {
        pMotion = 0;
        SndCall(2, 0, &pParts->world, 0, 0, 0);
        SndCall(2, 4, &pParts->world, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
}

// wep.mode == 4 (reload): step 0 starts the gun's reload motion of the tune level (0x21/0x25/
// 0x26) with the level's SE (2/0x20/0x21); at frame 34 ItemMgr.reload refills the magazine.
void cObjHkSniper::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        switch (pG->weapon_lv_reload) {
        default:
            m = WEP_ARC_PTR(0x21);
            break;
        case 1:
            m = WEP_ARC_PTR(0x25);
            break;
        case 2:
            m = WEP_ARC_PTR(0x26);
            break;
        }
        MotionSetCore(this, &this->Motion, m, 0, 0, 0, 0);
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
    }
    if (MotionCheckCrossFrame(&Motion, 34.0f)) {
        ItemMgr.reload();
    }
}

// Fills the player's motion table with the rifle-carrying footwork motions (idle, no walk [1],
// run, turns, back, the 0x39..0x42 damage set, 0x57/0x5B knife transitions; 0x3D stays the
// player archive's) and sets the weapon hand models (right hand 1, left hand 2).
void cObjHkSniper::setMotion(cPlayer* pl)
{
    PSet(pl->m_MotTbl[0x00], WEP_ARC_PTR(0x0E));
    PSet(pl->m_MotTbl[0x01], 0);
    PSet(pl->m_MotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->m_MotTbl[0x03], WEP_ARC_PTR(0x29));
    PSet(pl->m_MotTbl[0x06], WEP_ARC_PTR(0x13));
    PSet(pl->m_MotTbl[0x07], WEP_ARC_PTR(0x2B));
    PSet(pl->m_MotTbl[0x08], WEP_ARC_PTR(0x12));
    PSet(pl->m_MotTbl[0x09], WEP_ARC_PTR(0x2A));
    PSet(pl->m_MotTbl[0x0B], WEP_ARC_PTR(0x18));
    PSet(pl->m_MotTbl[0x0C], WEP_ARC_PTR(0x2C));
    PSet(pl->m_MotTbl[0x0D], WEP_ARC_PTR(0x0F));
    PSet(pl->m_MotTbl[0x0E], WEP_ARC_PTR(0x27));
    PSet(pl->m_MotTbl[0x0F], WEP_ARC_PTR(0x10));
    PSet(pl->m_MotTbl[0x10], WEP_ARC_PTR(0x28));
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x1F));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x20));
    PSet(pl->m_MotTbl[0x39], WEP_ARC_PTR(0x3A));
    PSet(pl->m_MotTbl[0x3A], WEP_ARC_PTR(0x3B));
    PSet(pl->m_MotTbl[0x3D], PL_ARC_PTR(pG->pPlayer, 0x5D));
    PSet(pl->m_MotTbl[0x41], WEP_ARC_PTR(0x3C));
    PSet(pl->m_MotTbl[0x42], WEP_ARC_PTR(0x3D));
    PSet(pl->m_MotTbl[0x3F], WEP_ARC_PTR(0x38));
    PSet(pl->m_MotTbl[0x40], WEP_ARC_PTR(0x39));
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xD));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}

