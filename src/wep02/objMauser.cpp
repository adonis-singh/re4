// Red9 (Mauser) weapon object (wep02 module, first object; real file name unknown): model by weapon
// type, ready / fire / down motions, cartridge ejection, the two-step reload (magazine then pin).
//
// cObjMauser is the cObjWep (game/objWep.cpp) of the Red9 (weapon_no 3), hanging on the player's
// right hand (parts 10) and driven by wep.mode / wep.step from the handgun routines
// (wep/pl_handgun.cpp): mode 1 -> moveReady (the gun's own draw motion), 2 -> moveFire, 3 ->
// moveDown, 4 -> moveReload (magazine at reloadFrame, the stripper pin ejected at pinFrame).
// weapon_type 2 is the model with the stock (0x7, weapon list id 0x26, much smaller lock random).

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjMauser : public cObjWep {
public:
    virtual ~cObjMauser() {}
    virtual void moveReady();
    virtual void moveFire();
    virtual void moveDown();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
    void setPin();
};

// reload motion frames of the magazine change / the pin (moveReload sets them by tune level)
static f32 reloadFrame;
static f32 pinFrame;

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 mauser_tbl[3];
const u8 mauser_tbl[3] = { 0xE, 0xC, 0xA };

// ObjInitFunc[0x27]: placement-constructs the class in the work cObjMgr::construct hands over.
void ObjMauser_init(cObj* obj)
{
    new (obj) cObjMauser();
}

// cObjWep::init override (Wep02_init, parent = the player): model 0x6 (type 2: 0x7 with the
// stock, weapon list id 0x25 / 0x26, lock random spread 1/5), a box atari with bits 8/9 off, hung
// on the right hand, light area, mauser_tbl bytes, idle motions 0x34 (normal) / 0x38 (empty).
void cObjMauser::init(cModel* parent)
{
    void* bin;

    if (pG->weapon_type != 2) {
        bin = WEP_ARC_PTR(0x6);
        wep.x24 = 0x25;
        setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
    } else {
        bin = WEP_ARC_PTR(0x7);
        wep.x24 = 0x26;
        setAbility(1.146f, 0.57199997f, 0.1432f, 0.1432f);
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMauser::init() failed.");
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    wep.x18 = mauser_tbl[0];
    wep.x19 = mauser_tbl[1];
    wep.x1A = mauser_tbl[2];
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x34));
    PSet(wep.pMotEmpty, WEP_ARC_PTR(0x38));
    resetMotion();
}

