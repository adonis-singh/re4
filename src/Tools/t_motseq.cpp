#include "types.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "db_cam.h"
#include "gx_sub.h"
#include "main_mem.h"
#include "file.h"
#include "snd.h"
#include "dbmodule.h"
#include "t_prim.h"
#include "t_util.h"
#include "db_mod.h"
#include <string.h>
#include <dolphin/os.h>
#include "tools.h"

// Motion sequence editor (Tools/t_motseq.cpp): edits the key sequence (u16 count + MotionSeqKey[])
// of the motion shown in db_mod's slot 0 and saves it as a .seq file.

extern "C" void EprintfSetCurrentNo(int no);   // game/eprintf.cpp defines `int EprintfSetCurrentNo()`: this unit was built with the vendor's one-argument prototype, so it is not in eprintf.h
int SetToolLight(int no);      // db_light_tools.cpp

#define MSQ_KEY_MAX 1024


// The sequence being edited: the file image (count + keys) followed by the editor state.
struct MsqSeq {
    u16 num;                        // 0x0000  keys in use
    u8 reverse;                     // 0x0002  1: the sequence runs backwards (msqMakeSequence start > end)
    u8 pad_3;
    MotionSeqKey key[MSQ_KEY_MAX];  // 0x0004  frame (10.6), se + 1, flag bits
    u8 pad_1004[0x1090 - 0x1004];
    u32 x1090;                      // 0x1090
    u32 x1094;                      // 0x1094
    u8 pad_1098[8];
    u8 x10A0;                       // 0x10A0
    u8 loaded;                      // 0x10A1  1: the sequence came from a file
    s8 cursor;                      // 0x10A2  0: frame, 1..8: flag bit, 9: se
    u8 changed;                     // 0x10A3
    u8 flagDisp[8];                 // 0x10A4  flag bits of the current key (display)
    u8 copyFlagDisp[8];             // 0x10AC  flag bits of the copy buffer (display)
    u8 speed;                       // 0x10B4  0: off, 1: on, 2: on + position reset
    u8 pad_10B5[3];
    MotionSeqKey copy;              // 0x10B8  copy buffer
    u32 viewFlag;                   // 0x10BC  MotionSetCore flags (low half -> dbModSlot[0].seqFlag)
};

struct MsqWork {
    MsqSeq seq[1];                  // 0x0000
    int mode;                       // 0x10C0  msqFunc index
    int sub1;                       // 0x10C4
    int sub2;                       // 0x10C8  menu cursor (msq_y_tbl index)
    int sub3;                       // 0x10CC  file menu cursor
    u8 x10D0;                       // 0x10D0
    u8 x10D1;
    u8 x10D2;
    u8 exitSel;                     // 0x10D3  1: exit confirmed
    int fileSub;                    // 0x10D4
    JOY joy;                        // 0x10D8  pad snapshot (cleared while the debug camera has it)
    u8 pad_1340[0x15AC - 0x1340];
    u32 clearCol;                   // 0x15AC
    u8 pad_15B0[8];
    u8 col;                         // 0x15B8  eprintf colour
    GXColor bg;                     // 0x15B9  copy-clear colour (unaligned)
    u8 pad_15BD[3];
    int camMode;                    // 0x15C0  1: the debug camera has the pad
    int errTimer;                   // 0x15C4  frames left for the error message
    u8 errType;                     // 0x15C8  1: sequence frame > motion frame, 2: <
    u8 pad_15C9[0x15F0 - 0x15C9];
    int x15F0;                      // 0x15F0
    u8 pad_15F4[0x1CA4 - 0x15F4];
    u32 seqNo;                      // 0x1CA4  0..7, the digit before ".seq"
    char fileName[0x1DAC - 0x1CA8]; // 0x1CA8
    int start;                      // 0x1DAC  msqMakeSequence parameters (10.6 frames)
    int end;                        // 0x1DB0
    int add;                        // 0x1DB4
    int max;                        // 0x1DB8
    u8 seMode;                      // 0x1DBC  0: enemy SE bank, 1: player
    u8 pad_1DBD[0x1E10 - 0x1DBD];
};

static MsqWork* msqWork = {0};
#define MSQ (msqWork)

static void msq_R0_Model();
static void msq_R0_SeqLoad();
static void msq_R0_SeqMake();
static void msq_R0_Sequence();
static void msq_R0_SeqResize();
static void msq_R0_File();
static void msq_R0_QuitCk();
static void msq_R0_Quit();

void msqToolInit();
void msqMakeSequence(int start, int end, int add);
void msqSeqDelete(u32 no);
void msqSeqAdd(u32 no);
void msqSeqFrameAdd(int no, u32 step, int sub);
void msqDisp();
int msqLoadFile();
int msqSaveFile();
void msqCameraMove();
void msqErrorMessage();
void msqFrameSizeCk();

// Tool routines by MsqWork::mode: 0 model select, 1 sequence file load, 2 new sequence parameters,
// 3 sequence editing, 4 resize, 5 file menu, 6 quit check, 7 quit.
static void (*msqFunc[8])() = {
    msq_R0_Model,    msq_R0_SeqLoad, msq_R0_SeqMake, msq_R0_Sequence,
    msq_R0_SeqResize, msq_R0_File,   msq_R0_QuitCk,  msq_R0_Quit,
};

// y of the ">" cursor per sub2 value: 0/1 unused, 2..11 the sequence lines, 12..19 the menu lines
static int msq_y_tbl[21] = {
    0x1C, 0x2A, 0xD2, 0xEE, 0xFC, 0x10A, 0x118, 0x126, 0x134, 0x142, 0x150, 0x15E,
    0x62, 0x70, 0x7E, 0x8C, 0x9A, 0xA8, 0xB6, 0xC4, 0,
};

// Switches the tool routine and resets its sub cursors.
static inline void msqSetMode(int mode)
{
    MSQ->mode = mode;
    MSQ->sub1 = 0;
    MSQ->sub2 = 0;
    MSQ->sub3 = 0;
}

