// game/db_cam.cpp: the debug camera tool (CamDbg), driven from CameraMove with pad 1. Any input
// takes the camera away from the game (Debug_flg[0] 0x10000000, B gives it back); two control
// layouts orbit / dolly / zoom it, A snaps the target onto the selected enemy / object / player,
// Z opens a menu with pages for debug flags, camera cut playback, hit display switches and the
// shoulder camera offset editor (adjust_qFPS, shared with the t_camera tool).

#include "types.h"
#include "vec.h"
#include "atari.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_qfps.h"
#include "joy.h"
#include "eprintf.h"
#include "db_log.h"
#include "math_sub.h"
#include "main_mem.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "player.h"
#include "db_cam.h"
#include "motion.h"
#include <string.h>
#include "dbmodule.h"
#include "view.h"


// Orthographic zoom: the top/left extents move together (plain block: a do-while's loop notes
// flip the f0/f13 allocation of the two chains).
#define ORTHO_ZOOM(t)                    \
    {                                    \
        ORTHO_T += (t) * 0.75f;          \
        ORTHO_L -= (t);                  \
    }


debugCamera CamDbg;
QfpsOfs g_local_ready[2][3];
QfpsOfs g_local_trans[2][3];
f32 g_local_floor_ratio;
f32 g_local_fovy[2];

const char* key_str[4] = {"DFLT", "SCR", "????", "????"};

static inline void Dec(int& v) { v--; }

// adjust_qFPS keeps the edited shoulder offset record as a byte pointer (the original copies it
// with memcpy and steps through it by byte offset).
#define QOFS(p) ((QfpsOfs*) (p))
#define QOFS_CAMPOS2 0xC

// Per-frame: Z toggles the menu (pauses the debug page), the menu page runs when open; otherwise
// input claims the camera (Debug_flg[0] 0x10000000, B releases unless `flag` bit0), the target
// type (EM / OBJ / PL / ORG) with A snaps the look-at to the selected work (Left / Right pick it,
// R + A steps its motion), the layout's control routine runs, and the camera info / target cross
// are drawn.
void debugCamera::move(Camera* pCam, JOY* pJoy, int attr)
{
    static void (debugCamera::*camera_type_tbl[4])(Camera*, JOY*) = {
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_01,
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_00,
    };
    static int numEm = 0;
    static int numObj = 0;

    m_timer--;
    if (m_timer & 0x80) {
        m_timer = 0;
        if (pJoy->trg & JOY_Z) {
            if (m_menu_sw == 0) {
                m_timer = 5;
                m_menu_sw = 1;
                m_printNo_bak = pG->debug_mode;
                pG->debug_mode = 1;
                m_cam_play = 0;
                adjust_qFPS(NULL, 0, 0, 1, NULL);
            } else {
                m_menu_sw = 0;
                pG->debug_mode = m_printNo_bak;
            }
        }
    }
    switch (m_menu_sw) {
    case 0:
        break;
    case 1:
        menu(pCam, pJoy);
        return;
    }
    if (pJoy->on & ~0x1A00) {
        if (DbgFlagChk(pG, DBG_TEST_MODE)) {
            m_draw_timer = 5;
        } else {
            m_draw_timer = 30;
        }
    }
    if (m_draw_timer) {
        m_draw_timer--;
        CameraDrawTarget(pCam, 0);
    }
    if (!DbgFlagChk(pG, DBG_DBG_CAM)) {
        if (pJoy->on & ~0x1A00) {
            DbgFlagOn(pG, DBG_DBG_CAM);
        }
    } else {
        if (!DbgFlagChk(pG, DBG_TEST_MODE) && (pG->Frame_cnt & 0x10)) {
            eprintf(160, 406, 4, 0, "DEBUG CAMERA --- [%s]", key_str[m_key_type]);
        }
        if (m_menu_sw == 0 && !(attr & 1) && (pJoy->on & JOY_B)) {
            DbgFlagOff(pG, DBG_DBG_CAM);
        }
    }
    switch (m_target_type) {
    case 0: {
        cEm* em;
        if (pJoy->trg & JOY_A) {
            int i;
            cEm* e;
            int num = EmMgr.getArrayNum();
            i = num;
            numEm++;
            if (num <= numEm) {
                numEm = 0;
            }
            while (--i) {
                e = EmMgr.at(numEm);
                if ((e->be_flag & 1) && e->id <= 0x3F) {
                    break;
                }
                numEm++;
                if (num <= numEm) {
                    numEm = 0;
                }
            }
        }
        if (pJoy->on & JOY_A) {
            if (EmMgr.at(numEm)->be_flag & 1) {
                cParts* parts = EmMgr.at(numEm)->getPartsPtr(0);
                if (parts == NULL) {
                    pCam->param.at = EmMgr.at(numEm)->pos;
                } else {
                    pCam->param.at = parts->world;
                }
            } else {
                pCam->param.at.x = 0.0f;
                pCam->param.at.y = 0.0f;
                pCam->param.at.z = 0.0f;
            }
            CameraSetOrientationZeroRoll(pCam);
        }
        em = EmMgr.at(numEm);
        if (em != NULL) {
            if ((em->be_flag & 1) && em != (cEm*) pPL) {
                int col = 0;
                int dead = em->dmg.m_Flag || em->dmg.m_Timer;
                if (dead || em->hp <= 0) {
                    col = 2;
                }
                eprintf2(8, 14, 32, 358, col, 7, "Id=%02x, be=%08x, type=%02x, set=%02x, List=%02d", em->id,
                         em->be_flag, em->type, em->set, em->emset_no);
                eprintf2(8, 14, 32, 372, col, 7, "POS[%.2f, %.2f, %.2f], Dir[%.2f]", em->pos.x, em->pos.y,
                         em->pos.z, em->ang.y);
                eprintf2(8, 14, 32, 386, col, 7, "RNO[%02x][%02x][%02x][%02x], HP[ %d], FRAME[%d/%d] ", em->r_no_0,
                         em->r_no_1, em->r_no_2, em->r_no_3, em->hp, (u32) em->Motion.Seq_frame, em->Motion.Seq_frame_num);
                eprintf2(8, 14, 32, 344, col, 7, "Flag=[%08x], L_pl[%.2f]", em->flag, SQRTF(em->l_pl));
                if (em->checkStatus(EM_STATUS_LOCKOFF)) {
                    eprintf2(10, 16, 400, 344, col, 7, "LOCKOFF");
                }
            }
        }
        if (em != NULL && (em->be_flag & 1) && (DbgFlagChk(pG, DBG_SKELETON_DISP))) {
            em->debugSkeletonDisp();
        }
        break;
    }
    case 1: {
        if (pJoy->trg & JOY_A) {
            int i;
            int num = ObjMgr.getArrayNum();
            cObj* obj;
            i = num;
            numObj++;
            if (num <= numObj) {
                numObj = 0;
            }
            for (;;) {
                obj = ObjMgrWork(numObj);
                if (!(obj->be_flag & 1)) {
                    if (--i == 0) {
                        break;
                    }
                    numObj++;
                    if (num <= numObj) {
                        numObj = 0;
                    }
                } else {
                    break;
                }
            }
        }
        if (pJoy->on & JOY_A) {
            if (ObjMgrWork(numObj)->be_flag & 1) {
                cParts* parts = ObjMgrWork(numObj)->getPartsPtr(0);
                if (parts == NULL) {
                    pCam->param.at = ObjMgrWork(numObj)->pos;
                } else {
                    pCam->param.at = parts->world;
                }
            } else {
                pCam->param.at.x = 0.0f;
                pCam->param.at.y = 0.0f;
                pCam->param.at.z = 0.0f;
            }
            CameraSetOrientationZeroRoll(pCam);
        }
        break;
    }
    case 2:
        if (pJoy->on & JOY_A) {
            if (pPL->be_flag & 1) {
                cParts* parts = pPL->getPartsPtr(0);
                if (parts == NULL) {
                    pCam->param.at = pPL->pos;
                } else {
                    pCam->param.at = parts->world;
                }
            } else {
                pCam->param.at.x = 0.0f;
                pCam->param.at.y = 0.0f;
                pCam->param.at.z = 0.0f;
            }
            CameraSetOrientationZeroRoll(pCam);
        }
        break;
    case 3:
        if (pJoy->on & JOY_A) {
            pCam->param.at.x = 0.0f;
            pCam->param.at.y = 0.0f;
            pCam->param.at.z = 0.0f;
            CameraSetOrientationZeroRoll(pCam);
        }
        break;
    case 4:
        break;
    }
    if (m_cam_mode == 5) {
        (this->*camera_type_tbl[1])(pCam, pJoy);
    } else {
        pCam->Distance = PSVECDistance(&pCam->param.pos, &pCam->param.at);
        (this->*camera_type_tbl[m_key_type])(pCam, pJoy);
    }
    if (DbgFlagChk(pG, DBG_DBG_CAM) && info_disp) {
        Mtx inv;
        Vec pos;
        Vec at;
        PSMTXInverse(pPL->mat, inv);
        PSMTXMultVec(inv, &pG->Camera.param.pos, &pos);
        PSMTXMultVec(inv, &pG->Camera.param.at, &at);
        eprintf(72, 420, 5, 0, "CAMPOS @pPl->mat: (%5.1f, %5.1f, %5.1f)", pos.x, pos.y, pos.z);
        eprintf(72, 434, 5, 0, "TARGET @pPl->mat: (%5.1f, %5.1f, %5.1f)", at.x, at.y, at.z);
    }
}

