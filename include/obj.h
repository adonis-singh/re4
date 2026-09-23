#ifndef OBJ_H
#define OBJ_H

#include "types.h"
#include "cManager.h"
#include "model.h"
#include "atariInfo.h"
#include "pendulum.h"
#include "main_mem.h"

class cModel;
struct EmAtkInfo;

// Per-object work layouts (game/obj03.cpp ...), all overlaid at cObj+0x328.
struct Obj03Work {
    u8 x0;        // 0x00
    u8 x1;        // 0x01
    u8 x2;        // 0x02
    u8 x3;        // 0x03
    f32 length;   // 0x04 path length
    f32 t;        // 0x08 current position on the path
    f32 speed;    // 0x0C
    u32 flags;    // 0x10 bit0: debug draw
    cModel* data; // 0x14  the path follower (PathGetMatEm pMod)
    void* path;   // 0x18
};

class cObj;

// Effect owner info at the head of every Efm work (esp_efm.cpp copies the caller's EspInfo,
// esp.h, into it; EfmDeleteSub matches flg / kind / pEm against g_Core_*).
struct EfmCore {
    u16 flg;              // 0x00
    u8 kind;              // 0x02
    u8 x3;                // 0x03
    u32 x4;               // 0x04
    cModel* pEm;          // 0x08
};

// Effect model work (game/obj04.cpp `Efm04`): a thrown/falling particle-like model.
struct Efm04Work {
    EfmCore core;         // 0x00
    f32 spdDamp;          // 0x0C  speed *= spdDamp every frame
    Vec acc;              // 0x10  added to speed every frame
    Vec rotSpd;           // 0x1C  added to rot every frame
    f32 scaleXZ;          // 0x28
    f32 scaleY;           // 0x2C
    f32 scale;            // 0x30  scale = scale + scaleSpd, scaleSpd *= scaleDamp
    f32 scaleSpd;         // 0x34
    f32 scaleDamp;        // 0x38
    u8 r0;                // 0x3C  start colour (EspGenWork x9C..x9F; PS2 OBJ04_FREE Col_start_r..a)
    u8 g0;                // 0x3D
    u8 b0;                // 0x3E
    u8 a0;                // 0x3F  alpha at the end of the fade-in
    f32 r;                // 0x40
    f32 g;                // 0x44
    f32 b;                // 0x48
    f32 a;                // 0x4C
    f32 rMul;             // 0x50  fade-out multipliers
    f32 gMul;             // 0x54
    f32 bMul;             // 0x58
    f32 aMul;             // 0x5C
    u16 fadeStart;        // 0x60
    u16 fadeLen;          // 0x62
    u16 moveStart;        // 0x64
    u16 scaleStart;       // 0x66
    u16 life;             // 0x68  0 = forever
    u16 frame;            // 0x6A
    cModel* parent;       // 0x6C  (esp_efm: the sequence's parent model)
    u32 parentSerial;     // 0x70
    cCoord* parentWorld;  // 0x74  pEffParentWorld when detached
    u8 rotFrame;          // 0x78  frame to re-orient along the parent (0xFF = never)
    u8 Parts_no;          // 0x79  parts of the parent the model follows (PS2 OBJ04_FREE Parts_no)
    u8 stopped;           // 0x7A  bit0: came to rest
    u8 Motion_no;         // 0x7B  EspGetEfmMotAddr motion (EspGenWork WorkSp8[2]) (PS2 OBJ04_FREE Motion_no)
    u32 flags;            // 0x7C  bit0: floor collision, bit1: scenario collision, bit3: MotionMove
    f32 groundOfs;        // 0x80
    Vec bounce;           // 0x84  x/z: horizontal, y: vertical rebound rate (EfmSetObj04: EspGenWork xE4 * 0.1; PS2 OBJ04_FREE RefRate)
};

// Effect model with loose parts (game/obj05.cpp `Efm05`): the obj04 scale / colour fade with the
// parts burst parameters; each parts keeps its own state in its cModel (efmStat / efmSpd / efmRotSpd).
struct Efm05Work {
    EfmCore core;         // 0x00
    Vec rotSpd;           // 0x0C  added to rot every frame
    f32 scaleXZ;          // 0x18
    f32 scaleY;           // 0x1C
    f32 scale;            // 0x20  scale = scale + scaleSpd, scaleSpd *= scaleDamp
    f32 scaleSpd;         // 0x24
    f32 scaleDamp;        // 0x28
    u8 r0;                // 0x2C  start colour (EspGenWork x9C..x9F; PS2 OBJ05_FREE Col_start_r..a)
    u8 g0;                // 0x2D
    u8 b0;                // 0x2E
    u8 a0;                // 0x2F  alpha at the end of the fade-in
    f32 r;                // 0x30
    f32 g;                // 0x34
    f32 b;                // 0x38
    f32 a;                // 0x3C
    f32 rMul;             // 0x40  fade-out multipliers
    f32 gMul;             // 0x44
    f32 bMul;             // 0x48
    f32 aMul;             // 0x4C
    u16 fadeStart;        // 0x50
    u16 fadeLen;          // 0x52
    u16 Pos_start_cnt;    // 0x54  (PS2 OBJ05_FREE Pos_start_cnt)
    u16 scaleStart;       // 0x56
    u16 life;             // 0x58  0 = forever
    u16 frame;            // 0x5A
    u32 flags;            // 0x5C  bit0: floor collision, bit1: scenario collision, bit3: parts tip over
    Vec center;           // 0x60  burst centre relative to pos (Efm05RotMatrix rotates it)
    u8 pow;               // 0x6C  burst speed (* 4096 / range)
    u8 rangeStep;         // 0x6D  burst range growth per frame (0xFF: everything at once)
    u8 rnd;               // 0x6E  speed random (/ 32)
    u8 rotAmp;            // 0x6F  rotation speed random (* 0.005)
    f32 grav;             // 0x70  added to the parts speed y
    f32 spdDamp;          // 0x74  parts speed *= spdDamp
    u32 groundOfs;        // 0x78
    Vec bounce;           // 0x7C  x/z: horizontal, y: vertical rebound rate (EfmSetObj05: EspGenWork xE4 * 0.1; PS2 OBJ05_FREE RefRate)
    u32 seed;             // 0x88  fRandSeed1_1 seed
};

