#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
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
#include "mercenaries.h"
#include "rnd.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

// Room 4-00 (D:/Bio4/Prog/r400.cpp): the Mercenaries village; the enemy resets per area, the
// bosses after enough kills and the three treasure boxes.

struct R400Work {
    u32 cnt;      // 0x00  enemies alive (SceCountEmAlive)
    u32 x4;
    u32 base;     // 0x08  enemies alive when the count started (+1 per reset)
    u32 timer;    // 0x0C  frames since the last em_destroy pass (1..300)
    u32 point;    // 0x10  frames of the point count (> 450: the second phase)
};

// One-member struct: every store through the work reloads the pointer.
struct R400WorkPtr {
    R400Work* p;
};

// Typed view of pG->emlist: the original indexes an EmListData array, so pG is loaded before the
// index shift and the table offset stays in the displacement (EM_LIST's byte form shifts first).
struct EmListView {
    u8 pad[0x52E8];
    EmListData Em_list[0x100];
};
#define EM_LIST_V(no) (((EmListView*) pG)->Em_list[(no)])

static R400WorkPtr r400_work;

Vec r400_pos[3] = {{-12400.0f, 2576.0f, 31080.0f}, {-7526.0f, 4232.0f, -8319.0f}, {25337.0f, 1110.0f, -704.0f}};
static Vec r400_rot[3] = {{0.0f, 2.486f, 0.0f}, {0.0f, 0.63f, 0.0f}, {0.0f, -1.66f, 0.0f}};

// The room's MercSysInitRoom parameters are 0x6C bytes (the DOL reads the first 0x5C).
struct R400MercInit {
    MercInit m;
    u32 x5C[4];
};

// COMPILER-DIFF 4: the list entry number is passed to the s16 parameter untruncated.
int cEmWrapSetEmI(cEmWrap* w, int no, int list, int errOn, int chkDead, int setAlive) asm("setEm__7cEmWrapsSciii");

static void r400_TreasureBoxOpen(int no);
static void r400_TreasureBoxOpened(int no);
void reset_40();
void reset_41();
void reset_42();
void reset_43();
void reset_44();
void reset_45();
void reset_46();
void reset_item0();
void reset_item1();
void reset_item2();
void setLadderMotion(int no);
void emset_boss(int no, int dir);
int em_reset(int no, int chk);
void em_destroy();

// Room init (Mercenaries: the village): Ada's ladder motions on the four ladders, the window break
// motions, window 0 pre-broken; the Mercenaries system with a random one of three start positions and
// the room's messages (MercSysInitRoom); three treasure box item events.
void R400Init()
{
    R400MercInit init;
    cEm* win;

#line 49 "D:/Bio4/Prog/r400.cpp"
    r400_work.p = (R400Work*) MEM_CALLOC(sizeof(R400Work), 1, 0xd);
    setLadderMotion(5);
    setLadderMotion(6);
    setLadderMotion(7);
    setLadderMotion(0x24);
    EvtMgr.SetEmWindowFcv(ROOM_ARC_PTR(pG->pRoom, 0x2E), ROOM_ARC_PTR(pG->pRoom, 0x2F), ROOM_ARC_PTR(pG->pRoom, 0x30));
    if (getRoomEtcWindow(0, &win, 1)) {
        ((cEmWindow*) win)->SetBreakModel();
    }
    MercSysInitStage();
    memset(&init, 0, sizeof(init));
    {
        u8 n = Rnd() % 3;

        init.m.pos = r400_pos[n];
        init.m.rot = r400_rot[n];
        init.m.x18 = 0;
        init.m.smdMot = ROOM_ARC_PTR(pG->pRoom, 0x31);
        init.m.x20 = 30000;
        init.m.mesStart = 1;
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
        MercSysInitRoom(&init.m);
    }
    SceSetItemEvent(0xB, 0x98, 3, -1, r400_TreasureBoxOpen, (void (*)()) r400_TreasureBoxOpened, 0x46, 0);
    SceSetItemEvent(0xD, 0x9A, 4, -1, r400_TreasureBoxOpen, (void (*)()) r400_TreasureBoxOpened, 0x47, 0);
    SceSetItemEvent(0xE, 0x9B, 5, -1, r400_TreasureBoxOpen, (void (*)()) r400_TreasureBoxOpened, 0x48, 0);
}

