#include "types.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"
#include "vec.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "cam_qfps.h"
#include "db_cam.h"
#include "player.h"
#include "eprintf.h"
#include "dbmodule.h"
#include "light.h"
#include "atari.h"
#include "t_camera.h"
#include <stdio.h>
#include <string.h>

// Camera tool (t_camera REL, t_camera_data.cpp): the bridge between the tool pools (tcAdat / tcCdat /
// tcLdat) and the game's room camera data image (tcDataExport / tcDataImport, the format CamCtrl
// reads), the tool <-> game camera copies, the preview player step, the shoulder-camera (quasi-FPS)
// offset transfer and the colour-rotating drawing wrappers the editors use.

void tcGetFileName(char* path, int no, int flag)
{
    if (flag & 2) {
        if (flag & 1) {
            sprintf(path, "X:/Soft/Room/Etc/Core/core00.cam");
        } else {
            sprintf(path, "y:/Room/Etc/Core/core00.cam");
        }
    } else {
        if (flag & 1) {
            sprintf(path, "X:/Soft/Room/St%x/R%x%02x/r%x%02x%02d.cam", pG->stage_no, pG->stage_no, pG->room_no,
                    pG->stage_no, pG->room_no, no);
        } else {
            sprintf(path, "y:/Room/St%x/R%x%02x/r%x%02x%02d.cam", pG->stage_no, pG->stage_no, pG->room_no,
                    pG->stage_no, pG->room_no, no);
        }
    }
}

