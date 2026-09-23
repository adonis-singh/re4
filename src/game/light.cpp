// game/light: the light manager (D:/Bio4/Prog/light.cpp). cLightMgr (LightMgr) owns the cLight
// pool (cManager) and the current light environment (LightEnv: fog, background colour, blur,
// contrast, mipmap, tune colours, wind). Light data comes as cLit blocks (core archive, room "LIT",
// third set) holding per-camera-cut cLightEnv records each followed by cLightWork lights;
// update(cut) swaps the room lights to a cut (fog interpolated over Hokan frames), move() runs
// every light's type function (LightFuncTbl) and parent tracking, setModel2/setCloth/setEsp pick
// up to 8 lights hitting a model volume. cLight adds parent attachment (enemy / scroll / etc /
// object parts) and spot direction helpers.
#include "light.h"
#include "ctrl.h"
#include "atari.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "obj.h"
#include "obj02.h"
#include "scroll.h"
#include "etc_model.h"
#include "view.h"
#include "filter.h"
#include "math_sub.h"
#include "cam_ctrl.h"
#include "pendulum.h"
#include "trans.h"

// game/trans.cpp texture LOD / TEV scale settings
// trans.cpp; uninitialised there, so not in trans.h (a header extern reorders trans.cpp's .bss / .sbss)
extern u8 min_lod;
extern u8 max_lod;
extern f32 lod_bias;

// pointer to game memory (0x80000000 .. 0x82FFFFFF)
#define IN_RANGE(p) ((u32) (p) - 0x80000000 <= 0x02FFFFFF)

void funcDelCtrl(cCtrl* pCtr);

// Light control work (cCtrl::work): the electric power path
struct LightCtrlWork {
    cLightPathData* pPath;   // 0x00
    cLightPathData* pPath2;  // 0x04
    u8 idx;                  // 0x08
};

static LightFog fogNew;

// value unknown: the linker dropped the object, only the `_GLOBAL_.I.FarDistance__9cLightMgr` name survives
const f32 cLightMgr::FarDistance = 100000.0f;

// Manager of cLight units.
cLightMgr::cLightMgr() : cManager<cLight>(sizeof(cLight), 0)
{
    setName("cLightMgr");
}

// Debug log when m_logMode is on.
void cLightMgr::log(const char* pStr, ...)
{
    va_list ap;

    if (m_logMode) {
        va_start(ap, pStr);
        pLog->vwarn(0, 0, pStr, ap);
    }
}

// Boot init: installs the type function table, clears the environment, power and kind flags.
void cLightMgr::init(void (**funcTbl)(cLight*))
{
    cManager<cLight>::init(funcTbl);
    pLitHeader = 0;
    memclr_asm(&LightEnv, sizeof(LightEnv));
    ElecPower = 1.0f;
    m_ColBrendRate = 0.0f;
    pArray = 0;
    nArray = 0;
    pLitPath = 0;
    memset_asm(kindFlags, 0xFF, sizeof(kindFlags));
    x204 = 0;
    dbFlag = 0;
    dbMem = 0;
}

// Room init: binds the three light data blocks (upgrading old versions), makes the room block the
// active one, resets the tune colours and kind flags.
int cLightMgr::roomInit(cLit* core, cLit* room, cLit* third)
{
    cManager<cLight>::roomInit();
    if (!IN_RANGE(core)) {
        pLog->err(0, 0, "cLightMgr::roomInit() CORE INVALID POINTER %08x", core);
        return 0;
    }
    if (!IN_RANGE(room)) {
        pLog->err(0, 0, "cLightMgr::roomInit() ROOM INVALID POINTER %08x", room);
        return 0;
    }
    if (!IN_RANGE(third)) {
        pLog->err(0, 0, "cLightMgr::roomInit() ROOM INVALID POINTER %08x", third);
        return 0;
    }
    m_pLitCore = core;
    core->versionUp();
    m_pLitRoom = room;
    room->versionUp();
    m_pLitRoom2 = third;
    pLitHeader = room;
    ElecPower = 1.0f;
    m_ColBrendRate = 0.0f;
    pArray = 0;
    nArray = 0;
    m_Tune[0].r = 0xC8;
    m_Tune[0].g = 0xC0;
    m_Tune[0].b = 0xF0;
    m_Tune[0].a = 0x80;
    m_Tune[1].r = 0;
    m_Tune[1].g = 0;
    m_Tune[1].b = 0;
    m_Tune[1].a = 0x80;
    m_Tune[2].r = 0xC8;
    m_Tune[2].g = 0xC0;
    m_Tune[2].b = 0xF0;
    m_Tune[2].a = 0x80;
    x204 = 0;
    dbFlag = 0;
    dbMem = 0;
    memset_asm(kindFlags, 0xFF, sizeof(kindFlags));
    return 1;
}

// Unit construction: placement-news the cLight subclass for type id (1, 2, 6, 7, 8 have their own).
int cLightMgr::construct(cLight* p, u32 id)
{
    switch (id) {
    default:
        new (p) cLight();
        break;
    case 1:
        new (p) cLight01();
        break;
    case 2:
        new (p) cLight02();
        break;
    case 6:
        new (p) cLight06();
        break;
    case 7:
        new (p) cLight07();
        break;
    case 8:
        new (p) cLight08();
        break;
    }
    return 1;
}

// Creates a light from a cLightWork record (front of the pool), copies it and runs its first move.
cLight* cLightMgr::create(cLightWork* pLw)
{
    cLight* l = cManager<cLight>::create(pLw->Type);
    if (l == 0) {
        return 0;
    }
    if (!l->isAlive()) {
        return 0;
    }
    *l = *pLw;
    l->move();
    return l;
}

// Same as create(cLightWork*) but allocated from the back of the pool.
cLight* cLightMgr::createBack(cLightWork* pLw)
{
    cLight* l = cManager<cLight>::createBack(pLw->Type);
    if (l == 0) {
        return 0;
    }
    *l = *pLw;
    l->move();
    return l;
}

