// game/espgen01: effect controller 01, lens flare (D:/Bio4/Prog/espgen01.cpp). Projects a light
// position (a parts of a model or a fixed point) to the screen and, while it is in front of the
// camera, respawns the est table's sprites every frame along the line to the screen centre,
// with alpha from screen distance, view direction cone, camera distance and a Z-buffer
// occlusion sample (HideCheck, run after the render). Entry points: Espgen01_Move,
// Espgen01_Trans, Espgen01_SetFreeWork.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "main_sub.h"
#include "view.h"
#include "cam_ctrl.h"
#include "db_log.h"

// Effect controller 01: lens flare. Projects the light position to the screen and lays the
// est table sprites along the line to the screen centre; HideCheck samples the Z buffer.
struct Espgen01Work {
    Vec offset;           // 0x14 light offset (from the parts)
    Vec pos;           // 0x20 world position
    Vec dir_vec;           // 0x2C light direction
    f32 dir_ang;           // 0x38 half of the visible cone angle
    f32 center_dist_ratio;      // 0x3C
    f32 size_ratio;     // 0x40
    f32 del_dist;          // 0x44 fade distance
    u8 pad_48[4];
    u16 Null_parts_no;         // 0x4C
    u8 pad_4E[2];
    cModel* pMod;     // 0x50
    u8 parts_no;        // 0x54
    u8 pad_55;
    u16 flg;         // 0x56 bit0: directional, bit1: hide check
    f32 pos_x;            // 0x58 screen position
    f32 pos_y;            // 0x5C
    f32 hide_alpha;     // 0x60
    f32 hide_r;    // 0x64
    u8 owner;          // 0x68
    u8 est_id;          // 0x69
    u16 delay_cnt;        // 0x6A frames to hide after a camera change
    u32 Rand_seed;          // 0x6C
};

extern "C" {
void espgen01_Move00(EspgenWork* w);
void espgen01_Move01(EspgenWork* w);
void SetEsp(EspgenWork* w);
u32 GetEstTblnum(EspSeqData* head);
cEsp* SetEstTbl(EspgenWork* w, EspSeqData* head, int no);
static f32 GetDistAlpha(EspgenWork* w);
static f32 GetDirAlpha(EspgenWork* w, Vec* dir);
void HideCheck(cEsp* esp);
}

// Step 0 of Espgen01MoveTbl: first frame, then step 1.
void espgen01_Move00(EspgenWork* w)
{
    SetEsp(w);
    w->step = 1;
}

// Step 1 of Espgen01MoveTbl: steady state.
void espgen01_Move01(EspgenWork* w)
{
    SetEsp(w);
}

// EspgenMoveTbl entry for controller type 1: dispatches on w->step.
void Espgen01_Move(EspgenWork* pEspgen)
{
    static void (*Espgen01MoveTbl[])(EspgenWork*) = {espgen01_Move00, espgen01_Move01};

    Espgen01MoveTbl[pEspgen->step](pEspgen);
}

// EspgenTransTbl entry: queues HideCheck to run after the scene render (needs the final Z buffer)
// for a live controller.
void Espgen01_Trans(EspgenWork* pEspgen)
{
    if ((pEspgen->flag & 1) && !(pEspgen->flag & 2)) {
        EspAddOtAfterRender((cEsp*) pEspgen, HideCheck);
    }
}