// Serialises the tool pools into a room camera data image (CameraDataHeader, per-area records with
// their cuts, lerps) at `buf`; returns the byte size.
int tcDataExport(u8* buf)
{
    CameraDataHeader* hdr = (CameraDataHeader*) buf;
    CameraAreaRec* rec;
    CameraAreaRec* r;
    // the target's `addi r31,buf,0x10` + `mr r25,r31` after the header stores and its loop-2 fovy
    // pointer in the same r31: one work pointer set at the top, copied into rec, reused as fovy
    // (a multi-set pseudo crossing the two calls -> the first callee-saved allocno, size exact)
    f32* fovy = (f32*) (buf + 0x10);
    CameraAreaInfo* area;
    CameraCut* cut;
    CameraLerp* lerp;
    // one CameraCut* for loops 2 and 4 and one CameraAreaInfo* for loops 1 and 5: gcse's `d + 1`
    // copies then merge into one pseudo per type (r30 / r4 in the target) and buf takes r29
    CameraCut* dc;
    CameraAreaInfo* da;
    Vec* vp;
    Vec* pos;
    u16* fp;
    TcAdat* a;
    TcCdat* c;
    TcLdat* l;
    int i;
    int j;
    int num;
    int size;
    // a static array, not a pointer local: the .rodata order is B404, EMPT (parsed first), and the
    // address high is computed at the strncpy (block 2), so `mr r3,buf` issues before `addi r4`
    // (a pointer local's high sits in block 0 and dies at the call setup: sched1 weight 0 vs 1)
    static const char tag[] = "B404";

    memclr_asm(buf, 0x10);
    if (*(u16*) &pTc->cdatNum == 0) {
        strncpy((char*) buf, "EMPT", 4);
        return 4;
    }
    strncpy((char*) buf, tag, 4);
    hdr->numCut = pTc->cdatNum;
    hdr->numArea = pTc->adatNum;
    hdr->numLerp = pTc->ldatNum;
    rec = (CameraAreaRec*) fovy;
    area = (CameraAreaInfo*) (rec + hdr->numArea);
    cut = (CameraCut*) (area + hdr->numArea);
    lerp = (CameraLerp*) (cut + hdr->numCut);
    vp = (Vec*) (lerp + hdr->numLerp);

    {
        da = area;
        for (i = 0; i < 0x60; i++) {
            a = &tcAdat[i];
            if (a->enable != 0xFF) {
                da->enable = a->enable;
                da->area_no = a->area_no;
                da->camera_no = a->cam_no;
                da->attr = tcTypeTbl[a->area_no][0];
                da->dir = a->dir;
                da->attr2 = a->attr2;
                da->attr3 = a->attr3;
                da->height = a->height;
                da->base_y = a->base_y;
                num = a->num;
                da->points = (Vec*) ((u8*) vp - buf);
                da->num = num;
                for (j = 0; j < a->num; j++) {
                    *vp++ = a->pt[j];
                }
                da++;
            }
        }
    }
    pos = (Vec*) vp;
    {
        dc = cut;
        for (i = 0; i < 0x40; i++) {
            TcCdat* cd = &tcCdat[i];
            if (cd->enable != 0xFF) {
                Vec* pp;
                Vec* at;
                f32* roll;
                dc->x0 = cd->enable;
                dc->camera_no = cd->cam_no;
                dc->type = cd->type;
                dc->num = cd->num;
                dc->flags = cd->flags;
                dc->aim_ofs = cd->aim_ofs;
                switch (cd->type) {
                case 4:
                    *(Vec*) &dc->floor_ratio = cd->u44.dir;
                    break;
                case 8:
                    dc->floor_ratio = cd->u44.floor;
                    break;
                }
                pp = pos;
                at = pp + cd->num;
                roll = (f32*) (at + cd->num);
                fovy = roll + cd->num;
                dc->pos = (Vec*) ((u8*) pp - buf);
                dc->at = (Vec*) ((u8*) at - buf);
                dc->roll = (f32*) ((u8*) roll - buf);
                dc->fovy = (f32*) ((u8*) fovy - buf);
                for (j = 0; j < cd->num; j++) {
                    *pp++ = cd->pos[j];
                    *at++ = cd->at[j];
                    *roll++ = cd->roll[j];
                    *fovy++ = cd->fovy[j];
                }
                pos = (Vec*) fovy;
                dc++;
            }
        }
    }
    {
        CameraLerp* d = lerp;
        for (i = 0; i < 0x40; i++) {
            l = &tcLdat[i];
            if (l->enable != 0xFF) {
                *d = *(CameraLerp*) l;
                d++;
            }
        }
    }
    fp = (u16*) pos;
    {
        dc = cut;
        for (i = 0; i < pTc->cdatNum; i++) {
            c = &tcCdat[i];
            if (c->enable != 0xFF) {
                if (c->type == 6 || c->type == 7) {
                    dc->frames = (u16*) ((u8*) fp - buf);
                    for (j = 0; j < c->num; j++) {
                        *fp++ = c->frame[j];
                    }
                }
                dc++;
            }
        }
    }
    {
        r = rec;  // r, da, size in this order: the preheader `mr r10,r25; mr r7,r28; subf r26` is LUID order
        da = area;
        size = (u8*) fp - buf;
        // `i++` in the header (the PRE'd `i + 1` shortens i's live range: i is allocated before
        // found), the cut walker is the shared `dc` (r8 in loops 2/4/5: one pseudo), and the area
        // offset is a raw word store: an INDIRECT_REF store is not MEM_IN_STRUCT_P, so the `pTc`
        // reload depends on it and issues after `extsb no` / `addi i` (no's load temp then takes r9)
        for (i = 0; i < pTc->adatNum; i++) {
            s8 no = da->area_no;
            dc = cut;
            int found = 0;
            *(u32*) &r->area = (u8*) da - buf;
            for (j = 0; j < pTc->cdatNum; j++, dc++) {
                if (no == dc->camera_no) {
                    r->cut = (CameraCut*) ((u8*) dc - buf);
                    found = 1;
                    r->type = tcTypeTbl[no][0];
                    break;
                }
            }
            if (found == 0) {
                r->cut = (CameraCut*) found;
                da->enable = found;
            }
            r++;
            da++;
        }
    }
    return size;
}