// Rigid body effect model work (game/obj09.cpp, set up by esp_efm EfmSetObj09): a box of
// `size` with mass / moments of inertia, pushed by `spd` (momentum). Extends to cObj+0x3D8.
struct Efm09Work {
    EfmCore core;         // 0x00
    f32 mass;             // 0x0C  size.x * size.y * size.z / 1e9 * mass_mul
    Vec moment;           // 0x10  moments of inertia: moment_mul * mass * (size.y^2 + size.z^2) / 12, ... (obj09 dwdt; PS2 OBJ09_FREE Ig)
    u8 pad_1C[4];
    Vec pos;              // 0x20  = basePos at set up
    Vec basePos;          // 0x2C  EspGenWork x0C + random (y + 0.0001)
    Mtx mat;              // 0x38  identity at set up
    Vec spd;              // 0x68  EspGenWork x24 + random, * mass * 100 (obj09: velocity)
    Vec w;                // 0x74  0 at set up (obj09: world angular velocity, mat * rotSpd) (PS2 OBJ09_FREE w)
    Vec size;             // 0x80  EspGenWork xD8..xE0 * 100 + 250
    Vec force;            // 0x8C  force accumulated by AddForce, cleared every CalcVel
    Vec torque;           // 0x98  torque accumulated by AddForce
    Vec rotSpd;           // 0xA4  EspGenWork x70 + random (overlaps cObj attr / callBack): local angular velocity
};

// Obstacle model work (game/obj20.cpp `SetObaModel`).
struct ObaModelWork {
    u8 pad_0[0xC];
    Vec ofs;              // 0x0C  position relative to the parent (parts) matrix
    int partsNo;          // 0x18  parts of the parent followed by type 0
    cObj* parent;         // 0x1C
};

// Fading attachment work (game/obj26.cpp): scales toward `tgtScale`, then shrinks and fades out.
struct Obj26Work {
    u8 pad_0[8];
    cObj* parent;         // 0x08  followed parts 2 of this object
    u8 pad_C[0xC];
    Vec Scale;         // 0x18
};

// Floating island work (game/obj1c.cpp): drifts back toward its home position, plays crash
// motions and spawns effects while the player is on it.
struct IslandWork {
    u32 x00;              // 0x00
    u8 pad_4[8];
    int crashTimer;       // 0x0C  frames since setCrashBig (ckCrash)
    int estTimer;         // 0x10  frames until the next idle effect
    int crashEstWait;     // 0x14  frames the crash effect is suppressed
    Vec spd;              // 0x18  push speed (setCrashBig)
    Vec home;             // 0x24  position it drifts back to
    u8 espKind;           // 0x30  effect kind (EspPullCoreKind)
    u8 pad_31[3];
    void* motIdle;        // 0x34  motions: idle / crash, and their big-scale (>= 1.5) variants
    void* motCrash;       // 0x38
    void* motIdleBig;     // 0x3C
    void* motCrashBig;    // 0x40
};

// Thrown / shot object work (game/obj08.cpp): a projectile with gravity, scenario / enemy /
// player hit checks and up to four effect sets.
struct Obj08Work {
    u32 be_flag;            // 0x00  bit0 start motion, bit1 motion running, bit3 rotate, bit4 enemy hit check, bit5 player hit check
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rot_spd;           // 0x0C
    Vec spd;              // 0x18
    f32 gravity;             // 0x24
    f32 r;              // 0x28  hit radius (min 1.0)
    cModel* parent;       // 0x2C  thrower (its id goes to SndCall)
    int timer;             // 0x30  frames left (-1 = forever)
    EmAtkInfo* pAtk;      // 0x34  attack record for EmAtkHitCk
    u32 atkFlags;         // 0x38  low 16 bits: GetWepTargetList flag, low byte: damage kind
    u32 estNo[4];         // 0x3C  effects: 0 ?, 1 scenario hit / timeout, 2 floor hit, 3 enemy / player hit
    u32 estPrm[4];        // 0x4C
    u16 blk_no;            // 0x5C  hit SE block number (0xFFFF = none)
    u16 call_no;             // 0x5E
    u8 hit_type;           // 0x60  1: the enemy-hit effect follows the target instead of the hit point
};

