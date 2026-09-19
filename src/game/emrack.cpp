// game/emrack.cpp: rack enemy (cEmRack): pushable shelves / crates that fall over when shot,
// shake when kicked and break (etc flag) on heavy damage.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emrack.h"
#include "emhit.h"
#include "etc_model.h"
#include "snd.h"
#include "motion.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
void EtcSetAddAmb(cModel* m, int kind);                                                         // EtcModel.cpp
void EmAtCheck(cEm* em);                                                                     // at_mod.cpp
void Em_R0_Scenario(cEm* em);                                                                // em_sub.cpp
// esp.h declares the effect id as int; this unit passes the u8 `eff` byte straight into r7
// (emRack_R1_Break: the byte load is shared by the compare and the calls), so it carries the
// prototype with a u8 parameter.
void EstSet(int a, int b, Vec* pos, Vec* rot, u8 c, int d, int e, int f, u32 g, void* h);
}

typedef void (*EmRackFunc)(cEmRack*);

static EmRackFunc EmRack_R0_move_tbl[5] = {
    emRack_R0_Init,
    emRack_R0_Move,
    0,
    0,
    (EmRackFunc) Em_R0_Scenario,
};

EmRackFunc EmRack_R1_move_tbl[4] = {
    emRack_R1_Set,
    emRack_R1_Down,
    emRack_R1_Break,
    emRack_R1_Shock,
};

// Creates a rack enemy (id 0x45) from a model / TPL at pos / rot. type 0 shelf, 1 tall shelf
// (2000 high, extra hit boxes, can be pushed over), 2 / 3 / 5 pillars and posts, 4 the large
// wardrobe (breaks with a scenario Rno). 1000 hp; tied to room etc flag `etcNo` (bit0 = already
// down / broken -> starts in Break with Rno3 4, no effects). NULL on failure.
cEmRack* SetRack(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo)
{
    cEmRack* em;
    FREE_EMRACK* w;
    u16* flg;
    int zero;

    em = (cEmRack*) EmMgr.create(0x45);
    if (em == 0) {
        return 0;
    }
    w = EMRACK_WK(em);
    w->Etc_no = etcNo;
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->ang = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetRack() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    EtcSetAddAmb(em, 8);
    w->Eff_id = 0xFF;
    em->type = type;
    switch (em->type) {
    case 0:
    default:
        w->Size_x = 700.0f;
        w->Size_y = 1000.0f;
        w->Size_z = 400.0f;
        break;
    case 1:
        w->Size_x = 700.0f;
        w->Size_y = 2000.0f;
        w->Size_z = 400.0f;
        break;
    case 2:
        w->Size_x = 750.0f;
        w->Size_y = 1500.0f;
        w->Size_z = 750.0f;
        break;
    case 3:
        w->Size_x = 500.0f;
        w->Size_y = 4100.0f;
        w->Size_z = 500.0f;
        break;
    case 5:
        w->Size_x = 600.0f;
        w->Size_y = 2400.0f;
        w->Size_z = 600.0f;
        break;
    case 4:
        w->Size_x = 1650.0f;
        w->Size_y = 2250.0f;
        w->Size_z = 1400.0f;
        break;
    }
    em->hp_max = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->LightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(EM_STATUS_LOCKOFF);
    em->setStatus(EM_STATUS_ASHLEY_NO_HELP);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    em->rackFlags = 0xF;
    {
        cAtariInfo* at = &em->atari;

        at->init(0, 2, 0, 0.0f, w->Size_y * 0.5f, 0.0f, w->Size_x - 100.0f, w->Size_z - 100.0f,
                 w->Size_z - 100.0f, w->Size_y * 0.5f);
        at->setPriority(PRI_LV3);
        at->m_flag &= ~0x100;
    }
    w->pSat = (cSat*) zero;
    w->pEatTop = 0;
    w->pEatCenter = 0;
    w->pEatUnder = 0;
    emRackYarareInit(em);
    w->Rack_hp = 0.0f;
    w->Be_flg = 0;
    if (em->type == 1) {
        w->Rack_hp = 1.0f;
    }
    w->Etc_no = etcNo;
    flg = GetEtcFlgPtr(etcNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 4;
    } else {
        em->r_no_0 = 1;
        em->r_no_1 = 0;
        em->r_no_2 = 0;
        em->r_no_3 = 0;
    }
    emRackSatSet(em);
    return em;
}