// Expands a room camera data image (any known version) into the tool pools; -1 on a bad header.
int tcDataImport(u8* buf)
{
    CameraDataHeader* hdr = (CameraDataHeader*) buf;
    CameraAreaRec* rec;
    CameraAreaInfo* area;
    CameraCut* cut;
    CameraLerp* lerp;
    u8 cnt[64];
    int ver;
    int lo;
    int i;
    int j;

    ver = cameraDataVersion((char*) buf);
    if (ver == -1) {
        pTc->adatNum = 0;
        return -1;
    }
    if (ver < -1) {
        pTc->adatNum = 0;
        return -1;
    }
    if (ver > 4) {
        pTc->adatNum = 0;
        return -1;
    }
    // `2` through a local: the tree folder would turn `ver < 2` into `ver <= 1`, but the target
    // shares one `cmpwi 2` (cr0 kept in r25) with the `ver <= 2` test inside the area loop.
    lo = 2;
    if (ver < lo) {
        pTc->adatNum = 0;
        return -1;
    }
    pTc->adatNum = 0;
    pTc->cdatNum = 0;
    pTc->ldatNum = 0;
    for (i = 0; i < 64; i++) cnt[i] = 0;
    rec = (CameraAreaRec*) (buf + 0x10);

    area = (CameraAreaInfo*) (rec + hdr->numArea);
    cut = (CameraCut*) (area + hdr->numArea);
    lerp = (CameraLerp*) (cut + hdr->numCut);

    {
        CameraAreaRec* r = rec;
        for (i = 0; i < hdr->numArea; i++) {
            if (r->cut) {
                tcTypeTbl[r->cut->camera_no][0] = r->type;
            } else {
                tcTypeTbl[r->area->area_no][0] = r->type;
            }
            r++;
        }
    }
    {
        CameraAreaInfo* s = area;
        for (i = 0; i < hdr->numArea; i++) {
            TcAdat* a = tcAdatNew();
            a->area_no = s->area_no;
            a->cam_no = s->camera_no;
            pTc->adatTypeNum[s->area_no]++;
            a->height = s->height;
            a->base_y = s->base_y;
            a->num = s->num;
            if (ver <= 2) {
                a->attr = 3;
                tcTypeTbl[s->area_no][0] = 3;
            } else {
                a->attr = s->attr;
            }
            a->dir = s->dir;
            if (ver <= 3) {
                a->attr2 = 1;
                a->attr3 = 0xFF;
            } else {
                a->attr2 = s->attr2;
                a->attr3 = s->attr3;
            }
            {
                Vec* pt = s->points;
                for (j = 0; j < s->num; j++) {
                    a->pt[j] = *pt++;
                }
            }
            s++;
        }
    }
    {
        CameraCut* s = cut;
        for (i = 0; i < hdr->numCut; i++) {
            TcCdat* c = tcCdatNew();
            c->cam_no = s->camera_no;
            cnt[s->camera_no]++;
            if (cnt[s->camera_no] != 1) {
                pLog->err(0, 0, "Camera[%02d] is duplicate.", s->camera_no);
            }
            c->type = s->type;
            c->num = s->num;
            c->aim_ofs = s->aim_ofs;
            c->flags = s->flags;
            switch (s->type) {
            case 4:
                c->u44.dir = *(Vec*) &s->floor_ratio;
                break;
            case 8:
                c->u44.floor = s->floor_ratio;
                break;
            }
            {
                Vec* pos = s->pos;
                Vec* at = s->at;
                f32* roll = s->roll;
                f32* fovy = s->fovy;
                u16* frames = s->frames;
                for (j = 0; j < s->num; j++) {
                    c->pos[j] = *pos++;
                    c->at[j] = *at++;
                    c->roll[j] = *roll++;
                    c->fovy[j] = *fovy++;
                    if (s->type == 6 || s->type == 7) {
                        c->frame[j] = *frames++;
                    }
                }
            }
            s++;
        }
    }
    {
        CameraLerp* s = lerp;
        for (i = 0; i < hdr->numLerp; i++) {
            TcLdat* l = tcLdatNew();
            *(CameraLerp*) l = *s;
            l->x5 = 0;
            s++;
        }
    }
    if (hdr->numArea != 0) {
        pTc->cdatNo = rec->area->area_no;
        pTc->adatNo = pTc->cdatNo;
        pTc->adatSuffix = 0;
        pTc->pAdat = tcAdatPtr(pTc->adatNo, pTc->adatSuffix);
    }
    return 0;
}

// Floor height used by the shoulder camera preview (fixed 100).
f32 tcGetFloor()
{
    return 100.0f;
}

struct TcPreviewWork {
    int blink;
    int x4;
};
static TcPreviewWork tcPreview = {8, 0};

// Preview: blinking [ PREVIEW ] with the current camera / area numbers, then the player moves.
void tcPlayerMove()
{
    if (tcPreview.blink <= 15) {
        eprintf(0xD8, 0xFC, 5, 0, "[ PREVIEW ]");
    }
    tcPreview.blink++;
    if (tcPreview.blink > 31) tcPreview.blink = 0;
    eprintf(0xD8, 0x10A, 0, 0, "C:%02d", pTc->cameraNo);
    eprintf(0x100, 0x10A, 0, 0, "A:%02d-%1d", pTc->areaNo, pTc->areaSuffix);
    pPL->move();
}

