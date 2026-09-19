#ifndef EM_WRAP_H
#define EM_WRAP_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Room-script enemy handle (src/st/em_wrap.cpp, the first object of every stage REL). A cEmWrap names
// an enemy of the room's list (ESL entry `no`) and checks it is still alive before every access; the
// members are real out-of-line functions (the rooms `bl` them; the original REL link dead-stripped the
// unused ones per module, see config/G4BE08/modules.py). The `*Goto/*FindPL/*Reset*` members forward
// to the Ganado's virtuals (cEmGanado below = the cEm10 slots this file needs).
class cEmWrap {
public:
    cEm* pEm;    // 0x0  the enemy, NULL until set
    s16 no;      // 0x4  enemy list entry (-1 for a bare pointer)
    s8 list;     // 0x6  enemy list number the entry belongs to (-1: any)
    u8 alive;    // 0x7  1 once pEm was set
    int errOn;   // 0x8  1: report failures through pLog

    cEmWrap();
    ~cEmWrap() {}   // trivial: only the array destructor loop of the rooms' local cEmWrap arrays (r311)
    void initWork();
    void err(const char* msg, int no);
    int setEm(s16 no, s8 list, int errOn, int chkDead, int setAlive);
    int setPtr(s16 no, s8 list, int errOn);
    int setPtr(cEm* em, int errOn);
    cEm* getPtr();
    int isAlive();
    int isActive();
    void destroy();
    void setTrans(int on);
    int isTrans();
    void setMove(int on);
    int isMove();
    void setBeFlag(u32 bit, int on);
    int isBeFlag(u32 bit);
    int isDamage();
    void setNoSuspend(int on);
    int isNoSuspend();
    void setRno(u8 r0, u8 r1);
    int ckRno01(int r0, int r1);
    void setHp(s16 hp);
    s16 getHp();
    s16 getHpMax();
    u8 Character();
    void setCharacter(u8 c);
    f32 getGuard_r();
    void setGuard_r(f32 r);
    int checkStatus(int stat);
    void setPos(Vec* pos);
    void setAng(Vec* ang);
    void setSca(Vec* sca);
    void setFlag(u32 bit);
    int ckFlag(u32 bit);
    void getPos(Vec* pos);
    f32 getPosX();
    f32 getPosY();
    f32 getPosZ();
    void getAng(Vec* ang);
    f32 getAngX();
    f32 getAngY();
    f32 getAngZ();
    void motionSet(void* data, int a, int b, int c, int d);
    void motionMove();
    void motionPause(int on);
    void addModel(cModelInfo* info);
    void setParent(cModel* parent);
    void beginEvent();
    void endEvent();
    void matCalc();
    int isNormalGanade();
    void setGotoSwitch(Vec* pos, int mode, void* sw);
    void setGoto(Vec* pos, int mode);
    int ckGoto();
    int ckResetEnable();
    void setReset();
    void setReset(int a, int b);
    int ckFindPL();
    void setFindPL();
    f32 get_l_pl();
    void clearFindPL();
    int ckBowgunFire();
    int ckParasite();
};

// The Ganado (cEm10, em10 modules) by vtable slot: only the virtuals cEmWrap forwards to, declared and
// never defined, so no vtable is emitted here (em10.h is the em10 library's header).
class cEmGanado : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMGANADO_WK)
    virtual void setHand(int no);                            // 0x50
    virtual void setWeaponFall();                            // 0x58
    virtual int ckFindPL();                                  // 0x60
    virtual void setFindPL();                                // 0x68
    virtual void clearFindPL();                              // 0x70
    virtual int ckParasite();                                // 0x78
    virtual int ckGoto();                                    // 0x80
    virtual void setGoto(Vec* pos, int mode);                // 0x88
    virtual void setGotoSwitch(Vec* pos, int mode, void* sw); // 0x90
    virtual int ckResetEnable();                             // 0x98
    virtual void setReset();                                 // 0xA0
    virtual void chgSet(u8 no);
    virtual void setEvtMotion(void* m0, void* m1, void* m2, void* m3);
    virtual void setGondolaMotion(void* m0, void* m1, void* m2, void* m3);
    virtual void setR11DMotion(void* m0);
    virtual void setDrill(void* m0, void* m1, void* m2, void* m3);
    virtual void setGatling(void* g, void* m0, void* m1, void* m2, void* m3);
    virtual void setGatlingMode(u8 mode);
    virtual int ckBombFire();
    virtual int ckShiled();
    virtual int ckBowgunFire();                              // 0xF0
    virtual void setSwitch(cModel* sw);                      // 0xF8
    virtual void setLost();                                  // 0x100
    virtual void setWeapon(void* bin, void* tpl, int type);  // 0x108
    virtual int ckWeapon();                                  // 0x110
    virtual int ckTakeAway();                                // 0x118
    virtual void setUFOCatch(void* m0, void* m1);            // 0x120
    virtual int ckR305BomberEnable();                        // 0x128
};

