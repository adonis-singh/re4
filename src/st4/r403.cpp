#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj13.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emwindow.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "motion.h"
#include "atariInfo.h"
#include "mercenaries.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "rnd.h"
#include "TexRender.h"
#include "db_log.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

// Room 4-03 (D:/Bio4/Prog/r403.cpp): the Mercenaries castle; the enemy resets per area, the two
// gatling gunners after enough kills, the slide down the banister and the three treasure cases.

struct R403Work {
    cObj* slide;         // 0x00  the banister slide object (SetObjSmd)
    u32 cnt;             // 0x04  enemies alive (SceCountEmAlive)
    u32 x8;
    u32 base;            // 0x0C  enemies alive when the count started (+1 per reset)
    u32 timer;           // 0x10  frames since the last em_destroy pass (1..300)
    u8 pad_14[0x2C - 0x14];
    TexRenderMng* tex;   // 0x2C
};

// One-member struct: every store through the work reloads the pointer.
struct R403WorkPtr {
    R403Work* p;
};

// Typed view of pG->emlist (r400): pG is loaded before the index shift.
struct EmListView {
    u8 pad[0x52E8];
    EmListData Em_list[0x100];
};
#define EM_LIST_V(no) (((EmListView*) pG)->Em_list[(no)])

static u8 r403_texTbl[0x20];
static R403WorkPtr r403_work;

static Vec r403_pos[3] = {{31558.0f, 8314.0f, 38823.0f}, {49963.0f, 20536.0f, 17277.0f}, {51563.0f, 11411.0f, -5610.0f}};
static Vec r403_rot[3] = {{0.0f, 2.345f, 0.0f}, {0.0f, -0.9f, 0.0f}, {0.0f, 1.25f, 0.0f}};

// The room's MercSysInitRoom parameters are 0x6C bytes (the DOL reads the first 0x5C).
struct R403MercInit {
    MercInit m;
    u32 x5C[4];
};

// Struct view of pPL: the in-struct load is invalidated by the collision flag stores (pPL reloaded per statement).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Store through a reference: the following pG load stays below it.
static inline void PSet(cObj*& d, cObj* v) { d = v; }

// COMPILER-DIFF 4: the list entry number is passed to the s16 parameter untruncated.
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

static void r403_DuraluminCaseOpen(int no);
static void r403_DuraluminCaseOpened(int no);
static int em_reset(int no, int chk);
static void reset_40();
static void reset_41();
static void reset_42();
static void reset_43();
static void reset_44();
static void reset_45();
static void reset_46();
void reset_47();
void reset_48();
void reset_49();
void reset_4a();
void reset_4b();
void reset_4c();
void reset_4d();
void reset_4e();
void reset_4f();
void reset_50();
void reset_51();
void reset_52();
void reset_53();
void reset_54();
void reset_55();
static void em_destroy();
void emset_gatling(int no);
static void slide_move();
static void setTexRender();
static void setLadderMotion(int no);