// Layout 0: L / R zoom (distance, or the ortho extents), main stick orbits the target, D-pad
// (+X) dollies forward / back / up / down along the camera or world axes, C-stick turns the
// camera in place. All scaled by m_move_gain.
void debugCamera::camera_type_00(Camera* pCam, JOY* pJoy)
{
    Vec mv = {0.0f, 0.0f, 0.0f};
    f32 spd = 100.0f;
    f32 d = pCam->Distance / 1000.0f;

    if (pJoy->on & (JOY_R | JOY_L)) {
        f32 t;
        if (pJoy->on & JOY_R) {
            f32 r = pJoy->triggerRight / 150.0f;
            t = r * -1000.0f * r * r;
        } else {
            f32 r = pJoy->triggerLeft / 150.0f;
            t = r * 1000.0f * r * r;
        }
        t *= (d / 10.0f + 1.0f) * 0.5f;
        t *= m_move_gain;
        switch (ProjType) {
        case 1: {
            f32 dist = pCam->Distance + t;
            if (dist < 100.0f) {
                mv.z = 0.0f;
            }
            CameraCamposDistance(pCam, dist);
            break;
        }
        case 2:
            ORTHO_ZOOM(t);
            ORTHO_B = -ORTHO_T;
            ORTHO_R = -ORTHO_L;
            break;
        }
    }
    if (pJoy->stickX) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(pCam, &axis, &pCam->param.at, m_move_gain * (f32) pJoy->stickX * 0.05f * DEG2RAD);
    }
    if (pJoy->stickY) {
        CameraCamposRot(pCam, 'x', m_move_gain * (f32) pJoy->stickY * -0.05f * DEG2RAD);
    }
    if (pJoy->on & JOY_LEFT) {
        mv.x = -100.0f;
    }
    if (pJoy->on & JOY_RIGHT) {
        mv.x = 100.0f;
    }
    if ((pJoy->on & (JOY_X | JOY_UP)) == (JOY_X | JOY_UP)) {
        mv.y = 100.0f;
    }
    if ((pJoy->on & (JOY_X | JOY_DOWN)) == (JOY_X | JOY_DOWN)) {
        mv.y = -100.0f;
    }
    if ((pJoy->on & (JOY_X | JOY_UP)) == JOY_UP) {
        mv.z = -100.0f;
    }
    if ((pJoy->on & (JOY_X | JOY_DOWN)) == JOY_DOWN) {
        mv.z = 100.0f;
    }
    PSVECScale(&mv, &mv, (d / 10.0f + 1.0f) * 2.0f);
    PSVECScale(&mv, &mv, m_move_gain);
    if (mv.x != 0.0f || mv.y != 0.0f || mv.z != 0.0f) {
        Vec axis;
        Mtx m;
        Vec up = {0.0f, 1.0f, 0.0f};
        Vec dir;
        Vec trans;
        if (!along_xyz) {
            dir.x = pCam->mat[0][2];
            dir.y = pCam->mat[1][2];
            dir.z = pCam->mat[2][2];
            if (dir.x == 0.0f && dir.z == 0.0f) {
                dir.x = pCam->mat[0][1];
                dir.y = pCam->mat[1][1];
                dir.z = pCam->mat[2][1];
            }
            dir.y = 0.0f;
#line 452 "D:/Bio4/Prog/db_cam.cpp"
            VECNormalize(&dir, &dir);
            PSVECCrossProduct(&up, &dir, &axis);
            MTX_SET_COLUMNS(m, &axis, &up, &dir, &trans);
        } else {
            PSMTXIdentity(m);
        }
        PSMTXMultVecSR(m, &mv, &mv);
        CameraDolly(pCam, &mv);
    }
    if (pJoy->substickX) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(pCam, &axis, &pCam->param.pos, m_move_gain * (f32) -pJoy->substickX * 0.05f * DEG2RAD);
    }
    if (pJoy->substickY) {
        CameraTargetRot(pCam, 'x', m_move_gain * (f32) -pJoy->substickY * -0.05f * DEG2RAD);
    }
}