// Per-frame flare: computes the light's world position (parts matrix * offset) and its screen
// position; when it is in front of the camera, alpha = 1 - (dist to screen centre / (height *
// center_dist_ratio*0.7))^2, times GetDirAlpha (flg bit 0), GetDistAlpha and hide_alpha (flg bit 1), and if
// > 0.01 spawns every est table record (one-frame sprites, screen-space parts 0xF8) spaced along
// the centre line by their record x offset, scaled by alpha*size_ratio.
void SetEsp(EspgenWork* pGen)
{
    Espgen01Work* p = (Espgen01Work*) pGen->work;
    Vec v;
    Vec scr;
    Vec d;
    Vec dir;
    Mtx m;
    EspSeqData* head;
    u32 num;
    u32 i;
    cEsp* esp;
    f32 alpha;
    f32 x0;
    f32 s;

    head = EspGetEstAddr(p->owner, p->est_id, 0);
    if (head == NULL) {
        pLog->err(0, 0, "ESP_FLARE : OWNER[%d] EST_ID[%d] invalid", p->owner, p->est_id);
        PushEspgen(pGen);
        return;
    }
    num = GetEstTblnum(head);
    if (p->pMod == NULL || p->parts_no == 0xFE) {
        p->pos = p->offset;
        dir = p->dir_vec;
    } else {
        cParts* part;

        if (p->parts_no >= p->pMod->nParts) {
            pLog->err(0, 0, "ESP_FLARE :PARTS_NO[%d] is invalid(MAX:%d).", p->parts_no, p->pMod->nParts);
            PushEspgen(pGen);
            return;
        }
        part = p->pMod->getPartsPtr(p->parts_no);
        PSMTXMultVec(part->mat, &p->offset, &p->pos);
        PSMTXCopy(part->mat, m);
        m[0][3] = 0.0f;
        m[1][3] = 0.0f;
        m[2][3] = 0.0f;
        PSMTXMultVec(m, &p->dir_vec, &dir);
    }
    PSMTXMultVec(pG->Camera.v_mat, &p->pos, &v);
    PSMTX44MultVec(pG->Camera.ProjMat, &v, &scr);
    scr.x = (scr.x * 0.5f + 0.5f) * Screen.width;
    scr.y = (-scr.y * 0.5f + 0.5f) * Screen.height;
    scr.z = 0.0f;
    p->pos_x = scr.x;
    p->pos_y = scr.y;
    if (v.z < 0.0f) {
        v.x = Screen.width * 0.5f;
        v.y = Screen.height * 0.5f;
        v.z = 0.0f;
        PSVECSubtract(&v, &scr, &d);
        alpha = PSVECMag(&d) / (Screen.height * (p->center_dist_ratio * 0.7f));
        alpha *= alpha;
        alpha = 1.0f - alpha;
        if (p->flg & 1) {
            alpha *= GetDirAlpha(pGen, &dir);
        }
        alpha *= GetDistAlpha(pGen);
        if (p->flg & 2) {
            alpha *= p->hide_alpha;
        }
        if (alpha > 0.01f) {
            esp = SetEstTbl(pGen, head, 0);
            if (esp == EspGetDmyPtr()) {
                PushEspgen(pGen);
                return;
            }
            esp->m_Parts_no = ESP_PARTS_NOPARTS;
            esp->m_Life_max = 1;
            x0 = esp->m_Pos.x;
            esp->m_Pos.x = p->pos_x;
            esp->m_Pos.y = p->pos_y;
            esp->m_Col_a *= alpha;
            if (p->size_ratio != 0.0f) {
                s = alpha * p->size_ratio + (1.0f - p->size_ratio);
                if (s < 0.0f) {
                    s = 0.0f;
                }
                esp->m_Size_base_x *= s;
                esp->m_Size_base_y *= s;
            }
            for (i = 1; i < num; i++) {
                esp = SetEstTbl(pGen, head, i);
                if (esp == EspGetDmyPtr()) {
                    PushEspgen(pGen);
                    return;
                }
                PSVECScale(&d, &v, (esp->m_Pos.x - x0) / (Screen.width * 0.5f - x0));
                esp->m_Parts_no = ESP_PARTS_NOPARTS;
                esp->m_Life_max = 1;
                esp->m_Pos.x = p->pos_x;
                esp->m_Pos.y = p->pos_y;
                esp->m_Col_a *= alpha;
                if (p->size_ratio != 0.0f) {
                    s = alpha * p->size_ratio + (1.0f - p->size_ratio);
                    if (s < 0.0f) {
                        s = 0.0f;
                    }
                    esp->m_Size_base_x *= s;
                    esp->m_Size_base_y *= s;
                }
                PSVECAdd(&v, &esp->m_Pos, &esp->m_Pos);
            }
        }
    }
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
}