// Item-event opener: chest `no` lid up (+X).
static void r400_TreasureBoxOpen(int no)
{
    OpenBoxMain(OpenBoxUpXP, 0, 0x5B, no, -1, -1);
}

// Item-event "already opened": chest `no` posed open.
static void r400_TreasureBoxOpened(int no)
{
    OpenBoxMain(OpenBoxUpXP, 1, 0x5B, no, -1, -1);
}

// Reset group for area zone 0 (Room_flg[2] bit 31): its 16 list entries re-set while fewer than 10 are alive.
void reset_40()
{
    em_reset(0xB, 1);
    em_reset(0xC, 1);
    em_reset(0x40, 1);
    em_reset(0x41, 1);
    em_reset(0xD, 1);
    em_reset(0x42, 1);
    em_reset(0x43, 1);
    em_reset(0xE, 1);
    em_reset(0x44, 1);
    em_reset(0xF, 1);
    em_reset(2, 1);
    em_reset(3, 1);
    em_reset(4, 1);
    em_reset(0xA, 1);
    em_reset(0x10, 1);
    em_reset(0x13, 1);
}

// Reset group for zone 1 (Room_flg[2] 0x40000000): 12 list entries.
void reset_41()
{
    em_reset(0x24, 1);
    em_reset(0x25, 1);
    em_reset(0x26, 1);
    em_reset(0x27, 1);
    em_reset(0x19, 1);
    em_reset(0x1F, 1);
    em_reset(0x23, 1);
    em_reset(0x2B, 1);
    em_reset(0x34, 1);
    em_reset(0x3A, 1);
    em_reset(0x3B, 1);
    em_reset(0x3F, 1);
}

// Reset group for zone 2 (0x20000000): the list entries around the centre.
void reset_42()
{
    em_reset(0x64, 1);
    em_reset(0x65, 1);
    em_reset(0x66, 1);
    em_reset(0x46, 1);
    em_reset(0x47, 1);
    em_reset(0x48, 1);
    em_reset(0x1A, 1);
    em_reset(0x1B, 1);
    em_reset(0x49, 1);
    em_reset(0x4A, 1);
    em_reset(0x68, 1);
    em_reset(0x1C, 1);
    em_reset(0x1D, 1);
    em_reset(0x69, 1);
    em_reset(0x6A, 1);
    em_reset(0x67, 1);
    em_reset(0x49, 1);
    em_reset(0x1E, 1);
    em_reset(0x6B, 1);
    em_reset(0x6C, 1);
    em_reset(0x4B, 1);
    em_reset(0x1E, 1);
    em_reset(0x45, 1);
    em_reset(0x4C, 1);
    em_reset(0x59, 1);
    em_reset(0x84, 1);
    em_reset(0x80, 1);
    em_reset(0x89, 1);
    em_reset(0x8F, 1);
    em_reset(0x92, 1);
    em_reset(0xA8, 1);
    em_reset(0xB2, 1);
    em_reset(0xB6, 1);
    em_reset(0xBF, 1);
    em_reset(0xC4, 1);
}

// Reset group for zone 3 (0x10000000): 13 list entries.
void reset_43()
{
    em_reset(0x20, 1);
    em_reset(0x14, 1);
    em_reset(0x15, 1);
    em_reset(0x22, 1);
    em_reset(0x16, 1);
    em_reset(0x17, 1);
    em_reset(0x18, 1);
    em_reset(0x21, 1);
    em_reset(0x60, 1);
    em_reset(0x63, 1);
    em_reset(0x6D, 1);
    em_reset(0x72, 1);
    em_reset(0x73, 1);
}

