// game/trans_lit: GX lighting set-up for the model draws. A model carries up to 8 cLight pointers
// picked by the light manager (LightInfo.pLight); LightSetModel turns them into GX light objects
// (position in camera space, colour x alpha scaled by the enemy's light area, attenuation by the
// light's type xD: 0 constant, 1 linear, 2 quadratic, 3 spot, 4 custom, 5 parallel, 6 spot-quad,
// 7 local ambient) and sets the ambient (scenery / effect / enemy ambient, plus the model's
// AddAmb) and material colours. The common*LightSet variants do the same for cloth, water and
// effects; LightDisable draws unlit.
#include "types.h"
#include "vec.h"
#include "gx.h"
#include "global.h"
#include "light.h"
#include "model.h"
#include "em.h"
#include "db_log.h"
#include "math_sub.h"
#include "main_mem.h"

extern "C" {
void LightSetInit();
void LightSetModel(cModel* m);
void commonClothLightSet(cLight** list, int n, Vec* pos, f32 size);
void commonWaterLightSet(cLight** list, int n, u32 alpha);
void commonEspLightSet(cLight** list, int n);
void lightSetConstant(cLight* l, GXLightObj* obj);
void lightSetLinear(cLight* l, GXLightObj* obj);
static void lightSetQuadratic(cLight* l, GXLightObj* obj);
void lightSetSpotlight(cLight* l, GXLightObj* obj);
void lightSetCustom(cLight* l, GXLightObj* obj);
void lightSetParallel(cLight* l, GXLightObj* obj);
void lightSetSpotQuad(cLight* l, GXLightObj* obj);
void lightSetLocalAmb(cLight* l, GXColor* amb);
void lightSetColor(GXLightObj* obj, cLight* l, cEm* em);
void lightSetAmbient(GXColor* col0);
void LightDisable();
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))


#define LIGHT_FUNC_TABLE                                                                              \
    static void (*funcLightParam[16])(cLight*, GXLightObj*) = {                                      \
        lightSetConstant, lightSetLinear,   lightSetQuadratic, lightSetSpotlight, lightSetCustom,    \
        lightSetParallel, lightSetSpotQuad, lightSetConstant,  lightSetConstant,  lightSetConstant,  \
        lightSetConstant, lightSetConstant, lightSetConstant,  lightSetConstant,  lightSetConstant,  \
        lightSetConstant,                                                                             \
    }

static GXLightObj lightObjBlack;
Vec obj_pos;
int obj_flag;
f32 obj_size;
const GXColor colZero = {0, 0, 0, 0};