// Creates light `lightNo` of cut `cutNo` of the data block with extra be_flag bits `flag`; the
// negative numbers are bulk operations: -1 load every light of the cut, -2 apply the cut's
// environment, -3 both. Returns the light (0 for the bulk forms / failures).
cLight* cLightMgr::create(cLit* lit, int cutNo, int lightNo, int flag)
{
    cLightEnv* cut;
    cLightWork* w;
    cLight* l;

    if (!IN_RANGE(lit)) {
        pLog->err(0, 0, "Light create failed PTR ERR 0x%08x", lit);
        return 0;
    }
    cut = lit->getCut(cutNo);
    if (cut == 0) {
        return 0;
    }
    if (lightNo >= 0) {
        if ((u32) lightNo >= cut->nLight) {
            return 0;
        }
        w = cut->getLightWork(lightNo);
        if (w == 0) {
            return 0;
        }
        l = create(w);
        if (l == 0) {
            if (0) {
                log("cLightMgr::create() WORK ALLOC FAILED", 0);
            }
            return 0;
        }
        l->be_flag = flag | 3;
    } else {
        if (lightNo == -1 || lightNo == -3) {
            loadLit(cut->getLightWork(lightNo), cut->nLight);
        }
        l = 0;
    }
    if (lightNo == -3 || lightNo == -2) {
        setEnv(cut, -1);
    }
    return l;
}

// Back-of-pool variant of create(cLit*, ...).
cLight* cLightMgr::createBack(cLit* lit, int cutNo, int lightNo, int flag)
{
    cLightEnv* cut;
    cLightWork* w;
    cLight* l;

    cut = lit->getCut(cutNo);
    if (cut == 0) {
        return 0;
    }
    if (lightNo >= 0) {
        w = cut->getLightWork(lightNo);
        if (!VALID_PTR(w)) {
            return 0;
        }
        l = createBack(w);
        if (!VALID_PTR(l)) {
            if (0) {
                log("cLightMgr::createBack() WORK ALLOC FAILED", 0);
            }
            return 0;
        }
        l->be_flag = flag | 3;
    } else {
        if (lightNo == -1 || lightNo == -3) {
            loadLit(cut->getLightWork(lightNo), cut->nLight);
        }
        l = 0;
    }
    if (lightNo == -3 || lightNo == -2) {
        setEnv(cut, -1);
    }
    return l;
}

// create() on data block litNo (0 core, 1 room, 2 third).
cLight* cLightMgr::create(int litNo, int cutNo, int lightNo, int flag)
{
    cLit* lit;

    switch (litNo) {
    case 0:
    default:
        lit = m_pLitCore;
        break;
    case 1:
        lit = m_pLitRoom;
        break;
    case 2:
        lit = m_pLitRoom2;
        break;
    }
    return create(lit, cutNo, lightNo, flag);
}

// createBack() on data block litNo.
cLight* cLightMgr::createBack(int litNo, int cutNo, int lightNo, int flag)
{
    cLit* lit;

    switch (litNo) {
    case 0:
    default:
        lit = m_pLitCore;
        break;
    case 1:
        lit = m_pLitRoom;
        break;
    case 2:
        lit = m_pLitRoom2;
        break;
    }
    return createBack(lit, cutNo, lightNo, flag);
}

// Adds d to the global electric power factor (clamped 0..1); returns the new value.
f32 cLightMgr::setElecPower(f32 d)
{
    ElecPower += d;
    if (ElecPower < 0.0f) {
        ElecPower = 0.0f;
    } else if (ElecPower > 1.0f) {
        ElecPower = 1.0f;
    }
    return ElecPower;
}

// Replaces the light-control ctrl unit (Id 1) with one that plays light path pathNo (power
// flicker of the room lights); 0 when the path or a ctrl slot is missing.
int cLightMgr::setElecPower2(u8 id, u8 flag)
{
    cCtrl* c;
    cCtrl* n;
    LightCtrlWork* w;
    cLightPathData* path;
    void (*func)(cCtrl*) = funcDelCtrl;

    c = CtrlMgr.getActiveWork();
    while (c) {
        n = c;
        c = (cCtrl*) c->pNext;
        func(n);
    }
    c = CtrlMgr.createBack(0);
    if (c == 0) {
        return 0;
    }
    c->Id = 1;
    w = (LightCtrlWork*) c->work;
    path = getPathPtr(id);
    if (!VALID_PTR(path)) {
        pLog->err(0, 0, "cLightMgr::setElecPower2() NO PATH DATA %d", id);
        CtrlMgr.destroy(c);
        return 0;
    }
    w->pPath = w->pPath2 = path;
    w->idx = flag;
    return 1;
}

// Destroys the light-control ctrl units (Id 1).
void funcDelCtrl(cCtrl* pCtr)
{
    if (pCtr->Id == 1) {
        CtrlMgr.destroy(pCtr);
    }
}

// Enables light kind `kind` (kinds are switched off by events / sub screens / item examine).
int cLightMgr::onKind(u8 kind)
{
    kindFlags[kind >> 5] |= 1 << (kind & 31);
    return 1;
}

// Disables light kind `kind`.
int cLightMgr::offKind(u8 kind)
{
    kindFlags[kind >> 5] &= ~(1 << (kind & 31));
    return 1;
}

// 1 when light kind `kind` is enabled.
int cLightMgr::checkKind(u8 kind)
{
    if (kindFlags[kind >> 5] & (1 << (kind & 31))) {
        return 1;
    }
    return 0;
}

// First alive light of the given kind.
cLight* cLightMgr::getKindLight(u8 kind)
{
    cLight* l;

    for (l = LightMgr.getActiveWork(); l; l = LightMgr.getNext(l)) {
        if (l->Kind == kind) {
            return l;
        }
    }
    return 0;
}

// Selects the active light data block (0 = the room block); events switch it to their own.
int cLightMgr::roomLitSet(cLit* lit)
{
    if (lit == 0) {
        lit = m_pLitRoom;
    }
    if (!IN_RANGE(lit)) {
        pLog->err(0, 0, "cLightMgr::roomLitSet() INVALID POINTER %08x", lit);
        lit = 0;
    }
    pLitHeader = lit;
    return 1;
}

// 1 when the room block is the active one.
int cLightMgr::roomLitCheck()
{
    return pLitHeader == m_pLitRoom;
}

// Per-frame: fog interpolation, die check, then lightMove on every alive light. Sets Status_flg[1]
// 0x200 (lights moved this frame).
int cLightMgr::move()
{
    cLight* l;
    cLight* n;
    void (*func)(cLight*);

    if (ElecPower != 1.0f) {
        pLog->err(0, 0, "cLightMgr ElecPower != 1.0f");
    }
    if (m_ColBrendRate != 0.0f) {
        pLog->err(0, 0, "cLightMgr m_ColBrendRate != 0.0f");
    }
    hokanMove();
    dieCheck();
    StaFlagOn(pG, STA_USE_CAST_SHADOW);
    func = lightMove;
    l = pAlive;
    while (l) {
        n = l;
        l = (cLight*) l->pNext;
        func(n);
    }
    return 1;
}