// Damage check: a damage volume hit breaks types 0 / 1; a registered weapon hit (not knife /
// grenades, and only when the rack's flag bit31 is clear) either spawns the hit est (types
// without breakage), knocks a shelf plank off (parts scaled to 0), breaks the rack (heavy /
// explosive weapons, or a close shotgun blast: Rno1 2 with the style in Rno3), or for handgun /
// knife hits topples it toward the hit position (setDown).
void emRackDmCk(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);
    YARARE_INFO* part;
    u8 wep;
    int type;
    Vec hit;

    if (em->hp > 0) {
        type = em->type;
        if (type >= 0) {
            if (type <= 1) {
                switch (DmgMgr.hitCheck(&em->pos, &hit)) {
                case 1:
                case 4:
                case 5:
                case 7:
                    em->hp = 0;
                    em->r_no_0 = 1;
                    em->r_no_1 = 2;
                    em->r_no_2 = 0;
                    em->r_no_3 = 0;
                    return;
                }
            }
        }
    }
    if (em->dmg.m_Flag == 0) {
        return;
    }
    em->dmg.m_Flag = 0;
    if (em->flag & 0x80000000) {
        return;
    }
    wep = em->dmg.m_Wep;
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
    if (wep == 0xE) {
        return;
    }
    switch (wep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        em->dmg.m_Timer = 0;
        break;
    }
    if (em->dmg.m_Wep == 0x10) {
        em->dmg.m_Timer = 0x11;
    }
    switch (em->type) {
    case 2:
    case 3:
    case 5:
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(em, w->Eff_id, 1, 0, 0, 0);
        }
        return;
    }
    part = em->dmg.m_pDamageYarare;
    switch (em->dmg.m_Wep) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(em, w->Eff_id, 1, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
        if (part->rad < 36000000.0f) {
            if (w->Rack_hp <= 0.0f) {
                em->r_no_0 = 1;
                em->r_no_1 = 2;
                em->r_no_2 = 0;
                em->r_no_3 = 3;
                return;
            }
            if (part->partsNo != 0) {
                cModel* p;

                if (w->Eff_id != 0xFF) {
                    EstSet((int) em, -1, 0, 0, w->Eff_id, 6, 0, 0, (u32) em, 0);
                }
                SndCall(6, 0x36, &em->pos, 0, 0, em);
                p = em->getPartsPtr(1);
                p->scale.x = 0.0f;
                p->scale.y = 0.0f;
                p->scale.z = 0.0f;
                part->flags &= ~1;
                return;
            }
            w->Rack_hp = 0.0f;
        }
        if (w->Eff_id != 0xFF) {
            EmDmBloodSet2(em, w->Eff_id, 2, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x29:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    default:
        em->r_no_0 = 1;
        em->r_no_1 = 2;
        em->r_no_2 = 0;
        em->r_no_3 = 2;
        break;
    case 0:
    case 0x14:
        em->setDown(&em->dmg.m_PosFrom);
        break;
    }
}

// Per-frame: damage check, the Rno0 routine (0 Init, 1 Move, 4 scenario), the model-vs-player
// atari while intact, and the effect collision quads.
void cEmRack::move()
{
    emRackDmCk(this);
    EmRack_R0_move_tbl[r_no_0](this);
    if (hp > 0) {
        EmAtCheck(this);
        atari.move();
    }
    emRackSatSet(this);
}

// Rno0 == 0: resets to the Set state.
void emRack_R0_Init(cEmRack* em)
{
    em->r_no_0 = 1;
    em->r_no_1 = 0;
    em->r_no_2 = 0;
    em->r_no_3 = 0;
}

// Rno0 == 1: dispatches on Rno1 (0 Set, 1 Down, 2 Break, 3 Shock).
void emRack_R0_Move(cEmRack* em)
{
    EmRack_R1_move_tbl[em->r_no_1](em);
}

// Rno1 == 0: standing rack; when its motion crosses frame 2 (the push motion) spawns the dust est
// (5, or 7 for the tall shelf), then updates the matrices.
void emRack_R1_Set(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);

    if (MotionCheckCrossFrame((MotionWork*) &em->pMotion, 2.0f)) {
        if (w->Eff_id != 0xFF) {
            if (em->type == 1) {
                EstSet((int) em, -1, 0, 0, w->Eff_id, 7, 0, 0, (u32) em, 0);
            } else {
                EstSet((int) em, -1, 0, 0, w->Eff_id, 5, 0, 0, (u32) em, 0);
            }
        }
    }
    em->matUpdate();
}

