// game/cam_ctrl.cpp: the camera controller (CamCtrl). The room's camera data (B40x file: trigger
// areas -> camera cuts, plus interpolation records) selects a cut when the player enters an area
// (areaHitCheck, with calm / battle attribute variants); each cut type maps to an r0 routine
// (fixed, pan, rail track / pan / behind, free, camera motion, shoulder camera = CameraQuasiFPS
// in cam_qfps.cpp) and the extras (scope, binoculars, push object, look-down, attached motion
// cameras in cam_extra.cpp) plug in as cCamera objects. Move() produces the frame's camera
// through the cut interpolation and smoothing; CameraMove (camera.cpp) copies it into pG->Cam.

#include "types.h"
#include "vec.h"
#include "global.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_extra.h"
#include "cam_motion.h"
#include "db_log.h"
#include "atari.h"
#include "light.h"
#include "model.h"
#include "player.h"
#include "em.h"
#include "main_mem.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "at_mod.h"
#include "joy.h"

extern "C" {
int strncmp(const char* a, const char* b, unsigned int n);
void* memset(void* dst, int c, unsigned int n);
void OSReport(const char* fmt, ...);
f32 sinf(f32);
f32 cosf(f32);
}

extern f32 ZNEAR;
u32 SubCharGetStatus();
int GetWaterHeight(Vec* pos, f32* height);
void QuakeInit();
void eprintf(int x, int y, int color, int p, const char* fmt, ...);


#define PI 3.1415927f
#define PI2 6.2831855f
#define DEG 0.017453292f

struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

void* g_pToolCamData = NULL;

#define CAMERA_MOTION_BUFFER_SIZE 0x440
static u8 CameraMotionBuffer[CAMERA_MOTION_BUFFER_SIZE];
extern CameraBSpline CamBSpline;

// internal linkage: the table is deferred behind the cManager template strings in .rodata
static const f32 smooth_ratio[12] = {0.0f, 0.9f, 0.85f, 0.92f, 0.8f, 0.92f, 0.9f, 0.9f, 0.9f, 0.9f, 0.0f, 0.0f};

// Byte-wise copy of the float `tmp` into the (unaligned) motion buffer. `&tmp` indexed directly
// (a pointer local is copy-propagated into loops 2/3), `n` is the function-scope counter shared
// by the three copies (one allocno -> r11 in all three, the `&tmp` copies fall to r12/r9/r9).
#define EXPORT_TMP(p)                             \
    {                                             \
        for (n = 0; n < 4; n++) {                 \
            *(p)++ = ((u8*) &tmp)[n];             \
        }                                         \
    }

// Converts a rail cut into the CameraMotion key-frame format (cam_motion): header, 4 channels
// (pos, at, roll, fovy) x 3 components of hermite keys {value, tangent in, tangent out}.
int CameraControl::HermiteExport(CameraCut* cut, u8* p)
{
    u8* buf = p;  // the parameter is the running pointer (r5: `sth 0(r5); stbu 2(r5); addi r5,1`), buf the saved base
    u32* table;
    u16* frames;
    int i;
    int j;
    int k;
    int k0;
    int k1;
    f32 tmp;
    f32 v;
    f32 tan;
    f32 v0;
    f32 v1;
    f32 dt0;
    f32 dt1;
    int n;

    *(u16*) p = (cut->num - 1) * 30;
    p += 2;
    *p++ = 4;
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
        case 1:
            *(u16*) p = 4;
            break;
        case 2:
        case 3:
            *(u16*) p = 2;
            break;
        }
        p += 2;
    }
    for (i = 0; i < 4; i++) {
        *p++ = i;
    }
    *p++ = 0;
    *(u32*) p = 0;
    p += 4;
    table = (u32*) p;
    for (i = 0; i < 4; i++) {
        *(u32*) p = 0;
        p += 4;
    }
    for (i = 0; i < 4; i++) {
        table[i] = p - buf;
        for (j = 0; j < 3; j++) {
            *(u16*) p = cut->num;
            p += 2;
            v = 0.0f;
            v0 = 0.0f;
            v1 = 0.0f;
            frames = (u16*) p;
            for (k = 0; k < cut->num; k++) {
                if (cut->frames == NULL) {
                    *(u16*) p = k * 30;
                } else {
                    *(u16*) p = cut->frames[k];
                }
                p += 2;
            }
            for (k = 0; k < cut->num; k++) {
                k1 = k + 1;
                k0 = k - 1;
                if (k1 > cut->num - 1) {
                    k1 = cut->num - 1;
                }
                if (k0 < 0) {
                    k0 = 0;
                }
                switch (i) {
                case 0:
                    v = (&cut->pos[k].x)[j];
                    v0 = (&cut->pos[k0].x)[j];
                    v1 = (&cut->pos[k1].x)[j];
                    break;
                case 1:
                    v = (&cut->at[k].x)[j];
                    v0 = (&cut->at[k0].x)[j];
                    v1 = (&cut->at[k1].x)[j];
                    break;
                case 2:
                    v = cut->roll[k];
                    v0 = cut->roll[k0];
                    v1 = cut->roll[k1];
                    break;
                case 3:
                    v = cut->fovy[k];
                    v0 = cut->fovy[k0];
                    v1 = cut->fovy[k1];
                    v1 *= DEG;
                    v *= DEG;
                    v0 *= DEG;
                    break;
                }
                tmp = v;
                EXPORT_TMP(p);
                dt0 = (f32) (frames[k] - frames[k0]);
                dt1 = (f32) (frames[k1] - frames[k]);
                if (k == 0) {
                    tan = (v1 - v) / dt1;
                } else if (cut->num - 1 == k) {
                    tan = (v - v0) / dt0;
                } else {
                    tan = (dt1 * ((v - v0) / dt0) + dt0 * ((v1 - v) / dt1)) / (dt0 + dt1);
                }
                tan *= dt0;
                tmp = tan;
                EXPORT_TMP(p);
                EXPORT_TMP(p);
            }
        }
        {
            // `rem` is one multi-set variable (in place in the `p - buf` register) and the pad loop
            // counts on `j` (a GPR elsewhere, so the reversed count stays `addic./bne`, no ctr).
            int rem = p - buf;
            rem %= 4;
            if (rem) {
                rem = 4 - rem;
                for (j = 0; j < rem; j++) {
                    *p++ = 0;
                }
            }
        }
    }
    *(u16*) buf = frames[cut->num - 1];
    return p - buf;
}

// 1 during the frame the camera cut changed (m_state_flag bit1); CamStick2World and the
// visibility tests use it.
int CameraControl::IsChangeCamera()
{
    if (m_state_flag & 2) {
        return 1;
    }
    return 0;
}

// Returns control to the area cameras after a forced cut / event camera: clears the "cut held"
// flag, re-enables the area check and drops the motion-camera flag.
void CameraControl::Comeback(int)
{
    data = (CameraDataHeader*) pG->pCamRoom;
    m_state_flag &= ~4;
    m_system_flag = (m_system_flag & ~8) | 0x10;
    if (m_system_flag & 0x20) {
        m_system_flag &= ~0x20;
    }
    Check();
}

// Stops the controller (r0 Wait, area check off); the room / event drives pG->Cam itself.
void CameraControl::Disable()
{
    r0 = 0;
    m_system_flag |= 8;
}

// mode 0 disables the per-frame area check (the current camera stays), else re-enables it.
void CameraControl::AreaCheckOnOff(int mode)
{
    switch (mode) {
    case 0:
        m_system_flag |= 8;
        break;
    case 1:
        m_system_flag = (m_system_flag & ~8) | 0x10;
        break;
    }
}

// Number of camera areas in the room data.
u8 CameraControl::AreaNum()
{
    return data->numArea;
}

// Area number of the active camera (-1 none).
int CameraControl::CurrentAreaNo()
{
    return areaNo;
}

// Camera number of the active cut (-1 none).
int CameraControl::CurrentCameraNo()
{
    return cameraNo;
}

// The cut record with camera_no `no` (the last record when not found).
CameraCut* CameraControl::DataSearch(int no)
{
    CameraAreaRec* rec = (CameraAreaRec*) (data + 1);
    CameraAreaInfo* area = (CameraAreaInfo*) (rec + data->numArea);
    CameraCut* cut = (CameraCut*) (area + data->numArea);
    int i = 0;

    while (i < data->numCut && no != cut->camera_no) {
        i++;
        cut++;
    }
    return cut;
}

// The interpolation record for the transition from one area / camera to another; NULL when the
// data has none (a hard cut).
CameraLerp* CameraControl::LerpDataSearch(int area_from, int cam_from, int area_to, int cam_to)
{
    CameraAreaRec* rec = (CameraAreaRec*) (data + 1);
    CameraAreaInfo* area = (CameraAreaInfo*) (rec + data->numArea);
    CameraCut* cut = (CameraCut*) (area + data->numArea);
    CameraLerp* lerp = (CameraLerp*) (cut + data->numCut);
    int i;

    for (i = 0; i < data->numLerp; i++, lerp++) {
        if (area_from == lerp->area_from && cam_from == lerp->cam_from && area_to == lerp->area_to &&
            cam_to == lerp->cam_to) {
            return lerp;
        }
    }
    return NULL;
}

// Relocates a camera data file in place ("B400".."B404": file offsets -> pointers for the area
// polygons and the cut key arrays); older versions get their attr 8 promoted to 0x20. Returns
// the buffer, or unchanged when already relocated / unknown.
CameraDataHeader* CameraControl::calcAddr(CameraDataHeader* pBuff)
{
    int ver2;
    int i;
    CameraAreaRec* rec;
    CameraAreaInfo* area;
    CameraCut* cut;

    if (cameraDataVersion((char*) pBuff) <= 1) {
        return pBuff;
    }
    ver2 = 0;  // assigned after the early return: its `li` lands after the strncmp call
    if (strncmp((char*) pBuff, "B402", 4) == 0) {
        ver2 = 1;
        OSReport("CameraControl::calcAddr(): R%1d%02x Ver02", pG->stage_no, pG->room_no);
    }

    rec = (CameraAreaRec*) (pBuff + 1);
    for (i = 0; i < pBuff->numArea; i++, rec++) {
        if ((s32) rec->area < 0) {
            return pBuff;
        }
        rec->area = (CameraAreaInfo*) ((u32) rec->area + (u32) pBuff);
        if (rec->cut) {
            rec->cut = (CameraCut*) ((u32) rec->cut + (u32) pBuff);
        }
    }

    area = (CameraAreaInfo*) rec;
    for (i = 0; i < pBuff->numArea; i++, area++) {
        area->points = (Vec*) ((u32) area->points + (u32) pBuff);
        if (ver2) {
            area->attr = 3;
        }
        if (area->attr & 8) {
            area->attr |= 0x20;
        }
        if (cameraDataVersion((char*) pBuff) <= 3) {
            area->attr2 = 1;
            area->attr3 = 0xFF;
            OSReport("CameraControl::calcAddr(): R%1d%02x Ver%02d", pG->stage_no, pG->room_no,
                     cameraDataVersion((char*) pBuff));
        }
    }

    cut = (CameraCut*) area;
    for (i = 0; i < pBuff->numCut; i++, cut++) {
        cut->pos = (Vec*) ((u32) cut->pos + (u32) pBuff);
        cut->at = (Vec*) ((u32) cut->at + (u32) pBuff);
        cut->roll = (f32*) ((u32) cut->roll + (u32) pBuff);
        cut->fovy = (f32*) ((u32) cut->fovy + (u32) pBuff);
        cut->frames = (u16*) ((u32) cut->frames + (u32) pBuff);
    }
    return pBuff;
}