// Interpolates the current fog towards fogNew over the remaining m_Hokan frames and updates the
// far plane (fog Type 0 = ZFAR, else fog end scaled by far_play_ratio).
void cLightMgr::hokanMove()
{
    if (m_Hokan != 0) {
        LightFog* fog = &LightEnv.Fog;

        m_Hokan--;
        fog->Start = (fog->Start * m_Hokan + fogNew.Start) / (m_Hokan + 1);
        fog->End = (fog->End * m_Hokan + fogNew.End) / (m_Hokan + 1);
        fog->Color.r = (u8) (((f32) fog->Color.r * m_Hokan + (f32) fogNew.Color.r) / (m_Hokan + 1));
        fog->Color.g = (u8) (((f32) fog->Color.g * m_Hokan + (f32) fogNew.Color.g) / (m_Hokan + 1));
        fog->Color.b = (u8) (((f32) fog->Color.b * m_Hokan + (f32) fogNew.Color.b) / (m_Hokan + 1));
        fog->Color.a = (u8) (((f32) fog->Color.a * m_Hokan + (f32) fogNew.Color.a) / (m_Hokan + 1));
        GXSetFog(fog->Type, fog->Start, fog->End, ZNEAR, ZFAR, fog->Color);
        if (fog->Type == 0) {
            View.setFarPlane(ZFAR);
        } else {
            View.setFarPlane(fog->End * (1.0f - LightEnv.far_play_ratio) + 1.0f);
        }
    }
}

// One light's frame: destroys it when its parent model died, recomputes World from the parent
// parts (be_flag 2 = attached), snaps against collision (hitAdjust), runs the type function; an
// etc-model parent (type 3) with hp <= 0 detaches the light.
void lightMove(cLight* pLi)
{
    cModel* p = pLi->pParent;
    cModel* em;

    if (p != 0 && !p->isAlive() && !DbgFlagChk(pG, DBG_TEST_MODE)) {
        if (pLi->isAlive()) {
            LightMgr.destroy(pLi);
        }
        return;
    }
    if (pLi->be_flag & 2) {
        pLi->calcPos(&pLi->Pos, &pLi->World);
    }
    pLi->hitAdjust();
    pLi->move();
    if (pLi->ParentType == 3) {
        if (getRoomEtcOnLight(pLi->ParentNo, &em, 0) == 1) {
            if (((cEm*) em)->hp <= 0) {
                pLi->be_flag &= ~2;
            }
        }
    }
}

// The current light environment.
cLightEnv* cLightMgr::getEnvPtr()
{
    return &LightEnv;
}

// LightFuncTbl[0] and the unused types: static light, DispCol = Col.
void Light00_Move(cLight* pLi)
{
    pLi->DispCol = pLi->Col;
}

// Picks the lights for a model: every alive, enabled light (xF mask vs EnableMask, kind on, not a
// foot-shadow type 4, SelectMask bit, non-black colour, volume hit test) up to 8, stored in
// LightInfo.pLight; during an event only lights on event-flagged parents.
void cLightMgr::setModel2(cModel* pMod)
{
    cLight* l;
    cModel* parent;
    u32 i;
    int n = 0;
    int hit;
    f32 pri[8];

    memclr_asm(pMod->LightInfo.pLight, sizeof(pMod->LightInfo.pLight));
    for (i = 0; i < nArray; i++) {
        l = fastAt(i);
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->xF & pMod->LightInfo.EnableMask) {
            hit = 1;
        } else if (!(pMod->LightInfo.EnableMask & 0x41) && l->isParent(pMod)) {
            hit = 1;
        } else {
            hit = 0;
        }
        if (hit == 0) {
            continue;
        }
        if (l->Type == 4) {
            continue;
        }
        if (!checkKind(l->Kind)) {
            continue;
        }
        if (pMod->id == 2 && (((cObjScr*) pMod)->Attribute & 1) && (l->Attribute & 4)) {
            continue;
        }
        if (i <= 31 && !((1 << i) & pMod->LightInfo.SelectMask) && !StaFlagChk(pG, STA_NO_LIGHTMASK)) {
            if (!StaFlagChk(pG, STA_THERMO_GRAPH)) {
                continue;
            }
        }
        if ((*(u32*) &l->DispCol & 0xFFFFFF00) == 0) {
            continue;
        }
        if (!lightHitCheck(pMod, l)) {
            continue;
        }
        parent = l->pParent;
        if (StaFlagChk(pG, STA_SUSPEND) && parent != 0 && !(parent->be_flag & 0x800)) {
            continue;
        }
        if (n > 7) {
            if (DbgFlagChk(pG, DBG_LIGHT_ERR_CHECK)) {
                pLog->warn(6, 3, "MODEL'S LIGHT OVER 8 !! [%08X]", pMod);
            }
            pMod->error();
            return;
        }
        pMod->LightInfo.pLight[n] = l;
        pri[n] = (f32) l->Priority;
        n++;
    }
}

// Picks up to 8 cloth lights (xF bit 0x10) hitting the model for the cloth renderer.
void cLightMgr::setCloth(cModel* pMod, u32 lightNum)
{
    cLight* l;
    u32 i;
    int n;

    if (!VALID_PTR(pMod)) {
        pLog->err(0, 0, "cLightMgr::setCloth() INVALID PTR %08x", pMod);
        return;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        l = fastAt(i);
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (l->Type == 4) {
            continue;
        }
        if (!(l->xF & 0x10)) {
            continue;
        }
        if ((*(u32*) &l->DispCol & 0xFFFFFF00) == 0) {
            continue;
        }
        if (!lightHitCheck(pMod, l)) {
            continue;
        }
        if (n > 7) {
            if (DbgFlagChk(pG, DBG_LIGHT_ERR_CHECK)) {
                pLog->warn(6, 3, "MODEL'S LIGHT OVER 8 !! [%08X]", pMod);
            }
            pMod->error();
            return;
        }
        pMod->LightInfo.pLight[n] = l;
        n++;
    }
}

// Fills the effect light list with every alive light whose xF matches `mask` (max 8).
void cLightMgr::setEsp(EspLightList* pEnv, u8 enableMask)
{
    cLight* l;
    u32 i;

    pEnv->num = 0;
    for (i = 0; i < nArray; i++) {
        l = fastAt(i);
        if ((l->be_flag & 3) != 3) {
            continue;
        }
        if (!(l->xF & enableMask)) {
            continue;
        }
        if (pEnv->num == 8) {
            pLog->warn(0, 0, "cLightMgr::setEsp():ESP LIGHT MAX(%d)", 8);
            return;
        }
        pEnv->p[pEnv->num] = l;
        pEnv->num++;
    }
}

