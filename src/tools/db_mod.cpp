#include "types.h"
#include "light.h"
#include "em.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "file.h"
#include "main_mem.h"
#include "db_log.h"
#include "math_sub.h"
#include "motion.h"
#include "dbmodule.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "db_light.h"
#include "dolphin/mtx.h"
#include "scheduler.h"
#include <stdio.h>
#include <dolphin/os.h>
#include <string.h>

#line 30 "D:/Bio4/Prog/db_mod.cpp"

// Debug model viewer (D:/Bio4/Prog/db_mod.cpp): one object shared by Tools and t_esp. The original REL
// link dead-stripped it at function level (modules.py STRIP_UNUSED): Tools lost DB_MODEL_FILES::append,
// dbModMotionSet and the dbModBinName..dbModelSetAng0 loader entry points, t_esp lost the view-flag
// getters, dbModMotionSetSeq and dbModGetMotFilename. .rodata/.data are the same bytes in both.

#define FILE_NUM 16
#define NAME_LEN 0x80
#define SLOT_NUM 64

// wrap / clamp helpers (the option pages)
#define LOOP(x, lo, hi) (((x) < (lo)) ? (hi) : ((x) > (hi)) ? (lo) : (x))

// The motion work of this build: model.h's MotionWork without the trailing blend/flip/blendTbl pointers
// (0xD0 bytes; em.h MotionWorkSub is the same block).
struct DbMotWork {
    MotionData* data;     // 0x00
    u32* keyTbl;          // 0x04
    u16 hist[2][2][3];    // 0x08
    f32 maxFrame;         // 0x20
    f32 frame;            // 0x24
    f32 prevFrame;        // 0x28
    f32 prevFrame2;       // 0x2C
    u8 nParts;            // 0x30
    u8 pad_31[3];
    u8* partsNo;          // 0x34
    u16* partsInfo;       // 0x38
    u16 rootPosIdx;       // 0x3C
    u16 rootRotIdx;       // 0x3E
    u16 flags;            // 0x40
    u16 state;            // 0x42
    u32 flags2;           // 0x44
    Vec pos;              // 0x48
    Vec posPrev;          // 0x54
    Vec posDelta;         // 0x60
    Vec basePos;          // 0x6C
    Vec speed;            // 0x78
    Vec rot;              // 0x84
    Vec rotPrev;          // 0x90
    Vec rotDelta;         // 0x9C
    MotionSeqKey* seq;    // 0xA8
    MotionSeqKey key0;    // 0xAC
    MotionSeqKey key1;    // 0xB0
    MotionSeqKey key2;    // 0xB4
    f32 seqFrame;         // 0xB8
    u16 seqMax;           // 0xBC
    u8 pad_BE[2];
    f32 speedRate;        // 0xC0
    u8 hokanMax;          // 0xC4
    u8 hokanCnt;          // 0xC5
    u8 pad_C6[2];
    f32 blendRate;        // 0xC8
    AttachCamera* cam;    // 0xCC
};

// File list of one model / texture / motion set (0x85C): names or data pointers, the load state per entry.
class DB_MODEL_FILES {
public:
    int m_mode;                       // 0x000  1 = names (HD read), 2 = data pointers
    int m_counter;                        // 0x004  next entry read() handles
    u8 m_num;                         // 0x008
    char m_name[FILE_NUM][NAME_LEN];  // 0x009
    s8 m_read_state[FILE_NUM];              // 0x809  0 empty, 1 loaded, 2 not found, 3 new name
    void* m_data[FILE_NUM];           // 0x81C

    DB_MODEL_FILES() { init(); }
    void init();
    void set(u8 num, char* names);
    void set(u8 num, void** data);
    int append(char* name);
    int append(void* data);
    int read(void** dst);
};

// One viewer slot (0x2754).
class DB_EM {
public:
    s8 m_load_model_rno;                    // model load step
    s8 m_load_motion_rno;                    // motion load step
    u8 alive;                 // 0x002  model loaded
    u8 Be_flag;
    cEm* pEm;                 // 0x004
    Vec pos0;                  // 0x008
    Vec ang0;                  // 0x014
    s16 x20;                  // 0x020
    s16 x22;                  // 0x022
    s8 type;                   // 0x024
    s8 lit_type;             // 0x025  0 pl / 1 em / 2 obj / 3 scr / 4 item / 5 none
    char label[0x22];          // 0x026  set name (dbModBinName)
    void* pBinBuff[FILE_NUM];      // 0x048
    void* pTplBuff[FILE_NUM];      // 0x088
    void* pMotBuff[FILE_NUM];  // 0x0C8
    DbMotWork motInfo[FILE_NUM];  // 0x108
    s8 motStat[FILE_NUM];     // 0xE08
    u8 motFlag[FILE_NUM];     // 0xE18
    s8 mot_num;                  // 0xE28
    s8 mot_cnt;                  // 0xE29
    u16 setNo;                // 0xE2A
    cEm* pEm_parent;              // 0xE2C  parent model (setParent)
    s8 parentNo;                    // 0xE30  slot number / parent slot
    u8 xE31;
    s16 partsNo;          // 0xE32  parts of the parent this model hangs on (-1: the model itself)
    u8 opt_flag;                  // 0xE34  bit0: draw the skeleton
    u8 pad_E35[3];
    u32 em_flag;                 // 0xE38
    s16 info_parts_no;              // 0xE3C  P_INFO parts cursor
    u8 pad_E3E[2];
    DB_MODEL_FILES m_files[3];  // 0xE40 bin, 0x169C tex, 0x1EF8 mot (one array: the static ctor loops 3x64)

    int loadModelSet(DB_MODEL_FILES* bin, DB_MODEL_FILES* tex);
    int loadModel();
    int loadMotionSet(DB_MODEL_FILES* mot);
    int loadMotion();
    void setLight();
    void IKreport();
    void setParent(s8 parentNo, s16 parts, Vec* pos, Vec* rot);
};

// Parsed mot_tbl.txt (0x560): the image bounds, the unit pointers and the five label buffers.
struct MotTbl {
    char* data;         // 0x000
    char* end;          // 0x004
    char* cur;          // 0x008
    int xC;             // 0x00C
    int unitNum[5];     // 0x010  blocks per unit (0 set, 1 bin, 2 tex, 3 motion, 4 locate)
    int count[5];       // 0x024  entries of the current block per unit
    int x38[5];         // 0x038
    char* unit[5];      // 0x04C  unit pointers per type
    char name[5][0x100];  // 0x060
};

// Viewer state (0x3460).
struct DbModState {
    u8 mode;               // 0x000  0 menu, 1 tool page, 2 tool menu (path)
    u8 step;               // 0x001
    u8 x2;                 // 0x002
    u8 x3;
    s8 page;               // 0x004
    s8 cursor;             // 0x005
    s8 sub;                // 0x006  model page: 0 model set / 1 motion
    s8 x7;                 // 0x007  LOCATE page: axis cursor
    s8 pathCursor;         // 0x008
    u8 pad_9[7];
    s8 no;                 // 0x010  slot
    u8 prevNo;             // 0x011
    u16 prevSetNo;         // 0x012
    u8 x14;
    u8 loopFlag;           // 0x015  LOOP page: motion flags bit 2 (loop) on / off
    s8 transMode;          // 0x016  TRANS page: 0 on / 1 off / 2 add / 3 inf
    u8 x17;
    u8 flipFlag;           // 0x018  FLIP page
    u8 x19;
    s8 playMode;           // 0x01A  0 play / 1 step / 2 reverse
    s8 type;               // 0x01B  model type filter (dbmodTypeName)
    s16 setNo;             // 0x01C
    s16 binNo;             // 0x01E
    s16 texNo;             // 0x020
    s16 motNo[FILE_NUM];   // 0x022
    s16 motSub[FILE_NUM];  // 0x042
    s16 motNum[FILE_NUM];  // 0x062
    s16 locNo;             // 0x082
    s8 digit;              // 0x084  cursor digit of the motion number
    s8 digits[FILE_NUM];   // 0x085  '#' count in the motion file name
    u8 pad_95[3];
    int hashOfs[FILE_NUM]; // 0x098  offset of the '#' run
    u16 curSetNo;          // 0x0D8
    u16 curBinNo;          // 0x0DA
    u16 curTexNo;          // 0x0DC
    u16 curMotNo[FILE_NUM];   // 0x0DE
    u16 curMotSub[FILE_NUM];  // 0x0FE
    u16 curMotNum[FILE_NUM];  // 0x11E
    s8 binNum;             // 0x13E
    s8 texNum;             // 0x13F
    u8 motFileNum;         // 0x140
    char setName[0x20];    // 0x141
    char binDir[0x20];     // 0x161
    char texDir[0x20];     // 0x181
    char motDir[FILE_NUM][0x20];       // 0x1A1
    char name[2][FILE_NUM][NAME_LEN];  // 0x3A1  [0] bin, [1] tex
    char motName[FILE_NUM][NAME_LEN];  // 0x13A1
    u8 pad_1BA1[0x1BC1 - 0x1BA1];
    s8 motType[FILE_NUM];  // 0x1BC1
    u8 pad_1BD1[0x1C04 - 0x1BD1];
    long locParts;         // 0x1C04
    Vec locPos;            // 0x1C08
    Vec locRot;            // 0x1C14
    u8 pad_1C20[0x1C44 - 0x1C20];
    s8 locBinNum;          // 0x1C44
    s8 locTexNum;          // 0x1C45
    u8 x1C46;
    char locName[2][FILE_NUM][NAME_LEN];  // 0x1C47  [0] bin, [1] tex
    u8 pad_2C47[0x3448 - 0x2C47];
    u8 timer;              // 0x3448
    u8 loadFail;           // 0x3449
    u8 pad_344A[2];
    int blendMode;         // 0x344C  0 blend / 1 add / 2 none
    f32 blendRate;         // 0x3450
    f32 scale;             // 0x3454
    s8 lightMode;          // 0x3458  0 default, 1 room, 2..5 stage presets
    s8 path;               // 0x3459
    u8 curPath;            // 0x345A
    u8 x345B;
    u32 viewFlag;          // 0x345C
};

DB_EM dbModSlot[SLOT_NUM];
DB_EM dbModSlotSub[SLOT_NUM];
DbModState* pDbModState;
MotTbl* m_MotTbl;

static void* dbmodMotTblImage = {0};
static int dbmodLoopNum = 0;

typedef int (*DbModFunc)();

static int dbmod_no();
static int dbmod_model();
static int dbmod_motion();
static int dbmod_locate();
static int dbmod_trans();
static int dbmod_loop();
static int dbmod_play();
static int dbmod_flip();
static int dbmod_blend();
static int dbmod_except();
static int dbmod_light();
static int dbmod_option();
static int dbmod_scale();
static int dbmod_p_info();
static int dbmod_null();

static DbModFunc dbmodFunc[] = {
    dbmod_no,    dbmod_model, dbmod_motion, dbmod_locate, dbmod_trans,  dbmod_loop,   dbmod_play,   dbmod_flip,
    dbmod_blend, dbmod_except, dbmod_light, dbmod_option, dbmod_scale,  dbmod_p_info, dbmod_null,
};

struct DbModMenu {
    const char* name;
    int id;
    int flag;   // needs a loaded model
};

static DbModMenu dbmodMenu[2][8] = {
    {{"NO    ", 0, 0}, {"MODEL ", 1, 0}, {"MOTION", 2, 1}, {"TRANS ", 4, 1}, {"LOOP  ", 5, 1}, {"PLAY", 6, 0}, {"BLEND ", 8, 1}, {"LOCATE", 3, 0}},
    {{"FLIP  ", 7, 1}, {"LIGHT ", 0xA, 0}, {"SCALE ", 0xC, 1}, {"P_INFO", 0xD, 1}, {"OPTION", 0xB, 0}},
};
static s8 dbmodMenuNum[3] = {8, 5, 8};   // [2] = rows drawn per page

extern "C" {
void dbModelInit();
void dbModelQuit();
int dbModel(int mode);
void dbModMotionMove();
void dbModSetViewFlag(u32 flag);
void dbModelSetCamera(int no, CAMERA* cam);
void dbmodGetSet();
void dbmodGetFilenames();
void dbmodGetLabel(int no, char* dst);
void dbmodDispModelName();
void dbmodInfoDisp();
void dbModPlayMode();
char* dbmodSkipPath(char* path);
void dbmodSetLight(DB_EM* em);
void drawOrientation(cParts* p);
void model_usage();
void motion_usage();
void blend_usage();
void position_usage(int mode);
void rotation_usage();
void scale_usage();
void mottblInit(MotTbl* tbl);
char* mottblGetLine(char* dst, int max, char* src);
char* mottblNextLine(char* p);
int mottblUnitCount(char* p);
int mottblUnitNum(char* p, const char* name);
char* mottblUnitPtr(char* p, int no);
void dbmodFilePath(u8 path);
void dbModMotionSet(int frame);
char* dbModBinName();
int GetSlctSetNo(char* name);
int LoadModelSetName(char* name, int motNum, int no);
void SetTransMode(int mode, int no);
void SetLoopFlag(int on, int no);
void SetXFlipFlag(int on, int no);
cEm* dbModGetEmPtr(u32 no);
void dbModelParentChild(s8 no, s8 parentNo, s8 parts, Vec* pos, Vec* rot);
int dbModelLoad(int no, DB_MODEL_FILES* bin, DB_MODEL_FILES* tex, DB_MODEL_FILES* mot);
int dbModelIsAlive(int no);
void dbModelSetPos0(int no, Vec* pos);
void dbModelSetAng0(int no, Vec* rot);
u32 dbModGetViewFlag();
void dbModUnsetViewFlag(u32 flag);
void dbModMotionSetSeq(int slot, void* seq, int flag, int no);
void dbModGetMotFilename(int slot, char* dst);
}
int SetToolLight(int no);  // db_light_tools.cpp / db_light_esp.cpp

// Sets viewer flag bits (bit0 parent/child link set, bit1 t_motseq's reverse toggle, bit2 no
// position wrap, bit3 wrap the model position at +-50000 instead of +-10000 units).
void dbModSetViewFlag(u32 flag)
{
    pDbModState->viewFlag |= flag;
}

// The viewer flag word.
u32 dbModGetViewFlag()
{
    return pDbModState->viewFlag;
}

// Clears viewer flag bits.
void dbModUnsetViewFlag(u32 flag)
{
    pDbModState->viewFlag &= ~flag;
}

// Resets viewer slots start..end: frees their model / texture / motion buffers, clears the motion
// works (motion 0 gets flags 0x15, an attach camera set from the game camera, speed 1), no parent.
void init_dbEm(DB_EM* em, int start, int end)
{
    int n, i;

    for (n = start; n <= end; em++, n++) {
        em->alive = 0;
        EmMgr.destroyAll();
        em->pEm = 0;
        for (i = 0; i < FILE_NUM; i++) {
            if (em->pBinBuff[i]) {
                Debug_free(em->pBinBuff[i]);
            }
            if (em->pTplBuff[i]) {
                Debug_free(em->pTplBuff[i]);
            }
            em->pBinBuff[i] = 0;
            em->pTplBuff[i] = 0;
        }
        for (i = 0; i <= FILE_NUM - 1; i++) {
            if (em->pMotBuff[i]) {
                Debug_free(em->pMotBuff[i]);
            }
            em->pMotBuff[i] = 0;
            memclr_asm(&em->motInfo[i], sizeof(DbMotWork));
            em->motStat[i] = em->motFlag[i] = 0; // chain: motStat's address first, motFlag's store first
        }
        em->mot_num = 0;
        em->mot_cnt = 0;
        em->motInfo[0].flags = 0x15;
        em->motInfo[0].cam = (AttachCamera*) mem_alloc(sizeof(AttachCamera), __FILE__, 0xEB, 1, 13);
        dbModelSetCamera(n, &pG->Camera);
        em->motInfo[0].speedRate = 1.0f;
        em->parentNo = n;
        // parent (the SI zero) before the name byte: the QI store then takes the wider zero's lowpart
        em->pEm_parent = 0;
        em->partsNo = 0;
        em->label[0] = 0;
    }
}

// Empty (the PATH selector of the tool menu has no effect in this build).
void dbmodFilePath(u8 path)
{
}

