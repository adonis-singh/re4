#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em10.h"
#include "em_set.h"
#include "emBarred.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "math_sub.h"
#include "stage.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "mes.h"
#include "act_btn.h"
#include "TexRender.h"
#include "db_log.h"
#include "wep_mod.h"
#include "st_mgr_event.h"

// Position and angle set together: both addresses are taken before the first call, so the angle
// pointer is kept in a register across setPos (`lwz; addi 0xa0` before `bl setPos`).
static inline void SetPosAng(cModel* m, Vec* pos, Vec* ang)
{
    m->setPos(pos);
    m->setAng(ang);
}

// Room 2-08 (D:/Bio4/Prog/r208.cpp): the castle courtyard with the water mill. The crank drains
// the moat and lowers the bridge, the two footings rise while Ashley turns the cranks on the far
// side, Ganado groups are reset by the areas the player crosses, and the enemies below the walls
// are fed in from the list while Ashley is carried over.

// The room work.
struct R208Work {
    TexRenderMng* tex;    // 0x000  water render target (setTexRender)
    int pad_004[3];       // 0x004
    cEmWrap em[32];       // 0x010
    cObj* crank;          // 0x190  crank / winch object the player or Ashley turns
    int pad_194;          // 0x194
    int emSetWait;        // 0x198  frames before the next enemy comes over the wall (R208Main)
    int side19C;          // 0x19C  alternates the wall entry (far side)
    int side1A0;          // 0x1A0  alternates the wall entry (near side)
    int gotoWait;         // 0x1A4  frames before the farthest enemy is sent to a new point
    int gotoIdx;          // 0x1A8  0..2 column of r208_gotoPos
    int footACnt;         // 0x1AC  Ashley's crank turns (footing A)
    int footBCnt;         // 0x1B0  Ashley's crank turns (footing B)
    f32 doorA;            // 0x1B4  gate A opening (0..150)
    f32 doorB;            // 0x1B8  gate B opening
    cEmWrap under[20];    // 0x1BC  enemies below the footings
    int underCnt;         // 0x2AC  under[] entries in use
    cSat* sat;            // 0x2B0  bridge collision
    int strOn;            // 0x2B4  battle stream playing
    int pad_2B8;          // 0x2B8
    int strFlag;          // 0x2BC  stream restarted after the bridge went down
    int ashleyCnt;        // 0x2C0  frames of Ashley's pointing / waving motion
    int crankSeCnt;       // 0x2C4  frames since the crank sound
    u32 crankSe;          // 0x2C8  SndCall handle of the crank
    u32 footASe;          // 0x2CC  SndCall handle of footing A
    u32 footBSe;          // 0x2D0  SndCall handle of footing B
};

static u8 r208_texTbl[0x20];
static R208Work* r208_work;
#define W r208_work
// EM_LIST through the struct view of pG: the load stays below a preceding work-struct store.
#define EM_LIST_S(no) (&pG->Em_list[no])
// Hit effects of attribute types 4 and 5
static const AtEffInfo r208_eff_info4 = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};
static const AtEffInfo r208_eff_info5 = {
    1, {1, 0x12}, {1, 0x15}, {1, 0x14}, {1, 0x13}, {1, 0x10}, {1, 0x10}, {1, 0x11}, {1, 0x15},
};

extern "C" {
// Also defined in r222.cpp (same module): static so the two objects do not clash in the -r link.
static void setResetNum(int n);
static u32 getResetNum();
static void incResetNum();
cEm* getMostFarEm(f32 range);
void setTexRender();
void emGroupeA_reset();
void emGroupeB1_reset();
void emGroupeB2_reset();
void emGroupeC_reset();
void emGroupeD_reset();
cEm* R208_setEm(s16 no);
cEm* R208_EmSetEvent(EmListData* d);
u32 getUnderEmNum();
void addUnderEmCnt();
void r208_CarryOnShoulder();
void r208_StrCheck();
void r208_continue();
}
static void em_all_destroy_task();
static void funcAshley(cEm* p);
static void funcAshley2(cEm* p);
static void funcAshley3(cEm* p);
static void asl_yubisasi();
static void setEmGo();
static void atari_exec_A();
static void atari_exec_B();
static void atari_exec_D();
static void brige1_down();
static void crank_set_exit();
static void crank_set();
static void r208_checkCrank();
static void r208_operateCrank();
static void under_set_task();
static void r208_snipe();
static void footingA_up_exit();
static void footingA_up();
static void footingB_up_exit();
static void footingB_up();
static void SubUnderCrankExec();

static Vec r208_satPos = {0.0f, 7000.0f, -40900.0f};
static Vec r208_zeroVec = {0.0f, 0.0f, 0.0f};
// Where the farthest enemy is sent, by the player's region (rows) and gotoIdx (columns).
static Vec r208_gotoPos[9][3] = {
    {{0.0f, 4000.0f, -4530.0f}, {0.0f, 4000.0f, -19860.0f}, {10779.0f, 4000.0f, -31260.0f}},
    {{-10779.0f, 4000.0f, -8344.0f}, {10779.0f, 4000.0f, -8344.0f}, {10779.0f, 4000.0f, -31260.0f}},
    {{0.0f, 4000.0f, -4530.0f}, {0.0f, 4000.0f, -19860.0f}, {-10779.0f, 4000.0f, -31260.0f}},
    {{0.0f, 4000.0f, -4530.0f}, {0.0f, 4000.0f, -19860.0f}, {10779.0f, 0.0f, -50252.0f}},
    {{0.0f, 4000.0f, -4530.0f}, {-10779.0f, 4000.0f, -28060.0f}, {10779.0f, 4000.0f, -28060.0f}},
    {{0.0f, 4000.0f, -4530.0f}, {0.0f, 4000.0f, -19860.0f}, {-10779.0f, 0.0f, -50252.0f}},
    {{10779.0f, 4000.0f, -31260.0f}, {0.0f, 4000.0f, -19860.0f}, {-7309.0f, 0.0f, -50252.0f}},
    {{-10779.0f, 4000.0f, -31260.0f}, {10779.0f, 4000.0f, -31260.0f}, {0.0f, 4000.0f, -19860.0f}},
    {{-10779.0f, 4000.0f, -31260.0f}, {0.0f, 4000.0f, -19860.0f}, {7300.0f, 0.0f, -50252.0f}},
};
static int r208_footTime0 = 750;
static int r208_footTime1 = 1050;
static int r208_footTime2 = 1350;
static Vec r208_goPos0 = {10278.0f, 4000.0f, -10008.0f};
static Vec r208_goPos1 = {-10278.0f, 4000.0f, -10088.0f};
static Vec r208_goPos2 = {1023.0f, 4000.0f, -7747.0f};
static Vec r208_goPos3 = {-1023.0f, 4000.0f, -7747.0f};
static Vec r208_goPos4 = {10278.0f, 4000.0f, -27181.0f};
static Vec r208_goPos5 = {-10278.0f, 4000.0f, -27181.0f};
static Vec r208_goPos6 = {10278.0f, 0.0f, -44181.0f};
static Vec r208_goPos7 = {-10278.0f, 0.0f, -44181.0f};
static Vec r208_goPos8 = {10278.0f, 4000.0f, -29000.0f};
static Vec r208_goPos9 = {-10278.0f, 4000.0f, -29000.0f};
// Enemy list entries fed in below the footings (-1: none).
static int r208_underEmTbl[16] = {
    0x42, 0x41, 0x42, 0x40, 0x42, 0x41, 0x42, 0x40, 0x40, 0x40, 0x40, 0x40, 0x42, 0x40, 0x42, 0x40,
};

// The number of enemy resets is kept in room flags 15..19.
static void setResetNum(int n)
{
    if (n & 1) {
        RsfSet(G_ROOM_ID, 15);
    } else {
        RsfClear(G_ROOM_ID, 15);
    }
    if (n & 2) {
        RsfSet(G_ROOM_ID, 16);
    } else {
        RsfClear(G_ROOM_ID, 16);
    }
    if (n & 4) {
        RsfSet(G_ROOM_ID, 17);
    } else {
        RsfClear(G_ROOM_ID, 17);
    }
    if (n & 8) {
        RsfSet(G_ROOM_ID, 18);
    } else {
        RsfClear(G_ROOM_ID, 18);
    }
    if (n & 0x10) {
        RsfSet(G_ROOM_ID, 19);
    } else {
        RsfClear(G_ROOM_ID, 19);
    }
}

