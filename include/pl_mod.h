#ifndef PL_MOD_H
#define PL_MOD_H

// Shared declarations of the player REL modules (files/em/plXX.rel, one unit src/plXX/plXX.cpp each):
// the per-costume / per-character player and partner classes and their helpers. Append only.

#include "types.h"
#include "vec.h"
#include "model.h"
#include "em.h"
#include "global.h"
#include "player.h"
#include "pl_body.h"
#include "pl_npc.h"
#include "pl_cloth.h"
#include "shape.h"

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp
extern void (*PlInitFunc)(cEm* em);   // game/em.cpp (the player modules' entry, EmCreate calls it for the player)
extern u8 pl_fs_tbl[];                // game/foot_shadow_tbl.cpp (incomplete type: full address, not @sda21)

// Motion / model data `no` of the partner's archive (cSubChar::subArc, read through pEm).
// PL_ARC(no) (the player archive) is in global.h, EM_ARC(w, no) (a work's own subArc) in em.h.
#define SUB_ARC(pl, no) PL_ARC_PTR((pl)->pEm->subArc, no)

// pl11 (pl15): Ashley in the knight armour, a cSubChar with its own model set (no cPlayer).
class cSubAshley : public cSubChar {
public:
    cSubAshley();
    // destructor implicit (synthesized: no vptr store)
    virtual void modelSet();
    virtual void setFace(int type);
    virtual void setHand(int no);
    // In-class on purpose: emitted after the synthesized destructor at the end of the unit.
    virtual void initCloth() { PlClothSetGirl(this, &girlHair, &girlSkirt, &girlSweater, 0); }
    virtual void moveCloth() { PlClothMoveGirl(this, &girlHair, &girlSkirt, &girlSweater); }
};

// pl06 (D:/Bio4/Prog/pl_hunk.cpp): HUNK, a cPlayer without cloth or face shapes.
class cPlHunk : public cPlayer {
public:
    cPlHunk();
    // destructor implicit (synthesized: ~cPlayer + the inlined ~cUnit)
    virtual void move();
    virtual void setModel();
    virtual void setMotion();
    virtual void setRightHand(int type);
    virtual void setLeftHand(u32 type);
    virtual void setFace(int type);
    virtual void setHead(int type);
    virtual void setHead(void* bin, void* tpl);
    // In-class on purpose: emitted after the synthesized destructor at the end of the unit.
    virtual void initCloth() {}
    virtual void moveCloth() {}
};

// pl02 (= pl0b, pl0c; real file name not in the binary): Ada, the Leon model set with her own hair / holster
// chains (costume 2) or no cloth (costume 1).
class cPlAda : public cPlayer {
public:
    cPlAda();
    // destructor implicit
    virtual void move();
    virtual void setModel();
    virtual void setMotion();
    virtual void setRightHand(int type);
    virtual void setLeftHand(u32 type);
    virtual void setFace(int type);
    virtual void setHead(int type);
    virtual void setHead(void* bin, void* tpl);
    virtual void initCloth();
    virtual void moveCloth();
};

void testHairMoveAda2(cModel* pl, PlCloth* c);
void testHolsterSetAda2(cModel* pl, PlCloth* c);
void testHolsterMoveAda2(cModel* pl, PlCloth* c);
void PlClothSetAda2(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair, int evt);
void PlClothMoveAda2(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair);
void PlClothSetAda3(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair, int evt);
void PlClothMoveAda3(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair);

// pl0a (D:/Bio4/Prog/pl_klauser.cpp): Krauser, the Leon model set plus the fading mutation models
// (krModel[3], em.h) and the X-button attack (pl_R1_KlauserAttack through cPlayer::pAuxFunc).
class cPlKlauser : public cPlayer {
public:
    cPlKlauser();
    // destructor implicit
    virtual void move();
    virtual int checkXbutton();
    virtual void setModel();
    virtual void setMotion();
    virtual void setRightHand(int type);
    virtual void setLeftHand(u32 type);
    virtual void setFace(int type);
    virtual void setHead(int type);
    virtual void setHead(void* bin, void* tpl);
    virtual void moveMatCalcBefore();
    // In-class on purpose: emitted after the synthesized destructor at the end of the unit.
    virtual void initCloth() {}
    virtual void moveCloth() {}
    void transMove();
};

extern "C" void setTexRender(cModelInfo* info);   // pl0a pl_klauser.cpp

// pl0d (D:/Bio4/Prog/pl_wesker.cpp): Wesker, the Leon model set with a MemAlloc'd jacket chain.
class cPlWesker : public cPlayer {
public:
    cPlWesker();
    // destructor implicit
    virtual void move();
    virtual void setModel();
    virtual void setMotion();
    virtual void setRightHand(int type);
    virtual void setLeftHand(u32 type);
    virtual void setFace(int type);
    virtual void setHead(int type);
    virtual void setHead(void* bin, void* tpl);
    virtual void initCloth();
    virtual void moveCloth();
};

void testJacketSetWesker(cModel* pl, PlCloth* c);
void testJacketMoveWesker(cModel* pl, PlCloth* c);
void PlClothSetWesker(cModel* pl, PlCloth* jacket);
void PlClothMoveWesker(cModel* pl, PlCloth* jacket);

#endif