// Viewer start: allocates the state, reads Room/Em/mot_tbl.txt from the host (loadFail when
// missing), resets both slot banks, parses the table (mottblInit) and loads the first set's names.
void dbModelInit()
{
    int size;

    pDbModState = (DbModState*) Debug_alloc(sizeof(DbModState), 1);
    memclr_asm(pDbModState, sizeof(DbModState));
    size = HDReadDebugAlloc("Room/Em/mot_tbl.txt", &dbmodMotTblImage, 1);
    if (size == 0) {
        pDbModState->loadFail = 1;
        return;
    }
    init_dbEm(dbModSlot, 0, SLOT_NUM - 1);
    init_dbEm(dbModSlotSub, SLOT_NUM, SLOT_NUM * 2 - 1);
    pDbModState->no = 0;
    pDbModState->blendMode = 2;
    pDbModState->loopFlag = 1;
    pDbModState->scale = 1.0f;
    pDbModState->type = 0;
    pDbModState->lightMode = 0;
    m_MotTbl = (MotTbl*) Debug_alloc(sizeof(MotTbl), 1);
    m_MotTbl->data = (char*) dbmodMotTblImage;
    m_MotTbl->end = (char*) dbmodMotTblImage + (size - 1);
    m_MotTbl->cur = m_MotTbl->data;
    mottblInit(m_MotTbl);
    if (m_MotTbl->unitNum[0]) {
        pDbModState->curSetNo = -1;
        dbmodGetSet();
    }
}

// Viewer end: destroys the slot enemies (cEm) and frees the table image, MotTbl and state.
void dbModelQuit()
{
    int i;

    memclr_asm(pDbModState, sizeof(DbModState));
    for (i = 0; i < SLOT_NUM; i++) {
        if (dbModSlot[i].pEm && dbModSlot[i].pEm->isAlive()) {
            EmMgr.destroy(dbModSlot[i].pEm);
        }
        if (dbModSlotSub[i].pEm && dbModSlotSub[i].pEm->isAlive()) {
            EmMgr.destroy(dbModSlotSub[i].pEm);
        }
    }
    if (dbmodMotTblImage) {
        Debug_free(dbmodMotTblImage);
        dbmodMotTblImage = 0;
    }
    if (m_MotTbl) {
        Debug_free(m_MotTbl);
        m_MotTbl = 0;
    }
    if (pDbModState) {
        Debug_free(pDbModState);
        pDbModState = 0;
    }
}

// Name of model set `no` (first word of its _SET line).
void dbmodGetLabel(int no, char* dst)
{
    char line[0x100];

    mottblGetLine(line, 0x100, mottblUnitPtr(m_MotTbl->unit[0], no));
    sscanf(line, "%s", dst);
}

// Selects model set pDbModState->setNo: reads its _SET block (set name, then the bin / tex / motion
// / locate unit names), resolves the unit numbers and pulls the file names (dbmodGetFilenames);
// motion numbers reset to 0 (or -1 without motions).
void dbmodGetSet()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    char line[0x100];
    s16 dummy;
    char* p;
    char* c;
    int i;
    s16* pNo = 0;
    int t = 0;

    if (pDbModState->curSetNo == (u16) pDbModState->setNo) {
        return;
    }
    em->setNo = pDbModState->setNo;
    p = mottblUnitPtr(m_MotTbl->unit[0], pDbModState->setNo);
    p = mottblGetLine(line, 0x100, p);
    sscanf(line, "%s", m_MotTbl->name[0]);
    strcpy(pDbModState->setName, m_MotTbl->name[0]);
    p = mottblUnitPtr(p, 0);
    p = mottblGetLine(line, 0x100, p);
    m_MotTbl->name[1][0] = 0;
    m_MotTbl->name[2][0] = 0;
    m_MotTbl->name[3][0] = 0;
    m_MotTbl->name[4][0] = 0;
    sscanf(line, "%s%s%s%s", m_MotTbl->name[1], m_MotTbl->name[2], m_MotTbl->name[3], m_MotTbl->name[4]);
    for (i = 1; i <= 4; i++) {
        c = strchr(m_MotTbl->name[i], ',');
        if (c) {
            *c = 0;
        }
    }
    for (i = 0; i <= 3; i++) {
        switch (i) {
        case 0:
            t = 1;
            pNo = &pDbModState->binNo;
            break;
        case 1:
            t = 2;
            pNo = &pDbModState->texNo;
            break;
        case 2:
            t = 3;
            pNo = &pDbModState->motNo[0];
            break;
        case 3:
            t = 4;
            pNo = &pDbModState->locNo;
            if (strlen(m_MotTbl->name[4]) == 0) {
                pDbModState->locNo = -1;
                pNo = &dummy;
            }
            break;
        }
        if (i == 2) {
            if (strncmp(m_MotTbl->name[t], "null", 4) == 0 || strncmp(m_MotTbl->name[t], "NULL", 4) == 0) {
                pDbModState->motNo[0] = -1;
                pDbModState->motSub[0] = 0;
                pDbModState->motNum[0] = -1;
                pDbModState->motFileNum = 0;
                break;
            }
            pDbModState->motSub[0] = 0;
            pDbModState->motNum[0] = 0;
            pDbModState->motFileNum = 1;
        }
        *pNo = mottblUnitNum(m_MotTbl->unit[t], m_MotTbl->name[t]);
        mottblNextLine(mottblUnitPtr(m_MotTbl->unit[t], *pNo));
    }
    for (i = 1; i < FILE_NUM; i++) {
        pDbModState->motNo[i] = pDbModState->motNo[0];
        pDbModState->motSub[i] = pDbModState->motSub[0];
        pDbModState->motNum[i] = -1;
    }
    dbmodGetFilenames();
}

// Empty file list.
void DB_MODEL_FILES::init()
{
    int i;

    m_mode = 0;
    m_num = 0;
    for (i = 0; i < FILE_NUM; i++) {
        m_name[i][0] = 0;
        m_read_state[i] = 0;
        m_data[i] = 0;
    }
}

// Fills the list with `num` host file names (NAME_LEN apart); changed names are marked for
// re-reading (state 3).
void DB_MODEL_FILES::set(u8 num, char* names)
{
    int i;

    m_mode = 1;
    if (num > FILE_NUM) {
        pLog->err(0, 0, "DB_MOD_FILES::set(): num (= %d) > FILE_NUM (= %d)", num, FILE_NUM);
        return;
    }
    m_num = num;
    for (i = 0; i < m_num; i++) {
        if (strcmp(m_name[i], names + i * NAME_LEN) != 0) {
            strcpy(m_name[i], names + i * NAME_LEN);
            m_read_state[i] = 3;
        }
    }
    m_counter = 0;
}

// Appends one host file name (marked new); 0 when full or the name is too long.
int DB_MODEL_FILES::append(char* name)
{
    m_mode = 1;
    if (m_num > FILE_NUM) {
        pLog->err(0, 0, "DB_MOD_FILES::apend(): m_num (= %d) > FILE_NUM (= %d)", m_num, FILE_NUM);
        return 0;
    }
    if (strlen(name) > NAME_LEN - 1) {
        pLog->err(0, 0, "DB_MOD_FILES::append(): strlen(%s) is longer than %d", name, NAME_LEN);
        return 0;
    }
    strcpy(m_name[m_num], name);
    m_read_state[m_num] = 3;
    m_counter = 0;
    m_num++;
    return 1;
}

// Loads the next entry into dst[i]: type 1 reads the host file (Debug heap; state 1 loaded, 2 not
// found), type 2 takes the data pointer. Returns 2 while entries remain, 0 when done, 1 on a
// missing file.
int DB_MODEL_FILES::read(void** dst)
{
    int i;
    int type;

    if (m_num == 0) {
        return 0;
    }
    i = m_counter;
    type = m_mode;
    m_counter = i + 1;
    switch (type) {
    case 1:
        if (m_read_state[i] == 1) {
            break;
        }
        if (dst[i]) {
            Debug_free(dst[i]);
            dst[i] = 0;
        }
        if (m_name[i][0] == 0) {
            m_read_state[i] = 0;
            break;
        }
        if (HDReadDebugAlloc(m_name[i], &dst[i], 1) == 0) {
            m_read_state[i] = 2;
            {
                char* s = dbmodSkipPath(m_name[i]);
                pLog->err(0, 0, "%s: not found ... (X X)", s);
            }
            return 1;
        }
        m_read_state[i] = type;
        break;
    case 2:
        if (dst[i]) {
            Debug_free(dst[i]);
            dst[i] = 0;
        }
        dst[i] = m_data[i];
        break;
    }
    return (m_counter <= m_num - 1) ? 2 : 0;
}

// Fills the list with `num` in-memory data pointers.
void DB_MODEL_FILES::set(u8 num, void** data)
{
    int i;

    m_mode = 2;
    if (num > FILE_NUM) {
        pLog->err(0, 0, "DB_MOD_FILES::set(): num (= %d) > FILE_NUM (= %d)", num, FILE_NUM);
        return;
    }
    m_num = num;
    for (i = 0; i < m_num; i++) {
        m_data[i] = data[i];
    }
    m_counter = 0;
}

// Appends one data pointer; 0 when full.
int DB_MODEL_FILES::append(void* data)
{
    m_mode = 2;
    if (m_num > FILE_NUM) {
        pLog->err(0, 0, "DB_MOD_FILES::apend(): m_num (= %d) > FILE_NUM (= %d)", m_num, FILE_NUM);
        return 0;
    }
    m_data[m_num] = data;
    m_counter = 0;
    m_num++;
    return 1;
}

// Builds the current set's file names from the table: bin and tex units (directory line, then one
// name per line into name[0]/name[1]), the motion units (directory, name pattern with a '#' run
// standing for the motion number, its position and digit count), and the locate unit's bin/tex
// names (locName).
void dbmodGetFilenames()
{
    char line[0x100];
    char name[5][0x100];
    char num[8];
    int t = 0;
    int i, k;
    char* p;
    char* q;
    char* s;
    char* c;
    s16* pNo = 0;
    s8* pNum = 0;
    char* dir = 0;
    s8 skip;
    s16 no;
    int digits;

    for (i = 0; i <= 1; i++) {
        switch (i) {
        case 0:
            t = 1;
            pNo = &pDbModState->binNo;
            pNum = &pDbModState->binNum;
            dir = pDbModState->binDir;
            break;
        case 1:
            t = 2;
            pNo = &pDbModState->texNo;
            pNum = &pDbModState->texNum;
            dir = pDbModState->texDir;
            break;
        }
        p = mottblUnitPtr(m_MotTbl->unit[t], *pNo);
        sscanf(p, "%s", dir);
        p = mottblNextLine(p);
        m_MotTbl->count[t] = mottblUnitCount(p);
        *pNum = m_MotTbl->count[t];
        for (k = 0; k < *pNum; k++) {
            q = mottblUnitPtr(p, k);
            mottblGetLine(line, 0x100, q);
            skip = strspn(line, "\t ");
            strcpy(pDbModState->name[i][k], line + skip);
        }
    }
    pDbModState->motFileNum = i = 0;
    for (; i < FILE_NUM; i++) {
        if (pDbModState->motNo[i] == -1) {
            pDbModState->motName[i][0] = 0;
        } else {
            p = mottblUnitPtr(m_MotTbl->unit[3], pDbModState->motNo[i]);
            sscanf(p, "%s", pDbModState->motDir[i]);
            p = mottblNextLine(p);
            m_MotTbl->count[3] = mottblUnitCount(p);
            q = mottblUnitPtr(p, pDbModState->motSub[i]);
            mottblGetLine(line, 0x100, q);
            skip = strspn(line, "\t ");
            strcpy(pDbModState->motName[i], line + skip);
            s = strstr(pDbModState->motName[i], "#");
            pDbModState->hashOfs[i] = s - pDbModState->motName[i];
            if (s) {
                pDbModState->digits[i] = strspn(s, "#");
            } else {
                pDbModState->digits[i] = (s8) (int) s;
            }
            if (pDbModState->digits[i] != 0) {
                if (pDbModState->motNum[i] > 0) {
                    digits = (int) log10((f64) pDbModState->motNum[i]) + 1;
                } else {
                    digits = 1;
                }
                sprintf(num, "%d", pDbModState->motNum[i]);
                for (k = 0; k < pDbModState->digits[i]; k++) {
                    if (k < pDbModState->digits[i] - digits) {
                        s[k] = '0';
                    } else {
                        s[k] = num[k - (pDbModState->digits[i] - digits)];
                    }
                }
            }
            if (pDbModState->motNum[i] == -1) {
                pDbModState->motName[i][0] = 0;
            }
        }
        pDbModState->motFileNum++;
    }
    if (pDbModState->locNo != -1) {
        q = mottblUnitPtr(m_MotTbl->unit[4], pDbModState->locNo);
        q = mottblNextLine(q);
        q = mottblGetLine(line, 0x100, q);
        sscanf(line, "%s", name[0]);
        c = strchr(name[0], ',');
        if (c) {
            *c = 0;
        }
        q = mottblGetLine(line, 0x100, q);
        sscanf(line, "%ld", &pDbModState->locParts);
        q = mottblGetLine(line, 0x100, q);
        sscanf(line, "%f,%f,%f", &pDbModState->locPos.x, &pDbModState->locPos.y, &pDbModState->locPos.z);
        mottblGetLine(line, 0x100, q);
        sscanf(line, "%f,%f,%f", &pDbModState->locRot.x, &pDbModState->locRot.y, &pDbModState->locRot.z);
        pDbModState->locRot.x *= DEG2RAD;
        pDbModState->locRot.y *= DEG2RAD;
        pDbModState->locRot.z *= DEG2RAD;
        no = mottblUnitNum(m_MotTbl->unit[0], name[0]);
        q = mottblUnitPtr(m_MotTbl->unit[0], no);
        q = mottblUnitPtr(mottblGetLine(line, 0x100, q), 0);
        mottblGetLine(line, 0x100, q);
        sscanf(line, "%s%s%s", name[1], name[2], name[3]);
        for (i = 1; i <= 3; i++) {
            c = strchr(name[i], ',');
            if (c) {
                *c = 0;
            }
        }
        for (i = 0; i <= 1; i++) {
            switch (i) {
            case 0:
                t = 1;
                pNum = &pDbModState->locBinNum;
                break;
            case 1:
                t = 2;
                pNum = &pDbModState->locTexNum;
                break;
            }
            no = mottblUnitNum(m_MotTbl->unit[t], name[t]);
            p = mottblUnitPtr(m_MotTbl->unit[t], no);
            p = mottblNextLine(p);
            *pNum = mottblUnitCount(p);
            for (k = 0; k < *pNum; k++) {
                q = mottblUnitPtr(p, k);
                mottblGetLine(line, 0x100, q);
                skip = strspn(line, "\t ");
                strcpy(pDbModState->locName[i][k], line + skip);
            }
        }
    } else {
        pDbModState->locBinNum = 0;
        pDbModState->locTexNum = 0;
    }
    pDbModState->curSetNo = pDbModState->setNo;
    pDbModState->curBinNo = pDbModState->binNo;
    pDbModState->curTexNo = pDbModState->texNo;
    for (i = 0; i < FILE_NUM; i++) {
        pDbModState->curMotNo[i] = pDbModState->motNo[i];
        pDbModState->curMotSub[i] = pDbModState->motSub[i];
        pDbModState->curMotNum[i] = pDbModState->motNum[i];
    }
}