// Reset group for zone 4 (0x08000000): 6 list entries.
void reset_44()
{
    em_reset(0x5A, 1);
    em_reset(0x5B, 1);
    em_reset(0x5E, 1);
    em_reset(0x5C, 1);
    em_reset(0x5D, 1);
    em_reset(0x5F, 1);
}

// Reset group for zone 5 (0x04000000): 14 list entries.
void reset_45()
{
    em_reset(0x55, 1);
    em_reset(0x57, 1);
    em_reset(0x53, 1);
    em_reset(0x4E, 1);
    em_reset(0x4D, 1);
    em_reset(0x4F, 1);
    em_reset(0x54, 1);
    em_reset(0x58, 1);
    em_reset(0x56, 1);
    em_reset(0x52, 1);
    em_reset(0x50, 1);
    em_reset(0x51, 1);
    em_reset(0x74, 1);
    em_reset(0x75, 1);
}

// Reset group for zone 6 (0x02000000): 8 list entries.
void reset_46()
{
    em_reset(0x2C, 1);
    em_reset(0x2E, 1);
    em_reset(0x30, 1);
    em_reset(0x32, 1);
    em_reset(0x2D, 1);
    em_reset(0x2F, 1);
    em_reset(0x31, 1);
    em_reset(0x33, 1);
}

// Reset group around treasure box 0: 3 list entries.
void reset_item0()
{
    em_reset(0x3C, 1);
    em_reset(0x3D, 1);
    em_reset(0x3E, 1);
}

// Reset group around treasure box 1: 5 list entries.
void reset_item1()
{
    em_reset(0x35, 1);
    em_reset(0x36, 1);
    em_reset(0x37, 1);
    em_reset(0x38, 1);
    em_reset(0x39, 1);
}

// Reset group around treasure box 2: 3 list entries.
void reset_item2()
{
    em_reset(0x28, 1);
    em_reset(0x29, 1);
    em_reset(0x2A, 1);
}

