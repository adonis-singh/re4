// game/emswitch.cpp: lever switch enemy (cEmSwitch): opens / closes barred gates, toggled by the
// action button or by a hit.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "emswitch.h"
#include "emhit.h"
#include "etc_model.h"
#include "act_btn.h"
#include "snd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "at_mod.h"
#include "embarrel.h"
#include "player.h"


typedef void (*EmSwitchFunc)(cEmSwitch*);

static EmSwitchFunc EmSwitch_R1_move_tbl[3] = {
    emSwitch_R1_Set,
    emSwitch_R1_Open,
    emSwitch_R1_Close,
};

// Weapon hit reaction: spawns the spark est (0x62, variant 1 when the shot was close) and, when
// Damage_ck is on, toggles the lever (open <-> close) like the action button would.
static void emSwitchDmCk(cEmSwitch* pEm)
{
    EmSwitchWork* w = EMSWITCH_WK(pEm);
    int near;
    u8 wep;

    if (pEm->dmg.m_Flag == 0) {
        return;
    }
    pEm->dmg.m_Flag = 0;
    near = 0;
    if (pEm->dmg.m_pDamageYarare->len < 36000000.0f) {
        near = 1;
    }
    wep = pEm->dmg.m_Wep;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    pEm->dmg.m_Timer = 1;
    if (wep == 0x10) {
        pEm->dmg.m_Timer = 0x11;
    }
    switch (pEm->dmg.m_Wep) {
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(pEm, 0x62, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(pEm, 0x62, 0, 0, 0, 0);
        }
        break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        EmDmBloodSet2(pEm, 0x62, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xE:
    case 0xF:
    case 0x28:
    case 0x2C:
        EmDmBloodSet2(pEm, 0x62, 0, 0, 0, 0);
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
        return;
    case 0x14:
    case 0x15:
        break;
    }
    if (w->Damage_ck) {
        if (pEm->ckOpen()) {
            pEm->setClose();
        } else {
            pEm->setOpen();
        }
    }
}

// Creates a lever switch enemy (id 0x4B, at the back of the pool) from a model / TPL at pos / rot,
// unless room etc flag `flagNo` bit0 marks it removed. Starts open (state 1) with the action
// button enabled, hit toggling on and a 1500 unit reach. NULL when no work is free.
cEmSwitch* SetEmSwitch(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo)
{
    cEmSwitch* em;
    EmSwitchWork* w;
    u16* flg;

    flg = GetEtcFlgPtr(flagNo, pG->room_id);
    if (flg && (*flg & 1)) {
        return 0;
    }
    em = (cEmSwitch*) EmMgr.createBack(0x4B);
    if (em == 0) {
        return 0;
    }
    w = EMSWITCH_WK(em);
    w->Etc_no = flagNo;
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetSwitch() failed.");
        EmMgr.destroy(em);
        return em;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    f32 zero = 0.0f;
    AtariInit(&em->atari, zero, zero, -700.0f, 350.0f, 700.0f, 700.0f, 2000.0f, 0, 2, 0);
    em->atari.m_flag &= ~0x300;
    em->atari.setPriority(PRI_LV3);
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    YarareInitCube((cEmHit*) em, zero, -300.0f, zero, 250.0f, 600.0f, 200.0f, 0, YAT_FLAG_ON);
    em->hp_max = 1000;
    em->hp = 0;
    if (pos) {
        em->pos = *pos;
    } else {
        em->pos.x = zero;
        em->pos.y = zero;
        em->pos.z = zero;
    }
    em->pos_old = em->pos;
    if (rot) {
        em->ang = *rot;
    }
    w->Status = 1;
    w->Onoff_flag = 1;
    w->pBarred = 0;
    w->pBarred2 = 0;
    w->pSwitch = 0;
    w->Mode = 0;
    w->Damage_ck = 1;
    w->Barrel_ck = 0;
    w->Barrel_wait = 0;
    w->Ck_dis = 1500.0f;
    em->setActButton(1);
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
    return em;
}

// Per-frame: Barrel_wait countdown, hit check, the Rno1 routine (0 Set, 1 Open, 2 Close), the
// model-vs-player atari.
void cEmSwitch::move()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    if (w->Barrel_wait) {
        w->Barrel_wait--;
    }
    emSwitchDmCk(this);
    EmSwitch_R1_move_tbl[r_no_1](this);
    EmAtCheck(this);
}