// Runs the debug camera (pad 2) on the tool camera.
void tcCameraDebugMove()
{
    tcToolCamera2GameCamera();
    CamDbg.move(&pG->Camera, &Joy[1], 0);
    tcGameCamera2ToolCamera();
}

// Copies pG->Camera into the tool camera.
void tcGameCamera2ToolCamera()
{
    pTc->cam = pG->Camera;
}

// Copies the tool camera into pG->Camera.
void tcToolCamera2GameCamera()
{
    pG->Camera = pTc->cam;
}

CAMERA tcGameCamera;

// Saves the game camera (tool entry).
void tcGameCameraStore()
{
    tcGameCamera = pG->Camera;
}

// Restores the saved game camera (tool exit).
void tcGameCameraLoad()
{
    pG->Camera = tcGameCamera;
}

// 3D line in RGBA colour (rotated into the ARGB word Draw_line3d takes).
void tcDrawLine3D(Vec* a, Vec* b, u32 color)
{
    Draw_line3d(a, b, (color >> 8) | (color << 24), 0);
}

// Wire sphere of radius r.
void tcDrawSphere(Vec* pos, u32 color, f32 r)
{
    Draw_sphere(pos, r, color, 1, 1);
}

// Filled quad in RGBA colour.
void tcDrawPoly(Vec* p, u32 color)
{
    Draw_poly(p, (color >> 8) | (color << 24), 1);
}

// Shoulder camera floor ratio into CamCtrl's quasi-FPS controller.
void tcSetBesideFloor(f32 ratio)
{
    tcCdatPtr(pTc->cdatNo)->u44.floor = ratio;
}

// Copies the shoulder camera ready / transition offset tables into the current cut's key data.
void tcSetBesideOffset(QfpsOfs (*ready)[3], QfpsOfs (*trans)[3])
{
    TcCdat* c = tcCdatPtr(pTc->cdatNo);
    int n = 0;
    int i;
    int j;
    // one `o` for both loops: the shared pseudo is live across loop 1's r9/r10/r11 temporaries,
    // so global alloc gives it r8 in loop 2 as well (a loop-local `o` takes r11 there)
    QfpsOfs* o;

    c->num = 24;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
            c->pos[n] = o->Campos;
            c->at[n] = o->Target;
            c->roll[n] = o->Roll;
            c->fovy[n] = o->Fovy;
            n++;
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
            c->pos[n++] = o->campos2;
        }
    }
}

// Applies the current cut's shoulder camera data (offset tables, floor ratio) to CamCtrl's
// quasi-FPS controller for the preview.
void tcSetBesideCamera()
{
    QfpsOfs ready[2][3];
    QfpsOfs trans[2][3];
    TcCdat* c = tcCdatPtr(pTc->cdatNo);
    int i;
    int j;
    int n = 0;

    CamCtrl.m_QuasiFPS.setAreaData(g_readyOfs[0], g_transOfs[0]);
    CamCtrl.m_QuasiFPS.getAreaData(ready, trans);
    if (c->num == 24) {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 3; j++, n++) {
                QfpsOfs* o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
                if (c->flags & 0x20) {
                    if (i > 1) continue;
                } else if (!(c->flags & 0x10)) {
                    if (i <= 1) continue;
                }
                o->Campos = c->pos[n];
                o->Target = c->at[n];
                o->Roll = c->roll[n];
                o->Fovy = c->fovy[n];
            }
        }
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 3; j++, n++) {
                QfpsOfs* o = i <= 1 ? &ready[i][j] : &trans[i - 2][j];
                if (c->flags & 0x20) {
                    if (i > 1) continue;
                } else if (!(c->flags & 0x10)) {
                    if (i <= 1) continue;
                }
                o->campos2 = c->pos[n];
                // dead test (o is re-set at the body top): the extra ref/live range lets `o` beat
                // the `&c->pos[n]` giv in global alloc (r8/r7 as the original); deleted at flow2
                if (c == 0) o = 0; // COMPILER-DIFF: #13 (global-alloc order, dead test)
            }
        }
    } else {
        c->num = n;
    }
    CamCtrl.m_QuasiFPS.setAreaData(ready, trans);
    CamCtrl.m_QuasiFPS.setFloorRatio(c->u44.floor);
}

// the split object ends .rodata with a 4-byte pad to 8 (the linker does not re-create it)
asm(".section .rodata; .balign 8; .text");
