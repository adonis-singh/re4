// FN57 (Blacktail) weapon object (wep01 module, first object; real file name unknown): model by
// weapon type, fire with cartridge ejection, reload by tune level.
//
// cObjFn57 is the cObjWep (game/objWep.cpp) of the Blacktail, hanging on the player's right hand
// (parts 10) and driven by mode / step from the handgun routines (wep/pl_handgun.cpp):
// mode 2 -> moveFire (slide motion, SEs, flash, cartridge), mode 4 -> moveReload (motion by tune
// level, ItemMgr.reload at its frame). weapon_type 1 is the upgraded model 0x7 (weapon list id
// 0x22). setMotion installs the handgun footwork motions into the player's table.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjFn57 : public cObjWep {
public:
    virtual ~cObjFn57() {}
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 fn57_tbl[3];
const u8 fn57_tbl[3] = { 0xE, 0xC, 0xA };

// ObjInitFunc[0x32]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjFn57_init(cObj* obj)
{
    new (obj) cObjFn57();
}

// cObjWep::init override (Wep01_init, parent = the player): model 0x6 / 0x7 by weapon_type
// (weapon list id itemId 0x21 / 0x22), a 100-unit box atari with bits 8/9 off, hung on the right
// hand, light area, idle motions 0x34 (normal) / 0x39 (empty), the fn57_tbl bytes, default lock spread.
void cObjFn57::init(cModel* parent)
{
    void* bin = 0;

    switch (pG->weapon_type) {
    case 0:
        itemId = 0x21;
        bin = WEP_ARC_PTR(0x6);
        break;
    case 1:
        itemId = 0x22;
        bin = WEP_ARC_PTR(0x7);
        break;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjFn57::init() failed.");
        return;
    }
    sub2B4.atari.init(0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 1, 0, 0);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    motReset[0] = WEP_ARC_PTR(0x34);
    motReset[1] = WEP_ARC_PTR(0x39);
    resetMotion();
    shotFrame[0] = fn57_tbl[0];
    shotFrame[1] = fn57_tbl[1];
    shotFrame[2] = fn57_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// mode == 2 (fire, set by the handgun fire00): step 0 starts the slide motion (0x32, 0x37 on
// the last round), plays the shot SEs, sets Status_flg[0] bit23 (shot noise), spawns the muzzle
// flash 0x35 (type 1 for the upgraded model), a cartridge and the pad vibration; the motion's end
// returns to mode 0.
void cObjFn57::moveFire()
{
    if (step == 0) {
        void* m;
        int type;
        int zero;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x32);
        } else {
            m = WEP_ARC_PTR(0x37);
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        zero = 0;
        StaFlagOn(pG, STA_PL_FIRE);
        type = 1;
        if (pG->weapon_type == 0) {
            type = 0;
        }
        EstSet(this, -1, 0, 0, EFF_WEP01, type, 0, ESP_CORE_KIND_PL_WEP, (void*) zero, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        step = 1;
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the reload tune level (0x36/0x3C/
// 0x3D, or 0x33/0x3A/0x3B from an empty magazine) with the level's SE (0x16/0x20/0x21); at the
// level's frame (31/26/17) ItemMgr.reload refills the magazine. The player routine ends the mode.
void cObjFn57::moveReload()
{
    static const f32 reloadEnd[3] = { 31.0f, 26.0f, 17.0f };

    if (step == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x36);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3C);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3D);
                break;
            }
        } else {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x33);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3A);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3B);
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
        m_StopSeId = SndCall(2, se, &pParts->world, 0, 0, 0);
        step = 1;
    }
    if (MotionCheckCrossFrame(&Motion, reloadEnd[pG->weapon_lv_reload])) {
        ItemMgr.reload();
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x8/0x9) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjFn57::setCartridge()
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
// the player archive's) and sets the weapon hand model (right hand 1, left hand 4).
void cObjFn57::setMotion(cPlayer* pl)
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
    WEP_MOT(pl, 0x5B, 0x30);
    WEP_MOT(pl, 0x57, 0x31);
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
