// game/obj06: object id 6, plain box object cObjBox (D:/Bio4/Prog/obj06.cpp): a static model that
// only rebuilds its matrix from pos/ang/scale every frame.
#include "obj.h"
#include "obj06.h"
#include "math_sub.h"

// Nothing beyond cObj.
cObjBox::cObjBox()
{
}

// Rebuilds the matrix and parts from pos/ang/scale.
void cObjBox::move()
{
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    partsMatCalc();
    partsWorldCalc();
}