// Enemy 0x2D (the dog): its own ckFindPL/ckResetEnable/setReset slots.
class cEmDog : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMDOG_WK)
    virtual int ckFindPL();            // 0x50
    virtual int ckResetEnable();       // 0x58
    virtual void setReset(int a, int b); // 0x60
};

// Enemy 0x36: ckFindPL/setFindPL slots.
class cEm36 : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EM36_WK)
    virtual void v50();
    virtual int ckFindPL();            // 0x58
    virtual void setFindPL();          // 0x60
};

// Scripted enemy control (cEmControl: a handle plus up to 16 way points), the patrol task and the
// guard task built on it.
struct EmControlPoint {
    Vec pos;
    int mode;                   // 0xC   setGoto mode of the point (the st3 route tables; 0 in the Vec tables)
};

class cEmControl {
public:
    int nPoint;                 // 0x0   way points (<= 15)
    int cur;                    // 0x4   current way point
    int prev;                   // 0x8   previous way point
    EmControlPoint point[16];   // 0xC
    cEmWrap em;                 // 0x10C
    int active;                 // 0x118

    int SetControl(s16 no, Vec* tbl, int n, int errOn);
    int SetControl(s16 no, EmControlPoint* tbl, int n, int errOn);  // st3 revision (src/st/em_wrap_v3.cpp)
    void SetTargetPos(Vec* tbl, int n);
    void SetTargetTbl(EmControlPoint* tbl, int n);                   // st3 revision
    void EndControl();
};

class cEmPatrol : public cEmControl {
public:
    int SetPatrol(s16 no, Vec* tbl, int n, u8 prio, int errOn);
    static void TaskMove(cEmPatrol* p);
};

// st3 revision only: run the way points once (setGoto mode 1, the last with mode 0xB), or run them with
// each point's own mode (the table's mode field), ending after the last point.
class cEmRouteRun : public cEmControl {
public:
    int SetRouteRun(s16 no, Vec* tbl, int n, u8 prio, int errOn);
    static void TaskMove(cEmRouteRun* p);
};

class cEmRouteExec : public cEmControl {
public:
    int SetRouteExec(s16 no, EmControlPoint* tbl, int n, u8 prio, int errOn);
    static void TaskMove(cEmRouteExec* p);
};

class cEmGuard : public cEmControl {
public:
    f32 ang;                    // 0x11C  facing angle while guarding
    int step;                   // 0x120
    f32 guard_r;                // 0x124  the enemy's guard radius on entry
    int alerted;                // 0x128
    int pad_12C;
    int x130;
    int (*check)(cEmWrap* em);  // 0x134  alert condition

    int SetGuard(s16 no, Vec* tbl, int n, int (*check)(cEmWrap*), f32 ang, u8 prio, int errOn);
    static void TaskMove(cEmGuard* g);
};

// Bare enemy pointer of list entry `no` (NULL when it cannot be set).
cEm* setEm(s16 no, s8 list, int errOn, int chkDead, int setAlive);
// 1 when any alive Ganado has found the player; `dist` (optional) receives the nearest one's distance.
int SceCkFindPL(f32* dist);

#endif