// Installs the room's camera data (relocated).
void CameraControl::RoomDataRead(CameraDataHeader* room)
{
    G_ROOM_CAM_DATA = calcAddr(room);
    data = (CameraDataHeader*) pG->pCamRoom;
}

// Installs the core (shared) camera data.
void CameraControl::CoreDataRead(CameraDataHeader* core)
{
    pG->pCamCore = calcAddr(core);
}

// Line of sight test for cameras: from -> to against characters, objects and the scenery (walls
// only, camera-ignored attributes masked); the nearest hit in *pos / *nrm. 1 when blocked.
int cameraHitCheck(Vec* pos, Vec* nrm, Vec* from, Vec* to)
{
    static f32 R_GAIN = 1.1f;
    static f32 GAIN = 1.33f;
    Vec posA;
    Vec posB;
    Vec posC;
    Vec nrmA;
    Vec nrmB;
    Vec nrmC;
    Vec p;
    Vec hp;
    Vec hn;
    int hitA;
    int hitB;
    int hitC;
    int ret = 0;
    int first;
    f32 dist;
    f32 d;

    hitA = EmHitCheck(&posA, &nrmA, from, to, 1);
    hitB = ObjHitCheck(&posB, &nrmB, from, to, 1);
    hitC = SatMgr.hitCheck(from, to, &posC, &nrmC, 0x8000, 0x1C2810);
    if (hitA | hitB | hitC) {
        // COMPILER-DIFF: codeless anchor. The empty loop leaves NOTE_INSN_LOOP_BEG/END here, which ends
        // the first cse pass's extended basic block at this point (cse1 stops at LOOP_END). Without it
        // cse1 folds the `&posB`/`&nrmB` recomputations into the earlier `&posA` pseudos and the target's
        // `mr r18,r28` (gcse PRE copy) and fresh `addi r6/r7` argument forms are not produced.
        do { } while (0);
        dist = 0.0f;
        first = 1;
        if (hitA) {
            dist = PSVECDistance(from, &posA);
            first = 0;
            *pos = posA;
            *nrm = nrmA;
        }
        if (hitB) {
            d = PSVECDistance(from, &posB);
            if (first || d < dist) {
                dist = d;
                first = 0;
                *pos = posB;
                *nrm = nrmB;
            }
        }
        if (hitC) {
            d = PSVECDistance(from, &posC);
            if (first || d < dist) {
                *pos = posC;
                *nrm = nrmC;
            }
        }
        ret = 1;
    }
    if (pSubEm && pSubEm->id == 3) {
        cAtariInfo atBuf;
        // The target reads/writes the info through a pointer register (lha 0x18(r29), stfs 0x4(r29)) that is
        // a copy of the constructor's `this` register (`mr r29,r30`), and the 76-byte copy below increments
        // that `this` register in place. A plain `cAtariInfo& at = atBuf;` cannot produce this: cse makes the
        // longer-lived reference the canonical register (the copy loop then runs on a copy of it), and gcse
        // copy propagation replaces the reference by the `this` temporary everywhere else.
        // COMPILER-DIFF: register pin (r29) plus launder. The pin keeps `at` out of cse's canonical class
        // (hard regs go last), so the copy loop's address is the `this` temporary; the launder gives `at` a
        // second set so the `at = this` copy is not propagated into the later field accesses.
        register cAtariInfo* at asm("r29") = &atBuf;
        asm("" : "+r"(at));
        cModel* parts;
        Vec w;

        atBuf = pSubEm->atari;
        if (at->m_parts_no != 0) {
            parts = pSubEm->getPartsPtr(at->m_parts_no - 1);
        } else {
            parts = pSubEm;
        }
        if (parts) {
            f32 r;
            int hit;

            at->m_offset.y -= 1000.0f;
            at->m_height += 1000.0f;
            PSMTXMultVec(parts->mat, &at->m_offset, &w);
            r = at->m_radius * R_GAIN;
            if (ret) {
                p = *pos;
            } else {
                p = *to;
            }
            // `hit = 0` after the `p` copy: the `li` sits in the join block and the w.y/p.y compare
            // registers come out as f12/f13 (declaring it initialised moves both).
            hit = 0;
            if (w.y <= p.y) {
                if (p.y <= w.y + at->m_height) {
                    Vec a;
                    Vec b;

                    a = w;
                    b = p;
                    a.y = 0.0f;
                    b.y = 0.0f;
                    if (PSVECDistance(&b, &a) <= r) {
                        hit = 1;
                    }
                }
            }
            if (hit == 1) {
                at->m_radius *= GAIN;
                if (ObaLineHitChk(pSubEm, at, from, &p, &hp, &hn)) {
                    ret = 1;
                    *pos = hp;
                }
            }
        }
    }
    return ret;
}

// Copies the cut's first key (pos / at / roll / fov) into a Camera and rebuilds its orientation.
void CameraSetCutData(Camera* cam, CameraCut* cut)
{
    cam->param.pos = *cut->pos;
    cam->param.at = *cut->at;
    cam->param.roll = *cut->roll;
    cam->param.fovy = *cut->fovy;
    CameraSetOrientationRoll(cam);
}

// Script: enables / disables the camera area (area_no, camera_no) for the area check.
void CameraControl::AreaOnOff(int area_no, int camera_no, int on)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (area_no == rec->area->area_no && camera_no == rec->area->camera_no) {
            rec->area->enable = on;
            break;
        }
    }
}

// Script: ORs `attr` bits into the area's attribute (0x20 normal, 1 / 2 calm / battle...).
void CameraControl::SetAreaAttr(int area_no, int camera_no, u8 attr)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (area_no == rec->area->area_no && camera_no == rec->area->camera_no) {
            rec->area->attr |= attr;
            break;
        }
    }
}

// Script: clears `attr` bits of the area's attribute.
void CameraControl::UnsetAreaAttr(int area_no, int camera_no, u8 attr)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (area_no == rec->area->area_no && camera_no == rec->area->camera_no) {
            rec->area->attr &= ~attr;
            break;
        }
    }
}

// Script: forces camera cut `no` (its first area record) and holds it (m_state_flag bit2) until
// Comeback.
void CameraControl::CutCall(int no)
{
    CameraDataHeader* d = data;
    CameraAreaRec* rec = (CameraAreaRec*) (d + 1);
    int found = 0;
    s8 i;

    for (i = 0; i < d->numArea; i++, rec++) {
        if (no == rec->cut->camera_no) {
            found = 1;
            break;
        }
    }
    if (found) {
        clearAttachCamera();
        interp.frame = 0;
        switchCamera(rec);
        m_system_flag |= 8;
        m_state_flag |= 4;
    } else {
        pLog->err(0, 0, "CameraControl::CutCall(): Cut %02d doesn't exist.", no);
    }
}

// Activates the area record: sets up the lerp from the current camera when the data has one,
// updates the room light area unless the area says not to, remembers area / camera numbers and
// picks the routine from the cut type: 0 Fix, 1 Pan, 2 Track, 3 RailPan, 4 RailBehind, 5 Free,
// 6 / 7 a CameraMotion from the room motion buffer (Motion / UpCut), 8 the shoulder camera with
// the area's offsets (bindAreaCamera).
void CameraControl::switchCamera(CameraAreaRec* rec)
{
    CameraAreaInfo* area = rec->area;
    CameraCut* cut = rec->cut;
    CameraLerp* lerp = NULL;
    CameraDataHeader* d;
    register int i asm("r11");  // COMPILER-DIFF: loop counter r11 / pointer r10 (global allocates `r` first in ours: 14 refs/28 insns vs `i` 16/64)
    CameraAreaRec* r;
    int size;

    if (areaNo != -1) {
        lerp = LerpDataSearch(areaNo, areaSuffix, area->area_no, area->camera_no);
        if (lerp && lerp->enable == 1) {
            interp.set(lerp->frame, &cur);
        }
    } else {
        interp.frame = 0;
    }

    if (m_system_flag & 2) {
        if (!(rec->area->attr & 8)) {
            d = data;
            for (r = (CameraAreaRec*) (d + 1), i = 0; i < d->numArea; r++, i++) {
                if (r->area->attr & 8) {
                    r->area->enable = 0;
                }
            }
        }
        m_system_flag &= ~2;
    }

    if (area_rec != NULL) {
        CameraAreaInfo* a = area_rec->area;
        if (a->attr & 0x10) {
            a->enable = 0;
        } else if (a->attr & 8) {
            d = data;
            for (r = (CameraAreaRec*) (d + 1), i = 0; i < d->numArea; r++, i++) {
                if (r->area->attr & 8) {
                    r->area->enable = 0;
                }
            }
        }
    }

    areaNo = area->area_no;
    areaSuffix = area->camera_no;
    cameraNo = cut->camera_no;
    area_rec = rec;

    if (areaNo != -1) {
        if (m_system_flag & 0x10) {
            if (!(m_system_flag & 0x40)) {
                LightMgr.update(areaNo, -1);
            }
        } else if (!(area->attr & 0x80)) {
            LightMgr.update(areaNo, -1);
        }
    }
    m_state_flag |= 2;

    switch (cut->type) {
    case 0:
        r1 = 0;
        r0 = 1;
        break;
    case 1:
        r0 = 2;
        r1 = 0;
        break;
    case 2:
        r0 = 3;
        r1 = 0;
        break;
    case 3:
        r0 = 4;
        r1 = 0;
        break;
    case 4:
        r0 = 6;
        r1 = 0;
        break;
    case 5:
        r0 = 7;
        r1 = 0;
        break;
    case 6:
        size = HermiteExport(cut, CameraMotionBuffer);
        if (size > CAMERA_MOTION_BUFFER_SIZE) {
            pLog->err(0, 0, "CameraControl::HermiteExport() = 0x%04x > 0x%04x", size, CAMERA_MOTION_BUFFER_SIZE);
        }
        if (extra) {
            delete extra;
        }
        extra = new (m_Free) CameraMotion(CameraMotionBuffer, 0, 0, 0.0f);
        ((CameraMotion*) extra)->base_mat = NULL;
        r0 = 5;
        break;
    case 7:
        size = HermiteExport(cut, CameraMotionBuffer);
        if (size > CAMERA_MOTION_BUFFER_SIZE) {
            pLog->err(0, 0, "CameraControl::HermiteExport() = 0x%04x > 0x%04x", size, CAMERA_MOTION_BUFFER_SIZE);
        }
        if (extra) {
            delete extra;
        }
        extra = new (m_Free) CameraMotion(CameraMotionBuffer, 0, 0, 0.0f);
        r0 = 9;
        break;
    case 8: {
        // two pointers to qfps: `q` (blend_src, setAreaData, bindAreaCamera) keeps the addi; `p`
        // (blend_dst, setBlendData) and the direct `qfps.` calls share gcse's copy (mr r29,r30)
        CameraQuasiFPS* q = &m_QuasiFPS;
        CameraQuasiFPS* p = &m_QuasiFPS;
        if (q->blend_src && p->blend_dst) {
            p->setBlendData(q->blend_src, p->blend_dst);
        }
        q->setAreaData(area_rec->cut);
        q->bindAreaCamera(area_rec);
        if (prev_state == 10 && !(m_system_flag & 0x10)) {
            m_QuasiFPS.setBlendCount(10);
        } else {
            m_QuasiFPS.init();
            r1 = 0;
        }
        r0 = 10;
        break;
    }
    }
    m_system_flag &= ~0x10;
}

