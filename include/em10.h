#ifndef EM10_H
#define EM10_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"
#include "emwep.h"
#include "emshield.h"
#include "pendulum.h"
#include "pl_cloth.h"
#include "camera.h"
#include "obj.h"
#include "objBull.h"
#include "obj12.h"
#include "obj16.h"
#include "obj14.h"
#include "obj15.h"
#include "objGondola.h"
#include "obj13.h"

// Shared Ganado enemy library (em10.cpp, D:/Bio4/Prog/em10.cpp): the same object is linked into the
// 16 Ganado modules em10..em17, em19..em1f, em20 (config/G4BE08/modules.py). The per-enemy files of
// each module (EmXXInit / EmXXSet / EmXXWeaponSet) fill the motion tables of the work.
//
// Work of the Ganado enemy, overlaid on cEm from 0x3E0 (em10_R0_Init prints its size: 0x818).
// Field names are the work-relative offsets; the comment gives the cEm offset.
struct Em10Work {
    u32 Be_flg;            // 0x000 (0x3E0)
    int Timer;               // 0x004 (0x3E4)  routine timer
    int Timer2;               // 0x008 (0x3E8)
    int Timer3;               // 0x00C (0x3EC)
    int Timer4;              // 0x010 (0x3F0)
    int Timer5;              // 0x014 (0x3F4)
    f32 TmpF;              // 0x018 (0x3F8)
    f32 TmpF2;              // 0x01C (0x3FC)
    int TmpU32;              // 0x020 (0x400)
    Vec TmpV;              // 0x024 (0x404)  scratch Vec (scale copy, pos copy, approach step) (PS2 TmpV)
    void* mot[79];        // 0x030 (0x410)  motion data table (Em10Set / Em10WeaponSet fill it; [0x29..] weapons)
    cEmWep* pWeapon;         // 0x16C (0x54C)  weapon in hand
    cEmWep* pWeapon2;        // 0x170 (0x550)
    cEmShield* pShield;   // 0x174 (0x554)
    cObj12* pCap;         // 0x178 (0x558)  (PS2 cObj12* pCap)
    cObj12* pGlasses;         // 0x17C (0x55C)  (PS2 cObj12* pGlasses)
    cEm* pCart;           // 0x180 (0x560)  lost head enemy  cart object of type 0xB (SetObj12; PS2 cObj12* pCart)
    cModelInfo* pRHand;     // 0x184 (0x564)  hand parts info (setHand(1))
    cModelInfo* pLHand;     // 0x188 (0x568)  hand parts info (setHand(0))
    cModelInfo* pHead;     // 0x18C (0x56C)  head parts info (em10HeadSet)  head parts info (em10HeadSet) (PS2 cModelInfo* pHead)
    cModelInfo* pRobe;     // 0x190 (0x570)  type 6: body parts info (em10ModelInit)
    cModelInfo* pCloth;     // 0x194 (0x574)  type 6: cloth parts info (em10ClothPartsSet)
    cModelInfo* pGoods;     // 0x198 (0x578)  type 6: goods parts info (em10GoodsPartsSet)
    cModelInfo* pSack;     // 0x19C (0x57C)  chainsaw Ganado: sack parts info (em10SackSet)
    cModel* pHood;         // 0x1A0 (0x580)  hood parts (em10SetAccesory; hides the head parts) (PS2 pHood)
    cModel* pWhood;         // 0x1A4 (0x584)  second hood variant (em10SetAccesory mot[23]) (PS2 pWhood)
    cModel* pAccesory[7]; // 0x1A8 (0x588)  accessory parts by flags_3C8 bits (em10SetAccesory) (PS2 pAccesory[7])
    YARARE_INFO hit[10];    // 0x1C4 (0x5A4)  extra hit boxes (YarareAdd in em10_R0_Init)
    Camera Cam;           // 0x3CC (0x7AC)  takeaway camera (em10CamMoveTakeaway installs it as CamCtrl.x250)
    u8 St_set;              // 0x4C4 (0x8A4)  chgSet value (cEm::x38D copy)  (PS2 St_set)
    u8 pad_4C5[3];
    Vec St_pos;           // 0x4C8 (0x8A8)  pos at init (PS2 St_pos)
    f32 St_dir;           // 0x4D4 (0x8B4)  rot.y at init (PS2 St_dir)
    Vec Keep_pos;             // 0x4D8 (0x8B8)  guard position: L_guard / L_pl_guard measured from it (PS2 Keep_pos)
    class cObjLadder* pLadder;  // 0x4E4 (0x8C4)  ladder being climbed / reset
    cModel* pSwitch;      // 0x4E8 (0x8C8)  setGotoSwitch: the switch object walked to
    Vec Route_target;             // 0x4EC (0x8CC)  destination of RouteCkToPos, L_go measured to it (PS2 Route_target: between Keep_pos and Return_ck_pos on both ports; the PS2 Goto_pos is 0x5F0)
    Vec Return_ck_pos;             // 0x4F8 (0x8D8)  pos saved by em10ReturnPosCk (PS2 Return_ck_pos)
    f32 Pl_dir;             // 0x504 (0x8E4)
    f32 Pl_rot;             // 0x508 (0x8E8)
    f32 Sub_dir;             // 0x50C (0x8EC)
    f32 Sub_rot;             // 0x510 (0x8F0)
    f32 L_sub;             // 0x514 (0x8F4)
    f32 Go_dir;             // 0x518 (0x8F8)
    f32 Go_rot;             // 0x51C (0x8FC)
    f32 L_go;             // 0x520 (0x900)
    f32 L_pl_route;             // 0x524 (0x904)
    f32 L_sub_route;             // 0x528 (0x908)
    f32 L_pl_guard;             // 0x52C (0x90C)
    f32 L_guard;             // 0x530 (0x910)
    Vec Pl_pos;             // 0x534 (0x914)  route point towards the player (Pl_dir) (PS2 Pl_pos)
    Vec Sub_pos;             // 0x540 (0x920)  route point towards the partner (Sub_dir) (PS2 Sub_pos)
    Vec Go_pos;             // 0x54C (0x92C)  route point towards Goto_pos (Go_dir) (PS2 Go_pos)
    class cEmWindow* pWindow;  // 0x558 (0x938)  window the Ganado breaks (em10_R1_WindowAtk)
    cEm* pTruck;          // 0x55C (0x93C)  truck enemy model the Ganado drives (em10SearchTruck)
    class cObjGondola* pGondola;  // 0x560 (0x940)  em10GetGondola (room 10F)
    cObj* pDrill;           // 0x564 (0x944)  r212 drill object (PS2 cObj* pDrill)
    class cObjGatling* pGatling;  // 0x568 (0x948)
    u8 Gatling_mode;       // 0x56C (0x94C)
    u8 pad_56D[3];
    class cCtrl* pDragon; // 0x570 (0x950)  GetCtrlDragon (room 222 dragon statues)
    class cObj16* pCore;  // 0x574 (0x954)  parasite object (em10SetParasite)  parasite core object (em10SetParasite SetObj16) (PS2 pCore)
    cEm* pTen[5];         // 0x578 (0x958)  tentacle objects around the core (SetObj16) (PS2 pTen[5])
    class cEmPartner* pParasite;  // 0x58C (0x96C)  partner enemy of another module (virtual slots only)  em25 parasite enemy of another module (virtual slots only; PS2 cEm25* pParasite)
    cModel* pGunBelt;         // 0x590 (0x970)  belt chain object (em10BeltSet: cObjChain)  (PS2 pGunBelt)
    cModel* pChain;         // 0x594 (0x974)  chain object of the chain Ganado (em10ChainSet: cObjChain)  (PS2 pChain)
    Vec Floor_ang;             // 0x598 (0x978)  smoothed floor slope angle, RotMatrix input (em10SlopeMove) (PS2 Floor_ang)
    Vec Spd;             // 0x5A4 (0x984)  fall speed (y -= 20 per frame) (PS2 Spd)
    class cCtrl* pCtrlGroup; // 0x5B0 (0x990)  GetCtrlCtrl12()
    class cCtrl* pCtrlSe; // 0x5B4 (0x994)  GetCtrlCtrl11()
    u32 Seid_voice;             // 0x5B8 (0x998)
    u32 Seid_breath;             // 0x5BC (0x99C)
    u32 Seid_frame;             // 0x5C0 (0x9A0)
    u32 Seid_csaw;             // 0x5C4 (0x9A4)
    f32 Neck_dir_x;             // 0x5C8 (0x9A8)
    f32 Neck_dir_y;             // 0x5CC (0x9AC)
    f32 Finger_dir;             // 0x5D0 (0x9B0)
    f32 Waist_dir_y;             // 0x5D4 (0x9B4)
    f32 Compress_y;             // 0x5D8 (0x9B8)
    f32 Target_dir;             // 0x5DC (0x9BC)
    Vec Target_pos;             // 0x5E0 (0x9C0)  action target pos / approach offset (scaled 0.2 per frame) (PS2 Target_pos)
    u32 Goto_mode;             // 0x5EC (0x9CC)  ckGoto  ckGoto (PS2 Goto_mode)
    Vec Goto_pos;             // 0x5F0 (0x9D0)  goto-mode destination (player / bell), copied to Route_target (PS2 Goto_pos)
    Vec Scale;        // 0x5FC (0x9DC)  scale at init
    Vec Campos;             // 0x608 (0x9E8)  takeaway camera position (PS2 Campos)
    void* evtMot[8];      // 0x614 (0x9F4)  event motions (setEvtMotion / setGondolaMotion / setDrill / setGatling)
    u32 HoseiCnt;             // 0x634 (0xA14)  frames the collision halved the movement (capped at 60) (PS2 HoseiCnt)
    u32 Lose_timer;             // 0x638 (0xA18)  frames without sight of the player (flags bit0), reset by setFindPL (PS2 Lose_timer)
    u32 Wander_route;             // 0x63C (0xA1C)  em10GetWanderRoute index (PS2 Wander_route)
    s32 Fire_timer;             // 0x640 (0xA20)  ckBombFire / bowgun ammo timer  ckBombFire: bomb fuse frames (9999 = lit) (PS2 Fire_timer)
    s16 Esc_timer;             // 0x644 (0xA24)  RouteCkEscEm while set (120 from em10WalkRtnSet) (PS2 Esc_timer)
    u16 CriAtk_wait;             // 0x646 (0xA26)  em10ClawCriAtkCk wait (PS2 CriAtk_wait)
    u16 Poison_wait;             // 0x648 (0xA28)  em10ParasiteAtkCk wait (Rnd % 300 + 300) (PS2 Poison_wait)
    u8 pad_64A[2];
    f32 Sin_neck;             // 0x64C (0xA2C)  neck sway phase: SINF(Sin_neck) * amp (PS2 Sin_neck)
    f32 Route_h;             // 0x650 (0xA30)  RouteCkToPos height out (PS2 Route_h)
    u16 Find_timer;             // 0x654 (0xA34)  em10FindLostCk: 150 after the player is found (450 after Dm_Claw), 0 -> FindLost (PS2 Find_timer)
    u16 Wander_timer;             // 0x656 (0xA36)  frames the player has been lost (flags 0x800000) (PS2 Wander_timer)
    u32 Parasite_wait;             // 0x658 (0xA38)  em10LostHead: frames until em10SetParasite (PS2 Parasite_wait)
    u16 Csaw_fake_timer;             // 0x65C (0xA3C)  em10CsawSignSe: far-away rev sound interval (PS2 Csaw_fake_timer)
    s16 Csaw_sign_wait;             // 0x65E (0xA3E)  em10CsawSignSe: chainsaw rev sound timer  (PS2 Csaw_sign_wait)
    u16 Anger_timer;             // 0x660 (0xA40)  300 after damage (em10DmCk); no flanking while set (PS2 Anger_timer)
    u8 pad_662[2];
    u32 Jcatch_wait;             // 0x664 (0xA44)  em10CatchPLRtnCk: frames until the next DashCatch lunge (Rnd % 300 + 300 after one ends or is declined) (PS2 Jcatch_wait: same row, between Parasite_wait and Slope_spd; the PS2 em10 code is not in the dump, so the use is the match: a wait set after the jumping grab and gated before the next)
    u16 Slope_timer;             // 0x668 (0xA48)  em10SlopeMove: 30 while no floor (PS2 Slope_timer)
    u8 pad_66A[2];
    f32 Slope_spd;             // 0x66C (0xA4C)  em10SlopeMove: smoothed forward displacement (PS2 Slope_spd)
    u16 Throw_timer;             // 0x670 (0xA50)  checkThrow  (PS2 Throw_timer)
    u8 pad_672[2];
    u32 Dash_wait;             // 0x674 (0xA54)  em10DashCk wait (PS2 Dash_wait)
    u8 pad_678[2];
    u16 WakeTimer;             // 0x67A (0xA5A)  em10_R1_DownWakeWait (PS2 WakeTimer)
    s16 Atk_wait;             // 0x67C (0xA5C)  em10CatchSubRtnCk: lha  frames until the next attack (EM10_WEP_ATK_CK) (PS2 Atk_wait)
    u16 Breath_se_wait;             // 0x67E (0xA5E)  voice / breath SE interval (PS2 Breath_se_wait)
    u16 Back_wait;             // 0x680 (0xA60)  frames the player has faced away (em10RouteCk flanking) (PS2 Back_wait)
    s16 Frame_timer;             // 0x682 (0xA62)  em10_R1_TorchFrame: 60 (PS2 Frame_timer)
    u16 Csaw_se_wait;             // 0x684 (0xA64)  chainsaw idle SE every 60 frames (PS2 Csaw_se_wait)
    u16 Claw_hp;             // 0x686 (0xA66)  damage left before Dm_Claw (Rnd % 150 + 150) (PS2 Claw_hp)
    u32 Csaw_se_id;       // 0x688 (0xA68)  SndCall handle of the chainsaw (PS2 Csaw_se_id)
    u32 Eff_timer;             // 0x68C (0xA6C)  burn effect (0x10 / 0x1E) interval: 120 (PS2 Eff_timer)
    u8 pad_690[4];
    u8 Eff_wait;              // 0x694 (0xA74)  parasite effect (0x10 / 0x33) interval: 29 (PS2 Eff_wait)
    u8 pad_695;
    u8 Atk_no_wait;              // 0x696 (0xA76)  SideStep / SitDown: no attack while set (PS2 Atk_no_wait)
    u8 Atk_ck;              // 0x697 (0xA77)  attack connected (em10AtkCk) (PS2 Atk_ck)
    u8 Atk_ck2;              // 0x698 (0xA78)  em10TorchFrameAtkCkSub (PS2 Atk_ck2)
    u8 pad_699;
    u8 Walk_type;              // 0x69A (0xA7A)  EM10_WALK_MOT switch (PS2 Walk_type)
    u8 DownCnt;              // 0x69B (0xA7B)  Rnd % 5 + 5 at init (PS2 DownCnt)
    u8 EffKindIdCsaw;              // 0x69C (0xA7C)  0x2C: weapon / chainsaw effects (PS2 EffKindIdCsaw)
    u8 EffKindIdEye;              // 0x69D (0xA7D)  0x2D: eye glow effects (PS2 EffKindIdEye)
    u8 EffKindIdWork;              // 0x69E (0xA7E)  effect kind of the enemy (EffectEsp*Delete)  0x2E: work effects (bucket, torch frame) (PS2 EffKindIdWork)
    u8 EffKindIdArrow;              // 0x69F (0xA7F)  0x2F: bowgun arrow effect (PS2 EffKindIdArrow)
    u8 EffKindIdCore;              // 0x6A0 (0xA80)  0x30: parasite core effects (PS2 EffKindIdCore)
    s8 Water_eff_wait;              // 0x6A1 (0xA81)  em10SetWaterEff: in-water splash interval  (PS2 Water_eff_wait)
    s8 Water_eff_wait2;              // 0x6A2 (0xA82)  em10SetWaterEff: wading ripple interval  (PS2 Water_eff_wait2)
    u8 Water_eff_wait3;              // 0x6A3 (0xA83)  em10SetDmWaterEff interval (PS2 Water_eff_wait3)
    u8 Bomb_se_wait;              // 0x6A4 (0xA84)  bomb fuse SE every 6 frames (PS2 Bomb_se_wait)
    u8 Look_cnt;              // 0x6A5 (0xA85)  EM10_ROUTE_SIGHT_CK side alternation (PS2 Look_cnt)
    u8 Gatling_roll;              // 0x6A6 (0xA86)  em10GatlingRollMove on (PS2 Gatling_roll)
    u8 pad_6A7;
    u32 Gatling_seid;             // 0x6A8 (0xA88)  gatling roll SndCall handle (PS2 Gatling_seid)
    u8 Route_type;              // 0x6AC (0xA8C)  em10RouteCk switch (PS2 Route_type)
    u8 Hand_type;              // 0x6AD (0xA8D)  em10HandSet (PS2 Hand_type)
    u8 Parasite_on;              // 0x6AE (0xA8E)  em10LostHead (PS2 Parasite_on)
    u8 Go_target;              // 0x6AF (0xA8F)  em10RouteCk (PS2 Go_target)
    u8 Trade_ck;              // 0x6B0 (0xA90)  em10TradeAction (PS2 Trade_ck)
    u8 Se_no;              // 0x6B1 (0xA91)  damage voice SE (em10SetDamageVoice) (PS2 Se_no)
    u8 Wep_type;           // 0x6B2 (0xA92)  weapon in hand kind (4 chainsaw, 8 bowgun, 9 ...)
    u8 Wep_type2;          // 0x6B3 (0xA93)
    u8 Cap_type;              // 0x6B4 (0xA94)  EM10_ACC_OBJ12 kind (PS2 Cap_type)
    u8 Arrow_num;              // 0x6B5 (0xA95)  bowgun arrows left (PS2 Arrow_num)
    u8 Now_hide;              // 0x6B6 (0xA96)  em10HideOn / em10HideOff (PS2 Now_hide)
    u8 Reset_enable;              // 0x6B7 (0xA97)  ckResetEnable  ckResetEnable (PS2 Reset_enable)
    u8 Pl_in_ck;              // 0x6B8 (0xA98)  setFindPL / em10DmCk (PS2 Pl_in_ck)
    u8 R11c_in_ck;              // 0x6B9 (0xA99)  room 11C position check (em10RouteCk) (PS2 R11c_in_ck)
    u8 R11c_in_ck2;              // 0x6BA (0xA9A)  (PS2 R11c_in_ck2)
    u8 Csaw_regist;              // 0x6BB (0xA9B)  EM10_GUARD_CK countdown (PS2 Csaw_regist)
    s8 No_adj_timer;              // 0x6BC (0xA9C)  frames the atari flag 8 stays set  frames the atari flag 8 stays set (PS2 No_adj_timer)
    u8 Landing_ck;              // 0x6BD (0xA9D)  em10_R1_Dm_Blow landed (PS2 Landing_ck)
    u8 Claw_rno_l;              // 0x6BE (0xA9E)  left claw routine (em10ClawMove) (PS2 Claw_rno_l)
    u8 Claw_rno_r;              // 0x6BF (0xA9F)  right claw routine (PS2 Claw_rno_r)
    u8 Chain_se_wait;              // 0x6C0 (0xAA0)  em10FootSe: chain Ganado SE delay (PS2 Chain_se_wait)
    u8 Die_wait;              // 0x6C1 (0xAA1)  hp == 1 death delay (PS2 Die_wait)
    u8 Atk_trg;              // 0x6C2 (0xAA2)  ckBowgunFire trigger, cleared each frame (PS2 Atk_trg)
    u8 No_dmg_timer;              // 0x6C3 (0xAA3)  em10DmCk: no damage reaction while set (PS2 No_dmg_timer)
    u8 Omake_set;              // 0x6C4 (0xAA4)  em10SetPoint done (PS2 Omake_set)
    u8 Ganado;              // 0x6C5 (0xAA5)  enemy class 0 / 1 / 2 (PS2 Ganado)
    u8 Se_tbl[19];        // 0x6C6 (0xAA6)  sound numbers (Em10SetSeTbl) (PS2 Se_tbl[19])
    u8 pad_6D9[3];
    PenCloth Cloth;       // 0x6DC (0xABC)  Em18ClothSet / Em1fClothSet / em10ChainSet / em10BeltSet
    f32 Blend;        // 0x73C (0xB1C)  em10BlendMotSet
    int Hokan;             // 0x740 (0xB20)  em10BlendMotSet: hokan frames left (low byte passed)  em10BlendMotSet: hokan frames (PS2 Hokan)
    u32 Frame;             // 0x744 (0xB24)  em10BlendMotSet: start frame (low half passed)  em10BlendMotSet: start frame (PS2 Frame)
    MotionWorkSub Sub_mot;  // 0x748 (0xB28)
};