// The enemy reset counter (0..31) read back from room save flags 15..19.
static u32 getResetNum()
{
    int n = 0;

    if (RsfCheck(G_ROOM_ID, 15)) {
        n += 1;
    }
    if (RsfCheck(G_ROOM_ID, 16)) {
        n += 2;
    }
    if (RsfCheck(G_ROOM_ID, 17)) {
        n += 4;
    }
    if (RsfCheck(G_ROOM_ID, 18)) {
        n += 8;
    }
    if (RsfCheck(G_ROOM_ID, 19)) {
        n += 0x10;
    }
    return n;
}

// Advance the reset counter (saturates at 31).
static void incResetNum()
{
    if (getResetNum() != 0x1F) {
        setResetNum(getResetNum() + 1);
    }
}

// Room init (the courtyard with the water mill): hit effects, the initial Ganados (list 2; extras on
// Game_level > 6), the group reset areas (6/7 -> A, 8/9 -> B, 0x15 -> D), Ashley's pointing on area
// 0x10 once (Room_flg bit 14); JumpPoint presets. The bridge collision; the wall crank raised (bit 5:
// objects 0x4E/0x4F shown, area 2 = the crank until the bridge is down, bit 6) else hidden with the
// first Ganados walking in; the carry-over areas 0xE/0x16 until bit 9; footings A/B per bits 10/11;
// the render target; continue point (bit 12) after a save-jump.
void R208Init()
{
#line 157 "D:/Bio4/Prog/r208.cpp"
    r208_work = (R208Work*) MEM_CALLOC(sizeof(R208Work), 1, 0xd);
    W->gotoWait = 900;
    EatMgr.registEffInfo(EAT_ET_ROOM0, (AtEffInfo*) &r208_eff_info4);
    EatMgr.registEffInfo(EAT_ET_ROOM1, (AtEffInfo*) &r208_eff_info5);
    W->em[0].setEm(2, 2, 0, 1, 1);
    W->em[1].setEm(3, 2, 0, 1, 1);
    W->em[2].setEm(4, 2, 0, 1, 1);
    W->em[3].setEm(5, 2, 0, 1, 1);
    W->em[4].setEm(6, 2, 0, 1, 1);
    W->em[5].setEm(7, 2, 0, 1, 1);
    W->em[6].setEm(8, 2, 0, 1, 1);
    W->em[7].setEm(1, 2, 0, 1, 1);
    W->em[8].setEm(0x11, 2, 0, 1, 1);
    if (pG->Game_level > 6) {
        W->em[9].setEm(0x3C, 2, 0, 1, 1);
        W->em[10].setEm(0x3D, 2, 0, 1, 1);
    }
    SceAtDataSet_exec(6, SCE_LEVEL10, 0, (TaskFunc) atari_exec_A, 0, 1);
    SceAtDataSet_exec(7, SCE_LEVEL10, 0, (TaskFunc) atari_exec_A, 0, 1);
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) atari_exec_B, 0, 1);
    SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) atari_exec_B, 0, 1);
    SceAtDataSet_exec(0x15, SCE_LEVEL10, 0, (TaskFunc) atari_exec_D, 0, 1);
    if (RsfCheck(G_ROOM_ID, 14) == 0) {
        SceAtDataSet_exec(0x10, SCE_LEVEL10, 0, (TaskFunc) asl_yubisasi, 0, 1);
    }
    if (pG->JumpPoint == 1) {
        RsfSet(G_ROOM_ID, 5);
        RsfSet(G_ROOM_ID, 6);
        pG->Room_flg[0] |= 0x10000000;
        W->em[0].destroy();
        W->em[1].destroy();
        W->em[2].destroy();
        W->em[3].destroy();
        W->em[4].destroy();
        W->em[5].destroy();
        W->em[6].destroy();
        W->em[7].destroy();
        W->em[8].destroy();
        W->em[9].destroy();
        W->em[10].destroy();
        RsfSet(G_ROOM_ID, 1);
        RsfSet(G_ROOM_ID, 4);
    }
    if (pG->room_id_prev == 0xFFF && StaFlagChk(pG, STA_SUB_ASHLEY) == 0) {
        StaFlagOn(pG, STA_SUB_ASHLEY);
        SubCharInit(1, &pPL->pos, pPL->ang.y);
        SubCharCtrl(SCC_CHASE, 0);
    }
    setTexRender();
    W->sat = EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_satPos, &r208_zeroVec, 3);
    if (RsfCheck(pG->room_id, 5)) {   // struct view: the pG load stays below the sat store
        SmdSetTrans(0x4E, 1);
        SmdSetTrans(0x4F, 1);
        SceAtSetEnable(0xD, 1);
        if (RsfCheck(G_ROOM_ID, 6) == 0) {
            SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r208_checkCrank, 0, 1);
        } else {
            SmdGetObjPtr(0x21)->be_flag |= 0x20;
            SmdGetObjPtr(0x21)->ang.x = 2.0943952f;
            W->sat->setCoord(&r208_satPos, &SmdGetObjPtr(0x21)->ang);
            SceAtSetEnable(3, 0);
        }
    } else {
        SmdSetTrans(0x4E, 0);
        SmdSetTrans(0x4F, 0);
        SceAtSetEnable(0xD, 0);
        SceExec(0x12, (TaskFunc) setEmGo, 0, 0, SCE_PRIO_DEF_2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 9) == 0) {
        SceAtDataSet_exec(0xE, SCE_LEVEL10, 0, (TaskFunc) r208_snipe, 0, 1);
        SceAtDataSet_exec(0x16, SCE_LEVEL10, 0, (TaskFunc) r208_snipe, 0, 1);
        pG->Room_flg[0] |= 0x10000000;
    } else {
        SceAtSetEnable(0x1D, 0);
        SceAtSetEnable(0x1E, 0);
    }
    SmdGetObjPtr(0x57)->be_flag |= 0x20;
    SmdGetObjPtr(0x58)->be_flag |= 0x20;
    if (RsfCheck(pG->room_id, 10) == 0) {   // struct view: the pG load stays below the be_flag store
        SmdGetObjPtr(0x57)->pos.y = 2900.0f;
    } else {
        SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 1);
        EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 5);
    }
    if (RsfCheck(G_ROOM_ID, 11) == 0) {
        SmdGetObjPtr(0x58)->pos.y = 2900.0f;
    } else {
        SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 2);
        EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 4);
    }
    {
        cEm* b0;
        cEm* b1;

        if (getRoomEtcBarred(0xE, &b0, 1) == 1 && getRoomEtcBarred(0xF, &b1, 1) == 1) {
            ((cEmBarred*) b0)->setClosed();
            ((cEmBarred*) b1)->setClosed();
        }
    }
    if (RsfCheck(G_ROOM_ID, 12) && (SysFlagChk(pG, SYS_CONTINUE))) {
        W->em[22].setEm(0xAA, 2, 0, 1, 1);
        W->em[23].setEm(0xAB, 2, 0, 1, 1);
        W->em[24].setEm(0xAC, 2, 0, 1, 1);
        W->em[25].setEm(0xAD, 2, 0, 1, 1);
        W->em[26].setEm(0xAE, 2, 0, 1, 1);
    }
    if (RsfCheck(G_ROOM_ID, 12)) {
        SceExec(0x12, (TaskFunc) em_all_destroy_task, 0, 0, SCE_PRIO_DEF_2, 0);
    }
}

// One frame in: remove every Ganado (0x10..0x20) — the return visit's clean-up.
static void em_all_destroy_task()
{
    SceSleep(1);
    SceDestroyEm(0x10, 0x20);
}

