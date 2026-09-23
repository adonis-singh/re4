// game/db_work.cpp: debug page 11, the "MODEL WORK VIEWER": browses the enemy, object and light
// work pools with the D-pad and prints the selected work's fields (be_flag, position, angles,
// routine numbers, ids, lights...) with a position marker and bounding boxes.

#include "types.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "math_sub.h"
#include "em.h"
#include "obj.h"
#include "obj02.h"
#include "obj18.h"
#include "scroll.h"
#include "db_work.h"

// game/dbmodule.cpp. dbmodule.h is not included: this unit was built with a by-value Vec prototype for
// Draw_sphere (the definition takes Vec*), and the header's conflicts with it.
extern "C" {
void Draw_pos(Vec* pos, int size);
void Draw_sphere(Vec pos, f32 r, int color, int zcmp, int zupd);
}

// obj18 work (cObj::work) as far as the viewer reads it
struct DbObj18Work {
    u8 pad_0[0x68];
    int type;        // 0x68
    u8 pad_6C[0xC];
    char name[0x30]; // 0x78
};

// Starts on enemy 0.
cDbWork::cDbWork()
{
    wkNo = wkType = 0;
}

// Per-frame on debug page 11: Up / Down cycle the pool (0 enemies, 1 objects, 2 lights), then
// the pool's display.
void cDbWork::move()
{
    if (pG->debug_mode != 11) {
        return;
    }
    eprintf(16, 14, 0, 0, "MODEL WORK VIEWER");
    if (Joy[0].rep & JOY_UP) {
        switch (wkType) {
        case 0:
            wkType = 2;
            break;
        case 1:
            wkType = 0;
            break;
        case 2:
            wkType = 1;
            break;
        }
    }
    if (Joy[0].rep & JOY_DOWN) {
        switch (wkType) {
        case 0:
            wkType = 1;
            break;
        case 1:
            wkType = 2;
            break;
        case 2:
            wkType = 0;
            break;
        }
    }
    switch (wkType) {
    case 0:
        dispEm();
        break;
    case 1:
        dispObj();
        break;
    case 2:
        dispLit();
        break;
    }
}

// Enemy view: Left / Right select the work; prints the model fields plus hp, hp_max, distance to
// the player and the list entry, marks the position.
void cDbWork::dispEm()
{
    cEm* em;

    eprintf(32, 28, 4, 0, "ENEMY %d", wkNo);
    em = EmMgr.at(wkNo);
    if (Joy[0].rep & JOY_RIGHT) {
        wkNo++;
    }
    if (Joy[0].rep & JOY_LEFT) {
        wkNo--;
    }
    wkNo = (wkNo + EmMgr.getArrayNum()) % EmMgr.getArrayNum();
    if (em->isAlive()) {
        dispModel(em, 4, 3);
        eprintf(32, 280, 0, 0, "HP       %d", em->hp);
        eprintf(32, 294, 0, 0, "HP MAX   %d", em->hp_max);
        eprintf(32, 308, 0, 0, "L PL     %f", SQRTF(em->l_pl));
        eprintf(32, 322, 0, 0, "EMSET NO %d", em->emset_no);
        Draw_pos(&em->pos, 500);
    }
}

// Object view: the model fields plus the scroll attribute / id (id 2) or the obj18 name / type.
void cDbWork::dispObj()
{
    cObj* obj;
    int x;
    int y;

    obj = ObjMgrWork(wkNo);
    eprintf(32, 28, 4, 0, "OBJ %d  [0x%08X]", wkNo, obj);
    if (Joy[0].rep & JOY_RIGHT) {
        wkNo++;
    }
    if (Joy[0].rep & JOY_LEFT) {
        wkNo--;
    }
    wkNo = (wkNo + ObjMgr.getArrayNum()) % ObjMgr.getArrayNum();
    if (obj->isAlive()) {
        dispModel(obj, 4, 3);
        x = 4;
        y = 20;
        if (obj->id == 2) {
            int id;
            eprintf(32, 280, 0, 0, "ATTR     %02X", ((cObjScr*) obj)->Attribute);
            y++;
            id = SmdGetWorkId(obj);
            if (id >= 0) {
                eprintf(32, 294, 0, 0, "SCR-ID  %3d", id);
            } else {
                eprintf(32, 294, 0, 0, "SCR-ID  ---");
            }
        }
        if (obj->id == 0x18) {
            DbObj18Work* w = (DbObj18Work*) ((cObj18*) obj)->free;
            eprintf(x * 8, y * 14, 0, 0, "NAME     %s", w->name);
            y++;
            eprintf(x * 8, y * 14, 0, 0, "TYPE     %2d", w->type);
        }
        Draw_pos(&obj->pos, 1000);
    }
}