#define EM10_WK(em) ((Em10Work*) (((cEm10*) (em))->free))

class cObjGatling;

// The enemy attached at Em10Work 0x58C lives in another module: only its virtual slots are known
// (docs/matching.md: a class with undefined virtuals emits no vtable). Slot names are the vtable byte offsets.
class cEmPartner : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMPARTNER_WK)
    virtual int v50();
    virtual void v58(cEm* em, int a, int b, int c);
    virtual int v60();
    virtual void v68();
    virtual int v70();
    virtual void v78();
    virtual int v80();
    virtual void v88(Vec* pos, f32 range);
    virtual void v90(Vec* pos, f32 range, void* sw);
    virtual int v98(int a);
    virtual void setReset();      // 0xA0
    virtual void vA8(u8 no);
    virtual void vB0();
    virtual int vB8();
    virtual void vC0();
    virtual int vC8();
};

// The Ganado (em10.cpp). Vtable order after the cEm virtuals: the declaration order below.
class cEm10 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM10_WK)
    // no user constructor: em10.cpp has no cEm10::cEm10 body (the in-class `cEm10() {}` would be
    // emitted out of line like every in-class member), EmXXInit's `new (em) cEm10()` synthesizes it
    virtual ~cEm10();
    virtual void move();
    virtual void setNoSuspend(int onoff);
    virtual int checkThrow();
    virtual void setHand(int no, int type);
    virtual void setWeaponFall();
    virtual int ckFindPL();
    virtual void setFindPL();
    virtual void clearFindPL();
    virtual int ckParasite();
    virtual u32 ckGoto();
    virtual void setGoto(Vec* pos, int range);
    virtual void setGotoSwitch(cModel* sw, int near, Vec* pos);
    virtual int ckResetEnable();
    virtual void setReset();
    virtual void chgSet(u8 no);
    virtual void setEvtMotion(void* m0, void* m1, void* m2, void* m3);
    virtual void setGondolaMotion(void* m0, void* m1, void* m2, void* m3);
    virtual void setR11DMotion(void* m0);
    virtual void setDrill(void* m0, void* m1, void* m2, void* m3);
    virtual void setGatling(cObjGatling* g, void* m0, void* m1, void* m2, void* m3);
    virtual void setGatlingMode(u8 mode);
    virtual int ckBombFire();
    virtual int ckShiled();
    virtual int ckBowgunFire();
    virtual void setSwitch(cModel* sw);
    virtual void setLost();
    virtual void setWeapon(void* bin, void* tpl, int type);
    virtual int ckWeapon();
    virtual int ckTakeAway();
    virtual void setUFOCatch(void* m0, void* m1);
    virtual int ckR305BomberEnable();
};

