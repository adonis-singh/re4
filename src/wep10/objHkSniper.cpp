// Semi-auto rifle weapon object (wep10 module, first object; real file name unknown): the
// cObjHkSniper class shared with wep40/wep47 (which carry their own copies).
//
// cObjHkSniper is the cObjWep (game/objWep.cpp) of the semi-auto rifle (weapon_no 0xA), hanging
// on the player's right hand (parts 10) and driven by r_no_0 / r_no_1 from the rifle routines
// (wep/pl_rifle.cpp): mode 2 -> moveFire (the shot's SEs and vibration; no gun motion, the
// pointer is cleared), mode 4 -> moveReload (motion by tune level, ItemMgr.reload at frame 34).
// Both modes are ended by the player routine.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
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
    AtariFlagsAnd(&atari, 0xFCFF);
    pList->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    shotFrame[0] = hksniper_tbl[0];
    shotFrame[1] = hksniper_tbl[1];
    shotFrame[2] = hksniper_tbl[2];
    motReset[0] = WEP_ARC_PTR(0x22);
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire, set by the rifle fire00 for the semi-auto): step 0 drops the gun's motion,
// plays the shot SEs, sets Status_flg[0] bit23 (shot noise) and vibrates the pad; step 1 waits.
void cObjHkSniper::moveFire()
{
    if (r_no_1 == 0) {
        Motion.pMot = 0;
        SndCall(2, 0, &pList->world, 0, 0, 0);
        SndCall(2, 4, &pList->world, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        r_no_1 = 1;
    }
}

// mode == 4 (reload): step 0 starts the gun's reload motion of the tune level (0x21/0x25/
// 0x26) with the level's SE (2/0x20/0x21); at frame 34 ItemMgr.reload refills the magazine.
void cObjHkSniper::moveReload()
{
    if (r_no_1 == 0) {
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
        m_StopSeId = SndCall(2, se, &pList->world, 0, 0, 0);
        r_no_1 = 1;
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
    WEP_MOT(pl, 0x00, 0x0E);
    NO_MOT(pl, 0x01);
    WEP_MOT(pl, 0x02, 0x11);
    WEP_MOT(pl, 0x03, 0x29);
    WEP_MOT(pl, 0x06, 0x13);
    WEP_MOT(pl, 0x07, 0x2B);
    WEP_MOT(pl, 0x08, 0x12);
    WEP_MOT(pl, 0x09, 0x2A);
    WEP_MOT(pl, 0x0B, 0x18);
    WEP_MOT(pl, 0x0C, 0x2C);
    WEP_MOT(pl, 0x0D, 0x0F);
    WEP_MOT(pl, 0x0E, 0x27);
    WEP_MOT(pl, 0x0F, 0x10);
    WEP_MOT(pl, 0x10, 0x28);
    WEP_MOT(pl, 0x5B, 0x1F);
    WEP_MOT(pl, 0x57, 0x20);
    WEP_MOT(pl, 0x39, 0x3A);
    WEP_MOT(pl, 0x3A, 0x3B);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x3C);
    WEP_MOT(pl, 0x42, 0x3D);
    WEP_MOT(pl, 0x3F, 0x38);
    WEP_MOT(pl, 0x40, 0x39);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xD));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}