// Does the light reach the model's light volume? Dispatches on LightInfo.getType() (0 cylinder, 1/3
// sphere, 2 box).
int lightHitCheck(cModel* pMod, cLight* pLight)
{
    static int (*funcTbl[4])(cModel*, cLight*) = {
        lightHitCheckCylinder,
        lightHitCheckSphere,
        lightHitCheckBBox,
        lightHitCheckSphere,
    };
    return funcTbl[pMod->LightInfo.getType()](pMod, pLight);
}

// Sphere volume (Size.x) vs light radius (0 = infinite).
int lightHitCheckSphere(cModel* pMod, cLight* pLight)
{
    Vec pos;
    Vec lpos;
    cLightInfo* li = &pMod->LightInfo;

    li->getPos(pMod, &pos);
    pLight->getPos(&lpos);
    if (GetDistance3(&pos, &lpos) < li->Size.x + pLight->Radius || pLight->Radius == 0.0f) {
        return 1;
    }
    return 0;
}

// Capsule volume: spheres of Size.x at +-Size.y along the parts' up axis vs the light radius.
int lightHitCheckCylinder(cModel* pMod, cLight* pLight)
{
    static const Vec vech = { 0.0f, 1.0f, 0.0f };
    Vec pos;
    Vec tmp;
    Vec lpos;
    cLightInfo* li = &pMod->LightInfo;
    cCoord* c;
    f32 r;

    c = li->getPos(pMod, &pos);
    pLight->getPos(&lpos);
    r = pLight->Radius;
    if (r == 0.0f) {
        return 1;
    }
    PSVECScale(&vech, &tmp, -li->Size.y);
    PSMTXMultVecSR(c->mat, &tmp, &tmp);
    PSVECAdd(&pos, &tmp, &tmp);
    if (GetDistance3(&tmp, &lpos) < li->Size.x + r) {
        return 1;
    }
    PSVECScale(&vech, &tmp, li->Size.y);
    PSMTXMultVecSR(c->mat, &tmp, &tmp);
    PSVECAdd(&pos, &tmp, &tmp);
    return GetDistance3(&tmp, &lpos) < li->Size.x + r;
}

// Box volume: the light position in the volume's local space against Size * model scale + radius.
int lightHitCheckBBox(cModel* pMod, cLight* pLight)
{
    Vec p;
    Vec* size;
    f32 sx;
    f32 sz;
    f32 sy;

    if (pLight->Radius == 0.0f) {
        return 1;
    }
    p = pLight->World;
    PSMTXMultVec(pMod->LightInfo.imat, &p, &p);
    size = &pMod->LightInfo.Size;
    sx = size->x * pMod->scale.x;
    sy = size->y * pMod->scale.y;
    sz = size->z * pMod->scale.z;
    if (p.x - pLight->Radius > sx || p.x + pLight->Radius < -sx || p.z - pLight->Radius > sz || p.z + pLight->Radius < -sz || p.y - pLight->Radius > sy || p.y + pLight->Radius < -sy) {
        return 0;
    }
    return 1;
}

// Switches the room lighting to camera cut `cut_no` (-1 = 0; clamped by getSafeCutNo): deletes the
// current room lights, applies the cut's environment (fog over `hokan` frames, -1 = the cut's own)
// and loads its lights, then reserves the spare slots (nMaxLight - nLight, at least 10) as
// invisible placeholders. No-op in thermal mode; Status_flg[2] 0x00400000 keeps the old cut.
int cLightMgr::update(int cutNo, int hokan)
{
    cLightEnv* cut;
    cLight* l;
    u32 n;
    u32 i;

    if (StaFlagChk(pG, STA_LIT_NO_UPDATE)) {
        cutNo = m_oldCutNo;
    } else {
        m_oldCutNo = cutNo;
    }
    if (StaFlagChk(pG, STA_THERMO_GRAPH)) {
        return 0;
    }
    if (!VALID_PTR(pLitHeader)) {
        pLog->err(0, 0, "cLightMgr::update() NO LIGHT DATA");
        return 0;
    }
    if (cutNo == -1) {
        pLog->warn(0, 0, "cLightMgr::update() Cut No is [-1]");
        cutNo = 0;
    }
    deleteScr();
    cutNo = pLitHeader->getSafeCutNo(cutNo);
    cut = pLitHeader->getCut(cutNo);
    registCut(cut, hokan);
    if (pLitHeader->nMaxLight >= nArray) {
        int max;
        pLog->err(0, 0, "cLightMgr::update() nMaxLight Over %d/%d", pLitHeader->nMaxLight, nArray);
        max = nArray - 1;
        pLitHeader->nMaxLight = max;
    }
    n = pLitHeader->nMaxLight - cut->nLight;
    if (n < 10) {
        n = 10;
    }
    for (i = 0; i < n; i++) {
        l = cManager<cLight>::create();
        l->be_flag |= 4;
        l->be_flag &= ~2;
    }
    return 1;
}

// Thermal scope lighting: replaces the room lights with core cut 11.
int cLightMgr::setThermo()
{
    cLightEnv* cut;
    cLight* l;
    u32 i;

    deleteScr();
    cut = getCutAddr(0, 11);
    registCut(cut, 0);
    if (pLitHeader->nMaxLight >= nArray) {
        int max;
        pLog->err(0, 0, "cLightMgr::update() nMaxLight Over %d/%d", pLitHeader->nMaxLight, nArray);
        max = nArray - 1;
        pLitHeader->nMaxLight = max;
    }
    for (i = 0; i < pLitHeader->nMaxLight - cut->nLight; i++) {
        l = cManager<cLight>::create();
        l->be_flag |= 4;
        l->be_flag &= ~2;
    }
    return 1;
}

// Applies a cut: environment (setEnv) then all its lights (loadLit), clamped to the pool size.
int cLightMgr::registCut(cLightEnv* pLe, int hokan)
{
    setEnv(pLe, hokan);
    if (pLe->nLight > nArray) {
        pLog->err(0, 0, "LightRegistCut() LIGHT NUM OVER %d", nArray);
        pLe->nLight = nArray;
        return 0;
    }
    loadLit(pLe->getLightWork(0), pLe->nLight);
    return 1;
}

// Light record `no` following the cut header (0 when the cut has no lights).
cLightWork* cLightEnv::getLightWork(int no)
{
    cLightWork* w;

    if (nLight == 0) {
        w = 0;
    } else {
        w = (cLightWork*) ((u8*) this + sizeof(cLightEnv) + no * sizeof(cLightWork));
    }
    return w;
}

// Byte size of the cut record with its lights.
u32 cLightEnv::getSize()
{
    return sizeof(cLightEnv) + nLight * sizeof(cLightWork);
}