// Boot: the black light object used by LightDisable.
void LightSetInit()
{
    GXInitLightColor(&lightObjBlack, colZero);
    GXInitLightPos(&lightObjBlack, 0.0f, 0.0f, 0.0f);
    GXInitLightDir(&lightObjBlack, 0.0f, 0.0f, 1.0f);
    GXInitLightAttn(&lightObjBlack, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
}

// Lighting for a model draw: unlit when LightInfo.Flag bit2; each of its lights (none when be_flag
// 0x8000) becomes a GX light by type (type 7 only raises the ambient), the light mask is loaded;
// self-lit models (data flags 0x40000000) use their colour as ambient; else the environment
// ambient chosen by EnableMask (0x10 scenery, 8 effects, else enemies) plus AddAmb (be_flag 8),
// material = the model colour.
void LightSetModel(cModel* pMod)
{
    LIGHT_FUNC_TABLE;
    GXLightObj lobj[8];
    Vec p;
    GXColor mat;
    GXColor amb;
    cModelInfo* info = pMod->pModelInfo;
    cModelData* data = info->model_addr;
    cLight** list = pMod->LightInfo.pLight;
    int n = (pMod->be_flag & 0x8000) ? 0 : 8;
    u32 mask;
    int i;
    cLightEnv* env;

    if (pMod->LightInfo.Flag & 4) {
        LightDisable();
        return;
    }
    obj_pos = pMod->pList->world;
    obj_size = pMod->LightInfo.Size.x > pMod->LightInfo.Size.y ? pMod->LightInfo.Size.x : pMod->LightInfo.Size.y;
    if ((pMod->LightInfo.Flag & 3) == 2) {
        obj_flag = 0;
    } else {
        obj_flag = 1;
    }
    mask = 0;
    amb.a = amb.b = amb.g = amb.r = 0;
    for (i = 0; i < n; i++) {
        cLight* l = list[i];

        if (l == NULL) {
            continue;
        }
        if (l->xD == 7) {
            lightSetLocalAmb(l, &amb);
            continue;
        }
        l->getPos(&p);
        mask |= 1 << i;
        PSMTXMultVec(pG->Camera.v_mat, &p, &p);
        GXInitLightPos(&lobj[i], p.x, p.y, p.z);
        lightSetColor(&lobj[i], l, (cEm*) pMod);
        GXInitLightDir(&lobj[i], 0.0f, 0.0f, 1.0f);
        if (l->xD > 7) {
            pLog->err(0, 0, "LIGHT() INVALIED TYPE %d", l->xD);
            memclr_asm(l, 0x154);
        }
        funcLightParam[l->xD](l, &lobj[i]);
        GXLoadLightObjImm(&lobj[i], 1 << i);
    }
    if (mask == 0) {
        GXLoadLightObjImm(&lightObjBlack, 1);
        mask = 1;
    }
    GXSetNumChans(1);
    if (data->flags & 0x40000000) {
        GXSetChanCtrl(0, 1, 1, 0, mask, 2, 1);
        GXSetChanCtrl(2, 0, 1, 1, 0, 2, 2);
        mat = *(GXColor*) info->color;
        GXSetChanMatColor(4, mat);
        return;
    }
    GXSetChanCtrl(0, 1, 0, 0, mask, 2, 1);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    if (pMod->LightInfo.EnableMask & 0x10) {
        amb.r = MAX(LightMgr.getEnvPtr()->AmbientScr.r, amb.r);
        amb.g = MAX(LightMgr.getEnvPtr()->AmbientScr.g, amb.g);
        amb.b = MAX(LightMgr.getEnvPtr()->AmbientScr.b, amb.b);
    } else if (pMod->LightInfo.EnableMask & 8) {
        amb.r = MAX(LightMgr.getEnvPtr()->AmbientEsp.r, amb.r);
        amb.g = MAX(LightMgr.getEnvPtr()->AmbientEsp.g, amb.g);
        amb.b = MAX(LightMgr.getEnvPtr()->AmbientEsp.b, amb.b);
    } else {
        amb.r = MAX(LightMgr.getEnvPtr()->AmbientEm.r, amb.r);
        amb.g = MAX(LightMgr.getEnvPtr()->AmbientEm.g, amb.g);
        amb.b = MAX(LightMgr.getEnvPtr()->AmbientEm.b, amb.b);
    }
    if (pMod->be_flag & 8) {
        amb.r += pMod->AddAmb_r;
        amb.g += pMod->AddAmb_g;
        amb.b += pMod->AddAmb_b;
    }
    lightSetAmbient(&amb);
    mat = *(GXColor*) info->color;
    GXSetChanMatColor(4, mat);
}

// Lighting for a cloth chain at `pos` (no distance fade, obj_flag 0): its lights, the scenery
// ambient, white material.
void commonClothLightSet(cLight** list, int n, Vec* pos, f32 size)
{
    LIGHT_FUNC_TABLE;
    GXLightObj lobj[8];
    Vec p;
    GXColor amb;
    u32 mask;
    int i;

    obj_flag = 0;
    obj_pos = *pos;
    obj_size = size;
    mask = 0;
    amb.a = amb.b = amb.g = amb.r = 0;
    for (i = 0; i < n; i++) {
        cLight* l = list[i];

        if (l == NULL) {
            continue;
        }
        if (l->xD == 7) {
            lightSetLocalAmb(l, &amb);
            continue;
        }
        l->getPos(&p);
        mask |= 1 << i;
        PSMTXMultVec(pG->Camera.v_mat, &p, &p);
        GXInitLightPos(&lobj[i], p.x, p.y, p.z);
        lightSetColor(&lobj[i], l, NULL);
        GXInitLightDir(&lobj[i], 0.0f, 0.0f, 1.0f);
        if (l->xD > 7) {
            pLog->err(0, 0, "LIGHT() INVALIED TYPE %d", l->xD);
            memclr_asm(l, 0x154);
        }
        funcLightParam[l->xD](l, &lobj[i]);
        GXLoadLightObjImm(&lobj[i], 1 << i);
    }
    if (mask == 0) {
        GXLoadLightObjImm(&lightObjBlack, 1);
        mask = 1;
    }
    GXSetNumChans(1);
    GXSetChanCtrl(0, 1, 0, 0, mask, 2, 1);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    amb.r = MAX(LightMgr.getEnvPtr()->AmbientScr.r, amb.r);
    amb.g = MAX(LightMgr.getEnvPtr()->AmbientScr.g, amb.g);
    amb.b = MAX(LightMgr.getEnvPtr()->AmbientScr.b, amb.b);
    lightSetAmbient(&amb);
    GXColor mat = {0xFF, 0xFF, 0xFF, 0xFF};
    GXSetChanMatColor(4, mat);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
}

// Lighting for the water surface: the lights with their alpha scaled by `alpha` / 256, scenery
// ambient, white material.
void commonWaterLightSet(cLight** pLightData, int data_num, u32 pow)
{
    LIGHT_FUNC_TABLE;
    GXLightObj lobj[8];
    Vec p;
    GXColor amb;
    u32 mask;
    int i;

    obj_flag = 0;
    mask = 0;
    amb.a = amb.b = amb.g = amb.r = 0;
    for (i = 0; i < data_num; i++) {
        cLight* l = pLightData[i];
        u8 a;

        if (l == NULL) {
            continue;
        }
        if (l->xD == 7) {
            lightSetLocalAmb(l, &amb);
            continue;
        }
        l->getPos(&p);
        mask |= 1 << i;
        PSMTXMultVec(pG->Camera.v_mat, &p, &p);
        GXInitLightPos(&lobj[i], p.x, p.y, p.z);
        a = l->DispCol.a;
        l->DispCol.a = (a * pow) >> 8;
        lightSetColor(&lobj[i], l, NULL);
        l->DispCol.a = a;
        GXInitLightDir(&lobj[i], 0.0f, 0.0f, 1.0f);
        if (l->xD > 7) {
            pLog->err(0, 0, "LIGHT() INVALIED TYPE %d", l->xD);
            memclr_asm(l, 0x154);
        }
        funcLightParam[l->xD](l, &lobj[i]);
        GXLoadLightObjImm(&lobj[i], 1 << i);
    }
    if (mask == 0) {
        GXLoadLightObjImm(&lightObjBlack, 1);
        mask = 1;
    }
    GXSetNumChans(1);
    GXSetChanCtrl(0, 1, 0, 0, mask, 2, 1);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    amb.r = MAX(LightMgr.getEnvPtr()->AmbientScr.r, amb.r);
    amb.g = MAX(LightMgr.getEnvPtr()->AmbientScr.g, amb.g);
    amb.b = MAX(LightMgr.getEnvPtr()->AmbientScr.b, amb.b);
    lightSetAmbient(&amb);
    GXColor mat = {0xFF, 0xFF, 0xFF, 0xFF};
    GXSetChanMatColor(4, mat);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
}

// Lighting for effects: the lights (positions taken directly), the effect ambient.
void commonEspLightSet(cLight** list, int n)
{
    LIGHT_FUNC_TABLE;
    GXLightObj lobj[8];
    Vec p;
    u32 mask;
    int i;

    obj_flag = 0;
    mask = 0;
    for (i = 0; i < n; i++) {
        cLight* l = list[i];

        if (l == NULL) {
            continue;
        }
        mask |= 1 << i;
        PSMTXMultVec(pG->Camera.v_mat, &l->Pos, &p);
        GXInitLightPos(&lobj[i], p.x, p.y, p.z);
        lightSetColor(&lobj[i], l, NULL);
        GXInitLightDir(&lobj[i], 0.0f, 0.0f, 1.0f);
        if (l->xD > 7) {
            pLog->err(0, 0, "cLight() INVALIED TYPE %d", l->xD);
            memclr_asm(l, 0x154);
        }
        funcLightParam[l->xD](l, &lobj[i]);
        GXLoadLightObjImm(&lobj[i], 1 << i);
    }
    if (mask == 0) {
        GXLoadLightObjImm(&lightObjBlack, 1);
        mask = 1;
    }
    GXSetNumChans(1);
    GXSetChanCtrl(0, 1, 0, 0, mask, 0, 1);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    lightSetAmbient(&LightMgr.getEnvPtr()->AmbientEsp);
}

// Type 0: constant brightness Intensity, fading to 0 over the last normal.x units of the light's
// radius + object size (when obj_flag).
void lightSetConstant(cLight* pLi, GXLightObj* pLo)
{
    Vec p = pLi->World;
    f32 d = GetDistance3(&obj_pos, &p);
    f32 range = pLi->Radius + obj_size;
    f32 br;

    if (d < range - pLi->normal.x || !(obj_flag & 1) || pLi->Radius == 0.0f) {
        br = pLi->Intensity;
    } else if (d < range) {
        br = pLi->Intensity * (range - d) / pLi->normal.x;
    } else {
        br = 0.0f;
    }
    GXInitLightAttn(pLo, br, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
}

// Type 1: brightness falls linearly to 0 at the radius.
void lightSetLinear(cLight* pLi, GXLightObj* pLo)
{
    Vec p = pLi->World;
    f32 br;

    if (pLi->Radius != 0.0f) {
        f32 d = GetDistance3(&obj_pos, &p);

        br = pLi->Intensity * (pLi->Radius - d) / pLi->Radius;
    } else {
        br = pLi->Intensity;
    }
    GXInitLightAttn(pLo, br, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
}

// Type 2: constant brightness with a quadratic distance attenuation k2 that reaches 0.1 at the radius.
static void lightSetQuadratic(cLight* pLi, GXLightObj* pLo)
{
    Vec p = pLi->World;
    f32 k2 = 0.1f;
    f32 d = GetDistance3(&obj_pos, &p);
    f32 range = pLi->Radius + obj_size;
    f32 br;

    if (d < range - pLi->normal.x || !(obj_flag & 1) || pLi->Radius == 0.0f) {
        br = pLi->Intensity;
    } else if (d < range) {
        br = pLi->Intensity * (range - d) / pLi->normal.x;
    } else {
        br = 0.001f;
    }
    f32 zero = 0.0f;
    f32 one = 1.0f;
    if (pLi->Radius != zero) {
        k2 = (pLi->Intensity - 0.1f) / 0.1f / (pLi->Radius * pLi->Radius);
    } else {
        k2 = zero;
    }
    GXInitLightAttn(pLo, br, zero, zero, one, zero, k2);
}

// Type 3: spot light along the light's normal, cone angle A0, brightness with the radius fade
// (A1 = fade width), GX distance attenuation over 5000.
void lightSetSpotlight(cLight* pLi, GXLightObj* pLo)
{
    Vec cdir;
    Vec dir;
    Vec p;
    LightSpot* sp = &pLi->spot;
    f32 refDist = 5000.0f;
    f32 d;
    f32 range;
    f32 br;

    pLi->getPos(&p);
    pLi->getNormal(&sp->Normal, &dir);
    PSMTXMultVecSR(pG->Camera.v_mat, &dir, &cdir);
    GXInitLightDir(pLo, cdir.x, cdir.y, cdir.z);
    d = GetDistance3(&obj_pos, &p);
    range = pLi->Radius + obj_size;
    if (d < range - sp->A1 || !(obj_flag & 1) || pLi->Radius == 0.0f) {
        br = pLi->Intensity;
    } else if (d < range) {
        br = pLi->Intensity * (range - d) / sp->A1;
    } else {
        br = 0.001f;
    }
    GXInitLightSpot(pLo, sp->A0, 2);
    GXInitLightDistAttn(pLo, 5000.0f, br, 2);
}

// Type 4: direction from the normal, the raw GX attenuation coefficients A0..A2 / K0..K2.
void lightSetCustom(cLight* pLi, GXLightObj* pLo)
{
    Vec cdir;
    Vec dir;
    LightSpot* sp = &pLi->spot;

    pLi->getNormal(&sp->Normal, &dir);
    PSMTXMultVecSR(pG->Camera.v_mat, &dir, &cdir);
    GXInitLightDir(pLo, cdir.x, cdir.y, cdir.z);
    GXInitLightAttn(pLo, sp->A0, sp->A1, sp->A2, sp->K0, sp->K1, sp->K2);
}

// Type 5: directional light — placed at the object + the normal (in camera space when flags
// bit0), brightness with the radius fade.
void lightSetParallel(cLight* pLi, GXLightObj* pLo)
{
    Vec p;
    LightSpot* sp = &pLi->spot;
    f32 d;
    f32 range;
    f32 br;

    if (sp->flags & 1) {
        Mtx inv;

        PSMTXInverse(pG->Camera.v_mat, inv);
        PSMTXMultVecSR(inv, &sp->Normal, &p);
    } else {
        p = sp->Normal;
    }
    PSVECAdd(&obj_pos, &p, &p);
    PSMTXMultVec(pG->Camera.v_mat, &p, &p);
    GXInitLightPos(pLo, p.x, p.y, p.z);
    Vec q;
    pLi->getPos(&q);
    d = GetDistance3(&obj_pos, &q);
    range = pLi->Radius + obj_size;
    if (d < range - sp->A1 || !(obj_flag & 1) || pLi->Radius == 0.0f) {
        br = pLi->Intensity;
    } else if (d < range) {
        br = pLi->Intensity * (range - d) / sp->A1;
    } else {
        br = 0.001f;
    }
    GXInitLightAttn(pLo, br, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
}

// Type 6: spot light with a quadratic distance attenuation scaled by the brightness.
void lightSetSpotQuad(cLight* pLi, GXLightObj* pLo)
{
    Vec cdir;
    Vec dir;
    Vec p;
    LightSpot* sp = &pLi->spot;
    f32 k2 = 0.1f;
    f32 d;
    f32 range;
    f32 br;

    pLi->getPos(&p);
    pLi->getNormal(&sp->Normal, &dir);
    PSMTXMultVecSR(pG->Camera.v_mat, &dir, &cdir);
    GXInitLightDir(pLo, cdir.x, cdir.y, cdir.z);
    d = GetDistance3(&obj_pos, &p);
    range = pLi->Radius + obj_size;
    if (d < range - sp->A1 || !(obj_flag & 1) || pLi->Radius == 0.0f) {
        br = pLi->Intensity;
    } else if (d < range) {
        br = pLi->Intensity * (range - d) / sp->A1;
    } else {
        br = 0.001f;
    }
    GXInitLightSpot(pLo, sp->A0, 2);
    if (pLi->Radius != 0.0f) {
        k2 = (pLi->Intensity - 0.1f) / 0.1f / (pLi->Radius * pLi->Radius) / br;
    } else {
        k2 = 0.0f;
    }
    GXInitLightAttnK(pLo, 1.0f / br, 0.0f, k2);
}

// Type 7: a local ambient light — raises the ambient colour to its colour within the radius
// (faded over the last normal.x units).
void lightSetLocalAmb(cLight* pLi, GXColor* lamb)
{
    Vec p = pLi->World;
    f32 d = GetDistance3(&obj_pos, &p);
    f32 range = pLi->Radius + obj_size;
    GXColor c;

    if (d < range - pLi->normal.x || !(obj_flag & 1) || pLi->Radius == 0.0f) {
        c = pLi->DispCol;
    } else if (d < range) {
        d = (range - d) / pLi->normal.x;
        c.r = (u8) (d * (f32) (int) pLi->DispCol.r);
        c.g = (u8) (d * (f32) (int) pLi->DispCol.g);
        c.b = (u8) (d * (f32) (int) pLi->DispCol.b);
    } else {
        c.a = c.b = c.g = c.r = 0;
    }
    lamb->r = MAX(lamb->r, c.r);
    lamb->g = MAX(lamb->g, c.g);
    lamb->b = MAX(lamb->b, c.b);
}

// GX light colour = DispCol.rgb x alpha / 128 (x the enemy's light-area scale when this is the
// area's light), clamped, alpha 0x80.
void lightSetColor(GXLightObj* lobj, cLight* pLi, cEm* pMod)
{
    f32 col[3];
    GXColor c;
    f32 r = (f32) pLi->DispCol.r;
    f32 g = (f32) pLi->DispCol.g;
    f32 b = (f32) pLi->DispCol.b;
    f32 a = (f32) (int) pLi->DispCol.a;

    col[0] = r * a * 0.0078125f;
    col[1] = g * a * 0.0078125f;
    col[2] = b * a * 0.0078125f;
    if (pMod != NULL) {
        EmLightArea* la = &pMod->litArea;

        if (la->chk(1) == 1 && la->chk(2) == 1 && la->lightNo == pLi->LitIndex) {
            col[0] *= la->scale;
            col[1] *= la->scale;
            col[2] *= la->scale;
        }
    }
    col[0] = col[0] < 0.0f ? 0.0f : (col[0] > 255.0f ? 255.0f : col[0]);
    col[1] = col[1] < 0.0f ? 0.0f : (col[1] > 255.0f ? 255.0f : col[1]);
    col[2] = col[2] < 0.0f ? 0.0f : (col[2] > 255.0f ? 255.0f : col[2]);
    c.r = (u8) col[0];
    c.g = (u8) col[1];
    c.b = (u8) col[2];
    c.a = 0x80;
    GXInitLightColor(lobj, c);
}

// Ambient colour of both colour channels.
void lightSetAmbient(GXColor* col0)
{
    GXSetChanAmbColor(4, *col0);
}

// Unlit draw: the black light, one channel with vertex colour only, black material / ambient.
void LightDisable()
{
    GXLoadLightObjImm(&lightObjBlack, 1);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 1, 0, 0, 1, 2, 1);
    GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
    GXColor c = {0, 0, 0, 0xFF};
    GXSetChanMatColor(4, c);
    GXSetChanAmbColor(4, c);
}

