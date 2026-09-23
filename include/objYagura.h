#ifndef OBJYAGURA_H
#define OBJYAGURA_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Ladder / tower work (PS2 FREE_YAGURA, a shorter GC layout), in cObjYagura::free.
struct YaguraWork {
    u8 pad_0[0x20];
    void* Mot_vib;     // 0x20  vibration motion set by setVib()
};

// Ladder (yagura = tower): a static collision model that can play a vibration motion.
class cObjYagura : public cObj {
public:
    u8 free[OBJ_WORK_SIZE - 0x328];   // 0x328  YaguraWork

    virtual void move();

    void setMotionVib(void* mot);
    void setVib();
};

#define YAGURA_WK(o) ((YaguraWork*) (o)->free)

cObj* SetYagura(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
