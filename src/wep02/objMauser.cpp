// Red9 (Mauser) weapon object (wep02 module, first object; real file name unknown): model by weapon
// type, ready / fire / down motions, cartridge ejection, the two-step reload (magazine then pin).
//
// cObjMauser is the cObjWep (game/objWep.cpp) of the Red9 (weapon_no 3), hanging on the player's
// right hand (parts 10) and driven by mode / step from the handgun routines
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

// shotFrame[0..2] of the object (an extern-linkage const: emitted here, before init's string)
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
        itemId = 0x25;
        setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
    } else {
        bin = WEP_ARC_PTR(0x7);
        itemId = 0x26;
        setAbility(1.146f, 0.57199997f, 0.1432f, 0.1432f);
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMauser::init() failed.");
        return;
    }
    sub2B4.atari.init(0.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1, 0, 0);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        LightInfo.init2(1, 1, &p0, &p1, 1);
    }
    m_pParent = parent;
    shotFrame[0] = mauser_tbl[0];
    shotFrame[1] = mauser_tbl[1];
    shotFrame[2] = mauser_tbl[2];
    motReset[0] = WEP_ARC_PTR(0x34);
    motReset[1] = WEP_ARC_PTR(0x38);
    resetMotion();
}

// mode == 1 (ready, set by the handgun ready00): plays the gun's draw motion 0x3B once, then
// back to mode 0.
void cObjMauser::moveReady()
{
    if (step == 0) {
        MotionSetCore(this, &Motion, WEP_ARC_PTR(0x3B), 0, 0, 0, 0);
        step = 1;
    } else if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// mode == 2 (fire): step 0 starts the bolt motion (0x32, 0x37 on the last round), plays the
// shot SEs, sets Status_flg[0] bit23 (shot noise), muzzle flash 0x36, a cartridge and the pad
// vibration; the motion's end returns to mode 0.
void cObjMauser::moveFire()
{
    if (step == 0) {
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
        EstSet(this, -1, 0, 0, EFF_WEP02, 0, 0, ESP_CORE_KIND_PL_WEP, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 0, 1);
        step = 1;
    }
    if (MotionGetState(this)) {
        mode = 0;
        step = 0;
    }
}

// mode == 3 (down, set by wepDown): plays the gun's holster motion 0x3C once, then mode 0.
void cObjMauser::moveDown()
{
    if (step == 0) {
        MotionSetCore(this, &Motion, WEP_ARC_PTR(0x3C), 0, 0, 0, 0);
        step = 1;
    } else if (MotionGetState(this)) {
        mode = 0;
        step = 0;
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

// mode == 4 (reload): step 0 picks the reload motion by ammunition state and tune level
// (MAUSER_RELOAD_MOTION, the same table for both models) with the level's SE (0x16/0x20/0x21);
// ItemMgr.reload refills at reloadFrame (44/34/24) and the empty stripper clip is thrown away at
// pinFrame (55/51/39). The player routine ends the mode.
void cObjMauser::moveReload()
{
    if (step == 0) {
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
        m_StopSeId = SndCall(2, se, &getPartsPtr(0)->world, 0, 0, 0);
        step = 1;
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
    WEP_MOT(pl, 0x00, 0x0B);
    WEP_MOT(pl, 0x01, 0x0C);
    WEP_MOT(pl, 0x02, 0x11);
    WEP_MOT(pl, 0x03, 0x12);
    WEP_MOT(pl, 0x06, 0x15);
    WEP_MOT(pl, 0x07, 0x16);
    WEP_MOT(pl, 0x08, 0x13);
    WEP_MOT(pl, 0x09, 0x14);
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
