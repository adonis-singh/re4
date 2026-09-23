// game/light_area: room light areas (D:/Bio4/Prog/light_area.cpp). The room's "SAR" block lists
// areas with a light number per character class (player / enemies / partner) and a power scale;
// every frame each character's EmLightArea is told which area light applies and eases its light
// scale towards the area power (or back to 1 outside). The player's weapon and rocket copy his.
#include "atari.h"
#include "em.h"
#include "area.h"
#include "player.h"
#include "pl_npc.h"
#include "math_sub.h"
#include "global.h"

// One light area (0xD8 bytes): the light each character kind gets scaled inside the area.
struct LightAreaData {
    u8 x0;
    u8 x1;
    u8 lightNoPl;    // 0x02  light no for the player (0xFF: none)
    u8 lightNoEm;    // 0x03  light no for the other characters
    u8 area[0x34];   // 0x04  AreaHitCheck data
    s8 power;        // 0x38  scale in percent
    u8 lightNoSub;   // 0x39  light no for the sub character
    u8 pad_3A[0xD8 - 0x3A];
};

struct LightAreaHed {
    u32 num;                // 0x00
    u8 pad_4[0x10 - 0x4];
    LightAreaData data[1];  // 0x10
};

static LightAreaHed* g_pLightAreaHed;

extern "C" {
void LightAreaInit();
int LightAreaDataLoad(LightAreaHed* p);
void LightAreaUpdate();
void LightAreaUpdateSub(cEm* em, int type);
}

// pl_wep.h view: the weapon object and the rocket a launcher carries
struct LightAreaWep {
    u8 pad_0[0x34];
    cEm* pObj;   // 0x34
};

struct LightAreaLauncher {
    u8 pad_0[0x384];
    cEm* rocket;  // 0x384
};

// The player's weapon object as a cEm (pl_wep.h WEP_OBJ reads the same field as cObjWep*); `em` is the player.
#define WEP_OBJ_EM() (((LightAreaWep*) ((cPlayer*) em)->Wep)->pObj)
#define WEP_ROCKET(w) (((LightAreaLauncher*) (w))->rocket)

// Sets a light-area flag bit (1 = active, 2 = inside an area).
static inline void LitAreaSet(EmLightArea* la, u32 bit)
{
    la->flags |= bit;
}

// Clears a light-area flag bit.
static inline void LitAreaReset(EmLightArea* la, u32 bit)
{
    la->flags &= ~bit;
}

// a light area that is not scaling starts from 1
static inline void LitAreaScaleInit(EmLightArea* la)
{
    if (la->chk(2) == 0) {
        la->scale = 1.0f;
    }
}

// Room init: no light area data.
void LightAreaInit()
{
    g_pLightAreaHed = 0;
}

// Binds the room's SAR block.
int LightAreaDataLoad(LightAreaHed* p)
{
    g_pLightAreaHed = p;
    return 1;
}

// Per-frame: updates the light area state of every alive character with litArea active (type 0
// player, 2 partner, 1 other enemies).
void LightAreaUpdate()
{
    u32 i;

    if (g_pLightAreaHed == 0) {
        return;
    }
    if (g_pLightAreaHed->num == 0) {
        return;
    }
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);
        int type;

        if (!em->isAlive()) {
            continue;
        }
        if (em->litArea.chk(1) != 1) {
            continue;
        }
        if (em == pPL) {
            type = 0;
        } else if (em == pSUB) {
            type = 2;
        } else {
            type = 1;
        }
        LightAreaUpdateSub(em, type);
    }
}

// Finds the first area containing the character (pos + 100 y) with a light for its type, sets
// litArea.lightNo and eases litArea.scale towards power/100 (or 1 when outside, clearing flag 2
// within 0.05); for the player also mirrors the state onto the held weapon object and, for the
// rocket launcher, its rocket.
void LightAreaUpdateSub(cEm* em, int type)
{
    static f32 lit_pow_mul = 0.3f;
    Vec pos;
    EmLightArea* la;
    LightAreaHed* hed;
    LightAreaData* d;
    int hit;
    u32 i;
    f32 rate;
    f32 scale;

    pos = em->pos;
    pos.y += 100.0f;
    hed = g_pLightAreaHed;
    d = hed->data;
    LitAreaScaleInit(&em->litArea);
    la = &em->litArea;
    hit = 0;
    rate = 0.0f;
    for (i = 0; i < hed->num; i++, d++) {
        // The in-loop re-assignment is a gcse-time set of `la` that stops cprop from folding
        // the preheader copy (`mr r30,r6` of the inline's &em->litArea) into its uses; loop.c
        // then hoists it and cse2 deletes it as a no-op, so the target's copy is all that remains.
        la = &em->litArea;
        if (type == 0 && d->lightNoPl == 0xFF) {
            continue;
        }
        if (type == 1 && d->lightNoEm == 0xFF) {
            continue;
        }
        if (type == 2 && d->lightNoSub == 0xFF) {
            continue;
        }
        if (AreaHitCheck(d->area, &pos) != 1) {
            continue;
        }
        hit = 1;
        rate = (f32) d->power / 100.0f;
        if (type == 0) {
            la->lightNo = d->lightNoPl;
        } else if (type == 1) {
            la->lightNo = d->lightNoEm;
        } else if (type == 2) {
            la->lightNo = d->lightNoSub;
        }
        break;
    }
    scale = la->scale;
    if (hit == 1) {
        scale += (rate - scale) * lit_pow_mul;
        la->flags |= 2;
    } else {
        scale += (1.0f - scale) * lit_pow_mul;
        if (fabsf(1.0f - scale) < 0.05f) {
            la->flags &= ~2;
        }
    }
    la->scale = scale;
    if (em == pPL) {
        if (WEP_OBJ_EM() != 0) {
            cEm* wep;

            LitAreaSet(&WEP_OBJ_EM()->litArea, 1);
            WEP_OBJ_EM()->litArea.scale = scale;
            WEP_OBJ_EM()->litArea.lightNo = la->lightNo;
            if (la->chk(2)) {
                LitAreaSet(&WEP_OBJ_EM()->litArea, 2);
            } else {
                LitAreaReset(&WEP_OBJ_EM()->litArea, 2);
            }
            wep = WEP_OBJ_EM();
            if (wep->id == 0x23) {
                if (WEP_ROCKET(wep) != 0) {
                    LitAreaSet(&WEP_ROCKET(wep)->litArea, 1);
                    WEP_ROCKET(wep)->litArea.scale = scale;
                    WEP_ROCKET(wep)->litArea.lightNo = la->lightNo;
                    if (em->litArea.chk(2)) {
                        LitAreaSet(&WEP_ROCKET(wep)->litArea, 2);
                    } else {
                        LitAreaReset(&WEP_ROCKET(wep)->litArea, 2);
                    }
                }
            }
        }
    }
}

asm(".section .sdata; .balign 8");
