// game/pl_leon: cPlLeon, the player class for Leon and the other gun-carrying characters (the
// character is chosen by pl_type / costume in the archive data): model set (body, costume extras,
// face morph head, hair, eyes, wound overlay), weapon load, the extra motions, hands and face
// morphs, and the partner command key (checkXbutton). Cloth runs through pl_cloth.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "snd.h"

extern "C" {
void OSReport(const char* fmt, ...);
void EspDataLoad(void* data, int a, int b);     // game/eff_sys.cpp
int SubCharCheckCtrl();                         // game/pl_sub.cpp
void SubCharCtrl(int mode, int sccf);                 // game/pl_sub.cpp
}
u32 SubCharGetStatus();                         // game/pl_npc.cpp
void ShapeSet(void* info, int a, void* data, int b);  // game/shape.cpp
void ShapeEnd(void* info);

extern cModel* pSUB;
extern u8 pl_fs_tbl[];   // game/foot_shadow_tbl.cpp (incomplete type: full address, not @sda21)

// Plain block, not do/while(0): the do-while's deleted back-jump lets cse rewrite the HALT store's
// zero as `info` (one more ref), which makes `info` outrank `data` in global allocation order.
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.
static inline void PSet(void*& d, void* v) { d = v; }
static inline void PSet(cModelInfo*& d, cModelInfo* v) { d = v; }

// Builds the main player (Leon, and the other gun-carrying characters through pl_type / costume):
// common init, model set, the equipped weapon (weapon_no / weapon_type) and its motion table, the
// extra motions, effect data (archive 0x1A), foot shadows.
cPlLeon::cPlLeon()
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
    EspDataLoad(PL_ARC_PTR(arc, 0x1A), 3, 0);
    startUp();
    pFootShadowTbl = pl_fs_tbl;
}

// Fills m_MotTbl 0x5F..0x6C (the ladder / crouch / partner-command motions) from archive 0x32..0x3F.
void cPlLeon::setMotion()
{
    PSet(m_MotTbl[0x5F], PL_ARC_PTR(pG->pPlayer, 0x32));
    PSet(m_MotTbl[0x60], PL_ARC_PTR(pG->pPlayer, 0x33));
    PSet(m_MotTbl[0x61], PL_ARC_PTR(pG->pPlayer, 0x34));
    PSet(m_MotTbl[0x62], PL_ARC_PTR(pG->pPlayer, 0x35));
    PSet(m_MotTbl[0x63], PL_ARC_PTR(pG->pPlayer, 0x36));
    PSet(m_MotTbl[0x64], PL_ARC_PTR(pG->pPlayer, 0x37));
    PSet(m_MotTbl[0x65], PL_ARC_PTR(pG->pPlayer, 0x38));
    PSet(m_MotTbl[0x66], PL_ARC_PTR(pG->pPlayer, 0x39));
    PSet(m_MotTbl[0x6B], PL_ARC_PTR(pG->pPlayer, 0x3A));
    PSet(m_MotTbl[0x6C], PL_ARC_PTR(pG->pPlayer, 0x3B));
    PSet(m_MotTbl[0x67], PL_ARC_PTR(pG->pPlayer, 0x3C));
    PSet(m_MotTbl[0x68], PL_ARC_PTR(pG->pPlayer, 0x3D));
    PSet(m_MotTbl[0x69], PL_ARC_PTR(pG->pPlayer, 0x3E));
    PSet(m_MotTbl[0x6A], PL_ARC_PTR(pG->pPlayer, 0x3F));
}

// Just cPlayer::move (the cloth runs through the moveCloth virtual).
void cPlLeon::move()
{
    cPlayer::move();
}

// Loads the body (archive 4/5) and adds the costume extras (0xA for costume 0, 0x10 for 1-3), the
// face (0xD, Body->pFace), head shape (8, pShape / pHeadData), hair (6) and eyes (9, be_flag
// 0x40), then the default face, empty right hand and left hand 1.
void cPlLeon::setModel()
{
    cModelInfo* info;
    cModelInfo* face;

    info = (cModelInfo*) modelInit(PL_ARC_PTR(pG->pPlayer, 4), PL_ARC_PTR(pG->pPlayer, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    switch (pG->pl_costume) {
    case 0:
    case 1:
    case 2:
    case 3:
        info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xA), PL_ARC_PTR(pG->pPlayer, 5));
        if (!VALID_PTR(info)) {
            pLog->err(0, 0, "cPlLeon::setModel() failed.");
            return;
        }
        addModel(info);
        break;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xD), PL_ARC_PTR(pG->pPlayer, 5));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    Body->pFace = info;
    face = Body->pFace;
    if (VALID_PTR(face)) {
        face->x84 = 0.0f;
        face->x70 = 0.0f;
        face->x5C = 0.0f;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 8), PL_ARC_PTR(pG->pPlayer, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(Body->pShape, info);
    PSet(Body->pHeadData, PL_ARC_PTR(pG->pPlayer, 8));
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 6), PL_ARC_PTR(pG->pPlayer, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    PSet(Body->pHair, info);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 9), PL_ARC_PTR(pG->pPlayer, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    info->be_flag |= 0x40;
    PSet(Body->pEye, info);
    if (pG->pl_costume >= 1 && pG->pl_costume <= 3) {
        info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0x10), PL_ARC_PTR(pG->pPlayer, 5));
        if (!VALID_PTR(info)) {
            pLog->err(0, 0, "cPlLeon::setModel() failed.");
            return;
        }
        addModel(info);
    }
    if (ScfFlagChk(pG, SCF_R317_LEON_WOUND)) {
        setWound();
    }
    TevScaleGroup = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(1);
}