// Number of records in the flare's est table.
u32 GetEstTblnum(EspSeqData* head)
{
    return head->num;
}

// Spawns record `no` of the est table with an identity matrix; returns the new esp (the dummy esp
// when the pool is full).
cEsp* SetEstTbl(EspgenWork* w, EspSeqData* head, int no)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    EspGenWork* rec = head->rec;
    Mtx m;
    cEsp* esp;

    rec = &rec[no];
    PSMTXIdentity(m);
    EspSeqSet(rec, &w->info, &p->Rand_seed, p->pMod, &m, 0, 0.0f, &esp, NULL, NULL);
    return esp;
}

// Alpha factor from the camera distance: 1 at the light fading to 0 at `dist` (1 when dist == 0).
static f32 GetDistAlpha(EspgenWork* w)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    CAMERA* cam;
    Vec d;
    f32 a;

    if (p->del_dist != 0.0f) {
        cam = &pG->Camera;
        d.x = p->pos.x - cam->param.pos.x;
        d.y = p->pos.y - cam->param.pos.y;
        d.z = p->pos.z - cam->param.pos.z;
        a = PSVECMag(&d) / p->del_dist;
        if (a > 1.0f) {
            a = 1.0f;
        }
        if (a < 0.0f) {
            a = 0.0f;
        }
        return 1.0f - a;
    }
    return 1.0f;
}

// Alpha factor from the light direction: 1 when the camera is on the light axis, 0 outside the cone
// of half-angle dir_ang (radians), linear in the cosine in between.
static f32 GetDirAlpha(EspgenWork* w, Vec* dir)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    CAMERA* cam;
    Vec d;
    f32 ang;
    f32 a;
    f32 c;

    ang = LIMIT_ANGLE(p->dir_ang);
    cam = &pG->Camera;
    d.x = p->pos.x - cam->param.pos.x;
    d.y = p->pos.y - cam->param.pos.y;
    d.z = p->pos.z - cam->param.pos.z;
#line 339 "D:/Bio4/Prog/espgen01.cpp"
    VECNormalize(&d, &d);
    a = -PSVECDotProduct(&d, dir);
    c = cosf(ang);
    a = a - c;
    if (a <= 0.0f) {
        a = 0.0f;
    } else {
        a = a / (1.0f - c);
    }
    return a;
}