// 1 when the area is enabled and matches both attribute masks.
int areaAttr(CameraAreaInfo* area, u8 attr, u8 attr2)
{
    if ((area->enable & 1) && (area->attr & attr) && (area->attr2 & attr2)) {
        return 1;
    }
    return 0;
}

// 1 when `pos` is inside the area polygon (and, with attr 0x40, the facing `dir` is within 135
// degrees of the area's direction).
int areaHit(Vec* pos, CameraAreaInfo* area, f32 dir)
{
    int ret;

    if (area->attr & 4) {
        return 0;
    }
    if (area->attr & 0x40) {
        f32 d = area->dir;
        while (dir >= PI) {
            dir -= PI2;
        }
        while (dir < -PI) {
            dir += PI2;
        }
        dir -= d;
        while (dir >= PI) {
            dir -= PI2;
        }
        while (dir < -PI) {
            dir += PI2;
        }
        if (dir > 2.3561945f || dir < -2.3561945f) {
            return 0;
        }
    }
    if (area->num > 4) {
        ret = area_hit_pN(pos, area);
    } else {
        ret = area_hit_p3(pos, area);
    }
    return ret;
}

// Point in a convex area polygon of up to 4 points (height band base_y .. base_y + height).
int area_hit_p3(Vec* pos, CameraAreaInfo* area)
{
    Vec* p[3];  // the three corner pointers live in memory (stw/lwz around the calls)
    Vec v1, v2, v0, c0, c1;
    f32 y = pos->y + 100.0f;
    int i, n, n1, i0;

    if (y < area->base_y) {
        return 0;
    }
    if (y >= area->base_y + area->height) {
        return 0;
    }
    for (i = 0; i <= 1; i++) {
        n = area->num;
        n1 = n - 1;   // its own statement: `(i0 + n - 1)` is reassociated by fold into `(i0 - 1) + n`
        i0 = i + i + 1;
        i0 %= n;      // two sets of i0: loop.c does not strength-reduce the 2i+1 giv
        p[0] = &area->points[i0];
        p[1] = &area->points[(i0 + n1) % n];
        p[2] = &area->points[(i0 + 1) % n];
        PSVECSubtract(pos, p[0], &v0);
        PSVECSubtract(p[1], p[0], &v1);
        PSVECSubtract(p[2], p[0], &v2);
        PSVECCrossProduct(&v1, &v0, &c0);
        PSVECCrossProduct(&v2, &v0, &c1);
        // one `||` return: the shared `li r3,0` block starts with a label, so loop.c's exit-block move
        // leaves it inside the loop and jump2 folds the two entry returns into it
        if (c0.y > 0.0f || c1.y < 0.0f) {
            return 0;
        }
    }
    return 1;
}

// Point in an area polygon of more than 4 points (fan of triangles).
int area_hit_pN(Vec* pos, CameraAreaInfo* area)
{
    f32 y = pos->y + 100.0f;
    f32 a0, c, pz;
    f32 xi, zi, a, b, dx, dz, xmin, xmax, zmin, zmax;
    Vec *pi, *pj;
    Vec* pt[2];  // 8-byte pointer pair = one DImode pseudo (r7:r8); its halves are copied out before each
                 // load (`mr r9,r7; lfs 0(r9)`), which combine cannot fold through the subreg
    int i, count, fx, fz;

    if (y < area->base_y) {
        return 0;
    }
    if (y >= area->base_y + area->height) {
        return 0;
    }
    a0 = 1.0f;  // a named 1.0: cse cannot fold it inside the loop (fmsubs/fmadds with f5), pool order 100/1.0/0.0
    pz = pos->z;
    c = pos->x - pz;
    count = 0;
    for (i = 0; i < area->num; i++) {
        pi = &area->points[i];
        pj = &area->points[(i + 1) % area->num];
        dx = pj->x - pi->x;
        dz = pj->z - pi->z;
        pt[0] = pi;
        pt[1] = pj;
        if (dz != 0.0f) {
            a = dx / dz;
            b = pi->x - a * pi->z;
            if (a0 == a) {
                continue;
            }
            xi = (a0 * b - c * a) / (a0 - a);
            zi = (b - c) / (a0 - a);
        } else {
            zi = pi->z;
            xi = a0 * zi + c;
        }
        if (dx > 0.0f) {
            xmin = pt[0]->x;
            xmax = pt[1]->x;
            fx = 0;
        } else {
            xmin = pt[1]->x;
            xmax = pt[0]->x;
            fx = 1;
        }
        if (dz > 0.0f) {
            zmin = pt[0]->z;
            zmax = pt[1]->z;
            fz = 0;
        } else {
            zmin = pt[1]->z;
            zmax = pt[0]->z;
            fz = 1;
        }
        if (!(xi >= pos->x)) {
            continue;
        }
        if (fx == 0) {
            if (!(xi >= xmin && xi < xmax)) {
                continue;
            }
        } else {
            if (!(xi > xmin && xi <= xmax)) {
                continue;
            }
        }
        if (fz == 0) {
            if (!(zi >= zmin && zi < zmax)) {
                continue;
            }
        } else {
            if (!(zi > zmin && zi <= zmax)) {
                continue;
            }
        }
        count++;
    }
    if (count & 1) {
        return 1;
    }
    return 0;
}

// Per-frame area check: decides the cut attribute (1 calm / 2 battle, from EmMgr.isBattle with a
// Battle_delay, or forced by debug), and on an attribute change or when the player left the
// current area finds the first enabled area (normal 0x20 first, then the attribute-specific
// ones) containing the player and switches to it; with no area the shoulder camera (0xA) with
// the default offsets takes over.
void CameraControl::areaHitCheck()
{
    static u8 blink = 0;
    CameraDataHeader* d;
    CameraAreaRec* rec;
    CameraAreaRec* first;
    CameraAreaInfo* area;
    CameraCut* cut;
    u8 attr = 1;
    u8 old_attr;
    int old_area = areaNo;
    s8 i;

    if (DbgFlagChk(pG, DBG_CAM_AREA_OFF)) {
        return;
    }
    d = data;
    if (d == NULL) {
        cameraNo = -1;
        areaNo = -1;
        areaSuffix = -1;
        r0 = 0xA;  // LAST: its 0xa register stays live across the `flags_2C & 0x10` test (andi. r10, not r9)
        if (old_area != -1 || (m_system_flag & 0x10)) {
            m_QuasiFPS.init();
            m_system_flag &= ~0x10;
        }
        area_rec = NULL;
        return;
    }
    if (m_system_flag & 1) {
        if (m_system_flag & 0x10) {
            r0 = 0xA;
            m_QuasiFPS.init();
            m_system_flag &= ~0x10;
        }
        return;
    }
    if (cameraDataVersion((char*) d) <= 1) {
        cameraNo = -1;
        areaNo = -1;
        areaSuffix = -1;
        r0 = 0xA;  // LAST: its 0xa register stays live across the `flags_2C & 0x10` test (andi. r10, not r9)
        if (old_area != -1 || (m_system_flag & 0x10)) {
            m_QuasiFPS.init();
            m_system_flag &= ~0x10;
        }
        area_rec = NULL;
        return;
    }

    if (SubCharGetStatus() & 0x20000000) {
        attr = 2;
    } else {
        switch (pG->pl_type) {
        case 0:
            break;
        case 1:
            if (pG->language == 0) {
                attr = 4;
            }
            break;
        case 2:
            attr = 8;
            break;
        case 3:
            attr = 0x20;
            break;
        case 5:
            attr = 0x10;
            break;
        case 4:
            attr = 0x40;
            break;
        }
    }

    first = rec = (CameraAreaRec*) (d + 1);
    for (i = 0; i < d->numArea; i++, rec++) {
        area = rec->area;
        cut = rec->cut;
        if (areaAttr(area, 0x20, attr) && areaHit(&pPL->pos, area, pPL->ang.y)) {
            if ((m_system_flag & 0x10) || cut->camera_no != cameraNo) {
                switchCamera(rec);
            }
            return;
        }
    }

    old_attr = m_cut_attr;
    if (DbgFlagChk(pG, DBG_BATTLE_CAM)) {
        m_cut_attr = 2;
        if (blink++ & 0x18) {
            eprintf(27, 18, 22, 0, "[ BATTLE ]");
        }
    } else {
        if (Battle_delay > 0) {
            Battle_delay--;
        }
        if (Battle_delay == 0 && EmMgr.isBattle()) {
            m_cut_attr = 2;
        } else {
            m_cut_attr = 1;
        }
    }
    if (m_system_flag & 4) {
        m_cut_attr = 2;
    }
    if (old_attr != m_cut_attr) {
        m_system_flag |= 0x10;
    }

    if (areaNo != -1 && !(m_system_flag & 0x10)) {
        area = area_rec->area;
        if (areaAttr(area, m_cut_attr, attr) && areaHit(&pPL->pos, area, pPL->ang.y)) {
            return;
        }
    }

    rec = first;
    for (i = 0; i < d->numArea; i++, rec++) {
        area = rec->area;
        cut = rec->cut;
        if (areaAttr(area, m_cut_attr, attr) && areaHit(&pPL->pos, area, pPL->ang.y)) {
            if ((m_system_flag & 0x10) || cut->camera_no != cameraNo) {
                switchCamera(rec);
            }
            return;
        }
    }

    areaNo = -1;
    areaSuffix = -1;
    cameraNo = -1;
    area_rec = NULL;
    r0 = 0xA;  // LAST (see the reset arms above); store order found by permutation
    if (m_system_flag & 0x10) {
        m_system_flag &= ~0x10;
        m_QuasiFPS.bindDefaultCamera();
        m_QuasiFPS.init();
        r1 = 0;
        LightMgr.update(0, -1);
    } else if (old_area != -1) {
        CameraQuasiFPS* q = &m_QuasiFPS;
        if (prev_state == 0xA) {
            if (q->blend_src && q->blend_dst) {
                q->setBlendData(q->blend_src, q->blend_dst);
            }
            q->setBlendCount(10);
        }
        q->bindDefaultCamera();
        LightMgr.update(0, -1);
    }
}