// Rno1 == 1: topples over: rotates parts 0 about x or z (direction Rno3 0..3) with growing speed
// until 72 degrees, then Rno1 2 Break with style 1.
void emRack_R1_Down(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);
    cModel* p;
    int done;

    switch (em->r_no_2) {
    case 0:
        w->TmpF = 0.0f;
        em->hp = 0;
        em->r_no_2++;
    case 1:
        p = em->getPartsPtr(0);
        done = 0;
        switch (em->r_no_3) {
        case 0:
        default:
            p->ang.x += w->TmpF;
            if (p->ang.x > 1.2566371f) {
                done = 1;
            }
            break;
        case 1:
            p->ang.x -= w->TmpF;
            if (p->ang.x < -1.2566371f) {
                done = 1;
            }
            break;
        case 2:
            p->ang.z += w->TmpF;
            if (p->ang.z > 1.2566371f) {
                done = 1;
            }
            break;
        case 3:
            p->ang.z -= w->TmpF;
            if (p->ang.z < -1.2566371f) {
                done = 1;
            }
            break;
        }
        w->TmpF += 0.01f;
        if (done) {
            em->r_no_0 = 1;
            em->r_no_1 = 2;
            em->r_no_2 = 0;
            em->r_no_3 = 1;
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Rno1 == 2: broken / fallen; on entry hides the model, sets bit0 of the etc flag, spawns the break
// est chosen by Rno3 (0 shot, 1 fell, 2 explosion, 3 shotgun, 4 silent restore, 5 wardrobe
// script) with the crash SE, and disables the collision.
void emRack_R1_Break(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);
    u16* flg;

    if (em->r_no_2 == 0) {
        em->hp = 0;
        em->be_flag &= ~2;
        flg = GetEtcFlgPtr(w->Etc_no, pGS->room_id);
        if (flg) {
            *flg |= 1;
        }
        switch (em->type) {
        default:
            if (w->Eff_id == 0xFF) {
                break;
            }
            switch (em->r_no_3) {
            case 0:
            default:
                EstSet((int) em, -1, 0, 0, w->Eff_id, 3, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 1:
                EstSet((int) em, -1, 0, 0, w->Eff_id, 5, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 2:
                EstSet((int) em, -1, 0, 0, w->Eff_id, 0, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 3:
                EstSet((int) em, -1, 0, 0, w->Eff_id, 4, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 4:
                break;
            }
            break;
        case 4:
            if (w->Eff_id == 0xFF) {
                break;
            }
            switch (em->r_no_3) {
            case 0:
            default:
                EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, 3, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 1:
                EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, 5, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 2:
                EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, 0, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 3:
                EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, 4, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 4:
                break;
            case 5:
                EstSet(0, -1, &em->pos, &em->ang, w->Eff_id, 3, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            }
            break;
        case 2:
        case 3:
        case 5:
            break;
        }
        emRackSatClear(em);
        em->r_no_2++;
    }
}

// Rno1 == 3: kicked: takes 50 hp (never below 1), plays the rattle SE and shakes parts 0 for 7
// frames, then back to Set.
void emRack_R1_Shock(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);
    cModel* p;

    switch (em->r_no_2) {
    case 0:
        w->Timer = 7;
        em->hp -= 50;
        if (em->hp <= 0) {
            em->hp = 1;
        }
        SndCall(6, 0x5F, &em->pos, 0, 0, em);
        em->r_no_2++;
    case 1:
        p = em->getPartsPtr(0);
        if (w->Timer != 0) {
            w->Timer--;
            p->ang.x = 0.0f;
            if (pGS->Frame_cnt & 1) {
                p->ang.x = fRand0_1() * 0.024543693f + 0.024543693f;
            }
        } else {
            p->ang.y = 0.0f;
            em->r_no_0 = 1;
            em->r_no_1 = 0;
            em->r_no_2 = 0;
            em->r_no_3 = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->ang);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Keeps the rack's effect collision (EatMgr quads, attribute 0x400000) in place while intact and
// the player is within 15000 units: one quad of the rack's footprint and, for the tall shelf,
// two more shelves at 1000 / 1500 height. Also enables the atari flag 0x200 (blocks the player).
void emRackSatSet(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);
    Vec v[4];
    f32 hx;
    f32 hz;
    f32 h;

    emRackSatClear(em);
    if (em->hp <= 0) {
        return;
    }
    {
        cAtariInfo* at = &em->atari;

        at->m_flag |= 0x200;
    }
    if (w->pEatUnder != 0 && em->plDist2 > 225000000.0f) {
        return;
    }
    hx = w->Size_x - 100.0f;
    hz = w->Size_z - 100.0f;
    v[0].x = -hx;
    v[0].y = 0.0f;
    v[0].z = -hz;
    v[1].x = hx;
    v[1].y = 0.0f;
    v[1].z = -hz;
    v[2].x = hx;
    v[2].y = 0.0f;
    v[2].z = hz;
    v[3].x = -hx;
    v[3].y = 0.0f;
    v[3].z = hz;
    if (em->type == 1) {
        h = 1000.0f;
    } else {
        h = w->Size_y;
    }
    if (w->pEatUnder == 0) {
        w->pEatUnder = EatMgr.create(&em->pos, &em->ang, v, 0x400000, 0, h);
    } else {
        w->pEatUnder->m_Flag |= 4;
        w->pEatUnder->setCoord(&em->pos, &em->ang);
    }
    if (em->type != 1) {
        return;
    }
    v[0].y = 1000.0f;
    v[1].y = 1000.0f;
    v[2].y = 1000.0f;
    v[3].y = 1000.0f;
    h = 500.0f;
    if (w->pEatCenter == 0) {
        w->pEatCenter = EatMgr.create(&em->pos, &em->ang, v, 0x400000, 0, h);
    } else {
        w->pEatCenter->m_Flag |= 4;
        w->pEatCenter->setCoord(&em->pos, &em->ang);
    }
    v[0].y = 1500.0f;
    v[1].y = 1500.0f;
    v[2].y = 1500.0f;
    v[3].y = 1500.0f;
    h = 500.0f;
    if (w->pEatTop == 0) {
        w->pEatTop = EatMgr.create(&em->pos, &em->ang, v, 0x400000, 0, h);
    } else {
        w->pEatTop->m_Flag |= 4;
        w->pEatTop->setCoord(&em->pos, &em->ang);
    }
}

// Deactivates the rack's collision quads and the atari blocking flag.
void emRackSatClear(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);

    em->atari.clrFlag200();
    if (w->pEatUnder) {
        w->pEatUnder->m_Flag &= ~4;
    }
    if (w->pEatCenter) {
        w->pEatCenter->m_Flag &= ~4;
    }
    if (w->pEatTop) {
        w->pEatTop->m_Flag &= ~4;
    }
}

// Hit boxes by type: the body cube; the tall shelf adds the top board, both sides and an inner
// cube (parts 2); pillars use flag 0x41 boxes.
void emRackYarareInit(cEmRack* em)
{
    FREE_EMRACK* w = EMRACK_WK(em);

    switch (em->type) {
    case 0:
    default:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->Size_x, w->Size_y, w->Size_z, 0, 1);
        break;
    case 1:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->Size_x, w->Size_y, w->Size_z, 0, 1);
        YarareAddCube((cEmHit*) em, &w->YarareTbl[0], 0.0f, 1800.0f, 0.0f, 700.0f, 200.0f, 400.0f, 0, 1);
        YarareAddCube((cEmHit*) em, &w->YarareTbl[1], -600.0f, 0.0f, 0.0f, 100.0f, w->Size_y, 400.0f, 0, 1);
        YarareAddCube((cEmHit*) em, &w->YarareTbl[2], 600.0f, 0.0f, 0.0f, 100.0f, w->Size_y, 400.0f, 0, 1);
        YarareAddCube((cEmHit*) em, &w->YarareTbl[3], 0.0f, 1000.0f, 0.0f, 500.0f, 800.0f, 450.0f, 2, 1);
        break;
    case 2:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->Size_x, w->Size_y, w->Size_z, 0, 0x41);
        break;
    case 3:
    case 5:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->Size_x, w->Size_y, w->Size_z, 0, 0x41);
        break;
    case 4:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->Size_x, w->Size_y, w->Size_z, 0, 0x41);
        break;
    }
}