// Layout 1: L / R zoom, C-stick dollies sideways / up, main stick orbits the target.
void debugCamera::camera_type_01(Camera* pCam, JOY* pJoy)
{
    Vec mv = {0.0f, 0.0f, 0.0f};
    f32 dist_min = 1500.0f;
    f32 d = pCam->Distance / 1000.0f;

    if (pJoy->on & (JOY_R | JOY_L)) {
        f32 t;
        if (pJoy->on & JOY_R) {
            f32 r = pJoy->triggerRight / 150.0f;
            t = r * -1000.0f * r * r;
        } else {
            f32 r = pJoy->triggerLeft / 150.0f;
            t = r * 1000.0f * r * r;
        }
        t *= (d / 10.0f + 1.0f) * 0.5f;
        t *= m_move_gain;
        switch (ProjType) {
        case 1: {
            f32 dist = pCam->Distance + t;
            if (dist < 1500.0f) {
                if (!(pJoy->on & JOY_A)) {
                    mv.z = dist - 1500.0f;
                }
                dist = 1500.0f;
            }
            CameraCamposDistance(pCam, dist);
            break;
        }
        case 2:
            ORTHO_ZOOM(t);
            ORTHO_B = -ORTHO_T;
            ORTHO_R = -ORTHO_L;
            break;
        }
    }
    if (pJoy->substickX) {
        mv.x = (f32) pJoy->substickX * 2.0f;
    }
    if (pJoy->substickY) {
        mv.y = (f32) pJoy->substickY * 2.0f;
    }
    PSVECScale(&mv, &mv, (d / 10.0f + 1.0f) * 2.0f);
    PSVECScale(&mv, &mv, m_move_gain);
    if (mv.x != 0.0f || mv.y != 0.0f || mv.z != 0.0f) {
        PSMTXMultVecSR(pCam->mat, &mv, &mv);
        CameraDolly(pCam, &mv);
    }
    if (pJoy->stickX) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        CameraRotAxisPosRad(pCam, &axis, &pCam->param.at, (f32) pJoy->stickX * 0.05f * m_move_gain * DEG2RAD);
    }
    if (pJoy->stickY) {
        CameraCamposRot(pCam, 'x', -(f32) pJoy->stickY * 0.05f * m_move_gain * DEG2RAD);
    }
}

// The Z menu: runs the current page (0 flags, 1 camera cuts, 2 hit display, 3 shoulder adjust),
// L / R change pages, and applies the CAMERA MODE selection (0 area cameras .. 5 bird's eye) to
// the camera controller when it changes.
void debugCamera::menu(Camera* pCam, JOY* pJoy)
{
    static int (debugCamera::*sel0_menu_tbl[4])(JOY*) = {
        &debugCamera::menuFlag,
        &debugCamera::menuCamera,
        &debugCamera::menuHitDisp,
        &debugCamera::menuAdjust,
    };
    static int old_cam_mode = 0;
    static CameraParam cameraBak;
    // Word view of campos: the copy below names its y word (see the anchors there).
    static union { Vec v; u32 w[3]; } campos = {{0.0f, 10000.0f, 0.0f}};
    static Vec target = {0.0f, 0.0f, 0.0f};
    static Vec up = {0.0f, 0.0f, -1.0f};
    int ret;

    if (m_menu_sw == 0) {
        return;
    }
    ret = (this->*sel0_menu_tbl[m_sel0])(pJoy);
    if (ret == 0) {
        if (pJoy->trg & JOY_L) {
            m_sel0--;
        }
        if (pJoy->trg & JOY_R) {
            m_sel0++;
        }
        if (m_sel0 < 0) {
            m_sel0 = 3;
        } else if (m_sel0 > 3) {
            m_sel0 = 0;
        }
    }
    if (ret == -1) {
        m_menu_sw = 0;
        m_timer = 5;
        pG->debug_mode = m_printNo_bak;
    }
    // The split `lbz r0,24(r31); clrlwi r11,r0,24` head and the `beq` landing on the tail's `stw`
    // are gcse PRE of the cam_mode byte into `old_cam_mode = cam_mode` below; it needs the empty
    // join block after the inner if/else, kept alive by the `do {} while (0)` at the end of this body.
    if (old_cam_mode != m_cam_mode) {
        switch (m_cam_mode) {
        case 0:
            CamCtrl.m_system_flag = (CamCtrl.m_system_flag & ~8) | 0x10;
            break;
        case 2:
            CamCtrl.r0 = 10;
            CamCtrl.m_system_flag |= 8;
            break;
        case 3:
            CamCtrl.r0 = 7;
            CamCtrl.m_system_flag |= 8;
            break;
        case 4:
            CamCtrl.r0 = 8;
            CamCtrl.m_system_flag |= 8;
            break;
        }
        CamCtrl.r1 = 0;
        if (m_cam_mode != 5) {
            if (old_cam_mode == 5) {
                pG->Camera.param = cameraBak;
                ProjType = 1;
            }
            CameraSetOrientationRoll(&pG->Camera);
        } else {
            cameraBak = pG->Camera.param;
            ProjType = 2;
            u32 t;
            // One destination pointer per copy: the three `addi rD,pG,off` bases are distinct
            // pseudos (r10/r9/r11), so global alloc does not have to give one long-lived `d`
            // the same register three times.
            {
                // The campos copy as three named words: `campos.w[k]` loads are `mem/s` and the
                // `*(u32*)((u32)d0 + k)` stores are flagless MEMs, the same RTL as memcpy's
                // move_by_pieces (a `*(u32*)(d0 + k)` store would be `mem/s`, `((u32*)d0)[k]` too).
                u8* d0 = (u8*) &pG->Camera.param.pos;
                u32 wx = campos.w[0];
                u32 wy = campos.w[1];
                u32 wz = campos.w[2];
                *(u32*) d0 = wx;
                *(u32*) ((u32) d0 + 4) = wy;
                *(u32*) ((u32) d0 + 8) = wz;
                // COMPILER-DIFF: candidate (sched2 tie: the target issues `lwz r0,4(r9)` before
                // `lwz r8,8(r9)`; both loads have priority 29 and 13 dependents at sched2 and the
                // LUID tie goes to z, which sched1 issues first because it kills the campos base).
                // A codeless reader of the y word gives y a 14th sched2 dependent. The `"f"(0.0f)`
                // input (the roll constant's pool load, issued at t13 after the copy stores) makes
                // the anchor ready at t14 so it lands behind the `lfs` at sched1 (any earlier slot
                // lengthens the LC50 high's life and swaps r10/r11); `=&r` keeps local-alloc from
                // tying t to wy; the second anchor after FSet keeps t (and so this insn) alive to
                // sched2 without touching the up copy's pG reload (a slot between the up stores and
                // that reload breaks the fake-death overlap that gives its `addi` r11).
                asm("" : "=&r"(t) : "r"(wy), "f"(0.0f));
            }
            {
                u8* d1 = (u8*) &pG->Camera.param.at;
                memcpy(d1, &target, sizeof(Vec));
            }
            {
                u8* d2 = (u8*) &pG->Camera.Up;
                memcpy(d2, &up, sizeof(Vec));
            }
            pG->Camera.param.roll = 0.0f;
            CameraSetOrientationUp(&pG->Camera);
            DbgFlagOn(pG, DBG_DBG_CAM);
        }
        do { } while (0);
    }
    old_cam_mode = m_cam_mode;
}

