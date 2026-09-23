#ifndef WEP_MOD_H
#define WEP_MOD_H

// Shared declarations of the weapon REL modules (files/em/wepXX.rel, one unit src/wepXX/wepXX.cpp
// each). Every module registers itself in _prolog: WeaponInitFunc (pl_wep.cpp calls it from
// cPlayer::init with the player), WeaponMoveFunc (player.cpp calls it every frame) and one
// ObjInitFunc slot (obj.cpp's cObjMgr::construct placement-constructs the module's weapon class
// through it). The weapon class derives from the DOL's cObjWep (pl_wep.h); its vtable and the cUnit
// linkonce copies are emitted by the module. Append only: the layouts below are shared by 37 units.

#include "types.h"
#include "atari.h"
#include "obj.h"
#include "pl_wep.h"
#include "player.h"
#include "global.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);

extern void (*ObjInitFunc[0x40])(cObj*);   // game/obj.cpp: per-id constructor table (cObjMgr::construct)
extern void (*WeaponMoveFunc)(cPlayer*);   // game/player.cpp: the equipped weapon's per-frame routine
// WeaponInitFunc (cModel*) is declared in pl_wep.h.

// Hand weapon (wep00 and the wep34..wep37 modules): the weapon object of the empty hand /
// event hand poses. keyKamae is the DOL's cObjHand::keyKamae (game/objWep.cpp), setMotion fills the
// player's motion table from the weapon archive in the module.
class cObjHand : public cObjWep {
public:
    virtual void setMotion(cPlayer* pEm);
    virtual int keyKamae();
};

// game/obj10.cpp: the ejected cartridge object (a thrown bullet-case model).
cObj* SetObj10(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags);
void Obj10SetEst(cObj* obj, int no0, int prm0, u32 type, int no1, int prm1, int no2, int prm2, int no3, int prm3);

// Collision flag bits changed through the info's address (`addi rX, obj, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariFlagsAnd(cAtariInfo* at, u16 mask) { at->m_flag &= mask; }
// wep17 ready00: the following pG load stays below the store and the info address is kept in a
// register (`addi rX, obj, 0x2b4; lhz/sth 0x1a(rX)`): only the volatile scalar access gives both.
static inline void AtariFlagsOr(cAtariInfo* at, u16 mask) { *(volatile u16*) &at->m_flag |= mask; }
// wep14 r2_down: the same for a cleared bit followed by a pG load (`addi 0x2b4; lhz/andi./sth; lwz pG`).
static inline void AtariFlagsAndV(cAtariInfo* at, u16 mask) { *(volatile u16*) &at->m_flag &= mask; }

// Machine gun (wep11 = TMP, wep29; wep/objMachinegun.cpp shared object; wep12 Thompson, wep27
// Klauser MG and wep39 carry their own copies of the class in the module object).
class cObjMachinegun : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* pMod);
    virtual void setMotion(cPlayer* pEm);

    void setCartridge();
};
// ObjMachinegun_init (wep/objMachinegun.cpp) is declared by wep11/wep29's entry: wep12/wep27/wep39 have
// their own static one.

// Thompson (wep12; wep12/objTompson.cpp): a machine gun variant without weapon types.
class cObjTompson : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* pMod);
    virtual void setMotion(cPlayer* pEm);

    void setCartridge();
};
void ObjTompson_init(cObj* obj);   // wep12/objTompson.cpp

// Semi-auto rifle (wep10 = own object wep10/objHkSniper.cpp; wep40 / wep47 carry a copy of the class
// in the module object). Routines: wep/pl_rifle.cpp.
class cObjHkSniper : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* pMod);
    virtual void setMotion(cPlayer* pEm);
};

// Knife (wep16 Leon's, wep26 Krauser's; wepXX/wepXX.cpp): a cObjWep whose own `init()` takes no
// parent (the vtable keeps cObjWep::init); the routines are the DOL's pl_knife.cpp (wep/pl_knife.cpp).
class cObjKnife : public cObjWep {
public:
    virtual void setMotion(cPlayer* pEm);

    void init();
};

// Krauser's bow (wep28; wep28/wep28.cpp): the arrow object (declared first: its vtable and destructor
// follow the bow's) and the bow, whose routines (wep/pl_bow.cpp) show/hide the arrow model.
class cObjAllow : public cObjWep {
public:
    virtual void moveFire();
    virtual void init(cModel* pMod);
    virtual void setMotion(cPlayer* pEm);
};

class cObjBow : public cObjWep {
public:
    cObjWep* pAllow;      // 0x368  the arrow object shown on the bow (cObjAllow, ObjMgr id 0x10)

    virtual void moveReady();
    virtual void moveFire();
    virtual void moveDown();
    virtual void init(cModel* pMod);
    virtual void setMotion(cPlayer* pEm);
    virtual void interrupt();
    virtual int keyKamae();

    void setDispAllow(int on);   // scale the arrow parts (parts 4) to 1 / 0
    void setAllow();             // shoot: SetMine arrow along the bow's line
};

#endif
