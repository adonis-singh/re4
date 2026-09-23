#ifndef OBJ02_H
#define OBJ02_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Scripted map object (SMD scroll object, ObjMgr id 2): per-type mover selected by `type`, optional
// motion and callback. The free area holds the mover's work (ObjScrRotWork / ObjScrSwingWork in
// obj02.cpp, the rooms' own views).
class cObjScr : public cObj {
public:
    u8 free[0x3D0 - 0x328];   // 0x328
    u8 attr;                  // 0x3D0  SMD object attribute byte (db_work "ATTR"): bit0 lit by attribute-4 lights, bit2 group (PS2 Attribute)
    u8 pad_3D1[3];
    void (*callBack)(cObj*);  // 0x3D4  (PS2 CallBackFunc)

    cObjScr();
    virtual void move();

    void moveNormal();
    void moveRotate();
    void moveSwingRot();
    void SetCallBack(void (*func)(cObj*));
    void SetSwingRot(f32 amp, f32 period, f32 phase);
};

#endif
