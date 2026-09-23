// Machine gun weapon object (wep11 / wep29 modules, first object; real file name unknown): model
// by weapon type, fire burst with cartridge ejection and reload motions.
//
// cObjMachinegun is the cObjWep (game/objWep.cpp) of the TMP: the model hanging on the player's
// right hand (parts 10), driven by mode / step, which the player routines
// (wep/pl_machine.cpp) set: mode 2 fire -> moveFire (one shot's flash, SEs, cartridge and the
// gun's own recoil motion), mode 4 reload -> moveReload (the gun motion, ItemMgr.reload at the
// tune level's frame). weapon_type 0..3 selects the model (0x6..0x9: TMP with / without the stock
// and the special variants), the idle motions and the lock random (setAbility). setMotion fills
// the player's motion table with the TMP versions of the footwork motions. ObjMachinegun_init is
// the ObjInitFunc entry the module registers for the object id.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

// ObjInitFunc entry: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjMachinegun_init(cObj* obj)
{
    new (obj) cObjMachinegun();
}

// cObjWep::init override, called by cPlayer::weaponInit with the player as parent: loads the
// model of weapon_type (0x6..0x9, texture 0x5), turns off atari bits 8/9, hangs the model on the
// player's right hand (parts 10), sets the light area, the idle motions (0x2A/0x2B normal, 0x2F
// empty), the weapon list id itemId (0x30..0x33) and the per-type lock random spread.
void cObjMachinegun::init(cModel* parent)
{
    void* bin;

    switch (pG->weapon_type) {
    default:
        bin = WEP_ARC_PTR(0x6);
        break;
    case 1:
        bin = WEP_ARC_PTR(0x7);
        break;
    case 2:
        bin = WEP_ARC_PTR(0x8);
        break;
    case 3:
        bin = WEP_ARC_PTR(0x9);
        break;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMachinegun::init() modelInit() failed.");
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

// mode == 2 (fire, set by wep11_r3_fire00 per round): step 0 starts the gun's recoil motion
// (0x27, 0x2D on the last round), sets Status_flg[0] bit23 (shot noise this frame), plays the
// three shot SEs, vibrates the pad, ejects a cartridge and spawns the muzzle flash (EstSet 0x45,
// type by weapon_type); the motion's end returns to mode 0 (stay).
void cObjMachinegun::moveFire()
{
    int type = 0;

    if (step == 0) {
        void* mot;

        if (ItemMgr.bulletNum()) {
            mot = WEP_ARC_PTR(0x27);
        } else {
            mot = WEP_ARC_PTR(0x2D);
        }
        MotionSetCore(this, &this->Motion, mot, 0, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        SndCall(2, 0x18, &pos, 0, 0, 0);
        SndCall(2, 0x15, &pos, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
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
        step = 1;
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0xA/0xB) from the right hand's ejection port
// offset (-109, -22, 90), thrown sideways / up with a random +-15 spread, gravity 10, 30 frames of
// life, with the shell-landing effect 0x13.
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

// mode == 4 (reload): step 0 starts the gun's reload motion of the reload tune level
// (0x29/0x32/0x33, or 0x28/0x30/0x31 from an empty magazine) with the magazine-out SE; when the
// motion crosses the level's frame (40/33/19) the magazine-in SE plays and ItemMgr.reload refills
// the item. The mode is left by the player routine (wep11_r2_reload).
void cObjMachinegun::moveReload()
{
    static const int reloadEnd[3] = { 40, 33, 19 };

    if (step == 0) {
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
        step = 1;
    }
    if (MotionCheckCrossFrame(&Motion, (f32) reloadEnd[pG->weapon_lv_reload])) {
        SndCall(2, 4, &pParts->world, 0, 0, 0);
        ItemMgr.reload();
    }
}

// Fills the player's motion table (m_MotTbl) with the TMP-carrying versions of the footwork
// motions (idle, walk, run, turns, back, 0x57/0x5B knife transitions, 0x39..0x42 the damage /
// stagger set; 0x3D stays the player archive's) and sets the weapon hand model (initWepHand +
// right hand 1, left hand 3). Called by cPlayer::weaponInit / PlReloadBullet.
void cObjMachinegun::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0D);
    WEP_MOT(pl, 0x02, 0x12);
    WEP_MOT(pl, 0x03, 0x13);
    WEP_MOT(pl, 0x06, 0x16);
    WEP_MOT(pl, 0x07, 0x17);
    WEP_MOT(pl, 0x08, 0x14);
    WEP_MOT(pl, 0x09, 0x15);
    WEP_MOT(pl, 0x0B, 0x18);
    WEP_MOT(pl, 0x0C, 0x19);
    WEP_MOT(pl, 0x0D, 0x0E);
    WEP_MOT(pl, 0x0E, 0x0F);
    WEP_MOT(pl, 0x0F, 0x10);
    WEP_MOT(pl, 0x10, 0x11);
    WEP_MOT(pl, 0x5B, 0x25);
    WEP_MOT(pl, 0x57, 0x26);
    WEP_MOT(pl, 0x3F, 0x34);
    WEP_MOT(pl, 0x40, 0x35);
    WEP_MOT(pl, 0x39, 0x36);
    WEP_MOT(pl, 0x3A, 0x37);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x38);
    WEP_MOT(pl, 0x42, 0x39);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0xC));
    pl->setRightHand(1);
    pl->setLeftHand(3);
}