// Cut record of block litNo (0 core, 1 room, 2 third).
cLightEnv* cLightMgr::getCutAddr(int type, int cutNo)
{
    cLit* lit;

    switch (type) {
    case 0:
    default:
        lit = m_pLitCore;
        break;
    case 1:
        lit = m_pLitRoom;
        break;
    case 2:
        lit = m_pLitRoom2;
        break;
    }
    return lit->getCut(cutNo);
}

// The cut number, or 0 when it is out of range / missing.
int cLit::getSafeCutNo(int cutNo)
{
    if (cutNo < 0 || cutNo >= CutNum || !VALID_PTR(getCut(cutNo))) {
        cutNo = 0;
    }
    return cutNo;
}

// Event fog curve: sets the fog start distance.
void cLightMgr::setFogStart(f32 datStart)
{
    LightEnv.Fog.Start = datStart;
}

// Event fog curve: sets the fog end distance.
void cLightMgr::setFogEnd(f32 datEnd)
{
    LightEnv.Fog.End = datEnd;
}

// Current fog start distance.
f32 cLightMgr::getFogStart()
{
    return LightEnv.Fog.Start;
}

// Current fog end distance.
f32 cLightMgr::getFogEnd()
{
    return LightEnv.Fog.End;
}

// Programs GX fog from LightEnv.Fog (black when Disp_flg 0x4000 or thermal) and the view far plane.
void cLightMgr::setFog()
{
    LightFog* fog = &LightEnv.Fog;
    u8 c = 0;

    if (DpfFlagChk(pG, DPF_FOG) || (StaFlagChk(pG, STA_THERMO_GRAPH))) {
        GXColor black;
        black.r = black.g = black.b = black.a = c;
        GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, black);
    } else {
        GXSetFog(fog->Type, fog->Start, fog->End, ZNEAR, ZFAR, fog->Color);
        if (fog->Type == 0) {
            View.setFarPlane(ZFAR);
        } else {
            View.setFarPlane(fog->End * (1.0f - LightEnv.far_play_ratio) + 1.0f);
        }
    }
}

// Pushes the environment's blur rate/power/type and contrast to filter00.
void cLightMgr::setBlur()
{
    s8 power = LightEnv.blur_power;
    s8 r = LightEnv.contrast[0];
    s8 g = LightEnv.contrast[1];
    s8 b = LightEnv.contrast[2];
    u32 type = LightEnv.blur_type;

    Filter00SetAlpha(LightEnv.blur_rate);
    Filter00SetPower(power);
    Filter00SetType(type);
    Filter00SetContrast(r, g, b);
}

// Destroys every room ("scr") light (be_flag 4), keeping the dynamic ones.
void cLightMgr::deleteScr()
{
    cLight* l;
    u32 i;

    for (i = 0; i < nArray; i++) {
        l = fastAt(i);
        if (l->checkScr()) {
            destroy(l);
        }
    }
}

// Clears mask bits from every room light's xF (light class mask).
void cLightMgr::offScr(u8 enable)
{
    cLight* l;
    u32 i;

    for (i = 0; i < nArray; i++) {
        l = fastAt(i);
        if (l->checkScr()) {
            l->xF &= ~enable;
        }
    }
}

// Number of room lights alive.
int cLightMgr::countScr()
{
    u32 i;
    int n = 0;

    for (i = 0; i < nArray; i++) {
        if ((fastAt(i))->checkScr()) {
            n++;
        }
    }
    return n;
}

// Installs a cut's environment: fog target (interpolated over hokan frames, -1 = cut->Hokan; with
// interpolation the old fog/bg colour are kept as the start), fog, blur, mipmap, tune, wind and
// the two TEV colour scales.
int cLightMgr::setEnv(cLightEnv* pLe, int hokan)
{
    fogNew = pLe->Fog;
    if (hokan < 0) {
        m_Hokan = pLe->Hokan;
    } else {
        m_Hokan = hokan;
    }
    if (m_Hokan != 0) {
        f32 fs = LightEnv.Fog.Start;
        f32 fe = LightEnv.Fog.End;
        GXColor fc = LightEnv.Fog.Color;
        LightEnv = *pLe;
        LightEnv.Fog.Start = fs;
        LightEnv.Fog.End = fe;
        LightEnv.Fog.Color = fc;
    } else {
        LightEnv = *pLe;
    }
    setFog();
    setBlur();
    setMipmap(pLe);
    setTune(pLe);
    pLe->wind.set();
    if (pLe->tev_scale[0] > 2) {
        pLog->err(0, 0, "setEnv() TEV_SCALE ERROR %d", pLe->tev_scale[0]);
        return 0;
    }
    if (pLe->tev_scale[1] > 2) {
        pLog->err(0, 0, "setEnv() TEV_SCALE ERROR %d", pLe->tev_scale[1]);
        return 0;
    }
    gxCsScale[0] = pLe->tev_scale[0];
    gxCsScale[1] = pLe->tev_scale[1];
    return 1;
}

// Tune colours (the three character colour tints): the cut's when tuneOn bit 0, else the defaults.
void cLightMgr::setTune(cLightEnv* pLe)
{
    if (pLe->tuneOn & 1) {
        m_Tune[0] = pLe->Tune[0];
        m_Tune[1] = pLe->Tune[1];
        m_Tune[2] = pLe->Tune[2];
    } else {
        m_Tune[0].r = 0xC8;
        m_Tune[0].g = 0xC0;
        m_Tune[0].b = 0xF0;
        m_Tune[0].a = 0x80;
        m_Tune[1].r = 0;
        m_Tune[1].g = 0;
        m_Tune[1].b = 0;
        m_Tune[1].a = 0x80;
        m_Tune[2].r = 0xC8;
        m_Tune[2].g = 0xC0;
        m_Tune[2].b = 0xF0;
        m_Tune[2].a = 0x80;
    }
}

// Global texture LOD settings from the cut (min/max lod 0..9, anisotropy 0..2, lod bias -5..10).
int cLightMgr::setMipmap(cLightEnv* pLe)
{
    if (!VALID_PTR(pLe)) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED PTR %08X", pLe);
        return 0;
    }
    if (pLe->min_lod > 9) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED MIN_LOD %d", pLe->min_lod);
        pLe->min_lod = 0;
    }
    if (pLe->max_lod > 9) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED MAX_LOD %d", pLe->max_lod);
        pLe->max_lod = 5;
    }
    if (pLe->max_lod < pLe->min_lod) {
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED MIN > MAX %d %d", pLe->min_lod, pLe->max_lod);
        max_lod = min_lod;
    }
    min_lod = pLe->min_lod;
    max_lod = pLe->max_lod;
    switch (pLe->aniso) {
    default:
        pLog->err(0, 0, "cLightMgr::setMipmap() INVALIED ANISO %d", pLe->aniso);
        pLe->aniso = 0;
        aniso = 0;
        break;
    case 0:
        aniso = 0;
        break;
    case 1:
        aniso = 1;
        break;
    case 2:
        aniso = 2;
        break;
    }
    if (lod_bias < -5.0f) {
        lod_bias = -5.0f;
    }
    if (lod_bias > 10.0f) {
        lod_bias = 10.0f;
    }
    lod_bias = pLe->lod_bias;
    return 0;
}