// Common model dump at text column x / row y; A inverts the model colour, X squashes it, the C-
// stick up / down moves it +-1000 in y; draws the bounding boxes.
void cDbWork::dispModel(cModel* pMod, int x, int y)
{
    int color;

    x *= 8;
    eprintf(x, y * 14, 0, 0, "BE FLAG  %08X", pMod->be_flag);
    y++;
    eprintf(x, y * 14, 0, 0, "POSITION %7.0f %7.0f %7.0f", pMod->pos.x, pMod->pos.y, pMod->pos.z);
    y++;
    eprintf(x, y * 14, 0, 0, "ANGLE    %4.2f %4.2f %4.2f", pMod->ang.x, pMod->ang.y, pMod->ang.z);
    y++;
    eprintf(x, y * 14, 0, 0, "SCALE    %4.2f %4.2f %4.2f", pMod->scale.x, pMod->scale.y, pMod->scale.z);
    y++;
    eprintf(x, y * 14, 0, 0, "RTN NO   %02X %02X %02X %02X", pMod->r_no_0, pMod->r_no_1, pMod->r_no_2, pMod->r_no_3);
    y++;
    eprintf(x, y * 14, 0, 0, "ID       %02X", pMod->id);
    y++;
    eprintf(x, y * 14, 0, 0, "TYPE     %02X", pMod->type);
    y++;
    eprintf(x, y * 14, 0, 0, "nParts   %02X", pMod->nParts);
    y++;
    eprintf(x, y * 14, 0, 0, "SPEED    %7.0f %7.0f %7.0f", pMod->speed.x, pMod->speed.y, pMod->speed.z);
    y++;
    eprintf(x, y * 14, 0, 0, "pCldShMd %08X", pMod->pCldShMd);
    y++;
    eprintf(x, y * 14, 0, 0, "SHD COL  %02X", pMod->Shd_color);
    y++;
    eprintf(x, y * 14, 0, 0, "CullMode %d", pMod->CullMode);
    y++;
    eprintf(x, y * 14, 0, 0, "pModInfo %08X", pMod->pModelInfo);
    y++;
    eprintf(x, y * 14, 0, 0, "pShMdIfo %08X", pMod->pShadowModelInfo);
    y++;
    color = 0;
    if (pMod->LightInfo.getLightNum() > 5) {
        color = 0x16;
    }
    eprintf(x, y * 14, color, 0, "nLight   %d", pMod->LightInfo.getLightNum());
    if (Joy[0].on & JOY_A) {
        if (pMod->pModelInfo != NULL) {
            pMod->pModelInfo->color[0] = ~pMod->pModelInfo->color[0];
            pMod->pModelInfo->color[1] = ~pMod->pModelInfo->color[1];
            pMod->pModelInfo->color[2] = ~pMod->pModelInfo->color[2];
        }
    } else {
        if (pMod->pModelInfo != NULL) {
            pMod->pModelInfo->color[0] = 0xFF;
            pMod->pModelInfo->color[1] = 0xFF;
            pMod->pModelInfo->color[2] = 0xFF;
        }
    }
    if (Joy[0].on & JOY_X) {
        if (pMod->scale.y == 0.2f) {
            pMod->scale.y = 1.0f;
        } else {
            pMod->scale.y = 0.2f;
        }
    }
    if (Joy[0].trg & 0x800000) {
        pMod->pos.y += 1000.0f;
        pMod->matUpdate();
    }
    if (Joy[0].trg & 0x400000) {
        pMod->pos.y -= 1000.0f;
        pMod->matUpdate();
    }
    pMod->drawAllBoundingBox(pMod->pModelInfo);
}

// Light view: be_flag, position, attribute, and a sphere of its radius.
void cDbWork::dispLit()
{
    cLight* l;

    eprintf(32, 28, 4, 0, "LIGHT %d", wkNo);
    l = LightMgr.at(wkNo);
    if (Joy[0].rep & JOY_RIGHT) {
        wkNo++;
    }
    if (Joy[0].rep & JOY_LEFT) {
        wkNo--;
    }
    wkNo = (wkNo + LightMgr.getArrayNum()) % LightMgr.getArrayNum();
    if (l->isAlive()) {
        eprintf(32, 280, 0, 0, "BE FLAG  %08X", l->be_flag);
        eprintf(32, 294, 0, 0, "POSITION %7.0f %7.0f %7.0f", l->Pos.x, l->Pos.y, l->Pos.z);
        eprintf(32, 308, 0, 0, "ATTR     %02x", l->Attribute);
        Draw_sphere(l->World, l->Radius, -1, 1, 1);
    }
}