// Room init (Mercenaries: the castle): Ada's ladder motions, window 0x1E pre-broken and hidden, the
// window break motions, the banister slide object with its motion (area 0xB); the Mercenaries system
// with the room's messages; three duralumin case item events.
void R403Init()
{
    cEm* win;

#line 53 "D:/Bio4/Prog/r403.cpp"
    r403_work.p = (R403Work*) MEM_CALLOC(sizeof(R403Work), 1, 0xd);
    setLadderMotion(0);
    setLadderMotion(0x1D);
    if (getRoomEtcWindow(0x1E, &win, 1)) {
        ((cEmWindow*) win)->SetBreakModel();
        win->be_flag &= ~2;
    }
    EvtMgr.SetEmWindowFcv(ROOM_ARC_PTR(pG->pRoom, 0x36), ROOM_ARC_PTR(pG->pRoom, 0x37), ROOM_ARC_PTR(pG->pRoom, 0x38));
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    {
        PSet(r403_work.p->slide, SetObjSmd(ROOM_ARC_PTR(pG->pRoom, 0x2A), ROOM_ARC_PTR(pG->pRoom, 0x2B), &pos, &rot, 0x10, 1));
    }
    if (pG->pl_type == 2) {
        r403_work.p->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2D), 0, 0, 1, 0);
    } else {
        r403_work.p->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 0, 0, 1, 0);
    }
    int one = 1;
    r403_work.p->slide->Motion.Seq_speed = 0.0f;
    r403_work.p->slide->be_flag |= 0x1000;
    r403_work.p->slide->setNoSuspend(0);
    SceAtDataSet_exec(0xB, SCE_LEVEL10, 0, (TaskFunc) slide_move, 0, 1);
    setTexRender();
    R403MercInit init;
    memset(&init, 0, sizeof(init));
    {
        u8 n = Rnd() % 3;

        init.m.pos = r403_pos[n];
        init.m.rot = r403_rot[n];
        init.m.x18 = 0;
        init.m.smdMot = ROOM_ARC_PTR(pG->pRoom, 0x2E);
        init.m.x20 = 30000;
        init.m.mesStart = one;
        init.m.mesA8 = 0xC;
        init.m.mesAC = 0xD;
        init.m.x58 = 0xE;
        init.m.mes[0] = 2;
        init.m.mes[1] = 3;
        init.m.mes[2] = 4;
        init.m.mes[3] = 5;
        init.m.mes[4] = 6;
        init.m.mes[5] = 7;
        init.m.mes[6] = 8;
        init.m.mes[7] = 9;
        init.m.mes[8] = 0xA;
        init.m.mes[9] = 0xB;
        switch ((u32) n) {
        case 0:
            setEm(0, -1, 1, 1, 1);
            setEm(1, -1, 1, 1, 1);
            setEm(2, -1, 1, 1, 1);
            setEm(3, -1, 1, 1, 1);
            setEm(4, -1, 1, 1, 1);
            setEm(5, -1, 1, 1, 1);
            setEm(0xB, -1, 1, 1, 1);
            setEm(0x11, -1, 1, 1, 1);
            setEm(0x17, -1, 1, 1, 1);
            setEm(0x1D, -1, 1, 1, 1);
            break;
        case 1:
            setEm(0x8C, -1, 1, 1, 1);
            setEm(0x8D, -1, 1, 1, 1);
            setEm(0x8E, -1, 1, 1, 1);
            setEm(0x8F, -1, 1, 1, 1);
            setEm(0x90, -1, 1, 1, 1);
            setEm(0x91, -1, 1, 1, 1);
            setEm(0x92, -1, 1, 1, 1);
            setEm(0x93, -1, 1, 1, 1);
            setEm(0x94, -1, 1, 1, 1);
            setEm(0x95, -1, 1, 1, 1);
            break;
        case 2:
            setEm(0x96, -1, 1, 1, 1);
            setEm(0x97, -1, 1, 1, 1);
            setEm(0x98, -1, 1, 1, 1);
            setEm(0x99, -1, 1, 1, 1);
            setEm(0x9A, -1, 1, 1, 1);
            setEm(0x9B, -1, 1, 1, 1);
            setEm(0x9C, -1, 1, 1, 1);
            setEm(0x9D, -1, 1, 1, 1);
            setEm(0x9E, -1, 1, 1, 1);
            setEm(0x9F, -1, 1, 1, 1);
            break;
        }
        MercSysInitRoom(&init.m);
    }
    SceSetItemEvent(0x2A, 0x80, 4, -1, r403_DuraluminCaseOpen, (void (*)()) r403_DuraluminCaseOpened, 0x15, 0);
    SceSetItemEvent(0x2B, 0x82, 5, -1, r403_DuraluminCaseOpen, (void (*)()) r403_DuraluminCaseOpened, 0x14, 0);
    SceSetItemEvent(0x2C, 0x83, 6, -1, r403_DuraluminCaseOpen, (void (*)()) r403_DuraluminCaseOpened, 0x17, 0);
}

// Item-event opener: case `no` (lid up -X) opens.
static void r403_DuraluminCaseOpen(int no)
{
    OpenBoxMain(OpenBoxPartsUpXM, 0, 0x5B, no, -1, -1);
}

// Item-event "already opened": case `no` posed open.
static void r403_DuraluminCaseOpened(int no)
{
    OpenBoxMain(OpenBoxPartsUpXM, 1, -1, no, -1, -1);
}