// Page 1: pick a camera cut number and area with the D-pad, A plays it (CutCall), X toggles
// its area on / off; prints the current area / camera numbers. -1 closes the menu (B).
int debugCamera::menuCamera(JOY* pJoy)
{
    static const char* str[4] = {"Roll", "FOVy", "Gain", "Play"};
    static int pos[2] = {240, 294};
    Camera* pCam = &pG->Camera;
    int d;
    int i;

    if (pJoy->trg & JOY_B) {
        return -1;
    }
    if (pJoy->rep & 0x80008) {
        m_sel1--;
    }
    if (pJoy->rep & 0x40004) {
        m_sel1++;
    }
    if (m_sel1 < 0) {
        m_sel1 = 3;
    } else if (m_sel1 > 3) {
        m_sel1 = 0;
    }
    d = 0;
    if (pJoy->rep & 0x1) {
        d = -1;
    }
    if (pJoy->rep & 0x10000) {
        d = -10;
    }
    if (pJoy->rep & 0x2) {
        d = 1;
    }
    if (pJoy->rep & 0x20000) {
        d = 10;
    }
    if (d) {
        switch (m_sel1) {
        case 0:
            pCam->param.roll += (f32) d * DEG2RAD;
            if (pCam->param.roll < -PI) {
                pCam->param.roll = -PI;
            }
            if (pCam->param.roll > PI) {
                pCam->param.roll = PI;
            }
            CameraSetOrientationRoll(pCam);
            break;
        case 1:
            pCam->param.fovy += (f32) d * 0.5f;
            if (pCam->param.fovy < 1.0f) {
                pCam->param.fovy = 1.0f;
            }
            if (pCam->param.fovy > 179.0f) {
                pCam->param.fovy = 179.0f;
            }
            CamCtrl.camera.param.fovy = pCam->param.fovy;
            break;
        case 2:
            m_move_gain += (f32) d * 0.1f;
            m_move_gain = m_move_gain < 0.1f ? 0.1f : (m_move_gain > 10.0f ? 10.0f : m_move_gain);
            break;
        case 3: {
            int max = -1;
            CameraDataHeader* data = CamCtrl.pCamData;
            CameraAreaRec* rec = (CameraAreaRec*) (data + 1);
            CameraAreaInfo* area = (CameraAreaInfo*) (rec + data->numArea);
            CameraCut* cut = (CameraCut*) (area + data->numArea);
            int n;
            for (n = 0; n < data->numCut; n++, cut++) {
                if (cut->camera_no > max) {
                    max = cut->camera_no;
                }
            }
            m_cam_no += d;
            m_cam_no = m_cam_no < 0 ? max : (m_cam_no > max ? 0 : m_cam_no);
            break;
        }
        }
    }
    if (m_sel1 == 3) {
        switch (m_cam_play) {
        case 0:
            if (pJoy->trg & JOY_A) {
                if (CamCtrl.DataSearch(m_cam_no)->type == 6) {
                    CamCtrl.CutCall(m_cam_no);
                    pG->debug_mode = m_printNo_bak;
                    m_cam_play++;
                }
            }
            break;
        case 1:
            if (CamCtrl.IsMotionEnd()) {
                m_cam_play++;
            }
            break;
        case 2:
            CamCtrl.Comeback(0);
            pG->debug_mode = 1;
            m_cam_play = 0;
            break;
        }
    }
    eprintf(240, 280, 5, 0, "----- CAMERA -----");
    for (i = 0; i < 4; i++) {
        int col = (i == m_sel1) ? 4 : 0;
        eprintf(pos[0], pos[1] + i * 14, col, 0, "%s", str[i]);
        switch (i) {
        case 0:
            eprintf(pos[0] + 40, pos[1], col, 0, "%f", pCam->param.roll);
            break;
        case 1:
            eprintf(pos[0] + 40, pos[1] + 14, col, 0, "%f", pCam->param.fovy);
            break;
        case 2:
            eprintf(pos[0] + 40, pos[1] + 28, col, 0, "%f", m_move_gain);
            break;
        case 3:
            eprintf(pos[0] + 40, pos[1] + 42, col, 0, "%02d", m_cam_no);
            break;
        }
    }
    return 0;
}

// Page 0: a list of debug switches (camera lock, target type, info display, axis mode, rail
// display, camera mode...) toggled with Left / Right on the selected line.
int debugCamera::menuFlag(JOY* pJoy)
{
    static const char* menu_str[7] = {"DBG_DBG_CAM", "KEY TYPE", "TARGET SEARCH", "INFO_DISP",
                                      "ALONG W_XYZ", "DBG_BACK_CLIP", "CAMERA MODE"};
    static const char* mode_str[6] = {"AREA   ", "BINOCLR", "BEHIND ", "FREE   ", "DEBUG  ", "BIRD   "};
    static const char* target_str[5] = {"EM ", "OBJ", "PL ", "ORG", "OFF"};
    int on = 0;
    int old;
    int d;
    int i;
    int j;
    int x;
    int y;

    if (pJoy->trg & JOY_B) {
        return -1;
    }
    if (pJoy->rep & 0x80008) {
        m_sel1--;
    }
    if (pJoy->rep & 0x40004) {
        m_sel1++;
    }
    if (m_sel1 < 0) {
        m_sel1 = 6;
    } else if (m_sel1 > 6) {
        m_sel1 = 0;
    }
    old = m_sel2;
    if (pJoy->trg & 0x10001) {
        m_sel2--;
    }
    if (pJoy->trg & 0x20002) {
        m_sel2++;
    }
    d = m_sel2 - old;
    if (d != 0) {
        switch (m_sel1) {
        case 0:
            if (DbgFlagChk(pG, DBG_DBG_CAM)) {
                DbgFlagOff(pG, DBG_DBG_CAM);
            } else {
                DbgFlagOn(pG, DBG_DBG_CAM);
            }
            break;
        case 1:
            if (d > 0) {
                m_key_type++;
            } else {
                m_key_type--;
            }
            m_key_type = m_key_type < 0 ? 1 : (m_key_type > 1 ? 0 : m_key_type);
            break;
        case 2:
            if (d > 0) {
                m_target_type++;
            } else {
                m_target_type--;
            }
            m_target_type = m_target_type < 0 ? 4 : (m_target_type > 4 ? 0 : m_target_type);
            break;
        case 3:
            if (d > 0) {
                info_disp = 0;
            } else {
                info_disp = 1;
            }
            break;
        case 4:
            if (d > 0) {
                along_xyz = 0;
            } else {
                along_xyz = 1;
            }
            break;
        case 5:
            if (DbgFlagChk(pG, DBG_BACK_CLIP)) {
                DbgFlagOff(pG, DBG_BACK_CLIP);
            } else {
                DbgFlagOn(pG, DBG_BACK_CLIP);
            }
            break;
        case 6:
            m_cam_mode = m_sel2 = m_sel2 < 0 ? 5 : (m_sel2 > 5 ? 0 : m_sel2);
            break;
        }
    }
    x = 30;
    y = 21;
    eprintf(x * 8, (y - 1) * 14, 5, 0, "------ FLAG ------");
    // `(y + i) * 14` is written out in every row: PRE shares one `y + i` (r25) for the arms, and in
    // the `case 6` arm cse already knows i == 6, so its `y + 6` becomes the hoisted `li r19,27`.
    // The ON/OFF columns are locals (`xon`/`xoff` below): gcse cprop turns `x + 19` into `li 49`
    // and loop.c keeps a 1-insn-lifetime constant in the arm (thr 71 * life 1 < ic 194, then
    // combine folds it into `li r3,392`); a local assigned before the `if (on)` lives long enough
    // to be hoisted (`li r20,49; slwi r3,r20,3`) and takes the extra callee-saved slot (r16..r31).
    // The key/target j-loops compute their colour in a statement (`cj`) before the call: with the
    // ternary inside the argument list the `(y + i) * 14` mult sits before the ternary's join label,
    // so loop.c cannot substitute it into its single use (`no_labels_between_p`) and hoists it
    // instead; as a statement the mult follows the join, is folded into `r4 = P * 14` (a hard-reg
    // dest, never movable) and stays in the loop (`mulli r4,r25,14`). That keeps the shared `y + i`
    // (r25) live across the loop calls, which lets sched1 move the PRE copy `mr r25,r4` up to the
    // head's `addi` (REG_N_CALLS_CROSSED != 0 -> no anti-dependence on the head call), so the head's
    // own `y + i` dies at its mult and gets r4 (`mulli r4,r4,14`).
    for (i = 0; i < 7; i++) {
        int col = (i == m_sel1) ? 4 : 0;
        u8 c = col;
        eprintf(x * 8, (y + i) * 14, c, 0, "%s", menu_str[i]);
        switch (i) {
        case 0:
        case 3:
        case 4:
        case 5: {
            int c_on;
            int c_off;
            switch (i) {
            case 0:
                on = DbgFlagChk(pG, DBG_DBG_CAM);
                break;
            case 5:
                on = DbgFlagChk(pG, DBG_BACK_CLIP);
                break;
            case 3:
                on = info_disp;
                break;
            case 4:
                on = along_xyz;
                break;
            }
            if (on) {
                c_on = col;
                c_off = 7;
            } else {
                c_on = 7;
                c_off = col;
            }
            {
                int xon = x + 15;
                int xoff = x + 19;
                eprintf(xon * 8, (y + i) * 14, c_on, 0, "ON ");
                eprintf(xoff * 8, (y + i) * 14, c_off, 0, "OFF");
            }
            break;
        }
        case 1:
            for (j = 0; j < 2; j++) {
                int cj = (j == m_key_type) ? col : 7;
                eprintf((x + 15 + j * 5) * 8, (y + i) * 14, cj, 0, "%s", key_str[j]);
            }
            break;
        case 2:
            for (j = 0; j < 5; j++) {
                int cj = (j == m_target_type) ? col : 7;
                eprintf((x + 15 + j * 4) * 8, (y + i) * 14, cj, 0, "%s", target_str[j]);
            }
            break;
        case 6:
            eprintf((x + 15) * 8, (y + i) * 14, c, 0, "%02d: %s", m_cam_mode, mode_str[m_cam_mode]);
            break;
        }
    }
    return 0;
}

