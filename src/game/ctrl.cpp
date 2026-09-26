// game/ctrl.cpp: the control work manager (CtrlMgr). A cCtrl is a small per-room helper object
// (0x214 bytes) with a virtual move / trans, specialised by id at construction: 0 / 1 light
// path controls, 0x10, 0x11 shared SE handles, 0x12 shared timers / counters / texture render
// targets, 0x14 the dragon head statue. CtrlMgr.move runs every live control each frame.

#include "types.h"
#include "cManager.h"
#include "ctrl.h"
#include "light.h"

// A cManager<cCtrl> pool (type 2).
cCtrlMgr::cCtrlMgr() : cManager<cCtrl>(sizeof(cCtrl), 2)
{
    setName("cCtrlMgr");
}

// Places the cCtrl subclass for `id` into the fresh work (unknown ids get the base class) and
// marks it live.
int cCtrlMgr::construct(cCtrl* pCtrl, u32 id)
{
    pCtrl->Id = id;
    switch (id) {
    case 0:
        new (pCtrl) cCtrl00;
        pCtrl->be_flag = 1;
        break;
    case 1:
        new (pCtrl) cCtrl01;
        pCtrl->be_flag = 1;
        break;
    case 0x10:
        new (pCtrl) cCtrl10;
        pCtrl->be_flag = 1;
        break;
    case 0x11:
        new (pCtrl) cCtrl11;
        pCtrl->be_flag = 1;
        break;
    case 0x12:
        new (pCtrl) cCtrl12;
        pCtrl->be_flag = 1;
        break;
    case 0x14:
        new (pCtrl) cCtrl14;
        pCtrl->be_flag = 1;
        break;
    default:
        new (pCtrl) cCtrl;
        pCtrl->be_flag = 1;
        break;
    }
    return 1;
}

// Per-frame: dieCheck, then move() on every live control.
void cCtrlMgr::move()
{
    u32 i;

    dieCheck();
    for (i = 0; i < nArray; i++) {
        cCtrl* p = fastAt(i);
        if (p->isAlive()) {
            p->move();
        }
    }
}

// Draw registration: trans() on every live control.
int cCtrlMgr::trans()
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        cCtrl* p = fastAt(i);
        if (p->isAlive()) {
            p->trans();
        }
    }
    return 1;
}

// Base control: nothing per frame.
void cCtrl::move()
{
}

// Base control: nothing to draw.
void cCtrl::trans()
{
}

cCtrlMgr CtrlMgr;