// Per frame: the stream check; the wall crank rises (once, bit 5) when both crank flags (Room_flg[0]
// 0x02000000 / 0x01000000) are set or debug trigger 0; the two barred gates open on their Room_flg[2]
// bits; while the crank is up and the bridge not down, enemies are fed over the wall every 320 / 590
// frames from the list (R208_EmSetEvent) alternating sides while the reset count allows; with more than
// five alive, the farthest Ganado is periodically sent to a new courtyard point (r208_gotoPos).
void R208Main()
{
    u32 alive;
    u32 i;

    r208_StrCheck();
    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        // two flag tests: separate ifs keep fold from merging them
        if (pG->Room_flg[0] & 0x02000000) {
            if (pG->Room_flg[0] & 0x01000000) {
                goto crank;
            }
        }
        if (DebugTrg(0) != 0) {
        crank:
            RsfSet(G_ROOM_ID, 5);
            SceExec(0x12, (TaskFunc) crank_set, 0, 0, SCE_PRIO_DEF_2, 0);
        }
    }
    if ((pG->Room_flg[0] & 0x00800000) == 0) {
        cEm* b0;
        cEm* b1;

        if (getRoomEtcBarred(0xE, &b0, 1) == 1 && getRoomEtcBarred(0xF, &b1, 1) == 1) {
            if (pG->Room_flg[2] & 0x08000000) {
                if (((cEmBarred*) b0)->ckStatus() == 2) {
                    ((cEmBarred*) b0)->setOpen(0);
                }
            } else {
                if (((cEmBarred*) b0)->ckStatus() == 1) {
                    ((cEmBarred*) b0)->setClose(0);
                }
            }
            if (pG->Room_flg[2] & 0x04000000) {
                if (((cEmBarred*) b1)->ckStatus() == 2) {
                    ((cEmBarred*) b1)->setOpen(0);
                }
            } else {
                if (((cEmBarred*) b1)->ckStatus() == 1) {
                    ((cEmBarred*) b1)->setClose(0);
                }
            }
        }
    }
    alive = SceCountEmAlive(0x10, 0x20);
    if (RsfCheck(G_ROOM_ID, 4) && RsfCheck(G_ROOM_ID, 6) == 0) {
        if (W->emSetWait == 0) {
            if ((RsfCheck(G_ROOM_ID, 5) == 0 && getResetNum() <= 1) || (RsfCheck(G_ROOM_ID, 5) && getResetNum() <= 5)) {
                if (RsfCheck(G_ROOM_ID, 5)) {
                    W->emSetWait = 590;
                } else {
                    W->emSetWait = 320;
                }
                if (alive <= 5) {
                    int no;

                    if (pPL->pos.z < -30000.0f) {
                        if (getResetNum() == 0) {
                            no = 0x51;
                        } else if (getResetNum() == 3) {
                            no = 0x50;
                        } else if (getResetNum() == 5) {
                            no = 0x51;
                        } else if (W->side19C == 0) {
                            W->side19C = 1;
                            no = 0x41;
                        } else {
                            W->side19C = 0;
                            no = 0x40;
                        }
                    } else {
                        if (getResetNum() == 0) {
                            no = 0x50;
                        } else if (getResetNum() == 5) {
                            no = 0x50;
                        } else if (W->side1A0 == 0) {
                            W->side1A0 = 1;
                            no = 0x28;
                        } else {
                            W->side1A0 = 0;
                            no = 0x14;
                        }
                    }
                    R208_EmSetEvent(&pG->Em_list[no]);
                    pG->Em_list[no].be_flag |= 2;
                    incResetNum();
                }
            }
        } else {
            W->emSetWait--;
        }
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0 && alive > 5) {
        if (W->gotoWait == 0) {
            cEm* em = getMostFarEm(10000.0f);
            int idx = 0;

            if (em) {
                if (pPL->pos.z > -38000.0f) {
                    if (pPL->pos.z > -12000.0f) {
                        if (pPL->pos.x > 5000.0f) {
                            idx = 0;
                        } else {
                            idx = (pPL->pos.x < -5000.0f) ? 2 : 1;
                        }
                    } else {
                        if (pPL->pos.x > 5000.0f) {
                            idx = 3;
                        } else {
                            idx = (pPL->pos.x < -5000.0f) ? 5 : 4;
                        }
                    }
                } else {
                    if (pPL->pos.x > 5000.0f) {
                        idx = 6;
                    } else {
                        idx = (pPL->pos.x < -5000.0f) ? 8 : 7;
                    }
                }
                ((cEmGanado*) em)->setGoto(&r208_gotoPos[idx][W->gotoIdx], 0xC);
                W->gotoIdx++;
                if ((u32) W->gotoIdx > 2) {
                    W->gotoIdx = 0;
                }
                W->gotoWait = r208_footTime0;
            }
        } else {
            W->gotoWait--;
        }
    }
    for (i = 0; i < 32; i++) {
        if (W->em[i].isAlive() == 1 && W->em[i].ckResetEnable() != 0) {
            W->em[i].destroy();
        }
    }
    if (RsfCheck(G_ROOM_ID, 13) == 0 && pSUB != NULL && pSUB->pos.z < -58000.0f && pSUB->pos.x > 5000.0f) {
        SceAtSetEnable(0xE, 1);
    } else {
        SceAtSetEnable(0xE, 0);
    }
    if (RsfCheck(G_ROOM_ID, 13) == 0 && pSUB != NULL && pSUB->pos.z < -58000.0f && pSUB->pos.x < -5000.0f) {
        SceAtSetEnable(0x16, 1);
    } else {
        SceAtSetEnable(0x16, 0);
    }
    if ((pG->Room_flg[0] & 0x04000000) == 0 && (SubCharGetStatus() & 0x01000000) == 0) {
        SmdGetObjPtr(0x4E)->Motion.Mot_attr |= 8;
        SmdGetObjPtr(0x50)->Motion.Mot_attr |= 8;
        SmdGetObjPtr(0x52)->Motion.Mot_attr |= 8;
        if (pSUB != NULL) {
            pSUB->atari.setPriority(0);
        }
    }
    if ((pG->Room_flg[2] & 0x80000000) || RsfCheck(G_ROOM_ID, 5)) {
        W->doorA += 15.0f;
    }
    if ((pG->Room_flg[2] & 0x80000000) == 0 && RsfCheck(G_ROOM_ID, 5) == 0) {
        W->doorA -= 15.0f;
    }
    if ((pG->Room_flg[2] & 0x40000000) || RsfCheck(G_ROOM_ID, 5)) {
        W->doorB += 15.0f;
    }
    if ((pG->Room_flg[2] & 0x40000000) == 0 && RsfCheck(G_ROOM_ID, 5) == 0) {
        W->doorB -= 15.0f;
    }
    if (W->doorA < 0.0f) {
        W->doorA = 0.0f;
    }
    if (W->doorB < 0.0f) {
        W->doorB = 0.0f;
    }
    if (W->doorA > 150.0f) {
        if ((pG->Room_flg[0] & 0x02000000) == 0) {
            SndCall(6, 8, &SmdGetObjPtr(0x5A)->pos, 0, 0, 0);
        }
        pG->Room_flg[0] |= 0x02000000;
        W->doorA = 150.0f;
    } else {
        if (pG->Room_flg[0] & 0x02000000) {
            SndCall(6, 9, &SmdGetObjPtr(0x5A)->pos, 0, 0, 0);
        }
        pG->Room_flg[0] &= ~0x02000000;
    }
    if (W->doorB > 150.0f) {
        if ((pG->Room_flg[0] & 0x01000000) == 0) {
            SndCall(6, 8, &SmdGetObjPtr(0x59)->pos, 0, 0, 0);
        }
        pG->Room_flg[0] |= 0x01000000;
        W->doorB = 150.0f;
    } else {
        if (pG->Room_flg[0] & 0x01000000) {
            SndCall(6, 9, &SmdGetObjPtr(0x59)->pos, 0, 0, 0);
        }
        pG->Room_flg[0] &= ~0x01000000;
    }
    SmdGetObjPtr(0x59)->be_flag |= 0x20;
    SmdGetObjPtr(0x5A)->be_flag |= 0x20;
    SmdGetObjPtr(0x5A)->pos.y = -W->doorA;
    SmdGetObjPtr(0x59)->pos.y = -W->doorB;
    {
        Vec pos;
        cSat* sat;

        sat = SceAtPtr(0x1B)->scr.pSat;
        pos.x = -5700.79f;
        pos.y = 124.796f;
        pos.z = -67214.7f;
        pos.y -= W->doorB;
        sat->setCoord(&pos, (Vec*) &vecZero);
        sat = SceAtPtr(0x1C)->scr.pSat;
        pos.x = 3425.79f;
        pos.y = 124.796f;
        pos.z = -67283.7f;
        pos.y -= W->doorA;
        sat->setCoord(&pos, (Vec*) &vecZero);
    }
    if ((pG->Room_flg[0] & 0x00200000) && (SubCharGetStatus() & 0x00800000)) {
        SetSubAux(funcAshley2, 0);
        pG->Room_flg[0] &= ~0x00200000;
    }
    if ((u32) W->crankSeCnt > 0x3B) {
        W->crankSeCnt = 0;
        SndCall(6, 2, &SmdGetObjPtr(0x21)->pos, 0, 0, 0);
    }
}

