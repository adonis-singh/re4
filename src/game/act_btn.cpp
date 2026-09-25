// game/act_btn.cpp: the action button prompt (ActBtn). Game code offers actions each frame with
// set() (message kind, button, callback); move() shows the highest-priority prompt through the
// message system / cockpit and, when the player presses the button, runs its callback directly,
// as a scenario task or as a scenario area action. Prompts are one-frame: everything is cleared
// again at the end of move().

#include "atari.h"
#include "act_btn.h"
#include "global.h"
#include "libgpu.h"
#include "cockpit.h"
#include "mes.h"
#include "id_sys.h"
#include "main.h"
#include "player.h"
#include "pl_sub.h"
#include "sce_sys.h"
#include "sce_at.h"

cActionButton ActBtn;

typedef void (*ActBtnFunc)(void* arg, int d);

// Clears the prompt table and this frame's flags; remembers whether prompts are disabled
// (Stop_flg 0x100).
void cActionButton::init()
{
    int stop;

    ClearOTagR(m_ot, 16);
    m_num = 0;
    StaFlagOff(pG, STA_PL_ACTION);
    StaFlagOff(pG, STA_ACT_DONT_FIRE);
    stop = 1;
    if (!SpfFlagChk(pG, SPF_ACTBTN)) {
        stop = 0;
    }
    this->m_stop_flag_old = stop;
}

// Per-frame: walks the prompts from the highest slot, skips those the player state refuses,
// shows the first valid one (unless Disp_flg 0x1000 hides prompts or flags bit3) and, when its
// button test passes, runs the action (type 0 call, 1 SceExec task, 2 scenario area action
// with its exec flag / one-shot disable); then clears the table.
void cActionButton::move()
{
    u32 tag;
    ActBtnWork* w;

    Cckpt.m_ActBttn.setDispFlag(0);
    m_active_flag = 0;
    if (SpfFlagChk(pG, SPF_ACTBTN) || (StaFlagChk(pG, STA_DIEDEMO)) || m_stop_flag_old) {
        init();
        return;
    }
    for (tag = m_ot[15]; tag != 0xFFFFFFFF; tag = w->tag) {
        w = (ActBtnWork*) (tag | 0x80000000);
        if ((s32) tag >= 0) {
            continue;
        }
        if (w->flags & ACTCTR_NO_EXEC) {
            break;
        }
        if (checkPLStatus(w) == 0) {
            continue;
        }
        m_active_flag = 1;
        if (!DpfFlagChk(pG, DPF_ACTBTN) && !(w->flags & ACTCTR_NO_DISP)) {
            disp(w);
        }
        if (checkButton(w) == 1 && w->func != 0) {
            int flag = 1;

            if (w->flags & ACTCTR_NO_SUSPEND) {
                flag = 2;
            }
            switch (w->type) {
            case ACT_FUNC_NORMAL:
                ((ActBtnFunc) w->func)(w->arg, w->d);
                break;
            case ACT_FUNC_SCE:
                SceExec(0x12, (TaskFunc) w->func, (int) w->arg, flag, w->slot, (void*) w->d);
                break;
            case ACT_FUNC_SCE_AT: {
                SceAtWork* at = (SceAtWork*) w->arg;

                SceAtSetExecFlg(at->no);
                ((ActBtnFunc) w->func)(w->arg, w->d);
                if (at->trigger & 0x80) {
                    SceAtSetEnable(at->no, 0);
                }
                break;
            }
            }
        }
        break;
    }
    init();
}