// wep.mode == 1 (ready, set by the handgun ready00): plays the gun's draw motion 0x3B once, then
// back to mode 0.
void cObjMauser::moveReady()
{
    if (wep.step == 0) {
        MotionSetCore(this, &Motion, WEP_ARC_PTR(0x3B), 0, 0, 0, 0);
        wep.step = 1;
    } else if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

// wep.mode == 2 (fire): step 0 starts the bolt motion (0x32, 0x37 on the last round), plays the
// shot SEs, sets Status_flg[0] bit23 (shot noise), muzzle flash 0x36, a cartridge and the pad
// vibration; the motion's end returns to mode 0.
void cObjMauser::moveFire()
{
    if (wep.step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x32);
        } else {
            m = WEP_ARC_PTR(0x37);
        }
        MotionSetCore(this, &Motion, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        StaFlagOn(pG, STA_PL_FIRE);
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

// wep.mode == 3 (down, set by wepDown): plays the gun's holster motion 0x3C once, then mode 0.
void cObjMauser::moveDown()
{
    if (wep.step == 0) {
        MotionSetCore(this, &Motion, WEP_ARC_PTR(0x3C), 0, 0, 0, 0);
        wep.step = 1;
    } else if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

// Ejects a cartridge: an obj10 shell model (archive 0x8/0x9) from the right hand's ejection port
// offset (-109, -22, 90) with a random +-15 spread, gravity 10, 30 frames, landing effect 0x13.
void cObjMauser::setCartridge()
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

// Reload motion by ammunition state and reload tune level (x4FBA); the magazine / pin frames of
// that motion go to reloadFrame / pinFrame.
#define MAUSER_RELOAD_MOTION(m)                    \
    if (ItemMgr.bulletNum()) {                     \
        switch (pG->weapon_lv_reload) {                       \
        default:                                   \
            m = WEP_ARC_PTR(0x36);                 \
            reloadFrame = 44.0f;                   \
            pinFrame = 55.0f;                      \
            break;                                 \
        case 1:                                    \
            m = WEP_ARC_PTR(0x41);                 \
            reloadFrame = 34.0f;                   \
            pinFrame = 51.0f;                      \
            break;                                 \
        case 2:                                    \
            m = WEP_ARC_PTR(0x42);                 \
            reloadFrame = 24.0f;                   \
            pinFrame = 39.0f;                      \
            break;                                 \
        }                                          \
    } else {                                       \
        switch (pG->weapon_lv_reload) {                       \
        default:                                   \
            m = WEP_ARC_PTR(0x33);                 \
            reloadFrame = 44.0f;                   \
            pinFrame = 55.0f;                      \
            break;                                 \
        case 1:                                    \
            m = WEP_ARC_PTR(0x3F);                 \
            reloadFrame = 34.0f;                   \
            pinFrame = 51.0f;                      \
            break;                                 \
        case 2:                                    \
            m = WEP_ARC_PTR(0x40);                 \
            reloadFrame = 24.0f;                   \
            pinFrame = 39.0f;                      \
            break;                                 \
        }                                          \
    }

// wep.mode == 4 (reload): step 0 picks the reload motion by ammunition state and tune level
// (MAUSER_RELOAD_MOTION, the same table for both models) with the level's SE (0x16/0x20/0x21);
// ItemMgr.reload refills at reloadFrame (44/34/24) and the empty stripper clip is thrown away at
// pinFrame (55/51/39). The player routine ends the mode.
void cObjMauser::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        if (pG->weapon_type != 2) {
            MAUSER_RELOAD_MOTION(m);
        } else {
            MAUSER_RELOAD_MOTION(m);
        }
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
        motionSet(m, 0, 0, 1, 0);
        wep.seHandle = SndCall(2, se, &getPartsPtr(0)->world, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&Motion, reloadFrame)) {
        ItemMgr.reload();
    }
    if (MotionCheckCrossFrame(&Motion, pinFrame)) {
        setPin();
    }
}

// Throws the empty stripper clip away: an obj10 model (archive 0x43/0x44) from the gun's top
// (-270, 0, 100 in the gun's frame) with a small random spread, gravity 8, 30 frames, effect 0x13.
void cObjMauser::setPin()
{
    const f32 rad = 50.0f;
    const f32 grav = 8.0f;
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -270.0f;
    pos.y = 0.0f;
    pos.z = 100.0f;
    PSMTXMultVec(pParts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 1.0f;
    spd.y = 40.0f;
    spd.z = 40.0f;
    spd.x += fRand1_1() * 5.0f;
    spd.y += fRand1_1() * 5.0f;
    spd.z += fRand1_1() * 5.0f;
    PSMTXMultVecSR(pParts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0x43), WEP_ARC_PTR(0x44), &pos, &rot, &spd, grav, rad, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

// Fills the player's motion table with the handgun-carrying footwork motions (idle, walk, run,
// turns, back, 0x57/0x5B knife transitions, 0x39..0x42 and 0x5D/0x5E the damage set; 0x3D stays
// the player archive's) and sets the weapon hand model (right hand 1, left hand 4).
void cObjMauser::setMotion(cPlayer* pl)
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
    PSet(pl->m_MotTbl[0x5B], WEP_ARC_PTR(0x30));
    PSet(pl->m_MotTbl[0x57], WEP_ARC_PTR(0x31));
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
