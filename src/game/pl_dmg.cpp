// game/pl_dmg: the player's damage routines — routine 0 == 1 (Pl_R0_Damage: normal hit, blown
// away, blast stagger, each with the get-up steps) and routine 0 == 2 (Pl_R0_Die). Entered from
// cPlayer::setDamage; r_no_3 carries the hit direction, m_Fwork0 the attacker's yaw (123 = keep),
// and the motions come from the player archive (0x48.. hits, 0x4C death, 0x51 fly, 0x52 stagger).

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "pad.h"
#include "snd.h"
#include "esp.h"
#include "math_sub.h"
#include "pl_sub.h"
#include "est.h"
#include "motion.h"

void damageNormal(cPlayer* pl);
void damageBlow(cPlayer* pl);
void damageBlast(cPlayer* pl);
void Pl_R0_Die(cPlayer* pl);


// Routine 0 == 1 (damage, entered by cPlayer::setDamage): r_no_1 0 normal hit, 1 blown away,
// 2 blast stagger.
void Pl_R0_Damage(cPlayer* pEm)
{
    static void (*funcTbl[])(cPlayer*) = {
        damageNormal,
        damageBlow,
        damageBlast,
    };

    funcTbl[pEm->r_no_1](pEm);
}

// Damage sub-routine 0: r_no_2 0 picks the hit motion by r_no_3 (0 front, 1 back, 2/4 left, 3/5
// right; 6 = life is 0: the collapse) turned toward m_Fwork0 (the attacker's yaw; 123 = keep),
// then 1 plays it — the player may cut it short with a key after m_Work0 frames; life 0 goes to
// routine 2/2 (die, already lying). r_no_2 0xA/0xB: the knocked-down variant, standing up with
// splash effects when in water. Ends with EndPlDamage and routine 0/0.
void damageNormal(cPlayer* pEm)
{
    void* mot = 0;
    void* mot2 = 0;
    f32 ang;
    f32 wh;
    Vec* pos;

    switch (pEm->r_no_2) {
    case 0:
        pEm->beginDamage();
        if ((s16) pG->pl_life <= 0) {
            pEm->r_no_3 = 6;
        }
        switch (pEm->r_no_3) {
        default:
            pEm->r_no_3 = 0;
        case 0:
            mot = PL_ARC_PTR(pG->pPlayer, 0x48);
            pEm->m_Work0 = 0x1E;
            break;
        case 1:
            mot = PL_ARC_PTR(pG->pPlayer, 0x49);
            pEm->m_Work0 = 0x23;
            break;
        case 2:
        case 4:
            mot = PL_ARC_PTR(pG->pPlayer, 0x4A);
            mot2 = PL_ARC_PTR(pG->pPlayer, 0x64);
            pEm->m_Work0 = 0x23;
            break;
        case 3:
        case 5:
            mot = PL_ARC_PTR(pG->pPlayer, 0x4B);
            mot2 = PL_ARC_PTR(pG->pPlayer, 0x65);
            pEm->m_Work0 = 0x23;
            break;
        case 6:
            mot = PL_ARC_PTR(pG->pPlayer, 0x4C);
            mot2 = PL_ARC_PTR(pG->pPlayer, 0x4D);
            pEm->m_Work0 = 0x3E7;
            break;
        }
        MotionSetCore(pEm, &pEm->Motion, mot, mot2, 5, 1, 0);
        if (pEm->m_Fwork0 != 123.0f) {
            ang = Muku2(pEm->ang.y, pEm->m_Fwork0, PI);
            pEm->ang.y += ang;
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
            pEm->getPartsPtr(0)->ang.y -= ang;
            pEm->m_Work1 = 1;
        } else {
            pEm->m_Work1 = 0;
        }
        if (pEm->r_no_3 != 6) {
            PlSetDamageSe(0);
        }
        pEm->setFace(1);
        pEm->r_no_2 = 1;
        pEm->dmg.m_Timer |= 0x80;
    case 1:
        if (pEm->Motion.Seq_frame > 19.7f && pEm->Motion.Seq_frame < 20.3f) {
            pEm->setFace(0);
        }
        if (pEm->m_Work0 != 0) {
            pEm->m_Work0--;
        }
        if (pEm->motionMove() != 0 || (pEm->m_Work0 == 0 && (Key.on & 0x10F))) {
            if (pEm->r_no_3 == 6) {
                pEm->r_no_0 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_1 = 2;
                pEm->r_no_3 = 0;
            } else {
                pEm->dmg.m_Flag = 0;
                pEm->dmg.m_Timer = 5;
                EndPlDamage();
                pEm->setRno(0, 0, 0, 0);
            }
        }
        if (pEm->m_Work1 != 0) {
            cParts* p = pEm->getPartsPtr(0);
            p->ang.y += Muku2(pEm->getPartsPtr(0)->ang.y, 0.0f, PI / 10.0f);
        }
        break;
    case 0xA:
        MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x4F), (void*) (pG->pPlayer->ofs[0x50] + (u32) pG->pPlayer), 3, 5, 0);
        EstSet(pEm, -1, 0, 0, EFF_PL00, ChkWaterEffectEnable(&pEm->pos) ? 0x10 : 0xF, 0, ESP_CORE_KIND_NONE, pEm, 0);
        pEm->r_no_2 = 0xB;
    case 0xB:
        pos = &pEm->pos;
        if (pEm->Motion.Seq_frame > 34.7f && pEm->Motion.Seq_frame < 35.3f) {
            SndCall(5, 3, pos, 0, 0, 0);
        }
        if (pEm->Motion.Seq_frame > 52.7f && pEm->Motion.Seq_frame < 53.3f) {
            SndCall(5, 2, pos, 0, 0, 0);
        }
        if (pEm->Motion.Seq_frame > 59.7f && pEm->Motion.Seq_frame < 60.3f) {
            SndCall(1, 0x29, &pEm->getPartsPtr(2)->world, 0, 0, 0);
        }
        if (GetWaterHeight(pos, &wh) && wh > pEm->pos.y) {
            if (MotionCheckCrossFrame(&pEm->Motion, 48.0f) || MotionCheckCrossFrame(&pEm->Motion, 54.0f) ||
                MotionCheckCrossFrame(&pEm->Motion, 65.0f)) {
                EstSet(pEm, -1, 0, 0, EFF_ROOM, 0x23, 0, ESP_CORE_KIND_NONE, pEm, 0);
            }
        }
        if (GetWaterHeight(pos, &wh) && pEm->pList->world.y < wh) {
            if (MotionCheckCrossFrame(&pEm->Motion, 18.0f)) {
                EstSet(pEm, -1, 0, 0, EFF_ROOM, 0x24, 0, ESP_CORE_KIND_NONE, pEm, 0);
            }
        }
        if (pEm->motionMove()) {
            pEm->dmg.m_Flag = 0;
            pEm->dmg.m_Timer = 5;
            EndPlDamage();
            pEm->setRno(0, 0, 0, 0);
        }
        break;
    default:
        pLog->err(0, 0, "invalid r_no_2 %d", pEm->r_no_2);
        break;
    }
}