// MOTION SEQUENCE TOOL entry (debug menu 6): init, then every frame the pad snapshot / debug
// camera (START toggles it), msqFunc[mode], the display, the sub stick up (bit 23) SE bank switch
// (enemy / player) and the model animation (dbModMotionMove).
void ToolMotSeq()
{
    msqToolInit();
    TaskSleep(1);
    for (;;) {
        msqCameraMove();
        msqFunc[MSQ->mode]();
        msqDisp();
        if (MSQ->joy.trg & 0x800000) {
            MSQ->seMode++;
            if (MSQ->seMode > 1) {
                MSQ->seMode = 0;
            }
        }
        CameraMove();
        msqErrorMessage();
        TaskSleep(1);
    }
}

// Tool start: work allocated (Debug heap), room arrays parked, default tool flags, tool light 2,
// db_mod viewer started (dbModelInit), colours; starts in the model select routine.
void msqToolInit()
{
    int i;
    MsqWork* w;
    CAMERA* cam;
    TprimRect rect;
    MsqWork*& wp = msqWork;
    f32 zero;

    TutilInitDefault();
    wp = (MsqWork*) Debug_alloc(sizeof(MsqWork), 1);
    if (wp == NULL) {
        OSReport("\n\nDebug Memory Allocation Error !!!");
        TaskExit();
    }
    EprintfSetCurrentNo(0);
    StaFlagOn(pG, STA_BG_OFF);
    DbgFlagOn(pG, DBG_DBG_CAM);
    pG->Stop_flg |= 0x10000000;
    pG->Stop_flg |= 0x00800000;
    pG->Disp_flg |= 0x02000000;
    pG->Disp_flg |= 0x00800000;
    memclr_asm(MSQ, sizeof(MsqWork));
    for (i = 0; i < 1; i++) {
        MSQ->seq[i].x1090 = 0;
        MSQ->seq[i].x1094 = 0;
        MSQ->seq[i].x10A0 = 0;
        MSQ->seq[i].speed = 0;
    }
    MSQ->clearCol = 0x58000000;
    MSQ->x15F0 = 1;
    MSQ->camMode = 0;
    MSQ->seMode = 0;
    MSQ->bg.r = MSQ->bg.g = MSQ->bg.b = 0x30;
    MSQ->bg.a = 0;
    bio4_GXSetCopyClear(MSQ->bg, 0xFFFFFF);
    w = MSQ;
    w->seq[0].cursor = 0;
    memclr_asm(w->seq[0].flagDisp, sizeof(w->seq[0].flagDisp));
    memclr_asm(w->seq[0].copyFlagDisp, sizeof(w->seq[0].copyFlagDisp));
    w->seq[0].copy.Free = 0;
    w->seq[0].copy.Se = 0;
    w->seq[0].viewFlag = 4;
    ToolArrayPush(0);
    zero = 0.0f;
    cam = &pG->Camera;
    cam->param.at.y = 1000.0f;
    cam->param.at.x = zero;
    cam->param.at.z = zero;
    cam->param.pos.x = zero;
    cam->param.pos.y = 1000.0f;
    cam->param.pos.z = 3000.0f;
    cam->param.roll = zero;
    CameraSetOrientationRoll(cam);
    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = 512.0f;
    rect.h = 448.0f;
    TprimInitEnv2D(&rect);
    SetToolLight(2);
    dbModelInit();
    pG->Stop_flg |= 0x40000000;
    msqSetMode(0);
}

// Mode 0: the db_mod menu picks the model / motion into slot 0 (dbModel); B there opens the
// EXIT: YES/NO prompt; a loaded model goes to the sequence load routine.
static void msq_R0_Model()
{
    JOY* joy = &Joy[0];
    int mode = 0;
    int ret;

    switch (MSQ->sub1) {
    case 0:
        if (joy->trg & 0x200) {
            MSQ->sub1 = 1;
        }
        break;
    case 1:
        mode = 1;
        if (joy->trg & 3) {
            MSQ->exitSel = MSQ->exitSel == 0;
        }
        if (MSQ->exitSel) {
            eprintf(200, 168, 4, MSQ->col, "EXIT: YES/---");
        } else {
            eprintf(200, 168, 4, MSQ->col, "EXIT: ---/NO-");
        }
        if (joy->trg & 0x100) {
            if (MSQ->exitSel == 1) {
                msqSetMode(7);
            }
        }
        if (joy->trg & 0x300) {
            MSQ->sub1 = 0;
            MSQ->exitSel = 0;
        }
        break;
    }
    ret = dbModel(mode);
    switch (ret) {
    case 1:
        msqSetMode(1);
        MSQ->seqNo = 0;
        break;
    case 3:
        msqSetMode(0);
        break;
    }
}

// Mode 1: picks the sequence number 0..7 (left/right) of the motion file (<motion>N.seq); A loads
// it (existing file -> editing, else -> new sequence parameters), B back to the model select.
static void msq_R0_SeqLoad()
{
    MsqWork* w = MSQ;
    char* p;

    if (w->sub1 == 0) {
        dbModGetMotFilename(0, w->fileName);
        p = strchr(MSQ->fileName, '.');
        if (p == NULL) {
            msqSetMode(0);
            return;
        }
        p[0] = '0';
        p[1] = '.';
        p[2] = 's';
        p[3] = 'e';
        p[4] = 'q';
        p[5] = 0;
        MSQ->sub1++;
    }
    if (MSQ->joy.rep & 2) {
        if (MSQ->seqNo < 7) {
            MSQ->seqNo++;
        }
    }
    if (MSQ->joy.rep & 1) {
        if (MSQ->seqNo != 0) {
            MSQ->seqNo--;
        }
    }
    p = strchr(MSQ->fileName, '.');
    p[-1] = MSQ->seqNo + '0';
    if (MSQ->joy.trg & 0x200) {
        msqSetMode(0);
    } else if (MSQ->joy.trg & 0x100) {
        if (msqLoadFile()) {
            w->seq[0].loaded = 1;
            dbModMotionSetSeq(0, w, w->seq[0].viewFlag, 0);
            msqFrameSizeCk();
            msqSetMode(3);
        } else {
            msqSetMode(2);
        }
    }
}

