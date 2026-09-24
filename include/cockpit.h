#ifndef COCKPIT_H
#define COCKPIT_H

#include "types.h"

// HUD (game/cockpit.cpp), instance `Cckpt` (0xCC bytes): the life meter, the bullet counter, the
// count-down timer and the action button prompt. Id table types: 0x21 life meter, 0x20 action
// button, 0x23 count-down, 0x2F message window, 0x30 HUD frame, 0x32 bullet icon.
class LifeMeter {
public:
    u32 flags;         // 0x00
    u8 pad_4[0x18];    // 0x04
    f32 m_life;          // 0x1C  smoothed player life
    s8 m_life_level;          // 0x20  lifeLevel(20, pl_life_max, 1200)
    u8 pad_21[3];
    f32 m_color0[4];       // 0x24  smoothed player meter colours (unit 0x12 col0 / col1)
    f32 m_color1[4];       // 0x34
    f32 m_life_sub;       // 0x44  smoothed partner life
    s8 m_life_level_sub;       // 0x48  lifeLevel(5, sub_life_max, 600)
    u8 pad_49[3];
    f32 m_color0_sub[4];    // 0x4C  smoothed partner meter colours (unit 3)
    f32 m_color1_sub[4];    // 0x5C
    u8 m_state_color0[3][4];       // 0x6C  colour templates: units 0x11 / 0x10 / 0x0F col0
    u8 m_state_color1[3][4];       // 0x78  and col1  (sizeof == 0x84)

    void roomInit();
    void move();
    void fix(int flag);
    void disp(int sw);
    void frameOut();
    void frameIn();
};

class BulletInfo {
public:
    u8 pad_0[0x10];
    s32 m_mark_old;        // 0x10  bullet icon currently shown (type 0x32 id), -1 none
    u8 pad_14[0x2C - 0x14];

    void roomInit();
    void move();
};

// CountDown::m_state bits (PS2 TIMER_STATE): checkState `state`.
enum TIMER_STATE {
    TIMER_STA_NULL = 0,
    TIMER_STA_ALIVE = 1,
    TIMER_STA_UP = 2,
    TIMER_STA_DOWN = 4,
    TIMER_STA_PAUSE = 8,
    TIMER_STA_ERASE = 16
};

class CountDown {
public:
    u32 m_state;         // 0x00  TIMER_STATE bits: ALIVE running, PAUSE by the game flags, ERASE hidden
    s8 m_minute;            // 0x04
    s8 m_second;            // 0x05
    s8 m_centisecond;             // 0x06  1/100 s
    u8 pad_7;
    u32 m_frame;         // 0x08  remaining time in frames
    u32 m_warn_frame;     // 0x0C  frame count below which the digits take unit 8's colour
    u32 m_counter;       // 0x10  1/100 digit jitter phase (0..5)
    u32 savedFlags;    // 0x14  saveDisp / loadDisp

    void roomInit();
    void move();
    void disp(int sw);
    void frameIn();
    void frameOut();
    void initTime(int m, int s, int c);
    void initTimeFrame(u32 frame);
    void warnTime(int m, int s, int c);
    void getTime(int* m, int* s, int* c);
    u32 getFrame();
    void saveDisp();
    void loadDisp();
    int checkState(u32 state);  // game/mercenaries.cpp: (flags & bit) ? 1 : 0
    void setState(u32 state) { m_state |= state; }
    void unsetState(u32 state) { m_state &= ~state; }
    u32 getState(u32 state) { return m_state & state; }
    int isZero()
    {
        int zero = 0;

        if (checkState(TIMER_STA_ALIVE)) {
            zero = m_frame == 0;
        }
        return zero;
    }
};

class ActionButton {
public:
    u8 m_disp_flag;             // 0x00  button prompt to show (0 none)
    u8 m_disp_flag_old;            // 0x01  prompt shown last frame
    u8 pad_2[2];

    void roomInit();
    void move();
};

class Cockpit {
public:
    LifeMeter m_LifeMeter;         // 0x00
    BulletInfo m_BlltInfo;      // 0x84
private:
    CountDown m_CountDown;    // 0xB0, used through the *CountDownTimer methods
public:
    ActionButton m_ActBttn;    // 0xC8  sizeof == 0xCC

    void gameInit();
    void roomInit();
    void move();
    void msgWindow(int sw);
    void lifeMeterDisp(int sw);
    void lifeMeterFix(int flag) { m_LifeMeter.fix(flag); }
    void startCountDownTimer(int min, int sec, int ces)
    {
        m_CountDown.setState(TIMER_STA_ALIVE);
        m_CountDown.initTime(min, sec, ces);
    }
    void startCountDownTimerFrame(u32 frame)
    {
        m_CountDown.setState(TIMER_STA_ALIVE);
        m_CountDown.initTimeFrame(frame);
    }
    void setWarningTime(int min, int sec, int ces) { m_CountDown.warnTime(min, sec, ces); }
    void getRemainTime(int* min, int* sec, int* ces) { m_CountDown.getTime(min, sec, ces); }
    u32 getRemainFrame() { return m_CountDown.getFrame(); }
    void endCountDownTimer()
    {
        m_CountDown.unsetState(TIMER_STA_ALIVE);
        m_CountDown.frameOut();
    }
    int isZeroCountDownTimer() { return m_CountDown.isZero(); }
    void pauseCountDownTimer() { m_CountDown.setState(TIMER_STA_PAUSE); }
    void playCountDownTimer() { m_CountDown.unsetState(TIMER_STA_PAUSE); }
    void dispCountDownTimer(int sw) { m_CountDown.disp(sw); }
    void saveCountDownTimer() { m_CountDown.saveDisp(); }
    void loadCountDownTimer() { m_CountDown.loadDisp(); }
    void transCountDownTimer(int sw)
    {
        if (sw) {
            m_CountDown.frameIn();
        } else {
            m_CountDown.frameOut();
        }
    }
};

extern Cockpit Cckpt;
extern int g_boss_bar_flag;   // game/cockpit.cpp

// game/cockpit.cpp: bullet icon (type 0x32) id for a weapon number, 0xFF none
u8 dispBulletIconMarkNo(u8 wepNo);

#endif