// Hanging object work (game/obj00.cpp): follows a parts of its parent (`oya`) with a slerp
// blend, or falls as a three-point rope (obj00FallMove).
struct Obj00Work {
    u32 be_flag;            // 0x00  bit2: falling, bit3: blending toward the parent, bit5: fading out
    void* pMot;           // 0x04
    int motA;             // 0x08  MotionSetCore 4th argument
    u32 mot_attr;           // 0x0C  low 16 bits: MotionSetCore 6th argument
    cModel* pEm_oya;          // 0x10  parent
    int oya_parts;          // 0x14  parts of the parent to follow
    f32 oya_hokan;             // 0x18  blend rate (1.0 = parent matrix)
    f32 oya_hokan_add;          // 0x1C
    s16 fallSpd[3][3];    // 0x20  rope point speeds * 10
    u8 pad_32[2];
    Mtx hokan_mat;              // 0x34  previous parent matrix
    u8 fall_se_id;             // 0x64  landing SE
    u8 fall_se_no;              // 0x65
    u8 fall_em_id;              // 0x66
    u8 fall_se_ck;          // 0x67
};

// Hanging / thrown object work (game/obj12.cpp `cObj12`): the obj00 layout with a life counter,
// the landing SE moved to 0x68 and a rope `type` selecting the three rope offsets.
struct Obj12Work {
    u32 be_flag;            // 0x00  bit2: falling, bit3: blending toward the parent, bit7: keep the parent matrix, bit8: thrown, bit9: fading out after `life`
    void* pMot;           // 0x04
    int Motion_info;        // 0x08  MotionMove result of this frame
    u32 mot_attr;           // 0x0C
    cModel* pEm_oya;          // 0x10  parent
    int oya_parts;          // 0x14
    f32 oya_hokan;             // 0x18  blend rate (1.0 = parent matrix)
    f32 oya_hokan_add;          // 0x1C
    s16 fallSpd[3][3];    // 0x20  rope point speeds * 10 (fallSpd[0] is the throw speed)
    u8 pad_32[2];
    Mtx hokan_mat;              // 0x34  previous parent matrix
    int Lost_wait;             // 0x64  frames before the fade out
    u8 fall_se_id;             // 0x68  landing SE (0xFF = none)
    u8 fall_se_no;              // 0x69
    u8 fall_em_id;              // 0x6A
    u8 fall_se_ck;          // 0x6B
    u8 fall_type;              // 0x6C  rope offsets table index (setFall)
};

// Event object model type (PS2 OBJ18_TYPE): SetObj18 `type` / Obj18Work::type, from the model name prefix
// (event.cpp ExePacket_SetOm OmTbl).
enum OBJ18_TYPE {
    OBJ18_TYPE_OBMXX = 0,
    OBJ18_TYPE_LEON = 1,
    OBJ18_TYPE_ASHLEY = 2,
    OBJ18_TYPE_ADA = 3,
    OBJ18_TYPE_LUIS = 4,
    OBJ18_TYPE_PLXX = 5,
    OBJ18_TYPE_GANADO = 6,
    OBJ18_TYPE_TRADER = 7,
    OBJ18_TYPE_MAYOR1 = 8,
    OBJ18_TYPE_NO2 = 9,
    OBJ18_TYPE_SADDLER = 10,
    OBJ18_TYPE_ELGIGANTE = 11,
    OBJ18_TYPE_EMXX = 12,
    OBJ18_TYPE_EVXX = 13,
    OBJ18_TYPE_ETXX = 14,
    OBJ18_TYPE_SCRXX = 15,
    OBJ18_TYPE_WEPXX = 16,
    OBJ18_TYPE_EFFECT = 17,
    OBJ18_TYPE_MAYOR2 = 18,
    OBJ18_TYPE_INSECTBOSS0 = 19,
    OBJ18_TYPE_INSECTBOSS1 = 20,
    OBJ18_TYPE_INSECTBOSS0S = 21,
    OBJ18_TYPE_INSECTBOSS1S = 22,
    OBJ18_TYPE_ADA_SKIRT = 23,
    OBJ18_TYPE_NO3 = 24
};

// Event costume / cloth model work (game/obj18.cpp): a model that follows a parts of its parent
// (like obj00) and runs one of the cloth simulations by `type`.
struct Obj18Work {
    u32 be_flag;            // 0x00  bit3: blending toward the parent, bit6: cloth simulation off
    u8 pad_4[0xC];
    cModel* pEm_oya;          // 0x10  parent
    int oya_parts;          // 0x14
    f32 oya_hokan;             // 0x18  blend rate (1.0 = parent matrix)
    f32 oya_hokan_add;          // 0x1C
    u8 pad_20[0x14];
    Mtx hokan_mat;              // 0x34  previous parent matrix
    u32 x64;              // 0x64
    u32 obj18_type;             // 0x68  OBJ18_TYPE (SetObj18 type, cloth set)
    u32 CommonFlag;              // 0x6C  Obj18CmfSet/Get flag bits
    cObj* child;          // 0x70  ribbon / rope object created by SetObj18
    int ObjChainFlagCommon;              // 0x74  bit26 (0x04000000): event ControlTransFlag skips the child flags
    char NameMod[12];     // 0x78  event model name of the packet that created it (event ExePacket_SetOm; PS2 NameMod)
    u8 DebugFlag;         // 0x84
    u8 pad_85[3];
};