// Damage sub-routine 1 (blown off the feet, e.g. by a blast or a big enemy): the fly motion (0x51,
// or 0x4E + 0x66 when dead), landing splash / dust, then the get-up (r_no_2 0xA/0xB) or routine
// 2/2 when dead.
void damageBlow(cPlayer* pEm)
{
    void* mot;
    void* mot2;
    f32 ang;
    f32 wh;
    Vec* pos;
    int splash;
    u32 dead;
    u32 n;

    switch (pEm->r_no_2) {
    case 0:
        pEm->beginDamage();
        dead = 1;
        if ((s16) pG->pl_life > 0) {
            dead = 0;
        }
        pEm->m_Work0 = dead;
        if (dead) {
            mot = PL_ARC_PTR(pG->pPlayer, 0x4E);
            mot2 = PL_ARC_PTR(pG->pPlayer, 0x66);
        } else {
            mot = PL_ARC_PTR(pG->pPlayer, 0x51);
            mot2 = PL_ARC_PTR(pG->pPlayer, 0x67);
        }
        MotionSetCore(pEm, &pEm->Motion, mot, mot2, 5, 1, 0);
        n = pEm->m_Work0;
        if (n) {
            EstSet(pEm, -1, 0, 0, EFF_PL00, ChkWaterEffectEnable(&pEm->pos) ? 0xE : 0xD, 0, ESP_CORE_KIND_NONE, pEm, 0);
        } else {
            EstSet(pEm, -1, 0, 0, EFF_PL00, ChkWaterEffectEnable(&pEm->pos) ? 6 : 5, 0, ESP_CORE_KIND_NONE, pEm, (void*) n);
        }
        if (pEm->m_Fwork0 != 123.0f) {
            ang = Muku2(pEm->ang.y, pEm->m_Fwork0, PI);
            pEm->ang.y += ang;
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
            pEm->getPartsPtr(0)->ang.y -= ang;
            pEm->m_Work1 = 1;
        } else {
            pEm->m_Work1 = 0;
        }
        if (pEm->m_Work0 == 0) {
            PlSetDamageSe(0);
        } else {
            SndCall(1, 0x4A, &pEm->getPartsPtr(2)->world, 0, 0, 0);
        }
        pEm->setFace(1);
        pEm->m_Work2 = 0;
        pEm->r_no_2 = 1;
        pEm->dmg.m_Timer |= 0x80;
    case 1:
        if (MotionCheckCrossFrame(&pEm->Motion, 20.0f)) {
            pEm->setFace(0);
        }
        if (pEm->m_Work0 == 0 && pEm->Motion.Seq_frame > 9.7f && pEm->Motion.Seq_frame < 10.3f) {
            SndCall(5, 5, &pEm->pos, 0, 0, 0);
        }
        if (pEm->Motion.Seq_frame >= 5.0f) {
            splash = pEm->m_Work2;
            if (splash == 0 && GetWaterHeight(&pEm->pList->world, &wh) && pEm->pList->world.y < wh + 400.0f) {
                pEm->m_Work2 = 1;
                EstSet(pEm, -1, 0, 0, EFF_ROOM, 0x24, 0, ESP_CORE_KIND_NONE, pEm, (void*) splash);
            }
        }
        if (pEm->motionMove()) {
            if (pEm->m_Work0 != 0) {
                pEm->r_no_0 = 2;
                pEm->r_no_1 = 2;
                pEm->r_no_2 = 0;
                pEm->r_no_3 = 0;
            } else {
                pEm->r_no_2 = 0xA;
            }
        }
        break;
    case 0xA:
        MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x4F), (void*) (pG->pPlayer->ofs[0x50] + (u32) pG->pPlayer), 3, 5, 0);
        EstSet(pEm, -1, 0, 0, EFF_PL00, ChkWaterEffectEnable(&pEm->pos) ? 0x10 : 0xF, 0, ESP_CORE_KIND_NONE, pEm, 0);
        pEm->r_no_2 = 0xB;
    case 0xB:
        pos = &pEm->pos;
        if (pEm->Motion.Seq_frame > 34.7f && pEm->Motion.Seq_frame < 35.3f) {
            SndCall(5, 3, pos, 0, 0, 0);
        }
        if (pEm->Motion.Seq_frame > 52.7f && pEm->Motion.Seq_frame < 53.3f) {
            SndCall(5, 2, pos, 0, 0, 0);
        }
        if (pEm->Motion.Seq_frame > 59.7f && pEm->Motion.Seq_frame < 60.3f) {
            SndCall(1, 0x29, &pEm->getPartsPtr(2)->world, 0, 0, 0);
        }
        if (GetWaterHeight(pos, &wh) && wh > pEm->pos.y) {
            if (MotionCheckCrossFrame(&pEm->Motion, 48.0f) || MotionCheckCrossFrame(&pEm->Motion, 54.0f) ||
                MotionCheckCrossFrame(&pEm->Motion, 65.0f)) {
                EstSet(pEm, -1, 0, 0, EFF_ROOM, 0x23, 0, ESP_CORE_KIND_NONE, pEm, 0);
            }
        }
        if (GetWaterHeight(pos, &wh) && pEm->pList->world.y < wh) {
            if (MotionCheckCrossFrame(&pEm->Motion, 18.0f)) {
                EstSet(pEm, -1, 0, 0, EFF_ROOM, 0x24, 0, ESP_CORE_KIND_NONE, pEm, 0);
            }
        }
        if (pEm->motionMove()) {
            pEm->dmg.m_Flag = 0;
            pEm->dmg.m_Timer = 5;
            EndPlDamage();
            pEm->setRno(0, 0, 0, 0);
        }
        break;
    }
}

