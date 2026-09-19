#ifndef EM39_H
#define EM39_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"
#include "emwep.h"
#include "obj.h"
#include "embarrel.h"

// Work of the em39 enemy (em39 module, D:/Bio4/Prog/em39.cpp; cModel::type 0/1 = knife fight,
// 2 = the second battle), overlaid on cEm from 0x3E0. Field names are the work-relative offsets;
// the comment gives the cEm offset.
struct Em39Work {
    u32 Be_flg;            // 0x000 (0x3E0)
    int Timer;               // 0x004 (0x3E4)  routine timer
    int Timer2;               // 0x008 (0x3E8)
    int Timer3;               // 0x00C (0x3EC)
    int TmpU32;              // 0x010 (0x3F0)
    int TmpU32B;              // 0x014 (0x3F4)
    f32 TmpF;              // 0x018 (0x3F8)  jump: per-frame fall of the vertical speed
    Vec TmpV;              // 0x01C (0x3FC)  jump: remaining horizontal movement (10% per frame)
    Vec Spd;              // 0x028 (0x408)  jump: vertical speed
    YARARE_INFO hit[19];    // 0x034 (0x414)  extra hit boxes (YarareAdd / YarareAddCube in em39_R0_Init)
    u8 pad_410[0x444 - 0x410];
    f32 routeAng;         // 0x444 (0x824)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x448 (0x828)
    u8 pad_44C[8];
    f32 targetAng;        // 0x454 (0x834)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x458 (0x838)
    f32 targetDist;       // 0x45C (0x83C)
    Vec routePos;         // 0x460 (0x840)  RouteCkToPos result towards the player
    u8 pad_46C[0xC];
    Vec targetPos;        // 0x478 (0x858)  chosen target position
    cPlayer* pTarget;     // 0x484 (0x864)
    u8 pad_488[0x580 - 0x488];
    cEmWep* pWep;         // 0x580 (0x960)  knife
    cEmWep* pMachineGun;        // 0x584 (0x964)  machine gun
    cEmWep* pBow;        // 0x588 (0x968)  bow
    cEmWep* pArrow;         // 0x58C (0x96C)  thrown knife (setFall when the enemy moves)
    cEmWep* pBomb;     // 0x590 (0x970)  grenade in hand (AppearGR / AppearGR2)
    cEmWep* pFlash;       // 0x594 (0x974)  flash grenade in hand (Flash)
    f32 Neck_dir_x;             // 0x598 (0x978)  em39NeckMove: smoothed head pitch -> parts 4 addRot.x (PS2 Neck_dir_x)
    f32 Neck_dir_y;             // 0x59C (0x97C)  em39NeckMove: smoothed head yaw -> parts 4 addRot.y (PS2 Neck_dir_y)
    u8 pad_5A0[0x664 - 0x5A0];
    cModelInfo* pHandInfo;   // 0x664 (0xA44)  em39HandSet: right hand parts info
    cModelInfo* pHandL;  // 0x668 (0xA48)  em39HandSet: left hand parts info
    cModelInfo* pModKnife;     // 0x66C (0xA4C)
    u8 Hand_type;          // 0x670 (0xA50)  em39HandSet type (0xFF = none)
    u8 pad_671[3];
    cObj* pCap;         // 0x674 (0xA54)  hanging object (SetObj12)  (PS2 pCap, next to Cap_hp)
    int Cap_hp;             // 0x678 (0xA58)
    u8 pad_67C[2];
    u16 dmgTotal;         // 0x67E (0xA5E)  damage taken
    int Atk_wait;             // 0x680 (0xA60)
    int LongAtk_wait;             // 0x684 (0xA64)
    int SuperDashWait;             // 0x688 (0xA68)
    int Action_timer;             // 0x68C (0xA6C)
    int Lock_timer;          // 0x690 (0xA70)  frames the player has been locked on (em39LockCk)
    int Escape_wait;             // 0x694 (0xA74)
    int Total_damage;             // 0x698 (0xA78)  damage since the last reaction
    int Flash_damage;             // 0x69C (0xA7C)
    int Dash_wait;             // 0x6A0 (0xA80)
    Vec jumpPos;          // 0x6A4 (0xA84)  jump target (em39JumpUpCk / em39JumpDownCk)
    f32 Target_dir;          // 0x6B0 (0xA90)  facing during the jump
    Vec Goto_pos;          // 0x6B4 (0xA94)  goto target (em39RouteCk overrides the player target)
    u8 Goto_mode;            // 0x6C0 (0xAA0)
    u8 pad_6C1[3];
    struct EmiEntry* pGotoPoint;  // 0x6C4 (0xAA4)  EMI point the enemy sits / waits at
    class cEmDoor* pDoor; // 0x6C8 (0xAA8)  door the knife swing opens / breaks (em39DoorOpenCk)
    f32 gunPitch;         // 0x6CC (0xAAC)  machine gun pitch in 1/1024 turns (-255..255)
    int Hokan;             // 0x6D0 (0xAB0)
    int Frame;             // 0x6D4 (0xAB4)
    MotionWorkSub blendMot;  // 0x6D8 (0xAB8)  em39BlendMotSet second motion
    void* bowMot0;        // 0x7A8 (0xB88)  bow shot blend motions (em39BlendMotSet)
    void* bowMot1;        // 0x7AC (0xB8C)
    void* bowMot2;        // 0x7B0 (0xB90)
    void* bowMot3;        // 0x7B4 (0xB94)
    u8 pad_7B8[0xC];
    MotionWorkSub Arm_mot;  // 0x7C4 (0xBA4)  tower form left arm motion (em39ArmControl)
    int stuckCnt;         // 0x894 (0xC74)  frames the enemy moved less than half of the intended distance
    int Hide_timer;             // 0x898 (0xC78)
    int Back_atk_wait;             // 0x89C (0xC7C)
    int Fire_timer;             // 0x8A0 (0xC80)  DmgMgr hit guard timer
    int No_fire_timer;             // 0x8A4 (0xC84)
    u16 Arm_se_wait;             // 0x8A8 (0xC88)
    u8 pad_8AA[2];
    u32 Se_id;          // 0x8AC (0xC8C)  em39SetVoice SndCall id
    u32 Str_seid;            // 0x8B0 (0xC90)  Die_Normal stream request id
    u8 Wep_type;              // 0x8B4 (0xC94)  weapon in hand (em39WepSet)
    u8 Route_type;              // 0x8B5 (0xC95)
    u8 Atk_ck;              // 0x8B6 (0xC96)  attack already hit
    u8 Act_ck;              // 0x8B7 (0xC97)  action button pressed (em39ActOn)
    s16 Speech_wait;       // 0x8B8 (0xC98)  frames until the queued voice (em39SetSpeech)
    u8 Speech_se;          // 0x8BA (0xC9A)
    u8 Arm_rno;              // 0x8BB (0xC9B)
    u8 Arm_type;              // 0x8BC (0xC9C)  em39ArmControl arm pose
    u8 pad_8BD[3];
    int Old_no;             // 0x8C0 (0xCA0)  EMI appear point used last (-1 none)
    u8 Locate;              // 0x8C4 (0xCA4)  battle phase
    u8 Slant_type;         // 0x8C5 (0xCA5)
    u8 EffKindId;           // 0x8C6 (0xCA6)  EspPullCoreKind at creation
    u8 EffKindIdArrow;          // 0x8C7 (0xCA7)
};

