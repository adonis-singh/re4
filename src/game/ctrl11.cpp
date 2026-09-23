// game/ctrl11.cpp: control 0x11, a shared table of sound handles with cooldown timers (15 slots
// plus the em38 voice slot). Enemies that share one voice / SE budget (the ganado crowd, em38)
// go through it so a sound is not restarted before its slot's timer ran out.

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "model.h"
#include "snd.h"

// Per-frame: counts every slot's cooldown down.
void cCtrl11::move()
{
    Ctrl11Work* w = (Ctrl11Work*) work;
    int i;

    for (i = 0; i < 15; i++) {
        if (w->Se_wait[i] != 0) {
            w->Se_wait[i]--;
        }
    }
}

// The room's single ctrl11 (created at the back of the pool on first use); NULL when the pool
// is full.
cCtrl* GetCtrlCtrl11()
{
    cCtrl* c;
    u32 i;
    u32 n = CtrlMgr.getArrayNum();

    for (i = 0; i < n; i++) {
        c = CtrlMgr.at(i);
        if (c->isAlive() && c->Id == 0x11) {
            return c;
        }
    }
    c = CtrlMgr.createBack(0x11);
    if (c == 0) {
        return 0;
    }
    return c;
}

// Plays SE `no` (block 8) at the model for slot `idx` unless the slot is still cooling down;
// sets the cooldown to `time` frames. Returns the SndCall handle (0 = not played).
u32 Ctrl11SetSe(cCtrl* pCtrl, cModel* m, s16 time, u16 no, int idx)
{
    Ctrl11Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x11) {
        return 0;
    }
    w = (Ctrl11Work*) pCtrl->work;
    if (w->Se_wait[idx] != 0) {
        return 0;
    }
    w->Se_id[idx] = SndCall(8, no, &m->pos, m->id, 0, m);
    w->Se_wait[idx] = time;
    return w->Se_id[idx];
}

// Same at the model's parts 0 with an explicit SE block, without the cooldown test.
u32 Ctrl11SetSe2(cCtrl* pCtrl, cModel* m, s16 time, u16 no, int idx, u16 blk)
{
    Ctrl11Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x11) {
        return 0;
    }
    w = (Ctrl11Work*) pCtrl->work;
    w->Se_id[idx] = SndCall(blk, no, &m->getPartsPtr(0)->world, m->id, 0, m);
    w->Se_wait[idx] = time;
    return w->Se_id[idx];
}

// Stops the slot's current sound and plays `no` (block 8) at parts 0, cooldown `time`.
u32 Ctrl11StopAndSetSe(cCtrl* pCtrl, cModel* m, s16 time, u16 no, int idx)
{
    Ctrl11Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x11) {
        return 0;
    }
    w = (Ctrl11Work*) pCtrl->work;
    SndStop(w->Se_id[idx], 0);
    w->Se_id[idx] = SndCall(8, no, &m->getPartsPtr(0)->world, m->id, 0, m);
    w->Se_wait[idx] = time;
    return w->Se_id[idx];
}

// The em38 voice slot: stops the previous voice and plays `no` at parts 0.
u32 Ctrl11SetSeEm38(cCtrl* pCtrl, cModel* pEm, u16 se)
{
    Ctrl11Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x11) {
        return 0;
    }
    w = (Ctrl11Work*) pCtrl->work;
    SndStop(w->Se_id_em38, 0);
    w->Se_id_em38 = SndCall(8, se, &pEm->getPartsPtr(0)->world, pEm->id, 0, pEm);
    return w->Se_id_em38;
}