// Rno1 == 0: idle lever; updates the matrix and offers the action button.
void emSwitch_R1_Set(cEmSwitch* pEm)
{
    pEm->matUpdate();
    emSwitchOperationActEvtCk(pEm);
}

// Rno1 == 1: pulls the lever up (parts 1 x angle -10 degrees / frame to 0) with the lever SE, then
// opens the linked gate(s) and settles in state 1 (Mode 2: immediately closes again).
void emSwitch_R1_Open(cEmSwitch* pEm)
{
    EmSwitchWork* w = EMSWITCH_WK(pEm);
    cParts* p;

    switch (pEm->r_no_2) {
    case 0:
        SndCall(6, 0x23, &pEm->pos, 0, 0, pEm);
        pEm->r_no_2++;
    case 1:
        p = pEm->getPartsPtr(1);
        p->ang.x -= 0.17453292f;
        if (p->ang.x < 0.0f) {
            p->ang.x = 0.0f;
            if (w->pBarred) {
                w->pBarred->setOpen(0);
            }
            if (w->pBarred2) {
                w->pBarred2->setOpen(0);
            }
            w->Status = 1;
            if (w->Mode == 2) {
                w->Status = 0;
                w->Onoff_flag = 0;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            } else {
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
        }
        break;
    }
    pEm->matUpdate();
}

// Rno1 == 2: pushes the lever down (to 78 degrees), then closes the linked gate(s); with setBarrel
// (room 227) also releases a rolling barrel every 150 frames; settles in state 2 (Mode 3
// setAutoOpen: swings back open at once).
void emSwitch_R1_Close(cEmSwitch* pEm)
{
    EmSwitchWork* w = EMSWITCH_WK(pEm);
    cParts* p;

    switch (pEm->r_no_2) {
    case 0:
        SndCall(6, 0x23, &pEm->pos, 0, 0, pEm);
        pEm->r_no_2++;
    case 1:
        p = pEm->getPartsPtr(1);
        p->ang.x += 0.17453292f;
        if (p->ang.x > 1.3613569f) {
            p->ang.x = 1.3613569f;
            if (w->pBarred) {
                w->pBarred->setClose(0);
            }
            if (w->pBarred2) {
                w->pBarred2->setClose(0);
            }
            if (w->Barrel_ck && w->Barrel_wait == 0) {
                Vec pos;
                Vec rot;

                pos.x = -63.0f;
                pos.y = 20000.0f;
                pos.z = -11699.0f;
                rot.x = 0.0f;
                rot.y = 1.3744467f;
                rot.z = 0.0f;
                SetR227Barrel(&pos, &rot);
                w->Barrel_wait = 150;
            }
            w->Status = 2;
            if (w->Mode == 3) {
                w->Status = 0;
                w->Onoff_flag = 1;
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 1;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            } else {
                pEm->r_no_0 = 1;
                pEm->r_no_1 = 0;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            }
        }
        break;
    }
    pEm->matUpdate();
}

// Lever state: 0 moving, 1 open, 2 closed.
int cEmSwitch::ckSwitch()
{
    return EMSWITCH_WK(this)->Status;
}

// 1 when the lever is in (or moving to) the open position.
int cEmSwitch::ckOpen()
{
    if (EMSWITCH_WK(this)->Onoff_flag) {
        return 1;
    }
    return 0;
}

// Starts opening a closed lever (Rno1 1) and forwards to the connected switch.
void cEmSwitch::setOpen()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    if (w->Status == 2) {
        w->Status = 0;
        w->Onoff_flag = 1;
        r_no_0 = 1;
        r_no_1 = 1;
        r_no_2 = 0;
        r_no_3 = 0;
        if (w->pSwitch) {
            w->pSwitch->setOpen();
        }
    }
}

// Starts closing an open lever (Rno1 2) unless Mode 1 (open-only); forwards to the connected
// switch.
void cEmSwitch::setClose()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    if (w->Status == 1 && w->Mode != 1) {
        w->Status = 0;
        w->Onoff_flag = 0;
        r_no_0 = 1;
        r_no_1 = 2;
        r_no_2 = 0;
        r_no_3 = 0;
        if (w->pSwitch) {
            w->pSwitch->setClose();
        }
    }
}