// The alive Ganado farthest from the player within `range` (NULL when there are less than two).
extern "C" cEm* getMostFarEm(f32 range)
{
    f32 best = 0.0f;
    cEm* far = NULL;
    int lo = 0x10;   // the id range is kept in variables: signed compare, no `<= C-1` fold
    int hi = 0x20;
    u32 cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        Vec d;
        f32 len;

        if (EM10_WK(em)->Wep_type == 8) {
            continue;
        }
        if (hi == -1) {
            hi = 0x10;
        }
        if (em->id < lo || em->id > hi) {
            continue;
        }
        if (SceCheckEmAlive(em) != 1) {
            continue;
        }
        PSVECSubtract(&pPL->pos, &em->pos, &d);
        len = PSVECMag(&d);
        if (len < range && len > best) {
            best = len;
            far = em;
            cnt++;
        }
    }
    if (cnt <= 1) {
        far = NULL;
    }
    return far;
}

// Ashley turns the crank of the footing (SetSubAux routine).
static void funcAshley(cEm* p)
{
    u32 limit;
    int mot;

    W->crank = NULL;   // the pSUB load stays below the store
    if (pSUB == NULL) {
        return;
    }
    pSUB->atari.setPriority(PRI_LV3);
    if (pG->Room_flg[0] & 0x08000000) {
        W->crank = SmdGetObjPtr(0x4E);
    } else if (pG->Room_flg[0] & 0x80000000) {
        W->crank = SmdGetObjPtr(0x50);
    } else {
        W->crank = SmdGetObjPtr(0x52);
    }
    W->crank->be_flag |= 0x20;
    switch (p->r_no_2) {
    case 0:
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x36), 3, 0, 5, ROOM_ARC_PTR(pG->pRoom, 0x37));
        W->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, 0, 5, ROOM_ARC_PTR(pG->pRoom, 0x2B));
        W->crank->setNoSuspend(0);
        {
            Vec pos = {427.81f, 0.0f, 563.42f};
            Vec* rot = &p->ang;   // taken before the calls: kept in a register (`mr r4` at setAng)

            PSMTXMultVec(W->crank->mat, &pos, &pos);
            pos.y = p->pos.y;
            p->ang.y = W->crank->ang.y - 1.5707964f;
            p->setPos(&pos);
            p->setAng(rot);
        }
        p->r_no_2 = 1;
    case 1:
        p->motionMove();
        if (MotionCheckCrossFrame(&p->Motion, 0.0f) == 1 || MotionCheckCrossFrame(&p->Motion, 50.0f) == 1
            || MotionCheckCrossFrame(&p->Motion, 100.0f) == 1) {
            SndCall(6, 0x35, &p->pos, 0, 0, 0);
        }
        // two tests: separate ifs keep fold from merging them
        if (pG->Room_flg[0] & 0x20000000) {
            limit = r208_footTime2;
        } else if (pG->Room_flg[0] & 0x40000000) {
            limit = r208_footTime2;
        } else {
            limit = r208_footTime1;
        }
        mot = 0;
        if (pG->Room_flg[0] & 0x08000000) {
            W->crankSeCnt++;
            SmdGetObjPtr(0x21)->be_flag |= 0x20;
            if (SmdGetObjPtr(0x21)->ang.x < 1.0f) {
                SmdGetObjPtr(0x21)->ang.x += (f32) (mot + 2) * 0.0007f;
            } else {
                SceExec(0x12, (TaskFunc) brige1_down, 0, 0, SCE_PRIO_DEF_2, 0);
                pG->Room_flg[0] &= ~0x08000000;
                p->setRno(mot, mot, mot, mot);
                SubCharCtrl(SCC_CHASE, 0);
            }
        } else if (pG->Room_flg[0] & 0x80000000) {
            if (DebugTrg(0) != 0) {
                W->footACnt = limit;
            }
            W->footACnt++;
            if ((u32) W->footACnt > limit) {
                pG->Room_flg[0] |= 0x40000000;
                SceExec(0x12, (TaskFunc) footingA_up, 0, 0, SCE_PRIO_DEF_2, 0);
                pG->Room_flg[0] &= ~0x80000000;
                p->setRno(0, 0, 0, 0);
                SubCharCtrl(SCC_CHASE, 0);
                if (pG->Room_flg[0] & 0x20000000) {
                    Vec pos;

                    RsfSet(G_ROOM_ID, 9);
                    pos.x = 10603.0f;
                    pos.y = 10000.0f;
                    pos.z = -60554.0f;
                    SubCharMoveTo(pos.x, pos.y, pos.z, 193.0f, 0);
                    pG->Room_flg[0] |= 0x00200000;
                }
            }
        } else {
            if (DebugTrg(0) != 0) {
                W->footBCnt = limit;
            }
            W->footBCnt++;
            if ((u32) W->footBCnt > limit) {
                pG->Room_flg[0] |= 0x20000000;
                SceExec(0x12, (TaskFunc) footingB_up, 0, 0, SCE_PRIO_DEF_2, 0);
                pG->Room_flg[0] |= 0x80000000;
                p->setRno(0, 0, 0, 0);
                SubCharCtrl(SCC_CHASE, 0);
                if (pG->Room_flg[0] & 0x40000000) {
                    Vec pos;

                    RsfSet(G_ROOM_ID, 9);
                    pos.x = -10603.0f;
                    pos.y = 10000.0f;
                    pos.z = -60554.0f;
                    SubCharMoveTo(pos.x, pos.y, pos.z, 193.0f, 0);
                    pG->Room_flg[0] |= 0x00200000;
                }
            }
        }
        break;
    }
}

// Ashley waves to the player (SetSubAux routine).
static void funcAshley2(cEm* p)
{
    if (p->r_no_2 == 0) {
        AtariFlagsAndV(&pSUB->atari, 0xFCFF);
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x43), 0x19, 0, 1, 0);
        p->r_no_2 = 1;
        W->ashleyCnt = 0;
    }
    p->ang.y += Muku(&p->pos, &pPL->pos, p->ang.y, 0.09817477f);
    W->ashleyCnt++;
    if (W->ashleyCnt == 0xF) {
        SndCall(8, 1, &p->pos, p->id, 0, 0);
    }
    if (p->motionMove() != 0) {
        p->setRno(0, 0, 0, 0);
        AtariFlagsOr(&pSUB->atari, 0x300);
        SubCharCtrl(SCC_CHASE, 0);
    }
}

// Ashley points at the wall (SetSubAux routine).
static void funcAshley3(cEm* p)
{
    Vec target = {15468.0f, 10000.0f, -52881.0f};

    if (p->r_no_2 == 0) {
        SubCharSetHand(3);
        AtariFlagsAndV(&pSUB->atari, 0xFCFF);
        p->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x44), 0, 0, 1, 0);
        p->r_no_2 = 1;
        W->ashleyCnt = 0;
    }
    p->ang.y += Muku(&p->pos, &target, p->ang.y, 0.09817477f);
    W->ashleyCnt++;
    if (p->motionMove() != 0) {
        p->setRno(0, 0, 0, 0);
        AtariFlagsOr(&pSUB->atari, 0x300);
        SubCharSetHand(0);
        SubCharCtrl(SCC_CHASE, 0);
    }
}