// Page 2: the collision display switches (Debug_flg[0] bits: scenery polygons, hit boxes,
// bodies, effect collision) toggled per line.
int debugCamera::menuHitDisp(JOY* pJoy)
{
    static int view_mode = 0;
    static int old_view_mode = -1;
    static int shadow_flag = 0;
    static int pos[2] = {240, 294};
    const char* str[9] = {"Game screen", "Scroll atari + BG", "Scroll atari", "Sprite atari + BG", "Sprite atari",
                          "Shadow + BG", "Shadow", "Mirror + BG", "Mirror"};

    if (pJoy->trg & JOY_B) {
        return -1;
    }
    if (pJoy->trg & 0x10001) {
        Dec(view_mode);
    }
    if (pJoy->trg & 0x20002) {
        view_mode++;
    }
    if (view_mode < 0) {
        view_mode = 8;
    } else if (view_mode > 8) {
        view_mode = 0;
    }
    if (old_view_mode != view_mode) {
        old_view_mode = view_mode;
        switch (view_mode) {
        case 0:
            if (!shadow_flag) {
                DpfFlagOff(pG, DPF_SHADOW);
            }
            DbgFlagOff(pG, DBG_SHADOW_POLYGON);
            StaFlagOff(pG, STA_BG_OFF);
            DbgFlagOff(pG, DBG_MIRROR_POLYGON);
            break;
        case 1:
            DbgFlagOn(pG, DBG_SCR_TEST);
            break;
        case 3:
            if (!shadow_flag) {
                DpfFlagOff(pG, DPF_SHADOW);
            }
            StaFlagOff(pG, STA_BG_OFF);
            DbgFlagOff(pG, DBG_SCR_TEST);
            DbgFlagOn(pG, DBG_SCR2_TEST);
            break;
        case 5:
            if (!shadow_flag) {
                DpfFlagOff(pG, DPF_SHADOW);
            }
            StaFlagOff(pG, STA_BG_OFF);
            DbgFlagOff(pG, DBG_SCR2_TEST);
            DbgFlagOn(pG, DBG_SHADOW_POLYGON);
            break;
        case 2:
        case 4:
        case 6:
            shadow_flag = DpfFlagChk(pG, DPF_SHADOW);
            StaFlagOn(pG, STA_BG_OFF);
            DpfFlagOn(pG, DPF_SHADOW);
            break;
        case 7:
            StaFlagOff(pG, STA_BG_OFF);
            DbgFlagOn(pG, DBG_MIRROR_POLYGON);
            DbgFlagOff(pG, DBG_SHADOW_POLYGON);
            break;
        case 8:
            StaFlagOn(pG, STA_BG_OFF);
            DbgFlagOn(pG, DBG_MIRROR_POLYGON);
            break;
        }
    }
    eprintf(240, 280, 5, 0, "------ DISP ------");
    eprintf(pos[0], pos[1], 0, 0, "%s", str[view_mode]);
    return 0;
}

// Page 3: the shoulder camera offset editor (adjust_qFPS at 240 / 294).
int debugCamera::menuAdjust(JOY* pJoy)
{
    static void (debugCamera::*camera_type_tbl[4])(Camera*, JOY*) = {
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_01,
        &debugCamera::camera_type_00,
        &debugCamera::camera_type_00,
    };
    static Vec target_bak;
    static int old_ret = 0;
    Camera* cam = &CamCtrl.camera;
    int ret;

    ret = adjust_qFPS(pJoy, 240, 294, 0, NULL);
    if (ret == 6) {
        if (old_ret != 6) {
            CamCtrl.r1 = 0;
        }
        CamCtrl.r0 = 10;
        CamCtrl.Move();
        pG->Camera = CamCtrl.camera;
    }
    old_ret = ret;
    cam->Distance = PSVECDistance(&cam->param.pos, &cam->param.at);
    switch (ret) {
    case -1:
        return -1;
    case 1:
        (this->*camera_type_tbl[m_key_type])(cam, &Joy[1]);
        target_bak = cam->param.at;
        break;
    case 2:
        (this->*camera_type_tbl[m_key_type])(cam, &Joy[1]);
        cam->param.at = target_bak;
        break;
    }
    eprintf(240, 280, 5, 0, "----- ADJUST -----");
    return ret;
}

