// pl06 module (D:/Bio4/Prog/pl_hunk.cpp): HUNK, the mercenaries player: model set (body, hair, hands), the
// Leon motion table, no face shapes and no cloth.
//
// cPlHunk (pl_mod.h) is the cPlayer of pl_type 3: the constructor builds the model set from the
// player archive (4/5 body, 6/7 the mask "hair", 0x11 hand texture, 0x12 right hand, 0x14/0x15
// left hands), loads / inits the weapon module and installs the event motions 0x5F..0x6C.
// Pl06Init is the module's PlInitFunc. The HALT in setRightHand is line 253 of the vendor file.

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "db_log.h"
#include "esp.h"
#include <dolphin/os.h>


// Plain block, not do/while(0) (pl_leon.cpp).

// Store through a reference: a scalar (non-struct) MEM, so pG is reloaded after every store.

// Builds HUNK: the cPlayer work init (init0), the model set, the equipped weapon module, the
// routine init (init1), the event motions, the player effects (archive 0x1A as group 3), startUp
// and the players' foot shadow table.
cPlHunk::cPlHunk()
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

// Installs the character's event / action motions (m_MotTbl 0x5F..0x6C from the player archive
// 0x32..0x3F); the weapon module fills the footwork slots.
void cPlHunk::setMotion()
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
    PLA_MOT(this, 0x6A, 0x3F);
}

// Per-frame update: the common cPlayer::move.
void cPlHunk::move()
{
    cPlayer::move();
}

// Builds the model set: the body (4/5) as the base model, the mask / hair (6/7, Body->m_pHead), a
// first right (0x12) and left (0x14) hand; TEV scale group 1, then the hands are re-set through
// setRightHand(0) / setLeftHand(1). (The error string still says cSubLuis.)
void cPlHunk::setModel()
{
    cModelInfo* info;

    if (modelInit(PL_ARC(4), PL_ARC(5)) == 0) {
        pLog->err(0, 0, "cSubLuis::init() failed.");
    }
    if ((info = ModInfoMgr.create(PL_ARC(6), PL_ARC(7))) != 0) {
        addModel(info);
        Body->m_pHead = info;
    }
    if ((info = ModInfoMgr.create(PL_ARC(0x12), PL_ARC(0x11))) != 0) {
        addModel(info);
        Body->m_pHandR = info;
    }
    if ((info = ModInfoMgr.create(PL_ARC(0x14), PL_ARC(0x11))) != 0) {
        addModel(info);
        Body->m_pHandL = info;
    }
    TevScaleGroup = 1;
    setFace(0);
    setRightHand(0);
    setLeftHand(1);
}

// Right hand model: 0 bare (0x12), 1 the weapon module's hand (Body->pWepHand), any other value
// a model data pointer; replaces Body->pRight. HALTs (pl_hunk.cpp line 253) when the model
// info cannot be created.
void cPlHunk::setRightHand(int no)
{
    cModelInfo* info;
    void* data;

    if (Body->m_pHandR) {
        deleteModelInfo(Body->m_pHandR);
        Body->m_pHandR = 0;
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
        Body->m_pHandR = info;
        Body->pRightData = data;
    }
    if (!info) {
#line 253 "D:/Bio4/Prog/pl_hunk.cpp"
        HALT();
    }
}

// Left hand model: 0 bare (0x14), 1 / 3 the gripping hand (0x15), 0x63 = the previous one, any
// other value a model data pointer; replaces Body->pLeft.
void cPlHunk::setLeftHand(u32 no)
{
    cModelInfo* info;
    void* data;

    if (Body->m_pHandL) {
        deleteModelInfo(Body->m_pHandL);
        Body->m_pHandL = 0;
        Body->pLeftData = 0;
    }
    if (no == 0x63) {
        no = Body->oldLhandNo;
    }
    switch (no) {
    case 0:
        data = PL_ARC(0x14);
        break;
    case 1:
    case 3:
        data = PL_ARC(0x15);
        break;
    default:
        data = (void*) no;
        break;
    }
    Body->oldLhandNo = Body->nowLhandNo;
    Body->nowLhandNo = no;
    info = ModInfoMgr.create(data, PL_ARC_PTR(pG->pPlayer, 0x11));
    if (info == 0) {
        pLog->err(0, 0, "cPlHunk::setLeftHand() ModInfoMgr.create() failed");
    } else {
        addModel(info);
        Body->m_pHandL = info;
        Body->pLeftData = data;
    }
}

// HUNK wears a mask: no face expressions.
void cPlHunk::setFace(int no)
{
}

// Head swap by number (only 0 does anything): replaces the mask model with the archive's 0xB.
void cPlHunk::setHead(int no)
{
    cModelInfo* info;

    if (no != 0) {
        return;
    }
    if (Body->m_pHead == 0) {
        return;
    }
    deleteModelInfo(Body->m_pHead);
    Body->m_pHead = 0;
    info = ModInfoMgr.create(PL_ARC_PTR(pG->pPlayer, 0xB), PL_ARC_PTR(pG->pPlayer, 7));
    if (info) {
        addModel(info);
    }
}

// Head swap with explicit model / texture data; never effective for HUNK (no Body->pShape).
void cPlHunk::setHead(void* bin, void* tpl)
{
    cModelInfo* info;

    if (Body->m_pFace == 0) {
        return;
    }
    deleteModelInfo(Body->m_pHead);
    Body->m_pHead = 0;
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        addModel(info);
    }
}

// PlInitFunc: placement-constructs HUNK in the player's cEm work.
void Pl06Init(cEm* em)
{
    new (em) cPlHunk();
}

// REL entry: registers the player constructor.
extern "C" void _prolog()
{
    PlInitFunc = Pl06Init;
    OSReport("Pl06 HUNK prolog Ok\n");
}

// REL exit: nothing to free.
extern "C" void _epilog()
{
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