// Damage sub-routine 2 (blast stagger, motion 0x52): turns toward m_Fwork0, pained face for 20
// frames, then back to routine 0/0.
void damageBlast(cPlayer* pEm)
{
    f32 ang;
    int no = pEm->r_no_2;

    switch (no) {
    case 0:
        pEm->beginDamage();
        MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x52), 0, 5, 1, 0);
        if (pEm->m_Fwork0 != 123.0f) {
            ang = Muku2(pEm->ang.y, pEm->m_Fwork0, PI);
            pEm->ang.y += ang;
            pEm->ang.y = LIMIT_ANGLE(pEm->ang.y);
            pEm->getPartsPtr(0)->ang.y -= ang;
            pEm->m_Work1 = 1;
        } else {
            pEm->m_Work1 = no;
        }
        pEm->setFace(1);
        pEm->r_no_2 = 1;
        pEm->dmg.m_Timer |= 0x80;
    case 1:
        if (pEm->Motion.Seq_frame > 19.7f && pEm->Motion.Seq_frame < 20.3f) {
            pEm->setFace(0);
        }
        if (pEm->motionMove()) {
            pEm->dmg.m_Flag = 0;
            pEm->dmg.m_Timer = 5;
            EndPlDamage();
            pEm->setRno(0, 0, 0, 0);
        }
        break;
    }
}