// Grenade work (game/obj01.cpp): flies under gravity, bounces off the scenario, can be held by a
// model (follows its parts) and explodes / lands in water after `life` frames.
struct Obj01Work {
    u32 be_flag;            // 0x00  bit0 start motion, bit1 motion running, bit2 water / bounce check, bit3 rotate parts 0
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rot_spd;           // 0x0C
    Vec spd;              // 0x18
    f32 gravity;             // 0x24
    f32 r;              // 0x28  bounce radius
    int timer;             // 0x2C  frames until the explosion (0 = now)
    cModel* hold;         // 0x30  model holding it (follows `holdParts`)
    int parts_no;        // 0x34
    Vec offset;          // 0x38
    Vec ang;          // 0x44
    int eff;           // 0x50  explosion effects (-1 = none: fade out instead)
    int est;          // 0x54
    int eff2;           // 0x58
    int est2;          // 0x5C
    int eff3;           // 0x60  water splash
    int est3;          // 0x64
    int eff4;           // 0x68  underwater explosion
    int est4;          // 0x6C
    u32 eff_action;             // 0x70  0 plain, 1 hand grenade, 2 incendiary, 3 flash, 4 ?
    int release_timer;        // 0x74  frames until it leaves the holder's hand
    u8 Bound_se_ck;            // 0x78  landing SE state
    u8 pad_79[3];
    u32 flag;          // 0x7C  bit3: water splash done
};

// Thrown weapon item work (game/obj10.cpp `cWepItem`): the grenade layout (Obj01Work) with the
// landing SE counters split out.
struct WepItemWork {
    u32 be_flag;            // 0x00  bit0 start motion, bit1 motion running, bit2 water / bounce check, bit3 rotate parts 0
    void* pMot;           // 0x04
    u8 pad_8[2];
    u16 motPrm;           // 0x0A
    Vec rot_spd;           // 0x0C
    Vec spd;              // 0x18
    f32 gravity;             // 0x24
    f32 r;              // 0x28  bounce radius
    int timer;             // 0x2C  frames until the explosion (0 = now)
    cModel* hold;         // 0x30  model holding it (follows `holdParts`)
    int parts_no;        // 0x34
    Vec offset;          // 0x38
    Vec ang;          // 0x44
    int eff;           // 0x50  explosion effects (-1 = none: fade out instead)
    int est;          // 0x54
    int eff2;           // 0x58
    int est2;          // 0x5C
    int eff3;           // 0x60  water splash
    int est3;          // 0x64
    int eff4;           // 0x68  underwater explosion
    int est4;          // 0x6C
    u32 eff_action;             // 0x70  0 plain, 1 water bomb, 2 explosive
    int release_timer;        // 0x74  frames until it leaves the holder's hand
    u8 Bound_se_ck;            // 0x78  bounce SEs left to play
    u8 se_count;             // 0x79  bounce SEs played
    u8 pad_7A[2];
    u32 flag;          // 0x7C  bit3: water splash done
};

// Gatling gun work (game/obj15.cpp `cObjGatling`): a mounted gun the player (or `ride`) fires
// at `target`; three cEmHit hit boxes take the damage, `eat` is its effect collision piece.
struct GatlingWork {
    u32 x00;              // 0x00
    int breakTimer;       // 0x04  frames of barrel spin-down after the break
    u8 pad_8[2];
    s16 cnt;              // 0x0A  frames since firing started (shots every 3rd frame after 30)
    u8 fire;              // 0x0C  setFire: start firing
    u8 firing;            // 0x0D
    u8 ammo;              // 0x0E  shots left (setReload: 40)
    u8 breakMode;         // 0x0F  0: weapon damage 0xD / 0x12 breaks it
    f32 rotY;             // 0x10  base yaw
    f32 maxRot;           // 0x14  yaw step limit (pi)
    int targetTimer;      // 0x18  frames until `target` reverts to the player
    u8 seOn;              // 0x1C  spin SE playing
    u8 pad_1D[3];
    u32 seHandle;         // 0x20
    class cSat* eat;      // 0x24
    class cEmHit* hit[3]; // 0x28
    u8 pad_34[8];
    class cEm* ride;      // 0x3C  enemy riding the gun
    class cModel* target; // 0x40  aimed-at model (player when NULL)
};