// Script entry: breaks the rack (wardrobe type 4 with its own style 5).
void cEmRack::setBreak()
{
    if (type == 4) {
        r_no_0 = 1;
        r_no_1 = 2;
        r_no_2 = 0;
        r_no_3 = 5;
    } else {
        r_no_0 = 1;
        r_no_1 = 2;
        r_no_2 = 0;
        r_no_3 = 0;
    }
}

// Topples an intact rack away from `target` (player / hit position): picks the fall axis from the
// angle (front / back / left / right); only the tall shelf actually falls (Rno1 1), others break
// at once.
void cEmRack::setDown(Vec* target)
{
    f32 ang;
    f32 abs;

    if (hp <= 0) {
        return;
    }
    ang = Muku(&pos, target, this->ang.y, 3.1415927f);
    abs = fabsf(ang);
    if (ang < 0.0f) {
        r_no_3 = 3;
    } else {
        r_no_3 = 2;
    }
    if (abs < 0.78539819f) {
        r_no_3 = 1;
    }
    if (abs > 2.3561945f) {
        r_no_3 = 0;
    }
    if (r_no_3 == 0 && type == 1) {
        r_no_0 = 1;
        r_no_1 = 1;
        r_no_2 = 0;
    } else {
        r_no_0 = 1;
        r_no_1 = 2;
        r_no_2 = 0;
        r_no_3 = 0;
    }
}