// Room start: installs the room / core camera data (be_flag bit0 when present), resets to the
// shoulder camera with default offsets, no area, the behind-camera tuning constants, clears the
// attach cameras; rooms without data start in Wait with the area check off.
void CameraControl::roomInit()
{
    s8 ver;

    StaFlagOn(pG, STA_CAMERA);
    if (data == NULL) {
        be_flag = 0;
    } else {
        m_system_flag = 0x10;
        ver = cameraDataVersion((char*) data);
        switch (ver) {
        case 2:
            OSReport("CameraControl::roomInit(): Ver.02");
            break;
        case 1:
            pLog->warn(0, 0, "CameraControl::roomInit(): Ver.01");
            break;
        case 0:
            pLog->warn(0, 0, "CameraControl::roomInit(): Ver.00");
            break;
        case -1:
            pLog->warn(0, 0, "CameraControl::roomInit(): Empty!");
            break;
        }
        if (ver >= -1) {
            if (ver > 1) {
                if (ver > 4) {
                    goto clear;
                }
                be_flag |= 1;
            } else {
                be_flag |= 1;
                m_system_flag |= 1;
            }
        } else {
        clear:
            be_flag = 0;
        }
    }
    area_rec = NULL;
    r0 = 0xA;
    be_flag &= ~4;
    m_state_flag &= ~4;
    m_QuasiFPS.offsetCorrection();
    m_QuasiFPS.bindDefaultCamera();
    m_QuasiFPS.setFloorRatio(0.33333334f);
    m_QuasiFPS.init();
    areaNo = -1;
    areaSuffix = -1;
    cameraNo = -1;
    m_behind_fovy = 60.0f;
    m_side_play = 600.0f;
    m_back_play = 400.0f;
    m_ang_h_limit = 0.7853982f;
    m_ang_v_limit = 0.3926991f;
    m_quick_cnt = 0xF;
    m_key_speed = 0.001f;
    m_behind_A_ratio = 0.75f;
    clearAttachCamera();
    BitOn(m_system_flag, 2);
    if (SysFlagChk(pG, SYS_DOORDEMO)) {
        r0 = 0;
        BitOn(m_system_flag, 8);
    }
    m_pExtraCamera = 0;
    extra = NULL;
    interp.frame = 0;
    Check();
    Move();
    CameraMove();
    QuakeInit();
    memset(m_Free, 9, sizeof(m_Free));
    g_pToolCamData = NULL;
}

// Per-frame, before Move: runs the area check (unless disabled), then handles the fall / drop
// cases: while the player's hip is more than 500 units above his feet (falling, ladder) the
// controller switches to an event-driven camera state (0xB) and back when he lands, unless an
// extra / boss camera or a held cut is active.
void CameraControl::Check()
{
    Vec d;

    if (StaFlagChk(pG, STA_SUB_SCRN)) {
        return;
    }
    if (!(be_flag & 1)) {
        return;
    }
    if (be_flag & 4) {
        return;
    }
    m_state_flag &= ~2;
    prev_state = r0;
    if (!(m_system_flag & 8)) {
        areaHitCheck();
    }
    checkAttachCamera();
    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL)) {
        return;
    }
    if (StaFlagChk(pG, STA_EVENT)) {
        return;
    }
    if (m_pExtraCamera != 0) {
        return;
    }
    if (m_state_flag & 4) {
        return;
    }
    if (pPL->p2A4 && ((EmWork2A4*) pPL->p2A4)->x5) {
        return;
    }
    PSVECSubtract(&pPL->getPartsPtr(1)->world, &pPL->pos, &d);
    if (r0 != 0xB) {
        if (d.y <= 500.0f) {
            interp.set(3, &camera.param);
            r0 = 0xB;
            if (extra) {
                delete extra;
            }
            extra = new (m_Free) CameraLookAt(&camera);
        }
    } else {
        if (d.y > 500.0f) {
            interp.set(30, &camera.param);
            m_system_flag = 0x10;
        }
    }
}

// Per-frame camera computation: refreshes the aim point from the cut, runs the r0 routine (0
// Wait, 1 Fix, 2 Pan, 3 Track, 4 RailPan, 5 Motion, 6 RailBehind, 7 Free, 8 Debug, 9 UpCut,
// 0xA shoulder, 0xB.. the cCamera extras: 0xC binocular, 0xD look-down, 0xF push object, 0x10
// scope, 0x11 attached motion) into `cur`, applies the cut interpolation and the smoothing, and
// rebuilds `camera`.
void CameraControl::Move()
{
    static f32 gain = 2.0f;
    f32 water_y;
    f32 t;
    f32 lim;

    if (StaFlagChk(pG, STA_SUB_SCRN)) {
        return;
    }
    if (!(be_flag & 1)) {
        return;
    }
    m_state_flag &= ~1;
    if (area_rec) {
        CalcAim(area_rec->cut);
    }
    switch (r0) {
    case 0:
        r0_Wait();
        break;
    case 1:
        r0_Fix();
        break;
    case 2:
        r0_Pan();
        break;
    case 3:
        r0_Track();
        break;
    case 4:
        r0_RailPan();
        break;
    case 5:
        CamSmth.m_ratio = 0.0f;
        extra->move();
        cur = extra->param;
        if (((CameraMotion*) extra)->end == 1) {
            if (extra) {
                delete extra;
            }
            r0 = 0;
        }
        break;
    case 6:
        r0_RailBehind();
        break;
    case 7:
        r0_Free();
        break;
    case 8:
        r0_Debug();
        break;
    case 9:
        r0_UpCut();
        break;
    case 0xA:
        m_QuasiFPS.move();
        cur = m_QuasiFPS.cam.param;
        break;
    case 0xC:
        CamSmth.m_ratio = 0.0f;
        extra->move();
        cur = extra->param;
        break;
    case 0x10:
    case 0x11:
        CamSmth.m_ratio = 0.0f;
        extra->move();
        cur = extra->param;
        break;
    case 0xB:
    case 0xF:
        extra->move();
        cur = extra->param;
        break;
    case 0xD:
        CamSmth.m_ratio = 0.0f;
        extra->move();
        cur = extra->param;
        break;
    default:
        r0_Debug();
        break;
    }

    if (!StaFlagChk(pG, STA_EVENT)) {
        if (GetWaterHeight(&cur.pos, &water_y)) {
            t = sinf(cur.fovy * PI / 360.0f) / cosf(cur.fovy * PI / 360.0f);
            lim = gain * (ZNEAR * t * 1.3333334f) + water_y;
            if (cur.pos.y < lim) {
                cur.pos.y = lim;
            }
        }
    }
    interp.move(&cur);
    if (interp.frame != 0) {
        CamSmth.m_flag &= ~1;
    }
    CamSmth.move(&interp.param);
    camera.param = *CamSmth.getParam();
    CameraSetOrientationRoll(&camera);
    if (!DbgFlagChk(pG, DBG_DBG_CAM) && (m_state_flag & 4)) {
        pG->Cam = CamCtrl.camera;
    }
}

// The aim point: player position + the cut's aim offset (flags bit0) or the default (1000 up).
void CameraControl::CalcAim(CameraCut* cut)
{
    static Vec offset0 = {0.0f, 1000.0f, 0.0f};

    switch (r0) {
    case 0:
    case 1:
    case 5:
    case 9:
        break;
    default:
        if (cut->flags & 1) {
            PSVECAdd(&pPL->pos, &cut->aim_ofs, &Aim);
        } else {
            PSVECAdd(&pPL->pos, &offset0, &Aim);
        }
        break;
    }
}

// Always 0 (unused pitch query).
f32 CameraControl::getCameraPitch()
{
    return 0.0f;
}

// Starts an interpolation of `f` frames from camera parameters `p`.
void CameraInterpolation::set(int f, CameraParam* p)
{
    frame = f;
    param = *p;
}

// One step toward the target parameters `p`: param moves 1 / frame of the remaining distance
// each frame; when frame reaches 0 it snaps to `p`.
void CameraInterpolation::move(CameraParam* p)
{
    CameraParam tmp;
    f32 r, s;

    if (frame != 0) {
        r = 1.0f / (f32) frame;
        s = 1.0f - r;
        tmp = *p;
        PSVECScale(&param.pos, &param.pos, s);
        PSVECScale(&param.at, &param.at, s);
        param.roll *= s;
        param.fovy *= s;
        PSVECScale(&p->pos, &p->pos, r);
        PSVECScale(&p->at, &p->at, r);
        p->roll *= r;
        p->fovy *= r;
        PSVECAdd(&p->pos, &param.pos, &param.pos);
        PSVECAdd(&p->at, &param.at, &param.at);
        param.roll += p->roll;
        param.fovy += p->fovy;
        frame--;
    } else {
        frame = 0;
        param = *p;
    }
}

// Resets the smoothing state to `p`.
void CameraSmooth::init(CameraParam* p)
{
    param = *p;
}

// Exponential smoothing: param = m_ratio * old + (1 - m_ratio) * p (the quake offset is removed
// from the old value first); a set reinit flag snaps to `p`.
void CameraSmooth::move(CameraParam* p)
{
    Vec tmp;

    if (m_flag & 1) {
        m_flag &= ~1;
        init(p);
        return;
    }
    PSVECAdd(&param.pos, &pG->quake_ofs, &param.pos);
    PSVECAdd(&param.at, &pG->quake_ofs, &param.at);
    PSVECScale(&param.pos, &param.pos, m_ratio);
    PSVECScale(&p->pos, &tmp, 1.0f - m_ratio);
    PSVECAdd(&param.pos, &tmp, &param.pos);
    PSVECScale(&param.at, &param.at, m_ratio);
    PSVECScale(&p->at, &tmp, 1.0f - m_ratio);
    PSVECAdd(&param.at, &tmp, &param.at);
    param.roll *= m_ratio;
    param.roll = p->roll * (1.0f - m_ratio) + param.roll;
    param.fovy *= m_ratio;
    param.fovy = p->fovy * (1.0f - m_ratio) + param.fovy;
}