// Mode 2, --New Sequence--: Start / End / Add frame rows (d-pad, R x10) within the motion length;
// A builds the keys (msqMakeSequence) and enters editing; B back.
static void msq_R0_SeqMake()
{
    cModel* m = dbModSlot[0].pModel;

    if (MSQ->sub1 == 0) {
        MSQ->start = 0;
        MSQ->end = (int) m->Motion.Mot_frame_max;
        MSQ->end <<= 6;
        MSQ->add = 0x40;
        MSQ->max = MSQ->end;
        MSQ->sub1++;
    }
    if (MSQ->joy.rep & 0x00080008) {
        MSQ->sub2--;
        if (MSQ->sub2 < 0) {
            MSQ->sub2 = 3;
        }
    }
    if (MSQ->joy.rep & 0x00040004) {
        MSQ->sub2++;
        if (MSQ->sub2 > 3) {
            MSQ->sub2 = 0;
        }
    }
    switch (MSQ->sub2) {
    case 0:
        if (MSQ->joy.rep & 0x00020002) {
            if (MSQ->joy.on & 0x20) {
                MSQ->start += 0x280;
            } else {
                MSQ->start += 0x40;
            }
            if (MSQ->start > MSQ->max) {
                MSQ->start = MSQ->max;
            }
        }
        if (MSQ->joy.rep & 0x00010001) {
            if (MSQ->joy.on & 0x20) {
                MSQ->start -= 0x280;
            } else {
                MSQ->start -= 0x40;
            }
            if (MSQ->start < 0) {
                MSQ->start = 0;
            }
        }
        break;
    case 1:
        if (MSQ->joy.rep & 0x00020002) {
            if (MSQ->joy.on & 0x20) {
                MSQ->end += 0x280;
            } else {
                MSQ->end += 0x40;
            }
            if (MSQ->end > MSQ->max) {
                MSQ->end = MSQ->max;
            }
        }
        if (MSQ->joy.rep & 0x00010001) {
            if (MSQ->joy.on & 0x20) {
                MSQ->end -= 0x280;
            } else {
                MSQ->end -= 0x40;
            }
            if (MSQ->end < 0) {
                MSQ->end = 0;
            }
        }
        break;
    case 2:
        if (MSQ->joy.rep & 0x00020002) {
            if (MSQ->joy.on & 0x20) {
                MSQ->add += 1;
            } else {
                MSQ->add += 0x40;
            }
            if (MSQ->add > MSQ->max) {
                MSQ->add = MSQ->max;
            }
        }
        if (MSQ->joy.rep & 0x00010001) {
            if (MSQ->joy.on & 0x20) {
                MSQ->add -= 1;
            } else {
                MSQ->add -= 0x40;
            }
            if (!(MSQ->add > 0)) {
                MSQ->add = 1;
            }
        }
        break;
    }
    if (MSQ->joy.trg & 0x200) {
        msqSetMode(0);
    } else if (MSQ->joy.trg & 0x100) {
        if (MSQ->sub2 == 3) {
            msqMakeSequence(MSQ->start, MSQ->end, MSQ->add);
            msqSetMode(3);
        }
    }
}

// Mode 4, RESIZE: rebuilds the key list at the new frame count keeping the flags / SE of the old
// keys, then back to editing.
static void msq_R0_SeqResize()
{
    MsqWork* w = MSQ;
    cModel* m = dbModSlot[0].pModel;
    int max;
    int i;
    MotionSeqKey* k;

    if (w->seq[0].reverse & 1) {
        msqSetMode(3);
        return;
    }
    max = (int) m->Motion.Mot_frame_max;
    max <<= 6;
    // Drop the trailing keys past the motion's last frame. `num` is never held in a local: every
    // read is the u16 field (gcse PRE gives the reload after each store), the decrement sits at the
    // end of the preheader and of the loop body (jump2 cross-jumps the two `sth`), the exits are
    // gotos so stmt.c does not rotate the loop, and `k` is recomputed from `i` (loop.c giv copy).
    i = w->seq[0].num - 1;
    if (i >= 0) {
        k = &w->seq[0].key[i];
        if (k->frame > max) {
            w->seq[0].num--;
            for (;;) {
                i--;
                if (i < 0) {
                    goto done;
                }
                k = &w->seq[0].key[i];
                if (k->frame <= max) {
                    goto done;
                }
                w->seq[0].num--;
            }
        }
    }
done:
    if (w->seq[0].num == 0) {
        k = &w->seq[0].key[0];
        k->frame = 0;
        w->seq[0].num = 1;
        k->Free = 0;
        k->Se = 0;
        dbModMotionSetSeq(0, w, w->seq[0].viewFlag, 0);
        msqSetMode(3);
        return;
    }
    if (w->seq[0].num > 1) {
        int step = w->seq[0].key[1].frame - w->seq[0].key[0].frame;
        int f;

        k = &w->seq[0].key[w->seq[0].num - 1];
        f = k->frame;
        if (step > 0) {
            f += step;
            // Goto loop (no loop notes): the shared zero of the two byte stores then has 3 refs
            // and is allocated after `max` (r5 / r6).
            if (f <= max && w->seq[0].num - 1 <= 0x3FF) {
                u8 z = 0;

            again2:
                w->seq[0].num++;
                k = &w->seq[0].key[w->seq[0].num - 1];
                k->frame = f;
                k->Free = z;
                k->Se = z;
                f += step;
                if (f <= max && w->seq[0].num - 1 <= 0x3FF) {
                    goto again2;
                }
            }
        }
    }
    dbModMotionSetSeq(0, w, w->seq[0].viewFlag, 0);
    msqSetMode(3);
}