// Model viewer frame (pad 1). mode 0 (menu): L/R flip the menu page, up/down pick an entry (entries
// needing a model greyed), A opens it (mode 1), Z the TOOL MENU (path selector, mode 2), B returns
// 2 (leave the viewer). mode 1 runs the page routine (dbmodFunc[id]; 0 when it returns to the menu).
// `mode` != 0 hides the menu (a hosting tool drives the pages itself).
int dbModel(int mode)
{
    JOY* joy = &Joy[0];
    int ret = 0;
    int color;
    int size;

    switch (pDbModState->mode) {
    case 0:
        if (mode == 0) {
            if (joy->trg & 0x200) {
                return 2;
            }
            if (joy->trg & 0x100) {
                pDbModState->mode = 1;
                pDbModState->step = 0;
                break;
            }
            if (joy->trg & 0x60) {
                pDbModState->timer = 8;
            }
            if (joy->trg & 0x40) {
                pDbModState->page--;
            }
            if (joy->trg & 0x20) {
                pDbModState->page++;
            }
            pDbModState->page = LOOP(pDbModState->page, 0, 1);
            if (joy->rep & 0x000C000C) {
                pDbModState->timer = 8;
            }
            if (joy->rep & 0x00080008) {
                pDbModState->cursor--;
            }
            if (joy->rep & 0x00040004) {
                pDbModState->cursor++;
            }
            pDbModState->cursor = CLAMP(pDbModState->cursor, 0, dbmodMenuNum[pDbModState->page] - 1);
            if (joy->trg & 0x10) {
                pDbModState->mode = 2;
                pDbModState->step = 0;
                pDbModState->pathCursor = 0;
                break;
            }
        }
        {
            int i;
            int x = 5;
            for (i = 0; i < dbmodMenuNum[2]; i++) {
                if (dbmodMenu[pDbModState->page][i].flag && dbModSlot[pDbModState->no].pEm == 0) {
                    color = (i == pDbModState->cursor) ? 4 : 7;
                } else {
                    color = (i == pDbModState->cursor) ? 4 : 0;
                }
                if (i < dbmodMenuNum[pDbModState->page]) {
                    eprintf(x * 8, (i + 3) * 14, color, 0, "%s", dbmodMenu[pDbModState->page][i].name);
                } else {
                    eprintf(x * 8, (i + 3) * 14, color, 0, "______");
                }
                if (i == pDbModState->cursor && (pDbModState->timer & 0x18)) {
                    eprintf((x - 1) * 8, (i + 3) * 14, 0x16, 0, ">");
                }
            }
            eprintf(4 * 8, (dbmodMenuNum[2] + 3) * 14, 0, 0, "[Page:%1d/%1d]", pDbModState->page + 1, 2);
            dbmodInfoDisp();
        }
        break;
    case 1:
        ret = dbmodFunc[dbmodMenu[pDbModState->page][pDbModState->cursor].id]();
        break;
    case 2:
        if (joy->rep & 0x000C000C) {
            pDbModState->timer = 8;
        }
        if (joy->trg & 0x00010001) {
            pDbModState->pathCursor--;
        }
        if (joy->trg & 0x00020002) {
            pDbModState->pathCursor++;
        }
        pDbModState->pathCursor = LOOP(pDbModState->pathCursor, 0, 0);
        if (pDbModState->pathCursor == 0) {
            if (joy->trg & 0x00010001) {
                pDbModState->path--;
            }
            if (joy->trg & 0x00020002) {
                pDbModState->path++;
            }
            pDbModState->path = CLAMP(pDbModState->path, 0, 2);
            if (joy->trg & 0x200) {
                pDbModState->mode = 0;
                pDbModState->path = pDbModState->curPath;
            } else if (joy->trg & 0x100) {
                if (pDbModState->curPath != pDbModState->path) {
                    pDbModState->curPath = pDbModState->path;
                    dbmodFilePath(pDbModState->path);
                    if (dbmodMotTblImage) {
                        Debug_free(dbmodMotTblImage);
                        dbmodMotTblImage = 0;
                    }
                    size = HDReadDebugAlloc("Room/Em/mot_tbl.txt", &dbmodMotTblImage, 1);
                    m_MotTbl->data = (char*) dbmodMotTblImage;
                    m_MotTbl->end = (char*) dbmodMotTblImage + (size - 1);
                    m_MotTbl->cur = m_MotTbl->data;
                    mottblInit(m_MotTbl);
                }
                pDbModState->mode = 0;
            }
        }
        {
            const char* item[1] = {"PATH:"};
            const char* pathName[3] = {"LOCAL", "SFT_S", "OBJ_S"};
            int i, k;
            int x = 10;
            eprintf(x * 8, 10 * 14, 6, 0, "----- TOOL MENU -----");
            for (i = 0; i < 1; i++) {
                eprintf(x * 8, (i + 11) * 14, (i == pDbModState->pathCursor) ? 4 : 0, 0, "%s", item[i]);
                if (i == 0) {
                    for (k = 0; k < 3; k++) {
                        color = 7;
                        if (k == pDbModState->path) {
                            color = 0;
                        }
                        eprintf((16 + k * 6) * 8, (i + 11) * 14, color, 0, "%s", pathName[k]);
                    }
                }
            }
        }
        break;
    }
    pDbModState->timer++;
    return ret;
}

// NO page: picks the viewer slot (0..63 in a grid, d-pad moves, A selects, B back).
static int dbmod_no()
{
    static s8 noY;
    static s8 noX;
    JOY* joy = &Joy[0];
    int i;
    int color;
    int x, y;

    switch (pDbModState->step) {
    case 0:
        pDbModState->prevNo = pDbModState->no;
        noY = pDbModState->no / 8;
        noX = pDbModState->no % 8;
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->trg & 0x100) {
            int no = pDbModState->no;
            if (pDbModState->prevNo != (u8) no) {
                pDbModState->setNo = dbModSlot[no].setNo;
                dbmodGetSet();
            }
        }
        if (joy->rep & 0x00010001) {
            noX--;
        }
        if (joy->rep & 0x00020002) {
            noX++;
        }
        noX = CLAMP(noX, 0, 7);
        if (joy->rep & 0x00080008) {
            noY--;
        }
        if (joy->rep & 0x00040004) {
            noY++;
        }
        noY = CLAMP(noY, 0, 7);
        pDbModState->no = noY * 8 + noX;
        break;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "------ NO ------");
    for (i = 0; i < SLOT_NUM; i++) {
        if (i == pDbModState->no) {
            color = 4;
        } else if (dbModSlot[i].alive) {
            color = 0;
        } else {
            color = 7;
        }
        x = (i % 8) * 3 + 6;
        y = (i / 8 + 4) * 14;
        eprintf(x * 8, y, color, 0, "%02d", i);
        if (i == pDbModState->no) {
            eprintf((x - 1) * 8, y, 0, 0, "[");
            eprintf((x + 2) * 8, y, 0, 0, "]");
        }
    }
    return 0;
}

static const char* dbmodTypeName[8] = {"pl", "wep", "em", "obm", "et", "idm", "itm", "pcs"};

// MODEL page: chooses the model set (stick / L / R skip through the sets filtered by type) and the
// first motion, A loads the set's bin/tex files into the slot (and the sub slot), B back.
static int dbmod_model()
{
    JOY* joy = &Joy[0];
    int ret = 0;
    int r;
    int old;
    int step, max;
    s8 len;
    char label[0x100];

    switch (pDbModState->step) {
    case 0:
        pDbModState->prevSetNo = pDbModState->setNo;
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            pDbModState->setNo = pDbModState->prevSetNo;
            break;
        }
        if (joy->trg & 0x100) {
            DB_MODEL_FILES bin;
            DB_MODEL_FILES tpl;
            DB_MODEL_FILES mot;

            bin.init();
            bin.set(pDbModState->binNum, pDbModState->name[0][0]);
            tpl.init();
            tpl.set(pDbModState->texNum, pDbModState->name[1][0]);
            dbModSlot[pDbModState->no].loadModelSet(&bin, &tpl);
            bin.init();
            bin.set(pDbModState->locBinNum, pDbModState->locName[0][0]);
            tpl.init();
            tpl.set(pDbModState->locTexNum, pDbModState->locName[1][0]);
            dbModSlotSub[pDbModState->no].loadModelSet(&bin, &tpl);
            mot.init();
            mot.set(pDbModState->motFileNum, pDbModState->motName[0]);
            dbModSlot[pDbModState->no].loadMotionSet(&mot);
            pDbModState->step++;
            pDbModState->x2 = 0;
            break;
        }
        if (!(joy->on & 0x800)) {
            if (joy->trg & 0x000C000C) {
                pDbModState->timer = 8;
            }
            if (joy->trg & 0x00080008) {
                pDbModState->sub--;
            }
            if (joy->trg & 0x00040004) {
                pDbModState->sub++;
            }
            pDbModState->sub = CLAMP(pDbModState->sub, 0, 1);
        }
        switch (pDbModState->sub) {
        case 0:
            if (joy->rep & 0x40) {
                pDbModState->type--;
            }
            if (joy->rep & 0x20) {
                pDbModState->type++;
            }
            pDbModState->type = LOOP(pDbModState->type, 0, 7);
            if (joy->trg & 0x60) {
                len = strlen(dbmodTypeName[pDbModState->type]);
                pDbModState->setNo = -1;
                do {
                    pDbModState->setNo++;
                    pDbModState->setNo = LOOP(pDbModState->setNo, 0, m_MotTbl->unitNum[0] - 1);
                    dbmodGetLabel(pDbModState->setNo, label);
                } while (strncmp(label, dbmodTypeName[pDbModState->type], len) != 0);
            }
            if ((joy->rep & 0x1) || (joy->on & 0x10000)) {
                pDbModState->setNo--;
            }
            if ((joy->rep & 0x2) || (joy->on & 0x20000)) {
                pDbModState->setNo++;
            }
            pDbModState->setNo = LOOP(pDbModState->setNo, 0, m_MotTbl->unitNum[0] - 1);
            dbmodGetSet();
            break;
        case 1:
            if (pDbModState->motNo[0] == -1) {
            } else if (joy->trg & 0x10) {
                pDbModState->motNum[0] = 0;
                break;
            } else if (joy->on & 0x800) {
                old = pDbModState->motSub[0];
                if (joy->trg & 0x00080008) {
                    pDbModState->motSub[0]++;
                }
                if (joy->trg & 0x00040004) {
                    pDbModState->motSub[0]--;
                }
                pDbModState->motSub[0] = LOOP(pDbModState->motSub[0], 0, m_MotTbl->count[3] - 1);
                if (old != pDbModState->motSub[0]) {
                    pDbModState->motNum[0] = 0;
                    pDbModState->digit = 0;
                }
            } else {
                pDbModState->digit = 0;
                if (joy->on & 0x20) {
                    pDbModState->digit = 1;
                }
                if (joy->on & 0x40) {
                    pDbModState->digit = 2;
                }
                pDbModState->digit = CLAMP(pDbModState->digit, 0, pDbModState->digits[0] - 1);
                step = (int) IPOW(10.0f, pDbModState->digit);
                max = (int) IPOW(10.0f, pDbModState->digits[0]) - 1;
                if (joy->rep2 & 0x00010001) {
                    pDbModState->motNum[0] -= step;
                }
                if (joy->rep2 & 0x00020002) {
                    pDbModState->motNum[0] += step;
                }
                if (dbmodLoopNum) {
                    pDbModState->motNum[0] = LOOP(pDbModState->motNum[0], -1, max);
                } else {
                    pDbModState->motNum[0] = CLAMP(pDbModState->motNum[0], -1, max);
                }
                if (pDbModState->motNum[0] == -1) {
                    pDbModState->digit = 0;
                }
            }
            dbmodGetFilenames();
            break;
        }
        break;
    case 2:
        r = dbModSlot[pDbModState->no].loadModel();
        switch (r) {
        case 0:
            break;
        case 1:
            pDbModState->step++;
            pDbModState->x2 = 0;
            break;
        case -1:
            pDbModState->step = 0;
            break;
        }
        break;
    case 3:
        r = dbModSlotSub[pDbModState->no].loadModel();
        switch (r) {
        case 0:
            break;
        case 1:
            pDbModState->step++;
            pDbModState->x2 = 0;
            if (pDbModState->locNo != -1) {
                dbModSlotSub[pDbModState->no].parentNo = pDbModState->no;
                dbModSlotSub[pDbModState->no].partsNo = pDbModState->locParts;
                dbModSlotSub[pDbModState->no].pos0 = pDbModState->locPos;
                dbModSlotSub[pDbModState->no].ang0 = pDbModState->locRot;
                dbModSlotSub[pDbModState->no].alive = 1;
            } else {
                dbModSlotSub[pDbModState->no].alive = 0;
            }
            break;
        case -1:
            pDbModState->step = 0;
            break;
        }
        break;
    case 4:
        r = dbModSlot[pDbModState->no].loadMotion();
        switch (r) {
        case 0:
            break;
        case 1:
            pDbModState->step = 0;
            ret = 1;
            break;
        case -1:
            pDbModState->step = 0;
            break;
        }
        break;
    }
    dbmodDispModelName();
    switch (pDbModState->sub) {
    case 0:
        model_usage();
        break;
    case 1:
        motion_usage();
        break;
    }
    return ret;
}

static const char* dbmodModelLabel[3] = {"MODEL :", "MOTION:", "SKIP  :"};
static const char* dbmodTypeLabel[8] = {"PL", "WP", "EM", "OB", "ET", "ID", "IT", "PS"};

// Draws the MODEL page: set / motion rows with the cursor, the motion file name with the digit
// cursor, the bin and tex directories and file lists with their load states.
void dbmodDispModelName()
{
    int x = 6;
    int y = 4;
    int i, k;
    int color;
    int len, nlen, hs, he;
    char* name;
    DB_MODEL_FILES* f;
    int no;

    eprintf(5 * 8, 3 * 14, 5, 0, "---- MODEL -----");
    for (i = 2; i >= 0; i--) {
        eprintf(x * 8, (i + 4) * 14, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodModelLabel[i]);
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf((x - 1) * 8, (i + 4) * 14, 0x16, 0, ">");
        }
        switch (i) {
        case 0:
            eprintf((x + 8) * 8, y * 14, 0, 0, "%s", pDbModState->setName);
            break;
        case 1:
            if (pDbModState->motNo[0] == -1) {
                eprintf((x + 8) * 8, (y + 1) * 14, 0, 0, "[%6s]", "null");
                break;
            }
            no = pDbModState->motNum[0];
            if (no == -1) {
                eprintf((x + 8) * 8, (y + 1) * 14, 0, 0, "[%6s] --------.---", pDbModState->motDir[0]);
                break;
            }
            // the surplus `no` argument is in the original: it is passed in r9 (unused by the format), which is
            // why motNum lands in r9 and why reload's "[%6s]"/"%s" highs use r11 (r9 is live at the lo_sum)
            eprintf((x + 8) * 8, (y + 1) * 14, 0, 0, "[%6s]", pDbModState->motDir[0], no);
            switch (pDbModState->motType[0]) {
            case 0:
                color = 0;
                break;
            case 1:
                break;
            case -1:
                color = 2;
                break;
            }
            hs = strlen(pDbModState->motName[0]) - pDbModState->hashOfs[0];
            name = dbmodSkipPath(pDbModState->motName[0]);
            nlen = strlen(name);
            hs = nlen - hs;
            he = hs + pDbModState->digits[0] - 1;
            for (k = 0; k < nlen; k++) {
                f = 0;  // COMPILER-DIFF: candidate (loop.c insn_count): two dead sets, deleted by flow, keep
                no = k; // the "^" high out of loop pass 1 (58 -> 60 real insns, its threshold 59 < 60)
                if (k >= hs && k <= he) {
                    color = (i == pDbModState->sub) ? 4 : 0;
                } else {
                    color = 0;
                }
                eprintf((23 + k) * 8, (i + 4) * 14, color, 0, "%c", name[k]);
                if (i == pDbModState->sub && k == hs + (pDbModState->digits[0] - pDbModState->digit - 1)) {
                    eprintf((23 + k) * 8, (i + 5) * 14, 0x16, 0, "^");
                }
            }
            break;
        case 2:
            for (k = 0; k < 8; k++) {
                if (k == pDbModState->type) {
                    color = 0;
                } else {
                    color = 7;
                }
                eprintf((14 + k * 3) * 8, (i + 4) * 14, color, 0, "%s", dbmodTypeLabel[k]);
            }
            break;
        }
    }
    eprintf(6 * 8, 8 * 14, 0, 0, "BIN:[%6s]", pDbModState->binDir);
    x = 6;
    y = 8;
    color = 0;
    f = &dbModSlot[pDbModState->no].m_files[0];
    for (i = 0; i < pDbModState->binNum; i++) {
        if (i < f->m_num && strcmp(pDbModState->name[0][i], f->m_name[i]) == 0) {
            switch (f->m_read_state[i]) {
            case 3:
                color = 0;
                break;
            case 2:
                color = 2;
                break;
            case 1:
                color = 7;
                break;
            }
        } else {
            color = 0;
        }
        name = dbmodSkipPath(pDbModState->name[0][i]);
        eprintf(x * 8, (9 + i) * 14, color, 0, "%s", name);
    }
    eprintf(20 * 8, y * 14, 0, 0, "TEX:[%6s]", pDbModState->texDir);
    x = 20;
    color = 0;
    f = &dbModSlot[pDbModState->no].m_files[1];
    for (i = 0; i < pDbModState->texNum; i++) {
        if (i < f->m_num && strcmp(pDbModState->name[1][i], f->m_name[i]) == 0) {
            switch (f->m_read_state[i]) {
            case 3:
                color = 0;
                break;
            case 2:
                color = 2;
                break;
            case 1:
                color = 7;
                break;
            }
        } else {
            color = 0;
        }
        name = dbmodSkipPath(pDbModState->name[1][i]);
        eprintf(x * 8, (9 + i) * 14, color, 0, "%s", name);
    }
}