// r0 == 0: idle (an event / room owns pG->Cam).
void CameraControl::r0_Wait()
{
}

// r0 == 8: the debug behind camera (Debug_flg): a fixed offset behind / above the player, pulled
// in front of walls.
void CameraControl::r0_Debug()
{
    Vec a;
    Vec b;
    Vec c;
    Vec unused[2];  // 0x18-byte frame slot between c and m in the original
    Mtx m;
    CameraParam p;
    Vec hit;
    const Vec campos_ofs = {0.0f, 1900.0f, -2000.0f};
    const Vec target_ofs = {0.0f, 1000.0f, 0.0f};
    Camera* cam = &camera;
    f32 rate;

    switch (r1) {
    case 0:
        this->campos_ofs = campos_ofs;
        this->target_ofs = target_ofs;
        PSMTXMultVec(pPLS->mat, &this->campos_ofs, &p.pos);
        PSVECAdd(&pPL->pos, &this->target_ofs, &p.at);
        p.roll = 0.0f;
        p.fovy = 55.0f;
        cur = p;
        CamSmth.m_flag |= 1;
        r1++;
        break;
    case 1: {
        JOY* joy = &Joy[0];
        Vec* dp = &this->campos_ofs;
        Vec* da = &this->target_ofs;

        if (joy->substickX != 0) {
            PSMTXRotRad(m, 'y', (f32) joy->substickX * 0.05f * DEG);
            PSMTXMultVec(m, dp, dp);
            PSMTXMultVec(m, da, da);
        }
        if (joy->substickY != 0) {
            Vec up = {0.0f, 1.0f, 0.0f};

            PSVECCrossProduct(dp, &up, &up);
            PSMTXRotAxisRad(m, &up, (f32) joy->substickY * 0.05f * DEG);
            PSMTXMultVec(m, dp, dp);
            PSMTXMultVec(m, da, da);
        }
        rate = 0.8f;
        if (joy->on == 0) {
            if (counter_58++ > 30) {
                rate = 0.8f * 1.2f;  // 0x3F75C290 (0.96f is 0x3F75C28F)
            }
        } else {
            counter_58 = 0;
        }
        PSVECAdd(&pPL->pos, dp, &a);
        PSVECAdd(&pPL->pos, da, &b);
        PSVECScale(&cam->param.pos, &cam->param.pos, rate);
        PSVECScale(&a, &c, 1.0f - rate);
        PSVECAdd(&cam->param.pos, &c, &cam->param.pos);
        PSVECScale(&cam->param.at, &cam->param.at, rate);
        PSVECScale(&b, &c, 1.0f - rate);
        PSVECAdd(&cam->param.at, &c, &cam->param.at);
        if (SatMgr.hitCheck(&cam->param.at, &cam->param.pos, &hit, NULL, 0x8000, 0)) {
            cam->param.pos = hit;
        }
        cur = cam->param;
        break;
    }
    }
}

// r0 == 1: fixed camera at the cut's key looking at the aim point (keeps the previous roll / fov
// when the cut has no key).
void CameraControl::r0_Fix()
{
    Camera cam;

    CameraSetCutData(&cam, area_rec->cut);
    cur = cam.param;
    CamSmth.m_flag |= 1;
    r0 = 0;
}

// Start smoothing with `ratio`: `stw flags` is issued before `stfs ratio` only when the flags store
// is the LAST user of the CamSmth address in RTL order (it then carries the base register's
// REG_DEAD, weight -2 in sched1's tie-break), while the ratio store is a scalar reference so the
// ratio load stays below the flags load.
static inline void smoothStart(f32 ratio)
{
    u32 f = CamSmth.m_flag;
    FSet(CamSmth.m_ratio, ratio);
    CamSmth.m_flag = f | 1;
}

// r0 == 2: fixed position panning to follow the aim point; a multi-key cut turns into a rail
// (Parametrize + searchRail + BSpline).
void CameraControl::r0_Pan()
{
    CameraParam p;
    CameraCut* cut = area_rec->cut;

    switch (r1) {
    case 0:
        p.pos = *cut->pos;
        p.roll = *cut->roll;
        p.fovy = *cut->fovy;
        p.at = Aim;
        cur = p;
        smoothStart(smooth_ratio[1]);
        r1++;
    case 1:
        p.pos = camera.param.pos;
        p.roll = camera.param.roll;
        p.fovy = camera.param.fovy;
        p.at = Aim;
        cur = p;
        break;
    }
}

// r0 == 3: the camera slides along the cut's B-spline rail to the point nearest the aim and looks
// at the aim.
void CameraControl::r0_Track()
{
    Camera cam;
    CameraBSpline* bs = &CamBSpline;
    CameraCut* cut = area_rec->cut;

    switch (r1) {
    case 0:
        Parametrize(cut, bs);
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cur = cam.param;
        smoothStart(smooth_ratio[2]);
        r1++;
        break;
    case 1:
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cur = cam.param;
        if (pGS->debug_mode == 0xF) {  // struct view: the pG load stays below the copy's stores
            debugDrawRail(cut);
        }
        break;
    }
}

// r0 == 4: rail camera whose target is the aim point (rail evaluated every frame).
void CameraControl::r0_RailPan()
{
    Camera cam;
    CameraBSpline* bs = &CamBSpline;
    CameraCut* cut = area_rec->cut;

    switch (r1) {
    case 0:
        Parametrize(cut, bs);
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cam.param.at = Aim;
        cur = cam.param;
        smoothStart(smooth_ratio[2]);
        r1++;
        break;
    case 1:
        searchRail(bs, cut, &Aim, 0);
        BSpline(bs, &cam, 0);
        cam.param.at = Aim;
        cur = cam.param;
        if (pGS->debug_mode == 0xF) {  // struct view: the pG load stays below the copy's stores
            debugDrawRail(cut);
        }
        break;
    }
}

// r0 == 9: placeholder (the up-cut camera is the CameraMotion extra started by switchCamera).
void CameraControl::r0_UpCut()
{
}

// r0_RailBehind: argument addresses substituted into the hard-register sets (see COMPILER-DIFF #3 there).
static inline void VecLinComb(Vec* a, Vec* b, f32 s, f32 t, Vec* out)
{
    VecLinearCombination(a, b, s, t, out);
}

