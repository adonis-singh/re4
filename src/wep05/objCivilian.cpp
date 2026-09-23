// Civilian handgun weapon object (wep05 module, first object; real file name unknown): a handgun
// without weapon types, fire / reload motions by reload tune level.
//
// cObjCivilian is the cObjWep (game/objWep.cpp) of the weapon (a revolver: no cartridge ejection,
// no empty-magazine motion, a long reload), hanging on the player's right hand (parts 10) and
// driven by r_no_0 / r_no_1 from the handgun routines (wep/pl_handgun.cpp). Its left hand
// model comes from the weapon archive (0x9) instead of a player hand number.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"

class cObjCivilian : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
};

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 civilian_tbl[3];
const u8 civilian_tbl[3] = { 0x14, 0x14, 0x14 };

// ObjInitFunc[0x2E]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjCivilian_init(cObj* obj)
{
    new (obj) cObjCivilian();
}

// cObjWep::init override (Wep05_init, parent = the player): model 0x6, a 100-unit box atari with
// bits 8/9 off, hung on the right hand, light area, weapon list id 0x29, idle motion 0x34 (no
// empty variant), the civilian_tbl bytes, default lock spread.
void cObjCivilian::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    atari.init(0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 1, 0, 0);
    AtariFlagsAnd(&atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    itemId = 0x29;
    motReset[0] = WEP_ARC_PTR(0x34);
    resetMotion();
    shotFrame[0] = civilian_tbl[0];
    shotFrame[1] = civilian_tbl[1];
    shotFrame[2] = civilian_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire): step 0 starts the recoil motion 0x32, plays the shot SEs, sets
// Status_flg[0] bit23 (shot noise) and the muzzle flash 0x39; the motion's end returns to mode 0.
void cObjCivilian::moveFire()
{
    if (r_no_1 == 0) {
        MotionSetCore(this, &this->Motion, WEP_ARC_PTR(0x32), 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        EstSet(this, -1, 0, 0, EFF_WEP05, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        r_no_1 = 1;
    }
    if (MotionGetState(this)) {
        r_no_0 = 0;
        r_no_1 = 0;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the tune level (0x33/0x35/0x36) with
// the level's SE (0x16/0x20/0x21); at the level's frame (75/48/28) ItemMgr.reload refills the
// cylinder. The player routine ends the mode.
void cObjCivilian::moveReload()
{
    static const f32 reloadEnd[3] = { 75.0f, 48.0f, 28.0f };

    if (r_no_1 == 0) {
        void* m;
        u16 se;

        switch (pG->weapon_lv_reload) {
        default:
            m = WEP_ARC_PTR(0x33);
            break;
        case 1:
            m = WEP_ARC_PTR(0x35);
            break;
        case 2:
            m = WEP_ARC_PTR(0x36);
            break;
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
        r_no_1 = 1;
    } else if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Fills the player's motion table with the handgun-carrying footwork motions (idle, walk, run,
// turns, back, the 0x39..0x42 and 0x5D/0x5E damage set, 0x57/0x5B knife transitions; 0x3D stays
// the player archive's), the weapon hand model (right hand 1) and the archive's left hand (0x9).
void cObjCivilian::setMotion(cPlayer* pl)
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
    WEP_MOT(pl, 0x57, 0x31);
    WEP_MOT(pl, 0x5B, 0x30);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x8));
    pl->setRightHand(1);
    pl->setLeftHand((u32) WEP_ARC_PTR(0x9));
}
