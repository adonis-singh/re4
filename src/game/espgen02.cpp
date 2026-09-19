// game/espgen02: effect controller 02, the path emitter (D:/Bio4/Prog/espgen02.cpp). Like
// controller 00 but each emitted esp is placed at a (random) fraction of an effect path
// (EspGetPathAddr / PathGetPos), optionally scaled/rotated and oriented along the path tangent
// (mode bits). Entry points: Espgen02_Move, Espgen02_SetFreeWork.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "db_log.h"

struct Espgen02Work;

extern "C" {
void espgen02_UpdateMatrix(EspgenWork* w);
void espgen02_Update(EspgenWork* w);
static void espgen02_Move00(EspgenWork* w);
void espgen02_Move01(EspgenWork* w);
static f32 Calc_D256(Espgen02Work* p, u8 d, f32 rate);
}

// Effect controller 02: like controller 00 but places every emitted esp on a path (path.cpp),
// optionally oriented along it.
struct Espgen02Work {
    EspGenWork* rec;   // 0x14
    cModel* pMod;     // 0x18
    u32 Guid_pMod;        // 0x1C model serial the controller was set up with
    u16 Time_cnt;           // 0x20 frame counter
    u16 life;          // 0x24 life time (0 = infinite)
    u8 pad_24;
    u8 wait;           // 0x25 frames between emissions
    u8 Next_cnt;        // 0x26 frames left until the next emission
    u8 Set_num;            // 0x27 emissions per frame - 1
    u32 Rand_seed;          // 0x28
    u8 Espgen_flg;          // 0x2C rec->x10B: bit0 spread the angle, bit1 fixed seed
    u8 Flg;         // 0x2D bit0 parts matrix fixed, bit1 head flag, bit2 scale, bit3 pass the position on
    u8 Null_parts_no;          // 0x2E
    u8 R_inter;        // 0x2F random range added to the wait
    Mtx Mat;           // 0x30
    Vec Offset;           // 0x60
    Vec Ang;           // 0x6C
    u8 D_size;         // 0x78 rate curve parameters (Calc_D256)
    u8 D_speed;           // 0x79
    u8 D_alpha;           // 0x7A
    u8 waitD;          // 0x7B
    EspSeqOpt opt;     // 0x7C
    EspSeqOpt* pOpt;   // 0x98
    u8 pathId;         // 0x9C
    u8 PathId;         // 0x9D
    u8 Start_ratio;        // 0x9E position along the path in 1/100
    u8 Rnd_ratio;        // 0x9F random range added to it
    u16 PntNo;           // 0xA0 path segment cache
    u8 PathRot_x;           // 0xA2 rotation in 1/256 turns
    u8 PathRot_y;           // 0xA3
    Vec PathScale;         // 0xA4
    u8 mode;           // 0xB0 bit0: orient along the path, bit1: orient along the path (type 2)
};

// Rebuilds the emitter matrix from parts Null_parts_no of pMod (same rules as espgen00): 0xFE = free
// position, invalid parts numbers kill the controller.
void espgen02_UpdateMatrix(EspgenWork* w)
{
    Espgen02Work* p = (Espgen02Work*) w->work;
    cModel* model = p->pMod;

    if ((p->Null_parts_no >= 0xF8 && p->Null_parts_no <= 0xFD) || p->Null_parts_no == 0xFF) {
        pLog->err(0, 0, "ESP_CTRL : NULL_PARTS_NO[%x] invalid.", p->Null_parts_no);
        PushEspgen(w);
        return;
    }
    if (p->Null_parts_no == 0xFE) {
        return;
    }
    if (model == NULL) {
        pLog->err(0, 0, "ESP_CTRL : PARTS_NO is set but No Parent.");
        return;
    }
    if (!(p->Flg & 1)) {
        if (p->Null_parts_no < model->nParts) {
            cModel* part;
            Vec ofs;
            Vec r;

            part = model->getPartsPtr(p->Null_parts_no);
            PSMTXIdentity(p->Mat);
            PSVECAdd(&p->Ang, &model->ang, &r);
            RotMatrix(p->Mat, &r);
            PSMTXMultVecSR(p->Mat, &p->Offset, &ofs);
            p->Mat[0][3] = part->mat[0][3] + ofs.x;
            p->Mat[1][3] = part->mat[1][3] + ofs.y;
            p->Mat[2][3] = part->mat[2][3] + ofs.z;
            if (!(p->Flg & 2)) {
                p->Flg |= 1;
            }
        } else {
            pLog->err(0, 0, "ESP_CTRL : PARTS_NO[%d] is invalid(MAX:%d).", p->Null_parts_no, model->nParts);
            PushEspgen(w);
            return;
        }
    }
}

