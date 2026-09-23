#ifndef OBJ12_H
#define OBJ12_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Hanging object (game/obj12.cpp): the Ganado's sack / lantern, the door's locks and chain.
class cObj12 : public cObjUnion {
public:
    void setParent(cModel* pCoord, int parts, int flag);
    void setFall(Vec* pSpd, u8 type);
    void setFallSe(u8 se_id, u8 se_no, u8 em_id);
    void setBurn();
};

cObj12* SetObj12(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
