#ifndef DB_MOD_H
#define DB_MOD_H

#include "types.h"
#include "model.h"

// Model viewer of the debug tools (Tools/t_esp db_mod.cpp). The entry points have C linkage in the .sym.
// The data views below are what t_mv addresses; db_mod.cpp owns the real definitions.

// One viewer slot (0x2754 bytes): the model pointer at 0x04.
struct DbModSlot {
    u32 x0;
    cModel* pModel;  // 0x04
    u8 pad_8[0x148 - 8];
    u16 seqFlag;     // 0x148  MotionSetCore flag word of the sequence (dbModMotionSetSeq; t_motseq copies its view flag here)
    u8 pad_14A[0x2754 - 0x14A];
};

// Viewer state (pointer at Tools .bss 0x13AAE8); 0x3449 is the "model set loaded" byte t_mv tests.
struct DbModState {
    u8 pad_0[0x3449];
    u8 loaded;  // 0x3449
};

// File list handed to dbModelLoad (t_esp db_mod.cpp, 0x85C bytes: names / data pointers of the
// bin, tpl and extra files of one event model). The constructor only inits; db_port.cpp keeps
// three as function statics (their guard words follow each one in .bss).
class DB_MODEL_FILES {
public:
    u8 pad[0x85C];

    DB_MODEL_FILES() { init(); }
    void init();
    void set(int no, char* name);
    void set(int no, void* data);
    void append(char* name);
    void append(void* data);
    int read(int no);
};

extern DbModSlot dbModSlot[64];  // Tools .bss 0xE8
extern DbModState* pDbModState;  // Tools .bss 0x13AAE8

extern "C" {
void dbModelInit();
void dbModelQuit();
int dbModel(int mode);
void dbModMotionMove();
u32 dbModGetViewFlag();
void dbModSetViewFlag(u32 flag);
void dbModUnsetViewFlag(u32 flag);
// Plays sequence `seq` (u16 count + MotionSeqKey[]) on slot `slot` from key `no`.
void dbModMotionSetSeq(int slot, void* seq, int flag, int no);
// Copies the motion file name of slot `slot` into `dst`.
void dbModGetMotFilename(int slot, char* dst);
// Slot access and the model set loader (db_port.cpp drives them for the effect tool).
class cEm* dbModGetEmPtr(u32 no);
int dbModelIsAlive(int no);
char* dbModBinName();
void dbModMotionSet(int frame);
void dbModelSetCamera(int no, struct CAMERA* cam);
int dbModelLoad(int no, DB_MODEL_FILES* bin, DB_MODEL_FILES* tex, DB_MODEL_FILES* mot);
void dbModelParentChild(s8 no, s8 parentNo, s8 parts, Vec* pos, Vec* rot);
void dbModelSetPos0(int no, Vec* pos);
void dbModelSetAng0(int no, Vec* rot);
int LoadModelSetName(char* name, int motNum, int no);
void SetLoopFlag(int on, int no);
void SetTransMode(int mode, int no);
void SetXFlipFlag(int on, int no);
}

#endif