// Rate curve: d >= 0 fades 1 -> 1 - d/128 over the life, d < 0 grows 1 -> 1 + 10 * -d/128.
static f32 Calc_D256(Espgen02Work* p, u8 d, f32 rate)
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

// One emitter frame: same life/wait/curve logic as espgen00, but every emission picks a point on
// the path at Start_ratio (+random Rnd_ratio) percent of its length (weighted paths follow pMod),
// applies PathScale / PathRot_x,y (1/256 turns) and, for mode bit 0 or 1, a basis along the path
// tangent (up = +y or -y), then ApplyMatrix on the spawned esp.
// The shared 0.0f is loaded into spdR and copied to scaleR and colR. The FPR order scaleR f25 /
// colR f24 / spdR f23 is a global-alloc live-length knife edge (all three have 6 weighted refs):
// with the copies between the `bScale` and `bSpd` zero stores (the original's sched order) colR
// loses to spdR (f23/f24 swapped); colR is therefore pinned (see the tag below).
void espgen02_Update(EspgenWork* w)
{
    Espgen02Work* p = (Espgen02Work*) w->work;
    f32 scaleR;
    f32 spdR = 0.0f;
    // COMPILER-DIFF: #17. colR pinned to f24 (global-alloc order of the three 0.0f copies); no
    // code is emitted.
    register f32 colR asm("fr24");
    int bScale = 0;
    scaleR = spdR;
    colR = spdR;
    int bSpd = 0;
    int bCol = 0;
    int add = 0;
    cModel* model = p->pMod;
    Mtx sm;
    Mtx rm;

    if (model != NULL) {
        if ((model->be_flag & 0x201) != 1 || model->serial != p->Guid_pMod) {
            PushEspgen(w);
            return;
        }
    }
    espgen02_UpdateMatrix(w);
    if (p->life != 0) {
        f32 rate = (f32) p->Time_cnt / (f32) (int) p->life;

        if (p->D_size) {
            scaleR = Calc_D256(p, p->D_size, rate);
            bScale = 1;
        }
        if (p->D_speed) {
            spdR = Calc_D256(p, p->D_speed, rate);
            bSpd = 1;
        }
        if (p->D_alpha) {
            colR = Calc_D256(p, p->D_alpha, rate);
            bCol = 1;
        }
        if (p->waitD) {
            add = (int) (rate * (f32) (s8) p->waitD);
        }
    }
    if (p->Next_cnt == 0) {
        Mtx mtx;
        EspGenWork* rec;
        int n;
        int i;

        PSMTXIdentity(mtx);
        rec = p->rec;
        n = p->wait + add;
        if (n < 0) {
            p->Next_cnt = 0;
        } else {
            p->Next_cnt = n;
        }
        if (p->R_inter) {
            int r = Rnd() % (p->R_inter * 2) - p->R_inter;

            if (p->Next_cnt + r < 0) {
                p->Next_cnt = 0;
            } else if (p->Next_cnt + r > 255) {
                p->Next_cnt = 255;
            } else {
                p->Next_cnt = p->Next_cnt + r;
            }
        }
        if (g_pEspSys->nEsp - g_pEspSys->ActiveEspNum < (u32) (p->Set_num + 1)) {
            pLog->warn(0, 0, "ESP : num max. retry.[left:%d/need:%d]", g_pEspSys->nEsp - g_pEspSys->ActiveEspNum,
                       p->Set_num + 1);
            return;
        }
        {
            f32 step = 6.28f / (f32) (p->Set_num + 1);
            f32 ang = 0.0f;

            for (i = 0; i < p->Set_num + 1; i++) {
                Vec pos;
                Vec rot2;
                Mtx m3;
                Mtx m2;
                Vec pos2;
                Vec right;
                Vec up;
                Vec dir;
                Vec dir2;
                cEsp* esp;
                Vec* pp;
                void* path;
                f32 len;
                f32 t;
                f32 d;
                int ret;

                path = EspGetPathAddr(p->pathId, p->PathId);
                if (path == NULL) {
                    return;
                }
                len = PathGetLength(path);
                t = ((f32) p->Start_ratio + fRandSeed0_1(&p->Rand_seed) * (f32) (int) p->Rnd_ratio) * 0.01f;
                while (t > 1.0f) {
                    t -= 1.0f;
                }
                while (t < 0.0f) {
                    t += 1.0f;
                }
                d = len * t;
                if (d < 0.0f) {
                    d = 0.0f;
                }
                if (d > len - 1.01f) {
                    d = len - 1.01f;
                }
                if (PathHasWeight(path)) {
                    if (p->pMod != NULL) {
                        ret = PathGetPosEm(path, d, p->pMod, &p->PntNo, &pos);
                    } else {
                        ret = PathGetPos(path, d, &p->PntNo, &pos);
                    }
                } else {
                    ret = PathGetPos(path, d, &p->PntNo, &pos);
                }
                if (ret == 0) {
                    pLog->err(0, 0, "ESP_CTRL02 : OUT OF RANGE.");
                }
                PSMTXIdentity(sm);
                if (p->Flg & 4) {
                    PSMTXScale(sm, p->PathScale.x, p->PathScale.y, p->PathScale.z);
                }
                rot2.x = (f32) p->PathRot_x * 3.1415927f * 2.0f * 0.00390625f;
                rot2.y = (f32) p->PathRot_y * 3.1415927f * 2.0f * 0.00390625f;
                rot2.z = 0.0f;
                RotMatrix(rm, &rot2);
                if (p->mode & 1) {
                    d += 1.0f;
                    if (PathHasWeight(path) && p->pMod != NULL) {
                        PathGetPosEm(path, d, p->pMod, &p->PntNo, &pos2);
                    } else {
                        PathGetPos(path, d, &p->PntNo, &pos2);
                    }
                    PSVECSubtract(&pos2, &pos, &dir);
                    up.x = 0.0f;
                    up.y = 1.0f;
                    up.z = 0.0f;
#line 284 "D:/Bio4/Prog/espgen02.cpp"
                    VECNormalize(&dir, &dir);
                    PSVECCrossProduct(&dir, &up, &right);
                    PSVECCrossProduct(&right, &dir, &up);
                    m2[0][0] = right.x;
                    m2[0][1] = right.y;
                    m2[0][2] = right.z;
                    m2[0][3] = 0.0f;
                    m2[1][0] = up.x;
                    m2[1][1] = up.y;
                    m2[1][2] = up.z;
                    m2[1][3] = 0.0f;
                    m2[2][0] = dir.x;
                    m2[2][1] = dir.y;
                    m2[2][2] = dir.z;
                    m2[2][3] = 0.0f;
                    PSMTXMultVec(sm, &pos, &pos);
                    PSMTXMultVec(rm, &pos, &pos);
                    PSMTXCopy(p->Mat, m3);
                    m3[0][3] -= rec->Pos.x + p->Mat[0][3];
                    m3[1][3] -= rec->Pos.y + p->Mat[1][3];
                    m3[2][3] -= rec->Pos.z + p->Mat[2][3];
                    PSMTXConcat(m2, m3, m3);
                    PSMTXConcat(rm, m3, m3);
                    m3[0][3] += rec->Pos.x + p->Mat[0][3];
                    m3[1][3] += rec->Pos.y + p->Mat[1][3];
                    m3[2][3] += rec->Pos.z + p->Mat[2][3];
                    m3[0][3] += pos.x;
                    m3[1][3] += pos.y;
                    m3[2][3] += pos.z;
                } else if (p->mode & 2) {
                    if (DbgFlagChk(pG, DBG_ESP_CHK)) {
                        pLog->warn(0, 0, "ESP : USE CTRL_PATH TYPE=2");
                    }
                    d += 1.0f;
                    if (PathHasWeight(path) && p->pMod != NULL) {
                        PathGetPosEm(path, d, p->pMod, &p->PntNo, &pos2);
                    } else {
                        PathGetPos(path, d, &p->PntNo, &pos2);
                    }
                    PSVECSubtract(&pos2, &pos, &dir2);
                    up.x = 0.0f;
                    up.y = -1.0f;
                    up.z = 0.0f;
#line 359 "D:/Bio4/Prog/espgen02.cpp"
                    VECNormalize(&dir2, &dir2);
                    PSVECCrossProduct(&up, &dir2, &right);
                    PSVECCrossProduct(&dir2, &right, &up);
                    m2[0][0] = right.x;
                    m2[0][1] = right.y;
                    m2[0][2] = right.z;
                    m2[0][3] = 0.0f;
                    m2[1][0] = up.x;
                    m2[1][1] = up.y;
                    m2[1][2] = up.z;
                    m2[1][3] = 0.0f;
                    m2[2][0] = dir2.x;
                    m2[2][1] = dir2.y;
                    m2[2][2] = dir2.z;
                    m2[2][3] = 0.0f;
                    PSMTXMultVec(sm, &pos, &pos);
                    PSMTXMultVec(rm, &pos, &pos);
                    PSMTXCopy(p->Mat, m3);
                    m3[0][3] -= rec->Pos.x + p->Mat[0][3];
                    m3[1][3] -= rec->Pos.y + p->Mat[1][3];
                    m3[2][3] -= rec->Pos.z + p->Mat[2][3];
                    PSMTXConcat(m2, m3, m3);
                    PSMTXConcat(rm, m3, m3);
                    m3[0][3] += rec->Pos.x + p->Mat[0][3];
                    m3[1][3] += rec->Pos.y + p->Mat[1][3];
                    m3[2][3] += rec->Pos.z + p->Mat[2][3];
                    m3[0][3] += pos.x;
                    m3[1][3] += pos.y;
                    m3[2][3] += pos.z;
                } else {
                    PSMTXMultVec(sm, &pos, &pos);
                    PSMTXMultVec(rm, &pos, &pos);
                    PSMTXCopy(p->Mat, m3);
                    m3[0][3] += pos.x;
                    m3[1][3] += pos.y;
                    m3[2][3] += pos.z;
                }
                pp = NULL;
                if (p->Flg & 8) {
                    pp = &p->Offset;
                }
                if (p->Espgen_flg & 1) {
                    ret = EspSeqSet(rec, &w->info, &p->Rand_seed, p->pMod, &mtx, 1, ang, &esp, p->pOpt, pp);
                    ang += step;
                } else {
                    ret = EspSeqSet(rec, &w->info, &p->Rand_seed, p->pMod, &mtx, 0, 0.0f, &esp, p->pOpt, pp);
                }
                if (ret) {
                    esp->ApplyMatrix(m3);
                    if (bScale) {
                        esp->m_Size_base_x *= scaleR;
                        esp->m_Size_base_y *= scaleR;
                    }
                    if (bSpd) {
                        PSVECScale(&esp->m_Speed, &esp->m_Speed, spdR);
                    }
                    if (bCol) {
                        esp->m_Col_start_a = (u8) ((f32) (int) esp->m_Col_start_a * colR);
                        esp->m_Col_a *= colR;
                    }
                }
            }
        }
    } else {
        p->Next_cnt--;
    }
    p->Time_cnt++;
    if (p->life != 0 && p->life <= p->Time_cnt) {
        PushEspgen(w);
    }
}

