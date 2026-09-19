#ifndef EM31_H
#define EM31_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"
#include "pl_cloth.h"

class cObj;
class cObj16;
class cObjPillar;
class cModelInfo;
class cEm31;

// One eye of the giant's back parasite (em31EyelidInit / em31EyelidMove / em31EyelidDmcK), 0x1C bytes.
struct EYELID_WK {
    u8 Rno;             // 0x00  0 open wait, 1 opening, 2 blink, 3 closing, 4 broken, 5 closed
    u8 Flag;            // 0x01  the lid is shut (no damage)
    u8 Parts;             // 0x02  lid parts (rotated / scaled)
    u8 Parts2;            // 0x03  eye parts (em->dmg.m_pDamageYarare->partsNo - 1)
    int Timer;            // 0x04  frames until the next state
    f32 Dir;              // 0x08  +-3.124: lid opening direction
    f32 H1;             // 0x0C  eye parts base y
    f32 H2;            // 0x10  posY - 50: closed position
    int Hp;               // 0x14  hits left (0 = broken)
    cObj* pObj;           // 0x18  the weak point object on the eye (em31WeakInit, obj00)
};

// Work of the em31 enemy (em31 module, D:/Bio4/Prog/em31.cpp): the giant. cModel::type 0 is the body,
// type 1 the parasite tentacle on its back (em31SearchBody links the two). Overlaid on cEm from 0x3E0.
struct Em31Work {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: route found, bit3: damage / die routine, bit4: damage routine running,
                          //                bit6: berserk, bit7: catch / jump (no damage switch), bit8: tentacle weak hit,
                          //                bit9: appearing (cloth off), bit10: attack hit, bit11: tentacle dead (die variant),
                          //                bit12: down, bit13: tentacle weak point hit, bit14: down enable, bit15: dashing (foot SE)
    int Timer;            // 0x004 (0x3E4)
    int Timer2;             // 0x008 (0x3E8)
    int motVar;           // 0x00C (0x3EC)  em31_R1_BridgeVs / T_Wait: motion variant chosen at the state start
    int x010;             // 0x010 (0x3F0)
    Vec jumpSpd;          // 0x014 (0x3F4)  em31_R1_Jump: movement left towards bridgePos
    YARARE_INFO hit[29];    // 0x020 (0x400)  hit boxes (YarareAdd)
    u8 pad_604[0x638 - 0x604];
    f32 routeAng;         // 0x638 (0xA18)  Muku towards the route point (player)
    f32 Pl_rot;      // 0x63C (0xA1C)
    f32 Sub_dir;             // 0x640 (0xA20)
    f32 Sub_rot;             // 0x644 (0xA24)
    f32 Go_dir;        // 0x648 (0xA28)  copy of the route angle / distance
    f32 Go_rot;     // 0x64C (0xA2C)
    f32 L_go;             // 0x650 (0xA30)
    Vec Pl_pos;         // 0x654 (0xA34)  RouteCkPosToPos result towards the player
    Vec Sub_pos;             // 0x660 (0xA40)
    Vec Go_pos;        // 0x66C (0xA4C)
    int pEm;             // 0x678 (0xA58)
    cEm31* pBody;         // 0x67C (0xA5C)  tentacle: the body (em31SearchBody)
    cEm31* pTen;          // 0x680 (0xA60)  body: the tentacle
    cObj16* pTail[4];     // 0x684 (0xA64)  the four small tentacle objects (em31SetTail)
    cObjPillar* pPillar;  // 0x694 (0xA74)  pillar being thrown (em31_R1_T_PillarThrow)
    cObj* pWeak;          // 0x698 (0xA78)  tentacle weak point object (em31WeakInit)
    f32 Neck_dir_y;             // 0x69C (0xA7C)
    u8 pad_6A0[0x700 - 0x6A0];
    PlCloth Cloth2;       // 0x700 (0xAE0)  Em31ClothSet2 / Em31ClothMove2 (the hanging chains)
    PlCloth Cloth3;       // 0x760 (0xB40)  Em31ClothSet3 / Em31ClothMove3
    cModelInfo* pHead;    // 0x7C0 (0xBA0)  extra body model
    int pHair;             // 0x7C4 (0xBA4)
    Camera Cam;           // 0x7C8 (0xBA8)  event camera (em31EscapeCamMove / em31StampCamMove)
    Vec Target_pos;        // 0x8C0 (0xCA0)  jump target (em31JumpCk / em31BridgeJumpCk / em31_R1_BridgeVs)
    u8 EffKindId;           // 0x8CC (0xCAC)  EspPullCoreKind at creation
    u8 pad_8CD[3];
    int Atk_wait;          // 0x8D0 (0xCB0)  frames until the next attack
    int Berserk_wait;        // 0x8D4 (0xCB4)  frames until the tentacle can be hit again (450)
    int Berserk_timer;     // 0x8D8 (0xCB8)  frames the berserk lasts
    int Wake_timer;        // 0x8DC (0xCBC)  frames the down lasts (210 / 90)
    int Fire_timer;          // 0x8E0 (0xCC0)  DmgMgr hit guard timer
    int Atk_wait2;       // 0x8E4 (0xCC4)  em31AtkRtnCk retry wait
    int Total_damage;      // 0x8E8 (0xCC8)  tentacle: damage on the weak point (getTotalDamage)
    u32 Seid;            // 0x8EC (0xCCC)  voice handle (setVoice)
    u32 Breath_seid;           // 0x8F0 (0xCD0)  breath handle (em31BreathSe)
    u16 breathTimer;      // 0x8F4 (0xCD4)  frames until the next breath
    u16 pad_8F6;
    int Str_seid;             // 0x8F8 (0xCD8)
    u16 tailSeTimer;      // 0x8FC (0xCDC)  em31TailAtkCk: frames until the next tail SE
    s16 Flash_timer;         // 0x8FE (0xCDE)  frames the eyelids stay shut after a weapon 0x17 hit
    EYELID_WK Eyelid[4];    // 0x900 (0xCE0)
    u8 Atk_ck;            // 0x970 (0xD50)  the attack hit the player
    u8 Act_ck;           // 0x971 (0xD51)  the player escaped the stamp (em31ActEscape)
    u8 Atk_enable;         // 0x972 (0xD52)  tentacle: the body may attack (ckAtkEnable)
    u8 Vs_cnt;         // 0x973 (0xD53)  pillars thrown in this bridge fight
    u8 Hokan;           // 0x974 (0xD54)  em31_R1_T_Down: motion 4th argument
    u8 Down_type;          // 0x975 (0xD55)  em31DmCk: down variant 0..3
};