// Per frame while the game runs (Status_flg[0] 0x1000 clear): counts the alive Ganados, the base count
// on the first frame, em_destroy every 300 frames, the time points; after 450 frames (Room_flg[0]
// 0x40000000) the reset groups of the zone the player is in (Room_flg[2] bits) and of the box he is
// near refill; after 10 kills the boss pair (0x11/0x12), after 25 kills boss 0x61.
void R400Main()
{
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    if (!StaFlagChk(pG, STA_EVENT)) {
        r400_work.p->cnt = SceCountEmAlive(0x10, 0x20);
        if (r400_work.p->timer == 1 && !(pG->Room_flg[0] & 0x80000000)) {
            U32Set(r400_work.p->base, r400_work.p->cnt);
            pG->Room_flg[0] |= 0x80000000;
        }
        r400_work.p->timer++;
        if (r400_work.p->timer > 300) {
            r400_work.p->timer = 1;
            em_destroy();
        }
        GameAddPoint(LVADD_TIMECOUNT);
        r400_work.p->point++;
        if (r400_work.p->point > 450) {
            pG->Room_flg[0] |= 0x40000000;
        }
        if (pG->Room_flg[0] & 0x40000000) {
            SceDebugDisp("EM_NUM[%d/%d]", r400_work.p->cnt, r400_work.p->base);
            if (pG->Room_flg[2] & 0x80000000) {
                reset_40();
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
            if (SceAtItemFlgCk(0x80) == 1) {
                if (RsfCheck(G_ROOM_ID, 0) == 0) {
                    RsfSet(G_ROOM_ID, 0);
                    reset_item0();
                }
            }
            if (SceAtItemFlgCk(0x81) == 1) {
                if (RsfCheck(G_ROOM_ID, 1) == 0) {
                    RsfSet(G_ROOM_ID, 1);
                    reset_item1();
                }
            }
            if (SceAtItemFlgCk(0x82) == 1) {
                if (RsfCheck(G_ROOM_ID, 2) == 0) {
                    RsfSet(G_ROOM_ID, 2);
                    reset_item2();
                }
            }
            if (r400_work.p->base != 0 && r400_work.p->base - r400_work.p->cnt > 9) {
                if (!(pG->Room_flg[0] & 0x10000000)) {
                    pG->Room_flg[0] |= 0x10000000;
                    emset_boss(0x11, 0);
                    emset_boss(0x12, 1);
                }
            }
            if (r400_work.p->base != 0 && r400_work.p->base - r400_work.p->cnt > 24) {
                if (!(pG->Room_flg[0] & 0x08000000)) {
                    pG->Room_flg[0] |= 0x08000000;
                    emset_boss(0x61, 0);
                    emset_boss(0x62, 1);
                }
            }
            if (r400_work.p->base != 0 && r400_work.p->base - r400_work.p->cnt > 34) {
                if (!(pG->Room_flg[0] & 0x04000000)) {
                    pG->Room_flg[0] |= 0x04000000;
                    emset_boss(0x76, 0);
                    emset_boss(0x77, 1);
                }
            }
        }
    }
}

// The ladder motions of the Ada game (her own climb set from the etc archive).
void setLadderMotion(int no)
{
    cEm* ladder;
    void* das;

    if (getRoomEtcLadder(no, &ladder, 1)) {
        if (pG->pl_type == 2) {
            if (EtcGetDasAddr(6, &das)) {
                void* mot[20];

                mot[0] = ROOM_ARC_PTR(pG->pRoom, 0x27);
                mot[1] = ROOM_ARC_PTR(pG->pRoom, 0x28);
                mot[2] = ROOM_ARC_PTR(pG->pRoom, 0x29);
                mot[3] = ROOM_ARC_PTR(pG->pRoom, 0x2A);
                mot[4] = ROOM_ARC_PTR(pG->pRoom, 0x2B);
                mot[5] = ROOM_ARC_PTR(pG->pRoom, 0x2C);
                mot[6] = GetEtcAddr(das, "et06000.fcv");
                mot[7] = GetEtcAddr(das, "et06001.fcv");
                mot[8] = GetEtcAddr(das, "et06002.fcv");
                // struct view: the pG load stays below the mot[8] frame store (the target issues
                // the das reload for mot[10] first); a plain pG read is hoisted above it
                mot[9] = ROOM_ARC_PTR(pGS->pRoom, 0x2D);
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

// A boss enemy at the list entry's position (dir: the second one of the pair).
void emset_boss(int no, int dir)
{
    int list;

    EM_LIST_V(no).be_flag &= ~2;
    list = pG->em_list_no;
    if (list >= 0) {
        u32* tbl = (u32*) (list * 0x20 + (u32) pG + 0x501C);  // pG->Em_flg[list], em_set.cpp style

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
    if (pG->Room_flg[2] & 0x20000000) {
        if (dir == 0) {
            l = EM_LIST(0x11);
        } else {
            l = EM_LIST(0x12);
        }
    } else if (pG->Room_flg[3] & 0x80000000) {
        if (dir == 0) {
            l = EM_LIST(0x61);
        } else {
            l = EM_LIST(0x62);
        }
    } else {
        if (dir == 0) {
            l = EM_LIST(0x75);
        } else {
            l = EM_LIST(0x76);
        }
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

// Resets list entry `no` (chk: only while fewer than 10 enemies are alive); 1 when it was set.
int em_reset(int no, int chk)
{
    if (chk == 1 && r400_work.p->cnt > 9) {
        return 0;
    }
    if (EM_LIST_V(no).be_flag & 2) {
        return 0;
    }
    cEmWrap em;
    cEmWrapSetEmI(&em, no, -1, 1, 1, 1);
    em.setGoto(&pPL->pos, 0xC);
    r400_work.p->base++;
    r400_work.p->cnt++;
    SceDebugDisp("RESET[%d]", no);
    return 1;
}

// Destroys the resettable enemies of the village (ids 0x10..0x20) so they can be set again.
void em_destroy()
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