// r0 == 6: the behind-the-player camera on a rail: C-stick looks around within the h / v angle
// limits (quick snap to a limit with a short tap), the camera slides along the rail behind the
// player with side / back play zones, fov m_behind_fovy, smoothing m_behind_A_ratio, pulled in
// by cameraHitCheck.
void CameraControl::r0_RailBehind()
{
    static Camera camera_old;
    static f32 move_z;
    static Vec campos_ofs0 = {0.0f, 1800.0f, -1200.0f};
    static Vec target_ofs0 = {0.0f, 1550.0f, 0.0f};
    static Vec pos_old;
    static int init_flg;
    static int edge_camera;
    static int c_rno;
    static int key_flg;
    static Vec ang;
    static int nI = 1;
    static int mI = 2;
    static f32 rate = 0.95f;
    Camera cam;
    Mtx m;
    Mtx inv;
    Camera* c = &camera;
    CameraCut* cut = area_rec->cut;
    Vec xaxis = {1.0f, 0.0f, 0.0f};
    Vec yaxis = {0.0f, 1.0f, 0.0f};
    Vec zaxis = {0.0f, 0.0f, 1.0f};
    Vec dir;
    Vec v;
    Vec d;
    Vec p0;
    Vec p1;
    Vec p2;
    Vec q;
    Vec q2;
    Vec hit;
    Vec floor;
    Vec a;
    int reset = 0;
    CameraBSpline* bs = &CamBSpline;
    JOY* joy = &Joy[0];
    int moved;
    int edge;
    f32 t;
    f32 k;
    f32 n;
    f32 mm;

    switch (r1) {
    case 0:
        Parametrize(cut, bs);
        if (cut->flags & 1) {
            this->campos_ofs = cut->aim_ofs;
            this->target_ofs = *(Vec*) &cut->floor_ratio;
        } else {
            this->campos_ofs = campos_ofs0;
            this->target_ofs = target_ofs0;
        }
        pos_old = pPL->pos;
        memclr_asm(&camera_old, sizeof(Camera));
        r2 = 0;
        r1++;
        edge_camera = 0;
        init_flg = 1;
        ang.x = ang.y = ang.z = 0.0f;
        key_flg = 0xFF;
        c_rno = 0;
        reset = 1;
    case 1:
        if (c_rno == 0) {
            if (joy->trg & 0xF00000) {
                if (joy->trg & 0x800000) {
                    if (key_flg == 2) {
                        key_flg = 0;
                    } else {
                        key_flg = 1;
                    }
                }
                if (joy->trg & 0x400000) {
                    if (key_flg == 1) {
                        key_flg = 0;
                    } else {
                        key_flg = 2;
                    }
                }
                if (joy->trg & 0x100000) {
                    if (key_flg == 4) {
                        key_flg = 0;
                    } else {
                        key_flg = 3;
                    }
                }
                if (joy->trg & 0x200000) {
                    if (key_flg == 3) {
                        key_flg = 0;
                    } else {
                        key_flg = 4;
                    }
                }
                c_rno++;
            }
            if (joy->trg & 0x200) {
                ang.x = ang.y = ang.z = 0.0f;
                key_flg = 0;
            }
        } else {
            if (joy->on & 0xF00000) {
                ang.y -= (f32) joy->substickX * m_key_speed;
                ang.x -= (f32) joy->substickY * m_key_speed;
                ang.y = ang.y < -m_ang_h_limit ? -m_ang_h_limit : (ang.y > m_ang_h_limit ? m_ang_h_limit : ang.y);
                ang.x = ang.x < -m_ang_v_limit ? -m_ang_v_limit : (ang.x > m_ang_v_limit ? m_ang_v_limit : ang.x);
                c_rno++;
            } else {
                if (c_rno < m_quick_cnt) {
                    ang.x = 0.0f;
                    ang.y = 0.0f;
                    switch (key_flg) {
                    case 0:
                        break;
                    case 1:
                        ang.x = -m_ang_v_limit;
                        break;
                    case 2:
                        ang.x = m_ang_v_limit;
                        break;
                    case 3:
                        ang.y = m_ang_h_limit;
                        break;
                    case 4:
                        ang.y = -m_ang_h_limit;
                        break;
                    }
                }
                c_rno = 0;
            }
        }
        moved = 0;
        if (PSVECDistance(&pos_old, &pPL->pos) > 50.0f) {
            moved = 1;
        }
        searchRail(bs, cut, &Aim, 0);
        edge = 0;
        if (cut->flags & 4) {
            if (bs->t == 0.0f || (f32) (cut->num - 1) == bs->t) {
                cam = camera_old;
                edge = 1;
            }
        }
        if (edge_camera != 0) {
            if (edge == 0) {
                edge_camera = 0;
            }
        } else {
            if (edge == 1) {
                edge_camera = 1;
            }
            if ((cut->flags & 8) && init_flg == 1) {
                edge_camera = 0;
                if (edge == 0) {
                    init_flg = 0;
                }
            }
        }
        t = bs->t;
        BSpline(bs, &cam, 0);
        p0 = cam.param.at;
        bs->t = t - 0.1f;
        if (bs->t < 0.0f) {
            bs->t = 0.0f;
        }
        BSpline(bs, &cam, 0);
        p1 = cam.param.at;
        bs->t = t + 0.1f;
        if (bs->t > (f32) (cut->num - 1)) {
            bs->t = (f32) (cut->num - 1);
        }
        BSpline(bs, &cam, 0);
        p2 = cam.param.at;
        PSVECSubtract(&p1, &p2, &dir);
        dir.y = 0.0f;
        switch (r2) {
        case 0:
            if (init_flg == 1 && edge_camera == 1) {
                PSVECSubtract(&pPL->pos, &p0, &v);
                if (PSVECDotProduct(&v, &dir) < 0.0f) {
                    PSVECScale(&dir, &dir, -1.0f);
                }
            } else {
                PSMTXRotRad(m, 'y', pPL->ang.y);
                PSMTXMultVecSR(m, &zaxis, &v);
                if (PSVECDotProduct(&v, &dir) < 0.0f) {
                    PSVECScale(&dir, &dir, -1.0f);
                }
                reset = 1;
            }
            r2++;
            break;
        case 1:
            PSVECSubtract(&pPL->pos, &c->param.pos, &v);
            if (PSVECDotProduct(&v, &dir) < 0.0f) {
                PSVECScale(&dir, &dir, -1.0f);
            }
            break;
        }
#line 2628 "D:/Bio4/Prog/cam_ctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECCrossProduct(&yaxis, &dir, &xaxis);
        m[0][0] = xaxis.x;
        m[1][0] = xaxis.y;
        m[2][0] = xaxis.z;
        m[0][1] = yaxis.x;
        m[1][1] = yaxis.y;
        m[2][1] = yaxis.z;
        m[0][2] = dir.x;
        m[1][2] = dir.y;
        m[2][2] = dir.z;
        m[0][3] = p0.x;
        m[1][3] = p0.y;
        m[2][3] = p0.z;
        if (edge_camera) {
            floor = p0;
            floor.y = EatMgr.getFloor(&floor, 600.0f, 100000.0f, NULL, 0);
        } else {
            floor = pPL->pos;
        }
        PSMTXMultVecSR(m, &this->campos_ofs, &cam.param.pos);
        PSVECAdd(&cam.param.pos, &floor, &cam.param.pos);
        PSMTXMultVecSR(m, &this->target_ofs, &cam.param.at);
        PSVECAdd(&cam.param.at, &floor, &cam.param.at);
        if (!(cut->flags & 1)) {
            cam.param.fovy = m_behind_fovy;
            cam.param.roll = 0.0f;
        } else {
            cam.param.roll = 0.0f;
        }
        PSMTXInverse(m, inv);
        PSMTXMultVec(inv, &cam.param.pos, &q);
        if (moved) {
            PSMTXMultVec(inv, &cam.param.at, &q2);
            q2.x = q.x;
            PSMTXMultVec(m, &q2, &cam.param.at);
        }
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            PSVECScale(&ang, &a, -1.0f);
        } else {
            a = ang;
        }
        n = (f32) nI;
        mm = (f32) mI;
        k = 1.0f / (mm + n);
        // COMPILER-DIFF: #3 fresh-addi arguments. The target recomputes the three pointer arguments of
        // this one call (`addi r3,r1,0xb8; addi r4,r1,0xac; addi r5,r1,0x220`) while every surrounding
        // call copies them from the live address registers (`mr r3,r28` ...). Inside this `if` the
        // `&x` arguments of a plain call are precomputed into pseudos and gcse PRE turns them into
        // copies of the reaching registers; through the inline wrapper integrate substitutes the
        // addresses straight into the hard-register argument sets, which PRE never touches.
        VecLinComb(&cam.param.at, &cam.param.pos, mm * k, n * k, &floor);
        PSVECSubtract(&cam.param.at, &cam.param.pos, &dir);
        dir.y = 0.0f;
        PSVECCrossProduct(&yaxis, &dir, &xaxis);
        MtxRotAxisPosRad(m, &xaxis, &floor, a.x);
        PSMTXMultVec(m, &cam.param.at, &cam.param.at);
        PSMTXMultVec(m, &cam.param.pos, &cam.param.pos);
        MtxRotAxisPosRad(m, &yaxis, &floor, a.y);
        PSMTXMultVec(m, &cam.param.at, &cam.param.at);
        PSMTXMultVec(m, &cam.param.pos, &cam.param.pos);
        PSMTXRotRad(m, 'y', pPL->ang.y);
        PSMTXMultVecSR(m, &zaxis, &v);
        if (PSVECDotProduct(&v, &dir) < 0.0f) {
            PSVECSubtract(&pos_old, &pPL->pos, &d);
            pos_old = pPL->pos;
            PSMTXMultVecSR(inv, &d, &d);
            FSet(move_z, move_z + d.z);
            if (move_z > m_back_play || move_z < -m_back_play) {
                if (edge_camera == 0) {
                    r2 = 0;
                }
            }
        } else {
            move_z = 0.0f;
        }
        if (SatMgr.hitCheck(&cam.param.at, &cam.param.pos, &hit, NULL, 0x8000, 0)) {
            cam.param.pos = hit;
        }
        if (edge_camera) {
            CamSmth.m_ratio = rate;
            cam.param.at = pPL->pos;
            cam.param.at.y += 1550.0f;
        } else {
            CamSmth.m_ratio = m_behind_A_ratio;
        }
        cur = cam.param;
        CamSmth.m_flag |= 1;
        if (reset == 1) {
            CamSmth.m_ratio = m_behind_A_ratio;
        }
        break;
    }
    pos_old = pPL->pos;
}

// Separate `on & bit` tests: fold merges `(on & a) || (on & b)` on one lvalue into one mask.
static inline u32 JoyOn(JOY* j, u32 bit)
{
    return j->on & bit;
}

// Button trigger test on a pad.
static inline u32 JoyTrg(JOY* j, u32 bit)
{
    return j->trg & bit;
}


// r0 == 7: the free behind camera: orbits the player at a fixed distance with C-stick yaw /
// pitch, recentres behind him when idle, fov m_behind_fovy, pulled in by cameraHitCheck.
void CameraControl::r0_Free()
{
    static Vec campos_ofs0 = {0.0f, 1800.0f, -1200.0f};
    static Vec target_ofs0 = {0.0f, 1550.0f, 0.0f};
    static Vec ang;
    static Mtx cam_mat;
    Camera cam;
    Mtx m;
    Vec hit;
    Vec nrm;
    Vec tmp;
    Vec unused[2];
    JOY* joy = &Joy[0];
    JOY* joy2 = joy;
    f32 rate;

    switch (r1) {
    case 0: {
        PSMTXIdentity(cam_mat);
        cam_mat[0][3] = pPL->mat[0][3];
        cam_mat[1][3] = pPL->mat[1][3];
        cam_mat[2][3] = pPL->mat[2][3];
        ang.x = 0.0f;
        ang.y = pPL->ang.y;
        ang.z = 0.0f;
        Vec xaxis = {1.0f, 0.0f, 0.0f};
        Vec yaxis = {0.0f, 1.0f, 0.0f};
        Vec tofs;
        Vec a;
        if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
            PSVECScale(&ang, &a, -1.0f);
        } else {
            a = ang;
        }
        tofs = target_ofs0;
        MtxRotAxisPosRad(m, &xaxis, &tofs, a.x);
        PSMTXMultVec(m, &campos_ofs0, &this->campos_ofs);
        PSMTXMultVec(m, &target_ofs0, &this->target_ofs);
        MtxRotAxisPosRad(m, &yaxis, &tofs, a.y);
        PSMTXMultVec(m, &this->campos_ofs, &this->campos_ofs);
        PSMTXMultVec(m, &this->target_ofs, &this->target_ofs);
        PSMTXMultVec(cam_mat, &this->campos_ofs, &cam.param.pos);
        PSMTXMultVec(cam_mat, &this->target_ofs, &cam.param.at);
        cam.param.roll = 0.0f;
        cam.param.fovy = m_behind_fovy;
        cur = cam.param;
        CamSmth.m_flag |= 1;
        r2 = 0;
        r1++;
    }
    case 1: {
        ang.y -= (f32) joy->substickX * 0.00125f;
        ang.x -= (f32) joy->substickY * 0.00125f;
        ang.x = ang.x < -0.7853982f ? -0.7853982f : (ang.x > PI * 0.35f ? PI * 0.35f : ang.x);
        ang.y = ang.y < -PI ? PI : (ang.y > PI ? -PI : ang.y);
        {
            cam_mat[0][3] = pPLS->mat[0][3];
            cam_mat[1][3] = pPLS->mat[1][3];
            cam_mat[2][3] = pPLS->mat[2][3];
            Vec xaxis = {1.0f, 0.0f, 0.0f};
            Vec yaxis = {0.0f, 1.0f, 0.0f};
            Vec tofs;
            Vec a;
            if (CfgFlagChk(pSys, CFG_AIM_REVERSE)) {
                PSVECScale(&ang, &a, -1.0f);
            } else {
                a = ang;
            }
            tofs = target_ofs0;
            MtxRotAxisPosRad(m, &xaxis, &tofs, a.x);
            PSMTXMultVec(m, &campos_ofs0, &this->campos_ofs);
            PSMTXMultVec(m, &target_ofs0, &this->target_ofs);
            MtxRotAxisPosRad(m, &yaxis, &tofs, a.y);
            PSMTXMultVec(m, &this->campos_ofs, &this->campos_ofs);
            PSMTXMultVec(m, &this->target_ofs, &this->target_ofs);
        }
        {
            int st = r2;

            asm("" : "+r"(st));  // COMPILER-DIFF 2: the original zero-extends the loaded byte again
            switch ((u8) st) {
            case 0:
                if (JoyTrg(joy, 0x200) || JoyOn(joy, 0x200) || JoyOn(joy, 0x20)) {
                    r2 = st + 1;
                }
                break;
            case 1: {
                Vec d;

                d.x = 0.0f - ang.x;
                d.y = pPL->ang.y - ang.y;
                d.z = 0.0f;
                VecRadLimit(&d);
                ang.y += d.y * 0.1f;
                ang.x += d.x * 0.1f;
                if (!JoyOn(joy, 0x200) && !JoyOn(joy, 0x20)) {
                    // the pairs fold to one halfword test each; the second pointer keeps fold
                    // from merging all four bytes into one word compare
                    if (PSVECMag(&d) < 0.05f || (joy->substickX != 0 || joy->substickY != 0) || (joy2->stickX != 0 || joy2->stickY != 0)) {
                        r2--;
                    }
                }
                break;
            }
            }
        }
        PSMTXMultVec(cam_mat, &this->campos_ofs, &cam.param.pos);
        PSMTXMultVec(cam_mat, &this->target_ofs, &cam.param.at);
        cam.param.roll = 0.0f;
        cam.param.fovy = m_behind_fovy;
        rate = 0.8f;
        PSVECScale(&cam.param.pos, &cam.param.pos, rate);
        PSVECScale(&cur.pos, &tmp, 1.0f - rate);
        PSVECAdd(&cam.param.pos, &tmp, &cam.param.pos);
        PSVECScale(&cam.param.at, &cam.param.at, rate);
        PSVECScale(&cur.at, &tmp, 1.0f - rate);
        PSVECAdd(&cam.param.at, &tmp, &cam.param.at);
        {
            Vec from = cam.param.at;
            Vec to = cam.param.pos;

            if (cameraHitCheck(&hit, &nrm, &from, &to)) {
                cam.param.pos = hit;
            }
        }
        cur = cam.param;
        break;
    }
    }
}