// Helicopter missile work (game/objMissile.cpp `cObjMissile`): hangs from a parts of the
// helicopter (setParent), then flies toward `target` (setFire) and explodes (objMissileBomb).
struct MissileWork {
    u32 Be_flg;              // 0x00
    int Timer;            // 0x04  fire wait / flight frames
    int Timer2;          // 0x08  frames before the hit checks start
    cModel* parent;       // 0x0C
    int oya_parts;          // 0x10
    int scale_mode;      // 0x14  keep the parent parts matrix as it is
    Vec Target;           // 0x18
    class cEmHit* pHit;    // 0x24
    u8 Target_ok;         // 0x28
    u8 pad_29[3];
    Vec Spd;              // 0x2C
};

// Gondola work (game/objGondola.cpp `cObjGondola`): a cable car the player / partner / up to
// five enemies ride; five scenario collision quads follow it.
struct GondolaWork {
    u32 Be_flg;              // 0x00
    int Timer;            // 0x04  break: frames before the sub motion starts
    u8 Ride_pl;            // 0x08  player is on board (ckRide)
    u8 pad_9[3];
    int Ride_sub;          // 0x0C  partner is on board
    int Act_wait;              // 0x10  counts down every frame
    Vec Spd;              // 0x14
    class cEm* pEm[5]; // 0x20
    class cSat* pSat[5];   // 0x34
    class cSat* pEat[5];  // 0x48
    struct MotionWork* subWork;  // 0x5C  sub (vibration / break) motion work (setSubMotion)
    void* Sub_mot1;         // 0x60  vibration motion (setVib)
    void* Sub_mot2;       // 0x64  break motion (R0_Break)
};

// Chain link work (game/obj1d.cpp): hangs between two parts of a parent model, fades out when
// the parent is lost.
struct ChainWork {
    u32 flags;            // 0x00  bit1: keep the parent parts matrices as they are (no axis normalize)
    int timer;            // 0x04  frames before the fade-out (LostWait)
    u8 pad_8[4];
    cModel* parent;       // 0x0C
    int parts1;           // 0x10
    int parts2;           // 0x14
    Vec ofs1;             // 0x18  offset in parts1
    Vec ofs2;             // 0x24  offset in parts2
    struct PenCloth* cloth;  // 0x30
};

// Player weapon object work (game/objWep.cpp `cObjWep`, a cObj subclass; see pl_wep.h).
struct ObjWepWork {
    void* motReset[2];    // 0x00 (0x328)  resetMotion idle motions: [0] normal, [1] empty magazine (pWepArc) (PS2 motReset[2])
    f32 bureX;            // 0x08 (0x330)  aim sway (lock random, pl_wep PlWepLockRand): pitch range, degrees -> radians in setAbility (PS2 bureX)
    f32 bureY;            // 0x0C (0x334)  yaw range (PS2 bureY)
    f32 bureSpeedX;       // 0x10 (0x338)  pitch step per frame (PS2 bureSpeedX)
    f32 bureSpeedY;       // 0x14 (0x33C)  yaw step per frame (PS2 bureSpeedY)
    u8 shotFrame[4];      // 0x18 (0x340)  fire motion shot frames, from each weapon's const table (ruger_tbl, xd9_tbl, ...) (PS2 shotFrame[4])
    u32 m_EraseTime;      // 0x1C (0x344)  (PS2 m_EraseTime; setEraseTime is not in the GC code)
    cModel* parent;       // 0x20 (0x348)  model the weapon hangs on (parentSet)
    u16 itemId;           // 0x24 (0x34C)  weapon item id (cObjLauncher::init: 0x35) (PS2 ITEM_ID itemId)
    u8 mode;              // 0x26 (0x34E)  0 stay, 1 ready, 2 fire, 3 down, 4 reload, 5 drop (move dispatch)
    u8 step;              // 0x27 (0x34F)  step inside the mode
    u8 disp;              // 0x28 (0x350)  bit0 draw the laser this frame, bit1 drawn last frame, bits 2-4 setDisp types 0/1/2
    u8 pad_29[3];
    u32 m_StopSeId;       // 0x2C (0x354)  SndCall handle stopped by resetMotion (PS2 m_StopSeId)
    Vec m_ShotPos;        // 0x30 (0x358)  laser sight end / hit marker position (pl_wep getMarkerPos, PlWepHitCheck2) (PS2 m_ShotPos)
    class cEm* m_SightEm; // 0x3C (0x364)  enemy the laser points at (GetWepTargetPos) (PS2 m_SightEm)
};

// Rocket launcher work (game/objRocket.cpp `cObjLauncher` : cObjWep).
struct LauncherWork {
    ObjWepWork wep;       // 0x00 .. 0x40
    u32 flags;            // 0x40 (0x368)  bit0: a rocket is in flight
    Vec from;             // 0x44 (0x36C)  launch line (getMarkerPos)
    Vec to;               // 0x50 (0x378)
    class cObjRocket* rocket;  // 0x5C (0x384)  loaded rocket (loadRocket)
};

// Bow work (wep28 module `cObjBow` : cObjWep).
struct BowWork {
    ObjWepWork wep;       // 0x00 .. 0x40
    class cObjWep* allow; // 0x40 (0x368)  the arrow object shown on the bow (cObjAllow, ObjMgr id 0x10)
};