// Resets list entry `no` (chk: only while fewer than 10 enemies are alive); 1 when it was set.
static int em_reset(int no, int chk)
{
    if (chk == 1 && r403_work.p->cnt > 9) {
        return 0;
    }
    if (EM_LIST_V(no).be_flag & 2) {
        return 0;
    }
    cEmWrap em;
    cEmWrapSetEmI(&em, no, -1, 1, 1, 1);
    em.setGoto(&pPL->pos, 0xC);
    r403_work.p->base++;
    r403_work.p->cnt++;
    SceDebugDisp("RESET[%d]", no);
    return 1;
}

// Reset group of area zone 0 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_40()
{
    SceDebugDisp("RESET_AREA[0]");
    em_reset(6, 1);
    em_reset(7, 1);
    em_reset(8, 1);
    em_reset(9, 1);
    em_reset(0xA, 1);
}

// Reset group of area zone 1 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_41()
{
    SceDebugDisp("RESET_AREA[1]");
    em_reset(0xC, 1);
    em_reset(0xD, 1);
    em_reset(0xE, 1);
    em_reset(0xF, 1);
    em_reset(0x10, 1);
}

// Reset group of area zone 2 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_42()
{
    SceDebugDisp("RESET_AREA[2]");
    em_reset(0x12, 1);
    em_reset(0x13, 1);
    em_reset(0x14, 1);
    em_reset(0x15, 1);
    em_reset(0x16, 1);
}

// Reset group of area zone 3 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_43()
{
    SceDebugDisp("RESET_AREA[3]");
    em_reset(0x18, 1);
    em_reset(0x19, 1);
    em_reset(0x1A, 1);
    em_reset(0x1B, 1);
    em_reset(0x1C, 1);
}

// Reset group of area zone 4 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_44()
{
    SceDebugDisp("RESET_AREA[4]");
    em_reset(0x1E, 1);
    em_reset(0x1F, 1);
    em_reset(0x20, 1);
    em_reset(0x21, 1);
    em_reset(0x22, 1);
}

// Reset group of area zone 5 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_45()
{
    SceDebugDisp("RESET_AREA[5]");
    em_reset(0x24, 1);
    em_reset(0x25, 1);
    em_reset(0x26, 1);
    em_reset(0x27, 1);
    em_reset(0x28, 1);
}

// Reset group of area zone 6 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
static void reset_46()
{
    SceDebugDisp("RESET_AREA[6]");
    em_reset(0x2A, 1);
    em_reset(0x2B, 1);
    em_reset(0x2C, 1);
    em_reset(0x2D, 1);
    em_reset(0x2E, 1);
}

// Reset group of area zone 7 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_47()
{
    SceDebugDisp("RESET_AREA[7]");
    em_reset(0x30, 1);
    em_reset(0x31, 1);
    em_reset(0x32, 1);
}

// Reset group of area zone 8 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_48()
{
    SceDebugDisp("RESET_AREA[8]");
    em_reset(0x34, 1);
    em_reset(0x35, 1);
    em_reset(0x36, 1);
    em_reset(0x37, 1);
    em_reset(0x38, 1);
}

// Reset group of area zone 9 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_49()
{
    SceDebugDisp("RESET_AREA[9]");
    em_reset(0x3A, 1);
    em_reset(0x3B, 1);
    em_reset(0x3C, 1);
    em_reset(0x3D, 1);
    em_reset(0x3E, 1);
}

// Reset group of area zone a (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_4a()
{
    SceDebugDisp("RESET_AREA[a]");
    em_reset(0x40, 1);
    em_reset(0x40, 1);
    em_reset(0x42, 1);
    em_reset(0x43, 1);
}

// Reset group of area zone b (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_4b()
{
    SceDebugDisp("RESET_AREA[b]");
    em_reset(0x46, 1);
    em_reset(0x47, 1);
    em_reset(0x48, 1);
    em_reset(0x49, 1);
    em_reset(0x4A, 1);
}

// Reset group of area zone c (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_4c()
{
    SceDebugDisp("RESET_AREA[c]");
    em_reset(0x4C, 1);
    em_reset(0x4D, 1);
    em_reset(0x4E, 1);
    em_reset(0x4F, 1);
    em_reset(0x50, 1);
}