// Shoulder camera: delay before it snaps to the stored player matrix (search frames).
void CamCtrlShoulderSetSearchFrame(s16 frame)
{
    CamCtrl.m_QuasiFPS.m_search_frame = frame;
    CamCtrl.m_QuasiFPS.m_search_cnt = 0;
}

// Shoulder camera: the aim point used during the search delay.
void CamCtrlShoulderSetAim(Vec* aim)
{
    CamCtrl.m_QuasiFPS.m_Aim = *aim;
}

// Shoulder camera: clears the C-stick look angles.
void CameraControl::resetCameraAngle()
{
    CameraQuasiFPS* q = &CamCtrl.m_QuasiFPS;

    q->angle_y = 0.0f;
    q->angle_x = 0.0f;
}

// Shoulder camera: returns and clears the yaw look angle (the player turns by it).
f32 CameraControl::getCameraDirection()
{
    f32 dir = CamCtrl.m_QuasiFPS.angle_x;
    CamCtrl.m_QuasiFPS.angle_x = 0.0f;
    return dir;
}

// Fits the cut's keys (pos, at, roll, fov) with a B-spline of degree min(2, num - 1): solves the
// de Boor-Cox basis matrix for the control points (temporary MEM_ALLOC buffers).
void Parametrize(CameraCut* cut, CameraBSpline* bs)
{
    int i;
    f32* B;
    f32* Binv;
    f32* px;
    f32* py;
    f32* pz;
    f32* ax;
    f32* ay;
    f32* az;
    f32* roll;
    f32* fovy;

    bs->num = cut->num;
    if (bs->num > 1) {
#line 3058 "D:/Bio4/Prog/cam_ctrl.cpp"
        B = (f32*) MEM_ALLOC(sizeof(f32) * bs->num * bs->num, 1, 0xd);
        Binv = (f32*) MEM_ALLOC(sizeof(f32) * bs->num * bs->num, 1, 0xd);
        px = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        py = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        pz = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        ax = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        ay = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        az = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        roll = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        fovy = (f32*) MEM_ALLOC(sizeof(f32) * bs->num, 1, 0xd);
        bs->k = 2;
        if (bs->k > bs->num - 1) {
            bs->k = bs->num - 1;
        }
        for (i = 0; i < bs->num; i++) {
            px[i] = cut->pos[i].x;
            py[i] = cut->pos[i].y;
            pz[i] = cut->pos[i].z;
            ax[i] = cut->at[i].x;
            ay[i] = cut->at[i].y;
            az[i] = cut->at[i].z;
            roll[i] = cut->roll[i];
            fovy[i] = cut->fovy[i];
        }
        for (i = 0; i < bs->num; i++) {
            de_Boor_Cox(bs->num, NULL, bs->k, (f32) i, &B[bs->num * i]);
        }
        MtxNNInverse(bs->num, B, Binv);
        MtxNNMultVecSR(bs->num, bs->num, Binv, px, bs->px);
        MtxNNMultVecSR(bs->num, bs->num, Binv, py, bs->py);
        MtxNNMultVecSR(bs->num, bs->num, Binv, pz, bs->pz);
        MtxNNMultVecSR(bs->num, bs->num, Binv, ax, bs->ax);
        MtxNNMultVecSR(bs->num, bs->num, Binv, ay, bs->ay);
        MtxNNMultVecSR(bs->num, bs->num, Binv, az, bs->az);
        MtxNNMultVecSR(bs->num, bs->num, Binv, roll, bs->roll);
        MtxNNMultVecSR(bs->num, bs->num, Binv, fovy, bs->fovy);
        Mem_free(B);
        Mem_free(Binv);
        Mem_free(px);
        Mem_free(py);
        Mem_free(pz);
        Mem_free(ax);
        Mem_free(ay);
        Mem_free(az);
        Mem_free(roll);
        Mem_free(fovy);
    }
}

// Evaluates the rail at parameter bs->t into the camera's pos / at / roll / fov.
void BSpline(CameraBSpline* bs, Camera* cam, int)
{
    int i;

    memclr_asm(cam, sizeof(Camera));
    de_Boor_CoxF(bs->num, NULL, bs->t, bs->k, bs->basis);  // COMPILER-DIFF 1 (floats-first alias)
    for (i = 0; i < bs->num; i++) {
        cam->param.at.x += bs->basis[i] * bs->ax[i];
        cam->param.at.y += bs->basis[i] * bs->ay[i];
        cam->param.at.z += bs->basis[i] * bs->az[i];
        cam->param.pos.x += bs->basis[i] * bs->px[i];
        cam->param.pos.y += bs->basis[i] * bs->py[i];
        cam->param.pos.z += bs->basis[i] * bs->pz[i];
        cam->param.roll += bs->basis[i] * bs->roll[i];
        cam->param.fovy += bs->basis[i] * bs->fovy[i];
    }
}

// Finds the rail parameter nearest the aim point: projects the aim on every key segment of the
// cut's `at` polyline (falls back to the nearest key), storing t and the segment.
void searchRail(CameraBSpline* bs, CameraCut* cut, Vec* aim, int)
{
    Vec d;
    Vec v;
    f32 min = 10000000000.0f;
    int found = 0;
    int i;
    f32 dot;
    f32 s;
    f32 dist;

    for (i = 0; i < cut->num - 1; i++) {
        // One variable per value (each block-local with a single death): `dot0` for the first
        // product, `dot` for the second, `prod` tied to `dot` (`fmuls f31, f30, f31`).
        f32 dot0;
        f32 prod;

        PSVECSubtract(&cut->at[i + 1], &cut->at[i], &d);
        d.y = 0.0f;
        PSVECSubtract(aim, &cut->at[i], &v);
        v.y = 0.0f;
        dot0 = PSVECDotProduct(&d, &v);
        s = dot0 / PSVECMag(&d);
        PSVECSubtract(aim, &cut->at[i + 1], &v);
        v.y = 0.0f;
        dot = PSVECDotProduct(&d, &v);
        dot = dot / PSVECMag(&d);
        prod = s * dot;
        if (prod < 0.0f) {
            d.y = cut->at[i + 1].y - cut->at[i].y;
            PSVECScale(&d, &v, s / PSVECMag(&d));
            PSVECAdd(&v, &cut->at[i], &v);
            dist = PSVECDistance(aim, &v);
            if (dist < min) {
                min = dist;
                bs->t = (f32) i + s / PSVECMag(&d);
                bs->seg = i;
                found = 1;
            }
        }
    }
    if (found) {
        f32 min2 = 10000000000.0f;
        int seg = 0;

        for (i = 0; i < cut->num; i++) {
            PSVECSubtract(aim, &cut->at[i], &d);
            dist = PSVECMag(&d);
            if (dist < min2) {
                min2 = dist;
                seg = i;
            }
        }
        if (min > min2) {
            bs->seg = seg;
            bs->t = (f32) seg;
        }
    } else {
        f32 min2 = 10000000000.0f;

        for (i = 0; i < cut->num; i++) {
            PSVECSubtract(aim, &cut->at[i], &d);
            dist = PSVECMag(&d);
            if (dist < min2) {
                min2 = dist;
                bs->seg = i;
                bs->t = (f32) i;
            }
        }
    }
}

// Debug: draws the cut's rail (spline samples) and its keys.
void CameraControl::debugDrawRail(CameraCut* cut)
{
    static Vec Fc_old;
    static Vec Ft_old;
    CameraBSpline* bs = &CamBSpline;
    Vec fc;
    Vec ft;
    int i;
    int j;

    for (i = 0; i < 100; i++) {
        de_Boor_Cox(cut->num, NULL, bs->k, (f32) ((cut->num - 1) * i) / 100.0f + 0.0f, bs->basis);
        fc.x = 0.0f;
        fc.y = 0.0f;
        fc.z = 0.0f;
        ft.x = 0.0f;
        ft.y = 0.0f;
        ft.z = 0.0f;
        for (j = 0; j < cut->num; j++) {
            fc.x += bs->basis[j] * bs->px[j];
            fc.y += bs->basis[j] * bs->py[j];
            fc.z += bs->basis[j] * bs->pz[j];
            ft.x += bs->basis[j] * bs->ax[j];
            ft.y += bs->basis[j] * bs->ay[j];
            ft.z += bs->basis[j] * bs->az[j];
        }
        if (i > 0) {
            Draw_line3d(&Fc_old, &fc, 0xFF2020FF, 0);
            Draw_line3d(&Ft_old, &ft, 0xFF20FF20, 0);
        }
        Fc_old = fc;
        Ft_old = ft;
    }
}

CameraControl CamCtrl;
CameraBSpline CamBSpline;
CameraSmooth CamSmth;