// Adds the wounded-arm overlay model (archive 0xE/0xF) — the chapter 5 injured Leon.
void cPlLeon::setWound()
{
    cModelInfo* info;

    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xE), PL_ARC_PTR(pG->pPlayer, 0xF));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
    } else {
        addModel(info);
    }
}

// Right hand model: 0 empty (archive 0x12), 1 the weapon grip hand (Body->pWepHand), anything else
// is taken as model data itself. Texture 0x11. A create failure HALTs.
void cPlLeon::setRightHand(int no)
{
    void* data;
    cModelInfo* info;

    if (Body->pRight) {
        deleteModelInfo(Body->pRight);
        Body->pRight = 0;
        Body->pRightData = 0;
    }
    switch (no) {
    case 0:
        data = PL_ARC_PTR(pG->pPlayer, 0x12);
        break;
    case 1:
        data = Body->pWepHand;
        break;
    default:
        data = (void*) no;
        break;
    }
    if ((info = ModInfoMgr.create((void*) data, PL_ARC_PTR(pG->pPlayer, 0x11))) != 0) {
        addModel(info);
        Body->pRight = info;
        Body->pRightData = (void*) data;
    }
    if (!info) {
#line 353 "D:/Bio4/Prog/pl_leon.cpp"
        HALT();
    }
}

// Left hand model 0-5 (archive 0x14..0x19: open, closed, the weapon grips); 0x63 = the previous
// hand again (oldLhandNo).
void cPlLeon::setLeftHand(u32 no)
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
        data = PL_ARC_PTR(pG->pPlayer, 0x14);
        break;
    case 1:
        data = PL_ARC_PTR(pG->pPlayer, 0x15);
        break;
    case 2:
        data = PL_ARC_PTR(pG->pPlayer, 0x16);
        break;
    case 3:
        data = PL_ARC_PTR(pG->pPlayer, 0x17);
        break;
    case 4:
        data = PL_ARC_PTR(pG->pPlayer, 0x18);
        break;
    case 5:
        data = PL_ARC_PTR(pG->pPlayer, 0x19);
        break;
    default:
        data = (void*) no;
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pGS->pPlayer, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlLeon::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->pLeft = info;
        Body->pLeftData = data;
    }
}

// Face morph on the head shape: 0 ends the morph (neutral), 1 pain (archive 0x62), 2 (0x63).
void cPlLeon::setFace(int no)
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
        data = PL_ARC_PTR(pG->pPlayer, 0x62);
        break;
    case 2:
        data = PL_ARC_PTR(pG->pPlayer, 0x63);
        break;
    }
    if (no != 0) {
        ShapeSet(Body->pShape, 0, data, 2);
    }
}

// no == 0: replaces the morphable head + hair + eyes with the plain head model (archive 0xB/7) —
// used when the head is swapped for an event.
void cPlLeon::setHead(int no)
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
    deleteModelInfo(Body->pEye);
    Body->pEye = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pGS->pPlayer, 0xB), PL_ARC_PTR(pGS->pPlayer, 7));
    if (info) {
        addModel(info);
    }
}

// Replaces the morphable head + hair + eyes with the given head model.
void cPlLeon::setHead(void* bin, void* tpl)
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

// Partner command key (Key 0x200, every 8 frames at most) while Ashley (pSUB id 3) follows:
// toggles her between "wait" (SubCharCtrl 1) and "follow" (0) with the call SE; Status_flg[1] bit2
// = the partner command is available. Returns 1 when a command was issued.
int cPlLeon::checkXbutton()
{
    if (m_CmdTimer) {
        m_CmdTimer--;
    }
    if (pSUB == 0) {
        return 0;
    }
    if (pSUB->id != 3) {
        return 0;
    }
    StaFlagOn(pG, STA_SUBCHAR_CTRL);
    if (m_CmdTimer != 0) {
        return 0;
    }
    if (SubCharCheckCtrl() == 0) {
        return 0;
    }
    if (!(Key.trg & 0x200)) {
        return 0;
    }
    if (SubCharGetStatus() & 0x40000000) {
        SndCall(1, 0x37, &pParts->world, 0, 0, 0);
        SubCharCtrl(1, 0);
    } else {
        SndCall(1, 0x36, &pParts->world, 0, 0, 0);
        SubCharCtrl(0, 0);
    }
    m_CmdTimer = 8;
    StaFlagOn(pG, STA_PL_FIRE);
    return 1;
}