// Ashley points out the wall crank: the two message cuts, then the save prompt.
static void asl_yubisasi()
{
    cPlayer* pl = pPL;

    while (pl->checkEvent() == 0 || CheckDoorJumpWithAshley() == 0) {
        SceSleep(1);
    }
    if (RsfCheck(G_ROOM_ID, 13)) {
        return;
    }
    RsfSet(G_ROOM_ID, 14);
    SceEventStart(1);
    SpfFlagOn(pG, SPF_KEY);
    pPL->setNoSuspend(1);
    pSUB->setNoSuspend(1);
    {
        Vec pos;

        pos.x = 0.0f;
        pos.y = 7000.0f;
        pos.z = -52998.0f;
        pPL->setPos(&pos);
        pos.x = 0.0f;
        pos.y = 1.58f;
        pos.z = 0.0f;
        pPL->setAng(&pos);
        pos.x = 448.0f;
        pos.y = 7000.0f;
        pos.z = -51764.0f;
        pSUB->setPos(&pos);
        pos.x = 0.0f;
        pos.y = 1.44f;
        pos.z = 0.0f;
        pSUB->setAng(&pos);
    }
    pPL->be_flag |= 0x00200000;   // the pSUB load stays below the store
    if (pSUB != NULL) {
        pSUB->be_flag |= 0x00200000;
    }
    SetSubAux(funcAshley3, 0);
    CamCtrl.CutCall(0xF);
    SceSleep(0xF);
    SndCall(6, 4, 0, 0, 0, 0);
    SceMesSet(2, 0xA0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(0x10);
    SceSleep(0xF);
    SceMesSet(3, 0xA0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    while (SubCharGetStatus() & 0x01000000) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SpfFlagOff(pG, SPF_KEY);
    SceEventEnd(0);
    pPL->setNoSuspend(0);
    pSUB->setNoSuspend(0);
    r208_continue();
}

// The water surface: a render target blended into the water object.
extern "C" void setTexRender()
{
    cObj* obj;
    u8* tbl = r208_texTbl;

    if (GetTexRenderMgr(&W->tex)) {
        tbl[0] = 1;
        tbl[1] = 0;
        tbl[4] = 0xF7;
        tbl[5] = W->tex->GetTexNo();
        W->tex->SetRepeatType(1);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, W->tex->GetCoreFlg() | 1, ESP_CORE_KIND_NONE, 0, 0);
    } else {
        pLog->err(0, 0, "SetTexRender() : Manager alloc failed!!");
    }
    obj = SmdGetObjPtr(0x14);
    obj->pModelInfo->setTexBlendTbl(tbl);
    obj->pModelInfo->setBlendRatio(0xFF);
    obj->pModelInfo->color[3] = 0xF0;
    obj->Shader_type = 2;
    obj->Refract_pow = 0x20;
    obj->Refract_ratio = 0x80;
}

// The first Ganado come at the player, then walk to the courtyard points.
static void setEmGo()
{
    int go7;
    int go8;
    int go2;
    int go0;

    SceSleep(0x1E);
    W->em[3].setFindPL();
    W->em[4].setFindPL();
    SceSleep(0x1E);
    W->em[1].setGoto(&pPL->pos, 8);
    SceSleep(0x2D);
    W->em[7].setFindPL();
    W->em[8].setFindPL();
    W->em[2].setFindPL();
    W->em[0].setFindPL();
    W->em[7].setGoto(&r208_goPos0, 0xC);
    W->em[8].setGoto(&r208_goPos1, 0xC);
    W->em[2].setGoto(&r208_goPos4, 0xC);
    W->em[0].setGoto(&r208_goPos5, 0xC);
    // Initialised between the last call and the loop (the `li`s are hoisted anyway): a call directly
    // followed by the loop label gets flow's `(use (const_int 0))` nop, which costs a sched1 issue slot.
    go7 = 0;
    go8 = 0;
    go2 = 0;
    go0 = 0;
    while (1) {
        if (W->em[7].isAlive() && W->em[7].ckGoto() == 0) {
            if (go7 == 0) {
                go7 = 1;
                W->em[7].setGoto(&r208_goPos2, 0xC);
            } else {
                W->em[7].setFindPL();
            }
        }
        if (W->em[8].isAlive() && W->em[8].ckGoto() == 0) {
            if (go8 == 0) {
                go8 = 1;
                W->em[8].setGoto(&r208_goPos3, 0xC);
            } else {
                W->em[8].setFindPL();
            }
        }
        if (W->em[2].isAlive() && W->em[2].ckGoto() == 0) {
            if (go2 == 0) {
                go2 = 1;
                W->em[2].setGoto(&r208_goPos6, 0xC);
            } else {
                W->em[2].setFindPL();
            }
        }
        if (W->em[0].isAlive() && W->em[0].ckGoto() == 0) {
            if (go0 == 0) {
                go0 = 1;
                W->em[0].setGoto(&r208_goPos7, 0xC);
            } else {
                W->em[0].setFindPL();
            }
        }
        if (W->em[7].isActive() == 0 && W->em[8].isActive() == 0 && W->em[2].isActive() == 0 && W->em[0].isActive() == 0) {
            break;
        }
        SceSleep(1);
    }
    SceSleep(1);
}

// Areas 6/7: group B1 when three or fewer Ganados are alive and Room_flg[2] 0x10000000.
static void atari_exec_A()
{
    if ((u32) SceCountEmAlive(0x10, 0x20) <= 3 && (pG->Room_flg[2] & 0x10000000)) {
        emGroupeB1_reset();
    }
}

// Areas 8/9: group B2 when five or fewer are alive and Room_flg[2] 0x20000000.
static void atari_exec_B()
{
    if ((u32) SceCountEmAlive(0x10, 0x20) <= 5 && (pG->Room_flg[2] & 0x20000000)) {
        emGroupeB2_reset();
    }
}

// Area 0x15: group D (behind the gates) when five or fewer are alive.
static void atari_exec_D()
{
    if ((u32) SceCountEmAlive(0x10, 0x20) <= 5) {
        emGroupeD_reset();
    }
}

// Group A: the two list events and up to five more Ganado depending on how many are alive.
extern "C" void emGroupeA_reset()
{
    u32 alive;

    if (RsfCheck(G_ROOM_ID, 1)) {
        return;
    }
    RsfSet(G_ROOM_ID, 1);
    R208_EmSetEvent(&pG->Em_list[0x20]);
    R208_EmSetEvent(&pG->Em_list[0x22]);
    alive = SceCountEmAlive(0x10, 0x20);
    if (alive <= 8) {
        W->em[11].setEm(0xD, 2, 0, 1, 1);
    }
    if (alive <= 7) {
        W->em[13].setEm(0xF, 2, 0, 1, 1);
    }
    if (alive <= 6) {
        W->em[12].setEm(0xE, 2, 0, 1, 1);
    }
    if (alive <= 5) {
        W->em[14].setEm(0x10, 2, 0, 1, 1);
    }
    if (alive <= 4) {
        W->em[15].setEm(0x11, 2, 0, 1, 1);
    }
}

// Group B1: only marks Room_flg bit 2 (its spawns were removed from this build).
extern "C" void emGroupeB1_reset()
{
    if (RsfCheck(G_ROOM_ID, 2)) {
        return;
    }
    RsfSet(G_ROOM_ID, 2);
}

// Group B2: only marks Room_flg bit 2 (same as B1).
extern "C" void emGroupeB2_reset()
{
    if (RsfCheck(G_ROOM_ID, 2)) {
        return;
    }
    RsfSet(G_ROOM_ID, 2);
}

// Group C once (Room_flg bit 3): five Ganados 0xAA..0xAE (list 2).
extern "C" void emGroupeC_reset()
{
    if (RsfCheck(G_ROOM_ID, 3)) {
        return;
    }
    RsfSet(G_ROOM_ID, 3);
    W->em[22].setEm(0xAA, 2, 0, 1, 1);
    W->em[23].setEm(0xAB, 2, 0, 1, 1);
    W->em[24].setEm(0xAC, 2, 0, 1, 1);
    W->em[25].setEm(0xAD, 2, 0, 1, 1);
    W->em[26].setEm(0xAE, 2, 0, 1, 1);
}

// Group D: the four Ganado behind the gates, with the camera cut on the gates opening.
extern "C" void emGroupeD_reset()
{
    cEm* b0;
    cEm* b1;
    u32 i;

    if (RsfCheck(G_ROOM_ID, 4)) {
        return;
    }
    RsfSet(G_ROOM_ID, 4);
    W->em[28].setEm(0x23, 2, 0, 1, 1);
    W->em[30].setEm(0x25, 2, 0, 1, 1);
    W->em[29].setEm(0x24, 2, 0, 1, 1);
    W->em[31].setEm(0x26, 2, 0, 1, 1);
    W->em[28].setNoSuspend(1);
    W->em[29].setNoSuspend(1);
    W->em[30].setNoSuspend(1);
    W->em[31].setNoSuspend(1);
    SceEventStart(1);
    pG->Room_flg[0] |= 0x00800000;
    CamCtrl.CutCall(0xA);
    SceSleep(1);
    W->em[0].setGoto(&r208_goPos9, 0xC);
    W->em[2].setGoto(&r208_goPos8, 0xC);
    W->em[1].setGoto(&r208_goPos9, 0xC);
    W->em[3].setGoto(&r208_goPos8, 0xC);
    if (getRoomEtcBarred(0xE, &b0, 1) == 1 && getRoomEtcBarred(0xF, &b1, 1) == 1) {
        ((cEmBarred*) b0)->setOpen(0);
        ((cEmBarred*) b1)->setOpen(0);
    }
    for (i = 0; i < 0x41; i++) {
        if (getRoomEtcBarred(0xE, &b0, 1) == 1 && getRoomEtcBarred(0xF, &b1, 1) == 1) {
            ((cEmBarred*) b0)->setOpen(0);
            ((cEmBarred*) b1)->setOpen(0);
        }
        SceSleep(1);
    }
    CamCtrl.CutCall(0xB);
    for (i = 0; i < 0x2D; i++) {
        if (getRoomEtcBarred(0xE, &b0, 1) == 1 && getRoomEtcBarred(0xF, &b1, 1) == 1) {
            ((cEmBarred*) b0)->setOpen(0);
            ((cEmBarred*) b1)->setOpen(0);
        }
        SceSleep(1);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
        if (getRoomEtcBarred(0xE, &b0, 1) == 1 && getRoomEtcBarred(0xF, &b1, 1) == 1) {
            ((cEmBarred*) b0)->setOpen(0);
            ((cEmBarred*) b1)->setOpen(0);
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    pG->Room_flg[0] &= ~0x00800000;
    W->em[28].setNoSuspend(0);
    W->em[29].setNoSuspend(0);
    W->em[30].setNoSuspend(0);
    W->em[31].setNoSuspend(0);
}

// The bridge comes down (camera event); the player is kept out of the way meanwhile.
static void brige1_down()
{
    Vec plPos;
    Vec pos;
    int hard;
    f32 spd;

    SceEventStart(1);
    CamCtrl.CutCall(6);
    SndCall(6, 1, 0, 0, 0, 0);
    SmdGetObjPtr(0x4E)->Motion.Mot_attr |= 8;
    SmdGetObjPtr(0x4E)->setNoSuspend(1);
    RsfSet(G_ROOM_ID, 6);
    pG->Room_flg[0] |= 0x10000000;
    W->sat->setCoord(&r208_satPos, &SmdGetObjPtr(0x21)->ang);
    plPos = pPL->pos;
    VecSet(&pos, 0.0f, -4000.0f, -500.0f);
    pPL->setPos(&pos);
    hard = pG->Game_level > 6;
    if (hard) {
        emGroupeC_reset();
        W->em[22].setNoSuspend(1);
        W->em[23].setNoSuspend(1);
        W->em[24].setNoSuspend(1);
        W->em[25].setNoSuspend(1);
        W->em[26].setNoSuspend(1);
    }
    SmdGetObjPtr(0x21)->be_flag |= 0x20;
    spd = 0.021f;
    while (SmdGetObjPtr(0x21)->ang.x < 2.0943952f) {
        SceSleep(1);
        spd += 0.0009f;
        if (spd > 0.065f) {
            spd = 0.065f;
        }
        SmdGetObjPtr(0x21)->ang.x += spd;
    }
    EstSet(0, -1, 0, 0, EFF_ROOM, 5, 1, ESP_CORE_KIND_NONE, 0, 0);
    SmdGetObjPtr(0x21)->ang.x = 2.0943952f;
    W->sat->setCoord(&r208_satPos, &SmdGetObjPtr(0x21)->ang);
    SceAtSetEnable(3, 0);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    if (hard) {
        W->em[22].setNoSuspend(0);
        W->em[23].setNoSuspend(0);
        W->em[24].setNoSuspend(0);
        W->em[25].setNoSuspend(0);
        W->em[26].setNoSuspend(0);
    }
    if (pPL->pos.z > -32000.0f) {
        pPL->ang.y = 3.14f;
    }
    pPL->setPos(&plPos);
}

// End of the crank-rise cutscene (also its cancel path): the crank objects 0x4E/0x4F snapped up, SE
// stopped, camera back, SceEventEnd, then group A.
static void crank_set_exit()
{
    SmdGetObjPtr(0x4E)->pos.y = 4984.0f;
    SmdGetObjPtr(0x4F)->pos.y = 4000.0f;
    SndStop(W->crankSe, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    emGroupeA_reset();
}

// The wall crank rises out of the water (camera event).
static void crank_set()
{
    f32 y;

    SceEventStart(1);
    CamCtrl.CutCall(8);
    SmdSetTrans(0x4E, 1);
    SmdSetTrans(0x4F, 1);
    SmdGetObjPtr(0x4E)->be_flag |= 0x20;
    SmdGetObjPtr(0x4F)->be_flag |= 0x20;
    SceAtSetEnable(0xD, 1);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r208_checkCrank, 0, 1);
    SceSetEventCancel(1, (TaskFunc) crank_set_exit, 0, -1, 1);
    W->crankSe = SndCall(6, 0, &SmdGetObjPtr(0x4E)->pos, 0, 0, 0);
    y = 2751.0f;
    while (y < 4000.0f) {
        SmdGetObjPtr(0x4E)->pos.y = y + 984.0f;
        SmdGetObjPtr(0x4F)->pos.y = y;
        y += 20.816668f;
        SceSleep(1);
    }
    SceSleep(0xF);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    crank_set_exit();
}

// The crank prompt: the player turns it, or Ashley is sent down to it.
static void r208_checkCrank()
{
    int sel;

    SceAtSetEnable(2, 0);
    if (CheckDoorJumpWithAshley() == 0) {
        sel = 1;
    } else {
        SceMesSet(0, 0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        sel = SceMesGetSelection();
    }
    if (sel == 1) {
        SceExec(0x12, (TaskFunc) r208_operateCrank, 0, 0, SCE_PRIO_DEF_2, 0);
    } else if (sel == 2) {
        SceExec(0x12, (TaskFunc) SubUnderCrankExec, 0, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceAtSetEnable(2, 1);
    }
}

// The player turns the wall crank: the bridge tilts down while the button is held.
static void r208_operateCrank()
{
    int spd = 0;
    int lastMot = 0;
    int accel = 0;
    int mot;

    pG->Room_flg[0] |= 0x04000000;
    SmdGetObjPtr(0x4E)->be_flag |= 0x20;
    W->crank = SmdGetObjPtr(0x4E);   // the pPL load stays below the store
    pPL->beginEvent(0);
    PlSetHand(1, 0);
    W->crank->beginEvent(0);
    pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 3, 0, 5, ROOM_ARC_PTR(pG->pRoom, 0x20));
    W->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, 0, 5, ROOM_ARC_PTR(pG->pRoom, 0x2B));
    CamCtrl.CutCall(9);
    {
        Vec v = {500.0f, 4000.0f, -29300.0f};
        cPlayer* pl;

        v.y = pPL->pos.y;   // struct view: the pPL load is issued after the W load (target order)
        (pPL->ang.y = W->crank->ang.y - 1.5707964f);
        pl = pPL;
        SetPosAng(pl, &v, &pl->ang);
    }
    while (1) {
        if (MotionCheckCrossFrame(&pPL->Motion, 0.0f) == 1 || MotionCheckCrossFrame(&pPL->Motion, 50.0f) == 1
            || MotionCheckCrossFrame(&pPL->Motion, 100.0f) == 1) {
            SndCall(6, 0x35, &pPL->pos, 0, 0, 0);
        }
        if ((PlGetStatus() & 0x00020000) == 0 || (Key.trg & 0x40000000)) {
            break;
        }
        accel++;
        if (accel > 8) {
            accel = 8;
            spd -= 5;
            if (spd < 0) {
                spd = 0;
            }
        }
        mot = spd / 20;
        if (mot > 7) {
            mot = 7;
        }
        if (mot != lastMot) {
            void* m0;
            void* m1;
            u32 n;
            u32 frame;
            f32 rate;

            lastMot = mot;
            switch (mot) {
            default:
            case 0:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x20);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2B);
                break;
            case 1:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x21);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2C);
                break;
            case 2:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x22);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2D);
                break;
            case 3:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x23);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2E);
                break;
            case 4:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x24);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x2F);
                break;
            case 5:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x25);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x30);
                break;
            case 6:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x26);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x31);
                break;
            case 7:
                m0 = ROOM_ARC_PTR(pG->pRoom, 0x27);
                m1 = ROOM_ARC_PTR(pG->pRoom, 0x32);
                break;
            }
            rate = pPL->Motion.Seq_frame / (f32) pPL->Motion.Seq_frame_num;
            n = *(u16*) m0;
            frame = (u32) ((f32) n * rate);
            frame++;
            if (frame >= n) {
                frame = 0;
            }
            pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x1F), 3, frame, 5, m0);
            W->crank->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x2A), 3, frame, 5, m1);
        }
        SmdGetObjPtr(0x21)->be_flag |= 0x20;
        W->crankSeCnt++;
        // `!(x < y)`: a single `blt` past the exit block (`>=` would need the unordered cror).
        if (!(SmdGetObjPtr(0x21)->ang.x < 1.0f)) {
            SceExec(0x12, (TaskFunc) brige1_down, 0, 0, SCE_PRIO_DEF_2, 0);
            SceSleep(1);
            break;
        }
        SmdGetObjPtr(0x21)->ang.x += (f32) (mot + 1) * 0.00068f;
        W->sat->setCoord(&r208_satPos, &SmdGetObjPtr(0x21)->ang);
        if (Key.trg & 0x00080000) {
            spd += accel;
            accel = 0;
            if (spd > 159) {
                spd = 159;
            }
        }
        ActBtn.set(ACT_ROTATE, 5, 0, 0, ACTCTR_ENFORCE_EXEC, DISP_A_RAPID, ACT_FUNC_NORMAL, 0);
        SceSleep(1);
    }
    W->crank->motionPause();
    PlSetHand(0, 0);
    pPL->endEvent(0);
    W->crank->endEvent(0);
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceAtSetEnable(2, 1);
    }
    CamCtrl.Comeback(0);
    pG->Room_flg[0] &= ~0x04000000;
}

