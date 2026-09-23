// game/esp11.cpp: effect id 0x11, a light source. SetFreeWork creates a cLight (Work8[0] kind,
// Work8[1] cut / type, Work8[2] number) flagged be_flag 0x40 (owned by an effect); with Type
// (Work8[3]) == 1 the light's position, radius (size x 10) and colour follow the sprite every
// frame, otherwise the sprite only counts its life. Destroying the effect destroys the light.

#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"

struct Esp11Work {
    u8 CutNo;         // 0x00 light type (gen->Work8[1])
    u8 LitNo;           // 0x01 light number (gen->Work8[2])
    u8 Kind;         // 0x02 0/1: create a light, 2: fixed type 8, 3: no light (gen->Work8[0])
    u8 Type;         // 0x03 1: light follows the sprite (gen->Work8[3])
    cLight* pLi;  // 0x04
    GXColor Base_col;     // 0x08 base color of the light
    u8 ToolState;    // 0x0C (gen->WorkSp8[0])
};

// Light source effect: creates a cLight and (mode 1) drives its position and color from the
// sprite.
class cEsp11 : public cEsp {
public:
    Esp11Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

extern "C" {
void Esp11_SetParam(cEsp11* esp);
}

// EspCreateTbl[0x11] factory.
cEsp* Esp11_Create()
{
    return new cEsp11;
}

// Publishes ToolState (WorkSp8[0]) to the effect tool state; releases itself when the light died
// elsewhere. Type 1: base update then Esp11_SetParam; other types: plain life countdown.
void cEsp11::move()
{
    Esp11Work* w = &m_Free;

    if (w->ToolState != 0) {
        EffSetToolState(w->ToolState);
    }
    if (w->pLi != NULL && !w->pLi->isAlive()) {
        w->pLi = NULL;
        PushEsp(this);
    } else if (w->Type == 1) {
        if (CommonMove()) {
            Esp11_SetParam(this);
        }
    } else {
        if (m_Life_max != 0 && m_Life_max <= m_Life_time) {
            PushEsp(this);
        } else {
            m_Life_time++;
        }
    }
}

// EspTransTbl[0x11]: draws nothing (the light itself is rendered by the light manager).
void Esp11_Trans(cEsp* pEsp)
{
}

// Destroys the owned cLight if it is still alive.
void cEsp11::Destruct()
{
    cLight* l = m_Free.pLi;

    if (l != NULL && l->isAlive()) {
        LightMgr.destroy(l);
    }
}

// Copies the sprite state into the light: world position, Radius = size x 10, colour =
// Base_col x sprite colour / 255, alpha = Base_col.a x sprite alpha / 2.
void Esp11_SetParam(cEsp11* esp)
{
    Esp11Work* w = &esp->m_Free;
    f32 alpha;

    if (w->pLi == NULL) {
        return;
    }
    w->pLi->Pos = esp->m_Pos;
    if (esp->parent != pEffParentWorld) {
        PSMTXMultVec(esp->parent->mat, &w->pLi->Pos, &w->pLi->Pos);
    }
    w->pLi->Radius = esp->m_Size_base_x * esp->m_Size_mul * 10.0f;
    alpha = esp->m_Col_a / 255.0f;
    w->pLi->Col.r = (u8)(w->Base_col.r * esp->m_Col_r / 255.0f);
    w->pLi->Col.g = (u8)(w->Base_col.g * esp->m_Col_g / 255.0f);
    w->pLi->Col.b = (u8)(w->Base_col.b * esp->m_Col_b / 255.0f);
    w->pLi->Col.a = (u8)(w->Base_col.a * alpha * 0.5f);
}

// Kind 0/1 creates the light (Kind 2 = fixed cut 8 light 0 following the sprite, Kind 3 = no
// light), records its base colour and applies Type 1 at once. Fails on missing light data or bad
// Kind/Type. In the effect tool it also clears Stop_flg 0x01000000 so lights keep moving.
int cEsp11::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp11Work* w = &m_Free;

    w->Kind = pSeq->Work8[0];
    w->CutNo = pSeq->Work8[1];
    w->LitNo = pSeq->Work8[2];
    w->Type = pSeq->Work8[3];
    w->ToolState = pSeq->WorkSp8[0];
    if (w->Kind == 2) {
        w->Kind = 0;
        w->CutNo = 8;
        w->LitNo = 0;
        w->Type = 1;
    }
    if (w->Kind <= 1) {
        w->pLi = LightMgr.create(w->Kind != 0, w->CutNo, w->LitNo, 0);
        if (w->pLi == NULL) {
            pLog->err(0, 0, "ESP11 : LightId[Kind:%d cut:%d no:%d] no data", w->Kind, w->CutNo, w->LitNo);
            return 0;
        }
        w->pLi->be_flag |= 0x40;
    } else if (w->Kind == 3) {
        w->pLi = NULL;
        return 1;
    } else {
        pLog->err(0, 0, "ESP11 : LightKind[%d] invalid", w->Kind);
        return 0;
    }
    w->Base_col = w->pLi->Col;
    if (w->Type == 0) {
    } else if (w->Type == 1) {
        Esp11_SetParam(this);
    } else {
        pLog->err(0, 0, "ESP11 : LightType[%d] invalid", w->Type);
        return 0;
    }
    if (DbgFlagChk(pG, DBG_IN_ESP_TOOL)) {
        SpfFlagOff(pG, SPF_LIGHT);
    }
    return 1;
}