static const char* dbmodMotionLabel[2] = {"MOTION 0:", "MOTION 1:"};

// MOTION page: per motion file of the slot, left/right step the motion number digit under the
// cursor (R x10, L x100, Z reset, Y+up/down change the file), A loads the motions, B back.
static int dbmod_motion()
{
    JOY* joy = &Joy[0];
    int ret = 0;
    int r;
    int old;
    int step, max;
    int x;
    int nx;
    int i, k, j;
    int color;
    int len, nlen, hs, he;
    char* name;
    int no;

    if (dbModSlot[pDbModState->no].pEm == 0) {
        pDbModState->mode--;
        return 0;
    }
    switch (pDbModState->step) {
    case 0:
        pDbModState->prevSetNo = pDbModState->setNo;
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            pDbModState->setNo = pDbModState->prevSetNo;
            break;
        }
        if (joy->trg & 0x100) {
            DB_MODEL_FILES mot;

            mot.init();
            mot.set(pDbModState->motFileNum, pDbModState->motName[0]);
            dbModSlot[pDbModState->no].loadMotionSet(&mot);
            pDbModState->step++;
            pDbModState->x2 = 0;
            break;
        }
        if (!(joy->on & 0x800)) {
            old = pDbModState->sub;
            if (joy->trg & 0x000C000C) {
                pDbModState->timer = 8;
            }
            if (joy->trg & 0x00080008) {
                pDbModState->sub--;
            }
            if (joy->trg & 0x00040004) {
                pDbModState->sub++;
            }
            pDbModState->sub = CLAMP(pDbModState->sub, 0, 1);
            if (old != pDbModState->sub) {
                pDbModState->digit = 0;
            }
        }
        k = pDbModState->sub;
        if (pDbModState->motNo[k] != -1) {
            if (joy->trg & 0x10) {
                pDbModState->motNum[k] = 0;
                break;
            }
            if (joy->on & 0x800) {
                int oldSub = pDbModState->motSub[k];
                if (joy->trg & 0x00080008) {
                    pDbModState->motSub[k]++;
                }
                if (joy->trg & 0x00040004) {
                    pDbModState->motSub[k]--;
                }
                pDbModState->motSub[k] = LOOP(pDbModState->motSub[k], 0, m_MotTbl->count[3] - 1);
                if (oldSub != pDbModState->motSub[k]) {
                    pDbModState->motNum[k] = 0;
                    pDbModState->digit = 0;
                }
            } else {
                pDbModState->digit = 0;
                if (joy->on & 0x20) {
                    pDbModState->digit = 1;
                }
                if (joy->on & 0x40) {
                    pDbModState->digit = 2;
                }
                pDbModState->digit = CLAMP(pDbModState->digit, 0, pDbModState->digits[k] - 1);
                step = (int) IPOW(10.0f, pDbModState->digit);
                max = (int) IPOW(10.0f, pDbModState->digits[k]) - 1;
                if (joy->rep2 & 0x00010001) {
                    pDbModState->motNum[k] -= step;
                }
                if (joy->rep2 & 0x00020002) {
                    pDbModState->motNum[k] += step;
                }
                if (dbmodLoopNum) {
                    pDbModState->motNum[k] = LOOP(pDbModState->motNum[k], -1, max);
                } else {
                    pDbModState->motNum[k] = CLAMP(pDbModState->motNum[k], -1, max);
                }
                if (pDbModState->motNum[k] == -1) {
                    pDbModState->digit = 0;
                }
            }
        }
        dbmodGetFilenames();
        break;
    case 2:
        r = dbModSlot[pDbModState->no].loadMotion();
        switch (r) {
        case 0:
            break;
        case 1:
            pDbModState->step = 0;
            ret = 1;
            break;
        case -1:
            pDbModState->step = 0;
            break;
        }
        break;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "---- MOTION ----");
    x = 6;
    for (i = 0; i <= 1; i++) {
        eprintf(x * 8, (i + 4) * 14, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodMotionLabel[i]);
        nx = 16;
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf(5 * 8, (i + 4) * 14, 0x16, 0, ">");
        }
        if (pDbModState->motNo[i] == -1) {
            eprintf(nx * 8, (i + 4) * 14, 0, 0, "[%6s]", "null");
            continue;
        }
        no = pDbModState->motNum[i];
        if (no == -1) {
            eprintf(nx * 8, (i + 4) * 14, 0, 0, "[%6s] --------.---", pDbModState->motDir[i]);
            continue;
        }
        // surplus `no` argument (r9) as in dbmodDispModelName: it is what puts motNum[i] into r9 (`lhax r9,r9,r11`)
        eprintf(nx * 8, (i + 4) * 14, 0, 0, "[%6s]", pDbModState->motDir[i], no);
        switch (pDbModState->motType[i]) {
        case 0:
            color = 0;
            break;
        case 1:
            break;
        case -1:
            color = 2;
            break;
        }
        hs = strlen(pDbModState->motName[i]) - pDbModState->hashOfs[i];
        name = dbmodSkipPath(pDbModState->motName[i]);
        nlen = strlen(name);
        hs = nlen - hs;
        he = hs + pDbModState->digits[i] - 1;
        for (j = 0; j < nlen; j++) {
            if (i == pDbModState->sub) {
                color = (j >= hs && j <= he) ? 4 : 0;
            } else {
                color = 0;
            }
            eprintf((25 + j) * 8, (i + 4) * 14, color, 0, "%c", name[j]);
        }
    }
    len = strlen(pDbModState->motName[pDbModState->sub]) - pDbModState->hashOfs[pDbModState->sub];
    len = strlen(dbmodSkipPath(pDbModState->motName[pDbModState->sub])) - len;
    len += pDbModState->digits[pDbModState->sub] - pDbModState->digit;
    eprintf((len - 1 + 25) * 8, 6 * 14, 0x16, 0, "^");
    motion_usage();
    return ret;
}

// LOCATE page: places the slot model (position in X-Z / Y or per axis, rotation per axis, scale;
// X switches the axis mode, Z resets, R/L fine steps) and the parent/child attachment from the
// _ONTO unit (locate bin/tex names, parts); B back.
static int dbmod_locate()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    int dir = 0;
    int changed = 0;
    int old;
    int i, k, n, y;
    int x;
    int color;
    f32 speed, mag, deg;
    f32* axis;
    Vec v;
    Vec ax;
    Vec ab[2];
    Mtx m;
    Vec ay, az;
    Vec* pv;
    Vec* pax;
    Vec* pos;

    switch (pDbModState->step) {
    case 0:
        pDbModState->sub = 0;
        pDbModState->x7 = 0;
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->trg & 0x000C000C) {
            pDbModState->timer = 8;
        }
        if (joy->trg & 0x00080008) {
            pDbModState->sub--;
        }
        if (joy->trg & 0x00040004) {
            pDbModState->sub++;
        }
        pDbModState->sub = CLAMP(pDbModState->sub, 0, 4);
        switch (pDbModState->sub) {
        case 0:
        case 1:
            if (joy->trg & 0x100) {
                pDbModState->step++;
            }
            break;
        case 2:
            if (joy->trg & 0x100) {
                changed = 1;
                ax.x = pG->Camera.mat[0][2];
                ax.y = pG->Camera.mat[1][2];
                ax.z = pG->Camera.mat[2][2];
                PSVECScale(&ax, &ax, -1000.0f);
                PSVECAdd(&pG->Camera.param.pos, &ax, &ax);
                em->pos0 = ax;
            }
            break;
        case 3:
            if (joy->rep & 0x00010001) {
                dir = -1;
            }
            if (joy->rep & 0x00020002) {
                dir = 1;
            }
            if (joy->rep & 0x00030003) {
                for (n = 0; n < SLOT_NUM; n++) {
                    em->parentNo += dir;
                    em->parentNo = LOOP(em->parentNo, 0, SLOT_NUM - 1);
                    if (dbModSlot[em->parentNo].alive) {
                        em->partsNo = 0;
                        break;
                    }
                }
                changed = 1;
            }
            break;
        case 4:
            if (dbModSlot[em->parentNo].pEm) {
                old = em->partsNo;
                if (joy->rep & 0x00010001) {
                    em->partsNo--;
                }
                if (joy->rep & 0x00020002) {
                    em->partsNo++;
                }
                em->partsNo = CLAMP(em->partsNo, 0, dbModSlot[em->parentNo].pEm->nParts - 1);
                if (old != em->partsNo) {
                    changed = 1;
                }
            } else {
                em->partsNo = 0;
            }
            break;
        }
        break;
    case 2:
        if (joy->trg & 0x200) {
            pDbModState->step--;
            break;
        }
        changed = 1;
        switch (pDbModState->sub) {
        case 0:
            pos = &em->pos0;
            if (joy->trg & 0x400) {
                pDbModState->step++;
                break;
            }
            if (joy->trg & 0x10) {
                pos->z = 0.0f;
                pos->y = 0.0f;
                pos->x = 0.0f;
                break;
            }
            speed = 100.0f;
            if (joy->on & 0x40) {
                speed *= 0.01f;
            } else if (joy->on & 0x20) {
                speed *= 0.1f;
            }
            v.x = v.y = v.z = 0.0f;
            if (!(joy->on & 0x800)) {
                if (joy->rep2 & 0x00010001) {
                    v.x -= speed;
                }
                if (joy->rep2 & 0x00020002) {
                    v.x += speed;
                }
                if (joy->rep2 & 0x00080008) {
                    v.z -= speed;
                }
                if (joy->rep2 & 0x00040004) {
                    v.z += speed;
                }
            } else {
                if (joy->rep2 & 0x00080008) {
                    v.y += speed;
                }
                if (joy->rep2 & 0x00040004) {
                    v.y -= speed;
                }
            }
            if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
                break;
            }
            if (v.y == 0.0f) {
                mag = PSVECMag(&v);
                PSMTXMultVecSR(pG->Camera.mat, &v, &v);
                v.y = 0.0f;
#line 1982
                VECNormalize(&v, &v);
                PSVECScale(&v, &v, mag);
            }
            PSVECAdd(pos, &v, pos);
            break;
        case 1:
            if (joy->trg & 0x00010001) {
                pDbModState->x7--;
            }
            if (joy->trg & 0x00020002) {
                pDbModState->x7++;
            }
            pDbModState->x7 = CLAMP(pDbModState->x7, 0, 2);
            axis = (f32*) &em->ang0;
            deg = axis[pDbModState->x7] * 57.29578f;
            if (joy->trg & 0x10) {
                axis[pDbModState->x7] = 0.0f;
                break;
            }
            speed = 10.0f;
            if (joy->on & 0x40) {
                speed *= 0.01f;
            } else if (joy->on & 0x20) {
                speed *= 0.1f;
            }
            if (joy->rep2 & 0x00080008) {
                deg += speed;
            }
            if (joy->rep2 & 0x00040004) {
                deg -= speed;
            }
            axis[pDbModState->x7] = deg * DEG2RAD;
            VecRadLimit((Vec*) axis);
            break;
        }
        break;
    case 3:
        changed = 1;
        if (pDbModState->sub != 0) {
            break;
        }
        if (joy->trg & 0x200) {
            pDbModState->step = 0;
            break;
        }
        if (joy->trg & 0x400) {
            pDbModState->step--;
            break;
        }
        if (joy->trg & 0x00010001) {
            pDbModState->x7--;
        }
        if (joy->trg & 0x00020002) {
            pDbModState->x7++;
        }
        pDbModState->x7 = CLAMP(pDbModState->x7, 0, 2);
        axis = (f32*) &em->pos0;
        if (joy->trg & 0x10) {
            axis[pDbModState->x7] = 0.0f;
            break;
        }
        speed = 100.0f;
        if (joy->on & 0x40) {
            speed *= 0.01f;
        } else if (joy->on & 0x20) {
            speed *= 0.1f;
        }
        if (joy->rep2 & 0x00080008) {
            axis[pDbModState->x7] += speed;
        }
        if (joy->rep2 & 0x00040004) {
            axis[pDbModState->x7] -= speed;
        }
        break;
    }
    if (pDbModState->no != em->parentNo) {
        em->pEm_parent = dbModSlot[em->parentNo].pEm;
    } else {
        em->pEm_parent = 0;
    }
    if (changed && em->pEm) {
        cEm* model = em->pEm;
        model->pos = em->pos0;
        model->ang = em->ang0;
        RotMatrix(model->mat, &model->ang);
        TransMatrix(model->mat, &model->pos);
        ScaleMatrix(model->mat, &model->scale);
        model->partsMatCalc();
        model->partsWorldCalc();
    }
    static const char* dbmodLocateLabel[5] = {"POSITION:", "ANGLE   :", "AHEAD 1m:", "PARENT  :", "PARTS NO:"};
    eprintf(5 * 8, 3 * 14, 5, 0, "---- LOCATE ----");
    pv = 0;
    x = 6;
    y = 4;
    for (i = 0; i <= 4; y++, i++) {
        eprintf(x * 8, y * 14, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodLocateLabel[i]);
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            if (changed) {
                eprintf(15 * 8, y * 14, 0x16, 0, ">");
            } else {
                eprintf(5 * 8, y * 14, 0x16, 0, ">");
            }
        }
        switch (i) {
        case 0:
            pv = &em->pos0;
            break;
        case 1:
            pv = &em->ang0;
            break;
        }
        switch (i) {
        case 0:
            eprintf((x + 10) * 8, y * 14, 0, 0, "(");
            for (k = 0; k <= 2; k++) {
                f32* p = (f32*) pv;
                if (k <= 1) {
                    eprintf((x + 11 + k * 8) * 8, y * 14, 0, 0, "%5.1f,", p[k]);
                } else {
                    eprintf((x + 11 + k * 8) * 8, y * 14, 0, 0, "%5.1f)", p[k]);
                }
                if (i == pDbModState->sub && changed) {
                    if (pDbModState->step == 2) {
                        if (joy->on & 0x800) {
                            if (k == 1) {
                                eprintf((x + 11 + k * 8) * 8, y * 14, 4, 0, "%5.1f", p[k]);
                            }
                        } else {
                            if (k != 1) {
                                eprintf((x + 11 + k * 8) * 8, y * 14, 4, 0, "%5.1f", p[k]);
                            }
                        }
                    } else if (pDbModState->step == 3) {
                        if (k == pDbModState->x7) {
                            eprintf((x + 11 + k * 8) * 8, y * 14, 4, 0, "%5.1f", p[k]);
                        }
                    }
                }
            }
            break;
        case 1:
            eprintf((x + 10) * 8, y * 14, 0, 0, "(");
            for (k = 0; k <= 2; k++) {
                f32* p = (f32*) pv;
                if (k <= 1) {
                    eprintf((x + 11 + k * 8) * 8, y * 14, 0, 0, "%5.1f,", p[k] * 57.29578f);
                } else {
                    eprintf((x + 11 + k * 8) * 8, y * 14, 0, 0, "%5.1f)", p[k] * 57.29578f);
                }
                if (i == pDbModState->sub && changed && k == pDbModState->x7) {
                    eprintf((x + 11 + k * 8) * 8, y * 14, 4, 0, "%5.1f", p[k] * 57.29578f);
                }
            }
            break;
        case 3:
            for (k = 0; k < SLOT_NUM; k++) {
                if (dbModSlot[k].alive) {
                    color = 0;
                } else {
                    color = 7;
                }
                eprintf((17 + k * 3) * 8, y * 14, color, 0, "%02d", k);
                if (k == em->parentNo) {
                    eprintf((16 + k * 3) * 8, y * 14, 0, 0, "[");
                    eprintf((19 + k * 3) * 8, y * 14, 0, 0, "]");
                }
            }
            break;
        case 4:
            eprintf(17 * 8, y * 14, 0, 0, "%02d", em->partsNo);
            break;
        }
        if (pDbModState->no == em->parentNo) {
            ab[0] = em->pos0;
            ab[0].y = 0.0f;
            ab[1] = ab[0];
            ab[1].z = 0.0f;
            Draw_line3d(&ab[0], &ab[1], 0xFF808080, 0);
            ab[1] = ab[0];
            ab[1].x = 0.0f;
            Draw_line3d(&ab[0], &ab[1], 0xFF808080, 0);
            ab[1] = em->pos0;
            Draw_line3d(&ab[0], &ab[1], 0xFF808080, 0);
            RotMatrix(m, &em->ang0);
            pax = &ax;
            ax.x = m[0][0];
            ax.y = m[1][0];
            ax.z = m[2][0];
            ay.x = m[0][1];
            ay.y = m[1][1];
            ay.z = m[2][1];
            az.x = m[0][2];
            az.y = m[1][2];
            az.z = m[2][2];
            PSVECScale(pax, pax, 500.0f);
            PSVECScale(&ay, &ay, 500.0f);
            PSVECScale(&az, &az, 500.0f);
            PSVECAdd(pax, &em->pos0, pax);
            PSVECAdd(&ay, &em->pos0, &ay);
            PSVECAdd(&az, &em->pos0, &az);
            Draw_line3d(&em->pos0, pax, 0xFFFF0000, 0);
            Draw_line3d(&em->pos0, &ay, 0xFF00FF00, 0);
            Draw_line3d(&em->pos0, &az, 0xFF2020FF, 0);
        }
    }
    if (pDbModState->step == 2) {
        switch (pDbModState->sub) {
        case 0:
            position_usage(0);
            break;
        case 1:
            rotation_usage();
            break;
        }
    } else if (pDbModState->step == 3) {
        if (pDbModState->sub == 0) {
            position_usage(1);
        }
    }
    return 0;
}