// Sets the handle's list entry when less than twelve enemies are alive.
extern "C" cEm* R208_setEm(s16 no)
{
    u32 alive = SceCountEmAlive(0x10, 0x20);

    if (alive > 0xB) {
        pLog->warn(0, 0, "R208_setEm(): too many EM[%d]", alive);
        return NULL;
    }
    return setEm(no, 2, 1, 1, 1);
}

// EmSetEvent only while enemy list 2 (the courtyard list) is the loaded one.
extern "C" cEm* R208_EmSetEvent(EmListData* d)
{
    if (pG->em_list_no == 2) {
        return EmSetEvent(d);
    }
    return NULL;
}

// Number of the enemies below the footings still active.
extern "C" u32 getUnderEmNum()
{
    u32 n = 0;
    u32 i;

    for (i = 0; i < (u32) W->underCnt; i++) {
        if (W->under[i].isActive()) {
            n++;
        }
    }
    return n;
}

// One more under[] slot in use.
extern "C" void addUnderEmCnt()
{
    W->underCnt++;
}

// Feeds the enemies below the footings in from the list, one per entry, while Ashley is up there.
static void under_set_task()
{
    Vec pos = {0.0f, 7000.0f, -54093.0f};
    u32 i = 0;

    while (1) {
        if (RsfCheck(G_ROOM_ID, 11) && RsfCheck(G_ROOM_ID, 11)) {
            break;
        }
        if (i < (u32) W->underCnt) {
            u32 max = 1;

            if (pG->Game_level > 7) {
                max = 4;
            } else if (pG->Game_level > 5) {
                max = 4;
            } else if (pG->Game_level > 3) {
                max = 3;
            } else if (pG->Game_level > 1) {
                max = 2;
            }
            if (getUnderEmNum() < max) {
                if (pG->Game_level <= 6 && (i == 3 || i == 7)) {
                } else if (pG->Game_level <= 3 && (i == 3 || i == 5 || i == 7)) {
                } else if (r208_underEmTbl[i] != -1) {
                    cEm* em = R208_EmSetEvent(&pG->Em_list[r208_underEmTbl[i]]);

                    if (em) {
                        W->under[i].setPtr(em, 0);
                        if (i == 0) {
                            W->under[0].setGoto(&pos, 0xC);
                        } else {
                            W->under[i].setGoto(&pPL->pos, 0xC);
                        }
                    }
                }
                i++;
                if (pG->Game_level <= 6) {
                    SceSleep(0x2D);
                }
            }
        }
        SceSleep(1);
    }
}

