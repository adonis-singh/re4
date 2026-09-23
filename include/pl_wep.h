#ifndef PL_WEP_H
#define PL_WEP_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "obj.h"
#include "main.h"

class cPlayer;

// Player weapon object (game/objWep.cpp). The vtable order is objWep's `cObjWep virtual table`;
// the in-class bodies are the ones the original emits after the destructor (objWep owns the
// vtable, so every in-class inline is emitted there: add none that the target lacks).
class cObjWep : public cObj {
public:
    void* motReset[2];    // 0x328  resetMotion idle motions: [0] normal, [1] empty magazine (pWepArc) (PS2 motReset[2])
    f32 bureX;            // 0x330  aim sway (lock random, pl_wep PlWepLockRand): pitch range, degrees -> radians in setAbility (PS2 bureX)
    f32 bureY;            // 0x334  yaw range (PS2 bureY)
    f32 bureSpeedX;       // 0x338  pitch step per frame (PS2 bureSpeedX)
    f32 bureSpeedY;       // 0x33C  yaw step per frame (PS2 bureSpeedY)
    u8 shotFrame[4];      // 0x340  fire motion shot frames, from each weapon's const table (ruger_tbl, xd9_tbl, ...) (PS2 shotFrame[4])
    u32 m_EraseTime;      // 0x344  (PS2 m_EraseTime; setEraseTime is not in the GC code)
    cModel* m_pParent;    // 0x348  model the weapon hangs on (parentSet) (PS2 m_pParent)
    u16 itemId;           // 0x34C  weapon item id (cObjLauncher::init: 0x35) (PS2 ITEM_ID itemId)
    u8 mode;              // 0x34E  0 stay, 1 ready, 2 fire, 3 down, 4 reload, 5 drop (move dispatch)
    u8 step;              // 0x34F  step inside the mode
    u8 disp;              // 0x350  bit0 draw the laser this frame, bit1 drawn last frame, bits 2-4 setDisp types 0/1/2
    u8 etcflag;           // 0x351  (PS2 etcflag)
    s8 m_EtcTimer;        // 0x352  (PS2 m_EtcTimer)
    u8 pad_2B;
    u32 m_StopSeId;       // 0x354  SndCall handle stopped by resetMotion (PS2 m_StopSeId)
    Vec m_ShotPos;        // 0x358  laser sight end / hit marker position (pl_wep getMarkerPos, PlWepHitCheck2) (PS2 m_ShotPos)
    class cEm* m_SightEm; // 0x364  enemy the laser points at (GetWepTargetPos) (PS2 m_SightEm)

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
        bureX = pitch * 0.017453292f;
        bureY = yaw * 0.017453292f;
        bureSpeedX = pitchStep * 0.017453292f;
        bureSpeedY = yawStep * 0.017453292f;
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
class cObjRocket : public cObj {
public:
    Vec oldPos;           // 0x328  position before this frame's motion (hit line start)
    int endTimer;         // 0x334  flight frames left (300)

    virtual ~cObjRocket() {}
    virtual void beginEvent(u32 flag);
    virtual void move();
    void init();
    void fire();
    static const Vec lightPos;   // light set origin / range shared with the launcher (objRocket.cpp)
    static const Vec lightSize;
};

// Rocket launcher (game/objRocket.cpp): carries a cObjRocket it launches.
class cObjLauncher : public cObjWep {
public:
    u32 flg;              // 0x368  bit0: a rocket is in flight (PS2 cFlag flg)
    Vec lpos;             // 0x36C  launch line (getMarkerPos) (PS2 lpos)
    Vec hpos;             // 0x378  (PS2 hpos)
    cObjRocket* pRocket;  // 0x384  loaded rocket (loadRocket)

    cObjLauncher();
    virtual ~cObjLauncher();
    // The launcher and its loaded rocket keep moving while the game is suspended.
    virtual void setNoSuspend(int on) {
        if (on) {
            be_flag |= 0x800;
        } else {
            be_flag &= ~0x800;
        }
        if (pRocket) {
            pRocket->setNoSuspend(on);
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
