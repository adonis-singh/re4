// Bolt-action rifle weapon object (wep09 module, first object; real file name unknown): scoped rifle
// with cartridge ejection after the shot and the level-dependent reload motion.
//
// cObjSniper is the cObjWep (game/objWep.cpp) of the rifle (weapon_no 9), hanging on the player's
// right hand (parts 10) and driven by r_no_0 / r_no_1 from the rifle routines
// (wep/pl_rifle.cpp): mode 2 -> moveFire is the bolt cycle (set by fire20 after the shot: the
// gun's motion 0x21 with the cartridge ejected at frame 14), mode 4 -> moveReload (motion by tune
// level, ItemMgr.reload at frame 10). The scope glass is display type 1 (setDisp in pl_rifle).

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjSniper : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 sniper_tbl[3];
const u8 sniper_tbl[3] = { 0x14, 0, 0 };

// ObjInitFunc[0x28]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjSniper_init(cObj* obj)
{
    new (obj) cObjSniper();
}

// cObjWep::init override (equipWeapon, parent = the player): weapon list id 0x2E, model 0xA /
// texture 0x9 (the object is destroyed when the model fails), atari bits 8/9 off, hung on the
// right hand, light area, idle motion 0x23, the sniper_tbl bytes, default lock spread.
void cObjSniper::init(cModel* parent)
{
    itemId = 0x2E;
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
    m_pParent = parent;
    motReset[0] = WEP_ARC_PTR(0x23);
    resetMotion();
    shotFrame[0] = sniper_tbl[0];
    shotFrame[1] = sniper_tbl[1];
    shotFrame[2] = sniper_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (the bolt cycle, set by the rifle fire20): step 0 starts the gun's bolt motion
// 0x21 at normal speed with the bolt SE and Status_flg[0] bit23; step 1 ejects the cartridge at
// frame 14 and returns to mode 0 at the motion's end.
void cObjSniper::moveFire()
{
    if (r_no_1 == 0) {
        MotionSetCore(this, &this->Motion, WEP_ARC_PTR(0x21), 0, 0, 0, 0);
        Motion.Seq_speed = 1.0f;
        SndCall(2, 4, &getPartsPtr(0)->world, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        r_no_1 = 1;
    } else {
        if (MotionCheckCrossFrame(&Motion, 14.0f)) {
            setCartridge();
        }
        if (MotionGetState(this)) {
            r_no_0 = 0;
            r_no_1 = 0;
        }
    }
}

// Ejects a cartridge: an obj10 model (archive 0xB/0xC) from the rifle's own parts 1 (the bolt)
// at (-240, -20, 70) with a random +-15 spread, gravity 10, 40 frames, landing effect 0x13.
void cObjSniper::setCartridge()
{
    cModel* parts = getPartsPtr(1);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -240.0f;
    pos.y = -20.0f;
    pos.z = 70.0f;
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
    obj = SetObj10(WEP_ARC_PTR(0xB), WEP_ARC_PTR(0xC), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// mode == 4 (reload): step 0 starts the gun's reload motion of the tune level (0x22/0x25/
// 0x26) with the level's SE (2/0x20/0x21); at frame 10 the clip effect 0x3D plays and
// ItemMgr.reload refills. The player routine ends the mode.
void cObjSniper::moveReload()
{
    if (r_no_1 == 0) {
        void* m;
        u16 se;

        switch (pG->weapon_lv_reload) {
        default:
            m = WEP_ARC_PTR(0x22);
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
        m_StopSeId = SndCall(2, se, &getPartsPtr(0)->world, 0, 0, 0);
        r_no_1 = 1;
    }
    if (MotionCheckCrossFrame(&Motion, 10.0f)) {
        EstSet(this, -1, 0, 0, EFF_WEP09, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        ItemMgr.reload();
    }
}

// Fills the player's motion table with the rifle-carrying footwork motions (idle, walk, run,
// turns, back, the 0x39..0x42 damage set, 0x57/0x5B knife transitions; 0x3D stays the player
// archive's) and sets the weapon hand models (right hand 1, left hand 2).
void cObjSniper::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0E);
    WEP_MOT(pl, 0x02, 0x11);
    WEP_MOT(pl, 0x03, 0x29);
    WEP_MOT(pl, 0x06, 0x13);
    WEP_MOT(pl, 0x07, 0x2B);
    WEP_MOT(pl, 0x08, 0x12);
    WEP_MOT(pl, 0x09, 0x2A);
    WEP_MOT(pl, 0x0B, 0x18);
    WEP_MOT(pl, 0x0C, 0x2D);
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