// Routine 0 == 2 (death): r_no_1 0 starts the death motion (0x4C/0x4D) with the blood effect,
// scream (when the character has hair data — Leon), rumble at frames 40 / 70; 1 plays it, 2 holds
// the last frame while the game-over sequence runs.
void Pl_R0_Die(cPlayer* pEm)
{
    int no = pEm->r_no_1;

    switch (no) {
    case 0:
        pEm->beginDamage();
        MotionSetCore(pEm, &pEm->Motion, PL_ARC_PTR(pG->pPlayer, 0x4C), (void*) (pG->pPlayer->ofs[0x4D] + (u32) pG->pPlayer), 5, 1, 0);
        EstSet(pEm, -1, 0, 0, EFF_PL00, ChkWaterEffectEnable(&pEm->pos) ? 4 : 3, 0, ESP_CORE_KIND_NONE, pEm, (void*) no);
        pEm->dmg.m_Timer |= 0x80;
        if (pEm->Body->pHair) {
            SndCall(1, 0xD, &pEm->getPartsPtr(4)->world, 0, 0, 0);
            pEm->setFace(1);
        }
        pEm->atari.m_parts_no = 4;
        pEm->r_no_1 = 1;
        pEm->m_Work0 = no;
    case 1:
        if (pEm->Motion.Seq_frame > 39.7f && pEm->Motion.Seq_frame < 40.3f) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 3, 1);
        }
        if (pEm->Motion.Seq_frame > 69.7f && pEm->Motion.Seq_frame < 70.3f) {
            VibSetData((VibDataTbl*) (pG->pCore->ofs_1C + (u32) pG->pCore), 3, 1);
        }
        if (pEm->motionMove()) {
            pEm->r_no_1 = 2;
        }
        break;
    case 2:
        pEm->motionMove();
        break;
    }
}