// Reset group of area zone d (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_4d()
{
    SceDebugDisp("RESET_AREA[d]");
    em_reset(0x52, 1);
    em_reset(0x53, 1);
    em_reset(0x54, 1);
    em_reset(0x55, 1);
}

// Reset group of area zone e (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_4e()
{
    SceDebugDisp("RESET_AREA[e]");
    em_reset(0x58, 1);
    em_reset(0x59, 1);
    em_reset(0x5A, 1);
    em_reset(0x5B, 1);
    em_reset(0x5C, 1);
}

// Reset group of area zone f (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_4f()
{
    SceDebugDisp("RESET_AREA[f]");
    em_reset(0x5E, 1);
    em_reset(0x5F, 1);
    em_reset(0x60, 1);
    em_reset(0x61, 1);
    em_reset(0x62, 1);
}

// Reset group of area zone 10 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_50()
{
    SceDebugDisp("RESET_AREA[10]");
    em_reset(0x83, 1);
    em_reset(0x84, 1);
    em_reset(0x85, 1);
    em_reset(0x86, 1);
    em_reset(0x87, 1);
}

// Reset group of area zone 11 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_51()
{
    SceDebugDisp("RESET_AREA[11]");
    em_reset(0x6A, 1);
    em_reset(0x6B, 1);
    em_reset(0x6C, 1);
    em_reset(0x6D, 1);
    em_reset(0x6E, 1);
    em_reset(0x6F, 1);
}

// Reset group of area zone 12 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_52()
{
    SceDebugDisp("RESET_AREA[12]");
    em_reset(0x64, 1);
    em_reset(0x65, 1);
    em_reset(0x66, 1);
    em_reset(0x67, 1);
}

// Reset group of area zone 13 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_53()
{
    SceDebugDisp("RESET_AREA[13]");
    em_reset(0x71, 1);
    em_reset(0x72, 1);
    em_reset(0x73, 1);
    em_reset(0x74, 1);
    em_reset(0x75, 1);
}

// Reset group of area zone 14 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_54()
{
    SceDebugDisp("RESET_AREA[14]");
    em_reset(0x77, 1);
    em_reset(0x78, 1);
    em_reset(0x79, 1);
    em_reset(0x7A, 1);
}

// Reset group of area zone 15 (a Room_flg[2] bit): its list entries re-set while fewer than 10 enemies are alive.
void reset_55()
{
    SceDebugDisp("RESET_AREA[15]");
    em_reset(0x7C, 1);
    em_reset(0x7D, 1);
    em_reset(0x7E, 1);
    em_reset(0x7F, 1);
    em_reset(0x80, 1);
    em_reset(0x81, 1);
}

// Destroys the resettable enemies (ids 0x10..0x20) so they can be set again.
static void em_destroy()
{
    u32 i;
    int lo = 0x10;  // a variable: `id >= 0x10` would fold to `id > 0xF`
    int hi = 0x20;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        int id;

        if (hi == -1) {
            hi = 0x10;
        }
        id = em->id;
        if (id >= lo && id <= hi && (em->be_flag & 0x201) == 1) {
            if (((cEmGanado*) em)->ckResetEnable()) {
                EmMgr.destroy(em);
            }
        }
    }
}

// A gatling gunner at the list entry's position.
void emset_gatling(int no)
{
    int list;

    EM_LIST_V(no).be_flag &= ~2;
    list = pG->em_list_no;
    if (list >= 0) {
        u32* tbl = (u32*) (list * 0x20 + (u32) pG + 0x501C);

        tbl[(u32) no >> 5] &= ~(0x80000000 >> (no & 31));
    }
    cEmWrap em;
    EmListData* l;
    Vec pos;
    Vec ang;
    f32 ry;

    cEmWrapSetEmI(&em, no, -1, 1, 1, 1);
    if (em.isActive()) {
        em.setFindPL();
    }
    if (pG->Room_flg[3] & 0x80000000) {
        l = EM_LIST(0x89);
    } else {
        l = EM_LIST(0x8A);
    }
    pos.x = (f32) l->pos[0] * 10.0f;
    pos.y = (f32) l->pos[1] * 10.0f;
    pos.z = (f32) l->pos[2] * 10.0f;
    ry = (f32) (l->rot[1] * 360 / 32768);
    em.setPos(&pos);
    ang.x = 0.0f;
    ang.y = ry;
    ang.z = 0.0f;
    em.setAng(&ang);
}

