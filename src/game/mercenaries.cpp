// game/mercenaries: the Mercenaries minigame (score, combo, bonus time, result screens)
// (D:/Bio4/Prog/mercenaries.cpp).
#include "types.h"
#include "global.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "sofdec.h"
#include "id_sys.h"
#include "mes.h"
#include "main.h"
#include "main_mem.h"
#include "snd.h"
#include "cockpit.h"
#include "sce.h"
#include "sce_sys.h"
#include "game.h"
#include "player.h"
#include "obj.h"
#include "scroll.h"
#include "motion.h"
#include "cam_ctrl.h"
#include "fade.h"
#include "dvd.h"
#include "cDataSwap.h"
#include "option.h"
#include "mercenaries.h"
#include "ref_access.h"
#include <string.h>

extern "C" {
static void IdSetColLoop(IDSystem* id, int no, u8 type, int on);
}

#define DATA_PTR(d, ofs) ((void*) (*(u32*) ((u8*) (d) + (ofs)) + (u32) (d)))
#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// Combo counter shown (bit 24) / hiding (bit 23), bonus time shown / hiding (22 / 21),
// bonus points shown (20), time warning colour (19), time added (18), all ranks S (25),
// per-stage record unlock (26..29, mercSysGetFlag), 31: cleared at room start.
#define MF_COMBO_ON 0x01000000
#define MF_COMBO_OFF 0x00800000
#define MF_BONUS_ON 0x00400000
#define MF_BONUS_OFF 0x00200000
#define MF_BONUS_SCORE 0x00100000
#define MF_TIME_WARN 0x00080000
#define MF_ADD_TIME 0x00040000
#define MF_ALL_RANK 0x02000000





// 1 while a fade slot is still fading.
// Through a pointer parameter: `&Fade[2]` stays a loop-invariant pseudo (`addi rX, Fade+0x48@l`).
static inline int fadeIsOn(FadeWork* f)
{
    return f->flags & 1;
}

#define EXT_FLAG_TBL ((u32*) &pSys->Extra_flg)

#define EXT_FLAG_TBL_S ((u32*) &pSys->Extra_flg)
#define MID (&mercId._idSys)

MercSysWork MercSysWk;
MercID mercId;

static int MercMin = 2;
int MercSec = 0;
int MercCes = 0;
static u32 BonusTimeAdd = 1000;
int ComboTimerMax = 300;
int ComboTimerFlash = 120;
static int BonusTimerFlash = 120;

// bit of MercSysWork::flags set when the stage record is unlocked
u32 mercSysGetFlag[4] = {2, 3, 4, 5};
// pSys->Extra_flg bit per stage: extra content unlocked
u32 extFlagTbl[4] = {4, 6, 5, 7};
// score thresholds per stage and rank
u32 RankTbl[4][6] = {
    {0, 1, 10000, 20000, 30000, 60000},
    {0, 1, 10000, 20000, 30000, 60000},
    {0, 1, 10000, 20000, 30000, 60000},
    {0, 1, 10000, 20000, 30000, 60000},
};