// Plays the SE of key `no` of the shown model (bank by seMode) and clears the model's own SE keys.
static inline void msqPlaySe(MsqWork* w, cModel* m)
{
    u8 se = w->seq[0].key[(u32) m->Motion.Seq_frame].Se;

    if (se != 0) {
        switch (MSQ->seMode) {
        case 0:
            SndCall(8, se - 1, &m->pos, 0xFF, 0, 0);
            break;
        case 1:
            SndCall(1, se - 1, &m->pos, 0, 0, 0);
            break;
        }
        m->Motion.Seq.Se = 0;
        m->Motion.Seq_old.Se = 0;
    }
}

// Frame step for the L/R edits of the current key: 1 (L+R), 16 (R), 640 (L), else 64.
static inline void msqFrameStep(s16 cur, u32 on, int sub)
{
    if ((on & 0x60) == 0x60) {
        msqSeqFrameAdd(cur, 1, sub);
    } else if (on & 0x20) {
        msqSeqFrameAdd(cur, 0x10, sub);
    } else if (on & 0x40) {
        msqSeqFrameAdd(cur, 0x280, sub);
    } else {
        msqSeqFrameAdd(cur, 0x40, sub);
    }
}

// Mode 3, the key editor: the motion plays (A + sub stick skips frames, X/Y with the L/R
// combinations add / delete a key at the current frame, R+Z / L+Z / L+R+Z copy / paste / cut a
// key); up/down pick the row (frame, 8 flag bits, SE number), left/right toggle the flag or step
// the SE; Z switches the SE bank test, B opens the file menu. SEs of the key are played on the
// model as the sequence passes them.
static void msq_R0_Sequence()
{
    MsqWork* w = MSQ;
    cModel* m = dbModSlot[0].pModel;
    s16 cur;
    s16 cur2;
    cParts* p;

    if (w->joy.trg & 0x200) {
        msqSetMode(5);
        return;
    }
    if ((w->joy.on & 0x60) == 0x60 && (w->joy.trg & 0x400)) {
        msqSeqDelete((u32) m->Motion.Seq_frame);
    }
    if ((MSQ->joy.on & 0x40) && (MSQ->joy.trg & 0x800)) {
        msqSeqAdd((u32) m->Motion.Seq_frame);
    }
    cur = (s16) m->Motion.Seq_frame;
    if ((MSQ->joy.on & 0x20100) == 0x20100 || (!(MSQ->joy.on & 0x100) && (MSQ->joy.rep & 0x20000))) {
        MSQ->clearCol &= ~0x20000000;
        w->seq[0].viewFlag &= ~2;
        dbModUnsetViewFlag(2);
        dbModSlot[0].seqFlag = (u16) w->seq[0].viewFlag;
        dbModMotionMove();
        msqPlaySe(w, m);
    }
    if ((MSQ->joy.on & 0x10100) == 0x10100 || (!(MSQ->joy.on & 0x100) && (MSQ->joy.rep & 0x10000))) {
        MSQ->clearCol &= ~0x20000000;
        w->seq[0].viewFlag |= 2;
        dbModSetViewFlag(2);
        dbModSlot[0].seqFlag = (u16) w->seq[0].viewFlag;
        dbModMotionMove();
        msqPlaySe(w, m);
    }
    cur2 = (s16) m->Motion.Seq_frame;
    if (MSQ->joy.trg & 8) {
        w->seq[0].cursor--;
        if (w->seq[0].cursor < 0) {
            w->seq[0].cursor = 9;
        }
    }
    if (MSQ->joy.trg & 4) {
        w->seq[0].cursor++;
        if (w->seq[0].cursor > 9) {
            w->seq[0].cursor = 0;
        }
    }
    if (cur != cur2) {
        int t;

        if (cur2 > 0) {
            cur2 = cur2 - 1;
        } else {
            cur2 = m->Motion.Seq_frame_num - 1;
        }
        // Dead in effect (cur is not read again): the original keeps a `cmpwi cur,0` with no branch here,
        // which needs a test whose arm is live at flow1 and dies only through a third test (pass 8/18b form).
        t = cur2;
        if (cur != 0) {
            t = cur;
        }
        if (t != cur2) {
            cur = t;
        }
    }
    if (MSQ->joy.trg & 0x10) {
        u32 on = MSQ->joy.on;

        if ((on & 0x60) == 0x60) {
            w->seq[0].copy = w->seq[0].key[cur2];
            w->seq[0].key[cur2].Se = 0;
            w->seq[0].key[cur2].Free = 0;
            w->seq[0].changed = 1;
        } else {
            if (on & 0x20) {
                w->seq[0].copy = w->seq[0].key[cur2];
            }
            if (MSQ->joy.on & 0x40) {
                w->seq[0].key[cur2].Se = w->seq[0].copy.Se;
                w->seq[0].key[cur2].Free = w->seq[0].copy.Free;
                w->seq[0].changed = 1;
            }
        }
    }
    if (MSQ->joy.on & 2) {
        u32 on = MSQ->joy.on;

        switch (w->seq[0].cursor) {
        case 1:
            w->seq[0].key[cur2].Free |= 0x01;
            break;
        case 2:
            w->seq[0].key[cur2].Free |= 0x02;
            break;
        case 3:
            w->seq[0].key[cur2].Free |= 0x04;
            break;
        case 4:
            w->seq[0].key[cur2].Free |= 0x08;
            break;
        case 5:
            w->seq[0].key[cur2].Free |= 0x10;
            break;
        case 6:
            w->seq[0].key[cur2].Free |= 0x20;
            break;
        case 7:
            w->seq[0].key[cur2].Free |= 0x40;
            break;
        case 8:
            w->seq[0].key[cur2].Free |= 0x80;
            break;
        case 9:
            if (MSQ->joy.rep & 2) {
                int step;

                if (on & 0x20) {
                    step = 0x10;
                } else {
                    step = 1;
                }
                int c = (u8) w->seq[0].key[cur2].Se;  // narrow `+` via an int local: `add step, c`

                w->seq[0].key[cur2].Se = step + c;
            }
            break;
        case 0:
            if (MSQ->joy.rep & 2) {
                msqFrameStep(cur2, on, 0);
            }
            break;
        }
        w->seq[0].changed = 1;
    }
    if (MSQ->joy.on & 1) {
        u32 on = MSQ->joy.on;

        switch (w->seq[0].cursor) {
        case 1:
            w->seq[0].key[cur2].Free &= ~0x01;
            break;
        case 2:
            w->seq[0].key[cur2].Free &= ~0x02;
            break;
        case 3:
            w->seq[0].key[cur2].Free &= ~0x04;
            break;
        case 4:
            w->seq[0].key[cur2].Free &= ~0x08;
            break;
        case 5:
            w->seq[0].key[cur2].Free &= ~0x10;
            break;
        case 6:
            w->seq[0].key[cur2].Free &= ~0x20;
            break;
        case 7:
            w->seq[0].key[cur2].Free &= ~0x40;
            break;
        case 8:
            w->seq[0].key[cur2].Free &= ~0x80;
            break;
        case 9:
            if (MSQ->joy.rep & 1) {
                int step;

                if (on & 0x20) {
                    step = 0x10;
                } else {
                    step = 1;
                }
                w->seq[0].key[cur2].Se -= step;
            }
            break;
        case 0:
            if (MSQ->joy.rep & 1) {
                msqFrameStep(cur2, on, 1);
            }
            break;
        }
        w->seq[0].changed = 1;
    }
    {
        u8 cf = w->seq[0].copy.Free;

        w->seq[0].flagDisp[0] = w->seq[0].key[cur2].Free & 0x01;
        w->seq[0].flagDisp[1] = w->seq[0].key[cur2].Free & 0x02;
        w->seq[0].flagDisp[2] = w->seq[0].key[cur2].Free & 0x04;
        // COMPILER-DIFF: #13 (free sched slot filler): the original's sched1 has one insn between
        // `stb flagDisp[2]` and `lbz flagDisp[3]`'s load, so the two chains share r0 instead of
        // alternating r0/r9 (local-alloc's birth-2/death+1 conflict). Codeless, no bytes.
        asm("" : "=m"(w->seq[0].flagDisp[2]) : "m"(w->seq[0].flagDisp[2]));
        w->seq[0].flagDisp[3] = w->seq[0].key[cur2].Free & 0x08;
        w->seq[0].flagDisp[4] = w->seq[0].key[cur2].Free & 0x10;
        w->seq[0].flagDisp[5] = w->seq[0].key[cur2].Free & 0x20;
        w->seq[0].flagDisp[6] = w->seq[0].key[cur2].Free & 0x40;
        w->seq[0].flagDisp[7] = w->seq[0].key[cur2].Free & 0x80;
        w->seq[0].copyFlagDisp[0] = cf & 0x01;
        w->seq[0].copyFlagDisp[1] = cf & 0x02;
        w->seq[0].copyFlagDisp[2] = cf & 0x04;
        w->seq[0].copyFlagDisp[3] = cf & 0x08;
        w->seq[0].copyFlagDisp[4] = cf & 0x10;
        w->seq[0].copyFlagDisp[5] = cf & 0x20;
        w->seq[0].copyFlagDisp[6] = cf & 0x40;
        w->seq[0].copyFlagDisp[7] = cf & 0x80;
    }
    MSQ->sub2 = w->seq[0].cursor + 2;
    for (p = m->pList; p != NULL; p = p->pList) {
        Draw_line3d(&p->world, &p->pParent->world, 0xFFFFFFFF, 0);
    }
}