// Step 0 of Espgen02MoveTbl: first frame, then step 1.
static void espgen02_Move00(EspgenWork* w)
{
    espgen02_Update(w);
    w->step = 1;
}

// Step 1 of Espgen02MoveTbl: steady state.
void espgen02_Move01(EspgenWork* w)
{
    espgen02_Update(w);
}

// EspgenMoveTbl entry for controller type 2: dispatches on w->step.
void Espgen02_Move(EspgenWork* w)
{
    static void (*Espgen02MoveTbl[])(EspgenWork*) = {espgen02_Move00, espgen02_Move01};

    Espgen02MoveTbl[w->step](w);
}

// Fills the path emitter from the record: the espgen00 fields plus path group/id
// (Espgen_work8_4[0..1]), Start_ratio/Rnd_ratio (percent), PathRot_x/y and mode (Espgen_work8_3[1..3]),
// and PathScale = 1 + Espgen_vec0/10 when non-zero. Always returns 1.
int Espgen02_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag)
{
    Espgen02Work* p = (Espgen02Work*) w->work;

    p->rec = rec;
    p->pMod = model;
    if (model != NULL) {
        p->Guid_pMod = model->serial;
    } else {
        p->Guid_pMod = (u32) model;
    }
    p->Next_cnt = p->Time_cnt = 0;
    p->life = rec->Espgen_work16[0];
    p->wait = rec->x10C;
    p->Set_num = rec->x10D;
    p->D_size = rec->x124;
    p->D_speed = rec->x125;
    p->D_alpha = rec->x126;
    p->waitD = rec->x127;
    p->Espgen_flg = rec->Espgen_flg;
    p->R_inter = rec->Espgen_work8_3[0];
    if (p->R_inter) {
        p->wait += (u32) Rnd() % p->R_inter;
    }
    if (head->flags & 1) {
        p->Flg |= 2;
    }
    if (flag == 1) {
        p->Flg |= 8;
    }
    p->Null_parts_no = parts;
    p->Offset = *pos;
    p->Ang = *rot;
    if (p->Espgen_flg & 2) {
        p->Rand_seed = 0x12345678 + rec->x10E;
    } else {
        p->Rand_seed = Rnd() | (Rnd() << 8) | (Rnd() << 16);
    }
    PSMTXCopy(*mtx, p->Mat);
    if (pSct != NULL) {
        p->pOpt = &p->opt;
        p->opt = *pSct;
    } else {
        p->pOpt = pSct;
    }
    p->pathId = rec->Espgen_work8_4[0];
    p->PathId = rec->Espgen_work8_4[1];
    p->Start_ratio = rec->Espgen_work8_4[2];
    p->Rnd_ratio = rec->Espgen_work8_4[3];
    p->PathRot_x = rec->Espgen_work8_3[1];
    p->PathRot_y = rec->Espgen_work8_3[2];
    p->mode = rec->Espgen_work8_3[3];
    if (rec->Espgen_vec0.x != 0.0f || rec->Espgen_vec0.y != 0.0f || rec->Espgen_vec0.z != 0.0f) {
        p->Flg |= 4;
        p->PathScale = rec->Espgen_vec0;
        PSVECScale(&p->PathScale, &p->PathScale, 0.1f);
        p->PathScale.x += 1.0f;
        p->PathScale.y += 1.0f;
        p->PathScale.z += 1.0f;
    }
    return 1;
}