// Draws the target cross at the camera's look-at point (red, green up) while the draw timer
// runs.
void CameraDrawTarget(Camera* pCam, int attr)
{
    Vec v[2];
#define a v[0]
#define b v[1]

    if (pG->debug_mode == 0) {
        return;
    }
    if (DbgFlagChk(pG, DBG_TEST_MODE)) {
        if (attr & 1) {
            attr |= 1;
        } else {
            attr &= ~1;
        }
    }
    a = pCam->param.at;
    b = pCam->param.at;
    a.x += 300.0f;
    b.x -= 300.0f;
    Draw_line3d(&a, &b, 0xFFFF0000, 0);
    b = a;
    b.x -= 60.0f;
    b.z += 60.0f;
    Draw_line3d(&a, &b, 0xFFFF0000, 0);
    b = a;
    b.x -= 60.0f;
    b.z -= 60.0f;
    Draw_line3d(&a, &b, 0xFFFF0000, 0);

    a = pCam->param.at;
    b = pCam->param.at;
    a.y += 300.0f;
    b.y -= 300.0f;
    Draw_line3d(&a, &b, 0xFF00FF00, 0);
    b = a;
    b.y -= 60.0f;
    b.x += 60.0f;
    Draw_line3d(&a, &b, 0xFF00FF00, 0);
    b = a;
    b.y -= 60.0f;
    b.x -= 60.0f;
    Draw_line3d(&a, &b, 0xFF00FF00, 0);

    a = pCam->param.at;
    b = pCam->param.at;
    a.z += 300.0f;
    b.z -= 300.0f;
    Draw_line3d(&a, &b, 0xFF2020FF, 0);
    b = a;
    b.z -= 60.0f;
    b.x += 60.0f;
    Draw_line3d(&a, &b, 0xFF2020FF, 0);
    b = a;
    b.z -= 60.0f;
    b.x -= 60.0f;
    Draw_line3d(&a, &b, 0xFF2020FF, 0);

    if (attr & 1) {
        a = pCam->param.at;
        b = pCam->param.at;
        a.y = 50.0f;
        b.y -= 300.0f;
        Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
        a = pCam->param.at;
        b = pCam->param.at;
        b.y = 50.0f;
        a.y = 50.0f;
        a.x += 300.0f;
        b.x -= 300.0f;
        Draw_line3d(&a, &b, 0xFFFF8080, 0);
        a = pCam->param.at;
        b = pCam->param.at;
        b.y = 50.0f;
        a.y = 50.0f;
        a.z += 300.0f;
        b.z -= 300.0f;
        Draw_line3d(&a, &b, 0xFF8080FF, 0);
    }
#undef a
#undef b
}

// Debug text: the game camera and the debug camera pos / target / roll / fov, and the target
// cross; only with the info display on.
void CameraDebugInformation()
{
    CameraControl* cc = &CamCtrl;
    Camera* cam = &cc->camera;
    eprintf(56, 266, 0, 15, "----- GAME CAMERA -----");
    eprintf(56, 280, 0, 15, "Cpos : (%.2f, %.2f, %.2f)", cam->param.pos.x, cam->param.pos.y, cam->param.pos.z);
    eprintf(56, 294, 0, 15, "Trgt : (%.2f, %.2f, %.2f)", cam->param.at.x, cam->param.at.y, cam->param.at.z);
    eprintf(56, 308, 0, 15, "Roll : %.2f", cam->param.roll);
    eprintf(176, 308, 0, 15, "FOVy : %.2f", cam->param.fovy);
    cam = &pG->Camera;
    eprintf(56, 336, 0, 15, "----- DEBUG CAMERA ----");
    eprintf(56, 350, 0, 15, "Cpos : (%.2f, %.2f, %.2f)", cam->param.pos.x, cam->param.pos.y, cam->param.pos.z);
    eprintf(56, 364, 0, 15, "Trgt : (%.2f, %.2f, %.2f)", cam->param.at.x, cam->param.at.y, cam->param.at.z);
    eprintf(56, 378, 0, 15, "Roll : %.2f", cam->param.roll);
    eprintf(176, 378, 0, 15, "FOVy : %.2f", cam->param.fovy);
}

// Maps a stick / dolly vector given in camera axes onto the world XZ plane (camera right and the
// horizontal part of forward) with the y kept.
void moveOnPlaneXZ(Vec* src, Vec* dst)
{
    Camera* cam = &pG->Camera;
    Vec vx;
    Vec vy;
    Vec vz;

    if (src->x == 0.0f && src->y == 0.0f && src->z == 0.0f) {
        memclr_asm(dst, sizeof(Vec));
        return;
    }
    getColumn(cam->mat, 0, &vx);
    getColumn(cam->mat, 1, &vy);
    getColumn(cam->mat, 2, &vz);
    if (vz.y != 0.0f) {
        Vec dx;
        Vec dz;
        Vec zero0 = {0.0f, 0.0f, 0.0f};
        Vec zero1 = {0.0f, 0.0f, 0.0f};
        Vec plane_p = {0.0f, 0.0f, 0.0f};
        Vec plane_n = {0.0f, 1.0f, 0.0f};
        Vec campos = cam->param.pos;
        Vec q;
        Vec s;
        Vec r;
        Mtx m;
        OrthographicProjection(&campos, &s, &vz, &plane_p, &plane_n);
        PSVECScale(&vx, &vx, 1000.0f);
        PSVECAdd(&campos, &vx, &q);
        OrthographicProjection(&q, &r, &vz, &plane_p, &plane_n);
        PSVECSubtract(&r, &s, &dx);
#line 1427 "D:/Bio4/Prog/db_cam.cpp"
        VECNormalize(&dx, &dx);
        PSVECScale(&vy, &vy, 1000.0f);
        PSVECAdd(&campos, &vy, &q);
        OrthographicProjection(&q, &r, &vz, &plane_p, &plane_n);
        PSVECSubtract(&r, &s, &dz);
#line 1434 "D:/Bio4/Prog/db_cam.cpp"
        VECNormalize(&dz, &dz);
        MTX_SET_COLUMNS(m, &dx, &dz, &zero0, &zero1);
        PSMTXMultVecSR(m, src, dst);
    } else {
        Vec v;
        v.x = src->x;
        v.y = 0.0f;
        v.z = src->y;
        PSMTXMultVecSR(cam->mat, &v, dst);
    }
}

// Draws a reference grid on the ground plane (1000 or 10000 unit cells) with the axes in white.
void drawGround(int flag)
{
    const f32 unit = 1000.0f;
    int n = flag ? 60 : 30;
    Vec a;
    Vec b;
    int i;
    f32 neg;
    f32 pos;

    b.y = 0.0f;
    a.y = 0.0f;
    a.x = (f32) -n * unit;
    b.x = (f32) n * unit;
    for (i = -n; i <= n; i++) {
        if (i != 0) {
            b.z = (f32) i * unit;
            a.z = (f32) i * unit;
            Draw_line3d(&a, &b, 0xFF404040, 0);
        }
    }
    b.y = 0.0f;
    a.y = 0.0f;
    a.z = (f32) -n * unit;
    b.z = (f32) n * unit;
    for (i = -n; i <= n; i++) {
        if (i != 0) {
            b.x = (f32) i * unit;
            a.x = (f32) i * unit;
            Draw_line3d(&a, &b, 0xFF404040, 0);
        }
    }
    neg = (f32) -n * unit;
    pos = (f32) n * unit;
    a.x = neg * 1.1f;
    a.z = b.z = a.y = b.y = 0.0f;
    b.x = pos * 1.1f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.x = pos;
    a.z = pos * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.z = neg * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.x = b.x = a.y = b.y = 0.0f;
    a.z = neg * 1.1f;
    b.z = pos * 1.1f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.z = pos;
    a.x = pos * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    a.x = neg * 0.05f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
}