// Mode 5, the file menu: CANCEL, SPEED (off / on / on + position reset), SAVE, RELOAD, RESIZE,
// SAVE & EXIT, RENEWAL (new sequence), EXIT; B back to editing.
static void msq_R0_File()
{
    MsqWork* w = MSQ;
    cModel* m = dbModSlot[0].pModel;

    if (w->joy.trg & 0x200) {
        msqSetMode(3);
        return;
    }
    if (w->joy.trg & 0x100) {
        switch (w->sub3) {
        case 0:
        default:
            msqSetMode(3);
            break;
        case 1:
            w->seq[0].speed++;
            if (w->seq[0].speed > 2) {
                w->seq[0].speed = 0;
            }
            switch (w->seq[0].speed) {
            case 0:
            default:
                w->seq[0].viewFlag |= 4;
                break;
            case 1:
                w->seq[0].viewFlag = (w->seq[0].viewFlag & ~0x10) | 1;
                break;
            case 2:
                w->seq[0].viewFlag |= 0x10;
                m->pos.z = 0.0f;
                m->pos.y = 0.0f;
                m->pos.x = 0.0f;
                break;
            }
            break;
        case 2:
            msqSaveFile();
            w->seq[0].changed = 0;
            msqSetMode(3);
            break;
        case 3:
            msqLoadFile();
            w->seq[0].changed = 0;
            msqSetMode(3);
            break;
        case 4:
            msqSetMode(4);
            MSQ->fileSub = 0;
            break;
        case 5:
            msqSaveFile();
            w->seq[0].changed = 0;
            msqSetMode(0);
            MSQ->fileSub = 0;
            break;
        case 6:
            msqSetMode(2);
            MSQ->fileSub = 0;
            break;
        case 7:
            w->seq[0].changed = 0;
            msqSetMode(0);
            MSQ->fileSub = 0;
            break;
        }
    } else {
        if (w->joy.trg & 8) {
            w->sub3--;
        }
        if (MSQ->joy.trg & 4) {
            MSQ->sub3++;
        }
        if (MSQ->sub3 < 0) {
            MSQ->sub3 = 7;
        }
        if (MSQ->sub3 > 7) {
            MSQ->sub3 = 0;
        }
        MSQ->sub2 = MSQ->sub3 + 12;
    }
}

