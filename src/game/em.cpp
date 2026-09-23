// game/em.cpp: the character work base (cEm) and its manager (cEmMgr): construction of the
// player / enemy / object classes by id, the per-frame emMove loop, the damage info (cDmgInfo).

#include "atari.h"
#include "ctrl.h"
#include "em.h"
#include "player.h"
#include "emobj.h"
#include "emdoor.h"
#include "emwep.h"
#include "embox.h"
#include "emwindow.h"
#include "emtorch.h"
#include "embarrel.h"
#include "emtree.h"
#include "emrock.h"
#include "emswitch.h"
#include "emitem.h"
#include "emhit.h"
#include "emBarred.h"
#include "emmine.h"
#include "emshield.h"
#include "emBar.h"
#include "snd.h"
#include "global.h"
#include "db_log.h"
#include "va_ppc.h"
#include "at_mod.h"
#include "route_ck.h"
#include "em_sub.h"
#include "read.h"
#include "shape.h"


// cManager<T>::arrayFree / arrayAlloc: definitions in cManager.h (game.cpp instantiates them too).

const char* cEmMgr::idName[96] = {
    "PLAYER", "", "", "ASHLEY", "LUIS", "", "", "", "", "", "", "", "", "", "JET SKI", "MOTOR BOAT",
    "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO",
    "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO", "GANADO",
    "SPIDER", "DOG", "DOG", "CROW", "SNAKE S", "PARASITE", "COW", "BLACKBASS",
    "CHICKEN", "BAT", "TRAP", "ELGIGANTE", "INSECT BOSS", "INSECT HUMAN", "SPIDER S", "SALAMANDER",
    "SADDLER", "", "U3", "INSECTBOSS EVENT", "MAYOR", "MAYOR AFTER", "REGENERATER", "NO2",
    "NO2 AFTER", "NO3", "NO3 AFTER", "TRUCK", "ARMOR", "HELICOPTER", "", "",
    "OBJ", "DOOR", "WEP", "BOX", "WALL", "RACK", "WINDOW", "TORCH",
    "BARREL", "TREE", "ROCK", "SWITCH", "ITEM", "HIT", "BARRED", "MINE",
    "SHIELD",
};

cPlayer* pPL;
cEm* pSUB;
void (*EmInitFunc)(cEm* em);
void (*PlInitFunc)(cEm* em);

static u32 battleCheckFlag;

// The character manager: a cManager<cEm> pool of 0xDE0 byte works (type 2), one serial (Guid)
// counter for the works it hands out.
cEmMgr::cEmMgr() : cManager<cEm>(EM_WORK_SIZE, 2)
{
    setName("cEmMgr");
    Guid = 0;
}

// cManager log hook: routes the manager's messages to pLog as level-6 warnings.
void cEmMgr::log(const char* pStr, ...)
{
    va_list ap;

    va_start(ap, pStr);
    pLog->vwarn(6, 0, pStr, ap);
}

