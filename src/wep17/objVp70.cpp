// VP70 weapon object (wep17 module, first object; real file name unknown): a Mauser-style handgun
// with ready / fire motions, cartridge ejection and the reload by tune level.
//
// cObjVp70 is the cObjWep (game/objWep.cpp) of Ada's / Krauser's VP70 (weapon list id 3, the
// Red9-style small lock random), hanging on the player's right hand (parts 10) and driven by
// r_no_0 / r_no_1 from the module's own routines (wep17/wep17.cpp): mode 1 -> moveReady (the
// gun's draw motion), 2 -> moveFire, 4 -> moveReload (ItemMgr.reload at the tune level's frame).

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjVp70 : public cObjWep {
public:
    virtual ~cObjVp70() {}
    virtual void moveReady();
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 vp70_tbl[3];
const u8 vp70_tbl[3] = { 0xE, 0xC, 0xA };

#define VP70_ARC_PTR(no) PL_ARC_PTR(pG->pPlArc, no)

// ObjInitFunc[0x34]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjVp70_init(cObj* obj)
{
    new (obj) cObjVp70();
}

// cObjWep::init override (Wep17_init, parent = the player): model 0x6 / texture 0x5, atari bits
// 8/9 off, hung on the right hand, light area, weapon list id 3, idle motions 0x36 (normal) /
// 0x38 (empty), the vp70_tbl bytes, a 1/5 lock random spread.
void cObjVp70::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjVp70::init() failed.");
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
    itemId = 3;
    motReset[0] = WEP_ARC_PTR(0x36);
    motReset[1] = WEP_ARC_PTR(0x38);
    resetMotion();
    shotFrame[0] = vp70_tbl[0];
    shotFrame[1] = vp70_tbl[1];
    shotFrame[2] = vp70_tbl[2];
    setAbility(1.146f, 0.57199997f, 0.1432f, 0.1432f);
}

// mode == 1 (ready): plays the gun's draw motion 0x39 once, then mode 0.
void cObjVp70::moveReady()
{
    if (r_no_1 == 0) {
        MotionSetCore(this, &Motion, WEP_ARC_PTR(0x39), 0, 0, 0, 0);
        r_no_1 = 1;
    } else if (MotionGetState(this)) {
        r_no_0 = 0;
        r_no_1 = 0;
    }
}

// mode == 2 (fire): step 0 starts the slide motion (0x35, 0x37 on the last round), the shot
// SE, Status_flg[0] bit23 (shot noise), muzzle flash 0x4B, a cartridge and the pad vibration;
// step 1 waits for the player routine.
void cObjVp70::moveFire()
{
    if (r_no_1 == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x35);
        } else {
            m = WEP_ARC_PTR(0x37);
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
        EstSet(this, -1, 0, 0, EFF_WEP17, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        r_no_1 = 1;
    }
}

// mode == 4 (reload): step 0 starts the reload motion of the tune level (0x26/0x2B/0x2C, or
// 0x25/0x28/0x29 from an empty magazine) with the level's SE (0x16/0x20/0x21); at the level's
// frame (33/27/19) ItemMgr.reload refills the magazine.
void cObjVp70::moveReload()
{
    if (r_no_1 == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x26);
                break;
            case 1:
                m = WEP_ARC_PTR(0x2B);
                break;
            case 2:
                m = WEP_ARC_PTR(0x2C);
                break;
            }
        } else {
            switch (pG->weapon_lv_reload) {
            default:
                m = WEP_ARC_PTR(0x25);
                break;
            case 1:
                m = WEP_ARC_PTR(0x28);
                break;
            case 2:
                m = WEP_ARC_PTR(0x29);
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
        m_StopSeId = SndCall(2, se, &getPartsPtr(0)->world, 0, 0, 0);
        r_no_1 = 1;
    }
    {
        // reload frame (the magazine change) by reload tune level
        static const f32 reloadFrame[3] = { 33.0f, 27.0f, 19.0f };

        if (MotionCheckCrossFrame(&Motion, reloadFrame[pG->weapon_lv_reload])) {
            ItemMgr.reload();
        }
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x7/0x8) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjVp70::setCartridge()
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
    obj = SetObj10(WEP_ARC_PTR(0x7), WEP_ARC_PTR(0x8), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with the VP70-carrying footwork motions (idle, no walk [1],
// run, turns, back, the 0x39..0x42 damage set, 0x57/0x5B knife transitions; 0x3D stays the
// player archive's) and sets the weapon hand models (right hand 1, left hand 4).
void cObjVp70::setMotion(cPlayer* pl)
{
    WEP_MOT(pl, 0x00, 0x0A);
    NO_MOT(pl, 0x01);
    WEP_MOT(pl, 0x02, 0x0D);
    WEP_MOT(pl, 0x03, 0x1D);
    WEP_MOT(pl, 0x06, 0x0F);
    WEP_MOT(pl, 0x07, 0x1F);
    WEP_MOT(pl, 0x08, 0x0E);
    WEP_MOT(pl, 0x09, 0x1E);
    WEP_MOT(pl, 0x0B, 0x10);
    WEP_MOT(pl, 0x0C, 0x20);
    WEP_MOT(pl, 0x0D, 0x0B);
    WEP_MOT(pl, 0x0E, 0x1B);
    WEP_MOT(pl, 0x0F, 0x0C);
    WEP_MOT(pl, 0x10, 0x1C);
    WEP_MOT(pl, 0x5B, 0x41);
    WEP_MOT(pl, 0x57, 0x42);
    WEP_MOT(pl, 0x39, 0x45);
    WEP_MOT(pl, 0x3A, 0x46);
    PLA_MOT(pl, 0x3D, 0x5D);
    WEP_MOT(pl, 0x41, 0x47);
    WEP_MOT(pl, 0x42, 0x48);
    WEP_MOT(pl, 0x3F, 0x43);
    WEP_MOT(pl, 0x40, 0x44);
    pl->Body->initWepHand((u32) WEP_ARC_PTR(0x9));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}