// Rocket work (game/objRocket.cpp `cObjRocket`).
struct RocketWork {
    Vec oldPos;           // 0x00 (0x328)  position before this frame's motion (hit line start)
    int timer;            // 0x0C (0x334)  flight frames left (300)
};

// Spear work (game/obj1b.cpp `cObjSpear`): thrown (R1_Throw), stuck in a parts of the enemy it hit
// (R1_Parent), then falling as a three-point rope (R1_Fall) and fading out (R1_LostWait / Lost).
struct SpearWork {
    u32 flags;            // 0x00  bit0: keep the parent parts matrix as it is (no axis normalize)
    int timer;            // 0x04  LostWait: frames before the fade (120); Throw: frames between the flight SEs
    int timer2;           // 0x08  Throw: flight frames left (60)
    u8 pad_C[0xC];
    cModel* parent;       // 0x18
    int partsNo;          // 0x1C
    Vec spd[3];           // 0x20  rope point speeds (R1_Fall)
    Vec throwSpd;         // 0x44
    int x50;              // 0x50
    int parentTimer;      // 0x54  frames until the spear falls off its parent (1800)
    int estTimer;         // 0x58  frames of the stuck-in-boss effect (600, every 2nd frame)
    u8 seBlk;             // 0x5C  landing SE (0xFF = none)
    u8 seNo;              // 0x5D
    u8 seId;              // 0x5E
    u8 sePlayed;          // 0x5F
    u8 type;              // 0x60  rope offsets table row (R1_Fall)
    u8 se2Blk;            // 0x61
    u8 se2No;             // 0x62
    u8 se2Id;             // 0x63
    u8 se3Blk;            // 0x64
    u8 se3No;             // 0x65
    u8 se3Id;             // 0x66
    u8 throwSeBlk;        // 0x67  flight SE (0xFF = none)
    u8 throwSeNo;         // 0x68
    u8 throwSeId;         // 0x69
    u8 estNo;             // 0x6A  landing effect (0xFF = none)
    u8 estPrm;            // 0x6B
    u8 x6C;               // 0x6C
    u8 x6D;               // 0x6D
    u8 x6E;               // 0x6E
    u8 x6F;               // 0x6F
    u8 espId;             // 0x70  effect owner id deleted on landing (0x32)
};

// Giant robot statue work (game/objRobo.cpp `cObjRobo`): the Salazar statue that walks after the
// player over the bridge; two scenario / effect collision pieces per side, 14 hit boxes.
struct RoboWork {
    s8 r_no_0;           // 0x00  R0Tbl index
    s8 r_no_1;              // 0x01
    u8 pad_2[6];
    int pillar;           // 0x08  r226: index of the bridge pillar being pushed over (playerPillarDownCk)
    class cSat* pSat[2];   // 0x0C  scenario pieces (front / back)
    class cSat* pEat[2];   // 0x14  effect pieces
    class cEmHit* pEmHitTbl[14];  // 0x1C
    class cSat* pEatBody;     // 0x54  effect piece at the model position
    cObj* smd[2];         // 0x58  scroll objects following the feet (SetObjSmd)
    f32 FallSpdY;         // 0x60
    int FallTimer;              // 0x64
    int BridgeTimer[6];        // 0x68  frames each bridge piece has been hit
    f32 BridgeFallPos;            // 0x80
    int SndTimer;            // 0x84
};

// Player sub weapon work (game/objSubWep.cpp `cSubWep`: hand grenade / incendiary / flash / egg).
struct SubWepWork {
    u32 effNo;            // 0x00 (0x328)  landing effect (AtEffInfo pair by type; 0xD2 = none)
    u8 effPrm;            // 0x04 (0x32C)
    u8 pad_5[3];
    s32 attr;             // 0x08 (0x330)  AtEffInfo::flags of the hit (bit31 set when known, bit0: solid ground)
    u8 pad_C[8];
    Vec rotSpd;           // 0x14 (0x33C)
    Vec spd;              // 0x20 (0x348)
    f32 grav;             // 0x2C (0x354)
    f32 rad;              // 0x30 (0x358)  bounce radius
    int life;             // 0x34 (0x35C)  frames until the explosion (-1: only on impact)
    u8 pad_38[0x7C - 0x38];
    u8 x7C;               // 0x7C (0x3A4)  (ctor: 3)
    u8 seCnt0;            // 0x7D (0x3A5)  floor bounce SEs played
    u8 seCnt1;            // 0x7E (0x3A6)  wall bounce SEs played
    u8 flags;             // 0x7F (0x3A7)  bit0: explodes on the floor (fire / light), bit1: egg, bit4: hit a wall
};