// Per frame while the game runs: counts the alive Ganados (the base on the first frame), em_destroy
// every 300 frames, the time points; the reset group of each zone the player is in (Room_flg[2] bits
// 31 down to 10) refills; after 20 kills the first gatling gunner (0x89), after 45 the second (0x8A).
void R403Main()
{
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    if (!StaFlagChk(pG, STA_EVENT)) {
        if (r403_work.p->timer == 1 && !(pG->Room_flg[0] & 0x80000000)) {
            U32Set(r403_work.p->base, r403_work.p->cnt);
            pG->Room_flg[0] |= 0x80000000;
        }
        r403_work.p->timer++;
        if (r403_work.p->timer > 300) {
            r403_work.p->timer = 1;
            em_destroy();
        }
        r403_work.p->cnt = SceCountEmAlive(0x10, 0x20);
        SceDebugDisp("EM_NUM[%d/%d]", r403_work.p->cnt, r403_work.p->base);
        if (r403_work.p->timer > 1800) {
            if (pG->Room_flg[2] & 0x80000000) {
                reset_40();
            }
        }
        if (pG->Room_flg[2] & 0x40000000) {
            reset_41();
        }
        if (pG->Room_flg[2] & 0x20000000) {
            reset_42();
        }
        if (pG->Room_flg[2] & 0x10000000) {
            reset_43();
        }
        if (pG->Room_flg[2] & 0x08000000) {
            reset_44();
        }
        if (pG->Room_flg[2] & 0x04000000) {
            reset_45();
        }
        if (pG->Room_flg[2] & 0x02000000) {
            reset_46();
        }
        if (pG->Room_flg[2] & 0x01000000) {
            reset_47();
        }
        if (pG->Room_flg[2] & 0x00800000) {
            reset_48();
        }
        if (pG->Room_flg[2] & 0x00400000) {
            reset_49();
        }
        if (pG->Room_flg[2] & 0x00200000) {
            reset_4a();
        }
        if (pG->Room_flg[2] & 0x00100000) {
            reset_4b();
        }
        if (pG->Room_flg[2] & 0x00080000) {
            reset_4c();
        }
        if (pG->Room_flg[2] & 0x00040000) {
            reset_4d();
        }
        if (pG->Room_flg[2] & 0x00020000) {
            reset_4e();
        }
        if (pG->Room_flg[2] & 0x00010000) {
            reset_4f();
        }
        if (r403_work.p->timer > 1800) {
            if (pG->Room_flg[2] & 0x8000) {
                reset_50();
            }
        }
        if (pG->Room_flg[2] & 0x4000) {
            reset_51();
        }
        if (pG->Room_flg[2] & 0x2000) {
            reset_52();
        }
        if (pG->Room_flg[2] & 0x1000) {
            reset_53();
        }
        if (pG->Room_flg[2] & 0x800) {
            reset_54();
        }
        if (pG->Room_flg[2] & 0x400) {
            reset_55();
        }
        if (r403_work.p->base != 0 && r403_work.p->base - r403_work.p->cnt > 19) {
            if (!(pG->Room_flg[0] & 0x40000000)) {
                pG->Room_flg[0] |= 0x40000000;
                emset_gatling(0x89);
            }
        }
        if (r403_work.p->base != 0 && r403_work.p->base - r403_work.p->cnt > 44) {
            if (!(pG->Room_flg[0] & 0x20000000)) {
                pG->Room_flg[0] |= 0x20000000;
                emset_gatling(0x8A);
            }
        }
    }
}

