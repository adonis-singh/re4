#ifndef OBJ06_H
#define OBJ06_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Plain box object: rebuilds its matrix from pos/rot/scale every frame.
class cObjBox : public cObj {
public:
    cObjBox();
    virtual void move();
};

#endif