// Places the class for character `id` into the fresh work: id 0 the player (cPlLeon / cPlAshley
// by pG->pl_type, other player types through PlInitFunc), 1..0xE and every enemy id through the
// loaded enemy module (EmReadSearch + EmInitFunc; 0 when the module is not loaded), 0x40..0x51
// the object classes (door, weapon, box, rack, window, torch, barrel, tree, rock, switch, item,
// hit, barred, mine, shield, bar), 0xFF a bare cEm. Then assigns the serial, be_flag 0x40 |
// 0x02000000, emset_no 0xFF and stores the read table entry.
int cEmMgr::construct(cEm* pEm, u32 id)
{
    switch (id) {
    case 0:
        switch (pG->pl_type) {
        case 0:
            pEm = new (pEm) cPlLeon;
            break;
        case 1:
            pEm = new (pEm) cPlAshley;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            PlInitFunc(pEm);
            break;
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
        pEm->subArc = (PlArc*)EmReadSearch((u8) id, 0, 0);
        if (pEm->subArc == 0) {
            return 0;
        }
        EmInitFunc(pEm);
        break;
    case 0x40:
        pEm = new (pEm) cEmObj;
        break;
    case 0x41:
        pEm = new (pEm) cEmDoor;
        break;
    case 0x42:
        pEm = new (pEm) cEmWep;
        break;
    case 0x43:
        pEm = new (pEm) cEmBox;
        break;
    case 0x45:
        pEm = new (pEm) cEmRack;
        break;
    case 0x46:
        pEm = new (pEm) cEmWindow;
        break;
    case 0x47:
        pEm = new (pEm) cEmTorch;
        break;
    case 0x48:
        pEm = new (pEm) cEmBarrel;
        break;
    case 0x49:
        pEm = new (pEm) cEmTree;
        break;
    case 0x4A:
        pEm = new (pEm) cEmRock;
        break;
    case 0x4B:
        pEm = new (pEm) cEmSwitch;
        break;
    case 0x4C:
        pEm = new (pEm) cEmItem;
        break;
    case 0x4D:
        pEm = new (pEm) cEmHit;
        break;
    case 0x4E:
        pEm = new (pEm) cEmBarred;
        break;
    case 0x4F:
        pEm = new (pEm) cEmMine;
        break;
    case 0x50:
        pEm = new (pEm) cEmShield;
        break;
    case 0x51:
        pEm = new (pEm) cEmBar;
        break;
    case 0xFF:
        pEm = new (pEm) cEm;
        break;
    default:
        pEm->subArc = (PlArc*)EmReadSearch((u8) id, 0, 0);
        if (pEm->subArc == 0) {
            return 0;
        }
        EmInitFunc(pEm);
        break;
    }
    switch (pEm->id) {
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
        pEm->id = 0x10;
        break;
    }
    pEm->guid = Guid;
    Guid++;
    pEm->emset_no = 0xFF;
    pEm->be_flag |= 0x40;
    pEm->id = id;
    pEm->be_flag |= 0x02000000;
    pEm->subArc2 = pEm->subArc;
    return 1;
}

// Allocates the pool of `n` character works and clears the player / partner pointers.
int cEmMgr::arrayAlloc(u32 workNum)
{
    cManager<cEm>::arrayAlloc(workNum);
    pPL = 0;
    pSUB = 0;
    return 1;
}

// Per-frame character update (game loop): dieCheck, the enemy route check, then emMove on every
// live work. Under Stop_flg 0x20000000 (characters frozen) only the partner (pSUB) moves, and
// not when Stop_flg 0x1000 freezes her too.
void cEmMgr::move()
{
    cEm* p;
    void (*func)(cEm*);

    dieCheck();
    RouteCk();
    if (!SpfFlagChk(pG, SPF_EM)) {
        p = pAlive;
        func = emMove;
        while (p) {
            cEm* cur = p;

            p = (cEm*) p->pNext;
            func(cur);
        }
    } else if (pSUB && !SpfFlagChk(pG, SPF_SUBCHAR)) {
        emMove(pSUB);
    }
}

// Releases a character work: validates the pointer and its live flags (be_flag 0x201 == 1),
// runs the work's push() cleanup and returns it to the pool.
void cEmMgr::destroy(cEm* pEm)
{
    if ((u32) pEm < 0x80000000 || (u32) pEm > 0x82FFFFFF || (pEm->be_flag & 0x201) != 1) {
        pLog->err(0, 0, "cEmMgr::destroy() WORK IS ALREADY DEAD. %08X", pEm);
        return;
    }
    pEm->push();
    cManager<cEm>::destroy(pEm);
}

// 1 when any live character has EM_STATUS_ATTACKING set (used for the battle music / save
// prompt rules).
int cEmMgr::isBattle()
{
    cEm* p;
    void (*func)(cEm*);

    // reference store: keeps the pAlive load below it (global.h BitSet)
    (battleCheckFlag = 0);
    func = battleCheck;
    p = pAlive;
    while (p) {
        cEm* cur = p;

        p = (cEm*) p->pNext;
        func(cur);
    }
    return battleCheckFlag;
}

// isBattle helper: raises battleCheckFlag for a character with EM_STATUS_ATTACKING.
void battleCheck(cEm* pEm)
{
    if (pEm->checkStatus(0)) {
        battleCheckFlag = 1;
    }
}

// destroyAll helper: destroys every character except the player (id 0).
void killEm(cEm* pEm)
{
    if (pEm->id != 0) {
        EmMgr.destroy(pEm);
    }
}

// Destroys every live character except the player (room change).
void cEmMgr::destroyAll()
{
    cEm* p;
    void (*func)(cEm*);

    p = pAlive;
    func = killEm;
    while (p) {
        cEm* cur = p;

        p = (cEm*) p->pNext;
        func(cur);
    }
}

// Next live character with `id` after `start` (from the head when start is NULL); NULL when none.
cEm* cEmMgr::getEmPtr(int id, cEm* pEm)
{
    cEm* p;

    p = pEm;
    if (p) {
        p = (cEm*) p->pNext;
    } else {
        p = pAlive;
    }
    while (p) {
        if (p->id == id) {
            return p;
        }
        p = (cEm*) p->pNext;
    }
    return 0;
}

// Base character constructor: builds the damage info and the default work (initWork).
cEm::cEm()
{
    initWork();
}

// Sets EM_STATUS bit `bit` in `status`.
void cEm::setStatus(int id)
{
    status |= 1 << id;
}

// Clears EM_STATUS bit `bit`.
void cEm::clearStatus(int id)
{
    status &= ~(1 << id);
}

// 1 when EM_STATUS bit `bit` is set.
int cEm::checkStatus(int id)
{
    if (status & (1 << id)) {
        return 1;
    }
    return 0;
}

// Virtual: can this character be thrown / knocked by the player? Base: never (0).
int cEm::checkThrow()
{
    return 0;
}

// Assigns the item the character drops when destroyed / killed (id, count, item flags, auto
// pickup flags, item effect type); EmSetDropItem spawns it.
void cEm::setItem(u16 item_id, u16 num, u16 item_flg, u16 auto_item_flg, u8 item_eff)
{
    Item_id = item_id;
    Item_num = num;
    Item_flg = item_flg;
    Auto_item_flg = auto_item_flg;
    itemFlag = item_eff;
}

// Clears the drop item (Item_id 0xFFFF).
void cEm::setNoItem()
{
    Item_id = 0xFFFF;
    Item_num = 0;
    Item_flg = 0;
    Auto_item_flg = 0;
    itemFlag = 0;
}

// Per-character frame step for every work but the player: skips hidden works during an event
// pause (Status_flg[1] 0x10000000 unless be_flag 0x800) and the frozen partner; caches the
// squared distance to the player (l_pl), ticks the damage info, runs the virtual move(), then
// the shape (skeleton) update, the queued SE (seNo), old position update, hit box debug display
// and bounding boxes, and resets invisible_factor2.
void emMove(cEm* pEm)
{
    f32 dx;
    f32 dz;

    if ((pEm->be_flag & 0x201) != 1) {
        pLog->err(2, 0, "emMove() DEAD WORK CALLED %08X(ID:%02X)", pEm, pEm->id);
        EmMgr.destroy(pEm);
        return;
    }
    if (StaFlagChk(pG, STA_SUSPEND) && !(pEm->be_flag & 0x800)) {
        return;
    }
    if (pEm == pPL) {
        return;
    }
    if (pEm == pSUB && (SpfFlagChk(pG, SPF_SUBCHAR))) {
        return;
    }
    dz = pPL->pos.z - pEm->pos.z;
    dx = pPL->pos.x - pEm->pos.x;
    pEm->l_pl = dx * dx + dz * dz;
    pEm->l_sub = 1e16f;
    pEm->dmg.move();
    pEm->move();
    if ((pEm->be_flag & 0x201) != 1) {
        return;
    }
    pEm->be_flag &= ~0x20000000;
    ShapeMove(pEm->pModelInfo);
    if (pEm->Motion.Seq_old.Se) {
        int no = pEm->Motion.Seq_old.Se - 1;
        cParts* parts = pEm->getPartsPtr(0);

        SndCall(8, no, &parts->world, pEm->id, 0, pEm);
        pEm->Motion.Seq_old.Se = 0;
    }
    pEm->updateOldPos();
    EmYarareDisp(pEm);
    if (DbgFlagChk(pG, DBG_OBA_VIEW)) {
        DrawOba(pEm);
    }
    if (pEm->be_flag & 0x80000000) {
        pEm->drawAllBoundingBox(pEm->pModelInfo);
    }
    pEm->invisible_factor2 = 1.0f;
}

// Virtual per-frame behaviour; the base character does nothing.
void cEm::move()
{
}

// Default work state: be_flag 0x21 (alive, ...), kindid 0.
int cEm::initWork()
{
    be_flag = 0x21;
    kindid = 0;
    return 1;
}

// Damage info starts cleared.
cDmgInfo::cDmgInfo()
{
    clear();
}

// Registers a hit on the character: stat = flag | 1 (a hit is pending), lifetime `timer` frames
// (bit7 = hold until cleared), damage kind, hit position / radius and the hit box that was hit.
// The character's own move reads and clears it.
void cDmgInfo::set(int flag, int timer, u8 wep, Vec* pos, f32 dist, YARARE_INFO* pYarare)
{
    m_Flag = flag | 1;
    m_Timer = timer;
    m_Wep = wep;
    m_PosFrom = *pos;
    m_Dist = dist;
    m_pDamageYarare = pYarare;
}

// Sets only the state byte and the timer (player damage motions).
void cDmgInfo::set(int flag, int timer)
{
    m_Flag = flag;
    m_Timer = timer;
}

// Forgets the registered hit.
void cDmgInfo::clear()
{
    m_Flag = 0;
    m_Timer = 0;
}

// Per-frame: counts the timer down (unless bit7 holds it) and clears the hit state when it
// reaches 0, so an unhandled hit expires.
void cDmgInfo::move()
{
    if (m_Timer & 0x80) {
        return;
    }
    if ((m_Timer & 0x7F) == 0) {
        return;
    }
    m_Timer--;
    if (m_Timer == 0) {
        m_Flag = 0;
    }
}

cEmMgr EmMgr;
