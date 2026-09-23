#ifndef OBJ16_H
#define OBJ16_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Enemy head object (game/obj16.cpp): the head / mouth model of the plaga-carrying enemies.
class cObj16 : public cObjUnion {
public:
    int ckAtkEnable();
    void setDamage();
    void setAtk(u8 a);
    void clearLostWait();
    void setMotData(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, void* i, void* j, void* k);
    void setLostWait(int a);
    void setBurn();
    void setPlDmgMot(void* mot, void* seq);
    void setDieEff();
    void setCritical();
    int ckAtkHit();
    void setScale(Vec* mag);
};

extern "C" {
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void MotSetObj16(cObj* obj, void* mot, int a, int b);
}

#endif