// Mode 6: EXIT YES/NO (unsaved changes) -> quit or back to editing.
static void msq_R0_QuitCk()
{
    MsqWork* w = MSQ;

    if (w->joy.trg & 0x200) {
        msqSetMode(3);
        return;
    }
    if (w->joy.trg & 0x100) {
        // default written FIRST with three identical arms: the case-1 arm cross-jumps into the
        // default body and its `beq` (now a jump to the next insn) is deleted after flow, leaving
        // the dead `cmpwi r0,1` the original has
        switch (w->sub3) {
        default:
            msqSetMode(3);
            break;
        case 0:
            msqSetMode(3);
            break;
        case 1:
            msqSetMode(3);
            break;
        }
    } else {
        if (w->joy.trg & 8) {
            w->sub3--;
        }
        if (MSQ->joy.trg & 4) {
            MSQ->sub3++;
        }
        if (MSQ->sub3 < 0) {
            MSQ->sub3 = 1;
        }
        if (MSQ->sub3 > 1) {
            MSQ->sub3 = 0;
        }
        MSQ->sub2 = MSQ->sub3 + 12;
    }
}

// Mode 7: closes the viewer (dbModelQuit), restores the arrays / flags / light, frees the work and
// ends the task.
static void msq_R0_Quit()
{
    dbModelQuit();
    pG->Stop_flg &= ~0x40000000;
    DbgFlagOff(pG, DBG_TEST_MODE);
    ToolWorkPop(0);
    bio4_GXSetCopyClear(g_sysBgColor, 0xFFFFFF);
    DbgFlagOff(pG, DBG_DBG_CAM);
    StaFlagOff(pG, STA_BG_OFF);
    pG->Stop_flg &= ~0x10000000;
    pG->Stop_flg &= ~0x00800000;
    pG->Disp_flg &= ~0x00800000;
    pG->Disp_flg &= ~0x02000000;
    TutilQuitDefault();
    TaskExit();
}

// Fills the sequence with keys from `start` to `end` every `add` frames (both 10.6 fixed point).
void msqMakeSequence(int start, int end, int add)
{
    MsqWork* w = MSQ;
    MotionSeqKey* k = w->seq[0].key;
    u32 n = 1;
    int f = start;
    int i;

    for (i = 0; i < MSQ_KEY_MAX; i++) {
        k->frame = f;
        k->Se = 0;
        k->Free = 0;
        if (start <= end) {
            f += add;
            if (f <= end) {
                n++;
            }
        } else {
            f -= add;
            if (f >= end) {
                n++;
            }
        }
        k++;
    }
    if (n > 999) {
        n = 999;
    }
    w->seq[0].num = n;
    w->seq[0].reverse = 0;
    if (start > end) {
        w->seq[0].reverse = 1;
    }
    dbModMotionSetSeq(0, w, w->seq[0].viewFlag, 0);
}

// Removes key `no`.
void msqSeqDelete(u32 no)
{
    MsqWork* w = MSQ;
    u32 i;

    if (w->seq[0].num > 1) {
        MotionSeqKey* d;
        MotionSeqKey* s;

        w->seq[0].num--;
        d = &w->seq[0].key[no];
        s = &w->seq[0].key[no + 1];
        for (i = no; i < MSQ_KEY_MAX - 1; i++) {
            *d++ = *s++;
        }
        *s = *d;
        if (no >= w->seq[0].num) {
            no = w->seq[0].num - 1;
        }
        dbModMotionSetSeq(0, w, w->seq[0].viewFlag, no);
        w->seq[0].changed = 1;
    }
}

// Duplicates key `no` (the keys after it move up by one).
void msqSeqAdd(u32 no)
{
    MsqWork* w = MSQ;
    MotionSeqKey prev;
    MotionSeqKey tmp;
    u32 i;

    if (w->seq[0].num < 999) {
        MotionSeqKey* p = &w->seq[0].key[no];
        w->seq[0].num++;
        prev = *p;
        for (i = no; i < MSQ_KEY_MAX; i++) {
            tmp = *p;
            *p = prev;
            prev = tmp;
            p++;
        }
        dbModMotionSetSeq(0, w, w->seq[0].viewFlag, no);
        w->seq[0].changed = 1;
    }
}

// Moves key `no` by `step` frames (sub: backwards), clamped to 0 / the motion length.
void msqSeqFrameAdd(int no, u32 step, int sub)
{
    MsqWork* w = MSQ;
    MotionSeqKey* k = &w->seq[0].key[no];
    cModel* m = dbModSlot[0].pModel;

    if (sub) {
        if (k->frame > step) {
            k->frame -= step;
        } else {
            k->frame = 0;
        }
    } else {
        u32 max;

        k->frame += step;
        max = (u32) m->Motion.Mot_frame_max << 6;
        if (k->frame > max) {
            k->frame = max;
        }
    }
}

