// game/pl_event: the player's event routine (routine 0 == 5, Pl_R0_Event): the scenario / event
// system owns the player — Normal plays a set motion (m_Flag 0x100 returns control when it ends),
// ToWalk turns and walks to m_VecWork0, Smooth changes motion with a footwork. Entered / left through
// cPlayer::beginAction / endEvent0 (pl_class).

#include "player.h"
#include "atari.h"
#include "math_sub.h"

// Routine 0 == 5 (event control): the scenario / event drives the player; r_no_1 picks Normal
// (play a motion), ToWalk (walk to m_VecWork0), Smooth (motion change with footwork).
void Pl_R0_Event(cPlayer* pl)
{
    static void (*funcTbl[])(cPlayer*) = {
        pl_R1_Event_Normal,
        pl_R1_Event_ToWalk,
        pl_R1_Event_Smooth,
    };

    funcTbl[pl->r_no_1](pl);
}

// Event sub-routine 0: plays the set motion; when it ends and m_Flag 0x100 (return when done) is
// set, back to routine 0/0 (normal control).
void pl_R1_Event_Normal(cPlayer* pl)
{
    if (pl->r_no_2 == 0) {
        pl->r_no_2 = 1;
    }
    if (pl->motionMove()) {
        if (pl->m_Flag & 0x100) {
            pl->m_Flag &= ~0x100;
            pl->r_no_0 = 0;
            pl->r_no_1 = 0;
            pl->r_no_2 = 0;
            pl->r_no_3 = 0;
        }
    }
}

// Event sub-routine 1: turns toward m_VecWork0 (turn motion when more than 60 degrees off, m_Fwork0
// = turn speed per frame), walks (m_MotTbl[2]) until within 100 units, then stands (m_MotTbl[0])
// and sets m_Work0 = 1 for the event script to see.
void pl_R1_Event_ToWalk(cPlayer* pl)
{
    f32 ang;

    switch (pl->r_no_2) {
    case 0:
        ang = Muku(&pl->pos, &pl->m_VecWork0, pl->ang.y, PI * 2.0f);
        if (fabsf(ang) > PI / 3.0f) {
            pl->motionSet(pl->m_MotTbl[2], 5, 0, 4, 0);
            pl->r_no_2 = 1;
            break;
        }
        goto set_walk;
    case 1:
        ang = Muku(&pl->pos, &pl->m_VecWork0, pl->ang.y, pl->m_Fwork0);
        pl->ang.y += ang;
        if (fabsf(ang) < pl->m_Fwork0 * 0.5f) {
        set_walk:
            pl->motionSet(pl->m_MotTbl[2], 5, 0, 5, 0);
            pl->r_no_2 = 2;
        }
        break;
    case 2:
        ang = Muku(&pl->pos, &pl->m_VecWork0, pl->ang.y, pl->m_Fwork0);
        pl->ang.y += ang;
        if (GetDistance(&pl->pos, &pl->m_VecWork0) < 10000.0f) {
            pl->motionSet(pl->m_MotTbl[0], 5, 0, 1, 0);
            pl->m_Work0 = 1;
            pl->r_no_2 = 3;
        }
        break;
    }
    pl->motionMove();
}

// Event sub-routine 2: lets the current motion end, then re-plants the feet (setFootwork) and
// keeps playing.
void pl_R1_Event_Smooth(cPlayer* pl)
{
    switch (pl->r_no_2) {
    case 0:
        if (pl->motionMove()) {
            pl->r_no_2 = 2;
        }
        break;
    case 2:
        pl->setFootwork();
        pl->motionMove();
        pl->r_no_2 = 3;
    case 3:
        pl->motionMove();
        break;
    }
}
