// game/espgen00: effect controller 00, the repeating emitter (D:/Bio4/Prog/espgen00.cpp). A
// controller record of Kind 1 / Espgen_type 0 spawns its esp record `num+1` times every `wait`
// frames for `life` frames, following a parts of its model, with per-emission scale/speed/alpha
// curves (Calc_D256). Entry points: Espgen00_Move (EspgenMoveTbl), Espgen00_SetFreeWork.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "db_log.h"

struct Espgen00Work;

extern "C" {
void espgen00_UpdateMatrix(EspgenWork* w);
void espgen00_Update(EspgenWork* w);
void espgen00_Move00(EspgenWork* w);
void espgen00_Move01(EspgenWork* w);
static f32 Calc_D256(Espgen00Work* p, u8 d, f32 rate);
}

// Effect controller 00: emits one esp record repeatedly (num at a time, every wait frames).
struct Espgen00Work {
    EspGenWork* rec;   // 0x14
    cModel* pMod;     // 0x18
    u32 Guid_pMod;        // 0x1C model serial the controller was set up with
    u16 Time_cnt;           // 0x20 frame counter
    u16 life;          // 0x24 life time (0 = infinite)
    u8 pad_24;
    u8 wait;           // 0x25 frames between emissions
    u8 waitCnt;        // 0x26 frames left until the next emission
    u8 num;            // 0x27 emissions per frame - 1
    u32 seed;          // 0x28
    u8 Flg;          // 0x2C rec->x10B: bit0 spread the angle, bit1 fixed seed
    u8 flags2;         // 0x2D bit0 parts matrix fixed, bit1 head flag, bit2 pass the position on
    u8 parts;          // 0x2E
    u8 waitRnd;        // 0x2F random range added to the wait
    Mtx Mat;           // 0x30
    Vec Offset;           // 0x60
    Vec Ang;           // 0x6C
    u8 scaleD;         // 0x78 rate curve parameters (Calc_D256)
    u8 spdD;           // 0x79
    u8 colD;           // 0x7A
    u8 waitD;          // 0x7B
    EspSeqOpt opt;     // 0x7C
    EspSeqOpt* pOpt;   // 0x98
};

// Rebuilds the emitter matrix from parts `parts` of pMod (parts rotation + Offset/Ang), once when
// flags2 bit 0 is not set (bit 1 = keep following every frame). 0xFE = free position (matrix left
// as set up); 0xF8..0xFD/0xFF or a missing parent kill the controller (PushEspgen) with a log.
void espgen00_UpdateMatrix(EspgenWork* w)
{
    Espgen00Work* p = (Espgen00Work*) w->work;
    cModel* model = p->pMod;

    if ((p->parts >= 0xF8 && p->parts <= 0xFD) || p->parts == 0xFF) {
        pLog->err(0, 0, "ESP_CTRL : NULL_PARTS_NO[%x] invalid.", p->parts);
        PushEspgen(w);
        return;
    }
    if (p->parts == 0xFE) {
        return;
    }
    if (model == NULL) {
        pLog->err(0, 0, "ESP_CTRL : PARTS_NO is set but No Parent.");
        return;
    }
    if (!(p->flags2 & 1)) {
        if (p->parts < model->nParts) {
            cModel* part;
            Vec ofs;
            Vec r;

            part = model->getPartsPtr(p->parts);
            PSMTXIdentity(p->Mat);
            PSVECAdd(&p->Ang, &model->ang, &r);
            RotMatrix(p->Mat, &r);
            PSMTXMultVecSR(p->Mat, &p->Offset, &ofs);
            p->Mat[0][3] = part->mat[0][3] + ofs.x;
            p->Mat[1][3] = part->mat[1][3] + ofs.y;
            p->Mat[2][3] = part->mat[2][3] + ofs.z;
            if (!(p->flags2 & 2)) {
                p->flags2 |= 1;
            }
        } else {
            pLog->err(0, 0, "ESP_CTRL : PARTS_NO[%d] is invalid(MAX:%d).", p->parts, model->nParts);
            PushEspgen(w);
            return;
        }
    }
}

// Rate curve: d >= 0 fades 1 -> 1 - d/128 over the life, d < 0 grows 1 -> 1 + 10 * -d/128.
static f32 Calc_D256(Espgen00Work* p, u8 d, f32 rate)
{
    s8 v = d;
    f32 t;
    f32 ret;

    // t is assigned in both arms: a global pseudo, so local-alloc does not tie the
    // (f32)d / constant operands to the product (d -> f13, 1/128 -> f12, 1.0 -> f11)
    if (v >= 0) {
        t = (f32) d * 0.0078125f;
        ret = 1.0f - rate * t;
    } else {
        t = (f32) v * -0.0078125f;
        ret = rate * t * 10.0f + 1.0f;
    }
    return ret;
}