// The shoulder camera offset editor: a menu (Select Site, Symmetry, Follow Grnd, Fovy, Reset)
// over the 2 x 3 (left / right x up / mid / down) ready and transition offset tables; the stick
// moves the selected site's camera / close / target points in player space (mirrored to the
// other side with Symmetry), edits go into the area override tables through
// CameraQuasiFPS::setAreaData. flag bit0 resets the editor state. Returns 1 while a value was
// changed, -1 on exit.
int adjust_qFPS(JOY* pJoy, int x, int y, int flag, int* out)
{
    static const char* menu_str[5] = {"Select Site", "Symmetry", "Follow Grnd", "Fovy", "Reset"};
    static u8* p_offset;   // byte pointers: the original copies the records with memcpy
    static u8* p_counter;
    static int menu_no = 0;
    static int menu_level = 0;
    static int symmetry_flag = 1;
    static int site_col = 0;
    static int site_row = 0;
    static int site_LR = 0;
    static int site_NF = 0;
    static int site_UMD = 0;
    static int yes_no = 0;
    static int near_far = 0;
    GlobalWork* g = pG;
    Camera* cam = &g->Camera;
    CameraQuasiFPS* q = &CamCtrl.m_QuasiFPS;
    Mtx inv;
    Vec target;
    Vec campos;
    Vec close;
    int cx = 0;
    int cy = 0;
    int ret = 0;
    int i;
    int j;

    PSMTXInverse(pPL->mat, inv);
    if (flag & 1) {
        if (flag & 2) {
            menu_no = -1;
        } else {
            menu_no = 0;
        }
        site_col = 1;
        site_row = 4;
        menu_level = 0;
        near_far = 0;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 3; j++) {
                g_local_ready[i][j] = g_readyOfs[0][i][j];
                g_local_trans[i][j] = g_transOfs[TRANS_DATA_LEON][i][j];
            }
        }
        q->setAreaData(g_local_ready, g_local_trans);
        g_local_floor_ratio = q->getFloorRatio();
        return 0;
    }
    switch (menu_level) {
    case 0:
        if (pJoy->trg & JOY_B) {
            menu_no = 0;
            menu_level = 0;
            ret = -1;
            break;
        }
        if (pJoy->rep & JOY_UP) {
            Dec(menu_no);
        }
        if (pJoy->rep & JOY_DOWN) {
            menu_no++;
        }
        if (flag & 2) {
            menu_no = menu_no < -1 ? -1 : (menu_no > 6 ? 6 : menu_no);
            *out = menu_no;
        } else {
            menu_no = menu_no < 0 ? 0 : (menu_no > 4 ? 4 : menu_no);
        }
        switch (menu_no) {
        case 0:
            if (pJoy->trg & JOY_A) {
                menu_level = 1;
                DbgFlagOn(pG, DBG_ADJUST_CAM);
                if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
                    CamDbg.m_cam_mode = 2;
                }
                SpfFlagOn(pG, SPF_PL);
            }
            break;
        case 1:
            if (pJoy->rep & (JOY_LEFT | JOY_RIGHT)) {
                symmetry_flag = !symmetry_flag;
            }
            break;
        case 2: {
            f32 step = (pJoy->on & JOY_X) ? 0.01f : 0.1f;
            if (pJoy->rep & JOY_LEFT) {
                g_local_floor_ratio = g_local_floor_ratio - step;
            }
            if (pJoy->rep & JOY_RIGHT) {
                g_local_floor_ratio = g_local_floor_ratio + step;
            }
            g_local_floor_ratio =
                 g_local_floor_ratio < 0.0f ? 0.0f : (g_local_floor_ratio > 2.0f ? 2.0f : g_local_floor_ratio);
            if (pJoy->trg & JOY_A) {
                ret = 5;
                q->setFloorRatio(g_local_floor_ratio);
            }
            break;
        }
        case 3:
            if (pJoy->trg & JOY_A) {
                q->getAreaData(g_local_ready, g_local_trans);
                g_local_fovy[0] = g_local_ready[0][1].Fovy;
                g_local_fovy[1] = g_local_trans[0][1].Fovy;
                menu_level = 5;
            }
            break;
        case 4:
            if (pJoy->trg & JOY_A) {
                menu_level = 4;
                yes_no = 0;
            }
            break;
        }
        break;
    case 1:
        if (pJoy->trg & JOY_B) {
            DbgFlagOff(pG, DBG_ADJUST_CAM);
            SpfFlagOff(pG, SPF_PL);
            if (CamDbg.m_cam_mode != 2) {
                CamDbg.m_cam_mode = 0;
            }
            menu_level = 0;
        } else if (pJoy->trg & JOY_A) {
            q->getAreaData(g_local_ready, g_local_trans);
            SpfFlagOn(pG, SPF_CAMERA);
            menu_level = 2;
        } else {
            int old_umd = site_UMD;
            int old_nf = site_NF;
            if (pJoy->rep & JOY_LEFT) {
                Dec(site_col);
            }
            if (pJoy->rep & JOY_RIGHT) {
                site_col++;
            }
            site_col = site_col < 0 ? 0 : (site_col > 1 ? 1 : site_col);
            if (symmetry_flag) {
                site_col = 1;
            }
            if (pJoy->rep & JOY_UP) {
                Dec(site_row);
            }
            if (pJoy->rep & JOY_DOWN) {
                site_row++;
            }
            site_row = site_row < 0 ? 0 : (site_row > 5 ? 5 : site_row);
            if (site_col == 0) {
                site_LR = 0;
            } else {
                site_LR = site_col;
            }
            if (site_row <= 2) {
                site_UMD = site_row;
                site_NF = 0;
            } else {
                site_NF = 1;
                site_UMD = site_row - 3;
            }
            if (site_NF) {
                if (site_LR) {
                    q->m_site = 2;
                } else {
                    q->m_site = 3;
                }
            } else {
                if (site_LR) {
                    q->m_site = 0;
                } else {
                    q->m_site = 1;
                }
            }
            switch (q->m_site) {
            case 0:
                p_offset = (u8*) &g_local_ready[0][site_UMD];
                p_counter = (u8*) &g_local_ready[1][site_UMD];
                break;
            case 1:
                p_offset = (u8*) &g_local_ready[1][site_UMD];
                p_counter = (u8*) &g_local_ready[0][site_UMD];
                break;
            case 2:
                p_offset = (u8*) &g_local_trans[0][site_UMD];
                p_counter = (u8*) &g_local_trans[1][site_UMD];
                break;
            case 3:
                p_offset = (u8*) &g_local_trans[1][site_UMD];
                p_counter = (u8*) &g_local_trans[0][site_UMD];
                break;
            }
            switch (site_UMD) {
            case 0:
                q->m_depression_ratio = 1.0f;
                break;
            case 1:
                q->m_depression_ratio = 0.0f;
                break;
            case 2:
                q->m_depression_ratio = -1.0f;
                break;
            }
            if (site_NF == 0) {
                if (old_umd != site_UMD) {
                    switch (site_UMD) {
                    case 0:
                        PlWepMotSet(1);
                        break;
                    case 1:
                        PlWepMotSet(0);
                        break;
                    case 2:
                        PlWepMotSet(2);
                        break;
                    }
                }
            } else {
                if (old_nf == 0) {
                    PlWepMotSet(3);
                }
            }
            MotionMove(pPL, 0);
            ret = 6;
        }
        break;
    case 2:
        MotionMove(pPL, 0);
        if (pJoy->trg & JOY_B) {
            SpfFlagOff(pG, SPF_CAMERA);
            menu_level = 1;
        } else if (pJoy->trg & JOY_A) {
            PSMTXMultVec(inv, &g->Camera.param.pos, &QOFS(p_offset)->Campos);
            PSMTXMultVec(inv, &g->Camera.param.at, &QOFS(p_offset)->Target);
            if (symmetry_flag) {
                memcpy(p_counter, p_offset, sizeof(QfpsOfs));
                QOFS(p_counter)->Campos.x = -QOFS(p_counter)->Campos.x;
                QOFS(p_counter)->Target.x = -QOFS(p_counter)->Target.x;
            }
            q->setAreaData(g_local_ready, g_local_trans);
            PSMTXMultVec(pPL->mat, &QOFS(p_offset)->campos2, &CamCtrl.camera.param.pos);
            CameraSetOrientationRoll(&CamCtrl.camera);
            menu_level = 3;
            ret = 3;
        } else {
            ret = 1;
        }
        break;
    case 3:
        MotionMove(pPL, 0);
        if (pJoy->trg & JOY_B) {
            PSMTXMultVec(pPL->mat, &QOFS(p_offset)->Campos, &CamCtrl.camera.param.pos);
            CameraSetOrientationRoll(&CamCtrl.camera);
            menu_level = 2;
        } else if (pJoy->trg & JOY_A) {
            PSMTXMultVec(inv, &g->Camera.param.pos, &QOFS(p_offset)->campos2);
            if (symmetry_flag) {
                memcpy(p_counter + QOFS_CAMPOS2, p_offset + QOFS_CAMPOS2, sizeof(Vec));
                QOFS(p_counter)->campos2.x = -QOFS(p_counter)->campos2.x;
            }
            q->setAreaData(g_local_ready, g_local_trans);
            ret = 3;
            SpfFlagOff(pG, SPF_CAMERA);
            menu_level = 1;
        } else {
            ret = 2;
        }
        break;
    case 4:
        if (pJoy->trg & JOY_B) {
            menu_level = 0;
            break;
        }
        if (pJoy->rep & JOY_LEFT) {
            yes_no = 1;
        }
        if (pJoy->rep & JOY_RIGHT) {
            yes_no = 0;
        }
        if (pJoy->trg & JOY_A) {
            if (yes_no) {
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 3; j++) {
                        g_local_ready[i][j] = g_readyOfs[0][i][j];
                        g_local_trans[i][j] = g_transOfs[TRANS_DATA_LEON][i][j];
                    }
                }
                ret = 4;
                q->setAreaData(g_local_ready, g_local_trans);
            }
            menu_level = 0;
        }
        break;
    case 5:
        if (pJoy->trg & JOY_B) {
            menu_level = 0;
            break;
        }
        if (pJoy->trg & JOY_UP) {
            near_far = 0;
        }
        if (pJoy->trg & JOY_DOWN) {
            near_far = 1;
        }
        {
            f32 step = 1.0f;
            if (pJoy->on & JOY_X) {
                step = 10.0f;
            }
            if (pJoy->rep & JOY_LEFT) {
                g_local_fovy[near_far] = g_local_fovy[near_far] - step;
            }
            if (pJoy->rep & JOY_RIGHT) {
                g_local_fovy[near_far] = g_local_fovy[near_far] + step;
            }
            g_local_fovy[near_far] = g_local_fovy[near_far] < 1.0f
                                         ? 1.0f
                                         : (g_local_fovy[near_far] > 90.0f ? 90.0f : g_local_fovy[near_far]);
        }
        if (pJoy->trg & JOY_A) {
            ret = 3;
            menu_level = 0;
        }
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 3; j++) {
                g_local_ready[i][j].Fovy = g_local_fovy[0];
                g_local_trans[i][j].Fovy = g_local_fovy[1];
            }
        }
        q->setAreaData(g_local_ready, g_local_trans);
        break;
    }
    for (i = 0; i < 5; i++) {
        int col = (menu_no == i) ? 4 : 0;
        eprintf(x + cx * 8, y + (cy + i) * 14, col, 0, "%s", menu_str[i]);
        switch (i) {
        case 0:
            break;
        case 1:
            if (symmetry_flag) {
                eprintf(x + (cx + 12) * 8, y + (cy + 1) * 14, col, 0, "ON-/---");
            } else {
                eprintf(x + (cx + 12) * 8, y + (cy + 1) * 14, col, 0, "---/OFF");
            }
            break;
        case 2:
            eprintf(x + (cx + 12) * 8, y + (cy + 2) * 14, col, 0, "%4.2f", g_local_floor_ratio);
            break;
        case 3:
            if (menu_level == 5) {
                static const char* fovy_str[2] = {"READY", "TRANS"};
                for (j = 0; j < 2; j++) {
                    eprintf(x + (cx + 12) * 8, y + (cy + 3 + j) * 14, (near_far == j) ? 4 : 0, 0, "%s", fovy_str[j]);
                    eprintf(x + (cx + 17) * 8, y + (cy + 3 + j) * 14, 0, 0, "%3.1f", g_local_fovy[j]);
                }
            }
            break;
        case 4:
            if (menu_level == 4) {
                if (yes_no) {
                    eprintf(x + (cx + 12) * 8, y + (cy + 4) * 14, col, 0, "YES/---");
                } else {
                    eprintf(x + (cx + 12) * 8, y + (cy + 4) * 14, col, 0, "---/NO-");
                }
            }
            break;
        }
    }
    if (menu_level >= 1 && menu_level <= 3) {
        static const char* umd_str[3] = {"Up", "Mid", "Dwn"};
        eprintf(x + 168, y - 14, 5, 0, "--- SITE ---");
        eprintf(x + 168, y, 5, 0, "LFT RGT");
        eprintf(x + 232, y + 28, 5, 0, "NEAR");
        eprintf(x + 232, y + 70, 5, 0, "FAR");
        for (i = 0; i < 2; i++) {
            // `line` lives in the outer body: the inner loop's giv init stays `line * 14 + y`
            // (mulli hoisted, folded to `li 14` in the outer preheader), not `addi y, 14`.
            int line = cy + 1;
            for (j = 0; j < 6; j++) {
                int col = 0;
                if (i == site_col) {
                    col = (j == site_row) ? 4 : 0;
                }
                eprintf(x + (21 + i * 4) * 8, line * 14 + y + j * 14, col, 0, "%s", umd_str[j % 3]);
            }
        }
    }
    if (menu_level == 2) {
        PSMTXMultVec(inv, &cam->param.at, &target);
        PSMTXMultVec(inv, &cam->param.pos, &campos);
        if (pG->Frame_cnt & 0x18) {
            eprintf(120, 14, 5, 0, "--- CAMERA OFFSET ---");
        }
        eprintf(120, 28, 4, 0, "Target: (%f, %f, %f)", target.x, target.y, target.z);
        eprintf(120, 42, 4, 0, "Campos: (%f, %f, %f)", campos.x, campos.y, campos.z);
    }
    if (menu_level == 3) {
        PSMTXMultVec(inv, &cam->param.at, &target);
        PSMTXMultVec(inv, &cam->param.pos, &close);
        if (pG->Frame_cnt & 0x18) {
            eprintf(120, 14, 5, 0, "---- CLOSE POINT ----");
        }
        eprintf(120, 28, 0, 0, "Target: (%f, %f, %f)", target.x, target.y, target.z);
        eprintf(120, 42, 4, 0, "Campos: (%f, %f, %f)", close.x, close.y, close.z);
    }
    return ret;
}

asm(".section .sdata; .balign 8");