// Ashley is carried over and set down at the wall; the two footing counters spawn the enemies.
static void r208_snipe()
{
    Vec pos;

    SceAtSetEnable(0xE, 0);
    SceAtSetEnable(0x16, 0);
    r208_CarryOnShoulder();
    RsfSet(G_ROOM_ID, 13);
    SceExec(0x12, (TaskFunc) under_set_task, 0, 0, SCE_PRIO_DEF_2, 0);
    R208_setEm(0x16);
    R208_setEm(0x17);
    R208_setEm(0x18);
    R208_setEm(0x19);
    addUnderEmCnt();
    {
    Vec posA = {15838.0f, 10007.0f, -51919.0f};
    Vec posB = {-15838.0f, 10007.0f, -51919.0f};
    Vec d;

    if (pG->Room_flg[0] & 0x80000000) {
        pos = posA;
    } else {
        pos = posB;
    }
    SubCharMoveTo(pos.x, pos.y, pos.z, 193.0f, 0);
    SceSleep(1);
    while (1) {
        if (pG->Room_flg[0] & 0x80000000) {
            pos = posA;
        } else {
            pos = posB;
        }
        PSVECSubtract(&pSUB->pos, &pos, &d);
        if (PSVECMag(&d) < 500.0f) {
            if (SubCharGetStatus() & 1) {
                SetSubAux(funcAshley, 0);
            }
        } else {
            if (SubCharGetStatus() & 1) {
                if (pG->Room_flg[0] & 0x80000000) {
                    pos = posA;
                } else {
                    pos = posB;
                }
                SubCharMoveTo(pos.x, pos.y, pos.z, 193.0f, 0);
            }
        }
        // two tests: separate ifs keep fold from merging them
        if (pG->Room_flg[0] & 0x20000000) {
            if (pG->Room_flg[0] & 0x40000000) {
                SceAtSetEnable(0x1D, 0);
                SceAtSetEnable(0x1E, 0);
                return;
            }
        }
        if (W->footACnt == 1) {
            cEm* em = R208_EmSetEvent(&pG->Em_list[0x19]);

            if ((pG->Room_flg[0] & 0x20000000) && em != NULL) {
                ((cEmGanado*) em)->setGoto(&pSUB->pos, 0xC);
            }
            addUnderEmCnt();
            if (pG->Game_level > 6) {
                addUnderEmCnt();
            }
        }
        if (W->footACnt == 300) {
            W->footACnt = 301;
            addUnderEmCnt();
        }
        if (W->footACnt == 600) {
            W->footACnt = 601;
            R208_EmSetEvent(EM_LIST_S(0xB0));
            addUnderEmCnt();
        }
        if (W->footACnt == 750) {
            W->footACnt = 751;
            if (pG->Game_level > 6) {
                addUnderEmCnt();
            }
        }
        if (W->footACnt == 1020) {
            W->footACnt = 1021;
            addUnderEmCnt();
        }
        if (W->footBCnt == 1) {
            cEm* em;

            W->footBCnt = 2;
            em = R208_EmSetEvent(EM_LIST_S(0xB));
            if ((pG->Room_flg[0] & 0x40000000) && em != NULL) {
                ((cEmGanado*) em)->setGoto(&pSUB->pos, 0xC);
            }
            if (pG->Game_level > 6) {
                addUnderEmCnt();
            }
            addUnderEmCnt();
        }
        if (W->footBCnt == 300) {
            W->footBCnt = 301;
            addUnderEmCnt();
        }
        if (W->footBCnt == 600) {
            W->footBCnt = 601;
            R208_EmSetEvent(EM_LIST_S(0x1A));
            addUnderEmCnt();
        }
        if (W->footBCnt == 750) {
            W->footBCnt = 751;
            if (pG->Game_level > 6) {
                addUnderEmCnt();
            }
        }
        if (W->footBCnt == 1020) {
            W->footBCnt = 1021;
            addUnderEmCnt();
        }
        SceSleep(1);
    }    }
}

