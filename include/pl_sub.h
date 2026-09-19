#ifndef PL_SUB_H
#define PL_SUB_H

#include "types.h"
#include "vec.h"

class cPlayer;

// game/pl_sub.cpp: player / partner helpers called from the rest of the game (C linkage; the joy*
// helpers, PlSetCostume and PlChangeData are C++ and declared in player.h).
extern "C" {
void PlSelect(int no);
void PlGachaInit();
void PlGachaMove();
int PlGachaGet();
void PlSetDamageSe(int no);
u32 PlGetStatus();
void PlSetCrouch();
void PlSetHand(int type, int on);
void SubCharSetHand(int no);
void SetPlDamage(cEm* em, void (*func)(cPlayer*));
void EndPlDamage();
void SetSubAux(int a, int b);
void SetSubBulldozer(int a, int b);
void SetSubDamage(cEm* em, void* mot);
void EndSubDamage();
void SubCharInit(int type, Vec* pos, f32 ang);
enum SCC_MODE {
    SCC_STAY = 0,
    SCC_CHASE = 1,
    SCC_KILL = 2,
    SCC_SLEEP = 3,
    SCC_BEHIND = 4,
    SCC_AUX_MOT = 5,
    SCC_RESET = 6,
    SCC_STOP = 7
};

void SubCharCtrl(int mode, int flag);
int SubCharCheckCtrl();
void SubCharCtrlHide(Vec* pos, int mode);
void SubCharMoveTo(int flag, f32 x, f32 y, f32 z, f32 w);
void PlSetLadder(Vec* pos, int level, f32 ang);
void PlSetNeck(int mode);
void PlEndCamera();
void PlRegistMotion(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7,
                    void* m8, void* m9, void* m10, void* m11);
void SubCharRegistMotion(void* m0, void* m1);
void PlRegistRoomEff(struct PlRoomEff* eff);
void PlReloadBullet();
void PlWaterProc(cPlayer* pl);
void PlMotionReset();
int SubCharCheckHealing();
int SubCharMotionReset();
void PlSetEyeMode(u8 mode);
f32 PlGetDirY();
void PlRegistBoss(void* a, void* b);
int PlIsArmor();
int PlSetWhistle();
int PlGetWeaponNo();
void PlSetFace(int no);
void SubCharSetFace(int no);
void PlDataRelease();
}

int PlSetCostume();
void PlChangeData();

#endif