// Draws the tool: title, the routine's rows (file menu, sequence number / file name, new sequence
// parameters, the current key's frame / flags / SE and the copy buffer, sequence and motion frame
// counters, the HELP column) with the ">" cursor from msq_y_tbl.
void msqDisp()
{
    MsqWork* w = MSQ;
    cModel* m = dbModSlot[0].pModel;
    TprimRect rc;
    GXColor col;
    int i;
    int c;
    int r;
    char* p;
    MotionSeqKey* k;
    s16 cur;
    s16 y0;
    int cx;  // text column; set to 3 and 48 (two sets: gcse cprop leaves `cx * 8` unfolded after the loops)
    int j;   // row counter post-incremented in the eprintf argument (the y giv's `+14` lands in the arg block)

    eprintf(24, 14, 4, w->col, "MOTION SEQUENCE TOOL");
    if (MSQ->camMode && (pG->Frame_cnt & 0x10)) {
        eprintf(24, 28, 4, MSQ->col, "1P CAMERA MODE");
    }
    switch (MSQ->mode) {
    case 5:
        eprintf(24, 84, 0, MSQ->col, "-- SEQUENCE MENU-- ");
        eprintf(24, 98, 0, MSQ->col, "CANCEL      -> return sequence tool");
        switch (w->seq[0].speed) {
        case 0:
        default:
            eprintf(24, 112, 0, MSQ->col, "SPEED OFF");
            break;
        case 1:
            eprintf(24, 112, 0, MSQ->col, "SPEED ON ");
            break;
        case 2:
            eprintf(24, 112, 0, MSQ->col, "SPEED ON POS RESET");
            break;
        }
        eprintf(24, 126, 0, MSQ->col, "SAVE        -> save sequece");
        eprintf(24, 140, 0, MSQ->col, "RELOAD      -> reload sequence");
        eprintf(24, 154, 0, MSQ->col, "RESIZE      -> resize sequence");
        eprintf(24, 168, 0, MSQ->col, "SAVE & EXIT -> save & exit tool");
        eprintf(24, 182, 0, MSQ->col, "RENEWAL     -> renewal sequence");
        eprintf(24, 196, 0, MSQ->col, "EXIT        -> exit tool");
        if (pG->Frame_cnt & 8) {
            eprintf(16, msq_y_tbl[MSQ->sub2], 0, MSQ->col, ">");
        }
        break;
    case 1:
        eprintf(24, 56, 0, MSQ->col, "Sequence No. %02d / 07", MSQ->seqNo);
        p = strchr(MSQ->fileName, '/');
        if (p != NULL) {
            p = strchr(p + 1, '/');
            if (p != NULL) {
                p = strchr(p + 1, '/');
                if (p != NULL) {
                    p = strchr(p + 1, '/');
                    if (p != NULL) {
                        eprintf(24, 70, 0, MSQ->col, "Filename   : %s", p + 1);
                    }
                }
            }
        }
        break;
    case 2:
        eprintf(24, 56, 0, MSQ->col, "Sequence No. %02d / 07", MSQ->seqNo);
        p = strchr(MSQ->fileName, '/');
        if (p != NULL) {
            p = strchr(p + 1, '/');
            if (p != NULL) {
                p = strchr(p + 1, '/');
                if (p != NULL) {
                    p = strchr(p + 1, '/');
                    if (p != NULL) {
                        eprintf(24, 70, 0, MSQ->col, "Filename   : %s", p + 1);
                    }
                }
            }
        }
        eprintf(24, 84, 0, MSQ->col, "SEQUENCE:--New Sequence--");
        eprintf(24, 112, 0, MSQ->col, "Start Frame :  %03d", MSQ->start / 64);
        eprintf(24, 126, 0, MSQ->col, "End   Frame :  %03d", MSQ->end / 64);
        eprintf(24, 140, 0, MSQ->col, "Add   Frame :  %03.2f", (f32) MSQ->add / 64.0f);
        eprintf(24, 154, 0, MSQ->col, "               Ok");
        eprintf(24, 168, 0, MSQ->col, "Max   Frame :  %03d", MSQ->max / 64);
        eprintf(136, (MSQ->sub2 + 8) * 14, 4, MSQ->col, ">");
        break;
    case 3: {
        eprintf(24, 56, 0, MSQ->col, "Sequence No. %02d / 07", MSQ->seqNo);
        p = strchr(MSQ->fileName, '/');
        if (p != NULL) {
            p = strchr(p + 1, '/');
            if (p != NULL) {
                p = strchr(p + 1, '/');
                if (p != NULL) {
                    p = strchr(p + 1, '/');
                    if (p != NULL) {
                        eprintf(24, 70, 0, MSQ->col, "Filename   : %s", p + 1);
                    }
                }
            }
        }
        if (w->seq[0].loaded == 0) {
            eprintf(24, 84, 0, MSQ->col, "SEQUENCE:--New Sequence--");
        }
        cur = (s16) m->Motion.Seq_frame;
        if (MSQ->seMode) {
            eprintf(24, 168, 0, MSQ->col, "SE:PL (Cange JOY_SRU)");
        } else {
            eprintf(24, 168, 0, MSQ->col, "SE:ENEMY (Change JOY_SRU)");
        }
        cx = 3;

        eprintf(24, 196, 0, MSQ->col, "--SEQUENCE INFO--");
        k = &w->seq[0].key[cur];
        eprintf(cx * 8, 210, 0, MSQ->col, "Frame:%4.2f [%03d]",
                (f32) (k->frame >> 6) + (f32) (k->frame & 0x3F) / 64.0f, cur);
        eprintf(cx * 8, 224, 0, MSQ->col, "Free :0x%02x", k->Free);
        j = 0;
        for (i = 0; i < 8; i++) {
            eprintf(cx * 8, 238 + j++ * 14, 0, MSQ->col, "Free%1d:%s", i, w->seq[0].flagDisp[i] ? "ON" : "--");
        }
        if (k->Se != 0) {
            eprintf(cx * 8, 350, 0, MSQ->col, "SE   :%02d:0x%02x", k->Se - 1, k->Se - 1);
        } else {
            eprintf(cx * 8, 350, 0, MSQ->col, "SE   :--:----");
        }
        eprintf(184, 350, 0, MSQ->col, "Seq num [%03d/%03d]", cur, m->Motion.Seq_frame_num);
        eprintf(184, 336, 0, MSQ->col, "Mot num [%03d]", (int) m->Motion.Mot_frame_max + 1);
        k = &w->seq[0].copy;
        cx = 48;
        eprintf(cx * 8, 224, 0, MSQ->col, "--COPY SEQUENCE--");
        j = 0;
        for (i = 0; i < 8; i++) {
            eprintf(cx * 8, 238 + j++ * 14, 0, MSQ->col, "Free%1d:%s", i, w->seq[0].copyFlagDisp[i] ? "ON" : "--");
        }
        if (k->Se != 0) {
            eprintf(cx * 8, 350, 0, MSQ->col, "SE   :%02d:0x%02x", k->Se - 1, k->Se - 1);
        } else {
            eprintf(cx * 8, 350, 0, MSQ->col, "SE   :--:----");
        }
        eprintf(384, 28, 0, MSQ->col, "  SLL: 1 STEP");
        eprintf(384, 42, 0, MSQ->col, "A+SLL: SKIP");
        eprintf(384, 56, 0, MSQ->col, "---------------");
        eprintf(384, 70, 0, MSQ->col, "LU:    UP");
        eprintf(384, 84, 0, MSQ->col, "LD:    DOWN");
        eprintf(384, 98, 0, MSQ->col, "LR:    FLAG ON");
        eprintf(384, 112, 0, MSQ->col, "LL:    FLAG OFF");
        eprintf(384, 126, 0, MSQ->col, "---------------");
        eprintf(384, 140, 0, MSQ->col, "L+Y:  Add frame");
        eprintf(384, 154, 0, MSQ->col, "L+R+X:Del frame");
        eprintf(384, 168, 0, MSQ->col, "---------------");
        eprintf(384, 182, 0, MSQ->col, "R+Z:  SEQ COPY");
        eprintf(384, 196, 0, MSQ->col, "L+Z:  SEQ PASTE");
        eprintf(384, 210, 0, MSQ->col, "L+R+Z:SEQ CUT");
        eprintf(384, 84, 0, MSQ->col, "ST:   1P CAMERA");
        if (pG->Frame_cnt & 8) {
            eprintf(16, msq_y_tbl[MSQ->sub2], 0, MSQ->col, ">");
        }
        break;
    }
    }
    Draw_floor(500, 20, 0x00202020);
    if (m != NULL && (m->be_flag & 1) && MSQ->mode > 2) {
        u8 k80 = 0x80;
        u8 k40 = 0x40;
        GXColor c1 = {k80, 0x80, 0x80, 0x40};
        GXColor c2 = {k40, 0x40, 0x40, 0x40};
        GXColor c3 = {0, 0, 0, 0x40};
        f32 x;
        f32 tx;  // the cursor tiles' x: a separate single-set variable (haifa's birthing boost issues its load first)

        TprimDraw2D(0);
        tx = 248.0f;
        rc.x = tx;
        rc.y = 362.0f;
        rc.w = 17.0f;
        rc.h = 73.0f;
        TprimDrawTile2D(&rc, 0.0f, &c1);
        if (w->seq[0].cursor != 0) {
            rc.x = tx;
            rc.y = (f32) (w->seq[0].cursor * 5 + 362);
            rc.w = 17.0f;
            if (w->seq[0].cursor == 9) {
                rc.h = 28.0f;
            } else {
                rc.h = 8.0f;
            }
            col.r = 0x20;
            col.g = 0x20;
            col.b = 0x80;
            col.a = 0x40;
            TprimDrawTile2D(&rc, 0.0f, &col);
        }
        x = 40.0f;
        rc.w = 13.0f;
        y0 = (s16) m->Motion.Seq_frame - 14;
        f32 rowStep = 5.0f;  // a variable set before the loop: its load precedes every loop.c hoist (369.0 after it)
        const f32 rowH = 4.0f;
        const f32 seH = 24.0f;
        const f32 colStep = 25.0f;
        const f32 xStep = 15.0f;
        for (c = 0; c <= 28; c++) {
            u8 bit = 1;
            f32 rowY = 369.0f;

            rc.x = x;
            rc.h = rowH;
            rc.y = rowY;
            for (r = 0; r < 8; r++) {
                if (y0 < 0 || y0 >= m->Motion.Seq_frame_num) {
                    col = c3;
                } else if (w->seq[0].key[y0].Free & bit) {
                    col.r = 0x20;
                    col.g = 0x80;
                    col.b = 0x20;
                    col.a = 0x40;
                } else {
                    col = c2;
                }
                TprimDrawTile2D(&rc, 0.0f, &col);
                bit <<= 1;
                rc.y += rowStep;
            }
            rc.h = seH;
            if (y0 < 0 || y0 >= m->Motion.Seq_frame_num) {
                col = c3;
            } else if (w->seq[0].key[y0].Se != 0) {
                col.r = 0x80;
                col.g = 0x20;
                col.b = 0x20;
                col.a = 0x40;
            } else {
                col = c2;
            }
            TprimDrawTile2D(&rc, 0.0f, &col);
            y0++;
            rc.y += colStep;
            x += xStep;
        }
    }
}

