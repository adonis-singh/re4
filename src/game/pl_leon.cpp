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
#include <dolphin/os.h>
#include "pl_sub.h"
#include "esp.h"
#include "pl_npc.h"
#include "pl_mod.h"


// Plain block, not do/while(0): the do-while's deleted back-jump lets cse rewrite the HALT store's
// zero as `info` (one more ref), which makes `info` outrank `data` in global allocation order.

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.

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
    EspDataLoad((u32) PL_ARC_PTR(arc, 0x1A), EFF_PL00, 0);
    startUp();
    pFsdTbl = pl_fs_tbl;
}

// Fills m_MotTbl 0x5F..0x6C (the ladder / crouch / partner-command motions) from archive 0x32..0x3F.
void cPlLeon::setMotion()
{
    PLA_MOT(this, 0x5F, 0x32);
    PLA_MOT(this, 0x60, 0x33);
    PLA_MOT(this, 0x61, 0x34);
    PLA_MOT(this, 0x62, 0x35);
    PLA_MOT(this, 0x63, 0x36);
    PLA_MOT(this, 0x64, 0x37);
    PLA_MOT(this, 0x65, 0x38);
    PLA_MOT(this, 0x66, 0x39);
    PLA_MOT(this, 0x6B, 0x3A);
    PLA_MOT(this, 0x6C, 0x3B);
    PLA_MOT(this, 0x67, 0x3C);
    PLA_MOT(this, 0x68, 0x3D);
    PLA_MOT(this, 0x69, 0x3E);
    m_MotTbl[0x6A] = PL_ARC_PTR(pG->pPlayer, 0x3F);
}

// Just cPlayer::move (the cloth runs through the moveCloth virtual).
void cPlLeon::move()
{
    cPlayer::move();
}

// Loads the body (archive 4/5) and adds the costume extras (0xA for costume 0, 0x10 for 1-3), the
// face (0xD, Body->m_pKnife), head shape (8, pShape / pHeadData), hair (6) and eyes (9, be_flag
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
    Body->m_pKnife = info;
    face = Body->m_pKnife;
    if (VALID_PTR(face)) {
        face->mat[2][2] = 0.0f;
        face->mat[1][1] = 0.0f;
        face->mat[0][0] = 0.0f;
    }
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 8), PL_ARC_PTR(pG->pPlayer, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    Body->m_pFace = info;
    Body->pHeadData = PL_ARC_PTR(pG->pPlayer, 8);
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 6), PL_ARC_PTR(pG->pPlayer, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    Body->m_pHead = info;
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 9), PL_ARC_PTR(pG->pPlayer, 7));
    if (!VALID_PTR(info)) {
        pLog->err(0, 0, "cPlLeon::setModel() failed.");
        return;
    }
    addModel(info);
    info->be_flag |= 0x40;
    Body->m_pHair = info;
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
    setTevScaleGroup(1);
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
void cPlLeon::setRightHand(int type)
{
    void* data;
    cModelInfo* info;

    if (Body->m_pHandR) {
        deleteModelInfo(Body->m_pHandR);
        Body->m_pHandR = 0;
        Body->pRightData = 0;
    }
    switch (type) {
    case 0:
        data = PL_ARC_PTR(pG->pPlayer, 0x12);
        break;
    case 1:
        data = Body->pWepHand;
        break;
    default:
        data = (void*) type;
        break;
    }
    if ((info = ModInfoMgr.create((void*) data, PL_ARC_PTR(pG->pPlayer, 0x11))) != 0) {
        addModel(info);
        Body->m_pHandR = info;
        Body->pRightData = (void*) data;
    }
    if (!info) {
#line 353 "D:/Bio4/Prog/pl_leon.cpp"
        HALT();
    }
}

// Left hand model 0-5 (archive 0x14..0x19: open, closed, the weapon grips); 0x63 = the previous
// hand again (oldLhandNo).
void cPlLeon::setLeftHand(u32 type)
{
    cModelInfo* info;
    void* data;

    if (Body->m_pHandL) {
        deleteModelInfo(Body->m_pHandL);
        Body->m_pHandL = 0;
        Body->pLeftData = 0;
    }
    if (type == 0x63) {
        type = Body->oldLhandNo;
    }
    switch (type) {
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
        data = (void*) type;
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = type;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pG->pPlayer, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlLeon::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->m_pHandL = info;
        Body->pLeftData = data;
    }
}

// Face morph on the head shape: 0 ends the morph (neutral), 1 pain (archive 0x62), 2 (0x63).
void cPlLeon::setFace(int type)
{
    void* data = 0;
    void* shape = Body->m_pFace;

    if (shape == 0) {
        return;
    }
    switch (type) {
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
    if (type != 0) {
        ShapeSet(Body->m_pFace, 0, data, 2);
    }
}

// no == 0: replaces the morphable head + hair + eyes with the plain head model (archive 0xB/7) —
// used when the head is swapped for an event.
void cPlLeon::setHead(int type)
{
    cModelInfo* info;

    if (type != 0) {
        return;
    }
    if (Body->m_pFace == 0) {
        return;
    }
    deleteModelInfo(Body->m_pFace);
    Body->m_pFace = 0;
    deleteModelInfo(Body->m_pHead);
    Body->m_pHead = 0;
    deleteModelInfo(Body->m_pHair);
    Body->m_pHair = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xB), PL_ARC_PTR(pG->pPlayer, 7));
    if (info) {
        addModel(info);
    }
}

// Replaces the morphable head + hair + eyes with the given head model.
void cPlLeon::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (Body->m_pFace == 0) {
        return;
    }
    deleteModelInfo(Body->m_pFace);
    Body->m_pFace = 0;
    deleteModelInfo(Body->m_pHead);
    Body->m_pHead = 0;
    deleteModelInfo(Body->m_pHair);
    Body->m_pHair = 0;
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
        SndCall(1, 0x37, &pList->world, 0, 0, 0);
        SubCharCtrl(1, 0);
    } else {
        SndCall(1, 0x36, &pList->world, 0, 0, 0);
        SubCharCtrl(0, 0);
    }
    m_CmdTimer = 8;
    StaFlagOn(pG, STA_PL_FIRE);
    return 1;
}