// One emitter frame: kills the controller when the model died or was reused (serial mismatch), then
// when waitCnt reaches 0 emits num+1 copies of the record through EspSeqSet (Flg bit 0 spreads them
// over 2pi), scaling size/speed/alpha by the life-rate curves scaleD/spdD/colD and reloading
// waitCnt from wait (+waitD curve, +-waitRnd). Ends itself after `life` frames.
void espgen00_Update(EspgenWork* w)
{
    Espgen00Work* p = (Espgen00Work*) w->work;
    f32 spdR = 0.0f;
    f32 scaleR = 0.0f;
    f32 colR = 0.0f;
    int bScale = 0;
    int bSpd = 0;
    int bCol = 0;
    int add = 0;
    cModel* model = p->pMod;

    if (model != NULL) {
        if ((model->be_flag & 0x201) != 1 || model->serial != p->Guid_pMod) {
            PushEspgen(w);
            return;
        }
    }
    espgen00_UpdateMatrix(w);
    if (p->life != 0) {
        f32 rate = (f32) p->Time_cnt / (f32) (int) p->life;

        if (p->scaleD) {
            scaleR = Calc_D256(p, p->scaleD, rate);
            bScale = 1;
        }
        if (p->spdD) {
            spdR = Calc_D256(p, p->spdD, rate);
            bSpd = 1;
        }
        if (p->colD) {
            colR = Calc_D256(p, p->colD, rate);
            bCol = 1;
        }
        if (p->waitD) {
            add = (int) (rate * (f32) (s8) p->waitD);
        }
    }
    if (p->waitCnt == 0) {
        EspGenWork* rec = p->rec;
        int n;
        int i;

        n = p->wait + add;
        if (n < 0) {
            p->waitCnt = 0;
        } else {
            p->waitCnt = n;
        }
        if (p->waitRnd) {
            int r = Rnd() % (p->waitRnd * 2) - p->waitRnd;

            if (p->waitCnt + r < 0) {
                p->waitCnt = 0;
            } else if (p->waitCnt + r > 255) {
                p->waitCnt = 255;
            } else {
                p->waitCnt = p->waitCnt + r;
            }
        }
        if (g_pEspSys->nEsp - g_pEspSys->ActiveEspNum < (u32) (p->num + 1)) {
            return;
        }
        {
            f32 step = 6.28f / (f32) (p->num + 1);
            f32 ang = 0.0f;

            for (i = 0; i < p->num + 1; i++) {
                cEsp* esp;
                Vec* pos = NULL;
                int ret;

                if (p->flags2 & 4) {
                    pos = &p->Offset;
                }
                if (p->Flg & 1) {
                    ret = EspSeqSet(rec, &w->info, &p->seed, p->pMod, &p->Mat, 1, ang, &esp, p->pOpt, pos);
                    ang += step;
                } else {
                    ret = EspSeqSet(rec, &w->info, &p->seed, p->pMod, &p->Mat, 0, 0.0f, &esp, p->pOpt, pos);
                }
                if (ret) {
                    if (bScale) {
                        esp->m_Size_base_x *= scaleR;
                        esp->m_Size_base_y *= scaleR;
                    }
                    if (bSpd) {
                        PSVECScale(&esp->m_Speed, &esp->m_Speed, spdR);
                    }
                    if (bCol) {
                        f32 a = (f32) (int) esp->m_Col_start_a * colR;

                        if (a > 255.0f) {
                            a = 255.0f;
                        }
                        esp->m_Col_start_a = (u8) a;
                        esp->m_Col_a *= colR;
                    }
                }
            }
        }
    } else {
        p->waitCnt--;
    }
    p->Time_cnt++;
    if (p->life != 0 && p->life <= p->Time_cnt) {
        PushEspgen(w);
    }
}

// Step 0 of Espgen00MoveTbl: first frame, then moves to step 1.
void espgen00_Move00(EspgenWork* w)
{
    espgen00_Update(w);
    w->step = 1;
}

// Step 1 of Espgen00MoveTbl: steady state, one Update per frame.
void espgen00_Move01(EspgenWork* w)
{
    espgen00_Update(w);
}

// EspgenMoveTbl entry for controller type 0: dispatches on w->step; while Status_flg[1] bit
// 0x10000000 (event pause) is set a controller whose model has be_flag 0x800 clear does not run.
void Espgen00_Move(EspgenWork* w)
{
    static void (*Espgen00MoveTbl[])(EspgenWork*) = {espgen00_Move00, espgen00_Move01};
    cModel* model = ((Espgen00Work*) w->work)->pMod;

    if (model != NULL && (StaFlagChk(pG, STA_SUSPEND))) {
        int susp = !(model->be_flag & 0x800);
        if (susp) {
            return;
        }
    }
    Espgen00MoveTbl[w->step](w);
}

// Fills the emitter from the controller record: life (Espgen_work16[0]), wait (x10C), num (x10D),
// the D curves (x124..x127), Espgen_flg, random wait range; head flag bit 0 keeps following the
// parts, `flag` == 1 passes the position on to the children; fixed seed 0x12345678+x10E when Flg
// bit 1. Copies the optional EspSeqOpt. Always returns 1.
int Espgen00_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag)
{
    Espgen00Work* p = (Espgen00Work*) w->work;

    p->rec = rec;
    p->pMod = model;
    if (model != NULL) {
        p->Guid_pMod = model->serial;
    } else {
        p->Guid_pMod = (u32) model;
    }
    p->waitCnt = p->Time_cnt = 0;
    p->life = rec->Espgen_work16[0];
    p->wait = rec->x10C;
    p->num = rec->x10D;
    p->scaleD = rec->x124;
    p->spdD = rec->x125;
    p->colD = rec->x126;
    p->waitD = rec->x127;
    p->Flg = rec->Espgen_flg;
    p->waitRnd = rec->Espgen_work8_3[0];
    if (p->waitRnd) {
        p->wait += (u32) Rnd() % p->waitRnd;
    }
    if (head->flags & 1) {
        p->flags2 |= 2;
    }
    if (flag == 1) {
        p->flags2 |= 4;
    }
    p->parts = parts;
    p->Offset = *pos;
    p->Ang = *rot;
    if (p->Flg & 2) {
        p->seed = 0x12345678 + rec->x10E;
    } else {
        p->seed = Rnd() | (Rnd() << 8) | (Rnd() << 16);
    }
    PSMTXCopy(*mtx, p->Mat);
    if (pSct != NULL) {
        p->pOpt = &p->opt;
        p->opt = *pSct;
    } else {
        p->pOpt = pSct;
    }
    return 1;
}
