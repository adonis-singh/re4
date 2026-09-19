#ifndef EMWEP_H
#define EMWEP_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"
#include "pendulum.h"
#include "camera.h"

class cPlayer;

// Work of the weapon enemy (game/emwep.cpp), overlaid on cEm from 0x3E0.
struct EmWepWork {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: setParent flag (no matrix normalisation), bit1: hidden, bit2: cloth set
    int Timer;            // 0x004 (0x3E4)  seThrow restart timer
    int Timer2;           // 0x008 (0x3E8)  Shot / ShotArrow: frames before the weapon is lost; Bomb / Flash: frame counter
    int Timer3;           // 0x00C (0x3EC)  Rocket / ShotArrow: frames without scenario collision
    int bounce;           // 0x010 (0x3F0)  Bomb / Flash / Grenade: first bounce sound pending
    u8 pad_14[0x20 - 0x14];
    int timer4;           // 0x020 (0x400)  Parent / Shot: frames until setFall
    PenCloth Cloth;       // 0x024 (0x404)  setCloth chain (10 links)
    cEm* pEm_oya;         // 0x084 (0x464)  model the weapon hangs on (setParent)
    cEm* pEm_old;          // 0x088 (0x468)  enemy that threw / shot it (setFall / setThrow keep the parent here)
    int oya_parts;          // 0x08C (0x46C)  parts of pParent
    u32 seid_throw;            // 0x090 (0x470)  SndCall handle of the throw sound
    int At_no;          // 0x094 (0x474)  scenario attribute destroyed with the weapon (setAtNo), -1 = none
    int Bomb_wait;             // 0x098 (0x478)  Bomb / Flash / Grenade fuse; ShotArrow: frames to the explosion
    void* Mot_escape;      // 0x09C (0x47C)  player motions of the grenade escape (setGrenadeThrow)
    void* motBackjump;    // 0x0A0 (0x480)
    void* motFront;       // 0x0A4 (0x484)
    void* motEscape2;     // 0x0A8 (0x488)
    Camera Cam;           // 0x0AC (0x48C)  escape event camera (emWepEscapeCamMove)
    Vec always2_offset;     // 0x1A4 (0x584)  setEffAlways2 offset in effAlwaysParts' matrix
    u8 always2_parts;    // 0x1B0 (0x590)
    u8 pad_1B1;
    u16 effAlwaysWait;    // 0x1B2 (0x592)
    u16 effAlwaysTimer;   // 0x1B4 (0x594)
    u8 pad_1B6[2];
    f32 grav;             // 0x1B8 (0x598)  gravity per frame
    f32 Roll;             // 0x1BC (0x59C)
    f32 rocketSpd;        // 0x1C0 (0x5A0)  Rocket: speed gain (+3 per frame up to 30)
    Vec pt[3];            // 0x1C4 (0x5A4)  Fall: rope node speeds kept between frames (setFall randomises them)
    Vec spd;              // 0x1E8 (0x5C8)  throw / shot speed
    u8 seFall[4];         // 0x1F4 (0x5D4)  SndCall blk / no / vol of the landing (setSeFall), [3]: played
    u8 fall_type;          // 0x1F8 (0x5D8)  setFall type: rope node offset table row
    u8 seHit[3];          // 0x1F9 (0x5D9)  hit the player
    u8 seDamage[3];       // 0x1FC (0x5DC)  damaged by the player
    u8 seThrow[4];        // 0x1FF (0x5DF)  flying sound, [3]: frames between its restarts
    u8 seHitWall[3];      // 0x203 (0x5E3)  hit the scenario
    u8 alwaysTimer;       // 0x206 (0x5E6)  setSeAlways: frames until the next call
    u8 alwaysWait;        // 0x207 (0x5E7)
    u8 seAlways[3];       // 0x208 (0x5E8)  sound played every alwaysWait frames at parts 0
    u8 effFall[2];        // 0x20B (0x5EB)  EstSet id / type when the fall ends (0xFF = none)
    u8 effHit[2];         // 0x20D (0x5ED)  EmPlBloodSet2 arguments when the player is hit
    u8 effDamage[2];      // 0x20F (0x5EF)  EstSet id / type when damaged
    u8 eff_id_always2[2];       // 0x211 (0x5F1)  EstSet id / type when entering water
    u8 effAlways[2];      // 0x213 (0x5F3)  setEffAlways2 effect
    u8 espKind;           // 0x215 (0x5F5)  effect kind deleted with the weapon (50)
    u8 Water_ck;           // 0x216 (0x5F6)
    u8 Act_ck;           // 0x217 (0x5F7)  Grenade: the player took the escape action
    EmAtkInfo* pAtk;      // 0x218 (0x5F8)  attack info used against the player (emWepAtk by default)
};