// Creates n lights from consecutive records; attached ones get their parent and world position.
int cLightMgr::loadLit(cLightWork* pLw, u32 nL)
{
    cLight* l;
    u32 i;

    for (i = 0; i < nL; i++, pLw++) {
        l = create(pLw);
        if (l->be_flag & 2) {
            l->calcParent();
            l->calcPos(&l->Pos, &l->World);
        }
        l->LitIndex = i;
    }
    return 1;
}

// Tool: writes every room light back into cLightWork records.
int cLightMgr::saveLit(cLightWork* pLw)
{
    cLight* l;
    u32 i;

    for (i = 0; i < nArray; i++) {
        l = fastAt(i);
        if (l->checkScr()) {
            *pLw = *l;
            pLw++;
        }
    }
    return 1;
}

// Address of the active light block pointer (tools).
cLit** cLightMgr::getLitPPtr()
{
    return &pLitHeader;
}

// Binds the light path block of the core archive.
int cLightMgr::initPath(LightPathHeader* p)
{
    if (!IN_RANGE(p)) {
        pLog->err(0, 0, "cLightMgr::initPath() INVALID PTR %08X", p);
        return 0;
    }
    pLitPath = p;
    return 1;
}

// Light path `no` (0 with an error when missing).
cLightPathData* cLightMgr::getPathPtr(u8 id)
{
    u32 ofs;

    if (!VALID_PTR(pLitPath)) {
        pLog->err(0, 0, "cLightMgr::getPathPtr() PATH NOT INITIALIZED.");
        return 0;
    }
    if (id >= pLitPath->num) {
        pLog->err(0, 0, "cLightMgr::getPathPtr() INVALID ID %d.", id);
        return 0;
    }
    ofs = *(u32*) (id * 4 + (u32) pLitPath + 4);
    if (ofs == 0) {
        return 0;
    }
    return (cLightPathData*) ((u8*) pLitPath + ofs);
}

// The light path block.
LightPathHeader* cLightMgr::getPathHeader()
{
    return pLitPath;
}

// Fresh light: alive + attached flags, no lit index.
cLight::cLight()
{
    be_flag = 3;
    LitIndex = -1;
}

// Runs the light type function (LightFuncTbl[Type]).
void cLight::move()
{
    LightMgr.funcTbl[Type](this);
}

// Copies a data record into the live light and resolves its parent.
cLight& cLight::operator=(cLightWork& w)
{
    be_flag = w.BeFlag;
    xD = w.xD;
    Type = w.Type;
    xF = w.xF;
    Pos = w.Pos;
    Radius = w.Radius;
    Col = w.Col;
    Intensity = w.Intensity;
    ParentType = w.ParentType;
    Kind = w.Kind;
    Attribute = w.Attribute;
    Priority = w.Priority;
    ParentNo = w.ParentNo;
    HitRadius = w.HitRadius;
    Dummy82 = w.Dummy82;
    Dummy9 = w.Dummy9;
    setParent(w.ParentType, w.ParentNo);
    spot = w.spot;
    sub = w.sub;
    path = w.path;
    return *this;
}

// Copies a live light back into a data record (tool save).
cLightWork& cLightWork::operator=(cLight& l)
{
    BeFlag = l.be_flag;
    xD = l.xD;
    Type = l.Type;
    xF = l.xF;
    Pos = l.Pos;
    Radius = l.Radius;
    Col = l.Col;
    Intensity = l.Intensity;
    ParentType = l.ParentType;
    Kind = l.Kind;
    Attribute = l.Attribute;
    Priority = l.Priority;
    ParentNo = l.ParentNo;
    HitRadius = l.HitRadius;
    Dummy82 = l.Dummy82;
    Dummy9 = l.Dummy9;
    spot = l.spot;
    sub = l.sub;
    path = l.path;
    return *this;
}

// 1 when this is an alive room light (be_flag 4).
int cLight::checkScr()
{
    if (isAlive()) {
        if (be_flag & 4) {
            return 1;
        }
        return 0;
    }
    return 0;
}

// Sets the parent parts number (high 16 bits of ParentNo).
void cLight::setPartsNo(int pno)
{
    ParentNo = (pno << 16) | parent.no;
}

// Attaches the light to parent (type, id) and resolves the model pointer.
int cLight::setParent(u8 type, u32 no)
{
    ParentType = type;
    ParentNo = no;
    calcParent();
    return 1;
}

// Attaches to a model by searching it among enemies (type 1), scroll objects (2) and objects (4).
int cLight::setParent(cModel* pMod)
{
    u32 i;
    u32 n;

    if (!VALID_PTR(pMod)) {
        pLog->err(0, 0, "cLight::setParent() INVALID PTR %08x", pMod);
        return 0;
    }
    n = EmMgr.getArrayNum();
    for (i = 0; i < n; i++) {
        if ((cModel*) EmMgr.fastAt(i) == pMod) {
            setParent(1, (ParentNo & 0xFFFF0000) | pMod->id);
            return 1;
        }
    }
    for (i = 0; i < 250; i++) {
        if (SmdGetGroupObjPtr2(i) == pMod) {
            setParent(2, (ParentNo & 0xFFFF0000) | i);
            return 1;
        }
    }
    n = ObjMgr.getArrayNum();
    for (i = 0; i < n; i++) {
        if ((cModel*) ObjMgr.fastAt(i) == pMod) {
            setParent(4, (ParentNo & 0xFFFF0000) | i);
            return 1;
        }
    }
    return 0;
}

// Resolves pParent from ParentType/ParentNo: 1 enemy, 2 scroll object, 3 etc model, 4 object.
cModel* cLight::calcParent()
{
    switch (ParentType) {
    default:
        pLog->err(0, 0, "cLight::calcPos() INVALID PARENT TYPE %d    NO:%d", ParentType, ParentNo);
        ParentType = 0;
        ParentNo = 1;
    case 0:
        pParent = 0;
        break;
    case 1:
        pParent = EmMgr.getEmPtr((u8) parent.no, 0);
        break;
    case 2:
        pParent = SmdGetGroupObjPtr(parent.no);
        break;
    case 3:
        if (getRoomEtcOnLight(parent.no, &pParent, 0) == 0) {
            pParent = 0;
        }
        break;
    case 4:
        pParent = (cModel*) ObjMgr.fastAt(parent.no);
        break;
    }
    return pParent;
}