// Bulldozer work (game/objBull.cpp `cObjBull`): the player / partner ride parts 2 through the
// break / lift / collision routines; one scenario piece (two while moving) and an effect piece.
struct BullWork {
    u32 Be_flg;            // 0x00  bit0 goal, bit1..4 break 1st..4th done, bit5 lift, bit6 truck go, bit7, bit8 lift wait
    int frame;            // 0x04  Collision: motion frames - 30
    u8 pad_8[4];
    void* Mot_tbl[12];        // 0x0C  setMotion table: 0 break1st/set, 1 to2nd, 2 break2nd, 3 to lift, 4 lift, 5 to3rd, 6 break3rd, 7 to4th, 8 break4th, 9..11 collision
    class cSat* pSat;      // 0x3C  scenario piece (type 1)
    class cSat* pSat2;     // 0x40  scenario piece (type 8) while moving
    class cSat* pEat;      // 0x44  effect piece (type 7)
    int Move_frame;              // 0x48  MotionMove calls of the current routine
    int Move_frame2;            // 0x4C  frames in the current routine (getMoveFrame*)
    u32 Move_point;              // 0x50  SetBull 5th argument
    u32 Start_point;             // 0x54  SetBull 5th argument: setRide start routine (0: break1st, 1: break2nd, 2: lift wait, 3: to3rd, 4: break3rd)
    u8 break1st;          // 0x58  break repeats left
    u8 break2nd;          // 0x59
    u8 break3rd;          // 0x5A
    u8 break4th;          // 0x5B
    void (*adjust_func)(cObj*);  // 0x5C  setAdjustMode: called before the player is carried along
    u8 Ride_mode;        // 0x60  0: the riders are not carried along
    u8 Ride_pl;              // 0x61  the player rides the bulldozer (setRide)
    u8 Act_ck;               // 0x62
    u8 Truck_down;        // 0x63  setBreakTruck: Collision continues with step 2
};

// Ladder work (game/obj13.cpp `cObjLadder`): a ladder the player / partner climbs (plobjLadderClimb),
// kicks down (plobjLadderDown) and stands up again (plobjLadderReset).
struct LadderWork {
    u32 flags;            // 0x00  bit0 motions set, bit1 off (setOff), bit2 partner climbing, bit3 transOld
    int status;           // 0x04  0 standing, 1 downed, 2 falling, 3 falling (timer done), 4 fall / reset motion
    int x08;              // 0x08
    u32 ladderNum;        // 0x0C  rungs (setLadderInfo; converted unsigned)
    u8 pad_10[4];
    Vec basePos;          // 0x14  position at SetLadder (R1_Set restores it)
    f32 baseRotY;         // 0x20
    int climbTimer;       // 0x24  frames the action button stays off after a climb (setClimb: 90)
    int resetReserve;     // 0x28  setResetReserve: 60
    int downTimer;        // 0x2C  setDown: 17 frames until status 3
    cObj* pair;           // 0x30  second ladder object sharing the collision flags
    int camera;           // 0x34  setCamera: camera cut of the climb (-1: the ladder cameras)
    void* mot[20];        // 0x38  setMotion table (player / partner climb, down, reset motions)
    u8 etcNo;             // 0x88  etc model number (GetEtcFlgPtr)
};

// Enemy head work (game/obj16.cpp `cObj16`): a head model hung on parts `partsNo` of `body` that
// looks at the player (obj16NeckMove), bites (R1_Atk / R1_Critical) and fades out when its
// enemies die.
struct Obj16Work {
    u32 Be_flag;            // 0x00  bit0: the lost-wait timer runs (setLostWait / clearLostWait)
    int Timer;            // 0x04  routine step timer
    int Timer2;         // 0x08  R1_Atk: attack frames left
    u8 pad_C[4];
    cModel* target;       // 0x10  enemy whose position / id the SEs use (SetObj16 3rd argument)
    cModel* body;         // 0x14  enemy the head is attached to (SetObj16 4th argument)
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
    int x6C;              // 0x6C
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

// Map object work (game/obj.cpp), sizeof 0x3D8: the cModel (0x320; motion work `mot` / `Motion.pMot`
// / `Motion.Seq_frame`.., `sub2B4.atari`, `sub2B4.pFsdTbl` are cModel members, see model.h), the
// scroll block and the per-object work area. Per-object modules keep their state in `work`.
class cObj : public cModel {
public:
    u8 pad_320[4];        // 0x320
    s32 blk;              // 0x324  scroll block the object belongs to (-2 free, -1 SetObjSmd)