// Stores the up-cut placement (sel 0 position, 1 angles, 2 scale) used by the up-cut motion
// camera.
void CameraControl::UpCutCall(int no, Vec* pos, Vec* at, Vec* up, int sel)
{
    switch (sel) {
    case 0:
        data = (CameraDataHeader*) pG->pCamCore;
        break;
    case 1:
        data = (CameraDataHeader*) pG->pCamRoom;
        break;
    }
    if (pos) {
        upcut_pos = *pos;
    }
    if (at) {
        upcut_ang = *at;
    }
    if (up) {
        upcut_scale = *up;
    }
    CutCall(no);
}

// Enters the push-object camera (r0 0xF, CameraPushObject extra).
void CameraControl::startPushObject()
{
    extra = new (m_Free) CameraPushObject();
    r0 = 0xF;
    AreaCheckOnOff(0);
}

// Leaves the push-object camera and returns to the area cameras.
void CameraControl::endPushObject()
{
    if (extra) {
        delete extra;
    }
    Comeback(0);
}

// Enters the look-down camera on enemy `em` (r0 0xD) from above the player; hides the HUD
// (Status_flg[0] 0x2000000 off).
void CameraControl::StartLookDownEm(void* em)
{
    Vec c;
    cModel* p[2];

    p[0] = pPL->getPartsPtr(0x20);
    p[1] = pPL->getPartsPtr(0x21);
    PSVECAdd(&p[0]->world, &p[1]->world, &c);
    PSVECScale(&c, &c, 0.5f);
    extra = new (m_Free) CameraLookDownEm(em, &c);
    r0 = 0xD;
    AreaCheckOnOff(0);
    StaFlagOff(pG, STA_SSCRN_ENABLE);
}

// Leaves the look-down camera, HUD back on.
void CameraControl::EndLookDownEm()
{
    if (extra) {
        delete extra;
    }
    Comeback(0);
    StaFlagOn(pG, STA_SSCRN_ENABLE);
}

// Enters the rifle scope camera (r0 0x10; Status_flg[0] 0x40 scope, 0x8000 first-person view).
void CameraControl::startScope(Vec* pos, Vec* at)
{
    if (!StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        StaFlagOn(pG, STA_SCOPE_CAMERA);
        StaFlagOn(pG, STA_LOOK_THROUGH);
        extra = new (m_Free) CameraScope(pos, at);
        r0 = 0x10;
        DpfFlagOn(pG, DPF_PL);
        AreaCheckOnOff(0);
    }
}

// Leaves the scope camera.
void CameraControl::endScope()
{
    if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        StaFlagOff(pG, STA_SCOPE_CAMERA);
        StaFlagOff(pG, STA_LOOK_THROUGH);
        DpfFlagOff(pG, DPF_PL);
        if (extra) {
            delete extra;
        }
        Comeback(0);
    }
}

// While scoped: the scope camera's pos / at (the rifle's shot line).
void CameraControl::getTrajectory(Vec* pos, Vec* at)
{
    if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        cCamera* c = extra;
        *pos = c->param.pos;
        *at = c->param.at;
    }
}

// Saves the scope zoom / pitch and reticle timers before the scope is closed.
void CameraControl::saveScopeParam()
{
    ((CameraScope*) extra)->getParam(&m_scope_zoom, &m_scope_ang_x);
    ((CameraScope*) extra)->m_id.save(0);
}

// Restores them when the scope is re-opened.
void CameraControl::loadScopeParam()
{
    ((CameraScope*) extra)->setParam(m_scope_zoom, m_scope_ang_x);
    ((CameraScope*) extra)->m_id.load(0);
}

// Limits the binocular view angles.
void CameraControl::SetBinocularRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up)
{
    ((CameraBinocular*) extra)->setRange(x_low, x_up, y_low, y_up);
}

// Enters the binocular camera (r0 0xC; Status_flg[0] 0x400 binocular, 0x8000 first person) with
// its HUD data.
void CameraControl::HoldBinocular(void* id_a, void* id_b, Vec* pos, Vec* at)
{
    StaFlagOn(pG, STA_BINOCULAR);
    StaFlagOn(pG, STA_LOOK_THROUGH);
    extra = new (m_Free) CameraBinocular(pos, at, id_a, id_b);
    r0 = 0xC;
    DpfFlagOn(pG, DPF_PL);
    AreaCheckOnOff(0);
}

// Leaves the binocular camera.
void CameraControl::LowerBinocular()
{
    StaFlagOff(pG, STA_BINOCULAR);
    StaFlagOff(pG, STA_LOOK_THROUGH);
    DpfFlagOff(pG, DPF_PL);
    if (extra) {
        delete extra;
    }
    Comeback(0);
}

// The binocular HUD data pointers.
void CameraControl::GetBinocularIDAddr(void** eff_addr, void** uwf_addr)
{
    *eff_addr = ((CameraBinocular*) extra)->id_a;
    *uwf_addr = ((CameraBinocular*) extra)->id_b;
}

// Plays a camera motion file (cutscene camera, r0 5) interpolating from the current camera over
// `frame` frames; flags the event camera (m_system_flag 0x28, Status_flg[2] 0x10000000).
void CameraControl::MotionSet(void* motion, int frame, f32 speed)
{
    BitOn(m_system_flag, 0x28);
    StaFlagOn(pG, STA_CUT_CHANGE);
    extra = new (m_Free) CameraMotion(motion, 0, 0, speed);
    ((CameraMotion*) extra)->base_mat = NULL;
    r0 = 5;
    interp.set(frame, &pG->Cam.param);
}

// 1 while a camera motion is playing (m_system_flag 0x20).
int CameraControl::IsMotionSet()
{
    if (m_system_flag & 0x20) {
        return 1;
    }
    return 0;
}

// 1 when no camera motion is set or the current one reached its end.
int CameraControl::IsMotionEnd()
{
    if (r0 != 5) {
        return 1;
    }
    return ((CameraMotion*) extra)->end == 1;
}

// Places the camera motion in the world through `mat` (event position).
void CameraControl::setMotionBaseMatPtr(Mtx* mat)
{
    ((CameraMotion*) extra)->base_mat = mat;
}

// The playing camera motion's work (frame / state).
void* CameraControl::getMotionInfoPtr()
{
    return &((CameraMotion*) extra)->m_info;
}

// Forgets all registered attach cameras (motion-driven cameras of models).
void CameraControl::clearAttachCamera()
{
    int i;

    m_attach_num = 0;
    m_p_attach_model_old = NULL;
    for (i = 0; i < 3; i++) {
        m_p_model[i] = NULL;
        m_p_attach[i] = NULL;
    }
}

// Registers (or replaces) the attach camera of `model` (up to 3).
void CameraControl::registAttachCamera(AttachCamera* cam, cModel* model)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (model == m_p_model[i]) {
            m_p_attach[i] = cam;
            return;
        }
    }
    for (i = 0; i < 3; i++) {
        if (m_p_model[i] == NULL) {
            m_attach_num++;
            m_p_model[i] = model;
            m_p_attach[i] = cam;
            return;
        }
    }
    pLog->err(0, 0, "registAttachCamera(): lack of ptr table.");
}

// Unregisters the attach camera of `model`.
void CameraControl::deleteAttachCamera(AttachCamera* cam, cModel* model)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (model == m_p_model[i] && cam == m_p_attach[i]) {
            m_attach_num--;
            m_p_model[i] = NULL;
            m_p_attach[i] = NULL;
            return;
        }
    }
}

// The model whose attach camera is active (any when `model` is NULL, else `model` if registered).
cModel* CameraControl::getAttachModel(cModel* model)
{
    int i;

    if (model == NULL) {
        for (i = 0; i < 3; i++) {
            if (m_p_model[i] != NULL) {
                return m_p_model[i];
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (model == m_p_model[i]) {
                return model;
            }
        }
    }
    return NULL;
}

// The active attach camera (any when `model` is NULL, else that model's).
AttachCamera* CameraControl::getAttachCamera(cModel* model)
{
    int i;

    if (model == NULL) {
        for (i = 0; i < 3; i++) {
            if (m_p_model[i] != NULL) {
                return m_p_attach[i];
            }
        }
    } else {
        for (i = 0; i < 3; i++) {
            if (model == m_p_model[i]) {
                return m_p_attach[i];
            }
        }
    }
    return NULL;
}

// struct-member view of pG with a direct symbol address (no `li rX, pG@sda21`): the original
// reloads pG and `extra` after every one of the four param copies below
extern GlobalWorkPtr pGW asm("pG");

// Per-frame: when a registered model's motion has an active camera track, switches to the
// attached-motion camera (r0 0x11, interpolating over the track's frame count) and back to the
// area cameras when it ends. Not while the scope is up.
void CameraControl::checkAttachCamera()
{
    static int inter_frame;
    cModel* em[3] = {NULL, NULL, NULL};
    cModel* model = NULL;
    AttachCamera* ac;
    int i;

    if (StaFlagChk(pG, STA_SCOPE_CAMERA)) {
        return;
    }
    if (m_state_flag & 4) {
        return;
    }
    switch (m_attach_num) {
    case 0:
        break;
    case 1:
        model = getAttachModel(NULL);
        break;
    default:
        for (i = 0; i < 2; i++) {
            model = getAttachModel(em[i]);
            if (model) {
                break;
            }
        }
        break;
    }
    if (model) {
        ac = getAttachCamera(model);
        if (m_p_attach_model_old != model) {
            BitOn(m_system_flag, 8);
            interp.set(ac->frame, &pG->Cam.param);
            r0 = 0x11;
            if (extra) {
                delete extra;
            }
            extra = new (m_Free) CameraAttachedToMotion(model);
            extra->param.pos = pGW.p->Cam.param.pos;
            extra->param.at = pGW.p->Cam.param.at;
            extra->param.roll = pGW.p->Cam.param.roll;
            extra->param.fovy = pGW.p->Cam.param.fovy;
        }
        inter_frame = ac->frame;
    } else if (m_p_attach_model_old) {
        BitSet(m_system_flag, 0x10);
        interp.set(inter_frame, &pG->Cam.param);
    }
    m_p_attach_model_old = model;
}

// The numeric version of a camera data file ("B40x" -> x); -1 when not camera data.
int cameraDataVersion(char* data)
{
    if (strncmp(data, "B404", 4) == 0) {
        return 4;
    }
    if (strncmp(data, "B403", 4) == 0) {
        return 3;
    }
    if (strncmp(data, "B402", 4) == 0) {
        return 2;
    }
    if (strncmp(data, "B401", 4) == 0) {
        return 1;
    }
    if (strncmp(data, "B400", 4) == 0) {
        return 0;
    }
    strncmp(data, "EMPT", 4);
    return -1;
}