// The slide down the banister: the player and the slide object run their motions together.
static void slide_move()
{
    cPlayer* pl = pPL;
    Vec v;
    u32 max;
    u32 i;

    pl->beginAction();
    pPLS->atari.clrFlag100();
    pPLS->atari.clrFlag200();
    pPLS->atari.setPriority(PRI_LV1);
    pPL->dmg.set(0, 0x80);
    pl->be_flag &= ~0x10;
    pl->setRightHand(1);
    pl->Wep->setTrans(0, 0);
    PlSetHand(1, 0);
    if (pG->pl_type == 2) {
        cModel* m = pPL;

        v.x = 58241.0f;
        v.y = 16600.2f;
        v.z = -11855.78f;
        m->setPos(&v);
        v.y = -0.18653207f;
        v.x = 0.0f;
        v.z = 0.0f;
        pPL->setAng(&v);
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x29), 0, 0, 0x201, 0);
        r403_work.p->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2D), 0, 0, 1, 0);
    } else {
        cModel* m = pPL;

        v.x = 58200.0f;
        v.y = 16588.34f;
        v.z = -11855.78f;
        m->setPos(&v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        pPL->setAng(&v);
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x28), 0, 0, 0x201, 0);
        r403_work.p->slide->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2C), 0, 0, 1, 0);
    }
    r403_work.p->slide->Motion.Seq_speed = 1.0f;
    SndCall(6, 0x18, 0, 0, 0, 0);
    max = (u32) MotionGetMaxFrame(&pPL->Motion);
    for (i = 0; i < max; i++) {
        if (i > 60 && i < max - 30) {
            PlWepHitCheck2(0, &pPL->pos, &pPL->pos, 0x13, 2, 1500.0f);
        }
        SceSleep(1);
        if (i == 80) {
            SndCall(6, 0x19, 0, 0, 0, 0);
        }
    }
    PlSetHand(0, 0);
    pl->setRightHand(1);
    pl->Wep->setTrans(1, 0);
    pl->endAction(5);
    pPL->dmg.clear();
    pPLS->atari.setFlag100();
    pPLS->atari.setFlag200();
    pPLS->atari.setPriority(0);
    pl->be_flag |= 0x10;
}

// The render target of the reflecting floor (object 0x16).
static void setTexRender()
{
    cObj* obj;
    u8* tbl = r403_texTbl;

    if (GetTexRenderMgr(&r403_work.p->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = r403_work.p->tex->texId;
        r403_work.p->tex->m_Rep_type = 1;
        EstSet(0, -1, 0, 0, 1, 0, r403_work.p->tex->mask | 1, 0, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x16);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->color[3] = 0xF0;
    obj->Shader_type = 2;
    obj->Refract_pow = 8;
    obj->Refract_ratio = 0x40;
}

// The ladder motions of the Ada game (her own climb set from the etc archive).
static void setLadderMotion(int no)
{
    cEm* ladder;
    void* das;

    if (getRoomEtcLadder(no, &ladder, 1)) {
        if (pG->pl_type == 2) {
            if (EtcGetDasAddr(6, &das)) {
                void* mot[20];

                mot[0] = ROOM_ARC_PTR(pG->pRoom, 0x2F);
                mot[1] = ROOM_ARC_PTR(pG->pRoom, 0x30);
                mot[2] = ROOM_ARC_PTR(pG->pRoom, 0x31);
                mot[3] = ROOM_ARC_PTR(pG->pRoom, 0x32);
                mot[4] = ROOM_ARC_PTR(pG->pRoom, 0x33);
                mot[5] = ROOM_ARC_PTR(pG->pRoom, 0x34);
                mot[6] = GetEtcAddr(das, "et06000.fcv");
                mot[7] = GetEtcAddr(das, "et06001.fcv");
                mot[8] = GetEtcAddr(das, "et06002.fcv");
                // struct view: the pG load stays below the mot[8] frame store (the target issues
                // the das reload for mot[10] first); a plain pG read is hoisted above it
                mot[9] = ROOM_ARC_PTR(pGS->pRoom, 0x35);
                mot[10] = GetEtcAddr(das, "et06003.fcv");
                mot[11] = GetEtcAddr(das, "et060000.seq");
                mot[12] = GetEtcAddr(das, "et060010.seq");
                mot[13] = GetEtcAddr(das, "et060020.seq");
                mot[14] = GetEtcAddr(das, "et060030.seq");
                mot[15] = GetEtcAddr(das, "et060031.seq");
                mot[16] = GetEtcAddr(das, "pl01106.fcv");
                mot[17] = GetEtcAddr(das, "pl01107.fcv");
                mot[18] = GetEtcAddr(das, "pl01108.fcv");
                mot[19] = GetEtcAddr(das, "pl01118.fcv");

                ((cObjLadder*) ladder)->setMotion(mot);
            }
        }
    }
}