static const char* dbmodTransLabel[4] = {"ON-", "OFF", "ADD", "INF"};

// TRANS page: root translation mode of motion 0 (ON / OFF / ADD / INF -> SetTransMode flags
// 0x11 / 0x4000).
static int dbmod_trans()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    u16* flag;
    int i;
    int y;
    int color;

    if (em->pEm == 0) {
        pDbModState->mode--;
        return 0;
    }
    flag = &em->motInfo[0].flags;
    switch (pDbModState->step) {
    case 0:
        if (em->motInfo[0].flags & 1) {
            if (em->motInfo[0].flags & 0x10) {
                pDbModState->transMode = 0;
            } else {
                pDbModState->transMode = 2;
            }
        } else {
            pDbModState->transMode = 1;
        }
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
        }
        if (joy->rep & 0x00010001) {
            pDbModState->transMode--;
        }
        if (joy->rep & 0x00020002) {
            pDbModState->transMode++;
        }
        pDbModState->transMode = CLAMP(pDbModState->transMode, 0, 3);
        switch (pDbModState->transMode) {
        case 0:
            *flag = (*flag | 0x11) & ~0x4000;
            break;
        case 1:
            *flag &= ~0x4011;
            break;
        case 2:
            *flag = (*flag | 0x1) & ~0x4010;
            break;
        case 3:
            *flag = (*flag & ~0x10) | 0x4001;
            break;
        }
        em->motInfo[0].flags = *flag;
        break;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "---- TRANS -----");
    y = 4;
    for (i = 0; i <= 3; i++) {
        if (i == pDbModState->transMode) {
            color = 0;
        } else {
            color = 7;
        }
        eprintf((6 + i * 4) * 8, y * 14, color, 0, "%s", dbmodTransLabel[i]);
        if (i != 3) {
            eprintf((9 + i * 4) * 8, y * 14, 0, 0, "/");
        }
    }
    return 0;
}

// LOOP page: motion loop flag (flags bit 2) on / off.
static int dbmod_loop()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    u16* flag;

    if (em->pEm == 0) {
        pDbModState->mode--;
        return 0;
    }
    flag = &em->motInfo[0].flags;
    switch (pDbModState->step) {
    case 0:
        if (em->motInfo[0].flags & 4) {
            pDbModState->loopFlag = 1;
        } else {
            pDbModState->loopFlag = 0;
        }
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->rep & 0x00010001) {
            pDbModState->loopFlag = 1;
        }
        if (joy->rep & 0x00020002) {
            pDbModState->loopFlag = 0;
        }
        if (pDbModState->loopFlag) {
            *flag |= 4;
        } else {
            *flag &= ~4;
        }
        em->motInfo[0].flags = *flag;
        break;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "----- LOOP -----");
    if (pDbModState->loopFlag) {
        eprintf(6 * 8, 4 * 14, 0, 0, "ON-/---");
    } else {
        eprintf(6 * 8, 4 * 14, 0, 0, "---/OFF");
    }
    return 0;
}

// PLAY page: playback mode PLAY / STEP (X + stick steps frames) / REV (dbModPlayMode).
static int dbmod_play()
{
    JOY* joy = &Joy[0];

    switch (pDbModState->step) {
    case 0:
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->on & 0x400) {
            break;
        }
        if (joy->rep & 0x00010001) {
            pDbModState->playMode--;
        }
        if (joy->rep & 0x00020002) {
            pDbModState->playMode++;
        }
        pDbModState->playMode = LOOP(pDbModState->playMode, 0, 2);
        break;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "----- PLAY -----");
    switch (pDbModState->playMode) {
    case 0:
        eprintf(6 * 8, 4 * 14, 0, 0, "PLAY/----/----");
        break;
    case 1:
        eprintf(6 * 8, 4 * 14, 0, 0, "----/STEP/----");
        break;
    case 2:
        eprintf(6 * 8, 4 * 14, 0, 0, "----/----/REV-");
        break;
    }
    return 0;
}

// FLIP page: x-mirror flag (flags 0x40) on / off.
static int dbmod_flip()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    u16* flag;

    if (em->pEm == 0) {
        pDbModState->mode--;
        return 0;
    }
    flag = &em->motInfo[0].flags;
    switch (pDbModState->step) {
    case 0:
        if (em->motInfo[0].flags & 0x40) {
            pDbModState->flipFlag = 1;
        } else {
            pDbModState->flipFlag = 0;
        }
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
        }
        if (joy->rep & 0x00010001) {
            pDbModState->flipFlag = 1;
        }
        if (joy->rep & 0x00020002) {
            pDbModState->flipFlag = 0;
        }
        if (pDbModState->flipFlag) {
            *flag |= 0x40;
        } else {
            *flag &= ~0x40;
        }
        em->motInfo[0].flags = *flag;
        break;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "----- FLIP -----");
    if (pDbModState->flipFlag) {
        eprintf(6 * 8, 4 * 14, 0, 0, "ON-/---");
    } else {
        eprintf(6 * 8, 4 * 14, 0, 0, "---/OFF");
    }
    return 0;
}

static const char* dbmodBlendLabel[3] = {"MOTION 0:", "MOTION 1:", "METHOD  :"};

// BLEND page: blend ratio of the secondary motions (left/right, R x10, L x100, Z reset) and the
// mode BLEND / ADD / NONE.
static int dbmod_blend()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    f32 speed = 0.01f;
    f32 rate;
    int old, method;
    int x;
    int y;
    int i, color;

    if (em->pEm == 0) {
        pDbModState->mode--;
        return 0;
    }
    switch (pDbModState->step) {
    case 0:
        pDbModState->sub = 1;
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
        }
        if (joy->trg & 0x000C000C) {
            pDbModState->timer = 8;
        }
        if (joy->trg & 0x00080008) {
            pDbModState->sub--;
        }
        if (joy->trg & 0x00040004) {
            pDbModState->sub++;
        }
        pDbModState->sub = CLAMP(pDbModState->sub, 1, 2);
        switch (pDbModState->sub) {
        case 0:
            break;
        case 1:
            if (joy->trg & 0x10) {
                pDbModState->blendRate = 0.0f;
                break;
            }
            if (joy->on & 0x40) {
                speed *= 100.0f;
            } else if (joy->on & 0x20) {
                speed *= 10.0f;
            }
            if ((joy->rep & 0x1) || (joy->on & 0x10000)) {
                pDbModState->blendRate -= speed;
            }
            if ((joy->rep & 0x2) || (joy->on & 0x20000)) {
                pDbModState->blendRate += speed;
            }
            pDbModState->blendRate = CLAMP(pDbModState->blendRate, 0.0f, 1.0f);
            break;
        case 2:
            old = pDbModState->blendMode;
            method = old;
            if (joy->rep & 0x00010001) {
                method = old - 1;
            }
            if (joy->rep & 0x00020002) {
                method++;
            }
            method = CLAMP(method, 0, 2);
            pDbModState->blendMode = method;
            if (em->pMotBuff[1] && old != method) {
                switch (pDbModState->blendMode) {
                case 0:
                    MotionSetCore(em->pEm, &em->pEm->Motion, em->pMotBuff[0], 0, 0, em->motInfo[0].flags | 0x200, 0);
                    em->pEm->Motion.blend = (MotionWork*) &em->motInfo[1];
                    em->pEm->Motion.blend->Mot_flag &= 0x7FFFFFFF;
                    break;
                case 1:
                    MotionSetCore(em->pEm, &em->pEm->Motion, em->pMotBuff[0], 0, 0, em->motInfo[0].flags | 0x200, 0);
                    em->pEm->Motion.blend = (MotionWork*) &em->motInfo[1];
                    em->pEm->Motion.blend->Mot_flag |= 0x80000000;
                    break;
                default:
                    em->motInfo[1].blendRate = 0.0f;
                    em->pEm->Motion.blend = 0;
                    break;
                }
            }
            break;
        }
        break;
    }
    if (em->pEm->Motion.blend) {
        em->pEm->Motion.blend->Brate = pDbModState->blendRate;
    }
    eprintf(5 * 8, 3 * 14, 5, 0, "---- BLEND ----");
    x = 6;
    y = 4;
    for (i = 0; i <= 2; i++) {
        int row = (i + 4) * 14;
        eprintf(x * 8, row, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodBlendLabel[i]);
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf((x - 1) * 8, row, 0x16, 0, ">");
        }
        color = 0;
        // COMPILER-DIFF: the target passes the switch-head `color` register (`li r5,0` before the
        // tree, no set in case 0); ours folds case 0's argument to a fresh `li r5,0` (cse1 knows
        // color == 0 there). The launder hides the constant from cse.
        asm("" : "+r"(color));
        switch (i) {
        case 0:
            if (pDbModState->blendMode == 0) {
                rate = 1.0f - pDbModState->blendRate;
            } else {
                rate = 1.0f;
            }
            eprintf((x + 10) * 8, row, color, 0, "%.2f", rate);
            break;
        case 1:
            if (em->pEm->Motion.blend == 0) {
                color = 7;
            }
            eprintf((x + 10) * 8, (x - 1) * 14, color, 0, "%.2f", pDbModState->blendRate);
            break;
        case 2:
            switch (pDbModState->blendMode) {
            case 1:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "-----/ADD--/-----");
                break;
            case 0:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "BLEND/-----/-----");
                break;
            default:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "-----/-----/NONE-");
                break;
            }
            break;
        }
    }
    if (pDbModState->sub == 1) {
        blend_usage();
    }
    return 0;
}

// Unused page id 9: returns to the menu at once.
static int dbmod_except()
{
    if (Joy[0].trg & 0x200) {
        pDbModState->mode--;
    }
    return 0;
}

static const char* dbmodLightLabel[2] = {"ENV :", "TOOL:"};

// LIGHT page: ENV light preset (DFLT / ROOM / ST1D / ST1N / ST2 / ST3 -> lightMode) and the TOOL
// light editor (db_light) toggle.
static int dbmod_light()
{
    static cLightTool* pLightTool;
    JOY* joy = &Joy[0];
    int old;
    int i;
    int x;
    int y;

    switch (pDbModState->step) {
    case 0:
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->rep & 0x80008) {
            pDbModState->sub--;
        }
        if (joy->rep & 0x40004) {
            pDbModState->sub++;
        }
        pDbModState->sub = CLAMP(pDbModState->sub, 0, 1);
        switch (pDbModState->sub) {
        case 0:
            old = pDbModState->lightMode;
            if (joy->trg & 0x10001) {
                pDbModState->lightMode--;
            }
            if (joy->trg & 0x20002) {
                pDbModState->lightMode++;
            }
            pDbModState->lightMode = LOOP(pDbModState->lightMode, 0, 5);
            if (old != pDbModState->lightMode) {
                switch (pDbModState->lightMode) {
                case 0:
                    SetToolLight(2);
                    break;
                case 1:
                    LightMgr.update(0, -1);
                    break;
                case 2:
                    SetToolLight(5);
                    break;
                case 3:
                    SetToolLight(6);
                    break;
                case 4:
                    SetToolLight(7);
                    break;
                case 5:
                    SetToolLight(8);
                    break;
                }
            }
            break;
        case 1:
            if (joy->trg & 0x100) {
                pDbModState->x2 = 0;
                pDbModState->step++;
            }
            break;
        }
        break;
    case 2:
        switch (pDbModState->x2) {
        case 0:
            pLightTool = new cLightTool;
            pDbModState->x2++;
        case 1:
            if (pLightTool->move() == 0) {
                delete pLightTool;
                pDbModState->step--;
            }
            return 4;
        }
        break;
    }

    eprintf(5 * 8, 3 * 14, 5, 0, "---- LIGHT ----");
    x = 6;
    y = 4;
    for (i = 0; i <= 1; i++) {
        eprintf(x * 8, (y + i) * 14, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodLightLabel[i]);
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf((x - 1) * 8, (y + i) * 14, 0x16, 0, ">");
        }
        if (i == 0) {
            eprintf((x + 5) * 8, y * 14, 7, 0, "DFLT/ROOM/ST1D/ST1N/ST2-/ST3-");
            switch (pDbModState->lightMode) {
            case 0:
                eprintf((x + 5) * 8, y * 14, 0, 0, "DFLT/    /    /    /    /    ");
                break;
            case 1:
                eprintf((x + 5) * 8, y * 14, 0, 0, "    /ROOM/    /    /    /    ");
                break;
            case 2:
                eprintf((x + 5) * 8, y * 14, 0, 0, "    /    /ST1D/    /    /    ");
                break;
            case 3:
                eprintf((x + 5) * 8, y * 14, 0, 0, "    /    /    /ST1N/    /    ");
                break;
            case 4:
                eprintf((x + 5) * 8, y * 14, 0, 0, "    /    /    /    /ST2 /    ");
                break;
            case 5:
                eprintf((x + 5) * 8, y * 14, 0, 0, "    /    /    /    /    /ST3 ");
                break;
            }
        }
    }
    return 0;
}

static const char* dbmodOptionLabel[5] = {"SKELETON:", "SYNCHRO :", "LIT TYPE:", "RANGE   :", "CALC SK1:"};