// points per kill by enemy kind and combo count (combo 1..9, 10+)
const u32 addScoreTbl[10][10] = {
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {5000, 5500, 6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {7000, 7500, 8000, 8500, 9000, 9500, 10000, 10500, 11000, 11500},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {300, 320, 350, 400, 500, 550, 600, 650, 700, 1000},
    {10000, 10500, 11000, 11500, 12000, 12500, 13000, 13500, 14000, 14500},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
// base points per enemy kind
const u32 defaultScoreTbl[10] = {300, 300, 5000, 300, 300, 7000, 300, 300, 10000, 0};

// Stage init of the Mercenaries: clears the whole MercSysWork.
int MercSysInitStage()
{
    MercSysWork* wk = &MercSysWk;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    memset(wk, 0, sizeof(MercSysWork));
    return 1;
}

// Starts BGM stream `no` and returns its request id.
// The SndStrReq 0.0 pool load is issued below the table stores: the call was an inlined wrapper
// (integrate.c drops RTX_UNCHANGING_P from the pool MEM, so the load depends on the stores).
static inline u32 MercStrReq(int no)
{
    return SndStrReq(0, no, 0x80000003, 0, 0, 0.0f);
}

// Room start of a Mercenaries map: clears the score part, difficulty preset 2, stage from room_id
// (r400 0, r402 1, r403 2, r404 3), mode from pl_type (Leon 0, Ada 1, HUNK 2,
// Krauser 3, Wesker 4), copies the room's message/motion parameters, creates the intro dummy model
// (SetObjSmd) at the start position, puts the player there, starts the MercSysMoveMain scenario
// task, the mode's BGM stream and the id graphics (id400.dat).
int MercSysInitRoom(MercInit* pMInit)
{
    MercSysWork* wk = &MercSysWk;
    cObj* smd;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (pMInit == NULL) {
        pLog->err(0, 0, "St4ResultInitRoom : pMInit is NULL");
        return 0;
    }
    memset(&wk->score, 0, sizeof(MercSysWork) - 0x24);
    GamePointInit(2);
    {
        // COMPILER-DIFF: #13 (int shape): the original never allocates the single-use REG_EQUIV zero;
        // reload re-materialises `li r11, 0` right before the store (after the pG load), where ours
        // local-allocs the constant to r0 and hoists it above the load.
        register int z asm("r11"); // COMPILER-DIFF: #13
        z = 0;
        wk->stage = z;
    }
    if (pG->room_id != 0x400) {
        if (pG->room_id == 0x402) {
            wk->stage = 1;
        } else if (pG->room_id == 0x403) {
            wk->stage = 2;
        } else if (pG->room_id == 0x404) {
            wk->stage = 3;
        } else {
            pLog->err(0, 0, "St4ResultInitRoom : RoomNo failed");
        }
    }
    wk->mode = 0;
    if (pG->pl_type == 0) {
        wk->mode = 0;
    }
    if (pG->pl_type == 2) {
        wk->mode = 1;
    }
    if (pG->pl_type == 4) {
        wk->mode = 2;
    }
    if (pG->pl_type == 3) {
        wk->mode = 3;
    }
    if (pG->pl_type == 5) {
        wk->mode = 4;
    }
    wk->CamNo = pMInit->CamNo;
    wk->smdMot = pMInit->smdMot;
    wk->ClearScore = pMInit->ClearScore;
    wk->mesStart = pMInit->mesStart;
    wk->mesA8 = pMInit->mesA8;
    wk->mesAC = pMInit->mesAC;
    wk->MesNoStart03 = pMInit->MesNoStart03;
    wk->mes[0] = pMInit->mes[0];
    wk->mes[1] = pMInit->mes[1];
    wk->mes[2] = pMInit->mes[2];
    wk->mes[3] = pMInit->mes[3];
    wk->mes[4] = pMInit->mes[4];
    wk->mes[5] = pMInit->mes[5];
    wk->mes[6] = pMInit->mes[6];
    wk->mes[7] = pMInit->mes[7];
    wk->mes[8] = pMInit->mes[8];
    wk->mes[9] = pMInit->mes[9];
    smd = SetObjSmd(ARC_PTR(ofs_20), ARC_PTR(ofs_24), &pMInit->pos, &pMInit->rot, 0x10, 0);
    wk->smd = smd;
    if (smd == NULL) {
        pLog->err(0, 0, "St4ResultInitRoom : DummyModel no create");
    } else {
        smd->setNoSuspend(1);
        smd->setMove(1);
        if (smd->Motion.pAttachCam == NULL) {
#line 274 "D:/Bio4/Prog/mercenaries.cpp"
            smd->Motion.pAttachCam = (AttachCamera*) MEM_CALLOC(0x98, 1, 13);
        }
    }
    pPL->setPos(&pMInit->pos);
    pPL->setAng(&pMInit->rot);
    pPL->matUpdate();
    CamCtrl.Comeback(0);
    SceExec(0x12, (TaskFunc) MercSysMoveMain, (int) wk, 4, SCE_PRIO_DEF_2, 0);
    {
        int strTbl[5] = {0x3F, 0x40, 0x41, 0x42, 0x3D};

        wk->strId = MercStrReq(strTbl[wk->mode]);
    }
    wk->flags &= 0x7FFFFFFF;
    mercId.init(0x60);
    return 1;
}

// The intro: an event with the countdown set to MercMin:MercSec, the dummy model's motion, the
// mission start message (plus the "not yet unlocked" / rank hints), then the model is destroyed
// and the player released.
int MercSysMoveStart(MercSysWork* wk)
{
    u8* st;
    cObj* smd;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    st = wk->startSt;
    smd = wk->smd;
    memset(st, 0, 5);
    SceSleep(2);
    SceEventStart(1);
    pPL->beginEvent(0);
    pPL->setNoSuspend(1);
    Cckpt.getCountDown()->m_state |= TIMER_STA_ALIVE;
    Cckpt.getCountDown()->initTime(MercMin, MercSec, MercCes);
    // Codeless fake store surviving to global alloc: one more real insn in the range of the
    // hoisted `mercId.idsys` high (r24) but not in `&cMes`'s (r25), so their equal-priority
    // buckets (int(10000 * floor(log2 refs) * refs / len): 290/289) tie and the lower pseudo
    // (&cMes) is allocated first, as in the original.
    asm("" : "=m"(*(u16*) st)); // COMPILER-DIFF: tie (global-alloc live length)
    Cckpt.getCountDown()->frameOut();
    st[0] = 1;
    do {
        switch (st[1]) {
        case 0:
            if (smd != NULL) {
                MotionSetCore(smd, MOTION(smd), wk->smdMot, 0, 0, 0x200, 0);
            }
            st[1]++;
            break;
        case 1: {
            MesWork* m = cMes.getWork();

            SceMesSet(wk->mesStart, 0x20, 1, 100, MES_Y(m));
            if (!FlagChkVar(EXT_FLAG_TBL, extFlagTbl[wk->stage])) {
                SceMesSet(wk->mesA8, 0x20, 1, 100, MES_Y(m));
            } else {
                MercSaveWork save;

                MercSysGetSaveWork(&save);
                if (save.rank[wk->mode][wk->stage] <= 4) {
                    SceMesSet(wk->mesAC, 0x20, 1, 100, MES_Y(m));
                }
            }
            if (pG->pl_type == 4) {
                SceMesSet(wk->mes[4], 0, 1, 100, MES_Y(cMes.getWork()));
            }
            st[1]++;
            break;
        }
        case 2:
            MotionClear(smd, 0);
            ObjMgr.destroy(smd);
            CamCtrl.clearAttachCamera();
            CamCtrl.m_system_flag &= ~8;
            st[0] = 0;
            break;
        }
        mercId._idSys.move();
        mercId._idSys.trans();
        SceSleep(1);
    } while (st[0] != 0);
    pPL->setNoSuspend(0);
    pPL->endEvent(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    return 1;
}

// Per-frame HUD/score bookkeeping: combo display start/timeout (comboTimer, flashing below
// ComboTimerFlash, resets the combo), bonus time display/timeout (bonusTimer) and the bonus kill
// score (500/1500/4000 by kills), the bonus score pop-up folded into the score when its animation
// ends, added time (MF_ADD_TIME) pushed into the countdown, and the low-time warning under 0:30.
int MercSysMoveScore(MercSysWork* pWk)
{
    int min;
    int sec;
    int cs;
    int zero;

    if (pWk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    // combo counter
    if (pWk->flags & MF_COMBO_ON) {
        pWk->flags &= ~(MF_COMBO_ON | MF_COMBO_OFF);
        IdSetTrans(MID, 0x30, IDC_GAUGE, 1);
        IdSetAnmStart(MID, 0x30, IDC_GAUGE, 1);
        IdSetColInit(MID, 0x30, IDC_GAUGE);
        IdSetColLoop(MID, 0x30, IDC_GAUGE, 0);
    }
    if (!(pWk->flags & MF_COMBO_OFF)) {
        if (pWk->comboTimer > 0) {
            pWk->comboTimer--;
            if (pWk->comboTimer > ComboTimerFlash) {
                IdSetTrans(MID, 0x30, IDC_GAUGE, 1);
                IdSetColInit(MID, 0x30, IDC_GAUGE);
                IdSetColLoop(MID, 0x30, IDC_GAUGE, 0);
            }
            if (pWk->comboTimer == ComboTimerFlash) {
                IdSetTrans(MID, 0x30, IDC_GAUGE, 1);
                IdSetColStart(MID, 0x30, 0x3F, IDC_GAUGE);
                IdSetColLoop(MID, 0x30, IDC_GAUGE, 1);
            }
            if (pWk->comboTimer == 0) {
                IdSetTrans(MID, 0x30, IDC_GAUGE, 1);
                IdSetColStart(MID, 0x30, 0x3E, IDC_GAUGE);
                IdSetColLoop(MID, 0x30, IDC_GAUGE, 0);
                pWk->flags |= MF_COMBO_OFF;
            }
        }
    } else {
        if (IdIsAnimEnd(MID, 0x30, IDC_GAUGE)) {
            pWk->combo = 0;
            pWk->flags &= ~MF_COMBO_OFF;
        }
    }
    // bonus time
    if (pWk->flags & MF_BONUS_ON) {
        pWk->flags &= ~(MF_BONUS_ON | MF_BONUS_OFF);
        IdSetTrans(MID, 0x40, IDC_GAUGE, 1);
        IdSetAnmStart(MID, 0x40, IDC_GAUGE, 1);
        IdSetColInit(MID, 0x40, IDC_GAUGE);
        IdSetColLoop(MID, 0x40, IDC_GAUGE, 0);
    }
    if (!(pWk->flags & MF_BONUS_OFF)) {
        if (pWk->bonusTimer > 0) {
            pWk->bonusTimer--;
            if (pWk->bonusTimer > BonusTimerFlash) {
                IdSetTrans(MID, 0x40, IDC_GAUGE, 1);
                IdSetColInit(MID, 0x40, IDC_GAUGE);
                IdSetColLoop(MID, 0x40, IDC_GAUGE, 0);
            }
            if (pWk->bonusTimer == BonusTimerFlash) {
                IdSetTrans(MID, 0x40, IDC_GAUGE, 1);
                IdSetColStart(MID, 0x40, 0x3F, IDC_GAUGE);
                IdSetColLoop(MID, 0x40, IDC_GAUGE, 1);
            }
            if (pWk->bonusTimer <= 1) {
                IdSetTrans(MID, 0x40, IDC_GAUGE, 1);
                IdSetColStart(MID, 0x40, 0x3E, IDC_GAUGE);
                IdSetColLoop(MID, 0x40, IDC_GAUGE, 0);
                pWk->flags |= MF_BONUS_OFF;
            }
        }
    } else {
        if (IdIsAnimEnd(MID, 0x40, IDC_GAUGE)) {
            pWk->bonusTimer = 0;
            pWk->flags &= ~MF_BONUS_OFF;
        }
    }
    // multi kill bonus
    if (pWk->killCnt > 7) {
        pWk->bonusScore += 4000;
    } else if (pWk->killCnt > 4) {
        pWk->bonusScore += 1500;
    } else if (pWk->killCnt > 2) {
        pWk->bonusScore += 500;
    }
    if (!(pWk->flags & MF_BONUS_SCORE)) {
        if (pWk->bonusScore > 0 && pWk->combo == 0 && pWk->bonusTimer == 0) {
            IdSetTrans(MID, 0x60, IDC_GAUGE, 1);
            IdSetAnmStart(MID, 0x60, IDC_GAUGE, 1);
            IdSetNum(MID, 0x61, IDC_GAUGE, pWk->bonusScore, 9999999, 7, 0);
            pWk->flags |= MF_BONUS_SCORE;
            pWk->bonusDisp = pWk->bonusScore;
            pWk->bonusScore = 0;
        }
    } else {
        if (IdIsAnimEnd(MID, 0x60, IDC_GAUGE)) {
            pWk->flags &= ~MF_BONUS_SCORE;
            pWk->score += pWk->bonusDisp;
        }
    }
    // score
    IdSetTrans(MID, 0x20, IDC_GAUGE, 1);
    IdSetNum(MID, 0x21, IDC_GAUGE, pWk->score, 9999999, 7, 0);
    // time added
    zero = 0;
    if (pWk->flags & MF_ADD_TIME) {
        pWk->flags &= ~MF_ADD_TIME;
        min = pWk->addTime / 60;
        sec = pWk->addTime % 60;
        cs = zero;
        pWk->addTime = zero;
        IdSetTrans(MID, 0x10, IDC_GAUGE, 1);
        IdSetAnmStart(MID, 0x10, IDC_GAUGE, 1);
        IdSetNum(MID, 0x15, IDC_GAUGE, min, 9, 1, 1);
        IdSetNum(MID, 0x13, IDC_GAUGE, sec, 99, 2, 1);
        IdSetNum(MID, 0x11, IDC_GAUGE, cs, 99, 2, 1);
    }
    // remaining time
    Cckpt.getCountDown()->getTime(&min, &sec, &cs);
    IdSetTrans(MID, 0, IDC_GAUGE, 1);
    IdSetNum(MID, 5, IDC_GAUGE, min, 99, 2, 1);
    IdSetNum(MID, 3, IDC_GAUGE, sec, 99, 2, 1);
    IdSetNum(MID, 1, IDC_GAUGE, cs, 99, 2, 1);
    {
        int safe = 1;

        if (min <= 0) {
            safe = sec > 29;
        }
        if (safe == 0) {
            if (!(pWk->flags & MF_TIME_WARN)) {
                IdSetColStart(MID, 0, 0xFE, IDC_GAUGE);
                IdSetColLoop(MID, 0, IDC_GAUGE, 1);
            }
            pWk->flags |= MF_TIME_WARN;
        } else {
            if (pWk->flags & MF_TIME_WARN) {
                IdSetColStart(MID, 0, 0xFD, IDC_GAUGE);
                IdSetColLoop(MID, 0, IDC_GAUGE, 0);
            }
            pWk->flags &= ~MF_TIME_WARN;
        }
    }
    IdSetNum(MID, 0x31, IDC_GAUGE, pWk->combo, 999, 3, 0);
    IdSetNum(MID, 0x51, IDC_GAUGE, BonusTimeAdd, 9999, 4, 0);
    IdSetNum(MID, 0x41, IDC_GAUGE, pWk->bonusKill, 99, 2, 0);
    pWk->killCnt = 0;
    return 1;
}

// The Mercenaries scenario task: intro, then every frame the score update and the countdown check
// (end at 0:00; the hurry-up sound in the last 15 s), the id move/trans; at the end clears the
// combo/bonus displays and runs the result screen.
int MercSysMoveMain(MercSysWork* pWk)
{
    u8* st;

    if (pWk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    st = pWk->mainSt;
    memset(st, 0, 5);
    MercSysMoveStart(pWk);
    mercId.dispMissionStart();
    st[0] = 1;
    do {
        MercSysMoveScore(pWk);
        if (!StaFlagChk(pG, STA_DIEDEMO)) {
            CountDown* cd = Cckpt.getCountDown();
            int end = 0;

            if (cd->checkState(TIMER_STA_ALIVE)) {
                end = cd->m_frame == 0;
            }
            if (end == 1) {
                st[0] = 0;
                break;
            }
        }
        if (Cckpt.getCountDown()->getFrame() <= 899) {
            if (pWk->sndId == 0) {
                pWk->sndId = SndCall(6, 0x78, 0, 0, 0, 0);
            }
        } else {
            if (pWk->sndId != 0) {
                SndCall(6, 0x7A, 0, 0, 0, 0);
                pWk->sndId = 0;
            }
        }
        mercId._idSys.move();
        mercId._idSys.trans();
        SceSleep(1);
    } while (st[0] != 0);
    pWk->flags &= ~(MF_COMBO_ON | MF_COMBO_OFF);
    IdSetTrans(&mercId._idSys, 0x30, IDC_GAUGE, 0);
    int zero = 0;
    pWk->flags &= ~(MF_BONUS_ON | MF_BONUS_OFF);
    IdSetTrans(&mercId._idSys, 0x40, IDC_GAUGE, 0);
    pWk->combo = zero;
    pWk->comboTimer = zero;
    pWk->bonusTimer = zero;
    SndCall(6, 0x7A, 0, 0, 0, 0);
    MercSysResultMove(pWk);
    return 1;
}

// Computes the result: remaining time, max combo, kills, rank from RankTbl[stage] (0..5), updates
// the saved high score / best rank per stage and mode, unlocks the stage's extra (rank >= 4 sets
// the unlock_flg bit), and the all-5-stars unlock (20 ranks of 5 -> unlock_flg 0x20000000).
int MercSysResultInit(MercSysWork* pWk)
{
    MercSaveWork save;
    int min;
    int sec;
    int cs;
    int i;

    if (pWk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    Cckpt.getCountDown()->getTime(&min, &sec, &cs);
    pWk->rslt.time = min * 6000 + sec * 100 + cs;
    pWk->rslt.maxCombo = pWk->maxCombo;
    pWk->rslt.kill = pWk->kill;
    pWk->rslt.mode = pWk->mode;
    pWk->rslt.rank = 0;
    pWk->rslt.score = pWk->score;
    for (i = 0;; i++) {
        if (i < 6 && RankTbl[pWk->stage][i] <= pWk->rslt.score) {
            pWk->rslt.rank = i;
        } else {
            break;
        }
    }
    if (pWk->rslt.rank > 5) {
        pLog->err(0, 0, "MercSysResult : RankId error %d", pWk->rslt.rank);
        pWk->rslt.rank = 5;
    }
    MercSysGetSaveWork(&save);
    if (save.stage[pWk->stage].score < pWk->rslt.score) {
        save.stage[pWk->stage].score = pWk->rslt.score;
        save.stage[pWk->stage].mode = pWk->rslt.mode;
        save.stage[pWk->stage].newFlag = 1;
    } else {
        save.stage[pWk->stage].newFlag = 0;
    }
    if (save.rank[pWk->rslt.mode][pWk->stage] < pWk->rslt.rank) {
        save.rank[pWk->rslt.mode][pWk->stage] = pWk->rslt.rank;
    }
    MercSysSetSaveWork(&save);
    pWk->rslt.hiScore = save.stage[pWk->stage].score;
    pWk->rslt.hiMode = save.stage[pWk->stage].mode;
    pWk->rslt.newRecord = save.stage[pWk->stage].newFlag;
    if (!FlagChkVar(EXT_FLAG_TBL_S, extFlagTbl[pWk->stage])) {
        if (pWk->rslt.rank > 3) {
            FlagOnVar(EXT_FLAG_TBL_S, extFlagTbl[pWk->stage]);
            FlagOnVar(&pWk->flags, mercSysGetFlag[pWk->stage]);
        }
    }
    {
        int cnt = 0;
        int k;
        int j;

        for (k = 0; k < 4; k++) {
            for (j = 0; j < 5; j++) {
                if (save.rank[j][k] > 4) {
                    cnt++;
                }
            }
        }
        if (!ExtFlagChk(pSys, EXT_GET_SW500) && cnt > 19) {
            ExtFlagOn(pSys, EXT_GET_SW500);
            pWk->flags |= MF_ALL_RANK;
        }
    }
    return 1;
}

// The result sequence: "time up", fade, MercSysResultInit, HUD off and game stopped, swaps the
// room archive out to load the result id data (omk_r1.dat), runs the MercResult screen, restores,
// saves the system file and requests the soft reset back to the title.
int MercSysResultMove(MercSysWork* pWk)
{
    static u32 stop_bak;
    static u32 disp_bak;
    static u32 MARGIN = 0x20000;
    static char data_name[] = "SS/___/omk_r1.dat";
    MercResult* pRslt;

    if (pWk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    {
        MercRsltSt* rs = &pWk->rsltSt;
        u32 size;

        memset(rs, 0, sizeof(MercRsltSt));
        rs->x8 = 0;
        SceEventStart(0);
        // constructed here: the 0x18-byte swap work is the first frame slot, the fade colours
        // and `size` only get theirs when their address is first taken
        cDataSwap swap;
        rs->run = 1;
        do {
            switch (rs->step) {
            case 0:
                mercId.dispTimeUp();
                SndRoomStrStop(3);
                SndRoomBgmStop(0, 3);
                SndStrReq(pWk->strId, 4, 600, 0);
                rs->cnt = 0;
                rs->step++;
            case 1:
                MercSysMoveScore(pWk);
                if (IdIsAnimEnd(&mercId._idSys, 0, IDC_EVENT)) {
                    FadeSetW(2, 0, 0, 0);
                    MercSysResultInit(pWk);
                    disp_bak = pG->Disp_flg;
                    pG->Disp_flg = 0xFFFFFFFF;
                    DpfFlagOff(pG, DPF_ID_SYSTEM);
                    DpfFlagOff(pG, DPF_MESSAGE);
                    DpfFlagOff(pG, DPF_COCKPIT);
                    stop_bak = pG->Stop_flg;
                    pG->Stop_flg = 0xFFFFFFFF;
                    SpfFlagOff(pG, SPF_SCE);
                    SpfFlagOff(pG, SPF_ID_SYSTEM);
                    rs->cnt = 0;
                    rs->step++;
                }
                break;
            case 2:
                rs->cnt++;
                if (rs->cnt > 1) {
                    mercId.kill();
                    setLangExt3(data_name + 3);
                    Dvd.FileExistCheck(data_name, &size);
                    size += 0x34;
                    size += MARGIN;
                    swap.SwapOut((u32) pG->pRoom, size, 0);
                    pRslt = new MercResult;
                    pRslt->init(pWk);
                    pWk->strId = SndStrReq(0, 0x3A, 0x80000003, 0, 0, 0.0f);
                    rs->cnt = 0;
                    rs->step++;
                }
                break;
            case 3:
                if (pRslt->move(pWk) == 0) {
                    SndStrReq(pWk->strId, 8, 0, 0);
                    rs->cnt = 0;
                    rs->step++;
                }
                break;
            case 4:
                if (fadeIsOn(&Fade[2]) == 0) {
                    pRslt->quit();
                    delete pRslt;
                    swap.SwapIn();
                    rs->run = 0;
                }
                break;
            }
            mercId._idSys.move();
            mercId._idSys.trans();
            SceSleep(1);
        } while (rs->run != 0);
        SceSleep(1);
        FadeSetW(2, 0, 0, 0);
        CardSysSave();
        SysFlagOn(pG, SYS_SOFT_RESET);
        CamCtrl.Comeback(0);
        SceEventEnd(0);
    }
    return 1;
}

// Unpacks the Mercenaries records from the system save: per stage the high score (x10, 28 bits),
// mode (3 bits) and new flag, and the 3-bit rank per (character, stage) from merc_rank.
void MercSysGetSaveWork(MercSaveWork* pSaveWk)
{
    int i;
    int j;

    // pSys read through a reference: its load is not hoisted above the stores through `save`
    // (a plain pSys is a fixed scalar that a varying struct store never aliases). The x10 table
    // goes through a pointer local: a pointer variable carries REG_POINTER, so regclass makes it
    // the base of the `tbl[i]` address and `i*4` the index (GENERAL_REGS: r0, dying at the load);
    // written `SysRef(pSys)->x10[i]` neither operand is flagged, `i*4` becomes BASE_REGS and its
    // longer life drags the rank-pointer giv init to the block end (r8 instead of r12).
    for (i = 0; i < 4; i++) {
        u32* tbl = pSys->MercSysRoom;
        u32 w = tbl[i];

        pSaveWk->stage[i].score = (w & 0x0FFFFFFF) * 10;
        pSaveWk->stage[i].mode = (w >> 28) & 7;
        pSaveWk->stage[i].newFlag = w >> 31;
        for (j = 0; j < 5; j++) {
            int r = 0;

            if (FlagChkVar(pSys->MercSysRank, (u32) (i * 15 + j * 3))) {
                r = 4;
            }
            if (FlagChkVar(pSys->MercSysRank, (u32) (i * 15 + j * 3 + 1))) {
                r |= 2;
            }
            if (FlagChkVar(pSys->MercSysRank, (u32) (i * 15 + j * 3 + 2))) {
                r |= 1;
            }
            pSaveWk->rank[j][i] = r;
        }
    }
}

// Packs the records back into the system save words.
void MercSysSetSaveWork(MercSaveWork* pSaveWk)
{
    int i;
    int j;

    for (i = 0; i < 4; i++) {
        {
            // Two-set `sc` (the load, then the scaled/masked value): two deaths make it a global
            // pseudo, so local-alloc cannot tie the first `or` to it (the result is tied to the mode
            // operand instead: `or r0,r7,r0`) and global.c gives it r7 after local-alloc took r10/r8
            // for i*12 / pSys.
            u32 sc = pSaveWk->stage[i].score;
            u32 w;
            sc = (sc / 10) & 0x0FFFFFFF;
            w = (sc | ((pSaveWk->stage[i].mode & 7) << 28)) | (pSaveWk->stage[i].newFlag << 31);
            pSys->MercSysRoom[i] = w;
        }
        for (j = 0; j < 5; j++) {
            int r = pSaveWk->rank[j][i];

            if (r & 4) {
                FlagOnVar(pSys->MercSysRank, (u32) (i * 15 + j * 3));
            }
            if (r & 2) {
                FlagOnVar(pSys->MercSysRank, (u32) (i * 15 + j * 3 + 1));
            }
            if (r & 1) {
                FlagOnVar(pSys->MercSysRank, (u32) (i * 15 + j * 3 + 2));
            }
        }
    }
}

// Score event from the game: kind 9 adds pt directly; otherwise a kill of enemy kind (0..8):
// increments the combo (starting the combo display at 2), scores defaultScoreTbl[kind] plus the
// combo bonus from addScoreTbl (banked into bonusScore, at least BonusTimeAdd during bonus time),
// counts kills. Ignored outside the Mercenaries (System_flg 0x40000000).
int MercSysSetPoint(int type, int point)
{
    MercSysWork* wk = &MercSysWk;
    u32 idx;
    u32 add;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (!SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        return 1;
    }
    if (type == MT_ITEM) {
        wk->score += point;
        return 1;
    }
    if (wk->combo == 1) {
        wk->flags |= MF_COMBO_ON;
    }
    wk->combo++;
    if (wk->maxCombo < wk->combo) {
        wk->maxCombo = wk->combo;
    }
    if (wk->combo > 1) {
        wk->comboTimer = ComboTimerMax;
        wk->flags &= ~MF_COMBO_OFF;
    }
    idx = wk->combo - 1;
    if (idx > 8) {
        idx = 9;
    }
    add = addScoreTbl[type][idx] - defaultScoreTbl[type];
    if (wk->bonusTimer > 0) {
        wk->bonusKill++;
        if (add < BonusTimeAdd) {
            add = BonusTimeAdd;
        }
    }
    wk->bonusScore += add;
    wk->score += defaultScoreTbl[type];
    wk->killCnt++;
    wk->kill++;
    return 1;
}

// Time bonus pick-up: queues sec seconds to add to the countdown.
int MercSysSetAddTime(int time)
{
    MercSysWork* wk = &MercSysWk;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (!SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        return 1;
    }
    wk->addTime += time;
    wk->flags |= MF_ADD_TIME;
    // no return: the original falls off the end (r3 still holds `sec`)
}

// Bonus time pick-up: extends the bonus timer (starting the display when it was 0).
int MercSysSetBonusTime(int time)
{
    MercSysWork* wk = &MercSysWk;

    if (wk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    if (!SysFlagChk(pG, SYS_OMAKE_ETC_GAME)) {
        return 1;
    }
    if (wk->bonusTimer == 0) {
        wk->bonusKill = 0;
        wk->flags |= MF_BONUS_ON;
    }
    wk->bonusTimer += time;
    wk->flags &= ~MF_BONUS_OFF;
    return 1;
}

// Shows/hides id unit (no, type).
void IdSetTrans(IDSystem* pIdSys, int idmNo, u8 idcNo, int flag)
{
    IdUnit* u = pIdSys->unitPtr(idmNo, idcNo);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetTrans : pIdUnit is NULL");
    } else {
        if (flag == 1) {
            u->be_flag |= 8;
        } else {
            u->be_flag &= ~8;
        }
    }
}

// Restarts an id unit's animation forwards (on) or backwards.
void IdSetAnmStart(IDSystem* pIdSys, int idmNo, u8 idcNo, int flag)
{
    IdUnit* u = pIdSys->unitPtr(idmNo, idcNo);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetAnmStart : pIdUnit is NULL");
    } else {
        if (flag == 1) {
            u->rev_flag &= ~0xF;
        } else {
            u->rev_flag |= 0xF;
        }
        pIdSys->setTime(u, 0);
    }
}

// Resets an id unit to opaque white with no colour curve.
void IdSetColInit(IDSystem* pIdSys, int idmNo, u8 idcNo)
{
    IdUnit* u = pIdSys->unitPtr(idmNo, idcNo);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetColInit : pIdUnit is NULL");
    } else {
        u->col[0] = 255.0f;
        u->col[1] = 255.0f;
        u->col[2] = 255.0f;
        u->col[3] = 255.0f;
        u->curve[2] = NULL;
    }
}

// Sets/clears the colour curve loop bit of an id unit (flashing).
static void IdSetColLoop(IDSystem* pIdSys, int idmNo, u8 idcNo, int flag)
{
    IdUnit* u = pIdSys->unitPtr(idmNo, idcNo);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetColInit : pIdUnit is NULL");
    } else {
        if (flag == 1) {
            u->loop_flag |= 4;
        } else {
            u->loop_flag &= ~4;
        }
    }
}

// Copies the colour curve and colours of unit src onto unit no and restarts it.
void IdSetColStart(IDSystem* pIdSys, int idmNo0, int idmNo1, u8 idcNo)
{
    IdUnit* u = pIdSys->unitPtr(idmNo0, idcNo);
    IdUnit* s = pIdSys->unitPtr(idmNo1, idcNo);

    if (u == NULL || s == NULL) {
        pLog->err(0, 0, "IdSetColStart : pIdUnit is NULL");
    } else {
        u->col0[0] = s->col0[0];
        u->col0[1] = s->col0[1];
        u->col0[2] = s->col0[2];
        u->col0[3] = s->col0[3];
        u->col1[0] = s->col1[0];
        u->col1[1] = s->col1[1];
        u->col1[2] = s->col1[2];
        u->col1[3] = s->col1[3];
        u->curve[2] = s->curve[2];
        u->timer[2] = 0;
    }
}

// Shows `val` (clamped to `max`) as `digits` decimal digits on the units no..no+digits-1;
// mode 0 hides leading zeros.
void IdSetNum(IDSystem* pIdSys, int idmNo, u8 idcNo, int num, int max, int keta, int mode)
{
    int d[32];
    int show;
    int i;

    if (num > max) {
        num = max;
    }
    for (int j = 0; j < keta; j++) {
        d[j] = num % 10;
        num /= 10;
    }
    show = mode;
    for (i = keta - 1; i >= 0; i--) {
        IdUnit* u = pIdSys->unitPtr(idmNo + i, idcNo);

        if (u == NULL) {
            pLog->err(0, 0, "IdSetNum : pIdUnit is NULL");
            return;
        }
        if (show == 0 && d[i] == 0 && i != 0) {
            u->be_flag &= ~8;
        } else {
            u->be_flag |= 8;
            show = 1;
            u->tex_flag |= 2;
            u->texNo = d[i];
        }
    }
}

// Sets an id unit's texture frame (held).
void IdSetTexNo(IDSystem* pIdSys, int idmNo, u8 idcNo, int texNo)
{
    IdUnit* u = pIdSys->unitPtr(idmNo, idcNo);

    if (u == NULL) {
        pLog->err(0, 0, "IdSetTexNo : pIdUnit is NULL");
    } else {
        u->texNo = texNo;
        u->tex_flag |= 2;
    }
}

// 1 when the unit's position or size curve has ended.
int IdIsAnimEnd(IDSystem* pIdSys, int idmNo, u8 idcNo)
{
    IdUnit* u = pIdSys->unitPtr(idmNo, idcNo);

    if (u != NULL) {
        return (u->anima_state & 3) ? 1 : 0;
    }
    pLog->err(0, 0, "IdIsAnimEnd : pIdUnit is NULL");
    return 1;
}

// Loads the Mercenaries HUD id data (SS/<lang>/id400.dat) and registers its textures.
void MercID::init(int num)
{
    static char data_name[] = "SS/___/id400.dat";
    void* addr;

    setLangExt3(data_name + 3);
#line 1715 "D:/Bio4/Prog/mercenaries.cpp"
    Dvd.ReadCheck(DVD_READ_N(data_name, 0, 0, 0, 0, 5), 0, 0, &addr);
    pData = addr;
    _idSys.gameInit(num);
    _idSys.roomInit();
    pTex = DATA_PTR(pData, 0x10);
    pIdMain = DATA_PTR(pData, 0x14);
    pIdStart = DATA_PTR(pData, 0x18);
    pIdTimeUp = DATA_PTR(pData, 0x1C);
    set();
    _idSys.set(pIdMain, 0xFF, IDC_GAUGE, 0x13, 5, 0);
    IdSetTrans(&_idSys, 0x20, IDC_GAUGE, 0);
    IdSetTrans(&_idSys, 0x60, IDC_GAUGE, 0);
    IdSetTrans(&_idSys, 0, IDC_GAUGE, 0);
    IdSetTrans(&_idSys, 0x10, IDC_GAUGE, 0);
    IdSetTrans(&_idSys, 0x30, IDC_GAUGE, 0);
    IdSetTrans(&_idSys, 0x40, IDC_GAUGE, 0);
}

// Registers the HUD textures under the event id texture owner.
void MercID::set()
{
    IdTexRelease(TEX_OWNER_ID_EVENT);
    IdTexDataLoad(pTex, TEX_OWNER_ID_EVENT);
}

// Releases the HUD textures.
void MercID::kill()
{
    IdTexRelease(TEX_OWNER_ID_EVENT);
    _idSys.kill(0xFF, IDC_GAUGE);
    _idSys.kill(0xFF, IDC_EVENT);
}

// Shows the "mission start" id animation with its sound.
void MercID::dispMissionStart()
{
    _idSys.set(pIdStart, 0xFF, IDC_EVENT, 0x13, 4, 0);
    SndCall(6, 0x7C, 0, 0, 0, 0);
}

// Shows the "time up" id animation with its sound.
void MercID::dispTimeUp()
{
    _idSys.set(pIdTimeUp, 0xFF, IDC_EVENT, 0x13, 4, 0);
    SndCall(6, 0x7E, 0, 0, 0, 0);
}

// Loads the result screen id data (omk_r1.dat: 5 rank layouts, extra unlock, end) and its textures.
int MercResult::init(MercSysWork* pWk)
{
    static char data_name[] = "SS/___/omk_r1.dat";
    void* addr;

    if (pWk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    setLangExt3(data_name + 3);
#line 1866 "D:/Bio4/Prog/mercenaries.cpp"
    Dvd.ReadCheck(DVD_READ_N(data_name, 0, 0, 0, 0, 5), 0, 0, &addr);
    omk_addr = addr;
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    pTex = DATA_PTR(omk_addr, 0x10);
    pIdRank[0] = DATA_PTR(omk_addr, 0x14);
    pIdRank[1] = DATA_PTR(omk_addr, 0x18);
    pIdRank[2] = DATA_PTR(omk_addr, 0x1C);
    pIdRank[3] = DATA_PTR(omk_addr, 0x20);
    pIdRank[4] = DATA_PTR(omk_addr, 0x24);
    pIdExtra = DATA_PTR(omk_addr, 0x28);
    pIdEnd = DATA_PTR(omk_addr, 0x2C);
    IdTexDataLoad(pTex, TEX_OWNER_ID_TITLE);
    IdSys.set(pIdRank[pWk->rslt.mode], 0xFF, IDC_TITLE, 0x13, 6, 0);
    _rno0 = 0;
    _rno1 = 0;
    _rno2 = 0;
    _rno3 = 0;
    return 1;
}

// Result screen state machine (_rno0): fade in and show the rank layout with score/time/combo/kills
// digits, wait for A; then (0xA) the "new character unlocked" screen with its message, then
// (0x14) the all-clear screen. Returns 0 when finished.
int MercResult::move(MercSysWork* pWk)
{
    int mes[4];

    if (pWk == NULL) {
        pLog->err(0, 0, "St4ResultInitStage : pWk is NULL");
        return 0;
    }
    mes[0] = pWk->mes[5];
    mes[1] = pWk->mes[6];
    mes[2] = pWk->mes[7];
    mes[3] = pWk->mes[8];
    switch (_rno0) {
    case 0:
        FadeSetW(0x80000002, 10, 0, 0);
        _rno0++;
        break;
    case 1:
        IdSetNum(&IdSys, 0x11, IDC_TITLE, pWk->rslt.kill, 9999, 4, 0);
        IdSetNum(&IdSys, 0x21, IDC_TITLE, pWk->rslt.score, 999999, 6, 0);
        IdSetNum(&IdSys, 0x31, IDC_TITLE, pWk->rslt.maxCombo, 999, 3, 0);
        for (int i = 1; i <= 5; i++) {
            IdSetTrans(&IdSys, i, IDC_TITLE, i <= pWk->rslt.rank);
        }
        IdSetTrans(&IdSys, 0, IDC_TITLE, 1);
        IdSetTexNo(&IdSys, 0, IDC_TITLE, pWk->rslt.hiMode);
        IdSetNum(&IdSys, 0x41, IDC_TITLE, pWk->rslt.hiScore, 999999, 6, 0);
        if (Key.trg & KEY_A) {
            FadeSetW(2, 10, 0, 0);
            if (FlagChkVar(&pWk->flags, mercSysGetFlag[pWk->stage])) {
                _rno0 = 0xA;
            } else if (pWk->flags & MF_ALL_RANK) {
                _rno0 = 0x14;
            } else {
                return 0;
            }
        }
        break;
    case 0xA:
        if (Fade[2].flags & 1) {
            break;
        }
        FadeSetW(0x80000002, 10, 0, 0);
        IdSys.kill(0xFF, IDC_TITLE);
        IdSys.set(pIdExtra, 0xFF, IDC_TITLE, 0x13, 4, 0);
        for (int i = 0; i < 4; i++) {
            int on = 0;

            if (FlagChkVar(EXT_FLAG_TBL, extFlagTbl[i])) {
                on = 1;
            }
            IdSetTrans(&IdSys, i + 1, IDC_TITLE, on);
        }
        IdSetTrans(&IdSys, pWk->stage + 1, IDC_TITLE, 1);
        IdSetAnmStart(&IdSys, pWk->stage + 1, IDC_TITLE, 1);
        IdSetColStart(&IdSys, pWk->stage + 1, 0, IDC_TITLE);
        _rno1 = 0;
        _rno0++;
        break;
    case 0xB:
        if (Fade[2].flags & 1) {
            break;
        }
        _rno1++;
        if (_rno1 > 29) {
            SceMesSet(mes[pWk->stage], 0xF0, 1, 100, MES_Y(cMes.getWork()));
            _rno0++;
        }
        break;
    case 0xC:
        if (Key.trg & KEY_A) {
            MessageControl* m = &cMes;

            for (int i = 0; i < 16; i++) {
                m->Delete(i);
            }
            FadeSetW(2, 10, 0, 0);
            if (pWk->flags & MF_ALL_RANK) {
                _rno0 = 0x14;
            } else {
                return 0;
            }
        }
        break;
    case 0x14:
        if (Fade[2].flags & 1) {
            break;
        }
        FadeSetW(0x80000002, 10, 0, 0);
        IdSys.kill(0xFF, IDC_TITLE);
        IdSys.set(pIdEnd, 0xFF, IDC_TITLE, 0x13, 4, 0);
        _rno1 = 0;
        _rno0++;
        break;
    case 0x15:
        if (Fade[2].flags & 1) {
            break;
        }
        _rno1++;
        if (_rno1 > 29) {
            SceMesSet(pWk->mes[9], 0xF0, 1, 100, MES_Y(cMes.getWork()));
            _rno0++;
        }
        break;
    case 0x16:
        if (Key.trg & KEY_A) {
            MessageControl* m = &cMes;

            for (int i = 0; i < 16; i++) {
                m->Delete(i);
            }
            FadeSetW(2, 10, 0, 0);
            return 0;
        }
        break;
    }
    return 1;
}

// Restores the cockpit after the result screen.
void MercResult::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

// Loads the Assignment Ada result id data (omk_r0.dat).
void AdaResult::init()
{
    static char data_name[] = "SS/___/omk_r0.dat";
    void* addr;

    setLangExt3(data_name + 3);
#line 2141 "D:/Bio4/Prog/mercenaries.cpp"
    Dvd.ReadCheck(DVD_READ_N(data_name, 0, 0, 0, 0, 5), 0, 0, &addr);
    omk_addr = addr;
    IdTexRelease(TEX_OWNER_ID_COCKPIT);
    IdSys.roomInit();
    pTex = DATA_PTR(omk_addr, 0x10);
    pId = DATA_PTR(omk_addr, 0x14);
    IdTexDataLoad(pTex, TEX_OWNER_ID_TITLE);
    IdSys.set(pId, 0xFF, IDC_TITLE, 0x13, 6, 0);
    _rno0 = 0;
    _rno1 = 0;
    _rno2 = 0;
    _rno3 = 0;
}

// Assignment Ada result: fade in, show the layout and message mesNo, wait for A. Returns 0 when done.
int AdaResult::move(int messNo)
{
    int i;

    switch (_rno0) {
    case 0:
        FadeSetW(0x80000002, 10, 0, 0);
        IdSys.set(pId, 0xFF, IDC_TITLE, 0x13, 4, 0);
        _rno1 = 0;
        _rno0++;
        break;
    case 1:
        if (Fade[2].flags & 1) {
            break;
        }
        _rno1++;
        if (_rno1 > 29) {
            SceMesSet(messNo, 0xF0, 1, 100, MES_Y(cMes.getWork()));
            _rno0++;
        }
        break;
    case 2:
        if (Key.trg & KEY_A) {
            MessageControl* m = &cMes;

            for (i = 0; i < 16; i++) {
                m->Delete(i);
            }
            FadeSetW(2, 10, 0, 0);
            return 0;
        }
        break;
    }
    return 1;
}

// Restores the cockpit.
void AdaResult::quit()
{
    Cckpt.roomInit();
    Cckpt.move();
}

// Countdown state bit test (bit 0 = running).
int CountDown::checkState(u32 state)
{
    return (m_state & state) ? 1 : 0;
}
