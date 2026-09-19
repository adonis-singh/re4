// game/se_at: the room's ambient sound emitters — the "ESE" sub-file of the room archive lists
// SeAt records (a SE block / number, a position, first-play wait, repeat count, fixed interval or
// a random one); SeAtCheck plays them on their timers every frame during play, the room scripts
// switch single emitters with SeAtSetOnOff / SeAtSndCall.
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "snd.h"
#include "global.h"
#include "db_log.h"
#include "rnd.h"

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp
extern int DebugMenuSelected;                            // game/db_menu.cpp

// Room start: takes the room's ESE emitter list (version 0x100) into Snd.se_at / se_at_list.
void SeAtInit()
{
    SndWork* s = &Snd;

    s->se_at = (SeAtHead*) GetDataExt(pG->pRoom, "ESE", 0);
    if (s->se_at == 0) {
        return;
    }
    if (s->se_at->version != 0x100) {
        pLog->err(0, 0, "SeAt DATA IS OLD VERSION");
        s->se_at = 0;
        s->se_at_list = 0;
        return;
    }
    s->se_at_list = (SeAt*) (s->se_at + 1);
}

// Per frame in the game routine (Rno0 3, not while Stop_flg 0x800): each enabled emitter waits
// its `wait` frames, then plays its SE (positioned unless flags2 bit0) and reloads `cnt` with the
// fixed interval or rnd_base + random(rnd_range); `repeat` counts the plays down (1 = last, -1 done).
void SeAtCheck()
{
    SndWork* s = &Snd;
    SeAt* at;
    Vec* pos;
    int i;

    if (SpfFlagChk(pG, SPF_SE_CALC)) {
        return;
    }
    if (DbgFlagChk(pG, DBG_TEST_MODE) && DebugMenuSelected != 0x18) {
        return;
    }
    if (pG->Rno0 != 3) {
        return;
    }
    if (s->se_at == 0) {
        return;
    }
    for (i = 0; i < s->se_at->num; i++) {
        at = &s->se_at_list[i];
        if ((at->flags & 1) == 0) {
            continue;
        }
        if (at->repeat < 0) {
            continue;
        }
        if (at->wait == 0) {
            if (at->cnt == 0) {
                pos = &at->pos;
                if (at->flags2 & 1) {
                    pos = 0;
                }
                if (SndCall(at->blk, at->se_no, pos, 0, 0, 0) == 0) {
                    continue;
                }
                if (at->repeat == 1) {
                    at->repeat = -1;
                    continue;
                }
                if (at->repeat != 0) {
                    at->repeat--;
                }
                if (at->interval == 0) {
                    at->cnt = at->rnd_base + Rnd() % at->rnd_range;
                } else {
                    at->cnt = at->interval;
                }
            } else {
                at->cnt--;
            }
        } else {
            at->wait--;
        }
    }
}

// Room script: enables / disables emitter `no` (flags bit0). 0 when not found.
int SeAtSetOnOff(int no, int on)
{
    SeAt* at = GetSeAtPtr(no);

    if (at == 0) {
        if (on == 1) {
            pLog->err(0, 0, "SeAtSetEnable() : AT DATA NOT FOUND");
        } else {
            pLog->err(0, 0, "SeAtSetDisable() : AT DATA NOT FOUND");
        }
        return 0;
    }
    if (on == 1) {
        at->flags |= 1;
    } else {
        at->flags &= ~1;
    }
    return 1;
}

// The emitter record numbered `no`, or 0.
SeAt* GetSeAtPtr(int no)
{
    SeAt* at;
    u32 i;

    if (Snd.se_at == 0) {
        return 0;
    }
    for (i = 0; i < Snd.se_at->num; i++) {
        at = &Snd.se_at_list[i];
        if (at->no == no) {
            return at;
        }
    }
    return 0;
}

// Plays emitter `no`'s SE once now; returns the SndCall handle (0 when not found).
u32 SeAtSndCall(int no)
{
    SeAt* at = GetSeAtPtr(no);

    if (at != 0) {
        if (at->flags2 & 1) {
            return SndCall(at->blk, at->se_no, 0, 0, 0, 0);
        }
        return SndCall(at->blk, at->se_no, &at->pos, 0, 0, 0);
    }
    pLog->err(0, 0, "SeAtSeCall() : AT DATA NOT FOUND");
    return 0;
}