// Script / kick entry: the shake reaction (Rno1 3).
void cEmRack::setShock()
{
    r_no_0 = 1;
    r_no_1 = 3;
    r_no_2 = 0;
    r_no_3 = 0;
}

// Est id used for the hit / break / dust effects.
void cEmRack::setEff(u8 eff)
{
    FREE_EMRACK* w = EMRACK_WK(this);

    w->Eff_id = eff;
}

// Defines how far the rack may be pushed from its start position in its local north / east /
// south / west directions (negative = 0) and enables the range clamp (rackFlags 0x10).
void cEmRack::setRange(f32 n, f32 e, f32 s, f32 w)
{
    RotMatrix(rackMat, &ang);
    TransMatrix(rackMat, &pos);
    PSMTXInverse(rackMat, rackInvMat);
    if (n > 0.0f) {
        rackRange[0] = n;
    } else {
        rackRange[0] = 0.0f;
    }
    if (e > 0.0f) {
        rackRange[1] = e;
    } else {
        rackRange[1] = 0.0f;
    }
    if (s > 0.0f) {
        rackRange[2] = s;
    } else {
        rackRange[2] = 0.0f;
    }
    if (w > 0.0f) {
        rackRange[3] = w;
    } else {
        rackRange[3] = 0.0f;
    }
    rackFlags |= 0x10;
}

// Player push (pl_push): clamps pos to the push range in direction `dir` (0 south, 1 east, 2
// north, 3 west, in rack space); 1 when the rack was stopped at its limit.
int cEmRack::adjustRange(u8 dir)
{
    Vec v;
    int ret;

    if (!(rackFlags & 0x10)) {
        return 0;
    }
    PSMTXMultVec(rackInvMat, &pos, &v);
    ret = 0;
    switch (dir) {
    case 0:
        if (v.z < -rackRange[2]) {
            v.z = -rackRange[2];
            ret = 1;
        }
        break;
    case 1:
        if (v.x > rackRange[1]) {
            v.x = rackRange[1];
            ret = 1;
        }
        break;
    case 2:
        if (v.z > rackRange[0]) {
            v.z = rackRange[0];
            ret = 1;
        }
        break;
    case 3:
        if (v.x < -rackRange[3]) {
            v.x = -rackRange[3];
            ret = 1;
        }
        break;
    }
    if (ret == 1) {
        PSMTXMultVec(rackMat, &v, &pos);
    }
    return ret;
}
