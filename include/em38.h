#ifndef EM38_H
#define EM38_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "camera.h"

class cCtrl;
class cModelInfo;
class cObj;
class cEm38;

// One parasite root of the lower body (em38RootInit / em38RootMove), 0x20 bytes: two parts swing
// while the tentacle hanging on them is out.
struct Em38Para {
    u8 state;             // 0x00  0 wait, 1 counting down, 2..3 rising (setIn at 30), 4..5 out, 6..7 sinking
    u8 x1;                // 0x01
    u8 parts;             // 0x02  root parts (its hit parts are parts + 1 / parts + 2)
    u8 parts2;            // 0x03
    u16 timer;            // 0x04  frames until the next rise
    s16 riseTimer;               // 0x06  rise / sink frames
    s16 swingAng;               // 0x08  swing step (0..359)  swing angle in degrees (0..359, 30-degree segments)
    s16 hp;               // 0x0A  damage left before the tentacle is killed (em38DmCk)
    s16 effTimer;         // 0x0C  frames until the next splash effect
    u8 pad_E[2];
    f32 phase;            // 0x10  scale wobble phase
    f32 ang;              // 0x14  swing angle
    f32 angSpd;           // 0x18  swing speed per frame
    cEm38* pEm;           // 0x1C  the tentacle enemy (em38SearchParts)
};

// Work of the em38 enemy (em38 module, D:/Bio4/Prog/em38.cpp), overlaid on cEm from 0x3E0. The boss
// is built from several enemies of one class: cModel::type 0 = the body, 1 / 2 = the tentacles, 3 = the
// upper body riding on it, 4 = the lower body (the parasite roots).
struct Em38Work {
    u32 flags;            // 0x000 (0x3E0)  bit0: route to the player found, bit1: partner present, bit2: targets the partner,
                          //                bit3: damage / die routine, bit4: routine running, bit5: head up (ckHeadUp), bit6: tentacle out (ckIn),
                          //                bit7: attack (ckCritical), bit8: second down done
    int timer;            // 0x004 (0x3E4)
    int timer2;           // 0x008 (0x3E8)
    int actVar;           // 0x00C (0x3EC)  Rnd() & 1: action button prompt variant
    u8 pad_10[0x10];
    YARARE_INFO hit[26];    // 0x020 (0x400)  extra hit boxes (YarareAdd / YarareAddCube)
    f32 routeAng;         // 0x568 (0x948)  Muku towards the route point (player)
    f32 routeAngAbs;      // 0x56C (0x94C)
    f32 subAng;           // 0x570 (0x950)  the same for the partner
    f32 subAngAbs;        // 0x574 (0x954)
    f32 targetAng;        // 0x578 (0x958)  copy of the chosen target's angle / distance
    f32 targetAngAbs;     // 0x57C (0x95C)
    f32 targetDist;       // 0x580 (0x960)
    Vec routePos;         // 0x584 (0x964)  RouteCkToPos result towards the player
    Vec subRoutePos;      // 0x590 (0x970)  towards the partner
    Vec targetPos;        // 0x59C (0x97C)  chosen target position
    cEm* pTarget;         // 0x5A8 (0x988)  pPL or pSUB
    cEm38* pBody;         // 0x5AC (0x98C)  the body enemy (em38SearchParts)
    cEm38* pTent[2];      // 0x5B0 (0x990)  the two tentacles
    cEm38* pUpper;        // 0x5B8 (0x998)  the upper body
    cEm38* pLower;        // 0x5BC (0x99C)  the lower body
    cModelInfo* pInfo;    // 0x5C0 (0x9A0)  extra model (type 0)
    Em38Para para[2];     // 0x5C4 (0x9A4)
    f32 eyeX;             // 0x604 (0x9E4)  eye pitch (em38EyeMove: parts 0x3A addRot)
    f32 eyeY;             // 0x608 (0x9E8)  eye yaw
    cObj* pWeak;          // 0x60C (0x9EC)  weak point object (em38WeakInit, obj00)
    int atkWait;          // 0x610 (0x9F0)  frames until the next attack (setAtkWait)
    int birthTimer;       // 0x614 (0x9F4)  frames until the next parasite (em38BirthParasite)
    int dmgCnt;           // 0x618 (0x9F8)  damage on the weak parts since the last down
    cCtrl* pCtrl11;       // 0x61C (0x9FC)  GetCtrlCtrl11()
    u16 seTimer;          // 0x620 (0xA00)  frames until the next voice
    u8 pad_622[2];
    int seWait;           // 0x624 (0xA04)  frames the ctrl11 SEs are held off
    u8 pad_628[4];
    s16 voiceWait;             // 0x62C (0xA0C)  frames until the body's first voice
    u8 pad_62E[2];
    int shellTimer;       // 0x630 (0xA10)  frames the shell stays open
    int waitCnt;          // 0x634 (0xA14)  em38_R1_Wait loops before the head goes up
    u8 mode;              // 0x638 (0xA18)  em38ShellControl step
    u8 atkHit;            // 0x639 (0xA19)  the attack hit the player
    u8 escaped;           // 0x63A (0xA1A)  the player escaped / is out of reach
    u8 espKind;           // 0x63B (0xA1B)  EspPullCoreKind at creation
    u8 espKind2;          // 0x63C (0xA1C)
    u8 pad_63D[3];
    f32 blendRate;        // 0x640 (0xA20)  em38BlendMotSet weight (-255..255)
    int blendA;           // 0x644 (0xA24)  interpolation frames left
    u32 blendB;           // 0x648 (0xA28)  frame counter of the blended motion
    void* mot[4];         // 0x64C (0xA2C)  motions blended by em38BlendMotSet
    u8 pad_65C[8];
    int blendKind;        // 0x664 (0xA44)  MotionSetCore flags of the blend
    MotionWorkSub blendMot;  // 0x668 (0xA48)  the blend motion work (cModel::blendMot)
    MotionWorkSub shellMot;  // 0x738 (0xB18)  the shell motion work (em38ShellControl)
    Camera cam;           // 0x808 (0xBE8)  event camera of the escape scenes (em38EscapeCamMove)
};

#define EM38_WK(em) ((Em38Work*) (((cEm38*) (em))->free))

class cEm38 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM38_WK)
    virtual void move();
    int ckHeadUp();
    void setAtkWait(int frames);
    void setIn();
    int ckIn();
    void setCritical();
    int ckCritical();
    virtual int ckDown();
};

void Em38Init(cEm* em);
void em38DmCk(cEm38* em);
void em38RouteCk(cEm38* em);
void em38EyeMove(cEm38* em);
void em38SearchParts(cEm38* em);
void em38UpperOnBody(cEm38* em);
void em38BirthParasite(cEm38* em);
void em38ShellControl(cEm38* em);
void em38BlendMotSet(cEm38* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int kind);
void em38BloodSet(cEm38* em);
void em38YarareInitBody(cEm38* em);
void em38YarareInitTentacle(cEm38* em);
void em38YarareInitUpper(cEm38* em);
void em38YarareInitLower(cEm38* em);
int em38SetDmVal(cEm38* em);
int em38AtkCk(cEm38* em, int no, int parts);
int em38AtkCk2(cEm38* em, u32 no, Vec* a, Vec* b);
void em38EscapeCamMove(cEm38* em);
void em38RootInit(cEm38* em);
void em38RootMove(cEm38* em);
void em38WeakInit(cEm38* em);
void em38WeakMove(cEm38* em);

#endif
