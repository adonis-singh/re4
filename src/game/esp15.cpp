// game/esp15.cpp: effect id 0x15, a camera-relative weather particle (rain, snow, dust). The
// sprite lives in a box of size Range (R_pos.z) around the camera: whenever it drifts out on the
// camera's side / up axes it is wrapped back by 1.2 x Range, along the view axis by Range. The
// alpha fades over the far Del_ratio (Work8[2] %) part of the box, and fades out over
// Room_del_frame frames while the player is in a weather-off area (Status_flg[1] 0x02000000).
// Vec0.x is a floor height the particle may not fall below.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp15Work {
    f32 Range;     // 0x00 half size of the box around the camera
    f32 Del_ratio;  // 0x04 1 - (distance ratio where the alpha starts fading)
    f32 Base_alpha;     // 0x08 base alpha
    f32 Min_y;    // 0x0C the sprite may not fall below this height (0 = none)
    u8 Room_del_frame;     // 0x10 fade in frames
    u8 Room_del_cnt;        // 0x11
};

// Camera-relative particle (rain / snow / dust): the position is wrapped so that it always
// stays inside a box around the camera.
class cEsp15 : public cEsp {
public:
    Esp15Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

// EspCreateTbl[0x15] factory.
cEsp* Esp15_Create()
{
    return new cEsp15;
}

// Restores the base alpha, runs the base update/animation, applies the indoor fade counter, wraps
// the position into the camera box on the three camera axes, fades the alpha with view distance
// and keeps the particle above Min_y.
void cEsp15::move()
{
    Esp15Work* w = &m_Free;
    Vec tmp;
    Vec dir;
    Vec sc;
    Vec sc2;
    f32 range;
    f32 half;
    f32 d;
    f32 oldY;
    int n;
    int flag1;
    int flag2;

    m_Col_a = w->Base_alpha;
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            flag1 = 1;
            flag2 = 1;
            oldY = m_Pos.y;
            if (w->Min_y != 0.0f && m_Pos.y < w->Min_y) {
                flag1 = 0;
            }
            w->Base_alpha = m_Col_a;
            if (w->Room_del_frame != 0) {
                if (StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
                    w->Room_del_cnt++;
                } else if (w->Room_del_cnt != 0) {
                    w->Room_del_cnt--;
                }
                if (w->Room_del_cnt != 0) {
                    if (w->Room_del_cnt >= w->Room_del_frame) {
                        w->Room_del_cnt = w->Room_del_frame;
                    }
                    m_Col_a = m_Col_a * (1.0f - (f32)w->Room_del_cnt / w->Room_del_frame);
                }
            }
            range = w->Range;
            half = range * 0.6f;

            PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &dir);
            PSVECCrossProduct(&dir, &pG->Cam.up, &dir);
#line 111 "D:/Bio4/Prog/esp15.cpp"
            VECNormalize(&dir, &dir);
            PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &tmp);
            d = PSVECDotProduct(&tmp, &dir);
            if (d >= 0.0f) {
                n = (int)((d + half) / (half * 2.0f));
            } else {
                n = (int)((d - half) / (half * 2.0f));
            }
            if (n != 0) {
                PSVECScale(&dir, &sc, -(half * 2.0f * (f32)n));
                PSVECAdd(&m_Pos, &sc, &m_Pos);
            }

#line 130 "D:/Bio4/Prog/esp15.cpp"
            VECNormalize(&pG->Cam.up, &dir);
            PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &tmp);
            d = PSVECDotProduct(&tmp, &dir);
            if (d >= 0.0f) {
                n = (int)((d + half) / (half * 2.0f));
            } else {
                n = (int)((d - half) / (half * 2.0f));
            }
            if (n != 0) {
                PSVECScale(&dir, &sc2, -(half * 2.0f * (f32)n));
                PSVECAdd(&m_Pos, &sc2, &m_Pos);
            }

            PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &dir);
#line 152 "D:/Bio4/Prog/esp15.cpp"
            VECNormalize(&dir, &dir);
            PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &tmp);
            d = PSVECDotProduct(&tmp, &dir);
            if (d >= 0.0f) {
                n = (int)(d / range);
            } else {
                n = -(int)(-d / range) - 1;
            }
            if (n != 0) {
                PSVECScale(&dir, &sc, -(range * (f32)n));
                PSVECAdd(&m_Pos, &sc, &m_Pos);
                PSVECSubtract(&m_Pos, &pG->Cam.param.pos, &tmp);
                d = PSVECDotProduct(&tmp, &dir);
            }
            if (d > range * w->Del_ratio) {
                m_Col_a = m_Col_a * (1.0f - (d - range * w->Del_ratio) / (range * (1.0f - w->Del_ratio)));
            }
            if (w->Min_y != 0.0f) {
                if (m_Pos.y < w->Min_y) {
                    flag2 = 0;
                }
                if (flag1 == 1 && flag2 == 0) {
                    m_Pos.y = oldY;
                }
            }
        }
    }
}

// Delete distances from Work8[0..1] (x 10), fade ratio Work8[2] (%), indoor fade frames Work8[3],
// box size R_pos.z (position randomised inside it), floor Vec0.x. Starts fully faded when the
// player is already indoors.
int cEsp15::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp15Work* w = &m_Free;

    m_Del_far = (s8)gen->Work8[0] * 10;
    m_Del_near = (s8)gen->Work8[1] * 10;
    w->Del_ratio = (f32)(s8)gen->Work8[2] / 100.0f;
    if (w->Del_ratio > 1.0f) {
        w->Del_ratio = 1.0f;
    }
    w->Del_ratio = 1.0f - w->Del_ratio;
    w->Room_del_frame = gen->Work8[3];
    w->Range = gen->R_pos.z;
    m_Pos.x += w->Range * fRandSeed1_1(seed);
    m_Pos.y += w->Range * fRandSeed1_1(seed);
    m_Pos.z += w->Range * fRandSeed1_1(seed);
    w->Base_alpha = m_Col_a;
    w->Min_y = gen->Vec0.x;
    if (StaFlagChk(pGS, STA_CAMERA_IN_ROOM)) {
        m_Col_a = 0.0f;
        w->Room_del_cnt = w->Room_del_frame;
    }
    return 1;
}