// The parent's parts the light follows (0 when unattached or dead).
cCoord* cLight::getCoord()
{
    cModel* p = pParent;
    int partsNo = parent.partsNo;

    if (p != 0 && p->isAlive() && partsNo < p->nParts) {
        return p->getPartsPtr(partsNo);
    }
    return 0;
}

// 1 when m is the parent model.
int cLight::isParent(cModel* pMod)
{
    return pMod == pParent;
}

// Local -> world position through the parent (calcPos).
int cLight::getPos2(Vec* pLiPos, Vec* pPos)
{
    return calcPos(pLiPos, pPos);
}

// Transforms a local position by the parent parts' matrix (etc models by number); unattached
// lights copy; a missing parent detaches the light (setTrans(0)).
int cLight::calcPos(Vec* pLiPos, Vec* pPos)
{
    cModel* p;
    cCoord* c;
    int partsNo;
    int no;

    if (!VALID_PTR(pPos)) {
        pLog->err(0, 0, "cLight::getPos() INVALED PTR %08x", pPos);
        return 0;
    }
    switch (ParentType) {
    default:
        pLog->err(0, 0, "Lit:calcPos() %d-%d INVALID PARENT TYPE", ParentType, ParentNo);
        setParent(0, 1);
    case 0:
        *pPos = *pLiPos;
        break;
    case 1:
    case 2:
    case 4:
        c = getCoord();
        if (c != 0) {
            PSMTXMultVec(c->mat, pLiPos, pPos);
        } else if (!(ParentType == 1 && parent.no == 3)) {
            if (G_ROOM_ID != 0x320) {
                pLog->err(0, 0, "Lit:calcPos() MODEL PARENT NOT FOUND");
            }
            setTrans(0);
        }
        break;
    case 3:
        partsNo = ParentNo >> 16;
        no = ParentNo & 0xFFFF;
        if (getRoomEtcOnLight(ParentNo, &p, 0) == 0) {
            p = 0;
            if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
                pLog->err(0, 0, "Lit:calcPos() %d-%d ETCMODEL PARENT NOT FOUND", no, partsNo);
            }
        } else if (p != 0 && p->isAlive() && partsNo < p->nParts) {
            PSMTXMultVec(p->getPartsPtr(partsNo)->mat, pLiPos, pPos);
        }
        break;
    }
    return 1;
}

// Rotates a local direction by the parent parts' matrix (enemy / scroll / etc / object parents).
int cLight::getNormal(Vec* pInNorm, Vec* pNorm)
{
    cModel* p;
    int partsNo;
    int no;

    if (!VALID_PTR(pNorm)) {
        pLog->err(0, 0, "cLight::getNormal() INVALED PTR %08x", pNorm);
        return 0;
    }
    switch (ParentType) {
    default:
        pLog->err(0, 0, "cLight::getPos() INVALID PARENT TYPE %d", ParentType);
    case 0:
        *pNorm = *pInNorm;
        break;
    case 1: {
        u32 pid = ParentNo;
        partsNo = pid >> 16;
        p = EmMgr.getEmPtr((u8) pid, 0);
        if (!(VALID_PTR(p) && p->isAlive() && partsNo < p->nParts)) {
            if (!(ParentType == 1 && parent.no == 3)) {
                if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
                    pLog->err(0, 0, "cLight::getNormal() FAILED.");
                    setTrans(0);
                }
            }
            *pNorm = *pInNorm;
            return 0;
        }
        PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, pInNorm, pNorm);
        break;
    }
    case 2: {
        u32 pid = ParentNo;
        no = pid & 0xFFFF;
        partsNo = pid >> 16;
        p = SmdGetGroupObjPtr(no);
        if (!VALID_PTR(p)) {
            pLog->err(0, 0, "cLight::getNormal() SCROLL No Error %d", no);
            *pNorm = *pInNorm;
            return 0;
        }
        if (p->isAlive() && partsNo < p->nParts) {
            PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, pInNorm, pNorm);
            if (pNorm->x == 0.0f && pNorm->y == 0.0f && pNorm->z == 0.0f) {
                pNorm->x = 0.001f;
            }
            break;
        }
        pLog->err(0, 0, "cLight::getNormal() FAILED.");
        *pNorm = *pInNorm;
        return 0;
    }
    case 3:
        partsNo = ParentNo >> 16;
        no = ParentNo & 0xFFFF;
        if (getRoomEtcOnLight(ParentNo, &p, 0) == 0) {
            p = 0;
            if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
                pLog->err(0, 0, "cLight::getNormal() ETCMODEL PARENT NOT FOUND %d %d", no, partsNo);
            }
        } else if ((p->be_flag & 1) && partsNo < p->nParts) {
            PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, pInNorm, pNorm);
        }
        break;
    case 4: {
        u32 pid = ParentNo;
        no = pid & 0xFFFF;
        partsNo = pid >> 16;
        p = ObjMgrWork(no);
        if (!(VALID_PTR(p) && p->isAlive() && partsNo < p->nParts)) {
            if (!DbgFlagChk(pG, DBG_TEST_MODE)) {
                pLog->err(0, 0, "cLight::getNormal() FAILED.");
            }
            *pNorm = *pInNorm;
            return 0;
        }
        PSMTXMultVecSR(p->getPartsPtr(partsNo)->mat, pInNorm, pNorm);
        break;
    }
    }
    return 1;
}

// Attaches (1) / detaches (0) the light from its parent (be_flag 2).
void cLight::setTrans(int on_off)
{
    if (on_off) {
        be_flag |= 2;
    } else {
        be_flag &= ~2;
    }
}

// Pushes the light out of the collision (SatMgr sphere test with HitRadius) from above the parent.
void cLight::hitAdjust()
{
    Vec pos;
    Vec top;

    if (HitRadius == 0.0f) {
        return;
    }
    pos = World;
    if (pParent != 0) {
        top.x = pParent->pos.x;
        top.y = pos.y;
        top.z = pParent->pos.z;
    } else {
        top = pos;
    }
    if (SatMgr.polySphereCk(&top, &pos, (f32) HitRadius, 0x80, 0, 0x8C2800) == 1) {
        World = pos;
    }
}