// Reads the .seq file (count + keys) into the work; 1 when more than the header was read.
int msqLoadFile()
{
    return (u32) HDRead(MSQ->fileName, MSQ) > 3;
}

// Writes the count + keys to the .seq file.
int msqSaveFile()
{
    return HDWrite(MSQ->fileName, MSQ, MSQ->seq[0].num * 4 + 4);
}

// START toggles camera mode: the debug camera takes pad 1 and the tool's pad copy is cleared.
void msqCameraMove()
{
    MSQ->joy = Joy[0];
    if (Joy[0].trg & 0x1000) {
        MSQ->camMode ^= 1;
    }
    if (MSQ->camMode) {
        MSQ->joy.trg = 0;
        MSQ->joy.on = 0;
        MSQ->joy.rep = 0;
        MSQ->joy.rep2 = 0;
        DbgFlagOn(pG, DBG_DBG_CAM);
        CamDbg.move(&pG->Camera, Joy, 0);
    }
}

// Shows "Sequence frame > / < Motion frame !!!" for errTimer frames after a mismatch.
void msqErrorMessage()
{
    if (MSQ->errTimer != 0) {
        MSQ->errTimer--;
        switch (MSQ->errType) {
        case 0:
            break;
        case 1:
            eprintf(80, 70, 6, MSQ->col, "Sequence frame > Motion frame !!!");
            break;
        case 2:
            eprintf(80, 70, 4, MSQ->col, "Sequence frame < Motion frame !!!");
            break;
        }
    }
}

// Flags the error message when a key lies beyond the motion length.
void msqFrameSizeCk()
{
    MsqWork* w = MSQ;
    cModel* m = dbModSlot[0].pModel;
    int i;
    int max;

    if (w->seq[0].reverse & 1) {
        msqSetMode(3);
        return;
    }
    max = (int) m->Motion.Mot_frame_max;
    max <<= 6;
    i = 0;
    if (i < w->seq[0].num) {
        do {
            if (w->seq[0].key[i].frame > max) {
                MSQ->errTimer = 150;
                MSQ->errType = 1;
                break;
            }
            i++;
        } while (i < w->seq[0].num);
    }
}