// After-render Z test for the flare: peeks the Z buffer at 12 points on a circle of radius hide_r
// around the screen position (off-screen points count as hidden; border 56 px in the widescreen
// System_flg 0x800 mode) and eases hide_alpha towards 1 - hidden/10 (0 when all 12 are hidden).
// A camera change forces hide_alpha to 0 for 2 frames.
void HideCheck(cEsp* pDat)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static int Zs_bias = -5000;
    static f32 hide_x_tbl[12] = {0.0f, 0.5f, 0.86f, 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f};
    static f32 hide_y_tbl[12] = {1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f, 0.0f, 0.5f, 0.86f};
    EspgenWork* w = (EspgenWork*) pDat;
    Espgen01Work* p = (Espgen01Work*) w->work;
    Vec v;
    Vec s;
    u32 z;
    int zval;
    u32 cnt;
    u32 i;
    f32 border;
    f32 a;
    f32 tmp;
    f32 m22;
    f32 m23;
    f32 iw;

    if (!(p->flg & 2)) {
        return;
    }
    PSMTXMultVec(pG->Camera.v_mat, &p->pos, &v);
    v.z += 150.0f;
    tmp = 1.0f / (ZFAR - ZNEAR);
    m22 = -(ZNEAR) * tmp;
    m23 = -(ZFAR * ZNEAR) * tmp;
    iw = 1.0f / -v.z;
    m22 = m22 * v.z;
    zval = (u32) ((iw * ((m22 + m23) * Zscale) + Zoffset) * 16777215.0f);
    if (SysFlagChk(pG, SYS_SCISSOR_ON)) {
        border = 56.0f;
    } else {
        border = 0.0f;
    }
    GXPixModeSync();
    GXDrawDone();
    cnt = 0;
    for (i = 0; i < 12; i++) {
        s.x = hide_x_tbl[i] * p->hide_r + p->pos_x;
        s.y = hide_y_tbl[i] * p->hide_r + p->pos_y;
        if (s.x < 0.0f || s.x >= Screen.width || s.y < border + 0.0f || s.y >= Screen.height - border) {
            cnt++;
        } else {
            GXPeekZ((u16) s.x, (u16) s.y, &z);
            if (zval > (int) (z - Zs_bias)) {
                cnt++;
            }
        }
    }
    if (cnt == 12) {
        p->hide_alpha = 0.0f;
    } else {
        a = 1.0f - (f32) cnt * 0.1f;
        if (a < 0.0f) {
            a = 0.0f;
        }
        if (a > 1.0f) {
            a = 1.0f;
        }
        p->hide_alpha = (a - p->hide_alpha) * 0.6f + p->hide_alpha;
    }
    if (CamCtrl.IsChangeCamera()) {
        p->delay_cnt = 2;
    }
    if (p->delay_cnt != 0) {
        p->delay_cnt--;
        p->hide_alpha = 0.0f;
    }
}

// Fills the flare from the record: offset = Pos, est owner/id = Work8[0..1], parts = Parts_no;
// Vec2 = (rot x deg, rot y deg, cone fov deg) enables the direction test; Vec0 = (size %, scale %,
// fade distance); Vec1.x != 0 is the hide-check radius in pixels.
int Espgen01_SetFreeWork(EspgenWork* w, EspGenWork* rec, EspSeqData* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, EspSeqOpt* pSct, int flag)
{
    Espgen01Work* p = (Espgen01Work*) w->work;
    Mtx m1;
    Mtx m2;
    f32 rx;
    f32 ry;
    f32 fov;

    p->offset = *(Vec*) &rec->Pos.x;
    p->owner = rec->Work8[0];
    p->est_id = rec->Work8[1];
    p->flg = 0;
    p->Null_parts_no = parts;
    p->pMod = model;
    p->parts_no = rec->Parts_no;
    fov = rec->Vec2.z;
    if (fov != 0.0f) {
        p->dir_ang = fov * 6.2831855f / 360.0f * 0.5f;
        p->dir_vec.x = 0.0f;
        p->dir_vec.z = 1.0f;
        p->dir_vec.y = 0.0f;
        rx = rec->Vec2.x * 6.2831855f / 360.0f;
        ry = rec->Vec2.y * 6.2831855f / 360.0f;
        rx = LIMIT_ANGLE(rx);
        ry = LIMIT_ANGLE(ry);
        PSMTXRotRad(m1, 'Y', ry);
        PSMTXRotRad(m2, 'X', rx);
        PSMTXConcat(m1, m2, m1);
        PSMTXMultVec(m1, &p->dir_vec, &p->dir_vec);
#line 482 "D:/Bio4/Prog/espgen01.cpp"
        VECNormalize(&p->dir_vec, &p->dir_vec);
        p->flg |= 1;
    }
    p->center_dist_ratio = 1.0f - rec->Vec0.x * 0.01f;
    if (p->center_dist_ratio > 1.0f) {
        p->center_dist_ratio = 1.0f;
    }
    p->size_ratio = rec->Vec0.y * 0.01f;
    p->del_dist = rec->Vec0.z;
    if (rec->Vec1.x != 0.0f) {
        p->hide_r = rec->Vec1.x;
        p->flg |= 2;
    }
    return 1;
}

asm(".section .sdata; .balign 8");