// OPTION page: skeleton drawing (opt_flag bit 0), the IK report, the light type of the model
// (PL / EM / OBJ / SCR / ITM -> lit_type), LARGE / SMALL info text and the info display.
static int dbmod_option()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    int old;
    int i;
    int x;
    int y;

    switch (pDbModState->step) {
    case 0:
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->rep & 0x80008) {
            pDbModState->sub--;
        }
        if (joy->rep & 0x40004) {
            pDbModState->sub++;
        }
        pDbModState->sub = CLAMP(pDbModState->sub, 0, 4);
        switch (pDbModState->sub) {
        case 0:
            if (joy->trg & 0x10001) {
                em->opt_flag |= 1;
            }
            if (joy->trg & 0x20002) {
                em->opt_flag &= ~1;
            }
            break;
        case 1:
            if (joy->trg & 0x10001) {
                pDbModState->viewFlag |= 1;
            }
            if (joy->trg & 0x20002) {
                pDbModState->viewFlag &= ~1;
            }
            break;
        case 2:
            old = em->lit_type;
            if (joy->trg & 0x10001) {
                em->lit_type--;
            }
            if (joy->trg & 0x20002) {
                em->lit_type++;
            }
            em->lit_type = LOOP(em->lit_type, 0, 5);
            if (old != em->lit_type) {
                dbmodSetLight(em);
            }
            break;
        case 3:
            if (joy->trg & 0x10001) {
                pDbModState->viewFlag |= 8;
            }
            if (joy->trg & 0x20002) {
                pDbModState->viewFlag &= ~8;
            }
            break;
        case 4:
            if (joy->trg & 0x10001) {
                em->pEm->be_flag &= ~0x4000;
            }
            if (joy->trg & 0x20002) {
                em->pEm->be_flag |= 0x4000;
            }
            break;
        }
        break;
    }

    eprintf(5 * 8, 3 * 14, 5, 0, "---- OPTION ----");
    x = 6;
    y = 4;
    for (i = 0; i <= 4; i++) {
        eprintf(x * 8, (y + i) * 14, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodOptionLabel[i]);
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf((x - 1) * 8, (y + i) * 14, 0x16, 0, ">");
        }
        switch (i) {
        case 0:
            if (em->opt_flag & 1) {
                eprintf((x + 10) * 8, y * 14, 0, 0, "ON-/---");
            } else {
                eprintf((x + 10) * 8, y * 14, 0, 0, "---/OFF");
            }
            break;
        case 1:
            if (pDbModState->viewFlag & 1) {
                eprintf((x + 10) * 8, (y + 1) * 14, 0, 0, "ON-/---");
            } else {
                eprintf((x + 10) * 8, (y + 1) * 14, 0, 0, "---/OFF");
            }
            break;
        case 2:
            switch (em->lit_type) {
            case 0:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "PL-/---/---/---/---");
                break;
            case 1:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "---/EM-/---/---/---");
                break;
            case 2:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "---/---/OBJ/---/---");
                break;
            case 3:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "---/---/---/SCR/---");
                break;
            case 4:
                eprintf((x + 10) * 8, (y + 2) * 14, 0, 0, "---/---/---/---/ITM");
                break;
            }
            break;
        case 3:
            if (pDbModState->viewFlag & 8) {
                eprintf((x + 10) * 8, (y + 3) * 14, 0, 0, "LARGE/-----");
            } else {
                eprintf((x + 10) * 8, (y + 3) * 14, 0, 0, "-----/SMALL");
            }
            break;
        case 4:
            if (em->pEm->be_flag & 0x4000) {
                eprintf((x + 10) * 8, (y + 4) * 14, 0, 0, "---/OFF");
            } else {
                eprintf((x + 10) * 8, (y + 4) * 14, 0, 0, "ON-/---");
            }
            break;
        }
    }
    return 0;
}

static const char* dbmodScaleLabel[1] = {"X Y Z:"};

// SCALE page: uniform model scale (left/right, R x10, L x100, Z reset to 1).
static int dbmod_scale()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    JOY* joy = &Joy[0];
    f32 speed = 0.01f;
    int i;
    int x;
    int y;

    if (em->pEm == 0) {
        pDbModState->mode--;
        return 0;
    }
    switch (pDbModState->step) {
    case 0:
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->trg & 0x10) {
            pDbModState->scale = 1.0f;
            break;
        }
        if (joy->on & 0x40) {
            speed *= 100.0f;
        } else if (joy->on & 0x20) {
            speed *= 10.0f;
        }
        if (joy->rep & 0x10001) {
            pDbModState->scale -= speed;
        }
        if (joy->rep & 0x20002) {
            pDbModState->scale += speed;
        }
        pDbModState->scale = CLAMP(pDbModState->scale, 0.01f, 10.0f);
        em->pEm->scale.x = pDbModState->scale;
        em->pEm->scale.y = pDbModState->scale;
        em->pEm->scale.z = pDbModState->scale;
        break;
    }

    eprintf(5 * 8, 3 * 14, 5, 0, "---- SCALE ----");
    x = 6;
    y = 4;
    for (i = 0; i <= 0; i++) {
        // COMPILER-DIFF: two codeless insns make the loop 66 real insns at loop pass 1, so the folded
        // `x - 1` (third movable, threshold 71-3-3 = 65 < 66) is moved in pass 2 after the giv init
        // (`li r30,56; li r24,5`); the original's loop had two more insns here that are gone by final.
        asm("" : : "r"(i));
        asm("" : : "r"(i));
        eprintf(x * 8, (y + i) * 14, (i == pDbModState->sub) ? 4 : 0, 0, "%s", dbmodScaleLabel[i]);
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf((x - 1) * 8, (y + i) * 14, 0x16, 0, ">");
        }
        if (i == 0) {
            eprintf((x + 10) * 8, y * 14, 0, 0, "%f", pDbModState->scale);
        }
    }
    scale_usage();
    return 0;
}

static f32 dbmodOrientLen = 800.0f;

// Draws a parts' local axes (x red, y green, z blue) from its world matrix (skeleton option).
void drawOrientation(cParts* p)
{
    Vec v[2];
    Vec w[2];
    Vec zero = {0.0f, 0.0f, 0.0f};
    Mtx m;
    int i, j;

    if (p == 0) {
        return;
    }
    {
        f32 (*d)[4] = m;
        f32 (*s)[4] = p->mat;
        i = 3;
        while (i--) {
            f32* dp = *d;
            f32* sp = *s;
            for (j = 0; j < 4; j++) {
                *dp++ = *sp++;
            }
            s++;
            d++;
        }
    }
    v[0] = v[1] = zero;
    v[0].x += dbmodOrientLen * 0.5f;
    v[1].x -= dbmodOrientLen * 0.5f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFFFF0000, 0);
    v[1] = v[0];
    v[1].x -= dbmodOrientLen / 10.0f;
    v[1].z += dbmodOrientLen / 10.0f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFFFF0000, 0);
    v[1] = v[0];
    v[1].x -= dbmodOrientLen / 10.0f;
    v[1].z -= dbmodOrientLen / 10.0f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFFFF0000, 0);

    v[0] = v[1] = zero;
    v[0].y += dbmodOrientLen * 0.5f;
    v[1].y -= dbmodOrientLen * 0.5f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFF00FF00, 0);
    v[1] = v[0];
    v[1].y -= dbmodOrientLen / 10.0f;
    v[1].x += dbmodOrientLen / 10.0f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFF00FF00, 0);
    v[1] = v[0];
    v[1].y -= dbmodOrientLen / 10.0f;
    v[1].x -= dbmodOrientLen / 10.0f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFF00FF00, 0);

    v[0] = v[1] = zero;
    v[0].z += dbmodOrientLen * 0.5f;
    v[1].z -= dbmodOrientLen * 0.5f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFF2020FF, 0);
    v[1] = v[0];
    v[1].z -= dbmodOrientLen / 10.0f;
    v[1].x += dbmodOrientLen / 10.0f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFF2020FF, 0);
    v[1] = v[0];
    v[1].z -= dbmodOrientLen / 10.0f;
    v[1].x -= dbmodOrientLen / 10.0f;
    PSMTXMultVec(m, &v[0], &w[0]);
    PSMTXMultVec(m, &v[1], &w[1]);
    Draw_line3d(&w[0], &w[1], 0xFF2020FF, 0);
}

static const char* dbmodPinfoType[3] = {"IK    :", "IK_TOE", "IK_ARM"};
static const char* dbmodPinfoLabel[6] = {"FLAG--", "POS---", "ANG---", "SCALE-", "WORLD-", "MAT---"};
static int dbmodPinfoX = 70;
static int dbmodPinfoY = 300;
static int dbmodPinfoW = 8;
static int dbmodPinfoH = 11;

// PARTS INFO page: lists the model's parts with their motion type; up/down pick a parts
// (info_parts_no), left/right change the entry (X held: by parts number), B back.
static int dbmod_p_info()
{
    static int pinfoNum;
    static u16 pinfoParts[FILE_NUM];
    static u8 pinfoType[FILE_NUM];
    DB_EM* em = &dbModSlot[pDbModState->no];
    u32 em2 = (u32) em; // COMPILER-DIFF: the p-info zero colour (see the j-loop below)
    cEm* model = em->pEm;
    JOY* joy = &Joy[0];
    MotionWork* mw;
    cParts* p;
    int n = 0;
    int i, j, k;
    int type;
    int x;
    int y = 4;
    Vec* v;

    if (model == 0) {
        pDbModState->mode--;
        return 0;
    }
    mw = &model->Motion;
    switch (pDbModState->step) {
    case 0:
        for (k = 0; k < mw->Joint_num; k++) {
            int info = mw->pJoint_kind[k] & 0xFF;
            int flag = info; // IKreport's second byte variable (`mr r9,r0` for the third test)
            u8 no = mw->pJoint_no[k];
            type = -1;
            if (info & 0x30) {
                type = 0;
                if (info & 0x20) {
                    type = 1;
                }
                if (flag & 0x80) {
                    type = 2;
                }
            }
            if (type != -1) {
                pinfoParts[n] = no;
                pinfoType[n] = type;
                n++;
            }
        }
        pinfoNum = n + 1;
        pDbModState->step++;
    case 1:
        if (joy->trg & 0x200) {
            pDbModState->mode--;
            break;
        }
        if (joy->rep & 0x80008) {
            pDbModState->sub--;
        }
        if (joy->rep & 0x40004) {
            pDbModState->sub++;
        }
        pDbModState->sub = CLAMP(pDbModState->sub, 0, pinfoNum - 1);
        if (pDbModState->sub == pinfoNum - 1) {
            if (!(joy->on & 0x400)) {
                if (joy->rep & 0x10001) {
                    em->info_parts_no--;
                }
                if (joy->rep & 0x20002) {
                    em->info_parts_no++;
                }
            }
            em->info_parts_no = LOOP(em->info_parts_no, 0, model->nParts - 1);
        } else {
            p = (cParts*) model->getPartsPtr(pinfoParts[pDbModState->sub]);
            if (!(joy->on & 0x400)) {
                if (joy->trg & 0x10001) {
                    p->motParts.flags |= 0x1000;
                }
                if (joy->trg & 0x20002) {
                    p->motParts.flags &= ~0x1000;
                }
            }
        }
        break;
    }

    eprintf(5 * 8, 3 * 14, 5, 0, "-- PARTS INFO --");
    v = 0;
    x = 6;
    for (i = 0; i < pinfoNum; i++) {
        int color = (i == pDbModState->sub) ? 4 : 0;
        if (i == pDbModState->sub && (pDbModState->timer & 0x18)) {
            eprintf((x - 1) * 8, (y + i) * 14, 0x16, 0, ">");
        }
        if (i < pinfoNum - 1) {
            eprintf(x * 8, (y + i) * 14, color, 0, "%s[  ]:", dbmodPinfoType[pinfoType[i]]);
            eprintf((x + 7) * 8, (y + i) * 14, 0, 0, "%02d", pinfoParts[i]);
        } else {
            eprintf(x * 8, (y + i) * 14, color, 0, "PARTS NO  :");
            eprintf((x + 13) * 8, (y + i) * 14, 0, 0, "%02d", em->info_parts_no);
        }
        if (i < pinfoNum - 1) {
            p = (cParts*) model->getPartsPtr(pinfoParts[i]);
            eprintf(18 * 8, (y + i) * 14, 0x14, 0, "      --- ---");
            if (p->motParts.flags & 0x1000) {
                eprintf(18 * 8, (y + i) * 14, 0, 0, "TWIST ON /   ");
            } else {
                eprintf(18 * 8, (y + i) * 14, 0, 0, "TWIST    /OFF");
            }
        } else {
            p = (cParts*) model->getPartsPtr(em->info_parts_no);
            for (j = 0; j <= 4; j++) {
                // COMPILER-DIFF: the colour of both calls is ONE zero-valued pseudo that gcse PREs into the j-loop
                // preheader (`li r25,0` after the four dbmodPinfo* highs; a REG_EQUIV constant, doubled live length,
                // lowest global priority): `em - em2` is not foldable before gcse (the copy `em2` is set in another
                // ebb), cprop turns it into `em - em` (not a constant, so PRE hoists the one expression of both
                // calls into the preheader) and cse2 folds the hoisted pseudo to 0.
                if (j == 0) {
                    eprintf2(dbmodPinfoW, dbmodPinfoH, dbmodPinfoX, dbmodPinfoY + (i + 1) * dbmodPinfoH, (int) ((u32) em - em2), 0, "%s %08x",
                             dbmodPinfoLabel[j], p->motParts.flags);
                } else {
                    switch (j) {
                    case 1:
                        v = &p->pos;
                        break;
                    case 2:
                        v = &p->ang;
                        break;
                    case 3:
                        v = &p->scale;
                        break;
                    case 4:
                        v = &p->world;
                        break;
                    }
                    eprintf2(dbmodPinfoW, dbmodPinfoH, dbmodPinfoX, dbmodPinfoY + (i + j + 1) * dbmodPinfoH, (int) ((u32) em - em2), 0,
                             "%s (%f, %f, %f)", dbmodPinfoLabel[j], v->x, v->y, v->z);
#line 3101
                }
            }
            drawOrientation(p);
        }
    }
    return 0;
}

// Placeholder page (id 14): does nothing and returns.
static int dbmod_null()
{
    pDbModState->mode--;
    return 0;
}

static const char* dbmodInfoLabel[6] = {"NO   :", "MOT 0:", "MOT 1:", "TRANS:", "LOOP :", "PLAY :"};

// Status lines of the current slot: slot number, motion 0 / 1 numbers, trans / loop / play modes.
void dbmodInfoDisp()
{
    DB_EM* em = &dbModSlot[pDbModState->no];
    int x = 0;
    int y = 3;
    int i;

    for (i = 0; i < 6; i++) {
        eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "%s", dbmodInfoLabel[i]);
        y++;
    }
    x = 7;
    y = 3;
    for (i = 0; i <= 5; i++) {
        switch (i) {
        case 0:
            eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "%02d", pDbModState->no);
            break;
        case 1:
            eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "%s", dbmodSkipPath(pDbModState->motName[0]));
            break;
        case 2:
            eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "%s", dbmodSkipPath(pDbModState->motName[0]));
            break;
        case 3:
            if (em->motInfo[0].flags & 1) {
                if (em->motInfo[0].flags & 0x10) {
                    eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "ON");
                } else {
                    eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "ADD");
                }
            } else {
                eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "OFF");
            }
            break;
        case 4:
            if (em->motInfo[0].flags & 4) {
                eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "ON");
            } else {
                eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "OFF");
            }
            break;
        case 5:
            switch (pDbModState->playMode) {
            case 0:
                eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "PLAY");
                break;
            case 1:
                eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "STEP");
                break;
            case 2:
                eprintf2(7, 10, 300 + x * 7, y * 10, 0, 0, "REV");
                break;
            }
            break;
        }
        y++;
    }
    em->IKreport();
}


// Restarts every loaded slot's motions at `frame` (motion 0 through the model's MotionWork, the
// others as blend motions; flag 0x200 = keep the parts), resets pos/ang to pos0/ang0.
void dbModMotionSet(int frame)
{
    DB_EM* em;
    cEm* model;
    int n, i;

    for (n = 0; n <= SLOT_NUM - 1; n++) {
        em = &dbModSlot[n];
        if (em->alive) {
            model = em->pEm;
            for (i = 0; i <= FILE_NUM - 1; i++) {
                if (em->pMotBuff[i]) {
                    em->motInfo[i].flags2 |= 0x20000000;
                    if (i == 0) {
                        *(DbMotWork*) &model->Motion = em->motInfo[0];
                        MotionSetCore(model, &model->Motion, em->pMotBuff[i], 0, 0, em->motInfo[0].flags | 0x200, frame);
                    } else {
                        MotionSetCore(model, &em->motInfo[i], em->pMotBuff[i], 0, 0, em->motInfo[0].flags | 0x200, frame);
                    }
                }
            }
            model->pos = em->pos0;
            model->ang = em->ang0;
            em->em_flag &= ~1;
        }
    }
}
// Plays motion sequence `seq` (MotionSeqKey table) on slot `no` from key `frame` with MotionWork
// flags `flag` (t_motseq / t_event drive this).
void dbModMotionSetSeq(int no, void* seq, int flag, int frame)
{
    DB_EM* em = &dbModSlot[no];
    cEm* model = em->pEm;

    em->motInfo[0].flags = flag;
    MotionSetCore(model, &model->Motion, em->pMotBuff[0], seq, 0, (u16) (flag | 0x200), (u16) frame);
    MotionGetPosition(model, &model->pos, &model->ang);
}

