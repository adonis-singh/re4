// pl0d module, third object (D:/Bio4/Prog/pl_wesker.cpp): Wesker: the jacket cloth chain (allocated on
// initCloth), the player class with Leon's motion table and model set (body, hair, head, face shapes).
//
// cPlWesker (pl_mod.h) is the cPlayer of pl_type 5 (Wesker in the mercenaries): the constructor
// builds the model set from the player archive (4/5 body, 0xA extra part, 8/7 head with the face
// shapes, 6/7 hair, 0x11 hand texture, 0x12 right hand, 0x14/0x16/0x18 left hands, 0x62/0x63 face
// shapes), loads / inits the weapon module and installs the event motions 0x5F..0x6C. The jacket
// is a 24-part pendulum cloth chain (weskerJacketP, 4 bundles, PenClothMove3) whose PlCloth work
// is MemAlloc'd on initCloth. Pl0dInit is the module's PlInitFunc; the module's first two objects
// are the Punisher class and the handgun routines rebuilt without their entry points.

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "pendulum.h"
#include "db_log.h"
#include "esp.h"
#include "main_mem.h"

extern "C" void OSReport(const char* fmt, ...);

// Plain block, not do/while(0) (pl_leon.cpp).
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModelInfo*& d, cModelInfo* v) { d = v; }