// Shows the prompt: the message (kind + 0x16) at the layout position for the button icon, and
// tells the cockpit which button icon to draw.
void cActionButton::disp(ActBtnWork* work)
{
    int col = 0;
    u8 kind = work->kind;
    u8 btn = work->btn;
    IdUnit* u;
    s16 sx;
    s16 sy;
    int x;
    // COMPILER-DIFF: candidate (local-alloc order): `fontH / 2` is computed in the MesSet argument
    // register r6 in the target; here the third fpmem-address scratch copy (`mr r6,r10`) takes r6 first.
    register int y asm("r6");

    if (work->flags & ACTCTR_DOOR_COLOR) {
        col = 7;
    }
    cMes.setLayout(1, LAYOUT_ACT_BTN);
    switch (btn) {
    case DISP_A_NORMAL:
    case DISP_X:
    case DISP_ROTATE:
    case DISP_STICK_A:
    case DISP_STICK_UP:
        u = IdSys.unitPtr(1, IDC_ACT_BUTTON);
        sx = (s16) u->pos0.x;
        sy = (s16) u->pos0.y;
        x = (s16) (((f32) sx + 320.0f) * 0.8f);
        y = cMes.getFontHeight(1) / 2;
        y = (s16) ((240.0f - (f32) sy) * 0.8f) - y;
        cMes.MesSet(kind + 0x16, x, (s16) y, 0x200F1, 1, col, 4);
        break;
    default:
        u = IdSys.unitPtr(0xF0, IDC_ACT_BUTTON);
        sx = (s16) u->pos0.x;
        sy = (s16) u->pos0.y;
        x = (s16) (((f32) sx + 320.0f) * 0.8f);
        y = cMes.getFontHeight(1) / 2;
        y = (s16) ((240.0f - (f32) sy) * 0.8f) - y;
        cMes.MesSet(kind + 0x16, x, (s16) y, 0xF1, 1, col, 4);
        break;
    }
    Cckpt.m_ActBttn.setDispFlag(btn);
}

// 1 when the prompt's button is pressed this frame: by button kind, trigger or hold (flags
// bit4), honouring the exclusive (bit6) and no-trigger (bit1) flags and the "button already
// consumed" bit Status_flg[0] 0x4000.
// Every failing test `break`s to the one `return 0` after the switch (a plain `return 0` in a two-way
// leaf gets its `li r3,0` hoisted into a conditional return by jump1; a jump to the shared block does
// not), and the `(u64) key & ~mask` test is written in each leaf (jump2 cross-jumps the two `!(flags &
// 2)` copies into the first one, `mr r10,rX; b`). The `register u64 key asm("r9")` pin fixes the DI pair
// order. Cases 9/0xA: separate case nodes (the target compares 9 and 0xA individually) that reach the ONE
// `li r3,0; blr` block; `case 9: return 0; case 0xA: return 0;` gives a second block (its `set r3 0;
// (return)` cannot cross-jump with the end block, whose `(use r3)` sits between the set and the return),
// and a `break` pair is grouped into a range node. The case-9 arm ends in a codeless `asm volatile("")`:
// a real insn in the arm keeps the nodes separate, and it also blocks jump2's `x = a; if (c) goto l;`
// hoist that turns the case-7 tail into `or.; li r3,1; beqlr` when the `li r3,0` block is adjacent.
int cActionButton::checkButton(ActBtnWork* work)
{
    u32 on = Key.on & 0x00CF0000;
    u32 trg = Key.trg & 0x00CF0000;
    // COMPILER-DIFF: 13 (value pin): `key` is assigned in several leaves, so it is a global-alloc pseudo
    // that gets the pair left over after local-alloc gave the `key & ~mask` temp r9:r10; the target has
    // key in r9:r10 and the temp in r11:r12. With the pair fixed, the `or.` scratch alternates r0/r9 as
    // in the target, so the C/D leaves stay separate copies (their `or. r9` no longer matches `or. r0`).
    register u64 key asm("r9");
    u32 flags;

    switch (work->btn) {
    case DISP_A_NORMAL:
    case DISP_A_RAPID:
    case DISP_A_ACCENT:
        flags = work->flags;
        if (!(flags & ACTCTR_EXACT_KEY)) {
            if (!(flags & ACTCTR_ENFORCE_EXEC)) {
                if (flags & ACTCTR_NO_TRG) {
                    if (on & 0x80000) {
                        return 1;
                    }
                    break;
                }
                if (StaFlagChk(pG, STA_PL_ACTION)) {
                    return 1;
                }
                break;
            }
            if (flags & ACTCTR_NO_TRG) {
                if (on & 0x80000) {
                    return 1;
                }
                break;
            }
            if (trg & 0x80000) {
                return 1;
            }
            break;
        }
        if (!(flags & ACTCTR_ENFORCE_EXEC)) {
            if (flags & ACTCTR_NO_TRG) {
                if (!(on & 0x80000)) {
                    break;
                }
                key = on;
                if (key & ~(u64) 0x80000) {
                    break;
                }
                return 1;
            }
            if (!StaFlagChk(pG, STA_PL_ACTION)) {
                break;
            }
            key = trg;
            if (key & ~(u64) 0x80000) {
                break;
            }
            return 1;
        }
        if (flags & ACTCTR_NO_TRG) {
            if (!(on & 0x80000)) {
                break;
            }
            key = on;
            if (key & ~(u64) 0x80000) {
                break;
            }
            return 1;
        }
        if (!(trg & 0x80000)) {
            break;
        }
        key = trg;
        if (key & ~(u64) 0x80000) {
            break;
        }
        return 1;
    case DISP_L_R:
        if ((trg & 0xC00000) == 0xC00000 || ((on & 0x400000) && (trg & 0x800000)) ||
            ((trg & 0x400000) && (on & 0x800000))) {
            if (!(work->flags & ACTCTR_EXACT_KEY)) {
                return 1;
            }
            if ((on & 0xC0000) == 0xC0000) {
                return 0;
            }
            return 1;
        }
        break;
    case DISP_A_B:
        if ((trg & 0xC0000) == 0xC0000 || ((on & 0x80000) && (trg & 0x40000)) ||
            ((trg & 0x80000) && (on & 0x40000))) {
            if (!(work->flags & ACTCTR_EXACT_KEY)) {
                return 1;
            }
            if ((on & 0xC00000) == 0xC00000) {
                return 0;
            }
            return 1;
        }
        break;
    case DISP_B:
        if (!(trg & 0x40000)) {
            break;
        }
        if (!(work->flags & ACTCTR_EXACT_KEY)) {
            return 1;
        }
        key = on;
        if (key & ~(u64) 0x40000) {
            break;
        }
        return 1;
    case DISP_X:
        if (!(trg & 0x20000)) {
            break;
        }
        if (!(work->flags & ACTCTR_EXACT_KEY)) {
            return 1;
        }
        key = on;
        if (key & ~(u64) 0x20000) {
            break;
        }
        return 1;
    case DISP_Y:
        if (!(trg & 0x10000)) {
            break;
        }
        if (!(work->flags & ACTCTR_EXACT_KEY)) {
            return 1;
        }
        key = on;
        if (key & ~(u64) 0x10000) {
            break;
        }
        return 1;
    case DISP_L:
        // COMPILER-DIFF: 6 (cross-jump): codeless real insn, see the header comment
        asm volatile("");
        break;
    case DISP_R:
        break;
    }
    return 0;
}