typedef void (*Em10Func)(cEm10*);
typedef void (*PlEm10Func)(cPlayer*);

// .data+0: the per-enemy set function _prolog stores (EmXXSet), run by em10_R0_Init.
extern Em10Func Em10SetFunc;


cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* v, f32 a, f32 b, int c, int d);
// game/obj08.cpp: the thrown projectile object (em2d poison; em10 declares them locally).
cObj* SetObj08(cModel* parent, void* bin, void* tpl, Vec* pos, Vec* rot, int flags, void* atk);
void SetObj08Spd(cObj* obj, Vec* spd, int life, f32 grav, f32 rad);
void SetObj08Est(cObj* obj, int no0, int prm0, int no1, int prm1, int no2, int prm2, int no3, int prm3, u8 flag);
void SetObj08Se(cObj* obj, u16 blk, u16 no);
void Obj01SetEst(cObj* pObj, u32 eff, u32 est, u32 action, u32 eff2, u32 est2, u32 eff3, u32 est3, u32 eff4, u32 est4);
int GetWepDmVal(cEm* pEm, u32 wep_no, int near);
void EmCatchSubSet(cEm* pEm, cEm* pSub, f32 pl_dir, u32 mode, f32 x, f32 y, f32 z, void (*ft)(cSubChar*));   // em_sub.cpp; PS2 order (the ang / mode swap is not visible in the bytes)
extern "C" {
void MotSetObj16(cObj* obj, void* mot, int a, int b);
int GetEm10EyeEffectEnable();
}

#endif
