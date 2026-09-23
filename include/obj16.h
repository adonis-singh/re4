#ifndef OBJ16_H
#define OBJ16_H

#include "types.h"
#include "vec.h"
#include "obj.h"

extern "C" {
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void MotSetObj16(cObj* obj, void* mot, int a, int b);
}

// Enemy head work (game/obj16.cpp `cObj16`): a head model hung on parts `partsNo` of `body` that
// looks at the player (obj16NeckMove), bites (R1_Atk / R1_Critical) and fades out when its
// enemies die.
struct Obj16Work {
    u32 Be_flag;            // 0x00  bit0: the lost-wait timer runs (setLostWait / clearLostWait)
    int Timer;            // 0x04  routine step timer
    int Timer2;         // 0x08  R1_Atk: attack frames left
    u8 pad_C[4];
    cModel* pEm;       // 0x10  enemy whose position / id the SEs use (SetObj16 3rd argument)
    cModel* pOya;         // 0x14  enemy the head is attached to (SetObj16 4th argument)
    int parts_no;          // 0x18  parts of `body` the head follows
    int Se_wait;          // 0x1C  frames between the type 2 / 3 loop SEs
    u32 Seid;         // 0x20  SndCall handle of the loop SE (SndStop)
    int Lost_wait;         // 0x24  frames before the fade out when the enemies are dead (150)
    int Wait_mno;              // 0x28
    int Eff_wait;      // 0x2C  frames before the die effect (setDieEff: 3)
    int Eff_wait2;         // 0x30  frames between the idle effects
    f32 Neck_dir;          // 0x34  neck yaw toward the player (smoothed)
    void* mot[11];        // 0x38  setMotData: 0-2 idle, 3-6 bite, 7-9 (unused), 10 ...
    void* Mot_pl_dm;          // 0x64  setPlDmgMot: player damage motion (plemDmMStar)
    void* Seq_pl_dm;         // 0x68  its MotionSetCore 4th argument (PS2 u32 Seq_pl_dm)
    int Mot_no;              // 0x6C
    s16 At_hit_wait;          // 0x70  frames the kind 2 attack is disabled after a hit (90)
    u8 Eff_wait3;               // 0x72
    u8 Atk_wait;               // 0x73
    u8 Atk_timer;               // 0x74
    u8 Wait_mode;            // 0x75  the head is awake (R1_CoreMove picks the awake motions)
    u8 Appear_timer;               // 0x76  (60, counts down)
    u8 EffKindId;           // 0x77  effect owner kind (0x3D)
    u8 EffKindId2;          // 0x78  effect owner kind of the attack effects (0x3E)
    u8 EffKindId3;               // 0x79
    u8 Atk_enable;         // 0x7A  ckAtkEnable: R1_CoreMove ran this frame
    u8 Atk_ck;            // 0x7B  ckAtkHit: obj16AtkCk hit the player this frame
    Vec Scale;            // 0x7C  target scale (setScale), blended into cModel::scale by move
    class cCtrl* pCtrlGroup;  // 0x88  GetCtrlCtrl12() (Ctrl12Set on a hit)
};

// Enemy head (obj 0x16): the head / mouth model of the plaga-carrying enemies, hung on a parts of
// its body (`o16.body`). It turns toward the player (obj16NeckMove), bites (R1_Atk, R1_Critical),
// takes damage motions (R1_Damage) and fades out once its enemies are dead.
class cObj16 : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  Obj16Work

    virtual void move();
    virtual ~cObj16() {}
    void setScale(Vec* s);
    void setDieEff();
    void clearLostWait();
    void setLostWait(int n);
    void setMotData(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                    void* m9, void* m10);
    void setPlDmgMot(void* mot, void* seq);
    void setAtk(u8 flag);
    void setCritical();
    void setDamage();
    int ckAtkEnable();
    void setBurn();
    int ckAtkHit();
};

#define OBJ16_WK(o) ((Obj16Work*) (o)->free)

#endif
