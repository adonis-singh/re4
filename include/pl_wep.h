#ifndef PL_WEP_H
#define PL_WEP_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "obj.h"
#include "main.h"

class cPlayer;

// Player weapon object (game/objWep.cpp): a cObj whose work area holds ObjWepWork (`wep`, obj.h).
// The vtable order is objWep's `cObjWep virtual table`; the in-class bodies are the ones the
// original emits after the destructor (objWep owns the vtable, so every in-class inline is
// emitted there: add none that the target lacks).
class cObjWep : public cObjUnion {
public:
    cObjWep();
    virtual ~cObjWep() {}
    virtual void move();
    virtual void moveAll() {}
    virtual void moveStay() {}
    virtual void moveReady() {}
    virtual void moveFire() {}
    virtual void moveDown() {}
    virtual void moveReload() {}
    virtual void moveDrop() {}
    virtual void init(cModel* parent) { setAbility(5.73f, 2.86f, 0.2864f, 0.2864f); }
    virtual void setMotion(cPlayer* pl) {}    // pl_sub PlReloadBullet: the launcher fills the player's motion table
    virtual void interrupt();
    virtual void endReload(int motOnly);
    // Aim sway ranges / per-frame steps in degrees (stored in radians) — each weapon module's
    // init() calls it (pl_wep PlWepLockRand).
    void setAbility(f32 pitch, f32 yaw, f32 pitchStep, f32 yawStep) {
        wep.bureX = pitch * 0.017453292f;
        wep.bureY = yaw * 0.017453292f;
        wep.bureSpeedX = pitchStep * 0.017453292f;
        wep.bureSpeedY = yawStep * 0.017453292f;
    }
    virtual int keyKamae() { return (Key.on >> 4) & 1; }   // pl_sub joyKamae
    virtual void fire() {}
    virtual void beginReload() {}

    void setDisp(int level, int onoff);
    void parentSet(cModel* pMod, int parts_no, Vec* pOffset, Vec* pAng);
    void parentRelease();
    void resetMotion();
    void trigger();
    int bulletNum();
    int reloadable();
    void drawLaserSight(int draw, int noCalc);
    void getMarkerPos(Vec* lpos, Vec* lcross);
    void satCheck();
};

// Rocket (game/objRocket.cpp): hangs on the launcher, flies with its motion and explodes on the
// scenario / water / player weapon target line (`rocket`, obj.h).
class cObjRocket : public cObjUnion {
public:
    virtual ~cObjRocket() {}
    virtual void beginEvent(u32 flag);
    virtual void move();

    void init();
    void fire();

    static const Vec lightPos;   // light set origin / range shared with the launcher (objRocket.cpp)
    static const Vec lightSize;
};

// Rocket launcher (game/objRocket.cpp): carries a cObjRocket (`launcher`, obj.h) it launches.
class cObjLauncher : public cObjWep {
public:
    cObjLauncher();
    virtual ~cObjLauncher();
    // The launcher and its loaded rocket keep moving while the game is suspended.
    virtual void setNoSuspend(int on) {
        if (on) {
            be_flag |= 0x800;
        } else {
            be_flag &= ~0x800;
        }
        if (launcher.rocket) {
            launcher.rocket->setNoSuspend(on);
        }
    }
    virtual void moveFire();
    virtual void moveDrop();
    virtual void init(cModel* pMod);
    virtual void setMotion(cPlayer* pEm);
    virtual void interrupt();
    virtual int keyKamae();

    void loadRocket();
    int ckBoss();
    void launch();
    void drop(int se);
    void grip(int onoff);
    void gripBack();
};

// Player weapon control (game/pl_wep.cpp), 0x44 bytes at cEm::pWep.
class cPlWep {
public:
    u8 pad_0[0x20];
    u8 m_EmRankPtr;              // 0x20  (ctor: 0)
    u8 m_ShotCancelCtr;              // 0x21  (wep07 pl_shotgun reload: 0)
    u8 pad_22;
    u8 m_ShotTimer;              // 0x23  (wep07 ready00: 0)
    u8 m_WepUd;      // 0x24  knife ready stance: 0 low, 1 middle, 2 high
    u8 pad_25;
    u8 m_Flag;              // 0x26  bit0: reload requested by the routine (wep11 pl_machine)
    u8 pad_27;
    f32 pitch;           // 0x28  aim pitch
    f32 m_CenterY;             // 0x2C
    f32 m_CamAdjY;             // 0x30  camera direction at the ready start (wep13 pl_rocket: the player turns to it over ready10's first frames)
    cObjWep* m_pWep;       // 0x34  weapon object (cObjLauncher for the rocket launcher)
    cObjWep* m_pWepHand;      // 0x38  second weapon object (rifles / launchers display part)
    u8 pad_3C[4];
    u8 m_LockTime;              // 0x40  lock frames left (lockInit/lockNext: 10; lockMove clears it on a stick move)  (PS2 m_LockTime)
    u8 pad_41[3];

    cPlWep();
    f32 getAngle();
    f32 getPitch();
    void move();
    int getMarkerPos(Vec* pPos);
    cEm* lockInit();
    void lockMove();
    cModel* lockNext();
    void setTrans(int on_off, int flag);   // pObj/pObj2 display by weapon (pl_sub PlSetHand)
};

// The weapon object of player `pl` and its own cAtariInfo (the object's collision with enemies
// while it is held).
#define WEP_OBJ(pl) ((pl)->Wep->m_pWep)
#define WEP_ATARI(pl) (&WEP_OBJ(pl)->sub2B4.atari)

// knife/weapon collision (pl, top, bottom, type, flags, length)
u32 PlWepHitCheck2(cModel* pl, Vec* pPos, Vec* pPos2, int weapon_no, u32 flag, f32 radius);
void PlWepLockCtrl(cModel* pl);

extern "C" {
u32 PlWepHitCheck3(Vec* pos, int type, u32 prio, f32 len);
void PlWepAutoTrack(cModel* pl, int mode, f32 rate);
void PlWepLockRandInit();
void PlWepLockRand(cModel* pl, int mflag, f32* ang_x, f32* ang_y);
void PlSetLockPitch(cModel* pl);
int GetWepSizeGroup(int wepId);
int PlCornerCheck();
cEm* SearchLockEm(Vec* pPos, cEm* pEm_now);
cEm* SearchTargetEm(Vec* pPos, cEm* pEm_now, f32 range_limit);
}

extern u8 lockCtr;
extern void (*WeaponInitFunc)(cModel*);

#endif