// Applies the PLAY page mode to a motion flag word: 0 play (clear 2 | 8), 1 step (pause bit 8;
// X + stick left/right steps one frame back / forward), 2 reverse (bit 1).
void dbModPlayMode(u16* flag)
{
    JOY* joy = &Joy[0];

    switch (pDbModState->playMode) {
    case 0:
        *flag &= ~0xA;
        break;
    case 1:
        *flag &= ~2;
        *flag |= 8;
        if (joy->on & 0x400) {
            if (joy->rep & 0x10000) {
                *flag |= 2;
                *flag &= ~8;
            } else if (joy->rep & 0x20000) {
                *flag &= ~0xA;
            }
        }
        break;
    case 2:
        *flag |= 2;
        *flag &= ~8;
        break;
    }
}


// Per frame: orders the slots parents first, advances every alive model's motions (MotionMove,
// blend of the secondary motions, the PLAY mode), attaches children to their parent's parts, wraps
// the root position inside +-10000 / 50000 units (viewFlag 4 / 8), draws skeletons per option;
// when every one-shot motion ended (allDone) restarts them from frame 0.
void dbModMotionMove()
{
    s8 order[SLOT_NUM];
    DB_EM* em;
    cEm* model;
    cEm* parent;
    cParts* pp;
    int n, i, j;
    int noMotion;
    int allDone = 1;
    Vec ax, ay, az, az2;
    f32 lim;

    for (n = 0; n < SLOT_NUM; n++) {
        order[n] = n;
    }
    i = 0;
    while (i <= SLOT_NUM - 1) {
        if (order[i] == dbModSlot[order[i]].parentNo) {
            i++;
            continue;
        }
        for (j = i + 1; j <= SLOT_NUM - 1; j++) {
            if (order[j] == dbModSlot[order[i]].parentNo) {
                u8 tmp = order[j];
                order[j] = order[i];
                order[i] = tmp;
                break;
            }
        }
        i++;
    }

    for (n = 0; n <= SLOT_NUM - 1; n++) {
        em = &dbModSlot[order[n]];
        if (em->alive == 0) {
            continue;
        }
        model = em->pEm;
        if (!model->isAlive()) {
            continue;
        }
        noMotion = 1;
        for (i = FILE_NUM - 1; i >= 0; i--) {
            if (em->pMotBuff[i]) {
                noMotion = 0;
                if (i == 0) {
                    if (!(pDbModState->viewFlag & 2)) {
                        dbModPlayMode(&em->motInfo[0].flags);
                    }
                    if (em->em_flag & 1) {
                        em->motInfo[0].flags |= 8;
                    }
                } else {
                    if (!(pDbModState->viewFlag & 2)) {
                        dbModPlayMode(&em->motInfo[i].flags);
                    }
                }
            }
        }
        if (noMotion == 0 && !SpfFlagChk(pG, SPF_OBJ)) {
            model->Motion.Mot_attr = em->motInfo[0].flags;
            MotionMove(model, 0);
            if (model->Motion.blend == 0 && em->mot_num > 1 && model->Motion.Mot_state != 0) {
                em->mot_cnt++;
                if (em->mot_cnt > em->mot_num - 1) {
                    em->mot_cnt = 0;
                }
                MotionSetCore(model, &model->Motion, em->pMotBuff[em->mot_cnt], 0, em->motFlag[em->mot_cnt], em->motInfo[0].flags | 0x200,
                              (u16) em->motStat[em->mot_cnt]);
            }
            if ((pDbModState->viewFlag & 1) && (em->em_flag & 1) == 0) {
                if (model->Motion.Mot_state != 0) {
                    em->em_flag |= 1;
                } else {
                    allDone = 0;
                }
            }
        }
        parent = em->pEm_parent;
        if (parent) {
            model->pos = em->pos0;
            model->ang = em->ang0;
            RotMatrix(model->mat, &model->ang);
            TransMatrix(model->mat, &model->pos);
            ScaleMatrix(model->mat, &model->scale);
            if (parent->pList) {
                pp = (cParts*) parent->getPartsPtr(em->partsNo);
                PSMTXConcat(pp->mat, model->mat, model->mat);
                ax.x = model->mat[0][0];
                ax.y = model->mat[1][0];
                ax.z = model->mat[2][0];
                ay.x = model->mat[0][1];
                ay.y = model->mat[1][1];
                ay.z = model->mat[2][1];
                az.x = model->mat[0][2];
                az.y = model->mat[1][2];
                az.z = model->mat[2][2];
#line 3791
                VECNormalize(&ax, &ax);
                VECNormalize(&ay, &ay);
                VECNormalize(&az, &az);
                model->mat[0][0] = ax.x;
                model->mat[1][0] = ax.y;
                model->mat[2][0] = ax.z;
                model->mat[0][1] = ay.x;
                model->mat[1][1] = ay.y;
                model->mat[2][1] = ay.z;
                model->mat[0][2] = az.x;
                model->mat[1][2] = az.y;
                model->mat[2][2] = az.z;
            }
        } else {
            if ((em->motInfo[0].flags & 1) && (em->motInfo[0].flags & 0x10)) {
                if (model->Motion.Mot_state & 3) {
                    model->pos = em->pos0;
                    model->ang = em->ang0;
                }
            } else {
                if (!(em->motInfo[0].flags & 1)) {
                    model->pos = em->pos0;
                    model->ang = em->ang0;
                }
            }
            if (em->pMotBuff[0] == 0) {
                model->pos = em->pos0;
                model->ang = em->ang0;
                RotMatrix(model->mat, &model->ang);
                TransMatrix(model->mat, &model->pos);
                ScaleMatrix(model->mat, &model->scale);
                model->partsMatCalc();
                model->partsWorldCalc();
            }
        }
        if (!(pDbModState->viewFlag & 4) && !(em->motInfo[0].flags & 0x4000)) {
            if (pDbModState->viewFlag & 8) {
                lim = 50000.0f;
            } else {
                lim = 10000.0f;
            }
            while (model->pos.x > lim) {
                model->pos.x -= lim + lim;
            }
            while (model->pos.x < -lim) {
                model->pos.x += lim + lim;
            }
            while (model->pos.y > lim) {
                model->pos.y -= lim + lim;
            }
            while (model->pos.y < -lim) {
                model->pos.y += lim + lim;
            }
            while (model->pos.z > lim) {
                model->pos.z -= lim + lim;
            }
            while (model->pos.z < -lim) {
                model->pos.z += lim + lim;
            }
            PartsWorldPosCalc(model);
        }
        model->partsWorldCalc();
        if (em->opt_flag & 1) {
            model->debugSkeletonDisp();
        }
    }
    if (allDone) {
        for (n = 0; n <= SLOT_NUM - 1; n++) {
            em = &dbModSlot[n];
            if (em->alive) {
                em->em_flag &= ~1;
            }
        }
    }

    for (n = 0; n < SLOT_NUM; n++) {
        em = &dbModSlotSub[n];
        if (em->alive == 0) {
            continue;
        }
        model = em->pEm;
        if (!model->isAlive()) {
            continue;
        }
        parent = dbModSlot[n].pEm;
        model->pos = em->pos0;
        model->ang = em->ang0;
        RotMatrix(model->mat, &model->ang);
        TransMatrix(model->mat, &model->pos);
        ScaleMatrix(model->mat, &model->scale);
        if (em->partsNo == -1) {
            PSMTXConcat(parent->mat, model->mat, model->mat);
        } else if (parent->pList) {
            pp = (cParts*) parent->getPartsPtr(em->partsNo);
            PSMTXConcat(pp->mat, model->mat, model->mat);
        }
        ax.x = model->mat[0][0];
        ax.y = model->mat[1][0];
        ax.z = model->mat[2][0];
        ay.x = model->mat[0][1];
        ay.y = model->mat[1][1];
        ay.z = model->mat[2][1];
        az2.x = model->mat[0][2];
        az2.y = model->mat[1][2];
        az2.z = model->mat[2][2];
#line 3929
        VECNormalize(&ax, &ax);
        VECNormalize(&ay, &ay);
        VECNormalize(&az2, &az2);
        model->mat[0][0] = ax.x;
        model->mat[1][0] = ax.y;
        model->mat[2][0] = ax.z;
        model->mat[0][1] = ay.x;
        model->mat[1][1] = ay.y;
        model->mat[2][1] = ay.z;
        model->mat[0][2] = az2.x;
        model->mat[1][2] = az2.y;
        model->mat[2][2] = az2.z;
        model->partsMatCalc();
        model->partsWorldCalc();
        if (em->opt_flag & 1) {
            model->debugSkeletonDisp();
        }
    }
}

// Copies the motion file name of slot `no`.
void dbModGetMotFilename(int no, char* dst)
{
    strcpy(dst, pDbModState->motName[no]);
}

static const char* dbmodTblUnit[5] = {"_SET", "_BIN", "_TPL", "_FCV", "_ONTO"};

// Locates the five units of mot_tbl.txt (_SET, _BIN, _TPL, _FCV, _ONTO) and counts their blocks.
void mottblInit(MotTbl* tbl)
{
    int i;
    int no;
    char* p;

    for (i = 0; i < 5; i++) {
        no = mottblUnitNum(tbl->data, dbmodTblUnit[i]);
        p = mottblNextLine(mottblUnitPtr(tbl->data, no));
        tbl->unit[i] = p;
        tbl->unitNum[i] = mottblUnitCount(p);
    }
}

// Copies one line (up to the CR LF) into dst; returns the next line or NULL past the end of the image.
char* mottblGetLine(char* dst, int max, char* src)
{
    int i;

    for (i = 0; i < max; i++, src++) {
        if (src[0] == '\r' && src[1] == '\n') {
            src += 2;
            break;
        }
        dst[i] = src[0];
    }
    dst[i] = 0;
    if (src > m_MotTbl->end) {
        return 0;
    }
    return src;
}

// Skips comment ("//") and empty lines.
char* mottblNextLine(char* p)
{
    char line[0x100];
    int skip;

    do {
        p = mottblGetLine(line, 0x100, p);
        skip = strncmp("//", line, 2) == 0;
        if (line[0] == 0) {
            skip = 1;
        }
    } while (skip);
    if (p > m_MotTbl->end) {
        return 0;
    }
    return p;
}

// Number of entries of the block p points into (nested "{ }" blocks count as one).
int mottblUnitCount(char* p)
{
    char line[0x100];
    int skip = 0;
    int depth = 0;
    int count = 0;
    int cont = 1;

    while (p) {
        p = mottblGetLine(line, 0x100, p);
        if (strncmp("//", line, 2) == 0) {
            skip = 1;
        }
        if (line[0] == 0) {
            skip = 1;
        }
        if (!skip) {
            switch (depth) {
            case 0:
                if (line[strlen(line) - 1] == '}') {
                    cont = 0;
                } else {
                    count++;
                    if (line[strlen(line) - 1] == '{') {
                        depth = 1;
                    }
                }
                break;
            case 1:
                if (line[strlen(line) - 1] == '{') {
                    depth = 2;
                } else if (line[strlen(line) - 1] == '}') {
                    depth = 0;
                }
                break;
            default:
                if (line[strlen(line) - 1] == '{') {
                    depth++;
                } else if (line[strlen(line) - 1] == '}') {
                    depth--;
                }
                break;
            }
        }
        skip = 0;
        if (!cont) {
            break;
        }
    }
    return count;
}

// Index of the entry named `name` in the block p points into.
int mottblUnitNum(char* p, const char* name)
{
    char line[0x100];
    int skip = 0;
    int depth = 0;
    int no = 0;
    int cont = 1;

    while (p) {
        p = mottblGetLine(line, 0x100, p);
        if (strncmp("//", line, 2) == 0) {
            skip = 1;
        }
        if (line[0] == 0) {
            skip = 1;
        }
        if (!skip) {
            switch (depth) {
            case 0:
                if (line[strlen(line) - 1] == '}') {
                    OSReport("Label[%s] not found.\n", name);
                    cont = 0;
                    no = 0;
                } else if (line[strlen(line) - 1] == '{') {
                    if (strncmp(name, line, strlen(name)) == 0) {
                        cont = 0;
                    } else {
                        no++;
                        depth = 1;
                    }
                } else {
                    no++;
                }
                break;
            case 1:
                if (line[strlen(line) - 1] == '{') {
                    depth = 2;
                } else if (line[strlen(line) - 1] == '}') {
                    depth = 0;
                }
                break;
            default:
                if (line[strlen(line) - 1] == '{') {
                    depth++;
                } else if (line[strlen(line) - 1] == '}') {
                    depth--;
                }
                break;
            }
        }
        skip = 0;
        if (!cont) {
            break;
        }
    }
    return no;
}

// Start of entry `no` of the block p points into (p itself when the block has fewer entries).
char* mottblUnitPtr(char* start, int no)
{
    char line[0x100];
    char* p = start;
    char* prev;
    int skip = 0;
    int depth = 0;
    int n = 0;
    int cont = 1;
    char* ret = 0;

    while (p) {
        prev = p;
        p = mottblGetLine(line, 0x100, p);
        if (strncmp("//", line, 2) == 0) {
            skip = 1;
        }
        if (line[0] == 0) {
            skip = 1;
        }
        if (!skip) {
            switch (depth) {
            case 0:
                if (line[strlen(line) - 1] == '}') {
                    OSReport("Block #%d not found.\n", no);
                    cont = 0;
                    ret = start;
                } else {
                    if (line[strlen(line) - 1] == '{') {
                        depth = 1;
                    }
                    if (no == n++) {
                        ret = prev;
                        cont = 0;
                    }
                }
                break;
            case 1:
                if (line[strlen(line) - 1] == '{') {
                    depth = 2;
                } else if (line[strlen(line) - 1] == '}') {
                    depth = 0;
                }
                break;
            default:
                if (line[strlen(line) - 1] == '{') {
                    depth++;
                } else if (line[strlen(line) - 1] == '}') {
                    depth--;
                }
                break;
            }
        }
        skip = 0;
        if (!cont) {
            break;
        }
    }
    return ret;
}

// The file name part after the last '/'.
char* dbmodSkipPath(char* path)
{
    char* p;

    while ((p = strchr(path, '/')) != 0) {
        path = p + 1;
    }
    return path;
}

// The whole-stage light box of the viewer models (one .rodata copy of the two Vecs, in front of
// dbmodSetLight).
static inline void dbmodLightInit(cEm* model, int flag)
{
    static const Vec center = {0.0f, 0.0f, 0.0f};
    static const Vec size = {10000.0f, 10000.0f};

    model->LightInfo.init2(0, 1, &center, &size, flag);
}

// Light set of a slot model by lit_type: 1 pl, 2 em, 4 obj, 0x10 scr, 0x20 item (5 = none).
void dbmodSetLight(DB_EM* em)
{
    cEm* model = em->pEm;

    if (model == 0) {
        return;
    }
    switch (em->lit_type) {
    case 0:
        dbmodLightInit(model, 1);
        break;
    case 1:
        dbmodLightInit(model, 2);
        break;
    case 2:
        dbmodLightInit(model, 4);
        break;
    case 3:
        dbmodLightInit(model, 0x10);
        break;
    case 4:
        dbmodLightInit(model, 0x20);
        break;
    }
}

// Same as dbmodSetLight for this slot.
void DB_EM::setLight()
{
    switch (lit_type) {
    case 0:
        dbmodLightInit(pEm, 1);
        break;
    case 1:
        dbmodLightInit(pEm, 2);
        break;
    case 2:
        dbmodLightInit(pEm, 4);
        break;
    case 3:
        dbmodLightInit(pEm, 0x10);
        break;
    case 4:
        dbmodLightInit(pEm, 0x20);
        break;
    }
}

// Copies the bin and tex file lists into the slot's m_files[0..1] and starts the load steps.
int DB_EM::loadModelSet(DB_MODEL_FILES* bin, DB_MODEL_FILES* tex)
{
    switch (bin->m_mode) {
    case 1:
        m_files[0].set(bin->m_num, bin->m_name[0]);
        break;
    case 2:
        m_files[0].set(bin->m_num, bin->m_data);
        break;
    }
    switch (tex->m_mode) {
    case 1:
        m_files[1].set(tex->m_num, tex->m_name[0]);
        break;
    case 2:
        m_files[1].set(tex->m_num, tex->m_data);
        break;
    }
    m_load_model_rno = 0;
    if (pEm) {
        pEm->be_flag &= ~2;
    }
    return 0;
}