#define EMWEP_WK(em) ((EmWepWork*) (((cEmWep*) (em))->free))

// Weapon enemy (game/emwep.cpp): weapons the enemies hold, drop, throw or shoot (axes, scythes,
// arrows, rockets, dynamite, flash / hand grenades). cEmMgr::construct builds it (id 0x42).
class cEmWep : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMWEP_WK)
    virtual void beginEvent();
    virtual void move();

    void setParent(cEm* parent, int partsNo, int flag);
    void setFall(int type, Vec* spd, f32 grav);
    void setThrow(Vec* spd, f32 grav, EmAtkInfo* atk);
    void setThrowScythe(Vec* spd, EmAtkInfo* atk);
    void setShot(Vec* spd, EmAtkInfo* atk);
    void setShotArrow(Vec* spd, EmAtkInfo* atk);
    void setRocket(cEm* owner, Vec* spd, EmAtkInfo* atk);
    void setBombThrow(Vec* spd, int fuse);
    void setFlashThrow(Vec* spd, int fuse);
    void setGrenadeThrow(Vec* spd, int fuse, void* motEscape, void* motEscape2, void* motBackjump, void* motFront);
    void setSeFall(u8 blk, u8 no, u8 vol);
    void setSeDamage(u8 blk, u8 no, u8 vol);
    void setSeHit(u8 blk, u8 no, u8 vol);
    void setSeHitWall(u8 blk, u8 no, u8 vol);
    void setSeThrow(u8 blk, u8 no, u8 vol, u8 wait);
    void setSeAlways(u8 blk, u8 no, u8 vol, u8 wait);
    void setEffFall(u8 id, u8 type);
    void setEffDamage(u8 id, u8 type);
    void setEffHit(u8 id, u8 type);
    void setEffWater(u8 id, u8 type);
    void setEffAlways(int id, int type);
    void setEffAlways2(u8 id, u8 type, u8 parts, Vec* ofs, u16 wait);
    void setYarare(Vec* size, f32 w, f32 h);
    void setYarareCube(Vec* size, f32 x, f32 y, f32 z);
    void setTransMode(int on);
    void setAtNo(int no);
    void setLost();
    void setWaitDrop();
    void setCloth(cModel* owner);
    void moveCloth();
    void setParentMatCalc(int noMotion);   // objTrolley objTrolleyMoveAdjustEM (0x80019DC4)
};

extern EmAtkInfo emWepAtk;

extern "C" {
cEmWep* SetWeapon(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type);
void emWepDmCk(cEmWep* em);
void emWep_R0_Init(cEmWep* em);
void emWep_R0_Move(cEmWep* em);
void emWep_R1_Set(cEmWep* em);
void emWep_R1_LostWait(cEmWep* em);
void emWep_R1_Lost(cEmWep* em);
void emWep_R1_Fall(cEmWep* em);
void emWep_R1_Throw(cEmWep* em);
void emWep_R1_ThrowScythe(cEmWep* em);
void emWep_R1_Shot(cEmWep* em);
void emWep_R1_ShotArrow(cEmWep* em);
void emWep_R1_Rocket(cEmWep* em);
void emWepRocketBobm(cEmWep* em);
void emWepArrowBomb(cEmWep* em);
void emWep_R1_BombThrow(cEmWep* em);
void emWep_R1_FlashThrow(cEmWep* em);
void emWep_R1_GrenadeThrow(cEmWep* em);
void emWepEscapeAction(cEmWep* em);
void plemBackjump(cPlayer* pl);
void plemFrontEscape(cPlayer* pl);
void emWepEscapeCamMove(cEmWep* em);
void emWepPlHeadLost();
int emWepShotHitVaseCk(Vec* pPos, Vec* pPos2);
int emWepShotHitWindowCk(Vec* pPos, Vec* pPos2);
}

#endif
