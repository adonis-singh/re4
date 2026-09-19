#ifndef EMWINDOW_H
#define EMWINDOW_H

#include "types.h"
#include "vec.h"
#include "emobj.h"

// Work of the window / fence enemy (game/emwindow.cpp), overlaid on the EmObjWork from 0x3E0.
struct EmWindowWork {
    u8 pad_0[0x25C];
    u8 eff;               // 0x25C (0x63C)  EmObjWork::eff
    u8 pad_25D[3];
    void* arc;            // 0x260 (0x640)  room etc archive (GetEtcAddr source of the break model / fcv)
    u32 etcFlag[3];       // 0x264 (0x644)  bit array (bit0 = damage disabled, bit1/2 = fence kind 1/2 disabled, bit3 = broken)
    int fieldAt[2];       // 0x270 (0x650)  SceAtCreateFieldAt ids (-1 = none)
    u8 pad_278[0x2A8 - 0x278];
    int shake;            // 0x2A8 (0x688)  frames the window still shakes (SetShake: 10)
    u8 pad_2AC[4];
    Vec rotBase;          // 0x2B0 (0x690)  rot before the shake
    u8 floor;             // 0x2BC (0x69C)  CalFloor: 1 when there is no floor behind the window
    u8 breakDir;          // 0x2BD (0x69D)  ChkBreakDir result of the last event
};

#define EMWINDOW_WK(em) ((EmWindowWork*) (((cEmObj*) (em))->free))

// Breakable window / fence enemy (game/emwindow.cpp).
class cEmWindow : public cEmObj {
public:
    virtual void move();   // key function: the vtable stays in this unit (cEmMgr::construct stores it)

    int init(void* bin, void* tpl, Vec* pos, Vec* rot, int type, u8 etcNo, void* arc);
    void DmCk();
    int ExeWindowEvent();
    void CalFloor();
    u8 GetFloor();
    int ChkBreakDir(Vec* pos);
    int ChkStatus();        // etc flag word of this window (GetEtcFlgPtr), 0 when none; bit0 = broken
    void SetStatus(u16 f);
    int SetShake();
    int SetBreakAll(Vec* pos, int break_size, int breakType);
    int SetBreakModel();
    int SetChangeModel(void* bin, void* tpl);
    int SetAtariOff();
    int SetBreakEsp(int dir, int kind, int flag);
    void SetEnableDamage(int on);
    int ChkEnableDamage();
    void SetEtcFlag(u32 no, int on);
    int ChkEtcFlag(u32 no);
    int SetEnableFence(int on, int kind);
    int ChkEnableFence(int kind);
};

// bin/tpl of the window model, position / rotation, WindowData row `type` (Et*_init 4th
// argument), etc flag number, and the room archive the effect data comes from.
cEmWindow* SetWindow(void* bin, void* tpl, Vec* pos, Vec* rot, int type, u8 etcNo, void* arc);

extern "C" {
// Window in front of `m` (its field `id` from SceAtCheckFieldInfo(b)): 1 when `m` may go through it;
// `status` gets the etc flag word, `dir` the through direction, `pos` the window position.
int ChkWindow(cModel* m, Vec* pos0, Vec* pos1, int id, u16* status, Vec* dir, Vec* pos, cEmWindow** out);
}

#endif
