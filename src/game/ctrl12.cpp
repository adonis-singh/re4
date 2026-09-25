// game/ctrl12.cpp: control 0x12, shared room state for the enemy modules: 13 countdown timers,
// 6 counters, a per-frame flag word and the texture render targets (TexRenderMng) the em2b /
// em2c / em32 bosses draw their special textures with.

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "atari.h"
#include "light.h"
#include "TexRender.h"
#include "esp.h"

// Per-frame: counts the timers down and clears the frame flags.
void cCtrl12::move()
{
    Ctrl12Work* w = (Ctrl12Work*) work;
    int i;
    u32 j;

    for (i = 0; i < 13; i++) {
        if (w->timer[i] != 0) {
            w->timer[i]--;
        }
    }
    for (j = 0; j < 1; j++) {
        w->flag[j] = 0;
    }
}

// The room's single ctrl12 (created at the back of the pool on first use); NULL when full.
cCtrl* GetCtrlCtrl12()
{
    cCtrl* c;
    u32 i;
    u32 n = CtrlMgr.getArrayNum();

    for (i = 0; i < n; i++) {
        c = CtrlMgr.at(i);
        if (c->isAlive() && c->Id == 0x12) {
            return c;
        }
    }
    c = CtrlMgr.createBack(0x12);
    if (c == 0) {
        return 0;
    }
    return c;
}

// Sets timer `idx` (0..12) to `val` frames.
void Ctrl12Set(cCtrl* pCtrl, int idx, s16 val)
{
    Ctrl12Work* w;

    if (pCtrl == 0) {
        return;
    }
    if (pCtrl->Id != 0x12) {
        return;
    }
    if (idx > 12) {
        return;
    }
    w = (Ctrl12Work*) pCtrl->work;
    w->timer[idx] = val;
}

// 1 while timer `idx` is running.
int Ctrl12Ck(cCtrl* pCtrl, int id)
{
    Ctrl12Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    if (id > 12) {
        return 0;
    }
    w = (Ctrl12Work*) pCtrl->work;
    if (w->timer[id] != 0) {
        return 1;
    }
    return 0;
}

// Adds `add` to counter `idx` (0..5), saturating at 0xFFFF.
void Ctrl12CntAdd(cCtrl* pCtrl, int id, int add)
{
    Ctrl12Work* w;
    u16 v;

    if (pCtrl == 0) {
        return;
    }
    if (pCtrl->Id != 0x12) {
        return;
    }
    if (id > 5) {
        return;
    }
    w = (Ctrl12Work*) pCtrl->work;
    v = w->cnt[id];
    w->cnt[id] = v + add;
}

// 1 when counter `idx` has reached `val`.
int Ctrl12CntCk(cCtrl* pCtrl, int id, u16 over)
{
    Ctrl12Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    if (id > 5) {
        return 0;
    }
    w = (Ctrl12Work*) pCtrl->work;
    return w->cnt[id] >= over;
}

// The em2b (El Gigante) texture render target, allocated on first use together with its est
// (owner 1, est 0x42) that renders into it.
TexRenderMng* Ctrl12GetTexRenderEm2b(cCtrl* pCtrl)
{
    Ctrl12Work* w;
    TexRenderMng* t;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    w = (Ctrl12Work*) pCtrl->work;
    t = w->tex2b;
    if (t == 0) {
        GetTexRenderMgr(&w->tex2b);
        if (w->tex2b != 0) {
            EstSet(0, -1, 0, 0, EFF_ROOM, 0x42, w->tex2b->GetCoreFlg() | 1, ESP_CORE_KIND_NONE, t, t);
        }
    }
    return w->tex2b;
}

// The em2c texture render target, allocated on first use.
TexRenderMng* Ctrl12GetTexRenderEm2c(cCtrl* pCtrl)
{
    Ctrl12Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    w = (Ctrl12Work*) pCtrl->work;
    if (w->tex2c == 0) {
        GetTexRenderMgr(&w->tex2c);
    }
    return w->tex2c;
}

// The em32 (U3) texture render target, allocated on first use.
TexRenderMng* Ctrl12GetTexRenderEm32(cCtrl* pCtrl)
{
    Ctrl12Work* w;

    if (pCtrl == 0) {
        return 0;
    }
    if (pCtrl->Id != 0x12) {
        return 0;
    }
    w = (Ctrl12Work*) pCtrl->work;
    if (w->tex32 == 0) {
        GetTexRenderMgr(&w->tex32);
    }
    return w->tex32;
}