// 1 when the live player may take the action (actCheck, or flags bit1 skips it); some button
// kinds need the player to be aiming (PlGetStatus 0x10), with flags bit0 marking Status_flg[0]
// 0x200000.
int cActionButton::checkPLStatus(ActBtnWork* work)
{
    if (pPL->hp > 0) {
        if ((work->flags & ACTCTR_ENFORCE_EXEC) || pPL->actCheck() != 0) {
            switch (work->btn) {
            case DISP_A_NORMAL:
            case DISP_A_RAPID:
            case DISP_A_B:
            case DISP_A_ACCENT:
                if (PlGetStatus() & 0x10) {
                    if (work->flags & ACTCTR_WEP_SET_IGNORE) {
                        StaFlagOn(pG, STA_ACT_DONT_FIRE);
                        return 1;
                    }
                    return 0;
                }
                break;
            }
            return 1;
        }
    }
    return 0;
}

// Next free prompt work of this frame (NULL when the 8 are used).
ActBtnWork* cActionButton::pullWork()
{
    ActBtnWork* w;

    if (m_num > 7) {
        return 0;
    }
    w = &work[m_num];
    m_num++;
    return w;
}

// Offers an action for this frame: message kind, priority slot 0..15, callback and its
// arguments, flags, button kind and call type.
void cActionButton::set(int kind, int slot, void* func, void* arg, int flags, int btn, int type, int d)
{
    ActBtnWork* w = pullWork();

    if (w == 0) {
        return;
    }
    if (kind > 0x41) {
        kind = 0x41;
    }
    w->kind = kind;
    w->func = func;
    w->arg = arg;
    w->type = type;
    w->flags = flags;
    w->btn = btn;
    w->slot = slot;
    w->d = d;
    AddPrim(&m_ot[slot], (u32*) w);
    switch (w->btn) {
    case DISP_A_NORMAL:
    case DISP_A_RAPID:
    case DISP_A_B:
    case DISP_A_ACCENT:
        if (w->flags & ACTCTR_WEP_SET_IGNORE) {
            StaFlagOn(pG, STA_ACT_DONT_FIRE);
        }
        break;
    }
}