#define EM39_WK(em) ((Em39Work*) (((cEm39*) (em))->free))

class cEm39 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM39_WK)
    virtual ~cEm39();
    virtual void move();
    virtual void setNoSuspend(int on);
    virtual void set2ndBattle();
    virtual int ckHide();
    virtual void set1stDoorClear();
    virtual void set2ndDoorClear();
    virtual void setDie();
    virtual void setDieCancel();
    virtual int ckTalk1st();
    virtual void setTalk1st();
    virtual void setTalk1stCancel();
    virtual int ckTalk2nd();
    virtual void setTalk2nd();
    virtual void setTalk2ndCancel();
    virtual int ckBombCutEnable();
};

typedef void (*Em39Func)(cEm39*);
typedef void (*PlEm39Func)(cPlayer*);

void Em39Init(cEm* em);
void em39DmCk(cEm39* em);
void em39HandSet(cEm39* em, int type);
void em39DieModelSet(cEm39* em);
void em39RouteCk(cEm39* em);
void em39NeckMove(cEm39* em);
void em39WaistMove(cEm39* em);
void em39MarkerMove(cEm39* em);
int em39GunHitCk(cEm39* em);
void em39SetCartridge(cEm39* em);
int em39LockCk(cEm39* em);
int em39HeadLockCk(cEm39* em);
int em39AtkCk(cEm39* em, int no, int parts);
int em39AtkCk2(cEm39* em, int no, Vec* a, Vec* b);
void em39PLNearTowerCk(cEm39* em);
int em39JumpDownCk(cEm39* em, int a);
int em39JumpUpCk(cEm39* em);
int em39JumpUpCk2(cEm39* em);
int em39JumpUpCk3(cEm39* em);
void em39BloodSet(cEm39* em);
void em39BlendMotSet(cEm39* em, void* m0, void* m1, void* m2, int a, int b, int c, u16 d);
int em39AppearCk(cEm39* em);
int em39ExitCk(cEm39* em);
int em39AreaMoveCk(cEm39* em);
int em39SitChg(cEm39* em);
int em39AreaCk(cEm39* em, int sub, int state, int group);
void em39WepSet(cEm39* em, int type);
int em39CatchCk(cEm39* em);
int em39KickHitCk(cEm39* em);
void em39ArrowSet(cEm39* em);
void em39ArrowFire(cEm39* em, Vec* target, int type);
void em39BowSet(cEm39* em, int on);
int em39DoorOpenCk(cEm39* em);
void em39ArmControl(cEm39* em);
int em39FanceJumpCk2(cEm39* em);
int em39FanceJumpCk(cEm39* em);
int em39SetDmVal(cEm39* em);
int em39AtkRtnCk(cEm39* em);
int em39GotoCk(cEm39* em);
void em39VoiceMove(cEm39* em);
void em39SetVoice(cEm39* em, int no);
void em39SetSpeech(cEm39* em, int no, int time);
void em39SpeechMove(cEm39* em);
int em39SlantCk(cEm39* em);
int em39SlantCk2(cEm39* em);
int em39GuardCk(cEm39* em);
void em39FootEff(cEm39* em);
void em39PLVoiceCk(cEm39* em);
void em39LeftArmAtkCk(cEm39* em, int no);
int em39GetCliffPos(cEm39* em);

#endif
