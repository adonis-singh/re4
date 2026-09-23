#ifndef OBJ19_H
#define OBJ19_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Item pickup model: a static model with a light set, placed once by setItemObj().
class cItemObj : public cObj {
public:
    Vec fall_spd;         // 0x328  (PS2 fall_spd; the GC item model is never moved)

    static const Vec zero;

    cItemObj();
    virtual void move();
};

#endif