// Snaps the lever to the open position (no gate update).
void cEmSwitch::setOpened()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    getPartsPtr(1)->ang.x = 0.0f;
    w->Status = 1;
    w->Onoff_flag = 1;
}

// Snaps the lever to the closed position.
void cEmSwitch::setClosed()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    getPartsPtr(1)->ang.x = 1.3613569f;
    w->Status = 2;
    w->Onoff_flag = 0;
}

// Links the primary gate the lever drives and matches its current open / closed state.
void cEmSwitch::setBarred(cEmBarred* b)
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    w->pBarred = b;
    if (w->Status == 1) {
        b->setOpened();
    }
    if (w->Status == 2) {
        b->setClosed();
    }
}

// Links a second gate driven together with the first.
void cEmSwitch::setBarred2nd(cEmBarred* pBarred)
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    w->pBarred2 = pBarred;
    if (w->Status == 1) {
        pBarred->setOpened();
    }
    if (w->Status == 2) {
        pBarred->setClosed();
    }
}

// Links another lever that mirrors this one's operation.
void cEmSwitch::setConnectSwitch(cEmSwitch* s)
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    w->pSwitch = s;
    if (w->Status == 1) {
        s->setOpened();
    }
    if (w->Status == 2) {
        s->setClosed();
    }
}

// Enables / disables the action button prompt.
void cEmSwitch::setActButton(int flag)
{
    EMSWITCH_WK(this)->actButton = flag;
}

// Offers action button 0x14 (pull / push lever) when the lever is at rest and the player stands
// within Ck_dis, at lever height, facing it (and, for type != 1, in front of it): opens a closed
// lever, closes an open one unless Mode 1.
void emSwitchOperationActEvtCk(cEmSwitch* pObj)
{
    EmSwitchWork* w = EMSWITCH_WK(pObj);
    f32 dz;
    f32 dx;

    if (w->actButton == 0) {
        return;
    }
    if (w->Status == 0) {
        return;
    }
    dz = pObj->pos.z - pPL->pos.z;
    dx = pObj->pos.x - pPL->pos.x;
    if (dx * dx + dz * dz > w->Ck_dis * w->Ck_dis) {
        return;
    }
    if (fabsf(pObj->pos.y - (pPL->pos.y + 1000.0f)) > 1000.0f) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &pObj->pos, pPL->ang.y, 3.1415927f)) > 0.78539819f) {
        return;
    }
    if (pObj->type != 1) {
        if (fabsf(Muku(&pObj->pos, &pPL->pos, pObj->ang.y, 3.1415927f)) > 1.5707964f) {
            return;
        }
    }
    if (w->Status == 2) {
        ActBtn.set(ACT_OPERATION, 5, (void*) emSwitchActOpen, pObj, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
    }
    if (w->Status == 1) {
        if (w->Mode != 1) {
            ActBtn.set(ACT_OPERATION, 5, (void*) emSwitchActClose, pObj, ACTCTR_NONE, DISP_A_NORMAL, ACT_FUNC_NORMAL, 0);
        }
    }
}

// Action button callback: open.
void emSwitchActOpen(cEmSwitch* ptr)
{
    ptr->setOpen();
}

// Action button callback: close.
void emSwitchActClose(cEmSwitch* ptr)
{
    ptr->setClose();
}

// Mode 1: the lever can only be opened.
void cEmSwitch::setOpenOnly()
{
    EMSWITCH_WK(this)->Mode = 1;
}

// Mode 3: the lever springs back open after closing (one-shot pulls).
void cEmSwitch::setAutoOpen()
{
    EMSWITCH_WK(this)->Mode = 3;
}

// Room 227: closing the lever releases a rolling barrel (SetR227Barrel).
void cEmSwitch::setBarrel()
{
    EMSWITCH_WK(this)->Barrel_ck = 1;
}

// Extends the action button reach to 2000 units.
void cEmSwitch::setLongCk()
{
    EMSWITCH_WK(this)->Ck_dis = 2000.0f;
}