// weskerJacket (the chain tables; the collision volumes are a global: REL field A = 0)
static u8 weskerJacketP[24] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87};
static u8 weskerJacketLp[24] = {66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 0xFF, 0xFF};
static u8 weskerJacketUp[24] = {0xFF, 64, 0xFF, 66, 0xFF, 68, 0xFF, 70, 0xFF, 72, 0xFF, 74, 0xFF, 76, 0xFF, 78, 0xFF, 80, 0xFF, 82, 0xFF, 84, 0xFF, 86};
static u8 weskerJacketDp[24] = {65, 0xFF, 67, 0xFF, 69, 0xFF, 71, 0xFF, 73, 0xFF, 75, 0xFF, 77, 0xFF, 79, 0xFF, 81, 0xFF, 83, 0xFF, 85, 0xFF, 87, 0xFF};
static f32 weskerJacketMax[24] = {0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f, 0.2f, 0.3f};
static f32 weskerJacketWindS[24] = {0.0f, 0.0f, 0.4f, 0.4f, 0.9f, 0.9f, 1.2f, 1.2f, 1.5f, 1.5f, 1.7f, 1.7f, 1.9f, 1.9f, 2.1f, 2.1f, 2.4f, 2.4f, 2.8f, 2.8f, 3.1f, 3.1f, -2.8f, -2.8f};
static f32 weskerJacketWindR[24] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
CLOTH_AT_SET weskerJacketAt[6] = {
    {0x0000, 0x11, 0x11, 1.0f, 130.0f, {-30.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x11, 1.0f, 130.0f, {30.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x12, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x11, 0x16, 0.4f, 125.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x12, 0x12, 1.0f, 120.0f, {30.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {0x0000, 0x16, 0x16, 1.0f, 120.0f, {-30.0f, -50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
};

static PlCloth* weskerJacket;

// Sets up Wesker's jacket as a pendulum cloth chain: 24 parts (weskerJacketP) in 4 bundles,
// linked parent/child (weskerJacketUp/Dp) and sideways (weskerJacketLp), sway limits 0.2 / 0.3,
// per-part wind phase / rate, 6 collision spheres on the hips / legs (weskerJacketAt), gravity
// 25, damping 0.5, stretch 0.1, flags 0x100; PenClothSet initialises the chain 100 units long.
void testJacketSetWesker(cModel* pl, PlCloth* c)
{
    f32 rate;

    c->Num = 24;
    c->pCloth = weskerJacketP;
    c->pLeft = weskerJacketLp;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->pUpRight = 0;
    c->pParent = weskerJacketUp;
    c->pChild = weskerJacketDp;
    c->pGravity = 0;
    c->pRate = 0;
    c->pMax = weskerJacketMax;
    c->pWindSin = weskerJacketWindS;
    c->pWindRate = weskerJacketWindR;
    c->pAtset = weskerJacketAt;
    c->At_num = 6;
    c->Gravity = 25.0f;
    rate = 0.5f;
    c->Bundle_num = 4;
    c->WindSin = 0.0f;
    c->Stretchy = 0.1f;
    c->pModel = 0;
    c->Rate = rate;
    c->Move_rate = rate;
    c->Flag = 0x100;
    c->pPtbl = 0;
    PenClothSet(pl, (PenCloth*) c, 100.0f);
}

// Per-frame update of the jacket chain (the sideways-linked pendulum variant).
void testJacketMoveWesker(cModel* pl, PlCloth* c)
{
    PenClothMove3(pl, (PenCloth*) c);
}

// Cloth set-up (cPlWesker::initCloth): the jacket chain.
void PlClothSetWesker(cModel* pl, PlCloth* jacket)
{
    testJacketSetWesker(pl, jacket);
}

// Cloth update (cPlWesker::moveCloth): the jacket, then the model's be_flag bits 21..23 (the
// cloth "just set" flags) are cleared.
void PlClothMoveWesker(cModel* pl, PlCloth* jacket)
{
    testJacketMoveWesker(pl, jacket);
    pl->be_flag &= ~0x00E00000;
}

// Builds Wesker: the cPlayer work init, the model set, the equipped weapon module, the routine
// init, the event motions, the player effects (archive 0x1A as group 3), startUp and the players'
// foot shadow table.
cPlWesker::cPlWesker()
{
    PlArc* arc;

    init0();
    setModel();
    weaponRelease();
    weaponLoad(pG->weapon_no, pG->weapon_type);
    weaponInit();
    init1();
    setMotion();
    arc = pG->pPlayer;
    EspDataLoad((u32) PL_ARC_PTR(arc, 0x1A), 3, 0);
    startUp();
    pFootShadowTbl = pl_fs_tbl;
}

// Installs the character's event / action motions (m_MotTbl 0x5F..0x6C from the player archive
// 0x32..0x3F); the weapon module fills the footwork slots.
void cPlWesker::setMotion()
{
    PSet(m_MotTbl[0x5F], PL_ARC(0x32));
    PSet(m_MotTbl[0x60], PL_ARC(0x33));
    PSet(m_MotTbl[0x61], PL_ARC(0x34));
    PSet(m_MotTbl[0x62], PL_ARC(0x35));
    PSet(m_MotTbl[0x63], PL_ARC(0x36));
    PSet(m_MotTbl[0x64], PL_ARC(0x37));
    PSet(m_MotTbl[0x65], PL_ARC(0x38));
    PSet(m_MotTbl[0x66], PL_ARC(0x39));
    PSet(m_MotTbl[0x6B], PL_ARC(0x3A));
    PSet(m_MotTbl[0x6C], PL_ARC(0x3B));
    PSet(m_MotTbl[0x67], PL_ARC(0x3C));
    PSet(m_MotTbl[0x68], PL_ARC(0x3D));
    PSet(m_MotTbl[0x69], PL_ARC(0x3E));
    PSet(m_MotTbl[0x6A], PL_ARC(0x3F));
}

// Per-frame update: the common cPlayer::move.
void cPlWesker::move()
{
    cPlayer::move();
}

// Builds the model set: the body (4/5) as the base model, an extra part (0xA/5), the head with the
// face shape data (8/7, Body->pShape / pHeadData), the hair (6/7, Body->pHair); TEV scale group
// 1, neutral face, bare hands.
void cPlWesker::setModel()
{
    cModelInfo* info;

    info = (cModelInfo*) modelInit(PL_ARC(4), PL_ARC(5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    info = ModInfoMgr.create(PL_ARC(0xA), PL_ARC(5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    addModel(info);
    info = ModInfoMgr.create(PL_ARC(8), PL_ARC(7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(Body->pShape, info);
    PSet(Body->pHeadData, PL_ARC(8));
    info = ModInfoMgr.create(PL_ARC(6), PL_ARC(7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlWesker::setModel() failed.");
        return;
    }
    addModel(info);
    Body->pHair = info;
    TevScaleGroup = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(0);
}

// Right hand model: 0 bare (0x12), 1 the weapon module's hand (Body->pWepHand), any other value
// a model data pointer; replaces Body->pRight. HALTs (pl_wesker.cpp line 515) when the model
// info cannot be created.
void cPlWesker::setRightHand(int no)
{
    cModelInfo* info;
    void* data;

    if (Body->pRight) {
        deleteModelInfo(Body->pRight);
        Body->pRight = 0;
        Body->pRightData = 0;
    }
    switch (no) {
    case 0:
        data = PL_ARC(0x12);
        break;
    case 1:
        data = Body->pWepHand;
        break;
    default:
        data = (void*) no;
        break;
    }
    if ((info = ModInfoMgr.create(data, PL_ARC(0x11))) != 0) {
        addModel(info);
        Body->pRight = info;
        Body->pRightData = data;
    }
    if (!info) {
#line 515 "D:/Bio4/Prog/pl_wesker.cpp"
        HALT();
    }
}

// Left hand model: 0 bare (0x14), 2 (0x16), 4 (0x18), 0x63 = the previous one, any other value a
// model data pointer; replaces Body->pLeft.
void cPlWesker::setLeftHand(u32 no)
{
    cModelInfo* info;
    void* data;

    if (Body->pLeft) {
        deleteModelInfo(Body->pLeft);
        Body->pLeft = 0;
        Body->pLeftData = 0;
    }
    if (no == 0x63) {
        no = Body->oldLhandNo;
    }
    switch (no) {
    case 0:
        data = PL_ARC(0x14);
        break;
    case 2:
        data = PL_ARC(0x16);
        break;
    case 4:
        data = PL_ARC(0x18);
        break;
    default:
        data = (void*) no;
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlayer, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlWesker::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->pLeft = info;
        Body->pLeftData = data;
    }
}

// Face expression: 0 ends the shape blend (neutral), 1 / 2 blend the face shapes 0x62 / 0x63
// onto the head model.
void cPlWesker::setFace(int no)
{
    void* data = 0;
    void* shape = Body->pShape;

    if (shape == 0) {
        return;
    }
    switch (no) {
    case 0:
    default:
        ShapeEnd(shape);
        break;
    case 1:
        data = PL_ARC(0x62);
        break;
    case 2:
        data = PL_ARC(0x63);
        break;
    }
    if (no != 0) {
        ShapeSet(Body->pShape, 0, data, 2);
    }
}

// Head swap by number (only 0 does anything): drops the head and hair models and adds the
// archive's event head 0xB.
void cPlWesker::setHead(int no)
{
    cModelInfo* info;

    if (no != 0) {
        return;
    }
    if (Body->pShape == 0) {
        return;
    }
    deleteModelInfo(Body->pShape);
    Body->pShape = 0;
    deleteModelInfo(Body->pHair);
    Body->pHair = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pGS->pPlayer, 0xB), PL_ARC_PTR(pGS->pPlayer, 7));
    if (info) {
        addModel(info);
    }
}

// Head swap with explicit model / texture data (events): drops the head, hair and eye models
// and adds the given one.
void cPlWesker::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (Body->pShape == 0) {
        return;
    }
    deleteModelInfo(Body->pShape);
    Body->pShape = 0;
    deleteModelInfo(Body->pHair);
    Body->pHair = 0;
    deleteModelInfo(Body->pEye);
    Body->pEye = 0;
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        addModel(info);
    }
}

// Cloth set-up (cPlayer::startUp): allocates the jacket's PlCloth work and initialises the chain.
void cPlWesker::initCloth()
{
    weskerJacket = (PlCloth*) MemAlloc(sizeof(PlCloth), 1);
    if (weskerJacket) {
        PlClothSetWesker(this, weskerJacket);
    }
}

// Per-frame cloth update (cPlayer::move) while the jacket work exists.
void cPlWesker::moveCloth()
{
    if (weskerJacket) {
        PlClothMoveWesker(this, weskerJacket);
    }
}

// PlInitFunc: placement-constructs Wesker in the player's cEm work.
void Pl0dInit(cEm* em)
{
    new (em) cPlWesker();
}

// REL entry: registers the player constructor.
extern "C" void _prolog()
{
    PlInitFunc = Pl0dInit;
    OSReport("Pl0d WESKER prolog Ok\n");
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