// Sets the spot direction (only for spot types xD 3 / 6).
void cLight::setSpotNormal(Vec* norm)
{
    if (xD != 3 && xD != 6) {
        pLog->err(0, 0, "lit.setSpot() TYPE ERROR");
        return;
    }
    normal = *norm;
}

// Aims the spot at a world point.
void cLight::setSpotTarget(Vec* pos)
{
    Vec n;

    PSVECSubtract(pos, &Pos, &n);
#line 2629 "D:/Bio4/Prog/light.cpp"
    VECNormalize(&n, &n);
    setSpotNormal(&n);
}

// Creates the item examine light (core block, cut 10, light 0).
void cLightMgr::setItemLight()
{
    cLight* l = create(0, 10, 0, 0);
    l->Kind = 0x7F;
}

// Event start: disables light kind 0x7F.
void cLightMgr::beginEvent()
{
    offKind(0x7F);
}

// Event end: re-enables kind 0x7F.
void cLightMgr::endEvent()
{
    onKind(0x7F);
}

// Tool: replaces the room light block.
void cLightMgr::dbSetRoomLit(cLit* pLit)
{
    pLitHeader = pLit;
    m_pLitRoom = pLit;
}

// Cut record `no` of the block (0 when absent).
cLightEnv* cLit::getCut(u16 no)
{
    cLightEnv* cut;

    u32* ofs = (u32*) (this + 1);

    if (no >= CutNum || ofs[no] == 0) {
        return 0;
    }
    cut = (cLightEnv*) ((u8*) this + ofs[no]);
    if (!VALID_PTR(cut)) {
        return 0;
    }
    return cut;
}

// Upgrades a light block from versions <= 0x2B to 0x2C in place (default lod/aniso, tev scales,
// tune, contrast, hokan, class bits, wind) and refreshes nMaxLight.
int cLit::versionUp()
{
    cLightEnv* cut;
    cLightWork* w;
    u32 i;
    u32 j;
    u32 max;
    int changed = 0;

    if (Version <= 0x20) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                cut->min_lod = 0;
                cut->max_lod = 5;
                cut->aniso = 0;
                cut->lod_bias = 0.0f;
            }
        }
    }
    if (Version <= 0x22) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    if (w->Kind != 0) {
                        pLog->warn(0, 0, "LitVer CAUTION %d", w->Kind);
                        changed = 1;
                        w->Kind = 0;
                    }
                }
            }
        }
    }
    if (Version <= 0x23) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->AmbientEsp = cut->AmbientScr;
                cut->AmbientEm = cut->AmbientScr;
            }
        }
    }
    if (Version <= 0x24) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    if (w->Type == 1) {
                        changed = 1;
                        w->Col = w->sub.color;
                    }
                }
            }
        }
    }
    if (Version <= 0x25) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->tev_scale[1] = 2;
                cut->tev_scale[0] = 2;
            }
        }
    }
    if (Version <= 0x26) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->tuneOn = 0;
                cut->Tune[0].r = 0;
                cut->Tune[0].g = 0;
                cut->Tune[0].b = 0;
                cut->Tune[0].a = 0;
                cut->Tune[1].r = 0;
                cut->Tune[1].g = 0;
                cut->Tune[1].b = 0;
                cut->Tune[1].a = 0;
                cut->Tune[2].r = 0;
                cut->Tune[2].g = 0;
                cut->Tune[2].b = 0;
                cut->Tune[2].a = 0;
            }
        }
    }
    if (Version <= 0x27) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                changed = 1;
                cut->contrast[0] = 0;
                cut->contrast[1] = 0;
                cut->contrast[2] = 0;
            }
        }
    }
    if (Version <= 0x28) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                cut->Hokan = 0;
                changed = 1;
            }
        }
    }
    if (Version <= 0x29) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    if (w->xF & 1) {
                        w->xF |= 0x40;
                        changed = 1;
                    }
                }
            }
        }
    }
    if (Version <= 0x2A) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                for (j = 0; j < cut->nLight; j++) {
                    w = cut->getLightWork(j);
                    w->Priority = 3;
                }
            }
        }
    }
    if (Version <= 0x2B) {
        for (i = 0; i < CutNum; i++) {
            if (VALID_PTR(cut = getCut(i))) {
                cut->wind.direction = 0;
                cut->wind.power = 0;
                cut->wind.frequency = 0;
            }
        }
    }
    max = getMaxLight();
    if (nMaxLight != max) {
        pLog->warn(0, 0, "cLit::versionUp() nMaxLight %d -> %d", nMaxLight, max);
        nMaxLight = max;
    }
    if (changed == 1) {
        pLog->warn(0, 0, "cLit::versionUp() VERSION UP");
    }
    Version = 0x2C;
    for (i = 0; i < CutNum; i++) {
        if (VALID_PTR(cut = getCut(i))) {
            if (cut->tev_scale[0] > 2) {
                cut->tev_scale[0] = 2;
            }
            if (cut->tev_scale[1] > 2) {
                cut->tev_scale[1] = 2;
            }
        }
    }
    return 1;
}

// Largest light count of any cut in the block.
u32 cLit::getMaxLight()
{
    cLightEnv* cut;
    u32 max = 0;
    u32 i;

    for (i = 0; i < CutNum; i++) {
        cut = getCut(i);
        if (cut != 0 && cut->nLight > max) {
            max = cut->nLight;
        }
    }
    return max;
}

// Sub screen (inventory) in: keep only the first 10 works alive for the item lights.
u32 nArrayBak;
cLight* pAliveBak;

// Entering the sub screen: drops the room lights, limits the pool to 10 and disables kind 0x7F.
void cLightMgr::inSscrn()
{
    deleteScr();
    nArrayBak = nArray;
    nArray = 10;
    pAliveBak = pAlive;
    offKind(0x7F);
}

// Leaving the sub screen: restores the pool and reloads the lights (mode 0 cut 0, 1 the camera's
// area cut, 2 thermal) and kind 0x7F.
void cLightMgr::outSscrn(u32 mode)
{
    nArray = nArrayBak;
    pAlive = pAliveBak;
    switch (mode) {
    case 0:
    default:
        LightMgr.update(0, 0);
        break;
    case 1:
        LightMgr.update(CamCtrl.areaNo, 0);
        break;
    case 2:
        StaFlagOn(pG, STA_THERMO_GRAPH);
        LightMgr.setThermo();
        break;
    }
    LightMgr.onKind(0x7F);
}

// Applies the cut's wind (direction in 1/127 pi, power and frequency in 1/100) to the pendulum/cloth wind.
void cPenWind::set()
{
    PenWindSet((f32) direction * 3.1415927f / 127.0f, (f32) power * 0.01f, (f32) frequency * 0.01f);
}

cLightMgr LightMgr;