// 0: still loading, 1: done, -1: a file is missing
int DB_EM::loadModel()
{
    cModelInfo* info;
    int i;

    switch (m_load_model_rno) {
    case 0:
        switch (m_files[0].read(pBinBuff)) {
        case 0:
            m_load_model_rno = 1;
            break;
        case 1:
            return -1;
        case 2:
            break;
        }
        break;
    case 1:
        switch (m_files[1].read(pTplBuff)) {
        case 0:
            m_load_model_rno = 2;
            break;
        case 1:
            return -1;
        case 2:
            break;
        }
        break;
    case 2:
        strcpy(label, pDbModState->setName);
        for (i = 0; i < m_files[0].m_num; i++) {
            if (i == 0) {
                if (pEm == 0) {
                    pEm = EmMgr.create(0xFF);
                }
                if (pEm->modelInit(pBinBuff[i], pTplBuff[i]) == 0) {
                    pLog->err(0, 0, "dbmod_load_model() failed.");
                    return 0;
                }
            } else {
                info = ModInfoMgr.create(pBinBuff[i], pTplBuff[i]);
                if (info) {
                    pEm->addModel(info);
                }
            }
        }
        if (m_files[0].m_num) {
            setLight();
            pEm->be_flag |= 2;
        }
        return 1;
    }
    return 0;
}

// USAGE box of the MODEL page.
void model_usage()
{
    eprintf(40 * 8, 5 * 14, 7, 0, "------- USAGE -------");
    eprintf(40 * 8, 6 * 14, 7, 0, "STICK : Model Select ");
    eprintf(40 * 8, 7 * 14, 7, 0, "R     : Skip Forward ");
    eprintf(40 * 8, 8 * 14, 7, 0, "L     : Skip Backward");
}

// USAGE box of the MOTION page.
void motion_usage()
{
    eprintf(40 * 8, 5 * 14, 7, 0, "------- USAGE -------");
    eprintf(40 * 8, 6 * 14, 7, 0, "LFT/RGHT: Down/Up No.");
    eprintf(40 * 8, 7 * 14, 7, 0, "        : Reset No.  ");
    eprintf(40 * 8, 7 * 14, 5, 0, "Z                    ");
    eprintf(40 * 8, 8 * 14, 7, 0, "R       : Move x  10 ");
    eprintf(40 * 8, 9 * 14, 7, 0, "L       : Move x 100 ");
    eprintf(40 * 8, 10 * 14, 7, 0, " +UP/DWN: Change File");
    eprintf(40 * 8, 10 * 14, 7, 0, "Y                    ");
    eprintf(40 * 8, 11 * 14, 7, 0, "        : Load Motion");
    eprintf(40 * 8, 11 * 14, 4, 0, "A                    ");
}

// USAGE box of the BLEND page.
void blend_usage()
{
    eprintf(38 * 8, 5 * 14, 7, 0, "-------- USAGE --------");
    eprintf(38 * 8, 6 * 14, 7, 0, "LFT/RGHT: Down/Up Ratio");
    eprintf(38 * 8, 7 * 14, 7, 0, "        : Reset Ratio  ");
    eprintf(38 * 8, 7 * 14, 5, 0, "Z                      ");
    eprintf(38 * 8, 8 * 14, 7, 0, "R       : Move x  10   ");
    eprintf(38 * 8, 9 * 14, 7, 0, "L       : Move x 100   ");
}

// USAGE box of the LOCATE position mode (0 X-Z plane + Y, 1 per axis).
void position_usage(int mode)
{
    int x = 43;
    int y = 5;

    eprintf(43 * 8, 4 * 14, 7, 0, "------ USAGE ------");
    if (mode == 0) {
        eprintf(43 * 8, 5 * 14, 7, 0, "  U  :             ");
        y = 9;
        eprintf(43 * 8, 6 * 14, 7, 0, "L-+-R:             ");
        eprintf(43 * 8, 7 * 14, 7, 0, "  D  :             ");
        eprintf(43 * 8, 77, 7, 0, "     : Move        ");
        eprintf(43 * 8, 91, 7, 0, "     :  (X-Z plane)");
        eprintf(43 * 8, 8 * 14, 7, 0, "Y+U/D: Move(Y axis)");
    } else if (mode == 1) {
        eprintf(43 * 8, 5 * 14, 7, 0, "L<->R: Select Axis ");
        y = 7;
        eprintf(43 * 8, 6 * 14, 7, 0, "Up/Dn: Move Pos    ");
    }
    eprintf(x * 8, y++ * 14, 7, 0, "     : Reset Pos   ");
    eprintf(x * 8, (y - 1) * 14, 5, 0, "Z                  ");
    eprintf(x * 8, y++ * 14, 7, 0, "R    : Move x 0.1  ");
    eprintf(x * 8, y++ * 14, 7, 0, "L    : Move x 0.01 ");
    if (mode == 0) {
        eprintf(x * 8, y++ * 14, 7, 0, "X    :->           ");
        eprintf(x * 8, (y - 1) * 14, 0x16, 0, "         X,Y,Z Mode");
    } else if (mode == 1) {
        eprintf(x * 8, y++ * 14, 7, 0, "X    :->           ");
        eprintf(x * 8, (y - 1) * 14, 0x16, 0, "         X-Z,Y Mode");
    }
    eprintf(x * 8, y++ * 14, 7, 0, "     : Back        ");
    eprintf(x * 8, (y - 1) * 14, 2, 0, "B                  ");
}

// USAGE box of the LOCATE rotation mode.
void rotation_usage()
{
    eprintf(42 * 8, 4 * 14, 7, 0, "------ USAGE ------");
    eprintf(42 * 8, 5 * 14, 7, 0, "L<->R: Select Axis ");
    eprintf(42 * 8, 6 * 14, 7, 0, "Up/Dn: Move Angle  ");
    eprintf(42 * 8, 7 * 14, 7, 0, "     : Reset Angel ");
    eprintf(42 * 8, 7 * 14, 5, 0, "Z                  ");
    eprintf(42 * 8, 8 * 14, 7, 0, "R    : Move x 0.1  ");
    eprintf(42 * 8, 9 * 14, 7, 0, "L    : Move x 0.01 ");
    eprintf(42 * 8, 10 * 14, 7, 0, "     : Back        ");
    eprintf(42 * 8, 10 * 14, 2, 0, "B                  ");
}

// USAGE box of the SCALE page.
void scale_usage()
{
    eprintf(38 * 8, 4 * 14, 7, 0, "-------- USAGE --------");
    eprintf(38 * 8, 5 * 14, 7, 0, "LFT/RGHT: Down/Up Scale");
    eprintf(38 * 8, 6 * 14, 7, 0, "        : Reset Scale  ");
    eprintf(38 * 8, 6 * 14, 5, 0, "Z                      ");
    eprintf(38 * 8, 7 * 14, 7, 0, "R       : Move x 10   ");
    eprintf(38 * 8, 8 * 14, 7, 0, "L       : Move x 100  ");
}

// Copies the motion file list into m_files[2] and starts the motion load steps.
int DB_EM::loadMotionSet(DB_MODEL_FILES* mot)
{
    switch (mot->m_mode) {
    case 1:
        m_files[2].set(mot->m_num, mot->m_name[0]);
        break;
    case 2:
        m_files[2].set(mot->m_num, mot->m_data);
        break;
    }
    alive = m_load_motion_rno = 0;
    return 0;
}

// 0: still loading, 1: done, -1: a file is missing
int DB_EM::loadMotion()
{
    int i;

    mot_num = 0;
    mot_cnt = 0;
    switch (m_load_motion_rno) {
    case 0:
        switch (m_files[2].read(pMotBuff)) {
        case 0:
            m_load_motion_rno = 1;
            break;
        case 1:
            return -1;
        case 2:
            break;
        }
        break;
    case 1:
        for (i = 0; i < m_files[2].m_num; i++) {
            if (pMotBuff[i]) {
                motInfo[i].flags2 |= 0x20000000;
                if (pDbModState->blendMode == 0 && i > 0) {
                    motInfo[i].flags2 |= 0x10000000;
                }
                MotionSetCore(pEm, &motInfo[i], pMotBuff[i], 0, 0, motInfo[0].flags | 0x200, 0);
                mot_num++;
            }
            switch (i) {
            case 0:
                *(DbMotWork*) &pEm->Motion = motInfo[0];
                pEm->pos = pos0;
                pEm->ang = ang0;
                break;
            case 1:
                motInfo[1].blendRate = 0.0f;
                break;
            }
        }
        alive = 1;
        return 1;
    }
    return 0;
}

static const char* dbmodIkLabel[3] = {"IK    ", "IK TOE", "IK ARM"};
static int dbmodIkX = 50;
static int dbmodIkY = 8;
static int dbmodIkUnused = 0;

// Prints the model's IK joint table (joint kind / parts number per joint) at dbmodIkX/Y.
void DB_EM::IKreport()
{
    MotionWork* mw;
    int i;
    int type;

    if (pEm == 0) {
        return;
    }
    {
    int x = dbmodIkX;
    int y = dbmodIkY;
    mw = &pEm->Motion;
    for (i = 0; i < mw->Joint_num; i++) {
        int info = mw->pJoint_kind[i] & 0xFF;
        // a second variable holding the byte: cse1 rewrites the first two tests to the older pseudo,
        // the copy (`mr r11,r0`) survives for the third test in the join block
        int flag = info;
        u8 no = mw->pJoint_no[i];
        type = -1;
        if (info & 0x30) {
            type = 0;
            if (info & 0x20) {
                type = 1;
            }
            if (flag & 0x80) {
                type = 2;
            }
        }
        if (type != -1) {
            eprintf(x * 8, y * 14, 2, 0, "%s[%02d]", dbmodIkLabel[type], no);
            y++;
        }
    }
    }
}

// Label (set name) of slot 0.
char* dbModBinName()
{
    return dbModSlot[0].label;
}

// Index of the model set called `name` in mot_tbl.txt, -1 when absent.
int GetSlctSetNo(char* name)
{
    char line[0x100];
    char* p;
    int i;

    for (i = 0; i < m_MotTbl->unitNum[0]; i++) {
        mottblGetLine(line, 0x100, mottblUnitPtr(m_MotTbl->unit[0], i));
        for (p = line; *p != ' ' && *p != '{'; p++) {
        }
        *p = 0;
        if (strcmp(line, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Loads model set `name` with motion number `motNum` into slot `no` (t_event / t_esp event models):
// selects the set, resolves the names and runs dbModelLoad. 0 when the set is unknown.
int LoadModelSetName(char* name, int motNum, int no)
{
    int setNo = GetSlctSetNo(name);

    if (setNo == -1) {
        return 0;
    }
    pDbModState->no = no;
    pDbModState->setNo = setNo;
    dbmodGetSet();
    pDbModState->motSub[0] = 0;
    pDbModState->motNum[0] = motNum;
    dbmodGetFilenames();
    {
    DB_MODEL_FILES bin;
    DB_MODEL_FILES tex;
    DB_MODEL_FILES mot;
    bin.init();
    bin.set(pDbModState->binNum, pDbModState->name[0][0]);
    tex.init();
    tex.set(pDbModState->texNum, pDbModState->name[1][0]);
    mot.init();
    mot.set(pDbModState->motFileNum, pDbModState->motName[0]);
    return dbModelLoad(no, &bin, &tex, &mot);
    }
}

// Root translation mode of slot `no`'s motion 0: 0 on (0x11), 1 off, 2 add (1), 3 infinite (0x4001).
void SetTransMode(int mode, int no)
{
    DB_EM* em = &dbModSlot[no];
    u16* flag = &em->motInfo[0].flags;

    switch (mode) {
    case 0:
        em->motInfo[0].flags |= 0x11;
        em->motInfo[0].flags &= ~0x4000;
        break;
    case 1:
        em->motInfo[0].flags &= ~0x4011;
        break;
    case 2:
        em->motInfo[0].flags |= 1;
        em->motInfo[0].flags &= ~0x4010;
        break;
    case 3:
        em->motInfo[0].flags &= ~0x10;
        em->motInfo[0].flags |= 0x4001;
        break;
    }
    em->motInfo[0].flags = *flag;
}

// Loop flag (bit 2) of slot `no`'s motion 0.
void SetLoopFlag(int on, int no)
{
    DB_EM* em = &dbModSlot[no];
    u16* flag = &em->motInfo[0].flags;

    if (on) {
        em->motInfo[0].flags |= 4;
    } else {
        em->motInfo[0].flags &= ~4;
    }
    em->motInfo[0].flags = *flag;
}

// X-mirror flag (0x40) of slot `no`'s motion 0.
void SetXFlipFlag(int on, int no)
{
    DB_EM* em = &dbModSlot[no];
    u16* flag = &em->motInfo[0].flags;

    if (on) {
        em->motInfo[0].flags |= 0x40;
    } else {
        em->motInfo[0].flags &= ~0x40;
    }
    em->motInfo[0].flags = *flag;
}

// The enemy (model) of slot `no`, 0 for a bad slot.
cEm* dbModGetEmPtr(u32 no)
{
    if (no > SLOT_NUM - 1) {
        return 0;
    }
    return dbModSlot[no].pEm;
}

// Attaches this slot to slot `parentNo`'s parts (-1 = the model) with local offset / rotation.
void DB_EM::setParent(s8 parentNo, s16 parts, Vec* p, Vec* r)
{
    this->parentNo = parentNo;
    partsNo = parts;
    pos0 = *p;
    ang0 = *r;
    pEm_parent = dbModSlot[this->parentNo].pEm;
}

// Parent/child link between two slots; viewFlag bit 0 makes dbModMotionMove apply it.
void dbModelParentChild(s8 no, s8 parentNo, s8 parts, Vec* pos, Vec* rot)
{
    dbModSlot[no].setParent(parentNo, parts, pos, rot);
    pDbModState->viewFlag |= 1;
}

// Loads a model (bin/tex lists) and its motions into slot `no` synchronously (runs the load steps
// to completion); 0 on a bad slot or a failed read.
int dbModelLoad(int no, DB_MODEL_FILES* bin, DB_MODEL_FILES* tex, DB_MODEL_FILES* mot)
{
    DB_EM* em = &dbModSlot[no];
    int r;

    if (no > SLOT_NUM - 1) {
        pLog->err(0, 0, "dbModelLoad(): no(= %2d) > %2d", no, SLOT_NUM - 1);
        return 0;
    }
    em->loadModelSet(bin, tex);
    while ((r = em->loadModel()) == 0) {
        dbmodDispModelName();
        TaskSleep(1);
    }
    if (r == -1) {
        pLog->err(0, 0, "dbModelLoad(): loadModel failed.");
        return 0;
    }
    em->loadMotionSet(mot);
    while ((r = em->loadMotion()) == 0) {
        dbmodDispModelName();
        TaskSleep(1);
    }
    if (r == -1) {
        pLog->err(0, 0, "dbModelLoad(): loadMotion failed.");
        return 0;
    }
    return 1;
}

// 1 when slot `no` holds a model.
int dbModelIsAlive(int no)
{
    return dbModSlot[no].pEm ? 1 : 0;
}

// Rest position of slot `no` (restored at every motion restart).
void dbModelSetPos0(int no, Vec* pos)
{
    dbModSlot[no].pos0 = *pos;
}

// Rest rotation of slot `no`.
void dbModelSetAng0(int no, Vec* rot)
{
    dbModSlot[no].ang0 = *rot;
}

// Copies `cam` (pos, at, roll, fovy) into slot `no`'s motion attach camera.
void dbModelSetCamera(int no, CAMERA* cam)
{
    AttachCamera* ac = dbModSlot[no].motInfo[0].cam;

    ac->camera_data[0] = cam->param.pos;
    ac->camera_data[1] = cam->param.at;
    ac->camera_data[2].y = cam->param.roll;
    ac->camera_data[3].y = cam->param.fovy;
    ac->camera_data[4].y = 0.0f;
}