    cObj();
    virtual ~cObj() {}
};

// One pool block per object: the manager's stride is the largest cObj subclass.
#define OBJ_WORK_SIZE 0x3D8

// Transitional: the per-class work structs overlaid on the bytes after cObj, for the subclasses
// that do not declare their own fields yet. A subclass moves off it once its fields are its own.
class cObjUnion : public cObj {
public:
    // 0x328: per-object work area (Efm09Work runs to the end of the object: attr / callBack are
    // inside the union so that they keep their offsets)
    union {
        u8 work[0x3D8 - 0x328];  // 0x328 per-object work area
        struct {
            u8 pad_work[0x3D0 - 0x328];
            u8 attr;              // 0x3D0  SMD object attribute byte (db_work "ATTR"): bit0 lit by attribute-4 lights, bit2 group
            u8 pad_3D1[3];
            void (*callBack)(cObj*);  // 0x3D4
        };
        Obj03Work obj03;
        Efm04Work efm04;
        Efm05Work efm05;
        Efm09Work efm09;
        ObaModelWork obaModel;
        Obj26Work obj26;
        IslandWork island;
        Obj08Work o8;
        ChainWork chain;
        Obj00Work o0;
        Obj12Work o12;
        Obj18Work o18;
        Obj01Work o1;
        WepItemWork wepItem;
        GatlingWork gatling;
        MissileWork missile;
        GondolaWork gondola;
        ObjWepWork wep;
        LauncherWork launcher;
        BowWork bow;
        RocketWork rocket;
        SpearWork spear;
        RoboWork robo;
        SubWepWork subWep;
        BullWork bull;
        LadderWork ladder;
        Obj16Work o16;
    };
};

class cObjMgr : public cManager<cObj> {
public:
    // Construct ids (PS2 cObjMgr::ID): the row of construct() / the class of the work; the GC switch ends at
    // ID_WEP_THERMO (0x3F), ID_GON..ID_PL_BATTERY are PS2 additions.
    enum ID {
        ID_NORMAL = 0,
        ID_MAGAZINE = 1,
        ID_SCROLL = 2,
        ID_03 = 3,
        ID_ESP = 4,
        ID_KABOOM = 5,
        ID_BOX = 6,
        ID_07 = 7,
        ID_MISSILE = 8,
        ID_ESP2 = 9,
        ID_WEP_ITEM = 10,
        ID_PL_WEAPON = 11,
        ID_0C = 12,
        ID_0D = 13,
        ID_0E = 14,
        ID_0F = 15,
        ID_WEP_ALLOW = 16,
        ID_WEP_BOW = 17,
        ID_EM12_WEAPON = 18,
        ID_LADDER = 19,
        ID_BELL = 20,
        ID_GATLING = 21,
        ID_EM10_PARASITE = 22,
        ID_22C_HITMARK = 23,
        ID_EVENT = 24,
        ID_ITEM = 25,
        ID_WEP_GRENADE = 26,
        ID_SPEAR = 27,
        ID_FLOATISLAND = 28,
        ID_CHAIN = 29,
        ID_LUIS_ITEM = 30,
        ID_PILLAR = 31,
        ID_OBAMODEL = 32,
        ID_WEP_RUGER = 33,
        ID_WEP_ROCKET = 34,
        ID_WEP_LAUNCHER = 35,
        ID_WEP_KNIFE = 36,
        ID_WEP_TOMPSON = 37,
        ID_EM2B_PARASITE = 38,
        ID_WEP_MAUSER = 39,
        ID_WEP_SNIPER = 40,
        ID_WEP_GRE_FIRE = 41,
        ID_WEP_GRE_LIGHT = 42,
        ID_WEP_SHOTGUN = 43,
        ID_WEP_MAGNUM = 44,
        ID_WEP_MACHINE = 45,
        ID_WEP_CIVILIAN = 46,
        ID_WEP_STRIKER = 47,
        ID_WEP_HKSNIPER = 48,
        ID_WEP_GOVERNMENT = 49,
        ID_WEP_FN57 = 50,
        ID_WEP_XD9 = 51,
        ID_WEP_VP70 = 52,
        ID_GONDOLA = 53,
        ID_WEP_MINE = 54,
        ID_ROBO = 55,
        ID_HELI_MISSILE = 56,
        ID_YAGURA = 57,
        ID_WEP_EGG = 58,
        ID_TROLLEY = 59,
        ID_WEP_HANDGRE = 60,
        ID_WEP_HAND = 61,
        ID_BULL = 62,
        ID_WEP_THERMO = 63,
        ID_GON = 64,
        ID_EM3F_TENTACLE = 65,
        ID_WEP_LASER = 66,
        ID_PL_BATTERY = 67,
        ID_NUM = 68
    };

    u32 Guid;

    cObjMgr();
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cObj* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cObj* pEm);
    virtual int construct(cObj* pSat, u32 room_no);   // calls the int overload (obj.cpp)
    int construct(cObj* pSat, ID id);                 // placement-new of the per-id class, or ObjInitFunc[id]
    void move();                              // dieCheck, then objMove on every live object
};

extern cObjMgr ObjMgr;

// Work `no` of ObjMgr, 0 when out of range. A free inline: an in-class one would be emitted out of
// line in obj.cpp (the unit owns cObjMgr's vtable), which the DOL does not have.
static inline cObj* ObjMgrWork(u32 no)
{
    if (no >= ObjMgr.nArray) {
        return 0;
    }
    return (cObj*)((u8*)ObjMgr.pArray + ObjMgr.size * no);
}

struct EspGenWork;
extern "C" {
// game/esp_efm.cpp: creates the obj04 / obj05 / obj09 effect model of a sequence record
// (`info` is the caller's EspInfo, esp.h). esp_sub.cpp EspSeqSet is the only caller.
cObj* EfmSeqSet(EspGenWork* gen, EfmCore* info, u32* seed, cModel* parent, Mtx m, int x, f32 rate, Vec* ofs);
// game/obj04.cpp / game/obj05.cpp: orient the model along `m`
void Efm04RotMatrix(cObj* pObj, Mtx pMat);
void Efm05RotMatrix(cObj* pObj, Mtx pMat);
}

#endif
