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
    void* Mot_escape2;    // 0x0A0 (0x480)
    void* Mot_escape3;       // 0x0A4 (0x484)
    void* Seq_escape;     // 0x0A8 (0x488)
    CAMERA Cam;           // 0x0AC (0x48C)  escape event camera (emWepEscapeCamMove)
    Vec always2_offset;     // 0x1A4 (0x584)  setEffAlways2 offset in effAlwaysParts' matrix
    u8 always2_parts;    // 0x1B0 (0x590)
    u8 pad_1B1;
    u16 always2_wait;    // 0x1B2 (0x592)
    u16 always2_timer;   // 0x1B4 (0x594)
    u8 pad_1B6[2];
    f32 Gravity;             // 0x1B8 (0x598)  gravity per frame
    f32 Roll;             // 0x1BC (0x59C)
    f32 Move_rate;        // 0x1C0 (0x5A0)  Rocket: speed gain (+3 per frame up to 30)
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
    u8 EffKindId;           // 0x215 (0x5F5)  effect kind deleted with the weapon (50)
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
    virtual void beginEvent(u32 flag);
    virtual void move();

    void setParent(cEm* pCoord, int oya_parts, int flag);
    void setFall(int type, Vec* pSpd, f32 gravity);
    void setThrow(Vec* spd, EmAtkInfo* atk, f32 grav);
    void setThrowScythe(Vec* spd, EmAtkInfo* atk);
    void setShot(Vec* spd, EmAtkInfo* atk);
    void setShotArrow(Vec* spd, EmAtkInfo* atk);
    void setRocket(cEm* owner, Vec* spd, EmAtkInfo* atk);
    void setBombThrow(Vec* pSpd, int bomb_wait);
    void setFlashThrow(Vec* pSpd, int bomb_wait);
    void setGrenadeThrow(Vec* spd, int fuse, void* motEscape, void* motEscape2, void* motBackjump, void* motFront);
    void setSeFall(u8 se_id, u8 se_no, u8 em_id);
    void setSeDamage(u8 se_id, u8 se_no, u8 em_id);
    void setSeHit(u8 se_id, u8 se_no, u8 em_id);
    void setSeHitWall(u8 se_id, u8 se_no, u8 em_id);
    void setSeThrow(u8 se_id, u8 se_no, u8 em_id, u8 wait);
    void setSeAlways(u8 se_id, u8 se_no, u8 em_id, u8 wait);
    void setEffFall(u8 eff_id, u8 est_id);
    void setEffDamage(u8 eff_id, u8 type);
    void setEffHit(u8 eff_id, u8 type);
    void setEffWater(u8 eff_id, u8 type);
    void setEffAlways(u8 eff_id, u8 est_id);
    void setEffAlways2(u8 eff_id, u8 type, u8 parts_no, Vec* pOffset, u16 wait);
    void setYarare(f32 r, f32 h, Vec* pOfs);
    void setYarareCube(f32 w, f32 h, f32 d, Vec* pOfs);
    void setTransMode(int mode);
    void setAtNo(int at_no);
    void setLost();
    void setWaitDrop();
    void setCloth(cModel* pEm);
    void moveCloth();
    void setParentMatCalc(int mode);   // objTrolley objTrolleyMoveAdjustEM (0x80019DC4)
};

extern EmAtkInfo emWepAtk;

extern "C" {
cEmWep* SetWeapon(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type);
void emWepDmCk(cEmWep* pEm);
void emWep_R0_Init(cEmWep* pEm);
void emWep_R0_Move(cEmWep* pEm);
void emWep_R1_Set(cEmWep* pEm);
void emWep_R1_LostWait(cEmWep* pEm);
void emWep_R1_Lost(cEmWep* pEm);
void emWep_R1_Fall(cEmWep* pEm);
void emWep_R1_Throw(cEmWep* pEm);
void emWep_R1_ThrowScythe(cEmWep* pEm);
void emWep_R1_Shot(cEmWep* pEm);
void emWep_R1_ShotArrow(cEmWep* pEm);
void emWep_R1_Rocket(cEmWep* pEm);
void emWepRocketBobm(cEmWep* pEm);
void emWepArrowBomb(cEmWep* pEm);
void emWep_R1_BombThrow(cEmWep* pEm);
void emWep_R1_FlashThrow(cEmWep* pEm);
void emWep_R1_GrenadeThrow(cEmWep* pEm);
void emWepEscapeAction(cEmWep* ptr);
void plemBackjump(cPlayer* pEm);
void plemFrontEscape(cPlayer* pEm);
void emWepEscapeCamMove(cEmWep* pEm);
void emWepPlHeadLost();
int emWepShotHitVaseCk(Vec* pPos, Vec* pPos2);
int emWepShotHitWindowCk(Vec* pPos, Vec* pPos2);
}

#endif