// The player lifts Ashley onto the wall (camera event, side by the player's x).
extern "C" void r208_CarryOnShoulder()
{
    Vec a;
    Vec b;
    f32 angY;

    if (pSUB == NULL) {
        return;
    }
    SndStrReq(1, 0x31, 0x80000003, 0, 0, 0.0f);
    SceEventStart(0);
    SubCharCtrl(SCC_AUX_MOT, 0);
    pPL->setNoSuspend(1);
    pSUB->setNoSuspend(1);
    if (pPL->pos.x > 0.0f) {
        a.x = 8842.0f;
        a.y = 7000.0f;
        a.z = -60554.0f;
        angY = 1.5707964f;
        b.x = 10148.0f;
        b.y = 10000.0f;
        b.z = -61054.0f;
        pG->Room_flg[0] |= 0x80000000;
    } else {
        a.x = -8842.0f;
        a.y = 7000.0f;
        a.z = -60554.0f;
        angY = -1.5707964f;
        b.x = -10148.0f;
        b.y = 10000.0f;
        b.z = -61054.0f;
        pG->Room_flg[0] &= ~0x80000000;
    }
    {
        Vec ofs1 = {0.0f, 0.0f, -532.5f};
        Vec ofs2 = {100.49f, 0.0f, -474.38f};
        Vec rot = {0.0f, 0.0f, 0.0f};
        Vec p1;
        Vec p2;
        Mtx m;
        Vec ang;
        cPlayer* pl;

        rot.y = -angY;
        low_RotMatrix(m, &rot);
        TransMatrix(m, &a);
        PSMTXMultVec(m, &ofs1, &p1);
        pl = pPL;
        pl->setPos(&p1);
        ang.x = 0.0f;
        ang.y = angY;
        ang.z = 0.0f;
        pl->setAng(&ang);
        low_RotMatrix(m, &pPL->ang);
        TransMatrix(m, &p1);
        PSMTXMultVec(m, &ofs2, &p2);
        SetPosAng(pSUB, &p2, &pPL->ang);
        pPL->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x34), 0xA, 0, 1, 0);
        pSUB->motionSet(ROOM_ARC_PTR(pG->pRoom, 0x35), 0xA, 0, 1, 0);
        SceSleep((u32) MotionGetMaxFrame(&pSUB->Motion) - 70);
        SndCall(6, 5, 0, 0, 0, 0);
        SceMesSet(1, 0xA0, 1, 0x64, 0x150 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        SceSleep(0x19);
        CamCtrl.Comeback(0);
        pPL->setNoSuspend(0);
        pSUB->setNoSuspend(0);
        SetPosAng(pSUB, &b, &pPL->ang);
        SubCharCtrl(SCC_CHASE, 0);
        SceEventEnd(0);
    }
}

// Battle stream on while a Ganado has found the player or the bridge is down.
extern "C" void r208_StrCheck()
{
    if (SceCkFindPL(0) == 1 || (pG->Room_flg[0] & 0x10000000)) {
        if (RsfCheck(G_ROOM_ID, 13)) {
            if (W->strFlag == 0 && W->strOn == 1) {
                SndRoomStrStop(3);
                SndBgmTblSet(0x208, 1);
                W->strOn = 0;
            }
            if (W->strOn == 0) {
                SndRoomStrStart(1, 0, 1);
                W->strOn = 1;
                W->strFlag = 1;
            }
        } else {
            if (W->strOn == 0) {
                SndRoomStrStart(1, 0, 1);
                W->strOn = 1;
            }
        }
    } else {
        if (W->strOn == 1) {
            SndRoomStrStop(3);
            W->strOn = 0;
        }
    }
}

// End of footing A's rise (also its cancel path): object 0x57 snapped to y 5400, SE stopped, camera
// back, SceEventEnd, its collision / attribute pieces created, dust effect dropped.
static void footingA_up_exit()
{
    SmdGetObjPtr(0x57)->pos.y = 5400.0f;
    SndStop(W->footASe, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 1);
    EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 5);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
}

// Footing A rises out of the water (camera event).
static void footingA_up()
{
    f32 y;
    int cnt = 0;
    u32 se = 0;

    RsfSet(G_ROOM_ID, 10);
    SceEventStart(1);
    CamCtrl.CutCall(0xE);
    EstSet(0, -1, 0, 0, EFF_ROOM, 3, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    SceSetEventCancel(1, (TaskFunc) footingA_up_exit, 0, -1, 1);
    y = 2900.0f;
    while (y < 5400.0f) {
        SmdGetObjPtr(0x57)->pos.y = y;
        y += 41.666668f;
        SceSleep(1);
        if (cnt++ == 0x1F) {
            se = SndCall(6, 3, &SmdGetObjPtr(0x57)->pos, 0, 0, 0);
            W->footASe = se;
        }
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(0x1E);
    SceSetEventCancel(0, 0, 0, -1, 1);
    footingA_up_exit();
}

// End of footing B's rise: as footingA_up_exit for object 0x58 (pieces 2 / 4).
static void footingB_up_exit()
{
    SmdGetObjPtr(0x58)->pos.y = 5400.0f;
    SndStop(W->footBSe, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 2);
    EatMgr.create(ROOM_ARC_PTR(pG->pRoom, 5), 0, &r208_zeroVec, &r208_zeroVec, 4);
    EffectEspDelete(1, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(1, ESP_CORE_KIND_ROOM00, 0);
}

// Footing B rises out of the water (camera event).
static void footingB_up()
{
    f32 y;
    int cnt = 0;
    u32 se = 0;

    RsfSet(G_ROOM_ID, 11);
    SceEventStart(1);
    EstSet(0, -1, 0, 0, EFF_ROOM, 4, 1, ESP_CORE_KIND_ROOM00, 0, 0);
    CamCtrl.CutCall(0xE);
    SmdGetObjPtr(0x58)->pos.y = 2900.0f;
    SceSetEventCancel(1, (TaskFunc) footingB_up_exit, 0, -1, 1);
    // `y` is initialised AFTER the call: a call directly followed by the loop label gets flow's
    // `(use (const_int 0))` nop, which takes a sched1 issue slot and splits the `cnt`/`se` zero pair.
    y = 2900.0f;
    while (y < 5400.0f) {
        SmdGetObjPtr(0x58)->pos.y = y;
        y += 41.666668f;
        SceSleep(1);
        if (cnt++ == 0x1F) {
            se = SndCall(6, 3, &SmdGetObjPtr(0x58)->pos, 0, 0, 0);
            W->footBSe = se;
        }
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSleep(0x1E);
    SceSetEventCancel(0, 0, 0, -1, 1);
    footingB_up_exit();
}

// Ashley walks to the wall crank and turns it (the bridge comes down without the player).
static void SubUnderCrankExec()
{
    int set = 0;

    if (pSUB != NULL) {
        Vec pos = {447.0f, 4000.0f, -29225.0f};
        Vec d;

        pG->Room_flg[0] |= 0x08000000;   // struct view: the template copy's three loads precede its first frame store
        while (1) {
            f32 dist;

            PSVECSubtract(&pSUB->pos, &pos, &d);
            dist = PSVECMag(&d);
            if (RsfCheck(G_ROOM_ID, 6)) {
                break;
            }
            if (dist < 500.0f) {
                if (SubCharGetStatus() & 1) {
                    set = 1;
                    SetSubAux(funcAshley, 0);
                }
            } else if ((SubCharGetStatus() & 0x01000000) == 0) {
                if (set != 0) {
                    SubCharCtrl(SCC_CHASE, 0);
                    break;
                }
                SubCharMoveTo(pos.x, pos.y, pos.z, 193.0f, 0);
            }
            SceSleep(1);
        }
        pG->Room_flg[0] &= ~0x08000000;
    }
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        SceAtSetEnable(2, 1);
    }
}

// Continue point (Room_flg bit 12): autosave.
extern "C" void r208_continue()
{
    RsfSet(G_ROOM_ID, 12);
    GameSave.save(pSaveData, -1);
}