#define EM31_WK(em) ((Em31Work*) (((cEm31*) (em))->free))

class cEm31 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM31_WK)
    virtual void move();
    virtual int ckDownEnable();
    virtual void setDownBody();
    virtual void setDownCancel();
    virtual void setHitCrane(Vec* target);
    virtual void setDie();
    virtual void setDieNormal();
    virtual void setDieCancel();
    virtual void setAppearCancel();
    virtual int ckRocketEnable();
    virtual void setCranePos(int no);

    void setDamageCrane(int flip);
    int ckAtkEnable();
    void setAtk(int no);
    void setDashAtk();
    void setWait();
    void setClimb();
    void setPillarThrow();
    void setBerserkStart();
    void setBerserkEnd();
    void setJump();
    void setStamp(u8 no);
    void setCatch();
    void setCatchHit();
    void setStepCatch();
    void setStepCatchHit();
    void setDmNormal();
    void setDown(int flip);
    void setDownDamage();
    int ckWeakDamage();
    int getTotalDamage();
    int ckBerserk();
    int ckAtkHit();
    int ckEyeBreak();
    void setVoice(int no, int timer);
};

typedef void (*Em31Func)(cEm31*);

void Em31Init(cEm* em);
void em31DmCk(cEm31* em);
void em31DmCkT(cEm31* em);
void em31EscapeCamMove(cEm31* em);
void em31RouteCk(cEm31* em);
void Em31ClothSet2(cEm31* em, PlCloth* c);
void Em31ClothMove2(cEm31* em, PlCloth* c);
void Em31ClothSet3(cEm31* em, PlCloth* c);
void Em31ClothMove3(cEm31* em, PlCloth* c);
int em31AtkCk(cEm31* em, Vec* pos, Vec* oldPos, int no);
void em31StampCamMove(cEm31* em);
void em31SearchBody(cEm31* em);
void em31TenMatCalc(cEm31* em);
void em31TailAtkCk(cEm31* em);
void em31EyelidInit(cEm31* em);
void em31EyelidMove(cEm31* em);
int em31EyelidDmcK(cEm31* em, int dmg);
void em31TentacleConnect(cEm31* em);
void em31SmallTentacleMove(cEm31* em);
int em31SetDmVal(cEm31* em);
void em31WeakMode(cEm31* em, int on);
int em31PillarCk(cEm31* em);
int em31PillarCk2(cEm31* em);
void em31PillarAtkCk(cEm31* em, Vec* pos);
int em31JumpCk(cEm31* em);
void em31BloodSet(cEm31* em);
void em31TBloodSet(cEm31* em);
void em31SetTail(cEm31* em);
void em31FootSe(cEm31* em);
int em31BridgeJumpCk(cEm31* em);
int em31BridgeVsCk(cEm31* em, int far);
int em31AtkRtnCk(cEm31* em);
void em31CatchPosSet(cEm31* em, int climb);
int em31PLCraneCk(cEm31* em);
void em31PlHeadLost();
void em31WeakInit(cEm31* em);
void em31WeakMove(cEm31* em);
void em31BreathSe(cEm31* em);
void em31BreathSeStopCk(cEm31* em);

#endif
